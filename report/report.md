# 操作系统实验报告：Lab2 物理内存和页表

## 实验基本信息

| 项目 | 内容 |
| --- | --- |
| 实验名称 | Lab2：物理内存和页表 |
| 小组成员 | 2411824 于蕾、2412790 杨茜茹、2413921 程子涵 |
| 总报告整合 | 2412790 杨茜茹（B），借助 Codex 整理 |
| 实现与运行记录 | 各成员开发记录及 2026-10-18 联调记录 |
| 本次文档整理日期 | 2026-10-20 |

### 小组分工

| 成员 | 实验任务 | 交付与公共职责 |
| --- | --- | --- |
| A：2411824 于蕾 | page_init、内存探测思考题、First-Fit 分析、Best-Fit | 对应实现、提示词、报告、两张测试图 |
| B：2412790 杨茜茹 | Buddy 实现、独立测试 | 两份实际提示词、Buddy 报告、分支接入和回归、总文档整合 |
| C：2413921 程子涵 | SLUB 实现与测试、QEMU 地址翻译与 TLB 六项调试 | 八项提示词整理稿、调试脚本、报告和十一张证据图 |


## 一、实验目的

1. 理解 RAM 发现、物理地址、虚拟地址、物理页号与 Page 描述符的关系。
2. 实现物理内存初始化，排除启动保留区和内核管理开销。
3. 理解 First-Fit，借助 AI 实现 Best-Fit、Buddy，并设计可检验的规格和独立测试。
4. 用简化 SLUB 理解页级分配与任意大小对象分配的分层。
5. 借助双 GDB 观察 Sv39 页表翻译及 QEMU SoftTLB，把实际访存、源码和手算结果对应起来。

## 二、实验环境

### 2.1 AI 工具与人员操作

| 成员 | AI 工具 | 模型版本及操作记录 |
| --- | --- | --- |
| A | 交付材料未明确记录具体工具名称 | 未记录精确模型；依据 A 的提示词、反馈、报告与截图保留结果 |
| B | Codex 桌面应用辅助，VS Code 打开 Ubuntu WSL 工程 | 未记录精确模型；本人在 WSL 终端执行编译、测试和 Git 操作并反馈输出 |
| C | Codex 桌面应用 | 未记录精确模型；依据 C 的代码、报告与调试截图保留记录 |

不将工具名称当作模型版本，也不推测未记录的型号。原提示词中的允许修改范围和阶段依赖按实际使用版本保留。

### 2.2 编译和调试环境

| 项目 | 已记录环境 |
| --- | --- |
| A | Best-Fit 完成阶段报告记录 RISC-V GCC 10.2.0；page_init 早期反馈曾仅具备 riscv64-linux-gnu-gcc，属于不同阶段 |
| B | Windows、Ubuntu WSL、VS Code；RISC-V GCC 10.2.0；QEMU 启动显示 OpenSBI v0.4；原生 GCC 与 Python 3 执行 Buddy 测试 |
| C | 报告记录 Windows+WSL2、Ubuntu 26.04.1 LTS、RISC-V GCC 14.2.0、调试版 QEMU 4.1.1、gdb/gdb-multiarch 17.1、ASan/UBSan |
| 客体 | RISC-V 64 位、Sv39、4 KiB 基本页；默认 RAM 为 [0x80000000,0x88000000) |
| 当前开关 | 默认 Best-Fit；BUDDY=1、SLUB_CHECK=1、MMU_PROBE=1 分别控制对应功能 |

本人的旧工具链不接受显式 zicsr/zifencei 扩展设置，因此 B 的集成验证使用命令行 ARCH_FLAGS= 恢复其可用默认设置；不修改 C 的 Makefile 默认值。C 的最终评分截图另显示显式 ARCH_FLAGS="-march=rv64imac -mabi=lp64"，各次运行参数应分别看待。

## 三、实验整体逻辑分析

### 3.1 从启动映射到页面和对象分配

OpenSBI 向入口传入 hartid 和 DTB 地址。entry.S 保存参数，建立静态根页表、写 satp 并刷新 TLB，再进入高地址 kern_init。框架已经完成分页开启；本次 page_init 管理的是映射背后的物理页资源，不重复开启分页。

dtb_init 提取 RAM 起点和长度。pmm_init 先选择并初始化 pmm_manager，再由 page_init 放置和初始化 Page 描述符、排除固件与内核占用，最后注册空闲完整页并执行管理器检查：

```text
entry.S：启动页表与 Sv39
  → kern_init：清 BSS、解析 DTB
  → 可选 MMU 探针（在 PMM 初始化之前）
  → pmm_init
      → init_pmm_manager → 管理器.init
      → page_init → 管理器.init_memmap
      → check_alloc_page → 管理器.check
  → 可选 SLUB 检查（在 PMM 初始化之后）
      → alloc_pages/free_pages → 当前页管理器
      → 回收后再次检查当前 PMM
```

### 3.2 为什么按这个顺序实现

先确定真实可用物理页，分配器才有合法资源；再理解 First-Fit 的状态约定，完成 Best-Fit 和 Buddy 的页级策略。SLUB 依赖已经可用的公共页接口，因此安排在上层；MMU 调试则针对启动映射，可在页分配前观察。公共接口让成员可以独立开发，集成时重点核对所有权、申请/释放长度和构建开关。

Best-Fit 成功时精确分配 n 页，Buddy 则向上取整到二次幂、按实际占用扣页，但释放必须传原始 n。SLUB 保存原始请求页数，避免把这两个契约混用。内核代码、静态 Buddy 元数据和 C 的探针/数组都会改变 end，因此空闲页基线由每次实际构建确定，不能写死。

## 四、实验内容与实现

### 功能模块一：物理内存初始化功能

**负责人：** 2411824 于蕾

#### 模块功能描述

**需要实现/修改的函数：**

```c
// code/kern/mm/pmm.c
static void page_init(void);
```

**功能说明：**

本模块根据设备树提供的物理内存信息，建立物理页描述符数组，并将排除 OpenSBI、内核镜像、描述符数组及对齐占用后的完整空闲页面交给当前物理内存管理器，为后续页面分配提供初始状态。

`page_init()` 不接收参数、不返回结果，由 `pmm_init()` 在管理器初始化后调用。执行时，框架已在 `entry.S` 中开启 Sv39 分页，`dtb_init()` 也已解析设备树。因此，本函数负责物理页管理信息的初始化，不负责重新建立启动页表。

本次仅修改 `code/kern/mm/pmm.c`，在 `//lab2 function1 2411824` 处实现 `page_init()`，未新增辅助函数，未修改头文件、设备树解析、管理器选择、测试或 Makefile。

核心实现分为以下几个步骤：

1. **验证设备树提供的物理内存信息。**

   通过 `get_memory_base()` 和 `get_memory_size()` 获取 RAM 起点与大小。起点或大小为零、起点与大小相加溢出、RAM 起点不符合当前固定基址模型，或者 RAM 无法覆盖内核镜像时，调用 `panic()` 终止初始化。

   计算 RAM 末端前，使用以下条件检查加法溢出：

   ```c
   if (mem_base > ~(uint64_t)0 - mem_size) {
       panic("page_init: memory base + size overflows");
   }
   ```

2. **设置地址偏移与可管理物理页号上界。**

   ```c
   va_pa_offset = PHYSICAL_MEMORY_OFFSET;
   ```

   当前偏移为 `0xFFFFFFFF40000000`，与启动映射一致。不能使用 `KERNBASE - DRAM_BASE` 计算偏移，因为 `KERNBASE` 对应内核物理起点 `0x80200000`，而 DRAM 起点为 `0x80000000`。

   可管理末端及页号上界的计算关系为：

   ```text
   ram_end     = mem_base + mem_size
   managed_end = ROUNDDOWN(min(ram_end, PHYSICAL_MEMORY_END), PGSIZE)
   npage       = PPN(managed_end)
   ```

   默认 128 MiB 配置下，理论值为：

   ```text
   RAM 范围：[0x80000000, 0x88000000)
   nbase = 0x80000
   npage = 0x88000
   描述符数量 = npage - nbase = 32768
   ```

   `npage` 表示可管理物理页号的上界，不是 RAM 页面总数。上述数值在后续默认配置 QEMU 输出中得到确认；早期初始化单独实现阶段尚未完成整机验证。

3. **在内核镜像之后安排描述符数组。**

   ```c
   pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE);
   uintptr_t pages_pa = PADDR((uintptr_t)pages);
   ```

   `end` 是链接脚本提供的内核末端虚拟地址。将其向上按页对齐，得到描述符数组的虚拟起点，再通过 `PADDR()` 转换成物理地址进行范围检查。

   ```text
   descriptor_count  = npage - nbase
   descriptor_bytes  = descriptor_count × sizeof(struct Page)
   descriptor_end_pa = pages_pa + descriptor_bytes
   ```

   按当前 RV64 结构体布局，`sizeof(struct Page)` 为 40 字节，默认配置下描述符数组占用 `1310720` 字节，即 1.25 MiB。

   实现先检查描述符字节数乘法是否溢出，再验证数组起点和长度是否位于可管理范围内：

   ```c
   if (pages_pa > managed_end) {
       panic("page_init: descriptor array starts beyond managed memory");
   }
   if (descriptor_bytes > managed_end - pages_pa) {
       panic("page_init: descriptor array does not fit in RAM");
   }
   ```

   通过检查后才写入描述符，避免数组越界后再发现空间不足。

4. **初始化全部描述符，并先标记为保留。**

   ```c
   for (size_t i = 0; i < descriptor_count; i++) {
       pages[i].ref = 0;
       pages[i].flags = 0;
       pages[i].property = 0;
       list_init(&pages[i].page_link);
       SetPageReserved(&pages[i]);
   }
   ```

   描述符数组位于内核 `end` 之后，不能假设已经随 BSS 清零，因此需要显式初始化。

   先保留全部页面，再登记可用区，可以使 OpenSBI、内核镜像、内核尾页和描述符存储页保持保留状态，避免被错误分配。

5. **注册剩余完整空闲页面。**

   ```c
   uintptr_t free_begin = ROUNDUP(descriptor_end_pa, PGSIZE);
   uintptr_t free_end = managed_end;

   if (free_begin < free_end) {
       size_t free_pages_count = (free_end - free_begin) / PGSIZE;
       init_memmap(pa2page(free_begin), free_pages_count);
   }
   ```

   空闲起点向上对齐，避开描述符占用的页面；末端向下对齐，排除不完整尾页。不存在完整空闲页时，不调用 `init_memmap()`，避免传入 `n == 0`。

   各区域按物理地址排列如下：

   ```text
   DRAM_BASE
     │ OpenSBI 等保留区域
   KERNEL_BEGIN_PADDR
     │ 内核镜像、启动栈、启动页表等
   PADDR(end)
     │ 对齐间隙
   pages_pa
     │ Page 描述符数组
   descriptor_end_pa
     │ 对齐间隙
   free_begin
     │ 可分配的完整物理页
   managed_end
   ```

   本函数通过公共 `init_memmap()` 接口登记空闲区，不依赖 First-Fit 或 Best-Fit 的私有数据结构，后续切换管理器时可以复用。

#### 最终提示词

本任务的完整提示词及初次实现反馈见 [物理内存初始化提示词](prompt.md#a-page-init)。

提示词通过 `[RELY]` 列出真实工程接口和地址约定，通过 `[GUARANTEE]` 限定修改范围，通过 `[SPECIFICATION]` 描述正常初始化、无效设备树信息、范围裁剪、页面对齐、描述符空间不足及无完整空闲页等 Case。

其中明确要求修改真实文件，仅在 `pmm.c` 指定标记处实现函数，不修改其他模块或测试，并如实报告编译与运行验证结果。完整提示词以附件原文为准。

#### 实现迭代过程

本模块记录了一次实现迭代。初始化提示词后的实现反馈显示：pmm.c 单独编译通过，早期整项目曾被尚未实现的 Best-Fit 所阻塞；没有跨分工补写。后续 Best-Fit 完成后，默认启动及评分通过，结果见第五章。

---

### Challenge：硬件的可用物理内存范围的获取方法

**负责人：** 2411824 于蕾

**问题：如果 OS 无法提前知道当前硬件的可用物理内存范围，有什么办法让 OS 获取？**

操作系统可以从固件、引导程序或平台描述中取得物理内存布局，再排除保留区域和启动占用，得到可以交给分配器的页面集合。

**1. 从设备树获取。**

在采用设备树的平台上，引导环境将 DTB 地址传给内核。内核解析内存节点的 `reg` 属性，取得 RAM 起点和长度。通用实现还需要根据 `#address-cells`、`#size-cells` 解码，并支持多个内存范围。

除了 RAM 信息，还要读取内存保留块和 `/reserved-memory` 等保留信息，避免将固件或其他组件使用的区域交给普通分配器。[参考：Devicetree 设备节点规范](https://devicetree-specification.readthedocs.io/en/latest/chapter3-devicenodes.html)、[DTB 内存保留块](https://devicetree-specification.readthedocs.io/en/v0.2/flattened-format.html)。

**2. 从固件或引导程序获取。**

在 UEFI 平台上，OS Loader 可以通过 `GetMemoryMap()` 取得带类型的内存映射，再传递给内核；传统 BIOS 平台可由引导程序使用 E820 接口获得地址映射。

内核需要根据内存类型和使用阶段判断是否可以回收，保留固件运行时仍需使用的区域，不能把所有报告范围立即作为空闲 RAM。[参考：ACPI 系统地址映射接口](https://uefi.org/specs/ACPI/6.6/15_System_Address_Map_Interfaces.html)。

对布局固定的开发板，也可以采用平台文档或板级配置，但这种方式绑定具体硬件，平台变化后必须同步更新。

**3. 区分 RAM、保留区域和设备地址空间。**

获得 RAM 范围后，还应扣除固件、内核、启动栈、页表、仍需保留的 DTB 和管理元数据等占用。

内存映射设备寄存器虽然也具有物理地址，但不是普通 RAM，不能参与页面分配。最终可用区还要按页对齐，并限制在内核当前能够访问和管理的范围内。

**4. 本实验设备树方案的作用与限制。**

本实验由 `entry.S` 保存 DTB 地址，`dtb_init()` 解析 RAM 信息，`page_init()` 建立描述符和初始空闲区。这使初始化能够依据设备树提供的大小工作，而不必直接写死默认 RAM 容量。

但当前实现仍是简化模型：解析器只提取首个匹配内存节点中的一组起点和大小，没有完整支持通用 cells 编码、多段 RAM 和额外保留区；初始化还要求 RAM 从 `DRAM_BASE` 开始，并将管理末端限制在 `PHYSICAL_MEMORY_END`。

因此，本实验体现了物理内存发现与初始化的基本流程，尚不能处理任意平台的完整内存布局。

**5. 信息缺失或不可靠时的处理。**

应先检查启动协议、固件接口和平台文档，寻找可靠信息来源，并对取得的范围检查溢出、重叠、类型和保留区。无法获得可信信息时，可以采用明确的平台配置，或者停止初始化。

不能简单通过逐地址写入再读回来扫描全部物理地址，因为未知地址可能对应设备、未映射区域或已有数据，访问可能引起副作用、异常或破坏。只有平台明确支持且能保护已有内容时，才考虑受控探测。

本次实现选择在设备树信息无效时调用 `panic()`，避免使用未经验证的默认容量继续初始化。

---
### 功能模块二：物理内存分配功能

### 任务一：理解first-fit 连续物理内存分配算法（思考题）

First-Fit（首次适应）算法使用按物理地址递增排列的双向链表管理空闲页块。链表只记录各空闲块的首页，首页描述符中的 `property` 表示该块包含的连续空闲页数，`nr_free` 记录空闲页面总数。申请页面时，算法从链表头开始查找，选择第一个足够大的空闲块。

**1. 初始化管理状态：`default_init()`**

该函数通过 `list_init()` 将空闲链表初始化为空，并将 `nr_free` 置为 0。此时仅建立管理器的初始状态，还没有登记可分配页面。

**2. 登记空闲页面：`default_init_memmap()`**

该函数接收一段连续页面的起始描述符 `base` 和页数 `n`。它检查这些页面原本处于保留状态，随后清除其标志和 `property`，将引用计数设为 0。

接着，将首页的 `property` 设为 `n`，设置首页的 `PageProperty` 标志，并将该空闲块按地址顺序插入链表，同时使 `nr_free` 增加 `n`。这样，初始化阶段确定的可用页面就交给了分配器。

**3. 分配连续页面：`default_alloc_pages()`**

申请 `n` 页时，函数首先检查空闲页总数是否足够。若不足，直接返回 `NULL`；否则从链表头开始遍历，选择第一个满足 `property >= n` 的空闲块。

找到后，将该块从链表移除。如果块大小恰好为 `n`，就分配整个块；如果大于 `n`，则分配前面的 `n` 页，并将剩余部分作为新的空闲块放回原位置。最后减少 `nr_free`，清除已分配块首页的 `PageProperty` 标志，并返回首页描述符。若没有足够大的连续块，即使空闲页总数足够，也会返回 `NULL`。

**4. 回收与合并页面：`default_free_pages()`**

释放页面时，函数检查页面状态，重置标志和引用计数，将释放区间登记为空闲块，并按地址顺序插回链表，同时增加 `nr_free`。

随后检查前后两个空闲块：若前块末端等于当前块起点，就与前块合并；若合并后的块末端等于后块起点，就继续与后块合并。合并时更新块长度，删除多余的链表节点并清除其首页标志，最终只保留合并后块首页的记录。

**5. 分配与释放示例**

假设最初有一个连续的 10 页空闲块：

```text
初始：        [空闲 10 页]
申请 3 页：   [已分配 3 页][空闲 7 页]
再申请 2 页： [已分配 3 页][已分配 2 页][空闲 5 页]
释放前 3 页： [空闲 3 页][已分配 2 页][空闲 5 页]
释放中间2页：[空闲 10 页]
```

最后一次释放的 2 页与前后空闲块都相邻，因此可以通过两次合并恢复为一个连续的 10 页空闲块。

### 练习：原有 First-Fit 算法是否有进一步的改进空间？

有。当前实现使用链表顺序查找，分配时最坏需要遍历所有空闲块；释放时为了按地址插入，也可能需要遍历链表。空闲块较多时，这些操作的开销会增加。

可以引入按大小分类的索引或平衡树，加快合适空闲块的查找，同时保留地址关系以支持相邻合并，但会增加元数据和维护成本。也可以采用 Next-Fit，从上次查找位置继续搜索，减少反复扫描链表前部的情况，不过其性能与碎片情况仍取决于请求序列。

此外，First-Fit 只选择第一个足够大的块，可能拆分较大的空闲块而留下小碎片。可以考虑 Best-Fit 等选择策略，但 Best-Fit 也可能产生难以利用的小块，不能保证始终减少碎片。当前实现已经支持相邻空闲块合并，后续改进应结合实际请求特点，在查找效率、碎片和实现复杂度之间权衡。

### 任务二：实现 Best-Fit 连续物理内存分配算法（设计提示词借助 AI 完成）

**负责人：** 2411824-于蕾

#### 模块功能描述

**需要实现/修改的函数：**

```c
// code/kern/mm/best_fit_pmm.c：实现的管理器接口
static void best_init(void);
static void best_init_memmap(struct Page *base, size_t n);
static struct Page *best_alloc_pages(size_t n);
static void best_free_pages(struct Page *base, size_t n);
static size_t best_nr_free_pages(void);

// 内部静态辅助函数：best_insert_free_block
static void best_insert_free_block(struct Page *base, size_t n);

// 原有检查函数：保留定义，仅在注册结构之前补充前向声明
static void best_fit_check(void);

// code/kern/mm/pmm.c：仅修改管理器选择
static void init_pmm_manager(void);
```

此外，定义并注册管理器对象 `const struct pmm_manager best_fit_pmm_manager`，将其初始化、空闲区登记、分配、释放、空闲页数查询和检查接口分别绑定至对应函数。`basic_check()`、`best_fit_check()` 的原有测试内容均保持不变。

**功能说明：**

本模块负责 ucore 内核中的连续物理页面分配与回收。内核通过统一的 `pmm_manager` 接口使用物理内存管理器；本任务将默认管理器切换为 Best-Fit，使实际分配和自检均经过本次实现。

模块采用按物理地址递增排列的双向循环空闲链表，仅将空闲块头页的 `page_link` 加入链表。头页的 `PageProperty` 标志表示该页是空闲块头，`property` 记录连续空闲页数；块内其他页面以及已分配页面不设置该标志。`nr_free` 记录空闲页面总数，正常管理状态下等于所有空闲块长度之和。

主要处理以下场景：

1. **初始化与空闲区登记。** `best_init()` 初始化空链表并清零计数；`best_init_memmap()` 将合法的保留页区间转为空闲页面，初始化页面状态，并按地址插入及合并相邻空闲块。
2. **最优适配分配。** 对请求的 `n` 页，遍历空闲链表，选择所有长度不小于 `n` 的空闲块中最小的一块；长度相同时选择物理地址最低的块。从选中块的低地址端分配恰好 `n` 页，存在剩余时将余块留在原地址顺序位置。
3. **释放与合并。** 将释放区间按地址插入链表，依据描述符连续性判断能否与前后块合并。例如，`previous + previous->property == base` 表示前块与当前块连续。合并时清除被吞并块头的标志并删除相应节点，空闲计数仅增加本次释放的 `n` 页。
4. **失败与边界处理。** 空链表、总空闲页数不足或不存在足够大的连续块时返回 `NULL`，不改变空闲状态；对零页请求以及参考实现明确检查的非法页面状态使用断言。页面标志检查不构成对所有重复释放和错误指针的完整检测。

模块与 `page_init()` 配合完成初始可用物理内存登记，与 `alloc_pages()`、`free_pages()`、`nr_free_pages()` 等公共包装函数配合向内核提供服务，并通过原有 `best_fit_check()` 完成检查。管理器内部直接操作自己的状态，避免调用公共包装函数导致递归。为兼容原有测试对 `free_list`、`nr_free` 的临时隔离与恢复，不引入额外空闲索引、缓存计数或分配大小表。

实际修改范围如下：

| 文件                          | 修改内容                                                     |
| ----------------------------- | ------------------------------------------------------------ |
| `code/kern/mm/best_fit_pmm.c` | 在 `//lab2 function2-2 2411824` 标记处实现管理器状态、辅助函数、各管理接口和注册结构。 |
| `code/kern/mm/pmm.c`          | 在 `init_pmm_manager()` 中将管理器选择改为 `pmm_manager = &best_fit_pmm_manager;`，保留名称打印及初始化调用。 |

#### 最终提示词

本次实现使用的最终提示词文件较长，已在 [prompt.md 的 A·Best-Fit 部分](prompt.md#a-best-fit)完整保留，正文不重复长篇粘贴。

完整提示词原文已集中复制到 [提示词汇总](prompt.md#a-best-fit)。
#### 实现迭代过程

---

##### 第一次迭代：初始提示词约束不足

**初始提示词：**

```markdown
请参考 default_pmm.c，在 best_fit_pmm.c 中实现 Best-Fit
连续物理内存分配算法，包括初始化、空闲内存登记、页面分配、
页面释放以及空闲页数查询。

分配时选择能够满足请求的最小空闲块，释放时合并相邻空闲块，
并将 pmm.c 中的物理内存管理器切换为 Best-Fit。
```

**遇到的问题：**

- **问题1：空闲链表组织方式不明确。** 初始提示词仅要求选择最小适配块，没有明确链表的排列顺序。如果按块大小排列，释放时定位物理相邻块需要额外处理，容易增加合并逻辑的复杂度。
- **问题2：页面状态与计数约束不完整。** 未明确拆分、释放和合并时 `PageProperty`、`property` 与 `nr_free` 的更新规则，存在被吞并块头标志残留或空闲页数重复累计的实现风险。
- **问题3：原有测试兼容要求不足。** 未说明测试直接使用 `free_list` 和 `nr_free`，也未覆盖测试临时隔离状态下链表为空、计数尚未同步的情况。
- **问题4：边界条件与修改范围不明确。** 未规定同大小候选块的选择规则、分配失败时的状态要求，也未明确要求保留原有测试、上一任务代码及 Makefile。

**问题解决策略：**

- 针对问题1：明确采用按物理地址递增排列的空闲链表；分配时遍历链表比较块大小，实现 Best-Fit；同大小时选择地址最低的块。
- 针对问题2：补充页面状态不变量，要求仅空闲块头页具有 `PageProperty` 标志，分配和释放分别按实际页数更新 `nr_free`，合并时不重复记账。
- 针对问题3：补充原有测试的状态保存与恢复方式，要求保留同名状态，不引入额外索引，并安全处理空链表。
- 针对问题4：逐函数列出前置条件、后置条件和边界场景，明确允许修改的文件与位置，禁止修改已有测试和评分脚本。

---

##### 第二次迭代：完善提示词并完成验证

**提示词调整：**

通过以下四个部分细化任务要求：详细全文见[prompt.md](./prompt.md)。
```

1. `[PROMPT]`：明确任务目标、操作要求及结果输出要求。
2. `[RELY]`：提供真实文件路径、结构体、链表接口、公共包装函数及测试约定。
3. `[GUARANTEE]`：明确必须实现的接口、管理器注册方式以及允许修改的范围。
4. `[SPECIFICATION]`：逐函数规定状态变化、选块规则、拆分合并逻辑及边界处理方式。
```


**最终结果：**

根据实际执行结果报告及测试截图，该模块最终：

- ✅ 编译链接通过。
- ✅ 内核输出 `memory management: best_fit_pmm_manager`，确认管理器切换生效。
- ✅ 输出 `check_alloc_page() succeeded!`。
- ✅ `make test` 得到 `grading: 6 / 6 points`。
- ✅ `make grade` 中 `check page init` 和 `check best fit` 均为 `OK`，总分为 `20/20`。
- ✅ 在现有测试覆盖范围内，功能符合预期。

**关键改进点总结：**

1. 将笼统的算法要求细化为逐函数的接口契约，减少实现歧义。
2. 明确空闲链表顺序、页面标志和空闲页数的维护规则。
3. 补充原有测试的兼容要求，避免额外状态破坏测试隔离流程。
4. 明确管理器注册与内核接入位置，确保实际运行本次实现。
5. 保持修改范围可控，使用原有检查和评分结果验证实现。


---


#### Best-Fit 的进一步改进空间

当前选块需要遍历全部空闲块，最坏查找成本与空闲块数量成正比；地址有序链表便于相邻合并，但也需要扫描插入位置。可以在精确匹配时提前结束扫描，或增加按大小索引、分级空闲表以加快选块，同时保留地址关系。不过额外状态必须与测试的临时隔离/恢复约定一致，不能直接加入测试无法保存的隐藏索引。

Best-Fit 可能留下难以利用的小余块，并不保证比 First-Fit 始终减少碎片。可通过独立请求序列比较分配成功率、剩余块分布和扫描成本，再决定是否优化。本次没有性能实测，不宣称已有速度或碎片率提升。参考页面状态断言也不等同于完整的非法释放检测。

### 功能模块二·任务三：Buddy System 实现与独立测试

**负责人：** 2412790 杨茜茹

#### 模块功能描述

**需要实现/修改的函数：**

```c
// code/kern/mm/buddy_pmm.c：实现 Buddy 管理器接口
static void buddy_init(void);
static void buddy_init_memmap(struct Page *base, size_t n);
static struct Page *buddy_alloc_pages(size_t n);
static void buddy_free_pages(struct Page *base, size_t n);
static size_t buddy_nr_free_pages(void);
static void buddy_check(void);

// 同一文件中的内部辅助函数
static size_t order_size(unsigned int order);
static size_t descriptor_index(const struct Page *page);
static bool partner_index(size_t index, unsigned int order, size_t *partner);
static void mark_block(size_t index, unsigned int order,
                       bool allocated, size_t requested);
static void return_block(size_t index, unsigned int order);

// code/kern/mm/pmm.c：添加 Buddy 的条件选择，保留默认管理器
static void init_pmm_manager(void);

// code/tools/buddy_host/cases.c：独立测试入口
int main(int argc, char **argv);

// code/tools/buddy_host/platform.c：宿主输出与断言适配
void host_note(const char *message);
int cprintf(const char *format, ...);
void __panic(const char *file, int line, const char *format, ...)
    __attribute__((noreturn));
```

此外，在 `buddy_pmm.c` 中定义 `const struct pmm_manager buddy_pmm_manager`，将六个管理接口注册到框架；在 `buddy_pmm.h` 中提供声明。`code/tools/buddy_host/run.py` 的 `main()` 负责逐项运行测试、核对输出和退出码，Makefile 提供 `BUDDY=1` 和 `buddy-host-test` 两个入口。

**功能说明：**

Buddy 管理的是 `page_init()` 已经登记的空闲物理页。它不重新探测内存，也不建立页表，而是通过 `pmm_manager` 向内核提供连续页分配、释放和空闲页数查询。SLUB 使用公共页接口取页，因此也可以运行在 Buddy 之上。

本实现把空闲空间按阶组织，每个块包含 `2^order` 页。`order_lists` 保存各阶空闲链表，`free_count` 记录空闲页总数，`records` 保存页面状态、块阶数和原始申请长度。空闲块只有首页设置 `PageProperty`，其 `property` 为块的实际页数；已分配页和块内其他页不设置该标志。

主要处理以下场景：

1. **初始化与登记。** `buddy_init()` 清空内部状态，此时不要求 `pages` 已经建立。`buddy_init_memmap()` 先检查输入范围及页面状态，再将区间拆成对齐的二次幂块。起点和长度不规整时也要完整利用页面；多次登记的区间可以合并，但不能跨过未登记的空洞。
2. **分裂与分配。** 申请 `n` 页时，先确定能容纳请求的最小二次幂 `k`，再寻找最小可用阶。如果只能找到更大的块，就保留低半块继续拆分，将高半块放回相应链表。成功后记录原始 `n`，空闲页数减少实际占用的 `k`。
3. **释放与合并。** 释放时必须传入分配首描述符和原始申请长度。`buddy_free_pages()` 核对记录后，通过 `return_block()` 查找同阶空闲伙伴并逐级合并。一次释放只增加原块占用的页数，不能按合并后的大块重复累计。
4. **失败和非法输入。** 空间不足、没有合适块或请求过大时返回 `NULL`。零页申请、错误长度、非首地址、重复释放、越界或重叠登记等使用断言拒绝。检查指针时先验证整数地址范围和描述符对齐，再访问数组。

伙伴关系按绝对物理页号计算。描述符下标 `index` 对应页号 `nbase + index`，若块大小为 `k`，其伙伴页号为：

```text
ppn = nbase + index
buddy_ppn = ppn XOR k
```

两块相邻、大小相同，还不一定互为伙伴，块首还必须满足该阶的物理对齐。例如在本实验 `nbase` 的对齐条件下，`[2,4)` 和 `[4,6)` 都是两页块，但不能合成一个四页块。

分配与释放的长度约定如下：

| 原始申请 | 实际占用 | 空闲页数变化 | 正确释放方式 |
| --- | --- | --- | --- |
| 1 页 | 1 页 | 分配减 1，释放加 1 | `free_pages(base, 1)` |
| 3 页 | 4 页 | 分配减 4，释放加 4 | `free_pages(base, 3)` |
| 5 页 | 8 页 | 分配减 8，释放加 8 | `free_pages(base, 5)` |
| 9 页 | 16 页 | 分配减 16，释放加 16 | `free_pages(base, 9)` |

因此，本实现不支持任意子区间释放。SLUB 保存原始请求页数，申请三页后仍以三页释放，符合这一约定。

`records` 和检查暂存放在静态 BSS 中，避免初始化分配器时又调用自身取页。当前容量覆盖 32768 个描述符，RV64 下记录数组约 512 KiB、检查暂存约 32 KiB。这些空间属于内核占用，由 `page_init()` 根据实际 `end` 排除，不能继续使用旧内核的空闲页数。

`buddy_check()` 检查链表、块大小与对齐、页面状态、计数以及遗漏的伙伴合并。它是结构检查；完整的分配释放行为另由独立测试验证。当前检查还要求受管理页面的引用计数为零，适用于本次实验；后续引入引用计数使用者时需要调整检查条件。本实验按单核串行调用实现，没有加入并发同步。由于更新状态时会遍历块内描述符，也不能将所有操作都简单写成对数复杂度。

实际修改范围如下：

| 文件 | 修改内容 |
| --- | --- |
| `code/kern/mm/buddy_pmm.c`、`buddy_pmm.h` | 新增 Buddy 管理器、内部状态、辅助函数和基础检查 |
| `code/kern/mm/pmm.c` | 添加 Buddy 头文件及条件选择；最终默认 Best-Fit，`BUDDY=1` 时选择 Buddy |
| `code/Makefile` | 添加 Buddy 编译开关、独立宿主测试目标 |
| `code/tools/buddy_host/cases.c` | 使用真实管理器接口测试正常行为和非法输入 |
| `code/tools/buddy_host/platform.c`、`run.py` | 适配宿主输出与断言，隔离运行并自动判定结果 |
| 设计与测试说明 | 接口约定、测试覆盖和运行结果已并入本报告，完整提示词见 `prompt.md` |

未修改公共 `Page`、`pmm_manager` 结构、地址转换接口，以及原有 First-Fit、Best-Fit 测试和评分规则。

#### 最终提示词

本任务使用两份提示词：[Buddy 实现与接入](prompt.md#b-buddy-implementation)、[Buddy 独立测试](prompt.md#b-buddy-tests)。完整文本保存在 `prompt.md` 的对应任务下。

两份提示词均采用 `[PROMPT]`、`[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]` 四部分：

- **实现提示词：** 写明新增文件和必须修改真实工程，列出六个管理接口、注册对象及构建开关。`[RELY]` 使用实际 `pmm.h`、`memlayout.h`、`libs/list.h` 的定义；`[SPECIFICATION]` 规定登记、分配、释放、失败及检查的前后条件和 Case，尤其明确原始请求与实际占用的区别。
- **测试提示词：** 在分配契约确定后编写，要求直接链接真实 Buddy 源码，通过公开回调验证。列出测试目标和允许修改范围，不允许改算法或原有测试来迁就预期。各 Case 写明输入、预期计数、地址范围及失败判据。

初次实现时提示词中的路径以独立 Lab2 工程为根目录，例如 `kern/mm/buddy_pmm.c`；小组仓库中的对应路径为 `code/kern/mm/buddy_pmm.c`。原始提示词保留当时的路径和默认管理器状态，最终接入采用本节说明的 Best-Fit/Buddy 选择方式。

#### 实现迭代过程

本任务按模块实现、独立测试和内核联调三个阶段完成，下面分别记录。

---

##### 第一次迭代：实现分配器与可选接入

**遇到的问题：**

AI 助手直接调用 WSL 时出现 `Wsl/E_ACCESSDENIED`，无法在该终端执行目标编译。开发初期，完整启动还需要其他成员完成初始化，因此不能只凭模块生成成功就认定内核已经可以运行。

**问题解决策略：**

按提示词限定范围完成 Buddy 文件和条件接入，不补写 `page_init()`，不改公共结构。先检查真实源码，再由本人在 VS Code 的 WSL 终端编译 Buddy 单元：

```bash
make BUDDY=1 obj/kern/mm/buddy_pmm.o
```

提示词在实现前明确了三个容易混淆的条件：按实际二次幂页数记账、用原始请求长度释放、按绝对物理页号判断伙伴。它们是前期设计约定，并不是生成后发现的算法故障。

**最终结果：**

- Buddy 独立编译通过。
- 管理器注册和 `BUDDY=1` 可选接入完成。
- 保留默认管理器及其他成员的修改范围。

---

##### 第二次迭代：编写并运行独立测试

**测试设计：**

使用原生 GCC 将真实 `buddy_pmm.c`、测试用例和宿主适配分别编译、链接，不通过包含分配器 C 文件访问内部静态变量。测试单独维护登记位图、占用位图和预期空闲数，并核对管理器返回结果。

正常和边界场景如下：

| 用例 | 覆盖内容 | 检验作用 |
| --- | --- | --- |
| `empty` | 未登记时申请 | 检查空状态及失败处理 |
| `orders` | 1 至 32768 页的各阶 | 检查块大小、物理对齐和计数恢复 |
| `rounding` | 3、5、9 页请求 | 检查实际占用与原始释放长度 |
| `split` | 32 次单页分配、耗尽、乱序释放 | 检查分裂、不重复分配及整块恢复 |
| `merge` | 分阶段恢复 2、4、8 页 | 检查逐级合并，而不只看空闲总量 |
| `nonpartners` | 相邻但不是伙伴的两块 | 防止错误合并 |
| `irregular` | `[3,16)`、`[5,19)` | 检查不规整区间不丢页、不越界 |
| `registration` | 多次逆序登记和空洞 | 检查跨登记合并及禁止跨洞合并 |
| `failure` | 超总量、极大请求 | 检查失败后计数、描述符和后续操作 |
| `interleave` | 四轮、每轮 256 步混合操作 | 检查存活区间、计数和反复分配回收 |

其中 `nonpartners` 在八页区间中保留外侧两个两页块，只释放中间 `[2,4)`、`[4,6)`。此时总空闲量是四页，但申请四页应失败；释放外侧块后才能恢复整个八页块。这个用例可以发现仅检查“相邻且同大小”就合并的错误。

另外设置 18 个隔离异常场景，覆盖零页申请，零长度、NULL、越界、未对齐、非首地址、错误长度、重复、保留页及未登记页释放，以及零长度、越界、非保留页、重复、重叠登记和容量超限。请求三页却传四页释放也单独测试。

**问题处理与判定方法：**

异常用例不能只凭非零退出判为通过，否则段错误、超时或测试自身断言也可能混入。每个用例在独立进程运行，非法操作前输出 `EXPECTED_OPERATION`；驱动要求退出码为 86，且断言来自真实 `buddy_pmm.c`、表达式与当前用例一致。正常返回、无关断言和进程启动失败均判为失败。

同阶返回顺序没有约定，测试不强制最低地址；需要精确地址时，先识别返回块或构造唯一候选。交错用例固定种子为 `0x2badd00d`，便于复现。内容模式写入独立宿主数组，用于检查模型中的区间隔离，不作为真实物理内存或 MMU 验证。

**最终结果：**

```bash
make buddy-host-test
```

本人在 WSL 运行后得到：

```text
RESULT: 28/28 passed
```

10 个正常场景及 18 个异常场景全部通过。在本轮编译和测试中，没有发现需要修改 Buddy 分配算法的问题。异常场景触发预期断言，属于拒绝非法调用的结果。

---

##### 第三次迭代：接入内核并完成 SLUB 联调

**遇到的问题：**

1. 合并 A 的代码时，`pmm.c` 的管理器选择发生冲突：A 将默认管理器改为 Best-Fit，本人的版本增加了 Buddy 条件选择。
2. 接入 C 后，旧工具链在编译 `entry.S` 时报告无法确定 `zicsr`、`zifencei` 扩展的默认版本，客体构建被阻断。
3. SLUB 大对象可能请求三页，Buddy 实际占用四页，需要确认上层释放时传回的长度符合本模块要求。

**问题解决策略：**

1. 合并时保留两种选择：定义 `LAB2_BUDDY` 时使用 `buddy_pmm_manager`，否则使用 `best_fit_pmm_manager`。不改 A 的初始化和 Best-Fit 测试。
2. 根据报错核对 Makefile，在本人的 GCC 10.2.0 环境使用命令行 `ARCH_FLAGS=`，恢复此前可用的默认架构参数。随后重新编译、运行，不修改分配算法或队友的默认配置。
3. 核对 SLUB 保存的是原始申请页数，释放三页请求时仍传三页；运行大对象测试，并在 SLUB 全部释放后再次检查 Buddy 状态和空闲页数。

这一阶段补充了集成运行条件，没有因测试失败而修改 Buddy 分配或合并逻辑。每次切换构建配置前均先执行 `make -C code clean`。

**最终结果：**

| 验证项目 | 实际结果 |
| --- | --- |
| Buddy 完整构建与 QEMU 启动 | 管理器名称正确，`buddy_check()`、`check_alloc_page()` 通过 |
| 接入 A、C 后的 Buddy 宿主测试 | 28/28 通过 |
| Best-Fit＋SLUB | 检查通过，释放后恢复 31928 个空闲页 |
| Buddy＋SLUB | 检查通过，释放后恢复 31792 个空闲页，随后 Buddy 基础检查再次通过 |
| Best-Fit＋SLUB＋MMU 探针 | 检查通过，释放后恢复 31927 个空闲页 |
| 默认配置 `make -C code ARCH_FLAGS= grade` | Page Init、Best-Fit 均为 OK，总分 20/20 |

20/20 对应原有页初始化和 Best-Fit 评分；Buddy 的行为验证依据为 28 项宿主测试和上述客体联调。不同配置的静态占用不同，回收检查比较同一配置的前后页数，不要求各配置基线相同。

**关键改进点总结：**

1. 在生成代码前确定申请、实际占用和释放长度，测试与 SLUB 接入使用同一约定。
2. 将分配器实现与测试分开，使用独立预期值和精确断言判定，避免测试只重复实现逻辑。
3. 分别记录单元编译、宿主测试和 QEMU 联调结果，结合实际管理器名称与回收计数确认构建配置。


### 功能模块二拓展：任意大小内存单元 SLUB 分配

#### 任务一：模块设计、实现与内核接入

**负责人：** 2413921 程子涵。

##### 模块功能描述

需要实现的接口位于 `code/kern/mm/slub.c` 和 `code/kern/mm/slub.h`：

```c
void slub_init(void);
void *slub_alloc(size_t size);
int slub_free(void *ptr);
size_t slub_object_size(const void *ptr);
size_t slub_owned_pages(void);
```

`slub_check()` 由任务二实现，本任务保留声明和条件接入。共享文件为 `code/kern/init/init.c` 的 SLUB 条件调用和 Makefile 的显式开关；A/B 的 PMM 实现、Page 布局和原有测试不改变。

完整实现规格和独立测试要求见[综合提示词](prompt.md)的任务一、任务二。本实现保留简化 SLUB 的两层思想，不包含 Linux 的 per-CPU slab、NUMA、并发同步或调试红区。

`slub_alloc(size)` 首先拒绝 0，按最小适配类别选择对象大小。在对应链表中寻找未满 slab，通过 bitmap 找到空槽后置位、增加 used 并返回 16 字节对齐的槽地址。未满 slab 不存在时，`new_slab` 向公共 PMM 取一页并在页首建立元数据。RV64 下头部 sizeof=72，对齐后占 80 字节，例如 16 字节类别能放 251 个对象。

超过 2048 字节的请求由单独的连续多页块承载，页数为 `ceil((size+header)/PGSIZE)`；记录原始 page、npages 和对象长度。请求 4096 字节时头部也占空间，因此需要两页，不能只取一页。计算前检查加法溢出；PMM 返回 NULL 时不登记任何新 slab。

`slub_free(ptr)` 先遍历可信的活动 slab 元数据，找到范围后验证是否正好位于槽起始处以及该槽是否占用，避免先把任意地址当作 slab 头解引用。释放槽后若 used 归零，则先保存页描述符、请求页数和后继，摘链，再调用 free_pages。归还后不再访问该页。NULL 成功返回，内部指针、未重分配前的重复释放和未知地址返回 -1。

`slub_init` 是幂等初始化，重复调用不会清空正在使用的对象。`slub_owned_pages` 给出本模块尚未交还 PMM 的请求页数，`slub_object_size` 用于查询有效对象类别/请求长度。实现不改变 struct Page，不占用 First-Fit/Best-Fit/Buddy 的私有标志位。

与 B 的 Buddy 契约核对后，3 页请求实际占 4 页，但必须仍以 `free_pages(page, 3)` 释放。SLUB 的 `npages` 保存原始请求数，符合该契约。`slub_owned_pages()` 统计的是请求页数，不包含 Buddy 额外舍入占用；测试以释放后 `nr_free_pages()` 恢复和 Buddy 不变量检查共同验证无泄漏。8193 字节大对象加头部后请求 3 页，最终 Buddy＋SLUB 实跑覆盖了这一非二次幂路径。

当前遍历链表和 bitmap 的复杂度不要求常数；释放也会扫描活动 slab。可通过 partial/full 链表、页归属索引或 per-CPU cache 改善，但需要额外元数据和同步。即时回收空页减少长期占页，也会在频繁分配释放时增加页级操作。

##### 实现规格

全文见 [综合提示词任务一](prompt.md#task-1)，采用 `[PROMPT]`、`[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]`。RELY 按真实 `pmm.h` 的 uintptr_t 签名和已注释的 KADDR 核对；GUARANTEE 列出五个实现接口、白名单和禁止改动项；SPECIFICATION 逐个规定初始化、分类、大对象、溢出、失败、非法释放、回收与查询的前后条件。明确要求修改真实文件，不仅展示代码。

##### 实现迭代过程

早期验证使用与原页接口兼容的隔离 First-Fit；用户提供 A/B 分支后，改为最终初始化和管理器联调。核对 B 的原始请求页数释放契约，保留 SLUB 的原始 npages；此处是集成约束核对，不声称发现并修复了不存在的 Buddy 算法错误。最终结果和可观察边界见第五章。

#### 任务二：独立测试与运行证据

**负责人：** 2413921 程子涵。

##### 模块功能描述

内核接口为 `void slub_check(void)`，实现位于 `code/kern/mm/slub_check.c`。宿主 `code/tools/member_c/host_test.c` 提供 main、PMM 模型、cprintf 和 __panic 的适配定义，直接链接真实 SLUB；这些定义不进入内核链接。run_host.sh 开启 ASan/UBSan，run_qemu.py 检查真实管理器与 PMM/SLUB 标记。

![成员 C 早期宿主 SLUB 测试](images/01-slub-host.png)

图 1：10 月 18 日在当前成员 C 工作树运行 `make member-c-host` 的宿主测试结果。类别边界、复用、回收、PMM 失败与碎片化测试通过，空闲页恢复到 256。该测试使用宿主 PMM 模型，QEMU 集成结果见第五章。

##### 实现规格

全文见 [综合提示词任务二](prompt.md#task-2)。按公开 API 独立验证类别、16 字节对齐、不重叠、逐字节内容、幂等初始化、槽复用、320 对象跨页、四轮回收、异常输入、底层失败和碎片化；分别规定内核和宿主 Case。禁止读取私有 bitmap 或改算法/原测试凑通过。

##### 实现迭代过程

分类判断从“容量大于请求”收紧为精确类别契约；保留 anchor 后复用 hole，排除底层 PMM 恰好重用同一页的干扰。最终分别切换 Best-Fit/Buddy，并在 SLUB 回收后再检查底层不变量。当前独立提示词属于规范化整理稿，实际结果来自本次执行观察。

### 调试模块：QEMU 地址翻译与 TLB

#### 调试任务一：关键调用路径、分支与真实访存

**负责人：** 2413921 程子涵。

##### 模块功能描述与涉及接口

`void mmu_probe(void)`、探针数据和两个汇编标签；capture_debug.py / record_environment.py 的 CLI。QEMU 的 get_physical_address、riscv_cpu_tlb_fill 等均为只读观察目标，不修改接口。

本次确认的数据读取慢路径为：

```text
客体 ld → TCG 生成的 x86 代码
       → helper_le_ldq_mmu
       → load_helper
       → [主 SoftTLB 不命中，victim 也未命中]
       → tlb_fill → riscv_cpu_tlb_fill
       → get_physical_address
       → 权限/PMP 检查 → tlb_set_page
       → 对实际宿主内存完成读取
```

双 GDB 调试观察到的调用栈显示：`load_helper` 在 `accel/tcg/cputlb.c:1248` 附近计算 mmu_idx、索引和按访问类型选择的 tag，1271 行附近判断主表是否命中，失败后检查 victim 表，再调用 tlb_fill。不能把所有读取都画成必然调用 C helper：正常 TCG 快路径直接执行生成的宿主指令，参见下文“调试任务三”。

![首次数据读取的慢路径与无效 TLB tag](images/03-tlb-miss.png)

图 2：10 月 17 日早期调试画面显示目标数据读取首次进入 `tlb_fill`，索引为 8，目标 tag 尚无效。它记录的是当时的隔离运行过程。

`target/riscv/cpu_helper.c:155` 附近的 `get_physical_address` 依次处理以下分支：

1. M 模式数据访问可能因 MPRV 使用 MPP 指定的有效特权级；M 模式或没有 MMU 特性时走恒等映射。
2. 通过 satp.MODE 选择 Bare、Sv32、Sv39 等。Bare 返回同地址，Sv39 设置 levels=3、ptidxbits=9、ptesize=8。
3. 检查地址的高位是否符合 Sv39 符号扩展；不规范地址翻译失败。
4. 逐层取 PTE，V=0 失败，R/W/X 全 0 则将 PPN 当作下级页表基址；遇到叶子则进行非法权限组合、U/S、SUM/MXR、超级页对齐及访问权限检查。
5. 必要时更新 A/D。成功后计算物理页基址和权限，再由调用者检查 PMP 并安装 SoftTLB；失败则返回 probe 失败或注入访问/页异常。

这里描述的是 QEMU 4.1.1 的代码，不能直接套用更新版本的函数名或新特权规范分支。



##### 实现规格

完整提示词见 [综合提示词任务三](prompt.md#task-3)。先校验源码与二进制，再用客体 ld 触发宿主条件断点。Case 区分真实数据读、取指、调试器旁路、环境失败和重新链接；禁止修改 QEMU 或生成替代 PMM。

##### 实现迭代过程

上述结论按源码分析与实际观察分别标注。最终内核重新链接后的 PC 从符号自动取得；历史调试观察与最终集成结果分开陈述，不把旧地址说成最终内核地址。没有实际触发的分支或故障不写成已经动态验证；相关问题处理汇总于任务六。

#### 调试任务二：单步页表翻译与手工核算

**负责人：** 2413921 程子涵。

##### 模块功能描述与涉及接口

capture_debug.py 的 --step 入口。观察 get_physical_address 的 env、physical、prot、addr、access_type、mmu_idx，不修改 QEMU C 接口。

10 月 17 日调试记录中的第一条指令为 `0xffffffffc02010f4: ld a3,0(a5)`，下一条为 `0xffffffffc02010f6: ld a4,0(a5)`。原稿另记录了重新链接后的 PC；本次可复核的最终截图 images/11-final-gdb-trace.png 显示两条 PC 为 `0xffffffffc0200f8e`、`0xffffffffc0200f90`，均通过 s0 指向 `mmu_probe_word`。早期截图通过 a5 寻址。它们属于不同构建的现场，VA、satp、PTE 与下面手算值一致，不能混作同一组 PC/寄存器。自动脚本从当前内核符号取地址，不硬编码旧记录中的 PC。两条指令的访问类型均是 `MMU_DATA_LOAD=0`，有效特权模式为 S，mmu_idx=1。

| 变量 | 实测值 |
| --- | --- |
| VA | `0xffffffffc0208000` |
| satp | `0x8000000000080206` |
| MODE / ASID / 根 PPN | 8（Sv39）/ 0 / `0x80206` |
| 根页表 PA | `0x80206000` |
| VPN[2] / VPN[1] / VPN[0] / 页内偏移 | 511 / 1 / 8 / 0 |
| 实际 PTE 读取地址 | `0x80206ff8` |
| PTE | `0x200000cf` |
| PPN / ptshift / i | `0x80000` / 18 / 0 |
| 输出物理页基址 | `0x80208000` |
| prot / 返回状态 | 7（R/W/X）/ 0（成功） |
| 两次实际读取结果 | 均为 `0x1122334455667788` |

核算过程如下：

```text
root = (satp & ((1<<44)-1)) << 12 = 0x80206000
idx  = (VA >> 30) & 0x1ff = 511
PTE 地址 = root + 511*8 = 0x80206ff8
PTE PPN  = 0x200000cf >> 10 = 0x80000
PTE flags = 0xcf = V | R | W | X | A | D
```

![根页表索引与 PTE 地址](images/04-page-walk.png)

图 3：早期单步画面中 `levels=3`、`i=0`、`ptshift=18`，根索引为 `0x1ff`，PTE 地址为 `0x80206ff8`。

根级 PTE 已含 R/X，是 1 GiB 超级页叶子，不是下级页表指针。PPN 的低 18 位为 0，满足超级页对齐；因此循环在 i=0 就成功返回，虽然 levels=3，也不会再取 VPN[1]、VPN[0] 对应的页表项。

QEMU 对超级页构造 4 KiB 粒度的结果：

```text
vpn = VA >> 12
physical_page = (PPN | (vpn & ((1<<18)-1))) << 12
              = (0x80000 | 0x208) << 12
              = 0x80208000
PA = physical_page | (VA & 0xfff) = 0x80208000
```

也可用映射起点交叉检查：该根项把 `0xffffffffc0000000` 起的 1 GiB 映射到 `0x80000000`，故 VA 的 `0x208000` 偏移得到同样结果。`get_physical_address` 在分页成功路径返回物理页基址；本次偏移恰为 0，不能据此声称该函数在所有输入下都会直接返回完整字节地址。

宿主 GDB 单步观察到局部变量、PTE 读取后的分支和函数返回值。刚停在 `ppn = ...` 这一行时赋值尚未执行，ppn 显示的是未初始化值；next 后才成为 524288。finish 返回至调用表达式时 `ret` 的赋值同样可能尚未完成，应看返回值 0 和已写入的 pa/prot，或再单步至下一行。因此不能把赋值执行前的旧值当作算法结果。


levels=3，实际 i=0、ptshift=18，根索引为 `0x1ff`，PTE 地址为 `0x80206ff8`。尚未将“支持三级”误写为“实际遍历三级”。


单步后 PPN=`0x80000`，函数成功返回，下一行可见 PA=`0x80208000`、prot=7。上述 PTE 与 PA 数值和手工计算结果一致。

![页表项、PPN 与物理地址结果](images/05-page-result.png)

图 4：早期单步结果显示 PTE=`0x200000cf`、PPN=`0x80000`，函数返回状态为 0、PA=`0x80208000`、prot=7，与手算一致。

##### 实现规格

完整提示词见 [综合提示词任务四](prompt.md#task-4)。Case 覆盖根表、真实 PTE、根级超级页、PA 组合、赋值时机和与预期不同的映射；单步与公式必须互相核对，不预设每次走满三级。

##### 实现迭代过程

上述结论按源码分析与实际观察分别标注。最终内核重新链接后的 PC 从符号自动取得；历史调试观察与最终集成结果分开陈述，不把旧地址说成最终内核地址。没有实际触发的分支或故障不写成已经动态验证；相关问题处理汇总于任务六。

#### 调试任务三：TLB 查询、未命中、填充与命中

**负责人：** 2413921 程子涵。

##### 模块功能描述与涉及接口

capture_debug.py 中 Query.stop、Fill.stop、DebugStop.stop 和 trace CLI；只读 load_helper、tlb_hit、victim_tlb_hit、tlb_set_page、tcg_out_tlb_load。

QEMU 4.1.1 中可以找到 C 代码：`cputlb.c` 的 `load_helper` 计算索引并用 `tlb_hit` 比较 tag，`victim_tlb_hit` 检查被替换表项，`riscv_cpu_tlb_fill` 调用页表翻译，`tlb_set_page` 安装结果。表项按访问类型有 addr_read、addr_write、addr_code，以及宿主地址换算 addend。

本次 SoftTLB 的 fast.mask=2016，即 `63*32`，表项大小 32，故共有 64 个直接索引槽，当前访问的索引为 `(VA>>12)&63=8`。这是本次运行快照，不是 QEMU 所有配置固定为 64 项。

| 观察时刻 | 读 tag | 说明 |
| --- | --- | --- |
| 第一条 ld 前 | `0xffffffffffffffff` | 无效表项 |
| 第一次 C 查询 | 同上，code_read=false | 数据读取，主表未命中 |
| 第一次 ld 完成 | `0xffffffffc0208000` | 已填充，权限为 R/W/X |
| 第二次 ld 完成 | 同上 | 两次均读取正确值，未再次进入目标地址的 refill/walk |

第一次 ld 的真实调用栈包含 victim 检查后的 tlb_fill，说明本次 victim 也没有提供匹配项。日志只出现一次目标地址的真实数据页表遍历，而客体日志确认两条 ld 都执行结束。

第二次访问不一定再次进入 load_helper，所以只在 C helper 上打断点，不能观察每次命中。x86_64 的 `tcg/i386/tcg-target.inc.c:1699` 中 `tcg_out_tlb_load` 会生成取索引、比较 tag、`jne slow_path` 和加 addend 的宿主机器码。匹配时直接访问 `guest_VA + addend` 对应的宿主 RAM 地址。它缓存的不仅是客体 VA→PA 关系，还包含把访存快速导向宿主内存的资料。两次读取均得到相同数据，目标慢路径只发生一次；该结果与 TCG 快路径源码相符。本次没有测量真实硬件命中耗时。

![第二次加载后的 SoftTLB 与寄存器](images/07-tlb-second-load.png)

图 5：早期调试快照中第二条 `ld` 执行后，目标 tag 仍在槽 8，两个寄存器值均为 `0x1122334455667788`，慢路径计数为 1。

![客体 GDB 的两条 ld 与读取结果](images/08-guest-two-loads.png)

图 6：客体 GDB 显示两条 `ld` 依次完成，a3、a4 读取结果相同。该画面与宿主 TLB 观察相互印证。

自动脚本只把调用者为 `riscv_cpu_tlb_fill` 的 get_physical_address 记为 REAL_WALK，避免把客体 GDB 的 `x`、断点设置或源码读取造成的 `riscv_cpu_get_phys_page_debug` 路径混入。取指使用 addr_code / MMU_INST_FETCH，不能和数据 load 的 addr_read 混淆。


填充后 addr_read/write/code 均为目标虚拟页，addend 用于换算宿主地址，目标数据慢路径计数为 1。addend 不能解释为客体物理页号。


PC 前进到 `0xffffffffc02010f8`，gpr[13]/gpr[14] 都为 `0x1122334455667788`，目标慢路径计数仍为 1，说明第二次没有重新遍历页表。



##### 实现规格

完整提示词见 [综合提示词任务五](prompt.md#task-5)。三次快照、两条指令和一次目标慢路径共同验证；Case 明确 miss/fill/hit、容量解释及证据缺失。禁止写表项制造命中，禁止把 addend 当客体 PA。

##### 实现迭代过程

上述结论按源码分析与实际观察分别标注。最终内核重新链接后的 PC 从符号自动取得；历史调试观察与最终集成结果分开陈述，不把旧地址说成最终内核地址。没有实际触发的分支或故障不写成已经动态验证；相关问题处理汇总于任务六。

#### 调试任务四：Bare 模式与真实 CPU TLB 的逻辑区别

**负责人：** 2413921 程子涵。

##### 模块功能描述与涉及接口

capture_debug.py 的真实 Bare store 观察分支；entry.S 的 sd、store_helper 与翻译函数均只读，不新增 C API。

客体在 `0x80200008` 执行 `sd a0,0(t0)`，保存 hartid。此时 satp=0，t0=`0x80207000`。日志记录：

```text
REAL_WALK addr=0x80207000 satp=0 access=1 mmu_idx=1
WALK_RETURN result=0 physical_page=2149609472 prot=7
FILL addr=2149609472 pa=2149609472 ret=0 prot=7 access=MMU_DATA_STORE
```

即 Bare 数据存储仍通过软件慢路径完成恒等翻译并填入 QEMU SoftTLB，没有访问页表。另有 satp=0 时对 `0x80200000` 的真实取指记录，但数据存储更便于与 Sv39 数据加载比较。

![Bare 模式真实数据存储的恒等翻译](images/02-bare-store.png)

图 7：早期双 GDB 画面显示 satp=0、数据存储 access_type=1，慢路径返回的 PA 与输入地址相同。

真实 RISC-V 在 Bare 模式下对这些访问不做分页地址翻译，但仍受特权、PMP 等物理访问规则约束；硬件是否复用某种内部缓存结构属于微架构选择。QEMU SoftTLB 则是模拟器加速全部被模拟内存访问的软件缓存，即使客体关闭分页，也需要把客体地址导向宿主 RAM、MMIO 或其他 MemoryRegion。因此不能把“QEMU Bare 仍查询 SoftTLB”解释为“硬件 Bare 也必须走分页 TLB”，也不能把软件直接索引、victim 策略或当前容量当成真实 CPU 的结构。


satp=0、access_type=1，调用栈为 store_helper 的慢路径，返回 PA=`0x80207000` 与输入相同；支持“未启用分页时仍有 QEMU 软件地址缓存”这一比较。

##### 实现规格

完整提示词见 [综合提示词任务六](prompt.md#task-6)。Case 明确 Bare sd、恒等地址结果、Sv39 对照、保护约束和软件/硬件区别；不以入口取指替代数据存储，不手动改 satp 制造结果。

##### 实现迭代过程

上述结论按源码分析与实际观察分别标注。最终内核重新链接后的 PC 从符号自动取得；历史调试观察与最终集成结果分开陈述，不把旧地址说成最终内核地址。没有实际触发的分支或故障不写成已经动态验证；相关问题处理汇总于任务六。

#### 调试任务五：实际观察与理解变化

**负责人：** 2413921 程子涵。

##### 模块功能描述与涉及接口

报告对应小节，无需新增 C/Python 接口。

首先，Sv39 有三个地址索引不意味着每次都读三个 PTE。本内核的汇编注释说“三级页表”，但实际只布置了根页表的第 511 项，一个叶子就覆盖 1 GiB。必须看叶子条件与实际循环次数。

其次，断在某个 C 赋值行不等于该行已经执行。单步日志中的 ppn 和 ret 一度显示旧值或垃圾值，结合当前源码行与 next 后结果，才能判断何时变量有效。

再次，第二条 ld 没有再次触发 load_helper 断点并不意味着没有访存；寄存器结果、SoftTLB 表项和 TCG 生成器共同解释了命中快路径。此前如果只跟踪 MMU 翻译函数，就只能看到未命中。

最后，客体暂停和 QEMU 进程暂停是不同状态。宿主 GDB 持有 QEMU 时，客体 GDB 等待 stub 应答属于预期配合。自动脚本用两个独立调试器复现这一关系，而不是把两个目标混在一个 GDB 架构里。

##### 实现规格

完整提示词见 [综合提示词任务七](prompt.md#task-7)。每个现象按观察、原理解、证据、修正展开；Case 包含三级与叶子、局部变量赋值、TCG 快路径、双 GDB 等待和地址版本，不编造趣事。

##### 实现迭代过程

上述结论按源码分析与实际观察分别标注。最终内核重新链接后的 PC 从符号自动取得；历史调试观察与最终集成结果分开陈述，不把旧地址说成最终内核地址。没有实际触发的分支或故障不写成已经动态验证；相关问题处理汇总于任务六。

#### 调试任务六：问题定位与记录

**负责人：** 2413921 程子涵。

##### 模块功能描述

本任务不新增代码接口，修改报告中的真实问题记录。用户先要求结合指定 A/B 分支完成成员 C 的功能、选图和清理，再要求按照仓库标准格式和七项检查逐任务重写提示词。以下记录问题和处理意图，不伪装成历史逐字聊天。

| 问题情景 | 思路与提问意图 | 实际处理 | 验证依据 |
| --- | --- | --- | --- |
| 集成基线选择 | 对照 A 的 `lab2` 与 B 的 `lab2-yang` | 以包含 A 成果的 `lab2-yang` 为底座，接入 C 的 SLUB 与探针 | 最终源码与三种配置的运行结果 |
| 单看翻译函数不足以证明第二次 hit | 需要同时看到指令完成、TLB 状态与慢路径次数 | 使用三次快照、两条 ld 的值及 TCG 源码交叉核对 | host/guest trace 结果与指令观察 |
| Bare 对照仅靠取指不充分 | 找启动阶段确定会执行的数据存储 | 观察保存 hartid 的真实 sd，过滤地址与访问类型 | satp=0、access=1 的 REAL_WALK/FILL |
| 切换宏可能复用旧对象 | 必须确认实际构建配置而非只看命令 | 各配置先 make clean，检查运行管理器名称 | 各配置的构建与启动结果 |
| Buddy 请求 3 页实际占 4 页 | 核对释放应传实际占用还是原请求 | 按 B 契约保留原始 3 页，执行大对象及回收后的不变量检查 | Buddy＋SLUB 恢复 31792 页 |
| 首次 make grade 被 CRLF 阻断 | 区分 shell 解析错误和分配器错误 | 统一 LF 后重跑，保留评分规则 | 评分结果 20/20 |
| 提示词需要细化 | 逐任务核对接口、范围和 Case | 按仓库格式整理八项任务并合并到单份提示词 | 本报告与 `prompt.md` |

##### 实现规格

全文见 [综合提示词任务八](prompt.md#task-8)。Case 明确历史与当前状态、提问意图和逐字原话、源码分析与实跑、代码 bug 与环境问题、没有实际 bug 时的表述，以及未知模型版本的处理。禁止编造 A/B 或本人经历。

##### 实现迭代过程

最初的短提示词按功能概述给出约束；本次对照标准文件补为八份独立任务。每份写明目标文件、必须修改真实文件、全部适用接口、真实 RELY、白名单/禁区、明确前后条件与 Case、系统限制。此次只调整文档，原有测试结果按实际执行记录整理；文档引用检查不被称为重新运行 QEMU。

没有将未出现的分配器越界、泄漏或重复发放写成“实际修复过的 bug”。B 的原稿已包含 A+B 联调，本次新增的是 C 的对象层与最终调试集成，不把 B 原稿误述为始终只有宿主测试。

---


#### 本次整合时对调试证据和工具的补充说明

C 的最终 trace 图可见 Bare 的 access=1 数据存储、目标 VA 的一次 REAL_WALK/PTE/FILL，以及客体两次 load 的正确寄存器值。早期三次快照和慢路径计数图另行保留；原始完整 trace 日志未随仓库提交，本报告不声称可以在交付目录直接找到这些历史日志。

现有 capture_debug.py 负责采集两种 GDB 的输出，不能只凭脚本退出成功判断全部语义通过；run_qemu.py 主要检查管理器/PMM/SLUB 标记和 panic，也不能作为所有提前异常退出的完整判据。本次采用已有图文与人工核对，没有在文档整合阶段擅自修改调试工具或评分器。提示词描述的是要求，实际实现和验证边界应分别陈述。



## 五、测试与验证

### 5.1 A 的初始化、Best-Fit 证据

A 报告记录构建通过、make test 得到 grading: 6 / 6 points，以及 make grade 为 20/20。其中 make test 没有单独截图，作为 A 的文字执行记录保留。下列两张原图分别支持默认管理器运行及评分结果，不将 20/20 扩展为 Buddy/SLUB 的分数。

![A：默认 Best-Fit 启动](images/Amake-qemu.png)

A 图 1：默认 Best-Fit，描述符 32768 个，空闲页 31929，check_alloc_page 成功。该图的静态页表 PA 为 0x80205000，属于该构建，不能替代 C 探针构建的 0x80206000。

![A：原评分](images/Amake-grade.png)

A 图 2：Page Init、Best-Fit 均为 OK，20/20。

### 5.2 B 在 A+B+C 工作区执行的回归

以下结果由 B 在 VS Code WSL 终端执行并反馈，已写入个人报告第十二节：

| 配置/测试 | 实际结果 |
| --- | --- |
| make -C code member-c-host | SLUB 两轮宿主检查通过，每轮恢复 256 页；失败注入与碎片化通过 |
| make -C code buddy-host-test | 28/28，通过正常与异常隔离场景 |
| make -C code ARCH_FLAGS= SLUB_CHECK=1 qemu | Best-Fit+SLUB，通过并恢复 31928 页 |
| make -C code ARCH_FLAGS= BUDDY=1 SLUB_CHECK=1 qemu | Buddy+SLUB，通过并恢复 31792 页，回收后 Buddy 检查通过 |
| make -C code ARCH_FLAGS= SLUB_CHECK=1 MMU_PROBE=1 qemu | Best-Fit+SLUB+探针，通过并恢复 31927 页 |
| make -C code ARCH_FLAGS= grade | check page init、check best fit 均 OK，20/20 |

B 每次切换客体配置前均 make -C code clean。QEMU 输出后进入已有 while(1) 是预期状态，使用 Ctrl+A、松开后 X 退出。清理构建会删除 obj 下 Buddy 日志，本报告保留输出摘录，不能据目录名推断历史日志仍然存在。

### 5.3 C 的测试、调试与最终集成证据

以下取自 C 的交付报告和配图，结果与 B 的重复回归分别归属；本次文档整理未重新执行这些命令。



#### C-5.1 SLUB 覆盖与结果

| 测试 | 覆盖内容 | 实际结果 |
| --- | --- | --- |
| 1、15、16、17 … 2048 及 2049/4095/4096/4097/8193 字节 | 类别边界、对齐和大对象页数 | 宿主与 QEMU 通过 |
| 多对象地址区间和逐字节内容检查 | 对象重叠、槽间距和数据完整性 | 通过 |
| 保留对象期间释放并重分配空槽 | slab 内槽复用 | 通过 |
| 320 个 16 字节对象及四轮交错释放 | 跨页增长与重复回收 | 通过 |
| 0、SIZE_MAX、NULL、栈地址、内部地址、重复释放 | 非法请求和释放边界 | 通过 |
| PMM 失败、页面耗尽和连续页碎片 | 失败原子性及大对象分配失败 | 宿主测试通过 |

宿主测试直接编译真实 `slub.c` 和 `slub_check.c`，底层使用 256 页可控 PMM，并开启 AddressSanitizer 与 UndefinedBehaviorSanitizer。两轮检查后可用页数恢复到 256。

最终 Best-Fit＋SLUB 在 QEMU 4.1.1 中通过初始化和对象检查，回收后剩余 31927 页。Buddy＋SLUB 回收后恢复到该配置的 31792 页基线；Buddy 将 3 页请求向上取整为 4 页，释放仍传原请求 3 页。

#### C-5.2 全工程验证

| 验证项 | 结果 |
| --- | --- |
| 默认 Best-Fit 启动 | PMM 检查通过，初始空闲页 31929 |
| Buddy 宿主测试 | 28/28，包含 10 个正常场景与 18 个预期拒绝场景 |
| Best-Fit＋SLUB＋MMU 探针 | SLUB 回收后恢复 31927 页 |
| Buddy＋SLUB（未启用 MMU 探针） | 回收后恢复 31792 页，前后不变量检查通过 |
| Buddy＋SLUB＋MMU 探针 | 回收后恢复 31791 页，Buddy 与 PMM 检查通过 |
| 原有评分 | Page Init 与 Best-Fit 均通过，20/20 |
| QEMU 地址翻译调试 | Bare 数据存储、Sv39 两次读取、一次目标慢路径及页表单步结果一致 |

20/20 仅覆盖原有评分项。Buddy 的 28 个场景在宿主执行，没有逐项在客体中运行。QEMU 内核保留框架原有等待循环，自动运行器在观察窗口结束后停止 QEMU。

#### C-5.3 最终集成截图

![最终 Best-Fit、SLUB 与 MMU 探针运行](images/09-final-bestfit.png)

图 8：10 月 18 日 Best-Fit＋SLUB＋MMU 探针的 QEMU 输出。显示实际管理器、PMM 检查、MMU 探针参数，以及 SLUB 回收后恢复 31927 页。

![最终 Buddy、SLUB 集成和 Buddy 宿主测试](images/10-final-buddy-host.png)

图 9：上半部分是 Buddy＋SLUB＋MMU 探针的 QEMU 检查（恢复 31791 页），下半部分是 Buddy 宿主测试的 `RESULT: 28/28 passed`。两项测试运行在不同层次，截图分别显示其结果。

![最终集成内核的双 GDB trace](images/11-final-gdb-trace.png)

图 10：10 月 18 日对最终集成内核进行双 GDB trace。宿主记录 Bare 访问和目标 Sv39 页表遍历，客体确认两条 `ld` 均读到 `0x1122334455667788`。

![最终页表步进结果与原评分](images/12-final-step-grade.png)

图 11：最终集成内核的单步结果显示 PA=`0x80208000`；随后原有 Page Init/Best-Fit 评分为 20/20。评分仅对应原有评分项。


### 5.4 复现命令与已验证范围

在仓库根目录使用 B 已验证的旧工具链配置：

```bash
make -C code member-c-host
make -C code buddy-host-test
make -C code clean
make -C code ARCH_FLAGS= SLUB_CHECK=1 qemu
```

退出 QEMU 后，切换 Buddy：

```bash
make -C code clean
make -C code ARCH_FLAGS= BUDDY=1 SLUB_CHECK=1 qemu
```

需要探针时，在干净构建后增加 MMU_PROBE=1。基础评分使用默认 Best-Fit：

```bash
make -C code clean
make -C code ARCH_FLAGS= grade
```

C 的双 GDB 复现另需在 Linux 中准备匹配源码的 QEMU 4.1.1 调试版、宿主 gdb 和客体 gdb-multiarch，并先干净构建 SLUB_CHECK=1 MMU_PROBE=1 的内核。code/tools/member_c/build_qemu.sh 可通过 QEMU_BUILD_ROOT 配置工程外构建位置，无需 make install。随后在 code 目录运行 capture_debug.py --qemu <调试版可执行文件> --source <QEMU源码目录>，添加 --step 切换源级单步；这只是复现入口说明，本次没有执行。

初始化无效范围、无完整空闲页、超上界裁剪等 Case 是规格和代码检查的一部分，没有对应实跑记录的不能标为动态通过。原 test_page_init 未考虑大 RAM 裁剪，因此不能直接用扩大 RAM 的运行替代默认评分；不修改测试掩盖差异。

## 六、实验总结与收获

### 6.1 重要知识点与 OS 原理的对应

| 知识点 | 本实验对应 | 联系、差异与限制 |
| --- | --- | --- |
| 内存发现与保留 | DTB、page_init、Page 数组 | 管理数据自身占内存；当前为固定基址单段模型，不是任意平台通用探测 |
| 连续物理页分配 | First-Fit、Best-Fit、Buddy | 连续虚拟地址与连续物理地址不同；总空闲页足够也可能没有连续块 |
| 内外部碎片 | Best-Fit 选最小适配块、Buddy 二次幂舍入 | Best-Fit 仍可能产生小余块，Buddy 有内部舍入成本，不能凭名称断言优劣 |
| 机制与策略分离 | pmm_manager 回调 | 公共接口一致不代表所有释放语义完全相同，SLUB 必须遵守当前管理器所有权契约 |
| 对象分配 | SLUB 类别、bitmap、空 slab 回收 | 减少小对象单独占页，但有头部和类别舍入；当前不是 Linux 完整 SLUB |
| Sv39 与超级页 | satp、VPN、PTE、根级叶子 | 支持三级不等于本次遍历三级；静态根项已覆盖 1 GiB |
| TLB 和缓存一致性 | sfence.vma、SoftTLB miss/fill/hit | QEMU 缓存还承担客体到宿主寻址加速，不等同于真实硬件微架构 |
| 测试与错误检测 | 公开 API、独立模型、断言、ASan/UBSan | 能发现已覆盖场景的问题，不能证明所有调用序列、并发或性能都正确 |

### 6.2 重要但本实验未实现的原理

按需分页、缺页调入、磁盘交换和页面置换；进程私有地址空间、写时复制与共享页引用协议；多核 TLB shootdown、并发页/对象分配、NUMA、物理页迁移和内存压缩；真实硬件页表遍历缓存与精确性能测量。这些内容不能由本次静态启动映射和串行分配测试推导为已具备。

### 6.3 AI 协作与实际问题处理

| 情景 | 处理与验证 | 记录边界 |
| --- | --- | --- |
| A 初期仅完成 page_init，Best-Fit 骨架导致构建失败 | 保留范围，不跨任务补写；后续 Best-Fit 完成再验证 | 早期阻塞不是 page_init 算法错误，原反馈保留在 prompt.md 的初始化部分 |
| A 的 Best-Fit 初始提示词约束不足 | 补地址顺序、状态、测试临时隔离和范围；既有检查通过 | 报告描述的部分是风险与规格迭代，不编造已经发生的每一种代码 bug |
| B 的 WSL 调用受限 | Codex 辅助生成与检查，B 在 WSL 实跑反馈 | 不把助手读文件说成直接完成 WSL 测试 |
| Buddy 原始请求与实际占用不同 | 先明确契约，再分别生成实现和独立测试，SLUB 保存原始页数 | 非法释放用例预期断言不是待修复故障 |
| A/B 合并的 pmm.c 选择冲突 | 保留默认 Best-Fit 与条件 Buddy，两种配置重验 | 不重写 A 初始化或原有测试 |
| B 旧工具链不接受 zicsr/zifencei 参数 | 传 ARCH_FLAGS=，组合运行和最终 20/20 通过 | 兼容问题，不是更改分配算法 |
| C 单看翻译函数不足以证明 hit | 同时看两条 ld、寄存器、表项和慢路径次数；区分 GDB 调试读取 | 采集脚本不是完整语义断言器，保留图文核对 |
| C 的早期 CRLF 评分问题 | C 报告记录转 LF 后重跑通过 | 不伪造 B 遇到同一问题，也不修改评分规则 |
| 总报告中各版本 PC/地址、人数和模板残留 | 对照当前文件与图片，清理模板，按运行配置标注 | 原稿在 Git 历史中保留，不修改提示词以追写历史 |

Spec 的价值是先确定地址、状态、所有权与失败行为；Prompt 把接口、依赖、修改范围和 Case 交给 AI；测试再独立检验这些结果。B 的两份提示词按实际使用版本保留，C 的规范化稿按其说明保留为整理稿；既不隐藏真实兼容问题，也不为报告数量编造算法 bug 或聊天原话。

### 6.4 文件与任务覆盖

| 要求 | 对应交付 |
| --- | --- |
| 整体逻辑与模块理解 | 总报告第三、四章 |
| page_init 与内存探测 Challenge | 第四章 A 部分、prompt.md A 初始化及其后的实现反馈 |
| First-Fit 与 Best-Fit 改进问题 | 第四章 A 部分及 Best-Fit 改进小节 |
| Buddy 两份提示词与测试有效性 | prompt.md B 两部分、第四章 Buddy、code/tools/buddy_host |
| SLUB 两份提示词与测试有效性 | prompt.md C 任务 1/2、第四章 SLUB、code/tools/member_c |
| QEMU 六项调试与 AI 记录 | 第四章调试六项、prompt.md C 任务 3—8、images/ 原图 |
| 真实结果、问题、知识对应与未覆盖内容 | 第五、六章 |

提交内容为实验代码、总报告、完整提示词及十三张实验图片。
