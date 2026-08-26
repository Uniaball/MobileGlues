#!/bin/sh
# Host-side checks for the parts of the pixel, framebuffer and shader-string
# code that are pure enough to run without a GPU. Both link the real translation
# units, not copies.
#
#   sh MobileGlues-cpp/tests/run.sh
#
# glsl_for_es.cpp drags in glslang and EGL headers, too much for a host build,
# so the uniform-declaration test compiles against the functions extracted from
# the real file instead. The extraction lives here so it cannot drift from the
# source it tests.
set -e
cd "$(dirname "$0")/.."
INC="-I. -I./includes -I./include -I./3rdparty/xxhash"
CXX="${CXX:-g++} -std=gnu++20 -w $INC"
# log.cpp cannot be linked here: the tests stub the android logger themselves,
# and global_settings would drag the whole config world in. The two symbols
# pixel.cpp/framebuffer.cpp newly reach for are stubbed in the test files.
$CXX -o /tmp/mg_pixel_test  tests/pixel_size_test.cpp         gl/pixel.cpp
$CXX -o /tmp/mg_fb_test     tests/framebuffer_shuffle_test.cpp gl/framebuffer.cpp
python3 -c "
src = open('gl/glsl/glsl_for_es.cpp').read()
begin = src.index('bool is_glsl_identifier')
end = src.index('\nstd::string processOutColorLocations')
open('/tmp/mg_glsl_extracted.inc', 'w').write(src[begin:end])
"
$CXX -o /tmp/mg_uniform_decl_test -I/tmp tests/glsl_uniform_decl_test.cpp
/tmp/mg_pixel_test
echo
/tmp/mg_fb_test
echo
/tmp/mg_uniform_decl_test "$@"
