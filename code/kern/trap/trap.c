#include <assert.h>
#include <clock.h>
#include <console.h>
#include <defs.h>
#include <kdebug.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>
#include <stdio.h>
#include <trap.h>

#define TICK_NUM 100

static void print_ticks() {
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S
 */
void idt_init(void) {

    extern void __alltraps(void);
    /* Set sup0 scratch register to 0, indicating to exception vector
       that we are presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
}

/* trap_in_kernel - test if trap happened in kernel */
bool trap_in_kernel(struct trapframe *tf) {
    return (tf->status & SSTATUS_SPP) != 0;
}

void print_trapframe(struct trapframe *tf) {
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  badvaddr 0x%08x\n", tf->badvaddr);
    cprintf("  cause    0x%08x\n", tf->cause);
}

void print_regs(struct pushregs *gpr) {
    cprintf("  zero     0x%08x\n", gpr->zero);
    cprintf("  ra       0x%08x\n", gpr->ra);
    cprintf("  sp       0x%08x\n", gpr->sp);
    cprintf("  gp       0x%08x\n", gpr->gp);
    cprintf("  tp       0x%08x\n", gpr->tp);
    cprintf("  t0       0x%08x\n", gpr->t0);
    cprintf("  t1       0x%08x\n", gpr->t1);
    cprintf("  t2       0x%08x\n", gpr->t2);
    cprintf("  s0       0x%08x\n", gpr->s0);
    cprintf("  s1       0x%08x\n", gpr->s1);
    cprintf("  a0       0x%08x\n", gpr->a0);
    cprintf("  a1       0x%08x\n", gpr->a1);
    cprintf("  a2       0x%08x\n", gpr->a2);
    cprintf("  a3       0x%08x\n", gpr->a3);
    cprintf("  a4       0x%08x\n", gpr->a4);
    cprintf("  a5       0x%08x\n", gpr->a5);
    cprintf("  a6       0x%08x\n", gpr->a6);
    cprintf("  a7       0x%08x\n", gpr->a7);
    cprintf("  s2       0x%08x\n", gpr->s2);
    cprintf("  s3       0x%08x\n", gpr->s3);
    cprintf("  s4       0x%08x\n", gpr->s4);
    cprintf("  s5       0x%08x\n", gpr->s5);
    cprintf("  s6       0x%08x\n", gpr->s6);
    cprintf("  s7       0x%08x\n", gpr->s7);
    cprintf("  s8       0x%08x\n", gpr->s8);
    cprintf("  s9       0x%08x\n", gpr->s9);
    cprintf("  s10      0x%08x\n", gpr->s10);
    cprintf("  s11      0x%08x\n", gpr->s11);
    cprintf("  t3       0x%08x\n", gpr->t3);
    cprintf("  t4       0x%08x\n", gpr->t4);
    cprintf("  t5       0x%08x\n", gpr->t5);
    cprintf("  t6       0x%08x\n", gpr->t6);
}

//lab3 function2 2413921 程子涵（成员 B：interrupt_handler）
#include <sbi.h>

/* Counts statistics lines, not individual interrupts. Course supplies no value;
 * 10 leaves time for the original Timer test to observe ticks=200. */
#ifndef SHUTDOWN_AFTER
#define SHUTDOWN_AFTER 10
#endif
#if SHUTDOWN_AFTER < 1
#error "SHUTDOWN_AFTER must be a positive statistics-line count"
#endif

/* Member C supplies this definition; no production exception stub is installed. */
void exception_handler(struct trapframe *tf);

/* 异步中断处理思路：入口分发已确认 scause 最高位为 1，这里再取出
 * 低位原因码；按类型完成应答或输出，最终沿统一汇编路径返回。
 * 本实验的实质处理是 S 态时钟：续约下一次事件 -> 累计 ticks ->
 * 每 TICK_NUM 次输出一行 -> 累计 SHUTDOWN_AFTER 行后通过 SBI 关机。
 * 中断与当前指令无关，不能像跳过异常指令那样推进 tf->epc。 */
void interrupt_handler(struct trapframe *tf) {
    /* 用 uintptr_t 构造与 cause 同宽的最高位掩码，避免把中断标志
     * 当作原因码的一部分；本实验 RV64 下即清除 bit 63。 */
    const uintptr_t interrupt_bit = (uintptr_t)1 << (sizeof(tf->cause) * 8 - 1);
    const uintptr_t cause = tf->cause & ~interrupt_bit;
    /* static 跨多次 Trap 保留统计行数；它与每次时钟都增加的 ticks
     * 含义不同，也不会在每次进入 handler 时重新清零。 */
    static size_t tick_reports;

    /* U/H/M 等分支按框架原因码提供分类提示，并不表示当前 S 态内核
     * 已实现这些特权级的中断路由或完整外设驱动。 */
    switch (cause) {
    case IRQ_U_SOFT:
        cprintf("User software interrupt\n");
        break;
    case IRQ_S_SOFT:
        /* S 态可写 sip.SSIP，清除 pending 位以应答软件中断，
         * 避免 sret 后同一请求仍挂起而立即再次进入。 */
        clear_csr(sip, MIP_SSIP);
        cprintf("Supervisor software interrupt\n");
        break;
    case IRQ_H_SOFT:
        cprintf("Hypervisor software interrupt\n");
        break;
    case IRQ_M_SOFT:
        cprintf("Machine software interrupt\n");
        break;
    case IRQ_U_TIMER:
        cprintf("User timer interrupt\n");
        break;
    case IRQ_S_TIMER:
        /* SBI 定时器设置的是一次绝对截止时间，不会自动周期重装。
         * 先以当前 time + timebase 设置下一次事件，保持连续触发；
         * 若只计数不续约，就无法得到后续周期时钟的正确行为。 */
        clock_set_next_event();
        ++ticks;
        /* TICK_NUM=100：每个中断只计一次，每满 100 次输出
         * "100 ticks"。tick_reports 只在真正到达统计边界时增加。 */
        if (ticks % TICK_NUM == 0) {
            ++tick_reports;
            /* DEBUG_GRADE 下 print_ticks 内部会 panic 结束评分场景；
             * 普通构建才会继续执行下面的累计报告关机判断。 */
            print_ticks();
            if (tick_reports >= SHUTDOWN_AFTER) {
                /* 默认阈值为 10 行，即通常在 ticks=1000 请求关机。
                 * 这里通过框架 SBI 接口交给固件结束系统。 */
                cprintf("Shutdown after %llu reports (ticks=%llu)\n",
                        (unsigned long long)tick_reports,
                        (unsigned long long)ticks);
                sbi_shutdown();
            }
        }
        break;
    case IRQ_H_TIMER:
        cprintf("Hypervisor timer interrupt\n");
        break;
    case IRQ_M_TIMER:
        cprintf("Machine timer interrupt\n");
        break;
    case IRQ_U_EXT:
        cprintf("User external interrupt\n");
        break;
    case IRQ_S_EXT:
        cprintf("Supervisor external interrupt\n");
        break;
    case IRQ_H_EXT:
        cprintf("Hypervisor external interrupt\n");
        break;
    case IRQ_M_EXT:
        cprintf("Machine external interrupt\n");
        break;
    default:
        /* 未识别原因只保留诊断现场；打印本身不等于完成了设备应答，
         * 也不保证未知中断返回后不会再次发生。 */
        cprintf("Unknown interrupt: cause %llu\n", (unsigned long long)cause);
        print_trapframe(tf);
        break;
    }
    /* 保持 tf->epc：硬件保存的是中断返回的继续执行位置，统一恢复流程
     * 会将它写回 sepc，再由 sret 继续被打断的执行流。 */
}

//lab3 function2 2411824 于蕾（成员 C：exception_handler）

/* exception_inst_length - 判断 epc 处指令长度（字节）。
 *
 * 只读取起始的 16 位，避免在可能仅按 2 字节对齐的地址上做 4 字节读。
 * 在本实验 16/32 位编码范围内：
 *   - 低 2 位 != 11                 -> 16 位压缩指令，返回 2；
 *   - 低 2 位 == 11 且低 5 位 != 11111 -> 32 位指令，返回 4；
 *   - 低 2 位 == 11 且低 5 位 == 11111 -> >= 48 位指令，本实验不支持，返回 0。
 * 返回 0 表示无法确认长度，调用方不得推进 epc。 */
static int exception_inst_length(uintptr_t epc) {
    uint16_t low16 = *(const uint16_t *)epc;

    if ((low16 & 0x3) != 0x3) {
        return 2; /* 16 位压缩指令 */
    }
    if ((low16 & 0x1f) != 0x1f) {
        return 4; /* 32 位指令 */
    }
    return 0; /* >= 48 位指令：本实验不支持 */
}

/* exception_handler - 处理同步异常。非法指令与断点按实际指令长度推进
 * tf->epc 后正常返回；其余异常只输出完整 Trap frame，不修改现场。
 *
 * 思路：同步异常的 epc 指向触发异常的指令，若原样返回就会再次触发。
 * 本实验对非法指令/断点选择“报告后跳过”，不模拟指令，也不终止进程；
 * 指令长度交给上面的解码辅助函数判断，避免把 c.ebreak 也固定跳过 4 字节。
 * 仅修改内存帧的 epc，由 RESTORE_ALL 统一写回 sepc，保持返回路径一致。
 * 长度判断直接读取 epc 的前 16 位，依赖本实验故障指令地址可读的前提；
 * 这不是可安全处理任意损坏地址的通用异常恢复器。 */
void exception_handler(struct trapframe *tf) {
    /* 保留触发位置供诊断和长度判断使用；tf->epc 表示最终返回位置。 */
    uintptr_t epc = tf->epc;
    int len;

    switch (tf->cause) {
    case CAUSE_ILLEGAL_INSTRUCTION:
        /* 按规定先打印原故障地址，再根据编码跳过 2/4 字节。
         * 不直接依赖 badvaddr/stval 解码，因为其内容未必提供完整指令。 */
        cprintf("Illegal instruction at %p\n", (void *)epc);
        len = exception_inst_length(epc);
        if (len == 2 || len == 4) {
            tf->epc = epc + len;
        } else {
            /* 长度无法确认（>= 48 位）：不擅自推进，仅诊断兜底。 */
            print_trapframe(tf);
        }
        break;

    case CAUSE_BREAKPOINT:
        /* ebreak 与 c.ebreak 的原因码相同，但长度可分别为 4/2 字节；
         * 必须读取实际编码，确保返回落到紧随断点后的指令边界。 */
        cprintf("Breakpoint at %p\n", (void *)epc);
        len = exception_inst_length(epc);
        if (len == 2 || len == 4) {
            tf->epc = epc + len;
        } else {
            /* 长度无法确认（>= 48 位）：不擅自推进，仅诊断兜底。 */
            print_trapframe(tf);
        }
        break;

    default:
        /* 其他未要求恢复的同步异常：只打印，不读取 epc 处指令，
         * 即使测试帧 epc=0 也不会在此取指。现场不变意味着这里只做
         * 诊断兜底，真实故障返回后仍可能重复发生，尚无通用恢复策略。 */
        print_trapframe(tf);
        break;
    }
}


static inline void trap_dispatch(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0) {
        // interrupts
        interrupt_handler(tf);
    } else {
        // exceptions
        exception_handler(tf);
    }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap()
 * returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void trap(struct trapframe *tf) {
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
}
