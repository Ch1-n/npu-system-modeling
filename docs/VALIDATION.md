# 第一章验证记录

核对日期：2026-09-12。本文区分源码核对、已有依赖上的重建与干净环境复现，避免将一次 smoke test 当作完整平台验证。

## 环境与来源

- macOS 26.6.2，arm64；Apple clang 21.0.0；SystemC 2.3.4。
- 本轮重新编译了本仓库两个 C++ 示例。
- gem5 可执行文件与共享库复用本机已有 25.1.0.1 构建，其源码树含本地扩展；没有把该源码树或二进制提交到公开仓库。
- 已另行下载官方 `v25.1.0.1` tarball，核对上游 `sc_module.cc` 调度实现、`CxxConfigManager` 生命周期及 RISC-V SE Process 装载流程。上游 `sc_module.cc` 与本机使用的文件一致。
- 官方下载归档本次 SHA-256：`86bf3123e68c953db3d5358017e1649419a2c02944cf8b4c390911c06aa119b9`。此记录只标识本次归档，不代表整个本机 gem5 构建与其相同。

## 本轮通过项目

| 测试 | 验收结果 |
|---|---|
| 独立 SystemC CTest | 8 个上升沿，停止于 7 ns；断言边沿数与时间 |
| gem5.opt Hello World | Guest 正常退出，tick 为 127806250 |
| Host 正常路径 | 重新编译的 Host 执行 Hello World，退出原因为最后一个线程退出，code=0；退出处两个时间戳一致 |
| Host tick 上限 | 上限为 1 tick 时返回非零，即使 gem5 超时事件 code=0 也不会误报成功 |
| Host 参数检查 | 负数、零上限及缺失配置参数均返回非零 |

自动检查命令（在完成依赖构建与配置生成后，于仓库根目录运行）：

```bash
ctest --test-dir build/systemc-hello --output-on-failure
python3 tests/check_chapter01_host.py \
  build/gem5-systemc-host/gem5_systemc_host \
  build/chapter-01/config.ini
```

Host 检查脚本每个进程设置 60 秒墙钟超时。默认模拟时间上限为 1 ms；两种超时解决不同问题。

## 尚未验证

- 官方干净 gem5 源码经本仓库脚本从零构建 `gem5.opt` 和 `libgem5`，再运行完整示例。
- Linux x86_64/arm64 构建，以及 Python 3.12/3.13、GCC/clang 的兼容矩阵。
- SystemC 不同安装布局。当前 CMake 默认搜索 `SYSTEMC_HOME/lib`；若库位于 `lib64`、`lib-linux64` 或 multiarch 子目录，需显式传入 `-DSYSTEMC_LIBRARY=/absolute/path/to/libsystemc.so`。
- 双向 Bridge 请求、提前唤醒、同一时间戳内的跨框架交互次序、反压和在途请求 drain。
- 同一进程重复构造或销毁 gem5，多个 gem5 主事件队列，以及多线程并行仿真。

上述事项不能从已有 smoke test 的通过结果推导出来。第一章不提供 NPU 功能或性能结论。

## 已知警告与模型边界

当前 DDR3 profile 的设备容量大于配置的 512 MiB 地址窗口，会输出容量警告；这里只将其作为 Hello World 的内存时序后端，不用该配置解释真实 DRAM 几何或带宽。部分 RVV OpClass 缺少 MinorCPU 功能单元、legacy statistics、未安装 pydot 等也会产生警告。Hello World 通过不代表这些功能已经覆盖。

## 版本维护

`chapter-01` 保留首次发布快照，不覆盖已有标签。修订以新的提交演进；复现或反馈时请注明提交号。本页的验证结果只适用于对应提交、依赖来源和测试配置。
