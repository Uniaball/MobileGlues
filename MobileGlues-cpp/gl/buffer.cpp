// MobileGlues - gl/buffer.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "buffer.h"
#include "../egl/context.h"
#include <mutex>
#include <unordered_map>
#include <array>
#include "ankerl/unordered_dense.h"
#include "texture.h"

#define DEBUG 0

GLuint bound_array;
static GLint maxBufferId = 0;
static GLint maxArrayId = 0;

// ---------------------------------------------------------------------------
// Per-share-group and per-context storage
//
// GL scopes buffer names to the share group -- two contexts created against each
// other see one set of names -- while vertex array objects and the current
// bindings are container state and belong to the context alone, even inside a
// share group. All of it used to be one process-wide set, so a second context
// inherited the first one's names, sizes and bindings.
//
// The tables stay private to this file and are selected by a thread_local
// pointer that eglMakeCurrent swaps, which is why the ~90 access sites only
// changed shape rather than routing through an accessor on every use.
// std::unordered_map keeps references stable, so these pointers survive the
// insertion of another group.
// ---------------------------------------------------------------------------

namespace {

struct buffer_group_state_t { // shared across a share group
    std::vector<GLuint> gen_buffers;
    std::vector<char> gen_buffer_exists;
    std::vector<GLuint> free_buffer_ids;
    std::vector<size_t> buffer_datasize;
};

struct buffer_ctx_state_t { // private to one context
    std::vector<GLuint> gen_arrays;
    std::vector<char> gen_array_exists;
    std::vector<GLuint> free_array_ids;
    std::vector<GLuint> element_array_buffer_per_vao;
    std::array<GLuint, 13> bound_buffers{};
};

std::mutex g_buf_mutex;
std::unordered_map<unsigned long long, buffer_group_state_t> g_buf_groups;
std::unordered_map<unsigned long long, buffer_ctx_state_t> g_buf_ctxs;

buffer_group_state_t g_buf_group_default;
buffer_ctx_state_t g_buf_ctx_default;

thread_local buffer_group_state_t* g_bg = &g_buf_group_default;
thread_local buffer_ctx_state_t* g_bc = &g_buf_ctx_default;

} // namespace

void mg_buffer_bind_context(unsigned long long ctx_id, unsigned long long group_id) {
    if (ctx_id == 0) {
        g_bg = &g_buf_group_default;
        g_bc = &g_buf_ctx_default;
        return;
    }
    std::lock_guard<std::mutex> lock(g_buf_mutex);
    g_bg = &g_buf_groups[group_id];
    g_bc = &g_buf_ctxs[ctx_id];
}

#define g_gen_buffers (g_bg->gen_buffers)
#define g_gen_buffer_exists (g_bg->gen_buffer_exists)
#define g_free_buffer_ids (g_bg->free_buffer_ids)
#define g_buffer_datasize (g_bg->buffer_datasize)
#define g_gen_arrays (g_bc->gen_arrays)
#define g_gen_array_exists (g_bc->gen_array_exists)
#define g_free_array_ids (g_bc->free_array_ids)
#define g_element_array_buffer_per_vao (g_bc->element_array_buffer_per_vao)

constexpr int INIT_CAPACITY = 1024;

static bool g_buffer_inited = false;
static bool g_array_inited = false;

static GLuint g_cached_vao = 0;
static GLuint g_cached_ibo = 0;

enum BindingIndex : int {
    BI_ARRAY_BUFFER = 0,
    BI_ATOMIC_COUNTER,
    BI_COPY_READ,
    BI_COPY_WRITE,
    BI_DRAW_INDIRECT,
    BI_DISPATCH_INDIRECT,
    BI_ELEMENT_ARRAY,
    BI_PIXEL_PACK,
    BI_PIXEL_UNPACK,
    BI_SHADER_STORAGE,
    BI_TRANSFORM_FEEDBACK,
    BI_UNIFORM_BUFFER,
    BI_PARAMETER_BUFFER,
    BINDING_COUNT
};
#define g_bound_buffers_arr (g_bc->bound_buffers)
static_assert(BINDING_COUNT == 13, "buffer_ctx_state_t::bound_buffers must match BindingIndex");

static inline void ensure_buffer_capacity(GLuint id) {
    if (g_gen_buffers.size() <= id) {
        size_t new_capacity = std::max((size_t)(id + 1), g_gen_buffers.size() * 2);
        g_gen_buffers.resize(new_capacity, 0);
        g_gen_buffer_exists.resize(new_capacity, 0);
        g_buffer_datasize.resize(new_capacity, 0);
    }
}
static inline void ensure_array_capacity(GLuint id) {
    if (g_gen_arrays.size() <= id) {
        size_t new_capacity = std::max((size_t)(id + 1), g_gen_arrays.size() * 2);
        g_gen_arrays.resize(new_capacity, 0);
        g_gen_array_exists.resize(new_capacity, 0);
        g_element_array_buffer_per_vao.resize(new_capacity, 0);
    }
}

void InitBufferMap(size_t expectedSize) {
    if (!g_buffer_inited) {
        size_t reserveSize = std::max(expectedSize + 2, (size_t)INIT_CAPACITY);
        g_gen_buffers.reserve(reserveSize);
        g_gen_buffer_exists.reserve(reserveSize);
        g_buffer_datasize.reserve(reserveSize);
        g_free_buffer_ids.reserve(reserveSize);
        g_gen_buffers.resize(1, 0);
        g_gen_buffer_exists.resize(1, 0);
        g_buffer_datasize.resize(1, 0);
        g_buffer_inited = true;
    }
}

void InitVertexArrayMap(size_t expectedSize) {
    if (!g_array_inited) {
        size_t reserveSize = std::max(expectedSize + 2, (size_t)INIT_CAPACITY);
        g_gen_arrays.reserve(reserveSize);
        g_gen_array_exists.reserve(reserveSize);
        g_element_array_buffer_per_vao.reserve(reserveSize);
        g_free_array_ids.reserve(reserveSize);
        g_gen_arrays.resize(1, 0);
        g_gen_array_exists.resize(1, 0);
        g_element_array_buffer_per_vao.resize(1, 0);
        g_array_inited = true;
    }
}

GLuint gen_buffer() {
    if (!g_free_buffer_ids.empty()) {
        GLuint id = g_free_buffer_ids.back();
        g_free_buffer_ids.pop_back();
        ensure_buffer_capacity(id);
        g_gen_buffers[id] = 0;
        g_gen_buffer_exists[id] = 1;
        g_buffer_datasize[id] = 0;
        if (id > (GLuint)maxBufferId) maxBufferId = id;
        return id;
    }
    maxBufferId++;
    ensure_buffer_capacity((GLuint)maxBufferId);
    g_gen_buffers[maxBufferId] = 0;
    g_gen_buffer_exists[maxBufferId] = 1;
    g_buffer_datasize[maxBufferId] = 0;
    return (GLuint)maxBufferId;
}

GLboolean has_buffer(GLuint key) {
    return key < g_gen_buffer_exists.size() ? (g_gen_buffer_exists[key] != 0) : 0;
}

void modify_buffer(GLuint key, GLuint value) {
    ensure_buffer_capacity(key);
    g_gen_buffers[key] = value;
    g_gen_buffer_exists[key] = 1;
}

void remove_buffer(GLuint key) {
    if (key < g_gen_buffer_exists.size() && g_gen_buffer_exists[key]) {
        g_gen_buffer_exists[key] = 0;
        g_gen_buffers[key] = 0;
        g_buffer_datasize[key] = 0;
        g_free_buffer_ids.push_back(key);
    }
}

GLuint find_real_buffer(GLuint key) {
    if (key < g_gen_buffers.size() && g_gen_buffer_exists[key]) return g_gen_buffers[key];
    return 0;
}

GLuint get_ibo_by_vao(GLuint vao) {
    if (vao < g_element_array_buffer_per_vao.size()) return g_element_array_buffer_per_vao[vao];
    return 0;
}

GLuint find_bound_array() {
    return bound_array;
}

void update_vao_ibo_binding(GLuint vao, GLuint ibo) {
    ensure_array_capacity(vao);
    g_element_array_buffer_per_vao[vao] = ibo;
    if (vao == g_cached_vao) {
        g_cached_ibo = ibo;
    }
}

void set_buffer_data_size(GLuint buffer, size_t size) {
    ensure_buffer_capacity(buffer);
    g_buffer_datasize[buffer] = size;
}

size_t get_buffer_data_size(GLuint buffer) {
    if (buffer < g_buffer_datasize.size()) return g_buffer_datasize[buffer];
    return 0;
}

static inline int binding_target_to_index(GLenum target) {
    switch (target) {
    case GL_ARRAY_BUFFER:
        return BI_ARRAY_BUFFER;
    case GL_ATOMIC_COUNTER_BUFFER:
        return BI_ATOMIC_COUNTER;
    case GL_COPY_READ_BUFFER:
        return BI_COPY_READ;
    case GL_COPY_WRITE_BUFFER:
        return BI_COPY_WRITE;
    case GL_DRAW_INDIRECT_BUFFER:
        return BI_DRAW_INDIRECT;
    case GL_DISPATCH_INDIRECT_BUFFER:
        return BI_DISPATCH_INDIRECT;
    case GL_ELEMENT_ARRAY_BUFFER:
        return BI_ELEMENT_ARRAY;
    case GL_PIXEL_PACK_BUFFER:
        return BI_PIXEL_PACK;
    case GL_PIXEL_UNPACK_BUFFER:
        return BI_PIXEL_UNPACK;
    case GL_SHADER_STORAGE_BUFFER:
        return BI_SHADER_STORAGE;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
        return BI_TRANSFORM_FEEDBACK;
    case GL_UNIFORM_BUFFER:
        return BI_UNIFORM_BUFFER;
    case GL_PARAMETER_BUFFER:
        return BI_PARAMETER_BUFFER;
    default:
        return -1;
    }
}

void set_bound_buffer_by_target(GLenum target, GLuint buffer) {
    int idx = binding_target_to_index(target);
    if (idx >= 0) g_bound_buffers_arr[idx] = buffer;
}

// find_bound_buffer below answers the *_BINDING query enums, which is what
// glGetIntegerv passes it. Callers holding a bind target need this one instead:
// handing a target to find_bound_buffer falls through to its default and comes
// back 0, which is a valid buffer name and so goes unnoticed.
GLuint find_bound_buffer_by_target(GLenum target) {
    if (target == GL_ELEMENT_ARRAY_BUFFER) return get_ibo_by_vao(find_bound_array());
    const int idx = binding_target_to_index(target);
    return idx >= 0 ? g_bound_buffers_arr[idx] : 0;
}

GLuint find_bound_buffer(GLenum key) {
    if (key == GL_ELEMENT_ARRAY_BUFFER_BINDING) {
        return g_cached_ibo;
    }
    GLenum target = 0;
    switch (key) {
    case GL_ARRAY_BUFFER_BINDING:
        target = GL_ARRAY_BUFFER;
        break;
    case GL_ATOMIC_COUNTER_BUFFER_BINDING:
        target = GL_ATOMIC_COUNTER_BUFFER;
        break;
    case GL_COPY_READ_BUFFER_BINDING:
        target = GL_COPY_READ_BUFFER;
        break;
    case GL_COPY_WRITE_BUFFER_BINDING:
        target = GL_COPY_WRITE_BUFFER;
        break;
    case GL_DRAW_INDIRECT_BUFFER_BINDING:
        target = GL_DRAW_INDIRECT_BUFFER;
        break;
    case GL_DISPATCH_INDIRECT_BUFFER_BINDING:
        target = GL_DISPATCH_INDIRECT_BUFFER;
        break;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
        target = GL_ELEMENT_ARRAY_BUFFER;
        break;
    case GL_PIXEL_PACK_BUFFER_BINDING:
        target = GL_PIXEL_PACK_BUFFER;
        break;
    case GL_PIXEL_UNPACK_BUFFER_BINDING:
        target = GL_PIXEL_UNPACK_BUFFER;
        break;
    case GL_SHADER_STORAGE_BUFFER_BINDING:
        target = GL_SHADER_STORAGE_BUFFER;
        break;
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
        target = GL_TRANSFORM_FEEDBACK_BUFFER;
        break;
    case GL_UNIFORM_BUFFER_BINDING:
        target = GL_UNIFORM_BUFFER;
        break;
    case GL_PARAMETER_BUFFER_BINDING:
        target = GL_PARAMETER_BUFFER;
        break;
    default:
        target = 0;
        break;
    }
    if (target == GL_ELEMENT_ARRAY_BUFFER) {
        return get_ibo_by_vao(find_bound_array());
    }
    int idx = binding_target_to_index(target);
    if (idx >= 0) return g_bound_buffers_arr[idx];
    return 0;
}

GLuint gen_array() {
    if (!g_free_array_ids.empty()) {
        GLuint id = g_free_array_ids.back();
        g_free_array_ids.pop_back();
        ensure_array_capacity(id);
        g_gen_arrays[id] = 0;
        g_gen_array_exists[id] = 1;
        g_element_array_buffer_per_vao[id] = 0;
        if (id > (GLuint)maxArrayId) maxArrayId = id;
        return id;
    }
    maxArrayId++;
    ensure_array_capacity((GLuint)maxArrayId);
    g_gen_arrays[maxArrayId] = 0;
    g_gen_array_exists[maxArrayId] = 1;
    g_element_array_buffer_per_vao[maxArrayId] = 0;
    return (GLuint)maxArrayId;
}

GLboolean has_array(GLuint key) {
    return key < g_gen_array_exists.size() ? (g_gen_array_exists[key] != 0) : 0;
}

void modify_array(GLuint key, GLuint value) {
    ensure_array_capacity(key);
    g_gen_arrays[key] = value;
    g_gen_array_exists[key] = 1;
}

void remove_array(GLuint key) {
    if (key < g_gen_array_exists.size() && g_gen_array_exists[key]) {
        g_gen_array_exists[key] = 0;
        g_gen_arrays[key] = 0;
        g_element_array_buffer_per_vao[key] = 0;
        g_free_array_ids.push_back(key);
    }
}

GLuint find_real_array(GLuint key) {
    if (key < g_gen_arrays.size() && g_gen_array_exists[key]) return g_gen_arrays[key];
    return 0;
}

static GLenum get_binding_query(GLenum target) {
    switch (target) {
    case GL_ARRAY_BUFFER: return GL_ARRAY_BUFFER_BINDING;
    case GL_ELEMENT_ARRAY_BUFFER: return GL_ELEMENT_ARRAY_BUFFER_BINDING;
    case GL_PIXEL_PACK_BUFFER: return GL_PIXEL_PACK_BUFFER_BINDING;
    case GL_PIXEL_UNPACK_BUFFER: return GL_PIXEL_UNPACK_BUFFER_BINDING;
    case GL_COPY_WRITE_BUFFER: return GL_COPY_WRITE_BUFFER_BINDING;
    case GL_COPY_READ_BUFFER: return GL_COPY_READ_BUFFER_BINDING;
    case GL_UNIFORM_BUFFER: return GL_UNIFORM_BUFFER_BINDING;
    case GL_SHADER_STORAGE_BUFFER: return GL_SHADER_STORAGE_BUFFER_BINDING;
    case GL_TRANSFORM_FEEDBACK_BUFFER: return GL_TRANSFORM_FEEDBACK_BUFFER_BINDING;
    case GL_ATOMIC_COUNTER_BUFFER: return GL_ATOMIC_COUNTER_BUFFER_BINDING;
    case GL_DRAW_INDIRECT_BUFFER: return GL_DRAW_INDIRECT_BUFFER_BINDING;
    case GL_DISPATCH_INDIRECT_BUFFER: return GL_DISPATCH_INDIRECT_BUFFER_BINDING;
    default: return 0;
    }
}

void glGenBuffers(GLsizei n, GLuint* buffers) {
    LOG()
    LOG_D("glGenBuffers(%i, %p)", n, buffers)
    ensure_buffer_capacity(maxBufferId + n);
    for (int i = 0; i < n; ++i) {
        buffers[i] = gen_buffer();
    }
}

void glDeleteBuffers(GLsizei n, const GLuint* buffers) {
    LOG()
    LOG_D("glDeleteBuffers(%i, %p)", n, buffers)
    for (int i = 0; i < n; ++i) {
        // GL resets a binding to 0 when the bound buffer is deleted. The
        // parameter buffer slot is the only source of truth gl/multidraw.cpp has
        // for where the draw count lives -- there is no driver-side binding to
        // cross-check it against -- and deleted ids are recycled by gen_buffer(),
        // so a stale slot would silently point at somebody else's buffer.
        if (buffers[i] != 0 && find_bound_buffer(GL_PARAMETER_BUFFER_BINDING) == buffers[i]) {
            set_bound_buffer_by_target(GL_PARAMETER_BUFFER, 0);
        }
        if (find_real_buffer(buffers[i])) {
            GLuint real_buff = find_real_buffer(buffers[i]);
            GLES.glDeleteBuffers(1, &real_buff);
            CHECK_GL_ERROR
        }
        remove_buffer(buffers[i]);
    }
}

GLboolean glIsBuffer(GLuint buffer) {
    LOG()
    LOG_D("glIsBuffer, buffer = %d", buffer)
    return has_buffer(buffer);
}

void glBindBuffer(GLenum target, GLuint buffer) {
    LOG()
    LOG_D("glBindBuffer, target = %s, buffer = %d", glEnumToString(target), buffer)
    set_bound_buffer_by_target(target, buffer);

    if (target == GL_PARAMETER_BUFFER) {
        // GLES has no GL_PARAMETER_BUFFER. The binding is tracked here and read
        // back by gl/multidraw.cpp for glMultiDraw*IndirectCount; forwarding the
        // target to the driver would only raise GL_INVALID_ENUM. The backing
        // object still has to exist, because nothing else will create it.
        if (buffer != 0 && has_buffer(buffer) && !find_real_buffer(buffer)) {
            GLuint real_buffer = 0;
            GLES.glGenBuffers(1, &real_buffer);
            modify_buffer(buffer, real_buffer);
            CHECK_GL_ERROR
        }
        return;
    }

    // save ibo binding to vao
    if (target == GL_ELEMENT_ARRAY_BUFFER) {
        update_vao_ibo_binding(find_bound_array(), buffer);
    }

    if (!has_buffer(buffer) || buffer == 0) {
        GLES.glBindBuffer(target, buffer);
        CHECK_GL_ERROR
        return;
    }
    GLuint real_buffer = find_real_buffer(buffer);
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer(buffer, real_buffer);
        CHECK_GL_ERROR
    }
    LOG_D("glBindBuffer: %d -> %d", buffer, real_buffer)
    GLES.glBindBuffer(target, real_buffer);
    CHECK_GL_ERROR
}

struct atomic_buffer {
    GLuint id;
    GLsizeiptr size;
    GLintptr offset;
};

static std::vector<atomic_buffer> g_buffer_map_atomic_buffer_info;
static std::vector<GLuint> g_buffer_map_ssbo_id;

GLuint find_bound_ssbo_at_index(GLuint index) {
    if (g_buffer_map_ssbo_id.empty() || index >= g_buffer_map_ssbo_id.size()) {
        return 0;
    }
    return g_buffer_map_ssbo_id[index];
}

void bindAllAtomicCounterAsSSBO() {
    const size_t count = g_buffer_map_atomic_buffer_info.size();
    for (size_t i = 0; i < count; ++i) {
        atomic_buffer buf = g_buffer_map_atomic_buffer_info[i];
        if (buf.id != 0) {
            GLuint realID = find_real_buffer(buf.id);
            GLES.glBindBufferRange(GL_SHADER_STORAGE_BUFFER, i, realID, buf.offset, buf.size);
            LOG_D("Bound atomic counter buffer %u(real: %u) as SSBO at index %zu", buf, realID, i);
        }
    }
}

void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    LOG()
    LOG_D("glBindBufferRange, target = %s, index = %d, buffer = %d, offset = %p, size = %zi", glEnumToString(target),
          index, buffer, (void*)offset, size)

    if (!has_buffer(buffer) || buffer == 0) {
        GLES.glBindBufferRange(target, index, buffer, offset, size);
        if (target == GL_SHADER_STORAGE_BUFFER) {
            if (g_buffer_map_ssbo_id.empty()) {
                g_buffer_map_ssbo_id.resize(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, 0);
            }
            g_buffer_map_ssbo_id[index] = 0;
        }
        CHECK_GL_ERROR
        return;
    }
    GLuint real_buffer = find_real_buffer(buffer);
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer(buffer, real_buffer);
        CHECK_GL_ERROR
    }
    GLES.glBindBufferRange(target, index, real_buffer, offset, size);
    if (target == GL_ATOMIC_COUNTER_BUFFER && global_settings.ext_shader_atomic_counters) {
        if (g_buffer_map_atomic_buffer_info.empty()) {
            g_buffer_map_atomic_buffer_info.resize(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, {});
        }
        g_buffer_map_atomic_buffer_info[index] = {buffer, size, offset};
    } else if (target == GL_SHADER_STORAGE_BUFFER) {
        if (g_buffer_map_ssbo_id.empty()) {
            g_buffer_map_ssbo_id.resize(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, 0);
        }
        g_buffer_map_ssbo_id[index] = buffer;
    }
    CHECK_GL_ERROR
}

void glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    LOG()
    LOG_D("glBindBufferBase, target = %s, index = %d, buffer = %d", glEnumToString(target), index, buffer)

    if (!has_buffer(buffer) || buffer == 0) {
        GLES.glBindBufferBase(target, index, buffer);
        if (target == GL_SHADER_STORAGE_BUFFER) {
            if (g_buffer_map_ssbo_id.empty()) {
                g_buffer_map_ssbo_id.resize(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, 0);
            }
            g_buffer_map_ssbo_id[index] = 0;
        }
        CHECK_GL_ERROR
        return;
    }
    GLuint real_buffer = find_real_buffer(buffer);
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer(buffer, real_buffer);
        CHECK_GL_ERROR
    }
    GLES.glBindBufferBase(target, index, real_buffer);
    if (target == GL_SHADER_STORAGE_BUFFER) {
        if (g_buffer_map_ssbo_id.empty()) {
            g_buffer_map_ssbo_id.resize(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, 0);
        }
        g_buffer_map_ssbo_id[index] = buffer;
    }
    CHECK_GL_ERROR
}

void glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
    LOG()
    LOG_D("glBindVertexBuffer, bindingindex = %d, buffer = %d, offset = %p, stride = %i", bindingindex, buffer, offset,
          stride)
    if (!has_buffer(buffer) || buffer == 0) {
        GLES.glBindVertexBuffer(bindingindex, buffer, offset, stride);
        CHECK_GL_ERROR
        return;
    }
    GLuint real_buffer = find_real_buffer(buffer);
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer(buffer, real_buffer);
        CHECK_GL_ERROR
    }
    GLES.glBindVertexBuffer(bindingindex, real_buffer, offset, stride);
    CHECK_GL_ERROR
}

size_t get_internal_format_size(GLenum internalformat) {
    switch (internalformat) {
    case GL_R8: return 1;
    case GL_R8I:
    case GL_R8UI: return 1;
    case GL_R16:
    case GL_R16I:
    case GL_R16UI:
    case GL_R16F: return 2;
    case GL_R32I:
    case GL_R32UI:
    case GL_R32F: return 4;

    case GL_RG8: return 2;
    case GL_RG8I:
    case GL_RG8UI: return 2;
    case GL_RG16: return 4;
    case GL_RG16I:
    case GL_RG16UI:
    case GL_RG16F: return 4;
    case GL_RG32I:
    case GL_RG32UI:
    case GL_RG32F: return 8;

    case GL_RGB8: return 3;
    case GL_RGB8I:
    case GL_RGB8UI: return 3;
    case GL_RGB16: return 6;
    case GL_RGB16I:
    case GL_RGB16UI:
    case GL_RGB16F: return 6;
    case GL_RGB32I:
    case GL_RGB32UI:
    case GL_RGB32F: return 12;

    case GL_RGBA8: return 4;
    case GL_RGBA8I:
    case GL_RGBA8UI: return 4;
    case GL_RGBA16: return 8;
    case GL_RGBA16I:
    case GL_RGBA16UI:
    case GL_RGBA16F: return 8;
    case GL_RGBA32I:
    case GL_RGBA32UI:
    case GL_RGBA32F: return 16;

    case GL_DEPTH_COMPONENT16: return 2;
    case GL_DEPTH_COMPONENT24: return 3;
    case GL_DEPTH_COMPONENT32: return 4;
    case GL_DEPTH_COMPONENT32F: return 4;
    case GL_DEPTH24_STENCIL8: return 4;
    case GL_DEPTH32F_STENCIL8: return 5;

    case GL_STENCIL_INDEX8: return 1;

    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: return 8;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: return 16;

    default:
        LOG_E("Unknown internal format size for %s", glEnumToString(internalformat));
        return 0;
    }
}

extern std::string bufSampelerName;
void glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer) {
    LOG()
    LOG_D("glTexBuffer, target = %s, internalformat = %s, buffer = %d", glEnumToString(target),
          glEnumToString(internalformat), buffer)
    if (target != GL_TEXTURE_BUFFER) return;

    if (!has_buffer(buffer) || buffer == 0) {
        GLES.glTexBuffer(target, internalformat, buffer);
        CHECK_GL_ERROR
        return;
    }
    GLuint real_buffer = find_real_buffer(buffer);
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer(buffer, real_buffer);
        CHECK_GL_ERROR
    }

    if (hardware->emulate_texture_buffer) {
        LOG_D("Emulating glTexBuffer");
        GLint boundTexture = 0;
        GLint prev_pixel_buffer_binding = 0;
        GLES.glActiveTexture(GL_TEXTURE0 + 15);
        GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
        LOG_D("Current GL_TEXTURE_BINDING_BUFFER = %d", boundTexture);
        GLES.glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &prev_pixel_buffer_binding);
        LOG_D("Previous GL_PIXEL_UNPACK_BUFFER_BINDING = %d", prev_pixel_buffer_binding);
        if (!boundTexture) {
            LOG_D("No texture bound to GL_TEXTURE_BUFFER, skipping emulation.");
            return;
        }
        GLES.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, real_buffer);
        LOG_D("Bound GL_PIXEL_UNPACK_BUFFER to buffer %u", real_buffer);
        GLint bufferSize;
        GLES.glGetBufferParameteriv(GL_PIXEL_UNPACK_BUFFER, GL_BUFFER_SIZE, &bufferSize);
        LOG_D("Buffer size = %d bytes", bufferSize);
        GLES.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        GLES.glBindTexture(GL_TEXTURE_2D, boundTexture);
        LOG_D("Binding texture %u to GL_TEXTURE_2D", boundTexture);

        const GLuint MAX_WIDTH = 8192;
        GLuint pixelSize = get_internal_format_size(internalformat);
        GLuint numElements = bufferSize / pixelSize;
        GLuint width = numElements;
        GLuint height = 1;
        if (width > MAX_WIDTH) {
            width = MAX_WIDTH;
            height = (numElements + MAX_WIDTH - 1) / MAX_WIDTH;
        }

        GLint prev_alignment, prev_row_length, prev_skip_pixels, prev_skip_rows;
        GLES.glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_alignment);
        GLES.glGetIntegerv(GL_UNPACK_ROW_LENGTH, &prev_row_length);
        GLES.glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &prev_skip_pixels);
        GLES.glGetIntegerv(GL_UNPACK_SKIP_ROWS, &prev_skip_rows);

        GLES.glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        GLES.glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

        GLES.glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, GL_RED_INTEGER, GL_BYTE, nullptr);

        GLES.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, real_buffer);

        if (height == 1) {
            GLES.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, 1, GL_RED_INTEGER, GL_BYTE, nullptr);
        } else {
            for (GLuint row = 0; row < height; ++row) {
                void* offset = (void*)(row * width * pixelSize);
                GLES.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, row, width, 1, GL_RED_INTEGER, GL_BYTE, offset);
            }
        }

        GLES.glPixelStorei(GL_UNPACK_ALIGNMENT, prev_alignment);
        GLES.glPixelStorei(GL_UNPACK_ROW_LENGTH, prev_row_length);
        GLES.glPixelStorei(GL_UNPACK_SKIP_PIXELS, prev_skip_pixels);
        GLES.glPixelStorei(GL_UNPACK_SKIP_ROWS, prev_skip_rows);

        auto tex = mgGetTexObjectByTarget(target);
        tex->target = ConvertGLEnumToTextureTarget(target);
        tex->internal_format = internalformat;
        tex->width = width;
        tex->height = height;
        tex->depth = 1;
        tex->swizzle_param[0] = GL_RED;
        tex->swizzle_param[1] = GL_GREEN;
        tex->swizzle_param[2] = GL_BLUE;
        tex->swizzle_param[3] = GL_ALPHA;

        LOG_D("Called glTexImage2D with internalformat = 0x%X", internalformat);

        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        LOG_D("Set texture parameters: MIN_FILTER=NEAREST, MAG_FILTER=NEAREST, WRAP_S/T=CLAMP_TO_EDGE");

        GLES.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, prev_pixel_buffer_binding);

        GLES.glActiveTexture(GL_TEXTURE0 + gl_state->current_tex_unit);

        LOG_D("Restored bindings: GL_PIXEL_UNPACK_BUFFER=%d", prev_pixel_buffer_binding);

        CHECK_GL_ERROR;
        return;
    }

    GLES.glTexBuffer(target, internalformat, real_buffer);
    CHECK_GL_ERROR
}

void glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    LOG()
    LOG_D("glTexBufferRange, target = %s, internalformat = %s, buffer = %d, offset = %p, size = %zi",
          glEnumToString(target), glEnumToString(internalformat), buffer, (void*)offset, size)
    if (!has_buffer(buffer) || buffer == 0) {
        GLES.glTexBufferRange(target, internalformat, buffer, offset, size);
        CHECK_GL_ERROR
        return;
    }
    GLuint real_buffer = find_real_buffer(buffer);
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer(buffer, real_buffer);
        CHECK_GL_ERROR
    }
    GLES.glTexBufferRange(target, internalformat, real_buffer, offset, size);
    CHECK_GL_ERROR
}

// GLES has no GL_PARAMETER_BUFFER, so glBindBuffer above tracks the binding
// without ever handing that target to the driver. GL 4.6 still lets an
// application fill and query the buffer through it, which used to reach GLES
// verbatim and come back GL_INVALID_ENUM -- the buffer stayed empty, and
// glMultiDraw*IndirectCount then found a zero-byte parameter buffer and drew
// nothing.
//
// GL_COPY_WRITE_BUFFER is borrowed for the duration of one call and put back
// afterwards. GLES defines it as a generic target with no meaning of its own, so
// the swap is invisible: nothing observes it, and no draw depends on it.
namespace {
struct borrowed_target_t {
    GLenum target;
    GLint saved = 0;
    bool borrowed = false;

    explicit borrowed_target_t(GLenum requested) : target(requested) {
        if (requested != GL_PARAMETER_BUFFER) return;
        const GLuint real = find_real_buffer(find_bound_buffer(GL_PARAMETER_BUFFER_BINDING));
        GLES.glGetIntegerv(GL_COPY_WRITE_BUFFER_BINDING, &saved);
        GLES.glBindBuffer(GL_COPY_WRITE_BUFFER, real);
        target = GL_COPY_WRITE_BUFFER;
        borrowed = true;
    }
    ~borrowed_target_t() {
        if (borrowed) GLES.glBindBuffer(GL_COPY_WRITE_BUFFER, static_cast<GLuint>(saved));
    }

    borrowed_target_t(const borrowed_target_t&) = delete;
    borrowed_target_t& operator=(const borrowed_target_t&) = delete;
};
} // namespace

void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    LOG()
    LOG_D("glBufferData, target = %s, size = %d, data = 0x%x, usage = %s", glEnumToString(target), size, data,
          glEnumToString(usage))
    borrowed_target_t t(target);
    GLES.glBufferData(t.target, size, data, usage);
    set_buffer_data_size(find_bound_buffer_by_target(target), size);
    CHECK_GL_ERROR
}

// Both of these were plain pass-throughs in gl/gl_native.cpp. They live here now
// so that GL_PARAMETER_BUFFER reaches the driver as a target it understands.
void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
    LOG()
    LOG_D("glBufferSubData, target = %s, offset = %p, size = %zi", glEnumToString(target), (void*)offset, size)
    borrowed_target_t t(target);
    GLES.glBufferSubData(t.target, offset, size, data);
    CHECK_GL_ERROR
}

void glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params) {
    LOG()
    LOG_D("glGetBufferParameteriv, target = %s, pname = %s", glEnumToString(target), glEnumToString(pname))
    borrowed_target_t t(target);
    GLES.glGetBufferParameteriv(t.target, pname, params);
    CHECK_GL_ERROR
}

void* glMapBuffer(GLenum target, GLenum access) {
    LOG()
    LOG_D("glMapBuffer, target = %s, access = %s", glEnumToString(target), glEnumToString(access))
    if (g_gles_caps.GL_OES_mapbuffer) {
        borrowed_target_t t(target);
        return GLES.glMapBufferOES(t.target, access);
    }
    GLint buffer_size;
    glGetBufferParameteriv(target, GL_BUFFER_SIZE, &buffer_size);
    if (buffer_size <= 0 || glGetError() != GL_NO_ERROR) {
        return nullptr;
    }
    GLbitfield flags = 0;
    switch (access) {
    case GL_READ_ONLY: flags = GL_MAP_READ_BIT; break;
    case GL_WRITE_ONLY: flags = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT; break;
    case GL_READ_WRITE: flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT; break;
    default: return nullptr;
    }
    void* ptr = glMapBufferRange(target, 0, buffer_size, flags);
    return ptr;
}

#if GLOBAL_DEBUG || DEBUG
#include <fstream>
#define BIN_FILE_PREFIX "/sdcard/MG/buf/"
#endif

#if !defined(__APPLE__)
extern "C"
{
    GLAPI GLAPIENTRY void* glMapBufferARB(GLenum target, GLenum access) __attribute__((alias("glMapBuffer")));
    GLAPI GLAPIENTRY void glBufferDataARB(GLenum target, GLsizeiptr size, const void* data, GLenum usage)
        __attribute__((alias("glBufferData")));
    GLAPI GLAPIENTRY GLboolean glUnmapBufferARB(GLenum target) __attribute__((alias("glUnmapBuffer")));
    GLAPI GLAPIENTRY void glBufferStorageARB(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags)
        __attribute__((alias("glBufferStorage")));
    GLAPI GLAPIENTRY void glBindBufferARB(GLenum target, GLuint buffer) __attribute__((alias("glBindBuffer")));
}
#endif

void* glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    LOG()
    if (global_settings.buffer_coherent_as_flush) access &= ~GL_MAP_FLUSH_EXPLICIT_BIT;
    //    access |= GL_MAP_UNSYNCHRONIZED_BIT;
    borrowed_target_t t(target);
    return GLES.glMapBufferRange(t.target, offset, length, access);
}

GLboolean glUnmapBuffer(GLenum target) {
    LOG()
    LOG_D("%s(%s)", __func__, glEnumToString(target));
    borrowed_target_t t(target);
    if (g_gles_caps.GL_OES_mapbuffer) return GLES.glUnmapBuffer(t.target);

    GLboolean result = GLES.glUnmapBuffer(t.target);
    CHECK_GL_ERROR
    return result;
}

void glBufferStorage(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags) {
    LOG()
    if (GLES.glBufferStorageEXT) {
        if (global_settings.buffer_coherent_as_flush &&
            ((flags & GL_MAP_PERSISTENT_BIT) != 0 || (flags & GL_DYNAMIC_STORAGE_BIT) != 0))
            flags |= (GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT);
        borrowed_target_t t(target);
        GLES.glBufferStorageEXT(t.target, size, data, flags);
        // Allocates storage just as glBufferData does, so it owes the same record.
        set_buffer_data_size(find_bound_buffer_by_target(target), size);
    }
    CHECK_GL_ERROR
}

void glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    LOG()
    if (!global_settings.buffer_coherent_as_flush) {
        borrowed_target_t t(target);
        GLES.glFlushMappedBufferRange(t.target, offset, length);
    }
}

void glGenVertexArrays(GLsizei n, GLuint* arrays) {
    LOG()
    LOG_D("glGenVertexArrays(%i, %p)", n, arrays)
    ensure_array_capacity(maxArrayId + n);
    for (int i = 0; i < n; ++i) {
        arrays[i] = gen_array();
    }
}

void glDeleteVertexArrays(GLsizei n, const GLuint* arrays) {
    LOG()
    LOG_D("glDeleteVertexArrays(%i, %p)", n, arrays)
    for (int i = 0; i < n; ++i) {
        if (find_real_array(arrays[i])) {
            GLuint real_array = find_real_array(arrays[i]);
            GLES.glDeleteVertexArrays(1, &real_array);
            CHECK_GL_ERROR
        }
        remove_array(arrays[i]);
    }
}

GLboolean glIsVertexArray(GLuint array) {
    LOG()
    LOG_D("glIsVertexArray(%d)", array)
    return has_array(array);
}

void glBindVertexArray(GLuint array) {
    LOG()
    LOG_D("glBindVertexArray(%d)", array)
    bound_array = array;
    
    g_cached_vao = array;
    g_cached_ibo = get_ibo_by_vao(array);
    
    set_bound_buffer_by_target(GL_ELEMENT_ARRAY_BUFFER, g_cached_ibo);

    if (!has_array(array) || array == 0) {
        LOG_D("Does not have va=%d found!", array)
        GLES.glBindVertexArray(array);
        CHECK_GL_ERROR
        return;
    }
    GLuint real_array = find_real_array(array);
    if (!real_array) {
        LOG_D("va=%d not initialized, initializing...", array)
        GLES.glGenVertexArrays(1, &real_array);
        modify_array(array, real_array);
        CHECK_GL_ERROR
    }
    LOG_D("glBindVertexArray: %d -> %d", array, real_array)
    GLES.glBindVertexArray(real_array);
    CHECK_GL_ERROR
}