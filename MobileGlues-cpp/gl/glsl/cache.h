// MobileGlues - gl/glsl/cache.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#ifndef MOBILEGLUES_PLUGIN_CACHE_H
#define MOBILEGLUES_PLUGIN_CACHE_H

#include "../mg.h"
#include "../../config/config.h"
#include "../../config/settings.h"

#include <list>
#include <array>
#include <string>
#include <cstdint>

class Cache {
public:
    Cache();
    ~Cache();

    // cacheMap holds iterators into cacheList, so a copy would leave the copy's
    // map pointing into the original's list. There is exactly one instance and
    // it is the singleton below; say so structurally rather than by convention.
    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;

    const char* get(const char* glsl);
    void put(const char* glsl, const char* essl);
    bool load();
    // Serialises the whole cache immediately. put() no longer calls this on
    // every insert -- see the persistence policy comment in cache.cpp -- so it
    // doubles as the "flush what is pending" entry point for any teardown hook
    // that later wants one.
    void save();

    static Cache& get_instance();

private:
    // The source's lookup key: two independent 64-bit hashes of it packed into
    // 128 bits. That is wide enough that a wrong answer needs a collision of the
    // same order as the SHA-256 this used to key on, and cheap enough (one or
    // two vectorised passes over a mid-kilobyte shader) that get() and put()
    // no longer need to memoise the digest the way they did the old one.
    using CacheKey = std::array<uint8_t, 16>;
    struct CacheEntry {
        CacheKey key;
        std::string essl;
        size_t size;
    };

    struct KeyHash {
        size_t operator()(const CacheKey& key) const;
    };

    std::list<CacheEntry> cacheList;
    using ListIterator = std::list<CacheEntry>::iterator;
    UnorderedMap<CacheKey, ListIterator, KeyHash> cacheMap;
    size_t cacheSize = 0;

    // Entries inserted since the last save(), and when that save happened on
    // CLOCK_MONOTONIC. The count is only a trigger, never a record of what is
    // unwritten: save() always serialises the entire list, so whichever save
    // runs next writes every pending entry regardless of how the counter got
    // where it is.
    int pendingEntries = 0;
    int64_t lastSaveNs = 0;

    // get() and put() are called back to back with the same source on every
    // miss, so a missing get() parks its key and the bytes it was computed from
    // here for put() to reuse. thread_local because the pair is always issued
    // from one thread and this class takes no lock anywhere.
    static thread_local std::string s_memo_source;
    static thread_local CacheKey s_memo_key;
    static thread_local bool s_memo_valid;

    static CacheKey computeKey(const uint8_t* data, size_t length);
    void maintainCacheSize();
    void flushIfDue();
};

#endif
