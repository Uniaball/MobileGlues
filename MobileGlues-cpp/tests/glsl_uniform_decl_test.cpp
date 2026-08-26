// Regression test for process_uniform_declarations in gl/glsl/glsl_for_es.cpp.
//
// The scanner matches the literal word "uniform", so Minecraft 26.x's mangled
// block-instance names (`_uniform_instance_00_00`) used to fire it mid
// identifier; with an '=' before the next ';', the initializer-stripping
// rewrite then replaced the surrounding statement with garbage and every
// terrain/box_blur variant carrying an RGSS or OIT condition failed to compile
// on the driver. The function is pulled straight out of the real translation
// unit at build time (see run.sh), never copied here.
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

using namespace std;

#include "mg_glsl_extracted.inc"

static int failures = 0;

static void expect(const char* what, const std::string& got, const std::string& want) {
    if (got == want) {
        printf("PASS %s\n", what);
        return;
    }
    ++failures;
    printf("FAIL %s\n  got:  %s\n  want: %s\n", what, got.c_str(), want.c_str());
}

int main(int argc, char** argv) {
    // The corruption seen on device, byte for byte: mangled instance name in
    // an if-condition whose branch body assigns.
    {
        const std::string src =
            "layout( std140) uniform _uniform_00_01\n"
            "{\n"
            "    highp int UseRgss;\n"
            "} _uniform_instance_00_01;\n"
            "void main() {\n"
            "    highp vec4 c = vec4(0.0);\n"
            "    if (_uniform_instance_00_01.UseRgss != int(0.0)) {\n"
            "        highp vec2 param_1 = vec2(1.0) / vec2(_uniform_instance_00_02.TextureSize);\n"
            "        c = sampleRGSS(_uniform_00_05, param_1);\n"
            "    }\n"
            "}\n";
        expect("mangled identifier left untouched", process_uniform_declarations(src), src);
    }
    // Same shape without the leading underscore: the boundary check cannot
    // reject it, so the rewrite precondition must.
    {
        const std::string src = "if (uniform_instance_00.UseRgss != 0) { float q = 1.0; }\n";
        expect("keyword-prefixed identifier left untouched", process_uniform_declarations(src), src);
    }
    // Prose in a comment: parsed name comes out empty, no rewrite.
    {
        const std::string src = "// set uniform foo = 7;\nvoid main(){}\n";
        expect("comment prose left untouched", process_uniform_declarations(src), src);
    }
    // Genuine initialised uniforms keep their rewrite semantics.
    expect("initializer stripped", process_uniform_declarations("uniform highp float x = 1.0;\n"),
           "uniform highp float x;\n");
    expect("mediump initializer stripped", process_uniform_declarations("uniform mediump int z = 3;"),
           "uniform mediump int z;");
    expect("plain declaration unchanged", process_uniform_declarations("uniform float y;\n"), "uniform float y;\n");

    // Optional deep sweep: hand it a converted-shader cache dump and every
    // segment in it must survive the pass byte for byte.
    if (argc > 1) {
        FILE* f = fopen(argv[1], "rb");
        if (!f) {
            printf("FAIL sweep (cannot open %s)\n", argv[1]);
            ++failures;
        } else {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            std::string data(size, '\0');
            if (fread(&data[0], 1, size, f) != (size_t)size || ferror(f)) {
                printf("FAIL sweep (short read)\n");
                ++failures;
            } else {
                fclose(f);
                size_t count = 0, mismatches = 0;
                size_t pos = data.find("#version");
                while (pos != std::string::npos) {
                    size_t next = data.find("#version", pos + 1);
                    std::string seg = data.substr(pos, (next == std::string::npos ? data.size() : next) - pos);
                    if (process_uniform_declarations(seg) != seg) ++mismatches;
                    ++count;
                    pos = next;
                }
                printf("%s sweep: %zu segments, %zu mismatches\n", mismatches ? "FAIL" : "PASS", count, mismatches);
                failures += static_cast<int>(mismatches);
            }
        }
    }

    printf(failures ? "== %d FAILURE(S) ==\n" : "== ALL GREEN ==\n", failures);
    return failures ? 1 : 0;
}
