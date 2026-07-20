// MobileGlues - egl/egl.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "egl.h"
#include "../config/settings.h"
#include "../gl/FSR1/FSR1.h"
#include "../gl/log.h"
#include "../gl/mg.h"
#include "../gles/loader.h"
#include "../glx/lookup.h"
#include "loader.h"
#include <EGL/eglext.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#define DEBUG 0

namespace {

    constexpr EGLint kBackendDesktopGlClientVersion = 3;
    constexpr EGLint kBackendDesktopGlRenderableBit = EGL_OPENGL_ES3_BIT;
    constexpr EGLint kVirtualDesktopProfileMask = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
    constexpr size_t kMaxEglAttributePairs = 128;

    struct DesktopContextInfo {
        EGLDisplay display;
        EGLint major;
        EGLint minor;
    };

    thread_local EGLenum frontend_api = EGL_OPENGL_ES_API;
    thread_local EGLint frontend_error = EGL_SUCCESS;
    std::mutex desktop_contexts_mutex;
    std::unordered_map<EGLContext, DesktopContextInfo> desktop_contexts;
    thread_local std::string frontend_extensions;

    void setFrontendError(EGLint error) {
        frontend_error = error;
    }

    bool copyAttributeList(const EGLint* attrib_list, std::vector<EGLint>* attributes) {
        attributes->clear();
        if (!attrib_list) return true;

        for (size_t pair = 0; pair < kMaxEglAttributePairs; ++pair) {
            const EGLint attribute = attrib_list[pair * 2];
            attributes->push_back(attribute);
            if (attribute == EGL_NONE) return true;
            attributes->push_back(attrib_list[pair * 2 + 1]);
        }

        return false;
    }

    bool containsOpenGLBit(EGLint value) {
        return (value & EGL_OPENGL_BIT) != 0;
    }

    EGLint backendRenderableMask(EGLint frontend_mask) {
        if (containsOpenGLBit(frontend_mask)) {
            frontend_mask &= ~EGL_OPENGL_BIT;
            frontend_mask |= kBackendDesktopGlRenderableBit;
        }
        return frontend_mask;
    }

    EGLint frontendRenderableMask(EGLint backend_mask) {
        constexpr EGLint kBackendGlesBits = EGL_OPENGL_ES_BIT | EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT;
        if ((backend_mask & kBackendDesktopGlRenderableBit) != 0) {
            backend_mask &= ~kBackendGlesBits;
            backend_mask |= EGL_OPENGL_BIT;
        }
        return backend_mask;
    }

    bool makeBackendConfigAttributes(const EGLint* attrib_list, std::vector<EGLint>* backend_attributes) {
        if (!copyAttributeList(attrib_list, backend_attributes)) return false;
        if (backend_attributes->empty()) backend_attributes->push_back(EGL_NONE);

        bool has_renderable_type = false;
        bool requests_desktop_gl = frontend_api == EGL_OPENGL_API;
        for (size_t i = 0; i + 1 < backend_attributes->size(); i += 2) {
            const EGLint attribute = (*backend_attributes)[i];
            if (attribute == EGL_NONE) break;

            EGLint& value = (*backend_attributes)[i + 1];
            if (attribute == EGL_RENDERABLE_TYPE) {
                has_renderable_type = true;
                requests_desktop_gl = requests_desktop_gl || containsOpenGLBit(value);
                value = backendRenderableMask(value);
            } else if (attribute == EGL_CONFORMANT) {
                requests_desktop_gl = requests_desktop_gl || containsOpenGLBit(value);
                value = backendRenderableMask(value);
            }
        }

        if (!has_renderable_type && requests_desktop_gl) {
            backend_attributes->insert(backend_attributes->end() - 1,
                                       {EGL_RENDERABLE_TYPE, kBackendDesktopGlRenderableBit});
        } else if (has_renderable_type && requests_desktop_gl) {
            for (size_t i = 0; i + 1 < backend_attributes->size(); i += 2) {
                if ((*backend_attributes)[i] == EGL_NONE) break;
                if ((*backend_attributes)[i] == EGL_RENDERABLE_TYPE) {
                    (*backend_attributes)[i + 1] |= kBackendDesktopGlRenderableBit;
                }
            }
        }
        return true;
    }

    bool isDesktopOnlyContextAttribute(EGLint attribute) {
        switch (attribute) {
        case EGL_CONTEXT_MINOR_VERSION:
        case EGL_CONTEXT_FLAGS_KHR:
        case EGL_CONTEXT_OPENGL_PROFILE_MASK:
        case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY:
        case EGL_CONTEXT_OPENGL_DEBUG:
        case EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE:
        case EGL_CONTEXT_OPENGL_ROBUST_ACCESS:
        case EGL_CONTEXT_OPENGL_NO_ERROR_KHR:
        case EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT:
        case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT:
            return true;
        default:
            return false;
        }
    }

    bool supportsDesktopContextAttribute(EGLint attribute, EGLint value) {
        switch (attribute) {
        case EGL_CONTEXT_FLAGS_KHR:
            return value == 0;
        case EGL_CONTEXT_OPENGL_PROFILE_MASK:
            return value == kVirtualDesktopProfileMask;
        case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY:
        case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT:
            return value == EGL_NO_RESET_NOTIFICATION;
        case EGL_CONTEXT_OPENGL_DEBUG:
        case EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE:
        case EGL_CONTEXT_OPENGL_ROBUST_ACCESS:
        case EGL_CONTEXT_OPENGL_NO_ERROR_KHR:
        case EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT:
            return value == EGL_FALSE;
        default:
            return false;
        }
    }

    EGLint defaultDesktopMajorVersion() {
        return global_settings.custom_gl_version.isEmpty() ? DEFAULT_GL_VERSION / 10
                                                           : global_settings.custom_gl_version.Major;
    }

    EGLint defaultDesktopMinorVersion() {
        return global_settings.custom_gl_version.isEmpty() ? DEFAULT_GL_VERSION % 10
                                                           : global_settings.custom_gl_version.Minor;
    }

    bool makeBackendContextAttributes(const EGLint* attrib_list, std::vector<EGLint>* backend_attributes,
                                      EGLint* frontend_major, EGLint* frontend_minor, EGLint* error) {
        *error = EGL_SUCCESS;
        if (!copyAttributeList(attrib_list, backend_attributes)) {
            *error = EGL_BAD_ATTRIBUTE;
            return false;
        }

        EGLint requested_major = defaultDesktopMajorVersion();
        EGLint requested_minor = defaultDesktopMinorVersion();
        bool saw_major_version = false;
        bool saw_minor_version = false;
        std::vector<EGLint> rewritten;
        rewritten.reserve(backend_attributes->size() + 3);

        for (size_t i = 0; i + 1 < backend_attributes->size(); i += 2) {
            const EGLint attribute = (*backend_attributes)[i];
            if (attribute == EGL_NONE) break;
            const EGLint value = (*backend_attributes)[i + 1];

            // EGL_CONTEXT_MAJOR_VERSION aliases EGL_CONTEXT_CLIENT_VERSION. Under the
            // desktop API it denotes the requested desktop GL version, not GLES.
            if (attribute == EGL_CONTEXT_CLIENT_VERSION) {
                if (saw_major_version) {
                    *error = EGL_BAD_ATTRIBUTE;
                    return false;
                }
                requested_major = value;
                saw_major_version = true;
                continue;
            }
            if (attribute == EGL_CONTEXT_MINOR_VERSION) {
                if (saw_minor_version) {
                    *error = EGL_BAD_ATTRIBUTE;
                    return false;
                }
                requested_minor = value;
                saw_minor_version = true;
                continue;
            }
            if (isDesktopOnlyContextAttribute(attribute)) {
                if (!supportsDesktopContextAttribute(attribute, value)) {
                    *error = EGL_BAD_MATCH;
                    return false;
                }
                continue;
            }

            rewritten.push_back(attribute);
            rewritten.push_back(value);
        }

        if (saw_major_version && !saw_minor_version) requested_minor = 0;
        if (requested_major != defaultDesktopMajorVersion() || requested_minor != defaultDesktopMinorVersion()) {
            *error = EGL_BAD_MATCH;
            return false;
        }

        *frontend_major = requested_major;
        *frontend_minor = requested_minor;

        rewritten.push_back(EGL_CONTEXT_CLIENT_VERSION);
        rewritten.push_back(kBackendDesktopGlClientVersion);
        rewritten.push_back(EGL_NONE);
        *backend_attributes = std::move(rewritten);
        return true;
    }

    void rememberDesktopContext(EGLContext context, EGLDisplay display, EGLint major, EGLint minor) {
        if (context == EGL_NO_CONTEXT) return;
        std::lock_guard<std::mutex> lock(desktop_contexts_mutex);
        desktop_contexts[context] = {display, major, minor};
    }

    bool findDesktopContext(EGLContext context, DesktopContextInfo* info) {
        std::lock_guard<std::mutex> lock(desktop_contexts_mutex);
        const auto it = desktop_contexts.find(context);
        if (it == desktop_contexts.end()) return false;
        *info = it->second;
        return true;
    }

    void forgetDesktopContext(EGLContext context) {
        std::lock_guard<std::mutex> lock(desktop_contexts_mutex);
        desktop_contexts.erase(context);
    }

    void forgetDisplayContexts(EGLDisplay display) {
        std::lock_guard<std::mutex> lock(desktop_contexts_mutex);
        for (auto it = desktop_contexts.begin(); it != desktop_contexts.end();) {
            if (it->second.display == display) {
                it = desktop_contexts.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool hasExtension(const std::string& extensions, const char* extension) {
        const std::string needle(extension);
        size_t offset = 0;
        while ((offset = extensions.find(needle, offset)) != std::string::npos) {
            const bool starts_at_boundary = offset == 0 || extensions[offset - 1] == ' ';
            const size_t end = offset + needle.size();
            const bool ends_at_boundary = end == extensions.size() || extensions[end] == ' ';
            if (starts_at_boundary && ends_at_boundary) return true;
            offset = end;
        }
        return false;
    }

    const char* frontendExtensionString(const char* backend_extensions) {
        frontend_extensions = backend_extensions;
        if (!hasExtension(frontend_extensions, "EGL_KHR_create_context")) {
            if (!frontend_extensions.empty()) frontend_extensions += ' ';
            frontend_extensions += "EGL_KHR_create_context";
        }
        return frontend_extensions.c_str();
    }

} // namespace

extern "C"
{
#define EGL_API __attribute__((visibility("default")))
    EGL_API EGLint eglGetError(void) {
        LOG_D("eglGetError");
        LOAD_EGL(eglGetError)

        if (frontend_error != EGL_SUCCESS) {
            const EGLint error = frontend_error;
            frontend_error = EGL_SUCCESS;
            // A virtual failure replaces, rather than queues behind, a stale backend error.
            egl_eglGetError();
            return error;
        }
        return egl_eglGetError();
    }

    EGL_API EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id) {
        LOG_D("eglGetDisplay, display_id: %p", display_id);
        LOAD_EGL(eglGetDisplay)
        return egl_eglGetDisplay(display_id);
    }

    EGL_API EGLBoolean eglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor) {
        LOG_D("eglInitialize, dpy: %p, major: %p, minor: %p", dpy, major, minor);
        LOAD_EGL(eglInitialize)
        return egl_eglInitialize(dpy, major, minor);
    }

    EGL_API EGLBoolean eglTerminate(EGLDisplay dpy) {
        LOG_D("eglTerminate, dpy: %p", dpy);
        LOAD_EGL(eglTerminate)
        const EGLBoolean result = egl_eglTerminate(dpy);
        if (result == EGL_TRUE) forgetDisplayContexts(dpy);
        return result;
    }

    EGL_API const char* eglQueryString(EGLDisplay dpy, EGLint name) {
        LOG_D("eglQueryString, dpy: %p, name: %d", dpy, name);
        LOAD_EGL(eglQueryString)
        const char* result = egl_eglQueryString(dpy, name);
        if (!result) return nullptr;
        if (name == EGL_CLIENT_APIS) return "OpenGL";
        if (name == EGL_EXTENSIONS) return frontendExtensionString(result);
        return result;
    }

    EGL_API EGLBoolean eglGetConfigs(EGLDisplay dpy, EGLConfig* configs, EGLint config_size, EGLint* num_config) {
        LOG_D("eglGetConfigs, dpy: %p, configs: %p, config_size: %d, num_config: %p", dpy, configs, config_size,
              num_config);
        LOAD_EGL(eglGetConfigs)
        return egl_eglGetConfigs(dpy, configs, config_size, num_config);
    }

    EGL_API EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint* attrib_list, EGLConfig* configs,
                                       EGLint config_size, EGLint* num_config) {
        LOG_D("eglChooseConfig, dpy: %p, attrib_list: %p, configs: %p, config_size: "
              "%d, num_config: %p",
              dpy, attrib_list, configs, config_size, num_config);
        LOAD_EGL(eglChooseConfig)
        std::vector<EGLint> backend_attributes;
        if (!makeBackendConfigAttributes(attrib_list, &backend_attributes)) {
            setFrontendError(EGL_BAD_ATTRIBUTE);
            return EGL_FALSE;
        }
        return egl_eglChooseConfig(dpy, backend_attributes.data(), configs, config_size, num_config);
    }

    EGL_API EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint* value) {
        LOG_D("eglGetConfigAttrib, dpy: %p, config: %p, attribute: %d, value: %p", dpy, config, attribute, value);
        LOAD_EGL(eglGetConfigAttrib)
        const EGLBoolean result = egl_eglGetConfigAttrib(dpy, config, attribute, value);
        if (result == EGL_TRUE && value && (attribute == EGL_RENDERABLE_TYPE || attribute == EGL_CONFORMANT)) {
            *value = frontendRenderableMask(*value);
        }
        return result;
    }

    EGL_API EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win,
                                              const EGLint* attrib_list) {
        LOG_D("eglCreateWindowSurface, dpy: %p, config: %p, win: %p, attrib_list: %p", dpy, config, win, attrib_list);
        LOAD_EGL(eglCreateWindowSurface)
        return egl_eglCreateWindowSurface(dpy, config, win, attrib_list);
    }

    EGL_API EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint* attrib_list) {
        LOG_D("eglCreatePbufferSurface, dpy: %p, config: %p, attrib_list: %p", dpy, config, attrib_list);
        LOAD_EGL(eglCreatePbufferSurface)
        return egl_eglCreatePbufferSurface(dpy, config, attrib_list);
    }

    EGL_API EGLSurface eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap,
                                              const EGLint* attrib_list) {
        LOG_D("eglCreatePixmapSurface, dpy: %p, config: %p, pixmap: %p, attrib_list: "
              "%p",
              dpy, config, pixmap, attrib_list);
        LOAD_EGL(eglCreatePixmapSurface)
        return egl_eglCreatePixmapSurface(dpy, config, pixmap, attrib_list);
    }

    EGL_API EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
        LOG_D("eglDestroySurface, dpy: %p, surface: %p", dpy, surface);
        LOAD_EGL(eglDestroySurface)
        return egl_eglDestroySurface(dpy, surface);
    }

    EGL_API EGLBoolean eglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint* value) {
        LOG_D("eglQuerySurface, dpy: %p, surface: %p, attribute: %d, value: %p", dpy, surface, attribute, value);
        LOAD_EGL(eglQuerySurface)
        return egl_eglQuerySurface(dpy, surface, attribute, value);
    }

    EGL_API EGLBoolean eglBindAPI(EGLenum api) {
        LOG_D("eglBindAPI, api: %d", api);
        LOAD_EGL(eglBindAPI)
        const EGLenum backend_api = api == EGL_OPENGL_API ? EGL_OPENGL_ES_API : api;
        const EGLBoolean result = egl_eglBindAPI(backend_api);
        if (result == EGL_TRUE) frontend_api = api;
        return result;
    }

    EGL_API EGLenum eglQueryAPI(void) {
        LOG_D("eglQueryAPI");
        return frontend_api;
    }

    EGL_API EGLBoolean eglWaitClient(void) {
        LOG_D("eglWaitClient");
        LOAD_EGL(eglWaitClient)
        return egl_eglWaitClient();
    }

    EGL_API EGLBoolean eglReleaseThread(void) {
        LOG_D("eglReleaseThread");
        LOAD_EGL(eglReleaseThread)
        const EGLBoolean result = egl_eglReleaseThread();
        if (result == EGL_TRUE) {
            frontend_api = EGL_OPENGL_ES_API;
            frontend_error = EGL_SUCCESS;
        }
        return result;
    }

    EGL_API EGLSurface eglCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer,
                                                        EGLConfig config, const EGLint* attrib_list) {
        LOG_D("eglCreatePbufferFromClientBuffer, dpy: %p, buftype: %d, buffer: %p, "
              "config: %p, attrib_list: %p",
              dpy, buftype, buffer, config, attrib_list);
        LOAD_EGL(eglCreatePbufferFromClientBuffer)
        return egl_eglCreatePbufferFromClientBuffer(dpy, buftype, buffer, config, attrib_list);
    }

    EGL_API EGLBoolean eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value) {
        LOG_D("eglSurfaceAttrib, dpy: %p, surface: %p, attribute: %d, value: %d", dpy, surface, attribute, value);
        LOAD_EGL(eglSurfaceAttrib)
        return egl_eglSurfaceAttrib(dpy, surface, attribute, value);
    }

    EGL_API EGLBoolean eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer) {
        LOG_D("eglBindTexImage, dpy: %p, surface: %p, buffer: %d", dpy, surface, buffer);
        LOAD_EGL(eglBindTexImage)
        return egl_eglBindTexImage(dpy, surface, buffer);
    }

    EGL_API EGLBoolean eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer) {
        LOG_D("eglReleaseTexImage, dpy: %p, surface: %p, buffer: %d", dpy, surface, buffer);
        LOAD_EGL(eglReleaseTexImage)
        return egl_eglReleaseTexImage(dpy, surface, buffer);
    }

    EGL_API EGLBoolean eglSwapInterval(EGLDisplay dpy, EGLint interval) {
        LOG_D("eglSwapInterval, dpy: %p, interval: %d", dpy, interval);
        LOAD_EGL(eglSwapInterval)
        return egl_eglSwapInterval(dpy, interval);
    }

    EGL_API EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context,
                                        const EGLint* attrib_list) {
        LOG_D("eglCreateContext, dpy: %p, config: %p, share_context: %p, "
              "attrib_list: %p",
              dpy, config, share_context, attrib_list);
        LOAD_EGL(eglCreateContext)
        if (frontend_api != EGL_OPENGL_API) {
            return egl_eglCreateContext(dpy, config, share_context, attrib_list);
        }

        std::vector<EGLint> backend_attributes;
        EGLint frontend_major = 0;
        EGLint frontend_minor = 0;
        EGLint context_error = EGL_SUCCESS;
        if (!makeBackendContextAttributes(attrib_list, &backend_attributes, &frontend_major, &frontend_minor,
                                          &context_error)) {
            setFrontendError(context_error);
            return EGL_NO_CONTEXT;
        }

        LOAD_EGL(eglBindAPI)
        if (egl_eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) return EGL_NO_CONTEXT;

        EGLContext context = egl_eglCreateContext(dpy, config, share_context, backend_attributes.data());
        if (context != EGL_NO_CONTEXT) {
            rememberDesktopContext(context, dpy, frontend_major, frontend_minor);
        }
        return context;
    }

    EGL_API EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
        LOG_D("eglDestroyContext, dpy: %p, ctx: %p", dpy, ctx);
        LOAD_EGL(eglDestroyContext)
        const EGLBoolean result = egl_eglDestroyContext(dpy, ctx);
        if (result == EGL_TRUE) forgetDesktopContext(ctx);
        return result;
    }

    EGL_API EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) {
        LOG_D("eglMakeCurrent, dpy: %p, draw: %p, read: %p, ctx: %p", dpy, draw, read, ctx);
        LOAD_EGL(eglMakeCurrent)
        return egl_eglMakeCurrent(dpy, draw, read, ctx);
    }

    EGL_API EGLContext eglGetCurrentContext(void) {
        LOG_D("eglGetCurrentContext");
        LOAD_EGL(eglGetCurrentContext)
        return egl_eglGetCurrentContext();
    }

    EGL_API EGLSurface eglGetCurrentSurface(EGLint readdraw) {
        LOG_D("eglGetCurrentSurface, readdraw: %d", readdraw);
        LOAD_EGL(eglGetCurrentSurface)
        return egl_eglGetCurrentSurface(readdraw);
    }

    EGL_API EGLDisplay eglGetCurrentDisplay(void) {
        LOG_D("eglGetCurrentDisplay");
        LOAD_EGL(eglGetCurrentDisplay)
        return egl_eglGetCurrentDisplay();
    }

    EGL_API EGLBoolean eglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint* value) {
        LOG_D("eglQueryContext, dpy: %p, ctx: %p, attribute: %d, value: %p", dpy, ctx, attribute, value);
        LOAD_EGL(eglQueryContext)
        const EGLBoolean result = egl_eglQueryContext(dpy, ctx, attribute, value);
        if (result != EGL_TRUE || !value) return result;

        DesktopContextInfo context_info{};
        if (value && findDesktopContext(ctx, &context_info)) {
            if (attribute == EGL_CONTEXT_CLIENT_TYPE) {
                *value = EGL_OPENGL_API;
                return EGL_TRUE;
            }
            if (attribute == EGL_CONTEXT_CLIENT_VERSION) {
                *value = context_info.major;
                return EGL_TRUE;
            }
        }
        return result;
    }

    EGL_API EGLBoolean eglWaitGL(void) {
        LOG_D("eglWaitGL");
        LOAD_EGL(eglWaitGL)
        return egl_eglWaitGL();
    }

    EGL_API EGLBoolean eglWaitNative(EGLint engine) {
        LOG_D("eglWaitNative, engine: %d", engine);
        LOAD_EGL(eglWaitNative)
        return egl_eglWaitNative(engine);
    }

    EGL_API EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
        LOG_D("eglSwapBuffers, dpy: %p, surface: %p", dpy, surface);
        LOAD_EGL(eglSwapBuffers)
        EGLBoolean result;
        if (global_settings.fsr1_setting != FSR1_Quality_Preset::Disabled) {
            ApplyFSR();
            result = egl_eglSwapBuffers(dpy, surface);
            CheckResolutionChange();
        } else {
            result = egl_eglSwapBuffers(dpy, surface);
        }
        return result;
    }

    EGL_API EGLBoolean eglCopyBuffers(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target) {
        LOG_D("eglCopyBuffers, dpy: %p, surface: %p, target: %p", dpy, surface, target);
        LOAD_EGL(eglCopyBuffers)
        return egl_eglCopyBuffers(dpy, surface, target);
    }

    EGL_API EGLDisplay eglGetPlatformDisplay(EGLenum platform, void* native_display, const EGLAttrib* attrib_list) {
        LOG_D("eglGetPlatformDisplay, platform: %d, native_display: %p, attrib_list: "
              "%p",
              platform, native_display, attrib_list);
        LOAD_EGL(eglGetPlatformDisplay)
        return egl_eglGetPlatformDisplay(platform, native_display, attrib_list);
    }

    EGL_API EGLSurface eglCreatePlatformWindowSurface(EGLDisplay dpy, EGLConfig config, void* native_window,
                                                      const EGLAttrib* attrib_list) {
        LOG_D("eglCreatePlatformWindowSurface, dpy: %p, config: %p, native_window: %p, attrib_list: %p", dpy, config,
              native_window, attrib_list);
        LOAD_EGL(eglCreatePlatformWindowSurface)
        return egl_eglCreatePlatformWindowSurface(dpy, config, native_window, attrib_list);
    }

    EGL_API EGLSurface eglCreatePlatformPixmapSurface(EGLDisplay dpy, EGLConfig config, void* native_pixmap,
                                                      const EGLAttrib* attrib_list) {
        LOG_D("eglCreatePlatformPixmapSurface, dpy: %p, config: %p, native_pixmap: %p, attrib_list: %p", dpy, config,
              native_pixmap, attrib_list);
        LOAD_EGL(eglCreatePlatformPixmapSurface)
        return egl_eglCreatePlatformPixmapSurface(dpy, config, native_pixmap, attrib_list);
    }

    EGL_API EGLDisplay eglGetPlatformDisplayEXT(EGLenum platform, void* native_display, const EGLint* attrib_list) {
        LOG_D("eglGetPlatformDisplayEXT, platform: %d, native_display: %p, attrib_list: %p", platform, native_display,
              attrib_list);
        LOAD_EGL(eglGetPlatformDisplayEXT)
        return egl_eglGetPlatformDisplayEXT(platform, native_display, attrib_list);
    }

    EGL_API EGLSurface eglCreatePlatformWindowSurfaceEXT(EGLDisplay dpy, EGLConfig config, void* native_window,
                                                         const EGLint* attrib_list) {
        LOG_D("eglCreatePlatformWindowSurfaceEXT, dpy: %p, config: %p, native_window: %p, attrib_list: %p", dpy, config,
              native_window, attrib_list);
        LOAD_EGL(eglCreatePlatformWindowSurfaceEXT)
        return egl_eglCreatePlatformWindowSurfaceEXT(dpy, config, native_window, attrib_list);
    }

    EGL_API EGLSurface eglCreatePlatformPixmapSurfaceEXT(EGLDisplay dpy, EGLConfig config, void* native_pixmap,
                                                         const EGLint* attrib_list) {
        LOG_D("eglCreatePlatformPixmapSurfaceEXT, dpy: %p, config: %p, native_pixmap: %p, attrib_list: %p", dpy, config,
              native_pixmap, attrib_list);
        LOAD_EGL(eglCreatePlatformPixmapSurfaceEXT)
        return egl_eglCreatePlatformPixmapSurfaceEXT(dpy, config, native_pixmap, attrib_list);
    }

    EGL_API EGLAPI __eglMustCastToProperFunctionPointerType EGLAPIENTRY eglGetProcAddress(const char* procname) {
        return reinterpret_cast<__eglMustCastToProperFunctionPointerType>(glXGetProcAddress(procname));
    }
}
