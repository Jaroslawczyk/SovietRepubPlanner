// Access to the game engine (C3DDLL64.dll) through IAT hooks in SOVIET64.exe.
#include "common.h"
#include <cmath>
#include <cstring>

namespace eng {

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
MB_HOOK(h_LPress, o_LPress) MB_HOOK(h_LDown, o_LDown) MB_HOOK(h_LPressA, o_LPressA)
MB_HOOK(h_RPress, o_RPress) MB_HOOK(h_RDown, o_RDown) MB_HOOK(h_RPressA, o_RPressA)
MB_HOOK(h_MDown, o_MDown)
static bool h_KeyDown(void* s, int k)  { return g_blockKeys ? false : o_KeyDown(s, k); }
static bool h_KeyPress(void* s, int k) { return g_blockKeys ? false : o_KeyPress(s, k); }
static V3* h_MouseRel(void* s, V3* out) {
    V3* r = o_MouseRel(s, out);
    if (g_blockMouseMove && r) { r->x = r->y = r->z = 0; }
    return r;
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
