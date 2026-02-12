// MobileGlues - gl/multidraw.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#ifndef MOBILEGLUES_MULTIDRAW_H
#define MOBILEGLUES_MULTIDRAW_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <GLES3/gl32.h>
#include "../includes.h"
#include <GL/gl.h>
#include "glcorearb.h"
#include "log.h"
#include "../gles/loader.h"
#include "mg.h"

#ifdef __cplusplus
extern "C"
{
#endif

    struct draw_elements_indirect_command_t {
        GLuint count;
        GLuint instanceCount;
        GLuint firstIndex;
        GLint baseVertex;
        GLuint reservedMustBeZero;
    };

    struct drawcmd_compute_t {
        GLuint firstIndex;
        GLint baseVertex;
    };

    GLAPI GLAPIENTRY void glMultiDrawElementsBaseVertex(GLenum mode, GLsizei* counts, GLenum type,
                                                        const void* const* indices, GLsizei primcount,
                                                        const GLint* basevertex);
    GLAPI GLAPIENTRY void mg_glMultiDrawElementsBaseVertex_indirect(GLenum mode, GLsizei* counts, GLenum type,
                                                                    const void* const* indices, GLsizei primcount,
                                                                    const GLint* basevertex);
    GLAPI GLAPIENTRY void mg_glMultiDrawElementsBaseVertex_multiindirect(GLenum mode, GLsizei* counts, GLenum type,
                                                                         const void* const* indices, GLsizei primcount,
                                                                         const GLint* basevertex);
    GLAPI GLAPIENTRY void mg_glMultiDrawElementsBaseVertex_basevertex(GLenum mode, GLsizei* counts, GLenum type,
                                                                      const void* const* indices, GLsizei primcount,
                                                                      const GLint* basevertex);
    GLAPI GLAPIENTRY void mg_glMultiDrawElementsBaseVertex_drawelements(GLenum mode, GLsizei* counts, GLenum type,
                                                                        const void* const* indices, GLsizei primcount,
                                                                        const GLint* basevertex);
    GLAPI GLAPIENTRY void mg_glMultiDrawElementsBaseVertex_compute(GLenum mode, GLsizei* counts, GLenum type,
                                                                   const void* const* indices, GLsizei primcount,
                                                                   const GLint* basevertex);

    GLAPI GLAPIENTRY void glMultiDrawElements(GLenum mode, const GLsizei* count, GLenum type,
                                              const void* const* indices, GLsizei primcount);
    GLAPI GLAPIENTRY void mg_glMultiDrawElements_indirect(GLenum mode, const GLsizei* count, GLenum type,
                                                          const void* const* indices, GLsizei primcount);
    GLAPI GLAPIENTRY void mg_glMultiDrawElements_multiindirect(GLenum mode, const GLsizei* count, GLenum type,
                                                               const void* const* indices, GLsizei primcount);
    GLAPI GLAPIENTRY void mg_glMultiDrawElements_basevertex(GLenum mode, const GLsizei* count, GLenum type,
                                                            const void* const* indices, GLsizei primcount);
    GLAPI GLAPIENTRY void mg_glMultiDrawElements_drawelements(GLenum mode, const GLsizei* count, GLenum type,
                                                              const void* const* indices, GLsizei primcount);
    GLAPI GLAPIENTRY void mg_glMultiDrawElements_compute(GLenum mode, const GLsizei* count, GLenum type,
                                                         const void* const* indices, GLsizei primcount);

#ifdef __cplusplus
}
#endif

#endif // MOBILEGLUES_MULTIDRAW_H
