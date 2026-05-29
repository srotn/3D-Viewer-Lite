# 高性能 CPU 软渲染器 —— 3D Viewer Lite



## 1\. 项目目标

本项目旨在挑战**纯 CPU 渲染的性能极限**。在不依赖 GPU 硬件光栅化的前提下，完全由 CPU 完成 3D 模型的顶点变换、光照计算、光栅化与深度测试，并达到实时交互帧率。

同时，项目集成 **Direct2D / Direct3D 11 互操作**与 **ImGui**，构建一个具有完整 GUI 控制面板的轻量级 3D 模型浏览器，可作为计算机图形学课程的综合性大作业。

&#x20;🎯 核心指标： 在 2K 分辨率下，对 80 万面的模型维持 120 FPS 以上的稳定渲染。



## 2\. 开发环境与依赖

### 2.1 语言与工具

* **编程语言：** C++ (基于 **C++17** 标准)
* **集成开发环境 (IDE)：** Visual Studio 2022
* **构建系统：** MSBuild (Visual Studio 原生)

### 2.2 外部依赖

|依赖库|用途|
|-|-|
|**Direct2D** (d2d1.h)|CPU 帧缓冲（Frame Buffer）到屏幕的高效上传与显示|
|**Direct3D 11** (d3d11.h)|提供 DXGI 交换链及与 Direct2D / ImGui 的互操作支持|
|**ImGui**|黑灰风格图形用户界面（用于调试面板、光照参数调节等）|
|**OpenMP** (omp.h)|多线程并行加速，关键用于几何变换、分箱（Binning）与光栅化|
|**Windows API**|窗口创建、文件对话框唤起、高精度性能计时|

> 💡 注意： 所有系统库均为 Windows SDK 自带，无需额外安装第三方依赖（ImGui 源码已直接包含在项目目录中）。



## 3\. 开发思路与核心亮点

### 3.1 纯 CPU 渲染管线

传统实时渲染完全依赖 GPU 硬件加速，本项目则在 CPU 端完整构建了从模型顶点到屏幕像素的完整渲染管线：



\[模型加载 (.obj)]
↓
\[顶点变换 (矩阵运算)]
↓
\[背面剔除 (Culling)]
↓
\[光照计算 (Lambert)]
↓
\[光栅化与深度测试 (Z-Buffer)]
↓
\[帧缓冲上传 (D2D Bitmap)] → \[屏幕显示]



* **模型加载：** 高效解析 Wavefront .obj 文件，兼容三角形与四边形面片。
* **顶点变换：** CPU 端矩阵运算，实现模型的欧拉角旋转、平移、缩放及透视投影。
* **背面剔除：** 基于屏幕空间的顺时针/逆时针（Winding Order）判定，提前过滤背向面。
* **光照计算：** 实时三光源 Lambert 漫反射模型，法线向量随旋转矩阵实时变换。
* **光栅化：** 采用经典的重心坐标法（Barycentric Coordinates）进行三角形填充，并结合逐像素深度测试（Z-buffer）。
* **帧缓冲：** 所有绘制操作直接写入 CPU 内存数组，最终通过 D2D 位图上传至 GPU 显存显示。



### 3.2 多线程二维无锁分箱架构（⭐ 核心亮点）

为了榨取多核 CPU 的并行潜力，项目设计了一种**二维无锁分箱（Binning）策略**。它将渲染任务分解为“几何处理”与“屏幕区域光栅化”两个维度，通过**私有数据副本**彻底消除了多线程间的数据竞争。

#### 核心流程

> 1. 屏幕切片： 将屏幕垂直划分为 N 个条带（Strips），其中 N = \\\\\\\\text{CPU 线程数}。
> 2. 任务投递： 声明任务箱 localBoxes\\\\\\\[几何线程 ID]\\\\\\\[条带 ID]。每个几何线程将处理后的三角形，投递到对应其所跨越条带的私有箱中。
> 3. 并行光栅化： 每个光栅化线程只负责处理自己条带内的所有三角形（这些三角形来自各个几何线程的私有箱），从而实现无锁写入。



关键代码实现 (Engine3D::DrawMesh3D)

cpp
// 二维任务箱：每个几何线程独立写，每个光栅化线程独立读
std::vector<std::vector<std::vector<PreparedTriangle>>> localBoxes(
num\_threads, std::vector<std::vector<PreparedTriangle>>(num\_threads));

\#pragma omp parallel for
for (int i = 0; i < (int)meshInput.tris.size(); i++) {
int tid = omp\_get\_thread\_num();

&#x20;   // 1. 顶点变换、背面剔除、光照计算...

&#x20;   // 2. 根据三角形垂直跨度，定位条带范围并投递
    int start\\\\\\\_t = std::max(0, triMinY / strip\\\\\\\_height);
    int end\\\\\\\_t = std::min(num\\\\\\\_threads - 1, triMaxY / strip\\\\\\\_height);
    for (int t = start\\\\\\\_t; t <= end\\\\\\\_t; t++) {
        localBoxes\\\\\\\[tid]\\\\\\\[t].push\\\\\\\_back(pt); // 无锁写入私有箱
    }


}

// 3. 光栅化阶段：每个条带独立处理
#pragma omp parallel for schedule(dynamic, 1)
for (int t = 0; t < num\_threads; t++) {
// 收集所有几何线程投递到当前条带 t 的三角形
for (int tid = 0; tid < num\_threads; tid++) {
for (auto\& pt : localBoxes\[tid]\[t]) {
Fill(pt.tri, 128, pt.color, minClipY, maxClipY);
}
}
}



#### 架构优势

* **完全并行与无锁：** 几何处理与光栅化阶段深度并行，移除了所有互斥锁（Mutex），使内存吞吐量达到最大化。
* **天然规避缓存冲突：** 按屏幕 Y 轴横向分割，不同的光栅化线程永远在写不同的像素行，完美避免了多核 CPU 的缓存伪共享（False Sharing）。
* **线性扩展：** 在 16 核及以上 CPU 上，光栅化阶段的吞吐量几乎呈现高效的线性扩展。



### 3.3 D2D + DX11 + ImGui 混合渲染架构

本项目巧妙地将三种渲染技术融合在同一个窗口生命周期内，兼顾了软渲染的灵活性与现代 UI 的美观度：



\[CPU 软渲染绘制] 写入 m\_frameBuffer (内存数组)
↓
\[Direct2D 核心] 通过 CopyFromMemory 上传至 GPU 纹理并绘制
↓
\[ImGui 调试层] 通过 DX11 Render Target 直接叠加至后缓冲区
↓
\[DXGI 交换链] Present(1, 0) 统一垂直同步刷新到屏幕



#### 核心桥梁代码

cpp
void Engine3D::EndDraw() {
if (m\_pBackBufferBitmap) {
// CPU 帧缓冲 -> GPU 纹理
m\_pBackBufferBitmap->CopyFromMemory(nullptr, m\_frameBuffer.data(),
m\_width \* sizeof(uint32\_t));
m\_pRenderTarget->DrawBitmap(m\_pBackBufferBitmap);
}
m\_pRenderTarget->EndDraw();
}



#### WinMain 主消息循环片段

cpp
engine.Render(); // CPU 绘制 + 纹理上传

// 绑定 DX11 渲染目标以供 ImGui 使用
engine.pd3dContext->OMSetRenderTargets(1, \&engine.pmainRenderTargetView, NULL);
ImGui\_ImplDX11\_NewFrame();

// ... 执行 ImGui UI 组件绘制 ...

ImGui::Render();
ImGui\_ImplDX11\_RenderDrawData(ImGui::GetDrawData());

engine.pSwapChain->Present(1, 0); // 统一刷新到屏幕



> 架构评价： 该设计既完整保留了 CPU 软渲染用于教学和算法验证的纯粹性，又充分利用了 GPU 的硬件级纹理映射与高效的 UI 叠加能力。



### 3.4 其他工程优化

* **归一化向量预计算：** 光源方向等全局向量每帧仅在循环外归一化一次，规避了在百万级三角形循环中重复调用高开销的 sqrt。
* **O(1) 条带快速定位：** 借助高效率的整数除法，直接映射出三角形垂直覆盖的条带区间，免去了传统的遍历循环。
* **ProfileTimer 性能探测：** 内嵌高精度计时器，将各个管线阶段的耗时实时输出至 Visual Studio 输出窗口，便于精准调优。



## 4\. 最终效果与功能展示

### 4.1 渲染功能

* **填充+光照模式：** 拥有三个独立方向光（RGB），方向与颜色均可自由调节。采用 Lambert 漫反射模型，配合 Z-buffer 保证深度正确。
* **线框模式：** 基于高效的 **Bresenham 算法** 绘制三角形边框，支持与填充模式独立开关或叠加。
* **实时视角控制：** 支持鼠标右键拖拽进行视角旋转（Yaw / Pitch）、鼠标滚轮平滑缩放，以及键盘动态调节 FOV。
* **模型自由切换：** 系统内置标准测试模型，同时集成了 Windows 文件对话框，支持加载外部任意 OBJ/PLY/STL 模型。



### 4.2 GUI 控制面板

UI 采用现代**黑灰暗色系风格**，多窗口分散布局，最大化节约屏幕可视空间，所有参数修改均可实时生效：

* **右上角固定窗口：** 显眼大字显示当前 **FPS** 及**单帧延迟 (Frame Time)**。
* **右侧中部窗口：** 高亮展示当前载入模型的**顶点数**与**三角面数**。
* **左侧控制主面板：**
* FOV 滑动条 (支持 1° \~ 150° 调节)
* Fill \& Light / Wireframe 渲染模式切换开关
* Load Model... 按钮（唤起系统原生文件选择器）
* Lighting 折叠面板：可分别微调红、绿、蓝三路光源的 3D 方向向量（X/Y/Z 滑动条）与颜色（集成 ColorEdit3 拾色器）





### 4.3 性能表现（实测数据）

* **测试环境：** AMD Ryzen 7945HX, 2K 分辨率 (2560 \* 1440)
* **测试模型：** 80万三角形 (Old Teapot 高阶细分版)
* **实测帧率：** 稳定 **120 FPS**

#### 单帧各阶段耗时深度分析

|管线阶段|耗时 (ms)|占比|
|-|-|-|
|**顶点变换与投影** (并行)|0.4 ms|4.8%|
|**几何处理与分箱** (并行)|1.8 ms|21.7%|
|**工作条带光栅化** (并行)|5.2 ms|62.7%|
|**线框拓扑绘制** (单线程)|0.9 ms|10.8%|
|**总计**|**8.3 ms**|**100%**|

🚀 吞吐量计算：每秒处理三角形数 = 80万 \* 120帧 \~= 9600万 Triangle/s，该数据已跨入纯 CPU 软件渲染器的顶尖梯队。



### 4.4 与工业级工具（MeshLab）的对比

MeshLab 等工业软件完全基于 GPU 硬件光栅化管线，同等模型下可轻松跑出 2000+ FPS 的极致性能。

本项目的核心价值**不在于绝对速度上超越 GPU**，而在于通过精细的多线程架构设计与底层算法优化，证明了 **CPU 同样具备胜任实时 3D 渲染的能力**，并以最直观的代码形式清晰呈现了计算机图形学管线的每一个底层细节。



5\. 项目文件结构
├── ExternalDependence.h   # 外部库引用、核心系统头文件集成
├── Engine3D.h             # 渲染引擎类声明（包含矩阵变换、光照、渲染接口等）
├── Engine3D.cpp           # 渲染引擎核心实现（初始化、软光栅化、Obj模型加载等）
├── Structs.h              # 基础图形学数据结构（向量、三角形、4x4矩阵）
└── 3D Viewer Lite.cpp     # Win32 窗口主程序入口、ImGui 交互界面构建



## 6\. 使用说明

1. 在 Visual Studio 2022 中打开项目解决方案。
2. **开启 OpenMP 支持：** 右键项目属性 \\rightarrow C/C++ \\rightarrow 语言 \\rightarrow 将 OpenMP支持 改为 **是 (/openmp)**。
3. 将配置切换为 **Release / x64**，编译并运行。程序启动后默认自动加载 Old Teapot 模型。
4. **快捷交互快捷键：**
* 鼠标右键拖拽：旋转视图
* 鼠标滚轮：缩放场景
* PageUp / PageDown：快速微调 FOV
* F 键：一键开关光照填充模式
* L 键：一键开关 Bresenham 线框模式
* 通过左侧 ImGui 面板点击 Load Model... 即可载入任意自定义 .obj/.ply/.stl 模型。

