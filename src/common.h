#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <functional>

struct V3 { float x, y, z; };
struct V2 { float u, v; };
struct M4 { float m[4][4]; };

void Log(const char* fmt, ...);
std::wstring GameDir();              // folder of SOVIET64.exe, with trailing backslash
std::string WideToUtf8(const std::wstring& w);

// ---- interface language ----
namespace i18n { extern bool en; const char* T(const char* ru); }
using i18n::T;
// game icon (media_soviet/editor/tool_<id>.png) as an ImGui texture id; nullptr when there is none
void* LoadIconTexture(const std::wstring& path);

// ---- engine access (IAT hooks on C3DDLL64 imports) ----
namespace eng {
    void InstallIatHooks();
    bool CameraSeen();                              // the game has started using its camera
    bool Ready();                                   // camera and terrain captured
    bool WorldToScreen(const V3& w, float sw, float sh, float& sx, float& sy);
    bool ScreenToTerrain(float sx, float sy, float sw, float sh, V3& out);
    float TerrainHeight(float x, float z);
    // air pollution grid of the game (200 m cells, see docs/internals.md): the two floats of the cell at (x, z);
    // false when the grid is not there
    bool PollutionAt(float x, float z, float& a, float& b);
    void LogPollutionGrid();                        // debug: size and maxima of the whole grid
    void QueueLevel(const std::vector<V3>& pts);    // flatten: each point is x, target height, z
    void LevelTick();                               // one levelling pass; call on the render thread
    int  LevelPending();                            // points still being levelled, 0 = idle
    int  Occupied(float x, float z);                // 1 = an existing building here, 2 = an existing engine path
    bool OccupancyReady();
    bool CursorActive();                            // the player has a building selected in the game's build menu
    float CursorRot();                              // its rotation, radians
    std::string SelectedType(DWORD* tick);          // ini name of the building picked last in the game menu
    std::string CursorTypeName();                   // engine identifier of that building type
    void StartAutoBuild(const std::vector<V3>& pos, const std::vector<float>& rot, float sw, float sh);
    // line script with the real mouse: action 0 = move only, 3 = left button held for the step (drag)
    void StartScript(const std::vector<V3>& pos, const std::vector<int>& action, const std::vector<int>& frames, float sw, float sh);
    int  ToolOrder(const std::string& id);          // position of the building in the game's tool table (build menu order), -1 = unknown
    bool InstantSupported();                        // direct build calls exist for this game build
    // one direct call into the game: kind 0 = building "type" at a turned by rot, 1 = road a->b, 2 = footpath a->b
    // kind 3 = a utility line a->b: mgr = offset of the network manager in the game state, lineType / sub / arg as the game's own tool passes them
    struct InstantJob { int kind = 0; std::string type; V3 a{}, b{}; float rot = 0; int lineType = 0; int mgr = 0, sub = 0, arg = -20; float ha = 0, hb = 0; };
    void QueueInstant(const std::vector<InstantJob>& jobs);
    bool InstantBusy();
    void InstantResults(std::vector<int>& codes);   // building: -1 placed, 10 ground too uneven; line: 1 placed
    void DebugDumpTools();
    void DebugDumpLoad();
    void DebugDumpRoads();
    void LogDarkness();                             // debug: log exe+0x9d5d84 every 20 s while DistrictPlanner.darkness exists                           // debug: log node capacities and building loads from the running game
    void SetPathSnapMode(int mode);
    void SetStraightLines(bool on);                 // debug: 0 = snap footpath ends like the area paste, 1 = like the building tool
    void DebugSetTool(const char* name);            // debug: make this game tool the current one
    void InstallPathLogging();
    // buildings already standing in the game (engine list exe+0x9e6a18): ini name, position, yaw (radians, same
    // sense as the build cursor rotation)
    struct WorldBld { std::string id; V3 pos; float yaw; };
    void ListBuildings(std::vector<WorldBld>& out);
    // lines already built in the game: mgrOffset 0xf7a0 roads, 0xf968 footpaths; each piece as its curve points
    void ListNetLines(int mgrOffset, std::vector<std::vector<V3>>& out);
    void StopAutoBuild();
    bool ScriptActive();                            // a line script is driving the real mouse
    bool AutoBuildActive();
    void AutoBuildStats(int& done, int& failed, int& left);
    void BeginFrame();                              // refresh cached matrices
    extern char g_scriptButton[64];
    extern bool g_blockMouseButtons;                // game sees no mouse buttons
    extern bool g_blockMouseMove;                   // game sees no wheel / relative motion
    extern bool g_hideMouse;                        // game sees the cursor tooFar off-screen
    extern bool g_blockKeys;                        // game sees no keyboard
}

// ---- building catalog ----
// Where a footpath (kind 0) or a road (kind 1) joins the building: model coordinates, direction pointing outwards.
// Utilities: 2 water in, 3 water out, 4 sewage in, 5 sewage out, 6 heating (small pipe), 8 power in, 9 power out (medium voltage).
struct Conn { int kind; float x, z, dx, dz; float y = 0; };   // y: height of the connection point above the model origin
struct BuildingType {
    std::string id;         // ini file name without extension
    std::string name;       // localized, UTF-8 (current interface language)
    std::string nameRu, nameEn;
    std::string category;   // LIVING, SHOP, ...
    std::string material;   // brick / panels / wood ... (from construction cost)
    std::string style;      // STYLE_FLAG
    int   capacity = 0;     // residents (LIVING)
    float quality = 0;      // QUALITY_OF_LIVING
    int   workers = 0;
    float sx = 0, sz = 0;   // footprint size, meters
    float height = 0;
    float cx = 0, cz = 0;   // footprint centre in model coordinates (the model origin is rarely the centre)
    std::vector<Conn> conns;
    bool  needsRoad = false; // cannot work without a road for vehicles
    // Utility loads the way the game's type loader (SOVIET64.exe 0x10e1f0) computes them, see docs/mechanics.md 4.4.
    int   profs = 0;        // $PROFESORS_NEEDED
    int   serve = 0;        // $CITIZEN_ABLE_SERVE
    float fLive = 1.0f, fLiveServe = 0.2f, fLight = 1.0f, fLightServe = 0.2f;   // $ELETRIC_CONSUMPTION_*_FACTOR*
    bool  heatBig = false;  // has $CONNECTION_HEATING_BIG
    float engineSpeed = 0;  // $ENGINE_SPEED (pumps): adds engineSpeed x 0.01 MW
    float fWater = 1.0f;    // $WATER_CONSUMPTION_LIVING_WORKER_FACTOR
    int   fuelStorages = 0; // ini storages of liquids / water / sewage: each adds 0.05 MW
    bool  noHeat = false, noPower = false, noWater = false, noSewage = false;   // $HEATING_DISABLE, $ELECTRIC_DISABLE, $WATERSEWAGE_DISABLE, $SEWAGE_DISABLE
    float waterLoad = 0;    // water tank the building fills from the network (50 residents = 2.25)
    float sewageLoad = 0;   // the same for sewage
    float heatLoad = 0;     // hot-water tank units the building asks for (game value; 50 residents = 35)
    float powerLoad = 0;    // MW the building asks for
    float powerNight = 0;   // MW drawn at full darkness with every worker present (adds the lighting half, 0x1bca60)
    float heatCap = 0;      // heat exchanger / heat pumping station: what it can serve (1000, big 3000)
    float powerCap = 0;     // substation 2.5, transformer 19, rail trafo 14 MW
};
namespace catalog {
    void Load();
    const std::vector<BuildingType>& All();
    void SetEnglish(bool en);                       // switch BuildingType::name
}

// ---- planner ----
// Works in a local frame (u, v) in meters; the UI maps it to the world and supplies terrain heights.
struct ServiceRule {
    const char* category;
    const char* label;
    bool  enabled;
    int   perResidents;     // one building per N residents
    int   typeIndex;        // index into catalog::All(), -1 = none
    const char* prefer = nullptr;   // building id picked by default (else the smallest of the category)
};
struct PlanInput {
    std::vector<V2> poly;                           // district outline
    int   population = 1000;
    float gap = 8.0f;
    float band = 7.0f;                              // strip of a footpath between two rows of buildings, m
    bool  fillAll = false;
    int   servicePop = 0;                           // residents the services are counted for (0 = population)
    int   layout = 1;                               // 0 parallel strips, 1 microdistricts (blocks), 2 village / suburb: houses on both sides of every street
    bool  blocks = true;                            // microdistricts: street grid, houses round each block, courtyards (false = old parallel strips)                          // build up the whole area: houses until nothing fits
    std::vector<int> houseTypes;                    // catalog indices
    std::vector<float> houseWeights;                // share of each type among the houses (any scale); empty = equal
    std::vector<ServiceRule>* services = nullptr;
    int   roadType = 1;                             // 0 none, 1 street, 2 avenue
    float roadV = 0;                                // where the street crosses the district
    std::vector<float> stopsU;                      // bus stops along the street
    struct ExtPort { int kind; float u, v, h; };        // a connection of a building that already stands (local frame)
    std::vector<ExtPort> extPorts;
    std::vector<std::vector<V2>> extRoads;          // roads already in the game (local frame): the plan joins them and does not double them                  // the planner feeds each network from the nearest fitting one
    int   stopType = -1;
    bool  crossRoad = false;                        // a second street across the first one (crossroads in the middle)                            // catalog index of the stop building (bus, trolleybus, tram); -1 = only mark the place
    float streetSpacing = 250;                      // a parallel street every N metres, 0 = one street only
    bool  utilBuild[4] = {};
    std::vector<V2> extNodes[4];                    // node buildings that already stand (local frame): they serve houses in reach                        // put this network's buildings and lines into the plan
    bool  utilChain = false;                        // also put switches and pipes between the nodes (the game bends such pipes into loops)
    bool  powerTrafo = true;                        // put a transformer for the substations too
    float utilRadius[4] = { 250, 250, 380, 250 };   // water, sewage, heating, power: how far a node serves, m (from the exe, see docs)
    // node load (the game checks only heating and power): a building's demand is split evenly among all nodes of the
    // network within reach; a node is overloaded above 90 % of its capacity. Values read from a running game.
    float utilPerPerson[4] = { 0, 0, 0.7f, 0.002f };  // heat-tank units / MW per resident (50-resident brick house: 35 and 0.1)
    float utilNodeCap[4] = { 0, 0, 1000, 2.5f };      // small heat exchanger 1000, substation 2.5 MW; 0 = the game sets no limit
    float walkRadius = 400;                         // how tooFar residents walk to a stop
    float maxDiff = 3.0f;                           // allowed height difference under one building
    std::function<bool(float, float)> occupied;     // something already stands at (u, v); may be empty
    std::function<float(float, float)> height;      // terrain height at (u, v); may be empty
};
struct Placed {
    int type; float u, v, w, d; bool rotated;
    int rot = 0;                                    // quarter turns: model X along +u, +v, -u, -v
    float ou = 0, ov = 0;                           // where the model origin goes (the game's cursor point)
    float pad = 0, cut = 0, fill = 0;               // levelled height, earth to remove / add (m3)
    bool tooFar = false;                               // too tooFar from every stop
    signed char net = -1, role = 0;                 // utility building: network 0..3; role 0 = node serving houses, 1 = switch / transformer
    short grp = 0;                                  // node and its switch share a number
    bool stop = false;                              // a stop the street runs through
};
struct UtilSeg { int net; float u1, v1, u2, v2; float h1 = 0, h2 = 0; };   // h: height of the end above the ground (connection point)
struct PathSeg { float u1, v1, u2, v2; };
struct RoadSeg { float u1, v1, u2, v2, width; };
struct PlanResult {
    std::vector<Placed>  buildings;
    std::vector<PathSeg> paths;
    std::vector<char>    pathBuild;                 // per path: 0 = only the walking line along a street (the street has pavements)
    std::vector<RoadSeg> roads;
    std::vector<V2>      stops, crossings;
    int joinedRoads = 0, droppedRoads = 0;
    float steepCut = 0;                             // metres of street left out because the ground is too steep          // plan street ends tied to existing roads / plan pieces already built
    std::vector<PathSeg> drives;                    // driveways from the street to road connections
    std::vector<V2>      pedPts, roadPts;           // connection points that got a footpath / a driveway
    int noPath = 0, noRoad = 0;
    int cutOff = 0, cutOffPaths = 0;                // buildings / footpaths dropped: not joined to the street net                     // buildings left without a footpath / without a needed road
    int capacity = 0, houses = 0, services = 0, unplaced = 0, farHouses = 0, steepSkipped = 0, jobs = 0, busySkipped = 0;
    std::vector<V2> utilNodes[4];                   // substations next to the streets
    int   utilUncovered[4] = {};                    // buildings out of reach of every node
    float utilDemand[4] = {};                       // total demand of the district (heat-tank units, MW)
    float utilMaxLoad[4] = {};                      // most loaded node, share of its capacity (0.9 and more = the game warns)
    int   utilOverloaded[4] = {};                   // nodes above 90 %
    std::vector<UtilSeg> utilLines;                 // pipes / wires between the utility buildings
    float utilLen[4] = {};                          // their length per network, m
    int   utilNoLine[4] = {};
    float utilFeed[4] = {};                         // length of the line from an existing source, 0 = none found                       // connections that found no way or no free port
    float normWalk[3] = { -1, -1, -1 }; int normFar[3] = { 0, 0, 0 };   // SNiP II-60-75 norms: shop 400, kindergarten 350, school 500 m
    float pathLength = 0, roadLength = 0, maxWalkToShop = -1, maxWalkToStop = -1, cut = 0, fill = 0;
};
PlanResult MakePlan(const PlanInput& in);
float NormRadius(const std::string& category);
bool PolyScan(const std::vector<V2>& poly, float v, float& left, float& right);   // widest span inside at height v
bool PolyContains(const std::vector<V2>& poly, float u, float v);

// ---- ui ----
namespace ui {
    void ApplyStyle();      // paper look matching the game windows
    void Draw();            // called once per frame between ImGui::NewFrame and Render
    bool WantsGameMouse();  // true while the mod uses clicks on the map (drawing the outline, dragging a stop)
}
