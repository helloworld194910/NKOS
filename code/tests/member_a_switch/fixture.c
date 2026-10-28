/* A-only validation fixture, NOT the final upper-layer challenge code. */
#include <defs.h>
#include <riscv.h>
#include <mmu.h>
#include <memlayout.h>
#include <trap.h>
#include <switch.h>
#define USER_CODE  0x400000ULL
#define USER_STACK 0x401000ULL
#define USER_TOP   0x402000ULL
static uint64_t root_table[512] __attribute__((aligned(4096)));
static uint64_t mid_table[512] __attribute__((aligned(4096)));
static uint64_t leaf_table[512] __attribute__((aligned(4096)));
static unsigned char user_stack_page[4096] __attribute__((aligned(4096)));
static unsigned char kernel_trap_stack[8192] __attribute__((aligned(16)));
static volatile unsigned test_case, traps;
extern char member_a_user_page[], member_a_user_page_end[];
extern uint64_t boot_page_table_sv39[];
extern long member_a_switch_abi_probe(uintptr_t, uintptr_t, uintptr_t);
typedef char frame_layout_check[sizeof(struct trapframe) == 288 ? 1 : -1];
static void sbi(unsigned long id, unsigned long value) {
    register unsigned long a0 asm("a0")=value;
    register unsigned long a1 asm("a1")=0;
    register unsigned long a2 asm("a2")=0;
    register unsigned long a7 asm("a7")=id;
    asm volatile("ecall" : "+r"(a0), "+r"(a1), "+r"(a2) : "r"(a7) : "memory");
}
static void out(const char *s) { while (*s) sbi(1,(unsigned char)*s++); }
static void hex(uintptr_t value) {
    const char *digits="0123456789abcdef";
    out("0x");
    for (int n=60;n>=0;n-=4) sbi(1,digits[(value>>n)&15]);
}
static void stop(void) __attribute__((noreturn));
static void stop(void) { sbi(8,0); for (;;) asm volatile("wfi"); }
static void check(int ok,const char *what,uintptr_t value) {
    if (ok) return;
    out("FAIL switch case=");hex(test_case);out(" ");out(what);out(" value=");hex(value);out("\n");stop();
}
static uintptr_t physical(const void *v) { return (uintptr_t)v-PHYSICAL_MEMORY_OFFSET; }
static uint64_t pte(const void *v,unsigned flags) { return ((physical(v)>>12)<<10)|flags; }
long member_a_user_trap(struct trapframe *tf) {
    check(tf->cause==8,"cause is not U ecall",tf->cause);
    check((tf->status&SSTATUS_SPP)==0,"SPP says S origin",tf->status);
    check((tf->status&SSTATUS_SIE)==0,"SIE on during callback",tf->status);
    uintptr_t low=(uintptr_t)kernel_trap_stack,high=low+sizeof(kernel_trap_stack);
    check((uintptr_t)tf>=low && (uintptr_t)tf<=high-sizeof(*tf),"frame outside kernel trap stack",(uintptr_t)tf);
    uintptr_t handler_sp;
    asm volatile("mv %0, sp":"=r"(handler_sp));
    check(handler_sp>=low && handler_sp<(uintptr_t)tf,"C running on wrong stack",handler_sp);
    check(tf->gpr.sp==USER_TOP-16,"wrong saved user sp",tf->gpr.sp);
    check(tf->epc>=USER_CODE && tf->epc<USER_CODE+4096,"epc not in user page",tf->epc);
    check(read_csr(sscratch)==0,"scratch not cleared",read_csr(sscratch));
    check(traps<2,"too many user traps",traps);
    out("OBSERVED: cause=");hex(tf->cause);out(" SPP=0 epc=");hex(tf->epc);
    out(" user_sp=");hex(tf->gpr.sp);out(" kernel_frame=");hex((uintptr_t)tf);out("\n");
    ++traps;
    if (traps==1) {
        check(tf->gpr.a7==0x5901 && tf->gpr.a0==0x1234,"bad first request",tf->gpr.a0);
        check(member_a_enter_user(0,0,0)==-2,"nested entry not rejected",0);
        tf->gpr.a0=0xabcd;
        tf->epc+=4; /* Explicit ecall instruction in this fixture is four bytes. */
        return MEMBER_A_SWITCH_RESUME;
    }
    check(tf->gpr.a7==0x5902 && tf->gpr.a0==0x5678,"bad finish request",tf->gpr.a0);
    return test_case==2 ? 7 : MEMBER_A_SWITCH_FINISH;
}
static void unexpected_kernel_trap(void) __attribute__((noreturn,aligned(4)));
static void unexpected_kernel_trap(void) { out("FAIL: unexpected kernel trap outside extension\n");stop(); }
void kern_init(void) __attribute__((noreturn));
void kern_init(void) {
    extern char edata[],end[];
    for (volatile char *p=edata;p<end;++p) *p=0;
    write_csr(sie,0);clear_csr(sstatus,SSTATUS_SIE);
    write_csr(stvec,(uintptr_t)unexpected_kernel_trap);
    write_csr(sscratch,0x5a5a);
    out("MEMBER-A isolated S-U-S fixture; upper-layer protocol is a proposal\n");
    check((uintptr_t)member_a_user_page_end-(uintptr_t)member_a_user_page==4096,"user code size",(uintptr_t)member_a_user_page_end);
    check(((uintptr_t)member_a_user_page&4095)==0,"user code alignment",(uintptr_t)member_a_user_page);
    root_table[511]=boot_page_table_sv39[511];
    check((root_table[511]&PTE_U)==0,"kernel mapping became user accessible",root_table[511]);
    root_table[0]=pte(mid_table,PTE_V);
    mid_table[2]=pte(leaf_table,PTE_V);
    leaf_table[0]=pte(member_a_user_page,PTE_V|PTE_R|PTE_X|PTE_U|PTE_A);
    leaf_table[1]=pte(user_stack_page,PTE_V|PTE_R|PTE_W|PTE_U|PTE_A|PTE_D);
    uintptr_t original_satp=read_csr(satp);
    uintptr_t test_satp=(8ULL<<60)|(physical(root_table)>>12);
    write_csr(satp,test_satp);asm volatile("sfence.vma":::"memory");
    for (test_case=0;test_case<3;++test_case) {
        traps=0;
        uintptr_t before_vec=read_csr(stvec),before_scratch=read_csr(sscratch);
        uintptr_t before_status=read_csr(sstatus),before_epc=read_csr(sepc),before_sie=read_csr(sie);
        long rc=member_a_switch_abi_probe(USER_CODE,USER_TOP,(uintptr_t)kernel_trap_stack+sizeof(kernel_trap_stack));
        uintptr_t after_status=read_csr(sstatus);
        check(rc==(test_case==2 ? -1 : 0),"return value or callee-saved register mismatch",(uintptr_t)rc);
        check(traps==2,"wrong ecall count",traps);
        check(read_csr(stvec)==before_vec,"stvec not restored",read_csr(stvec));
        check(read_csr(sscratch)==before_scratch,"sscratch not restored",read_csr(sscratch));
        check(after_status==before_status,"sstatus not restored",after_status);
        check(read_csr(sepc)==before_epc,"sepc not restored",read_csr(sepc));
        check(read_csr(sie)==before_sie,"sie not restored",read_csr(sie));
        check(read_csr(satp)==test_satp,"bottom layer changed satp",read_csr(satp));
        out(test_case==0 ? "PASS: S-U-S with user resume and kernel context restoration\n" :
            test_case==1 ? "PASS: repeated S-U-S and nested-entry rejection\n" :
                           "PASS: invalid callback action restores kernel context\n");
    }
    write_csr(satp,original_satp);asm volatile("sfence.vma":::"memory");
    check(read_csr(satp)==original_satp,"test did not restore satp",read_csr(satp));
    out("RESULT: 3/3 switch cases passed\n");stop();
}
