//
// Created by hanji on 2025/2/6.
//

#include "framebuffer.h"
#include "log.h"
#include "../config/settings.h"
#include "FSR1/FSR1.h"
#include <vector>
#include <memory>
#include <algorithm>

#define DEBUG 0

struct attachment_t {
    GLenum textarget;
    GLuint texture;
    GLint level;
};

struct framebuffer_t {
    bool initialized = false;
    std::unique_ptr<attachment_t[]> color_attachments; // 用unique_ptr自动管理内存
    attachment_t depth_attachment = {0};
    attachment_t stencil_attachment = {0};
};

static GLint MAX_COLOR_ATTACHMENTS = 0;
static GLint MAX_DRAW_BUFFERS = 0;
static GLuint current_draw_fbo = 0;
static GLuint current_read_fbo = 0;
static std::vector<framebuffer_t> framebuffers;

// 用于减少重复调用 glGetIntegerv
inline void ensure_max_attachments() {
    if (MAX_COLOR_ATTACHMENTS == 0) {
        GLES.glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &MAX_COLOR_ATTACHMENTS);
        MAX_COLOR_ATTACHMENTS = std::max(MAX_COLOR_ATTACHMENTS, 8);
    }
    if (MAX_DRAW_BUFFERS == 0) {
        GLES.glGetIntegerv(GL_MAX_DRAW_BUFFERS, &MAX_DRAW_BUFFERS);
        MAX_DRAW_BUFFERS = std::max(MAX_DRAW_BUFFERS, 8);
    }
}

// 更快扩容，避免频繁resize
inline framebuffer_t& get_framebuffer(GLuint id) {
    if (id >= framebuffers.size()) {
        framebuffers.resize(std::max(framebuffers.size() * 2, static_cast<size_t>(id + 1)));
    }
    return framebuffers[id];
}

void InitFramebufferMap(size_t expectedSize) {
    framebuffers.reserve(expectedSize);
}

// 初始化FBO，自动管理color_attachments
inline void init_framebuffer(framebuffer_t& fbo) {
    if (!fbo.initialized) {
        fbo.color_attachments = std::make_unique<attachment_t[]>(MAX_COLOR_ATTACHMENTS);
        std::fill_n(fbo.color_attachments.get(), MAX_COLOR_ATTACHMENTS, attachment_t{0});
        fbo.initialized = true;
    }
}

// 绑定FBO
void glBindFramebuffer(GLenum target, GLuint framebuffer) {
    ensure_max_attachments();
    auto& fbo = get_framebuffer(framebuffer);
    
    if (framebuffer == 0 && target != GL_READ_FRAMEBUFFER) {
        framebuffer = FSR1_Context::g_renderFBO;
        FSR1_Context::g_dirty = true;
    }

    if (target != GL_READ_FRAMEBUFFER) {
        set_gl_state_current_draw_fbo(framebuffer);
    }

    if (framebuffer != 0) {
        init_framebuffer(fbo);
    }
    if (target == GL_DRAW_FRAMEBUFFER || target == GL_FRAMEBUFFER) {
        current_draw_fbo = framebuffer;
    }
    if (target == GL_READ_FRAMEBUFFER || target == GL_FRAMEBUFFER) {
        current_read_fbo = framebuffer;
    }
    
    GLES.glBindFramebuffer(target, framebuffer);
}

// 更新attachment, 用于color/depth/stencil
inline void update_attachment(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    GLuint current_fbo = (target == GL_READ_FRAMEBUFFER) ? current_read_fbo : current_draw_fbo;
    if (current_fbo == 0) return;
    auto& fbo = framebuffers[current_fbo];
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS) {
        int index = attachment - GL_COLOR_ATTACHMENT0;
        fbo.color_attachments[index] = {textarget, texture, level};
    } else if (attachment == GL_DEPTH_ATTACHMENT) {
        fbo.depth_attachment = {textarget, texture, level};
    } else if (attachment == GL_STENCIL_ATTACHMENT) {
        fbo.stencil_attachment = {textarget, texture, level};
    }
}

// 通过update_attachment和底层接口
void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    update_attachment(target, attachment, textarget, texture, level);
    GLES.glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    update_attachment(target, attachment, GL_TEXTURE_2D, texture, level);
    GLES.glFramebufferTexture(target, attachment, texture, level);
}

// 更快的draw buffer设置，避免频繁分配vector
void glDrawBuffer(GLenum buffer) {
    GLint currentFBO;
    GLES.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
    if (currentFBO == 0) {
        GLES.glDrawBuffers(1, &buffer);
        return;
    }
    GLint maxAttachments = MAX_COLOR_ATTACHMENTS;
    if (buffer == GL_NONE) {
        static std::vector<GLenum> buffers(8, GL_NONE);
        if (maxAttachments > buffers.size()) buffers.resize(maxAttachments, GL_NONE);
        GLES.glDrawBuffers(maxAttachments, buffers.data());
    } else if (buffer >= GL_COLOR_ATTACHMENT0 && buffer < GL_COLOR_ATTACHMENT0 + maxAttachments) {
        static std::vector<GLenum> buffers(8, GL_NONE);
        if (maxAttachments > buffers.size()) buffers.resize(maxAttachments, GL_NONE);
        std::fill(buffers.begin(), buffers.end(), GL_NONE);
        buffers[buffer - GL_COLOR_ATTACHMENT0] = buffer;
        GLES.glDrawBuffers(maxAttachments, buffers.data());
    }
    CHECK_GL_ERROR;
}

// 批量draw buffers，减少vector分配
void glDrawBuffers(GLsizei n, const GLenum* bufs) {
    LOG_D("glDrawBuffers called with n=%d", n); // 添加日志
    
    // 处理默认帧缓冲区 (FBO 0)
    if (current_draw_fbo == 0) {
        GLES.glDrawBuffers(n, bufs);
        return;
    }
    
    framebuffer_t& fbo = framebuffers[current_draw_fbo];
    
    // 检查是否所有缓冲区都是 GL_NONE
    bool all_none = true;
    for (int i = 0; i < n; ++i) {
        if (bufs[i] != GL_NONE) {
            all_none = false;
            break;
        }
    }
    
    // 处理全为 GL_NONE 的情况
    if (all_none) {
        LOG_D("glDrawBuffers, fb %d all_none true", current_draw_fbo);
        fbo.color_attachments_all_none = true;
        GLES.glDrawBuffers(n, bufs);
        return;
    }
    
    LOG_D("glDrawBuffers, fb %d all_none false", current_draw_fbo);
    fbo.color_attachments_all_none = false;
    
    // 使用静态 vector 避免重复内存分配
    static std::vector<GLenum> new_bufs(8);
    if (n > new_bufs.size()) {
        new_bufs.resize(n);
    }
    
    // 处理每个缓冲区
    for (int i = 0; i < n; ++i) {
        if (bufs[i] >= GL_COLOR_ATTACHMENT0 && 
            bufs[i] < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS) {
            
            GLenum logical_attachment = bufs[i];
            GLenum physical_attachment = GL_COLOR_ATTACHMENT0 + i;
            new_bufs[i] = physical_attachment;
            
            int index = logical_attachment - GL_COLOR_ATTACHMENT0;
            auto& attach = fbo.color_attachments[index];
            
            // 设置帧缓冲区纹理附件
            GLES.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, physical_attachment,
                                   attach.textarget, attach.texture, attach.level);
        } else {
            new_bufs[i] = bufs[i];
        }
    }
    
    // 最终调用 OpenGL
    GLES.glDrawBuffers(n, new_bufs.data());
}


// 读buffer，根据attachment快速设置
void glReadBuffer(GLenum src) {
    if (current_read_fbo != 0 && src >= GL_COLOR_ATTACHMENT0 &&
        src < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS) {
        auto& fbo = framebuffers[current_read_fbo];
        int index = src - GL_COLOR_ATTACHMENT0;
        auto& attach = fbo.color_attachments[index];
        GLES.glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               attach.textarget, attach.texture, attach.level);
        GLES.glReadBuffer(GL_COLOR_ATTACHMENT0);
    } else {
        GLES.glReadBuffer(src);
    }
}

// 帧缓冲状态，强制完整防止误判
GLenum glCheckFramebufferStatus(GLenum target) {
    GLenum status = GLES.glCheckFramebufferStatus(target);
    if (global_settings.ignore_error == IgnoreErrorLevel::Full && status != GL_FRAMEBUFFER_COMPLETE)
        return GL_FRAMEBUFFER_COMPLETE;
    return status;
}
