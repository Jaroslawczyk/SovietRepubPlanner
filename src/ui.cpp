// In-game panel: free-form territory outline, house / service / road settings, and the plan preview
// (slopes, levelling, street, stops, footpaths, buildings) drawn over the terrain.
#include <map>
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
int   g_stopKind = 0;               // index into kStopIds
const char* const kStopIds[] = { "busstop", "busstop_t", "muddy_busstop", "road_busstation_small", "road_busstation", "tram_stop_small", "tram_stop_big", "" };
const int kStopCount = 8;
bool  g_crossRoad = false;          // a second street across the first one
int   g_roadKind = 2;               // street surface for instant building: 0 gravel, 1 asphalt
int   g_pathKind = 1;               // footpath: 0 dirt trail, 1 gravel, 2 asphalt
std::vector<ServiceRule> g_services = {     // labels are translated with T() where shown
    { "SHOP",          "Магазин",                true,  2000, -1 },
    { "PUB",           "Пивная",                 true,  3000, -1, "pub" },
    { "KINDERGARTEN",  "Детский сад",            true,  2500, -1 },
    { "SCHOOL",        "Школа",                  true,  3000, -1 },
    { "UNIVERSITY",    "Университет",            true,  6000, -1, "university_technical" },
    { "KINO",          "Кинотеатр / культура",   true,  5000, -1, "kino" },
    { "SPORT",         "Спорт: футбольное поле", true,  3000, -1, "playground_football" },
    { "SPORT",         "Спорт: теннис",          true,  2500, -1, "playground_tenis" },
    { "ATTRACTION",    "Бассейн / пляж",         false, 8000, -1, "co_swimmingpool" },
    { "MONUMENT",      "Памятник",               true,  4000, -1, "monument_statue_small" },
    { "MONUMENT",      "Фонтан",                 true,  3000, -1, "fountain1" },
    { "ATTRACTION",    "Парк / аттракцион",      true,  4000, -1, "co_chamomile" },
    { "CITYHALL",      "Горком",                 true,  100000, -1, "cityhall" },
    { "FIRESTATION",   "Пожарная часть",         true,  10000, -1 },
    { "POLICE_STATION","Милиция",                true,  8000, -1, "police_small" },
    { "SECRET_POLICE", "КГБ",                    true,  100000, -1 },
    { "PRISON",        "Тюрьма",                 false, 100000, -1 },
    { "COURT_HOUSE",   "Суд",                    false, 100000, -1, "court_small" },
    { "HOSPITAL",      "Больница",               true,  10000, -1 },
    { "GAS_STATION",   "Автозаправка",           false, 8000, -1, "gas_station" },
    { "PARKING",       "Парковка",               false, 3000, -1, "parking_small" },
    { "CHURCH",        "Церковь",                false, 8000, -1 },
};
int   g_population = 1000;
float g_gap = 8.0f;
int   g_density = 90;               // 0..100: gaps between buildings and rows
bool  g_autoHouse = true;          // the house ticked by default; the first manual tick replaces it
bool  g_fillAll = true;    // no empty corners: houses until nothing fits
int   g_roadType = 1;
float g_roadV = 0;
std::vector<float> g_stops;
float g_walk = 400.0f, g_maxDiff = 5.0f;
// utilities: water, sewage, heating, power
const char* const kUtilName[4] = { "Вода", "Канализация", "Отопление", "Электричество" };
const char* const kUtilNode[4] = { "водораспределитель", "канализационная станция", "тепловой пункт", "трансформатор" };
const char* const kUtilUnit[4] = { "ед. бака", "ед. бака", "ед. бака", "МВт" };
const ImU32 kUtilCol[4] = { IM_COL32(70, 150, 255, 255), IM_COL32(165, 110, 55, 255), IM_COL32(245, 85, 60, 255), IM_COL32(250, 220, 60, 255) };
bool  g_utilShow[4] = { false, false, false, false };
bool  g_utilExisting[4] = { true, true, true, true };    // node buildings already standing nearby serve the district
float g_polA = 0, g_polB = 0; bool g_polOk = false;   // worst air pollution cell under the district (game grid)
int   g_rebuilds = 0; char g_rebuildNote[96] = "";
bool  g_utilBuild[4] = { false, false, false, false };   // the network goes into the plan and gets built
bool  g_powerUnder = false;                               // medium-voltage lines as underground cable
float g_utilRadius[4] = { 250, 250, 380, 250 };   // the game links a building to a node closer than this (2D distance; SOVIET64.exe 0x441340, 0x441600, 0x440d90, 0x4406d0)
bool  g_powerTrafo = true;
bool  g_utilChain = true;       // switches and pipes between the nodes of one network
float g_utilNorm[4] = { 0.12f, 0.12f, 0.7f, 0.002f };  // per resident; heat and power read from the game (brick house: 35 tank units, 0.1 MW for 50), water and sewage are guesses
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
std::vector<V3> g_stopPts, g_crossPts, g_pedPts, g_roadPts, g_gapPts;   // gap: an entrance not joined to the street after a build
std::vector<std::pair<V3, V3>> g_gapTried; // repair links already tried in this build (the game refused them)
std::vector<std::pair<V3, V3>> g_gapFix;   // entrance -> nearest point of the street's footpath net
std::vector<std::vector<V3>> g_driveLines;
int   g_lineMode = 2;               // unused: the game only accepts press-drag-release               // how the game's line tool is driven: 0 two clicks a piece, 1 chain, 2 hold and drag

// ---------- geometry helpers ----------
V2 Turn2(int rot, float x, float z) {   // the planner's quarter turn (model X along +u, +v, -u, -v)
    switch (rot & 3) { case 0: return { x, z }; case 1: return { -z, x }; case 2: return { -x, -z }; default: return { z, -x }; }
}
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
        if (s.prefer) for (size_t i = 0; i < all.size(); ++i) if (all[i].id == s.prefer && all[i].category == s.category) { s.typeIndex = (int)i; best = -1; }
        if (best < 0) continue;
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
    in.poly = a.local; in.population = g_population;
    in.gap = 10.0f - g_density * 0.095f;             // density 0 -> 10 m, 100 -> 0.5 m between building zones
    in.band = 8.0f - g_density * 0.035f;             // and 8 -> 4.5 m for the footpath strip between rows
    in.fillAll = g_fillAll;
    for (size_t i = 0; i < g_houseSel.size(); ++i) if (g_houseSel[i]) { in.houseTypes.push_back((int)i); in.houseWeights.push_back((float)g_houseShare[i]); }
    in.services = &g_services;
    in.roadType = g_roadType; in.roadV = g_roadV; in.stopsU = g_stops; in.crossRoad = g_crossRoad;
    for (size_t t = 0; t < catalog::All().size(); ++t) if (kStopIds[g_stopKind][0] && catalog::All()[t].id == kStopIds[g_stopKind]) in.stopType = (int)t; in.walkRadius = g_walk; in.maxDiff = g_maxDiff; in.streetSpacing = g_streetGap;
    for (int k = 0; k < 4; ++k) { in.utilRadius[k] = g_utilRadius[k]; in.utilBuild[k] = g_utilBuild[k]; if (k >= 2) in.utilPerPerson[k] = g_utilNorm[k]; }
    in.powerTrafo = g_powerTrafo; in.utilChain = g_utilChain;
    {   // node buildings the player already has: they serve houses in their radius, the plan adds none there
        static const char* nodeIds[4][3] = { { "water_substation", "", "" }, { "sewage_substation", "", "" },
                                             { "heating_endstation_small", "heating_endstation", "heating_endstation_big" }, { "eletric_substation", "", "" } };
        std::vector<eng::WorldBld> world; eng::ListBuildings(world);
        for (auto& w : world)
            for (int k = 0; k < 4; ++k) {
                if (!g_utilExisting[k]) continue;
                for (auto* id : nodeIds[k]) if (*id && w.id == id) { in.extNodes[k].push_back(WorldToLocal(a, w.pos)); break; }
            }
    }
    in.occupied = [&a](float u, float v) { return eng::Occupied(a.o.x + a.ux * u + a.vx * v, a.o.z + a.uz * u + a.vz * v) != 0; };
    in.height = [&a](float u, float v) { return eng::TerrainHeight(a.o.x + a.ux * u + a.vx * v, a.o.z + a.uz * u + a.vz * v); };
    {   // connections of the buildings that already stand near the district, for feeding its networks
        std::vector<eng::WorldBld> world; eng::ListBuildings(world);
        std::map<std::string, int> byId;
        const auto& all = catalog::All();
        for (size_t t = 0; t < all.size(); ++t) byId[all[t].id] = (int)t;
        float cu = (a.minU + a.maxU) / 2, cv = (a.minV + a.maxV) / 2;
        for (auto& w : world) {
            auto it = byId.find(w.id); if (it == byId.end()) continue;
            const BuildingType& t = all[it->second];
            float c = cosf(w.yaw), sn = sinf(w.yaw);
            for (auto& cn : t.conns) {
                if (cn.kind != 3 && cn.kind != 4 && cn.kind != 6 && cn.kind != 7 && cn.kind != 9 && cn.kind != 11) continue;
                // model -> world: X along (cos yaw, -sin yaw), Z along (sin yaw, cos yaw) - the inverse of how the plan
                // turns a building for the build cursor (rotation = -plan angle)
                V3 p{ w.pos.x + cn.x * c + cn.z * sn, w.pos.y, w.pos.z - cn.x * sn + cn.z * c };
                V2 l = WorldToLocal(a, p);
                if (fabsf(l.u - cu) > 2500 || fabsf(l.v - cv) > 2500) continue;
                in.extPorts.push_back({ cn.kind, l.u, l.v, w.pos.y + cn.y - eng::TerrainHeight(p.x, p.z) });
            }
        }
        static size_t lastN = (size_t)-1;
        if (lastN != in.extPorts.size()) { lastN = in.extPorts.size(); Log("plan: %d standing buildings, %d of their ports can feed the district", (int)world.size(), (int)lastN); }
    }
    g_plan = MakePlan(in);
    {   // air pollution over the district: the worst cell of the game's 200 m grid under the outline
        g_polA = g_polB = 0; g_polOk = false;
        static bool gridLogged = false; if (!gridLogged) { gridLogged = true; eng::LogPollutionGrid(); }
        for (float u = a.minU; u <= a.maxU; u += 50)
            for (float v = a.minV; v <= a.maxV; v += 50) {
                if (!PolyContains(a.local, u, v)) continue;
                V3 w = LocalToWorld(a, u, v, 0); float pa, pb;
                if (eng::PollutionAt(w.x, w.z, pa, pb)) { g_polOk = true; g_polA = std::max(g_polA, pa); g_polB = std::max(g_polB, pb); }
            }
        static float lastA = -1, lastB = -1;
        if (g_polOk && (g_polA != lastA || g_polB != lastB)) { lastA = g_polA; lastB = g_polB; Log("pollution over the district: max %.4f / %.4f (cell floats 0 / 1)", g_polA, g_polB); }
    }
    if (g_rebuilds) snprintf(g_rebuildNote, sizeof(g_rebuildNote), T("План пересобран (%d): %d зданий, %d отброшено как оторванные."), g_rebuilds, (int)g_plan.buildings.size(), g_plan.cutOff);
    if (g_plan.cutOff) Log("plan: %d buildings and %d footpaths dropped - not joined to the street net", g_plan.cutOff, g_plan.cutOffPaths);
    {
        std::vector<eng::WorldBld> world; eng::ListBuildings(world);
        const auto& all = catalog::All();
        if (GetFileAttributesW((GameDir() + L"DistrictPlanner.portcheck").c_str()) != INVALID_FILE_ATTRIBUTES) {
            DeleteFileW((GameDir() + L"DistrictPlanner.portcheck").c_str());
            // debug: compare where our own placed nodes' ports should be (plan) with the formula on the standing building
            for (auto& b : g_plan.buildings) {
                if (b.net < 0) continue;
                const BuildingType& t = all[b.type];
                V3 o = LocalToWorld(a, b.ou, b.ov, 0);
                { float bd = 1e9f; const eng::WorldBld* bw = nullptr;
                  for (auto& w : world) if (w.id == t.id) { float d = fabsf(w.pos.x - o.x) + fabsf(w.pos.z - o.z); if (d < bd) { bd = d; bw = &w; } }
                  Log("PORTNEAR %s plan origin %.1f %.1f nearest same-id %.1f m at %.1f %.1f yaw %.3f", t.id.c_str(), o.x, o.z, bd, bw ? bw->pos.x : 0, bw ? bw->pos.z : 0, bw ? bw->yaw : 0); }
                for (auto& w : world) {
                    if (w.id != t.id || fabsf(w.pos.x - o.x) + fabsf(w.pos.z - o.z) > 40) continue;
                    float c = cosf(w.yaw), sn = sinf(w.yaw);
                    for (auto& cn : t.conns) {
                        if (cn.kind < 2) continue;
                        V2 pl = Turn2(b.rot, cn.x, cn.z);
                        V3 want = LocalToWorld(a, b.ou + pl.u, b.ov + pl.v, 0);
                        V3 f1{ w.pos.x + cn.x * c + cn.z * sn, 0, w.pos.z - cn.x * sn + cn.z * c };
                        V3 f2{ w.pos.x + cn.x * c - cn.z * sn, 0, w.pos.z + cn.x * sn + cn.z * c };
                        Log("PORT %s kind %d yaw %.3f plan rot %d: want %.1f %.1f | f1 %.1f %.1f | f2 %.1f %.1f | origin plan %.1f %.1f world %.1f %.1f",
                            t.id.c_str(), cn.kind, w.yaw, b.rot, want.x, want.z, f1.x, f1.z, f2.x, f2.z, o.x, o.z, w.pos.x, w.pos.z);
                    }
                }
            }
        }
    }


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
    auto strip = [&](float u1, float v1, float u2, float v2, float width, ImU32 col) {     // a band of the real width
        float len = hypotf(u2 - u1, v2 - v1); if (len < 0.1f) return;
        float nu = -(v2 - v1) / len * width / 2, nv = (u2 - u1) / len * width / 2;
        int n = std::max(1, (int)(len / 8.0f));
        for (int i = 0; i < n; ++i) {
            float t0 = (float)i / n, t1 = (float)(i + 1) / n;
            float a0 = u1 + (u2 - u1) * t0, b0 = v1 + (v2 - v1) * t0, a1 = u1 + (u2 - u1) * t1, b1 = v1 + (v2 - v1) * t1;
            Cell c; c.col = col;
            c.c[0] = LocalToWorld(a, a0 + nu, b0 + nv); c.c[1] = LocalToWorld(a, a1 + nu, b1 + nv);
            c.c[2] = LocalToWorld(a, a1 - nu, b1 - nv); c.c[3] = LocalToWorld(a, a0 - nu, b0 - nv);
            g_roadQuads.push_back(c);
        }
    };
    for (auto& p : g_plan.drives) strip(p.u1, p.v1, p.u2, p.v2, 8.0f, IM_COL32(70, 70, 78, 200));
    for (size_t k = 0; k < g_plan.paths.size(); ++k) {
        const auto& p = g_plan.paths[k];
        if (k < g_plan.pathBuild.size() && !g_plan.pathBuild[k]) continue;      // the street's edge: nothing built
        strip(p.u1, p.v1, p.u2, p.v2, 2.5f, IM_COL32(205, 205, 200, 150));
    }
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
    for (auto& c : g_gapPts) if (Proj(c, s)) { dl->AddCircle(s, 11.0f, IM_COL32(220, 30, 30, 255), 0, 3.0f); }   // gap found after the build
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
        const char* t = T("Остановка");
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
        ImGui::SetTooltip(T("Перетащите остановку — район перестроится"));
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
    if (g_draft.empty()) snprintf(buf, sizeof(buf), T("Клик — первая точка контура   (ПКМ — отмена)"));
    else if (g_draft.size() < 3) snprintf(buf, sizeof(buf), T("Клик — следующая точка   (ПКМ — убрать последнюю)"));
    else snprintf(buf, sizeof(buf), nearFirst ? T("Клик — замкнуть контур") : T("Клик — точка, клик по первой точке или Enter — замкнуть   (ПКМ — убрать последнюю)"));
    ImVec2 tp(io.MousePos.x + 18, io.MousePos.y + 18), ts = ImGui::CalcTextSize(buf);
    dl->AddRectFilled(ImVec2(tp.x - 5, tp.y - 3), ImVec2(tp.x + ts.x + 5, tp.y + ts.y + 3), IM_COL32(0, 0, 0, 190), 4);
    dl->AddText(tp, IM_COL32(255, 255, 255, 255), buf);

    bool close = g_draft.size() >= 3 && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || (nearFirst && ImGui::IsMouseClicked(0)));
    if (close) { SetArea(g_draft); g_draft.clear(); g_pick = false; return; }
    if (ok && ImGui::IsMouseClicked(0) && g_draft.size() < 64) g_draft.push_back(hit);
}

// ---------- panel ----------
const char* MaterialName(const std::string& m) {
    if (m == "brick") return T("кирпич");
    if (m == "panels") return T("панели");
    if (m == "wood") return T("дерево");
    if (m == "concrete") return T("бетон");
    if (m == "steel") return T("сталь");
    return m.empty() ? "-" : m.c_str();
}

std::vector<int> g_houseOrder;      // catalog indices of residential buildings in display order
bool g_onlySelected = false;

// Building volume, m3: the game prices construction from the model (walls, roof), so the volume per resident is the
// honest yardstick of how much material and work a home costs.
float HouseVolume(const BuildingType& b) { return b.sx * b.sz * std::max(3.0f, b.height); }
int  g_resMin = 0, g_resMax = 1000; int g_matFilter = 0;   // house list filter: residents range, wall material
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
            case 2: return a.capacity < b.capacity;
            case 3: return a.sx * a.sz < b.sx * b.sz;
            case 4: return a.quality < b.quality;
            case 5: return a.material < b.material;
            case 6: return a.style < b.style;
            case 7: return HouseVolume(a) / std::max(1, a.capacity) < HouseVolume(b) / std::max(1, b.capacity);
            default: return a.name < b.name;
        }
    });
}

void ToggleHouse(int i, bool sel) {
    if (sel && g_autoHouse) std::fill(g_houseSel.begin(), g_houseSel.end(), 0);   // the default pick goes away
    g_autoHouse = false;
    g_houseSel[i] = sel; g_dirty = true;
}
std::map<std::string, void*> g_icons;
void* Icon(const std::string& id) {
    auto it = g_icons.find(id);
    if (it != g_icons.end()) return it->second;
    void* t = LoadIconTexture(GameDir() + L"media_soviet\\editor\\tool_" + std::wstring(id.begin(), id.end()) + L".png");
    g_icons[id] = t;
    return t;
}
bool g_houseIcons = true;
// Houses in the order of the game's build menu (tool table order), unknown ones at the end.
const std::vector<int>& MenuOrder() {
    static std::vector<int> order; static bool exact = false;
    if (order.empty() || !exact) {
        const auto& all = catalog::All();
        order.clear();
        for (size_t i = 0; i < all.size(); ++i) if (all[i].category == "LIVING" && all[i].capacity > 0) order.push_back((int)i);
        exact = eng::ToolOrder(all[order.empty() ? 0 : order[0]].id) >= 0 || eng::ToolOrder("hrusevka") >= 0;
        std::stable_sort(order.begin(), order.end(), [&](int x, int y) {
            int a = eng::ToolOrder(all[x].id), b = eng::ToolOrder(all[y].id);
            if ((a < 0) != (b < 0)) return b < 0;
            if (a != b) return a < b;
            return all[x].capacity < all[y].capacity;
        });
    }
    return order;
}
void HouseIconGrid(const std::function<bool(const BuildingType&)>& passes) {
    const auto& all = catalog::All();
    ImGui::TextDisabled(T("Как в меню стройки игры. Клик — выбрать / снять."));
    int shareSum = 0, nSel = 0;
    for (size_t i = 0; i < g_houseSel.size(); ++i) if (g_houseSel[i]) { shareSum += std::max(0, g_houseShare[i]); ++nSel; }
    float listH = std::min(34.0f * nSel + 30, 170.0f);
    ImGui::BeginChild("icons", ImVec2(0, std::max(120.0f, ImGui::GetContentRegionAvail().y - listH)), ImGuiChildFlags_Borders);
    const float sz = 72, pad = 6;
    int perRow = std::max(1, (int)((ImGui::GetContentRegionAvail().x + pad) / (sz + 10 + pad)));
    int col = 0;
    for (int i : MenuOrder()) {
        const BuildingType& b = all[i];
        if (g_onlySelected && !g_houseSel[i]) continue;
        if (!passes(b)) continue;
        if (g_filter[0] && b.name.find(g_filter) == std::string::npos && b.id.find(g_filter) == std::string::npos) continue;
        if (col) ImGui::SameLine(0, pad);
        ImGui::PushID(i);
        bool sel = g_houseSel[i] != 0;
        void* tex = Icon(b.id);
        ImGui::PushStyleColor(ImGuiCol_Button, sel ? ImVec4(0.573f, 0.176f, 0.149f, 1) : ImVec4(0.85f, 0.80f, 0.70f, 1));
        bool click = tex ? ImGui::ImageButton("ic", (ImTextureID)(intptr_t)tex, ImVec2(sz, sz))
                         : ImGui::Button(b.name.c_str(), ImVec2(sz + 8, sz + 8));
        ImGui::PopStyleColor();
        if (click) ToggleHouse(i, !sel);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("%s\n%d жит. · %.0fx%.0f м · %s"), b.name.c_str(), b.capacity, b.sx, b.sz, MaterialName(b.material));
        ImGui::PopID();
        col = (col + 1) % perRow;
    }
    ImGui::EndChild();
    ImGui::Text(T("Выбрано домов: %d"), nSel);
    ImGui::BeginChild("sel", ImVec2(0, 0));
    for (size_t i = 0; i < g_houseSel.size(); ++i) if (g_houseSel[i]) {
        ImGui::PushID((int)i);
        if (void* t = Icon(all[i].id)) { ImGui::Image((ImTextureID)(intptr_t)t, ImVec2(26, 26)); ImGui::SameLine(); }
        ImGui::SetNextItemWidth(58);
        if (ImGui::DragInt("##share", &g_houseShare[i], 1.0f, 0, 100, "%d")) { g_houseShare[i] = std::clamp(g_houseShare[i], 0, 100); g_dirty = true; }
        ImGui::SetItemTooltip(T("Вес этого типа. Потяните мышью или дважды кликните и введите число.\nСправа — получившаяся доля среди домов района."));
        ImGui::SameLine(); ImGui::Text("%3d %%  %s — %d", shareSum > 0 ? (int)lroundf(100.0f * g_houseShare[i] / shareSum) : 0, all[i].name.c_str(), all[i].capacity);
        ImGui::SameLine(); if (ImGui::SmallButton("x")) ToggleHouse((int)i, false);
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void TabArea() {
    if (ImGui::Checkbox(T("English interface"), &i18n::en)) { catalog::SetEnglish(i18n::en); g_dirty = true; }
    ImGui::SetItemTooltip(T("Язык интерфейса и названий зданий (названия берутся из файлов игры)."));
    ImGui::SeparatorText(T("Территория"));
    ImGui::BeginDisabled(!eng::Ready());
    if (ImGui::Button(g_pick ? T("Отменить рисование") : T("Нарисовать на карте"), ImVec2(210, 0))) { g_pick = !g_pick; g_draft.clear(); }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (g_area.ok) {
        ImGui::Text(T("%.1f га, %d точек"), g_area.sqm / 10000.0f, (int)g_area.pts.size());
        ImGui::SameLine();
        if (ImGui::SmallButton(T("Сбросить"))) { g_area.ok = false; g_dirty = g_areaChanged = true; }
    } else ImGui::TextDisabled(T("участок не выбран"));
    ImGui::TextDisabled(T("Кликайте по карте углы района любой формы, замкните кликом по первой точке."));
    ImGui::SetNextItemWidth(220);
    if (ImGui::SliderFloat(T("поворот застройки, °"), &g_angle, -90.0f, 90.0f, "%.0f") && g_area.ok) {
        BuildFrame(g_area, true); g_dirty = g_areaChanged = true;
    }

    ImGui::SeparatorText(T("Рельеф"));
    ImGui::Checkbox(T("показывать крутые склоны (оранжевый > 12 %, красный > 25 %)"), &g_showSlope);
    ImGui::Checkbox(T("показывать выравнивание: объёмные здания и жёлтые линии среза / насыпи"), &g_showLevel);
    ImGui::SetNextItemWidth(220);
    if (ImGui::SliderFloat(T("допустимый перепад под зданием, м"), &g_maxDiff, 0.5f, 12.0f, "%.1f")) g_dirty = true;

    ImGui::SeparatorText(T("Население"));
    ImGui::SetNextItemWidth(220);
    if (ImGui::InputInt(T("жителей"), &g_population, 100, 1000)) { g_population = std::clamp(g_population, 10, 200000); g_dirty = true; }
    ImGui::SetNextItemWidth(220);
    if (ImGui::SliderInt(T("плотность застройки, %"), &g_density, 0, 100)) g_dirty = true;
    ImGui::SetItemTooltip(T("Сжимает промежутки между зданиями (14 → 3 м) и полосы дорожек между рядами (9 → 5 м).\nНа большой плотности игре может не хватить места под площадки у входов."));
    if (ImGui::Checkbox(T("застроить весь участок"), &g_fillAll)) g_dirty = true;
    ImGui::SetItemTooltip(T("Дома ставятся, пока помещаются, без оглядки на число жителей выше."));

    ImGui::SeparatorText(T("Выбранные дома"));
    const auto& all = catalog::All();
    int n = 0;
    for (size_t i = 0; i < g_houseSel.size(); ++i) if (g_houseSel[i]) {
        ImGui::BulletText(T("%s — %d жит."), all[i].name.c_str(), all[i].capacity); ++n;
    }
    if (!n) ImGui::TextColored(ImVec4(0.65f, 0.1f, 0.1f, 1), T("Не выбрано ни одного дома — откройте вкладку «Дома»."));
}

void TabHouses() {
    const auto& all = catalog::All();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##filter", T("поиск по названию"), g_filter, sizeof(g_filter));
    ImGui::SameLine(); ImGui::Checkbox(T("только выбранные"), &g_onlySelected);
    // filter: how many residents, which wall material (the material decides what the building site needs)
    ImGui::SetNextItemWidth(220);
    ImGui::DragIntRange2(T("жителей в доме"), &g_resMin, &g_resMax, 1.0f, 0, 1000, T("от %d"), T("до %d"));
    const char* mats[] = { T("любой материал"), T("кирпич"), T("панели"), T("дерево") };
    static const char* matKeys[] = { "", "brick", "panel", "wood" };
    ImGui::SameLine(); ImGui::SetNextItemWidth(150); ImGui::Combo("##mat", &g_matFilter, mats, IM_ARRAYSIZE(mats));
    auto passes = [&](const BuildingType& b) {
        if (b.capacity < g_resMin || b.capacity > g_resMax) return false;
        if (g_matFilter > 0 && b.material.find(matKeys[g_matFilter]) == std::string::npos) return false;
        return true;
    };
    ImGui::SameLine();
    if (ImGui::SmallButton(T("Выбрать по фильтру"))) {
        for (int i : g_houseOrder) if (passes(catalog::All()[i])) g_houseSel[i] = 1; else g_houseSel[i] = 0;
        g_autoHouse = false;
        g_dirty = true;
    }
    ImGui::SetItemTooltip(T("Отмечает все дома, подходящие под фильтр, и снимает остальные."));
    ImGui::SameLine();
    if (ImGui::SmallButton(T("Снять все"))) { std::fill(g_houseSel.begin(), g_houseSel.end(), 0); g_autoHouse = false; g_dirty = true; }
    if (ImGui::RadioButton(T("иконки"), g_houseIcons)) g_houseIcons = true;
    ImGui::SameLine(); if (ImGui::RadioButton(T("таблица"), !g_houseIcons)) g_houseIcons = false;
    if (g_houseIcons) { HouseIconGrid(passes); return; }
    const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                  ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable;
    int shareSum = 0;
    for (size_t i = 0; i < g_houseSel.size(); ++i) if (g_houseSel[i]) shareSum += std::max(0, g_houseShare[i]);
    if (ImGui::BeginTable("houses", 8, flags, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(T("Дом"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(T("Доля"), ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 112);
        ImGui::TableSetupColumn(T("Жителей"), ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending, 70);
        ImGui::TableSetupColumn(T("Размер, м"), ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn(T("Кач."), ImGuiTableColumnFlags_WidthFixed, 46);
        ImGui::TableSetupColumn(T("Материал"), ImGuiTableColumnFlags_WidthFixed, 78);
        ImGui::TableSetupColumn(T("Стиль"), ImGuiTableColumnFlags_WidthFixed, 96);
        ImGui::TableSetupColumn(T("м³ на жит."), ImGuiTableColumnFlags_WidthFixed, 74);
        ImGui::TableHeadersRow();
        ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
        if (g_houseOrder.empty() || (specs && specs->SpecsDirty)) { SortHouses(specs); if (specs) specs->SpecsDirty = false; }
        for (int i : g_houseOrder) {
            const BuildingType& b = all[i];
            if (g_onlySelected && !g_houseSel[i]) continue;
            if (!passes(b)) continue;
            if (g_filter[0] && b.name.find(g_filter) == std::string::npos && b.id.find(g_filter) == std::string::npos) continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID(i);
            bool sel = g_houseSel[i] != 0;
            if (ImGui::Checkbox(b.name.c_str(), &sel)) ToggleHouse(i, sel);
            ImGui::SetItemTooltip("%s", b.id.c_str());
            ImGui::TableNextColumn();
            if (sel) {      // share of this type among the houses of the district
                ImGui::SetNextItemWidth(58);
                if (ImGui::DragInt("##share", &g_houseShare[i], 1.0f, 0, 100, "%d")) { g_houseShare[i] = std::clamp(g_houseShare[i], 0, 100); g_dirty = true; }
                ImGui::SetItemTooltip(T("Вес этого типа. Потяните мышью или дважды кликните и введите число.\nСправа — получившаяся доля среди домов района."));
                ImGui::SameLine(); ImGui::Text("%d %%", shareSum > 0 ? (int)lroundf(100.0f * g_houseShare[i] / shareSum) : 0);
            }
            ImGui::PopID();
            ImGui::TableNextColumn(); ImGui::Text("%d", b.capacity);
            ImGui::TableNextColumn(); ImGui::Text("%.0fx%.0f", b.sx, b.sz);
            ImGui::TableNextColumn(); if (b.quality > 0) ImGui::Text("%.2f", b.quality); else ImGui::TextDisabled("-");
            ImGui::TableNextColumn(); ImGui::TextUnformatted(MaterialName(b.material));
            ImGui::TableNextColumn(); ImGui::TextUnformatted(b.style.empty() ? "-" : b.style.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.0f", HouseVolume(b) / std::max(1, b.capacity));
            ImGui::SetItemTooltip(T("Объём здания на одного жителя — чем меньше, тем дешевле стройка на человека\n(игра считает стоимость по стенам и крыше модели)."));
        }
        ImGui::EndTable();
    }
}

void TabServices() {
    const auto& all = catalog::All();
    ImGui::TextDisabled(T("Что строить в квартале и на сколько жителей одно здание."));
    if (ImGui::BeginTable("services", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(T("Что строить"), ImGuiTableColumnFlags_WidthFixed, 170);
        ImGui::TableSetupColumn(T("Здание"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(T("1 на N жит."), ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableHeadersRow();
        for (auto& s : g_services) {
            ImGui::PushID((int)(&s - g_services.data()));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(s.typeIndex < 0);
            if (ImGui::Checkbox(T(s.label), &s.enabled)) g_dirty = true;
            if (s.typeIndex >= 0) ImGui::SetItemTooltip("%s", all[s.typeIndex].id.c_str());
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##type", s.typeIndex >= 0 ? all[s.typeIndex].name.c_str() : T("нет в игре"))) {
                for (size_t i = 0; i < all.size(); ++i) {
                    if (all[i].category != s.category) continue;
                    char label[200]; snprintf(label, sizeof(label), T("%s  (%.0fx%.0f м)##%d"), all[i].name.c_str(), all[i].sx, all[i].sz, (int)i);
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
    if (job.lines == 0) snprintf(t, sizeof(t), T("%s: нечего строить%s"), what, job.skipped ? T(" (всё за экраном или за холмом)") : "");
    else if (job.skipped) snprintf(t, sizeof(t), T("Проложить в игре %s — %d отрезков (не видно ещё %d)"), what, job.lines, job.skipped);
    else snprintf(t, sizeof(t), T("Проложить в игре %s — %d отрезков"), what, job.lines);
    ImGui::BeginDisabled(job.lines == 0 || eng::AutoBuildActive());
    if (ImGui::Button(t, ImVec2(-FLT_MIN, 0))) { ImVec2 ds = ImGui::GetIO().DisplaySize; eng::StartScript(job.pos, job.action, job.frames, ds.x, ds.y); }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(T("Сначала выберите в игровом меню «Транспортная отрасль» %s.\nМод сам протянет каждый отрезок игровым инструментом: двигает настоящий курсор и зажимает кнопку.\nПанель на это время прячется, мышь не трогайте. Esc — остановить. Платит игра.\nОтрезок к зданию начинается точно в его точке подключения, чтобы игра к ней привязалась.\nПорядок: сначала улицы и подъезды, потом дорожки.\nОтрезки, концы которых за экраном, под панелями игры или закрыты холмом, пропускаются —\nподнимите камеру выше и смотрите на участок сверху. Камеру во время работы не двигайте."), tool);
}

// Everything of the plan placed at once by direct calls into the game (the mechanism behind the game's own
// "paste area"): streets, buildings, driveways, footpaths. Buildings the game refuses because the ground is
// uneven get their pad levelled and one more try.
struct InstantRun {
    int phase = 0;                      // 0 idle, 1 first batch, 2 levelling, 3 retry batch, 4 gap repair
    int repaired = 0;                   // gap repair rounds done (up to 3, each tries other links)
    std::vector<eng::InstantJob> jobs;
    std::vector<int> plan;              // per job: index of the plan building, -1 for lines
    std::vector<int> retry;             // plan buildings waiting for levelling
    std::vector<eng::InstantJob> lineRetry;     // utility lines that failed while some buildings were still missing
    std::vector<eng::InstantJob> built;         // road / footpath pieces the game accepted (checked in its network)
    int bOk = 0, bAll = 0, rOk = 0, rAll = 0, pOk = 0, pAll = 0, nOk = 0, nAll = 0;
    std::string note;
} g_inst;
void InstantLine(int kind, int lineType, float u1, float v1, float u2, float v2, float h1 = 0, float h2 = 0) {
    float len = sqrtf((u2 - u1) * (u2 - u1) + (v2 - v1) * (v2 - v1));
    if (len < 0.5f) return;
    int n = std::max(1, (int)ceilf(len / 60.0f));     // pieces of at most 60 m: the game refuses a long wire / pipe in one piece (straight start tangents keep the joints from bending)
    for (int i = 0; i < n; ++i) {
        float t0 = (float)i / n, t1 = (float)(i + 1) / n;
        eng::InstantJob j; j.kind = kind; j.lineType = lineType;
        j.a = LocalToWorld(g_area, u1 + (u2 - u1) * t0, v1 + (v2 - v1) * t0, 0);
        j.b = LocalToWorld(g_area, u1 + (u2 - u1) * t1, v1 + (v2 - v1) * t1, 0);
        if (i == 0) j.ha = h1;
        if (i == n - 1) j.hb = h2;
        g_inst.jobs.push_back(j); g_inst.plan.push_back(-1);
    }
}
// The game joins lines end to end only: a piece that stops on the middle of another one just lies on top of it.
// So every line is cut wherever another line of the set ends on it or crosses it, and all pieces share end points.
// Returns the pieces with the index of the line each came from.
std::vector<std::pair<PathSeg, int>> Noded(const std::vector<PathSeg>& in) {
    std::vector<std::pair<PathSeg, int>> out;
    for (size_t i = 0; i < in.size(); ++i) {
        const PathSeg& s = in[i];
        float du = s.u2 - s.u1, dv = s.v2 - s.v1, len2 = du * du + dv * dv, len = sqrtf(len2);
        if (len < 0.05f) continue;
        std::vector<float> cuts;
        auto onMe = [&](float u, float v) {          // a point of another line lying on this one, away from its ends
            float t = ((u - s.u1) * du + (v - s.v1) * dv) / len2;
            if (t * len < 0.5f || (1 - t) * len < 0.5f) return;
            float pu = s.u1 + du * t - u, pv = s.v1 + dv * t - v;
            if (pu * pu + pv * pv < 0.5f * 0.5f) cuts.push_back(t);
        };
        for (size_t j = 0; j < in.size(); ++j) {
            if (j == i) continue;
            const PathSeg& o = in[j];
            onMe(o.u1, o.v1); onMe(o.u2, o.v2);
            float eu = o.u2 - o.u1, ev = o.v2 - o.v1, den = du * ev - dv * eu;      // a real crossing
            if (fabsf(den) < 1e-4f) continue;
            float t = ((o.u1 - s.u1) * ev - (o.v1 - s.v1) * eu) / den, w = ((o.u1 - s.u1) * dv - (o.v1 - s.v1) * du) / den;
            if (w > 0 && w < 1 && t * len > 0.5f && (1 - t) * len > 0.5f) cuts.push_back(t);
        }
        std::sort(cuts.begin(), cuts.end());
        float prev = 0;
        for (size_t k = 0; k <= cuts.size(); ++k) {
            float t = k < cuts.size() ? cuts[k] : 1.0f;
            if (k < cuts.size() && (t - prev) * len < 0.5f) continue;          // two neighbours ending on almost the same spot share one cut
            out.push_back({ PathSeg{ s.u1 + du * prev, s.v1 + dv * prev, s.u1 + du * t, s.v1 + dv * t }, (int)i });
            prev = t;
        }
    }
    return out;
}
eng::InstantJob InstantBuilding(const Placed& b) {
    eng::InstantJob j; j.kind = 0; j.type = catalog::All()[b.type].id;
    j.a = LocalToWorld(g_area, b.ou, b.ov, 0);
    j.rot = g_buildRotSign * (atan2f(g_area.uz, g_area.ux) + b.rot * kPi / 2);
    return j;
}
void InstantStart() {
    g_inst = InstantRun();
    g_gapPts.clear(); g_gapTried.clear();
    std::vector<PathSeg> rd;        // streets and driveways cut at each other's ends, so they join
    for (auto& s : g_plan.roads) rd.push_back({ s.u1, s.v1, s.u2, s.v2 });
    const int nStreets = (int)rd.size();
    rd.insert(rd.end(), g_plan.drives.begin(), g_plan.drives.end());
    const auto roadPieces = Noded(rd);
    // order: buildings (stops included), then streets - whole, cut only by length and at the stops, whose road
    // connections they snap to - then driveways (cut where they meet each other, joined to the middle of the street),
    // footpaths, and last the utility lines
    for (size_t i = 0; i < g_plan.buildings.size(); ++i) {
        const Placed& b = g_plan.buildings[i];
        V3 w = LocalToWorld(g_area, b.u, b.v, 0);
        if (eng::Occupied(w.x, w.z) & 1) continue;      // already there
        g_inst.jobs.push_back(InstantBuilding(b)); g_inst.plan.push_back((int)i);
    }
    for (int k = 0; k < nStreets; ++k) InstantLine(1, g_roadKind, rd[k].u1, rd[k].v1, rd[k].u2, rd[k].v2);
    for (auto& pc : roadPieces) if (pc.second >= nStreets) InstantLine(1, g_roadKind, pc.first.u1, pc.first.v1, pc.first.u2, pc.first.v2);
    // footpaths: the lines that run along a street at the pavement distance are the street's own pavements when the
    // street has them; the rest is built
    auto alongStreet = [&](const PathSeg& p) {
        if (g_roadKind != 2) return false;
        for (auto& rd : g_plan.roads) {
            bool horiz = rd.v1 == rd.v2, phoriz = p.v1 == p.v2;
            if (horiz != phoriz) continue;
            float off = horiz ? fabsf(p.v1 - rd.v1) : fabsf(p.u1 - rd.u1);
            if (off > rd.width / 2 + 0.5f && off < rd.width / 2 + 3.5f) return true;
        }
        return false;
    };
    // trunk first (row footpaths and the links between rows and to the street), then the links to the entrances,
    // so every entrance joins a line that already stands
    {
        const auto pieces = Noded(g_plan.paths);
        auto isLink = [&](int src) {
            const auto& p = g_plan.paths[src];
            for (auto& q : g_plan.pedPts) if (fabsf(q.u - p.u1) + fabsf(q.v - p.v1) < 0.5f || fabsf(q.u - p.u2) + fabsf(q.v - p.v2) < 0.5f) return true;
            return false;
        };
        for (int pass = 0; pass < 2; ++pass)
            for (auto& pc : pieces) {
                if (pc.second < (int)g_plan.pathBuild.size() && !g_plan.pathBuild[pc.second]) continue;
                if (alongStreet(pc.first) || isLink(pc.second) != (pass == 1)) continue;
                InstantLine(2, g_pathKind - 1, pc.first.u1, pc.first.v1, pc.first.u2, pc.first.v2);
            }
    }
    for (auto& s : g_plan.utilLines) {      // after the buildings, so the ends snap to their connections
        // manager offset, type, sub-type, start argument - as logged from the game's own tools
        const int pw = g_powerUnder ? 20 : 10;
        // net 4: high voltage (feed of the district's own transformer)
        const int net[5][4] = { { 0xfaf0, 20, 0, -20 }, { 0xfaf0, 20, 5, -20 }, { 0xfa98, 10, 0, -20 }, { 0xfa20, pw, 0, pw }, { 0xfa18, 10, 0, 10 } };
        size_t from = g_inst.jobs.size();
        InstantLine(3, net[s.net][1], s.u1, s.v1, s.u2, s.v2, s.h1, s.h2);
        for (size_t k = from; k < g_inst.jobs.size(); ++k) { g_inst.jobs[k].mgr = net[s.net][0]; g_inst.jobs[k].sub = net[s.net][2]; g_inst.jobs[k].arg = net[s.net][3]; }
    }
    if (g_inst.jobs.empty()) { g_inst.note = T("Нечего строить."); return; }
    eng::QueueInstant(g_inst.jobs);
    g_inst.phase = 1;
}
// After a build: which entrances really reach the street over what the game accepted. The net = built streets,
// driveways and footpaths (plus the street edges people walk on); pieces touch when an end lies on another piece.
// Entrances outside the street's net are gaps: logged and marked on the map.
void CheckGaps(const InstantRun& r) {
    g_gapPts.clear(); g_gapFix.clear();
    struct S { V3 a, b; bool road; };
    std::vector<S> segs;
    for (auto& j : r.built) segs.push_back({ j.a, j.b, j.kind == 1 });
    // a stop is a pass-through piece of the street that people walk through
    for (auto& b : g_plan.buildings) if (b.stop) {
        V3 w = LocalToWorld(g_area, b.ou, b.ov, 0);
        if (!(eng::Occupied(w.x, w.z) & 1)) continue;
        segs.push_back({ LocalToWorld(g_area, b.ou - b.w / 2 - 1, b.ov, 0), LocalToWorld(g_area, b.ou + b.w / 2 + 1, b.ov, 0), true });
        segs.push_back({ LocalToWorld(g_area, b.ou, b.ov - b.d / 2 - 1, 0), LocalToWorld(g_area, b.ou, b.ov + b.d / 2 + 1, 0), true });
        for (int sd = -1; sd <= 1; sd += 2)    // its edges, where footpaths end
            segs.push_back({ LocalToWorld(g_area, b.u - b.w / 2 - 1, b.v + sd * (b.d / 2 + 1), 0), LocalToWorld(g_area, b.u + b.w / 2 + 1, b.v + sd * (b.d / 2 + 1), 0), true });
    }
    if (segs.empty()) return;
    auto onSeg = [](const S& s, float x, float z, float tol) {
        float dx = s.b.x - s.a.x, dz = s.b.z - s.a.z, l2 = dx * dx + dz * dz;
        float t = l2 > 1e-6f ? std::clamp(((x - s.a.x) * dx + (z - s.a.z) * dz) / l2, 0.0f, 1.0f) : 0.0f;
        return hypotf(s.a.x + dx * t - x, s.a.z + dz * t - z) < tol;
    };
    std::vector<int> par(segs.size());
    for (size_t k = 0; k < par.size(); ++k) par[k] = (int)k;
    std::function<int(int)> root = [&](int x) { return par[x] == x ? x : par[x] = root(par[x]); };
    for (size_t a = 0; a < segs.size(); ++a)
        for (size_t b = a + 1; b < segs.size(); ++b) {
            bool joined;
            if (segs[a].road || segs[b].road) {         // a footpath joins a street only when drawn onto it
                const float tol = 4.0f;
                joined = onSeg(segs[b], segs[a].a.x, segs[a].a.z, tol) || onSeg(segs[b], segs[a].b.x, segs[a].b.z, tol) ||
                         onSeg(segs[a], segs[b].a.x, segs[b].a.z, tol) || onSeg(segs[a], segs[b].b.x, segs[b].b.z, tol);
            } else {                                    // footpaths join end to end only (the game's own rule)
                auto closeTo = [](const V3& p, const V3& q) { return hypotf(p.x - q.x, p.z - q.z) < 1.5f; };
                joined = closeTo(segs[a].a, segs[b].a) || closeTo(segs[a].a, segs[b].b) || closeTo(segs[a].b, segs[b].a) || closeTo(segs[a].b, segs[b].b);
            }
            if (joined) par[root((int)a)] = root((int)b);
        }
    int mainRoot = -1;
    for (size_t k = 0; k < segs.size(); ++k) if (segs[k].road) { mainRoot = root((int)k); break; }
    if (mainRoot < 0) {
        std::vector<int> cnt(segs.size(), 0); for (size_t k = 0; k < segs.size(); ++k) ++cnt[root((int)k)];
        for (size_t k = 0; k < segs.size(); ++k) if (mainRoot < 0 || cnt[root((int)k)] > cnt[mainRoot]) mainRoot = root((int)k);
    }
    const auto& all = catalog::All();
    int checked = 0;
    for (auto& b : g_plan.buildings) {
        if (b.stop) continue;
        if (!(eng::Occupied(LocalToWorld(g_area, b.u, b.v, 0).x, LocalToWorld(g_area, b.u, b.v, 0).z) & 1)) continue;   // the building is not there
        // a building is joined when any of its entrances is (the link goes to the cheapest one)
        bool has = false, ok = false; V3 first{};
        for (auto& c : all[b.type].conns) {
            if (c.kind != 0) continue;
            float cs[4][2] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };     // quarter turns, as the planner turns models
            float cu = c.x * cs[b.rot & 3][0] - c.z * cs[b.rot & 3][1], cv = c.x * cs[b.rot & 3][1] + c.z * cs[b.rot & 3][0];
            V3 w = LocalToWorld(g_area, b.ou + cu, b.ov + cv, 0);
            if (!has) first = w;
            has = true;
            for (size_t k = 0; k < segs.size() && !ok; ++k) if (root((int)k) == mainRoot && onSeg(segs[k], w.x, w.z, 3.0f)) ok = true;
        }
        if (!has) continue;
        ++checked;
        if (!ok) { g_gapPts.push_back(first); Log("gap: no entrance of '%s' at %.1f %.1f is joined to the street", all[b.type].id.c_str(), first.x, first.z); }
    }
    Log("gap check: %d entrances checked, %d not joined, %d built pieces", checked, (int)g_gapPts.size(), (int)segs.size());
    {   // debug: how the built net falls apart
        std::map<int, int> sz, roads;
        for (size_t k = 0; k < segs.size(); ++k) { ++sz[root((int)k)]; if (segs[k].road) ++roads[root((int)k)]; }
        std::string t;
        for (auto& kv : sz) { char b[48]; snprintf(b, sizeof(b), " [%d pieces, %d road%s]", kv.second, roads[kv.first], kv.first == mainRoot ? ", main" : ""); t += b; }
        Log("gap check: parts of the net:%s", t.c_str());
        for (size_t k = 0; k < segs.size() && k < 400; ++k) if (segs[k].road) Log("gap check: road piece %.1f %.1f -> %.1f %.1f part %d", segs[k].a.x, segs[k].a.z, segs[k].b.x, segs[k].b.z, root((int)k));
    }
    // Repair plan: one footpath per cut-off piece of net (or per lone entrance), from its point nearest to the
    // street's net, straight, not through a building of the plan. Targets: the street net's footpaths and the edge of
    // its carriageway (a footpath meeting a street joins its pavement).
    auto blocked = [&](V3 p, V3 q) {
        float len = hypotf(q.x - p.x, q.z - p.z);
        for (float t = 2.5f; t < len - 1.0f; t += 1.0f) {
            V3 m{ p.x + (q.x - p.x) * t / len, 0, p.z + (q.z - p.z) * t / len };
            V2 l = WorldToLocal(g_area, m);
            for (auto& bb : g_plan.buildings) if (fabsf(l.u - bb.u) < bb.w / 2 + 3.0f && fabsf(l.v - bb.v) < bb.d / 2 + 6.0f) return true;   // the zone with its apron: the game cuts a path there
        }
        return false;
    };
    std::vector<V3> targets;
    for (size_t k = 0; k < segs.size(); ++k) {
        if (root((int)k) != mainRoot) continue;
        const S& sg = segs[k];
        if (!sg.road) { targets.push_back({ sg.a.x, 0, sg.a.z }); targets.push_back({ sg.b.x, 0, sg.b.z }); continue; }   // footpaths join at piece ends only
        float len = hypotf(sg.b.x - sg.a.x, sg.b.z - sg.a.z);
        for (float t = 0; t <= len + 0.01f; t += 4.0f) {
            V3 p{ sg.a.x + (sg.b.x - sg.a.x) * std::min(t, len) / std::max(len, 0.01f), 0, sg.a.z + (sg.b.z - sg.a.z) * std::min(t, len) / std::max(len, 0.01f) };
            if (!sg.road) { targets.push_back(p); continue; }
            float nx = -(sg.b.z - sg.a.z) / std::max(len, 0.01f), nz = (sg.b.x - sg.a.x) / std::max(len, 0.01f), off = 0.0f;   // onto the street itself: only that joins
            targets.push_back({ p.x + nx * off, 0, p.z + nz * off });
            targets.push_back({ p.x - nx * off, 0, p.z - nz * off });
        }
    }
    std::vector<int> done;
    for (auto& w : g_gapPts) {
        int comp = -1;
        for (size_t k = 0; k < segs.size(); ++k) if (onSeg(segs[k], w.x, w.z, 3.0f)) { comp = root((int)k); break; }
        if (comp >= 0 && std::find(done.begin(), done.end(), comp) != done.end()) continue;
        std::vector<V3> src;
        if (comp < 0) src.push_back(w);
        else {
            done.push_back(comp);
            for (size_t k = 0; k < segs.size(); ++k) {
                if (root((int)k) != comp) continue;
                const S& sg = segs[k]; float len = hypotf(sg.b.x - sg.a.x, sg.b.z - sg.a.z);
                for (float t = 0; t <= len + 0.01f; t += 4.0f) src.push_back({ sg.a.x + (sg.b.x - sg.a.x) * std::min(t, len) / std::max(len, 0.01f), 0, sg.a.z + (sg.b.z - sg.a.z) * std::min(t, len) / std::max(len, 0.01f) });
            }
        }
        float bd = 1e9f; V3 bs{}, bt{};
        for (auto& p : src) for (auto& q : targets) {
            float d = hypotf(q.x - p.x, q.z - p.z);
            if (d < 5.5f || d >= bd || d > 120) continue;          // the game refuses footpaths under ~5 m
            bool tried = false;
            for (auto& tr : g_gapTried) if (hypotf(tr.first.x - p.x, tr.first.z - p.z) + hypotf(tr.second.x - q.x, tr.second.z - q.z) < 8) { tried = true; break; }
            if (tried || blocked(p, q)) continue;
            bd = d; bs = p; bt = q;
        }
        if (bd < 1e8f) g_gapFix.push_back({ bs, bt });
        else Log("gap repair: no free straight way for the piece at %.1f %.1f", w.x, w.z);
    }
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
        } else if (j.kind == 1) { ++r.rAll; if (codes[i] == 1) { ++r.rOk; r.built.push_back(j); } }
        else if (j.kind == 3) { if (r.phase == 1) ++r.nAll; if (codes[i] == 1) ++r.nOk; else if (r.phase == 1) failedLines.push_back(j); }
        else { ++r.pAll; if (codes[i] == 1) { ++r.pOk; r.built.push_back(j); } }
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
    CheckGaps(r);
    if (r.repaired < 3 && !g_gapFix.empty()) {
        // repair: every entrance left out gets one straight footpath to the nearest point of the net that stands,
        // then the check runs again
        ++r.repaired;
        for (auto& fx : g_gapFix) g_gapTried.push_back(fx);
        r.jobs.clear(); r.plan.clear();
        for (auto& fx : g_gapFix) {
            eng::InstantJob j; j.kind = 2; j.lineType = g_pathKind - 1; j.a = fx.first; j.b = fx.second;
            r.jobs.push_back(j); r.plan.push_back(-1);
            Log("gap repair: footpath %.1f %.1f -> %.1f %.1f", j.a.x, j.a.z, j.b.x, j.b.z);
        }
        eng::QueueInstant(r.jobs);
        r.phase = 4;
        return;
    }
    char t[260];
    snprintf(t, sizeof(t), T("Построено (проверено по сети игры): зданий %d из %d, дорог %d из %d отрезков, дорожек %d из %d отрезков, сетей %d из %d отрезков."), r.bOk, r.bAll, r.rOk, r.rAll, r.pOk, r.pAll, r.nOk, r.nAll);
    r.note = t;
    if (!g_gapPts.empty()) { char g[160]; snprintf(g, sizeof(g), T(" Разрывы: %d вход(ов) не связаны с улицей — отмечены красным."), (int)g_gapPts.size()); r.note += g; }
    r.phase = 0;
    Log("instant build summary: buildings %d/%d, roads %d/%d, footpaths %d/%d, utility lines %d/%d", r.bOk, r.bAll, r.rOk, r.rAll, r.pOk, r.pAll, r.nOk, r.nAll);
}
void InstantUi() {
    InstantTick();
    InstantRun& r = g_inst;
    if (r.phase) {
        ImGui::BeginDisabled();
        ImGui::Button(r.phase == 2 ? T("Выравниваю площадки под отклонённые здания…") : T("Строю…"), ImVec2(-FLT_MIN, 0));
        ImGui::EndDisabled();
        return;
    }
    {   // one switch for all networks; the Сети tab has each one, its node buildings and their radii
        bool anyU = g_utilBuild[0] || g_utilBuild[1] || g_utilBuild[2] || g_utilBuild[3];
        if (ImGui::Checkbox(T("строить коммуникации (вода, канализация, отопление, электричество)"), &anyU)) {
            for (int k = 0; k < 4; ++k) g_utilBuild[k] = g_utilShow[k] = anyU;
            g_dirty = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("Узлы: водораспределитель, канализационная станция, тепловой пункт, подстанция;\nпо отдельности и с радиусами — во вкладке «Сети». Уже стоящие узлы рядом учитываются."));
    }
    char t[160]; snprintf(t, sizeof(t), T("Построить всё сразу — %d зданий, улицы, подъезды, дорожки"), (int)g_plan.buildings.size());
    ImGui::BeginDisabled(g_plan.buildings.empty() && g_plan.paths.empty());
    if (ImGui::Button(t, ImVec2(-FLT_MIN, 0))) InstantStart();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(T("Весь план ставится в игру одним действием, тем же способом, каким игра вставляет скопированную зону:\nничего выбирать в меню стройки и кликать не нужно, камера может стоять как угодно.\nПорядок: улицы, здания, подъезды, дорожки. Игра сама проверяет каждое место и берёт плату\nв текущем режиме стройки. Где земля слишком неровная, мод выравнивает площадку и повторяет."));
    if (!r.note.empty()) ImGui::TextWrapped("%s", r.note.c_str());
}

void TabRoads() {
    if (eng::InstantSupported()) {
        ImGui::SeparatorText(T("Покрытие"));
        const char* rk[] = { T("гравийная дорога"), T("асфальтовая дорога"), T("асфальтовая с тротуарами") };
        const char* pk[] = { T("тропинка"), T("гравийная дорожка"), T("асфальтовая дорожка") };
        ImGui::SetNextItemWidth(280); ImGui::Combo(T("улицы и подъезды"), &g_roadKind, rk, 3);
        ImGui::SetNextItemWidth(280); ImGui::Combo(T("дорожки"), &g_pathKind, pk, 3);
        ImGui::TextDisabled(T("Используется кнопкой «Построить всё сразу» внизу панели."));
        ImGui::TextDisabled(T("Зелёные точки — пешеходные входы зданий, оранжевые квадраты — подъезды для машин."));
        const ImVec4 red(0.65f, 0.1f, 0.1f, 1);
        if (g_plan.noRoad) ImGui::TextColored(red, g_plan.roads.empty() ? T("Зданиям нужна автодорога: %d — включите улицу ниже.") : T("Не удалось подвести автодорогу к зданиям: %d."), g_plan.noRoad);
        if (g_plan.noPath) ImGui::TextColored(red, T("Не удалось подвести дорожку к зданиям: %d."), g_plan.noPath);
    } else if (g_area.ok && !g_plan.buildings.empty()) {
        ImGui::SeparatorText(T("Построить в игре"));
        if (eng::AutoBuildActive()) {
            int done, failed, left; eng::AutoBuildStats(done, failed, left);
            char t[120]; snprintf(t, sizeof(t), T("Стоп  ·  кликов сделано %d, шагов осталось %d"), done, left);
            if (ImGui::Button(t, ImVec2(-FLT_MIN, 0))) eng::StopAutoBuild();
        } else {
            LineJob paths, roads, drives;
            for (auto& ps : g_plan.paths) AddLine(paths, ps.u1, ps.v1, ps.u2, ps.v2);
            for (auto& rd : g_plan.roads) AddLine(roads, rd.u1, rd.v1, rd.u2, rd.v2);
            for (auto& rd : g_plan.drives) AddLine(drives, rd.u1, rd.v1, rd.u2, rd.v2);
            if (!g_plan.roads.empty()) LineButton(T("улицы"), T("тип дороги"), roads);
            if (!g_plan.drives.empty()) LineButton(T("подъезды к зданиям"), T("тип дороги (подъезд можно попроще)"), drives);
            LineButton(T("дорожки"), T("тип пешеходной дорожки"), paths);
        }
        ImGui::TextDisabled(T("Зелёные точки — пешеходные входы зданий, оранжевые квадраты — подъезды для машин."));
        const ImVec4 red(0.65f, 0.1f, 0.1f, 1);
        if (g_plan.noRoad) ImGui::TextColored(red, g_plan.roads.empty() ? T("Зданиям нужна автодорога: %d — включите улицу ниже.") : T("Не удалось подвести автодорогу к зданиям: %d."), g_plan.noRoad);
        if (g_plan.noPath) ImGui::TextColored(red, T("Не удалось подвести дорожку к зданиям: %d."), g_plan.noPath);
    }
    ImGui::SeparatorText(T("Улица через район"));
    const char* types[] = { T("без дороги (только дорожки)"), T("улица, 2 полосы"), T("проспект, 4 полосы") };
    ImGui::SetNextItemWidth(280);
    if (ImGui::Combo(T("тип дороги"), &g_roadType, types, 3)) g_dirty = true;
    if (ImGui::Checkbox(T("вторая улица поперёк (перекрёсток в середине)"), &g_crossRoad)) g_dirty = true;
    if (ImGui::SliderFloat(T("улица через каждые, м (0 — одна)"), &g_streetGap, 0.0f, 600.0f, "%.0f")) { if (g_streetGap < 80) g_streetGap = 0; g_dirty = true; }
    ImGui::TextDisabled(T("Вдоль улицы идут тротуары, дорожки кварталов сходятся к переходу у остановки."));

    ImGui::SeparatorText(T("Остановки"));
    {
        const auto& all = catalog::All();
        auto nameOf = [&](const char* id) -> std::string { for (auto& t : all) if (t.id == id) return t.name; return id; };
        std::string cur = kStopIds[g_stopKind][0] ? nameOf(kStopIds[g_stopKind]) : std::string(T("только отметить место"));
        ImGui::SetNextItemWidth(280);
        if (ImGui::BeginCombo(T("вид остановки"), cur.c_str())) {
            for (int k = 0; k < kStopCount; ++k) {
                std::string nm = kStopIds[k][0] ? nameOf(kStopIds[k]) : std::string(T("только отметить место"));
                if (ImGui::Selectable(nm.c_str(), k == g_stopKind)) { g_stopKind = k; g_dirty = true; }
            }
            ImGui::EndCombo();
        }
        ImGui::SetItemTooltip(T("Остановка ставится прямо на улицу: дорога проходит сквозь неё.\nТроллейбусной нужна контактная сеть, трамвайной — рельсы на улице:\nвыберите им подходящее покрытие улицы ниже."));
    }
    ImGui::BeginDisabled(g_roadType == 0 || !g_area.ok);
    int removeAt = -1;
    for (size_t i = 0; i < g_stops.size(); ++i) {
        ImGui::PushID((int)i);
        ImGui::BulletText(T("Остановка %d"), (int)i + 1);
        if (g_stops.size() > 1) { ImGui::SameLine(); if (ImGui::SmallButton(T("убрать"))) removeAt = (int)i; }
        ImGui::PopID();
    }
    if (removeAt >= 0) { g_stops.erase(g_stops.begin() + removeAt); g_dirty = true; }
    if (ImGui::Button(T("Добавить остановку")) && g_stops.size() < 8) {
        float L = g_area.minU, R = g_area.maxU;
        PolyScan(g_area.local, g_roadV, L, R);
        g_stops.push_back(L + (R - L) * (0.25f + 0.5f * (float)(g_stops.size() % 2)));
        g_dirty = true;
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled(T("Остановку (синий кружок на карте) можно перетащить мышью:\nулица пройдёт через неё, район перестроится."));

    ImGui::SeparatorText(T("Пешая доступность"));
    ImGui::SetNextItemWidth(280);
    if (ImGui::SliderFloat(T("сколько идут до остановки, м"), &g_walk, 150.0f, 900.0f, "%.0f")) g_dirty = true;
    ImGui::TextDisabled(T("Дома дальше этого расстояния обводятся красным."));
}

void TabUtils() {
    ImGui::TextWrapped(T("По правилам игры: дома берут воду, тепло и свет от узла в радиусе обслуживания, к самим домам линии не нужны. Труба соединяет только два здания, поэтому узлы висят на распределителях, а те идут цепочкой; провод среднего напряжения идёт от трансформатора прямо к подстанции (до 6 на трансформатор). Начало каждой цепочки и высокую сторону трансформатора подключаете вы — к своему источнику. Радиусы взяты из кода игры: вода, канализация и электричество 250 м, отопление 380 м (по прямой)."));
    ImGui::Spacing();
    int people = g_plan.capacity;
    for (int k = 0; k < 4; ++k) {
        ImGui::PushID(k);
        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImGui::ColorConvertU32ToFloat4(kUtilCol[k]));
        if (ImGui::Checkbox(T(kUtilName[k]), &g_utilBuild[k])) { g_dirty = true; g_utilShow[k] = g_utilBuild[k]; }
        ImGui::PopStyleColor();
        ImGui::SameLine(); ImGui::Checkbox(T("показывать"), &g_utilShow[k]);
        if (k == 3) { ImGui::SameLine(); ImGui::Checkbox(T("кабель под землёй"), &g_powerUnder); ImGui::SameLine(); if (ImGui::Checkbox(T("своя подстанция"), &g_powerTrafo)) g_dirty = true; }
        if (ImGui::Checkbox(T("учитывать уже стоящие узлы (свои или поставлю сам)"), &g_utilExisting[k])) g_dirty = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("Узлы этой сети, которые уже стоят рядом, обслуживают дома в своём радиусе —\nтам план новых узлов не ставит. Чтобы поставить узел самому, поставьте его и пересоберите план."));
        ImGui::SetNextItemWidth(200);
        if (ImGui::SliderFloat(T("радиус узла, м"), &g_utilRadius[k], 60.0f, 600.0f, "%.0f")) g_dirty = true;
        ImGui::SetNextItemWidth(120);
        if (k < 2) ImGui::TextDisabled(T("Нагрузка по формуле игры: жилой дом — жители × 0,045; службы — (рабочие + специалисты) × 0,02."));
        else ImGui::TextDisabled(k == 2 ? T("Нагрузка по формуле игры: жилой дом — жители × 0,7; службы — по рабочим и посетителям.")
                                          : T("Нагрузка по формуле игры: жилой дом — жители × 0,002 МВт; службы — по рабочим и посетителям, не меньше 0,05 МВт."));
        if (g_utilBuild[k] && g_area.ok && !g_plan.buildings.empty()) {
            int nodes = (int)g_plan.utilNodes[k].size(), helpers = 0;
            for (auto& b : g_plan.buildings) if (b.net == k && b.role == 1) ++helpers;
            ImGui::Text(T("Узлов (%s): %d  ·  %s: %d  ·  линий %.0f м  ·  нагрузка %.0f %s"), T(kUtilNode[k]), nodes,
                        k == 3 ? T("трансформаторов") : T("распределителей"), helpers, g_plan.utilLen[k], g_plan.utilDemand[k], T(kUtilUnit[k]));
            if (g_plan.utilUncovered[k]) ImGui::TextColored(ImVec4(0.65f, 0.1f, 0.1f, 1), T("Вне радиуса зданий: %d."), g_plan.utilUncovered[k]);
            if (k >= 2 && nodes > 0) {
                float cap = 0; for (auto& t : catalog::All()) { if (k == 2 && t.id == "heating_endstation_small") cap = t.heatCap; if (k == 3 && t.id == "eletric_substation") cap = t.powerCap; }
                ImGui::Text(T("Спрос района %.*f %s · самый загруженный узел %.0f %% (игра предупреждает с 90 %%, ёмкость узла %g)"),
                            k == 3 ? 2 : 0, g_plan.utilDemand[k], T(kUtilUnit[k]), g_plan.utilMaxLoad[k] * 100, cap);
                if (g_plan.utilOverloaded[k]) ImGui::TextColored(ImVec4(0.65f, 0.1f, 0.1f, 1), T("Перегружено узлов: %d."), g_plan.utilOverloaded[k]);
                if (k == 3) {   // what the district actually draws at night: services add the lighting half
                    float night = 0;
                    for (auto& b : g_plan.buildings) night += catalog::All()[b.type].powerNight;
                    ImGui::TextDisabled(T("Ночью район берёт из сети до %.2f МВт (освещение служб); ёмкость узлов это не меняет — учитывайте для электростанции."), night);
                }
            }
            if (g_plan.utilNoLine[k]) ImGui::TextColored(ImVec4(0.65f, 0.1f, 0.1f, 1), T("Не удалось проложить соединений: %d."), g_plan.utilNoLine[k]);
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    if (ImGui::Checkbox(T("связать узлы трубами через распределители (опыт: игра гнёт такие трубы петлями)"), &g_utilChain)) g_dirty = true;
    ImGui::TextDisabled(T("Отмеченные сети ставятся вместе со всем планом кнопкой мгновенной постройки. Каждый узел воды, канализации и тепла подключите к своей магистрали сами."));
}

void Footer() {
    ImGui::Separator();
    const ImVec4 red(0.65f, 0.1f, 0.1f, 1);
    if (!g_area.ok) ImGui::TextDisabled(T("Нарисуйте участок на карте."));
    else if (g_plan.buildings.empty()) ImGui::TextColored(red, T("Ничего не поместилось: отметьте дома, увеличьте участок или допустимый перепад."));
    else {
        ImGui::Text(T("Домов %d  ·  мест %d из %d  ·  служб %d"), g_plan.houses, g_plan.capacity, g_population, g_plan.services);
        ImGui::Text(T("Дорог %.0f м  ·  дорожек %.0f м  ·  срезать %.0f м³  ·  насыпать %.0f м³"), g_plan.roadLength, g_plan.pathLength, g_plan.cut, g_plan.fill);
        ImGui::Text(T("Рабочих мест в службах %d  ·  работников ≈ %d (60 %% жителей)"), g_plan.jobs, g_plan.capacity * 6 / 10);
        if (g_plan.maxWalkToStop >= 0) {
            if (g_plan.farHouses) ImGui::TextColored(red, T("До остановки до %.0f м — %d домов дальше %.0f м, добавьте остановку."), g_plan.maxWalkToStop, g_plan.farHouses, g_walk);
            else ImGui::Text(T("До остановки не дальше %.0f м  ·  до магазина не дальше %.0f м"), g_plan.maxWalkToStop, std::max(0.0f, g_plan.maxWalkToShop));
        } else if (g_plan.maxWalkToShop >= 0) ImGui::Text(T("До магазина не дальше %.0f м"), g_plan.maxWalkToShop);
        {   // Soviet micro-district norms (SNiP II-60-75): shop 400, kindergarten 350, school 500 m
            const char* kName[3] = { T("магазина"), T("детсада"), T("школы") };
            static const float kR[3] = { 400, 350, 500 };
            for (int k = 0; k < 3; ++k) if (g_plan.normWalk[k] >= 0) {
                if (g_plan.normFar[k]) ImGui::TextColored(red, T("До %s до %.0f м: %d домов дальше нормы %.0f м"), kName[k], g_plan.normWalk[k], g_plan.normFar[k], kR[k]);
                else ImGui::Text(T("До %s не дальше %.0f м (норма %.0f м)"), kName[k], g_plan.normWalk[k], kR[k]);
            }
        }
        if (g_plan.unplaced > 0) ImGui::TextColored(red, T("Вмещается около %d жит., не влезло зданий: %d — нужен участок больше."), g_plan.capacity, g_plan.unplaced);
        else if (g_plan.steepSkipped > 0 || g_plan.busySkipped > 0) ImGui::TextDisabled(T("Обойдено: крутые места %d раз, существующая застройка %d раз."), g_plan.steepSkipped, g_plan.busySkipped);
    }
    if (g_polOk) {
        if (g_polA > 0.001f || g_polB > 0.001f) ImGui::TextColored(ImVec4(0.65f, 0.1f, 0.1f, 1), T("Загрязнение воздуха над участком: %.3f / %.3f (сетка игры, 200 м) — жителям вредно."), g_polA, g_polB);
        else ImGui::TextDisabled(T("Воздух над участком чистый (сетка загрязнения игры)."));
    }
    if (ImGui::Button(T("Пересобрать план"), ImVec2(-FLT_MIN, 0))) {
        if (!g_area.ok) snprintf(g_rebuildNote, sizeof(g_rebuildNote), T("Сначала нарисуйте участок на карте."));
        else { g_dirty = g_areaChanged = true; ++g_rebuilds; g_rebuildNote[0] = 0; }
    }
    if (g_rebuildNote[0]) ImGui::TextDisabled("%s", g_rebuildNote);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(T("Заново читает рельеф и расставляет здания, улицу и дорожки по всему участку."));
    static bool levelling = false;
    int pending = eng::LevelPending();
    if (levelling && !pending) { levelling = false; g_dirty = g_areaChanged = true; }
    bool canLevel = g_area.ok && !g_plan.buildings.empty() && !pending;
    ImGui::BeginDisabled(!canLevel);
    char lbl[96];
    if (pending) snprintf(lbl, sizeof(lbl), T("Выравниваю… осталось точек: %d"), pending);
    else snprintf(lbl, sizeof(lbl), T("Выровнять землю под зданиями (сразу, бесплатно)"));
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
        ImGui::SetTooltip(T("Землю под зданиями плана сразу ставит на высоту площадок. Бесплатно, без техники; отменить нельзя — сохранитесь заранее."));
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
            char t[160]; snprintf(t, sizeof(t), T("Стоп  ·  поставлено %d, не вышло %d, осталось %d"), done, failed, left);
            if (ImGui::Button(t, ImVec2(-FLT_MIN, 0))) { eng::StopAutoBuild(); seq = false; }
        } else if (seq) {
            if (seqRunning) { seqRunning = false; seqDone.push_back(seqType); seqType = -1; }      // one type finished
            else if (types.empty()) seq = false;
            else {
                if (std::find(types.begin(), types.end(), seqType) == types.end()) { seqType = types[0]; seqSince = GetTickCount(); }
                ImGui::TextColored(ImVec4(0.573f, 0.176f, 0.149f, 1), T("Выберите в меню стройки игры: %s"), all[seqType].name.c_str());
                ImGui::SetItemTooltip(T("Обычно мод сам замечает выбор и начинает ставить. Если не заметил — нажмите «Уже выбрано — ставить»."));
                DWORD tick = 0;
                std::string cur = lower(eng::SelectedType(&tick));
                bool cursor = eng::CursorActive();
                if (cursor && tick > seqSince && cur == lower(all[seqType].id)) { if (start(seqType)) seqRunning = true; }
                float w3 = (ImGui::GetContentRegionAvail().x - 16) / 3;
                ImGui::BeginDisabled(!cursor);
                if (ImGui::Button(T("Уже выбрано — ставить"), ImVec2(w3 * 1.4f, 0))) { if (start(seqType)) seqRunning = true; }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button(T("Пропустить"), ImVec2(w3 * 0.8f, 0))) { seqDone.push_back(seqType); seqType = -1; }
                ImGui::SameLine();
                if (ImGui::Button(T("Стоп"), ImVec2(-FLT_MIN, 0))) seq = false;
            }
        } else if (!types.empty() || !seqDone.empty()) {
            char t[160];
            if (types.empty()) snprintf(t, sizeof(t), T("Повторить для пропущенных и не вставших зданий"));
            else snprintf(t, sizeof(t), T("Построить в игре всё по очереди — %d зданий"), total);
            if (ImGui::Button(t, ImVec2(-FLT_MIN, 0))) { seq = !types.empty(); seqRunning = false; seqType = -1; seqDone.clear(); }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip(T("Поставьте камеру так, чтобы весь участок был на экране, и не двигайте её.\nМод по очереди называет здание — выберите его в игровом меню строительства.\nДальше он сам поворачивает здание как на плане, наводит игровой курсор и кликает\nво всех местах этого типа, затем называет следующее. Платит игра как обычно.\nНа неровном месте мод удерживает кнопку, чтобы игра выровняла площадку."));
        } else if (!g_plan.buildings.empty()) ImGui::TextDisabled(T("Все здания плана уже стоят в игре."));
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
    const char* title = T(":: Планировщик квартала ::");
    ImVec2 ts = ImGui::CalcTextSize(title);
    dl->AddText(ImVec2(wp.x + (ww - ts.x) / 2, wp.y + (hh - ts.y) / 2), IM_COL32(255, 244, 222, 255), title);
    ImGui::SetCursorPos(ImVec2(ww - 32, 6));
    if (ImGui::InvisibleButton("##close", ImVec2(24, 24))) g_open = false;
    ImU32 xc = ImGui::IsItemHovered() ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 230, 200, 200);
    ImVec2 c(wp.x + ww - 20, wp.y + 18);
    dl->AddLine(ImVec2(c.x - 6, c.y - 6), ImVec2(c.x + 6, c.y + 6), xc, 2.5f);
    dl->AddLine(ImVec2(c.x - 6, c.y + 6), ImVec2(c.x + 6, c.y - 6), xc, 2.5f);
    ImGui::SetCursorPos(ImVec2(ImGui::GetStyle().WindowPadding.x, hh + 8));

    if (!eng::Ready()) ImGui::TextColored(ImVec4(0.65f, 0.1f, 0.1f, 1), T("Загрузите карту или сохранение, чтобы выбрать участок."));

    // the summary and the build buttons below the tabs grew past a fixed height and were cut off: they get their own
    // scrolling area, about 40 % of the window
    float footer = std::max(180.0f, ImGui::GetContentRegionAvail().y * 0.42f);
    if (ImGui::BeginTabBar("##tabs")) {
        struct { const char* name; void (*fn)(); } tabs[] = { { T("Участок"), TabArea }, { T("Дома"), TabHouses }, { T("Службы"), TabServices }, { T("Дороги"), TabRoads }, { T("Сети"), TabUtils } };
        for (auto& t : tabs) {
            if (!ImGui::BeginTabItem(t.name)) continue;
            ImGui::BeginChild("##body", ImVec2(0, std::max(120.0f, ImGui::GetContentRegionAvail().y - footer)));
            t.fn();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::BeginChild("##footer", ImVec2(0, 0), ImGuiChildFlags_None);
    Footer();
    ImGui::EndChild();
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
    ImGui::SetItemTooltip(T("Планировщик квартала (F8). Можно перетащить."));
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
    if (GetFileAttributesW((GameDir() + L"DistrictPlanner.darkness").c_str()) != INVALID_FILE_ATTRIBUTES) eng::LogDarkness();
    if (GetFileAttributesW((GameDir() + L"DistrictPlanner.replan").c_str()) != INVALID_FILE_ATTRIBUTES) { DeleteFileW((GameDir() + L"DistrictPlanner.replan").c_str()); g_dirty = true; }
    std::wstring dlFlag = GameDir() + L"DistrictPlanner.dumpload";
    if (GetFileAttributesW(dlFlag.c_str()) != INVALID_FILE_ATTRIBUTES) { DeleteFileW(dlFlag.c_str()); eng::DebugDumpLoad(); }
    std::wstring stFlag = GameDir() + L"DistrictPlanner.straight";
    if (GetFileAttributesW(stFlag.c_str()) != INVALID_FILE_ATTRIBUTES) {
        int m = 1; FILE* f = _wfopen(stFlag.c_str(), L"r"); if (f) { fscanf(f, "%d", &m); fclose(f); }
        DeleteFileW(stFlag.c_str()); eng::SetStraightLines(m != 0);
    }
    std::wstring smFlag = GameDir() + L"DistrictPlanner.snapmode";     // debug: footpath snap mode for A/B tests
    if (GetFileAttributesW(smFlag.c_str()) != INVALID_FILE_ATTRIBUTES) {
        int m = 1; FILE* f = _wfopen(smFlag.c_str(), L"r"); if (f) { fscanf(f, "%d", &m); fclose(f); }
        DeleteFileW(smFlag.c_str()); eng::SetPathSnapMode(m);
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
    int stMode = 1; { FILE* f = _wfopen(flag.c_str(), L"r"); if (f) { fscanf(f, "%d", &stMode); fclose(f); } }   // 2 = keep the services, 3 = also bigger (450 m, 2000 people)
    const float side = stMode == 3 ? 450.0f : 300.0f;
    DeleteFileW(flag.c_str());
    g_selfTest = 2;
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    const float sx[6] = { 0.40f, 0.66f, 0.80f, 0.74f, 0.52f, 0.42f }, sy[6] = { 0.42f, 0.40f, 0.52f, 0.80f, 0.86f, 0.66f };
    std::vector<V3> pts;
    for (int i = 0; i < 6; ++i) { V3 p; if (eng::ScreenToTerrain(ds.x * sx[i], ds.y * sy[i], ds.x, ds.y, p)) pts.push_back(p); }
    Log("selftest: display %.0fx%.0f, points hit %d of 6", ds.x, ds.y, (int)pts.size());
    if (pts.size() < 6) return;
    {   // prefer a free, fairly flat 300 m square near the view: earlier test districts may still stand in front of the camera
        V3 c{ 0, 0, 0 }; for (auto& q : pts) { c.x += q.x / 6; c.z += q.z / 6; }
        bool found = false;
        for (int ring = 0; ring < 30 && !found; ++ring)
            for (int a = -ring; a <= ring && !found; ++a) for (int b = -ring; b <= ring && !found; ++b) {
                if (std::max(abs(a), abs(b)) != ring) continue;
                float x0 = c.x + a * side / 2, z0 = c.z + b * side / 2, lo = 1e9f, hi = -1e9f; bool ok = true;
                for (float x = -20; x <= side + 20 && ok; x += 20) for (float z = -20; z <= side + 20 && ok; z += 20) {
                    if (eng::Occupied(x0 + x, z0 + z)) ok = false;
                    float h = eng::TerrainHeight(x0 + x, z0 + z); lo = std::min(lo, h); hi = std::max(hi, h);
                }
                if (!ok || hi - lo > 12 || lo < 1) continue;
                pts = { { x0, 0, z0 }, { x0 + side, 0, z0 }, { x0 + side, 0, z0 + side }, { x0, 0, z0 + side } };
                for (auto& q : pts) q.y = eng::TerrainHeight(q.x, q.z);
                Log("selftest: free square at %.0f %.0f (ring %d, height %.0f..%.0f)", x0, z0, ring, lo, hi);
                found = true;
            }
    }
    g_population = stMode == 3 ? 2000 : 600; g_open = true; g_showLevel = false;
    g_crossRoad = stMode == 3;
    {   // test district: only a building that is unlocked from the start, no services
        const auto& all = catalog::All();
        for (size_t i = 0; i < all.size(); ++i) g_houseSel[i] = all[i].id == "residental_brick_2";
        for (auto& sv : g_services) sv.enabled = stMode >= 2 && sv.typeIndex >= 0;
        for (int k = 0; k < 4; ++k) { g_utilBuild[k] = g_utilShow[k] = true; }
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
    {   // test helper: a file DistrictPlanner.hide closes the panel and its overlay (screenshots of the bare game)
        static DWORD lastHide = 0;
        if (GetTickCount() - lastHide > 1000) { lastHide = GetTickCount();
            if (DeleteFileW((GameDir() + L"DistrictPlanner.hide").c_str())) g_open = !g_open; }
    }
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
