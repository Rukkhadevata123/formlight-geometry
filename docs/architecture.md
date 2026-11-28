# 架构与核心实现

Formlight Geometry 将几何内核、交互文档状态和 Vulkan/ImGui 前端拆分成清晰的三层 C++23 命名模块，让几何算法与拟合逻辑可以在无窗口环境下独立测试。[CMakeLists.txt](../CMakeLists.txt)

## 1. 模块分层

| CMake 目标 | 职责 | 主要源码目录 |
| --- | --- | --- |
| `formlight_geometry_core` | 纯几何内核：B-Rep 实体与三角化、B 样条/Bézier 曲线与曲面求值、节点与权重工具、等参线与采样网格 | `src/geometry/brep/`、`src/geometry/curves/`、`src/geometry/surfaces/` |
| `formlight_geometry_model` | 应用与文档层：可编辑曲线/曲面会话、曲线/曲面插值与逼近、单步撤销历史、`.occtlab` 与 STEP 读写、GPU NURBS 缓冲打包、B-Rep 面拾取与投影变换 | `src/application/`、`src/document/`、`src/render/compute/nurbs_grid_*`、`src/render/viewport/` |
| `formlight-geometry` | 交互工作台前端：Vulkan-Hpp（动态渲染 + Synchronization2）、Slang 计算/预览管线、环绕相机、SDL3 窗口与 Dear ImGui 编辑面板 | `src/vulkan_window.cppm`、`src/render/`、`src/ui/` |

模块书写约定：`.cppm` 文件用 `export` 暴露对外接口，像曲线拟合、曲面插值/逼近这类较长的实现拆到同模块的独立 `.cpp` 单元中（例如 [bspline_curve.cppm](../src/geometry/curves/bspline_curve.cppm) 与 [curve_fitting.cpp](../src/geometry/curves/curve_fitting.cpp)）。

## 2. GPU NURBS 曲面求值与数值稳定性

在工作台中开启 `Show GPU mesh` 或执行 `Compare positions and normals with OCCT` 时，CPU 负责打包参数与节点，GPU 通过计算着色器 [nurbs_grid.slang](../shaders/nurbs_grid.slang) 并行计算网格顶点与单位法向，再与 OCCT 在 CPU（`double` 精度）的求值结果逐点对照。[缓冲打包](../src/render/compute/nurbs_grid_input.cppm) · [计算管线](../src/render/compute/nurbs_grid_mesh.cppm) · [误差报告](../src/render/compute/nurbs_grid_report.cppm)

### 内存布局
CPU 将整张曲面的输入与输出打包进同一块 `RWByteAddressBuffer`（binding 0）：
1. **输出顶点区**：每个采样点占 8 个 `float`（`NurbsGridVertex`：局部坐标 + 有理分母 `W`，以及单位法向 + 状态标志 `W`）；初始化为全 0，用来检测未被着色器写入的异常顶点。
2. **控制点区**：每个控制点存为 `float4(local_xyz, scaled_weight)`，坐标相对 `origin = poles.front()` 偏移、权重按最大权重归一化，减少 FP32 精度损失。
3. **节点与采样表**：U、V 两轴各自展开后的归一化节点向量，紧跟每个网格采样点的 `(parameter, packed_span_and_crease)` 对。

### 为保证 FP32 精度与法向一致性所做的四处设计
1. **CPU 双精度定位 span 与折痕**：在 `pack_nurbs_grid` 中用 `double` 查找每个采样参数所在的节点区间 `span`，同时检测重数等于次数的内部节点并打上 `crease` 标记，打包为 `(span << 1) | crease` 传给 GPU。这样参数舍入到 `float` 时不会意外跨过 $C^0$ 折痕。
2. **每采样点选最大基函数控制点作局部锚点**：着色器用 `strongestBasis` 选出当前贡献最大的控制点作为局部 `anchor`，累加所有偏移量 `(p.xyz - anchor) * (p.w * bu[i] * bv[j])` 后再加回 `anchor`。即使在退化边或权重相差悬殊的区域，也能避免大坐标相减造成有效位丢失。
3. **按局部跨距缩放导数**：`basisAt` 在递推最高阶基函数时对局部跨距坐标求导（乘以当前区间宽度 `width`），保持正向缩放而不必除以极小的节点区间长度。
4. **商法则切向与退化法向保护**：记齐次和为 $(S, W)$，其对局部参数的一阶偏导为 $(S_u, W_u)$ 与 $(S_v, W_v)$，则切向量方向与下式同向（略去正公共分母 $W$）：

   $$
   \mathbf{t}_u \propto S_u - \mathbf{p}\, W_u, \qquad \mathbf{t}_v \propto S_v - \mathbf{p}\, W_v
   $$

   着色器先将 $\mathbf{t}_u$、$\mathbf{t}_v$ 各自按最大分量缩放后单位化，再做叉积。结果 `normal.w` 明确区分三种状态：`1` 为有效单位法向，`0` 为折痕处或两切向近乎平行（叉积模长 $\le 8\varepsilon_{\text{fp32}}$），`-1` 为非有限数值异常。

## 3. B-Rep 三角化、面拾取与 STEP 往返

- **保留拓扑面映射的三角化**（[brep_mesh.cppm](../src/geometry/brep/brep_mesh.cppm)）：`BRepMeshData` 除了保存连续的 `MeshVertex`（局部坐标 + 法向）与 `indices` 供 Vulkan 绘制外，还记录每个 `TopoDS_Face` 对应的 `(first_index, index_count, face)` 区间。拾取射线命中三角形后，可沿索引区间反查回具体的 OCCT 拓扑面，高亮整个面并读取其解析曲面类型与表面积（[面拾取](../src/render/viewport/brep_face_picking.cppm) · [面属性](../src/geometry/brep/brep_face_properties.cppm)）。
- **原子写出与 STEP 核对**（[plate_step.cppm](../src/document/plate_step.cppm)）：导出带孔板时经 `STEPControl_Writer`（`STEPControl_ManifoldSolidBrep`）通过 `io::write_atomically` 原子落盘；重读后用 `BRepCheck_Analyzer` 校验拓扑有效性，并用 `BRepGProp` 核对实体的体积与唯一柱面孔壁的面积。
