#ifndef PORT_CHARGES_H
#define PORT_CHARGES_H

#include <string>
#include "Graph.h"

using namespace std;

struct PortChargeNode {
    string portName;
    int dailyCharge;
    PortChargeNode* next;

    PortChargeNode();
};

struct PortChargeList {
    PortChargeNode* head;

    PortChargeList();
};

void addPortCharge(PortChargeList& list, const string& name, int charge);
bool loadPortChargesFromFile(PortChargeList& list, const string& filePath);
PortChargeNode* findPortCharge(PortChargeList& list, const string& name);

void applyPortChargesToGraph(PortChargeList& list, Graph& g);

void clearPortChargeList(PortChargeList& list);

#endif
