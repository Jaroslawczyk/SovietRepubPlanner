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
    void BeginFrame();                              // refresh cached matrices
    extern bool g_blockMouseButtons;                // game sees no mouse buttons
    extern bool g_blockMouseMove;                   // game sees no wheel / relative motion
    extern bool g_hideMouse;                        // game sees the cursor tooFar off-screen
    extern bool g_blockKeys;                        // game sees no keyboard
}

// ---- building catalog ----
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
    std::vector<ServiceRule>* services = nullptr;
    int   roadType = 1;                             // 0 none, 1 street, 2 avenue
    float roadV = 0;                                // where the street crosses the district
    std::vector<float> stopsU;                      // bus stops along the street
    float walkRadius = 400;                         // how tooFar residents walk to a stop
    float maxDiff = 3.0f;                           // allowed height difference under one building
    std::function<float(float, float)> height;      // terrain height at (u, v); may be empty
};
struct Placed {
    int type; float u, v, w, d; bool rotated;
    float pad = 0, cut = 0, fill = 0;               // levelled height, earth to remove / add (m3)
    bool tooFar = false;                               // too tooFar from every stop
};
struct PathSeg { float u1, v1, u2, v2; };
struct RoadSeg { float u1, v1, u2, v2, width; };
struct PlanResult {
    std::vector<Placed>  buildings;
    std::vector<PathSeg> paths;
    std::vector<RoadSeg> roads;
    std::vector<V2>      stops, crossings;
    int capacity = 0, houses = 0, services = 0, unplaced = 0, farHouses = 0, steepSkipped = 0;
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
