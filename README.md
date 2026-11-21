# occt-lab

occt-lab 是一个用 C++23 编写的交互式几何实验工作台，用于学习计算机辅助几何设计（CAGD）。在三维视口中移动控制点，可以观察曲线和曲面如何变形，也可以比较 CPU 与 GPU 对同一曲面的求值结果。

## 主要功能

- 编辑 B 样条曲线的控制点和权重，比较插值与逼近，观察插入节点和升阶后的控制多边形。
- 创建和编辑双三次 Bézier 曲面片，查看控制网、等参数线和法向，将 GPU 采样得到的位置和法向与 OCCT 的结果逐点对照。
- 构造方块和圆柱等 B-Rep 实体，将其三角化并在视口中显示。

## 技术栈

- **几何计算** — Open CASCADE Technology（OCCT）
- **渲染与交互** — Vulkan、Vulkan-Hpp、Slang、SDL3、Dear ImGui
- **构建与测试** — CMake、Ninja、vcpkg、CTest

项目采用 [MIT License](LICENSE)。
