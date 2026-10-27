# S→U→S 底层独立验证记录

负责人：2412790-杨茜茹。

## 当前状态

**本人已在 WSL 完成 RISC-V 编译、链接及 QEMU 三个独立场景，全部通过。** 实际环境为 QEMU 4.1.1、OpenSBI v0.4。Windows 侧静态检查在此前完成，当前运行结论以 WSL 日志为依据。

## 执行与判定

工程根目录：`python3 tests/member_a_switch/run_switch.py`。脚本沿用真实 kern/init/entry.S 与 tools/kernel.ld，产物位于 obj/member-a-switch，超时上限 15 秒，只运行自建 QEMU。它不构建正式 trap.c 或 PMM，也不修改共享 Makefile。

三个场景：正常用户恢复后回内核、重复整轮并检查重入拒绝、非法回调动作后恢复内核。每轮用户先在 U 页读写自己的栈，以 0x5901 ecall 进入 S；回调修改 a0 和 epc 后恢复 U，用户验证返回值再以 0x5902 结束。第三轮回调故意给出未知动作，预期底层返回 -1。

检查依据包括 cause=8、SPP=0、用户 epc/sp、位于专用内核栈上的帧与 C 栈、ABI s0～s11/gp/tp 模式值、原 stvec/sscratch/sstatus/sepc/sie 和 satp 边界。sscratch 原值使用哨兵，重复使用用于检查活动上下文是否释放。回调内尝试重入必须得到 -2。

成功汇总应为 `RESULT: 3/3 switch cases passed` 和 `ISOLATED_SWITCH: PASS`；缺少标记、FAIL、超时或非零退出均失败。日志位于 logs/member-a-switch-build.log、logs/member-a-switch-environment.log、logs/member-a-switch-qemu.log、logs/member-a-switch-result.json。

## 验证范围与交接

用户代码、三层测试页表和回调为 A 的验证夹具，不是上层成员的正式代码。接口协议见 member_a_switch_handoff.md。即使隔离测试通过，也仍需真实上层模块接入、用户态异常处理与基础内核完整评分回归。

## 参考

RISC-V Supervisor 规范中 sstatus/SPP、sscratch 和用户页权限：
https://docs.riscv.org/reference/isa/priv/supervisor.html

旧 QEMU 的 SPIE 实现差异仍按上一次基础验证记录解释，本轮不据其后值伪造版本无关结论。

## 2026-10-27 实际运行

```text
PASS: S-U-S with user resume and kernel context restoration
PASS: repeated S-U-S and nested-entry rejection
PASS: invalid callback action restores kernel context
RESULT: 3/3 switch cases passed
```

QEMU 正常退出，驱动同时核对输出标记和退出码。六次 OBSERVED 记录分别对应三轮各两次 U 态 ecall，原因均为 8、SPP 均为零。两处 epc 为 0x40001c、0x40003c，用户 sp 为 0x401ff0，本次 kernel_frame 为 0xffffffffc020dee0。测试自动核对 C 栈及帧均处于专用内核 Trap 栈范围。

测试还核对 s0～s11、gp、tp、原 stvec、sscratch、sstatus、sepc、sie 的恢复，以及底层未修改 satp。重复往返和错误动作返回都未遗留活动状态。没有观察到需要修正底层实现的失败，因而不补写虚构迭代。

基础必做文件未改动。完整内核、实际时钟处理和上层共同 Challenge 验收仍未执行；本次通过不替代这些项目。
