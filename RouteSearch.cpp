

#include "RouteSearch.h"
#include <iostream>

using namespace std;

const int MIN_LAYOVER_MINUTES = 60;

const int MAX_CONNECTION_DAYS = 30;

const int SEARCH_WINDOW_DAYS = 365;

Route* copyRoute(Route* original) {
    if (!original) return nullptr;

    Route* copy = new Route;
    copy->destinationPort = original->destinationPort;
    copy->voyageDate = original->voyageDate;
    copy->departureTime = original->departureTime;
    copy->arrivalTime = original->arrivalTime;
    copy->voyageCost = original->voyageCost;
    copy->shippingCompany = original->shippingCompany;
    copy->next = nullptr;

    return copy;
}

int compareDates(const Date& a, const Date& b) {
    if (a.year != b.year) return (a.year < b.year) ? -1 : 1;
    if (a.month != b.month) return (a.month < b.month) ? -1 : 1;
    if (a.day != b.day) return (a.day < b.day) ? -1 : 1;
    return 0;
}

int getDayOfYear(const Date& d) {
    int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int dayOfYear = d.day;
    for (int m = 1; m < d.month; m++) {
        dayOfYear += daysInMonth[m];
    }
    return dayOfYear + d.year * 365;
}

bool isDateInRange(const Date& date, const Date& startDate, int maxDays) {

    int dateDay = getDayOfYear(date);
    int startDay = getDayOfYear(startDate);

    int yearDiff = date.year - startDate.year;
    if (yearDiff > 0) {
        dateDay += yearDiff * 365;
    } else if (yearDiff < 0) {

        return false;
    }

    int diff = dateDay - startDay;

    return diff >= 0 && diff <= maxDays;
}

bool isDateOnOrAfter(const Date& dateA, const Date& dateB) {
    if (dateA.year != dateB.year) return dateA.year > dateB.year;
    if (dateA.month != dateB.month) return dateA.month > dateB.month;
    return dateA.day >= dateB.day;
}

bool voyageCrossesMidnight(const Time& departure, const Time& arrival) {
    int depMin = departure.hour * 60 + departure.minute;
    int arrMin = arrival.hour * 60 + arrival.minute;
    return arrMin < depMin;
}

Date getActualArrivalDate(const Date& departureDate, const Time& departure, const Time& arrival) {
    Date arrivalDate = departureDate;
    if (voyageCrossesMidnight(departure, arrival)) {

        arrivalDate.day++;

        int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (arrivalDate.day > daysInMonth[arrivalDate.month]) {
            arrivalDate.day = 1;
            arrivalDate.month++;
            if (arrivalDate.month > 12) {
                arrivalDate.month = 1;
                arrivalDate.year++;
            }
        }
    }
    return arrivalDate;
}

bool isValidConnectionSameDay(const Date& leg1Date, const Time& leg1ArrivalTime, const Date& leg2Date, const Time& leg2DepartureTime, int minLayoverMinutes) {

    if (compareDates(leg1Date, leg2Date) != 0) {
        return false;
    }

    int arrMin = leg1ArrivalTime.hour * 60 + leg1ArrivalTime.minute;
    int depMin = leg2DepartureTime.hour * 60 + leg2DepartureTime.minute;
    int layover = depMin - arrMin;

    if (layover < minLayoverMinutes) {
        return false;
    }

    return true;
}

bool isValidConnectionMultiDay(const Date& leg1DepartureDate, const Time& leg1DepartureTime, const Time& leg1ArrivalTime, const Date& leg2DepartureDate, const Time& leg2DepartureTime, int minLayoverMinutes, int maxDaysGap) {

    Date actualArrivalDate = getActualArrivalDate(leg1DepartureDate, leg1DepartureTime, leg1ArrivalTime);

    int cmp = compareDates(actualArrivalDate, leg2DepartureDate);

    if (cmp > 0) {
        return false;
    }

    int arrivalDayNum = getDayOfYear(actualArrivalDate) + actualArrivalDate.year * 365;
    int departureDayNum = getDayOfYear(leg2DepartureDate) + leg2DepartureDate.year * 365;
    int daysDiff = departureDayNum - arrivalDayNum;

    if (daysDiff > maxDaysGap) {
        return false;
    }

    if (cmp == 0) {

        int arrivalMinutes = leg1ArrivalTime.hour * 60 + leg1ArrivalTime.minute;
        int departureMinutes = leg2DepartureTime.hour * 60 + leg2DepartureTime.minute;
        int layoverMinutes = departureMinutes - arrivalMinutes;

        if (layoverMinutes < 0) {
            return false;
        }

        if (layoverMinutes < minLayoverMinutes) {
            return false;
        }
    }

    return true;
}

bool isValidConnection(const Date& arrivalDate, const Time& arrivalTime, const Date& nextDepDate, const Time& nextDepTime) {
    return isValidConnectionSameDay(arrivalDate, arrivalTime, nextDepDate, nextDepTime, MIN_LAYOVER_MINUTES);
}

bool isTimeBefore(const Time& a, const Time& b) {
    if (a.hour < b.hour) return true;
    if (a.hour > b.hour) return false;
    return a.minute < b.minute;
}

bool isLayoverFeasible(const Time& arrival, const Time& nextDeparture) {

    int arrivalMinutes = arrival.hour * 60 + arrival.minute;
    int departureMinutes = nextDeparture.hour * 60 + nextDeparture.minute;

    return (departureMinutes - arrivalMinutes) >= 60;
}

Route* getDirectRoutes(Graph& g, const string& origin, const Date& d) {

    Port* originPort = findPort(g, origin);
    if (!originPort) return nullptr;

    Route* resultHead = nullptr;
    Route* resultTail = nullptr;

    Route* current = originPort->routeHead;
    while (current) {

        if (compareDates(current->voyageDate, d) == 0) {

            Route* copy = copyRoute(current);

            if (!resultHead) {
                resultHead = copy;
                resultTail = copy;
            } else {
                resultTail->next = copy;
                resultTail = copy;
            }
        }
        current = current->next;
    }

    return resultHead;
}

TwoLegRoute* getOneStopConnections(Graph& g, const string& origin, const string& destination, const Date& d) {

    Port* originPort = findPort(g, origin);
    if (!originPort) return nullptr;

    TwoLegRoute* resultHead = nullptr;
    TwoLegRoute* resultTail = nullptr;

    int leg1Count = 0;
    int leg2Candidates = 0;
    int validConnections = 0;
    int rejectedEarlyDeparture = 0;

    Route* leg1 = originPort->routeHead;
    while (leg1) {

        if (compareDates(leg1->voyageDate, d) == 0) {
            leg1Count++;
            string intermediatePort = leg1->destinationPort;

            if (intermediatePort == destination) {
                leg1 = leg1->next;
                continue;
            }

            Port* interPort = findPort(g, intermediatePort);
            if (interPort) {

                Route* leg2 = interPort->routeHead;
                while (leg2) {

                    if (leg2->destinationPort.compare(destination) == 0) {
                        leg2Candidates++;

                        if (isValidConnectionMultiDay(leg1->voyageDate, leg1->departureTime, leg1->arrivalTime, leg2->voyageDate, leg2->departureTime, MIN_LAYOVER_MINUTES, MAX_CONNECTION_DAYS)) {
                            validConnections++;

                            TwoLegRoute* twoLeg = new TwoLegRoute;
                            twoLeg->leg1 = copyRoute(leg1);
                            twoLeg->leg2 = copyRoute(leg2);
                            twoLeg->next = nullptr;

                            if (!resultHead) {
                                resultHead = twoLeg;
                                resultTail = twoLeg;
                            } else {
                                resultTail->next = twoLeg;
                                resultTail = twoLeg;
                            }
                        } else {
                            rejectedEarlyDeparture++;
                        }
                    }
                    leg2 = leg2->next;
                }
            }
        }
        leg1 = leg1->next;
    }

    cout << "[DEBUG OneStop] Leg1 routes: " << leg1Count
         << ", Leg2 candidates: " << leg2Candidates
         << ", Valid: " << validConnections
         << ", Rejected (early departure): " << rejectedEarlyDeparture << endl;

    return resultHead;
}

ThreeLegRoute* getTwoStopConnections(Graph& g, const string& origin, const string& destination, const Date& d) {

    Port* originPort = findPort(g, origin);
    if (!originPort) return nullptr;

    ThreeLegRoute* resultHead = nullptr;
    ThreeLegRoute* resultTail = nullptr;

    int validRoutes = 0;
    int rejectedConnections = 0;

    Route* leg1 = originPort->routeHead;
    while (leg1) {

        if (compareDates(leg1->voyageDate, d) == 0) {
            string stop1 = leg1->destinationPort;

            if (stop1 == destination) {
                leg1 = leg1->next;
                continue;
            }

            Port* stop1Port = findPort(g, stop1);
            if (stop1Port) {

                Route* leg2 = stop1Port->routeHead;
                while (leg2) {

                    if (leg2->destinationPort == origin) {
                        leg2 = leg2->next;
                        continue;
                    }

                    if (isValidConnectionMultiDay(leg1->voyageDate, leg1->departureTime, leg1->arrivalTime, leg2->voyageDate, leg2->departureTime, MIN_LAYOVER_MINUTES, MAX_CONNECTION_DAYS)) {
                        string stop2 = leg2->destinationPort;

                        Port* stop2Port = findPort(g, stop2);
                        if (stop2Port) {

                            Route* leg3 = stop2Port->routeHead;
                            while (leg3) {

                                if (leg3->destinationPort.compare(destination) == 0) {

                                    if (isValidConnectionMultiDay(leg2->voyageDate, leg2->departureTime, leg2->arrivalTime, leg3->voyageDate, leg3->departureTime, MIN_LAYOVER_MINUTES, MAX_CONNECTION_DAYS)) {
                                        validRoutes++;

                                        ThreeLegRoute* threeLeg = new ThreeLegRoute;
                                        threeLeg->leg1 = copyRoute(leg1);
                                        threeLeg->leg2 = copyRoute(leg2);
                                        threeLeg->leg3 = copyRoute(leg3);
                                        threeLeg->next = nullptr;

                                        if (!resultHead) {
                                            resultHead = threeLeg;
                                            resultTail = threeLeg;
                                        } else {
                                            resultTail->next = threeLeg;
                                            resultTail = threeLeg;
                                        }
                                    } else {
                                        rejectedConnections++;
                                    }
                                }
                                leg3 = leg3->next;
                            }
                        }
                    }
                    leg2 = leg2->next;
                }
            }
        }
        leg1 = leg1->next;
    }

    cout << "[DEBUG TwoStop] Valid routes: " << validRoutes
         << ", Rejected (invalid timing): " << rejectedConnections << endl;

    return resultHead;
}

FourLegRoute* getThreeStopConnections(Graph& g, const string& origin, const string& destination, const Date& d) {
    Port* originPort = findPort(g, origin);
    if (!originPort) return nullptr;

    FourLegRoute* resultHead = nullptr;
    FourLegRoute* resultTail = nullptr;

    int validRoutes = 0;
    int rejectedConnections = 0;

    Route* leg1 = originPort->routeHead;
    while (leg1) {
        if (compareDates(leg1->voyageDate, d) == 0) {
            string stop1 = leg1->destinationPort;
            if (stop1 == destination) {
                leg1 = leg1->next;
                continue;
            }

            Port* stop1Port = findPort(g, stop1);
            if (stop1Port) {
                Route* leg2 = stop1Port->routeHead;
                while (leg2) {
                    if (leg2->destinationPort == origin) {
                        leg2 = leg2->next;
                        continue;
                    }

                    if (isValidConnectionMultiDay(leg1->voyageDate, leg1->departureTime,
                                                  leg1->arrivalTime,
                                                  leg2->voyageDate, leg2->departureTime,
                                                  MIN_LAYOVER_MINUTES, MAX_CONNECTION_DAYS)) {
                        string stop2 = leg2->destinationPort;
                        if (stop2 == destination) {
                            leg2 = leg2->next;
                            continue;
                        }

                        Port* stop2Port = findPort(g, stop2);
                        if (stop2Port) {
                            Route* leg3 = stop2Port->routeHead;
                            while (leg3) {
                                if (leg3->destinationPort == origin || leg3->destinationPort == stop1) {
                                    leg3 = leg3->next;
                                    continue;
                                }

                                if (isValidConnectionMultiDay(leg2->voyageDate, leg2->departureTime,
                                                              leg2->arrivalTime,
                                                              leg3->voyageDate, leg3->departureTime,
                                                              MIN_LAYOVER_MINUTES, MAX_CONNECTION_DAYS)) {
                                    string stop3 = leg3->destinationPort;
                                    if (stop3 == destination) {
                                        leg3 = leg3->next;
                                        continue;
                                    }

                                    Port* stop3Port = findPort(g, stop3);
                                    if (stop3Port) {
                                        Route* leg4 = stop3Port->routeHead;
                                        while (leg4) {
                                            if (leg4->destinationPort.compare(destination) == 0) {
                                                if (isValidConnectionMultiDay(leg3->voyageDate, leg3->departureTime, leg3->arrivalTime, leg4->voyageDate, leg4->departureTime, MIN_LAYOVER_MINUTES, MAX_CONNECTION_DAYS)) {
                                                    validRoutes++;
                                                    FourLegRoute* fourLeg = new FourLegRoute;
                                                    fourLeg->leg1 = copyRoute(leg1);
                                                    fourLeg->leg2 = copyRoute(leg2);
                                                    fourLeg->leg3 = copyRoute(leg3);
                                                    fourLeg->leg4 = copyRoute(leg4);
                                                    fourLeg->next = nullptr;

                                                    if (!resultHead) {
                                                        resultHead = fourLeg;
                                                        resultTail = fourLeg;
                                                    } else {
                                                        resultTail->next = fourLeg;
                                                        resultTail = fourLeg;
                                                    }
                                                } else {
                                                    rejectedConnections++;
                                                }
                                            }
                                            leg4 = leg4->next;
                                        }
                                    }
                                }
                                leg3 = leg3->next;
                            }
                        }
                    }
                    leg2 = leg2->next;
                }
            }
        }
        leg1 = leg1->next;
    }

    cout << "[DEBUG ThreeStop] Valid routes: " << validRoutes
         << ", Rejected (invalid timing): " << rejectedConnections << endl;

    return resultHead;
}

FiveLegRoute* getFourStopConnections(Graph& g, const string& origin, const string& destination, const Date& d) {
    Port* originPort = findPort(g, origin);
    if (!originPort) return nullptr;

    FiveLegRoute* resultHead = nullptr;
    FiveLegRoute* resultTail = nullptr;

    int validRoutes = 0;
    int rejectedConnections = 0;

    Route* leg1 = originPort->routeHead;
    while (leg1) {
        if (compareDates(leg1->voyageDate, d) == 0) {
            string stop1 = leg1->destinationPort;
            if (stop1 == destination) {
                leg1 = leg1->next;
                continue;
            }

            Port* stop1Port = findPort(g, stop1);
            if (stop1Port) {
                Route* leg2 = stop1Port->routeHead;
                while (leg2) {
                    if (leg2->destinationPort == origin) {
                        leg2 = leg2->next;
                        continue;
                    }

                    if (isValidConnectionMultiDay(leg1->voyageDate, leg1->departureTime,
                                                  leg1->arrivalTime,
                                                  leg2->voyageDate, leg2->departureTime,
                                                  MIN_LAYOVER_MINUTES, MAX_CONNECTION_DAYS)) {
                        string stop2 = leg2->destinationPort;
                        if (stop2 == destination) {
                            leg2 = leg2->next;
                            continue;
                        }

                        Port* stop2Port = findPort(g, stop2);
                        if (stop2Port) {
                            Route* leg3 = stop2Port->routeHead;
                            while (leg3) {
                                if (leg3->destinationPort == origin || leg3->destinationPort == stop1) {
                                    leg3 = leg3->next;
                                    continue;
                                }

                                if (isValidConnectionMultiDay(leg2->voyageDate, leg2->departureTime, leg2->arrivalTime, leg3->voyageDate, leg3->departureTime, MIN_LAYOVER_MINUTES, MAX_CONNECTION_DAYS)) {
                                    string stop3 = leg3->destinationPort;
                                    if (stop3 == destination) {
                                        leg3 = leg3->next;
                                        continue;
                                    }

                                    Port* stop3Port = findPort(g, stop3);
                                    if (stop3Port) {
                                        Route* leg4 = stop3Port->routeHead;
                                        while (leg4) {
                                            if (leg4->destinationPort == origin || leg4->destinationPort == stop1 || leg4->destinationPort == stop2) {
                                                leg4 = leg4->next;
                                                continue;
                                            }

                                            if (isValidConnectionMultiDay(leg3->voyageDate, leg3->departureTime, leg3->arrivalTime, leg4->voyageDate, leg4->departureTime, MIN_LAYOVER_MINUTES, MAX_CONNECTION_DAYS)) {
                                                string stop4 = leg4->destinationPort;
                                                if (stop4 == destination) {
                                                    leg4 = leg4->next;
                                                    continue;
                                                }

                                                Port* stop4Port = findPort(g, stop4);
                                                if (stop4Port) {
                                                    Route* leg5 = stop4Port->routeHead;
                                                    while (leg5) {
                                                        if (leg5->destinationPort.compare(destination) == 0) {
                                                            if (isValidConnectionMultiDay(leg4->voyageDate, leg4->departureTime, leg4->arrivalTime, leg5->voyageDate, leg5->departureTime, MIN_LAYOVER_MINUTES, MAX_CONNECTION_DAYS)) {
                                                                validRoutes++;
                                                                FiveLegRoute* fiveLeg = new FiveLegRoute;
                                                                fiveLeg->leg1 = copyRoute(leg1);
                                                                fiveLeg->leg2 = copyRoute(leg2);
                                                                fiveLeg->leg3 = copyRoute(leg3);
                                                                fiveLeg->leg4 = copyRoute(leg4);
                                                                fiveLeg->leg5 = copyRoute(leg5);
                                                                fiveLeg->next = nullptr;

                                                                if (!resultHead) {
                                                                    resultHead = fiveLeg;
                                                                    resultTail = fiveLeg;
                                                                } else {
                                                                    resultTail->next = fiveLeg;
                                                                    resultTail = fiveLeg;
                                                                }
                                                            } else {
                                                                rejectedConnections++;
                                                            }
                                                        }
                                                        leg5 = leg5->next;
                                                    }
                                                }
                                            }
                                            leg4 = leg4->next;
                                        }
                                    }
                                }
                                leg3 = leg3->next;
                            }
                        }
                    }
                    leg2 = leg2->next;
                }
            }
        }
        leg1 = leg1->next;
    }

    cout << "[DEBUG FourStop] Valid routes: " << validRoutes
         << ", Rejected (invalid timing): " << rejectedConnections << endl;

    return resultHead;
}

void getAllPossibleRoutes(Graph& g, const string& origin, const string& destination, const Date& d, Route*& directHead, TwoLegRoute*& oneStopHead, ThreeLegRoute*& twoStopHead, FourLegRoute*& threeStopHead, FiveLegRoute*& fourStopHead) {

    directHead = nullptr;
    oneStopHead = nullptr;
    twoStopHead = nullptr;
    threeStopHead = nullptr;
    fourStopHead = nullptr;

    Route* allDirectRoutes = getDirectRoutes(g, origin, d);

    Route* filteredHead = nullptr;
    Route* filteredTail = nullptr;

    Route* current = allDirectRoutes;
    while (current) {
        Route* nextRoute = current->next;

        if (current->destinationPort.compare(destination) == 0) {

            current->next = nullptr;

            if (!filteredHead) {
                filteredHead = current;
                filteredTail = current;
            } else {
                filteredTail->next = current;
                filteredTail = current;
            }
        } else {

            delete current;
        }

        current = nextRoute;
    }

    directHead = filteredHead;

    oneStopHead = getOneStopConnections(g, origin, destination, d);

    twoStopHead = getTwoStopConnections(g, origin, destination, d);

    threeStopHead = getThreeStopConnections(g, origin, destination, d);

    fourStopHead = getFourStopConnections(g, origin, destination, d);
}

void printDirectRoutes(Route* head) {
    if (!head) {
        cout << "No direct routes found.\n";
        return;
    }

    int count = 1;
    Route* current = head;
  while (current) {
        cout << "Direct Route " << count << ":\n";
     cout << "  Destination: " << current->destinationPort << "\n";
        cout << "  Date: " << current->voyageDate.day << "/"
         << current->voyageDate.month << "/"
             << current->voyageDate.year << "\n";
        cout << "  Departure: " << current->departureTime.hour << ":"
          << (current->departureTime.minute < 10 ? "0" : "")
             << current->departureTime.minute << "\n";
        cout << "  Arrival: " << current->arrivalTime.hour << ":"
 << (current->arrivalTime.minute < 10 ? "0" : "")
             << current->arrivalTime.minute << "\n";
        cout << "  Cost: $" << current->voyageCost << "\n";
        cout << "  Company: " << current->shippingCompany << "\n\n";

        count++;
        current = current->next;
  }
}

void printTwoLegRoutes(TwoLegRoute* head) {
  if (!head) {
 cout << "No one-stop routes found.\n";
  return;
    }

    int count = 1;
    TwoLegRoute* current = head;
    while (current) {
        cout << "One-Stop Route " << count << ":\n";

  cout << "  Leg 1:\n";
      cout << "    Destination: " << current->leg1->destinationPort << "\n";
  cout << "    Date: " << current->leg1->voyageDate.day << "/"
      << current->leg1->voyageDate.month << "/"
        << current->leg1->voyageDate.year << "\n";
  cout << "    Departure: " << current->leg1->departureTime.hour << ":"
   << (current->leg1->departureTime.minute < 10 ? "0" : "")
     << current->leg1->departureTime.minute << "\n";
        cout << "    Arrival: " << current->leg1->arrivalTime.hour << ":"
    << (current->leg1->arrivalTime.minute < 10 ? "0" : "")
           << current->leg1->arrivalTime.minute << "\n";
        cout << "    Cost: $" << current->leg1->voyageCost << "\n";
        cout << "  Company: " << current->leg1->shippingCompany << "\n";

        cout << "  Leg 2:\n";
  cout << "    Destination: " << current->leg2->destinationPort << "\n";
        cout << "    Date: " << current->leg2->voyageDate.day << "/"
  << current->leg2->voyageDate.month << "/"
      << current->leg2->voyageDate.year << "\n";
        cout << "    Departure: " << current->leg2->departureTime.hour << ":"
             << (current->leg2->departureTime.minute < 10 ? "0" : "")
 << current->leg2->departureTime.minute << "\n";
 cout << "    Arrival: " << current->leg2->arrivalTime.hour << ":"
             << (current->leg2->arrivalTime.minute < 10 ? "0" : "")
   << current->leg2->arrivalTime.minute << "\n";
        cout << "    Cost: $" << current->leg2->voyageCost << "\n";
  cout << "    Company: " << current->leg2->shippingCompany << "\n";

    int totalCost = current->leg1->voyageCost + current->leg2->voyageCost;
  cout << "  Total Cost: $" << totalCost << "\n\n";

        count++;
        current = current->next;
  }
}

void printThreeLegRoutes(ThreeLegRoute* head) {
    if (!head) {
   cout << "No two-stop routes found.\n";
     return;
    }

    int count = 1;
    ThreeLegRoute* current = head;
    while (current) {
        cout << "Two-Stop Route " << count << ":\n";

     cout << "  Leg 1:\n";
        cout << "    Destination: " << current->leg1->destinationPort << "\n";
        cout << "    Date: " << current->leg1->voyageDate.day << "/"
       << current->leg1->voyageDate.month << "/"
     << current->leg1->voyageDate.year << "\n";
     cout << "    Departure: " << current->leg1->departureTime.hour << ":"
      << (current->leg1->departureTime.minute < 10 ? "0" : "")
       << current->leg1->departureTime.minute << "\n";
        cout << "    Arrival: " << current->leg1->arrivalTime.hour << ":"
   << (current->leg1->arrivalTime.minute < 10 ? "0" : "")
    << current->leg1->arrivalTime.minute << "\n";
        cout << "    Cost: $" << current->leg1->voyageCost << "\n";
        cout << "    Company: " << current->leg1->shippingCompany << "\n";

   cout << "  Leg 2:\n";
        cout << "    Destination: " << current->leg2->destinationPort << "\n";
     cout << "    Date: " << current->leg2->voyageDate.day << "/"
 << current->leg2->voyageDate.month << "/"
<< current->leg2->voyageDate.year << "\n";
     cout << "    Departure: " << current->leg2->departureTime.hour << ":"
     << (current->leg2->departureTime.minute < 10 ? "0" : "")
      << current->leg2->departureTime.minute << "\n";
        cout << "    Arrival: " << current->leg2->arrivalTime.hour << ":"
  << (current->leg2->arrivalTime.minute < 10 ? "0" : "")
  << current->leg2->arrivalTime.minute << "\n";
 cout << "    Cost: $" << current->leg2->voyageCost << "\n";
    cout << "    Company: " << current->leg2->shippingCompany << "\n";

        cout << "  Leg 3:\n";
        cout << "    Destination: " << current->leg3->destinationPort << "\n";
        cout << "    Date: " << current->leg3->voyageDate.day << "/"
     << current->leg3->voyageDate.month << "/"
<< current->leg3->voyageDate.year << "\n";
        cout << "    Departure: " << current->leg3->departureTime.hour << ":"
      << (current->leg3->departureTime.minute < 10 ? "0" : "")
   << current->leg3->departureTime.minute << "\n";
      cout << "    Arrival: " << current->leg3->arrivalTime.hour << ":"
<< (current->leg3->arrivalTime.minute < 10 ? "0" : "")
      << current->leg3->arrivalTime.minute << "\n";
        cout << "    Cost: $" << current->leg3->voyageCost << "\n";
        cout << "    Company: " << current->leg3->shippingCompany << "\n";

        int totalCost = current->leg1->voyageCost + current->leg2->voyageCost + current->leg3->voyageCost;
cout << "  Total Cost: $" << totalCost << "\n\n";

  count++;
        current = current->next;
 }
}

void printFourLegRoutes(FourLegRoute* head) {
    if (!head) {
        cout << "No three-stop routes found.\n";
        return;
    }

    int count = 1;
    FourLegRoute* current = head;
    while (current) {
        cout << "Three-Stop Route " << count << ":\n";

        cout << "  Leg 1:\n";
        cout << "    Destination: " << current->leg1->destinationPort << "\n";
        cout << "    Date: " << current->leg1->voyageDate.day << "/"
             << current->leg1->voyageDate.month << "/"
             << current->leg1->voyageDate.year << "\n";
        cout << "    Departure: " << current->leg1->departureTime.hour << ":"
             << (current->leg1->departureTime.minute < 10 ? "0" : "")
             << current->leg1->departureTime.minute << "\n";
        cout << "    Arrival: " << current->leg1->arrivalTime.hour << ":"
             << (current->leg1->arrivalTime.minute < 10 ? "0" : "")
             << current->leg1->arrivalTime.minute << "\n";
        cout << "    Cost: $" << current->leg1->voyageCost << "\n";
        cout << "    Company: " << current->leg1->shippingCompany << "\n";

        cout << "  Leg 2:\n";
        cout << "    Destination: " << current->leg2->destinationPort << "\n";
        cout << "    Date: " << current->leg2->voyageDate.day << "/"
             << current->leg2->voyageDate.month << "/"
             << current->leg2->voyageDate.year << "\n";
        cout << "    Departure: " << current->leg2->departureTime.hour << ":"
             << (current->leg2->departureTime.minute < 10 ? "0" : "")
             << current->leg2->departureTime.minute << "\n";
        cout << "    Arrival: " << current->leg2->arrivalTime.hour << ":"
             << (current->leg2->arrivalTime.minute < 10 ? "0" : "")
             << current->leg2->arrivalTime.minute << "\n";
        cout << "    Cost: $" << current->leg2->voyageCost << "\n";
        cout << "    Company: " << current->leg2->shippingCompany << "\n";

        cout << "  Leg 3:\n";
        cout << "    Destination: " << current->leg3->destinationPort << "\n";
        cout << "    Date: " << current->leg3->voyageDate.day << "/"
             << current->leg3->voyageDate.month << "/"
             << current->leg3->voyageDate.year << "\n";
        cout << "    Departure: " << current->leg3->departureTime.hour << ":"
             << (current->leg3->departureTime.minute < 10 ? "0" : "")
             << current->leg3->departureTime.minute << "\n";
        cout << "    Arrival: " << current->leg3->arrivalTime.hour << ":"
             << (current->leg3->arrivalTime.minute < 10 ? "0" : "")
             << current->leg3->arrivalTime.minute << "\n";
        cout << "    Cost: $" << current->leg3->voyageCost << "\n";
        cout << "    Company: " << current->leg3->shippingCompany << "\n";

        cout << "  Leg 4:\n";
        cout << "    Destination: " << current->leg4->destinationPort << "\n";
        cout << "    Date: " << current->leg4->voyageDate.day << "/"
             << current->leg4->voyageDate.month << "/"
             << current->leg4->voyageDate.year << "\n";
        cout << "    Departure: " << current->leg4->departureTime.hour << ":"
             << (current->leg4->departureTime.minute < 10 ? "0" : "")
             << current->leg4->departureTime.minute << "\n";
        cout << "    Arrival: " << current->leg4->arrivalTime.hour << ":"
             << (current->leg4->arrivalTime.minute < 10 ? "0" : "")
             << current->leg4->arrivalTime.minute << "\n";
        cout << "    Cost: $" << current->leg4->voyageCost << "\n";
        cout << "    Company: " << current->leg4->shippingCompany << "\n";

        int totalCost = current->leg1->voyageCost + current->leg2->voyageCost +
                        current->leg3->voyageCost + current->leg4->voyageCost;
        cout << "  Total Cost: $" << totalCost << "\n\n";

        count++;
        current = current->next;
    }
}

void printFiveLegRoutes(FiveLegRoute* head) {
    if (!head) {
        cout << "No four-stop routes found.\n";
        return;
    }

    int count = 1;
    FiveLegRoute* current = head;
    while (current) {
        cout << "Four-Stop Route " << count << ":\n";

        cout << "  Leg 1:\n";
        cout << "    Destination: " << current->leg1->destinationPort << "\n";
        cout << "    Date: " << current->leg1->voyageDate.day << "/"
             << current->leg1->voyageDate.month << "/"
             << current->leg1->voyageDate.year << "\n";
        cout << "    Departure: " << current->leg1->departureTime.hour << ":"
             << (current->leg1->departureTime.minute < 10 ? "0" : "")
             << current->leg1->departureTime.minute << "\n";
        cout << "    Arrival: " << current->leg1->arrivalTime.hour << ":"
             << (current->leg1->arrivalTime.minute < 10 ? "0" : "")
             << current->leg1->arrivalTime.minute << "\n";
        cout << "    Cost: $" << current->leg1->voyageCost << "\n";
        cout << "    Company: " << current->leg1->shippingCompany << "\n";

        cout << "  Leg 2:\n";
        cout << "    Destination: " << current->leg2->destinationPort << "\n";
        cout << "    Date: " << current->leg2->voyageDate.day << "/"
             << current->leg2->voyageDate.month << "/"
             << current->leg2->voyageDate.year << "\n";
        cout << "    Departure: " << current->leg2->departureTime.hour << ":"
             << (current->leg2->departureTime.minute < 10 ? "0" : "")
             << current->leg2->departureTime.minute << "\n";
        cout << "    Arrival: " << current->leg2->arrivalTime.hour << ":"
             << (current->leg2->arrivalTime.minute < 10 ? "0" : "")
             << current->leg2->arrivalTime.minute << "\n";
        cout << "    Cost: $" << current->leg2->voyageCost << "\n";
        cout << "    Company: " << current->leg2->shippingCompany << "\n";

        cout << "  Leg 3:\n";
        cout << "    Destination: " << current->leg3->destinationPort << "\n";
        cout << "    Date: " << current->leg3->voyageDate.day << "/"
             << current->leg3->voyageDate.month << "/"
             << current->leg3->voyageDate.year << "\n";
        cout << "    Departure: " << current->leg3->departureTime.hour << ":"
             << (current->leg3->departureTime.minute < 10 ? "0" : "")
             << current->leg3->departureTime.minute << "\n";
        cout << "    Arrival: " << current->leg3->arrivalTime.hour << ":"
             << (current->leg3->arrivalTime.minute < 10 ? "0" : "")
             << current->leg3->arrivalTime.minute << "\n";
        cout << "    Cost: $" << current->leg3->voyageCost << "\n";
        cout << "    Company: " << current->leg3->shippingCompany << "\n";

        cout << "  Leg 4:\n";
        cout << "    Destination: " << current->leg4->destinationPort << "\n";
        cout << "    Date: " << current->leg4->voyageDate.day << "/"
             << current->leg4->voyageDate.month << "/"
             << current->leg4->voyageDate.year << "\n";
        cout << "    Departure: " << current->leg4->departureTime.hour << ":"
             << (current->leg4->departureTime.minute < 10 ? "0" : "")
             << current->leg4->departureTime.minute << "\n";
        cout << "    Arrival: " << current->leg4->arrivalTime.hour << ":"
             << (current->leg4->arrivalTime.minute < 10 ? "0" : "")
             << current->leg4->arrivalTime.minute << "\n";
        cout << "    Cost: $" << current->leg4->voyageCost << "\n";
        cout << "    Company: " << current->leg4->shippingCompany << "\n";

        cout << "  Leg 5:\n";
        cout << "    Destination: " << current->leg5->destinationPort << "\n";
        cout << "    Date: " << current->leg5->voyageDate.day << "/"
             << current->leg5->voyageDate.month << "/"
             << current->leg5->voyageDate.year << "\n";
        cout << "    Departure: " << current->leg5->departureTime.hour << ":"
             << (current->leg5->departureTime.minute < 10 ? "0" : "")
             << current->leg5->departureTime.minute << "\n";
        cout << "    Arrival: " << current->leg5->arrivalTime.hour << ":"
             << (current->leg5->arrivalTime.minute < 10 ? "0" : "")
             << current->leg5->arrivalTime.minute << "\n";
        cout << "    Cost: $" << current->leg5->voyageCost << "\n";
        cout << "    Company: " << current->leg5->shippingCompany << "\n";

        int totalCost = current->leg1->voyageCost + current->leg2->voyageCost +
                        current->leg3->voyageCost + current->leg4->voyageCost +
                        current->leg5->voyageCost;
        cout << "  Total Cost: $" << totalCost << "\n\n";

        count++;
        current = current->next;
    }
}

void freeDirectList(Route* head) {
    while (head) {
      Route* temp = head;
        head = head->next;
    delete temp;
    }
}

void freeTwoLegList(TwoLegRoute* head) {
    while (head) {
        TwoLegRoute* temp = head;
        head = head->next;

     if (temp->leg1) delete temp->leg1;
        if (temp->leg2) delete temp->leg2;

        delete temp;
    }
}

void freeThreeLegList(ThreeLegRoute* head) {
    while (head) {
        ThreeLegRoute* temp = head;
    head = head->next;

        if (temp->leg1) delete temp->leg1;
   if (temp->leg2) delete temp->leg2;
        if (temp->leg3) delete temp->leg3;

        delete temp;
    }
}

void freeFourLegList(FourLegRoute* head) {
    while (head) {
        FourLegRoute* temp = head;
        head = head->next;

        if (temp->leg1) delete temp->leg1;
        if (temp->leg2) delete temp->leg2;
        if (temp->leg3) delete temp->leg3;
        if (temp->leg4) delete temp->leg4;

        delete temp;
    }
}

void freeFiveLegList(FiveLegRoute* head) {
    while (head) {
        FiveLegRoute* temp = head;
        head = head->next;

        if (temp->leg1) delete temp->leg1;
        if (temp->leg2) delete temp->leg2;
        if (temp->leg3) delete temp->leg3;
        if (temp->leg4) delete temp->leg4;
        if (temp->leg5) delete temp->leg5;

        delete temp;
    }
}

void searchCustomRoute(Graph& g, const string& origin, const string& destination,
    int day, int month, int year) {
    cout << "\n========================================\n";
    cout << "  SEARCHING ROUTES: " << origin << " -> " << destination << "\n";
    cout << "  Date: " << day << "/" << month << "/" << year << "\n";
    cout << "========================================\n\n";

    Date searchDate = {day, month, year};
    Route* direct = nullptr;
    TwoLegRoute* oneStop = nullptr;
    ThreeLegRoute* twoStop = nullptr;
    FourLegRoute* threeStop = nullptr;
    FiveLegRoute* fourStop = nullptr;

    getAllPossibleRoutes(g, origin, destination, searchDate,
        direct, oneStop, twoStop, threeStop, fourStop);

    int directCount = 0, oneStopCount = 0, twoStopCount = 0;
    Route* r = direct;
    while (r) { directCount++; r = r->next; }
    TwoLegRoute* tl = oneStop;
    while (tl) { oneStopCount++; tl = tl->next; }
    ThreeLegRoute* ttl = twoStop;
  while (ttl) { twoStopCount++; ttl = ttl->next; }

    cout << "SEARCH SUMMARY:\n";
    cout << "  Direct routes found: " << directCount << "\n";
    cout << "One-stop routes found: " << oneStopCount << "\n";
    cout << "  Two-stop routes found: " << twoStopCount << "\n";
    cout << "  Total routes found: " << (directCount + oneStopCount + twoStopCount) << "\n\n";

    if (directCount > 0) {
        cout << "===== DIRECT ROUTES =====\n";
        printDirectRoutes(direct);
    }

    if (oneStopCount > 0) {
   cout << "===== ONE-STOP ROUTES =====\n";
        printTwoLegRoutes(oneStop);
    }

    if (twoStopCount > 0) {
        cout << "===== TWO-STOP ROUTES =====\n";
        printThreeLegRoutes(twoStop);
    }

    if (directCount == 0 && oneStopCount == 0 && twoStopCount == 0) {
  cout << "No routes found for this origin-destination-date combination.\n";
        cout << "Suggestions:\n";
        cout << "  - Check if the port names are spelled correctly\n";
   cout << "  - Try a different date (check Routes.txt for available dates)\n";
        cout << "  - Try searching from a different origin port\n\n";
    }

    freeDirectList(direct);
    freeTwoLegList(oneStop);
    freeThreeLegList(twoStop);
    freeFourLegList(threeStop);
    freeFiveLegList(fourStop);

    cout << "========================================\n\n";
}

void testOneStopRoutes(Graph& g) {
  cout << "\n========================================\n";
  cout << "   DEDICATED ONE-STOP ROUTE TESTS\n";
    cout << "========================================\n\n";
    cout << "Testing one-stop (two-leg) routing functionality:\n";
    cout << "- Intermediate port identification\n";
    cout << "  - Layover feasibility (minimum 1 hour)\n";
    cout << "  - Same-day routing constraint\n";
    cout << "  - Total cost calculation\n\n";

 cout << ">>> TEST CASE 1: Tokyo -> Mumbai via intermediate hub <<<\n";
    cout << "Date: 8/12/2024\n";
    cout << "Expected: Route through Colombo or Dubai if timing allows\n";
 cout << "------------------------------------------------------\n";
  Date date1 = {8, 12, 2024};
    TwoLegRoute* routes1 = getOneStopConnections(g, "Tokyo", "Mumbai", date1);

    if (routes1) {
   int count = 0;
      TwoLegRoute* temp = routes1;
  while (temp) { count++; temp = temp->next; }
        cout << "Found " << count << " one-stop route(s):\n\n";
      printTwoLegRoutes(routes1);

   temp = routes1;
        cout << "Layover Analysis:\n";
  while (temp) {

         if (compareDates(temp->leg1->voyageDate, temp->leg2->voyageDate) == 0) {
      int layoverMins = (temp->leg2->departureTime.hour * 60 + temp->leg2->departureTime.minute) -
        (temp->leg1->arrivalTime.hour * 60 + temp->leg1->arrivalTime.minute);
   cout << "  Via " << temp->leg1->destinationPort
 << ": " << layoverMins << " minutes layover\n";
            }
      temp = temp->next;
        }
        cout << "\n";
    } else {
  cout << "No one-stop routes found. This could mean:\n";
   cout << "  - No same-day connections available\n";
        cout << "  - Layover times don't meet 1-hour minimum\n\n";
    }
    freeTwoLegList(routes1);

cout << ">>> TEST CASE 2: Singapore -> Istanbul via intermediate <<<\n";
    cout << "Date: 19/12/2024\n";
    cout << "------------------------------------------------------\n";
    Date date2 = {19, 12, 2024};
    TwoLegRoute* routes2 = getOneStopConnections(g, "Singapore", "Istanbul", date2);

    if (routes2) {
 int count = 0;
        TwoLegRoute* temp = routes2;
while (temp) { count++; temp = temp->next; }
   cout << "Found " << count << " one-stop route(s):\n\n";
 printTwoLegRoutes(routes2);
    } else {
 cout << "No one-stop routes found.\n\n";
    }
  freeTwoLegList(routes2);

    cout << ">>> TEST CASE 3: HongKong -> Genoa (high-traffic date) <<<\n";
  cout << "Date: 12/12/2024 (16 routes available this date)\n";
    cout << "------------------------------------------------------\n";
    Date date3 = {12, 12, 2024};
    TwoLegRoute* routes3 = getOneStopConnections(g, "HongKong", "Genoa", date3);

    if (routes3) {
        int count = 0;
      TwoLegRoute* temp = routes3;
   while (temp) { count++; temp = temp->next; }
        cout << "Found " << count << " one-stop route(s):\n\n";
   printTwoLegRoutes(routes3);
    } else {
 cout << "No one-stop routes found.\n\n";
    }
freeTwoLegList(routes3);

    cout << "========================================\n";
    cout << "   ONE-STOP TESTS COMPLETED\n";
    cout << "========================================\n\n";
}

void testTwoStopRoutes(Graph& g) {
    cout << "\n========================================\n";
    cout << "   DEDICATED TWO-STOP ROUTE TESTS\n";
    cout << "========================================\n\n";
    cout << "Testing two-stop (three-leg) routing functionality:\n";
  cout << "  - Multiple intermediate port connections\n";
  cout << "  - Layover feasibility at each stop\n";
    cout << "  - Complex routing logic\n";
  cout << "  - Total journey cost\n\n";

    cout << ">>> TEST CASE 1: Karachi -> Vancouver (long distance) <<<\n";
    cout << "Date: 22/12/2024\n";
    cout << "Expected: Multi-hop through Asian and North American ports\n";
    cout << "------------------------------------------------------\n";
Date date1 = {22, 12, 2024};
    ThreeLegRoute* routes1 = getTwoStopConnections(g, "Karachi", "Vancouver", date1);

    if (routes1) {
     int count = 0;
        ThreeLegRoute* temp = routes1;
  while (temp) { count++; temp = temp->next; }
        cout << "Found " << count << " two-stop route(s):\n\n";
        printThreeLegRoutes(routes1);

     temp = routes1;
      cout << "Journey Analysis:\n";
      while (temp) {

  if (compareDates(temp->leg1->voyageDate, temp->leg2->voyageDate) == 0 &&
      compareDates(temp->leg2->voyageDate, temp->leg3->voyageDate) == 0) {
      int layover1 = (temp->leg2->departureTime.hour * 60 + temp->leg2->departureTime.minute) -
         (temp->leg1->arrivalTime.hour * 60 + temp->leg1->arrivalTime.minute);
       int layover2 = (temp->leg3->departureTime.hour * 60 + temp->leg3->departureTime.minute) -
        (temp->leg2->arrivalTime.hour * 60 + temp->leg2->arrivalTime.minute);
     cout << "  Route: Karachi -> " << temp->leg1->destinationPort
    << " (" << layover1 << " min layover) -> "
   << temp->leg2->destinationPort
 << " (" << layover2 << " min layover) -> Vancouver\n";
   }
            temp = temp->next;
   }
        cout << "\n";
    } else {
  cout << "No two-stop routes found.\n\n";
    }
  freeThreeLegList(routes1);

    cout << ">>> TEST CASE 2: Singapore -> Sydney via two hubs <<<\n";
    cout << "Date: 12/12/2024\n";
    cout << "------------------------------------------------------\n";
    Date date2 = {12, 12, 2024};
    ThreeLegRoute* routes2 = getTwoStopConnections(g, "Singapore", "Sydney", date2);

 if (routes2) {
        int count = 0;
     ThreeLegRoute* temp = routes2;
      while (temp) { count++; temp = temp->next; }
   cout << "Found " << count << " two-stop route(s):\n\n";
        printThreeLegRoutes(routes2);
    } else {
        cout << "No two-stop routes found.\n\n";
  }
    freeThreeLegList(routes2);

    cout << ">>> TEST CASE 3: Tokyo -> London (cross-continent) <<<\n";
    cout << "Date: 8/12/2024\n";
    cout << "------------------------------------------------------\n";
    Date date3 = {8, 12, 2024};
    ThreeLegRoute* routes3 = getTwoStopConnections(g, "Tokyo", "London", date3);

    if (routes3) {
     int count = 0;
        ThreeLegRoute* temp = routes3;
        while (temp) { count++; temp = temp->next; }
      cout << "Found " << count << " two-stop route(s):\n\n";
  printThreeLegRoutes(routes3);
  } else {
 cout << "No two-stop routes found.\n\n";
    }
    freeThreeLegList(routes3);

    cout << "========================================\n";
    cout << "   TWO-STOP TESTS COMPLETED\n";
    cout << "========================================\n\n";
}

void testDay2(Graph& g) {
    cout << "\n========================================\n";
    cout << "       DAY 2 ROUTE SEARCH TESTS\n";
    cout << "========================================\n\n";

    cout << "TEST 1: Direct routes from Karachi on 22/12/2024\n";
    cout << "------------------------------------------------\n";
    Date testDate1 = {22, 12, 2024};
    Route* directRoutes = getDirectRoutes(g, "Karachi", testDate1);
    printDirectRoutes(directRoutes);
    freeDirectList(directRoutes);

    cout << "\nTEST 2: One-stop routes from Tokyo to Mumbai on 8/12/2024\n";
    cout << "------------------------------------------------------------\n";
    cout << "This test checks:\n";
    cout << "  - Finding intermediate connection points\n";
    cout << "  - Validating 1-hour minimum layover\n";
    cout << "  - Same-day routing constraint\n\n";
    Date testDate2 = {8, 12, 2024};
    TwoLegRoute* oneStopRoutes = getOneStopConnections(g, "Tokyo", "Mumbai", testDate2);
 printTwoLegRoutes(oneStopRoutes);
    freeTwoLegList(oneStopRoutes);

    cout << "\nTEST 3: Two-stop routes from Singapore to Mumbai on 8/12/2024\n";
    cout << "----------------------------------------------------------------\n";
    cout << "This test checks:\n";
    cout << "  - Finding multi-hop routes through two intermediate ports\n";
cout << "  - Validating layovers at both stops\n";
  cout << "  - Complex route planning\n\n";
    ThreeLegRoute* twoStopRoutes = getTwoStopConnections(g, "Singapore", "Mumbai", testDate2);
    printThreeLegRoutes(twoStopRoutes);
    freeThreeLegList(twoStopRoutes);

    cout << "\nTEST 4: One-stop routes from HongKong to Istanbul on 19/12/2024\n";
    cout << "-----------------------------------------------------------------\n";
    cout << "Testing connections through major shipping hubs\n\n";
    Date testDate3 = {19, 12, 2024};
    oneStopRoutes = getOneStopConnections(g, "HongKong", "Istanbul", testDate3);
    printTwoLegRoutes(oneStopRoutes);
    freeTwoLegList(oneStopRoutes);

    cout << "\nTEST 5: Two-stop routes from Karachi to Vancouver on 22/12/2024\n";
    cout << "----------------------------------------------------------------\n";
    cout << "Testing long-distance multi-hop routing\n\n";
    twoStopRoutes = getTwoStopConnections(g, "Karachi", "Vancouver", testDate1);
    printThreeLegRoutes(twoStopRoutes);
    freeThreeLegList(twoStopRoutes);

    cout << "\nTEST 6: All possible routes from Mumbai to Genoa on 9/12/2024\n";
    cout << "-----------------------------------------------------------------\n";
    Date testDate4 = {9, 12, 2024};
    Route* allDirect = nullptr;
    TwoLegRoute* allOneStop = nullptr;
    ThreeLegRoute* allTwoStop = nullptr;
    FourLegRoute* allThreeStop = nullptr;
    FiveLegRoute* allFourStop = nullptr;

    getAllPossibleRoutes(g, "Mumbai", "Genoa", testDate4,
        allDirect, allOneStop, allTwoStop, allThreeStop, allFourStop);

    cout << "DIRECT ROUTES:\n";
    printDirectRoutes(allDirect);

    cout << "ONE-STOP ROUTES:\n";
    printTwoLegRoutes(allOneStop);

    cout << "TWO-STOP ROUTES:\n";
    printThreeLegRoutes(allTwoStop);

    freeDirectList(allDirect);
    freeTwoLegList(allOneStop);
    freeThreeLegList(allTwoStop);
    freeFourLegList(allThreeStop);
    freeFiveLegList(allFourStop);

    cout << "\nTEST 7: One-stop routes from Singapore to Busan on 12/12/2024\n";
    cout << "--------------------------------------------------------------------\n";
    cout << "Date with 16 available routes - good for finding connections\n\n";
    Date testDate5 = {12, 12, 2024};
    oneStopRoutes = getOneStopConnections(g, "Singapore", "Busan", testDate5);
    printTwoLegRoutes(oneStopRoutes);
    freeTwoLegList(oneStopRoutes);

    cout << "\nTEST 8: Two-stop routes from Tokyo to Genoa on 12/12/2024\n";
    cout << "--------------------------------------------------------------------\n";
    twoStopRoutes = getTwoStopConnections(g, "Tokyo", "Genoa", testDate5);
    printThreeLegRoutes(twoStopRoutes);
    freeThreeLegList(twoStopRoutes);

    cout << "\nTEST 9: All routes from Dubai to Jeddah on 15/12/2024\n";
    cout << "--------------------------------------------------------------------\n";
    Date testDate6 = {15, 12, 2024};
    allDirect = nullptr;
    allOneStop = nullptr;
    allTwoStop = nullptr;
    allThreeStop = nullptr;
    allFourStop = nullptr;

    getAllPossibleRoutes(g, "Dubai", "Jeddah", testDate6,
        allDirect, allOneStop, allTwoStop, allThreeStop, allFourStop);

    cout << "DIRECT ROUTES:\n";
    printDirectRoutes(allDirect);

    cout << "ONE-STOP ROUTES:\n";
    printTwoLegRoutes(allOneStop);

    cout << "TWO-STOP ROUTES:\n";
    printThreeLegRoutes(allTwoStop);

    freeDirectList(allDirect);
    freeTwoLegList(allOneStop);
    freeThreeLegList(allTwoStop);
    freeFourLegList(allThreeStop);
    freeFiveLegList(allFourStop);

    cout << "\n========================================\n";
    cout << "       DAY 2 TESTS COMPLETED\n";
    cout << "========================================\n\n";
}
