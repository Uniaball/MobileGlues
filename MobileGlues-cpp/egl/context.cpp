// MobileGlues - egl/context.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "context.h"
#include "../gl/log.h"
#include "../gl/mg.h"
#include <mutex>
#include <unordered_map>

#define DEBUG 0

thread_local MGContext* g_current_ctx = nullptr;

namespace {

// Where gl_state points when no tracked context is current. gl_state is written
// through on the draw hot path, so it must never be left pointing at a released
// record. Every member is a scalar, so this is constant-initialised and is
// already usable when the library's constructors run.
gl_state_s g_default_gl_state;

// The library initialises itself from a static constructor, and that
// constructor reaches these tables through mg_display_initialised. A namespace
// scope std::unordered_map is dynamically initialised, so whether it had been
// constructed by then came down to the order the linker happened to emit the
// translation units in -- and it did not: the first insertion ran against an
// all zero map, whose zero max_load_factor asked for an infinite bucket count
// and aborted the process before main. Constructing on first use removes the
// ordering question instead of relying on it.
std::mutex& ctx_mutex() {
    static std::mutex m;
    return m;
}
std::unordered_map<EGLContext, std::shared_ptr<MGContext>>& contexts() {
    static std::unordered_map<EGLContext, std::shared_ptr<MGContext>> m;
    return m;
}
std::unordered_map<EGLDisplay, int>& display_refs() {
    static std::unordered_map<EGLDisplay, int> m;
    return m;
}

unsigned long long g_next_ctx_id = 1;
unsigned long long g_next_group_id = 1;

} // namespace

// Access sites read the same way they did when these were plain globals.
#define g_ctx_mutex ctx_mutex()
#define g_contexts contexts()
#define g_display_refs display_refs()

namespace {

// Caller holds g_ctx_mutex.
void release_locked(const std::shared_ptr<MGContext>& ctx) {
    if (ctx->current_count > 0 || !ctx->destroy_pending) return;
    LOG_D("MGContext %llu released", ctx->id)
    // The map is the only owner, so erasing frees the record and the gl_state_s
    // embedded in it.
    if (gl_state == &ctx->gl) gl_state = &g_default_gl_state;
    g_contexts.erase(ctx->handle);
}

} // namespace

MGContext* mg_context_create(EGLDisplay dpy, EGLContext handle, EGLContext share_handle, EGLenum client_type,
                             EGLint major, EGLint minor, EGLint profile_mask, EGLint context_flags) {
    if (handle == EGL_NO_CONTEXT) return nullptr;

    std::lock_guard<std::mutex> lock(g_ctx_mutex);

    auto ctx = std::make_shared<MGContext>();
    ctx->id = g_next_ctx_id++;
    ctx->display = dpy;
    ctx->handle = handle;
    ctx->client_type = client_type;
    ctx->granted_major = major;
    ctx->granted_minor = minor;
    ctx->profile_mask = profile_mask;
    ctx->context_flags = context_flags;
    ctx->draw = EGL_NO_SURFACE;
    ctx->read = EGL_NO_SURFACE;
    ctx->current_count = 0;
    ctx->destroy_pending = false;
    mg_enable_reset(&ctx->enable);
    ctx->gl = gl_state_s{};

    // A context created with a share handle joins that context's group, so the
    // objects GL shares stay described by one record.
    if (share_handle != EGL_NO_CONTEXT) {
        const auto it = g_contexts.find(share_handle);
        if (it != g_contexts.end()) {
            ctx->share_group = it->second->share_group;
        }
    }
    if (!ctx->share_group) {
        ctx->share_group = std::make_shared<MGShareGroup>();
        ctx->share_group->id = g_next_group_id++;
    }

    LOG_D("MGContext %llu created (handle %p, share group %llu, %d.%d)", ctx->id, handle, ctx->share_group->id, major,
          minor)
    g_contexts[handle] = ctx;
    return ctx.get();
}

void mg_display_initialised(EGLDisplay dpy) {
    if (dpy == EGL_NO_DISPLAY) return;
    std::lock_guard<std::mutex> lock(g_ctx_mutex);
    ++g_display_refs[dpy];
    LOG_D("EGLDisplay %p initialise count -> %d", dpy, g_display_refs[dpy])
}

bool mg_display_release(EGLDisplay dpy) {
    if (dpy == EGL_NO_DISPLAY) return false;
    std::lock_guard<std::mutex> lock(g_ctx_mutex);
    const auto it = g_display_refs.find(dpy);
    if (it == g_display_refs.end()) return false;
    if (--it->second > 0) return false;
    g_display_refs.erase(it);
    return true;
}

MGContext* mg_context_find(EGLContext handle) {
    if (handle == EGL_NO_CONTEXT) return nullptr;
    std::lock_guard<std::mutex> lock(g_ctx_mutex);
    const auto it = g_contexts.find(handle);
    return it == g_contexts.end() ? nullptr : it->second.get();
}

void mg_context_make_current(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext handle) {
    std::lock_guard<std::mutex> lock(g_ctx_mutex);

    if (g_current_ctx != nullptr) {
        const auto old = g_contexts.find(g_current_ctx->handle);
        if (old != g_contexts.end()) {
            if (old->second->current_count > 0) --old->second->current_count;
            // Releasing here is why eglDestroyContext can be called on a context
            // that is still current: the record outlives the EGL handle until the
            // last thread lets go of it.
            release_locked(old->second);
        }
        g_current_ctx = nullptr;
        gl_state = &g_default_gl_state;
        // 0 selects each subsystem's fallback instance, the one used before any
        // tracked context is current.
        mg_buffer_bind_context(0, 0);
        mg_texture_bind_context(0, 0);
        mg_framebuffer_bind_context(0);
        mg_fsr1_bind_context(0);
    }

    if (handle == EGL_NO_CONTEXT) {
        LOG_D("MGContext: no context current on this thread")
        return;
    }

    const auto it = g_contexts.find(handle);
    if (it == g_contexts.end()) {
        // A context this layer never saw created -- the bootstrap probe context,
        // or one made before the library was loaded. Leaving g_current_ctx null
        // keeps every consumer on its fallback path rather than inventing a
        // record with made-up attributes.
        LOG_D("MGContext: handle %p is not tracked, leaving no current record", handle)
        return;
    }

    it->second->display = dpy;
    it->second->draw = draw;
    it->second->read = read;
    ++it->second->current_count;
    g_current_ctx = it->second.get();
    mg_buffer_bind_context(g_current_ctx->id, g_current_ctx->share_group->id);
    mg_texture_bind_context(g_current_ctx->id, g_current_ctx->share_group->id);
    mg_framebuffer_bind_context(g_current_ctx->id);
    mg_fsr1_bind_context(g_current_ctx->id);
    // Repoint the shared gl_state at this context's copy. Everything that reads
    // gl_state-> keeps working unchanged; it simply stops being one set of values
    // for every context in the process.
    gl_state = &g_current_ctx->gl;
    LOG_D("MGContext %llu is now current", it->second->id)
}

void mg_context_destroy(EGLContext handle) {
    if (handle == EGL_NO_CONTEXT) return;
    std::lock_guard<std::mutex> lock(g_ctx_mutex);
    const auto it = g_contexts.find(handle);
    if (it == g_contexts.end()) return;

    it->second->destroy_pending = true;
    // Deliberately not erased while still current: GL permits destroying a
    // context that is current, and the record has to answer queries until the
    // last thread makes something else current.
    release_locked(it->second);
}

void mg_context_forget_display(EGLDisplay dpy) {
    std::lock_guard<std::mutex> lock(g_ctx_mutex);
    for (auto it = g_contexts.begin(); it != g_contexts.end();) {
        if (it->second->display == dpy && it->second->current_count == 0) {
            it = g_contexts.erase(it);
        } else {
            // eglTerminate marks resources for deletion but a context that is
            // still current stays usable until it is replaced, so it survives.
            if (it->second->display == dpy) it->second->destroy_pending = true;
            ++it;
        }
    }
}
