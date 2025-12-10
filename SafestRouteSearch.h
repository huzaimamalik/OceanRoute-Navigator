#ifndef SAFEST_ROUTE_SEARCH_H
#define SAFEST_ROUTE_SEARCH_H

#include <string>
#include "Graph.h"
#include "Route.h"
#include "DateTime.h"
#include "RoutePreferences.h"

using namespace std;

// Generic journey representation using linked list of route pointers
struct SafeJourneyLeg {
    Route* route;
    SafeJourneyLeg* next;
    
    SafeJourneyLeg() : route(nullptr), next(nullptr) {}
};

struct SafeJourney {
    SafeJourneyLeg* legsHead;
    int legCount;
    int totalCost;
    int totalTime;
    int safetyScore;
    
    SafeJourney() : legsHead(nullptr), legCount(0), totalCost(0), totalTime(0), safetyScore(0) {}
};

// List to store multiple journey results
struct SafeJourneyList {
    SafeJourney* journeys;
    int count;
    int capacity;
    
    SafeJourneyList() : journeys(nullptr), count(0), capacity(0) {}
};

// Core DFS-based safest route search - finds all valid routes
void findAllSafestRoutes(
    Graph& g,
    const string& originPort,
    const string& destPort,
    const Date& searchDate,
    const RoutePreferences& prefs,
    SafeJourneyList& allJourneys,
    int maxDepth = 15
);

// Original function - finds single best route
void findSafestRoute(
    Graph& g,
    const string& originPort,
    const string& destPort,
    const Date& searchDate,
    const RoutePreferences& prefs,
    SafeJourney& bestJourney,
    int maxDepth = 15
);

// Helper functions
void clearSafeJourney(SafeJourney& journey);
void copySafeJourney(const SafeJourney& src, SafeJourney& dest);
int calculateSafetyScore(const SafeJourney& journey, const RoutePreferences& prefs);
void printSafeJourney(const SafeJourney& journey);
void clearSafeJourneyList(SafeJourneyList& list);
void addToSafeJourneyList(SafeJourneyList& list, const SafeJourney& journey);

#endif
