# occt-lab

occt-lab 是一个用 C++23 编写的交互式计算机辅助几何设计（CAGD）工作台。你可以从一组点创建 NURBS 曲线，或从一张控制网创建曲面，拖动控制点、改变权重，观察形状如何变化。曲面既可以用 Open CASCADE Technology（OCCT）在 CPU 上求值，也可以用 Slang 计算着色器在 GPU 上求值，并逐点比较位置和法向。

![NURBS 曲面编辑器中的鞍形曲面、控制网、等参线和法向探针](docs/images/nurbs-surface.png)

图中的青色曲面是 GPU 生成的网格，浅蓝色折线连接控制点，金色和绿色曲线分别对应固定 U 和固定 V 的等参线。

## 主要功能

- 创建**曲线**时，可以在 XYZ 表格中输入坐标，或在 XY 平面上依次点选。选 `From control points` 时，这些点构成控制多边形；选 `Interpolate through points` 时，曲线依次经过它们。自定义直线和控制点曲线都可以勾选 `Custom knots` 编辑节点值与重数，创建后在 `Knots and multiplicities` 查看内部节点保证的连续阶数。还可以按容差逼近数据点、查看残差，以及插入节点或升阶。
- 创建**曲面**时，可以从平面、双三次 Bézier 预设或自定义 U/V 控制网开始。可分别设置两轴的次数和节点，编辑每个控制点的 XYZ 与权重，用探针读取指定参数处的位置和法向。曲线与曲面都支持单步撤销／重做，并可保存为 `.occtlab` 项目文件。
- **实体预览**使用 OCCT 构造方块、圆柱和带孔板，将实体三角化后交给 Vulkan 绘制。点击一张面即可高亮整面，查看曲面类型和面积。
- **带孔板**可以用圆柱从板上切出通孔，也可以从带圆孔的平面轮廓拉伸得到。在 `Plate STEP` 面板导出并重读 STEP 文件，可核对实体的有效性、体积和孔壁面积。

## 试一张曲面

在左侧 `Shape` 中选择 `NURBS surface`，展开右侧 `GPU surface mesh`，勾选 `Show GPU mesh` 即可看到图中的鞍形曲面。用左键拖动一个蓝色控制点，再点 `Compare positions and normals with OCCT`，查看 GPU 与 OCCT 在同一批参数点上的位置误差（mm）和法向夹角（度）。

中键拖动旋转视图，`Shift + 中键` 拖动平移，滚轮缩放。在 `Surface probe` 中可以 `Ctrl + 单击` 参数滑块输入精确的 U、V 值，按 Enter 确认。

## 构建与测试

Windows x64 的 Release 预设使用 Ninja，项目和 vcpkg 依赖都从 PATH 查找 `clang-cl.exe`。请将 `VCPKG_ROOT` 指向 vcpkg 安装目录。运行需要支持 Vulkan 1.3 的驱动。[CMake 预设](CMakePresets.json)

以下命令在仓库根目录的 x64 VS 开发者 PowerShell 中运行。

```powershell
cmake --preset release
cmake --build --preset release
.\build-release\occt_lab.exe
ctest --test-dir build-release --output-on-failure
```

窗口和输入使用 SDL3，编辑面板使用 Dear ImGui；渲染使用 Vulkan-Hpp、动态渲染和 Synchronization2。CMake 负责编译 Slang 着色器，vcpkg 管理依赖。[依赖清单](vcpkg.json)

项目采用 [MIT License](LICENSE)。
