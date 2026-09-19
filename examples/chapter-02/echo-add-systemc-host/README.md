# 第二章最小 Echo Add 仿真

这个示例把第一章的 gem5 + SystemC Host 扩展成一条最小命令通路：Host 代表 CPU 侧提交 `7 + 5`，SystemC 设备通过一个深度为 1 的请求 FIFO 接收命令，等待三个 SystemC 纳秒后产生结果，Host 再取回 `12`。

它已经验证了 `custom-0/npu_add` 的 gem5 译码、Guest 结果写回、请求接受、设备延迟、完成结果和联合仿真退出。当前 `npu_add` 在 gem5 内立即完成标量加法，SystemC Bridge 请求由 Host 侧示例线程并行提交；下一步再把自定义指令直接连接到 Bridge。

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
bridge result: 12 (expected 12)
PASS echo_add_systemc_host
```
