#ifndef MINHEAP_H
#define MINHEAP_H

#include <vector>
#include <string>
#include <utility>

class MinHeap{
private:
    std::vector<std::pair<double,std::string>> heap;
    int maxSize;
    
    void heapifyUp(int index);
    void heapifyDown(int index);
    int parent(int i){
        return (i-1)/2; 
    }
    int leftChild(int i){ 
        return 2*i+1;
    }
    int rightChild(int i){
        return 2*i+2;
    }

    public:
    MinHeap(int k);
    void insert(double score,const std::string& word);

    std::pair<double,std::string> extractMin();
    std::pair<double,std::string> getMin();
    int size() const{
        return heap.size(); 
    }
    bool isEmpty() const{
        return heap.empty(); 
    }
    bool isFull() const{
        return (int)heap.size()>=maxSize;
    }
    std::vector<std::pair<double,std::string>> getAll();
    void clear();
};

#endif

