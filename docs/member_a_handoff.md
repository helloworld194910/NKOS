# Lab3 成员 A 交接

负责人：2412790 杨茜茹。此文区分已经准备的基础实现与尚未共同确定的扩展协议，不代表已经测试通过。

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
- 当前Context没有完整独立覆盖sp/ra等，补充证据后再写结论。

## 4. A/C态切换扩展：待共同确认的协议清单

A：入口/返回辅助汇编、原内核上下文、用户/内核栈、sscratch和SPP/SPIE/sepc。
C：用户代码、ecall触发/识别、必要用户映射、上层返回验证。

开始前共同确定：入口函数签名和返回值；原内核上下文存储；U入口/栈和内核栈；各阶段sscratch的意义和实际切栈；ecall编号/参数/返回路径；stvec、页表/TLB与中断状态的隔离和恢复；扩展独立目录/分支及构建开关。

现有SAVE_ALL不切用户栈，启动映射U=0。不能只清SPP或将整块内核统一改成用户页。扩展不修改基础SAVE_ALL。上述为交接待确认项，不能当作两人已经同意的接口或已完成扩展。

## 5. 状态与报告

Spec/Prompt、偏移表和基础实现已准备；本人于2026-10-27在WSL完成RISC-V单文件编译，日志为logs/member-a-trapentry-build.log。完整Context、异常与持续时钟返回尚待联调；S→U→S尚未实现。单文件编译不是完整内核或动态行为通过。

A主写整体逻辑、功能模块一、扩展底层、个人知识点和AI经验；测试证据交B，最终整合交C。交付以本人WSL实际差异、编译日志及后续联调结果为准，不把助手访问WSL失败写成算法Bug。
