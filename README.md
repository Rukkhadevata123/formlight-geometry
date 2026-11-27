# Formlight Geometry

Formlight Geometry 是一个用 C++23 编写的交互式计算机辅助几何设计（CAGD）工作台。你可以拖动 NURBS 控制点、调整权重，观察曲线和曲面的变化，也可以比较 Open CASCADE Technology（OCCT）在 CPU 上与 Slang 计算着色器在 GPU 上的求值结果。

![青色鞍形曲面及其控制网，右侧展开了 U/V 节点表和曲面探针](docs/images/nurbs-surface.png)

图中是 U、V 均为二次的鞍形曲面。浅蓝色折线连接 3×3 个控制点，青色网格由 GPU 生成。金色和绿色曲线分别是固定 u 和固定 v 的等参线，中央箭头表示探针位置的法向。

## 试一张曲面

1. 在左侧 `Shape` 选择 `NURBS surface`。展开右侧 `GPU surface mesh`，勾选 `Show GPU mesh`。
2. 用左键拖动蓝色控制点，观察曲面如何变形。中键拖动旋转视图，按住 Shift 并用中键拖动可平移，滚轮缩放。
3. 在 `Surface probe` 调整 u、v，读取该位置的坐标和单位法向。按住 Ctrl 单击滑块可输入精确值，按 Enter 确认。
4. 点击 `Compare positions and normals with OCCT`，查看 GPU 与 OCCT 在相同参数点上的位置误差（mm）和法向夹角（度）。

## 创建曲线和曲面

曲线使用一列有序点，曲面使用按 U/V 排列的矩形点网。可以在 XYZ 表格中输入坐标，也可以在 XY 平面上依次点选。

| 创建方式 | 曲线 | 曲面 |
| --- | --- | --- |
| 用控制点定义形状 | `From control points` | `From control net` |
| 插值，经过每个数据点 | `Interpolate through points` | `Interpolate through point grid` |
| 按指定容差逼近数据点 | `Approximate points` | `Approximate point grid` |

曲面也可以从平面或双三次 Bézier 预设开始。曲线和曲面都支持编辑控制点的 XYZ 与权重、单步撤销和重做，以及保存和重开 `.occtlab` 项目。

### 查看拟合误差

- **曲线**的金色拟合曲线和紫色插值曲线使用同一组输入点。预览显示控制点数和最大残差，创建后可以逐点查看弦长参数和对应的残差。
- **曲面**用粉色圆环标出原始数据点。亮粉色大圆环标出偏差最大的点，连线另一端是曲面在对应参数处的位置。调整容差，可以比较控制网尺寸、最大偏差（mm）和该点的 U/V 编号。

### 节点与升阶

自定义直线和控制点曲线可在 `Custom knots` 中设置节点值与重数。自定义曲面控制网可以分别设置 U、V 两个方向的次数和节点。

- **查看节点**　曲线展开 `Knots and multiplicities`。曲面分别展开 `U knots and multiplicities` 和 `V knots and multiplicities`。表中列出节点值、重数和内部节点保证的连续阶数。
- **插入节点**　曲线可以在指定参数处插入节点。曲面在 `Surface probe` 选好参数后，点击 `Insert U knot at u` 增加一列控制点，或点击 `Insert V knot at v` 增加一行。曲面的形状保持不变。
- **升阶**　曲线可直接升阶。曲面的 `Elevate U degree (+1)` 和 `Elevate V degree (+1)` 每次将所选方向的次数提高一级，同时保留曲面形状和原始数据点网。

## 实体与 STEP

OCCT 构造方块、圆柱和带孔板，并将实体三角化后交给 Vulkan 绘制。点击一个面可以高亮它，查看曲面类型和面积。

创建带孔板时，可以用圆柱从板上切出通孔，也可以拉伸带圆孔的平面轮廓。在 `Plate STEP` 面板导出并重读 STEP 文件，可以核对实体的有效性、体积和孔壁面积。

## 构建与测试

Windows x64 的 Release 预设使用 CMake 4.5 或更高版本、Ninja 和 clang-cl。[CMake 预设](CMakePresets.json)

- 项目和 vcpkg 依赖都从 `PATH` 查找 `clang-cl.exe`。使用共享预设 `release` 时，将 `VCPKG_ROOT` 指向已完成 bootstrap 的 vcpkg 根目录。首次配置会按清单锁定的基线安装依赖。[依赖清单](vcpkg.json)
- `import std;` 需要 MSVC 工具集中的 C++23 标准库模块文件。运行程序需要支持 Vulkan 1.3 的驱动。

在 x64 VS 开发者 PowerShell 中进入仓库根目录，运行以下命令。

```powershell
cmake --preset release
cmake --build --preset release
ctest --preset release
.\build-release\formlight-geometry.exe
```

个人机器的 vcpkg 路径和缓存位置可以放在仓库根目录的 `CMakeUserPresets.json`。Git 已忽略这个文件。若在其中定义了继承 `release` 的 `release-local`，并添加同名的构建和测试预设，上面三条 CMake/CTest 命令都可改用 `release-local`。[忽略规则](.gitignore)

以 `D:\vcpkg` 为例，其下的 `downloads`、`archives`、`registries` 分别存放下载文件、二进制包缓存和 vcpkg registry 缓存，可跨项目复用。本机的个人预设显式使用 `build-release`，与 clangd 的编译数据库路径一致。vcpkg 安装的依赖、构建依赖的中间文件和打包目录都在 `build-release\vcpkg_installed` 内。

项目使用 C++23 命名模块。`.cppm` 文件用 `export` 标出对外接口，曲线拟合等较长的实现放在同一模块的独立 `.cpp` 文件中。[曲线模块](src/geometry/curves/bspline_curve.cppm) · [拟合实现](src/geometry/curves/curve_fitting.cpp)

SDL3 处理窗口和输入，Dear ImGui 绘制编辑面板。渲染使用 Vulkan-Hpp、动态渲染和 Synchronization2。CMake 编译 Slang 着色器，vcpkg 管理依赖。[依赖清单](vcpkg.json)

界面的中文和英文共用随程序分发的 Noto Sans SC Regular 2.004。CMake 将这份静态字体（约 7.95 MiB）和许可证复制到可执行文件旁的 `fonts/` 目录。[字体来源](https://github.com/notofonts/noto-cjk/blob/Sans2.004/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf) · [字体许可证（SIL OFL 1.1）](assets/fonts/OFL.txt)

项目代码采用 [MIT License](LICENSE)。
