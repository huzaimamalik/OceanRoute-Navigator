#pragma once

#include <string>
#include <iostream>
#include "Route.h"

using namespace std;

struct Port {
 string name;
 string normalizedName;
 Route *routeHead;
 Port *next;
 int dailyCharge;

 Port() : name(), normalizedName(), routeHead(nullptr), next(nullptr), dailyCharge(-1) {}
};

struct Graph {
 Port *portHead;
 int portCount;
 Graph() : portHead(nullptr), portCount(0) {}
};

Port* findPort(Graph &g, const string &name);

Port* addPortIfNotExists(Graph &g, const string &name);

void addRoute(Graph &g, const string &origin, const string &destination, const Date &date, const Time &dep, const Time &arr, int cost, const string &company);

bool loadRoutesFromFile(Graph &g, const string &filePath);

void freeGraph(Graph &g);
