

#include <iostream>
#include "Graph.h"
#include "PortCharges.h"
#include "JourneyManager.h"
#include "SfmlApp.h"

using namespace std;

int main() {
    cout << "========================================\n";
    cout << " OceanRoute Nav - Maritime Navigation  \n";
    cout << " Optimizer (SFML World Map UI)         \n";
    cout << "========================================\n\n";

    Graph graph;

    cout << "Loading routes from Routes.txt...\n";
    if (!loadRoutesFromFile(graph, "Routes.txt")) {
        cout << "ERROR: Could not open Routes.txt\n";
        cout << "Make sure Routes.txt is in the application directory.\n";
        return 1;
    }
    cout << "  Loaded " << graph.portCount << " ports.\n";

    PortChargeList portCharges;

    cout << "Loading port charges from PortCharges.txt...\n";
    if (!loadPortChargesFromFile(portCharges, "PortCharges.txt")) {
        cout << "Warning: Could not load PortCharges.txt (continuing without charges)\n";
    } else {
        applyPortChargesToGraph(portCharges, graph);
        cout << "  Port charges applied.\n";
    }
    cout << "\n";

    JourneyManager journeyManager;
    initJourneyManager(journeyManager);

    cout << "Backend initialized successfully.\n";
    cout << "Launching SFML World Map UI...\n\n";

    runOceanRouteNavUI(graph, journeyManager);

    clearJourneyManager(journeyManager);
    clearPortChargeList(portCharges);
    freeGraph(graph);

    cout << "\n========================================\n";
    cout << " OceanRoute Nav terminated successfully \n";
    cout << "========================================\n";

    return 0;
}
