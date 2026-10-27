# Lab3：中断与异常

本分支为杨茜茹（2412790，成员 A）的阶段交付。基础汇编、S→U→S 底层实现及两组独立 QEMU 验证已完成；正式 PMM、异常/中断处理、用户态上层接入及原评分回归由后续分工完成。

## 小组交接

- **[交接总入口](docs/handoff/README.md)**
- **[给成员 B：中断时钟与返回联调](docs/handoff/to_member_b.md)**
- **[给成员 C：PMM、异常与用户态上层接入](docs/handoff/to_member_c.md)**
- [底层扩展接口与栈协议](docs/member_a_switch_handoff.md)

尚未开始工作的成员可从 `lab3_yang` 创建自己的工作分支，保留共同历史后并行实现。分模块提交，保留另一位成员在共享 trap.c 中的函数。

## 报告与证据

- [A 的个人报告](docs/member_a_report.md)
- [三份完整提示词](prompts/member_a_prompt_summary.md)
- [演示与截图步骤](docs/member_a_demo.md)
- [完成情况及后续依赖](docs/member_a_delivery_checklist.md)
- [基础上下文日志](logs/member-a-context-qemu.log) / [截图](images/member-a-context.png)
- [S→U→S 日志](logs/member-a-switch-qemu.log) / [截图](images/member-a-switch.png)

## 独立验证

在已配置 RISC-V GCC、objcopy、QEMU 的 WSL 中，于工程根目录运行：

```bash
python3 tests/member_a/run_context.py
```

```bash
python3 tests/member_a_switch/run_switch.py
```

两组各 3/3 通过，结果限定于独立测试。运行环境为 QEMU 4.1.1，版本差异及首次 SPIE 测试问题见报告。不能把这些结果当作原 Context/Illegal/Timer 评分；完整内核仍需对应模块齐备。

仓库只存代码、报告、提示词与证据；交接 ZIP 由本人单独发给队友，不上传。
