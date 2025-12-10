

#include <iostream>
#include <fstream>
#include <sstream>
#include "Graph.h"
#include "DateTime.h"

using namespace std;

Port* findPort(Graph& g, const string& name) {
	Port* cur = g.portHead;
	while (cur) {
		if (cur->name == name) return cur;
		cur = cur->next;
	}
	return nullptr;
}

Port* addPortIfNotExists(Graph& g, const string& name) {
	Port* p = findPort(g, name);
	if (p) return p;
	p = new Port();
	p->name = name;
	p->routeHead = nullptr;
	p->next = g.portHead;
	g.portHead = p;
	g.portCount++;
	return p;
}

void addRoute(Graph& g, const string& origin, const string& destination, const Date& date, const Time& dep, const Time& arr, int cost, const string& company) {
	Port* originPort = addPortIfNotExists(g, origin);
	addPortIfNotExists(g, destination);
	Route* r = createRoute(destination, date, dep, arr, cost, company);
	originPort->routeHead = prependRoute(originPort->routeHead, r);
}

static bool parseLine(const string& line, string& origin, string& destination, Date& date, Time& dep, Time& arr, int& cost, string& company) {
	if (line.empty()) return false;
	istringstream iss(line);
	string dateStr, depStr, arrStr;
	if (!(iss >> origin >> destination >> dateStr >> depStr >> arrStr >> cost >> company)) {
		return false;
	}
	date = parseDate(dateStr);
	dep = parseTime(depStr);
	arr = parseTime(arrStr);
	return true;
}

// Parses Routes.txt and populates graph with all voyage routes
bool loadRoutesFromFile(Graph& g, const string& filePath) {
	ifstream fin(filePath.c_str());
	if (!fin.is_open()) {
		cout << "Failed to open routes file: " << filePath << endl;
		return false;
	}
	string line;
	int lineCount = 0;
	while (getline(fin, line)) {
		if (line.empty()) continue;
		string origin, destination, company; Date date; Time dep; Time arr; int cost;
		if (parseLine(line, origin, destination, date, dep, arr, cost, company)) {
			addRoute(g, origin, destination, date, dep, arr, cost, company);
			lineCount++;
		}
		else {
			cout << "Invalid line skipped: " << line << endl;
		}
	}
	cout << "Loaded " << lineCount << " routes." << endl;
	return true;
}

void freeGraph(Graph& g) {
	Port* p = g.portHead;
	while (p) {
		Route* r = p->routeHead;
		while (r) {
			Route* nextR = r->next;
			delete r;
			r = nextR;
		}
		Port* nextP = p->next;
		delete p;
		p = nextP;
	}
	g.portHead = nullptr;
	g.portCount = 0;
}
