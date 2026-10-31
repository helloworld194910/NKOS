# Lab3 实验提示词汇总

根据教师最新通知，旧版 Challenge 1（用户态／内核态切换）无需完成；相关提示词不再列入本次任务。本次提交已移除相关历史材料。

| 成员 | 工具与模型 | 当前收录内容 |
| --- | --- | --- |
| 2412790-杨茜茹 | Codex 桌面应用；GPT-6.1 Sol | Trap 基础实现、上下文独立验证两份完整四段提示词，以及完整内核复验与报告整理要求 |
| 2413921-程子涵 | 已整理在本文 B 部分 | 中断时钟、stvec、临界区规格和本轮真实适配过程已归档 |
| 2411824-于蕾 | 待于蕾填写 | Lab2 依赖补齐与异常处理的实际提示词待补 |

文中工程相对路径与命令以仓库 `code/` 为工作目录；下述独立测试提示词仅保留实际开发记录，脚本及日志不随最终提交附带。基础实现初版对应提交 `1e62511`，上下文测试修订与运行记录对应 `0cf26be`。以下为根据实际约束整理的修订版，不作为首次对话的逐字记录。

| 任务 | 提示词 |
| --- | --- |
| Trap 上下文恢复与入口 | [实现提示词](#trap-context) |
| 上下文独立 QEMU 验证 | [测试提示词](#context-validation) |
| 完整内核复验与报告整理 | [复验要求](#final-validation) |

---

<a id="trap-context"></a>

## 功能模块一：Trap 上下文恢复与 Trap 入口

**负责人：** 2412790-杨茜茹。

[PROMPT]

**任务：**

在当前 Lab3 工程 `kern/trap/trapentry.S` 的两个 `#lab3 function1 YOUR CODE` 标记处，实现基础 Trap 入口调用和上下文恢复；若标记已署名为 `2412790`，审查已有实现并仅作必要修正。

Trap 发生时，硬件记录异常位置和部分控制状态，但不会自动保存全部通用寄存器。框架的 `SAVE_ALL` 已将原程序寄存器和 Trap CSR 保存为栈上的 `struct trapframe`。本任务应让 C 处理函数收到该帧，并在处理函数返回后恢复帧中的有效状态，由原有 `sret` 继续执行。C 可以合法修改返回位置、通用寄存器和返回状态，汇编必须尊重这些修改。

**操作要求：**

1. 必须修改工程中的真实文件，不能只展示代码或补丁；若已有实现满足规格，不为制造修改而重写。
2. 实施前查看 Git 状态，阅读目标文件、`trap.h`、`riscv.h`、`trap.c` 及原有 Context 测试，核对实际接口。
3. 正式源码只允许修改 `RESTORE_ALL` 宏体和 `__alltraps` 中保存完上下文后的调用缺口。保留两处 `lab3 function1` 标记，将对应 `YOUR CODE` 替换为 `2412790`。
4. 不修改 `SAVE_ALL`、`trap.h` 布局、`trap()` 签名、真实 `trap.c`、PMM、启动汇编、链接脚本、共享 Makefile、原有测试或评分脚本。
5. 可新增本人提示词与设计、交接说明。不在本任务实现用户态切栈、时钟中断分派或其他成员负责的异常处理。
6. 保留用户已有改动。不自动提交、推送、回滚或覆盖其他成员代码。

**输出与验证要求：**

说明实际修改位置、帧布局、参数传递和返回状态的处理依据。执行当前条件允许的差异检查及单文件编译，报告真实命令、结果和未执行内容。完整内核受 PMM 或 handler 缺失阻塞时明确记录；单文件通过不能写成 QEMU、Context 评分或全实验通过。独立运行测试与正式内核评分分别记录。

[RELY]

## 最小工程依赖

- `kern/trap/trapentry.S`：已有 `SAVE_ALL`、`RESTORE_ALL`、`__alltraps`、`__trapret`。`__trapret` 已调用 `RESTORE_ALL` 并执行 `sret`。
- `kern/trap/trap.h`：`struct pushregs` 按 x0 至 x31 的顺序记录 32 个通用寄存器；`struct trapframe` 随后包含 `status`、`epc`、`badvaddr`、`cause`。
- `libs/riscv.h`：RV64 下 `REGBYTES=8`，`LOAD=ld`，`STORE=sd`。类型来自真实 `libs/defs.h`，不能另造一套布局。
- `kern/trap/trap.c` 的 C 接口：

```c
void trap(struct trapframe *tf);
```

- RISC-V 调用约定：第一个参数在 `a0`；调用可能破坏 `ra`、参数寄存器和临时寄存器。正常 C 调用返回时应恢复自己的栈帧，调用者仍可定位原 Trap 帧。

## 当前帧布局

| 内容 | 相对当前帧基址的字节偏移 |
| --- | --- |
| xN | `N * REGBYTES` |
| 原始 x2 / sp | 16 |
| status | 256 |
| epc | 264 |
| badvaddr | 272 |
| cause | 280 |

总大小为 288 字节，满足原栈对齐时的 16 字节 C 调用对齐要求。`SAVE_ALL` 保存通用寄存器原值后借用 s0～s4 读取 CSR；它将原始 sp 放入 x2 槽，并清零 sscratch。不能把借用后的 s0～s4 当作被中断程序的原值。

基础框架使用当前内核栈，未实现完整 U 态进入内核时的栈交换。本任务只对当前 S 态入口前提负责。`sret` 对 SIE、SPIE、SPP 的后续变换属于处理器行为，不能要求返回后 sstatus 与保存值逐位相等。

[GUARANTEE]

**必须交付的接口与连接：**

```asm
.macro RESTORE_ALL
    # 恢复当前有效 trapframe 所规定的返回状态
.endm

__alltraps:
    # 保留已有 SAVE_ALL
    # 将当前帧地址传给 trap，并正常调用后返回

__trapret:
    # 保留已有 RESTORE_ALL 和 sret
```

宏和符号名称保持不变，不新增跨文件的正式接口。允许在两处标记范围内选择必要的汇编临时寄存器；这些寄存器最终也必须从对应槽恢复。

**系统约束：**

- 原程序通用寄存器状态来自处理后的帧，不以 C 调用后寄存器的瞬时内容替代。
- 使用帧基址时不能先把它覆盖为原始 sp；恢复 sp 之后不得继续依赖原基址加载其他槽。
- 不写回只用于诊断的 scause、sbadaddr，不将其错作返回地址。
- 不在汇编中固定给 epc 加 2 或加 4；异常指令长度与跳转策略由处理函数决定。
- 不强制设置 SPP、SPIE，不改变处理函数合法决定的返回意图。

[SPECIFICATION]

## RESTORE_ALL 宏

**Pre-Condition：** sp 指向完整、有效且经 C 处理后的 trapframe；本轮采用框架支持的内核栈入口。

**Post-Condition：** x1、x3～x31 与各自槽值一致，x2 使用保存的 sp 槽；sstatus、sepc 在执行 sret 前采用处理后的 status、epc。x0 保持零。帧存储不被破坏。

**Case 1：处理函数不改变帧。** 恢复被中断程序的 GPR 和返回位置，即使 C 实际使用过 ra、a0、临时寄存器，也不能丢失原值。

**Case 2：处理函数修改返回位置。** 采用新的 epc；两字节和四字节断点均不由恢复宏重复调整。

**Case 3：处理函数修改寄存器槽。** 对 t0、a0 或 s2 等槽的合法修改应体现在返回后的寄存器中，其他槽保持各自值。

**Case 4：处理函数修改返回状态。** 将修改后的 status 交给 sret，不用硬编码常量覆盖它；最终位变化按实际处理器或模拟器语义解释。

**Requirements：** 恢复 CSR 时借用的 GPR 也须正确恢复；不能因提前恢复 sp 丢失帧地址。不要通过增加整段新保存逻辑重复或改变 SAVE_ALL 的既定语义。

## __alltraps 调用部分

**Pre-Condition：** 硬件已进入入口，原有 SAVE_ALL 已完整构造帧；栈可用于正常 C 调用。

**Post-Condition：** trap 收到该帧的地址；调用返回后能够继续使用同一帧进入 __trapret。

**Case 1：同步异常。** C 根据异常类型处理并可更新 epc；返回流程尊重处理结果。

**Case 2：异步中断。** C 完成中断处理后可返回原执行位置；本任务不负责重设时钟或累计 ticks。

**Requirements：** a0 必须是当前帧地址，不是原始 sp、原始 a0 或 sepc。调用必须能够返回，不能使用 tail 取代正常调用，也不能只靠 caller-saved 寄存器跨调用保留唯一帧地址。

## 验证与边界

**Pre-Condition：** RISC-V 工具链可用，原始测试未被修改。

**Post-Condition：** 真实记录验证范围、结果和依赖。

**Case 1：仅能编译汇编单元。** 记录单文件通过，完整运行仍待验证。

**Case 2：独立测试可运行。** 使用单独测试镜像验证真实汇编，明确测试 C handler 为夹具，不把其行为当作正式异常处理已完成。

**Case 3：其他模块齐备。** 再运行原有 Context 和整机检查，分别记录分数及持续中断返回表现。

**Requirements：** 没执行的项目不能填通过；发现失败应保留输出，先定位实现、测试与运行环境的差异，不修改原始评分预期来迁就结果。

---

<a id="context-validation"></a>

## 补充验证：Trap 上下文独立 QEMU 测试

**负责人：** 2412790-杨茜茹。

[PROMPT]

**任务：**

为真实 `kern/trap/trapentry.S` 建立独立的 QEMU 运行测试。新增或修改以下真实文件：

- `tests/member_a/context_fixture.c`：测试专用入口、trap 处理夹具及结果检查。
- `tests/member_a/context_probe.S`：寄存器模式值、异常指令和返回快照。
- `tests/member_a/run_context.py`：隔离构建、环境记录、运行及自动判定。
- `docs/member_a_context_validation.md`：规格、实际迭代和验证范围。

测试目标是检查汇编入口是否正确传参、恢复上下文并尊重 C 对帧的合法修改。它不依赖其他成员尚未完成的 PMM 和异常分派；被测汇编必须直接取自真实工程，不能在测试中复制一份简化实现。

**操作要求：**

1. 必须修改或新增工程中的真实文件，不能只展示代码。
2. 先读取实际 trapentry.S、trap.h、riscv.h、entry.S、链接脚本和原有 Context 测试，保留用户修改。
3. 只允许修改上述独立测试、本提示词和对应说明。不得修改正式 trapentry.S、SAVE_ALL、trap.c、trap.h、PMM、启动汇编、链接脚本、Makefile、原有测试和评分脚本。
4. 测试夹具可以提供 kern_init 和 trap 符号，但只能进入独立镜像，不能接入正式内核构建。
5. 测试发现失败时保留输入、环境、预期和实际值。不能读取实际输出后将它自动改成预期，也不能删除关键检查来得到 PASS。
6. 不自动提交、推送或回滚 Git。

**输出与验证要求：**

提供 `python3 tests/member_a/run_context.py` 一条完整运行命令。正常完成时逐场景输出 PASS，最后输出 `RESULT: 3/3 context scenarios passed`。编译失败、超时、缺少标记、出现 FAIL 或 QEMU 非零退出都应使驱动失败；仅 QEMU 退出码为零不能判通过。

构建产物放在 obj/member-a-context，日志放 logs。保存 QEMU 版本、被测源码哈希、SPIE 预期依据、构建日志和运行日志。未实际执行或修订后未复跑时，状态必须写明等待验证。

[RELY]

## 真实接口与数据

```c
void trap(struct trapframe *tf);
```

`trap.h` 的前 32 槽为 x0～x31；随后为 status、epc、badvaddr、cause。RV64 总大小 288 字节，原始 sp 位于第 2 槽，status、epc 的偏移分别为 256、264。测试应包含编译期布局检查，不另造结构体。

`kern/init/entry.S` 提供启动栈、Sv39 启动映射与 kern_entry，`tools/kernel.ld` 定义高地址链接布局；两者只读，直接用于测试链接。`libs/riscv.h` 提供真实 CSR 宏与 SIE、SPIE、SPP 常量。测试编译所需类型来自 `libs/defs.h`。

## 执行环境与版本差异

RISC-V GCC、objcopy、QEMU 应可在 WSL 的 PATH 中找到。测试使用 OpenSBI 的旧式 console_putchar 和 shutdown 服务输出及退出，不引入 PMM 依赖。

现代 RISC-V xRET 语义包含将 xPIE 置 1；但上游 QEMU 4.1.1 的 `target/riscv/op_helper.c` 中 `helper_sret` 将保存的 SPIE 复制给 SIE，随后明确将 SPIE 清零，并清除 SPP。参考：
https://github.com/qemu/qemu/blob/v4.1.1/target/riscv/op_helper.c#L68-L93

因此，不能把该旧版模拟器的 SPIE=0 直接判断为 RESTORE_ALL 错误。运行器应先记录 `qemu-system-riscv64 --version`；明确识别为 4.1.1 时使用经源码核对的 SPIE=0 预期，其余版本使用标准 SPIE=1 预期。无法识别版本则失败；其他版本若出现不符，保留结果继续核对，不能自动学习实际位值来放宽预期。版本号不能证明任意私改构建与上游完全一致，这一限制需记录。

[GUARANTEE]

**必须交付的测试入口：**

```c
/* 仅测试镜像使用，不替换正式工程中的同名处理函数。 */
void kern_init(void);
void trap(struct trapframe *tf);

/* 汇编探针遵守调用约定，返回到测试 C 调用者。 */
void member_a_probe_word(void);
void member_a_probe_half(void);
void member_a_probe_edit(void);
```

汇编提供每项场景的异常指令与返回标签，以及独立快照数组。内部辅助函数名可自行选择；全部 GPR 值必须可检查，包括 ra、sp、gp、tp、a0 及 callee-saved 寄存器。

**运行器：** 提供 Python main 入口，支持明确的工程根目录；通过前需核对所有成功标记和退出码。运行超时有界，只终止自己启动的 QEMU。结果文件在开始时标为失败或未完成，全部通过后才改为通过。

**系统约束：**

- 探针必须在设置模式值前保存调用者状态，验证后完整恢复，不能让测试本身破坏 C ABI。
- 设置任意 gp 时，测试 C 代码不能隐含依赖原 gp。构建应禁用 small-data 与相应链接松弛，不修改全局框架解决这一问题。
- sie 保持为零，避免测试 SPIE 时引入外部中断源。
- 调用前后的 sp 均应正确，C handler 必须收到真正的 trapframe 基址。
- 单独检查 SIE 是否采用保存或编辑后的 SPIE；不能因为兼容旧版本而略去这一关键关系。
- SBI 关机可能给出零退出码，驱动仍必须识别夹具输出的 FAIL。

[SPECIFICATION]

## 场景一：四字节断点与完整上下文

**Pre-Condition：** 独立镜像已初始化栈、stvec、sscratch，SIE 关闭，sie=0。探针具有可恢复的 C 调用者状态。

**Post-Condition：** 保存帧和返回快照均与已知模式值一致，正确恢复原始 sp、ra，并能正常回到测试 C 调用者。

**Case：** 显式编码 `0x00100073`。进入 handler 后检查 cause=3、SPP=1、SIE=0、epc 指向该指令、sscratch=0、帧基址加帧大小等于原始 sp。检查全部 32 槽；handler 将 epc 指向下一条指令，返回后再次检查全部 GPR。

**Requirements：** 不用对帧内值的检查替代返回后快照，不只检查 s0～s11。固定指令长度通过明确编码保证。

## 场景二：两字节断点

**Pre-Condition：** 支持压缩指令的实际实验环境，SIE 关闭；使用新的场景状态。

**Post-Condition：** 不丢失寄存器，准确从两字节断点后继续执行，不跳过其他指令。

**Case：** 显式编码 `0x9002`，检查同样的帧与返回寄存器。handler 根据该场景的明确返回标签设置 epc。

**Requirements：** 不能用汇编器可能自动选择长度的裸 ebreak 代替长度明确的测试。

## 场景三：handler 修改有效帧

**Pre-Condition：** 输入 GPR 为已知模式值，handler 已确认入口保存正确。

**Post-Condition：** 指定寄存器反映修改，其他寄存器保持原值；控制流使用新 epc；返回后的 SIE=1，SPP 清零，SPIE 符合预先选择的环境预期。

**Case：** handler 修改 t0、a0、s2 槽，将 SPIE 置 1，并将 epc 指向跳过一条会改变 t0 的指令之后。返回快照必须检查这些修改，若执行了应跳过的指令则失败。

**Requirements：** 场景一、二的 SIE 预期为零，场景三为一；这一差异必须实测。SPIE 后值按环境规格检查，不在生产汇编中强行置位掩盖模拟器行为。

## 运行驱动与失败处理

**Pre-Condition：** 真实源码和工具可访问，正式工程未被本测试修改。

**Post-Condition：** 获得环境、构建、输出与明确结论；独立测试结果不被当作 Context 评分、完整整机、时钟中断或用户态切换结果。

**Case 1：全部通过。** QEMU 正常退出，三个 PASS 与汇总齐全，未出现 FAIL；结果记录为通过。

**Case 2：夹具检查失败。** 保存 phase、检查项及实际值，即使 QEMU 零退出仍判失败。

**Case 3：构建失败、工具缺失或超时。** 非零返回，保存已有日志，不输出总体成功。

**Case 4：版本差异。** 先依据可核对的源码解释差异，再修订环境预期；保留原始失败与修订依据，等待重新运行，不提前填写成功。

**Requirements：** 首次失败和修订后输出分开保存；禁止修改原始评分测试或队友的正式代码。记录哪些是测试问题、实现问题或尚未定位的问题。

<a id="final-validation"></a>

## 杨茜茹：完整内核复验与报告整理

本阶段没有修改正式实现，主要检查已集成的代码，并补充本人报告与截图。以下为本阶段交互要求的整理，不作为原始逐字提示词。本节路径相对于仓库根目录，构建和运行命令须在 `code/` 中执行，或从仓库根目录使用 `make -C code`。

[PROMPT]

检查最新集成版本中本人负责的 Trap 入口与上下文恢复实现，指导我在 WSL 中完成完整构建、课程原有评分和正常内核运行。根据实际反馈整理本人报告，添加真实截图，更新已完成事项。

[RELY]

- 正式实现位于 `code/kern/trap/trapentry.S`，帧布局由 `code/kern/trap/trap.h` 定义。
- 正式中断、异常处理及物理内存管理模块已经集成。
- 本轮验证基线为 `625b8d8`。
- 运行结果由本人在 VS Code 的 WSL 终端执行后提供，不能将队友的结果记为本人执行。
- 本人此前的 3/3 独立上下文测试属于阶段记录，不能替代最新完整内核验证。

[GUARANTEE]

- 保留正式实现、原有测试和评分条件；发现问题先定位原因。
- 仅更新 `report/report.md` 中本人相关记录，并添加本人真实截图。
- 保留队友的操作过程、结果及待填写内容。
- 未实际执行的测试不得标记为通过。
- 正式评分的覆盖范围与历史独立测试分别说明。

[SPECIFICATION]

**Case 1：完整构建与评分**

清理后构建内核并运行 `make grade`，记录 Context、Illegal、Timer 各项结果及总分；失败时保留错误输出。

**Case 2：正常运行**

重新清理并运行 `make qemu`，观察内存自检、持续时钟中断和自动关机，并确认返回终端。

**Case 3：代码复核**

核对帧布局、传入 `trap()` 的参数、CSR 和通用寄存器恢复顺序，以及最后恢复 `sp` 的要求。汇编采用处理后的 EPC，不统一增加固定指令长度。

**Case 4：报告整理**

将实际命令、验证版本、结果及真实截图写入本人报告，更新过时待办。不得把源码复核、历史测试和本轮动态结果混为一谈。

---

## 程子涵：中断、时钟与补充实验

以下是 2026-10-28 个人报告中已保存的规格和提示词整理稿，保持当时依赖状态描述；不是本轮重新输入的原始逐字对话。四段结构映射为：任务对应 PROMPT，输入对应 RELY，接口/约束对应 GUARANTEE，可观察验收对应 SPECIFICATION。

#### 中断、时钟与关机

```text
[PROMPT]
你是负责 RISC-V ucore Lab3 中断模块的成员 B，程子涵 2413921。

[RELY]
初始基线为 NKOS 的 lab3_yang（17205d43fa49958bf965b046eb87d96c57bd36f9）；本轮集成基线为 e7ab36b。
读取 trap.c、trap.h、clock.c/h、intr.c/h、riscv.h、sbi.h、原评分脚本。
A 已实现 trapentry.S 的 SAVE_ALL→trap→RESTORE_ALL→sret 闭环。
C 的 PMM 和 exception_handler 尚未提供；不要用生产空桩假装已经集成。

[GUARANTEE]
仅在 trap.c 的 lab3 function2 区域增加 interrupt_handler 及所需声明/宏。
保持原函数签名、SAVE_ALL、trapframe、trap_dispatch、trap、print_ticks 和
其他原公共函数不变；保留实验标记，署名2413921。
用无符号最高位掩码获取原因编号，允许原评分直接传入裸编号15。
软件/时钟/外部中断按现有 IRQ 宏分类；S态软件中断清除SSIP。
每次S态时钟：先重设下一次事件，再仅增加一次ticks。
每到TICK_NUM=100的整数倍调用原print_ticks。
统计输出计数达到SHUTDOWN_AFTER时调用原sbi_shutdown。
默认阈值10（材料未给数值，本次选择，须注明），支持编译参数覆盖，拒绝非正值。
保留 Supervisor software interrupt / Unknown interrupt: cause 15 / 100 ticks。
未知原因调用print_trapframe(tf)。不修改任何中断帧字段，尤其不推进异步中断EPC。
不要在C handler里直接执行sret，不修改评分脚本逻辑。

[SPECIFICATION]
真实trap.c与A汇编在QEMU中持续返回，主流程能观察ticks=200。
默认10次统计后在1000 ticks关机；阈值3时300 ticks关机。
边界99/100/101恰好计数、只在100输出；非时钟不改ticks。
带/不带中断标志的未知编号15均打印完整帧，handler不改EPC/status/GPR。
DEBUG_GRADE触发既有100 ticks处panic调用路径，区别于原GRADE_LAB3_*宏。
保留原始输出、编译参数、退出码和源码哈希，不能把独立测试算作原评分。
```

#### stvec 扩展

```text
[RELY]
以B1和A真实trapentry.S为底层；查官方v1.9.1的stvec章节、v1.10修订说明、
现行Supervisor ISA的stvec模式和WARL规则，记录章节及链接。

[GUARANTEE]
解释BASE低两位补零、MODE=00/01及保留编码，Direct所有Trap走BASE，
Vectored同步异常走BASE、异步中断走BASE+4*cause，RV64乘数仍是4。
解释旧版本低两位为零为何在新版本仍对应Direct，并限制兼容性结论范围。
独立扩展目录构建真实4字节跳转槽，关闭压缩/松弛并充分对齐。
跳板记录真实槽号，保存并恢复所用临时寄存器、原sp、sscratch约定，再进入A入口。
写stvec后读回；平台不支持时记录SKIP/受限，不伪报通过。
在两种模式中触发真实四字节断点、真实SSIP、真实连续时钟，检查槽号与返回；
断点处理仅属于实验夹具，不填入C的正式函数。
结束恢复stvec，不修改基础idt_init或正式Makefile，不把用户态切换测试算作时钟测试。

[SPECIFICATION]
检查timer槽符号相对BASE恰为20字节，并保存反汇编。
本平台支持时：Direct三个事件皆槽0；Vectored断点槽0、软件槽1、时钟槽5。
原始日志可复现，其他平台受限情况有明确分支。
```

#### 临界区与宏

```text
[RELY]
阅读sync.h、intr.c、atomic.h及pmm.c的alloc/free/nr_free包装路径。

[GUARANTEE]
解释do/while(0)的一条语句语义、调用方分号、if/else和局部作用域。
用基线头文件实际重现restore宏自带分号引发的else编译错误，保存日志；
将restore包装成do/while(0)，不改变原save/restore函数语义。
运行验证初始SIE=0和1、独立保存变量的LIFO嵌套、if/else的save/restore调用。
在真实时钟中断期间进入临界区，等待超过定时间隔，检查ticks不变且STIP挂起；
退出恢复后验证handler继续运行。
解释只屏蔽本hart可屏蔽中断的范围，以及与多核锁、AMO和内存序的区别。
对现有常用原子位操作做冒烟验证，完整PMM运行仍标记需C代码。

[SPECIFICATION]
不能无条件开中断，内层恢复不能提前开启，外层恢复回到原状态。
每种结果必须来自实际CSR/执行路径观察，不能只打印预设状态。
```

## 于蕾：Lab2 物理内存管理依赖提示词（待填写）

待于蕾填写本次实际用于迁入或补齐 PMM、Best-Fit 的提示词；如复用 Lab2 提示词，说明来源和本次适配。

## 于蕾：异常处理提示词（待填写）

待于蕾按四段模板填写非法指令、断点长度识别与异常兜底的实际提示词。
