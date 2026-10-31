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

void interrupt_handler(struct trapframe *tf) {
    const uintptr_t interrupt_bit = (uintptr_t)1 << (sizeof(tf->cause) * 8 - 1);
    const uintptr_t cause = tf->cause & ~interrupt_bit;
    static size_t tick_reports;

    switch (cause) {
    case IRQ_U_SOFT:
        cprintf("User software interrupt\n");
        break;
    case IRQ_S_SOFT:
        /* SSIP is writable in S mode; acknowledge a real software interrupt. */
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
        clock_set_next_event();
        ++ticks;
        if (ticks % TICK_NUM == 0) {
            ++tick_reports;
            print_ticks();
            if (tick_reports >= SHUTDOWN_AFTER) {
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
        cprintf("Unknown interrupt: cause %llu\n", (unsigned long long)cause);
        print_trapframe(tf);
        break;
    }
    /* Asynchronous interrupts resume at the saved EPC without advancing it. */
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
 * tf->epc 后正常返回；其余异常只输出完整 Trap frame，不修改现场。 */
void exception_handler(struct trapframe *tf) {
    uintptr_t epc = tf->epc;
    int len;

    switch (tf->cause) {
    case CAUSE_ILLEGAL_INSTRUCTION:
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
        /* 其他未要求恢复的同步异常：输出完整现场，保持现场不变。 */
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
