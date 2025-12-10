#ifndef SHORTEST_PATH_H
#define SHORTEST_PATH_H

#include <string>
#include "Graph.h"
#include "Journey.h"
#include "PriorityQueue.h"
#include "RoutePreferences.h"

using namespace std;

struct ShortestPathResult {
    bool found;
    int totalCost;
    int nodesExpanded;
    BookedJourney journey;

    struct ExploredEdge {
        string fromPort;
        string toPort;
    };
    ExploredEdge exploredEdges[500];
    int exploredEdgeCount;

    ShortestPathResult() : found(false), totalCost(0), nodesExpanded(0), exploredEdgeCount(0) {
        initJourney(journey);
    }
};

void findCheapestRoute(Graph& g, const string& originPort, const string& destPort, ShortestPathResult& result);

void findCheapestRouteIgnoringDates(Graph& g, const string& originPort, const string& destPort, ShortestPathResult& result, int maxLegs = 15, const RoutePreferences* prefs = nullptr);

void findFastestRouteIgnoringDates(Graph& g, const string& originPort, const string& destPort, ShortestPathResult& result, int maxLegs = 15, const RoutePreferences* prefs = nullptr);

#endif
