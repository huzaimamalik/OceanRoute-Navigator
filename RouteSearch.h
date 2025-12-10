#pragma once

#include <string>
#include "Graph.h"
#include "Route.h"
#include "DateTime.h"

using namespace std;

struct TwoLegRoute {
    Route* leg1;
  Route* leg2;
  TwoLegRoute* next;

    TwoLegRoute() : leg1(nullptr), leg2(nullptr), next(nullptr) {}
};

struct ThreeLegRoute {
    Route* leg1;
    Route* leg2;
    Route* leg3;
    ThreeLegRoute* next;

    ThreeLegRoute() : leg1(nullptr), leg2(nullptr), leg3(nullptr), next(nullptr) {}
};

struct FourLegRoute {
    Route* leg1;
    Route* leg2;
    Route* leg3;
    Route* leg4;
    FourLegRoute* next;

    FourLegRoute() : leg1(nullptr), leg2(nullptr), leg3(nullptr), leg4(nullptr), next(nullptr) {}
};

struct FiveLegRoute {
    Route* leg1;
    Route* leg2;
    Route* leg3;
    Route* leg4;
    Route* leg5;
    FiveLegRoute* next;

    FiveLegRoute() : leg1(nullptr), leg2(nullptr), leg3(nullptr), leg4(nullptr), leg5(nullptr), next(nullptr) {}
};

bool isTimeBefore(const Time& a, const Time& b);

bool isLayoverFeasible(const Time& arrival, const Time& nextDeparture);

bool isValidConnectionMultiDay(const Date& leg1DepartureDate, const Time& leg1DepartureTime, const Time& leg1ArrivalTime, const Date& leg2DepartureDate, const Time& leg2DepartureTime, int minLayoverMinutes, int maxDaysGap);

Route* getDirectRoutes(Graph& g, const string& origin, const Date& d);

TwoLegRoute* getOneStopConnections(Graph& g, const string& origin, const string& destination, const Date& d);

ThreeLegRoute* getTwoStopConnections(Graph& g, const string& origin, const string& destination, const Date& d);

FourLegRoute* getThreeStopConnections(Graph& g, const string& origin, const string& destination, const Date& d);

FiveLegRoute* getFourStopConnections(Graph& g, const string& origin, const string& destination, const Date& d);

void getAllPossibleRoutes(Graph& g, const string& origin, const string& destination, const Date& d, Route*& directHead, TwoLegRoute*& oneStopHead, ThreeLegRoute*& twoStopHead, FourLegRoute*& threeStopHead, FiveLegRoute*& fourStopHead);

void printDirectRoutes(Route* head);

void printTwoLegRoutes(TwoLegRoute* head);

void printThreeLegRoutes(ThreeLegRoute* head);

void printFourLegRoutes(FourLegRoute* head);

void printFiveLegRoutes(FiveLegRoute* head);

void freeDirectList(Route* head);

void freeTwoLegList(TwoLegRoute* head);

void freeThreeLegList(ThreeLegRoute* head);

void freeFourLegList(FourLegRoute* head);

void freeFiveLegList(FiveLegRoute* head);

void searchCustomRoute(Graph& g, const string& origin, const string& destination, int day, int month, int year);

void testOneStopRoutes(Graph& g);

void testTwoStopRoutes(Graph& g);

void testDay2(Graph& g);
