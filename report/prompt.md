# Lab4 提示词记录

负责人：2412790 杨茜茹。本文件当前只包含前置迁移任务；页操作实现与测试提示词后续补充。

## 任务一：迁移 Lab2/Lab3 基础功能

[PROMPT]

**任务：** 在 Lab4 工程中补齐 `code/kern/mm/default_pmm.c`、`code/kern/mm/pmm.c` 的 lab2 标记，以及 `code/kern/trap/trapentry.S`、`code/kern/trap/trap.c` 的 lab3 标记。实际修改文件，不仅展示代码。参考本组 Lab3 集成提交 `6d8bc3cc39a3e5751aa3ce9dda7b6464773e5f07` 中已有的物理内存和 Trap 实现，按照 Lab4 当前接口适配，保留原作者与迁移者信息。

**思路：** 设备树提供 RAM 范围，page_init 建立 Page 数组，保留内核及管理结构占用页，把其余完整页面交给 Best-Fit。分配器按地址组织空闲块，分配时遍历选最小适配块，释放时合并相邻块。Trap 先由框架保存寄存器，再传入 C 处理，最后恢复返回状态及通用寄存器。定时器继续重装和计数，不沿用 Lab3 十次报告后关机策略。

**修改边界：** 只改上述四个源文件的填空及必要的错误头文件引用。保留原 basic_check、best_fit_check、页表检查、SAVE_ALL、forkrets、idt_init 与 trap 分发。不得修改进程创建、调度、switch.S、Makefile、结构体及原评分脚本；本次不实现 page_insert/page_remove。允许新增内部静态辅助函数和变量，不新增跨文件公共接口。

**输出：** 给出四文件差异、迁移说明及四个对象文件的编译结果。未运行的 QEMU/评分明确写未验证，不把对象编译当作完整内核通过。

[RELY]

- `code/kern/mm/pmm.h` 提供 pmm_manager 的 init、init_memmap、alloc_pages、free_pages、nr_free_pages、check 接口；签名以该文件为准。
- Lab4 `default_pmm.h` 声明的是 default_pmm_manager，pmm.c 也选择该对象。default_pmm.c 引用的 best_fit_pmm.h 在框架中不存在，应改为 default_pmm.h；算法仍为 Best-Fit，以兼容原测试。
- `code/kern/mm/memlayout.h` 的 Page 包括 ref、flags、property、page_link、pra_page_link、pra_vaddr；后两项也应初始化，但本任务不实现换页。
- 链表与基本类型来自 `code/libs/list.h`、`code/libs/defs.h`。引用计数及地址转换使用 pmm.h；内存信息使用 dtb.h 的 get_memory_base/get_memory_size。
- 可管理物理上界按 `KERNTOP - va_pa_offset` 计算；npage 是物理页号上界，描述符数量为 npage-nbase。
- Lab4 SAVE_ALL 使用 36 个 REGBYTES 槽；槽 2 是原 sp，32/33 是 sstatus/sepc，34/35 是诊断信息。当前框架没有写 x0 槽，不能照搬“该槽必为零”的测试假设。sscratch 行为也以当前框架为准。
- clock_set_next_event、ticks、print_ticks 已提供；不新增时钟源或调度策略。

[GUARANTEE]

必须提供以下定义并与现有调用连接：

```c
static void page_init(void);
static void best_init(void);
static void best_init_memmap(struct Page *base, size_t n);
static struct Page *best_alloc_pages(size_t n);
static void best_free_pages(struct Page *base, size_t n);
static size_t best_nr_free_pages(void);
static void best_insert_free_block(struct Page *base, size_t n);
const struct pmm_manager default_pmm_manager; // check 指向原 best_fit_check
void interrupt_handler(struct trapframe *tf);
static int exception_inst_length(uintptr_t epc);
void exception_handler(struct trapframe *tf);
```

汇编补齐 RESTORE_ALL 与 __alltraps 内传参、调用。保持 free_area/free_list/nr_free 的原测试约定，必要时前向声明 best_fit_check。

[SPECIFICATION]

### page_init

**Pre-Condition：** 已解析设备树，启动映射生效，管理器已初始化；采用固定 DRAM_BASE 的单段 RAM。

**Post-Condition：** offset/npage/pages 正确；保留区不会进入空闲链表。

**Case 1：** 正常 RAM：裁剪上界、尾端向下页对齐；描述符置于内核 end 后的页边界，先检查容纳空间再写入；所有页初始保留，描述符后完整空闲页注册一次。

**Case 2：** base/size 为零、求和溢出、base 不符、RAM 不覆盖内核或放不下描述符：panic，不静默使用默认 RAM。

**Case 3：** 描述符能放下但无完整空闲页：不调用 init_memmap。

**Requirements：** VA/PA/PPN/Page 指针分清，初始化两个链表节点及 pra_vaddr；保留实际范围输出。

### best_init / best_init_memmap / best_insert_free_block

**Pre-Condition：** init 尚未管理活动分配；登记的 n>0，区间有效且每页 Reserved。

**Post-Condition：** init 得到空表和零计数；登记后页引用为零、解除保留，按地址入表，相邻块合并。

**Cases：** 空表插入；表头/中间/表尾插入；前邻、后邻、双向连续分别合并；n=0 或非 Reserved 触发断言。

**Requirements：** 只给块头设 PageProperty；每次登记/释放只累计一次 n，不因合并重复计数。

### best_alloc_pages

**Pre-Condition：** 已初始化；有效调用 n>0。

**Post-Condition：** 成功分配恰好 n 页，计数减 n，已分配头清 PageProperty。

**Case 1：** 多个可用块选择 property>=n 中最小的；同长选低地址。完全匹配摘除，较大则低端分配、余块仍按地址排列。

**Case 2：** 总量不足、空表或无连续块：NULL，状态不变。

**Case 3：** n=0：断言。

### best_free_pages / best_nr_free_pages

**Pre-Condition：** 释放有效已分配连续区间，n>0，每页非 Reserved 且非空闲块头；不是任意错误指针检测器。

**Post-Condition：** 清引用和标志，通过插入辅助函数回收及合并，计数加 n；查询返回计数且不改状态。

**Cases：** 不相邻、仅前邻、仅后邻、双向相邻；零长度/不符页面状态断言。保持原测试的临时链表隔离方式，不增加独立索引。

### RESTORE_ALL / __alltraps

**Pre-Condition：** 框架已保存完整的内核态帧；栈符合调用对齐。

**Post-Condition：** a0=sp 调用 trap，返回后恢复 sstatus/sepc 与 x1、x3..x31，最后加载原 sp，经已有 sret 返回。

**Cases：** 普通返回；处理器修改帧内寄存器/epc 后按修改返回；forkrets 直接进入 __trapret 仍有效。

**Requirements：** x0 不恢复，sp 必须最后恢复，不恢复诊断 CSR；不增加 U/S 切换，不改变 SAVE_ALL。

### interrupt_handler

**Pre-Condition：** 分发器已识别为中断，tf 有效。

**Post-Condition：** 原 epc 不变。

**Cases：** S timer 重装后 ticks 加一，每100次调用原 print_ticks；S software 清 SSIP；其他原因打印现场。

**Requirements：** 不带入十次报告关机，不冒充完整外设处理，不修改调度策略；原 DEBUG_GRADE 行为保留。

### exception_inst_length / exception_handler

**Pre-Condition：** 对断点和非法指令，epc 处至少16位可读；其他异常不读取 epc 内容。

**Post-Condition：** 16位返回2，32位返回4，长编码返回0；只对已知长度的断点/非法指令推进 epc。

**Cases：** c.ebreak 跳2，ebreak 跳4，非法指令按编码跳2/4；长编码和其他异常仅诊断，不擅自推进。

**Requirements：** 这是课程内核的恢复范围，不声称支持任意损坏地址或缺页恢复。

### 验证阶段

先强制重编四个对象，记录命令和退出码，并核对原测试/队友文件未改。页操作及线程模块尚未完成，完整构建、QEMU 和原评分留到后续进行；不得改测试使编译通过。


## 任务二：页映射插入与删除

负责人：2412790 杨茜茹。

[PROMPT]

在真实文件 `code/kern/mm/pmm.c` 的 LAB4 function1 标记处实现 page_insert 与 page_remove，将 YOUR CODE 换为2412790。插入建立或替换一个4KiB映射，删除撤销对应映射。维护 PTE、数据页引用计数及 TLB 一致性。修改真实文件，不只输出代码。

允许修改此标记内的实现，禁止修改 get_pte、tlb_invalidate、原有检查、公共头文件、进程和调度模块。输出具体差异与实际编译结果；完整运行和失败注入测试没有执行时不能宣称通过。

[RELY]

框架 pmm.h 提供以下签名：
```c
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create);
void tlb_invalidate(pde_t *pgdir, uintptr_t la);
```
使用现有 page_ref_inc/page_ref_dec、pte2page/pde2page、page2ppn、pte_create、free_page。当前 get_pte 按 PDX1、PDX0、PTX 访问三级页表；第二次分配失败不会回收第一次分配的页表。当前 tlb_invalidate 对地址执行 sfence.vma，不检查 pgdir 是否活跃。不得把其注释当作额外保证。

[GUARANTEE]

保留函数签名、原测试和页表布局；支持 la 含页内偏移。这里只处理框架管理的4KiB页表路径，不处理经过大页叶子的拆分。调用者提供有效页目录和物理页，保证没有并发修改同一路径。权限按调用者传入值构造，并加 PTE_V，不擅自增加 PTE_R 或改变测试使用的权限组合。

[SPECIFICATION]

### page_insert

**Pre-Condition：** pgdir、page 有效；页面尚未释放；页表路径符合上述约定。

**Post-Condition：** 成功返回0，目标页表项指向page，权限更新，对应引用增加一次；失败返回-E_NO_MEM，不增加数据页引用，不留下本次创建的半成品页表。

**Case 1：** 无旧映射：定位/创建末级PTE，引用加一，写入映射并刷新对应TLB。

**Case 2：** 原映射为同一页：更新权限，但净引用计数不变，不调用free_page释放目标页。先加引用，再抵消这次增量。

**Case 3：** 原映射为其他页：取得新页引用，撤销旧映射、递减旧页引用；旧页仍有其他映射时不释放，归零才释放；最后安装新映射并刷新。

**Case 4：** 首个页表分配失败：返回错误，原状态不变。

**Case 5：** 新建第一个中间表后第二次分配失败：恢复调用前根项，刷新并释放本次创建的中间表；已有中间表不能回收。此回滚依据当前get_pte仅有两次分配的具体行为。

**Requirements：** 取得PTE成功后才变更数据页引用；不要复制整套get_pte；不把已有共享页表当作失败清理对象。

### page_remove

**Pre-Condition：** 页目录有效，路径符合4KiB框架约定，有效映射的引用计数正确。

**Post-Condition：** 对应映射消失，引用减一，只有归零才回收数据页；重复删除无额外影响。

**Case 1：** 中间页表不存在：get_pte(create=0)，直接返回，不分配。

**Case 2：** PTE存在但无效：直接返回，不减少引用。

**Case 3：** 有效且存在其他引用：清PTE、刷新、减引用，不释放。

**Case 4：** 最后引用：清PTE并刷新后减到零，释放数据页。

**Requirements：** 不自动回收空中间页表，原检查及地址空间拥有者负责其生命周期。此任务不改调度/关中断策略。

### 待执行验证

对象编译只是第一步。独立测试需覆盖新映射、同页更新权限、不同页替换、多地址共享、未对齐la、缺失/无效/重复删除、两级分配失败回滚和已有中间表的保留；之后运行原页表检查与集成评分。每项记录实际结果，不能以设计用例代替执行证据。
