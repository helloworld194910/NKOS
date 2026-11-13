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
