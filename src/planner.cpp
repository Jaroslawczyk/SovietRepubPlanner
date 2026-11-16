// District layout: rows of buildings on both sides of footpaths, services in the middle.
#include "common.h"
#include <cmath>
#include <algorithm>

namespace {
struct Item { int type; float w, d; bool rotated; };

Item MakeItem(int type) {
    const BuildingType& b = catalog::All()[type];
    Item it{ type, b.sx, b.sz, false };
    if (it.d > it.w) { std::swap(it.w, it.d); it.rotated = true; }   // long side along the path
    return it;
}
}

PlanResult MakePlan(const PlanInput& in) {
    PlanResult r;
    const auto& all = catalog::All();
    if (in.width < 20 || in.depth < 20 || in.houseTypes.empty()) return r;

    const float margin = 6.0f, pathBand = 7.0f;     // 3 m path + 2 m setback each side
    const float gap = in.gap;
    const float usable = in.width - 2 * margin;

    // houses: round-robin over the chosen types until the target population fits
    std::vector<Item> houses, services;
    int cap = 0;
    for (size_t i = 0; cap < in.population && i < 5000; ++i) {
        int t = in.houseTypes[i % in.houseTypes.size()];
        if (all[t].capacity <= 0) { if (i > in.houseTypes.size() * 2 && cap == 0) break; continue; }
        houses.push_back(MakeItem(t));
        cap += all[t].capacity;
    }
    if (in.services) for (auto& s : *in.services) {
        if (!s.enabled || s.typeIndex < 0) continue;
        int n = (int)ceilf((float)in.population / (float)std::max(1, s.perResidents));
        for (int i = 0; i < std::max(1, n); ++i) services.push_back(MakeItem(s.typeIndex));
    }

    // services go between two halves of the housing so they end up in the middle of the district
    std::vector<Item> items;
    size_t half = houses.size() / 2;
    items.insert(items.end(), houses.begin(), houses.begin() + half);
    items.insert(items.end(), services.begin(), services.end());
    items.insert(items.end(), houses.begin() + half, houses.end());

    size_t i = 0;
    auto takeRow = [&](std::vector<Item>& row, float& rowW, float& rowD) {
        row.clear(); rowW = 0; rowD = 0;
        while (i < items.size()) {
            const Item& it = items[i];
            if (it.w > usable) { ++i; ++r.unplaced; continue; }      // wider than the whole area
            float w = rowW + (row.empty() ? 0 : gap) + it.w;
            if (w > usable) break;
            rowW = w; rowD = std::max(rowD, it.d); row.push_back(it); ++i;
        }
    };
    auto placeRow = [&](const std::vector<Item>& row, float rowW, float pathV, bool north) {
        float extra = (usable - rowW) / (float)(row.size() + 1);
        float u = margin + extra;
        for (auto& it : row) {
            float cu = u + it.w / 2;
            float cv = north ? pathV + pathBand / 2 + it.d / 2 : pathV - pathBand / 2 - it.d / 2;
            r.buildings.push_back({ it.type, cu, cv, it.w, it.d, it.rotated });
            r.paths.push_back({ cu, pathV, cu, north ? pathV + pathBand / 2 : pathV - pathBand / 2 });
            const BuildingType& b = all[it.type];
            if (b.category == "LIVING") { r.capacity += b.capacity; ++r.houses; } else ++r.services;
            u += it.w + gap + extra;
        }
    };

    struct Strip { size_t b0, b1, p0, p1; };
    std::vector<Strip> strips;
    std::vector<float> pathVs;
    std::vector<Item> south, north;
    float v = margin;
    while (i < items.size()) {
        size_t start = i;
        float sw, sd, nw, nd;
        takeRow(south, sw, sd);
        takeRow(north, nw, nd);
        if (south.empty() && north.empty()) break;
        if (v + sd + pathBand + nd > in.depth - margin) {
            // try the south row alone before giving up
            i = start; takeRow(south, sw, sd); north.clear(); nd = 0;
            if (south.empty() || v + sd + pathBand > in.depth - margin) { i = start; break; }
        }
        float pathV = v + sd + pathBand / 2;
        Strip st{ r.buildings.size(), 0, r.paths.size(), 0 };
        placeRow(south, sw, pathV, false);
        if (!north.empty()) placeRow(north, nw, pathV, true);
        r.paths.push_back({ margin / 2, pathV, in.width - margin / 2, pathV });
        st.b1 = r.buildings.size(); st.p1 = r.paths.size();
        strips.push_back(st);
        pathVs.push_back(pathV);
        v = pathV + pathBand / 2 + nd + gap;
    }
    r.unplaced += (int)(items.size() - i);

    // spread the strips over the whole depth instead of crowding them at one edge
    float spare = in.depth - margin - (v - gap);
    if (spare > 1 && !strips.empty()) {
        for (size_t k = 0; k < strips.size(); ++k) {
            float shift = spare * ((float)k + 0.5f) / (float)strips.size();
            for (size_t b = strips[k].b0; b < strips[k].b1; ++b) r.buildings[b].v += shift;
            for (size_t p = strips[k].p0; p < strips[k].p1; ++p) { r.paths[p].v1 += shift; r.paths[p].v2 += shift; }
            pathVs[k] += shift;
        }
    }

    if (pathVs.size() > 1) {    // side paths link all the rows together
        r.paths.push_back({ margin / 2, pathVs.front(), margin / 2, pathVs.back() });
        r.paths.push_back({ in.width - margin / 2, pathVs.front(), in.width - margin / 2, pathVs.back() });
        if (in.width > 250) r.paths.push_back({ in.width / 2, pathVs.front(), in.width / 2, pathVs.back() });
    }
    for (auto& p : r.paths) r.pathLength += fabsf(p.u2 - p.u1) + fabsf(p.v2 - p.v1);

    // longest walk from a house to the nearest shop (along the path grid, approximated as Manhattan distance)
    for (auto& h : r.buildings) {
        if (all[h.type].category != "LIVING") continue;
        float best = -1;
        for (auto& s : r.buildings) {
            if (all[s.type].category != "SHOP") continue;
            float dist = fabsf(h.u - s.u) + fabsf(h.v - s.v);
            if (best < 0 || dist < best) best = dist;
        }
        if (best > r.maxWalkToShop) r.maxWalkToShop = best;
    }
    return r;
}
