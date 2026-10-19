# 模块二-任务二：BEST-FIT提示词
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