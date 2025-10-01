#include "framebuffer.h"
#include "log.h"
#include "../config/settings.h"
#include "FSR1/FSR1.h"
#include <vector>
#include <algorithm>
#include <memory>

// 全局变量定义
static GLint MAX_COLOR_ATTACHMENTS = 0;
static GLint MAX_DRAW_BUFFERS = 0;
GLuint current_draw_fbo = 0;
GLuint current_read_fbo = 0;
std::vector<framebuffer_t> framebuffers;

// 初始化最大附件数量
void ensure_max_attachments() {
    if (MAX_COLOR_ATTACHMENTS == 0) {
        GLES.glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &MAX_COLOR_ATTACHMENTS);
        MAX_COLOR_ATTACHMENTS = MAX_COLOR_ATTACHMENTS > 0 ? MAX_COLOR_ATTACHMENTS : 8;
    }
    if (MAX_DRAW_BUFFERS == 0) {
        GLES.glGetIntegerv(GL_MAX_DRAW_BUFFERS, &MAX_DRAW_BUFFERS);
        MAX_DRAW_BUFFERS = MAX_DRAW_BUFFERS > 0 ? MAX_DRAW_BUFFERS : 8;
    }
}

// 获取帧缓冲区对象
framebuffer_t& get_framebuffer(GLuint id) {
    if (id >= framebuffers.size()) {
        framebuffers.resize(id + 10); // 更保守的扩容策略
    }
    return framebuffers[id];
}

// 初始化帧缓冲区映射
void InitFramebufferMap(size_t expectedSize) {
    framebuffers.reserve(expectedSize);
}

// 初始化帧缓冲区
void init_framebuffer(framebuffer_t& fbo) {
    if (!fbo.initialized) {
        fbo.color_attachments = new attachment_t[MAX_COLOR_ATTACHMENTS];
        std::fill_n(fbo.color_attachments, MAX_COLOR_ATTACHMENTS, attachment_t{0});
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

// 更新附件信息
void update_attachment(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    GLuint current_fbo = (target == GL_READ_FRAMEBUFFER) ? current_read_fbo : current_draw_fbo;
    if (current_fbo == 0) return;
    
    auto& fbo = framebuffers[current_fbo];
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS) {
        int index = attachment - GL_COLOR_ATTACHMENT0;
        fbo.color_attachments[index] = {textarget, texture, level};
    } 
    else if (attachment == GL_DEPTH_ATTACHMENT) {
        fbo.depth_attachment = {textarget, texture, level};
    }
    else if (attachment == GL_STENCIL_ATTACHMENT) {
        fbo.stencil_attachment = {textarget, texture, level};
    }
}

// 帧缓冲纹理操作
void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    update_attachment(target, attachment, textarget, texture, level);
    GLES.glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    update_attachment(target, attachment, GL_TEXTURE_2D, texture, level);
    GLES.glFramebufferTexture(target, attachment, texture, level);
}

// 绘制缓冲区设置
void glDrawBuffer(GLenum buffer) {
    if (current_draw_fbo == 0) {
        GLenum buffers[] = {buffer};
        glDrawBuffers(1, buffers);
        return;
    }
    
    auto& fbo = framebuffers[current_draw_fbo];
    if (buffer == GL_NONE) {
        fbo.color_attachments_all_none = true;
        static std::vector<GLenum> none_bufs(MAX_DRAW_BUFFERS, GL_NONE);
        GLES.glDrawBuffers(MAX_DRAW_BUFFERS, none_bufs.data());
    } 
    else if (buffer >= GL_COLOR_ATTACHMENT0 && buffer < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS) {
        fbo.color_attachments_all_none = false;
        std::vector<GLenum> bufs(MAX_DRAW_BUFFERS, GL_NONE);
        bufs[buffer - GL_COLOR_ATTACHMENT0] = buffer;
        GLES.glDrawBuffers(MAX_DRAW_BUFFERS, bufs.data());
    }
}

// 批量绘制缓冲区设置
void glDrawBuffers(GLsizei n, const GLenum* bufs) {
    if (current_draw_fbo == 0) {
        GLES.glDrawBuffers(n, bufs);
        return;
    }
    
    auto& fbo = framebuffers[current_draw_fbo];
    
    // 检查是否所有缓冲区都是 GL_NONE
    bool all_none = true;
    for (int i = 0; i < n; ++i) {
        if (bufs[i] != GL_NONE) {
            all_none = false;
            break;
        }
    }
    
    if (all_none) {
        fbo.color_attachments_all_none = true;
        GLES.glDrawBuffers(n, bufs);
        return;
    }
    
    fbo.color_attachments_all_none = false;
    
    // 使用静态vector避免重复内存分配
    static std::vector<GLenum> new_bufs;
    new_bufs.resize(n);
    
    for (int i = 0; i < n; ++i) {
        if (bufs[i] >= GL_COLOR_ATTACHMENT0 && bufs[i] < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS) {
            int index = bufs[i] - GL_COLOR_ATTACHMENT0;
            auto& attach = fbo.color_attachments[index];
            GLES.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                                   attach.textarget, attach.texture, attach.level);
            new_bufs[i] = GL_COLOR_ATTACHMENT0 + i;
        } else {
            new_bufs[i] = bufs[i];
        }
    }
    
    GLES.glDrawBuffers(n, new_bufs.data());
}

// 读缓冲区设置
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

// 帧缓冲状态检查
GLenum glCheckFramebufferStatus(GLenum target) {
    GLenum status = GLES.glCheckFramebufferStatus(target);
    return (global_settings.ignore_error == IgnoreErrorLevel::Full && status != GL_FRAMEBUFFER_COMPLETE)
           ? GL_FRAMEBUFFER_COMPLETE : status;
}

// 清理函数
void cleanup_framebuffers() {
    for (auto& fbo : framebuffers) {
        if (fbo.color_attachments) {
            delete[] fbo.color_attachments;
            fbo.color_attachments = nullptr;
        }
    }
    framebuffers.clear();
}
