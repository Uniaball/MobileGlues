# 优化 FSR1 性能 Spec

## Why
当前 FSR1 实现在移动 GPU 上存在多个性能瓶颈：`RecreateFSRFBO` 错误地使用了 `GL_RGBA32F`（128 位/像素）渲染纹理（初始化时用的是 `GL_RGBA8`），每帧通过 `GLStateGuard` 执行大量 `glGetIntegerv` 状态查询与状态恢复，每帧无条件 `glClear` 目标 FBO，每帧重复上传未变更的 uniform，且未利用 `glInvalidateFramebuffer` 帮助 tile-based GPU 跳过不必要的 tile resolve。此外当前单 pass 把 EASU 与 RCAS 拼接在一起，RCAS 直接从低分辨率输入纹理取 5 tap，既不正确也额外消耗带宽。本改动旨在以最小风险显著降低 FSR 每帧的 CPU/GPU 开销。

## What Changes
- 修复 `RecreateFSRFBO` 中渲染纹理格式由 `GL_RGBA32F` 改回 `GL_RGBA8`，与 `InitFSRResources` 一致
- 移除 `ApplyFSR` 中对目标 FBO 的 `glClear`（FSR shader 写入每个像素，clear 浪费带宽）
- 缓存 FSR uniform（`uConst0`、`uViewportSize`、`uInputTex`），仅在值变化或程序重建时上传
- 缓存 viewport 状态，仅在尺寸变化时调用 `glViewport`
- 缩减 `GLStateGuard` 范围：仅保存/恢复 FSR pass 实际修改的状态（program/VAO/active texture/texture/FBO），去掉 `GL_ARRAY_BUFFER_BINDING`、`GL_RENDERBUFFER_BINDING` 等未修改项
- 在 `ApplyFSR` 末尾对 `g_renderFBO` 的 depth/stencil 附件调用 `glInvalidateFramebuffer`，让 tile-based GPU 跳过 resolve
- 在 `InitFSRResources`/`RecreateFSRFBO` 中使用 `glTextureStorage2D`（DSA 形式 `glTextureStorage2D` 或 `glTexStorage2D`）替代 `glTexImage2D`，以获得不可变存储的驱动优化
- 在 `ApplyFSR` 进入 FSR pass 时显式 `glDisable(GL_DEPTH_TEST)`、`glDisable(GL_STENCIL_TEST)`、`glDisable(GL_BLEND)`、`glColorMask(GL_TRUE,...)`，并在末尾恢复
- 将 `g_renderTexture` 的过滤模式由 `GL_LINEAR` 改为 `GL_NEAREST`（EASU 通过 `textureGather` 显式取 4 tap，过滤模式不影响结果，可省去潜在 filtering 开销）
- 保留 `g_targetTexture` 的 `GL_LINEAR`，因为最终 `glBlitFramebuffer` 用到该过滤模式
- 维持现有单 pass EASU+RCAS 结构（本次不拆分为两个 pass，避免引入额外 FBO 和带宽，留作后续优化）

## Impact
- Affected specs: FSR1 上分子系统
- Affected code:
  - [FSR1.cpp](file:///workspace/MobileGlues-cpp/gl/FSR1/FSR1.cpp)
  - [FSR1.h](file:///workspace/MobileGlues-cpp/gl/FSR1/FSR1.h)
- 受影响的外部调用点：
  - [egl.cpp](file:///workspace/MobileGlues-cpp/egl/egl.cpp) 中 `eglSwapBuffers` 调用 `ApplyFSR` / `CheckResolutionChange` 的行为不变
  - [shader.cpp](file:///workspace/MobileGlues-cpp/gl/shader.cpp) 中 `glCreateShader` 触发 `InitFSRResources` 的行为不变

## ADDED Requirements

### Requirement: FSR Uniform 缓存
系统 SHALL 在 FSR uniform 值未发生变化时跳过 `glUniform*` 调用，仅在程序重建或值变化时重新上传。

#### Scenario: 首帧上传
- **WHEN** FSR 首次执行 `ApplyFSR`
- **THEN** 所有 uniform 上传一次，并记录已上传的值

#### Scenario: 值未变化
- **WHEN** 后续帧的 `g_renderWidth`、`g_renderHeight`、`g_targetWidth`、`g_targetHeight` 均未变化
- **THEN** 跳过所有 `glUniform*` 调用

#### Scenario: 分辨率变化
- **WHEN** `g_resolutionChanged` 触发 `RecreateFSRFBO` 后下一帧 `ApplyFSR`
- **THEN** uniform 缓存被标记为脏，重新上传新值

## MODIFIED Requirements

### Requirement: FSR 每帧 CPU/GPU 开销
FSR pass 在稳定状态下（分辨率未变化）每帧 SHALL 满足：
- 不调用 `glGetIntegerv` 查询未被 FSR pass 修改的状态
- 不调用 `glClear` 清空目标 FBO
- 不重复上传未变更的 uniform
- 不重复设置未变更的 viewport
- 通过 `glInvalidateFramebuffer` 通知 GPU 渲染 FBO 的 depth/stencil 不需要保留

#### Scenario: 稳定状态 ApplyFSR
- **WHEN** 连续多帧分辨率未变化
- **THEN** `ApplyFSR` 不执行 `glClear`、不执行冗余 `glUniform*`、不执行冗余 `glViewport`

### Requirement: FSR 渲染纹理格式
`RecreateFSRFBO` 创建的 `g_renderTexture` SHALL 使用 `GL_RGBA8` 内部格式，与 `InitFSRResources` 保持一致。

#### Scenario: 分辨率变化触发重建
- **WHEN** `CheckResolutionChange` 调用 `RecreateFSRFBO`
- **THEN** 新的 `g_renderTexture` 使用 `GL_RGBA8`（32 位/像素），不再使用 `GL_RGBA32F`

### Requirement: FSR Pass 期间 GL 状态
FSR pass SHALL 显式禁用深度/模板/混合测试，并在 pass 结束后恢复调用方状态，避免依赖外部状态。

#### Scenario: 进入 FSR pass
- **WHEN** `ApplyFSR` 开始执行
- **THEN** `GL_DEPTH_TEST`、`GL_STENCIL_TEST`、`GL_BLEND` 被禁用，color mask 全开

#### Scenario: 退出 FSR pass
- **WHEN** `ApplyFSR` 执行完毕
- **THEN** 上述状态恢复为进入前的值

## REMOVED Requirements
无。
