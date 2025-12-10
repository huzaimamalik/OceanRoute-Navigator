#ifndef SFML_APP_H
#define SFML_APP_H

#include "Graph.h"
#include "JourneyManager.h"
#include "RoutePreferences.h"
#include "DockingManager.h"
#include "RouteSearch.h"
#include "AStarSearch.h"
#include "MultiLegBuilder.h"
#include "ShipAnimator.h"
#include <string>
#include <SFML/Graphics.hpp>

using namespace std;

const int MAX_PORTS = 50;

struct PortCoord {
    string name;
    float lat;
    float lon;
};

struct MapCalibration {
    float xOffsetNorm = 0.0f;
    float yOffsetNorm = 0.0f;
    float xScale = 1.0f;
    float yScale = 1.0f;
};

const PortCoord PORT_POSITIONS[] = {

    {"London", 51.50f, -0.12f},
    {"Dublin", 53.35f, -6.26f},
    {"Hamburg", 53.55f, 9.99f},
    {"Rotterdam", 51.92f, 4.48f},
    {"Antwerp", 51.22f, 4.40f},
    {"Marseille", 43.30f, 5.37f},
    {"Genoa", 44.41f, 8.93f},
    {"Lisbon", 38.72f, -9.14f},
    {"Copenhagen", 55.68f, 12.57f},
    {"Oslo", 59.91f, 10.75f},
    {"Stockholm", 59.33f, 18.07f},
    {"Helsinki", 60.17f, 24.94f},
    {"Athens", 37.98f, 23.73f},
    {"Istanbul", 41.01f, 28.98f},

    {"Dubai", 25.27f, 55.30f},
    {"AbuDhabi", 24.47f, 54.37f},
    {"Jeddah", 21.54f, 39.17f},
    {"Doha", 25.29f, 51.53f},

    {"Alexandria", 31.20f, 29.92f},
    {"CapeTown", -33.92f, 18.42f},
    {"Durban", -29.86f, 31.03f},
    {"PortLouis", -20.16f, 57.50f},

    {"Karachi", 24.86f, 67.01f},
    {"Mumbai", 19.08f, 72.88f},
    {"Colombo", 6.93f, 79.85f},
    {"Chittagong", 22.36f, 91.78f},

    {"Singapore", 1.29f, 103.85f},
    {"Jakarta", -6.21f, 106.85f},
    {"Manila", 14.60f, 120.98f},
    {"HongKong", 22.32f, 114.17f},
    {"Shanghai", 31.23f, 121.47f},
    {"Tokyo", 35.68f, 139.69f},
    {"Osaka", 34.69f, 135.50f},
    {"Busan", 35.18f, 129.08f},

    {"Sydney", -33.87f, 151.21f},
    {"Melbourne", -37.81f, 144.96f},

    {"NewYork", 40.71f, -74.01f},
    {"Montreal", 45.50f, -73.57f},
    {"Vancouver", 49.28f, -123.12f},
    {"LosAngeles", 34.05f, -118.24f}
};

const int PORT_COUNT = 40;

const bool DEBUG_CALIBRATION = false;

const bool PLACEMENT_MODE_ENABLED = false;

const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1000;

const int LEFT_SIDEBAR_WIDTH = 340;
const int RIGHT_SIDEBAR_WIDTH = 380;
const int MAP_X = LEFT_SIDEBAR_WIDTH;
const int MAP_WIDTH = WINDOW_WIDTH - LEFT_SIDEBAR_WIDTH - RIGHT_SIDEBAR_WIDTH;

namespace Colors {

    const unsigned int DARK_BG = 0x0d0d1aFF;
    const unsigned int PANEL_BG = 0x151528FF;
    const unsigned int PANEL_HEADER = 0x1a1a3aFF;
    const unsigned int ACCENT = 0x2d1b69FF;
    const unsigned int PANEL_GRADIENT_TOP = 0x1e1e40FF;
    const unsigned int PANEL_GRADIENT_BOT = 0x12122aFF;

    const unsigned int HIGHLIGHT = 0x00ffccFF;
    const unsigned int HIGHLIGHT_DIM = 0x00cc99FF;
    const unsigned int SECONDARY = 0xff6b9dFF;
    const unsigned int TERTIARY = 0xffaa00FF;
    const unsigned int ELECTRIC_BLUE = 0x00d9ffFF;
    const unsigned int NEON_PURPLE = 0xb44affFF;
    const unsigned int LIME_GREEN = 0x7fff00FF;

    const unsigned int TEXT_PRIMARY = 0xffffffFF;
    const unsigned int TEXT_SECONDARY = 0xa0b0c8FF;
    const unsigned int TEXT_MUTED = 0x6a7a90FF;
    const unsigned int TEXT_ACCENT = 0x00ffccFF;

    const unsigned int BUTTON_DEFAULT = 0x2a2a55FF;
    const unsigned int BUTTON_HOVER = 0x4a4a80FF;
    const unsigned int BUTTON_ACTIVE = 0x00ffccFF;
    const unsigned int INPUT_BG = 0x1c1c3aFF;
    const unsigned int INPUT_BORDER = 0x4a4a7aFF;
    const unsigned int INPUT_FOCUS = 0x00ffccFF;

    const unsigned int SUCCESS = 0x00ff88FF;
    const unsigned int WARNING = 0xffcc00FF;
    const unsigned int DANGER = 0xff4466FF;
    const unsigned int INFO = 0x44bbffFF;

    const unsigned int PORT_COLOR = 0xffaa00FF;
    const unsigned int PORT_HOVER = 0xffff00FF;
    const unsigned int ROUTE_COLOR = 0xffffff44;
    const unsigned int JOURNEY_COLOR = 0xff3366FF;
    const unsigned int JOURNEY_GLOW = 0xff00ff88;
    const unsigned int OCEAN_COLOR = 0x0a1830FF;
    const unsigned int OCEAN_LIGHT = 0x102040FF;

    const unsigned int LEG_COLORS[] = {
        0xff4444FF,
        0xffff00FF,
        0x00ff00FF,
        0xff8800FF,
        0xff00ffFF,
        0x00ffffFF,
    };
    const int LEG_COLOR_COUNT = 6;

    const unsigned int DROPDOWN_BG = 0x1e1e42FF;
    const unsigned int DROPDOWN_ITEM_HOVER = 0x3535668;
    const unsigned int BORDER = 0x4a4a7aFF;
    const unsigned int BUTTON_BG = 0x2a2a55FF;
    const unsigned int TEXT = 0xffffffFF;
    const unsigned int ERROR = 0xff4466FF;
    const unsigned int DROPDOWN_BORDER = 0x5555aaFF;
    const unsigned int DROPDOWN_SHADOW = 0x000000aa;

    const unsigned int ROUTE_PANEL_BG = 0x1a1a35FF;
    const unsigned int ROUTE_NODE_BG = 0x2a2a55FF;
    const unsigned int ROUTE_ARROW = 0x00ffccFF;
}

enum UIStrategy {
    UI_DIJKSTRA_COST = 0,
    UI_DIJKSTRA_TIME = 1,
    UI_ASTAR_COST = 2,
    UI_ASTAR_TIME = 3,
    UI_SAFEST = 4
};

enum ViewMode {
    VIEW_MAIN_SEARCH = 0,
    VIEW_COMPANY_ROUTES = 1
};

enum class AppState {
    MAIN_MENU = 0,
    ROUTE_PLANNER = 1,
    COMPANY_VIEWER = 2,
    MULTILEG_EDITOR = 3,
    DOCKING_MANAGER = 4
};

struct UIState {

    string originPort;
    string destPort;
    int originIndex;
    int destIndex;

    int day;
    int month;
    int year;

    int maxCost;
    int maxLegs;
    UIStrategy strategy;

    enum ActiveField { NONE, DAY, MONTH, YEAR, MAX_COST, MAX_LEGS };
    ActiveField activeField;
    string inputBuffer;

    bool hasResults;
    int selectedJourneyId;

    string journeyPorts[10];
    int journeyPortCount;

    struct LegSchedule {
        string fromPort = "";
        string toPort = "";
        int depDay = 0, depMonth = 0, depYear = 0;
        int depHour = 0, depMinute = 0;
        int arrHour = 0, arrMinute = 0;
        string company = "";
        int cost = 0;
    };
    struct JourneyInfo {
        int id = 0;
        int cost = 0;
        int time = 0;
        int legs = 0;
        int risk = 0;
        string route = "";
        bool valid = false;
        LegSchedule schedule[5];
        int totalMinutes = 0;
    };
    JourneyInfo journeyList[20];
    int journeyListCount;
    int journeyScrollOffset = 0;
    int selectedJourneyIndex = -1;

    LegSchedule journeySchedule[10];
    int journeyScheduleCount;

    string selectedDockingPort;
    bool dockingSimPlaying;
    int hoveredDockingPort;
    bool dockingBackButtonHovered;

    struct StrategyResult {
        bool valid = false;
        int cost = 0;
        int totalCost = 0;
        int legs = 0;
        int totalTime = 0;
        int risk = 0;
        string route = "";
        int nodesExpanded = 0;
    };
    StrategyResult cheapestResult;
    StrategyResult fastestResult;
    StrategyResult astarResult;
    StrategyResult safestResult;

    string statusMessage;
    bool isError;

    bool originDropdownOpen;
    bool destDropdownOpen;
    int dropdownScroll;
    int hoveredDropdownItem;

    int hoveredPort;
    bool searchButtonHovered;

    bool showAllRoutes;

    AppState appState;

    ViewMode currentView;

    int editorJourneyId;
    bool editorActive;
    int editorSelectedNodeIndex;
    bool editorShowResults;

    string editorInputBuffer;
    bool editorInputActive;

    int editorStartDay;
    int editorStartMonth;
    int editorStartYear;
    bool editorUseDateSearch;

    struct EditorSegmentResult {
        bool valid = false;
        string fromPort = "";
        string toPort = "";
        int cost = 0;
        int legs = 0;
        string errorMessage = "";
    };
    EditorSegmentResult editorSegmentResults[10];
    int editorSegmentCount;

    string companyList[50];
    int companyCount;
    int selectedCompanyIndex;
    bool showAllCompanies;
    int companyScrollOffset;

    float pulseTimer;

    enum AnimationState {
        ANIM_IDLE,
        ANIM_EXPLORING,
        ANIM_DRAWING_LINE,
        ANIM_SHIP_MOVING
    };
    AnimationState animState;
    bool shipAnimationActive;
    int shipCurrentLeg;
    float shipProgress;
    float lineDrawProgress;
    float shipX, shipY;
    float shipAngle;
    float shipSpeed;

    float explorationAnimTime;
    float explorationAnimDuration;
    int explorationEdgesDrawn;

    struct ExploredEdgeViz {
        string fromPort;
        string toPort;
    };
    ExploredEdgeViz exploredEdges[500];
    int totalExploredEdges;

    float mapViewCenterX;
    float mapViewCenterY;
    float mapViewZoom;

    float menuFadeAlpha;
    float transitionAlpha;
    int selectedMenuIndex;
    float buttonScales[5];
    float routeDrawProgress;
    bool routeAnimationPlayed;
    float exploredFadeAlpha;

    int hoveredSectionBox;
    float sectionGlowIntensity[5];
    int hoveredStrategyButton;
    float strategyButtonScales[4];
    float strategyHighlightX;
    bool showTooltip;
    int tooltipType;
    float tooltipFadeAlpha;

    struct AnimatedValue {
        float current;
        float target;
        float displayValue;
        bool isAnimating;
    };
    AnimatedValue animatedCost;
    AnimatedValue animatedLegs;
    AnimatedValue animatedStatesExplored;
    AnimatedValue animatedTime;
    float efficiencyTimeBar;
    float efficiencyCostBar;
    float riskMeterPosition;
    int hoveredRouteCard;
    float routeCardHoverLifts[20];
    bool resultsJustUpdated;
    float resultsPopInProgress;

    bool hoveredEdge;
    string hoveredEdgeSource;
    string hoveredEdgeDest;
    string hoveredEdgeCompany;
    string hoveredEdgeDate;
    string hoveredEdgeTime;
    float hoveredEdgeCost;
    float hoveredEdgeTooltipX;
    float hoveredEdgeTooltipY;

    enum RightPanelTab {
        TAB_ALGORITHMS = 0,
        TAB_ROUTE_STATS = 1,
        TAB_RISK = 2
    };
    RightPanelTab activeRightTab;
    float tabHoverGlow[3];

    bool preferencesEnabled;
    string preferredCompanies[5];
    int preferredCompaniesCount;
    string avoidedPorts[5];
    int avoidedPortsCount;
    int maxVoyageTimeHours;
    bool useMaxVoyageTime;
    bool preferencesDropdownOpen;
    bool avoidPortsDropdownOpen;
    int preferencesScrollOffset;
    int avoidPortsScrollOffset;
    bool preferencesButtonHovered;
    bool clearPreferencesHovered;

    UIState() {
        originPort = "Karachi";
        destPort = "Montreal";
        originIndex = 0;
        destIndex = 1;
        day = 24;
        month = 12;
        year = 2024;
        maxCost = 999999;
        maxLegs = 5;
        strategy = UI_DIJKSTRA_COST;
        activeField = NONE;
        inputBuffer = "";
        hasResults = false;
        selectedJourneyId = -1;
        journeyPortCount = 0;
        journeyListCount = 0;
        journeyScrollOffset = 0;
        selectedJourneyIndex = -1;
        statusMessage = "Select origin, destination, date and click SEARCH";
        isError = false;
        originDropdownOpen = false;
        destDropdownOpen = false;
        dropdownScroll = 0;
        hoveredDropdownItem = -1;
        hoveredPort = -1;
        searchButtonHovered = false;
        showAllRoutes = false;
        appState = AppState::MAIN_MENU;
        currentView = VIEW_MAIN_SEARCH;
        editorJourneyId = -1;
        editorActive = false;
        editorSelectedNodeIndex = -1;
        editorShowResults = false;
        editorSegmentCount = 0;
        editorInputBuffer = "";
        editorInputActive = false;
        companyCount = 0;
        selectedCompanyIndex = -1;
        showAllCompanies = true;
        companyScrollOffset = 0;
        pulseTimer = 0.0f;

        menuFadeAlpha = 0.0f;
        transitionAlpha = 0.0f;
        selectedMenuIndex = 0;
        for (int i = 0; i < 5; i++) buttonScales[i] = 1.0f;
        routeDrawProgress = 0.0f;
        routeAnimationPlayed = false;
        exploredFadeAlpha = 1.0f;

        hoveredSectionBox = -1;
        for (int i = 0; i < 5; i++) sectionGlowIntensity[i] = 0.0f;
        hoveredStrategyButton = -1;
        for (int i = 0; i < 4; i++) strategyButtonScales[i] = 1.0f;
        strategyHighlightX = 0.0f;
        showTooltip = false;
        tooltipType = 0;
        tooltipFadeAlpha = 0.0f;

        animatedCost = {0.0f, 0.0f, 0.0f, false};
        animatedLegs = {0.0f, 0.0f, 0.0f, false};
        animatedStatesExplored = {0.0f, 0.0f, 0.0f, false};
        animatedTime = {0.0f, 0.0f, 0.0f, false};
        efficiencyTimeBar = 0.0f;
        efficiencyCostBar = 0.0f;
        riskMeterPosition = 0.0f;
        hoveredRouteCard = -1;
        for (int i = 0; i < 20; i++) routeCardHoverLifts[i] = 0.0f;
        resultsJustUpdated = false;
        resultsPopInProgress = 0.0f;

        animState = ANIM_IDLE;
        shipAnimationActive = false;
        shipCurrentLeg = 0;
        shipProgress = 0.0f;
        lineDrawProgress = 0.0f;
        shipX = 0;
        shipY = 0;
        shipAngle = 0;
        shipSpeed = 0.15f;

        explorationAnimTime = 0.0f;
        explorationAnimDuration = 1.5f;
        explorationEdgesDrawn = 0;
        totalExploredEdges = 0;

        mapViewCenterX = MAP_X + MAP_WIDTH / 2.0f;
        mapViewCenterY = WINDOW_HEIGHT / 2.0f;
        mapViewZoom = 1.0f;

        journeyScheduleCount = 0;

        hoveredEdge = false;
        hoveredEdgeSource = "";
        hoveredEdgeDest = "";
        hoveredEdgeCompany = "";
        hoveredEdgeDate = "";
        hoveredEdgeTime = "";
        hoveredEdgeCost = 0.0f;
        hoveredEdgeTooltipX = 0.0f;
        hoveredEdgeTooltipY = 0.0f;

        activeRightTab = TAB_ALGORITHMS;
        for (int i = 0; i < 3; i++) {
            tabHoverGlow[i] = 0.0f;
        }

        preferencesEnabled = false;
        preferredCompaniesCount = 0;
        avoidedPortsCount = 0;
        maxVoyageTimeHours = 48;
        useMaxVoyageTime = false;
        preferencesDropdownOpen = false;
        avoidPortsDropdownOpen = false;
        preferencesScrollOffset = 0;
        avoidPortsScrollOffset = 0;
        preferencesButtonHovered = false;
        clearPreferencesHovered = false;

        shipAnimationActive = false;
        shipCurrentLeg = 0;
        shipProgress = 0.0f;
        shipX = 0.0f;
        shipY = 0.0f;
        shipAngle = 0.0f;
        shipSpeed = 0.3f;

        cheapestResult.valid = false;
        astarResult.valid = false;
        safestResult.valid = false;

        selectedDockingPort = "";
        dockingSimPlaying = false;
        hoveredDockingPort = -1;
        dockingBackButtonHovered = false;
    }
};

void runOceanRouteNavUI(Graph& graph, JourneyManager& journeyManager);

bool getPortCoords(const string& name, float& x, float& y);

string buildRouteSummary(const BookedJourney& journey);

string getJourneyCompanies(const BookedJourney& journey);

void performSearch(Graph& graph, JourneyManager& journeyManager, UIState& state);

void getJourneyPortSequence(const BookedJourney& journey, string ports[], int& count);

void getPortList(Graph& graph, string ports[], int& count);

#endif
