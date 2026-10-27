# Test-only U program. Upper-layer member supplies the final program.
.section .text.member_a_user_page,"ax",@progbits
.option push
.option norelax
.option norvc
.balign 4096
.globl member_a_user_page
member_a_user_page:
    addi sp, sp, -16
    li t0, 0x1234
    sd t0, 0(sp)
    ld a0, 0(sp)
    li a7, 0x5901
    ecall
    li t0, 0xabcd
    bne a0, t0, .Luser_bad
    li a0, 0x5678
    li a7, 0x5902
    ecall
.Luser_bad:
    li a0, 0xbad
    li a7, 0x59ff
    ecall
    j .Luser_bad
.balign 4096
.globl member_a_user_page_end
member_a_user_page_end:
.option pop
