// MobileGlues - gl/shader.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include <cctype>
#include <cstdio>      // 新增：用于文件写入调试
#include "shader.h"
#include <GL/gl.h>
#include "log.h"
#include "program.h"
#include "../gles/loader.h"
#include "../includes.h"
#include "glsl/glsl_for_es.h"
#include "../config/settings.h"
#include "FSR1/FSR1.h"

#define DEBUG 0

struct shader_t shaderInfo;
UnorderedMap<GLuint, bool> shader_map_is_sampler_buffer_emulated;
UnorderedMap<GLuint, bool> shader_map_is_atomic_counter_emulated;

bool can_run_essl3(unsigned int esversion, const char* glsl) {
    if (strncmp(glsl, "#version 100", 12) == 0) {
        return true;
    }
    
    unsigned int glsl_version = 0;
    if (strncmp(glsl, "#version 300 es", 15) == 0) {
        glsl_version = 300;
    } else if (strncmp(glsl, "#version 310 es", 15) == 0) {
        glsl_version = 310;
    } else if (strncmp(glsl, "#version 320 es", 15) == 0) {
        glsl_version = 320;
    } else {
        return false;
    }
    
    return esversion >= glsl_version;
}

bool is_direct_shader(const char* glsl) {
    bool es3_ability = can_run_essl3(hardware->es_version, glsl);
    return es3_ability;
}

bool check_if_sampler_buffer_used(std::string str) {
    return str.find("samplerBuffer") != std::string::npos;
}

// 检查 GLSL 源码是否携带合法的 ESSL 版本声明
static bool has_valid_essl_version(const std::string& src) {
    size_t pos = src.find("#version");
    if (pos == std::string::npos) {
        // 无版本声明可能是 GLSL 100，视为合法
        return true;
    }
    size_t end = src.find('\n', pos);
    std::string line = src.substr(pos, end - pos);
    // ESSL 版本行一定包含 "es" 关键字
    return line.find("es") != std::string::npos;
}

// 生成一个绝对合法的 ESSL 占位着色器，根据类型适配
static std::string create_safe_placeholder(GLenum shaderType, int esVersion) {
    std::string ver = (esVersion >= 320) ? "#version 320 es\n" : "#version 300 es\n";
    std::string prec = "precision mediump float;\n";
    if (shaderType == GL_VERTEX_SHADER) {
        return ver + prec + "void main() { gl_Position = vec4(0.0); }";
    } else if (shaderType == GL_FRAGMENT_SHADER) {
        return ver + prec + "void main() { }";
    } else {
        // 其他类型（compute/geometry）极少见，给空 main
        return ver + prec + "void main() { }";
    }
}

void glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) {
    LOG()
    shaderInfo.id = 0;
    shaderInfo.converted = "";
    shaderInfo.frag_data_changed = 0;
    
    // 提前获取着色器类型，以便生成安全的占位对象
    GLint shaderType = GL_FRAGMENT_SHADER;
    GLES.glGetShaderiv(shader, GL_SHADER_TYPE, &shaderType);
    
    size_t l = 0;
    for (int i = 0; i < count; i++) l += (length && length[i] >= 0) ? length[i] : strlen(string[i]);
    
    std::string glsl_src, essl_src;
    glsl_src.reserve(l + 1);
    
    if (length) {
        for (int i = 0; i < count; i++) {
            if (length[i] >= 0) glsl_src += std::string_view(string[i], length[i]);
            else glsl_src += string[i];
        }
    } else {
        for (int i = 0; i < count; i++) {
            glsl_src += string[i];
        }
    }
    
    bool is_sampler_buffer_emulated = hardware->emulate_texture_buffer && check_if_sampler_buffer_used(glsl_src);
    
    if (is_direct_shader(glsl_src.c_str())) {
        LOG_D("[INFO] [Shader] Direct shader source: ")
        LOG_D("%s", glsl_src.c_str())
        essl_src = glsl_src;
    } else {
        int glsl_version = getGLSLVersion(glsl_src.c_str());
        LOG_D("[INFO] [Shader] Shader source: ")
        LOG_D("%s", glsl_src.c_str())
        
        int return_code = 0;
        essl_src = GLSLtoGLSLES(glsl_src.c_str(), shaderType, hardware->es_version, glsl_version, return_code);
        
        if (return_code == 1) {
            shader_map_is_atomic_counter_emulated[shader] = true;
            LOG_D("[INFO] [Shader] Atomic counter emulated in shader %d", shader)
        }
        
        if (essl_src.empty()) {
            LOG_E("Failed to convert shader %d.", shader)
            // 不再直接 return，继续向下转为安全占位
        } else {
            LOG_D("\n[INFO] [Shader] Converted Shader source: \n%s", essl_src.c_str())
        }
    }
    
    // 终极安全检查：任何最终要送给 GLES 的源码必须是合法 ESSL
    if (essl_src.empty() || !has_valid_essl_version(essl_src)) {
        if (!essl_src.empty()) {
            LOG_E("[DesktopGlues] Shader %d: 转换结果仍包含桌面 #version，替换为安全占位", shader);
        } else {
            LOG_E("[DesktopGlues] Shader %d: 转换产出为空，使用安全占位防止空白屏幕", shader);
        }
        essl_src = create_safe_placeholder(shaderType, hardware->es_version);
        shaderInfo.id = 0;  // 占位不作为有效原始 shader
    }
    
    // ───── 临时调试转储：将所有转换后的着色器写入 /sdcard/ ─────
    static int dump_counter = 0;
    const char* type_str = (shaderType == GL_VERTEX_SHADER) ? "vert" : 
                           (shaderType == GL_FRAGMENT_SHADER) ? "frag" : "other";
    char dump_path[256];
    snprintf(dump_path, sizeof(dump_path), "/sdcard/dlg_shader_%d_%s.glsl", 
             dump_counter++, type_str);
    FILE* fp = fopen(dump_path, "w");
    if (fp) {
        fputs(essl_src.c_str(), fp);
        fclose(fp);
        LOG_D("[DesktopGlues] Dumped converted shader to %s", dump_path);
    } else {
        LOG_W("[DesktopGlues] Failed to dump shader to %s", dump_path);
    }
    // ───────────────────────────────────────────────────────────
    
    // 统一出口：只调用一次 GLES.glShaderSource，且只传单个拼接好的字符串
    if (!essl_src.empty()) {
        shaderInfo.id = shader;
        shaderInfo.converted = essl_src;
        const char* s[] = { essl_src.c_str() };
        GLES.glShaderSource(shader, 1, s, nullptr);
        
        if (hardware->emulate_texture_buffer)
            shader_map_is_sampler_buffer_emulated[shader] = is_sampler_buffer_emulated;
    } else {
        // 极意外情况，绝不应该发生
        LOG_E("Critical: no shader source supplied for shader %d", shader);
    }
    
    CHECK_GL_ERROR
}

void glGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
    LOG()
    GLES.glGetShaderiv(shader, pname, params);
    
    if (global_settings.ignore_error >= IgnoreErrorLevel::Partial && pname == GL_COMPILE_STATUS && !*params) {
        GLchar infoLog[512];
        GLES.glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        LOG_W_FORCE("Shader %d compilation failed: \n%s", shader, infoLog)
        LOG_W_FORCE("Now try to cheat.")
        *params = GL_TRUE;
    }
    
    CHECK_GL_ERROR
}

GLuint glCreateShader(GLenum shaderType) {
    if (global_settings.fsr1_setting != FSR1_Quality_Preset::Disabled && !fsrInitialized) {
        InitFSRResources();
    }
    
    LOG()
    LOG_D("glCreateShader(%s)", glEnumToString(shaderType))
    
    GLuint shader = GLES.glCreateShader(shaderType);
    
    if (shader != 0 && hardware->emulate_texture_buffer)
        shader_map_is_sampler_buffer_emulated[shader] = false;
    
    CHECK_GL_ERROR
    return shader;
}