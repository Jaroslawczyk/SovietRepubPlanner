// District layout inside an arbitrary outline: a street with bus stops, strips of buildings on both
// sides of footpaths, services next to the street, steep spots skipped.
#include "common.h"
#include <cmath>
#include <algorithm>

bool PolyScan(const std::vector<V2>& poly, float v, float& left, float& right) {
    std::vector<float> xs;
    size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        const V2& a = poly[i]; const V2& b = poly[(i + 1) % n];
        if ((a.v <= v && b.v > v) || (b.v <= v && a.v > v))
            xs.push_back(a.u + (v - a.v) / (b.v - a.v) * (b.u - a.u));
    }
    std::sort(xs.begin(), xs.end());
    float best = 0; bool ok = false;
    for (size_t i = 0; i + 1 < xs.size(); i += 2)
        if (xs[i + 1] - xs[i] > best) { best = xs[i + 1] - xs[i]; left = xs[i]; right = xs[i + 1]; ok = true; }
    return ok;
}

bool PolyContains(const std::vector<V2>& poly, float u, float v) {
    bool in = false;
    size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const V2& a = poly[i]; const V2& b = poly[j];
        if (((a.v > v) != (b.v > v)) && (u < (b.u - a.u) * (v - a.v) / (b.v - a.v) + a.u)) in = !in;
    }
    return in;
}

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
    if (in.poly.size() < 3 || in.houseTypes.empty()) return r;

    float minU = 1e9f, maxU = -1e9f, minV = 1e9f, maxV = -1e9f;
    for (auto& p : in.poly) { minU = std::min(minU, p.u); maxU = std::max(maxU, p.u); minV = std::min(minV, p.v); maxV = std::max(maxV, p.v); }
    if (maxU - minU < 20 || maxV - minV < 20) return r;

    const float margin = 5.0f, band = 7.0f, gap = in.gap;

    // what to build: services first (they end up next to the street), then houses round-robin
    std::vector<Item> items;
    if (in.services) for (auto& s : *in.services) {
        if (!s.enabled || s.typeIndex < 0) continue;
        int n = (int)ceilf((float)in.population / (float)std::max(1, s.perResidents));
        for (int i = 0; i < std::max(1, n); ++i) items.push_back(MakeItem(s.typeIndex));
    }
    int cap = 0;
    for (size_t i = 0; cap < in.population && i < 5000; ++i) {
        int t = in.houseTypes[i % in.houseTypes.size()];
        if (all[t].capacity <= 0) { if (i > in.houseTypes.size() * 2 && cap == 0) break; continue; }
        items.push_back(MakeItem(t));
        cap += all[t].capacity;
    }

    // the street
    float roadHalf = in.roadType == 0 ? 0.0f : in.roadType == 1 ? 4.0f : 9.0f;
    float roadV = std::clamp(in.roadV, minV + roadHalf + 2, maxV - roadHalf - 2);
    float roadBand = 0;
    if (roadHalf > 0) {
        float L, R;
        if (PolyScan(in.poly, roadV, L, R) && R - L > 30) {
            r.roads.push_back({ L, roadV, R, roadV, roadHalf * 2 });
            r.paths.push_back({ L, roadV - roadHalf - 2, R, roadV - roadHalf - 2 });    // pavements
            r.paths.push_back({ L, roadV + roadHalf + 2, R, roadV + roadHalf + 2 });
            for (float su : in.stopsU) r.stops.push_back({ std::clamp(su, L + 10, R - 10), roadV });
            roadBand = roadHalf + 6;
        } else roadHalf = 0;
    }
    float spineU = r.stops.empty() ? (minU + maxU) / 2 : r.stops[0].u;

    auto heightRange = [&](float cu, float cv, float w, float d) {
        if (!in.height) return 0.0f;
        float lo = 1e9f, hi = -1e9f;
        const float pu[5] = { -0.5f, 0.5f, 0.5f, -0.5f, 0 }, pv[5] = { -0.5f, -0.5f, 0.5f, 0.5f, 0 };
        for (int k = 0; k < 5; ++k) { float h = in.height(cu + pu[k] * w, cv + pv[k] * d); lo = std::min(lo, h); hi = std::max(hi, h); }
        return hi - lo;
    };

    size_t i = 0;
    // one row of buildings between vlo and vhi, facing the path at pathV
    auto placeRow = [&](float vlo, float vhi, bool north, float pathV, size_t limit, float& rowL, float& rowR) {
        float L = -1e9f, R = 1e9f, l, rr;
        const float scan[3] = { vlo + 0.5f, (vlo + vhi) / 2, vhi - 0.5f };
        for (float sv : scan) { if (!PolyScan(in.poly, sv, l, rr)) return 0; L = std::max(L, l); R = std::min(R, rr); }
        int placed = 0;
        float u = L + margin;
        while (i < limit) {
            const Item& it = items[i];
            if (it.d > vhi - vlo + 0.01f) break;
            if (u + it.w > R - margin) break;
            float cu = u + it.w / 2;
            float cv = north ? vlo + it.d / 2 : vhi - it.d / 2;
            if (heightRange(cu, cv, it.w, it.d) > in.maxDiff) { u += 6; ++r.steepSkipped; continue; }   // too steep here
            r.buildings.push_back({ it.type, cu, cv, it.w, it.d, it.rotated });
            r.paths.push_back({ cu, pathV, cu, north ? pathV + band / 2 : pathV - band / 2 });
            rowL = std::min(rowL, cu); rowR = std::max(rowR, cu);
            u += it.w + gap; ++i; ++placed;
        }
        return placed;
    };

    float pathLo = 1e9f, pathHi = -1e9f;
    // one strip: south row, footpath, north row; returns false when it does not fit
    auto strip = [&](float lo, float hi, float depth) {
        size_t limit = std::min(items.size(), i + 40);
        float pathV = lo + depth + band / 2;
        float rowL = spineU, rowR = spineU;
        int placed = placeRow(lo, lo + depth, false, pathV, limit, rowL, rowR);
        placed += placeRow(pathV + band / 2, hi, true, pathV, limit, rowL, rowR);
        if (placed) {
            r.paths.push_back({ rowL - 4, pathV, rowR + 4, pathV });
            pathLo = std::min(pathLo, pathV); pathHi = std::max(pathHi, pathV);
        }
        return placed;
    };
    auto nextDepth = [&]() {
        float d = 0;
        for (size_t k = i; k < items.size() && k < i + 16; ++k) d = std::max(d, items[k].d);
        return d;
    };

    // fill strips alternately above and below the street so that everything stays close to it
    float up = roadV + roadBand, down = roadV - roadBand;
    bool upDone = false, downDone = false;
    int emptyUp = 0, emptyDown = 0;
    while (i < items.size() && !(upDone && downDone)) {
        for (int side = 0; side < 2 && i < items.size(); ++side) {
            bool goUp = side == 0;
            if (goUp ? upDone : downDone) continue;
            float depth = nextDepth(), h = 2 * depth + band;
            float lo = goUp ? up : down - h, hi = lo + h;
            if (goUp ? hi > maxV - margin : lo < minV + margin) { (goUp ? upDone : downDone) = true; continue; }
            int placed = strip(lo, hi, depth);
            int& empty = goUp ? emptyUp : emptyDown;
            empty = placed ? 0 : empty + 1;
            float stepBy = placed ? h + gap : 12.0f;          // nothing fitted: slide a little and try again
            if (goUp) up += stepBy; else down -= stepBy;
            if (empty > 40) (goUp ? upDone : downDone) = true;
        }
    }
    r.unplaced = (int)(items.size() - i);

    if (pathHi >= pathLo) {     // the spine links every strip with the street and the first stop
        float lo = std::min(pathLo, roadHalf > 0 ? roadV : pathLo), hi = std::max(pathHi, roadHalf > 0 ? roadV : pathHi);
        r.paths.push_back({ spineU, lo, spineU, hi });
        if (roadHalf > 0) r.crossings.push_back({ spineU, roadV });
    }
    for (auto& p : r.paths) r.pathLength += fabsf(p.u2 - p.u1) + fabsf(p.v2 - p.v1);
    for (auto& p : r.roads) r.roadLength += fabsf(p.u2 - p.u1) + fabsf(p.v2 - p.v1);

    for (auto& b : r.buildings) {
        const BuildingType& t = all[b.type];
        bool living = t.category == "LIVING";
        if (living) { r.capacity += t.capacity; ++r.houses; } else ++r.services;

        if (in.height) {        // levelling: pad at the mean height, earth volumes from a 3x3 sample
            float h[9], sum = 0; int k = 0;
            for (int a = -1; a <= 1; ++a) for (int c = -1; c <= 1; ++c) { h[k] = in.height(b.u + a * b.w / 3, b.v + c * b.d / 3); sum += h[k++]; }
            b.pad = sum / 9;
            float cell = b.w * b.d / 9;
            for (float hv : h) { if (hv > b.pad) b.cut += (hv - b.pad) * cell; else b.fill += (b.pad - hv) * cell; }
            r.cut += b.cut; r.fill += b.fill;
        }
        if (!living) continue;
        if (!r.stops.empty()) {
            float best = 1e9f;
            for (auto& s : r.stops) best = std::min(best, fabsf(b.u - s.u) + fabsf(b.v - s.v));
            r.maxWalkToStop = std::max(r.maxWalkToStop, best);
            if (best > in.walkRadius) { b.tooFar = true; ++r.farHouses; }
        }
        float shop = -1;
        for (auto& s : r.buildings) {
            if (all[s.type].category != "SHOP") continue;
            float dist = fabsf(b.u - s.u) + fabsf(b.v - s.v);
            if (shop < 0 || dist < shop) shop = dist;
        }
        r.maxWalkToShop = std::max(r.maxWalkToShop, shop);
    }
    return r;
}
