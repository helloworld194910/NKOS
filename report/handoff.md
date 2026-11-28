# Lab5 交接说明

交付人：2412790-杨茜茹。接收人：2413921-程子涵、2411824-于蕾。共同基线分支为 `origin/lab5_yang`。

## 杨茜茹已交付

| 提交 | 内容 |
| --- | --- |
| `6e9bc57` | 全队共用的 Lab2–4 迁移基线，含物理页与页表、启动、Trap、内核线程、调度和上下文切换。 |
| `1d3fe34` | 模块一的 `ecall`、9 个系统调用入口及用户库 ABI；模块四的 `do_exit()`、`do_yield()`、`do_kill()` 与时钟调度请求。 |
| `cb7f021` | 杨茜茹的 [实验报告](report.md) 和 [完整提示词](prompts.md)。 |

验证记录：迁移的 8 个对象与新增功能的 6 个对象分别编译通过，5 组宿主检查通过。`1d3fe34` 的完整构建到达内核链接，缺少 `do_execve()` 和 `do_wait()`。整机 QEMU 和课程评分待合并后运行。

## 程子涵 · 首个用户进程、装载与等待

| 事项 | 需要接好的工作 |
| --- | --- |
| 用户进程启动 | 完成 `code/kern/process/proc.c` 中首个用户进程的创建与装载，接好 `kernel_execve()`、`do_execve()`、`load_icode()`；在 `code/kern/trap/trapentry.S` 完成 U/S 双向恢复，核对 `sscratch`、用户栈及 `sret`。把 `alloc_proc()`、`copy_thread()` 的最终约定同时给于蕾。 |
| `exec` 接口 | `sys_exec()` 已检查名称和 ELF 缓冲区的可读性，并按四参数转发。`do_execve()` 若替换旧 `mm`，先准备好旧地址空间中的输入；成功后设置新用户入口和栈。普通 `ecall` 的 `epc` 已由入口推进 4 字节。 |
| `wait` 接口 | `do_exit()` 写 `exit_code`、设置 `PROC_ZOMBIE`，并唤醒处在 `WT_CHILD` 的父进程。`do_wait()` 负责查找可回收的子进程、在实际写回时校验可选的退出码地址、摘除登记、释放子进程内核栈和 PCB。阻塞后被唤醒时复查 `PF_EXITING`。课程 `Process Control` 用例要求 `do_wait(-1, NULL)` 返回 `-E_BAD_PROC`。 |

**请回传** `do_execve()`、`do_wait()` 与 U/S 返回的分支和提交号，附 `hello`、`testbss` 的真实输出；和杨茜茹复验 `exit`、`badarg`、`waitkill`，确认退出码、非法输出地址、等待中断和最终回收。请补齐模块二及加载思考题，合并后汇总报告。

## 于蕾 · 普通 fork、地址空间复制与 COW

| 事项 | 需要接好的工作 |
| --- | --- |
| 普通 `fork` | 完成 `code/kern/process/proc.c` 中的 `do_fork()`、`copy_mm()`，以及 `code/kern/mm/vmm.c` 的 `dup_mmap()`、`code/kern/mm/pmm.c` 的 `copy_range()`。目前 `copy_mm()` 只有迁移的内核线程路径。`sys_fork()` 已调用 `do_fork(0, tf->gpr.sp, tf)`，子帧应使用已推进的 `epc` 和自己的 `a0=0`，父进程收到子 PID。 |
| 登记与回收 | 沿用 `set_links()`，亲属链和进程数只登记一次。对齐 `CLONE_VM`、`mm_count` 和分配失败的回滚方式。`do_exit()` 会先切离用户页表，减少 `mm` 引用，归零才释放地址空间。请确认普通复制和共享下退出均符合这个约定。 |
| COW | 在基础版通过后单独提交。说明写缺页处理入口、引用计数、权限和 TLB 更新规则，并与杨茜茹对齐 `exception_handler()` 的分支位置：可恢复的 COW 写缺页要在基础版用户异常退出路径之前处理。和程子涵联验 `exec` 和退出码写回遇到共享页的场景。 |

**请回传** 普通 `fork` 分支和提交号，附 `forktest`、`forktree` 的实测输出及失败回滚记录。COW 另给提交号、设计与状态转换说明，附写入隔离、只读页保护、退出和 `exec` 资源回归的结果；补齐模块三和 Challenge 报告。

## 合并与验收

1. 两位先执行 `git fetch origin lab5_yang`，在各自分支按函数对比和合入。`proc.c` 与 `trap.c` 有多人工作，合并时核对各自函数范围。
2. 杨茜茹先合普通 `fork` 和首次用户进程、`exec`、`wait`，执行 `make -C code`、`make -C code qemu`、`make -C code grade`。补测 `waitkill`，记录测试对应的源码提交和真实输出。
3. 基础版通过后再合 COW，分别保留基础版与 COW 版的验收记录。最后由程子涵定稿报告，三人共同完成双重 GDB 展示。
