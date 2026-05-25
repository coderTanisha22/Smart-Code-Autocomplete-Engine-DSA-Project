#include "../include/freq_store.h"
#include <fstream>
#include <sstream>
#include <filesystem>

FreqStore::FreqStore(const std::string& path) : filePath(path) {
    load();
    // Started after load() so the writer never sees a half-built map.
    writer = std::thread(&FreqStore::writerLoop, this);
}

FreqStore::~FreqStore() {
    {
        std::lock_guard<std::mutex> lk(writerMtx);
        stopping = true;
    }
    writerCv.notify_all();
    if (writer.joinable()) {
        writer.join();
    }
    //Final flush after the thread is gone, so nothing races on the file.
    writeSnapshotToDisk();
}

void FreqStore::load() {
    std::ifstream file(filePath);

    if (!file.is_open()) {
        return;
    }

    std::unordered_map<std::string, int> loaded;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // A token may be an accepted phrase, and phrases contain spaces
        // ("while const"). Split on the LAST space so the token keeps its own.
        size_t sep = line.find_last_of(' ');
        if (sep == std::string::npos || sep == 0) continue;

        std::string token = line.substr(0, sep);
        int freq = 0;
        try {
            freq = std::stoi(line.substr(sep + 1));
        } catch (const std::exception&) {
            continue;                       // corrupt count - skip, never throw
        }

        loaded[token] = freq;
    }
    file.close();

    // Parse outside the lock, publish inside it.
    std::unique_lock lock(mtx);
    frequencies = std::move(loaded);
}

std::unordered_map<std::string, int> FreqStore::snapshot() const {
    std::shared_lock lock(mtx);
    return frequencies;
}

void FreqStore::writeSnapshotToDisk() {
    // Copy under the lock, write with it released.
    std::unordered_map<std::string, int> copy = snapshot();

    const std::string tmpPath = filePath + ".tmp";
    {
        std::ofstream file(tmpPath);
        if (!file.is_open()) {
            return;
        }
        for (const auto& [token, freq] : copy) {
            // A newline would split one record into two unreadable ones.
            if (token.empty() || token.find('\n') != std::string::npos) continue;
            file << token << " " << freq << "\n";
        }
    }

    // Atomic replace - never leaves a partially written file.
    std::error_code ec;
    std::filesystem::rename(tmpPath, filePath, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
    }
}

void FreqStore::markDirty() {
    {
        std::lock_guard<std::mutex> lk(writerMtx);
        dirty = true;
    }
    writerCv.notify_one();
}

void FreqStore::writerLoop() {
    for (;;) {
        bool shouldWrite = false;
        bool shouldStop  = false;

        {
            std::unique_lock<std::mutex> lk(writerMtx);
            writerCv.wait(lk, [this] { return dirty || stopping; });

            shouldWrite = dirty;
            shouldStop  = stopping;
            dirty = false;          // any bumps arriving after this point set it
                                    // again and get picked up on the next pass
        }

        if (shouldWrite) {
            writeSnapshotToDisk();
        }
        if (shouldStop) {
            return;
        }
    }
}

void FreqStore::save() {
    // Clear dirty first: a racing bump costs one redundant write
    // rather than a dropped update.
    {
        std::lock_guard<std::mutex> lk(writerMtx);
        dirty = false;
    }
    writeSnapshotToDisk();
}

int FreqStore::get(const std::string& token) const {
    std::shared_lock lock(mtx);
    auto it = frequencies.find(token);
    return (it != frequencies.end()) ? it->second : 0;
}

void FreqStore::bump(const std::string& token, int amount) {
    {
        // Was a shared_lock around a mutation plus a save() that re-locked
        // the same shared_mutex - a data race and a recursive acquire.
        std::unique_lock lock(mtx);
        frequencies[token] += amount;
    }
    markDirty();
}

void FreqStore::set(const std::string& token, int freq) {
    {
        std::unique_lock lock(mtx);
        frequencies[token] = freq;
    }
    markDirty();
}
