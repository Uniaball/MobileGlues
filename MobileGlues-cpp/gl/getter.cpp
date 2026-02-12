// MobileGlues - gl/getter.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "getter.h"
#include "buffer.h"
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <algorithm>
#include <format>
#include <random>
#include "FSR1/FSR1.h"
#include "log.h"
#include "random_string_gen.h"

#define DEBUG 0

Version GLVersion;

// 小工具函数
inline int count_spaces(const std::string& str) {
    return std::count(str.begin(), str.end(), ' ');
}

inline std::vector<std::string> split(const std::string& str, char delim) {
    std::vector<std::string> tokens;
    size_t start = 0, end;
    while ((end = str.find(delim, start)) != std::string::npos) {
        if (end != start)
            tokens.emplace_back(str.substr(start, end - start));
        start = end + 1;
    }
    if (start < str.size())
        tokens.emplace_back(str.substr(start));
    return tokens;
}

void glGetIntegerv(GLenum pname, GLint *params) {
#if DEBUG
    LOG()
    LOG_D("glGetIntegerv, pname: %s", glEnumToString(pname))
#endif
    
    switch (pname) {
        case GL_NUM_EXTENSIONS + GL_BACKEND_GETTER_MG:
            GLES.glGetIntegerv(pname - GL_BACKEND_GETTER_MG, params);
            return;
        case GL_CONTEXT_PROFILE_MASK:
            *params = GL_CONTEXT_CORE_PROFILE_BIT;
            break;
        case GL_NUM_EXTENSIONS: {
            static GLint num_extensions = -1;
            if (num_extensions == -1) {
                const GLubyte* ext_str = glGetString(GL_EXTENSIONS);
                if (ext_str) {
                    std::string copy_str((const char*)ext_str);
                    num_extensions = static_cast<GLint>(split(copy_str, ' ').size());
                } else {
                    num_extensions = 0;
                }
            }
            *params = num_extensions;
            break;
        }
        case GL_MAJOR_VERSION:
            *params = GLVersion.Major;
            break;
        case GL_MINOR_VERSION:
            *params = GLVersion.Minor;
            break;
        case GL_MAX_TEXTURE_IMAGE_UNITS: {
            int es_params = 16;
            GLES.glGetIntegerv(pname, &es_params);
            *params = es_params * 2;
            break;
        }
        case GL_CONTEXT_FLAGS:
            *params = GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT | GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT | GL_CONTEXT_FLAG_NO_ERROR_BIT;
            break;
        case GL_ARRAY_BUFFER_BINDING:
        case GL_ATOMIC_COUNTER_BUFFER_BINDING:
        case GL_COPY_READ_BUFFER_BINDING:
        case GL_COPY_WRITE_BUFFER_BINDING:
        case GL_DRAW_INDIRECT_BUFFER_BINDING:
        case GL_DISPATCH_INDIRECT_BUFFER_BINDING:
        case GL_ELEMENT_ARRAY_BUFFER_BINDING:
        case GL_PIXEL_PACK_BUFFER_BINDING:
        case GL_PIXEL_UNPACK_BUFFER_BINDING:
        case GL_SHADER_STORAGE_BUFFER_BINDING:
        case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
        case GL_UNIFORM_BUFFER_BINDING:
            *params = static_cast<GLint>(find_bound_buffer(pname));
            break;
        case GL_VERTEX_ARRAY_BINDING:
            *params = static_cast<GLint>(find_bound_array());
            break;
        default:
            GLES.glGetIntegerv(pname, params);
    }
    
#if DEBUG
    LOG_D("  -> %d", *params)
    CHECK_GL_ERROR
#endif
}

GLenum glGetError() {
#if DEBUG
    LOG()
    GLenum err = GLES.glGetError();
    if (err != GL_NO_ERROR) {
        LOG_W("glGetError\n -> %d", err)
        LOG_W("Now try to cheat.")
    }
#endif
    return GL_NO_ERROR;
}

// 扩展列表缓存
static std::string es_ext;

std::string GetExtensionsList() {
    return es_ext;
}

void InitGLESBaseExtensions() {
    std::vector<std::string> extensions;

    if (global_settings.hide_mg_env_level == HideMGEnvLevel::Disabled) {
        extensions.push_back("GL_MG_mobileglues");
        extensions.push_back("GL_MG_backend_string_getter_access");
        extensions.push_back("GL_MG_settings_string_dump");
    }

    const char* base_exts[] = {
        "GL_ARB_fragment_program", "GL_ARB_vertex_buffer_object", "GL_ARB_vertex_array_object",
        "GL_ARB_vertex_buffer", "GL_EXT_vertex_array", "GL_ARB_ES2_compatibility",
        "GL_ARB_ES3_compatibility", "GL_EXT_packed_depth_stencil", "GL_EXT_depth_texture",
        "GL_ARB_depth_texture", "GL_ARB_shading_language_100", "GL_ARB_imaging",
        "GL_ARB_draw_buffers_blend", "OpenGL15", "GL_ARB_shader_storage_buffer_object",
        "GL_ARB_shader_image_load_store", "GL_ARB_clear_texture", "GL_ARB_get_program_binary",
        "GL_ARB_separate_shader_objects", "GL_ARB_multi_bind", "GL_KHR_no_error"
    };

    extensions.insert(extensions.end(), std::begin(base_exts), std::end(base_exts));

    // 保留原始随机化逻辑
    if (global_settings.hide_mg_env_level >= HideMGEnvLevel::Level1) {
        for (int i = extensions.size() - 1; i > 0; --i) {
            int j = rand() % (i + 1);
            std::swap(extensions[i], extensions[j]);
        }
    }

    es_ext.clear();
    for (const auto& ext : extensions) {
        es_ext += ext;
        es_ext += " ";
    }
}

void AppendExtension(const char* ext) {
    es_ext += ext;
    es_ext += ' ';
}

inline std::string getBeforeThirdSpace(const std::string& str) {
    int spaceCount = 0;
    size_t endPos = 0;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == ' ') {
            spaceCount++;
            if (spaceCount == 3) {
                endPos = i;
                break;
            }
        }
        if (spaceCount < 3) endPos = str.length();
    }
    return str.substr(0, endPos);
}

// GPU名称缓存
std::string getGpuName() {
    static std::string lastGpuName;
    if (!lastGpuName.empty()) return lastGpuName;
    
    std::string gpuName = reinterpret_cast<const char*>(GLES.glGetString(GL_RENDERER));
    if (gpuName.empty()) return "<unknown>";

    if (gpuName.find("MetalANGLE, ANGLE") != std::string::npos) {
        if (gpuName.length() >= 25) {
            std::string gpu = gpuName.substr(23, gpuName.length() - 24);
            lastGpuName = gpu + " | MetalANGLE | Metal";
        } else {
            lastGpuName = gpuName;
        }
    } else if (gpuName.rfind("ANGLE", 0) == 0 && gpuName.find("Vulkan") != std::string::npos) {
        size_t firstParen = gpuName.find('(');
        size_t secondParen = gpuName.find('(', firstParen + 1);
        size_t lastParen = gpuName.rfind('(');
        std::string gpu = gpuName.substr(secondParen + 1, lastParen - secondParen - 2);
        size_t vulkanStart = gpuName.find("Vulkan ");
        size_t vulkanEnd = gpuName.find(' ', vulkanStart + 7);
        std::string vulkanVersion = gpuName.substr(vulkanStart + 7, vulkanEnd - (vulkanStart + 7));
        lastGpuName = gpu + " | ANGLE | Vulkan " + vulkanVersion;
    } else {
        lastGpuName = gpuName;
    }
    
    return lastGpuName;
}

void set_es_version() {
    std::string ESVersionStr = getBeforeThirdSpace(reinterpret_cast<const char*>(GLES.glGetString(GL_VERSION)));
    int major = 0, minor = 0;
    if (sscanf(ESVersionStr.c_str(), "OpenGL ES %d.%d", &major, &minor) == 2) {
        hardware->es_version = major * 100 + minor * 10;
    } else {
        hardware->es_version = 300;
    }
#if DEBUG
    LOG_I("OpenGL ES Version: %s (%d)", ESVersionStr.c_str(), hardware->es_version)
    if (hardware->es_version < 300) {
        LOG_I("OpenGL ES version is lower than 3.0! This version is not supported!")
    }
#endif
}

std::string getGLESName() {
    return getBeforeThirdSpace(reinterpret_cast<const char*>(GLES.glGetString(GL_VERSION)));
}

// 字符串缓存
static std::string rendererString;
static std::string vendorString;
static std::string versionString;
static std::string shadingLangString;

const GLubyte * glGetString(GLenum name) {
#if DEBUG
    LOG()
    LOG_D("glGetString, %s", glEnumToString(name))
#endif
    
    switch (name) {
        case GL_VENDOR:
            if (vendorString.empty()) {
                if (global_settings.hide_mg_env_level == HideMGEnvLevel::Disabled) {
                    vendorString = "Swung0x48, BZLZHH, Tungsten, Uniaball";
                } else {
                    const char choices[] = "AINM";
                    vendorString = choices[rand() % 4];

                    RandomStringOptions randStrOpts;
                    randStrOpts.includeDigits = false;
                    randStrOpts.minLength = 3;
                    randStrOpts.maxLength = 8;
                    randStrOpts.includeLowercase = false;
                    randStrOpts.includeUppercase = false;
                    randStrOpts.customChars = "IMenaNtMseAVlD";
                    vendorString += GenerateRandomString(randStrOpts);
                }
            }
            return reinterpret_cast<const GLubyte*>(vendorString.c_str());
            
        case GL_VERSION:
            if (versionString.empty()) {
                versionString = GLVersion.toString();
                if (global_settings.hide_mg_env_level == HideMGEnvLevel::Disabled) {
                    if (GLVersion.toInt(2) == DEFAULT_GL_VERSION) {
                        versionString += " DesktopGlues ";
                    } else {
                        Version defaultVersion = Version(DEFAULT_GL_VERSION);
                        versionString += " §4§l(" + defaultVersion.toString() + ") DesktopGlues§r ";
                    }
                    
                    versionString += std::to_string(MAJOR) + "." + std::to_string(MINOR) + "." + std::to_string(REVISION);
#if PATCH != 0
                    versionString += "." + std::to_string(PATCH);
#endif
#if defined(VERSION_TYPE)
#if VERSION_TYPE == VERSION_ALPHA
                    versionString += "·Alpha";
#elif VERSION_TYPE == VERSION_BETA
                    versionString += "·Beta";
#elif VERSION_TYPE == VERSION_DEVELOPMENT
                    versionString += "·Dev";
#elif VERSION_TYPE == VERSION_RC
                    versionString += "·RC" + std::to_string(VERSION_RC_NUMBER);
#endif
#endif
                    versionString += VERSION_SUFFIX;
                } else {
                    const char choices[] = "AIN";
                    versionString += " ";
                    versionString += choices[rand() % 3];

                    RandomStringOptions randStrOpts;
                    randStrOpts.includeDigits = false;
                    randStrOpts.customChars = " ";
                    versionString += GenerateRandomString(randStrOpts);

                    RandomStringOptions randStrOpts2;
                    randStrOpts2.includeDigits = false;
                    randStrOpts2.includeUppercase = false;
                    randStrOpts2.minLength = 1;
                    randStrOpts2.maxLength = 4;

                    versionString += std::to_string(MAJOR) + GenerateRandomString(randStrOpts2) + std::to_string(MINOR) +
                                     GenerateRandomString(randStrOpts2) + std::to_string(REVISION) +
                                     GenerateRandomString(randStrOpts2) + std::to_string(PATCH) +
                                     GenerateRandomString(randStrOpts2);
                }
            }
            return reinterpret_cast<const GLubyte*>(versionString.c_str());
            
        case GL_RENDERER:
            if (rendererString.empty()) {
                if (global_settings.hide_mg_env_level == HideMGEnvLevel::Disabled) {
                    rendererString = getGpuName() + " | " + getGLESName();
                } else {
                    const char choices[] = "AINM";
                    rendererString = choices[rand() % 4];

                    RandomStringOptions randStrOpts;
                    randStrOpts.includeDigits = true;
                    randStrOpts.minLength = 6;
                    randStrOpts.maxLength = 12;
                    randStrOpts.includeLowercase = false;
                    randStrOpts.includeUppercase = false;
                    randStrOpts.customChars = "IRMenaNtfsoerAceVlDG";
                    rendererString += GenerateRandomString(randStrOpts);

                    int junkInfoTime = rand() % 3 + 1;
                    for (int i = 0; i < junkInfoTime; ++i) {
                        rendererString += " ";
                        RandomStringOptions randStrOpts2;
                        randStrOpts2.minLength = 3;
                        randStrOpts2.maxLength = 6;
                        randStrOpts2.includeLowercase = false;
                        randStrOpts2.includeUppercase = false;
                        randStrOpts2.customChars = "IRenaNtfsoerAcieVDcsG";
                        rendererString += GenerateRandomString(randStrOpts2);
                    }
                }
            }
            return reinterpret_cast<const GLubyte*>(rendererString.c_str());
            
        case GL_SHADING_LANGUAGE_VERSION:
            if (shadingLangString.empty()) {
                std::string baseVer = (hardware->es_version < 310) ? "4.00" : "4.60";
                
                if (global_settings.hide_mg_env_level >= HideMGEnvLevel::Level1) {
                    shadingLangString = baseVer;
                    
                    int junkCount = rand() % 2 + 1;
                    for (int i = 0; i < junkCount; ++i) {
                        shadingLangString += " ";
                        RandomStringOptions junkOpts;
                        junkOpts.minLength = 2;
                        junkOpts.maxLength = 5;
                        junkOpts.includeLowercase = false;
                        junkOpts.includeUppercase = false;
                        junkOpts.customChars = "IAneNDtVsaMIl";
                        shadingLangString += GenerateRandomString(junkOpts);
                    }
                } else {
                    shadingLangString = baseVer + " DesktopGlues with glslang and SPIRV-Cross";
                }
            }
            return reinterpret_cast<const GLubyte*>(shadingLangString.c_str());
            
        case GL_EXTENSIONS: {
#if defined(__APPLE__)
            static std::string* appleCache = new std::string(GetExtensionsList());
            return reinterpret_cast<const GLubyte*>(appleCache->c_str());
#else
            static const std::string& cached = GetExtensionsList();
            return reinterpret_cast<const GLubyte*>(cached.c_str());
#endif
        }
            
        case GL_SETTINGS_MG: {
            if (global_settings.hide_mg_env_level >= HideMGEnvLevel::Level1) 
                return GLES.glGetString(name);
                
            static char* settings_string = nullptr;
            if (settings_string) free(settings_string);
            std::string tmp = dump_settings_string("  ");
            settings_string = strdup(tmp.c_str());
            return reinterpret_cast<const GLubyte*>(settings_string);
        }
            
        case GL_VERSION + GL_BACKEND_GETTER_MG:
        case GL_VENDOR + GL_BACKEND_GETTER_MG:
        case GL_RENDERER + GL_BACKEND_GETTER_MG:
        case GL_EXTENSIONS + GL_BACKEND_GETTER_MG:
        case GL_SHADING_LANGUAGE_VERSION + GL_BACKEND_GETTER_MG:
            if (global_settings.hide_mg_env_level == HideMGEnvLevel::Disabled)
                return GLES.glGetString(name - GL_BACKEND_GETTER_MG);
            else
                return GLES.glGetString(name);
                
        default:
            return GLES.glGetString(name);
    }
}

// 字符串索引缓存
const GLubyte * glGetStringi(GLenum name, GLuint index) {
#if DEBUG
    LOG()
#endif
    
    struct StringCache {
        GLenum name;
        std::vector<std::string> parts;
    };
    
    static std::vector<StringCache> caches;
    static bool initialized = false;
    
    if (name == GL_EXTENSIONS + GL_BACKEND_GETTER_MG && 
        global_settings.hide_mg_env_level == HideMGEnvLevel::Disabled) {
        return GLES.glGetStringi(name - GL_BACKEND_GETTER_MG, index);
    }

    if (!initialized) {
        caches = {
            {GL_EXTENSIONS, {}},
            {GL_VENDOR, {}},
            {GL_VERSION, {}},
            {GL_SHADING_LANGUAGE_VERSION, {}}
        };
        
        for (auto &cache : caches) {
            std::string str;
            switch (cache.name) {
                case GL_VENDOR:
                    str = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
                    cache.parts = split(str, ',');
                    for (auto& s : cache.parts) {
                        size_t start = s.find_first_not_of(" ");
                        if (start != std::string::npos) s = s.substr(start);
                    }
                    break;
                case GL_VERSION:
                    str = reinterpret_cast<const char*>(glGetString(GL_VERSION));
                    cache.parts = split(str, ' ');
                    break;
                case GL_SHADING_LANGUAGE_VERSION:
                    str = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
                    cache.parts = split(str, ' ');
                    break;
                case GL_EXTENSIONS:
                    str = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
                    cache.parts = split(str, ' ');
                    break;
            }
        }
        initialized = true;
    }
    
    for (auto &cache : caches) {
        if (cache.name == name) {
            if (index < cache.parts.size()) {
                return reinterpret_cast<const GLubyte*>(cache.parts[index].c_str());
            }
            return nullptr;
        }
    }
    
    return GLES.glGetStringi(name, index);
}

void glGetQueryObjectiv(GLuint id, GLenum pname, GLint* params) {
#if DEBUG
    LOG()
#endif
    if (GLES.glGetQueryObjectivEXT) {
        GLES.glGetQueryObjectivEXT(id, pname, params);
    }
#if DEBUG
    CHECK_GL_ERROR
#endif
}

void glGetQueryObjecti64v(GLuint id, GLenum pname, GLint64* params) {
#if DEBUG
    LOG()
#endif
    if (GLES.glGetQueryObjecti64vEXT) {
        GLES.glGetQueryObjecti64vEXT(id, pname, params);
    }
#if DEBUG
    CHECK_GL_ERROR
#endif
}
