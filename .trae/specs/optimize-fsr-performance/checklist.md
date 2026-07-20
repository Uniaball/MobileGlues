# Checklist

- [x] `RecreateFSRFBO` 中 `g_renderTexture` 使用 `GL_RGBA8` + `GL_UNSIGNED_BYTE`，与 `InitFSRResources` 一致
- [x] `InitFSRResources` 与 `RecreateFSRFBO` 中 `g_renderTexture` 和 `g_targetTexture` 使用 `glTexStorage2D` 而非 `glTexImage2D`
- [x] `GLStateGuard` 不再保存/恢复 `GL_ARRAY_BUFFER_BINDING` 和 `GL_RENDERBUFFER_BINDING`
- [x] `GLStateGuard` 仍正确保存/恢复 `GL_CURRENT_PROGRAM`、`GL_VERTEX_ARRAY_BINDING`、`GL_ACTIVE_TEXTURE`、`GL_TEXTURE_BINDING_2D`、`GL_READ_FRAMEBUFFER_BINDING`、`GL_DRAW_FRAMEBUFFER_BINDING`
- [x] `ApplyFSR` 不再调用 `glClear` 清空 `g_targetFBO`
- [x] `ApplyFSR` 在 uniform 值未变化时跳过 `glUniform4fv` / `glUniform2fv` 调用
- [x] `ApplyFSR` 在 viewport 未变化时跳过冗余 `glViewport` 调用
- [x] `RecreateFSRFBO` 末尾将 uniform 缓存与 viewport 缓存标记为脏
- [x] `CompileFSRShader` 末尾将 uniform 缓存标记为脏
- [x] `ApplyFSR` 进入 FSR pass 时显式禁用 `GL_DEPTH_TEST`、`GL_STENCIL_TEST`、`GL_BLEND`，并设置 color mask 全开
- [x] `ApplyFSR` 结束时恢复上述深度/模板/混合/color mask 状态
- [x] `ApplyFSR` 在末尾对 `g_renderFBO` 调用 `glInvalidateFramebuffer` 丢弃 depth/stencil
- [x] `g_renderTexture` 使用 `GL_NEAREST` 过滤模式，`g_targetTexture` 仍使用 `GL_LINEAR`
- [x] `eglSwapBuffers` → `ApplyFSR` → `CheckResolutionChange` 调用顺序与返回值未发生变化
- [x] CMake 构建通过，无新增编译错误或警告
- [x] `glCreateShader` 触发 `InitFSRResources` 的行为未发生变化
