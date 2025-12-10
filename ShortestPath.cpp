

#include "ShortestPath.h"
#include <limits.h>
#include <iostream>

using namespace std;

// Filters routes during search based on company/port preferences
static bool routeMatchesPreferences(const Route* route, const string& currentPort, const RoutePreferences* prefs) {
    if (!prefs || !route) return true;
    
    if (!isCompanyAllowed(*prefs, route->shippingCompany)) {
        return false;
    }
    
    if (isPortForbidden(*prefs, currentPort) || isPortForbidden(*prefs, route->destinationPort)) {
        return false;
    }
    
    return true;
}

static int dateToAbsoluteDays(const Date& d) {
    return d.year * 365 + d.month * 31 + d.day;
}

static int timeToMinutes(const Time& t) {
    return t.hour * 60 + t.minute;
}

static bool isValidConnection(const Date& arrivalDate, const Time& arrivalTime, const Route* route, int minLayoverMinutes = 60) {
    int arrDays = dateToAbsoluteDays(arrivalDate);
    int depDays = dateToAbsoluteDays(route->voyageDate);

    if (depDays > arrDays) {

        return true;
    } else if (depDays == arrDays) {

        int arrMins = timeToMinutes(arrivalTime);
        int depMins = timeToMinutes(route->departureTime);
        return (depMins >= arrMins + minLayoverMinutes);
    }

    return false;
}

static Port** buildPortArray(Graph& g, int& portCount) {
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

static int findPortIndex(Port** portArray, int portCount, const string& portName) {
    for (int i = 0; i < portCount; i++) {
        if (portArray[i]->name == portName) {
            return i;
        }
    }
    return -1;
}

struct DijkstraState {
    int portIndex;
    int cost;
    Date arrivalDate;
    Time arrivalTime;
    int parentStateIdx;
    Route* routeUsed;
};

struct StatePQ {
    DijkstraState* heap;
    int capacity;
    int size;
};

static void initStatePQ(StatePQ& pq, int cap) {
    pq.capacity = cap;
    pq.size = 0;
    pq.heap = new DijkstraState[cap];
}

static void clearStatePQ(StatePQ& pq) {
    delete[] pq.heap;
    pq.heap = nullptr;
    pq.size = 0;
}

static void heapifyUpState(StatePQ& pq, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (pq.heap[idx].cost < pq.heap[parent].cost) {
            DijkstraState temp = pq.heap[idx];
            pq.heap[idx] = pq.heap[parent];
            pq.heap[parent] = temp;
            idx = parent;
        } else {
            break;
        }
    }
}

static void heapifyDownState(StatePQ& pq, int idx) {
    while (true) {
        int smallest = idx;
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;

        if (left < pq.size && pq.heap[left].cost < pq.heap[smallest].cost) {
            smallest = left;
        }
        if (right < pq.size && pq.heap[right].cost < pq.heap[smallest].cost) {
            smallest = right;
        }

        if (smallest != idx) {
            DijkstraState temp = pq.heap[idx];
            pq.heap[idx] = pq.heap[smallest];
            pq.heap[smallest] = temp;
            idx = smallest;
        } else {
            break;
        }
    }
}

static void pushState(StatePQ& pq, const DijkstraState& state) {
    if (pq.size >= pq.capacity) {

        int newCap = pq.capacity * 2;
        DijkstraState* newHeap = new DijkstraState[newCap];
        for (int i = 0; i < pq.size; i++) {
            newHeap[i] = pq.heap[i];
        }
        delete[] pq.heap;
        pq.heap = newHeap;
        pq.capacity = newCap;
    }
    pq.heap[pq.size] = state;
    heapifyUpState(pq, pq.size);
    pq.size++;
}

static bool popState(StatePQ& pq, DijkstraState& out) {
    if (pq.size == 0) return false;
    out = pq.heap[0];
    pq.size--;
    if (pq.size > 0) {
        pq.heap[0] = pq.heap[pq.size];
        heapifyDownState(pq, 0);
    }
    return true;
}

void findCheapestRoute(Graph& g, const string& originPort, const string& destPort, ShortestPathResult& result) {

    result.found = false;
    result.totalCost = 0;
    result.nodesExpanded = 0;
    clearJourney(result.journey);
    initJourney(result.journey);

    int portCount;
    Port** portArray = buildPortArray(g, portCount);
    if (portArray == nullptr || portCount == 0) {
        return;
    }

    int originIdx = findPortIndex(portArray, portCount, originPort);
    int destIdx = findPortIndex(portArray, portCount, destPort);

    if (originIdx == -1 || destIdx == -1) {
        delete[] portArray;
        return;
    }

    int* bestCost = new int[portCount];
    for (int i = 0; i < portCount; i++) {
        bestCost[i] = INT_MAX;
    }

    const int MAX_STATES = 10000;
    DijkstraState* allStates = new DijkstraState[MAX_STATES];
    int stateCount = 0;

    result.exploredEdgeCount = 0;

    StatePQ pq;
    initStatePQ(pq, portCount * 50);

    DijkstraState startState;
    startState.portIndex = originIdx;
    startState.cost = 0;
    startState.arrivalDate = {1, 1, 2000};
    startState.arrivalTime = {0, 0};
    startState.parentStateIdx = -1;
    startState.routeUsed = nullptr;

    pushState(pq, startState);
    bestCost[originIdx] = 0;

    int destStateIdx = -1;

    while (pq.size > 0) {
        DijkstraState current;
        if (!popState(pq, current)) break;

        if (current.cost > bestCost[current.portIndex] && current.portIndex != originIdx) {
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
            result.totalCost = current.cost;
            destStateIdx = currentStateIdx;
            break;
        }

        result.nodesExpanded++;

        Port* currentPort = portArray[current.portIndex];
        Route* route = currentPort->routeHead;

        while (route != nullptr) {
            int neighborIdx = findPortIndex(portArray, portCount, route->destinationPort);

            if (neighborIdx != -1) {

                bool validConnection = isValidConnection(
                    current.arrivalDate, current.arrivalTime, route, 60);

                if (validConnection) {
                    int newCost = current.cost + route->voyageCost;

                    if (result.exploredEdgeCount < 500) {
                        result.exploredEdges[result.exploredEdgeCount].fromPort = currentPort->name;
                        result.exploredEdges[result.exploredEdgeCount].toPort = route->destinationPort;
                        result.exploredEdgeCount++;
                    }

                    if (newCost < bestCost[neighborIdx]) {
                        bestCost[neighborIdx] = newCost;

                        Date arrDate = route->voyageDate;
                        Time arrTime = route->arrivalTime;

                        if (timeToMinutes(arrTime) < timeToMinutes(route->departureTime)) {
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

                        DijkstraState newState;
                        newState.portIndex = neighborIdx;
                        newState.cost = newCost;
                        newState.arrivalDate = arrDate;
                        newState.arrivalTime = arrTime;
                        newState.parentStateIdx = currentStateIdx;
                        newState.routeUsed = route;

                        pushState(pq, newState);
                    }
                }
            }
            route = route->next;
        }
    }

    if (result.found && destStateIdx >= 0) {

        Route* pathRoutes[20];
        int parentIndices[20];
        int pathLen = 0;

        int stateIdx = destStateIdx;
        while (stateIdx >= 0 && allStates[stateIdx].routeUsed != nullptr && pathLen < 20) {
            pathRoutes[pathLen] = allStates[stateIdx].routeUsed;
            parentIndices[pathLen] = allStates[stateIdx].parentStateIdx;
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

            appendLeg(result.journey,
                fromPort,
                r->destinationPort,
                r->voyageDate,
                r->departureTime,
                r->arrivalTime,
                r->voyageCost,
                r->shippingCompany);
        }
    }

    clearStatePQ(pq);
    delete[] portArray;
    delete[] bestCost;
    delete[] allStates;
}

#include "ShortestPath.h"
#include <limits.h>
#include <iostream>

using namespace std;

struct SimpleState {
    int portIndex;
    int costOrTime;
    int legCount;
    Date arrivalDate;
    Time arrivalTime;
    int parentStateIdx;
    Route* routeUsed;
};

struct SimpleStatePQ {
    SimpleState* heap;
    int capacity;
    int size;
};

static void initSimpleStatePQ(SimpleStatePQ& pq, int cap) {
    pq.capacity = cap;
    pq.size = 0;
    pq.heap = new SimpleState[cap];
}

static void clearSimpleStatePQ(SimpleStatePQ& pq) {
    delete[] pq.heap;
    pq.heap = nullptr;
    pq.size = 0;
}

static void heapifyUpSimple(SimpleStatePQ& pq, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (pq.heap[idx].costOrTime < pq.heap[parent].costOrTime) {
            SimpleState temp = pq.heap[idx];
            pq.heap[idx] = pq.heap[parent];
            pq.heap[parent] = temp;
            idx = parent;
        } else {
            break;
        }
    }
}

static void heapifyDownSimple(SimpleStatePQ& pq, int idx) {
    while (true) {
        int smallest = idx;
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;

        if (left < pq.size && pq.heap[left].costOrTime < pq.heap[smallest].costOrTime) {
            smallest = left;
        }
        if (right < pq.size && pq.heap[right].costOrTime < pq.heap[smallest].costOrTime) {
            smallest = right;
        }

        if (smallest != idx) {
            SimpleState temp = pq.heap[idx];
            pq.heap[idx] = pq.heap[smallest];
            pq.heap[smallest] = temp;
            idx = smallest;
        } else {
            break;
        }
    }
}

static void pushSimpleState(SimpleStatePQ& pq, const SimpleState& state) {
    if (pq.size >= pq.capacity) {

        int newCap = pq.capacity * 2;
        SimpleState* newHeap = new SimpleState[newCap];
        for (int i = 0; i < pq.size; i++) {
            newHeap[i] = pq.heap[i];
        }
        delete[] pq.heap;
        pq.heap = newHeap;
        pq.capacity = newCap;
    }
    pq.heap[pq.size] = state;
    heapifyUpSimple(pq, pq.size);
    pq.size++;
}

static bool popSimpleState(SimpleStatePQ& pq, SimpleState& out) {
    if (pq.size == 0) return false;
    out = pq.heap[0];
    pq.heap[0] = pq.heap[pq.size - 1];
    pq.size--;
    if (pq.size > 0) {
        heapifyDownSimple(pq, 0);
    }
    return true;
}

static int calculateRouteTravelTime(const Route* route) {
    int depMinutes = route->departureTime.hour * 60 + route->departureTime.minute;
    int arrMinutes = route->arrivalTime.hour * 60 + route->arrivalTime.minute;

    if (arrMinutes >= depMinutes) {
        return arrMinutes - depMinutes;
    } else {

        return (24 * 60 - depMinutes) + arrMinutes;
    }
}

static int dateToAbsoluteDaysSimple(const Date& d) {
    return d.year * 365 + d.month * 31 + d.day;
}

static int timeToMinutesSimple(const Time& t) {
    return t.hour * 60 + t.minute;
}

// Validates minimum layover time between connecting routes
static bool isValidLayoverConnection(const Date& arrivalDate, const Time& arrivalTime, const Route* route, int minLayoverMinutes = 60) {
    int arrDays = dateToAbsoluteDaysSimple(arrivalDate);
    int depDays = dateToAbsoluteDaysSimple(route->voyageDate);

    if (depDays > arrDays) {

        return true;
    } else if (depDays == arrDays) {

        int arrMins = timeToMinutesSimple(arrivalTime);
        int depMins = timeToMinutesSimple(route->departureTime);
        return (depMins >= arrMins + minLayoverMinutes);
    }

    return false;
}

// Dijkstra's algorithm finding minimum cost path with preference filtering
void findCheapestRouteIgnoringDates(Graph& g, const string& originPort, const string& destPort, ShortestPathResult& result, int maxLegs, const RoutePreferences* prefs) {

    result.found = false;
    result.totalCost = 0;
    result.nodesExpanded = 0;
    result.exploredEdgeCount = 0;
    clearJourney(result.journey);

    int portCount;
    Port** portArray = buildPortArray(g, portCount);
    if (!portArray) return;

    int originIdx = findPortIndex(portArray, portCount, originPort);
    int destIdx = findPortIndex(portArray, portCount, destPort);

    if (originIdx < 0 || destIdx < 0) {
        delete[] portArray;
        return;
    }

    int* bestCost = new int[portCount];
    for (int i = 0; i < portCount; i++) {
        bestCost[i] = INT_MAX;
    }

    const int MAX_STATES = 10000;
    SimpleState* allStates = new SimpleState[MAX_STATES];
    int stateCount = 0;

    SimpleStatePQ pq;
    initSimpleStatePQ(pq, portCount * 50);

    SimpleState startState;
    startState.portIndex = originIdx;
    startState.costOrTime = 0;
    startState.legCount = 0;
    startState.arrivalDate = {1, 1, 2000};
    startState.arrivalTime = {0, 0};
    startState.parentStateIdx = -1;
    startState.routeUsed = nullptr;

    pushSimpleState(pq, startState);
    bestCost[originIdx] = 0;

    int destStateIdx = -1;

    while (pq.size > 0) {
        SimpleState current;
        if (!popSimpleState(pq, current)) break;

        int currentStateIdx = stateCount;
        if (stateCount < MAX_STATES) {
            allStates[stateCount++] = current;
        } else {
            continue;
        }

        if (current.portIndex == destIdx) {
            result.found = true;
            result.totalCost = current.costOrTime;
            destStateIdx = currentStateIdx;
            break;
        }

        if (current.costOrTime > bestCost[current.portIndex]) {
            continue;
        }

        result.nodesExpanded++;

        Port* currentPort = portArray[current.portIndex];
        Route* route = currentPort->routeHead;

        while (route != nullptr) {
            if (!routeMatchesPreferences(route, currentPort->name, prefs)) {
                route = route->next;
                continue;
            }
            
            int neighborIdx = findPortIndex(portArray, portCount, route->destinationPort);

            if (neighborIdx != -1) {

                int newLegCount = current.legCount + 1;
                if (newLegCount > maxLegs) {
                    route = route->next;
                    continue;
                }

                bool validConnection = isValidLayoverConnection(
                    current.arrivalDate, current.arrivalTime, route, 60);

                if (!validConnection) {
                    route = route->next;
                    continue;
                }

                int newCost = current.costOrTime + route->voyageCost;

                if (result.exploredEdgeCount < 500) {
                    result.exploredEdges[result.exploredEdgeCount].fromPort = currentPort->name;
                    result.exploredEdges[result.exploredEdgeCount].toPort = route->destinationPort;
                    result.exploredEdgeCount++;
                }

                Date arrDate = route->voyageDate;
                Time arrTime = route->arrivalTime;

                if (timeToMinutesSimple(arrTime) < timeToMinutesSimple(route->departureTime)) {
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

                if (newCost <= bestCost[neighborIdx] || bestCost[neighborIdx] == INT_MAX) {

                    if (newCost < bestCost[neighborIdx]) {
                        bestCost[neighborIdx] = newCost;
                    }

                    SimpleState newState;
                    newState.portIndex = neighborIdx;
                    newState.costOrTime = newCost;
                    newState.legCount = newLegCount;
                    newState.arrivalDate = arrDate;
                    newState.arrivalTime = arrTime;
                    newState.parentStateIdx = currentStateIdx;
                    newState.routeUsed = route;

                    pushSimpleState(pq, newState);
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

            appendLeg(result.journey,
                fromPort,
                r->destinationPort,
                r->voyageDate,
                r->departureTime,
                r->arrivalTime,
                r->voyageCost,
                r->shippingCompany);
        }
    }

    clearSimpleStatePQ(pq);
    delete[] portArray;
    delete[] bestCost;
    delete[] allStates;
}

// Dijkstra's algorithm finding minimum time path with preference filtering
void findFastestRouteIgnoringDates(Graph& g, const string& originPort, const string& destPort, ShortestPathResult& result, int maxLegs, const RoutePreferences* prefs) {

    result.found = false;
    result.totalCost = 0;
    result.nodesExpanded = 0;
    result.exploredEdgeCount = 0;
    clearJourney(result.journey);

    int portCount;
    Port** portArray = buildPortArray(g, portCount);
    if (!portArray) return;

    int originIdx = findPortIndex(portArray, portCount, originPort);
    int destIdx = findPortIndex(portArray, portCount, destPort);

    if (originIdx < 0 || destIdx < 0) {
        delete[] portArray;
        return;
    }

    int* bestTime = new int[portCount];
    for (int i = 0; i < portCount; i++) {
        bestTime[i] = INT_MAX;
    }

    const int MAX_STATES = 10000;
    SimpleState* allStates = new SimpleState[MAX_STATES];
    int stateCount = 0;

    SimpleStatePQ pq;
    initSimpleStatePQ(pq, portCount * 50);

    SimpleState startState;
    startState.portIndex = originIdx;
    startState.costOrTime = 0;
    startState.legCount = 0;
    startState.arrivalDate = {1, 1, 2000};
    startState.arrivalTime = {0, 0};
    startState.parentStateIdx = -1;
    startState.routeUsed = nullptr;

    pushSimpleState(pq, startState);
    bestTime[originIdx] = 0;

    int destStateIdx = -1;
    int totalCost = 0;

    while (pq.size > 0) {
        SimpleState current;
        if (!popSimpleState(pq, current)) break;

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
            while (idx >= 0 && allStates[idx].routeUsed != nullptr) {
                totalCost += allStates[idx].routeUsed->voyageCost;
                idx = allStates[idx].parentStateIdx;
            }
            result.totalCost = totalCost;
            destStateIdx = currentStateIdx;
            break;
        }

        if (current.costOrTime > bestTime[current.portIndex]) {
            continue;
        }

        result.nodesExpanded++;

        Port* currentPort = portArray[current.portIndex];
        Route* route = currentPort->routeHead;

        while (route != nullptr) {
            if (!routeMatchesPreferences(route, currentPort->name, prefs)) {
                route = route->next;
                continue;
            }
            
            int neighborIdx = findPortIndex(portArray, portCount, route->destinationPort);

            if (neighborIdx != -1) {

                int newLegCount = current.legCount + 1;
                if (newLegCount > maxLegs) {
                    route = route->next;
                    continue;
                }

                bool validConnection = isValidLayoverConnection(
                    current.arrivalDate, current.arrivalTime, route, 60);

                if (!validConnection) {
                    route = route->next;
                    continue;
                }

                int travelTime = calculateRouteTravelTime(route);
                int newTime = current.costOrTime + travelTime;

                if (result.exploredEdgeCount < 500) {
                    result.exploredEdges[result.exploredEdgeCount].fromPort = currentPort->name;
                    result.exploredEdges[result.exploredEdgeCount].toPort = route->destinationPort;
                    result.exploredEdgeCount++;
                }

                Date arrDate = route->voyageDate;
                Time arrTime = route->arrivalTime;

                if (timeToMinutesSimple(arrTime) < timeToMinutesSimple(route->departureTime)) {
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

                if (newTime <= bestTime[neighborIdx] || bestTime[neighborIdx] == INT_MAX) {

                    if (newTime < bestTime[neighborIdx]) {
                        bestTime[neighborIdx] = newTime;
                    }

                    SimpleState newState;
                    newState.portIndex = neighborIdx;
                    newState.costOrTime = newTime;
                    newState.legCount = newLegCount;
                    newState.arrivalDate = arrDate;
                    newState.arrivalTime = arrTime;
                    newState.parentStateIdx = currentStateIdx;
                    newState.routeUsed = route;

                    pushSimpleState(pq, newState);
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

            appendLeg(result.journey,
                fromPort,
                r->destinationPort,
                r->voyageDate,
                r->departureTime,
                r->arrivalTime,
                r->voyageCost,
                r->shippingCompany);
        }
    }

    clearSimpleStatePQ(pq);
    delete[] portArray;
    delete[] bestTime;
    delete[] allStates;
}
