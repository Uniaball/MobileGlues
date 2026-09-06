// MobileGlues - gl/glsl/cache.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "cache.h"
#include "xxhash64.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>

using namespace std;

namespace {
    // Two seeds far enough apart to be independent in practice: the two halves
    // of the key fall on different 64-bit hash lanes, so a collision needs both
    // XXH64 passes to meet on the same source, which is the same astronomically
    // low bar the 256-bit digest used to set.
    constexpr uint64_t kKeySeedHi = 0xC2B2AE3D27D4EB4FULL;
    constexpr uint64_t kKeySeedLo = 0x9E3779B97F4A7C15ULL;

    // First bytes of the on-disk format. Anything the other way is a pre-xxh3
    // cache whose entries were keyed on a digest that cannot be rediscovered
    // from the source alone; such a file is passed over and rewritten by the
    // next save(). SEE ALSO: load()'s comment.
    constexpr char kFormatMagic[4] = {'M', 'G', 'C', 'S'};
    constexpr uint32_t kFormatVersion = 1;
} // namespace

Cache::CacheKey Cache::computeKey(const uint8_t* data, size_t length) {
    CacheKey key{};
    const uint64_t lo = XXHash64::hash(data, length, kKeySeedLo);
    const uint64_t hi = XXHash64::hash(data, length, kKeySeedHi);
    memcpy(key.data(), &lo, sizeof(lo));
    memcpy(key.data() + sizeof(lo), &hi, sizeof(hi));
    return key;
}

size_t Cache::KeyHash::operator()(const CacheKey& key) const {
    // XXH64 lanes are already well mixed; whichever half lands on the first
    // bytes of the machine word hashes the map's buckets well enough, and this
    // stays endian-consistent because get/put/load all produce the same bytes.
    uint64_t half;
    memcpy(&half, key.data(), sizeof(half));
    return static_cast<size_t>(half);
}

// Persistence policy.
//
// put() used to call save() on every insert, and save() rewrites the whole file,
// so a shader pack loading hundreds of programs paid O(n^2) bytes of I/O for a
// file that is only ever read once, at startup. That is the stutter felt while a
// world loads. The on-disk format is one "count, then entries" blob with nowhere
// to append a single record, and it has to stay exactly that so caches written by
// older builds keep loading -- what a save costs therefore cannot change, only
// how often one happens.
//
// A save is deferred until 16 entries are pending or 5 seconds have passed since
// the last one, whichever comes first, and the check runs on every cache
// operation rather than only on inserts so that a run of hits still flushes a
// tail left by earlier misses. The exposure that buys is bounded and small: a
// crash or an Android kill loses at most the 15 most recently translated
// shaders, and only ones translated since the last flush -- they are recompiled
// and re-cached on the next run. Nothing older can be lost, because save() no
// longer overwrites the live file in place.
//
// The alternative shape, write-behind on a thread, was rejected: it needs a
// lock around cacheList that nothing in this class has today, and a background
// writer racing an Android kill has the same failure window anyway.
namespace {
    constexpr int kPendingEntriesBeforeSave = 16;
    constexpr int64_t kSaveIntervalNs = 5LL * 1000 * 1000 * 1000;

    int64_t monotonic_now_ns() {
        // CLOCK_MONOTONIC, so a wall-clock jump -- which Android does hand out
        // after an NTP sync -- can neither stall the flush nor force one.
        timespec ts{};
        if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
        return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + static_cast<int64_t>(ts.tv_nsec);
    }

    // get() and put() are called back to back with the same source on every miss
    // -- once to discover the miss, once to file what the translator produced --
    // and shader pack sources run to tens of kilobytes, so hashing twice per
    // compile is half the hashing done for nothing. A missing get() parks its
    // key here together with the bytes it was computed from, and put() reuses
    // it when the source matches byte for byte.
    //
    // The match has to be on content. Keying on the pointer would be wrong: the
    // caller hashes a local std::string, so a later compile can hand back the
    // same address holding different text, and a false match would file one
    // shader's ESSL under another shader's key -- a silently wrong program for
    // as long as the cache lives. The memcmp that rules that out is still
    // cheaper than the hashing it defers.
    //
    // The pair lives in the class as thread_local statics; only one thread ever
    // issues them at a time, since this class takes no lock anywhere.
} // namespace

thread_local string Cache::s_memo_source;
thread_local Cache::CacheKey Cache::s_memo_key{};
thread_local bool Cache::s_memo_valid = false;

Cache::Cache() {
    lastSaveNs = monotonic_now_ns();
    load();
}

Cache::~Cache() {
    // Best effort on top of the bound stated above, never the thing that bound
    // rests on: the instance is a function-local static, so this runs on an
    // orderly unload or exit and not at all when Android kills the process.
    // Both globals it reaches through save() outlive it -- global_settings_t is
    // trivially destructible, glsl_cache_file_path is a never-freed raw pointer,
    // and this object is constructed lazily on the first translation, hence
    // destroyed before either of them.
    if (pendingEntries > 0) save();
}

void Cache::flushIfDue() {
    if (pendingEntries == 0) return;
    if (pendingEntries < kPendingEntriesBeforeSave && (monotonic_now_ns() - lastSaveNs) < kSaveIntervalNs) return;
    save();
}

const char* Cache::get(const char* glsl) {
    if (global_settings.max_glsl_cache_size <= 0) return nullptr;
    flushIfDue();

    const size_t length = strlen(glsl);
    CacheKey key;
    if (s_memo_valid && s_memo_source.size() == length &&
        memcmp(s_memo_source.data(), glsl, length) == 0) {
        // Same source coming back is all the string compare costs; the key is
        // already known, so skip the XXH64 passes for repeated translations.
        key = s_memo_key;
    } else {
        key = computeKey(reinterpret_cast<const uint8_t*>(glsl), length);
    }
    auto it = cacheMap.find(key);
    if (it == cacheMap.end()) {
        // A miss is what put() follows; a hit ends the translation here, so only
        // the miss is worth remembering.
        s_memo_valid = false;
        s_memo_source.assign(glsl, length);
        s_memo_key = key;
        s_memo_valid = true;
        return nullptr;
    }

    cacheList.splice(cacheList.end(), cacheList, it->second);
    return it->second->essl.c_str();
}

void Cache::put(const char* glsl, const char* essl) {
    if (global_settings.max_glsl_cache_size <= 0) return;

    const size_t length = strlen(glsl);
    CacheKey key;
    if (s_memo_valid && s_memo_source.size() == length &&
        memcmp(s_memo_source.data(), glsl, length) == 0) {
        key = s_memo_key;
    } else {
        key = computeKey(reinterpret_cast<const uint8_t*>(glsl), length);
    }
    size_t esslStrSize = strlen(essl) + 1;

    size_t entryMemory = sizeof(CacheKey) + sizeof(size_t) + esslStrSize;

    if (auto it = cacheMap.find(key); it != cacheMap.end()) {
        cacheSize -= (sizeof(CacheKey) + sizeof(size_t) + it->second->size);
        cacheList.erase(it->second);
        cacheMap.erase(it);
    }

    cacheList.emplace_back(CacheEntry{key, essl, esslStrSize});
    cacheMap[key] = prev(cacheList.end());
    cacheSize += entryMemory;

    maintainCacheSize();
    ++pendingEntries;
    flushIfDue();
}

void Cache::maintainCacheSize() {
    if (global_settings.max_glsl_cache_size <= 0) return;
    while (cacheSize > global_settings.max_glsl_cache_size && !cacheList.empty()) {
        const auto& oldEntry = cacheList.front();
        size_t removedMemory = sizeof(CacheKey) + sizeof(size_t) + oldEntry.size;
        cacheSize -= removedMemory;
        cacheMap.erase(oldEntry.key);
        cacheList.pop_front();
    }
}

bool Cache::load() {
    try {
        // check_path() runs long before the first translation, but the path is a
        // global that starts null and ifstream would take it straight to fopen.
        if (!glsl_cache_file_path) return false;
        ifstream file(glsl_cache_file_path, ios::binary);
        if (!file) return false;

        // Versioned since the cache stopped keying on SHA-256. A file without
        // the magic is a pre-versioned build: its entries were keyed on a digest
        // that cannot be recomputed from a source string, so there is no way to
        // look any of them up. Those bytes are simply not worth reading -- the
        // cache is regenerable -- and the next save() writes the current format
        // over them. (The count came first in that format, so this is also what
        // a corrupted or truncated file lands on.)
        char magic[4];
        uint32_t version;
        file.read(magic, sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (memcmp(magic, kFormatMagic, sizeof(magic)) != 0 || version != kFormatVersion) return true;

        size_t count;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));

        while (count-- && file.good()) {
            CacheKey key{};
            size_t esslSize;

            file.read(reinterpret_cast<char*>(key.data()), key.size());
            file.read(reinterpret_cast<char*>(&esslSize), sizeof(esslSize));

            string essl(esslSize, '\0');
            file.read(essl.data(), (long)esslSize);

            if (cacheMap.count(key)) continue;

            size_t entryMemory = sizeof(CacheKey) + sizeof(size_t) + esslSize;
            cacheSize += entryMemory;

            cacheList.emplace_back(CacheEntry{key, move(essl), esslSize});
            cacheMap[key] = prev(cacheList.end());
        }

        maintainCacheSize();
        return true;
    }
    catch (...) {
        LOG_W_FORCE("Error while loading glsl cache file. Clearing it...")
        cacheMap.clear();
        cacheSize = 0;
        cacheList.clear();
        save();
        return false;
    }
}

void Cache::save() {
    if (global_settings.max_glsl_cache_size <= 0) return;
    if (!glsl_cache_file_path) return;

    // Cleared before the attempt, not after it. save() serialises the whole list
    // every time, so the counter is a trigger and not a record of what is
    // missing: if this write fails, the next one still writes those entries, and
    // clearing here is what stops a device with a full or unwritable /sdcard
    // from retrying a whole-file write on every single compile.
    pendingEntries = 0;
    lastSaveNs = monotonic_now_ns();

    // Written to a sibling and renamed over the cache instead of truncating the
    // live file: the write costs the same, but a crash in the middle of one can
    // no longer leave a half-written file that load() can only react to by
    // discarding every entry in it. rename(2) is atomic against this process
    // dying, which is the failure this cache has; it promises nothing about
    // power loss, which regenerable data does not need protecting from.
    const string temp_path = string(glsl_cache_file_path) + ".new";
    {
        ofstream file(temp_path, ios::binary);
        if (!file) return;

        file.write(kFormatMagic, sizeof(kFormatMagic));
        file.write(reinterpret_cast<const char*>(&kFormatVersion), sizeof(kFormatVersion));

        size_t count = cacheList.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));

        for (const auto& entry : cacheList) {
            file.write(reinterpret_cast<const char*>(entry.key.data()), (long)entry.key.size());
            size_t esslSize = entry.size;
            file.write(reinterpret_cast<const char*>(&esslSize), sizeof(esslSize));
            file.write(entry.essl.data(), (long)esslSize);
        }

        file.flush();
        if (!file) {
            // A short write must not become the cache: keep whatever is already
            // in place and drop the partial file.
            file.close();
            std::remove(temp_path.c_str());
            return;
        }
    }

    if (std::rename(temp_path.c_str(), glsl_cache_file_path) != 0) {
        std::remove(temp_path.c_str());
    }
}

Cache& Cache::get_instance() {
    static Cache s_cache;
    return s_cache;
}