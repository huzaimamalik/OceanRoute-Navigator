#include "RoutePreferences.h"
#include <iostream>

RoutePreferences::RoutePreferences() {
    useMaxTotalCost = false;
    maxTotalCost = 0;
    useMaxLegs = false;
    maxLegs = 3;
    allowedCompaniesCount = 0;
    forbiddenPortsCount = 0;
    preferredPortsCount = 0;
    preferCheapest = true;
    preferFastest = false;
    minLayoverMinutes = 60;
    sameDayOnly = true;
}

void initRoutePreferences(RoutePreferences& prefs) {
    prefs.useMaxTotalCost = false;
    prefs.maxTotalCost = 0;
    prefs.useMaxLegs = false;
    prefs.maxLegs = 5;
    prefs.allowedCompaniesCount = 0;
    prefs.forbiddenPortsCount = 0;
    prefs.preferredPortsCount = 0;
    prefs.preferCheapest = true;
    prefs.preferFastest = false;
    prefs.minLayoverMinutes = 60;
    prefs.sameDayOnly = true;
}

bool isPortForbidden(const RoutePreferences& prefs, const string& portName) {
    for (int i = 0; i < prefs.forbiddenPortsCount; i++) {
        if (prefs.forbiddenPorts[i] == portName) return true;
    }
    return false;
}

// Checks if shipping company is allowed (returns true if no restrictions)
bool isCompanyAllowed(const RoutePreferences& prefs, const string& companyName) {

    if (prefs.allowedCompaniesCount == 0) return true;

    for (int i = 0; i < prefs.allowedCompaniesCount; i++) {
        if (prefs.allowedCompanies[i] == companyName) return true;
    }
    return false;
}

bool passesSingleRoutePreferences(const RoutePreferences& prefs, Route* route, const string& originPort) {
    if (!route) return false;

    if (!isCompanyAllowed(prefs, route->shippingCompany)) return false;

    if (isPortForbidden(prefs, originPort)) return false;
    if (isPortForbidden(prefs, route->destinationPort)) return false;

    if (prefs.useMaxTotalCost && route->voyageCost > prefs.maxTotalCost) return false;

    if (prefs.useMaxLegs && 1 > prefs.maxLegs) return false;

    return true;
}

bool passesTwoLegRoutePreferences(const RoutePreferences& prefs, TwoLegRoute* route, const string& originPort) {
    if (!route || !route->leg1 || !route->leg2) return false;

    if (!isCompanyAllowed(prefs, route->leg1->shippingCompany)) return false;
    if (!isCompanyAllowed(prefs, route->leg2->shippingCompany)) return false;

    if (isPortForbidden(prefs, originPort)) return false;
    if (isPortForbidden(prefs, route->leg1->destinationPort)) return false;
    if (isPortForbidden(prefs, route->leg2->destinationPort)) return false;

    if (prefs.useMaxTotalCost) {
        int totalCost = route->leg1->voyageCost + route->leg2->voyageCost;
        if (totalCost > prefs.maxTotalCost) return false;
    }

    if (prefs.useMaxLegs && 2 > prefs.maxLegs) return false;

    return true;
}

bool passesThreeLegRoutePreferences(const RoutePreferences& prefs, ThreeLegRoute* route, const string& originPort) {
    if (!route || !route->leg1 || !route->leg2 || !route->leg3) return false;

    if (!isCompanyAllowed(prefs, route->leg1->shippingCompany)) return false;
    if (!isCompanyAllowed(prefs, route->leg2->shippingCompany)) return false;
    if (!isCompanyAllowed(prefs, route->leg3->shippingCompany)) return false;

    if (isPortForbidden(prefs, originPort)) return false;
    if (isPortForbidden(prefs, route->leg1->destinationPort)) return false;
    if (isPortForbidden(prefs, route->leg2->destinationPort)) return false;
    if (isPortForbidden(prefs, route->leg3->destinationPort)) return false;

    if (prefs.useMaxTotalCost) {
        int totalCost = route->leg1->voyageCost + route->leg2->voyageCost + route->leg3->voyageCost;
        if (totalCost > prefs.maxTotalCost) return false;
    }

    if (prefs.useMaxLegs && 3 > prefs.maxLegs) return false;

    return true;
}

Route* filterDirectRoutesByPreferences(const RoutePreferences& prefs, Route* inputList, const string& originPort) {
    Route* filteredHead = nullptr;
    Route* filteredTail = nullptr;

    Route* cur = inputList;
    while (cur) {
        if (passesSingleRoutePreferences(prefs, cur, originPort)) {

            Route* copy = new Route();
            copy->destinationPort = cur->destinationPort;
            copy->voyageDate = cur->voyageDate;
            copy->departureTime = cur->departureTime;
            copy->arrivalTime = cur->arrivalTime;
            copy->voyageCost = cur->voyageCost;
            copy->shippingCompany = cur->shippingCompany;
            copy->next = nullptr;

            if (!filteredHead) {
                filteredHead = copy;
                filteredTail = copy;
            } else {
                filteredTail->next = copy;
                filteredTail = copy;
            }
        }
        cur = cur->next;
    }

    return filteredHead;
}

TwoLegRoute* filterOneStopRoutesByPreferences(const RoutePreferences& prefs, TwoLegRoute* inputList, const string& originPort) {
    TwoLegRoute* filteredHead = nullptr;
    TwoLegRoute* filteredTail = nullptr;

    TwoLegRoute* cur = inputList;
    while (cur) {
        if (passesTwoLegRoutePreferences(prefs, cur, originPort)) {

            TwoLegRoute* copy = new TwoLegRoute();

            copy->leg1 = new Route();
            copy->leg1->destinationPort = cur->leg1->destinationPort;
            copy->leg1->voyageDate = cur->leg1->voyageDate;
            copy->leg1->departureTime = cur->leg1->departureTime;
            copy->leg1->arrivalTime = cur->leg1->arrivalTime;
            copy->leg1->voyageCost = cur->leg1->voyageCost;
            copy->leg1->shippingCompany = cur->leg1->shippingCompany;
            copy->leg1->next = nullptr;

            copy->leg2 = new Route();
            copy->leg2->destinationPort = cur->leg2->destinationPort;
            copy->leg2->voyageDate = cur->leg2->voyageDate;
            copy->leg2->departureTime = cur->leg2->departureTime;
            copy->leg2->arrivalTime = cur->leg2->arrivalTime;
            copy->leg2->voyageCost = cur->leg2->voyageCost;
            copy->leg2->shippingCompany = cur->leg2->shippingCompany;
            copy->leg2->next = nullptr;

            copy->next = nullptr;

            if (!filteredHead) {
                filteredHead = copy;
                filteredTail = copy;
            } else {
                filteredTail->next = copy;
                filteredTail = copy;
            }
        }
        cur = cur->next;
    }

    return filteredHead;
}

ThreeLegRoute* filterTwoStopRoutesByPreferences(const RoutePreferences& prefs, ThreeLegRoute* inputList, const string& originPort) {
    ThreeLegRoute* filteredHead = nullptr;
    ThreeLegRoute* filteredTail = nullptr;

    ThreeLegRoute* cur = inputList;
    while (cur) {
        if (passesThreeLegRoutePreferences(prefs, cur, originPort)) {

            ThreeLegRoute* copy = new ThreeLegRoute();

            copy->leg1 = new Route();
            copy->leg1->destinationPort = cur->leg1->destinationPort;
            copy->leg1->voyageDate = cur->leg1->voyageDate;
            copy->leg1->departureTime = cur->leg1->departureTime;
            copy->leg1->arrivalTime = cur->leg1->arrivalTime;
            copy->leg1->voyageCost = cur->leg1->voyageCost;
            copy->leg1->shippingCompany = cur->leg1->shippingCompany;
            copy->leg1->next = nullptr;

            copy->leg2 = new Route();
            copy->leg2->destinationPort = cur->leg2->destinationPort;
            copy->leg2->voyageDate = cur->leg2->voyageDate;
            copy->leg2->departureTime = cur->leg2->departureTime;
            copy->leg2->arrivalTime = cur->leg2->arrivalTime;
            copy->leg2->voyageCost = cur->leg2->voyageCost;
            copy->leg2->shippingCompany = cur->leg2->shippingCompany;
            copy->leg2->next = nullptr;

            copy->leg3 = new Route();
            copy->leg3->destinationPort = cur->leg3->destinationPort;
            copy->leg3->voyageDate = cur->leg3->voyageDate;
            copy->leg3->departureTime = cur->leg3->departureTime;
            copy->leg3->arrivalTime = cur->leg3->arrivalTime;
            copy->leg3->voyageCost = cur->leg3->voyageCost;
            copy->leg3->shippingCompany = cur->leg3->shippingCompany;
            copy->leg3->next = nullptr;

            copy->next = nullptr;

            if (!filteredHead) {
                filteredHead = copy;
                filteredTail = copy;
            } else {
                filteredTail->next = copy;
                filteredTail = copy;
            }
        }
        cur = cur->next;
    }

    return filteredHead;
}

bool passesFourLegRoutePreferences(const RoutePreferences& prefs, FourLegRoute* route, const string& originPort) {
    if (!route || !route->leg1 || !route->leg2 || !route->leg3 || !route->leg4) return false;

    if (!isCompanyAllowed(prefs, route->leg1->shippingCompany)) return false;
    if (!isCompanyAllowed(prefs, route->leg2->shippingCompany)) return false;
    if (!isCompanyAllowed(prefs, route->leg3->shippingCompany)) return false;
    if (!isCompanyAllowed(prefs, route->leg4->shippingCompany)) return false;

    if (isPortForbidden(prefs, originPort)) return false;
    if (isPortForbidden(prefs, route->leg1->destinationPort)) return false;
    if (isPortForbidden(prefs, route->leg2->destinationPort)) return false;
    if (isPortForbidden(prefs, route->leg3->destinationPort)) return false;
    if (isPortForbidden(prefs, route->leg4->destinationPort)) return false;

    if (prefs.useMaxTotalCost) {
        int totalCost = route->leg1->voyageCost + route->leg2->voyageCost +
                        route->leg3->voyageCost + route->leg4->voyageCost;
        if (totalCost > prefs.maxTotalCost) return false;
    }

    if (prefs.useMaxLegs && 4 > prefs.maxLegs) return false;

    return true;
}

bool passesFiveLegRoutePreferences(const RoutePreferences& prefs, FiveLegRoute* route, const string& originPort) {
    if (!route || !route->leg1 || !route->leg2 || !route->leg3 || !route->leg4 || !route->leg5) return false;

    if (!isCompanyAllowed(prefs, route->leg1->shippingCompany)) return false;
    if (!isCompanyAllowed(prefs, route->leg2->shippingCompany)) return false;
    if (!isCompanyAllowed(prefs, route->leg3->shippingCompany)) return false;
    if (!isCompanyAllowed(prefs, route->leg4->shippingCompany)) return false;
    if (!isCompanyAllowed(prefs, route->leg5->shippingCompany)) return false;

    if (isPortForbidden(prefs, originPort)) return false;
    if (isPortForbidden(prefs, route->leg1->destinationPort)) return false;
    if (isPortForbidden(prefs, route->leg2->destinationPort)) return false;
    if (isPortForbidden(prefs, route->leg3->destinationPort)) return false;
    if (isPortForbidden(prefs, route->leg4->destinationPort)) return false;
    if (isPortForbidden(prefs, route->leg5->destinationPort)) return false;

    if (prefs.useMaxTotalCost) {
        int totalCost = route->leg1->voyageCost + route->leg2->voyageCost +
                        route->leg3->voyageCost + route->leg4->voyageCost +
                        route->leg5->voyageCost;
        if (totalCost > prefs.maxTotalCost) return false;
    }

    if (prefs.useMaxLegs && 5 > prefs.maxLegs) return false;

    return true;
}

FourLegRoute* filterThreeStopRoutesByPreferences(const RoutePreferences& prefs, FourLegRoute* inputList, const string& originPort) {
    FourLegRoute* filteredHead = nullptr;
    FourLegRoute* filteredTail = nullptr;

    FourLegRoute* cur = inputList;
    while (cur) {
        if (passesFourLegRoutePreferences(prefs, cur, originPort)) {

            FourLegRoute* copy = new FourLegRoute();

            copy->leg1 = new Route();
            copy->leg1->destinationPort = cur->leg1->destinationPort;
            copy->leg1->voyageDate = cur->leg1->voyageDate;
            copy->leg1->departureTime = cur->leg1->departureTime;
            copy->leg1->arrivalTime = cur->leg1->arrivalTime;
            copy->leg1->voyageCost = cur->leg1->voyageCost;
            copy->leg1->shippingCompany = cur->leg1->shippingCompany;
            copy->leg1->next = nullptr;

            copy->leg2 = new Route();
            copy->leg2->destinationPort = cur->leg2->destinationPort;
            copy->leg2->voyageDate = cur->leg2->voyageDate;
            copy->leg2->departureTime = cur->leg2->departureTime;
            copy->leg2->arrivalTime = cur->leg2->arrivalTime;
            copy->leg2->voyageCost = cur->leg2->voyageCost;
            copy->leg2->shippingCompany = cur->leg2->shippingCompany;
            copy->leg2->next = nullptr;

            copy->leg3 = new Route();
            copy->leg3->destinationPort = cur->leg3->destinationPort;
            copy->leg3->voyageDate = cur->leg3->voyageDate;
            copy->leg3->departureTime = cur->leg3->departureTime;
            copy->leg3->arrivalTime = cur->leg3->arrivalTime;
            copy->leg3->voyageCost = cur->leg3->voyageCost;
            copy->leg3->shippingCompany = cur->leg3->shippingCompany;
            copy->leg3->next = nullptr;

            copy->leg4 = new Route();
            copy->leg4->destinationPort = cur->leg4->destinationPort;
            copy->leg4->voyageDate = cur->leg4->voyageDate;
            copy->leg4->departureTime = cur->leg4->departureTime;
            copy->leg4->arrivalTime = cur->leg4->arrivalTime;
            copy->leg4->voyageCost = cur->leg4->voyageCost;
            copy->leg4->shippingCompany = cur->leg4->shippingCompany;
            copy->leg4->next = nullptr;

            copy->next = nullptr;

            if (!filteredHead) {
                filteredHead = copy;
                filteredTail = copy;
            } else {
                filteredTail->next = copy;
                filteredTail = copy;
            }
        }
        cur = cur->next;
    }

    return filteredHead;
}

FiveLegRoute* filterFourStopRoutesByPreferences(const RoutePreferences& prefs, FiveLegRoute* inputList, const string& originPort) {
    FiveLegRoute* filteredHead = nullptr;
    FiveLegRoute* filteredTail = nullptr;

    FiveLegRoute* cur = inputList;
    while (cur) {
        if (passesFiveLegRoutePreferences(prefs, cur, originPort)) {

            FiveLegRoute* copy = new FiveLegRoute();

            copy->leg1 = new Route();
            copy->leg1->destinationPort = cur->leg1->destinationPort;
            copy->leg1->voyageDate = cur->leg1->voyageDate;
            copy->leg1->departureTime = cur->leg1->departureTime;
            copy->leg1->arrivalTime = cur->leg1->arrivalTime;
            copy->leg1->voyageCost = cur->leg1->voyageCost;
            copy->leg1->shippingCompany = cur->leg1->shippingCompany;
            copy->leg1->next = nullptr;

            copy->leg2 = new Route();
            copy->leg2->destinationPort = cur->leg2->destinationPort;
            copy->leg2->voyageDate = cur->leg2->voyageDate;
            copy->leg2->departureTime = cur->leg2->departureTime;
            copy->leg2->arrivalTime = cur->leg2->arrivalTime;
            copy->leg2->voyageCost = cur->leg2->voyageCost;
            copy->leg2->shippingCompany = cur->leg2->shippingCompany;
            copy->leg2->next = nullptr;

            copy->leg3 = new Route();
            copy->leg3->destinationPort = cur->leg3->destinationPort;
            copy->leg3->voyageDate = cur->leg3->voyageDate;
            copy->leg3->departureTime = cur->leg3->departureTime;
            copy->leg3->arrivalTime = cur->leg3->arrivalTime;
            copy->leg3->voyageCost = cur->leg3->voyageCost;
            copy->leg3->shippingCompany = cur->leg3->shippingCompany;
            copy->leg3->next = nullptr;

            copy->leg4 = new Route();
            copy->leg4->destinationPort = cur->leg4->destinationPort;
            copy->leg4->voyageDate = cur->leg4->voyageDate;
            copy->leg4->departureTime = cur->leg4->departureTime;
            copy->leg4->arrivalTime = cur->leg4->arrivalTime;
            copy->leg4->voyageCost = cur->leg4->voyageCost;
            copy->leg4->shippingCompany = cur->leg4->shippingCompany;
            copy->leg4->next = nullptr;

            copy->leg5 = new Route();
            copy->leg5->destinationPort = cur->leg5->destinationPort;
            copy->leg5->voyageDate = cur->leg5->voyageDate;
            copy->leg5->departureTime = cur->leg5->departureTime;
            copy->leg5->arrivalTime = cur->leg5->arrivalTime;
            copy->leg5->voyageCost = cur->leg5->voyageCost;
            copy->leg5->shippingCompany = cur->leg5->shippingCompany;
            copy->leg5->next = nullptr;

            copy->next = nullptr;

            if (!filteredHead) {
                filteredHead = copy;
                filteredTail = copy;
            } else {
                filteredTail->next = copy;
                filteredTail = copy;
            }
        }
        cur = cur->next;
    }

    return filteredHead;
}
