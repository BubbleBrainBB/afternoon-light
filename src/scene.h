// scene.h — materials, procedural textures and the whole scene, built in code.
#pragma once
#include "geom.h"

// ---------------- global scene parameters ----------------
static const float RX = 5.6f, RZ = 7.8f, RH = 2.85f; // room extents (x: left wall -> right, z: back wall -> behind camera)
static const float WT = 0.30f;                      // wall thickness
static const float DZ0 = 0.42f, DZ1 = 1.92f, DY1 = 2.42f; // french-door opening in the left wall (x = 0)
static const float CURT_X = 0.115f, CURT_Z0 = 0.05f, CURT_Z1 = 0.58f, CURT_TOP = 2.56f, CURT_BOT = 0.018f;
static const float CURT_CLOTH_W = 1.5f;             // flattened cloth width (m)
// second opening: a window in the right wall (never in frame) that lets in cool sky light
static const float W2Z0 = 1.2f, W2Z1 = 3.6f, W2Y0 = 0.85f, W2Y1 = 2.35f;             // flattened cloth width (m)

static V3 SUN_TRAVEL, TO_SUN, ESUN;
static float SUN_COS_MAX, SUN_OMEGA;
static void initSun(float azDeg, float elDeg) {
    float az = rad(azDeg), el = rad(elDeg);
    SUN_TRAVEL = normalize(V3(cosf(el) * cosf(az), -sinf(el), -cosf(el) * sinf(az)));
    TO_SUN = -SUN_TRAVEL;
    float ang = rad(0.30f);
    SUN_COS_MAX = cosf(ang);
    SUN_OMEGA = 2 * PI * (1 - SUN_COS_MAX);
    ESUN = V3(1.0f, 0.80f, 0.58f) * 10.5f; // warm, low afternoon sun (irradiance at normal incidence)
}
static V3 skyColor(V3 d) {
    if (d.y < 0) return V3(0.42f, 0.44f, 0.40f) * 0.55f;
    float t = powf(d.y, 0.42f);
    V3 c = lerp(V3(0.92f, 0.95f, 1.0f) * 1.15f, V3(0.24f, 0.42f, 0.86f) * 0.8f, t);
    float cs = std::max(0.f, dot(d, TO_SUN));
    c += V3(1.0f, 0.85f, 0.65f) * (0.5f * powf(cs, 6.0f) + 2.0f * powf(cs, 48.0f));
    return c;
}

// ---------------- materials ----------------
enum { MT_OPAQUE, MT_METAL, MT_THIN, MT_SHEER };
enum { TX_NONE, TX_PLASTER, TX_FLOOR, TX_WOOD, TX_CURTAIN, TX_LEAF, TX_PAGES, TX_CERAMIC, TX_HEDGE, TX_GROUND,
       TX_ART, TX_FRUIT, TX_STRAW, TX_LINEN, TX_FARTREES, TX_CLAY, TX_PAINT, TX_AUTUMN, TX_BARK };
struct Mat { int type; V3 col; float rough; float f0; int tex; V3 col2; float trans; };
enum {
    M_WALL, M_CEIL, M_FLOOR, M_TRIM, M_CURTAIN, M_ROD, M_LEAF, M_BARK, M_HEDGE, M_FARTREES, M_GROUND,
    M_TABLE, M_CHAIR, M_VASE, M_FRUIT, M_CALYX, M_TWIG, M_DRYLEAF, M_TEAPOT, M_CUP, M_TEA,
    M_BOOK1, M_BOOK2, M_BOOK3, M_PAGES, M_FRAME, M_MATBOARD, M_ART, M_HAT, M_RIBBON, M_BRASS, M_SHRUB, M_COUNT
};
static Mat MATS[M_COUNT];
static void initMaterials() {
    MATS[M_WALL]     = {MT_OPAQUE, V3(0.80f, 0.765f, 0.70f), 0.85f, 0.03f, TX_PLASTER, V3(0), 0};
    MATS[M_CEIL]     = {MT_OPAQUE, V3(0.82f, 0.80f, 0.76f), 0.9f, 0.03f, TX_NONE, V3(0), 0};
    MATS[M_FLOOR]    = {MT_OPAQUE, V3(0.45f, 0.305f, 0.165f), 0.22f, 0.05f, TX_FLOOR, V3(0.26f, 0.158f, 0.075f), 0};
    MATS[M_TRIM]     = {MT_OPAQUE, V3(0.80f, 0.78f, 0.72f), 0.38f, 0.045f, TX_PAINT, V3(0), 0};
    MATS[M_CURTAIN]  = {MT_SHEER, V3(0.94f, 0.92f, 0.87f), 0.9f, 0.0f, TX_CURTAIN, V3(0), 0.60f};
    MATS[M_ROD]      = {MT_METAL, V3(0.20f, 0.17f, 0.14f), 0.4f, 0.5f, TX_NONE, V3(0), 0};
    MATS[M_LEAF]     = {MT_THIN, V3(0.60f, 0.40f, 0.055f), 0.6f, 0.04f, TX_AUTUMN, V3(0.50f, 0.17f, 0.035f), 0.42f};
    MATS[M_SHRUB]    = {MT_THIN, V3(0.62f, 0.36f, 0.05f), 0.6f, 0.04f, TX_AUTUMN, V3(0.56f, 0.14f, 0.03f), 0.42f};
    MATS[M_BARK]     = {MT_OPAQUE, V3(0.075f, 0.058f, 0.046f), 0.9f, 0.03f, TX_BARK, V3(0), 0};
    MATS[M_HEDGE]    = {MT_OPAQUE, V3(0.055f, 0.085f, 0.04f), 0.9f, 0.02f, TX_HEDGE, V3(0.17f, 0.20f, 0.085f), 0};
    MATS[M_FARTREES] = {MT_OPAQUE, V3(0.10f, 0.17f, 0.08f), 0.9f, 0.02f, TX_FARTREES, V3(0.22f, 0.30f, 0.14f), 0};
    MATS[M_GROUND]   = {MT_OPAQUE, V3(0.50f, 0.46f, 0.39f), 0.9f, 0.03f, TX_GROUND, V3(0.11f, 0.21f, 0.045f), 0};
    MATS[M_TABLE]    = {MT_OPAQUE, V3(0.235f, 0.125f, 0.060f), 0.32f, 0.045f, TX_WOOD, V3(0.105f, 0.050f, 0.024f), 0};
    MATS[M_CHAIR]    = {MT_OPAQUE, V3(0.56f, 0.40f, 0.22f), 0.36f, 0.045f, TX_WOOD, V3(0.40f, 0.255f, 0.12f), 0};
    MATS[M_VASE]     = {MT_OPAQUE, V3(0.70f, 0.69f, 0.64f), 0.30f, 0.05f, TX_CERAMIC, V3(0.36f, 0.25f, 0.17f), 0};
    MATS[M_FRUIT]    = {MT_OPAQUE, V3(0.86f, 0.27f, 0.025f), 0.33f, 0.045f, TX_FRUIT, V3(0.92f, 0.46f, 0.05f), 0};
    MATS[M_CALYX]    = {MT_OPAQUE, V3(0.20f, 0.17f, 0.07f), 0.7f, 0.03f, TX_NONE, V3(0), 0};
    MATS[M_TWIG]     = {MT_OPAQUE, V3(0.13f, 0.09f, 0.065f), 0.75f, 0.03f, TX_NONE, V3(0), 0};
    MATS[M_DRYLEAF]  = {MT_THIN, V3(0.36f, 0.15f, 0.05f), 0.6f, 0.04f, TX_LEAF, V3(0.46f, 0.25f, 0.07f), 0.30f};
    MATS[M_TEAPOT]   = {MT_OPAQUE, V3(0.30f, 0.115f, 0.062f), 0.42f, 0.04f, TX_CLAY, V3(0), 0};
    MATS[M_CUP]      = {MT_OPAQUE, V3(0.84f, 0.84f, 0.80f), 0.12f, 0.05f, TX_NONE, V3(0), 0};
    MATS[M_TEA]      = {MT_OPAQUE, V3(0.30f, 0.11f, 0.012f), 0.03f, 0.03f, TX_NONE, V3(0), 0};
    MATS[M_BOOK1]    = {MT_OPAQUE, V3(0.12f, 0.19f, 0.27f), 0.75f, 0.035f, TX_LINEN, V3(0), 0};
    MATS[M_BOOK2]    = {MT_OPAQUE, V3(0.52f, 0.25f, 0.12f), 0.75f, 0.035f, TX_LINEN, V3(0), 0};
    MATS[M_BOOK3]    = {MT_OPAQUE, V3(0.55f, 0.50f, 0.40f), 0.75f, 0.035f, TX_LINEN, V3(0), 0};
    MATS[M_PAGES]    = {MT_OPAQUE, V3(0.80f, 0.76f, 0.66f), 0.85f, 0.03f, TX_PAGES, V3(0), 0};
    MATS[M_FRAME]    = {MT_OPAQUE, V3(0.50f, 0.36f, 0.20f), 0.38f, 0.045f, TX_WOOD, V3(0.36f, 0.23f, 0.11f), 0};
    MATS[M_MATBOARD] = {MT_OPAQUE, V3(0.83f, 0.81f, 0.76f), 0.9f, 0.03f, TX_NONE, V3(0), 0};
    MATS[M_ART]      = {MT_OPAQUE, V3(0.8f), 0.8f, 0.03f, TX_ART, V3(0), 0};
    MATS[M_HAT]      = {MT_OPAQUE, V3(0.62f, 0.49f, 0.27f), 0.7f, 0.035f, TX_STRAW, V3(0), 0};
    MATS[M_RIBBON]   = {MT_OPAQUE, V3(0.05f, 0.07f, 0.11f), 0.6f, 0.04f, TX_NONE, V3(0), 0};
    MATS[M_BRASS]    = {MT_METAL, V3(0.72f, 0.52f, 0.26f), 0.32f, 0.5f, TX_NONE, V3(0), 0};
}

// ---------------- procedural textures ----------------
static float woodPat(V3 q, float seed, float ringScale) { // q.x runs along the grain (metres)
    float warp = fbm(V3(q.x * 1.3f + seed * 13.7f, q.y * 6.0f, q.z * 6.0f), 3);
    float ry = q.y + 0.07f + 0.02f * pnoise(V3(q.x * 2.1f, seed * 7.3f, 0.5f));
    float rz = q.z + 0.13f + 0.02f * pnoise(V3(q.x * 1.7f, 3.1f, seed * 5.1f));
    float r = sqrtf(ry * ry + rz * rz) * ringScale + warp * 1.1f;
    float fr = r - floorf(r);
    float ring = smoothstep(0.0f, 0.65f, fr) * (1.0f - smoothstep(0.80f, 1.0f, fr));
    float fiber = fbm(V3(q.x * 9.0f, q.y * 260.0f, q.z * 260.0f + seed), 2);
    return sat(0.22f + 0.60f * ring + 0.35f * fiber);
}

struct Surf {
    V3 p, ng, n, alb, spec;
    float rough; int kind; // 0 coated diffuse, 1 metal, 2 thin translucent
    float trans; bool sheer;
};

template <class H>
static V3 bumpN(V3 n, V3 p, float eps, float amp, H&& h) {
    float h0 = h(p);
    V3 g((h(p + V3(eps, 0, 0)) - h0) / eps, (h(p + V3(0, eps, 0)) - h0) / eps, (h(p + V3(0, 0, eps)) - h0) / eps);
    g = g - n * dot(g, n);
    return normalize(n - g * amp);
}

// openness (probability a ray passes straight through) of the sheer curtain
static float sheerOpen(V3 T, float cosTheta) {
    float su = pnoise(V3(T.x * 210.0f, 0.3f, 1.7f)), sv = pnoise(V3(T.y * 260.0f, 4.1f, 0.2f));
    float su2 = pnoise(V3(T.x * 37.0f, 2.3f, 5.7f));
    float cover = 0.33f * (1.0f + 0.22f * su + 0.12f * sv + 0.12f * su2);
    float H = CURT_TOP - CURT_BOT;
    if (T.y > H - 0.075f || T.x < 0.028f || T.x > T.z - 0.028f) cover = 1.0f - sqr(1.0f - cover); // doubled hems
    if (T.y < 0.07f) cover = 0.93f;                                                                         // header tape
    float c = powf(std::max(fabsf(cosTheta), 0.12f), 0.8f);
    return sat(1.0f - cover / c);
}

// lod > 0: the hit is on an indirect bounce, so sub-pixel texture detail and bump are skipped
static void evalSurface(const Hit& h, V3 rd, Surf& s, int lod = 0) {
    const Tri& t = G.tris[h.tri];
    const Mat& m = MATS[t.mat];
    float w = 1 - h.u - h.v;
    V3 p = G.P[t.a] * w + G.P[t.b] * h.u + G.P[t.c] * h.v;
    V3 n = normalize(G.N[t.a] * w + G.N[t.b] * h.u + G.N[t.c] * h.v);
    V3 T = G.T[t.a] * w + G.T[t.b] * h.u + G.T[t.c] * h.v;
    V3 ng = normalize(cross(G.P[t.b] - G.P[t.a], G.P[t.c] - G.P[t.a]));
    if (dot(ng, rd) > 0) ng = -ng;
    if (dot(n, ng) < 0) n = -n;
    s.p = p; s.ng = ng;
    s.alb = m.col; s.rough = m.rough; s.spec = V3(m.f0); s.trans = m.trans;
    s.sheer = m.type == MT_SHEER;
    s.kind = m.type == MT_METAL ? 1 : (m.type == MT_THIN || m.type == MT_SHEER ? 2 : 0);
    if (s.kind == 1) s.spec = m.col;
    int tex = m.tex;
    if (lod > 0) {
        if (tex == TX_PLASTER || tex == TX_PAINT || tex == TX_BARK || tex == TX_CLAY || tex == TX_LINEN || tex == TX_PAGES || tex == TX_STRAW) tex = TX_NONE;
        else if (tex == TX_WOOD) { s.alb = lerp(m.col2, m.col, 0.55f); tex = TX_NONE; }
    }
    switch (tex) {
    case TX_PLASTER: {
        float a = fbm(T * 1.3f + V3(3.1f, 0, 0), 3), b = fbm(T * 19.0f, 3);
        s.alb = m.col * (1.0f + 0.045f * a + 0.03f * b);
        n = bumpN(n, T, 0.003f, 1.0f, [](V3 q) { return 0.0030f * fbm(q * 7.0f, 3) + 0.0007f * fbm(q * 55.0f, 2); });
    } break;
    case TX_PAINT: {
        s.alb = m.col * (1.0f + 0.03f * fbm(T * 9.0f, 2));
        n = bumpN(n, T, 0.002f, 1.0f, [](V3 q) { return 0.0006f * fbm(V3(q.x * 30.0f, q.y * 6.0f, q.z * 30.0f), 2); });
    } break;
    case TX_FLOOR: {
        const float W = 0.085f; const int N = 5;
        float u = (T.x + T.z) * 0.70710678f / W + 100.0f, v = (T.z - T.x) * 0.70710678f / W + 100.0f;
        float k = floorf(v); float xp = u - k, yp = v - k;
        float cell = floorf(xp / (2 * N)); float mm = xp - cell * 2 * N;
        float along, across; uint32_t id;
        if (mm < N) { along = mm; across = yp; id = hash3i((int)k, (int)cell, 0); }
        else { float c = floorf(mm) - N; across = mm - floorf(mm); along = yp - (c + 1 - N); id = hash3i((int)(k + c), (int)cell, 1); }
        float de = std::min(std::min(across, 1 - across), std::min(along, N - along)) * W;
        float groove = smoothstep(0.0002f, 0.0013f, de);
        float seed = hashf(id);
        float pat = lod > 0 ? 0.55f : woodPat(V3(along * W + seed * 11.0f, (across - 0.5f) * W + (hashf(id * 7 + 3) - 0.5f) * 0.2f, seed * 3.0f), seed * 10.0f, 95.0f);
        float tone = 0.80f + 0.42f * hashf(id * 3 + 1);
        V3 tint = V3(1.0f, 0.96f + 0.08f * hashf(id * 5 + 2), 0.92f + 0.16f * hashf(id * 11 + 5));
        s.alb = lerp(m.col2, m.col, pat) * tint * tone * (0.25f + 0.75f * groove);
        float smudge = lod > 0 ? 0.0f : fbm(V3(T.x * 2.3f, 1.0f, T.z * 2.3f), 3);
        s.rough = m.rough + 0.07f * smudge + 0.05f * (pat - 0.5f) + (1 - groove) * 0.35f + 0.04f * (hashf(id * 13 + 7) - 0.5f);
        V3 tilt((hashf(id * 17 + 1) - 0.5f) * 0.006f, 0, (hashf(id * 19 + 2) - 0.5f) * 0.006f);
        if (lod == 0) n = normalize(n + tilt + V3(0.0015f * pnoise(V3(T.x * 9, 0, T.z * 9)), 0, 0.0015f * pnoise(V3(T.x * 9, 7, T.z * 9))));
    } break;
    case TX_WOOD: {
        float pat = woodPat(T, T.x * 0.0f + 1.7f, 140.0f);
        s.alb = lerp(m.col2, m.col, pat);
        s.rough = m.rough + 0.08f * (0.5f - pat);
    } break;
    case TX_CURTAIN: {
        float sl = pnoise(V3(T.x * 180.0f, 0.3f, 1.7f));
        s.alb = m.col * (0.94f + 0.05f * sl);
    } break;
    case TX_LEAF: {
        float r = hashf((uint32_t)(T.z * 65536.0f));
        s.alb = lerp(m.col, m.col2, r) * (0.85f + 0.3f * hashf((uint32_t)(T.z * 65536.0f) * 7 + 1));
        float vein = smoothstep(0.10f, 0.0f, fabsf(T.y));
        s.alb = s.alb * (1.0f + 0.25f * vein);
    } break;
    case TX_AUTUMN: {
        uint32_t hid = (uint32_t)(T.z * 65536.0f);
        float r = hashf(hid), r2 = hashf(hid * 7 + 1);
        V3 green(0.13f, 0.19f, 0.045f);
        V3 c = r < 0.46f ? lerp(m.col, V3(0.66f, 0.50f, 0.09f), r2) : (r < 0.70f ? lerp(m.col2, m.col, r2 * 0.6f) : lerp(green, m.col, r2 * 0.5f));
        float vein = smoothstep(0.10f, 0.0f, fabsf(T.y));
        float tipBrown = smoothstep(0.75f, 1.0f, T.x) * hashf(hid * 13 + 5);
        s.alb = lerp(c * (0.85f + 0.3f * r2) * (1.0f + 0.2f * vein), V3(0.30f, 0.14f, 0.05f), 0.6f * tipBrown);
    } break;
    case TX_PAGES: {
        float l = pnoise(V3(T.y * 2600.0f, T.x * 3.0f, T.z * 3.0f));
        s.alb = m.col * (0.86f + 0.14f * l);
    } break;
    case TX_CERAMIC: {
        // dipped glaze: lower part raw clay, upper part speckled off-white glaze
        float edge = 0.062f + 0.006f * pnoise(V3(T.x * 30.0f, 0.0f, T.z * 30.0f)) + 0.004f * pnoise(V3(T.x * 90.0f, 3.0f, T.z * 90.0f));
        float g = smoothstep(edge - 0.0015f, edge + 0.0015f, T.y);
        float sp = pnoise(T * 520.0f);
        float speck = smoothstep(0.60f, 0.74f, sp);
        V3 glaze = m.col * (1.0f + 0.05f * fbm(T * 40.0f, 2)) * (1.0f - 0.42f * speck);
        V3 clay = m.col2 * (1.0f + 0.15f * fbm(T * 120.0f, 2));
        s.alb = lerp(clay, glaze, g);
        s.rough = lerp(0.75f, m.rough, g);
        s.spec = V3(lerp(0.03f, m.f0, g));
    } break;
    case TX_CLAY: {
        s.alb = m.col * (1.0f + 0.10f * fbm(T * 150.0f, 2));
    } break;
    case TX_BARK: {
        float a = fbm(V3(T.x * 60.0f, T.y * 7.0f, T.z * 60.0f), 3), b = fbm(T * 9.0f, 2);
        s.alb = m.col * (1.0f + 0.45f * a + 0.25f * b);
        n = bumpN(n, T, 0.002f, 1.0f, [](V3 q) { return 0.004f * fbm(V3(q.x * 60.0f, q.y * 7.0f, q.z * 60.0f), 3); });
    } break;
    case TX_HEDGE: {
        float a = fbm(V3(T.x * 1.1f, T.y * 1.3f, T.z), 4) * 0.5f + 0.5f;
        float b = fbm(V3(T.x * 7.0f, T.y * 7.0f, 3.0f), 4) * 0.5f + 0.5f;
        float c = fbm(V3(T.x * 31.0f, T.y * 31.0f, 9.0f), 3) * 0.5f + 0.5f;
        float leafy = sat(0.5f * a + 0.9f * (b - 0.5f) + 0.7f * (c - 0.5f) + 0.2f);
        s.alb = lerp(m.col * 0.35f, m.col2, smoothstep(0.25f, 0.8f, leafy));
        s.alb = lerp(s.alb, V3(0.30f, 0.20f, 0.05f), 0.5f * smoothstep(0.62f, 0.8f, a) * c); // a few turning patches
        if (lod == 0) n = bumpN(n, T, 0.006f, 1.0f, [](V3 q) { return 0.05f * fbm(V3(q.x * 9.0f, q.y * 9.0f, 1.0f), 3) + 0.02f * fabsf(pnoise(V3(q.x * 42.0f, q.y * 42.0f, 5.0f))); });
    } break;
    case TX_FARTREES: {
        float a = fbm(V3(T.x * 0.35f, T.y * 0.5f, 1.0f), 4) * 0.5f + 0.5f;
        float b = fbm(V3(T.x * 2.5f, T.y * 2.5f, 4.0f), 3) * 0.5f + 0.5f;
        V3 c = lerp(m.col, m.col2, sat(a + 0.5f * (b - 0.5f)));
        c = lerp(c, V3(0.34f, 0.24f, 0.08f), 0.6f * smoothstep(0.55f, 0.75f, b));
        s.alb = lerp(c, V3(0.36f, 0.42f, 0.48f), 0.35f); // aerial haze
    } break;
    case TX_GROUND: {
        if (T.x > -4.2f + 0.25f * pnoise(V3(0, 0, T.z * 0.7f))) { // stone terrace
            float fx = T.x / 0.62f, fz = T.z / 0.62f + 0.5f * floorf(fx);
            float ex = fx - floorf(fx), ez = fz - floorf(fz);
            float joint = smoothstep(0.0f, 0.02f, std::min(std::min(ex, 1 - ex), std::min(ez, 1 - ez)));
            float tone = 0.85f + 0.3f * hashf(hash3i((int)floorf(fx), (int)floorf(fz), 5));
            s.alb = m.col * tone * (0.55f + 0.45f * joint) * (1.0f + 0.12f * fbm(T * 6.0f, 3));
        } else {
            float a = fbm(V3(T.x * 0.8f, 0.0f, T.z * 0.8f), 3) * 0.5f + 0.5f;
            float b = fbm(V3(T.x * 14.0f, 0.0f, T.z * 14.0f), 3);
            s.alb = lerp(m.col2 * 0.75f, V3(0.17f, 0.20f, 0.06f), a) * (1.0f + 0.5f * b);
        }
    } break;
    case TX_ART: {
        // a quiet little landscape print: pale sky, low sun, layered hills
        float u = T.x, v = T.y; // 0..1, v up
        V3 paper(0.80f, 0.77f, 0.69f);
        V3 c = lerp(V3(0.78f, 0.70f, 0.58f), V3(0.62f, 0.68f, 0.70f), smoothstep(0.35f, 1.0f, v));
        float sun = length(V3((u - 0.64f) * 0.8f, v - 0.60f, 0));
        c = lerp(V3(0.80f, 0.36f, 0.16f), c, smoothstep(0.085f, 0.090f, sun));
        for (int i = 0; i < 4; i++) {
            float hgt = 0.50f - 0.11f * i + 0.07f * fbm(V3(u * (1.6f + i) + i * 7.3f, i * 3.1f, 0.5f), 3) + 0.03f * sinf(u * (3.0f + i) + i);
            V3 hc = lerp(V3(0.50f, 0.56f, 0.58f), V3(0.13f, 0.19f, 0.22f), i / 3.0f);
            c = lerp(c, hc, smoothstep(0.004f, -0.004f, v - hgt));
        }
        float grain = 1.0f + 0.06f * fbm(V3(u * 180.0f, v * 180.0f, 0.0f), 2);
        bool in = u > 0.0f && u < 1.0f && v > 0.0f && v < 1.0f;
        s.alb = (in ? c : paper) * grain;
    } break;
    case TX_FRUIT: {
        float a = fbm(T * 25.0f, 3);
        float top = smoothstep(0.2f, 1.0f, T.y); // T.y = normalized height (-1..1)
        s.alb = lerp(m.col, m.col2, sat(0.25f + 0.6f * top + 0.5f * a));
        s.alb = s.alb * (1.0f + 0.12f * fbm(T * 90.0f, 2));
    } break;
    case TX_STRAW: {
        float r = sqrtf(T.x * T.x + T.z * T.z);
        float row = (r + T.y * 1.0f) / 0.0062f;
        float fr = row - floorf(row);
        float braid = 0.80f + 0.20f * sinf(fr * PI);
        float ang = atan2f(T.z, T.x);
        float weave = 0.92f + 0.08f * sinf(ang * r * 900.0f + floorf(row) * 2.4f);
        s.alb = m.col * braid * weave * (0.9f + 0.2f * hashf((uint32_t)floorf(row) * 31 + 7));
    } break;
    case TX_LINEN: {
        float wv = pnoise(V3(T.x * 900.0f, T.y * 60.0f, T.z * 60.0f)) + pnoise(V3(T.x * 60.0f, T.y * 60.0f, T.z * 900.0f));
        s.alb = m.col * (1.0f + 0.10f * wv);
    } break;
    default: break;
    }
    if (dot(n, rd) > 0) n = normalize(n - rd * (dot(n, rd) + 1e-3f)); // keep shading normal facing the viewer
    s.n = n;
    s.rough = clampf(s.rough, 0.03f, 1.0f);
}

// ---------------- scene construction ----------------
static void setTexLocal(V3 off = V3(0, 0, 0)) { TEXWORLD = false; TEX = xfT(off); }
// wood grain along local axis ax (0=x,1=y,2=z) with a random offset so each part differs
static void setTexWood(int ax, RNG& rng) {
    TEXWORLD = false; Xf t;
    if (ax == 1) { t.X = V3(0, 1, 0); t.Y = V3(1, 0, 0); t.Z = V3(0, 0, 1); }       // local y -> tex x
    else if (ax == 2) { t.X = V3(0, 0, 1); t.Y = V3(0, 1, 0); t.Z = V3(1, 0, 0); }  // local z -> tex x
    t.T = V3(rng.range(0, 20), rng.range(-0.1f, 0.1f), rng.range(-0.1f, 0.1f));
    TEX = t;
}

static void buildRoom() {
    CUR = Xf(); TEXWORLD = true;
    const float e = 0.15f;
    quad(V3(-WT, 0, -e), V3(RX + e, 0, -e), V3(RX + e, 0, RZ + e), V3(-WT, 0, RZ + e), M_FLOOR);
    quad(V3(-WT, RH, -e), V3(RX + e, RH, -e), V3(RX + e, RH, RZ + e), V3(-WT, RH, RZ + e), M_CEIL);
    quad(V3(-WT - e, -e, 0), V3(RX + e, -e, 0), V3(RX + e, RH + e, 0), V3(-WT - e, RH + e, 0), M_WALL);   // back wall
    quad(V3(-WT - e, -e, RZ), V3(RX + e, -e, RZ), V3(RX + e, RH + e, RZ), V3(-WT - e, RH + e, RZ), M_WALL); // front wall
    // right wall with the fill window
    quad(V3(RX, -e, -e), V3(RX, -e, W2Z0), V3(RX, RH + e, W2Z0), V3(RX, RH + e, -e), M_WALL);
    quad(V3(RX, -e, W2Z1), V3(RX, -e, RZ + e), V3(RX, RH + e, RZ + e), V3(RX, RH + e, W2Z1), M_WALL);
    quad(V3(RX, -e, W2Z0), V3(RX, -e, W2Z1), V3(RX, W2Y0, W2Z1), V3(RX, W2Y0, W2Z0), M_WALL);
    quad(V3(RX, W2Y1, W2Z0), V3(RX, W2Y1, W2Z1), V3(RX, RH + e, W2Z1), V3(RX, RH + e, W2Z0), M_WALL);
    quad(V3(RX, W2Y0, W2Z0), V3(RX + WT, W2Y0, W2Z0), V3(RX + WT, W2Y1, W2Z0), V3(RX, W2Y1, W2Z0), M_WALL);
    quad(V3(RX, W2Y0, W2Z1), V3(RX + WT, W2Y0, W2Z1), V3(RX + WT, W2Y1, W2Z1), V3(RX, W2Y1, W2Z1), M_WALL);
    quad(V3(RX, W2Y0, W2Z0), V3(RX + WT, W2Y0, W2Z0), V3(RX + WT, W2Y0, W2Z1), V3(RX, W2Y0, W2Z1), M_TRIM);
    quad(V3(RX, W2Y1, W2Z0), V3(RX + WT, W2Y1, W2Z0), V3(RX + WT, W2Y1, W2Z1), V3(RX, W2Y1, W2Z1), M_WALL);
    for (int k = 0; k < 2; k++) { // left wall: inner face (x=0) and outer face (x=-WT)
        float x = k ? -WT : 0.0f;
        quad(V3(x, -e, -e), V3(x, -e, DZ0), V3(x, RH + e, DZ0), V3(x, RH + e, -e), M_WALL);
        quad(V3(x, -e, DZ1), V3(x, -e, RZ + e), V3(x, RH + e, RZ + e), V3(x, RH + e, DZ1), M_WALL);
        quad(V3(x, DY1, DZ0), V3(x, DY1, DZ1), V3(x, RH + e, DZ1), V3(x, RH + e, DZ0), M_WALL);
    }
    // reveals
    quad(V3(-WT, 0, DZ0), V3(0, 0, DZ0), V3(0, DY1, DZ0), V3(-WT, DY1, DZ0), M_WALL);
    quad(V3(-WT, 0, DZ1), V3(0, 0, DZ1), V3(0, DY1, DZ1), V3(-WT, DY1, DZ1), M_WALL);
    quad(V3(-WT, DY1, DZ0), V3(0, DY1, DZ0), V3(0, DY1, DZ1), V3(-WT, DY1, DZ1), M_WALL);

    // painted trim: casing around the opening + baseboards
    const float cw = 0.085f, ct = 0.018f;
    rboxMM(V3(0, 0, DZ0 - cw), V3(ct, DY1 + cw, DZ0), 0.004f, M_TRIM);
    rboxMM(V3(0, 0, DZ1), V3(ct, DY1 + cw, DZ1 + cw), 0.004f, M_TRIM);
    rboxMM(V3(0, DY1, DZ0 - cw), V3(ct, DY1 + cw, DZ1 + cw), 0.004f, M_TRIM);
    const float bh = 0.115f, bt = 0.016f;
    rboxMM(V3(0, 0, 0), V3(RX, bh, bt), 0.005f, M_TRIM);
    rboxMM(V3(0, 0, bt), V3(bt, bh, DZ0 - cw), 0.005f, M_TRIM);
    rboxMM(V3(0, 0, DZ1 + cw), V3(bt, bh, RZ), 0.005f, M_TRIM);
}

static void buildDoor() {
    CUR = Xf(); TEXWORLD = true;
    const float xc = -0.215f, th = 0.022f; // door plane centre, half thickness
    const float jw = 0.045f;
    // frame
    rboxMM(V3(xc - 0.05f, DY1 - jw, DZ0), V3(xc + 0.05f, DY1, DZ1), 0.004f, M_TRIM);
    rboxMM(V3(xc - 0.05f, 0, DZ0), V3(xc + 0.05f, DY1, DZ0 + jw), 0.004f, M_TRIM);
    rboxMM(V3(xc - 0.05f, 0, DZ1 - jw), V3(xc + 0.05f, DY1, DZ1), 0.004f, M_TRIM);
    rboxMM(V3(-WT, -0.01f, DZ0), V3(xc + 0.07f, 0.022f, DZ1), 0.006f, M_CHAIR); // oak threshold
    float zc = (DZ0 + DZ1) * 0.5f;
    for (int leaf = 0; leaf < 2; leaf++) {
        float z0 = leaf ? zc + 0.0015f : DZ0 + jw + 0.002f, z1 = leaf ? DZ1 - jw - 0.002f : zc - 0.0015f;
        float y0 = 0.028f, y1 = DY1 - jw - 0.003f;
        const float sw = 0.078f, tr = 0.082f, br = 0.27f;
        rboxMM(V3(xc - th, y0, z0), V3(xc + th, y1, z0 + sw), 0.004f, M_TRIM);
        rboxMM(V3(xc - th, y0, z1 - sw), V3(xc + th, y1, z1), 0.004f, M_TRIM);
        rboxMM(V3(xc - th, y1 - tr, z0 + sw), V3(xc + th, y1, z1 - sw), 0.004f, M_TRIM);
        rboxMM(V3(xc - th, y0, z0 + sw), V3(xc + th, y0 + br, z1 - sw), 0.004f, M_TRIM);
        // recessed kick panel moulding
        rboxMM(V3(xc + th - 0.002f, y0 + 0.05f, z0 + sw + 0.05f), V3(xc + th + 0.004f, y0 + br - 0.05f, z1 - sw - 0.05f), 0.003f, M_TRIM);
        // muntins: 2 x 5 panes
        float gz0 = z0 + sw, gz1 = z1 - sw, gy0 = y0 + br, gy1 = y1 - tr;
        const float mw = 0.011f, md = 0.015f;
        float zm = (gz0 + gz1) * 0.5f;
        rboxMM(V3(xc - md, gy0, zm - mw), V3(xc + md, gy1, zm + mw), 0.003f, M_TRIM);
        for (int r = 1; r < 5; r++) {
            float y = gy0 + (gy1 - gy0) * r / 5.0f;
            rboxMM(V3(xc - md, y - mw, gz0), V3(xc + md, y + mw, gz1), 0.003f, M_TRIM);
        }
    }
    // lever handle on the near leaf, room side
    {
        float hz = zc + 0.0015f + 0.039f, hy = 1.03f, hx = xc + th;
        rboxMM(V3(hx, hy - 0.09f, hz - 0.017f), V3(hx + 0.006f, hy + 0.09f, hz + 0.017f), 0.003f, M_BRASS);
        tube({V3(hx, hy + 0.03f, hz), V3(hx + 0.045f, hy + 0.03f, hz), V3(hx + 0.055f, hy + 0.03f, hz + 0.012f), V3(hx + 0.055f, hy + 0.028f, hz + 0.11f)},
             {0.008f, 0.008f, 0.0075f, 0.006f}, M_BRASS, 12);
    }
}

static void curtainPanel(float Z0, float Z1, float clothW, float NF, float seed, int NU) {
    CUR = Xf(); TEXWORLD = false;
    const int NV = 170;
    size_t v0 = G.P.size(), t0 = G.tris.size();
    std::vector<uint32_t> id((size_t)(NU + 1) * (NV + 1));
    float H = CURT_TOP - CURT_BOT;
    for (int i = 0; i <= NU; i++)
        for (int j = 0; j <= NV; j++) {
            float u = (float)i / NU, v = (float)j / NV;
            float relax = smoothstep(0.0f, 0.5f, v);
            float ph = 2 * PI * (NF * u + 0.22f * pnoise(V3(u * 5.0f, 1.3f + seed, 0.7f)) + relax * 0.18f * pnoise(V3(u * 7.0f, v * 1.6f, 4.2f + seed)));
            float amp = 0.030f * (0.72f + 0.28f * relax) * (1.0f + 0.25f * pnoise(V3(u * 6.0f, v * 1.1f, 9.1f + seed)));
            float sw = sinf(ph);
            float x = CURT_X + amp * (sw + 0.15f * sinf(2 * ph + 0.6f)) + relax * 0.018f * pnoise(V3(u * 2.4f, v * 1.3f, 2.2f + seed)) + 0.012f * v * v;
            float z = lerp(Z0, Z1, u) + 0.012f * cosf(ph) * (0.6f + 0.4f * relax) + relax * 0.02f * pnoise(V3(u * 3.0f, v * 1.2f, 7.7f + seed));
            float y = CURT_TOP - v * H;
            id[(size_t)i * (NV + 1) + j] = addVT(V3(x, y, z), V3(1, 0, 0), V3(u * clothW, v * H, clothW));
        }
    for (int i = 0; i < NU; i++)
        for (int j = 0; j < NV; j++)
            addQuadI(id[(size_t)i * (NV + 1) + j], id[(size_t)(i + 1) * (NV + 1) + j], id[(size_t)(i + 1) * (NV + 1) + j + 1], id[(size_t)i * (NV + 1) + j + 1], M_CURTAIN);
    recomputeNormals(v0, t0);
}
static void buildCurtain() {
    curtainPanel(CURT_Z0, CURT_Z1, CURT_CLOTH_W, 6.5f, 0.0f, 520);   // far panel, by the corner: catches the sun
    CUR = Xf(); TEXWORLD = false;
    // rod, finial and rings
    float ry = CURT_TOP + 0.028f;
    cyl(V3(CURT_X, ry, 0.03f), V3(CURT_X, ry, 2.30f), 0.010f, 0.010f, M_ROD, 16);
    for (int k = 0; k < 2; k++) {
        float z = k ? 2.30f : 0.03f;
        Xf save = CUR; CUR = xfT(CURT_X, ry, z) * xfRX(k ? rad(90) : rad(-90));
        lathe({V3(0.010f, 0, 0), V3(0.012f, 0.004f, 0), V3(0.02f, 0.02f, 0), V3(0.017f, 0.036f, 0), V3(0.0f, 0.044f, 0)}, M_ROD, 24, 4);
        CUR = save;
    }
    for (int k = 0; k < 3; k++) { // wall brackets
        float z = k == 0 ? 0.10f : (k == 1 ? 1.17f : 2.20f);
        cyl(V3(0, ry, z), V3(CURT_X, ry, z), 0.006f, 0.006f, M_ROD, 10);
    }
}

static void addLeaf(V3 base, V3 axis, V3 nrm, float L, float W, int mat, RNG& rng, bool detailed) {
    V3 a = normalize(axis); V3 s = normalize(cross(nrm, a)); V3 n = cross(a, s);
    float rnd = rng.f();
    size_t v0 = G.P.size(), t0 = G.tris.size();
    Xf save = CUR; CUR = Xf();
    if (!detailed) {
        // hexagonal blade
        V3 p[6] = {base, base + a * (0.3f * L) + s * (0.5f * W), base + a * (0.72f * L) + s * (0.42f * W), base + a * L,
                   base + a * (0.72f * L) - s * (0.42f * W), base + a * (0.3f * L) - s * (0.5f * W)};
        float tj[6] = {0, 1, 1, 0, -1, -1}; float tt[6] = {0, 0.3f, 0.72f, 1, 0.72f, 0.3f};
        uint32_t id[6];
        for (int i = 0; i < 6; i++) id[i] = addVT(p[i], n, V3(tt[i], tj[i], rnd));
        addTri(id[0], id[1], id[5], mat); addQuadI(id[1], id[2], id[4], id[5], mat); addTri(id[2], id[3], id[4], mat);
    } else {
        const int NT = 7; float curl = rng.range(-0.25f, 0.35f), fold = rng.range(0.1f, 0.5f), twist = rng.range(-0.5f, 0.5f);
        std::vector<uint32_t> id((NT + 1) * 5);
        for (int i = 0; i <= NT; i++)
            for (int j = -2; j <= 2; j++) {
                float t = (float)i / NT; float w = 0.5f * W * powf(sinf(PI * powf(t, 0.72f)), 0.85f);
                float jj = j * 0.5f;
                float tw = twist * t;
                V3 side = s * cosf(tw) + n * sinf(tw), up = n * cosf(tw) - s * sinf(tw);
                V3 p = base + a * (t * L) + side * (jj * w) + up * (curl * t * t * L * 0.5f + fold * fabsf(jj) * w);
                id[i * 5 + j + 2] = addVT(p, n, V3(t, jj, rnd));
            }
        for (int i = 0; i < NT; i++)
            for (int j = 0; j < 4; j++) addQuadI(id[i * 5 + j], id[(i + 1) * 5 + j], id[(i + 1) * 5 + j + 1], id[i * 5 + j + 1], mat);
        recomputeNormals(v0, t0);
    }
    CUR = save;
}

static float CFG_treeAimY = 2.35f, CFG_treeAimZ = 1.0f, CFG_treeR = 0.95f;
// the garden tree that throws dappled shade through the door
static void buildTree(uint32_t seed) {
    RNG rng(seed, 77);
    CUR = Xf(); TEXWORLD = true;
    // canopy centre sits on the line from the upper far part of the door toward the sun
    V3 aim(-0.24f, CFG_treeAimY, CFG_treeAimZ);
    V3 C = aim + TO_SUN * 5.6f;
    V3 trunkBase(C.x - 0.6f, -0.12f, C.z + 1.3f);
    V3 crown = C + V3(-0.3f, -1.5f, 0.6f);
    tube({trunkBase, lerp(trunkBase, crown, 0.5f) + V3(0.1f, 0, -0.1f), crown}, {0.11f, 0.09f, 0.07f}, M_BARK, 12);
    V3 side, upv; onb(TO_SUN, side, upv);
    int nClusters = 34;
    for (int c = 0; c < nClusters; c++) {
        // cluster centre inside a flattened ellipsoid; a few outliers drift lower so the dapple thins toward the edges
        V3 o;
        do { o = V3(rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1)); } while (dot(o, o) > 1);
        float spread = c < 26 ? 1.0f : 1.7f;
        V3 cc = C + V3(o.x * 1.2f, o.y * 0.9f, o.z * 1.2f) * (spread * CFG_treeR);
        // branch from crown to cluster
        V3 mid = lerp(crown, cc, 0.55f) + V3(rng.range(-0.15f, 0.15f), rng.range(-0.05f, 0.2f), rng.range(-0.15f, 0.15f));
        {
            std::vector<V3> pts = crRefine(std::vector<V3>{crown, mid, cc}, 4);
            std::vector<float> rr(pts.size());
            for (size_t i = 0; i < pts.size(); i++) rr[i] = lerp(0.035f, 0.008f, (float)i / (pts.size() - 1));
            tube(pts, rr, M_BARK, 6, false);
        }
        int nl = (int)rng.range(38, 80);
        float cr = rng.range(0.22f, 0.36f);
        for (int l = 0; l < nl; l++) {
            V3 q;
            do { q = V3(rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1)); } while (dot(q, q) > 1);
            V3 base = cc + q * cr;
            V3 ax = normalize(V3(rng.range(-1, 1), rng.range(-0.7f, 0.3f), rng.range(-1, 1)));
            V3 nn = normalize(V3(rng.range(-0.6f, 0.6f), 1.0f, rng.range(-0.6f, 0.6f)));
            addLeaf(base, ax, nn, rng.range(0.085f, 0.13f), rng.range(0.045f, 0.065f), M_LEAF, rng, false);
        }
    }
}

static void addMapleLeaf(V3 base, V3 axis, V3 nrm, float size, RNG& rng) {
    V3 a = normalize(axis); V3 sd = normalize(cross(nrm, a)); V3 n = cross(a, sd);
    float rnd = rng.f(); float droop = rng.range(0.0f, 0.35f);
    static const float ang[5] = {-1.25f, -0.62f, 0.0f, 0.62f, 1.25f}, len[5] = {0.62f, 0.9f, 1.0f, 0.9f, 0.62f};
    Xf save = CUR; CUR = Xf();
    for (int k = 0; k < 5; k++) {
        V3 d = a * cosf(ang[k]) + sd * sinf(ang[k]); V3 pd = cross(n, d);
        float L = size * len[k], w = L * 0.20f;
        V3 tip = base + d * L - n * (droop * L * 0.5f), mid = base + d * (L * 0.42f) - n * (droop * L * 0.12f);
        uint32_t i0 = addVT(base, n, V3(0, 0, rnd)), i1 = addVT(mid + pd * w, n, V3(0.4f, 1, rnd)), i2 = addVT(tip, n, V3(1, 0, rnd)), i3 = addVT(mid - pd * w, n, V3(0.4f, -1, rnd));
        addQuadI(i0, i1, i2, i3, M_SHRUB);
    }
    CUR = save;
}
// an autumn maple in the garden, in the line of sight through the door (on the -z side: it never shades the door)
static void buildMaple(uint32_t seed) {
    RNG rng(seed, 31);
    CUR = Xf(); TEXWORLD = true;
    V3 base(-3.10f, -0.12f, -4.35f), fork(-2.95f, 0.90f, -4.15f);
    TEXWORLD = false;
    {
        std::vector<V3> tp = crRefine(std::vector<V3>{base, V3(-3.02f, 0.3f, -4.30f), V3(-3.03f, 0.62f, -4.2f), fork}, 4);
        std::vector<float> tr(tp.size());
        for (size_t i = 0; i < tp.size(); i++) tr[i] = lerp(0.066f, 0.046f, (float)i / (tp.size() - 1)) * (1.0f + 0.06f * pnoise(V3(i * 0.9f, 0, 3)));
        tube(tp, tr, M_BARK, 14, false);
    }
    // main limbs
    const int NL = 7;
    std::vector<std::vector<V3>> limbs; std::vector<std::vector<float>> lrad;
    for (int l = 0; l < NL; l++) {
        float a = 2 * PI * (l + rng.range(-0.3f, 0.3f)) / NL;
        V3 d = normalize(V3(cosf(a), rng.range(0.45f, 1.0f), sinf(a)));
        float len = rng.range(1.2f, 1.9f);
        V3 p1 = fork + d * (len * 0.45f) + V3(rng.range(-0.12f, 0.12f), rng.range(0.0f, 0.15f), rng.range(-0.12f, 0.12f));
        V3 d2 = normalize(V3(d.x, d.y * 0.45f, d.z));
        V3 p2 = p1 + d2 * (len * 0.55f) + V3(rng.range(-0.15f, 0.15f), rng.range(-0.05f, 0.2f), rng.range(-0.15f, 0.15f));
        std::vector<V3> pts = crRefine(std::vector<V3>{fork, p1, p2}, 8);
        std::vector<float> rr(pts.size());
        for (size_t i = 0; i < pts.size(); i++) rr[i] = lerp(0.036f, 0.010f, (float)i / (pts.size() - 1));
        tube(pts, rr, M_BARK, 8, false);
        limbs.push_back(pts); lrad.push_back(rr);
    }
    for (int sp = 0; sp < 105; sp++) {
        int l = (int)(rng.f() * NL) % NL;
        const std::vector<V3>& L = limbs[l];
        size_t i0 = (size_t)(rng.range(0.30f, 1.0f) * (L.size() - 1));
        V3 start = L[i0];
        V3 ld = normalize(L[std::min(i0 + 1, L.size() - 1)] - L[i0 > 0 ? i0 - 1 : 0]);
        V3 off = normalize(ld * 0.7f + V3(rng.range(-1, 1), rng.range(-0.25f, 0.55f), rng.range(-1, 1)));
        V3 cc = start + off * rng.range(0.25f, 0.85f);
        if (cc.y < 0.5f) cc.y = 0.5f + rng.range(0, 0.3f);
        V3 mid = lerp(start, cc, 0.5f) + V3(rng.range(-0.06f, 0.06f), rng.range(0.0f, 0.08f), rng.range(-0.06f, 0.06f));
        std::vector<V3> pts = crRefine(std::vector<V3>{start, mid, cc}, 4);
        std::vector<float> rr(pts.size());
        for (size_t i = 0; i < pts.size(); i++) rr[i] = lerp(std::min(0.012f, lrad[l][i0] * 0.7f), 0.003f, (float)i / (pts.size() - 1));
        tube(pts, rr, M_BARK, 6, false);
        V3 tn = normalize(V3(rng.range(-0.25f, 0.25f), 1.0f, rng.range(-0.25f, 0.25f))), tu, tv; onb(tn, tu, tv);
        float R = rng.range(0.26f, 0.46f); int nl = (int)rng.range(24, 46);
        V3 out = normalize(cc - start);
        for (int k = 0; k < nl; k++) {
            float a = rng.range(0, 2 * PI), r = R * sqrtf(rng.f());
            V3 p = cc + tu * (r * cosf(a)) + tv * (r * sinf(a)) + tn * rng.range(-0.05f, 0.05f);
            V3 ax = normalize(out * 0.8f + tu * cosf(a) * 0.8f + tv * sinf(a) * 0.8f + V3(0, rng.range(-0.5f, 0.1f), 0));
            V3 nn = normalize(tn + V3(rng.range(-0.45f, 0.45f), 0, rng.range(-0.45f, 0.45f)));
            addMapleLeaf(p, ax, nn, rng.range(0.05f, 0.075f), rng);
        }
    }
}

static void buildExterior() {
    CUR = Xf(); TEXWORLD = true;
    quad(V3(-70, -0.12f, -70), V3(-WT, -0.12f, -70), V3(-WT, -0.12f, 70), V3(-70, -0.12f, 70), M_GROUND);
    quad(V3(-WT, -0.12f, -70), V3(40, -0.12f, -70), V3(40, -0.12f, -0.4f), V3(-WT, -0.12f, -0.4f), M_GROUND);
    // hedge and far tree line, seen (out of focus) through the panes
    for (float x = -40; x < 6; x += 0.12f) {
        auto top = [](float xx) { return 2.42f + 0.07f * fbm(V3(xx * 2.2f, 0.3f, 1.1f), 3) + 0.03f * pnoise(V3(xx * 11.0f, 0, 0)); };
        quad(V3(x, -0.12f, -9.5f), V3(x + 0.12f, -0.12f, -9.5f), V3(x + 0.12f, top(x + 0.12f), -9.5f), V3(x, top(x), -9.5f), M_HEDGE);
    }
    for (float x = -90; x < 30; x += 0.4f) {
        auto top = [](float xx) { return 4.6f + 2.6f * fbm(V3(xx * 0.11f, 2.3f, 0.7f), 4) + 0.5f * fbm(V3(xx * 0.9f, 0, 5.0f), 3); };
        quad(V3(x, -0.12f, -34), V3(x + 0.4f, -0.12f, -34), V3(x + 0.4f, top(x + 0.4f), -34), V3(x, top(x), -34), M_FARTREES);
    }
    buildMaple(21);
    {   // fallen leaves on the terrace and lawn
        RNG rng(77, 3);
        for (int i = 0; i < 520; i++) {
            float a = rng.range(0, 2 * PI), r = 0.2f + 3.6f * powf(rng.f(), 0.75f);
            V3 p(-3.0f + r * cosf(a), -0.12f + 0.004f + 0.006f * rng.f(), -4.2f + r * sinf(a) * 1.15f);
            if (p.x > -0.45f) continue;
            float b = rng.range(0, 2 * PI);
            addMapleLeaf(p, V3(cosf(b), rng.range(-0.02f, 0.12f), sinf(b)), normalize(V3(rng.range(-0.15f, 0.15f), 1, rng.range(-0.15f, 0.15f))), rng.range(0.05f, 0.075f), rng);
        }
    }
}

// ----- furniture & still life -----
static void buildTable(V3 pos, RNG& rng) {
    CUR = xfT(pos);
    setTexWood(0, rng);
    lathe({V3(0, 0.498f, 0), V3(0.205f, 0.498f, 0), V3(0.232f, 0.500f, 0), V3(0.2405f, 0.506f, 0), V3(0.2415f, 0.512f, 0), V3(0.2395f, 0.518f, 0),
           V3(0.233f, 0.520f, 0), V3(0.12f, 0.520f, 0), V3(0, 0.520f, 0)}, M_TABLE, 128, 3);
    for (int k = 0; k < 3; k++) {
        float a = rad(90.0f + 120.0f * k + 14.0f);
        V3 d(cosf(a), 0, sinf(a));
        tube({d * 0.215f, d * 0.165f + V3(0, 0.25f, 0), d * 0.115f + V3(0, 0.499f, 0)}, {0.0095f, 0.013f, 0.0165f}, M_TABLE, 16, true, V3(rng.range(0, 9), 0, 0));
    }
    // small under-shelf ring
    lathe({V3(0, 0.205f, 0), V3(0.160f, 0.205f, 0), V3(0.166f, 0.211f, 0), V3(0.160f, 0.217f, 0), V3(0, 0.217f, 0)}, M_TABLE, 64, 2);
}

static void buildPersimmon(V3 c, float R, V3 up, RNG& rng) {
    V3 t, b; onb(up, t, b);
    Xf save = CUR; Xf loc; loc.X = t; loc.Y = up; loc.Z = b; loc.T = c;
    CUR = CUR * loc * xfRY(rng.range(0, 6.28f));
    size_t v0 = G.P.size(), t0 = G.tris.size();
    const int NA = 40, NB = 24;
    std::vector<uint32_t> id((NA) * (NB + 1));
    for (int j = 0; j <= NB; j++)
        for (int i = 0; i < NA; i++) {
            float th = PI * j / NB, ph = 2 * PI * i / NA;
            float st = sinf(th), ct = cosf(th);
            float lobes = 1.0f + 0.035f * cosf(4 * ph) * st * st;
            float rr = R * lobes * st;
            float y = R * 0.80f * ct;
            y -= R * 0.10f * powf(std::max(0.f, ct), 6.0f); // dimple under the calyx
            y += R * 0.06f * powf(std::max(0.f, -ct), 8.0f);
            // slightly squarer shoulders
            float sq = powf(st, 0.85f);
            V3 p(rr / std::max(st, 1e-4f) * sq * cosf(ph), y, rr / std::max(st, 1e-4f) * sq * sinf(ph));
            id[j * NA + i] = addVT(p, V3(0, 1, 0), V3(p.x / R, ct, p.z / R) + V3(c.x * 31, 0, c.z * 17));
        }
    for (int j = 0; j < NB; j++)
        for (int i = 0; i < NA; i++) {
            int i2 = (i + 1) % NA;
            addQuadI(id[j * NA + i], id[j * NA + i2], id[(j + 1) * NA + i2], id[(j + 1) * NA + i], M_FRUIT);
        }
    recomputeNormals(v0, t0);
    // calyx: four small sepals
    for (int k = 0; k < 4; k++) {
        float a = PI / 4 + k * PI / 2 + rng.range(-0.15f, 0.15f);
        V3 d(cosf(a), 0, sinf(a));
        V3 base = V3(0, R * 0.715f, 0) + d * (R * 0.06f);
        V3 tip = d * (R * 0.62f) + V3(0, R * (0.72f + rng.range(0.0f, 0.10f)), 0);
        V3 sd(-d.z, 0, d.x);
        V3 n = normalize(cross(sd, tip - base));
        V3 mid = lerp(base, tip, 0.55f) + V3(0, R * 0.05f, 0);
        uint32_t i0 = addVT(base - sd * (R * 0.12f), n, V3(0)), i1 = addVT(base + sd * (R * 0.12f), n, V3(0)),
                 i2 = addVT(mid + sd * (R * 0.26f), n, V3(0)), i3 = addVT(tip, n, V3(0)), i4 = addVT(mid - sd * (R * 0.26f), n, V3(0));
        addTri(i0, i1, i2, M_CALYX); addTri(i0, i2, i4, M_CALYX); addTri(i4, i2, i3, M_CALYX);
    }
    cyl(V3(0, R * 0.70f, 0), V3(0, R * 0.78f, 0), R * 0.22f, R * 0.10f, M_CALYX, 10);
    CUR = save;
}

// angular persimmon twigs; returns nothing, adds fruit at some tips
static void growTwig(V3 p, V3 dir, float len, float r0, int depth, RNG& rng, bool fruit) {
    int nseg = std::max(3, (int)(len / 0.04f));
    std::vector<V3> pts; std::vector<float> rr;
    V3 d = normalize(dir), q = p;
    for (int i = 0; i <= nseg; i++) {
        pts.push_back(q); rr.push_back(r0 * (1.0f - 0.5f * i / nseg));
        if (i == nseg) break;
        d = normalize(d + V3(rng.range(-1, 1), rng.range(-0.5f, 0.9f), rng.range(-1, 1)) * 0.17f);
        q = q + d * (len / nseg);
        if (depth < 2 && i >= 1 && rng.f() < (depth == 0 ? 0.55f : 0.35f)) {
            V3 t, b; onb(d, t, b); float a = rng.range(0, 2 * PI);
            V3 cd = normalize(d * 0.75f + (t * cosf(a) + b * sinf(a)) * rng.range(0.55f, 0.95f) + V3(0, 0.15f, 0));
            growTwig(q, cd, len * rng.range(0.30f, 0.52f), rr.back() * 0.62f, depth + 1, rng, rng.f() < 0.62f);
        }
    }
    tube(pts, rr, M_TWIG, 8, true);
    V3 tip = pts.back();
    if (fruit) {
        float R = rng.range(0.027f, 0.033f);
        V3 up = normalize(V3(rng.range(-0.25f, 0.25f), 1, rng.range(-0.25f, 0.25f)));
        V3 fc = tip - up * (R * 0.80f + 0.012f);
        cyl(tip, fc + up * (R * 0.76f), rr.back(), 0.0022f, M_TWIG, 6);
        buildPersimmon(fc, R, up, rng);
    } else if (depth >= 1 && rng.f() < 0.5f) {
        V3 ax = normalize(V3(d.x, -0.9f, d.z) + V3(rng.range(-0.3f, 0.3f), 0, rng.range(-0.3f, 0.3f)));
        V3 nn = normalize(V3(rng.range(-1, 1), 0.3f, rng.range(-1, 1)));
        Xf save = CUR;
        V3 wb = CUR.pt(tip);
        addLeaf(wb, CUR.vec(ax), CUR.vec(nn), rng.range(0.06f, 0.085f), rng.range(0.035f, 0.045f), M_DRYLEAF, rng, true);
        CUR = save;
    }
}

static void buildStillLife(V3 tablePos, uint32_t seed) {
    RNG rng(seed, 5);
    float ty = 0.52f;
    // vase
    V3 vpos = tablePos + V3(-0.055f, ty, -0.045f);
    CUR = xfT(vpos); setTexLocal();
    lathe({V3(0, 0.0015f, 0), V3(0.040f, 0.0f, 0), V3(0.047f, 0.004f, 0), V3(0.060f, 0.04f, 0), V3(0.073f, 0.085f, 0), V3(0.076f, 0.12f, 0),
           V3(0.066f, 0.165f, 0), V3(0.044f, 0.20f, 0), V3(0.027f, 0.225f, 0), V3(0.022f, 0.245f, 0), V3(0.026f, 0.262f, 0), V3(0.0305f, 0.270f, 0),
           V3(0.0285f, 0.273f, 0), V3(0.024f, 0.268f, 0), V3(0.019f, 0.25f, 0), V3(0.019f, 0.20f, 0)}, M_VASE, 128, 6);
    // persimmon branch: hand-placed main stem, procedural twigs
    {
        std::vector<V3> stem = {V3(0.004f, 0.10f, 0.0f), V3(0.010f, 0.27f, 0.004f), V3(0.035f, 0.42f, 0.020f), V3(0.10f, 0.56f, 0.035f),
                                V3(0.19f, 0.66f, 0.030f), V3(0.30f, 0.725f, 0.010f), V3(0.40f, 0.745f, -0.012f)};
        std::vector<V3> pts = crRefine(stem, 5);
        std::vector<float> rr(pts.size());
        for (size_t i = 0; i < pts.size(); i++) rr[i] = lerp(0.0062f, 0.0026f, (float)i / (pts.size() - 1));
        // small angular kinks
        for (size_t i = 5; i + 1 < pts.size(); i++) pts[i] += V3(rng.range(-1, 1), rng.range(-1, 1), rng.range(-1, 1)) * 0.004f;
        tube(pts, rr, M_TWIG, 10, true);
        struct Child { float t; V3 dir; float len; bool fruit; };
        std::vector<Child> ch = {
            {0.34f, V3(-0.75f, 0.65f, 0.25f), 0.26f, true},
            {0.48f, V3(0.25f, 0.95f, -0.15f), 0.20f, false},
            {0.58f, V3(-0.35f, 0.75f, 0.45f), 0.17f, true},
            {0.70f, V3(0.55f, 0.80f, 0.20f), 0.17f, true},
            {0.84f, V3(0.30f, 0.85f, -0.35f), 0.13f, false},
            {1.00f, V3(0.95f, 0.10f, -0.05f), 0.10f, true},
            {0.26f, V3(0.70f, 0.60f, 0.30f), 0.12f, false},
        };
        for (auto& c : ch) {
            size_t i = std::min(pts.size() - 1, (size_t)(c.t * (pts.size() - 1)));
            growTwig(pts[i], c.dir, c.len, rr[i] * 0.72f, 1, rng, c.fruit);
        }
        // a second, shorter stem leaning the other way
        std::vector<V3> s2 = crRefine(std::vector<V3>{V3(-0.004f, 0.10f, 0.002f), V3(-0.012f, 0.27f, 0.0f), V3(-0.05f, 0.40f, -0.01f), V3(-0.11f, 0.49f, 0.015f)}, 5);
        std::vector<float> r2(s2.size());
        for (size_t i = 0; i < s2.size(); i++) r2[i] = lerp(0.0045f, 0.0022f, (float)i / (s2.size() - 1));
        tube(s2, r2, M_TWIG, 8, true);
        growTwig(s2.back(), V3(-0.5f, 0.6f, 0.3f), 0.08f, 0.002f, 2, rng, true);
        growTwig(s2[s2.size() * 2 / 3], V3(0.3f, 0.9f, 0.3f), 0.09f, 0.002f, 2, rng, false);
    }
    // teapot
    {
        CUR = xfT(tablePos + V3(0.105f, ty, 0.045f)) * xfRY(rad(205)); setTexLocal(V3(3, 0, 0));
        lathe({V3(0, 0.001f, 0), V3(0.030f, 0.0f, 0), V3(0.036f, 0.003f, 0), V3(0.049f, 0.016f, 0), V3(0.0555f, 0.034f, 0), V3(0.052f, 0.051f, 0),
               V3(0.041f, 0.062f, 0), V3(0.031f, 0.066f, 0), V3(0.0295f, 0.068f, 0)}, M_TEAPOT, 96, 6);
        lathe({V3(0.0300f, 0.0665f, 0), V3(0.0295f, 0.0695f, 0), V3(0.022f, 0.0735f, 0), V3(0.010f, 0.0755f, 0), V3(0.0055f, 0.0765f, 0),
               V3(0.0085f, 0.0815f, 0), V3(0.0075f, 0.087f, 0), V3(0.0f, 0.0895f, 0)}, M_TEAPOT, 64, 5);
        std::vector<V3> sp = crRefine(std::vector<V3>{V3(0.046f, 0.028f, 0), V3(0.064f, 0.036f, 0), V3(0.078f, 0.052f, 0), V3(0.088f, 0.066f, 0)}, 6);
        std::vector<float> sr(sp.size());
        for (size_t i = 0; i < sp.size(); i++) sr[i] = lerp(0.0115f, 0.0048f, powf((float)i / (sp.size() - 1), 0.8f));
        tube(sp, sr, M_TEAPOT, 16, true);
        std::vector<V3> hd = crRefine(std::vector<V3>{V3(-0.047f, 0.054f, 0), V3(-0.066f, 0.060f, 0), V3(-0.084f, 0.050f, 0), V3(-0.087f, 0.034f, 0),
                                                      V3(-0.074f, 0.020f, 0), V3(-0.052f, 0.016f, 0)}, 6);
        tube(hd, std::vector<float>(hd.size(), 0.0042f), M_TEAPOT, 12, true);
    }
    // cup with tea
    {
        CUR = xfT(tablePos + V3(-0.005f, ty, 0.135f)); setTexLocal();
        lathe({V3(0, 0.0005f, 0), V3(0.015f, 0.0f, 0), V3(0.0175f, 0.003f, 0), V3(0.0265f, 0.014f, 0), V3(0.0305f, 0.030f, 0), V3(0.0318f, 0.0395f, 0),
               V3(0.0310f, 0.0405f, 0), V3(0.0300f, 0.0395f, 0), V3(0.0285f, 0.029f, 0), V3(0.0235f, 0.014f, 0), V3(0.012f, 0.008f, 0), V3(0.0f, 0.0075f, 0)},
              M_CUP, 96, 6);
        lathe({V3(0, 0.0325f, 0), V3(0.0291f, 0.0325f, 0)}, M_TEA, 64, 1);
    }
}

static void buildChair(Xf at, RNG& rng) {
    CUR = at;
    // seat
    setTexWood(0, rng);
    { Xf s = CUR; CUR = CUR * xfT(0, 0.445f, 0); rbox(V3(0.215f, 0.014f, 0.205f), 0.007f, M_CHAIR); CUR = s; }
    for (int sx = -1; sx <= 1; sx += 2) {
        tube({V3(sx * 0.188f, 0, 0.178f), V3(sx * 0.183f, 0.25f, 0.174f), V3(sx * 0.18f, 0.432f, 0.17f)}, {0.0135f, 0.0165f, 0.0175f}, M_CHAIR, 16, true, V3(rng.range(0, 9), 0, 0));
        std::vector<V3> post = crRefine(std::vector<V3>{V3(sx * 0.188f, 0, -0.192f), V3(sx * 0.182f, 0.44f, -0.176f), V3(sx * 0.177f, 0.70f, -0.214f), V3(sx * 0.172f, 0.905f, -0.268f)}, 6);
        std::vector<float> pr(post.size());
        for (size_t i = 0; i < post.size(); i++) { float t = (float)i / (post.size() - 1); pr[i] = lerp(0.0135f, 0.0175f, sat(t * 2.2f)) * lerp(1.0f, 0.72f, smoothstep(0.5f, 1.0f, t)); }
        tube(post, pr, M_CHAIR, 16, true, V3(rng.range(0, 9), 0, 0));
        cyl(V3(sx * 0.185f, 0.20f, 0.174f), V3(sx * 0.185f, 0.22f, -0.186f), 0.0085f, 0.0085f, M_CHAIR, 12, false, V3(rng.range(0, 9), 0, 0));
    }
    cyl(V3(-0.183f, 0.29f, 0.173f), V3(0.183f, 0.29f, 0.173f), 0.0085f, 0.0085f, M_CHAIR, 12, false, V3(rng.range(0, 9), 0, 0));
    cyl(V3(-0.185f, 0.31f, -0.184f), V3(0.185f, 0.31f, -0.184f), 0.0085f, 0.0085f, M_CHAIR, 12, false, V3(rng.range(0, 9), 0, 0));
    // seat rails
    for (int sx = -1; sx <= 1; sx += 2) { Xf s = CUR; CUR = CUR * xfT(sx * 0.19f, 0.405f, 0); setTexWood(2, rng); rbox(V3(0.009f, 0.026f, 0.175f), 0.003f, M_CHAIR); CUR = s; }
    for (int sz = -1; sz <= 1; sz += 2) { Xf s = CUR; CUR = CUR * xfT(0, 0.405f, sz * 0.178f); setTexWood(0, rng); rbox(V3(0.175f, 0.026f, 0.009f), 0.003f, M_CHAIR); CUR = s; }
    // two bowed back slats
    for (int k = 0; k < 2; k++) {
        float y = k ? 0.815f : 0.655f, z = k ? -0.243f : -0.206f, hh = k ? 0.042f : 0.032f;
        Xf s = CUR; CUR = CUR * xfT(0, y, z) * xfRX(rad(-13));
        setTexWood(0, rng);
        const float half = 0.176f, bow = 0.03f;
        DEFORM = [=](V3& p, V3& n) {
            float sx = p.x / half; p.z -= bow * (1 - sx * sx);
            float slope = 2 * bow * sx / half; float a = atanf(slope);
            float c = cosf(a), sn = sinf(a);
            n = V3(n.x * c - n.z * sn, n.y, n.x * sn + n.z * c);
        };
        rbox(V3(half, hh, 0.0055f), 0.004f, M_CHAIR, 3);
        DEFORM = nullptr; CUR = s;
    }
}

static void buildHat(Xf at) {
    CUR = at; setTexLocal();
    // crown + brim as one lathe; the brim waves gently and turns up a little at the edge
    DEFORM = [](V3& p, V3& n) {
        float r = sqrtf(p.x * p.x + p.z * p.z); float a = atan2f(p.z, p.x);
        float w = smoothstep(0.095f, 0.2f, r);
        p.y += w * (0.006f * sinf(2 * a + 0.6f) + 0.003f * sinf(3 * a + 2.0f)) + 0.012f * w * w + 0.004f * smoothstep(0.09f, 0.12f, r);
        // slight pinch of the crown
        if (p.y > 0.05f && r < 0.09f) p.y -= 0.006f * (1.0f - smoothstep(0.0f, 0.06f, fabsf(p.z))) * smoothstep(0.05f, 0.1f, p.y);
        (void)n;
    };
    lathe({V3(0.0f, 0.108f, 0), V3(0.035f, 0.107f, 0), V3(0.060f, 0.099f, 0), V3(0.077f, 0.080f, 0), V3(0.085f, 0.05f, 0), V3(0.088f, 0.02f, 0), V3(0.089f, 0.004f, 0),
           V3(0.096f, 0.0012f, 0), V3(0.13f, 0.0012f, 0), V3(0.17f, 0.0012f, 0), V3(0.203f, 0.0012f, 0), V3(0.2055f, 0.0f, 0), V3(0.203f, -0.0012f, 0),
           V3(0.13f, -0.0012f, 0), V3(0.092f, -0.0012f, 0)}, M_HAT, 128, 5);
    DEFORM = nullptr;
    // ribbon
    lathe({V3(0.0900f, 0.010f, 0), V3(0.0893f, 0.022f, 0), V3(0.0878f, 0.036f, 0)}, M_RIBBON, 96, 1);
}

static void buildBook(Xf at, V3 size, int coverMat) { // size = (width, thickness, length); spine on -x side
    Xf save = CUR; CUR = at; setTexLocal(V3(at.T.x * 3, 0, at.T.z * 5));
    float w = size.x * 0.5f, t = size.y * 0.5f, l = size.z * 0.5f; const float ct = 0.0022f;
    Xf s = CUR;
    CUR = s * xfT(0, ct, 0); rbox(V3(w, ct, l), 0.0012f, coverMat, 2);
    CUR = s * xfT(0, 2 * t - ct, 0); rbox(V3(w, ct, l), 0.0012f, coverMat, 2);
    CUR = s * xfT(-w + ct, t, 0); rbox(V3(ct, t, l), 0.002f, coverMat, 2);
    CUR = s * xfT(0.002f, t, 0); TEX = xfT(V3(at.T.x * 7, 0, 0));
    rbox(V3(w - 0.004f, t - 2 * ct + 0.0003f, l - 0.004f), 0.0006f, M_PAGES, 1);
    CUR = save;
}

static void buildPicture(V3 c, float w, float h, RNG& rng) { // hangs on the back wall, c = centre on wall
    CUR = xfT(c);
    const float fw = 0.018f, fd = 0.028f;
    for (int k = 0; k < 2; k++) {
        float sgn = k ? 1.f : -1.f;
        { Xf s = CUR; CUR = CUR * xfT(sgn * (w * 0.5f - fw * 0.5f), 0, fd * 0.5f); setTexWood(1, rng); rbox(V3(fw * 0.5f, h * 0.5f, fd * 0.5f), 0.003f, M_FRAME); CUR = s; }
        { Xf s = CUR; CUR = CUR * xfT(0, sgn * (h * 0.5f - fw * 0.5f), fd * 0.5f); setTexWood(0, rng); rbox(V3(w * 0.5f - fw, fw * 0.5f, fd * 0.5f), 0.003f, M_FRAME); CUR = s; }
    }
    float iw = w * 0.5f - fw, ih = h * 0.5f - fw; float z = 0.010f;
    // mat board with an opening; artwork slightly recessed
    float aw = iw * 0.56f, ah = ih * 0.56f, ay = 0.012f;
    auto mq = [&](float x0, float y0, float x1, float y1) { quad(V3(x0, y0, z), V3(x1, y0, z), V3(x1, y1, z), V3(x0, y1, z), M_MATBOARD); };
    TEXWORLD = true;
    mq(-iw, -ih, iw, ay - ah); mq(-iw, ay + ah, iw, ih); mq(-iw, ay - ah, -aw, ay + ah); mq(aw, ay - ah, iw, ay + ah);
    TEXWORLD = false;
    float za = z - 0.0025f;
    quadT(V3(-aw, ay - ah, za), V3(aw, ay - ah, za), V3(aw, ay + ah, za), V3(-aw, ay + ah, za), V3(0, 0, 0), V3(1, 0, 0), V3(1, 1, 0), V3(0, 1, 0), M_ART);
    // bevel of the mat opening
    quad(V3(-aw, ay - ah, za), V3(aw, ay - ah, za), V3(aw, ay - ah, z), V3(-aw, ay - ah, z), M_MATBOARD);
    quad(V3(-aw, ay + ah, za), V3(aw, ay + ah, za), V3(aw, ay + ah, z), V3(-aw, ay + ah, z), M_MATBOARD);
    quad(V3(-aw, ay - ah, za), V3(-aw, ay + ah, za), V3(-aw, ay + ah, z), V3(-aw, ay - ah, z), M_MATBOARD);
    quad(V3(aw, ay - ah, za), V3(aw, ay + ah, za), V3(aw, ay + ah, z), V3(aw, ay - ah, z), M_MATBOARD);
}

struct SceneCfg {
    float sunAz = 30.0f, sunEl = 24.0f;
    uint32_t treeSeed = 11, lifeSeed = 3;
};
static SceneCfg CFG;

static void buildScene() {
    initMaterials();
    initSun(CFG.sunAz, CFG.sunEl);
    RNG rng(2024, 9);
    buildRoom();
    buildDoor();
    buildCurtain();
    buildExterior();
    buildTree(CFG.treeSeed);
    V3 tablePos(0.80f, 0, 0.47f);
    buildTable(tablePos, rng);
    buildStillLife(tablePos, CFG.lifeSeed);
    Xf chairXf = xfT(1.78f, 0, 0.66f) * xfRY(rad(-32));
    buildChair(chairXf, rng);
    buildHat(chairXf * xfT(0.005f, 0.4612f, 0.0f) * xfRY(rad(40)));
    // books on the floor, leaning stack near the wall
    {
        Xf b = xfT(2.52f, 0, 0.30f) * xfRY(rad(8));
        buildBook(b, V3(0.17f, 0.030f, 0.24f), M_BOOK1);
        buildBook(b * xfT(0.006f, 0.030f, -0.004f) * xfRY(rad(-11)), V3(0.155f, 0.022f, 0.225f), M_BOOK3);
        buildBook(b * xfT(-0.004f, 0.052f, 0.006f) * xfRY(rad(17)), V3(0.135f, 0.018f, 0.20f), M_BOOK2);
    }
    buildPicture(V3(2.98f, 1.56f, 0), 0.44f, 0.58f, rng);
    CUR = Xf(); TEXWORLD = false; DEFORM = nullptr;
}
