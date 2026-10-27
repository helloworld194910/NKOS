# Lab3 成员 A：基础 Trap 上下文设计

负责人：2412790 杨茜茹。阶段：本人 WSL 单文件编译与独立 QEMU 三场景验证通过；完整内核评分仍待联调。

## 1. 修改范围

只补 kern/trap/trapentry.S 的两处 lab3 function1。SAVE_ALL、trap.h、trap.c、PMM、启动及原测试评分不改。__alltraps 用当前 sp 作为 a0 调用 trap；返回后落入原 __trapret。

RESTORE_ALL 从处理后的 frame 读取 sstatus/sepc，先写回返回 CSR，再恢复通用寄存器，最后从第2槽加载原始 sp。x0 恒零不加载；badvaddr/cause 不作为返回状态写回。宏不自行推进 epc、不硬编码 SPP，故尊重 C 的异常处理结果。

## 2. 布局

当前为 RV64，REGBYTES=8，sizeof(pushregs)=256，完整 frame=288 字节。字段顺序直接核对当前 trap.h 与 SAVE_ALL；以下字节偏移相对 SAVE_ALL 完成后的 sp。

| 寄存器 | ABI 名称 | 槽号 | 字节偏移 |
| --- | --- | --- | --- |
| x0 | zero | 0 | 0 |
| x1 | ra | 1 | 8 |
| x2 | sp | 2 | 16 |
| x3 | gp | 3 | 24 |
| x4 | tp | 4 | 32 |
| x5 | t0 | 5 | 40 |
| x6 | t1 | 6 | 48 |
| x7 | t2 | 7 | 56 |
| x8 | s0 | 8 | 64 |
| x9 | s1 | 9 | 72 |
| x10 | a0 | 10 | 80 |
| x11 | a1 | 11 | 88 |
| x12 | a2 | 12 | 96 |
| x13 | a3 | 13 | 104 |
| x14 | a4 | 14 | 112 |
| x15 | a5 | 15 | 120 |
| x16 | a6 | 16 | 128 |
| x17 | a7 | 17 | 136 |
| x18 | s2 | 18 | 144 |
| x19 | s3 | 19 | 152 |
| x20 | s4 | 20 | 160 |
| x21 | s5 | 21 | 168 |
| x22 | s6 | 22 | 176 |
| x23 | s7 | 23 | 184 |
| x24 | s8 | 24 | 192 |
| x25 | s9 | 25 | 200 |
| x26 | s10 | 26 | 208 |
| x27 | s11 | 27 | 216 |
| x28 | t3 | 28 | 224 |
| x29 | t4 | 29 | 232 |
| x30 | t5 | 30 | 240 |
| x31 | t6 | 31 | 248 |
| CSR 字段 | status / sstatus | 32 | 256 |
| CSR 字段 | epc / sepc | 33 | 264 |
| CSR 字段 | badvaddr / sbadaddr | 34 | 272 |
| CSR 字段 | cause / scause | 35 | 280 |

## 3. 保存和恢复为何对应

SAVE_ALL 先暂存原 sp，再扣除288字节。其他 GPR 原值已保存后才借 s0～s4 读 CSR。因此恢复阶段可以借 s0/s1处理返回 CSR，然后从相应槽恢复原来的 s0/s1。最后恢复 x2，避免提前失去 frame 基址。

C 的第一个参数 a0 是 frame 地址，原程序 a0 已在第10槽，不因传参丢失。call trap 改写的 ra 最后会从第1槽恢复。正常 C ABI 保持调用返回后的 sp；288是16的倍数，原栈满足对齐时本轮不会破坏标准栈对齐。

当前基础实现只使用已有内核栈，不声称具备 U 态切栈。扩展需独立入口和 A/C 共同协议。

## 4. 当前已完成的检查

- 核对全部36槽字段、恢复 GPR 和 CSR 的对应关系。
- SAVE_ALL 和两处填空以外内容保持原样（源码规范化为LF）。
- 原始 sp 是恢复宏的最后一条加载；两处标记署名2412790。
- 助手直接访问 WSL 仍受限；本人已于 2026-10-27 在 WSL 执行独立汇编编译并返回成功日志。
- 编译命令：make obj/kern/trap/trapentry.o；输出含 + cc kern/trap/trapentry.S，退出码为 0。
- 构建日志：logs/member-a-trapentry-build.log；对象文件头核对为 RV64 little-endian RISC-V ELF。
- 独立测试镜像的 QEMU 三场景已通过，详见下方实际运行记录。正式内核 Context、Illegal、Timer 与整机评分仍待联调。

## 5. 后续验证

已在真实工程完成 make obj/kern/trap/trapentry.o，仅证明独立汇编单元成立。完整 Context 依赖 C 的 page_init/Best-Fit和异常处理；Timer依赖B。补充核对原 sp/ra、a0、处理后epc、2/4字节断点、持续中断返回。

原 Context 评分不是全部寄存器/CSR的穷举测试；尤其不能只凭35分声称sp/ra/gp/tp/a0每项都已单独动态验证。报告要区分实跑、静态检查和待验证。

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
