#include "MultiLegBuilder.h"
#include "ShortestPath.h"
#include "RoutePreferences.h"
#include <iostream>

MultiLegRouteBuilder::MultiLegRouteBuilder(Graph* graph)
    : head(nullptr), tail(nullptr), nodeCount(0), graphRef(graph) {
}

MultiLegRouteBuilder::~MultiLegRouteBuilder() {
    clear();
}

bool MultiLegRouteBuilder::appendPort(const string& portName) {

    Port* port = findPort(*graphRef, portName);
    if (!port) {
        return false;
    }

    if (head == nullptr) {
        MultiLegNode* newNode = new MultiLegNode(portName);
        head = tail = newNode;
        nodeCount = 1;
        return true;
    }

    MultiLegNode* newNode = new MultiLegNode(portName);
    newNode->prev = tail;
    tail->next = newNode;
    tail = newNode;
    nodeCount++;

    return true;
}

bool MultiLegRouteBuilder::insertPortAfter(MultiLegNode* afterNode, const string& portName) {
    if (!afterNode) {
        return appendPort(portName);
    }

    Port* port = findPort(*graphRef, portName);
    if (!port) {
        return false;
    }

    MultiLegNode* newNode = new MultiLegNode(portName);
    newNode->prev = afterNode;
    newNode->next = afterNode->next;

    if (afterNode->next) {
        afterNode->next->prev = newNode;
    } else {
        tail = newNode;
    }

    afterNode->next = newNode;
    nodeCount++;

    return true;
}

bool MultiLegRouteBuilder::deleteNode(MultiLegNode* node) {
    if (!node || !head) {
        return false;
    }

    if (node->prev) {
        node->prev->next = node->next;
    } else {
        head = node->next;
    }

    if (node->next) {
        node->next->prev = node->prev;
    } else {
        tail = node->prev;
    }

    delete node;
    nodeCount--;

    return true;
}

void MultiLegRouteBuilder::clear() {
    MultiLegNode* current = head;
    while (current) {
        MultiLegNode* next = current->next;
        delete current;
        current = next;
    }
    head = tail = nullptr;
    nodeCount = 0;
}

MultiLegNode* MultiLegRouteBuilder::getNodeAtIndex(int index) const {
    if (index < 0 || index >= nodeCount) {
        return nullptr;
    }

    MultiLegNode* current = head;
    for (int i = 0; i < index && current; i++) {
        current = current->next;
    }
    return current;
}

bool MultiLegRouteBuilder::hasValidRoute(const string& fromPort, const string& toPort) const {
    Port* from = findPort(*graphRef, fromPort);
    if (!from) return false;

    Route* route = from->routeHead;
    while (route) {
        if (route->destinationPort == toPort) {
            return true;
        }
        route = route->next;
    }

    return false;
}

int MultiLegRouteBuilder::validateFullJourney() const {
    if (!head || !head->next) {
        return -1;
    }

    int segmentIndex = 0;
    MultiLegNode* current = head;

    while (current->next) {
        if (!hasValidRoute(current->portName, current->next->portName)) {
            return segmentIndex;
        }
        current = current->next;
        segmentIndex++;
    }

    return -1;
}

MultiLegRouteBuilder::SegmentResult MultiLegRouteBuilder::findSegmentRoute(
    const string& fromPort, const string& toPort) const {

    SegmentResult result;
    result.fromPort = fromPort;
    result.toPort = toPort;
    result.valid = false;
    result.cost = 0;
    result.legs = 0;
    result.pathPortCount = 0;

    ShortestPathResult pathResult;
    findCheapestRoute(*graphRef, fromPort, toPort, pathResult);

    if (!pathResult.found || pathResult.journey.legCount == 0) {
        result.errorMessage = "No valid route found for segment " + fromPort + " -> " + toPort;
        return result;
    }

    result.valid = true;
    result.cost = pathResult.totalCost;
    result.legs = pathResult.journey.legCount;

    BookedLeg* leg = pathResult.journey.head;
    bool first = true;
    while (leg && result.pathPortCount < 50) {
        if (first) {
            result.pathPorts[result.pathPortCount++] = leg->originPort;
            first = false;
        }
        if (result.pathPortCount < 50) {
            result.pathPorts[result.pathPortCount++] = leg->destinationPort;
        }
        leg = leg->next;
    }

    return result;
}

void MultiLegRouteBuilder::findCompleteRoute(SegmentResult results[], int& resultCount) const {
    resultCount = 0;

    if (!head || !head->next) {
        return;
    }

    MultiLegNode* current = head;
    while (current->next) {
        results[resultCount] = findSegmentRoute(current->portName, current->next->portName);
        resultCount++;
        current = current->next;
    }
}
