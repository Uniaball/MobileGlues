//
// Created by Swung 0x48 on 2024/10/7.
//

#include "config/settings.h"
#include "egl/egl.h"
#include "egl/loader.h"
#include "gl/envvars.h"
#include "gl/gl.h"
#include "gl/log.h"
#include "gl/mg.h"
#include "gles/loader.h"
#include "includes.h"
#include <cerrno>
#include <cstring>
#include <sys/stat.h>

#define DEBUG 0

#ifndef __APPLE__
__attribute__((used))
#endif
const char *license = "GNU LGPL-2.1 License";

void init_config() {
    if (check_path())
        config_refresh();
}

void show_license() {
    LOG_V("The Open Source License of MobileGlues: ");
    LOG_V("  %s", license);
}

#if PROFILING

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

void init_perfetto() {
    perfetto::TracingInitArgs args;

    args.backends |= perfetto::kSystemBackend;

    perfetto::Tracing::Initialize(args);
    perfetto::TrackEvent::Register();
}
#endif

void proc_init() {
    init_config();

    clear_log();
    start_log();

    LOG_V("Initializing %s ...", RENDERERNAME);
    show_license();

    init_settings();

    load_libs();
    init_target_egl();
    init_target_gles();
    set_multidraw_setting();

    init_settings_post();

#if PROFILING
    init_perfetto();
#endif

    // Cleanup
#ifndef __APPLE__
    destroy_temp_egl_ctx();
#endif
    g_initialized = 1;
}
