# NPU System Modeling with gem5 and SystemC

[简体中文](README.md) | [English](README.en.md)

This project documents how I am building an NPU system simulator, from connecting a RISC-V CPU to SystemC models through to data movement, engine scheduling, and runtime software. It accompanies a [Chinese technical article series](SERIES_OUTLINE.md).

The long-term goal is to run inference for a small language model in the simulator and trace how software maps onto hardware, including where time is spent waiting for data or resources. **The current public release is the co-simulation foundation, not a working NPU or an inference framework.**

The public examples use simplified parameters and independent teaching engines. General-purpose components will be released incrementally; proprietary compute engines, product instruction encodings, hardware-specific performance parameters, and internal test assets are outside the scope of this repository.

## Architecture

An external Accellera SystemC kernel runs a host C++ application that links against `libgem5`. The upstream `gem5_within_systemc` adapter coordinates gem5's main event queue with SystemC time.

![System architecture and planned NPU components; labels are in Chinese](docs/images/chapter-01/02-cosim-architecture.png)

- **Guest:** a RISC-V ELF executed by gem5's CPU model in syscall emulation (SE) mode. No guest Linux kernel is booted.
- **Host:** the native C++ application, entered through `sc_main`, that creates the models and runs the simulation.
- **Adapter:** coordinates the two event schedulers in a single process. Both use a 1 ps time unit; this is not a fixed simulation step or a CPU clock period.
- **SystemC side:** currently a clock and a counter. The NPU blocks shown in yellow are planned additions.

Chapter 1 uses a RISC-V MinorCPU, a bus, and a basic DDR3 memory configuration without CPU caches. It does not exercise RVV workloads, custom NPU instructions, DMA, or HBM.

## What Has Been Verified

The two public C++ examples were rebuilt and tested on macOS arm64 with SystemC 2.3.4. The gem5 executable and shared library were reused from an existing local 25.1.0.1 build whose source tree includes local extensions.

| Check | Observed result |
| --- | --- |
| Standalone SystemC example | Eight clock rising edges; simulation ends at 7 ns |
| RISC-V Hello World | Guest exits normally in standalone gem5 and the co-simulation host |
| Host exit | Exit reason and code are checked; gem5 and SystemC timestamps agree at exit |
| Failure paths | A one-tick limit, invalid limits, and missing arguments are rejected |

**A full clean-source build using the supplied scripts has not yet been validated end to end. Linux builds are also unverified.** Passing Hello World does not validate RVV execution, bidirectional bridge timing, or NPU performance. See the [validation record](docs/VALIDATION.md) (Chinese) for dependency provenance, warnings, and outstanding checks.

## Build and Run Chapter 1

These are the intended build steps for the public example, subject to the validation limits above. Run them from the repository root.

### Prerequisites

| Dependency | Target version |
| --- | --- |
| gem5 | `v25.1.0.1` |
| Accellera SystemC | `2.3.4`, compatible with the host compiler and C++17 ABI |
| Python | `3.12` or `3.13`, including development files and `venv` |
| SCons | `4.10.1`, installed by the setup script |
| CMake | `3.20` or later |
| Native compiler | Apple Clang or GCC; compiler/platform combinations are not fully tested |

You also need Git, curl, tar, zlib development files, and the applicable [gem5 build dependencies](https://www.gem5.org/documentation/general_docs/building). The scripts do not install system packages or SystemC.

**No Xuantie or other RISC-V cross-compiler is required for Chapter 1.** It runs the prebuilt Hello World ELF supplied in gem5's `tests/test-progs/hello/bin/riscv/linux/hello`. Native C++ tools compile the host programs, not the Guest executable.

### Check SystemC First

```bash
git clone https://github.com/Ch1-n/npu-system-modeling.git
cd npu-system-modeling

export SYSTEMC_HOME=/absolute/path/to/systemc

cmake -S examples/chapter-01/systemc-hello -B build/systemc-hello
cmake --build build/systemc-hello -j4
ctest --test-dir build/systemc-hello --output-on-failure
```

The standalone executable prints `PASS systemc_hello at 7 ns`. The first rising edge is at time zero, so the eighth occurs at 7 ns.

### Build gem5 and Run the Host

```bash
./scripts/setup_gem5.sh
./scripts/build_gem5.sh
./scripts/build_libgem5.sh
./scripts/prepare_chapter01.sh

cmake -S examples/chapter-01/gem5-systemc-host \
      -B build/gem5-systemc-host \
      -DGEM5_ROOT="$PWD/third_party/gem5" \
      -DSYSTEMC_HOME="$SYSTEMC_HOME"
cmake --build build/gem5-systemc-host -j4

./build/gem5-systemc-host/gem5_systemc_host build/chapter-01/config.ini

python3 tests/check_chapter01_host.py \
  build/gem5-systemc-host/gem5_systemc_host \
  build/chapter-01/config.ini
```

The setup script downloads the pinned gem5 release if a source tree is not already present. Existing trees are reused, so check their version and local patches. It also prepares the Python environment and SCons. `PYTHON_BIN` can select Python explicitly, and `JOBS` controls gem5 build parallelism.

`prepare_chapter01.sh` first runs the Guest in standalone gem5, then generates the configuration used by the C++ host. Regenerate `config.ini` after moving the repository: it contains absolute paths.

The scripts use `USE_SYSTEMC=n` to disable gem5's internal SystemC implementation. The host separately links the external Accellera SystemC library; co-simulation remains enabled through the adapter.

If SystemC is installed outside the expected `lib` directory, pass `-DSYSTEMC_LIBRARY=/absolute/path/to/libsystemc.so` (or the appropriate library on your platform) to each CMake configuration command. The host test script applies a 60-second wall-clock timeout to each run; the host's default simulated-time limit is 1 ms.

## Roadmap

Only the foundation examples are published so far. Planned additions are:

1. A CPU-to-SystemC command bridge with independent Echo/Add examples and explicit request/completion semantics.
2. DMA, local memory, and external-memory timing, including Ramulator2 integration.
3. Multiple engines, command scheduling, and synchronization.
4. Runtime software, operator execution, and workload scheduling.
5. Validation, profiling, and inference experiments on a small language model.

See the [six-part outline](SERIES_OUTLINE.md) for the broader plan. These items are development goals, not available features.

## Articles and Feedback

- [Chapter 1: Building a gem5 and SystemC Co-simulation Environment](docs/chapter-01-gem5-systemc-foundation.md) (Chinese)
- [Validation record](docs/VALIDATION.md) (Chinese)
- [Third-party components](THIRD_PARTY.md)

The articles are currently in Chinese. This English README provides a project overview and a build entry point; full article translations are not yet maintained. Figures include editable Draw.io sources alongside PNGs.

Questions and reproducibility reports are welcome through [GitHub Issues](https://github.com/Ch1-n/npu-system-modeling/issues). Please include the repository commit, OS/architecture, dependency versions, whether gem5 has local patches, the command you ran, and the relevant error output. Remove private paths or credentials before posting logs.

The `chapter-01` tag preserves the initial publication snapshot. Later corrections are made through new commits rather than moving that tag.

## License

Original code and documentation are distributed under the [MIT License](LICENSE). Third-party projects retain their own licenses. Their source trees are not included in this repository; consult [THIRD_PARTY.md](THIRD_PARTY.md) for details.
