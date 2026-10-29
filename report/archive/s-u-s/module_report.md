### Challenge：S→U→S 的底层入口、切栈与返回

> 目录说明：文中的工程相对路径、编译和测试命令以仓库的 `code/` 为工作目录；原实测日志归档在 `report/logs/`，重新运行生成的日志位于 `code/logs/`。整理前的文件位置可在 Git 历史中查看。

> 本轮为 A 侧接口与交接文字定稿：实现和两组通过日志均不变；上层按本文接口对接，不以事先确认作为继续开发的条件。原有阶段表述保留在 Git 历史中。

**负责人：** 2412790-杨茜茹。

#### 模块功能描述

**需要实现/修改的函数：**

```c
long member_a_enter_user(uintptr_t entry, uintptr_t user_sp,
                         uintptr_t kernel_trap_stack_top);
void member_a_user_trap_entry(void);
// 上层提供，A 的独立验证中使用测试回调：
long member_a_user_trap(struct trapframe *tf);
```

**功能说明：**

本模块在独立目录提供 A 负责的底层能力。普通内核函数保存自身 ABI 状态后进入 U；U 态发生 Trap 时，通过 sscratch 交换到专用内核栈再调用上层回调。上层决定恢复用户执行还是结束实验，结束时回到原 S 态调用点并恢复保存的 CSR。基础 trapentry.S 和 SAVE_ALL 不修改。

本阶段附带的用户代码、页表和 callback 只用于验证该既定接口。接口由 A 先行定义，不能将这些夹具记为另一位同学的工作成果，也不能据此宣布共同 Challenge 完成。

#### 最终提示词

以下为本轮实现和隔离验证所用提示词，已在本人 WSL 独立测试中完成验证；接口由 A 先行定稿，不代表其他成员已经接入。

````markdown
[PROMPT]

**任务：** 必须在真实工程中新增独立扩展文件，完成从 S 态函数调用进入 U 态、从用户 Trap 切至专用内核栈、根据上层回调决定继续 U 态或回到原 S 态调用者的底层能力。正式模块放 extensions/member_a_switch，隔离测试放 tests/member_a_switch；不得修改基础 trapentry.S。

**原理与功能：** 进入用户态必须同时提供有效用户映射、用户栈和特权返回状态。用户 Trap 不能直接在用户栈上执行内核 C 函数。底层负责内核调用上下文、专用入口、sscratch 栈约定及 CSR 恢复；上层负责用户代码、映射和具体 ecall 语义。本轮夹具只验证底层约定，不提前替上层完成整合。

**操作要求：** 阅读真实 trap.h、riscv.h、entry.S、链接脚本和既有基础实现。只新增上述独立目录、本提示词和对应设计交接文档；不修改共享 Makefile、正式 init.c/trap.c、PMM、SAVE_ALL、结构体、原测试与评分。保留已有改动，不自动提交或推送。

**输出与验证要求：** 提供接口头文件、汇编实现、逐阶段 sscratch/栈/CSR 协议、三种场景的有界 QEMU 驱动和原始日志。没有执行前只写待验证；完整扩展与正式内核评分仍须等上层接入及共同验收。阶段输出应有 cause、SPP、帧地址、用户 sp 等实测依据，不能只打印预设特权级标签。

[RELY]

- RV64 的 trapframe 为 36 个八字节槽：前 32 槽为 x0～x31，随后为 status、epc、badvaddr、cause。入口 C 参数按 ABI 使用 a0。
- 基础 entry.S 已创建高地址内核映射，其 U 位为零。它不能直接用作用户可执行页。
- sret 根据 SPP 选择返回特权级；从 U 进入 S 的保存状态应有 SPP=0。sscratch 可用于入口处交换栈指针。以工程真实 CSR 宏为准，参考 https://docs.riscv.org/reference/isa/priv/supervisor.html 。
- QEMU 4.1.1 的 sret 会把 SPIE 清零；不能将其后值按现代标准写成无条件为一。本轮保存原内核状态，并在回到调用者前恢复，不依赖该差异制造成功。
- 测试工具链使用 WSL 中真实 RISC-V GCC、objcopy、QEMU；运行器沿用原 entry.S 和 kernel.ld。当前没有完整正式 PMM/handler 的前提下，只运行独立镜像。

[GUARANTEE]

**必须实现的底层接口：**

```c
long member_a_enter_user(uintptr_t entry, uintptr_t user_sp,
                         uintptr_t kernel_trap_stack_top);
void member_a_user_trap_entry(void); // stvec 指向的汇编入口
```

**由上层提供的接口：**

```c
long member_a_user_trap(struct trapframe *tf);
```

回调返回 0 表示使用修改后的帧恢复用户执行，1 表示结束实验并回到原 S 态调用者。其他值视为协议错误，底层恢复内核上下文后返回 -1。重复进入时直接返回 -2，不改变正在使用的上下文。正常完成返回 0。

**边界约定：** 单核、单实例、非嵌套、扩展窗口屏蔽中断；不构成多进程调度。调用者提供正确用户映射、合法入口和 16 字节对齐的独立栈；底层不分配页、不修改 satp，上层负责安装和恢复页表。底层保存 ra、sp、gp、tp、s0～s11 和 stvec、sscratch、sstatus、sepc、sie；正常 C ABI 中 caller-saved 寄存器不承诺保留。上层不得把内核地址写进用户返回帧，须保留 U 态返回权限约束。

[SPECIFICATION]

## member_a_enter_user

**Pre-Condition：** 调用者处于 S 态、非嵌套且输入映射与栈有效；内核代码和上下文仍具有 S 态映射。用户代码应遵守最小测试协议，无浮点或向量状态需求。

**Post-Condition：** 正常退出或协议错误时回到原调用者，保存的 ABI 寄存器及原 stvec、sscratch、sstatus、sepc、sie 恢复；satp 不由底层修改。

**Case 1：正常调用。** 能进入用户代码，并在完成请求后回到原内核调用点。
**Case 2：活动期间再次调用。** 返回 -2，原活动实例不被覆盖。
**Case 3：回调返回非法动作。** 恢复原内核上下文，返回 -1，后续可重新调用。

**Requirements：** 用户初始通用寄存器不携带内核保存上下文地址；SPP 指向 U，扩展窗口 SIE/SPIE 和 sie 均关闭，返回后恢复原值。原始内核栈与专用 Trap 栈分离。

## member_a_user_trap_entry

**Pre-Condition：** 用户执行时 sscratch 保存专用内核 Trap 栈顶，当前 sp 为用户 sp；Trap 已由硬件进入 S。

**Post-Condition：** C 回调在内核栈上收到符合真实布局的帧；原用户 sp 留在 x2 槽。继续 U 态时保留修改后的 GPR、epc 和状态；结束时恢复原内核调用上下文。

**Case 1：回调要求继续 U。** 正确恢复帧并 sret；为下一次用户 Trap 重新建立 sscratch 栈约定。
**Case 2：回调要求结束。** 不从用户栈 ret，不使用用户 ra 作为内核返回地址。
**Case 3：出现意外 S 态 Trap。** 不把零指针当可写栈；上层必须识别 SPP 并报告失败。本轮不声称支持递归异常恢复。

**Requirements：** 调用上层 C 前恢复内核 gp/tp；保存期间用到的临时寄存器不能覆盖尚未保存的用户值；共享基础 SAVE_ALL/RESTORE_ALL 不改。

## 独立测试夹具与驱动

**Pre-Condition：** 工具链可用，仅新增的测试代码参与隔离链接。

**Post-Condition：** 三次运行分别检验正常完成、正常重复使用和错误动作后的安全恢复；每次均有真实 U 态 ecall 原因、SPP=0、用户 sp、内核栈帧和返回后的状态证据。

**Case 1、2：** 用户通过读写用户栈得到已知值，第一次 ecall 要求继续用户执行；回调改写 a0、epc，用户核对返回值后第二次 ecall 要求结束。第二次整轮重跑确保活动标记和返回状态清理。
**Case 3：** 第二次 ecall 的回调故意返回非法动作，底层必须返回 -1 并恢复调用状态。

**Requirements：** 单独映射用户代码 R/X/U 和用户栈 R/W/U，内核映射 U 位保持零。测试对内核 callee-saved 寄存器设置模式值，核对原 CSR 恢复；回调中追加一次重入尝试，要求 -2。不得把夹具当作上层成员已完成的用户代码、页表或 handler；不以截图代替状态检查。任意 FAIL、超时、缺失汇总或非零退出均判失败。
````

#### 实现迭代过程

##### 第一次迭代：完成底层实现与独立运行

**最终结果：**

- ✅ 独立汇编、接口头文件、夹具和运行器完成，五个编译单元、链接和镜像生成通过。
- ✅ 正常 S→U→S、重复往返及重入拒绝、错误回调动作后的恢复三个场景全部通过。
- ✅ 用户 Trap 的 cause=8、SPP=0，原用户 sp 正确保存，C 回调在专用内核栈运行。
- ✅ 用户在第一次 ecall 返回后核对了修改后的 a0，再执行第二次 ecall；底层可恢复修改后的用户帧。
- ✅ 返回后内核 ABI 寄存器与原 CSR 恢复，底层不修改 satp。
- ✅ 基础 trapentry.S、SAVE_ALL、PMM、正式 handler、Makefile 和原评分未修改。

本轮只进行一轮生成与本人 WSL 实际运行，未出现需要修改底层代码的失败。接口已由 A 先行定义并用于独立验证；测试专用用户程序、页表和回调没有被记作她的正式实现，也不把三个独立场景通过写成共同扩展或完整评分通过。

**关键改进点总结：**

1. 将原内核调用栈、用户栈和内核 Trap 栈分开管理，明确 sscratch 在各阶段保存的值。
2. 使用真实 cause、SPP、用户 sp 和内核帧地址证明路径，不只打印“S 态/U 态”标签。
3. 以重复运行、活动期间再次调用和非法动作测试恢复边界，避免只检查一次正常返回。
4. 普通函数返回恢复原内核 ABI 与 CSR，恢复用户执行使用处理后的 Trap frame；两条返回路径各有明确职责。
