// In-game panel: free-form territory outline, house / service / road settings, and the plan preview
// (slopes, levelling, street, stops, footpaths, buildings) drawn over the terrain.
#include "common.h"
#include "imgui.h"
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cctype>

namespace {

const float kPi = 3.14159265f;

bool g_open = false;
bool g_pick = false;                // drawing the outline on the map
std::vector<V3> g_draft;            // outline points clicked so tooFar

struct Area {
    bool ok = false;
    std::vector<V3> pts;            // world outline
    V3 o{}; float ux = 1, uz = 0, vx = 0, vz = 1;   // local frame
    std::vector<V2> local;
    float minU = 0, maxU = 0, minV = 0, maxV = 0, sqm = 0;
} g_area;
float g_angle = 0;                  // extra rotation of the building grid, degrees

std::vector<char> g_houseSel;       // per catalog index
std::vector<int> g_houseShare;      // per catalog index: weight of the type among the houses
int   g_roadKind = 1;               // street surface for instant building: 0 gravel, 1 asphalt
int   g_pathKind = 1;               // footpath: 0 dirt trail, 1 gravel, 2 asphalt
std::vector<ServiceRule> g_services = {
    { "SHOP",         "Магазин",          true,  2000, -1 },
    { "PUB",          "Пивная",           true,  3000, -1 },
    { "KINDERGARTEN", "Детский сад",      true,  2500, -1 },
    { "SCHOOL",       "Школа",            true,  3000, -1 },
    { "KINO",         "Кинотеатр",        true,  6000, -1 },
    { "SPORT",        "Спорт",            true,  6000, -1 },
    { "MONUMENT",     "Памятник",         true,  3000, -1 },
    { "ATTRACTION",   "Парк / площадка",  true,  2000, -1 },
    { "HOSPITAL",     "Больница",         false, 10000, -1 },
    { "CHURCH",       "Церковь",          false, 8000, -1 },
};
int   g_population = 1000;
float g_gap = 8.0f;
int   g_roadType = 1;
float g_roadV = 0;
std::vector<float> g_stops;
float g_walk = 400.0f, g_maxDiff = 5.0f;
// utilities: water, sewage, heating, power
const char* const kUtilName[4] = { "Вода", "Канализация", "Отопление", "Электричество" };
const char* const kUtilNode[4] = { "водораспределитель", "канализационная станция", "тепловой пункт", "трансформатор" };
const char* const kUtilUnit[4] = { "м³/сут", "м³/сут", "кВт", "кВт" };
const ImU32 kUtilCol[4] = { IM_COL32(70, 150, 255, 255), IM_COL32(165, 110, 55, 255), IM_COL32(245, 85, 60, 255), IM_COL32(250, 220, 60, 255) };
bool  g_utilShow[4] = { false, false, false, false };
bool  g_utilBuild[4] = { false, false, false, false };   // the network goes into the plan and gets built
bool  g_powerUnder = true;                                // medium-voltage lines as underground cable
float g_utilRadius[4] = { 200, 200, 250, 150 };
float g_utilNorm[4] = { 0.12f, 0.12f, 0.9f, 0.25f };   // per resident; rough guesses, editable in the panel
std::vector<std::vector<V3>> g_utilLines[4];
std::vector<V3> g_utilPts[4];
float g_buildRotSign = -1.0f;       // sign between the plan angle and the game cursor rotation
float g_streetGap = 250.0f;        // parallel streets every N metres, 0 = off
bool  g_showSlope = true, g_showLevel = false;
bool  g_dirty = true, g_areaChanged = false, g_inited = false;
char  g_filter[64] = "";
int   g_dragStop = -1, g_hoverStop = -1;

PlanResult g_plan;
struct Quad { V3 g[4]; V3 mid; float pad, h; int type; bool tooFar; };
struct Cell { V3 c[4]; ImU32 col; };
std::vector<Quad> g_quads;
std::vector<Cell> g_slope, g_roadQuads;
std::vector<std::vector<V3>> g_lines, g_outline;
std::vector<V3> g_stopPts, g_crossPts, g_pedPts, g_roadPts;
std::vector<std::vector<V3>> g_driveLines;
int   g_lineMode = 2;               // unused: the game only accepts press-drag-release               // how the game's line tool is driven: 0 two clicks a piece, 1 chain, 2 hold and drag

// ---------- geometry helpers ----------
V3 LocalToWorld(const Area& a, float u, float v, float lift = 0.3f) {
    V3 p{ a.o.x + a.ux * u + a.vx * v, 0, a.o.z + a.uz * u + a.vz * v };
    p.y = eng::TerrainHeight(p.x, p.z) + lift;
    return p;
}
V2 WorldToLocal(const Area& a, const V3& p) {
    float dx = p.x - a.o.x, dz = p.z - a.o.z;
    return { dx * a.ux + dz * a.uz, dx * a.vx + dz * a.vz };
}
std::vector<V3> Sample(const Area& a, float u1, float v1, float u2, float v2, float step = 10.0f) {
    float len = sqrtf((u2 - u1) * (u2 - u1) + (v2 - v1) * (v2 - v1));
    int n = std::max(1, (int)(len / step));
    std::vector<V3> pts;
    for (int i = 0; i <= n; ++i) { float t = (float)i / n; pts.push_back(LocalToWorld(a, u1 + (u2 - u1) * t, v1 + (v2 - v1) * t)); }
    return pts;
}

// Local frame along the longest edge (plus the user's rotation); also resets the street to the middle.
void BuildFrame(Area& a, bool resetRoad) {
    size_t n = a.pts.size(), best = 0; float bestLen = -1;
    for (size_t i = 0; i < n; ++i) {
        const V3& p = a.pts[i]; const V3& q = a.pts[(i + 1) % n];
        float len = (q.x - p.x) * (q.x - p.x) + (q.z - p.z) * (q.z - p.z);
        if (len > bestLen) { bestLen = len; best = i; }
    }
    const V3& p = a.pts[best]; const V3& q = a.pts[(best + 1) % n];
    float ang = atan2f(q.z - p.z, q.x - p.x) + g_angle * kPi / 180.0f;
    a.ux = cosf(ang); a.uz = sinf(ang); a.vx = -a.uz; a.vz = a.ux; a.o = a.pts[0];
    a.local.clear();
    a.minU = a.minV = 1e9f; a.maxU = a.maxV = -1e9f;
    for (auto& w : a.pts) {
        V2 l = WorldToLocal(a, w); a.local.push_back(l);
        a.minU = std::min(a.minU, l.u); a.maxU = std::max(a.maxU, l.u); a.minV = std::min(a.minV, l.v); a.maxV = std::max(a.maxV, l.v);
    }
    float s = 0;
    for (size_t i = 0; i < n; ++i) { const V2& x = a.local[i]; const V2& y = a.local[(i + 1) % n]; s += x.u * y.v - y.u * x.v; }
    a.sqm = fabsf(s) / 2;
    if (resetRoad) {
        g_roadV = (a.minV + a.maxV) / 2;
        float L, R;
        g_stops.assign(1, PolyScan(a.local, g_roadV, L, R) ? (L + R) / 2 : (a.minU + a.maxU) / 2);
    }
}

void SetArea(const std::vector<V3>& pts) {
    g_area = Area(); g_area.pts = pts; g_area.ok = true;
    BuildFrame(g_area, true);
    g_dirty = g_areaChanged = true;
}

void InitDefaults() {
    const auto& all = catalog::All();
    g_houseSel.assign(all.size(), 0);
    g_houseShare.assign(all.size(), 50);
    bool any = false;
    for (size_t i = 0; i < all.size(); ++i) if (all[i].id == "hrusevka") { g_houseSel[i] = 1; any = true; }
    for (size_t i = 0; i < all.size() && !any; ++i) if (all[i].category == "LIVING" && all[i].capacity > 0) { g_houseSel[i] = 1; any = true; }
    for (auto& s : g_services) {
        float best = 1e9f;
        for (size_t i = 0; i < all.size(); ++i) {
            if (all[i].category != s.category) continue;
            float area = all[i].sx * all[i].sz;
            if (area < best) { best = area; s.typeIndex = (int)i; }
        }
        if (s.typeIndex < 0) s.enabled = false;
    }
    g_inited = true;
}

// Steep ground inside the outline: orange above 12 %, red above 25 %.
void RebuildSlope() {
    g_slope.clear();
    if (!g_area.ok) return;
    const Area& a = g_area;
    float step = std::clamp(sqrtf((a.maxU - a.minU) * (a.maxV - a.minV)) / 55.0f, 6.0f, 30.0f);
    for (float v = a.minV; v < a.maxV; v += step)
        for (float u = a.minU; u < a.maxU; u += step) {
            if (!PolyContains(a.local, u + step / 2, v + step / 2)) continue;
            Cell c;
            c.c[0] = LocalToWorld(a, u, v, 0.2f); c.c[1] = LocalToWorld(a, u + step, v, 0.2f);
            c.c[2] = LocalToWorld(a, u + step, v + step, 0.2f); c.c[3] = LocalToWorld(a, u, v + step, 0.2f);
            float lo = 1e9f, hi = -1e9f;
            for (auto& p : c.c) { lo = std::min(lo, p.y); hi = std::max(hi, p.y); }
            V3 mid = LocalToWorld(a, u + step / 2, v + step / 2, 0.2f);
            if (eng::Occupied(mid.x, mid.z)) { c.col = IM_COL32(150, 60, 220, 120); g_slope.push_back(c); continue; }   // existing buildings
            float slope = (hi - lo) / step;
            if (slope < 0.12f) continue;
            c.col = slope > 0.25f ? IM_COL32(230, 40, 30, 95) : IM_COL32(255, 150, 30, 70);
            g_slope.push_back(c);
        }
}

void Regenerate() {
    static bool scanned = false;
    if (!scanned && eng::OccupancyReady()) {    // diagnostic: how much of the map the engine reports as built-up
        scanned = true;
        int b = 0, p = 0, shown = 0;
        for (float z = -10000; z < 10000; z += 50) for (float x = -10000; x < 10000; x += 50) {
            int o = eng::Occupied(x, z);
            if (o & 1) { ++b; if (shown < 6) { ++shown; Log("occupied by a building: %.0f %.0f", x, z); } }
            if (o & 2) ++p;
        }
        Log("occupancy scan (50 m grid): %d building hits, %d path hits of 160000", b, p);
    }
    g_dirty = false;
    g_quads.clear(); g_lines.clear(); g_outline.clear(); g_roadQuads.clear(); g_stopPts.clear(); g_crossPts.clear();
    g_pedPts.clear(); g_roadPts.clear(); g_driveLines.clear();
    g_plan = PlanResult();
    if (g_areaChanged) { g_areaChanged = false; RebuildSlope(); }
    if (!g_area.ok) return;
    const Area& a = g_area;
    PlanInput in;
    in.poly = a.local; in.population = g_population; in.gap = g_gap;
    for (size_t i = 0; i < g_houseSel.size(); ++i) if (g_houseSel[i]) { in.houseTypes.push_back((int)i); in.houseWeights.push_back((float)g_houseShare[i]); }
    in.services = &g_services;
    in.roadType = g_roadType; in.roadV = g_roadV; in.stopsU = g_stops; in.walkRadius = g_walk; in.maxDiff = g_maxDiff; in.streetSpacing = g_streetGap;
    for (int k = 0; k < 4; ++k) { in.utilRadius[k] = g_utilRadius[k]; in.utilBuild[k] = g_utilBuild[k]; }
    in.occupied = [&a](float u, float v) { return eng::Occupied(a.o.x + a.ux * u + a.vx * v, a.o.z + a.uz * u + a.vz * v) != 0; };
    in.height = [&a](float u, float v) { return eng::TerrainHeight(a.o.x + a.ux * u + a.vx * v, a.o.z + a.uz * u + a.vz * v); };
    g_plan = MakePlan(in);

    const auto& all = catalog::All();
    for (auto& b : g_plan.buildings) {
        Quad q; q.type = b.type; q.pad = b.pad; q.h = std::max(3.0f, all[b.type].height); q.tooFar = b.tooFar;
        float hw = b.w / 2, hd = b.d / 2;
        q.g[0] = LocalToWorld(a, b.u - hw, b.v - hd); q.g[1] = LocalToWorld(a, b.u + hw, b.v - hd);
        q.g[2] = LocalToWorld(a, b.u + hw, b.v + hd); q.g[3] = LocalToWorld(a, b.u - hw, b.v + hd);
        q.mid = LocalToWorld(a, b.u, b.v);
        g_quads.push_back(q);
    }
    for (auto& p : g_plan.paths) g_lines.push_back(Sample(a, p.u1, p.v1, p.u2, p.v2));
    for (auto& rd : g_plan.roads) {
        float len = fabsf(rd.u2 - rd.u1); int n = std::max(1, (int)(len / 8.0f));
        for (int i = 0; i < n; ++i) {
            float u0 = rd.u1 + (rd.u2 - rd.u1) * i / n, u1 = rd.u1 + (rd.u2 - rd.u1) * (i + 1) / n;
            Cell c; c.col = IM_COL32(70, 70, 78, 200);
            c.c[0] = LocalToWorld(a, u0, rd.v1 - rd.width / 2); c.c[1] = LocalToWorld(a, u1, rd.v1 - rd.width / 2);
            c.c[2] = LocalToWorld(a, u1, rd.v1 + rd.width / 2); c.c[3] = LocalToWorld(a, u0, rd.v1 + rd.width / 2);
            g_roadQuads.push_back(c);
        }
        g_lines.push_back(Sample(a, rd.u1, rd.v1, rd.u2, rd.v2));       // centre line
    }
    for (int k = 0; k < 4; ++k) {
        g_utilLines[k].clear(); g_utilPts[k].clear();
        for (auto& s : g_plan.utilLines) if (s.net == k) g_utilLines[k].push_back(Sample(a, s.u1, s.v1, s.u2, s.v2));
        for (auto& nd : g_plan.utilNodes[k]) g_utilPts[k].push_back(LocalToWorld(a, nd.u, nd.v, 1.0f));
    }
    for (auto& p : g_plan.drives) g_driveLines.push_back(Sample(a, p.u1, p.v1, p.u2, p.v2, 5.0f));
    for (auto& s : g_plan.pedPts) g_pedPts.push_back(LocalToWorld(a, s.u, s.v, 0.5f));
    for (auto& s : g_plan.roadPts) g_roadPts.push_back(LocalToWorld(a, s.u, s.v, 0.5f));
    for (auto& s : g_plan.stops) g_stopPts.push_back(LocalToWorld(a, s.u, s.v, 1.0f));
    for (auto& s : g_plan.crossings) g_crossPts.push_back(LocalToWorld(a, s.u, s.v, 0.6f));
    size_t n = a.local.size();
    for (size_t i = 0; i < n; ++i) g_outline.push_back(Sample(a, a.local[i].u, a.local[i].v, a.local[(i + 1) % n].u, a.local[(i + 1) % n].v));
}

// ---------- world drawing ----------
ImU32 CategoryColor(const std::string& c, int alpha) {
    if (c == "LIVING")     return IM_COL32(90, 160, 255, alpha);
    if (c == "SHOP")       return IM_COL32(255, 170, 40, alpha);
    if (c == "PUB")        return IM_COL32(200, 120, 60, alpha);
    if (c == "KINDERGARTEN" || c == "SCHOOL") return IM_COL32(120, 220, 120, alpha);
    if (c == "KINO" || c == "SPORT")          return IM_COL32(200, 110, 230, alpha);
    if (c == "MONUMENT" || c == "ATTRACTION") return IM_COL32(60, 200, 190, alpha);
    if (c == "HOSPITAL")   return IM_COL32(240, 80, 80, alpha);
    return IM_COL32(200, 200, 200, alpha);
}
bool Proj(const V3& w, ImVec2& s) {
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    return eng::WorldToScreen(w, ds.x, ds.y, s.x, s.y);
}
void DrawPolyline(ImDrawList* dl, const std::vector<V3>& pts, ImU32 col, float thick) {
    bool have = false; ImVec2 prev, s;
    for (auto& p : pts) {
        if (!Proj(p, s)) { have = false; continue; }
        if (have) dl->AddLine(prev, s, col, thick);
        prev = s; have = true;
    }
}
void DrawCells(ImDrawList* dl, const std::vector<Cell>& cells) {
    for (auto& c : cells) {
        ImVec2 s[4]; bool ok = true;
        for (int i = 0; i < 4 && ok; ++i) ok = Proj(c.c[i], s[i]);
        if (ok) dl->AddQuadFilled(s[0], s[1], s[2], s[3], c.col);
    }
}

void DrawBuilding(ImDrawList* dl, const Quad& q) {
    const BuildingType& b = catalog::All()[q.type];
    ImVec2 g[4]; bool ok = true;
    for (int i = 0; i < 4 && ok; ++i) ok = Proj(q.g[i], g[i]);
    if (!ok) return;
    ImU32 line = q.tooFar ? IM_COL32(255, 60, 50, 255) : CategoryColor(b.category, 255);
    float lineW = q.tooFar ? 2.5f : 1.5f;
    ImVec2 labelAt; bool haveLabel = Proj(q.mid, labelAt);
    float minX = std::min(std::min(g[0].x, g[1].x), std::min(g[2].x, g[3].x));
    float maxX = std::max(std::max(g[0].x, g[1].x), std::max(g[2].x, g[3].x));

    if (g_showLevel) {      // translucent box standing on the levelled pad, yellow = what happens to the ground
        ImVec2 lo[4], hi[4];
        for (int i = 0; i < 4 && ok; ++i) {
            ok = Proj(V3{ q.g[i].x, q.pad, q.g[i].z }, lo[i]) && Proj(V3{ q.g[i].x, q.pad + q.h, q.g[i].z }, hi[i]);
        }
        if (!ok) return;
        for (int i = 0; i < 4; ++i) {
            int j = (i + 1) % 4;
            dl->AddQuadFilled(lo[i], lo[j], hi[j], hi[i], CategoryColor(b.category, 60));
            dl->AddLine(lo[i], hi[i], line, 1.0f);
        }
        dl->AddQuadFilled(hi[0], hi[1], hi[2], hi[3], CategoryColor(b.category, 85));
        dl->AddQuad(hi[0], hi[1], hi[2], hi[3], line, lineW);
        const ImU32 yellow = IM_COL32(255, 225, 40, 255);
        dl->AddQuad(lo[0], lo[1], lo[2], lo[3], yellow, 2.0f);                 // the levelled pad
        for (int i = 0; i < 4; ++i) dl->AddLine(lo[i], g[i], yellow, 2.0f);    // cut (ground above) or fill (ground below)
        haveLabel = haveLabel && Proj(V3{ q.mid.x, q.pad + q.h, q.mid.z }, labelAt);
    } else {
        dl->AddQuadFilled(g[0], g[1], g[2], g[3], CategoryColor(b.category, 90));
        dl->AddQuad(g[0], g[1], g[2], g[3], line, lineW);
    }
    ImVec2 ts = ImGui::CalcTextSize(b.name.c_str());
    if (haveLabel && ts.x < (maxX - minX)) {
        dl->AddText(ImVec2(labelAt.x - ts.x / 2 + 1, labelAt.y - ts.y / 2 + 1), IM_COL32(0, 0, 0, 220), b.name.c_str());
        dl->AddText(ImVec2(labelAt.x - ts.x / 2, labelAt.y - ts.y / 2), IM_COL32(255, 255, 255, 255), b.name.c_str());
    }
}

void DrawWorld() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (g_showSlope) DrawCells(dl, g_slope);
    for (auto& l : g_outline) DrawPolyline(dl, l, IM_COL32(255, 235, 80, 230), 2.5f);
    DrawCells(dl, g_roadQuads);
    for (auto& l : g_lines) DrawPolyline(dl, l, IM_COL32(235, 235, 235, 220), 2.0f);
    for (int k = 0; k < 4; ++k) {
        if (!g_utilShow[k]) continue;
        for (auto& l : g_utilLines[k]) DrawPolyline(dl, l, kUtilCol[k], 2.0f);
        for (auto& p : g_utilPts[k]) {
            ImVec2 sp;
            if (!Proj(p, sp)) continue;
            dl->AddRectFilled(ImVec2(sp.x - 7, sp.y - 7), ImVec2(sp.x + 7, sp.y + 7), kUtilCol[k], 2.0f);
            dl->AddRect(ImVec2(sp.x - 7, sp.y - 7), ImVec2(sp.x + 7, sp.y + 7), IM_COL32(30, 30, 30, 255), 2.0f, 0, 1.5f);
        }
    }
    for (auto& l : g_driveLines) DrawPolyline(dl, l, IM_COL32(70, 70, 78, 235), 5.0f);
    for (auto& q : g_quads) DrawBuilding(dl, q);
    ImVec2 s;
    for (auto& c : g_pedPts) if (Proj(c, s)) {          // footpath entrance of a building
        dl->AddCircleFilled(s, 4.5f, IM_COL32(60, 200, 90, 255)); dl->AddCircle(s, 4.5f, IM_COL32(20, 20, 20, 255), 0, 1.2f);
    }
    for (auto& c : g_roadPts) if (Proj(c, s)) {         // road connection of a building
        dl->AddRectFilled(ImVec2(s.x - 5, s.y - 5), ImVec2(s.x + 5, s.y + 5), IM_COL32(255, 150, 40, 255));
        dl->AddRect(ImVec2(s.x - 5, s.y - 5), ImVec2(s.x + 5, s.y + 5), IM_COL32(20, 20, 20, 255), 0, 0, 1.2f);
    }
    for (auto& c : g_crossPts) if (Proj(c, s)) {       // pedestrian crossing
        for (int k = -2; k <= 2; ++k) dl->AddLine(ImVec2(s.x + k * 4.0f, s.y - 6), ImVec2(s.x + k * 4.0f, s.y + 6), IM_COL32(255, 255, 255, 255), 2.0f);
    }
    for (size_t i = 0; i < g_stopPts.size(); ++i) if (Proj(g_stopPts[i], s)) {
        bool hot = (int)i == g_hoverStop || (int)i == g_dragStop;
        dl->AddCircleFilled(s, hot ? 13.0f : 10.0f, IM_COL32(40, 110, 220, 255));
        dl->AddCircle(s, hot ? 13.0f : 10.0f, IM_COL32(255, 255, 255, 255), 0, 2.0f);
        const char* t = "Остановка";
        ImVec2 ts = ImGui::CalcTextSize(t);
        dl->AddRectFilled(ImVec2(s.x - ts.x / 2 - 4, s.y - 34), ImVec2(s.x + ts.x / 2 + 4, s.y - 14), IM_COL32(0, 0, 0, 170), 4);
        dl->AddText(ImVec2(s.x - ts.x / 2, s.y - 33), IM_COL32(255, 255, 255, 255), t);
    }
}

// Drag a stop on the map: the street follows it and the district is rebuilt around the new place.
void StopsInteraction() {
    ImGuiIO& io = ImGui::GetIO();
    if (g_dragStop >= 0) {
        if (!ImGui::IsMouseDown(0) || g_dragStop >= (int)g_stops.size()) { g_dragStop = -1; return; }
        V3 hit;
        if (eng::ScreenToTerrain(io.MousePos.x, io.MousePos.y, io.DisplaySize.x, io.DisplaySize.y, hit)) {
            V2 l = WorldToLocal(g_area, hit);
            g_stops[g_dragStop] = l.u;
            g_roadV = std::clamp(l.v, g_area.minV + 12, g_area.maxV - 12);
            g_dirty = true;
        }
        return;
    }
    g_hoverStop = -1;
    if (io.WantCaptureMouse) return;
    ImVec2 s;
    for (size_t i = 0; i < g_stopPts.size(); ++i)
        if (Proj(g_stopPts[i], s) && fabsf(s.x - io.MousePos.x) < 16 && fabsf(s.y - io.MousePos.y) < 16) g_hoverStop = (int)i;
    if (g_hoverStop >= 0) {
        ImGui::SetTooltip("Перетащите остановку — район перестроится");
        if (ImGui::IsMouseClicked(0)) g_dragStop = g_hoverStop;
    }
}

// Outline drawing: every click adds a corner; click the first corner (or press Enter) to close.
void Picking() {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImU32 yellow = IM_COL32(255, 235, 80, 255);
    if (ImGui::IsMouseClicked(1)) {
        if (g_draft.empty()) { g_pick = false; return; }
        g_draft.pop_back();
    }
    V3 hit;
    bool ok = !io.WantCaptureMouse && eng::ScreenToTerrain(io.MousePos.x, io.MousePos.y, io.DisplaySize.x, io.DisplaySize.y, hit);
    if (ok) hit.y += 0.3f;

    std::vector<V3> line;
    for (size_t i = 0; i < g_draft.size(); ++i) {
        const V3& p = g_draft[i]; const V3& q = (i + 1 < g_draft.size()) ? g_draft[i + 1] : hit;
        if (i + 1 == g_draft.size() && !ok) break;
        int n = std::max(1, (int)(sqrtf((q.x - p.x) * (q.x - p.x) + (q.z - p.z) * (q.z - p.z)) / 10.0f));
        for (int k = 0; k <= n; ++k) {
            V3 w{ p.x + (q.x - p.x) * k / n, 0, p.z + (q.z - p.z) * k / n };
            w.y = eng::TerrainHeight(w.x, w.z) + 0.3f;
            line.push_back(w);
        }
    }
    DrawPolyline(dl, line, yellow, 2.5f);
    bool nearFirst = false; ImVec2 s;
    for (size_t i = 0; i < g_draft.size(); ++i) if (Proj(g_draft[i], s)) {
        bool first = i == 0;
        if (first && g_draft.size() >= 3 && fabsf(s.x - io.MousePos.x) < 16 && fabsf(s.y - io.MousePos.y) < 16) nearFirst = true;
        dl->AddCircleFilled(s, first ? (nearFirst ? 10.0f : 7.0f) : 4.5f, first ? IM_COL32(255, 120, 40, 255) : yellow);
    }

    char buf[200];
    if (g_draft.empty()) snprintf(buf, sizeof(buf), "Клик — первая точка контура   (ПКМ — отмена)");
    else if (g_draft.size() < 3) snprintf(buf, sizeof(buf), "Клик — следующая точка   (ПКМ — убрать последнюю)");
    else snprintf(buf, sizeof(buf), nearFirst ? "Клик — замкнуть контур" : "Клик — точка, клик по первой точке или Enter — замкнуть   (ПКМ — убрать последнюю)");
    ImVec2 tp(io.MousePos.x + 18, io.MousePos.y + 18), ts = ImGui::CalcTextSize(buf);
    dl->AddRectFilled(ImVec2(tp.x - 5, tp.y - 3), ImVec2(tp.x + ts.x + 5, tp.y + ts.y + 3), IM_COL32(0, 0, 0, 190), 4);
    dl->AddText(tp, IM_COL32(255, 255, 255, 255), buf);

    bool close = g_draft.size() >= 3 && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || (nearFirst && ImGui::IsMouseClicked(0)));
    if (close) { SetArea(g_draft); g_draft.clear(); g_pick = false; return; }
    if (ok && ImGui::IsMouseClicked(0) && g_draft.size() < 64) g_draft.push_back(hit);
}

// ---------- panel ----------
const char* MaterialName(const std::string& m) {
    if (m == "brick") return "кирпич";
    if (m == "panels") return "панели";
    if (m == "wood") return "дерево";
    if (m == "concrete") return "бетон";
    if (m == "steel") return "сталь";
    return m.empty() ? "-" : m.c_str();
}

std::vector<int> g_houseOrder;      // catalog indices of residential buildings in display order
bool g_onlySelected = false;

void SortHouses(ImGuiTableSortSpecs* specs) {
    const auto& all = catalog::All();
    if (g_houseOrder.empty())
        for (size_t i = 0; i < all.size(); ++i) if (all[i].category == "LIVING" && all[i].capacity > 0) g_houseOrder.push_back((int)i);
    if (!specs || specs->SpecsCount == 0) return;
    int col = specs->Specs[0].ColumnIndex;
    bool asc = specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
    std::stable_sort(g_houseOrder.begin(), g_houseOrder.end(), [&](int x, int y) {
        const BuildingType& a = all[asc ? x : y]; const BuildingType& b = all[asc ? y : x];
        switch (col) {
            case 1: return a.capacity < b.capacity;
            case 2: return a.sx * a.sz < b.sx * b.sz;
            case 3: return a.quality < b.quality;
            case 4: return a.material < b.material;
            case 5: return a.style < b.style;
            default: return a.name < b.name;
        }
    });
}

void TabArea() {
    ImGui::SeparatorText("Территория");
    ImGui::BeginDisabled(!eng::Ready());
    if (ImGui::Button(g_pick ? "Отменить рисование" : "Нарисовать на карте", ImVec2(210, 0))) { g_pick = !g_pick; g_draft.clear(); }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (g_area.ok) {
        ImGui::Text("%.1f га, %d точек", g_area.sqm / 10000.0f, (int)g_area.pts.size());
        ImGui::SameLine();
        if (ImGui::SmallButton("Сбросить")) { g_area.ok = false; g_dirty = g_areaChanged = true; }
    } else ImGui::TextDisabled("участок не выбран");
    ImGui::TextDisabled("Кликайте по карте углы района любой формы, замкните кликом по первой точке.");
    ImGui::SetNextItemWidth(220);
    if (ImGui::SliderFloat("поворот застройки, °", &g_angle, -90.0f, 90.0f, "%.0f") && g_area.ok) {
        BuildFrame(g_area, true); g_dirty = g_areaChanged = true;
    }

    ImGui::SeparatorText("Рельеф");
    ImGui::Checkbox("показывать крутые склоны (оранжевый > 12 %, красный > 25 %)", &g_showSlope);
    ImGui::Checkbox("показывать выравнивание: объёмные здания и жёлтые линии среза / насыпи", &g_showLevel);
    ImGui::SetNextItemWidth(220);
    if (ImGui::SliderFloat("допустимый перепад под зданием, м", &g_maxDiff, 0.5f, 12.0f, "%.1f")) g_dirty = true;

    ImGui::SeparatorText("Население");
    ImGui::SetNextItemWidth(220);
    if (ImGui::InputInt("жителей", &g_population, 100, 1000)) { g_population = std::clamp(g_population, 10, 200000); g_dirty = true; }
    ImGui::SetNextItemWidth(220);
    if (ImGui::SliderFloat("отступ между зданиями, м", &g_gap, 3.0f, 30.0f, "%.0f")) g_dirty = true;

    ImGui::SeparatorText("Выбранные дома");
    const auto& all = catalog::All();
    int n = 0;
    for (size_t i = 0; i < g_houseSel.size(); ++i) if (g_houseSel[i]) {
        ImGui::BulletText("%s — %d жит.", all[i].name.c_str(), all[i].capacity); ++n;
    }
    if (!n) ImGui::TextColored(ImVec4(0.65f, 0.1f, 0.1f, 1), "Не выбрано ни одного дома — откройте вкладку «Дома».");
}

void TabHouses() {
    const auto& all = catalog::All();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##filter", "поиск по названию", g_filter, sizeof(g_filter));
    ImGui::SameLine(); ImGui::Checkbox("только выбранные", &g_onlySelected);
    ImGui::SameLine();
    if (ImGui::SmallButton("Снять все")) { std::fill(g_houseSel.begin(), g_houseSel.end(), 0); g_dirty = true; }
    const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                  ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable;
    int shareSum = 0;
    for (size_t i = 0; i < g_houseSel.size(); ++i) if (g_houseSel[i]) shareSum += std::max(0, g_houseShare[i]);
    if (ImGui::BeginTable("houses", 7, flags, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Дом", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Доля", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 112);
        ImGui::TableSetupColumn("Жителей", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending, 70);
        ImGui::TableSetupColumn("Размер, м", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Кач.", ImGuiTableColumnFlags_WidthFixed, 46);
        ImGui::TableSetupColumn("Материал", ImGuiTableColumnFlags_WidthFixed, 78);
        ImGui::TableSetupColumn("Стиль", ImGuiTableColumnFlags_WidthFixed, 96);
        ImGui::TableHeadersRow();
        ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
        if (g_houseOrder.empty() || (specs && specs->SpecsDirty)) { SortHouses(specs); if (specs) specs->SpecsDirty = false; }
        for (int i : g_houseOrder) {
            const BuildingType& b = all[i];
            if (g_onlySelected && !g_houseSel[i]) continue;
            if (g_filter[0] && b.name.find(g_filter) == std::string::npos && b.id.find(g_filter) == std::string::npos) continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID(i);
            bool sel = g_houseSel[i] != 0;
            if (ImGui::Checkbox(b.name.c_str(), &sel)) { g_houseSel[i] = sel; g_dirty = true; }
            ImGui::SetItemTooltip("%s", b.id.c_str());
            ImGui::TableNextColumn();
            if (sel) {      // share of this type among the houses of the district
                ImGui::SetNextItemWidth(58);
                if (ImGui::DragInt("##share", &g_houseShare[i], 1.0f, 0, 100, "%d")) { g_houseShare[i] = std::clamp(g_houseShare[i], 0, 100); g_dirty = true; }
                ImGui::SetItemTooltip("Вес этого типа. Потяните мышью или дважды кликните и введите число.\nСправа — получившаяся доля среди домов района.");
                ImGui::SameLine(); ImGui::Text("%d %%", shareSum > 0 ? (int)lroundf(100.0f * g_houseShare[i] / shareSum) : 0);
            }
            ImGui::PopID();
            ImGui::TableNextColumn(); ImGui::Text("%d", b.capacity);
            ImGui::TableNextColumn(); ImGui::Text("%.0fx%.0f", b.sx, b.sz);
            ImGui::TableNextColumn(); if (b.quality > 0) ImGui::Text("%.2f", b.quality); else ImGui::TextDisabled("-");
            ImGui::TableNextColumn(); ImGui::TextUnformatted(MaterialName(b.material));
            ImGui::TableNextColumn(); ImGui::TextUnformatted(b.style.empty() ? "-" : b.style.c_str());
        }
        ImGui::EndTable();
    }
}

void TabServices() {
    const auto& all = catalog::All();
    ImGui::TextDisabled("Что строить в квартале и на сколько жителей одно здание.");
    if (ImGui::BeginTable("services", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Что строить", ImGuiTableColumnFlags_WidthFixed, 170);
        ImGui::TableSetupColumn("Здание", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("1 на N жит.", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableHeadersRow();
        for (auto& s : g_services) {
            ImGui::PushID(s.category);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(s.typeIndex < 0);
            if (ImGui::Checkbox(s.label, &s.enabled)) g_dirty = true;
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##type", s.typeIndex >= 0 ? all[s.typeIndex].name.c_str() : "нет в игре")) {
                for (size_t i = 0; i < all.size(); ++i) {
                    if (all[i].category != s.category) continue;
                    char label[200]; snprintf(label, sizeof(label), "%s  (%.0fx%.0f м)##%d", all[i].name.c_str(), all[i].sx, all[i].sz, (int)i);
                    if (ImGui::Selectable(label, s.typeIndex == (int)i)) { s.typeIndex = (int)i; g_dirty = true; }
                }
                ImGui::EndCombo();
            }
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##per", &s.perResidents, 0)) { s.perResidents = std::clamp(s.perResidents, 100, 100000); g_dirty = true; }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

// A world point can be clicked through the game only if it is on screen, clear of the game HUD, and not hidden
// behind a hill (the game picks the first terrain point under the cursor).
bool Clickable(const V3& w) {
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    float sx, sy;
    V3 p{ w.x, eng::TerrainHeight(w.x, w.z), w.z };
    if (!eng::WorldToScreen(p, ds.x, ds.y, sx, sy)) return false;
    if (sx < 60 || sy < 70 || sx > ds.x - 60 || sy > ds.y - 190) return false;
    V3 hit;
    if (!eng::ScreenToTerrain(sx, sy, ds.x, ds.y, hit)) return false;
    return fabsf(hit.x - w.x) + fabsf(hit.z - w.z) < 5.0f;
}

// Lines of the plan (footpaths or the street) drawn with the game's own line tool: click at every point of a
// polyline, right click to end it. Long pieces are split so the line follows the ground.
struct LineJob { std::vector<V3> pos; std::vector<int> action, frames; int lines = 0, skipped = 0; };
void AddLine(LineJob& job, float u1, float v1, float u2, float v2) {
    float len = sqrtf((u2 - u1) * (u2 - u1) + (v2 - v1) * (v2 - v1));
    if (len < 3.0f) return;
    int n = std::max(1, (int)ceilf(len / 70.0f));
    std::vector<V3> pts;
    for (int i = 0; i <= n; ++i) {
        float t = (float)i / n;
        pts.push_back(LocalToWorld(g_area, u1 + (u2 - u1) * t, v1 + (v2 - v1) * t, 0));
        if (!Clickable(pts.back())) { ++job.skipped; return; }
    }
    auto step = [&](const V3& w, int action, int frames) { job.pos.push_back(w); job.action.push_back(action); job.frames.push_back(frames); };
    for (size_t i = 0; i + 1 < pts.size(); ++i) {     // press at the start, drag, release at the end
        const V3& a = pts[i]; const V3& b = pts[i + 1];
        V3 mid{ (a.x + b.x) / 2, 0, (a.z + b.z) / 2 };
        step(a, 0, 14); step(a, 3, 10); step(mid, 3, 10); step(b, 3, 16); step(b, 0, 8);
    }
    ++job.lines;
}
void LineButton(const char* what, const char* tool, LineJob& job) {
    char t[220];
    if (job.lines == 0) snprintf(t, sizeof(t), "%s: нечего строить%s", what, job.skipped ? " (всё за экраном или за холмом)" : "");
    else if (job.skipped) snprintf(t, sizeof(t), "Проложить в игре %s — %d отрезков (не видно ещё %d)", what, job.lines, job.skipped);
    else snprintf(t, sizeof(t), "Проложить в игре %s — %d отрезков", what, job.lines);
    ImGui::BeginDisabled(job.lines == 0 || eng::AutoBuildActive());
    if (ImGui::Button(t, ImVec2(-FLT_MIN, 0))) { ImVec2 ds = ImGui::GetIO().DisplaySize; eng::StartScript(job.pos, job.action, job.frames, ds.x, ds.y); }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Сначала выберите в игровом меню «Транспортная отрасль» %s.\n"
                          "Мод сам протянет каждый отрезок игровым инструментом: двигает настоящий курсор и зажимает кнопку.\n"
                          "Панель на это время прячется, мышь не трогайте. Esc — остановить. Платит игра.\n"
                          "Отрезок к зданию начинается точно в его точке подключения, чтобы игра к ней привязалась.\n"
                          "Порядок: сначала улицы и подъезды, потом дорожки.\n"
                          "Отрезки, концы которых за экраном, под панелями игры или закрыты холмом, пропускаются —\n"
                          "поднимите камеру выше и смотрите на участок сверху. Камеру во время работы не двигайте.", tool);
}

// Everything of the plan placed at once by direct calls into the game (the mechanism behind the game's own
// "paste area"): streets, buildings, driveways, footpaths. Buildings the game refuses because the ground is
// uneven get their pad levelled and one more try.
struct InstantRun {
    int phase = 0;                      // 0 idle, 1 first batch, 2 levelling, 3 retry batch
    std::vector<eng::InstantJob> jobs;
    std::vector<int> plan;              // per job: index of the plan building, -1 for lines
    std::vector<int> retry;             // plan buildings waiting for levelling
    std::vector<eng::InstantJob> lineRetry;     // utility lines that failed while some buildings were still missing
    int bOk = 0, bAll = 0, rOk = 0, rAll = 0, pOk = 0, pAll = 0, nOk = 0, nAll = 0;
    std::string note;
} g_inst;
void InstantLine(int kind, int lineType, float u1, float v1, float u2, float v2) {
    float len = sqrtf((u2 - u1) * (u2 - u1) + (v2 - v1) * (v2 - v1));
    if (len < 1.5f) return;
    int n = std::max(1, (int)ceilf(len / 60.0f));       // short pieces follow the ground
    for (int i = 0; i < n; ++i) {
        float t0 = (float)i / n, t1 = (float)(i + 1) / n;
        eng::InstantJob j; j.kind = kind; j.lineType = lineType;
        j.a = LocalToWorld(g_area, u1 + (u2 - u1) * t0, v1 + (v2 - v1) * t0, 0);
        j.b = LocalToWorld(g_area, u1 + (u2 - u1) * t1, v1 + (v2 - v1) * t1, 0);
        g_inst.jobs.push_back(j); g_inst.plan.push_back(-1);
    }
}
eng::InstantJob InstantBuilding(const Placed& b) {
    eng::InstantJob j; j.kind = 0; j.type = catalog::All()[b.type].id;
    j.a = LocalToWorld(g_area, b.ou, b.ov, 0);
    j.rot = g_buildRotSign * (atan2f(g_area.uz, g_area.ux) + b.rot * kPi / 2);
    return j;
}
void InstantStart() {
    g_inst = InstantRun();
    for (auto& rd : g_plan.roads) InstantLine(1, g_roadKind, rd.u1, rd.v1, rd.u2, rd.v2);
    for (size_t i = 0; i < g_plan.buildings.size(); ++i) {
        const Placed& b = g_plan.buildings[i];
        V3 w = LocalToWorld(g_area, b.u, b.v, 0);
        if (eng::Occupied(w.x, w.z) & 1) continue;      // already there
        g_inst.jobs.push_back(InstantBuilding(b)); g_inst.plan.push_back((int)i);
    }
    for (auto& rd : g_plan.drives) InstantLine(1, g_roadKind, rd.u1, rd.v1, rd.u2, rd.v2);
    for (auto& ps : g_plan.paths) InstantLine(2, g_pathKind - 1, ps.u1, ps.v1, ps.u2, ps.v2);
    for (auto& s : g_plan.utilLines) {      // after the buildings, so the ends snap to their connections
        // manager offset, type, sub-type, start argument - as logged from the game's own tools
        const int pw = g_powerUnder ? 20 : 10;
        const int net[4][4] = { { 0xfaf0, 20, 0, -20 }, { 0xfaf0, 20, 5, -20 }, { 0xfa98, 10, 0, -20 }, { 0xfa20, pw, 0, pw } };
        size_t from = g_inst.jobs.size();
        InstantLine(3, net[s.net][1], s.u1, s.v1, s.u2, s.v2);
        for (size_t k = from; k < g_inst.jobs.size(); ++k) { g_inst.jobs[k].mgr = net[s.net][0]; g_inst.jobs[k].sub = net[s.net][2]; g_inst.jobs[k].arg = net[s.net][3]; }
    }
    if (g_inst.jobs.empty()) { g_inst.note = "Нечего строить."; return; }
    eng::QueueInstant(g_inst.jobs);
    g_inst.phase = 1;
}
void InstantTick() {
    InstantRun& r = g_inst;
    if (r.phase == 0 || eng::InstantBusy()) return;
    if (r.phase == 2) {                 // pads levelled: try those buildings again
        if (eng::LevelPending()) return;
        r.jobs.clear(); r.plan.clear();
        for (int bi : r.retry) { r.jobs.push_back(InstantBuilding(g_plan.buildings[bi])); r.plan.push_back(bi); }
        for (auto& j : r.lineRetry) { r.jobs.push_back(j); r.plan.push_back(-1); }
        r.lineRetry.clear();
        r.retry.clear();
        eng::QueueInstant(r.jobs);
        r.phase = 3;
        return;
    }
    std::vector<int> codes; eng::InstantResults(codes);
    std::vector<eng::InstantJob> failedLines;
    for (size_t i = 0; i < r.jobs.size() && i < codes.size(); ++i) {
        const eng::InstantJob& j = r.jobs[i];
        if (j.kind == 0) {
            if (r.phase == 1) ++r.bAll;
            if (codes[i] == -1) ++r.bOk;
            else if (r.phase == 1 && codes[i] == 10) r.retry.push_back(r.plan[i]);
        } else if (j.kind == 1) { ++r.rAll; if (codes[i] == 1) ++r.rOk; }
        else if (j.kind == 3) { if (r.phase == 1) ++r.nAll; if (codes[i] == 1) ++r.nOk; else if (r.phase == 1) failedLines.push_back(j); }
        else { ++r.pAll; if (codes[i] == 1) ++r.pOk; }
    }
    if (r.phase == 1 && !r.retry.empty()) {
        r.lineRetry = failedLines;
        std::vector<V3> pts;
        for (int bi : r.retry) {
            const Placed& b = g_plan.buildings[bi];
            for (float du = -b.w / 2 - 4; du <= b.w / 2 + 4.01f; du += 2.5f)
                for (float dv = -b.d / 2 - 4; dv <= b.d / 2 + 4.01f; dv += 2.5f) {
                    V3 w = LocalToWorld(g_area, b.u + du, b.v + dv, 0);
                    pts.push_back({ w.x, b.pad, w.z });
                }
        }
        eng::QueueLevel(pts);
        r.phase = 2;
        return;
    }
    char t[260];
    snprintf(t, sizeof(t), "Поставлено: зданий %d из %d, дорог %d из %d отрезков, дорожек %d из %d отрезков, сетей %d из %d отрезков.", r.bOk, r.bAll, r.rOk, r.rAll, r.pOk, r.pAll, r.nOk, r.nAll);
    r.note = t; r.phase = 0;
    Log("instant build summary: buildings %d/%d, roads %d/%d, footpaths %d/%d, utility lines %d/%d", r.bOk, r.bAll, r.rOk, r.rAll, r.pOk, r.pAll, r.nOk, r.nAll);
}
void InstantUi() {
    InstantTick();
    InstantRun& r = g_inst;
    if (r.phase) {
        ImGui::BeginDisabled();
        ImGui::Button(r.phase == 2 ? "Выравниваю площадки под отклонённые здания…" : "Строю…", ImVec2(-FLT_MIN, 0));
        ImGui::EndDisabled();
        return;
    }
    char t[160]; snprintf(t, sizeof(t), "Построить всё сразу — %d зданий, улицы, подъезды, дорожки", (int)g_plan.buildings.size());
    ImGui::BeginDisabled(g_plan.buildings.empty() && g_plan.paths.empty());
    if (ImGui::Button(t, ImVec2(-FLT_MIN, 0))) InstantStart();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Весь план ставится в игру одним действием, тем же способом, каким игра вставляет скопированную зону:\n"
                          "ничего выбирать в меню стройки и кликать не нужно, камера может стоять как угодно.\n"
                          "Порядок: улицы, здания, подъезды, дорожки. Игра сама проверяет каждое место и берёт плату\n"
                          "в текущем режиме стройки. Где земля слишком неровная, мод выравнивает площадку и повторяет.");
    if (!r.note.empty()) ImGui::TextWrapped("%s", r.note.c_str());
}

void TabRoads() {
    if (eng::InstantSupported()) {
        ImGui::SeparatorText("Покрытие");
        const char* rk[] = { "гравийная дорога", "асфальтовая дорога" };
        const char* pk[] = { "тропинка", "гравийная дорожка", "асфальтовая дорожка" };
        ImGui::SetNextItemWidth(280); ImGui::Combo("улицы и подъезды", &g_roadKind, rk, 2);
        ImGui::SetNextItemWidth(280); ImGui::Combo("дорожки", &g_pathKind, pk, 3);
        ImGui::TextDisabled("Используется кнопкой «Построить всё сразу» внизу панели.");
        ImGui::TextDisabled("Зелёные точки — пешеходные входы зданий, оранжевые квадраты — подъезды для машин.");
        const ImVec4 red(0.65f, 0.1f, 0.1f, 1);
        if (g_plan.noRoad) ImGui::TextColored(red, g_plan.roads.empty() ? "Зданиям нужна автодорога: %d — включите улицу ниже." : "Не удалось подвести автодорогу к зданиям: %d.", g_plan.noRoad);
        if (g_plan.noPath) ImGui::TextColored(red, "Не удалось подвести дорожку к зданиям: %d.", g_plan.noPath);
    } else if (g_area.ok && !g_plan.buildings.empty()) {
        ImGui::SeparatorText("Построить в игре");
        if (eng::AutoBuildActive()) {
            int done, failed, left; eng::AutoBuildStats(done, failed, left);
            char t[120]; snprintf(t, sizeof(t), "Стоп  ·  кликов сделано %d, шагов осталось %d", done, left);
            if (ImGui::Button(t, ImVec2(-FLT_MIN, 0))) eng::StopAutoBuild();
        } else {
            LineJob paths, roads, drives;
            for (auto& ps : g_plan.paths) AddLine(paths, ps.u1, ps.v1, ps.u2, ps.v2);
            for (auto& rd : g_plan.roads) AddLine(roads, rd.u1, rd.v1, rd.u2, rd.v2);
            for (auto& rd : g_plan.drives) AddLine(drives, rd.u1, rd.v1, rd.u2, rd.v2);
            if (!g_plan.roads.empty()) LineButton("улицы", "тип дороги", roads);
            if (!g_plan.drives.empty()) LineButton("подъезды к зданиям", "тип дороги (подъезд можно попроще)", drives);
            LineButton("дорожки", "тип пешеходной дорожки", paths);
        }
        ImGui::TextDisabled("Зелёные точки — пешеходные входы зданий, оранжевые квадраты — подъезды для машин.");
        const ImVec4 red(0.65f, 0.1f, 0.1f, 1);
        if (g_plan.noRoad) ImGui::TextColored(red, g_plan.roads.empty() ? "Зданиям нужна автодорога: %d — включите улицу ниже." : "Не удалось подвести автодорогу к зданиям: %d.", g_plan.noRoad);
        if (g_plan.noPath) ImGui::TextColored(red, "Не удалось подвести дорожку к зданиям: %d.", g_plan.noPath);
    }
    ImGui::SeparatorText("Улица через район");
    const char* types[] = { "без дороги (только дорожки)", "улица, 2 полосы", "проспект, 4 полосы" };
    ImGui::SetNextItemWidth(280);
    if (ImGui::Combo("тип дороги", &g_roadType, types, 3)) g_dirty = true;
    if (ImGui::SliderFloat("улица через каждые, м (0 — одна)", &g_streetGap, 0.0f, 600.0f, "%.0f")) { if (g_streetGap < 80) g_streetGap = 0; g_dirty = true; }
    ImGui::TextDisabled("Вдоль улицы идут тротуары, дорожки кварталов сходятся к переходу у остановки.");

    ImGui::SeparatorText("Остановки");
    ImGui::BeginDisabled(g_roadType == 0 || !g_area.ok);
    int removeAt = -1;
    for (size_t i = 0; i < g_stops.size(); ++i) {
        ImGui::PushID((int)i);
        ImGui::BulletText("Остановка %d", (int)i + 1);
        if (g_stops.size() > 1) { ImGui::SameLine(); if (ImGui::SmallButton("убрать")) removeAt = (int)i; }
        ImGui::PopID();
    }
    if (removeAt >= 0) { g_stops.erase(g_stops.begin() + removeAt); g_dirty = true; }
    if (ImGui::Button("Добавить остановку") && g_stops.size() < 8) {
        float L = g_area.minU, R = g_area.maxU;
        PolyScan(g_area.local, g_roadV, L, R);
        g_stops.push_back(L + (R - L) * (0.25f + 0.5f * (float)(g_stops.size() % 2)));
        g_dirty = true;
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled("Остановку (синий кружок на карте) можно перетащить мышью:\nулица пройдёт через неё, район перестроится.");

    ImGui::SeparatorText("Пешая доступность");
    ImGui::SetNextItemWidth(280);
    if (ImGui::SliderFloat("сколько идут до остановки, м", &g_walk, 150.0f, 900.0f, "%.0f")) g_dirty = true;
    ImGui::TextDisabled("Дома дальше этого расстояния обводятся красным.");
}

void TabUtils() {
    ImGui::TextWrapped("По правилам игры: дома берут воду, тепло и свет от узла в радиусе обслуживания, к самим домам линии не нужны. "
                       "Труба соединяет только два здания, поэтому узлы висят на распределителях, а те идут цепочкой; провод среднего "
                       "напряжения идёт от трансформатора прямо к подстанции (до 6 на трансформатор). Начало каждой цепочки и "
                       "высокую сторону трансформатора подключаете вы — к своему источнику. Радиусы в игре не подписаны: проверьте их и поправьте здесь.");
    ImGui::Spacing();
    int people = g_plan.capacity;
    for (int k = 0; k < 4; ++k) {
        ImGui::PushID(k);
        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImGui::ColorConvertU32ToFloat4(kUtilCol[k]));
        if (ImGui::Checkbox(kUtilName[k], &g_utilBuild[k])) { g_dirty = true; g_utilShow[k] = g_utilBuild[k]; }
        ImGui::PopStyleColor();
        ImGui::SameLine(); ImGui::Checkbox("показывать", &g_utilShow[k]);
        if (k == 3) { ImGui::SameLine(); ImGui::Checkbox("кабель под землёй", &g_powerUnder); }
        ImGui::SetNextItemWidth(200);
        if (ImGui::SliderFloat("радиус узла, м", &g_utilRadius[k], 60.0f, 600.0f, "%.0f")) g_dirty = true;
        ImGui::SetNextItemWidth(120);
        ImGui::InputFloat("на жителя", &g_utilNorm[k], 0, 0, "%.2f");
        ImGui::SameLine(); ImGui::TextDisabled("%s (прикидка)", kUtilUnit[k]);
        if (g_utilBuild[k] && g_area.ok && !g_plan.buildings.empty()) {
            int nodes = (int)g_plan.utilNodes[k].size(), helpers = 0;
            for (auto& b : g_plan.buildings) if (b.net == k && b.role == 1) ++helpers;
            ImGui::Text("Узлов (%s): %d  ·  %s: %d  ·  линий %.0f м  ·  нагрузка %.0f %s", kUtilNode[k], nodes,
                        k == 3 ? "трансформаторов" : "распределителей", helpers, g_plan.utilLen[k], people * g_utilNorm[k], kUtilUnit[k]);
            if (g_plan.utilUncovered[k]) ImGui::TextColored(ImVec4(0.65f, 0.1f, 0.1f, 1), "Вне радиуса зданий: %d.", g_plan.utilUncovered[k]);
            if (g_plan.utilNoLine[k]) ImGui::TextColored(ImVec4(0.65f, 0.1f, 0.1f, 1), "Не удалось проложить соединений: %d.", g_plan.utilNoLine[k]);
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::TextDisabled("Отмеченные сети ставятся вместе со всем планом кнопкой мгновенной постройки.");
}

void Footer() {
    ImGui::Separator();
    const ImVec4 red(0.65f, 0.1f, 0.1f, 1);
    if (!g_area.ok) ImGui::TextDisabled("Нарисуйте участок на карте.");
    else if (g_plan.buildings.empty()) ImGui::TextColored(red, "Ничего не поместилось: отметьте дома, увеличьте участок или допустимый перепад.");
    else {
        ImGui::Text("Домов %d  ·  мест %d из %d  ·  служб %d", g_plan.houses, g_plan.capacity, g_population, g_plan.services);
        ImGui::Text("Дорог %.0f м  ·  дорожек %.0f м  ·  срезать %.0f м³  ·  насыпать %.0f м³", g_plan.roadLength, g_plan.pathLength, g_plan.cut, g_plan.fill);
        ImGui::Text("Рабочих мест в службах %d  ·  работников ≈ %d (60 %% жителей)", g_plan.jobs, g_plan.capacity * 6 / 10);
        if (g_plan.maxWalkToStop >= 0) {
            if (g_plan.farHouses) ImGui::TextColored(red, "До остановки до %.0f м — %d домов дальше %.0f м, добавьте остановку.", g_plan.maxWalkToStop, g_plan.farHouses, g_walk);
            else ImGui::Text("До остановки не дальше %.0f м  ·  до магазина не дальше %.0f м", g_plan.maxWalkToStop, std::max(0.0f, g_plan.maxWalkToShop));
        } else if (g_plan.maxWalkToShop >= 0) ImGui::Text("До магазина не дальше %.0f м", g_plan.maxWalkToShop);
        if (g_plan.unplaced > 0) ImGui::TextColored(red, "Вмещается около %d жит., не влезло зданий: %d — нужен участок больше.", g_plan.capacity, g_plan.unplaced);
        else if (g_plan.steepSkipped > 0 || g_plan.busySkipped > 0) ImGui::TextDisabled("Обойдено: крутые места %d раз, существующая застройка %d раз.", g_plan.steepSkipped, g_plan.busySkipped);
    }
    if (ImGui::Button("Пересобрать план", ImVec2(-FLT_MIN, 0)) && g_area.ok) g_dirty = g_areaChanged = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Заново читает рельеф и расставляет здания, улицу и дорожки по всему участку.");
    static bool levelling = false;
    int pending = eng::LevelPending();
    if (levelling && !pending) { levelling = false; g_dirty = g_areaChanged = true; }
    bool canLevel = g_area.ok && !g_plan.buildings.empty() && !pending;
    ImGui::BeginDisabled(!canLevel);
    char lbl[96];
    if (pending) snprintf(lbl, sizeof(lbl), "Выравниваю… осталось точек: %d", pending);
    else snprintf(lbl, sizeof(lbl), "Выровнять землю под зданиями (сразу, бесплатно)");
    if (ImGui::Button(lbl, ImVec2(-FLT_MIN, 0))) {
        std::vector<V3> pts;
        for (auto& b : g_plan.buildings)
            for (float du = -b.w / 2; du <= b.w / 2 + 0.01f; du += 2.5f)
                for (float dv = -b.d / 2; dv <= b.d / 2 + 0.01f; dv += 2.5f) {
                    V3 w = LocalToWorld(g_area, b.u + du, b.v + dv, 0);
                    pts.push_back({ w.x, b.pad, w.z });
                }
        eng::QueueLevel(pts);
        levelling = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Землю под зданиями плана сразу ставит на высоту площадок. "
                          "Бесплатно, без техники; отменить нельзя — сохранитесь заранее.");
    // Building in the game. The game's own build tool does the work: the player picks a building in the game's
    // build menu, the mod notices which one, turns it as the plan says, puts the game's cursor on every spot of
    // that type and clicks; then it asks for the next type.
    if (eng::InstantSupported() && g_area.ok) InstantUi();
    else {
        static bool seq = false, seqRunning = false;
        static int seqType = -1; static DWORD seqSince = 0;
        static std::vector<int> seqDone;
        const auto& all = catalog::All();
        auto lower = [](std::string v) { for (auto& ch : v) ch = (char)tolower((unsigned char)ch); return v; };
        auto spots = [&](int type, std::vector<V3>& pos, std::vector<float>& rot) {
            for (auto& b : g_plan.buildings) {
                if (b.type != type) continue;
                V3 w = LocalToWorld(g_area, b.u, b.v, 0);
                if (eng::Occupied(w.x, w.z) & 1) continue;              // already built
                pos.push_back(LocalToWorld(g_area, b.ou, b.ov, 0));     // the game's cursor holds the model origin
                rot.push_back(g_buildRotSign * (atan2f(g_area.uz, g_area.ux) + b.rot * kPi / 2));
            }
        };
        auto start = [&](int type) {
            std::vector<V3> pos; std::vector<float> rot; spots(type, pos, rot);
            if (pos.empty()) return false;
            ImVec2 ds = ImGui::GetIO().DisplaySize;
            eng::StartAutoBuild(pos, rot, ds.x, ds.y);
            return true;
        };
        std::vector<int> types; int total = 0;      // types that still have free spots, in plan order
        for (auto& b : g_plan.buildings) {
            if (std::find(types.begin(), types.end(), b.type) != types.end()) continue;
            if (std::find(seqDone.begin(), seqDone.end(), b.type) != seqDone.end()) continue;
            std::vector<V3> pos; std::vector<float> rot; spots(b.type, pos, rot);
            if (!pos.empty()) { types.push_back(b.type); total += (int)pos.size(); }
        }

        if (eng::AutoBuildActive()) {
            int done, failed, left; eng::AutoBuildStats(done, failed, left);
            char t[160]; snprintf(t, sizeof(t), "Стоп  ·  поставлено %d, не вышло %d, осталось %d", done, failed, left);
            if (ImGui::Button(t, ImVec2(-FLT_MIN, 0))) { eng::StopAutoBuild(); seq = false; }
        } else if (seq) {
            if (seqRunning) { seqRunning = false; seqDone.push_back(seqType); seqType = -1; }      // one type finished
            else if (types.empty()) seq = false;
            else {
                if (std::find(types.begin(), types.end(), seqType) == types.end()) { seqType = types[0]; seqSince = GetTickCount(); }
                ImGui::TextColored(ImVec4(0.573f, 0.176f, 0.149f, 1), "Выберите в меню стройки игры: %s", all[seqType].name.c_str());
                ImGui::SetItemTooltip("Обычно мод сам замечает выбор и начинает ставить. Если не заметил — нажмите «Уже выбрано — ставить».");
                DWORD tick = 0;
                std::string cur = lower(eng::SelectedType(&tick));
                bool cursor = eng::CursorActive();
                if (cursor && tick > seqSince && cur == lower(all[seqType].id)) { if (start(seqType)) seqRunning = true; }
                float w3 = (ImGui::GetContentRegionAvail().x - 16) / 3;
                ImGui::BeginDisabled(!cursor);
                if (ImGui::Button("Уже выбрано — ставить", ImVec2(w3 * 1.4f, 0))) { if (start(seqType)) seqRunning = true; }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Пропустить", ImVec2(w3 * 0.8f, 0))) { seqDone.push_back(seqType); seqType = -1; }
                ImGui::SameLine();
                if (ImGui::Button("Стоп", ImVec2(-FLT_MIN, 0))) seq = false;
            }
        } else if (!types.empty() || !seqDone.empty()) {
            char t[160];
            if (types.empty()) snprintf(t, sizeof(t), "Повторить для пропущенных и не вставших зданий");
            else snprintf(t, sizeof(t), "Построить в игре всё по очереди — %d зданий", total);
            if (ImGui::Button(t, ImVec2(-FLT_MIN, 0))) { seq = !types.empty(); seqRunning = false; seqType = -1; seqDone.clear(); }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Поставьте камеру так, чтобы весь участок был на экране, и не двигайте её.\n"
                                  "Мод по очереди называет здание — выберите его в игровом меню строительства.\n"
                                  "Дальше он сам поворачивает здание как на плане, наводит игровой курсор и кликает\n"
                                  "во всех местах этого типа, затем называет следующее. Платит игра как обычно.\n"
                                  "На неровном месте мод удерживает кнопку, чтобы игра выровняла площадку.");
        } else if (!g_plan.buildings.empty()) ImGui::TextDisabled("Все здания плана уже стоят в игре.");
    }
}

void Panel() {
    ImGui::SetNextWindowSize(ImVec2(660, 680), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(70, 110), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(540, 460), ImVec2(1400, 1400));
    if (!ImGui::Begin("##dp_panel3", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) { ImGui::End(); return; }

    // header band like the game windows: dark red strip with a centred caption
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos(); float ww = ImGui::GetWindowWidth();
    const float hh = 36;
    dl->AddRectFilled(wp, ImVec2(wp.x + ww, wp.y + hh), IM_COL32(146, 45, 38, 255), ImGui::GetStyle().WindowRounding, ImDrawFlags_RoundCornersTop);
    const char* title = ":: Планировщик квартала ::";
    ImVec2 ts = ImGui::CalcTextSize(title);
    dl->AddText(ImVec2(wp.x + (ww - ts.x) / 2, wp.y + (hh - ts.y) / 2), IM_COL32(255, 244, 222, 255), title);
    ImGui::SetCursorPos(ImVec2(ww - 32, 6));
    if (ImGui::InvisibleButton("##close", ImVec2(24, 24))) g_open = false;
    ImU32 xc = ImGui::IsItemHovered() ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 230, 200, 200);
    ImVec2 c(wp.x + ww - 20, wp.y + 18);
    dl->AddLine(ImVec2(c.x - 6, c.y - 6), ImVec2(c.x + 6, c.y + 6), xc, 2.5f);
    dl->AddLine(ImVec2(c.x - 6, c.y + 6), ImVec2(c.x + 6, c.y - 6), xc, 2.5f);
    ImGui::SetCursorPos(ImVec2(ImGui::GetStyle().WindowPadding.x, hh + 8));

    if (!eng::Ready()) ImGui::TextColored(ImVec4(0.65f, 0.1f, 0.1f, 1), "Загрузите карту или сохранение, чтобы выбрать участок.");

    float footer = ImGui::GetTextLineHeightWithSpacing() * 6 + ImGui::GetFrameHeightWithSpacing() * 3 + 14;
    if (ImGui::BeginTabBar("##tabs")) {
        struct { const char* name; void (*fn)(); } tabs[] = { { "Участок", TabArea }, { "Дома", TabHouses }, { "Службы", TabServices }, { "Дороги", TabRoads }, { "Сети", TabUtils } };
        for (auto& t : tabs) {
            if (!ImGui::BeginTabItem(t.name)) continue;
            ImGui::BeginChild("##body", ImVec2(0, std::max(120.0f, ImGui::GetContentRegionAvail().y - footer)));
            t.fn();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    Footer();
    ImGui::End();
}

// Square paper button in the style of the game's left toolbar, with a small "houses" pictogram.
void ToggleButton() {
    ImGuiIO& io = ImGui::GetIO();
    const float sz = 54;
    ImGui::SetNextWindowPos(ImVec2(10, io.DisplaySize.y * 0.80f), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##dp_toggle2", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImVec2 p = ImGui::GetCursorScreenPos();
    static bool dragged = false;
    ImGui::InvisibleButton("##toggle", ImVec2(sz, sz));
    if (ImGui::IsItemActivated()) dragged = false;
    if (ImGui::IsItemActive() && (dragged || ImGui::IsMouseDragging(0, 5.0f))) {
        dragged = true;
        ImVec2 wpos = ImGui::GetWindowPos();
        ImGui::SetWindowPos(ImVec2(std::clamp(wpos.x + io.MouseDelta.x, 0.0f, io.DisplaySize.x - sz - 8),
                                   std::clamp(wpos.y + io.MouseDelta.y, 0.0f, io.DisplaySize.y - sz - 8)));
    }
    if (ImGui::IsItemDeactivated() && !dragged) g_open = !g_open;
    bool hov = ImGui::IsItemHovered();
    ImGui::SetItemTooltip("Планировщик квартала (F8). Можно перетащить.");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 q(p.x + sz, p.y + sz);
    dl->AddRectFilled(ImVec2(p.x + 2, p.y + 3), ImVec2(q.x + 2, q.y + 3), IM_COL32(0, 0, 0, 70), 5);
    dl->AddRectFilled(p, q, g_open ? IM_COL32(250, 236, 200, 255) : hov ? IM_COL32(246, 234, 206, 255) : IM_COL32(233, 222, 196, 255), 5);
    dl->AddRect(p, q, g_open ? IM_COL32(146, 45, 38, 255) : IM_COL32(120, 92, 62, 255), 5, 0, g_open ? 3.0f : 1.5f);
    const ImU32 wall = IM_COL32(120, 92, 62, 255), roof = IM_COL32(146, 45, 38, 255), win = IM_COL32(250, 240, 215, 255);
    auto block = [&](float x, float y, float w, float h) {      // apartment block with a roof line and windows
        dl->AddRectFilled(ImVec2(p.x + x, p.y + y), ImVec2(p.x + x + w, p.y + y + h), wall);
        dl->AddRectFilled(ImVec2(p.x + x - 1, p.y + y - 3), ImVec2(p.x + x + w + 1, p.y + y), roof);
        for (float wy = y + 4; wy + 4 < y + h; wy += 7)
            for (float wx = x + 3; wx + 4 <= x + w - 2; wx += 6)
                dl->AddRectFilled(ImVec2(p.x + wx, p.y + wy), ImVec2(p.x + wx + 3, p.y + wy + 4), win);
    };
    block(9, 22, 15, 24); block(28, 13, 17, 33);
    dl->AddLine(ImVec2(p.x + 6, p.y + 47), ImVec2(q.x - 6, p.y + 47), wall, 2.0f);
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// Debug aid: creating DistrictPlanner.selftest next to the game exe (even while the game runs) outlines a
// district in front of the camera without any input and writes the numbers to the log.
int g_selfTest = 0;
void SelfTest() {
    static int tick = 0;
    if (++tick % 120 != 0 || !eng::Ready()) return;
    std::wstring ptFlag = GameDir() + L"DistrictPlanner.pathtest";     // debug: draw the longest plan footpath with the selected game tool
    if (GetFileAttributesW(ptFlag.c_str()) != INVALID_FILE_ATTRIBUTES) {
        LineJob job;
        {   // the file may hold the line mode (0, 1, 2) and how many of the last plan lines to draw (the newest are the house links)
            FILE* f = _wfopen(ptFlag.c_str(), L"r"); int m = -1, n = 0;
            if (f) { char btn[64] = ""; if (fscanf(f, "%d %d %63s", &m, &n, btn) >= 1 && m >= 0 && m <= 2) g_lineMode = m; if (btn[0]) strcpy(eng::g_scriptButton, btn); fclose(f); }
            size_t from = n > 0 && (size_t)n < g_plan.paths.size() ? g_plan.paths.size() - n : 0;
            for (size_t k = from; k < g_plan.paths.size(); ++k) { auto& ps = g_plan.paths[k]; AddLine(job, ps.u1, ps.v1, ps.u2, ps.v2); }
        }
        DeleteFileW(ptFlag.c_str());
        Log("pathtest: mode %d, %d lines, %d skipped (hidden or off screen)", g_lineMode, job.lines, job.skipped);
        ImVec2 d = ImGui::GetIO().DisplaySize;
        if (job.lines) eng::StartScript(job.pos, job.action, job.frames, d.x, d.y);
    }
    std::wstring tlFlag = GameDir() + L"DistrictPlanner.tool";         // debug: select a game tool by name and log the path calls
    if (GetFileAttributesW(tlFlag.c_str()) != INVALID_FILE_ATTRIBUTES) {
        char nm[96] = ""; FILE* f = _wfopen(tlFlag.c_str(), L"r"); if (f) { fscanf(f, "%95s", nm); fclose(f); }
        DeleteFileW(tlFlag.c_str());
        eng::InstallPathLogging();
        if (nm[0]) eng::DebugSetTool(nm);
    }
    std::wstring inFlag = GameDir() + L"DistrictPlanner.instant";      // debug: place the first N plan buildings by direct game calls
    if (GetFileAttributesW(inFlag.c_str()) != INVALID_FILE_ATTRIBUTES) {
        DeleteFileW(inFlag.c_str());
        eng::DebugDumpTools();
        Log("instant test: supported %d, plan %d buildings %d roads %d drives %d paths", (int)eng::InstantSupported(), (int)g_plan.buildings.size(),
            (int)g_plan.roads.size(), (int)g_plan.drives.size(), (int)g_plan.paths.size());
        if (eng::InstantSupported() && g_area.ok) InstantStart();
    }
    std::wstring abFlag = GameDir() + L"DistrictPlanner.autobuild";    // debug: place the selected game building on the first plan spots
    if (GetFileAttributesW(abFlag.c_str()) != INVALID_FILE_ATTRIBUTES) {
        DeleteFileW(abFlag.c_str());
        std::vector<V3> pos; std::vector<float> rot;
        for (size_t i = 0; i < g_plan.buildings.size() && pos.size() < 3; i += 4) {
            const Placed& b = g_plan.buildings[i];
            pos.push_back(LocalToWorld(g_area, b.ou, b.ov, 0));
            rot.push_back(g_buildRotSign * (atan2f(g_area.uz, g_area.ux) + b.rot * kPi / 2));
        }
        Log("autobuild test: cursor active %d, type '%s', %d targets", (int)eng::CursorActive(), eng::CursorTypeName().c_str(), (int)pos.size());
        if (!pos.empty()) { ImVec2 d = ImGui::GetIO().DisplaySize; eng::StartAutoBuild(pos, rot, d.x, d.y); }
    }
    std::wstring flag = GameDir() + L"DistrictPlanner.selftest";
    if (GetFileAttributesW(flag.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    DeleteFileW(flag.c_str());
    g_selfTest = 2;
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    const float sx[6] = { 0.40f, 0.66f, 0.80f, 0.74f, 0.52f, 0.42f }, sy[6] = { 0.42f, 0.40f, 0.52f, 0.80f, 0.86f, 0.66f };
    std::vector<V3> pts;
    for (int i = 0; i < 6; ++i) { V3 p; if (eng::ScreenToTerrain(ds.x * sx[i], ds.y * sy[i], ds.x, ds.y, p)) pts.push_back(p); }
    Log("selftest: display %.0fx%.0f, points hit %d of 6", ds.x, ds.y, (int)pts.size());
    if (pts.size() < 6) return;
    g_population = 600; g_open = true; g_showLevel = false;
    {   // test district: only a building that is unlocked from the start, no services
        const auto& all = catalog::All();
        for (size_t i = 0; i < all.size(); ++i) g_houseSel[i] = all[i].id == "residental_brick_2";
        for (auto& sv : g_services) sv.enabled = false;
        for (int k = 0; k < 4; ++k) { g_utilBuild[k] = g_utilShow[k] = true; g_utilRadius[k] = 110; }      // small radius: several nodes, so the chains get tested
    }
    SetArea(pts);
}

} // namespace

namespace ui {

void ApplyStyle() {
    ImGui::StyleColorsLight();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 7; st.FrameRounding = 4; st.GrabRounding = 4; st.TabRounding = 4; st.PopupRounding = 5; st.ChildRounding = 4;
    st.WindowBorderSize = 1.5f; st.FrameBorderSize = 1; st.WindowPadding = ImVec2(12, 10); st.ItemSpacing = ImVec2(8, 7);
    st.FramePadding = ImVec2(8, 5); st.ScrollbarSize = 16; st.CellPadding = ImVec2(6, 4);
    const ImVec4 paper(0.925f, 0.885f, 0.785f, 0.98f), light(0.975f, 0.95f, 0.875f, 1), ink(0.21f, 0.14f, 0.10f, 1);
    const ImVec4 brown(0.47f, 0.36f, 0.24f, 1), tan(0.84f, 0.76f, 0.61f, 1), tanHi(0.90f, 0.82f, 0.64f, 1), red(0.573f, 0.176f, 0.149f, 1);
    ImVec4* c = st.Colors;
    c[ImGuiCol_Text] = ink; c[ImGuiCol_TextDisabled] = ImVec4(0.47f, 0.40f, 0.32f, 1);
    c[ImGuiCol_WindowBg] = paper; c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0); c[ImGuiCol_PopupBg] = ImVec4(0.95f, 0.92f, 0.83f, 1);
    c[ImGuiCol_Border] = brown; c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = light; c[ImGuiCol_FrameBgHovered] = ImVec4(1, 0.98f, 0.91f, 1); c[ImGuiCol_FrameBgActive] = tanHi;
    c[ImGuiCol_Button] = tan; c[ImGuiCol_ButtonHovered] = tanHi; c[ImGuiCol_ButtonActive] = ImVec4(0.76f, 0.64f, 0.46f, 1);
    c[ImGuiCol_CheckMark] = red; c[ImGuiCol_SliderGrab] = red; c[ImGuiCol_SliderGrabActive] = ImVec4(0.70f, 0.22f, 0.18f, 1);
    c[ImGuiCol_Header] = ImVec4(0.84f, 0.76f, 0.61f, 0.7f); c[ImGuiCol_HeaderHovered] = tanHi; c[ImGuiCol_HeaderActive] = tan;
    c[ImGuiCol_Separator] = brown; c[ImGuiCol_SeparatorHovered] = red; c[ImGuiCol_SeparatorActive] = red;
    c[ImGuiCol_ResizeGrip] = ImVec4(0.47f, 0.36f, 0.24f, 0.4f); c[ImGuiCol_ResizeGripHovered] = brown; c[ImGuiCol_ResizeGripActive] = red;
    c[ImGuiCol_Tab] = tan; c[ImGuiCol_TabHovered] = tanHi; c[ImGuiCol_TabSelected] = light; c[ImGuiCol_TabSelectedOverline] = red;
    c[ImGuiCol_TabDimmed] = tan; c[ImGuiCol_TabDimmedSelected] = light;
    c[ImGuiCol_TableHeaderBg] = tan; c[ImGuiCol_TableBorderStrong] = brown; c[ImGuiCol_TableBorderLight] = ImVec4(0.47f, 0.36f, 0.24f, 0.45f);
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0); c[ImGuiCol_TableRowBgAlt] = ImVec4(0.47f, 0.36f, 0.24f, 0.08f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.47f, 0.36f, 0.24f, 0.12f); c[ImGuiCol_ScrollbarGrab] = ImVec4(0.62f, 0.50f, 0.36f, 1);
    c[ImGuiCol_ScrollbarGrabHovered] = brown; c[ImGuiCol_ScrollbarGrabActive] = red;
    c[ImGuiCol_TitleBg] = red; c[ImGuiCol_TitleBgActive] = red; c[ImGuiCol_TextSelectedBg] = ImVec4(0.573f, 0.176f, 0.149f, 0.3f);
    c[ImGuiCol_NavCursor] = red;
}

bool WantsGameMouse() { return g_pick || g_dragStop >= 0 || g_hoverStop >= 0; }

void Draw() {
    if (!g_inited && !catalog::All().empty()) InitDefaults();
    if (ImGui::IsKeyPressed(ImGuiKey_F8, false)) g_open = !g_open;
    ToggleButton();

    if (!g_inited) return;
    if (!eng::Ready()) { g_pick = false; g_dragStop = g_hoverStop = -1; }
    SelfTest();
    if (g_open && !eng::ScriptActive()) Panel();        // the line script needs the real mouse over the map
    if (g_dirty && (eng::Ready() || !g_area.ok)) {
        Regenerate();
        if (g_selfTest == 2 && g_dragStop < 0)
            Log("plan: %.1f ha, houses %d, capacity %d, services %d, unplaced %d, steep skips %d, roads %.0f m, paths %.0f m, cut %.0f, fill %.0f, tooFar %d, stop walk %.0f m",
                g_area.sqm / 10000.0f, g_plan.houses, g_plan.capacity, g_plan.services, g_plan.unplaced, g_plan.steepSkipped,
                g_plan.roadLength, g_plan.pathLength, g_plan.cut, g_plan.fill, g_plan.farHouses, g_plan.maxWalkToStop);
    }
    if (eng::Ready()) {
        if (g_area.ok && g_open) { DrawWorld(); if (!g_pick) StopsInteraction(); }
        else g_hoverStop = g_dragStop = -1;
        if (g_pick) Picking();
    }
}

} // namespace ui
