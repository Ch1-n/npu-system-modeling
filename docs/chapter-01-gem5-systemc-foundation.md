# 从零搭建 gem5 与 SystemC 联合仿真环境

> NPU 系统建模与联合仿真系列第一篇

这段时间，我在做一个基于 gem5 和 SystemC 的 NPU 系统仿真项目。打算借这个系列，把搭建环境、连接 CPU、处理访存和调度的过程整理下来，也把其中一些通用代码单独放出来，方便大家动手试一试。

先从一个具体问题说起。假设已经有了矩阵乘模型，输入数据能算出正确结果，接下来想知道：DMA 还在搬数据时，计算单元能不能开始工作？CPU 提交命令后，是一直等着，还是可以继续执行？如果再增加一个计算单元，性能会不会被内存带宽卡住？

这些问题都需要把计算单元放进系统里看。这也是我选择 gem5 + SystemC 的原因：用 gem5 执行 CPU 上的软件，用 SystemC 描述 NPU 的内部模块，再把两边的时间协调起来。

第一篇先不放计算 Engine，只让 RISC-V CPU 跑一段 Hello World，同时让 SystemC 时钟正常运行。后面的命令接口、DMA 和 Runtime，都从这个小例子往上加。

配套代码在 [npu-system-modeling](https://github.com/Ch1-n/npu-system-modeling)。图也保留了 Draw.io 源文件，可以下载修改。

## 一 为什么选 gem5 和 SystemC

做 NPU 仿真时，功能模型、周期模型和 RTL 都会用到，只是看问题的角度不同。

![不同建模层级回答的问题](images/chapter-01/01-modeling-levels.png)

图 1 不同建模层级回答的问题

比如验证一次 FP16 矩阵乘，功能模型可以给出参考结果，检查数据类型、舍入和计算过程是否符合预期。但如果想知道这次运算花了多少周期，就要把流水延迟、队列、存储端口和等待时间也算进去。这些是周期模型要描述的东西。

再往下做到 RTL，才需要把寄存器、组合逻辑和逐周期握手落实成电路。架构还在调整时，我更希望能方便地修改队列深度、访存延迟或计算吞吐率，观察整个系统的变化，因此这个系列先围绕周期模型展开。

SystemC 很适合组织这些硬件模块。时钟、FIFO、事件、多个模块之间的并行，都有现成的表达方式。不过，单独一个 NPU 模型还需要有人给它发命令。可以在 testbench 里写死一串请求，也可以让 CPU 执行程序来提交请求。后者能把软件侧的开销和行为一起带进来，所以这里接入 gem5，复用它的 RISC-V CPU 和存储系统模型。

用上这两个工具，并不意味着模型自然就是“周期精确”的。比如把 DMA 简化成固定延迟后，这段延迟就不会反映真实的带宽竞争。模型省略了什么，后面分析性能时还得记着。

## 二 先分清 Host 和 Guest

下面这张图是整个系列的结构。黄色框里的 NPU 模块会在后续文章中逐步加入，第一篇暂时只有时钟和一个计数模块。

![gem5 与 SystemC 联合仿真总体架构](images/chapter-01/02-cosim-architecture.png)

图 2 gem5 与 SystemC 联合仿真总体架构

图里有两种程序，编译和调试时要分清。

**Host** 是在电脑上直接运行的 C++ 程序，入口是 `sc_main`。它链接 SystemC 和 `libgem5`，负责创建模型、读取配置和启动仿真。

**Guest** 是交给模拟 CPU 执行的 RISC-V ELF。后面写的 Runtime、测试程序都属于这一侧。Host 不会直接调用 Guest 的 `main`：gem5 先装载 ELF、建立进程地址空间并初始化栈和 PC，再由模拟 CPU 取指执行。

所以，看到终端打印 `Hello world!` 时，这句话应该来自 Guest 的执行结果，而不是 Host 里的一条 `printf`。

这里采用 gem5 的 SE 模式，即 syscall emulation。它能运行用户态 RISC-V 程序，把支持的系统调用交给模拟器处理，不需要先启动 Linux 内核。对于当前的实验，这样比较轻便；如果以后要研究内核驱动、中断或操作系统调度，就需要另外搭 full-system 环境。本篇也没有配置 CPU cache，先让 CPU 端口直接连接总线和内存控制器。

### 为什么把两边放在一个进程里

gem5 和 SystemC 可以分别运行，通过 socket 或共享内存通信。不过这样还要约定：两边各自能往前跑多远，消息对应哪个模拟时刻，什么时候停下来等对方。

这里采用 gem5 自带的 `gem5_within_systemc` 示例所使用的方式，把 gem5 编译成共享库，由外部 SystemC kernel 承载它的事件队列。最终运行的是一个 Host 进程，里面同时有 gem5 对象和 SystemC 模块：

```text
外部 Accellera SystemC kernel
        +
libgem5 C++ embedding library
        +
gem5_within_systemc adapter
```

构建时有个看起来有些反直觉的选项：这里要用 `USE_SYSTEMC=n`。它关闭的是 gem5 内部的 SystemC 实现；我们使用的 Accellera SystemC 库由 Host 单独链接，并没有把联合仿真所需的 SystemC 关掉。

这份上游 adapter 支持单个 gem5 主事件队列，整个仿真里只创建一个 `Gem5SystemC::Module`。第一篇就按这个结构来，不涉及多个 gem5 实例或并行事件队列。

## 三 时间由谁往前推

把库链接到一起之后，还有一个问题：gem5 和 SystemC 都有自己的事件调度器，究竟听谁的？

这里由 SystemC 控制外层时间。adapter 查看 gem5 队列中最早的事件，如果已经到了执行时刻，就处理它；如果还没到，就向 SystemC 安排一次定时通知，把控制权交回去。

举个例子：SystemC 当前是 10 ns，gem5 下一事件在 12 ns。adapter 会安排一个 2 ns 后的通知。期间如果 SystemC 模块在 11 ns 有事要做，内核仍会先处理它，不会因为 gem5 在等 12 ns 就跳过它。

![SystemC 与 gem5 的时间协调](images/chapter-01/03-time-coordination.png)

图 3 SystemC 与 gem5 的时间协调

上游 `eventLoop()` 的主要逻辑可以缩写成下面这样。这里只保留时间判断和退出处理，完整实现见文末源码链接。

```cpp
while (!gem5_event_queue.empty()) {
    catchup();  // Align gem5's current tick with SystemC time.
    auto next = gem5_event_queue.nextTick();
    auto now = sc_time_stamp().value();
    if (next > now) {
        wakeup.notify(sc_time::from_value(next - now));
        return;  // SC_METHOD must not call wait().
    }
    if (next < now) {
        fatal("event scheduled in the past");
    }
    if (gem5_event_queue.serviceOne()) {
        exit_notification.notify(SC_ZERO_TIME);
        return;
    }
}
```

注意这里的 `return`。`eventLoop()` 是一个 `SC_METHOD`，不能在里面调用 `wait()`，而是先安排通知，再返回。等通知到来，SystemC 会重新调用它。Host 中调用 `simulate()` 的则是 `SC_THREAD`，这个线程可以等待仿真退出通知。

还有一种情况：原本打算等到 12 ns，但 SystemC 模块在 11 ns 向 gem5 插入了一个更早的事件。这时必须通过 adapter 的外部通知机制提前唤醒事件处理，重新检查队列。上游用 `externalSchedulingEvent` 处理这件事，后面写 Bridge 时还会用到。

代码里的 `catchup()` 也值得留意。在两个 gem5 事件之间，SystemC 可能已经往前走了，`gem5::curTick()` 还停留在上次处理事件的时刻。`catchup()` 把它对齐到当前 SystemC 时间。所以“共用一条时间轴”并不意味着随时读取两个时间戳，它们都恰好相等。以后从 SystemC 回调 gem5 时，也要先处理好这个对齐关系。

### 统一时间单位

这份 adapter 假定一个 gem5 tick 就是 1 ps，因此 Host 将 gem5 频率设为 `10^12 tick/s`，SystemC 分辨率设为 `1 ps`：

```text
1 gem5 tick = 1 ps = sc_time_stamp().value() 的一个单位
```

这只是时间刻度，不是 CPU 或 NPU 的时钟周期。例子里的 CPU 是 1.6 GHz，对应 625 ps；SystemC 侧时钟是 800 MHz，对应 1250 ps。两个时钟都能用整数个 tick 表示，换算比较直接。

Host 在启动时会检查 SystemC 分辨率。如果它不是 1 ps，就停止运行，避免带着错误的时间单位继续仿真。同一个时间戳内部仍有 gem5 事件优先级和 SystemC delta cycle 的先后关系，这部分留到接入双向请求时再展开。

## 四 程序怎么启动和退出

接下来看代码入口。把下面这条调用顺序理清，后面遇到“对象没初始化”或“端口没接上”时，会比较容易定位。

![联合仿真的启动与退出流程](images/chapter-01/04-lifecycle.png)

图 4 联合仿真的启动与退出流程

在 `sc_main` 里，先设置时间分辨率，再创建时钟、计数模块和 `Gem5Host`。`Gem5Host` 继承上游的 `Gem5SystemC::Module`，构造时初始化日志、统计和事件队列，并读取 `config.ini`。

这个配置文件决定要创建哪些 gem5 对象，以及它们之间怎么连接。示例先调用 `findAllObjects()`，再逐个 `bindObjectPorts()`，最后执行 `instantiate(false)`，完成对象初始化和统计注册。这里传 `false`，是因为对象创建和端口绑定已经做过了；默认的 `instantiate()` 会把这两步也一起完成。

随后，`sc_main` 调用 `sc_start()`。SystemC 先执行 elaboration 相关回调，我们在 `end_of_elaboration()` 中调用 gem5 的 `initState()` 和 `startup()`。SE 进程在这个阶段装载 ELF、初始化地址映射、栈和 PC。接着 Host 的线程调用 `simulate()`，CPU 才开始在事件调度下执行 Guest。

Guest 退出时，gem5 返回一个 `GlobalSimLoopExitEvent`。Host 检查退出原因、退出码，以及这个时刻两边的时间是否对齐，然后调用 `sc_stop()`，最后导出统计。

这里不能只检查退出码是否为 0，因为 `simulate()` 达到时间上限时，也可能返回 code=0。示例会同时确认退出原因是最后一个 Guest 线程正常结束。

本篇还没有 NPU 请求，可以这样直接停止。后面加上 DMA 后，就要考虑另一种情况：Guest 已经退出，但最后一笔数据还没写回。这时需要先停止提交新工作，继续处理在途请求，等它们排空后再退出，也就是 drain。完成回调在此期间仍要保留，否则请求等不到响应，反而退不出去。具体实现放到后面接 DMA 时再讲。

## 五 把例子跑起来

先下载仓库，后面的命令都在仓库根目录执行：

```bash
git clone https://github.com/Ch1-n/npu-system-modeling.git
cd npu-system-modeling
```

下面是示例选用的依赖版本。本机已在 macOS arm64、SystemC 2.3.4 和已有 gem5 构建上跑通；官方干净源码的完整构建、Linux 环境还没有验证完。具体进度放在仓库的 `docs/VALIDATION.md`，读者复现前可以先看一下。

| 组件 | 版本或要求 | 用途 |
|---|---|---|
| gem5 | v25.1.0.1 | RISC-V CPU 与内存系统 |
| SystemC | 2.3.4 | 外部事件内核与硬件模型 |
| Python | 3.12 或 3.13 | gem5 构建与配置脚本 |
| SCons | 4.10.1 | 构建 gem5 |
| CMake | 3.20 以上 | 构建 C++ 示例 |
| C++ 编译器 | Apple clang 或 GCC | 编译 Host |

构建前还需要准备 Python 开发文件、venv、zlib 开发文件，以及 curl、tar 等工具。仓库脚本负责下载 gem5 和准备 SCons，不代替系统包管理器。Linux 的依赖安装可参考文末 gem5 官方构建文档。

### 先单独运行 SystemC

设置 `SYSTEMC_HOME`，让 CMake 找到 SystemC 的头文件和库。建议先用 2.3.4，并确认它和 Host 的 C++17、编译器 ABI 兼容。

```bash
export SYSTEMC_HOME=/path/to/systemc
```

如果用 macOS Homebrew 安装，路径可能是下面两种之一，按实际安装位置选择。Homebrew 当前默认版本不一定还是 2.3.4。

```bash
export SYSTEMC_HOME=/opt/homebrew/opt/systemc
# 或版本化目录：
export SYSTEMC_HOME=/opt/homebrew/Cellar/systemc/2.3.4
```

然后运行独立示例：

```bash
cmake -S examples/chapter-01/systemc-hello \
      -B build/systemc-hello
cmake --build build/systemc-hello -j4
./build/systemc-hello/systemc_hello
```

这个程序只观察一个 1 ns 时钟，在第八个上升沿停止。预期输出是：

```text
PASS systemc_hello at 7 ns
```

为什么是 7 ns？因为默认 `sc_clock` 在 0 ns 就产生了第一个上升沿。`dont_initialize()` 禁止的是进程初始化时的那次执行，不会屏蔽这个时钟事件。

这一步先通过，再去编译 gem5。否则后面一旦遇到链接错误，很难马上判断是 SystemC 安装问题，还是 gem5 的问题。

### 分别构建 gem5.opt 和 libgem5

这里需要两种 gem5 产物。`gem5.opt` 能运行 Python 配置脚本，用来先测试 Guest、生成配置；`libgem5_opt` 则由 C++ Host 链接。后者采用以下选项，启用 C++ 配置接口并去掉 Python 嵌入依赖：

```text
--with-cxx-config
--without-python
--without-tcmalloc
```

两种构建使用不同配置，示例将输出目录分开，避免来回切换选项导致重编译或生成文件混用：

```text
third_party/gem5/build/RISCV_OPT
third_party/gem5/build/RISCV_LIB
```

按顺序执行：

```bash
./scripts/setup_gem5.sh
./scripts/build_gem5.sh
./scripts/build_libgem5.sh
```

首次编译会比较久。之后只修改本文的 Host 或 SystemC 模型时，通常不用再编译整个 gem5。

### 生成 config.ini

平时直接用 gem5 时，可以在 Python 里创建 CPU、总线和内存控制器。我们的 Host 没有嵌入 Python，因此先用 `gem5.opt` 把这些对象及连接关系导出成 `config.ini`，再交给 C++ 配置管理器读取。

本篇配置很简单：

```text
RiscvMinorCPU
  -> SystemXBar
  -> MemCtrl
  -> DDR3 timing model
```

运行下面的脚本：

```bash
./scripts/prepare_chapter01.sh
```

它先让 `gem5.opt` 单独跑一次 RISC-V Hello World，确认 Guest 可以正常退出，再生成 Host 使用的配置。这样调试时就有了一个参照：如果单独运行 gem5 已经失败，先不用看 SystemC 侧。

`config.ini` 里含有 Guest ELF 的绝对路径。换电脑或移动仓库后要重新生成，这个文件不提交到 Git。

### 构建并运行 Host

准备好配置后，编译联合仿真程序：

```bash
cmake -S examples/chapter-01/gem5-systemc-host \
      -B build/gem5-systemc-host \
      -DGEM5_ROOT="$PWD/third_party/gem5" \
      -DSYSTEMC_HOME="$SYSTEMC_HOME"

cmake --build build/gem5-systemc-host -j4

./build/gem5-systemc-host/gem5_systemc_host \
  build/chapter-01/config.ini
```

macOS 下可能遇到一种情况：编译链接成功，启动却报找不到 `build/RISCV_LIB/libgem5_opt.dylib`。原因是某些 gem5 构建把相对路径写进了动态库 install name。示例 CMake 已在链接后调用 `install_name_tool -change` 修正这个依赖路径；Linux 使用 build RPATH 查找共享库。

## 六 怎么看运行结果

本机这组配置的关键输出如下，较长的退出信息做了折行，tick 数会随版本和配置变化：

```text
Hello world!
gem5 exit: tick=127806250
  cause="exiting with last active thread context" code=0
SystemC stop: time=127806250 ps npu_clock_edges=102246
PASS gem5_systemc_host
```

第一句来自 RISC-V Guest。后面的退出信息显示，gem5 的 tick 和 SystemC 时间对得上，SystemC 侧也确实收到了时钟边沿。`npu_clock_edges` 包含 0 时刻的上升沿，只是一个观察计数，目前还没有 NPU 计算可统计。

示例默认给 `simulate()` 设置了 `10^9` tick，也就是 1 ms 模拟时间的上限。可以在命令后再传一个数字来修改它。如果传 `1`，程序应该因为达到上限而失败，不能仍然打印 PASS。仓库里的检查脚本也覆盖了这条路径：

```bash
ctest --test-dir build/systemc-hello --output-on-failure
python3 tests/check_chapter01_host.py \
  build/gem5-systemc-host/gem5_systemc_host \
  build/chapter-01/config.ini
```

模拟时间上限和我们实际等待的时间是两回事。假如 Host 自己卡在一个不让出控制权的循环里，模拟时间也可能停住，所以检查脚本另外给每个进程设置了 60 秒的墙钟超时。

到这里，验证的是 Guest 执行、两个框架的启动和退出，以及退出时的时间对齐。CPU 与 NPU 还没有交换请求，暂时也看不到带宽、计算吞吐率这些结果。下一篇接上命令 Bridge 后，才开始检查请求有没有丢失、返回时间是否正确。

## 七 后面准备怎么接着做

这篇先把环境搭到能运行一个小程序。下一篇会加一个简单的 Echo/Add 设备，让 CPU 发出命令，SystemC 侧等若干周期后返回结果。这样就能沿着一条真实请求，看清 CPU 在哪里等待、设备何时完成，以及响应怎么送回来。

再往后是数据搬运、片上存储、多 Engine 调度和 Runtime。公开版本会逐步整理原项目里可以复用的 common 模块，核心计算 Engine 则用独立的小例子替代。希望读者最后拿到的不只是几个分散的测试，而是一套能继续添加自己模块的仿真环境。

文章正文、代码和配图放在同一个仓库里，系列提纲保存在 `SERIES_OUTLINE.md`。后续修改会保留提交记录，已经发布的章节标签也会保留，方便对照文章当时的版本。

## 参考资料

1. gem5 官方仓库：[https://github.com/gem5/gem5](https://github.com/gem5/gem5)
2. gem5 `v25.1.0.1`：[https://github.com/gem5/gem5/releases/tag/v25.1.0.1](https://github.com/gem5/gem5/releases/tag/v25.1.0.1)
3. gem5 within SystemC adapter 源码：[sc_module.cc](https://github.com/gem5/gem5/blob/v25.1.0.1/util/systemc/gem5_within_systemc/sc_module.cc)
4. Accellera SystemC：[https://www.accellera.org/downloads/standards/systemc](https://www.accellera.org/downloads/standards/systemc)
5. SystemC 2.3.4 Release：[https://github.com/accellera-official/systemc/releases/tag/2.3.4](https://github.com/accellera-official/systemc/releases/tag/2.3.4)
6. gem5 构建依赖：[Building gem5](https://www.gem5.org/documentation/general_docs/building)
7. gem5 C++ 生命周期：[cxx_manager.cc](https://github.com/gem5/gem5/blob/v25.1.0.1/src/sim/cxx_manager.cc)
8. 本文配套代码、提纲与验证记录：[https://github.com/Ch1-n/npu-system-modeling](https://github.com/Ch1-n/npu-system-modeling)
