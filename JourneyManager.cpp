#include "JourneyManager.h"
#include <iostream>

JourneyNode::JourneyNode() : id(-1), next(nullptr) {

}

JourneyManager::JourneyManager() : head(nullptr), nextId(1), count(0) {}

void initJourneyManager(JourneyManager& jm) {
    jm.head = nullptr;
    jm.nextId = 1;
    jm.count = 0;
}

BookedJourney deepCopyJourney(const BookedJourney& source) {
    BookedJourney copy;
    initJourney(copy);

    BookedLeg* cur = source.head;
    while (cur) {
        appendLeg(copy, cur->originPort, cur->destinationPort, cur->voyageDate, cur->departureTime, cur->arrivalTime, cur->voyageCost, cur->shippingCompany);
        cur = cur->next;
    }

    return copy;
}

int addJourney(JourneyManager& jm, const BookedJourney& journey) {
    JourneyNode* node = new JourneyNode();
    node->id = jm.nextId++;
    node->journey = deepCopyJourney(journey);
    node->next = nullptr;

    if (!jm.head) {
        jm.head = node;
    } else {

        JourneyNode* tail = jm.head;
        while (tail->next) tail = tail->next;
        tail->next = node;
    }

    jm.count++;
    return node->id;
}

void clearJourneyManager(JourneyManager& jm) {
    JourneyNode* cur = jm.head;
    while (cur) {
        JourneyNode* nxt = cur->next;
        clearJourney(cur->journey);
        delete cur;
        cur = nxt;
    }
    jm.head = nullptr;
    jm.nextId = 1;
    jm.count = 0;
}
