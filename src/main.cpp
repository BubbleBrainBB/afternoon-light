// main.cpp — unidirectional path tracer with sun NEE, portal-guided sky sampling,
// single-scattering light shafts and dust motes. Writes linear float AOVs.
//
// usage: render W H SPP outPrefix [x0 y0 x1 y1]   (optional crop, in fractions of the frame)
#include "scene.h"
#include <chrono>

static BVH bvh;

// ---------------- camera ----------------
struct Camera {
    V3 pos{2.50f, 1.25f, 6.60f};
    float tx0 = -0.600f, tx1 = 0.145f, ty0 = -0.305f, ty1 = 0.19167f; // shifted frame (film plane parallel to back wall), 3:2
    float focus = 6.05f, lensR = 0.011f;
};
static Camera CAM;

// ---------------- BSDF ----------------
static inline float ggxD(float nh, float a) { float a2 = a * a; float d = nh * nh * (a2 - 1) + 1; return a2 / (PI * d * d); }
static inline float smithG1(float nv, float a) { float a2 = a * a; return 2 * nv / (nv + sqrtf(a2 + (1 - a2) * nv * nv)); }
static inline float schlick(float f0, float c) { float m = 1 - c; float m2 = m * m; return f0 + (1 - f0) * m2 * m2 * m; }
static inline V3 schlick3(V3 f0, float c) { float m = 1 - c; float m2 = m * m; float w = m2 * m2 * m; return f0 + (V3(1) - f0) * w; }
static inline float specProb(const Surf& s, float no) {
    if (s.kind == 1) return 1.0f;
    float Fo = schlick(s.spec.x, no);
    float dw = (1 - Fo) * lum(s.alb);
    return clampf(Fo / (Fo + dw + 1e-6f), 0.08f, 0.92f);
}
// returns f * |cos_i|; pdf w.r.t. solid angle of the BSDF sampling routine
static V3 bsdfEval(const Surf& s, V3 wo, V3 wi, float& pdf, float* specFrac = nullptr) {
    pdf = 0; if (specFrac) *specFrac = 0;
    if (s.kind == 2) {
        float ci = dot(s.n, wi);
        float w = ci > 0 ? (1 - s.trans) : s.trans;
        float c = fabsf(ci);
        pdf = w * c * INV_PI;
        return s.alb * (w * c * INV_PI);
    }
    if (dot(s.ng, wi) <= 0) return V3(0);
    float no = dot(s.n, wo), ni = dot(s.n, wi);
    if (ni <= 1e-5f || no <= 1e-5f) return V3(0);
    V3 h = normalize(wo + wi); float nh = dot(s.n, h), oh = dot(wo, h);
    float a = s.rough * s.rough;
    float D = ggxD(nh, a), G1o = smithG1(no, a), G1i = smithG1(ni, a);
    V3 F = schlick3(s.spec, oh);
    V3 spec = F * (D * G1o * G1i / (4 * no));
    float ps = specProb(s, no);
    float pdfSpec = G1o * D / (4 * no);
    V3 diff(0);
    if (s.kind == 0) { float Fo = schlick(s.spec.x, no), Fi = schlick(s.spec.x, ni); diff = s.alb * ((1 - Fo) * (1 - Fi) * ni * INV_PI); }
    pdf = ps * pdfSpec + (1 - ps) * ni * INV_PI;
    if (specFrac) *specFrac = ps * pdfSpec / std::max(pdf, 1e-12f);
    return spec + diff;
}
static V3 bsdfSample(const Surf& s, V3 wo, float u0, float u1, float u2) {
    if (s.kind == 2) return u0 < s.trans ? cosHemi(-s.n, u1, u2) : cosHemi(s.n, u1, u2);
    float no = std::max(dot(s.n, wo), 1e-4f);
    float ps = specProb(s, no);
    if (u0 < ps) {
        V3 t, b; onb(s.n, t, b);
        float a = s.rough * s.rough;
        V3 Ve(dot(wo, t), dot(wo, b), no);
        V3 Vh = normalize(V3(a * Ve.x, a * Ve.y, Ve.z));
        float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
        V3 T1 = lensq > 1e-12f ? V3(-Vh.y, Vh.x, 0) / sqrtf(lensq) : V3(1, 0, 0);
        V3 T2 = cross(Vh, T1);
        float r = sqrtf(u1), phi = 2 * PI * u2;
        float t1 = r * cosf(phi), t2 = r * sinf(phi);
        float sm = 0.5f * (1 + Vh.z);
        t2 = (1 - sm) * sqrtf(std::max(0.f, 1 - t1 * t1)) + sm * t2;
        V3 Nh = T1 * t1 + T2 * t2 + Vh * sqrtf(std::max(0.f, 1 - t1 * t1 - t2 * t2));
        V3 Ne = normalize(V3(a * Nh.x, a * Nh.y, std::max(0.f, Nh.z)));
        V3 H = t * Ne.x + b * Ne.y + s.n * Ne.z;
        return normalize(H * (2 * dot(wo, H)) - wo);
    }
    return cosHemi(s.n, u1, u2);
}

// ---------------- visibility ----------------
static inline float sheerOpenHit(uint32_t tri, float u, float v, V3 d) {
    const Tri& t = G.tris[tri]; float w = 1 - u - v;
    V3 T = G.T[t.a] * w + G.T[t.b] * u + G.T[t.c] * v;
    V3 n = normalize(G.N[t.a] * w + G.N[t.b] * u + G.N[t.c] * v);
    return sheerOpen(T, dot(n, d));
}
static float transmit(V3 o, V3 d, float tmax) {
    float Tr = 1;
    bool blocked = bvh.anyHit(o, d, 1e-5f, tmax, [&](uint32_t tri, float, float u, float v) {
        if (MATS[G.tris[tri].mat].type == MT_SHEER) { Tr *= sheerOpenHit(tri, u, v, d); return Tr < 1e-3f; }
        return true;
    });
    return blocked ? 0.0f : Tr;
}
static bool traceReal(V3 o, V3 d, RNG& rng, Surf& s, float& dist, float& pass, int lod = 0) {
    dist = 0; pass = 1; Hit h;
    for (int k = 0; k < 12; k++) {
        if (!bvh.intersect(o, d, 1e-5f, 1e30f, h)) return false;
        if (MATS[G.tris[h.tri].mat].type == MT_SHEER) {
            float open = sheerOpenHit(h.tri, h.u, h.v, d);
            if (rng.f() < open) { o = o + d * (h.t + 2e-4f); dist += h.t + 2e-4f; pass *= open; continue; }
        }
        evalSurface(h, d, s, lod); dist += h.t; return true;
    }
    return false;
}

// ---------------- portals: door (sun side) and the fill window ----------------
static const float PORTAL_A = (DZ1 - DZ0) * DY1;
struct Portal { float x, y0, y1, z0, z1, A, prob; };
static const Portal PORT[2] = {
    {0.0f, 0.0f, DY1, DZ0, DZ1, (DZ1 - DZ0) * DY1, 0.22f},
    {RX, W2Y0, W2Y1, W2Z0, W2Z1, (W2Z1 - W2Z0) * (W2Y1 - W2Y0), 0.13f},
};
static inline float portalPdf(int i, V3 p, V3 wi) {
    const Portal& P = PORT[i];
    if (fabsf(wi.x) < 1e-6f) return 0;
    float t = (P.x - p.x) / wi.x; if (t <= 1e-6f) return 0;
    V3 q = p + wi * t;
    if (q.z < P.z0 || q.z > P.z1 || q.y < P.y0 || q.y > P.y1) return 0;
    return t * t / (P.A * fabsf(wi.x));
}
static inline bool inRoom(V3 p) { const float e = 5e-4f; return p.x > -e && p.x < RX + e && p.z > -e && p.z < RZ + e && p.y > -e && p.y < RH + e; }

static inline V3 sampleSun(float u1, float u2) {
    float cz = 1 - u1 * (1 - SUN_COS_MAX); float sz = sqrtf(std::max(0.f, 1 - cz * cz)); float ph = 2 * PI * u2;
    V3 t, b; onb(TO_SUN, t, b);
    return normalize(t * (sz * cosf(ph)) + b * (sz * sinf(ph)) + TO_SUN * cz);
}

struct PixSample { V3 L, sun, alb, nrm, vol; float depth; };
static int MAXD = 14; static int DBG = 0;
static const float CLAMP_IND = 14.0f;
static inline V3 clampLum(V3 c, float m) { float l = lum(c); return l > m ? c * (m / l) : c; }

// A "virtual point light": a point y inside the room that the sun reaches through the door.
// Sampled from the light side (uniform point on the door portal, direction inside the sun disc),
// it is connected to every eye-path vertex and MIS-combined with ordinary path tracing.
struct VPL { bool ok; Surf s; float reach, geomB; V3 w; };
static void makeVPL(RNG& rng, float u1, float u2, VPL& v) {
    v.ok = false;
    V3 q(0.0006f, u1 * DY1, DZ0 + u2 * (DZ1 - DZ0));
    V3 w = sampleSun(rng.f(), rng.f());
    if (transmit(q, w, 1e30f) <= 0) return;
    float dist;
    if (!traceReal(q, -w, rng, v.s, dist, v.reach, 1)) return;
    if (!inRoom(v.s.p) || v.s.sheer) return;
    float cy = fabsf(dot(v.s.ng, w));
    if (cy < 2e-3f) return;
    v.geomB = cy / (PORTAL_A * fabsf(w.x)); v.w = w; v.ok = true;
}

// strat: [0,1] sun disk, [2,3] bounce dir, [4] selector, [5] volume position, [6,7] portal point for the VPL
static void radiance(V3 o, V3 d, RNG& rng, const float* strat, PixSample& out) {
    V3 T(1), L(0); float lastPdf = -1;
    out.sun = V3(0); out.alb = V3(0); out.nrm = V3(0); out.vol = V3(0); out.depth = 0;
    VPL vpl; makeVPL(rng, strat[6], strat[7], vpl);
    bool prevB = false;          // did the previous vertex attempt a VPL connection?
    float minRough = 0;          // path regularisation: glossy lobes are widened after a diffuse bounce
    vpl.s.rough = std::max(vpl.s.rough, 0.55f);
    float vA = 0, vG = 0;        // area pdf with which the current vertex was generated; its light-side pdf
    for (int depth = 0; depth < MAXD; depth++) {
        Surf s; float dist, pass;
        bool hit = traceReal(o, d, rng, s, dist, pass, depth > 0 && minRough > 0.3f ? 1 : 0);
        if (depth == 0) {
            // single scattering of sunlight in the room air along the primary ray (3 stratified points)
            float tl = std::min(hit ? dist : 30.0f, 16.0f);
            const int NV = 3;
            for (int k = 0; k < NV; k++) {
                float u = (k + strat[5] + (k ? rng.f() - 0.5f : 0.0f)) / NV; u = u - floorf(u);
                V3 pv = o + d * (u * tl);
                if (!inRoom(pv)) continue;
                V3 ws = sampleSun(rng.f(), rng.f());
                float tr = transmit(pv, ws, 1e30f);
                if (tr > 0) {
                    float g = 0.45f, c = dot(SUN_TRAVEL, -d);
                    float hg = (1 - g * g) / (4 * PI * powf(1 + g * g - 2 * g * c, 1.5f));
                    float ph = 0.75f * hg + 0.25f / (4 * PI);
                    out.vol += ESUN * (tr * ph * tl / NV);
                }
            }
            out.depth = hit ? dist : 60.0f;
        }
        if (!hit) {
            V3 add = T * skyColor(d);
            if (dot(d, TO_SUN) > SUN_COS_MAX && depth > 0) {
                float w = lastPdf < 0 ? 1.0f : lastPdf / (lastPdf + 1.0f / SUN_OMEGA);
                if (vG > 0) w *= vA / (vA + pass * vG);
                add += T * ESUN * (w / SUN_OMEGA);
            }
            if (depth > 0) add = clampLum(add, CLAMP_IND);
            else { out.alb = V3(1); out.nrm = V3(0, 0, 1); }
            if (!(DBG & 4) || depth == 0) L += add;
            break;
        }
        if (depth == 0) { out.alb = s.kind == 1 ? s.spec : s.alb; out.nrm = s.n; }
        s.rough = std::max(s.rough, minRough);
        V3 wo = -d;
        V3 po = s.p + s.ng * 1.5e-4f, pt = s.p - s.ng * 1.5e-4f;
        bool in = inRoom(s.p);
        float pq[2] = {0, 0};
        if (in && !(DBG & 2))
            for (int k = 0; k < 2; k++) {
                const Portal& P = PORT[k]; bool ok = s.kind == 2;
                for (int i = 0; i < 4 && !ok; i++) ok = dot(s.n, V3(P.x, (i >> 1) ? P.y1 : P.y0, (i & 1) ? P.z1 : P.z0) - s.p) > 1e-3f;
                if (ok && fabsf(P.x - s.p.x) > 1e-3f) pq[k] = P.prob;
            }
        float pp = pq[0] + pq[1];
        auto mixPdf = [&](float pdfB, V3 wi) {
            float p = (1 - pp) * pdfB;
            if (pq[0] > 0) p += pq[0] * portalPdf(0, s.p, wi);
            if (pq[1] > 0) p += pq[1] * portalPdf(1, s.p, wi);
            return p;
        };
        // light-side pdf of this vertex (only if the previous vertex also tried the VPL strategy)
        vA = 0; vG = 0;
        if (depth > 0 && prevB && in && !s.sheer) {
            vA = lastPdf * pass * fabsf(dot(s.ng, d)) / (dist * dist);
            vG = fabsf(dot(s.ng, TO_SUN)) / (PORTAL_A * fabsf(TO_SUN.x));
        }
        // ---- next-event estimation: the sun ----
        {
            float u1 = depth == 0 ? strat[0] : rng.f(), u2 = depth == 0 ? strat[1] : rng.f();
            V3 wi = sampleSun(u1, u2);
            float pdfB; V3 f = bsdfEval(s, wo, wi, pdfB);
            if (maxc(f) > 0) {
                float tr = transmit(dot(wi, s.ng) > 0 ? po : pt, wi, 1e30f);
                if (tr > 0) {
                    float pm = mixPdf(pdfB, wi);
                    float ps = 1.0f / SUN_OMEGA;
                    float w = ps / (ps + pm);
                    if (vG > 0) w *= vA / (vA + tr * vG);
                    V3 c = T * f * ESUN * (tr * w);
                    if (depth == 0) out.sun = c; else c = clampLum(c, CLAMP_IND);
                    if (!(DBG & 8) || depth == 0) L += c;
                }
            }
        }
        // ---- connection to the virtual point light (sun -> y -> this vertex) ----
        prevB = false;
        if (in && depth < MAXD - 1 && !(DBG & 1)) {
            prevB = true;
            if (vpl.ok) {
                V3 dv = vpl.s.p - s.p; float r2 = dot(dv, dv);
                if (r2 > 1e-8f) {
                    float r = sqrtf(r2); V3 wi = dv / r;
                    float pdfBx; V3 fx = bsdfEval(s, wo, wi, pdfBx);
                    if (maxc(fx) > 0) {
                        Surf ys = vpl.s;
                        if (dot(ys.ng, wi) > 0) { ys.ng = -ys.ng; ys.n = -ys.n; } // orient toward x
                        float tmp; V3 fy = bsdfEval(ys, -wi, vpl.w, tmp);
                        if (maxc(fy) > 0) {
                            V3 org = dot(wi, s.ng) > 0 ? po : pt;
                            float trxy = transmit(org, wi, r - 4e-4f);
                            if (trxy > 0) {
                                float cyx = fabsf(dot(ys.ng, wi));
                                float pA = mixPdf(pdfBx, wi) * trxy * cyx / r2;
                                float pB = vpl.reach * vpl.geomB;
                                V3 c = T * fx * fy * ESUN * (trxy * cyx / r2 * vpl.reach / (pA + pB));
                                if (!(DBG & 16)) L += clampLum(c, CLAMP_IND);
                            }
                        }
                    }
                }
            }
        }
        // ---- continuation: mixture of BSDF sampling and portal sampling ----
        float u0 = depth == 0 ? strat[4] : rng.f();
        float ub1 = depth == 0 ? strat[2] : rng.f(), ub2 = depth == 0 ? strat[3] : rng.f();
        V3 wi;
        if (u0 < pp) {
            const Portal& P = PORT[u0 < pq[0] ? 0 : 1];
            V3 q(P.x, lerp(P.y0, P.y1, ub1), lerp(P.z0, P.z1, ub2));
            wi = normalize(q - s.p);
        } else {
            float us = pp > 0 ? (u0 - pp) / (1 - pp) : u0;
            wi = bsdfSample(s, wo, us, ub1, ub2);
        }
        float pdfB, sf; V3 f = bsdfEval(s, wo, wi, pdfB, &sf);
        float pdf = mixPdf(pdfB, wi);
        if (!(pdf > 1e-9f) || maxc(f) <= 0) break;
        T *= f / pdf; lastPdf = pdf;
        sf *= (1 - pp) * pdfB / pdf;
        minRough = std::max(minRough, lerp(0.60f, std::min(0.6f, s.rough * 1.6f), sf));
        if (depth >= 2) {
            float q = clampf(maxc(T), 0.05f, 0.95f);
            if (rng.f() > q) break;
            T *= 1.0f / q;
        }
        o = dot(wi, s.ng) > 0 ? po : pt; d = wi;
    }
    out.L = L;
}

static void writeF32(const std::string& path, const std::vector<float>& v) {
    FILE* f = fopen(path.c_str(), "wb"); if (!f) { fprintf(stderr, "cannot write %s\n", path.c_str()); exit(1); }
    fwrite(v.data(), sizeof(float), v.size(), f); fclose(f);
}

int main(int argc, char** argv) {
    if (argc < 5) { fprintf(stderr, "usage: render W H SPP outPrefix [x0 y0 x1 y1]\n"); return 1; }
    int W = atoi(argv[1]), H = atoi(argv[2]), SPP = atoi(argv[3]); std::string prefix = argv[4];
    float cx0 = 0, cy0 = 0, cx1 = 1, cy1 = 1;
    if (argc >= 9) { cx0 = atof(argv[5]); cy0 = atof(argv[6]); cx1 = atof(argv[7]); cy1 = atof(argv[8]); }
    if (const char* e = getenv("MAXD")) MAXD = atoi(e);
    if (const char* e = getenv("DBG")) DBG = atoi(e);
    if (const char* e = getenv("SUN_AZ")) CFG.sunAz = atof(e);
    if (const char* e = getenv("SUN_EL")) CFG.sunEl = atof(e);
    if (const char* e = getenv("TREE_SEED")) CFG.treeSeed = atoi(e);
    if (const char* e = getenv("TREE_Y")) CFG_treeAimY = atof(e);
    if (const char* e = getenv("TREE_Z")) CFG_treeAimZ = atof(e);
    if (const char* e = getenv("TREE_R")) CFG_treeR = atof(e);
    if (const char* e = getenv("LIFE_SEED")) CFG.lifeSeed = atoi(e);
    int NT = std::thread::hardware_concurrency(); if (const char* e = getenv("NT")) NT = atoi(e);
    int sm = (int)ceilf(sqrtf((float)SPP)); int sn = (SPP + sm - 1) / sm; SPP = sm * sn;

    auto t0 = std::chrono::steady_clock::now();
    buildScene();
    bvh.build();
    auto t1 = std::chrono::steady_clock::now();
    fprintf(stderr, "scene: %zu tris, %zu verts, bvh %zu nodes, %.1fs\n", G.tris.size(), G.P.size(), bvh.nodes.size(), std::chrono::duration<double>(t1 - t0).count());

    int px0 = (int)floorf(cx0 * W), px1 = (int)ceilf(cx1 * W), py0 = (int)floorf(cy0 * H), py1 = (int)ceilf(cy1 * H);
    int cw = px1 - px0, chh = py1 - py0;
    std::vector<float> bBeauty((size_t)cw * chh * 3), bSun((size_t)cw * chh * 3), bAlb((size_t)cw * chh * 3), bNrm((size_t)cw * chh * 3), bVol((size_t)cw * chh * 3), bDepth((size_t)cw * chh);

    const int TS = 32; int tilesX = (cw + TS - 1) / TS, tilesY = (chh + TS - 1) / TS; int nTiles = tilesX * tilesY;
    std::atomic<int> next(0), done(0);
    auto worker = [&](int tid) {
        for (;;) {
            int ti = next.fetch_add(1); if (ti >= nTiles) break;
            int tx = ti % tilesX, ty = ti / tilesX;
            for (int y = ty * TS; y < std::min(chh, (ty + 1) * TS); y++)
                for (int x = tx * TS; x < std::min(cw, (tx + 1) * TS); x++) {
                    int gx = px0 + x, gy = py0 + y;
                    uint32_t pix = (uint32_t)(gy * W + gx);
                    RNG rng(hash32(pix * 2654435761u + 12345u), pix * 2 + 1);
                    V3 aL(0), aS(0), aA(0), aN(0), aV(0); float aD = 0;
                    for (int s = 0; s < SPP; s++) {
                        float f0, f1, l0, l1, st[8];
                        cmj(s, sm, sn, hash32(pix * 7u + 0), f0, f1);
                        cmj(s, sm, sn, hash32(pix * 7u + 1), l0, l1);
                        cmj(s, sm, sn, hash32(pix * 7u + 2), st[0], st[1]);
                        cmj(s, sm, sn, hash32(pix * 7u + 3), st[2], st[3]);
                        cmj(s, sm, sn, hash32(pix * 7u + 4), st[4], st[5]);
                        cmj(s, sm, sn, hash32(pix * 7u + 5), st[6], st[7]);
                        // tent filter, radius 1 px
                        float ox = f0 < 0.5f ? sqrtf(2 * f0) - 1 : 1 - sqrtf(2 - 2 * f0);
                        float oy = f1 < 0.5f ? sqrtf(2 * f1) - 1 : 1 - sqrtf(2 - 2 * f1);
                        float fx = (gx + 0.5f + ox) / W, fy = (gy + 0.5f + oy) / H;
                        float tx_ = lerp(CAM.tx0, CAM.tx1, fx), ty_ = lerp(CAM.ty1, CAM.ty0, fy);
                        V3 pf = CAM.pos + V3(tx_ * CAM.focus, ty_ * CAM.focus, -CAM.focus);
                        float lx, ly; concentricDisk(l0, l1, lx, ly);
                        V3 o = CAM.pos + V3(lx * CAM.lensR, ly * CAM.lensR, 0);
                        V3 d = normalize(pf - o);
                        PixSample ps; radiance(o, d, rng, st, ps);
                        if (!(ps.L.x == ps.L.x && ps.L.y == ps.L.y && ps.L.z == ps.L.z)) continue; // NaN guard
                        aL += ps.L; aS += ps.sun; aA += ps.alb; aN += ps.nrm; aV += ps.vol; aD += ps.depth;
                    }
                    float inv = 1.0f / SPP; size_t i3 = ((size_t)y * cw + x) * 3;
                    for (int c = 0; c < 3; c++) { bBeauty[i3 + c] = aL[c] * inv; bSun[i3 + c] = aS[c] * inv; bAlb[i3 + c] = aA[c] * inv; bNrm[i3 + c] = aN[c] * inv; bVol[i3 + c] = aV[c] * inv; }
                    bDepth[(size_t)y * cw + x] = aD * inv;
                }
            int dn = done.fetch_add(1) + 1;
            if (tid == 0 && (dn % 64 == 0)) {
                double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();
                fprintf(stderr, "\r%5.1f%%  elapsed %.0fs  eta %.0fs   ", 100.0 * dn / nTiles, el, el * (nTiles - dn) / dn);
            }
        }
    };
    std::vector<std::thread> th;
    for (int i = 0; i < NT; i++) th.emplace_back(worker, i);
    for (auto& t : th) t.join();
    double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();
    fprintf(stderr, "\nrender %dx%d @ %d spp: %.1fs\n", cw, chh, SPP, el);

    writeF32(prefix + "_beauty.f32", bBeauty); writeF32(prefix + "_sun.f32", bSun); writeF32(prefix + "_alb.f32", bAlb);
    writeF32(prefix + "_nrm.f32", bNrm); writeF32(prefix + "_vol.f32", bVol); writeF32(prefix + "_depth.f32", bDepth);
    FILE* f = fopen((prefix + ".txt").c_str(), "w"); fprintf(f, "%d %d %d %d %d %d %d\n", cw, chh, SPP, px0, py0, W, H); fclose(f);
    // dust motes: tiny specks floating in the sunbeam. Position, sun visibility and camera visibility are
    // resolved here; the darkroom script splats them (defocused discs).
    {
        RNG rng(4242, 3);
        std::vector<float> motes;
        const int NM = 8000;
        for (int i = 0; i < NM; i++) {
            V3 p(rng.range(0.02f, 3.6f), rng.range(0.02f, 2.6f), rng.range(0.02f, 2.6f));
            float size = 0.06f + 0.94f * powf(rng.f(), 5.0f); // few large, many tiny
            V3 ws = sampleSun(rng.f(), rng.f());
            float tr = transmit(p, ws, 1e30f);
            if (tr <= 0.02f) continue;
            V3 dc = CAM.pos - p; float dl = length(dc);
            float tv = transmit(p, dc / dl, dl);
            if (tv <= 0.02f) continue;
            float zd = CAM.pos.z - p.z;
            float tx = (p.x - CAM.pos.x) / zd, ty = (p.y - CAM.pos.y) / zd;
            float fx = (tx - CAM.tx0) / (CAM.tx1 - CAM.tx0) * W, fy = (CAM.ty1 - ty) / (CAM.ty1 - CAM.ty0) * H;
            float coc = CAM.lensR * fabsf(1.0f / zd - 1.0f / CAM.focus) / ((CAM.tx1 - CAM.tx0) / W);
            motes.push_back(fx - px0); motes.push_back(fy - py0); motes.push_back(coc); motes.push_back(size * tr * tv / (zd * zd) * 36.0f);
        }
        writeF32(prefix + "_motes.f32", motes);
    }
    return 0;
}
