// In-game panel: territory picking, house / service selection, plan preview drawn over the terrain.
#include "common.h"
#include "imgui.h"
#include <cfloat>
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
    if (ImGui::Button(g_pick ? "Отменить выбор" : "Выбрать на карте", ImVec2(190, 0))) g_pick = g_pick ? 0 : 1;
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (g_area.ok) {
        ImGui::Text("%.0f x %.0f м  (%.1f га)", g_area.w, g_area.d, g_area.w * g_area.d / 10000.0f);
        ImGui::SameLine();
        if (ImGui::SmallButton("Сбросить")) { g_area.ok = false; g_dirty = true; }
    } else ImGui::TextDisabled("участок не выбран");
    ImGui::TextDisabled("Три клика по карте: угол, ширина, глубина. ПКМ — отмена.");

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
    if (ImGui::BeginTable("houses", 6, flags, ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Дом", ImGuiTableColumnFlags_WidthStretch);
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

void Footer() {
    ImGui::Separator();
    const ImVec4 red(0.65f, 0.1f, 0.1f, 1);
    if (!g_area.ok) ImGui::TextDisabled("Выберите участок на карте.");
    else if (g_plan.buildings.empty()) ImGui::TextColored(red, "Ничего не поместилось: отметьте дома или увеличьте участок.");
    else {
        ImGui::Text("Домов %d  ·  мест %d из %d  ·  служб %d", g_plan.houses, g_plan.capacity, g_population, g_plan.services);
        if (g_plan.maxWalkToShop >= 0) ImGui::Text("Дорожек %.0f м  ·  до магазина не дальше %.0f м", g_plan.pathLength, g_plan.maxWalkToShop);
        else ImGui::Text("Дорожек %.0f м", g_plan.pathLength);
        if (g_plan.unplaced > 0) ImGui::TextColored(red, "Не поместилось зданий: %d — увеличьте участок.", g_plan.unplaced);
    }
    ImGui::BeginDisabled(true);
    ImGui::Button("Построить в игре (в разработке)", ImVec2(-FLT_MIN, 0));
    ImGui::EndDisabled();
}

void Panel() {
    ImGui::SetNextWindowSize(ImVec2(640, 620), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(70, 110), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(520, 420), ImVec2(1400, 1400));
    if (!ImGui::Begin("##dp_panel", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) { ImGui::End(); return; }

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

    float footer = ImGui::GetTextLineHeightWithSpacing() * 3 + ImGui::GetFrameHeightWithSpacing() + 14;
    if (ImGui::BeginTabBar("##tabs")) {
        struct { const char* name; void (*fn)(); } tabs[] = { { "Участок", TabArea }, { "Дома", TabHouses }, { "Службы", TabServices } };
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
    if (ImGui::InvisibleButton("##toggle", ImVec2(sz, sz))) g_open = !g_open;
    bool hov = ImGui::IsItemHovered();
    ImGui::SetItemTooltip("Планировщик квартала (F8)");
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

bool WantsGameMouse() { return g_pick != 0; }

void Draw() {
    if (!g_inited && !catalog::All().empty()) InitDefaults();
    if (ImGui::IsKeyPressed(ImGuiKey_F8, false)) g_open = !g_open;

    ToggleButton();

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
