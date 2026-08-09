// MobileGlues - gl/pixel.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "pixel.h"
#include "log.h"
#include "mg.h"

#define DEBUG 0

// Three tables that have to agree, and used to be shuffled together.
//
// gl_sizeof is keyed on the pixel-transfer TYPE and answers bytes -- per
// component for an unpacked type, per whole pixel for a packed one.
// pixel_sizeof is keyed on the FORMAT and answers components per pixel.
// is_type_packed says which of the two readings applies.
//
// Format enums had leaked into the type table (GL_LUMINANCE_ALPHA, GL_ALPHA,
// GL_DEPTH_COMPONENT, GL_COLOR_INDEX), sized internalformats and a texgen
// coordinate had leaked into the format table (GL_RGB8, GL_RGBA8, GL_R32F,
// GL_R11F_G11F_B10F, GL_R), and a format sat in the packed-type list
// (GL_DEPTH_STENCIL) while the actual packed type it stands for
// (GL_UNSIGNED_INT_24_8) was missing from it. Most of that was unreachable
// through the one caller, but GL_R32F with GL_FLOAT really did answer 16 bytes
// for a 4-byte texel, and every entry that was merely latent was a trap for the
// next caller. Each table now holds only its own kind of enum.

GLsizei gl_sizeof(GLenum type) {
    switch (type) {
    case GL_DOUBLE:
    // One 64-bit unit per pixel: 32-bit float depth plus a padded stencil byte.
    case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
        return 8;
    case GL_FLOAT:
    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_UNSIGNED_INT_10_10_10_2:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_UNSIGNED_INT_8_8_8_8:
    case GL_UNSIGNED_INT_8_8_8_8_REV:
    case GL_UNSIGNED_INT_24_8:
    // GL 3.0 packed float formats. Their absence made pixel_sizeof answer 0 for
    // any R11F_G11F_B10F or RGB9_E5 transfer, which the DSA readback path reads
    // as "unsupported" and drops.
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
    case GL_UNSIGNED_INT_5_9_9_9_REV:
    case GL_4_BYTES:
        return 4;
    case GL_3_BYTES:
        return 3;
    case GL_SHORT:
    case GL_HALF_FLOAT:
    case GL_UNSIGNED_SHORT:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_4_4_4_4_REV:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_5_6_5_REV:
    case GL_2_BYTES:
        return 2;
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_BYTE_2_3_3_REV:
    case GL_UNSIGNED_BYTE_3_3_2:
        return 1;
    default:
        LOG_D("Unsupported pixel data type: %s\n", glEnumToString(type))
        return 0;
    }
}

GLboolean is_type_packed(GLenum type) {
    switch (type) {
    case GL_4_BYTES:
    case GL_3_BYTES:
    case GL_2_BYTES:
    case GL_UNSIGNED_BYTE_2_3_3_REV:
    case GL_UNSIGNED_BYTE_3_3_2:
    case GL_UNSIGNED_INT_10_10_10_2:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_UNSIGNED_INT_8_8_8_8:
    case GL_UNSIGNED_INT_8_8_8_8_REV:
    // The packed depth-stencil TYPE. GL_DEPTH_STENCIL, the FORMAT, used to stand
    // here in its place; the result stayed right only because that format is
    // independently one component wide, so the collapse below never had to fire.
    case GL_UNSIGNED_INT_24_8:
    case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
    case GL_UNSIGNED_INT_5_9_9_9_REV:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_4_4_4_4_REV:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_5_6_5_REV:
        return true;
    default:
        return false;
    }
}

GLsizei pixel_sizeof(GLenum format, GLenum type) {
    GLsizei width = 0;
    switch (format) {
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_STENCIL:
    case GL_STENCIL_INDEX:
    case GL_COLOR_INDEX:
    // The integer client formats. Every one of them was missing, so reading back
    // any integer texture -- an id or picking buffer, a voxel lookup -- sized to
    // zero and was dropped without an error.
    case GL_RED_INTEGER:
    case GL_GREEN_INTEGER:
    case GL_BLUE_INTEGER:
    case GL_ALPHA_INTEGER:
        width = 1;
        break;
    case GL_RG:
    case GL_LUMINANCE_ALPHA:
    case GL_RG_INTEGER:
        width = 2;
        break;
    case GL_RGB:
    case GL_BGR:
    case GL_RGB_INTEGER:
    case GL_BGR_INTEGER:
        width = 3;
        break;
    case GL_RGBA:
    case GL_BGRA:
    case GL_RGBA_INTEGER:
    case GL_BGRA_INTEGER:
        width = 4;
        break;
    default:
        LOG_D("unsupported pixel format %s\n", glEnumToString(format))
        return 0;
    }

    // A packed type carries the whole pixel in one unit, so the component count
    // stops applying.
    if (is_type_packed(type)) width = 1;

    return width * gl_sizeof(type);
}

// ---------------------------------------------------------------------------
// The pixel-store parameters GLES does not have
// ---------------------------------------------------------------------------

namespace {
// Where each of the six lives in the per-context record, or nullptr for a pname
// that is not one of them.
GLint* desktop_pixel_store_slot(GLenum pname) {
    if (gl_state == nullptr) return nullptr;
    switch (pname) {
    case GL_UNPACK_SWAP_BYTES:
        return &gl_state->unpack_swap_bytes;
    case GL_UNPACK_LSB_FIRST:
        return &gl_state->unpack_lsb_first;
    case GL_PACK_SWAP_BYTES:
        return &gl_state->pack_swap_bytes;
    case GL_PACK_LSB_FIRST:
        return &gl_state->pack_lsb_first;
    case GL_PACK_IMAGE_HEIGHT:
        return &gl_state->pack_image_height;
    case GL_PACK_SKIP_IMAGES:
        return &gl_state->pack_skip_images;
    default:
        return nullptr;
    }
}
} // namespace

bool mg_pixel_store_set(GLenum pname, GLint param) {
    GLint* slot = desktop_pixel_store_slot(pname);
    if (slot == nullptr) return false;
    // The booleans store as 0 or 1, the two counts as themselves. GL rejects a
    // negative count; the driver would have said so, and nothing here can.
    switch (pname) {
    case GL_PACK_IMAGE_HEIGHT:
    case GL_PACK_SKIP_IMAGES:
        if (param < 0) {
            mg_set_gl_error(GL_INVALID_VALUE);
            return true;
        }
        *slot = param;
        break;
    default:
        *slot = param != 0 ? 1 : 0;
        break;
    }
    return true;
}

bool mg_pixel_store_query_int(GLenum pname, GLint* out) {
    const GLint* slot = desktop_pixel_store_slot(pname);
    if (slot == nullptr) return false;
    if (out != nullptr) *out = *slot;
    return true;
}

bool mg_unpack_swaps_bytes(GLenum type) {
    if (gl_state == nullptr || !gl_state->unpack_swap_bytes) return false;
    return gl_sizeof(type) > 1;
}

bool mg_pack_swaps_bytes(GLenum type) {
    if (gl_state == nullptr || !gl_state->pack_swap_bytes) return false;
    return gl_sizeof(type) > 1;
}
