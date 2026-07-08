// MobileGlues - gl/glsl/cache.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "cache.h"
#include <fstream>
#include <cstring>
#include <vector>
#include <iostream>

using namespace std;

Cache::Cache() {
    load();
}

uint64_t Cache::computeXXHash(const char* data) {
    return XXH64(data, strlen(data), 0);
}

const char* Cache::get(const char* glsl) {
    if (global_settings.max_glsl_cache_size <= 0) return nullptr;
    auto hash = computeXXHash(glsl);
    auto it = cacheMap.find(hash);
    if (it == cacheMap.end()) return nullptr;

    cacheList.splice(cacheList.end(), cacheList, it->second);
    return it->second->essl.c_str();
}

void Cache::put(const char* glsl, const char* essl) {
    if (global_settings.max_glsl_cache_size <= 0) return;
    auto hash = computeXXHash(glsl);
    size_t esslStrSize = strlen(essl) + 1;

    if (auto it = cacheMap.find(hash); it != cacheMap.end()) {
        cacheSize -= (sizeof(CacheEntry) + it->second->size);
        cacheList.erase(it->second);
        cacheMap.erase(it);
    }

    cacheList.emplace_back(CacheEntry{hash, essl, esslStrSize});
    cacheMap[hash] = prev(cacheList.end());
    cacheSize += (sizeof(CacheEntry) + esslStrSize);

    maintainCacheSize();
    save();
}

void Cache::maintainCacheSize() {
    if (global_settings.max_glsl_cache_size <= 0) return;
    while (cacheSize > global_settings.max_glsl_cache_size && !cacheList.empty()) {
        const auto& oldEntry = cacheList.front();
        cacheSize -= (sizeof(CacheEntry) + oldEntry.size);
        cacheMap.erase(oldEntry.hash);
        cacheList.pop_front();
    }
}

bool Cache::load() {
    try {
        ifstream file(glsl_cache_file_path, ios::binary);
        if (!file) return false;

        size_t count;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));

        while (count--) {
            uint64_t hash;
            size_t esslSize;
            file.read(reinterpret_cast<char*>(&hash), sizeof(hash));
            file.read(reinterpret_cast<char*>(&esslSize), sizeof(esslSize));

            string essl(esslSize, '\0');
            file.read(essl.data(), static_cast<long>(esslSize));

            if (cacheMap.count(hash)) continue;

            cacheSize += (sizeof(CacheEntry) + esslSize);
            cacheList.emplace_back(CacheEntry{hash, move(essl), esslSize});
            cacheMap[hash] = prev(cacheList.end());
        }

        maintainCacheSize();
        return true;
    } catch (...) {
        LOG_W_FORCE("Error while loading glsl cache file. Clearing it...");
        cacheMap.clear();
        cacheSize = 0;
        cacheList.clear();
        save();
        return false;
    }
}

void Cache::save() {
    if (global_settings.max_glsl_cache_size <= 0) return;
    ofstream file(glsl_cache_file_path, ios::binary);
    if (!file) return;

    size_t count = cacheList.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& entry : cacheList) {
        file.write(reinterpret_cast<const char*>(&entry.hash), sizeof(entry.hash));
        size_t esslSize = entry.size;
        file.write(reinterpret_cast<const char*>(&esslSize), sizeof(esslSize));
        file.write(entry.essl.data(), static_cast<long>(esslSize));
    }
}

Cache& Cache::get_instance() {
    static Cache s_cache;
    return s_cache;
}