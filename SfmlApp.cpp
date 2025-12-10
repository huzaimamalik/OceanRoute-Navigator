#include "SfmlApp.h"
#include "ShortestPath.h"
#include "AStarSearch.h"
#include "SafestRouteSearch.h"
#include "ShipAnimator.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <fstream>

using namespace std;

static MapCalibration gMapCalib = {-0.0740f, 0.0520f, 1.0000f, 1.0000f};
static sf::FloatRect gMapBounds;
static ShipAnimator gShipAnimator;

sf::Vector2f geoToMapCoords(float lat, float lon) {

    float xNorm = (lon + 180.0f) / 360.0f;
    float yNorm = (90.0f - lat) / 180.0f;

    xNorm = (xNorm * gMapCalib.xScale) + gMapCalib.xOffsetNorm;
    yNorm = (yNorm * gMapCalib.yScale) + gMapCalib.yOffsetNorm;

    xNorm = max(0.0f, min(1.0f, xNorm));
    yNorm = max(0.0f, min(1.0f, yNorm));

    float x = gMapBounds.left + xNorm * gMapBounds.width;
    float y = gMapBounds.top + yNorm * gMapBounds.height;

    return sf::Vector2f(x, y);
}

bool getPortCoords(const string& name, float& x, float& y) {
    for (int i = 0; i < PORT_COUNT; i++) {
        if (PORT_POSITIONS[i].name == name) {
            sf::Vector2f pos = geoToMapCoords(PORT_POSITIONS[i].lat, PORT_POSITIONS[i].lon);
            x = pos.x;
            y = pos.y;
            return true;
        }
    }
    return false;
}

string buildRouteSummary(const BookedJourney& journey) {
    if (journey.legCount == 0 || journey.head == nullptr) {
        return "(no route)";
    }

    string summary = "";
    BookedLeg* leg = journey.head;
    summary += leg->originPort;

    while (leg) {
        summary += " > " + leg->destinationPort;
        leg = leg->next;
    }

    if (summary.length() > 40) {
        summary = summary.substr(0, 37) + "...";
    }

    return summary;
}

string getJourneyCompaniesInternal(const BookedJourney& journey) {
    if (journey.head == nullptr) return "N/A";

    string companies = "";
    string seen[10];
    int seenCount = 0;

    BookedLeg* leg = journey.head;
    while (leg && seenCount < 10) {
        bool found = false;
        for (int i = 0; i < seenCount; i++) {
            if (seen[i] == leg->shippingCompany) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (seenCount > 0) companies += ", ";
            companies += leg->shippingCompany;
            seen[seenCount++] = leg->shippingCompany;
        }
        leg = leg->next;
    }

    return companies;
}

void getJourneyPortSequence(const BookedJourney& journey, string ports[], int& count) {
    count = 0;
    if (journey.head == nullptr) return;

    BookedLeg* leg = journey.head;
    ports[count++] = leg->originPort;

    while (leg && count < 10) {
        ports[count++] = leg->destinationPort;
        leg = leg->next;
    }
}

int calculateJourneyTravelTime(const BookedJourney& journey) {
    if (journey.head == nullptr) return 0;

    int totalMinutes = 0;
    BookedLeg* leg = journey.head;
    int prevArrDay = 0, prevArrMonth = 0, prevArrYear = 0;
    int prevArrHour = 0, prevArrMinute = 0;
    int legIdx = 0;

    while (leg) {

        int depMinutes = leg->departureTime.hour * 60 + leg->departureTime.minute;
        int arrMinutes = leg->arrivalTime.hour * 60 + leg->arrivalTime.minute;
        int legDuration = arrMinutes - depMinutes;
        if (legDuration < 0) legDuration += 24 * 60;

        if (legIdx > 0) {

            int prevDayNum = prevArrYear * 365 + prevArrMonth * 30 + prevArrDay;
            int currDayNum = leg->voyageDate.year * 365 + leg->voyageDate.month * 30 + leg->voyageDate.day;
            int daysDiff = currDayNum - prevDayNum;

            if (daysDiff > 0) {

                int layover = daysDiff * 24 * 60 + (depMinutes - (prevArrHour * 60 + prevArrMinute));
                totalMinutes += layover;
            } else {

                totalMinutes += depMinutes - (prevArrHour * 60 + prevArrMinute);
            }
        }

        totalMinutes += legDuration;

        prevArrDay = leg->voyageDate.day;
        prevArrMonth = leg->voyageDate.month;
        prevArrYear = leg->voyageDate.year;
        prevArrHour = leg->arrivalTime.hour;
        prevArrMinute = leg->arrivalTime.minute;

        if (leg->arrivalTime.hour < leg->departureTime.hour) {
            prevArrDay++;
        }

        legIdx++;
        leg = leg->next;
    }

    return totalMinutes;
}

void getPortList(Graph& graph, string ports[], int& count) {
    count = 0;
    Port* p = graph.portHead;
    while (p && count < MAX_PORTS) {
        ports[count++] = p->name;
        p = p->next;
    }
}

// Converts UI preferences to RoutePreferences struct for search algorithms
RoutePreferences convertToRoutePreferences(const UIState& state) {
    RoutePreferences prefs;
    initRoutePreferences(prefs);
    
    if (!state.preferencesEnabled) {
        return prefs;
    }
    
    prefs.allowedCompaniesCount = state.preferredCompaniesCount;
    for (int i = 0; i < state.preferredCompaniesCount; i++) {
        prefs.allowedCompanies[i] = state.preferredCompanies[i];
    }
    
    prefs.forbiddenPortsCount = state.avoidedPortsCount;
    for (int i = 0; i < state.avoidedPortsCount; i++) {
        prefs.forbiddenPorts[i] = state.avoidedPorts[i];
    }
    
    return prefs;
}

// Validates if a complete journey satisfies all preference filters
bool journeyPassesPreferences(const UIState& state, const BookedJourney& journey) {
    if (!state.preferencesEnabled) return true;
    
    BookedLeg* leg = journey.head;
    while (leg) {
        for (int i = 0; i < state.avoidedPortsCount; i++) {
            if (leg->originPort == state.avoidedPorts[i] || 
                leg->destinationPort == state.avoidedPorts[i]) {
                return false;
            }
        }
        
        if (state.preferredCompaniesCount > 0) {
            bool found = false;
            for (int i = 0; i < state.preferredCompaniesCount; i++) {
                if (leg->shippingCompany == state.preferredCompanies[i]) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
        
        if (state.useMaxVoyageTime) {
            int depMinutes = leg->departureTime.hour * 60 + leg->departureTime.minute;
            int arrMinutes = leg->arrivalTime.hour * 60 + leg->arrivalTime.minute;
            int travelTime;
            if (arrMinutes >= depMinutes) {
                travelTime = arrMinutes - depMinutes;
            } else {
                travelTime = (24 * 60 - depMinutes) + arrMinutes;
            }
            int maxMinutes = state.maxVoyageTimeHours * 60;
            if (travelTime > maxMinutes) return false;
        }
        
        leg = leg->next;
    }
    
    return true;
}

// Executes selected pathfinding algorithm and stores results in UIState
void performSearch(Graph& graph, JourneyManager& journeyManager, UIState& state) {

    clearJourneyManager(journeyManager);
    initJourneyManager(journeyManager);
    state.hasResults = false;
    state.journeyListCount = 0;
    state.journeyPortCount = 0;
    state.animState = UIState::ANIM_IDLE;
    state.shipAnimationActive = false;
    state.lineDrawProgress = 0.0f;

    Port* origin = findPort(graph, state.originPort);
    Port* dest = findPort(graph, state.destPort);

    if (!origin) {
        state.statusMessage = "Error: Invalid origin port";
        state.isError = true;
        return;
    }
    if (!dest) {
        state.statusMessage = "Error: Invalid destination port";
        state.isError = true;
        return;
    }
    if (state.originPort == state.destPort) {
        state.statusMessage = "Error: Origin and destination must differ";
        state.isError = true;
        return;
    }

    if (state.strategy == UI_DIJKSTRA_COST || state.strategy == UI_DIJKSTRA_TIME ||
        state.strategy == UI_ASTAR_COST || state.strategy == UI_ASTAR_TIME) {

        cout << "\n========== GRAPH OPTIMIZATION SEARCH (Date-Agnostic) ==========\n";
        cout << "Strategy: " << (state.strategy == UI_DIJKSTRA_COST ? "Dijkstra (Cost)" :
                                 state.strategy == UI_DIJKSTRA_TIME ? "Dijkstra (Time)" :
                                 state.strategy == UI_ASTAR_COST ? "A* (Cost)" : "A* (Time)") << "\n";
        cout << "Mode: Pure graph shortest-path (ignoring dates)\n";
        cout << "Origin: " << state.originPort << " -> Destination: " << state.destPort << "\n";

        RoutePreferences prefs = convertToRoutePreferences(state);
        const RoutePreferences* prefsPtr = state.preferencesEnabled ? &prefs : nullptr;
        
        if (state.preferencesEnabled) {
            cout << "Preferences: ENABLED (filtering during search)\n";
            if (prefs.allowedCompaniesCount > 0) {
                cout << "  Preferred Companies: ";
                for (int i = 0; i < prefs.allowedCompaniesCount; i++) {
                    cout << prefs.allowedCompanies[i];
                    if (i < prefs.allowedCompaniesCount - 1) cout << ", ";
                }
                cout << "\n";
            }
            if (prefs.forbiddenPortsCount > 0) {
                cout << "  Avoided Ports: ";
                for (int i = 0; i < prefs.forbiddenPortsCount; i++) {
                    cout << prefs.forbiddenPorts[i];
                    if (i < prefs.forbiddenPortsCount - 1) cout << ", ";
                }
                cout << "\n";
            }
            if (state.useMaxVoyageTime) {
                cout << "  Max Voyage Time: " << state.maxVoyageTimeHours << " hours\n";
            }
        }

        ShortestPathResult result;

        if (state.strategy == UI_DIJKSTRA_COST) {

            findCheapestRouteIgnoringDates(graph, state.originPort, state.destPort, result, state.maxLegs, prefsPtr);
        }
        else if (state.strategy == UI_DIJKSTRA_TIME) {

            findFastestRouteIgnoringDates(graph, state.originPort, state.destPort, result, state.maxLegs, prefsPtr);
        }
        else if (state.strategy == UI_ASTAR_COST) {

            AStarResult astarRes;
            findRouteAStar(graph, state.originPort, state.destPort, astarRes, prefsPtr);
            result.found = astarRes.found;
            result.totalCost = astarRes.totalCost;
            result.nodesExpanded = astarRes.nodesExpanded;
            result.exploredEdgeCount = astarRes.exploredEdgeCount;
            for (int i = 0; i < astarRes.exploredEdgeCount; i++) {
                result.exploredEdges[i].fromPort = astarRes.exploredEdges[i].fromPort;
                result.exploredEdges[i].toPort = astarRes.exploredEdges[i].toPort;
            }
            result.journey = astarRes.journey;
        }
        else {

            AStarResult astarRes;
            findFastestRouteAStarIgnoringDates(graph, state.originPort, state.destPort, astarRes, state.maxLegs, prefsPtr);
            result.found = astarRes.found;
            result.totalCost = astarRes.totalCost;
            result.nodesExpanded = astarRes.nodesExpanded;
            result.exploredEdgeCount = astarRes.exploredEdgeCount;
            for (int i = 0; i < astarRes.exploredEdgeCount; i++) {
                result.exploredEdges[i].fromPort = astarRes.exploredEdges[i].fromPort;
                result.exploredEdges[i].toPort = astarRes.exploredEdges[i].toPort;
            }
            result.journey = astarRes.journey;
        }

        if (!result.found) {
            state.statusMessage = "No connecting path found (graph-wide search)";
            state.isError = true;
            cout << "Result: NO PATH FOUND in entire graph\n";
            cout << "=============================================================\n\n";
            return;
        }

        cout << "Result: OPTIMAL PATH FOUND\n";
        cout << "Total Cost: $" << result.totalCost << "\n";
        cout << "Legs: " << result.journey.legCount << "\n";
        cout << "Nodes Expanded: " << result.nodesExpanded << "\n";
        if (state.preferencesEnabled) {
            cout << "Preferences: SATISFIED (filtered during search)\n";
        }
        cout << "=============================================================\n\n";

        addJourney(journeyManager, result.journey);

        state.totalExploredEdges = min(result.exploredEdgeCount, 500);
        for (int i = 0; i < state.totalExploredEdges; i++) {
            state.exploredEdges[i].fromPort = result.exploredEdges[i].fromPort;
            state.exploredEdges[i].toPort = result.exploredEdges[i].toPort;
        }

        state.hasResults = true;
        state.journeyListCount = 1;
        state.selectedJourneyIndex = 0;

        UIState::JourneyInfo& info = state.journeyList[0];
        info.id = 1;
        info.cost = result.totalCost;
        info.legs = result.journey.legCount;
        info.route = buildRouteSummary(result.journey);
        info.valid = true;
        
        int legCount = result.journey.legCount;
        if (legCount == 1) {
            info.risk = 15 + (rand() % 10);
        } else if (legCount == 2) {
            info.risk = 30 + (rand() % 15);
        } else if (legCount == 3) {
            info.risk = 50 + (rand() % 20);
        } else {
            info.risk = 70 + (rand() % 20);
        }

        BookedLeg* leg = result.journey.head;
        int legIdx = 0;
        while (leg && legIdx < 5) {
            info.schedule[legIdx].fromPort = leg->originPort;
            info.schedule[legIdx].toPort = leg->destinationPort;
            info.schedule[legIdx].company = leg->shippingCompany;
            info.schedule[legIdx].cost = leg->voyageCost;
            info.schedule[legIdx].depDay = leg->voyageDate.day;
            info.schedule[legIdx].depMonth = leg->voyageDate.month;
            info.schedule[legIdx].depYear = leg->voyageDate.year;
            info.schedule[legIdx].depHour = leg->departureTime.hour;
            info.schedule[legIdx].depMinute = leg->departureTime.minute;
            info.schedule[legIdx].arrHour = leg->arrivalTime.hour;
            info.schedule[legIdx].arrMinute = leg->arrivalTime.minute;

            leg = leg->next;
            legIdx++;
        }

        state.journeyPortCount = 0;
        leg = result.journey.head;
        if (leg) {
            state.journeyPorts[state.journeyPortCount++] = leg->originPort;
            while (leg && state.journeyPortCount < 50) {
                state.journeyPorts[state.journeyPortCount++] = leg->destinationPort;
                leg = leg->next;
            }
        }

        if (state.strategy == UI_DIJKSTRA_COST || state.strategy == UI_DIJKSTRA_TIME) {
            state.cheapestResult.valid = true;
            state.cheapestResult.cost = result.totalCost;
            state.cheapestResult.totalCost = result.totalCost;
            state.cheapestResult.legs = result.journey.legCount;
            state.cheapestResult.totalTime = calculateJourneyTravelTime(result.journey);
            state.cheapestResult.route = buildRouteSummary(result.journey);
            state.cheapestResult.nodesExpanded = result.nodesExpanded;
        }
        else {
            state.astarResult.valid = true;
            state.astarResult.cost = result.totalCost;
            state.astarResult.totalCost = result.totalCost;
            state.astarResult.legs = result.journey.legCount;
            state.astarResult.totalTime = calculateJourneyTravelTime(result.journey);
            state.astarResult.route = buildRouteSummary(result.journey);
            state.astarResult.nodesExpanded = result.nodesExpanded;
        }

        state.statusMessage = "Optimal route found (graph-wide search)";
        state.isError = false;

        if (state.journeyPortCount > 1) {
            if ((state.strategy == UI_DIJKSTRA_COST || state.strategy == UI_DIJKSTRA_TIME ||
                 state.strategy == UI_ASTAR_COST || state.strategy == UI_ASTAR_TIME) && state.totalExploredEdges > 0) {

                state.animState = UIState::ANIM_EXPLORING;
                state.explorationAnimTime = 0.0f;
                state.explorationEdgesDrawn = 0;
            } else {

                state.animState = UIState::ANIM_DRAWING_LINE;
            }
            state.lineDrawProgress = 0.0f;
            state.shipAnimationActive = false;
            state.lineDrawProgress = 0.0f;
            state.shipCurrentLeg = 0;
            state.shipProgress = 0.0f;
            gShipAnimator.reset();  // Reset the sprite animator

            if (getPortCoords(state.journeyPorts[0], state.shipX, state.shipY)) {

            }
        } else {
            state.animState = UIState::ANIM_IDLE;
            state.shipAnimationActive = false;
            gShipAnimator.reset();
        }

        return;
    }

    cout << "\n========== DATE-CONSTRAINED SEARCH (Safest Route) ==========\n";
    cout << "Strategy: Safest Route (DFS-based)\n";
    cout << "Mode: Date-aware with layover constraints\n";
    cout << "Origin: " << state.originPort << " -> Destination: " << state.destPort << "\n";
    cout << "Travel Date: " << state.day << "/" << state.month << "/" << state.year << "\n";

    RoutePreferences prefs;
    initRoutePreferences(prefs);
    prefs.useMaxTotalCost = true;
    prefs.maxTotalCost = state.maxCost;
    prefs.useMaxLegs = true;
    prefs.maxLegs = state.maxLegs;
    prefs.minLayoverMinutes = 60;
    prefs.sameDayOnly = false;

    Date searchDate;
    searchDate.day = state.day;
    searchDate.month = state.month;
    searchDate.year = state.year;

    // Use new DFS-based safest route search - find ALL valid routes
    SafeJourneyList allJourneys;
    int maxDepth = state.maxLegs > 0 ? state.maxLegs : 15; // Use user preference or default to 15
    
    findAllSafestRoutes(graph, state.originPort, state.destPort, searchDate, prefs, allJourneys, maxDepth);

    // Convert each SafeJourney to BookedJourney and add to journey manager
    if (allJourneys.count > 0) {
        for (int i = 0; i < allJourneys.count; i++) {
            BookedJourney j = buildJourneyFromSafeJourney(state.originPort, allJourneys.journeys[i]);
            addJourney(journeyManager, j);
            clearJourney(j);
        }
        
        cout << "Added " << allJourneys.count << " safe route(s) to journey manager" << endl;
    } else {
        cout << "No safe route found within constraints" << endl;
    }
    
    // Clean up
    clearSafeJourneyList(allJourneys);

    if (journeyManager.count == 0) {
        state.statusMessage = "No safe route found within constraints";
        state.isError = true;
        return;
    }

    JourneyNode* node = journeyManager.head;
    state.journeyListCount = 0;
    state.journeyScrollOffset = 0;
    state.selectedJourneyIndex = 0;
    while (node && state.journeyListCount < 20) {
        UIState::JourneyInfo& info = state.journeyList[state.journeyListCount];
        info.id = node->id;
        info.cost = node->journey.totalCost;
        info.legs = node->journey.legCount;
        info.route = buildRouteSummary(node->journey);
        info.valid = true;

        int legCount = node->journey.legCount;
        if (legCount == 1) {
            info.risk = 15 + (rand() % 10);
        } else if (legCount == 2) {
            info.risk = 30 + (rand() % 15);
        } else if (legCount == 3) {
            info.risk = 50 + (rand() % 20);
        } else {
            info.risk = 70 + (rand() % 20);
        }

        BookedLeg* leg = node->journey.head;
        int legIdx = 0;
        int totalMinutes = 0;
        int prevArrDay = 0, prevArrMonth = 0, prevArrYear = 0;
        int prevArrHour = 0, prevArrMinute = 0;

        while (leg && legIdx < 5) {
            info.schedule[legIdx].fromPort = leg->originPort;
            info.schedule[legIdx].toPort = leg->destinationPort;
            info.schedule[legIdx].depDay = leg->voyageDate.day;
            info.schedule[legIdx].depMonth = leg->voyageDate.month;
            info.schedule[legIdx].depYear = leg->voyageDate.year;
            info.schedule[legIdx].depHour = leg->departureTime.hour;
            info.schedule[legIdx].depMinute = leg->departureTime.minute;
            info.schedule[legIdx].arrHour = leg->arrivalTime.hour;
            info.schedule[legIdx].arrMinute = leg->arrivalTime.minute;
            info.schedule[legIdx].company = leg->shippingCompany;
            info.schedule[legIdx].cost = leg->voyageCost;

            int depMinutes = leg->departureTime.hour * 60 + leg->departureTime.minute;
            int arrMinutes = leg->arrivalTime.hour * 60 + leg->arrivalTime.minute;
            int legDuration = arrMinutes - depMinutes;
            if (legDuration < 0) legDuration += 24 * 60;

            if (legIdx > 0) {

                int prevDayNum = prevArrYear * 365 + prevArrMonth * 30 + prevArrDay;
                int currDayNum = leg->voyageDate.year * 365 + leg->voyageDate.month * 30 + leg->voyageDate.day;
                int daysDiff = currDayNum - prevDayNum;

                if (daysDiff > 0) {

                    int layover = daysDiff * 24 * 60 + (depMinutes - (prevArrHour * 60 + prevArrMinute));
                    totalMinutes += layover;
                } else {

                    totalMinutes += depMinutes - (prevArrHour * 60 + prevArrMinute);
                }
            }

            totalMinutes += legDuration;

            prevArrDay = leg->voyageDate.day;
            prevArrMonth = leg->voyageDate.month;
            prevArrYear = leg->voyageDate.year;
            prevArrHour = leg->arrivalTime.hour;
            prevArrMinute = leg->arrivalTime.minute;

            if (leg->arrivalTime.hour < leg->departureTime.hour) {
                prevArrDay++;
            }

            legIdx++;
            leg = leg->next;
        }
        info.totalMinutes = totalMinutes;

        state.journeyListCount++;
        node = node->next;
    }

    ShortestPathResult dijkstraResult;
    findCheapestRoute(graph, state.originPort, state.destPort, dijkstraResult);

    state.cheapestResult.valid = dijkstraResult.found;
    if (dijkstraResult.found) {
        state.cheapestResult.cost = dijkstraResult.totalCost;
        state.cheapestResult.totalCost = dijkstraResult.totalCost;
        state.cheapestResult.legs = dijkstraResult.journey.legCount;
        state.cheapestResult.totalTime = calculateJourneyTravelTime(dijkstraResult.journey);
        state.cheapestResult.risk = 0;
        state.cheapestResult.route = buildRouteSummary(dijkstraResult.journey);
        state.cheapestResult.nodesExpanded = dijkstraResult.nodesExpanded;

        if (state.strategy == UI_DIJKSTRA_COST || state.strategy == UI_DIJKSTRA_TIME) {
            state.totalExploredEdges = min(dijkstraResult.exploredEdgeCount, 500);
            for (int i = 0; i < state.totalExploredEdges; i++) {
                state.exploredEdges[i].fromPort = dijkstraResult.exploredEdges[i].fromPort;
                state.exploredEdges[i].toPort = dijkstraResult.exploredEdges[i].toPort;
            }
        }

        clearJourney(dijkstraResult.journey);
    }

    AStarResult astarResult;
    findRouteAStar(graph, state.originPort, state.destPort, astarResult);

    state.astarResult.valid = astarResult.found;
    if (astarResult.found) {
        state.astarResult.cost = astarResult.totalCost;
        state.astarResult.totalCost = astarResult.totalCost;
        state.astarResult.legs = astarResult.journey.legCount;
        state.astarResult.totalTime = calculateJourneyTravelTime(astarResult.journey);
        state.astarResult.risk = 0;
        state.astarResult.route = buildRouteSummary(astarResult.journey);
        state.astarResult.nodesExpanded = astarResult.nodesExpanded;

        if (state.strategy == UI_ASTAR_COST || state.strategy == UI_ASTAR_TIME) {
            state.totalExploredEdges = min(astarResult.exploredEdgeCount, 500);
            for (int i = 0; i < state.totalExploredEdges; i++) {
                state.exploredEdges[i].fromPort = astarResult.exploredEdges[i].fromPort;
                state.exploredEdges[i].toPort = astarResult.exploredEdges[i].toPort;
            }
        }

        clearJourney(astarResult.journey);
    }

    state.safestResult.valid = false;

    if ((state.strategy == UI_DIJKSTRA_COST || state.strategy == UI_DIJKSTRA_TIME) && dijkstraResult.found) {

        getJourneyPortSequence(dijkstraResult.journey, state.journeyPorts, state.journeyPortCount);
    } else if ((state.strategy == UI_ASTAR_COST || state.strategy == UI_ASTAR_TIME) && astarResult.found) {

        getJourneyPortSequence(astarResult.journey, state.journeyPorts, state.journeyPortCount);
    } else if (journeyManager.head) {
        getJourneyPortSequence(journeyManager.head->journey, state.journeyPorts, state.journeyPortCount);
    }

    state.hasResults = true;
    state.isError = false;
    state.statusMessage = "Found " + to_string(journeyManager.count) + " routes";

    if (state.journeyPortCount > 1) {
        if ((state.strategy == UI_DIJKSTRA_COST || state.strategy == UI_DIJKSTRA_TIME ||
             state.strategy == UI_ASTAR_COST || state.strategy == UI_ASTAR_TIME) && state.totalExploredEdges > 0) {

            state.animState = UIState::ANIM_EXPLORING;
            state.explorationAnimTime = 0.0f;
            state.explorationEdgesDrawn = 0;
        } else {

            state.animState = UIState::ANIM_DRAWING_LINE;
        }
        state.lineDrawProgress = 0.0f;
        state.shipAnimationActive = false;
        state.lineDrawProgress = 0.0f;
        state.shipCurrentLeg = 0;
        state.shipProgress = 0.0f;
        gShipAnimator.reset();  // Reset the sprite animator

        if (getPortCoords(state.journeyPorts[0], state.shipX, state.shipY)) {

        }
    } else {
        state.animState = UIState::ANIM_IDLE;
        state.shipAnimationActive = false;
        gShipAnimator.reset();
    }
}

// Extracts all unique shipping companies from graph for preference dropdown
void collectAllCompanies(Graph& graph, UIState& state) {
    state.companyCount = 0;
    string seen[50];
    int seenCount = 0;

    Port* port = graph.portHead;
    while (port && state.companyCount < 50) {
        Route* route = port->routeHead;
        while (route && state.companyCount < 50) {

            bool alreadySeen = false;
            for (int i = 0; i < seenCount; i++) {
                if (seen[i] == route->shippingCompany) {
                    alreadySeen = true;
                    break;
                }
            }

            if (!alreadySeen && !route->shippingCompany.empty()) {
                state.companyList[state.companyCount++] = route->shippingCompany;
                seen[seenCount++] = route->shippingCompany;
            }

            route = route->next;
        }
        port = port->next;
    }
}

sf::Color hexToColor(unsigned int hex) {
    return sf::Color((hex >> 24) & 0xFF, (hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

void drawRoundedRect(sf::RenderWindow& window, float x, float y, float w, float h, unsigned int fillColor, float radius = 6.0f) {
    sf::RectangleShape rect(sf::Vector2f(w, h));
    rect.setPosition(x, y);
    rect.setFillColor(hexToColor(fillColor));
    window.draw(rect);
}

void drawRect(sf::RenderWindow& window, float x, float y, float w, float h, unsigned int color) {
    sf::RectangleShape rect(sf::Vector2f(w, h));
    rect.setPosition(x, y);
    rect.setFillColor(hexToColor(color));
    window.draw(rect);
}

void drawPanel(sf::RenderWindow& window, sf::Font& font, float x, float y, float w, float h, const string& title, unsigned int bgColor, unsigned int headerColor) {

    sf::RectangleShape outerGlow(sf::Vector2f(w + 8, h + 8));
    outerGlow.setPosition(x - 4, y - 4);
    outerGlow.setFillColor(sf::Color(0, 255, 204, 15));
    window.draw(outerGlow);

    sf::RectangleShape outerGlow2(sf::Vector2f(w + 4, h + 4));
    outerGlow2.setPosition(x - 2, y - 2);
    outerGlow2.setFillColor(sf::Color(0, 255, 204, 25));
    window.draw(outerGlow2);

    sf::RectangleShape bg1(sf::Vector2f(w, h * 0.5f));
    bg1.setPosition(x, y);
    sf::Color topColor = hexToColor(Colors::PANEL_GRADIENT_TOP);
    topColor.a = 235;
    bg1.setFillColor(topColor);
    window.draw(bg1);

    sf::RectangleShape bg2(sf::Vector2f(w, h * 0.5f));
    bg2.setPosition(x, y + h * 0.5f);
    sf::Color botColor = hexToColor(Colors::PANEL_GRADIENT_BOT);
    botColor.a = 235;
    bg2.setFillColor(botColor);
    window.draw(bg2);

    sf::RectangleShape innerOverlay(sf::Vector2f(w, h));
    innerOverlay.setPosition(x, y);
    innerOverlay.setFillColor(sf::Color(40, 60, 100, 20));
    window.draw(innerOverlay);

    if (!title.empty()) {

        sf::RectangleShape header(sf::Vector2f(w, 50));
        header.setPosition(x, y);
        sf::Color headerCol = hexToColor(headerColor);
        headerCol.a = 240;
        header.setFillColor(headerCol);
        window.draw(header);

        sf::RectangleShape accentGlow(sf::Vector2f(w, 4));
        accentGlow.setPosition(x, y + 46);
        accentGlow.setFillColor(hexToColor(0x00ffcc66));
        window.draw(accentGlow);

        sf::RectangleShape accent(sf::Vector2f(w, 3));
        accent.setPosition(x, y + 47);
        accent.setFillColor(hexToColor(Colors::HIGHLIGHT));
        window.draw(accent);

        sf::CircleShape titleIcon(6);
        titleIcon.setPosition(x + 18, y + 18);
        titleIcon.setFillColor(hexToColor(Colors::HIGHLIGHT));
        window.draw(titleIcon);

        sf::Text txt(title, font, 18);
        txt.setPosition(x + 38, y + 12);
        txt.setFillColor(hexToColor(Colors::TEXT_PRIMARY));
        txt.setStyle(sf::Text::Bold);
        window.draw(txt);
    }

    sf::RectangleShape sideAccent(sf::Vector2f(4, h));
    sideAccent.setPosition(x, y);
    sideAccent.setFillColor(hexToColor(Colors::NEON_PURPLE));
    window.draw(sideAccent);

    sf::RectangleShape border(sf::Vector2f(w, h));
    border.setPosition(x, y);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(hexToColor(0x5555aa66));
    border.setOutlineThickness(2);
    window.draw(border);
}

void drawText(sf::RenderWindow& window, sf::Font& font, const string& text, float x, float y, int size, unsigned int color, bool bold = false) {
    sf::Text txt(text, font, size);
    txt.setPosition(x, y);
    txt.setFillColor(hexToColor(color));
    if (bold) txt.setStyle(sf::Text::Bold);
    window.draw(txt);
}

void drawThickLineWithArrow(sf::RenderWindow& window, float x1, float y1, float x2, float y2, float thickness, sf::Color color) {

    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrt(dx * dx + dy * dy);

    if (length < 0.1f) return;

    float ndx = dx / length;
    float ndy = dy / length;

    float pdx = -ndy;
    float pdy = ndx;

    sf::ConvexShape line(4);
    line.setPoint(0, sf::Vector2f(x1 + pdx * thickness / 2, y1 + pdy * thickness / 2));
    line.setPoint(1, sf::Vector2f(x2 + pdx * thickness / 2, y2 + pdy * thickness / 2));
    line.setPoint(2, sf::Vector2f(x2 - pdx * thickness / 2, y2 - pdy * thickness / 2));
    line.setPoint(3, sf::Vector2f(x1 - pdx * thickness / 2, y1 - pdy * thickness / 2));
    line.setFillColor(color);
    window.draw(line);

    float arrowSize = thickness * 2.5f;

    sf::ConvexShape arrow(3);
    arrow.setPoint(0, sf::Vector2f(x2, y2));
    arrow.setPoint(1, sf::Vector2f(x2 - ndx * arrowSize + pdx * arrowSize / 2,
                                   y2 - ndy * arrowSize + pdy * arrowSize / 2));
    arrow.setPoint(2, sf::Vector2f(x2 - ndx * arrowSize - pdx * arrowSize / 2,
                                   y2 - ndy * arrowSize - pdy * arrowSize / 2));
    arrow.setFillColor(color);
    window.draw(arrow);
}

bool drawButton(sf::RenderWindow& window, sf::Font& font, const string& text, float x, float y, float w, float h, sf::Vector2i mouse, bool clicked, bool selected = false) {
    bool hover = mouse.x >= x && mouse.x <= x + w && mouse.y >= y && mouse.y <= y + h;

    unsigned int bgColor = selected ? Colors::BUTTON_ACTIVE : (hover ? Colors::BUTTON_HOVER : Colors::BUTTON_DEFAULT);

    if (hover && !selected) {
        sf::RectangleShape glow(sf::Vector2f(w + 4, h + 4));
        glow.setPosition(x - 2, y - 2);
        glow.setFillColor(hexToColor(0x00d4ff22));
        window.draw(glow);
    }

    sf::RectangleShape btn(sf::Vector2f(w, h));
    btn.setPosition(x, y);
    btn.setFillColor(hexToColor(bgColor));
    btn.setOutlineColor(hexToColor(selected ? Colors::HIGHLIGHT : (hover ? Colors::HIGHLIGHT_DIM : Colors::INPUT_BORDER)));
    btn.setOutlineThickness(1);
    window.draw(btn);

    sf::Text txt(text, font, 13);
    sf::FloatRect bounds = txt.getLocalBounds();
    txt.setPosition(x + (w - bounds.width) / 2, y + (h - bounds.height) / 2 - 3);
    txt.setFillColor(hexToColor(selected ? Colors::DARK_BG : Colors::TEXT_PRIMARY));
    window.draw(txt);

    return hover && clicked;
}

void drawInputField(sf::RenderWindow& window, sf::Font& font, const string& text, float x, float y, float w, float h, bool focused, bool hover) {

    unsigned int bgColor = focused ? 0x252555FF : Colors::INPUT_BG;
    drawRect(window, x, y, w, h, bgColor);

    if (focused) {
        sf::RectangleShape glow(sf::Vector2f(w + 4, h + 4));
        glow.setPosition(x - 2, y - 2);
        glow.setFillColor(sf::Color(0, 255, 200, 30));
        window.draw(glow);
    }

    sf::RectangleShape border(sf::Vector2f(w, h));
    border.setPosition(x, y);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(hexToColor(focused ? Colors::INPUT_FOCUS : (hover ? Colors::HIGHLIGHT_DIM : Colors::INPUT_BORDER)));
    border.setOutlineThickness(focused ? 3 : 2);
    window.draw(border);

    sf::Text txt(text, font, 15);
    txt.setPosition(x + 12.0f, y + (h - 20.0f) / 2.0f);
    txt.setFillColor(hexToColor(Colors::TEXT_PRIMARY));
    window.draw(txt);
}

void drawBar(sf::RenderWindow& window, float x, float y, float w, float h, float percent, unsigned int bgColor, unsigned int fillColor) {
    drawRect(window, x, y, w, h, bgColor);
    if (percent > 0) {
        drawRect(window, x, y, w * percent, h, fillColor);
    }
}

void drawDropdown(sf::RenderWindow& window, sf::Font& font, string portNames[], int portCount, int scroll, float x, float y, float w, int& hoveredItem, sf::Vector2i mouse, bool clicked, int& selectedIndex, string& selectedPort, bool& isOpen) {

    const float itemHeight = 40;
    const int visibleItems = 10;
    float dropdownHeight = min(portCount - scroll, visibleItems) * itemHeight + 20;

    drawRect(window, x + 6, y + 6, w, dropdownHeight, Colors::DROPDOWN_SHADOW);

    drawRect(window, x, y, w, dropdownHeight, Colors::DROPDOWN_BG);

    sf::RectangleShape border(sf::Vector2f(w, dropdownHeight));
    border.setPosition(x, y);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(hexToColor(Colors::HIGHLIGHT));
    border.setOutlineThickness(2);
    window.draw(border);

    hoveredItem = -1;
    float itemY = y + 10;

    for (int i = scroll; i < min(portCount, scroll + visibleItems); i++) {
        bool itemHover = mouse.x >= x + 6 && mouse.x <= x + w - 6 &&
                        mouse.y >= itemY && mouse.y <= itemY + itemHeight - 6;

        if (itemHover) {
            hoveredItem = i;

            drawRect(window, x + 6, itemY, w - 12, itemHeight - 6, 0x3535aaFF);

            drawRect(window, x + 6, itemY, 5, itemHeight - 6, Colors::HIGHLIGHT);
        }

        sf::Text txt(portNames[i], font, 15);
        txt.setPosition(x + 20, itemY + 8);
        txt.setFillColor(hexToColor(itemHover ? Colors::TEXT_PRIMARY : Colors::TEXT_SECONDARY));
        if (itemHover) txt.setStyle(sf::Text::Bold);
        window.draw(txt);

        if (itemHover && clicked) {
            selectedPort = portNames[i];
            selectedIndex = i;
            isOpen = false;
        }

        itemY += itemHeight;
    }

    if (portCount > visibleItems) {
        float scrollbarH = dropdownHeight - 20;
        float thumbH = scrollbarH * ((float)visibleItems / portCount);
        float thumbY = y + 10 + (scrollbarH - thumbH) * ((float)scroll / (portCount - visibleItems));

        drawRect(window, x + w - 12, y + 10, 6, scrollbarH, 0x1a1a40FF);
        drawRect(window, x + w - 12, thumbY, 6, thumbH, Colors::HIGHLIGHT);
    }
}

void drawSectionBox(sf::RenderWindow& window, sf::Font& font, float x, float y, float w, float h, const string& title, float glowIntensity, bool hasIcon = true) {

    if (glowIntensity > 0.01f) {
        sf::Uint8 glowAlpha = (sf::Uint8)(glowIntensity * 40.0f);
        sf::RectangleShape glow1(sf::Vector2f(w + 12.0f, h + 12.0f));
        glow1.setPosition(x - 6.0f, y - 6.0f);
        glow1.setFillColor(sf::Color(0, 255, 204, (sf::Uint8)(glowAlpha * 0.3f)));
        window.draw(glow1);

        sf::RectangleShape glow2(sf::Vector2f(w + 6.0f, h + 6.0f));
        glow2.setPosition(x - 3.0f, y - 3.0f);
        glow2.setFillColor(sf::Color(0, 255, 204, (sf::Uint8)(glowAlpha * 0.6f)));
        window.draw(glow2);
    }

    sf::RectangleShape mainBox(sf::Vector2f(w, h));
    mainBox.setPosition(x, y);
    sf::Color boxColor = hexToColor(0x1a1a3aFF);
    boxColor.a = 245;
    mainBox.setFillColor(boxColor);
    window.draw(mainBox);

    sf::RectangleShape innerGradient(sf::Vector2f(w, h));
    innerGradient.setPosition(x, y);
    innerGradient.setFillColor(sf::Color(40, 60, 100, 15));
    window.draw(innerGradient);

    sf::RectangleShape border(sf::Vector2f(w, h));
    border.setPosition(x, y);
    border.setFillColor(sf::Color::Transparent);
    sf::Color borderColor = hexToColor(Colors::INPUT_BORDER);
    if (glowIntensity > 0.5f) {
        borderColor = hexToColor(Colors::HIGHLIGHT);
    }
    border.setOutlineColor(borderColor);
    border.setOutlineThickness(1.5f);
    window.draw(border);

    if (!title.empty()) {
        if (hasIcon) {
            sf::CircleShape icon(4);
            icon.setPosition(x + 12, y + 13);
            icon.setFillColor(hexToColor(Colors::HIGHLIGHT));
            window.draw(icon);
        }

        sf::Text titleText(title, font, 13);
        titleText.setPosition(x + (hasIcon ? 26 : 12), y + 8);
        titleText.setFillColor(hexToColor(Colors::TEXT_PRIMARY));
        titleText.setStyle(sf::Text::Bold);
        titleText.setLetterSpacing(1.1f);
        window.draw(titleText);

        float underlineWidth = w - 24.0f;
        if (glowIntensity > 0.01f) {
            underlineWidth *= glowIntensity;
        }
        sf::RectangleShape underline(sf::Vector2f(underlineWidth, 2.0f));
        underline.setPosition(x + 12.0f, y + 28.0f);
        sf::Color underlineColor = hexToColor(Colors::HIGHLIGHT);
        underlineColor.a = (sf::Uint8)(glowIntensity * 180.0f + 75.0f);
        underline.setFillColor(underlineColor);
        window.draw(underline);
    }
}

void drawStrategyControl(sf::RenderWindow& window, sf::Font& font, float x, float y, float w, int selectedStrategy, int hoveredButton, float* buttonScales) {
    float buttonW = (w - 8.0f) / 2.0f;
    float buttonH = 38.0f;
    float spacing = 4.0f;

    const char* labels[4] = {"Dijkstra (Cost)", "Dijkstra (Time)", "A* (Cost)", "A* (Time)"};
    UIStrategy strategies[4] = {UI_DIJKSTRA_COST, UI_DIJKSTRA_TIME, UI_ASTAR_COST, UI_ASTAR_TIME};

    for (int i = 0; i < 4; i++) {
        int row = i / 2;
        int col = i % 2;
        float bx = x + col * (buttonW + spacing);
        float by = y + row * (buttonH + spacing);

        bool selected = (strategies[i] == selectedStrategy);
        bool hovered = (i == hoveredButton);
        float scale = buttonScales[i];

        float scaledW = buttonW * scale;
        float scaledH = buttonH * scale;
        float offsetX = (buttonW - scaledW) / 2;
        float offsetY = (buttonH - scaledH) / 2;

        if (selected || hovered) {
            sf::RectangleShape glow(sf::Vector2f(scaledW + 6.0f, scaledH + 6.0f));
            glow.setPosition(bx + offsetX - 3.0f, by + offsetY - 3.0f);
            sf::Color glowColor = selected ? hexToColor(Colors::HIGHLIGHT) : hexToColor(Colors::NEON_PURPLE);
            glowColor.a = (sf::Uint8)(selected ? 60 : 40);
            glow.setFillColor(glowColor);
            window.draw(glow);
        }

        sf::RectangleShape btn(sf::Vector2f(scaledW, scaledH));
        btn.setPosition(bx + offsetX, by + offsetY);
        btn.setFillColor(hexToColor(selected ? 0x2a3a6aFF : 0x1a1a3aFF));
        window.draw(btn);

        sf::RectangleShape border(sf::Vector2f(scaledW, scaledH));
        border.setPosition(bx + offsetX, by + offsetY);
        border.setFillColor(sf::Color::Transparent);
        border.setOutlineColor(hexToColor(selected ? Colors::HIGHLIGHT : Colors::INPUT_BORDER));
        border.setOutlineThickness(selected ? 2.0f : 1.0f);
        window.draw(border);

        sf::Text txt(labels[i], font, 11);
        sf::FloatRect bounds = txt.getLocalBounds();
        txt.setPosition(bx + offsetX + (scaledW - bounds.width) / 2,
                       by + offsetY + (scaledH - bounds.height) / 2 - 3);
        txt.setFillColor(hexToColor(selected ? Colors::TEXT_PRIMARY : Colors::TEXT_SECONDARY));
        txt.setLetterSpacing(1.1f);
        if (selected) txt.setStyle(sf::Text::Bold);
        window.draw(txt);
    }
}

void drawTooltip(sf::RenderWindow& window, sf::Font& font, const string& text, float x, float y, float alpha) {
    if (alpha < 0.01f) return;

    sf::Text txt(text, font, 11);
    sf::FloatRect bounds = txt.getLocalBounds();
    float w = bounds.width + 20;
    float h = 26;

    sf::Uint8 fadeAlpha = (sf::Uint8)(alpha * 255.0f);

    sf::RectangleShape glow(sf::Vector2f(w + 6.0f, h + 6.0f));
    glow.setPosition(x - 3.0f, y - 3.0f);
    glow.setFillColor(sf::Color(0, 255, 204, (sf::Uint8)(fadeAlpha * 0.3f)));
    window.draw(glow);

    sf::RectangleShape bg(sf::Vector2f(w, h));
    bg.setPosition(x, y);
    bg.setFillColor(sf::Color(26, 26, 50, fadeAlpha));
    window.draw(bg);

    sf::RectangleShape border(sf::Vector2f(w, h));
    border.setPosition(x, y);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(sf::Color(0, 255, 204, fadeAlpha));
    border.setOutlineThickness(1.5f);
    window.draw(border);

    txt.setPosition(x + 10.0f, y + 6.0f);
    txt.setFillColor(sf::Color(200, 220, 255, fadeAlpha));
    txt.setLetterSpacing(1.05f);
    window.draw(txt);
}

void drawCard(sf::RenderWindow& window, float x, float y, float w, float h, unsigned int borderColor, float glowIntensity = 0.0f) {

    if (glowIntensity > 0.0f) {
        sf::RectangleShape glow(sf::Vector2f(w + 4, h + 4));
        glow.setPosition(x - 2, y - 2);
        sf::Color gc = hexToColor(borderColor);
        gc.a = (sf::Uint8)(glowIntensity * 60);
        glow.setFillColor(gc);
        window.draw(glow);
    }

    sf::RectangleShape cardBg(sf::Vector2f(w, h));
    cardBg.setPosition(x, y);
    cardBg.setFillColor(sf::Color(18, 22, 35, 240));
    window.draw(cardBg);

    sf::RectangleShape topGrad(sf::Vector2f(w, h * 0.4f));
    topGrad.setPosition(x, y);
    topGrad.setFillColor(sf::Color(25, 30, 45, 100));
    window.draw(topGrad);

    sf::RectangleShape border(sf::Vector2f(w, h));
    border.setPosition(x, y);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(hexToColor(borderColor));
    border.setOutlineThickness(1.5f);
    window.draw(border);
}

void drawTab(sf::RenderWindow& window, sf::Font& font, const string& label, float x, float y, float w, float h, bool active, float hoverGlow) {

    sf::Color bgColor = active ? sf::Color(40, 50, 80, 255) : sf::Color(20, 25, 40, 200);
    sf::RectangleShape bg(sf::Vector2f(w, h));
    bg.setPosition(x, y);
    bg.setFillColor(bgColor);
    window.draw(bg);

    if (hoverGlow > 0.0f) {
        sf::RectangleShape glow(sf::Vector2f(w, h));
        glow.setPosition(x, y);
        glow.setFillColor(sf::Color(0, 200, 255, (sf::Uint8)(hoverGlow * 40)));
        window.draw(glow);
    }

    if (active) {
        sf::RectangleShape indicator(sf::Vector2f(w, 3));
        indicator.setPosition(x, y + h - 3);
        indicator.setFillColor(hexToColor(Colors::ELECTRIC_BLUE));
        window.draw(indicator);
    }

    sf::Text txt(label, font, 11);
    txt.setStyle(sf::Text::Bold);
    sf::FloatRect bounds = txt.getLocalBounds();
    txt.setPosition(x + (w - bounds.width) / 2, y + (h - bounds.height) / 2 - 2);
    txt.setFillColor(active ? hexToColor(Colors::TEXT_PRIMARY) : hexToColor(Colors::TEXT_SECONDARY));
    window.draw(txt);
}

void drawSectionHeader(sf::RenderWindow& window, sf::Font& font, const string& title, float x, float y, float w, unsigned int accentColor) {

    sf::RectangleShape bar(sf::Vector2f(3, 18));
    bar.setPosition(x, y);
    bar.setFillColor(hexToColor(accentColor));
    window.draw(bar);

    sf::Text txt(title, font, 11);
    txt.setStyle(sf::Text::Bold);
    txt.setPosition(x + 8, y);
    txt.setFillColor(hexToColor(Colors::TEXT_PRIMARY));
    txt.setLetterSpacing(1.1f);
    window.draw(txt);

    sf::RectangleShape underline(sf::Vector2f(w - 12, 1));
    underline.setPosition(x + 8, y + 20);
    underline.setFillColor(sf::Color(80, 90, 120, 100));
    window.draw(underline);
}

class MapCameraController {
private:
    sf::FloatRect viewport;
    sf::Vector2f textureSize;
    float currentZoom;
    float minZoom;
    float maxZoom;
    sf::Vector2f panOffset;
    bool isPanning;
    sf::Vector2i lastMousePos;

public:
    MapCameraController() : currentZoom(1.0f), minZoom(1.0f), maxZoom(8.0f), isPanning(false), panOffset(0, 0) {}

    void initialize(const sf::FloatRect& mapViewport, const sf::Vector2f& texSize) {
        viewport = mapViewport;
        textureSize = texSize;

        float scaleX = viewport.width / textureSize.x;
        float scaleY = viewport.height / textureSize.y;
        minZoom = (scaleX < scaleY) ? scaleX : scaleY;

        currentZoom = minZoom;
        panOffset = sf::Vector2f(0, 0);
    }

    void handleMouseWheel(float delta, const sf::Vector2f& mousePixelPos) {

        float zoomFactor = (delta > 0) ? 1.1f : 0.9f;
        float newZoom = currentZoom * zoomFactor;

        if (newZoom < minZoom) {
            newZoom = minZoom;
            zoomFactor = newZoom / currentZoom;
        }
        if (newZoom > maxZoom) {
            newZoom = maxZoom;
            zoomFactor = newZoom / currentZoom;
        }

        if (newZoom != currentZoom) {

            sf::Vector2f mousePosInViewport = mousePixelPos - sf::Vector2f(viewport.left, viewport.top);

            sf::Vector2f mousePosOnMap = (mousePosInViewport - panOffset) / currentZoom;

            currentZoom = newZoom;

            panOffset = mousePosInViewport - (mousePosOnMap * currentZoom);

            clampPan();
        }
    }

    void startPan(const sf::Vector2i& mousePos) {
        if (currentZoom > minZoom) {
            isPanning = true;
            lastMousePos = mousePos;
        }
    }

    void updatePan(const sf::Vector2i& mousePos) {
        if (isPanning) {
            sf::Vector2i delta = mousePos - lastMousePos;
            panOffset.x += (float)delta.x;
            panOffset.y += (float)delta.y;

            clampPan();
            lastMousePos = mousePos;
        }
    }

    void stopPan() {
        isPanning = false;
    }

    void resetZoom() {
        currentZoom = minZoom;
        panOffset = sf::Vector2f(0, 0);
    }

    void zoomIn() {
        float newZoom = currentZoom * 1.1f;
        if (newZoom <= maxZoom) {

            sf::Vector2f centerInViewport(viewport.width / 2.0f, viewport.height / 2.0f);
            sf::Vector2f centerOnMap = (centerInViewport - panOffset) / currentZoom;

            currentZoom = newZoom;
            panOffset = centerInViewport - (centerOnMap * currentZoom);
            clampPan();
        }
    }

    void zoomOut() {
        float newZoom = currentZoom * 0.9f;
        if (newZoom >= minZoom) {

            sf::Vector2f centerInViewport(viewport.width / 2.0f, viewport.height / 2.0f);
            sf::Vector2f centerOnMap = (centerInViewport - panOffset) / currentZoom;

            currentZoom = newZoom;
            panOffset = centerInViewport - (centerOnMap * currentZoom);
            clampPan();
        } else {
            currentZoom = minZoom;
            panOffset = sf::Vector2f(0, 0);
        }
    }

    void applySpriteTransform(sf::Sprite& sprite) {

        sprite.setScale(currentZoom, currentZoom);

        sprite.setPosition(viewport.left + panOffset.x, viewport.top + panOffset.y);
    }

    sf::FloatRect getTransformedBounds(const sf::Sprite& sprite) const {
        return sprite.getGlobalBounds();
    }

    float getZoom() const { return currentZoom; }
    float getMinZoom() const { return minZoom; }
    float getMaxZoom() const { return maxZoom; }

private:
    void clampPan() {

        float scaledWidth = textureSize.x * currentZoom;
        float scaledHeight = textureSize.y * currentZoom;

        float minPanX = viewport.width - scaledWidth;
        if (minPanX > 0) minPanX = 0;

        float maxPanX = 0;

        float minPanY = viewport.height - scaledHeight;
        if (minPanY > 0) minPanY = 0;

        float maxPanY = 0;

        if (panOffset.x < minPanX) panOffset.x = minPanX;
        if (panOffset.x > maxPanX) panOffset.x = maxPanX;
        if (panOffset.y < minPanY) panOffset.y = minPanY;
        if (panOffset.y > maxPanY) panOffset.y = maxPanY;
    }
};

void runOceanRouteNavUI(Graph& graph, JourneyManager& journeyManager) {

    sf::RenderWindow window(sf::VideoMode(1920, 1000), "OceanRoute Navigator - Maritime Route Planning System", sf::Style::Default);
    window.setFramerateLimit(60);

    float windowScaleX = 1.0f;
    float windowScaleY = 1.0f;

    MultiLegRouteBuilder multiLegBuilder(&graph);

    static DockingManager dockingManager;
    static bool dockingManagerInitialized = false;
    if (!dockingManagerInitialized) {
        char portNamesForDocking[MAX_PORTS][30];
        int portCount = 0;
        for (int i = 0; i < MAX_PORTS && i < (int)(sizeof(PORT_POSITIONS) / sizeof(PORT_POSITIONS[0])); i++) {
            strncpy(portNamesForDocking[portCount], PORT_POSITIONS[i].name.c_str(), 29);
            portNamesForDocking[portCount][29] = '\0';
            portCount++;
        }
        dockingManager.initializePorts(portNamesForDocking, portCount);

        int shipCounter = 0;
        Port* port = graph.portHead;
        while (port && shipCounter < 100) {
            Route* route = port->routeHead;
            while (route && shipCounter < 100) {
                DockingShip ship;

                char shipId[50];
                snprintf(shipId, sizeof(shipId), "SHIP-%04d", shipCounter + 1);

                int voyageDuration;
                if (shipCounter < 20) {
                    voyageDuration = 0;
                } else {
                    voyageDuration = 5 + (shipCounter % 25);
                }

                int serviceDuration = 30 + (shipCounter % 60);

                int departureTime = 0;

                ship.setData(shipId, route->shippingCompany.c_str(), (shipCounter % 3 == 0) ? "Container" : (shipCounter % 3 == 1) ? "Tanker" : "Bulk", port->name.c_str(), route->destinationPort.c_str(), route->voyageDate.day, route->voyageDate.month, route->voyageDate.year, departureTime, voyageDuration, serviceDuration);

                dockingManager.enqueueShip(ship);

                shipCounter++;
                route = route->next;
            }
            port = port->next;
        }

        dockingManagerInitialized = true;
    }

    sf::Font font;
    bool fontLoaded = false;

    const char* fontPaths[] = {
        "assets/Roboto-Regular.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "arial.ttf"
    };

    for (int i = 0; i < 5 && !fontLoaded; i++) {
        if (font.loadFromFile(fontPaths[i])) {
            fontLoaded = true;
        }
    }

    if (!fontLoaded) {
        cout << "Warning: Could not load font. UI text may not display.\n";
    }

    sf::Texture mapTexture;
    sf::Sprite mapSprite;
    bool hasMap = mapTexture.loadFromFile("Assets/world_map.png");
    if (!hasMap) hasMap = mapTexture.loadFromFile("assets/world_map.png");
    if (!hasMap) hasMap = mapTexture.loadFromFile("world_map.png");

    float mapAreaHeight = (float)(WINDOW_HEIGHT - 145);

    MapCameraController mapCamera;

    if (hasMap) {
        mapSprite.setTexture(mapTexture);

        float scaleX = (float)MAP_WIDTH / mapTexture.getSize().x;
        float scaleY = mapAreaHeight / mapTexture.getSize().y;
        mapSprite.setScale(scaleX, scaleY);
        mapSprite.setPosition((float)MAP_X, 0);

        sf::FloatRect mapViewport((float)MAP_X, 0, (float)MAP_WIDTH, mapAreaHeight);
        sf::Vector2f texSize((float)mapTexture.getSize().x, (float)mapTexture.getSize().y);
        mapCamera.initialize(mapViewport, texSize);

        gMapBounds = mapSprite.getGlobalBounds();
    } else {

        gMapBounds = sf::FloatRect((float)MAP_X, 0, (float)MAP_WIDTH, mapAreaHeight);
    }

    // Initialize ship animator with sprite sheet
    // Ship sprite sheet is 832x768, which gives 4x4 grid of 208x192 frames
    bool shipSpriteLoaded = gShipAnimator.loadSpriteSheet("Assets/ship.png", 208, 192);
    if (!shipSpriteLoaded) {
        shipSpriteLoaded = gShipAnimator.loadSpriteSheet("assets/ship.png", 208, 192);
    }
    if (shipSpriteLoaded) {
        gShipAnimator.setSpeed(0.20f);
        gShipAnimator.setWaveAmplitude(2.5f);
        gShipAnimator.setWaveFrequency(1.5f);
        gShipAnimator.setScale(0.25f);  // Scale down the large sprite
        cout << "Ship sprite animation loaded successfully.\n";
    } else {
        cout << "Warning: Could not load ship sprite. Using fallback shape.\n";
    }

    struct Particle {
        float x, y, vx, vy, life;
        int type;
    };
    const int MAX_PARTICLES = 50;
    Particle particles[MAX_PARTICLES];
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particles[i].x = (float)(MAP_X + rand() % MAP_WIDTH);
        particles[i].y = (float)(rand() % (int)mapAreaHeight);
        particles[i].vx = (rand() % 100 - 50) / 100.0f;
        particles[i].vy = (rand() % 100 - 50) / 100.0f;
        particles[i].life = (rand() % 100) / 100.0f;
        particles[i].type = rand() % 3 == 0 ? 1 : 0;
    }

    string portNames[MAX_PORTS];
    int portCount = 0;
    getPortList(graph, portNames, portCount);

    UIState state;

    for (int i = 0; i < portCount; i++) {
        if (portNames[i] == state.originPort) state.originIndex = i;
        if (portNames[i] == state.destPort) state.destIndex = i;
    }

    sf::Clock animClock;

    while (window.isOpen()) {
        bool clicked = false;
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);
        float dt = animClock.restart().asSeconds();
        state.pulseTimer += dt;

        if (state.appState == AppState::MAIN_MENU && state.menuFadeAlpha < 1.0f) {
            state.menuFadeAlpha += dt * 1.43f;
            if (state.menuFadeAlpha > 1.0f) state.menuFadeAlpha = 1.0f;
        }

        for (int i = 0; i < 5; i++) {
            float targetScale = 1.0f;
            if (state.appState == AppState::MAIN_MENU && i == state.selectedMenuIndex) {
                targetScale = 1.05f;
            }

            state.buttonScales[i] += (targetScale - state.buttonScales[i]) * dt * 8.0f;
        }

        if (state.animState == UIState::ANIM_SHIP_MOVING && state.exploredFadeAlpha > 0.0f) {
            state.exploredFadeAlpha -= dt * 0.3f;
            if (state.exploredFadeAlpha < 0.0f) state.exploredFadeAlpha = 0.0f;
        } else if (state.animState == UIState::ANIM_EXPLORING || state.animState == UIState::ANIM_DRAWING_LINE) {
            state.exploredFadeAlpha = 1.0f;
        }

        for (int i = 0; i < 5; i++) {
            float targetGlow = (i == state.hoveredSectionBox) ? 1.0f : 0.0f;
            state.sectionGlowIntensity[i] += (targetGlow - state.sectionGlowIntensity[i]) * dt * 10.0f;
        }

        for (int i = 0; i < 4; i++) {
            float targetScale = 1.0f;
            if (i == state.hoveredStrategyButton) targetScale = 1.03f;
            state.strategyButtonScales[i] += (targetScale - state.strategyButtonScales[i]) * dt * 12.0f;
        }

        float targetHighlightX = 0.0f;
        if (state.strategy == UI_DIJKSTRA_TIME || state.strategy == UI_ASTAR_TIME) {
            targetHighlightX = 1.0f;
        }
        state.strategyHighlightX += (targetHighlightX - state.strategyHighlightX) * dt * 15.0f;

        if (state.showTooltip && state.tooltipFadeAlpha < 1.0f) {
            state.tooltipFadeAlpha += dt * 8.0f;
            if (state.tooltipFadeAlpha > 1.0f) state.tooltipFadeAlpha = 1.0f;
        } else if (!state.showTooltip && state.tooltipFadeAlpha > 0.0f) {
            state.tooltipFadeAlpha -= dt * 12.0f;
            if (state.tooltipFadeAlpha < 0.0f) state.tooltipFadeAlpha = 0.0f;
        }

        auto updateAnimatedValue = [&](UIState::AnimatedValue& val) {
            if (val.isAnimating) {
                val.displayValue += (val.target - val.displayValue) * dt * 5.0f;
                if (abs(val.displayValue - val.target) < 0.5f) {
                    val.displayValue = val.target;
                    val.isAnimating = false;
                }
            }
        };
        updateAnimatedValue(state.animatedCost);
        updateAnimatedValue(state.animatedLegs);
        updateAnimatedValue(state.animatedStatesExplored);
        updateAnimatedValue(state.animatedTime);

        if (state.hasResults) {

            float targetTimeEff = state.journeyPortCount > 1 ? 0.75f : 0.0f;
            float targetCostEff = state.journeyPortCount > 1 ? 0.85f : 0.0f;
            state.efficiencyTimeBar += (targetTimeEff - state.efficiencyTimeBar) * dt * 3.0f;
            state.efficiencyCostBar += (targetCostEff - state.efficiencyCostBar) * dt * 3.0f;
        }

        if (state.hasResults && state.journeyPortCount > 1) {

            int currentRisk = 0;
            float targetRiskPos = currentRisk / 2.0f;
            state.riskMeterPosition += (targetRiskPos - state.riskMeterPosition) * dt * 6.0f;
        }

        for (int i = 0; i < 20; i++) {
            float targetLift = (i == state.hoveredRouteCard) ? 1.0f : 0.0f;
            state.routeCardHoverLifts[i] += (targetLift - state.routeCardHoverLifts[i]) * dt * 10.0f;
        }

        if (state.resultsJustUpdated && state.resultsPopInProgress < 1.0f) {
            state.resultsPopInProgress += dt * 2.5f;
            if (state.resultsPopInProgress >= 1.0f) {
                state.resultsPopInProgress = 1.0f;
                state.resultsJustUpdated = false;
            }
        }

        if (state.hasResults && state.journeyPortCount > 1) {
            if (state.animState == UIState::ANIM_EXPLORING) {

                state.explorationAnimTime += dt;

                float progress = state.explorationAnimTime / state.explorationAnimDuration;
                state.explorationEdgesDrawn = (int)(progress * state.totalExploredEdges);

                if (state.explorationAnimTime >= state.explorationAnimDuration) {

                    state.explorationEdgesDrawn = state.totalExploredEdges;
                    state.animState = UIState::ANIM_DRAWING_LINE;
                    state.lineDrawProgress = 0.0f;
                }
            } else if (state.animState == UIState::ANIM_DRAWING_LINE) {

                state.lineDrawProgress += state.shipSpeed * 0.8f * dt;

                if (state.lineDrawProgress >= 1.0f) {

                    state.lineDrawProgress = 1.0f;
                    state.animState = UIState::ANIM_SHIP_MOVING;
                    state.shipAnimationActive = true;
                    state.shipCurrentLeg = 0;
                    state.shipProgress = 0.0f;

                    getPortCoords(state.journeyPorts[0], state.shipX, state.shipY);
                    
                    // Build route path for ShipAnimator
                    std::vector<sf::Vector2f> routePath;
                    for (int p = 0; p < state.journeyPortCount; p++) {
                        float px, py;
                        if (getPortCoords(state.journeyPorts[p], px, py)) {
                            routePath.push_back(sf::Vector2f(px, py));
                        }
                    }
                    if (routePath.size() >= 2) {
                        gShipAnimator.setRoute(routePath);
                        gShipAnimator.start();
                    }
                }
            } else if (state.animState == UIState::ANIM_SHIP_MOVING && state.shipAnimationActive) {

                // Update ShipAnimator
                gShipAnimator.update(dt);
                
                // Sync position/angle from animator for legacy code compatibility
                sf::Vector2f shipPos = gShipAnimator.getPosition();
                state.shipX = shipPos.x;
                state.shipY = shipPos.y;
                state.shipAngle = gShipAnimator.getAngle();
                
                // Check if animation is complete
                if (gShipAnimator.isComplete()) {
                    state.animState = UIState::ANIM_DRAWING_LINE;
                    state.shipAnimationActive = false;
                    state.lineDrawProgress = 0.0f;
                    state.shipCurrentLeg = 0;
                    state.shipProgress = 0.0f;
                    gShipAnimator.reset();
                }
            }
        }

        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }

            if (state.appState == AppState::MAIN_MENU) {
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Escape) {
                        window.close();
                    }

                    else if (event.key.code == sf::Keyboard::Up) {
                        state.selectedMenuIndex = (state.selectedMenuIndex - 1 + 5) % 5;
                    }
                    else if (event.key.code == sf::Keyboard::Down) {
                        state.selectedMenuIndex = (state.selectedMenuIndex + 1) % 5;
                    }
                    else if (event.key.code == sf::Keyboard::Return || event.key.code == sf::Keyboard::Space) {

                        clicked = true;
                    }
                }

                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    clicked = true;
                }
                continue;
            }

            if (event.type == sf::Event::Resized) {

                sf::FloatRect visibleArea(0, 0, (float)event.size.width, (float)event.size.height);
                window.setView(sf::View(visibleArea));

                windowScaleX = (float)event.size.width / WINDOW_WIDTH;
                windowScaleY = (float)event.size.height / WINDOW_HEIGHT;

                if (hasMap) {
                    float newMapWidth = MAP_WIDTH * windowScaleX;
                    float newMapHeight = mapAreaHeight * windowScaleY;
                    float scaleX = newMapWidth / mapTexture.getSize().x;
                    float scaleY = newMapHeight / mapTexture.getSize().y;
                    mapSprite.setScale(scaleX, scaleY);
                    mapSprite.setPosition(LEFT_SIDEBAR_WIDTH * windowScaleX, 0);

                    gMapBounds = mapSprite.getGlobalBounds();
                }
            }

            if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    clicked = true;
                }
            }

            if (state.appState != AppState::ROUTE_PLANNER &&
                state.appState != AppState::COMPANY_VIEWER &&
                state.appState != AppState::MULTILEG_EDITOR &&
                state.appState != AppState::DOCKING_MANAGER) {
                continue;
            }

            if (event.type == sf::Event::MouseWheelScrolled) {
                if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel && hasMap) {
                    sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
                    sf::Vector2f mousePos((float)mousePixel.x, (float)mousePixel.y);

                }
            }

            if (event.type == sf::Event::MouseButtonPressed) {

                if ((event.mouseButton.button == sf::Mouse::Middle ||
                         event.mouseButton.button == sf::Mouse::Right) && hasMap) {

                    if (mousePos.x >= MAP_X && mousePos.x < MAP_X + MAP_WIDTH &&
                        mousePos.y >= 0 && mousePos.y < mapAreaHeight) {
                        mapCamera.startPan(mousePos);
                    }
                }
            }

            if (event.type == sf::Event::MouseButtonReleased) {
                if (event.mouseButton.button == sf::Mouse::Left) {

                }
                else if (event.mouseButton.button == sf::Mouse::Middle ||
                         event.mouseButton.button == sf::Mouse::Right) {

                    mapCamera.stopPan();
                }
            }

            if (event.type == sf::Event::MouseMoved) {
                if (hasMap) {

                    mapCamera.updatePan(mousePos);
                }
            }

            if (event.type == sf::Event::TextEntered) {

                if (state.appState == AppState::MULTILEG_EDITOR && state.editorInputActive) {
                    char c = static_cast<char>(event.text.unicode);

                    if (c == 8) {
                        if (!state.editorInputBuffer.empty()) {
                            state.editorInputBuffer.pop_back();
                        }
                    } else if (c == 13 || c == 10) {

                        if (!state.editorInputBuffer.empty() && multiLegBuilder.getNodeCount() < 10) {
                            multiLegBuilder.appendPort(state.editorInputBuffer);
                            state.editorInputBuffer = "";
                            state.editorShowResults = false;
                        }
                    } else if (c >= 32 && c < 127 && state.editorInputBuffer.length() < 30) {

                        state.editorInputBuffer += c;
                    }
                } else if (state.activeField != UIState::NONE) {
                    char c = static_cast<char>(event.text.unicode);
                    if (c >= '0' && c <= '9') {
                        int maxLen = 4;
                        if (state.activeField == UIState::DAY || state.activeField == UIState::MONTH) maxLen = 2;
                        if (state.activeField == UIState::MAX_LEGS) maxLen = 2;
                        if (state.activeField == UIState::MAX_COST) maxLen = 7;

                        if (state.inputBuffer.length() < (size_t)maxLen) {
                            state.inputBuffer += c;
                        }
                    }
                }
            }

            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) {
                    if (state.activeField != UIState::NONE || state.originDropdownOpen || state.destDropdownOpen) {
                        state.activeField = UIState::NONE;
                        state.originDropdownOpen = false;
                        state.destDropdownOpen = false;
                    } else {
                        window.close();
                    }
                }

                if (event.key.code == sf::Keyboard::N) {
                    state.showAllRoutes = !state.showAllRoutes;
                }

                if (event.key.code == sf::Keyboard::Equal || event.key.code == sf::Keyboard::Add) {

                    if (hasMap) {

                    }
                }
                if (event.key.code == sf::Keyboard::Hyphen || event.key.code == sf::Keyboard::Subtract) {

                    if (hasMap) {

                    }
                }
                if (event.key.code == sf::Keyboard::R) {

                    if (hasMap) {

                        state.mapViewZoom = 1.0f;
                    }
                }

                if (event.key.code == sf::Keyboard::BackSpace && state.activeField != UIState::NONE) {
                    if (!state.inputBuffer.empty()) {
                        state.inputBuffer.pop_back();
                    }
                }
                if (event.key.code == sf::Keyboard::Return && state.activeField != UIState::NONE) {
                    int val = 0;
                    if (!state.inputBuffer.empty()) {
                        val = stoi(state.inputBuffer);
                    }

                    switch (state.activeField) {
                        case UIState::DAY:
                            state.day = max(1, min(31, val));
                            if (state.appState == AppState::DOCKING_MANAGER) {
                                dockingManager.loadRoutesForDate("Routes.txt", state.day, state.month, state.year);
                            }
                            break;
                        case UIState::MONTH:
                            state.month = max(1, min(12, val));
                            if (state.appState == AppState::DOCKING_MANAGER) {
                                dockingManager.loadRoutesForDate("Routes.txt", state.day, state.month, state.year);
                            }
                            break;
                        case UIState::YEAR:
                            state.year = max(2024, min(2030, val));
                            if (state.appState == AppState::DOCKING_MANAGER) {
                                dockingManager.loadRoutesForDate("Routes.txt", state.day, state.month, state.year);
                            }
                            break;
                        case UIState::MAX_COST:
                            state.maxCost = max(1000, min(999999, val));
                            break;
                        case UIState::MAX_LEGS:
                            state.maxLegs = max(1, min(15, val));
                            break;
                        default: break;
                    }

                    state.activeField = UIState::NONE;
                    state.inputBuffer = "";
                }
            }

            if (event.type == sf::Event::MouseWheelScrolled) {
                if (state.appState == AppState::DOCKING_MANAGER && state.originDropdownOpen) {
                    state.dropdownScroll -= (int)(event.mouseWheelScroll.delta * 2);
                    state.dropdownScroll = max(0, min(portCount - 9, state.dropdownScroll));
                }
                else if (state.originDropdownOpen || state.destDropdownOpen) {
                    state.dropdownScroll -= (int)(event.mouseWheelScroll.delta * 2);
                    state.dropdownScroll = max(0, min(portCount - 8, state.dropdownScroll));
                }
                else if (state.preferencesDropdownOpen) {
                    state.preferencesScrollOffset -= (int)(event.mouseWheelScroll.delta * 2);
                    state.preferencesScrollOffset = max(0, min(10 - 6, state.preferencesScrollOffset));
                }
                else if (state.avoidPortsDropdownOpen) {
                    state.avoidPortsScrollOffset -= (int)(event.mouseWheelScroll.delta * 2);
                    state.avoidPortsScrollOffset = max(0, min(portCount - 9, state.avoidPortsScrollOffset));
                }
                else if (state.currentView == VIEW_COMPANY_ROUTES && mousePos.x < LEFT_SIDEBAR_WIDTH) {
                    state.companyScrollOffset -= (int)(event.mouseWheelScroll.delta);
                    int maxScroll = max(0, state.companyCount - 12);
                    state.companyScrollOffset = max(0, min(maxScroll, state.companyScrollOffset));
                }

                else if (mousePos.x >= (WINDOW_WIDTH - RIGHT_SIDEBAR_WIDTH) && state.hasResults) {
                    state.journeyScrollOffset -= (int)(event.mouseWheelScroll.delta);
                    int maxScroll = max(0, state.journeyListCount - 3);
                    state.journeyScrollOffset = max(0, min(maxScroll, state.journeyScrollOffset));
                }

                else if (hasMap && mousePos.x >= MAP_X && mousePos.x < MAP_X + MAP_WIDTH &&
                         mousePos.y >= 0 && mousePos.y < mapAreaHeight) {

                    sf::Vector2f mousePixelPos((float)mousePos.x, (float)mousePos.y);
                    mapCamera.handleMouseWheel(event.mouseWheelScroll.delta, mousePixelPos);
                    state.mapViewZoom = mapCamera.getZoom();
                }
            }
        }

        if (state.shipAnimationActive && state.journeyPortCount > 1) {

            state.shipProgress += state.shipSpeed * dt;

            if (state.shipProgress >= 1.0f) {
                state.shipCurrentLeg++;
                state.shipProgress = 0.0f;

                if (state.shipCurrentLeg >= state.journeyPortCount - 1) {

                    state.shipCurrentLeg = 0;
                    state.shipProgress = 0.0f;
                }
            }

            float x1, y1, x2, y2;
            if (getPortCoords(state.journeyPorts[state.shipCurrentLeg], x1, y1) &&
                getPortCoords(state.journeyPorts[state.shipCurrentLeg + 1], x2, y2)) {

                state.shipX = x1 + (x2 - x1) * state.shipProgress;
                state.shipY = y1 + (y2 - y1) * state.shipProgress;

                state.shipAngle = atan2(y2 - y1, x2 - x1) * 180.0f / 3.14159f;
            }
        }

        window.clear(hexToColor(Colors::DARK_BG));

        state.hoveredEdge = false;

        if (state.appState == AppState::MAIN_MENU) {

            sf::Uint8 fadeAlpha = (sf::Uint8)(state.menuFadeAlpha * 255);

            sf::RectangleShape bgGradient1(sf::Vector2f((float)WINDOW_WIDTH, (float)WINDOW_HEIGHT / 2));
            bgGradient1.setPosition(0, 0);
            bgGradient1.setFillColor(sf::Color(10, 20, 40, (sf::Uint8)(fadeAlpha * 0.3f)));
            window.draw(bgGradient1);

            sf::RectangleShape bgGradient2(sf::Vector2f((float)WINDOW_WIDTH, (float)WINDOW_HEIGHT / 2));
            bgGradient2.setPosition(0, (float)WINDOW_HEIGHT / 2);
            bgGradient2.setFillColor(sf::Color(15, 10, 30, (sf::Uint8)(fadeAlpha * 0.3f)));
            window.draw(bgGradient2);

            float titleY = 120;

            for (int glow = 3; glow > 0; glow--) {
                sf::Text titleGlow;
                titleGlow.setFont(font);
                titleGlow.setString("OCEANROUTE NAVIGATOR");
                titleGlow.setCharacterSize(76);
                titleGlow.setFillColor(sf::Color(0, 255, 200, (sf::Uint8)(fadeAlpha * 0.15f * glow)));
                titleGlow.setPosition((float)WINDOW_WIDTH / 2 - 420 - glow * 2, titleY - glow * 2);
                titleGlow.setLetterSpacing(1.2f);
                window.draw(titleGlow);
            }

            sf::Text titleText;
            titleText.setFont(font);
            titleText.setString("OCEANROUTE NAVIGATOR");
            titleText.setCharacterSize(76);
            titleText.setFillColor(sf::Color(0, 255, 220, fadeAlpha));
            titleText.setPosition((float)WINDOW_WIDTH / 2 - 420, titleY);
            titleText.setLetterSpacing(1.2f);
            titleText.setStyle(sf::Text::Bold);
            window.draw(titleText);

            float lineY = titleY + 90;
            sf::RectangleShape decorLine(sf::Vector2f(600, 3));
            decorLine.setPosition((float)WINDOW_WIDTH / 2 - 300, lineY);
            decorLine.setFillColor(sf::Color(0, 255, 200, (sf::Uint8)(fadeAlpha * 0.6f)));
            window.draw(decorLine);

            sf::RectangleShape lineGlow(sf::Vector2f(600, 12));
            lineGlow.setPosition((float)WINDOW_WIDTH / 2 - 300, lineY - 5);
            lineGlow.setFillColor(sf::Color(0, 255, 200, (sf::Uint8)(fadeAlpha * 0.2f)));
            window.draw(lineGlow);

            sf::Text subtitleText;
            subtitleText.setFont(font);
            subtitleText.setString("Maritime Route Planning System");
            subtitleText.setCharacterSize(24);
            subtitleText.setFillColor(sf::Color(150, 200, 255, (sf::Uint8)(fadeAlpha * 0.85f)));
            subtitleText.setPosition((float)WINDOW_WIDTH / 2 - 200, lineY + 30);
            subtitleText.setLetterSpacing(1.5f);
            window.draw(subtitleText);

            float btnY = 320;
            float btnWidth = 520;
            float btnHeight = 68;
            float btnSpacing = 82;
            float btnX = (float)WINDOW_WIDTH / 2 - btnWidth / 2;

            struct MenuButton {
                string label;
                AppState targetState;
                sf::FloatRect bounds;
            };

            MenuButton menuButtons[] = {{"ROUTE PLANNER", AppState::ROUTE_PLANNER, sf::FloatRect(btnX, btnY, btnWidth, btnHeight)}, {"COMPANY ROUTES VIEWER", AppState::COMPANY_VIEWER, sf::FloatRect(btnX, btnY + btnSpacing, btnWidth, btnHeight)}, {"MULTI-LEG JOURNEY EDITOR", AppState::MULTILEG_EDITOR, sf::FloatRect(btnX, btnY + btnSpacing * 2, btnWidth, btnHeight)}, {"DOCKING & LAYOVER MANAGER", AppState::DOCKING_MANAGER, sf::FloatRect(btnX, btnY + btnSpacing * 3, btnWidth, btnHeight)}, {"EXIT", AppState::MAIN_MENU, sf::FloatRect(btnX, btnY + btnSpacing * 4, btnWidth, btnHeight)}};

            for (int i = 0; i < 5; i++) {
                bool hovered = menuButtons[i].bounds.contains((float)mousePos.x, (float)mousePos.y) || (i == state.selectedMenuIndex);
                float scale = state.buttonScales[i];

                float scaledW = btnWidth * scale;
                float scaledH = btnHeight * scale;
                float scaledX = menuButtons[i].bounds.left - (scaledW - btnWidth) / 2;
                float scaledY = menuButtons[i].bounds.top - (scaledH - btnHeight) / 2;

                if (hovered) {
                    for (int g = 4; g > 0; g--) {
                        sf::RectangleShape glow(sf::Vector2f(scaledW + g * 8, scaledH + g * 8));
                        glow.setPosition(scaledX - g * 4, scaledY - g * 4);
                        unsigned int glowColor = (i == 4) ? 0xff4466FF : 0x00ffccFF;
                        sf::Uint8 glowAlpha = (sf::Uint8)(fadeAlpha * 0.15f * (5 - g));
                        glow.setFillColor(sf::Color((glowColor >> 24) & 0xFF, (glowColor >> 16) & 0xFF, (glowColor >> 8) & 0xFF, glowAlpha));
                        window.draw(glow);
                    }
                }

                sf::RectangleShape btn(sf::Vector2f(scaledW, scaledH));
                btn.setPosition(scaledX, scaledY);

                if (hovered) {

                    unsigned int topColor = (i == 4) ? 0x4a2a40FF : 0x2a4a5aFF;
                    btn.setFillColor(sf::Color((topColor >> 24) & 0xFF, (topColor >> 16) & 0xFF, (topColor >> 8) & 0xFF, fadeAlpha));
                } else {
                    btn.setFillColor(sf::Color(26, 26, 50, (sf::Uint8)(fadeAlpha * 0.7f)));
                }

                unsigned int borderColor = (i == 4) ? 0xff4466FF : (hovered ? 0x00ffccFF : 0x4a4a80FF);
                btn.setOutlineColor(sf::Color((borderColor >> 24) & 0xFF, (borderColor >> 16) & 0xFF, (borderColor >> 8) & 0xFF, fadeAlpha));
                btn.setOutlineThickness(hovered ? 3.0f : 1.5f);
                window.draw(btn);

                if (hovered) {
                    sf::Text textGlow;
                    textGlow.setFont(font);
                    textGlow.setString(menuButtons[i].label);
                    textGlow.setCharacterSize(22);
                    textGlow.setFillColor(sf::Color(255, 255, 255, (sf::Uint8)(fadeAlpha * 0.3f)));
                    sf::FloatRect glowBounds = textGlow.getLocalBounds();
                    textGlow.setPosition(
                        scaledX + (scaledW - glowBounds.width) / 2 - 1,
                        scaledY + (scaledH - glowBounds.height) / 2 - 6
                    );
                    textGlow.setLetterSpacing(1.1f);
                    window.draw(textGlow);
                }

                sf::Text btnText;
                btnText.setFont(font);
                btnText.setString(menuButtons[i].label);
                btnText.setCharacterSize(22);
                btnText.setFillColor(sf::Color(255, 255, 255, fadeAlpha));
                btnText.setLetterSpacing(1.1f);
                if (hovered) btnText.setStyle(sf::Text::Bold);

                sf::FloatRect textBounds = btnText.getLocalBounds();
                btnText.setPosition(
                    scaledX + (scaledW - textBounds.width) / 2,
                    scaledY + (scaledH - textBounds.height) / 2 - 5
                );
                window.draw(btnText);

                if ((clicked && hovered && menuButtons[i].bounds.contains((float)mousePos.x, (float)mousePos.y)) ||
                    (clicked && i == state.selectedMenuIndex && !menuButtons[i].bounds.contains((float)mousePos.x, (float)mousePos.y))) {
                    if (i == 4) {
                        window.close();
                    } else {
                        state.appState = menuButtons[i].targetState;
                        state.currentView = VIEW_MAIN_SEARCH;
                        state.menuFadeAlpha = 0.0f;

                        if (state.appState == AppState::COMPANY_VIEWER) {

                            collectAllCompanies(graph, state);
                        } else if (state.appState == AppState::MULTILEG_EDITOR) {

                            multiLegBuilder.clear();
                            state.editorActive = true;
                            state.editorJourneyId = -1;
                            state.editorSelectedNodeIndex = -1;
                            state.editorShowResults = false;
                            state.editorInputBuffer = "";
                            state.editorInputActive = false;
                            state.editorSegmentCount = 0;
                        } else if (state.appState == AppState::DOCKING_MANAGER) {
                            dockingManager.loadRoutesForDate("Routes.txt", state.day, state.month, state.year);
                        }
                    }
                }
            }

            sf::Text footerText;
            footerText.setFont(font);
            footerText.setString(L"\u00A9 2025 OceanRoute Navigator  |  Press ESC to exit");
            footerText.setCharacterSize(16);
            footerText.setLetterSpacing(1.3f);

            sf::FloatRect footerBounds = footerText.getLocalBounds();
            float footerX = (float)WINDOW_WIDTH / 2 - footerBounds.width / 2;
            float footerY = (float)WINDOW_HEIGHT - 60;

            sf::Text footerGlow;
            footerGlow.setFont(font);
            footerGlow.setString(L"\u00A9 2025 OceanRoute Navigator  |  Press ESC to exit");
            footerGlow.setCharacterSize(16);
            footerGlow.setLetterSpacing(1.3f);
            footerGlow.setFillColor(sf::Color(0, 255, 204, (sf::Uint8)(fadeAlpha * 0.2f)));
            footerGlow.setPosition(footerX, footerY);
            window.draw(footerGlow);

            footerText.setFillColor(sf::Color(120, 140, 170, (sf::Uint8)(fadeAlpha * 0.6f)));
            footerText.setPosition(footerX, footerY);
            window.draw(footerText);

            window.display();
            continue;
        }

        float mapH = (float)(WINDOW_HEIGHT - 145);

        sf::RectangleShape mapBg(sf::Vector2f((float)MAP_WIDTH, mapH));
        mapBg.setPosition((float)MAP_X, 0);
        mapBg.setFillColor(sf::Color(5, 8, 20, 255));
        window.draw(mapBg);

        float glowIntensity = 40 + 20 * sin(state.pulseTimer * 2.0f);
        for (int i = 0; i < 4; i++) {
            float thickness = 3.0f + i * 2.0f;
            float alpha = glowIntensity - i * 8.0f;

            sf::RectangleShape topGlow(sf::Vector2f((float)MAP_WIDTH, thickness));
            topGlow.setPosition((float)MAP_X, -thickness / 2);
            topGlow.setFillColor(sf::Color(0, 255, 220, (sf::Uint8)alpha));
            window.draw(topGlow);

            sf::RectangleShape bottomGlow(sf::Vector2f((float)MAP_WIDTH, thickness));
            bottomGlow.setPosition((float)MAP_X, mapH - thickness / 2);
            bottomGlow.setFillColor(sf::Color(0, 200, 255, (sf::Uint8)alpha));
            window.draw(bottomGlow);

            sf::RectangleShape leftGlow(sf::Vector2f(thickness, mapH));
            leftGlow.setPosition((float)MAP_X - thickness / 2, 0);
            leftGlow.setFillColor(sf::Color(0, 230, 255, (sf::Uint8)alpha));
            window.draw(leftGlow);

            sf::RectangleShape rightGlow(sf::Vector2f(thickness, mapH));
            rightGlow.setPosition((float)(MAP_X + MAP_WIDTH) - thickness / 2, 0);
            rightGlow.setFillColor(sf::Color(0, 210, 255, (sf::Uint8)alpha));
            window.draw(rightGlow);
        }

        if (hasMap) {

            mapCamera.applySpriteTransform(mapSprite);

            window.draw(mapSprite);

            gMapBounds = mapSprite.getGlobalBounds();

            float scanY = fmod(state.pulseTimer * 100, mapH);
            sf::RectangleShape scanline(sf::Vector2f((float)MAP_WIDTH, 3));
            scanline.setPosition((float)MAP_X, scanY);
            scanline.setFillColor(sf::Color(0, 200, 255, 30));
            window.draw(scanline);
        }

        if (!hasMap) {

            for (int gx = 0; gx < MAP_WIDTH; gx += 50) {
                float alpha = 20 + 10 * sin(state.pulseTimer + gx * 0.01f);
                sf::RectangleShape gridV(sf::Vector2f(1, mapH));
                gridV.setPosition((float)(MAP_X + gx), 0);
                gridV.setFillColor(sf::Color(0, 100, 200, (sf::Uint8)alpha));
                window.draw(gridV);
            }
            for (int gy = 0; gy < (int)mapH; gy += 50) {
                float alpha = 20 + 10 * sin(state.pulseTimer + gy * 0.01f);
                sf::RectangleShape gridH(sf::Vector2f((float)MAP_WIDTH, 1));
                gridH.setPosition((float)MAP_X, (float)gy);
                gridH.setFillColor(sf::Color(0, 100, 200, (sf::Uint8)alpha));
                window.draw(gridH);
            }
        }

        for (int i = 0; i < MAX_PARTICLES; i++) {
            particles[i].x += particles[i].vx * dt * 30;
            particles[i].y += particles[i].vy * dt * 30;
            particles[i].life += dt * 0.3f;

            if (particles[i].x < MAP_X) particles[i].x = MAP_X + MAP_WIDTH;
            if (particles[i].x > MAP_X + MAP_WIDTH) particles[i].x = MAP_X;
            if (particles[i].y < 0) particles[i].y = mapH;
            if (particles[i].y > mapH) particles[i].y = 0;

            float alpha = 80 + 40 * sin(particles[i].life * 3);

            if (particles[i].type == 1) {

                sf::ConvexShape ship(3);
                ship.setPoint(0, sf::Vector2f(0, -6));
                ship.setPoint(1, sf::Vector2f(-4, 6));
                ship.setPoint(2, sf::Vector2f(4, 6));
                ship.setPosition(particles[i].x, particles[i].y);
                ship.setFillColor(sf::Color(100, 200, 255, (sf::Uint8)alpha));
                ship.setRotation(atan2(particles[i].vy, particles[i].vx) * 57.3f + 90);
                window.draw(ship);
            } else {

                sf::CircleShape dot(2 + sin(particles[i].life * 2) * 1);
                dot.setPosition(particles[i].x - 2, particles[i].y - 2);
                dot.setFillColor(sf::Color(0, 180, 255, (sf::Uint8)(alpha * 0.5f)));
                window.draw(dot);
            }
        }

        sf::RectangleShape mapBorder(sf::Vector2f((float)MAP_WIDTH, mapH));
        mapBorder.setPosition((float)MAP_X, 0);
        mapBorder.setFillColor(sf::Color::Transparent);
        mapBorder.setOutlineColor(sf::Color(0, 150, 255, 150));
        mapBorder.setOutlineThickness(2);
        window.draw(mapBorder);

        float cornerSize = 30 + 5 * sin(state.pulseTimer * 2);
        sf::Color cornerColor(0, 255, 200, 200);

        sf::RectangleShape tlH(sf::Vector2f(cornerSize, 3));
        tlH.setPosition((float)MAP_X, 0);
        tlH.setFillColor(cornerColor);
        window.draw(tlH);
        sf::RectangleShape tlV(sf::Vector2f(3, cornerSize));
        tlV.setPosition((float)MAP_X, 0);
        tlV.setFillColor(cornerColor);
        window.draw(tlV);

        sf::RectangleShape trH(sf::Vector2f(cornerSize, 3));
        trH.setPosition((float)(MAP_X + MAP_WIDTH - cornerSize), 0);
        trH.setFillColor(cornerColor);
        window.draw(trH);
        sf::RectangleShape trV(sf::Vector2f(3, cornerSize));
        trV.setPosition((float)(MAP_X + MAP_WIDTH - 3), 0);
        trV.setFillColor(cornerColor);
        window.draw(trV);

        sf::RectangleShape blH(sf::Vector2f(cornerSize, 3));
        blH.setPosition((float)MAP_X, mapH - 3);
        blH.setFillColor(cornerColor);
        window.draw(blH);
        sf::RectangleShape blV(sf::Vector2f(3, cornerSize));
        blV.setPosition((float)MAP_X, mapH - cornerSize);
        blV.setFillColor(cornerColor);
        window.draw(blV);

        sf::RectangleShape brH(sf::Vector2f(cornerSize, 3));
        brH.setPosition((float)(MAP_X + MAP_WIDTH - cornerSize), mapH - 3);
        brH.setFillColor(cornerColor);
        window.draw(brH);
        sf::RectangleShape brV(sf::Vector2f(3, cornerSize));
        brV.setPosition((float)(MAP_X + MAP_WIDTH - 3), mapH - cornerSize);
        brV.setFillColor(cornerColor);
        window.draw(brV);

        if (state.appState != AppState::DOCKING_MANAGER) {
            float compassX = MAP_X + MAP_WIDTH - 70;
            float compassY = mapH - 70;
            float compassR = 35;

            sf::CircleShape compassBg(compassR);
            compassBg.setPosition(compassX - compassR, compassY - compassR);
            compassBg.setFillColor(sf::Color(10, 20, 40, 200));
            compassBg.setOutlineColor(sf::Color(0, 200, 255, 150));
            compassBg.setOutlineThickness(2);
            window.draw(compassBg);

            float needleAngle = state.pulseTimer * 5;
            sf::ConvexShape needle(3);
            needle.setPoint(0, sf::Vector2f(0, -compassR + 8));
            needle.setPoint(1, sf::Vector2f(-6, 10));
            needle.setPoint(2, sf::Vector2f(6, 10));
            needle.setPosition(compassX, compassY);
            needle.setRotation(needleAngle);
            needle.setFillColor(sf::Color(255, 50, 50, 200));
            window.draw(needle);

            sf::Text nText("N", font, 12);
            nText.setPosition(compassX - 4, compassY - compassR + 3);
            nText.setFillColor(sf::Color::White);
            nText.setStyle(sf::Text::Bold);
            window.draw(nText);
        }

        if (state.currentView == VIEW_COMPANY_ROUTES) {

            Port* p = graph.portHead;

            unsigned int companyColors[] = {0xFF6B6BFF, 0x4ECDC4FF, 0xFFE66DFF, 0x95E1D3FF, 0xF38181FF, 0xAA96DAFF, 0xFCBAD3FF, 0xA8E6CFFF, 0xFF8B94FF, 0xC7CEAAFF, 0xFFD3B6FF, 0xDCEDC1FF};
            int colorCount = 12;

            while (p) {
                float ox, oy;
                if (getPortCoords(p->name, ox, oy)) {
                    Route* r = p->routeHead;
                    while (r) {

                        bool shouldDisplay = false;
                        int companyColorIdx = 0;

                        if (state.showAllCompanies || state.selectedCompanyIndex == -1) {

                            shouldDisplay = true;

                            for (int i = 0; i < state.companyCount; i++) {
                                if (state.companyList[i] == r->shippingCompany) {
                                    companyColorIdx = i % colorCount;
                                    break;
                                }
                            }
                        } else if (state.selectedCompanyIndex >= 0 && state.selectedCompanyIndex < state.companyCount) {

                            if (r->shippingCompany == state.companyList[state.selectedCompanyIndex]) {
                                shouldDisplay = true;
                                companyColorIdx = state.selectedCompanyIndex % colorCount;
                            }
                        }

                        if (shouldDisplay) {
                            float dx, dy;
                            if (getPortCoords(r->destinationPort, dx, dy)) {
                                float length = sqrt((dx-ox)*(dx-ox) + (dy-oy)*(dy-oy));
                                float angle = atan2(dy-oy, dx-ox) * 180 / 3.14159f;

                                float mousePosX = (float)mousePos.x;
                                float mousePosY = (float)mousePos.y;
                                float edgeDx = dx - ox;
                                float edgeDy = dy - oy;
                                float edgeLength = sqrt(edgeDx*edgeDx + edgeDy*edgeDy);
                                float lineDist = abs(edgeDx * (oy - mousePosY) - (ox - mousePosX) * edgeDy) / edgeLength;
                                float t = ((mousePosX - ox) * edgeDx + (mousePosY - oy) * edgeDy) / (edgeLength * edgeLength);
                                bool withinSegment = (t >= 0 && t <= 1);
                                bool isHovered = (withinSegment && lineDist < 8.0f);

                                if (isHovered) {
                                    state.hoveredEdge = true;
                                    state.hoveredEdgeSource = p->name;
                                    state.hoveredEdgeDest = r->destinationPort;
                                    state.hoveredEdgeCompany = r->shippingCompany;
                                    state.hoveredEdgeCost = (float)r->voyageCost;
                                    state.hoveredEdgeTooltipX = mousePosX + 15.0f;
                                    state.hoveredEdgeTooltipY = mousePosY - 50.0f;

                                    char dateStr[32];
                                    snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%d",
                                            r->voyageDate.day, r->voyageDate.month, r->voyageDate.year);
                                    state.hoveredEdgeDate = dateStr;

                                    char timeStr[16];
                                    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", r->departureTime.hour, r->departureTime.minute);
                                    state.hoveredEdgeTime = timeStr;
                                }

                                sf::Color routeColor = hexToColor(companyColors[companyColorIdx]);

                                float lineWidth = (isHovered || state.selectedCompanyIndex >= 0) ? 3.5f : 2.5f;
                                int alpha = isHovered ? 255 : (state.selectedCompanyIndex >= 0 ? 200 : 120);
                                routeColor.a = alpha;

                                sf::RectangleShape routeLine;
                                routeLine.setSize(sf::Vector2f(length, lineWidth));
                                routeLine.setPosition(ox, oy - lineWidth/2);
                                routeLine.setRotation(angle);
                                routeLine.setFillColor(routeColor);
                                window.draw(routeLine);

                                if (isHovered || state.selectedCompanyIndex >= 0) {
                                    sf::RectangleShape glow;
                                    glow.setSize(sf::Vector2f(length, lineWidth + 4));
                                    glow.setPosition(ox, oy - (lineWidth + 4)/2);
                                    glow.setRotation(angle);
                                    sf::Color glowColor = routeColor;
                                    glowColor.a = isHovered ? 100 : 60;
                                    glow.setFillColor(glowColor);
                                    window.draw(glow);
                                }

                                if (state.selectedCompanyIndex >= 0) {

                                    Port* destPort = graph.portHead;
                                    while (destPort != nullptr) {
                                        if (destPort->name == r->destinationPort) {

                                            Route* connectingRoute = destPort->routeHead;
                                            while (connectingRoute != nullptr) {
                                                if (connectingRoute->shippingCompany == r->shippingCompany) {

                                                    float cx, cy;
                                                    if (getPortCoords(connectingRoute->destinationPort, cx, cy)) {
                                                        float cLength = sqrt((cx-dx)*(cx-dx) + (cy-dy)*(cy-dy));
                                                        float cAngle = atan2(cy-dy, cx-dx) * 180 / 3.14159f;

                                                        sf::Color secondLegColor = routeColor;
                                                        secondLegColor.a = 180;

                                                        sf::RectangleShape secondGlow;
                                                        secondGlow.setSize(sf::Vector2f(cLength, lineWidth + 6));
                                                        secondGlow.setPosition(dx, dy - (lineWidth + 6)/2);
                                                        secondGlow.setRotation(cAngle);
                                                        sf::Color secondGlowColor = secondLegColor;
                                                        secondGlowColor.a = 80;
                                                        secondGlow.setFillColor(secondGlowColor);
                                                        window.draw(secondGlow);

                                                        sf::RectangleShape secondLine;
                                                        secondLine.setSize(sf::Vector2f(cLength, lineWidth));
                                                        secondLine.setPosition(dx, dy - lineWidth/2);
                                                        secondLine.setRotation(cAngle);
                                                        secondLine.setFillColor(secondLegColor);
                                                        window.draw(secondLine);

                                                        sf::CircleShape connector(5);
                                                        connector.setPosition(dx - 5, dy - 5);
                                                        connector.setFillColor(secondLegColor);
                                                        connector.setOutlineColor(sf::Color(255, 255, 255, 200));
                                                        connector.setOutlineThickness(1.5f);
                                                        window.draw(connector);
                                                    }
                                                    break;
                                                }
                                                connectingRoute = connectingRoute->next;
                                            }
                                            break;
                                        }
                                        destPort = destPort->next;
                                    }
                                }
                            }
                        }

                        r = r->next;
                    }
                }
                p = p->next;
            }
        } else {

        if (state.showAllRoutes) {
            Port* p = graph.portHead;
            while (p) {
                float ox, oy;
                if (getPortCoords(p->name, ox, oy)) {
                    Route* r = p->routeHead;
                    while (r) {
                        float dx, dy;
                        if (getPortCoords(r->destinationPort, dx, dy)) {
                            float length = sqrt((dx-ox)*(dx-ox) + (dy-oy)*(dy-oy));
                            float angle = atan2(dy-oy, dx-ox) * 180 / 3.14159f;

                            sf::RectangleShape routeLine;
                            routeLine.setSize(sf::Vector2f(length, 1));
                            routeLine.setPosition(ox, oy);
                            routeLine.setRotation(angle);
                            routeLine.setFillColor(sf::Color(100, 120, 150, 50));
                            window.draw(routeLine);
                        }
                        r = r->next;
                    }
                }
                p = p->next;
            }
        }

        if (state.showAllRoutes && state.hasResults && state.journeyListCount > 0) {

            for (int ji = 0; ji < state.journeyListCount; ji++) {
                const UIState::JourneyInfo& journey = state.journeyList[ji];
                if (!journey.valid || journey.legs == 0) continue;

                if (ji == state.selectedJourneyIndex) continue;

                string ports[10];
                int portCount = 0;
                ports[portCount++] = journey.schedule[0].fromPort;
                for (int leg = 0; leg < journey.legs && leg < 5; leg++) {
                    ports[portCount++] = journey.schedule[leg].toPort;
                }

                for (int i = 0; i < portCount - 1; i++) {
                    float x1, y1, x2, y2;
                    if (!getPortCoords(ports[i], x1, y1) || !getPortCoords(ports[i+1], x2, y2)) continue;

                    float length = sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
                    float angle = atan2(y2-y1, x2-x1) * 180 / 3.14159f;

                    float costRatio = (float)journey.cost / max(1, state.maxCost);
                    sf::Color routeColor;
                    routeColor.r = (sf::Uint8)(100 + costRatio * 120);
                    routeColor.g = (sf::Uint8)(150 - costRatio * 80);
                    routeColor.b = 120;
                    routeColor.a = 100;

                    sf::RectangleShape routeLine;
                    routeLine.setSize(sf::Vector2f(length, 4));
                    routeLine.setPosition(x1, y1 - 2);
                    routeLine.setRotation(angle);
                    routeLine.setFillColor(routeColor);
                    window.draw(routeLine);
                }
            }
        }

        if (state.animState == UIState::ANIM_EXPLORING ||
            (state.animState == UIState::ANIM_DRAWING_LINE && state.totalExploredEdges > 0) ||
            (state.animState == UIState::ANIM_SHIP_MOVING && state.totalExploredEdges > 0)) {

            int edgesToShow = state.explorationEdgesDrawn;
            float baseOpacity = 60.0f;

            if (state.animState != UIState::ANIM_EXPLORING) {
                edgesToShow = state.totalExploredEdges;
                baseOpacity = 25.0f * state.exploredFadeAlpha;
            }

            for (int i = 0; i < edgesToShow && i < state.totalExploredEdges; i++) {
                float x1, y1, x2, y2;
                if (getPortCoords(state.exploredEdges[i].fromPort, x1, y1) &&
                    getPortCoords(state.exploredEdges[i].toPort, x2, y2)) {

                    float length = sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
                    float angle = atan2(y2-y1, x2-x1) * 180 / 3.14159f;

                    float heatProgress = (float)i / (float)max(1, edgesToShow);

                    sf::Color exploreColor;
                    if (state.strategy == UI_DIJKSTRA_COST || state.strategy == UI_DIJKSTRA_TIME) {

                        sf::Uint8 r = (sf::Uint8)(60 + heatProgress * 40);
                        sf::Uint8 g = (sf::Uint8)(150 + heatProgress * 50);
                        sf::Uint8 b = (sf::Uint8)(255 - heatProgress * 55);
                        exploreColor = sf::Color(r, g, b, (sf::Uint8)baseOpacity);
                    } else {

                        sf::Uint8 r = (sf::Uint8)(150 + heatProgress * 50);
                        sf::Uint8 g = (sf::Uint8)(100 + heatProgress * 50);
                        sf::Uint8 b = (sf::Uint8)(255 - heatProgress * 55);
                        exploreColor = sf::Color(r, g, b, (sf::Uint8)baseOpacity);
                    }

                    if (state.animState == UIState::ANIM_EXPLORING) {

                        sf::RectangleShape exploreGlow;
                        exploreGlow.setSize(sf::Vector2f(length, 5));
                        exploreGlow.setPosition(x1, y1 - 2.5f);
                        exploreGlow.setRotation(angle);
                        sf::Color glowColor = exploreColor;
                        glowColor.a = (sf::Uint8)(baseOpacity * 0.5f);
                        exploreGlow.setFillColor(glowColor);
                        window.draw(exploreGlow);
                    }

                    sf::RectangleShape exploreLine;
                    exploreLine.setSize(sf::Vector2f(length, 3));
                    exploreLine.setPosition(x1, y1 - 1.5f);
                    exploreLine.setRotation(angle);
                    exploreLine.setFillColor(exploreColor);
                    window.draw(exploreLine);
                }
            }
        }

        bool shouldDrawRoute = (state.hasResults || (state.appState == AppState::MULTILEG_EDITOR && state.editorShowResults))
                       && state.journeyPortCount > 1
                       && state.animState != UIState::ANIM_EXPLORING;
        if (shouldDrawRoute) {

            if ((state.currentView == VIEW_MAIN_SEARCH && state.appState != AppState::MULTILEG_EDITOR) ||
                (state.appState == AppState::MULTILEG_EDITOR && state.editorShowResults)) {
                for (int i = 0; i < state.journeyPortCount - 1; i++) {
                    float x1, y1, x2, y2;
                    if (getPortCoords(state.journeyPorts[i], x1, y1) &&
                        getPortCoords(state.journeyPorts[i+1], x2, y2)) {

                        float length = sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
                        float angle = atan2(y2-y1, x2-x1) * 180 / 3.14159f;

                        sf::Color pathColor = hexToColor(0x00ff66FF);
                        pathColor.a = 160;

                        float pulse = 0.5f + 0.5f * sin(state.pulseTimer * 4.0f + i * 0.5f);

                        sf::RectangleShape pathGlow;
                        pathGlow.setSize(sf::Vector2f(length, 8));
                        pathGlow.setPosition(x1, y1 - 4);
                        pathGlow.setRotation(angle);
                        sf::Color glowColor = pathColor;
                        glowColor.a = (sf::Uint8)(30 + pulse * 50);
                        pathGlow.setFillColor(glowColor);
                        window.draw(pathGlow);

                        sf::RectangleShape pathLine;
                        pathLine.setSize(sf::Vector2f(length, 3.5f));
                        pathLine.setPosition(x1, y1 - 1.75f);
                        pathLine.setRotation(angle);
                        pathLine.setFillColor(pathColor);
                        window.draw(pathLine);
                    }
                }
            }

            unsigned int strategyPrimaryColor;
            unsigned int strategyGlowColor;
            float baseLineWidth = 2.5f;
            float glowIntensity = 1.2f;

            if (state.strategy == UI_DIJKSTRA_COST || state.strategy == UI_DIJKSTRA_TIME) {

                strategyPrimaryColor = Colors::SUCCESS;
                strategyGlowColor = 0x00FF88FF;
            } else if (state.strategy == UI_ASTAR_COST || state.strategy == UI_ASTAR_TIME) {

                strategyPrimaryColor = Colors::NEON_PURPLE;
                strategyGlowColor = 0xFF00FFFF;
            } else {

                strategyPrimaryColor = Colors::INFO;
                strategyGlowColor = Colors::HIGHLIGHT;
            }

            for (int i = 0; i < state.journeyPortCount - 1; i++) {
                float x1, y1, x2, y2;
                if (getPortCoords(state.journeyPorts[i], x1, y1) &&
                    getPortCoords(state.journeyPorts[i+1], x2, y2)) {

                    float mousePosX = (float)mousePos.x;
                    float mousePosY = (float)mousePos.y;

                    float edgeDx = x2 - x1;
                    float edgeDy = y2 - y1;
                    float edgeLength = sqrt(edgeDx*edgeDx + edgeDy*edgeDy);
                    float lineDist = abs(edgeDx * (y1 - mousePosY) - (x1 - mousePosX) * edgeDy) / edgeLength;

                    float t = ((mousePosX - x1) * edgeDx + (mousePosY - y1) * edgeDy) / (edgeLength * edgeLength);
                    bool withinSegment = (t >= 0 && t <= 1);

                    if (withinSegment && lineDist < 8.0f) {
                        state.hoveredEdge = true;
                        state.hoveredEdgeSource = state.journeyPorts[i];
                        state.hoveredEdgeDest = state.journeyPorts[i+1];
                        state.hoveredEdgeTooltipX = mousePosX + 15.0f;
                        state.hoveredEdgeTooltipY = mousePosY - 50.0f;

                        if (i < state.journeyScheduleCount) {
                            state.hoveredEdgeCompany = state.journeySchedule[i].company;
                            char dateStr[32];
                            snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%d", state.journeySchedule[i].depDay, state.journeySchedule[i].depMonth, state.journeySchedule[i].depYear);
                            state.hoveredEdgeDate = dateStr;

                            char timeStr[16];
                            snprintf(timeStr, sizeof(timeStr), "%02d:%02d", state.journeySchedule[i].depHour, state.journeySchedule[i].depMinute);
                            state.hoveredEdgeTime = timeStr;

                            state.hoveredEdgeCost = (float)state.journeySchedule[i].cost;
                        }
                    }

                    unsigned int legColor = Colors::LEG_COLORS[i % Colors::LEG_COLOR_COUNT];

                    float glow = 0.5f + 0.5f * sin(state.pulseTimer * 3.0f + i * 0.8f);

                    float length = sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
                    float angle = atan2(y2-y1, x2-x1) * 180 / 3.14159f;
                    float angleRad = atan2(y2-y1, x2-x1);

                    if (state.animState == UIState::ANIM_DRAWING_LINE) {

                        float totalLegs = (float)(state.journeyPortCount - 1);
                        float legStartProgress = (float)i / totalLegs;
                        float legEndProgress = (float)(i + 1) / totalLegs;

                        if (state.lineDrawProgress > legStartProgress) {

                            float legDrawProgress = min(1.0f, (state.lineDrawProgress - legStartProgress) / (legEndProgress - legStartProgress));
                            float drawnLength = length * legDrawProgress;

                            sf::RectangleShape outerGlow;
                            float outerGlowWidth = baseLineWidth + 8 + glow * 3;
                            outerGlow.setSize(sf::Vector2f(drawnLength, outerGlowWidth));
                            outerGlow.setPosition(x1, y1 - outerGlowWidth/2);
                            outerGlow.setRotation(angle);
                            sf::Color outerColor = hexToColor(strategyGlowColor);
                            outerColor.a = (sf::Uint8)(30 + glow * 20);
                            outerGlow.setFillColor(outerColor);
                            window.draw(outerGlow);

                            sf::RectangleShape innerGlow;
                            float innerGlowWidth = baseLineWidth + 4 + glow * 2;
                            innerGlow.setSize(sf::Vector2f(drawnLength, innerGlowWidth));
                            innerGlow.setPosition(x1, y1 - innerGlowWidth/2);
                            innerGlow.setRotation(angle);
                            sf::Color innerColor = hexToColor(strategyGlowColor);
                            innerColor.a = (sf::Uint8)(60 + glow * 40);
                            innerGlow.setFillColor(innerColor);
                            window.draw(innerGlow);

                            sf::RectangleShape line;
                            line.setSize(sf::Vector2f(drawnLength, baseLineWidth));
                            line.setPosition(x1, y1 - baseLineWidth/2);
                            line.setRotation(angle);
                            sf::Color primaryColor = hexToColor(strategyPrimaryColor);
                            primaryColor.a = 255;
                            line.setFillColor(primaryColor);
                            window.draw(line);
                        }
                        continue;
                    }

                    bool isCompleted = state.shipAnimationActive && (i < state.shipCurrentLeg);
                    bool isInProgress = state.shipAnimationActive && (i == state.shipCurrentLeg);
                    bool isFuture = state.shipAnimationActive && (i > state.shipCurrentLeg);
                    float highlightProgress = isInProgress ? state.shipProgress : (isCompleted ? 1.0f : 0.0f);

                    if (state.shipAnimationActive) {

                        if (highlightProgress > 0.0f) {
                            float traveledLength = length * highlightProgress;

                            float glowPulse = 0.7f + 0.3f * sin(state.pulseTimer * 4.0f);

                            sf::RectangleShape outerGlow;
                            outerGlow.setSize(sf::Vector2f(traveledLength, 8 + glowPulse * 2));
                            outerGlow.setPosition(x1, y1 - (4 + glowPulse));
                            outerGlow.setRotation(angle);
                            sf::Color glowColor = hexToColor(legColor);
                            glowColor.a = (sf::Uint8)(50 + glowPulse * 30);
                            outerGlow.setFillColor(glowColor);
                            window.draw(outerGlow);

                            sf::RectangleShape innerGlow;
                            innerGlow.setSize(sf::Vector2f(traveledLength, 5 + glowPulse));
                            innerGlow.setPosition(x1, y1 - (2.5f + glowPulse/2));
                            innerGlow.setRotation(angle);
                            sf::Color innerColor = hexToColor(legColor);
                            innerColor.a = (sf::Uint8)(100 + glowPulse * 50);
                            innerGlow.setFillColor(innerColor);
                            window.draw(innerGlow);

                            sf::RectangleShape line;
                            line.setSize(sf::Vector2f(traveledLength, 2.5f));
                            line.setPosition(x1, y1 - 1.25f);
                            line.setRotation(angle);

                            sf::Color brightColor = hexToColor(legColor);
                            brightColor.r = (sf::Uint8)min(255, brightColor.r + 80);
                            brightColor.g = (sf::Uint8)min(255, brightColor.g + 80);
                            brightColor.b = (sf::Uint8)min(255, brightColor.b + 80);
                            line.setFillColor(brightColor);
                            window.draw(line);
                        }

                        float futureStartX = x1;
                        float futureStartY = y1;
                        float futureLength = length;

                        if (isInProgress) {

                            futureLength = length * (1.0f - highlightProgress);
                            futureStartX = x1 + (x2 - x1) * highlightProgress;
                            futureStartY = y1 + (y2 - y1) * highlightProgress;
                        } else if (isFuture) {

                            futureLength = length;
                            futureStartX = x1;
                            futureStartY = y1;
                        }

                        if (isInProgress || isFuture) {

                            sf::RectangleShape dimGlow;
                            dimGlow.setSize(sf::Vector2f(futureLength, 5));
                            dimGlow.setPosition(futureStartX, futureStartY - 2.5f);
                            dimGlow.setRotation(angle);
                            sf::Color glowColor = hexToColor(legColor);
                            glowColor.a = 30;
                            dimGlow.setFillColor(glowColor);
                            window.draw(dimGlow);

                            sf::RectangleShape line;
                            line.setSize(sf::Vector2f(futureLength, 2.0f));
                            line.setPosition(futureStartX, futureStartY - 1.0f);
                            line.setRotation(angle);

                            sf::Color dimmedColor = hexToColor(legColor);
                            dimmedColor.a = 60;
                            line.setFillColor(dimmedColor);
                            window.draw(line);
                        }
                    } else {

                        sf::Color primaryColor = hexToColor(strategyPrimaryColor);
                        sf::Color glowColor = hexToColor(strategyGlowColor);

                        sf::RectangleShape outerGlow;
                        float outerWidth = baseLineWidth + 8 + glow * 3;
                        outerGlow.setSize(sf::Vector2f(length, outerWidth));
                        outerGlow.setPosition(x1, y1 - outerWidth/2);
                        outerGlow.setRotation(angle);
                        glowColor.a = (sf::Uint8)(30 + glow * 20);
                        outerGlow.setFillColor(glowColor);
                        window.draw(outerGlow);

                        sf::RectangleShape innerGlow;
                        float innerWidth = baseLineWidth + 4 + glow * 2;
                        innerGlow.setSize(sf::Vector2f(length, innerWidth));
                        innerGlow.setPosition(x1, y1 - innerWidth/2);
                        innerGlow.setRotation(angle);
                        glowColor.a = (sf::Uint8)(60 + glow * 40);
                        innerGlow.setFillColor(glowColor);
                        window.draw(innerGlow);

                        sf::RectangleShape line;
                        line.setSize(sf::Vector2f(length, baseLineWidth));
                        line.setPosition(x1, y1 - baseLineWidth/2);
                        line.setRotation(angle);
                        primaryColor.a = 255;
                        line.setFillColor(primaryColor);
                        window.draw(line);
                    }

                    float mx = (x1 + x2) / 2;
                    float my = (y1 + y2) / 2;

                    float arrowX = x1 + (x2 - x1) * 0.8f;
                    float arrowY = y1 + (y2 - y1) * 0.8f;
                    float arrowSize = 16.0f;

                    float pdx = -(y2 - y1) / length;
                    float pdy = (x2 - x1) / length;
                    float ndx = (x2 - x1) / length;
                    float ndy = (y2 - y1) / length;

                    sf::ConvexShape arrow(3);
                    arrow.setPoint(0, sf::Vector2f(arrowX, arrowY));
                    arrow.setPoint(1, sf::Vector2f(arrowX - ndx * arrowSize + pdx * arrowSize / 2,
                                                   arrowY - ndy * arrowSize + pdy * arrowSize / 2));
                    arrow.setPoint(2, sf::Vector2f(arrowX - ndx * arrowSize - pdx * arrowSize / 2,
                                                   arrowY - ndy * arrowSize - pdy * arrowSize / 2));
                    arrow.setFillColor(sf::Color(255, 255, 255, 220));
                    arrow.setOutlineColor(hexToColor(legColor));
                    arrow.setOutlineThickness(2);
                    window.draw(arrow);

                    sf::CircleShape markerBg(20);
                    markerBg.setPosition(mx - 20, my - 20);
                    markerBg.setFillColor(sf::Color(0, 0, 0, 220));
                    window.draw(markerBg);

                    sf::CircleShape markerGlow(18);
                    markerGlow.setPosition(mx - 18, my - 18);
                    sf::Color mgColor = hexToColor(legColor);
                    mgColor.a = (sf::Uint8)(150 + glow * 50);
                    markerGlow.setFillColor(mgColor);
                    window.draw(markerGlow);

                    sf::CircleShape marker(14);
                    marker.setPosition(mx - 14, my - 14);
                    marker.setFillColor(hexToColor(legColor));
                    marker.setOutlineColor(sf::Color::White);
                    marker.setOutlineThickness(3);
                    window.draw(marker);

                    sf::Text legNumShadow(to_string(i+1), font, 18);
                    sf::FloatRect lnbs = legNumShadow.getLocalBounds();
                    legNumShadow.setPosition(mx - lnbs.width/2 + 1, my - 10 + 1);
                    legNumShadow.setFillColor(sf::Color::Black);
                    legNumShadow.setStyle(sf::Text::Bold);
                    window.draw(legNumShadow);

                    sf::Text legNum(to_string(i+1), font, 18);
                    legNum.setPosition(mx - lnbs.width/2, my - 10);
                    legNum.setFillColor(sf::Color::White);
                    legNum.setStyle(sf::Text::Bold);
                    window.draw(legNum);
                }
            }

            if (state.shipAnimationActive) {
                // Draw glow effect under ship
                float shipGlow = 0.6f + 0.4f * sin(state.pulseTimer * 4.0f);
                sf::CircleShape shipGlowCircle(18);
                shipGlowCircle.setPosition(state.shipX - 18, state.shipY - 18);
                sf::Color glowColor(100, 200, 255, (sf::Uint8)(50 * shipGlow));
                shipGlowCircle.setFillColor(glowColor);
                window.draw(shipGlowCircle);

                // Draw wake/trail behind ship
                float trailLength = 20.0f;
                float trailAngleRad = (state.shipAngle + 180.0f) * 3.14159f / 180.0f;
                float trailX = state.shipX + cos(trailAngleRad) * trailLength;
                float trailY = state.shipY + sin(trailAngleRad) * trailLength;

                sf::Vertex trail[] = {
                    sf::Vertex(sf::Vector2f(state.shipX, state.shipY), sf::Color(150, 220, 255, 120)), 
                    sf::Vertex(sf::Vector2f(trailX, trailY), sf::Color(150, 220, 255, 0))
                };
                window.draw(trail, 2, sf::Lines);

                // Draw the sprite-based ship using ShipAnimator
                gShipAnimator.draw(window);
            }

            {
                float ox, oy;
                if (getPortCoords(state.journeyPorts[0], ox, oy)) {
                    sf::Text originLabel("START: " + state.journeyPorts[0], font, 14);
                    sf::FloatRect ob = originLabel.getLocalBounds();

                    sf::RectangleShape shadowBg(sf::Vector2f(ob.width + 24, 30));
                    shadowBg.setPosition(ox - ob.width/2 - 10, oy + 22);
                    shadowBg.setFillColor(sf::Color(0, 0, 0, 200));
                    window.draw(shadowBg);

                    float pulse = 0.8f + 0.2f * sin(state.pulseTimer * 3);
                    sf::RectangleShape originBg(sf::Vector2f(ob.width + 20, 26));
                    originBg.setPosition(ox - ob.width/2 - 8, oy + 24);
                    originBg.setFillColor(sf::Color(0, (sf::Uint8)(220 * pulse), (sf::Uint8)(80 * pulse), 255));
                    originBg.setOutlineColor(sf::Color::White);
                    originBg.setOutlineThickness(3);
                    window.draw(originBg);

                    originLabel.setPosition(ox - ob.width/2, oy + 27);
                    originLabel.setFillColor(sf::Color::White);
                    originLabel.setStyle(sf::Text::Bold);
                    window.draw(originLabel);
                }
            }

            {
                float dx, dy;
                if (getPortCoords(state.journeyPorts[state.journeyPortCount - 1], dx, dy)) {
                    sf::Text destLabel("END: " + state.journeyPorts[state.journeyPortCount - 1], font, 14);
                    sf::FloatRect db = destLabel.getLocalBounds();

                    sf::RectangleShape shadowBg(sf::Vector2f(db.width + 24, 30));
                    shadowBg.setPosition(dx - db.width/2 - 10, dy + 22);
                    shadowBg.setFillColor(sf::Color(0, 0, 0, 200));
                    window.draw(shadowBg);

                    float pulse = 0.8f + 0.2f * sin(state.pulseTimer * 3 + 1.5f);
                    sf::RectangleShape destBg(sf::Vector2f(db.width + 20, 26));
                    destBg.setPosition(dx - db.width/2 - 8, dy + 24);
                    destBg.setFillColor(sf::Color((sf::Uint8)(255 * pulse), 50, (sf::Uint8)(80 * pulse), 255));
                    destBg.setOutlineColor(sf::Color::White);
                    destBg.setOutlineThickness(3);
                    window.draw(destBg);

                    destLabel.setPosition(dx - db.width/2, dy + 27);
                    destLabel.setFillColor(sf::Color::White);
                    destLabel.setStyle(sf::Text::Bold);
                    window.draw(destLabel);
                }
            }
        }

        }

        float legendX = MAP_X + 15;
        float legendY = 15;

        if (state.currentView == VIEW_MAIN_SEARCH) {

        sf::RectangleShape legendBg(sf::Vector2f(160, 90));
        legendBg.setPosition(legendX, legendY);
        legendBg.setFillColor(sf::Color(10, 15, 30, 220));
        legendBg.setOutlineColor(sf::Color(0, 180, 255, 100));
        legendBg.setOutlineThickness(1);
        window.draw(legendBg);

        drawText(window, font, "LEGEND", legendX + 10, legendY + 5, 11, Colors::HIGHLIGHT, true);

        sf::CircleShape legOrigin(6);
        legOrigin.setPosition(legendX + 15, legendY + 28);
        legOrigin.setFillColor(hexToColor(Colors::SUCCESS));
        window.draw(legOrigin);
        drawText(window, font, "Origin", legendX + 35, legendY + 26, 10, Colors::SUCCESS);

        sf::CircleShape legDest(6);
        legDest.setPosition(legendX + 95, legendY + 28);
        legDest.setFillColor(hexToColor(Colors::SECONDARY));
        window.draw(legDest);
        drawText(window, font, "Dest", legendX + 115, legendY + 26, 10, Colors::SECONDARY);

        sf::RectangleShape legRoute(sf::Vector2f(30, 4));
        legRoute.setPosition(legendX + 15, legendY + 52);
        legRoute.setFillColor(hexToColor(Colors::JOURNEY_COLOR));
        window.draw(legRoute);
        drawText(window, font, "Route Path", legendX + 55, legendY + 48, 10, Colors::JOURNEY_COLOR);

        drawText(window, font, "Click = Origin | Shift = Dest", legendX + 10, legendY + 70, 9, Colors::TEXT_MUTED);
        } else if (state.currentView == VIEW_COMPANY_ROUTES) {

            sf::RectangleShape legendBg(sf::Vector2f(200, state.selectedCompanyIndex >= 0 ? 70 : 100));
            legendBg.setPosition(legendX, legendY);
            legendBg.setFillColor(sf::Color(10, 15, 30, 220));
            legendBg.setOutlineColor(sf::Color(170, 100, 255, 100));
            legendBg.setOutlineThickness(1);
            window.draw(legendBg);

            drawText(window, font, "COMPANY ROUTES", legendX + 10, legendY + 5, 11, Colors::NEON_PURPLE, true);

            if (state.selectedCompanyIndex >= 0) {

                string compName = state.companyList[state.selectedCompanyIndex];
                if (compName.length() > 22) compName = compName.substr(0, 19) + "...";
                drawText(window, font, "Company:", legendX + 10, legendY + 28, 10, Colors::TEXT_SECONDARY);
                drawText(window, font, compName, legendX + 10, legendY + 45, 11, Colors::NEON_PURPLE, true);
            } else {

                drawText(window, font, "Showing all companies", legendX + 10, legendY + 28, 10, Colors::TEXT_SECONDARY);
                drawText(window, font, ("Total: " + to_string(state.companyCount)), legendX + 10, legendY + 48, 11, Colors::INFO);
                drawText(window, font, "Each company = different color", legendX + 10, legendY + 68, 9, Colors::TEXT_MUTED);
            }
        }

        state.hoveredPort = -1;
        for (int i = 0; i < PORT_COUNT; i++) {

            float x, y;
            if (!getPortCoords(PORT_POSITIONS[i].name, x, y)) {
                continue;
            }

            bool inJourney = false;
            int journeyPos = -1;
            for (int j = 0; j < state.journeyPortCount; j++) {
                if (state.journeyPorts[j] == PORT_POSITIONS[i].name) {
                    inJourney = true;
                    journeyPos = j;
                    break;
                }
            }

            bool hover = (mousePos.x >= x - 15 && mousePos.x <= x + 15 &&
                         mousePos.y >= y - 15 && mousePos.y <= y + 15);

            if (hover) state.hoveredPort = i;

            unsigned int color = Colors::PORT_COLOR;
            float radius = 8;

            bool isOrigin = (state.currentView == VIEW_MAIN_SEARCH) && (state.appState != AppState::MULTILEG_EDITOR) && (PORT_POSITIONS[i].name == state.originPort);
            bool isDest = (state.currentView == VIEW_MAIN_SEARCH) && (state.appState != AppState::MULTILEG_EDITOR) && (PORT_POSITIONS[i].name == state.destPort);

            if (isOrigin) {
                color = 0x00ff66FF;
                radius = 18;
            } else if (isDest) {
                color = 0xff3366FF;
                radius = 18;
            } else if (inJourney) {

                color = Colors::LEG_COLORS[journeyPos % Colors::LEG_COLOR_COUNT];
                radius = 13;
            } else if (hover) {
                color = 0xffff00FF;
                radius = 11;
            }

            if (isOrigin || isDest) {

                float pulse1 = 0.5f + 0.5f * sin(state.pulseTimer * 3.0f);
                float pulse2 = 0.5f + 0.5f * sin(state.pulseTimer * 3.0f + 1.0f);

                sf::CircleShape glow1(radius + 10 + pulse1 * 5);
                glow1.setPosition(x - radius - 10 - pulse1 * 2.5f, y - radius - 10 - pulse1 * 2.5f);
                sf::Color g1Color = hexToColor(color);
                g1Color.a = (sf::Uint8)(20 + pulse1 * 30);
                glow1.setFillColor(g1Color);
                window.draw(glow1);

                sf::CircleShape glow2(radius + 6 + pulse2 * 3);
                glow2.setPosition(x - radius - 6 - pulse2 * 1.5f, y - radius - 6 - pulse2 * 1.5f);
                sf::Color g2Color = hexToColor(color);
                g2Color.a = (sf::Uint8)(40 + pulse2 * 40);
                glow2.setFillColor(g2Color);
                window.draw(glow2);
            } else if (inJourney || hover) {
                float glowPulse = 0.5f + 0.5f * sin(state.pulseTimer * 2.0f);
                sf::CircleShape glow(radius + 5 + glowPulse * 3);
                glow.setPosition(x - radius - 5 - glowPulse * 1.5f, y - radius - 5 - glowPulse * 1.5f);
                sf::Color glowColor = hexToColor(color);
                glowColor.a = (sf::Uint8)(50 + glowPulse * 40);
                glow.setFillColor(glowColor);
                window.draw(glow);
            }

            sf::CircleShape portBlackOutline(radius + 4);
            portBlackOutline.setPosition(x - radius - 4, y - radius - 4);
            portBlackOutline.setFillColor(sf::Color(0, 0, 0, 200));
            window.draw(portBlackOutline);

            sf::CircleShape portOuter(radius);
            portOuter.setPosition(x - radius, y - radius);
            portOuter.setFillColor(hexToColor(color));
            portOuter.setOutlineColor(sf::Color::White);
            portOuter.setOutlineThickness(3);
            window.draw(portOuter);

            sf::CircleShape portInner(radius * 0.4f);
            portInner.setPosition(x - radius * 0.25f, y - radius * 0.4f);
            portInner.setFillColor(sf::Color(255, 255, 255, 150));
            window.draw(portInner);

            if (hover || isOrigin || isDest || inJourney) {

                sf::Text label(PORT_POSITIONS[i].name, font, 12);
                sf::FloatRect labelBounds = label.getLocalBounds();

                unsigned int labelBgColor = isOrigin ? 0x00aa55ee :
                                           isDest ? 0xcc4466ee :
                                           inJourney ? 0x4488ccee : 0x6644aaee;

                if (hover) {

                    float tooltipW = 180;
                    float tooltipH = 70;
                    float tooltipX = x + radius + 10;
                    float tooltipY = y - 35;

                    if (tooltipX + tooltipW > MAP_X + MAP_WIDTH - 10) {
                        tooltipX = x - tooltipW - radius - 10;
                    }

                    sf::RectangleShape shadow(sf::Vector2f(tooltipW, tooltipH));
                    shadow.setPosition(tooltipX + 4, tooltipY + 4);
                    shadow.setFillColor(sf::Color(0, 0, 0, 150));
                    window.draw(shadow);

                    sf::RectangleShape tooltip(sf::Vector2f(tooltipW, tooltipH));
                    tooltip.setPosition(tooltipX, tooltipY);
                    tooltip.setFillColor(sf::Color(15, 20, 40, 250));
                    tooltip.setOutlineColor(hexToColor(color));
                    tooltip.setOutlineThickness(2);
                    window.draw(tooltip);

                    sf::Text portName(PORT_POSITIONS[i].name, font, 14);
                    portName.setPosition(tooltipX + 10, tooltipY + 8);
                    portName.setFillColor(hexToColor(color));
                    portName.setStyle(sf::Text::Bold);
                    window.draw(portName);

                    drawText(window, font, "Click to set as ORIGIN", tooltipX + 10, tooltipY + 32, 10, Colors::SUCCESS);
                    drawText(window, font, "Shift+Click for DESTINATION", tooltipX + 10, tooltipY + 48, 10, Colors::SECONDARY);
                } else {

                    sf::RectangleShape labelBg(sf::Vector2f(labelBounds.width + 16, 22));
                    labelBg.setPosition(x + radius + 5, y - 11);
                    labelBg.setFillColor(hexToColor(labelBgColor));
                window.draw(labelBg);

                    label.setPosition(x + radius + 13, y - 9);
                label.setFillColor(sf::Color::White);
                    label.setStyle(sf::Text::Bold);
                window.draw(label);
                }

                if (isOrigin) {
                    sf::RectangleShape badge(sf::Vector2f(50, 16));
                    badge.setPosition(x - 25, y - radius - 22);
                    badge.setFillColor(hexToColor(Colors::SUCCESS));
                    window.draw(badge);
                    drawText(window, font, "START", x - 20, y - radius - 20, 10, Colors::DARK_BG, true);
                } else if (isDest) {
                    sf::RectangleShape badge(sf::Vector2f(40, 16));
                    badge.setPosition(x - 20, y - radius - 22);
                    badge.setFillColor(hexToColor(Colors::SECONDARY));
                    window.draw(badge);
                    drawText(window, font, "END", x - 12, y - radius - 20, 10, Colors::DARK_BG, true);
                }
            }

            if (hover && clicked && !state.originDropdownOpen && !state.destDropdownOpen) {

                if (sf::Keyboard::isKeyPressed(sf::Keyboard::LShift)) {
                    state.destPort = PORT_POSITIONS[i].name;
                    for (int pi = 0; pi < portCount; pi++) {
                        if (portNames[pi] == state.destPort) state.destIndex = pi;
                    }
                } else {
                    state.originPort = PORT_POSITIONS[i].name;
                    for (int pi = 0; pi < portCount; pi++) {
                        if (portNames[pi] == state.originPort) state.originIndex = pi;
                    }
                }
            }
        }

        if (state.appState == AppState::MAIN_MENU || state.appState == AppState::ROUTE_PLANNER || state.appState == AppState::COMPANY_VIEWER) {
            if (state.currentView == VIEW_MAIN_SEARCH) {

                sf::RectangleShape leftBg(sf::Vector2f((float)LEFT_SIDEBAR_WIDTH, (float)WINDOW_HEIGHT));
                leftBg.setPosition(0, 0);
                leftBg.setFillColor(sf::Color(12, 15, 25, 255));
                window.draw(leftBg);

                sf::RectangleShape titleBar(sf::Vector2f((float)LEFT_SIDEBAR_WIDTH, 45));
                titleBar.setPosition(0, 0);
                titleBar.setFillColor(sf::Color(20, 25, 40, 255));
                window.draw(titleBar);

                sf::Text panelTitle("ROUTE PLANNER", font, 13);
                panelTitle.setStyle(sf::Text::Bold);
                panelTitle.setPosition(15, 14);
                panelTitle.setFillColor(hexToColor(Colors::TEXT_PRIMARY));
                panelTitle.setLetterSpacing(1.3f);
                window.draw(panelTitle);

                float ly = 55.0f;
                float cardMargin = 12.0f;
                float cardWidth = LEFT_SIDEBAR_WIDTH - cardMargin * 2;

                float card1H = 180.0f;
                drawCard(window, cardMargin, ly, cardWidth, card1H, Colors::HIGHLIGHT);
                drawSectionHeader(window, font, "PORTS", cardMargin + 10, ly + 8, cardWidth - 20, Colors::SUCCESS);

                float cy = ly + 35;

                drawText(window, font, "Origin", cardMargin + 15, cy, 9, Colors::TEXT_MUTED);
                cy += 16;

                bool originHover = mousePos.x >= cardMargin + 10 && mousePos.x <= cardMargin + cardWidth - 10 &&
                                  mousePos.y >= cy && mousePos.y <= cy + 34;

                drawInputField(window, font, state.originPort, cardMargin + 10, cy, cardWidth - 20, 34,
                              state.originDropdownOpen, originHover);

                if (originHover && clicked && !state.destDropdownOpen && !state.preferencesDropdownOpen && !state.avoidPortsDropdownOpen) {
                    state.originDropdownOpen = !state.originDropdownOpen;
                    state.dropdownScroll = 0;
                }
                cy += 42;

                drawText(window, font, "Destination", cardMargin + 15, cy, 9, Colors::TEXT_MUTED);
                cy += 16;

                bool destHover = mousePos.x >= cardMargin + 10 && mousePos.x <= cardMargin + cardWidth - 10 &&
                                mousePos.y >= cy && mousePos.y <= cy + 34;

                drawInputField(window, font, state.destPort, cardMargin + 10, cy, cardWidth - 20, 34,
                              state.destDropdownOpen, destHover);

                if (destHover && clicked && !state.originDropdownOpen && !state.preferencesDropdownOpen && !state.avoidPortsDropdownOpen) {
                    state.destDropdownOpen = !state.destDropdownOpen;
                    state.dropdownScroll = 0;
                }

                drawText(window, font, "💡 Click ports on map to select", cardMargin + 15, ly + card1H - 20, 8, Colors::TEXT_MUTED);

                ly += card1H + 10;

                float card2H = 90.0f;
                drawCard(window, cardMargin, ly, cardWidth, card2H, Colors::SECONDARY);
                drawSectionHeader(window, font, "TRAVEL DATE", cardMargin + 10, ly + 8, cardWidth - 20, Colors::SECONDARY);

                cy = ly + 35;

                float dateFieldW = (cardWidth - 40) / 3 - 4;

                drawText(window, font, "Day", cardMargin + 15, cy, 8, Colors::TEXT_MUTED);
                drawText(window, font, "Month", cardMargin + 25 + dateFieldW, cy, 8, Colors::TEXT_MUTED);
                drawText(window, font, "Year", cardMargin + 35 + dateFieldW * 2, cy, 8, Colors::TEXT_MUTED);
                cy += 14;

                string dayStr = (state.activeField == UIState::DAY) ? state.inputBuffer + "|" : to_string(state.day);
                bool dayHover = mousePos.x >= cardMargin + 10 && mousePos.x <= cardMargin + 10 + dateFieldW &&
                               mousePos.y >= cy && mousePos.y <= cy + 28;
                drawInputField(window, font, dayStr, cardMargin + 10, cy, dateFieldW, 28,
                              state.activeField == UIState::DAY, dayHover);
                if (dayHover && clicked) {
                    state.activeField = UIState::DAY;
                    state.inputBuffer = "";
                }

                string monthStr = (state.activeField == UIState::MONTH) ? state.inputBuffer + "|" : to_string(state.month);
                bool monthHover = mousePos.x >= cardMargin + 20 + dateFieldW && mousePos.x <= cardMargin + 20 + dateFieldW * 2 &&
                                 mousePos.y >= cy && mousePos.y <= cy + 28;
                drawInputField(window, font, monthStr, cardMargin + 20 + dateFieldW, cy, dateFieldW, 28,
                              state.activeField == UIState::MONTH, monthHover);
                if (monthHover && clicked) {
                    state.activeField = UIState::MONTH;
                    state.inputBuffer = "";
                }

                string yearStr = (state.activeField == UIState::YEAR) ? state.inputBuffer + "|" : to_string(state.year);
                bool yearHover = mousePos.x >= cardMargin + 30 + dateFieldW * 2 && mousePos.x <= cardMargin + cardWidth - 10 &&
                                mousePos.y >= cy && mousePos.y <= cy + 28;
                drawInputField(window, font, yearStr, cardMargin + 30 + dateFieldW * 2, cy, dateFieldW, 28,
                              state.activeField == UIState::YEAR, yearHover);
                if (yearHover && clicked) {
                    state.activeField = UIState::YEAR;
                    state.inputBuffer = "";
                }

                ly += card2H + 10;

                float card3H = 95.0f;
                drawCard(window, cardMargin, ly, cardWidth, card3H, Colors::WARNING);
                drawSectionHeader(window, font, "PREFERENCES", cardMargin + 10, ly + 8, cardWidth - 20, Colors::WARNING);

                cy = ly + 35;

                drawText(window, font, "Max Cost", cardMargin + 15, cy, 9, Colors::TEXT_MUTED);
                cy += 16;
                string costStr = (state.activeField == UIState::MAX_COST) ? state.inputBuffer + "|" : to_string(state.maxCost);
                bool costHover = mousePos.x >= cardMargin + 10 && mousePos.x <= cardMargin + 10 + (cardWidth - 25) / 2 &&
                                mousePos.y >= cy && mousePos.y <= cy + 28;
                drawInputField(window, font, costStr, cardMargin + 10, cy, (cardWidth - 25) / 2, 28,
                              state.activeField == UIState::MAX_COST, costHover);
                if (costHover && clicked) {
                    state.activeField = UIState::MAX_COST;
                    state.inputBuffer = "";
                }

                drawText(window, font, "Max Legs", cardMargin + 25 + (cardWidth - 25) / 2, cy - 16, 9, Colors::TEXT_MUTED);
                string legsStr = (state.activeField == UIState::MAX_LEGS) ? state.inputBuffer + "|" : to_string(state.maxLegs);
                bool legsHover = mousePos.x >= cardMargin + 20 + (cardWidth - 25) / 2 && mousePos.x <= cardMargin + cardWidth - 10 &&
                                mousePos.y >= cy && mousePos.y <= cy + 28;
                drawInputField(window, font, legsStr, cardMargin + 20 + (cardWidth - 25) / 2, cy, (cardWidth - 25) / 2, 28,
                              state.activeField == UIState::MAX_LEGS, legsHover);
                if (legsHover && clicked) {
                    state.activeField = UIState::MAX_LEGS;
                    state.inputBuffer = "";
                }

                ly += card3H + 10;

                float cardPrefH = state.preferencesEnabled ? 200.0f : 80.0f;
                drawCard(window, cardMargin, ly, cardWidth, cardPrefH, Colors::NEON_PURPLE);
                drawSectionHeader(window, font, "CUSTOM PREFERENCES", cardMargin + 10, ly + 8, cardWidth - 20, Colors::NEON_PURPLE);

                cy = ly + 35;

                bool enableHover = mousePos.x >= cardMargin + 10 && mousePos.x <= cardMargin + cardWidth - 10 &&
                                  mousePos.y >= cy && mousePos.y <= cy + 20;
                
                sf::RectangleShape checkbox(sf::Vector2f(16, 16));
                checkbox.setPosition(cardMargin + 15, cy + 2);
                checkbox.setFillColor(state.preferencesEnabled ? hexToColor(Colors::SUCCESS) : hexToColor(Colors::INPUT_BG));
                checkbox.setOutlineColor(hexToColor(Colors::INPUT_BORDER));
                checkbox.setOutlineThickness(1.5f);
                window.draw(checkbox);

                if (state.preferencesEnabled) {
                    sf::Text checkmark("X", font, 12);
                    checkmark.setPosition(cardMargin + 18, cy + 2);
                    checkmark.setFillColor(hexToColor(Colors::DARK_BG));
                    checkmark.setStyle(sf::Text::Bold);
                    window.draw(checkmark);
                }

                drawText(window, font, "Enable Custom Filters", cardMargin + 40, cy + 4, 10, Colors::TEXT_PRIMARY);

                if (enableHover && clicked) {
                    state.preferencesEnabled = !state.preferencesEnabled;
                }

                cy += 25;

                if (state.preferencesEnabled) {
                    drawText(window, font, "Preferred Companies:", cardMargin + 15, cy, 9, Colors::TEXT_MUTED);
                    cy += 14;
                    
                    string companiesText = "";
                    if (state.preferredCompaniesCount == 0) {
                        companiesText = "All allowed";
                    } else {
                        for (int i = 0; i < state.preferredCompaniesCount; i++) {
                            companiesText += state.preferredCompanies[i];
                            if (i < state.preferredCompaniesCount - 1) companiesText += ", ";
                        }
                    }
                    
                    bool compHover = mousePos.x >= cardMargin + 10 && mousePos.x <= cardMargin + cardWidth - 10 &&
                                    mousePos.y >= cy && mousePos.y <= cy + 26;
                    drawInputField(window, font, companiesText, cardMargin + 10, cy, cardWidth - 20, 26,
                                  state.preferencesDropdownOpen, compHover);
                    
                    if (compHover && clicked && !state.originDropdownOpen && !state.destDropdownOpen && !state.avoidPortsDropdownOpen) {
                        state.preferencesDropdownOpen = !state.preferencesDropdownOpen;
                        if (state.preferencesDropdownOpen) state.preferencesScrollOffset = 0;
                    }
                    
                    cy += 32;

                    drawText(window, font, "Avoid Ports:", cardMargin + 15, cy, 9, Colors::TEXT_MUTED);
                    cy += 14;
                    
                    string portsText = "";
                    if (state.avoidedPortsCount == 0) {
                        portsText = "None";
                    } else {
                        for (int i = 0; i < state.avoidedPortsCount; i++) {
                            portsText += state.avoidedPorts[i];
                            if (i < state.avoidedPortsCount - 1) portsText += ", ";
                        }
                    }
                    
                    bool portHover = mousePos.x >= cardMargin + 10 && mousePos.x <= cardMargin + cardWidth - 10 &&
                                    mousePos.y >= cy && mousePos.y <= cy + 26;
                    drawInputField(window, font, portsText, cardMargin + 10, cy, cardWidth - 20, 26,
                                  state.avoidPortsDropdownOpen, portHover);
                    
                    if (portHover && clicked && !state.originDropdownOpen && !state.destDropdownOpen && !state.preferencesDropdownOpen) {
                        state.avoidPortsDropdownOpen = !state.avoidPortsDropdownOpen;
                        if (state.avoidPortsDropdownOpen) state.avoidPortsScrollOffset = 0;
                    }
                    
                    cy += 32;

                    bool timeCheckHover = mousePos.x >= cardMargin + 10 && mousePos.x <= cardMargin + 150 &&
                                         mousePos.y >= cy && mousePos.y <= cy + 20;
                    
                    sf::RectangleShape timeCheckbox(sf::Vector2f(16, 16));
                    timeCheckbox.setPosition(cardMargin + 15, cy + 2);
                    timeCheckbox.setFillColor(state.useMaxVoyageTime ? hexToColor(Colors::SUCCESS) : hexToColor(Colors::INPUT_BG));
                    timeCheckbox.setOutlineColor(hexToColor(Colors::INPUT_BORDER));
                    timeCheckbox.setOutlineThickness(1.5f);
                    window.draw(timeCheckbox);

                    if (state.useMaxVoyageTime) {
                        sf::Text checkmark("X", font, 12);
                        checkmark.setPosition(cardMargin + 18, cy + 2);
                        checkmark.setFillColor(hexToColor(Colors::DARK_BG));
                        checkmark.setStyle(sf::Text::Bold);
                        window.draw(checkmark);
                    }

                    drawText(window, font, "Max Voyage Time", cardMargin + 40, cy + 4, 9, Colors::TEXT_SECONDARY);

                    if (timeCheckHover && clicked) {
                        state.useMaxVoyageTime = !state.useMaxVoyageTime;
                    }

                    string timeStr = to_string(state.maxVoyageTimeHours) + "h";
                    drawText(window, font, timeStr, cardMargin + cardWidth - 50, cy + 4, 10, Colors::TEXT_PRIMARY, true);

                    cy += 24;
                    bool clearHover = mousePos.x >= cardMargin + 10 && mousePos.x <= cardMargin + cardWidth - 10 &&
                                     mousePos.y >= cy && mousePos.y <= cy + 24;
                    
                    sf::RectangleShape clearBtn(sf::Vector2f(cardWidth - 20, 24));
                    clearBtn.setPosition(cardMargin + 10, cy);
                    clearBtn.setFillColor(clearHover ? hexToColor(Colors::DANGER) : hexToColor(0x3a1a1aFF));
                    clearBtn.setOutlineColor(hexToColor(Colors::DANGER));
                    clearBtn.setOutlineThickness(1.0f);
                    window.draw(clearBtn);

                    drawText(window, font, "Clear All Filters", cardMargin + cardWidth / 2 - 45, cy + 6, 9, clearHover ? Colors::TEXT_PRIMARY : Colors::TEXT_MUTED);

                    if (clearHover && clicked) {
                        state.preferredCompaniesCount = 0;
                        state.avoidedPortsCount = 0;
                        state.useMaxVoyageTime = false;
                    }
                }

                ly += cardPrefH + 10;

                float card4H = 150.0f;
                drawCard(window, cardMargin, ly, cardWidth, card4H, Colors::ELECTRIC_BLUE);
                drawSectionHeader(window, font, "ALGORITHM", cardMargin + 10, ly + 8, cardWidth - 20, Colors::ELECTRIC_BLUE);

                cy = ly + 35;

                float btnW = (cardWidth - 35) / 2;
                float btnH = 30;

                const char* stratLabels[] = {"Dijkstra (Cost)", "Dijkstra (Time)", "A* (Cost)", "A* (Time)", "Safest Route"};
                UIStrategy stratVals[] = {UI_DIJKSTRA_COST, UI_DIJKSTRA_TIME, UI_ASTAR_COST, UI_ASTAR_TIME, UI_SAFEST};

                for (int i = 0; i < 5; i++) {
                    float bx, by;
                    float currentBtnW = btnW;

                    if (i < 4) {

                        bx = cardMargin + 10 + (i % 2) * (btnW + 5);
                        by = cy + (i / 2) * (btnH + 5);
                    } else {

                        bx = cardMargin + 10;
                        by = cy + 70;
                        currentBtnW = cardWidth - 20;
                    }

                    bool selected = (state.strategy == stratVals[i]);
                    bool hovered = mousePos.x >= bx && mousePos.x <= bx + currentBtnW &&
                                  mousePos.y >= by && mousePos.y <= by + btnH;

                    unsigned int fillColor = selected ? Colors::HIGHLIGHT : (i == 4 ? 0x2a4a2aFF : 0x1a1a3aFF);
                    unsigned int borderColor = selected ? Colors::ELECTRIC_BLUE : (i == 4 ? Colors::SUCCESS : Colors::INPUT_BORDER);

                    sf::RectangleShape btn(sf::Vector2f(currentBtnW, btnH));
                    btn.setPosition(bx, by);
                    btn.setFillColor(hovered && !selected ? sf::Color(40, 50, 80) : hexToColor(fillColor));
                    btn.setOutlineColor(hexToColor(borderColor));
                    btn.setOutlineThickness(selected ? 2.0f : 1.0f);
                    window.draw(btn);

                    sf::Text btnTxt(stratLabels[i], font, 9);
                    btnTxt.setStyle(selected ? sf::Text::Bold : sf::Text::Regular);
                    sf::FloatRect bounds = btnTxt.getLocalBounds();
                    btnTxt.setPosition(bx + (currentBtnW - bounds.width) / 2, by + 9);
                    btnTxt.setFillColor(selected ? hexToColor(Colors::DARK_BG) : hexToColor(i == 4 ? Colors::SUCCESS : Colors::TEXT_SECONDARY));
                    window.draw(btnTxt);

                    if (hovered && clicked) {
                        state.strategy = stratVals[i];
                    }
                }

                ly += card4H + 10;

                float searchBtnH = 44;
                bool searchHover = mousePos.x >= cardMargin && mousePos.x <= cardMargin + cardWidth &&
                                  mousePos.y >= ly && mousePos.y <= ly + searchBtnH;

                if (searchHover) {
                    sf::RectangleShape glow(sf::Vector2f(cardWidth + 6, searchBtnH + 6));
                    glow.setPosition(cardMargin - 3, ly - 3);
                    glow.setFillColor(sf::Color(0, 255, 150, 50));
                    window.draw(glow);
                }

                sf::RectangleShape searchBtn(sf::Vector2f(cardWidth, searchBtnH));
                searchBtn.setPosition(cardMargin, ly);
                searchBtn.setFillColor(searchHover ? hexToColor(0x00dd99FF) : hexToColor(0x00bb77FF));
                searchBtn.setOutlineColor(hexToColor(0x00ff99FF));
                searchBtn.setOutlineThickness(1.5f);
                window.draw(searchBtn);

                sf::Text searchTxt("SEARCH ROUTES", font, 13);
                searchTxt.setStyle(sf::Text::Bold);
                sf::FloatRect searchBounds = searchTxt.getLocalBounds();
                searchTxt.setPosition(cardMargin + (cardWidth - searchBounds.width) / 2, ly + 13);
                searchTxt.setFillColor(hexToColor(Colors::DARK_BG));
                searchTxt.setLetterSpacing(1.5f);
                window.draw(searchTxt);

                if (searchHover && clicked) {
                    performSearch(graph, journeyManager, state);
                }

                ly += searchBtnH + 15;

                bool backHover = mousePos.x >= cardMargin && mousePos.x <= cardMargin + cardWidth &&
                                mousePos.y >= ly && mousePos.y <= ly + 34;

                sf::RectangleShape backBtn(sf::Vector2f(cardWidth, 34));
                backBtn.setPosition(cardMargin, ly);
                backBtn.setFillColor(backHover ? sf::Color(45, 30, 35) : sf::Color(30, 22, 28));
                backBtn.setOutlineColor(backHover ? sf::Color(100, 60, 70) : sf::Color(60, 40, 50));
                backBtn.setOutlineThickness(1.0f);
                window.draw(backBtn);

                sf::Text backTxt("Main Menu", font, 11);
                sf::FloatRect backBounds = backTxt.getLocalBounds();
                backTxt.setPosition(cardMargin + (cardWidth - backBounds.width) / 2, ly + 10);
                backTxt.setFillColor(hexToColor(backHover ? 0xffaa88FF : Colors::TEXT_MUTED));
                window.draw(backTxt);

                if (backHover && clicked) {
                    state.appState = AppState::MAIN_MENU;

                    state.hasResults = false;
                    state.journeyPortCount = 0;
                    state.journeyScheduleCount = 0;
                    state.selectedJourneyIndex = -1;
                    state.showAllRoutes = false;
                }

                ly += 42;

                bool toggleHover = mousePos.x >= cardMargin && mousePos.x <= cardMargin + cardWidth &&
                                  mousePos.y >= ly && mousePos.y <= ly + 28;

                sf::RectangleShape toggleBg(sf::Vector2f(cardWidth, 28));
                toggleBg.setPosition(cardMargin, ly);
                toggleBg.setFillColor(toggleHover ? sf::Color(30, 30, 50) : sf::Color(20, 20, 35));
                toggleBg.setOutlineColor(state.showAllRoutes ? hexToColor(Colors::HIGHLIGHT) : sf::Color(50, 50, 70));
                toggleBg.setOutlineThickness(1.0f);
                window.draw(toggleBg);

                sf::RectangleShape toggleCheckbox(sf::Vector2f(16, 16));
                toggleCheckbox.setPosition(cardMargin + 10, ly + 6);
                toggleCheckbox.setFillColor(state.showAllRoutes ? hexToColor(Colors::HIGHLIGHT) : sf::Color(40, 40, 60));
                toggleCheckbox.setOutlineColor(state.showAllRoutes ? hexToColor(Colors::ELECTRIC_BLUE) : sf::Color(60, 60, 80));
                toggleCheckbox.setOutlineThickness(1.0f);
                window.draw(toggleCheckbox);

                if (state.showAllRoutes) {
                    sf::Text checkmark("✓", font, 12);
                    checkmark.setPosition(cardMargin + 12, ly + 6);
                    checkmark.setFillColor(hexToColor(Colors::DARK_BG));
                    checkmark.setStyle(sf::Text::Bold);
                    window.draw(checkmark);
                }

                drawText(window, font, "Show All Routes (N)", cardMargin + 32, ly + 8, 10,
                        state.showAllRoutes ? Colors::TEXT_PRIMARY : Colors::TEXT_SECONDARY);

                if (toggleHover && clicked) {
                    state.showAllRoutes = !state.showAllRoutes;
                }

                ly += 38;

                unsigned int statusColor = state.isError ? Colors::ERROR : (state.hasResults ? Colors::SUCCESS : Colors::TEXT_MUTED);
        sf::RectangleShape statusBar(sf::Vector2f(LEFT_SIDEBAR_WIDTH - 16, 28));
        statusBar.setPosition(8, ly);
        statusBar.setFillColor(state.isError ? sf::Color(40, 20, 20, 150) : (state.hasResults ? sf::Color(20, 40, 20, 150) : sf::Color(25, 25, 35, 150)));
        statusBar.setOutlineColor(hexToColor(statusColor));
        statusBar.setOutlineThickness(1);
        window.draw(statusBar);

        sf::CircleShape statusIcon(4);
        statusIcon.setPosition(22, ly + 10);
        statusIcon.setFillColor(hexToColor(statusColor));
        window.draw(statusIcon);

        drawText(window, font, state.statusMessage, 35, ly + 6, 11, statusColor);

        } else {

            drawPanel(window, font, 0, 0, (float)LEFT_SIDEBAR_WIDTH, (float)WINDOW_HEIGHT,
                      "Company Routes", Colors::PANEL_BG, Colors::PANEL_HEADER);

            float ly = 65;

            if (state.appState == AppState::COMPANY_VIEWER) {
                bool backMenuHover = mousePos.x >= 15 && mousePos.x <= LEFT_SIDEBAR_WIDTH - 15 &&
                                   mousePos.y >= ly && mousePos.y <= ly + 35;

                sf::RectangleShape backMenuBtn(sf::Vector2f(LEFT_SIDEBAR_WIDTH - 30, 35));
                backMenuBtn.setPosition(15, ly);
                backMenuBtn.setFillColor(hexToColor(backMenuHover ? 0x402020FF : 0x301818FF));
                backMenuBtn.setOutlineColor(hexToColor(0x604040FF));
                backMenuBtn.setOutlineThickness(1);
                window.draw(backMenuBtn);

                drawText(window, font, "< MAIN MENU", 30, ly + 9, 12,
                        backMenuHover ? Colors::WARNING : Colors::TEXT_MUTED, false);

                if (backMenuHover && clicked) {
                    state.appState = AppState::MAIN_MENU;
                    state.currentView = VIEW_MAIN_SEARCH;

                    state.hasResults = false;
                    state.journeyPortCount = 0;
                    state.journeyScheduleCount = 0;
                    state.selectedJourneyIndex = -1;
                    state.showAllRoutes = false;
                }
                ly += 45;
            }

            drawText(window, font, "Select a company to view their routes", 18, ly, 11, Colors::TEXT_SECONDARY);
            ly += 30;

            bool allHover = mousePos.x >= 15 && mousePos.x <= LEFT_SIDEBAR_WIDTH - 15 &&
                           mousePos.y >= ly && mousePos.y <= ly + 35;

            sf::RectangleShape allCompBtn(sf::Vector2f(LEFT_SIDEBAR_WIDTH - 30, 35));
            allCompBtn.setPosition(15, ly);
            allCompBtn.setFillColor(hexToColor(state.selectedCompanyIndex == -1 ? Colors::INFO : (allHover ? 0x2a2a45FF : 0x1a1a35FF)));
            allCompBtn.setOutlineColor(hexToColor(state.selectedCompanyIndex == -1 ? Colors::HIGHLIGHT : 0x3a3a55FF));
            allCompBtn.setOutlineThickness(state.selectedCompanyIndex == -1 ? 2 : 1);
            window.draw(allCompBtn);

            drawText(window, font, "ALL COMPANIES", 30, ly + 9, 13,
                    state.selectedCompanyIndex == -1 ? Colors::HIGHLIGHT : (allHover ? Colors::INFO : Colors::TEXT_MUTED),
                    state.selectedCompanyIndex == -1);

            if (allHover && clicked) {
                state.selectedCompanyIndex = -1;
                state.showAllCompanies = true;
            }
            ly += 45;

            drawText(window, font, "COMPANIES:", 18, ly, 12, Colors::TEXT_SECONDARY, true);
            ly += 25;

            int visibleCompanies = 12;
            int startIdx = state.companyScrollOffset;
            int endIdx = min(state.companyCount, startIdx + visibleCompanies);

            for (int i = startIdx; i < endIdx; i++) {
                bool compHover = mousePos.x >= 15 && mousePos.x <= LEFT_SIDEBAR_WIDTH - 15 &&
                                mousePos.y >= ly && mousePos.y <= ly + 32;
                bool isSelected = (i == state.selectedCompanyIndex);

                sf::RectangleShape compBtn(sf::Vector2f(LEFT_SIDEBAR_WIDTH - 30, 32));
                compBtn.setPosition(15, ly);
                compBtn.setFillColor(hexToColor(isSelected ? 0x3a2a50FF : (compHover ? 0x2a2a45FF : 0x1a1a30FF)));
                compBtn.setOutlineColor(hexToColor(isSelected ? Colors::NEON_PURPLE : 0x3a3a55FF));
                compBtn.setOutlineThickness(isSelected ? 2 : 1);
                window.draw(compBtn);

                string displayName = state.companyList[i];
                if (displayName.length() > 20) displayName = displayName.substr(0, 17) + "...";
                drawText(window, font, displayName, 25, ly + 8, 11,
                        isSelected ? Colors::NEON_PURPLE : (compHover ? Colors::INFO : Colors::TEXT_PRIMARY));

                if (compHover && clicked) {
                    state.selectedCompanyIndex = i;
                    state.showAllCompanies = false;
                }

                ly += 37;
            }

            if (state.companyCount > visibleCompanies) {
                drawText(window, font, "(Scroll for more)", 18, ly, 10, Colors::TEXT_MUTED);
                ly += 20;
            }

            ly = WINDOW_HEIGHT - 50;
            sf::RectangleShape statusBar(sf::Vector2f(LEFT_SIDEBAR_WIDTH - 30, 28));
            statusBar.setPosition(15, ly);
            statusBar.setFillColor(hexToColor(0x1a1a30FF));
            statusBar.setOutlineColor(hexToColor(Colors::INFO));
            statusBar.setOutlineThickness(1);
            window.draw(statusBar);

            sf::CircleShape statusIcon(4);
            statusIcon.setPosition(22, ly + 10);
            statusIcon.setFillColor(hexToColor(Colors::INFO));
            window.draw(statusIcon);

            string statusMsg = state.selectedCompanyIndex == -1 ?
                             ("Showing " + to_string(state.companyCount) + " companies") :
                             ("Viewing: " + state.companyList[state.selectedCompanyIndex]);
            drawText(window, font, statusMsg, 35, ly + 6, 10, Colors::INFO);
        }

        }

        if ((state.appState == AppState::MAIN_MENU || state.appState == AppState::ROUTE_PLANNER) &&
            state.currentView == VIEW_MAIN_SEARCH) {
            float rx = (float)(WINDOW_WIDTH - RIGHT_SIDEBAR_WIDTH);

            sf::RectangleShape rightBg(sf::Vector2f((float)RIGHT_SIDEBAR_WIDTH, (float)WINDOW_HEIGHT));
            rightBg.setPosition(rx, 0);
            rightBg.setFillColor(sf::Color(12, 15, 25, 255));
            window.draw(rightBg);

            sf::RectangleShape titleBar(sf::Vector2f((float)RIGHT_SIDEBAR_WIDTH, 45));
            titleBar.setPosition(rx, 0);
            titleBar.setFillColor(sf::Color(20, 25, 40, 255));
            window.draw(titleBar);

            sf::Text panelTitle("ANALYTICS", font, 13);
            panelTitle.setStyle(sf::Text::Bold);
            panelTitle.setPosition(rx + 15, 14);
            panelTitle.setFillColor(hexToColor(Colors::TEXT_PRIMARY));
            panelTitle.setLetterSpacing(1.3f);
            window.draw(panelTitle);

            float ry = 55.0f;

            string strategyName = "";
            unsigned int stratColor = Colors::ELECTRIC_BLUE;
            if (state.strategy == UI_DIJKSTRA_COST) {
                strategyName = "Strategy: Dijkstra (Cost)";
                stratColor = Colors::SUCCESS;
            } else if (state.strategy == UI_DIJKSTRA_TIME) {
                strategyName = "Strategy: Dijkstra (Time)";
                stratColor = Colors::SUCCESS;
            } else if (state.strategy == UI_ASTAR_COST) {
                strategyName = "Strategy: A* (Cost)";
                stratColor = Colors::NEON_PURPLE;
            } else if (state.strategy == UI_ASTAR_TIME) {
                strategyName = "Strategy: A* (Time)";
                stratColor = Colors::NEON_PURPLE;
            } else if (state.strategy == UI_SAFEST) {
                strategyName = "Strategy: Safest Route";
                stratColor = Colors::LIME_GREEN;
            }

            if (!strategyName.empty() && state.hasResults) {
                drawText(window, font, strategyName, rx + 15, ry + 5, 10, stratColor, true);
                ry += 20;
            }

            float tabBarY = ry;
            float tabBarH = 38.0f;
            float tabW = (RIGHT_SIDEBAR_WIDTH - 30.0f) / 3.0f;

            const char* tabLabels[] = {"Algorithms", "Route Stats", "Risk"};
            UIState::RightPanelTab tabValues[] = {UIState::TAB_ALGORITHMS, UIState::TAB_ROUTE_STATS, UIState::TAB_RISK};

            for (int i = 0; i < 3; i++) {
                float tabX = rx + 15.0f + i * tabW;
                bool isActive = (state.activeRightTab == tabValues[i]);
                bool isHovered = mousePos.x >= tabX && mousePos.x <= tabX + tabW - 2 &&
                                mousePos.y >= tabBarY && mousePos.y <= tabBarY + tabBarH;

                if (isHovered && !isActive) {
                    state.tabHoverGlow[i] = std::min(1.0f, state.tabHoverGlow[i] + dt * 5.0f);
                } else if (!isHovered) {
                    state.tabHoverGlow[i] = std::max(0.0f, state.tabHoverGlow[i] - dt * 5.0f);
                }

                drawTab(window, font, tabLabels[i], tabX, tabBarY, tabW - 2, tabBarH, isActive, state.tabHoverGlow[i]);

                if (isHovered && clicked) {
                    state.activeRightTab = tabValues[i];
                }
            }

            ry += tabBarH + 15;

            float contentY = ry;
            float contentH = WINDOW_HEIGHT - ry - 80;

            if (state.hasResults) {

                if (state.activeRightTab == UIState::TAB_ALGORITHMS) {

                    float cardMargin = 15.0f;
                    float cardW = RIGHT_SIDEBAR_WIDTH - cardMargin * 2;

                    bool isDijkstraStrategy = (state.strategy == UI_DIJKSTRA_COST || state.strategy == UI_DIJKSTRA_TIME);
                    bool isAStarStrategy = (state.strategy == UI_ASTAR_COST || state.strategy == UI_ASTAR_TIME);
                    bool isSafestStrategy = (state.strategy == UI_SAFEST);

                    float cardH = 135.0f;
                    drawCard(window, rx + cardMargin, contentY, cardW, cardH, Colors::SUCCESS);

                    drawText(window, font, "DIJKSTRA ALGORITHM", rx + cardMargin + 15, contentY + 12, 11, Colors::SUCCESS, true);

                    if (state.cheapestResult.valid && isDijkstraStrategy) {

                        if (state.strategy == UI_DIJKSTRA_COST) {

                            string costStr = "$" + to_string((int)state.cheapestResult.totalCost);
                            sf::Text bigCost(costStr, font, 28);
                            bigCost.setPosition(rx + cardMargin + 15, contentY + 38);
                            bigCost.setFillColor(hexToColor(Colors::LIME_GREEN));
                            bigCost.setStyle(sf::Text::Bold);
                            window.draw(bigCost);

                            string legsStr = to_string(state.cheapestResult.legs) + " legs";
                            drawText(window, font, legsStr, rx + cardMargin + 15, contentY + 75, 10, Colors::TEXT_SECONDARY);

                            int totalMins = state.cheapestResult.totalTime;
                            int hours = totalMins / 60;
                            int mins = totalMins % 60;
                            string timeStr = to_string(hours) + "h " + to_string(mins) + "m";
                            drawText(window, font, timeStr, rx + cardMargin + 15, contentY + 93, 10, Colors::TEXT_MUTED);
                        } else {

                            int totalMins = state.cheapestResult.totalTime;
                            int hours = totalMins / 60;
                            int mins = totalMins % 60;
                            string timeStr = to_string(hours) + "h " + to_string(mins) + "m";
                            sf::Text bigTime(timeStr, font, 26);
                            bigTime.setPosition(rx + cardMargin + 15, contentY + 38);
                            bigTime.setFillColor(hexToColor(Colors::LIME_GREEN));
                            bigTime.setStyle(sf::Text::Bold);
                            window.draw(bigTime);

                            string legsStr = to_string(state.cheapestResult.legs) + " legs";
                            drawText(window, font, legsStr, rx + cardMargin + 15, contentY + 75, 10, Colors::TEXT_SECONDARY);

                            string costStr = "$" + to_string((int)state.cheapestResult.totalCost);
                            drawText(window, font, costStr, rx + cardMargin + 15, contentY + 93, 10, Colors::TEXT_MUTED);
                        }

                        string nodesStr = "Nodes: " + to_string(state.cheapestResult.nodesExpanded);
                        drawText(window, font, nodesStr, rx + cardMargin + 15, contentY + 111, 9, Colors::TEXT_MUTED);
                    } else if (isSafestStrategy || isAStarStrategy) {
                        drawText(window, font, "Not evaluated", rx + cardMargin + 15, contentY + 50, 10, Colors::TEXT_MUTED);
                        drawText(window, font, "(using " + string(isSafestStrategy ? "Safest" : "A*") + " strategy)",
                                rx + cardMargin + 15, contentY + 70, 9, Colors::TEXT_MUTED);
                    } else {
                        drawText(window, font, "No route found", rx + cardMargin + 15, contentY + 50, 10, Colors::TEXT_MUTED);
                    }

                    contentY += cardH + 10;

                    drawCard(window, rx + cardMargin, contentY, cardW, cardH, Colors::SECONDARY);

                    drawText(window, font, "A* ALGORITHM", rx + cardMargin + 15, contentY + 12, 11, Colors::SECONDARY, true);

                    if (state.astarResult.valid && isAStarStrategy) {

                        if (state.strategy == UI_ASTAR_COST) {

                            string costStr = "$" + to_string((int)state.astarResult.totalCost);
                            sf::Text bigCost(costStr, font, 28);
                            bigCost.setPosition(rx + cardMargin + 15, contentY + 38);
                            bigCost.setFillColor(hexToColor(Colors::ELECTRIC_BLUE));
                            bigCost.setStyle(sf::Text::Bold);
                            window.draw(bigCost);

                            string legsStr = to_string(state.astarResult.legs) + " legs";
                            drawText(window, font, legsStr, rx + cardMargin + 15, contentY + 75, 10, Colors::TEXT_SECONDARY);

                            int totalMins = state.astarResult.totalTime;
                            int hours = totalMins / 60;
                            int mins = totalMins % 60;
                            string timeStr = to_string(hours) + "h " + to_string(mins) + "m";
                            drawText(window, font, timeStr, rx + cardMargin + 15, contentY + 93, 10, Colors::TEXT_MUTED);
                        } else {

                            int totalMins = state.astarResult.totalTime;
                            int hours = totalMins / 60;
                            int mins = totalMins % 60;
                            string timeStr = to_string(hours) + "h " + to_string(mins) + "m";
                            sf::Text bigTime(timeStr, font, 26);
                            bigTime.setPosition(rx + cardMargin + 15, contentY + 38);
                            bigTime.setFillColor(hexToColor(Colors::ELECTRIC_BLUE));
                            bigTime.setStyle(sf::Text::Bold);
                            window.draw(bigTime);

                            string legsStr = to_string(state.astarResult.legs) + " legs";
                            drawText(window, font, legsStr, rx + cardMargin + 15, contentY + 75, 10, Colors::TEXT_SECONDARY);

                            string costStr = "$" + to_string((int)state.astarResult.totalCost);
                            drawText(window, font, costStr, rx + cardMargin + 15, contentY + 93, 10, Colors::TEXT_MUTED);
                        }

                        string nodesStr = "Nodes: " + to_string(state.astarResult.nodesExpanded);
                        drawText(window, font, nodesStr, rx + cardMargin + 15, contentY + 111, 9, Colors::TEXT_MUTED);
                    } else if (isSafestStrategy || isDijkstraStrategy) {
                        drawText(window, font, "Not evaluated", rx + cardMargin + 15, contentY + 50, 10, Colors::TEXT_MUTED);
                        drawText(window, font, "(using " + string(isSafestStrategy ? "Safest" : "Dijkstra") + " strategy)",
                                rx + cardMargin + 15, contentY + 70, 9, Colors::TEXT_MUTED);
                    } else {
                        drawText(window, font, "No route found", rx + cardMargin + 15, contentY + 50, 10, Colors::TEXT_MUTED);
                    }

                    contentY += cardH + 15;

                    if (isSafestStrategy && state.safestResult.valid) {
                        float safeCardH = 80;
                        drawCard(window, rx + cardMargin, contentY, cardW, safeCardH, Colors::INFO);

                        drawText(window, font, "SAFEST ROUTE", rx + cardMargin + 15, contentY + 12, 10, Colors::INFO, true);

                        string costStr = "$" + to_string(state.safestResult.cost);
                        drawText(window, font, costStr, rx + cardMargin + 15, contentY + 35, 11, Colors::TEXT_PRIMARY, true);

                        string legsStr = to_string(state.safestResult.legs) + " legs  |  Risk: " + to_string(state.safestResult.risk);
                        drawText(window, font, legsStr, rx + cardMargin + 15, contentY + 55, 9, Colors::TEXT_SECONDARY);
                    }

                    if (state.cheapestResult.valid && state.astarResult.valid &&
                        (isDijkstraStrategy || isAStarStrategy)) {
                        contentY += (isSafestStrategy ? 90 : 5);
                        float compH = 90;
                        drawCard(window, rx + cardMargin, contentY, cardW, compH, Colors::WARNING);

                        drawText(window, font, "ALGORITHM COMPARISON", rx + cardMargin + 15, contentY + 12, 10, Colors::WARNING, true);

                        float costDiff = ((float)state.astarResult.totalCost - state.cheapestResult.totalCost);
                        float timeSaved = state.cheapestResult.totalTime - state.astarResult.totalTime;
                        int nodesSaved = state.cheapestResult.nodesExpanded - state.astarResult.nodesExpanded;

                        char compStr[80];
                        snprintf(compStr, sizeof(compStr), "Time: A* saves %d min", (int)timeSaved);
                        drawText(window, font, compStr, rx + cardMargin + 15, contentY + 35, 9, Colors::TEXT_SECONDARY);

                        snprintf(compStr, sizeof(compStr), "Cost: A* adds $%d", (int)costDiff);
                        drawText(window, font, compStr, rx + cardMargin + 15, contentY + 52, 9, Colors::TEXT_SECONDARY);

                        snprintf(compStr, sizeof(compStr), "Efficiency: %d fewer nodes", nodesSaved);
                        drawText(window, font, compStr, rx + cardMargin + 15, contentY + 69, 9, Colors::TEXT_MUTED);
                    }

                } else if (state.activeRightTab == UIState::TAB_ROUTE_STATS) {

                    float cardMargin = 15.0f;
                    float cardW = RIGHT_SIDEBAR_WIDTH - cardMargin * 2;

                    bool hasSelected = (state.selectedJourneyIndex >= 0 &&
                                       state.selectedJourneyIndex < state.journeyListCount);

                    if (state.journeyListCount > 0) {

                        drawText(window, font, "AVAILABLE ROUTES (" + to_string(state.journeyListCount) + ")",
                                rx + cardMargin + 15, contentY, 11, Colors::ELECTRIC_BLUE, true);
                        contentY += 25;

                        int maxRouteCards = min(3, state.journeyListCount);
                        for (int i = 0; i < maxRouteCards; i++) {
                            const UIState::JourneyInfo* journey = &state.journeyList[i];
                            if (!journey->valid) continue;

                            bool isSelected = (i == state.selectedJourneyIndex);
                            float routeCardH = 75.0f;
                            float routeCardX = rx + cardMargin;
                            float routeCardY = contentY;

                            // Check if mouse is hovering over this route card
                            bool routeCardHover = (mousePos.x >= routeCardX && 
                                                  mousePos.x <= routeCardX + cardW &&
                                                  mousePos.y >= routeCardY && 
                                                  mousePos.y <= routeCardY + routeCardH);

                            // Handle click on route card to switch selection
                            if (routeCardHover && clicked && !isSelected) {
                                state.selectedJourneyIndex = i;
                                
                                // Update journey ports for animation from schedule
                                if (journey->valid && journey->legs > 0) {
                                    state.journeyPortCount = journey->legs + 1;
                                    
                                    // First port is the origin from first leg
                                    if (journey->legs > 0) {
                                        state.journeyPorts[0] = journey->schedule[0].fromPort;
                                        
                                        // Subsequent ports are destination of each leg
                                        for (int p = 0; p < journey->legs && p < 19; p++) {
                                            state.journeyPorts[p + 1] = journey->schedule[p].toPort;
                                        }
                                    }
                                    
                                    // Reset animation
                                    state.animState = UIState::ANIM_EXPLORING;
                                    state.explorationAnimTime = 0.0f;
                                    state.explorationEdgesDrawn = 0;
                                    state.lineDrawProgress = 0.0f;
                                    state.shipAnimationActive = false;
                                    state.shipCurrentLeg = 0;
                                    state.shipProgress = 0.0f;
                                    gShipAnimator.reset();  // Reset sprite animator
                                }
                            }

                            unsigned int cardColor = isSelected ? Colors::SUCCESS : Colors::INFO;
                            drawCard(window, routeCardX, routeCardY, cardW, routeCardH, cardColor);

                            string routeLabel = "Route " + to_string(i + 1);
                            if (isSelected) routeLabel += " (Selected)";
                            drawText(window, font, routeLabel, routeCardX + 10, routeCardY + 8, 9, cardColor, true);

                            string costStr = "$" + to_string(journey->cost);
                            sf::Text routeCost(costStr, font, 18);
                            routeCost.setPosition(routeCardX + 10, routeCardY + 26);
                            routeCost.setFillColor(hexToColor(Colors::LIME_GREEN));
                            routeCost.setStyle(sf::Text::Bold);
                            window.draw(routeCost);

                            int hours = journey->totalMinutes / 60;
                            int mins = journey->totalMinutes % 60;
                            string statsStr = to_string(journey->legs) + " legs  |  " +
                                             to_string(hours) + "h " + to_string(mins) + "m";
                            drawText(window, font, statsStr, routeCardX + 10, routeCardY + 52, 8, Colors::TEXT_SECONDARY);

                            if (!isSelected) {
                                string hintText = routeCardHover ? "Click to select" : "Click to select";
                                drawText(window, font, hintText, routeCardX + cardW - 95, routeCardY + 8, 7, 
                                        routeCardHover ? Colors::LIME_GREEN : Colors::TEXT_MUTED);
                            }

                            contentY += routeCardH + 8;
                        }

                        if (state.journeyListCount > maxRouteCards) {
                            drawText(window, font, "... +" + to_string(state.journeyListCount - maxRouteCards) + " more routes available",
                                    rx + cardMargin + 15, contentY, 8, Colors::TEXT_MUTED);
                            contentY += 20;
                        }

                        contentY += 10;

                        const UIState::JourneyInfo* selectedJourney = hasSelected ?
                            &state.journeyList[state.selectedJourneyIndex] : nullptr;

                        if (selectedJourney && selectedJourney->valid) {

                            drawText(window, font, "SELECTED ROUTE DETAILS", rx + cardMargin + 15, contentY, 10, Colors::TEXT_MUTED, true);
                            contentY += 22;

                        float legStartY = contentY;
                        float legEndY = (float)WINDOW_HEIGHT - 95.0f;
                        float legCardH = 85.0f;
                        int maxVisibleLegs = (int)((legEndY - legStartY) / (legCardH + 5));
                        maxVisibleLegs = max(1, min(maxVisibleLegs, selectedJourney->legs));

                        for (int li = 0; li < min(maxVisibleLegs, selectedJourney->legs); li++) {
                            const UIState::LegSchedule& leg = selectedJourney->schedule[li];

                            drawCard(window, rx + cardMargin, contentY, cardW, legCardH, Colors::SUCCESS);

                            string legLabel = "LEG " + to_string(li + 1);
                            drawText(window, font, legLabel, rx + cardMargin + 10, contentY + 8, 9, Colors::SUCCESS, true);

                            string routeStr = leg.fromPort + " -> " + leg.toPort;
                            drawText(window, font, routeStr, rx + cardMargin + 10, contentY + 26, 10, Colors::TEXT_PRIMARY, true);

                            char depStr[40];
                            snprintf(depStr, sizeof(depStr), "Dep: %02d/%02d %02d:%02d",
                                    leg.depDay, leg.depMonth, leg.depHour, leg.depMinute);
                            drawText(window, font, depStr, rx + cardMargin + 10, contentY + 44, 8, Colors::TEXT_SECONDARY);

                            char arrStr[40];
                            snprintf(arrStr, sizeof(arrStr), "Arr: %02d:%02d", leg.arrHour, leg.arrMinute);
                            drawText(window, font, arrStr, rx + cardMargin + 10, contentY + 60, 8, Colors::TEXT_SECONDARY);

                            string compStr = leg.company + " - $" + to_string(leg.cost);
                            drawText(window, font, compStr, rx + cardMargin + 10, contentY + 76, 8, Colors::TEXT_MUTED);

                            contentY += legCardH + 5;
                            
                            if (li < selectedJourney->legs - 1) {
                                const UIState::LegSchedule& nextLeg = selectedJourney->schedule[li + 1];
                                
                                int arrTotalMins = leg.arrHour * 60 + leg.arrMinute;
                                int depTotalMins = nextLeg.depHour * 60 + nextLeg.depMinute;
                                int layoverMins = depTotalMins - arrTotalMins;
                                
                                if (layoverMins < 0) {
                                    layoverMins += 24 * 60;
                                }
                                
                                int layoverHours = layoverMins / 60;
                                int layoverRemainder = layoverMins % 60;
                                
                                char layoverStr[60];
                                if (layoverHours > 0) {
                                    snprintf(layoverStr, sizeof(layoverStr), "Layover: %dh %dm at %s", 
                                            layoverHours, layoverRemainder, leg.toPort.c_str());
                                } else {
                                    snprintf(layoverStr, sizeof(layoverStr), "Layover: %dm at %s", 
                                            layoverMins, leg.toPort.c_str());
                                }
                                
                                drawText(window, font, layoverStr, rx + cardMargin + 15, contentY, 8, 
                                        layoverMins < 60 ? Colors::WARNING : Colors::INFO);
                                contentY += 18;
                            }
                        }

                        if (selectedJourney->legs > maxVisibleLegs) {
                            drawText(window, font, "... + " + to_string(selectedJourney->legs - maxVisibleLegs) + " more legs",
                                    rx + cardMargin + 15, contentY + 5, 8, Colors::TEXT_MUTED);
                        }
                    } else {

                        float msgCardH = 100.0f;
                        drawCard(window, rx + cardMargin, contentY, cardW, msgCardH, Colors::INFO);

                        drawText(window, font, "NO ROUTE SELECTED", rx + cardMargin + 15, contentY + 20, 11, Colors::INFO, true);
                        drawText(window, font, "Run a search and select", rx + cardMargin + 15, contentY + 50, 9, Colors::TEXT_SECONDARY);
                        drawText(window, font, "a route to see details", rx + cardMargin + 15, contentY + 68, 9, Colors::TEXT_SECONDARY);

                        contentY += msgCardH + 10;
                    }

                    } else {

                        float msgCardH = 100.0f;
                        drawCard(window, rx + cardMargin, contentY, cardW, msgCardH, Colors::INFO);
                        drawText(window, font, "NO ROUTES FOUND", rx + cardMargin + 15, contentY + 20, 11, Colors::INFO, true);
                        drawText(window, font, "Run a search to find routes", rx + cardMargin + 15, contentY + 50, 9, Colors::TEXT_SECONDARY);
                    }

                    if (state.journeyListCount > 1 && hasSelected) {
                        contentY = (float)WINDOW_HEIGHT - 180.0f;

                        drawText(window, font, "ALL ROUTES SUMMARY", rx + cardMargin + 15, contentY, 9, Colors::TEXT_MUTED, true);
                        contentY += 18;

                        float statH = 60;
                        float statW = (cardW - 5) / 2;

                        drawCard(window, rx + cardMargin, contentY, statW, statH, Colors::SUCCESS);
                        drawText(window, font, "TOTAL ROUTES", rx + cardMargin + 10, contentY + 10, 9, Colors::SUCCESS, true);
                        string totalStr = to_string(state.journeyListCount);
                        sf::Text totalTxt(totalStr, font, 18);
                        totalTxt.setPosition(rx + cardMargin + 10, contentY + 30);
                        totalTxt.setFillColor(hexToColor(Colors::LIME_GREEN));
                        totalTxt.setStyle(sf::Text::Bold);
                        window.draw(totalTxt);

                        drawCard(window, rx + cardMargin + statW + 5, contentY, statW, statH, Colors::WARNING);
                        drawText(window, font, "MAX LEGS", rx + cardMargin + statW + 15, contentY + 10, 9, Colors::WARNING, true);
                        string maxLegsStr = to_string(state.maxLegs);
                        sf::Text maxLegsTxt(maxLegsStr, font, 20);
                        maxLegsTxt.setPosition(rx + cardMargin + statW + 15, contentY + 30);
                        maxLegsTxt.setFillColor(hexToColor(0xffaa00FF));
                        maxLegsTxt.setStyle(sf::Text::Bold);
                        window.draw(maxLegsTxt);
                    }

                } else if (state.activeRightTab == UIState::TAB_RISK) {

                    float cardMargin = 15.0f;
                    float cardW = RIGHT_SIDEBAR_WIDTH - cardMargin * 2;

                    if (state.journeyListCount > 0 && state.selectedJourneyIndex >= 0 && state.selectedJourneyIndex < state.journeyListCount) {
                        UIState::JourneyInfo* selectedJourney = &state.journeyList[state.selectedJourneyIndex];
                        
                        int legCount = selectedJourney->legs;
                        float riskScore = (float)selectedJourney->risk;
                        const char* riskLevel = "Unknown";
                        unsigned int riskColor = Colors::TEXT_MUTED;

                        if (riskScore < 25) {
                            riskLevel = "Low";
                            riskColor = Colors::SUCCESS;
                        } else if (riskScore < 45) {
                            riskLevel = "Moderate";
                            riskColor = Colors::WARNING;
                        } else if (riskScore < 70) {
                            riskLevel = "High";
                            riskColor = 0xff7f00FF;
                        } else {
                            riskLevel = "Very High";
                            riskColor = Colors::DANGER;
                        }

                        float riskCardH = 70.0f;
                        drawCard(window, rx + cardMargin, contentY, cardW, riskCardH, riskColor);
                        
                        char riskText[100];
                        snprintf(riskText, 100, "RISK SCORE: %.0f%%", riskScore);
                        drawText(window, font, riskText, rx + cardMargin + 15, contentY + 15, 12, riskColor, true);
                        drawText(window, font, riskLevel, rx + cardMargin + 15, contentY + 40, 16, riskColor, true);
                        
                        float barWidth = cardW - 30;
                        float barX = rx + cardMargin + 15;
                        float barY = contentY + riskCardH - 15;
                        
                        sf::RectangleShape riskBarBg(sf::Vector2f(barWidth, 8));
                        riskBarBg.setPosition(barX, barY);
                        riskBarBg.setFillColor(sf::Color(40, 40, 45));
                        window.draw(riskBarBg);
                        
                        sf::RectangleShape riskBarFill(sf::Vector2f(barWidth * (riskScore / 100.0f), 8));
                        riskBarFill.setPosition(barX, barY);
                        riskBarFill.setFillColor(hexToColor(riskColor));
                        window.draw(riskBarFill);
                        
                        contentY += riskCardH + 15;

                        float factorsCardH = 180.0f;
                        drawCard(window, rx + cardMargin, contentY, cardW, factorsCardH, Colors::INFO);
                        
                        drawText(window, font, "RISK FACTORS", rx + cardMargin + 15, contentY + 12, 10, Colors::INFO, true);
                        float factorY = contentY + 35;

                        char legRisk[100];
                        snprintf(legRisk, 100, "Journey Complexity: %d leg(s)", legCount);
                        unsigned int legRiskColor = (legCount == 1) ? Colors::SUCCESS : 
                                                    (legCount == 2) ? Colors::WARNING :
                                                    (legCount == 3) ? 0xff7f00FF : Colors::DANGER;
                        drawText(window, font, legRisk, rx + cardMargin + 25, factorY, 9, legRiskColor);
                        factorY += 22;

                        if (legCount > 1 && selectedJourney->legs > 0) {
                            int minLayover = INT_MAX;
                            for (int i = 0; i < legCount - 1 && i < selectedJourney->legs - 1; i++) {
                                int arrTime = selectedJourney->schedule[i].arrHour * 60 + 
                                            selectedJourney->schedule[i].arrMinute;
                                int depTime = selectedJourney->schedule[i + 1].depHour * 60 + 
                                            selectedJourney->schedule[i + 1].depMinute;
                                int layover = depTime - arrTime;
                                if (layover < 0) layover += 1440;
                                if (layover < minLayover) minLayover = layover;
                            }

                            if (minLayover != INT_MAX) {
                                char layoverRisk[100];
                                snprintf(layoverRisk, 100, "Minimum Layover: %dh %dm", minLayover / 60, minLayover % 60);
                                unsigned int layoverColor = (minLayover < 60) ? Colors::DANGER :
                                                            (minLayover < 120) ? 0xff7f00FF :
                                                            (minLayover < 180) ? Colors::WARNING : Colors::SUCCESS;
                                drawText(window, font, layoverRisk, rx + cardMargin + 25, factorY, 9, layoverColor);
                                factorY += 22;
                            }
                        }

                        int totalMins = selectedJourney->time;
                        char timeRisk[100];
                        snprintf(timeRisk, 100, "Total Journey: %dh %dm", totalMins / 60, totalMins % 60);
                        unsigned int timeColor = (totalMins < 480) ? Colors::SUCCESS :
                                                (totalMins < 1440) ? Colors::WARNING :
                                                (totalMins < 2880) ? 0xff7f00FF : Colors::DANGER;
                        drawText(window, font, timeRisk, rx + cardMargin + 25, factorY, 9, timeColor);
                        factorY += 22;

                        char costRisk[100];
                        snprintf(costRisk, 100, "Total Cost: $%d", selectedJourney->cost);
                        unsigned int costColor = (selectedJourney->cost < 20000) ? Colors::SUCCESS :
                                                (selectedJourney->cost < 40000) ? Colors::WARNING : 0xff7f00FF;
                        drawText(window, font, costRisk, rx + cardMargin + 25, factorY, 9, costColor);
                        factorY += 22;

                        contentY += factorsCardH + 15;

                        float recoCardH = 120.0f;
                        drawCard(window, rx + cardMargin, contentY, cardW, recoCardH, Colors::HIGHLIGHT);
                        
                        drawText(window, font, "RECOMMENDATIONS", rx + cardMargin + 15, contentY + 12, 10, Colors::HIGHLIGHT, true);
                        float recoY = contentY + 35;

                        if (legCount >= 3) {
                            drawText(window, font, "Consider direct routes when available", rx + cardMargin + 25, recoY, 8, Colors::TEXT_SECONDARY);
                            recoY += 20;
                        }
                        if (legCount > 1) {
                            drawText(window, font, "Book with buffer time for connections", rx + cardMargin + 25, recoY, 8, Colors::TEXT_SECONDARY);
                            recoY += 20;
                        }
                        if (totalMins > 2880) {
                            drawText(window, font, "Long journey increases delay risk", rx + cardMargin + 25, recoY, 8, Colors::TEXT_SECONDARY);
                            recoY += 20;
                        }

                    } else {
                        float msgCardH = 100.0f;
                        drawCard(window, rx + cardMargin, contentY, cardW, msgCardH, Colors::INFO);

                        drawText(window, font, "NO ROUTE SELECTED", rx + cardMargin + 15, contentY + 20, 11, Colors::INFO, true);
                        drawText(window, font, "Run a search and select", rx + cardMargin + 15, contentY + 50, 9, Colors::TEXT_SECONDARY);
                        drawText(window, font, "a route to view risk analysis", rx + cardMargin + 15, contentY + 68, 9, Colors::TEXT_SECONDARY);
                    }
                }

                float footerY = (float)WINDOW_HEIGHT - 65.0f;
                sf::RectangleShape footer(sf::Vector2f((float)RIGHT_SIDEBAR_WIDTH - 30, 55));
                footer.setPosition(rx + 15, footerY);
                footer.setFillColor(sf::Color(20, 25, 35, 250));
                footer.setOutlineColor(hexToColor(Colors::ACCENT));
                footer.setOutlineThickness(1.0f);
                window.draw(footer);

                drawText(window, font, "BEST ROUTE", rx + 25, footerY + 8, 9, Colors::TEXT_MUTED, true);

                if (state.cheapestResult.valid) {
                    string bestDetails = "$" + to_string((int)state.cheapestResult.totalCost);
                    int totalMins = (int)state.cheapestResult.totalTime;
                    int hours = totalMins / 60;
                    int mins = totalMins % 60;
                    bestDetails += " - " + to_string(hours) + "h " + to_string(mins) + "m";

                    drawText(window, font, bestDetails, rx + 25, footerY + 28, 11, Colors::LIME_GREEN, true);
                }

            } else {

                float placeholderY = contentY + 80;

                sf::CircleShape compass(35.0f);
                compass.setPosition(rx + (float)RIGHT_SIDEBAR_WIDTH / 2.0f - 35.0f, placeholderY);
                compass.setFillColor(sf::Color(25, 30, 50, 255));
                compass.setOutlineColor(hexToColor(Colors::HIGHLIGHT_DIM));
                compass.setOutlineThickness(2.5f);
                window.draw(compass);

                sf::RectangleShape needle1(sf::Vector2f(35.0f, 3.0f));
                needle1.setPosition(rx + (float)RIGHT_SIDEBAR_WIDTH / 2.0f - 17.5f, placeholderY + 33.0f);
                needle1.setFillColor(hexToColor(Colors::HIGHLIGHT));
                window.draw(needle1);

                sf::RectangleShape needle2(sf::Vector2f(3.0f, 35.0f));
                needle2.setPosition(rx + (float)RIGHT_SIDEBAR_WIDTH / 2.0f - 1.5f, placeholderY + 16.0f);
                needle2.setFillColor(hexToColor(Colors::SECONDARY));
                window.draw(needle2);

                placeholderY += 90.0f;

                string titleText = "READY TO NAVIGATE";
                float titleW = (float)titleText.length() * 7.5f;
                drawText(window, font, titleText, rx + (float)RIGHT_SIDEBAR_WIDTH / 2.0f - titleW / 2.0f,
                        placeholderY, 12, Colors::HIGHLIGHT, true);

                placeholderY += 35.0f;

                sf::CircleShape dot(4.0f);
                dot.setFillColor(hexToColor(Colors::SUCCESS));
                dot.setPosition(rx + 40.0f, placeholderY + 4.0f);
                window.draw(dot);
                drawText(window, font, "Select origin & destination", rx + 55.0f, placeholderY, 11, Colors::TEXT_SECONDARY);
                placeholderY += 24.0f;

                dot.setFillColor(hexToColor(Colors::WARNING));
                dot.setPosition(rx + 40.0f, placeholderY + 4.0f);
                window.draw(dot);
                drawText(window, font, "Set travel preferences", rx + 55.0f, placeholderY, 11, Colors::TEXT_SECONDARY);
                placeholderY += 24.0f;

                dot.setFillColor(hexToColor(Colors::ELECTRIC_BLUE));
                dot.setPosition(rx + 40.0f, placeholderY + 4.0f);
                window.draw(dot);
                drawText(window, font, "Click SEARCH ROUTES", rx + 55.0f, placeholderY, 11, Colors::TEXT_SECONDARY);
            }
        }

        if ((state.appState == AppState::MAIN_MENU || state.appState == AppState::ROUTE_PLANNER || state.appState == AppState::COMPANY_VIEWER) &&
            state.currentView == VIEW_COMPANY_ROUTES) {
            float rx = (float)(WINDOW_WIDTH - RIGHT_SIDEBAR_WIDTH);

            drawPanel(window, font, rx, 0, (float)RIGHT_SIDEBAR_WIDTH, (float)WINDOW_HEIGHT,
                      "Company Info", Colors::PANEL_BG, Colors::PANEL_HEADER);
        }

        if (state.hasResults && state.journeyPortCount > 1) {
            float miniX = MAP_X + 15;
            float miniY = mapH - 140;
            float miniW = 180;
            float miniH = 120;

            sf::RectangleShape miniBg(sf::Vector2f(miniW, miniH));
            miniBg.setPosition(miniX, miniY);
            miniBg.setFillColor(sf::Color(5, 10, 25, 230));
            miniBg.setOutlineColor(sf::Color(0, 150, 255, 150));
            miniBg.setOutlineThickness(2);
            window.draw(miniBg);

            drawText(window, font, "ROUTE OVERVIEW", miniX + 10, miniY + 5, 10, Colors::HIGHLIGHT, true);

            float scale = 0.12f;
            float offsetX = miniX + 10 - MAP_X * scale;
            float offsetY = miniY + 25;

            for (int pi = 0; pi < state.journeyPortCount - 1; pi++) {
                float x1, y1, x2, y2;
                if (getPortCoords(state.journeyPorts[pi], x1, y1) &&
                    getPortCoords(state.journeyPorts[pi+1], x2, y2)) {

                    float mx1 = offsetX + x1 * scale;
                    float my1 = offsetY + y1 * scale * 0.8f;
                    float mx2 = offsetX + x2 * scale;
                    float my2 = offsetY + y2 * scale * 0.8f;

                    mx1 = max(miniX + 5, min(miniX + miniW - 5, mx1));
                    my1 = max(miniY + 20, min(miniY + miniH - 5, my1));
                    mx2 = max(miniX + 5, min(miniX + miniW - 5, mx2));
                    my2 = max(miniY + 20, min(miniY + miniH - 5, my2));

                    sf::Vertex line[] = {sf::Vertex(sf::Vector2f(mx1, my1), hexToColor(Colors::LEG_COLORS[pi % Colors::LEG_COLOR_COUNT])), sf::Vertex(sf::Vector2f(mx2, my2), hexToColor(Colors::LEG_COLORS[pi % Colors::LEG_COLOR_COUNT]))};
                    window.draw(line, 2, sf::Lines);

                    sf::CircleShape miniDot(3);
                    miniDot.setPosition(mx1 - 3, my1 - 3);
                    miniDot.setFillColor(pi == 0 ? hexToColor(Colors::SUCCESS) : hexToColor(Colors::ELECTRIC_BLUE));
                    window.draw(miniDot);
                }
            }

            float dx, dy;
            if (getPortCoords(state.journeyPorts[state.journeyPortCount - 1], dx, dy)) {
                float mdx = offsetX + dx * scale;
                float mdy = offsetY + dy * scale * 0.8f;
                mdx = max(miniX + 5, min(miniX + miniW - 5, mdx));
                mdy = max(miniY + 20, min(miniY + miniH - 5, mdy));

                sf::CircleShape destDot(4);
                destDot.setPosition(mdx - 4, mdy - 4);
                destDot.setFillColor(hexToColor(Colors::SECONDARY));
                window.draw(destDot);
            }

            drawText(window, font, state.journeyPorts[0], miniX + 10, miniY + miniH - 30, 9, Colors::SUCCESS);
            drawText(window, font, "->", miniX + miniW/2 - 8, miniY + miniH - 30, 9, Colors::TEXT_MUTED);
            drawText(window, font, state.journeyPorts[state.journeyPortCount-1], miniX + miniW/2 + 10, miniY + miniH - 30, 9, Colors::SECONDARY);

            string legsStr = to_string(state.journeyPortCount - 1) + " legs";
            drawText(window, font, legsStr, miniX + 10, miniY + miniH - 15, 9, Colors::TEXT_MUTED);
        }

        float routePanelY = (float)(WINDOW_HEIGHT - 135);
        float routePanelX = (float)MAP_X;
        float routePanelW = (float)MAP_WIDTH;
        float routePanelH = 90;

        sf::RectangleShape routePanelBg(sf::Vector2f(routePanelW, routePanelH));
        routePanelBg.setPosition(routePanelX, routePanelY);
        routePanelBg.setFillColor(sf::Color(10, 15, 35, 250));
        window.draw(routePanelBg);

        sf::RectangleShape routeAccent(sf::Vector2f(routePanelW, 4));
        routeAccent.setPosition(routePanelX, routePanelY);
        routeAccent.setFillColor(hexToColor(0x00aaffffFF));
        window.draw(routeAccent);

        sf::RectangleShape routeAccent2(sf::Vector2f(routePanelW, 2));
        routeAccent2.setPosition(routePanelX, routePanelY + 4);
        routeAccent2.setFillColor(hexToColor(0x0066aaFF));
        window.draw(routeAccent2);

        if (state.hasResults && state.journeyPortCount > 0) {

            drawText(window, font, "SELECTED ROUTE", routePanelX + 20, routePanelY + 12, 14, Colors::HIGHLIGHT, true);

            string routeStr = "";
            for (int i = 0; i < state.journeyPortCount; i++) {
                routeStr += state.journeyPorts[i];
                if (i < state.journeyPortCount - 1) routeStr += "  ->  ";
            }
            drawText(window, font, routeStr, routePanelX + 200, routePanelY + 14, 13, Colors::TEXT_PRIMARY);

            float nodeStartX = routePanelX + 30;
            float nodeY = routePanelY + 55;
            float nodeSpacing = min(130.0f, (routePanelW - 60) / (float)state.journeyPortCount);

            for (int i = 0; i < state.journeyPortCount; i++) {
                float nodeX = nodeStartX + i * nodeSpacing;

                if (i < state.journeyPortCount - 1) {
                    unsigned int arrowColor = Colors::LEG_COLORS[i % Colors::LEG_COLOR_COUNT];

                    sf::RectangleShape arrow(sf::Vector2f(nodeSpacing - 50, 6));
                    arrow.setPosition(nodeX + 40, nodeY + 5);
                    arrow.setFillColor(hexToColor(arrowColor));
                    window.draw(arrow);

                    sf::ConvexShape arrowHead(3);
                    arrowHead.setPoint(0, sf::Vector2f(0, 0));
                    arrowHead.setPoint(1, sf::Vector2f(0, 18));
                    arrowHead.setPoint(2, sf::Vector2f(14, 9));
                    arrowHead.setPosition(nodeX + nodeSpacing - 22, nodeY - 1);
                    arrowHead.setFillColor(hexToColor(arrowColor));
                    window.draw(arrowHead);

                    string legLabel = "LEG " + to_string(i + 1);
                    drawText(window, font, legLabel, nodeX + nodeSpacing/2 + 10, nodeY - 20, 11, arrowColor, true);
                }

                unsigned int nodeColor;
                if (i == 0) {
                    nodeColor = Colors::SUCCESS;
                } else if (i == state.journeyPortCount - 1) {
                    nodeColor = Colors::SECONDARY;
                } else {
                    nodeColor = Colors::ELECTRIC_BLUE;
                }

                sf::CircleShape nodeGlow(20);
                nodeGlow.setPosition(nodeX - 4, nodeY - 12);
                sf::Color glowCol = hexToColor(nodeColor);
                glowCol.a = 100;
                nodeGlow.setFillColor(glowCol);
                window.draw(nodeGlow);

                sf::CircleShape node(14);
                node.setPosition(nodeX, nodeY - 6);
                node.setFillColor(hexToColor(nodeColor));
                node.setOutlineColor(sf::Color::White);
                node.setOutlineThickness(3);
                window.draw(node);

                sf::Text portLabel(state.journeyPorts[i], font, 13);
                sf::FloatRect lb = portLabel.getLocalBounds();
                float labelX = nodeX + 7 - lb.width / 2;
                portLabel.setPosition(labelX, nodeY + 20);
                portLabel.setFillColor(hexToColor(Colors::TEXT_PRIMARY));
                portLabel.setStyle(sf::Text::Bold);
                window.draw(portLabel);
            }
        } else {

            sf::Text noRouteText("No route selected - Search for routes to display the path here", font, 16);
            sf::FloatRect bounds = noRouteText.getLocalBounds();
            noRouteText.setPosition(routePanelX + (routePanelW - bounds.width) / 2, routePanelY + 35);
            noRouteText.setFillColor(hexToColor(Colors::TEXT_MUTED));
            window.draw(noRouteText);
        }

        if (state.appState != AppState::MULTILEG_EDITOR && state.appState != AppState::DOCKING_MANAGER) {
            drawRect(window, 0.0f, (float)(WINDOW_HEIGHT - 45), (float)WINDOW_WIDTH, 45.0f, Colors::PANEL_HEADER);

            sf::RectangleShape footerAccent(sf::Vector2f((float)WINDOW_WIDTH, 2));
            footerAccent.setPosition(0, (float)(WINDOW_HEIGHT - 45));
            footerAccent.setFillColor(hexToColor(0x0088ffFF));
            window.draw(footerAccent);

            sf::Text titleText("OceanRoute Navigator v2.0", font, 16);
            titleText.setPosition(25.0f, (float)(WINDOW_HEIGHT - 35));
            titleText.setFillColor(hexToColor(Colors::HIGHLIGHT));
            titleText.setStyle(sf::Text::Bold);
            window.draw(titleText);

            drawText(window, font, "|  Maritime Navigation Optimizer", 260.0f, (float)(WINDOW_HEIGHT - 33), 14, Colors::TEXT_SECONDARY);

            unsigned int stratColor = (state.strategy == UI_DIJKSTRA_COST || state.strategy == UI_DIJKSTRA_TIME) ? Colors::SUCCESS :
                                      (state.strategy == UI_ASTAR_COST || state.strategy == UI_ASTAR_TIME) ? Colors::NEON_PURPLE : Colors::INFO;

            string stratName = (state.strategy == UI_DIJKSTRA_COST) ? "DIJKSTRA (COST)" :
                              (state.strategy == UI_DIJKSTRA_TIME) ? "DIJKSTRA (TIME)" :
                              (state.strategy == UI_ASTAR_COST) ? "A* (COST)" :
                              (state.strategy == UI_ASTAR_TIME) ? "A* (TIME)" : "SAFEST";

            sf::RectangleShape stratBadge(sf::Vector2f(180.0f, 26.0f));
            stratBadge.setPosition(560.0f, (float)(WINDOW_HEIGHT - 38));
            stratBadge.setFillColor(hexToColor(stratColor));
            stratBadge.setOutlineColor(sf::Color::White);
            stratBadge.setOutlineThickness(1);
            window.draw(stratBadge);
            drawText(window, font, stratName, 575.0f, (float)(WINDOW_HEIGHT - 34), 12, Colors::DARK_BG, true);

            if (state.hoveredPort >= 0) {
                sf::RectangleShape hoverBg(sf::Vector2f(250.0f, 30.0f));
                hoverBg.setPosition((float)(WINDOW_WIDTH - 280), (float)(WINDOW_HEIGHT - 40));
                hoverBg.setFillColor(hexToColor(0x2a1a4aFF));
                window.draw(hoverBg);

                sf::Text hoverText("PORT: " + PORT_POSITIONS[state.hoveredPort].name, font, 15);
                hoverText.setPosition((float)(WINDOW_WIDTH - 270), (float)(WINDOW_HEIGHT - 35));
                hoverText.setFillColor(hexToColor(Colors::NEON_PURPLE));
                hoverText.setStyle(sf::Text::Bold);
                window.draw(hoverText);
            }
        }

        if (state.appState == AppState::MULTILEG_EDITOR) {

            drawPanel(window, font, 0, 0, (float)LEFT_SIDEBAR_WIDTH, (float)WINDOW_HEIGHT,
                      "Multi-Leg Route Builder", Colors::PANEL_BG, Colors::PANEL_HEADER);

            float ly = 65;

            bool backMenuHover = mousePos.x >= 15 && mousePos.x <= LEFT_SIDEBAR_WIDTH - 15 &&
                               mousePos.y >= ly && mousePos.y <= ly + 40;

            sf::RectangleShape backMenuBtn(sf::Vector2f(LEFT_SIDEBAR_WIDTH - 30, 40));
            backMenuBtn.setPosition(15, ly);
            backMenuBtn.setFillColor(hexToColor(backMenuHover ? 0x402020FF : 0x301818FF));
            backMenuBtn.setOutlineColor(hexToColor(0x604040FF));
            backMenuBtn.setOutlineThickness(1);
            window.draw(backMenuBtn);

            drawText(window, font, "< BACK TO MAIN MENU", 30, ly + 11, 12,
                    backMenuHover ? Colors::WARNING : Colors::TEXT_MUTED, false);

            if (backMenuHover && clicked) {

                multiLegBuilder.clear();
                state.editorShowResults = false;
                state.editorSelectedNodeIndex = -1;
                state.editorSegmentCount = 0;
                state.appState = AppState::MAIN_MENU;
                state.currentView = VIEW_MAIN_SEARCH;
            }
            ly += 50;

            drawText(window, font, "BUILD CUSTOM MULTI-STOP ROUTE", 20, ly, 13, Colors::HIGHLIGHT, true);
            ly += 25;

            sf::RectangleShape infoBg(sf::Vector2f(LEFT_SIDEBAR_WIDTH - 30, 75));
            infoBg.setPosition(15, ly);
            infoBg.setFillColor(hexToColor(0x1a2a40FF));
            infoBg.setOutlineColor(hexToColor(Colors::INFO));
            infoBg.setOutlineThickness(1);
            window.draw(infoBg);

            drawText(window, font, "1. Type port name below", 25, ly + 8, 10, Colors::TEXT_SECONDARY);
            drawText(window, font, "2. Click 'Add Port' to add to sequence", 25, ly + 24, 10, Colors::TEXT_SECONDARY);
            drawText(window, font, "3. Build chain (2-10 ports)", 25, ly + 40, 10, Colors::TEXT_SECONDARY);
            drawText(window, font, "4. Click 'Find Route' to validate", 25, ly + 56, 10, Colors::TEXT_SECONDARY);
            ly += 85;

            drawText(window, font, "ENTER PORT NAME:", 20, ly, 11, Colors::HIGHLIGHT, true);
            ly += 22;

            bool inputHover = mousePos.x >= 15 && mousePos.x <= LEFT_SIDEBAR_WIDTH - 15 &&
                            mousePos.y >= ly && mousePos.y <= ly + 35;

            sf::RectangleShape inputBox(sf::Vector2f(LEFT_SIDEBAR_WIDTH - 100, 35));
            inputBox.setPosition(15, ly);
            inputBox.setFillColor(hexToColor(state.editorInputActive ? Colors::INPUT_BG : 0x1a1a2aFF));
            inputBox.setOutlineColor(hexToColor(state.editorInputActive ? Colors::INPUT_FOCUS : Colors::INPUT_BORDER));
            inputBox.setOutlineThickness(state.editorInputActive ? 2 : 1);
            window.draw(inputBox);

            drawText(window, font, state.editorInputBuffer.empty() ? "Type city..." : state.editorInputBuffer.c_str(),
                    25, ly + 9, 11, state.editorInputBuffer.empty() ? Colors::TEXT_MUTED : Colors::TEXT_PRIMARY);

            bool addBtnHover = mousePos.x >= LEFT_SIDEBAR_WIDTH - 80 && mousePos.x <= LEFT_SIDEBAR_WIDTH - 15 &&
                             mousePos.y >= ly && mousePos.y <= ly + 35;

            sf::RectangleShape addBtn(sf::Vector2f(60, 35));
            addBtn.setPosition(LEFT_SIDEBAR_WIDTH - 80, ly);
            addBtn.setFillColor(hexToColor(addBtnHover ? Colors::BUTTON_HOVER : Colors::BUTTON_DEFAULT));
            addBtn.setOutlineColor(hexToColor(Colors::SUCCESS));
            addBtn.setOutlineThickness(1);
            window.draw(addBtn);

            drawText(window, font, "ADD", LEFT_SIDEBAR_WIDTH - 65, ly + 9, 11, Colors::SUCCESS, true);

            if (inputHover && clicked) {
                state.editorInputActive = true;
            } else if (clicked && mousePos.x >= LEFT_SIDEBAR_WIDTH) {

                state.editorInputActive = false;
            }

            if (addBtnHover && clicked && !state.editorInputBuffer.empty()) {

                if (multiLegBuilder.getNodeCount() < 10) {
                    multiLegBuilder.appendPort(state.editorInputBuffer);
                    state.editorInputBuffer = "";
                    state.editorShowResults = false;
                }
            }

            ly += 45;

            drawText(window, font, "ROUTE SEQUENCE:", 20, ly, 12, Colors::HIGHLIGHT, true);
            ly += 25;

            int nodeIndex = 0;
            MultiLegNode* currentNode = multiLegBuilder.getHead();

            if (!currentNode) {
                sf::RectangleShape emptyBox(sf::Vector2f(LEFT_SIDEBAR_WIDTH - 30, 50));
                emptyBox.setPosition(15, ly);
                emptyBox.setFillColor(hexToColor(0x1a1a30FF));
                emptyBox.setOutlineColor(hexToColor(Colors::BORDER));
                emptyBox.setOutlineThickness(1);
                window.draw(emptyBox);

                drawText(window, font, "No ports added yet", 25, ly + 16, 11, Colors::TEXT_MUTED);
                ly += 60;
            } else {

                while (currentNode) {
                    bool isHovered = mousePos.x >= 15 && mousePos.x <= LEFT_SIDEBAR_WIDTH - 15 &&
                                   mousePos.y >= ly && mousePos.y <= ly + 40;

                    sf::RectangleShape nodeBox(sf::Vector2f(LEFT_SIDEBAR_WIDTH - 30, 40));
                    nodeBox.setPosition(15, ly);
                    nodeBox.setFillColor(hexToColor(state.editorSelectedNodeIndex == nodeIndex ? 0x2a3a50FF :
                                                   (isHovered ? 0x252535FF : 0x1a2030FF)));

                    if (!currentNode->prev) {
                        nodeBox.setOutlineColor(hexToColor(Colors::SUCCESS));
                    } else if (!currentNode->next) {
                        nodeBox.setOutlineColor(hexToColor(Colors::SECONDARY));
                    } else {
                        nodeBox.setOutlineColor(hexToColor(Colors::BORDER));
                    }
                    nodeBox.setOutlineThickness(1);
                    window.draw(nodeBox);

                    char nodeLabel[128];
                    string cleanName = currentNode->portName;

                    if (cleanName.length() >= 3 && (unsigned char)cleanName[0] == 0xEF &&
                        (unsigned char)cleanName[1] == 0xBB && (unsigned char)cleanName[2] == 0xBF) {
                        cleanName = cleanName.substr(3);
                    }

                    size_t start = cleanName.find_first_not_of(" \t\r\n");
                    size_t end = cleanName.find_last_not_of(" \t\r\n");
                    if (start != string::npos && end != string::npos) {
                        cleanName = cleanName.substr(start, end - start + 1);
                    }
                    snprintf(nodeLabel, sizeof(nodeLabel), "%d. %s", nodeIndex + 1, cleanName.c_str());
                    drawText(window, font, nodeLabel, 25, ly + 11, 11,
                            !currentNode->prev ? Colors::SUCCESS :
                            (!currentNode->next ? Colors::SECONDARY : Colors::TEXT_PRIMARY));

                    bool canRemove = (multiLegBuilder.getNodeCount() > 1);
                    if (canRemove) {
                        float btnX = LEFT_SIDEBAR_WIDTH - 75;
                        bool removeHover = isHovered && mousePos.x >= btnX && mousePos.x <= btnX + 55;

                        sf::RectangleShape removeBtn(sf::Vector2f(55, 28));
                        removeBtn.setPosition(btnX, ly + 6);
                        removeBtn.setFillColor(hexToColor(removeHover ? Colors::DANGER : 0x402020FF));
                        window.draw(removeBtn);

                        drawText(window, font, "Remove", btnX + 5, ly + 11, 9, Colors::WARNING);

                        if (removeHover && clicked) {
                            multiLegBuilder.deleteNode(currentNode);
                            state.editorShowResults = false;

                            if (state.editorSelectedNodeIndex >= multiLegBuilder.getNodeCount()) {
                                state.editorSelectedNodeIndex = multiLegBuilder.getNodeCount() - 1;
                            }
                            clicked = false;
                            break;
                        }
                    }

                    if (clicked && isHovered && mousePos.x < LEFT_SIDEBAR_WIDTH - 80) {
                        state.editorSelectedNodeIndex = nodeIndex;
                        clicked = false;
                    }

                    ly += 45;

                    if (currentNode->next) {

                        bool isValid = multiLegBuilder.hasValidRoute(currentNode->portName, currentNode->next->portName);
                        unsigned int arrowColor = isValid ? Colors::HIGHLIGHT : Colors::DANGER;
                    }

                    currentNode = currentNode->next;
                    nodeIndex++;
                }
            }

            ly += 10;

            bool clearHover = mousePos.x >= 15 && mousePos.x <= LEFT_SIDEBAR_WIDTH - 15 &&
                            mousePos.y >= ly && mousePos.y <= ly + 35;

            sf::RectangleShape clearBtn(sf::Vector2f(LEFT_SIDEBAR_WIDTH - 30, 35));
            clearBtn.setPosition(15, ly);
            clearBtn.setFillColor(hexToColor(clearHover ? Colors::DANGER : 0x402020FF));
            clearBtn.setOutlineColor(hexToColor(Colors::WARNING));
            clearBtn.setOutlineThickness(1);
            window.draw(clearBtn);

            drawText(window, font, "CLEAR ALL", LEFT_SIDEBAR_WIDTH / 2 - 35, ly + 9, 12, Colors::WARNING);

            if (clearHover && clicked) {
                multiLegBuilder.clear();
                state.editorShowResults = false;
                state.editorSelectedNodeIndex = -1;
                state.editorSegmentCount = 0;
            }

            ly += 45;

            if (multiLegBuilder.getNodeCount() >= 2) {
                bool findRouteHover = mousePos.x >= 15 && mousePos.x <= LEFT_SIDEBAR_WIDTH - 15 &&
                                    mousePos.y >= ly && mousePos.y <= ly + 50;

                if (findRouteHover) {
                    sf::RectangleShape glow(sf::Vector2f(LEFT_SIDEBAR_WIDTH - 24, 56));
                    glow.setPosition(12, ly - 3);
                    glow.setFillColor(sf::Color(0, 200, 255, 40));
                    window.draw(glow);
                }

                sf::RectangleShape findBtn(sf::Vector2f(LEFT_SIDEBAR_WIDTH - 30, 50));
                findBtn.setPosition(15, ly);
                findBtn.setFillColor(hexToColor(findRouteHover ? 0x00cc66FF : 0x00aa55FF));
                findBtn.setOutlineColor(hexToColor(Colors::SUCCESS));
                findBtn.setOutlineThickness(2);
                window.draw(findBtn);

                drawText(window, font, "FIND ROUTE (DIJKSTRA)", LEFT_SIDEBAR_WIDTH / 2 - 85, ly + 17, 12, Colors::DARK_BG, true);

                if (findRouteHover && clicked) {

                    MultiLegRouteBuilder::SegmentResult results[10];
                    int resultCount = 0;
                    multiLegBuilder.findCompleteRoute(results, resultCount);

                    state.editorSegmentCount = resultCount;
                    for (int i = 0; i < resultCount && i < 10; i++) {
                        state.editorSegmentResults[i].valid = results[i].valid;
                        state.editorSegmentResults[i].fromPort = results[i].fromPort;
                        state.editorSegmentResults[i].toPort = results[i].toPort;
                        state.editorSegmentResults[i].cost = results[i].cost;
                        state.editorSegmentResults[i].legs = results[i].legs;
                        state.editorSegmentResults[i].errorMessage = results[i].errorMessage;
                    }

                    state.editorShowResults = true;

                    bool allValid = true;
                    for (int i = 0; i < resultCount; i++) {
                        if (!results[i].valid) {
                            allValid = false;
                            state.statusMessage = results[i].errorMessage;
                            state.isError = true;
                            break;
                        }
                    }

                    if (allValid) {
                        int totalCost = 0;
                        int totalLegs = 0;
                        for (int i = 0; i < resultCount; i++) {
                            totalCost += results[i].cost;
                            totalLegs += results[i].legs;
                        }

                        state.journeyPortCount = 0;
                        for (int seg = 0; seg < resultCount && state.journeyPortCount < 50; seg++) {

                            for (int p = 0; p < results[seg].pathPortCount && state.journeyPortCount < 50; p++) {
                                if (seg > 0 && p == 0) continue;
                                state.journeyPorts[state.journeyPortCount++] = results[seg].pathPorts[p];
                            }
                        }

                        char msg[256];
                        snprintf(msg, sizeof(msg), "Route found! Cost: $%d, Legs: %d", totalCost, totalLegs);
                        state.statusMessage = msg;
                        state.isError = false;
                    }
                }
            }

            ly += 60;

            if (!state.statusMessage.empty()) {
                unsigned int statusBg = state.isError ? 0x3a1a1aFF : 0x1a3a1aFF;
                unsigned int statusColor = state.isError ? Colors::DANGER : Colors::SUCCESS;

                sf::RectangleShape statusBar(sf::Vector2f(LEFT_SIDEBAR_WIDTH - 30, 50));
                statusBar.setPosition(15, ly);
                statusBar.setFillColor(hexToColor(statusBg));
                statusBar.setOutlineColor(hexToColor(statusColor));
                statusBar.setOutlineThickness(1);
                window.draw(statusBar);

                drawText(window, font, state.statusMessage.substr(0, 40), 25, ly + 10, 10, statusColor);
                if (state.statusMessage.length() > 40) {
                    drawText(window, font, state.statusMessage.substr(40), 25, ly + 26, 10, statusColor);
                }
            }

            float rx = (float)(WINDOW_WIDTH - RIGHT_SIDEBAR_WIDTH);
            drawPanel(window, font, rx, 0, (float)RIGHT_SIDEBAR_WIDTH, (float)WINDOW_HEIGHT,
                      "Route Results", Colors::PANEL_BG, Colors::PANEL_HEADER);

            float ry = 60;

            if (state.editorShowResults && state.editorSegmentCount > 0) {
                drawText(window, font, "DIJKSTRA OPTIMIZATION RESULTS", rx + 25, ry, 13, Colors::HIGHLIGHT, true);
                ry += 30;

                for (int i = 0; i < state.editorSegmentCount; i++) {
                    const auto& seg = state.editorSegmentResults[i];

                    sf::RectangleShape segBox(sf::Vector2f(RIGHT_SIDEBAR_WIDTH - 50, 80));
                    segBox.setPosition(rx + 25, ry);
                    segBox.setFillColor(hexToColor(seg.valid ? 0x1a2a20FF : 0x2a1a1aFF));
                    segBox.setOutlineColor(hexToColor(seg.valid ? Colors::SUCCESS : Colors::DANGER));
                    segBox.setOutlineThickness(2);
                    window.draw(segBox);

                    char segTitle[128];
                    snprintf(segTitle, sizeof(segTitle), "Segment %d: %s -> %s",
                            i + 1, seg.fromPort.c_str(), seg.toPort.c_str());
                    drawText(window, font, segTitle, rx + 35, ry + 8, 11, Colors::TEXT_PRIMARY, true);

                    if (seg.valid) {
                        char costStr[64];
                        snprintf(costStr, sizeof(costStr), "Cost: $%d | Legs: %d", seg.cost, seg.legs);
                        drawText(window, font, costStr, rx + 35, ry + 28, 10, Colors::SUCCESS);
                        drawText(window, font, "| Route found", rx + 35, ry + 48, 10, Colors::SUCCESS);
                    } else {
                        drawText(window, font, "X NO ROUTE FOUND", rx + 35, ry + 28, 10, Colors::DANGER, true);
                        drawText(window, font, "Invalid segment!", rx + 35, ry + 48, 9, Colors::WARNING);
                    }

                    ry += 90;
                }

                ry += 10;
                drawText(window, font, "TOTAL JOURNEY", rx + 25, ry, 13, Colors::HIGHLIGHT, true);
                ry += 25;

                int totalCost = 0;
                int totalLegs = 0;
                bool allValid = true;

                for (int i = 0; i < state.editorSegmentCount; i++) {
                    if (state.editorSegmentResults[i].valid) {
                        totalCost += state.editorSegmentResults[i].cost;
                        totalLegs += state.editorSegmentResults[i].legs;
                    } else {
                        allValid = false;
                    }
                }

                if (allValid) {
                    char totalStr[128];
                    snprintf(totalStr, sizeof(totalStr), "Total Cost: $%d", totalCost);
                    drawText(window, font, totalStr, rx + 35, ry, 12, Colors::SUCCESS, true);
                    ry += 25;

                    snprintf(totalStr, sizeof(totalStr), "Total Legs: %d", totalLegs);
                    drawText(window, font, totalStr, rx + 35, ry, 12, Colors::SUCCESS);
                    ry += 25;

                    snprintf(totalStr, sizeof(totalStr), "Segments: %d", state.editorSegmentCount);
                    drawText(window, font, totalStr, rx + 35, ry, 12, Colors::INFO);
                } else {
                    drawText(window, font, "Journey has invalid segments!", rx + 35, ry, 11, Colors::DANGER, true);
                    ry += 20;
                    drawText(window, font, "Fix route to continue", rx + 35, ry, 10, Colors::WARNING);
                }
            } else {
                drawText(window, font, "No route calculated yet", rx + 35, ry, 11, Colors::TEXT_MUTED);
                ry += 25;
                drawText(window, font, "Add ports and click", rx + 35, ry, 10, Colors::TEXT_MUTED);
                ry += 18;
                drawText(window, font, "'Find Route' to optimize", rx + 35, ry, 10, Colors::TEXT_MUTED);
            }

            MultiLegNode* node = multiLegBuilder.getHead();
            int segIndex = 0;

            while (node && node->next) {
                float x1, y1, x2, y2;
                if (getPortCoords(node->portName, x1, y1) &&
                    getPortCoords(node->next->portName, x2, y2)) {

                    bool isValid = multiLegBuilder.hasValidRoute(node->portName, node->next->portName);

                    sf::Color legColor;
                    if (!isValid) {
                        legColor = hexToColor(Colors::DANGER);
                    } else if (state.editorSelectedNodeIndex == segIndex) {
                        legColor = hexToColor(Colors::HIGHLIGHT);
                    } else {
                        legColor = hexToColor(Colors::SUCCESS);
                    }

                    float thickness = (state.editorSelectedNodeIndex == segIndex) ? 6.0f : 4.0f;

                    drawThickLineWithArrow(window, x1, y1, x2, y2, thickness, legColor);

                    if (state.editorSelectedNodeIndex == segIndex) {
                        drawThickLineWithArrow(window, x1, y1, x2, y2, 10.0f,
                                             sf::Color(0, 200, 255, 60));
                    }

                    if (!isValid) {
                        float mx = (x1 + x2) / 2;
                        float my = (y1 + y2) / 2;

                        sf::CircleShape warning(15);
                        warning.setPosition(mx - 15, my - 15);
                        warning.setFillColor(hexToColor(Colors::DANGER));
                        warning.setOutlineColor(sf::Color::White);
                        warning.setOutlineThickness(2);
                        window.draw(warning);

                        sf::Text warnText("!", font, 20);
                        sf::FloatRect wb = warnText.getLocalBounds();
                        warnText.setPosition(mx - wb.width / 2, my - wb.height / 2 - 3);
                        warnText.setFillColor(sf::Color::White);
                        warnText.setStyle(sf::Text::Bold);
                        window.draw(warnText);
                    }
                }

                node = node->next;
                segIndex++;
            }

            node = multiLegBuilder.getHead();
            nodeIndex = 0;

            while (node) {
                float px, py;
                if (getPortCoords(node->portName, px, py)) {
                    float radius = (!node->prev) ? 14.0f : (!node->next ? 12.0f : 10.0f);

                    sf::CircleShape glow(radius + 6);
                    glow.setPosition(px - radius - 6, py - radius - 6);
                    glow.setFillColor(sf::Color(0, 200, 255, 50));
                    window.draw(glow);

                    sf::CircleShape portCircle(radius);
                    portCircle.setPosition(px - radius, py - radius);
                    portCircle.setFillColor(hexToColor(!node->prev ? Colors::SUCCESS :
                                                      (!node->next ? Colors::SECONDARY : Colors::ACCENT)));
                    portCircle.setOutlineColor(sf::Color::White);
                    portCircle.setOutlineThickness(2);
                    window.draw(portCircle);

                    char numStr[8];
                    snprintf(numStr, sizeof(numStr), "%d", nodeIndex + 1);
                    sf::Text numText(numStr, font, (unsigned int)(radius * 1.2f));
                    sf::FloatRect numBounds = numText.getLocalBounds();
                    numText.setPosition(px - numBounds.width / 2, py - numBounds.height / 2 - radius / 4);
                    numText.setFillColor(hexToColor(Colors::DARK_BG));
                    numText.setStyle(sf::Text::Bold);
                    window.draw(numText);
                }

                node = node->next;
                nodeIndex++;
            }

            const float FOOTER_HEIGHT = (state.appState == AppState::MULTILEG_EDITOR) ? 0.0f : 45.0f;
            const float PANEL_MARGIN = 12.0f;
            const float PANEL_HEIGHT = 120.0f;

            float routePanelX = LEFT_SIDEBAR_WIDTH + PANEL_MARGIN;
            float routePanelW = WINDOW_WIDTH - LEFT_SIDEBAR_WIDTH - RIGHT_SIDEBAR_WIDTH - (PANEL_MARGIN * 2);
            float routePanelH = PANEL_HEIGHT;

            float routePanelY = WINDOW_HEIGHT - FOOTER_HEIGHT - routePanelH - PANEL_MARGIN;

            sf::RectangleShape panelBg(sf::Vector2f(routePanelW, routePanelH));
            panelBg.setPosition(routePanelX, routePanelY);
            panelBg.setFillColor(hexToColor(Colors::PANEL_BG));
            panelBg.setOutlineColor(hexToColor(Colors::HIGHLIGHT));
            panelBg.setOutlineThickness(2);
            window.draw(panelBg);

            float glowPulse = 0.3f + 0.2f * sin(state.pulseTimer * 2.0f);
            sf::RectangleShape panelGlow(sf::Vector2f(routePanelW + 8, routePanelH + 8));
            panelGlow.setPosition(routePanelX - 4, routePanelY - 4);
            sf::Color glowColor = hexToColor(Colors::ELECTRIC_BLUE);
            glowColor.a = (sf::Uint8)(glowPulse * 80);
            panelGlow.setFillColor(sf::Color::Transparent);
            panelGlow.setOutlineColor(glowColor);
            panelGlow.setOutlineThickness(4);
            window.draw(panelGlow);

            if (multiLegBuilder.getNodeCount() > 0) {

                drawText(window, font, "LINKED LIST ROUTE SEQUENCE", routePanelX + 20, routePanelY + 15, 15, Colors::HIGHLIGHT, true);

                string routeStr = "";
                MultiLegNode* routeNode = multiLegBuilder.getHead();
                int totalNodes = 0;
                while (routeNode) {
                    routeStr += routeNode->portName;
                    if (routeNode->next) routeStr += "  ->  ";
                    routeNode = routeNode->next;
                    totalNodes++;
                }

                sf::Text routeText(routeStr, font, 12);
                float maxTextWidth = routePanelW - 380;
                if (routeText.getLocalBounds().width > maxTextWidth) {
                    routeStr = routeStr.substr(0, 50) + "...";
                }
                drawText(window, font, routeStr, routePanelX + 320, routePanelY + 17, 12, Colors::TEXT_SECONDARY);

                char nodeCountStr[32];
                snprintf(nodeCountStr, sizeof(nodeCountStr), "%d STOPS", totalNodes);
                sf::RectangleShape badge(sf::Vector2f(90, 22));
                badge.setPosition(routePanelX + routePanelW - 110, routePanelY + 12);
                badge.setFillColor(hexToColor(Colors::SUCCESS));
                badge.setOutlineColor(sf::Color::White);
                badge.setOutlineThickness(1);
                window.draw(badge);
                drawText(window, font, nodeCountStr, routePanelX + routePanelW - 105, routePanelY + 15, 11, Colors::DARK_BG, true);

                float nodeStartX = routePanelX + 40;
                float nodeY = routePanelY + 75;
                float availableWidth = routePanelW - 80;
                float nodeSpacing = min(150.0f, availableWidth / max(1, totalNodes));

                if (totalNodes * nodeSpacing < availableWidth) {
                    nodeStartX += (availableWidth - totalNodes * nodeSpacing) / 2;
                }

                routeNode = multiLegBuilder.getHead();
                int idx = 0;

                while (routeNode) {
                    float nodeX = nodeStartX + idx * nodeSpacing;

                    if (routeNode->next) {
                        unsigned int arrowColor = Colors::LEG_COLORS[idx % Colors::LEG_COLOR_COUNT];
                        float arrowLength = nodeSpacing - 55;

                        float arrowGlow = 0.5f + 0.5f * sin(state.pulseTimer * 3.0f + idx * 0.8f);
                        sf::RectangleShape arrowGlowShape(sf::Vector2f(arrowLength, 10));
                        arrowGlowShape.setPosition(nodeX + 42, nodeY + 3);
                        sf::Color arrowGlowCol = hexToColor(arrowColor);
                        arrowGlowCol.a = (sf::Uint8)(arrowGlow * 60);
                        arrowGlowShape.setFillColor(arrowGlowCol);
                        window.draw(arrowGlowShape);

                        sf::RectangleShape arrow(sf::Vector2f(arrowLength, 5));
                        arrow.setPosition(nodeX + 42, nodeY + 5);
                        arrow.setFillColor(hexToColor(arrowColor));
                        window.draw(arrow);

                        sf::ConvexShape arrowHead(3);
                        arrowHead.setPoint(0, sf::Vector2f(0, 0));
                        arrowHead.setPoint(1, sf::Vector2f(0, 20));
                        arrowHead.setPoint(2, sf::Vector2f(16, 10));
                        arrowHead.setPosition(nodeX + arrowLength + 30, nodeY);
                        arrowHead.setFillColor(hexToColor(arrowColor));
                        window.draw(arrowHead);

                        string legLabel = "LEG " + to_string(idx + 1);
                        float legLabelX = nodeX + nodeSpacing/2 + 15;
                        float legLabelY = nodeY - 25;

                        sf::RectangleShape legBg(sf::Vector2f(55, 18));
                        legBg.setPosition(legLabelX - 5, legLabelY - 2);
                        legBg.setFillColor(hexToColor(arrowColor));
                        legBg.setOutlineColor(sf::Color::White);
                        legBg.setOutlineThickness(1);
                        window.draw(legBg);

                        drawText(window, font, legLabel, legLabelX, legLabelY, 10, Colors::DARK_BG, true);
                    }

                    unsigned int nodeColor;
                    if (!routeNode->prev) {
                        nodeColor = Colors::SUCCESS;
                    } else if (!routeNode->next) {
                        nodeColor = Colors::SECONDARY;
                    } else {
                        nodeColor = Colors::ELECTRIC_BLUE;
                    }

                    float nodePulse = 0.6f + 0.4f * sin(state.pulseTimer * 2.5f + idx * 0.5f);
                    sf::CircleShape nodeGlow(24);
                    nodeGlow.setPosition(nodeX - 8, nodeY - 14);
                    sf::Color glowCol = hexToColor(nodeColor);
                    glowCol.a = (sf::Uint8)(nodePulse * 120);
                    nodeGlow.setFillColor(glowCol);
                    window.draw(nodeGlow);

                    sf::CircleShape glowRing(18);
                    glowRing.setPosition(nodeX - 2, nodeY - 8);
                    sf::Color ringCol = hexToColor(nodeColor);
                    ringCol.a = 160;
                    glowRing.setFillColor(ringCol);
                    window.draw(glowRing);

                    sf::CircleShape nodeCircle(16);
                    nodeCircle.setPosition(nodeX - 1, nodeY - 7);
                    nodeCircle.setFillColor(hexToColor(nodeColor));
                    nodeCircle.setOutlineColor(sf::Color::White);
                    nodeCircle.setOutlineThickness(3);
                    window.draw(nodeCircle);

                    char numStr[8];
                    snprintf(numStr, sizeof(numStr), "%d", idx + 1);
                    sf::Text numText(numStr, font, 13);
                    sf::FloatRect numBounds = numText.getLocalBounds();
                    numText.setPosition(nodeX + 15 - numBounds.width / 2, nodeY + 3 - numBounds.height / 2 - 5);
                    numText.setFillColor(hexToColor(Colors::DARK_BG));
                    numText.setStyle(sf::Text::Bold);
                    window.draw(numText);

                    string portName = routeNode->portName;

                    if (portName.length() > 12) {
                        portName = portName.substr(0, 10) + "..";
                    }

                    sf::Text portLabel(portName, font, 12);
                    sf::FloatRect lb = portLabel.getLocalBounds();
                    float labelX = nodeX + 15 - lb.width / 2;

                    sf::Text shadow(portName, font, 12);
                    shadow.setPosition(labelX + 1, nodeY + 26);
                    shadow.setFillColor(sf::Color(0, 0, 0, 180));
                    shadow.setStyle(sf::Text::Bold);
                    window.draw(shadow);

                    portLabel.setPosition(labelX, nodeY + 25);
                    portLabel.setFillColor(hexToColor(Colors::TEXT_PRIMARY));
                    portLabel.setStyle(sf::Text::Bold);
                    window.draw(portLabel);

                    routeNode = routeNode->next;
                    idx++;
                }
            } else {

                sf::Text emptyIcon("", font, 40);
                sf::FloatRect iconBounds = emptyIcon.getLocalBounds();
                emptyIcon.setPosition(routePanelX + routePanelW/2 - iconBounds.width/2, routePanelY + 30);
                emptyIcon.setFillColor(hexToColor(Colors::TEXT_MUTED));
                window.draw(emptyIcon);

                sf::Text noRouteText("Add ports and find route to visualize the multi-leg linked list here", font, 15);
                sf::FloatRect bounds = noRouteText.getLocalBounds();
                noRouteText.setPosition(routePanelX + (routePanelW - bounds.width) / 2, routePanelY + 85);
                noRouteText.setFillColor(hexToColor(Colors::TEXT_MUTED));
                window.draw(noRouteText);
            }
        }

        if (state.appState == AppState::COMPANY_VIEWER) {

            if (state.currentView != VIEW_COMPANY_ROUTES) {
                state.currentView = VIEW_COMPANY_ROUTES;
                collectAllCompanies(graph, state);
            }
        }

        if (state.appState == AppState::DOCKING_MANAGER) {

            if (state.dockingSimPlaying) {
                dockingManager.updateSimulation();
            }

            if (hasMap) {
                window.draw(mapSprite);
            }

            sf::RectangleShape leftBg(sf::Vector2f((float)LEFT_SIDEBAR_WIDTH, (float)WINDOW_HEIGHT));
            leftBg.setPosition(0, 0);
            leftBg.setFillColor(sf::Color(12, 15, 25, 255));
            window.draw(leftBg);

            sf::RectangleShape titleBar(sf::Vector2f((float)LEFT_SIDEBAR_WIDTH, 45));
            titleBar.setPosition(0, 0);
            titleBar.setFillColor(sf::Color(20, 25, 40, 255));
            window.draw(titleBar);

            sf::Text panelTitle("DOCKING SIMULATION", font, 13);
            panelTitle.setStyle(sf::Text::Bold);
            panelTitle.setPosition(15, 14);
            panelTitle.setFillColor(hexToColor(Colors::TEXT_PRIMARY));
            panelTitle.setLetterSpacing(1.3f);
            window.draw(panelTitle);

            float ly = 55.0f;
            float cardMargin = 12.0f;
            float cardWidth = LEFT_SIDEBAR_WIDTH - cardMargin * 2;

            float card1H = 120.0f;
            drawCard(window, cardMargin, ly, cardWidth, card1H, Colors::HIGHLIGHT);
            drawSectionHeader(window, font, "SELECT PORT", cardMargin + 10, ly + 8, cardWidth - 20, Colors::HIGHLIGHT);

            float cy = ly + 35;

            drawText(window, font, "Port", cardMargin + 15, cy, 9, Colors::TEXT_MUTED);
            cy += 16;

            string selectedPort = state.selectedDockingPort.empty() ? "Select a port..." : state.selectedDockingPort;
            bool portHover = mousePos.x >= cardMargin + 10 && mousePos.x <= cardMargin + cardWidth - 10 &&
                            mousePos.y >= cy && mousePos.y <= cy + 34;

            drawInputField(window, font, selectedPort, cardMargin + 10, cy, cardWidth - 20, 34,
                          state.originDropdownOpen, portHover);

            if (portHover && clicked && !state.destDropdownOpen) {
                state.originDropdownOpen = !state.originDropdownOpen;
                state.dropdownScroll = 0;
            }

            float portDropdownY = cy + 36;
            float portDropdownX = cardMargin + 10;

            drawText(window, font, "click port on map", cardMargin + 15, ly + card1H - 18, 8, Colors::TEXT_MUTED);

            ly += card1H + 10;

            float card2H = 90.0f;
            drawCard(window, cardMargin, ly, cardWidth, card2H, Colors::SECONDARY);
            drawSectionHeader(window, font, "SIMULATION DATE", cardMargin + 10, ly + 8, cardWidth - 20, Colors::SECONDARY);

            cy = ly + 35;

            float dateFieldW = (cardWidth - 40) / 3 - 4;

            drawText(window, font, "Day", cardMargin + 15, cy, 8, Colors::TEXT_MUTED);
            drawText(window, font, "Month", cardMargin + 25 + dateFieldW, cy, 8, Colors::TEXT_MUTED);
            drawText(window, font, "Year", cardMargin + 35 + dateFieldW * 2, cy, 8, Colors::TEXT_MUTED);
            cy += 14;

            string dayStr = (state.activeField == UIState::DAY) ? state.inputBuffer + "|" : to_string(state.day);
            bool dayHover = mousePos.x >= cardMargin + 10 && mousePos.x <= cardMargin + 10 + dateFieldW &&
                           mousePos.y >= cy && mousePos.y <= cy + 28;
            drawInputField(window, font, dayStr, cardMargin + 10, cy, dateFieldW, 28,
                          state.activeField == UIState::DAY, dayHover);
            if (dayHover && clicked) {
                state.activeField = UIState::DAY;
                state.inputBuffer = "";
            }

            string monthStr = (state.activeField == UIState::MONTH) ? state.inputBuffer + "|" : to_string(state.month);
            bool monthHover = mousePos.x >= cardMargin + 20 + dateFieldW && mousePos.x <= cardMargin + 20 + dateFieldW * 2 &&
                             mousePos.y >= cy && mousePos.y <= cy + 28;
            drawInputField(window, font, monthStr, cardMargin + 20 + dateFieldW, cy, dateFieldW, 28,
                          state.activeField == UIState::MONTH, monthHover);
            if (monthHover && clicked) {
                state.activeField = UIState::MONTH;
                state.inputBuffer = "";
            }

            string yearStr = (state.activeField == UIState::YEAR) ? state.inputBuffer + "|" : to_string(state.year);
            bool yearHover = mousePos.x >= cardMargin + 30 + dateFieldW * 2 && mousePos.x <= cardMargin + 30 + dateFieldW * 3 &&
                            mousePos.y >= cy && mousePos.y <= cy + 28;
            drawInputField(window, font, yearStr, cardMargin + 30 + dateFieldW * 2, cy, dateFieldW, 28,
                          state.activeField == UIState::YEAR, yearHover);
            if (yearHover && clicked) {
                state.activeField = UIState::YEAR;
                state.inputBuffer = "";
            }

            ly += card2H + 10;

            float card3H = 210.0f;
            drawCard(window, cardMargin, ly, cardWidth, card3H, Colors::INFO);
            drawSectionHeader(window, font, "TIME CONTROLS", cardMargin + 10, ly + 8, cardWidth - 20, Colors::INFO);

            cy = ly + 40;

            char timeStr[20];
            dockingManager.getFormattedTime(timeStr);
            drawText(window, font, "CURRENT TIME", cardMargin + 15, cy, 11, Colors::TEXT_SECONDARY, true);
            cy += 25;

            sf::RectangleShape timeBg(sf::Vector2f(cardWidth - 40, 45));
            timeBg.setPosition(cardMargin + 20, cy);
            timeBg.setFillColor(hexToColor(Colors::BUTTON_BG));
            timeBg.setOutlineColor(hexToColor(Colors::ACCENT));
            timeBg.setOutlineThickness(2);
            window.draw(timeBg);

            drawText(window, font, timeStr, cardMargin + cardWidth / 2 - 25, cy + 10, 22, Colors::HIGHLIGHT, true);
            cy += 65;

            float btnW = (cardWidth - 60) / 3;
            bool playBtnHover = mousePos.x >= cardMargin + 20 && mousePos.x <= cardMargin + 20 + btnW &&
                               mousePos.y >= cy && mousePos.y <= cy + 40;

            sf::RectangleShape playBtn(sf::Vector2f(btnW, 40));
            playBtn.setPosition(cardMargin + 20, cy);
            playBtn.setFillColor(hexToColor(playBtnHover ? Colors::BUTTON_HOVER : Colors::BUTTON_BG));
            playBtn.setOutlineColor(hexToColor(state.dockingSimPlaying ? Colors::SUCCESS : Colors::HIGHLIGHT));
            playBtn.setOutlineThickness(2);
            window.draw(playBtn);

            drawText(window, font, state.dockingSimPlaying ? "PAUSE" : "PLAY",
                    cardMargin + 20 + btnW / 2 - 20, cy + 12, 11, Colors::TEXT_PRIMARY);

            if (playBtnHover && clicked) {
                state.dockingSimPlaying = !state.dockingSimPlaying;
                if (state.dockingSimPlaying) {
                    dockingManager.play();
                } else {
                    dockingManager.pause();
                }
            }

            bool stepBackHover = mousePos.x >= cardMargin + 30 + btnW && mousePos.x <= cardMargin + 30 + btnW * 2 &&
                                mousePos.y >= cy && mousePos.y <= cy + 40;

            sf::RectangleShape stepBackBtn(sf::Vector2f(btnW, 40));
            stepBackBtn.setPosition(cardMargin + 30 + btnW, cy);
            stepBackBtn.setFillColor(hexToColor(stepBackHover ? Colors::BUTTON_HOVER : Colors::BUTTON_BG));
            stepBackBtn.setOutlineColor(hexToColor(Colors::BORDER));
            stepBackBtn.setOutlineThickness(2);
            window.draw(stepBackBtn);
            drawText(window, font, "<<", cardMargin + 30 + btnW + btnW / 2 - 10, cy + 12, 11, Colors::TEXT_PRIMARY);

            if (stepBackHover && clicked) {
                dockingManager.stepBackward(30);
            }

            bool stepFwdHover = mousePos.x >= cardMargin + 40 + btnW * 2 && mousePos.x <= cardMargin + 40 + btnW * 3 &&
                               mousePos.y >= cy && mousePos.y <= cy + 40;

            sf::RectangleShape stepFwdBtn(sf::Vector2f(btnW, 40));
            stepFwdBtn.setPosition(cardMargin + 40 + btnW * 2, cy);
            stepFwdBtn.setFillColor(hexToColor(stepFwdHover ? Colors::BUTTON_HOVER : Colors::BUTTON_BG));
            stepFwdBtn.setOutlineColor(hexToColor(Colors::BORDER));
            stepFwdBtn.setOutlineThickness(2);
            window.draw(stepFwdBtn);
            drawText(window, font, ">>", cardMargin + 40 + btnW * 2 + btnW / 2 - 10, cy + 12, 11, Colors::TEXT_PRIMARY);

            if (stepFwdHover && clicked) {
                dockingManager.stepForward(30);
            }

            cy += 45;

            bool resetBtnHover = mousePos.x >= cardMargin + 20 && mousePos.x <= cardMargin + cardWidth - 20 &&
                                mousePos.y >= cy && mousePos.y <= cy + 32;

            sf::RectangleShape resetBtn(sf::Vector2f(cardWidth - 40, 32));
            resetBtn.setPosition(cardMargin + 20, cy);
            resetBtn.setFillColor(hexToColor(resetBtnHover ? Colors::BUTTON_HOVER : Colors::BUTTON_BG));
            resetBtn.setOutlineColor(hexToColor(Colors::WARNING));
            resetBtn.setOutlineThickness(2);
            window.draw(resetBtn);
            drawText(window, font, "RESET SIMULATION", cardMargin + cardWidth / 2 - 55, cy + 9, 11, Colors::TEXT_PRIMARY);

            if (resetBtnHover && clicked) {
                dockingManager.reset();
                dockingManager.loadRoutesForDate("Routes.txt", state.day, state.month, state.year);
                state.dockingSimPlaying = false;
            }

            ly += card3H + 10;

            float card4H = 60.0f;
            bool backBtnHover = mousePos.x >= cardMargin && mousePos.x <= cardMargin + cardWidth &&
                               mousePos.y >= ly && mousePos.y <= ly + card4H;

            sf::RectangleShape backCard(sf::Vector2f(cardWidth, card4H));
            backCard.setPosition(cardMargin, ly);
            backCard.setFillColor(hexToColor(backBtnHover ? 0x2a2a55FF : 0x1e1e42FF));
            backCard.setOutlineColor(hexToColor(backBtnHover ? Colors::HIGHLIGHT : Colors::BORDER));
            backCard.setOutlineThickness(2);
            window.draw(backCard);

            drawText(window, font, "<- BACK TO MAIN MENU", cardMargin + 60, ly + 21, 12,
                    backBtnHover ? Colors::HIGHLIGHT : Colors::TEXT_PRIMARY);

            if (backBtnHover && clicked) {
                state.appState = AppState::MAIN_MENU;
                state.dockingSimPlaying = false;
                dockingManager.pause();
            }

            PortDockingData* portNode = dockingManager.getAllPortData();
            while (portNode) {
                float px, py;
                if (!getPortCoords(portNode->portName, px, py)) {
                    portNode = portNode->next;
                    continue;
                }

                bool isHovered = (px - mousePos.x) * (px - mousePos.x) +
                                (py - mousePos.y) * (py - mousePos.y) < 100;
                bool isSelected = state.selectedDockingPort == portNode->portName;

                float portRadius = isSelected ? 12.0f : (isHovered ? 10.0f : 8.0f);
                sf::CircleShape portCircle(portRadius);
                portCircle.setPosition(px - portRadius, py - portRadius);
                portCircle.setFillColor(hexToColor(isSelected ? Colors::ACCENT :
                                                (isHovered ? Colors::HIGHLIGHT : Colors::PORT_COLOR)));
                portCircle.setOutlineColor(hexToColor(isSelected ? Colors::ACCENT : Colors::HIGHLIGHT));
                portCircle.setOutlineThickness(isSelected ? 2.0f : 1.0f);
                window.draw(portCircle);

                drawText(window, font, portNode->portName, px - 30, py + 15, 9, Colors::TEXT_SECONDARY);

                if (isHovered && clicked) {
                    state.selectedDockingPort = portNode->portName;

                    if (!state.dockingSimPlaying) {
                        state.dockingSimPlaying = true;
                        dockingManager.play();
                    }
                }

                portNode = portNode->next;
            }

            DockingShip shipsInTransit[100];
            int shipCount = dockingManager.getShipsInTransit(shipsInTransit, 100);
            for (int i = 0; i < shipCount; i++) {
                DockingShip& ship = shipsInTransit[i];

                bool isRelatedToPort = false;
                if (!state.selectedDockingPort.empty()) {
                    isRelatedToPort = (strcmp(ship.originPort, state.selectedDockingPort.c_str()) == 0 ||
                                      strcmp(ship.destinationPort, state.selectedDockingPort.c_str()) == 0);
                } else {
                    isRelatedToPort = true;
                }

                if (!isRelatedToPort) continue;

                float ox, oy, dx, dy;
                if (getPortCoords(ship.originPort, ox, oy) &&
                    getPortCoords(ship.destinationPort, dx, dy)) {

                    float shipX = ox + (dx - ox) * ship.animationProgress;
                    float shipY = oy + (dy - oy) * ship.animationProgress;

                    sf::Vertex routeLine[] = {sf::Vertex(sf::Vector2f(ox, oy), hexToColor(0xFFFFFF22)), sf::Vertex(sf::Vector2f(dx, dy), hexToColor(0xFFFFFF22))};
                    window.draw(routeLine, 2, sf::Lines);

                    float angle = atan2(dy - oy, dx - ox) * 180.0f / 3.14159f;
                    sf::CircleShape shipIcon(6, 3);
                    shipIcon.setPosition(shipX - 6, shipY - 6);
                    shipIcon.setRotation(angle + 90);
                    shipIcon.setFillColor(hexToColor(Colors::JOURNEY_COLOR));
                    shipIcon.setOutlineColor(hexToColor(Colors::HIGHLIGHT));
                    shipIcon.setOutlineThickness(1);
                    window.draw(shipIcon);

                    if (abs(mousePos.x - shipX) < 15 && abs(mousePos.y - shipY) < 15) {
                        char shipInfo[256];
                        snprintf(shipInfo, sizeof(shipInfo), "%s -> %s",
                                ship.originPort, ship.destinationPort);
                        drawText(window, font, shipInfo, shipX + 10, shipY - 20, 9, Colors::HIGHLIGHT);

                        snprintf(shipInfo, sizeof(shipInfo), "%s (%s)",
                                ship.company, ship.shipType);
                        drawText(window, font, shipInfo, shipX + 10, shipY - 5, 8, Colors::TEXT_SECONDARY);

                        snprintf(shipInfo, sizeof(shipInfo), "Dep: %02d/%02d/%04d %02d:%02d",
                                ship.departureDay, ship.departureMonth, ship.departureYear,
                                ship.departureTimeMinutes / 60, ship.departureTimeMinutes % 60);
                        drawText(window, font, shipInfo, shipX + 10, shipY + 10, 8, Colors::TEXT_MUTED);
                    }
                }
            }

            if (!state.selectedDockingPort.empty()) {
                PortDockingData* portData = dockingManager.getPortData(state.selectedDockingPort.c_str());
                if (portData) {
                    float rightPanelW = (float)RIGHT_SIDEBAR_WIDTH;
                    float rightPanelX = WINDOW_WIDTH - rightPanelW;

                    sf::RectangleShape rightBg(sf::Vector2f(rightPanelW, (float)WINDOW_HEIGHT));
                    rightBg.setPosition(rightPanelX, 0);
                    rightBg.setFillColor(sf::Color(12, 15, 25, 255));
                    window.draw(rightBg);

                    sf::RectangleShape rightTitleBar(sf::Vector2f(rightPanelW, 45));
                    rightTitleBar.setPosition(rightPanelX, 0);
                    rightTitleBar.setFillColor(sf::Color(20, 25, 40, 255));
                    window.draw(rightTitleBar);

                    sf::Text rightTitle(state.selectedDockingPort, font, 13);
                    rightTitle.setStyle(sf::Text::Bold);
                    rightTitle.setPosition(rightPanelX + 15, 14);
                    rightTitle.setFillColor(hexToColor(Colors::HIGHLIGHT));
                    rightTitle.setLetterSpacing(1.3f);
                    window.draw(rightTitle);

                    float ry = 55.0f;
                    float rMargin = 12.0f;
                    float rCardWidth = rightPanelW - rMargin * 2;

                    int totalInQueue = portData->getTotalWaiting();

                    char docksHeader[128];
                    snprintf(docksHeader, sizeof(docksHeader), "CURRENT DOCKS (%d docked ships)", totalInQueue);

                    float rCard1MaxH = (float)WINDOW_HEIGHT - ry - 20;
                    drawCard(window, rightPanelX + rMargin, ry, rCardWidth, rCard1MaxH, Colors::WARNING);
                    drawSectionHeader(window, font, docksHeader, rightPanelX + rMargin + 10, ry + 8, rCardWidth - 20, Colors::WARNING);

                    float detailY = ry + 40;

                    int totalDisplayed = 0;
                    CompanyQueueNode* cNode = portData->companyHead;
                    while (cNode && totalDisplayed < 12) {
                        TypeQueueNode* tNode = cNode->typeHead;
                        while (tNode && totalDisplayed < 12) {
                            if (tNode->queue.size > 0) {
                                char queueText[256];
                                snprintf(queueText, sizeof(queueText), "%s - %s: %d ships",
                                        cNode->company, tNode->shipType, tNode->queue.size);
                                drawText(window, font, queueText, rightPanelX + rMargin + 20, detailY, 10, Colors::TEXT_SECONDARY);
                                detailY += 20;
                                totalDisplayed++;
                            }
                            tNode = tNode->next;
                        }
                        cNode = cNode->next;
                    }

                    if (portData->getTotalWaiting() == 0) {
                        drawText(window, font, "No ships waiting", rightPanelX + rMargin + 20, detailY, 10, Colors::TEXT_MUTED);
                    }
                }
            }

            if (state.originDropdownOpen) {
                int totalPorts = portCount;
                int maxVisible = 9;
                float dropdownH = maxVisible * 32.0f;
                float dropdownW = LEFT_SIDEBAR_WIDTH - cardMargin * 2 - 20;

                sf::RectangleShape dropdownBg(sf::Vector2f(dropdownW, dropdownH));
                dropdownBg.setPosition(portDropdownX, portDropdownY);
                dropdownBg.setFillColor(hexToColor(Colors::DROPDOWN_BG));
                dropdownBg.setOutlineColor(hexToColor(Colors::DROPDOWN_BORDER));
                dropdownBg.setOutlineThickness(2);
                window.draw(dropdownBg);

                int startIdx = state.dropdownScroll;
                int endIdx = min(totalPorts, startIdx + maxVisible);

                for (int i = startIdx; i < endIdx; i++) {
                    float itemY = portDropdownY + (i - startIdx) * 32;
                    bool itemHover = mousePos.x >= portDropdownX && mousePos.x <= portDropdownX + dropdownW &&
                                    mousePos.y >= itemY && mousePos.y <= itemY + 32;

                    if (itemHover) {
                        sf::RectangleShape itemBg(sf::Vector2f(dropdownW, 32));
                        itemBg.setPosition(portDropdownX, itemY);
                        itemBg.setFillColor(hexToColor(Colors::DROPDOWN_ITEM_HOVER));
                        window.draw(itemBg);
                    }

                    drawText(window, font, portNames[i], portDropdownX + 10, itemY + 9, 10,
                            itemHover ? Colors::HIGHLIGHT : Colors::TEXT_PRIMARY);

                    if (itemHover && clicked) {
                        state.selectedDockingPort = portNames[i];
                        state.originDropdownOpen = false;

                        if (!state.dockingSimPlaying) {
                            state.dockingSimPlaying = true;
                            dockingManager.play();
                        }
                    }
                }
            }
        }

        if (state.appState == AppState::MAIN_MENU || state.appState == AppState::ROUTE_PLANNER || state.appState == AppState::COMPANY_VIEWER) {
            if (state.originDropdownOpen) {
                float dropY = 65 + 28 + 44 + 5;
                drawDropdown(window, font, portNames, portCount, state.dropdownScroll,
                            18, dropY, (float)(LEFT_SIDEBAR_WIDTH - 36), state.hoveredDropdownItem,
                            mousePos, clicked, state.originIndex, state.originPort, state.originDropdownOpen);
            }

            if (state.destDropdownOpen) {
                float dropY = 65 + 28 + 44 + 55 + 28 + 44 + 5;
                drawDropdown(window, font, portNames, portCount, state.dropdownScroll,
                            18, dropY, (float)(LEFT_SIDEBAR_WIDTH - 36), state.hoveredDropdownItem,
                            mousePos, clicked, state.destIndex, state.destPort, state.destDropdownOpen);
            }

            if (state.preferencesDropdownOpen) {
                const char* companies[] = {"MaerskLine", "MSC", "CMA_CGM", "COSCO", "HapagLloyd", 
                                          "Evergreen", "ZIM", "YangMing", "PIL", "ONE"};
                int companyCount = 10;
                float prefCardY = 65 + 28 + 44 + 5 + 180 + 10 + 90 + 10 + 35 + 30;
                float dropY = prefCardY + 48;
                int maxVisible = 6;
                float dropdownH = maxVisible * 32.0f;
                float dropdownW = LEFT_SIDEBAR_WIDTH - 36;

                sf::RectangleShape dropdownBg(sf::Vector2f(dropdownW, dropdownH));
                dropdownBg.setPosition(18, dropY);
                dropdownBg.setFillColor(hexToColor(Colors::DROPDOWN_BG));
                dropdownBg.setOutlineColor(hexToColor(0xb44affFF));
                dropdownBg.setOutlineThickness(2);
                window.draw(dropdownBg);

                int startIdx = state.preferencesScrollOffset;
                int endIdx = min(companyCount, startIdx + maxVisible);

                for (int i = startIdx; i < endIdx; i++) {
                    float itemY = dropY + (i - startIdx) * 32;
                    bool itemHover = mousePos.x >= 18 && mousePos.x <= 18 + dropdownW &&
                                    mousePos.y >= itemY && mousePos.y <= itemY + 32;

                    bool isSelected = false;
                    for (int j = 0; j < state.preferredCompaniesCount; j++) {
                        if (state.preferredCompanies[j] == companies[i]) {
                            isSelected = true;
                            break;
                        }
                    }

                    if (itemHover || isSelected) {
                        sf::RectangleShape itemBg(sf::Vector2f(dropdownW, 32));
                        itemBg.setPosition(18, itemY);
                        itemBg.setFillColor(isSelected ? hexToColor(0x3a2a5aFF) : hexToColor(Colors::DROPDOWN_ITEM_HOVER));
                        window.draw(itemBg);
                    }

                    if (isSelected) {
                        sf::Text checkmark("X", font, 11);
                        checkmark.setPosition(25, itemY + 9);
                        checkmark.setFillColor(hexToColor(Colors::SUCCESS));
                        checkmark.setStyle(sf::Text::Bold);
                        window.draw(checkmark);
                    }

                    drawText(window, font, companies[i], 45, itemY + 9, 10,
                            isSelected ? Colors::HIGHLIGHT : (itemHover ? Colors::TEXT_PRIMARY : Colors::TEXT_SECONDARY));

                    if (itemHover && clicked) {
                        if (isSelected) {
                            for (int j = 0; j < state.preferredCompaniesCount; j++) {
                                if (state.preferredCompanies[j] == companies[i]) {
                                    for (int k = j; k < state.preferredCompaniesCount - 1; k++) {
                                        state.preferredCompanies[k] = state.preferredCompanies[k + 1];
                                    }
                                    state.preferredCompaniesCount--;
                                    break;
                                }
                            }
                        } else {
                            if (state.preferredCompaniesCount < 5) {
                                state.preferredCompanies[state.preferredCompaniesCount++] = companies[i];
                            }
                        }
                    }
                }
            }

            if (state.avoidPortsDropdownOpen) {
                float prefCardY = 65 + 28 + 44 + 5 + 180 + 10 + 90 + 10 + 35 + 30;
                float dropY = prefCardY + 90;
                int maxVisible = 9;
                float dropdownH = maxVisible * 28.0f;
                float dropdownW = LEFT_SIDEBAR_WIDTH - 36;

                sf::RectangleShape dropdownBg(sf::Vector2f(dropdownW, dropdownH));
                dropdownBg.setPosition(18, dropY);
                dropdownBg.setFillColor(hexToColor(Colors::DROPDOWN_BG));
                dropdownBg.setOutlineColor(hexToColor(0xb44affFF));
                dropdownBg.setOutlineThickness(2);
                window.draw(dropdownBg);

                int startIdx = state.avoidPortsScrollOffset;
                int endIdx = min(portCount, startIdx + maxVisible);                for (int i = startIdx; i < endIdx; i++) {
                    float itemY = dropY + (i - startIdx) * 28;
                    bool itemHover = mousePos.x >= 18 && mousePos.x <= 18 + dropdownW &&
                                    mousePos.y >= itemY && mousePos.y <= itemY + 28;

                    bool isSelected = false;
                    for (int j = 0; j < state.avoidedPortsCount; j++) {
                        if (state.avoidedPorts[j] == portNames[i]) {
                            isSelected = true;
                            break;
                        }
                    }

                    if (itemHover || isSelected) {
                        sf::RectangleShape itemBg(sf::Vector2f(dropdownW, 28));
                        itemBg.setPosition(18, itemY);
                        itemBg.setFillColor(isSelected ? hexToColor(0x4a2a2aFF) : hexToColor(Colors::DROPDOWN_ITEM_HOVER));
                        window.draw(itemBg);
                    }

                    if (isSelected) {
                        sf::Text xmark("X", font, 11);
                        xmark.setPosition(25, itemY + 7);
                        xmark.setFillColor(hexToColor(Colors::DANGER));
                        xmark.setStyle(sf::Text::Bold);
                        window.draw(xmark);
                    }

                    drawText(window, font, portNames[i], 45, itemY + 7, 9,
                            isSelected ? Colors::DANGER : (itemHover ? Colors::TEXT_PRIMARY : Colors::TEXT_SECONDARY));

                    if (itemHover && clicked) {
                        if (isSelected) {
                            for (int j = 0; j < state.avoidedPortsCount; j++) {
                                if (state.avoidedPorts[j] == portNames[i]) {
                                    for (int k = j; k < state.avoidedPortsCount - 1; k++) {
                                        state.avoidedPorts[k] = state.avoidedPorts[k + 1];
                                    }
                                    state.avoidedPortsCount--;
                                    break;
                                }
                            }
                        } else {
                            if (state.avoidedPortsCount < 5) {
                                state.avoidedPorts[state.avoidedPortsCount++] = portNames[i];
                            }
                        }
                    }
                }
            }
        }

        if (clicked && !state.originDropdownOpen && !state.destDropdownOpen && 
            !state.preferencesDropdownOpen && !state.avoidPortsDropdownOpen) {

        } else if (clicked) {

            bool inOriginDropdown = state.originDropdownOpen &&
                                   mousePos.x >= 15 && mousePos.x <= LEFT_SIDEBAR_WIDTH - 15 &&
                                   mousePos.y >= 126 && mousePos.y <= 126 + 280;
            bool inDestDropdown = state.destDropdownOpen &&
                                 mousePos.x >= 15 && mousePos.x <= LEFT_SIDEBAR_WIDTH - 15 &&
                                 mousePos.y >= 194 && mousePos.y <= 194 + 280;
            bool inPrefDropdown = state.preferencesDropdownOpen &&
                                 mousePos.x >= 15 && mousePos.x <= LEFT_SIDEBAR_WIDTH - 15;
            bool inAvoidDropdown = state.avoidPortsDropdownOpen &&
                                  mousePos.x >= 15 && mousePos.x <= LEFT_SIDEBAR_WIDTH - 15;

            if (state.originDropdownOpen && !inOriginDropdown && mousePos.y > 126 + 36) {
                state.originDropdownOpen = false;
            }
            if (state.destDropdownOpen && !inDestDropdown && mousePos.y > 194 + 36) {
                state.destDropdownOpen = false;
            }
            if (state.preferencesDropdownOpen && !inPrefDropdown) {
                state.preferencesDropdownOpen = false;
            }
            if (state.avoidPortsDropdownOpen && !inAvoidDropdown) {
                state.avoidPortsDropdownOpen = false;
            }
        }

        if (state.hoveredEdge) {
            float tooltipW = 280;
            float tooltipH = 140;
            float tx = state.hoveredEdgeTooltipX;
            float ty = state.hoveredEdgeTooltipY;

            if (tx + tooltipW > WINDOW_WIDTH - 10) tx = WINDOW_WIDTH - tooltipW - 10;
            if (ty < 10) ty = 10;
            if (ty + tooltipH > WINDOW_HEIGHT - 10) ty = WINDOW_HEIGHT - tooltipH - 10;

            sf::RectangleShape tooltipGlow(sf::Vector2f(tooltipW + 4, tooltipH + 4));
            tooltipGlow.setPosition(tx - 2, ty - 2);
            tooltipGlow.setFillColor(sf::Color(0, 200, 255, 40));
            window.draw(tooltipGlow);

            sf::RectangleShape tooltipBg(sf::Vector2f(tooltipW, tooltipH));
            tooltipBg.setPosition(tx, ty);
            tooltipBg.setFillColor(sf::Color(15, 20, 35, 245));
            tooltipBg.setOutlineColor(sf::Color(0, 180, 255, 200));
            tooltipBg.setOutlineThickness(2);
            window.draw(tooltipBg);

            drawText(window, font, "ROUTE EDGE", tx + 10, ty + 8, 11, Colors::ELECTRIC_BLUE, true);

            float yOffset = ty + 30;

            string route = state.hoveredEdgeSource + " -> " + state.hoveredEdgeDest;
            drawText(window, font, route, tx + 10, yOffset, 12, Colors::TEXT_PRIMARY, true);
            yOffset += 25;

            if (!state.hoveredEdgeCompany.empty()) {
                drawText(window, font, "Company:", tx + 10, yOffset, 10, Colors::TEXT_MUTED);
                drawText(window, font, state.hoveredEdgeCompany, tx + 90, yOffset, 10, Colors::SUCCESS);
                yOffset += 20;
            }

            if (!state.hoveredEdgeDate.empty()) {
                drawText(window, font, "Departure:", tx + 10, yOffset, 10, Colors::TEXT_MUTED);
                string datetime = state.hoveredEdgeDate + " " + state.hoveredEdgeTime;
                drawText(window, font, datetime, tx + 90, yOffset, 10, Colors::HIGHLIGHT);
                yOffset += 20;
            }

            if (state.hoveredEdgeCost > 0) {
                drawText(window, font, "Cost:", tx + 10, yOffset, 10, Colors::TEXT_MUTED);
                char costStr[32];
                snprintf(costStr, sizeof(costStr), "$%.2f", state.hoveredEdgeCost);
                drawText(window, font, costStr, tx + 90, yOffset, 10, Colors::WARNING);
            }
        }

        window.display();
    }

    cout << "OceanRoute Nav UI closed.\n";
}
