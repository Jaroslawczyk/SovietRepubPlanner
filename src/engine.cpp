// Access to the game engine (C3DDLL64.dll) through IAT hooks in SOVIET64.exe.
#include "common.h"
#include <cmath>
#include <cstring>
#include <mutex>
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
#define MB_HOOK(name, orig) static bool name(void* s) { return g_blockMouseButtons ? false : orig(s); }
MB_HOOK(h_LPress, o_LPress)
static bool h_LDown(void* s) { static DWORD t = 0; if (t != GetCurrentThreadId()) { t = GetCurrentThreadId(); Log("input polled on thread %lu", t); } return g_blockMouseButtons ? false : o_LDown(s); }   // polled every frame
 MB_HOOK(h_LPressA, o_LPressA)
MB_HOOK(h_RPress, o_RPress) MB_HOOK(h_RDown, o_RDown) MB_HOOK(h_RPressA, o_RPressA)
MB_HOOK(h_MDown, o_MDown)
static bool h_KeyDown(void* s, int k)  { return g_blockKeys ? false : o_KeyDown(s, k); }
static bool h_KeyPress(void* s, int k) { return g_blockKeys ? false : o_KeyPress(s, k); }
static V3* h_MouseRel(void* s, V3* out) {
    V3* r = o_MouseRel(s, out);
    if (g_blockMouseMove && r) { r->x = r->y = r->z = 0; }
    return r;
}

// While the cursor is over our panel the game is told the mouse is far off-screen, so its own
// buttons and tooltips underneath do not react.
static V3* h_MouseSolid(void* s, V3* out) {
    V3* r = o_MouseSolid(s, out);
    if (g_hideMouse && r) { r->x = -10000; r->y = -10000; }
    return r;
}
static V3* h_MouseSolidInv(void* s, V3* out) {
    V3* r = o_MouseSolidInv(s, out);
    if (g_hideMouse && r) { r->x = -10000; r->y = -10000; }
    return r;
}
static float h_AxisValue(void* s, char* name, int* a, float b, float c) {
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
static void h_SetCursorSingle(void* s, int type, V3* pos, float rot, bool b1, float f2, bool b2) {
    static int n = 0, lastType = -1;
    if (n < 40 && type != lastType) { ++n; lastType = type; Log("EditorSetCursorSingle this=%p type=%d pos=(%.1f %.1f %.1f) rot=%.3f b=%d f=%.3f b=%d", s, type, pos->x, pos->y, pos->z, rot, (int)b1, f2, (int)b2); }
    o_SetCursorSingle(s, type, pos, rot, b1, f2, b2);
}
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
// EXPERIMENTAL, not reliable yet. The game's flatten tool calls
// EditRaiseLowerAngleLimit(pos, 0, 1.7, c, 0.0012, false, 0.409) on a 5 m grid every frame with |c| <= 0.00004,
// which moves the ground about 2 cm per call (c * ~550 m). Calling it ourselves moves the ground once (about
// 0.6 m) and then stalls until some parameter changes; the reason is not understood. The heightmap texture
// must be open (HeightmapTextureOpen) or the engine crashes, and all of it must run on the render thread.
static std::mutex g_lvlMx;
static std::vector<V3> g_lvlPts;            // x, target height, z
static std::atomic<int> g_lvlLeft{ 0 };
static float g_lvlGain = 0.0002f;
static int g_lvlPass = 0;
static bool g_lvlRegen = false;
static int g_lvlStall = 0;
static bool g_lvlOpen = false;
static int g_lvlVar = 0;
static DWORD g_lvlLast = 0;

void QueueLevel(const std::vector<V3>& pts) {
    std::lock_guard<std::mutex> l(g_lvlMx);
    g_lvlPts = pts; g_lvlPass = 0; g_lvlGain = 0.0002f; g_lvlRegen = false; g_lvlStall = 0; g_lvlVar = 0;
    g_lvlLeft = (int)pts.size();
    Log("level: queued %d points", (int)pts.size());
}
int LevelPending() { return g_lvlLeft; }

static std::vector<float> g_lvlPrevH, g_lvlPrevStep;
static BldVoid_t f_HmOpen, f_HmClose;

// Called from the Present hook: the heightmap texture is mapped / unmapped on the D3D context, which is only
// safe on the render thread (doing it from the input-polling thread crashed the game).
void LevelTick() {
    if (!g_lvlLeft || !g_terrain || !o_EdRL || !o_TerrHeight) return;
    DWORD now = GetTickCount();
    if (now - g_lvlLast < 30) return;       // polled several times per frame
    g_lvlLast = now;
    std::lock_guard<std::mutex> l(g_lvlMx);
    if (!f_HmOpen) {
        HMODULE c3d = GetModuleHandleW(L"C3DDLL64.dll");
        f_HmOpen = (BldVoid_t)GetProcAddress(c3d, "?HeightmapTextureOpen@C3D_TERRAIN@@QEAAXXZ");
        f_HmClose = (BldVoid_t)GetProcAddress(c3d, "?HeightmapTextureClose@C3D_TERRAIN@@QEAAXXZ");
        Log("level: HeightmapTextureOpen=%p Close=%p thread %lu", f_HmOpen, f_HmClose, GetCurrentThreadId());
    }
    if (!f_HmOpen || !f_HmClose) { g_lvlLeft = 0; return; }
    size_t n = g_lvlPts.size();
    if (g_lvlPass == 0) { g_lvlPrevH.assign(n, 0); g_lvlPrevStep.assign(n, 0); }
    const float tol = 0.12f;
    // heights now show what the previous pass achieved; tune the gain from that
    std::vector<float> h(n);
    double want = 0, got = 0; int active = 0; float worst = 0;
    for (size_t i = 0; i < n; ++i) {
        V3 p{ g_lvlPts[i].x, 0, g_lvlPts[i].z };
        h[i] = o_TerrHeight(g_terrain, &p);
        float ps = g_lvlPrevStep[i];
        if (ps != 0) { want += fabsf(ps); got += (h[i] - g_lvlPrevH[i]) * (ps > 0 ? 1.0 : -1.0); }
        float d = fabsf(g_lvlPts[i].y - h[i]);
        if (d > worst) worst = d;
        if (d >= tol) ++active;
    }
    if (want > 0) {                          // aim at closing roughly 40 % of the gap per pass
        double ratio = got / want;
        if (ratio < 0.01) { ++g_lvlStall; } else g_lvlStall = 0;
        if (ratio < 0.01) {}
        else if (ratio < 0.25) g_lvlGain *= 1.5f;
        else if (ratio > 0.7) g_lvlGain *= 0.5f;
        if (g_lvlGain > 0.0008f) g_lvlGain = 0.0008f; else if (g_lvlGain < 0.00001f) g_lvlGain = 0.00001f;
    }
    if (g_lvlPass < 15 || g_lvlPass % 25 == 0)
        Log("level: pass %d, active %d, worst %.2f m, gain %.6f, moved %.3f of %.3f, variant %d", g_lvlPass, active, worst, g_lvlGain, got, want, g_lvlVar);
    if (!active || g_lvlPass >= 400 || g_lvlStall > 40) {
        Log("level: finished after %d passes, worst %.2f m", g_lvlPass, worst);
        if (g_lvlOpen) { f_HmClose(g_terrain); g_lvlOpen = false; }
        g_lvlLeft = 0; return;
    }
    if (!g_lvlOpen) { f_HmOpen(g_terrain); g_lvlOpen = true; }   // stays open for the whole job, like the game's own tool
    for (size_t i = 0; i < n; ++i) {
        float d = g_lvlPts[i].y - h[i];
        g_lvlPrevH[i] = h[i]; g_lvlPrevStep[i] = 0;
        if (fabsf(d) < tol) continue;
        float step = d > 1.5f ? 1.5f : d < -1.5f ? -1.5f : d;
        g_lvlPrevStep[i] = step;
        V3 q{ g_lvlPts[i].x, h[i], g_lvlPts[i].z };
        float c = step * g_lvlGain;
        if (c > 0.001f) c = 0.001f; else if (c < -0.001f) c = -0.001f;     // larger steps are ignored by the engine
        o_EdRL(g_terrain, &q, 0.0f, 1.7f, c, 0.0012f, false);
    }
    ++g_lvlPass;
    g_lvlLeft = active;
}

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
    { "?AddCursorBuildings@C3D_BUILDING@@QEAAXXZ", h_AddCursor, (void**)&o_AddCursor },
    { "?EditLevel@C3D_TERRAIN@@QEAAXVC3DVECTOR3@@MMM_N@Z", h_EdLevel, (void**)&o_EdLevel },
    { "?EditSmooth@C3D_TERRAIN@@QEAAXVC3DVECTOR3@@MMM_N@Z", h_EdSmooth, (void**)&o_EdSmooth },
    { "?EditRaiseLower@C3D_TERRAIN@@QEAAXVC3DVECTOR3@@MMMM_N@Z", h_EdRL, (void**)&o_EdRL },
    { "?EditRaiseLowerAngleLimit@C3D_TERRAIN@@QEAAXVC3DVECTOR3@@MMMM_NM@Z", h_EdRLA, (void**)&o_EdRLA },
    { "?EditRaiseLowerAngleLimitGetCost@C3D_TERRAIN@@QEAAMVC3DVECTOR3@@MMMM_N@Z", h_EdRLC, (void**)&o_EdRLC },
    { "?EditSmoothWithLimits@C3D_TERRAIN@@QEAAXVC3DVECTOR3@@MMMMMMMMM@Z", h_EdSWL, (void**)&o_EdSWL },
    { "?RegenerateCollisionDataSoviet@C3D_TERRAIN@@QEAAXXZ", h_RegenColl, (void**)&o_RegenColl },
    { "?RegenerateCollisionNormalDataSoviet@C3D_TERRAIN@@QEAAXXZ", h_RegenNorm, (void**)&o_RegenNorm },
    { "?GetMouseRelative@C3D_INPUT@@QEAA?AVC3DVECTOR3@@XZ", h_MouseRel, (void**)&o_MouseRel },
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
