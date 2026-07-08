// MobileGlues - gl/glsl/glsl_for_es.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "glsl_for_es.h"

#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <spirv_cross/spirv_cross_c.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <vector>
#include <set>
#include <map>
#include <cctype>
#include <cstring>
#include "cache.h"
#include "../../version.h"

#define DEBUG 0

const char* atomicCounterEmulatedWatermark = "// Non-opaque atomic uniform converted to SSBO";

static TBuiltInResource InitResources() {
    TBuiltInResource Resources{};
    Resources.maxLights = 32;
    Resources.maxClipPlanes = 6;
    Resources.maxTextureUnits = 32;
    Resources.maxTextureCoords = 32;
    Resources.maxVertexAttribs = 64;
    Resources.maxVertexUniformComponents = 4096;
    Resources.maxVaryingFloats = 64;
    Resources.maxVertexTextureImageUnits = 32;
    Resources.maxCombinedTextureImageUnits = 80;
    Resources.maxTextureImageUnits = 32;
    Resources.maxFragmentUniformComponents = 4096;
    Resources.maxDrawBuffers = 32;
    Resources.maxVertexUniformVectors = 128;
    Resources.maxVaryingVectors = 8;
    Resources.maxFragmentUniformVectors = 16;
    Resources.maxVertexOutputVectors = 16;
    Resources.maxFragmentInputVectors = 15;
    Resources.minProgramTexelOffset = -8;
    Resources.maxProgramTexelOffset = 7;
    Resources.maxClipDistances = 8;
    Resources.maxComputeWorkGroupCountX = 65535;
    Resources.maxComputeWorkGroupCountY = 65535;
    Resources.maxComputeWorkGroupCountZ = 65535;
    Resources.maxComputeWorkGroupSizeX = 1024;
    Resources.maxComputeWorkGroupSizeY = 1024;
    Resources.maxComputeWorkGroupSizeZ = 64;
    Resources.maxComputeUniformComponents = 1024;
    Resources.maxComputeTextureImageUnits = 16;
    Resources.maxComputeImageUniforms = 8;
    Resources.maxComputeAtomicCounters = 8;
    Resources.maxComputeAtomicCounterBuffers = 1;
    Resources.maxVaryingComponents = 60;
    Resources.maxVertexOutputComponents = 64;
    Resources.maxGeometryInputComponents = 64;
    Resources.maxGeometryOutputComponents = 128;
    Resources.maxFragmentInputComponents = 128;
    Resources.maxImageUnits = 8;
    Resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
    Resources.maxCombinedShaderOutputResources = 8;
    Resources.maxImageSamples = 0;
    Resources.maxVertexImageUniforms = 0;
    Resources.maxTessControlImageUniforms = 0;
    Resources.maxTessEvaluationImageUniforms = 0;
    Resources.maxGeometryImageUniforms = 0;
    Resources.maxFragmentImageUniforms = 8;
    Resources.maxCombinedImageUniforms = 8;
    Resources.maxGeometryTextureImageUnits = 16;
    Resources.maxGeometryOutputVertices = 256;
    Resources.maxGeometryTotalOutputComponents = 1024;
    Resources.maxGeometryUniformComponents = 1024;
    Resources.maxGeometryVaryingComponents = 64;
    Resources.maxTessControlInputComponents = 128;
    Resources.maxTessControlOutputComponents = 128;
    Resources.maxTessControlTextureImageUnits = 16;
    Resources.maxTessControlUniformComponents = 1024;
    Resources.maxTessControlTotalOutputComponents = 4096;
    Resources.maxTessEvaluationInputComponents = 128;
    Resources.maxTessEvaluationOutputComponents = 128;
    Resources.maxTessEvaluationTextureImageUnits = 16;
    Resources.maxTessEvaluationUniformComponents = 1024;
    Resources.maxTessPatchComponents = 120;
    Resources.maxPatchVertices = 32;
    Resources.maxTessGenLevel = 64;
    Resources.maxViewports = 16;
    Resources.maxVertexAtomicCounters = 0;
    Resources.maxTessControlAtomicCounters = 0;
    Resources.maxTessEvaluationAtomicCounters = 0;
    Resources.maxGeometryAtomicCounters = 0;
    Resources.maxFragmentAtomicCounters = 8;
    Resources.maxCombinedAtomicCounters = 8;
    Resources.maxAtomicCounterBindings = 1;
    Resources.maxVertexAtomicCounterBuffers = 0;
    Resources.maxTessControlAtomicCounterBuffers = 0;
    Resources.maxTessEvaluationAtomicCounterBuffers = 0;
    Resources.maxGeometryAtomicCounterBuffers = 0;
    Resources.maxFragmentAtomicCounterBuffers = 1;
    Resources.maxCombinedAtomicCounterBuffers = 1;
    Resources.maxAtomicCounterBufferSize = 16384;
    Resources.maxTransformFeedbackBuffers = 4;
    Resources.maxTransformFeedbackInterleavedComponents = 64;
    Resources.maxCullDistances = 8;
    Resources.maxCombinedClipAndCullDistances = 8;
    Resources.maxSamples = 4;
    Resources.maxMeshOutputVerticesNV = 256;
    Resources.maxMeshOutputPrimitivesNV = 512;
    Resources.maxMeshWorkGroupSizeX_NV = 32;
    Resources.maxMeshWorkGroupSizeY_NV = 1;
    Resources.maxMeshWorkGroupSizeZ_NV = 1;
    Resources.maxTaskWorkGroupSizeX_NV = 32;
    Resources.maxTaskWorkGroupSizeY_NV = 1;
    Resources.maxTaskWorkGroupSizeZ_NV = 1;
    Resources.maxMeshViewCountNV = 4;
    Resources.limits.nonInductiveForLoops = true;
    Resources.limits.whileLoops = true;
    Resources.limits.doWhileLoops = true;
    Resources.limits.generalUniformIndexing = true;
    Resources.limits.generalAttributeMatrixVectorIndexing = true;
    Resources.limits.generalVaryingIndexing = true;
    Resources.limits.generalSamplerIndexing = true;
    Resources.limits.generalVariableIndexing = true;
    Resources.limits.generalConstantMatrixVectorIndexing = true;
    return Resources;
}

int getGLSLVersion(const char* glsl_code) {
    const char* p = strstr(glsl_code, "#version");
    if (!p) return -1;
    p += 8;
    while (*p == ' ' || *p == '\t') ++p;
    int version = 0;
    while (*p >= '0' && *p <= '9') {
        version = version * 10 + (*p - '0');
        ++p;
    }
    return version;
}

static inline void replace_all(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

static inline void trim(std::string& s) {
    s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), ::isspace));
    s.erase(std::find_if_not(s.rbegin(), s.rend(), ::isspace).base(), s.end());
}

static std::string removeLayoutBinding(const std::string& glslCode) {
    std::string result;
    result.reserve(glslCode.size());
    size_t pos = 0;
    while (pos < glslCode.size()) {
        size_t start = glslCode.find("layout(", pos);
        if (start == std::string::npos) {
            result.append(glslCode, pos, std::string::npos);
            break;
        }
        result.append(glslCode, pos, start - pos);
        size_t end = start + 7;
        int depth = 1;
        while (end < glslCode.size() && depth > 0) {
            if (glslCode[end] == '(') ++depth;
            else if (glslCode[end] == ')') --depth;
            ++end;
        }
        std::string inside = glslCode.substr(start + 7, end - start - 8);
        auto removeAttribute = [&](const std::string& attr) {
            size_t p = inside.find(attr);
            while (p != std::string::npos) {
                size_t eq = inside.find('=', p);
                if (eq == std::string::npos) break;
                size_t endAttr = inside.find_first_of(",)", eq);
                if (endAttr == std::string::npos) endAttr = inside.size();
                inside.erase(p, endAttr - p);
                if (p < inside.size() && inside[p] == ',') {
                    inside.erase(p, 1);
                } else if (p > 0 && inside[p-1] == ',') {
                    inside.erase(p-1, 1);
                }
                p = inside.find(attr, p);
            }
        };
        removeAttribute("set");
        removeAttribute("binding");
        trim(inside);
        if (!inside.empty()) {
            result += "layout(" + inside + ")";
        }
        pos = end;
    }
    return result;
}

static std::string processOutColorLocations(const std::string& glslCode) {
    std::string result;
    result.reserve(glslCode.size());
    size_t pos = 0;
    while (pos < glslCode.size()) {
        size_t start = glslCode.find("out ", pos);
        if (start == std::string::npos) {
            result.append(glslCode, pos, std::string::npos);
            break;
        }
        size_t nameStart = start + 4;
        while (nameStart < glslCode.size() && isspace(glslCode[nameStart])) ++nameStart;
        if (glslCode.compare(nameStart, 8, "outColor") == 0) {
            size_t numStart = nameStart + 8;
            if (numStart < glslCode.size() && isdigit(glslCode[numStart])) {
                size_t numEnd = numStart;
                while (numEnd < glslCode.size() && isdigit(glslCode[numEnd])) ++numEnd;
                std::string num = glslCode.substr(numStart, numEnd - numStart);
                size_t semicolon = glslCode.find(';', numEnd);
                if (semicolon != std::string::npos) {
                    std::string replacement = "layout(location=" + num + ") out highp vec4 outColor" + num + ";";
                    result += replacement;
                    pos = semicolon + 1;
                    continue;
                }
            }
        }
        result.append(glslCode, pos, start - pos + 4);
        pos = start + 4;
    }
    return result;
}

static bool process_non_opaque_atomic_to_ssbo(std::string& source) {
    if (source.find("atomicCounter") == std::string::npos) return false;

    std::string result;
    result.reserve(source.size());
    size_t pos = 0;
    std::map<std::string, std::string> bindingMap;
    std::set<std::string> atomicVars;

    while (pos < source.size()) {
        size_t start = source.find("layout(", pos);
        if (start == std::string::npos) {
            result.append(source, pos, std::string::npos);
            break;
        }
        size_t end = source.find(';', start);
        if (end == std::string::npos) {
            result.append(source, pos, std::string::npos);
            break;
        }
        std::string decl = source.substr(start, end - start + 1);
        if (decl.find("atomic_uint") != std::string::npos) {
            size_t bindPos = decl.find("binding=");
            if (bindPos != std::string::npos) {
                bindPos += 8;
                size_t bindEnd = decl.find_first_of(",)", bindPos);
                if (bindEnd != std::string::npos) {
                    std::string binding = decl.substr(bindPos, bindEnd - bindPos);
                    size_t varStart = decl.find_last_of(" \t") + 1;
                    if (varStart < decl.size()) {
                        size_t varEnd = decl.find(';', varStart);
                        if (varEnd != std::string::npos) {
                            std::string var = decl.substr(varStart, varEnd - varStart);
                            atomicVars.insert(var);
                            bindingMap[var] = binding;
                            std::string replacement = "layout(std430, binding=" + binding + ") buffer AtomicCounterSSBO_" + binding + " {\n    uint " + var + ";\n};\n";
                            result += replacement;
                            pos = end + 1;
                            continue;
                        }
                    }
                }
            }
        }
        result.append(source, start, end - start + 1);
        pos = end + 1;
    }

    if (!atomicVars.empty()) {
        for (const auto& var : atomicVars) {
            size_t p = 0;
            std::string searchInc = "atomicCounterIncrement(" + var + ")";
            std::string searchDec = "atomicCounterDecrement(" + var + ")";
            std::string searchAdd = "atomicCounterAdd(" + var + ", ";
            std::string searchVal = "atomicCounter(" + var + ")";
            while ((p = result.find(searchInc, p)) != std::string::npos) {
                result.replace(p, searchInc.size(), "atomicAdd(" + var + ", 1u)");
                p += 1;
            }
            p = 0;
            while ((p = result.find(searchDec, p)) != std::string::npos) {
                result.replace(p, searchDec.size(), "atomicAdd(" + var + ", uint(-1))");
                p += 1;
            }
            p = 0;
            while ((p = result.find(searchAdd, p)) != std::string::npos) {
                size_t argStart = p + searchAdd.size();
                size_t argEnd = result.find(')', argStart);
                if (argEnd != std::string::npos) {
                    std::string arg = result.substr(argStart, argEnd - argStart);
                    result.replace(p, argEnd - p + 1, "atomicAdd(" + var + ", " + arg + ")");
                    p += 1;
                } else {
                    p += 1;
                }
            }
            p = 0;
            while ((p = result.find(searchVal, p)) != std::string::npos) {
                result.replace(p, searchVal.size(), var);
                p += 1;
            }
        }
        source.swap(result);
        source += "\n" + std::string(atomicCounterEmulatedWatermark);
        return true;
    }

    source.swap(result);
    return false;
}

static void process_sampler_buffer(std::string& source) {
    if (source.find("isamplerBuffer") == std::string::npos) return;
    replace_all(source, "isamplerBuffer", "isampler2D");
    replace_all(source, "texelFetch(", "texelFetch(");
    size_t p = 0;
    while ((p = source.find("texelFetch(", p)) != std::string::npos) {
        size_t start = p + 11;
        size_t comma1 = source.find(',', start);
        if (comma1 != std::string::npos) {
            size_t comma2 = source.find(',', comma1 + 1);
            if (comma2 != std::string::npos) {
                std::string sampler = source.substr(start, comma1 - start);
                std::string index = source.substr(comma1 + 1, comma2 - comma1 - 1);
                std::string replacement = "texelFetch(" + sampler + ", ivec2((" + index + ") % u_BufferTexWidth, (" + index + ") / u_BufferTexWidth), 0)";
                source.replace(p, comma2 - p + 1, replacement);
                p += replacement.size();
            } else {
                p += 1;
            }
        } else {
            p += 1;
        }
    }

    const char* boundaryProtection = R"(
ivec2 bufferCoords(int index) {
    int width = u_BufferTexWidth;
    int x = index % width;
    int y = index / width;
    if (y >= u_BufferTexHeight) {
        y = u_BufferTexHeight - 1;
        x = width - 1;
    }
    return ivec2(x, y);
}
)";
    size_t insertion_point = 0;
    size_t versionPos = source.find("#version");
    if (versionPos != std::string::npos) {
        size_t nextNewline = source.find('\n', versionPos);
        insertion_point = (nextNewline != std::string::npos) ? nextNewline + 1 : source.length();
    } else {
        insertion_point = source.find('\n') + 1;
    }
    source.insert(insertion_point, boundaryProtection);

    const char* uniformDecl = R"(
uniform int u_BufferTexWidth;
uniform int u_BufferTexHeight;
)";
    insertion_point = source.find('\n', insertion_point) + 1;
    source.insert(insertion_point, uniformDecl);
}

static void inject_textureQueryLod(std::string& glsl) {
    if (glsl.find("textureQueryLod") == std::string::npos) return;
    if (glsl.find("mg_textureQueryLod") != std::string::npos) return;
    const std::string textureQueryLodImpl = R"(
#define textureQueryLod mg_textureQueryLod
vec2 mg_textureQueryLod(sampler2D tex, vec2 uv) {
    vec4 _ = textureLod(tex, uv, 0.0);
    vec2 tSize = vec2(textureSize(tex, 0));
    vec2 dx = dFdx(uv * tSize);
    vec2 dy = dFdy(uv * tSize);
    float dmax = max(length(dx), length(dy));
    float lod = log2(max(dmax, 1e-6));
    return vec2(lod);
}
vec2 mg_textureQueryLod(sampler2DShadow tex, vec2 uv) {
    float _ = textureLod(tex, vec3(uv, 0.0), 0.0);
    vec2 tSize = vec2(textureSize(tex, 0));
    vec2 dx = dFdx(uv * tSize);
    vec2 dy = dFdy(uv * tSize);
    float dmax = max(length(dx), length(dy));
    float lod = log2(max(dmax, 1e-6));
    return vec2(lod);
}
vec2 mg_textureQueryLod(samplerCube tex, vec3 dir) {
    vec4 _ = textureLod(tex, dir, 0.0);
    vec3 dx = dFdx(dir);
    vec3 dy = dFdy(dir);
    float dmax = max(length(dx), length(dy)) * 512.0;
    float lod = log2(max(dmax, 1e-6));
    return vec2(lod);
}
)";
    size_t insertPos = 0;
    size_t versionPos = glsl.find("#version");
    if (versionPos != std::string::npos) {
        size_t nextNewline = glsl.find('\n', versionPos);
        insertPos = (nextNewline != std::string::npos) ? nextNewline + 1 : glsl.length();
    } else {
        insertPos = glsl.find('\n') + 1;
    }
    glsl.insert(insertPos, "\n" + textureQueryLodImpl + "\n");
}

static void inject_temporal_filter(std::string& glsl) {
    if (glsl.find("GI_TemporalFilter") == std::string::npos) return;
    if (glsl.find("GI_TemporalFilter(") != std::string::npos) return;
    const std::string GI_TemporalFilterImpl = R"(
vec4 GI_TemporalFilter() {
    vec2 uv = gl_FragCoord.xy / screenSize;
    uv += taaJitter * pixelSize;
    vec4 currentGI = texture(colortex0, uv);
    float depth = texture(depthtex0, uv).r;
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = gbufferProjectionInverse * clipPos;
    viewPos /= viewPos.w;
    vec4 worldPos = gbufferModelViewInverse * viewPos;
    vec4 prevClipPos = gbufferPreviousProjection * (gbufferPreviousModelView * worldPos);
    prevClipPos /= prevClipPos.w;
    vec2 prevUV = prevClipPos.xy * 0.5 + 0.5;
    vec4 historyGI = texture(colortex1, prevUV);
    float difference = length(currentGI.rgb - historyGI.rgb);
    float thresholdValue = 0.1;
    float adaptiveBlend = mix(0.9, 0.0, smoothstep(thresholdValue, thresholdValue * 2.0, difference));
    vec4 filteredGI = mix(currentGI, historyGI, adaptiveBlend);
    if (difference > thresholdValue * 2.0) {
        filteredGI = currentGI;
    }
    return filteredGI;
}
)";
    size_t insertPos = 0;
    size_t versionPos = glsl.find("#version");
    if (versionPos != std::string::npos) {
        size_t nextNewline = glsl.find('\n', versionPos);
        insertPos = (nextNewline != std::string::npos) ? nextNewline + 1 : glsl.length();
    } else {
        insertPos = glsl.find('\n') + 1;
    }
    glsl.insert(insertPos, "\n" + GI_TemporalFilterImpl + "\n");
}

#define xstr(s) str(s)
#define str(s) #s

static void inject_mg_macro_definition(std::string& glslCode) {
    std::string macro_definitions =
            "\n#define MG_MOBILEGLUES\n"
            "#define MG_MOBILEGLUES_VERSION " xstr(MAJOR) xstr(MINOR) xstr(REVISION) xstr(PATCH) "\n";
    size_t versionPos = glslCode.rfind("#version");
    size_t insertionPos = 0;
    if (versionPos != std::string::npos) {
        size_t nextNewline = glslCode.find('\n', versionPos);
        insertionPos = (nextNewline != std::string::npos) ? nextNewline + 1 : glslCode.length();
    } else {
        size_t firstNewline = glslCode.find('\n');
        insertionPos = (firstNewline != std::string::npos) ? firstNewline + 1 : 0;
    }
    glslCode.insert(insertionPos, macro_definitions);
}

static std::string preprocess_glsl(const std::string& glsl, GLenum shaderType, bool* atomicCounterEmulated) {
    std::string ret = glsl;
    size_t p = 0;
    while ((p = ret.find("#line", p)) != std::string::npos) {
        size_t end = ret.find('\n', p);
        if (end != std::string::npos) {
            ret.erase(p, end - p + 1);
        } else {
            ret.erase(p);
            break;
        }
        p = 0;
    }
    replace_all(ret, "#ifdef GL_ARB_derivative_control", "#if 0");
    replace_all(ret, "#ifndef GL_ARB_derivative_control", "#if 1");
    replace_all(ret, "#ifdef VULKAN", "#if 0");
    replace_all(ret, "#ifndef VULKAN", "#if 1");
    replace_all(ret,
                "const mat3 rotInverse = transpose(rot);",
                "const mat3 rotInverse = mat3(rot[0][0], rot[1][0], rot[2][0], rot[0][1], rot[1][1], rot[2][1], rot[0][2], rot[1][2], rot[2][2]);");
    inject_temporal_filter(ret);
    if (!g_gles_caps.GL_EXT_texture_query_lod) inject_textureQueryLod(ret);
    inject_mg_macro_definition(ret);
    if (hardware->emulate_texture_buffer) process_sampler_buffer(ret);
    *atomicCounterEmulated = process_non_opaque_atomic_to_ssbo(ret);
    ret = removeLayoutBinding(ret);
    ret = processOutColorLocations(ret);
    return ret;
}

static std::string forceSupporterOutput(const std::string& glslCode) {
    bool hasPrecisionFloat = glslCode.find("precision ") != std::string::npos &&
                             glslCode.find("float;") != std::string::npos;
    bool hasPrecisionInt = glslCode.find("precision ") != std::string::npos &&
                           glslCode.find("int;") != std::string::npos;
    std::string result = glslCode;
    std::string precisionFloat, precisionInt;
    if (hasPrecisionFloat && hasPrecisionInt) {
        std::istringstream iss(result);
        std::vector<std::string> lines;
        std::string line;
        lines.reserve(128);
        while (std::getline(iss, line)) {
            bool isPrecisionLine = (line.find("precision ") != std::string::npos) &&
                                   (line.find("float;") != std::string::npos || line.find("int;") != std::string::npos);
            if (!isPrecisionLine) lines.push_back(line);
        }
        result.clear();
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i != 0) result += '\n';
            result += lines[i];
        }
        precisionFloat = "precision highp float;\n";
        precisionInt = "precision highp int;\n";
    } else {
        precisionFloat = hasPrecisionFloat ? "" : "precision highp float;\n";
        precisionInt = hasPrecisionInt ? "" : "precision highp int;\n";
    }
    size_t lastExtensionPos = result.rfind("#extension");
    size_t insertionPos = 0;
    if (lastExtensionPos != std::string::npos) {
        size_t nextNewline = result.find('\n', lastExtensionPos);
        insertionPos = (nextNewline != std::string::npos) ? nextNewline + 1 : result.length();
    } else {
        size_t firstNewline = result.find('\n');
        if (firstNewline != std::string::npos) {
            insertionPos = firstNewline + 1;
        } else {
            result = precisionFloat + precisionInt + result;
            return result;
        }
    }
    result.insert(insertionPos, precisionFloat + precisionInt);
    return result;
}

static std::vector<unsigned int> glsl_to_spirv(GLenum shader_type, int glsl_version, const char* const* shader_src,
                                               int& errc) {
    EShLanguage shader_language;
    switch (shader_type) {
        case GL_VERTEX_SHADER: shader_language = EShLangVertex; break;
        case GL_FRAGMENT_SHADER: shader_language = EShLangFragment; break;
        case GL_COMPUTE_SHADER: shader_language = EShLangCompute; break;
        case GL_TESS_CONTROL_SHADER: shader_language = EShLangTessControl; break;
        case GL_TESS_EVALUATION_SHADER: shader_language = EShLangTessEvaluation; break;
        case GL_GEOMETRY_SHADER: shader_language = EShLangGeometry; break;
        default: LOG_D("GLSL type not supported!"); errc = -1; return {};
    }
    using namespace glslang;
    glslang::TShader shader(shader_language);
    shader.setStrings(shader_src, 1);
    shader.setEnvInput(EShSourceGlsl, shader_language, EShClientVulkan, glsl_version);
    shader.setEnvClient(EShClientOpenGL, EShTargetOpenGL_450);
    shader.setEnvTarget(EShTargetSpv, EShTargetSpv_1_5);
    shader.setAutoMapLocations(true);
    shader.setAutoMapBindings(true);
    TBuiltInResource TBuiltInResource_resources = InitResources();
    if (!shader.parse(&TBuiltInResource_resources, glsl_version, true, EShMsgDefault)) {
        LOG_D("GLSL Compiling ERROR: \n%s", shader.getInfoLog());
        errc = -1; return {};
    }
    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(EShMsgDefault)) {
        LOG_D("Shader Linking ERROR: %s", program.getInfoLog());
        errc = -1; return {};
    }
    std::vector<unsigned int> spirv_code;
    glslang::SpvOptions spvOptions;
    spvOptions.disableOptimizer = false;
    glslang::GlslangToSpv(*program.getIntermediate(shader_language), spirv_code, &spvOptions);
    errc = 0;
    return spirv_code;
}

static std::string spirv_to_essl(std::vector<unsigned int> spirv, uint essl_version, int& errc) {
    spvc_context context = nullptr;
    spvc_parsed_ir ir = nullptr;
    spvc_compiler compiler_glsl = nullptr;
    spvc_compiler_options options = nullptr;
    const char* result = nullptr;
    spvc_context_create(&context);
    spvc_context_parse_spirv(context, spirv.data(), spirv.size(), &ir);
    spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler_glsl);
    spvc_compiler_create_compiler_options(compiler_glsl, &options);
    spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION,
                                   essl_version >= 300 ? essl_version : 300);
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_TRUE);
    spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_VULKAN_SEMANTICS, SPVC_FALSE);
    spvc_compiler_install_compiler_options(compiler_glsl, options);
    spvc_compiler_compile(compiler_glsl, &result);
    if (!result) {
        LOG_E("Error: unexpected error in spirv-cross.");
        errc = -1;
        spvc_context_destroy(context);
        return "";
    }
    std::string essl(result);
    spvc_context_destroy(context);
    errc = 0;
    return essl;
}

static bool glslang_inited = false;

std::string GLSLtoGLSLES_2(const char* glsl_code, GLenum glsl_type, uint essl_version, int& return_code) {
    bool atomicCounterEmulated = false;
    std::string correct_glsl_str = preprocess_glsl(glsl_code, glsl_type, &atomicCounterEmulated);
    int glsl_version = getGLSLVersion(correct_glsl_str.c_str());
    if (glsl_version == -1) {
        glsl_version = 150;
        correct_glsl_str.insert(0, "#version 150\n");
    } else if (glsl_version < 140) {
        replace_all(correct_glsl_str, "#version", "#version 150 compatibility");
        glsl_version = 150;
    }
    if (!glslang_inited) {
        glslang::InitializeProcess();
        glslang_inited = true;
    }
    const char* s[] = {correct_glsl_str.c_str()};
    int errc = 0;
    std::vector<unsigned int> spirv_code = glsl_to_spirv(glsl_type, glsl_version, s, errc);
    if (errc != 0) {
        return_code = -1;
        return "";
    }
    errc = 0;
    std::string essl = spirv_to_essl(spirv_code, essl_version, errc);
    if (errc != 0) {
        return_code = -2;
        return "";
    }
    if (glsl_type != GL_COMPUTE_SHADER) {
        essl = removeLayoutBinding(essl);
        essl = processOutColorLocations(essl);
    } else {
        essl = removeLayoutBinding(essl);
    }
    essl = forceSupporterOutput(essl);
    return_code = atomicCounterEmulated ? 1 : 0;
    return essl;
}

std::string GLSLtoGLSLES_1(const char* glsl_code, GLenum glsl_type, uint esversion, int& return_code) {
    return_code = 0;
    return "";
}

std::string GLSLtoGLSLES(const char* glsl_code, GLenum glsl_type, uint essl_version, uint glsl_version,
                         int& return_code) {
    std::string sha256_string(glsl_code);
    sha256_string += "\n//" + std::to_string(MAJOR) + "." + std::to_string(MINOR) + "." + std::to_string(REVISION) +
                     "|" + std::to_string(essl_version);
    const char* cachedESSL = Cache::get_instance().get(sha256_string.c_str());
    if (cachedESSL) {
        LOG_D("GLSL Hit Cache:\n%s\n-->\n%s", glsl_code, cachedESSL);
        bool atomicCounterEmulated = (std::string(cachedESSL).find(atomicCounterEmulatedWatermark) != std::string::npos);
        return_code = atomicCounterEmulated ? 1 : 0;
        return std::string(cachedESSL);
    }
    return_code = -1;
    std::string converted = GLSLtoGLSLES_2(glsl_code, glsl_type, essl_version, return_code);
    if (return_code >= 0 && !converted.empty()) {
        Cache::get_instance().put(sha256_string.c_str(), converted.c_str());
    }
    return (return_code >= 0) ? converted : glsl_code;
}