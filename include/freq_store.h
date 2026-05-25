#ifndef FREQ_STORE_H
#define FREQ_STORE_H

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <thread>
#include <condition_variable>

/**
 * FreqStore - token usage counts, persisted to disk.
 *
 * mtx (shared_mutex) guards `frequencies`: readers share, writers are exclusive.
 * writerMtx guards the dirty/stopping flags for the background writer.
 *
 * bump() updates the map and returns; a writer thread coalesces bumps into one
 * snapshot write, so file I/O stays off the keystroke path. The snapshot is
 * copied under a shared lock and written after releasing it.
 */
class FreqStore {
private:
    std::unordered_map<std::string, int> frequencies;
    std::string filePath;

    mutable std::shared_mutex mtx;

    std::thread writer;
    std::mutex writerMtx;
    std::condition_variable writerCv;
    bool dirty = false;
    bool stopping = false;

    void writerLoop();
    void writeSnapshotToDisk();
    std::unordered_map<std::string, int> snapshot() const;
    void markDirty();

public:
    explicit FreqStore(const std::string& path);
    ~FreqStore();

    // Owns a thread and a mutex - neither is copyable.
    FreqStore(const FreqStore&) = delete;
    FreqStore& operator=(const FreqStore&) = delete;

    void load();
    void save();                                    // synchronous flush
    int get(const std::string& token) const;
    void bump(const std::string& token, int amount = 1);
    void set(const std::string& token, int freq);
};

#endif
