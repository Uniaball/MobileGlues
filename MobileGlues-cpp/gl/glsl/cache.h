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
#include <string>
#include <cstdint>
#include <unordered_map>

#include "../../3rdparty/xxhash/xxhash.h"

class Cache {
public:
    Cache();
    ~Cache() = default;

    const char* get(const char* glsl);
    void put(const char* glsl, const char* essl);

    bool load();
    void save();

    static Cache& get_instance();

private:
    struct CacheEntry {
        uint64_t hash;
        std::string essl;
        size_t size;
    };

    using ListIterator = std::list<CacheEntry>::iterator;

    std::list<CacheEntry> cacheList;
    std::unordered_map<uint64_t, ListIterator> cacheMap;
    size_t cacheSize = 0;

    static uint64_t computeXXHash(const char* data);
    void maintainCacheSize();
};

#endif