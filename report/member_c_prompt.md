# 成员 C 综合任务提示词

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
