// MobileGlues - config/settings.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "settings.h"
#include "config.h"
#include "../gl/log.h"
#include "../gl/envvars.h"
#include "gpu_utils.h"
#include "../gl/getter.h"

#define DEBUG 0

global_settings_t global_settings;

void init_settings() {
#if defined(__APPLE__)
    global_settings.angle = AngleMode::Disabled;
    global_settings.ignore_error = IgnoreErrorLevel::Partial;
    global_settings.ext_compute_shader = false;
    global_settings.max_glsl_cache_size = 30 * 1024 * 1024;
    global_settings.angle_depth_clear_fix_mode = AngleDepthClearFixMode::Disabled;
    global_settings.ext_direct_state_access = true;
    global_settings.custom_gl_version = {0, 0, 0}; // will go default
    global_settings.fsr1_setting = FSR1_Quality_Preset::Disabled;
    global_settings.hide_mg_env_level = HideMGEnvLevel::Disabled;

#else

    int success = initialized;
    if (!success) {
        success = config_refresh();
        if (!success) {
            LOG_V("Failed to load config. Use default config.")
        }
    }

    AngleConfig angleConfig =
        success ? static_cast<AngleConfig>(config_get_int("enableANGLE")) : AngleConfig::DisableIfPossible;
    NoErrorConfig noErrorConfig =
        success ? static_cast<NoErrorConfig>(config_get_int("enableNoError")) : NoErrorConfig::Auto;
    bool enableExtComputeShader = success ? (config_get_int("enableExtComputeShader") > 0) : false;
    bool enableExtTimerQuery = success ? (config_get_int("enableExtTimerQuery") > 0) : false;
    bool enableExtDirectStateAccess = success ? (config_get_int("enableExtDirectStateAccess") > 0) : false;
    AngleDepthClearFixMode angleDepthClearFixMode =
        success ? static_cast<AngleDepthClearFixMode>(config_get_int("angleDepthClearFixMode"))
                : AngleDepthClearFixMode::Disabled;
    int customGLVersionInt = success ? config_get_int("customGLVersion") : DEFAULT_GL_VERSION;
    FSR1_Quality_Preset fsr1Setting =
        success ? static_cast<FSR1_Quality_Preset>(config_get_int("fsr1Setting")) : FSR1_Quality_Preset::Disabled;
    HideMGEnvLevel hideMGEnvLevel =
        success ? static_cast<HideMGEnvLevel>(config_get_int("hideMGEnvLevel")) : HideMGEnvLevel::Disabled;

    if (customGLVersionInt < 0) {
        customGLVersionInt = 0;
    }

    size_t maxGlslCacheSize = 0;
    if (config_get_int("maxGlslCacheSize") > 0) {
        maxGlslCacheSize = success ? config_get_int("maxGlslCacheSize") * 1024 * 1024 : 0;
    }

    if (static_cast<int>(angleConfig) < 0 || static_cast<int>(angleConfig) > 3) {
        angleConfig = AngleConfig::EnableIfPossible;
    }
    if (static_cast<int>(noErrorConfig) < 0 || static_cast<int>(noErrorConfig) > 3) {
        noErrorConfig = NoErrorConfig::Auto;
    }
    if (static_cast<int>(angleDepthClearFixMode) < 0 ||
        static_cast<int>(angleDepthClearFixMode) >= static_cast<int>(AngleDepthClearFixMode::MaxValue)) {
        angleDepthClearFixMode = AngleDepthClearFixMode::Disabled;
    }
    if (customGLVersionInt > 46) {
        customGLVersionInt = 46;
    } else if (customGLVersionInt < 32 && customGLVersionInt != 0) {
        customGLVersionInt = 32;
    } else if (customGLVersionInt > 33 && customGLVersionInt < 40) {
        customGLVersionInt = 33;
    } else if (customGLVersionInt == 0) {
        customGLVersionInt = DEFAULT_GL_VERSION;
    }
    if (static_cast<int>(fsr1Setting) < 0 ||
        static_cast<int>(fsr1Setting) >= static_cast<int>(FSR1_Quality_Preset::MaxValue)) {
        fsr1Setting = FSR1_Quality_Preset::Disabled;
    }
    if (static_cast<int>(hideMGEnvLevel) < 0 ||
        static_cast<int>(hideMGEnvLevel) >= static_cast<int>(HideMGEnvLevel::MaxValue)) {
        hideMGEnvLevel = HideMGEnvLevel::Disabled;
    }

    Version customGLVersion(customGLVersionInt);

    int isInPluginApp = 0;
    GetEnvVarInt("MG_PLUGIN_STATUS", &isInPluginApp, 0);
    int fclVersion = 0;
    GetEnvVarInt("FCL_VERSION_CODE", &fclVersion, 0);
    int zlVersion = 0;
    GetEnvVarInt("ZALITH_VERSION_CODE", &zlVersion, 0);
    int pgwVersion = 0;
    GetEnvVarInt("PGW_VERSION_CODE", &pgwVersion, 0);

    LOG_V("MG_DIR_PATH = %s", mg_directory_path ? mg_directory_path : "(default)")

    if (isInPluginApp == 0 && fclVersion == 0 && zlVersion == 0 && pgwVersion == 0 && !is_custom_mg_dir) {
        LOG_V("Unsupported launcher detected, force using default config.")
        angleConfig = AngleConfig::DisableIfPossible;
        noErrorConfig = NoErrorConfig::Auto;
        enableExtComputeShader = false;
        enableExtTimerQuery = true;
        enableExtDirectStateAccess = true;
        maxGlslCacheSize = 0;
        angleDepthClearFixMode = AngleDepthClearFixMode::Disabled;
        fsr1Setting = FSR1_Quality_Preset::Disabled;
        hideMGEnvLevel = HideMGEnvLevel::Disabled;
    }

    AngleMode finalAngleMode = AngleMode::Disabled;
    std::string gpuString = getGPUInfo();
    const char* gpu_cstr = gpuString.c_str();
    LOG_D("GPU: %s", gpu_cstr ? gpu_cstr : "(unknown)")

    int hasVk12 = hasVulkan12();
    int isQcom = isAdreno(gpu_cstr);
    int is730 = isAdreno730(gpu_cstr);
    int is740 = isAdreno740(gpu_cstr);
    int is830 = isAdreno830(gpu_cstr);
    bool isANGLESupported = checkIfANGLESupported(gpu_cstr);

    LOG_D("Has Vulkan 1.2? = %s", hasVk12 ? "true" : "false")
    LOG_D("Is Adreno? = %s", isQcom ? "true" : "false")
    LOG_D("Is Adreno 730? = %s", is730 ? "true" : "false")
    LOG_D("Is Adreno 740? = %s", is740 ? "true" : "false")
    LOG_D("Is Adreno 830? = %s", is830 ? "true" : "false")
    LOG_D("Is ANGLE supported? = %s", isANGLESupported ? "true" : "false")

    switch (angleConfig) {
    case AngleConfig::ForceDisable:
        finalAngleMode = AngleMode::Disabled;
        LOG_D("ANGLE: Force disabled");
        break;

    case AngleConfig::ForceEnable:
        finalAngleMode = AngleMode::Enabled;
        LOG_D("ANGLE: Force enabled");
        break;

    case AngleConfig::EnableIfPossible: {
        finalAngleMode = isANGLESupported ? AngleMode::Enabled : AngleMode::Disabled;
        LOG_D("ANGLE: Conditionally %s", (finalAngleMode == AngleMode::Enabled) ? "enabled" : "disabled");
        break;
    }

    case AngleConfig::DisableIfPossible:
    default:
        finalAngleMode = AngleMode::Disabled;
        break;
    }

    global_settings.angle = finalAngleMode;
    LOG_D("Final ANGLE setting: %d", static_cast<int>(global_settings.angle))
    global_settings.buffer_coherent_as_flush = (global_settings.angle == AngleMode::Disabled);

    if (global_settings.angle == AngleMode::Enabled) {
        // setenv("LIBGL_GLES", "libGLESv2_angle.so", 1);
        // setenv("LIBGL_EGL", "libEGL_angle.so", 1);
    }

    switch (noErrorConfig) {
    case NoErrorConfig::Level1:
        global_settings.ignore_error = IgnoreErrorLevel::Partial;
        LOG_D("Error ignoring: Level 1 (Partial)");
        break;

    case NoErrorConfig::Level2:
        global_settings.ignore_error = IgnoreErrorLevel::Full;
        LOG_D("Error ignoring: Level 2 (Full)");
        break;

    case NoErrorConfig::Auto:
    case NoErrorConfig::Disable:
    default:
        global_settings.ignore_error = IgnoreErrorLevel::None;
        LOG_D("Error ignoring: Disabled");
        break;
    }

    global_settings.ext_compute_shader = enableExtComputeShader;
    global_settings.ext_timer_query = enableExtTimerQuery;
    global_settings.ext_direct_state_access = enableExtDirectStateAccess;
    global_settings.max_glsl_cache_size = maxGlslCacheSize;
    global_settings.angle_depth_clear_fix_mode = angleDepthClearFixMode;
    global_settings.custom_gl_version = customGLVersion;
    global_settings.fsr1_setting = fsr1Setting;
    global_settings.hide_mg_env_level = hideMGEnvLevel;
#endif

    LOG_V("[MobileGlues] Setting: enableAngle                 = %s",
          global_settings.angle == AngleMode::Enabled ? "true" : "false")
    LOG_V("[MobileGlues] Setting: ignoreError                 = %i", static_cast<int>(global_settings.ignore_error))
    LOG_V("[MobileGlues] Setting: enableExtComputeShader      = %s",
          global_settings.ext_compute_shader ? "true" : "false")
    LOG_V("[MobileGlues] Setting: enableExtTimerQuery         = %s", global_settings.ext_timer_query ? "true" : "false")
    LOG_V("[MobileGlues] Setting: enableExtDirectStateAccess  = %s",
          global_settings.ext_direct_state_access ? "true" : "false")
    LOG_V("[MobileGlues] Setting: maxGlslCacheSize            = %i",
          static_cast<int>(global_settings.max_glsl_cache_size / 1024 / 1024))
    LOG_V("[MobileGlues] Setting: angleDepthClearFixMode      = %i",
          static_cast<int>(global_settings.angle_depth_clear_fix_mode))
    LOG_V("[MobileGlues] Setting: bufferCoherentAsFlush       = %i",
          static_cast<int>(global_settings.buffer_coherent_as_flush))
    if (global_settings.custom_gl_version.isEmpty()) {
        LOG_V("[MobileGlues] Setting: customGLVersion             = (default)");
    } else {
        LOG_V("[MobileGlues] Setting: customGLVersion             = %s",
              global_settings.custom_gl_version.toString().c_str());
    }
    LOG_V("[MobileGlues] Setting: fsr1Setting                 = %i", static_cast<int>(global_settings.fsr1_setting))
    LOG_V("[MobileGlues] Setting: hideMGEnvLevel              = %i",
          static_cast<int>(global_settings.hide_mg_env_level))

    GLVersion =
        global_settings.custom_gl_version.isEmpty() ? Version(DEFAULT_GL_VERSION) : global_settings.custom_gl_version;
}

// ---------------------------------------------------------------------------
// Multi-draw backend selection
//
// One key per entry point, carrying a backend NAME rather than an index. Names
// avoid the two problems the single "multidrawMode" integer had: a value that is
// meaningful for one entry point but not another (BaseVertex on
// glMultiDrawElements, which has no base vertex), and a value space that cannot
// grow without renumbering something a user already wrote into config.json.
// ---------------------------------------------------------------------------

namespace {

using B = md_backend_t;
using E = md_entry_t;

constexpr unsigned md_bit(B b) {
    return 1u << static_cast<int>(b);
}

constexpr int MD_ENTRY_COUNT = static_cast<int>(E::MaxValue);
constexpr int MD_LADDER_MAX = 6;

struct md_backend_name_t {
    const char* name;
    B backend;
};

const md_backend_name_t k_md_backend_names[] = {
    {"auto", B::Auto},           {"unroll", B::Unroll},   {"basevertex", B::BaseVertex},
    {"indirect", B::Indirect},   {"multiindirect", B::MultiIndirect}, {"multibasevertex", B::MultiBaseVertex},
    {"multiarrays", B::MultiArrays}, {"compute", B::Compute},
};

struct md_entry_desc_t {
    const char* key;
    const char* label;
    unsigned allowed;             // backends that are a DISTINCT implementation here
    B ladder[MD_LADDER_MAX];      // Auto preference order, terminated by B::MaxValue
    const char* why;              // explains a rejection, so the log says why not just "invalid"
};

const md_entry_desc_t k_md_entries[MD_ENTRY_COUNT] = {
    {"multidrawModeArrays", "glMultiDrawArrays",
     md_bit(B::Auto) | md_bit(B::Unroll) | md_bit(B::MultiArrays) | md_bit(B::MultiIndirect),
     {B::MultiArrays, B::MultiIndirect, B::Unroll, B::MaxValue},
     "glMultiDrawArrays draws no indices, so index-side backends do not apply"},

    // BaseVertex/Compute are excluded: with no base vertex to apply or rebase,
    // they would be the same unrolled loop as Unroll.
    {"multidrawModeElements", "glMultiDrawElements",
     md_bit(B::Auto) | md_bit(B::Unroll) | md_bit(B::Indirect) | md_bit(B::MultiIndirect) | md_bit(B::MultiBaseVertex) |
         md_bit(B::MultiArrays),
     {B::MultiIndirect, B::MultiArrays, B::MultiBaseVertex, B::Indirect, B::Unroll, B::MaxValue},
     "glMultiDrawElements has no base vertex, so basevertex/compute are the same loop as unroll"},

    {"multidrawModeElementsBaseVertex", "glMultiDrawElementsBaseVertex",
     md_bit(B::Auto) | md_bit(B::Unroll) | md_bit(B::BaseVertex) | md_bit(B::Indirect) | md_bit(B::MultiIndirect) |
         md_bit(B::MultiBaseVertex) | md_bit(B::Compute),
     {B::MultiIndirect, B::MultiBaseVertex, B::Indirect, B::BaseVertex, B::Unroll, B::MaxValue},
     "multiarrays (EXT_multi_draw_arrays) carries no base vertex"},

    // These two receive a command buffer from the application; the only choice is
    // whether to hand the whole batch to the driver or walk it one command at a
    // time. "unroll" would mean the same thing as "indirect" here.
    {"multidrawModeArraysIndirect", "glMultiDrawArraysIndirect",
     md_bit(B::Auto) | md_bit(B::Indirect) | md_bit(B::MultiIndirect),
     {B::MultiIndirect, B::Indirect, B::MaxValue},
     "the application supplies the commands, so only indirect/multiindirect exist here"},

    {"multidrawModeElementsIndirect", "glMultiDrawElementsIndirect",
     md_bit(B::Auto) | md_bit(B::Indirect) | md_bit(B::MultiIndirect),
     {B::MultiIndirect, B::Indirect, B::MaxValue},
     "the application supplies the commands, so only indirect/multiindirect exist here"},
};

struct md_caps_t {
    bool basevertex;
    bool indirect_arrays;
    bool indirect_elements;
    bool multiindirect_arrays;
    bool multiindirect_elements;
    bool multibasevertex;
    bool multiarrays;
    bool compute;
};

bool md_is_arrays_side(E e) {
    return e == E::Arrays || e == E::ArraysIndirect;
}

bool md_backend_available(E e, B b, const md_caps_t& c) {
    switch (b) {
    case B::Unroll:
        return true;
    case B::BaseVertex:
        return c.basevertex;
    case B::Indirect:
        return md_is_arrays_side(e) ? c.indirect_arrays : c.indirect_elements;
    case B::MultiIndirect:
        return md_is_arrays_side(e) ? c.multiindirect_arrays : c.multiindirect_elements;
    case B::MultiBaseVertex:
        return c.multibasevertex;
    case B::MultiArrays:
        return c.multiarrays;
    case B::Compute:
        return c.compute;
    default:
        return false;
    }
}

// Case-insensitive, whitespace-tolerant name lookup.
bool md_parse_backend(const std::string& raw, B* out) {
    std::string s;
    for (char ch : raw) {
        if (ch == ' ' || ch == '\t' || ch == '_' || ch == '-') continue;
        s += static_cast<char>(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
    }
    if (s.empty()) return false;
    for (const auto& n : k_md_backend_names) {
        if (s == n.name) {
            *out = n.backend;
            return true;
        }
    }
    return false;
}

std::string md_allowed_list(unsigned allowed) {
    std::string out;
    for (const auto& n : k_md_backend_names) {
        if (allowed & md_bit(n.backend)) {
            if (!out.empty()) out += ", ";
            out += n.name;
        }
    }
    return out;
}

std::string md_config_string(const char* key) {
    const char* v = config_get_string(const_cast<char*>(key));
    // config_get_string hands back a pointer into the live cJSON tree; copy now.
    return v ? std::string(v) : std::string();
}

} // namespace

const char* md_backend_name(md_backend_t b) {
    for (const auto& n : k_md_backend_names) {
        if (n.backend == b) return n.name;
    }
    return "(unknown)";
}

const char* md_backend_suffix(md_backend_t b) {
    switch (b) {
    case B::Unroll:
        return "_drawelements"; // historical symbol name, kept so it stays resolvable
    case B::BaseVertex:
        return "_basevertex";
    case B::Indirect:
        return "_indirect";
    case B::MultiIndirect:
        return "_multiindirect";
    case B::MultiBaseVertex:
        return "_multibasevertex";
    case B::MultiArrays:
        return "_multiarrays";
    case B::Compute:
        return "_compute";
    default:
        return nullptr; // Auto never survives resolution
    }
}

// Parses "multidrawDisableBackends" into a bitmask. A disabled backend is
// treated exactly like one the driver does not have, so it flows through the
// same degradation path -- both here during resolution and in the runtime
// fallback chains in gl/multidraw.cpp, which consult md_backend_disabled().
// Resolution alone would not be enough: a chain can hand control to a backend
// the user asked never to be used.
static unsigned parse_multidraw_disable_mask() {
    const std::string raw = md_config_string("multidrawDisableBackends");
    unsigned mask = 0;
    std::string token;
    for (size_t i = 0; i <= raw.size(); ++i) {
        const char ch = i < raw.size() ? raw[i] : ',';
        if (ch == ',' || ch == ';') {
            bool blank = true;
            for (char c : token)
                if (c != ' ' && c != '\t' && c != '_' && c != '-') blank = false;
            if (!token.empty() && !blank) {
                B b;
                if (!md_parse_backend(token, &b) || b == B::Auto) {
                    LOG_W_FORCE("multidrawDisableBackends: '%s' is not a backend name, ignored", token.c_str())
                } else {
                    mask |= md_bit(b);
                    LOG_W_FORCE("multidraw: backend '%s' disabled by configuration", md_backend_name(b))
                }
                token.clear();
            }
        } else {
            token += ch;
        }
    }
    return mask;
}

// Resolves one entry point: read its key, reject anything that is not a distinct
// implementation there, then walk down to something the driver actually has.
static md_backend_t resolve_multidraw_entry(md_entry_t e, const md_caps_t& caps, unsigned disabled) {
    const md_entry_desc_t& d = k_md_entries[static_cast<int>(e)];

    B want = B::Auto;
    const std::string raw = md_config_string(d.key);
    if (!raw.empty()) {
        if (!md_parse_backend(raw, &want)) {
            LOG_W_FORCE("%s: '%s' is not a backend name; using auto. Accepted: %s", d.key, raw.c_str(),
                        md_allowed_list(d.allowed).c_str())
            want = B::Auto;
        } else if ((d.allowed & md_bit(want)) == 0) {
            LOG_W_FORCE("%s: '%s' is not a distinct strategy for %s (%s); using auto. Accepted: %s", d.key,
                        md_backend_name(want), d.label, d.why, md_allowed_list(d.allowed).c_str())
            want = B::Auto;
        }
    }

    const bool explicit_choice = (want != B::Auto);
    if (explicit_choice) {
        if ((disabled & md_bit(want)) != 0) {
            // Disabling wins over forcing: a disabled backend is indistinguishable
            // from one the driver does not have.
            LOG_W_FORCE("%s: '%s' is disabled by multidrawDisableBackends; falling back", d.key,
                        md_backend_name(want))
        } else if (md_backend_available(e, want, caps)) {
            LOG_V("[MobileGlues] %-32s = %s", d.key, md_backend_name(want))
            return want;
        } else {
            LOG_W_FORCE("%s: '%s' is not supported by this driver; falling back", d.key, md_backend_name(want))
        }
    }

    for (int i = 0; i < MD_LADDER_MAX && d.ladder[i] != B::MaxValue; ++i) {
        const B cand = d.ladder[i];
        if ((disabled & md_bit(cand)) != 0) continue;
        if (!md_backend_available(e, cand, caps)) continue;
        LOG_V("[MobileGlues] %-32s = %s%s", d.key, md_backend_name(cand), explicit_choice ? " (fallback)" : " (auto)")
        return cand;
    }

    // Fall back to the last rung of this entry's own ladder rather than to a
    // fixed backend: Unroll is not a member of the *Indirect entries' sets, and
    // reporting a backend they do not accept would make the log contradict the
    // key's documented values.
    B last = B::Unroll;
    for (int i = 0; i < MD_LADDER_MAX && d.ladder[i] != B::MaxValue; ++i)
        last = d.ladder[i];
    LOG_W_FORCE("%s: every backend is unavailable or disabled; using %s", d.key, md_backend_name(last))
    return last;
}

void set_multidraw_setting() { // should be called after init_gles_target()
    // multidrawMode is no longer read. It selected one strategy for every
    // multi-draw entry point at once, which meant most of its values were
    // meaningless for most entry points. init_settings_post() now resolves one
    // backend per entry point from the multidrawMode<EntryPoint> keys.
    if (config_get_int(const_cast<char*>("multidrawMode")) != -1) {
        LOG_W_FORCE("multidrawMode is no longer used. Each entry point has its own key now: "
                    "multidrawModeArrays, multidrawModeElements, multidrawModeElementsBaseVertex, "
                    "multidrawModeArraysIndirect, multidrawModeElementsIndirect. Values are backend names, "
                    "e.g. \"multiindirect\". See also multidrawDisableBackends.")
    }
}

void init_settings_post() {
    const bool has_es31 = (g_gles_caps.major > 3) || (g_gles_caps.major == 3 && g_gles_caps.minor >= 1);
    const bool has_es32 = (g_gles_caps.major > 3) || (g_gles_caps.major == 3 && g_gles_caps.minor >= 2);
    const bool has_bv_ext =
        g_gles_caps.GL_EXT_draw_elements_base_vertex || g_gles_caps.GL_OES_draw_elements_base_vertex;

    // A capability counts only when the extension string *and* the resolved entry
    // point agree. The GLES loader uses a plain dlsym, so a driver can advertise
    // GL_EXT_multi_draw_indirect while the symbol is missing from the library that
    // was actually opened; trusting the string alone meant a null jump on the
    // first frame that issued a multi-draw.
    const bool multidraw = g_gles_caps.GL_EXT_multi_draw_indirect && GLES.glMultiDrawElementsIndirectEXT != nullptr;
    const bool basevertex = (has_bv_ext || has_es32) && GLES.glDrawElementsBaseVertex != nullptr;
    const bool indirect = has_es31 && GLES.glDrawElementsIndirect != nullptr;
    // EXT/OES_draw_elements_base_vertex also define the multi-draw form, whose
    // signature matches GL 3.2 core exactly -- but only when EXT_multi_draw_arrays
    // is supported as well. gl/multidraw.cpp checks that string, because neither
    // the resolved symbol nor a runtime probe can: a driver without the extension
    // accepts the call, draws nothing, and reports no error.
    const bool multibasevertex = mg_multi_draw_elements_basevertex_ext_available();

    // Compute mode used to be accepted without checking anything at all.
    bool compute = false;
    if (has_es31 && GLES.glDispatchCompute) {
        GLint ssbo_blocks = 0;
        GLES.glGetIntegerv(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, &ssbo_blocks);
        // The multidraw compute shader declares exactly four shader storage
        // blocks, which is the GLES 3.1 guaranteed minimum.
        compute = ssbo_blocks >= 4;
        if (!compute) LOG_W_FORCE("Compute multidraw needs 4 SSBO blocks, driver reports %d", ssbo_blocks)
    }

    // ---- per-entry-point backend selection ----
    md_caps_t md_caps = {};
    md_caps.basevertex = basevertex;
    md_caps.indirect_elements = indirect;
    md_caps.indirect_arrays = has_es31 && GLES.glDrawArraysIndirect != nullptr;
    md_caps.multiindirect_elements = multidraw;
    md_caps.multiindirect_arrays =
        g_gles_caps.GL_EXT_multi_draw_indirect && GLES.glMultiDrawArraysIndirectEXT != nullptr;
    md_caps.multibasevertex = multibasevertex;
    md_caps.multiarrays = mg_multi_draw_arrays_ext_available();
    md_caps.compute = compute;

    global_settings.multidraw_disabled_mask = parse_multidraw_disable_mask();
    for (int i = 0; i < MD_ENTRY_COUNT; ++i) {
        global_settings.multidraw_backend[i] =
            resolve_multidraw_entry(static_cast<md_entry_t>(i), md_caps, global_settings.multidraw_disabled_mask);
    }

}

std::string dump_settings_string(std::string prefix) {
    std::stringstream ss;

    ss << prefix << "Angle: " << (global_settings.angle == AngleMode::Enabled ? "Enabled" : "Disabled") << "\n";
    ss << prefix << "IgnoreError: ";
    switch (global_settings.ignore_error) {
    case IgnoreErrorLevel::None:
        ss << "None";
        break;
    case IgnoreErrorLevel::Partial:
        ss << "Partial";
        break;
    case IgnoreErrorLevel::Full:
        ss << "Full";
        break;
    }
    ss << "\n";

    ss << prefix << "ExtComputeShader: " << (global_settings.ext_compute_shader ? "True" : "False") << "\n";
    ss << prefix << "ExtTimerQuery: " << (global_settings.ext_timer_query ? "True" : "False") << "\n";
    ss << prefix << "ExtDirectStateAccess: " << (global_settings.ext_direct_state_access ? "True" : "False") << "\n";
    ss << prefix << "MaxGlslCacheSize: " << (global_settings.max_glsl_cache_size / 1024 / 1024) << "MB\n";

    for (int i = 0; i < MD_ENTRY_COUNT; ++i) {
        ss << prefix << k_md_entries[i].key << ": "
           << md_backend_name(global_settings.multidraw_backend[i]) << "\n";
    }
    if (global_settings.multidraw_disabled_mask != 0) {
        ss << prefix << "MultidrawDisabledBackends:";
        for (const auto& n : k_md_backend_names) {
            if (global_settings.multidraw_disabled_mask & md_bit(n.backend)) ss << " " << n.name;
        }
        ss << "\n";
    }

    ss << prefix << "AngleDepthClearFixMode: "
       << (global_settings.angle_depth_clear_fix_mode == AngleDepthClearFixMode::Disabled ? "Disabled" : "Enabled")
       << "\n";

    ss << prefix << "BufferCoherentAsFlush: " << (global_settings.buffer_coherent_as_flush ? "True" : "False") << "\n";

    ss << prefix << "CustomGLVersion: "
       << ((GLVersion.toInt(2) == DEFAULT_GL_VERSION) ? "(Default)" : std::to_string(GLVersion.toInt(2))) << "\n";

    ss << prefix << "Fsr1Setting: ";

    switch (global_settings.fsr1_setting) {
    case FSR1_Quality_Preset::Disabled:
        ss << "Disabled";
        break;
    case FSR1_Quality_Preset::UltraQuality:
        ss << "UltraQuality";
        break;
    case FSR1_Quality_Preset::Quality:
        ss << "Quality";
        break;
    case FSR1_Quality_Preset::Balanced:
        ss << "Balanced";
        break;
    case FSR1_Quality_Preset::Performance:
        ss << "Performance";
        break;
    default:
        ss << "Unknown";
        break;
    }
    ss << "\n";

    ss << prefix << "HideMGEnvLevel: "
       << ((global_settings.hide_mg_env_level == HideMGEnvLevel::Disabled)
               ? "Disabled"
               : std::to_string(static_cast<int>(global_settings.hide_mg_env_level)));

    ss << "\n";

    return ss.str();
}
