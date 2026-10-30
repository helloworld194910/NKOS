/* Isolated stvec experiment. Not part of the production Makefile. */
#include <riscv.h>

    .section .text.member_b_vector,"ax",@progbits
    .option push
    .option norvc
    .option norelax
    .balign 256
    .globl member_b_vector_base
member_b_vector_base:
    j .Lslot0
    j .Lslot1
    j .Lunexpected
    j .Lunexpected
    j .Lunexpected
    .globl member_b_vector_timer
member_b_vector_timer:
    j .Lslot5
    .rept 10
    j .Lunexpected
    .endr

    /* Record the slot without changing any interrupted register or sscratch.
     * Restore the temporary stack allocation before A's SAVE_ALL sees sp. */
    .macro RECORD_SLOT number
    addi sp, sp, -16
    sd t0, 0(sp)
    sd t1, 8(sp)
    la t0, member_b_vector_slot
    li t1, \number
    sd t1, 0(t0)
    ld t0, 0(sp)
    ld t1, 8(sp)
    addi sp, sp, 16
    j __alltraps
    .endm
.Lslot0:
    RECORD_SLOT 0
.Lslot1:
    RECORD_SLOT 1
.Lslot5:
    RECORD_SLOT 5
.Lunexpected:
    RECORD_SLOT 255
    .option pop
