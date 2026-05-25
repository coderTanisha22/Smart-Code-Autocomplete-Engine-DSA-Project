#include "../include/tst.h"
#include <algorithm>

TST::TST() : root(nullptr) {}

std::shared_ptr<TSTNode> TST::insertUtil(std::shared_ptr<TSTNode> node, 
    const std::string& word, int index) {
    if (node == nullptr) {
        node = std::make_shared<TSTNode>(word[index]);
    }
    
    if (word[index] < node->data) {
        node->left = insertUtil(node->left, word, index);
        } else if (word[index] > node->data) {
        node->right = insertUtil(node->right, word, index);
    } else {
        if (index < (int)word.length() - 1) {
            node->eq = insertUtil(node->eq, word, index + 1);
        } else {
            node->isEndOfString = true;
        }
    }
    
    return node;
}

void TST::insert(const std::string& word) {
    if (word.empty()) return;
    root = insertUtil(root, word, 0);
}

std::shared_ptr<TSTNode> TST::searchPrefix(const std::string& prefix) {
    if (prefix.empty()) return root;
    
    auto node = root;
    int i = 0;
    
    while (node != nullptr && i < (int)prefix.length()) {
        if (prefix[i] < node->data) {
            node = node->left;
        } else if (prefix[i] > node->data) {
            node = node->right;
        } else {
            i++;
            if (i < (int)prefix.length()) {
                node = node->eq;
            }
        }
    }
    
    return (i == (int)prefix.length()) ? node : nullptr;
}

void TST::collectWords(std::shared_ptr<TSTNode> node,
        std::string prefix,
        std::vector<std::string>& results,
        int limit) {
    if (node == nullptr) return;

    // The walk is alphabetical, so once we hold `limit` results the remaining
    // nodes can only produce words we would discard. Keeps this O(L + k).
    auto reached = [&]() { return limit >= 0 && (int)results.size() >= limit; };

    if (reached()) return;

    collectWords(node->left, prefix, results, limit);
    if (reached()) return;

    std::string current = prefix + node->data;

    if (node->isEndOfString) {
        results.push_back(current);
        if (reached()) return;
    }

    collectWords(node->eq, current, results, limit);
    if (reached()) return;

    collectWords(node->right, prefix, results, limit);
}

std::vector<std::string> TST::prefixSearch(const std::string& prefix, int k) {
    std::vector<std::string> results;
    
    if (k <= 0) return results;

    if (prefix.empty()) {
        collectWords(root, "", results, k);
        return results;
    }

    auto node = searchPrefix(prefix);

    if (node == nullptr) {
        return results;
    }

    // The prefix may itself be a word; it sorts first and counts toward k.
    if (node->isEndOfString) {
        results.push_back(prefix);
    }

    collectWords(node->eq, prefix, results, k);

    return results;
}

bool TST::search(const std::string& word) {
    if (word.empty()) return false;
    
    auto node = root;
    int i = 0;
    
    while (node != nullptr && i < (int)word.length()) {
        if (word[i] < node->data) {
            node = node->left;
        } else if (word[i] > node->data) {
            node = node->right;
        } else {
            i++;
            if (i < (int)word.length()) {
                node = node->eq;
            }
        }
    }
    
    return (node != nullptr && i == (int)word.length() && node->isEndOfString);
}

void TST::getAllWords(std::vector<std::string>& results) {
    collectWords(root, "", results, -1);   // -1 = unbounded
}

