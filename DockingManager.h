#pragma once
#include <cstring>
#include <climits>

struct DockingShip {
    char id[50];
    char company[30];
    char shipType[20];
    char originPort[30];
    char destinationPort[30];
    char currentPort[30];
    int departureDay;
    int departureMonth;
    int departureYear;
    int departureTimeMinutes;
    int arrivalTimeMinutes;
    int voyageDurationMinutes;
    int serviceDuration;
    int dockingStartTime;
    float animationProgress;
    bool completed;
    DockingShip* next;

    DockingShip() : departureDay(0), departureMonth(0), departureYear(0), departureTimeMinutes(0), arrivalTimeMinutes(0), voyageDurationMinutes(0), serviceDuration(0), dockingStartTime(-1), animationProgress(0.0f), completed(false), next(nullptr) {
        id[0] = '\0';
        company[0] = '\0';
        shipType[0] = '\0';
        originPort[0] = '\0';
        destinationPort[0] = '\0';
        currentPort[0] = '\0';
    }

    void setData(const char* _id, const char* _company, const char* _shipType, const char* _origin, const char* _dest, int _depDay, int _depMonth, int _depYear, int _depTime, int _voyageTime, int _serviceDuration) {
        strncpy(id, _id, 49); id[49] = '\0';
        strncpy(company, _company, 29); company[29] = '\0';
        strncpy(shipType, _shipType, 19); shipType[19] = '\0';
        strncpy(originPort, _origin, 29); originPort[29] = '\0';
        strncpy(destinationPort, _dest, 29); destinationPort[29] = '\0';
        strncpy(currentPort, _origin, 29); currentPort[29] = '\0';
        departureDay = _depDay;
        departureMonth = _depMonth;
        departureYear = _depYear;
        departureTimeMinutes = _depTime;
        arrivalTimeMinutes = (_depTime + _voyageTime) % 1440;
        voyageDurationMinutes = _voyageTime;
        serviceDuration = _serviceDuration;
        dockingStartTime = -1;
        animationProgress = 0.0f;
        completed = false;
        next = nullptr;
    }
};

struct DockingShipQueue {
    DockingShip* front;
    DockingShip* rear;
    int size;

    DockingShipQueue() : front(nullptr), rear(nullptr), size(0) {}

    void enqueue(const DockingShip& ship) {
        DockingShip* newShip = new DockingShip();
        *newShip = ship;
        newShip->next = nullptr;

        if (rear == nullptr) {
            front = rear = newShip;
        } else {
            rear->next = newShip;
            rear = newShip;
        }
        size++;
    }

    bool dequeue(DockingShip& ship) {
        if (front == nullptr) return false;

        DockingShip* temp = front;
        ship = *front;
        ship.next = nullptr;
        front = front->next;

        if (front == nullptr) rear = nullptr;

        delete temp;
        size--;
        return true;
    }

    bool isEmpty() const { return front == nullptr; }

    const DockingShip* peek() const { return front; }

    void clear() {
        DockingShip temp;
        while (dequeue(temp)) {}
    }
};

struct TypeQueueNode {
    char shipType[20];
    DockingShipQueue queue;
    TypeQueueNode* next;

    TypeQueueNode() : next(nullptr) {
        shipType[0] = '\0';
    }
};

struct CompanyQueueNode {
    char company[30];
    TypeQueueNode* typeHead;
    CompanyQueueNode* next;

    CompanyQueueNode() : typeHead(nullptr), next(nullptr) {
        company[0] = '\0';
    }

    ~CompanyQueueNode() {
        while (typeHead) {
            TypeQueueNode* temp = typeHead;
            typeHead = typeHead->next;
            temp->queue.clear();
            delete temp;
        }
    }
};

struct DockedShipArray {
    DockingShip* ships;
    int count;
    int capacity;

    DockedShipArray() : ships(nullptr), count(0), capacity(0) {}

    ~DockedShipArray() {
        if (ships) delete[] ships;
    }

    void add(const DockingShip& ship) {
        if (count >= capacity) {
            int newCapacity = capacity == 0 ? 4 : capacity * 2;
            DockingShip* newShips = new DockingShip[newCapacity];
            for (int i = 0; i < count && i < capacity; i++) {
                newShips[i] = ships[i];
            }
            if (ships) delete[] ships;
            ships = newShips;
            capacity = newCapacity;
        }
        if (count < capacity) {
            ships[count++] = ship;
        }
    }

    void removeAt(int index) {
        if (index < 0 || index >= count) return;
        for (int i = index; i < count - 1; i++) {
            ships[i] = ships[i + 1];
        }
        count--;
    }

    void clear() {
        count = 0;
    }
};

struct PortDockingData {
    char portName[30];
    int maxActiveDocks;
    DockedShipArray currentlyDocked;
    CompanyQueueNode* companyHead;
    PortDockingData* next;

    PortDockingData() : maxActiveDocks(2), companyHead(nullptr), next(nullptr) {
        portName[0] = '\0';
    }

    void setPortName(const char* name, int maxDocks) {
        strncpy(portName, name, 29);
        portName[29] = '\0';
        maxActiveDocks = maxDocks;
    }

    int getTotalWaiting() const {
        int total = 0;
        CompanyQueueNode* cNode = companyHead;
        while (cNode) {
            TypeQueueNode* tNode = cNode->typeHead;
            while (tNode) {
                total += tNode->queue.size;
                tNode = tNode->next;
            }
            cNode = cNode->next;
        }
        return total;
    }

    DockingShipQueue* getQueue(const char* company, const char* shipType) {

        CompanyQueueNode* cNode = companyHead;
        CompanyQueueNode* cPrev = nullptr;

        while (cNode && strcmp(cNode->company, company) != 0) {
            cPrev = cNode;
            cNode = cNode->next;
        }

        if (!cNode) {
            cNode = new CompanyQueueNode();
            strncpy(cNode->company, company, 29);
            cNode->company[29] = '\0';
            if (cPrev) cPrev->next = cNode;
            else companyHead = cNode;
        }

        TypeQueueNode* tNode = cNode->typeHead;
        TypeQueueNode* tPrev = nullptr;

        while (tNode && strcmp(tNode->shipType, shipType) != 0) {
            tPrev = tNode;
            tNode = tNode->next;
        }

        if (!tNode) {
            tNode = new TypeQueueNode();
            strncpy(tNode->shipType, shipType, 19);
            tNode->shipType[19] = '\0';
            if (tPrev) tPrev->next = tNode;
            else cNode->typeHead = tNode;
        }

        return &tNode->queue;
    }

    void clear() {
        currentlyDocked.clear();
        while (companyHead) {
            CompanyQueueNode* temp = companyHead;
            companyHead = companyHead->next;
            delete temp;
        }
        companyHead = nullptr;
    }

    ~PortDockingData() {
        clear();
    }
};

class DockingManager {
private:
    PortDockingData* portHead;
    int currentTimeMinutes;
    bool isPlaying;
    int timeStep;

    void generateDemoSchedule();

    PortDockingData* findPort(const char* portName);

public:
    DockingManager();
    ~DockingManager();

    void initializePorts(const char portNames[][30], int numPorts);

    void enqueueShip(const DockingShip& ship);

    void updateSimulation();

    void play() { isPlaying = true; }
    void pause() { isPlaying = false; }
    void stepForward(int minutes = 30);
    void stepBackward(int minutes = 30);
    void setTime(int minutes);
    void reset();
    void loadRoutesForDate(const char* filename, int day, int month, int year);

    int getCurrentTime() const { return currentTimeMinutes; }
    bool getIsPlaying() const { return isPlaying; }
    int getTimeStep() const { return timeStep; }
    PortDockingData* getPortData(const char* portName);
    PortDockingData* getAllPortData() { return portHead; }

    int getShipsInTransit(DockingShip* buffer, int maxShips) const;

    void getFormattedTime(char* buffer) const;
};
