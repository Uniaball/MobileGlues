#pragma once

#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

#ifndef __APPLE__
#include <android/log.h>
#include <malloc.h>
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
} // namespace FSR1_Context

extern bool fsrInitialized;
void ApplyFSR();
void InitFSRResources();
void CheckResolutionChange();
void OnResize(int width, int height);

extern "C" {
GLAPI void glViewport(GLint x, GLint y, GLsizei w, GLsizei h);
}