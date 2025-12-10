🌊 OceanRoute Navigator
Maritime Route Planning & Visualization System (SFML C++ Project)

OceanRoute Navigator is an interactive maritime simulation system built with C++ and SFML, featuring real-time global route visualization, port-to-port optimization algorithms, dynamic UI panels, ship animations, and multi-leg journey editing.

🚢 Key Features

✔ Global Map Visualization using SFML
✔ Dijkstra (Cost/Time) and A* (Cost/Time) optimization
✔ Safest Route Finder (departure-date-based validation)
✔ Multi-Leg Journey Editor using Linked List
✔ Docking & Layover Management using Queues
✔ Company Routes Viewer with filtering
✔ Animated Ship Navigation with sprite-based movement
✔ Dynamic Panels for analytics, route stats, and visual feedback
✔ Fully Redesigned Premium UI

🧩 Data Structures Used
Feature	Data Structure	Purpose
Route Graph	Adjacency List	Fast lookups between ports
Dijkstra / A*	Priority Queue	Optimal pathfinding
Docking Queue	Queue	FIFO ship handling
Multi-Leg Builder	Doubly Linked List	Editable user journeys
Company Ships	Maps / vectors	Separate DMA for each company

📦 Project Structure
OceanRoute-Navigator/
│── Assets/
├── AStarSearch.cpp / .h
├── SafestRouteSearch.cpp / .h
├── ShortestPath.cpp / .h
├── RouteSearch.cpp / .h
├── RoutePreferences.cpp / .h
├── Graph.cpp / .h
├── Journey.cpp / .h
├── JourneyManager.cpp / .h
├── MultiLegBuilder.cpp / .h
├── DockingManager.cpp / .h
├── ShipAnimator.cpp / .h
├── PriorityQueue.cpp / .h
├── SfmlApp.cpp / .h
├── DateTime.cpp / .h
├── main_sfml.cpp

▶️ How to Build & Run
Requirements:

C++17 or later

SFML 2.6.x

A compiler: MinGW / MSVC / Clang

Build:
g++ -std=c++17 main_sfml.cpp *.cpp -lsfml-graphics -lsfml-window -lsfml-system -o OceanRoute

Run:
./OceanRoute


🏗 Future Improvements

Weather-based routing

Port congestion heatmaps

Real rental/chartering cost simulation

🙌 Author

Muhammad Huzaima Malik
BS Software Engineering
FAST NUCES Islamabad
