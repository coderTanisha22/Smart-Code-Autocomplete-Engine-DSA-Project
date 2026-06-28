#ifndef LRU_H
#define LRU_H

#include<string>
#include<vector>
#include<unordered_map>
#include <shared_mutex>



using namespace std;

struct Node{
    string key;
    vector<string> val;
    Node* prev;
    Node* next;
    
    Node(string k, vector<string>v){
        key=k;
        val=v;
        prev=nullptr;
        next=nullptr;
    }
};

class lru_cache{
private:
    int cap;
    unordered_map<string, Node*>cacheMap;
    Node* head;
    Node* tail;
    mutable std::shared_mutex mtx;

    void addNodeToFront(Node* node);
    void removeNode(Node* node);
    void moveNodeToFront(Node* node);
    void removeLRUNode();

public:
    lru_cache(int cap);
    vector<string>get(const string& key);
    void put(const string& key, const vector<string>& val);
    bool exists(const string& key);
    void clear();
};

using LRUCache = lru_cache;

#endif
