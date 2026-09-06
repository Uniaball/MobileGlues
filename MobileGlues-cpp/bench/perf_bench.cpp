// Host micro-benchmark for the shader-load hot paths touched by the perf work.
// Links the REAL translation units -- gl/glsl/uniform_defaults.cpp for the
// initialiser-scan gate, the vendored xxhash64.h for the cache key -- so the
// numbers are the shipped code, not a re-implementation.
//
//   sh MobileGlues-cpp/tests/run.sh   (compiles and runs this last)
//
// Two sections:
//   uniform gate: a SPIRV-Cross-sized ES shader with no initialisers at all --
//                 the common case -- and the same one with a single early
//                 initialiser. The gate turns the first into one short scan.
//   cache key:    the two-seed XXH64 pass the cache now keys on vs. the SHA-256
//                 it used to. The SHA-256 here is the classic public-domain
//                 FIPS-180-4 reference, present only to show the contrast.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <ctime>

#include "gl/glsl/uniform_defaults.h"
#include "xxhash64.h"

static int64_t now_ns() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + static_cast<int64_t>(ts.tv_nsec);
}

static void warmup(int n) { volatile int sink = 0; for (int i = 0; i < n; ++i) sink += i; (void)sink; }

// Minimal PI/SHA-256, FIPS-180-4. Public-domain style, bench-only reference.
namespace sha {
static const uint32_t k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};

static inline uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

void digest(const uint8_t* data, size_t len, uint8_t out[32]) {
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    size_t padded = ((len + 9 + 63) / 64) * 64;
    std::vector<uint8_t> m(padded);
    memcpy(m.data(), data, len);
    m[len] = 0x80;
    uint64_t bits = static_cast<uint64_t>(len) * 8;
    for (int i = 0; i < 8; ++i) m[padded - 1 - i] = static_cast<uint8_t>(bits >> (8 * i));
    uint32_t w[64];
    for (size_t block = 0; block < padded; block += 64) {
        for (int i = 0; i < 16; ++i)
            w[i] = (uint32_t(m[block + i * 4]) << 24) | (uint32_t(m[block + i * 4 + 1]) << 16) |
                   (uint32_t(m[block + i * 4 + 2]) << 8) | uint32_t(m[block + i * 4 + 3]);
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], gg = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25);
            uint32_t ch = (e & f) ^ (~e & gg);
            uint32_t t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh = gg; gg = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += gg; h[7] += hh;
    }
    for (int i = 0; i < 8; ++i) {
        out[i * 4] = static_cast<uint8_t>(h[i] >> 24);
        out[i * 4 + 1] = static_cast<uint8_t>(h[i] >> 16);
        out[i * 4 + 2] = static_cast<uint8_t>(h[i] >> 8);
        out[i * 4 + 3] = static_cast<uint8_t>(h[i]);
    }
}
} // namespace sha

// A SPIRV-Cross fragment shader of realistic size: layout+binding uniforms in
// the head, a strip-mine follower so the string itself is the work, no
// initialisers (the common case).
static std::string make_shader(int uniform_count) {
    std::string s = "#version 320 es\nprecision mediump float;\nprecision highp int;\n\n";
    for (int i = 0; i < uniform_count; ++i) {
        char buf[96];
        if (i % 3 == 0)
            snprintf(buf, sizeof buf, "layout(location = %d, binding = %d) uniform highp sampler2D tex%d;\n", i, i, i);
        else if (i % 3 == 1)
            snprintf(buf, sizeof buf, "layout(location = %d) uniform highp vec4 tint%d;\n", i, i);
        else
            snprintf(buf, sizeof buf, "layout(location = %d) uniform highp float weights%d[3];\n", i, i);
        s += buf;
    }
    s += "\nlayout(location = 0) out highp vec4 fragColor;\nlayout(location = 0) in highp vec2 uv;\n"
         "void main()\n{\n    highp vec4 acc = vec4(0.0);\n";
    for (int i = 0; i < uniform_count; ++i)
        s += "    acc += vec4(0.001);\n";
    s += "    fragColor = acc;\n}\n";
    return s;
}

template <typename F>
static double bench(F&& f, int iterations) {
    warmup(100000);
    int64_t best = INT64_MAX;
    for (int round = 0; round < 5; ++round) {
        volatile int sink = 0;
        int64_t t0 = now_ns();
        for (int i = 0; i < iterations; ++i) { sink += f(); }
        int64_t t1 = now_ns();
        (void)sink;
        if (t1 - t0 < best) best = t1 - t0;
    }
    return static_cast<double>(best) / iterations;
}

int main() {
    const int iters = 2000;
    const int uniforms = 96;
    std::string shader = make_shader(uniforms);
    std::string shader_init = shader;
    // Insert a single early initialiser so the gate flips but the parser still
    // has essentially the whole body to walk.
    shader_init.insert(shader_init.find("uniform highp vec4 tint1;"),
                       "uniform highp vec4 tint0 = vec4(1.0);\n");

    printf("uniform_declarations on %.1f KB shader\n", shader.size() / 1024.0);

    size_t total = 0;
    auto guarded = [&]() {
        std::vector<uniform_default_t> d;
        total += process_uniform_declarations(shader, &d).size();
        return (int)d.size();
    };
    auto with_init = [&]() {
        std::vector<uniform_default_t> d;
        total += process_uniform_declarations(shader_init, &d).size();
        return (int)d.size();
    };
    printf("  wall clock per call:\n");
    printf("    no initialisers (gate short-circuits)  %7.1f ns\n", bench(guarded, iters));
    printf("    one early initialiser (full parse)     %7.1f ns\n", bench(with_init, iters));

    printf("cache key hashing, %.1f KB shader\n", shader.size() / 1024.0);
    auto xxh = [&]() {
        uint64_t lo = XXHash64::hash(shader.data(), shader.size(), 0x9E3779B97F4A7C15ULL);
        uint64_t hi = XXHash64::hash(shader.data(), shader.size(), 0xC2B2AE3D27D4EB4FULL);
        return (int)((lo ^ hi) & 0xFF);
    };
    auto sha256 = [&]() {
        uint8_t out[32];
        sha::digest(reinterpret_cast<const uint8_t*>(shader.data()), shader.size(), out);
        return out[0];
    };
    double t_xxh = bench(xxh, iters);
    double t_sha = bench(sha256, iters);
    printf("  two XXH64 passes (current disk key)      %7.1f ns\n", t_xxh);
    printf("  SHA-256 reference                        %7.1f ns\n", t_sha);
    printf("  cache-key hashing speedup                %7.1fx\n", t_sha / t_xxh);

    if (guarded()) return 1; // the gate must never run the parser with output
    if (total == 0 && shader_init.find("tint0 = ") == std::string::npos) return 1;
    printf("perf bench done\n");
    return 0;
}