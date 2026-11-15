# Lab4 提示词
- [第一章：虚拟内存页操作——杨茜茹](#module-one)
- [第二章：进程创建及初始化——程子涵](#module-two)
- [第三章：进程调度——于蕾](#module-three)

---

<a name="module-one"></a>

## 功能模块一——虚拟内存页操作

**负责人：2412790 杨茜茹。**

### 任务一：迁移 Lab2/Lab3 基础功能

本页按新要求补全已有提示词，保留实现范围与实际验证记录。

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

### best_init

**Pre-Condition**：管理器尚未接管活动分配。

**Post-Condition**：空闲链表自环，nr_free 为零。

**Case 1**：启动首次调用后，在登记 RAM 前查询空闲页数，得到零。

**Requirements**：初始化模块自己的状态，不改物理页描述符布局。

### best_init_memmap

**Pre-Condition：** 管理器已初始化；登记的 n>0，区间有效且每页 Reserved。

**Post-Condition：** 登记页引用为零、解除保留，通过 best_insert_free_block 按地址入表并合并相邻块；已有空闲区保留。

**Cases：** 空表插入；表头/中间/表尾插入；前邻、后邻、双向连续分别合并；n=0 或非 Reserved 触发断言。

**Requirements：** 只给块头设 PageProperty；每次登记/释放只累计一次 n，不因合并重复计数。

### best_insert_free_block

**Pre-Condition：** n>0，区间有效且未在空闲链表中；调用者已清理区间内页面的标志和引用。该函数同时供登记和释放路径调用，不要求页面仍为 Reserved。

**Post-Condition：** 区间按地址顺序插入一次；相邻空闲块合并，nr_free 只增加 n。

**Cases：** 空表、表头、中间和表尾插入；仅前邻、仅后邻、双向相邻分别合并。

**Requirements：** 插入完成后停止查找，不能重复挂接同一节点；合并时清除被吞并块头的 PageProperty 并摘除其节点。

### best_alloc_pages

**Pre-Condition：** 已初始化；有效调用 n>0。

**Post-Condition：** 成功分配恰好 n 页，计数减 n，已分配头清 PageProperty。

**Case 1：** 多个可用块选择 property>=n 中最小的；同长选低地址。完全匹配摘除，较大则低端分配、余块仍按地址排列。

**Case 2：** 总量不足、空表或无连续块：NULL，状态不变。

**Case 3：** n=0：断言。

### best_nr_free_pages

**Pre-Condition**：管理器已初始化。

**Post-Condition**：返回当前空闲页数，链表和计数保持原状。

**Case 1**：空表且计数为零时返回零。登记、分配或释放后返回各次操作更新的计数。

**Requirements**：按页计数，维持原测试直接读写 nr_free 的约定。

### best_free_pages

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

### default_pmm_manager 注册

**Pre-Condition**：Lab4 的 default_pmm.h 声明 default_pmm_manager，pmm.c 的 init_pmm_manager 已选用该对象。

**Post-Condition**：init、init_memmap、alloc_pages、free_pages、nr_free_pages 分别绑定对应 best_* 函数，check 绑定原 best_fit_check。

**Case 1**：完成初始化后，经公共 alloc_pages 调用进入 Best-Fit 实现。若最先遇到的空闲块较大，仍选择后面最小的适配块。

**Requirements**：使用 Lab4 的 default_pmm_manager 符号。示例中的 best_fit_pmm_manager 名称属于 Lab2。保留原检查函数及 free_list、nr_free 约定。链表接口取自 code/libs/list.h。

### 验证阶段

先强制重编四个对象，记录命令和退出码，并核对原测试/队友文件未改。页操作及线程模块尚未完成，完整构建、QEMU 和原评分留到后续进行；不得改测试使编译通过。

### 任务二：页映射插入与删除

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

**Post-Condition：** 成功返回0，目标页表项指向page，权限更新；新建映射或异页替换时新页引用增加一次，同页重插时净引用不变；失败返回-E_NO_MEM，不增加数据页引用，不留下本次创建的半成品页表。

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

### 验收及输出

列出实际修改位置、成功和失败路径、执行的命令、退出码及未验证场景。提供可复现记录。

对象编译只是第一步。独立测试需覆盖新映射、同页更新权限、不同页替换、多地址共享、未对齐la、缺失/无效/重复删除、两级分配失败回滚和已有中间表的保留；之后运行原页表检查与集成评分。每项记录实际结果，不能以设计用例代替执行证据。

### 任务三：页操作独立验证——事后整理的测试规格

负责人：2412790 杨茜茹。

**记录性质：** 实际过程是先提出独立测试需求，AI 编写宿主测试与临时 QEMU 工程脚本，再由本人在 WSL 执行并反馈日志。以下按照课程四段式模板整理已实现的测试设计，供复现与答辩使用，不声称是运行前逐字发送的原始提示词。前两项为按最新要求完善后的实现提示词，不作为历史逐字原文。

[PROMPT]

为实际 `code/kern/mm/pmm.c` 中的 page_insert/page_remove 编写独立验证，修改真实测试文件：`code/tests/member_a/run_page_ops_host.py`、`code/tests/member_a/run_page_ops_qemu.py`。不要修改生产实现或原评分。将模拟依赖的接口测试和使用真实页表的 QEMU 测试分开，说明每一层能证明什么。

宿主测试提取当前实现函数体，而非复制一份算法重写。QEMU 使用临时副本，仅替换其中的 kern/init/init.c，调用真实 pmm_init 和页操作，不要求尚未完成的进程创建/调度模块参与运行。测试有断言，成功标记只能在全部检查通过后打印。输出日志、退出码和来源哈希；不得生成虚假截图或把本测试当作原评分结果。

[RELY]

- 实现入口位于 pmm.c 的 LAB4 function1 标记；源文件中 page_insert 在 page_remove 前。
- 宿主环境提供 gcc；模拟 Page 引用及释放状态、末级 PTE、get_pte 故障、free_page 和 tlb_invalidate 调用。模拟只验证这些接口约定，不执行 RISC-V sfence.vma。
- QEMU 使用工程原 Makefile、链接脚本、启动汇编、PMM、get_pte 和 TLB 失效实现。Makefile 的 --gc-sections 可移除测试入口未使用的函数，故“参与编译”不等于“运行覆盖”。
- pmm_init 会调用原物理页和页表检查。独立用例选用 0x40000000 子树，必须先断言原根项为空，不能覆盖已有映射。
- 本工程 page_remove 不回收中间表；测试独占子树的两张表由测试在撤销所有映射后清理。

[GUARANTEE]

测试脚本提供 `--root` 参数，接收仓库根目录，构建产物放 code/obj 或临时目录，日志放 report/logs。宿主脚本提供 --cc 选择编译器。

正式源码、Makefile、原测试、队友模块保持不变。QEMU 用例执行结束调用 sbi_shutdown，最多等待30秒。失败应返回非零，不能因打印了部分 PASS 就认定全部通过。宿主编译开启 -Wall -Wextra -Werror；QEMU 保留原编译规则，警告如实记录。

[SPECIFICATION]

### 宿主验证

**Pre-Condition：** 读取当前 pmm.c；提取两个真实函数体，测试依赖不替代被测函数本身。

**Post-Condition：** 编译和运行均退出0，8个场景断言全部通过才输出8/8；记录被测源文件 SHA256。

| Case | 输入与操作 | 必须检查 |
| --- | --- | --- |
| 1 | 新页映射到0x123 | 按所属页定位，PTE指向正确页，引用为1，刷新一次 |
| 2 | 同页重插并增加写权限 | 权限更新，引用仍为1，没有释放，发生刷新 |
| 3 | 两地址共享一页，再替换其中一项 | 旧页引用从2降到1，不释放，新页引用为1 |
| 4 | 删除旧页最后映射，再重复删除 | 第一次归零释放，PTE清零；第二次计数、刷新与分配次数不变 |
| 5 | 不同页替换只有一个引用的旧页 | 旧页归零释放，新页拥有一个引用 |
| 6 | 查询不存在页表、首次分配失败 | 删除不创建表；插入返回-E_NO_MEM，数据页引用不变 |
| 7 | 模拟先建表再失败 | 恢复调用前无效根项，清除新增表引用并释放；不增加数据页引用 |
| 8 | 已有中间表但后续分配失败 | 原表项、原表引用保持，不释放旧表 |

**Requirements：** free_page 模拟器拒绝非零引用释放和重复释放；明确这些故障由模拟 get_pte 注入，不声称真实分配器已经历内存耗尽。

### QEMU 验证

**Pre-Condition：** 工程已完成前置迁移及页操作；WSL 有交叉工具链和 QEMU。

**Post-Condition：** 构建成功、QEMU正常退出、输出最终成功标记且无panic，才记为通过。源文件哈希、构建日志、运行日志和结果JSON分别保存。

**Case 1：** pmm_init 返回，原 check_alloc_page、check_pgdir、check_boot_pgdir 检查通过。

**Case 2：** 通过物理页直映地址写入已知数据，建立映射后从实际虚拟地址读出；两虚拟页指向同一物理页，通过一个地址写入，另一个读到变化。

**Case 3：** 同页重插保持引用数；对已访问地址替换物理页，实际读取必须变为新页数据，另一个共享地址仍指向旧页。

**Case 4：** 撤销映射后 get_page 为NULL，重复删除不增加空闲页；删除全部数据映射后仅剩测试拥有的两张中间表，清理后空闲页总数恢复测试前基线。

**Requirements：** 不通过访问已删除地址触发未接入处理器的故障；不据普通读写推定权限故障已验证。物理页及中间表计数按当前工程检查，不硬编码启动空闲页数。测试入口没有初始化中断/调度，不能声称这些模块运行通过。

### 已取得的实际结果

- 宿主：用户在 WSL 执行，8/8，编译和运行退出码均为0；提交0aa69b5保存脚本和日志。
- QEMU：原页表自检与新增4组检查通过，ISOLATED_QEMU: PASS；提交3192bda保存脚本、日志、结果和源码哈希。
- 已知警告：kmalloc.c:92 参数类型转换警告；此次构建成功，没有修改框架以消除警告。
- 独立测试阶段未覆盖：真实分配器内存耗尽注入、权限异常、并发修改、多核TLB、Trap运行链及全组评分。
- 后续集成阶段：本人在 9f98313 上执行评分与普通启动，记录见 report.md 第5.1节。该记录不表示所有 Trap 异常场景均已验证；最终源码与修复记录的一致性仍需集成复核。

## 功能模块二——进程创建及初始化

**负责人：2413921 程子涵。**

### 01　初始化进程控制块

[PROMPT]:

**任务**: 在 `kern/process/proc.c` 的 `//LAB4 function2 2413921` 区域完善 `alloc_proc()`。PCB 由内核堆分配，随后可能被用作启动线程，也可能在普通线程创建失败时直接回收。需要为每个字段建立明确初值，并给两个侵入式链表节点建立尚未挂接时的自环状态。

**操作要求**: 必须修改项目中的真实文件。允许修改 `alloc_proc()` 函数体并补全该模块的学号标记。保留原有注释、头文件包含与函数签名。禁止改动 `proc.h`、分配器、链表库、其他进程函数及官方评分文件。

**输出要求**: 写入完整实现后，说明修改位置，逐组列出 PCB 初值，并给出分配成功与失败各自的验证结果或复测方法。

[RELY]

- `kern/process/proc.h` 的 `struct proc_struct` 含 `state`、`pid`、`runs`、`kstack`、`need_resched`、`parent`、`mm`、`context`、`tf`、`pgdir`、`flags`、`name[PROC_NAME_LEN + 1]`、`list_link` 和 `hash_link`。`PROC_NAME_LEN = 15`。
- `kern/mm/kmalloc.h` 提供 `void *kmalloc(size_t n)`，返回的存储必须由调用方初始化。`libs/string.h` 提供 `memset()`。`libs/list.h` 的 `list_init()` 将节点的前后指针指向节点本身。
- `kern/mm/pmm.h` 声明 `extern uintptr_t boot_pgdir_pa`，保存内核根页表物理字节地址。

[GUARANTEE]

~~~c
static struct proc_struct *alloc_proc(void);
~~~

成功返回 PCB 指针，失败返回 `NULL`。本函数只分配 PCB，内核栈、PID 和全局登记由后续步骤处理。

[SPECIFICATION]

- **Case 1：内存分配失败。** `kmalloc(sizeof(struct proc_struct))` 返回 `NULL` 时直接返回，进程链表、哈希和进程计数保持原值。
- **Case 2：从非零内存分配。** 设 `state=PROC_UNINIT`、`pid=-1`、`pgdir=boot_pgdir_pa`。将 `runs`、`kstack`、`need_resched`、`flags`、整个 `context` 和 `name` 清零，`parent`、`mm`、`tf` 设为 `NULL`。
- **Case 3：登记前检查节点。** 两个节点各自初始化为自环。此时对节点调用 `list_empty()` 应为真，总进程链表与哈希桶未新增成员。

### 02　保留内核线程的地址空间接口

[PROMPT]:

**任务**: 在 `kern/process/proc.c` 的 `LAB4 function2` 区域完善 `copy_mm()`。本阶段的父子线程都在内核态运行并共享内核映射，PCB 中的 `mm` 字段预留给后续的用户地址空间。该函数保留统一创建接口，明确当前阶段的前提。

**操作要求**: 必须修改项目中的真实文件。允许修改 `copy_mm()` 函数体。保留函数签名和原有注释。禁止修改 `do_fork()`、`kernel_thread()`、虚拟内存子系统、页表接口和官方测试。

**输出要求**: 写入实现后，说明 `mm` 与 `pgdir` 各自表示什么，指出内核线程调用成功的条件，并说明接口如何反馈前提被破坏。

[RELY]

- `kern/process/proc.h` 中 `current` 指向当前线程，`proc_struct.mm` 是 `struct mm_struct *`，`proc_struct.pgdir` 是 `uintptr_t`。
- `alloc_proc()` 已将子 PCB 的 `pgdir` 设为 `boot_pgdir_pa`。本模块的 `kernel_thread()` 通过 `do_fork()` 传入带 `CLONE_VM` 的标志。
- `CLONE_VM` 定义于 `kern/mm/pmm.h`。内核断言接口在 `kern/debug/assert.h`。

[GUARANTEE]

~~~c
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
~~~

在当前内核线程契约下返回 0，保留 `clone_flags` 形参以及子线程的内核页表基址。

[SPECIFICATION]

- **Case 1：当前线程 `mm == NULL`。** 用断言明确前提，将 `proc->mm` 置为 `NULL`，返回 0。该步骤不申请页表或用户内存。
- **Case 2：传入带 `CLONE_VM` 或其他标志。** 当前实验的内核线程行为一致，保持子线程已有的 `pgdir`。
- **Case 3：当前线程的 `mm` 非空。** 按本实验契约触发断言。用户页表复制、引用计数、写时复制留给后续实验。

### 03　构造新线程的栈和首次恢复现场

[PROMPT]:

**任务**: 在 `kern/process/proc.c` 的 `LAB4 function2` 区域完善 `copy_thread()`。子线程此前没有被调度过，需要先在自己的内核栈中放好完整的中断返回帧，再用较小的 `context` 把第一次内核切换接到 `forkret()`。

**操作要求**: 必须修改项目中的真实文件。允许修改 `copy_thread()` 函数体。保留函数签名及注释。禁止修改 `struct context`、`struct trapframe`、`switch.S`、`trapentry.S`、已有 `forkret()` 和评分文件。

**输出要求**: 写入实现后，输出 `kstack`、帧基址、`context.sp` 和 `gpr.sp` 的对应关系，分别说明参数为 0 和非 0 时可观察到的字段值。

[RELY]

- `kern/mm/memlayout.h` 定义 `KSTACKPAGE = 2` 和 `KSTACKSIZE = KSTACKPAGE * PGSIZE`。`proc->kstack` 由 `setup_kstack()` 设为子线程栈的内核虚拟低地址。
- `kern/trap/trap.h` 的 `struct trapframe` 包含 `gpr`、`status`、`epc`、`badvaddr`、`cause`，`gpr` 中有 `a0` 与 `sp`。本包 RV64 布局对应 36 个 8 字节槽。
- `kern/process/proc.h` 的 `struct context` 包含 `ra`、`sp` 和 `s0–s11`。现有 `forkret()` 调 `forkrets(current->tf)`，恢复宏读取帧保存的寄存器。

[GUARANTEE]

~~~c
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
~~~

调用方已分配有效内核栈并传入有效模板帧。子线程独立持有帧副本，初始 `context.ra` 指向 `forkret`，`context.sp` 指向帧基址。

[SPECIFICATION]

- **Case 1：`esp == 0`。** 从 `kstack + KSTACKSIZE` 向下预留 `sizeof(struct trapframe)`，复制模板。设置子帧 `a0=0`、`sp=帧基址`。
- **Case 2：`esp != 0`。** 子帧 `gpr.sp` 取 `esp`，`context.sp` 仍指向帧基址，`context.ra` 和子帧 `a0` 与 Case 1 相同。此 Case 验证接口字段约定，当前实验的内核线程入口实际传 0。
- **Case 3：检查帧独立性和对齐。** 源模板不被改写，模板离开调用者栈后子帧仍有效。帧落在已分配的栈区内，RV64 初始栈保持 16 字节对齐。除明确覆盖的 `a0` 和 `sp` 外，保留模板寄存器与控制字段。

### 04　实现创建、登记与失败回收

[PROMPT]:

**任务**: 在 `kern/process/proc.c` 的 `LAB4 function2` 区域完善 `do_fork()`。按依赖顺序申请 PCB、内核栈并构造现场。只有资源准备完毕后才允许新线程进入全局管理结构，确保调度器读到的线程已经具备运行所需字段。

**操作要求**: 必须修改项目中的真实文件。只允许修改 `do_fork()` 函数体。保留签名、已有注释和工具注入锚点。禁止重写 `get_pid()`、`hash_proc()`、`wakeup_proc()`、分配器、调度器、结构体及评分规则。临时失败注入仅用于隔离测试副本。

**输出要求**: 写入实现后，列出资源获取顺序、临界区覆盖的步骤、每条错误路径的返回值和回收内容。注明实测覆盖范围。

[RELY]

~~~c
static struct proc_struct *alloc_proc(void);
static int setup_kstack(struct proc_struct *proc);
static void put_kstack(struct proc_struct *proc);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
static int get_pid(void);
static void hash_proc(struct proc_struct *proc);
void wakeup_proc(struct proc_struct *proc);
void kfree(void *objp);
~~~

- 前七个静态函数均在 `kern/process/proc.c`。`wakeup_proc()` 位于 `kern/schedule/sched.c`，要求进程尚未处于 RUNNABLE 或 ZOMBIE，成功后置 `PROC_RUNNABLE`。`kfree()` 声明在 `kern/mm/kmalloc.h`。
- `nr_process` 是本文件静态计数，包含 idle。上限 `MAX_PROCESS=4096` 定义于 `proc.h`。`-E_NO_MEM` 和 `-E_NO_FREE_PROC` 对应 `libs/error.h` 中的错误码。
- `libs/list.h` 的 `list_add()` 将节点插到给定节点之后。`kern/sync/sync.h` 提供 `local_intr_save(flag)` 与 `local_intr_restore(flag)`，用于本实验的单核中断保护。

[GUARANTEE]

~~~c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
~~~

成功返回正 PID。失败返回负错误码，调用所取得的资源按相反顺序回收。PID、哈希、进程链表、计数和就绪状态在同一临界区内完成发布。

[SPECIFICATION]

- **Case 1：正常创建。** 设置 `parent=current`，分配两页内核栈，完成 `copy_mm()` 和 `copy_thread()`。保存原中断状态、复查上限、分配 PID、插入哈希和进程链表、计数加一、调用 `wakeup_proc()`，最后恢复原中断状态并返回 PID。返回后 `find_proc(pid)` 应指向新 PCB。
- **Case 2：初检已满。** 返回 `-E_NO_FREE_PROC`，不调用分配器。
- **Case 3：PCB 分配失败。** 返回 `-E_NO_MEM`，无资源需要回收。
- **Case 4：栈分配失败。** 保留 `setup_kstack()` 的错误码，仅 `kfree(proc)`。
- **Case 5：`copy_mm()` 返回错误。** 保留其错误码，先 `put_kstack(proc)` 再 `kfree(proc)`。本实验正常的 `copy_mm()` 返回 0，此 Case 可在测试副本模拟。
- **Case 6：登记前复查达到上限。** 恢复原中断状态，先释放栈再释放 PCB，返回 `-E_NO_FREE_PROC`。所有失败 Case 都不产生新 PID、链表项、哈希项或计数增长。
- **Case 7：验证中断恢复。** 入口开中断则退出临界区后恢复为开，入口关中断则仍为关。

### 05　将普通 C 函数包装为内核线程

[PROMPT]:

**任务**: 在 `kern/process/proc.c` 的 `LAB4 function2` 区域完善 `kernel_thread()`。按照启动汇编的寄存器约定，把普通 C 函数和实参编码到临时 `trapframe`，再交给 `do_fork()` 生成可调度的内核线程。

**操作要求**: 必须修改项目中的真实文件。只允许修改 `kernel_thread()` 函数体。保留签名和原有注释。禁止修改 `do_fork()`、汇编入口、CSR 定义、结构体和评分脚本。

**输出要求**: 写入实现后，列出模板帧各字段、传给 `do_fork()` 的三个参数及返回值规则，解释帧内 `SIE` 与当前硬件 `sstatus.SIE` 的区别。

[RELY]

- `kern/process/entry.S` 的 `kernel_thread_entry` 执行 `move a0, s1`、`jalr s0`，线程函数返回后调用已有 `do_exit`。
- `kern/trap/trap.h` 定义 `struct trapframe`。恢复汇编在 `kern/trap/trapentry.S`，把帧中的 `status`、`epc` 写入 CSR，再执行 `sret`。
- `libs/riscv.h` 定义 `read_csr(sstatus)`、`SSTATUS_SPP=0x100`、`SSTATUS_SPIE=0x20`、`SSTATUS_SIE=0x2`。`kern/mm/pmm.h` 定义 `CLONE_VM`。
- `proc.h` 声明 `int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)`。

[GUARANTEE]

~~~c
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
~~~

将 `fn` 与 `arg` 放入启动约定的寄存器，保留调用方传入的标志，并原样返回 `do_fork()` 的结果。

[SPECIFICATION]

- **Case 1：传入有效线程函数。** 清零整个局部帧，将 `s0` 设为 `(uintptr_t)fn`、`s1` 设为 `(uintptr_t)arg`、`epc` 设为 `(uintptr_t)kernel_thread_entry`。`arg == NULL` 时应能原样传递。
- **Case 2：当前 SIE 开或关。** 都令模板帧的 `SPP=1`、`SPIE=1`、`SIE=0`，其余 `status` 位取自当前 CSR。构造模板时只读当前 CSR。首次恢复后 `sret` 返回 S 态并恢复 SIE。
- **Case 3：向 `do_fork()` 传递请求。** 调用 `do_fork(clone_flags | CLONE_VM, 0, &tf)`。正常返回子 PID，失败原样传回负错误码。模板帧由创建过程同步复制。

### 06　初始化 idleproc 与 initproc

[PROMPT]:

**任务**: 在 `kern/process/proc.c` 的 `LAB4 function2` 区域完善 `proc_init()`。启动期间已经有一条执行流与启动栈，先把它登记为调度器使用的 idle，再通过普通创建路径得到可查询的 init。同步检查本文件的 `set_proc_name()`，保证对短名字的读写有界。

**操作要求**: 必须修改项目中的真实文件。允许修改 `proc_init()` 和同文件内与命名边界直接相关的 `set_proc_name()`。保留函数签名、文件注释、`init_main()` 及评分注入入口。禁止改动 `find_proc()`、`get_pid()`、其他成员的代码、启动汇编及官方测试。

**输出要求**: 写入实现后，列出 idle 与 init 的 PID、名称、父线程、栈、状态和登记方式，说明命名函数的长度边界。给出 `make qemu`、`make grade` 和 GDB 的复测命令。

[RELY]

- `proc.c` 提供 `proc_list`、`hash_list[HASH_LIST_SIZE]`、`nr_process`、`alloc_proc()`、`init_main(void *arg)`、`find_proc(int pid)`。哈希桶数为 `1 << HASH_SHIFT`，本包 `HASH_SHIFT=10`。`find_proc()` 只接受 `0 < pid < MAX_PID`。
- `proc.h` 声明 `idleproc`、`initproc`、`current` 和 `kernel_thread()`。`pmm.h` 声明 `extern char bootstack[], bootstacktop[]`。
- `libs/string.h` 提供 `strnlen()`、`memmove()` 和 `memset()`。`proc_struct.name` 容量为 `PROC_NAME_LEN + 1`，最多存 15 个名字字节。
- `kern/init/init.c` 在 PMM、陷入入口与 VMM 初始化后调用 `proc_init()`，随后才初始化时钟、开中断并进入 `cpu_idle()`。评分器在 `proc_init()` 末尾注入 `test_lab4_process_create()`。

[GUARANTEE]

~~~c
void proc_init(void);
char *set_proc_name(struct proc_struct *proc, const char *name);
~~~

启动时完成一次初始化。命名函数返回 `proc->name`，保证以空字符终止。

[SPECIFICATION]

- **Case 1：建立全局结构和 idle。** 对总链表及每个哈希桶调用 `list_init()`。分配 idle PCB，设置 PID 0、`PROC_RUNNABLE`、`kstack=(uintptr_t)bootstack`、`need_resched=1` 和名称 `idle`。把 idle 计入总数并令 `current=idleproc`。idle 由调度器直接引用，两个节点保持自环。
- **Case 2：创建 init。** 调用 `kernel_thread(init_main, "Hello world!!", 0)`。以返回 PID 查找 PCB，验证指针和 PID 后命名为 `init`。完成时 `current` 仍为 idle，init 的 `parent` 指向 idle，init 在普通链表和哈希中可见且为 RUNNABLE。
- **Case 3：启动失败。** idle 分配失败、创建 init 得到非正 PID 或查不到合法的 PID 1 PCB 时立即报错或断言。对 init 命名前必须先通过查找结果检查。
- **Case 4：命名边界。** 短字符串只复制有效字节，超长名字最多复制 15 字节，尾部始终有终止符。以短名字覆盖长名字时清除旧尾巴，输入与 PCB 名称缓冲区重叠时使用支持重叠的复制接口。

### 07　Challenge 1：推导 PID 回绕与查重

[PROMPT]:

**任务**: 阅读 `kern/process/proc.c` 的 `get_pid()`、`do_fork()`、`hash_proc()` 及 `find_proc()`，把回绕和查重的源码推导补到 `report/report.md` 的「课堂拓展」第 1 小节。说明扫描为什么要同时维护候选值和安全区间边界。

**操作要求**: 必须修改真实的 `report/report.md`，只更新这一小节。代码作为只读依据，禁止修改分配器、查找接口及评分文件。

**输出要求**: 写出可逐步核对的状态表，包括扫描顺序、候选号、`next_safe` 变化、是否重扫以及最终 PID。

[RELY]

- `get_pid(void)` 是 `proc.c` 内部静态函数。其静态变量 `last_pid`、`next_safe` 初值均为 `MAX_PID`。扫描遍历 `proc_list`，借 `le2proc(le, list_link)` 还原 PCB。
- `proc.h` 定义 `MAX_PROCESS=4096`、`MAX_PID=8192`。idle 使用 PID 0。分配器的有效正 PID 为 1 到 8191。
- `do_fork()` 在一个关中断临界区内调用 `get_pid()` 并登记。哈希用于 `find_proc(int pid)` 查询，分配时的查重依据总进程链表。

[GUARANTEE]

本题交付源码分析，无需实现或修改 C 接口。解释 `get_pid(void)` 与 `find_proc(int pid)` 各自的职责。

[SPECIFICATION]

- **Case 1：启动首次分配。** 推导静态初值与空链表如何产生 PID 1。
- **Case 2：候选值仍在安全区间。** 举例说明何时递增后可直接返回，何时达到 `next_safe` 后必须重扫。
- **Case 3：从 8191 回绕。** 链表顺序为 2、1、5 时，说明为何扫过 2 后遇到 1 会触发重扫，最终为何返回 3。
- **Case 4：已有线程被移除。** 说明缓存边界可能变得保守，下一次触边界重扫可重新识别空闲编号。

### 08　Challenge 2：区分数字唯一性和对象身份

[PROMPT]:

**任务**: 在 `report/report.md` 的「课堂拓展」第 2 小节说明，为什么 `get_pid()` 对现存编号查重成功后，旧的 PID 引用仍有可能命中新创建的进程。用 A 和 B 的生命周期推演 ABA 场景，并给出两种能绑定具体进程实例的设计。

**操作要求**: 必须修改真实的 `report/report.md`，只更新这一小节。允许读取本包 `proc.c`，禁止实现新 PID 机制、退出回收或改变实验现有接口。

**输出要求**: 给出时间线以及整数 PID、带代次标识和稳定句柄的比较。明确本实验可观测到的行为及需要依赖真实进程回收才能发生的情况。

[RELY]

- `kern/process/proc.c` 的 `find_proc(int pid)` 以正整数 PID 选哈希桶，再比较 PCB 的 `pid`。`proc_struct` 未提供代次字段。
- 本包 `do_exit(int error_code)` 仍以 `panic("process exit!!.\n")` 作为占位。因此退出后回收 PID 的例子属于机制推演。

[GUARANTEE]

本题交付报告推演，无需新增 C 接口。答案应区分「同一时刻的编号唯一」与「跨时间引用到同一对象」。

[SPECIFICATION]

- **Case 1：A 仍登记。** 同一个数字只能对应 A，新的分配会跳过该编号。
- **Case 2：A 回收且数字暂未分给 B。** 按数字查找应找不到目标。
- **Case 3：数字随后分给 B。** 旧整数查找会得到 B，系统在每一时刻仍满足现存 PID 唯一。解释这如何构成 ABA。
- **Case 4：旧引用带代次或持有稳定句柄。** 带代次标识在匹配时同时验证编号和代次，需考虑代次回绕。稳定句柄绑定实例对象，需管理引用计数或对象生命周期。

### 09　Challenge 3：核实课堂指定项目的 PID 复用机制

[PROMPT]:

**任务**: 根据分工说明中的课堂纪要核实 `newcall` 对应的真实项目，查到可固定的源码版本后，在 `report/report.md` 的「课堂拓展」第 3 小节说明该项目如何防止旧引用误命中新进程。追踪编号分配、退出或解绑、旧引用解析三条路径。

**操作要求**: 必须修改真实的 `report/report.md`，只更新这一小节。可以检索课件或仓库，阅读固定版本的源码。禁止改动任何实验代码、第三方仓库或官方测试。项目名、函数名、版本和源码证据必须可核对。

**输出要求**: 给出项目名、仓库链接、版本标签或提交号，以及「文件路径、函数、源码行为」的对应表。写明编号可以何时复用、旧引用绑定什么、旧对象退出后的行为。课堂项目无法确认时，记录所缺依据，并单列带版本的对照案例。

[RELY]

- `Lab4_三人任务分配方案.md` 只提供 `newcall` 拼写，未提供课堂指定仓库及版本。
- 已核对的对照源码为 Linux `v6.12`。`include/linux/pid.h` 有 `struct pid` 和 `get_pid()`。`kernel/pid.c` 有 `alloc_pid()`、`free_pid()` 和 `pidfd_open` 系统调用。`fs/pidfs.c` 有 `pidfs_alloc_file()` 和 `pidfd_pid()`。`kernel/signal.c` 有 `pidfd_send_signal` 系统调用与 `kill_pid_info_type()`。路径均相对于 Linux 仓库根目录。

[GUARANTEE]

本题交付源码证据与报告结论，无需在 uCore 新增接口。若使用 Linux 对照，应覆盖 `pidfd_open`、`pidfd_send_signal` 及背后的对象生命周期。

[SPECIFICATION]

- **Case 1：能核实课堂项目。** 固定版本后追踪分配、退出、旧引用查询。每个判断附上具体文件、函数及固定版本源码链接。
- **Case 2：资料不足以识别课堂项目。** 明确缺少可唯一定位的课件、仓库链接或源码版本，并把 Linux `v6.12` 清楚标为对照。
- **Case 3：Linux 对照中 PID 被复用。** 解释 IDR 编号解绑后允许数字复用，旧 pidfd 仍引用原来的 `struct pid`。说明原实例退出并被回收后旧 pidfd 发信号返回 `ESRCH`。
- **Case 4：持有旧数字后才获取句柄。** 说明复用之后才 `pidfd_open` 可能取得新进程句柄，以及创建时通过 `CLONE_PIDFD` 获取句柄解决的是哪个窗口。

---

<a name="module-three"></a>

## 功能模块三——进程调度

**负责人：2411824 于蕾。**

### 任务一：实现 RV64 线程上下文切换 switch_to

[PROMPT]:

**任务**: 在 `code/kern/process/switch.S` 的 `//LAB4 function3 2411824` 模块实现或完善 `switch_to`。线程在普通 C 函数调用处切换时，先把旧线程的返回地址、栈指针和被调用者保存寄存器写入旧 `context`，再从目标 `context` 恢复同一组寄存器，最后通过恢复后的 `ra` 转入目标执行流。已有线程恢复上次暂停的位置；首次运行的线程通过成员二预设的 `context.ra` 和 `context.sp` 进入 `forkret`。

**操作要求**: 你必须修改项目中的真实文件，而不是仅展示代码。只允许修改 `code/kern/process/switch.S` 内 `switch_to` 的实现，保留现有注释、头文件包含、`.text`、`.globl switch_to` 和学号标记。禁止修改 `proc.h`、`proc.c`、`entry.S`、`trapentry.S`、`riscv.h`、构建规则及官方测试；禁止改变 `struct context` 布局或增加跨文件接口。已有指令满足要求时保留，不能为了形成差异改写正确代码。

**输出要求**: 将完整实现直接写入目标文件，说明实际修改位置，列出保存、恢复的寄存器及偏移，说明 RV64 加载和存储宽度。给出实际编译结果；运行验证依赖其他模块时明确说明依赖，未执行的项目写“未验证”。不得凭静态阅读声称上下文往返测试已经通过。

[RELY]

`code/kern/process/proc.h` 已定义：

```c
struct context {
    uintptr_t ra;
    uintptr_t sp;
    uintptr_t s0;
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;
};
```

- 当前工程为 RV64，`uintptr_t` 占 8 字节，结构体共 14 个连续字段、112 字节。`ra`、`sp`、`s0` 到 `s11` 的偏移分别是 0、8、16 到 104 字节。
- `code/libs/riscv.h` 在 `__riscv_xlen == 64` 时定义 `STORE=sd`、`LOAD=ld`、`REGBYTES=8`，目标汇编已有该头文件包含。
- `code/kern/process/proc.c` 已声明 `void switch_to(struct context *from, struct context *to);`，调用时 `a0` 指向旧线程的 `context`，`a1` 指向目标线程的 `context`。
- `copy_thread()` 已令新线程的 `context.ra=(uintptr_t)forkret`、`context.sp=(uintptr_t)proc->tf`。已有首次运行链是 `forkret → forkrets → __trapret → kernel_thread_entry → init_main`。
- 调用者负责选择线程、关中断、更新 `current`、切换页表及处理 TLB；这些职责不属于本汇编函数。

[GUARANTEE]

必须提供且只保留一个同名全局汇编入口，对应接口为：

```c
void switch_to(struct context *from, struct context *to);
```

完整保存、恢复 `ra`、`sp`、`s0`～`s11` 共 14 个寄存器。允许使用文件已有的 `STORE`、`LOAD` 和 `REGBYTES` 宏，不新增公共接口或独立的上下文格式。

[SPECIFICATION]

#### switch_to

**Pre-Condition**:

- `from`、`to` 指向有效、对齐的 `struct context`，位于切换前后均可访问的内核映射中。
- 目标栈有效；新线程初始栈符合 RV64 的 16 字节对齐约定，已有线程的栈来自此前保存的现场。
- 调用者已关闭中断并准备好目标地址空间；目标线程具备有效的运行现场。

**Post-Condition**:

- 旧线程在调用点需要保留的 14 个寄存器完整写入 `*from`；目标 14 个寄存器恢复到 CPU，执行转向目标 `ra`。
- 旧线程以后被切回时，原 `switch_to()` 调用才会返回，并继续执行旧线程调用点之后的代码。

**Case 1：切换到曾运行的线程。** 两个不同线程各自有独立栈和有效现场。先保存旧现场，再恢复目标现场并 `ret`；目标的 `sp`、返回位置和全部 `s` 寄存器与上次保存值一致。

**Case 2：首次运行新线程。** 目标 `ra` 指向 `forkret`、`sp` 指向预先构造的 trapframe。`ret` 后进入现有恢复链，使用目标线程的帧启动其入口，不能返回旧线程调用点或直接跳到 `init_main`。

**Case 3：64 位值与现场往返。** 在隔离调试环境中让被保存寄存器含非零高 32 位，并执行 A→B→A。恢复后各值完整保留，旧线程恢复自己的栈和返回地址；不得使用 `sw/lw` 截断数据。仅编译通过不足以判定此 Case 通过。

**Requirements**:

- 全部旧寄存器保存完成后再恢复目标寄存器，尤其不能提前覆盖旧 `sp` 或 `ra`。
- 基址 `a0/a1` 在各自访问结束前保持有效；不得通过额外压栈改变要保存的调用现场。
- 本函数保存的是 C 调用边界的 `context`；不复制 trapframe，不恢复 `sstatus/sepc`，不负责保存全部通用寄存器。
- 不修改 `current`、线程状态、`runs`、`need_resched`、SATP 或中断开关，不输出调试字符串。

### 任务二：实现安全的线程运行切换 proc_run

[PROMPT]:

**任务**: 在 `code/kern/process/proc.c` 的 `//LAB4 function3 2411824` 区域完善 `proc_run()`。该函数连接调度选择与底层汇编：在中断保护下记录旧线程、更新 `current`，加载目标根页表物理地址并刷新 TLB，然后调用 `switch_to()`。切换过程中不能让中断处理看到 `current`、页表和执行现场处于不一致状态。

**操作要求**: 你必须修改项目中的真实文件，而不是仅展示代码。只允许修改 `proc_run()` 函数体，保留签名、已有注释、包含和学号标记。禁止修改本文件的进程创建函数、`cpu_idle()`、`schedule()`、`switch_to()`、`sync.h`、`intr.c`、`riscv.h`、`pmm.h`、结构体及官方测试。当前 `lsatp()` 已采用正确的 RV64/Sv39 编码，不得重复修补该头文件；在本函数调用已有 TLB 刷新接口。

**输出要求**: 直接写入完整实现，说明切换步骤、页表参数的地址单位和刷新位置，解释中断保护嵌套时如何恢复。给出实际验证结果或复测方法；区分“切换时目标开始运行”与“旧线程恢复后本次调用返回”。

[RELY]

```c
// code/kern/process/proc.h
extern struct proc_struct *idleproc, *initproc, *current;
void proc_run(struct proc_struct *proc);

// code/kern/process/proc.c 中已有声明
void switch_to(struct context *from, struct context *to);

// code/libs/riscv.h
static inline void lsatp(unsigned long pgdir);

// code/kern/mm/pmm.h
static inline void flush_tlb();
```

- `proc_struct` 含 `struct context context` 和 `uintptr_t pgdir`。成员二的 `alloc_proc()` 将 `pgdir` 初始化为 `boot_pgdir_pa`，保存的是页对齐的根页表**物理字节地址**，不是内核虚拟地址、PPN 或已经编码的 SATP 值。
- 当前 `lsatp()` 写入 `((unsigned long)SATP_MODE_SV39 << 60) | (pgdir >> RISCV_PGSHIFT)`，不包含 TLB 刷新；`flush_tlb()` 执行 `sfence.vma`。
- `code/kern/sync/sync.h` 的 `local_intr_save(flag)` 把入口 `sstatus.SIE` 是否为 1 保存到 `bool` 并在需要时关闭中断；`local_intr_restore(flag)` 仅在 `flag` 为真时开中断。
- `code/kern/driver/intr.c` 的 `intr_enable()` 与 `intr_disable()` 分别置位、清除 `SSTATUS_SIE`。`flag` 不保存整个 `sstatus`。
- 当前内核线程共享内核映射；目标页表必须映射切换代码、PCB 和切换前后用到的栈。`schedule()` 本身也关中断，因此本函数必须允许嵌套保护。
- 当前 `proc_run()` 已调用 `lsatp()` 和 `switch_to()`，但没有显式调用 `flush_tlb()`。

[GUARANTEE]

必须实现：

```c
void proc_run(struct proc_struct *proc);
```

使用已有 `local_intr_save/local_intr_restore`、`lsatp`、`flush_tlb` 和 `switch_to` 完成切换，不新增接口。目标就是当前线程时直接返回；其他情况下严格按“保存旧指针→更新 current→加载 SATP→刷新 TLB→切换 context”的顺序执行，状态更新及切换位于中断保护内。

[SPECIFICATION]

#### proc_run

**Pre-Condition**:

- `current`、`proc` 为有效 PCB；目标由调度器选为可运行线程，或为有效的 idle。
- 目标 `pgdir` 与 `context` 已初始化，相关代码和数据映射有效。本函数不承担空指针容错、资源创建或调度策略选择。

**Post-Condition**:

- 异线程切换时，目标线程以自己的上下文执行，`current` 指向目标，SATP 使用目标根页表，并已处理旧 TLB 翻译。
- 旧线程以后恢复并退出本次调用时，按旧线程此次调用保存的标志恢复中断使能状态。

**Case 1：`proc == current`。** 不写 SATP、不刷新 TLB、不调用 `switch_to()`，不改变中断状态和 PCB 字段。

**Case 2：从 idle 首次切到 init。** 保存 `prev=idleproc`，在关中断状态令 `current=initproc`，调用 `lsatp(initproc->pgdir)`、`flush_tlb()`，再调用 `switch_to(&prev->context, &initproc->context)`，目标沿现有首次恢复链执行。

**Case 3：不同线程共享同一页表。** 即使两者 `pgdir` 相同，也保留加载页表、刷新 TLB和切换现场的统一路径；不以共享地址空间为理由跳过线程切换。

**Case 4：入口 SIE=1。** 保存标志为真，切换前保持 SIE=0。旧线程恢复到本调用的后续位置后，通过对应恢复操作重新开中断。首次运行目标线程的中断状态由其 trapframe 与 `sret` 决定，不能立即把旧线程标志用于目标线程。

**Case 5：入口 SIE=0 或位于 schedule 的临界区。** 本层标志为假，本层恢复不打开中断；若外层原先开中断，由外层恢复自己的标志。若调用前就关中断，调用恢复后仍保持关闭。

**Requirements**:

- 每层临界区使用自己的局部 `bool intr_flag`，保存、恢复必须配对，不无条件调用 `intr_enable()`。
- 在更新 `current` 前保留旧 PCB 指针，向 `switch_to()` 传入两个 `context` 的地址，不能误传 PCB 或 trapframe 指针。
- 将物理字节地址直接传给 `lsatp()`，不重复右移，也不使用 RV32 的 SATP MODE 编码。
- 页表切换后、恢复目标现场前执行 TLB 刷新；不修改线程状态、运行计数、链表或重调度标志，这些由对应模块负责。

### 任务三：实现循环选择可运行线程的 schedule

[PROMPT]:

**任务**: 在 `code/kern/schedule/sched.c` 的 `//LAB4 function3 2411824` 区域实现或完善 `schedule()`。沿已有双向循环进程链表进行轮转选择：从当前普通线程的后继开始；当前为 idle 时从表头后继开始。跳过非 `PROC_RUNNABLE` 线程和哨兵，找不到候选时选择 idle。清除当前线程的重调度请求，增加被选线程的运行次数，必要时通过 `proc_run()` 切换。

**操作要求**: 你必须修改项目中的真实文件，而不是仅展示代码。只允许修改 `code/kern/schedule/sched.c` 的 `schedule()` 函数体，保留现有注释、头文件包含、函数签名及学号标记。禁止修改 `wakeup_proc()`、进程创建和切换函数、链表库、结构体、中断库、时钟和陷入逻辑、Makefile、评分脚本及官方测试。不增加优先级、时间片、独立就绪队列或多核调度机制。

**输出要求**: 写入完整实现，说明循环起点、结束条件、越过哨兵和回退 idle 的处理；逐项给出下面 Case 的结果或复测方法。说明选中当前线程时 `runs` 和 `proc_run()` 各自的行为，不能把官方基础评分当作所有边界 Case 均已覆盖。

[RELY]

```c
// code/kern/process/proc.h
extern list_entry_t proc_list;
extern struct proc_struct *idleproc, *initproc, *current;
void proc_run(struct proc_struct *proc);

// code/kern/schedule/sched.h
void schedule(void);
void wakeup_proc(struct proc_struct *proc);
```

- `proc_list` 是带哨兵的双向循环链表；`&proc_list` 不是 PCB 节点。`code/libs/list.h` 提供 `list_next()`；`le2proc(le, list_link)` 通过 `proc.h` 还原 PCB。
- 当前 `proc_init()` 不将 idle 挂入 `proc_list`；普通已登记线程通过 `list_link` 挂入该链表。idle 单独作为保底选择。
- `proc_struct` 的相关字段为 `enum proc_state state`、`int runs`、`volatile bool need_resched` 和 `list_entry_t list_link`。合法状态为 `PROC_UNINIT`、`PROC_SLEEPING`、`PROC_RUNNABLE`、`PROC_ZOMBIE`，当前正在执行的普通线程也可为 `PROC_RUNNABLE`。
- `local_intr_save/local_intr_restore` 定义于 `code/kern/sync/sync.h`，保存和恢复入口 SIE。`proc_run()` 内部也使用该保护，嵌套调用时内层不得提前开中断。
- `code/tools/test_lab4.c` 的 `test_lab4_schedule()` 创建两个测试线程，并通过主动调用 `schedule()` 验证它们都执行过。该测试不单独证明空链表、全部阻塞和 TLB 行为。
- 当前普通启动由 `cpu_idle()` 发起首次调度；评分线程也会主动调用 `schedule()`。本任务不要求修改时钟路径实现抢占。

[GUARANTEE]

必须实现：

```c
void schedule(void);
```

一次调用只完成一次选择。关中断后清除原 `current->need_resched`，在至多一轮链表扫描中找到首个符合条件的线程；无候选则使用 idle。对最终选择的线程执行 `runs++`；仅当它与 `current` 不同时调用 `proc_run()`，最后按本层保存标志恢复中断。

[SPECIFICATION]

#### schedule

**Pre-Condition**:

- `current`、`idleproc` 有效，idle 能运行，链表已经初始化。
- 当前为普通线程时，其 `list_link` 仍在 `proc_list` 中。链表无损坏，所扫描 PCB 生命周期有效；本实验使用单核中断保护。

**Post-Condition**:

- 原调用线程的 `need_resched` 被清零；被选线程的 `runs` 恰加一。
- 选择顺序符合循环链表顺序，无候选时选择 idle；链表结构及所有线程状态保持不变。
- 需要切换则调用 `proc_run()`；原调用线程以后恢复并退出本次 `schedule()` 时，按入口标志恢复中断。

**Case 1：idle 发起调度，链表含多个 runnable 线程。** 从 `list_next(&proc_list)` 开始，选中链表中的第一个 runnable 线程，不依赖 PID 大小或创建时间重新排序。

**Case 2：普通线程有 runnable 后继。** 当前 A 后面依次为 B、C，B runnable，则选 B；A 的标志清零、B 的 `runs` 加一并切换。

**Case 3：跳过非 runnable 线程。** A 后继依次是 UNINIT、SLEEPING、ZOMBIE、RUNNABLE，依次跳过前三个，选择最后一个；不得修改被跳过线程的状态或计数。

**Case 4：从链表尾回绕。** 当前普通线程位于尾部，越过哨兵后继续检查头部线程，选中第一个 runnable；不能把哨兵传给 `le2proc()`，也不能在遇到哨兵时提前结束整轮搜索。

**Case 5：只有当前普通线程 runnable。** 检查完整一轮后允许选回当前线程，`current->need_resched=0`、其 `runs` 加一，不调用 `proc_run()`，不进行实际上下文切换。

**Case 6：没有普通 runnable 线程。** idle 面对空链表，或链表内所有普通线程都不可运行时，选择 idle。当前为非 runnable 的普通线程且没有其他候选时同样回退 idle。只增加 idle 的 `runs`，当前已为 idle 时不调用 `proc_run()`。

**Case 7：中断恢复与嵌套。** 入口 SIE=1，退出本次调用时恢复为 1；入口 SIE=0 则保持为 0。选择、计数和 `proc_run()` 调用均处于本层保护内，内层恢复不能打开外层关闭的中断。

**Requirements**:

- idle 以哨兵为搜索起点；普通线程以自己的 `list_link` 为起点，先前进再检查。以返回起点作为扫描结束条件，普通线程自身也应在整轮末尾得到检查机会。
- 候选指针必须初始化；空表或扫描失败时不得解引用未初始化的指针，循环必须终止。
- `next->runs++` 位于是否切换的判断之前；选回当前线程和 idle 保底也计数。
- 不删除或重排节点，不分配内存，不自行调用 `switch_to()`，不增加额外状态，不无条件开启中断。

### 任务四：实现空闲线程循环 cpu_idle

[PROMPT]:

**任务**: 在 `code/kern/process/proc.c` 的 `//LAB4 function3 2411824` 区域实现或完善 `cpu_idle()`。它是启动线程完成初始化后的永久空闲循环。持续读取当前线程的 `need_resched`，有请求时调用 `schedule()`，从而把 CPU 交给已创建的内核线程；暂时没有请求时继续等待，未来再次观察标志。

**操作要求**: 你必须修改项目中的真实文件，而不是仅展示代码。只允许修改 `cpu_idle()` 函数体，保留签名、原有注释、包含和学号标记，保持头文件中已有的 `noreturn` 契约。禁止修改 `proc_run()`、`proc_init()`、`schedule()`、时钟和陷入处理、`init_main()`、`do_exit()`、结构体、构建和官方测试。不引入 `wfi`、主动开关中断、新的唤醒协议或每轮无条件调度。

**输出要求**: 将完整实现直接写入目标文件，说明循环何时调用 `schedule()`、为何不会返回，以及标志的读取和清除分别属于哪个函数。三模块具备后，给出实际构建、启动与评分结果；只记录真实执行的命令和输出，不预填成功日志。

[RELY]

```c
// code/kern/process/proc.h
void cpu_idle(void) __attribute__((noreturn));
extern struct proc_struct *current;

// code/kern/schedule/sched.h
void schedule(void);
```

- `proc_struct.need_resched` 的真实类型是 `volatile bool`，应在循环中反复读取，不能在循环外缓存为普通局部变量。
- 当前 `proc_init()` 将 `current=idleproc` 并设置 `idleproc->need_resched=1`；init 已经创建为 runnable 并在普通进程链表中登记。
- `code/kern/init/init.c` 在 `proc_init()` 后调用 `clock_init()`、`intr_enable()`，随后进入 `cpu_idle()`。
- `schedule()` 清除原当前线程的重调度标志并完成选择；`proc_run()` 和 `switch_to()` 完成切换。本函数不直接操作它们的内部字段。
- 普通启动的预期输出包含 `this initproc, pid = 1, name = "init"`、`Hello world!!` 和告别消息。当前 `init_main()` 返回后，`kernel_thread_entry` 调用已有 `do_exit()`，后者仍以 `panic("process exit!!.\n")` 占位。
- 构建入口在 `code/Makefile`，命令应从 `code/` 执行。`make grade` 使用评分副本，`code/tools/inject.py` 在副本中注入原版测试调用，不应为评分把这些调用手动加到正式源码中。

[GUARANTEE]

必须实现：

```c
void cpu_idle(void);
```

保持无限循环，仅当本轮读到 `current->need_resched` 非零时调用 `schedule()`。不清除标志、不自行选择线程、不直接切换页表或寄存器，不从函数返回。

[SPECIFICATION]

#### cpu_idle

**Pre-Condition**:

- 已完成内存、陷入、进程和时钟初始化，当前 PCB 有效，普通启动时为 idle。
- 其他三个调度函数及成员一、二模块已具备；普通启动由调用者在进入本函数前开启中断。

**Post-Condition**:

- 本函数不返回。标志为真时进入调度路径；被切回 idle 并从 `schedule()` 返回后继续检查。
- 无请求时继续空闲循环，不修改 PCB、链表、页表和中断状态。

**Case 1：首次进入 idle，标志为 1。** 调用 `schedule()`，由调度器选择 runnable 的 init，再通过 `proc_run → switch_to → forkret → forkrets → __trapret → kernel_thread_entry → init_main` 首次运行。应观察实际 init 输出，不能只凭非空 `context.ra` 判断成功。

**Case 2：标志为 0。** 当前循环不调用 `schedule()`，继续读取标志；不主动返回、睡眠、关机或修改中断开关。

**Case 3：标志从 0 变为 1。** 在隔离调试环境中改变标志，后续循环应观察到变化并调用调度器；不得使用循环外缓存值。该 Case 不要求修改正式时钟路径。

**Case 4：调度器仍选择 idle。** 没有普通 runnable 线程，`schedule()` 清除标志后返回；继续循环，不能递归调用 `cpu_idle()`，也不能因没有候选而直接退出。

**Case 5：恢复到 idle。** 在可返回的隔离线程场景中，其他线程以后切回 idle，先恢复原有调度调用栈，随后继续循环。本包普通 init 会进入 `do_exit()` 的 panic 占位，不能将普通启动描述为已验证此往返场景，也不能为此擅自实现退出回收。

**Requirements**:

- 每轮直接检查 `current->need_resched`，不无条件调用 `schedule()`，不在调用前抢先清零请求。
- 保留已有分层职责；不在本函数内维护 `runs`、重写 `current` 或强制唤醒线程。
- 普通启动与评分分开记录。完整验证可从仓库根目录执行：

```sh
cd code
make clean
make
make grade
make qemu
```

- 评分应核对 `[Test PMM] PASS`、`[Test Process Create] PASS`、`[Test Schedule] PASS` 和实际总分；预期总分为 30/30，必须以真实结果为准。普通启动应核对 init 的 PID、名称和消息，之后出现现有 `process exit!!` 占位需如实说明；缺失消息或提前异常应定位，不能改判分规则或伪造输出。
- 上述命令用于代码实现后的验证；本提示词文档的生成本身不代表已运行构建、评分或 QEMU。遇到其他成员模块问题，只报告具体位置及证据，修复仍须遵守本任务修改边界。
