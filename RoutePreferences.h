#ifndef ROUTE_PREFERENCES_H
#define ROUTE_PREFERENCES_H

#include <string>
#include "DateTime.h"
#include "Route.h"
#include "RouteSearch.h"

using namespace std;

const int MAX_ALLOWED_COMPANIES = 10;
const int MAX_FORBIDDEN_PORTS   = 10;
const int MAX_PREFERRED_PORTS   = 10;

struct RoutePreferences {
    bool   useMaxTotalCost;
    int    maxTotalCost;

    bool   useMaxLegs;
    int    maxLegs;

    string allowedCompanies[MAX_ALLOWED_COMPANIES];
    int    allowedCompaniesCount;

    string forbiddenPorts[MAX_FORBIDDEN_PORTS];
    int    forbiddenPortsCount;

    string preferredPorts[MAX_PREFERRED_PORTS];
    int    preferredPortsCount;

    bool   preferCheapest;
    bool   preferFastest;

    int    minLayoverMinutes;
    bool   sameDayOnly;

    RoutePreferences();
};

void initRoutePreferences(RoutePreferences& prefs);

bool isPortForbidden(const RoutePreferences& prefs, const string& portName);

bool isCompanyAllowed(const RoutePreferences& prefs, const string& companyName);

Route* filterDirectRoutesByPreferences(const RoutePreferences& prefs, Route* inputList, const string& originPort);

TwoLegRoute* filterOneStopRoutesByPreferences(const RoutePreferences& prefs, TwoLegRoute* inputList, const string& originPort);

ThreeLegRoute* filterTwoStopRoutesByPreferences(const RoutePreferences& prefs, ThreeLegRoute* inputList, const string& originPort);

FourLegRoute* filterThreeStopRoutesByPreferences(const RoutePreferences& prefs, FourLegRoute* inputList, const string& originPort);

FiveLegRoute* filterFourStopRoutesByPreferences(const RoutePreferences& prefs, FiveLegRoute* inputList, const string& originPort);

#endif
