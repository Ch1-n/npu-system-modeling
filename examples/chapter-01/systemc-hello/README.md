# SystemC Hello

这个最小示例只验证 SystemC 安装、C++ ABI、动态库搜索路径和 1 ps 时间分辨率。模块观察一个 1 ns 时钟，在第 8 个上升沿停止仿真。

```bash
export SYSTEMC_HOME=/path/to/systemc
cmake -S examples/chapter-01/systemc-hello -B build/systemc-hello
cmake --build build/systemc-hello -j4
ctest --test-dir build/systemc-hello --output-on-failure
```

预期最后输出：

```text
PASS systemc_hello at 7500 ps
```
