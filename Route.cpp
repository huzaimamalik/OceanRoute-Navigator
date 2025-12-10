

#include "Route.h"

using namespace std;

Route *createRoute(const string &destination, const Date &date, const Time &dep, const Time &arr, int cost, const string &company) {
 Route *r = new Route();
 r->destinationPort = destination;
 r->voyageDate = date;
 r->departureTime = dep;
 r->arrivalTime = arr;
 r->voyageCost = cost;
 r->shippingCompany = company;
 r->next = nullptr;
 return r;
}

Route *prependRoute(Route *head, Route *r) {
 if (!r) return head;
 r->next = head;
 return r;
}
