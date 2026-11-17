// District layout inside an arbitrary outline: a street with bus stops, strips of buildings on both
// sides of footpaths, services next to the street, steep spots skipped.
#include "common.h"
#include <cmath>
#include <algorithm>
#include <queue>

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
    if (!c && !b.conns.empty()) c = &b.conns[0];
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

PlanResult MakePlan(const PlanInput& in) {
    PlanResult r;
    const auto& all = catalog::All();
    if (in.poly.size() < 3 || in.houseTypes.empty()) return r;

    float minU = 1e9f, maxU = -1e9f, minV = 1e9f, maxV = -1e9f;
    for (auto& p : in.poly) { minU = std::min(minU, p.u); maxU = std::max(maxU, p.u); minV = std::min(minV, p.v); maxV = std::max(maxV, p.v); }
    if (maxU - minU < 20 || maxV - minV < 20) return r;

    const float margin = 5.0f, band = 7.0f, gap = in.gap, corridor = 4.5f;

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

    // existing buildings: sample the footprint plus a 4 m margin on a grid of about 6 m
    auto busy = [&](float cu, float cv, float w, float d) {
        if (!in.occupied) return false;
        float W = w + 8, D = d + 8;
        int nu = std::max(2, (int)(W / 6) + 1), nv = std::max(2, (int)(D / 6) + 1);
        for (int a = 0; a <= nu; ++a) for (int b = 0; b <= nv; ++b)
            if (in.occupied(cu - W / 2 + W * a / nu, cv - D / 2 + D * b / nv)) return true;
        return false;
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
            if (u < spineU + corridor && u + it.w > spineU - corridor) { u = spineU + corridor; continue; }  // leave the spine free
            float cu = u + it.w / 2;
            float cv = north ? vlo + it.d / 2 : vhi - it.d / 2;
            if (busy(cu, cv, it.w, it.d)) { u += 6; ++r.busySkipped; continue; }                        // something is already built here
            if (heightRange(cu, cv, it.w, it.d) > in.maxDiff) { u += 6; ++r.steepSkipped; continue; }   // too steep here
            Placed pb{ it.type, cu, cv, it.w - kSide, it.d - kApron, it.rotated };
            pb.rot = (it.rot + (north ? 2 : 0)) & 3;                // the entrance looks at this row's footpath
            V2 off = Turn(pb.rot, all[it.type].cx, all[it.type].cz);
            pb.ou = cu - off.u; pb.ov = cv - off.v;
            r.buildings.push_back(pb);
            rowL = std::min(rowL, cu - it.w / 2 + 4); rowR = std::max(rowR, cu + it.w / 2 - 4);
            u += it.w + gap; ++i; ++placed;
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
        return d;
    };

    // fill strips alternately above and below the street so that everything stays close to it
    float up = roadV + roadBand, down = roadV - roadBand;
    bool upDone = false, downDone = false;
    float nextUpStreet = in.streetSpacing, nextDownStreet = in.streetSpacing;
    int emptyUp = 0, emptyDown = 0;
    while (i < items.size() && !(upDone && downDone)) {
        for (int side = 0; side < 2 && i < items.size(); ++side) {
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
            if (havePrev) link(prev, st);
            prev = st; havePrev = true;
        }
    }
    if (roadHalf > 0 && !r.roads.empty() && (!stripsUp.empty() || !stripsDown.empty()))
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
                mark(block, 1, b.u - b.w / 2 - 1, b.v - b.d / 2 - 1, b.u + b.w / 2 + 1, b.v + b.d / 2 + 1);
                mark(block, 2, b.u - b.w / 2 - 3, b.v - b.d / 2 - 3, b.u + b.w / 2 + 3, b.v + b.d / 2 + 3);
            }
            for (auto& p : r.paths) { mark(goal, 1, p.u1, p.v1, p.u2, p.v2); mark(slow, 1, p.u1, p.v1, p.u2, p.v2); }
            for (auto& rd : r.roads) {
                mark(goal, 2, rd.u1, rd.v1, rd.u2, rd.v2);
                mark(block, 1, rd.u1, rd.v1 - rd.width / 2 + 0.5f, rd.u2, rd.v1 + rd.width / 2 - 0.5f);    // nobody walks on the roadway
            }
        }
        const int du[4] = { 1, 0, -1, 0 }, dv[4] = { 0, 1, 0, -1 };
        // mode 0 = footpath, 1 = driveway; returns the price of the line, < 0 = no way
        auto route = [&](V2 p, V2 n, int mode, const Placed& own, std::vector<V2>& out) {
            const unsigned char bit = mode ? 2 : 1;
            const float infl = (mode ? 3.0f : 1.0f) + cell;
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
            while (!pq.empty() && pops++ < 40000) {
                QE e = pq.top(); pq.pop();
                if (e.first > dist[e.second]) continue;
                int c = e.second / 4, d = e.second % 4, i = c % W, j = c / W;
                if (goal[c] & bit) { found = e.second; break; }
                if (e.first > 260) break;
                for (int nd = 0; nd < 4; ++nd) {
                    if (nd == (d + 2) % 4) continue;
                    int ni = i + du[nd], nj = j + dv[nd];
                    if (ni < 0 || nj < 0 || ni >= W || nj >= H) continue;
                    int nc = nj * W + ni;
                    if (block[nc] & bit) continue;
                    float cost = e.first + 1 + (nd != d ? 6 : 0) + (mode && (slow[nc] & 1) ? 12 : 0);
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
            for (int s : touched) { dist[s] = 1e9f; parent[s] = -1; }
            touched.clear();
            return price;
        };
        std::vector<PathSeg> extra;
        for (auto& b : r.buildings) {
            const BuildingType& t = all[b.type];
            for (int mode = 0; mode < 2; ++mode) {
                if (mode == 1 && !t.needsRoad) continue;
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

    // Utilities follow the footpath network (so they never run under a building); substations stand beside the
    // streets, chosen greedily so that every building is within the service radius (measured along the grid).
    r.utilLength = std::max(0.0f, r.pathLength - r.roadLength);     // both pavements of a street share one trench
    for (int k = 0; k < 4; ++k) {
        std::vector<V2> cand;
        for (auto& rd : r.roads) {
            float off = rd.width / 2 + 4.5f;
            for (float u = rd.u1 + 12 + k * 6.0f; u < rd.u2 - 6; u += 15) { cand.push_back({ u, rd.v1 + off }); cand.push_back({ u, rd.v1 - off }); }
        }
        std::vector<char> covered(r.buildings.size(), 0);
        size_t left = r.buildings.size();
        float R = in.utilRadius[k];
        while (left > 0 && !cand.empty() && r.utilNodes[k].size() < 200) {
            int best = -1, bestN = 0;
            for (size_t ci = 0; ci < cand.size(); ++ci) {
                int n = 0;
                for (size_t bi = 0; bi < r.buildings.size(); ++bi)
                    if (!covered[bi] && fabsf(r.buildings[bi].u - cand[ci].u) + fabsf(r.buildings[bi].v - cand[ci].v) <= R) ++n;
                if (n > bestN) { bestN = n; best = (int)ci; }
            }
            if (best < 0) break;
            r.utilNodes[k].push_back(cand[best]);
            for (size_t bi = 0; bi < r.buildings.size(); ++bi)
                if (!covered[bi] && fabsf(r.buildings[bi].u - cand[best].u) + fabsf(r.buildings[bi].v - cand[best].v) <= R) { covered[bi] = 1; --left; }
        }
        r.utilUncovered[k] = (int)left;
    }
    return r;
}
