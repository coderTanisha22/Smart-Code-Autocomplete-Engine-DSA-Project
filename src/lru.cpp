#include "../include/lru.h"
using namespace std;

lru_cache::lru_cache(int cap){
    this->cap = cap;
    head = nullptr;
    tail = nullptr;
}

lru_cache::~lru_cache(){
    clear();
}

void lru_cache::addNodeToFront(Node* node){
    if(node == nullptr) return;
    node->next = head;
    node->prev = nullptr;
    
    if(head != nullptr) head->prev = node;
    head = node;
    
    if(tail == nullptr) tail = node;
}

void lru_cache::removeNode(Node* node){
    if(node == nullptr) return;
    if(node == head){
        head = node->next;
        if(head) head->prev = nullptr;
    }
    else if(node == tail){
        tail = node->prev;
        if(tail) tail->next = nullptr;
    } 
    else{
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }
    node->prev = nullptr;
    node->next = nullptr;
}

void lru_cache::moveNodeToFront(Node* node){
    removeNode(node);
    addNodeToFront(node);
}

void lru_cache::removeLRUNode(){
    if(tail == nullptr) return;
    
    cacheMap.erase(tail->key);
    Node* old_tail = tail;
    removeNode(old_tail);
    delete old_tail;
}

vector<string> lru_cache::get(const string& key){
    std::unique_lock lock(mtx);
    if(cacheMap.find(key) == cacheMap.end()) return {};
    Node* node = cacheMap[key];
    moveNodeToFront(node);
    return node->val;
}

void lru_cache::put(const string& key, const vector<string>& val){
    std::unique_lock lock(mtx);
    if(cacheMap.find(key) != cacheMap.end()){
        Node* node = cacheMap[key];
        node->val = val;
        moveNodeToFront(node);
    }
    else{
        Node* new_node = new Node(key,val);
        addNodeToFront(new_node);
        cacheMap[key] = new_node;
        
        if((int)cacheMap.size()>cap) removeLRUNode();
    }
}

bool lru_cache::exists(const string& key) {
    std::shared_lock lock(mtx);
    return cacheMap.find(key) != cacheMap.end();
}

void lru_cache::clear() {
    std::unique_lock lock(mtx);
    Node* curr = head;
    while (curr != nullptr) {
        Node* next = curr->next;
        delete curr;
        curr = next;
    }

    head = nullptr;
    tail = nullptr;
    cacheMap.clear();
}
