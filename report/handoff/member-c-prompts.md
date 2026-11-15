# Lab4 成员三分任务提示词（仅代码部分）

负责人：于蕾（2411824）。依据当前代码、`Lab4_三人任务分配方案(1).md`、`LAB4.txt` 和 `3prompt.md` 编写。

按 `switch_to → proc_run → schedule → cpu_idle` 顺序执行，每节是一份可单独复制的提示词。所有路径均相对于仓库根目录，实际内核工程位于 `code/`。当前四个函数已有实现，应在真实文件中核对、完善；已满足的部分保留，不假设还存在 `YOUR CODE` 空白。本文件仅提供四个代码任务的提示词，不包含 Challenge 作答、报告整理或额外测试脚本任务。

## 任务一：实现 RV64 线程上下文切换 switch_to

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

### switch_to

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

## 任务二：实现安全的线程运行切换 proc_run

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

### proc_run

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

## 任务三：实现循环选择可运行线程的 schedule

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

### schedule

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

## 任务四：实现空闲线程循环 cpu_idle

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

### cpu_idle

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
