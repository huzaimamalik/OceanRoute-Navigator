#include "DockingManager.h"
#include <cstdlib>
#include <cstdio>
#include <ctime>

DockingManager::DockingManager()
    : portHead(nullptr), currentTimeMinutes(0), isPlaying(false), timeStep(5) {
}

DockingManager::~DockingManager() {
    while (portHead) {
        PortDockingData* temp = portHead;
        portHead = portHead->next;
        delete temp;
    }
}

PortDockingData* DockingManager::findPort(const char* portName) {
    PortDockingData* current = portHead;
    while (current) {
        if (strcmp(current->portName, portName) == 0) {
            return current;
        }
        current = current->next;
    }
    return nullptr;
}

void DockingManager::initializePorts(const char portNames[][30], int numPorts) {

    while (portHead) {
        PortDockingData* temp = portHead;
        portHead = portHead->next;
        delete temp;
    }
    portHead = nullptr;

    for (int i = 0; i < numPorts; i++) {
        PortDockingData* newPort = new PortDockingData();

        int capacity = 2;
        if (strcmp(portNames[i], "Singapore") == 0 || strcmp(portNames[i], "Rotterdam") == 0 ||
            strcmp(portNames[i], "Shanghai") == 0 || strcmp(portNames[i], "Dubai") == 0) {
            capacity = 4;
        } else if (strcmp(portNames[i], "Karachi") == 0 || strcmp(portNames[i], "Mumbai") == 0 ||
                   strcmp(portNames[i], "Hamburg") == 0 || strcmp(portNames[i], "Antwerp") == 0) {
            capacity = 3;
        }

        newPort->setPortName(portNames[i], capacity);
        newPort->next = portHead;
        portHead = newPort;
    }

    generateDemoSchedule();
}

void DockingManager::generateDemoSchedule() {
    const char* companies[] = {"MaerskLine", "CMA_CGM", "COSCO", "HapagLloyd", "MSC"};
    const char* shipTypes[] = {"Container", "Tanker", "Bulk"};
    int numCompanies = 5;
    int numTypes = 3;

    struct RoutePair {
        const char* origin;
        const char* dest;
    };

    RoutePair routes[] = {{"Singapore", "Dubai"}, {"Dubai", "Rotterdam"}, {"Rotterdam", "NewYork"}, {"Shanghai", "LosAngeles"}, {"Karachi", "Dubai"}, {"Mumbai", "Singapore"}, {"HongKong", "Tokyo"}, {"Busan", "Shanghai"}, {"Sydney", "Singapore"}, {"Rotterdam", "Hamburg"}, {"Dubai", "Mumbai"}, {"Singapore", "HongKong"}};
    int numRoutes = 12;

    int shipCounter = 0;
    int numShips = 15 + (rand() % 6);

    for (int i = 0; i < numShips; i++) {
        const char* company = companies[shipCounter % numCompanies];
        const char* shipType = shipTypes[shipCounter % numTypes];

        RoutePair route = routes[shipCounter % numRoutes];

        PortDockingData* originPort = findPort(route.origin);
        PortDockingData* destPort = findPort(route.dest);

        if (!originPort || !destPort) {
            shipCounter++;
            continue;
        }

        int depDay = 24;
        int depMonth = 12;
        int depYear = 2024;

        int depTime = (i * 60 + (shipCounter % 180)) % 1440;

        int voyageTime = 120 + (shipCounter % 360);

        int serviceTime = 60 + (shipCounter % 120);

        char shipId[50];
        snprintf(shipId, 50, "%s_%s_%03d", company, shipType, shipCounter);

        DockingShip ship;
        ship.setData(shipId, company, shipType, route.origin, route.dest, depDay, depMonth, depYear, depTime, voyageTime, serviceTime);

        DockingShipQueue* queue = originPort->getQueue(company, shipType);
        if (queue) {
            queue->enqueue(ship);
        }

        shipCounter++;
    }
}

void DockingManager::enqueueShip(const DockingShip& ship) {
    PortDockingData* port = findPort(ship.currentPort);
    if (!port) return;

    DockingShipQueue* queue = port->getQueue(ship.company, ship.shipType);
    if (queue) {
        queue->enqueue(ship);
    }
}

// Updates ship positions and states in real-time simulation without time limit
void DockingManager::updateSimulation() {
    if (!isPlaying) return;

    currentTimeMinutes += timeStep;

    bool anyShipActive = false;

    PortDockingData* port = portHead;
    while (port) {

        CompanyQueueNode* cNode = port->companyHead;
        while (cNode) {
            TypeQueueNode* tNode = cNode->typeHead;
            while (tNode) {
                DockingShipQueue tempQueue;

                while (!tNode->queue.isEmpty()) {
                    DockingShip ship;
                    tNode->queue.dequeue(ship);

                    if (currentTimeMinutes >= ship.departureTimeMinutes && !ship.completed) {
                        anyShipActive = true;

                        int elapsedTime = currentTimeMinutes - ship.departureTimeMinutes;
                        ship.animationProgress = (float)elapsedTime / (float)ship.voyageDurationMinutes;

                        if (ship.animationProgress >= 1.0f) {
                            ship.animationProgress = 1.0f;
                            ship.completed = true;
                            strncpy(ship.currentPort, ship.destinationPort, 29);
                            ship.currentPort[29] = '\0';

                            PortDockingData* destPort = findPort(ship.destinationPort);
                            if (destPort) {
                                DockingShipQueue* destQueue = destPort->getQueue(ship.company, ship.shipType);
                                if (destQueue) {
                                    destQueue->enqueue(ship);
                                }
                            }
                        } else {

                            tempQueue.enqueue(ship);
                        }
                    } else if (!ship.completed) {
                        anyShipActive = true;

                        tempQueue.enqueue(ship);
                    }
                }

                while (!tempQueue.isEmpty()) {
                    DockingShip ship;
                    tempQueue.dequeue(ship);
                    tNode->queue.enqueue(ship);
                }

                tNode = tNode->next;
            }
            cNode = cNode->next;
        }

        for (int i = port->currentlyDocked.count - 1; i >= 0; i--) {
            if (port->currentlyDocked.ships[i].dockingStartTime >= 0 &&
                currentTimeMinutes >= port->currentlyDocked.ships[i].dockingStartTime +
                                      port->currentlyDocked.ships[i].serviceDuration) {
                port->currentlyDocked.removeAt(i);
            }
        }

        if (port->currentlyDocked.count > 0) anyShipActive = true;

        while (port->currentlyDocked.count < port->maxActiveDocks) {
            DockingShip* nextShip = nullptr;
            char selectedCompany[30] = "";
            char selectedType[20] = "";
            int earliestArrival = INT_MAX;

            cNode = port->companyHead;
            while (cNode) {
                TypeQueueNode* tNode = cNode->typeHead;
                while (tNode) {
                    if (!tNode->queue.isEmpty()) {
                        const DockingShip* frontShip = tNode->queue.peek();

                        if (frontShip && frontShip->completed &&
                            frontShip->arrivalTimeMinutes < earliestArrival) {
                            earliestArrival = frontShip->arrivalTimeMinutes;
                            nextShip = (DockingShip*)frontShip;
                            strncpy(selectedCompany, cNode->company, 29);
                            selectedCompany[29] = '\0';
                            strncpy(selectedType, tNode->shipType, 19);
                            selectedType[19] = '\0';
                        }
                    }
                    tNode = tNode->next;
                }
                cNode = cNode->next;
            }

            if (nextShip) {
                anyShipActive = true;
                DockingShip dockingShip = *nextShip;
                dockingShip.dockingStartTime = currentTimeMinutes;
                port->currentlyDocked.add(dockingShip);

                DockingShipQueue* queue = port->getQueue(selectedCompany, selectedType);
                if (queue) {
                    DockingShip temp;
                    queue->dequeue(temp);
                }
            } else {
                break;
            }
        }

        port = port->next;
    }

    if (!anyShipActive) {
        pause();
    }
}

void DockingManager::stepForward(int minutes) {
    currentTimeMinutes += minutes;
    if (currentTimeMinutes >= 1440) {
        currentTimeMinutes = 1439;
    }
}

void DockingManager::stepBackward(int minutes) {
    int targetTime = currentTimeMinutes - minutes;
    if (targetTime < 0) targetTime = 0;

    reset();

    while (currentTimeMinutes < targetTime) {
        stepForward(timeStep);
    }
}

void DockingManager::setTime(int minutes) {
    if (minutes < 0) minutes = 0;
    if (minutes >= 1440) minutes = 1439;

    if (minutes < currentTimeMinutes) {
        stepBackward(currentTimeMinutes - minutes);
    } else if (minutes > currentTimeMinutes) {
        stepForward(minutes - currentTimeMinutes);
    }
}

void DockingManager::reset() {
    currentTimeMinutes = 0;
    isPlaying = false;

    PortDockingData* port = portHead;
    while (port) {
        port->clear();
        port = port->next;
    }
}

// Loads actual routes from Routes.txt file matching the selected date for simulation
void DockingManager::loadRoutesForDate(const char* filename, int day, int month, int year) {
    PortDockingData* port = portHead;
    while (port) {
        port->clear();
        port = port->next;
    }

    FILE* file = fopen(filename, "r");
    if (!file) {
        generateDemoSchedule();
        return;
    }

    char line[512];
    int shipCounter = 0;

    while (fgets(line, sizeof(line), file)) {
        char origin[30], dest[30], company[30];
        int depDay, depMonth, depYear;
        int depHour, depMinute, arrHour, arrMinute;
        int cost;

        int parsed = sscanf(line, "%s %s %d/%d/%d %d:%d %d:%d %d %s",
                           origin, dest, &depDay, &depMonth, &depYear,
                           &depHour, &depMinute, &arrHour, &arrMinute,
                           &cost, company);

        if (parsed != 11) continue;

        if (depDay != day || depMonth != month || depYear != year) continue;

        PortDockingData* originPort = findPort(origin);
        if (!originPort) continue;

        int depTime = depHour * 60 + depMinute;
        int arrTime = arrHour * 60 + arrMinute;
        int voyageTime = arrTime - depTime;
        if (voyageTime < 0) voyageTime += 1440; // Handle day crossing

        const char* shipType;
        if (cost < 15000) shipType = "Container";
        else if (cost < 30000) shipType = "Tanker";
        else shipType = "Bulk";

        int serviceTime = 60 + (shipCounter % 120);

        char shipId[50];
        snprintf(shipId, 50, "%s_%s_%03d", company, shipType, shipCounter);

        DockingShip ship;
        ship.setData(shipId, company, shipType, origin, dest,
                    depDay, depMonth, depYear, depTime, voyageTime, serviceTime);

        DockingShipQueue* queue = originPort->getQueue(company, shipType);
        if (queue) {
            queue->enqueue(ship);
        }

        shipCounter++;
    }

    fclose(file);

    if (shipCounter == 0) {
        generateDemoSchedule();
    }
}

PortDockingData* DockingManager::getPortData(const char* portName) {
    return findPort(portName);
}

// Formats current simulation time with day overflow (HH:MM +Xd)
void DockingManager::getFormattedTime(char* buffer) const {
    int hours = (currentTimeMinutes / 60) % 24;  // Wrap hours to 0-23
    int minutes = currentTimeMinutes % 60;
    int days = currentTimeMinutes / 1440;  // Calculate additional days
    
    if (days > 0) {
        snprintf(buffer, 20, "%02d:%02d +%dd", hours, minutes, days);
    } else {
        snprintf(buffer, 20, "%02d:%02d", hours, minutes);
    }
}

int DockingManager::getShipsInTransit(DockingShip* buffer, int maxShips) const {
    int count = 0;

    PortDockingData* port = portHead;
    while (port && count < maxShips) {
        CompanyQueueNode* cNode = port->companyHead;
        while (cNode && count < maxShips) {
            TypeQueueNode* tNode = cNode->typeHead;
            while (tNode && count < maxShips) {

                DockingShipQueue tempQueue;
                DockingShipQueue& originalQueue = tNode->queue;

                DockingShip ship;
                while (!originalQueue.isEmpty() && count < maxShips) {
                    ((DockingShipQueue&)originalQueue).dequeue(ship);

                    if (ship.animationProgress > 0.0f && ship.animationProgress < 1.0f) {
                        // Ship is in transit
                        buffer[count++] = ship;
                    } else if (currentTimeMinutes < ship.departureTimeMinutes && ship.animationProgress == 0.0f) {
                        // Ship is waiting to depart - show it at origin with no movement
                        buffer[count++] = ship;
                    }

                    tempQueue.enqueue(ship);
                }

                while (!tempQueue.isEmpty()) {
                    tempQueue.dequeue(ship);
                    ((DockingShipQueue&)originalQueue).enqueue(ship);
                }

                tNode = tNode->next;
            }
            cNode = cNode->next;
        }
        port = port->next;
    }

    return count;
}
