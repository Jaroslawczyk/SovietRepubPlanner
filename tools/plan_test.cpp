// Offline check of the planner: builds a plan for a rectangle with the game's own building catalogue and writes it
// as plain text for tools/plan_draw.py. Build and run with tools/plan_test.bat.
#include "../src/common.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>

std::wstring GameDir() { return L"F:\\steam\\steamapps\\common\\SovietRepublic\\"; }
std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
void Log(const char* fmt, ...) { va_list a; va_start(a, fmt); vprintf(fmt, a); va_end(a); printf("\n"); }

static int Find(const char* id) {
    const auto& all = catalog::All();
    for (size_t i = 0; i < all.size(); ++i) if (all[i].id == id) return (int)i;
    return -1;
}
int main(int argc, char** argv) {
    catalog::Load();
    float W = argc > 1 ? (float)atof(argv[1]) : 500, H = argc > 2 ? (float)atof(argv[2]) : 480;
    PlanInput in;
    in.poly = { { 0, 0 }, { W, 0 }, { W, H }, { 0, H } };
    in.population = argc > 3 ? atoi(argv[3]) : 9500;
    in.fillAll = true;
    in.gap = 1.45f; in.band = 4.85f;
    in.houseTypes = { Find("residental_brick_2") };
    if (argc > 4) { in.houseTypes.clear(); for (int k = 4; k < argc; ++k) in.houseTypes.push_back(Find(argv[k])); }
    static std::vector<ServiceRule> svc = {
        { "SHOP", "shop", true, 2000, Find("shop_prior") }, { "KINDERGARTEN", "kg", true, 2500, Find("kindergarten") },
        { "SCHOOL", "school", true, 3000, Find("school") } };
    in.services = &svc;
    in.roadType = 1; in.roadV = H / 2; in.stopsU = { W / 2 + 40 }; in.stopType = Find("busstop_big");
    if (in.stopType < 0) in.stopType = Find("bus_stop");
    in.streetSpacing = 220;
    for (int k = 0; k < 4; ++k) in.utilBuild[k] = true;
    PlanResult r = MakePlan(in);
    FILE* f = fopen("plan.txt", "w");
    fprintf(f, "A %g %g\n", W, H);
    const auto& all = catalog::All();
    for (auto& b : r.buildings) fprintf(f, "B %g %g %g %g %d %d %s\n", b.u, b.v, b.w, b.d, b.rot, b.net, all[b.type].category.c_str());
    for (size_t k = 0; k < r.paths.size(); ++k) { auto& p = r.paths[k]; fprintf(f, "P %g %g %g %g %d\n", p.u1, p.v1, p.u2, p.v2, k < r.pathBuild.size() ? (int)r.pathBuild[k] : 1); }
    for (auto& p : r.roads) fprintf(f, "R %g %g %g %g %g\n", p.u1, p.v1, p.u2, p.v2, p.width);
    for (auto& p : r.drives) fprintf(f, "D %g %g %g %g\n", p.u1, p.v1, p.u2, p.v2);
    for (auto& p : r.utilLines) fprintf(f, "U %g %g %g %g %d\n", p.u1, p.v1, p.u2, p.v2, p.net);
    for (auto& p : r.pedPts) fprintf(f, "E %g %g\n", p.u, p.v);
    fclose(f);
    printf("houses %d capacity %d services %d unplaced %d noPath %d noRoad %d cutOff %d uncovered %d %d %d %d\n", r.houses, r.capacity, r.services, r.unplaced,
           r.noPath, r.noRoad, r.cutOff, r.utilUncovered[0], r.utilUncovered[1], r.utilUncovered[2], r.utilUncovered[3]);
    return 0;
}
