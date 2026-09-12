# 从零搭建 gem5 与 SystemC 联合仿真环境

> NPU 系统建模与联合仿真系列第一章

实现一个算子模型并不难，真正困难的是回答下面这些系统问题：谁执行驱动程序，谁负责推进时间，CPU 与 NPU 如何交换命令和数据，访存延迟从哪里产生，仿真结束时还有没有请求留在队列里。

一个不含时序的 C++ 矩阵乘函数主要用于验证计算结果；仅有独立 NPU 模型时，也缺少执行 Guest 软件的 CPU 和配套存储系统。SystemC 本身可以描述这些模块，但本文选择复用 gem5 的 CPU 与存储模型。我们先搭建整个系列的底座：让 gem5 中的 RISC-V CPU 和 SystemC 中的硬件模型运行在同一个进程、同一条仿真时间轴上。

本章暂时不加入具体 NPU Engine。完成本章后，我们将得到一个可以继续扩展命令 Bridge、Shared Memory、DMA 和 Runtime 的最小联合仿真环境。

## 一 为什么需要联合仿真

NPU 模型通常会经历功能模型、周期模型和 RTL 三个阶段。它们不是互相替代的关系，而是回答不同问题。

![不同建模层级回答的问题](images/chapter-01/01-modeling-levels.png)

图 1 不同建模层级回答的问题

功能模型关心结果是否正确。例如输入两个 FP16 矩阵，输出是否与 NumPy 或 ONNX Runtime 的参考结果一致。它适合快速验证算法、数据类型和算子组合，但通常不会告诉我们命令排队了多久，也不会告诉我们 Shared Memory 是否发生 bank conflict。

周期模型关心数据在什么时候可用。它需要描述 FIFO 深度、模块吞吐率、流水延迟、端口冲突、反压和完成事件。它通常省略电路实现细节，运行速度也更适合早期架构探索；但时序精度取决于具体模型和校准情况，不能仅凭使用 C++、SystemC 或 RTL 来判定。RTL 仿真本身也不等于包含布局布线延迟的门级时序仿真。

RTL 进一步描述寄存器、组合逻辑和逐周期握手，是综合、时序分析和芯片实现的基础。不过，在架构尚未稳定时直接使用 RTL 扫描大量参数，开发成本会很高。

本系列用 gem5 与 SystemC 构建周期级系统模型。两者接在一起不会自动获得周期精确性，仍需逐项定义和验证时序契约：

- gem5 负责执行 RISC-V Guest ELF，并提供可选的 CPU、cache 和内存系统模型。本章没有配置 cache；SE 使用模拟进程的地址映射，不是完整操作系统的页表遍历路径。
- SystemC 负责表达 NPU 内部模块、FIFO、端口、延迟、反压和多 Engine 并行。
- 软件参考模型继续用于数值对比。
- RTL 在接口和微架构冻结后承担更精确的实现验证。

这套分工让我们可以在写出完整 RTL 之前，就观察一条 NPU 命令从 CPU 发出、进入队列、访问存储、完成计算并唤醒软件的全过程。

## 二 本系列要搭建什么系统

最终系统由 Host 程序、RISC-V Guest、gem5 和 SystemC NPU 模型组成。

![gem5 与 SystemC 联合仿真总体架构](images/chapter-01/02-cosim-architecture.png)

图 2 gem5 与 SystemC 联合仿真总体架构

Host 是在本机运行的 C++ 可执行程序。它包含 `sc_main`，链接外部 SystemC 库和 `libgem5`，创建全部模型并启动仿真。

Guest 是由 gem5 中的 RISC-V CPU 执行的 ELF。后续章节中，Guest 会调用 Runtime、提交 NPU 命令并检查输出。Guest 不是 Host 的普通函数，Host 也不会直接调用 Guest 的 `main`。gem5 会按照 ELF 装载规则建立进程地址空间、初始化栈和程序计数器，然后由模拟 CPU 取指执行。

SystemC NPU 位于 Host 进程内部。第一章只保留一个时钟和观察模块；第二章再加入命令 Bridge 与 Echo/Add 设备；之后逐步增加 Shared Memory、DMA、计算 Engine、同步和 Runtime。

本文采用 gem5 syscall emulation，也就是 SE 模式。它可以执行用户态 RISC-V 程序，但没有启动 Linux 内核，因此不能把本文环境描述成完整 SoC 或 Linux 板级仿真。涉及驱动、中断控制器、页表和操作系统调度的问题，需要在后续改用 full-system 模式或 RTL 平台验证。

## 三 为什么采用单进程嵌入

连接 gem5 和 SystemC 有多种方式。最直接的做法是让二者分别运行，再通过 socket 或共享内存通信。这样容易把程序启动起来，但时间协调会变得复杂：一边前进多少周期后应该等待另一边，消息到达时间如何转换，进程调度造成的延迟是否影响模拟结果，都需要额外协议。

本系列采用 gem5 自带的 `gem5_within_systemc` 方式：将 gem5 构建为 C++ 可配置的共享库 `libgem5`，由外部 SystemC kernel 承载 gem5 event queue。

这里有一个容易混淆的配置点。我们不是启用 gem5 内部自带的 SystemC kernel，而是：

```text
外部 Accellera SystemC kernel
        +
libgem5 C++ embedding library
        +
gem5_within_systemc adapter
```

因此，在这一方案中构建 `libgem5` 时使用 `USE_SYSTEMC=n` 是合理的。SystemC 由 Host 工程单独链接，adapter 负责把 gem5 的事件队列映射到外部 SystemC 时间轴。`USE_SYSTEMC=y` 对应另一种 gem5 构建方式，不能与本文方案混为一谈。

单进程方式便于统一时间协调和退出管理，但不会自动统一两个框架的生命周期。本文使用的上游 adapter 只支持单个 gem5 主事件队列、协作式 SystemC 进程，并要求仿真中至多创建一个 `Gem5SystemC::Module`。它不是并行运行多个 gem5 kernel 的通用接口。构建时，gem5、SystemC、编译器和 C++ ABI 也必须兼容。

## 四 两套调度器如何共享一条时间轴

SystemC 和 gem5 都有自己的事件调度机制：

- SystemC 使用 `sc_event`、`SC_THREAD`、`SC_METHOD` 和离散事件内核。
- gem5 使用 EventQueue，并用 tick 表示模拟时间。

联合仿真不能让两个调度器各走各的。本文让 SystemC 成为外层时间轴，gem5 adapter 每次查看 gem5 的下一事件：如果事件就在当前时刻，则立即处理；如果事件发生在未来，则安排一个延迟的 `sc_event`，等 SystemC 时间到达后再处理。

![SystemC 与 gem5 的时间协调](images/chapter-01/03-time-coordination.png)

图 3 SystemC 与 gem5 的时间协调

上游的 `eventLoop()` 是 `SC_METHOD`，不能在其中调用 `wait()`。它安排定时事件后返回，把控制权交还给 SystemC kernel；`simulate()` 则由 Host 的 `SC_THREAD` 调用，等待退出通知。下面是事件处理方法的示意伪代码，不是可直接编译的替代实现：

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

上游还使用 `externalSchedulingEvent` 处理提前唤醒：SystemC 一侧插入更早的 gem5 事件后，必须通知 adapter 重新检查队列，不能继续等原来的截止时间。同一时间戳内还涉及 gem5 事件优先级和 SystemC delta cycle，不能把它简化为任意排列的同时执行。

共享时间轴也不意味着任意时刻的 `gem5::curTick()` 都与 `sc_time_stamp()` 相等。当 SystemC 在两个 gem5 事件之间运行时，gem5 的当前 tick 可以暂时落后；adapter 在处理事件时通过 `catchup()` 对齐。后续 Bridge 从 SystemC 调回 gem5、安排新事件前，也需要遵循这一时间对齐与通知约定。

为了简化换算，本系列把 gem5 tick frequency 设置为 `10^12 tick/s`，并将 SystemC 时间分辨率设为 `1 ps`。这样：

```text
1 gem5 tick = 1 ps = sc_time_stamp().value() 的一个单位
```

这不是说 CPU 或 NPU 的周期都是 1 ps。CPU 可以使用 1.6 GHz，NPU 可以使用 800 MHz；1 ps 只是公共时间刻度。实际时钟周期仍由各自 clock domain 决定。

如果 SystemC 分辨率不是 1 ps，直接把 `sc_time_stamp().value()` 当作 gem5 tick 就会产生错误。因此示例 Host 在启动阶段显式检查时间分辨率，发现不一致就立即终止。

## 五 从 sc_main 到 Guest 退出

联合仿真的生命周期比普通 SystemC 程序长一些。

![联合仿真的启动与退出流程](images/chapter-01/04-lifecycle.png)

图 4 联合仿真的启动与退出流程

### 1 创建 Host 对象

`sc_main` 首先设置 1 ps 时间分辨率，然后创建 SystemC Top、观察时钟和 `Gem5Host`。

`Gem5Host` 继承 `Gem5SystemC::Module`。构造阶段完成 gem5 日志、统计、事件队列和 C++ 配置管理器的初始化，并读取由 gem5 Python 配置脚本生成的 `config.ini`。

### 2 实例化 gem5 SimObject

示例先用 `findAllObjects()` 创建模型对象，再逐个调用 `bindObjectPorts()`，最后调用 `instantiate(false)` 完成 `init()`、统计和 probe 注册。`false` 表示对象和端口已经准备好。默认的 `instantiate()` 会自行完成对象创建与端口绑定；后续若手动连接外部端口，则必须在 SimObject 的 `init()` 前完成，避免未连接错误或重复绑定。

### 3 装载 Guest ELF

SystemC elaboration 结束后，Host 调用 `initState()` 和 `startup()`。gem5 的 SE workload 在这一步装载 ELF、建立地址映射、初始化栈和 PC。

### 4 推进两个事件系统

调用 `sc_start()` 后，SystemC 先执行包括 `end_of_elaboration()` 在内的启动回调，再进入进程调度。`Gem5Host` 内部线程调用有 tick 上限的 `simulate()`，adapter 按上一节的方法协调 gem5 EventQueue 与 SystemC 时间。

### 5 处理退出和 drain

Guest 执行退出系统调用后，gem5 产生 `GlobalSimLoopExitEvent`。第一章没有外部 NPU 请求，因此检查退出原因、退出码和退出时的时间对齐后就调用 `sc_stop()`，并未执行完整系统 drain。超时事件也可能带有退出码 0，不能只用退出码判断成功。

加入 DMA 和异步 Engine 后，应先停止接受新工作，保留在途请求所需的完成回调并继续推进事件，待请求排空后再停止仿真。不能先屏蔽全部通知，否则 drain 可能永远无法结束。应用级请求排空与 gem5 `DrainManager` 的协议也需要分别处理。

这也是周期模型中一个经常被忽略的问题：程序结束只是软件事件，硬件队列可能仍有工作。

## 六 准备构建环境

本文选用以下目标依赖版本。当前运行验证来自 macOS arm64 上已有的 gem5 构建和 SystemC 2.3.4；尚未完成官方干净源码在 Linux/macOS 上的全量构建验证，因此下表不是已通过测试的跨平台兼容矩阵：

| 组件 | 版本或要求 | 用途 |
|---|---|---|
| gem5 | v25.1.0.1 | RISC-V CPU 与内存系统 |
| SystemC | 2.3.4 | NPU 模型与外部事件内核 |
| Python | 3.12 或 3.13 | gem5 构建与配置脚本 |
| SCons | 4.10.1 | 构建 gem5 |
| CMake | 3.20 以上 | 构建示例 Host 和 SystemC 模型 |
| C++ 编译器 | Apple clang 或 GCC | Host 模型编译 |

仓库没有复制 gem5 或 SystemC 源码。脚本会下载固定版本的 gem5；SystemC 建议安装 2.3.4，或者自行编译后设置 `SYSTEMC_HOME`。包管理器当前默认版本可能已更新，不能假定直接安装就一定得到 2.3.4。

脚本不是系统依赖的一键安装器。开始前需要 C/C++ 工具链、CMake、curl、tar、Python 的 venv 与开发文件，以及 zlib 开发文件；其他可选组件按 gem5 构建输出处理。Linux 软件包名称和编译器要求请对照文末 gem5 构建文档。Host 当前使用 C++17，SystemC 库的 C++ 标准与 ABI 必须匹配。测试范围与剩余验证项记录在仓库 `docs/VALIDATION.md`。

```bash
export SYSTEMC_HOME=/path/to/systemc
```

在 macOS Homebrew 环境中，它通常指向 `opt` 软链接或版本化 Cellar 目录：

```bash
export SYSTEMC_HOME=/opt/homebrew/opt/systemc
# 如果没有 opt 软链接，也可以使用：
export SYSTEMC_HOME=/opt/homebrew/Cellar/systemc/2.3.4
```

先运行独立 SystemC 示例，确认编译器、头文件和动态库没有问题：

```bash
cmake -S examples/chapter-01/systemc-hello \
      -B build/systemc-hello
cmake --build build/systemc-hello -j4
./build/systemc-hello/systemc_hello
```

它会创建一个周期为 1 ns 的时钟，在第 8 个上升沿停止。默认 `sc_clock` 在 0 ns 产生第一个上升沿，所以预期停止于 7 ns。`dont_initialize()` 只禁止进程初始化执行，不会屏蔽 0 时刻的真实时钟事件。示例同时检查边沿数和停止时间。

## 七 分开构建 gem5.opt 与 libgem5

本系列需要两种 gem5 产物：

1. `gem5.opt` 用来运行 Python 配置脚本、验证 RISC-V Guest，并生成 `config.ini`。
2. `libgem5_opt` 由 C++ Host 链接，用来在外部 SystemC kernel 中实例化 gem5。

两者使用的 SCons 配置不同。`libgem5` 需要：

```text
--with-cxx-config
--without-python
--without-tcmalloc
```

如果让它们共用一个 build 目录，SCons 可能反复重建生成文件，甚至让 Python 配置对象与 C++ 配置对象互相污染。因此示例把它们分别放在：

```text
third_party/gem5/build/RISCV_OPT
third_party/gem5/build/RISCV_LIB
```

运行：

```bash
./scripts/setup_gem5.sh
./scripts/build_gem5.sh
./scripts/build_libgem5.sh
```

完整编译需要一定时间。第一次构建后，后续修改本文 Host 或 SystemC 模型不需要重新编译整个 gem5。

## 八 生成 C++ 配置文件

普通 gem5 用户经常直接运行 Python 配置脚本。C++ embedding 模式不能在 Host 中依赖 Python，因此需要先把 SimObject 配置写成 `config.ini`。

示例脚本建立一个最小 SE 系统：

```text
RiscvMinorCPU
  -> SystemXBar
  -> MemCtrl
  -> DDR3 timing model
```

生成并验证配置：

```bash
./scripts/prepare_chapter01.sh
```

脚本首先让 `gem5.opt` 运行一次 RISC-V Hello World，同时使用 `--dump-config` 输出 `config.ini`。这样可以把问题分成两层：如果普通 gem5 都不能运行，先检查 Guest 和 gem5 构建；只有这一步通过后，再调试 C++ embedding。

`config.ini` 中会包含 Guest ELF 的绝对路径。移动仓库、切换电脑或更改 gem5 目录后，应当重新生成配置，不能把旧文件当成可移植资产提交。

## 九 构建联合仿真 Host

配置准备完成后，构建 Host：

```bash
cmake -S examples/chapter-01/gem5-systemc-host \
      -B build/gem5-systemc-host \
      -DGEM5_ROOT="$PWD/third_party/gem5" \
      -DSYSTEMC_HOME="$SYSTEMC_HOME"

cmake --build build/gem5-systemc-host -j4

./build/gem5-systemc-host/gem5_systemc_host \
  build/chapter-01/config.ini
```

运行过程中，RISC-V CPU 在 gem5 中执行 Hello World；SystemC heartbeat 进程同时记录外部时钟。Guest 退出后，Host 打印 gem5 退出原因、tick 和 SystemC 时间，并停止仿真。

这里验证的关键点不是终端里出现一句 Hello World，而是：

- Guest 确实由模拟 RISC-V CPU 取指执行。
- gem5 对象由 C++ `config.ini` 实例化。
- gem5 EventQueue 由外部 SystemC kernel 推进。
- 退出事件处的 gem5 tick 与 SystemC 时间一致，SystemC 时钟边沿持续推进。
- Guest 退出能够正常终止整个联合仿真进程。

示例默认最多执行 `10^9` tick，即 1 ms 模拟时间；可通过第二个参数覆盖。模拟时间上限不等于墙钟超时，CI 仍应设置进程超时。输出中的 `npu_clock_edges` 包含 0 时刻上升沿，不是已完成计算周期或 NPU 利用率。最终时间相等只检查一个同步点，不能替代后续双向 Bridge、提前唤醒和反压测试。

## 十 仓库为什么这样组织

```text
npu-system-modeling/
├── docs/
│   ├── chapter-01-gem5-systemc-foundation.md
│   └── images/chapter-01/
├── examples/chapter-01/
│   ├── systemc-hello/
│   └── gem5-systemc-host/
├── scripts/
├── third_party/          # 下载生成，不提交
├── build/                # 构建生成，不提交
└── SERIES_OUTLINE.md
```

文章、图片和对应代码在同一个 Git 提交中演进。每完成一章就创建一个 Tag，读者可以停留在任意阶段复现当时的最小系统。`third_party`、`build`、`config.ini` 和运行日志不提交，以免仓库混入第三方源码、绝对路径和过期产物。

后续从实际工程整理 common 模块时，也会遵循同一原则：保留通用机制，替换产品接口和参数。核心 Tensor、Vector/ACVT 等 Engine 不进入公开仓库，而是使用独立的教学计算单元维持完整数据流。

## 十一 常见问题

### 把 USE_SYSTEMC 当成唯一开关

`USE_SYSTEMC=y` 并不是所有 gem5 + SystemC 方案的必要条件。本文使用外部 SystemC kernel 加 `gem5_within_systemc` adapter，`libgem5` 构建为 `USE_SYSTEMC=n`。判断配置是否正确，首先要明确谁拥有 SystemC kernel。

### gem5.opt 和 libgem5 共用构建目录

两类产物的配置不同。混用 build 目录会造成大量无效重编译，并可能出现生成头文件不一致。固定使用独立的 `RISCV_OPT` 和 `RISCV_LIB`。

### 复制旧 config.ini 到新路径

`config.ini` 可能包含 Guest、重定向目录和资源文件的绝对路径。仓库移动后重新生成，比手工替换字符串可靠。

### macOS 找不到 libgem5

某些 gem5 构建生成的动态库 install name 是相对于 gem5 源码目录的路径。即使 CMake 使用绝对路径链接，程序启动时仍可能报告找不到 `build/RISCV_LIB/libgem5_opt.dylib`。示例 CMake 在链接后调用 `install_name_tool -change`，把可执行文件中的依赖改为当前 `libgem5` 绝对路径；Linux 则通过 build RPATH 查找共享库。

### 用 Host 预置数据冒充 Guest 行为

Host 可以通过功能接口直接写模型内存，这对测试初始化很方便，但不消耗 CPU 指令，也不经过 timing 数据通路。后续文章会把 Host preload、Guest 访问和 DMA timing 访问分别统计。

### 看到数值正确就认为周期模型正确

数值比对只能证明某条功能路径得到正确结果。周期模型还要验证请求是否守恒、反压时是否丢包、完成是否重复、退出时是否 drain，以及时间戳是否单调。

### 把 SE 模式称为裸机 SoC 或 Linux 系统

SE 模式承载的是用户态进程语义。它足以研究 CPU 指令、NPU 命令和部分存储行为，但没有模拟完整内核和板级外设。

## 十二 本章结论与模型边界

本章建立了后续所有功能的共同底座：RISC-V Guest 由 gem5 执行，SystemC 负责外层离散事件调度，`libgem5` adapter 把 gem5 EventQueue 映射到同一条时间轴。

当前最小系统尚未包含 NPU 命令、DMA、Shared Memory 或计算 Engine，因此不能用于评估 NPU 性能。它验证的是当前 Hello World 路径下的启动、退出和退出时的时间对齐，不足以证明任意跨框架交互的时序正确性。

下一章将在这个 Host 中加入教学命令 Bridge。我们会让 RISC-V CPU 发出第一条自定义 NPU 指令，并仔细区分四个经常被混用的时刻：命令被接受、Engine 执行完成、软件消费结果，以及 CPU 指令最终退休。

## 参考资料

1. gem5 官方仓库：[https://github.com/gem5/gem5](https://github.com/gem5/gem5)
2. gem5 `v25.1.0.1`：[https://github.com/gem5/gem5/releases/tag/v25.1.0.1](https://github.com/gem5/gem5/releases/tag/v25.1.0.1)
3. gem5 within SystemC adapter 源码：[sc_module.cc](https://github.com/gem5/gem5/blob/v25.1.0.1/util/systemc/gem5_within_systemc/sc_module.cc)
4. Accellera SystemC：[https://www.accellera.org/downloads/standards/systemc](https://www.accellera.org/downloads/standards/systemc)
5. SystemC 2.3.4 Release：[https://github.com/accellera-official/systemc/releases/tag/2.3.4](https://github.com/accellera-official/systemc/releases/tag/2.3.4)
6. gem5 构建依赖：[Building gem5](https://www.gem5.org/documentation/general_docs/building)
7. gem5 C++ 生命周期：[cxx_manager.cc](https://github.com/gem5/gem5/blob/v25.1.0.1/src/sim/cxx_manager.cc)
8. 本文配套代码、提纲与验证记录：[https://github.com/Ch1-n/npu-system-modeling](https://github.com/Ch1-n/npu-system-modeling)
