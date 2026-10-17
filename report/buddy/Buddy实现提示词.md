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
