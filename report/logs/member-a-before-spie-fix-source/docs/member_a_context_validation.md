# Trap 上下文独立验证
负责人：2412790 杨茜茹

## 验证范围
链接当前工程的真实 `kern/trap/trapentry.S`、`kern/init/entry.S` 与 `tools/kernel.ld`；测试专用 C 入口和 trap 函数放在 tests/member_a，不进入正式内核构建。三个场景分别检查四字节断点、两字节断点，以及处理函数修改 GPR、epc、SPIE 后的恢复行为。每次都检查全部 32 个通用寄存器槽，包括原始 sp、ra、gp、tp、a0。

## 执行
在工程根目录执行 `python3 tests/member_a/run_context.py`。无需修改 Makefile，也不调用缺少其他成员实现的正式内核。构建位于 obj/member-a-context；运行有 15 秒上限，成功后通过 SBI 请求 QEMU 退出。

## 判定与记录
应看到三个 PASS、`RESULT: 3/3 context scenarios passed` 和 `ISOLATED_CONTEXT: PASS`。日志位于 logs/member-a-context-build.log、logs/member-a-context-qemu.log、logs/member-a-context-result.json。运行前结果标记为 false；只有构建、QEMU 正常退出以及全部输出标记均成功后才改为 true。

**当前状态：等待在 WSL 上执行，尚无运行结果。** 此验证不代表原有 Context 评分通过，不覆盖时钟中断、PMM、完整异常分派或 S→U→S Challenge。断点跳转策略由测试替身提供，用于检查汇编是否尊重修改后的帧，不替代 C 同学的正式异常处理实现。

## 测试自身的保护
探针先保存调用者寄存器，再设置测试模式值；返回后借用 sscratch 暂存 t6，以保留快照中的全部原值，随后清零。最终恢复调用者，避免测试自身破坏 C ABI。编译禁用 small-data 与链接松弛，保证刻意修改 gp 时测试 C 代码仍可执行；sie 为零，避免 SPIE 测试引入外部中断。
