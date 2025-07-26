//
// Created by BZLZHH on 2025/7/26.
//

#include "DSAWrapper.h"
#include <cassert>
#include "..\texture.h"

#define DEBUG 0

GLenum GetBindingQuery(GLenum target, bool forceTexture = false) {
	switch (target) {
	case GL_TEXTURE_BUFFER:                return forceTexture ? GL_TEXTURE_BINDING_BUFFER : GL_TEXTURE_BUFFER_BINDING;

	case GL_ARRAY_BUFFER:                  return GL_ARRAY_BUFFER_BINDING;
	case GL_ATOMIC_COUNTER_BUFFER:         return GL_ATOMIC_COUNTER_BUFFER_BINDING;
	case GL_COPY_READ_BUFFER:              return GL_COPY_READ_BUFFER_BINDING;
	case GL_COPY_WRITE_BUFFER:             return GL_COPY_WRITE_BUFFER_BINDING;
	case GL_DISPATCH_INDIRECT_BUFFER:      return GL_DISPATCH_INDIRECT_BUFFER_BINDING;
	case GL_DRAW_INDIRECT_BUFFER:          return GL_DRAW_INDIRECT_BUFFER_BINDING;
	case GL_ELEMENT_ARRAY_BUFFER:          return GL_ELEMENT_ARRAY_BUFFER_BINDING;
	case GL_PIXEL_PACK_BUFFER:             return GL_PIXEL_PACK_BUFFER_BINDING;
	case GL_PIXEL_UNPACK_BUFFER:           return GL_PIXEL_UNPACK_BUFFER_BINDING;
	case GL_QUERY_BUFFER:                  return GL_QUERY_BUFFER_BINDING;
	case GL_SHADER_STORAGE_BUFFER:         return GL_SHADER_STORAGE_BUFFER_BINDING;
	case GL_TRANSFORM_FEEDBACK_BUFFER:     return GL_TRANSFORM_FEEDBACK_BUFFER_BINDING;
	case GL_UNIFORM_BUFFER:                return GL_UNIFORM_BUFFER_BINDING;

	case GL_FRAMEBUFFER:                   return GL_FRAMEBUFFER_BINDING;
	case GL_DRAW_FRAMEBUFFER:              return GL_DRAW_FRAMEBUFFER_BINDING;
	case GL_READ_FRAMEBUFFER:              return GL_READ_FRAMEBUFFER_BINDING;

	case GL_RENDERBUFFER:                  return GL_RENDERBUFFER_BINDING;

	case GL_VERTEX_ARRAY:                  return GL_VERTEX_ARRAY_BINDING;
	case GL_VERTEX_ARRAY_BINDING:

	case GL_PROGRAM_PIPELINE:              return GL_PROGRAM_PIPELINE_BINDING;

	case GL_PROGRAM:                       return GL_CURRENT_PROGRAM;

	case GL_SAMPLER:                       return GL_SAMPLER_BINDING;

	case GL_TEXTURE:                       return GL_TEXTURE_BINDING_2D;
	case GL_TEXTURE_1D:                    return GL_TEXTURE_BINDING_1D;
	case GL_TEXTURE_1D_ARRAY:              return GL_TEXTURE_BINDING_1D_ARRAY;
	case GL_TEXTURE_2D:                    return GL_TEXTURE_BINDING_2D;
	case GL_TEXTURE_2D_ARRAY:              return GL_TEXTURE_BINDING_2D_ARRAY;
	case GL_TEXTURE_2D_MULTISAMPLE:        return GL_TEXTURE_BINDING_2D_MULTISAMPLE;
	case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:  return GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
	case GL_TEXTURE_3D:                    return GL_TEXTURE_BINDING_3D;
	case GL_TEXTURE_CUBE_MAP:              return GL_TEXTURE_BINDING_CUBE_MAP;
	case GL_TEXTURE_CUBE_MAP_ARRAY:        return GL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
	case GL_TEXTURE_RECTANGLE:             return GL_TEXTURE_BINDING_RECTANGLE;

	case GL_TRANSFORM_FEEDBACK:            return GL_TRANSFORM_FEEDBACK_BINDING;

	case GL_SAMPLES_PASSED:                return GL_SAMPLES_PASSED;
	case GL_PRIMITIVES_GENERATED:          return GL_PRIMITIVES_GENERATED;

	case GL_DEBUG_OUTPUT:                  return GL_DEBUG_OUTPUT;
	case GL_DEBUG_OUTPUT_SYNCHRONOUS:      return GL_DEBUG_OUTPUT_SYNCHRONOUS;

	default:
		assert(false && "GetBindingQuery: unknown target");
		return 0;
	}
}

// buffer
static thread_local ankerl::unordered_dense::map<GLenum, std::vector<GLuint>> bufferBindingStack;
void temporarilyBindBuffer(GLuint bufferID, GLenum target = GL_ARRAY_BUFFER) {
	GLenum bindingQuery = GetBindingQuery(target);
	GLint prev = 0;
	glGetIntegerv(bindingQuery, &prev);

	bufferBindingStack[target].push_back(static_cast<GLuint>(prev));

	LOG_D("[TempBind] target=0x%X, prev=%u -> bind=%u", target, prev, bufferID);
	glBindBuffer(target, bufferID);
}
void restoreTemporaryBufferBinding(GLenum target = GL_ARRAY_BUFFER) {
	auto it = bufferBindingStack.find(target);
	if (it == bufferBindingStack.end() || it->second.empty()) {
		LOG_D("[Restore] no saved binding for target 0x%X", target);
		return;
	}

	GLuint toRestore = it->second.back();
	it->second.pop_back();

	LOG_D("[Restore] target=0x%X, bind back to %u", target, toRestore);
	glBindBuffer(target, toRestore);

	if (it->second.empty())
		bufferBindingStack.erase(it);
}

void glCreateBuffers(GLsizei n, GLuint* buffers) {
	LOG()
	LOG_D("glCreateBuffers, n: %d, buffers: %p", n, buffers);
	
	if (n <= 0 || !buffers) {
		LOG_E("Invalid parameters for glCreateBuffers");
		return;
	}

	for (GLsizei i = 0; i < n; ++i) {
		GLuint bufID = 0;
		glGenBuffers(1, &bufID);
		if (bufID == 0) {
			LOG_E("Failed to create buffer at index %d", i);
			continue;
		}

		temporarilyBindBuffer(bufID); // after binding, the buffer object should be created
		restoreTemporaryBufferBinding();
		buffers[i] = bufID;
	}
	
	LOG_D("Created %d buffers successfully", n);
}

void glNamedBufferStorage(GLuint buffer, GLsizeiptr size, const void* data, GLbitfield flags) {
	LOG()
	LOG_D("glNamedBufferStorage, buffer: %u, size: %lld, data: %p, flags: %u", buffer, size, data, flags);
	
	if (buffer == 0 || size <= 0) {
		LOG_E("Invalid parameters for glNamedBufferStorage");
		return;
	}

	temporarilyBindBuffer(buffer);
	glBufferStorage(GL_ARRAY_BUFFER, size, data, flags);
	restoreTemporaryBufferBinding();
	
	LOG_D("Buffer %u stored with size %lld", buffer, size);
}

void glNamedBufferData(GLuint buffer, GLsizeiptr size, const void* data, GLenum usage) {
	LOG()
	LOG_D("glNamedBufferData, buffer: %u, size: %lld, data: %p, usage: %u", buffer, size, data, usage);
	
	if (buffer == 0 || size <= 0) {
		LOG_E("Invalid parameters for glNamedBufferData");
		return;
	}
	
	temporarilyBindBuffer(buffer);
	glBufferData(GL_ARRAY_BUFFER, size, data, usage);
	restoreTemporaryBufferBinding();
	
	LOG_D("Buffer %u data set with size %lld", buffer, size);
}

void glNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, const void* data) {
	LOG()
	LOG_D("glNamedBufferSubData, buffer: %u, offset: %lld, size: %lld, data: %p", buffer, offset, size, data);
	
	if (buffer == 0 || size <= 0 || offset < 0) {
		LOG_E("Invalid parameters for glNamedBufferSubData");
		return;
	}
	temporarilyBindBuffer(buffer);
	glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
	restoreTemporaryBufferBinding();
	
	LOG_D("Buffer %u sub-data set with size %lld at offset %lld", buffer, size, offset);
}

void glCopyNamedBufferSubData(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
	LOG()
	LOG_D("glCopyNamedBufferSubData, readBuffer: %u, writeBuffer: %u, readOffset: %lld, writeOffset: %lld, size: %lld", readBuffer, writeBuffer, readOffset, writeOffset, size);
	
	if (readBuffer == 0 || writeBuffer == 0 || size <= 0 || readOffset < 0 || writeOffset < 0) {
		LOG_E("Invalid parameters for glCopyNamedBufferSubData");
		return;
	}
	temporarilyBindBuffer(readBuffer, GL_COPY_READ_BUFFER);
	temporarilyBindBuffer(writeBuffer, GL_COPY_WRITE_BUFFER);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, readOffset, writeOffset, size);
	restoreTemporaryBufferBinding(GL_COPY_READ_BUFFER);
	restoreTemporaryBufferBinding(GL_COPY_WRITE_BUFFER);
	LOG_D("Copied %lld bytes from buffer %u to buffer %u", size, readBuffer, writeBuffer);
}

void glClearNamedBufferData(GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void* data) {
	LOG()
	LOG_D("glClearNamedBufferData, buffer: %u, internalformat: 0x%X, format: 0x%X, type: 0x%X, data: %p", buffer, internalformat, format, type, data);
	
	if (buffer == 0) {
		LOG_E("Invalid buffer ID for glClearNamedBufferData");
		return;
	}
	temporarilyBindBuffer(buffer);
	glClearBufferData(GL_ARRAY_BUFFER, internalformat, format, type, data);
	restoreTemporaryBufferBinding();
	
	LOG_D("Cleared buffer %u with specified data", buffer);
}

void glClearNamedBufferSubData(GLuint buffer, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void* data) {
	LOG()
	LOG_D("glClearNamedBufferSubData, buffer: %u, internalformat: 0x%X, offset: %lld, size: %lld, format: 0x%X, type: 0x%X, data: %p", buffer, internalformat, offset, size, format, type, data);
	
	if (buffer == 0 || size <= 0 || offset < 0) {
		LOG_E("Invalid parameters for glClearNamedBufferSubData");
		return;
	}
	temporarilyBindBuffer(buffer);
	glClearBufferSubData(GL_ARRAY_BUFFER, internalformat, offset, size, format, type, data);
	restoreTemporaryBufferBinding();
	
	LOG_D("Cleared sub-data of buffer %u with size %lld at offset %lld", buffer, size, offset);
}

void* glMapNamedBuffer(GLuint buffer, GLenum access) {
	LOG()
	LOG_D("glMapNamedBuffer, buffer: %u, access: 0x%X", buffer, access);
	
	if (buffer == 0) {
		LOG_E("Invalid buffer ID for glMapNamedBuffer");
		return nullptr;
	}
	temporarilyBindBuffer(buffer);
	void* mappedData = glMapBuffer(GL_ARRAY_BUFFER, access);
	restoreTemporaryBufferBinding();
	
	if (!mappedData) {
		LOG_E("Failed to map buffer %u", buffer);
	} else {
		LOG_D("Mapped buffer %u successfully", buffer);
	}
	return mappedData;
}

GLvoid* glMapNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access) {
	LOG()
	LOG_D("glMapNamedBufferRange, buffer: %u, offset: %lld, length: %lld, access: 0x%X", buffer, offset, length, access);
	
	if (buffer == 0 || length <= 0 || offset < 0) {
		LOG_E("Invalid parameters for glMapNamedBufferRange");
		return nullptr;
	}
	temporarilyBindBuffer(buffer);
	void* mappedData = glMapBufferRange(GL_ARRAY_BUFFER, offset, length, access);
	restoreTemporaryBufferBinding();
	
	if (!mappedData) {
		LOG_E("Failed to map buffer range for buffer %u", buffer);
	} else {
		LOG_D("Mapped buffer range for buffer %u successfully", buffer);
	}
	return mappedData;
}

GLboolean glUnmapNamedBuffer(GLuint buffer) {
	LOG()
	LOG_D("glUnmapNamedBuffer, buffer: %u", buffer);
	
	if (buffer == 0) {
		LOG_E("Invalid buffer ID for glUnmapNamedBuffer");
		return GL_FALSE;
	}
	temporarilyBindBuffer(buffer);
	GLboolean result = glUnmapBuffer(GL_ARRAY_BUFFER);
	restoreTemporaryBufferBinding();
	
	if (result == GL_FALSE) {
		LOG_E("Failed to unmap buffer %u", buffer);
	} else {
		LOG_D("Unmapped buffer %u successfully", buffer);
	}
	return result;
}

void glFlushMappedNamedBufferRange(GLuint buffer, GLintptr offset, GLsizeiptr length) {
	LOG()
	LOG_D("glFlushMappedNamedBufferRange, buffer: %u, offset: %lld, length: %lld", buffer, offset, length);
	
	if (buffer == 0 || length <= 0 || offset < 0) {
		LOG_E("Invalid parameters for glFlushMappedNamedBufferRange");
		return;
	}
	temporarilyBindBuffer(buffer);
	glFlushMappedBufferRange(GL_ARRAY_BUFFER, offset, length);
	restoreTemporaryBufferBinding();
	
	LOG_D("Flushed mapped range of buffer %u from offset %lld with length %lld", buffer, offset, length);
}

void glGetNamedBufferParameteriv(GLuint buffer, GLenum pname, GLint* params) {
	LOG()
	LOG_D("glGetNamedBufferParameteriv, buffer: %u, pname: 0x%X, params: %p", buffer, pname, params);
	
	if (buffer == 0 || !params) {
		LOG_E("Invalid parameters for glGetNamedBufferParameteriv");
		return;
	}
	temporarilyBindBuffer(buffer);
	glGetBufferParameteriv(GL_ARRAY_BUFFER, pname, params);
	restoreTemporaryBufferBinding();
	
	LOG_D("Retrieved buffer parameter 0x%X for buffer %u", pname, buffer);
}

void glGetNamedBufferParameteri64v(GLuint buffer, GLenum pname, GLint64* params) {
	LOG()
	LOG_D("glGetNamedBufferParameteri64v, buffer: %u, pname: 0x%X, params: %p", buffer, pname, params);
	
	if (buffer == 0 || !params) {
		LOG_E("Invalid parameters for glGetNamedBufferParameteri64v");
		return;
	}
	temporarilyBindBuffer(buffer);
	glGetBufferParameteri64v(GL_ARRAY_BUFFER, pname, params);
	restoreTemporaryBufferBinding();
	
	LOG_D("Retrieved 64-bit buffer parameter 0x%X for buffer %u", pname, buffer);
}

void glGetNamedBufferPointerv(GLuint buffer, GLenum pname, void** params) {
	LOG()
	LOG_D("glGetNamedBufferPointerv, buffer: %u, pname: 0x%X, params: %p", buffer, pname, params);
	
	if (buffer == 0 || !params) {
		LOG_E("Invalid parameters for glGetNamedBufferPointerv");
		return;
	}
	temporarilyBindBuffer(buffer);
	glGetBufferPointerv(GL_ARRAY_BUFFER, pname, params);
	restoreTemporaryBufferBinding();
	
	LOG_D("Retrieved buffer pointer parameter 0x%X for buffer %u", pname, buffer);
}

void glGetNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, void* data) {
	LOG()
	LOG_D("glGetNamedBufferSubData, buffer: %u, offset: %lld, size: %lld, data: %p", buffer, offset, size, data);
	
	if (buffer == 0 || size <= 0 || offset < 0 || !data) {
		LOG_E("Invalid parameters for glGetNamedBufferSubData");
		return;
	}
	temporarilyBindBuffer(buffer);
	glGetBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
	restoreTemporaryBufferBinding();
	
	LOG_D("Retrieved sub-data from buffer %u with size %lld at offset %lld", buffer, size, offset);
}

// framebuffer
static thread_local ankerl::unordered_dense::map<GLenum, std::vector<GLuint>> framebufferBindingStack;
void temporarilyBindFramebuffer(GLuint framebufferID, GLenum target = GL_FRAMEBUFFER) {
	GLenum bindingQuery = GetBindingQuery(target);
	GLint prev = 0;
	glGetIntegerv(bindingQuery, &prev);
	framebufferBindingStack[target].push_back(static_cast<GLuint>(prev));
	LOG_D("[TempBind] target=0x%X, prev=%u -> bind=%u", target, prev, framebufferID);
	glBindFramebuffer(target, framebufferID);
}
void restoreTemporaryFramebufferBinding(GLenum target = GL_FRAMEBUFFER) {
	auto it = framebufferBindingStack.find(target);
	if (it == framebufferBindingStack.end() || it->second.empty()) {
		LOG_D("[Restore] no saved binding for target 0x%X", target);
		return;
	}
	GLuint toRestore = it->second.back();
	it->second.pop_back();
	LOG_D("[Restore] target=0x%X, bind back to %u", target, toRestore);
	glBindFramebuffer(target, toRestore);
	if (it->second.empty())
		framebufferBindingStack.erase(it);
}

void glCreateFramebuffers(GLsizei n, GLuint* framebuffers) {
	LOG()
	LOG_D("glCreateFramebuffers, n: %d, framebuffers: %p", n, framebuffers);
	
	if (n <= 0 || !framebuffers) {
		LOG_E("Invalid parameters for glCreateFramebuffers");
		return;
	}
	for (GLsizei i = 0; i < n; ++i) {
		GLuint fboID = 0;
		glGenFramebuffers(1, &fboID);
		if (fboID == 0) {
			LOG_E("Failed to create framebuffer at index %d", i);
			continue;
		}
		temporarilyBindFramebuffer(fboID); // after binding, the framebuffer object should be created
		restoreTemporaryFramebufferBinding();
		framebuffers[i] = fboID;
	}
	
	LOG_D("Created %d framebuffers successfully", n);
}

void glNamedFramebufferRenderbuffer(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
	LOG()
	LOG_D("glNamedFramebufferRenderbuffer, framebuffer: %u, attachment: 0x%X, renderbuffertarget: 0x%X, renderbuffer: %u", framebuffer, attachment, renderbuffertarget, renderbuffer);
	
	temporarilyBindFramebuffer(framebuffer);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, renderbuffertarget, renderbuffer);
	restoreTemporaryFramebufferBinding();
	
	LOG_D("Attached renderbuffer %u to framebuffer %u with attachment 0x%X", renderbuffer, framebuffer, attachment);
}

void glNamedFramebufferParameteri(GLuint framebuffer, GLenum pname, GLint param) {
	LOG()
	LOG_D("glNamedFramebufferParameteri, framebuffer: %u, pname: 0x%X, param: %d", framebuffer, pname, param);
	
	temporarilyBindFramebuffer(framebuffer);
	glFramebufferParameteri(GL_FRAMEBUFFER, pname, param);
	restoreTemporaryFramebufferBinding();
	
	LOG_D("Set framebuffer parameter 0x%X to %d for framebuffer %u", pname, param, framebuffer);
}

void glNamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level) {
	LOG()
	LOG_D("glNamedFramebufferTexture, framebuffer: %u, attachment: 0x%X, texture: %u, level: %d", framebuffer, attachment, texture, level);
	
	if (texture == 0) {
		LOG_E("Invalid parameters for glNamedFramebufferTexture");
		return;
	}
	temporarilyBindFramebuffer(framebuffer);
	glFramebufferTexture(GL_FRAMEBUFFER, attachment, texture, level);
	restoreTemporaryFramebufferBinding();
	
	LOG_D("Attached texture %u to framebuffer %u with attachment 0x%X at level %d", texture, framebuffer, attachment, level);
}

void glNamedFramebufferTextureLayer(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer) {
	LOG()
	LOG_D("glNamedFramebufferTextureLayer, framebuffer: %u, attachment: 0x%X, texture: %u, level: %d, layer: %d", framebuffer, attachment, texture, level, layer);
	
	if (texture == 0) {
		LOG_E("Invalid parameters for glNamedFramebufferTextureLayer");
		return;
	}
	temporarilyBindFramebuffer(framebuffer);
	glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, texture, level, layer);
	restoreTemporaryFramebufferBinding();
	
	LOG_D("Attached texture %u to framebuffer %u with attachment 0x%X at level %d and layer %d", texture, framebuffer, attachment, level, layer);
}

void glNamedFramebufferDrawBuffer(GLuint framebuffer, GLenum mode) {
	LOG()
	LOG_D("glNamedFramebufferDrawBuffer, framebuffer: %u, mode: 0x%X", framebuffer, mode);
	
	temporarilyBindFramebuffer(framebuffer);
	glDrawBuffer(mode);
	restoreTemporaryFramebufferBinding();
	
	LOG_D("Set draw buffer mode 0x%X for framebuffer %u", mode, framebuffer);
}

void glNamedFramebufferDrawBuffers(GLuint framebuffer, GLsizei n, const GLenum* bufs) {
	LOG()
	LOG_D("glNamedFramebufferDrawBuffers, framebuffer: %u, n: %d, bufs: %p", framebuffer, n, bufs);
	
	if (n <= 0 || !bufs) {
		LOG_E("Invalid parameters for glNamedFramebufferDrawBuffers");
		return;
	}
	temporarilyBindFramebuffer(framebuffer);
	glDrawBuffers(n, bufs);
	restoreTemporaryFramebufferBinding();
	
	LOG_D("Set %d draw buffers for framebuffer %u", n, framebuffer);
}

void glNamedFramebufferReadBuffer(GLuint framebuffer, GLenum mode) {
	LOG()
	LOG_D("glNamedFramebufferReadBuffer, framebuffer: %u, mode: 0x%X", framebuffer, mode);
	
	temporarilyBindFramebuffer(framebuffer);
	glReadBuffer(mode);
	restoreTemporaryFramebufferBinding();
	
	LOG_D("Set read buffer mode 0x%X for framebuffer %u", mode, framebuffer);
}

void glInvalidateNamedFramebufferData(GLuint framebuffer, GLsizei numAttachments, const GLenum* attachments) {
	LOG()
	LOG_D("glInvalidateNamedFramebufferData, framebuffer: %u, numAttachments: %d, attachments: %p", framebuffer, numAttachments, attachments);
	
	if (numAttachments <= 0 || !attachments) {
		LOG_E("Invalid parameters for glInvalidateNamedFramebufferData");
		return;
	}
	temporarilyBindFramebuffer(framebuffer);
	glInvalidateFramebuffer(GL_FRAMEBUFFER, numAttachments, attachments);
	restoreTemporaryFramebufferBinding();
	
	LOG_D("Invalidated framebuffer %u with %d attachments", framebuffer, numAttachments);
}

void glInvalidateNamedFramebufferSubData(GLuint framebuffer, GLsizei numAttachments, const GLenum* attachments, GLint x, GLint y, GLsizei width, GLsizei height) {
	LOG()
	LOG_D("glInvalidateNamedFramebufferSubData, framebuffer: %u, numAttachments: %d, attachments: %p, x: %d, y: %d, width: %d, height: %d", framebuffer, numAttachments, attachments, x, y, width, height);
	
	if (numAttachments <= 0 || !attachments || width <= 0 || height <= 0) {
		LOG_E("Invalid parameters for glInvalidateNamedFramebufferSubData");
		return;
	}
	temporarilyBindFramebuffer(framebuffer);
	glInvalidateSubFramebuffer(GL_FRAMEBUFFER, numAttachments, attachments, x, y, width, height);
	restoreTemporaryFramebufferBinding();
	
	LOG_D("Invalidated sub-data of framebuffer %u with %d attachments at (%d, %d) with size (%d, %d)", framebuffer, numAttachments, x, y, width, height);
}

void glClearNamedFramebufferiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint* value) {
	LOG()
	LOG_D("glClearNamedFramebufferiv, framebuffer: %u, buffer: 0x%X, drawbuffer: %d, value: %p", framebuffer, buffer, drawbuffer, value);
	
	if (!value) {
		LOG_E("Invalid parameters for glClearNamedFramebufferiv");
		return;
	}
	temporarilyBindFramebuffer(framebuffer);
	glClearBufferiv(buffer, drawbuffer, value);
	restoreTemporaryFramebufferBinding();
	
	LOG_D("Cleared framebuffer %u with buffer 0x%X at drawbuffer %d", framebuffer, buffer, drawbuffer);
}

void glClearNamedFramebufferuiv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint* value) {
	LOG()
	LOG_D("glClearNamedFramebufferuiv, framebuffer: %u, buffer: 0x%X, drawbuffer: %d, value: %p", framebuffer, buffer, drawbuffer, value);
	
	if (!value) {
		LOG_E("Invalid parameters for glClearNamedFramebufferuiv");
		return;
	}
	temporarilyBindFramebuffer(framebuffer);
	glClearBufferuiv(buffer, drawbuffer, value);
	restoreTemporaryFramebufferBinding();
	
	LOG_D("Cleared framebuffer %u with unsigned int buffer 0x%X at drawbuffer %d", framebuffer, buffer, drawbuffer);
}

void glClearNamedFramebufferfv(GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat* value) {
	LOG()
	LOG_D("glClearNamedFramebufferfv, framebuffer: %u, buffer: 0x%X, drawbuffer: %d, value: %p", framebuffer, buffer, drawbuffer, value);
	
	if (!value) {
		LOG_E("Invalid parameters for glClearNamedFramebufferfv");
		return;
	}
	temporarilyBindFramebuffer(framebuffer);
	glClearBufferfv(buffer, drawbuffer, value);
	restoreTemporaryFramebufferBinding();
	
	LOG_D("Cleared framebuffer %u with float buffer 0x%X at drawbuffer %d", framebuffer, buffer, drawbuffer);
}

void glClearNamedFramebufferfi(GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
	LOG()
	LOG_D("glClearNamedFramebufferfi, framebuffer: %u, buffer: 0x%X, drawbuffer: %d, depth: %f, stencil: %d", framebuffer, buffer, drawbuffer, depth, stencil);
	
	temporarilyBindFramebuffer(framebuffer);
	glClearBufferfi(buffer, drawbuffer, depth, stencil);
	restoreTemporaryFramebufferBinding();
	
	LOG_D("Cleared framebuffer %u with float and int buffer 0x%X at drawbuffer %d", framebuffer, buffer, drawbuffer);
}

void glBlitNamedFramebuffer(GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
	GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
	LOG()
	LOG_D("glBlitNamedFramebuffer, readFramebuffer: %u, drawFramebuffer: %u, src: (%d, %d) to (%d, %d), dst: (%d, %d) to (%d, %d), mask: 0x%X, filter: 0x%X",
		readFramebuffer, drawFramebuffer, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
	
	temporarilyBindFramebuffer(readFramebuffer, GL_READ_FRAMEBUFFER);
	temporarilyBindFramebuffer(drawFramebuffer, GL_DRAW_FRAMEBUFFER);
	glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1,
		dstX0, dstY0, dstX1, dstY1,
		mask, filter);
	restoreTemporaryFramebufferBinding(GL_READ_FRAMEBUFFER);
	restoreTemporaryFramebufferBinding(GL_DRAW_FRAMEBUFFER);
	
	LOG_D("Blitted from framebuffer %u to framebuffer %u", readFramebuffer, drawFramebuffer);
}

GLenum glCheckNamedFramebufferStatus(GLuint framebuffer, GLenum target) {
	LOG()
	LOG_D("glCheckNamedFramebufferStatus, framebuffer: %u, target: 0x%X", framebuffer, target);
	
	temporarilyBindFramebuffer(framebuffer, target);
	GLenum status = glCheckFramebufferStatus(target);
	restoreTemporaryFramebufferBinding(target);
	
	LOG_D("Checked framebuffer %u status: 0x%X", framebuffer, status);
	return status;
}

void glGetNamedFramebufferParameteriv(GLuint framebuffer, GLenum pname, GLint* param) {
	LOG()
	LOG_D("glGetNamedFramebufferParameteriv, framebuffer: %u, pname: 0x%X, param: %p", framebuffer, pname, param);
	
	if (!param) {
		LOG_E("Invalid parameters for glGetNamedFramebufferParameteriv");
		return;
	}
	temporarilyBindFramebuffer(framebuffer);
	glGetFramebufferParameteriv(GL_FRAMEBUFFER, pname, param);
	restoreTemporaryFramebufferBinding();
	
	LOG_D("Retrieved framebuffer parameter 0x%X for framebuffer %u", pname, framebuffer);
}

void glGetNamedFramebufferAttachmentParameteriv(GLuint framebuffer, GLenum attachment, GLenum pname, GLint* params) {
	LOG()
	LOG_D("glGetNamedFramebufferAttachmentParameteriv, framebuffer: %u, attachment: 0x%X, pname: 0x%X, params: %p", framebuffer, attachment, pname, params);
	
	if (!params) {
		LOG_E("Invalid parameters for glGetNamedFramebufferAttachmentParameteriv");
		return;
	}
	temporarilyBindFramebuffer(framebuffer);
	glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment, pname, params);
	restoreTemporaryFramebufferBinding();
	
	LOG_D("Retrieved framebuffer attachment parameter 0x%X for framebuffer %u and attachment 0x%X", pname, framebuffer, attachment);
}

// renderbuffer
static thread_local ankerl::unordered_dense::map<GLenum, std::vector<GLuint>> renderbufferBindingStack;
void temporarilyBindRenderbuffer(GLuint renderbufferID) {
	GLenum bindingQuery = GetBindingQuery(GL_RENDERBUFFER);
	GLint prev = 0;
	glGetIntegerv(bindingQuery, &prev);
	renderbufferBindingStack[GL_RENDERBUFFER].push_back(static_cast<GLuint>(prev));
	LOG_D("[TempBind] prev=%u -> bind=%u", prev, renderbufferID);
	glBindRenderbuffer(GL_RENDERBUFFER, renderbufferID);
}
void restoreTemporaryRenderbufferBinding() {
	auto it = renderbufferBindingStack.find(GL_RENDERBUFFER);
	if (it == renderbufferBindingStack.end() || it->second.empty()) {
		LOG_D("[Restore] no saved binding for GL_RENDERBUFFER");
		return;
	}
	GLuint toRestore = it->second.back();
	it->second.pop_back();
	LOG_D("[Restore] bind back to %u", toRestore);
	glBindRenderbuffer(GL_RENDERBUFFER, toRestore);
	if (it->second.empty())
		renderbufferBindingStack.erase(it);
}

void glCreateRenderbuffers(GLsizei n, GLuint* renderbuffers) {
	LOG()
	LOG_D("glCreateRenderbuffers, n: %d, renderbuffers: %p", n, renderbuffers);
	
	if (n <= 0 || !renderbuffers) {
		LOG_E("Invalid parameters for glCreateRenderbuffers");
		return;
	}
	for (GLsizei i = 0; i < n; ++i) {
		GLuint rboID = 0;
		glGenRenderbuffers(1, &rboID);
		if (rboID == 0) {
			LOG_E("Failed to create renderbuffer at index %d", i);
			continue;
		}
		temporarilyBindRenderbuffer(rboID); // after binding, the renderbuffer object should be created
		restoreTemporaryRenderbufferBinding();
		renderbuffers[i] = rboID;
	}
	
	LOG_D("Created %d renderbuffers successfully", n);
}

void glNamedRenderbufferStorage(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height) {
	LOG()
	LOG_D("glNamedRenderbufferStorage, renderbuffer: %u, internalformat: 0x%X, width: %d, height: %d", renderbuffer, internalformat, width, height);
	
	if (renderbuffer == 0 || width <= 0 || height <= 0) {
		LOG_E("Invalid parameters for glNamedRenderbufferStorage");
		return;
	}
	temporarilyBindRenderbuffer(renderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, internalformat, width, height);
	restoreTemporaryRenderbufferBinding();
	
	LOG_D("Set storage for renderbuffer %u with internal format 0x%X and size (%d, %d)", renderbuffer, internalformat, width, height);
}

void glNamedRenderbufferStorageMultisample(GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
	LOG()
	LOG_D("glNamedRenderbufferStorageMultisample, renderbuffer: %u, samples: %d, internalformat: 0x%X, width: %d, height: %d", renderbuffer, samples, internalformat, width, height);
	
	if (renderbuffer == 0 || samples <= 0 || width <= 0 || height <= 0) {
		LOG_E("Invalid parameters for glNamedRenderbufferStorageMultisample");
		return;
	}
	temporarilyBindRenderbuffer(renderbuffer);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, internalformat, width, height);
	restoreTemporaryRenderbufferBinding();
	
	LOG_D("Set multisample storage for renderbuffer %u with internal format 0x%X and size (%d, %d)", renderbuffer, internalformat, width, height);
}

void glGetNamedRenderbufferParameteriv(GLuint renderbuffer, GLenum pname, GLint* params) {
	LOG()
	LOG_D("glGetNamedRenderbufferParameteriv, renderbuffer: %u, pname: 0x%X, params: %p", renderbuffer, pname, params);
	
	if (renderbuffer == 0 || !params) {
		LOG_E("Invalid parameters for glGetNamedRenderbufferParameteriv");
		return;
	}
	temporarilyBindRenderbuffer(renderbuffer);
	glGetRenderbufferParameteriv(GL_RENDERBUFFER, pname, params);
	restoreTemporaryRenderbufferBinding();
	
	LOG_D("Retrieved renderbuffer parameter 0x%X for renderbuffer %u", pname, renderbuffer);
}

// texture
static thread_local ankerl::unordered_dense::map<GLenum, std::vector<GLuint>> textureBindingStack;
void temporarilyBindTexture(GLuint textureID, GLenum target = GL_TEXTURE_2D) {
	GLenum bindingQuery = GetBindingQuery(target, true);
	GLint prev = 0;
	glGetIntegerv(bindingQuery, &prev);
	textureBindingStack[target].push_back(static_cast<GLuint>(prev));
	LOG_D("[TempBind] target=0x%X, prev=%u -> bind=%u", target, prev, textureID);
	glBindTexture(target, textureID);
}
void restoreTemporaryTextureBinding(GLenum target = GL_TEXTURE_2D) {
	auto it = textureBindingStack.find(target);
	if (it == textureBindingStack.end() || it->second.empty()) {
		LOG_D("[Restore] no saved binding for target 0x%X", target);
		return;
	}
	GLuint toRestore = it->second.back();
	it->second.pop_back();
	LOG_D("[Restore] target=0x%X, bind back to %u", target, toRestore);
	glBindTexture(target, toRestore);
	if (it->second.empty())
		textureBindingStack.erase(it);
}

void glCreateTextures(GLenum target, GLsizei n, GLuint* textures) {
	LOG()
	LOG_D("glCreateTextures, target: 0x%X, n: %d, textures: %p", target, n, textures);
	
	if (n <= 0 || !textures) {
		LOG_E("Invalid parameters for glCreateTextures");
		return;
	}
	for (GLsizei i = 0; i < n; ++i) {
		GLuint texID = 0;
		glGenTextures(1, &texID);
		if (texID == 0) {
			LOG_E("Failed to create texture at index %d", i);
			continue;
		}
		temporarilyBindTexture(texID, target); // after binding, the texture object should be created
		restoreTemporaryTextureBinding(target);
		textures[i] = texID;
	}
	
	LOG_D("Created %d textures successfully", n);
}

void glTextureBuffer(GLuint texture, GLenum internalformat, GLuint buffer) {
	LOG()
	LOG_D("glTextureBuffer, texture: %u, internalformat: 0x%X, buffer: %u", texture, internalformat, buffer);
	
	if (texture == 0 || buffer == 0) {
		LOG_E("Invalid parameters for glTextureBuffer");
		return;
	}
	temporarilyBindTexture(texture);
	glTexBuffer(GL_TEXTURE_BUFFER, internalformat, buffer);
	restoreTemporaryTextureBinding();
	
	LOG_D("Set buffer for texture %u with internal format 0x%X", texture, internalformat);
}

void glTextureBufferRange(GLuint texture, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
	LOG()
	LOG_D("glTextureBufferRange, texture: %u, internalformat: 0x%X, buffer: %u, offset: %lld, size: %lld", texture, internalformat, buffer, offset, size);
	
	if (texture == 0 || buffer == 0 || size <= 0 || offset < 0) {
		LOG_E("Invalid parameters for glTextureBufferRange");
		return;
	}
	temporarilyBindTexture(texture);
	glTexBufferRange(GL_TEXTURE_BUFFER, internalformat, buffer, offset, size);
	restoreTemporaryTextureBinding();
	
	LOG_D("Set buffer range for texture %u with internal format 0x%X and size %lld at offset %lld", texture, internalformat, size, offset);
}

void glTextureStorage1D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width) {
	LOG()
	LOG_D("glTextureStorage1D, texture: %u, levels: %d, internalformat: 0x%X, width: %d", texture, levels, internalformat, width);
	
	if (texture == 0 || levels <= 0 || width <= 0) {
		LOG_E("Invalid parameters for glTextureStorage1D");
		return;
	}
	temporarilyBindTexture(texture, GL_TEXTURE_1D);
	glTexStorage1D(GL_TEXTURE_1D, levels, internalformat, width);
	restoreTemporaryTextureBinding(GL_TEXTURE_1D);
	
	LOG_D("Set storage for texture %u with internal format 0x%X and size %d", texture, internalformat, width);
}

void glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
	LOG()
	LOG_D("glTextureStorage2D, texture: %u, levels: %d, internalformat: 0x%X, width: %d, height: %d", texture, levels, internalformat, width, height);
	
	if (texture == 0 || levels <= 0 || width <= 0 || height <= 0) {
		LOG_E("Invalid parameters for glTextureStorage2D");
		return;
	}
	temporarilyBindTexture(texture, GL_TEXTURE_2D);
	glTexStorage2D(GL_TEXTURE_2D, levels, internalformat, width, height);
	restoreTemporaryTextureBinding(GL_TEXTURE_2D);
	
	LOG_D("Set storage for texture %u with internal format 0x%X and size (%d, %d)", texture, internalformat, width, height);
}

void glTextureStorage3D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
	LOG()
	LOG_D("glTextureStorage3D, texture: %u, levels: %d, internalformat: 0x%X, width: %d, height: %d, depth: %d", texture, levels, internalformat, width, height, depth);
	
	if (texture == 0 || levels <= 0 || width <= 0 || height <= 0 || depth <= 0) {
		LOG_E("Invalid parameters for glTextureStorage3D");
		return;
	}
	temporarilyBindTexture(texture, GL_TEXTURE_3D);
	glTexStorage3D(GL_TEXTURE_3D, levels, internalformat, width, height, depth);
	restoreTemporaryTextureBinding(GL_TEXTURE_3D);
	
	LOG_D("Set storage for texture %u with internal format 0x%X and size (%d, %d, %d)", texture, internalformat, width, height, depth);
}

void glTextureStorage2DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {
	LOG()
	LOG_D("glTextureStorage2DMultisample, texture: %u, samples: %d, internalformat: 0x%X, width: %d, height: %d, fixedsamplelocations: %d", texture, samples, internalformat, width, height, fixedsamplelocations);
	
	if (texture == 0 || samples <= 0 || width <= 0 || height <= 0) {
		LOG_E("Invalid parameters for glTextureStorage2DMultisample");
		return;
	}
	temporarilyBindTexture(texture, GL_TEXTURE_2D_MULTISAMPLE);
	glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internalformat, width, height, fixedsamplelocations);
	restoreTemporaryTextureBinding(GL_TEXTURE_2D_MULTISAMPLE);
	
	LOG_D("Set multisample storage for texture %u with internal format 0x%X and size (%d, %d)", texture, internalformat, width, height);
}

void glTextureStorage3DMultisample(GLuint texture, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
	LOG()
	LOG_D("glTextureStorage3DMultisample, texture: %u, samples: %d, internalformat: 0x%X, width: %d, height: %d, depth: %d, fixedsamplelocations: %d", texture, samples, internalformat, width, height, depth, fixedsamplelocations);
	
	if (texture == 0 || samples <= 0 || width <= 0 || height <= 0 || depth <= 0) {
		LOG_E("Invalid parameters for glTextureStorage3DMultisample");
		return;
	}
	temporarilyBindTexture(texture, GL_TEXTURE_3D);
	glTexStorage3DMultisample(GL_TEXTURE_3D, samples, internalformat, width, height, depth, fixedsamplelocations);
	restoreTemporaryTextureBinding(GL_TEXTURE_3D);
	
	LOG_D("Set multisample storage for texture %u with internal format 0x%X and size (%d, %d, %d)", texture, internalformat, width, height, depth);
}

void glTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void* pixels) {
	LOG()
	LOG_D("glTextureSubImage1D, texture: %u, level: %d, xoffset: %d, width: %d, format: 0x%X, type: 0x%X, pixels: %p", texture, level, xoffset, width, format, type, pixels);
	
	if (texture == 0 || width <= 0 || xoffset < 0 || !pixels) {
		LOG_E("Invalid parameters for glTextureSubImage1D");
		return;
	}
	temporarilyBindTexture(texture);
	glTexSubImage1D(GL_TEXTURE_1D, level, xoffset, width, format, type, pixels);
	restoreTemporaryTextureBinding();
	
	LOG_D("Updated sub-image of texture %u at level %d with size %d at offset %d", texture, level, width, xoffset);
}

void glTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
	LOG()
	LOG_D("glTextureSubImage2D, texture: %u, level: %d, xoffset: %d, yoffset: %d, width: %d, height: %d, format: 0x%X, type: 0x%X, pixels: %p", texture, level, xoffset, yoffset, width, height, format, type, pixels);
	
	if (texture == 0 || width <= 0 || height <= 0 || xoffset < 0 || yoffset < 0 || !pixels) {
		LOG_E("Invalid parameters for glTextureSubImage2D");
		return;
	}
	temporarilyBindTexture(texture);
	glTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width, height, format, type, pixels);
	restoreTemporaryTextureBinding();
	
	LOG_D("Updated sub-image of texture %u at level %d with size (%d, %d) at offset (%d, %d)", texture, level, width, height, xoffset, yoffset);
}

void glTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) {
	LOG()
	LOG_D("glTextureSubImage3D, texture: %u, level: %d, xoffset: %d, yoffset: %d, zoffset: %d, width: %d, height: %d, depth: %d, format: 0x%X, type: 0x%X, pixels: %p", texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
	
	if (texture == 0 || width <= 0 || height <= 0 || depth <= 0 || xoffset < 0 || yoffset < 0 || zoffset < 0 || !pixels) {
		LOG_E("Invalid parameters for glTextureSubImage3D");
		return;
	}
	temporarilyBindTexture(texture);
	glTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
	restoreTemporaryTextureBinding();
	
	LOG_D("Updated sub-image of texture %u at level %d with size (%d, %d, %d) at offset (%d, %d, %d)", texture, level, width, height, depth, xoffset, yoffset, zoffset);
}

void glCompressedTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void* data) {
	LOG()
	LOG_D("glCompressedTextureSubImage1D, texture: %u, level: %d, xoffset: %d, width: %d, format: 0x%X, imageSize: %d, data: %p", texture, level, xoffset, width, format, imageSize, data);
	
	if (texture == 0 || width <= 0 || xoffset < 0 || !data || imageSize <= 0) {
		LOG_E("Invalid parameters for glCompressedTextureSubImage1D");
		return;
	}
	temporarilyBindTexture(texture);
	glCompressedTexSubImage1D(GL_TEXTURE_1D, level, xoffset, width, format, imageSize, data);
	restoreTemporaryTextureBinding();
	
	LOG_D("Updated compressed sub-image of texture %u at level %d with size %d at offset %d", texture, level, width, xoffset);
}

void glCompressedTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
	LOG()
	LOG_D("glCompressedTextureSubImage2D, texture: %u, level: %d, xoffset: %d, yoffset: %d, width: %d, height: %d, format: 0x%X, imageSize: %d, data: %p", texture, level, xoffset, yoffset, width, height, format, imageSize, data);
	
	if (texture == 0 || width <= 0 || height <= 0 || xoffset < 0 || yoffset < 0 || !data || imageSize <= 0) {
		LOG_E("Invalid parameters for glCompressedTextureSubImage2D");
		return;
	}
	temporarilyBindTexture(texture);
	glCompressedTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width, height, format, imageSize, data);
	restoreTemporaryTextureBinding();
	
	LOG_D("Updated compressed sub-image of texture %u at level %d with size (%d, %d) at offset (%d, %d)", texture, level, width, height, xoffset, yoffset);
}

void glCompressedTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data) {
	LOG()
	LOG_D("glCompressedTextureSubImage3D, texture: %u, level: %d, xoffset: %d, yoffset: %d, zoffset: %d, width: %d, height: %d, depth: %d, format: 0x%X, imageSize: %d, data: %p", texture, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
	
	if (texture == 0 || width <= 0 || height <= 0 || depth <= 0 || xoffset < 0 || yoffset < 0 || zoffset < 0 || !data || imageSize <= 0) {
		LOG_E("Invalid parameters for glCompressedTextureSubImage3D");
		return;
	}
	temporarilyBindTexture(texture);
	glCompressedTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
	restoreTemporaryTextureBinding();
	
	LOG_D("Updated compressed sub-image of texture %u at level %d with size (%d, %d, %d) at offset (%d, %d, %d)", texture, level, width, height, depth, xoffset, yoffset, zoffset);
}

void glCopyTextureSubImage1D(GLuint texture, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width) {
	LOG()
	LOG_D("glCopyTextureSubImage1D, texture: %u, level: %d, xoffset: %d, x: %d, y: %d, width: %d", texture, level, xoffset, x, y, width);
	
	if (texture == 0 || width <= 0 || xoffset < 0 || x < 0 || y < 0) {
		LOG_E("Invalid parameters for glCopyTextureSubImage1D");
		return;
	}
	temporarilyBindTexture(texture);
	glCopyTexSubImage1D(GL_TEXTURE_1D, level, xoffset, x, y, width);
	restoreTemporaryTextureBinding();
	
	LOG_D("Copied sub-image to texture %u at level %d with size %d at offset %d", texture, level, width, xoffset);
}

void glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
	LOG()
	LOG_D("glCopyTextureSubImage2D, texture: %u, level: %d, xoffset: %d, yoffset: %d, x: %d, y: %d, width: %d, height: %d", texture, level, xoffset, yoffset, x, y, width, height);
	
	if (texture == 0 || width <= 0 || height <= 0 || xoffset < 0 || x < 0 || y < 0 || yoffset < 0) {
		LOG_E("Invalid parameters for glCopyTextureSubImage2D");
		return;
	}
	temporarilyBindTexture(texture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, x, y, width, height);
	restoreTemporaryTextureBinding();
	
	LOG_D("Copied sub-image to texture %u at level %d with size (%d, %d) at offset (%d, %d)", texture, level, width, height, xoffset, yoffset);
}

void glCopyTextureSubImage3D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
	LOG()
	LOG_D("glCopyTextureSubImage3D, texture: %u, level: %d, xoffset: %d, yoffset: %d, zoffset: %d, x: %d, y: %d, width: %d, height: %d", texture, level, xoffset, yoffset, zoffset, x, y, width, height);
	
	if (texture == 0 || width <= 0 || height <= 0 || xoffset < 0 || x < 0 || y < 0 || zoffset < 0 || yoffset < 0) {
		LOG_E("Invalid parameters for glCopyTextureSubImage3D");
		return;
	}
	temporarilyBindTexture(texture);
	glCopyTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset, x, y, width, height);
	restoreTemporaryTextureBinding();
	
	LOG_D("Copied sub-image to texture %u at level %d with size (%d, %d) at offset (%d, %d)", texture, level, width, height, xoffset, yoffset);
}

void glTextureParameterf(GLuint texture, GLenum pname, GLfloat param) {
	LOG()
	LOG_D("glTextureParameterf, texture: %u, pname: 0x%X, param: %f", texture, pname, param);
	
	if (texture == 0) {
		LOG_E("Invalid texture ID for glTextureParameterf");
		return;
	}
	temporarilyBindTexture(texture);
	glTexParameterf(GL_TEXTURE_2D, pname, param);
	restoreTemporaryTextureBinding();
	
	LOG_D("Set float parameter 0x%X for texture %u to %f", pname, texture, param);
}

void glTextureParameterfv(GLuint texture, GLenum pname, const GLfloat* param) {
	LOG()
	LOG_D("glTextureParameterfv, texture: %u, pname: 0x%X, param: %p", texture, pname, param);
	
	if (texture == 0 || !param) {
		LOG_E("Invalid parameters for glTextureParameterfv");
		return;
	}
	temporarilyBindTexture(texture);
	glTexParameterfv(GL_TEXTURE_2D, pname, param);
	restoreTemporaryTextureBinding();
	
	LOG_D("Set float vector parameter 0x%X for texture %u", pname, texture);
}

void glTextureParameteri(GLuint texture, GLenum pname, GLint param) {
	LOG()
	LOG_D("glTextureParameteri, texture: %u, pname: 0x%X, param: %d", texture, pname, param);
	
	if (texture == 0) {
		LOG_E("Invalid texture ID for glTextureParameteri");
		return;
	}
	temporarilyBindTexture(texture);
	glTexParameteri(GL_TEXTURE_2D, pname, param);
	restoreTemporaryTextureBinding();
	
	LOG_D("Set integer parameter 0x%X for texture %u to %d", pname, texture, param);
}

void glTextureParameterIiv(GLuint texture, GLenum pname, const GLint* params) {
	LOG()
	LOG_D("glTextureParameterIiv, texture: %u, pname: 0x%X, params: %p", texture, pname, params);
	
	if (texture == 0 || !params) {
		LOG_E("Invalid parameters for glTextureParameterIiv");
		return;
	}
	temporarilyBindTexture(texture);
	glTexParameterIiv(GL_TEXTURE_2D, pname, params);
	restoreTemporaryTextureBinding();
	
	LOG_D("Set integer vector parameter 0x%X for texture %u", pname, texture);
}

void glTextureParameterIuiv(GLuint texture, GLenum pname, const GLuint* params) {
	LOG()
	LOG_D("glTextureParameterIuiv, texture: %u, pname: 0x%X, params: %p", texture, pname, params);
	
	if (texture == 0 || !params) {
		LOG_E("Invalid parameters for glTextureParameterIuiv");
		return;
	}
	temporarilyBindTexture(texture);
	glTexParameterIuiv(GL_TEXTURE_2D, pname, params);
	restoreTemporaryTextureBinding();
	
	LOG_D("Set unsigned integer vector parameter 0x%X for texture %u", pname, texture);
}

void glTextureParameteriv(GLuint texture, GLenum pname, const GLint* param) {
	LOG()
	LOG_D("glTextureParameteriv, texture: %u, pname: 0x%X, param: %p", texture, pname, param);
	
	if (texture == 0 || !param) {
		LOG_E("Invalid parameters for glTextureParameteriv");
		return;
	}
	temporarilyBindTexture(texture);
	glTexParameteriv(GL_TEXTURE_2D, pname, param);
	restoreTemporaryTextureBinding();
	
	LOG_D("Set integer vector parameter 0x%X for texture %u", pname, texture);
}

void glGenerateTextureMipmap(GLuint texture) {
	LOG()
	LOG_D("glGenerateTextureMipmap, texture: %u", texture);
	
	if (texture == 0) {
		LOG_E("Invalid texture ID for glGenerateTextureMipmap");
		return;
	}
	temporarilyBindTexture(texture);
	glGenerateMipmap(GL_TEXTURE_2D);
	restoreTemporaryTextureBinding();
	
	LOG_D("Generated mipmap for texture %u", texture);
}

void glBindTextureUnit(GLuint unit, GLuint texture) {
	LOG()
	LOG_D("glBindTextureUnit, unit: %u, texture: %u", unit, texture);
	
	if (unit >= GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS || texture == 0) {
		LOG_E("Invalid parameters for glBindTextureUnit");
		return;
	}
	GLenum target = mgGetTexTarget(texture);
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(target, texture);
	LOG_D("Bound texture %u to texture unit %u", texture, unit);
}

void glGetTextureImage(GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* pixels) {
	LOG()
	LOG_D("glGetTextureImage, texture: %u, level: %d, format: 0x%X, type: 0x%X, bufSize: %d, pixels: %p", texture, level, format, type, bufSize, pixels);
	
	if (texture == 0 || bufSize <= 0 || !pixels) {
		LOG_E("Invalid parameters for glGetTextureImage");
		return;
	}
	temporarilyBindTexture(texture);
	glGetTexImage(GL_TEXTURE_2D, level, format, type, pixels);
	restoreTemporaryTextureBinding();
	
	LOG_D("Retrieved texture image from texture %u at level %d", texture, level);
}

void glGetCompressedTextureImage(GLuint texture, GLint level, GLsizei bufSize, void* pixels) {
	LOG()
	LOG_D("glGetCompressedTextureImage, texture: %u, level: %d, bufSize: %d, pixels: %p", texture, level, bufSize, pixels);
	
	if (texture == 0 || bufSize <= 0 || !pixels) {
		LOG_E("Invalid parameters for glGetCompressedTextureImage");
		return;
	}
	temporarilyBindTexture(texture);
	glGetCompressedTexImage(GL_TEXTURE_2D, level, pixels);
	restoreTemporaryTextureBinding();
	
	LOG_D("Retrieved compressed texture image from texture %u at level %d", texture, level);
}

void glGetTextureLevelParameterfv(GLuint texture, GLint level, GLenum pname, GLfloat* params) {
	LOG()
	LOG_D("glGetTextureLevelParameterfv, texture: %u, level: %d, pname: 0x%X, params: %p", texture, level, pname, params);
	
	if (texture == 0 || !params) {
		LOG_E("Invalid parameters for glGetTextureLevelParameterfv");
		return;
	}
	temporarilyBindTexture(texture);
	glGetTexLevelParameterfv(GL_TEXTURE_2D, level, pname, params);
	restoreTemporaryTextureBinding();
	
	LOG_D("Retrieved texture level parameter 0x%X for texture %u at level %d", pname, texture, level);
}

void glGetTextureLevelParameteriv(GLuint texture, GLint level, GLenum pname, GLint* params) {
	LOG()
	LOG_D("glGetTextureLevelParameteriv, texture: %u, level: %d, pname: 0x%X, params: %p", texture, level, pname, params);
	
	if (texture == 0 || !params) {
		LOG_E("Invalid parameters for glGetTextureLevelParameteriv");
		return;
	}
	temporarilyBindTexture(texture);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, level, pname, params);
	restoreTemporaryTextureBinding();
	
	LOG_D("Retrieved texture level parameter 0x%X for texture %u at level %d", pname, texture, level);
}

void glGetTextureParameterfv(GLuint texture, GLenum pname, GLfloat* params) {
	LOG()
	LOG_D("glGetTextureParameterfv, texture: %u, pname: 0x%X, params: %p", texture, pname, params);
	
	if (texture == 0 || !params) {
		LOG_E("Invalid parameters for glGetTextureParameterfv");
		return;
	}
	temporarilyBindTexture(texture);
	glGetTexParameterfv(GL_TEXTURE_2D, pname, params);
	restoreTemporaryTextureBinding();
	
	LOG_D("Retrieved texture parameter 0x%X for texture %u", pname, texture);
}

void glGetTextureParameterIiv(GLuint texture, GLenum pname, GLint* params) {
	LOG()
	LOG_D("glGetTextureParameterIiv, texture: %u, pname: 0x%X, params: %p", texture, pname, params);
	
	if (texture == 0 || !params) {
		LOG_E("Invalid parameters for glGetTextureParameterIiv");
		return;
	}
	temporarilyBindTexture(texture);
	glGetTexParameterIiv(GL_TEXTURE_2D, pname, params);
	restoreTemporaryTextureBinding();
	
	LOG_D("Retrieved integer texture parameter 0x%X for texture %u", pname, texture);
}	

void glGetTextureParameterIuiv(GLuint texture, GLenum pname, GLuint* params) {
	LOG()
	LOG_D("glGetTextureParameterIuiv, texture: %u, pname: 0x%X, params: %p", texture, pname, params);
	
	if (texture == 0 || !params) {
		LOG_E("Invalid parameters for glGetTextureParameterIuiv");
		return;
	}
	temporarilyBindTexture(texture);
	glGetTexParameterIuiv(GL_TEXTURE_2D, pname, params);
	restoreTemporaryTextureBinding();
	
	LOG_D("Retrieved unsigned integer texture parameter 0x%X for texture %u", pname, texture);
}

void glGetTextureParameteriv(GLuint texture, GLenum pname, GLint* params) {
	LOG()
	LOG_D("glGetTextureParameteriv, texture: %u, pname: 0x%X, params: %p", texture, pname, params);
	
	if (texture == 0 || !params) {
		LOG_E("Invalid parameters for glGetTextureParameteriv");
		return;
	}
	temporarilyBindTexture(texture);
	glGetTexParameteriv(GL_TEXTURE_2D, pname, params);
	restoreTemporaryTextureBinding();
	
	LOG_D("Retrieved integer texture parameter 0x%X for texture %u", pname, texture);
}

// vertex array
static thread_local GLint prevVAO = 0;
void temporarilyBindVertexArray(GLint vaoID) {
	if (prevVAO == vaoID) {
		return;
	}
	LOG_D("[TempBind] VAO: %u -> bind=%u", prevVAO, vaoID);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
	glBindVertexArray(vaoID);
}
void restoreTemporaryVertexArrayBinding() {
	if (prevVAO == 0) {
		return;
	}
	LOG_D("[Restore] VAO: bind back to %u", prevVAO);
	glBindVertexArray(prevVAO);
	prevVAO = 0;
}

void glCreateVertexArrays(GLsizei n, GLuint* arrays) {
	LOG()
	LOG_D("glCreateVertexArrays, n: %d, arrays: %p", n, arrays);
	
	if (n <= 0 || !arrays) {
		LOG_E("Invalid parameters for glCreateVertexArrays");
		return;
	}
	for (GLsizei i = 0; i < n; ++i) {
		GLuint vaoID = 0;
		glGenVertexArrays(1, &vaoID);
		if (vaoID == 0) {
			LOG_E("Failed to create vertex array at index %d", i);
			continue;
		}
		temporarilyBindVertexArray(vaoID); // after binding, the vertex array object should be created
		restoreTemporaryVertexArrayBinding();
		arrays[i] = vaoID;
	}
	
	LOG_D("Created %d vertex arrays successfully", n);
}

void glDisableVertexArrayAttrib(GLuint vaobj, GLuint index) {
	LOG()
	LOG_D("glDisableVertexArrayAttrib, vaobj: %u, index: %u", vaobj, index);
	
	if (vaobj == 0 || index >= GL_MAX_VERTEX_ATTRIBS) {
		LOG_E("Invalid parameters for glDisableVertexArrayAttrib");
		return;
	}
	temporarilyBindVertexArray(vaobj);
	glDisableVertexAttribArray(index);
	restoreTemporaryVertexArrayBinding();
	
	LOG_D("Disabled vertex array attribute %u for vertex array object %u", index, vaobj);
}

void glEnableVertexArrayAttrib(GLuint vaobj, GLuint index) {
	LOG()
	LOG_D("glEnableVertexArrayAttrib, vaobj: %u, index: %u", vaobj, index);
	
	if (vaobj == 0 || index >= GL_MAX_VERTEX_ATTRIBS) {
		LOG_E("Invalid parameters for glEnableVertexArrayAttrib");
		return;
	}
	temporarilyBindVertexArray(vaobj);
	glEnableVertexAttribArray(index);
	restoreTemporaryVertexArrayBinding();
	
	LOG_D("Enabled vertex array attribute %u for vertex array object %u", index, vaobj);
}

void glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer) {
	LOG()
	LOG_D("glVertexArrayElementBuffer, vaobj: %u, buffer: %u", vaobj, buffer);
	
	if (vaobj == 0 || buffer == 0) {
		LOG_E("Invalid parameters for glVertexArrayElementBuffer");
		return;
	}
	temporarilyBindVertexArray(vaobj);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
	restoreTemporaryVertexArrayBinding();
	
	LOG_D("Bound element buffer %u to vertex array object %u", buffer, vaobj);
}

void glVertexArrayVertexBuffer(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
	LOG()
	LOG_D("glVertexArrayVertexBuffer, vaobj: %u, bindingindex: %u, buffer: %u, offset: %lld, stride: %d", vaobj, bindingindex, buffer, offset, stride);
	
	if (vaobj == 0 || bindingindex >= GL_MAX_VERTEX_ATTRIB_BINDINGS || buffer == 0 || stride < 0 || offset < 0) {
		LOG_E("Invalid parameters for glVertexArrayVertexBuffer");
		return;
	}
	temporarilyBindVertexArray(vaobj);
	glBindVertexBuffer(bindingindex, buffer, offset, stride);
	restoreTemporaryVertexArrayBinding();
	
	LOG_D("Bound vertex buffer %u to binding index %u for vertex array object %u with offset %lld and stride %d", buffer, bindingindex, vaobj, offset, stride);
}

void glVertexArrayVertexBuffers(GLuint vaobj, GLuint first, GLsizei count, const GLuint* buffers, const GLintptr* offsets, const GLsizei* strides) {
	LOG()
	LOG_D("glVertexArrayVertexBuffers, vaobj: %u, first: %u, count: %d, buffers: %p, offsets: %p, strides: %p", vaobj, first, count, buffers, offsets, strides);
	
	if (vaobj == 0 || first >= GL_MAX_VERTEX_ATTRIB_BINDINGS || count <= 0 || !buffers || !offsets || !strides) {
		LOG_E("Invalid parameters for glVertexArrayVertexBuffers");
		return;
	}
	temporarilyBindVertexArray(vaobj);
	glBindVertexBuffers(first, count, buffers, offsets, strides);
	restoreTemporaryVertexArrayBinding();
	
	LOG_D("Bound vertex buffers starting from index %u for vertex array object %u", first, vaobj);
}

void glVertexArrayAttribFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) {
	LOG()
	LOG_D("glVertexArrayAttribFormat, vaobj: %u, attribindex: %u, size: %d, type: 0x%X, normalized: %d, relativeoffset: %u", vaobj, attribindex, size, type, normalized, relativeoffset);
	
	if (vaobj == 0 || attribindex >= GL_MAX_VERTEX_ATTRIBS || size <= 0 || (type != GL_FLOAT && type != GL_INT && type != GL_UNSIGNED_INT)) {
		LOG_E("Invalid parameters for glVertexArrayAttribFormat");
		return;
	}
	temporarilyBindVertexArray(vaobj);
	glVertexAttribFormat(attribindex, size, type, normalized, relativeoffset);
	restoreTemporaryVertexArrayBinding();
	
	LOG_D("Set vertex array attribute format for index %u in vertex array object %u", attribindex, vaobj);
}

void glVertexArrayAttribIFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
	LOG()
	LOG_D("glVertexArrayAttribIFormat, vaobj: %u, attribindex: %u, size: %d, type: 0x%X, relativeoffset: %u", vaobj, attribindex, size, type, relativeoffset);
	
	if (vaobj == 0 || attribindex >= GL_MAX_VERTEX_ATTRIBS || size <= 0 || (type != GL_INT && type != GL_UNSIGNED_INT)) {
		LOG_E("Invalid parameters for glVertexArrayAttribIFormat");
		return;
	}
	temporarilyBindVertexArray(vaobj);
	glVertexAttribIFormat(attribindex, size, type, relativeoffset);
	restoreTemporaryVertexArrayBinding();
	
	LOG_D("Set integer vertex array attribute format for index %u in vertex array object %u", attribindex, vaobj);
}

void glVertexArrayAttribLFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
	LOG()
	LOG_D("glVertexArrayAttribLFormat, vaobj: %u, attribindex: %u, size: %d, type: 0x%X, relativeoffset: %u", vaobj, attribindex, size, type, relativeoffset);
	
	if (vaobj == 0 || attribindex >= GL_MAX_VERTEX_ATTRIBS || size <= 0 || type != GL_DOUBLE) {
		LOG_E("Invalid parameters for glVertexArrayAttribLFormat");
		return;
	}
	temporarilyBindVertexArray(vaobj);
	glVertexAttribLFormat(attribindex, size, type, relativeoffset);
	restoreTemporaryVertexArrayBinding();
	
	LOG_D("Set double vertex array attribute format for index %u in vertex array object %u", attribindex, vaobj);
}

void glVertexArrayAttribBinding(GLuint vaobj, GLuint attribindex, GLuint bindingindex) {
	LOG()
	LOG_D("glVertexArrayAttribBinding, vaobj: %u, attribindex: %u, bindingindex: %u", vaobj, attribindex, bindingindex);
	
	if (vaobj == 0 || attribindex >= GL_MAX_VERTEX_ATTRIBS || bindingindex >= GL_MAX_VERTEX_ATTRIB_BINDINGS) {
		LOG_E("Invalid parameters for glVertexArrayAttribBinding");
		return;
	}
	temporarilyBindVertexArray(vaobj);
	glVertexAttribBinding(attribindex, bindingindex);
	restoreTemporaryVertexArrayBinding();
	
	LOG_D("Set vertex array attribute binding for index %u in vertex array object %u to binding index %u", attribindex, vaobj, bindingindex);
}

void glVertexArrayBindingDivisor(GLuint vaobj, GLuint bindingindex, GLuint divisor) {
	LOG()
	LOG_D("glVertexArrayBindingDivisor, vaobj: %u, bindingindex: %u, divisor: %u", vaobj, bindingindex, divisor);
	
	if (vaobj == 0 || bindingindex >= GL_MAX_VERTEX_ATTRIB_BINDINGS) {
		LOG_E("Invalid parameters for glVertexArrayBindingDivisor");
		return;
	}
	temporarilyBindVertexArray(vaobj);
	glVertexBindingDivisor(bindingindex, divisor);
	restoreTemporaryVertexArrayBinding();
	
	LOG_D("Set vertex array binding divisor for binding index %u in vertex array object %u to %u", bindingindex, vaobj, divisor);
}

void glGetVertexArrayiv(GLuint vaobj, GLenum pname, GLint* param) {
	LOG()
	LOG_D("glGetVertexArrayiv, vaobj: %u, pname: 0x%X, param: %p", vaobj, pname, param);
	
	if (vaobj == 0 || !param) {
		LOG_E("Invalid parameters for glGetVertexArrayiv");
		return;
	}
	temporarilyBindVertexArray(vaobj);
	glGetVertexArrayiv(vaobj, pname, param);
	restoreTemporaryVertexArrayBinding();
	
	LOG_D("Retrieved vertex array parameter 0x%X for vertex array object %u", pname, vaobj);
}

void glGetVertexArrayIndexediv(GLuint vaobj, GLuint index, GLenum pname, GLint* param) {
	LOG()
	LOG_D("glGetVertexArrayIndexediv, vaobj: %u, index: %u, pname: 0x%X, param: %p", vaobj, index, pname, param);
	
	if (vaobj == 0 || index >= GL_MAX_VERTEX_ATTRIBS || !param) {
		LOG_E("Invalid parameters for glGetVertexArrayIndexediv");
		return;
	}
	temporarilyBindVertexArray(vaobj);
	glGetVertexArrayIndexediv(vaobj, index, pname, param);
	restoreTemporaryVertexArrayBinding();
	
	LOG_D("Retrieved indexed vertex array parameter 0x%X for vertex array object %u at index %u", pname, vaobj, index);
}

void glGetVertexArrayIndexed64iv(GLuint vaobj, GLuint index, GLenum pname, GLint64* param) {
	LOG()
	LOG_D("glGetVertexArrayIndexed64iv, vaobj: %u, index: %u, pname: 0x%X, param: %p", vaobj, index, pname, param);
	
	if (vaobj == 0 || index >= GL_MAX_VERTEX_ATTRIBS || !param) {
		LOG_E("Invalid parameters for glGetVertexArrayIndexed64iv");
		return;
	}
	temporarilyBindVertexArray(vaobj);
	glGetVertexArrayIndexed64iv(vaobj, index, pname, param);
	restoreTemporaryVertexArrayBinding();
	
	LOG_D("Retrieved indexed 64-bit vertex array parameter 0x%X for vertex array object %u at index %u", pname, vaobj, index);
}

// sampler
void glCreateSamplers(GLsizei n, GLuint* samplers) {
	LOG()
	LOG_D("glCreateSamplers, n: %d, samplers: %p", n, samplers);
	
	if (n <= 0 || !samplers) {
		LOG_E("Invalid parameters for glCreateSamplers");
		return;
	}
	for (GLsizei i = 0; i < n; ++i) {
		GLuint samplerID = 0;
		glGenSamplers(1, &samplerID);
		if (samplerID == 0) {
			LOG_E("Failed to create sampler at index %d", i);
			continue;
		}

		GLuint prevSampler = 0;
		glGetIntegerv(GL_SAMPLER_BINDING, (GLint*)&prevSampler);
		glBindSampler(1, samplerID);
		glBindSampler(1, prevSampler);

		samplers[i] = samplerID;
	}
	
	LOG_D("Created %d samplers successfully", n);
}

// program pipeline
void glCreateProgramPipelines(GLsizei n, GLuint* pipelines) {
	LOG()
	LOG_D("glCreateProgramPipelines, n: %d, pipelines: %p", n, pipelines);
	
	if (n <= 0 || !pipelines) {
		LOG_E("Invalid parameters for glCreateProgramPipelines");
		return;
	}
	for (GLsizei i = 0; i < n; ++i) {
		GLuint pipelineID = 0;
		glGenProgramPipelines(1, &pipelineID);
		if (pipelineID == 0) {
			LOG_E("Failed to create program pipeline at index %d", i);
			continue;
		}

		GLuint prevPipeline = 0;
		glGetIntegerv(GL_PROGRAM_PIPELINE_BINDING, (GLint*)&prevPipeline);
		glBindProgramPipeline(pipelineID);
		glBindProgramPipeline(prevPipeline);
		pipelines[i] = pipelineID;
	}
	
	LOG_D("Created %d program pipelines successfully", n);
}

// query
void glCreateQueries(GLenum target, GLsizei n, GLuint* ids) {
	if (n <= 0 || !ids) return;
	glGenQueries(n, ids);
}

static GLint pushQueryBufferBinding(GLuint buffer) {
	GLint prev = 0;
	glGetIntegerv(GL_QUERY_BUFFER_BINDING, &prev);
	glBindBuffer(GL_QUERY_BUFFER, buffer);
	return prev;
}
static void popQueryBufferBinding(GLint prev) {
	glBindBuffer(GL_QUERY_BUFFER, (GLuint)prev);
}

void glGetQueryBufferObjectiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
	assert(pname == GL_QUERY_RESULT || pname == GL_QUERY_RESULT_AVAILABLE);
	GLint prev = pushQueryBufferBinding(buffer);
	GLint value = 0;
	glGetQueryObjectiv(id, pname, &value);
	glBufferSubData(GL_QUERY_BUFFER, offset, sizeof(value), &value);
	popQueryBufferBinding(prev);
}

void glGetQueryBufferObjectuiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
	assert(pname == GL_QUERY_RESULT || pname == GL_QUERY_RESULT_AVAILABLE);
	GLint prev = pushQueryBufferBinding(buffer);
	GLuint value = 0;
	glGetQueryObjectuiv(id, pname, &value);
	glBufferSubData(GL_QUERY_BUFFER, offset, sizeof(value), &value);
	popQueryBufferBinding(prev);
}

void glGetQueryBufferObjecti64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
	assert(pname == GL_QUERY_RESULT || pname == GL_QUERY_RESULT_AVAILABLE);
	GLint prev = pushQueryBufferBinding(buffer);
	GLint64 value = 0;
	glGetQueryObjecti64v(id, pname, &value);
	glBufferSubData(GL_QUERY_BUFFER, offset, sizeof(value), &value);
	popQueryBufferBinding(prev);
}

void glGetQueryBufferObjectui64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
	assert(pname == GL_QUERY_RESULT || pname == GL_QUERY_RESULT_AVAILABLE);
	GLint prev = pushQueryBufferBinding(buffer);
	GLuint64 value = 0;
	glGetQueryObjectui64v(id, pname, &value);
	glBufferSubData(GL_QUERY_BUFFER, offset, sizeof(value), &value);
	popQueryBufferBinding(prev);
}

// transform feedback, similar to vertex array
static thread_local GLint prevTFBO = 0;
void temporarilyBindTransformFeedback(GLint tfboID) {
	if (prevTFBO == tfboID) {
		return;
	}
	LOG_D("[TempBind] TFBO: %u -> bind=%u", prevTFBO, tfboID);
	glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &prevTFBO);
	glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, tfboID);
}
void restoreTemporaryTransformFeedbackBinding() {
	if (prevTFBO == 0) {
		return;
	}
	LOG_D("[Restore] TFBO: bind back to %u", prevTFBO);
	glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, prevTFBO);
	prevTFBO = 0;
}

void glCreateTransformFeedbacks(GLsizei n, GLuint* ids) {
	LOG()
	LOG_D("glCreateTransformFeedbacks, n: %d, ids: %p", n, ids);
	
	if (n <= 0 || !ids) {
		LOG_E("Invalid parameters for glCreateTransformFeedbacks");
		return;
	}
	for (GLsizei i = 0; i < n; ++i) {
		GLuint tfboID = 0;
		glGenBuffers(1, &tfboID);
		if (tfboID == 0) {
			LOG_E("Failed to create transform feedback buffer at index %d", i);
			continue;
		}
		temporarilyBindTransformFeedback(tfboID); // after binding, the transform feedback buffer should be created
		restoreTemporaryTransformFeedbackBinding();
		ids[i] = tfboID;
	}
	
	LOG_D("Created %d transform feedback buffers successfully", n);
}

void glTransformFeedbackBufferBase(GLuint xfb, GLuint index, GLuint buffer) {
	LOG()
		LOG_D("glTransformFeedbackBufferBase, xfb: %u, index: %u, buffer: %u", xfb, index, buffer);

	if (xfb == 0 || index >= GL_MAX_TRANSFORM_FEEDBACK_BUFFERS || buffer == 0) {
		LOG_E("Invalid parameters for glTransformFeedbackBufferBase");
		return;
	}
	temporarilyBindTransformFeedback(xfb);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, index, buffer);
	restoreTemporaryTransformFeedbackBinding();

	LOG_D("Bound transform feedback buffer %u to index %u for transform feedback object %u", buffer, index, xfb);
}

void glTransformFeedbackBufferRange(GLuint xfb, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
	LOG()
	LOG_D("glTransformFeedbackBufferRange, xfb: %u, index: %u, buffer: %u, offset: %lld, size: %lld", xfb, index, buffer, offset, size);
	
	if (xfb == 0 || index >= GL_MAX_TRANSFORM_FEEDBACK_BUFFERS || buffer == 0 || size < 0 || offset < 0) {
		LOG_E("Invalid parameters for glTransformFeedbackBufferRange");
		return;
	}
	temporarilyBindTransformFeedback(xfb);
	glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, index, buffer, offset, size);
	restoreTemporaryTransformFeedbackBinding();
	
	LOG_D("Bound transform feedback buffer %u to index %u with offset %lld and size %lld for transform feedback object %u", buffer, index, offset, size, xfb);
}

void glGetTransformFeedbackiv(GLuint xfb, GLenum pname, GLint* param) {
	LOG()
	LOG_D("glGetTransformFeedbackiv, xfb: %u, pname: 0x%X, param: %p", xfb, pname, param);
	
	if (xfb == 0 || !param) {
		LOG_E("Invalid parameters for glGetTransformFeedbackiv");
		return;
	}
	temporarilyBindTransformFeedback(xfb);
	glGetTransformFeedbackiv(xfb, pname, param);
	restoreTemporaryTransformFeedbackBinding();
	
	LOG_D("Retrieved transform feedback parameter 0x%X for transform feedback object %u", pname, xfb);
}

void glGetTransformFeedbacki_v(GLuint xfb, GLenum pname, GLuint index, GLint* param) {
	LOG()
	LOG_D("glGetTransformFeedbacki_v, xfb: %u, pname: 0x%X, index: %u, param: %p", xfb, pname, index, param);
	
	if (xfb == 0 || index >= GL_MAX_TRANSFORM_FEEDBACK_BUFFERS || !param) {
		LOG_E("Invalid parameters for glGetTransformFeedbacki_v");
		return;
	}
	temporarilyBindTransformFeedback(xfb);
	glGetTransformFeedbacki_v(xfb, pname, index, param);
	restoreTemporaryTransformFeedbackBinding();
	
	LOG_D("Retrieved indexed transform feedback parameter 0x%X for transform feedback object %u at index %u", pname, xfb, index);
}

void glGetTransformFeedbacki64_v(GLuint xfb, GLenum pname, GLuint index, GLint64* param) {
	LOG()
	LOG_D("glGetTransformFeedbacki64_v, xfb: %u, pname: 0x%X, index: %u, param: %p", xfb, pname, index, param);
	
	if (xfb == 0 || index >= GL_MAX_TRANSFORM_FEEDBACK_BUFFERS || !param) {
		LOG_E("Invalid parameters for glGetTransformFeedbacki64_v");
		return;
	}
	temporarilyBindTransformFeedback(xfb);
	glGetTransformFeedbacki64_v(xfb, pname, index, param);
	restoreTemporaryTransformFeedbackBinding();
	
	LOG_D("Retrieved indexed 64-bit transform feedback parameter 0x%X for transform feedback object %u at index %u", pname, xfb, index);
}