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

#define MB_HOOK(name, orig) static bool name(void* s) { if (g_abActive) return false; return g_blockMouseButtons ? false : orig(s); }
static bool h_LPress(void* s) { if (g_abActive) return g_abClick; return g_blockMouseButtons ? false : o_LPress(s); }
static bool h_LDown(void* s) { static DWORD t = 0; if (t != GetCurrentThreadId()) { t = GetCurrentThreadId(); Log("input polled on thread %lu", t); } if (g_abActive) return g_abClick; return g_blockMouseButtons ? false : o_LDown(s); }   // polled every frame
static bool h_LPressA(void* s) { if (g_abActive) return g_abClick; return g_blockMouseButtons ? false : o_LPressA(s); }
MB_HOOK(h_RPress, o_RPress) MB_HOOK(h_RDown, o_RDown) MB_HOOK(h_RPressA, o_RPressA)
MB_HOOK(h_MDown, o_MDown)
static bool h_KeyDown(void* s, int k)  { return g_blockKeys ? false : o_KeyDown(s, k); }
static bool h_KeyPress(void* s, int k) { return g_blockKeys ? false : o_KeyPress(s, k); }
static V3* h_MouseRel(void* s, V3* out) {
    V3* r = o_MouseRel(s, out);
    // Zeroing this while the cursor is over the panel made the camera drift; only the auto-builder needs it.
    if (g_abActive && r) { r->x = r->y = r->z = 0; }
    return r;
}

// While the cursor is over our panel the game is told the mouse is far off-screen, so its own
// buttons and tooltips underneath do not react.
static V3* h_MouseSolid(void* s, V3* out) {
    V3* r = o_MouseSolid(s, out);
    { static DWORD last = 0; static int n = 0; DWORD now = GetTickCount();
      if (r && n < 40 && now - last > 1500) { ++n; last = now; POINT pt; GetCursorPos(&pt); Log("MOUSESOLID %.2f %.2f %.2f | screen cursor %ld %ld", r->x, r->y, r->z, pt.x, pt.y); } }
    if (g_abActive && r) { r->x = g_abSpoofX; r->y = g_abSpoofY; }
    return r;
}
static V3* h_MouseSolidInv(void* s, V3* out) {
    V3* r = o_MouseSolidInv(s, out);
    if (g_abActive && r) { r->x = g_abSpoofX; r->y = g_abSpoofY; }
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
void StartAutoBuild(const std::vector<V3>& pos, const std::vector<float>& rot, float sw, float sh) {
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
void StopAutoBuild() { g_abActive = false; g_abClick = false; g_abAxis = 0; }
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
    if (g_abActive) AutoTick();
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
