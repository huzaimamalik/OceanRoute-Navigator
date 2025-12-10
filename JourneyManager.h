#ifndef JOURNEY_MANAGER_H
#define JOURNEY_MANAGER_H

#include "Journey.h"

struct JourneyNode {
    BookedJourney journey;
    int           id;
    JourneyNode*  next;

    JourneyNode();
};

struct JourneyManager {
    JourneyNode* head;
    int          nextId;
    int          count;

    JourneyManager();
};

void initJourneyManager(JourneyManager& jm);

int addJourney(JourneyManager& jm, const BookedJourney& journey);

void clearJourneyManager(JourneyManager& jm);

#endif
