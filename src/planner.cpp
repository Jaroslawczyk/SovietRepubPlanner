// District layout inside an arbitrary outline: a street with bus stops, strips of buildings on both
// sides of footpaths, services next to the street, steep spots skipped.
#include "common.h"
#include <cmath>
#include <algorithm>
#include <queue>
#include <deque>

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
struct Item { int type; float w, d; bool rotated; int rot; };
const float kApron = 9.0f, kSide = 3.0f;   // extra depth / width the game needs beyond the model's bounding box

// A model point after "rot" quarter turns, in plan axes (model X along +u, +v, -u, -v).
V2 Turn(int rot, float x, float z) {
    switch (rot & 3) { case 0: return { x, z }; case 1: return { -z, x }; case 2: return { -x, -z }; default: return { z, -x }; }
}
// Quarter turns that make the entrance (the first footpath connection, else the first road one) face +v.
int EntranceTurn(const BuildingType& b) {
    const Conn* c = nullptr;
    for (auto& k : b.conns) if (k.kind == 0) { c = &k; break; }
    if (!c) for (auto& k : b.conns) if (k.kind == 1) { c = &k; break; }
    if (!c) return -1;
    if (fabsf(c->dz) >= fabsf(c->dx)) return c->dz > 0 ? 0 : 2;
    return c->dx > 0 ? 1 : 3;
}
Item MakeItem(int type) {
    const BuildingType& b = catalog::All()[type];
    int rot = EntranceTurn(b);
    if (rot < 0) rot = b.sz > b.sx ? 1 : 0;                         // no connections: long side along the path
    Item it{ type, b.sx, b.sz, (rot & 1) != 0, rot };
    if (it.rotated) std::swap(it.w, it.d);
    it.w += kSide; it.d += kApron;   // room for the entrance apron / construction zone the game adds around a building
    return it;
}
}

float Demand(const BuildingType& t, float perPerson, int k);
PlanResult MakePlanImpl(const PlanInput& in, const std::vector<V2>* sites) {
    PlanResult r;
    const auto& all = catalog::All();
    if (in.poly.size() < 3 || in.houseTypes.empty()) return r;

    float minU = 1e9f, maxU = -1e9f, minV = 1e9f, maxV = -1e9f;
    for (auto& p : in.poly) { minU = std::min(minU, p.u); maxU = std::max(maxU, p.u); minV = std::min(minV, p.v); maxV = std::max(maxV, p.v); }
    if (maxU - minU < 20 || maxV - minV < 20) return r;

    const float margin = 5.0f, band = std::clamp(in.band, 4.0f, 12.0f), gap = in.gap;
    float corridor = 4.5f;
    const float crossU = (minU + maxU) / 2;

    // what to build: services first (they end up next to the street), then houses round-robin
    std::vector<Item> items;
    if (in.services) for (auto& s : *in.services) {
        if (!s.enabled || s.typeIndex < 0) continue;
        int n = (int)ceilf((float)in.population / (float)std::max(1, s.perResidents));
        for (int i = 0; i < std::max(1, n); ++i) items.push_back(MakeItem(s.typeIndex));
    }
    int cap = 0;
    // houses in the requested proportion: each time take the type that is furthest behind its share
    std::vector<int> made(in.houseTypes.size(), 0);
    std::vector<float> share(in.houseTypes.size(), 1.0f);
    float wsum = 0;
    for (size_t k = 0; k < share.size(); ++k) { if (k < in.houseWeights.size()) share[k] = std::max(0.0f, in.houseWeights[k]); wsum += share[k]; }
    for (auto& w : share) w = wsum > 0 ? w / wsum : 1.0f / share.size();
    for (size_t i = 0; (in.fillAll || cap < in.population) && i < 5000; ++i) {
        int pick = -1; float bestD = -1e9f;
        for (size_t k = 0; k < share.size(); ++k) {
            if (all[in.houseTypes[k]].capacity <= 0 || share[k] <= 0) continue;
            float d = share[k] * (float)(i + 1) - (float)made[k];
            if (d > bestD) { bestD = d; pick = (int)k; }
        }
        if (pick < 0) break;
        int t = in.houseTypes[pick]; ++made[pick];
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
            for (float su : in.stopsU) r.stops.push_back({ std::clamp(su, L + 20, R - 20), roadV });
            roadBand = roadHalf + 6;
            // Stops are pass-through buildings: the road runs through them (two ROAD_ALLOWPASS connections on the
            // model's X axis), the shelters stand beside it. The stop sits on the street axis, the street and the
            // pavements are cut where it stands, and its own footpath entrances are linked like any building's.
            std::vector<std::pair<float, float>> roadGap, walkGap;
            if (in.stopType >= 0) {
                const BuildingType& st = all[in.stopType];
                float hl = 0;
                for (auto& c : st.conns) if (c.kind == 1) hl = std::max(hl, fabsf(c.x));
                if (hl < 5) hl = st.sx / 2;
                for (auto& sp : r.stops) {
                    if (in.crossRoad && fabsf(sp.u - crossU) < hl + 14) sp.u = std::min(R - hl - 2, crossU + hl + 14);   // not on the crossroads
                    Placed pb{ in.stopType, sp.u + st.cx, roadV + st.cz, st.sx, st.sz, false };
                    pb.rot = 0; pb.ou = sp.u; pb.ov = roadV; pb.stop = true;
                    r.buildings.push_back(pb);
                    roadGap.push_back({ sp.u - hl, sp.u + hl });
                    walkGap.push_back({ sp.u + st.cx - st.sx / 2 - 1, sp.u + st.cx + st.sx / 2 + 1 });
                }
                roadBand = std::max(roadBand, st.sz / 2 + fabsf(st.cz) + 1.5f);
            }
            auto cutLine = [](float a, float b, std::vector<std::pair<float, float>> gaps, std::vector<std::pair<float, float>>& out) {
                std::sort(gaps.begin(), gaps.end());
                float u = a;
                for (auto& g : gaps) { if (g.first > u + 1) out.push_back({ u, std::min(g.first, b) }); u = std::max(u, g.second); }
                if (b > u + 1) out.push_back({ u, b });
            };
            std::vector<std::pair<float, float>> pieces;
            cutLine(L, R, roadGap, pieces);
            for (auto& pc : pieces) r.roads.push_back({ pc.first, roadV, pc.second, roadV, roadHalf * 2 });
            pieces.clear(); cutLine(L, R, walkGap, pieces);
            for (auto& pc : pieces) {                                                     // pavements
                r.paths.push_back({ pc.first, roadV - roadHalf - 2, pc.second, roadV - roadHalf - 2 });
                r.paths.push_back({ pc.first, roadV + roadHalf + 2, pc.second, roadV + roadHalf + 2 });
            }
        } else roadHalf = 0;
    }
    float spineU = r.stops.empty() ? (minU + maxU) / 2 : r.stops[0].u;
    if (in.crossRoad && roadHalf > 0) { spineU = crossU; corridor = roadHalf + 4.0f; }   // the cross street runs in the spine

    auto heightRange = [&](float cu, float cv, float w, float d) {
        if (!in.height) return 0.0f;
        float lo = 1e9f, hi = -1e9f;
        const float pu[5] = { -0.5f, 0.5f, 0.5f, -0.5f, 0 }, pv[5] = { -0.5f, -0.5f, 0.5f, 0.5f, 0 };
        for (int k = 0; k < 5; ++k) { float h = in.height(cu + pu[k] * w, cv + pv[k] * d); lo = std::min(lo, h); hi = std::max(hi, h); }
        return hi - lo;
    };

    // existing buildings: sample the footprint plus a 4 m margin on a grid of about 6 m
    auto busy = [&](float cu, float cv, float w, float d) {
        if (!in.occupied) return false;
        float W = w + 8, D = d + 8;
        int nu = std::max(2, (int)(W / 6) + 1), nv = std::max(2, (int)(D / 6) + 1);
        for (int a = 0; a <= nu; ++a) for (int b = 0; b <= nv; ++b)
            if (in.occupied(cu - W / 2 + W * a / nu, cv - D / 2 + D * b / nv)) return true;
        return false;
    };

    // utility buildings: [network][0] = the node that serves houses in its radius, [1] = switch (pipes) / transformer (power)
    int utilType[4][2] = { { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 } };
    {
        static const char* const ids[4][2] = { { "water_substation", "water_switch" }, { "sewage_substation", "sewage_switch" },
                                               { "heating_endstation_small", "heating_pumpingstation_small" }, { "eletric_substation", "eletric_transformator" } };
        for (size_t t = 0; t < all.size(); ++t) for (int k = 0; k < 4; ++k) for (int j = 0; j < 2; ++j) if (all[t].id == ids[k][j]) utilType[k][j] = (int)t;
    }
    struct Pend { Item it; signed char net, role; short grp; };
    std::deque<Pend> pending, queued;   // utility buildings waiting for the next free spot in a row / for the building that asked for them
    std::vector<char> siteDone[4];
    if (sites) for (int k = 0; k < 4; ++k) siteDone[k].assign(sites[k].size(), 0);
    int utilGrp[4] = {};

    size_t i = 0;
    // one row of buildings between vlo and vhi, facing the path at pathV
    auto placeRow = [&](float vlo, float vhi, bool north, float pathV, size_t limit, float& rowL, float& rowR) {
        float L = -1e9f, R = 1e9f, l, rr;
        const float scan[3] = { vlo + 0.5f, (vlo + vhi) / 2, vhi - 0.5f };
        for (float sv : scan) { if (!PolyScan(in.poly, sv, l, rr)) return 0; L = std::max(L, l); R = std::min(R, rr); }
        int placed = 0;
        float u = L + margin;
        while (i < limit || !pending.empty()) {       // nodes asked for by the last building still get their spot
            if (sites && i < limit && pending.empty() && queued.empty()) {
                // this building stands at a spot chosen for a utility node (or just past it in the row): the node follows
                // right after it, the switch / transformer after the nodes
                const Item& h = items[i];
                float hu = u + h.w / 2, hv = north ? vlo + h.d / 2 : vhi - h.d / 2;
                std::vector<Pend> helpers;
                for (int k = 0; k < 4; ++k) {
                    if (!in.utilBuild[k] || utilType[k][0] < 0) continue;
                    for (size_t s = 0; s < sites[k].size(); ++s) {
                        const V2& st = sites[k][s];
                        if (siteDone[k][s] || !(hypotf(st.u - hu, st.v - hv) < 25 || (fabsf(st.v - hv) < 6 && hu > st.u))) continue;
                        siteDone[k][s] = 1;
                        short g = (short)utilGrp[k]++;
                        queued.push_back({ MakeItem(utilType[k][0]), (signed char)k, 0, g });
                        if (utilType[k][1] >= 0 && (k < 3 ? in.utilChain : (in.powerTrafo && g % 6 == 0))) helpers.push_back({ MakeItem(utilType[k][1]), (signed char)k, 1, g });
                    }
                }
                for (auto& hp : helpers) queued.push_back(hp);
            }
            const bool util = !pending.empty();
            const Item it = util ? pending.front().it : items[i];
            if (it.d > vhi - vlo + 0.01f) { if (util) { pending.pop_front(); continue; } break; }
            if (u + it.w > R - margin) break;
            if (u < spineU + corridor && u + it.w > spineU - corridor) { u = spineU + corridor; continue; }  // leave the spine free
            float cu = u + it.w / 2;
            float cv = north ? vlo + it.d / 2 : vhi - it.d / 2;
            if (busy(cu, cv, it.w, it.d)) { u += 6; ++r.busySkipped; continue; }                        // something is already built here
            if (heightRange(cu, cv, it.w, it.d) > in.maxDiff) { u += 6; ++r.steepSkipped; continue; }   // too steep here
            Placed pb{ it.type, cu, cv, it.w - kSide, it.d - kApron, it.rotated };
            pb.rot = (it.rot + (north ? 2 : 0)) & 3;                // the entrance looks at this row's footpath
            if (all[it.type].needsRoad && roadHalf > 0) {
                // a building that needs vehicles turns its road connection toward the street (a half turn keeps the
                // footprint), so its driveway is short and direct instead of running round the rows
                for (auto& c : all[it.type].conns) if (c.kind == 1) {
                    float nearV = roadV, bd = 1e9f;
                    for (auto& rd : r.roads) if (rd.v1 == rd.v2 && fabsf(rd.v1 - cv) < bd) { bd = fabsf(rd.v1 - cv); nearV = rd.v1; }
                    float want = nearV < cv ? -1.0f : 1.0f;
                    if (Turn(pb.rot, c.dx, c.dz).v * want < 0) pb.rot = (pb.rot + 2) & 3;
                    break;
                }
            }
            V2 off = Turn(pb.rot, all[it.type].cx, all[it.type].cz);
            pb.ou = cu - off.u; pb.ov = cv - off.v;
            if (util) { pb.net = pending.front().net; pb.role = pending.front().role; pb.grp = pending.front().grp; }
            r.buildings.push_back(pb);
            rowL = std::min(rowL, cu - it.w / 2 + 4); rowR = std::max(rowR, cu + it.w / 2 - 4);
            u += it.w + gap; ++placed;
            if (util) pending.pop_front(); else { ++i; pending.insert(pending.end(), queued.begin(), queued.end()); queued.clear(); }
        }
        return placed;
    };

    struct StripInfo { float pathV, l, r; };
    std::vector<StripInfo> stripsUp, stripsDown;    // in order, going away from the street
    bool curUp = true;
    // one strip: south row, footpath, north row (rows = 1: only the row next to the previous strip)
    auto strip = [&](float lo, float hi, float depth, int rows = 2) {
        size_t limit = std::min(items.size(), i + 40);
        bool southOnly = rows == 1 && curUp, northOnly = rows == 1 && !curUp;
        float pathV = northOnly ? lo + band / 2 : lo + depth + band / 2;
        float rowL = 1e9f, rowR = -1e9f;
        int placed = 0;
        if (!northOnly) placed += placeRow(lo, lo + depth, false, pathV, limit, rowL, rowR);
        if (!southOnly) placed += placeRow(pathV + band / 2, hi, true, pathV, limit, rowL, rowR);
        if (placed) {
            r.paths.push_back({ rowL - 4, pathV, rowR + 4, pathV });
            (curUp ? stripsUp : stripsDown).push_back({ pathV, rowL - 4, rowR + 4 });
        }
        return placed;
    };
    auto nextDepth = [&]() {
        float d = 0;
        for (size_t k = i; k < items.size() && k < i + 16; ++k) d = std::max(d, items[k].d);
        for (auto& p : pending) d = std::max(d, p.it.d);
        return d;
    };

    // fill strips alternately above and below the street so that everything stays close to it
    float up = roadV + roadBand, down = roadV - roadBand;
    bool upDone = false, downDone = false;
    float nextUpStreet = in.streetSpacing, nextDownStreet = in.streetSpacing;
    int emptyUp = 0, emptyDown = 0;
    while ((i < items.size() || !pending.empty()) && !(upDone && downDone)) {
        for (int side = 0; side < 2 && (i < items.size() || !pending.empty()); ++side) {
            bool goUp = side == 0;
            if (goUp ? upDone : downDone) continue;
            // a parallel street every "streetSpacing" metres, so that the whole area gets road access
            float& nextStreet = goUp ? nextUpStreet : nextDownStreet;
            if (roadHalf > 0 && in.streetSpacing > 0 && (goUp ? up - roadV : roadV - down) >= nextStreet) {
                float sv = goUp ? up + roadBand : down - roadBand, L, R;
                bool fits = goUp ? sv + roadBand < maxV - margin : sv - roadBand > minV + margin;
                if (fits && PolyScan(in.poly, sv, L, R) && R - L > 30) {
                    r.roads.push_back({ L, sv, R, sv, roadHalf * 2 });
                    float nearV = goUp ? sv - roadHalf - 2 : sv + roadHalf + 2, farV = goUp ? sv + roadHalf + 2 : sv - roadHalf - 2;
                    r.paths.push_back({ L, nearV, R, nearV });
                    r.paths.push_back({ L, farV, R, farV });
                    auto& list = goUp ? stripsUp : stripsDown;
                    list.push_back({ nearV, L, R });
                    list.push_back({ farV, L, R });
                    if (goUp) up = sv + roadBand; else down = sv - roadBand;
                }
                nextStreet += in.streetSpacing;
            }
            float depth = nextDepth(), h = 2 * depth + band;
            float lo = goUp ? up : down - h, hi = lo + h;
            curUp = goUp;
            if (goUp ? hi > maxV - margin : lo < minV + margin) {
                // no room for a full strip: one last single row with its footpath, if that fits
                float h1 = depth + band;
                float lo1 = goUp ? up : down - h1, hi1 = lo1 + h1;
                if (!(goUp ? hi1 > maxV - margin : lo1 < minV + margin)) strip(lo1, hi1, depth, 1);
                (goUp ? upDone : downDone) = true; continue;
            }
            int placed = strip(lo, hi, depth);
            int& empty = goUp ? emptyUp : emptyDown;
            empty = placed ? 0 : empty + 1;
            float stepBy = placed ? h + gap : 12.0f;          // nothing fitted: slide a little and try again
            if (goUp) up += stepBy; else down -= stepBy;
            if (empty > 40) (goUp ? upDone : downDone) = true;
        }
    }
    r.unplaced = (int)(items.size() - i);

    // the last node of a pipe network hangs on the previous switch, so its own switch is not needed
    {
        int lastGrp[4] = { -1, -1, -1, -1 };
        for (auto& b : r.buildings) if (b.net >= 0 && b.role == 0) lastGrp[b.net] = std::max(lastGrp[b.net], (int)b.grp);
        r.buildings.erase(std::remove_if(r.buildings.begin(), r.buildings.end(), [&](const Placed& b) {
            return b.net >= 0 && b.net < 3 && b.role == 1 && b.grp >= lastGrp[b.net]; }), r.buildings.end());
        for (int k = 0; k < 4; ++k) r.utilNodes[k].clear();
        for (auto& b : r.buildings) if (b.net >= 0 && b.role == 0) r.utilNodes[b.net].push_back({ b.u, b.v });
    }

    // Links between neighbouring strips (and the street): a short footpath across, placed as close to the stop
    // as possible where it stays inside both footpaths' extents and does not run through a building.
    auto blocked = [&](float u, float v1, float v2) {
        if (v1 > v2) std::swap(v1, v2);
        for (auto& b : r.buildings)
            if (fabsf(b.u - u) < b.w / 2 + 1.5f && b.v + b.d / 2 > v1 && b.v - b.d / 2 < v2) return true;
        return false;
    };
    auto link = [&](const StripInfo& a, const StripInfo& b) {
        float lo = std::max(a.l, b.l), hi = std::min(a.r, b.r);
        if (hi < lo) return false;
        for (int k = 0; k < 400; ++k) {
            float cu = spineU + (k % 2 ? 1 : -1) * (float)((k + 1) / 2) * 2.0f;
            if (cu < lo || cu > hi) { if (spineU + k < lo && spineU - k > hi) break; continue; }
            if (blocked(cu, a.pathV, b.pathV)) continue;
            r.paths.push_back({ cu, a.pathV, cu, b.pathV });
            return true;
        }
        return false;
    };
    for (int side = 0; side < 2; ++side) {
        auto& list = side == 0 ? stripsUp : stripsDown;
        StripInfo prev{}; bool havePrev = false;
        if (roadHalf > 0 && !r.roads.empty()) {
            float pv = side == 0 ? roadV + roadHalf + 2 : roadV - roadHalf - 2;     // the pavement on this side
            prev = { pv, r.roads[0].u1, r.roads[0].u2 }; havePrev = true;
        } else if (side == 1 && !stripsUp.empty()) { prev = stripsUp[0]; havePrev = true; }
        for (auto& st : list) {
            if (havePrev && !(in.crossRoad && roadHalf > 0)) link(prev, st);
            prev = st; havePrev = true;
        }
    }
    if (in.crossRoad && roadHalf > 0) {
        std::vector<V2> tp; for (auto& q : in.poly) tp.push_back({ q.v, q.u });       // scan across: swap the axes
        float B, T;
        if (PolyScan(tp, spineU, B, T) && T - B > 30) {
            std::vector<float> cuts{ B };
            for (auto& rd : r.roads) if (rd.v1 == rd.v2 && rd.v1 > B + 5 && rd.v1 < T - 5 && spineU > std::min(rd.u1, rd.u2) && spineU < std::max(rd.u1, rd.u2)) cuts.push_back(rd.v1);
            cuts.push_back(T); std::sort(cuts.begin(), cuts.end());
            for (size_t k = 0; k + 1 < cuts.size(); ++k) r.roads.push_back({ spineU, cuts[k], spineU, cuts[k + 1], roadHalf * 2 });
            r.paths.push_back({ spineU - roadHalf - 2, B, spineU - roadHalf - 2, T });
            r.paths.push_back({ spineU + roadHalf + 2, B, spineU + roadHalf + 2, T });
        }
    } else if (roadHalf > 0 && !r.roads.empty() && (!stripsUp.empty() || !stripsDown.empty()))
        r.crossings.push_back({ spineU, roadV });
    // Connections: a footpath from every building's footpath entrance to the nearest footpath, and a driveway from
    // the road connection of buildings that need vehicles to the nearest street. A search on a 2 m grid with a
    // price on turns keeps the lines straight and out of the buildings.
    {
        const float cell = 2.0f, gu = minU - 12, gv = minV - 12;
        const int W = (int)((maxU - minU + 24) / cell) + 1, H = (int)((maxV - minV + 24) / cell) + 1;
        const size_t nBase = r.paths.size();
        std::vector<unsigned char> block, goal, slow;
        std::vector<float> dist; std::vector<int> parent, touched;
        bool gridOk = (long long)W * H <= 1200000;
        auto ci = [&](float u) { return std::clamp((int)floorf((u - gu) / cell), 0, W - 1); };
        auto cj = [&](float v) { return std::clamp((int)floorf((v - gv) / cell), 0, H - 1); };
        auto mark = [&](std::vector<unsigned char>& a, unsigned char bit, float u1, float v1, float u2, float v2) {
            int i1 = ci(std::min(u1, u2)), i2 = ci(std::max(u1, u2)), j1 = cj(std::min(v1, v2)), j2 = cj(std::max(v1, v2));
            for (int j = j1; j <= j2; ++j) for (int i = i1; i <= i2; ++i) a[(size_t)j * W + i] |= bit;
        };
        if (gridOk) {
            block.assign((size_t)W * H, 0); goal = block; slow = block;
            dist.assign((size_t)W * H * 4, 1e9f); parent.assign((size_t)W * H * 4, -1);
            for (auto& b : r.buildings) {
                mark(block, 1 | 4, b.u - b.w / 2 - 1, b.v - b.d / 2 - 1, b.u + b.w / 2 + 1, b.v + b.d / 2 + 1);     // 4 = shut for pipes too
                mark(block, 2, b.u - b.w / 2 - 5, b.v - b.d / 2 - 5, b.u + b.w / 2 + 5, b.v + b.d / 2 + 5);   // a road is 8 m wide: its axis keeps 4 m + 1 m off
            }
            for (auto& p : r.paths) { mark(goal, 1, p.u1, p.v1, p.u2, p.v2); mark(slow, 1, p.u1, p.v1, p.u2, p.v2); }
            for (auto& rd : r.roads) {
                mark(goal, 2, rd.u1, rd.v1, rd.u2, rd.v2);
                mark(block, 1, rd.u1, rd.v1 - rd.width / 2 + 0.5f, rd.u2, rd.v1 + rd.width / 2 - 0.5f);    // nobody walks on the roadway
            }
        }
        const int du[4] = { 1, 0, -1, 0 }, dv[4] = { 0, 1, 0, -1 };
        // mode 0 = footpath, 1 = driveway, 2 = utility line to the cell marked 4 in goal (may cross paths and roads); returns the price of the line, < 0 = no way
        auto route = [&](V2 p, V2 n, int mode, const Placed& own, std::vector<V2>& out) {
            const unsigned char bit = mode == 2 ? 4 : mode ? 2 : 1;
            const float infl = (mode == 1 ? 5.0f : 1.0f) + cell;
            int dir = fabsf(n.u) >= fabsf(n.v) ? (n.u > 0 ? 0 : 2) : (n.v > 0 ? 1 : 3);
            V2 q = p; float walked = 0;         // straight out of the own building first
            while (fabsf(q.u - own.u) < own.w / 2 + infl && fabsf(q.v - own.v) < own.d / 2 + infl) {
                q.u += du[dir] * 1.0f; q.v += dv[dir] * 1.0f; walked += 1;
                if (walked > 60) return -1.0f;
            }
            int si = ci(q.u), sj = cj(q.v);
            if (block[(size_t)sj * W + si] & bit) return -1.0f;
            typedef std::pair<float, int> QE;
            std::priority_queue<QE, std::vector<QE>, std::greater<QE>> pq;
            int start = (sj * W + si) * 4 + dir, found = -1, pops = 0;
            dist[start] = 0; parent[start] = -1; touched.push_back(start); pq.push({ 0.0f, start });
            while (!pq.empty() && pops++ < (mode == 2 ? 600000 : 40000)) {
                QE e = pq.top(); pq.pop();
                if (e.first > dist[e.second]) continue;
                int c = e.second / 4, d = e.second % 4, i = c % W, j = c / W;
                if (goal[c] & bit) { found = e.second; break; }
                if (e.first > (mode == 2 ? 900 : 260)) break;
                for (int nd = 0; nd < 4; ++nd) {
                    if (nd == (d + 2) % 4) continue;
                    int ni = i + du[nd], nj = j + dv[nd];
                    if (ni < 0 || nj < 0 || ni >= W || nj >= H) continue;
                    int nc = nj * W + ni;
                    if (block[nc] & bit) continue;
                    float cost = e.first + 1 + (nd != d ? 6 : 0) + (mode == 1 && (slow[nc] & 1) ? 12 : 0);
                    int ns = nc * 4 + nd;
                    if (cost < dist[ns]) {
                        if (dist[ns] > 1e8f) touched.push_back(ns);
                        dist[ns] = cost; parent[ns] = e.second; pq.push({ cost, ns });
                    }
                }
            }
            float price = -1;
            if (found >= 0) {
                price = dist[found] + walked / cell;
                std::vector<V2> cells;
                for (int s = found; s >= 0; s = parent[s]) { int c = s / 4; cells.push_back({ gu + (c % W + 0.5f) * cell, gv + (c / W + 0.5f) * cell }); }
                std::reverse(cells.begin(), cells.end());
                std::vector<V2> corner;         // keep the corners only
                for (size_t k = 0; k < cells.size(); ++k) {
                    if (k > 0 && k + 1 < cells.size() &&
                        ((cells[k - 1].u == cells[k].u && cells[k].u == cells[k + 1].u) || (cells[k - 1].v == cells[k].v && cells[k].v == cells[k + 1].v))) continue;
                    corner.push_back(cells[k]);
                }
                // the first leg leaves the connection point exactly along its direction
                bool alongU = dir == 0 || dir == 2;
                if (alongU) corner[0].v = p.v; else corner[0].u = p.u;
                if (corner.size() > 1 && (alongU ? corner[1].v == cells[0].v : corner[1].u == cells[0].u)) { if (alongU) corner[1].v = p.v; else corner[1].u = p.u; }
                out.clear(); out.push_back(p);
                for (auto& c : corner) out.push_back(c);
                if (mode < 2) {
                // the last leg ends exactly on the footpath / street it reached
                V2& last = out.back(); const V2 prev = out[out.size() - 2];
                float best = 1e9f; V2 hit = last; bool horiz = true;
                auto consider = [&](float u1, float v1, float u2, float v2) {
                    float cu = std::clamp(last.u, std::min(u1, u2), std::max(u1, u2)), cv = std::clamp(last.v, std::min(v1, v2), std::max(v1, v2));
                    float dd = fabsf(cu - last.u) + fabsf(cv - last.v);
                    if (dd < best) { best = dd; hit = { cu, cv }; horiz = fabsf(u2 - u1) >= fabsf(v2 - v1); }
                };
                if (mode) for (auto& rd : r.roads) consider(rd.u1, rd.v1, rd.u2, rd.v2);
                else for (size_t k = 0; k < nBase; ++k) consider(r.paths[k].u1, r.paths[k].v1, r.paths[k].u2, r.paths[k].v2);
                bool legVertical = fabsf(last.v - prev.v) >= fabsf(last.u - prev.u);
                if (horiz && legVertical) last = { prev.u, hit.v };
                else if (!horiz && !legVertical) last = { hit.u, prev.v };
                else last = hit;
                }
            }
            for (int s : touched) { dist[s] = 1e9f; parent[s] = -1; }
            touched.clear();
            return price;
        };
        std::vector<PathSeg> extra;
        for (auto& b : r.buildings) {
            const BuildingType& t = all[b.type];
            for (int mode = 0; mode < 2; ++mode) {
                if (mode == 1 && (!t.needsRoad || b.stop)) continue;
                bool has = false; float best = -1; std::vector<V2> line, bestLine;
                int tried = 0;
                for (auto& c : t.conns) {
                    if (c.kind != mode) continue;
                    has = true;
                    if (!gridOk || (mode == 1 && r.roads.empty()) || tried++ >= 4) continue;
                    V2 o = Turn(b.rot, c.x, c.z), n = Turn(b.rot, c.dx, c.dz);
                    float price = route({ b.ou + o.u, b.ov + o.v }, n, mode, b, line);
                    if (price >= 0 && (best < 0 || price < best)) { best = price; bestLine = line; }
                }
                if (!has) continue;
                if (best < 0) { ++(mode ? r.noRoad : r.noPath); continue; }
                (mode ? r.roadPts : r.pedPts).push_back(bestLine[0]);
                for (size_t k = 0; k + 1 < bestLine.size(); ++k) {
                    PathSeg sg{ bestLine[k].u, bestLine[k].v, bestLine[k + 1].u, bestLine[k + 1].v };
                    if (fabsf(sg.u2 - sg.u1) + fabsf(sg.v2 - sg.v1) < 0.05f) continue;
                    if (mode) r.drives.push_back(sg); else extra.push_back(sg);
                }
            }
        }
        r.paths.insert(r.paths.end(), extra.begin(), extra.end());

        // Utility lines by the game's rules: a pipe joins two buildings only, so every node hangs on a switch and the
        // switches form a chain (the last node is fed by the previous switch); a medium-voltage wire runs from a
        // transformer output straight to a substation. The head of each chain is left for the player to feed.
        if (gridOk) {
            auto port = [&](const Placed& b, int kind, int nth) -> const Conn* {
                for (auto& c : all[b.type].conns) if (c.kind == kind && nth-- == 0) return &c;
                return nullptr;
            };
            auto pipe = [&](int net, const Placed* A, const Conn* ca, const Placed* B, const Conn* cb) {
                if (!A || !B || !ca || !cb) { ++r.utilNoLine[net]; return; }
                if (net == 3) {     // a wire goes over everything on poles: always one straight piece, port to port
                    V2 p = Turn(A->rot, ca->x, ca->z), q = Turn(B->rot, cb->x, cb->z);
                    p.u += A->ou; p.v += A->ov; q.u += B->ou; q.v += B->ov;
                    r.utilLines.push_back({ net, p.u, p.v, q.u, q.v, ca->y, cb->y }); r.utilLen[net] += hypotf(q.u - p.u, q.v - p.v);
                    return;
                }
                V2 p = Turn(A->rot, ca->x, ca->z), pn = Turn(A->rot, ca->dx, ca->dz);
                V2 q = Turn(B->rot, cb->x, cb->z), qn = Turn(B->rot, cb->dx, cb->dz);
                p.u += A->ou; p.v += A->ov; q.u += B->ou; q.v += B->ov;
                // the search ends at the first free spot straight out of the target's connection
                int qd = fabsf(qn.u) >= fabsf(qn.v) ? (qn.u > 0 ? 0 : 2) : (qn.v > 0 ? 1 : 3);
                V2 qo = q;
                for (int s = 0; s < 60 && fabsf(qo.u - B->u) < B->w / 2 + 1 + cell && fabsf(qo.v - B->v) < B->d / 2 + 1 + cell; ++s) { qo.u += du[qd]; qo.v += dv[qd]; }
                // nothing in the way: one straight piece from port to port, the way a player draws it (pieces joined end
                // to end come out as arcs in the game, so corners are the fallback only)
                {
                    float len = hypotf(q.u - p.u, q.v - p.v); bool clear = len > 0.5f;
                    for (float s = 3; clear && s < len - 3; s += 1.0f) {
                        float cu = p.u + (q.u - p.u) * s / len, cv = p.v + (q.v - p.v) * s / len;
                        if (block[(size_t)cj(cv) * W + ci(cu)] & 4) clear = false;
                    }
                    if (clear) { r.utilLines.push_back({ net, p.u, p.v, q.u, q.v, ca->y, cb->y }); r.utilLen[net] += len; return; }
                }
                size_t gc = (size_t)cj(qo.v) * W + ci(qo.u);
                goal[gc] |= 4;
                std::vector<V2> line;
                float price = route(p, pn, 2, *A, line);
                goal[gc] &= ~4;
                if (price < 0) { ++r.utilNoLine[net]; return; }
                line.push_back(q);
                for (size_t k = 0; k + 1 < line.size(); ++k) {
                    float len = hypotf(line[k + 1].u - line[k].u, line[k + 1].v - line[k].v);
                    if (len < 0.05f) continue;
                    r.utilLines.push_back({ net, line[k].u, line[k].v, line[k + 1].u, line[k + 1].v, k == 0 ? ca->y : 0.0f, k + 2 == line.size() ? cb->y : 0.0f });
                    r.utilLen[net] += len;
                }
            };
            for (int k = 0; k < 4; ++k) {
                std::vector<const Placed*> nodes, helps;
                for (auto& b : r.buildings) if (b.net == k) (b.role ? helps : nodes).push_back(&b);
                if (k == 3) {
                    if (!in.extPorts.empty()) {
                        auto nearest = [&](int want, V2 at, float& d) -> const PlanInput::ExtPort* {
                            const PlanInput::ExtPort* b = nullptr; d = 3000;
                            for (auto& e : in.extPorts) if (e.kind == want && hypotf(e.u - at.u, e.v - at.v) < d) { d = hypotf(e.u - at.u, e.v - at.v); b = &e; }
                            return b;
                        };
                        if (!helps.empty()) {
                            for (auto* h : helps) if (const Conn* hc = port(*h, 10, 0)) {
                                V2 hp = Turn(h->rot, hc->x, hc->z); hp.u += h->ou; hp.v += h->ov; float d;
                                if (auto* e = nearest(11, hp, d)) { r.utilLines.push_back({ 4, e->u, e->v, hp.u, hp.v, e->h, hc->y }); r.utilFeed[3] = std::max(r.utilFeed[3], d); }
                            }
                        } else for (auto* n : nodes) if (const Conn* nc = port(*n, 8, 0)) {
                            V2 np = Turn(n->rot, nc->x, nc->z); np.u += n->ou; np.v += n->ov; float d;
                            if (auto* e = nearest(9, np, d)) { r.utilLines.push_back({ 3, e->u, e->v, np.u, np.v, e->h, nc->y }); r.utilFeed[3] = std::max(r.utilFeed[3], d); }
                        }
                    }
                    std::vector<int> used(helps.size(), 0);
                    for (auto* n : nodes) {
                        int best = -1; float bd = 1e9f;
                        for (size_t h = 0; h < helps.size(); ++h) {
                            float d = hypotf(helps[h]->u - n->u, helps[h]->v - n->v);
                            if (port(*helps[h], 9, used[h]) && d < bd) { bd = d; best = (int)h; }
                        }
                        if (best < 0) { ++r.utilNoLine[k]; continue; }
                        pipe(k, helps[best], port(*helps[best], 9, used[best]), n, port(*n, 8, 0));
                        ++used[best];
                    }
                    continue;
                }
                const int nodeKind = k == 0 ? 2 : k == 1 ? 5 : 6, downKind = k == 0 ? 3 : k == 1 ? 4 : 6, upKind = nodeKind;
                const int upNth = k == 2 ? 2 : 0;
                // feed from what already stands: the head of the chain (the first switch's upstream port, or the first
                // node's port when there are no switches) gets one straight line from the nearest existing building
                // that has the matching port - water output, sewage input, heating
                if (!nodes.empty() && !in.extPorts.empty()) {
                    const Placed* head = nodes[0]; int hk = nodeKind, hn = 0;
                    for (auto* h : helps) if (h->grp == nodes[0]->grp) { head = h; hk = upKind; hn = upNth; }
                    if (const Conn* hc = port(*head, hk, hn)) {
                        V2 hp = Turn(head->rot, hc->x, hc->z); hp.u += head->ou; hp.v += head->ov;
                        const int want = k == 0 ? 3 : k == 1 ? 4 : 6;
                        const PlanInput::ExtPort* best = nullptr; float bd = 1500;
                        for (auto& e : in.extPorts) if ((e.kind == want || (k == 2 && e.kind == 7)) && hypotf(e.u - hp.u, e.v - hp.v) < bd) { bd = hypotf(e.u - hp.u, e.v - hp.v); best = &e; }
                        if (best) { r.utilLines.push_back({ k, best->u, best->v, hp.u, hp.v, best->h, hc->y }); r.utilLen[k] += bd; r.utilFeed[k] = bd; }
                    }
                }
                const Placed* prev = nullptr; int prevPort = 0;
                for (auto* n : nodes) {
                    const Placed* H = nullptr;
                    for (auto* h : helps) if (h->grp == n->grp) H = h;
                    if (H) {
                        if (prev) pipe(k, prev, port(*prev, downKind, prevPort++), H, port(*H, upKind, upNth));
                        pipe(k, H, port(*H, downKind, 0), n, port(*n, nodeKind, 0));
                        prev = H; prevPort = 1;
                    } else if (prev) pipe(k, prev, port(*prev, downKind, prevPort++), n, port(*n, nodeKind, 0));
                }
            }
        }
    }
    for (auto& p : r.paths) r.pathLength += fabsf(p.u2 - p.u1) + fabsf(p.v2 - p.v1);
    for (auto& p : r.drives) r.roadLength += fabsf(p.u2 - p.u1) + fabsf(p.v2 - p.v1);
    for (auto& p : r.roads) r.roadLength += fabsf(p.u2 - p.u1) + fabsf(p.v2 - p.v1);

    for (auto& b : r.buildings) {
        const BuildingType& t = all[b.type];
        bool living = t.category == "LIVING";
        if (living) { r.capacity += t.capacity; ++r.houses; } else { ++r.services; r.jobs += t.workers; }

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

    // Coverage: houses take water, heat and power from a node within its service radius, no line to the house itself.
    // Load, the way the game counts it: each building's demand is shared evenly by the nodes that reach it.
    for (int k = 0; k < 4; ++k) {
        if (!in.utilBuild[k]) continue;
        std::vector<float> load(r.utilNodes[k].size(), 0.0f);
        for (auto& b : r.buildings) {
            if (b.net >= 0 && !(k == 3 && b.net == 2)) continue;     // heat nodes draw power too
            int reach = 0;
            for (auto& nd : r.utilNodes[k]) if (hypotf(nd.u - b.u, nd.v - b.v) <= in.utilRadius[k]) ++reach;
            if (!reach) { ++r.utilUncovered[k]; continue; }
            float dem = Demand(all[b.type], in.utilPerPerson[k], k);
            r.utilDemand[k] += dem;
            for (size_t i = 0; i < r.utilNodes[k].size(); ++i)
                if (hypotf(r.utilNodes[k][i].u - b.u, r.utilNodes[k][i].v - b.v) <= in.utilRadius[k]) load[i] += dem / reach;
        }
        if (in.utilNodeCap[k] > 0)
            for (float l : load) {
                float f = l / in.utilNodeCap[k];
                r.utilMaxLoad[k] = std::max(r.utilMaxLoad[k], f);
                if (f > 0.9f) ++r.utilOverloaded[k];
            }
    }
    return r;
}

// What a building asks of a network: residents, or the workers of a service building, times the per-person norm.
// k = 2 heat, 3 power: the game's own per-type values (catalog); water and sewage: per-person guess
float Demand(const BuildingType& t, float, int k) {
    return k == 0 ? t.waterLoad : k == 1 ? t.sewageLoad : k == 2 ? t.heatLoad : t.powerLoad;
}

// Utility nodes are few and central: a dry layout without them shows where the buildings stand, a greedy cover picks
// the spots (the building that brings most uncovered ones into reach, radius less an allowance for the shift the
// inserted nodes cause), then the real layout puts a node next to each chosen spot.
PlanResult MakePlan(const PlanInput& inRaw) {
    PlanInput in = inRaw;       // node capacities from the game's formulas for the node types the plan uses
    for (auto& t : catalog::All()) {
        if (t.id == "heating_endstation_small" && t.heatCap > 0) in.utilNodeCap[2] = t.heatCap;
        if (t.id == "eletric_substation" && t.powerCap > 0) in.utilNodeCap[3] = t.powerCap;
    }
    bool any = false;
    for (int k = 0; k < 4; ++k) any = any || in.utilBuild[k];
    if (!any) return MakePlanImpl(in, nullptr);
    const auto& all = catalog::All();
    PlanInput dry = in;
    for (int k = 0; k < 4; ++k) dry.utilBuild[k] = false;
    PlanResult cur = MakePlanImpl(dry, nullptr), best;
    int bestScore = 1 << 30;
    // the nodes take room in the rows and push later buildings along, so the spots are picked again on the layout
    // that already holds them, a few times; the layout with the fewest buildings out of reach (then the fewest nodes) wins
    int extra[4] = {};       // more nodes than the load needs on average, after a pass left one over 90 %
    for (int pass = 0; pass < 6; ++pass) {
        std::vector<V2> sites[4];
        std::vector<const Placed*> bs;
        for (auto& b : cur.buildings) if (b.net < 0 && !b.stop) bs.push_back(&b);   // stops are not in the rows: no node can follow them
        const size_t n = bs.size();
        for (int k = 0; k < 4; ++k) {
            if (!in.utilBuild[k]) continue;
            const float R = std::max(40.0f, in.utilRadius[k] - 15.0f);
            std::vector<char> covered(n, 0);
            size_t left = n;
            while (left > 0 && sites[k].size() < 60) {
                int pick = -1, pickN = 0;
                for (size_t c = 0; c < n; ++c) {
                    int cnt = 0;
                    for (size_t b = 0; b < n; ++b) if (!covered[b] && hypotf(bs[b]->u - bs[c]->u, bs[b]->v - bs[c]->v) <= R) ++cnt;
                    if (cnt > pickN) { pickN = cnt; pick = (int)c; }
                }
                if (pick < 0) break;
                sites[k].push_back({ bs[pick]->u, bs[pick]->v });
                for (size_t b = 0; b < n; ++b) if (!covered[b] && hypotf(bs[b]->u - bs[pick]->u, bs[b]->v - bs[pick]->v) <= R) { covered[b] = 1; --left; }
            }
            // enough nodes for the load: the game warns above 90 % of a node's capacity; extra nodes go to the
            // buildings farthest from the chosen ones, so that the demand is shared out
            if (in.utilNodeCap[k] > 0 && !sites[k].empty()) {
                float dem = 0;
                for (size_t b = 0; b < n; ++b) dem += Demand(all[bs[b]->type], in.utilPerPerson[k], k);
                size_t need = (size_t)ceilf(dem / (0.85f * in.utilNodeCap[k])) + extra[k];
                while (sites[k].size() < need && sites[k].size() < 60) {
                    int farIdx = -1; float farD = -1;
                    for (size_t b = 0; b < n; ++b) {
                        float d = 1e9f;
                        for (auto& st : sites[k]) d = std::min(d, hypotf(bs[b]->u - st.u, bs[b]->v - st.v));
                        if (d > farD) { farD = d; farIdx = (int)b; }
                    }
                    if (farIdx < 0 || farD < 1.0f) break;
                    sites[k].push_back({ bs[farIdx]->u, bs[farIdx]->v });
                }
            }
        }
        cur = MakePlanImpl(in, sites);
        int score = 0;
        for (int k = 0; k < 4; ++k) score += (cur.utilUncovered[k] + cur.utilOverloaded[k]) * 1000;
        for (auto& b : cur.buildings) if (b.net >= 0) ++score;
        if (score < bestScore) { bestScore = score; best = cur; }
        for (int k = 0; k < 4; ++k) if (cur.utilOverloaded[k]) ++extra[k];
        if (score < 1000) break;
    }
    return best;
}
