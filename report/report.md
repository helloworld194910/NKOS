# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
| --- | --- |
| **实验名称** | Lab5：用户进程管理 |
| **小组成员** | 2412790-杨茜茹、2411824-于蕾、2413921-程子涵 |
| **完成日期** | 全组完成最终验收后填写 |

### 小组分工

| 成员 | 负责的练习或模块 |
| --- | --- |
| 2412790-杨茜茹 | 迁移全队需要的 Lab2–4 前置代码；完成模块一用户态系统调用、模块四退出、让出与终止；负责最终代码整合和回归。 |
| 2411824-于蕾 | 完成模块二首个用户进程的创建与加载、U/S 态返回和 `do_wait()`；负责 ELF 嵌入与加载时机问答、报告定稿和调试记录整理。 |
| 2413921-程子涵 | 完成模块三用户进程复制和 COW Challenge；负责复制、引用和回收测试及对应报告。 |

各成员撰写自己的功能、提示词和验证记录，于蕾汇总总稿。双重 GDB 展示由全组共同完成。本文先记录杨茜茹在 `lab5_yang` 上的迁移和功能实现，当前源码证据对应 `6e9bc57`、`1d3fe34`。

---

## 一、实验目的

1. 理解用户态如何借助 `ecall` 切到内核，掌握陷入帧中的调用号、参数、返回值和返回地址。
2. 了解第一个用户程序从 ELF 数据到用户地址空间，再经 `sret` 开始执行的过程。
3. 理解 `fork`、`exec`、`exit`、`wait` 的衔接，并能解释父子进程、地址空间、调度与资源回收之间的关系。
4. 将前序实验的页表、物理页分配、Trap 和内核线程迁到 Lab5，检查这些基础接口能否支持用户进程管理。

---

## 二、实验环境

| 成员 | AI 编程工具 | 底层模型 | 备注 |
| --- | --- | --- | --- |
| 2412790-杨茜茹 | Codex 桌面应用 | 本次实验的精确版本待本人核对 | Windows 下使用 VS Code 和 WSL Ubuntu；在 WSL 以 `riscv64-unknown-elf-gcc` 编译。 |
| 2411824-于蕾 | 由于蕾填写 | 由于蕾填写 | 由于蕾填写 |
| 2413921-程子涵 | WorkBuddy 桌面应用 | DeepSeek-V4.1-Flash（精确版本以客户端版本号为准） | Windows 本机没有 RISC‑V 交叉工具链和 QEMU，改用 zig 0.13.0 的 `zig cc`（目标 `riscv64-freestanding`，`-mcmodel=medany -O2 -std=gnu99 -nostdinc -ffunction-sections -fdata-sections`，与 Makefile 同参数）做对象级编译检查。整机构建和 QEMU 运行在 A 的 WSL 环境复验。 |

本阶段在 WSL 的 `/home/yxqjrh/NKOS-lab5` 中执行对象编译和整机构建。宿主测试使用 GCC。QEMU 和交叉工具链的完整版本号在最终复验时随命令留存。

---

## 三、实验整体逻辑分析

### 3.1 本章节的逻辑主线

Lab4 的执行流主要在内核线程之间切换。Lab5 要先给一个进程准备用户地址空间和用户栈，再让它在 U 态执行；需要内核服务时，由 `ecall` 陷入 S 态。内核从 Trap 帧取得调用号和参数，选择对应服务，把结果写回帧，最后恢复现场。`fork` 让同一调用分出父子两条执行流，`exec` 换掉当前程序映像，`exit` 与 `wait` 分两步结束和回收进程。

### 3.2 功能的逐步实现

1. 我先迁移全队后续功能共用的物理内存、页表、启动、Trap、内核线程和调度实现，保留 Lab5 提供的进程树和用户返回相关接口。
2. 接通模块一的调用约定。用户库和内核分发在 `a0` 及 `a1`–`a5` 上对齐后，才能用同一入口验证 `getpid`、`fork`、`wait` 等服务。
3. 模块二负责装载和首次用户态返回，模块三负责用户地址空间复制和子进程登记。我的 `sys_exec()`、`sys_wait()`、`sys_fork()` 分别通过已有公开接口接到这些实现。
4. 我完成模块四的进程控制。当前进程先处理用户地址空间、留下退出码并切走，父进程之后才能回收 PCB 和内核栈。时钟和 `yield` 通过同一个调度标志让进程交出 CPU。

---

## 四、实验内容与实现

### 功能模块：Lab2–4 前置代码迁移

**负责人：** 2412790-杨茜茹

#### 模块功能描述

**需要实现/修改的函数：**

```c
// code/kern/mm/default_pmm.c
static void best_init(void);
static void best_insert_free_block(struct Page *base, size_t n);
static void best_init_memmap(struct Page *base, size_t n);
static struct Page *best_alloc_pages(size_t n);
static void best_free_pages(struct Page *base, size_t n);
static size_t best_nr_free_pages(void);
extern const struct pmm_manager default_pmm_manager;

// code/kern/mm/pmm.c
static void page_init(void);
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create);
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);
static void check_pgdir(void);
static void check_boot_pgdir(void);

// code/kern/trap/trap.c
void interrupt_handler(struct trapframe *tf);
void exception_handler(struct trapframe *tf);

// code/kern/process/proc.c
static struct proc_struct *alloc_proc(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
static int init_main(void *arg);
void proc_init(void);
void proc_run(struct proc_struct *proc);
void cpu_idle(void);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);

// code/kern/schedule/sched.c；switch_to 由 code/kern/process/switch.S 实现
void schedule(void);
void switch_to(struct context *from, struct context *to);
```

汇编还涉及 `code/kern/init/entry.S` 的 `kern_entry`、`boot_page_table_sv39`，以及 `code/kern/trap/trapentry.S` 的 `__alltraps`、`RESTORE_ALL`、`__trapret`。

**功能说明：**

我从组内 Lab4 `444c0c2` 取前序实现，按函数落到 Lab5 原始框架 `569be17`，迁移结果提交为 `6e9bc57`。此次把全队会用到的前置能力放进同一个基线，包括于蕾后续要用的线程初态和 Trap 恢复，以及程子涵做进程复制需要的页映射和引用计数。

- **物理内存与页表。** `page_init()` 读取 DTB 的 RAM 范围，初始化页描述符，并将描述符数组之后的完整空闲页登记给物理页管理器。沿用本组的 Best-Fit 策略，注册名仍是框架和评分脚本使用的 `default_pmm_manager`。映射创建和删除保留引用计数与 TLB 同步，`get_pte()` 用 `ptd_create()` 创建非叶项。原有页表检查按当前真实映射核对非叶项和启动大页。
- **启动和 Trap。** `entry.S` 开启分页时暂用恒等映射取指，跳到高地址后清除临时项。Trap 汇编继续用框架已有的 `SAVE_ALL` 保存现场；我迁入 Lab3 的调用 C 处理函数和寄存器恢复部分，给模块二的双向用户态返回留出同一帧布局。
- **内核线程和调度。** 把旧 PCB 初始化、内核线程初态、调度器和 `switch_to()` 接到 Lab5。临时的内核线程 `do_fork()` 使用 Lab5 已有的 `set_links()` 登记进程树，`copy_mm()` 此时仅支持旧的内核线程场景。`proc_run()` 在切换当前进程时更新 `satp` 并刷新 TLB。

#### 最终提示词

[前置迁移完整提示词](prompts.md#yang-prerequisites)。下方为同一份复现规格。

````markdown
[PROMPT]

**任务**　在 Lab5 课程框架 `569be17` 上迁移本组 Lab4 `444c0c2` 已继承的 Lab2–4 实现，给全组提供同一套物理内存、页表、启动、Trap、内核线程和调度基础。复用可用的函数，按 Lab5 的结构和启动约定适配。后续首个用户进程和用户地址空间复制都依赖这套基线。

**操作要求**　必须修改工程中的实际文件。按下表定位已有的 `lab2`、`lab3`、`lab4` 标记和 Lab5 过渡位置。迁移前核对同名函数的签名、数据结构变化及来源提交。保留 Lab5 页表辅助函数、进程树、用户 ELF 构建、评分入口和既有保存现场逻辑。

**输出要求**　列出来源提交、实际落点和 Lab5 适配点。对这 8 个对象执行编译，报告真实结果，并列出仍依赖 Lab5 新模块的入口。

[RELY]

| 当前工程文件 | 已有依赖与约定 |
| --- | --- |
| `code/kern/mm/default_pmm.c`、`code/kern/mm/pmm.h`、`code/kern/mm/memlayout.h` | `struct pmm_manager`、`struct Page`、`free_area_t`、`PageReserved`、`PageProperty`、空闲链表和原 `default_check()` 约定。框架使用 `default_pmm_manager`。 |
| `code/kern/mm/pmm.c`、`code/kern/mm/mmu.h`、`code/libs/riscv.h`、`code/kern/driver/dtb.h` | `get_memory_base()`、`get_memory_size()`、`nbase`、`pages`、`PADDR`、`KADDR`、页引用计数；Sv39 通过 `PDX1`、`PDX0`、`PTX` 逐级定位，非叶用 `ptd_create()`，叶子用 `pte_create()`。 |
| `code/kern/init/entry.S`、`code/tools/kernel.ld` | 物理装载地址 `0x80200000`；内核链接到高地址；`boot_page_table_sv39` 及高半区直接映射供 `page_init()` 使用。 |
| `code/kern/trap/trap.h`、`code/kern/trap/trapentry.S`、`code/kern/driver/clock.h` | `struct trapframe` 的寄存器和 CSR 槽位、已有 `SAVE_ALL`、`forkrets`、`trap()`、`clock_set_next_event()` 和 `ticks`。 |
| `code/kern/process/proc.h`、`code/kern/process/proc.c`、`code/kern/schedule/sched.c` | `struct context`、`struct proc_struct`、`boot_pgdir_pa`、`set_links()`、`hash_proc()`、`wakeup_proc()`、`proc_list`、`local_intr_save/restore`；`set_links()` 已负责进程链及计数。 |

[GUARANTEE]

| 允许修改的文件 | 必须提供或适配的入口 |
| --- | --- |
| `code/kern/mm/default_pmm.c` | `static void best_init(void)`、`static void best_init_memmap(struct Page *base, size_t n)`、`static struct Page *best_alloc_pages(size_t n)`、`static void best_free_pages(struct Page *base, size_t n)`、`static size_t best_nr_free_pages(void)`、内部 `best_insert_free_block(struct Page *base, size_t n)`，注册 `const struct pmm_manager default_pmm_manager`。 |
| `code/kern/mm/pmm.c` | `static void page_init(void)`、`pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)`、`int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)`、`void page_remove(pde_t *pgdir, uintptr_t la)`；按实际非叶权限和启动大页适配 `check_pgdir()`、`check_boot_pgdir()`。 |
| `code/kern/init/entry.S` | 适配 `kern_entry` 和 `boot_page_table_sv39`，接好临时恒等映射、分页开启及高地址跳转。 |
| `code/kern/trap/trap.c` | `void interrupt_handler(struct trapframe *tf)`、`void exception_handler(struct trapframe *tf)` 的 Lab3 基础，内部指令长度辅助函数。 |
| `code/kern/trap/trapentry.S` | `__alltraps`、`__trapret` 和 `RESTORE_ALL` 的 Lab3 恢复主体，匹配已有 Trap 布局。 |
| `code/kern/process/proc.c` | `alloc_proc()`、`kernel_thread()`、`copy_mm()`、`copy_thread()`、`init_main()`、`proc_init()`、`proc_run()`、`cpu_idle()`、`do_fork()` 的旧内核线程路径。 |
| `code/kern/process/switch.S`、`code/kern/schedule/sched.c` | `void switch_to(struct context *from, struct context *to)` 和 `void schedule(void)`。 |

只在上表范围迁入所需实现。`check_pgdir()`、`check_boot_pgdir()` 的改动限于准确验证当前 Sv39 表项和启动映射。保留 `default_pmm_manager` 对外名称、原 `default_check()`、Lab5 原构建与评分、`SAVE_ALL`、`forkrets` 和 `wakeup_proc()`。保留课程标记。首次用户进程、U/S 双向恢复、`do_wait()` 和用户地址空间复制留给对应 Lab5 模块。

[SPECIFICATION]

### `best_init()`、`best_init_memmap()`、`best_insert_free_block()`、`best_alloc_pages()`、`best_free_pages()`、`best_nr_free_pages()`

**Pre-Condition**　页描述符和标志按当前框架定义；登记区间来自保留页，划拨或释放请求以页数为单位。

**Post-Condition**　空闲链按物理地址递增，块头保存长度和 `PageProperty`，`nr_free` 等于空闲页总数。注册对象调用迁入的函数和原检查函数。

- **Case 1 · 初始化与登记**　`best_init()` 建立空链并归零；`best_init_memmap()` 校验 `n > 0` 和保留标志，清理页状态后登记整个区间。
- **Case 2 · 选择与拆分**　`best_alloc_pages()` 选长度最小且足够的块；相同长度取先遇到的低地址块。摘除旧头，余块就地登记，空闲数只扣实际分配页数。
- **Case 3 · 空闲不足**　找不到合适的连续块时返回 `NULL`，链表和计数保持原状。`n == 0` 由既有断言拒绝。
- **Case 4 · 释放与合并**　`best_free_pages()` 清理页状态，辅助函数按地址插链；前插与尾插均只执行一次，分别检查前后物理邻接块并合并。`best_nr_free_pages()` 返回计数。

**Requirements**　管理器注册名与评分脚本一致，空闲数只随登记、分配和释放变化。拆分余块与合并后链表中每块只保留一个有效块头。

### `page_init()` 与启动入口

**Pre-Condition**　DTB 已提供 RAM 范围；启动页表可覆盖高地址内核及即将管理的物理内存。

**Post-Condition**　高地址入口可继续取指；`pages`、`npage`、`va_pa_offset` 与物理布局一致，空闲范围在页描述符数组之后。

- **Case 1 · 启动切换**　写入 `satp` 后先借临时恒等映射保持取指，再跳到高地址，从根表删除临时项并执行 `sfence.vma`。
- **Case 2 · DTB 和描述符**　验证 RAM 起点、长度、加法溢出与内核覆盖范围。裁剪并按页对齐可管理末端。初始化 `npage - nbase` 个描述符为保留页，只将描述符数组末端向上对齐后的完整空闲页登记给 PMM。
- **Case 3 · 参数无效或资源不足**　无效 DTB、描述符放不下等条件给出明确诊断；有效内存范围内没有剩余完整空闲页时跳过空闲区登记。

**Requirements**　管理上界、描述符区间和启动映射要彼此一致。保留原启动入口及 DTB 参数传递。

### `get_pte()`、`page_insert()`、`page_remove()` 与页表检查

**Pre-Condition**　传入有效的页表根，`get_pte()` 的用户页映射路径按 4 KiB 页表层次访问。

**Post-Condition**　叶项映射、引用计数和 TLB 一致；失败时仅回滚本次新建的页表资源。

- **Case 1 · 查询与创建**　`create=0` 遇到缺表返回 `NULL`；`create=1` 分配并清零所需中间页表，两个非叶项用 `ptd_create()` 创建。
- **Case 2 · 插入或替换**　先增加新页引用；同页重插抵消多加的引用并更新权限。替换旧页时撤销旧映射，提交新叶项后同步 TLB。
- **Case 3 · 分配失败**　返回 `-E_NO_MEM`。本次新建第一张表、第二张表分配失败时恢复原根项并释放新表；已有表仍归原地址空间。
- **Case 4 · 删除**　缺表、空叶项和重复删除直接返回。有效项先清除和同步 TLB，再减少页引用，归零才释放物理页。空的中间表由地址空间清理路径管理。
- **Case 5 · 自检**　`check_pgdir()` 核对用户权限在叶项，两级非叶均为有效表指针；`check_boot_pgdir()` 按根级 1 GiB 叶项核对内核直接映射的地址关系。

**Requirements**　保留自检对映射替换、引用和回收的检查。检查启动大页时直接使用对应根叶项。

### `interrupt_handler()`、`exception_handler()`、`__alltraps`、`__trapret` 与 `RESTORE_ALL`

**Pre-Condition**　Lab5 已有 `SAVE_ALL` 及 `trapframe` 排布可用；此任务验证旧内核线程需求。

**Post-Condition**　内核 Trap 能把帧交给 C 并按原布局恢复，Lab3 的内核时钟与异常处理作为 Lab5 拓展的起点。

- **Case 1 · 时钟/软件中断**　时钟重装并计数，软件中断清对应挂起位。中断保留原 `epc`，Lab5 不沿用固定次数关机策略。
- **Case 2 · 内核同步异常**　断点、非法指令根据编码确定 2 或 4 字节后推进 `epc`，无法判定长度时保留诊断。用户异常留给模块一接入。
- **Case 3 · 调用与恢复**　`__alltraps` 传入帧指针并调用 `trap()`，然后经 `__trapret` 恢复 `sstatus`、`sepc`、通用寄存器及栈指针，执行 `sret`。

**Requirements**　恢复顺序遵循 `SAVE_ALL` 与 `trapframe` 槽位；U/S 双向栈恢复由模块二在基线上完成。

### `alloc_proc()`、`kernel_thread()`、`copy_mm()`、`copy_thread()`、`init_main()`、`proc_init()`、`proc_run()`、`cpu_idle()`、`do_fork()`、`schedule()`、`switch_to()`

**Pre-Condition**　PMM、内核堆和内核页表可用，Lab5 已提供 PID、哈希、亲属链和唤醒接口。

**Post-Condition**　具备 Lab4 内核线程创建和切换的基础，并为后续 Lab5 模块保留原有接口。

- **Case 1 · 启动和现场**　`alloc_proc()` 初始化 PCB、链节点和新加字段。`proc_init()` 建立 idle 及 init 的旧路径。`kernel_thread()` 构造 S 态起始帧；`copy_thread()` 保存帧，设置子线程 `a0=0` 及首次进入 `forkret` 的上下文。`init_main()` 仅提供迁移阶段的内核线程入口。
- **Case 2 · 创建登记**　`do_fork()` 依次分配 PCB、内核栈、准备地址空间及现场，再在临界区分配 PID、挂哈希、调用 `set_links()` 并唤醒。`copy_mm()` 仅处理内核线程，遇到已有用户 `mm` 返回 `-E_INVAL`，交给模块三提供真正复制。
- **Case 3 · 创建失败**　达到进程数限制或资源不足返回相应错误，回收已申请的栈和 PCB，不残留半注册的进程。
- **Case 4 · 切换与空闲**　`cpu_idle()` 按 `need_resched` 调 `schedule()`，后者选就绪进程，无其他候选时选 idle。`proc_run()` 关中断更新 `current`、切 `satp` 并同步 TLB，`switch_to()` 保存和恢复 `ra`、`sp` 与 `s0`–`s11`。

**Requirements**　启动页表根使用物理地址。`set_links()` 已含进程数递增，调用处只登记一次。阶段验收使用 `make -C code` 指定这 8 个对象，后续待 Lab5 模块接齐后验证启动和用户进程。
````

#### 实现迭代过程

##### 第一次迭代

**问题解决策略：** 我先核对哪些内容能直接沿用，哪些要求按 Lab5 适配。页表检查要同时认识 4 KiB 用户页的非叶项和启动时的根级大页。进程树使用框架的 `set_links()`，这样登记子链时进程数只增加一次。首个用户进程的装载和用户 `fork` 留到对应模块接入再验证。

**最终结果：** 8 个迁移对象编译通过，提交 `6e9bc57` 已推到 `lab5_yang`。这次验证记录的是迁移对象的编译结果。

### 功能模块一：用户态系统调用

**负责人：** 2412790-杨茜茹

#### 模块功能描述

**需要实现/修改的函数：**

```c
// code/kern/trap/trap.c
void exception_handler(struct trapframe *tf);

// code/kern/syscall/syscall.c
void syscall(void);
static int sys_exit(uint64_t arg[]);
static int sys_fork(uint64_t arg[]);
static int sys_wait(uint64_t arg[]);
static int sys_exec(uint64_t arg[]);
static int sys_yield(uint64_t arg[]);
static int sys_kill(uint64_t arg[]);
static int sys_getpid(uint64_t arg[]);
static int sys_putc(uint64_t arg[]);
static int sys_pgdir(uint64_t arg[]);

// code/kern/mm/pmm.c
void print_pgdir(void);

// code/user/libs/syscall.c 和 syscall.h，另有 ulib.c/.h 的 exec()
static inline int syscall(uintptr_t num, uintptr_t arg0, uintptr_t arg1,
                          uintptr_t arg2, uintptr_t arg3, uintptr_t arg4);
int sys_exit(int64_t error_code);
int sys_fork(void);
int sys_wait(int64_t pid, int *store);
int sys_exec(const char *name, size_t len, const unsigned char *binary, size_t size);
int sys_yield(void);
int sys_kill(int64_t pid);
int sys_getpid(void);
int sys_putc(int64_t c);
int sys_pgdir(void);
int exec(const char *name, size_t len, const unsigned char *binary, size_t size);
```

**功能说明：**

我先从用户库和 `unistd.h` 核对调用约定。这个 Lab5 框架在 `a0` 放调用号，参数放 `a1`–`a5`，返回值仍放 `a0`。`exception_handler()` 确认是 U 态 `ecall` 后，先推进 `epc` 4 字节再分发。这样 `fork` 复制陷入帧时，子进程也从 `ecall` 的下一条指令开始；创建路径在子帧写入 0，父帧收到分发返回的子进程 PID。用户态其他同步异常在当前基础实现中记录现场并以 `-E_KILLED` 退出。

| 请求 | 我的处理 |
| --- | --- |
| `sys_exit`、`sys_yield`、`sys_kill` | 对接本人实现的三个 `do_*` 函数；`kill` 先检查 PID 的机器字位宽。 |
| `sys_fork` | 用当前陷入帧和保存的用户栈指针调用 `do_fork(0, ...)`。 |
| `sys_wait` | 退出码指针允许为空，非空时检查用户区间可写性，然后转发 `do_wait()`。 |
| `sys_exec` | 分别检查名称和 ELF 所在区间可读，再转发到 `do_execve()`。装载时要先处理这两份旧地址空间里的输入。 |
| `sys_getpid`、`sys_putc`、`sys_pgdir` | 返回当前 PID、输出低 8 位字符，或查询当前用户 VMA 的有效页并打印 `rwx` 权限。 |

`syscall()` 用完整机器字宽度检查编号，越界和空槽一律返回 `-E_INVAL`。用户库用固定参数和寄存器约束传值，内核将有符号返回值写回整个 `a0`。`print_pgdir()` 以 `create=0` 查表，查询过程不分配页表。

#### 最终提示词

[用户态系统调用完整提示词](prompts.md#yang-syscall)。下方为同一份复现规格。

````markdown
[PROMPT]

**任务**　在 Lab5 用户态 `ecall` 入口接通统一分发，让用户库、Trap 和内核服务使用同一套 RV64 约定。以 `6e9bc57` 的 Lab5 基线为起点，在 `code/kern/trap/trap.c` 的 `lab5 function1` 处更新 `exception_handler()`，在 `code/kern/syscall/syscall.c` 的同一任务标记内实现分发及 9 个 `sys_*` 处理函数。同步修改用户库封装，并接上 `print_pgdir()`。

**操作要求**　必须修改工程中的实际文件。使用当前仓库的编号、函数签名和陷入帧；保留 Lab5 用户态原有公开接口的签名。保留 `lab5` 任务标记，在本人完成的标记内将 `YOUR CODE` 换为 `2412790`。按功能给实现和依赖找到真正的调用点。

**输出要求**　列出实际修改的文件、实现的接口、`ecall` 到返回的寄存器约定和用于验证的命令。列明哪个结果来自宿主测试、对象编译或整机运行。链接缺少队友符号时保留原错误。

[RELY]

| 真实文件 | 当前约定 |
| --- | --- |
| `code/libs/unistd.h` | `SYS_exit=1`、`SYS_fork=2`、`SYS_wait=3`、`SYS_exec=4`、`SYS_yield=10`、`SYS_kill=12`、`SYS_getpid=18`、`SYS_putc=30`、`SYS_pgdir=31`。编号表存在空槽。 |
| `code/kern/trap/trap.h`、`code/kern/trap/trap.c`、`code/libs/riscv.h` | `struct trapframe` 提供 `gpr.a0`–`gpr.a5`、`gpr.sp`、`epc`、`cause` 和 `status`。`trap()` 在分发期间设置 `current->tf`，`trap_in_kernel()` 用 `SSTATUS_SPP` 区分来源。`CAUSE_USER_ECALL` 表示 U 态系统调用。 |
| `code/kern/process/proc.h` | `int do_exit(int error_code)`、`int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)`、`int do_wait(int pid, int *code_store)`、`int do_execve(const char *name, size_t len, unsigned char *binary, size_t size)`、`int do_yield(void)`、`int do_kill(int pid)`。`current->pid` 与 `current->mm` 属于当前进程。 |
| `code/kern/mm/vmm.h`、`code/kern/mm/vmm.c` | `bool user_mem_check(struct mm_struct *mm, uintptr_t start, size_t len, bool write)` 检查地址区间及 VMA 权限。用户态入口需要有当前用户 `mm`。`mm->mmap_list` 按起始地址组织 VMA，`mm->pgdir` 提供页表根。 |
| `code/kern/mm/pmm.h`、`code/libs/stdio.h` | `get_pte(pgdir, va, 0)` 查询现有 4 KiB 叶项，`void print_pgdir(void)` 是已有声明，`void cputchar(int c)` 输出单字符。 |
| `code/user/libs/syscall.c`、`code/user/libs/syscall.h`、`code/user/libs/ulib.c`、`code/user/libs/ulib.h` | 本包 ABI 在 `a0` 传调用号，`a1`–`a5` 传参数，返回值回到 `a0`。用户库原有 `exit/fork/wait/waitpid/yield/kill/getpid` 等入口继续可用。 |

[GUARANTEE]

- `code/kern/trap/trap.c` 提供 `void exception_handler(struct trapframe *tf)` 的用户分支，内核同步异常继续用迁入的处理逻辑。时钟调度另见任务三。
- `code/kern/syscall/syscall.c` 提供 `void syscall(void)`，以及签名为 `static int sys_名称(uint64_t arg[])` 的 `sys_exit`、`sys_fork`、`sys_wait`、`sys_exec`、`sys_yield`、`sys_kill`、`sys_getpid`、`sys_putc`、`sys_pgdir`。分发表指向这 9 个实现，handler 放在初始化分发表之前。
- `code/user/libs/syscall.c` 用 `static inline int syscall(uintptr_t num, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4)` 统一发起 `ecall`，调整已有封装传参。新增 `int sys_exec(const char *name, size_t len, const unsigned char *binary, size_t size)`，在用户头文件中声明；在 `ulib.c/.h` 新增同参数的 `int exec(...)`。
- `code/kern/mm/pmm.c` 实现 `void print_pgdir(void)`，只展示当前用户 VMA 内存在的用户页映射及其读写执行权限。

允许修改以上文件内与本任务直接相关的代码、声明及必要的头文件包含。保留 `unistd.h` 编号、`trapframe` 布局、汇编返回约定、已有用户接口签名和原评分测试。`do_wait()`、`do_execve()`、用户 `do_fork()` 的底层实现由各模块负责。Lab5 用户 `exec` 传入旧地址空间中的名称和 ELF，装载层需要在销毁旧地址空间前准备好这两个输入。

[SPECIFICATION]

### `exception_handler()`

**Pre-Condition**　`tf` 指向当前陷入帧；进程上下文中的 `trap()` 已把它挂到 `current->tf`。

**Post-Condition**　从 U 态发起的 `ecall` 进入分发，返回位置指向下一条指令。同步异常和系统调用按原因码走各自路径。

- **Case 1 · U 态 `ecall`**　先执行 `tf->epc += 4`，再调用 `syscall()`。`fork` 随后复制的子帧也具有已推进的 `epc`。
- **Case 2 · U 态其他同步异常**　基础版打印陷入帧并以 `-E_KILLED` 退出。写时复制联调时由相应模块提供可恢复缺页的处理入口。
- **Case 3 · S 态同步异常**　沿基线处理，内核断点和非法指令仍由编码决定步长。

**Requirements**　`ecall` 为 4 字节；这个固定步长只应用于 `ecall`。异步中断的返回地址保持原位。若 `exec` 成功更换程序，装载层之后写入的新入口由返回流程使用。

### 内核 `syscall()` 与用户库 `syscall()`

**Pre-Condition**　内核分发持有有效陷入帧；用户发起调用时按 RV64 的寄存器宽度封装参数。

**Post-Condition**　合法请求只调用一次目标 handler。其 `int` 结果经带符号转换写回完整宽度的 `a0`，用户库从该寄存器取回结果。

- **Case 1 · 合法调用**　用完整 `uintptr_t` 宽度检查 `a0`，将 `a1`–`a5` 按顺序组成参数数组，再调用表项。
- **Case 2 · 无效调用**　编号越界或表内为空时，`a0` 写回 `-E_INVAL`。高位非零的编号先按完整宽度检查。
- **Case 3 · 用户态封装**　以寄存器约束把编号和参数绑定到 `a0`–`a5`，使用 `a0` 作为读写操作数，未用参数明确填 0。返回从 `a0` 转为 `int`。

**Requirements**　保留 `ecall` 的 `memory` clobber，满足指针实参和负值结果的位宽要求。参数传递采用明确形参与寄存器约束，避免从可变参数列表读取不存在的参数，也避免把 8 字节寄存器值写进 4 字节局部变量。

### 9 个 `sys_*` 处理函数和 `print_pgdir()`

**Pre-Condition**　处理函数接收完整的 5 个机器字。涉及用户地址时，先按访问方向校验区间。

**Post-Condition**　正确转发请求，拒绝无效参数，保持下表约定的结果。

| Case | 条件 | 预期行为 |
| --- | --- | --- |
| 1 · `sys_exit` | `a1` 为退出码 | 以 `int` 类型传给 `do_exit()`。成功退出后进入调度。 |
| 2 · `sys_fork` | 普通 `fork` | 调 `do_fork(0, tf->gpr.sp, tf)`，父进程收到创建结果。子帧返回 0 由创建路径设置，联调用例检查同一 `ecall` 后的两个返回值。 |
| 3 · `sys_wait` | PID 能无损转换为 `int`，输出地址可选 | `NULL` 直接转发；非空时要求有用户 `mm`，检查 `sizeof(int)` 字节的可写性，非法 PID 位宽或地址返回 `-E_INVAL`。真正等待和退出码写回由 `do_wait()` 处理。 |
| 4 · `sys_exec` | 依次传名称、名称长度、ELF 地址和大小 | 使用用户 `mm` 分别检查两个区间的可读性。无 `mm` 或校验失败返回 `-E_INVAL`，有效时按四个参数的原顺序转给 `do_execve()`。 |
| 5 · `sys_yield` | 函数无业务参数 | 调 `do_yield()`，按返回路径落实重调度。 |
| 6 · `sys_kill` | PID 从机器字转成 `int` | 对高位与该 `int` 符号扩展不一致的输入返回 `-E_INVAL`；其余由 `do_kill()` 判断目标。 |
| 7 · `sys_getpid` | 存在当前进程 | 返回 `current->pid`。 |
| 8 · `sys_putc` | `a1` 为字符值 | 取低 8 位，调用 `cputchar()` 后返回 0。 |
| 9 · `sys_pgdir` | 当前用户页表可查询 | `print_pgdir()` 按用户 VMA 遍历，调用 `get_pte(mm->pgdir, va, 0)`，只打印 `PTE_V` 和 `PTE_U` 都有效的页及 `rwx`。无用户 `mm` 或缺页表时直接返回。`sys_pgdir()` 返回 0。 |

**Requirements**　等待读取退出码的最终写回需要由 `do_wait()` 在正确时机处理。`exec` 的区间可读性检查与 ELF 内容解析属于两个阶段。`print_pgdir()` 查询不得分配页表，也不得访问内核大页分支。用 `tools/test_lab5.c` 中的 `test_lab5_syscall()` 检查 `epc` 从 `0x1000` 变为 `0x1004`、`getpid` 及编号 0；再在 QEMU 中用 `hello`、`badarg`、`pgdir`、`forktest` 等现有程序验证完整的用户态调用路径。
````

#### 实现迭代过程

##### 第一次迭代

**遇到的问题：** 对照框架时发现，原用户库的可变参数封装每次都读取五个参数，调用方有时并未传足。同时原汇编用 `sd` 把 `a0` 写入 `int ret`，寄存器和目标对象的宽度不一致。这两点来自源码核对。

**问题解决策略：** 在规格中明确每个参数槽的位置和宽度。用固定形参，未用的槽传 0，直接以 `a0` 读写操作数取回结果。分发表也按完整宽度检查编号，再检查表项是否为空。

##### 第二次迭代

**遇到的问题：** 在 WSL 完整构建进入 `bin/kernel` 链接时，`sys_exec()`、`sys_wait()` 分别报 `undefined reference to do_execve` 和 `undefined reference to do_wait`。

**最终结果：** 本人提交 `1d3fe34` 的相关对象已重新编译。宿主测试通过了 `ecall` 步长、编号边界和转发检查。上述两个符号由模块二提供，真实用户态 `exec`、`wait` 与整机运行留到联调。

### 功能模块二：第一个用户进程的创建与加载

**负责人：** 2411824-于蕾

由于蕾按本章统一格式补充首个用户进程、ELF 装载、U/S 返回、`do_wait()` 和加载时机问答，并填写其实际验证和迭代。

### 功能模块三：复制用户进程

**负责人：** 2413921-程子涵

#### 模块功能描述

**需要实现/修改的函数：**

```c
// code/kern/process/proc.c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
static void put_mm(struct proc_struct *proc);          // 失败回滚辅助

// code/kern/mm/vmm.c
int dup_mmap(struct mm_struct *to, struct mm_struct *from);

// code/kern/mm/pmm.c
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share);
```

**功能说明：**

- **创建与登记。** `do_fork()` 依次分配 PCB、内核栈，由 `copy_mm()` 决定地址空间，`copy_thread()` 复制中断帧并设子进程 `a0=0`，最后在同一个关中断临界区内取 PID、`hash_proc()`、`set_links()` 登记并 `wakeup_proc()` 唤醒。登记压在临界区里，调度器不会看到"已入队但还没 PID"的半成品。
- **三条地址空间路径。** `copy_mm()` 按 `current->mm` 和 `clone_flags` 分流：内核线程没有用户地址空间，`mm` 置空并把 `pgdir` 设成 `boot_pgdir_pa`；`CLONE_VM` 共享父进程 `mm`，只递增 `mm_count`；普通 `fork` 走 `mm_create()` → `setup_pgdir()` → `dup_mmap()`，由子进程独占一份新地址空间。
- **区间与页的两级复制。** `dup_mmap()` 按 `from->mmap_list` 的顺序逐个复制 vma，用 `insert_vma_struct()` 挂进子进程，再对每个区间调一次 `copy_range()`。先区间后页，子进程的 vma 链始终和已建立的映射一致，不会出现"有页无区间"的中间态。
- **页级映射。** `copy_range()` 沿 `[start, end)` 逐页 `get_pte(from, va, 0)`：二级页表不存在时整段 4 MiB 跳到下一个 `PTSIZE` 边界；叶项有效就从源页表项截出 `U/R/W/X/A/D` 权限位，深拷贝路径 `alloc_page()` + `memcpy()` 后 `page_insert()` 建映射。
- **失败回收。** 复制路径上任何一步失败都按逆序回滚。`dup_mmap()` 失败时调用方执行 `exit_mmap()` → `put_pgdir()` → `mm_destroy()` 拆掉刚建的一半地址空间；`copy_range()` 内部则先 `free_page()` 再返回。`do_fork()` 新增的 `put_mm()` 专门处理"地址空间已复制但 PID 登记失败"这个边界，按 `mm_count` 决定整份释放还是只减引用。

#### 最终提示词

[复制用户进程完整提示词](prompts.md#cheng-fork)。下方为同一份复现规格。

````markdown
[PROMPT]

**任务**　在 `code/kern/process/proc.c` 的 `lab5 function3` 标记中实现 `do_fork()` 和 `copy_mm()`，在 `code/kern/mm/vmm.c` 实现 `dup_mmap()`，在 `code/kern/mm/pmm.c` 实现 `copy_range()`。让用户进程经 `fork` 得到一个地址空间独立、返回值不同、并已登记到进程树和调度队列的子进程。本任务先交付逐页深拷贝的可回退基础版，写时复制留给任务五。

**操作要求**　必须修改工程中的实际文件。保留 `lab5` 标记，把本人完成处的 `YOUR CODE` 改为 `2413921:`。`copy_range()` 的 `share` 参数必须保留：`0` 走深拷贝，`1` 留给任务五的共享路径，这样基础版能独立验收，也能一行退回。`copy_mm()` 用 `clone_flags` 区分普通 `fork` 与 `CLONE_VM`。失败路径按逆序回收，不能残留半登记的 PCB 或半复制的地址空间。

**输出要求**　说明三条地址空间路径各自的 `mm` 与 `pgdir` 归属、vma 与页的两级复制顺序、失败回滚的每一级，以及 `mm_count` 在共享和独占两种情形下的配对关系。

[RELY]

| 真实文件 | 当前约定 |
| --- | --- |
| `code/kern/process/proc.c`、`code/kern/process/proc.h` | `alloc_proc()`、`setup_kstack()`、`copy_thread()`、`get_pid()`、`hash_proc()`、`set_links()`、`wakeup_proc()`；`struct proc_struct` 的 `mm`、`pgdir`、`parent` 和 `cptr`/`yptr`/`optr` 亲属链；`current`、`nr_process`、`MAX_PROCESS`、`KSTACKSIZE`。 |
| `code/kern/mm/vmm.c`、`code/kern/mm/vmm.h` | `mm_create()`、`setup_pgdir()`、`vma_create()`、`insert_vma_struct()`、`find_vma()`、`lock_mm()`/`unlock_mm()`、`mm_count_inc()`/`mm_count_dec()`、`exit_mmap()`、`mm_destroy()`；`struct mm_struct` 的 `mmap_list`、`map_count`。 |
| `code/kern/mm/pmm.c`、`code/kern/mm/mmu.h`、`code/libs/riscv.h` | `get_pte()`、`page_insert()`、`alloc_page()`、`free_page()`、`page_ref()`、`page2kva()`、`pte2page()`；`PTE_V/R/W/X/U/A/D`、`PADDR()`、`ROUNDDOWN()`、`PTSIZE`、`USER_ACCESS()`；`boot_pgdir_pa`。 |

[GUARANTEE]

| 允许修改的文件 | 必须提供的入口 |
| --- | --- |
| `code/kern/process/proc.c` | `int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)`；`static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)`；内部 `static void put_mm(struct proc_struct *proc)`。 |
| `code/kern/mm/vmm.c`、`code/kern/mm/vmm.h` | `int dup_mmap(struct mm_struct *to, struct mm_struct *from)`。 |
| `code/kern/mm/pmm.c` | `int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share)`。 |

必须保留 `setup_kstack()`、`put_kstack()`、`copy_thread()`、`set_links()`、`hash_proc()`、`wakeup_proc()`、`mm_create()`、`setup_pgdir()`、`exit_mmap()`、`mm_destroy()`、`get_pte()`、`page_insert()` 的既有语义与签名。`do_fork()` 的 PID 登记必须与 `set_links()` 处于同一临界区；`set_links()` 会自行递增进程数，调用处不得重复计数。内核线程路径不得改动 `boot_pgdir_pa`。

[SPECIFICATION]

### `do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)`

**Pre-Condition**　`current` 是发起 `fork` 的进程；`tf` 是本次系统调用的陷入帧，内核线程路径为构造帧。

**Post-Condition**　成功返回子进程 pid；子进程处于 `PROC_RUNNABLE`，已挂入 `proc_list` 与 pid 哈希表，其 `tf` 中 `a0` 为 0，`mm`/`pgdir` 由 `copy_mm()` 决定。

- **Case 1 · 进程数已满**　`nr_process >= MAX_PROCESS` 时直接返回 `-E_NO_FREE_PROC`，不分配任何资源。
- **Case 2 · PCB 或内核栈分配失败**　`alloc_proc()` 失败返回 `-E_NO_MEM`；`setup_kstack()` 失败回收 PCB。两者都不留下已登记的进程。
- **Case 3 · 地址空间准备失败**　`copy_mm()` 返回负值后回收内核栈与 PCB。此时还没有 PID，不需要回滚进程树。
- **Case 4 · 登记阶段失败**　地址空间已复制但临界区内发现进程数已满，走 `put_mm()` 按 `mm_count` 决定释放还是只减引用，再回收内核栈和 PCB。

**Requirements**　`proc->parent` 在 `copy_mm()` 之前设置，`copy_thread()` 在地址空间准备好之后调用。子进程 `a0=0` 由 `copy_thread()` 负责，`do_fork()` 不得覆盖。PID 分配、`hash_proc()`、`set_links()`、`wakeup_proc()` 必须在同一个 `local_intr_save`/`local_intr_restore` 区间内完成。

### `copy_mm(uint32_t clone_flags, struct proc_struct *proc)`

**Pre-Condition**　`proc` 的内核栈已就绪，`mm`/`pgdir` 尚未确定。

**Post-Condition**　`proc->mm` 与 `proc->pgdir` 指向子进程要使用的地址空间：内核线程为空 `mm` 加 `boot_pgdir_pa`；`CLONE_VM` 为父进程 `mm` 且引用加一；普通 `fork` 为独占的新 `mm`，引用计数为 1。

- **Case 1 · 内核线程**　`current->mm == NULL`，`proc->mm = NULL`、`proc->pgdir = boot_pgdir_pa` 后返回 0。
- **Case 2 · `CLONE_VM`**　`mm_count_inc(oldmm)`，`proc->mm = oldmm`，`proc->pgdir = PADDR(oldmm->pgdir)`。退出时由 `mm_count` 决定谁真正释放。
- **Case 3 · 普通 `fork`**　`mm_create()` 失败返回 `-E_NO_MEM`；`setup_pgdir()` 失败先 `mm_destroy()`。成功后在 `lock_mm(oldmm)` 保护下 `dup_mmap()`。
- **Case 4 · 复制失败**　`dup_mmap()` 返回非 0 时按 `exit_mmap()` → `put_pgdir()` → `mm_destroy()` 逆序拆除，不把半成品挂到 `proc`。

**Requirements**　新建 `mm` 由子进程独占，初值 1 才能与 `do_exit()` 的 `mm_count_dec()` 配对。`proc->pgdir` 一律存物理地址（`PADDR()`），因为 `proc_run()` 直接写 `satp`。`dup_mmap()` 必须在 `lock_mm()` 保护下执行，父进程的 vma 链和页表在此期间不能变。

### `dup_mmap(struct mm_struct *to, struct mm_struct *from)`

**Pre-Condition**　`to` 已 `setup_pgdir()` 且 `mmap_list` 为空；`from` 是父进程的 `mm`，调用方已持锁。

**Post-Condition**　`to` 的 vma 链按地址升序与 `from` 一一对应，每个区间的页映射都已按 `share` 语义建立。

- **Case 1 · vma 复制**　按 `from->mmap_list` 顺序取 vma，`vma_create()` 后用 `insert_vma_struct()` 挂入 `to`。分配失败返回 `-E_NO_MEM`，已挂的 vma 由调用方的 `exit_mmap()` 统一回收。
- **Case 2 · 页复制**　对每个区间调一次 `copy_range(to->pgdir, from->pgdir, vma->vm_start, vma->vm_end, share)`，一次遍历完成整段，而不是逐页再进一次外层循环。
- **Case 3 · 空区间**　`vma->vm_start == vma->vm_end` 时 `copy_range()` 的循环不产生改动，vma 本身仍被复制。

**Requirements**　复制顺序必须是先 vma 后页，否则子进程会出现"页已建立但没有 vma"的中间态。`share` 由调用点给定，`dup_mmap()` 自身不判断要不要 COW。

### `copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share)`

**Pre-Condition**　`start`、`end` 按页对齐且在用户地址空间内；`from` 是源页表根，`to` 是已建好内核高半区映射的目标页表根。

**Post-Condition**　`[start, end)` 内源页表有效的叶项在 `to` 中都有对应映射；`share=0` 时目标持有独立物理页，`share=1` 时父子指向同一页。

- **Case 1 · 缺二级页表**　`get_pte(from, start, 0)` 返回 `NULL` 表示这 4 MiB 全无映射，`start` 跳到下一个 `PTSIZE` 边界。
- **Case 2 · 叶项无效**　`*ptep` 无 `PTE_V` 时交给 `start += PGSIZE` 继续，不建映射。
- **Case 3 · 深拷贝（`share=0`）**　`perm` 从源页表项截出 `U/R/W/X/A/D`，`alloc_page()` 后 `memcpy()`，再 `page_insert(to, npage, start, perm)`。`page_insert()` 失败时 `free_page(npage)` 回滚本页。
- **Case 4 · 共享（`share=1`）**　原本可写的页需要父子同时降权，见任务五。
- **Case 5 · 分配失败**　返回 `-E_NO_MEM`，已建立的页由调用方的 `exit_mmap()` 回收。

**Requirements**　权限位只取硬件可见的 `U/R/W/X/A/D`，页表项里的软件位（如 COW 标记）不随页复制。目标映射一律走 `page_insert()`，由它维护引用计数和 TLB，不得直接写页表项。
````

#### 实现迭代过程

##### 第一次迭代

**问题解决策略：** 我先照 Lab4 的内核线程路径把 `do_fork()` 的顺序固定下来（PCB → 内核栈 → 地址空间 → 现场 → 登记），再往 `copy_mm()` 里加用户地址空间分支。初版只写了 `mm_create()`/`setup_pgdir()`/`dup_mmap()` 的主干，失败回滚只回收了内核栈和 PCB，漏掉了"地址空间已经复制但登记失败"这条路径。

**遇到的问题：** 内核线程路径下 `alloc_proc()` 把 `pgdir` 初始化为 `NULL`，如果 `copy_mm()` 不显式设成 `boot_pgdir_pa`，`proc_run()` 切 `satp` 时会写进非法值。另外这个工程的 `bool` 是 `typedef long long`，没有 `true`/`false` 常量，`copy_range()` 的 `share` 实参只能传 `1`/`0`。

**最终结果：** 补上 `put_mm()` 后，`do_fork()` 的四条失败路径都能回到干净状态；`copy_mm()` 三条路径各自设好 `mm` 与 `pgdir`。

##### 第二次迭代

**问题解决策略：** 早期版本的 `dup_mmap()` 是先按页复制再补 vma。改成一外层遍历 vma、每个区间调一次 `copy_range()`：vma 先建立，页复制失败时调用方的 `exit_mmap()` 才能按 vma 找到并回收这些页。

**最终结果：** `proc.c`、`vmm.c`、`pmm.c` 对象用 `zig cc` 编译通过，无新增警告。`forktest`/`forktree` 的整机运行依赖模块二的 `load_icode()`、U 态返回和 `do_wait()`，见 5.4。

### 功能模块四：退出、让出与终止

**负责人：** 2412790-杨茜茹

#### 模块功能描述

**需要实现/修改的函数：**

```c
// code/kern/process/proc.c
int do_exit(int error_code);
int do_yield(void);
int do_kill(int pid);

// code/kern/trap/trap.c：更新 IRQ_S_TIMER 分支
void interrupt_handler(struct trapframe *tf);
```

**功能说明：**

- **退出。** `do_exit()` 先保护 idle 和 init。若当前进程有用户 `mm`，先将页表切回 `boot_pgdir_pa`，刷新 TLB 并断开 `current->mm`。只有最后一个引用消失才先解除用户映射，再释放页表根和 `mm`。之后关中断写退出码、设为 `PROC_ZOMBIE`，唤醒正处于 `WT_CHILD` 等待的父进程。若还有子进程，则转交给 `initproc`，同时更新子链和两个兄弟方向。最后进入调度。子进程的 PCB 和仍在使用的内核栈留到 `do_wait()` 回收。
- **让出。** `do_yield()` 设置 `need_resched` 后返回。框架的 `trap()` 在返回 U 态前检查标志，由 `schedule()` 决定下一次运行谁。
- **终止。** `do_kill()` 在临界区查进程、检查状态并设置 `PF_EXITING`。对不可用目标返回 `-E_INVAL`，对已收到请求的进程返回 `-E_KILLED`。只有处于可中断等待的目标才立即唤醒，实际退出由相应检查点触发。时钟中断会重装计时并提出调度请求，给 `spin` 中不主动陷入系统调用的用户进程提供切换机会。

#### 最终提示词

[退出、让出与终止完整提示词](prompts.md#yang-process-control)。下方为同一份复现规格。

````markdown
[PROMPT]

**任务**　在 `code/kern/process/proc.c` 的 `lab5 function4` 标记中实现 `do_exit()`、`do_yield()`、`do_kill()`。在 `code/kern/trap/trap.c` 的 S 态时钟中断分支设置重调度请求。让退出完成本进程的用户地址空间处置并使父进程可以等待回收，让运行中的进程能主动让出或在合适的返回点响应终止请求。

**操作要求**　必须修改工程中的实际文件。保留 `lab5` 标记，把本人完成处的 `YOUR CODE` 改为 `2412790`。用真实 PCB 和等待标志判断唤醒条件，保证亲属链更改位于关中断临界区。确认整个内核栈和 PCB 的最终回收由 `do_wait()` 完成。

**输出要求**　说明退出时的地址空间和 PCB 资源归属、退出码何时可见、唤醒条件和孤儿转交顺序。测试依赖 `wait`、用户 `fork` 或首次 U 态返回时，说明实测边界和剩余联验项。

[RELY]

| 真实文件 | 当前约定 |
| --- | --- |
| `code/kern/process/proc.h`、`code/kern/process/proc.c` | `current`、`idleproc`、`initproc`、`PROC_SLEEPING`、`PROC_RUNNABLE`、`PROC_ZOMBIE`、`PF_EXITING`、`WT_CHILD`、`WT_INTERRUPTED`、`find_proc(int pid)`、`put_pgdir(struct mm_struct *mm)`。`cptr` 指向最新子进程，`optr` 指向更早的兄弟，`yptr` 指向更新的兄弟。 |
| `code/kern/mm/vmm.h`、`code/kern/mm/vmm.c`、`code/kern/mm/pmm.h`、`code/libs/riscv.h` | `mm_count_dec()` 递减引用；`exit_mmap()` 要求 `mm_count(mm) == 0`；`mm_destroy()` 清理 VMA 和 `mm`；`boot_pgdir_pa`、`lsatp()` 与 `flush_tlb()` 用于切回内核页表。 |
| `code/kern/schedule/sched.c`、`code/kern/sync/sync.h`、`code/kern/trap/trap.c` | `wakeup_proc()` 把睡眠进程变为可运行并清等待状态。`schedule()` 负责切换。`local_intr_save/restore` 保存并恢复先前的中断状态。原 `trap()` 在返回用户态前先查 `PF_EXITING`，再查 `need_resched`。 |

[GUARANTEE]

必须实现 `int do_exit(int error_code)`、`int do_yield(void)`、`int do_kill(int pid)`，并更新 `void interrupt_handler(struct trapframe *tf)` 的 `IRQ_S_TIMER` 分支。允许在本人模块加入必要的内部辅助逻辑。保留 `do_wait()`、`set_links()`、`remove_links()`、`wakeup_proc()`、调度器本体、PCB 定义、汇编和原评分测试。等待者负责摘除进程记录、释放内核栈和 PCB；本任务负责退出状态、用户地址空间与唤醒。

[SPECIFICATION]

### `do_exit(int error_code)`

**Pre-Condition**　`current` 为当前进程。普通进程可能有独立、共享或空的用户 `mm`。

**Post-Condition**　正常退出时进程保存退出码并成为 `PROC_ZOMBIE`，交给调度器。等待者仍可以访问僵尸 PCB 和退出码。

- **Case 1 · 保护进程**　尝试退出 `idleproc` 或 `initproc` 时触发内核保护。
- **Case 2 · 有用户地址空间**　把当前记录和硬件页表切回 `boot_pgdir_pa` 并刷新 TLB，清除 `current->mm`。若引用减到 0，按 `exit_mmap()`、`put_pgdir()`、`mm_destroy()` 的顺序释放；仍有引用时保留共享映射。内核线程的 `mm` 为空，跳过该段。
- **Case 3 · 等待者**　在关中断区域写入 `exit_code`、设置 `PROC_ZOMBIE`。父进程同时满足 `PROC_SLEEPING` 和 `WT_CHILD` 才唤醒。
- **Case 4 · 孤儿托管**　从当前进程的 `cptr` 依次摘出子进程，更新原链头反向指针，再把子进程挂到 `initproc` 的子链头，修正 `parent` 和两个兄弟方向。被托管的是僵尸且 `initproc` 正等待子进程时，唤醒 `initproc`。

**Requirements**　父进程观察到僵尸状态时，必要的用户地址空间处置已在前完成。孤儿转交只改变亲属关系，全局链、哈希和计数由等待回收流程维护。恢复原中断状态后调用 `schedule()`，随后继续运行属于错误。

### `do_yield()`、`interrupt_handler()` 与调度检查

**Pre-Condition**　`do_yield()` 在有效的当前进程上下文中调用。时钟中断可能发生在 `current` 建立之前。

**Post-Condition**　调用 `do_yield()` 返回 0 且 `current->need_resched=1`。计时中断在有当前进程时设置同一请求，保留时钟重装与计数。

- **Case 1 · 主动让出**　只更新请求，再由用户 Trap 返回路径执行调度。用户进程再次获选后可从原调用之后继续执行。
- **Case 2 · 忙循环**　时钟中断先重装并计数，再对有效的 `current` 设置调度标志，让一直运行的 U 态进程也能够让出 CPU。
- **Case 3 · 暂无进程或 S 态中断**　`current == NULL` 时仍可处理时钟，跳过进程旗标。调度沿原检查点进行，异步中断保留 `epc`。

**Requirements**　保留计时输出，时钟处理函数不直接修改 `sepc`。通过 `yield` 与 `spin` 的运行输出分别观察主动让出和时钟促成的调度。

### `do_kill(int pid)`

**Pre-Condition**　从 PID 查进程；查找、判断和修改状态放在同一个关中断区域。

**Post-Condition**　首次合法终止请求返回 0，并置 `PF_EXITING`；实际退出发生在进程处理该标志的检查点。

- **Case 1 · 无效目标**　PID 无效、目标不存在、目标是 idle/init 或已是僵尸时返回 `-E_INVAL`。
- **Case 2 · 重复请求**　目标已有 `PF_EXITING` 时返回 `-E_KILLED`，维持现有状态。
- **Case 3 · 可中断等待**　目标是 `PROC_SLEEPING` 且 `wait_state & WT_INTERRUPTED` 非零时，设置退出标志后调用 `wakeup_proc()`。
- **Case 4 · 运行或不可中断等待**　标记退出请求。由原返回或等待路径处理退出，既有进程状态保持到对应检查点。

**Requirements**　各路径都恢复原中断状态。`kill()` 请求终止，`do_exit()` 和 `do_wait()` 分别完成实际退出与最终回收。等待在唤醒后要复查退出标志，这一段与模块二联验。用课程 `Process Control` 中已有的 `do_yield()`、`do_kill(-1)` 检查配合 `exit`、`yield`、`spin` 及补充用例 `waitkill` 验证；`waitkill` 结果单独记录。
````

#### 实现迭代过程

##### 第一次迭代

**问题解决策略：** 我从 `spin` 反查了调度时机。子进程的忙循环不会自己调用 `yield()`，所以将时钟重装和计数保留在中断路径，并设置 `need_resched`，实际切换沿用用户态返回前的检查。退出处理按用户地址空间处置、进程状态发布、最后回收这三个动作核对了顺序。

**最终结果：** `proc.c`、`trap.c` 对象重新编译成功；宿主测试检查了 `yield`、`kill` 的错误分支、可中断等待唤醒、孤儿转交、最后一个 `mm` 引用释放及共享 `mm` 保留。真正的 `exit` 与 `waitpid`、`spin`、`waitkill` 仍需用整合后的内核复验。

### Challenge：写时复制

**负责人：** 2413921-程子涵

`fork` 不再复制物理页，父子共享同一份只读映射，谁先写谁付复制成本。设计上要回答四个问题：怎么认出"这是 COW 页"、引用计数怎么记账、权限和 TLB 怎么保持一致、以及哪些情况不能当成 COW 处理。

#### 设计

**用什么标记 COW 页。** Sv39 的 bit 8–9 是 RSW，硬件不解释，`mmu.h` 原本就把这两位留作 `PTE_SOFT`。取 bit 8 作 `PTE_COW`（`0x100`），与 `V/R/W/X/U/G/A/D` 完全不重叠，`copy_range()` 复制权限位时也不带它。这样"共享且只读"和"本来就只读"在页表项上可以区分：代码段和常量区没有 `PTE_COW`，收到写请求就该按越权处理，而不是悄悄给一份可写副本。

**引用计数不变量。** 实现依赖三条：

1. `page_ref(page)` 等于**映射这一页的页表项数**。共享页在 `fork` 后为 2；`page_insert()` 增一、`page_remove()` 减一，`exit_mmap()` 逐页 `page_remove()` 就够，回收路径不需要为 COW 做任何特殊处理。
2. 带 `PTE_COW` 的页若 `page_ref == 1`，说明共享方已经退出，此时不必复制，把写权限直接还回去。
3. `page_ref > 1` 时，任何进程都不能拿到带 `PTE_W` 的映射。

**权限与 TLB。** 降权必须父子两侧都做——只降一侧，另一侧仍能直接写入，写隔离立刻失效。降权用 `perm = (perm & ~(PTE_W | PTE_D)) | PTE_COW` 重建表项，同时清 `PTE_D`，因为脏位代表"私有可写且已被写过"，共享只读页不该带。每次改页表项都要同步 TLB：原地改 `*ptep` 的地方显式 `tlb_invalidate()`，走 `page_insert()` 的地方由它内部的 `sfence.vma` 负责。否则旧的可写映射可能还留在 TLB 里，写缺页根本不会发生。

**RISC‑V 的 R/W 约束。** 硬件要求 `W=1` 时 `R=1`，所以 `ref==1` 就地恢复写权限时是 `*ptep | PTE_R | PTE_W`，不能只置 `W`。

**降权失败也能自愈。** 先降父进程再降子进程。中途 `page_insert()` 失败时，父页处于"带 `PTE_COW` 且 `page_ref == 1`"的状态，下一次写缺页正好命中 `ref==1` 分支就地恢复写权限，不会把父进程永久锁成只读。

#### 状态转换

| 状态 | 父 PTE | 子 PTE | 原页 `page_ref` | 触发条件与处理 |
| --- | --- | --- | --- | --- |
| S0 私有可写 | `R\|W\|U` | 不存在 | 1 | `fork` 之前的普通用户页 |
| S1 只读共享 | `R\|U\|COW` | `R\|U\|COW` | 2 | `copy_range(share=1)` 两侧降权 |
| S2 子进程已复制 | `R\|U\|COW` | `R\|W\|U` | 1 | 子进程写缺页，`ref>1` 分支分配新页 |
| S3 父进程已复制 | `R\|W\|U` | — | 父独占 | 父进程写缺页，子已退出时走 `ref==1`，不复制 |
| S4 一方退出 | 不变 | 撤销 | 减一 | `exit_mmap()` → `page_remove()` |

只读页（代码段、常量区）不进入 S1：`copy_range()` 只对原本带 `PTE_W` 的页降权，其余保持原权限父子共读，因此它们的写请求始终是越权，不会被 COW 逻辑吞掉。

#### 与 Dirty COW 的关系

CVE-2016-5195 的成因是在 `get_user_pages()` 处理只读私有映射的 `FOLL_WRITE` 请求时，反复重试把 `PTE_W` 直接写进了已经共享的页表项，绕过了 COW 的复制判定，于是用户能改到本不该改的只读文件映射。本实验没有 page cache、没有文件映射、也没有 `get_user_pages()`，结构上不具备那条路径。但两者防的是同一件事：**写权限必须由"这一页是否被多方共享"决定，而不能由调用者的写意图决定。** 本实现在 `do_cow_pgfault()` 里先查引用计数再决定复制还是原地恢复，并且让 `copy_to_user()` 这种"内核代表用户写"的 S 态路径也走同一判断，就是为了不留"某条路径能绕过共享检查"的口子。

适用边界也要说清楚：写保护只在**页粒度**成立，同一页内的不同变量无法各自 COW，写一个字节就会复制整页；`mmap` 共享映射、大页和 DMA 缓冲都不在本实现的覆盖范围；`ref==1` 的就地恢复依赖"引用计数严格等于映射数"，一旦某条路径漏了 `page_remove()`，这里就会误判成独占并改出脏数据。

#### 测试用例

| 用例 | 覆盖点 | 预期 |
| --- | --- | --- |
| `make run-nox-cowtest` | 继承可见性、写私有化、写隔离、跨 3 页写入 | 子进程 `value=2222`、父进程 `value=1111`，`cowtest pass.` |
| `make run-nox-forktest` | 连续 `fork` 32 次、父子返回值、`wait` 回收 | 32 行 `I am child n`，最后 `forktest pass.` |
| `make run-nox-forktree` | 多层 `fork`、进程树登记、调度 | 打印 1+2+4+8 个进程的路径串，无 panic |
| 写代码段 | 只读区没有 `PTE_COW` | 打印现场后进程被 `-E_KILLED` 结束，内核不崩 |
| `waitpid(&code)` 写回 | 退出码写回时该页仍是共享页 | 内核写缺页被正确私有化，退出码正确 |

`cowtest` 是本次新增的用户程序，放在 `code/user/` 下，构建时被自动扫描进 `USER_BINS`，不需要改 Makefile。

#### 实现迭代过程

##### 第一次迭代

**问题解决策略：** 最初只在用户态分支接 COW，认为写缺页只可能来自用户程序。

**遇到的问题：** `copy_to_user()` 在 S 态写共享页时同样会触发写缺页，当时的代码把它当成内核缺页打印现场，用户程序的 `waitpid` 退出码写回直接失败。另外 `ref==1` 分支一开始只置了 `PTE_W`，RISC‑V 要求 `W=1` 时 `R=1`，硬件报权限错误。

**最终结果：** S 态写缺页在 `current->mm != NULL` 时也交给 `do_cow_pgfault()`，其余内核缺页保持原诊断路径；`ref==1` 分支改为 `| PTE_R | PTE_W`。三处改动都落在 `vmm.c` 和 `trap.c` 内，没有动 `page_insert()` 和 `exit_mmap()` 的语义。

#### 最终提示词

[COW Challenge 完整提示词](prompts.md#cheng-cow)。下方为同一份复现规格。

````markdown
[PROMPT]

**任务**　在任务四的基础上把 `fork` 改成写时复制。新增 `do_cow_pgfault()` 处理写缺页，在 `code/kern/trap/trap.c` 的 `exception_handler()` 中接入用户态和内核态的写缺页分支，并把 `copy_range()` 的共享路径接上：父子共享同一物理页、同时降为只读并打 COW 标记。

**操作要求**　必须修改工程中的实际文件。新增的 `PTE_COW` 只能占用 RSW 位，不得动硬件位。降权必须父子两侧都做。`do_cow_pgfault()` 只在能确认是 COW 页时返回 0，其余情况返回负值让原异常路径继续，不能把越权写当成可恢复缺页吞掉。改页表后必须同步 TLB。保留 `page_insert()`、`page_remove()`、`exit_mmap()` 的既有语义，回收路径不为 COW 特判。

**输出要求**　给出 COW 标记的选位理由、引用计数不变量、`ref==1` 与 `ref>1` 两种分支的处理、权限与 TLB 的一致性做法、状态转换表，以及与 Dirty COW 的关系和写保护的适用边界。

[RELY]

| 真实文件 | 当前约定 |
| --- | --- |
| `code/kern/mm/mmu.h`、`code/kern/mm/pmm.c` | `PTE_V/R/W/X/U/G/A/D` 与 `PTE_SOFT`（RSW 位 9:8 保留给软件）；`page_insert()`、`page_remove()`、`page_ref()`、`pte2page()`、`tlb_invalidate()`。 |
| `code/kern/mm/vmm.c`、`code/kern/mm/vmm.h` | `find_vma()`、`struct vma_struct` 的 `vm_flags`、`VM_WRITE`、`mm->pgdir`；`exit_mmap()` 按 vma 逐页 `page_remove()`，不区分子进程是否共享。 |
| `code/kern/trap/trap.c`、`code/kern/trap/trap.h` | `struct trapframe` 的 `cause`、`tval`、`epc`；`CAUSE_STORE_PAGE_FAULT`、`CAUSE_USER_ECALL`；`trap_in_kernel()`；原有"用户非 `ecall` 异常打印现场并 `do_exit(-E_KILLED)`"策略。 |

[GUARANTEE]

| 允许修改的文件 | 必须提供或适配的入口 |
| --- | --- |
| `code/kern/mm/mmu.h` | `#define PTE_COW`，取值落在 RSW 位。 |
| `code/kern/mm/vmm.c`、`code/kern/mm/vmm.h` | `int do_cow_pgfault(struct mm_struct *mm, uintptr_t addr)`；COW 复制计数。 |
| `code/kern/mm/pmm.c` | `copy_range()` 的 `share=1` 路径：父子同时降权。 |
| `code/kern/trap/trap.c` | `exception_handler()` 的写缺页分支，用户态放在结束进程之前，内核态放在原诊断路径之前。 |
| `code/user/cowtest.c`（新增） | 写隔离、继承可见性、跨页写入的验收程序。 |

保留 `page_insert()`、`page_remove()`、`exit_mmap()` 的既有语义：共享页的引用由 `page_insert()`/`page_remove()` 维护，`exit_mmap()` 逐页 `page_remove()` 即可，不需要为 COW 改写回收路径。保留用户态"非可恢复异常一律退出"的策略，只有 `do_cow_pgfault()` 明确返回 0 才继续执行。

[SPECIFICATION]

### `PTE_COW`

**Pre-Condition**　Sv39 页表项的 bit 8–9 是 RSW，硬件不解释。

**Post-Condition**　`PTE_COW` 取 bit 8（`0x100`），与 `PTE_SOFT`（`0x300`）掩码一致，不与 `V/R/W/X/U/G/A/D` 冲突；`copy_range()` 复制权限位时不含该位。

- **Case 1 · 与硬件位共存**　降权时 `perm = (perm & ~(PTE_W | PTE_D)) | PTE_COW`，只清写位和脏位。
- **Case 2 · 与 `page_insert()` 交互**　`page_insert()` 用 `pte_create(page2ppn(page), perm)` 重建表项，`perm` 中带上 `PTE_COW` 才能保留标记。

**Requirements**　不得占用 bit 0–7 和 bit 10 以上（PPN）。

### `copy_range()` 的共享路径

**Pre-Condition**　`share=1`，源叶项有效。

**Post-Condition**　原本可写的页在父子两侧都变成"只读 + COW"，`page_ref` 为 2；原本只读的页保持原权限，父子共读且不打 COW 标记。

- **Case 1 · 可写页降权**　先 `page_insert(from, page, start, perm)` 把父进程改只读，再 `page_insert(to, page, start, perm)` 建立子进程映射，第二次插入把引用加到 2。
- **Case 2 · 只读页共读**　`perm` 无 `PTE_W` 时不降权、不打 COW，两侧直接共享；写这种页仍按越权处理。
- **Case 3 · 降权中途失败**　父页留在"只读 COW 且引用为 1"，下一次写缺页在 `do_cow_pgfault()` 中走 `ref==1` 分支就地恢复写权限，失败可自愈。

**Requirements**　降权必须在父子两侧都做。只降一侧会让另一方直接写入共享页，写隔离失效。

### `do_cow_pgfault(struct mm_struct *mm, uintptr_t addr)`

**Pre-Condition**　`mm` 非空，`addr` 是触发写缺页的用户虚拟地址。

**Post-Condition**　返回 0 表示缺页已处理，触发指令可以重新执行；返回负值表示不是可处理的 COW 缺页，由调用方维持原策略。

- **Case 1 · 地址或 vma 不合法**　`mm`/`pgdir` 为空、`addr` 不在 `USER_ACCESS` 内、`find_vma()` 为空，或 vma 无 `VM_WRITE`，返回 `-E_INVAL`。只读区收到写请求是越权，不能顺手给一份可写副本。
- **Case 2 · 页不是 COW 页**　`get_pte(..., 0)` 为空、叶项无 `PTE_V`、无 `PTE_COW`，或已带 `PTE_W`，返回 `-E_INVAL`。已经可写还缺页说明另有原因，不能在这里吞掉。
- **Case 3 · 独占（`page_ref == 1`）**　共享对象已不存在，原地恢复 `PTE_W` 并补 `PTE_R`，清 `PTE_COW` 与 `PTE_D`，`tlb_invalidate()` 后返回 0，不分配新页。
- **Case 4 · 共享（`page_ref > 1`）**　`alloc_page()` 失败返回 `-E_NO_MEM`；成功则 `memcpy()` 整页内容，`perm = (原项 & (PTE_U | PTE_X)) | PTE_R | PTE_W`，`page_insert()` 替换映射。`page_insert()` 摘掉旧映射时把共享页引用减 1，新页引用为 1。
- **Case 5 · 引用计数异常**　`page_ref <= 0` 返回 `-E_INVAL`。

**Requirements**　每次改页表项都要 `tlb_invalidate()`，或依赖 `page_insert()` 内部的 `sfence.vma`，否则旧的可写映射可能还在 TLB 里。`do_cow_pgfault()` 只处理写缺页，取指缺页和读缺页不走这里。

### `exception_handler()` 的接入位置

**Pre-Condition**　`tf->cause` 已确定，`trap_in_kernel(tf)` 区分陷入来源。

**Post-Condition**　可恢复的 COW 写缺页被处理并在 `sret` 后重执行；其余用户异常维持"打印现场 + `do_exit(-E_KILLED)`"。

- **Case 1 · 用户态 `ecall`**　先 `tf->epc += 4` 再 `syscall()`。`fork` 会复制这个帧，子进程必须从 `ecall` 之后继续。
- **Case 2 · 用户态写缺页**　`CAUSE_STORE_PAGE_FAULT` 且 `do_cow_pgfault(current->mm, tf->tval) == 0` 时直接返回，不推进 `epc`。位置必须在"用户异常一律退出"之前。
- **Case 3 · 内核态写缺页**　`copy_to_user()` 写 COW 页会在 S 态触发同样的缺页，`current->mm` 非空时同样交给 `do_cow_pgfault()`。其余内核缺页保持原诊断路径，不掩盖真实错误。
- **Case 4 · 非 COW 缺页**　`do_cow_pgfault()` 返回负值，走原分支。

**Requirements**　用户态分支的判定顺序不能颠倒；内核态分支必须先确认 `current->mm != NULL`，内核线程没有用户地址空间。

### `user/cowtest.c`

**Pre-Condition**　内核已支持普通 `fork` 与 `waitpid`。

**Post-Condition**　子进程读到父进程 `fork` 前的数据、写入后能读回、父进程数据不变；输出 `cowtest pass.`。

- **Case 1 · 继承可见性**　`fork` 前写 `shared_value = 1111` 并填满 3 页模式 buffer，子进程校验值与模式。
- **Case 2 · 写私有化**　子进程写 `shared_value = 2222` 并 `memset` 整个 buffer，逐字节读回 `0x5a`。
- **Case 3 · 写隔离**　父进程 `waitpid` 后校验 `shared_value == 1111` 且模式未被改。

**Requirements**　用 `make run-nox-cowtest` 运行。依赖模块二提供 `load_icode()`、U 态返回和 `do_wait()`，模块二合入前无法整机执行。
````

### 分支任务：双重 GDB 观察系统调用与返回

**负责人：** 2412790-杨茜茹、2411824-于蕾、2413921-程子涵

整合后共同完成。杨茜茹负责核对用户调用号、参数、`epc` 前移和 `a0` 返回值；于蕾负责归纳首次进入用户态及 `sret`，程子涵核对运行证据。于蕾把实际调试记录和 QEMU/TCG 的观察并入本节。

---

<a id="verification"></a>

## 五、测试与验证

### 5.1 前置迁移与 RISC‑V 对象编译

`6e9bc57` 的迁移验收在仓库根目录执行：

```bash
make -C code \
  obj/kern/mm/default_pmm.o obj/kern/mm/pmm.o \
  obj/kern/init/entry.o obj/kern/trap/trap.o \
  obj/kern/trap/trapentry.o obj/kern/process/switch.o \
  obj/kern/schedule/sched.o obj/kern/process/proc.o
```

8 个对象通过。对后来提交到 `1d3fe34` 的功能源码，在正确的 WSL 工作区做了强制重编：

```bash
make -B -j1 -C code \
  obj/kern/trap/trap.o obj/kern/syscall/syscall.o \
  obj/kern/mm/pmm.o obj/kern/process/proc.o \
  obj/user/libs/syscall.o obj/user/libs/ulib.o
```

终端输出包含 6 条对应的 `+ cc` 记录，命令成功返回。

【此处应放置截图：执行本节第二条 `make -B -j1 -C code` 完整命令，截取 6 条 `+ cc` 和返回终端的输出。】

### 5.2 独立宿主测试

我用宿主 GCC 检验内核侧的分发、Trap 分支和进程控制函数。测试输入由陷入帧和模拟 PCB 构造，`do_fork()`、`do_wait()`、`do_execve()`、`schedule()`、页表切换等依赖用测试替身记录参数和调用顺序。5 组检查全部通过。

| 场景 | 检查内容 |
| --- | --- |
| 1. Trap 入口和分发 | `epc` 从 `0x1000` 到 `0x1004`，`getpid`、`putc`、`pgdir`，非法编号和带高位编号，`fork` 收到已推进的帧及用户栈。 |
| 2. 用户参数 | `wait` 输出地址为空、合法及不合法时的分支，PID 位宽检查；`exec` 的读取权限和四参数转发。 |
| 3. 让出、终止和时钟 | `need_resched`、无效 PID、重复终止、可中断和不可中断等待、计时重装和 `epc` 保持。 |
| 4. 退出与孤儿托管 | 退出码、最后一个 `mm` 引用的释放顺序、父进程唤醒和亲属链重挂。 |
| 5. 共享地址空间及保护进程 | 还有引用时保留 `mm`；用户非 `ecall` 异常走退出；idle 和 init 触发保护。 |

该测试没有执行 RV64 用户库的 `ecall` 或真实 `sret`，`pgdir` 场景检查的是分发调用，真实页表输出待 QEMU 验证。

### 5.3 完整构建与待完成的整机验证

```bash
make -j1 -C code > /tmp/lab5-yang-1d3fe34-build.log 2>&1
printf 'make 退出码：%s\n' "$?"
tail -n 30 /tmp/lab5-yang-1d3fe34-build.log
```

在 `1d3fe34` 上，构建编译了内核和用户程序，执行到 `+ ld bin/kernel` 后因 `do_execve` 和 `do_wait` 未定义停止，退出码为 2。`user/testbss.c` 越界写的编译警告对应其测试异常路径，实际停止点是内核链接。此提交还没有 QEMU 串口结果或课程评分。

【此处应放置截图：执行本节的 `make -j1 -C code` 构建和日志查看命令，截取 `+ ld bin/kernel`、两个 `undefined reference` 和退出码 2。此图记录阶段构建的阻塞。】

整合后按同一源码提交先完整构建，再记录 `make -C code qemu` 和 `make -C code grade` 的真实输出。重点核对 `Syscall`、`Process Control`、`hello`、`pgdir`、`badarg`、`yield`、`exit`、`spin` 和 `forktest`。`waitkill` 是课包中的补充用例，补测可中断等待和孤儿托管，单独记录运行结果。

【此处应放置截图：整合后执行 `make -C code qemu`，截取内存与页表自检，以及当前选定用户程序的实际运行输出。】

【此处应放置截图：整合后执行 `make -C code grade`，截取原评分脚本的各测试结论和最终总分。】

### 5.4 模块三与 COW 的对象编译与用例

本机是 Windows，没有 `riscv64-unknown-elf-gcc` 和 `qemu-system-riscv64`，所以本人这一层验证做到"对象级编译 + 用户程序编译 + 用例设计"，整机运行留给整合后的复验。用 zig 0.13.0 的 `zig cc` 顶替工具链，目标三元组和编译参数与 Makefile 一致：

```bash
zig cc -target riscv64-freestanding -mcmodel=medany -O2 -std=gnu99 \
  -Wno-unused -fno-builtin -nostdinc \
  -Ilibs -Ikern/debug -Ikern/driver -Ikern/trap -Ikern/mm -Ikern/libs \
  -Ikern/sync -Ikern/fs -Ikern/process -Ikern/schedule -Ikern/syscall \
  -fno-stack-protector -ffunction-sections -fdata-sections -c <源文件> -o <目标>
```

| 范围 | 数量 | 结果 |
| --- | --- | --- |
| `code/kern/` 全部 `.c` | 19 | 全部编译通过；本模块改动的 `vmm.c`、`pmm.c`、`proc.c`、`trap.c` 无新增警告 |
| `code/user/*.c` | 16 | 全部通过，含新增的 `cowtest.c` |
| `code/user/libs/*.c` | 5 | 全部通过 |

顺带发现一个基线遗留问题：`code/kern/mm/kmalloc.c` 的 `__slob_free_pages(unsigned long kva)` 把整型直接传给 `kva2page(void *)`，GCC 只告警而 Clang 视为 error。这属 Lab2 迁移代码，本人未改动。

用例与预期见第四章 Challenge 的测试用例表。整机部分（`run-nox-cowtest`、`run-nox-forktest`、`run-nox-forktree`、`grade`）依赖模块二提供的 `load_icode()`、U 态返回和 `do_wait()`，模块二合入前内核链接会停在 `undefined reference`，因此本阶段不提供 QEMU 串口输出。

【此处应放置截图：整合后执行 `make -C code run-nox-cowtest`，截取子进程与父进程的 `value`、`first byte` 两行和 `cowtest pass.`。】

---

## 六、实验总结与收获

### 对操作系统的理解

| 本实验知识点 | 与操作系统原理的对应 |
| --- | --- |
| 从 `ecall` 到内核分发 | 特权级转换是硬件提供的入口，调用约定由操作系统定义。处理器借助 CSR 和 Trap 保存返回位置，内核用帧中的寄存器解释编号、参数与结果。 |
| `exit`／`wait` 分两步回收 | 用户地址空间和代表进程身份、退出码的 PCB 生命周期不同。退出者仍占用当前内核栈，要先切走，由等待者在安全时机完成最后回收。 |
| `yield`、时钟中断与调度 | 设置 `need_resched` 表达调度意图。可运行状态、何时真正切换和切换到谁，分别由进程状态、检查点和调度算法决定。 |
| 地址空间引用与切换 | `mm_count` 管理地址空间共享者的生命周期，物理页引用计数管理实际页。释放最后一份 `mm` 前要切离它的页表，避免仍从待回收页表取地址转换。 |

本阶段的实现和验收没有涉及多核调度中的跨核抢占，也没有测量真实工作负载下的调度公平性。全组知识点汇总和双重 GDB 的运行观察待最终记录合入。

### AI 协作开发的经验

我先把寄存器 ABI、源码路径和已经存在的函数列进提示词，再对照生成代码检查参数位置和返回值宽度。原用户库的可变参数读取和 `sd` 的宽度问题都是从代码里核对出来的。测试时先看当前阶段能验证到哪一层：对象编译成功后，整机仍可能在链接阶段缺接口。把源码判断、宿主结果和真实运行日志分开，合并后才能知道还要补哪一项验证。
