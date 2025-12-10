#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <string>
using namespace std;

struct PQNode {
    string nodeName;
    int priority;
};

struct PriorityQueue {
    PQNode* heap;
    int size;
    int capacity;

    PriorityQueue() : heap(nullptr), size(0), capacity(0) {}
};

void initPriorityQueue(PriorityQueue& pq, int capacity);

bool isEmpty(const PriorityQueue& pq);

void push(PriorityQueue& pq, const string& nodeName, int priority);

bool pop(PriorityQueue& pq, string& nodeName, int& priority);

void clearPriorityQueue(PriorityQueue& pq);

#endif
