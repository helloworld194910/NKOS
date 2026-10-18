#include <default_pmm.h>
#include <best_fit_pmm.h>
#ifdef LAB2_BUDDY
#include <buddy_pmm.h>
#endif
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
#ifdef LAB2_BUDDY
    pmm_manager = &buddy_pmm_manager;
#else
    pmm_manager = &best_fit_pmm_manager;
#endif
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

//lab2 function1 2411824
static void page_init(void) {
    extern char end[];

    // 虚拟地址到物理地址的偏移量，必须与启动页表一致。
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    uint64_t mem_base = get_memory_base();
    uint64_t mem_size = get_memory_size();

    cprintf("page_init: probed RAM base=0x%lx size=0x%lx\n",
            (unsigned long)mem_base, (unsigned long)mem_size);

    // 设备树信息无效时直接报错，不得使用写死的默认值继续初始化。
    if (mem_base == 0) {
        panic("page_init: device tree reports memory base 0");
    }
    if (mem_size == 0) {
        panic("page_init: device tree reports memory size 0");
    }
    // 计算 RAM 末端前先检查加法溢出。
    if (mem_base > ~(uint64_t)0 - mem_size) {
        panic("page_init: memory base + size overflows");
    }
    uint64_t ram_end = mem_base + mem_size;

    // 当前采用以固定 DRAM_BASE 为基准的单段 RAM 模型，不同起点直接拒绝。
    if (mem_base != DRAM_BASE) {
        panic("page_init: unsupported RAM base 0x%lx (expect 0x%lx)",
              (unsigned long)mem_base, (unsigned long)DRAM_BASE);
    }

    // RAM 必须覆盖内核镜像的实际占用范围。
    uintptr_t kernel_end_pa = PADDR((uintptr_t)end);
    if (ram_end <= kernel_end_pa) {
        panic("page_init: RAM does not cover the kernel image");
    }

    // 实际可管理末端：裁剪到 PHYSICAL_MEMORY_END，并向下按页对齐。
    uint64_t managed_end = ram_end;
    if (managed_end > PHYSICAL_MEMORY_END) {
        cprintf("page_init: trim RAM end 0x%lx to PHYSICAL_MEMORY_END 0x%lx\n",
                (unsigned long)ram_end, (unsigned long)PHYSICAL_MEMORY_END);
        managed_end = PHYSICAL_MEMORY_END;
    }
    managed_end = ROUNDDOWN(managed_end, PGSIZE);

    npage = PPN(managed_end);
    if (npage <= nbase) {
        panic("page_init: no manageable physical pages (npage <= nbase)");
    }

    // 描述符数组紧跟在按页对齐的内核末端之后。
    pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE);
    uintptr_t pages_pa = PADDR((uintptr_t)pages);

    size_t descriptor_count = npage - nbase;
    if (descriptor_count > ~(size_t)0 / sizeof(struct Page)) {
        panic("page_init: descriptor array size overflows");
    }
    size_t descriptor_bytes = descriptor_count * sizeof(struct Page);

    // 描述符数组必须完整落在实际 RAM 和可管理范围内。
    if (pages_pa > managed_end) {
        panic("page_init: descriptor array starts beyond managed memory");
    }
    if (descriptor_bytes > managed_end - pages_pa) {
        panic("page_init: descriptor array does not fit in RAM");
    }
    uintptr_t descriptor_end_pa = pages_pa + descriptor_bytes;

    // 确定性初始化全部描述符，初始状态统一为保留。
    for (size_t i = 0; i < descriptor_count; i++) {
        pages[i].ref = 0;
        pages[i].flags = 0;
        pages[i].property = 0;
        list_init(&pages[i].page_link);
        SetPageReserved(&pages[i]);
    }

    // 初始空闲区：描述符数组末端按页对齐之后，到可管理末端。
    uintptr_t free_begin = ROUNDUP(descriptor_end_pa, PGSIZE);
    uintptr_t free_end = managed_end;

    cprintf("page_init: npage=%lu nbase=%lu descriptors=%lu\n",
            (unsigned long)npage, (unsigned long)nbase,
            (unsigned long)descriptor_count);
    cprintf("page_init: pages_va=0x%lx pages_pa=0x%lx descriptor_end_pa=0x%lx\n",
            (unsigned long)(uintptr_t)pages, (unsigned long)pages_pa,
            (unsigned long)descriptor_end_pa);
    cprintf("page_init: free range [0x%lx, 0x%lx)\n",
            (unsigned long)free_begin, (unsigned long)free_end);

    if (free_begin < free_end) {
        size_t free_pages_count = (free_end - free_begin) / PGSIZE;
        cprintf("page_init: register %lu free pages\n",
                (unsigned long)free_pages_count);
        init_memmap(pa2page(free_begin), free_pages_count);
    }
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
