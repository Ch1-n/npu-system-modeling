# NPU 系统建模与联合仿真

本仓库配套技术系列《从零构建周期级 NPU 系统：gem5 + SystemC 联合仿真实战》，记录一个 NPU 周期模型从仿真底座、CPU 接口、存储系统和多 Engine 调度，逐步发展到 Runtime、算子执行与性能分析的过程。

仓库中的公开实现采用教学参数和独立示例。核心计算 Engine、产品 ISA、专用性能参数以及内部测试资产不在公开范围内。通用构建框架、时间协调方法、请求完成机制、存储抽象、同步、Runtime DAG 和验证方法会随文章逐步整理。

## 文章

- [系列提纲](SERIES_OUTLINE.md)
- [第一章 从零搭建 gem5 与 SystemC 联合仿真环境](docs/chapter-01-gem5-systemc-foundation.md)

## 第一章示例

```text
examples/chapter-01/
├── systemc-hello/       # 独立 SystemC 时钟与进程
└── gem5-systemc-host/   # 外部 SystemC kernel 承载 libgem5
```

第一章使用以下目标依赖版本，实际验证范围见[验证记录](docs/VALIDATION.md)：

- gem5 `v25.1.0.1`
- Accellera SystemC `2.3.4`
- CMake `3.20+`
- Python `3.12` 或 `3.13`
- SCons `4.10.1`

构建步骤见第一章正文和各示例目录中的说明。

目前运行验证基于 macOS arm64 的已有 gem5 构建，不代表已完成官方干净源码或 Linux 的端到端构建验证。

## 仓库状态

当前发布内容对应第一章。后续章节会以独立目录和 Git Tag 增量发布，计划使用 `chapter-01` 至 `chapter-06` 标记可复现节点。

## 许可证

本仓库原创代码和文档采用 MIT License。gem5、SystemC、Ramulator2 等第三方项目仍遵循各自许可证；本仓库不重新分发这些项目的源码。
