// Host harness for gl/glsl/cache.cpp -- links the real translation unit, not a
// copy. Runs in four modes because the cache singleton is process-lifetime and a
// genuine load happens only in the constructor:
//
//   cache_test seed    fresh-file cache; put 16 entries to force a save(); the
//                      destructor re-saves the replacement, so the file left on
//                      disk is the map exactly as it stands at exit.
//   cache_test load    fresh process; the file from "seed" is read by the real
//                      load() before the first use, proving the format round
//                      trips across processes.
//   cache_test crafted file written by hand in the current format; load() must
//                      hit both entries, which also locks the key encoding used
//                      on disk to the two seeded XXH64 lanes.
//   cache_test garbage a file that is neither the current format nor empty; the
//                      build must start with an empty cache and keep working.
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>

#include "gl/glsl/cache.h"
#include "config/config.h"
#include "config/settings.h"
#include "xxhash64.h"

// --- the externals cache.cpp reaches for ------------------------------------
global_settings_t global_settings{};
char* glsl_cache_file_path = nullptr;
int __android_log_print(int, const char*, const char*, ...) { return 0; }
extern "C" void write_log(const char*, ...) {}
extern "C" int mg_debug_enabled(const char*) { return 0; }

static int fails = 0;
static void expect(const char* what, bool cond) {
    if (!cond) { printf("  FAIL %s\n", what); ++fails; }
}

// The two halves of the on-disk key are separate seeded passes of the same
// XXH64; duplicates of the constants in cache.cpp, which is exactly what a
// hand-written file has to stay in step with.
static const uint64_t kSeedLo = 0x9E3779B97F4A7C15ULL;
static const uint64_t kSeedHi = 0xC2B2AE3D27D4EB4FULL;

static void write_entry(std::ofstream& f, const char* src, const char* essl) {
    size_t len = strlen(src);
    uint64_t lo = XXHash64::hash(src, len, kSeedLo);
    uint64_t hi = XXHash64::hash(src, len, kSeedHi);
    f.write(reinterpret_cast<const char*>(&lo), sizeof(lo));
    f.write(reinterpret_cast<const char*>(&hi), sizeof(hi));
    size_t es = strlen(essl) + 1;
    f.write(reinterpret_cast<const char*>(&es), sizeof(es));
    f.write(essl, (long)es);
}

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "seed";
    global_settings.max_glsl_cache_size = 1 << 20;

    const char* file = "/tmp/mg_cache_test.cache";
    glsl_cache_file_path = const_cast<char*>(file);

    if (strcmp(mode, "seed") == 0) {
        std::remove(file);
        Cache& c = Cache::get_instance();
        std::string srcs[16], essls[16];
        for (int i = 0; i < 16; ++i) {
            srcs[i] = "source " + std::to_string(i) + " -- " + std::to_string(i * 7);
            essls[i] = "essl for " + std::to_string(i);
            c.put(srcs[i].c_str(), essls[i].c_str());
        }
        c.put(srcs[1].c_str(), "essl for 1 replacement");
        printf("cache: in-memory after save trigger\n");
        expect("16th put saved", []{ std::ifstream in("/tmp/mg_cache_test.cache", std::ios::binary); return in.good(); }());
        expect("get k5", strcmp(c.get(srcs[5].c_str()), essls[5].c_str()) == 0);
        expect("get k1 replacement", strcmp(c.get(srcs[1].c_str()), "essl for 1 replacement") == 0);
        expect("get unknown is null", c.get("nope") == nullptr);
    } else if (strcmp(mode, "load") == 0) {
        Cache& c = Cache::get_instance();
        printf("cache: cold-start load hits\n");
        expect("get k3", strcmp(c.get("source 3 -- 21"), "essl for 3") == 0);
        expect("get k11", strcmp(c.get("source 11 -- 77"), "essl for 11") == 0);
        expect("get k1 replacement", strcmp(c.get("source 1 -- 7"), "essl for 1 replacement") == 0);
        expect("get unknown is null", c.get("nope") == nullptr);
    } else if (strcmp(mode, "crafted") == 0) {
        {
            std::ofstream f(file, std::ios::binary);
            f.write("MGCS", 4);
            uint32_t version = 1;
            f.write(reinterpret_cast<const char*>(&version), sizeof(version));
            size_t count = 2;
            f.write(reinterpret_cast<const char*>(&count), sizeof(count));
            write_entry(f, "craftedAA", "esslA");
            write_entry(f, "craftedBB", "esslB");
        }
        Cache& c = Cache::get_instance();
        printf("cache: hand-written current-format file\n");
        expect("get craftedAA", strcmp(c.get("craftedAA"), "esslA") == 0);
        expect("get craftedBB", strcmp(c.get("craftedBB"), "esslB") == 0);
        expect("get other is null", c.get("craftedCC") == nullptr);
    } else if (strcmp(mode, "garbage") == 0) {
        {
            std::ofstream f(file, std::ios::binary);
            const char* legacy = "legacy-era cache, keyed on a digest nobody can recompute here";
            f.write(legacy, (long)strlen(legacy));
        }
        Cache& c = Cache::get_instance();
        printf("cache: legacy file is ignored, cache still works\n");
        expect("get anything is null", c.get("whatever") == nullptr);
        c.put("x", "y");
        expect("put/get after legacy load", strcmp(c.get("x"), "y") == 0);
    } else {
        printf("unknown mode %s\n", mode);
        return 2;
    }

    if (fails) { printf("%d failure(s)\n", fails); return 1; }
    printf("cache: all passed\n");
    return 0;
}