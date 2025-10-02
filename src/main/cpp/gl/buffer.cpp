//
// Created by BZLZHH on 2025/1/28.
//

#include "buffer.h"
#include "ankerl/unordered_dense.h"
#include "texture.h"

#define DEBUG 0

GLuint bound_array;
static GLint maxBufferId = 0;
static GLint maxArrayId = 0;

static std::vector<GLuint> g_gen_buffers;
static std::vector<uint8_t> g_gen_buffer_exists; // 优化: 用uint8_t代替char
static std::vector<GLuint> g_free_buffer_ids;

static std::vector<GLuint> g_gen_arrays;
static std::vector<uint8_t> g_gen_array_exists; // 优化: 用uint8_t代替char
static std::vector<GLuint> g_free_array_ids;

static std::vector<size_t> g_buffer_datasize;
static std::vector<GLuint> g_element_array_buffer_per_vao;

constexpr int INIT_CAPACITY = 1024;

static bool g_buffer_inited = false;
static bool g_array_inited = false;

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
    BINDING_COUNT
};
static std::array<GLuint, BINDING_COUNT> g_bound_buffers_arr = {0};

// --- 优化: 分支减少，指数扩容+对齐 ---
static inline void ensure_buffer_capacity(GLuint id) {
    size_t want = (size_t)id + 1;
    if (g_gen_buffers.size() < want) {
        size_t new_capacity = want;
        if (new_capacity < g_gen_buffers.size() * 2 + 64) new_capacity = g_gen_buffers.size() * 2 + 64;
        g_gen_buffers.resize(new_capacity, 0);
        g_gen_buffer_exists.resize(new_capacity, 0);
        g_buffer_datasize.resize(new_capacity, 0);
    }
}

static inline void ensure_array_capacity(GLuint id) {
    size_t want = (size_t)id + 1;
    if (g_gen_arrays.size() < want) {
        size_t new_capacity = want;
        if (new_capacity < g_gen_arrays.size() * 2 + 64) new_capacity = g_gen_arrays.size() * 2 + 64;
        g_gen_arrays.resize(new_capacity, 0);
        g_gen_array_exists.resize(new_capacity, 0);
        g_element_array_buffer_per_vao.resize(new_capacity, 0);
    }
}

// --- 优化: 初始化时一次性预分配较大空间 ---
void InitBufferMap(size_t expectedSize) {
    if (!g_buffer_inited) {
        size_t reserveSize = std::max(expectedSize + 2, (size_t)INIT_CAPACITY);
        g_gen_buffers.reserve(reserveSize);
        g_gen_buffer_exists.reserve(reserveSize);
        g_buffer_datasize.reserve(reserveSize);
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
        if (key < g_buffer_datasize.size()) g_buffer_datasize[key] = 0;
        g_free_buffer_ids.push_back(key);
    }
}

GLuint find_real_buffer(GLuint key) {
    return (key < g_gen_buffers.size() && g_gen_buffer_exists[key]) ? g_gen_buffers[key] : 0;
}

GLuint get_ibo_by_vao(GLuint vao) {
    return (vao < g_element_array_buffer_per_vao.size()) ? g_element_array_buffer_per_vao[vao] : 0;
}

GLuint find_bound_array() {
    return bound_array;
}

void update_vao_ibo_binding(GLuint vao, GLuint ibo) {
    ensure_array_capacity(vao);
    g_element_array_buffer_per_vao[vao] = ibo;
}

void set_buffer_data_size(GLuint buffer, size_t size) {
    ensure_buffer_capacity(buffer);
    g_buffer_datasize[buffer] = size;
}

size_t get_buffer_data_size(GLuint buffer) {
    return (buffer < g_buffer_datasize.size()) ? g_buffer_datasize[buffer] : 0;
}

static inline int binding_target_to_index(GLenum target) {
    switch (target) {
    case GL_ARRAY_BUFFER: return BI_ARRAY_BUFFER;
    case GL_ATOMIC_COUNTER_BUFFER: return BI_ATOMIC_COUNTER;
    case GL_COPY_READ_BUFFER: return BI_COPY_READ;
    case GL_COPY_WRITE_BUFFER: return BI_COPY_WRITE;
    case GL_DRAW_INDIRECT_BUFFER: return BI_DRAW_INDIRECT;
    case GL_DISPATCH_INDIRECT_BUFFER: return BI_DISPATCH_INDIRECT;
    case GL_ELEMENT_ARRAY_BUFFER: return BI_ELEMENT_ARRAY;
    case GL_PIXEL_PACK_BUFFER: return BI_PIXEL_PACK;
    case GL_PIXEL_UNPACK_BUFFER: return BI_PIXEL_UNPACK;
    case GL_SHADER_STORAGE_BUFFER: return BI_SHADER_STORAGE;
    case GL_TRANSFORM_FEEDBACK_BUFFER: return BI_TRANSFORM_FEEDBACK;
    case GL_UNIFORM_BUFFER: return BI_UNIFORM_BUFFER;
    default: return -1;
    }
}

void set_bound_buffer_by_target(GLenum target, GLuint buffer) {
    int idx = binding_target_to_index(target);
    if (idx >= 0) g_bound_buffers_arr[idx] = buffer;
}

GLuint find_bound_buffer(GLenum key) {
    GLenum target = 0;
    switch (key) {
    case GL_ARRAY_BUFFER_BINDING: target = GL_ARRAY_BUFFER; break;
    case GL_ATOMIC_COUNTER_BUFFER_BINDING: target = GL_ATOMIC_COUNTER_BUFFER; break;
    case GL_COPY_READ_BUFFER_BINDING: target = GL_COPY_READ_BUFFER; break;
    case GL_COPY_WRITE_BUFFER_BINDING: target = GL_COPY_WRITE_BUFFER; break;
    case GL_DRAW_INDIRECT_BUFFER_BINDING: target = GL_DRAW_INDIRECT_BUFFER; break;
    case GL_DISPATCH_INDIRECT_BUFFER_BINDING: target = GL_DISPATCH_INDIRECT_BUFFER; break;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING: target = GL_ELEMENT_ARRAY_BUFFER; break;
    case GL_PIXEL_PACK_BUFFER_BINDING: target = GL_PIXEL_PACK_BUFFER; break;
    case GL_PIXEL_UNPACK_BUFFER_BINDING: target = GL_PIXEL_UNPACK_BUFFER; break;
    case GL_SHADER_STORAGE_BUFFER_BINDING: target = GL_SHADER_STORAGE_BUFFER; break;
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING: target = GL_TRANSFORM_FEEDBACK_BUFFER; break;
    case GL_UNIFORM_BUFFER_BINDING: target = GL_UNIFORM_BUFFER; break;
    default: target = 0; break;
    }
    if (target == GL_ELEMENT_ARRAY_BUFFER) {
        return get_ibo_by_vao(find_bound_array());
    }
    int idx = binding_target_to_index(target);
    return (idx >= 0) ? g_bound_buffers_arr[idx] : 0;
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
        if (key < g_element_array_buffer_per_vao.size()) g_element_array_buffer_per_vao[key] = 0;
        g_free_array_ids.push_back(key);
    }
}

GLuint find_real_array(GLuint key) {
    return (key < g_gen_arrays.size() && g_gen_array_exists[key]) ? g_gen_arrays[key] : 0;
}

// --- glGenBuffers/glGenVertexArrays 批量分配优化: 批量resize减少循环内resize ---
void glGenBuffers(GLsizei n, GLuint* buffers) {
    ensure_buffer_capacity(maxBufferId + n);
    for (int i = 0; i < n; ++i) {
        buffers[i] = gen_buffer();
    }
}

void glDeleteBuffers(GLsizei n, const GLuint* buffers) {
    for (int i = 0; i < n; ++i) {
        GLuint real_buff = find_real_buffer(buffers[i]);
        if (real_buff) {
            GLES.glDeleteBuffers(1, &real_buff);
        }
        remove_buffer(buffers[i]);
    }
}

GLboolean glIsBuffer(GLuint buffer) {
    return has_buffer(buffer);
}

void glBindBuffer(GLenum target, GLuint buffer) {
    set_bound_buffer_by_target(target, buffer);
    if (target == GL_ELEMENT_ARRAY_BUFFER) {
        update_vao_ibo_binding(find_bound_array(), buffer);
    }
    if (!has_buffer(buffer) || buffer == 0) {
        GLES.glBindBuffer(target, buffer);
        return;
    }
    GLuint real_buffer = find_real_buffer(buffer);
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer(buffer, real_buffer);
    }
    GLES.glBindBuffer(target, real_buffer);
}

struct atomic_buffer {
    GLuint id;
    GLsizeiptr size;
    GLintptr offset;
};

static std::vector<atomic_buffer> g_buffer_map_atomic_buffer_info;
static std::vector<GLuint> g_buffer_map_ssbo_id;

void bindAllAtomicCounterAsSSBO() {
    for (size_t i = 0; i < g_buffer_map_atomic_buffer_info.size(); ++i) {
        atomic_buffer buf = g_buffer_map_atomic_buffer_info[i];
        if (buf.id != 0) {
            GLuint realID = find_real_buffer(buf.id);
            GLES.glBindBufferRange(GL_SHADER_STORAGE_BUFFER, i, realID, buf.offset, buf.size);
        }
    }
}

void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    if (!has_buffer(buffer) || buffer == 0) {
        GLES.glBindBufferRange(target, index, buffer, offset, size);
        return;
    }
    GLuint real_buffer = find_real_buffer(buffer);
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer(buffer, real_buffer);
    }
    GLES.glBindBufferRange(target, index, real_buffer, offset, size);
    if (target == GL_ATOMIC_COUNTER_BUFFER) {
        if (g_buffer_map_atomic_buffer_info.empty()) {
            g_buffer_map_atomic_buffer_info.resize(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, {});
        }
        g_buffer_map_atomic_buffer_info[index] = {buffer, size, offset};
    }
}

void glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    if (!has_buffer(buffer) || buffer == 0) {
        GLES.glBindBufferBase(target, index, buffer);
        return;
    }
    GLuint real_buffer = find_real_buffer(buffer);
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer(buffer, real_buffer);
    }
    GLES.glBindBufferBase(target, index, real_buffer);
    if (target == GL_SHADER_STORAGE_BUFFER) {
        if (g_buffer_map_ssbo_id.empty()) {
            g_buffer_map_ssbo_id.resize(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, 0);
        }
        g_buffer_map_ssbo_id[index] = buffer;
    }
}

void glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
    if (!has_buffer(buffer) || buffer == 0) {
        GLES.glBindVertexBuffer(bindingindex, buffer, offset, stride);
        return;
    }
    GLuint real_buffer = find_real_buffer(buffer);
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer(buffer, real_buffer);
    }
    GLES.glBindVertexBuffer(bindingindex, real_buffer, offset, stride);
}

size_t get_internal_format_size(GLenum internalformat) {
    switch (internalformat) {
    case GL_R8: case GL_R8I: case GL_R8UI: case GL_STENCIL_INDEX8: return 1;
    case GL_R16: case GL_R16I: case GL_R16UI: case GL_R16F: return 2;
    case GL_R32I: case GL_R32UI: case GL_R32F: case GL_DEPTH_COMPONENT32: case GL_DEPTH_COMPONENT32F: return 4;
    case GL_RG8: case GL_RG8I: case GL_RG8UI: return 2;
    case GL_RG16: case GL_RG16I: case GL_RG16UI: case GL_RG16F: return 4;
    case GL_RG32I: case GL_RG32UI: case GL_RG32F: return 8;
    case GL_RGB8: case GL_RGB8I: case GL_RGB8UI: return 3;
    case GL_RGB16: case GL_RGB16I: case GL_RGB16UI: case GL_RGB16F: return 6;
    case GL_RGB32I: case GL_RGB32UI: case GL_RGB32F: return 12;
    case GL_RGBA8: case GL_RGBA8I: case GL_RGBA8UI: return 4;
    case GL_RGBA16: case GL_RGBA16I: case GL_RGBA16UI: case GL_RGBA16F: return 8;
    case GL_RGBA32I: case GL_RGBA32UI: case GL_RGBA32F: return 16;
    case GL_DEPTH_COMPONENT16: return 2;
    case GL_DEPTH_COMPONENT24: return 3;
    case GL_DEPTH24_STENCIL8: return 4;
    case GL_DEPTH32F_STENCIL8: return 5;
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT: case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: return 8;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT: case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: return 16;
    default: return 0;
    }
}

extern std::string bufSampelerName;
void glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer) {
    if (target != GL_TEXTURE_BUFFER) return;
    if (!has_buffer(buffer) || buffer == 0) {
        GLES.glTexBuffer(target, internalformat, buffer);
        return;
    }
    GLuint real_buffer = find_real_buffer(buffer);
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer(buffer, real_buffer);
    }

    if (hardware->emulate_texture_buffer) {
        GLint boundTexture = 0;
        GLint prev_pixel_buffer_binding = 0;
        GLES.glActiveTexture(GL_TEXTURE0 + 15);
        GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
        GLES.glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &prev_pixel_buffer_binding);

        if (!boundTexture) return;

        GLES.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, real_buffer);
        GLint bufferSize;
        GLES.glGetBufferParameteriv(GL_PIXEL_UNPACK_BUFFER, GL_BUFFER_SIZE, &bufferSize);
        GLES.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        GLES.glBindTexture(GL_TEXTURE_2D, boundTexture);

        const GLuint MAX_WIDTH = 8192;
        GLuint pixelSize = get_internal_format_size(internalformat);
        GLuint numElements = bufferSize / pixelSize;
        GLuint width = numElements, height = 1;
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

        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        GLES.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

        GLES.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, prev_pixel_buffer_binding);
        GLES.glActiveTexture(GL_TEXTURE0 + gl_state->current_tex_unit);
        return;
    }

    GLES.glTexBuffer(target, internalformat, real_buffer);
}

void glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    if (!has_buffer(buffer) || buffer == 0) {
        GLES.glTexBufferRange(target, internalformat, buffer, offset, size);
        return;
    }
    GLuint real_buffer = find_real_buffer(buffer);
    if (!real_buffer) {
        GLES.glGenBuffers(1, &real_buffer);
        modify_buffer(buffer, real_buffer);
    }
    GLES.glTexBufferRange(target, internalformat, real_buffer, offset, size);
}

void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    GLES.glBufferData(target, size, data, usage);
    set_buffer_data_size(find_bound_buffer(target), size);
}

void* glMapBuffer(GLenum target, GLenum access) {
    if (g_gles_caps.GL_OES_mapbuffer) {
        return GLES.glMapBufferOES(target, access);
    }
    GLint buffer_size;
    GLES.glGetBufferParameteriv(target, GL_BUFFER_SIZE, &buffer_size);
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

void* glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    if (global_settings.buffer_coherent_as_flush) access &= ~GL_MAP_FLUSH_EXPLICIT_BIT;
    return GLES.glMapBufferRange(target, offset, length, access);
}

GLboolean glUnmapBuffer(GLenum target) {
    if (g_gles_caps.GL_OES_mapbuffer) return GLES.glUnmapBuffer(target);
    return GLES.glUnmapBuffer(target);
}

void glBufferStorage(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags) {
    if (GLES.glBufferStorageEXT) {
        if (global_settings.buffer_coherent_as_flush && ((flags & GL_MAP_PERSISTENT_BIT) != 0 ||
            (flags & GL_DYNAMIC_STORAGE_BIT) != 0))
            flags |= (GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT);
        GLES.glBufferStorageEXT(target, size, data, flags);
    }
}

void glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    if (!global_settings.buffer_coherent_as_flush) GLES.glFlushMappedBufferRange(target, offset, length);
}

// --- glGenVertexArrays 批量预分配优化 ---
void glGenVertexArrays(GLsizei n, GLuint* arrays) {
    ensure_array_capacity(maxArrayId + n);
    for (int i = 0; i < n; ++i) {
        arrays[i] = gen_array();
    }
}

void glDeleteVertexArrays(GLsizei n, const GLuint* arrays) {
    for (int i = 0; i < n; ++i) {
        GLuint real_array = find_real_array(arrays[i]);
        if (real_array) {
            GLES.glDeleteVertexArrays(1, &real_array);
        }
        remove_array(arrays[i]);
    }
}

GLboolean glIsVertexArray(GLuint array) {
    return has_array(array);
}

void glBindVertexArray(GLuint array) {
    bound_array = array;
    set_bound_buffer_by_target(GL_ELEMENT_ARRAY_BUFFER, get_ibo_by_vao(array));
    if (!has_array(array) || array == 0) {
        GLES.glBindVertexArray(array);
        return;
    }
    GLuint real_array = find_real_array(array);
    if (!real_array) {
        GLES.glGenVertexArrays(1, &real_array);
        modify_array(array, real_array);
    }
    GLES.glBindVertexArray(real_array);
}