# 操作系统实验报告
## 实验基本信息
| 项目         | 内容                                  |
| ------------ | ------------------------------------- |
| **实验名称** | Lab X: [实验名称]                     |
| **小组成员** | 学号1-姓名1、学号2-姓名2、学号3-姓名3 |
| **完成日期** | YYYY-MM-DD                            |
### 小组分工
练习如何分工？
| 成员        | 负责的练习/模块  |
| ----------- | ---------------- |
| 学号1-姓名1 | [练习或模块名称] |
| 学号2-姓名2 | [练习或模块名称] |
| 学号3-姓名3 | [练习或模块名称] |
实验报告如何分工？
---
## 一、实验目的
<!-- 说明本次实验的主要目标和预期成果 -->
本实验的主要目的是：
1. [目的1：例如，理解用户态进程如何通过系统调用进入内核]
2. [目的2：例如，掌握进程创建、执行、退出的完整生命周期]
3. [目的3：例如，学习使用 AI 辅助开发操作系统核心功能]
---
## 二、实验环境
你们使用的 AI 工具
| 成员        | AI 编程工具                                                  | 底层模型                                                  | 备注               |
| ----------- | ------------------------------------------------------------ | --------------------------------------------------------- | ------------------ |
| 学号1-姓名1 | [例如：Cline（VS Code插件） / Claude Code（终端 Agent） / DeepSeek Harness等] | [例如：Claude Sonnet 4.5 / GPT-5.5 / DeepSeek-V4-Flash等] | [可选：特殊配置等] |
| 学号2-姓名2 |                                                              |                                                           |                    |
| 学号3-姓名3 |                                                              |                                                           |                    |
**说明：**
- **AI 编程工具**：指具体使用的终端工具、编辑器插件、桌面应用或浏览器界面
- **底层模型**：指该工具使用的大语言模型及版本
---
## 三、实验整体逻辑分析
### 2.1 本章节的逻辑主线
[请用自己的话描述本章实验的核心主线，例如：
- 本章围绕哪个核心主题展开？（例如：用户进程管理、虚拟内存、文件系统等）
- 整个实验解决什么问题？（例如：如何让用户程序运行在操作系统之上）]
### 2.2 功能的逐步实现
[说明本章实验是按照什么顺序逐步实现各个功能的，以及为什么要按这个顺序，例如：
1. 首先实现**功能A** → 为什么首先实现这个？（例如：建立系统调用接口是后续所有用户态功能的基础）
2. 接着再完善**功能B** → 为什么接着实现这个？（例如：有了系统调用后，需要能够加载和启动第一个用户进程）
3. 然后完成**功能C** → 为什么然后实现这个？（例如：有了第一个进程后，需要实现进程复制功能）
4. 最后补充了**功能D** → 为什么最后实现这个？（例如：最后完善进程退出和清理机制，形成完整闭环）]
---
## 四、实验内容与实现
<!-- 
说明：本部分按照 exercises 文档中的练习顺序组织
每个功能模块或者练习中可能包含多项任务，请根据实际情况调整
-->
### 功能模块一：物理内存初始化功能

**负责人：** 2411824－[姓名]

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

   `npage` 表示可管理物理页号的上界，不是 RAM 页面总数。上述数值为默认配置下的计算结果，尚未通过本次 QEMU 运行确认。

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

本任务实际使用的完整提示词保存在同目录的 [prompt1.md](./prompt1.md)。

提示词通过 `[RELY]` 列出真实工程接口和地址约定，通过 `[GUARANTEE]` 限定修改范围，通过 `[SPECIFICATION]` 描述正常初始化、无效设备树信息、范围裁剪、页面对齐、描述符空间不足及无完整空闲页等 Case。

其中明确要求修改真实文件，仅在 `pmm.c` 指定标记处实现函数，不修改其他模块或测试，并如实报告编译与运行验证结果。完整提示词以附件原文为准。

#### 实现迭代过程

本模块当前记录了一次实现迭代

---

### Challenge：硬件的可用物理内存范围的获取方法

**负责人：** 2411824－[姓名]

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
// 用于按地址插入空闲块并合并相邻块；现有结果报告未提供完整函数签名。

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

本次实现使用的最终提示词文件较长，仅在此给出部分内容，详情见[prompt.md](./prompt.md)的完整内容。

`````
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

`````


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

## 五、测试与验证


**测试命令及结果：**

在工程 `code` 目录下执行：

```bash
make
make qemu
make test
make grade
```

| 验证项目              | 实际结果                                                     | 依据                         |
| --------------------- | ------------------------------------------------------------ | ---------------------------- |
| 构建与链接            | 使用 `riscv64-unknown-elf-gcc 10.2.0`，在 `-Werror` 条件下通过，无警告。 | 执行结果报告。               |
| QEMU 内核运行         | 显示 `memory management: best_fit_pmm_manager`，并输出 `check_alloc_page() succeeded!`。 | 执行结果报告与图 1。         |
| 内核测试 `make test`  | `grading: 6 / 6 points`，分配器检查成功。                    | 执行结果报告，未附独立截图。 |
| 评分测试 `make grade` | `check page init: OK`、`check best fit: OK`，`Total Score: 20/20`。 | 执行结果报告与图 2。         |

**测试截图：**

![图1：QEMU 内核启动与分配器检查结果](./images/Amake-qemu.png)

图 1　QEMU 内核运行结果。截图包含 Best-Fit 管理器名称、物理内存登记信息及分配器检查成功信息。

![图2：make grade 评分结果](./images/Amake-grade.png)

图 2　`make grade` 评分结果。页面初始化与 Best-Fit 两项检查均为 `OK`，总分为 `20/20`。

**结果分析：**

图 1 中 `memory management: best_fit_pmm_manager` 表明管理器切换已经生效，后续分配与检查使用本次实现。内核登记了 `31929` 个空闲页面，并输出 `check_alloc_page() succeeded!`，说明初始化及已有分配器检查顺利完成。

最终提示词说明，原有 `best_fit_check()` 包含区分 Best-Fit 与 First-Fit 的用例：低地址存在 2 页空闲块、高地址存在 1 页空闲块时，请求 1 页应选择高地址的 1 页块。结果报告记录原有检查成功，图 2 进一步显示评分中的 `check best fit` 通过，共同支持本次实现满足已有测试对最优适配策略的要求。

`check page init` 与 `check best fit` 均通过，说明页面初始化和本次分配器在已有评分流程中能够正常配合。测试结果支持本实现通过当前提供的检查，不能据此声称已经穷尽所有非法释放、异常指针或长期运行场景。

---
## 六、实验总结与收获
### 对操作系统的理解
1. 列出你们认为本实验中重要的知识点，以及与对应的 OS 原理中的知识点，并简要说明二者的含义、关系和差异
2. 列出你们认为 OS 原理中很重要、但在本实验中没有对应上的知识点
### AI 协作开发的经验
[写出你们在与 AI 协作过程中实际收获的经验或者感悟]

---