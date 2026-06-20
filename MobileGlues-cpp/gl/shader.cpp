// MobileGlues - gl/shader.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include <cctype>
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

void glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) {
    LOG()
    shaderInfo.id = 0;
    shaderInfo.converted = "";
    shaderInfo.frag_data_changed = 0;
    
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
    
    bool is_sampler_buffer_emulated = check_if_sampler_buffer_used(glsl_src);
    
    if (is_direct_shader(glsl_src.c_str())) {
        LOG_D("[INFO] [Shader] Direct shader source: ")
        LOG_D("%s", glsl_src.c_str())
        essl_src = glsl_src;
    } else {
        int glsl_version = getGLSLVersion(glsl_src.c_str());
        LOG_D("[INFO] [Shader] Shader source: ")
        LOG_D("%s", glsl_src.c_str())
        
        GLint shaderType;
        GLES.glGetShaderiv(shader, GL_SHADER_TYPE, &shaderType);
        
        int return_code = 0;
        essl_src = GLSLtoGLSLES(glsl_src.c_str(), shaderType, hardware->es_version, glsl_version, return_code);
        
        if (return_code == 1) {
            // atomicCounterEmulated
            shader_map_is_atomic_counter_emulated[shader] = true;
            LOG_D("[INFO] [Shader] Atomic counter emulated in shader %d", shader)
        }
        
        if (essl_src.empty()) {
            LOG_E("Failed to convert shader %d.", shader)
            return;
        }
        
        LOG_D("\n[INFO] [Shader] Converted Shader source: \n%s", essl_src.c_str())
    }
    
    if (!essl_src.empty()) {
        shaderInfo.id = shader;
        shaderInfo.converted = essl_src;

        // ================= 新增：输出最终 ESSL 源码 =================
        GLint shaderType = 0;
        GLES.glGetShaderiv(shader, GL_SHADER_TYPE, &shaderType);
        const char* typeName = "UNKNOWN";
        switch (shaderType) {
            case GL_VERTEX_SHADER:          typeName = "VERTEX"; break;
            case GL_FRAGMENT_SHADER:        typeName = "FRAGMENT"; break;
            case GL_COMPUTE_SHADER:         typeName = "COMPUTE"; break;
            case GL_GEOMETRY_SHADER:        typeName = "GEOMETRY"; break;
            case GL_TESS_CONTROL_SHADER:    typeName = "TESS_CTRL"; break;
            case GL_TESS_EVALUATION_SHADER: typeName = "TESS_EVAL"; break;
        }
        LOG_I("======== FINAL ESSL (Shader %u, %s) ========", shader, typeName);
        LOG_I("%s", essl_src.c_str());
        LOG_I("==============================================");
        // =========================================================

        const char* s[] = {essl_src.c_str()};
        GLES.glShaderSource(shader, count, s, nullptr);   
        shader_map_is_sampler_buffer_emulated[shader] = is_sampler_buffer_emulated;
    } else {
        LOG_E("Failed to convert glsl.")
    }
    
    CHECK_GL_ERROR
}

void glGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
    LOG()
    GLES.glGetShaderiv(shader, pname, params);
    
    // ===== 临时注释掉强制成功逻辑，以获取驱动真实的编译错误 =====
    // （如需查看编译错误，请删除下面这段注释符号，然后重新编译）
    /*
    if (global_settings.ignore_error >= IgnoreErrorLevel::Partial && pname == GL_COMPILE_STATUS && !*params) {
        GLchar infoLog[512];
        GLES.glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        LOG_W_FORCE("Shader %d compilation failed: \n%s", shader, infoLog)
        LOG_W_FORCE("Now try to cheat.")
        *params = GL_TRUE;
    }
    */
    // =============================================================
    
    CHECK_GL_ERROR
}

GLuint glCreateShader(GLenum shaderType) {
    if (global_settings.fsr1_setting != FSR1_Quality_Preset::Disabled && !fsrInitialized) {
        InitFSRResources();
    }
    
    LOG()
    LOG_D("glCreateShader(%s)", glEnumToString(shaderType))
    
    GLuint shader = GLES.glCreateShader(shaderType);
    
    if (shader != 0) shader_map_is_sampler_buffer_emulated[shader] = false;
    
    CHECK_GL_ERROR
    return shader;
}