//
// Created by hanji on 2025/2/6.
//

#ifndef MOBILEGLUES_FRAMEBUFFER_H
#define MOBILEGLUES_FRAMEBUFFER_H

#include <GL/gl.h>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

GLint getMaxDrawBuffers();

GLAPI GLAPIENTRY void glBindFramebuffer(GLenum target, GLuint framebuffer);
GLAPI GLAPIENTRY void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
GLAPI GLAPIENTRY void glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level);
GLAPI GLAPIENTRY void glDrawBuffer(GLenum buf);
GLAPI GLAPIENTRY void glDrawBuffers(GLsizei n, const GLenum *bufs);
GLAPI GLAPIENTRY void glReadBuffer(GLenum src);
GLAPI GLAPIENTRY GLenum glCheckFramebufferStatus(GLenum target);

#ifdef __cplusplus
}
#endif

void InitFramebufferMap(size_t expectedSize);

#endif //MOBILEGLUES_FRAMEBUFFER_H
