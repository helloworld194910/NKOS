# Lab3 成员 A 交接

负责人：2412790 杨茜茹。此文区分已经准备的基础实现与尚未共同确定的扩展协议，验证范围以下文状态和日志为准。

## 1. 基础实现

只改 trapentry.S 的 RESTORE_ALL 及 __alltraps 两处。frame、trap签名不变；a0传当前frame地址；恢复处理后status/epc，原sp最后加载；不改PMM、B/C的handler、原测试评分或SAVE_ALL。

## 2. 给 B

- 中断处理后返回相同frame；不要错误推进异步中断的epc。
- 请完成时钟重设、ticks、统计和关机；阈值及DEBUG_GRADE差异由B确定，不在汇编中处理。
- 联调重点：重复中断能返回，SPIE/SIE状态和epc正确，评分能观察ticks=200。
- 本人的基础代码不改stvec、临界区或trap.c。

## 3. 给 C

- frame共288字节，布局见member_a_trap_context.md；handler更新tf->epc后，汇编采用新值。
- 请补齐Lab2依赖、exception_handler后联调Context；判断并正确跳过2/4字节断点属于C的handler工作，A协验反汇编与返回。
- 请互审a0参数、sp恢复次序、CSR临时寄存器和槽对应。
- A 的独立 QEMU 测试已覆盖全部 GPR（含 sp、ra、gp、tp、a0）；两种断点的跳转由测试夹具提供，正式 exception_handler 仍需完成和联调。

## 4. A/C态切换扩展：待共同确认的协议清单

A：入口/返回辅助汇编、原内核上下文、用户/内核栈、sscratch和SPP/SPIE/sepc。
C：用户代码、ecall触发/识别、必要用户映射、上层返回验证。

开始前共同确定：入口函数签名和返回值；原内核上下文存储；U入口/栈和内核栈；各阶段sscratch的意义和实际切栈；ecall编号/参数/返回路径；stvec、页表/TLB与中断状态的隔离和恢复；扩展独立目录/分支及构建开关。

现有SAVE_ALL不切用户栈，启动映射U=0。不能只清SPP或将整块内核统一改成用户页。扩展不修改基础SAVE_ALL。上述为交接待确认项，不能当作两人已经同意的接口或已完成扩展。

## 5. 状态与报告

Spec/Prompt、偏移表和基础实现已准备；本人于2026-10-27在WSL完成RISC-V单文件编译，日志为logs/member-a-trapentry-build.log。完整Context、异常与持续时钟返回尚待联调；S→U→S尚未实现。另已通过独立 QEMU 三场景验证；这不代表正式内核评分或用户态扩展通过。

A主写整体逻辑、功能模块一、扩展底层、个人知识点和AI经验；测试证据交B，最终整合交C。交付以本人WSL实际差异、编译日志及后续联调结果为准，不把助手访问WSL失败写成算法Bug。

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
