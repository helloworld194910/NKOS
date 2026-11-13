# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
| --- | --- |
| 实验名称 | Lab4：进程管理 |
| 当前撰写人 | 2412790-杨茜茹 |
| 小组成员 | 待汇总负责人填写并核对本次分工对应姓名 |
| 完成日期 | 尚未完成全组集成，不填最终日期 |

### 小组分工

杨茜茹负责 Lab2/Lab3 前置迁移、虚拟内存页操作、相关测试及分页拓展。进程创建负责人填写功能模块二与 PID 拓展；调度负责人填写功能模块三、中断保护拓展，并统一组织联调和最终报告汇总。以下只记录杨茜茹已完成的工作，不代写队友的实现过程。

## 一、实验目的

1. 理解虚拟内存布局与页表映射的关系。
2. 掌握内核线程创建、首次运行及调度切换过程。
3. 借助 AI 实现接口，通过代码检查和实际运行验证结果。

## 二、实验环境

杨茜茹使用 Windows 上的 VS Code、WSL Ubuntu 和 Codex 桌面应用。当前 Lab4 模型版本待本人核对，不根据上次实验记录推定。

实际使用 python3、make、宿主 gcc、riscv64-unknown-elf-gcc 和 qemu-system-riscv64。交叉工具链位于 riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14 目录；精确版本号以最终环境采集为准。QEMU 日志中的 OpenSBI v0.4 是固件版本，不是 QEMU 版本。

队友填写各自实际环境和 AI 工具。

## 三、实验整体逻辑分析

### 3.1 本章节的逻辑主线

先恢复物理内存分配和 Trap 基础，再维护页映射，之后创建内核线程并通过调度器切换执行。本实验的内核线程共享内核地址空间，不能把创建线程等同于创建独立用户地址空间。

### 3.2 功能的逐步实现

`kern_init → pmm_init → pic_init/idt_init → vmm_init → proc_init → cpu_idle → schedule → proc_run → switch_to`。其中页操作负责地址映射和页引用，进程创建负责 PCB、栈与首次现场，调度负责选择线程并恢复其现场。当前独立测试只运行内存相关前半段。

## 四、实验内容与实现

### 前置任务：Lab2/Lab3 基础迁移

**负责人：** 2412790-杨茜茹。

#### 模块功能描述

**需要实现/修改的函数：**

```c
static void page_init(void);
static void best_init(void);
static void best_init_memmap(struct Page *base, size_t n);
static struct Page *best_alloc_pages(size_t n);
static void best_free_pages(struct Page *base, size_t n);
static size_t best_nr_free_pages(void);
static void best_insert_free_block(struct Page *base, size_t n);
void interrupt_handler(struct trapframe *tf);
static int exception_inst_length(uintptr_t epc);
void exception_handler(struct trapframe *tf);
// 汇编：RESTORE_ALL、__alltraps 中的传参和调用
```

**功能说明：**

在 `code/kern/mm/default_pmm.c`、`pmm.c`、`code/kern/trap/trap.c`、`trapentry.S` 的标记处迁移。参考 Lab3 集成提交 `6d8bc3cc39a3e5751aa3ce9dda7b6464773e5f07`：物理内存、Best-Fit、同步异常原实现来自于蕾；定时器处理原实现来自程子涵；Trap 入口及恢复原实现来自杨茜茹。本次适配由杨茜茹完成。

`page_init` 检查设备树 RAM，建立描述符并保留内核及数组占用页。Lab4 增加的 Page 字段也初始化。分配器保留 Best-Fit 算法，但使用 Lab4 已声明的 `default_pmm_manager` 名称。Trap 传入帧指针调用 C，再恢复返回 CSR 和通用寄存器，最后恢复 sp；保持原 SAVE_ALL 和 forkrets。不迁移旧实验十次计时后关机的策略。

#### 最终提示词

完整文本见 [prompt.md](prompt.md) 的“任务一：迁移 Lab2/Lab3 基础功能”，包含四段式规格及修改边界。

#### 实现迭代过程

**第一次实现：** 发现原 default_pmm.c 引用缺失的 best_fit_pmm.h，而头文件和调用方使用 default_pmm_manager；统一引用及注册名称，保留原测试。根据 Lab4 的映射上界和 Page 布局适配初始化。

**实际结果：** 四个对象文件编译通过；后续独立 QEMU 中物理内存与原页表检查通过。此测试没有进入 Trap 中断/异常路径，Trap 运行验证仍留到集成阶段。

### 功能模块一：虚拟内存页操作

**负责人：** 2412790-杨茜茹。

#### 模块功能描述

**需要实现/修改的函数：**

```c
// code/kern/mm/pmm.c，LAB4 function1 2412790 标记
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);
```

**功能说明：**

`page_insert` 先保存原根项，再调用 get_pte 创建或查找末级 PTE。失败时，如果此次新增了中间表，就恢复原根项并释放该页；已有中间表不回收。成功后先取得新页引用：旧映射为同页则抵消本次增量，旧映射为其他页则调用 page_remove 撤销。最后通过 pte_create 写入页号、权限和 PTE_V，并刷新对应地址。

先加引用可以避免同页重插时把仍要使用的页降到零并释放。权限按调用者传入值更新，不根据文件名或旧权限自行补充。

`page_remove` 使用 get_pte(create=0)，缺少页表或有效 PTE 时直接返回。有映射时保存物理页指针，清 PTE、刷新 TLB，再递减引用；归零才 free_page。中间页表由地址空间拥有者管理，本函数不主动回收。

这两个函数沿用框架4KiB三级页表路径，不实现巨页拆分、并发页表锁或用户地址空间复制。失败回滚依赖当前 get_pte 最多两次中间表分配的实现，框架变化时应重新审查。

#### 最终提示词

完整文本见 [prompt.md](prompt.md) 的“任务二：页映射插入与删除”。测试规格另见后续测试记录，区分原始实现提示词与事后整理内容。

#### 实现迭代过程

**第一次实现：** 阅读 get_pte 时发现第二次分配失败不会回收第一次分配的中间表，因此在实现规格中加入回滚要求。此项是实现前检查发现的问题，不是运行中已发生的故障。

**验证过程：** 对象编译通过后，先运行宿主接口测试，再运行真实 PMM 和页表的独立 QEMU 测试；目前两轮测试均通过，没有因测试失败修改页操作实现。

**关键点：** 同页重插不改变净引用数；异页替换只释放旧页最后引用；删除幂等；失败不遗留本次新增页表；修改有效映射后刷新 TLB。

### 功能模块二：进程创建及初始化

待该模块负责人按“模块功能描述、最终提示词、实现迭代过程”填写本人内容与真实结果。

### 功能模块三：进程调度

待该模块负责人按相同结构填写，并组织最终联调。

### 分页拓展

杨茜茹待补：Sv32/Sv39/Sv48 对比、get_pte 相似遍历与查找/分配拆分、启动映射、VMA 与 PTE、各段权限和巨页细化思路。暂不以未完成的分析充作已提交答案。

## 五、测试与验证

| 验证 | 实际结果 | 范围 |
| --- | --- | --- |
| 四个基础对象及 pmm.o 编译 | 通过 | 编译不等于运行 |
| 宿主接口测试 | 8/8，退出码0 | 真实页操作函数体；get_pte、分配器与TLB由测试模拟 |
| 独立 QEMU | ISOLATED_QEMU: PASS | 真实PMM、get_pte、页操作及虚拟访存；临时测试入口 |
| 完整内核与原评分 | 未执行 | 需要进程创建、调度实现 |

宿主测试覆盖新映射、非页对齐地址、同页权限更新、共享页替换、最后引用释放、重复删除、首级分配失败、部分分配回滚和已有中间表保留，组织为8个场景。

QEMU 输出 check_alloc_page、check_pgdir、check_boot_pgdir 均 succeeded，新增四组检查通过，包括实际地址读写、共享页、同页重插、已访问地址替换后的读取和资源回收。清理测试独占的中间表后空闲页数恢复基线。

失败注入目前只在宿主测试中执行。QEMU 没有动态验证权限故障、Trap 路径和线程调度；不能由本次结果声称整机全部完成。

构建日志中 kmalloc.c:92 存在整数传给指针参数的警告；构建及运行成功，未改动该框架文件。链接器 removing unused section 是移除未使用段，也说明不能把被移除模块当作本次运行覆盖。

可复现脚本：`code/tests/member_a/run_page_ops_host.py`、`run_page_ops_qemu.py`，均传入 `--root` 指定仓库根目录。证据位于 `report/logs/member-a-page-ops-host.log` 和 `member-a-page-qemu*`；真实截图待补充。最终整机截图及评分由调度与集成负责人补充。

## 六、实验总结与收获

### 对操作系统的理解

页表项管理可访问关系，引用计数管理物理页生命期，TLB保存近期翻译；三者必须同步。多个虚拟地址可共享一页，因此删除一项不等于立即释放物理页。失败清理也要区分本次取得和调用前已有的资源。

本模块尚未覆盖按需分页、换页、多核TLB通知、并发页表修改及完整用户进程隔离；这些是OS中的重要问题，不能从内核线程共享映射测试推导其已实现。

队友补充各自模块收获，由汇总负责人去重整理。
