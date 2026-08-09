// MobileGlues - gl/ExtWrappers/MultiBindWrapper.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "MultiBindWrapper.h"
#include <cassert>
#include "../texture.h"

#define DEBUG 0

void glBindTextures(GLuint first, GLsizei count, const GLuint* textures) {
    GLuint prevUnit;
    glGetIntegerv(GL_ACTIVE_TEXTURE, reinterpret_cast<GLint*>(&prevUnit));
    for (GLsizei i = 0; i < count; ++i) {
        // ARB_multi_bind allows 'textures' itself to be NULL, and allows any entry to
        // be zero; both mean "unbind these units". The spec unbinds every target on the
        // unit; only GL_TEXTURE_2D is dropped here, because a zero name carries no
        // target and this wrapper has no record of which others the unit held. A unit
        // left holding a non-2D binding is a known deviation -- but it is the one the
        // previous code had too, on top of faulting on the name lookup.
        GLuint tex = textures ? textures[i] : 0u;
        if (tex == 0) {
            glActiveTexture(GL_TEXTURE0 + first + i);
            glBindTexture(GL_TEXTURE_2D, 0);
            continue;
        }
        // The object table only learns a name once glBindTexture has seen it, so a name
        // straight out of glGenTextures, or one already deleted, looks up as null here.
        // Reading its target used to fault; leave the unit as it is instead.
        TextureObject* texObject = mgGetTexObjectByID(tex);
        if (texObject == nullptr) {
            continue;
        }
        glActiveTexture(GL_TEXTURE0 + first + i);
        glBindTexture(ConvertTextureTargetToGLEnum(texObject->target), tex);
    }
    glActiveTexture(prevUnit);
}

void glBindSamplers(GLuint first, GLsizei count, const GLuint* samplers) {
    for (GLsizei i = 0; i < count; ++i) {
        glBindSampler(first + i, samplers[i]);
    }
}

void glBindImageTextures(GLuint first, GLsizei count, const GLuint* textures) {
    for (int i = 0; i < count; i++) {
        // A name the layer has never bound has no entry in the object table, so the lookup
        // comes back null and reading its internal format straight out of the argument list
        // faulted. Such a unit gets the same zero binding an absent name gets.
        TextureObject* texObject =
            (textures != nullptr && textures[i] != 0) ? mgGetTexObjectByID(textures[i]) : nullptr;
        if (texObject == nullptr) {
            glBindImageTexture(first + i, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
        } else {
            glBindImageTexture(first + i, textures[i], 0, GL_TRUE, 0, GL_READ_WRITE, texObject->internal_format);
        }
    }
}

void glBindVertexBuffers(GLuint first, GLsizei count, const GLuint* buffers, const GLintptr* offsets,
                         const GLsizei* strides) {
    for (GLsizei i = 0; i < count; ++i) {
        glBindVertexBuffer(first + i, buffers[i], offsets[i], strides[i]);
    }
}
