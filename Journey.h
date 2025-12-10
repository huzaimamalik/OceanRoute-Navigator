#ifndef JOURNEY_H
#define JOURNEY_H

#include <string>
#include "DateTime.h"
#include "Route.h"
#include "RouteSearch.h"

using namespace std;

// Forward declaration for SafeJourney
struct SafeJourney;

struct BookedLeg {
    string originPort;
    string destinationPort;
    Date   voyageDate;
    Time   departureTime;
    Time   arrivalTime;
    int    voyageCost;
    string shippingCompany;

    BookedLeg* next;

    BookedLeg();
};

struct BookedJourney {
    BookedLeg* head;
    BookedLeg* tail;
    int        legCount;
    int        totalCost;

    BookedJourney();
};

void initJourney(BookedJourney& journey);

void appendLeg(BookedJourney& journey, const string& origin, const string& destination, const Date& date, const Time& dep, const Time& arr, int cost, const string& company);

BookedJourney buildJourneyFromDirect(const string& originPort, Route* directRoute);

BookedJourney buildJourneyFromTwoLeg(const string& originPort, TwoLegRoute* twoLegRoute);

BookedJourney buildJourneyFromThreeLeg(const string& originPort, ThreeLegRoute* threeLegRoute);

BookedJourney buildJourneyFromFourLeg(const string& originPort, FourLegRoute* fourLegRoute);

BookedJourney buildJourneyFromFiveLeg(const string& originPort, FiveLegRoute* fiveLegRoute);

BookedJourney buildJourneyFromSafeJourney(const string& originPort, const SafeJourney& safeJourney);

void printJourney(const BookedJourney& journey);

void clearJourney(BookedJourney& journey);

#endif

