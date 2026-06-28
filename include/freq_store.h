#ifndef FREQ_STORE_H
#define FREQ_STORE_H

#include <string>
#include <unordered_map>
#include <shared_mutex>

class FreqStore {
private:
    std::unordered_map<std::string, int> frequencies;
    std::string filePath;
    mutable std::shared_mutex mtx;

public:
    FreqStore(const std::string& path);
    void load();
    void save();
    int get(const std::string& token);
    void bump(const std::string& token, int amount = 1);
    void set(const std::string& token, int freq);
};

#endif
