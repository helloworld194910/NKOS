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
#include <vmm.h>

#define TICK_NUM 100

static void print_ticks()
{
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* idt_init - initialize IDT to each of the entry points in kern/trap/vectors.S
 */
void idt_init(void)
{
    extern void __alltraps(void);
    /* Set sscratch register to 0, indicating to exception vector that we are
     * presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
    /* Allow kernel to access user memory */
    set_csr(sstatus, SSTATUS_SUM);
}

void print_trapframe(struct trapframe *tf)
{
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  badvaddr 0x%08x\n", tf->badvaddr);
    cprintf("  cause    0x%08x\n", tf->cause);
}

void print_regs(struct pushregs *gpr)
{
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

extern struct mm_struct *check_mm_struct;

//lab3 请补充你在lab3中的代码
// Lab3 timer policy adapted for Lab4 by 2412790; original handler: 2413921.
void interrupt_handler(struct trapframe *tf) {
    const uintptr_t bit = (uintptr_t)1 << (sizeof(tf->cause) * 8 - 1);
    switch (tf->cause & ~bit) {
    case IRQ_S_SOFT:
        clear_csr(sip, MIP_SSIP);
        break;
    case IRQ_S_TIMER:
        clock_set_next_event();
        ++ticks;
        if (ticks % TICK_NUM == 0) {
            print_ticks();
        }
        break;
    default:
        print_trapframe(tf);
        break;
    }
    // An asynchronous interrupt must not advance the saved PC.
}

// Original exception handling: 2411824; migration: 2412790.
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



/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap()
 * returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void trap(struct trapframe *tf)
{
    // dispatch based on what type of trap occurred
    if ((intptr_t)tf->cause < 0)
    {
        // interrupts
        interrupt_handler(tf);
    }
    else
    {
        // exceptions
        exception_handler(tf);
    }
}
