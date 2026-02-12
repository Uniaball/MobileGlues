// MobileGlues - includes.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#ifndef MOBILEGLUES_INCLUDES_H
#define MOBILEGLUES_INCLUDES_H

#define RENDERERNAME "MobileGlues"
#ifdef __ANDROID__
#include <android/log.h>
#endif
#include <dlfcn.h>

#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <MG/extensions.h>

#include "egl/egl.h"
#include "egl/loader.h"

#if PROFILING
#include <perfetto.h>
PERFETTO_DEFINE_CATEGORIES(perfetto::Category("glcalls").SetDescription("Calls from OpenGL"),
                           perfetto::Category("internal").SetDescription("Internal calls"));
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    static int g_initialized = 0;

    void proc_init();

#ifdef __cplusplus
}
#endif

#include <FastSTL/UnorderedMap.h>

template <typename Key, typename T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>>
using UnorderedMap = FastSTL::unordered_map<Key, T, Hash, KeyEqual, Allocator>;

#endif // MOBILEGLUES_INCLUDES_H
