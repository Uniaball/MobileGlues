#!/bin/sh
# Host-side checks for the parts of the pixel, framebuffer and shader-translation
# code that are pure enough to run without a GPU. All link the real translation
# units, not copies.
#
#   sh MobileGlues-cpp/tests/run.sh
set -e
cd "$(dirname "$0")/.."
INC="-I. -I./includes -I./include -I./3rdparty/xxhash"
CXX="${CXX:-g++} -std=gnu++20 -w $INC"
# log.cpp cannot be linked here: the tests stub the android logger themselves,
# and global_settings would drag the whole config world in. The two symbols
# pixel.cpp/framebuffer.cpp newly reach for are stubbed in the test files.
$CXX -o /tmp/mg_pixel_test  tests/pixel_size_test.cpp         gl/pixel.cpp
$CXX -o /tmp/mg_fb_test     tests/framebuffer_shuffle_test.cpp gl/framebuffer.cpp
$CXX -o /tmp/mg_uniform_test tests/uniform_declarations_test.cpp gl/glsl/uniform_defaults.cpp
/tmp/mg_pixel_test
echo
/tmp/mg_fb_test
echo
/tmp/mg_uniform_test