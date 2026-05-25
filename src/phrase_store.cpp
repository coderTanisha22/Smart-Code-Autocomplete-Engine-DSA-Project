#include "../include/phrase_store.h"
#include <sstream>

PhraseStore::PhraseStore(const string& path) : filePath(path) {
    load();
}

void PhraseStore::addPhrase(const string& trigger, const string& fullText) {
    // Don't store if trigger and snippet are the same (no learning value)
    if (trigger == fullText) {
        return;
    }

    // A trigger carrying a delimiter would not read back.
    if (trigger.find('|') != string::npos ||
        trigger.find('\n') != string::npos ||
        fullText.find('\n') != string::npos) {
        return;
    }

    // Don't store if snippet is too short (less than trigger + 3 chars)
    if (fullText.length() < trigger.length() + 3) {
        return;
    }

    // Check if this exact phrase already exists
    auto& phraseList = phrases[trigger];
    for (auto& phrase : phraseList) {
        if (phrase.snippet == fullText) {
            // Phrase exists, increment use count
            phrase.useCount++;
            return;
        }
    }

    // New phrase, add it
    phraseList.push_back(Phrase(trigger, fullText));
}

vector<Phrase> PhraseStore::getPhrases(const string& trigger) {
    if (phrases.find(trigger) == phrases.end()) {
        return vector<Phrase>();
    }

    auto phraseList = phrases[trigger];

    // Sort by use count (descending - most used first)
    sort(phraseList.begin(), phraseList.end(),
         [](const Phrase& a, const Phrase& b) {
             return a.useCount > b.useCount;
         });

    return phraseList;
}

vector<Phrase> PhraseStore::getTopPhrases(const string& trigger, int n) {
    auto allPhrases = getPhrases(trigger);

    if ((int)allPhrases.size() <= n) {
        return allPhrases;
    }

    return vector<Phrase>(allPhrases.begin(), allPhrases.begin() + n);
}

bool PhraseStore::hasPhrase(const string& trigger, const string& fullText) {
    if (phrases.find(trigger) == phrases.end()) {
        return false;
    }

    for (const auto& phrase : phrases[trigger]) {
        if (phrase.snippet == fullText) {
            return true;
        }
    }
    return false;
}

void PhraseStore::save() {
    ofstream file(filePath);
    if (!file.is_open()) {
        return;
    }

    // Format: trigger|snippet|useCount
    for (const auto& [trigger, phraseList] : phrases) {
        for (const auto& phrase : phraseList) {
            file << trigger << "|" << phrase.snippet << "|" << phrase.useCount << "\n";
        }
    }

    file.close();
}

void PhraseStore::load() {
    ifstream file(filePath);
    if (!file.is_open()) {
        return;
    }

    string line;
    while (getline(file, line)) {
        if (line.empty()) continue;

        // Format is trigger|snippet|useCount, but snippets are code and
        // contain '|' ("if(a||b)"). Splitting on every '|' left an empty
        // count, and stoi("") threw and aborted the program on startup.
        // Anchor on the first '|' and the last; the middle is the snippet.
        size_t first = line.find('|');
        size_t last  = line.rfind('|');

        if (first == string::npos || last == first) continue;   // malformed

        string trigger  = line.substr(0, first);
        string snippet  = line.substr(first + 1, last - first - 1);
        string countStr = line.substr(last + 1);

        if (trigger.empty() || snippet.empty()) continue;

        // A corrupt count must not take the editor down.
        int count = 1;
        try {
            count = stoi(countStr);
        } catch (const exception&) {
            continue;
        }
        if (count < 1) count = 1;

        Phrase phrase(trigger, snippet);
        phrase.useCount = count;
        phrases[trigger].push_back(phrase);
    }

    file.close();
}

int PhraseStore::getTotalPhrases() const {
    int total = 0;
    for (const auto& [trigger, phraseList] : phrases) {
        total += phraseList.size();
    }
    return total;
}
