#ifndef MULTILEG_BUILDER_H
#define MULTILEG_BUILDER_H

#include <string>
#include "Graph.h"

using namespace std;

struct MultiLegNode {
    string portName;
    MultiLegNode* next;
    MultiLegNode* prev;

    MultiLegNode(const string& name)
        : portName(name), next(nullptr), prev(nullptr) {}
};

class MultiLegRouteBuilder {
private:
    MultiLegNode* head;
    MultiLegNode* tail;
    int nodeCount;
    Graph* graphRef;

public:
    MultiLegRouteBuilder(Graph* graph);
    ~MultiLegRouteBuilder();

    bool appendPort(const string& portName);

    bool insertPortAfter(MultiLegNode* afterNode, const string& portName);

    bool deleteNode(MultiLegNode* node);

    void clear();

    MultiLegNode* getHead() const { return head; }

    MultiLegNode* getTail() const { return tail; }

    int getNodeCount() const { return nodeCount; }

    bool isEmpty() const { return head == nullptr; }

    MultiLegNode* getNodeAtIndex(int index) const;

    bool hasValidRoute(const string& fromPort, const string& toPort) const;

    int validateFullJourney() const;

    struct SegmentResult {
        bool valid = false;
        string fromPort;
        string toPort;
        int cost = 0;
        int legs = 0;
        string errorMessage;

        int pathPortCount = 0;
        string pathPorts[50];
    };

    SegmentResult findSegmentRoute(const string& fromPort, const string& toPort) const;

    void findCompleteRoute(SegmentResult results[], int& resultCount) const;
};

#endif
