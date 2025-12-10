#include "Journey.h"
#include "SafestRouteSearch.h"
#include <iostream>

BookedLeg::BookedLeg() : originPort(), destinationPort(), voyageDate{0,0,0}, departureTime{0,0}, arrivalTime{0,0}, voyageCost(0), shippingCompany(), next(nullptr) {}

BookedJourney::BookedJourney() : head(nullptr), tail(nullptr), legCount(0), totalCost(0) {}

void initJourney(BookedJourney& journey) {
    journey.head = nullptr;
    journey.tail = nullptr;
    journey.legCount = 0;
    journey.totalCost = 0;
}

void appendLeg(BookedJourney& journey, const string& origin, const string& destination, const Date& date, const Time& dep, const Time& arr, int cost, const string& company) {
    BookedLeg* leg = new BookedLeg();
    leg->originPort = origin;
    leg->destinationPort = destination;
    leg->voyageDate = date;
    leg->departureTime = dep;
    leg->arrivalTime = arr;
    leg->voyageCost = cost;
    leg->shippingCompany = company;
    leg->next = nullptr;

    if (journey.head == nullptr) {
        journey.head = leg;
        journey.tail = leg;
    } else {
        journey.tail->next = leg;
        journey.tail = leg;
    }

    journey.legCount++;
    journey.totalCost += cost;
}

BookedJourney buildJourneyFromDirect(const string& originPort, Route* directRoute) {
    BookedJourney j;
    initJourney(j);
    if (!directRoute) return j;

    appendLeg(j, originPort, directRoute->destinationPort, directRoute->voyageDate, directRoute->departureTime, directRoute->arrivalTime, directRoute->voyageCost, directRoute->shippingCompany);

    return j;
}

BookedJourney buildJourneyFromTwoLeg(const string& originPort, TwoLegRoute* twoLegRoute) {
    BookedJourney j;
    initJourney(j);
    if (!twoLegRoute) return j;

    Route* r1 = twoLegRoute->leg1;
    Route* r2 = twoLegRoute->leg2;
    if (!r1 || !r2) return j;

    appendLeg(j, originPort, r1->destinationPort, r1->voyageDate, r1->departureTime, r1->arrivalTime, r1->voyageCost, r1->shippingCompany);

    appendLeg(j, r1->destinationPort, r2->destinationPort, r2->voyageDate, r2->departureTime, r2->arrivalTime, r2->voyageCost, r2->shippingCompany);

    return j;
}

BookedJourney buildJourneyFromThreeLeg(const string& originPort, ThreeLegRoute* threeLegRoute) {
    BookedJourney j;
    initJourney(j);
    if (!threeLegRoute) return j;

    Route* r1 = threeLegRoute->leg1;
    Route* r2 = threeLegRoute->leg2;
    Route* r3 = threeLegRoute->leg3;
    if (!r1 || !r2 || !r3) return j;

    appendLeg(j, originPort, r1->destinationPort, r1->voyageDate, r1->departureTime, r1->arrivalTime, r1->voyageCost, r1->shippingCompany);
    appendLeg(j, r1->destinationPort, r2->destinationPort, r2->voyageDate, r2->departureTime, r2->arrivalTime, r2->voyageCost, r2->shippingCompany);
    appendLeg(j, r2->destinationPort, r3->destinationPort, r3->voyageDate, r3->departureTime, r3->arrivalTime, r3->voyageCost, r3->shippingCompany);

    return j;
}

BookedJourney buildJourneyFromFourLeg(const string& originPort, FourLegRoute* fourLegRoute) {
    BookedJourney j;
    initJourney(j);
    if (!fourLegRoute) return j;

    Route* r1 = fourLegRoute->leg1;
    Route* r2 = fourLegRoute->leg2;
    Route* r3 = fourLegRoute->leg3;
    Route* r4 = fourLegRoute->leg4;
    if (!r1 || !r2 || !r3 || !r4) return j;

    appendLeg(j, originPort, r1->destinationPort, r1->voyageDate, r1->departureTime, r1->arrivalTime, r1->voyageCost, r1->shippingCompany);
    appendLeg(j, r1->destinationPort, r2->destinationPort, r2->voyageDate, r2->departureTime, r2->arrivalTime, r2->voyageCost, r2->shippingCompany);
    appendLeg(j, r2->destinationPort, r3->destinationPort, r3->voyageDate, r3->departureTime, r3->arrivalTime, r3->voyageCost, r3->shippingCompany);
    appendLeg(j, r3->destinationPort, r4->destinationPort, r4->voyageDate, r4->departureTime, r4->arrivalTime, r4->voyageCost, r4->shippingCompany);

    return j;
}

BookedJourney buildJourneyFromFiveLeg(const string& originPort, FiveLegRoute* fiveLegRoute) {
    BookedJourney j;
    initJourney(j);
    if (!fiveLegRoute) return j;

    Route* r1 = fiveLegRoute->leg1;
    Route* r2 = fiveLegRoute->leg2;
    Route* r3 = fiveLegRoute->leg3;
    Route* r4 = fiveLegRoute->leg4;
    Route* r5 = fiveLegRoute->leg5;
    if (!r1 || !r2 || !r3 || !r4 || !r5) return j;

    appendLeg(j, originPort, r1->destinationPort, r1->voyageDate, r1->departureTime, r1->arrivalTime, r1->voyageCost, r1->shippingCompany);
    appendLeg(j, r1->destinationPort, r2->destinationPort, r2->voyageDate, r2->departureTime, r2->arrivalTime, r2->voyageCost, r2->shippingCompany);
    appendLeg(j, r2->destinationPort, r3->destinationPort, r3->voyageDate, r3->departureTime, r3->arrivalTime, r3->voyageCost, r3->shippingCompany);
    appendLeg(j, r3->destinationPort, r4->destinationPort, r4->voyageDate, r4->departureTime, r4->arrivalTime, r4->voyageCost, r4->shippingCompany);
    appendLeg(j, r4->destinationPort, r5->destinationPort, r5->voyageDate, r5->departureTime, r5->arrivalTime, r5->voyageCost, r5->shippingCompany);

    return j;
}

void printJourney(const BookedJourney& journey) {
    cout << "Booked Journey (" << journey.legCount << " leg(s), total cost: $" << journey.totalCost << "):\n";
    BookedLeg* cur = journey.head;
    int idx = 1;
    while (cur) {
        cout << "  Leg " << idx << ":\n";
        cout << "    Origin: " << cur->originPort << "\n";
        cout << "    Destination: " << cur->destinationPort << "\n";
        cout << "    Date: " << cur->voyageDate.day << "/" << cur->voyageDate.month << "/" << cur->voyageDate.year << "\n";
        cout << "    Departure: " << cur->departureTime.hour << ":" << (cur->departureTime.minute < 10 ? "0" : "") << cur->departureTime.minute << "\n";
        cout << "    Arrival: " << cur->arrivalTime.hour << ":" << (cur->arrivalTime.minute < 10 ? "0" : "") << cur->arrivalTime.minute << "\n";
        cout << "    Cost: $" << cur->voyageCost << "\n";
        cout << "    Company: " << cur->shippingCompany << "\n";
        cur = cur->next;
        idx++;
    }
}

void clearJourney(BookedJourney& journey) {
    BookedLeg* cur = journey.head;
    while (cur) {
        BookedLeg* nxt = cur->next;
        delete cur;
        cur = nxt;
    }
    journey.head = nullptr;
    journey.tail = nullptr;
    journey.legCount = 0;
    journey.totalCost = 0;
}

BookedJourney buildJourneyFromSafeJourney(const string& originPort, const SafeJourney& safeJourney) {
    BookedJourney j;
    initJourney(j);
    
    if (safeJourney.legsHead == nullptr) return j;
    
    string currentPort = originPort;
    SafeJourneyLeg* leg = safeJourney.legsHead;
    
    while (leg != nullptr) {
        Route* route = leg->route;
        appendLeg(j, currentPort, route->destinationPort, route->voyageDate, 
                  route->departureTime, route->arrivalTime, route->voyageCost, 
                  route->shippingCompany);
        currentPort = route->destinationPort;
        leg = leg->next;
    }
    
    return j;
}
