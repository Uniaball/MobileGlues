// MobileGlues - gl/FSR1/FSR1.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
#pragma once

#include <cstdlib>
#include <cstring>
#include <vector>

#ifndef __APPLE__
#include <malloc.h>
#endif

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "../../gles/gles.h"
#include "../../gles/loader.h"
#include "../../includes.h"
#include "../framebuffer.h"
#include "../glsl/glsl_for_es.h"
#include "../log.h"
#include "../mg.h"
#include "../pixel.h"
#include <GL/gl.h>
#include <ankerl/unordered_dense.h>
#include <glm/glm.hpp>

namespace FSR1_Context {
    extern GLuint g_renderFBO;
    extern GLuint g_renderTexture;
    extern GLuint g_depthStencilRBO;
    extern GLuint g_quadVAO;
    extern GLuint g_quadVBO;
    extern GLuint g_fsrProgram;

    extern GLuint g_targetFBO;
    extern GLuint g_targetTexture;

    extern GLuint g_currentDrawFBO;
    extern GLint g_viewport[4];
    extern GLsizei g_targetWidth;
    extern GLsizei g_targetHeight;
    extern GLsizei g_renderWidth;
    extern GLsizei g_renderHeight;
    extern bool g_dirty;

    extern bool g_resolutionChanged;
    extern GLsizei g_pendingWidth;
    extern GLsizei g_pendingHeight;

    // Cached uniform state — uploaded to GPU only when dirty.
    extern GLint g_uInputTexLoc;
    extern GLint g_uConst0Loc;
    extern GLint g_uViewportSizeLoc;
    extern bool g_uniformsDirty;
    extern glm::vec4 g_lastConst0;
    extern glm::vec2 g_lastViewportSize;

    // Cached viewport state — glViewport skipped when target dimensions match.
    extern GLsizei g_lastFsrViewportW;
    extern GLsizei g_lastFsrViewportH;
    extern bool g_viewportCacheValid;
} // namespace FSR1_Context

extern bool fsrInitialized;
void ApplyFSR();
void InitFSRResources();
void CheckResolutionChange();
void OnResize(int width, int height);

extern "C"
{
    GLAPI void glViewport(GLint x, GLint y, GLsizei w, GLsizei h);
}