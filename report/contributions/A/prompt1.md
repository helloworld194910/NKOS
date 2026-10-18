[PROMPT]

**任务**：
在当前仓库 `code/kern/mm/pmm.c` 中，标有
`//lab2 function1 YOUR CODE`
的位置，实现完整的：

```c
static void page_init(void);
```

该函数要实现的功能为：
根据设备树提供的物理内存信息建立可管理物理页的描述符布局，并把排除 OpenSBI、内核镜像和描述符数组占用空间后的完整空闲页面交给当前pmm_manager管理。具体为
读取 RAM 的起点和大小，确定内核可管理的物理页范围，在内核镜像之后建立 Page 描述符数组，保留 OpenSBI、内核及描述符数组占用的页面，最后将剩余完整页面交给当前 pmm_manager 管理。

本任务只实现物理内存初始化，不实现 Best-Fit，不切换当前管理器，不重新建立启动页表。

**操作要求**：
1. 必须修改工程中的真实文件，不能只在回复中展示代码或补丁。
2. 先阅读当前工程相关源码，再实施修改，不得依据其他版本 ucore 的代码猜测接口。
3. 仓库根目录下的目标文件是 `code/kern/mm/pmm.c`；
   如果工作目录已经是 `code`，对应路径为 `kern/mm/pmm.c`。
4. 只允许在目标标记处新增 page_init()，以及确有必要的文件内 static 辅助函数。
5. 保留原有头文件包含、已有注释和其他函数。
6. 保留 `lab2` 注释；仅在已提供真实学号时，将该标记中的
   `YOUR CODE` 替换为该学号。未提供学号时不得编造。我的学号是2411824.
7. 不得修改其他文件，不得修改测试，不得绕过或关闭断言。
8. 不得修改 init_pmm_manager() 中当前管理器的选择。
9. 不得提交或推送 Git，除非另有明确要求。

**输出要求**：
完成真实文件修改后，简要报告：
1. 修改了哪个文件、实现了哪些函数。
2. va_pa_offset、npage、pages 的计算依据。
3. 描述符数组与初始空闲区的布局关系。
4. 实际执行的验证命令、结果和未执行项目。
5. 若验证被框架中尚未完成的 Best-Fit 模块阻塞，准确说明原因，
   不得通过修改其他模块或测试来绕过。
6. 若源码、作业要求和测试存在冲突，指出具体冲突及处理依据，
   不得悄悄修改测试或放宽内存安全约束。

[RELY]

## 1. 必须阅读的真实文件

```text
code/kern/mm/pmm.c
code/kern/mm/pmm.h
code/kern/mm/memlayout.h
code/kern/mm/mmu.h
code/kern/driver/dtb.c
code/kern/driver/dtb.h
code/kern/init/entry.S
code/kern/init/init.c
code/libs/defs.h
code/libs/riscv.h
code/kern/debug/assert.h
code/tools/kernel.ld
code/tools/test_lab2.c
```

如果当前工程发生变化，应以实际声明核对接口。
本任务的修改范围不能因为接口差异而自动扩大；如确需改动其他文件，
先说明具体原因与所需改动。

## 2. 当前执行环境

entry.S 已经设置启动页表并开启 Sv39，再进入 kern_init()。

kern_init() 先调用 dtb_init()，随后调用 pmm_init()。

pmm_init() 的关键顺序是：

```c
init_pmm_manager();
page_init();
check_alloc_page();
```

因此：
- page_init() 执行时已经处于分页模式。
- 当前 pmm_manager 的 init() 已经执行。
- page_init() 不应再次初始化管理器或开启分页。
- 本函数只负责建立描述符布局并注册初始空闲区。

## 3. 设备树提供的接口

由 dtb.c、dtb.h 提供：

```c
uint64_t get_memory_base(void);
uint64_t get_memory_size(void);
```

它们返回设备树解析出的 RAM 起点和字节数。
解析失败时，这些信息可能为零。

不得在设备树信息无效时静默使用写死的默认内存大小。

## 4. 内存布局常量

当前 memlayout.h 定义：

```c
#define KERNBASE                0xFFFFFFFFC0200000
#define KMEMSIZE                0x7E00000
#define KERNTOP                 (KERNBASE + KMEMSIZE)

#define PHYSICAL_MEMORY_END     0x88000000
#define PHYSICAL_MEMORY_OFFSET  0xFFFFFFFF40000000
#define KERNEL_BEGIN_PADDR      0x80200000
#define KERNEL_BEGIN_VADDR      0xFFFFFFFFC0200000
```

DRAM_BASE 由 libs/riscv.h 提供，当前为 0x80000000。

这些常量表示当前平台和映射约定。
必须使用工程常量，不能在实现中重复散布对应数值。

设备树报告范围与内核管理范围不同：
实际可管理末端不得超过 PHYSICAL_MEMORY_END。

va_pa_offset 应与启动映射一致，即 PHYSICAL_MEMORY_OFFSET。
不得简单用 KERNBASE 减去设备树 RAM 起点计算偏移：
KERNBASE 对应的是内核物理起点，不是 DRAM 起点。

## 5. 全局变量与页号约定

pmm.c 已定义：

```c
struct Page *pages;
size_t npage = 0;
uint64_t va_pa_offset;
const size_t nbase = DRAM_BASE / PGSIZE;
```

含义：

```text
pages：Page 描述符数组的内核虚拟地址。
nbase：DRAM 起点对应的物理页号。
npage：可管理物理页号的上界，不是 RAM 页面总数。
描述符数量：npage - nbase。
```

pmm.h 已提供：

```c
static inline ppn_t page2ppn(struct Page *page);
static inline uintptr_t page2pa(struct Page *page);
static inline struct Page *pa2page(uintptr_t pa);
static inline void set_page_ref(struct Page *page, int val);
```

转换关系：

```text
page2ppn(page) = page - pages + nbase
page2pa(page)  = page2ppn(page) << PGSHIFT
pa2page(pa)    = &pages[PPN(pa) - nbase]
```

当前 pa2page() 只检查上界。
调用者必须保证 PPN(pa) >= nbase，避免数组下标下溢。

PADDR(kva) 是已有宏：
- 接收内核虚拟地址；
- 返回 kva - va_pa_offset；
- 依赖 va_pa_offset 已正确设置。

KADDR 宏在当前 pmm.h 中被注释掉，不得假设它可以使用。

## 6. 页面与对齐接口

mmu.h 提供：

```c
#define PGSIZE  4096
#define PGSHIFT 12
#define PPN(la) (((uintptr_t)(la)) >> PGSHIFT)
```

libs/defs.h 提供：

```c
ROUNDUP(a, n)
ROUNDDOWN(a, n)
```

ROUNDUP 内部存在加法。
对不可信值或接近整数上界的数值，必须先检查溢出。

memlayout.h 提供 struct Page 和页面状态宏：

```c
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

SetPageReserved(page)
ClearPageReserved(page)
PageReserved(page)
SetPageProperty(page)
ClearPageProperty(page)
PageProperty(page)
```

## 7. 内核末端和空闲区注册

链接脚本提供：

```c
extern char end[];
```

end 是内核镜像末端的虚拟地址。

pmm.c 已提供：

```c
static void init_memmap(struct Page *base, size_t n);
```

该函数转发至当前管理器的 init_memmap。

必须先完成描述符数组初始化，再调用 init_memmap()。

panic(...) 是 assert.h 中提供的宏，最终调用不返回的 __panic()。
不要新增一个与其冲突的 panic 函数声明。

[GUARANTEE]

## 必须实现的接口

```c
static void page_init(void);
```

该函数必须放在 pmm_init() 之前的指定标记位置，
使 pmm_init() 对 page_init() 的调用具有正确的声明可见性。

## 必须正确建立的状态

```c
va_pa_offset
npage
pages
```

并完成：
- 全部受管理物理页对应描述符的初始化；
- 保留状态设置；
- 非空初始空闲区的 init_memmap() 注册。

## 允许修改的范围

仅允许：
- 在 pmm.c 指定标记处新增 page_init()；
- 在同一位置新增必要的 static 辅助函数；
- 按已提供真实学号替换本任务对应的 YOUR CODE。

## 禁止修改的范围

不得修改：
- pmm.h、memlayout.h、mmu.h、defs.h 等接口及结构体；
- dtb.c、dtb.h 的设备树解析逻辑；
- entry.S 和链接脚本；
- default_pmm.c、best_fit_pmm.c；
- init_pmm_manager()、pmm_init() 和分配释放包装函数；
- 任何现有测试、评分脚本或 Makefile。

不得：
- 使用 malloc 等宿主或动态内存分配接口；
- 将未验证的地址当作描述符数组写入；
- 将内核、OpenSBI 或描述符存储页交给分配器；
- 为通过测试而硬编码空闲页数或伪造成功输出。

[SPECIFICATION]

## page_init 函数

**Pre-Condition**：
1. 已按当前工程流程进入内核分页执行环境。
2. dtb_init() 已执行，但其结果仍需验证。
3. init_pmm_manager() 已执行，当前管理器已完成内部初始化。
4. end 由当前链接脚本提供。
5. 当前工程采用以固定 DRAM_BASE 为基准的单段 RAM 描述符模型。

**Post-Condition：成功返回时**
1. va_pa_offset == PHYSICAL_MEMORY_OFFSET。
2. npage 为裁剪、向下按页对齐后的可管理物理末端对应页号。
3. npage > nbase。
4. pages 位于 end 之后，并按 PGSIZE 对齐。
5. 描述符数组覆盖 [nbase, npage) 的所有物理页。
6. 描述符数组自身完整位于有效 RAM 和当前可管理范围内。
7. 保留区域的描述符保持 PageReserved。
8. 只有真实可用的完整页面被交给 init_memmap()。
9. 若不存在完整空闲页，不调用 init_memmap()。

**Case 1：正常 RAM 范围，存在完整空闲页**
- 设备树报告有效 RAM 起点和大小。
- RAM 覆盖当前框架要求的 DRAM 基址、内核和描述符存储区域。
- 描述符数组之后至少有一个完整页面。
- 正确建立全部描述符，调用一次 init_memmap() 注册空闲区。

**Case 2：设备树信息无效**
包括：
- RAM 起点为零；
- RAM 大小为零；
- 起点加大小发生整数溢出；
- RAM 范围不满足当前固定基址单段模型；
- RAM 无法覆盖内核实际占用范围。
处理：
- 在任何依赖该范围的描述符写入或注册之前调用 panic()；
- 不得继续使用默认值初始化；
- 不得调用 init_memmap()。

本框架的正常 RAM 起点应为 DRAM_BASE。
对于不同起点，明确拒绝当前模型不支持的布局，
不要擅自改变 nbase 或伪装成同一段完整 RAM。

**Case 3：设备树末端超过可管理上界**
- 原始探测末端大于 PHYSICAL_MEMORY_END。
处理：
- 保留原始探测范围用于输出；
- 将实际可管理末端裁剪到 PHYSICAL_MEMORY_END；
- npage、描述符数量和空闲区都依据裁剪后范围计算；
- 不得登记超出上界的页面。

**Case 4：RAM 末端没有按页对齐**
- 实际可管理末端包含不完整尾页。
处理：
- 末端向下按 PGSIZE 对齐；
- 不完整尾页不进入描述符管理范围或空闲区；
- 不得通过向上对齐末端扩大 RAM 范围。

**Case 5：内核末端没有按页对齐**
处理：
- pages 的虚拟起点取 end 向上按页对齐的地址；
- 内核尾页及对齐间隙保持保留；
- 不得将 end 当作物理地址直接使用。

**Case 6：描述符数组无法完整放入 RAM**
包括：
- 描述符数量计算下溢；
- 数量乘 sizeof(struct Page) 溢出；
- 描述符起点加字节数溢出；
- 描述符范围超出实际 RAM 或可管理上界。
处理：
- 在初始化任何描述符之前 panic()；
- 不得先循环写入，再在末尾检查；
- 不得缩减描述符数量来掩盖布局错误。

**Case 7：描述符数组放得下，但没有完整空闲页**
- 描述符字节范围合法；
- 描述符末端向上按页对齐后，等于或超过可管理末端。
处理：
- 完成合法的描述符初始化；
- 空闲页数保持为零；
- 不调用 init_memmap()，尤其不传入 n == 0。

此 Case 只要求 page_init() 的行为正确。
后续默认分配器自检可能要求存在空闲页面，
不能将它的失败误判为必须登记虚假空闲页。

**Requirements**：

### A. 地址与范围计算
1. 使用半开区间 [base, end) 进行内部计算。
2. 在计算 RAM 末端前检查加法溢出。
3. 正确区分：
   - 内核虚拟地址；
   - 物理地址；
   - 物理页号；
   - Page 描述符地址。
4. 不能把物理 RAM 的字节数直接赋给 npage。
5. 设置 va_pa_offset 后，才使用依赖它的 PADDR。
6. 用减法比较、上界检查等方式避免布局计算中的整数溢出。
7. 在形成并解引用描述符指针前，先验证相应存储地址范围。

### B. 描述符布局
在范围验证通过后，满足以下关系：

```text
managed_end = 向下按页对齐(min(原始 RAM 末端,
                              PHYSICAL_MEMORY_END))

npage = PPN(managed_end)

descriptor_count = npage - nbase

pages_va = 向上按页对齐(end)

descriptor_bytes = descriptor_count * sizeof(struct Page)

descriptor_end_pa = PADDR(pages_va) + descriptor_bytes

free_begin = 向上按页对齐(descriptor_end_pa)
free_end   = managed_end
```

上述表达式表示布局关系，不代表可以跳过溢出和边界检查直接计算。

### C. 描述符初始状态
1. 对全部 descriptor_count 个描述符进行确定性初始化。
2. 不得依赖描述符存储区原先已经清零。
3. 至少将 ref、flags、property 初始化为明确状态。
4. 所有页面最初均标记为保留，且不得遗留 PageProperty 标记。
5. 初始空闲区由当前管理器的 init_memmap() 转换为可分配状态。
6. page_link 的处理应符合当前管理器的使用方式，
   不得要求其读取未初始化的链表指针。

### D. 空闲区注册
仅当 free_begin < free_end 时：

```text
free_pages_count = (free_end - free_begin) / PGSIZE
```

确认：
- free_pages_count > 0；
- PPN(free_begin) >= nbase；
- 最后一个登记页仍位于描述符覆盖范围内；

然后调用：

```c
init_memmap(pa2page(free_begin), free_pages_count);
```

本函数不得直接依赖 First-Fit 或 Best-Fit 的私有空闲链表和计数。

### E. 输出
使用 cprintf() 输出实际探测的 RAM 起点、大小和末端。
如果发生裁剪，应能区分原始探测范围和实际管理范围。
可额外输出描述符区与初始空闲区，方便手算核对。
输出格式和参数类型必须与当前工程的 cprintf() 实现兼容。

### F. 验证与现有测试限制
1. 检查实际修改差异，确认仅修改允许范围。
2. 在工具链可用时尝试编译，并如实报告结果。
3. 当前 best_fit_pmm.c 尚未完成时，其测试可能因 free_list、
   nr_free 等缺失而阻塞整项目编译。
   不得为本任务修改该文件，也不得宣称整项目已经通过。
4. 已有 tools/test_lab2.c 在默认内存配置下检查：

```text
npage == PPN(get_memory_base() + get_memory_size())
```

该检查没有考虑超出 PHYSICAL_MEMORY_END 后的裁剪。
默认 128 MiB 范围内二者应一致；
扩大 RAM 后，按作业约束裁剪可能与这条测试冲突。
必须说明该限制，不得删除裁剪逻辑或修改测试来隐藏冲突。

5. 原有正常启动测试要求空闲页数大于零，
   不能单独覆盖“没有完整空闲页”的边界 Case。
6. 未实际执行的边界测试，只能列出验证方案，不得写成已通过。

实现完成。以下是简要报告。
     
  1. 修改的文件与实现的函数
     
  - 文件：code/kern/mm/pmm.c（唯一改动文件，git status 确认）。
  - 函数：在 //lab2 function1 标记处实现了 static void page_init(void)，并按已提供的学号将标记替换为 //lab2 function1 2411824。
  - 未新增其他辅助函数，未改动任何头文件、dtb.c、init_pmm_manager()、pmm_init()、测试或 Makefile。

  2. va_pa_offset、npage、pages 的计算依据

  - va_pa_offset = PHYSICAL_MEMORY_OFFSET（0xFFFFFFFF40000000）：与 entry.S 启动页表一致（li t1, 0xffffffffc0000000 - 0x80000000，再 sub t0, t0, t1）。不是用 KERNBASE - DRAM_BASE，因为 KERNBASE
    对应内核物理起点 0x80200000 而非 DRAM 起点。
  - npage = PPN(managed_end)，其中 managed_end = ROUNDDOWN(min(ram_end, PHYSICAL_MEMORY_END), PGSIZE)。默认 128 MiB 配置下 ram_end = 0x80000000 + 0x8000000 = 0x88000000 ==
    PHYSICAL_MEMORY_END，故 npage = 0x88000（页号上界，非 RAM 页总数）。
  - pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE)：end 是链接脚本 kernel.ld 提供的内核镜像末端虚拟地址，向上按页对齐后作为描述符数组的虚拟起点，物理起点 pages_pa = PADDR(pages)。      
  
  3. 描述符数组与初始空闲区的布局关系

  [0x80000000 .. OpenSBI .. 0x80200000 .. 内核镜像 .. end]
                                └─ pages(按页对齐) ─ descriptor_count 个 Page ─┘
                                                                                └─ free_begin ─ managed_end(=0x88000000)

  - 描述符数量 descriptor_count = npage - nbase = 0x88000 - 0x80000 = 0x8000（32768）。
  - descriptor_bytes = descriptor_count * sizeof(struct Page)（sizeof(struct Page)=40，约 1.25 MiB）。
  - descriptor_end_pa = pages_pa + descriptor_bytes；free_begin = ROUNDUP(descriptor_end_pa, PGSIZE)，free_end = managed_end。
  - 全部 descriptor_count 个描述符确定性初始化（ref=0、flags=0、property=0、list_init(page_link)、SetPageReserved），使 OpenSBI、内核镜像、内核尾页、描述符数组存储页均保持保留态。
  - 仅当 free_begin < free_end 时，以 (free_end - free_begin) / PGSIZE 页调用一次 init_memmap(pa2page(free_begin), n)。

  4. 验证命令、结果与未执行项目

  - pmm.c 单独编译（riscv64-linux-gnu-gcc -Wall -Werror -Wno-unused -std=gnu99 -nostdinc … -c kern/mm/pmm.c）：通过，exit=0，无警告。
  - 整项目 make：在 best_fit_pmm.c 处失败（见下）。
  - 未执行：QEMU 运行、make grade、以及描述符数组溢出 / 无完整空闲页 / RAM 末端不按页对齐等边界 Case 的实机验证（工具链中无 riscv64-unknown-elf-gcc，仅有 riscv64-linux-gnu-gcc，且 Best-Fit     
    未完成）。

  5. Best-Fit 模块阻塞说明

  整项目编译被 kern/mm/best_fit_pmm.c 阻塞：

  kern/mm/best_fit_pmm.c:24:36: error: 'free_list' undeclared
  kern/mm/best_fit_pmm.c:28:34: error: 'nr_free' undeclared
  kern/mm/best_fit_pmm.c:64:25: error: 'free_list' undeclared
  kern/mm/best_fit_pmm.c:95:34: error: 'nr_free' undeclared
  make: *** [obj/kern/mm/best_fit_pmm.o] Error 1

  该文件 //lab2 function2-2 标记处尚未实现 free_list/nr_free 等私有数据结构与 best_fit_pmm_manager。按要求我未修改该文件，也未通过改动其他模块或测试来绕过，故不宣称整项目通过。

  6. 源码/作业/测试冲突说明

  - tools/test_lab2.c 第 3 项检查 npage == PPN(get_memory_base() + get_memory_size())，未考虑超出 PHYSICAL_MEMORY_END 后的裁剪。默认 128 MiB 下二者一致（0x88000000），不冲突；若将来扩大        
    RAM，按作业约束裁剪后会与该测试冲突。我保留裁剪逻辑、未修改测试。
  - test_lab2.c 不在默认 make 内核构建目标中（tools/ 不在 KSRCDIR），由 make grade 经 tools/inject.py 注入后运行，故“Page Init: PASS”需在 grader 注入并完成 Best-Fit 后才能最终验证。

  实现严格遵循了 prompt1.md 的 [SPECIFICATION]：半开区间 [base, end)、加法/乘法溢出检查、设备树无效即 panic()、不向分配器提交内核/OpenSBI/描述符存储页、free_begin < free_end 才注册且不传 n ==  
  0。
