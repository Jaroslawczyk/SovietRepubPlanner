// Reads building definitions from media_soviet/buildings_types and names from the language file.
#include "common.h"
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
#include <cmath>

static std::vector<BuildingType> g_types;

static std::vector<unsigned char> ReadAll(const std::wstring& path) {
    std::vector<unsigned char> d;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return d;
    DWORD sz = GetFileSize(h, nullptr), rd = 0;
    d.resize(sz);
    if (sz) ReadFile(h, d.data(), sz, &rd, nullptr);
    CloseHandle(h);
    d.resize(rd);
    return d;
}

// .btf: big-endian u32 count, u32 ?, u32 totalChars; then count * {u32 id, u32 offset, u16 len}; then UTF-16BE text.
static std::map<int, std::wstring> LoadNames(const std::wstring& path) {
    std::map<int, std::wstring> names;
    auto d = ReadAll(path);
    if (d.size() < 12) return names;
    auto be32 = [&](size_t o) { return (unsigned)(d[o] << 24 | d[o + 1] << 16 | d[o + 2] << 8 | d[o + 3]); };
    size_t count = be32(0);
    size_t text = 12 + count * 10;
    if (text > d.size()) return names;
    for (size_t i = 0; i < count; ++i) {
        size_t e = 12 + i * 10;
        int id = (int)be32(e);
        size_t off = be32(e + 4), len = (size_t)(d[e + 8] << 8 | d[e + 9]);
        size_t p = text + off * 2;
        if (p + len * 2 > d.size()) continue;
        std::wstring s;
        for (size_t k = 0; k < len; ++k) s.push_back((wchar_t)(d[p + k * 2] << 8 | d[p + k * 2 + 1]));
        names[id] = s;
    }
    return names;
}

namespace catalog {

const std::vector<BuildingType>& All() { return g_types; }

// Heat tank and power demand / capacity, as the game's type loader (0x10e1f0) computes them:
//  heat:  living = residents * 0.7 (0x1198b9); other = 0.7 * (workers + professors) + 0.49 * workers * able-to-serve
//         (0x1198f9); heat exchanger / heat pumping station = 1000, or 3000 with a big heating connection (0x1197d5).
//  power: living = residents * 0.0005 * 2, doubled again while the global flag at exe+0x9d54d4 is set (it is in a
//         normal game: 50 residents read 0.1 MW) (0x11a0d0); other = (workers + professors) * 0.0005 * (living factor
//         + lighting factor) + (workers + professors) * able-to-serve * 0.0005 * (both able-to-serve factors)
//         (0x11a10a; the lighting part also needs a global > 0 at exe+0x9d5d84); at least 0.05 MW (0x11a248).
//         Capacity: substation 2.5, transformer 19, rail trafo 14 MW (0x11a1b3).
//  Own power of heat nodes: tank / 60 x 0.006 (1000 -> 0.1 MW); $ENGINE_SPEED x 0.01 MW (small heat pumping station
//  10 -> +0.1). Water and sewage nodes have no power storage at all.
//  Not modelled: storage-based and vehicle loading terms (industry), heating plants (sum of production).
static void ComputeLoads(BuildingType& b) {
    const std::string& c = b.category;
    int wp = b.workers + b.profs;
    if (c == "LIVING") { b.heatLoad = b.capacity * 0.7f; b.powerLoad = b.capacity * 0.002f; }
    else if (c == "HEATING_ENDSTATION" || c == "HEATING_SWITCH") {
        // the node's own power: its heat tank / 60 x 0.003, doubled (0x119c2e), plus the pump (below)
        b.heatCap = b.heatBig ? 3000.0f : 1000.0f;
        b.powerLoad = b.heatCap / 60.0f * 0.003f * 2.0f;
    }
    else if (c == "SUBSTATION") b.powerCap = 2.5f;
    else if (c == "TRANSFORMATOR") b.powerCap = 19.0f;
    else if (c == "RAIL_TRAFO") b.powerCap = 14.0f;
    else if (wp > 0) {
        b.heatLoad = 0.7f * wp + 0.49f * b.workers * b.serve;
        // the "lighting" half needs exe+0x9d5d84 > 0: that is the darkness of the day/night cycle (1 at night, 0 by day,
        // read by the window and shadow shaders); building types are loaded at start-up while it is still 0, so the
        // lighting half never enters a building's capacity - it is night-time consumption only
        b.powerLoad = wp * 0.0005f * b.fLive + (b.serve > 0 ? wp * b.serve * 0.0005f * b.fLiveServe : 0.0f);
    }
    // every storage from the ini adds the largest per-resource value of its resources (loop 0x119fc1; the values are set
    // in the resource definitions 0x2a1d50, resource + transport code x 32 + 0xd8 / 0xdc): 0.05 MW for liquids (oil,
    // fuel, bitumen, liquid fertiliser - transport code 3) and for water / used water (codes 15, 16), 0 for the rest
    b.powerLoad += 0.05f * b.fuelStorages;
    // night: the per-tick draw (0x1bca60, once per worker on site) adds 0.0005 x darkness x (lighting factor +
    // able-to-serve x lighting able-to-serve factor) per worker; at full darkness that is the lighting half on top
    b.powerNight = b.powerLoad + (c != "LIVING" && wp > 0 ? wp * 0.0005f * b.fLight + (b.serve > 0 ? wp * b.serve * 0.0005f * b.fLightServe : 0.0f) : 0.0f);
    // water (0x11a46b): living = residents x 0.045; with workers = (workers + professors) x 0.02 x water factor, plus
    // workers x able-to-serve x 0.045 for hotels, orphanages and prisons; sewage the same amount (0x11a725)
    if (c == "LIVING") b.waterLoad = b.capacity * 0.045f;
    else if (wp > 0) {
        b.waterLoad = wp * 0.02f * b.fWater;
        if (c == "HOTEL" || c == "ORPHANAGE" || c == "PRISON") b.waterLoad += b.workers * b.serve * 0.045f;
    }
    b.sewageLoad = b.waterLoad;
    if (b.noHeat) b.heatLoad = 0;
    if (b.noWater) b.waterLoad = 0;
    if (b.noSewage) b.sewageLoad = 0;
    if (b.engineSpeed > 0) b.powerLoad += b.engineSpeed * 0.01f;      // pumps (0x11a1db)
    if (c != "SUBSTATION" && c != "TRANSFORMATOR" && c != "RAIL_TRAFO" && (b.powerLoad > 0 || c == "LIVING" || wp > 0)) b.powerLoad = std::max(0.05f, b.powerLoad);
    if (b.noPower) b.powerLoad = b.powerNight = 0;
    if (b.powerNight < b.powerLoad) b.powerNight = b.powerLoad;
}

void Load() {
    g_types.clear();
    std::wstring media = GameDir() + L"media_soviet\\";
    auto names = LoadNames(media + L"sovietRussian.btf");
    auto namesEn = LoadNames(media + L"sovietEnglish.btf");
    if (names.empty()) names = namesEn;
    std::wstring dir = media + L"buildings_types\\";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"*.ini").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) { Log("catalog: buildings_types not found"); return; }
    do {
        std::wstring file = fd.cFileName;
        std::wstring stem = file.substr(0, file.size() - 4);
        auto data = ReadAll(dir + file);
        if (data.empty()) continue;
        BuildingType b;
        b.id = WideToUtf8(stem);
        int nameId = -1;
        std::istringstream ss(std::string(data.begin(), data.end()));
        std::string line;
        // connections: the keyword, then its points on the next lines (bare "x y z" or $CONNECTION_ADVANCED_POINT);
        // the first point is the outer end, where a footpath / road joins
        std::vector<std::vector<V2>> connPts; std::vector<std::vector<float>> connY; std::vector<int> connKind; bool collecting = false;
        while (std::getline(ss, line)) {
            size_t s = line.find_first_not_of(" \t");
            if (s == std::string::npos) continue;
            if (line[s] != '$') {
                float x, y, z;
                if (collecting && sscanf(line.c_str() + s, "%f %f %f", &x, &y, &z) == 3) { connPts.back().push_back({ x, z }); connY.back().push_back(y); }
                else if (line.find("$CONNECTION") != std::string::npos && line.find("ADVANCED_POINT") == std::string::npos) collecting = false;
                continue;
            }
            std::istringstream ls(line.substr(s + 1));
            std::string key; ls >> key;
            if (key == "CONNECTION_ADVANCED_POINT") {
                float x, y, z;
                if (collecting && (ls >> x >> y >> z)) { connPts.back().push_back({ x, z }); connY.back().push_back(y); }
                continue;
            }
            collecting = false;
            int kind = key == "CONNECTION_PEDESTRIAN" || key == "CONNECTION_PEDESTRIAN_ALLOWPASS" ? 0
                     : key == "CONNECTION_ROAD" || key == "CONNECTION_ROAD_ALLOWPASS" || key == "CONNECTION_ROAD_INPUT" || key == "CONNECTION_ROAD_OUTPUT" ? 1
                     : key == "CONNECTION_WATERPIPE_INPUT" ? 2 : key == "CONNECTION_WATERPIPE_OUTPUT" ? 3
                     : key == "CONNECTION_SEWAGE_INPUT" ? 4 : key == "CONNECTION_SEWAGE_OUTPUT" ? 5
                     : key == "CONNECTION_HEATING_SMALL" ? 6 : key == "CONNECTION_HEATING_BIG" ? 7
                     : key == "CONNECTION_ELETRIC_LOW_INPUT" ? 8 : key == "CONNECTION_ELETRIC_LOW_OUTPUT" ? 9
                     : key == "CONNECTION_ELETRIC_HIGH_INPUT" ? 10 : key == "CONNECTION_ELETRIC_HIGH_OUTPUT" ? 11 : -1;
            if (kind >= 0) { connPts.emplace_back(); connY.emplace_back(); connKind.push_back(kind); collecting = true; continue; }
            if (key == "NAME") ls >> nameId;
            else if (key.rfind("TYPE_", 0) == 0) { if (b.category.empty()) b.category = key.substr(5); }
            else if (key == "STORAGE") {
                std::string res; int n = 0; ls >> res >> n;
                if (res == "RESOURCE_TRANSPORT_PASSANGER" && n > b.capacity) b.capacity = n;
                if (res == "RESOURCE_TRANSPORT_OIL" || res == "RESOURCE_TRANSPORT_WATER" || res == "RESOURCE_TRANSPORT_SEWAGE") ++b.fuelStorages;
            }
            else if (key == "STYLE_FLAG") { if (b.style.empty()) ls >> b.style; }
            else if (key == "COST_RESOURCE_AUTO") {
                std::string res; ls >> res;
                if (res.rfind("wall_", 0) == 0 && b.material.empty()) b.material = res.substr(5);
            }
            else if (key == "QUALITY_OF_LIVING") ls >> b.quality;
            else if (key == "PRODUCTION" || key == "CONSUMPTION") {
                std::string res; float v = 0; ls >> res >> v;
                if (!res.empty() && v > 0) (key == "PRODUCTION" ? b.prod : b.cons).push_back({ res, v });
            }
            else if (key == "CONSUMPTION_PER_SECOND") { std::string res; float v = 0; ls >> res >> v; if (res == "eletric") b.powerPerWorker = v; }
            else if (key == "WORKERS_NEEDED") ls >> b.workers;
            else if (key == "PROFESORS_NEEDED") ls >> b.profs;
            else if (key == "CITIZEN_ABLE_SERVE") ls >> b.serve;
            else if (key == "ELETRIC_CONSUMPTION_LIVING_WORKER_FACTOR") ls >> b.fLive;
            else if (key == "ELETRIC_CONSUMPTION_LIVING_WORKER_FACTOR_ABLE_SERVE") ls >> b.fLiveServe;
            else if (key == "ELETRIC_CONSUMPTION_LIGHTING_WORKER_FACTOR") ls >> b.fLight;
            else if (key == "ELETRIC_CONSUMPTION_LIGHTING_WORKER_FACTOR_ABLE_SERVE") ls >> b.fLightServe;
            else if (key == "CONNECTION_HEATING_BIG") b.heatBig = true;
            else if (key == "ENGINE_SPEED") ls >> b.engineSpeed;
            else if (key == "WATER_CONSUMPTION_LIVING_WORKER_FACTOR") ls >> b.fWater;
            else if (key == "STORAGE_FUEL") {
                std::string res; ls >> res;
                if (res == "RESOURCE_TRANSPORT_OIL" || res == "RESOURCE_TRANSPORT_WATER" || res == "RESOURCE_TRANSPORT_SEWAGE") ++b.fuelStorages;
            }
            else if (key == "HEATING_DISABLE") b.noHeat = true;
            else if (key == "ELECTRIC_DISABLE") b.noPower = true;
            else if (key == "WATERSEWAGE_DISABLE") b.noWater = b.noSewage = true;
            else if (key == "SEWAGE_DISABLE") b.noSewage = true;
        }
        if (b.category.empty()) continue;
        auto bb = ReadAll(dir + stem + L".bbox");
        if (bb.size() < 24) continue;
        // the file is a count and that many 540-byte parts, each ending with its box (min xyz, max xyz)
        float f[6] = { 1e9f, 1e9f, 1e9f, -1e9f, -1e9f, -1e9f };
        int parts = 0;
        if (bb.size() >= 4) memcpy(&parts, bb.data(), 4);
        if (parts < 1 || bb.size() != 4 + (size_t)parts * 540) parts = 1;
        for (int p = 0; p < parts; ++p) {
            float g[6];
            memcpy(g, bb.data() + bb.size() - 24 - (size_t)(parts - 1 - p) * 540, 24);
            for (int k = 0; k < 3; ++k) { f[k] = std::min(f[k], g[k]); f[k + 3] = std::max(f[k + 3], g[k + 3]); }
        }
        b.sx = f[3] - f[0]; b.sz = f[5] - f[2]; b.height = f[4];
        b.cx = (f[0] + f[3]) / 2; b.cz = (f[2] + f[5]) / 2;
        if (!(b.sx > 0.5f && b.sz > 0.5f && b.sx < 2000 && b.sz < 2000)) continue;
        for (size_t c = 0; c < connPts.size(); ++c) {
            auto& pts = connPts[c];
            if (pts.empty()) continue;
            Conn cn{ connKind[c], pts[0].u, pts[0].v, 0, 0, connY[c][0] };
            if (pts.size() > 1) { cn.dx = pts[0].u - pts[1].u; cn.dz = pts[0].v - pts[1].v; }
            // wire points are a row of insulators, not an inner / outer pair
            // (a wire port still starts with an outer / inner pair at the same height: that pair gives its direction)
            if ((cn.kind >= 8 && !(pts.size() > 1 && fabsf(connY[c][1] - connY[c][0]) < 0.01f)) || fabsf(cn.dx) + fabsf(cn.dz) < 0.05f) { cn.dx = cn.x - b.cx; cn.dz = cn.z - b.cz; }
            bool dup = false;
            for (auto& o : b.conns) if (o.kind == cn.kind && fabsf(o.x - cn.x) + fabsf(o.z - cn.z) < 0.5f) dup = true;
            if (!dup) b.conns.push_back(cn);
        }
        // one point that serves both a footpath and a road is always a road exit in the game (user, 2026-11-18): no
        // footpath link is drawn from it
        b.conns.erase(std::remove_if(b.conns.begin(), b.conns.end(), [&](const Conn& p) {
            if (p.kind != 0) return false;
            for (auto& q : b.conns) if (q.kind == 1 && fabsf(q.x - p.x) + fabsf(q.z - p.z) < 1.0f) return true;
            return false; }), b.conns.end());
        for (auto& cn : b.conns) if (cn.kind == 1 && b.category != "LIVING") b.needsRoad = true;
        ComputeLoads(b);
        auto it = names.find(nameId);
        b.name = (it != names.end() && !it->second.empty()) ? WideToUtf8(it->second) : b.id;
        auto ie = namesEn.find(nameId);
        b.nameRu = b.name; b.nameEn = (ie != namesEn.end() && !ie->second.empty()) ? WideToUtf8(ie->second) : b.name;
        g_types.push_back(b);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(g_types.begin(), g_types.end(), [](const BuildingType& a, const BuildingType& b) {
        if (a.category != b.category) return a.category < b.category;
        if (a.capacity != b.capacity) return a.capacity > b.capacity;
        return a.id < b.id;
    });
    int living = 0;
    for (auto& t : g_types) if (t.category == "LIVING") ++living;
    int ped = 0, road = 0;
    for (auto& t : g_types) { if (t.needsRoad) ++road; for (auto& c : t.conns) if (c.kind == 0) { ++ped; break; } }
    Log("catalog: %d buildings, %d residential, %d names, %d with a footpath entrance, %d need a road",
        (int)g_types.size(), living, (int)names.size(), ped, road);
}

void SetEnglish(bool en) { for (auto& t : g_types) t.name = en ? t.nameEn : t.nameRu; }

} // namespace catalog
