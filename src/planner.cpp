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
const float kApron = 8.0f, kSide = 1.5f;   // extra depth / width the game needs beyond the model's bounding box

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
    float corridor = 6.5f;      // half of the free passage kept for the footpath that joins the rows to the street
    const float crossU = (minU + maxU) / 2;

    // what to build: services first (they end up next to the street), then houses round-robin
    std::vector<Item> items;
    if (in.services) for (auto& s : *in.services) {
        if (!s.enabled || s.typeIndex < 0) continue;
        // as many as the district's real number of residents asks for (with "fill the whole area" that is what
        // fits, not the number typed in)
        int base = in.servicePop > 0 ? in.servicePop : in.population;
        int n = (int)ceilf((float)base / (float)std::max(1, s.perResidents));
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

    if (in.layout == 2) {
        // ===== Village / suburb =====
        // Parallel streets one "two lots deep" apart, cross streets every streetSpacing; every street is lined on both
        // sides with buildings facing it (front yard, entrance link straight to the street). Utility nodes go to their
        // chosen spots, services to the lot farthest from any other of the same kind (even coverage, the first one
        // next to the stop), houses fill the remaining lots street by street from the main street outwards.
        const float hw = roadHalf > 0 ? roadHalf : 1.5f;          // no street: footpath lanes instead
        const float front = kApron + 1.5f, back = 3.0f;         // front yard (the game keeps ~9 m before an entrance free; links shorter than ~5 m are refused), half the back yard
        std::vector<Item> houses, svcs;
        for (auto& it : items) (all[it.type].category == "LIVING" ? houses : svcs).push_back(it);
        // bigger houses on the streets nearest the centre, the small ones further out (lanes go from the main street)
        std::stable_sort(houses.begin(), houses.end(), [&](const Item& x, const Item& y) { return all[x.type].capacity > all[y.type].capacity; });
        float maxD0 = 8;
        for (auto& h : houses) maxD0 = std::max(maxD0, h.d - kApron);
        const float P = 2 * (front + maxD0 + back) + 2 * hw;    // distance between neighbouring parallel streets
        const float crossSp = std::clamp(in.streetSpacing > 0 ? in.streetSpacing : 200.0f, 100.0f, 400.0f);
        // optional street round the outline: its axis runs hw + 1 m inside the edge; the lanes stop at it
        std::vector<V2> lanePoly = in.poly;
        if (in.ringRoad && roadHalf > 0 && in.poly.size() >= 3) {
            float area2 = 0; size_t n = in.poly.size();
            for (size_t i = 0; i < n; ++i) { const V2& a = in.poly[i]; const V2& b = in.poly[(i + 1) % n]; area2 += a.u * b.v - b.u * a.v; }
            float sgn = area2 > 0 ? 1.0f : -1.0f, inset = hw + 1;
            std::vector<V2> ring(n);
            for (size_t i = 0; i < n; ++i) {
                const V2& p0 = in.poly[(i + n - 1) % n]; const V2& p1 = in.poly[i]; const V2& p2 = in.poly[(i + 1) % n];
                float d1u = p1.u - p0.u, d1v = p1.v - p0.v, l1 = std::max(0.01f, hypotf(d1u, d1v));
                float d2u = p2.u - p1.u, d2v = p2.v - p1.v, l2 = std::max(0.01f, hypotf(d2u, d2v));
                V2 n1{ -d1v / l1 * sgn, d1u / l1 * sgn }, n2{ -d2v / l2 * sgn, d2u / l2 * sgn };   // inward normals
                float dot = n1.u * n2.u + n1.v * n2.v, k = inset / std::max(0.3f, 1 + dot);
                ring[i] = { p1.u + (n1.u + n2.u) * k, p1.v + (n1.v + n2.v) * k };
            }
            // rounded corners: both legs stop r metres short of the corner, an arc piece joins them (the game bends a
            // piece that starts on the end of another to continue its direction - that is the arc). Corners flatter
            // than 25 degrees stay sharp.
            std::vector<V2> a(n), b(n);          // a[i]: where the leg into corner i stops, b[i]: where the leg out of it starts
            std::vector<char> round(n, 0);
            for (size_t i = 0; i < n; ++i) {
                const V2& p0 = ring[(i + n - 1) % n]; const V2& p1 = ring[i]; const V2& p2 = ring[(i + 1) % n];
                float l1 = hypotf(p1.u - p0.u, p1.v - p0.v), l2 = hypotf(p2.u - p1.u, p2.v - p1.v);
                V2 d1{ (p1.u - p0.u) / std::max(0.01f, l1), (p1.v - p0.v) / std::max(0.01f, l1) }, d2{ (p2.u - p1.u) / std::max(0.01f, l2), (p2.v - p1.v) / std::max(0.01f, l2) };
                float turn = acosf(std::clamp(d1.u * d2.u + d1.v * d2.v, -1.0f, 1.0f));
                float rr = std::min({ 18.0f, l1 * 0.3f, l2 * 0.3f });
                round[i] = turn > 0.44f && rr > 8;
                a[i] = round[i] ? V2{ p1.u - d1.u * rr, p1.v - d1.v * rr } : p1;
                b[i] = round[i] ? V2{ p1.u + d2.u * rr, p1.v + d2.v * rr } : p1;
            }
            for (size_t i = 0; i < n; ++i) {
                size_t j = (i + 1) % n;
                r.roads.push_back({ b[i].u, b[i].v, a[j].u, a[j].v, hw * 2 });                  // the straight leg
                if (round[j]) { RoadSeg arc{ a[j].u, a[j].v, b[j].u, b[j].v, hw * 2 }; arc.curve = true; r.roads.push_back(arc); }
            }
            lanePoly = ring;
            // the main street (laid before) must end on the ring, not 5 m past it: that stub was merged away and the
            // ring and the lanes then ended mid-way on the main street - refused (v0.49.0 log)
            for (auto& rd : r.roads) {
                if (rd.curve || fabsf(rd.v1 - roadV) > 0.5f || fabsf(rd.v2 - roadV) > 0.5f) continue;
                float L2, R2;
                if (!PolyScan(lanePoly, roadV, L2, R2)) continue;
                float a = std::max(std::min(rd.u1, rd.u2), L2), b = std::min(std::max(rd.u1, rd.u2), R2);
                if (b - a > 1) { rd.u1 = a; rd.u2 = b; }
            }
            r.roads.erase(std::remove_if(r.roads.begin(), r.roads.end(), [&](const RoadSeg& rd) {
                return !rd.curve && fabsf(rd.v1 - roadV) < 0.5f && fabsf(rd.u2 - rd.u1) < 1; }), r.roads.end());
        }
        std::vector<float> hvs{ roadV };
        for (float v = roadV + P; v < maxV - front - 4; v += P) hvs.push_back(v);
        for (float v = roadV - P; v > minV + front + 4; v -= P) hvs.push_back(v);
        std::vector<float> vus;
        for (float u = crossU; u < maxU - 30; u += crossSp) vus.push_back(u);
        for (float u = crossU - crossSp; u > minU + 30; u -= crossSp) vus.push_back(u);
        for (auto& x : vus) for (auto& st : r.stops) if (fabsf(x - st.u) < 35) x = st.u + (x >= st.u ? 35.0f : -35.0f);
        struct Lane { float v, L, R; };
        std::vector<Lane> lanes;
        for (float v : hvs) {
            float L, R;
            if (!PolyScan(lanePoly, v, L, R) || R - L < 30) continue;
            lanes.push_back({ v, L, R });
            if (v == roadV && roadHalf > 0) continue;            // the main street is already there
            if (roadHalf > 0) r.roads.push_back({ L, v, R, v, hw * 2 });
            else r.paths.push_back({ L, v, R, v });
            if (roadHalf > 0) { r.paths.push_back({ L, v - hw - 2, R, v - hw - 2 }); r.paths.push_back({ L, v + hw + 2, R, v + hw + 2 }); }
        }
        if (roadHalf == 0 && !lanes.empty()) {                   // the main lane too when there is no street at all
            bool have = false; for (auto& p : r.paths) if (fabsf(p.v1 - roadV) < 0.5f && fabsf(p.v2 - roadV) < 0.5f) have = true;
            float L, R; if (!have && PolyScan(in.poly, roadV, L, R)) r.paths.push_back({ L, roadV, R, roadV });
        }
        {   std::vector<V2> tp; for (auto& q : lanePoly) tp.push_back({ q.v, q.u });
            for (float u : vus) {
                float B, T;
                if (!PolyScan(tp, u, B, T) || T - B < 30) continue;
                std::vector<float> cuts{ B };
                for (auto& ln : lanes) if (ln.v > B + 5 && ln.v < T - 5 && u > ln.L && u < ln.R) cuts.push_back(ln.v);
                cuts.push_back(T); std::sort(cuts.begin(), cuts.end());
                for (size_t k = 0; k + 1 < cuts.size(); ++k) if (cuts[k + 1] - cuts[k] > 12) {
                    if (roadHalf > 0) r.roads.push_back({ u, cuts[k], u, cuts[k + 1], hw * 2 });
                    else r.paths.push_back({ u, cuts[k], u, cuts[k + 1] });
                }
                if (roadHalf > 0) { r.paths.push_back({ u - hw - 2, B, u - hw - 2, T }); r.paths.push_back({ u + hw + 2, B, u + hw + 2, T }); }
            }
        }
        std::sort(lanes.begin(), lanes.end(), [&](const Lane& a, const Lane& b) { return fabsf(a.v - roadV) < fabsf(b.v - roadV); });
        // Stops spread over the whole area, not in a row on the main street: greedy cover of the area (points every
        // 25 m) within the walking radius; each stop goes on the street and spot that covers most still-uncovered
        // ground, away from crossroads. The stop stands on the street axis and the street is cut where it stands.
        if (in.autoStops && in.stopType >= 0 && roadHalf > 0) {
            const BuildingType& st = all[in.stopType];
            float hl = 0;
            for (auto& c : st.conns) if (c.kind == 1) hl = std::max(hl, fabsf(c.x));
            if (hl < 5) hl = st.sx / 2;
            std::vector<V2> pts;
            for (float v = minV + 12; v < maxV; v += 25) for (float u = minU + 12; u < maxU; u += 25) if (PolyContains(in.poly, u, v)) pts.push_back({ u, v });
            std::vector<char> cov(pts.size(), 0);
            const float R = in.walkRadius * 0.8f;   // spare for the walk along streets, not straight
            for (int it = 0; it < 16; ++it) {
                int best = 0; const Lane* bl = nullptr; float bu = 0;
                for (auto& ln : lanes) for (float u = ln.L + hl + 20; u < ln.R - hl - 20; u += 20) {
                    bool nearCross = false;
                    for (float x : vus) if (fabsf(u - x) < hl + 16) nearCross = true;
                    for (auto& sp : r.stops) if (hypotf(sp.u - u, sp.v - ln.v) < 2 * hl + 20) nearCross = true;
                    if (nearCross) continue;
                    int n = 0;
                    for (size_t k = 0; k < pts.size(); ++k) if (!cov[k] && hypotf(pts[k].u - u, pts[k].v - ln.v) <= R) ++n;
                    if (n > best) { best = n; bl = &ln; bu = u; }
                }
                if (!bl || best < 1) break;
                Placed pb{ in.stopType, bu + st.cx, bl->v + st.cz, st.sx, st.sz, false };
                pb.rot = 0; pb.ou = bu; pb.ov = bl->v; pb.stop = true;
                r.buildings.push_back(pb);
                r.stops.push_back({ bu, bl->v });
                Log("auto stop at u %.0f v %.0f, street cut %.0f..%.0f", bu, bl->v, bu - hl, bu + hl);
                for (size_t k = 0; k < pts.size(); ++k) if (hypotf(pts[k].u - bu, pts[k].v - bl->v) <= R) cov[k] = 1;
                std::vector<RoadSeg> cut;           // the street is cut where the stop stands
                for (auto& rd : r.roads) {
                    bool here = fabsf(rd.v1 - bl->v) < 0.5f && fabsf(rd.v2 - bl->v) < 0.5f && bu > std::min(rd.u1, rd.u2) && bu < std::max(rd.u1, rd.u2);
                    if (!here) { cut.push_back(rd); continue; }
                    float a = std::min(rd.u1, rd.u2), b = std::max(rd.u1, rd.u2);
                    if (bu - hl - a > 10) cut.push_back({ a, rd.v1, bu - hl, rd.v1, rd.width });
                    if (b - bu - hl > 10) cut.push_back({ bu + hl, rd.v1, b, rd.v1, rd.width });
                }
                r.roads.swap(cut);
            }
        }
        if (in.fillAll && in.houseTypes.size() > 1) {
            // "build up everything": the queue of houses is sized to the street frontage, in the requested shares,
            // biggest first - so the big ones line the central streets and the small ones the outer ones
            float wsum = 0, front = 0, len = 0;
            std::vector<float> sh(in.houseTypes.size(), 1.0f);
            for (size_t k = 0; k < sh.size(); ++k) { if (k < in.houseWeights.size()) sh[k] = std::max(0.0f, in.houseWeights[k]); wsum += sh[k]; }
            for (size_t k = 0; k < sh.size(); ++k) { sh[k] = wsum > 0 ? sh[k] / wsum : 1.0f / sh.size(); front += sh[k] * (MakeItem(in.houseTypes[k]).w - kSide + std::max(gap, 2.0f)); }
            for (auto& ln : lanes) len += 2 * (ln.R - ln.L);
            int n = (int)(len / std::max(4.0f, front) * 0.9f) + 2;   // corners and cross streets eat ~10 %
            houses.clear();
            for (size_t k = 0; k < sh.size(); ++k) for (int c = 0; c < (int)ceilf(n * sh[k]); ++c) houses.push_back(MakeItem(in.houseTypes[k]));
            std::stable_sort(houses.begin(), houses.end(), [&](const Item& x, const Item& y) { return all[x.type].capacity > all[y.type].capacity; });
        }
        auto inside = [&](float cu, float cv, float w, float d) {
            return PolyContains(in.poly, cu - w / 2, cv - d / 2) && PolyContains(in.poly, cu + w / 2, cv - d / 2) &&
                   PolyContains(in.poly, cu - w / 2, cv + d / 2) && PolyContains(in.poly, cu + w / 2, cv + d / 2);
        };
        auto overlaps = [&](float cu, float cv, float w, float d) {
            for (auto& b : r.buildings)
                if (fabsf(cu - b.u) < (w + b.w) / 2 + kSide && fabsf(cv - b.v) < (d + b.d) / 2 + kSide) return true;
            for (auto& rd : r.roads) {      // distance from the footprint to the road's axis (roads may run slanted)
                float m = rd.width / 2 + 1;
                if (cu + w / 2 + m < std::min(rd.u1, rd.u2) || cu - w / 2 - m > std::max(rd.u1, rd.u2) ||
                    cv + d / 2 + m < std::min(rd.v1, rd.v2) || cv - d / 2 - m > std::max(rd.v1, rd.v2)) continue;   // quick reject (this test ran thousands of times per plan)
                float len = hypotf(rd.u2 - rd.u1, rd.v2 - rd.v1);
                int n = std::max(1, (int)(len / 2));
                for (int k = 0; k <= n; ++k) {
                    float pu = rd.u1 + (rd.u2 - rd.u1) * k / n, pv = rd.v1 + (rd.v2 - rd.v1) * k / n;
                    if (fabsf(pu - cu) < w / 2 + m && fabsf(pv - cv) < d / 2 + m) return true;
                }
            }
            return false;
        };
        // one building on lane ln, side +1 (north of the street, entrance facing -v) or -1, its middle at u
        auto tryAt = [&](const Item& it, const Lane& ln, int side, float u, int net, int role, int grp, bool commit) {
            float w0 = it.w - kSide, d0 = it.d - kApron;
            int roadRot = -1;
            if (all[it.type].needsRoad) {          // the road connection looks at the street (driveway = short stub)
                for (auto& c : all[it.type].conns) if (c.kind == 1) {
                    float l = hypotf(c.dx, c.dz);
                    for (int rr = 0; rr < 4 && l > 0.01f; ++rr) if (Turn(rr, c.dx, c.dz).v * -side > 0.7f * l) { roadRot = rr; break; }
                    break;
                }
                if (roadRot >= 0) { const BuildingType& bt = all[it.type]; w0 = (roadRot & 1) ? bt.sz : bt.sx; d0 = (roadRot & 1) ? bt.sx : bt.sz; }
            }
            float cv = ln.v + side * (hw + front + d0 / 2);
            if (u - w0 / 2 < ln.L + margin || u + w0 / 2 > ln.R - margin) return false;
            if (!inside(u, cv, w0, d0) || overlaps(u, cv, w0, d0) || busy(u, cv, w0, d0) || heightRange(u, cv, w0, d0) > in.maxDiff) return false;
            if (!commit) return true;
            Placed pb{ it.type, u, cv, w0, d0, it.rotated };
            pb.rot = side > 0 ? (it.rot + 2) & 3 : it.rot;
            if (roadRot >= 0) { pb.rot = roadRot; pb.rotated = (roadRot & 1) != 0; }
            V2 off = Turn(pb.rot, all[it.type].cx, all[it.type].cz);
            pb.ou = u - off.u; pb.ov = cv - off.v;
            pb.net = (signed char)net; pb.role = (signed char)role; pb.grp = (short)grp;
            r.buildings.push_back(pb);
            return true;
        };
        // the free lot nearest to a point, or the one with the best score
        auto placeBest = [&](const Item& it, const std::function<float(float, float)>& score, int net, int role, int grp) {
            float best = -1e18f; const Lane* bl = nullptr; int bs = 0; float bu = 0;
            for (auto& ln : lanes) for (int side = -1; side <= 1; side += 2)
                for (float u = ln.L + margin + it.w / 2; u < ln.R - margin - it.w / 2; u += 6) {
                    float cv = ln.v + side * (hw + front + (it.d - kApron) / 2);
                    float sc = score(u, cv);
                    if (sc <= best) continue;
                    if (tryAt(it, ln, side, u, net, role, grp, false)) { best = sc; bl = &ln; bs = side; bu = u; }
                }
            return bl && tryAt(it, *bl, bs, bu, net, role, grp, true);
        };
        // utility nodes at their spots
        int unplacedSvc = 0;
        if (sites) for (int k = 0; k < 4; ++k) {
            if (!in.utilBuild[k] || utilType[k][0] < 0) continue;
            for (size_t s = 0; s < sites[k].size(); ++s) {
                V2 st = sites[k][s]; short g = (short)utilGrp[k]++;
                auto nearSite = [&](float u, float v) { return -hypotf(u - st.u, v - st.v); };
                if (!placeBest(MakeItem(utilType[k][0]), nearSite, k, 0, g)) { ++r.unplaced; continue; }
                if (k == 3 && in.powerTrafo && utilType[3][1] >= 0 && g % 6 == 0) placeBest(MakeItem(utilType[3][1]), nearSite, 3, 1, g);
            }
        }
        // services: farthest from the same kind; the first of a kind as close to the stop / centre as possible
        V2 centre = r.stops.empty() ? V2{ crossU, roadV } : r.stops[0];
        if (in.boiler && in.utilBuild[2]) {
            int bt = -1; for (size_t t = 0; t < all.size(); ++t) if (all[t].id == "heating_plant_small") bt = (int)t;
            if (bt >= 0) {
                auto farScore = [&](float u, float v) { return hypotf(u - centre.u, v - centre.v); };
                if (!placeBest(MakeItem(bt), farScore, -1, 0, 0)) ++unplacedSvc;
            }
        }
        for (auto& it : svcs) {
            const std::string& cat = all[it.type].category;
            auto score = [&](float u, float v) {
                float dmin = 1e9f;
                for (auto& b : r.buildings) if (all[b.type].category == cat) dmin = std::min(dmin, hypotf(u - b.u, v - b.v));
                float dc = hypotf(u - centre.u, v - centre.v);
                return dmin > 1e8f ? -dc : dmin - 0.05f * dc;
            };
            if (!placeBest(it, score, -1, 0, 0)) ++unplacedSvc;
        }
        // houses: lane by lane, both sides, from the main street outwards
        size_t hi = 0; int cap = 0;
        for (auto& ln : lanes) for (int side = -1; side <= 1 && hi < houses.size(); side += 2) {
            float u = ln.L + margin;
            int fails = 0;
            while (hi < houses.size() && u < ln.R - margin && (in.fillAll || cap < in.population)) {
                const Item& it = houses[hi];
                float cu = u + (it.w - kSide) / 2;
                bool ok = tryAt(it, ln, side, cu, -1, 0, 0, true);
                if (!ok) {   // a gap too small for this one: the next smaller type in the queue may still fit
                    for (size_t j = hi + 1; j < houses.size() && !ok; ++j) {
                        if (houses[j].type == it.type || houses[j].w >= it.w) continue;
                        float cj = u + (houses[j].w - kSide) / 2;
                        if (tryAt(houses[j], ln, side, cj, -1, 0, 0, true)) { std::swap(houses[hi], houses[j]); cu = cj; ok = true; }
                    }
                }
                if (ok) { const Item& pl = houses[hi]; cap += all[pl.type].capacity; ++hi; u = cu + (pl.w - kSide) / 2 + std::max(gap, 2.0f); fails = 0; }
                else { u += 3; if (++fails > 400) break; }
            }
        }
        r.unplaced += (int)(houses.size() - hi) + unplacedSvc;
    } else if (!in.blocks) {
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
            Item it = util ? pending.front().it : items[i];
            bool side = false;      // turned a quarter: entrance along the row, the footpath link is routed round it
            if (it.d > vhi - vlo + 0.01f || u + it.w > R - margin) {
                // dense packing: the end of a row (or a shallow strip) still takes the building turned a quarter
                Item t = it;
                t.w = it.d - kApron + kSide; t.d = it.w - kSide + kApron;
                t.rot = (it.rot + 1) & 3; t.rotated = !it.rotated;
                if (t.d <= vhi - vlo + 0.01f && u + t.w <= R - margin) { it = t; side = true; }
            }
            // A utility node with vehicles (substation booth, transformer yard, heat point...) is turned so that its road
            // connection looks at the nearest street and stands on the street side of the row: its driveway is a
            // short straight stub. Its connections may be on a short side, so any of the four turns is allowed.
            bool roadTurn = false; int roadRot = 0; float rfw = 0, rfd = 0, roadWant = 1;
            if (util && all[it.type].needsRoad && roadHalf > 0) {
                const BuildingType& t = all[it.type];
                float pu = u + it.w / 2, pv = (vlo + vhi) / 2, nearV = roadV, bd = 1e9f;
                for (auto& rd : r.roads) if (fabsf(rd.v1 - rd.v2) < 0.01f && fabsf(rd.v1 - pv) < bd) { bd = fabsf(rd.v1 - pv); nearV = rd.v1; }
                roadWant = nearV < pv ? -1.0f : 1.0f;
                for (auto& c : t.conns) if (c.kind == 1) {
                    float l = hypotf(c.dx, c.dz);
                    for (int rr = 0; rr < 4 && l > 0.01f; ++rr) if (Turn(rr, c.dx, c.dz).v * roadWant > 0.7f * l) { roadRot = rr; roadTurn = true; break; }
                    break;
                }
                if (roadTurn) {
                    rfw = (roadRot & 1) ? t.sz : t.sx; rfd = (roadRot & 1) ? t.sx : t.sz;
                    it.w = rfw + kSide; it.d = rfd + 2.0f; it.rot = roadRot; it.rotated = (roadRot & 1) != 0;
                }
            }
            if (it.d > vhi - vlo + 0.01f) { if (util) { pending.pop_front(); continue; } break; }
            if (u + it.w > R - margin) break;
            if (u < spineU + corridor && u + it.w > spineU - corridor) { u = spineU + corridor; continue; }  // leave the spine free
            float cu = u + it.w / 2;
            float cv = north ? vlo + it.d / 2 : vhi - it.d / 2;
            if (roadTurn) cv = roadWant < 0 ? vlo + it.d / 2 : vhi - it.d / 2;        // pressed to the street side
            if (util && all[it.type].needsRoad) {
                // a node with vehicles only in a row facing a street: anywhere else its driveway loops round the rows
                float gapToStreet = 1e9f;
                for (auto& rd : r.roads) if (fabsf(rd.v1 - rd.v2) < 0.01f && cu > std::min(rd.u1, rd.u2) && cu < std::max(rd.u1, rd.u2))
                    gapToStreet = std::min(gapToStreet, fabsf(cv - rd.v1) - it.d / 2 - rd.width / 2);
                if (!r.roads.empty() && gapToStreet > 16.0f) { pending.pop_front(); ++r.unplaced; continue; }
            }
            if (busy(cu, cv, it.w, it.d)) { u += 6; ++r.busySkipped; continue; }                        // something is already built here
            if (heightRange(cu, cv, it.w, it.d) > in.maxDiff) { u += 6; ++r.steepSkipped; continue; }   // too steep here
            Placed pb{ it.type, cu, cv, it.w - (side ? kApron : kSide), it.d - (side ? kSide : kApron), it.rotated };
            pb.rot = side ? it.rot : (it.rot + (north ? 2 : 0)) & 3;   // the entrance looks at this row's footpath
            if (roadTurn) { pb.rot = roadRot; pb.w = rfw; pb.d = rfd; pb.rotated = (roadRot & 1) != 0; }
            else if (all[it.type].needsRoad && roadHalf > 0) {
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

    // Links between neighbouring strips (and the street): a short footpath across, placed as close to the stop
    // as possible where it stays inside both footpaths' extents and does not run through a building.
    auto blocked = [&](float u, float v1, float v2) {
        if (v1 > v2) std::swap(v1, v2);
        for (auto& b : r.buildings)
            if (!b.stop && fabsf(b.u - u) < b.w / 2 + 3.5f && b.v + b.d / 2 > v1 && b.v - b.d / 2 < v2) return true;   // the game refuses a path along a building wall (its apron)
        return false;
    };
    auto link = [&](const StripInfo& a, const StripInfo& b) {
        float lo = std::max(a.l, b.l), hi = std::min(a.r, b.r);
        if (hi < lo) return false;
        for (int k = 0; k < 400; ++k) {
            float cu = spineU + (k % 2 ? 1 : -1) * (float)((k + 1) / 2) * 2.0f;
            if (cu < lo || cu > hi) { if (spineU + k < lo && spineU - k > hi) break; continue; }
            if (blocked(cu, a.pathV, b.pathV)) continue;
            // a stop stands on the street: the footpath stops at its edge (the game refuses a path into the model),
            // the stop's own entrances take people on
            float va = a.pathV;
            for (auto& st : r.buildings) if (st.stop && fabsf(cu - st.u) < st.w / 2 + 1.0f) {
                float sgn = b.pathV > a.pathV ? 1.0f : -1.0f, edge = st.v + sgn * (st.d / 2 + 1.0f);
                if ((edge - a.pathV) * sgn > 0 && (b.pathV - edge) * sgn > 5.5f) va = edge;
            }
            r.paths.push_back({ cu, va, cu, b.pathV });
            return true;
        }
        Log("plan: no link between footpaths v %.1f [%.1f..%.1f] and v %.1f [%.1f..%.1f], spine %.1f", a.pathV, a.l, a.r, b.pathV, b.l, b.r, spineU);
        return false;
    };
    for (int side = 0; side < 2; ++side) {
        auto& list = side == 0 ? stripsUp : stripsDown;
        StripInfo prev{}; bool havePrev = false;
        if (roadHalf > 0 && !r.roads.empty()) {
            float pv = side == 0 ? roadV + roadHalf + 2 : roadV - roadHalf - 2;     // the pavement on this side
            float ru1 = 1e9f, ru2 = -1e9f;            // the whole street, not only its first piece (stops cut it)
            for (auto& rd : r.roads) if (fabsf(rd.v1 - roadV) < 0.01f && fabsf(rd.v2 - roadV) < 0.01f) { ru1 = std::min(ru1, std::min(rd.u1, rd.u2)); ru2 = std::max(ru2, std::max(rd.u1, rd.u2)); }
            if (ru1 > ru2) { ru1 = r.roads[0].u1; ru2 = r.roads[0].u2; }
            prev = { pv, ru1, ru2 }; havePrev = true;
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
    } else {
        // ===== Microdistricts =====
        // A grid of streets cuts the area into blocks. Each block: houses along its edges with their backs to the
        // streets and the entrances to the courtyard, a ring footpath in the courtyard, gates through the edge rows
        // from the ring to every street, and the inside of the courtyard filled with strips (services first: school,
        // kindergarten...), each strip's footpath joining the ring. Services are dealt out over the blocks so that
        // the same kind never stands twice side by side.
        const float S = in.streetSpacing >= 80 ? in.streetSpacing : 1e6f;
        const float hw = roadHalf;
        const float inset = 3.0f;                       // from the street's edge to the backs of the houses
        std::vector<float> hvs, vus;                    // horizontal streets (v), vertical streets (u)
        if (hw > 0) {
            hvs.push_back(roadV);
            for (float v = roadV + S; v < maxV - 0.6f * S; v += S) hvs.push_back(v);
            for (float v = roadV - S; v > minV + 0.6f * S; v -= S) hvs.push_back(v);
            std::sort(hvs.begin(), hvs.end());
            for (float u = crossU; u < maxU - 0.6f * S; u += S) vus.push_back(u);
            for (float u = crossU - S; u > minU + 0.6f * S; u -= S) vus.push_back(u);
            if (!in.crossRoad && S > 1e5f) vus.clear();
            std::sort(vus.begin(), vus.end());
            for (auto& x : vus) for (auto& st : r.stops) if (fabsf(x - st.u) < 35) x = st.u + (x >= st.u ? 35.0f : -35.0f);
            for (float v : hvs) {
                if (v == roadV) continue;
                float L, R;
                if (!PolyScan(in.poly, v, L, R) || R - L < 30) continue;
                r.roads.push_back({ L, v, R, v, hw * 2 });
                r.paths.push_back({ L, v - hw - 2, R, v - hw - 2 }); r.paths.push_back({ L, v + hw + 2, R, v + hw + 2 });   // pavements: walking lines only
            }
            std::vector<V2> tp; for (auto& q : in.poly) tp.push_back({ q.v, q.u });
            for (float u : vus) {
                float B, T;
                if (!PolyScan(tp, u, B, T) || T - B < 30) continue;
                std::vector<float> cuts{ B };
                for (auto& rd : r.roads) if (fabsf(rd.v1 - rd.v2) < 0.01f && rd.v1 > B + 5 && rd.v1 < T - 5 && u > std::min(rd.u1, rd.u2) && u < std::max(rd.u1, rd.u2)) cuts.push_back(rd.v1);
                cuts.push_back(T); std::sort(cuts.begin(), cuts.end());
                for (size_t k = 0; k + 1 < cuts.size(); ++k) if (cuts[k + 1] - cuts[k] > 10) r.roads.push_back({ u, cuts[k], u, cuts[k + 1], hw * 2 });
                r.paths.push_back({ u - hw - 2, B, u - hw - 2, T }); r.paths.push_back({ u + hw + 2, B, u + hw + 2, T });
            }
        }
        // the blocks, nearest to the middle of the main street first
        struct Blk { float u0, u1, v0, v1; bool sS, sN, sW, sE; float sv, nv, wu, eu; };
        std::vector<Blk> blks;
        {
            std::vector<float> ub{ minU }, vb{ minV };
            for (float u : vus) ub.push_back(u);
            ub.push_back(maxU);
            for (float v : hvs) vb.push_back(v);
            vb.push_back(maxV);
            auto edge = [&](float s, bool isStreetMain) { return isStreetMain ? roadBand + 1.0f : hw + inset; };
            for (size_t a = 0; a + 1 < ub.size(); ++a)
                for (size_t c = 0; c + 1 < vb.size(); ++c) {
                    Blk b{};
                    b.sW = a > 0; b.sE = a + 2 < ub.size(); b.sS = c > 0; b.sN = c + 2 < vb.size();
                    b.wu = ub[a]; b.eu = ub[a + 1]; b.sv = vb[c]; b.nv = vb[c + 1];
                    b.u0 = ub[a] + (b.sW ? edge(ub[a], false) : margin);
                    b.u1 = ub[a + 1] - (b.sE ? edge(ub[a + 1], false) : margin);
                    b.v0 = vb[c] + (b.sS ? edge(vb[c], vb[c] == roadV) : margin);
                    b.v1 = vb[c + 1] - (b.sN ? edge(vb[c + 1], vb[c + 1] == roadV) : margin);
                    if (b.u1 - b.u0 < 40 || b.v1 - b.v0 < 40) continue;
                    blks.push_back(b);
                }
            std::sort(blks.begin(), blks.end(), [&](const Blk& x, const Blk& y) {
                return hypotf((x.u0 + x.u1) / 2 - crossU, (x.v0 + x.v1) / 2 - roadV) < hypotf((y.u0 + y.u1) / 2 - crossU, (y.v0 + y.v1) / 2 - roadV); });
        }
        // services dealt out over the blocks, houses taken in order
        std::vector<Item> houses, svcAll;
        for (auto& it : items) (all[it.type].category == "LIVING" ? houses : svcAll).push_back(it);
        size_t hi = 0;
        int unplacedSvc = 0;
        auto inside = [&](float cu, float cv, float w, float d) {
            return PolyContains(in.poly, cu - w / 2, cv - d / 2) && PolyContains(in.poly, cu + w / 2, cv - d / 2) &&
                   PolyContains(in.poly, cu - w / 2, cv + d / 2) && PolyContains(in.poly, cu + w / 2, cv + d / 2);
        };
        float lastDepth = 0;
        // place one item; side 0 S (back at v=edge, entrance +v), 1 N, 2 W (back at u=edge, entrance +u), 3 E.
        // t = start along the side; returns the frontage used, 0 = did not fit here
        auto putAt = [&](const Item& it0, int side, float edgeCoord, float t, int net, int role, int grp, bool roadOut) -> float {
            const BuildingType& bt = all[it0.type];
            int rot; float F, D;               // frontage along the side, depth across it (raw, no apron)
            if (roadOut) {                     // road connection toward the street behind the row
                V2 want = side == 0 ? V2{ 0, -1 } : side == 1 ? V2{ 0, 1 } : side == 2 ? V2{ -1, 0 } : V2{ 1, 0 };
                rot = -1;
                for (auto& c : bt.conns) if (c.kind == 1 && rot < 0) {
                    float l = hypotf(c.dx, c.dz);
                    for (int rr = 0; rr < 4 && l > 0.01f; ++rr) { V2 n = Turn(rr, c.dx, c.dz); if (n.u * want.u + n.v * want.v > 0.7f * l) { rot = rr; break; } }
                }
                if (rot < 0) roadOut = false;      // no road connection (water / sewage node): placed like a house
            }
            if (roadOut) {
                float eu = (rot & 1) ? bt.sz : bt.sx, ev = (rot & 1) ? bt.sx : bt.sz;   // extents along u / v
                F = side < 2 ? eu : ev; D = side < 2 ? ev : eu;
                D += 2.0f;                       // room for its yard / doors on the courtyard side
            }
            if (!roadOut) {
                // the entrance to the courtyard: it.rot makes it face +v
                rot = (it0.rot + (side == 0 ? 0 : side == 1 ? 2 : side == 2 ? 3 : 1)) & 3;
                float rawF = it0.w - kSide, rawD = it0.d - kApron;      // MakeItem: w along the entrance side
                F = rawF; D = rawD + kApron;
            }
            float rawDepth = roadOut ? D - 2.0f : D - kApron;
            float cu, cv, w, d;
            if (side < 2) { cu = t + F / 2; cv = side == 0 ? edgeCoord + rawDepth / 2 : edgeCoord - rawDepth / 2; w = F; d = rawDepth; }
            else { cv = t + F / 2; cu = side == 2 ? edgeCoord + rawDepth / 2 : edgeCoord - rawDepth / 2; w = rawDepth; d = F; }
            if (!inside(cu, cv, w, d)) return -F;
            if (busy(cu, cv, w, d) || heightRange(cu, cv, w, d) > in.maxDiff) return -F;
            Placed pb{ it0.type, cu, cv, w, d, (rot & 1) != 0 };
            pb.rot = rot;
            V2 off = Turn(rot, bt.cx, bt.cz);
            pb.ou = cu - off.u; pb.ov = cv - off.v;
            pb.net = (signed char)net; pb.role = (signed char)role; pb.grp = (short)grp;
            r.buildings.push_back(pb);
            lastDepth = std::max(lastDepth, D);
            return F;
        };
        std::deque<Pend> nodeQ;          // utility nodes waiting for a spot in an edge row
        auto wantNodes = [&](float hu, float hv2) {
            if (!sites) return;
            for (int k = 0; k < 4; ++k) {
                if (!in.utilBuild[k] || utilType[k][0] < 0) continue;
                for (size_t s = 0; s < sites[k].size(); ++s) {
                    if (siteDone[k][s] || hypotf(sites[k][s].u - hu, sites[k][s].v - hv2) > 40) continue;
                    siteDone[k][s] = 1;
                    short g = (short)utilGrp[k]++;
                    nodeQ.push_back({ MakeItem(utilType[k][0]), (signed char)k, 0, g });
                    if (utilType[k][1] >= 0 && (k < 3 ? in.utilChain : (in.powerTrafo && g % 6 == 0))) nodeQ.push_back({ MakeItem(utilType[k][1]), (signed char)k, 1, g });
                }
            }
        };
        // one edge row from a to b along the side, leaving the gate free; street-side services and nodes go here
        auto edgeRow = [&](int side, float edgeCoord, float a, float b, float& gate, bool gateFixed, std::vector<Item>& svcHere, bool fill, bool street) -> float {
            float t = a + 1.0f, maxD = 0;
            int fails = 0;
            while (t < b - 6) {
                bool isNode = street && !nodeQ.empty();      // vehicles need the street behind the row
                Item it; int net = -1, role = 0, grp = 0; bool roadOut = false; bool isSvc = false;
                if (isNode) { it = nodeQ.front().it; net = nodeQ.front().net; role = nodeQ.front().role; grp = nodeQ.front().grp; roadOut = true; }
                else if (street && std::any_of(svcHere.begin(), svcHere.end(), [&](const Item& x) { return all[x.type].needsRoad; })) {
                    auto pos = std::find_if(svcHere.begin(), svcHere.end(), [&](const Item& x) { return all[x.type].needsRoad; });
                    std::rotate(svcHere.begin(), pos, pos + 1);       // the street-side service to the front
                    it = svcHere.front(); roadOut = true; isSvc = true;
                }
                else if (fill && hi < houses.size()) it = houses[hi];
                else break;
                float Fguess = roadOut ? std::max(all[it.type].sx, all[it.type].sz) : it.w - kSide;
                if (t < gate + 7 && t + Fguess > gate - 7) {
                    // the gate moves into the gap before this building (only a stop's gate stays where it is)
                    if (!gateFixed && t + 7 < b) { gate = t + 6; t = gate + 7; continue; }
                    t = gate + 7; continue;
                }
                if (t + Fguess > b) {
                    if (isNode) { nodeQ.pop_front(); ++unplacedSvc; continue; }
                    if (isSvc) break;
                    break;
                }
                lastDepth = 0;
                float used = putAt(it, side, edgeCoord, t, net, role, grp, roadOut);
                if (used <= 0) {
                    if (used == 0) { if (isNode) { nodeQ.pop_front(); ++unplacedSvc; } else if (isSvc) { svcHere.erase(svcHere.begin()); ++unplacedSvc; } else ++hi; continue; }
                    t += 6; if (++fails > 200) break; continue;
                }
                maxD = std::max(maxD, lastDepth);
                const Placed& pb = r.buildings.back();
                if (isNode) nodeQ.pop_front();
                else if (isSvc) svcHere.erase(svcHere.begin());
                else { ++hi; wantNodes(pb.u, pb.v); }
                t += used + gap;
            }
            return maxD;
        };
        // strips inside the courtyard: rows along u, footpath in the middle running the full width to the ring
        float lastStripTop = 0;
        auto courtyard = [&](float cu0, float cu1, float cv0, float cv1, std::vector<Item>& svcHere, bool fill) {
            float v = cv0;
            while (true) {
                // a service too deep for what is left of the courtyard goes on to the next block
                for (size_t k = 0; k < svcHere.size();) {
                    if (svcHere[k].d + band > cv1 - v || svcHere[k].w > cu1 - cu0) { svcAll.push_back(svcHere[k]); svcHere.erase(svcHere.begin() + k); }
                    else ++k;
                }
                // the deepest of the next items decides the strip
                float depth = 0;
                for (auto& s : svcHere) depth = std::max(depth, s.d);
                for (size_t k = hi; k < houses.size() && k < hi + 8 && fill; ++k) depth = std::max(depth, houses[k].d);
                if (depth <= 0) break;
                float h = 2 * depth + band;
                bool two = v + h <= cv1, one = !two && v + depth + band <= cv1;
                if (!two && !one) break;
                float pathV = v + depth + band / 2;
                int placedHere = 0;
                for (int row = 0; row < (two ? 2 : 1); ++row) {
                    float u = cu0 + 1;
                    int fails = 0;
                    while (true) {
                        Item it; bool isSvc = false;
                        if (!svcHere.empty()) { it = svcHere.front(); isSvc = true; }
                        else if (fill && hi < houses.size()) it = houses[hi];
                        else break;
                        if (it.d > depth + 0.01f) { if (isSvc) { svcHere.push_back(svcHere.front()); svcHere.erase(svcHere.begin()); if (++fails > 8) break; continue; } break; }
                        if (u + it.w > cu1) break;
                        float cu = u + it.w / 2, fd = it.d - kApron;
                        float cv = row == 0 ? pathV - band / 2 - kApron - fd / 2 : pathV + band / 2 + kApron + fd / 2;
                        float w = it.w - kSide;
                        if (!inside(cu, cv, w, fd) || busy(cu, cv, w, fd) || heightRange(cu, cv, w, fd) > in.maxDiff) { u += 6; if (++fails > 200) break; continue; }
                        Placed pb{ it.type, cu, cv, w, fd, it.rotated };
                        pb.rot = (it.rot + (row == 0 ? 0 : 2)) & 3;
                        if (all[it.type].needsRoad) {      // a road connection looks along the strip toward the nearer ring side
                            for (auto& c : all[it.type].conns) if (c.kind == 1) {
                                float want = (cu - cu0 < cu1 - cu) ? -1.0f : 1.0f;
                                if (Turn(pb.rot, c.dx, c.dz).u * want < 0 && fabsf(Turn(pb.rot, c.dx, c.dz).u) > 0.5f) pb.rot = (pb.rot + 2) & 3;
                                break;
                            }
                        }
                        V2 off = Turn(pb.rot, all[it.type].cx, all[it.type].cz);
                        pb.ou = cu - off.u; pb.ov = cv - off.v;
                        r.buildings.push_back(pb); ++placedHere;
                        if (isSvc) svcHere.erase(svcHere.begin()); else ++hi;
                        u += it.w + gap;
                    }
                }
                if (placedHere) r.paths.push_back({ cu0 - 3, pathV, cu1 + 3, pathV });
                v += (two ? h : depth + band) + gap;
                if (placedHere) lastStripTop = v;
            }
        };
        for (size_t bi = 0; bi < blks.size(); ++bi) {
            const Blk& b = blks[bi];
            bool fill = hi < houses.size();
            if (!fill && svcAll.empty()) break;
            // services of this block: one of each kind (never two schools side by side); once the houses are used
            // up, all that is left
            std::vector<Item> svc;
            for (size_t k = 0; k < svcAll.size();) {
                bool have = false;
                for (auto& x : svc) if (all[x.type].category == all[svcAll[k].type].category) have = true;
                // Soviet norm (SNiP II-60-75): a kind is not repeated while one already stands within its walking
                // radius of this block's middle, so schools / kindergartens / shops spread to cover the whole area
                if (!have && fill) {
                    float R = NormRadius(all[svcAll[k].type].category);
                    float bu = (b.u0 + b.u1) / 2, bv = (b.v0 + b.v1) / 2;
                    if (R > 0) for (auto& p : r.buildings)
                        if (p.type == svcAll[k].type && fabsf(p.u - bu) + fabsf(p.v - bv) < 0.6f * R) { have = true; break; }
                }
                if (!have || !fill) { svc.push_back(svcAll[k]); svcAll.erase(svcAll.begin() + k); } else ++k;
            }
            // gates: middle of each side, or at a stop standing on that side's street
            float gS = (b.u0 + b.u1) / 2, gN = gS, gW = (b.v0 + b.v1) / 2, gE = gW;
            float stopEdgeS = 0, stopEdgeN = 0; bool stopS = false, stopN = false;
            for (auto& st : r.buildings) if (st.stop && st.u > b.u0 + 15 && st.u < b.u1 - 15) {
                if (b.sS && fabsf(b.sv - roadV) < 0.01f && fabsf(st.ov - b.sv) < 1) { gS = st.u; stopS = true; stopEdgeS = st.v + st.d / 2 + 1; }
                if (b.sN && fabsf(b.nv - roadV) < 0.01f && fabsf(st.ov - b.nv) < 1) { gN = st.u; stopN = true; stopEdgeN = st.v - st.d / 2 - 1; }
            }
            // edge rows: street-side services and nodes go in the row facing the main street first
            float dS = edgeRow(0, b.v0, b.u0, b.u1, gS, stopS, svc, fill, b.sS);
            float dN = edgeRow(1, b.v1, b.u0, b.u1, gN, stopN, svc, fill, b.sN);
            float wv0 = b.v0 + std::max(dS, 4.0f) + 2, wv1 = b.v1 - std::max(dN, 4.0f) - 2;
            float dW = wv1 - wv0 > 20 ? edgeRow(2, b.u0, wv0, wv1, gW, false, svc, fill, b.sW) : 0;
            float dE = wv1 - wv0 > 20 ? edgeRow(3, b.u1, wv0, wv1, gE, false, svc, fill, b.sE) : 0;
            // the ring
            float rv0 = b.v0 + std::max(dS, 0.0f) + 1.5f, rv1 = b.v1 - std::max(dN, 0.0f) - 1.5f;
            float ru0 = b.u0 + std::max(dW, 0.0f) + 1.5f, ru1 = b.u1 - std::max(dE, 0.0f) - 1.5f;
            if (dS + dN + dW + dE <= 0) { for (auto& x : svc) svcAll.push_back(x); continue; }
            if (ru1 - ru0 > 8 && rv1 - rv0 > 8) {
                r.paths.push_back({ ru0, rv0, ru1, rv0 }); r.paths.push_back({ ru1, rv0, ru1, rv1 });
                r.paths.push_back({ ru1, rv1, ru0, rv1 }); r.paths.push_back({ ru0, rv1, ru0, rv0 });
            }
            // gates to the streets
            if (b.sS) r.paths.push_back({ gS, rv0, gS, stopS ? stopEdgeS : b.sv });
            if (b.sN) r.paths.push_back({ gN, rv1, gN, stopN ? stopEdgeN : b.nv });
            if (b.sW) r.paths.push_back({ ru0, gW, b.wu, gW });
            if (b.sE) r.paths.push_back({ ru1, gE, b.eu, gE });
            // the courtyard
            if (ru1 - ru0 > 30 && rv1 - rv0 > 30) {
                size_t nb0 = r.buildings.size(), np0 = r.paths.size();
                lastStripTop = 0;
                courtyard(ru0 + 3, ru1 - 3, rv0 + 3, rv1 - 3, svc, fill);
                // the strips sit at the bottom of the courtyard: centre them, the spare height becomes two narrow lawns
                // along the ring instead of one wide empty band
                float spare = lastStripTop > 0 ? (rv1 - 3) - (lastStripTop - gap) : 0;
                if (spare > 2) {
                    float sh = spare / 2;
                    bool ok = true;
                    for (size_t k = nb0; k < r.buildings.size() && ok; ++k) {
                        const Placed& p = r.buildings[k];
                        if (!inside(p.u, p.v + sh, p.w, p.d) || busy(p.u, p.v + sh, p.w, p.d)) ok = false;
                    }
                    if (ok) {
                        for (size_t k = nb0; k < r.buildings.size(); ++k) { r.buildings[k].v += sh; r.buildings[k].ov += sh; }
                        for (size_t k = np0; k < r.paths.size(); ++k) { r.paths[k].v1 += sh; r.paths[k].v2 += sh; }
                    }
                }
            }
            for (auto& x : svc) svcAll.push_back(x);       // did not fit: the next block takes it
        }
        r.unplaced = (int)(houses.size() - hi) + unplacedSvc + (int)svcAll.size();
    }

    // the last node of a pipe network hangs on the previous switch, so its own switch is not needed
    {
        int lastGrp[4] = { -1, -1, -1, -1 };
        for (auto& b : r.buildings) if (b.net >= 0 && b.role == 0) lastGrp[b.net] = std::max(lastGrp[b.net], (int)b.grp);
        r.buildings.erase(std::remove_if(r.buildings.begin(), r.buildings.end(), [&](const Placed& b) {
            return b.net >= 0 && b.net < 3 && b.role == 1 && b.grp >= lastGrp[b.net]; }), r.buildings.end());
        for (int k = 0; k < 4; ++k) r.utilNodes[k].clear();
        for (auto& b : r.buildings) if (b.net >= 0 && b.role == 0) r.utilNodes[b.net].push_back({ b.u, b.v });
    }

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
                if (fabsf(rd.u1 - rd.u2) < 0.01f) mark(block, 1, rd.u1 - rd.width / 2 + 0.5f, rd.v1, rd.u1 + rd.width / 2 - 0.5f, rd.v2);
                else mark(block, 1, rd.u1, rd.v1 - rd.width / 2 + 0.5f, rd.u2, rd.v1 + rd.width / 2 - 0.5f);    // nobody walks on the roadway
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
                if (in.layout == 2 && in.straightLinks && mode == 0 && !b.stop) {
                    // village: the entrance faces its street, so the footpath is one straight piece from the entrance
                    // to just inside the road's edge (the game joins a footpath that touches the road)
                    bool done = false;
                    for (auto& c : t.conns) {
                        if (c.kind != 0) continue;
                        V2 o = Turn(b.rot, c.x, c.z); o.u += b.ou; o.v += b.ov;
                        V2 n = Turn(b.rot, c.dx, c.dz); float nl = hypotf(n.u, n.v);
                        if (nl < 0.01f) continue;
                        n.u /= nl; n.v /= nl;
                        float bestT = 1e9f, crossT = 0;
                        for (auto& rd : r.roads) {
                            float r2u = rd.u2 - rd.u1, r2v = rd.v2 - rd.v1, den = n.u * r2v - n.v * r2u;
                            if (fabsf(den) < 1e-6f) continue;
                            float tt = ((rd.u1 - o.u) * r2v - (rd.v1 - o.v) * r2u) / den, q = ((rd.u1 - o.u) * n.v - (rd.v1 - o.v) * n.u) / den;
                            if (tt <= 0 || tt > 40 || q < 0 || q > 1) continue;
                            float sn = std::max(0.3f, fabsf(den) / std::max(0.01f, hypotf(r2u, r2v)));
                            float nearT = tt - (rd.width / 2 + in.linkEndOff) / sn;   // 2 m off the planned edge: ending on the road or at its edge is refused
                            if (nearT < bestT) { bestT = nearT; crossT = tt + (rd.width / 2 + in.linkEndOff) / sn; }
                        }
                        if (bestT > 5.5f && bestT < 40) {
                            extra.push_back({ o.u, o.v, o.u + n.u * bestT, o.v + n.v * bestT });
                            // ...and on across the street to the pavement on the far side: a footpath that only reaches a
                            // road is not joined to it, one that crosses it gets a crossing and joins the road network
                            // (DistrictPlanner.jointest, v0.43.0: road lists grew only for the crossing footpath)
                            bool nearJunction = false;          // a crossing within 14 m of another street's axis runs into the crossroads
                            {   float cu = o.u + n.u * (bestT + crossT) / 2, cv = o.v + n.v * (bestT + crossT) / 2;
                                for (auto& rd : r.roads) {
                                    float du = rd.u2 - rd.u1, dv = rd.v2 - rd.v1, l2 = du * du + dv * dv;
                                    if (l2 < 1) continue;
                                    float along = fabsf(du * n.u + dv * n.v) / sqrtf(l2);   // ~1 = this street runs along the link: the cross street
                                    if (along < 0.7f) continue;
                                    float t = std::clamp(((cu - rd.u1) * du + (cv - rd.v1) * dv) / l2, 0.0f, 1.0f);
                                    if (hypotf(rd.u1 + du * t - cu, rd.v1 + dv * t - cv) < 14 + rd.width / 2) nearJunction = true;
                                }
                            }
                            if (in.crossLinks && !nearJunction) extra.push_back({ o.u + n.u * bestT, o.v + n.v * bestT, o.u + n.u * crossT, o.v + n.v * crossT });
                            r.pedPts.push_back(o); done = true; ++r.straightLinks; break;
                        }
                    }
                    if (done) continue;
                    ++r.straightMiss;
                }
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
                if (best < 0 && mode == 1 && !r.roads.empty()) {
                    // fallback (v0.52, services by the street got no driveway: the search keeps a road 5 m off every
                    // building, so a pub / kiosk / fire station squeezed between neighbours could not even start):
                    // one straight driveway out of the road connection to the street it faces, if it passes 3.5 m
                    // clear of the other buildings and meets the street at least 10 m from a junction.
                    for (auto& c : t.conns) {
                        if (c.kind != 1 || best >= 0) continue;
                        V2 o = Turn(b.rot, c.x, c.z); o.u += b.ou; o.v += b.ov;
                        V2 n = Turn(b.rot, c.dx, c.dz); float nl = hypotf(n.u, n.v);
                        if (nl < 0.01f) continue;
                        n.u /= nl; n.v /= nl;
                        float bestT = 1e9f;
                        for (auto& rd : r.roads) {
                            float r2u = rd.u2 - rd.u1, r2v = rd.v2 - rd.v1, den = n.u * r2v - n.v * r2u;
                            if (fabsf(den) < 1e-6f) continue;
                            float tt = ((rd.u1 - o.u) * r2v - (rd.v1 - o.v) * r2u) / den, q = ((rd.u1 - o.u) * n.v - (rd.v1 - o.v) * n.u) / den;
                            if (tt <= 0.5f || tt > 60 || q < 0 || q > 1) continue;
                            float rl = hypotf(r2u, r2v), alongEnd = std::min(q, 1 - q) * rl;
                            bool atJunction = false;           // the street piece may end at a junction: check other streets near the hit
                            float hu = o.u + n.u * tt, hv = o.v + n.v * tt;
                            for (auto& o2 : r.roads) {
                                if (&o2 == &rd) continue;
                                float ddu = o2.u2 - o2.u1, ddv = o2.v2 - o2.v1, l2 = ddu * ddu + ddv * ddv;
                                if (l2 < 1) continue;
                                if (fabsf((ddu * r2u + ddv * r2v) / (sqrtf(l2) * std::max(0.01f, rl))) > 0.95f) continue;   // the same street, next piece
                                float t2 = std::clamp(((hu - o2.u1) * ddu + (hv - o2.v1) * ddv) / l2, 0.0f, 1.0f);
                                if (hypotf(o2.u1 + ddu * t2 - hu, o2.v1 + ddv * t2 - hv) < 10 + o2.width / 2) atJunction = true;
                            }
                            (void)alongEnd;
                            if (!atJunction && tt < bestT) bestT = tt;
                        }
                        if (bestT > 60 || bestT < 10) continue;   // shorter: the game turns it into a slanted stub into the junction
                        bool clear = true;
                        for (auto& b2 : r.buildings) {
                            if (&b2 == &b) continue;
                            for (int k = 0; k <= 20 && clear; ++k) {
                                float u = o.u + n.u * bestT * k / 20, v = o.v + n.v * bestT * k / 20;
                                if (fabsf(u - b2.u) < b2.w / 2 + 3.5f && fabsf(v - b2.v) < b2.d / 2 + 3.5f) clear = false;
                            }
                        }
                        if (!clear) continue;
                        bestLine = { o, { o.u + n.u * bestT, o.v + n.v * bestT } };
                        best = bestT; ++r.straightDrives;
                        Log("plan: straight driveway for '%s' at u %.0f v %.0f, %.0f m", t.id.c_str(), b.u, b.v, bestT);
                    }
                }
                if (best < 0) { ++(mode ? r.noRoad : r.noPath); Log("plan: no %s for '%s' at u %.0f v %.0f", mode ? "driveway" : "footpath", t.id.c_str(), b.u, b.v); continue; }
                (mode ? r.roadPts : r.pedPts).push_back(bestLine[0]);
                // the game refuses pieces shorter than ~9 m (road) / ~5 m (footpath): a short leg is folded into its
                // neighbour, so the corner becomes one slanted piece
                {   const float minL = mode ? 10.0f : 5.5f;
                    for (bool again = true; again && bestLine.size() > 2;) {
                        again = false;
                        for (size_t k = 1; k + 1 < bestLine.size(); ++k) {
                            float l1 = hypotf(bestLine[k].u - bestLine[k - 1].u, bestLine[k].v - bestLine[k - 1].v);
                            float l2 = hypotf(bestLine[k + 1].u - bestLine[k].u, bestLine[k + 1].v - bestLine[k].v);
                            if (l1 < minL || l2 < minL) { bestLine.erase(bestLine.begin() + k); again = true; break; }
                        }
                    }
                }
                for (size_t k = 0; k + 1 < bestLine.size(); ++k) {
                    PathSeg sg{ bestLine[k].u, bestLine[k].v, bestLine[k + 1].u, bestLine[k + 1].v };
                    if (fabsf(sg.u2 - sg.u1) + fabsf(sg.v2 - sg.v1) < 0.05f) continue;
                    if (mode) r.drives.push_back(sg); else extra.push_back(sg);
                }
            }
        }
        r.paths.insert(r.paths.end(), extra.begin(), extra.end());

        // Connectivity of the plan: every building must reach the street (or, without a street, the main footpath
        // net). Lines touch when an end of one lies on the other. A house whose entrance is in a piece cut off from
        // the rest is dropped together with its link - no lone houses far away.
        {
            std::vector<PathSeg> segs = r.paths;
            const size_t nP = segs.size();
            for (auto& rd : r.roads) segs.push_back({ rd.u1, rd.v1, rd.u2, rd.v2 });
            for (auto& dv : r.drives) segs.push_back(dv);
            // a stop is a pass-through piece of the street: the street and its pavement lines are cut where it stands,
            // people walk through it
            for (auto& b : r.buildings) if (b.stop) {
                float h = roadHalf + 2;
                segs.push_back({ b.ou - b.w / 2 - 1, roadV - h, b.ou + b.w / 2 + 1, roadV - h });
                segs.push_back({ b.ou - b.w / 2 - 1, roadV + h, b.ou + b.w / 2 + 1, roadV + h });
                segs.push_back({ b.ou - b.w / 2 - 1, roadV, b.ou + b.w / 2 + 1, roadV });
                segs.push_back({ b.u - b.w / 2 - 1, b.v - b.d / 2 - 1, b.u + b.w / 2 + 1, b.v - b.d / 2 - 1 });   // its edges, where footpaths end
                segs.push_back({ b.u - b.w / 2 - 1, b.v + b.d / 2 + 1, b.u + b.w / 2 + 1, b.v + b.d / 2 + 1 });
            }
            std::vector<int> par(segs.size());
            for (size_t k = 0; k < par.size(); ++k) par[k] = (int)k;
            std::function<int(int)> root = [&](int x) { return par[x] == x ? x : par[x] = root(par[x]); };
            auto onSeg = [](const PathSeg& sg, float u, float v, float tol) {
                float du = sg.u2 - sg.u1, dv = sg.v2 - sg.v1, l2 = du * du + dv * dv;
                float t = l2 > 1e-6f ? std::clamp(((u - sg.u1) * du + (v - sg.v1) * dv) / l2, 0.0f, 1.0f) : 0.0f;
                return hypotf(sg.u1 + du * t - u, sg.v1 + dv * t - v) < tol;
            };
            for (size_t a = 0; a < segs.size(); ++a)
                for (size_t b = a + 1; b < segs.size(); ++b) {
                    const float tol = (a >= nP || b >= nP) ? 7.0f : 1.0f;    // a street is wide: a pavement line touches it
                    if (onSeg(segs[b], segs[a].u1, segs[a].v1, tol) || onSeg(segs[b], segs[a].u2, segs[a].v2, tol) ||
                        onSeg(segs[a], segs[b].u1, segs[b].v1, tol) || onSeg(segs[a], segs[b].u2, segs[b].v2, tol)) par[root((int)a)] = root((int)b);
                }
            // the main net: the one holding the streets, or the biggest one
            std::vector<int> cnt(segs.size(), 0);
            for (size_t k = 0; k < segs.size(); ++k) ++cnt[root((int)k)];
            int mainRoot = -1;
            if (segs.size() > nP) mainRoot = root((int)nP);
            else for (size_t k = 0; k < segs.size(); ++k) if (mainRoot < 0 || cnt[root((int)k)] > cnt[mainRoot]) mainRoot = root((int)k);
            if (mainRoot >= 0) {
                std::vector<Placed> keep;
                for (auto& b : r.buildings) {
                    const BuildingType& t = all[b.type];
                    bool hasPed = false, ok = false;
                    for (auto& c : t.conns) {
                        if (c.kind != 0) continue;
                        hasPed = true;
                        V2 o = Turn(b.rot, c.x, c.z); o.u += b.ou; o.v += b.ov;
                        for (size_t k = 0; k < segs.size() && !ok; ++k) if (root((int)k) == mainRoot && onSeg(segs[k], o.u, o.v, 2.5f)) ok = true;
                    }
                    if (!hasPed || ok || b.stop || b.net >= 0) { keep.push_back(b); continue; }
                    ++r.cutOff;
                    Log("plan: dropped '%s' at u %.1f v %.1f (entrance not on the street net)", t.id.c_str(), b.u, b.v);
                }
                r.buildings.swap(keep);
                std::vector<PathSeg> np;
                for (size_t k = 0; k < nP; ++k) { if (root((int)k) == mainRoot) np.push_back(r.paths[k]); else ++r.cutOffPaths; }
                r.paths.swap(np);
            }
        }

        // Utility lines by the game's rules: a pipe joins two buildings only, so every node hangs on a switch and the
        // switches form a chain (the last node is fed by the previous switch); a medium-voltage wire runs from a
        // transformer output straight to a substation. The head of each chain is left for the player to feed.
        if (gridOk) {
            // a line from inside the zone (a) toward a point outside (b) stops where it leaves the outline, 3 m inside
            auto toEdge = [&](V2 a, V2 b) {
                if (PolyContains(in.poly, b.u, b.v)) return b;
                float len = hypotf(b.u - a.u, b.v - a.v), last = 0;
                for (float s = 0; s <= len; s += 1.0f) {
                    if (!PolyContains(in.poly, a.u + (b.u - a.u) * s / len, a.v + (b.v - a.v) * s / len)) break;
                    last = s;
                }
                last = std::max(0.0f, last - 3);
                return V2{ a.u + (b.u - a.u) * last / std::max(1.0f, len), a.v + (b.v - a.v) * last / std::max(1.0f, len) };
            };
            auto feed = [&](int net, V2 ext, float extH, V2 at, float atH, float& lenOut) {
                // a source outside the zone: no line at all (a wire or pipe ending in the air is refused by the game,
                // v0.42.0 test) - the node's input is left for the player to feed from wherever he likes
                if (!PolyContains(in.poly, ext.u, ext.v)) { V2 e0 = toEdge(at, ext); lenOut = hypotf(e0.u - at.u, e0.v - at.v); return; }
                V2 e = toEdge(at, ext);
                float len = hypotf(e.u - at.u, e.v - at.v);
                if (len < 6) { lenOut = 0; return; }        // the port is at the edge already
                r.utilLines.push_back({ net, e.u, e.v, at.u, at.v, PolyContains(in.poly, ext.u, ext.v) ? extH : 0.0f, atH });
                lenOut = len;
            };
            auto port = [&](const Placed& b, int kind, int nth) -> const Conn* {
                for (auto& c : all[b.type].conns) if (c.kind == kind && nth-- == 0) return &c;
                return nullptr;
            };
            auto pipe = [&](int net, const Placed* A, const Conn* ca, const Placed* B, const Conn* cb) {
                if (!A || !B || !ca || !cb) { ++r.utilNoLine[net]; return; }
                if (net == 3) {     // a wire goes over everything on poles: always one straight piece, port to port
                    // The game snaps a wire end only to a port that faces the wire (log v0.53: the plan's first input of a
                    // substation, facing away, never snapped, the game took the input on the side the wire came from and
                    // refused the rest). So each end takes the free port of the same kind facing the other building best.
                    static std::vector<std::pair<const Placed*, const Conn*>> used;
                    { bool any = false; for (auto& l : r.utilLines) if (l.net == 3) any = true; if (!any) used.clear(); }   // a new plan
                    auto pick = [&](const Placed* X, const Conn* cx, const Placed* Y) {
                        const Conn* best = cx; float bd = -2;
                        for (auto& c : all[X->type].conns) {
                            if (c.kind != cx->kind) continue;
                            bool taken = false; for (auto& u : used) if (u.first == X && u.second == &c) taken = true;
                            if (taken) continue;
                            V2 o = Turn(X->rot, c.x, c.z), n = Turn(X->rot, c.dx, c.dz);
                            float tu = Y->ou - (X->ou + o.u), tv = Y->ov - (X->ov + o.v), tl = hypotf(tu, tv), nl = hypotf(n.u, n.v);
                            if (tl < 0.01f || nl < 0.01f) continue;
                            float d = (n.u * tu + n.v * tv) / (nl * tl);
                            if (d > bd) { bd = d; best = &c; }
                        }
                        used.push_back({ X, best });
                        return best;
                    };
                    ca = pick(A, ca, B); cb = pick(B, cb, A);
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
                                if (auto* e = nearest(11, hp, d)) { float l; feed(4, { e->u, e->v }, e->h, hp, hc->y, l); r.utilFeed[3] = std::max(r.utilFeed[3], l); }
                            }
                        } else for (auto* n : nodes) if (const Conn* nc = port(*n, 8, 0)) {
                            V2 np = Turn(n->rot, nc->x, nc->z); np.u += n->ou; np.v += n->ov; float d;
                            if (auto* e = nearest(9, np, d)) { float l; feed(3, { e->u, e->v }, e->h, np, nc->y, l); r.utilFeed[3] = std::max(r.utilFeed[3], l); }
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
                        PlanInput::ExtPort own{};
                        if (k == 2) for (auto& b : r.buildings) if (all[b.type].id == "heating_plant_small")      // the plan's own boiler house
                            for (auto& c : all[b.type].conns) if (c.kind == 6 || c.kind == 7) {
                                V2 o = Turn(b.rot, c.x, c.z); own = { c.kind, b.ou + o.u, b.ov + o.v, c.y }; best = &own; bd = hypotf(own.u - hp.u, own.v - hp.v); break;
                            }
                        if (!best) for (auto& e : in.extPorts) if ((e.kind == want || (k == 2 && e.kind == 7)) && hypotf(e.u - hp.u, e.v - hp.v) < bd) { bd = hypotf(e.u - hp.u, e.v - hp.v); best = &e; }
                        if (best) { float l; feed(k, { best->u, best->v }, best->h, hp, hc->y, l); r.utilLen[k] += l; r.utilFeed[k] = l; }
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
    // Junctions at least 6 m apart. The builder cuts every footpath where another one ends on it, and the game refuses
    // pieces shorter than ~5 m: two entrance links meeting the ring 3 m apart, or one 2 m from a corner, left a short
    // piece the game threw away - and the ring broke there. A line end that lands within 6 m of another junction (or
    // of the host line's end) is moved onto it.
    for (int pass = 0; pass < 3; ++pass)
        for (size_t h = 0; h < r.paths.size(); ++h) {
            PathSeg& H = r.paths[h];
            float du = H.u2 - H.u1, dv = H.v2 - H.v1, L = hypotf(du, dv);
            if (L < 1) continue;
            struct J { float t; size_t k; int e; };
            std::vector<J> js;
            for (size_t k = 0; k < r.paths.size(); ++k) {
                if (k == h) continue;
                for (int e = 0; e < 2; ++e) {
                    float pu = e ? r.paths[k].u2 : r.paths[k].u1, pv = e ? r.paths[k].v2 : r.paths[k].v1;
                    float t = ((pu - H.u1) * du + (pv - H.v1) * dv) / (L * L);
                    if (t < -0.01f || t > 1.01f) continue;
                    if (hypotf(H.u1 + du * t - pu, H.v1 + dv * t - pv) > 0.6f) continue;
                    js.push_back({ std::clamp(t, 0.0f, 1.0f) * L, k, e });
                }
            }
            if (js.empty()) continue;
            std::sort(js.begin(), js.end(), [](const J& a, const J& b) { return a.t < b.t; });
            std::vector<float> fixedAt{ 0.0f, L };           // the host's own ends
            for (auto& j : js) {
                float best = -1, bd = 6.0f;
                for (float x : fixedAt) if (fabsf(x - j.t) < bd && fabsf(x - j.t) > 0.05f) { bd = fabsf(x - j.t); best = x; }
                if (best < 0) { bool same = false; for (float x : fixedAt) if (fabsf(x - j.t) <= 0.05f) same = true; if (!same) fixedAt.push_back(j.t); continue; }
                float& eu = j.e ? r.paths[j.k].u2 : r.paths[j.k].u1; float& ev = j.e ? r.paths[j.k].v2 : r.paths[j.k].v1;
                eu = H.u1 + du * best / L; ev = H.v1 + dv * best / L;
            }
        }
    // a footpath running along a street at the pavement distance is only the walking line: people walk on the edges
    // of the street itself, nothing is built there
    r.pathBuild.assign(r.paths.size(), 1);
    for (size_t k = 0; k < r.paths.size(); ++k) {
        const PathSeg& p = r.paths[k];
        for (auto& rd : r.roads) {
            bool horiz = fabsf(rd.v1 - rd.v2) < 0.01f, phoriz = fabsf(p.v1 - p.v2) < 0.01f;
            bool vert = fabsf(rd.u1 - rd.u2) < 0.01f, pvert = fabsf(p.u1 - p.u2) < 0.01f;
            if (!((horiz && phoriz) || (vert && pvert))) continue;
            float off = horiz ? fabsf(p.v1 - rd.v1) : fabsf(p.u1 - rd.u1);
            if (off > rd.width / 2 + 0.5f && off < rd.width / 2 + 3.5f) { r.pathBuild[k] = 0; break; }
        }
    }
    // A footpath that reaches a street must be drawn onto the street itself: the pavement line is not built, so a
    // path stopping there hangs in the air next to the road and the game does not join them. Each end lying on a
    // pavement line is carried across the pavement to the street's axis (the game makes the joint / crossing).
    // (not in the village layout: there the links end at the pavement edge - a piece carried onto the road axis was refused, v0.42.0 test)
    for (size_t k = 0; k < r.paths.size() && in.layout != 2; ++k) {
        if (!r.pathBuild[k]) continue;
        PathSeg& p = r.paths[k];
        for (int e = 0; e < 2; ++e) {
            float& eu = e ? p.u2 : p.u1; float& ev = e ? p.v2 : p.v1;
            for (size_t j = 0; j < r.paths.size(); ++j) {
                if (r.pathBuild[j]) continue;
                const PathSeg& q = r.paths[j];
                bool horiz = fabsf(q.v1 - q.v2) < 0.01f;
                if (horiz ? (fabsf(ev - q.v1) > 0.6f || eu < std::min(q.u1, q.u2) - 1 || eu > std::max(q.u1, q.u2) + 1)
                          : (fabsf(eu - q.u1) > 0.6f || ev < std::min(q.v1, q.v2) - 1 || ev > std::max(q.v1, q.v2) + 1)) continue;
                for (auto& rd : r.roads) {
                    bool rh = fabsf(rd.v1 - rd.v2) < 0.01f;
                    if (rh != horiz) continue;
                    float off = horiz ? fabsf(q.v1 - rd.v1) : fabsf(q.u1 - rd.u1);
                    if (off < rd.width / 2 + 0.5f || off > rd.width / 2 + 3.5f) continue;
                    if (horiz) ev = rd.v1; else eu = rd.u1;
                    break;
                }
                break;
            }
        }
    }
    // Footpath / street / building conflicts (user report v0.49: paths cut diagonally over streets near junctions,
    // ran under buildings and along the carriageway, short stubs hung in the air; the game then refused those pieces
    // and also neighbouring roads and lines). A built footpath is dropped when it
    //  - runs along a street inside its carriageway,
    //  - crosses a street at a slant (> ~10 degrees off square) or within 8 m of a street junction / corner,
    //  - passes through a building (footprint shrunk 1.5 m: entrance links end on the wall),
    //  - is a short (< 20 m) stub whose end touches nothing (no footpath, street or building).
    {
        auto segDist = [](float pu, float pv, float au, float av, float bu, float bv) {
            float du = bu - au, dv = bv - av, l2 = du * du + dv * dv;
            float t = l2 > 0 ? std::clamp(((pu - au) * du + (pv - av) * dv) / l2, 0.0f, 1.0f) : 0.0f;
            return hypotf(au + du * t - pu, av + dv * t - pv);
        };
        auto cross = [](float au, float av, float bu, float bv, float cu, float cv, float du, float dv, float& t) {
            float ru = bu - au, rv = bv - av, su = du - cu, sv = dv - cv, den = ru * sv - rv * su;
            if (fabsf(den) < 1e-6f) return false;
            t = ((cu - au) * sv - (cv - av) * su) / den; float w = ((cu - au) * rv - (cv - av) * ru) / den;
            return t > 0.02f && t < 0.98f && w > -0.02f && w < 1.02f;
        };
        // street junctions and corners: piece ends shared by 3+ ends, or by 2 that turn
        std::vector<V2> junc;
        for (size_t i = 0; i < r.roads.size(); ++i) for (int e = 0; e < 2; ++e) {
            const RoadSeg& a = r.roads[i]; float pu = e ? a.u2 : a.u1, pv = e ? a.v2 : a.v1;
            int n = 0; bool turn = false; float adu = a.u2 - a.u1, adv = a.v2 - a.v1, al = std::max(0.01f, hypotf(adu, adv));
            for (auto& b : r.roads) for (int f = 0; f < 2; ++f) {
                float qu = f ? b.u2 : b.u1, qv = f ? b.v2 : b.v1;
                if (hypotf(qu - pu, qv - pv) > 1.0f) continue;
                ++n; float bl = std::max(0.01f, hypotf(b.u2 - b.u1, b.v2 - b.v1));
                if (fabsf((adu * (b.u2 - b.u1) + adv * (b.v2 - b.v1)) / (al * bl)) < 0.95f) turn = true;
            }
            if (n >= 3 || (n == 2 && turn)) junc.push_back({ pu, pv });
        }
        int onRoad = 0, slant = 0, nearJ = 0, inBld = 0, stub = 0;
        for (size_t k = 0; k < r.paths.size(); ++k) {
            if (!r.pathBuild[k]) continue;
            const PathSeg& p = r.paths[k];
            float pdu = p.u2 - p.u1, pdv = p.v2 - p.v1, pl = hypotf(pdu, pdv);
            if (pl < 0.5f) { r.pathBuild[k] = 0; continue; }
            bool drop = false;
            for (auto& rd : r.roads) {
                if (rd.curve) continue;
                float rdu = rd.u2 - rd.u1, rdv = rd.v2 - rd.v1, rl = std::max(0.01f, hypotf(rdu, rdv));
                float c = fabsf((pdu * rdu + pdv * rdv) / (pl * rl));
                if (c > 0.98f && segDist(p.u1, p.v1, rd.u1, rd.v1, rd.u2, rd.v2) < rd.width / 2 + 0.3f
                              && segDist(p.u2, p.v2, rd.u1, rd.v1, rd.u2, rd.v2) < rd.width / 2 + 0.3f) { drop = true; ++onRoad; break; }
                float t;
                if (cross(p.u1, p.v1, p.u2, p.v2, rd.u1, rd.v1, rd.u2, rd.v2, t)) {
                    if (c > 0.17f) { drop = true; ++slant; break; }
                    float xu = p.u1 + pdu * t, xv = p.v1 + pdv * t;
                    for (auto& j : junc) if (hypotf(j.u - xu, j.v - xv) < 8.0f) { drop = true; break; }
                    if (drop) { ++nearJ; break; }
                }
            }
            for (size_t b = 0; b < r.buildings.size() && !drop; ++b) {
                const Placed& B = r.buildings[b];
                float hu = B.w / 2 - 1.5f, hv = B.d / 2 - 1.5f;
                if (hu <= 0 || hv <= 0) continue;
                for (int s2 = 0; s2 <= 20; ++s2) {
                    float u = p.u1 + pdu * s2 / 20, v = p.v1 + pdv * s2 / 20;
                    if (fabsf(u - B.u) < hu && fabsf(v - B.v) < hv) { drop = true; ++inBld; break; }
                }
            }
            if (drop) r.pathBuild[k] = 0;
        }
        for (int pass = 0; pass < 5; ++pass) {
            bool changed = false;
            for (size_t k = 0; k < r.paths.size(); ++k) {
                if (!r.pathBuild[k]) continue;
                const PathSeg& p = r.paths[k];
                if (hypotf(p.u2 - p.u1, p.v2 - p.v1) >= 20) continue;
                for (int e = 0; e < 2; ++e) {
                    float eu = e ? p.u2 : p.u1, ev = e ? p.v2 : p.v1;
                    bool touch = false;
                    for (size_t j = 0; j < r.paths.size() && !touch; ++j)
                        if (j != k && r.pathBuild[j] && segDist(eu, ev, r.paths[j].u1, r.paths[j].v1, r.paths[j].u2, r.paths[j].v2) < 1.0f) touch = true;
                    for (size_t j = 0; j < r.paths.size() && !touch; ++j)       // a walking line along a street counts as the street
                        if (j != k && !r.pathBuild[j] && segDist(eu, ev, r.paths[j].u1, r.paths[j].v1, r.paths[j].u2, r.paths[j].v2) < 1.0f) touch = true;
                    for (auto& rd : r.roads) if (!touch && segDist(eu, ev, rd.u1, rd.v1, rd.u2, rd.v2) < rd.width / 2 + 4.0f) touch = true;
                    for (auto& B : r.buildings) if (!touch && fabsf(eu - B.u) < B.w / 2 + 2.5f && fabsf(ev - B.v) < B.d / 2 + 2.5f) touch = true;
                    if (!touch) { Log("plan: loose footpath stub %.1f %.1f - %.1f %.1f dropped", p.u1, p.v1, p.u2, p.v2); r.pathBuild[k] = 0; ++stub; changed = true; break; }
                }
            }
            if (!changed) break;
        }
        Log("plan: footpath conflicts dropped: on street %d, slant crossing %d, near junction %d, through building %d, loose stub %d",
            onRoad, slant, nearJ, inBld, stub);
    }
    for (size_t k = 0; k < r.paths.size(); ++k) if (r.pathBuild[k]) r.pathLength += fabsf(r.paths[k].u2 - r.paths[k].u1) + fabsf(r.paths[k].v2 - r.paths[k].v1);
    // streets go round steep ground: a street stops where the slope along it exceeds 15 % over 10 m (the game refuses
    // roads there, levelling helps only part of the way, see docs/internals.md)
    if (in.height && !r.roads.empty()) {
        std::vector<RoadSeg> keep;
        for (auto& s : r.roads) {
            if (s.curve) { keep.push_back(s); continue; }   // a corner arc stays whole
            float du = s.u2 - s.u1, dv = s.v2 - s.v1, len = sqrtf(du * du + dv * dv);
            int n = std::max(1, (int)(len / 5));
            std::vector<char> bad(n, 0);
            for (int k = 0; k < n; ++k) {           // step k covers [k, k+1] / n; judged over 10 m around it
                float t0 = std::max(0.0f, (k - 0.5f) / n), t1 = std::min(1.0f, (k + 1.5f) / n);
                float h0 = in.height(s.u1 + du * t0, s.v1 + dv * t0), h1 = in.height(s.u1 + du * t1, s.v1 + dv * t1);
                if (fabsf(h1 - h0) > 0.15f * std::max(1.0f, (t1 - t0) * len)) bad[k] = 1;
            }
            int k = 0;
            while (k < n) {
                if (bad[k]) { r.steepCut += len / n; ++k; continue; }
                int e = k; while (e < n && !bad[e]) ++e;
                float ta = (float)k / n, tb = (float)e / n;
                if ((tb - ta) * len >= 12) keep.push_back({ s.u1 + du * ta, s.v1 + dv * ta, s.u1 + du * tb, s.v1 + dv * tb, s.width });
                else r.steepCut += (tb - ta) * len;
                k = e;
            }
        }
        r.roads = keep;
    }
    // roads already in the game: do not lay a plan piece over one, and tie loose street ends to the nearest one
    if (!in.extRoads.empty() && !r.roads.empty()) {
        auto segDist = [](float pu, float pv, float au, float av, float bu, float bv, float& qu, float& qv) {
            float du = bu - au, dv = bv - av, l2 = du * du + dv * dv;
            float t = l2 > 0 ? std::clamp(((pu - au) * du + (pv - av) * dv) / l2, 0.0f, 1.0f) : 0.0f;
            qu = au + du * t; qv = av + dv * t;
            return sqrtf((pu - qu) * (pu - qu) + (pv - qv) * (pv - qv));
        };
        auto nearestExt = [&](float u, float v, V2& at) {
            float best = 1e9f;
            for (auto& L : in.extRoads) for (size_t k = 0; k + 1 < L.size(); ++k) {
                float qu, qv, d = segDist(u, v, L[k].u, L[k].v, L[k + 1].u, L[k + 1].v, qu, qv);
                if (d < best) { best = d; at = { qu, qv }; }
            }
            return best;
        };
        V2 tmp;
        size_t before = r.roads.size();
        r.roads.erase(std::remove_if(r.roads.begin(), r.roads.end(), [&](const RoadSeg& s) {
            // a game road running alongside within a street width already serves this piece (and the game would
            // refuse a road laid over it)
            float lim = s.width / 2 + 8;
            return nearestExt((s.u1 + s.u2) / 2, (s.v1 + s.v2) / 2, tmp) < lim && nearestExt(s.u1, s.v1, tmp) < lim + 2 && nearestExt(s.u2, s.v2, tmp) < lim + 2;
        }), r.roads.end());
        r.droppedRoads = (int)(before - r.roads.size());
        std::vector<RoadSeg> joins;
        for (size_t i = 0; i < r.roads.size(); ++i) for (int e = 0; e < 2; ++e) {
            const RoadSeg& s = r.roads[i];
            float u = e ? s.u2 : s.u1, v = e ? s.v2 : s.v1, qu, qv;
            bool loose = true;
            for (size_t j = 0; j < r.roads.size() && loose; ++j)
                if (j != i && segDist(u, v, r.roads[j].u1, r.roads[j].v1, r.roads[j].u2, r.roads[j].v2, qu, qv) < 1.5f) loose = false;
            if (!loose) continue;
            V2 at; float d = nearestExt(u, v, at);
            bool clear = d > 9 && d < 90;    // the link must not run through a building of the plan
            for (auto& b : r.buildings) {
                if (!clear) break;
                for (int k = 0; k <= 30 && clear; ++k) {
                    float t = k / 30.0f, pu = u + (at.u - u) * t, pv = v + (at.v - v) * t;
                    if (fabsf(pu - b.u) < b.w / 2 + s.width / 2 + 2 && fabsf(pv - b.v) < b.d / 2 + s.width / 2 + 2) clear = false;
                }
            }
            if (clear) joins.push_back({ u, v, at.u, at.v, s.width });
        }
        if (joins.empty()) {
            // no loose end got a link (a ring road has no ends): one link from the plan's street point nearest to a
            // game road, if that road passes within 120 m and the link crosses no building of the plan
            float bd = 1e9f; V2 pa{}, pb{}; float bw = 8;
            for (auto& s : r.roads) {
                float len = hypotf(s.u2 - s.u1, s.v2 - s.v1); int n = std::max(1, (int)(len / 5));
                for (int k = 0; k <= n; ++k) {
                    float u = s.u1 + (s.u2 - s.u1) * k / n, v = s.v1 + (s.v2 - s.v1) * k / n; V2 at;
                    float d = nearestExt(u, v, at);
                    if (d > 9 && d < bd) { bd = d; pa = { u, v }; pb = at; bw = s.width; }
                }
            }
            if (bd < 120) {
                bool clear = true;
                for (auto& b : r.buildings) for (int k = 0; k <= 40 && clear; ++k) {
                    float t = k / 40.0f, u = pa.u + (pb.u - pa.u) * t, v = pa.v + (pb.v - pa.v) * t;
                    if (fabsf(u - b.u) < b.w / 2 + bw / 2 + 2 && fabsf(v - b.v) < b.d / 2 + bw / 2 + 2) clear = false;
                }
                if (clear) joins.push_back({ pa.u, pa.v, pb.u, pb.v, bw });
            }
        }
        r.joinedRoads = (int)joins.size();
        r.roads.insert(r.roads.end(), joins.begin(), joins.end());
    }
    {
    // A driveway that meets a street at its dead end makes a right-angle bend there, and the game refuses that (in-game
    // log v0.56: every refused street / driveway piece ended on a dead-end node, a T in the middle of a street was
    // accepted). Such a street is carried on 12 m past the driveway so the join becomes a T.
    for (auto& dr : r.drives) for (int e = 0; e < 2; ++e) {
        float pu = e ? dr.u2 : dr.u1, pv = e ? dr.v2 : dr.v1;
        for (size_t k = 0; k < r.roads.size(); ++k) {
            RoadSeg& rd = r.roads[k];
            if (rd.curve) continue;
            for (int f = 0; f < 2; ++f) {
                float eu = f ? rd.u2 : rd.u1, ev = f ? rd.v2 : rd.v1;
                { float du0 = rd.u2 - rd.u1, dv0 = rd.v2 - rd.v1, l2 = du0 * du0 + dv0 * dv0;   // the driveway must meet this piece
                  float t = l2 > 0 ? std::clamp(((pu - rd.u1) * du0 + (pv - rd.v1) * dv0) / l2, 0.0f, 1.0f) : 0;
                  if (hypotf(rd.u1 + du0 * t - pu, rd.v1 + dv0 * t - pv) > 1.0f) continue; }
                float dEnd = hypotf(eu - pu, ev - pv);
                if (dEnd > 12.5f) continue;   // the builder drops street cuts < 12 m from a node: the join slides onto the dead end
                int ends = 0;                      // other street pieces ending here: then it is a junction, not a dead end
                for (auto& o : r.roads) { if (&o == &rd) continue; if (hypotf(o.u1 - eu, o.v1 - ev) < 1 || hypotf(o.u2 - eu, o.v2 - ev) < 1) ++ends; }
                if (ends) continue;
                float du = f ? rd.u2 - rd.u1 : rd.u1 - rd.u2, dv = f ? rd.v2 - rd.v1 : rd.v1 - rd.v2, L = hypotf(du, dv);
                if (L < 1) continue;
                du /= L; dv /= L;
                float ext = 13.0f - dEnd, nu = eu + du * ext, nv = ev + dv * ext;
                bool hit = false;
                for (auto& b : r.buildings)
                    for (int t = 1; t <= (int)ext && !hit; ++t) {
                        float qu = eu + du * t, qv = ev + dv * t;
                        if (fabsf(qu - b.u) < b.w / 2 + rd.width / 2 && fabsf(qv - b.v) < b.d / 2 + rd.width / 2) hit = true;
                    }
                if (hit) { Log("plan: driveway at a dead end u %.0f v %.0f, no room to carry the street on", eu, ev); continue; }
                if (f) { rd.u2 = nu; rd.v2 = nv; } else { rd.u1 = nu; rd.v1 = nv; }   // the same piece made longer: no node at the old end
                Log("plan: street carried on %.0f m past its dead end u %.0f v %.0f (driveway %.1f m from it)", ext, eu, ev, dEnd);
                goto nextEnd;
            }
        }
        nextEnd:;
    }
    }
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
        static const char* kNorm[3] = { "SHOP", "KINDERGARTEN", "SCHOOL" };
        for (int k = 0; k < 3; ++k) {
            float best = -1;
            for (auto& s : r.buildings) if (all[s.type].category == kNorm[k]) {
                float dist = fabsf(b.u - s.u) + fabsf(b.v - s.v);
                if (best < 0 || dist < best) best = dist;
            }
            if (best < 0) continue;
            r.normWalk[k] = std::max(r.normWalk[k], best);
            if (best > NormRadius(kNorm[k])) ++r.normFar[k];
        }
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
            bool ext = false;
            for (auto& e : in.extNodes[k]) if (hypotf(e.u - b.u, e.v - b.v) <= in.utilRadius[k]) ext = true;
            if (!reach) { if (!ext) ++r.utilUncovered[k]; continue; }
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
// Walking radii of the Soviet micro-district norms (SNiP II-60-75, Shkvarikov 1963): kindergarten 350 m, school
// 500 m, daily shop 300-500 m (400 used). 0 = no norm.
float NormRadius(const std::string& cat) {
    if (cat == "KINDERGARTEN") return 350; if (cat == "SCHOOL") return 500; if (cat == "SHOP") return 400; return 0;
}

PlanResult MakePlan(const PlanInput& inRaw) {
    PlanInput in = inRaw;       // node capacities from the game's formulas for the node types the plan uses
    for (auto& t : catalog::All()) {
        if (t.id == "heating_endstation_small" && t.heatCap > 0) in.utilNodeCap[2] = t.heatCap;
        if (t.id == "eletric_substation" && t.powerCap > 0) in.utilNodeCap[3] = t.powerCap;
    }
    if (in.servicePop <= 0 && in.services) {
        // services for the residents that really fit: typing 34 700 people into a plot for 1 000 gave 14 kindergartens
        // and pushed the houses out (v0.44.0 report)
        PlanInput probe = in;
        for (int k = 0; k < 4; ++k) probe.utilBuild[k] = false;
        probe.servicePop = 1;                           // one of each service while measuring
        probe.boiler = false;
        int cap = std::max(1, MakePlanImpl(probe, nullptr).capacity);
        in.servicePop = in.fillAll ? cap : std::min(in.population, cap);
        // the services for that many take room from the houses: measure again with them in place, so raising the
        // number of residents never makes the plan hold fewer (7 200 typed gave 1 800 places, 6 500 gave 4 000)
        probe.servicePop = in.servicePop;
        int cap2 = std::max(1, MakePlanImpl(probe, nullptr).capacity);
        in.servicePop = std::min(in.servicePop, cap2);
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
        // A node needs a road for its vehicles: it may only follow a building of a row that faces a street, so its
        // driveway is a short stub to that street, not a loop round the rows that cuts footpaths off.
        std::vector<char> streetSide(n, 1);
        {
            bool anyStreet = false;
            for (auto& rd : cur.roads) if (fabsf(rd.v1 - rd.v2) < 0.01f) anyStreet = true;
            if (anyStreet)
                for (size_t b = 0; b < n; ++b) {
                    float gapToStreet = 1e9f;
                    for (auto& rd : cur.roads) {
                        if (fabsf(rd.v1 - rd.v2) < 0.01f && bs[b]->u > std::min(rd.u1, rd.u2) && bs[b]->u < std::max(rd.u1, rd.u2))
                            gapToStreet = std::min(gapToStreet, fabsf(bs[b]->v - rd.v1) - bs[b]->d / 2 - rd.width / 2);
                        if (fabsf(rd.u1 - rd.u2) < 0.01f && bs[b]->v > std::min(rd.v1, rd.v2) && bs[b]->v < std::max(rd.v1, rd.v2))
                            gapToStreet = std::min(gapToStreet, fabsf(bs[b]->u - rd.u1) - bs[b]->w / 2 - rd.width / 2);
                    }
                    streetSide[b] = gapToStreet < 14.0f;
                }
        }
        for (int k = 0; k < 4; ++k) {
            if (!in.utilBuild[k]) continue;
            const float R = std::max(40.0f, in.utilRadius[k] - 15.0f);
            std::vector<char> covered(n, 0);
            size_t left = n;
            // nodes that already stand (the player's own) serve what they reach; no new node there
            for (auto& e : in.extNodes[k])
                for (size_t b = 0; b < n; ++b) if (!covered[b] && hypotf(bs[b]->u - e.u, bs[b]->v - e.v) <= in.utilRadius[k]) { covered[b] = 1; --left; }
            while (left > 0 && sites[k].size() < 60) {
                // the spot that reaches most; among equals the one nearest the middle of what is still uncovered, so
                // a node stands in the centre of the district, not at its first house
                float mu = 0, mv = 0; int mn = 0;
                for (size_t b = 0; b < n; ++b) if (!covered[b]) { mu += bs[b]->u; mv += bs[b]->v; ++mn; }
                if (mn) { mu /= mn; mv /= mn; }
                int pick = -1, pickN = 0; float pickD = 1e9f;
                for (size_t c = 0; c < n; ++c) {
                    if (!streetSide[c]) continue;
                    int cnt = 0;
                    for (size_t b = 0; b < n; ++b) if (!covered[b] && hypotf(bs[b]->u - bs[c]->u, bs[b]->v - bs[c]->v) <= R) ++cnt;
                    float d = hypotf(bs[c]->u - mu, bs[c]->v - mv);
                    if (cnt > pickN || (cnt == pickN && cnt > 0 && d < pickD)) { pickN = cnt; pick = (int)c; pickD = d; }
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
                size_t need = (size_t)ceilf(dem / (0.6f * in.utilNodeCap[k])) + extra[k];   // 60 %: one node read 109 % in the game at 85 % planning
                while (sites[k].size() < need && sites[k].size() < 60) {
                    int farIdx = -1; float farD = -1;
                    for (size_t b = 0; b < n; ++b) {
                        if (!streetSide[b]) continue;
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
