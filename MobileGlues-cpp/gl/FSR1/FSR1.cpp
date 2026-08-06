// MobileGlues - gl/FSR1/FSR1.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "FSR1.h"
#include <mutex>
#include <unordered_map>
#include "FSRShaderSource.h"
#include "../../config/settings.h"

#define DEBUG 0

struct GLStateGuard {
    GLint prevProgram;
    GLint prevVAO;
    GLint prevActiveTexture;
    GLint prevTexture;
    GLint prevReadFBO;
    GLint prevDrawFBO;
    GLboolean prevDepthTest;
    GLboolean prevStencilTest;
    GLboolean prevBlend;
    GLboolean prevColorMask[4];

    GLStateGuard() {
        GLES.glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
        GLES.glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
        GLES.glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTexture);
        GLES.glActiveTexture(prevActiveTexture);
        GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
        GLES.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFBO);
        GLES.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFBO);
        prevDepthTest = GLES.glIsEnabled(GL_DEPTH_TEST);
        prevStencilTest = GLES.glIsEnabled(GL_STENCIL_TEST);
        prevBlend = GLES.glIsEnabled(GL_BLEND);
        GLES.glGetBooleanv(GL_COLOR_WRITEMASK, prevColorMask);
    }

    ~GLStateGuard() {
        GLES.glUseProgram(prevProgram);
        GLES.glBindVertexArray(prevVAO);
        GLES.glActiveTexture(prevActiveTexture);
        GLES.glBindTexture(GL_TEXTURE_2D, prevTexture);
        GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFBO);
        GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDrawFBO);
        if (prevDepthTest) GLES.glEnable(GL_DEPTH_TEST); else GLES.glDisable(GL_DEPTH_TEST);
        if (prevStencilTest) GLES.glEnable(GL_STENCIL_TEST); else GLES.glDisable(GL_STENCIL_TEST);
        if (prevBlend) GLES.glEnable(GL_BLEND); else GLES.glDisable(GL_BLEND);
        GLES.glColorMask(prevColorMask[0], prevColorMask[1], prevColorMask[2], prevColorMask[3]);
    }
};

namespace FSR1_Context {
    GLuint g_renderFBO = 0;
    GLuint g_renderTexture = 0;
    GLuint g_depthStencilRBO = 0;
    GLuint g_quadVAO = 0;
    GLuint g_quadVBO = 0;
    GLuint g_fsrProgram = 0;

    GLuint g_targetFBO = 0;
    GLuint g_targetTexture = 0;

    GLuint g_currentDrawFBO = 0;
    GLint g_viewport[4] = {0};
    GLsizei g_targetWidth = 2400;
    GLsizei g_targetHeight = 1080;
    GLsizei g_renderWidth = 1200;
    GLsizei g_renderHeight = 540;
    bool g_dirty = false;

    bool g_resolutionChanged = false;
    GLsizei g_pendingWidth = 0;
    GLsizei g_pendingHeight = 0;

    GLint g_uInputTexLoc = -1;
    GLint g_uConst0Loc = -1;
    GLint g_uViewportSizeLoc = -1;

    // Cached uniform state — uploaded to GPU only when dirty.
    bool g_uniformsDirty = true;
    glm::vec4 g_lastConst0 = glm::vec4(0.0f);
    glm::vec2 g_lastViewportSize = glm::vec2(0.0f);

    // Cached viewport state — glViewport skipped when target dimensions match.
    GLsizei g_lastFsrViewportW = 0;
    GLsizei g_lastFsrViewportH = 0;
    bool g_viewportCacheValid = false;
}

void CalculateTargetResolution(FSR1_Quality_Preset preset, int renderWidth, int renderHeight, int* targetWidth,
                               int* targetHeight) {
    float scale;
    switch (preset) {
    case FSR1_Quality_Preset::UltraQuality:
        scale = 1.3f;
        break;
    case FSR1_Quality_Preset::Quality:
        scale = 1.5f;
        break;
    case FSR1_Quality_Preset::Balanced:
        scale = 1.7f;
        break;
    case FSR1_Quality_Preset::Performance:
        scale = 2.0f;
        break;
    default:
        scale = 1.5f;
        break;
    }

    *targetWidth = static_cast<int>(renderWidth * scale);
    *targetHeight = static_cast<int>(renderHeight * scale);

    *targetWidth = (*targetWidth + 1) & ~1;
    *targetHeight = (*targetHeight + 1) & ~1;
    LOG_D("Render resolution: %dx%d", renderWidth, renderHeight);
    LOG_D("Target resolution: %dx%d", *targetWidth, *targetHeight);
}

void CalculateRenderResolution(FSR1_Quality_Preset preset, int targetWidth, int targetHeight, int* renderWidth,
                               int* renderHeight) {
    float scale;
    switch (preset) {
    case FSR1_Quality_Preset::UltraQuality:
        scale = 1.3f;
        break;
    case FSR1_Quality_Preset::Quality:
        scale = 1.5f;
        break;
    case FSR1_Quality_Preset::Balanced:
        scale = 1.7f;
        break;
    case FSR1_Quality_Preset::Performance:
        scale = 2.0f;
        break;
    default:
        scale = 1.5f;
    }

    *renderWidth = (int)(targetWidth / scale);
    *renderHeight = (int)(targetHeight / scale);

    *renderWidth = (*renderWidth + 1) & ~1;
    *renderHeight = (*renderHeight + 1) & ~1;
}

GLuint CompileFSRShader() {
    if (FSR1_Context::g_fsrProgram != 0) return FSR1_Context::g_fsrProgram;

    GLuint program = glCreateProgram();

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &FSR_VSSource, nullptr);
    glCompileShader(vs);

    GLint status;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        glGetShaderInfoLog(vs, 512, nullptr, log);
        LOG_F("Vertex shader error: %s\n", log);
        return 0;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &FSR_FSSource, nullptr);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        glGetShaderInfoLog(fs, 512, nullptr, log);
        LOG_F("Fragment shader error: %s\n", log);
        return 0;
    }

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        LOG_F("Program link error: %s\n", log);
        return 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    FSR1_Context::g_uInputTexLoc = glGetUniformLocation(program, "uInputTex");
    FSR1_Context::g_uConst0Loc = glGetUniformLocation(program, "uConst0");
    FSR1_Context::g_uViewportSizeLoc = glGetUniformLocation(program, "uViewportSize");

    FSR1_Context::g_fsrProgram = program;
    // Program (re)linked — uniform values need to be re-uploaded on next ApplyFSR.
    FSR1_Context::g_uniformsDirty = true;
    return program;
}

void InitFullscreenQuad() {
    GLStateGuard state;
    // GLStateGuard no longer tracks GL_ARRAY_BUFFER_BINDING; save/restore locally
    // because this function binds g_quadVBO to GL_ARRAY_BUFFER.
    GLint prevArrayBuffer = 0;
    GLES.glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevArrayBuffer);

    const float quadVertices[] = {-1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,
                                  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f};

    GLES.glGenVertexArrays(1, &FSR1_Context::g_quadVAO);
    GLES.glGenBuffers(1, &FSR1_Context::g_quadVBO);

    GLES.glBindVertexArray(FSR1_Context::g_quadVAO);
    GLES.glBindBuffer(GL_ARRAY_BUFFER, FSR1_Context::g_quadVBO);

    GLES.glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    GLES.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    GLES.glEnableVertexAttribArray(0);

    GLES.glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    GLES.glEnableVertexAttribArray(1);

    GLES.glBindBuffer(GL_ARRAY_BUFFER, 0);
    GLES.glBindVertexArray(0);

    GLES.glBindBuffer(GL_ARRAY_BUFFER, prevArrayBuffer);
}

bool fsrInitialized = false;
void InitFSRResources() {
    if (fsrInitialized) return;
    fsrInitialized = true;
    GLStateGuard state;
    // GLStateGuard no longer tracks GL_RENDERBUFFER_BINDING; save/restore locally
    // because this function binds g_depthStencilRBO to set its storage.
    GLint prevRenderbuffer = 0;
    GLES.glGetIntegerv(GL_RENDERBUFFER_BINDING, &prevRenderbuffer);

    FSR1_Context::g_fsrProgram = CompileFSRShader();
    InitFullscreenQuad();

    GLES.glGenTextures(1, &FSR1_Context::g_renderTexture);
    GLES.glBindTexture(GL_TEXTURE_2D, FSR1_Context::g_renderTexture);
    GLES.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FSR1_Context::g_renderWidth, FSR1_Context::g_renderHeight);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    GLES.glGenRenderbuffers(1, &FSR1_Context::g_depthStencilRBO);
    GLES.glBindRenderbuffer(GL_RENDERBUFFER, FSR1_Context::g_depthStencilRBO);
    GLES.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, FSR1_Context::g_renderWidth,
                               FSR1_Context::g_renderHeight);

    GLES.glGenFramebuffers(1, &FSR1_Context::g_renderFBO);
    GLES.glBindFramebuffer(GL_FRAMEBUFFER, FSR1_Context::g_renderFBO);
    GLES.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FSR1_Context::g_renderTexture, 0);
    GLES.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                   FSR1_Context::g_depthStencilRBO);

    GLES.glGenTextures(1, &FSR1_Context::g_targetTexture);
    GLES.glBindTexture(GL_TEXTURE_2D, FSR1_Context::g_targetTexture);
    GLES.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FSR1_Context::g_targetWidth, FSR1_Context::g_targetHeight);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    GLES.glGenFramebuffers(1, &FSR1_Context::g_targetFBO);
    GLES.glBindFramebuffer(GL_FRAMEBUFFER, FSR1_Context::g_targetFBO);
    GLES.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FSR1_Context::g_targetTexture, 0);

    GLES.glBindFramebuffer(GL_FRAMEBUFFER, FSR1_Context::g_renderFBO);
    GLES.glBindRenderbuffer(GL_RENDERBUFFER, prevRenderbuffer);

    // First-time init — force uniform upload and viewport set on next ApplyFSR.
    FSR1_Context::g_uniformsDirty = true;
    FSR1_Context::g_viewportCacheValid = false;
}

void RecreateFSRFBO() {
    GLStateGuard state;
    // GLStateGuard no longer tracks GL_RENDERBUFFER_BINDING; save/restore locally
    // because this function binds g_depthStencilRBO to set its storage.
    GLint prevRenderbuffer = 0;
    GLES.glGetIntegerv(GL_RENDERBUFFER_BINDING, &prevRenderbuffer);

    GLES.glDeleteFramebuffers(1, &FSR1_Context::g_renderFBO);
    GLES.glDeleteTextures(1, &FSR1_Context::g_renderTexture);
    GLES.glDeleteRenderbuffers(1, &FSR1_Context::g_depthStencilRBO);

    GLES.glDeleteFramebuffers(1, &FSR1_Context::g_targetFBO);
    GLES.glDeleteTextures(1, &FSR1_Context::g_targetTexture);

    GLES.glGenTextures(1, &FSR1_Context::g_renderTexture);
    GLES.glBindTexture(GL_TEXTURE_2D, FSR1_Context::g_renderTexture);
    GLES.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FSR1_Context::g_renderWidth, FSR1_Context::g_renderHeight);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    GLES.glGenRenderbuffers(1, &FSR1_Context::g_depthStencilRBO);
    GLES.glBindRenderbuffer(GL_RENDERBUFFER, FSR1_Context::g_depthStencilRBO);
    GLES.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, FSR1_Context::g_renderWidth,
                               FSR1_Context::g_renderHeight);

    GLES.glGenFramebuffers(1, &FSR1_Context::g_renderFBO);
    GLES.glBindFramebuffer(GL_FRAMEBUFFER, FSR1_Context::g_renderFBO);
    GLES.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FSR1_Context::g_renderTexture, 0);
    GLES.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                   FSR1_Context::g_depthStencilRBO);

    GLES.glGenTextures(1, &FSR1_Context::g_targetTexture);
    GLES.glBindTexture(GL_TEXTURE_2D, FSR1_Context::g_targetTexture);
    GLES.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FSR1_Context::g_targetWidth, FSR1_Context::g_targetHeight);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    GLES.glGenFramebuffers(1, &FSR1_Context::g_targetFBO);
    GLES.glBindFramebuffer(GL_FRAMEBUFFER, FSR1_Context::g_targetFBO);
    GLES.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FSR1_Context::g_targetTexture, 0);

    GLES.glBindFramebuffer(GL_FRAMEBUFFER, FSR1_Context::g_renderFBO);
    GLES.glViewport(0, 0, FSR1_Context::g_renderWidth, FSR1_Context::g_renderHeight);
    GLES.glBindRenderbuffer(GL_RENDERBUFFER, prevRenderbuffer);

    // Resources rebuilt — uniforms and viewport cache must be re-pushed next frame.
    FSR1_Context::g_uniformsDirty = true;
    FSR1_Context::g_viewportCacheValid = false;
}

void ApplyFSR() {
    GLStateGuard state;

    // FSR pass writes every pixel of the target; disable depth/stencil/blend
    // explicitly so the GPU does not pay for those stages this pass.
    GLES.glDisable(GL_DEPTH_TEST);
    GLES.glDisable(GL_STENCIL_TEST);
    GLES.glDisable(GL_BLEND);
    GLES.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    GLES.glBindFramebuffer(GL_FRAMEBUFFER, FSR1_Context::g_targetFBO);
    // Skip glViewport if the target dimensions match the last value FSR pushed.
    if (!FSR1_Context::g_viewportCacheValid ||
        FSR1_Context::g_lastFsrViewportW != FSR1_Context::g_targetWidth ||
        FSR1_Context::g_lastFsrViewportH != FSR1_Context::g_targetHeight) {
        GLES.glViewport(0, 0, FSR1_Context::g_targetWidth, FSR1_Context::g_targetHeight);
        FSR1_Context::g_lastFsrViewportW = FSR1_Context::g_targetWidth;
        FSR1_Context::g_lastFsrViewportH = FSR1_Context::g_targetHeight;
    }
    // FSR fragment shader writes every pixel; no need to glClear target FBO.

    GLES.glUseProgram(FSR1_Context::g_fsrProgram);

    GLES.glActiveTexture(GL_TEXTURE0);
    GLES.glBindTexture(GL_TEXTURE_2D, FSR1_Context::g_renderTexture);

    glm::vec4 const0 = {float(FSR1_Context::g_renderWidth) / FSR1_Context::g_targetWidth,
                        float(FSR1_Context::g_renderHeight) / FSR1_Context::g_targetHeight,
                        1.0f / FSR1_Context::g_targetWidth, 1.0f / FSR1_Context::g_targetHeight};

    glm::vec2 viewportSize = {(float)FSR1_Context::g_renderWidth, (float)FSR1_Context::g_renderHeight};

    if (FSR1_Context::g_uniformsDirty) {
        if (FSR1_Context::g_uInputTexLoc >= 0)
            GLES.glUniform1i(FSR1_Context::g_uInputTexLoc, 0);
        if (FSR1_Context::g_uConst0Loc >= 0)
            GLES.glUniform4fv(FSR1_Context::g_uConst0Loc, 1, reinterpret_cast<const GLfloat*>(&const0));
        if (FSR1_Context::g_uViewportSizeLoc >= 0)
            GLES.glUniform2fv(FSR1_Context::g_uViewportSizeLoc, 1, reinterpret_cast<const GLfloat*>(&viewportSize));
        FSR1_Context::g_lastConst0 = const0;
        FSR1_Context::g_lastViewportSize = viewportSize;
        FSR1_Context::g_uniformsDirty = false;
    } else {
        if (FSR1_Context::g_uInputTexLoc >= 0)
            GLES.glUniform1i(FSR1_Context::g_uInputTexLoc, 0);
        if (FSR1_Context::g_uConst0Loc >= 0 && FSR1_Context::g_lastConst0 != const0) {
            GLES.glUniform4fv(FSR1_Context::g_uConst0Loc, 1, reinterpret_cast<const GLfloat*>(&const0));
            FSR1_Context::g_lastConst0 = const0;
        }
        if (FSR1_Context::g_uViewportSizeLoc >= 0 && FSR1_Context::g_lastViewportSize != viewportSize) {
            GLES.glUniform2fv(FSR1_Context::g_uViewportSizeLoc, 1, reinterpret_cast<const GLfloat*>(&viewportSize));
            FSR1_Context::g_lastViewportSize = viewportSize;
        }
    }

    GLES.glBindVertexArray(FSR1_Context::g_quadVAO);
    GLES.glDrawArrays(GL_TRIANGLES, 0, 6);
    GLES.glBindVertexArray(0);

    GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER, FSR1_Context::g_targetFBO);
    GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    GLES.glBlitFramebuffer(0, 0, FSR1_Context::g_targetWidth, FSR1_Context::g_targetHeight, 0, 0,
                           FSR1_Context::g_targetWidth, FSR1_Context::g_targetHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    GLES.glBindFramebuffer(GL_FRAMEBUFFER, FSR1_Context::g_renderFBO);
    // Tell tile-based GPUs the render FBO's depth/stencil need not be preserved
    // when we leave this FBO; the app is expected to clear depth next frame.
    {
        const GLenum invalidateAttachments[] = {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
        GLES.glInvalidateFramebuffer(GL_FRAMEBUFFER, 2, invalidateAttachments);
    }
    if (!FSR1_Context::g_viewportCacheValid ||
        FSR1_Context::g_lastFsrViewportW != FSR1_Context::g_renderWidth ||
        FSR1_Context::g_lastFsrViewportH != FSR1_Context::g_renderHeight) {
        GLES.glViewport(0, 0, FSR1_Context::g_renderWidth, FSR1_Context::g_renderHeight);
        FSR1_Context::g_lastFsrViewportW = FSR1_Context::g_renderWidth;
        FSR1_Context::g_lastFsrViewportH = FSR1_Context::g_renderHeight;
        FSR1_Context::g_viewportCacheValid = true;
    }
}

void CheckResolutionChange(EGLDisplay display, EGLSurface surface) {
    GLsizei width = 0, height = 0;
    LOAD_EGL(eglQuerySurface);
    // Taken from the swap this is hooked into rather than latched into statics on
    // first use. The old code kept the first display and surface it ever saw, so
    // after a rotation or a surface rebuild it queried a destroyed surface every
    // frame and the resolution never changed again.
    if (display == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE) {
        display = eglGetCurrentDisplay();
        surface = eglGetCurrentSurface(EGL_DRAW);
    }
    egl_eglQuerySurface(display, surface, EGL_WIDTH, &width);
    egl_eglQuerySurface(display, surface, EGL_HEIGHT, &height);
    OnResize(width, height);
    }

    if (FSR1_Context::g_resolutionChanged) {
        FSR1_Context::g_resolutionChanged = false;
        GLsizei w = FSR1_Context::g_pendingWidth;
        GLsizei h = FSR1_Context::g_pendingHeight;
        FSR1_Context::g_renderWidth = w;
        FSR1_Context::g_renderHeight = h;

        CalculateTargetResolution(global_settings.fsr1_setting, w, h,
                                  reinterpret_cast<int*>(&FSR1_Context::g_targetWidth),
                                  reinterpret_cast<int*>(&FSR1_Context::g_targetHeight));
        RecreateFSRFBO();
    }
    // ApplyFSR just set this viewport; skip the redundant GLES.glViewport when
    // the cache is still valid and matches render dimensions. If RecreateFSRFBO
    // ran above, the cache was invalidated and the call below is required.
    if (!FSR1_Context::g_viewportCacheValid ||
        FSR1_Context::g_lastFsrViewportW != FSR1_Context::g_renderWidth ||
        FSR1_Context::g_lastFsrViewportH != FSR1_Context::g_renderHeight) {
        GLES.glViewport(0, 0, FSR1_Context::g_renderWidth, FSR1_Context::g_renderHeight);
        FSR1_Context::g_lastFsrViewportW = FSR1_Context::g_renderWidth;
        FSR1_Context::g_lastFsrViewportH = FSR1_Context::g_renderHeight;
        FSR1_Context::g_viewportCacheValid = true;
    }
}

void OnResize(int width, int height) {
    if (FSR1_Context::g_renderWidth == width && FSR1_Context::g_renderHeight == height) return;
    FSR1_Context::g_pendingWidth = width;
    FSR1_Context::g_pendingHeight = height;
    FSR1_Context::g_resolutionChanged = true;
}

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    LOG()
    LOG_D("glViewport: x=%d, y=%d, w=%d, h=%d", x, y, w, h);

    if (w > FSR1_Context::g_pendingWidth || h > FSR1_Context::g_pendingHeight) {
        FSR1_Context::g_pendingWidth = w;
        FSR1_Context::g_pendingHeight = h;
        FSR1_Context::g_resolutionChanged = true;
    }

    GLES.glViewport(x, y, w, h);
    // App-driven glViewport changes the GL viewport state behind FSR's back;
    // invalidate the FSR viewport cache so the next ApplyFSR re-sets it.
    FSR1_Context::g_viewportCacheValid = false;
}

// ---------------------------------------------------------------------------

namespace {

struct fsr1_ctx_state_t {
    GLuint renderFBO = 0, renderTexture = 0, depthStencilRBO = 0;
    GLuint quadVAO = 0, quadVBO = 0, fsrProgram = 0;
    GLuint targetFBO = 0, targetTexture = 0, currentDrawFBO = 0;
    GLsizei targetWidth = 0, targetHeight = 0, renderWidth = 0, renderHeight = 0;
    bool initialised = false;
};

std::mutex g_fsr_mutex;
std::unordered_map<unsigned long long, fsr1_ctx_state_t> g_fsr_states;
fsr1_ctx_state_t g_fsr_default;
thread_local unsigned long long g_fsr_current_id = 0;

void store_into(fsr1_ctx_state_t& d) {
    d.renderFBO = FSR1_Context::g_renderFBO;
    d.renderTexture = FSR1_Context::g_renderTexture;
    d.depthStencilRBO = FSR1_Context::g_depthStencilRBO;
    d.quadVAO = FSR1_Context::g_quadVAO;
    d.quadVBO = FSR1_Context::g_quadVBO;
    d.fsrProgram = FSR1_Context::g_fsrProgram;
    d.targetFBO = FSR1_Context::g_targetFBO;
    d.targetTexture = FSR1_Context::g_targetTexture;
    d.currentDrawFBO = FSR1_Context::g_currentDrawFBO;
    d.targetWidth = FSR1_Context::g_targetWidth;
    d.targetHeight = FSR1_Context::g_targetHeight;
    d.renderWidth = FSR1_Context::g_renderWidth;
    d.renderHeight = FSR1_Context::g_renderHeight;
    d.initialised = fsrInitialized;
}

void load_from(const fsr1_ctx_state_t& s) {
    FSR1_Context::g_renderFBO = s.renderFBO;
    FSR1_Context::g_renderTexture = s.renderTexture;
    FSR1_Context::g_depthStencilRBO = s.depthStencilRBO;
    FSR1_Context::g_quadVAO = s.quadVAO;
    FSR1_Context::g_quadVBO = s.quadVBO;
    FSR1_Context::g_fsrProgram = s.fsrProgram;
    FSR1_Context::g_targetFBO = s.targetFBO;
    FSR1_Context::g_targetTexture = s.targetTexture;
    FSR1_Context::g_currentDrawFBO = s.currentDrawFBO;
    FSR1_Context::g_targetWidth = s.targetWidth;
    FSR1_Context::g_targetHeight = s.targetHeight;
    FSR1_Context::g_renderWidth = s.renderWidth;
    FSR1_Context::g_renderHeight = s.renderHeight;
    fsrInitialized = s.initialised;
    // Left alone deliberately: g_dirty, g_resolutionChanged and the pending size
    // describe work queued for the frame in flight, not the context's objects.
}

} // namespace

void mg_fsr1_bind_context(unsigned long long ctx_id) {
    if (ctx_id == g_fsr_current_id) return;
    std::lock_guard<std::mutex> lock(g_fsr_mutex);
    store_into(g_fsr_current_id == 0 ? g_fsr_default : g_fsr_states[g_fsr_current_id]);
    load_from(ctx_id == 0 ? g_fsr_default : g_fsr_states[ctx_id]);
    g_fsr_current_id = ctx_id;
}
