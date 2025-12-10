

#include "AStarSearch.h"
#include "ShortestPath.h"
#include <limits.h>
#include <iostream>
#include <cmath>

using namespace std;

static int astarDateToAbsoluteDays(const Date& d) {
    return d.year * 365 + d.month * 31 + d.day;
}

static int astarTimeToMinutes(const Time& t) {
    return t.hour * 60 + t.minute;
}

// Validates if layover time between arrival and next departure is sufficient
static bool astarIsValidConnection(const Date& arrivalDate, const Time& arrivalTime, const Route* route, int minLayoverMinutes = 60) {
    int arrDays = astarDateToAbsoluteDays(arrivalDate);
    int depDays = astarDateToAbsoluteDays(route->voyageDate);

    if (depDays > arrDays) {
        return true;
    } else if (depDays == arrDays) {

        int arrMins = astarTimeToMinutes(arrivalTime);
        int depMins = astarTimeToMinutes(route->departureTime);
        return (depMins >= arrMins + minLayoverMinutes);
    }
    return false;
}

bool getAStarPortCoords(const string& portName, float& x, float& y) {
    for (int i = 0; i < ASTAR_PORT_COUNT; i++) {
        if (ASTAR_PORT_COORDS[i].name == portName) {
            x = ASTAR_PORT_COORDS[i].x;
            y = ASTAR_PORT_COORDS[i].y;
            return true;
        }
    }

    x = 900;
    y = 400;
    return false;
}

static float euclideanDistance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

// Calculates estimated cost from current port to destination using Euclidean distance
float calculateHeuristic(const string& fromPort, const string& destPort) {
    float x1, y1, x2, y2;

    if (!getAStarPortCoords(fromPort, x1, y1)) {
        return 0;
    }
    if (!getAStarPortCoords(destPort, x2, y2)) {
        return 0;
    }

    float distance = euclideanDistance(x1, y1, x2, y2);

    const float COST_PER_PIXEL = 10.0f;

    return distance * COST_PER_PIXEL;
}

// Calculates estimated travel time in minutes from current port to destination
float calculateTimeHeuristic(const string& fromPort, const string& destPort) {
    float x1, y1, x2, y2;

    if (!getAStarPortCoords(fromPort, x1, y1)) {
        return 0;
    }
    if (!getAStarPortCoords(destPort, x2, y2)) {
        return 0;
    }

    float distance = euclideanDistance(x1, y1, x2, y2);

    // Map calibration: Conservative estimate to ensure admissibility
    // We need heuristic to NEVER overestimate actual travel time
    // Using lower speed and distance factor to stay admissible
    const float PIXELS_TO_NAUTICAL_MILES = 2.0f;  // Reduced from 4.0
    const float AVERAGE_SHIP_SPEED_KNOTS = 30.0f;  // Increased from 22.0 (faster = lower time estimate)

    float nauticalMiles = distance * PIXELS_TO_NAUTICAL_MILES;
    float hours = nauticalMiles / AVERAGE_SHIP_SPEED_KNOTS;
    float minutes = hours * 60.0f;

    return minutes;
}

static Port** buildAStarPortArray(Graph& g, int& portCount) {
    portCount = g.portCount;
    if (portCount == 0) return nullptr;

    Port** portArray = new Port*[portCount];
    Port* current = g.portHead;
    int idx = 0;
    while (current != nullptr) {
        portArray[idx++] = current;
        current = current->next;
    }
    return portArray;
}

static int findAStarPortIndex(Port** portArray, int portCount, const string& portName) {
    for (int i = 0; i < portCount; i++) {
        if (portArray[i]->name == portName) {
            return i;
        }
    }
    return -1;
}

struct AStarState {
    int portIndex;
    int gCost;
    float hCost;
    float fCost;
    Date arrivalDate;
    Time arrivalTime;
    int parentStateIdx;
    Route* routeUsed;

    AStarState() : portIndex(-1), gCost(INT_MAX), hCost(0.0f), fCost(INT_MAX), arrivalDate{0,0,0}, arrivalTime{0,0}, parentStateIdx(-1), routeUsed(nullptr) {}
};

struct AStarStatePQ {
    AStarState* heap;
    int capacity;
    int size;
};

static void initAStarStatePQ(AStarStatePQ& pq, int cap) {
    pq.capacity = cap;
    pq.size = 0;
    pq.heap = new AStarState[cap];
}

static void clearAStarStatePQ(AStarStatePQ& pq) {
    delete[] pq.heap;
    pq.heap = nullptr;
    pq.size = 0;
    pq.capacity = 0;
}

static void heapifyUpAStarState(AStarStatePQ& pq, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (pq.heap[idx].fCost < pq.heap[parent].fCost) {
            AStarState temp = pq.heap[idx];
            pq.heap[idx] = pq.heap[parent];
            pq.heap[parent] = temp;
            idx = parent;
        } else {
            break;
        }
    }
}

static void heapifyDownAStarState(AStarStatePQ& pq, int idx) {
    while (true) {
        int smallest = idx;
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;

        if (left < pq.size && pq.heap[left].fCost < pq.heap[smallest].fCost) {
            smallest = left;
        }
        if (right < pq.size && pq.heap[right].fCost < pq.heap[smallest].fCost) {
            smallest = right;
        }

        if (smallest != idx) {
            AStarState temp = pq.heap[idx];
            pq.heap[idx] = pq.heap[smallest];
            pq.heap[smallest] = temp;
            idx = smallest;
        } else {
            break;
        }
    }
}

static void pushAStarState(AStarStatePQ& pq, const AStarState& state) {
    if (pq.size >= pq.capacity) {

        int newCap = pq.capacity * 2;
        AStarState* newHeap = new AStarState[newCap];
        for (int i = 0; i < pq.size; i++) {
            newHeap[i] = pq.heap[i];
        }
        delete[] pq.heap;
        pq.heap = newHeap;
        pq.capacity = newCap;
    }
    pq.heap[pq.size] = state;
    heapifyUpAStarState(pq, pq.size);
    pq.size++;
}

static bool popAStarState(AStarStatePQ& pq, AStarState& out) {
    if (pq.size == 0) return false;
    out = pq.heap[0];
    pq.size--;
    if (pq.size > 0) {
        pq.heap[0] = pq.heap[pq.size];
        heapifyDownAStarState(pq, 0);
    }
    return true;
}

// A* pathfinding algorithm with date-aware layover validation and preference filtering
void findRouteAStar(Graph& g, const string& originPort, const string& destPort, AStarResult& result, const RoutePreferences* prefs) {

    result.found = false;
    result.totalCost = 0;
    result.nodesExpanded = 0;
    clearJourney(result.journey);
    initJourney(result.journey);

    int portCount;
    Port** portArray = buildAStarPortArray(g, portCount);
    if (portArray == nullptr || portCount == 0) {
        return;
    }

    int originIdx = findAStarPortIndex(portArray, portCount, originPort);
    int destIdx = findAStarPortIndex(portArray, portCount, destPort);

    if (originIdx == -1 || destIdx == -1) {
        delete[] portArray;
        return;
    }

    int* bestCost = new int[portCount];
    for (int i = 0; i < portCount; i++) {
        bestCost[i] = INT_MAX;
    }

    const int MAX_STATES = 10000;
    AStarState* allStates = new AStarState[MAX_STATES];
    int stateCount = 0;

    result.exploredEdgeCount = 0;

    AStarStatePQ openSet;
    initAStarStatePQ(openSet, portCount * 50);

    float hStart = calculateHeuristic(originPort, destPort);

    AStarState startState;
    startState.portIndex = originIdx;
    startState.gCost = 0;
    startState.hCost = hStart;
    startState.fCost = hStart;
    startState.arrivalDate = {1, 1, 2000};
    startState.arrivalTime = {0, 0};
    startState.parentStateIdx = -1;
    startState.routeUsed = nullptr;

    pushAStarState(openSet, startState);
    bestCost[originIdx] = 0;

    int destStateIdx = -1;

    while (openSet.size > 0) {
        AStarState current;
        if (!popAStarState(openSet, current)) break;

        if (current.gCost > bestCost[current.portIndex] && current.portIndex != originIdx) {
            continue;
        }

        int currentStateIdx = stateCount;
        if (stateCount < MAX_STATES) {
            allStates[stateCount++] = current;
        } else {
            continue;
        }

        if (current.portIndex == destIdx) {
            result.found = true;
            result.totalCost = current.gCost;
            destStateIdx = currentStateIdx;
            break;
        }

        result.nodesExpanded++;

        Port* currentPort = portArray[current.portIndex];
        Route* route = currentPort->routeHead;

        while (route != nullptr) {
            int neighborIdx = findAStarPortIndex(portArray, portCount, route->destinationPort);

            if (neighborIdx != -1) {

                if (prefs && (prefs->allowedCompaniesCount > 0 || prefs->forbiddenPortsCount > 0)) {
                    if (!isCompanyAllowed(*prefs, route->shippingCompany)) {
                        route = route->next;
                        continue;
                    }

                    if (isPortForbidden(*prefs, currentPort->name) || 
                        isPortForbidden(*prefs, route->destinationPort)) {
                        route = route->next;
                        continue;
                    }
                }

                bool validConnection = astarIsValidConnection(
                    current.arrivalDate, current.arrivalTime, route, 60);

                if (validConnection) {
                    int newGCost = current.gCost + route->voyageCost;

                    if (result.exploredEdgeCount < 500) {
                        result.exploredEdges[result.exploredEdgeCount].fromPort = currentPort->name;
                        result.exploredEdges[result.exploredEdgeCount].toPort = route->destinationPort;
                        result.exploredEdgeCount++;
                    }

                    if (newGCost < bestCost[neighborIdx]) {
                        bestCost[neighborIdx] = newGCost;

                        Date arrDate = route->voyageDate;
                        Time arrTime = route->arrivalTime;

                        if (astarTimeToMinutes(arrTime) < astarTimeToMinutes(route->departureTime)) {
                            arrDate.day++;
                            if (arrDate.day > 28) {
                                arrDate.day = 1;
                                arrDate.month++;
                                if (arrDate.month > 12) {
                                    arrDate.month = 1;
                                    arrDate.year++;
                                }
                            }
                        }

                        float h = calculateHeuristic(route->destinationPort, destPort);

                        AStarState newState;
                        newState.portIndex = neighborIdx;
                        newState.gCost = newGCost;
                        newState.hCost = h;
                        newState.fCost = newGCost + h;
                        newState.arrivalDate = arrDate;
                        newState.arrivalTime = arrTime;
                        newState.parentStateIdx = currentStateIdx;
                        newState.routeUsed = route;

                        pushAStarState(openSet, newState);
                    }
                }
            }
            route = route->next;
        }
    }

    if (result.found && destStateIdx >= 0) {

        Route* pathRoutes[20];
        int pathLen = 0;

        int stateIdx = destStateIdx;
        while (stateIdx >= 0 && allStates[stateIdx].routeUsed != nullptr && pathLen < 20) {
            pathRoutes[pathLen] = allStates[stateIdx].routeUsed;
            pathLen++;
            stateIdx = allStates[stateIdx].parentStateIdx;
        }

        for (int i = pathLen - 1; i >= 0; i--) {
            Route* r = pathRoutes[i];
            string fromPort;
            if (i == pathLen - 1) {
                fromPort = originPort;
            } else {
                fromPort = pathRoutes[i + 1]->destinationPort;
            }

            appendLeg(result.journey, fromPort, r->destinationPort, r->voyageDate, r->departureTime, r->arrivalTime, r->voyageCost, r->shippingCompany);
        }
    }

    clearAStarStatePQ(openSet);
    delete[] portArray;
    delete[] bestCost;
    delete[] allStates;
}

string compareAStarVsDijkstra(Graph& g, const string& originPort, const string& destPort) {

    AStarResult astarResult;
    findRouteAStar(g, originPort, destPort, astarResult);

    ShortestPathResult dijkstraResult;
    findCheapestRoute(g, originPort, destPort, dijkstraResult);

    string comparison = "=== A* vs Dijkstra Comparison ===\n";
    comparison += "Route: " + originPort + " -> " + destPort + "\n\n";

    comparison += "A* Algorithm:\n";
    if (astarResult.found) {
        comparison += "  - Path found: YES\n";
        comparison += "  - Total cost: $" + to_string(astarResult.totalCost) + "\n";
        comparison += "  - Nodes expanded: " + to_string(astarResult.nodesExpanded) + "\n";
        comparison += "  - Legs: " + to_string(astarResult.journey.legCount) + "\n";
    } else {
        comparison += "  - Path found: NO\n";
    }

    comparison += "\nDijkstra Algorithm:\n";
    if (dijkstraResult.found) {
        comparison += "  - Path found: YES\n";
        comparison += "  - Total cost: $" + to_string(dijkstraResult.totalCost) + "\n";
        comparison += "  - Nodes expanded: " + to_string(dijkstraResult.nodesExpanded) + "\n";
        comparison += "  - Legs: " + to_string(dijkstraResult.journey.legCount) + "\n";
    } else {
        comparison += "  - Path found: NO\n";
    }

    if (astarResult.found && dijkstraResult.found) {
        if (astarResult.totalCost == dijkstraResult.totalCost) {
            comparison += "\nBoth algorithms found the SAME optimal cost!\n";
        } else {
            comparison += "\nCost difference detected!\n";
        }
    }

    return comparison;
}

static int calculateAStarRouteTravelTime(const Route* route) {
    int depMinutes = route->departureTime.hour * 60 + route->departureTime.minute;
    int arrMinutes = route->arrivalTime.hour * 60 + route->arrivalTime.minute;

    if (arrMinutes >= depMinutes) {
        return arrMinutes - depMinutes;
    } else {
        return (24 * 60 - depMinutes) + arrMinutes;
    }
}

struct AStarTimeState {
    int portIndex;
    int gTime;
    float hCost;
    float fCost;
    Date arrivalDate;
    Time arrivalTime;
    int parentStateIdx;
    Route* routeUsed;

    AStarTimeState() : portIndex(-1), gTime(INT_MAX), hCost(0.0f), fCost(INT_MAX), arrivalDate{0,0,0}, arrivalTime{0,0}, parentStateIdx(-1), routeUsed(nullptr) {}
};

struct AStarTimeStatePQ {
    AStarTimeState* heap;
    int capacity;
    int size;
};

static void initAStarTimeStatePQ(AStarTimeStatePQ& pq, int cap) {
    pq.capacity = cap;
    pq.size = 0;
    pq.heap = new AStarTimeState[cap];
}

static void clearAStarTimeStatePQ(AStarTimeStatePQ& pq) {
    delete[] pq.heap;
    pq.heap = nullptr;
    pq.size = 0;
    pq.capacity = 0;
}

static void heapifyUpAStarTimeState(AStarTimeStatePQ& pq, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (pq.heap[idx].fCost < pq.heap[parent].fCost) {
            AStarTimeState temp = pq.heap[idx];
            pq.heap[idx] = pq.heap[parent];
            pq.heap[parent] = temp;
            idx = parent;
        } else {
            break;
        }
    }
}

static void heapifyDownAStarTimeState(AStarTimeStatePQ& pq, int idx) {
    while (true) {
        int smallest = idx;
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;

        if (left < pq.size && pq.heap[left].fCost < pq.heap[smallest].fCost) {
            smallest = left;
        }
        if (right < pq.size && pq.heap[right].fCost < pq.heap[smallest].fCost) {
            smallest = right;
        }

        if (smallest != idx) {
            AStarTimeState temp = pq.heap[idx];
            pq.heap[idx] = pq.heap[smallest];
            pq.heap[smallest] = temp;
            idx = smallest;
        } else {
            break;
        }
    }
}

static void pushAStarTimeState(AStarTimeStatePQ& pq, const AStarTimeState& state) {
    if (pq.size >= pq.capacity) {
        int newCap = pq.capacity * 2;
        AStarTimeState* newHeap = new AStarTimeState[newCap];
        for (int i = 0; i < pq.size; i++) {
            newHeap[i] = pq.heap[i];
        }
        delete[] pq.heap;
        pq.heap = newHeap;
        pq.capacity = newCap;
    }
    pq.heap[pq.size] = state;
    heapifyUpAStarTimeState(pq, pq.size);
    pq.size++;
}

static bool popAStarTimeState(AStarTimeStatePQ& pq, AStarTimeState& out) {
    if (pq.size == 0) return false;
    out = pq.heap[0];
    pq.size--;
    if (pq.size > 0) {
        pq.heap[0] = pq.heap[pq.size];
        heapifyDownAStarTimeState(pq, 0);
    }
    return true;
}

void findFastestRouteAStarIgnoringDates(Graph& g, const string& originPort, const string& destPort, AStarResult& result, int maxLegs, const RoutePreferences* prefs) {

    result.found = false;
    result.totalCost = 0;
    result.nodesExpanded = 0;
    clearJourney(result.journey);
    initJourney(result.journey);

    int portCount;
    Port** portArray = buildAStarPortArray(g, portCount);
    if (portArray == nullptr || portCount == 0) {
        return;
    }

    int originIdx = findAStarPortIndex(portArray, portCount, originPort);
    int destIdx = findAStarPortIndex(portArray, portCount, destPort);

    if (originIdx == -1 || destIdx == -1) {
        delete[] portArray;
        return;
    }

    int* bestTime = new int[portCount];
    for (int i = 0; i < portCount; i++) {
        bestTime[i] = INT_MAX;
    }

    const int MAX_STATES = 10000;
    AStarTimeState* allStates = new AStarTimeState[MAX_STATES];
    int stateCount = 0;

    result.exploredEdgeCount = 0;

    AStarTimeStatePQ openSet;
    initAStarTimeStatePQ(openSet, portCount * 50);

    float hStart = calculateTimeHeuristic(originPort, destPort);
    
    cout << "A* Time Heuristic from " << originPort << " to " << destPort 
         << ": " << hStart << " minutes (" << (hStart/60.0f) << " hours)" << endl;

    AStarTimeState startState;
    startState.portIndex = originIdx;
    startState.gTime = 0;
    startState.hCost = hStart;
    startState.fCost = hStart;
    startState.arrivalDate = {1, 1, 2000};
    startState.arrivalTime = {0, 0};
    startState.parentStateIdx = -1;
    startState.routeUsed = nullptr;

    pushAStarTimeState(openSet, startState);
    bestTime[originIdx] = 0;

    int destStateIdx = -1;
    int totalCost = 0;

    while (openSet.size > 0) {
        AStarTimeState current;
        if (!popAStarTimeState(openSet, current)) break;

        if (current.gTime > bestTime[current.portIndex] && current.portIndex != originIdx) {
            continue;
        }

        int currentStateIdx = stateCount;
        if (stateCount < MAX_STATES) {
            allStates[stateCount++] = current;
        } else {
            continue;
        }

        if (current.portIndex == destIdx) {
            result.found = true;

            int idx = currentStateIdx;
            totalCost = 0;
            int totalTime = 0;
            while (idx >= 0 && allStates[idx].routeUsed != nullptr) {
                totalCost += allStates[idx].routeUsed->voyageCost;
                totalTime += calculateAStarRouteTravelTime(allStates[idx].routeUsed);
                idx = allStates[idx].parentStateIdx;
            }
            result.totalCost = totalCost;
            
            cout << "A* Time found route: Total time = " << current.gTime << " minutes (" 
                 << (current.gTime / 60) << "h " << (current.gTime % 60) << "m), Cost = $" 
                 << totalCost << endl;
            
            destStateIdx = currentStateIdx;
            break;
        }

        result.nodesExpanded++;

        Port* currentPort = portArray[current.portIndex];
        Route* route = currentPort->routeHead;

        while (route != nullptr) {
            int neighborIdx = findAStarPortIndex(portArray, portCount, route->destinationPort);

            if (neighborIdx != -1) {

                if (prefs && (prefs->allowedCompaniesCount > 0 || prefs->forbiddenPortsCount > 0)) {
                    if (!isCompanyAllowed(*prefs, route->shippingCompany)) {
                        route = route->next;
                        continue;
                    }

                    if (isPortForbidden(*prefs, currentPort->name) || 
                        isPortForbidden(*prefs, route->destinationPort)) {
                        route = route->next;
                        continue;
                    }
                }

                bool validConnection = astarIsValidConnection(
                    current.arrivalDate, current.arrivalTime, route, 60);

                if (validConnection) {
                    int travelTime = calculateAStarRouteTravelTime(route);
                    int newGTime = current.gTime + travelTime;

                    if (result.exploredEdgeCount < 500) {
                        result.exploredEdges[result.exploredEdgeCount].fromPort = currentPort->name;
                        result.exploredEdges[result.exploredEdgeCount].toPort = route->destinationPort;
                        result.exploredEdgeCount++;
                    }

                    if (newGTime < bestTime[neighborIdx]) {
                        bestTime[neighborIdx] = newGTime;

                        Date arrDate = route->voyageDate;
                        Time arrTime = route->arrivalTime;

                        if (astarTimeToMinutes(arrTime) < astarTimeToMinutes(route->departureTime)) {
                            arrDate.day++;
                            if (arrDate.day > 28) {
                                arrDate.day = 1;
                                arrDate.month++;
                                if (arrDate.month > 12) {
                                    arrDate.month = 1;
                                    arrDate.year++;
                                }
                            }
                        }

                        float h = calculateTimeHeuristic(route->destinationPort, destPort);

                        AStarTimeState newState;
                        newState.portIndex = neighborIdx;
                        newState.gTime = newGTime;
                        newState.hCost = h;
                        newState.fCost = newGTime + h;
                        newState.arrivalDate = arrDate;
                        newState.arrivalTime = arrTime;
                        newState.parentStateIdx = currentStateIdx;
                        newState.routeUsed = route;

                        pushAStarTimeState(openSet, newState);
                    }
                }
            }
            route = route->next;
        }
    }

    if (result.found && destStateIdx >= 0) {

        Route* pathRoutes[20];
        int pathLen = 0;

        int stateIdx = destStateIdx;
        while (stateIdx >= 0 && allStates[stateIdx].routeUsed != nullptr && pathLen < 20) {
            pathRoutes[pathLen] = allStates[stateIdx].routeUsed;
            pathLen++;
            stateIdx = allStates[stateIdx].parentStateIdx;
        }

        for (int i = pathLen - 1; i >= 0; i--) {
            Route* r = pathRoutes[i];
            string fromPort;
            if (i == pathLen - 1) {
                fromPort = originPort;
            } else {
                fromPort = pathRoutes[i + 1]->destinationPort;
            }

            appendLeg(result.journey, fromPort, r->destinationPort, r->voyageDate, r->departureTime, r->arrivalTime, r->voyageCost, r->shippingCompany);
        }
    }

    clearAStarTimeStatePQ(openSet);
    delete[] portArray;
    delete[] bestTime;
    delete[] allStates;
}
