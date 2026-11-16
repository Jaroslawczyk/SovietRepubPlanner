#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>

struct V3 { float x, y, z; };
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
    extern bool g_hideMouse;                        // game sees the cursor far off-screen
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
struct ServiceRule {
    const char* category;
    const char* label;
    bool  enabled;
    int   perResidents;     // one building per N residents
    int   typeIndex;        // index into catalog::All(), -1 = auto
};
struct PlanInput {
    float width = 0, depth = 0;     // meters
    int   population = 1000;
    float gap = 8.0f;
    std::vector<int> houseTypes;    // catalog indices
    std::vector<ServiceRule>* services = nullptr;
};
struct Placed { int type; float u, v, w, d; bool rotated; };
struct PathSeg { float u1, v1, u2, v2; };
struct PlanResult {
    std::vector<Placed>  buildings;
    std::vector<PathSeg> paths;
    int capacity = 0, houses = 0, services = 0, unplaced = 0;
    float pathLength = 0, maxWalkToShop = -1;
};
PlanResult MakePlan(const PlanInput& in);

// ---- ui ----
namespace ui {
    void ApplyStyle();      // paper look matching the game windows
    void Draw();            // called once per frame between ImGui::NewFrame and Render
    bool WantsGameMouse();  // true while picking territory on the map
}
