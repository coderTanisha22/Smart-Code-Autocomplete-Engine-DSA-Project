#ifndef TST_H
#define TST_H

#include <string>
#include <vector>
#include <memory>

struct TSTNode {
    char data;
    bool isEndOfString;
    std::shared_ptr<TSTNode> left;
    std::shared_ptr<TSTNode> eq;
    std::shared_ptr<TSTNode> right;
    
    TSTNode(char c) : data(c), isEndOfString(false), 
                    left(nullptr), eq(nullptr), right(nullptr) {}
};

class TST {
private:
    std::shared_ptr<TSTNode> root;
    
    std::shared_ptr<TSTNode> insertUtil(std::shared_ptr<TSTNode> node, 
                                        const std::string& word, int index);
    
    // Collects words in lexicographic order.
    // `limit` bounds how many results are gathered: the traversal stops as soon
    // as `results` holds `limit` entries. Pass -1 for an unbounded walk.
    void collectWords(std::shared_ptr<TSTNode> node,
                    std::string prefix,
                    std::vector<std::string>& results,
                    int limit = -1);

    std::shared_ptr<TSTNode> searchPrefix(const std::string& prefix);

public:
    TST();
    void insert(const std::string& word);
    std::vector<std::string> prefixSearch(const std::string& prefix, int k = 10);
    bool search(const std::string& word);
    void getAllWords(std::vector<std::string>& results);
};
#endif