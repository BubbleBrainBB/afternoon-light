// core.h — math, RNG, sampling, noise. Part of the "afternoon light" path tracer.
#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>
#include <thread>
#include <atomic>
#include <string>
#include <functional>

static const float PI = 3.14159265358979f;
static const float INV_PI = 0.31830988618f;

struct V3 {
    float x, y, z;
    V3() : x(0), y(0), z(0) {}
    explicit V3(float a) : x(a), y(a), z(a) {}
    V3(float a, float b, float c) : x(a), y(b), z(c) {}
    float operator[](int i) const { return (&x)[i]; }
    float& operator[](int i) { return (&x)[i]; }
};
static inline V3 operator+(V3 a, V3 b) { return V3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V3 operator-(V3 a, V3 b) { return V3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline V3 operator*(V3 a, V3 b) { return V3(a.x * b.x, a.y * b.y, a.z * b.z); }
static inline V3 operator*(V3 a, float s) { return V3(a.x * s, a.y * s, a.z * s); }
static inline V3 operator*(float s, V3 a) { return V3(a.x * s, a.y * s, a.z * s); }
static inline V3 operator/(V3 a, float s) { float i = 1.0f / s; return V3(a.x * i, a.y * i, a.z * i); }
static inline V3 operator-(V3 a) { return V3(-a.x, -a.y, -a.z); }
static inline V3& operator+=(V3& a, V3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
static inline V3& operator*=(V3& a, V3 b) { a.x *= b.x; a.y *= b.y; a.z *= b.z; return a; }
static inline V3& operator*=(V3& a, float s) { a.x *= s; a.y *= s; a.z *= s; return a; }
static inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 cross(V3 a, V3 b) { return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static inline float length(V3 a) { return sqrtf(dot(a, a)); }
static inline V3 normalize(V3 a) { float l = length(a); return l > 0 ? a / l : V3(0, 1, 0); }
static inline V3 lerp(V3 a, V3 b, float t) { return a + (b - a) * t; }
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float sat(float x) { return clampf(x, 0.f, 1.f); }
static inline float smoothstep(float a, float b, float x) { float t = sat((x - a) / (b - a)); return t * t * (3 - 2 * t); }
static inline float maxc(V3 a) { return std::max(a.x, std::max(a.y, a.z)); }
static inline float lum(V3 a) { return 0.2126f * a.x + 0.7152f * a.y + 0.0722f * a.z; }
static inline V3 vmin(V3 a, V3 b) { return V3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)); }
static inline V3 vmax(V3 a, V3 b) { return V3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)); }
static inline float sqr(float x) { return x * x; }

// affine transform
struct Xf {
    V3 X{1, 0, 0}, Y{0, 1, 0}, Z{0, 0, 1}, T{0, 0, 0};
    V3 pt(V3 p) const { return X * p.x + Y * p.y + Z * p.z + T; }
    V3 vec(V3 v) const { return X * v.x + Y * v.y + Z * v.z; }
};
static inline Xf operator*(const Xf& a, const Xf& b) { // a after b
    Xf r; r.X = a.vec(b.X); r.Y = a.vec(b.Y); r.Z = a.vec(b.Z); r.T = a.pt(b.T); return r;
}
static inline Xf xfT(V3 t) { Xf r; r.T = t; return r; }
static inline Xf xfT(float x, float y, float z) { return xfT(V3(x, y, z)); }
static inline Xf xfS(float s) { Xf r; r.X = V3(s, 0, 0); r.Y = V3(0, s, 0); r.Z = V3(0, 0, s); return r; }
static inline Xf xfRY(float a) { Xf r; float c = cosf(a), s = sinf(a); r.X = V3(c, 0, -s); r.Z = V3(s, 0, c); return r; }
static inline Xf xfRX(float a) { Xf r; float c = cosf(a), s = sinf(a); r.Y = V3(0, c, s); r.Z = V3(0, -s, c); return r; }
static inline Xf xfRZ(float a) { Xf r; float c = cosf(a), s = sinf(a); r.X = V3(c, s, 0); r.Y = V3(-s, c, 0); return r; }
static inline float rad(float deg) { return deg * PI / 180.0f; }

// orthonormal basis (Duff et al.)
static inline void onb(V3 n, V3& b1, V3& b2) {
    float sign = copysignf(1.0f, n.z);
    float a = -1.0f / (sign + n.z);
    float b = n.x * n.y * a;
    b1 = V3(1.0f + sign * n.x * n.x * a, sign * b, -sign * n.x);
    b2 = V3(b, sign + n.y * n.y * a, -n.y);
}

// ---------- hashing / RNG ----------
static inline uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x;
}
static inline uint32_t hash3i(int x, int y, int z) {
    return hash32((uint32_t)x * 73856093U ^ hash32((uint32_t)y * 19349663U ^ hash32((uint32_t)z * 83492791U + 0x9e3779b9U)));
}
static inline float hashf(uint32_t x) { return hash32(x) * (1.0f / 4294967296.0f); }

struct RNG {
    uint64_t state, inc;
    RNG(uint64_t seed = 1, uint64_t seq = 1) { state = 0; inc = (seq << 1) | 1; u32(); state += seed; u32(); }
    uint32_t u32() {
        uint64_t old = state; state = old * 6364136223846793005ULL + inc;
        uint32_t xs = (uint32_t)(((old >> 18u) ^ old) >> 27u); uint32_t rot = (uint32_t)(old >> 59u);
        return (xs >> rot) | (xs << ((-rot) & 31));
    }
    float f() { return (u32() >> 8) * (1.0f / 16777216.0f); }
    float range(float a, float b) { return a + (b - a) * f(); }
};

// Correlated multi-jittered sampling (Kensler 2013)
static inline uint32_t cmjPermute(uint32_t i, uint32_t l, uint32_t p) {
    uint32_t w = l - 1;
    w |= w >> 1; w |= w >> 2; w |= w >> 4; w |= w >> 8; w |= w >> 16;
    do {
        i ^= p; i *= 0xe170893d; i ^= p >> 16;
        i ^= (i & w) >> 4; i ^= p >> 8; i *= 0x0929eb3f;
        i ^= p >> 23; i ^= (i & w) >> 1; i *= 1 | p >> 27;
        i *= 0x6935fa69; i ^= (i & w) >> 11; i *= 0x74dcb303;
        i ^= (i & w) >> 2; i *= 0x9e501cc3; i ^= (i & w) >> 2; i *= 0xc860a3df;
        i &= w; i ^= i >> 5;
    } while (i >= l);
    return (i + p) % l;
}
static inline float cmjRand(uint32_t i, uint32_t p) {
    i ^= p; i ^= i >> 17; i ^= i >> 10; i *= 0xb36534e5; i ^= i >> 12; i ^= i >> 21; i *= 0x93fc4795;
    i ^= 0xdf6e307f; i ^= i >> 17; i *= 1 | p >> 18;
    return i * (1.0f / 4294967808.0f);
}
static inline void cmj(int s, int m, int n, uint32_t p, float& ox, float& oy) {
    s = cmjPermute(s, m * n, p * 0x51633e2d);
    int sx = cmjPermute(s % m, m, p * 0xa511e9b3);
    int sy = cmjPermute(s / m, n, p * 0x63d83595);
    float jx = cmjRand(s, p * 0xa399d265);
    float jy = cmjRand(s, p * 0x711ad6a5);
    ox = (s % m + (sy + jx) / n) / m;
    oy = (s / m + (sx + jy) / m) / n;
}

// ---------- sampling helpers ----------
static inline void concentricDisk(float u1, float u2, float& dx, float& dy) {
    float a = 2 * u1 - 1, b = 2 * u2 - 1;
    if (a == 0 && b == 0) { dx = dy = 0; return; }
    float r, phi;
    if (fabsf(a) > fabsf(b)) { r = a; phi = (PI / 4) * (b / a); }
    else { r = b; phi = PI / 2 - (PI / 4) * (a / b); }
    dx = r * cosf(phi); dy = r * sinf(phi);
}
static inline V3 cosHemi(V3 n, float u1, float u2) {
    float dx, dy; concentricDisk(u1, u2, dx, dy);
    float dz = sqrtf(std::max(0.f, 1 - dx * dx - dy * dy));
    V3 t, b; onb(n, t, b);
    return normalize(t * dx + b * dy + n * dz);
}

// ---------- Perlin noise ----------
static inline float pgrad(uint32_t h, float x, float y, float z) {
    switch (h & 15) {
        case 0: return x + y; case 1: return -x + y; case 2: return x - y; case 3: return -x - y;
        case 4: return x + z; case 5: return -x + z; case 6: return x - z; case 7: return -x - z;
        case 8: return y + z; case 9: return -y + z; case 10: return y - z; case 11: return -y - z;
        case 12: return y + x; case 13: return -y + z; case 14: return y - x; default: return -y - z;
    }
}
static inline float pnoise(V3 p) {
    float fx = floorf(p.x), fy = floorf(p.y), fz = floorf(p.z);
    int X = (int)fx, Y = (int)fy, Z = (int)fz;
    float x = p.x - fx, y = p.y - fy, z = p.z - fz;
    float u = x * x * x * (x * (x * 6 - 15) + 10), v = y * y * y * (y * (y * 6 - 15) + 10), w = z * z * z * (z * (z * 6 - 15) + 10);
#define PG(i, j, k) pgrad(hash3i(X + i, Y + j, Z + k), x - i, y - j, z - k)
    float a = lerp(lerp(lerp(PG(0, 0, 0), PG(1, 0, 0), u), lerp(PG(0, 1, 0), PG(1, 1, 0), u), v),
                   lerp(lerp(PG(0, 0, 1), PG(1, 0, 1), u), lerp(PG(0, 1, 1), PG(1, 1, 1), u), v), w);
#undef PG
    return a; // roughly [-1,1]
}
static inline float fbm(V3 p, int oct) {
    float s = 0, a = 0.5f;
    for (int i = 0; i < oct; i++) { s += a * pnoise(p); p = p * 2.03f + V3(17.13f, 5.71f, 9.37f); a *= 0.5f; }
    return s;
}
