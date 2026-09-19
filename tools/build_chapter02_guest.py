#!/usr/bin/env python3
"""Build the tiny chapter 2 Guest without requiring a RISC-V toolchain."""

from pathlib import Path
import struct
import sys


ELF_HEADER = struct.Struct("<16sHHIQQQIHHHHHH")
PROGRAM_HEADER = struct.Struct("<IIQQQQQQ")
ENTRY = 0x10000
PAGE = 0x1000


def instruction(word: int) -> bytes:
    return struct.pack("<I", word)


def build_guest(output: Path) -> None:
    # li a0, 7; li a1, 5; npu_add a2, a0, a1; exit(a2)
    code = b"".join(
        instruction(word)
        for word in (
            0x00700513,  # addi a0, zero, 7
            0x00500593,  # addi a1, zero, 5
            0x02B5060B,  # .insn r 0x0b, 0, 1, a2, a0, a1
            0x00060513,  # addi a0, a2, 0
            0x05D00893,  # addi a7, zero, 93 (exit)
            0x00000073,  # ecall
        )
    )

    ident = b"\x7fELF" + bytes((2, 1, 1, 0)) + bytes(8)
    header = ELF_HEADER.pack(
        ident,
        2,          # ET_EXEC
        243,        # EM_RISCV
        1,          # EV_CURRENT
        ENTRY,
        ELF_HEADER.size,
        0,
        0,
        ELF_HEADER.size,
        PROGRAM_HEADER.size,
        1,
        0,
        0,
        0,
    )
    program = PROGRAM_HEADER.pack(
        1,              # PT_LOAD
        5,              # PF_R | PF_X
        PAGE,
        ENTRY,
        ENTRY,
        len(code),
        len(code),
        PAGE,
    )

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(header + program + bytes(PAGE - len(header) - len(program)) + code)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} OUTPUT")
    build_guest(Path(sys.argv[1]))
