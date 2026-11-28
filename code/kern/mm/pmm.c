#include <default_pmm.h>
#include <defs.h>
#include <error.h>
#include <kmalloc.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <proc.h>
#include <vmm.h>
#include <sbi.h>
#include <dtb.h>
#include <stdio.h>
#include <string.h>
#include <sync.h>
#include <vmm.h>
#include <riscv.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// The kernel image is mapped at VA=KERNBASE and PA=info.base
uint_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
pde_t *boot_pgdir_va = NULL;
// physical address of boot-time page directory
uintptr_t boot_pgdir_pa;

// physical memory management
const struct pmm_manager *pmm_manager;

static void check_alloc_page(void);
static void check_pgdir(void);
static void check_boot_pgdir(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void)
{
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n)
{
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n)
{
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void)
{
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

//lab2 请补充你在lab2中的代码

// 沿用组内 Lab4 已验收的 Lab2 物理页初始化。
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

    // 实际可管理末端：裁剪到 managed_limit，并向下按页对齐。
    const uint64_t managed_limit = (uintptr_t)KERNTOP - va_pa_offset;
    uint64_t managed_end = ram_end;
    if (managed_end > managed_limit) {
        cprintf("page_init: trim RAM end 0x%lx to managed_limit 0x%lx\n",
                (unsigned long)ram_end, (unsigned long)managed_limit);
        managed_end = managed_limit;
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
        list_init(&pages[i].pra_page_link);
        pages[i].pra_vaddr = 0;
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

// boot_map_segment - setup&enable the paging mechanism
// parameters
//  la:   linear address of this memory need to map (after x86 segment map)
//  size: memory size
//  pa:   physical address of this memory
//  perm: permission of this memory
static void boot_map_segment(pde_t *pgdir, uintptr_t la, size_t size,
                             uintptr_t pa, uint32_t perm)
{
    assert(PGOFF(la) == PGOFF(pa));
    size_t n = ROUNDUP(size + PGOFF(la), PGSIZE) / PGSIZE;
    la = ROUNDDOWN(la, PGSIZE);
    pa = ROUNDDOWN(pa, PGSIZE);
    for (; n > 0; n--, la += PGSIZE, pa += PGSIZE)
    {
        pte_t *ptep = get_pte(pgdir, la, 1);
        assert(ptep != NULL);
        *ptep = pte_create(pa >> PGSHIFT, PTE_V | perm);
    }
}

// boot_alloc_page - allocate one page using pmm->alloc_pages(1)
// return value: the kernel virtual address of this allocated page
// note: this function is used to get the memory for PDT(Page Directory
// Table)&PT(Page Table)
static void *boot_alloc_page(void)
{
    struct Page *p = alloc_page();
    if (p == NULL)
    {
        panic("boot_alloc_page failed.\n");
    }
    return page2kva(p);
}

// pmm_init - setup a pmm to manage physical memory, build PDT&PT to setup
// paging mechanism
//         - check the correctness of pmm & paging mechanism, print PDT&PT
void pmm_init(void)
{
    // We need to alloc/free the physical memory (granularity is 4KB or other
    // size).
    // So a framework of physical memory manager (struct pmm_manager)is defined
    // in pmm.h
    // First we should init a physical memory manager(pmm) based on the
    // framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a
    // pmm
    check_alloc_page();

    // create boot_pgdir, an initial page directory(Page Directory Table, PDT)
    extern char boot_page_table_sv39[];
    boot_pgdir_va = (pte_t *)boot_page_table_sv39;
    boot_pgdir_pa = PADDR(boot_pgdir_va);

    check_pgdir();

    static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0);

    // now the basic virtual memory map(see memalyout.h) is established.
    // check the correctness of the basic virtual memory map.
    check_boot_pgdir();

    kmalloc_init();
}

// get_pte - get pte and return the kernel virtual address of this pte for la
//        - if the PT contians this pte didn't exist, alloc a page for PT
// parameter:
//  pgdir:  the kernel virtual base address of PDT
//  la:     the linear address need to map
//  create: a logical value to decide if alloc a page for PT
// return vaule: the kernel virtual address of this pte
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    pde_t *pdep1 = &pgdir[PDX1(la)];
    if (!(*pdep1 & PTE_V))
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep1 = ptd_create(page2ppn(page));
    }

    pde_t *pdep0 = &((pde_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
    if (!(*pdep0 & PTE_V))
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep0 = ptd_create(page2ppn(page));
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}

// get_page - get related Page struct for linear address la using PDT pgdir
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep_store != NULL)
    {
        *ptep_store = ptep;
    }
    if (ptep != NULL && *ptep & PTE_V)
    {
        return pte2page(*ptep);
    }
    return NULL;
}

// page_remove_pte - free an Page sturct which is related linear address la
//                - and clean(invalidate) pte which is related linear address la
// note: PT is changed, so the TLB need to be invalidate
static inline void page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep)
{
    if (*ptep & PTE_V)
    {
        struct Page *page = pte2page(*ptep);
        page_ref_dec(page);
        if (page_ref(page) == 0)
        {
            free_page(page);
        }
        *ptep = 0;
        tlb_invalidate(pgdir, la);
    }
}

void unmap_range(pde_t *pgdir, uintptr_t start, uintptr_t end)
{
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));

    do
    {
        pte_t *ptep = get_pte(pgdir, start, 0);
        if (ptep == NULL)
        {
            start = ROUNDDOWN(start + PTSIZE, PTSIZE);
            continue;
        }
        if (*ptep != 0)
        {
            page_remove_pte(pgdir, start, ptep);
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
}

void exit_range(pde_t *pgdir, uintptr_t start, uintptr_t end)
{
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));

    uintptr_t d1start, d0start;
    int free_pt, free_pd0;
    pde_t *pd0, *pt, pde1, pde0;
    d1start = ROUNDDOWN(start, PDSIZE);
    d0start = ROUNDDOWN(start, PTSIZE);
    do
    {
        // level 1 page directory entry
        pde1 = pgdir[PDX1(d1start)];
        // if there is a valid entry, get into level 0
        // and try to free all page tables pointed to by
        // all valid entries in level 0 page directory,
        // then try to free this level 0 page directory
        // and update level 1 entry
        if (pde1 & PTE_V)
        {
            pd0 = page2kva(pde2page(pde1));
            // try to free all page tables
            free_pd0 = 1;
            do
            {
                pde0 = pd0[PDX0(d0start)];
                if (pde0 & PTE_V)
                {
                    pt = page2kva(pde2page(pde0));
                    // try to free page table
                    free_pt = 1;
                    for (int i = 0; i < NPTEENTRY; i++)
                        if (pt[i] & PTE_V)
                        {
                            free_pt = 0;
                            break;
                        }
                    // free it only when all entry are already invalid
                    if (free_pt)
                    {
                        free_page(pde2page(pde0));
                        pd0[PDX0(d0start)] = 0;
                    }
                }
                else
                    free_pd0 = 0;
                d0start += PTSIZE;
            } while (d0start != 0 && d0start < d1start + PDSIZE && d0start < end);
            // free level 0 page directory only when all pde0s in it are already invalid
            if (free_pd0)
            {
                free_page(pde2page(pde1));
                pgdir[PDX1(d1start)] = 0;
            }
        }
        d1start += PDSIZE;
        d0start = d1start;
    } while (d1start != 0 && d1start < end);
}


//lab5 function3 2413921: copy_range — 按页把 [start, end) 的映射复制到目标页表。
// 只处理已经存在的叶子映射，源页表缺二级表时整段跳过。
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share)
{
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));

    // 只沿用硬件可见的权限位；页表项中的软件位（如 COW 标记）不随页复制。
    const uint32_t perm_mask = PTE_U | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D;

    do
    {
        pte_t *ptep = get_pte(from, start, 0);
        if (ptep == NULL)
        {
            // 这一段地址没有二级页表，对应的 4 MiB 全都没有映射。
            start = ROUNDDOWN(start + PTSIZE, PTSIZE);
            continue;
        }
        if (*ptep & PTE_V)
        {
            struct Page *page = pte2page(*ptep);
            uint32_t perm = (*ptep & perm_mask);

            // 基础版：分配新页并复制内容，父子各持有一份独立的物理页。
            // 共享（COW）路径在 COW 提交中接入，届时父页也要同步降权。
            struct Page *npage = alloc_page();
            if (npage == NULL)
            {
                return -E_NO_MEM;
            }
            memcpy(page2kva(npage), page2kva(page), PGSIZE);
            if (page_insert(to, npage, start, perm) != 0)
            {
                free_page(npage);
                return -E_NO_MEM;
            }
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
    return 0;
}


//lab4 请补充你在lab4中的代码

// 保留映射引用计数、TLB 同步和部分分配失败的回滚。
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
{
    pde_t *root_entry = &pgdir[PDX1(la)];
    pde_t old_root = *root_entry;
    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL) {
        // get_pte may allocate the first table before the second allocation
        // fails. Only undo a table created by this call, never an old table.
        if (!(old_root & PTE_V) && (*root_entry & PTE_V)) {
            struct Page *table = pde2page(*root_entry);
            *root_entry = old_root;
            tlb_invalidate(pgdir, la);
            set_page_ref(table, 0);
            free_page(table);
        }
        return -E_NO_MEM;
    }

    // Acquire the new reference first: replacing a page with itself must
    // not momentarily reduce its reference count to zero and free it.
    page_ref_inc(page);
    if (*ptep & PTE_V) {
        struct Page *old_page = pte2page(*ptep);
        if (old_page == page) {
            page_ref_dec(page);
        } else {
            page_remove(pgdir, la);
        }
    }
    *ptep = pte_create(page2ppn(page), perm | PTE_V);
    tlb_invalidate(pgdir, la);
    return 0;
}

void page_remove(pde_t *pgdir, uintptr_t la)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep == NULL || !(*ptep & PTE_V)) {
        return;
    }
    struct Page *page = pte2page(*ptep);
    *ptep = 0;
    tlb_invalidate(pgdir, la);
    if (page_ref_dec(page) == 0) {
        free_page(page);
    }
    // Empty intermediate page tables remain owned by the address space.
}


// invalidate a TLB entry, but only if the page tables being
// edited are the ones currently in use by the processor.
void tlb_invalidate(pde_t *pgdir, uintptr_t la)
{
    asm volatile("sfence.vma %0" : : "r"(la));
}

// pgdir_alloc_page - call alloc_page & page_insert functions to
//                  - allocate a page size memory & setup an addr map
//                  - pa<->la with linear address la and the PDT pgdir
struct Page *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm)
{
    struct Page *page = alloc_page();
    if (page != NULL)
    {
        if (page_insert(pgdir, page, la, perm) != 0)
        {
            free_page(page);
            return NULL;
        }
        // swap_map_swappable(check_mm_struct, la, page, 0);
        page->pra_vaddr = la;
        assert(page_ref(page) == 1);
        // cprintf("get No. %d  page: pra_vaddr %x, pra_link.prev %x,
        // pra_link_next %x in pgdir_alloc_page\n", (page-pages),
        // page->pra_vaddr,page->pra_page_link.prev,
        // page->pra_page_link.next);
    }

    return page;
}

static void check_alloc_page(void)
{
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

static void check_pgdir(void)
{
    // assert(npage <= KMEMSIZE / PGSIZE);
    // The memory starts at 2GB in RISC-V
    // so npage is always larger than KMEMSIZE / PGSIZE
    size_t nr_free_store;

    nr_free_store = nr_free_pages();

    assert(npage <= KERNTOP / PGSIZE);
    assert(boot_pgdir_va != NULL && (uint32_t)PGOFF(boot_pgdir_va) == 0);
    assert(get_page(boot_pgdir_va, 0x0, NULL) == NULL);

    struct Page *p1, *p2;
    p1 = alloc_page();
    assert(page_insert(boot_pgdir_va, p1, 0x0, 0) == 0);

    pte_t *ptep;
    assert((ptep = get_pte(boot_pgdir_va, 0x0, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert(page_ref(p1) == 1);

    ptep = (pte_t *)KADDR(PDE_ADDR(boot_pgdir_va[0]));
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
    assert(get_pte(boot_pgdir_va, PGSIZE, 0) == ptep);

    p2 = alloc_page();
    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(*ptep & PTE_U);
    assert(*ptep & PTE_W);
    pde_t root_entry = boot_pgdir_va[0];
    pde_t middle_entry = ((pde_t *)KADDR(PDE_ADDR(root_entry)))[0];
    assert((root_entry & (PTE_V | PTE_R | PTE_W | PTE_X | PTE_U | PTE_A | PTE_D)) == PTE_V);
    assert((middle_entry & (PTE_V | PTE_R | PTE_W | PTE_X | PTE_U | PTE_A | PTE_D)) == PTE_V);
    assert(page_ref(p2) == 1);

    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2);
    assert(page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert((*ptep & PTE_U) == 0);

    page_remove(boot_pgdir_va, 0x0);
    assert(page_ref(p1) == 1);
    assert(page_ref(p2) == 0);

    page_remove(boot_pgdir_va, PGSIZE);
    assert(page_ref(p1) == 0);
    assert(page_ref(p2) == 0);

    assert(page_ref(pde2page(boot_pgdir_va[0])) == 1);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_pgdir() succeeded!\n");
}

static void check_boot_pgdir(void)
{
    size_t nr_free_store;
    nr_free_store = nr_free_pages();

    const uintptr_t root_offset_mask = (1UL << 30) - 1;
    for (uintptr_t pa = DRAM_BASE; pa < npage * PGSIZE; pa += PGSIZE)
    {
        uintptr_t va = (uintptr_t)KADDR(pa);
        pte_t leaf = boot_pgdir_va[PDX1(va)];
        assert((leaf & (PTE_V | PTE_R)) == (PTE_V | PTE_R));
        assert((PTE_ADDR(leaf) & root_offset_mask) == 0);
        assert(PTE_ADDR(leaf) + (va & root_offset_mask) == pa);
    }

    assert(boot_pgdir_va[0] == 0);

    struct Page *p;
    p = alloc_page();
    assert(page_insert(boot_pgdir_va, p, 0x100, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 1);
    assert(page_insert(boot_pgdir_va, p, 0x100 + PGSIZE, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 2);

    const char *str = "ucore: Hello world!!";
    strcpy((void *)0x100, str);
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);

    *(char *)(page2kva(p) + 0x100) = '\0';
    assert(strlen((const char *)0x100) == 0);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(p);
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_boot_pgdir() succeeded!\n");
}

// perm2str - use string 'u,r,w,-' to present the permission
static const char *perm2str(int perm)
{
    static char str[4];
    str[0] = (perm & PTE_U) ? 'u' : '-';
    str[1] = 'r';
    str[2] = (perm & PTE_W) ? 'w' : '-';
    str[3] = '\0';
    return str;
}

// get_pgtable_items - In [left, right] range of PDT or PT, find a continuous
// linear addr space
//                  - (left_store*X_SIZE~right_store*X_SIZE) for PDT or PT
//                  - X_SIZE=PTSIZE=4M, if PDT; X_SIZE=PGSIZE=4K, if PT
// paramemters:
//  left:        no use ???
//  right:       the high side of table's range
//  start:       the low side of table's range
//  table:       the beginning addr of table
//  left_store:  the pointer of the high side of table's next range
//  right_store: the pointer of the low side of table's next range
//  return value: 0 - not a invalid item range, perm - a valid item range with
//  perm permission
static int get_pgtable_items(size_t left, size_t right, size_t start,
                             uintptr_t *table, size_t *left_store,
                             size_t *right_store)
{
    if (start >= right)
    {
        return 0;
    }
    while (start < right && !(table[start] & PTE_V))
    {
        start++;
    }
    if (start < right)
    {
        if (left_store != NULL)
        {
            *left_store = start;
        }
        int perm = (table[start++] & PTE_USER);
        while (start < right && (table[start] & PTE_USER) == perm)
        {
            start++;
        }
        if (right_store != NULL)
        {
            *right_store = start;
        }
        return perm;
    }
    return 0;
}

//lab5 function1 2412790: show the present 4 KiB mappings of the current user VMAs.
void
print_pgdir(void)
{
    if (current == NULL || current->mm == NULL || current->mm->pgdir == NULL)
    {
        return;
    }
    struct mm_struct *mm = current->mm;
    list_entry_t *head = &mm->mmap_list;
    for (list_entry_t *le = list_next(head); le != head; le = list_next(le))
    {
        struct vma_struct *vma = le2vma(le, list_link);
        uintptr_t start = ROUNDDOWN(vma->vm_start, PGSIZE);
        for (uintptr_t va = start; va < vma->vm_end; va += PGSIZE)
        {
            pte_t *pte = get_pte(mm->pgdir, va, 0);
            if (pte != NULL && (*pte & (PTE_V | PTE_U)) == (PTE_V | PTE_U))
            {
                cprintf("  %p %c%c%c\n", (void *)va, (*pte & PTE_R) ? 'r' : '-',
                        (*pte & PTE_W) ? 'w' : '-', (*pte & PTE_X) ? 'x' : '-');
            }
        }
    }
}
