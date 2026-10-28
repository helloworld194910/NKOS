#include <riscv.h>
#lab3 member-a-switch 2412790
# Isolated extension; the mandatory trapentry.S remains unchanged.
.section .text
.option push
.option norelax
.option norvc
.balign 4
.globl member_a_enter_user
member_a_enter_user:
    la t0, member_a_switch_saved_sp
    LOAD t1, 0(t0)
    bnez t1, .Lbusy
    csrr t1, sstatus
    csrci sstatus, 2
    addi sp, sp, -192
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
    STORE t1, 136(sp) # original sstatus
    csrr t1, stvec
    STORE t1, 120(sp)
    csrr t1, sscratch
    STORE t1, 128(sp)
    csrr t1, sepc
    STORE t1, 144(sp)
    csrr t1, sie
    STORE t1, 152(sp)
    csrw sie, zero
    STORE sp, 0(t0)
    la t0, member_a_user_trap_entry
    csrw stvec, t0
    csrw sscratch, a2
    csrw sepc, a0
    csrr t0, sstatus
    li t1, 0x122 # SPP | SPIE | SIE
    not t1, t1
    and t0, t0, t1
    csrw sstatus, t0
    mv sp, a1
    li x1, 0
    li x3, 0
    li x4, 0
    li x5, 0
    li x6, 0
    li x7, 0
    li x8, 0
    li x9, 0
    li x10, 0
    li x11, 0
    li x12, 0
    li x13, 0
    li x14, 0
    li x15, 0
    li x16, 0
    li x17, 0
    li x18, 0
    li x19, 0
    li x20, 0
    li x21, 0
    li x22, 0
    li x23, 0
    li x24, 0
    li x25, 0
    li x26, 0
    li x27, 0
    li x28, 0
    li x29, 0
    li x30, 0
    li x31, 0
    sret
.Lbusy:
    li a0, -2
    ret
.balign 4
.globl member_a_user_trap_entry
member_a_user_trap_entry:
    csrrw sp, sscratch, sp
    bnez sp, .Lstack_ready
    # An unexpected S trap with scratch=0 must not store through sp=0.
    csrrw sp, sscratch, sp
    csrw sscratch, sp
.Lstack_ready:
    addi sp, sp, -288
    STORE x0, 0(sp)
    STORE x1, 8(sp)
    STORE x3, 24(sp)
    STORE x4, 32(sp)
    STORE x5, 40(sp)
    STORE x6, 48(sp)
    STORE x7, 56(sp)
    STORE x8, 64(sp)
    STORE x9, 72(sp)
    STORE x10, 80(sp)
    STORE x11, 88(sp)
    STORE x12, 96(sp)
    STORE x13, 104(sp)
    STORE x14, 112(sp)
    STORE x15, 120(sp)
    STORE x16, 128(sp)
    STORE x17, 136(sp)
    STORE x18, 144(sp)
    STORE x19, 152(sp)
    STORE x20, 160(sp)
    STORE x21, 168(sp)
    STORE x22, 176(sp)
    STORE x23, 184(sp)
    STORE x24, 192(sp)
    STORE x25, 200(sp)
    STORE x26, 208(sp)
    STORE x27, 216(sp)
    STORE x28, 224(sp)
    STORE x29, 232(sp)
    STORE x30, 240(sp)
    STORE x31, 248(sp)
    csrr t0, sscratch
    STORE t0, 16(sp)
    csrw sscratch, zero
    csrr t0, sstatus
    STORE t0, 256(sp)
    csrr t0, sepc
    STORE t0, 264(sp)
    csrr t0, sbadaddr
    STORE t0, 272(sp)
    csrr t0, scause
    STORE t0, 280(sp)
    # The U program may have changed gp/tp; use saved kernel values for C.
    la t0, member_a_switch_saved_sp
    LOAD t0, 0(t0)
    LOAD gp, 8(t0)
    LOAD tp, 16(t0)
    mv a0, sp
    call member_a_user_trap
    beqz a0, .Lresume_user
    li t0, 1
    bne a0, t0, .Lbad_action
    li t1, 0
    j .Lfinish
.Lbad_action:
    li t1, -1
.Lfinish:
    la t0, member_a_switch_saved_sp
    LOAD sp, 0(t0)
    STORE zero, 0(t0)
    LOAD t0, 120(sp)
    csrw stvec, t0
    LOAD t0, 128(sp)
    csrw sscratch, t0
    LOAD t0, 144(sp)
    csrw sepc, t0
    LOAD t0, 152(sp)
    csrw sie, t0
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
    mv a0, t1
    LOAD t0, 136(sp)
    csrw sstatus, t0
    addi sp, sp, 192
    ret
.Lresume_user:
    LOAD t0, 256(sp)
    andi t1, t0, 0x100
    bnez t1, .Lbad_action # Callback cannot escalate a user return to S mode.
    addi t1, sp, 288
    csrw sscratch, t1
    csrw sstatus, t0
    LOAD t0, 264(sp)
    csrw sepc, t0
    LOAD x1, 8(sp)
    LOAD x3, 24(sp)
    LOAD x4, 32(sp)
    LOAD x5, 40(sp)
    LOAD x6, 48(sp)
    LOAD x7, 56(sp)
    LOAD x8, 64(sp)
    LOAD x9, 72(sp)
    LOAD x10, 80(sp)
    LOAD x11, 88(sp)
    LOAD x12, 96(sp)
    LOAD x13, 104(sp)
    LOAD x14, 112(sp)
    LOAD x15, 120(sp)
    LOAD x16, 128(sp)
    LOAD x17, 136(sp)
    LOAD x18, 144(sp)
    LOAD x19, 152(sp)
    LOAD x20, 160(sp)
    LOAD x21, 168(sp)
    LOAD x22, 176(sp)
    LOAD x23, 184(sp)
    LOAD x24, 192(sp)
    LOAD x25, 200(sp)
    LOAD x26, 208(sp)
    LOAD x27, 216(sp)
    LOAD x28, 224(sp)
    LOAD x29, 232(sp)
    LOAD x30, 240(sp)
    LOAD x31, 248(sp)
    LOAD sp, 16(sp)
    sret
.option pop
.section .bss
.balign 8
member_a_switch_saved_sp:
    .zero 8
