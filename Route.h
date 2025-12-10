#pragma once

#include <string>
#include "DateTime.h"

using namespace std;

struct Route {
 string destinationPort;
 Date voyageDate;
 Time departureTime;
 Time arrivalTime;
 int voyageCost;
 string shippingCompany;
 Route *next;

 Route() : destinationPort(), voyageDate{0,0,0}, departureTime{0,0}, arrivalTime{0,0}, voyageCost(0), shippingCompany(), next(nullptr) {}
};

Route *createRoute(const string &destination, const Date &date, const Time &dep, const Time &arr, int cost, const string &company);

Route *prependRoute(Route *head, Route *r);
