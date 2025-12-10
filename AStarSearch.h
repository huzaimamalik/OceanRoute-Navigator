#ifndef ASTAR_SEARCH_H
#define ASTAR_SEARCH_H

#include <string>
#include "Graph.h"
#include "Journey.h"
#include "PriorityQueue.h"
#include "ShortestPath.h"

using namespace std;

struct AStarPortCoord {
    string name;
    float x;
    float y;
};

const AStarPortCoord ASTAR_PORT_COORDS[] = {

    {"London", 800, 200},
    {"Dublin", 760, 195},
    {"Hamburg", 830, 190},
    {"Rotterdam", 815, 198},
    {"Antwerp", 810, 202},
    {"Marseille", 820, 275},
    {"Genoa", 835, 265},
    {"Lisbon", 745, 295},
    {"Copenhagen", 845, 175},
    {"Oslo", 840, 150},
    {"Stockholm", 875, 155},
    {"Helsinki", 905, 145},
    {"Athens", 895, 310},
    {"Istanbul", 930, 295},

    {"Dubai", 1030, 385},
    {"AbuDhabi", 1020, 395},
    {"Jeddah", 965, 400},
    {"Doha", 1010, 390},

    {"Alexandria", 920, 340},
    {"CapeTown", 870, 610},
    {"Durban", 925, 570},
    {"PortLouis", 1020, 530},

    {"Karachi", 1055, 395},
    {"Mumbai", 1080, 435},
    {"Colombo", 1100, 485},
    {"Chittagong", 1150, 415},

    {"Singapore", 1185, 505},
    {"Jakarta", 1205, 545},
    {"Manila", 1290, 430},
    {"HongKong", 1255, 375},
    {"Shanghai", 1280, 325},
    {"Tokyo", 1370, 285},
    {"Osaka", 1355, 305},
    {"Busan", 1330, 300},

    {"Sydney", 1410, 605},
    {"Melbourne", 1385, 645},

    {"NewYork", 505, 285},
    {"Montreal", 495, 245},
    {"Vancouver", 390, 235},
    {"LosAngeles", 410, 345}
};

const int ASTAR_PORT_COUNT = 40;

struct AStarResult {
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

    AStarResult() : found(false), totalCost(0), nodesExpanded(0), exploredEdgeCount(0) {
        initJourney(journey);
    }
};

float calculateHeuristic(const string& fromPort, const string& destPort);
float calculateTimeHeuristic(const string& fromPort, const string& destPort);

bool getAStarPortCoords(const string& portName, float& x, float& y);

void findRouteAStar(Graph& g, const string& originPort, const string& destPort, AStarResult& result, const RoutePreferences* prefs = nullptr);

void findRouteAStarIgnoringDates(Graph& g, const string& originPort, const string& destPort, AStarResult& result, int maxLegs = 15, const RoutePreferences* prefs = nullptr);

void findFastestRouteAStarIgnoringDates(Graph& g, const string& originPort, const string& destPort, AStarResult& result, int maxLegs = 15, const RoutePreferences* prefs = nullptr);

string compareAStarVsDijkstra(Graph& g, const string& originPort, const string& destPort);

#endif
