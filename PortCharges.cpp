#include "PortCharges.h"
#include <fstream>
#include <iostream>

using namespace std;

PortChargeNode::PortChargeNode() {
    dailyCharge = 0;
    next = nullptr;
}

PortChargeList::PortChargeList() {
    head = nullptr;
}

void addPortCharge(PortChargeList& list, const string& name, int charge) {
    PortChargeNode* newNode = new PortChargeNode();
    newNode->portName = name;
    newNode->dailyCharge = charge;
    newNode->next = list.head;
    list.head = newNode;
}

bool loadPortChargesFromFile(PortChargeList& list, const string& filePath) {
    ifstream in(filePath);
    if (!in) {
        cerr << "Error: Could not open file " << filePath << endl;
        return false;
    }

    string name;
    int charge;
    while (in >> name >> charge) {
        addPortCharge(list, name, charge);
    }

    in.close();
    return true;
}

PortChargeNode* findPortCharge(PortChargeList& list, const string& name) {
    PortChargeNode* current = list.head;
    while (current != nullptr) {
        if (current->portName == name) {
            return current;
        }
        current = current->next;
    }
    return nullptr;
}

void applyPortChargesToGraph(PortChargeList& list, Graph& g) {
    Port* currentPort = g.portHead;
    while (currentPort != nullptr) {
        PortChargeNode* chargeNode = findPortCharge(list, currentPort->name);
        if (chargeNode != nullptr) {
            currentPort->dailyCharge = chargeNode->dailyCharge;
        }

        currentPort = currentPort->next;
    }
}

void clearPortChargeList(PortChargeList& list) {
    PortChargeNode* current = list.head;
    while (current != nullptr) {
        PortChargeNode* temp = current;
        current = current->next;
        delete temp;
    }
    list.head = nullptr;
}
