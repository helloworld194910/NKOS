#include <riscv.h>
# Test-only ABI canaries; caller-saved registers follow the C ABI.
.section .text
.option push
.option norelax
.option norvc
.balign 4
.globl member_a_switch_abi_probe
member_a_switch_abi_probe:
    addi sp, sp, -128
    STORE ra, 0(sp)
    STORE gp, 8(sp)
    STORE tp, 16(sp)
    STORE s0, 24(sp)
    STORE s1, 32(sp)
    STORE s2, 40(sp)
    STORE s3, 48(sp)
    STORE s4, 56(sp)
    STORE s5, 64(sp)
    STORE s6, 72(sp)
    STORE s7, 80(sp)
    STORE s8, 88(sp)
    STORE s9, 96(sp)
    STORE s10, 104(sp)
    STORE s11, 112(sp)
    li s0, 1792
    li s1, 1793
    li s2, 1794
    li s3, 1795
    li s4, 1796
    li s5, 1797
    li s6, 1798
    li s7, 1799
    li s8, 1800
    li s9, 1801
    li s10, 1802
    li s11, 1803
    call member_a_enter_user
    li t0, 1792
    bne s0, t0, .Labi_bad
    li t0, 1793
    bne s1, t0, .Labi_bad
    li t0, 1794
    bne s2, t0, .Labi_bad
    li t0, 1795
    bne s3, t0, .Labi_bad
    li t0, 1796
    bne s4, t0, .Labi_bad
    li t0, 1797
    bne s5, t0, .Labi_bad
    li t0, 1798
    bne s6, t0, .Labi_bad
    li t0, 1799
    bne s7, t0, .Labi_bad
    li t0, 1800
    bne s8, t0, .Labi_bad
    li t0, 1801
    bne s9, t0, .Labi_bad
    li t0, 1802
    bne s10, t0, .Labi_bad
    li t0, 1803
    bne s11, t0, .Labi_bad
    LOAD t0, 8(sp)
    bne gp, t0, .Labi_bad
    LOAD t0, 16(sp)
    bne tp, t0, .Labi_bad
    j .Labi_done
.Labi_bad:
    li a0, -3
.Labi_done:
    LOAD ra, 0(sp)
    LOAD gp, 8(sp)
    LOAD tp, 16(sp)
    LOAD s0, 24(sp)
    LOAD s1, 32(sp)
    LOAD s2, 40(sp)
    LOAD s3, 48(sp)
    LOAD s4, 56(sp)
    LOAD s5, 64(sp)
    LOAD s6, 72(sp)
    LOAD s7, 80(sp)
    LOAD s8, 88(sp)
    LOAD s9, 96(sp)
    LOAD s10, 104(sp)
    LOAD s11, 112(sp)
    addi sp, sp, 128
    ret
.option pop
