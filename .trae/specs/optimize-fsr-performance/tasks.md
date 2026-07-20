# Tasks

- [x] Task 1: 修复 RecreateFSRFBO 渲染纹理格式
  - [x] SubTask 1.1: 在 `RecreateFSRFBO` 中将 `g_renderTexture` 的 `glTexImage2D` 内部格式由 `GL_RGBA32F` 改回 `GL_RGBA8`，pixel type 由 `GL_FLOAT` 改回 `GL_UNSIGNED_BYTE`
  - [x] SubTask 1.2: 验证 `InitFSRResources` 与 `RecreateFSRFBO` 两条路径的纹理格式与过滤参数完全一致

- [x] Task 2: 改用不可变纹理存储
  - [x] SubTask 2.1: 在 `InitFSRResources` 与 `RecreateFSRFBO` 中将 `g_renderTexture` 与 `g_targetTexture` 的 `glTexImage2D` 调用替换为 `glTexStorage2D`（1 level，相同 internalFormat）
  - [x] SubTask 2.2: 验证 `glTexStorage2D` 调用后 `glTexParameteri`、`glFramebufferTexture2D` 行为不变

- [x] Task 3: 缩减 GLStateGuard 范围
  - [x] SubTask 3.1: 修改 `GLStateGuard` 结构，仅保存/恢复 FSR pass 实际修改的状态：`GL_CURRENT_PROGRAM`、`GL_VERTEX_ARRAY_BINDING`、`GL_ACTIVE_TEXTURE`、`GL_TEXTURE_BINDING_2D`、`GL_READ_FRAMEBUFFER_BINDING`、`GL_DRAW_FRAMEBUFFER_BINDING`
  - [x] SubTask 3.2: 移除对 `GL_ARRAY_BUFFER_BINDING`、`GL_RENDERBUFFER_BINDING` 的保存/恢复（FSR pass 不修改这些状态）

- [x] Task 4: 移除目标 FBO 的冗余 clear
  - [x] SubTask 4.1: 在 `ApplyFSR` 中删除 `GLES.glClearColor(...)` 与 `GLES.glClear(GL_COLOR_BUFFER_BIT)` 调用
  - [x] SubTask 4.2: 确认 FSR fragment shader 对每个目标像素都写入颜色，无需 clear

- [x] Task 5: 实现 FSR uniform 缓存
  - [x] SubTask 5.1: 在 `FSR1_Context` 中新增 `g_uniformsDirty` 标志和上次上传的 const0/viewportSize 缓存
  - [x] SubTask 5.2: 在 `ApplyFSR` 中比较当前值与缓存值，相同则跳过 `glUniform4fv` / `glUniform2fv`；不同则上传并更新缓存
  - [x] SubTask 5.3: 在 `RecreateFSRFBO` 末尾将 `g_uniformsDirty` 置为 true，强制下一帧重新上传
  - [x] SubTask 5.4: 在 `CompileFSRShader` 末尾将 `g_uniformsDirty` 置为 true（程序刚链接，uniform 值需重新上传）

- [x] Task 6: 缓存 viewport 状态
  - [x] SubTask 6.1: 在 `FSR1_Context` 中新增 `g_lastFsrViewportW`、`g_lastFsrViewportH`、`g_viewportCacheValid`，初始为 0/false
  - [x] SubTask 6.2: 在 `ApplyFSR` 中分别对 target viewport 和 render viewport 与缓存比较，相同则跳过 `glViewport`
  - [x] SubTask 6.3: 在 `RecreateFSRFBO` 与 `CheckResolutionChange` 末尾重置 viewport 缓存以强制下次设置

- [x] Task 7: 显式管理 FSR pass 的深度/模板/混合状态
  - [x] SubTask 7.1: 扩展 `GLStateGuard`，保存 `GL_DEPTH_TEST`、`GL_STENCIL_TEST`、`GL_BLEND` 的启用状态以及 color mask
  - [x] SubTask 7.2: 在 `ApplyFSR` 进入 FSR pass 时 `glDisable(GL_DEPTH_TEST)`、`glDisable(GL_STENCIL_TEST)`、`glDisable(GL_BLEND)`、`glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE)`
  - [x] SubTask 7.3: 在 `GLStateGuard` 析构时恢复上述状态

- [x] Task 8: 调用 glInvalidateFramebuffer 优化 tile-based GPU
  - [x] SubTask 8.1: 在 `ApplyFSR` 末尾（恢复 renderFBO 之后），对 `g_renderFBO` 调用 `glInvalidateFramebuffer(GL_FRAMEBUFFER, ...)`，附件列表包含 `GL_DEPTH_ATTACHMENT` 和 `GL_STENCIL_ATTACHMENT`
  - [x] SubTask 8.2: 确认 `glInvalidateFramebuffer` 调用前 `g_renderFBO` 已绑定，且函数指针在 GLES 中可用（GLES3.0+，已确认在 gles.h 中声明）
  - [x] SubTask 8.3: 在 FSR program 链接后对 `g_targetFBO` 的 color attachment 不做 invalidate（下一帧 blit 需要）

- [x] Task 9: 调整 g_renderTexture 过滤模式
  - [x] SubTask 9.1: 在 `InitFSRResources` 与 `RecreateFSRFBO` 中将 `g_renderTexture` 的 `GL_TEXTURE_MIN_FILTER` 和 `GL_TEXTURE_MAG_FILTER` 设置为 `GL_NEAREST`
  - [x] SubTask 9.2: 保持 `g_targetTexture` 仍为 `GL_LINEAR`（`glBlitFramebuffer` 使用）
  - [x] SubTask 9.3: 验证 EASU 通过 `textureGather` 取样不依赖过滤模式（已确认 FsrEasuRF/GF/BF 用 textureGather，FsrRcasLoadF 用 texelFetch，均不受过滤模式影响）

- [x] Task 10: 编译验证与回归
  - [x] SubTask 10.1: 在 `/workspace/MobileGlues-cpp/gl/FSR1` 下执行 `g++ -std=c++20 -fsyntax-only` 对 FSR1.cpp 进行语法检查（3rdparty 完整 CMake 构建需要外部 submodule，沙箱内不可用；语法检查通过，仅余预先存在的 typedef 警告）
  - [x] SubTask 10.2: 人工 review 改动确保 `eglSwapBuffers` → `ApplyFSR` → `CheckResolutionChange` 的调用流程未发生行为变化

# Task Dependencies
- Task 2 依赖 Task 1（不可变存储应在格式确定后再切换，避免重复改）
- Task 5、Task 6 可与 Task 3、Task 4 并行
- Task 7、Task 8、Task 9 互相独立，可并行
- Task 10 依赖所有其他 Task 完成
