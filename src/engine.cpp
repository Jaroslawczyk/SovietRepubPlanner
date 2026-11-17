// Access to the game engine (C3DDLL64.dll) through IAT hooks in SOVIET64.exe.
#include "common.h"
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <map>
#include <atomic>

namespace eng {

bool g_hideMouse = false;
bool g_blockMouseButtons = false, g_blockMouseMove = false, g_blockKeys = false;

static void* g_camera = nullptr;
static void* g_terrain = nullptr;
static M4 g_viewProj{}, g_invViewProj{};
static bool g_matOk = false;

// ---- originals ----
typedef void  (*CamSetV3_t)(void*, V3*);
typedef float (*TerrHeight_t)(void*, V3*);
typedef bool  (*TerrRay_t)(void*, V3*, V3*, V3*, bool, bool, float);
typedef bool  (*InBool_t)(void*);
typedef bool  (*InKey_t)(void*, int);
typedef V3*   (*InV3_t)(void*, V3*);
typedef void* (*CamMat_t)(void*, void*);

static CamSetV3_t   o_ViewSetPosition;
static TerrHeight_t o_TerrHeight;
static TerrRay_t    o_TerrRay;
static InBool_t     o_LPress, o_LDown, o_LPressA, o_RPress, o_RDown, o_RPressA, o_MDown;
static InKey_t      o_KeyDown, o_KeyPress;
static InV3_t       o_MouseRel;
typedef float (*InAxis_t)(void*, char*, int*, float, float);
static InV3_t       o_MouseSolid, o_MouseSolidInv;
static InAxis_t     o_AxisValue;
static CamMat_t     f_ViewGetMatrix, f_ProjGetMatrix;

static void h_ViewSetPosition(void* self, V3* p) {
    if (g_camera != self) { Log("camera = %p", self); g_camera = self; }
    o_ViewSetPosition(self, p);
}
static float h_TerrHeight(void* self, V3* p) {
    if (g_terrain != self) { Log("terrain = %p (height)", self); g_terrain = self; }
    return o_TerrHeight(self, p);
}
static bool h_TerrRay(void* self, V3* a, V3* b, V3* out, bool f1, bool f2, float f) {
    if (g_terrain != self) { Log("terrain = %p (ray)", self); g_terrain = self; }
    return o_TerrRay(self, a, b, out, f1, f2, f);
}
// ---- auto-build through the game's own construction tool ----
// While the player has a building selected in the game's build menu, the game reports its cursor every frame
// (EditorSetCursorSingle: type, position, rotation). The mod then feeds the game a fake mouse position, a fake
// rotation-wheel value and a fake click, so the game itself places the building (and charges for it) exactly as
// if the player had clicked there.
struct AutoJob { V3 pos; float rot; };
static std::mutex g_abMx;
static std::vector<AutoJob> g_abJobs;
static std::atomic<bool> g_abActive{ false };
static std::atomic<int> g_abDone{ 0 }, g_abFailed{ 0 }, g_abLeft{ 0 };
static size_t g_abIdx = 0;
static int g_abPhase = 0, g_abTicks = 0, g_abStable = 0, g_abRotTries = 0;
static V3 g_abAim{};
static float g_abSpoofX = 0, g_abSpoofY = 0, g_abAxis = 0, g_abRadPerNotch = 0, g_abRotBefore = 0;
static bool g_abClick = false, g_abAxisSent = false;
static float g_abLastNotches = 0;
static void* g_bld = nullptr; static void* g_path = nullptr;
int Occupied(float x, float z);
bool WorldToScreen(const V3& w, float sw, float sh, float& sx, float& sy);
float TerrainHeight(float x, float z);
static std::atomic<DWORD> g_curTick{ 0 };
static int g_curType = -1; static V3 g_curPos{}; static float g_curRot = 0;
static float g_abW = 1920, g_abH = 1080;
static int g_abRetry = 0;
static std::atomic<DWORD> g_rotTick{ 0 }, g_typeTick{ 0 };
static std::atomic<float> g_lastRot{ 0 };
static char g_lastTypeName[96] = "";
float CursorRot();
bool CursorActive();
static std::atomic<int> g_dbgSet{ 0 }, g_dbgClear{ 0 }, g_dbgPoll{ 0 };

static std::atomic<int> g_dbgCalls[6];
static bool t_press = false, t_drag = false, t_dragPress = false; static float t_rel[3] = {}, t_solid[2] = {}; static int t_lines = 0, t_after = 0;
static std::atomic<int> g_abPressGen{ 0 };
// The game polls the mouse from two threads; a scripted "press" event must reach each of them exactly once
// (for a few milliseconds, so every caller within that thread's frame sees it).
static bool ScriptPress() {
    thread_local int seen = -1; thread_local ULONGLONG since = 0;
    int gen = g_abPressGen;
    LARGE_INTEGER f, t; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t);
    ULONGLONG ms = (ULONGLONG)(t.QuadPart * 1000 / f.QuadPart);
    if (seen != gen) { seen = gen; since = ms; }
    return ms - since < 6;
}
static void ScriptTick();
static int g_abPressAge = 0;      // frames the scripted button has been down; the game's line tools want a single press event
static bool g_abRClick = false; static int g_abMode = 0;   // 0 = buildings, 1 = scripted clicks (roads, paths)
static bool Spoof();
#define MB_HOOK(name, orig) static bool name(void* s) { if (Spoof()) return false; return g_blockMouseButtons ? false : orig(s); }
static bool h_LPress(void* s) { if (Spoof()) ++g_dbgCalls[0]; if (Spoof()) return g_abClick && (true); return g_blockMouseButtons ? false : o_LPress(s); }
static bool h_LDown(void* s) { static DWORD t = 0; if (t != GetCurrentThreadId()) { t = GetCurrentThreadId(); Log("input polled on thread %lu", t); } if (Spoof()) ++g_dbgCalls[1]; if (Spoof()) return g_abClick; return g_blockMouseButtons ? false : o_LDown(s); }   // polled every frame
static bool h_LPressA(void* s) { if (Spoof()) ++g_dbgCalls[2]; if (Spoof()) return g_abClick && (true); return g_blockMouseButtons ? false : o_LPressA(s); }
static bool h_RPress(void* s)  { if (Spoof()) return g_abRClick; return g_blockMouseButtons ? false : o_RPress(s); }
static bool h_RDown(void* s)   { if (Spoof()) return g_abRClick; return g_blockMouseButtons ? false : o_RDown(s); }
static bool h_RPressA(void* s) { if (Spoof()) return g_abRClick; return g_blockMouseButtons ? false : o_RPressA(s); }
MB_HOOK(h_MDown, o_MDown)
// Named buttons ("actions") of the game's input layer; logged to learn which ones the line tools poll.
typedef bool (*InName_t)(void*, char*);
static InName_t o_BtnDown, o_BtnPress;
char g_scriptButton[64] = "MOUSEDRAG";   // the named button the line tools read while the left button is held
static void GameThreadTick(); static void DumpLoadNow(); static std::atomic<bool> g_dumpLoad{ false };
static bool NamedButton(InName_t orig, const char* kind, void* s, char* name) {
    bool v = orig(s, name);
    if (name && kind[0] == 'p' && strcmp(name, "TOOL_CANCEL") == 0) GameThreadTick();
    if (v && name && strcmp(name, "MOUSEDRAG") == 0) { if (kind[0] == 'd') t_drag = true; else t_dragPress = true; }
    static std::vector<std::string> seen; static std::mutex mx; static int trues = 0;
    std::lock_guard<std::mutex> l(mx);
    std::string key = std::string(kind) + (name ? name : "?");
    bool known = false; for (auto& k : seen) if (k == key) known = true;
    if (!known && seen.size() < 200) { seen.push_back(key); Log("BUTTON %s '%s' polled (script %d)", kind, name ? name : "?", (int)g_abActive); }
    if (v && trues < 60 && !(name && strcmp(name, "MOUSEDRAG") == 0 && trues > 8)) { ++trues; Log("BUTTON %s '%s' = true", kind, name ? name : "?"); }
    return v;
}
static bool h_BtnDown(void* s, char* name) { return NamedButton(o_BtnDown, "down", s, name); }
static bool h_BtnPress(void* s, char* name) { return NamedButton(o_BtnPress, "press", s, name); }
static bool h_KeyDown(void* s, int k)  { return g_blockKeys ? false : o_KeyDown(s, k); }
static bool h_KeyPress(void* s, int k) { return g_blockKeys ? false : o_KeyPress(s, k); }
static V3* h_MouseRel(void* s, V3* out) {
    V3* r = o_MouseRel(s, out);
    if (r && !g_abActive) { t_rel[0] = r->x; t_rel[1] = r->y; t_rel[2] = r->z; }
    // Zeroing this while the cursor is over the panel made the camera drift; only the auto-builder needs it.
    if (Spoof() && r) { r->x = r->y = r->z = 0; }
    return r;
}

// While the cursor is over our panel the game is told the mouse is far off-screen, so its own
// buttons and tooltips underneath do not react.
static V3* h_MouseSolid(void* s, V3* out) {
    V3* r = o_MouseSolid(s, out);
    { static DWORD last = 0; static int n = 0; DWORD now = GetTickCount();
      if (r && n < 40 && now - last > 1500) { ++n; last = now; POINT pt; GetCursorPos(&pt); Log("MOUSESOLID %.2f %.2f %.2f | screen cursor %ld %ld", r->x, r->y, r->z, pt.x, pt.y); } }
    if (Spoof()) ++g_dbgCalls[3];
    if (r && !g_abActive) { t_solid[0] = r->x; t_solid[1] = r->y; }
    if (Spoof() && r) { r->x = g_abSpoofX; r->y = g_abSpoofY; }
    return r;
}
static V3* h_MouseSolidInv(void* s, V3* out) {
    V3* r = o_MouseSolidInv(s, out);
    if (Spoof() && r) { r->x = g_abSpoofX; r->y = g_abSpoofY; }
    return r;
}
static float h_AxisValue(void* s, char* name, int* a, float b, float c) {
    { static std::vector<std::string> seen; static std::mutex mx; std::lock_guard<std::mutex> l(mx);
      bool known = false; for (auto& k : seen) if (name && k == name) known = true;
      if (name && !known && seen.size() < 80) { seen.push_back(name); float v = o_AxisValue(s, name, a, b, c); Log("AXIS '%s' a=%d b=%.3f c=%.3f -> %.3f", name, a ? *a : -999, b, c, v); } }
    { float v = o_AxisValue(s, name, a, b, c); static int n = 0; if (v != 0 && n < 40 && name) { ++n; Log("AXISVAL '%s' = %.4f", name, v); } }
    if (g_abActive) {
        if (name && strcmp(name, "BUILDING_ROTATE") == 0) { float v = g_abAxis; if (v != 0) g_abAxisSent = true; return v; }
        return 0.0f;                         // no zoom / camera motion while the mod is placing buildings
    }
    return g_blockMouseMove ? 0.0f : o_AxisValue(s, name, a, b, c);   // camera zoom / rotation axes
}

// Diagnostics: record how the game calls its own terrain-edit and building-cursor functions.
typedef void (*TerrEdit_t)(void*, V3*, V3*, float, float, float, bool);
typedef void (*BldCursor_t)(void*, int, V3*, float, bool, float, bool);
typedef void (*BldVoid_t)(void*);
static TerrEdit_t o_EditToHeight; static BldCursor_t o_SetCursorSingle; static BldVoid_t o_AddCursor;
static void h_EditToHeight(void* s, V3* a, V3* b, float f1, float f2, float f3, bool flag) {
    static int n = 0;
    if (n < 40) { ++n; Log("EditToHeight this=%p a=(%.1f %.1f %.1f) b=(%.1f %.1f %.1f) f=%.3f %.3f %.3f flag=%d", s, a->x, a->y, a->z, b->x, b->y, b->z, f1, f2, f3, (int)flag); }
    o_EditToHeight(s, a, b, f1, f2, f3, flag);
}
static float WrapPi(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static void AutoNext(bool ok) {
    if (ok) ++g_abDone; else ++g_abFailed;
    ++g_abIdx; g_abPhase = 10; g_abTicks = 0; g_abStable = 0; g_abRotTries = 0; g_abRetry = 0; g_abAxis = 0; g_abClick = false;
    g_abLeft = (int)(g_abJobs.size() - g_abIdx);
    if (g_abIdx >= g_abJobs.size()) { g_abActive = false; Log("autobuild: finished, placed %d, failed %d", (int)g_abDone, (int)g_abFailed); }
    else g_abAim = g_abJobs[g_abIdx].pos;
}
// One step per rendered frame. The game does not report where its building cursor is, so the mod simply puts
// the fake mouse on the screen point of the target, clicks, and then checks whether a building appeared there.
// If nothing appeared, the left button is held for a while (the game then levels the site itself) and the
// click is repeated once.
static void AutoTick() {
    std::lock_guard<std::mutex> l(g_abMx);
    if (!g_abActive || g_abIdx >= g_abJobs.size()) return;
    const AutoJob& job = g_abJobs[g_abIdx];
    ++g_abTicks;
    float sx, sy;
    if (!WorldToScreen({ job.pos.x, TerrainHeight(job.pos.x, job.pos.z), job.pos.z }, g_abW, g_abH, sx, sy) ||
        sx < 60 || sy < 70 || sx > g_abW - 60 || sy > g_abH - 190) {
        Log("autobuild: target %d is off screen or under the game HUD, skipped", (int)g_abIdx); AutoNext(false); return;
    }
    g_abSpoofX = sx; g_abSpoofY = sy;
    switch (g_abPhase) {
    case 10: {                               // turn the building: the game reports its angle every frame, the mod
        if (!CursorActive()) {               // feeds the rotation axis (1000 units = 1 rad) until it matches the plan
            if (g_abTicks > 240) { Log("autobuild: no building is selected in the game's build menu, stopped"); g_abActive = false; g_abAxis = 0; }
            break;
        }
        if (g_abAxis != 0) { if (g_abAxisSent || g_abTicks > 30) { g_abAxis = 0; g_abTicks = 0; } break; }
        if (g_abTicks < 4) break;            // let the game apply the previous pulse
        float diff = WrapPi(job.rot - CursorRot());
        if (fabsf(diff) < 0.012f || ++g_abRotTries > 30) {
            if (g_abRotTries > 30) Log("autobuild: target %d - rotation still off by %.3f rad", (int)g_abIdx, diff);
            g_abPhase = 0; g_abTicks = 0; break;
        }
        g_abAxis = diff * 1000.0f; g_abAxisSent = false; g_abTicks = 0;
        break; }
    case 0:                                  // aim: let the game move its cursor to the spot
        g_abClick = false;
        if (g_abTicks >= 25) { g_abPhase = 1; g_abTicks = 0; g_abClick = true; }
        break;
    case 1:                                  // click
        if (g_abTicks >= 3) { g_abClick = false; g_abPhase = 2; g_abTicks = 0; }
        break;
    case 2:                                  // did a building appear?
        if (g_abTicks >= 25) {
            bool ok = (Occupied(job.pos.x, job.pos.z) & 1) != 0;
            Log("autobuild: target %d at (%.0f %.0f), screen (%.0f %.0f), rot %.3f (wanted %.3f), try %d -> %s", (int)g_abIdx, job.pos.x, job.pos.z, sx, sy, CursorRot(), job.rot, g_abRetry, ok ? "placed" : "not placed");
            if (ok) AutoNext(true);
            else if (g_abRetry++ == 0) { g_abPhase = 3; g_abTicks = 0; g_abClick = true; }
            else AutoNext(false);
        }
        break;
    case 3:                                  // hold the button: the game levels the ground under the cursor
        if (g_abTicks >= 240) { g_abClick = false; g_abPhase = 0; g_abTicks = 0; }
        break;
    }
}
static void h_SetCursorSingle(void* s, int type, V3* pos, float rot, bool b1, float f2, bool b2) {
    g_curType = type; g_curPos = *pos; g_curRot = rot; g_curTick = GetTickCount(); ++g_dbgSet;
    if (g_bld != s) g_bld = s;
    o_SetCursorSingle(s, type, pos, rot, b1, f2, b2);
}
static BldVoid_t o_ClearCursor;
static void h_ClearCursor(void* s) { ++g_dbgClear; o_ClearCursor(s); }
static void h_AddCursor(void* s) {
    static int n = 0;
    if (n < 40) { ++n; Log("AddCursorBuildings this=%p", s); }
    o_AddCursor(s);
}

// More terrain-edit diagnostics: the game's flatten tool may use one of these instead of EditToHeight.
#define LOGV(name, fmt, ...) { static int n = 0; if (n < 60) { ++n; Log(name " this=%p p=(%.1f %.1f %.1f) " fmt, s, p->x, p->y, p->z, __VA_ARGS__); } }
typedef void (*EdLevel_t)(void*, V3*, float, float, float, bool);
typedef void (*EdRL_t)(void*, V3*, float, float, float, float, bool);
typedef void (*EdRLA_t)(void*, V3*, float, float, float, float, bool, float);
typedef float (*EdRLC_t)(void*, V3*, float, float, float, float, bool);
typedef void (*EdSWL_t)(void*, V3*, float, float, float, float, float, float, float, float, float);
static EdLevel_t o_EdLevel, o_EdSmooth; static EdRL_t o_EdRL; static EdRLA_t o_EdRLA; static EdRLC_t o_EdRLC; static EdSWL_t o_EdSWL;
static BldVoid_t o_RegenColl, o_RegenNorm;
static void h_EdLevel(void* s, V3* p, float a, float b, float c, bool d) { LOGV("EditLevel", "f=%.3f %.3f %.3f flag=%d", a, b, c, (int)d); o_EdLevel(s, p, a, b, c, d); }
static void h_EdSmooth(void* s, V3* p, float a, float b, float c, bool d) { LOGV("EditSmooth", "f=%.3f %.3f %.3f flag=%d", a, b, c, (int)d); o_EdSmooth(s, p, a, b, c, d); }
static void h_EdRL(void* s, V3* p, float a, float b, float c, float e, bool d) { LOGV("EditRaiseLower", "f=%.3f %.3f %.3f %.3f flag=%d", a, b, c, e, (int)d); o_EdRL(s, p, a, b, c, e, d); }
static void h_EdRLA(void* s, V3* p, float a, float b, float c, float e, bool d, float g) {
    static int n = 0;
    if (n >= 120) { o_EdRLA(s, p, a, b, c, e, d, g); return; }
    ++n;
    V3 q{ p->x, 0, p->z }; float h0 = o_TerrHeight(s, &q);
    V3 keep = *p;
    o_EdRLA(s, p, a, b, c, e, d, g);
    V3 q2{ keep.x, 0, keep.z }; float h1 = o_TerrHeight(s, &q2);
    Log("GAME EdRLA tid=%lu p=(%.2f %.3f %.2f) a=%.6f b=%.6f c=%.7f e=%.6f flag=%d g=%.4f | h %.3f -> %.3f", GetCurrentThreadId(), keep.x, keep.y, keep.z, a, b, c, e, (int)d, g, h0, h1);
}
static float h_EdRLC(void* s, V3* p, float a, float b, float c, float e, bool d) { float r = o_EdRLC(s, p, a, b, c, e, d); LOGV("EditRaiseLowerAngleLimitGetCost", "f=%.3f %.3f %.3f %.3f flag=%d -> %.3f", a, b, c, e, (int)d, r); return r; }
static void h_EdSWL(void* s, V3* p, float a, float b, float c, float d, float e, float f, float g, float h, float i) { LOGV("EditSmoothWithLimits", "f=%.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f", a, b, c, d, e, f, g, h, i); o_EdSWL(s, p, a, b, c, d, e, f, g, h, i); }
static void h_RegenColl(void* s) { static int n = 0; if (n < 30) { ++n; Log("RegenerateCollisionDataSoviet this=%p", s); } o_RegenColl(s); }
static void h_RegenNorm(void* s) { static int n = 0; if (n < 30) { ++n; Log("RegenerateCollisionNormalDataSoviet this=%p", s); } o_RegenNorm(s); }

// ---- terrain levelling ----
// From the disassembly of C3D_TERRAIN::EditRaiseLower(pos, inner, outer, amount, limit, selfOpen):
//  - inner / outer are brush radii in heightmap texels; weight is 1 inside "inner" and falls to 0 at "outer";
//  - heights are normalised (0..1 of the terrain height scale); new = h + weight * amount;
//  - "limit" is a normalised target: raising stops at it, lowering stops at it, texels already past it are skipped;
//  - the heightmap texture must be open (HeightmapTextureOpen) or the engine reads a null buffer and crashes;
//    selfOpen = true makes the call open / close it itself;
//  - collision heights are refreshed by the call.
// A large amount with limit = target therefore puts every texel under the brush exactly on the target height.
// Runs from the Present hook: the texture is mapped on the D3D context, which is only safe on the render thread.
static std::mutex g_lvlMx;
static std::vector<V3> g_lvlPts;            // x, target height, z
static std::atomic<int> g_lvlLeft{ 0 };
static int g_lvlStage = 0;                  // 0 edit, 1 verify
static BldVoid_t f_HmOpen, f_HmClose;
typedef float (*TerrScale_t)(void*);
static TerrScale_t f_TerrScale;

void QueueLevel(const std::vector<V3>& pts) {
    std::lock_guard<std::mutex> l(g_lvlMx);
    g_lvlPts = pts; g_lvlStage = 0;
    g_lvlLeft = (int)pts.size();
    Log("level: queued %d points", (int)pts.size());
}
int LevelPending() { return g_lvlLeft; }

void LevelTick() {
    if (!g_lvlLeft || !g_terrain || !o_EdRL || !o_TerrHeight) return;
    std::lock_guard<std::mutex> l(g_lvlMx);
    if (!f_HmOpen) {
        HMODULE c3d = GetModuleHandleW(L"C3DDLL64.dll");
        f_HmOpen = (BldVoid_t)GetProcAddress(c3d, "?HeightmapTextureOpen@C3D_TERRAIN@@QEAAXXZ");
        f_HmClose = (BldVoid_t)GetProcAddress(c3d, "?HeightmapTextureClose@C3D_TERRAIN@@QEAAXXZ");
        f_TerrScale = (TerrScale_t)GetProcAddress(c3d, "?GetTerrainHeight@C3D_TERRAIN@@QEAAMXZ");
    }
    float scale = f_TerrScale ? f_TerrScale(g_terrain) : 0;
    if (!f_HmOpen || !f_HmClose || !(scale > 1.0f && scale < 100000.0f)) {
        Log("level: unavailable (open %p, close %p, height scale %.3f)", f_HmOpen, f_HmClose, scale);
        g_lvlLeft = 0; return;
    }
    if (g_lvlStage == 1) {                   // next frame: report how flat it became
        float worst = 0; int off = 0;
        for (auto& t : g_lvlPts) {
            V3 p{ t.x, 0, t.z };
            float d = fabsf(o_TerrHeight(g_terrain, &p) - t.y);
            if (d > worst) worst = d;
            if (d > 0.3f) ++off;
        }
        { V3 p0{ g_lvlPts[0].x, 0, g_lvlPts[0].z }; Log("level: first point target %.2f, now %.2f", g_lvlPts[0].y, o_TerrHeight(g_terrain, &p0)); }
        Log("level: done, worst deviation %.2f m, points off by more than 0.3 m: %d of %d", worst, off, (int)g_lvlPts.size());
        g_lvlLeft = 0; return;
    }
    float worst = 0;
    // world height = origin.y + normalised * scale; the terrain origin vector sits at +0x890 (x, y, z), as used by
    // the engine's own world -> texel conversion
    const float* tf = (const float*)((const BYTE*)g_terrain + 0x878);
    float originY = tf[7];
    Log("level: heightmap %u x %u texels; terrain floats", *(const unsigned*)((const BYTE*)g_terrain + 0x220), *(const unsigned*)((const BYTE*)g_terrain + 0x224));
    Log("level: terrain floats %.2f %.2f %.2f %.2f %.2f %.2f | origin %.2f %.2f %.2f", tf[0], tf[1], tf[2], tf[3], tf[4], tf[5], tf[6], tf[7], tf[8]);
    if (!(fabsf(originY) < 5000.0f)) originY = 0;
    // The heightmap is coarse (about 10 m per texel on a 20 km map) and heights are interpolated between texels,
    // so every texel around a footprint point matters. Texels shared by neighbouring pads get the mean target.
    const BYTE* tb = (const BYTE*)g_terrain;
    unsigned texW = *(const unsigned*)(tb + 0x220), texH = *(const unsigned*)(tb + 0x224);
    float originX = tf[6], originZ = tf[8], sizeX = tf[1], sizeZ = tf[2];
    if (texW < 16 || texW > 65536 || texH < 16 || texH > 65536 || !(sizeX > 10) || !(sizeZ > 10)) {
        Log("level: unexpected terrain layout, cancelled"); g_lvlLeft = 0; return;
    }
    std::map<unsigned long long, std::pair<double, int>> cells;
    for (auto& t : g_lvlPts) {
        V3 p{ t.x, 0, t.z };
        float d = fabsf(t.y - o_TerrHeight(g_terrain, &p));
        if (d > worst) worst = d;
        float fx = (t.x - originX) / sizeX * texW, fz = (t.z - originZ) / sizeZ * texH;
        int ix = (int)floorf(fx), iz = (int)floorf(fz);
        for (int a = 0; a < 2; ++a) for (int b = 0; b < 2; ++b) {
            int cx = ix + a, cz = iz + b;
            if (cx < 1 || cz < 1 || cx >= (int)texW - 1 || cz >= (int)texH - 1) continue;
            auto& c = cells[((unsigned long long)cx << 32) | (unsigned)cz];
            c.first += t.y; ++c.second;
        }
    }
    f_HmOpen(g_terrain);
    for (auto& kv : cells) {
        int cx = (int)(kv.first >> 32), cz = (int)(kv.first & 0xffffffffu);
        float target = (float)(kv.second.first / kv.second.second);
        float lim = (target - originY) / scale;
        V3 q{ originX + cx * sizeX / texW, target, originZ + cz * sizeZ / texH };
        o_EdRL(g_terrain, &q, 0.2f, 0.3f, 1.0f, lim, false);     // brush smaller than a texel: exactly this one (inner must be > 0: the engine skips distance == inner)
        q = { originX + cx * sizeX / texW, target, originZ + cz * sizeZ / texH };
        o_EdRL(g_terrain, &q, 0.2f, 0.3f, -1.0f, lim, false);
    }
    Log("level: %d heightmap texels set", (int)cells.size());
    f_HmClose(g_terrain);
    Log("level: edited %d points, height scale %.2f, worst gap before %.2f m", (int)g_lvlPts.size(), scale, worst);
    g_lvlStage = 1;
}

// ---- existing buildings / paths ----
// The game keeps its buildings in the engine's C3D_BUILDING manager and asks it for 2D bounding-box hits itself,
// so the same query tells the planner where something already stands.
static BldVoid_t o_BldRenderSel;
typedef void (*PathRefresh_t)(void*, void*, void*, unsigned, float, float);
static PathRefresh_t o_PathRefresh;
static void h_BldRenderSel(void* s) { if (g_bld != s) { g_bld = s; Log("buildings = %p", s); } o_BldRenderSel(s); }
static void h_PathRefresh(void* s, void* cam, void* list, unsigned n, float a, float b) { if (g_path != s) { g_path = s; Log("paths = %p", s); } o_PathRefresh(s, cam, list, n, a, b); }
typedef bool (*BldHit_t)(void*, V3*, int, int);
typedef bool (*PathHit_t)(void*, V3*, void**, bool);
int Occupied(float x, float z) {
    static BldHit_t f_bld; static PathHit_t f_path; static bool looked = false;
    if (!looked) {
        looked = true;
        HMODULE c3d = GetModuleHandleW(L"C3DDLL64.dll");
        f_bld = (BldHit_t)GetProcAddress(c3d, "?Collision2DBBoxIgnore@C3D_BUILDING@@QEAA_NVC3DVECTOR3@@HH@Z");
        f_path = (PathHit_t)GetProcAddress(c3d, "?Collision@C3D_PATH@@QEAA_NVC3DVECTOR3@@PEAPEAX_N@Z");
    }
    int r = 0;
    if (g_bld && f_bld) { V3 p{ x, TerrainHeight(x, z), z }; if (f_bld(g_bld, &p, -1, -1)) r |= 1; }
    if (g_path && f_path) { V3 p{ x, TerrainHeight(x, z), z }; void* frag = nullptr; if (f_path(g_path, &p, &frag, false)) r |= 2; }
    return r;
}
bool OccupancyReady() { return g_bld != nullptr; }

bool CursorActive() { return GetTickCount() - g_rotTick < 500; }      // the game is showing a building to place
float CursorRot() { return g_lastRot; }
// Building picked last in the game's build menu (ini name) and when; empty if nothing was picked since the map loaded.
std::string SelectedType(DWORD* tick) {
    if (tick) *tick = g_typeTick;
    return g_typeTick ? std::string(g_lastTypeName) : std::string();
}
std::string CursorTypeName() { return SelectedType(nullptr); }
// Scripted clicks for the game's line tools (footpaths, roads): each step moves the fake mouse to a world point,
// waits, and optionally presses the left or right button during its last frames.
struct ScriptStep { V3 pos; int action; int frames; };
static std::vector<ScriptStep> g_scr;
static size_t g_scrIdx = 0; static int g_scrTicks = 0;
static HWND g_scrWnd = nullptr; static bool g_scrBtn = false;
static bool Spoof() { return g_abActive && g_abMode == 0; }
static void ScriptButton(bool down) {
    if (down == g_scrBtn) return;
    g_scrBtn = down;
    INPUT in{}; in.type = INPUT_MOUSE; in.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    SendInput(1, &in, sizeof(in));
}
static void ScriptEnd(const char* why) {
    ScriptButton(false);
    g_abActive = false; g_abClick = g_abRClick = false;
    Log("script: %s, %d lines drawn, %d steps left", why, (int)g_abDone, (int)g_abLeft);
}
// The game's line tools (roads, footpaths) only react to a real press-drag-release: the game reads the mouse on
// two threads and treats a faked hold as a camera drag. So the script moves the real cursor and holds the real
// button; Esc or switching to another window stops it.
static void ScriptTick() {
    std::lock_guard<std::mutex> l(g_abMx);
    if (!g_abActive) return;
    if (g_scrIdx >= g_scr.size()) { ScriptEnd("finished"); return; }
    if (GetForegroundWindow() != g_scrWnd) { ++g_abFailed; ScriptEnd("the game lost focus, stopped"); return; }
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) { ScriptEnd("stopped with Esc"); return; }
    const ScriptStep& st = g_scr[g_scrIdx];
    float sx, sy;
    if (!WorldToScreen({ st.pos.x, TerrainHeight(st.pos.x, st.pos.z), st.pos.z }, g_abW, g_abH, sx, sy) ||
        sx < 60 || sy < 70 || sx > g_abW - 60 || sy > g_abH - 190) {
        ++g_abFailed; ScriptEnd("a point is off screen or under the game HUD, stopped"); return;
    }
    POINT pt{ (LONG)(sx + 0.5f), (LONG)(sy + 0.5f) };
    ClientToScreen(g_scrWnd, &pt);
    SetCursorPos(pt.x + (g_scrTicks & 1), pt.y);       // the one-pixel wobble keeps "mouse moved" events coming
    ++g_scrTicks;
    ScriptButton(st.action == 3 && g_scrTicks > 2);
    if (g_scrTicks >= st.frames) {
        bool held = st.action == 3 && g_scrIdx + 1 < g_scr.size() && g_scr[g_scrIdx + 1].action == 3;   // drag goes on
        if (st.action == 3 && !held) ++g_abDone;
        if (!held) ScriptButton(false);
        ++g_scrIdx; g_scrTicks = held ? 3 : 0;
        g_abLeft = (int)(g_scr.size() - g_scrIdx);
    }
}
void StartScript(const std::vector<V3>& pos, const std::vector<int>& action, const std::vector<int>& frames, float sw, float sh) {
    std::lock_guard<std::mutex> l(g_abMx);
    g_scr.clear();
    for (size_t i = 0; i < pos.size(); ++i) g_scr.push_back({ pos[i], action[i], frames[i] });
    g_scrIdx = 0; g_scrTicks = 0; g_abClick = g_abRClick = false; g_abAxis = 0;
    g_abDone = 0; g_abFailed = 0; g_abLeft = (int)g_scr.size(); g_abW = sw; g_abH = sh;
    if (g_scr.empty()) return;
    Log("script: %d steps", (int)g_scr.size());
    g_scrWnd = GetForegroundWindow(); g_scrBtn = false;
    g_abMode = 1; g_abActive = true;
}
void StartAutoBuild(const std::vector<V3>& pos, const std::vector<float>& rot, float sw, float sh) {
    g_abMode = 0;
    std::lock_guard<std::mutex> l(g_abMx);
    g_abJobs.clear();
    for (size_t i = 0; i < pos.size(); ++i) g_abJobs.push_back({ pos[i], rot[i] });
    g_abIdx = 0; g_abPhase = 10; g_abTicks = 0; g_abStable = 0; g_abRotTries = 0; g_abRetry = 0; g_abAxis = 0; g_abClick = false;
    g_abDone = 0; g_abFailed = 0; g_abLeft = (int)g_abJobs.size(); g_abW = sw; g_abH = sh;
    if (g_abJobs.empty()) return;
    g_abAim = g_abJobs[0].pos;
    Log("autobuild: %d targets, cursor type %d '%s'", (int)g_abJobs.size(), g_curType, CursorTypeName().c_str());
    g_abActive = true;
}
bool ScriptActive() { return g_abActive && g_abMode == 1; }
void StopAutoBuild() { if (g_abActive && g_abMode == 1) ScriptButton(false); g_abActive = false; g_abClick = false; g_abRClick = false; g_abAxis = 0; }
bool AutoBuildActive() {
    return g_abActive;
}
void AutoBuildStats(int& done, int& failed, int& left) { done = g_abDone; failed = g_abFailed; left = g_abLeft; }

// Diagnostics / feedback: what the game asks the building manager about while a building is being placed.
typedef int   (*RotStep_t)(void*, float);
typedef void* (*RotMat_t)(void*, void*, int);
typedef int   (*TypeIdx_t)(void*, char*);
typedef void* (*TypeByName_t)(void*, char*, bool);
typedef void* (*TypeByIdx_t)(void*, int);
static RotStep_t o_RotStep; static RotMat_t o_RotMat; static TypeIdx_t o_TypeIdx, o_TypeIdxNoLog;
static TypeByName_t o_TypeByName; static TypeByIdx_t o_TypeByIdx;
static std::atomic<int> g_nRotStep{ 0 }, g_nRotMat{ 0 }, g_nTypeIdx{ 0 }, g_nTypeName{ 0 }, g_nTypeByIdx{ 0 };
static std::atomic<int> g_lastStep{ -1 }, g_lastStepFromRot{ -1 }, g_lastTypeIdx{ -1 };
static int h_RotStep(void* s, float rot) { int r = o_RotStep(s, rot); g_rotTick = GetTickCount(); g_lastRot = rot; g_lastStepFromRot = r; ++g_nRotStep; return r; }
static void* h_RotMat(void* s, void* out, int step) { g_lastStep = step; ++g_nRotMat; return o_RotMat(s, out, step); }
static int h_TypeIdx(void* s, char* n) { int r = o_TypeIdx(s, n); g_typeTick = GetTickCount(); if (n) strncpy(g_lastTypeName, n, 95); g_lastTypeIdx = r; ++g_nTypeIdx; return r; }
static int h_TypeIdxNoLog(void* s, char* n) { int r = o_TypeIdxNoLog(s, n); g_typeTick = GetTickCount(); if (n) strncpy(g_lastTypeName, n, 95); g_lastTypeIdx = r; ++g_nTypeIdx; return r; }
static void* h_TypeByName(void* s, char* n, bool b) { if (n) strncpy(g_lastTypeName, n, 95); ++g_nTypeName; return o_TypeByName(s, n, b); }
static void* h_TypeByIdx(void* s, int i) { g_lastTypeIdx = i; ++g_nTypeByIdx; return o_TypeByIdx(s, i); }

struct IatHook { const char* name; void* hook; void** orig; };

// ---------------------------------------------------------------------------------------------------------
// Direct calls into SOVIET64.exe, learnt from the game's own "copy area / paste area" tool (paste = function
// at RVA 0x2168F0). Paste places a copied building by pointing the build tool at it and calling the building
// tool's update function with "place without a click"; we do the same for the buildings of the plan.
// The addresses are for game build 1.1.1.13 only and are checked before use.
namespace gx {
static BYTE* Base() { return (BYTE*)GetModuleHandleW(nullptr); }
static BYTE* Game() { return Base() + 0x9d4f10; }                 // the game state object (a global)
static const BYTE kSigBuild[16] = { 0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xac };
static const BYTE kSigFind[16]  = { 0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7c,0x24,0x18,0x4c };
static bool Supported() {
    static int ok = -1;
    if (ok < 0) {
        BYTE* b = Base();
        auto* nt = (IMAGE_NT_HEADERS*)(b + ((IMAGE_DOS_HEADER*)b)->e_lfanew);
        ok = nt->OptionalHeader.SizeOfImage == 0xa9d000 && memcmp(b + 0x2bad60, kSigBuild, 16) == 0 && memcmp(b + 0x3aaa0, kSigFind, 16) == 0;
        Log("direct build calls: %s", ok ? "available" : "this game build is not supported");
    }
    return ok == 1;
}
// building tool update: (game, type name, force place, place if the spot is valid, unused, rotation step, 0, 0)
typedef void (*BuildTool_t)(void*, const char*, unsigned char, unsigned char, UINT64, int, unsigned char, unsigned char);
// tools: vector at game+0xd280 of 0x2d0-byte records, the name is at the start of a record
static const char* FindTool(const char* name, void** tool) {
    BYTE* G = Game();
    BYTE* b = *(BYTE**)(G + 0xd280); BYTE* e = *(BYTE**)(G + 0xd288);
    for (; b && b < e; b += 0x2d0) if (_stricmp((const char*)b, name) == 0) { *tool = b; return (const char*)b; }
    return nullptr;
}
static void DumpTools() {
    BYTE* G = Game();
    BYTE* b = *(BYTE**)(G + 0xd280); BYTE* e = *(BYTE**)(G + 0xd288);
    int n = 0, shown = 0;
    for (; b && b < e; b += 0x2d0, ++n) {
        const char* nm = (const char*)b;
        if (shown < 150 && (strstr(nm, "road") || strstr(nm, "pedestrian") || strstr(nm, "pipe") || strstr(nm, "eletric") || strstr(nm, "heating") || strstr(nm, "sewage"))) {
            ++shown;
            const int* f = (const int*)(b + 0x40);
            Log("TOOL %d '%s' | %d %d %d %d %d %d %d %d %d %d %d %d", n, nm, f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9], f[10], f[11]);
        }
    }
    Log("tools in the game: %d", n);
}
static int PlaceRaw(const char* nm, void* tool, const V3& pos, float rot, int step) {
    BYTE* G = Game();
    V3 sp = *(V3*)(G + 0xf6c); float sr = *(float*)(G + 0xf94); void* st = *(void**)(G + 0xd428); BYTE sm = G[0x11db0];
    int code = -200;
    __try {
        *(V3*)(G + 0xf6c) = pos; *(float*)(G + 0xf94) = rot; G[0x11db0] = 0; *(void**)(G + 0xd428) = tool;
        ((BuildTool_t)(Base() + 0x2bad60))(G, nm, 0, 1, 0, step, 0, 0);
        code = *(int*)(G + 0x11a10);
    } __except (EXCEPTION_EXECUTE_HANDLER) { code = -300; }
    *(V3*)(G + 0xf6c) = sp; *(float*)(G + 0xf94) = sr; *(void**)(G + 0xd428) = st; G[0x11db0] = sm;
    return code;
}

// A piece of road / footpath / pipe the way paste does it: snap the point like the cursor would (this is what
// catches building connections and existing ends), start, update to the end point, commit if a preview exists.
// Where a caught fault happened, for the log (module + offset), so a refused call can be looked up in the disassembly.
static int CrashInfo(EXCEPTION_POINTERS* ep, const char* what, int stage) {
    static int n = 0;
    if (n++ < 12) {
        void* a = ep->ExceptionRecord->ExceptionAddress; HMODULE mod = nullptr; char nm[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)a, &mod) && mod) GetModuleFileNameA(mod, nm, MAX_PATH);
        const char* base = strrchr(nm, '\\');
        Log("FAULT in %s stage %d: code %08lx at %s+0x%llx, touching %p", what, stage, ep->ExceptionRecord->ExceptionCode, base ? base + 1 : nm,
            (unsigned long long)((BYTE*)a - (BYTE*)mod), ep->ExceptionRecord->NumberParameters > 1 ? (void*)ep->ExceptionRecord->ExceptionInformation[1] : nullptr);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
typedef void (*Snap_t)(void*, void*, int, int);
// footpath snap itself (the dispatcher 0x21ba40 calls it with r8b = 0); the game's own building tool calls it with
// r8b = 1 when it links an entrance to a nearby footpath, which also catches the middle of a footpath piece
typedef void (*PathSnap_t)(void*, int, unsigned char, UINT64, unsigned char, unsigned char);
int g_pathSnapMode = 1;
bool g_straightLines = true;   // set the start direction of every line piece (debug flag DistrictPlanner.straight 0/1)     // 0 = like the area paste (ends only), 1 = like the building tool (middle of pieces too)
// road snap itself (the dispatcher calls it with r8b = 0 and 1.0 at [rsp+0x38]); the building tool calls it with
// r8b = 1 (0x2bd2ce), which also catches the middle of a road piece - a driveway then makes a T-junction
typedef void (*RoadSnap_t)(void*, int, unsigned char, UINT64, unsigned char, unsigned char, unsigned char, float, unsigned char, unsigned char);
// The connection snaps look for buildings only in a candidate list at G+0x11dc0..0x11dc8 that the building update
// (0x139a70) refills every tick - the buildings in view. A line end at a building out of view (or not yet in the list)
// finds nothing. For the duration of a snap the list is replaced by the buildings standing near the point.
typedef V3* (*NodeVec2_t)(void*, V3*);
static BYTE* g_near[256];
static int NearBuildings(const V3& p, float radius) {
    static NodeVec2_t gp = nullptr;
    if (!gp) gp = (NodeVec2_t)GetProcAddress(GetModuleHandleW(L"C3DDLL64.dll"), "?GetPosition@C3D_NODE@@QEAA?AVC3DVECTOR3@@XZ");
    if (!gp) return 0;
    BYTE** v = *(BYTE***)(Base() + 0x9e6a18); BYTE** e = *(BYTE***)(Base() + 0x9e6a20);
    int n = 0;
    for (BYTE** it = v; it && it < e && n < 256; ++it) {
        if (!*it || !*(BYTE**)(*it + 0x318)) continue;
        V3 q{}; gp(*it + 0x320, &q);
        if (fabsf(q.x - p.x) < radius && fabsf(q.z - p.z) < radius) g_near[n++] = *it;
    }
    return n;
}
static void SnapNear(BYTE* G, void* m, int mgrOffset, int type, int sub);
static void SnapAt(BYTE* G, void* m, int mgrOffset, int type, int sub) {
    if (mgrOffset == 0xf968 && g_pathSnapMode == 1) ((PathSnap_t)(Base() + 0x3d4a80))(G, 0, 1, 0, 0, 0);
    else if (mgrOffset == 0xf7a0 && g_pathSnapMode == 1) ((RoadSnap_t)(Base() + 0x3c4790))(G, 0, 1, 0, 0, 0, 0, 1.0f, 0, 0);
    else ((Snap_t)(Base() + 0x21ba40))(G, m, type, sub);
}
static void SnapNearRaw(BYTE* G, void* m, int mgrOffset, int type, int sub);
// A snap may pull an end several metres onto something else (a pavement end onto the street axis, a driveway corner
// back onto the building). An end is accepted only where the plan put it: moved by at most 1.5 m; otherwise the point
// stays and what the snap attached is cleared.
static void SnapNear(BYTE* G, void* m, int mgrOffset, int type, int sub) {
    V3 want = *(V3*)(G + 0xf6c);
    SnapNearRaw(G, m, mgrOffset, type, sub);
    V3 got = *(V3*)(G + 0xf6c);
    if (mgrOffset == 0xf7a0 && fabsf(got.x - want.x) + fabsf(got.z - want.z) > 1.5f) {
        *(V3*)(G + 0xf6c) = want;
        *(void**)(G + 0xf648) = nullptr; *(void**)(G + 0xf650) = nullptr; *(void**)(G + 0xf680) = nullptr;
        *(void**)(G + 0xf690) = nullptr; *(void**)(G + 0xf6a0) = nullptr;
    }
}
static void SnapNearRaw(BYTE* G, void* m, int mgrOffset, int type, int sub) {
    int n = NearBuildings(*(V3*)(G + 0xf6c), 80.0f);
    BYTE** lst = (BYTE**)(G + 0x11dc0);
    BYTE* save0 = lst[0]; BYTE* save1 = lst[1]; BYTE* save2 = lst[2];
    if (n > 0) { lst[0] = (BYTE*)g_near; lst[1] = (BYTE*)(g_near + n); lst[2] = (BYTE*)(g_near + n); }
    __try { SnapAt(G, m, mgrOffset, type, sub); } __finally { lst[0] = save0; lst[1] = save1; lst[2] = save2; }
}
typedef void* (*PStart_t)(void*, V3*, UINT64, unsigned char, int, UINT64, V3*);
typedef void* (*PUpdate_t)(void*, V3*, int, int, unsigned char, unsigned char, unsigned char, UINT64, unsigned char, unsigned char);
typedef void* (*PCommit_t)(void*, UINT64, UINT64, UINT64, unsigned char, unsigned char, unsigned char);
typedef void* (*PCancel_t)(void*);
static int LineRaw(int mgrOffset, int type, int sub, int arg, const char* toolName, V3 a, V3 b) {
    BYTE* G = Game();
    void* m = *(void**)(G + mgrOffset);
    if (!m) return -100;
    V3 sp = *(V3*)(G + 0xf6c);
    // the snap reads the current tool when it meets a building connection (a null tool faults at exe+0x3c7712)
    void* st = *(void**)(G + 0xd428); void* tool = nullptr;
    if (toolName && FindTool(toolName, &tool)) *(void**)(G + 0xd428) = tool;
    int r = -200; volatile int stage = 0; V3 a0{}, b0{};
    __try {
        a0 = a; b0 = b;
        *(V3*)(G + 0xf6c) = a; SnapNear(G, m, mgrOffset, type, sub); a = *(V3*)(G + 0xf6c);
        void* sa[4] = { *(void**)(G + 0xf648), *(void**)(G + 0xf650), *(void**)(G + 0xf6a0), *(void**)(G + 0xf690) };
        V3 out{}; stage = 1;
        ((PStart_t)(Base() + 0x4fbfd0))(m, &a, (UINT64)(unsigned)sub, 0, arg, 0, &out);
        // start tangent: start (0x4fbfd0, 0x4fc350) copies the direction of the existing piece it attaches to into
        // mgr+0x328, and the update bends the new piece to continue it - that is what made arcs and loops. A zero
        // vector there (what start writes when nothing is attached) gives a straight piece.
        if (g_straightLines) *(V3*)((BYTE*)m + 0x328) = V3{ 0, 0, 0 };
        stage = 2; *(V3*)(G + 0xf6c) = b; SnapNear(G, m, mgrOffset, type, sub); b = *(V3*)(G + 0xf6c);
        stage = 3; ((PUpdate_t)(Base() + 0x4fca80))(m, &b, type, sub, 0, 0, 0, 0, 0, 0);
        void* p428 = *(void**)((BYTE*)m + 0x428); void* p430 = *(void**)((BYTE*)m + 0x430);
        {   static int nl2 = 0;
            if ((mgrOffset == 0xf7a0 || mgrOffset == 0xf968) && nl2++ < 60)
                Log("SNAP mgr+0x%x a(%.1f %.1f)->(%.1f %.1f) objs %p %p %p %p | b(%.1f %.1f)->(%.1f %.1f) objs %p %p %p %p", mgrOffset, a0.x, a0.z, a.x, a.z, sa[0], sa[1], sa[2], sa[3],
                    b0.x, b0.z, b.x, b.z, *(void**)(G + 0xf648), *(void**)(G + 0xf650), *(void**)(G + 0xf6a0), *(void**)(G + 0xf690)); }
        if (mgrOffset != 0xf7a0 && mgrOffset != 0xf968) {
            static int nlog = 0;
            if (nlog++ < 40) Log("LINE mgr+0x%x type %d sub %d arg %d: a %.1f %.1f %.1f -> %.1f %.1f %.1f, b %.1f %.1f %.1f -> %.1f %.1f %.1f, preview %p %p, snap objs %p %p %p %p",
                mgrOffset, type, sub, arg, a0.x, a0.y, a0.z, a.x, a.y, a.z, b0.x, b0.y, b0.z, b.x, b.y, b.z, p428, p430,
                *(void**)(G + 0xf648), *(void**)(G + 0xf650), *(void**)(G + 0xf6a0), *(void**)(G + 0xf690));
        }
        stage = 4;
        if (p430 && p428 != p430) {
            // the real test: the manager's node list (mgr+0x298..0x2a0) grows when the game accepts the piece
            size_t n0 = (size_t)(*(BYTE**)((BYTE*)m + 0x2a0) - *(BYTE**)((BYTE*)m + 0x298)) / 8;
            ((PCommit_t)(Base() + 0x518a10))(m, 0, 0, 0, 0, 0, 0);
            size_t n1 = (size_t)(*(BYTE**)((BYTE*)m + 0x2a0) - *(BYTE**)((BYTE*)m + 0x298)) / 8;
            r = 1;   // the node count is logged only: a piece that continues an existing end may not add nodes, so it is no proof
            static int nl3 = 0;
            if (nl3++ < 80) Log("COMMIT mgr+0x%x nodes %zu -> %zu (%s) %.1f %.1f -> %.1f %.1f", mgrOffset, n0, n1, r == 1 ? "built" : "REFUSED", a.x, a.z, b.x, b.z);
        }
        else { ((PCancel_t)(Base() + 0x520530))(m); r = 0; }
    } __except (CrashInfo(GetExceptionInformation(), "line", stage)) { r = -300; }
    *(V3*)(G + 0xf6c) = sp; *(void**)(G + 0xd428) = st;
    return r;
}

// Logging detours on the path tool functions, to learn their arguments from the game's own use.
typedef void* (*P10)(void*, void*, UINT64, UINT64, UINT64, UINT64, UINT64, UINT64, UINT64, UINT64);
static P10 t_PathUpdate, t_PathStart, t_PathCommit;
static int MgrOffset(void* m) {
    BYTE* G = Game();
    for (int o = 0xf000; o < 0xfc00; o += 8) if (*(void**)(G + o) == m) return o;
    return 0;
}
static void* d_PathUpdate(void* m, void* p, UINT64 a3, UINT64 a4, UINT64 a5, UINT64 a6, UINT64 a7, UINT64 a8, UINT64 a9, UINT64 a10) {
    static void* lm; static UINT64 l[8]; static int n = 0;
    UINT64 c[8] = { a3 & 0xffffffff, a4 & 0xffffffff, a5 & 0xff, a6 & 0xff, a7 & 0xff, a8, a9 & 0xff, a10 & 0xff };
    if (n < 400 && (m != lm || memcmp(c, l, sizeof(c)) != 0)) {
        ++n; lm = m; memcpy(l, c, sizeof(c));
        const float* f = (const float*)p;
        Log("PATH update mgr %p (game+0x%x) pos %.1f %.1f %.1f type %d sub %d a5 %d a6 %d a7 %d a8 %p a9 %d a10 %d thread %lu",
            m, MgrOffset(m), f ? f[0] : 0, f ? f[1] : 0, f ? f[2] : 0, (int)c[0], (int)c[1], (int)c[2], (int)c[3], (int)c[4], (void*)a8, (int)c[6], (int)c[7], GetCurrentThreadId());
    }
    return t_PathUpdate(m, p, a3, a4, a5, a6, a7, a8, a9, a10);
}
static void* d_PathStart(void* m, void* p, UINT64 a3, UINT64 a4, UINT64 a5, UINT64 a6, UINT64 a7, UINT64 a8, UINT64 a9, UINT64 a10) {
    static int n = 0;
    if (n < 400) { ++n; const float* f = (const float*)p;
        Log("PATH start mgr %p (game+0x%x) pos %.1f %.1f %.1f r8 %llx r9 %d a5 %d a6 %llx a7 %p", m, MgrOffset(m), f ? f[0] : 0, f ? f[1] : 0, f ? f[2] : 0, a3, (int)(a4 & 0xff), (int)a5, a6, (void*)a7); }
    return t_PathStart(m, p, a3, a4, a5, a6, a7, a8, a9, a10);
}
static void* d_PathCommit(void* m, void* p, UINT64 a3, UINT64 a4, UINT64 a5, UINT64 a6, UINT64 a7, UINT64 a8, UINT64 a9, UINT64 a10) {
    static int n = 0;
    if (n < 400) { ++n; Log("PATH commit mgr %p (game+0x%x) rdx %p r8 %llx r9 %llx a5 %d a6 %d a7 %d | preview %p %p", m, MgrOffset(m), p, a3, a4, (int)(a5 & 0xff), (int)(a6 & 0xff), (int)(a7 & 0xff),
                       *(void**)((BYTE*)m + 0x428), *(void**)((BYTE*)m + 0x430)); }
    return t_PathCommit(m, p, a3, a4, a5, a6, a7, a8, a9, a10);
}
static void* Detour(DWORD rva, int stolen, void* hook) {
    BYTE* fn = Base() + rva;
    BYTE* tr = (BYTE*)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tr) return nullptr;
    memcpy(tr, fn, stolen);
    BYTE jmp[14] = { 0xff, 0x25, 0, 0, 0, 0 };
    UINT64 back = (UINT64)(fn + stolen); memcpy(jmp + 6, &back, 8); memcpy(tr + stolen, jmp, 14);
    DWORD old;
    if (!VirtualProtect(fn, 32, PAGE_EXECUTE_READWRITE, &old)) return nullptr;
    UINT64 to = (UINT64)hook; memcpy(jmp + 6, &to, 8); memcpy(fn, jmp, 14);
    for (int i = 14; i < stolen; ++i) fn[i] = 0x90;
    VirtualProtect(fn, 32, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, 32);
    return tr;
}
static void InstallPathLogging() {
    static bool done = false;
    if (done || !Supported()) return;
    done = true;
    t_PathUpdate = (P10)Detour(0x4fca80, 21, d_PathUpdate);
    t_PathStart  = (P10)Detour(0x4fbfd0, 15, d_PathStart);
    t_PathCommit = (P10)Detour(0x518a10, 19, d_PathCommit);
    Log("path logging detours: %p %p %p", t_PathUpdate, t_PathStart, t_PathCommit);
}
} // namespace gx

static std::vector<InstantJob> g_inst; static std::vector<int> g_instRes; static size_t g_instAt = 0; static std::mutex g_instMx;
static std::atomic<bool> g_instBusy{ false };
static std::atomic<bool> g_dumpTools{ false };
static char g_dbgTool[96] = ""; static std::atomic<bool> g_dbgToolSet{ false };
bool InstantSupported() { return gx::Supported(); }
void QueueInstant(const std::vector<InstantJob>& jobs) {
    std::lock_guard<std::mutex> l(g_instMx);
    g_inst = jobs; g_instRes.assign(jobs.size(), -999); g_instAt = 0; g_instBusy = !jobs.empty();
    Log("instant build: %d jobs queued", (int)jobs.size());
}
bool InstantBusy() { return g_instBusy; }
void InstantResults(std::vector<int>& codes) { std::lock_guard<std::mutex> l(g_instMx); codes = g_instRes; }
// Runs on the game's own update thread (from one of its per-frame input polls).
static void GameThreadTick() {
    if (!gx::Supported()) return;
    if (g_dumpTools.exchange(false)) gx::DumpTools();
    if (g_dumpLoad.exchange(false)) DumpLoadNow();
    if (g_dbgToolSet.exchange(false)) {
        void* tool = nullptr; const char* nm = gx::FindTool(g_dbgTool, &tool);
        if (nm) *(void**)(gx::Game() + 0xd428) = tool;
        Log("debug: current tool set to '%s' -> %p", g_dbgTool, tool);
    }
    if (!g_instBusy) return;
    for (int k = 0; k < 12; ++k) {
        InstantJob j; size_t at;
        { std::lock_guard<std::mutex> l(g_instMx); if (g_instAt >= g_inst.size()) { g_instBusy = false; Log("instant build: done"); return; } at = g_instAt++; j = g_inst[at]; }
        int code;
        if (j.kind == 0) {
            void* tool = nullptr;
            const char* nm = gx::FindTool(j.type.c_str(), &tool);
            code = -100;
            if (nm) {
                V3 p{ j.a.x, TerrainHeight(j.a.x, j.a.z), j.a.z };
                int step = (o_RotStep && g_bld) ? o_RotStep(g_bld, j.rot + 6.2831853f / 256.0f) : 0;
                code = gx::PlaceRaw(nm, tool, p, j.rot, step);
            }
            if (code != -1) Log("instant build: '%s' at %.0f %.0f -> %s (code %d)", j.type.c_str(), j.a.x, j.a.z,
                                code == -100 ? "no such build tool" : code == -300 ? "CRASH caught" : "refused by the game", code);
            k += 3;         // a building is heavier than a line
        } else {
            // ends at a building sit at its connection point's height (wires on poles 5-7 m up, pipes 2-4 m down)
            V3 a{ j.a.x, TerrainHeight(j.a.x, j.a.z) + j.ha, j.a.z }, b{ j.b.x, TerrainHeight(j.b.x, j.b.z) + j.hb, j.b.z };
            // networks seen in the game's own calls: roads G+0xf7a0 (type -1 mud, 0 gravel, 1 asphalt, 2 asphalt with pavements), footpaths G+0xf968,
            // pipes G+0xfaf0 (type 20; sub 0/1 water small/big, 5/6 sewage), heating G+0xfa98 (type 10; sub 0 small, 1 big),
            // medium voltage G+0xfa20 and high voltage G+0xfa18 (type = start argument = 10 overhead, 20 underground); the start argument is -20 elsewhere
            const char* tn = j.kind == 1 ? (j.lineType < 0 ? "road_mud" : j.lineType == 0 ? "road_gravel" : j.lineType == 2 ? "road_asphaltside" : "road_asphalt")
                           : j.kind == 2 ? (j.lineType < 0 ? "pedestrianway_mud" : j.lineType == 0 ? "pedestrianway" : "pedestrianway_asplhalt")
                           : j.mgr == 0xfaf0 ? (j.sub >= 5 ? "sewagepipe_0" : "waterpipe_0") : j.mgr == 0xfa98 ? "heating_pipe_small" : j.lineType == 20 ? "eletric_ulow0" : "eletric_low0";
            if (j.kind == 3) code = gx::LineRaw(j.mgr, j.lineType, j.sub, j.arg, tn, a, b);
            else code = gx::LineRaw(j.kind == 1 ? 0xf7a0 : 0xf968, j.lineType, 0, -20, tn, a, b);
            if (code != 1) Log("instant build: %s %.0f %.0f -> %.0f %.0f: %s (code %d)", j.kind == 1 ? "road" : j.kind == 3 ? (j.mgr == 0xfaf0 ? (j.sub >= 5 ? "sewage" : "water") : j.mgr == 0xfa98 ? "heating" : "power") : "footpath", a.x, a.z, b.x, b.z,
                               code == -300 ? "CRASH caught" : "refused by the game", code);
        }
        { std::lock_guard<std::mutex> l(g_instMx); if (at < g_instRes.size()) g_instRes[at] = code; }
    }
}
void DebugDumpTools() { g_dumpTools = true; }
typedef V3* (*NodeVec_t)(void*, V3*);
static bool ReadWorldBld(BYTE* b, NodeVec_t gp, NodeVec_t gr, char* nm, V3& pos, float& yaw) {
    __try {
        BYTE* t = *(BYTE**)(b + 0x318); if (!t) return false;
        strncpy(nm, (const char*)t, 63); nm[63] = 0;
        V3 p{}, r{}; gp(b + 0x320, &p); gr(b + 0x320, &r);
        pos = p; yaw = r.y;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void ListBuildings(std::vector<WorldBld>& out) {
    out.clear();
    if (!gx::Supported()) return;
    static NodeVec_t gp = nullptr, gr = nullptr;
    if (!gp) {
        HMODULE c3d = GetModuleHandleW(L"C3DDLL64.dll");
        gp = (NodeVec_t)GetProcAddress(c3d, "?GetPosition@C3D_NODE@@QEAA?AVC3DVECTOR3@@XZ");
        gr = (NodeVec_t)GetProcAddress(c3d, "?GetRotation@C3D_NODE@@QEAA?AVC3DVECTOR3@@XZ");
        if (!gp || !gr) { Log("ListBuildings: C3D_NODE getters not found"); return; }
    }
    BYTE** v = *(BYTE***)(gx::Base() + 0x9e6a18); BYTE** e = *(BYTE***)(gx::Base() + 0x9e6a20);
    for (BYTE** it = v; it && it < e; ++it) {
        char nm[64]; WorldBld w;
        if (!*it || !ReadWorldBld(*it, gp, gr, nm, w.pos, w.yaw)) continue;
        w.id = strncmp(nm, "MIRRORZ_", 8) == 0 ? std::string(nm + 8) : std::string(nm);
        out.push_back(w);
    }
}
// Debug: per building type, the values the game uses for node load (power +0x10f8, heat +0x1060 are pointers to floats).
void DebugDumpLoad() { g_dumpLoad = true; }
void LogDarkness() {
    static DWORD last = 0; DWORD now = GetTickCount();
    if (now - last < 20000) return;
    last = now;
    Log("DARK %g", *(float*)(gx::Base() + 0x9d5d84));
}
struct BldRaw { char name[64]; int id; float p, h; long long pr, hr; int lp, lh; };
static bool ReadBld(BYTE* b, BldRaw& o) {
    __try {
        BYTE* t = *(BYTE**)(b + 0x318); if (!t) return false;
        strncpy(o.name, (const char*)t, 63); o.name[63] = 0; o.id = *(int*)(t + 0x360);
        float* p = *(float**)(b + 0x10f8); float* h = *(float**)(b + 0x1060);
        o.p = p ? *p : -1; o.h = h ? *h : -1;
        o.pr = p ? (long long)((BYTE*)p - b) : 0; o.hr = h ? (long long)((BYTE*)h - b) : 0;
        o.lp = (int)((*(BYTE**)(b + 0x10d0) - *(BYTE**)(b + 0x10c8)) / 8); o.lh = (int)((*(BYTE**)(b + 0x1038) - *(BYTE**)(b + 0x1030)) / 8);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// raw look at the memory behind a load pointer and which fields of the building point near it
static void HexAround(const char* tag, BYTE* b, int off) {
    __try {
        BYTE* p = *(BYTE**)(b + off); if (!p) return;
        char line[700]; int n = 0;
        for (int i = -0x40; i < 0x60; i += 4) {
            n += snprintf(line + n, sizeof(line) - n, "%s%g", i == 0 ? " [" : " ", *(float*)(p + i));
            if (i == 0) n += snprintf(line + n, sizeof(line) - n, "]");
        }
        Log("RAW %s +0x%x -> %p:%s", tag, off, p, line);
        n = 0;
        for (int i = -0x40; i < 0x60; i += 4) n += snprintf(line + n, sizeof(line) - n, " %08x", *(unsigned*)(p + i));
        Log("RAWX %s +0x%x:%s", tag, off, line);
        for (int k = 0; k < 0x1400; k += 8) {
            BYTE* q = *(BYTE**)(b + k);
            if (q && q != p && q > p - 0x400 && q < p + 0x400) Log("RAWREF %s building+0x%x -> %p (target %+lld)", tag, k, q, (long long)(p - q));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("RAW %s fault", tag); }
}
// the building's storage vector +0x970: records of 0xE0 bytes, capacity at +0x8c, resource code at +0x90
// resources a storage may hold: vector of 16-byte items at record+0 (item[0] = resource object); the power the code
// adds for a storage is max over its resources of res[code*32 + 0xd8], res[code*32 + 0xdc] (loop 0x119fc1)
static void SafeName(BYTE* r, char* out, int n) {
    __try {
        int k = 0;
        for (; k < n - 1; ++k) { char ch = (char)r[k]; if (ch < 32 || ch > 126) break; out[k] = ch; }
        out[k] = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { out[0] = 0; }
}
static void StorageRes(const char* tag, BYTE* rec) {
    __try {
        int code = *(int*)(rec + 0x90);
        BYTE* v = *(BYTE**)(rec); BYTE* e = *(BYTE**)(rec + 8);
        for (BYTE* it = v; it && it < e && it < v + 16 * 12; it += 16) {
            BYTE* res = *(BYTE**)it; if (!res) continue;
            char nm[40]; SafeName(res, nm, sizeof(nm));
            char nm8[40]; SafeName(res + 8, nm8, sizeof(nm8));
            Log("RES %s storage code %d: res %p '%s'/'%s' d8 %g dc %g item+8 %g", tag, code, res, nm, nm8,
                *(float*)(res + code * 32 + 0xd8), *(float*)(res + code * 32 + 0xdc), *(float*)(it + 8));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("RES %s fault", tag); }
}
static void StorageList(const char* tag, BYTE* b) {
    __try {
        BYTE* v = *(BYTE**)(b + 0x970); BYTE* e = *(BYTE**)(b + 0x978);
        char line[600]; int n = 0;
        for (BYTE* r = v; r && r < e && n < 560; r += 0xE0) n += snprintf(line + n, sizeof(line) - n, " [code %d cap %g]", *(int*)(r + 0x90), *(float*)(r + 0x8c));
        for (BYTE* r = v; r && r < e; r += 0xE0) StorageRes(tag, r);
        Log("STOR %s:%s", tag, n ? line : " none");
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("STOR %s fault", tag); }
}
static void DumpLoadNow() {
    BYTE** v = *(BYTE***)(gx::Base() + 0x9e6a18); BYTE** e = *(BYTE***)(gx::Base() + 0x9e6a20);
    struct Agg { int id = 0, n = 0; float p = 0, h = 0, pmin = 1e9f, pmax = -1e9f, hmin = 1e9f, hmax = -1e9f; long long pr = 0, hr = 0; int lp = 0, lh = 0; };
    std::map<std::string, Agg> m;
    for (BYTE** it = v; it && it < e; ++it) {
        BldRaw r; if (!*it || !ReadBld(*it, r)) continue;
        Agg& a = m[r.name]; a.id = r.id; ++a.n; a.p += r.p; a.h += r.h; a.pr = r.pr; a.hr = r.hr;
        a.pmin = std::min(a.pmin, r.p); a.pmax = std::max(a.pmax, r.p); a.hmin = std::min(a.hmin, r.h); a.hmax = std::max(a.hmax, r.h);
        a.lp = r.lp; a.lh = r.lh;
        if (a.n == 1 && (r.id == 2 || r.id == 0x12 || r.id == 0x47)) { HexAround(r.name, *it, 0x10f8); HexAround(r.name, *it, 0x1060); }
        if (a.n == 1) StorageList(r.name, *it);
    }
    Log("LOAD globals: darkness exe+0x9d5d84 = %g, living x2 flag exe+0x9d54d4 = %d", *(float*)(gx::Base() + 0x9d5d84), *(int*)(gx::Base() + 0x9d54d4));
    Log("LOAD dump: %d buildings, %d types (name id count | power +0x10f8 avg min max ptr-type | heat +0x1060 avg min max ptr-type | list sizes)", (int)(e - v), (int)m.size());
    for (auto& kv : m) {
        const Agg& a = kv.second;
        Log("LOAD %s id 0x%x n %d | P %.4g %.4g %.4g off %lld | H %.4g %.4g %.4g off %lld | lists %d %d", kv.first.c_str(), a.id, a.n,
            a.p / a.n, a.pmin, a.pmax, a.pr, a.h / a.n, a.hmin, a.hmax, a.hr, a.lp, a.lh);
    }
}
void SetStraightLines(bool on) { gx::g_straightLines = on; Log("straight lines %d", (int)on); }
void SetPathSnapMode(int m) { gx::g_pathSnapMode = m; Log("footpath snap mode %d", m); }
void DebugSetTool(const char* name) { strncpy(g_dbgTool, name, sizeof(g_dbgTool) - 1); g_dbgToolSet = true; }
void InstallPathLogging() { gx::InstallPathLogging(); }

static IatHook g_hooks[] = {
    { "?ViewSetPosition@C3D_CAMERA@@QEAAXVC3DVECTOR3@@@Z", h_ViewSetPosition, (void**)&o_ViewSetPosition },
    { "?CollisionTerrainHeightOverPoint@C3D_TERRAIN@@QEAAMVC3DVECTOR3@@@Z", h_TerrHeight, (void**)&o_TerrHeight },
    { "?CollisionRay@C3D_TERRAIN@@QEAA_NVC3DVECTOR3@@0PEAV2@_N2M@Z", h_TerrRay, (void**)&o_TerrRay },
    { "?GetMouseLeftPress@C3D_INPUT@@QEAA_NXZ", h_LPress, (void**)&o_LPress },
    { "?GetMouseLeftDown@C3D_INPUT@@QEAA_NXZ", h_LDown, (void**)&o_LDown },
    { "?GetMouseLeftPressAOff@C3D_INPUT@@QEAA_NXZ", h_LPressA, (void**)&o_LPressA },
    { "?GetMouseRightPress@C3D_INPUT@@QEAA_NXZ", h_RPress, (void**)&o_RPress },
    { "?GetMouseRightDown@C3D_INPUT@@QEAA_NXZ", h_RDown, (void**)&o_RDown },
    { "?GetMouseRightPressAOff@C3D_INPUT@@QEAA_NXZ", h_RPressA, (void**)&o_RPressA },
    { "?GetMouseMiddleDown@C3D_INPUT@@QEAA_NXZ", h_MDown, (void**)&o_MDown },
    { "?CommonInterfaceGetButtonDown@C3D_INPUT@@QEAA_NPEAD@Z", h_BtnDown, (void**)&o_BtnDown },
    { "?CommonInterfaceGetButtonPress@C3D_INPUT@@QEAA_NPEAD@Z", h_BtnPress, (void**)&o_BtnPress },
    { "?GetKeyDown@C3D_INPUT@@QEAA_NH@Z", h_KeyDown, (void**)&o_KeyDown },
    { "?GetKeyPress@C3D_INPUT@@QEAA_NH@Z", h_KeyPress, (void**)&o_KeyPress },
    { "?GetMouseSolid@C3D_INPUT@@QEAA?AVC3DVECTOR3@@XZ", h_MouseSolid, (void**)&o_MouseSolid },
    { "?GetMouseSolidWithInvZoom@C3D_INPUT@@QEAA?AVC3DVECTOR3@@XZ", h_MouseSolidInv, (void**)&o_MouseSolidInv },
    { "?CommonInterfaceGetAxisValue@C3D_INPUT@@QEAAMPEADPEAHMM@Z", h_AxisValue, (void**)&o_AxisValue },
    { "?EditToHeight@C3D_TERRAIN@@QEAAXVC3DVECTOR3@@0MMM_N@Z", h_EditToHeight, (void**)&o_EditToHeight },
    { "?EditorSetCursorSingle@C3D_BUILDING@@QEAAXHVC3DVECTOR3@@M_NM1@Z", h_SetCursorSingle, (void**)&o_SetCursorSingle },
    { "?GetRotationStepFromRotation@C3D_BUILDING@@QEAAHM@Z", h_RotStep, (void**)&o_RotStep },
    { "?GetRotationStepMatrix@C3D_BUILDING@@QEAA?AVC3DMATRIX@@H@Z", h_RotMat, (void**)&o_RotMat },
    { "?GetTypeIndex@C3D_BUILDING@@QEAAHPEAD@Z", h_TypeIdx, (void**)&o_TypeIdx },
    { "?GetTypeIndexNoLog@C3D_BUILDING@@QEAAHPEAD@Z", h_TypeIdxNoLog, (void**)&o_TypeIdxNoLog },
    { "?GetType@C3D_BUILDING@@QEAAPEAUTYPE@1@PEAD_N@Z", h_TypeByName, (void**)&o_TypeByName },
    { "?GetType@C3D_BUILDING@@QEAAPEAUTYPE@1@H@Z", h_TypeByIdx, (void**)&o_TypeByIdx },
    { "?AddCursorBuildings@C3D_BUILDING@@QEAAXXZ", h_AddCursor, (void**)&o_AddCursor },
    { "?EditorClearCursor@C3D_BUILDING@@QEAAXXZ", h_ClearCursor, (void**)&o_ClearCursor },
    { "?EditLevel@C3D_TERRAIN@@QEAAXVC3DVECTOR3@@MMM_N@Z", h_EdLevel, (void**)&o_EdLevel },
    { "?EditSmooth@C3D_TERRAIN@@QEAAXVC3DVECTOR3@@MMM_N@Z", h_EdSmooth, (void**)&o_EdSmooth },
    { "?EditRaiseLower@C3D_TERRAIN@@QEAAXVC3DVECTOR3@@MMMM_N@Z", h_EdRL, (void**)&o_EdRL },
    { "?EditRaiseLowerAngleLimit@C3D_TERRAIN@@QEAAXVC3DVECTOR3@@MMMM_NM@Z", h_EdRLA, (void**)&o_EdRLA },
    { "?EditRaiseLowerAngleLimitGetCost@C3D_TERRAIN@@QEAAMVC3DVECTOR3@@MMMM_N@Z", h_EdRLC, (void**)&o_EdRLC },
    { "?EditSmoothWithLimits@C3D_TERRAIN@@QEAAXVC3DVECTOR3@@MMMMMMMMM@Z", h_EdSWL, (void**)&o_EdSWL },
    { "?RegenerateCollisionDataSoviet@C3D_TERRAIN@@QEAAXXZ", h_RegenColl, (void**)&o_RegenColl },
    { "?RegenerateCollisionNormalDataSoviet@C3D_TERRAIN@@QEAAXXZ", h_RegenNorm, (void**)&o_RegenNorm },
    { "?GetMouseRelative@C3D_INPUT@@QEAA?AVC3DVECTOR3@@XZ", h_MouseRel, (void**)&o_MouseRel },
    { "?RenderSelection@C3D_BUILDING@@QEAAXXZ", h_BldRenderSel, (void**)&o_BldRenderSel },
    { "?Refresh@C3D_PATH@@QEAAXPEAVC3D_CAMERA@@PEAUENDNODEVISIBLE_LIST@@IMM@Z", h_PathRefresh, (void**)&o_PathRefresh },
};

void InstallIatHooks() {
    BYTE* base = (BYTE*)GetModuleHandleW(nullptr);
    auto* dos = (IMAGE_DOS_HEADER*)base;
    auto* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return;
    int done = 0;
    for (auto* imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); imp->Name; ++imp) {
        if (_stricmp((const char*)(base + imp->Name), "C3DDLL64.dll") != 0) continue;
        auto* names = (IMAGE_THUNK_DATA*)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        auto* funcs = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++funcs) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            const char* nm = ((IMAGE_IMPORT_BY_NAME*)(base + names->u1.AddressOfData))->Name;
            for (auto& h : g_hooks) {
                if (*h.orig || strcmp(nm, h.name) != 0) continue;
                DWORD old;
                if (VirtualProtect(&funcs->u1.Function, sizeof(void*), PAGE_READWRITE, &old)) {
                    *h.orig = (void*)funcs->u1.Function;
                    funcs->u1.Function = (ULONGLONG)h.hook;
                    VirtualProtect(&funcs->u1.Function, sizeof(void*), old, &old);
                    ++done;
                }
            }
        }
    }
    Log("IAT hooks installed: %d of %d", done, (int)(sizeof(g_hooks) / sizeof(g_hooks[0])));
    for (auto& h : g_hooks) if (!*h.orig) Log("  missing import: %s", h.name);
}

static M4 Mul(const M4& a, const M4& b) {
    M4 r{};
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j)
        r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
    return r;
}
static bool Invert(const M4& in, M4& out) {
    double a[4][8];
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) { a[i][j] = in.m[i][j]; a[i][j + 4] = (i == j); }
    for (int c = 0; c < 4; ++c) {
        int p = c;
        for (int r = c + 1; r < 4; ++r) if (fabs(a[r][c]) > fabs(a[p][c])) p = r;
        if (fabs(a[p][c]) < 1e-12) return false;
        if (p != c) for (int j = 0; j < 8; ++j) { double t = a[c][j]; a[c][j] = a[p][j]; a[p][j] = t; }
        double d = a[c][c];
        for (int j = 0; j < 8; ++j) a[c][j] /= d;
        for (int r = 0; r < 4; ++r) if (r != c) {
            double f = a[r][c];
            if (f != 0) for (int j = 0; j < 8; ++j) a[r][j] -= f * a[c][j];
        }
    }
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) out.m[i][j] = (float)a[i][j + 4];
    return true;
}

void BeginFrame() {
    if (g_abActive) { if (g_abMode) ScriptTick(); else AutoTick(); }
    { static DWORD last = 0; static int n = 0; static char prev[200] = ""; DWORD now = GetTickCount();
      if (now - last > 2000 && n < 150) { last = now;
        int a = g_nRotStep.exchange(0), b = g_nRotMat.exchange(0), c = g_nTypeIdx.exchange(0), d = g_nTypeName.exchange(0), e = g_nTypeByIdx.exchange(0);
        char key[200]; snprintf(key, sizeof(key), "rot %.4f step %d/%d type %d '%s' act %d%d%d%d%d", g_lastRot.load(), g_lastStepFromRot.load(), g_lastStep.load(), g_lastTypeIdx.load(), g_lastTypeName, a > 0, b > 0, c > 0, d > 0, e > 0);
        if (strcmp(key, prev) != 0) { ++n; strcpy(prev, key); Log("DIAG %s | calls rotStep %d rotMat %d typeIdx %d typeName %d typeByIdx %d", key, a, b, c, d, e); } } }
    g_matOk = false;
    if (!g_camera) return;
    if (!f_ViewGetMatrix) {
        HMODULE c3d = GetModuleHandleW(L"C3DDLL64.dll");
        if (!c3d) return;
        f_ViewGetMatrix = (CamMat_t)GetProcAddress(c3d, "?ViewGetMatrix@C3D_CAMERA@@QEAA?AVC3DMATRIX@@XZ");
        f_ProjGetMatrix = (CamMat_t)GetProcAddress(c3d, "?ProjGetMatrix@C3D_CAMERA@@QEAA?AVC3DMATRIX@@XZ");
        Log("ViewGetMatrix=%p ProjGetMatrix=%p", f_ViewGetMatrix, f_ProjGetMatrix);
    }
    if (!f_ViewGetMatrix || !f_ProjGetMatrix) return;
    // C3DMATRIX is returned by value through a hidden pointer; oversize the buffers to be safe.
    alignas(16) float vbuf[64] = {}, pbuf[64] = {};
    f_ViewGetMatrix(g_camera, vbuf);
    f_ProjGetMatrix(g_camera, pbuf);
    M4 v, p;
    memcpy(&v, vbuf, sizeof(M4));
    memcpy(&p, pbuf, sizeof(M4));
    g_viewProj = Mul(v, p);     // D3D row-vector convention: clip = world * View * Proj
    g_matOk = Invert(g_viewProj, g_invViewProj);
    static bool logged = false;
    if (!logged && g_terrain && p.m[0][0] != 0) {
        logged = true;
        Log("view row3: %.2f %.2f %.2f %.2f | proj diag: %.3f %.3f %.3f %.3f",
            v.m[3][0], v.m[3][1], v.m[3][2], v.m[3][3], p.m[0][0], p.m[1][1], p.m[2][2], p.m[3][3]);
    }
}

bool CameraSeen() { return g_camera != nullptr; }
bool Ready() { return g_camera && g_terrain && o_TerrHeight && g_matOk; }

static bool Transform(const M4& m, float x, float y, float z, float& ox, float& oy, float& oz, float& ow) {
    ox = x * m.m[0][0] + y * m.m[1][0] + z * m.m[2][0] + m.m[3][0];
    oy = x * m.m[0][1] + y * m.m[1][1] + z * m.m[2][1] + m.m[3][1];
    oz = x * m.m[0][2] + y * m.m[1][2] + z * m.m[2][2] + m.m[3][2];
    ow = x * m.m[0][3] + y * m.m[1][3] + z * m.m[2][3] + m.m[3][3];
    return true;
}

bool WorldToScreen(const V3& w, float sw, float sh, float& sx, float& sy) {
    if (!g_matOk) return false;
    float x, y, z, ww;
    Transform(g_viewProj, w.x, w.y, w.z, x, y, z, ww);
    if (ww < 0.05f) return false;
    sx = (x / ww * 0.5f + 0.5f) * sw;
    sy = (1.0f - (y / ww * 0.5f + 0.5f)) * sh;
    return true;
}

float TerrainHeight(float x, float z) {
    if (!g_terrain || !o_TerrHeight) return 0;
    V3 p{ x, 0, z };
    float h = o_TerrHeight(g_terrain, &p);
    return (h == h && fabs(h) < 1e6f) ? h : 0;
}

bool ScreenToTerrain(float sx, float sy, float sw, float sh, V3& out) {
    if (!Ready()) return false;
    float nx = sx / sw * 2 - 1, ny = 1 - sy / sh * 2;
    float x0, y0, z0, w0, x1, y1, z1, w1;
    Transform(g_invViewProj, nx, ny, 0.0f, x0, y0, z0, w0);
    Transform(g_invViewProj, nx, ny, 1.0f, x1, y1, z1, w1);
    if (fabs(w0) < 1e-9f || fabs(w1) < 1e-9f) return false;
    V3 o{ x0 / w0, y0 / w0, z0 / w0 }, e{ x1 / w1, y1 / w1, z1 / w1 };
    V3 d{ e.x - o.x, e.y - o.y, e.z - o.z };
    float len = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
    if (len < 1e-3f) return false;
    d.x /= len; d.y /= len; d.z /= len;
    // March along the ray until it goes under the terrain, then bisect.
    float maxT = len < 60000 ? len : 60000;
    float t = 0, prev = 0;
    while (t < maxT) {
        float step = t * 0.02f; if (step < 2) step = 2;
        prev = t; t += step;
        V3 p{ o.x + d.x * t, o.y + d.y * t, o.z + d.z * t };
        if (p.y <= TerrainHeight(p.x, p.z)) {
            float lo = prev, hi = t;
            for (int i = 0; i < 20; ++i) {
                float mid = (lo + hi) * 0.5f;
                V3 q{ o.x + d.x * mid, o.y + d.y * mid, o.z + d.z * mid };
                if (q.y <= TerrainHeight(q.x, q.z)) hi = mid; else lo = mid;
            }
            out = { o.x + d.x * hi, 0, o.z + d.z * hi };
            out.y = TerrainHeight(out.x, out.z);
            return true;
        }
    }
    return false;
}

} // namespace eng
