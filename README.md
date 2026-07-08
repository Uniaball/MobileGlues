# DesktopGlues

DesktopGlues，取「(on) Desktop, GL uses ES」之意，是基于 MobileGlues(https://github.com/MobileGL-Dev/MobileGlues) 深度优化的桌面端（bushi） GL 实现

<details>
<summary> 目前版本 </summary>
 V1.0.6
</details>

## License

DesktopGlues 延续 MobileGlues 的开源协议，采用 GNU LGPL-2.1 License。

详见 "LICENSE" (https://github.com/MobileGL-Dev/MobileGlues/blob/main/LICENSE)

## Build Optimizations

DesktopGlues 在编译时支持两组可选的性能优化开关，通过环境变量控制：

- **`USE_FULL_LTO`**：启用完整的链接时优化（Full LTO，`-flto`），可显著减小体积并提升运行时性能。默认 `ON`。
- **`USE_EXTRA_OPTIMIZATIONS`**：启用额外的编译和链接优化，进一步压缩体积和提高执行效率。默认 `ON`。

当 `USE_EXTRA_OPTIMIZATIONS=ON` 时，将应用以下优化（针对 Android ARM64 平台）：

**编译优化**
- `-fomit-frame-pointer`：省略帧指针，释放寄存器
- `-fno-semantic-interposition`：允许更激进的优化
- `-fmerge-all-constants`：合并相同的常量
- `-fno-math-errno`：不设置数学函数的 errno
- `-fno-signed-zeros`：忽略符号零
- `-fno-trapping-math`：不捕获浮点异常
- `-fno-unwind-tables` / `-fno-asynchronous-unwind-tables`：移除 unwind 表，减小体积
- `-fstrict-aliasing`：启用严格别名优化
- `-fno-rtti`：禁用运行时类型识别
- `-march=armv8.2-a+fp16+rcpc+dotprod+crypto`：启用 ARMv8.2 扩展指令集（提升 AI/图形性能）
- `-mtune=cortex-a76`：针对 Cortex-A76 微架构优化
- `-D_NDK_MATH_NO_SOFT=1`：强制使用硬件浮点
- `-ffast-math`：激进的浮点优化（可能轻微影响精度，适合图形计算）

**链接优化**
- `-Wl,--as-needed`：只链接实际使用的库
- `-Wl,--icf=all`：合并相同的代码段
- `-Wl,--hash-style=gnu`：使用 GNU 哈希表，加快符号查找
- `-Wl,-Bsymbolic`：优先使用库内部符号
- `-Wl,--exclude-libs,ALL`：隐藏所有静态库符号，减小导出表

> **注意**：`-march` 和 `-mtune` 标志会牺牲对旧款 ARMv8.0 设备的兼容性。如需兼容更广泛的设备，可在构建时手动移除这些标志。

在 CI（如 GitHub Actions）中，可通过环境变量控制：

```yaml
env:
  USE_FULL_LTO: ON          # 启用 Full LTO
  USE_EXTRA_OPTIMIZATIONS: ON   # 启用额外优化
```

本地构建时，可通过 CMake 参数传递：

```bash
cmake -B build -DUSE_EXTRA_OPTIMIZATIONS=ON
```

默认情况下，这些优化选项均为开启状态，以获取最佳性能和最小体积。

# Third-party components

**SPIRV-Cross** by **KhronosGroup** - [Apache License 2.0](https://github.com/KhronosGroup/SPIRV-Cross/blob/master/LICENSE): [github](https://github.com/KhronosGroup/SPIRV-Cross)

**glslang** by **KhronosGroup** - [Various Licenses](https://github.com/KhronosGroup/glslang/blob/main/LICENSE.txt): [github](https://github.com/KhronosGroup/glslang)

**cJSON** by **DaveGamble** - [MIT License](https://github.com/DaveGamble/cJSON/blob/master/LICENSE): [github](https://github.com/DaveGamble/cJSON)

**OpenGL Mathematics (*GLM*)** by **G-Truc Creation** - [The Happy Bunny License](https://github.com/g-truc/glm/blob/master/copying.txt): [github](https://github.com/g-truc/glm)

**FidelityFX-FSR** by **AMD** - [MIT License](https://github.com/GPUOpen-Effects/FidelityFX-FSR/blob/master/license.txt): [github](https://github.com/GPUOpen-Effects/FidelityFX-FSR) 

**Perfetto** by **Google** - [Apache License 2.0](https://github.com/google/perfetto/blob/main/LICENSE): [github](https://github.com/google/perfetto)

**xxHash** by **Yann Collet** - [BSD 2-Clause License](https://github.com/Cyan4973/xxHash/blob/dev/LICENSE): [github](https://github.com/Cyan4973/xxHash)