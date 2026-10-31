# Lab3 工程

本目录为工程根目录，包含 Makefile、kern/、libs/、tools/ 和独立机制验证代码。报告与截图位于 ../report/。

```bash
make
make grade
make
make qemu
python3 extensions/member_b/run.py
```

需要 RISC-V GCC/objcopy/objdump、QEMU、make 和 Python 3。完整构建沿用课程 Makefile，独立验证自动探测新旧工具链的 ISA 写法。

trapentry.S 由杨茜茹负责；trap.c 的 interrupt_handler、时钟与 sync.h 宏修复由程子涵负责；PMM 与 exception_handler 由于蕾负责。B 独立测试只为验证中断、stvec 和临界区，不替代原完整评分。临时编译时重命名正式 trap/exception 符号，夹具分派到真实 interrupt_handler，避免与集成后的正式异常定义冲突。

原评分 100/100，正常内核 PMM 自检通过，默认十次统计后自动关机。obj/、bin/、build/、logs/ 均为本地生成产物，不提交。B 独立产物使用 build/member-b/，避免被原评分 make clean 清理。用户态切换 Challenge 已取消。
