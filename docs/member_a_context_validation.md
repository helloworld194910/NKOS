# Trap 上下文独立验证
负责人：2412790 杨茜茹

## 验证范围
链接当前工程的真实 `kern/trap/trapentry.S`、`kern/init/entry.S` 与 `tools/kernel.ld`；测试专用 C 入口和 trap 函数放在 tests/member_a，不进入正式内核构建。三个场景分别检查四字节断点、两字节断点，以及处理函数修改 GPR、epc、SPIE 后的恢复行为。每次都检查全部 32 个通用寄存器槽，包括原始 sp、ra、gp、tp、a0。

## 执行
在工程根目录执行 `python3 tests/member_a/run_context.py`。无需修改 Makefile，也不调用缺少其他成员实现的正式内核。构建位于 obj/member-a-context；运行有 15 秒上限，成功后通过 SBI 请求 QEMU 退出。

## 判定与记录
应看到三个 PASS、`RESULT: 3/3 context scenarios passed` 和 `ISOLATED_CONTEXT: PASS`。日志位于 logs/member-a-context-build.log、logs/member-a-context-qemu.log、logs/member-a-context-result.json。运行前结果标记为 false；只有构建、QEMU 正常退出以及全部输出标记均成功后才改为 true。

**当前状态：本人已在 WSL 完成修订后的独立 QEMU 三场景验证，3/3 通过。** 此验证不代表原有 Context 评分通过，不覆盖时钟中断、PMM、完整异常分派或 S→U→S Challenge。断点跳转策略由测试替身提供，用于检查汇编是否尊重修改后的帧，不替代 C 同学的正式异常处理实现。

## 测试自身的保护
探针先保存调用者寄存器，再设置测试模式值；返回后借用 sscratch 暂存 t6，以保留快照中的全部原值，随后清零。最终恢复调用者，避免测试自身破坏 C ABI。编译禁用 small-data 与链接松弛，保证刻意修改 gp 时测试 C 代码仍可执行；sie 为零，避免 SPIE 测试引入外部中断。

## 第一次实际运行与修订

WSL 实际运行已完成四个编译单元、链接和镜像生成。QEMU 启动后在场景零报告：

```text
FAIL member-A phase=0x0000000000000000 sret SPIE mismatch got=0x8000000000006000
ERROR: QEMU failed/incomplete (exit=0)
```

原测试把 sret 后 SPIE 必须为 1 当作不区分环境的检查，未考虑 QEMU 4.1.1 `helper_sret` 明确清零 SPIE 的实现。问题位于独立测试的环境假设；现有输出不能据此认定正式汇编有误，也不能认定三个场景通过。

源码依据：https://github.com/qemu/qemu/blob/v4.1.1/target/riscv/op_helper.c#L68-L93

修订后先读取实际 QEMU 版本，4.1.1 对应 SPIE=0，其余可识别版本按标准 SPIE=1 检查。继续严格验证前两个场景 SIE=0、第三个场景 SIE=1，并检查 SPP 清零。预期来自版本和源码，不能从被测输出反推。私改构建与上游可能不同，出现新差异仍需另行核对。

**修订后状态：本人已在 WSL 重新执行，三个场景均通过；正式 trapentry.S 未因该测试问题而修改。** 原失败日志在更新前复制保留为 logs/member-a-context-before-spie-fix-*，不覆盖原始记录。

## 2026-10-27 独立 QEMU 实际结果

本人在 WSL 执行独立验证，四个编译单元、链接和镜像生成成功。实际环境：QEMU emulator version 4.1.1；按运行前确定的环境规格检查返回后 SPIE=0。

| 场景 | 实际结果 |
| --- | --- |
| 四字节断点 | PASS；入口帧与返回后全部 GPR 检查通过，含原始 sp、ra、gp、tp、a0 |
| 两字节断点 | PASS；从明确返回标签继续执行，全部 GPR 检查通过 |
| C 修改帧 | PASS；t0、a0、s2、epc 修改生效，跳过指定指令，修改后的 SPIE 正确影响返回 SIE |

前两场景返回 SIE=0，第三场景返回 SIE=1；三个场景均检查 SPP 清零和环境规定的 SPIE 后值。总结果 `RESULT: 3/3 context scenarios passed`，QEMU 退出码为 0。测试直接链接真实 trapentry.S，测试专用 trap 不进入正式构建。

复现命令：`python3 tests/member_a/run_context.py`（工程根目录）。证据：logs/member-a-context-build.log、logs/member-a-context-environment.log、logs/member-a-context-qemu.log、logs/member-a-context-result.json。JSON 保存被测源码哈希，需与对应运行版本配合阅读。

首次因 SPIE 环境假设失败的输出与旧版文本已保留；此后修订了测试规格，未修改正式汇编。正式内核 Context/Illegal/Timer 评分、持续时钟中断返回和 S→U→S Challenge 均不属于本次通过范围。尚未引入其他成员的新实现。
