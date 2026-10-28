# Lab3 Trap 上下文独立验证提示词

> 目录说明：文中的工程相对路径、编译和测试命令以仓库的 `code/` 为工作目录；原实测日志归档在 `report/logs/`，重新运行生成的日志位于 `code/logs/`。整理前的文件位置可在 Git 历史中查看。

**负责人：** 2412790 杨茜茹。

本文件为独立验证提示词的修订版。首次运行出现 SPIE 检查失败；本版补充按已核对的 QEMU 版本解释返回位。初始提示词与失败结果单独保留，不将本版写成首次输入。

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
