// geom.h — triangle soup, procedural mesh builders, BVH.
#pragma once
#include "core.h"

struct Tri { uint32_t a, b, c; uint16_t mat; };
struct Mesh {
    std::vector<V3> P, N, T; // position, shading normal, 3D texture coordinate
    std::vector<Tri> tris;
};
static Mesh G;
static Xf CUR;               // current object transform (local -> world)
static Xf TEX;               // local -> texture space
static bool TEXWORLD = false; // texture coordinate = world position
static std::function<void(V3&, V3&)> DEFORM; // optional local-space deformation (p, n)

static inline uint32_t addV(V3 p, V3 n) {
    V3 t = TEX.pt(p);
    if (DEFORM) DEFORM(p, n);
    V3 wp = CUR.pt(p);
    G.P.push_back(wp); G.N.push_back(normalize(CUR.vec(n))); G.T.push_back(TEXWORLD ? wp : t);
    return (uint32_t)G.P.size() - 1;
}
static inline uint32_t addVT(V3 p, V3 n, V3 t) {
    if (DEFORM) DEFORM(p, n);
    G.P.push_back(CUR.pt(p)); G.N.push_back(normalize(CUR.vec(n))); G.T.push_back(t);
    return (uint32_t)G.P.size() - 1;
}
static inline void addTri(uint32_t a, uint32_t b, uint32_t c, int mat) {
    V3 e1 = G.P[b] - G.P[a], e2 = G.P[c] - G.P[a];
    if (dot(cross(e1, e2), cross(e1, e2)) < 1e-20f) return; // degenerate
    G.tris.push_back({a, b, c, (uint16_t)mat});
}
static inline void addQuadI(uint32_t a, uint32_t b, uint32_t c, uint32_t d, int mat) { addTri(a, b, c, mat); addTri(a, c, d, mat); }

// flat quad (local coordinates), normal from winding
static void quad(V3 p0, V3 p1, V3 p2, V3 p3, int mat) {
    V3 n = normalize(cross(p1 - p0, p3 - p0));
    uint32_t a = addV(p0, n), b = addV(p1, n), c = addV(p2, n), d = addV(p3, n);
    addQuadI(a, b, c, d, mat);
}
// quad with explicit texture coordinates
static void quadT(V3 p0, V3 p1, V3 p2, V3 p3, V3 t0, V3 t1, V3 t2, V3 t3, int mat) {
    V3 n = normalize(cross(p1 - p0, p3 - p0));
    uint32_t a = addVT(p0, n, t0), b = addVT(p1, n, t1), c = addVT(p2, n, t2), d = addVT(p3, n, t3);
    addQuadI(a, b, c, d, mat);
}

// recompute smooth normals for all vertices >= v0 using triangles >= t0 (welded meshes only)
static void recomputeNormals(size_t v0, size_t t0) {
    for (size_t i = v0; i < G.N.size(); i++) G.N[i] = V3(0, 0, 0);
    for (size_t i = t0; i < G.tris.size(); i++) {
        Tri& t = G.tris[i];
        V3 n = cross(G.P[t.b] - G.P[t.a], G.P[t.c] - G.P[t.a]);
        G.N[t.a] += n; G.N[t.b] += n; G.N[t.c] += n;
    }
    for (size_t i = v0; i < G.N.size(); i++) G.N[i] = normalize(G.N[i]);
}

// box with rounded edges, centred at local origin, half extents h, bevel radius r
static void rbox(V3 h, float r, int mat, int seg = 3) {
    r = std::min(r, std::min(h.x, std::min(h.y, h.z)) * 0.999f);
    if (r < 1e-5f) { r = 1e-5f; seg = 1; }
    auto coords = [&](float hh) {
        std::vector<float> c;
        for (int k = seg; k >= 0; k--) c.push_back(-hh + r - r * tanf(k * (PI / 4) / seg));
        for (int k = 0; k <= seg; k++) c.push_back(hh - r + r * tanf(k * (PI / 4) / seg));
        return c;
    };
    for (int a = 0; a < 3; a++) {
        int u = (a + 1) % 3, v = (a + 2) % 3;
        std::vector<float> cu = coords(h[u]), cv = coords(h[v]);
        for (int s = -1; s <= 1; s += 2) {
            std::vector<uint32_t> id(cu.size() * cv.size());
            for (size_t i = 0; i < cu.size(); i++)
                for (size_t j = 0; j < cv.size(); j++) {
                    V3 p; p[a] = s * h[a]; p[u] = cu[i]; p[v] = cv[j];
                    V3 q(clampf(p.x, -(h.x - r), h.x - r), clampf(p.y, -(h.y - r), h.y - r), clampf(p.z, -(h.z - r), h.z - r));
                    V3 n = normalize(p - q);
                    id[i * cv.size() + j] = addV(q + n * r, n);
                }
            for (size_t i = 0; i + 1 < cu.size(); i++)
                for (size_t j = 0; j + 1 < cv.size(); j++)
                    addQuadI(id[i * cv.size() + j], id[(i + 1) * cv.size() + j], id[(i + 1) * cv.size() + j + 1], id[i * cv.size() + j + 1], mat);
        }
    }
}
// rounded box given by min/max corners
static void rboxMM(V3 mn, V3 mx, float r, int mat, int seg = 3) {
    Xf save = CUR; CUR = CUR * xfT((mn + mx) * 0.5f);
    rbox((mx - mn) * 0.5f, r, mat, seg);
    CUR = save;
}

// swept tube along a polyline with per-point radii. Texture coord = straightened (arc length, cross-section).
static void tube(const std::vector<V3>& pts, const std::vector<float>& rad, int mat, int segs = 14, bool caps = true, V3 texOff = V3(0, 0, 0)) {
    int n = (int)pts.size(); if (n < 2) return;
    std::vector<V3> tan(n);
    for (int i = 0; i < n; i++) {
        V3 a = pts[std::max(i - 1, 0)], b = pts[std::min(i + 1, n - 1)];
        tan[i] = normalize(b - a);
    }
    V3 nrm, bin; onb(tan[0], nrm, bin);
    std::vector<uint32_t> ring((size_t)n * segs);
    float s = 0;
    for (int i = 0; i < n; i++) {
        if (i > 0) {
            s += length(pts[i] - pts[i - 1]);
            nrm = normalize(nrm - tan[i] * dot(nrm, tan[i]));
            bin = cross(tan[i], nrm);
        }
        for (int k = 0; k < segs; k++) {
            float a = 2 * PI * k / segs; float ca = cosf(a), sa = sinf(a);
            V3 d = nrm * ca + bin * sa;
            ring[(size_t)i * segs + k] = addVT(pts[i] + d * rad[i], d, texOff + V3(s, rad[i] * ca, rad[i] * sa));
        }
    }
    for (int i = 0; i + 1 < n; i++)
        for (int k = 0; k < segs; k++) {
            int k2 = (k + 1) % segs;
            addQuadI(ring[(size_t)i * segs + k], ring[(size_t)i * segs + k2], ring[(size_t)(i + 1) * segs + k2], ring[(size_t)(i + 1) * segs + k], mat);
        }
    if (caps) {
        for (int e = 0; e < 2; e++) {
            int i = e ? n - 1 : 0; V3 t = e ? tan[i] : -tan[i];
            V3 nn, bb; onb(tan[i], nn, bb);
            float se = e ? s : 0;
            uint32_t c = addVT(pts[i], t, texOff + V3(se, 0, 0));
            std::vector<uint32_t> r2(segs);
            for (int k = 0; k < segs; k++) {
                float a = 2 * PI * k / segs;
                V3 d = nn * cosf(a) + bb * sinf(a);
                r2[k] = addVT(pts[i] + d * rad[i], t, texOff + V3(se, rad[i] * cosf(a), rad[i] * sinf(a)));
            }
            for (int k = 0; k < segs; k++) addTri(c, r2[k], r2[(k + 1) % segs], mat);
        }
    }
}
static void cyl(V3 a, V3 b, float ra, float rb, int mat, int segs = 14, bool caps = true, V3 texOff = V3(0, 0, 0)) {
    tube({a, b}, {ra, rb}, mat, segs, caps, texOff);
}

// Catmull-Rom refinement of a polyline
template <class TT>
static std::vector<TT> crRefine(const std::vector<TT>& p, int sub) {
    std::vector<TT> out; int n = (int)p.size();
    for (int i = 0; i + 1 < n; i++) {
        TT p0 = p[std::max(i - 1, 0)], p1 = p[i], p2 = p[i + 1], p3 = p[std::min(i + 2, n - 1)];
        for (int k = 0; k < sub; k++) {
            float t = (float)k / sub, t2 = t * t, t3 = t2 * t;
            out.push_back((p1 * 2.0f + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 + (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f);
        }
    }
    out.push_back(p.back());
    return out;
}

// surface of revolution around local Y. profile = list of (r, y) stored as V3(r, y, 0)
static void lathe(const std::vector<V3>& profIn, int mat, int segs = 96, int refine = 6) {
    std::vector<V3> prof = refine > 1 ? crRefine(profIn, refine) : profIn;
    size_t v0 = G.P.size(), t0 = G.tris.size();
    int n = (int)prof.size();
    std::vector<uint32_t> id((size_t)n * segs);
    float s = 0;
    for (int i = 0; i < n; i++) {
        if (i > 0) s += length(prof[i] - prof[i - 1]);
        float r = std::max(prof[i].x, 0.0f);
        for (int k = 0; k < segs; k++) {
            float a = 2 * PI * k / segs;
            V3 p(r * cosf(a), prof[i].y, r * sinf(a));
            id[(size_t)i * segs + k] = addVT(p, V3(0, 1, 0), TEXWORLD ? CUR.pt(p) : TEX.pt(V3(p.x, p.y, p.z)) + V3(0, 0, 0));
        }
    }
    (void)s;
    for (int i = 0; i + 1 < n; i++)
        for (int k = 0; k < segs; k++) {
            int k2 = (k + 1) % segs;
            addQuadI(id[(size_t)i * segs + k], id[(size_t)(i + 1) * segs + k], id[(size_t)(i + 1) * segs + k2], id[(size_t)i * segs + k2], mat);
        }
    recomputeNormals(v0, t0);
}

// ---------------- BVH ----------------
struct Hit { float t, u, v; uint32_t tri; };
struct TriAccel { V3 p0, e1, e2; };

struct BVH {
    struct Node { V3 mn; uint32_t left; V3 mx; uint32_t count; };
    std::vector<Node> nodes;
    std::vector<uint32_t> idx;
    std::vector<TriAccel> acc;
    std::vector<V3> cen, tmn, tmx;

    void build() {
        size_t n = G.tris.size();
        idx.resize(n); cen.resize(n); tmn.resize(n); tmx.resize(n);
        for (size_t i = 0; i < n; i++) {
            idx[i] = (uint32_t)i;
            V3 a = G.P[G.tris[i].a], b = G.P[G.tris[i].b], c = G.P[G.tris[i].c];
            tmn[i] = vmin(a, vmin(b, c)); tmx[i] = vmax(a, vmax(b, c)); cen[i] = (tmn[i] + tmx[i]) * 0.5f;
        }
        nodes.reserve(n * 2); nodes.push_back(Node());
        buildRec(0, 0, (uint32_t)n);
        acc.resize(n);
        for (size_t i = 0; i < n; i++) {
            const Tri& t = G.tris[idx[i]];
            acc[i].p0 = G.P[t.a]; acc[i].e1 = G.P[t.b] - G.P[t.a]; acc[i].e2 = G.P[t.c] - G.P[t.a];
        }
        cen.clear(); tmn.clear(); tmx.clear();
        cen.shrink_to_fit(); tmn.shrink_to_fit(); tmx.shrink_to_fit();
    }
    static float area(V3 mn, V3 mx) { V3 d = mx - mn; return d.x * d.y + d.y * d.z + d.z * d.x; }
    void buildRec(uint32_t ni, uint32_t first, uint32_t count) {
        V3 mn(1e30f), mx(-1e30f), cmn(1e30f), cmx(-1e30f);
        for (uint32_t i = first; i < first + count; i++) {
            uint32_t t = idx[i];
            mn = vmin(mn, tmn[t]); mx = vmax(mx, tmx[t]); cmn = vmin(cmn, cen[t]); cmx = vmax(cmx, cen[t]);
        }
        nodes[ni].mn = mn; nodes[ni].mx = mx;
        if (count <= 4) { nodes[ni].left = first; nodes[ni].count = count; return; }
        const int NB = 16;
        float bestCost = 1e30f; int bestAxis = -1, bestBin = 0;
        for (int ax = 0; ax < 3; ax++) {
            float lo = cmn[ax], hi = cmx[ax];
            if (hi - lo < 1e-9f) continue;
            V3 bmn[NB], bmx[NB]; int bc[NB];
            for (int b = 0; b < NB; b++) { bmn[b] = V3(1e30f); bmx[b] = V3(-1e30f); bc[b] = 0; }
            float sc = NB / (hi - lo);
            for (uint32_t i = first; i < first + count; i++) {
                uint32_t t = idx[i];
                int b = std::min(NB - 1, (int)((cen[t][ax] - lo) * sc));
                bc[b]++; bmn[b] = vmin(bmn[b], tmn[t]); bmx[b] = vmax(bmx[b], tmx[t]);
            }
            float la[NB], ra[NB]; int lc[NB], rc[NB];
            V3 amn(1e30f), amx(-1e30f); int c = 0;
            for (int b = 0; b < NB - 1; b++) { amn = vmin(amn, bmn[b]); amx = vmax(amx, bmx[b]); c += bc[b]; la[b] = area(amn, amx); lc[b] = c; }
            amn = V3(1e30f); amx = V3(-1e30f); c = 0;
            for (int b = NB - 1; b > 0; b--) { amn = vmin(amn, bmn[b]); amx = vmax(amx, bmx[b]); c += bc[b]; ra[b - 1] = area(amn, amx); rc[b - 1] = c; }
            for (int b = 0; b < NB - 1; b++) {
                if (lc[b] == 0 || rc[b] == 0) continue;
                float cost = la[b] * lc[b] + ra[b] * rc[b];
                if (cost < bestCost) { bestCost = cost; bestAxis = ax; bestBin = b; }
            }
        }
        uint32_t mid;
        if (bestAxis < 0) {
            mid = first + count / 2;
        } else {
            float lo = cmn[bestAxis], hi = cmx[bestAxis]; float sc = NB / (hi - lo);
            auto it = std::partition(idx.begin() + first, idx.begin() + first + count, [&](uint32_t t) {
                int b = std::min(NB - 1, (int)((cen[t][bestAxis] - lo) * sc)); return b <= bestBin; });
            mid = (uint32_t)(it - idx.begin());
            if (mid == first || mid == first + count) mid = first + count / 2;
        }
        uint32_t l = (uint32_t)nodes.size();
        nodes.push_back(Node()); nodes.push_back(Node());
        nodes[ni].left = l; nodes[ni].count = 0;
        buildRec(l, first, mid - first);
        buildRec(l + 1, mid, first + count - mid);
    }

    static inline bool slab(const Node& n, V3 o, V3 id, float tmax, float& tn) {
        float t1 = (n.mn.x - o.x) * id.x, t2 = (n.mx.x - o.x) * id.x;
        float tmin = std::min(t1, t2), tmx_ = std::max(t1, t2);
        t1 = (n.mn.y - o.y) * id.y; t2 = (n.mx.y - o.y) * id.y;
        tmin = std::max(tmin, std::min(t1, t2)); tmx_ = std::min(tmx_, std::max(t1, t2));
        t1 = (n.mn.z - o.z) * id.z; t2 = (n.mx.z - o.z) * id.z;
        tmin = std::max(tmin, std::min(t1, t2)); tmx_ = std::min(tmx_, std::max(t1, t2));
        tn = tmin;
        return tmx_ >= std::max(tmin, 0.0f) && tmin < tmax;
    }
    static inline bool triHit(const TriAccel& a, V3 o, V3 d, float tmin, float tmax, float& t, float& u, float& v) {
        V3 pv = cross(d, a.e2); float det = dot(a.e1, pv);
        if (fabsf(det) < 1e-16f) return false;
        float inv = 1.0f / det; V3 tv = o - a.p0;
        u = dot(tv, pv) * inv; if (u < 0 || u > 1) return false;
        V3 qv = cross(tv, a.e1);
        v = dot(d, qv) * inv; if (v < 0 || u + v > 1) return false;
        t = dot(a.e2, qv) * inv;
        return t > tmin && t < tmax;
    }
    bool intersect(V3 o, V3 d, float tmin, float tmax, Hit& h) const {
        V3 id(1.0f / d.x, 1.0f / d.y, 1.0f / d.z);
        uint32_t stack[64]; int sp = 0; uint32_t ni = 0; bool found = false; float tn;
        if (!slab(nodes[0], o, id, tmax, tn)) return false;
        for (;;) {
            const Node& n = nodes[ni];
            if (n.count) {
                for (uint32_t i = n.left; i < n.left + n.count; i++) {
                    float t, u, v;
                    if (triHit(acc[i], o, d, tmin, tmax, t, u, v)) { tmax = t; h.t = t; h.u = u; h.v = v; h.tri = idx[i]; found = true; }
                }
                if (!sp) break; ni = stack[--sp];
            } else {
                float ta, tb; bool ha = slab(nodes[n.left], o, id, tmax, ta), hb = slab(nodes[n.left + 1], o, id, tmax, tb);
                if (ha && hb) {
                    if (ta < tb) { stack[sp++] = n.left + 1; ni = n.left; } else { stack[sp++] = n.left; ni = n.left + 1; }
                } else if (ha) ni = n.left; else if (hb) ni = n.left + 1;
                else { if (!sp) break; ni = stack[--sp]; }
            }
        }
        return found;
    }
    // any-hit traversal. onHit(tri, t, u, v) returns true to stop (fully blocked).
    template <class F>
    bool anyHit(V3 o, V3 d, float tmin, float tmax, F&& onHit) const {
        V3 id(1.0f / d.x, 1.0f / d.y, 1.0f / d.z);
        uint32_t stack[64]; int sp = 0; uint32_t ni = 0; float tn;
        if (!slab(nodes[0], o, id, tmax, tn)) return false;
        for (;;) {
            const Node& n = nodes[ni];
            if (n.count) {
                for (uint32_t i = n.left; i < n.left + n.count; i++) {
                    float t, u, v;
                    if (triHit(acc[i], o, d, tmin, tmax, t, u, v)) { if (onHit(idx[i], t, u, v)) return true; }
                }
                if (!sp) break; ni = stack[--sp];
            } else {
                float ta, tb; bool ha = slab(nodes[n.left], o, id, tmax, ta), hb = slab(nodes[n.left + 1], o, id, tmax, tb);
                if (ha && hb) { stack[sp++] = n.left + 1; ni = n.left; }
                else if (ha) ni = n.left; else if (hb) ni = n.left + 1;
                else { if (!sp) break; ni = stack[--sp]; }
            }
        }
        return false;
    }
};
