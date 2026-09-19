# 第二章最小 Echo Add 仿真

这个示例把第一章的 gem5 + SystemC Host 扩展成一条最小命令通路：Host 代表 CPU 侧提交 `7 + 5`，SystemC 设备通过一个深度为 1 的请求 FIFO 接收命令，等待三个 SystemC 纳秒后产生结果，Host 再取回 `12`。

它已经验证了请求接受、设备延迟、完成结果和联合仿真退出，但还没有把 `custom-0` 自定义指令接入 gem5 的 RISC-V decoder。文章中的 `.insn` 和 decoder 片段仍是下一步 ISA 接入的接口约定，不能把本示例误解为已经完成了自定义指令联调。

在仓库根目录执行：

```sh
./scripts/prepare_chapter01.sh
cmake -S examples/chapter-02/echo-add-systemc-host \
      -B build/chapter-02/echo-add-systemc-host \
      -DGEM5_ROOT="$PWD/third_party/gem5" \
      -DSYSTEMC_HOME="$SYSTEMC_HOME"
cmake --build build/chapter-02/echo-add-systemc-host -j4
./build/chapter-02/echo-add-systemc-host/echo_add_systemc_host \
  build/chapter-01/config.ini
```

预期输出至少包含：

```text
bridge accept: handle=1
device issue: handle=1
device complete: handle=1 value=12
bridge result: 12 (expected 12)
PASS echo_add_systemc_host
```
