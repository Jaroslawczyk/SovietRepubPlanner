// In-game panel: territory picking, house / service selection, plan preview drawn over the terrain.
#include "common.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>

namespace {

bool g_open = false;
int  g_pick = 0;                    // 0 none, 1 first corner, 2 second corner, 3 depth
V3   g_a{}, g_b{};

struct Area { bool ok = false; V3 o{}; float ux = 1, uz = 0, vx = 0, vz = 1, w = 0, d = 0; } g_area;

std::vector<char> g_houseSel;       // per catalog index
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
bool  g_dirty = true, g_inited = false;
char  g_filter[64] = "";

PlanResult g_plan;
struct Quad { V3 c[4]; V3 mid; int type; };
std::vector<Quad> g_quads;
std::vector<std::vector<V3>> g_lines, g_outline;

V3 ToWorld(const Area& a, float u, float v) {
    V3 p{ a.o.x + a.ux * u + a.vx * v, 0, a.o.z + a.uz * u + a.vz * v };
    p.y = eng::TerrainHeight(p.x, p.z) + 0.3f;
    return p;
}
std::vector<V3> Sample(const Area& a, float u1, float v1, float u2, float v2) {
    float len = sqrtf((u2 - u1) * (u2 - u1) + (v2 - v1) * (v2 - v1));
    int n = std::max(1, (int)(len / 10.0f));
    std::vector<V3> pts;
    for (int i = 0; i <= n; ++i) { float t = (float)i / n; pts.push_back(ToWorld(a, u1 + (u2 - u1) * t, v1 + (v2 - v1) * t)); }
    return pts;
}
std::vector<std::vector<V3>> Outline(const Area& a) {
    return { Sample(a, 0, 0, a.w, 0), Sample(a, a.w, 0, a.w, a.d), Sample(a, a.w, a.d, 0, a.d), Sample(a, 0, a.d, 0, 0) };
}

void InitDefaults() {
    const auto& all = catalog::All();
    g_houseSel.assign(all.size(), 0);
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

void Regenerate() {
    g_dirty = false;
    g_quads.clear(); g_lines.clear(); g_outline.clear();
    g_plan = PlanResult();
    if (!g_area.ok) return;
    PlanInput in;
    in.width = g_area.w; in.depth = g_area.d; in.population = g_population; in.gap = g_gap;
    for (size_t i = 0; i < g_houseSel.size(); ++i) if (g_houseSel[i]) in.houseTypes.push_back((int)i);
    in.services = &g_services;
    g_plan = MakePlan(in);
    for (auto& b : g_plan.buildings) {
        Quad q; q.type = b.type;
        float hw = b.w / 2, hd = b.d / 2;
        q.c[0] = ToWorld(g_area, b.u - hw, b.v - hd); q.c[1] = ToWorld(g_area, b.u + hw, b.v - hd);
        q.c[2] = ToWorld(g_area, b.u + hw, b.v + hd); q.c[3] = ToWorld(g_area, b.u - hw, b.v + hd);
        q.mid = ToWorld(g_area, b.u, b.v);
        g_quads.push_back(q);
    }
    for (auto& p : g_plan.paths) g_lines.push_back(Sample(g_area, p.u1, p.v1, p.u2, p.v2));
    g_outline = Outline(g_area);
}

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

void DrawPolyline(ImDrawList* dl, const std::vector<V3>& pts, ImU32 col, float thick) {
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    bool have = false; ImVec2 prev;
    for (auto& p : pts) {
        float sx, sy;
        if (!eng::WorldToScreen(p, ds.x, ds.y, sx, sy)) { have = false; continue; }
        if (have) dl->AddLine(prev, ImVec2(sx, sy), col, thick);
        prev = ImVec2(sx, sy); have = true;
    }
}

void DrawWorld() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    const auto& all = catalog::All();
    for (auto& l : g_outline) DrawPolyline(dl, l, IM_COL32(255, 235, 80, 230), 2.5f);
    for (auto& l : g_lines)   DrawPolyline(dl, l, IM_COL32(235, 235, 235, 220), 2.0f);
    for (auto& q : g_quads) {
        ImVec2 s[4]; bool ok = true;
        for (int i = 0; i < 4 && ok; ++i) ok = eng::WorldToScreen(q.c[i], ds.x, ds.y, s[i].x, s[i].y);
        if (!ok) continue;
        const BuildingType& b = all[q.type];
        dl->AddQuadFilled(s[0], s[1], s[2], s[3], CategoryColor(b.category, 90));
        dl->AddQuad(s[0], s[1], s[2], s[3], CategoryColor(b.category, 255), 1.5f);
        float minX = std::min(std::min(s[0].x, s[1].x), std::min(s[2].x, s[3].x));
        float maxX = std::max(std::max(s[0].x, s[1].x), std::max(s[2].x, s[3].x));
        ImVec2 ts = ImGui::CalcTextSize(b.name.c_str());
        float mx, my;
        if (ts.x < (maxX - minX) && eng::WorldToScreen(q.mid, ds.x, ds.y, mx, my)) {
            dl->AddText(ImVec2(mx - ts.x / 2 + 1, my - ts.y / 2 + 1), IM_COL32(0, 0, 0, 220), b.name.c_str());
            dl->AddText(ImVec2(mx - ts.x / 2, my - ts.y / 2), IM_COL32(255, 255, 255, 255), b.name.c_str());
        }
    }
}

void Picking() {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (ImGui::IsMouseClicked(1)) { g_pick = 0; return; }
    V3 hit;
    bool ok = !io.WantCaptureMouse && eng::ScreenToTerrain(io.MousePos.x, io.MousePos.y, io.DisplaySize.x, io.DisplaySize.y, hit);
    const char* hint = g_pick == 1 ? "Клик — первый угол участка" : g_pick == 2 ? "Клик — второй угол (ширина)" : "Клик — глубина участка";
    char buf[160]; snprintf(buf, sizeof(buf), "%s   (ПКМ — отмена)", hint);

    Area tmp;
    if (ok && g_pick >= 2) {
        V3 b = g_pick == 2 ? hit : g_b;
        float dx = b.x - g_a.x, dz = b.z - g_a.z, w = sqrtf(dx * dx + dz * dz);
        if (w > 1.0f) {
            tmp.o = g_a; tmp.ux = dx / w; tmp.uz = dz / w; tmp.w = w;
            float nx = -tmp.uz, nz = tmp.ux;
            float d = g_pick == 3 ? (hit.x - g_a.x) * nx + (hit.z - g_a.z) * nz : 0;
            if (d < 0) { nx = -nx; nz = -nz; d = -d; }
            tmp.vx = nx; tmp.vz = nz; tmp.d = d; tmp.ok = true;
            if (g_pick == 2) DrawPolyline(dl, Sample(tmp, 0, 0, w, 0), IM_COL32(255, 235, 80, 255), 2.5f);
            else for (auto& l : Outline(tmp)) DrawPolyline(dl, l, IM_COL32(255, 235, 80, 255), 2.5f);
            snprintf(buf, sizeof(buf), "%s   %.0f x %.0f м   (ПКМ — отмена)", hint, tmp.w, tmp.d);
        }
    }
    ImVec2 tp(io.MousePos.x + 18, io.MousePos.y + 18), ts = ImGui::CalcTextSize(buf);
    dl->AddRectFilled(ImVec2(tp.x - 5, tp.y - 3), ImVec2(tp.x + ts.x + 5, tp.y + ts.y + 3), IM_COL32(0, 0, 0, 190), 4);
    dl->AddText(tp, IM_COL32(255, 255, 255, 255), buf);

    if (ok && ImGui::IsMouseClicked(0)) {
        if (g_pick == 1) { g_a = hit; g_pick = 2; }
        else if (g_pick == 2 && tmp.ok) { g_b = hit; g_pick = 3; }
        else if (g_pick == 3 && tmp.ok && tmp.d > 20 && tmp.w > 20) { g_area = tmp; g_pick = 0; g_dirty = true; }
    }
}

void Panel() {
    const auto& all = catalog::All();
    ImGui::SetNextWindowSize(ImVec2(520, 900), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(60, 110), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Планировщик квартала", &g_open)) { ImGui::End(); return; }

    if (!eng::Ready()) ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1), "Загрузите карту или сохранение, чтобы выбрать участок.");

    ImGui::SeparatorText("Территория");
    ImGui::BeginDisabled(!eng::Ready());
    if (ImGui::Button(g_pick ? "Выбор на карте…" : "Выбрать на карте")) g_pick = g_pick ? 0 : 1;
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (g_area.ok) {
        ImGui::Text("%.0f x %.0f м  (%.1f га)", g_area.w, g_area.d, g_area.w * g_area.d / 10000.0f);
        ImGui::SameLine();
        if (ImGui::SmallButton("Сбросить")) { g_area.ok = false; g_dirty = true; }
    } else ImGui::TextDisabled("участок не выбран");

    ImGui::SeparatorText("Население");
    if (ImGui::InputInt("Жителей", &g_population, 100, 1000)) { g_population = std::clamp(g_population, 10, 200000); g_dirty = true; }
    if (ImGui::SliderFloat("Отступ между зданиями, м", &g_gap, 3.0f, 30.0f, "%.0f")) g_dirty = true;

    ImGui::SeparatorText("Жилые дома");
    ImGui::SetNextItemWidth(180);
    ImGui::InputTextWithHint("##filter", "поиск", g_filter, sizeof(g_filter));
    ImGui::SameLine();
    if (ImGui::SmallButton("Снять все")) { std::fill(g_houseSel.begin(), g_houseSel.end(), 0); g_dirty = true; }
    if (ImGui::BeginTable("houses", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV, ImVec2(0, 190))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Дом", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Жителей", ImGuiTableColumnFlags_WidthFixed, 62);
        ImGui::TableSetupColumn("Размер, м", ImGuiTableColumnFlags_WidthFixed, 74);
        ImGui::TableSetupColumn("Кач.", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < all.size(); ++i) {
            const BuildingType& b = all[i];
            if (b.category != "LIVING" || b.capacity <= 0) continue;
            if (g_filter[0] && b.name.find(g_filter) == std::string::npos && b.id.find(g_filter) == std::string::npos) continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID((int)i);
            bool sel = g_houseSel[i] != 0;
            if (ImGui::Checkbox(b.name.c_str(), &sel)) { g_houseSel[i] = sel; g_dirty = true; }
            ImGui::SetItemTooltip("%s", b.id.c_str());
            ImGui::PopID();
            ImGui::TableNextColumn(); ImGui::Text("%d", b.capacity);
            ImGui::TableNextColumn(); ImGui::Text("%.0fx%.0f", b.sx, b.sz);
            ImGui::TableNextColumn(); if (b.quality > 0) ImGui::Text("%.2f", b.quality); else ImGui::TextDisabled("-");
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Нужды жителей");
    if (ImGui::BeginTable("services", 3, ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Что строить", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Здание", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("1 на N жит.", ImGuiTableColumnFlags_WidthFixed, 92);
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

    ImGui::SeparatorText("План");
    if (!g_area.ok) ImGui::TextDisabled("Выберите участок на карте.");
    else if (g_plan.buildings.empty()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Ничего не поместилось: отметьте дома или увеличьте участок.");
    else {
        ImGui::Text("Домов: %d, мест: %d из %d", g_plan.houses, g_plan.capacity, g_population);
        ImGui::Text("Зданий обслуживания: %d", g_plan.services);
        ImGui::Text("Дорожек: %.0f м", g_plan.pathLength);
        if (g_plan.maxWalkToShop >= 0) ImGui::Text("Самый дальний путь до магазина: %.0f м", g_plan.maxWalkToShop);
        if (g_plan.unplaced > 0)
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Не поместилось зданий: %d. Увеличьте участок\nили выберите дома вместительнее.", g_plan.unplaced);
    }
    ImGui::BeginDisabled(true);
    ImGui::Button("Построить в игре");
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Автоматическая постройка ещё не готова: пока мод показывает план на местности.");
    ImGui::End();
}

// Debug aid: if DistrictPlanner.selftest exists next to the game exe, build a district in front of the
// camera without any input and write the numbers to the log.
int g_selfTest = 0;
void SelfTest() {
    static int frames = 0;
    if (g_selfTest == 0)
        g_selfTest = GetFileAttributesW((GameDir() + L"DistrictPlanner.selftest").c_str()) != INVALID_FILE_ATTRIBUTES ? 1 : -1;
    if (g_selfTest != 1 || !eng::Ready() || ++frames < 240) return;
    g_selfTest = 2;
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    V3 a, b, c;
    bool ok = eng::ScreenToTerrain(ds.x * 0.38f, ds.y * 0.50f, ds.x, ds.y, a) &&
              eng::ScreenToTerrain(ds.x * 0.62f, ds.y * 0.50f, ds.x, ds.y, b) &&
              eng::ScreenToTerrain(ds.x * 0.55f, ds.y * 0.74f, ds.x, ds.y, c);
    Log("selftest: display %.0fx%.0f, hit=%d", ds.x, ds.y, (int)ok);
    if (!ok) return;
    float sx = 0, sy = 0;
    bool back = eng::WorldToScreen(a, ds.x, ds.y, sx, sy);
    Log("selftest: a=(%.1f %.1f %.1f) b=(%.1f %.1f %.1f) c=(%.1f %.1f %.1f)", a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z);
    Log("selftest: a back to screen %d (%.1f %.1f), expected (%.1f %.1f)", (int)back, sx, sy, ds.x * 0.38f, ds.y * 0.50f);
    float dx = b.x - a.x, dz = b.z - a.z, w = sqrtf(dx * dx + dz * dz);
    if (w < 1) return;
    Area t; t.o = a; t.ux = dx / w; t.uz = dz / w; t.w = w;
    float nx = -t.uz, nz = t.ux, d = (c.x - a.x) * nx + (c.z - a.z) * nz;
    if (d < 0) { nx = -nx; nz = -nz; d = -d; }
    t.vx = nx; t.vz = nz; t.d = d; t.ok = true;
    g_area = t; g_open = true; g_population = 3000; g_dirty = true;
}

} // namespace

namespace ui {

bool WantsGameMouse() { return g_pick != 0; }

void Draw() {
    if (!g_inited && !catalog::All().empty()) InitDefaults();
    if (ImGui::IsKeyPressed(ImGuiKey_F8, false)) g_open = !g_open;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 150, 96), ImGuiCond_FirstUseEver);
    ImGui::Begin("##dp_toggle", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    if (ImGui::Button(g_open ? "Квартал  <" : "Квартал  >")) g_open = !g_open;
    ImGui::SetItemTooltip("Планировщик квартала (F8)");
    ImGui::End();

    if (!g_inited) return;
    if (!eng::Ready()) g_pick = 0;
    SelfTest();
    if (g_open) Panel();
    if (g_dirty && (eng::Ready() || !g_area.ok)) {
        Regenerate();
        if (g_selfTest == 2)
            Log("plan: area %.0fx%.0f, houses %d, capacity %d, services %d, unplaced %d, paths %.0f m, walk %.0f m",
                g_area.w, g_area.d, g_plan.houses, g_plan.capacity, g_plan.services, g_plan.unplaced, g_plan.pathLength, g_plan.maxWalkToShop);
    }
    if (eng::Ready()) {
        if (g_area.ok && g_open) DrawWorld();
        if (g_pick) Picking();
    }
}

} // namespace ui
