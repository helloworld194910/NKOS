# Lab 4 成员 B 分任务提示词

01–06 分别对应六个代码任务。07–09 分别对应三道课堂拓展题。每段可单独复制使用，接口与路径以本实验包的当前代码为准。

## 01　初始化进程控制块

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

## 02　保留内核线程的地址空间接口

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

## 03　构造新线程的栈和首次恢复现场

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

## 04　实现创建、登记与失败回收

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

## 05　将普通 C 函数包装为内核线程

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

## 06　初始化 idleproc 与 initproc

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

## 07　Challenge 1：推导 PID 回绕与查重

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

## 08　Challenge 2：区分数字唯一性和对象身份

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

## 09　Challenge 3：核实课堂指定项目的 PID 复用机制

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
