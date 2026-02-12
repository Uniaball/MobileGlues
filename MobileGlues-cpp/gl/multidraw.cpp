// MobileGlues - gl/multidraw.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "multidraw.h"
#include "../config/settings.h"
#include <cstdint>
#include <limits>
#include <vector>
#include <memory>
#include <algorithm>

#define DEBUG 0

namespace {
    // 缓存临时buffer避免重复分配和释放
    std::vector<GLuint> temp_buffers;
    std::vector<void*> temp_indices_ptrs;
    size_t temp_buffers_size = 0;

    // 一次分配全部需要的buffer
    void allocate_temp_buffers(GLsizei primcount) {
        if (temp_buffers_size < static_cast<size_t>(primcount)) {
            temp_buffers.resize(primcount);
            temp_indices_ptrs.resize(primcount);
            temp_buffers_size = primcount;
        }
    }
}

typedef void (*glMultiDrawElements_t)(GLenum, const GLsizei*, GLenum, const void* const*, GLsizei);

void glMultiDrawElements(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices,
                         GLsizei primcount) {
    static glMultiDrawElements_t func_ptr = nullptr;

    if (func_ptr == nullptr) {
        switch (global_settings.multidraw_mode) {
        case multidraw_mode_t::PreferIndirect:
            func_ptr = mg_glMultiDrawElements_indirect;
            break;
        case multidraw_mode_t::PreferBaseVertex:
            func_ptr = mg_glMultiDrawElements_basevertex;
            break;
        case multidraw_mode_t::PreferMultidrawIndirect:
            func_ptr = mg_glMultiDrawElements_multiindirect;
            break;
        case multidraw_mode_t::DrawElements:
            func_ptr = mg_glMultiDrawElements_drawelements;
            break;
        case multidraw_mode_t::Compute:
            func_ptr = mg_glMultiDrawElements_compute;
            break;
        default:
            func_ptr = mg_glMultiDrawElements_drawelements;
            break;
        }
    }
    func_ptr(mode, count, type, indices, primcount);
}

typedef void (*glMultiDrawElementsBaseVertex_t)(GLenum, GLsizei*, GLenum, const void* const*, GLsizei, const GLint*);

void glMultiDrawElementsBaseVertex(GLenum mode, GLsizei* counts, GLenum type, const void* const* indices,
                                   GLsizei primcount, const GLint* basevertex) {
    static glMultiDrawElementsBaseVertex_t func_ptr = nullptr;

    if (func_ptr == nullptr) {
        switch (global_settings.multidraw_mode) {
        case multidraw_mode_t::PreferIndirect:
            func_ptr = mg_glMultiDrawElementsBaseVertex_indirect;
            break;
        case multidraw_mode_t::PreferBaseVertex:
            func_ptr = mg_glMultiDrawElementsBaseVertex_basevertex;
            break;
        case multidraw_mode_t::PreferMultidrawIndirect:
            func_ptr = mg_glMultiDrawElementsBaseVertex_multiindirect;
            break;
        case multidraw_mode_t::DrawElements:
            func_ptr = mg_glMultiDrawElementsBaseVertex_drawelements;
            break;
        case multidraw_mode_t::Compute:
            func_ptr = mg_glMultiDrawElementsBaseVertex_compute;
            break;
        default:
            func_ptr = mg_glMultiDrawElementsBaseVertex_drawelements;
            break;
        }
    }

    func_ptr(mode, counts, type, indices, primcount, basevertex);
}

static bool g_indirect_cmds_inited = false;
// 提升初始分配大小，减少后续扩容
static GLsizei g_cmdbufsize = 128;
static GLuint g_indirectbuffer = 0;
static GLuint prevIndirectBuffer = 0;

void prepare_indirect_buffer(const GLsizei* counts, GLenum type, const void* const* indices, GLsizei primcount,
                             const GLint* basevertex) {
    GLES.glGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, (GLint*)&prevIndirectBuffer);
    if (!g_indirect_cmds_inited) {
        GLES.glGenBuffers(1, &g_indirectbuffer);
        GLES.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_indirectbuffer);
        // 初始分配更大，避免频繁扩容
        GLES.glBufferData(GL_DRAW_INDIRECT_BUFFER, g_cmdbufsize * sizeof(draw_elements_indirect_command_t), NULL,
                          GL_DYNAMIC_DRAW);

        g_indirect_cmds_inited = true;
    }
    GLES.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_indirectbuffer);

    if (g_cmdbufsize < primcount) {
        size_t sz = g_cmdbufsize;
        // 更简洁的扩容逻辑
        while (sz < static_cast<size_t>(primcount))
            sz *= 2;
        GLES.glBufferData(GL_DRAW_INDIRECT_BUFFER, sz * sizeof(draw_elements_indirect_command_t), NULL,
                          GL_DYNAMIC_DRAW);
        g_cmdbufsize = static_cast<GLsizei>(sz);
    }

    auto* pcmds = (draw_elements_indirect_command_t*)GLES.glMapBufferRange(
        GL_DRAW_INDIRECT_BUFFER, 0, primcount * sizeof(draw_elements_indirect_command_t),
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    GLsizei elementSize;
    switch (type) {
    case GL_UNSIGNED_BYTE:
        elementSize = 1;
        break;
    case GL_UNSIGNED_SHORT:
        elementSize = 2;
        break;
    case GL_UNSIGNED_INT:
        elementSize = 4;
        break;
    default:
        elementSize = 4;
    }

    for (GLsizei i = 0; i < primcount; ++i) {
        auto byteOffset = reinterpret_cast<uintptr_t>(indices[i]);
        pcmds[i].firstIndex = static_cast<GLuint>(byteOffset / elementSize);
        pcmds[i].count = counts[i];
        pcmds[i].instanceCount = 1;
        pcmds[i].baseVertex = basevertex ? basevertex[i] : 0;
        pcmds[i].reservedMustBeZero = 0;
    }

    GLES.glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER);
}

// 优化后的basevertex实现
void mg_glMultiDrawElementsBaseVertex_drawelements(GLenum mode, GLsizei* counts, GLenum type,
                                                   const void* const* indices, GLsizei primcount,
                                                   const GLint* basevertex) {
    LOG()
    void prepareForDraw();
    prepareForDraw();
    GLint prevElementBuffer;
    GLES.glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &prevElementBuffer);

    // 使用缓冲区复用优化
    allocate_temp_buffers(primcount);

    size_t indexSize;
    switch (type) {
    case GL_UNSIGNED_INT:
        indexSize = sizeof(GLuint);
        break;
    case GL_UNSIGNED_SHORT:
        indexSize = sizeof(GLushort);
        break;
    case GL_UNSIGNED_BYTE:
        indexSize = sizeof(GLubyte);
        break;
    default:
        return;
    }

    // 一次分配全部temp buffer
    GLES.glGenBuffers(primcount, temp_buffers.data());

    for (GLsizei i = 0; i < primcount; ++i) {
        if (counts[i] <= 0) continue;

        GLsizei currentCount = counts[i];
        const GLvoid* currentIndices = indices[i];
        GLint currentBaseVertex = basevertex[i];

        void* srcData = nullptr;
        temp_indices_ptrs[i] = malloc(currentCount * indexSize);
        if (!temp_indices_ptrs[i]) continue;

        if (prevElementBuffer != 0) {
            GLES.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prevElementBuffer);
            srcData = GLES.glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)currentIndices, currentCount * indexSize,
                                            GL_MAP_READ_BIT);

            if (!srcData) {
                free(temp_indices_ptrs[i]);
                continue;
            }
        } else {
            srcData = (void*)currentIndices;
        }

        switch (type) {
        case GL_UNSIGNED_INT:
            for (int j = 0; j < currentCount; ++j) {
                ((GLuint*)temp_indices_ptrs[i])[j] = ((GLuint*)srcData)[j] + currentBaseVertex;
            }
            break;
        case GL_UNSIGNED_SHORT:
            for (int j = 0; j < currentCount; ++j) {
                ((GLushort*)temp_indices_ptrs[i])[j] = ((GLushort*)srcData)[j] + currentBaseVertex;
            }
            break;
        case GL_UNSIGNED_BYTE:
            for (int j = 0; j < currentCount; ++j) {
                ((GLubyte*)temp_indices_ptrs[i])[j] = ((GLubyte*)srcData)[j] + currentBaseVertex;
            }
            break;
        }

        if (prevElementBuffer != 0) {
            GLES.glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
        }

        GLES.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, temp_buffers[i]);
        GLES.glBufferData(GL_ELEMENT_ARRAY_BUFFER, currentCount * indexSize, temp_indices_ptrs[i], GL_STREAM_DRAW);
        GLES.glDrawElements(mode, currentCount, type, 0);
    }

    GLES.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prevElementBuffer);

    // 清理分配
    for (GLsizei i = 0; i < primcount; ++i) {
        if (temp_indices_ptrs[i])
            free(temp_indices_ptrs[i]);
    }
    GLES.glDeleteBuffers(primcount, temp_buffers.data());

    CHECK_GL_ERROR
}

void mg_glMultiDrawElementsBaseVertex_indirect(GLenum mode, GLsizei* counts, GLenum type, const void* const* indices,
                                               GLsizei primcount, const GLint* basevertex) {
    LOG()
    void prepareForDraw();
    prepareForDraw();

    prepare_indirect_buffer(counts, type, indices, primcount, basevertex);

    // Draw indirect!
    for (GLsizei i = 0; i < primcount; ++i) {
        const GLvoid* offset = reinterpret_cast<GLvoid*>(i * sizeof(draw_elements_indirect_command_t));
        GLES.glDrawElementsIndirect(mode, type, offset);
    }

    GLES.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, prevIndirectBuffer);

    CHECK_GL_ERROR
}

void mg_glMultiDrawElementsBaseVertex_multiindirect(GLenum mode, GLsizei* counts, GLenum type,
                                                    const void* const* indices, GLsizei primcount,
                                                    const GLint* basevertex) {
    LOG()
    void prepareForDraw();
    prepareForDraw();

    prepare_indirect_buffer(counts, type, indices, primcount, basevertex);

    // Multi-draw indirect!
    GLES.glMultiDrawElementsIndirectEXT(mode, type, 0, primcount, 0);

    GLES.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, prevIndirectBuffer);

    CHECK_GL_ERROR
}

void mg_glMultiDrawElementsBaseVertex_basevertex(GLenum mode, GLsizei* counts, GLenum type, const void* const* indices,
                                                 GLsizei primcount, const GLint* basevertex) {
    LOG()
    void prepareForDraw();
    prepareForDraw();

    for (GLsizei i = 0; i < primcount; ++i) {
        const GLsizei count = counts[i];
        if (count > 0) {
            LOG_D("GLES.glDrawElementsBaseVertex, mode = %s, count = %d, type = %s, indices[i] = 0x%x, basevertex[i] = "
                  "%d",
                  glEnumToString(mode), count, glEnumToString(type), indices[i], basevertex[i])
            GLES.glDrawElementsBaseVertex(mode, count, type, indices[i], basevertex[i]);
        }
    }
    CHECK_GL_ERROR
}

void mg_glMultiDrawElements_indirect(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices,
                                     GLsizei primcount) {
    LOG()
    void prepareForDraw();
    prepareForDraw();

    prepare_indirect_buffer(count, type, indices, primcount, 0);
    // Draw indirect!
    for (GLsizei i = 0; i < primcount; ++i) {
        const GLvoid* offset = reinterpret_cast<GLvoid*>(i * sizeof(draw_elements_indirect_command_t));
        GLES.glDrawElementsIndirect(mode, type, offset);
    }

    GLES.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, prevIndirectBuffer);
    CHECK_GL_ERROR
}

void mg_glMultiDrawElements_drawelements(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices,
                                         GLsizei primcount) {
    LOG()
    void prepareForDraw();
    prepareForDraw();

    for (GLsizei i = 0; i < primcount; ++i) {
        const GLsizei c = count[i];
        if (c > 0) {
            GLES.glDrawElements(mode, c, type, indices[i]);
        }
    }

    CHECK_GL_ERROR
}

void mg_glMultiDrawElements_compute(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices,
                                    GLsizei primcount) {
    LOG()
    void prepareForDraw();
    prepareForDraw();

    for (GLsizei i = 0; i < primcount; ++i) {
        const GLsizei c = count[i];
        if (c > 0) {
            GLES.glDrawElements(mode, c, type, indices[i]);
        }
    }

    CHECK_GL_ERROR
}

void mg_glMultiDrawElements_multiindirect(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices,
                                          GLsizei primcount) {
    LOG()
    void prepareForDraw();
    prepareForDraw();

    prepare_indirect_buffer(count, type, indices, primcount, 0);

    // Multi-draw indirect!
    GLES.glMultiDrawElementsIndirectEXT(mode, type, 0, primcount, 0);

    GLES.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, prevIndirectBuffer);

    CHECK_GL_ERROR
}

void mg_glMultiDrawElements_basevertex(GLenum mode, const GLsizei* count, GLenum type, const void* const* indices,
                                       GLsizei primcount) {
    LOG()
    void prepareForDraw();
    prepareForDraw();

    for (GLsizei i = 0; i < primcount; ++i) {
        const GLsizei c = count[i];
        if (c > 0) {
            GLES.glDrawElements(mode, c, type, indices[i]);
        }
    }

    CHECK_GL_ERROR
}

static bool is_strip_like_mode(GLenum mode) {
    switch (mode) {
    case GL_LINE_STRIP:
    case GL_LINE_LOOP:
    case GL_TRIANGLE_STRIP:
    case GL_TRIANGLE_FAN:
    case GL_LINE_STRIP_ADJACENCY:
    case GL_TRIANGLE_STRIP_ADJACENCY:
        return true;
    default:
        return false;
    }
}

// 更完整的compute shader
const std::string multidraw_comp_shader =
    R"(#version 310 es

layout(local_size_x = 64) in;

layout(location = 0) uniform uint uElementSize;

layout(std430, binding = 0) readonly buffer Input { uint in_indices[]; };
layout(std430, binding = 1) readonly buffer FirstIndex { uint firstIndex[]; };
layout(std430, binding = 2) readonly buffer BaseVertex { int baseVertex[]; };
layout(std430, binding = 3) readonly buffer Prefix { uint prefixSums[]; };
layout(std430, binding = 4) writeonly buffer Output { uint out_indices[]; };

uint read_index(uint elementIndex) {
    if (uElementSize == 4u) {
        return in_indices[elementIndex];
    }
    if (uElementSize == 2u) {
        uint word = in_indices[elementIndex >> 1u];
        uint shift = (elementIndex & 1u) * 16u;
        return (word >> shift) & 0xFFFFu;
    }
    uint word = in_indices[elementIndex >> 2u];
    uint shift = (elementIndex & 3u) * 8u;
    return (word >> shift) & 0xFFu;
}

void main() {
    uint outIdx = gl_GlobalInvocationID.x;
    uint drawCount = uint(prefixSums.length());
    if (drawCount == 0u) {
        return;
    }
    uint total = prefixSums[drawCount - 1u];
    if (outIdx >= total) {
        return;
    }

    int low = 0;
    int high = int(drawCount) - 1;
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (prefixSums[mid] > outIdx) {
            high = mid; // next [low, mid)
        } else {
            low = mid + 1; // next [mid + 1, high)
        }
    }

    uint localIdx = outIdx - ((low == 0) ? 0u : (prefixSums[low - 1]));
    uint inIndex = localIdx + firstIndex[low];

    int idx = int(read_index(inIndex));
    out_indices[outIdx] = uint(idx + baseVertex[low]);
}

)";

static bool g_compute_inited = false;
// 提高初始分配大小
std::vector<GLuint> g_prefix_sum(128);
GLuint g_prefixsumbuffer = 0;
GLuint g_firstidx_ssbo = 0;
GLuint g_basevtx_ssbo = 0;
GLuint g_outputibo = 0;
GLuint g_compute_program = 0;
GLint g_element_size_loc = -1;
char g_compile_info[1024];

GLuint compile_compute_program(const std::string& src) {
    INIT_CHECK_GL_ERROR
    auto program = GLES.glCreateProgram();
    CHECK_GL_ERROR_NO_INIT
    GLuint shader = GLES.glCreateShader(GL_COMPUTE_SHADER);
    CHECK_GL_ERROR_NO_INIT
    const char* s[] = {src.c_str()};
    const GLint length[] = {static_cast<GLint>(src.length())};
    GLES.glShaderSource(shader, 1, s, length);
    CHECK_GL_ERROR_NO_INIT
    GLES.glCompileShader(shader);
    CHECK_GL_ERROR_NO_INIT
    int success = 0;
    GLES.glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    CHECK_GL_ERROR_NO_INIT
    if (!success) {
        GLES.glGetShaderInfoLog(shader, 1024, NULL, g_compile_info);
        CHECK_GL_ERROR_NO_INIT
        LOG_E("%s: %s shader compile error: %s\nsrc:\n%s", __func__, "compute", g_compile_info, src.c_str());
#if DEBUG || GLOBAL_DEBUG
        abort();
#endif
        GLES.glDeleteShader(shader);
        GLES.glDeleteProgram(program);
        return 0;
    }

    GLES.glAttachShader(program, shader);
    CHECK_GL_ERROR_NO_INIT
    GLES.glLinkProgram(program);
    CHECK_GL_ERROR_NO_INIT

    GLES.glGetProgramiv(program, GL_LINK_STATUS, &success);
    CHECK_GL_ERROR_NO_INIT
    if (!success) {
        GLES.glGetProgramInfoLog(program, 1024, NULL, g_compile_info);
        CHECK_GL_ERROR_NO_INIT
        LOG_E("program link error: %s", g_compile_info);
#if DEBUG || GLOBAL_DEBUG
        abort();
#endif
        GLES.glDeleteShader(shader);
        GLES.glDeleteProgram(program);
        return 0;
    }

    GLES.glDeleteShader(shader);
    CHECK_GL_ERROR_NO_INIT
    g_element_size_loc = GLES.glGetUniformLocation(program, "uElementSize");
    CHECK_GL_ERROR_NO_INIT

    return program;
}

GLAPI GLAPIENTRY void mg_glMultiDrawElementsBaseVertex_compute(GLenum mode, GLsizei* counts, GLenum type,
                                                               const void* const* indices, GLsizei primcount,
                                                               const GLint* basevertex) {
    LOG()
    void prepareForDraw();
    prepareForDraw();

    INIT_CHECK_GL_ERROR

    if (primcount <= 0) return;
    if (!counts || !indices) {
        LOG_E("mg_glMultiDrawElementsBaseVertex_compute: counts or indices is null")
        return;
    }
    if (is_strip_like_mode(mode)) {
        LOG_D("mg_glMultiDrawElementsBaseVertex_compute: strip/loop mode, fallback")
        mg_glMultiDrawElementsBaseVertex_drawelements(mode, counts, type, indices, primcount, basevertex);
        return;
    }

    GLuint elementSize = 0;
    switch (type) {
    case GL_UNSIGNED_BYTE:
        elementSize = 1;
        break;
    case GL_UNSIGNED_SHORT:
        elementSize = 2;
        break;
    case GL_UNSIGNED_INT:
        elementSize = 4;
        break;
    default:
        LOG_E("mg_glMultiDrawElementsBaseVertex_compute: unsupported index type %s", glEnumToString(type))
        mg_glMultiDrawElementsBaseVertex_drawelements(mode, counts, type, indices, primcount, basevertex);
        return;
    }

    // Init compute buffers
    if (!g_compute_inited) {
        LOG_D("Initializing multidraw compute pipeline...")
        GLES.glGenBuffers(1, &g_prefixsumbuffer);
        GLES.glGenBuffers(1, &g_firstidx_ssbo);
        GLES.glGenBuffers(1, &g_basevtx_ssbo);
        GLES.glGenBuffers(1, &g_outputibo);

        g_compute_program = compile_compute_program(multidraw_comp_shader);
        if (g_compute_program == 0) {
            LOG_E("mg_glMultiDrawElementsBaseVertex_compute: compute program init failed, fallback")
            GLES.glDeleteBuffers(1, &g_prefixsumbuffer);
            GLES.glDeleteBuffers(1, &g_firstidx_ssbo);
            GLES.glDeleteBuffers(1, &g_basevtx_ssbo);
            GLES.glDeleteBuffers(1, &g_outputibo);
            g_prefixsumbuffer = 0;
            g_firstidx_ssbo = 0;
            g_basevtx_ssbo = 0;
            g_outputibo = 0;
            mg_glMultiDrawElementsBaseVertex_drawelements(mode, counts, type, indices, primcount, basevertex);
            return;
        }

        g_compute_inited = true;
    }

    GLint ibo = 0;
    GLES.glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &ibo);
    CHECK_GL_ERROR_NO_INIT
    if (ibo == 0) {
        LOG_D("mg_glMultiDrawElementsBaseVertex_compute: no element array buffer bound, fallback")
        mg_glMultiDrawElementsBaseVertex_drawelements(mode, counts, type, indices, primcount, basevertex);
        return;
    }
    GLint ibo_size = 0;
    GLES.glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &ibo_size);
    CHECK_GL_ERROR_NO_INIT
    if (ibo_size <= 0) {
        LOG_E("mg_glMultiDrawElementsBaseVertex_compute: invalid index buffer size, fallback")
        mg_glMultiDrawElementsBaseVertex_drawelements(mode, counts, type, indices, primcount, basevertex);
        return;
    }
    if (elementSize < 4 && (ibo_size % 4) != 0) {
        LOG_E("mg_glMultiDrawElementsBaseVertex_compute: index buffer size not 4-byte aligned, fallback")
        mg_glMultiDrawElementsBaseVertex_drawelements(mode, counts, type, indices, primcount, basevertex);
        return;
    }

    // 动态扩容prefix sum buffer
    size_t sz = g_prefix_sum.size();
    while (sz < static_cast<size_t>(primcount))
        sz *= 2;
    g_prefix_sum.resize(sz);
    
    std::vector<GLuint> first_index(primcount, 0);
    std::vector<GLint> base_vtx(primcount, 0);

    uint64_t running = 0;
    for (GLsizei i = 0; i < primcount; ++i) {
        GLsizei c = counts[i];
        if (c < 0) {
            LOG_E("mg_glMultiDrawElementsBaseVertex_compute: negative count at %d", i)
            c = 0;
        }
        running += static_cast<uint64_t>(c);
        if (running > static_cast<uint64_t>(std::numeric_limits<GLuint>::max())) {
            LOG_E("mg_glMultiDrawElementsBaseVertex_compute: total index count overflow, fallback")
            mg_glMultiDrawElementsBaseVertex_drawelements(mode, counts, type, indices, primcount, basevertex);
            return;
        }
        g_prefix_sum[i] = static_cast<GLuint>(running);

        if (c > 0) {
            if (!indices[i]) {
                LOG_E("mg_glMultiDrawElementsBaseVertex_compute: indices[%d] is null", i)
                mg_glMultiDrawElementsBaseVertex_drawelements(mode, counts, type, indices, primcount, basevertex);
                return;
            }
            uintptr_t byteOffset = reinterpret_cast<uintptr_t>(indices[i]);
            uint64_t byteOffset64 = static_cast<uint64_t>(byteOffset);
            if ((byteOffset % elementSize) != 0) {
                LOG_E("mg_glMultiDrawElementsBaseVertex_compute: misaligned index offset at %d", i)
            }
            if (byteOffset64 > static_cast<uint64_t>(ibo_size)) {
                LOG_E("mg_glMultiDrawElementsBaseVertex_compute: index offset out of range at %d", i)
                mg_glMultiDrawElementsBaseVertex_drawelements(mode, counts, type, indices, primcount, basevertex);
                return;
            }
            uint64_t byteEnd = byteOffset64 + static_cast<uint64_t>(c) * elementSize;
            if (byteEnd > static_cast<uint64_t>(ibo_size)) {
                LOG_E("mg_glMultiDrawElementsBaseVertex_compute: index range out of bounds at %d", i)
                mg_glMultiDrawElementsBaseVertex_drawelements(mode, counts, type, indices, primcount, basevertex);
                return;
            }
            uint64_t elementOffset = byteOffset64 / elementSize;
            if (elementOffset > static_cast<uint64_t>(std::numeric_limits<GLuint>::max())) {
                LOG_E("mg_glMultiDrawElementsBaseVertex_compute: index offset overflow at %d", i)
                mg_glMultiDrawElementsBaseVertex_drawelements(mode, counts, type, indices, primcount, basevertex);
                return;
            }
            first_index[i] = static_cast<GLuint>(elementOffset);
        }

        if (basevertex) {
            base_vtx[i] = basevertex[i];
        }
    }

    auto total_indices = g_prefix_sum[primcount - 1];
    if (total_indices == 0) {
        return;
    }

    GLint prev_ssbo_binding = 0;
    GLES.glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &prev_ssbo_binding);
    CHECK_GL_ERROR_NO_INIT

    // Fill in the data
    GLES.glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_firstidx_ssbo);
    CHECK_GL_ERROR_NO_INIT
    GLES.glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint) * primcount, first_index.data(), GL_DYNAMIC_DRAW);
    CHECK_GL_ERROR_NO_INIT

    GLES.glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_basevtx_ssbo);
    CHECK_GL_ERROR_NO_INIT
    GLES.glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLint) * primcount, base_vtx.data(), GL_DYNAMIC_DRAW);
    CHECK_GL_ERROR_NO_INIT

    GLES.glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_prefixsumbuffer);
    CHECK_GL_ERROR_NO_INIT
    GLES.glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint) * primcount, g_prefix_sum.data(), GL_DYNAMIC_DRAW);
    CHECK_GL_ERROR_NO_INIT

    // Allocate output buffer
    GLES.glBindBuffer(GL_SHADER_STORAGE_BUFFER, g_outputibo);
    CHECK_GL_ERROR_NO_INIT
    GLES.glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint) * total_indices, nullptr, GL_DYNAMIC_DRAW);
    CHECK_GL_ERROR_NO_INIT

    GLES.glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    CHECK_GL_ERROR_NO_INIT

    // Bind buffers
    GLint prev_ssbo_base[5] = {};
    for (int i = 0; i < 5; ++i) {
        GLES.glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, i, &prev_ssbo_base[i]);
        CHECK_GL_ERROR_NO_INIT
    }

    GLES.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ibo);
    CHECK_GL_ERROR_NO_INIT
    GLES.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, g_firstidx_ssbo);
    CHECK_GL_ERROR_NO_INIT
    GLES.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, g_basevtx_ssbo);
    CHECK_GL_ERROR_NO_INIT
    GLES.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, g_prefixsumbuffer);
    CHECK_GL_ERROR_NO_INIT
    GLES.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, g_outputibo);
    CHECK_GL_ERROR_NO_INIT

    // Save states
    GLint prev_program = 0;
    GLES.glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
    CHECK_GL_ERROR_NO_INIT
    GLint prev_vb = 0;
    GLES.glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_vb);
    CHECK_GL_ERROR_NO_INIT

    // Dispatch compute
    LOG_D("Using compute program = %d", g_compute_program)
    GLES.glUseProgram(g_compute_program);
    CHECK_GL_ERROR_NO_INIT
    if (g_element_size_loc >= 0) {
        GLES.glUniform1ui(g_element_size_loc, elementSize);
        CHECK_GL_ERROR_NO_INIT
    }
    LOG_D("Dispatch compute")
    GLES.glDispatchCompute((total_indices + 63) / 64, 1, 1);
    CHECK_GL_ERROR_NO_INIT

    // Wait for compute to complete
    LOG_D("memory barrier")
    GLES.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT);
    CHECK_GL_ERROR_NO_INIT

    // Bind index buffer and do draw
    LOG_D("draw")
    GLES.glUseProgram(prev_program);
    CHECK_GL_ERROR_NO_INIT
    GLES.glBindBuffer(GL_ARRAY_BUFFER, prev_vb);
    CHECK_GL_ERROR_NO_INIT
    GLES.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_outputibo);
    CHECK_GL_ERROR_NO_INIT
    GLES.glDrawElements(mode, total_indices, GL_UNSIGNED_INT, 0);

    // Restore states
    for (int i = 0; i < 5; ++i) {
        GLES.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i, prev_ssbo_base[i]);
        CHECK_GL_ERROR_NO_INIT
    }
    GLES.glBindBuffer(GL_SHADER_STORAGE_BUFFER, prev_ssbo_binding);
    CHECK_GL_ERROR_NO_INIT
    GLES.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    CHECK_GL_ERROR_NO_INIT
}
