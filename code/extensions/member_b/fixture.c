/* Test-only boot/console/exception fixture. Links the real B trap.c, clock.c,
 * intr.c, SBI implementation and A trapentry.S; it never pretends to test PMM. */
#include <assert.h>
#include <atomic.h>
#include <clock.h>
#include <defs.h>
#include <intr.h>
#include <riscv.h>
#include <sbi.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sync.h>
#include <trap.h>

extern void interrupt_handler(struct trapframe *tf);
extern char member_b_vector_base[], member_b_vector_timer[];
volatile uintptr_t member_b_vector_slot;
static volatile unsigned break_count;

static uintptr_t now(void) { return read_csr(time); }
static int sie_enabled(void) { return !!(read_csr(sstatus) & SSTATUS_SIE); }
static void putch(int ch, void *count) {
    sbi_console_putchar((unsigned char)ch);
    ++*(int *)count;
}
int cprintf(const char *fmt, ...) {
    int n = 0;
    va_list ap;
    va_start(ap, fmt);
    vprintfmt(putch, &n, fmt, ap);
    va_end(ap);
    return n;
}
static void finish(void) __attribute__((noreturn));
static void finish(void) {
    intr_disable();
    write_csr(sie, 0);
    sbi_shutdown();
    for (;;) asm volatile("wfi");
}
static void require(int ok, const char *label) {
    if (ok) return;
    intr_disable();
    cprintf("FAIL: %s (ticks=%llu)\n", label, (unsigned long long)ticks);
    finish();
}
void __panic(const char *file, int line, const char *fmt, ...) {
    intr_disable();
    cprintf("PANIC at %s:%d: %s\n", file, line, fmt);
#if TEST_CASE == 4
    require(!strcmp(fmt, "EOT: kernel seems ok.") && ticks == 100,
            "DEBUG_GRADE must stop at the first 100 ticks");
    cprintf("PASS: DEBUG_GRADE first report enters existing panic path\n");
#else
    cprintf("FAIL: unexpected panic\n");
#endif
    finish();
}
/* This isolated fixture does not exercise C's production handler. Only the stvec routing experiment
 * accepts a deliberately encoded four-byte breakpoint here. */
void exception_handler(struct trapframe *tf) {
#if TEST_CASE == 6
    require(tf->cause == CAUSE_BREAKPOINT, "fixture accepts only breakpoint");
    require(member_b_vector_slot == 0, "synchronous exception must use BASE");
    ++break_count;
    tf->epc += 4;
#else
    (void)tf;
    require(0, "unexpected exception (C handler is not under test)");
#endif
}
/* Isolated dispatcher: production trap/exception symbols are renamed only in
 * the test compilation. Full make grade exercises the unmodified dispatcher. */
void trap(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0) interrupt_handler(tf);
    else exception_handler(tf);
}
static void await_ticks(size_t target) {
    uintptr_t deadline = now() + 50000000;
    while (ticks < target) {
        if (now() > deadline) require(0, "timer stopped before target");
        asm volatile("" ::: "memory");
    }
}
static void synthetic(void) {
    struct trapframe tf, before;
    const uintptr_t bit = (uintptr_t)1 << 63;
    for (unsigned cause = 0; cause < 12; ++cause) {
        if (cause == IRQ_S_TIMER) continue;
        memset(&tf, 0x5a, sizeof(tf));
        tf.cause = bit | cause;
        memcpy(&before, &tf, sizeof(tf));
        interrupt_handler(&tf);
        require(!memcmp(&tf, &before, sizeof(tf)), "non-timer changed trap frame");
        require(ticks == 0, "non-timer changed ticks");
    }
    for (unsigned flagged = 0; flagged < 2; ++flagged) {
        tf.cause = (flagged ? bit : 0) | 15;
        memcpy(&before, &tf, sizeof(tf));
        interrupt_handler(&tf);
        require(!memcmp(&tf, &before, sizeof(tf)), "unknown changed trap frame");
    }
    cprintf("PASS: classifications, raw/flagged unknown and unchanged frame\n");
    ticks = 98;
    tf.cause = bit | IRQ_S_TIMER;
    memcpy(&before, &tf, sizeof(tf));
    for (unsigned i = 0; i < 3; ++i) {
        interrupt_handler(&tf);
        require(ticks == 99 + i, "one increment per timer call");
        require(!memcmp(&tf, &before, sizeof(tf)), "timer changed EPC/status/GPR");
    }
    cprintf("PASS: timer boundaries 99/100/101 and unchanged EPC/status/GPR\n");
    finish();
}
static void timer_run(void) {
    clock_init();
    require(ticks == 0 && !sie_enabled(), "clock_init changed global SIE/ticks");
    intr_enable();
    await_ticks(200);
    intr_disable();
    require(ticks == 200, "expected exactly 200 completed timer traps");
    cprintf("PASS: real sustained timer returned to caller, ticks=200\n");
    intr_enable();
    /* The real interrupt_handler must shut down; runner checks exact line
     * count, terminal ticks, clean exit and this earlier return marker. */
    for (;;) asm volatile("wfi");
}
static void sync_run(void) {
    bool outer, inner;
    write_csr(sie, 0);
    intr_disable();
    local_intr_save(outer);
    require(outer == 0 && !sie_enabled(), "save initial SIE=0");
    local_intr_restore(outer);
    require(!sie_enabled(), "restore initial SIE=0");
    cprintf("PASS: initial SIE=0 preserved\n");
    intr_enable();
    local_intr_save(outer);
    require(outer == 1 && !sie_enabled(), "save initial SIE=1");
    local_intr_save(inner);
    require(inner == 0 && !sie_enabled(), "nested save");
    local_intr_restore(inner);
    require(!sie_enabled(), "inner restore enabled interrupts too early");
    local_intr_restore(outer);
    require(sie_enabled(), "outer restore did not restore SIE=1");
    cprintf("PASS: initial SIE=1 and nested LIFO restore\n");
    /* Both macros must behave as a single statement with caller semicolon. */
    if (1) local_intr_save(outer); else require(0, "save dangling else");
    if (1) local_intr_restore(outer); else require(0, "restore dangling else");
    require(sie_enabled(), "if/else macro semantics");
    intr_disable();
    cprintf("PASS: save/restore macros compile and run under if/else\n");

    clock_init();
    intr_enable();
    await_ticks(1);
    local_intr_save(outer);
    size_t held = ticks;
    uintptr_t until = now() + 400000;
    while (now() < until) asm volatile("" ::: "memory");
    require(ticks == held, "timer handler entered critical section");
    require(read_csr(sip) & MIP_STIP, "timer not pending during critical section");
    local_intr_restore(outer);
    await_ticks(held + 1);
    intr_disable();
    cprintf("PASS: pending timer deferred in critical section and delivered after restore\n");
    volatile unsigned long bits = 0;
    set_bit(1, &bits);
    require(test_bit(1, &bits), "atomic set");
    require(test_and_set_bit(1, &bits), "atomic old set value");
    require(test_and_clear_bit(1, &bits), "atomic old clear value");
    change_bit(2, &bits);
    clear_bit(2, &bits);
    require(bits == 0, "atomic final bits");
    cprintf("PASS: existing atomic bit operations smoke check\n");
    finish();
}
static void vector_mode(unsigned mode) {
    intr_disable();
    write_csr(sie, 0);
    clear_csr(sip, MIP_SSIP);
    uintptr_t requested = (uintptr_t)member_b_vector_base | mode;
    write_csr(stvec, requested);
    uintptr_t actual = read_csr(stvec);
    cprintf("stvec requested=0x%llx actual=0x%llx\n",
            (unsigned long long)requested, (unsigned long long)actual);
    if (actual != requested && mode == 1) {
        cprintf("SKIP: Vectored request not retained; WARL platform limitation\n");
        return;
    }
    require(actual == requested, "Direct stvec readback");
    member_b_vector_slot = 255;
    unsigned before_break = break_count;
    asm volatile(".word 0x00100073" ::: "memory");
    require(break_count == before_break + 1 && member_b_vector_slot == 0,
            "breakpoint routing and return");
    cprintf("PASS: mode=%u synchronous exception enters BASE\n", mode);

    member_b_vector_slot = 255;
    set_csr(sie, MIP_SSIP);
    set_csr(sip, MIP_SSIP);
    intr_enable();
    uintptr_t deadline = now() + 10000000;
    while (member_b_vector_slot == 255 && now() < deadline) asm volatile("");
    intr_disable();
    write_csr(sie, 0);
    require(member_b_vector_slot == (mode ? 1 : 0), "software vector slot");
    require(!(read_csr(sip) & MIP_SSIP), "SSIP was not acknowledged");
    cprintf("PASS: mode=%u software interrupt slot=%u and SSIP cleared\n",
            mode, (unsigned)member_b_vector_slot);

    clock_init();
    member_b_vector_slot = 255;
    intr_enable();
    await_ticks(3);
    intr_disable();
    write_csr(sie, 0);
    require(member_b_vector_slot == (mode ? 5 : 0), "timer vector slot");
    cprintf("PASS: mode=%u timer slot=%u; repeated sret return\n",
            mode, (unsigned)member_b_vector_slot);
}
static void vector_run(void) {
    uintptr_t saved = read_csr(stvec);
    require(((uintptr_t)member_b_vector_base & 255) == 0, "vector base alignment");
    require(member_b_vector_timer - member_b_vector_base == 20, "timer slot offset");
    vector_mode(0);
    vector_mode(1);
    write_csr(stvec, saved);
    require(read_csr(stvec) == saved, "stvec restore");
    cprintf("PASS: stvec restored after independent experiment\n");
    finish();
}
void kern_init(void) __attribute__((noreturn));
void kern_init(void) {
    extern char edata[], end[];
    for (volatile char *p = edata; p < end; ++p) *p = 0;
    intr_disable();
    write_csr(sie, 0);
    idt_init();
    cprintf("MEMBER-B isolated case=%u; real trap.c + A trapentry.S; no PMM\n", TEST_CASE);
    if (TEST_CASE == 1) synthetic();
    else if (TEST_CASE == 5) sync_run();
    else if (TEST_CASE == 6) vector_run();
    else timer_run();
    finish();
}
