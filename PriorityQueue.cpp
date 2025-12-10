#include "PriorityQueue.h"
#include <limits.h>

static int parent(int i) { return (i - 1) / 2; }
static int leftChild(int i) { return 2 * i + 1; }
static int rightChild(int i) { return 2 * i + 2; }

static void swapNodes(PQNode& a, PQNode& b) {
    PQNode temp = a;
  a = b;
    b = temp;
}

static void heapifyUp(PriorityQueue& pq, int index) {
    while (index > 0) {
        int parentIdx = parent(index);
        if (pq.heap[index].priority < pq.heap[parentIdx].priority) {
     swapNodes(pq.heap[index], pq.heap[parentIdx]);
            index = parentIdx;
        } else {
      break;
        }
    }
}

static void heapifyDown(PriorityQueue& pq, int index) {
    while (true) {
        int smallest = index;
        int left = leftChild(index);
 int right = rightChild(index);

    if (left < pq.size && pq.heap[left].priority < pq.heap[smallest].priority) {
         smallest = left;
        }
     if (right < pq.size && pq.heap[right].priority < pq.heap[smallest].priority) {
    smallest = right;
 }

     if (smallest != index) {
            swapNodes(pq.heap[index], pq.heap[smallest]);
  index = smallest;
      } else {
            break;
     }
    }
}

void initPriorityQueue(PriorityQueue& pq, int capacity) {
    pq.heap = new PQNode[capacity];
    pq.size = 0;
    pq.capacity = capacity;
}

bool isEmpty(const PriorityQueue& pq) {
    return pq.size == 0;
}

void push(PriorityQueue& pq, const string& nodeName, int priority) {

    if (pq.size >= pq.capacity) {

        return;
  }

    pq.heap[pq.size].nodeName = nodeName;
    pq.heap[pq.size].priority = priority;

    heapifyUp(pq, pq.size);
    pq.size++;
}

bool pop(PriorityQueue& pq, string& nodeName, int& priority) {
    if (isEmpty(pq)) {
      return false;
    }

    nodeName = pq.heap[0].nodeName;
    priority = pq.heap[0].priority;

    pq.size--;
    if (pq.size > 0) {
        pq.heap[0] = pq.heap[pq.size];

        heapifyDown(pq, 0);
    }

    return true;
}

void clearPriorityQueue(PriorityQueue& pq) {
    if (pq.heap != nullptr) {
   delete[] pq.heap;
        pq.heap = nullptr;
    }
    pq.size = 0;
    pq.capacity = 0;
}
