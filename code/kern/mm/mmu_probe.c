/* lab2 Member C: 2413921 */
#include <defs.h>
#include <stdio.h>

/* lab2 member C: known real guest data accesses, independent of page_init. */
volatile uint64_t mmu_probe_word __attribute__((aligned(4096))) = 0x1122334455667788ULL;
volatile uint64_t mmu_probe_results[2];

void __attribute__((noinline)) mmu_probe(void) {
    uintptr_t satp;
    asm volatile("csrr %0, satp" : "=r"(satp));
    cprintf("MMU probe: VA=%016lx satp=%016lx\n", (uintptr_t)&mmu_probe_word, satp);
    /* First load after sfence should refill; second load can hit SoftTLB. */
    asm volatile("sfence.vma\n"
                 ".global mmu_probe_load_first\n"
                 "mmu_probe_load_first:\n"
                 "ld %0, 0(%2)\n"
                 ".global mmu_probe_load_second\n"
                 "mmu_probe_load_second:\n"
                 "ld %1, 0(%2)\n"
                 : "=&r"(mmu_probe_results[0]), "=&r"(mmu_probe_results[1])
                 : "r"(&mmu_probe_word) : "memory");
}
