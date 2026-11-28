# 构建与工具链配置

[首页](../README.md#4-构建与运行)给出了最短构建命令。仓库共享预设 `release` 使用 **CMake 4.5+**、**Ninja**、**`clang-cl`** 与自定义 triplet **`x64-windows-clang-release`**，默认输出到 `build-release`。[CMake 预设](../CMakePresets.json) · [triplet](../triplets/x64-windows-clang-release.cmake) · [依赖清单](../vcpkg.json)

## 1. 用 `CMakeUserPresets.json` 收口本机路径与缓存

共享预设从环境变量 `VCPKG_ROOT` 查找 `scripts/buildsystems/vcpkg.cmake`。在仓库根目录创建被 [.gitignore](../.gitignore) 忽略的 `CMakeUserPresets.json`，可以把本机 vcpkg 路径与缓存全部收口，不污染系统环境变量和 `%LOCALAPPDATA%\vcpkg`：

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "release-local",
      "inherits": "release",
      "displayName": "Release with local vcpkg (clang-cl x64)",
      "binaryDir": "${sourceDir}/build-release",
      "environment": {
        "VCPKG_ROOT": "D:/vcpkg",
        "VCPKG_DOWNLOADS": "$env{VCPKG_ROOT}/downloads",
        "VCPKG_DEFAULT_BINARY_CACHE": "$env{VCPKG_ROOT}/archives",
        "X_VCPKG_REGISTRIES_CACHE": "$env{VCPKG_ROOT}/registries",
        "VCPKG_DISABLE_METRICS": "1"
      },
      "cacheVariables": {
        "VCPKG_INSTALL_OPTIONS": "--x-buildtrees-root=${sourceDir}/build-release/vcpkg_installed/vcpkg/blds;--x-packages-root=${sourceDir}/build-release/vcpkg_installed/vcpkg/pkgs"
      }
    }
  ],
  "buildPresets": [
    { "name": "release-local", "inherits": "release", "configurePreset": "release-local" }
  ],
  "testPresets": [
    { "name": "release-local", "inherits": "release", "configurePreset": "release-local" }
  ]
}
```

配置好后，configure / build / test 均可改用 `--preset release-local`：
- **缓存分工**：`D:\vcpkg` 下的 `downloads`、`archives`、`registries` 可跨项目复用（首次使用前先建好 `archives` 与 `registries` 目录）；单项目的依赖安装树、构建中间目录 `blds` 和打包暂存 `pkgs` 全部留在 `build-release\vcpkg_installed` 内。
- **与 clangd 对齐**：`release-local` 显式保留 `binaryDir` 为 `build-release`，与 [.clangd](../.clangd) 的编译数据库路径保持一致。

## 2. OCCT 裁剪与工具链细节

为缩短编译时间并去掉用不到的依赖，triplet 与 manifest 对 Open CASCADE 8.0.1 做了定向裁剪：
- **跳过 OpenGL 模块**：渲染由 Vulkan 承担，在 [triplet](../triplets/x64-windows-clang-release.cmake) 中传入 `-DUSE_OPENGL=OFF` 跳过 `TKOpenGl`。
- **只保留 STEP 数据交换**：传入 `-DBUILD_MODULE_DataExchange=OFF` 与 `-DBUILD_ADDITIONAL_TOOLKITS=TKDESTEP`，只构建 STEP 及其必需的依赖工具包。
- **关闭默认 FreeType**：[vcpkg.json](../vcpkg.json) 对 `opencascade` 设 `default-features: false`，界面文字由 Dear ImGui 与随附的 Noto Sans SC 字体绘制。
- **可选 Vulkan 校验层**：`vcpkg.json` 另声明了 `validation-layers` feature（`vulkan-validationlayers`），默认 Release 配置不启用，本地调试时按需开启。

## 3. C++23 模块与 Slang 着色器构建

- **`import std;` 与 `import vulkan;`**：[CMakeLists.txt](../CMakeLists.txt) 启用 `CMAKE_CXX_SCAN_FOR_MODULES ON` 与 `cxx_std_23`；`import std;` 依赖 MSVC 工具集里的 C++23 标准库模块文件，`import vulkan;` 通过 `FILE_SET vulkan_hpp TYPE CXX_MODULES` 直接引入 `${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm`（并定义 `VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL=0`）。
- **Slang 自动编译**：`shader-slang` 作为 vcpkg 的 `host` 依赖提供 `slangc`。CMake 会在构建 `formlight-geometry` 前通过 `formlight_geometry_shaders` 目标自动将 `shaders/` 下的 5 个 `.slang` 文件编译为 SPIR-V（位于 `build-release/shaders/`）：
  - 图形着色器：`triangle.slang`、`brep_preview.slang`（顶点 + 片元）
  - 计算着色器：`buffer_check.slang`、`compute_pyramid.slang`、`nurbs_grid.slang`

## 4. CTest 回归测试一览

运行 `ctest --preset release-local`（或 `release`）会执行 6 组独立几何与文档回归测试（不依赖图形窗口）：

| 测试目标 | 覆盖范围 |
| --- | --- |
| `occt_box_check` | OCCT 方块构造、B-Rep 三角化与面属性 |
| `occt_cylinder_check` | 圆柱实体构造与三角化 |
| `occt_plate_with_hole_check` | 带孔板两种建模方式、STEP 原子导出重读、有效性/体积/孔壁面积核对 |
| `bezier_patch_check` | Bernstein 解析参考与 OCCT Bézier 曲面求值对照 |
| `bspline_curve_check` | B 样条曲线创建、插值/逼近拟合、控制点编辑、节点插入/升阶、视口拾取与 `.occtlab` 项目读写 |
| `bspline_surface_check` | NURBS 曲面控制网/自定义节点、插值/逼近、节点插入/升阶、GPU 缓冲打包、解析曲面求值对照与项目读写 |
