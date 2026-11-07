#include <riscv.h>

    .macro SAVE_ALL

    csrw sscratch, sp

    addi sp, sp, -36 * REGBYTES
    # save x registers
    STORE x0, 0*REGBYTES(sp)
    STORE x1, 1*REGBYTES(sp)
    STORE x3, 3*REGBYTES(sp)
    STORE x4, 4*REGBYTES(sp)
    STORE x5, 5*REGBYTES(sp)
    STORE x6, 6*REGBYTES(sp)
    STORE x7, 7*REGBYTES(sp)
    STORE x8, 8*REGBYTES(sp)
    STORE x9, 9*REGBYTES(sp)
    STORE x10, 10*REGBYTES(sp)
    STORE x11, 11*REGBYTES(sp)
    STORE x12, 12*REGBYTES(sp)
    STORE x13, 13*REGBYTES(sp)
    STORE x14, 14*REGBYTES(sp)
    STORE x15, 15*REGBYTES(sp)
    STORE x16, 16*REGBYTES(sp)
    STORE x17, 17*REGBYTES(sp)
    STORE x18, 18*REGBYTES(sp)
    STORE x19, 19*REGBYTES(sp)
    STORE x20, 20*REGBYTES(sp)
    STORE x21, 21*REGBYTES(sp)
    STORE x22, 22*REGBYTES(sp)
    STORE x23, 23*REGBYTES(sp)
    STORE x24, 24*REGBYTES(sp)
    STORE x25, 25*REGBYTES(sp)
    STORE x26, 26*REGBYTES(sp)
    STORE x27, 27*REGBYTES(sp)
    STORE x28, 28*REGBYTES(sp)
    STORE x29, 29*REGBYTES(sp)
    STORE x30, 30*REGBYTES(sp)
    STORE x31, 31*REGBYTES(sp)

    # get sr, epc, badvaddr, cause
    # Set sscratch register to 0, so that if a recursive exception
    # occurs, the exception vector knows it came from the kernel
    csrrw s0, sscratch, x0
    csrr s1, sstatus
    csrr s2, sepc
    csrr s3, stval
    csrr s4, scause

    STORE s0, 2*REGBYTES(sp)
    STORE s1, 32*REGBYTES(sp)
    STORE s2, 33*REGBYTES(sp)
    STORE s3, 34*REGBYTES(sp)
    STORE s4, 35*REGBYTES(sp)
    .endm

    .macro RESTORE_ALL

    #lab3 function1 2412790
    # 入口条件：sp 仍指向 SAVE_ALL 建立的 struct trapframe。
    # 帧共 36 个 REGBYTES：0~31 槽为 x0~x31，32~35 槽依次为
    # sstatus、sepc、stval、scause；RV64 下每槽 8 字节，共 288 字节。
    # 恢复以 C 处理后的帧为准：异常处理可能已修改 epc，不能重新读取
    # 进入 Trap 时的 CSR 来覆盖它，也不能在这里统一给 epc 加偏移。
    # 先借用 s0/s1 写回返回所需 CSR，稍后再恢复这两个寄存器的原值。
    LOAD s0, 32*REGBYTES(sp)
    LOAD s1, 33*REGBYTES(sp)
    csrw sstatus, s0
    csrw sepc, s1

    # sstatus 提供 sret 使用的 SPP/SPIE 等状态，sepc 提供返回 PC。
    # stval/scause 是诊断信息，sret 不使用它们，因此不需要写回；
    # sscratch 已被 SAVE_ALL 清零，沿用当前内核态约定，无须恢复为旧 sp。
    # 从现在起保持 sp 为帧基址，逐槽恢复除 x0、x2 外的通用寄存器。
    # x0 恒为零，无须加载；即使 ABI 允许 C 改写 caller-saved 寄存器，
    # Trap 返回仍须恢复它们，因为被打断的代码没有主动发起普通函数调用。
    LOAD x1, 1*REGBYTES(sp)
    LOAD x3, 3*REGBYTES(sp)
    LOAD x4, 4*REGBYTES(sp)
    LOAD x5, 5*REGBYTES(sp)
    LOAD x6, 6*REGBYTES(sp)
    LOAD x7, 7*REGBYTES(sp)
    LOAD x8, 8*REGBYTES(sp)
    LOAD x9, 9*REGBYTES(sp)
    LOAD x10, 10*REGBYTES(sp)
    LOAD x11, 11*REGBYTES(sp)
    LOAD x12, 12*REGBYTES(sp)
    LOAD x13, 13*REGBYTES(sp)
    LOAD x14, 14*REGBYTES(sp)
    LOAD x15, 15*REGBYTES(sp)
    LOAD x16, 16*REGBYTES(sp)
    LOAD x17, 17*REGBYTES(sp)
    LOAD x18, 18*REGBYTES(sp)
    LOAD x19, 19*REGBYTES(sp)
    LOAD x20, 20*REGBYTES(sp)
    LOAD x21, 21*REGBYTES(sp)
    LOAD x22, 22*REGBYTES(sp)
    LOAD x23, 23*REGBYTES(sp)
    LOAD x24, 24*REGBYTES(sp)
    LOAD x25, 25*REGBYTES(sp)
    LOAD x26, 26*REGBYTES(sp)
    LOAD x27, 27*REGBYTES(sp)
    LOAD x28, 28*REGBYTES(sp)
    LOAD x29, 29*REGBYTES(sp)
    LOAD x30, 30*REGBYTES(sp)
    LOAD x31, 31*REGBYTES(sp)
    # 最后恢复 x2(sp)：SAVE_ALL 先把原 sp 存入 sscratch，再分配帧，
    # 因而第 2 槽记录的是进入前的 sp，而非当前帧地址。
    # 本条先按旧 sp 计算加载地址，再把结果写入 sp；之后不能再访问此帧。
    # 直接使用保存值也保留了 C 层修改有效上下文的语义，无须再 addi 释放帧。
    LOAD x2, 2*REGBYTES(sp)

    .endm

    .globl __alltraps
    .align(2)
__alltraps:
    # stvec 使用 Direct 模式，中断和异常都从这里进入；上方 .align(2)
    # 保证入口四字节对齐。硬件只记录关键 CSR 并更新特权/中断状态，
    # 不会替我们压栈保存通用寄存器，因此必须先 SAVE_ALL 再调用 C。
    # 本实验沿用当前内核栈，不在这里实现用户栈到内核栈的切换。
    SAVE_ALL

    #lab3 function1 2412790
    # 按 RISC-V ABI，首个指针参数由 a0 传递：此时 sp 就是完整帧的地址，
    # 所以对应 C 接口 trap(struct trapframe *tf)，传的是帧本身而非副本。
    # 原 a0、ra 已分别保存在第 10、1 槽，下面传参和 call 改写它们是安全的。
    # RV64 的帧大小 288 是 16 的倍数，入口栈对齐满足时仍满足 C 调用对齐。
    mv a0, sp
    call trap
    # call 写入 ra，使 trap 正常 ret 后回到这里；C 按 ABI 平衡自己的栈，
    # 返回时 sp 仍指向同一个帧，随后顺序落入 __trapret。
    # C 可修改帧中的 epc/寄存器槽，RESTORE_ALL 将采用这些最终值。

    .globl __trapret
__trapret:
    RESTORE_ALL
    # return from supervisor call
    sret
