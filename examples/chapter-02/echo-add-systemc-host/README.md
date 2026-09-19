# 第二章最小 Echo Add 仿真

这个示例把第一章的 gem5 + SystemC Host 扩展成一条最小命令通路：Guest 执行 `custom-0/npu_add`，gem5 指令语义调用 Host 侧适配函数，适配函数把 `7 + 5` 写入 SystemC 设备的深度为 1 的请求 FIFO，等待三个 SystemC 纳秒后取回 `12`，再写回 Guest 的 `a2`。

它验证了完整的 `Guest 指令 -> gem5 -> Bridge -> SystemC Engine -> gem5 -> Guest` 路径，包括自定义指令译码、请求接受、设备延迟、完成结果、结果写回和联合仿真退出。为便于教学，这里的 Bridge 是一个进程内回调加 FIFO；它展示接口和时序，不代表真实 NPU 的最终硬件接口。

在仓库根目录执行：

```sh
./scripts/apply_chapter02_gem5_patch.sh
./scripts/build_gem5.sh
./scripts/build_libgem5.sh
./scripts/prepare_chapter02.sh
cmake -S examples/chapter-02/echo-add-systemc-host \
      -B build/chapter-02/echo-add-systemc-host \
      -DGEM5_ROOT="$PWD/third_party/gem5" \
      -DSYSTEMC_HOME="$SYSTEMC_HOME"
cmake --build build/chapter-02/echo-add-systemc-host -j4
./build/chapter-02/echo-add-systemc-host/echo_add_systemc_host \
  build/chapter-02/config.ini
```

预期输出至少包含：

```text
bridge accept: handle=1
device issue: handle=1
device complete: handle=1 value=12
custom-0 return: handle=1 value=12
bridge result: 12 (expected 12)
PASS echo_add_systemc_host
```
