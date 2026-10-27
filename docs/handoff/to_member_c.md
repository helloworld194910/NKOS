# 给成员 C：PMM、异常处理与用户态上层接入

发送人：杨茜茹，2412790（成员 A）。本文对应 A 的代码与验证基线 `1004af6`；我的个人分支为 `lab3_yang`。

## 我已完成什么

- 基础入口与 RESTORE_ALL 已完成，真实汇编的独立 QEMU 测试 3/3 通过。
- S→U→S 底层已完成，正常往返、重复使用/重入拒绝、错误回调恢复三个独立场景通过。
- 保存/恢复内核调用上下文，用户 Trap 切到专用内核栈，支持继续 U 或回原 S 调用点。
- 已交付模板报告、三份完整提示词、实际迭代与日志、两张截图。

## 你的正式代码按这个接口接入

```c
long member_a_enter_user(uintptr_t entry, uintptr_t user_sp,
                         uintptr_t kernel_trap_stack_top);
long member_a_user_trap(struct trapframe *tf);
```

- 你准备用户入口、用户页表、用户栈和独立内核 Trap 栈。前两个参数为用户虚拟地址，第三个为内核地址且按 16 字节对齐。
- 上层 callback 返回 0：按修改后的帧继续 U；返回 1：结束并回原 S 调用点。
- enter_user 正常返回 0，非法回调动作返回 -1，活动期间重复进入返回 -2。
- 默认测试请求：a7=0x5901 表示继续，a7=0x5902 表示结束。它们不是课程指定的通用 syscall 编号；如需更改，同步调整用户程序与 callback。
- 你负责安装、撤销用户映射，恢复 satp 并刷新 TLB；我的底层不改 satp。内核页保持 U=0。
- 我保存内核 ra/sp/gp/tp/s0～s11 及 stvec/sscratch/sstatus/sepc/sie。扩展当前为单核、单实例、非嵌套、整数上下文，窗口内屏蔽中断。

接口已由我定稿，可以直接按约实现；有具体不兼容再沟通，不需要先等双方批准才能开始。详细栈协议见[底层接口与栈约定](../member_a_switch_handoff.md)。

## 我还需要你完成、提供什么

| 你负责完成 | 请提供给我的内容 | 我拿到后做什么 |
| --- | --- | --- |
| Lab2 依赖补齐 | pmm.c、best_fit_pmm.c 的实际实现及必要的兼容改动；编译/PMM 检查方法与结果 | 让正式内核具备运行 Context 等检查的条件 |
| 正式 exception_handler | 实际函数代码；非法指令、2/4 字节断点的长度处理，以及兜底输出；提供异常后继续执行的结果 | 配合调试原 Context，核对 sp/ra、epc、两种断点返回；这些工作不能由我的夹具替代 |
| S→U→S 上层 | 用户程序、映射与栈设置、member_a_user_trap 回调、调用入口和构建方式，提供完整文件或提交 | 将你的真实上层与我的底层连接，检查状态、栈和内核返回 |
| 最终集成 | 合并 B 后可复现的代码版本、命令、日志与已知问题 | 配合完整回归，核对基础入口未被扩展破坏及最终证据一致 |

**每次交接请给：** 分支与提交号、修改文件、运行命令、日志/截图、已知问题。先完成哪个模块就先给我哪个，不必等报告或全实验都完成。

共享 trap.c 请保留 B 的中断部分，按函数合并，不整段覆盖。原评分脚本和测试不改。

## 现成夹具如何使用

真实底层：[switch.S](../../extensions/member_a_switch/switch.S)、[switch.h](../../extensions/member_a_switch/switch.h)。

我的测试用户程序、页表及 callback 在 [tests/member_a_switch/](../../tests/member_a_switch/)，只作隔离验证。你可以参考或复用并注明来源，完成自己的正式接入与验证；不要求为避免复用而重写一套相同代码。报告中区分原有夹具、你的改动及共同联调即可。

## 仓库基线与复现

`lab3_yang` 包含 A 的阶段工程和未完成的其他模块，不是最终三人内核。如果尚未开始，请从该分支创建自己的工作分支，沿用共同历史后实现你的模块。已有修改先保留，再按提交或文件差异接入，不整目录覆盖。

在工程根目录运行：

```bash
python3 tests/member_a/run_context.py
```

```bash
python3 tests/member_a_switch/run_switch.py
```

两组独立测试各 3/3 通过，不等于原 Context/Illegal/Timer 评分通过。正式评分仍需你和 B 的模块齐备。

阅读入口：[个人报告](../member_a_report.md)、[完整提示词](../../prompts/member_a_prompt_summary.md)、[基础运行日志](../../logs/member-a-context-qemu.log)、[扩展运行日志](../../logs/member-a-switch-qemu.log)、[截图目录](../../images/)。
