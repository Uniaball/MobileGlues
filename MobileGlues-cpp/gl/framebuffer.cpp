// MobileGlues - gl/framebuffer.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "framebuffer.h"
#include "../egl/context.h"
#include <mutex>
#include <unordered_map>
#include "log.h"
#include "../config/settings.h"
#include "FSR1/FSR1.h"

#define DEBUG 0

static GLint MAX_COLOR_ATTACHMENTS = 0;
static GLint MAX_DRAW_BUFFERS = 0;
GLuint current_draw_fbo = 0;
GLuint current_read_fbo = 0;
// Framebuffer objects are container state: GL does not share them across a share
// group, so they belong to one context. See gl/buffer.cpp for the pattern.
namespace {
std::mutex g_fbo_mutex;
std::unordered_map<unsigned long long, std::vector<framebuffer_t>> g_fbo_ctxs;
std::vector<framebuffer_t> g_fbo_default;
thread_local std::vector<framebuffer_t>* g_fc = &g_fbo_default;
} // namespace

void mg_framebuffer_bind_context(unsigned long long ctx_id) {
    if (ctx_id == 0) {
        g_fc = &g_fbo_default;
        return;
    }
    std::lock_guard<std::mutex> lock(g_fbo_mutex);
    g_fc = &g_fbo_ctxs[ctx_id];
}

#define framebuffers (*g_fc)

std::vector<framebuffer_t>& mg_framebuffers() {
    return *g_fc;
}
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
        memset(fbo.color_attachments, 0, sizeof(attachment_t) * MAX_COLOR_ATTACHMENTS);
        fbo.initialized = true;
    }
}
void glBindFramebuffer(GLenum target, GLuint framebuffer) {
    ensure_max_attachments();
    framebuffer_t& fbo = get_framebuffer(framebuffer);

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
void update_attachment(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    GLuint current_fbo = (target == GL_READ_FRAMEBUFFER) ? current_read_fbo : current_draw_fbo;
    if (current_fbo == 0) return;
    framebuffer_t& fbo = framebuffers[current_fbo];
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS) {
        int index = attachment - GL_COLOR_ATTACHMENT0;
        fbo.color_attachments[index] = {textarget, texture, level};
    } else if (attachment == GL_DEPTH_ATTACHMENT) {
        fbo.depth_attachment = {textarget, texture, level};
    } else if (attachment == GL_STENCIL_ATTACHMENT) {
        fbo.stencil_attachment = {textarget, texture, level};
    }
}
void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    update_attachment(target, attachment, textarget, texture, level);
    GLES.glFramebufferTexture2D(target, attachment, textarget, texture, level);
}
void glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    update_attachment(target, attachment, GL_TEXTURE_2D, texture, level);
    GLES.glFramebufferTexture(target, attachment, texture, level);
}
void glDrawBuffer(GLenum buffer) {
    LOG()
    LOG_D("glDrawBuffer %d", buffer)

    //    GLint currentFBO;
    //    GLES.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
    if (current_draw_fbo == 0) {
        GLenum buffers[] = {buffer};
        glDrawBuffers(1, buffers);
    } else {
        GLint maxAttachments;
        GLES.glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAttachments);

        if (buffer == GL_NONE) {
            framebuffers[current_draw_fbo].color_attachments_all_none = true;
            std::vector<GLenum> buffers(maxAttachments, GL_NONE);
            glDrawBuffers(maxAttachments, buffers.data());
        } else if (buffer >= GL_COLOR_ATTACHMENT0 && buffer < GL_COLOR_ATTACHMENT0 + maxAttachments) {
            framebuffers[current_draw_fbo].color_attachments_all_none = false;
            std::vector<GLenum> buffers(maxAttachments, GL_NONE);
            buffers[buffer - GL_COLOR_ATTACHMENT0] = buffer;
            glDrawBuffers(maxAttachments, buffers.data());
        }
    }
    CHECK_GL_ERROR;
}
void glDrawBuffers(GLsizei n, const GLenum* bufs) {
    LOG()
    if (current_draw_fbo == 0) {
        GLES.glDrawBuffers(n, bufs);
        return;
    }

    framebuffer_t& fbo = framebuffers[current_draw_fbo];

    bool all_none = true;
    for (int i = 0; i < n; ++i) {
        if (bufs[i] != GL_NONE) {
            all_none = false;
            break;
        }
    }

    if (all_none) {
        LOG_D("glDrawBuffers, fb %d all_none true", current_draw_fbo)
        fbo.color_attachments_all_none = true;
        GLES.glDrawBuffers(n, bufs);
        return;
    } else {
        LOG_D("glDrawBuffers, fb %d all_none false", current_draw_fbo)
        fbo.color_attachments_all_none = false;
    }

    std::vector<GLenum> new_bufs(n);
    for (int i = 0; i < n; i++) {
        if (bufs[i] >= GL_COLOR_ATTACHMENT0 && bufs[i] < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS) {
            GLenum logical_attachment = bufs[i];
            GLenum physical_attachment = GL_COLOR_ATTACHMENT0 + i;
            new_bufs[i] = physical_attachment;
            int index = logical_attachment - GL_COLOR_ATTACHMENT0;
            attachment_t& attach = fbo.color_attachments[index];
            GLES.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, physical_attachment, attach.textarget, attach.texture,
                                        attach.level);
        } else {
            new_bufs[i] = bufs[i];
        }
    }
    GLES.glDrawBuffers(n, new_bufs.data());
}
void glReadBuffer(GLenum src) {
    if (current_read_fbo != 0 && src >= GL_COLOR_ATTACHMENT0 && src < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS) {
        framebuffer_t& fbo = framebuffers[current_read_fbo];
        int index = src - GL_COLOR_ATTACHMENT0;
        attachment_t& attach = fbo.color_attachments[index];
        GLES.glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, attach.textarget, attach.texture,
                                    attach.level);
        GLES.glReadBuffer(GL_COLOR_ATTACHMENT0);
    } else {
        GLES.glReadBuffer(src);
    }
}
GLenum glCheckFramebufferStatus(GLenum target) {
    GLenum status = GLES.glCheckFramebufferStatus(target);
    if (global_settings.ignore_error == IgnoreErrorLevel::Full && status != GL_FRAMEBUFFER_COMPLETE) {
        return GL_FRAMEBUFFER_COMPLETE;
    }
    return status;
}