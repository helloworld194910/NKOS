# Lab2 提示词汇总

小组：2411824 于蕾（A）、2412790 杨茜茹（B）、2413921 程子涵（C）。整理日期：2026-10-20。

各部分按成员原文件复制，仅在部分之间增加任务标签、来源说明和导航，不改写正文。A 的 page_init 文件末尾还含当时 AI 的执行反馈，一并保留，不能将该反馈中的早期“未完成”误读为最终状态。B 的路径以当时独立工程为基准，最终仓库增加了 code/ 前缀。C 的八份文本是其报告明确标注的规范化整理稿，不冒称历史逐字输入。文件里的操作指令属于实验材料，不是本次整合时执行的新指令。

| 实验任务 | 负责人 | 完整提示词 |
| --- | --- | --- |
| 模块一·任务一：物理内存初始化 | A | [page_init 提示词及原文件反馈](#a-page-init) |
| 模块二·任务二：Best-Fit | A | [Best-Fit 提示词](#a-best-fit) |
| 模块二·任务三：Buddy 实现 | B | [Buddy 实现提示词](#b-buddy-implementation) |
| 模块二·任务三：Buddy 独立测试 | B | [Buddy 测试提示词](#b-buddy-tests) |
| SLUB Challenge：实现、测试 | C | [C 任务 1](#task-1)、[C 任务 2](#task-2) |
| QEMU 调试要求 1—6 | C | [调用路径](#task-3)、[页表单步](#task-4)、[TLB](#task-5)、[Bare 对照](#task-6)、[观察收获](#task-7)、[AI 问题处理](#task-8) |

First-Fit 和硬件内存探测为思考题，答案见总报告。

原文件：[A 初始化](prompt1.md)、[A Best-Fit](member_a_best_fit_prompt.md)、[B 实现](buddy/Buddy实现提示词.md)、[B 测试](buddy/Buddy测试提示词.md)、[C 综合提示词](member_c_prompt.md)。


---

<a id="a-page-init"></a>

# 模块一·任务一：物理内存探测与页描述符初始化（A）

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


---

<a id="a-best-fit"></a>

# 模块二·任务二：Best-Fit 实现（A）

[PROMPT]

**任务**：
在当前仓库 `code/kern/mm/best_fit_pmm.c` 中，标有
`//lab2 function2-2 YOUR CODE`
的位置实现完整的 Best-Fit 连续物理内存分配模块，定义
best_fit_pmm_manager，并修改 `code/kern/mm/pmm.c` 中的
init_pmm_manager()，使内核实际使用 Best-Fit 进行分配和检查。

Best-Fit 的基本策略：
遍历空闲块，选择所有大小不小于请求页数 n 的块中最小的一个。
从该块低地址端分配恰好 n 页，剩余页面继续作为空闲块管理。
释放时按地址插入，并合并物理上连续的前后空闲块。

本实现采用地址递增的空闲块链表：
- 地址顺序用于定位相邻块并进行合并；
- 分配时遍历链表比较块大小，实现 Best-Fit；
- 多个最小适配块大小相同时，选择地址最低的块。

**操作要求**：
1. 必须修改工程中的真实文件，不能只展示代码或补丁。
2. 实施前阅读当前工程接口、First-Fit 实现和原有 Best-Fit 测试。
3. 仓库根目录下的路径以 `code/` 开头；
   如果工作目录已经是 code，则使用对应的相对路径。
4. 保持 basic_check() 和 best_fit_check() 两个测试函数原样。
5. 保留目标文件已有的头文件包含、注释与测试。
6. 保留 lab2 标记；已提供真实学号时，仅将本任务标记中的
   YOUR CODE 替换为该学号。未提供时不得编造。
7. 不修改上一任务完成的 page_init()。
8. 不修改任何现有测试、评分脚本、Makefile 或公共接口。
9. 不提交或推送 Git，除非另有明确要求。

**输出要求**：
完成真实文件修改后，报告：
1. 实际修改的文件与实现的接口。
2. 空闲链表、页面标志和计数遵循的状态约定。
3. Best-Fit 选块、拆分和合并的实现方式。
4. 实际执行的构建和验证命令、结果及未执行项目。
5. 发现的问题及实际修复方式。
6. 若实现或验证受其他任务的错误阻塞，报告具体原因，
   不得擅自扩大修改范围或伪造通过结果。

[RELY]

## 1. 必须阅读的真实文件

```text
code/kern/mm/best_fit_pmm.c
code/kern/mm/best_fit_pmm.h
code/kern/mm/default_pmm.c
code/kern/mm/default_pmm.h
code/kern/mm/pmm.c
code/kern/mm/pmm.h
code/kern/mm/memlayout.h
code/kern/mm/mmu.h
code/libs/list.h
code/libs/defs.h
code/kern/debug/assert.h
code/Makefile
code/tools/test_lab2.c
```

注意：链表接口实际位于 libs/list.h，不是 kern/libs/list.h。

若当前源码与下述信息存在差异，先核对真实接口。
不得因差异自动扩大修改范围。

## 2. 管理器接口

pmm.h 已定义：

```c
struct pmm_manager {
    const char *name;
    void (*init)(void);
    void (*init_memmap)(struct Page *base, size_t n);
    struct Page *(*alloc_pages)(size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};
```

本实现按 First-Fit 和现有测试的使用方式：
每次成功申请恰好 n 个连续页面，释放时按传入的 n 记账。
不得将请求向其他大小取整。

## 3. 页面和空闲区结构

memlayout.h 已提供：

```c
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

typedef struct {
    list_entry_t free_list;
    unsigned int nr_free;
} free_area_t;
```

页面状态接口包括：

```c
SetPageReserved(page)
ClearPageReserved(page)
PageReserved(page)

SetPageProperty(page)
ClearPageProperty(page)
PageProperty(page)

le2page(le, member)
```

状态含义：
- PageReserved 为真：该页不可交给普通分配器使用。
- PageProperty 为真：该页是空闲块的头页。
- 空闲块头页的 property 表示块内连续页面总数。
- 空闲块内部页不应设置 PageProperty。
- 已分配页面不应设置 PageProperty。
- property 只有在 PageProperty 为真时才有块长度语义，
  不得依赖已分配页面中残留的 property 值判断分配状态。

## 4. 链表接口

libs/list.h 提供：

```text
list_init
list_add
list_add_before
list_del
list_empty
list_next
list_prev
```

使用前核对真实函数签名与插入语义。

## 5. 页接口与公共包装函数

pmm.h、pmm.c 提供：

```c
static inline int page_ref(struct Page *page);
static inline void set_page_ref(struct Page *page, int val);
static inline uintptr_t page2pa(struct Page *page);
static inline struct Page *pa2page(uintptr_t pa);

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
```

以及：

```c
#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)
```

公共包装函数会转发给当前 pmm_manager。
因此 Best-Fit 管理函数内部不得调用同名公共包装函数来实现自身，
否则会递归进入当前管理器。

## 6. 原有测试约定

best_fit_pmm.c 已包含：

```c
static void basic_check(void);
static void best_fit_check(void);
```

两个函数的实际定义必须保持原样。

它们直接引用：

```c
free_list
nr_free
```

并通过以下方式临时隔离空闲状态：

```c
list_entry_t free_list_store = free_list;
list_init(&free_list);

unsigned int nr_free_store = nr_free;
nr_free = 0;
```

测试结束后再恢复原有状态。

因此：
- 必须提供测试所需的同名状态。
- 不得额外依赖测试无法同步保存和恢复的空闲索引、缓存计数或隐藏状态。
- 不得在分配失败路径中假设 nr_free > 0 就一定存在可用链表节点。
- 不得添加会破坏测试临时隔离流程的全局一致性断言。

best_fit_check() 中已有明确的 Best-Fit 案例：
先形成较低地址的 2 页空闲块和较高地址的 1 页空闲块，
申请 1 页时必须选择后者。

## 7. 注册声明和接入点

best_fit_pmm.h 已声明：

```c
extern const struct pmm_manager best_fit_pmm_manager;
```

pmm.c 已包含该头文件。

init_pmm_manager() 当前选择管理器后，会打印名称并调用 init()。
本任务只修改其中的管理器选择，不改变后续调用顺序。

[GUARANTEE]

## 必须实现的状态

在 best_fit_pmm.c 的指定实现区域提供：

```c
static free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free   (free_area.nr_free)
```

## 必须实现的接口

```c
static void best_init(void);

static void best_init_memmap(struct Page *base, size_t n);

static struct Page *best_alloc_pages(size_t n);

static void best_free_pages(struct Page *base, size_t n);

static size_t best_nr_free_pages(void);

const struct pmm_manager best_fit_pmm_manager;
```

允许添加文件内部的 static 辅助函数。

由于 best_fit_check() 定义在实现标记之后，
注册结构引用它之前必须存在正确的静态前向声明：

```c
static void best_fit_check(void);
```

不得为解决声明顺序修改测试函数定义或删除 static。

## 必须修改的接入点

在 pmm.c 的 init_pmm_manager() 中设置：

```c
pmm_manager = &best_fit_pmm_manager;
```

保留打印和 init() 调用。

## 允许修改的范围

1. best_fit_pmm.c 的指定实现区域：
   添加状态、声明、辅助函数、管理函数和注册结构。
2. pmm.c 的 init_pmm_manager()：
   仅修改当前管理器的选择。

## 禁止修改的范围

不得修改：
- basic_check()、best_fit_check() 的任何内容；
- page_init()；
- default_pmm.c；
- 公共头文件、结构体和函数签名；
- Makefile、链接脚本、启动代码、设备树代码；
- tools 下的现有测试和评分脚本。

不新增跨文件公共接口，不引入动态内存分配依赖。

[SPECIFICATION]

## 一、模块状态不变量

正常管理状态下必须满足：
1. free_list 为双向循环链表。
2. 链表仅包含空闲块头页的 page_link。
3. 空闲块按物理地址递增排列。
4. 各空闲块互不重叠。
5. 完成释放合并后，不存在两个仍可合并的相邻空闲块。
6. 每个空闲块头页满足：
   - PageReserved 为假；
   - PageProperty 为真；
   - property > 0。
7. 空闲块内部页的 PageProperty 为假。
8. nr_free 等于链表中所有空闲块 property 的总和。

现有测试在隔离状态的中间步骤可能暂时不满足第 8 条。
实现必须兼容这些已提供的测试步骤，
不得在所有函数入口无条件扫描并断言该不变量。

## 二、best_init()

**Pre-Condition**：
- 模块开始一次新的初始化。
- 尚未登记本轮初始空闲区域。

**Post-Condition**：
- free_list 为空的双向循环链表。
- nr_free == 0。

**Requirements**：
- 调用 list_init()。
- 不访问物理页内容，不调用分配接口。
- 不登记初始空闲区；登记由 best_init_memmap() 完成。

## 三、best_init_memmap(base, n)

**Pre-Condition**：
- best_init() 已执行。
- n > 0。
- [base, base + n) 为合法的连续描述符范围。
- 对应页面均处于保留状态。
- 此范围不与已经登记的空闲块重叠。
- 正常调用来自 page_init()，只包含真正可用的完整物理页。

**Post-Condition**：
- 这 n 页转为可分配页面。
- 每页 ref 为 0。
- 每页保留标志被清除。
- 最终空闲块头正确设置 PageProperty 和 property。
- nr_free 增加 n，且只增加一次。
- 链表保持地址有序。

**Case 1：空链表**
- 将此区间作为初始空闲块插入。

**Case 2：非空链表，区间与既有块不相邻**
- 按地址插入为独立空闲块。

**Case 3：区间与前块或后块相邻**
- 合并连续区间，清除被吞并块头的 PageProperty，
  移除相应链表节点，保证总页数不变。

**Case 4：n == 0 或输入页不是保留页**
- 使用与参考实现一致的断言拒绝非法调用。
- 不得将非法区间加入链表。

**Requirements**：
- 先检查前置约束，再修改页面状态。
- 初始化 flags、ref 和 property，避免依赖旧值。
- 避免链表插入后继续遍历导致重复插入或死循环。
- 如复用释放函数或内部辅助函数，必须说明状态转换合法，
  并避免 nr_free 重复累加。

## 四、best_alloc_pages(n)

**Pre-Condition**：
- n > 0。
- 模块已初始化。
- 除框架测试明确采用的临时隔离状态外，
  空闲链表与计数满足模块不变量。

**Post-Condition：成功时**
- 返回被选空闲块低地址端的头页。
- 调用者获得恰好 n 个连续页面。
- 被分配范围不再属于空闲链表。
- 被分配范围不存在 PageProperty 标记。
- nr_free 减少 n。
- 剩余空闲块仍按地址有序。

**Case 1：存在恰好 n 页的块**
- 优先选取恰好匹配的块。
- 多个同样大小的块中选择地址最低者。
- 将其移出空闲链表，不产生余块。

**Case 2：没有恰好匹配，但存在更大的块**
- 在所有 property >= n 的块中选择 property 最小者。
- 同样大小时选择地址最低者。
- 从头部分配 n 页。
- 余块头为 page + n，大小为原块大小减 n。
- 余块设置 PageProperty，并保持链表地址顺序。

**Case 3：n > nr_free**
- 返回 NULL。
- 链表、计数和页面状态不变。

**Case 4：总空闲页数足够，但没有足够大的连续块**
- 返回 NULL。
- 不得拼接不连续页面。
- 链表、计数和页面状态不变。

**Case 5：链表为空**
- 返回 NULL。
- 即使框架测试尚未同步将 nr_free 清零，也必须安全处理，
  不得解引用链表哨兵。

**Case 6：n == 0**
- 断言失败。

**Requirements**：
- 找到第一个足够大的块后不能直接停止，
  除非该块大小已经恰好等于 n。
- 拆分前保存原块长度与必要链表位置。
- 正确移除原块头，正确插入余块。
- ClearPageProperty() 作用于分配块头。
- 失败路径不得提前扣减 nr_free 或修改链表。
- 不修改调用者已分配页面的数据内容。

## 五、best_free_pages(base, n)

**Pre-Condition**：
- n > 0。
- [base, base + n) 为合法连续描述符范围。
- 页面当前属于调用者，未被释放。
- 页面不是保留页。
- 范围内各页不存在 PageProperty 标记。
- 允许释放一个较大已分配区间中的合法子区间，
  不得要求 base 必须是原始申请的起点。

**Post-Condition**：
- n 页重新进入空闲集合。
- 每页 ref 为 0，页面标志恢复为正确空闲状态。
- nr_free 增加 n，且只增加一次。
- 新区间与相邻空闲块完成合并。
- 合并后仅最终块头具有 PageProperty。
- 链表保持地址递增。

**Case 1：空链表**
- 新建一个 n 页空闲块。

**Case 2：与前后块均不相邻**
- 按地址插入，不合并。

**Case 3：仅与前块相邻**
- 前块长度增加 n。
- 新区间头不再作为独立空闲块头。

**Case 4：仅与后块相邻**
- 新区间头作为合并后块头。
- 长度增加后块长度。
- 清除后块头标记并移除其节点。

**Case 5：同时与前后块相邻**
- 合并成一个完整空闲块。
- 合并前块后，更新当前块指针，
  再判断更新后的块与后块是否相邻。

**Case 6：n == 0**
- 断言失败。

**Case 7：保留页或范围内已有空闲块头**
- 按参考实现的页面状态检查方式断言失败。
- 不得把这类明显非法输入登记为空闲页。

**Requirements**：
- 用描述符连续性判断相邻关系，例如：

```c
previous + previous->property == base
base + base->property == next
```

- 只有地址连续才合并，不能仅因链表相邻就合并。
- 合并时不再次增加 nr_free。
- 不引入原始分配大小表或其他跨调用隐藏状态。
- 页面状态断言不是完整的非法释放检测机制；
  不得宣称已能检测所有重复释放或任意错误指针。

## 六、best_nr_free_pages()

**Pre-Condition**：
- 模块已初始化。

**Post-Condition**：
- 返回当前 nr_free 的值。

**Requirements**：
- 不修改链表、计数或页面状态。
- 不调用公共 nr_free_pages()，避免递归。
- 返回类型保持为 size_t。

## 七、best_fit_pmm_manager 注册

**Pre-Condition**：
- 所需管理函数已实现或已正确声明。
- best_fit_check() 在注册结构之前有静态前向声明。

**Post-Condition**：
定义：

```c
const struct pmm_manager best_fit_pmm_manager = {
    .name = "best_fit_pmm_manager",
    .init = best_init,
    .init_memmap = best_init_memmap,
    .alloc_pages = best_alloc_pages,
    .free_pages = best_free_pages,
    .nr_free_pages = best_nr_free_pages,
    .check = best_fit_check,
};
```

**Requirements**：
- 与 best_fit_pmm.h 的 extern 声明一致。
- check 指向原有 best_fit_check()。
- 不新增替代测试，不把 check 设置为空函数。

## 八、内核接入

在 pmm.c 的 init_pmm_manager() 中选择：

```c
pmm_manager = &best_fit_pmm_manager;
```

保留原有：
- 管理器名称打印；
- pmm_manager->init() 调用；
- 后续 page_init() 和分配器检查顺序。

---

<a id="b-buddy-implementation"></a>

# 模块二·任务三：Buddy 实现与可选接入（B，第一份）

[PROMPT]

**任务**

在当前 Lab2 工程中，从零实现 Buddy System 连续物理页分配模块，并接入现有 `pmm_manager` 框架。

目标文件：
- 新增 `kern/mm/buddy_pmm.c`。
- 新增 `kern/mm/buddy_pmm.h`。
- 在 `kern/mm/pmm.c` 中添加 Buddy 管理器的可选接入。
- 在 `Makefile` 中添加 `BUDDY=1` 构建选项。
- 新增 `docs/buddy_design.md`，记录设计契约、接入方法和实际验证结果。

Buddy 将空闲页组织成包含 2^order 个连续物理页的块。分配时应选择能够容纳请求的最小可用阶；必要时分裂较大的块。释放时只允许与同阶、空闲且满足伙伴关系的块合并，并允许递归合并。

必须遵守以下分配契约：
- 请求 n 页，实际占用能够容纳 n 的最小二次幂页数。
- 例如请求 3 页，实际占用 4 页，空闲页数减少 4。
- 释放时调用 `free_pages(base, n)`，其中 n 必须是原始申请页数。
- 模块记录并核对原始申请长度和实际占用块大小。
- 不支持部分释放、重复释放、非块首地址释放或使用错误长度释放。
- 对齐与伙伴关系以绝对物理页号为基准，不能直接用描述符数组下标代替物理页号。
- 初始空闲区的起点和长度不要求满足二次幂对齐，必须完整管理其中的页面。

**操作要求**

你必须读取当前工程，并修改或新增真实文件，不能仅在回答中展示代码。

开始修改前：
- 查看 Git 状态，保留用户已有修改。
- 核对下述依赖与工程实际定义。
- 阅读 `default_pmm.c` 理解框架调用和页面状态，但不要将 First-Fit 改名后当作 Buddy。

允许修改的范围：
- 上述两个 Buddy 文件。
- `pmm.c` 中必要的头文件包含和 `init_pmm_manager()` 选择逻辑。
- Makefile 中与 `BUDDY=1` 有关的编译配置。
- `docs/buddy_design.md`。

禁止修改：
- `page_init()` 及其待实现位置，这是另一位成员的任务。
- `struct Page`、`struct pmm_manager` 及公共地址转换接口。
- `default_pmm.c`、`best_fit_pmm.c` 中已有实现和测试。
- `tools/test_lab2.c`、`tools/inject.py`、`tools/grade.sh`。
- 启动汇编、链接脚本、设备树解析等无关模块。
- 不得删除、覆盖已有工作，不得自行提交、推送或回滚 Git。

如果发现必须修改允许范围以外的文件，先说明具体原因和最小修改需求，不要自行扩大范围。

**输出与验证要求**

- 完成真实文件修改。
- 使用 `BUDDY=1` 显式选择 Buddy；未指定时保留当前默认管理器选择。
- 不在本任务中替另一位成员实现或切换 Best-Fit。
- 当前默认管理器可能仍是 First-Fit；最终集成时默认 Best-Fit 是后续工作。
- 本轮提供管理器 `.check` 所需的基础检查；完整行为测试由后续独立提示词完成。
- 核对生成文件是否被实际构建系统纳入。
- 尝试完成当前条件允许的编译和验证。
- 若 `page_init()` 缺失导致全工程无法构建或启动，不得补写它；应明确报告阻塞，并独立检查 Buddy 编译单元。
- 未实际运行的检查标为“未执行”，不得报告完整内核或测试已通过。
- 不得修改测试断言来迁就错误实现。

完成后说明：
1. 修改和新增了哪些文件。
2. 使用了什么数据结构，如何记录申请和实际块大小。
3. 如何定义伙伴、处理不规整初始区间及防止非法释放。
4. 启用 Buddy、恢复默认配置的命令。
5. 实际执行的命令、结果和未完成验证。
6. 已知限制及需要 A 提供的依赖。

[RELY]

以下是当前框架的最小依赖。先核对实际文件；若存在差异，报告差异并以真实工程为准，不能修改公共接口来迎合提示词。

**物理内存管理接口：`kern/mm/pmm.h`**

```c
struct pmm_manager {
    const char *name;
    void (*init)(void);
    void (*init_memmap)(struct Page *base, size_t n);
    struct Page *(*alloc_pages)(size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};

extern const struct pmm_manager *pmm_manager;

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);

#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)
```

`pmm.c` 中的公共分配、释放、计数函数转发到当前管理器。

初始化调用关系：
`init_pmm_manager()` 调用管理器的 `init()`；
随后 `page_init()` 通过当前管理器的 `init_memmap()` 登记空闲区；
最后调用管理器的 `check()`。

**页描述符：`kern/mm/memlayout.h`**

```c
struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};
```

框架提供：
- `PageReserved`、`SetPageReserved`、`ClearPageReserved`。
- `PageProperty`、`SetPageProperty`、`ClearPageProperty`。
- `le2page`。

`PageReserved` 表示保留页，不能交给普通分配。
Buddy 的空闲块首描述符应设置 `PageProperty`，其 `property`
表示块的实际页数；非空闲块首描述符不得残留该标志。

**页号与地址：`pmm.h`、`pmm.c`、`mmu.h`**

```c
extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern uint64_t va_pa_offset;

static inline ppn_t page2ppn(struct Page *page);
static inline uintptr_t page2pa(struct Page *page);
static inline struct Page *pa2page(uintptr_t pa);
static inline int page_ref(struct Page *page);
static inline void set_page_ref(struct Page *page, int val);
```

- `PGSIZE` 为 4096，`PGSHIFT` 为 12。
- 描述符数组覆盖物理页号 `[nbase, npage)`。
- `page2ppn(page)` 对应 `page - pages + nbase`。
- `npage` 是物理页号上界，不是描述符数量。
- 描述符数量是 `npage - nbase`。
- 当前 `pa2page()` 不负责检查低于 `nbase` 的输入，
  Buddy 不能依靠它替代完整范围检查。
- 当前 `KADDR` 定义被注释，不可假设它能直接使用。
- 内存上界与偏移以 `memlayout.h` 真实常量为准。

**通用支持**

- 链表在 `libs/list.h`，不是 `kern/libs/list.h`。
- 类型和辅助宏在 `libs/defs.h`。
- `assert`、`panic` 在 `kern/debug/assert.h` 中提供，其中
  `panic` 是宏，不应自行重声明为普通函数。
- `cprintf` 在 `libs/stdio.h` 中声明。
- Makefile 已扫描 `kern/mm` 下的 C 文件，应核对新增模块是否自动纳入。

[GUARANTEE]

**必须实现的接口**

`kern/mm/buddy_pmm.c`：

```c
static void buddy_init(void);
static void buddy_init_memmap(struct Page *base, size_t n);
static struct Page *buddy_alloc_pages(size_t n);
static void buddy_free_pages(struct Page *base, size_t n);
static size_t buddy_nr_free_pages(void);
static void buddy_check(void);

const struct pmm_manager buddy_pmm_manager;
```

注册结构要求：
- `.name = "buddy_pmm_manager"`。
- 各函数指针分别指向对应 Buddy 函数。
- `.check = buddy_check`。

`kern/mm/buddy_pmm.h`：
- 使用头文件保护。
- 声明 `extern const struct pmm_manager buddy_pmm_manager`。

**内部状态要求**

可以添加模块私有的静态变量、辅助函数和元数据结构。

需要可靠记录：
- 被登记管理的页面范围。
- 各阶空闲块。
- 当前实际空闲页总数。
- 每个存活分配的起点、原始请求页数和实际块大小。
- 检查伙伴是否为同阶空闲块所需的信息。

不得使用尚不存在的 `malloc`、SLUB 或页分配接口为自身初始化
分配元数据，避免循环依赖。

若采用静态元数据，容量必须覆盖当前框架允许管理的物理页，
初始化时检查容量；不得静默截断或使用大型内核栈数组。

**接入要求**

- `make BUDDY=1` 选择 Buddy。
- 未指定时保留现有选择。
- 两种配置都保留 `pmm_manager->init()` 和现有管理器名称输出。
- 切换配置后须干净构建，运行说明要写明这一点。
- 不增加与本任务无关的公共接口。

[SPECIFICATION]

## 全局不变量

在完成每次合法操作后：
- 每个已登记页面恰好属于一个空闲块或一个存活分配。
- 不存在重叠的空闲块或重叠的存活分配。
- 空闲块大小为二次幂，并按绝对物理页号满足块大小对齐。
- 同一空闲块只出现在对应阶的管理结构中一次。
- 可合并的同阶空闲伙伴最终应合并。
- 空闲页计数等于所有空闲块实际页数之和。
- 保留页、未登记页不能被分配或合并进入空闲块。
- 失败操作不得破坏已有合法状态。

## buddy_init

**Pre-Condition**
- 由框架选择管理器后、登记空闲区前调用。
- 不要求此时 `pages` 和 `npage` 已完成初始化。

**Post-Condition**
- 管理结构为空，空闲页数为 0。
- 没有已登记区间和存活分配记录。

**Requirements**
- 不依赖页分配接口或尚未建立的页描述符布局。
- 正常框架生命周期仅在登记前初始化；
  不要求支持带存活分配时重新初始化。

## buddy_init_memmap

**Pre-Condition**
- `buddy_init()` 已完成。
- `pages`、`npage` 已正确建立。
- 输入是 `[nbase, npage)` 内连续、尚未登记的保留页面。
- 登记操作不与其他分配或释放并发执行。

**Case 1 正常登记**
- `n > 0` 且区间合法。
- 登记全部 n 页，空闲页数恰好增加 n。
- 起点或长度不规整时仍不丢页、不越界。
- 清除新登记页的保留状态，引用计数置 0。
- 支持多次登记互不重叠的区间。
- 若不同登记区间形成合法的空闲伙伴，允许并完成合并。

**Case 2 非法输入**
- n 为 0、区间越界、重复或重叠登记、元数据容量不足。
- 通过断言或 panic 终止，给出可定位原因。
- 在修改管理状态前完成必要的输入验证。

**Requirements**
- 不得把两段之间未登记的空洞纳入管理。
- 检查区间大小时避免加法溢出。

## buddy_alloc_pages

**Pre-Condition**
- 管理器已初始化，初始空闲区已登记。
- 当前状态满足全局不变量。

**Case 1 成功**
- n > 0，向上取整得到实际页数 k，k 为最小的满足 k >= n 的二次幂。
- 从所有可满足请求的空闲块中，选择最小可用阶。
- 返回实际 k 页连续块的首描述符。
- 首物理页号按 k 对齐。
- 分配区间全部来自已登记空闲页。
- 实际空闲页数减少 k。
- 保存原始 n 与实际 k，供释放验证。
- 分配块的所有页不带空闲块首标志，引用计数为 0。
- 剩余空闲空间继续满足全局不变量。
- 同阶多个块的选择次序由实现决定，不强制地址优先。

**Case 2 资源不足**
- 总空闲页不足，或不存在能够形成目标块的可用空间。
- 返回 NULL，管理结构、计数和已有分配记录不变。

**Case 3 请求过大**
- 请求超过管理能力，或向上取整会溢出。
- 返回 NULL，状态不变，不能发生非法移位或死循环。

**Case 4 n 为 0**
- 断言失败，在修改状态前报告非法请求。
- 此情况是对合法调用前置条件的防御检查。

## buddy_free_pages

**Pre-Condition**
- 管理器已初始化。
- 合法释放使用本管理器返回的分配首地址和原始申请页数。
- 不要求支持部分释放。

**Case 1 合法释放**
- 输入准确匹配一个当前存活分配。
- 释放其全部实际 k 页，空闲页数恰好增加 k。
- 清除该存活分配记录。
- 只与同阶、空闲、已登记且满足物理页号伙伴关系的块合并。
- 合并后仍有合法伙伴时继续合并。
- 不能仅因两个块相邻就合并。
- 释放完成后满足全局不变量。

**Case 2 非法释放**
- n 为 0、NULL、区间外地址、非块首地址、错误申请长度、
  重复释放、保留页或未登记页释放。
- 使用断言或 panic 终止，给出可定位原因。
- 必须先验证再修改状态，不能先破坏链表或计数后报错。

**Requirements**
- 区间外指针不得先解引用或做无效的描述符指针运算再检查。
- 不得触及实际分配块以外的页面。

## buddy_nr_free_pages

**Pre-Condition**
- `buddy_init()` 已完成。

**Post-Condition**
- 返回按实际页数计算的空闲页总数。

**Requirements**
- 不改变状态。
- 不把块数、可满足请求的次数或原始请求页数当作空闲页数。

## buddy_check

**Pre-Condition**
- 已完成正常内核初始化和空闲区登记。

**Post-Condition**
- 检查当前各阶块大小、物理页号对齐、计数一致性、
  区间合法性和可合并伙伴是否遗漏等结构不变量。
- 检查通过后输出明确的 Buddy 基础检查成功信息。
- 检查失败通过断言或 panic 报告。
- 检查前后管理器状态不变。

**Requirements**
- 本轮完成基础结构检查，不宣称替代后续完整行为测试。
- 不调用 First-Fit 或 Best-Fit 的私有测试。
- 不伪造通过信息。
- 运行成本可接受即可，本实验不要求优化时空复杂度。

## 当前实验范围

- 以当前实验的单核、串行管理器调用场景实现。
- 不增加多核锁、换页、虚拟页分配等额外功能。
- 不修改公共页描述符结构。
- 正常分配和释放避免大量输出；管理器选择与检查输出
  应能够确认运行的是 Buddy。
- 若初始化依赖未完成，记录阻塞并保留模块实现，
  不以临时伪实现掩盖完整运行尚未验证的事实。


---

<a id="b-buddy-tests"></a>

# 模块二·任务三：Buddy 独立测试（B，第二份）

[PROMPT]

**任务**

为当前 Lab2 工程已经实现的 Buddy System 设计并实现独立测试。

被测模块：
- `kern/mm/buddy_pmm.c`
- `kern/mm/buddy_pmm.h`

新增测试文件放在：
- `tools/buddy_host/`：宿主测试入口、必要的平台适配和运行脚本。
- `docs/buddy_test.md`：测试设计、预期结果、运行方法及实际验证记录。

允许在 Makefile 中添加独立的 `buddy-host-test` 目标。

测试应在 WSL 的宿主 Linux 上使用原生 GCC 执行，直接编译、
链接真实的 `kern/mm/buddy_pmm.c`，通过
`buddy_pmm_manager` 的接口验证算法。

本阶段使用模拟 Page 描述符数组和真实 Buddy 算法，不启动
QEMU，不依赖尚未实现的 `page_init()`。

本阶段通过不能被表述为完整内核或真实物理内存访问已经验证。

**操作要求**

必须修改或新增实际文件，不能仅展示测试代码。

开始前读取：
- 当前 Buddy 实现和头文件。
- `pmm.h`、`memlayout.h`、`mmu.h`、`defs.h`、`list.h`。
- 当前 Makefile。
- `docs/buddy_design.md`。

允许修改：
- `tools/buddy_host/` 中新增的测试与适配文件。
- Makefile 中与宿主测试有关的独立规则。
- `docs/buddy_test.md`。

禁止修改：
- Buddy 算法及其私有状态。
- `pmm.c`、`page_init()`。
- 公共结构体、接口、宏和地址转换。
- 原有 First-Fit、Best-Fit 代码与测试。
- 原有评分脚本、启动汇编和链接脚本。
- 已有设计文档中的历史验证记录。

如果测试发现实现问题：
- 保留能复现问题的测试。
- 记录输入、预期、实际结果和复现命令。
- 不降低断言标准，不在本任务中修改分配算法。
- 明确报告失败，交由后续修复迭代处理。

保留用户已有改动，不自动提交、推送或回滚 Git。

**测试原则**

- 通过公开的 `buddy_pmm_manager` 回调进行操作。
- 不包含被测 `.c` 文件来访问其 static 数据。
- 不直接读取或修改 `records`、`order_lists`、`free_count` 等私有变量。
- 不复制或重写 Buddy 算法。
- 允许提供模拟 `pages/npage/nbase` 和必要的平台符号。
- 使用独立的期望值、手算案例和存活区间记录判断正确性。
- 正常与异常场景均应有自动判定。
- 测试需要确定、可复现，不能依赖人工观察“似乎正常”。

**输出与验证要求**

完成后交付：
1. 实际新增和修改文件列表。
2. 一条可重复运行全部宿主测试的命令：
   `make buddy-host-test`。
3. 每个用例的明确 PASS/FAIL，以及总结果。
4. 未通过时返回非零状态，不得通过管道或脚本掩盖失败。
5. 覆盖矩阵：每项设计要求对应哪些测试。
6. 实际执行的命令、输出摘要和环境。
7. 尚未覆盖的内容及后续 QEMU 联调事项。

若宿主工具不可执行，保留测试文件，明确标为未运行，
提供用户可执行的命令，不虚构结果。

[RELY]

**真实管理器接口**

由 `kern/mm/buddy_pmm.h` 声明：

```c
extern const struct pmm_manager buddy_pmm_manager;
```

通过以下回调调用被测实现：

```c
buddy_pmm_manager.init();
buddy_pmm_manager.init_memmap(base, n);
buddy_pmm_manager.alloc_pages(n);
buddy_pmm_manager.free_pages(base, n);
buddy_pmm_manager.nr_free_pages();
buddy_pmm_manager.check();
```

各成员的真实签名以 `kern/mm/pmm.h` 为准。

**当前分配契约**

- 请求 n 页，实际占用最小的、能够容纳 n 的二次幂页数 k。
- 释放时传原始请求 n，而不是实际块大小 k。
- 计数按实际占用 k 变化。
- 同阶块的选择次序未约定，不得断言一定返回最低地址。
- 初始区间可不规整，可以分多次登记。
- 伙伴关系以绝对物理页号为基准。
- 空闲块首带 `PageProperty`，property 为块的实际页数。
- 已分配页面不带空闲块首标志。
- 不支持部分释放、错误长度、重复释放及非首地址释放。
- 非法输入触发 assert/panic；普通分配失败返回 NULL。
- `.check` 进行基础不变量检查。
- 当前只考虑串行操作。

**框架语义**

- `pages` 是 Page 描述符数组起点。
- `nbase` 是 DRAM 起始物理页号。
- `npage` 是物理页号上界。
- 描述符数量为 `npage - nbase`。
- `page2ppn(p)` 对应 `p - pages + nbase`。
- `page2pa()` 返回物理地址数值，宿主测试不能把它当宿主指针解引用。
- 页面大小由 `PGSIZE` 定义。
- 通过真实 Page 状态宏建立初始保留页状态。
- 相关类型与宏来自真实工程头文件，不得在测试中重新定义另一套。

**宿主适配约束**

- 宿主测试使用原生 GCC，不使用 RISC-V 交叉编译器生成宿主程序。
- 不执行 RISC-V 特权指令，不调用 SBI。
- 可以提供 `cprintf` 和断言所需的 `__panic` 平台实现。
- 适配代码只处理输出和进程终止，不实现分配逻辑。
- `panic` 是框架宏，应适配其调用的真实函数，
  不能通过替换 assert 或 panic 宏跳过检查。
- 工程自定义 `size_t` 等类型可能与宿主标准头冲突；
  应隔离平台适配编译单元或采取兼容的构建组织方式，
  不修改公共头文件来解决。
- 不复用内核 obj 目录中的 RISC-V 对象文件。
- 宿主产物放在独立子目录，与内核构建隔离。

[GUARANTEE]

**必须交付的能力**

1. 正常行为测试入口。
2. 每项非法操作的独立运行入口或独立子进程场景。
3. 运行全部用例并核对退出结果的驱动。
4. `make buddy-host-test` 构建与执行目标。
5. `docs/buddy_test.md` 覆盖矩阵和实际结果。

测试文件内部函数名可以自行设计，但每个用例必须有稳定名称，
能够对应到失败信息和文档。

**测试夹具**

- 创建有界模拟 Page 数组。
- 设置与数组大小一致的 `pages`、`npage` 和真实 `nbase`。
- 登记前将待登记页设为保留状态。
- 每个独立场景从全新状态开始。
- 不在真实内核活动状态上重置管理器。
- 正常场景结束时释放所有存活分配，检查空闲页恢复。
- 仅在测试进程内模拟异常全局状态，不能污染工程源码。

**异常隔离**

每个预期断言场景独立执行。

宿主 `__panic` 可以记录已发生框架断言并以约定的非零状态退出，
由父进程或测试驱动核对。

不得把任意非零退出都判为“预期断言通过”：
段错误、未捕获信号、超时和加载失败必须判失败。
必须有证据证明触发了预期的断言路径。

[SPECIFICATION]

## Case 1 初始化为空

**前置条件**
- 模拟环境有效，但未登记任何空闲页。

**操作与预期**
- 调用 init。
- 空闲页数为 0。
- 申请 1 页返回 NULL，计数保持 0。
- 若重复开展该用例，使用全新夹具状态。

## Case 2 单页与各阶精确申请

**前置条件**
- 登记一个物理页号对齐、长度为二次幂的区间。
- 用例规模不超过当前元数据容量。

**操作与预期**
- 覆盖从单页到该测试区间最大阶的申请。
- 返回非 NULL。
- 实际块大小与请求相同。
- 首物理页号按块大小对齐。
- 区间全部落在已登记范围。
- 计数准确减少，释放后恢复。
- 关键操作前后调用 check。

## Case 3 非二次幂申请

**操作与预期**
- 至少测试 3、5、9 页请求，分别实际占用 4、8、16 页。
- 分配对齐和计数按实际占用判断。
- 使用原始请求长度释放，计数恢复。
- 不得在正常用例中用向上取整后的长度替代原始长度释放。

## Case 4 连续分裂与耗尽

**前置条件**
- 登记一个小型、对齐的二次幂区间。

**操作与预期**
- 连续申请单页直到耗尽。
- 所有地址互异，所有返回区间合法。
- 成功次数等于初始页数。
- 耗尽后再申请返回 NULL，状态不变。
- 按非原始顺序释放，计数逐次正确。
- 全释放后可以重新申请整个大块。

## Case 5 正确伙伴递归合并

**前置条件**
- 使用可手算的对齐小区间。

**操作与预期**
- 分配形成多个小块。
- 释放真正伙伴后可以申请相应上阶块。
- 分阶段验证多级递归合并，不能只检查最终空闲总量。
- 不要求同阶选择固定地址；
  如需精确地址断言，先构造只有唯一合法候选的状态。

## Case 6 相邻但非伙伴

**手算场景**
- 在 nbase 为 8 的倍数的条件下，登记下标 `[0, 8)`。
- 连续申请 4 个 2 页块，按返回地址识别它们，
  不假定申请顺序就是地址顺序。
- 保持外侧 `[0,2)` 与 `[6,8)` 存活，
  释放中间 `[2,4)` 与 `[4,6)`。

**预期**
- 空闲总量为 4 页。
- 中间两块相邻，但不是同阶伙伴。
- 申请 4 页必须失败，状态和计数不变。
- 最后释放外侧分配，恢复整个 8 页块。

## Case 7 不规整初始区间

**手算场景**
- 使用足够大的描述符数组。
- 在真实 nbase 满足相应对齐的条件下，登记 `[3,16)` 共 13 页。

**预期**
- 初始空闲数精确为 13。
- 连续单页分配可恰好取得 13 个不同页面。
- 不取得下标小于 3 或大于等于 16 的页。
- 全部释放后恢复 13 页。
- 未登记区域保持保留状态。
- 另补充一个尾部不规整的区间，避免只测到长度不规整、
  但末端恰好对齐的一种情况。

## Case 8 多次登记与空洞

**场景 A**
- 先登记 `[20,24)`，再登记 `[16,20)`。
- 当 nbase 满足 8 页对齐时，能够分配 `[16,24)` 的 8 页块。

**场景 B**
- 登记 `[32,34)` 与 `[36,38)`。
- 总空闲数为 4，但申请 4 页失败。
- 不得跨空洞合并；失败后计数和可用空间不变。

**要求**
- 场景之间重置夹具。
- 保留并检查空洞页状态。
- 如果真实 nbase 不满足上述对齐，调整手算下标，
  并在文档写明实际物理页号，不能忽略绝对页号约束。

## Case 9 分配失败保持状态

**覆盖**
- 请求超过总空闲量。
- 总量够但没有足够大的连续伙伴块。
- 极大请求，例如 size_t 的最大值。

**预期**
- 返回 NULL。
- 原有存活分配仍有效。
- 空闲页数不变。
- check 通过。
- 失败后继续正常分配与释放，结果符合原有资源状态。

## Case 10 存活区间与交错多轮操作

**操作与预期**
- 混合不同请求大小。
- 每次保存原始请求和独立计算的实际占用。
- 用独立存活区间表检查所有实际占用区间两两不重叠。
- 按确定的交错顺序分配、释放和重新申请。
- 每次核对空闲页数。
- 多轮结束后空闲量恢复，仍可申请初始最大块。
- 如使用伪随机序列，固定并输出种子。

**内存内容验证**
- 若测试对象写入完整性，另建有界宿主字节数组作为模拟页内容。
- 依据已验证的描述符下标映射到该数组。
- 为不同存活分配写入不同模式，检查其他操作未破坏内容。
- 不直接解引用模拟物理地址。
- 该验证只能说明模拟模型中的区间隔离，
  不能宣称真实 MMU 或 QEMU 数据访问已验证。

## Case 11 非法申请与释放

以下每一项使用独立异常场景：

- 申请 0 页。
- 释放时 n 为 0。
- 释放 NULL。
- 释放描述符范围外的地址。
- 释放指向描述符内部的未对齐地址。
- 释放分配块的非首描述符。
- 使用与原始申请不符的长度释放。
- 申请 3 页后错误地用 4 页释放。
- 同一分配释放两次。
- 释放保留页或未登记页。

**预期**
- 到达对应操作时触发框架断言。
- 正常返回判失败。
- 无关位置的断言、段错误、超时不能判通过。
- 保留场景名称和诊断输出以便定位。

## Case 12 非法登记与容量

独立异常场景覆盖：
- 登记 0 页。
- 起点或区间越界。
- 重复登记。
- 部分重叠登记。
- 输入区间中存在非保留页。
- 模拟描述符数量超过静态容量。

**预期**
- 触发框架断言。
- 不得通过给已登记页面重新设置 Reserved 来绕过重叠检查。
- 越界和容量测试避免测试夹具自身先发生越界。
- 不依靠异常进程退出后的状态推断失败前完全无修改；
  这一点还需要对实现验证顺序进行代码审查。

## 测试独立性与报告

**Requirements**
- 预期页数和关键小案例应手算，不调用被测内部函数生成预期。
- 同阶选择未规定时，只验证集合、范围、对齐和计数。
- 正常用例失败立即产生非零结果。
- 异常用例只有命中预期断言才通过。
- 测试结束生成各 Case 与结果的明确映射。
- 报告区分宿主算法测试、代码审查和未执行的目标环境测试。
- 本轮不宣称完成 RISC-V 架构、启动布局、页表或 QEMU 集成验证。


---

<a id="c-all"></a>

# SLUB Challenge 与 QEMU 调试（C，原八项任务）

负责人：程子涵，2413921。以下任务按顺序执行；每项都要求依据当前仓库真实源码操作，并按对应范围提交结果。最终提交包含 `code/`、`report/report.md`、本文件，以及报告正文实际引用的 `report/images/` 截图；未引用的截图、日志和临时构建文件在完成后删除。路径以仓库根目录为基准。

<a id="task-1"></a>
## 任务 1：SLUB 设计实现与内核接入

[PROMPT]

**任务**：在 `code/kern/mm/slub.c`、`code/kern/mm/slub.h` 中实现简化 SLUB 任意大小内存对象分配模块，并在 `code/kern/init/init.c`、`code/Makefile` 中完成受开关控制的内核接入。负责人：2413921 程子涵。源码路径以仓库根目录为准，均以 `code/` 开头。

SLUB 采用两层结构：第一层通过已有 PMM 申请和释放连续物理页；第二层将单页划为固定大小的对象槽，用 bitmap 管理占用。小对象按 16、32、64、128、256、512、1024、2048 字节分类，选择不小于请求的最小类别；大于 2048 字节的对象单独申请连续多页。最后一个对象释放时立即归还所属页，避免空 slab 长期占用物理内存。

**操作要求**：必须修改工程中的真实文件，不能仅展示代码或补丁。先阅读真实接口与已完成实现；已有模块应按本规格核对并修订，不得清空其他成员代码。保留已有注释和 `lab2` 标记，C 的新增标记使用 2413921。只允许修改下列白名单，不得借此重写 PMM 或调整测试以掩盖错误。

**输出要求**：交付可编译源码和头文件、条件接入代码；说明对象分类、元数据、失败原子性与回收顺序，列出实际改动、实际验证命令和未验证项。本综合提示词将 SLUB 实现和测试分别列为任务一、任务二；本任务负责分配器实现，完整 `slub_check()` 与宿主测试由[任务二](#task-2)完成。代码、报告和附件由当前交付统一提交至 `lab2-cheng` 分支。

[RELY]

## 1. 真实文件与公共页接口

必须阅读 `code/kern/mm/pmm.h`、`memlayout.h`、`mmu.h`、`pmm.c`、`buddy_pmm.c`、`code/kern/init/init.c`、`code/libs/defs.h`、`code/libs/string.h`、`code/kern/debug/assert.h`、`code/Makefile`。

```c
// code/kern/mm/pmm.h，声明/内联接口，不能改签名
extern struct Page *pages;
extern size_t npage;
extern const size_t nbase;
extern uint64_t va_pa_offset;
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
static inline uintptr_t page2pa(struct Page *page);
static inline struct Page *pa2page(uintptr_t pa);
```

`PGSIZE=4096` 来自 `mmu.h`。物理页对应可访问内核地址为 `page2pa(page)+va_pa_offset`；当前 `pmm.h` 中 `KADDR` 宏已注释，不能依赖它。`panic` 是 `code/kern/debug/assert.h` 中转发到 `__panic` 的宏，不是另一个普通函数声明。链表和基础类型实际位于 `code/libs/list.h`、`code/libs/defs.h`，不是 `code/kern/libs/`。

默认管理器是 Best-Fit，`BUDDY=1` 选择 Buddy。Best-Fit 按 n 页记账；Buddy 将请求向上取整为二次幂，但释放必须传回原始请求 n，例如请求 3 页实际占 4 页时调用 `free_pages(page,3)`。SLUB 不得访问任何管理器的私有链表、`records` 或改写 `struct Page` 字段。

## 2. 现有编译和调用关系

Makefile 自动收集 `code/kern/mm` 的 C 文件，已有 `BUDDY` 选择；C 使用 `SLUB_CHECK=1` 控制 `MEMBER_C_SLUB_CHECK`。检查必须在 `pmm_init()` 成功后调用。`MMU_PROBE` 为另一任务的独立开关，本任务不得删除或改变。架构参数由 `ARCH_FLAGS` 提供，不能用覆盖 `DEFS` 的方式丢失已有宏。

[GUARANTEE]

## 1. 必须实现或保持的接口

```c
// code/kern/mm/slub.h 对外公开；本任务实现前五个
void slub_init(void);
void *slub_alloc(size_t size);
int slub_free(void *ptr);
size_t slub_object_size(const void *ptr);
size_t slub_owned_pages(void);
void slub_check(void);  // 只保留声明；定义由任务二完成
```

`slub_init()` 可重复调用，不能丢失存活分配。`slub_object_size()` 对有效小对象返回类别大小、大对象返回原始请求长度，对无效地址返回 0。`slub_owned_pages()` 返回当前未归还的原始请求页数，不能宣称包含 Buddy 额外舍入占用。

允许在 `slub.c` 内添加私有结构、静态变量和辅助函数，包括头部大小计算、bitmap 查询、slab 创建、对象定位；不暴露 PMM 私有状态。每个 slab 保存原始 `Page*`、原始请求页数、对象长度、容量、存活数、bitmap 和链表关系。所有对象起始地址至少 16 字节对齐。

## 2. 修改边界

- 允许：`code/kern/mm/slub.c`、`slub.h`；`init.c` 中必要头文件包含和 `MEMBER_C_SLUB_CHECK` 条件调用；Makefile 中 SLUB 开关及必要构建接入。
- 禁止：修改 `pmm.c`、`pmm.h`、`memlayout.h`、`mmu.h`、`default_pmm.c`、`best_fit_pmm.c`、`buddy_pmm.c` 的现有实现和接口；修改原有 `basic_check()`、`best_fit_check()`、评分脚本与已有测试；更改默认 Best-Fit 或 BUDDY 切换契约。
- 不得以空实现的 `slub_check()` 冒充测试通过；任务二未完成时应明确指出检查依赖。

[SPECIFICATION]

## 一、初始化和通用不变量

**Pre-Condition**：单 hart、非并发、非中断重入；PMM 已成功初始化，页接口和地址偏移有效。

**Post-Condition**：活动 slab 互不重叠；每个槽最多发放一次；bitmap 占用数等于存活数；SLUB 仅拥有自己通过公共接口取得的页。

**Case 1：首次显式或惰性初始化。** 建立空类别链表，已拥有页数为 0；首次 `slub_alloc` 可触发初始化。

**Case 2：存在存活对象时再次初始化。** 保留链表、bitmap、对象内容与计数，已有对象仍可正常释放。

## 二、slub_alloc(size)

**Pre-Condition**：满足通用运行约束；允许任意 `size_t` 输入。

**Post-Condition：成功时**：返回至少可使用 size 字节的 16 字节对齐地址；不覆盖元数据或其他对象；仅在底层申请成功后登记新 slab。

**Case 1：size 为 0。** 返回 NULL，不向 PMM 取页、不改变已有状态。

**Case 2：1—2048 字节。** 使用最小适配类别，例如 16→16、17→32、2048→2048。优先在该类别未满 slab 中找空槽；全部已满时取一页并扣除对齐头部后计算容量。

**Case 3：大于 2048 字节。** 计算 `ceil((size+header)/PGSIZE)`，为对象独占申请连续多页；4096 字节请求加头部需要 2 页；8193 字节加头部需要 3 页，释放仍传 3。

**Case 4：长度或页数算术溢出。** 在加头部、加 `PGSIZE-1` 等运算之前检查上界，返回 NULL；不得先溢出再检查，不把极大值变成小分配。

**Case 5：空闲总量不足，或总量足够但无连续块，或 PMM 返回 NULL。** 返回 NULL，活动对象内容、类别结构与页数计数保持不变，不能解引用 NULL 或发放虚构对象。

## 三、slub_free(ptr)

**Pre-Condition**：允许任意指针值；有效释放只能使用分配返回的对象起始地址。

**Post-Condition**：合法对象恰好释放一次；失败时不改变对象占用和 PMM 状态。

**Case 1：NULL。** 返回 0，不改变状态。

**Case 2：有效对象且 slab 仍有其他存活对象。** 清除对应 bitmap 位、递减存活数，返回 0；该槽可复用，其他对象数据保持。

**Case 3：释放 slab 最后一个对象。** 先保存页指针、原始页数和后继并摘链、更新计数，再调用 `free_pages`；归还后不得读写该页元数据。

**Case 4：未知指针、内部指针或未重分配前的重复释放。** 先在可信活动链表中查归属，再验证槽边界和占用；返回 -1，不先从不可信地址推导并解引用头部。

**Case 5：旧指针地址已被重新分配给新对象。** 原始指针接口不能区分两次生命周期；说明限制，不能宣称检测此类悬空指针。

## 四、查询与验收

`slub_object_size` 和 `slub_owned_pages` 不改变状态；未初始化时查询不得解引用无效链表。分配不承诺清零，不承诺高于 16 字节的任意对齐，不声称具备 Linux 的 per-CPU、NUMA 或并发能力。

依赖齐全时执行 `make clean && make SLUB_CHECK=1`，检查真实构建错误；完整行为验证转交任务二。输出应包含实际接口、修改清单、当前已完成和未完成的验证，不预填“全部通过”。

<a id="task-2"></a>
## 任务 2：SLUB 独立测试与运行证据

[PROMPT]

**任务**：为已有 SLUB 实现编写独立测试，实现 `code/kern/mm/slub_check.c`，完善 `code/tools/member_c/host_test.c`、`run_host.sh`、`run_qemu.py` 和必要的测试接入。负责人：2413921 程子涵。本任务只验证对象分配模块，不重新实现或替换分配算法。源码路径以仓库根目录为准，均以 `code/` 开头。

测试原理：通过公共 API 申请对象，用独立预期值、区间、数据模式和 PMM 空闲计数验证行为。内核检查验证真实地址和最终 PMM 的兼容；宿主模型用于可控失败、耗尽与碎片化，并用 ASan/UBSan 检测内存问题。两类结果必须分开陈述。

**操作要求**：必须修改工程中的真实测试文件，不能仅输出测试示例。先阅读真实 `slub.h`、`slub.c` 和页接口，不包含分配器 C 文件、不访问私有 bitmap/链表、不复制分配策略做“同源预期”。禁止调整实现、已有框架测试或评分规则以让新测试通过。

**输出要求**：交付可执行测试、运行脚本和覆盖矩阵；运行日志仅作临时验证材料，最终提交前删除。逐项说明输入、预期、实际结果和检验价值；未运行或失败必须如实记录。不得把宿主模型成功写成内核成功，不得把 20/20 评分写成 SLUB 测试分数。

[RELY]

必须阅读 `code/kern/mm/slub.h`、`slub.c`、`pmm.h`、`memlayout.h`、`code/libs/defs.h`、`code/libs/string.h`、`code/libs/stdio.h`、`code/kern/debug/assert.h`、`code/kern/init/init.c` 和 Makefile。

```c
void slub_init(void);
void *slub_alloc(size_t size);
int slub_free(void *ptr);
size_t slub_object_size(const void *ptr);
size_t slub_owned_pages(void);
void slub_check(void);
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
```

小对象类别为 16—2048 的八个二次幂；大对象查询返回原始请求大小。零大小与溢出返回 NULL；NULL 释放返回 0；无效、内部和未重分配重复释放返回 -1。`slub_init` 重入不能清空存活对象。`slub_owned_pages` 是原始请求页数；Buddy 的实际占用可能更大，不能将其与 PMM 扣页数逐次强行等同。

宿主测试应链接真实 `slub.c` 和 `slub_check.c`，仅替换 PMM backing store。`pmm.h` 的 `page2pa` 返回 `uintptr_t`、`pa2page` 参数为 `uintptr_t`；可用 256 个 4096 字节对齐页与真实 Page 描述符，通过 `va_pa_offset` 对接宿主内存。内核头文件定义了自己的基础类型，宿主适配不能混入不兼容的 libc 类型定义。`panic` 为宏，平台实现应提供 `__panic`，不另造普通 `panic()`。

[GUARANTEE]

## 1. 必须实现/修改的入口

```c
// code/kern/mm/slub_check.c
void slub_check(void);
// code/tools/member_c/host_test.c，宿主专用定义，不进入内核链接
int main(void);
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);
int cprintf(const char *fmt, ...);
void __panic(const char *file, int line, const char *fmt, ...);
```

宿主必须提供 `pages`、`npage`、`nbase`、`va_pa_offset` 的匹配定义；实现可靠的忙闲记录、原始页数检查、可控下一次申请失败。`__panic` 应终止并返回失败状态。脚本入口保持 `make member-c-host`；内核运行器保留 `--qemu`、`--manager`、`--output`、`--pmm-only` 参数。

## 2. 修改边界与系统约束

- 允许修改上述测试/脚本；Makefile 的测试目标；`init.c` 中 `MEMBER_C_SLUB_CHECK` 条件调用和测试后 PMM 不变量检查；`report/report.md` 的 SLUB 测试记录。
- 禁止修改 `slub.c` 的分配逻辑、公共头文件已有接口、A/B 管理器、原有 Best-Fit 测试、注入评分文件。真实缺陷应报告并由实现任务修复，不改预期掩盖。
- 宿主模型与内核对象隔离构建；不得把原生对象链接进 RISC-V 内核。内核大数组采用静态存储，避免超出启动栈。
- 保留默认 Best-Fit、BUDDY 开关和 MMU_PROBE；切换参数先 `make clean`。脚本采用 LF，任何失败应返回非零。

[SPECIFICATION]

**Pre-Condition**：SLUB 实现已完成；内核 PMM 可运行；宿主具有 GCC/Python，QEMU 验证另需交叉编译器和 QEMU。

**Post-Condition**：每项测试有可观察断言；全部释放后 SLUB 占页为 0、PMM 空闲数恢复各自初值；实际日志可追踪。

**Case 1：类别边界。** 请求 1、15、16、17、31、32、33，继续覆盖各类别前/等于/后值直到 2047、2048、2049，再测试 4095、4096、4097、8193。逐项比较明确类别或大对象原始长度，检查 16 字节对齐，不仅断言“容量足够”。

**Case 2：多对象隔离。** 同时保留各大小对象，两两比较请求范围不重叠；为各对象写不同字节模式，全部分配后逐字节复核，发现后续分配覆盖。

**Case 3：初始化幂等。** 存活对象存在时再次 `slub_init()`，原指针仍合法、内容不变、均能释放。

**Case 4：页内复用。** 保留 32 字节 anchor，释放同类 hole 后再申请，验证复用 hole 且 anchor 内容不变；anchor 保持该 slab 活动，排除 PMM 重新返回相同物理页的干扰。

**Case 5：跨页和多轮回收。** 申请 320 个 16 字节对象，净数据已超一页；验证跨页、唯一指针和内容，先释放偶数再释放奇数，执行四轮，每轮恢复 PMM 初值和零 SLUB 占页。

**Case 6：非法输入。** 检查 0、`(size_t)-1`、NULL、栈地址、对象内部偏移、未重新分配的重复释放；分别得到契约规定的 NULL/0/-1，状态不变。不得要求无法保证的生命周期识别。

**Case 7：总量不足。** 请求大于当前可用页总量的对象，预期 NULL；失败前后计数一致，已有对象不受损。

**Case 8：宿主底层失败。** 注入下一次 `alloc_pages` 返回 NULL；分别覆盖空 SLUB 首次分配和保留已有对象时的大对象失败，确认计数和已有内容不变。

**Case 9：耗尽与连续性失败。** 先占满 256 页，SLUB 无法取新页；再释放每隔一页形成 128 个单页洞，申请需要连续两页的 4096 字节对象应失败。最后归还全部并重跑检查。

**Case 10：最终管理器联调。** 分别干净构建默认 Best-Fit＋SLUB、BUDDY=1＋SLUB；日志必须同时确认实际管理器名称、`check_alloc_page() succeeded!`、`slub_check() succeeded!` 和页数恢复。Buddy 大对象 8193 字节覆盖原始 3 页请求释放，SLUB 后再次检查底层不变量。

**Case 11：运行器失败。** 无镜像、缺少标记、管理器不符、panic 或启动失败必须非零。只结束自己启动的 QEMU；内核无限循环用有界观察窗口终止，不把预期 SIGTERM 当 panic，也不掩盖提前异常退出。

执行并记录 `make member-c-host`（ASan/UBSan）、两套内核命令和日志位置；无对应环境时标明未执行。报告说明测试覆盖范围，不声称并发、性能、NUMA 或所有可能请求序列已证明。

<a id="task-3"></a>
## 任务 3：QEMU 调试一：环境、真实访存与关键分支

[PROMPT]

**任务**：完成 QEMU 调试任务一：建立 QEMU 4.1.1 双 GDB 环境，定位真实客体数据访存的关键调用路径和分支，并通过一条真实 load 演示虚拟地址到物理地址的转换。目标为 `code/kern/mm/mmu_probe.c`、`code/tools/member_c/capture_debug.py`、`record_environment.py` 及 `report/report.md` 中“调试任务一”小节；负责人 2413921 程子涵。源码路径以仓库根目录为准，均以 `code/` 开头。

原理和功能：客体 RISC-V 指令由 QEMU TCG 执行；软件 TLB 未命中时进入数据 helper、refill 和 RISC-V 页表翻译函数。通过客体 GDB 固定实际访存，再由宿主 GDB 观察 QEMU C 代码，避免把静态源码分析或调试器读内存误当作客体执行证据。

**操作要求**：必须修改真实探针、脚本和报告文件，不能只给命令或示意调用链。已有实现先核对并保留正确部分。QEMU 源码只读，调试构建放在工程外的可配置目录；不能修改 QEMU 来制造指定输出。

**输出要求**：记录 QEMU 源码版本、实际二进制和哈希、编译配置、双 GDB 角色、操作顺序、条件断点、真实调用栈、访问类型、satp、mmu_idx 与翻译结果。路径结论应有源码位置和原始日志，实际失败或缺工具如实说明，不生成伪造运行记录。

[RELY]

必须阅读 `code/kern/init/entry.S`、`code/kern/init/init.c`、`code/kern/mm/mmu_probe.c`、`pmm.h`、Makefile 和 `code/tools/member_c` 现有脚本。内核通过静态 Sv39 启动映射进入高地址，探针调用位于 `pmm_init()` 前，不依赖对象分配。

QEMU 4.1.1 源码重点为 `target/riscv/cpu_helper.c`、`accel/tcg/cputlb.c`、`tcg/i386/tcg-target.inc.c`；真实源码签名包括：

```c
static int get_physical_address(CPURISCVState *env, hwaddr *physical,
                               int *prot, target_ulong addr,
                               int access_type, int mmu_idx);
```

数据慢路径涉及 `helper_le_ldq_mmu`、`load_helper`、`tlb_fill`、`riscv_cpu_tlb_fill`、`get_physical_address` 和 `tlb_set_page`。宿主 GDB 调试 QEMU 的本机代码，`gdb-multiarch` 调试 RISC-V `code/bin/kernel`。QEMU 的 GDB stub 地址为 `127.0.0.1:12349`；端口冲突时明确失败，不能连接无关进程。

[GUARANTEE]

## 1. 必须实现/保持的入口

```c
// code/kern/mm/mmu_probe.c
void mmu_probe(void);
volatile uint64_t mmu_probe_word;        // 4096 字节对齐，值 0x1122334455667788
volatile uint64_t mmu_probe_results[2];
// 汇编全局标签，不是需要定义为 C 函数的接口：
// mmu_probe_load_first / mmu_probe_load_second
```

探针使用 `sfence.vma` 后的两条相邻真实 `ld` 读取同一数据地址；内联汇编必须采用正确输入/输出约束和 memory clobber，禁止编译器合并读取。`init.c` 中只在 `MEMBER_C_MMU_PROBE` 开启时调用；Makefile 保持 `MMU_PROBE=1` 与 `SLUB_CHECK`、`BUDDY` 独立。

脚本须保持 CLI：`capture_debug.py --qemu PATH --source DIR [--step]`；`record_environment.py --qemu PATH --source DIR [--archive PATH]`；QEMU 构建脚本保留 `QEMU_BUILD_ROOT` 和 `JOBS` 环境参数。CLI 是脚本入口，不要求虚构 C 函数接口。

## 2. 修改边界

允许：上述 C 探针与脚本、必要的 code/Makefile MMU 开关、`init.c` 条件调用、报告中的复现命令、报告任务一、本次运行生成的临时 trace 结果（提交前删除）。允许按需完善 `build_qemu.sh`。

禁止：修改 A/B 分配器、公共 Page/PMM 接口、SLUB 算法、原有框架测试/评分；修改 QEMU 翻译算法；生成替代 First-Fit 初始化冒充最终内核；硬编码个人绝对路径作为脚本唯一配置。

## 3. 系统约束

从实际 `code/bin/kernel` 符号读取 VA 和标签 PC，不套用历史调试地址；根目录由脚本自身定位。宿主 GDB 暂停时客体可能等待 stub，必须让宿主继续后才能完成客体单步。只清理脚本自身启动的进程组，设置有界等待；失败保留诊断日志，不终止其他调试会话。

[SPECIFICATION]

**Pre-Condition**：最终 A+B+C 源码已集成；交叉工具链、两种 GDB 和调试版 QEMU 可用；源码与二进制版本可核对。

**Post-Condition**：一次真实数据读取的路径和 PA 有日志支持，脚本可复现，默认内核配置不受影响。

**Case 1：环境准备。** 记录宿主系统、编译器、两个 GDB、QEMU 版本、configure、实际路径和哈希。已有调试版可复用；若缺失，使用配置目录构建，不要求 make install。

**Case 2：真实访存。** 客体停在第一条 ld，记录 PC、目标 VA、satp 和指令；宿主设目标地址且 access_type=数据读的条件断点，客体 stepi 触发真实 helper/refill 调用栈。

**Case 3：关键分支。** 结合真实源码解释有效特权级/MPRV、Bare 与 Sv39、地址规范性、PTE 有效性、下级页表/叶子、权限和超级页对齐、A/D、PMP 与异常返回。未触发的失败分支只作为源码分析，不能写成实测。

**Case 4：调试器干扰。** `riscv_cpu_get_phys_page_debug` 等调试读取与真实 `riscv_cpu_tlb_fill` 调用区分记录；REAL_WALK 应过滤调用者，取指与数据读分别标注。

**Case 5：环境错误。** 无二进制、无符号、源码版本不匹配、端口占用、GDB 失败或超时必须非零并指明日志，不将空日志视为成功。

**Case 6：版本重新链接。** PC 改变时自动读取新符号；报告明确历史调试观察和最终运行地址的差异。环境哈希对应本次真正运行的内核和 C 源文件。

验证命令为干净构建 `make SLUB_CHECK=1 MMU_PROBE=1` 后执行 capture_debug 的 trace 模式并记录环境。报告给出实际栈与分支解释，不能只列函数名。

<a id="task-4"></a>
## 任务 4：QEMU 调试二：页表单步和 PA 手算

[PROMPT]

**任务**：完成 QEMU 调试任务二：在真实客体 load 触发的 `get_physical_address()` 中单步页表翻译，记录实际 PTE 并手工核算最终 PA。必须修改 `code/tools/member_c/capture_debug.py` 的 `--step` 路径与 `report/report.md` 中“调试任务二”，保存真实单步日志。负责人 2413921 程子涵，工程路径为仓库根目录。

**原理与功能**：satp 给出页表模式和根 PPN，VPN 逐级选择 PTE；遇叶子时按页面级别组合 PPN、低位 VPN 与页内偏移。Sv39 支持三级不意味着每次都读三个 PTE；本实验根项可直接映射 1 GiB 超级页，必须以实际叶子条件决定走访次数。

**操作要求**：必须修改真实脚本和报告，而非只展示推导。沿任务一已经建立的最终集成内核调试，使用宿主 GDB 的 next/finish 观察变量；不得修改 QEMU、内核页表或分配器来匹配预设数值。

**输出要求**：提供访问类型、VA、satp、根 PA、各级 VPN、实际读取层数、PTE 地址/值、PPN、叶子标志、PA、prot 和返回状态；逐步公式与日志相互核对，并解释未执行赋值导致的无效局部值。

[RELY]

读取 `code/kern/init/entry.S`、`code/kern/mm/mmu_probe.c`、`code/tools/member_c/capture_debug.py`、环境记录和 QEMU 4.1.1 的 `target/riscv/cpu_helper.c`。关键目标仍是：

```c
static int get_physical_address(CPURISCVState *env, hwaddr *physical,
                               int *prot, target_ulong addr,
                               int access_type, int mmu_idx);
```

Sv39 的 levels=3、ptidxbits=9、ptesize=8、PGSHIFT=12。PTE 的 PPN 从 bit 10 开始；V/R/W/X/A/D 的含义以当前源码为准。当前已记录实例 VA=`0xffffffffc0208000`、satp=`0x8000000000080206`，仅可用作交叉检查，必须从当前运行读取，不能硬写成“本次实测”。

[GUARANTEE]

必须保持 `capture_debug.py --qemu PATH --source DIR --step` 入口与 trace 模式；新增/修订宿主源级断点、next/finish 命令和 step 日志输出。无需新增 C 对外接口，不改变 `get_physical_address` 的任何参数、返回值或实现。

允许：该脚本的 step 模式、报告任务二、报告中的单步说明，以及本次运行生成的宿主/客体 step 临时结果（提交前删除）。禁止：改动任务一 trace 语义、PMM/SLUB、启动页表和 QEMU 算法、已有调试观察中的数值；不得删掉出现未初始化值的真实日志以伪装“全程有效”。

系统约束：宿主/客体同步并设置超时；使用真实源文件定位赋值行，不能盲套其他 QEMU 版本行号。PC、PTE 和有效页大小均从当前运行确定；成功分页路径的 `physical` 是物理页基址，应另组合字节偏移，不能普遍当作完整 PA。

[SPECIFICATION]

**Pre-Condition**：任务一 trace 已确认真实数据读路径；最终内核、调试版 QEMU 和源码一致。

**Post-Condition**：日志出现源级单步及有效结果，手算和运行一致；无法取得有效局部变量时明确标记，不以猜测补数。

**Case 1：根表定位。** 读取 satp.MODE、ASID、PPN，计算根 PA=`PPN<<12`；从 VA 提取 VPN[2:0] 和 offset，检查 Sv39 高位规范性。

**Case 2：实际 PTE。** 在读取后记录 `i`、`ptshift`、`idx`、`pte_addr`、`pte`；核算 `root+idx*8`，解出 PPN 和权限。若是下级表才继续，不预设读满三级。

**Case 3：根级叶子。** 当前实例根索引 511、PTE 地址 `0x80206ff8`、PTE=`0x200000cf`；核对 PPN=`0x80000` 且低 18 位为 0。若实测相同，说明为何 i=0 就返回。

**Case 4：超级页物理地址。** 结合剩余 VPN 位计算物理页基址，再加低 12 位 offset；当前实例应得到 `0x80208000`。不得把 1 GiB 叶子错误地按普通 4 KiB PTE 处理。

**Case 5：赋值前后。** 刚停在 `ppn=pte>>...` 时不能采用旧 ppn；next 后再读。finish 停在调用表达式时 ret 可能尚未赋值，应再 next 或使用真实函数返回状态，区分它与 pa/prot 输出参数。

**Case 6：不同映射或失败。** 若重新链接改变表位置，全部按新数值重算；若出现无效 PTE/权限错误，只记录实际失败并解释源码分支，不强造成功路径或编造未触发异常。

完成后核对两条客体读取都得到预期值，报告用文字列出页表核算值并注明运行版本；未执行的单步不得标记完成。

<a id="task-5"></a>
## 任务 5：QEMU 调试三：TLB 查询、miss/fill/hit

[PROMPT]

**任务**：完成 QEMU 调试任务三：定位 QEMU 4.1.1 中软件 TLB 查询、miss、填充和 hit 的实现，通过连续两次真实数据 load 给出证据。必须修改 `code/tools/member_c/capture_debug.py` 的 trace 观察点和 `report/report.md` 中“调试任务三”，保存原始日志。负责人 2413921 程子涵；工程路径为仓库根目录。

**原理与功能**：SoftTLB 按访问类型保存 tag 与宿主地址换算 addend；TCG 生成的代码可直接检查 tag 并完成命中访存，未命中才走 C helper。既要找到 C 查询代码，又要解释为什么第二次 hit 可能不会进入 `load_helper`。

**操作要求**：必须修改真实脚本/报告，不能只给调用关系图。只读 QEMU 源码和运行时变量，不改表项、计数或寄存器制造命中。保持任务一的地址过滤和任务二的 step 模式。

**输出要求**：给出查询源码位置、index 计算、读/写/取指 tag 区别、victim 作用、首次未命中及 fill、第二次读取完成和慢路径次数。每个结论注明源码分析还是实测，不能仅以“不触发断点”证明成功。

[RELY]

阅读 QEMU 4.1.1 的 `accel/tcg/cputlb.c`、`include/exec/cpu_ldst.h`、`include/exec/cpu-defs.h`、`tcg/i386/tcg-target.inc.c`、`target/riscv/cpu_helper.c`，以及本工程 `mmu_probe.c` 和 capture 脚本。

已有观察目标为 `load_helper`、`tlb_hit`、`victim_tlb_hit`、`tlb_fill`、`riscv_cpu_tlb_fill`、`tlb_set_page`、`cpu_handle_debug_exception` 与 x86 TCG 后端 `tcg_out_tlb_load`。实际结构有 addr_read、addr_write、addr_code、addend；当前运行可从 `RISCVCPU` 的 `neg.tlb.f[mmu_idx]` 查看 fast 表与 mask，必须核对版本布局。

探针的 `mmu_probe_load_first`、`mmu_probe_load_second` 标签标记对同一 `mmu_probe_word` 的两次 ld；标签地址由当前内核符号获取。现有自动脚本的 `Query.stop()`、`Fill.stop()`、`DebugStop.stop()` 用于记录查询、refill 与暂停快照。

[GUARANTEE]

必须保持脚本 `--qemu`、`--source`、`--step` CLI，修改 trace 模式中上述观察方法和对应 GDB 命令。无需新增内核 C 接口；不得改动 QEMU 现有目标函数的实现或签名。

允许：capture 脚本 trace 观察、报告任务三、报告中的对应解释、本次运行生成的宿主/客体 trace 临时结果（提交前删除）。禁止：改内核探针语义、强写 SoftTLB、绕过 `sfence.vma`、混入调试器读内存、改其他成员模块/框架测试、把固定调试条件当作通用配置。

系统约束：分别在第一条 ld 前、第一条后、第二条后取得快照；index 从当前 VA、表项大小和 mask 计算；addend 是客体 VA 到宿主访存地址的换算资料，不是客体物理页号。日志只统计目标数据访问，取指与调试器旁路另行区分。

[SPECIFICATION]

**Pre-Condition**：任务一已建立真实 load 路径；两次 load 读同一数据地址，客体与宿主调试能同步。

**Post-Condition**：已执行的两条指令、表项变化与一次目标慢路径有交叉证据；同时解释 C helper 与生成代码的关系。

**Case 1：首次查询。** 记录 index、tag、code_read、访问类型，判断主表未命中；若 victim 未匹配而进入 refill，以实际路径说明，不编造 victim 命中实验。

**Case 2：填充后。** 记录 addr_read/write/code 和 addend；只有实际权限允许的类型才解释为有效。当前案例可见目标页 tag 和 prot=7，不推广为所有页都有相同权限。

**Case 3：第二次 hit。** 两条 ld 均单步完成，a3/a4 都为探针预期值；第二次后表项仍有效，目标 REAL_WALK/refill 只有一次。结合 TCG tag 比较与慢路径跳转源码解释命中，不要求每次 hit 都进入 C 函数。

**Case 4：容量与冲突。** 若当前 mask=2016、表项 32 字节，可推算本次 64 个索引槽；明确这是当前快照，不声称 QEMU 或真实硬件始终只有 64 项。不要求未实施的冲突实验被写成完成。

**Case 5：异常观察。** 两次读取值不一致、多次 refill、缺快照、调试器失败时不判通过；先排查 sfence、取指干扰、过滤和版本，再据实记录。

运行 trace 模式并用 guest/host 日志核对。报告列出首次 miss、填充、第二次 hit 与寄存器值，注明运行版本；不伪造性能测量。

<a id="task-6"></a>
## 任务 6：QEMU 调试四：Bare/Sv39 与硬件 TLB 比较

[PROMPT]

**任务**：完成 QEMU 调试任务四：调试一条 satp=0 的真实 Bare 数据存储，与已取得的 Sv39 数据读取路径比较，解释 QEMU SoftTLB 和真实 CPU TLB 的逻辑区别。必须修改 `code/tools/member_c/capture_debug.py` 的 Bare 观察和 `report/report.md` 中“调试任务四”。负责人 2413921 程子涵，路径以仓库根目录为准。

**原理与功能**：硬件 Bare 关闭分页地址翻译，但 QEMU 仍须把客体访问导向宿主 RAM/MMIO；其软件缓存不仅缓存客体 VA→PA，还服务模拟器的宿主访存加速。需要实际路径对照支撑这一解释，不能把 QEMU 软件表结构当作硬件微架构。

**操作要求**：必须修改真实脚本和报告，不能只给理论问答。只观察现有入口的真实 `sd`，不修改启动页表、satp 时序、OpenSBI、QEMU 翻译逻辑或 PMP 配置。

**输出要求**：记录 Bare 的指令 PC、数据地址、satp、访问类型、mmu_idx/有效模式、翻译状态与 PA，并与 Sv39 的页表读取和填充结果逐项比较。区分“本次没有分页遍历”“仍受保护规则约束”和“软件缓存用于宿主寻址”，不给出无证据的硬件结构结论。

[RELY]

必须阅读 `code/kern/init/entry.S`、`code/kern/mm/mmu_probe.c`、capture 脚本、QEMU 4.1.1 的 `target/riscv/cpu_helper.c` 与 `accel/tcg/cputlb.c`。本工程入口在写 satp 前通过 `sd a0,0(t0)` 保存 boot_hartid；具体 PC 和保存位置从实际反汇编/符号读取。

Bare 的目标函数仍是 `get_physical_address` 与 `riscv_cpu_tlb_fill`，数据存储慢路径含 `store_helper`；`MMU_DATA_STORE=1`、`MMU_DATA_LOAD=0`。satp.MODE=0 走 Bare 恒等地址分支；有效特权级应结合 mmu_idx、MPRV/MPP 判断，不能仅凭 satp 推断当前特权级。

当前记录 Bare 存储地址为 `0x80207000`，Sv39 目标 VA 为 `0xffffffffc0208000`；它们是已记录实例，不是可写死的普遍约定。启动符号换算必须采用当前映射偏移 `PHYSICAL_MEMORY_OFFSET`，不要混用 KERNBASE 与 DRAM_BASE。

[GUARANTEE]

实现/保持 capture CLI 和真实 Bare 的 `Walk.stop()`、`Fill.stop()` 观察分支；不新增内核对外接口，不改 entry.S 的保存动作。允许完善脚本符号换算与过滤，保持 trace/step 两模式可用。

允许修改：capture 脚本的 Bare 观察、报告任务四、报告与本次实际 trace 结果。禁止修改：内核启动流程、A/B/C 分配算法、QEMU 源码、已有调试观察、已有评分/测试；不通过调试器手动改 satp 或内存制造所需路径。

系统约束：只计入调用者为真实 CPU refill 的地址翻译，不把 GDB 读内存或入口取指当作 Bare 数据存储证据；区分客体 PA 和宿主地址。脚本有界等待且只结束自己创建的进程。

[SPECIFICATION]

**Pre-Condition**：最终集成内核可运行；调试版 QEMU 与源码匹配；已具备 Sv39 load 对照日志。

**Post-Condition**：Bare 数据存储和 Sv39 load 都有真实证据，逻辑比较不超出可观察范围。

**Case 1：Bare 的真实 sd。** 在启动保存 hartid 的数据地址设置 store 条件，记录 satp=0、access=1 和 `store_helper→tlb_fill→riscv_cpu_tlb_fill→get_physical_address`，不能只用取指代替。

**Case 2：Bare 地址结果。** 输入地址与输出 PA 相同，无 PTE 遍历，仍有软件 refill/fill 路径；若实际路径不同，保留输出并按源码解释，不伪造恒等转换。

**Case 3：Sv39 对照。** 使用任务二的实际 VA、satp、PTE 与 PA，说明开启分页后的叶子与权限处理。对比的是分页逻辑，不由地址不同推断额外耗时。

**Case 4：保护约束。** 说明 Bare 无分页不等于无 PMP/特权检查；没有动态触发保护异常时，只写源码分析，不声称已测完整保护机制。

**Case 5：软件与硬件差别。** 解释 SoftTLB 的宿主地址 addend、RAM/MMIO 分派、生成代码快路径。不得把软件直接索引、victim、容量或 Bare 仍查询缓存推广成真实 CPU 的强制实现。

**Case 6：缺少证据。** 若 Bare store 未命中、符号地址错误或只有调试器路径，任务不判完成；先检查入口反汇编和条件，再重采或明确未完成。

在报告中说明已有 Bare 存储观察并注明时间，结合最终 trace 结果；不为配图改写实际结果。

<a id="task-7"></a>
## 任务 7：QEMU 调试五：真实现象与理解变化

[PROMPT]

**任务**：完成 QEMU 调试任务五：从真实调试记录提炼有趣现象、先前误解、判断依据和理解变化，并写入 `report/report.md` 的“调试任务五：实际观察与理解变化”。负责人 2413921 程子涵，工作目录为仓库根目录。

**原理与功能**：将模拟器内部变量、实际指令结果和操作系统概念联系起来，说明为什么原来的判断不充分，以及怎样通过新证据修正。此任务是证据驱动的报告写作，不新增内核算法。

**操作要求**：必须修改真实 `report/report.md`，不能只在对话中给提纲。不制造趣事、错误、崩溃、性能数据或个人操作经历；已有调试观察只用于支持有据可查的结论，AI 执行和本人操作须如实区分。

**输出要求**：每个现象按“当时看到什么→原先怎样理解→为什么不充分→证据和最终解释”展开，用报告中列出的实测值与源码位置支持。至少覆盖页表层数、赋值时机、TCG 命中快路径和双 GDB 配合，不重复堆砌函数名。

[RELY]

只读输入：当前报告中的实测结果与调试观察、最终源码、用户提供的任务要求，以及可核对的 QEMU 4.1.1 对应源码。若某个原始日志或图片未随最终交付保留，不得声称可以从交付目录复查该材料。

实际现象包括：Sv39 levels=3 但根级叶子 i=0 返回；停在赋值行时 ppn/ret 可能尚未有效；第二次 ld 可能不进入 C helper；宿主 GDB 暂停会使客体 GDB 等待。最终内核两条 ld 的 PC 与早期调试记录不同，但目标 VA/PTE 可一致；须注明版本。

[GUARANTEE]

本任务没有必须新增或修改的 C/Python API；唯一必须写入的接口是报告中约定的小节及其源码/实测值依据。涉及 `get_physical_address`、`load_helper`、`tcg_out_tlb_load` 的名称只用于解释，不修改其签名或实现。

允许：修改报告该小节；必要时修正同一报告内相互引用的标题。禁止：改代码、其他任务结果或无法核实的历史材料、A/B 的个人叙述；不得把整理后提示词冒充历史逐字聊天记录。

系统约束：图文以对应运行版本为准；变量有效性必须结合当前源码行；不以一次成功路径证明所有异常或真实硬件性能；不要为凑“bug 数量”把正常等待写成死锁。

[SPECIFICATION]

**Pre-Condition**：已取得报告中已记录的实际 trace/step 与访存结果，或者能明确指出哪些证据缺失。

**Post-Condition**：每个现象有来源、修正理由和知识点；无虚构过程、无过度结论。

**Case 1：三级与根叶子。** 从 levels、i、ptshift 和 PTE 的 R/X 解释“三级能力”与“本次一次 PTE 读取”的区别，不额外编造下两级表项。

**Case 2：变量垃圾值。** 用 next/finish 前后解释赋值尚未完成，明确何时读取才有效；不能把未初始化值写成算法算错后的真实修复。

**Case 3：断点没有再次触发。** 结合两条 ld 完成、寄存器值、一次 refill 和 TCG 源码，解释命中快路径；不只用“没停住”作证。

**Case 4：双 GDB 等待。** 区分客体暂停与宿主进程暂停，说明先继续宿主的原因；未出现内核死锁不能写“修复死锁”。

**Case 5：版本与地址。** 若对比早期调试记录与最终运行结果，明确两种 PC；不把不同运行拼成同一现场，也不因重新链接误判访存目标变化。

**Case 6：证据缺失。** 删除或降级没有依据的个人经历描述，写清“不足以确定”；不能用常见事故补齐故事。

完成后检查相对链接和前后结论。此任务无需重新编译内核，除非发现必须重新采集的证据缺口；未重跑就不声称重跑。

<a id="task-8"></a>
## 任务 8：QEMU 调试六：问题定位与迭代记录

[PROMPT]

**任务**：完成 QEMU 调试任务六及报告的问题处理记录：在真实 `report/report.md` 的“调试任务六：问题定位与记录”和各功能的“实现迭代过程”中，写明问题情景、当时思路、关键提问意图、AI 采取的处理、验证与剩余限制。负责人 2413921 程子涵，目录为仓库根目录。

**原理与功能**：把可核验的问题—处理—证据链写清楚，说明 spec/prompt 如何约束代码和调试，而不是列工具名字或编造完整对话。必须分开历史实际记录、本次按标准格式整理的可复现提示词，以及没有记录的内容。

**操作要求**：必须修改真实报告文件，不能只回复建议。禁止虚构用户逐字提问、模型版本、A/B 个人经历、未发生的 bug、未执行的验证。若没有原始聊天记录，使用“提问意图/整理后的约束”，不要加引号声称逐字引用。

**输出要求**：为每个真实问题给出情景、分析、提问意图、实际措施、实际结果和结论；同时列出修订后独立提示词与任务映射。记录哪些是代码错误、环境问题或观察方法不足，不把正常限制包装成已修复缺陷。

[RELY]

只读材料：本会话可见指令、当前报告、`report/prompt.md`、最终源码和报告中已记录的实际验证结果。已清理的临时日志、环境记录和图片不属于最终复核材料；无法从现有材料核实的细节必须标为未知。

可确认的本次事实：用户指定 A/B 分支要求完成 C、指定截图并清理；随后要求按标准报告和 `[RELY]/[GUARANTEE]/[SPECIFICATION]` 风格逐任务重写。10 月 18 日已完成最终 Best-Fit/Buddy 的 SLUB 联调、双 GDB、Buddy 宿主 28/28 和原评分 20/20。首次评分被 CRLF shell 换行阻断，统一 LF 后通过。A/B 来源分支提交用于追踪输入代码；本次成员 C 的完整交付应提交至 `lab2-cheng`。

其他历史环境问题只能按现有记录明确标为历史材料，不把无法独立追踪的完整交互补造成逐字聊天。B 的 10 月 18 日原稿已有 A+B 内核联调，不能错误写成“B 始终只有宿主验证”；本次新增的是 C 的 SLUB 与最终双 GDB 集成。

[GUARANTEE]

没有需要新增/修改的 C 或 Python API；必须落盘报告中的 AI 处理小节、真实问题表、提示词链接与迭代说明。目标文档结构依照指定仓库的六章报告格式，具体实现内容不被模板占位符覆盖。

允许：报告上述小节、实验环境中的工具信息和必要的交叉引用；提示词清单可引用本任务文件。禁止：改测试预期、改已记录的实际结果、修改 A/B 原始提示词来冒充历史、填入未记录的精确模型型号、推送到 `lab2-cheng` 以外的分支。

系统约束：以实际观察区分“未执行”“失败”“通过”；历史调试观察与最终集成结果分开；不把检查已有日志叫重新实跑，不把来源分支提交号叫最终提交号。

[SPECIFICATION]

**Pre-Condition**：已有至少一条可核对的问题处理记录和验证材料，或能明确说明记录边界。

**Post-Condition**：报告可审查每项处理的事实依据，提示词是整理后的复现规范而不是虚构聊天实录。

**Case 1：环境权限/工具缺失。** 只根据真实工具输出或明确标注的历史记录写情景与处理；没有保存完整证据就不补写精确安装日志或个人操作。

**Case 2：早期骨架与最终集成。** 说明早期隔离 First-Fit 的验证范围；最终使用用户提供 A/B 已完成代码并在实际管理器重验。不能在最终状态继续写“等待 A/B 实现”。

**Case 3：观察方法修正。** 记录仅看翻译函数不足以证明 hit，因而加入三次 TLB 快照、两条指令和寄存器结果；Bare 对照采用真实数据 sd。每项给出现有日志位置。

**Case 4：真实 CRLF 问题。** 记录首次 shell 解析失败和 LF 规范化，最终 grade 为 20/20；不将其误写为 PMM 算法故障，不改评分规则。

**Case 5：提示词修订。** 逐份列出目标文件、真实接口、白名单/禁区、前后条件、明确 Case 与系统约束。说明这些文本按现有实施约束整理，不声称最初就是以相同文字提交给 AI。

**Case 6：没有实际算法 bug。** 明确没有观察到的越界/泄漏等仅为风险防护或测试项；不能编造“先有 bug 再修好”的迭代。

**Case 7：缺失型号/对话。** 模型精确版本未记录时直接说明未记录；没有用户原话就记提问意图。保留事实边界，不留 `[N]`、`[问题1]` 等模板占位。

完成后核对报告六章结构、每项任务的接口/提示词/迭代/证据、全部图片与文件链接；仅在替代文档落盘并核对后删除旧 C 提示词稿和临时参考副本，不删除仍被引用的证据或其他成员现行材料。
