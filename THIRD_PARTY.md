# Third Party Components

The repository does not vendor the following projects. Setup scripts download or link to them, and their own licenses continue to apply.

| Component | Version used by chapter 1 | License and source |
|---|---:|---|
| gem5 | v25.1.0.1 | BSD-3-Clause, https://github.com/gem5/gem5 |
| Accellera SystemC | 2.3.4 | Apache-2.0, https://github.com/accellera-official/systemc |

The chapter 1 host compiles adapter source files from gem5's `util/systemc/gem5_within_systemc` directory in the locally downloaded gem5 tree. Those files remain covered by the gem5 license.

The table is a summary, not a replacement for per-file notices. The adapter includes ARM and other copyright notices and an intellectual-property qualification. Retain the full applicable notices and disclaimers if redistributing adapter source or compiled binaries. The MIT license in this repository does not relicense third-party code.
