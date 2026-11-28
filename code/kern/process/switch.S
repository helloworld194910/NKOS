#include <riscv.h>

//lab4 请补充你在lab4中的代码

//LAB4 function3 2411824
#include <riscv.h>

/*
 * switch_to(from, to)  —— void switch_to(struct context *from, struct context *to)
 *   a0 = from（当前线程的 context 地址）
 *   a1 = to  （下一个线程的 context 地址）
 *
 * 保存返回地址 ra、栈指针 sp 和被调用者保存寄存器 s0~s11，共 14 个：
 * 因为线程切换发生在一个普通 C 函数调用内部，调用者保存寄存器
 * 已经由编译器在调用点保存好了，这里不必重复保存。
 * struct context 的字段顺序就是 ra, sp, s0...s11，偏移量依次是 0,8,16,...,104。
 */
.text
.globl switch_to
switch_to:
    # ---- 保存 from 的执行现场 ----
    STORE ra,  0*REGBYTES(a0)
    STORE sp,  1*REGBYTES(a0)
    STORE s0,  2*REGBYTES(a0)
    STORE s1,  3*REGBYTES(a0)
    STORE s2,  4*REGBYTES(a0)
    STORE s3,  5*REGBYTES(a0)
    STORE s4,  6*REGBYTES(a0)
    STORE s5,  7*REGBYTES(a0)
    STORE s6,  8*REGBYTES(a0)
    STORE s7,  9*REGBYTES(a0)
    STORE s8, 10*REGBYTES(a0)
    STORE s9, 11*REGBYTES(a0)
    STORE s10,12*REGBYTES(a0)
    STORE s11,13*REGBYTES(a0)

    # ---- 恢复 to 的执行现场 ----
    LOAD ra,  0*REGBYTES(a1)
    LOAD sp,  1*REGBYTES(a1)
    LOAD s0,  2*REGBYTES(a1)
    LOAD s1,  3*REGBYTES(a1)
    LOAD s2,  4*REGBYTES(a1)
    LOAD s3,  5*REGBYTES(a1)
    LOAD s4,  6*REGBYTES(a1)
    LOAD s5,  7*REGBYTES(a1)
    LOAD s6,  8*REGBYTES(a1)
    LOAD s7,  9*REGBYTES(a1)
    LOAD s8, 10*REGBYTES(a1)
    LOAD s9, 11*REGBYTES(a1)
    LOAD s10,12*REGBYTES(a1)
    LOAD s11,13*REGBYTES(a1)

    # ra 已经被换成 to 的返回地址，ret 就跳到 to 上次停下的地方
    ret
