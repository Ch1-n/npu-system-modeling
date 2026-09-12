# NPU 系统建模与联合仿真系列提纲

## 系列定位

本系列围绕一个真实 NPU 周期级仿真项目展开，兼顾三个目标：

1. 给读者提供一条可以动手复现的 gem5 + SystemC 联合仿真路线。
2. 梳理 NPU 系统建模过程中形成的架构、接口、调度、存储和验证方法。
3. 用公开代码、架构图、测试结果和性能分析完整呈现系统级 NPU 工程能力。

公开仓库可以复用经过清理的通用基础设施，包括构建框架、日志与 trace、TLM 辅助类、gem5/SystemC 时间协调、请求完成机制、共享存储、外存抽象、同步、Runtime DAG 和 profiling。核心 Tensor、Vector/ACVT 等计算 Engine 使用独立教学实现替代；产品 ISA、私有 descriptor 字段、专用参数和内部测试资产不公开。

每篇文章固定回答五个问题：遇到了什么工程问题，为什么选择当前架构，关键模块如何实现，怎样验证结果，以及模型结论适用于什么范围。每章对应一个可运行目录和 Git Tag。

## 第一章 从零搭建 gem5 与 SystemC 联合仿真环境

### 目标

建立联合仿真的最小底座，让 RISC-V Guest 在 gem5 中执行，同时由外部 SystemC kernel 统一推进仿真时间。

### 主要内容

- 功能模型、周期模型与 RTL 仿真的分工。
- gem5、SystemC、Host、Guest 和 Runtime 的职责边界。
- 为什么采用单进程 `libgem5` 嵌入方式。
- gem5 event queue 与 SystemC 时间轴如何协调。
- 从 `sc_main`、对象实例化、ELF 装载到退出的生命周期。
- gem5 可执行程序与 `libgem5` 为什么使用独立构建目录。
- macOS 和 Linux 构建差异、配置文件绝对路径和时间精度等常见问题。
- 公开仓库结构、模型边界和后续扩展接口。

### 代码里程碑

- 独立 SystemC 时钟示例运行通过。
- gem5 RISC-V Hello World 运行通过。
- 外部 SystemC kernel 成功承载 `libgem5`，Guest 正常退出。

### 展示的工程能力

仿真框架选型、CMake/SCons 构建、gem5 C++ 嵌入、SystemC 生命周期和跨平台问题定位。

## 第二章 一条 NPU 命令如何从 RISC-V CPU 走到 SystemC

### 目标

建立 CPU 到 NPU 的完整控制通路，区分命令接受、执行完成、结果消费和 CPU 退休。

### 主要内容

- MMIO、自定义指令和协处理器接口的取舍。
- gem5 指令译码、请求封装和 SystemC Bridge。
- 标量命令、矢量上下文和普通 RVV/VLM 访存的区别。
- descriptor、标量操作数、请求 ID 和返回状态。
- 异步提交、poll、wait、hart suspend/wakeup。
- 有限队列、retry、CDC 延迟、错误传播和退出 drain。

### 代码里程碑

RISC-V Guest 使用教学自定义指令提交 Echo/Add 命令；SystemC 设备延迟若干周期后返回结果并唤醒 CPU。

### 展示的工程能力

ISA 扩展、gem5 MinorCPU 修改、软硬件协议、事件队列和跨时钟域抽象。

## 第三章 NPU 的数据从哪里来

### 目标

建立从 Guest 地址空间到片上 Shared Memory 和外部 HBM 的完整数据通路。

### 主要内容

- Guest 虚拟地址、gem5 物理地址、NPU 逻辑地址和 SMEM 本地地址。
- 功能访问与 timing 访问的区别。
- CPU 和 NPU 如何共享 backing store。
- 多 bank Shared Memory、端口仲裁、bank conflict 和统计。
- DMA 请求拆分、在途事务、反压和完成可见性。
- 从固定延迟内存替换到 Ramulator2。
- HBM transaction、tCK、排队、带宽和跨时钟域离散化。
- cache coherence 的实现边界。

### 代码里程碑

完成 `HBM -> DMA -> SMEM -> DMA -> HBM` Copy，覆盖非对齐、越界、retry 和背压测试。

### 展示的工程能力

存储层次、DMA、TLM、HBM 时序、地址转换和一致性边界分析。

## 第四章 从一个计算单元到多 Engine 流水

### 目标

接入教学计算 Engine，并让 DMA、计算和激活单元通过硬件事件形成并行流水。

### 主要内容

- 简化 Vector Add、MAC 或小矩阵乘 Engine。
- CMD Processor、共享 Command Buffer 和 Engine instruction buffer。
- 同一 Engine 保序与不同 Engine 并行。
- Event、Require、Enable 和 completion 的职责。
- 计算完成与结果可见之间的差异。
- SYNC 如何形成内存可见性屏障。
- 双缓冲、队列深度、流水重叠、反压和死锁风险。

### 代码里程碑

`DMA input -> Compute -> Activation -> DMA output` 在 CPU 不逐条等待的情况下自动完成。

### 展示的工程能力

NPU 控制面、多 Engine 调度、同步协议、周期级流水和性能意识。

## 第五章 从手写指令到 NPU Runtime

### 目标

把底层命令接口提升为可运行算子图的软件栈。

### 主要内容

- 命令描述符、buffer 分配、地址重定位和任务句柄。
- 阻塞 API 与异步 API。
- 乱序 completion 缓存、失败传播和资源回收。
- 用 DAG 表示 fan-in、fan-out 和算子依赖。
- ready queue、active queue 与多任务并发。
- 哪些依赖由 CPU 调度，哪些依赖下沉到硬件 SYNC。
- Runtime DAG 到 CPU transport/Guest 执行计划的转换。
- 从手写算子扩展到 ONNX 前端的路径和边界。

### 代码里程碑

运行简化的 `MatMul -> Activation -> Add` 或小型 MLP 图，并观测多个命令同时在途。

### 展示的工程能力

Runtime、图调度、编译 lowering、软硬件协同和端到端系统集成。

## 第六章 如何证明 NPU 周期模型可信

### 目标

建立分层验证和性能分析方法，用一个完整算子定位系统瓶颈。

### 主要内容

- 单元测试、协议测试、集成测试和端到端测试。
- 与软件 golden model 做数值比较。
- 请求守恒、重复 completion、非法 handle 和退出 drain。
- 随机反压、队列耗尽、reset/cancel、超时和死锁检测。
- 周期、队列占用、SMEM 冲突、DMA 带宽和 Engine 利用率。
- 扫描计算能力、DMA 并发度、SMEM bank 和 HBM 带宽。
- 分析增加计算资源后性能不再提升的原因。
- 区分功能正确、模型时序正确和真实芯片性能。

### 代码里程碑

自动输出 CSV/JSON 性能报告，对一个算子完成参数扫描和瓶颈定位。

### 展示的工程能力

验证策略、性能建模、实验设计、数据分析和架构取舍。

## 发布与维护约定

- 文章正文保存为 Markdown，知乎发布版保存为 DOCX。
- 架构图同时保存 `.drawio` 和 PNG，正文使用相对路径引用 PNG。
- 每章代码能够独立构建，README 写明依赖版本和验证范围。
- 每章完成后创建对应 Git Tag，并记录本机验证环境。
- 验证记录区分已有依赖复跑、示例重编译和官方干净环境完整构建；已发布 Tag 不静默移动。
- common 模块可从原工程清理提取，但核心计算 Engine 不公开；每章区分原工程经验、公开实现与尚未验证的计划。
- 公开数据全部由程序生成，不提交内部权重、算子包或性能目标。
- 性能结论必须注明时钟、数据类型、shape、缓存配置和模型近似。
