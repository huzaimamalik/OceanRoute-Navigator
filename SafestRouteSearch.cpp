#include "SafestRouteSearch.h"
#include "RoutePreferences.h"
#include <iostream>
#include <climits>

using namespace std;

// Utility: Calculate travel time for a route
static int calculateRouteTravelTimeMinutes(const Route* route) {
    int depMinutes = route->departureTime.hour * 60 + route->departureTime.minute;
    int arrMinutes = route->arrivalTime.hour * 60 + route->arrivalTime.minute;
    
    if (arrMinutes >= depMinutes) {
        return arrMinutes - depMinutes;
    } else {
        return (24 * 60 - depMinutes) + arrMinutes;
    }
}

// Utility: Check if layover between two routes is valid
static bool isValidLayover(const Route* prevRoute, const Route* nextRoute, int minLayoverMinutes = 60) {
    if (!prevRoute || !nextRoute) return true;
    
    // Check if dates are compatible
    int prevDay = prevRoute->voyageDate.year * 365 + prevRoute->voyageDate.month * 31 + prevRoute->voyageDate.day;
    int nextDay = nextRoute->voyageDate.year * 365 + nextRoute->voyageDate.month * 31 + nextRoute->voyageDate.day;
    
    if (nextDay < prevDay) return false; // Going backwards in time
    
    if (nextDay == prevDay) {
        // Same day - check if enough layover time
        int arrMinutes = prevRoute->arrivalTime.hour * 60 + prevRoute->arrivalTime.minute;
        int depMinutes = nextRoute->departureTime.hour * 60 + nextRoute->departureTime.minute;
        return (depMinutes >= arrMinutes + minLayoverMinutes);
    }
    
    // Different day - valid as long as next departure is after previous arrival
    return true;
}

// Utility: Check if route departs on or after a given date
static bool isRouteOnOrAfterDate(const Route* route, const Date& searchDate) {
    // Check if route departs exactly on the search date
    return (route->voyageDate.year == searchDate.year &&
            route->voyageDate.month == searchDate.month &&
            route->voyageDate.day == searchDate.day);
}

// Utility: Add a leg to journey
static void addLegToJourney(SafeJourney& journey, Route* route) {
    SafeJourneyLeg* newLeg = new SafeJourneyLeg();
    newLeg->route = route;
    newLeg->next = nullptr;
    
    if (journey.legsHead == nullptr) {
        journey.legsHead = newLeg;
    } else {
        SafeJourneyLeg* current = journey.legsHead;
        while (current->next != nullptr) {
            current = current->next;
        }
        current->next = newLeg;
    }
    
    journey.legCount++;
    journey.totalCost += route->voyageCost;
    journey.totalTime += calculateRouteTravelTimeMinutes(route);
}

// Utility: Remove last leg from journey
static void removeLastLegFromJourney(SafeJourney& journey) {
    if (journey.legsHead == nullptr) return;
    
    if (journey.legsHead->next == nullptr) {
        // Only one leg
        journey.totalCost -= journey.legsHead->route->voyageCost;
        journey.totalTime -= calculateRouteTravelTimeMinutes(journey.legsHead->route);
        delete journey.legsHead;
        journey.legsHead = nullptr;
        journey.legCount = 0;
    } else {
        // Find second-to-last leg
        SafeJourneyLeg* current = journey.legsHead;
        while (current->next->next != nullptr) {
            current = current->next;
        }
        
        journey.totalCost -= current->next->route->voyageCost;
        journey.totalTime -= calculateRouteTravelTimeMinutes(current->next->route);
        delete current->next;
        current->next = nullptr;
        journey.legCount--;
    }
}

// Utility: Get last leg from journey
static Route* getLastLeg(const SafeJourney& journey) {
    if (journey.legsHead == nullptr) return nullptr;
    
    SafeJourneyLeg* current = journey.legsHead;
    while (current->next != nullptr) {
        current = current->next;
    }
    return current->route;
}

// Calculate safety score (lower is better/safer)
int calculateSafetyScore(const SafeJourney& journey, const RoutePreferences& prefs) {
    int score = 0;
    
    // Penalize more legs (each leg adds risk)
    score += journey.legCount * 100;
    
    // Penalize forbidden ports
    SafeJourneyLeg* current = journey.legsHead;
    while (current != nullptr) {
        if (isPortForbidden(prefs, current->route->destinationPort)) {
            score += 1000; // Heavy penalty for forbidden ports
        }
        
        // Check if company is not in allowed list
        if (prefs.allowedCompaniesCount > 0) {
            if (!isCompanyAllowed(prefs, current->route->shippingCompany)) {
                score += 500; // Penalty for non-preferred companies
            }
        }
        
        current = current->next;
    }
    
    // Prefer shorter time and lower cost
    score += journey.totalTime / 10;  // Time factor (divided for scaling)
    score += journey.totalCost / 100; // Cost factor (divided for scaling)
    
    return score;
}

// Clear journey and free memory
void clearSafeJourney(SafeJourney& journey) {
    SafeJourneyLeg* current = journey.legsHead;
    while (current != nullptr) {
        SafeJourneyLeg* next = current->next;
        delete current;
        current = next;
    }
    
    journey.legsHead = nullptr;
    journey.legCount = 0;
    journey.totalCost = 0;
    journey.totalTime = 0;
    journey.safetyScore = 0;
}

// Copy journey (deep copy)
void copySafeJourney(const SafeJourney& src, SafeJourney& dest) {
    clearSafeJourney(dest);
    
    SafeJourneyLeg* current = src.legsHead;
    while (current != nullptr) {
        addLegToJourney(dest, current->route);
        current = current->next;
    }
    
    dest.safetyScore = src.safetyScore;
}

// Print journey for debugging
void printSafeJourney(const SafeJourney& journey) {
    cout << "Journey: " << journey.legCount << " legs, Cost: $" << journey.totalCost 
         << ", Time: " << journey.totalTime << " min, Safety: " << journey.safetyScore << endl;
    
    SafeJourneyLeg* current = journey.legsHead;
    int legNum = 1;
    while (current != nullptr) {
        Route* r = current->route;
        cout << "  Leg " << legNum++ << ": " << r->shippingCompany 
             << " - " << r->destinationPort 
             << " (Departs: " << r->voyageDate.day << "/" << r->voyageDate.month 
             << " at " << r->departureTime.hour << ":" << r->departureTime.minute
             << ", Cost: $" << r->voyageCost << ")" << endl;
        current = current->next;
    }
}

// DFS recursive helper to explore all possible routes
static void dfsSafestRoute(
    Graph& g,
    const string& currentPort,
    const string& destPort,
    const Date& searchDate,
    const RoutePreferences& prefs,
    SafeJourney& currentJourney,
    SafeJourney& bestJourney,
    bool* visited,
    Port** portArray,
    int portCount,
    int maxDepth,
    int& solutionsFound
) {
    // Base case: reached destination
    if (currentPort == destPort) {
        int currentScore = calculateSafetyScore(currentJourney, prefs);
        currentJourney.safetyScore = currentScore;
        
        // Update best journey if this is better (lower score = safer)
        if (bestJourney.legCount == 0 || currentScore < bestJourney.safetyScore) {
            copySafeJourney(currentJourney, bestJourney);
        }
        solutionsFound++;
        return;
    }
    
    // Pruning: max depth exceeded
    if (currentJourney.legCount >= maxDepth) {
        return;
    }
    
    // Pruning: cost exceeds max
    if (prefs.useMaxTotalCost && currentJourney.totalCost > prefs.maxTotalCost) {
        return;
    }
    
    // Pruning: legs exceed max
    if (prefs.useMaxLegs && currentJourney.legCount >= prefs.maxLegs) {
        return;
    }
    
    // Find current port in graph
    Port* currentPortNode = nullptr;
    for (int i = 0; i < portCount; i++) {
        if (portArray[i]->name == currentPort) {
            currentPortNode = portArray[i];
            break;
        }
    }
    
    if (!currentPortNode) return;
    
    // Mark current port as visited
    int currentPortIdx = -1;
    for (int i = 0; i < portCount; i++) {
        if (portArray[i]->name == currentPort) {
            currentPortIdx = i;
            break;
        }
    }
    if (currentPortIdx >= 0) {
        visited[currentPortIdx] = true;
    }
    
    // Get last route for layover validation
    Route* lastRoute = getLastLeg(currentJourney);
    
    // Explore all outgoing routes from current port
    Route* route = currentPortNode->routeHead;
    while (route != nullptr) {
        string nextPort = route->destinationPort;
        
        // Find next port index
        int nextPortIdx = -1;
        for (int i = 0; i < portCount; i++) {
            if (portArray[i]->name == nextPort) {
                nextPortIdx = i;
                break;
            }
        }
        
        // Check constraints
        bool canUseRoute = true;
        
        // For first leg: check if route departs on or after search date
        if (canUseRoute && currentJourney.legCount == 0) {
            if (!isRouteOnOrAfterDate(route, searchDate)) {
                canUseRoute = false;
            }
        }
        
        // Avoid cycles (already visited)
        if (canUseRoute && nextPortIdx >= 0 && visited[nextPortIdx] && nextPort != destPort) {
            canUseRoute = false;
        }
        
        // Check forbidden ports
        if (canUseRoute && isPortForbidden(prefs, nextPort)) {
            canUseRoute = false;
        }
        
        // Check allowed companies
        if (canUseRoute && prefs.allowedCompaniesCount > 0) {
            if (!isCompanyAllowed(prefs, route->shippingCompany)) {
                canUseRoute = false;
            }
        }
        
        // Check layover validity
        if (canUseRoute && lastRoute != nullptr) {
            if (!isValidLayover(lastRoute, route)) {
                canUseRoute = false;
            }
        }
        
        // Recursively explore this route
        if (canUseRoute) {
            addLegToJourney(currentJourney, route);
            
            dfsSafestRoute(g, nextPort, destPort, searchDate, prefs, currentJourney, bestJourney, 
                          visited, portArray, portCount, maxDepth, solutionsFound);
            
            removeLastLegFromJourney(currentJourney);
        }
        
        route = route->next;
    }
    
    // Unmark current port as visited (backtrack)
    if (currentPortIdx >= 0) {
        visited[currentPortIdx] = false;
    }
}

// Main entry point: Find the safest route using DFS
void findSafestRoute(
    Graph& g,
    const string& originPort,
    const string& destPort,
    const Date& searchDate,
    const RoutePreferences& prefs,
    SafeJourney& bestJourney,
    int maxDepth
) {
    clearSafeJourney(bestJourney);
    
    // Build port array for easier access
    int portCount = g.portCount;
    if (portCount == 0) {
        cout << "Error: No ports in graph" << endl;
        return;
    }
    
    Port** portArray = new Port*[portCount];
    Port* current = g.portHead;
    int idx = 0;
    while (current != nullptr) {
        portArray[idx++] = current;
        current = current->next;
    }
    
    // Initialize visited array
    bool* visited = new bool[portCount];
    for (int i = 0; i < portCount; i++) {
        visited[i] = false;
    }
    
    // Create current journey for DFS exploration
    SafeJourney currentJourney;
    currentJourney.legsHead = nullptr;
    currentJourney.legCount = 0;
    currentJourney.totalCost = 0;
    currentJourney.totalTime = 0;
    currentJourney.safetyScore = 0;
    
    int solutionsFound = 0;
    
    cout << "\n========== SAFEST ROUTE SEARCH (DFS) ==========\n";
    cout << "Origin: " << originPort << " -> Destination: " << destPort << endl;
    cout << "Max Depth: " << maxDepth << " legs" << endl;
    cout << "Search Date: " << searchDate.day << "/" << searchDate.month << "/" << searchDate.year << endl;
    
    // Start DFS from origin
    dfsSafestRoute(g, originPort, destPort, searchDate, prefs, currentJourney, bestJourney,
                   visited, portArray, portCount, maxDepth, solutionsFound);
    
    cout << "Solutions explored: " << solutionsFound << endl;
    
    if (bestJourney.legCount > 0) {
        cout << "Best safest route found:" << endl;
        printSafeJourney(bestJourney);
    } else {
        cout << "No route found within constraints" << endl;
    }
    cout << "===============================================\n\n";
    
    // Cleanup
    clearSafeJourney(currentJourney);
    delete[] portArray;
    delete[] visited;
}

// Helper: Clear journey list
void clearSafeJourneyList(SafeJourneyList& list) {
    if (list.journeys != nullptr) {
        for (int i = 0; i < list.count; i++) {
            clearSafeJourney(list.journeys[i]);
        }
        delete[] list.journeys;
        list.journeys = nullptr;
    }
    list.count = 0;
    list.capacity = 0;
}

// Helper: Add journey to list
void addToSafeJourneyList(SafeJourneyList& list, const SafeJourney& journey) {
    if (list.count >= list.capacity) {
        int newCapacity = list.capacity == 0 ? 10 : list.capacity * 2;
        SafeJourney* newArray = new SafeJourney[newCapacity];
        
        for (int i = 0; i < list.count; i++) {
            copySafeJourney(list.journeys[i], newArray[i]);
        }
        
        if (list.journeys != nullptr) {
            for (int i = 0; i < list.count; i++) {
                clearSafeJourney(list.journeys[i]);
            }
            delete[] list.journeys;
        }
        
        list.journeys = newArray;
        list.capacity = newCapacity;
    }
    
    copySafeJourney(journey, list.journeys[list.count]);
    list.count++;
}

// DFS helper that collects ALL valid routes
static void dfsSafestRouteAll(
    Graph& g,
    const string& currentPort,
    const string& destPort,
    const Date& searchDate,
    const RoutePreferences& prefs,
    SafeJourney& currentJourney,
    SafeJourneyList& allJourneys,
    bool* visited,
    Port** portArray,
    int portCount,
    int maxDepth,
    int& solutionsFound
) {
    // Base case: reached destination
    if (currentPort == destPort) {
        int currentScore = calculateSafetyScore(currentJourney, prefs);
        currentJourney.safetyScore = currentScore;
        
        // Add this journey to the list
        addToSafeJourneyList(allJourneys, currentJourney);
        solutionsFound++;
        return;
    }
    
    // Pruning conditions
    if (currentJourney.legCount >= maxDepth) return;
    if (prefs.useMaxTotalCost && currentJourney.totalCost > prefs.maxTotalCost) return;
    if (prefs.useMaxLegs && currentJourney.legCount >= prefs.maxLegs) return;
    
    // Find current port
    Port* currentPortNode = nullptr;
    int currentPortIdx = -1;
    for (int i = 0; i < portCount; i++) {
        if (portArray[i]->name == currentPort) {
            currentPortNode = portArray[i];
            currentPortIdx = i;
            break;
        }
    }
    
    if (!currentPortNode) return;
    
    // Mark visited
    if (currentPortIdx >= 0) {
        visited[currentPortIdx] = true;
    }
    
    Route* lastRoute = getLastLeg(currentJourney);
    
    // Explore all routes
    Route* route = currentPortNode->routeHead;
    while (route != nullptr) {
        string nextPort = route->destinationPort;
        
        // Find next port index
        int nextPortIdx = -1;
        for (int i = 0; i < portCount; i++) {
            if (portArray[i]->name == nextPort) {
                nextPortIdx = i;
                break;
            }
        }
        
        // Check constraints
        bool canUseRoute = true;
        
        // First leg: check date
        if (canUseRoute && currentJourney.legCount == 0) {
            if (!isRouteOnOrAfterDate(route, searchDate)) {
                canUseRoute = false;
            }
        }
        
        // Avoid cycles
        if (canUseRoute && nextPortIdx >= 0 && visited[nextPortIdx] && nextPort != destPort) {
            canUseRoute = false;
        }
        
        // Check forbidden ports
        if (canUseRoute && isPortForbidden(prefs, nextPort)) {
            canUseRoute = false;
        }
        
        // Check companies
        if (canUseRoute && prefs.allowedCompaniesCount > 0) {
            if (!isCompanyAllowed(prefs, route->shippingCompany)) {
                canUseRoute = false;
            }
        }
        
        // Check layover
        if (canUseRoute && lastRoute != nullptr) {
            if (!isValidLayover(lastRoute, route)) {
                canUseRoute = false;
            }
        }
        
        // Recurse
        if (canUseRoute) {
            addLegToJourney(currentJourney, route);
            dfsSafestRouteAll(g, nextPort, destPort, searchDate, prefs, currentJourney, allJourneys,
                             visited, portArray, portCount, maxDepth, solutionsFound);
            removeLastLegFromJourney(currentJourney);
        }
        
        route = route->next;
    }
    
    // Backtrack
    if (currentPortIdx >= 0) {
        visited[currentPortIdx] = false;
    }
}

// Main function to find all safe routes
void findAllSafestRoutes(
    Graph& g,
    const string& originPort,
    const string& destPort,
    const Date& searchDate,
    const RoutePreferences& prefs,
    SafeJourneyList& allJourneys,
    int maxDepth
) {
    // Initialize
    allJourneys.count = 0;
    allJourneys.capacity = 0;
    allJourneys.journeys = nullptr;
    
    // Get port count
    int portCount = 0;
    Port* current = g.portHead;
    while (current != nullptr) {
        portCount++;
        current = current->next;
    }
    
    if (portCount == 0) return;
    
    // Build port array
    Port** portArray = new Port*[portCount];
    current = g.portHead;
    int idx = 0;
    while (current != nullptr) {
        portArray[idx++] = current;
        current = current->next;
    }
    
    // Initialize visited
    bool* visited = new bool[portCount];
    for (int i = 0; i < portCount; i++) {
        visited[i] = false;
    }
    
    // Create current journey
    SafeJourney currentJourney;
    currentJourney.legsHead = nullptr;
    currentJourney.legCount = 0;
    currentJourney.totalCost = 0;
    currentJourney.totalTime = 0;
    currentJourney.safetyScore = 0;
    
    int solutionsFound = 0;
    
    cout << "\n========== SAFEST ROUTE SEARCH (DFS - ALL ROUTES) ==========\n";
    cout << "Origin: " << originPort << " -> Destination: " << destPort << endl;
    cout << "Max Depth: " << maxDepth << " legs" << endl;
    cout << "Search Date: " << searchDate.day << "/" << searchDate.month << "/" << searchDate.year << endl;
    
    // Start DFS
    dfsSafestRouteAll(g, originPort, destPort, searchDate, prefs, currentJourney, allJourneys,
                     visited, portArray, portCount, maxDepth, solutionsFound);
    
    cout << "Total solutions found: " << allJourneys.count << endl;
    cout << "===============================================\n\n";
    
    // Cleanup
    clearSafeJourney(currentJourney);
    delete[] portArray;
    delete[] visited;
}

