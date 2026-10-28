# Lab3：中断与异常

成员 B：程子涵，2413921。分支：`lab3_cheng`。

## 提交内容

```text
.
├── README.md             # 目录、运行方式与成员协作说明
├── 实验报告.md           # 原理、实现、Spec/Prompt、迭代记录与测试结果
├── images/               # 实验报告引用的六张实际运行截图
├── Makefile
├── kern/                 # 内核源码；B 主要修改 trap/trap.c 与 sync/sync.h
├── libs/                 # 公共库及 RISC-V/SBI 接口
├── tools/                # 课程构建、调试与原评分工具
└── extensions/member_b/  # stvec 与临界区实验源码、独立验证脚本
    ├── vector.S
    ├── fixture.c
    └── run.py
```

本分支提交源码与 [Markdown 实验报告](实验报告.md)。报告已包含老师要求的逻辑主线、核心函数理解、各练习 Spec/Prompt、真实问题与修复、知识点对照和未覆盖原理。报告引用的六张实际运行截图保存在 `images/`；原始日志、生成页面、中间产物和编译产物不上传。

## 环境与验证

已使用 WSL Ubuntu、SiFive GCC-Metal 10.2.0-2020.12.8 / RISC-V GCC 14.2.0+19、QEMU 4.1.1（virt，单 hart，128 MiB）验证。依赖 `riscv64-unknown-elf-gcc`、`objcopy`、`objdump`、`qemu-system-riscv64` 和 Python 3。

从工程根目录运行 B 的全部独立验证：

```bash
python3 extensions/member_b/run.py
```

仅验证扩展时可运行：

```bash
python3 extensions/member_b/run.py --cases sync stvec
```

完整六组场景覆盖中断分类、时钟边界、持续返回、默认/自定义关机、DEBUG_GRADE、stvec 两模式及临界区状态恢复；另检查非正关机阈值被拒绝。此前六组独立场景均通过。脚本直接链接正式中断处理、时钟代码及既有汇编入口，`fixture.c` 只提供实验所需启动、控制台和有限断点处理，不是正式异常实现。

运行会生成被 Git 忽略的 `obj/` 和 `logs/`，其中 `result.json` 记录环境、源码哈希及实际结果。独立脚本先实际试编译 CSR、fence.i 与 AMO 指令：新工具链采用 `rv64imac_zicsr_zifencei`，旧工具链采用 `rv64imac`，不要求更换编译器。探测输出保存为 `logs/member-b/isa-probe-build.log`，所选 ISA 记录在 `result.json`。脚本同时自动使用 `-Dsbadaddr=0x143`，兼容现代汇编器对旧 CSR 名称的限制，不修改原汇编。shell 脚本固定采用 LF 换行。

完整工程的常规命令为：

```bash
make
make qemu
make grade
```

当前 C 的 PMM 和正式异常处理尚未合入，完整构建仍在 `best_fit_pmm.c` 的缺失定义处失败；最近原评分为 0/100，三组均为构建失败。独立测试通过不等于完整内核通过。现代工具链构建完整工程时，需统一处理旧 CSR 名称；内层评分会设置 DEFS，应使用兼容工具链或对整个构建生效的同编号编译器包装，不改评分条件。

## 成员协作与后续合并

基于 A 的 `lab3_yang` 提交 `17205d43fa49958bf965b046eb87d96c57bd36f9`。仅保留 B 的个人内容；基础汇编入口、公共库和课程工具沿用原实现与署名。

B 已完成 `interrupt_handler()`、周期计数与关机、SSIP 清除、未知中断兜底，以及 `local_intr_restore` 的宏语句修复。`TICK_NUM=100`，默认 `SHUTDOWN_AFTER=10` 次统计后关机，可通过编译参数覆盖；材料没有规定具体数值，若课程补充要求不同应重新设置并验证。异步中断不推进 EPC，返回使用统一汇编流程；`DEBUG_GRADE` 在第一次统计后的原 panic 处停止，与原 `GRADE_LAB3_*` 评分配置不同。

合并时按函数处理 `kern/trap/trap.c`：保留 B 的中断函数和配置，在同一区域增加 C 的 `exception_handler()` 定义及其署名；不要整文件覆盖。C 还需提供完整 PMM、异常指令长度处理和所负责的用户态上层。A 可进一步复核中断返回状态、EPC 和栈恢复。B 的 `stvec` 实验保持独立，不更改基础 `idt_init()` 或正式 Makefile。

模块齐备后，在干净副本中依次检查 PMM 自检、原 Context/Illegal/Timer 评分、至少 200 次时钟返回、正常阈值关机、2/4 字节异常返回、未知异常兜底及实际 PMM 临界区状态。`images/` 中是 B 的阶段独立验证截图；最终小组报告的正常运行和完整评分截图仍需对应最终合并版本补充。不要用独立夹具替代正式异常处理，也不要修改评分脚本使失败变通过。
