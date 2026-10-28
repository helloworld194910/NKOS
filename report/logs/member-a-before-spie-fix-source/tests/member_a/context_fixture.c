/* Isolated test fixture; the production trap.c is not linked or changed. */
#include <defs.h>
#include <riscv.h>
#include <trap.h>

volatile uintptr_t member_a_expected_sp;
uintptr_t member_a_snapshot_word[32], member_a_snapshot_half[32];
uintptr_t member_a_snapshot_edit[32];
static volatile unsigned phase, trap_count;
extern void __alltraps(void);
extern void member_a_probe_word(void), member_a_probe_half(void);
extern void member_a_probe_edit(void);
extern char member_a_break_word[], member_a_break_half[], member_a_break_edit[];
extern char member_a_resume_word[], member_a_resume_half[], member_a_resume_edit[];
typedef char frame_size_check[sizeof(struct trapframe) == 288 ? 1 : -1];
typedef char gpr_size_check[sizeof(struct pushregs) == 256 ? 1 : -1];

static void sbi(unsigned long id, unsigned long ch) {
    register unsigned long a0 asm("a0") = ch;
    register unsigned long a1 asm("a1") = 0;
    register unsigned long a2 asm("a2") = 0;
    register unsigned long a7 asm("a7") = id;
    asm volatile("ecall" : "+r"(a0), "+r"(a1), "+r"(a2) : "r"(a7) : "memory");
}
static void puts_test(const char *s) {
    while (*s) sbi(1, (unsigned char)*s++);
}
static void hex(uintptr_t v) {
    const char *digits = "0123456789abcdef";
    puts_test("0x");
    for (int shift = 60; shift >= 0; shift -= 4) sbi(1, digits[(v >> shift) & 15]);
}
static void stop(void) __attribute__((noreturn));
static void stop(void) {
    sbi(8, 0);
    for (;;) asm volatile("wfi");
}
static void require(int ok, const char *what, uintptr_t got) {
    if (ok) return;
    puts_test("\nFAIL member-A phase="); hex(phase);
    puts_test(" "); puts_test(what); puts_test(" got="); hex(got);
    puts_test("\n"); stop();
}
static uintptr_t expected(unsigned i, int edited) {
    if (!i) return 0;
    if (i == 2) return member_a_expected_sp;
    if (edited && i == 5) return 0x5505;
    if (edited && i == 10) return 0x660a;
    if (edited && i == 18) return 0x7712;
    return 0x400 + i;
}
static void check_regs(const uintptr_t *regs, int edited) {
    for (unsigned i = 0; i < 32; ++i) {
        if (regs[i] != expected(i, edited)) {
            puts_test("register x"); hex(i);
            require(0, "GPR mismatch", regs[i]);
        }
    }
}
void trap(struct trapframe *tf) {
    require(phase < 3, "unexpected phase", phase);
    require(tf->cause == 3, "unexpected cause", tf->cause);
    require((tf->status & SSTATUS_SPP) != 0, "trap not from S mode", tf->status);
    require((tf->status & SSTATUS_SIE) == 0, "SIE on in handler", tf->status);
    require((uintptr_t)tf + sizeof(*tf) == member_a_expected_sp, "frame/SP mismatch", (uintptr_t)tf);
    require(read_csr(sscratch) == 0, "sscratch not cleared", read_csr(sscratch));
    check_regs((const uintptr_t *)&tf->gpr, 0);
    const char *origin = phase == 0 ? member_a_break_word :
                         phase == 1 ? member_a_break_half : member_a_break_edit;
    require(tf->epc == (uintptr_t)origin, "wrong saved epc", tf->epc);
    ++trap_count;
    tf->epc = (uintptr_t)(phase == 0 ? member_a_resume_word :
                         phase == 1 ? member_a_resume_half : member_a_resume_edit);
    if (phase == 2) {
        tf->gpr.t0 = 0x5505;
        tf->gpr.a0 = 0x660a;
        tf->gpr.s2 = 0x7712;
        tf->status |= SSTATUS_SPIE;
    }
}
void kern_init(void) __attribute__((noreturn));
void kern_init(void) {
    extern char edata[], end[];
    for (volatile char *p = edata; p < end; ++p) *p = 0;
    write_csr(sie, 0);
    clear_csr(sstatus, SSTATUS_SIE);
    write_csr(sscratch, 0);
    write_csr(stvec, (uintptr_t)__alltraps);
    puts_test("MEMBER-A isolated context validation; real trapentry.S\n");
    for (phase = 0; phase < 3; ++phase) {
        clear_csr(sstatus, SSTATUS_SIE);
        if (phase == 0) member_a_probe_word();
        else if (phase == 1) member_a_probe_half();
        else member_a_probe_edit();
        uintptr_t status = read_csr(sstatus);
        require(((status & SSTATUS_SIE) != 0) == (phase == 2), "sret SIE mismatch", status);
        require((status & SSTATUS_SPIE) != 0, "sret SPIE mismatch", status);
        clear_csr(sstatus, SSTATUS_SIE);
        check_regs(phase == 0 ? member_a_snapshot_word :
                   phase == 1 ? member_a_snapshot_half : member_a_snapshot_edit, phase == 2);
        require(trap_count == phase + 1, "trap count mismatch", trap_count);
        require(read_csr(sscratch) == 0, "scratch leaked", read_csr(sscratch));
        puts_test(phase == 0 ? "PASS: 32-bit breakpoint and all GPRs\n" :
                  phase == 1 ? "PASS: 16-bit breakpoint and all GPRs\n" :
                  "PASS: handler edits GPRs, epc and SPIE\n");
    }
    puts_test("RESULT: 3/3 context scenarios passed\n");
    stop();
}
