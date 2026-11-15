# Lab4 进程管理

小组成员为杨茜茹（2412790）、程子涵（2413921）和于蕾（2411824）。`lab4_cheng` 基于 `lab4_yang`，集成了虚拟内存页操作、进程创建和调度。

- [实验报告](report/report.md)
- [内核、构建和评分工具](code/)
- [进程创建与 PID 拓展](report/report.md#member-b)
- [进程创建规格与提示词](report/handoff/member-b-prompts.md)

在 WSL 或 Linux 中使用 RISC-V 交叉工具链和 QEMU：

```sh
cd code
make clean
make
make qemu
make grade
```

在 GCC 14 下，评分副本仅对注入的 `test_lab4.c` 将原测试回调的类型诊断保留为警告。
