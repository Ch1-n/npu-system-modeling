# gem5 within SystemC Host

这个示例让外部 Accellera SystemC kernel 承载 `libgem5`。RISC-V Guest 在 gem5 中执行 Hello World，SystemC 同时记录教学 NPU 时钟；Guest 退出后，Host 停止整个联合仿真。

先在仓库根目录准备 gem5 和 `config.ini`：

```bash
./scripts/setup_gem5.sh
./scripts/build_gem5.sh
./scripts/build_libgem5.sh
./scripts/prepare_chapter01.sh
```

再构建并运行 Host：

```bash
export SYSTEMC_HOME=/path/to/systemc
cmake -S examples/chapter-01/gem5-systemc-host \
      -B build/gem5-systemc-host \
      -DGEM5_ROOT="$PWD/third_party/gem5" \
      -DSYSTEMC_HOME="$SYSTEMC_HOME"
cmake --build build/gem5-systemc-host -j4
./build/gem5-systemc-host/gem5_systemc_host build/chapter-01/config.ini
```

一组已验证运行的关键输出如下，具体 tick 会随 gem5 版本和配置变化：

```text
Hello world!
gem5 exit: tick=127806250 cause="exiting with last active thread context" code=0
SystemC stop: time=127806250 ps npu_cycles=102246
```

`config.ini` 含本机绝对路径，移动仓库或切换机器后应重新运行 `prepare_chapter01.sh`，不要提交生成文件。
