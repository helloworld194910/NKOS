# Trap 帧传递与返回约定

负责人：2412790-杨茜茹。本文供程子涵、于蕾连接正式中断和异常模块使用。

## 基础接口

- `SAVE_ALL` 和 `struct trapframe` 保持框架布局：RV64 共 36 槽、288 字节。
- `__alltraps` 保存后将当前 sp 作为 a0，正常调用 `void trap(struct trapframe *tf)`。
- C 返回后 sp 仍定位原帧。`RESTORE_ALL` 采用处理后的 status、epc 与 GPR 槽；sp 最后恢复，再执行原 `sret`。
- 不恢复只用于诊断的 scause、sbadaddr，不强制将 SPP 清零，也不在汇编里固定推进 epc。
- [完整槽位与偏移](../notes/member_a_trap_context.md)。

## 程子涵的中断路径

完成时钟重设、ticks、统计和关机后正常返回；异步中断不固定推进 epc。请提供关机阈值、`DEBUG_GRADE` 差异、持续中断输出。杨茜茹配合检查能否重复返回及 SIE/SPIE 状态衔接。

## 于蕾的异常与内存路径

补齐 Lab2 PMM/Best-Fit 后提供可启动组合。正式 handler 负责区分非法指令、断点和其他异常，并决定 2/4 字节返回位置；杨茜茹配合检查反汇编、帧、sp/ra 与后续执行。未要求恢复的异常按手册输出帧，不任意推进 epc。

## 已有证据和后续范围

上下文独立测试在 QEMU 4.1.1 中 3/3 通过；首次 SPIE 预期问题及修订后的原始日志已保留。独立 handler 明确指定断点返回标签，因此这些结果不代表正式异常分类或原 Context/Illegal/Timer 评分通过。

仓库根目录运行 `python3 code/tests/member_a/run_context.py`。已有[运行日志](../logs/member-a-context-qemu.log)、[结果与源码哈希](../logs/member-a-context-result.json)、[截图](../images/member-a-context.png)。

正式代码齐备后再验证原 Context、两种长度断点及持续中断返回。程子涵组织评分与证据汇总，于蕾最终集成和提交。旧版用户态切换 Challenge 1 已取消，不属于本协议或后续待办。
