// MobileGlues - gl/pixel.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#ifndef MOBILEGLUES_PIXEL_H
#define MOBILEGLUES_PIXEL_H

#include <GL/gl.h>
#include "../gles/gles.h"
#include "log.h"

// Round a row length up to a pixel-store alignment boundary.
//
// The mask form below is only meaningful for a power-of-two alignment: with
// align 0 it produces 0 for every width (align-1 wraps to all-ones and the mask
// becomes zero), and with a non-power-of-two it produces something that is not a
// multiple of align at all. GL only ever hands us 1, 2, 4 or 8, but this is fed
// from GL_UNPACK_ALIGNMENT / GL_PACK_ALIGNMENT read back from the driver, and a
// stride of 0 walks rows forever, so the degenerate cases return the width
// untouched rather than a number nothing can use.
static inline uintptr_t mg_width_align(uintptr_t width, uintptr_t align) {
    if (align <= 1) return width;
    if ((align & (align - 1)) != 0) return width; // not a power of two: no sane rounding
    return (width + (align - 1)) & ~(align - 1);
}
#define widthalign(width, align) mg_width_align((uintptr_t)(width), (uintptr_t)(align))

GLsizei gl_sizeof(GLenum type);

GLsizei pixel_sizeof(GLenum format, GLenum type);

GLboolean is_type_packed(GLenum type);

// The six pixel-store parameters GLES does not have. Both directions go through
// here so a value that was set can be read back; see gl_state_s in gl/mg.h.
//
// Each returns true when pname is one of the six, which is the caller's signal to
// stop -- forwarding any of them to the driver only earns a GL_INVALID_ENUM.
bool mg_pixel_store_set(GLenum pname, GLint param);
bool mg_pixel_store_query_int(GLenum pname, GLint* out);

// Whether a transfer has to reverse the byte order of each component. Answered
// per direction, and only ever true for a component wider than one byte -- the
// parameter has no meaning for a byte, and GL says so.
bool mg_unpack_swaps_bytes(GLenum type);
bool mg_pack_swaps_bytes(GLenum type);


#endif // MOBILEGLUES_PIXEL_H
