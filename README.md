# Lab2：物理内存和页表

成员：2411824 于蕾、2412790 杨茜茹、2413921 程子涵。

- [小组总报告](report/report.md)
- [按任务标注的完整提示词汇总](report/prompt.md)

源码在 code/。默认管理器 Best-Fit；BUDDY=1 选择 Buddy；SLUB_CHECK=1 开启 SLUB 检查；MMU_PROBE=1 开启地址翻译探针。切换构建参数前 make -C code clean。

旧 GCC 10.2.0 环境已验证使用 make -C code ARCH_FLAGS= qemu 和 make -C code ARCH_FLAGS= grade；现代工具链可使用 Makefile 默认架构参数。宿主测试入口为 make -C code buddy-host-test 和 make -C code member-c-host。QEMU 常驻等待时使用 Ctrl+A、松开后 X 退出。

原评分 20/20 只覆盖页初始化及默认 Best-Fit；Buddy 的 28/28 是独立宿主测试，SLUB 和 QEMU 双 GDB 的证据分别见总报告。文档整合不等于重新运行所有实验。
