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

// ---- engine access (IAT hooks on C3DDLL64 imports) ----
namespace eng {
    void InstallIatHooks();
    bool CameraSeen();                              // the game has started using its camera
    bool Ready();                                   // camera and terrain captured
    bool WorldToScreen(const V3& w, float sw, float sh, float& sx, float& sy);
    bool ScreenToTerrain(float sx, float sy, float sw, float sh, V3& out);
    float TerrainHeight(float x, float z);
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
    bool InstantSupported();                        // direct build calls exist for this game build
    // one direct call into the game: kind 0 = building "type" at a turned by rot, 1 = road a->b, 2 = footpath a->b
    // kind 3 = a utility line a->b: mgr = offset of the network manager in the game state, lineType / sub / arg as the game's own tool passes them
    struct InstantJob { int kind = 0; std::string type; V3 a{}, b{}; float rot = 0; int lineType = 0; int mgr = 0, sub = 0, arg = -20; };
    void QueueInstant(const std::vector<InstantJob>& jobs);
    bool InstantBusy();
    void InstantResults(std::vector<int>& codes);   // building: -1 placed, 10 ground too uneven; line: 1 placed
    void DebugDumpTools();
    void DebugSetTool(const char* name);            // debug: make this game tool the current one
    void InstallPathLogging();
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
struct Conn { int kind; float x, z, dx, dz; };
struct BuildingType {
    std::string id;         // ini file name without extension
    std::string name;       // localized, UTF-8
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
};
namespace catalog {
    void Load();
    const std::vector<BuildingType>& All();
}

// ---- planner ----
// Works in a local frame (u, v) in meters; the UI maps it to the world and supplies terrain heights.
struct ServiceRule {
    const char* category;
    const char* label;
    bool  enabled;
    int   perResidents;     // one building per N residents
    int   typeIndex;        // index into catalog::All(), -1 = none
};
struct PlanInput {
    std::vector<V2> poly;                           // district outline
    int   population = 1000;
    float gap = 8.0f;
    std::vector<int> houseTypes;                    // catalog indices
    std::vector<float> houseWeights;                // share of each type among the houses (any scale); empty = equal
    std::vector<ServiceRule>* services = nullptr;
    int   roadType = 1;                             // 0 none, 1 street, 2 avenue
    float roadV = 0;                                // where the street crosses the district
    std::vector<float> stopsU;                      // bus stops along the street
    float streetSpacing = 250;                      // a parallel street every N metres, 0 = one street only
    bool  utilBuild[4] = {};                        // put this network's buildings and lines into the plan
    bool  utilChain = false;                        // also put switches and pipes between the nodes (the game bends such pipes into loops)
    bool  powerTrafo = true;                        // put a transformer for the substations too
    float utilRadius[4] = { 140, 170, 160, 140 };   // water, sewage, heating, power: how far a node serves, m
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
};
struct UtilSeg { int net; float u1, v1, u2, v2; };
struct PathSeg { float u1, v1, u2, v2; };
struct RoadSeg { float u1, v1, u2, v2, width; };
struct PlanResult {
    std::vector<Placed>  buildings;
    std::vector<PathSeg> paths;
    std::vector<RoadSeg> roads;
    std::vector<V2>      stops, crossings;
    std::vector<PathSeg> drives;                    // driveways from the street to road connections
    std::vector<V2>      pedPts, roadPts;           // connection points that got a footpath / a driveway
    int noPath = 0, noRoad = 0;                     // buildings left without a footpath / without a needed road
    int capacity = 0, houses = 0, services = 0, unplaced = 0, farHouses = 0, steepSkipped = 0, jobs = 0, busySkipped = 0;
    std::vector<V2> utilNodes[4];                   // substations next to the streets
    int   utilUncovered[4] = {};                    // buildings out of reach of every node
    std::vector<UtilSeg> utilLines;                 // pipes / wires between the utility buildings
    float utilLen[4] = {};                          // their length per network, m
    int   utilNoLine[4] = {};                       // connections that found no way or no free port
    float pathLength = 0, roadLength = 0, maxWalkToShop = -1, maxWalkToStop = -1, cut = 0, fill = 0;
};
PlanResult MakePlan(const PlanInput& in);
bool PolyScan(const std::vector<V2>& poly, float v, float& left, float& right);   // widest span inside at height v
bool PolyContains(const std::vector<V2>& poly, float u, float v);

// ---- ui ----
namespace ui {
    void ApplyStyle();      // paper look matching the game windows
    void Draw();            // called once per frame between ImGui::NewFrame and Render
    bool WantsGameMouse();  // true while the mod uses clicks on the map (drawing the outline, dragging a stop)
}
