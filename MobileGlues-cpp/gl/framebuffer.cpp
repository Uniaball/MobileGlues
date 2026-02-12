// MobileGlues - gl/framebuffer.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "framebuffer.h"
#include "log.h"
#include "../config/settings.h"
#include "FSR1/FSR1.h"
#include <vector>
#include <algorithm>

#define DEBUG 0

static GLint MAX_COLOR_ATTACHMENTS = 0;
static GLint MAX_DRAW_BUFFERS = 0;
GLuint current_draw_fbo = 0;
GLuint current_read_fbo = 0;
std::vector<framebuffer_t> framebuffers;

void ensure_max_attachments() {
    if (MAX_COLOR_ATTACHMENTS == 0) {
        GLES.glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &MAX_COLOR_ATTACHMENTS);
        MAX_COLOR_ATTACHMENTS = MAX_COLOR_ATTACHMENTS > 0 ? MAX_COLOR_ATTACHMENTS : 8;
#if DEBUG
        LOG_D("MAX_COLOR_ATTACHMENTS: %d", MAX_COLOR_ATTACHMENTS);
#endif
    }
    if (MAX_DRAW_BUFFERS == 0) {
        GLES.glGetIntegerv(GL_MAX_DRAW_BUFFERS, &MAX_DRAW_BUFFERS);
        MAX_DRAW_BUFFERS = MAX_DRAW_BUFFERS > 0 ? MAX_DRAW_BUFFERS : 8;
#if DEBUG
        LOG_D("MAX_DRAW_BUFFERS: %d", MAX_DRAW_BUFFERS);
#endif
    }
}

framebuffer_t& get_framebuffer(GLuint id) {
    if (id >= framebuffers.size()) {
        framebuffers.resize(id + 10);
    }
    return framebuffers[id];
}

void InitFramebufferMap(size_t expectedSize) {
    framebuffers.reserve(expectedSize);
}

void init_framebuffer(framebuffer_t& fbo) {
    if (!fbo.initialized) {
        fbo.color_attachments = new attachment_t[MAX_COLOR_ATTACHMENTS];
        std::fill_n(fbo.color_attachments, MAX_COLOR_ATTACHMENTS, attachment_t{0});
        fbo.initialized = true;
#if DEBUG
        LOG_D("Initialized FBO %d", &fbo - framebuffers.data());
#endif
    }
}

void glBindFramebuffer(GLenum target, GLuint framebuffer) {
#if DEBUG
    LOG()
#endif
    ensure_max_attachments();
    
    auto& fbo = get_framebuffer(framebuffer);
    
    if (framebuffer == 0 && target != GL_READ_FRAMEBUFFER) {
        framebuffer = FSR1_Context::g_renderFBO;
        FSR1_Context::g_dirty = true;
#if DEBUG
        LOG_D("Bound FSR FBO: %d", framebuffer);
#endif
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
#if DEBUG
    CHECK_GL_ERROR
#endif
}

void update_attachment(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    GLuint current_fbo = (target == GL_READ_FRAMEBUFFER) ? current_read_fbo : current_draw_fbo;
    if (current_fbo == 0) return;
    
    auto& fbo = framebuffers[current_fbo];
    
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS) {
        int index = attachment - GL_COLOR_ATTACHMENT0;
        fbo.color_attachments[index] = {textarget, texture, level};
#if DEBUG
        LOG_D("Updated color attachment %d for FBO %d: tex=%d", index, current_fbo, texture);
#endif
    } else if (attachment == GL_DEPTH_ATTACHMENT) {
        fbo.depth_attachment = {textarget, texture, level};
    } else if (attachment == GL_STENCIL_ATTACHMENT) {
        fbo.stencil_attachment = {textarget, texture, level};
    }
}

void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    update_attachment(target, attachment, textarget, texture, level);
    GLES.glFramebufferTexture2D(target, attachment, textarget, texture, level);
#if DEBUG
    CHECK_GL_ERROR
#endif
}

void glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    update_attachment(target, attachment, GL_TEXTURE_2D, texture, level);
    GLES.glFramebufferTexture(target, attachment, texture, level);
#if DEBUG
    CHECK_GL_ERROR
#endif
}

void glDrawBuffer(GLenum buffer) {
#if DEBUG
    LOG()
    LOG_D("glDrawBuffer %d", buffer)
    // GLint currentFBO;
    // GLES.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
#endif
    
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
    } else if (buffer >= GL_COLOR_ATTACHMENT0 && buffer < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS) {
        fbo.color_attachments_all_none = false;
        std::vector<GLenum> bufs(MAX_DRAW_BUFFERS, GL_NONE);
        bufs[buffer - GL_COLOR_ATTACHMENT0] = buffer;
        GLES.glDrawBuffers(MAX_DRAW_BUFFERS, bufs.data());
    }
    
#if DEBUG
    CHECK_GL_ERROR
#endif
}

void glDrawBuffers(GLsizei n, const GLenum* bufs) {
#if DEBUG
    LOG()
    LOG_D("glDrawBuffers: n=%d", n);
#endif
    
    if (current_draw_fbo == 0) {
        GLES.glDrawBuffers(n, bufs);
        return;
    }
    
    auto& fbo = framebuffers[current_draw_fbo];
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
    static std::vector<GLenum> new_bufs;
    new_bufs.resize(n);
    
    for (int i = 0; i < n; ++i) {
        if (bufs[i] >= GL_COLOR_ATTACHMENT0 && bufs[i] < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS) {
            GLenum logical_attachment = bufs[i];
            GLenum physical_attachment = GL_COLOR_ATTACHMENT0 + i;
            new_bufs[i] = physical_attachment;
            int index = logical_attachment - GL_COLOR_ATTACHMENT0;
            attachment_t& attach = fbo.color_attachments[index];
            GLES.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, physical_attachment, attach.textarget, attach.texture, attach.level);
        } else {
            new_bufs[i] = bufs[i];
        }
    }
    
    GLES.glDrawBuffers(n, new_bufs.data());
    
#if DEBUG
    CHECK_GL_ERROR
#endif
}

void glReadBuffer(GLenum src) {
#if DEBUG
    LOG_D("glReadBuffer: %d", src);
#endif
    
    if (current_read_fbo != 0 && src >= GL_COLOR_ATTACHMENT0 && src < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS) {
        framebuffer_t& fbo = framebuffers[current_read_fbo];
        int index = src - GL_COLOR_ATTACHMENT0;
        attachment_t& attach = fbo.color_attachments[index];
        GLES.glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, attach.textarget, attach.texture, attach.level);
        GLES.glReadBuffer(GL_COLOR_ATTACHMENT0);
    } else {
        GLES.glReadBuffer(src);
    }
    
#if DEBUG
    CHECK_GL_ERROR
#endif
}

GLenum glCheckFramebufferStatus(GLenum target) {
    GLenum status = GLES.glCheckFramebufferStatus(target);
#if DEBUG
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG_D("Framebuffer incomplete: status=0x%X", status);
    }
#endif
    
    if (global_settings.ignore_error == IgnoreErrorLevel::Full && status != GL_FRAMEBUFFER_COMPLETE) {
        return GL_FRAMEBUFFER_COMPLETE;
    }
    
    return status;
}

void cleanup_framebuffers() {
#if DEBUG
    LOG_D("Cleaning up framebuffers");
#endif
    
    for (auto& fbo : framebuffers) {
        if (fbo.color_attachments) {
            delete[] fbo.color_attachments;
            fbo.color_attachments = nullptr;
        }
    }
    
    framebuffers.clear();
    current_draw_fbo = 0;
    current_read_fbo = 0;
}
