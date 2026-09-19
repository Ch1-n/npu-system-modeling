    .section .text
    .globl _start
    .type _start, @function

_start:
    li      a0, 7
    li      a1, 5
    .insn   r 0x0b, 0, 1, a2, a0, a1
    addi    a0, a2, 0
    li      a7, 93
    ecall

    .size _start, .-_start
