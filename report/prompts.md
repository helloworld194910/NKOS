# Lab5 提示词汇总

负责人：2412790-杨茜茹。实验：uCore Lab5 用户进程管理。

以下是按指导书、实际工程和本人实现整理的可复现提示词，依据是前置迁移提交 `6e9bc57` 和本人功能提交 `1d3fe34`。复现命令、实测结果及当前构建限制见 [实验报告第五章](report.md#verification)。

| 实验任务 | 负责人 | 完整提示词 |
| --- | --- | --- |
| Lab2–4 前置代码迁移与兼容 | 2412790-杨茜茹 | [迁移全队共用基线](#yang-prerequisites) |
| 模块一：用户态系统调用 | 2412790-杨茜茹 | [接通 Trap 与系统调用 ABI](#yang-syscall) |
| 模块四：退出、让出与终止 | 2412790-杨茜茹 | [实现进程控制与回收衔接](#yang-process-control) |

<a id="yang-prerequisites"></a>

## 任务一：Lab2–4 前置代码迁移与兼容

[PROMPT]

**任务**　在 Lab5 课程框架 `569be17` 上迁移本组 Lab4 `444c0c2` 已继承的 Lab2–4 实现，给全组提供同一套物理内存、页表、启动、Trap、内核线程和调度基础。复用可用的函数，按 Lab5 的结构和启动约定适配。后续首个用户进程和用户地址空间复制都依赖这套基线。

**操作要求**　必须修改工程中的实际文件。按下表定位已有的 `lab2`、`lab3`、`lab4` 标记和 Lab5 过渡位置。迁移前核对同名函数的签名、数据结构变化及来源提交。保留 Lab5 页表辅助函数、进程树、用户 ELF 构建、评分入口和既有保存现场逻辑。

**输出要求**　列出来源提交、实际落点和 Lab5 适配点。对这 8 个对象执行编译，报告真实结果，并列出仍依赖 Lab5 新模块的入口。

[RELY]

| 当前工程文件 | 已有依赖与约定 |
| --- | --- |
| `code/kern/mm/default_pmm.c`、`code/kern/mm/pmm.h`、`code/kern/mm/memlayout.h` | `struct pmm_manager`、`struct Page`、`free_area_t`、`PageReserved`、`PageProperty`、空闲链表和原 `default_check()` 约定。框架使用 `default_pmm_manager`。 |
| `code/kern/mm/pmm.c`、`code/kern/mm/mmu.h`、`code/libs/riscv.h`、`code/kern/driver/dtb.h` | `get_memory_base()`、`get_memory_size()`、`nbase`、`pages`、`PADDR`、`KADDR`、页引用计数；Sv39 通过 `PDX1`、`PDX0`、`PTX` 逐级定位，非叶用 `ptd_create()`，叶子用 `pte_create()`。 |
| `code/kern/init/entry.S`、`code/tools/kernel.ld` | 物理装载地址 `0x80200000`；内核链接到高地址；`boot_page_table_sv39` 及高半区直接映射供 `page_init()` 使用。 |
| `code/kern/trap/trap.h`、`code/kern/trap/trapentry.S`、`code/kern/driver/clock.h` | `struct trapframe` 的寄存器和 CSR 槽位、已有 `SAVE_ALL`、`forkrets`、`trap()`、`clock_set_next_event()` 和 `ticks`。 |
| `code/kern/process/proc.h`、`code/kern/process/proc.c`、`code/kern/schedule/sched.c` | `struct context`、`struct proc_struct`、`boot_pgdir_pa`、`set_links()`、`hash_proc()`、`wakeup_proc()`、`proc_list`、`local_intr_save/restore`；`set_links()` 已负责进程链及计数。 |

[GUARANTEE]

| 允许修改的文件 | 必须提供或适配的入口 |
| --- | --- |
| `code/kern/mm/default_pmm.c` | `static void best_init(void)`、`static void best_init_memmap(struct Page *base, size_t n)`、`static struct Page *best_alloc_pages(size_t n)`、`static void best_free_pages(struct Page *base, size_t n)`、`static size_t best_nr_free_pages(void)`、内部 `best_insert_free_block(struct Page *base, size_t n)`，注册 `const struct pmm_manager default_pmm_manager`。 |
| `code/kern/mm/pmm.c` | `static void page_init(void)`、`pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)`、`int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)`、`void page_remove(pde_t *pgdir, uintptr_t la)`；按实际非叶权限和启动大页适配 `check_pgdir()`、`check_boot_pgdir()`。 |
| `code/kern/init/entry.S` | 适配 `kern_entry` 和 `boot_page_table_sv39`，接好临时恒等映射、分页开启及高地址跳转。 |
| `code/kern/trap/trap.c` | `void interrupt_handler(struct trapframe *tf)`、`void exception_handler(struct trapframe *tf)` 的 Lab3 基础，内部指令长度辅助函数。 |
| `code/kern/trap/trapentry.S` | `__alltraps`、`__trapret` 和 `RESTORE_ALL` 的 Lab3 恢复主体，匹配已有 Trap 布局。 |
| `code/kern/process/proc.c` | `alloc_proc()`、`kernel_thread()`、`copy_mm()`、`copy_thread()`、`init_main()`、`proc_init()`、`proc_run()`、`cpu_idle()`、`do_fork()` 的旧内核线程路径。 |
| `code/kern/process/switch.S`、`code/kern/schedule/sched.c` | `void switch_to(struct context *from, struct context *to)` 和 `void schedule(void)`。 |

只在上表范围迁入所需实现。`check_pgdir()`、`check_boot_pgdir()` 的改动限于准确验证当前 Sv39 表项和启动映射。保留 `default_pmm_manager` 对外名称、原 `default_check()`、Lab5 原构建与评分、`SAVE_ALL`、`forkrets` 和 `wakeup_proc()`。保留课程标记。首次用户进程、U/S 双向恢复、`do_wait()` 和用户地址空间复制留给对应 Lab5 模块。

[SPECIFICATION]

### `best_init()`、`best_init_memmap()`、`best_insert_free_block()`、`best_alloc_pages()`、`best_free_pages()`、`best_nr_free_pages()`

**Pre-Condition**　页描述符和标志按当前框架定义；登记区间来自保留页，划拨或释放请求以页数为单位。

**Post-Condition**　空闲链按物理地址递增，块头保存长度和 `PageProperty`，`nr_free` 等于空闲页总数。注册对象调用迁入的函数和原检查函数。

- **Case 1 · 初始化与登记**　`best_init()` 建立空链并归零；`best_init_memmap()` 校验 `n > 0` 和保留标志，清理页状态后登记整个区间。
- **Case 2 · 选择与拆分**　`best_alloc_pages()` 选长度最小且足够的块；相同长度取先遇到的低地址块。摘除旧头，余块就地登记，空闲数只扣实际分配页数。
- **Case 3 · 空闲不足**　找不到合适的连续块时返回 `NULL`，链表和计数保持原状。`n == 0` 由既有断言拒绝。
- **Case 4 · 释放与合并**　`best_free_pages()` 清理页状态，辅助函数按地址插链；前插与尾插均只执行一次，分别检查前后物理邻接块并合并。`best_nr_free_pages()` 返回计数。

**Requirements**　管理器注册名与评分脚本一致，空闲数只随登记、分配和释放变化。拆分余块与合并后链表中每块只保留一个有效块头。

### `page_init()` 与启动入口

**Pre-Condition**　DTB 已提供 RAM 范围；启动页表可覆盖高地址内核及即将管理的物理内存。

**Post-Condition**　高地址入口可继续取指；`pages`、`npage`、`va_pa_offset` 与物理布局一致，空闲范围在页描述符数组之后。

- **Case 1 · 启动切换**　写入 `satp` 后先借临时恒等映射保持取指，再跳到高地址，从根表删除临时项并执行 `sfence.vma`。
- **Case 2 · DTB 和描述符**　验证 RAM 起点、长度、加法溢出与内核覆盖范围。裁剪并按页对齐可管理末端。初始化 `npage - nbase` 个描述符为保留页，只将描述符数组末端向上对齐后的完整空闲页登记给 PMM。
- **Case 3 · 参数无效或资源不足**　无效 DTB、描述符放不下等条件给出明确诊断；有效内存范围内没有剩余完整空闲页时跳过空闲区登记。

**Requirements**　管理上界、描述符区间和启动映射要彼此一致。保留原启动入口及 DTB 参数传递。

### `get_pte()`、`page_insert()`、`page_remove()` 与页表检查

**Pre-Condition**　传入有效的页表根，`get_pte()` 的用户页映射路径按 4 KiB 页表层次访问。

**Post-Condition**　叶项映射、引用计数和 TLB 一致；失败时仅回滚本次新建的页表资源。

- **Case 1 · 查询与创建**　`create=0` 遇到缺表返回 `NULL`；`create=1` 分配并清零所需中间页表，两个非叶项用 `ptd_create()` 创建。
- **Case 2 · 插入或替换**　先增加新页引用；同页重插抵消多加的引用并更新权限。替换旧页时撤销旧映射，提交新叶项后同步 TLB。
- **Case 3 · 分配失败**　返回 `-E_NO_MEM`。本次新建第一张表、第二张表分配失败时恢复原根项并释放新表；已有表仍归原地址空间。
- **Case 4 · 删除**　缺表、空叶项和重复删除直接返回。有效项先清除和同步 TLB，再减少页引用，归零才释放物理页。空的中间表由地址空间清理路径管理。
- **Case 5 · 自检**　`check_pgdir()` 核对用户权限在叶项，两级非叶均为有效表指针；`check_boot_pgdir()` 按根级 1 GiB 叶项核对内核直接映射的地址关系。

**Requirements**　保留自检对映射替换、引用和回收的检查。检查启动大页时直接使用对应根叶项。

### `interrupt_handler()`、`exception_handler()`、`__alltraps`、`__trapret` 与 `RESTORE_ALL`

**Pre-Condition**　Lab5 已有 `SAVE_ALL` 及 `trapframe` 排布可用；此任务验证旧内核线程需求。

**Post-Condition**　内核 Trap 能把帧交给 C 并按原布局恢复，Lab3 的内核时钟与异常处理作为 Lab5 拓展的起点。

- **Case 1 · 时钟/软件中断**　时钟重装并计数，软件中断清对应挂起位。中断保留原 `epc`，Lab5 不沿用固定次数关机策略。
- **Case 2 · 内核同步异常**　断点、非法指令根据编码确定 2 或 4 字节后推进 `epc`，无法判定长度时保留诊断。用户异常留给模块一接入。
- **Case 3 · 调用与恢复**　`__alltraps` 传入帧指针并调用 `trap()`，然后经 `__trapret` 恢复 `sstatus`、`sepc`、通用寄存器及栈指针，执行 `sret`。

**Requirements**　恢复顺序遵循 `SAVE_ALL` 与 `trapframe` 槽位；U/S 双向栈恢复由模块二在基线上完成。

### `alloc_proc()`、`kernel_thread()`、`copy_mm()`、`copy_thread()`、`init_main()`、`proc_init()`、`proc_run()`、`cpu_idle()`、`do_fork()`、`schedule()`、`switch_to()`

**Pre-Condition**　PMM、内核堆和内核页表可用，Lab5 已提供 PID、哈希、亲属链和唤醒接口。

**Post-Condition**　具备 Lab4 内核线程创建和切换的基础，并为后续 Lab5 模块保留原有接口。

- **Case 1 · 启动和现场**　`alloc_proc()` 初始化 PCB、链节点和新加字段。`proc_init()` 建立 idle 及 init 的旧路径。`kernel_thread()` 构造 S 态起始帧；`copy_thread()` 保存帧，设置子线程 `a0=0` 及首次进入 `forkret` 的上下文。`init_main()` 仅提供迁移阶段的内核线程入口。
- **Case 2 · 创建登记**　`do_fork()` 依次分配 PCB、内核栈、准备地址空间及现场，再在临界区分配 PID、挂哈希、调用 `set_links()` 并唤醒。`copy_mm()` 仅处理内核线程，遇到已有用户 `mm` 返回 `-E_INVAL`，交给模块三提供真正复制。
- **Case 3 · 创建失败**　达到进程数限制或资源不足返回相应错误，回收已申请的栈和 PCB，不残留半注册的进程。
- **Case 4 · 切换与空闲**　`cpu_idle()` 按 `need_resched` 调 `schedule()`，后者选就绪进程，无其他候选时选 idle。`proc_run()` 关中断更新 `current`、切 `satp` 并同步 TLB，`switch_to()` 保存和恢复 `ra`、`sp` 与 `s0`–`s11`。

**Requirements**　启动页表根使用物理地址。`set_links()` 已含进程数递增，调用处只登记一次。阶段验收使用 `make -C code` 指定这 8 个对象，后续待 Lab5 模块接齐后验证启动和用户进程。

<a id="yang-syscall"></a>

## 任务二：模块一 · 用户态系统调用

[PROMPT]

**任务**　在 Lab5 用户态 `ecall` 入口接通统一分发，让用户库、Trap 和内核服务使用同一套 RV64 约定。以 `6e9bc57` 的 Lab5 基线为起点，在 `code/kern/trap/trap.c` 的 `lab5 function1` 处更新 `exception_handler()`，在 `code/kern/syscall/syscall.c` 的同一任务标记内实现分发及 9 个 `sys_*` 处理函数。同步修改用户库封装，并接上 `print_pgdir()`。

**操作要求**　必须修改工程中的实际文件。使用当前仓库的编号、函数签名和陷入帧；保留 Lab5 用户态原有公开接口的签名。保留 `lab5` 任务标记，在本人完成的标记内将 `YOUR CODE` 换为 `2412790`。按功能给实现和依赖找到真正的调用点。

**输出要求**　列出实际修改的文件、实现的接口、`ecall` 到返回的寄存器约定和用于验证的命令。列明哪个结果来自宿主测试、对象编译或整机运行。链接缺少队友符号时保留原错误。

[RELY]

| 真实文件 | 当前约定 |
| --- | --- |
| `code/libs/unistd.h` | `SYS_exit=1`、`SYS_fork=2`、`SYS_wait=3`、`SYS_exec=4`、`SYS_yield=10`、`SYS_kill=12`、`SYS_getpid=18`、`SYS_putc=30`、`SYS_pgdir=31`。编号表存在空槽。 |
| `code/kern/trap/trap.h`、`code/kern/trap/trap.c`、`code/libs/riscv.h` | `struct trapframe` 提供 `gpr.a0`–`gpr.a5`、`gpr.sp`、`epc`、`cause` 和 `status`。`trap()` 在分发期间设置 `current->tf`，`trap_in_kernel()` 用 `SSTATUS_SPP` 区分来源。`CAUSE_USER_ECALL` 表示 U 态系统调用。 |
| `code/kern/process/proc.h` | `int do_exit(int error_code)`、`int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)`、`int do_wait(int pid, int *code_store)`、`int do_execve(const char *name, size_t len, unsigned char *binary, size_t size)`、`int do_yield(void)`、`int do_kill(int pid)`。`current->pid` 与 `current->mm` 属于当前进程。 |
| `code/kern/mm/vmm.h`、`code/kern/mm/vmm.c` | `bool user_mem_check(struct mm_struct *mm, uintptr_t start, size_t len, bool write)` 检查地址区间及 VMA 权限。用户态入口需要有当前用户 `mm`。`mm->mmap_list` 按起始地址组织 VMA，`mm->pgdir` 提供页表根。 |
| `code/kern/mm/pmm.h`、`code/libs/stdio.h` | `get_pte(pgdir, va, 0)` 查询现有 4 KiB 叶项，`void print_pgdir(void)` 是已有声明，`void cputchar(int c)` 输出单字符。 |
| `code/user/libs/syscall.c`、`code/user/libs/syscall.h`、`code/user/libs/ulib.c`、`code/user/libs/ulib.h` | 本包 ABI 在 `a0` 传调用号，`a1`–`a5` 传参数，返回值回到 `a0`。用户库原有 `exit/fork/wait/waitpid/yield/kill/getpid` 等入口继续可用。 |

[GUARANTEE]

- `code/kern/trap/trap.c` 提供 `void exception_handler(struct trapframe *tf)` 的用户分支，内核同步异常继续用迁入的处理逻辑。时钟调度另见任务三。
- `code/kern/syscall/syscall.c` 提供 `void syscall(void)`，以及签名为 `static int sys_名称(uint64_t arg[])` 的 `sys_exit`、`sys_fork`、`sys_wait`、`sys_exec`、`sys_yield`、`sys_kill`、`sys_getpid`、`sys_putc`、`sys_pgdir`。分发表指向这 9 个实现，handler 放在初始化分发表之前。
- `code/user/libs/syscall.c` 用 `static inline int syscall(uintptr_t num, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4)` 统一发起 `ecall`，调整已有封装传参。新增 `int sys_exec(const char *name, size_t len, const unsigned char *binary, size_t size)`，在用户头文件中声明；在 `ulib.c/.h` 新增同参数的 `int exec(...)`。
- `code/kern/mm/pmm.c` 实现 `void print_pgdir(void)`，只展示当前用户 VMA 内存在的用户页映射及其读写执行权限。

允许修改以上文件内与本任务直接相关的代码、声明及必要的头文件包含。保留 `unistd.h` 编号、`trapframe` 布局、汇编返回约定、已有用户接口签名和原评分测试。`do_wait()`、`do_execve()`、用户 `do_fork()` 的底层实现由各模块负责。Lab5 用户 `exec` 传入旧地址空间中的名称和 ELF，装载层需要在销毁旧地址空间前准备好这两个输入。

[SPECIFICATION]

### `exception_handler()`

**Pre-Condition**　`tf` 指向当前陷入帧；进程上下文中的 `trap()` 已把它挂到 `current->tf`。

**Post-Condition**　从 U 态发起的 `ecall` 进入分发，返回位置指向下一条指令。同步异常和系统调用按原因码走各自路径。

- **Case 1 · U 态 `ecall`**　先执行 `tf->epc += 4`，再调用 `syscall()`。`fork` 随后复制的子帧也具有已推进的 `epc`。
- **Case 2 · U 态其他同步异常**　基础版打印陷入帧并以 `-E_KILLED` 退出。写时复制联调时由相应模块提供可恢复缺页的处理入口。
- **Case 3 · S 态同步异常**　沿基线处理，内核断点和非法指令仍由编码决定步长。

**Requirements**　`ecall` 为 4 字节；这个固定步长只应用于 `ecall`。异步中断的返回地址保持原位。若 `exec` 成功更换程序，装载层之后写入的新入口由返回流程使用。

### 内核 `syscall()` 与用户库 `syscall()`

**Pre-Condition**　内核分发持有有效陷入帧；用户发起调用时按 RV64 的寄存器宽度封装参数。

**Post-Condition**　合法请求只调用一次目标 handler。其 `int` 结果经带符号转换写回完整宽度的 `a0`，用户库从该寄存器取回结果。

- **Case 1 · 合法调用**　用完整 `uintptr_t` 宽度检查 `a0`，将 `a1`–`a5` 按顺序组成参数数组，再调用表项。
- **Case 2 · 无效调用**　编号越界或表内为空时，`a0` 写回 `-E_INVAL`。高位非零的编号先按完整宽度检查。
- **Case 3 · 用户态封装**　以寄存器约束把编号和参数绑定到 `a0`–`a5`，使用 `a0` 作为读写操作数，未用参数明确填 0。返回从 `a0` 转为 `int`。

**Requirements**　保留 `ecall` 的 `memory` clobber，满足指针实参和负值结果的位宽要求。参数传递采用明确形参与寄存器约束，避免从可变参数列表读取不存在的参数，也避免把 8 字节寄存器值写进 4 字节局部变量。

### 9 个 `sys_*` 处理函数和 `print_pgdir()`

**Pre-Condition**　处理函数接收完整的 5 个机器字。涉及用户地址时，先按访问方向校验区间。

**Post-Condition**　正确转发请求，拒绝无效参数，保持下表约定的结果。

| Case | 条件 | 预期行为 |
| --- | --- | --- |
| 1 · `sys_exit` | `a1` 为退出码 | 以 `int` 类型传给 `do_exit()`。成功退出后进入调度。 |
| 2 · `sys_fork` | 普通 `fork` | 调 `do_fork(0, tf->gpr.sp, tf)`，父进程收到创建结果。子帧返回 0 由创建路径设置，联调用例检查同一 `ecall` 后的两个返回值。 |
| 3 · `sys_wait` | PID 能无损转换为 `int`，输出地址可选 | `NULL` 直接转发；非空时要求有用户 `mm`，检查 `sizeof(int)` 字节的可写性，非法 PID 位宽或地址返回 `-E_INVAL`。真正等待和退出码写回由 `do_wait()` 处理。 |
| 4 · `sys_exec` | 依次传名称、名称长度、ELF 地址和大小 | 使用用户 `mm` 分别检查两个区间的可读性。无 `mm` 或校验失败返回 `-E_INVAL`，有效时按四个参数的原顺序转给 `do_execve()`。 |
| 5 · `sys_yield` | 函数无业务参数 | 调 `do_yield()`，按返回路径落实重调度。 |
| 6 · `sys_kill` | PID 从机器字转成 `int` | 对高位与该 `int` 符号扩展不一致的输入返回 `-E_INVAL`；其余由 `do_kill()` 判断目标。 |
| 7 · `sys_getpid` | 存在当前进程 | 返回 `current->pid`。 |
| 8 · `sys_putc` | `a1` 为字符值 | 取低 8 位，调用 `cputchar()` 后返回 0。 |
| 9 · `sys_pgdir` | 当前用户页表可查询 | `print_pgdir()` 按用户 VMA 遍历，调用 `get_pte(mm->pgdir, va, 0)`，只打印 `PTE_V` 和 `PTE_U` 都有效的页及 `rwx`。无用户 `mm` 或缺页表时直接返回。`sys_pgdir()` 返回 0。 |

**Requirements**　等待读取退出码的最终写回需要由 `do_wait()` 在正确时机处理。`exec` 的区间可读性检查与 ELF 内容解析属于两个阶段。`print_pgdir()` 查询不得分配页表，也不得访问内核大页分支。用 `tools/test_lab5.c` 中的 `test_lab5_syscall()` 检查 `epc` 从 `0x1000` 变为 `0x1004`、`getpid` 及编号 0；再在 QEMU 中用 `hello`、`badarg`、`pgdir`、`forktest` 等现有程序验证完整的用户态调用路径。

<a id="yang-process-control"></a>

## 任务三：模块四 · 退出、让出与终止

[PROMPT]

**任务**　在 `code/kern/process/proc.c` 的 `lab5 function4` 标记中实现 `do_exit()`、`do_yield()`、`do_kill()`。在 `code/kern/trap/trap.c` 的 S 态时钟中断分支设置重调度请求。让退出完成本进程的用户地址空间处置并使父进程可以等待回收，让运行中的进程能主动让出或在合适的返回点响应终止请求。

**操作要求**　必须修改工程中的实际文件。保留 `lab5` 标记，把本人完成处的 `YOUR CODE` 改为 `2412790`。用真实 PCB 和等待标志判断唤醒条件，保证亲属链更改位于关中断临界区。确认整个内核栈和 PCB 的最终回收由 `do_wait()` 完成。

**输出要求**　说明退出时的地址空间和 PCB 资源归属、退出码何时可见、唤醒条件和孤儿转交顺序。测试依赖 `wait`、用户 `fork` 或首次 U 态返回时，说明实测边界和剩余联验项。

[RELY]

| 真实文件 | 当前约定 |
| --- | --- |
| `code/kern/process/proc.h`、`code/kern/process/proc.c` | `current`、`idleproc`、`initproc`、`PROC_SLEEPING`、`PROC_RUNNABLE`、`PROC_ZOMBIE`、`PF_EXITING`、`WT_CHILD`、`WT_INTERRUPTED`、`find_proc(int pid)`、`put_pgdir(struct mm_struct *mm)`。`cptr` 指向最新子进程，`optr` 指向更早的兄弟，`yptr` 指向更新的兄弟。 |
| `code/kern/mm/vmm.h`、`code/kern/mm/vmm.c`、`code/kern/mm/pmm.h`、`code/libs/riscv.h` | `mm_count_dec()` 递减引用；`exit_mmap()` 要求 `mm_count(mm) == 0`；`mm_destroy()` 清理 VMA 和 `mm`；`boot_pgdir_pa`、`lsatp()` 与 `flush_tlb()` 用于切回内核页表。 |
| `code/kern/schedule/sched.c`、`code/kern/sync/sync.h`、`code/kern/trap/trap.c` | `wakeup_proc()` 把睡眠进程变为可运行并清等待状态。`schedule()` 负责切换。`local_intr_save/restore` 保存并恢复先前的中断状态。原 `trap()` 在返回用户态前先查 `PF_EXITING`，再查 `need_resched`。 |

[GUARANTEE]

必须实现 `int do_exit(int error_code)`、`int do_yield(void)`、`int do_kill(int pid)`，并更新 `void interrupt_handler(struct trapframe *tf)` 的 `IRQ_S_TIMER` 分支。允许在本人模块加入必要的内部辅助逻辑。保留 `do_wait()`、`set_links()`、`remove_links()`、`wakeup_proc()`、调度器本体、PCB 定义、汇编和原评分测试。等待者负责摘除进程记录、释放内核栈和 PCB；本任务负责退出状态、用户地址空间与唤醒。

[SPECIFICATION]

### `do_exit(int error_code)`

**Pre-Condition**　`current` 为当前进程。普通进程可能有独立、共享或空的用户 `mm`。

**Post-Condition**　正常退出时进程保存退出码并成为 `PROC_ZOMBIE`，交给调度器。等待者仍可以访问僵尸 PCB 和退出码。

- **Case 1 · 保护进程**　尝试退出 `idleproc` 或 `initproc` 时触发内核保护。
- **Case 2 · 有用户地址空间**　把当前记录和硬件页表切回 `boot_pgdir_pa` 并刷新 TLB，清除 `current->mm`。若引用减到 0，按 `exit_mmap()`、`put_pgdir()`、`mm_destroy()` 的顺序释放；仍有引用时保留共享映射。内核线程的 `mm` 为空，跳过该段。
- **Case 3 · 等待者**　在关中断区域写入 `exit_code`、设置 `PROC_ZOMBIE`。父进程同时满足 `PROC_SLEEPING` 和 `WT_CHILD` 才唤醒。
- **Case 4 · 孤儿托管**　从当前进程的 `cptr` 依次摘出子进程，更新原链头反向指针，再把子进程挂到 `initproc` 的子链头，修正 `parent` 和两个兄弟方向。被托管的是僵尸且 `initproc` 正等待子进程时，唤醒 `initproc`。

**Requirements**　父进程观察到僵尸状态时，必要的用户地址空间处置已在前完成。孤儿转交只改变亲属关系，全局链、哈希和计数由等待回收流程维护。恢复原中断状态后调用 `schedule()`，随后继续运行属于错误。

### `do_yield()`、`interrupt_handler()` 与调度检查

**Pre-Condition**　`do_yield()` 在有效的当前进程上下文中调用。时钟中断可能发生在 `current` 建立之前。

**Post-Condition**　调用 `do_yield()` 返回 0 且 `current->need_resched=1`。计时中断在有当前进程时设置同一请求，保留时钟重装与计数。

- **Case 1 · 主动让出**　只更新请求，再由用户 Trap 返回路径执行调度。用户进程再次获选后可从原调用之后继续执行。
- **Case 2 · 忙循环**　时钟中断先重装并计数，再对有效的 `current` 设置调度标志，让一直运行的 U 态进程也能够让出 CPU。
- **Case 3 · 暂无进程或 S 态中断**　`current == NULL` 时仍可处理时钟，跳过进程旗标。调度沿原检查点进行，异步中断保留 `epc`。

**Requirements**　保留计时输出，时钟处理函数不直接修改 `sepc`。通过 `yield` 与 `spin` 的运行输出分别观察主动让出和时钟促成的调度。

### `do_kill(int pid)`

**Pre-Condition**　从 PID 查进程；查找、判断和修改状态放在同一个关中断区域。

**Post-Condition**　首次合法终止请求返回 0，并置 `PF_EXITING`；实际退出发生在进程处理该标志的检查点。

- **Case 1 · 无效目标**　PID 无效、目标不存在、目标是 idle/init 或已是僵尸时返回 `-E_INVAL`。
- **Case 2 · 重复请求**　目标已有 `PF_EXITING` 时返回 `-E_KILLED`，维持现有状态。
- **Case 3 · 可中断等待**　目标是 `PROC_SLEEPING` 且 `wait_state & WT_INTERRUPTED` 非零时，设置退出标志后调用 `wakeup_proc()`。
- **Case 4 · 运行或不可中断等待**　标记退出请求。由原返回或等待路径处理退出，既有进程状态保持到对应检查点。

**Requirements**　各路径都恢复原中断状态。`kill()` 请求终止，`do_exit()` 和 `do_wait()` 分别完成实际退出与最终回收。等待在唤醒后要复查退出标志，这一段与模块二联验。用课程 `Process Control` 中已有的 `do_yield()`、`do_kill(-1)` 检查配合 `exit`、`yield`、`spin` 及补充用例 `waitkill` 验证；`waitkill` 结果单独记录。
