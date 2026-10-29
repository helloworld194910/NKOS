# 文件用途索引

本页说明当前分支中每个文件的用途。GitHub 列表右侧显示的是最近一次修改该路径的提交说明；文件职责请以本索引和相应接口文档为准。

成员：杨茜茹（2412790，Trap 汇编与态切换底层）、程子涵（2413921，中断时钟与机制补充）、于蕾（2411824，内存依赖、异常与最终集成交付）。

已归档的源码与运行证据保持原样；尚未取得的队友实现、原评分和正式上层结果不写成完成。新运行日志生成在 code/logs/，已提交的历史记录位于 report/logs/。

## 首页与配置

| 文件 | 用途与当前归属 |
| --- | --- |
| [.gitignore](../.gitignore) | 排除构建产物、运行缓存和新生成日志；不忽略已归档的报告证据。 |
| [README.md](../README.md) | 仓库首页：代码、共用报告、交接和复现命令的统一入口。 |

## 工程代码与原工具

| 文件 | 用途与当前归属 |
| --- | --- |
| [code/Makefile](../code/Makefile) | RISC-V 编译、链接、QEMU/GDB 与原评分目标；工程工作目录为 code/。 |
| [code/README.md](../code/README.md) | 工程布局、模块归属、独立测试入口及当前验证范围。 |
| [code/extensions/member_a_switch/switch.S](../code/extensions/member_a_switch/switch.S) | 已取消 Challenge 1 的历史探索材料，不属于当前课程要求、正式交接或验收待办。 |
| [code/extensions/member_a_switch/switch.h](../code/extensions/member_a_switch/switch.h) | 已取消 Challenge 1 的历史探索材料，不属于当前课程要求、正式交接或验收待办。 |
| [code/kern/debug/assert.h](../code/kern/debug/assert.h) | 内核断言和 panic 相关定义。 |
| [code/kern/debug/kdebug.c](../code/kern/debug/kdebug.c) | 内核符号、调试信息和栈相关打印辅助。 |
| [code/kern/debug/kdebug.h](../code/kern/debug/kdebug.h) | 内核调试辅助接口声明。 |
| [code/kern/debug/kmonitor.c](../code/kern/debug/kmonitor.c) | 内核监视器命令处理与交互入口。 |
| [code/kern/debug/kmonitor.h](../code/kern/debug/kmonitor.h) | 内核监视器接口声明。 |
| [code/kern/debug/panic.c](../code/kern/debug/panic.c) | 内核错误终止及错误输出支持。 |
| [code/kern/debug/stab.h](../code/kern/debug/stab.h) | STABS 调试信息条目结构和常量；框架保留文件。 |
| [code/kern/driver/clock.c](../code/kern/driver/clock.c) | 读取时间、设置下一次 SBI 时钟事件并初始化 ticks。 |
| [code/kern/driver/clock.h](../code/kern/driver/clock.h) | 时钟初始化、下一事件与 ticks 的公共声明。 |
| [code/kern/driver/console.c](../code/kern/driver/console.c) | 控制台字符输入输出及控制台初始化。 |
| [code/kern/driver/console.h](../code/kern/driver/console.h) | 控制台输入输出接口声明。 |
| [code/kern/driver/dtb.c](../code/kern/driver/dtb.c) | 解析启动设备树并提取内存起点和大小。 |
| [code/kern/driver/dtb.h](../code/kern/driver/dtb.h) | 启动 hart/DTB 信息及内存查询接口声明。 |
| [code/kern/driver/intr.c](../code/kern/driver/intr.c) | 通过 sstatus 的 SIE 位提供中断开关操作。 |
| [code/kern/driver/intr.h](../code/kern/driver/intr.h) | 开关中断的声明。 |
| [code/kern/init/entry.S](../code/kern/init/entry.S) | 启动汇编、启动栈与 Sv39 启动页表；供正式内核和独立镜像使用。 |
| [code/kern/init/init.c](../code/kern/init/init.c) | 内核 C 初始化主流程，串联控制台、内存、Trap 与时钟等初始化。 |
| [code/kern/libs/stdio.c](../code/kern/libs/stdio.c) | 内核格式化输出、字符串和字符控制台接口的适配。 |
| [code/kern/mm/best_fit_pmm.c](../code/kern/mm/best_fit_pmm.c) | Best-Fit 模块及已有检查；C 负责补齐并核对本次框架兼容性。 |
| [code/kern/mm/best_fit_pmm.h](../code/kern/mm/best_fit_pmm.h) | Best-Fit 管理器对象的对外声明。 |
| [code/kern/mm/default_pmm.c](../code/kern/mm/default_pmm.c) | 框架提供的 First-Fit 管理器与检查，作为页面管理参考实现。 |
| [code/kern/mm/default_pmm.h](../code/kern/mm/default_pmm.h) | First-Fit 管理器对象声明。 |
| [code/kern/mm/memlayout.h](../code/kern/mm/memlayout.h) | 内存布局、Page 描述符、页状态与空闲区结构。 |
| [code/kern/mm/mmu.h](../code/kern/mm/mmu.h) | 页大小、分页相关常量及地址索引等宏。 |
| [code/kern/mm/pmm.c](../code/kern/mm/pmm.c) | 物理内存初始化、页分配包装与地址转换相关状态；C 补齐 Lab2 缺口。 |
| [code/kern/mm/pmm.h](../code/kern/mm/pmm.h) | 物理内存管理器接口、页描述符与虚实地址转换辅助接口。 |
| [code/kern/sync/sync.h](../code/kern/sync/sync.h) | 中断状态保存和恢复的临界区封装；B 分析并验证。 |
| [code/kern/trap/trap.c](../code/kern/trap/trap.c) | Trap 初始化、打印与分派框架；B 负责 interrupt_handler，C 负责 exception_handler。 |
| [code/kern/trap/trap.h](../code/kern/trap/trap.h) | pushregs、trapframe 的字段布局与 Trap 相关声明；汇编槽位依赖此布局。 |
| [code/kern/trap/trapentry.S](../code/kern/trap/trapentry.S) | A 已完成：__alltraps 传入真实帧，RESTORE_ALL 恢复有效上下文并衔接 sret。 |
| [code/libs/atomic.h](../code/libs/atomic.h) | 位操作等原子辅助；Page 状态宏等依赖它。 |
| [code/libs/defs.h](../code/libs/defs.h) | 基础类型、常用宏与结构成员到容器的转换支持。 |
| [code/libs/error.h](../code/libs/error.h) | 框架错误码定义。 |
| [code/libs/list.h](../code/libs/list.h) | 侵入式双向循环链表及插入、删除、遍历操作。 |
| [code/libs/printfmt.c](../code/libs/printfmt.c) | printf 风格格式串解析与输出格式化。 |
| [code/libs/readline.c](../code/libs/readline.c) | 控制台逐行读取与输入编辑支持。 |
| [code/libs/riscv.h](../code/libs/riscv.h) | RISC-V CSR、异常原因、中断位及体系结构操作定义。 |
| [code/libs/sbi.c](../code/libs/sbi.c) | 通过 SBI 向固件请求控制台、时钟及关机等服务。 |
| [code/libs/sbi.h](../code/libs/sbi.h) | SBI 调用及具体服务接口声明。 |
| [code/libs/stdarg.h](../code/libs/stdarg.h) | 可变参数宏支持，供格式化输出使用。 |
| [code/libs/stdio.h](../code/libs/stdio.h) | 内核字符、字符串及格式化输入输出声明。 |
| [code/libs/string.c](../code/libs/string.c) | 内存和字符串基本操作实现。 |
| [code/libs/string.h](../code/libs/string.h) | 内存和字符串操作声明。 |
| [code/tests/member_a/context_fixture.c](../code/tests/member_a/context_fixture.c) | 独立内核及测试 handler；检查真实帧、CSR 和处理后帧生效。 |
| [code/tests/member_a/context_probe.S](../code/tests/member_a/context_probe.S) | 显式 2/4 字节断点、通用寄存器与返回状态探针。 |
| [code/tests/member_a/run_context.py](../code/tests/member_a/run_context.py) | 构建真实 trapentry.S 的独立镜像，按 QEMU 版本核对 SPIE，并检查三场景。 |
| [code/tests/member_a_switch/abi_probe.S](../code/tests/member_a_switch/abi_probe.S) | 已取消 Challenge 1 的历史探索材料，不属于当前课程要求、正式交接或验收待办。 |
| [code/tests/member_a_switch/fixture.c](../code/tests/member_a_switch/fixture.c) | 已取消 Challenge 1 的历史探索材料，不属于当前课程要求、正式交接或验收待办。 |
| [code/tests/member_a_switch/run_switch.py](../code/tests/member_a_switch/run_switch.py) | 已取消 Challenge 1 的历史探索材料，不属于当前课程要求、正式交接或验收待办。 |
| [code/tests/member_a_switch/user_probe.S](../code/tests/member_a_switch/user_probe.S) | 已取消 Challenge 1 的历史探索材料，不属于当前课程要求、正式交接或验收待办。 |
| [code/tools/boot.ld](../code/tools/boot.ld) | 框架保留的引导链接脚本；当前内核链接设置另见 Makefile/kernel.ld。 |
| [code/tools/function.mk](../code/tools/function.mk) | Makefile 使用的编译、依赖与目标生成辅助宏。 |
| [code/tools/gdbinit](../code/tools/gdbinit) | 客体 GDB 的初始化配置。 |
| [code/tools/grade.sh](../code/tools/grade.sh) | 课程原有评分驱动，组织测试并判定输出。 |
| [code/tools/inject.py](../code/tools/inject.py) | 评分时在临时工程注入测试所需代码。 |
| [code/tools/kernel.ld](../code/tools/kernel.ld) | 高地址内核的段布局、入口和链接符号定义。 |
| [code/tools/kernel_nopage.ld](../code/tools/kernel_nopage.ld) | 框架保留的未启用高地址分页布局脚本。 |
| [code/tools/sign.c](../code/tools/sign.c) | 框架保留的传统启动扇区签名工具，含 510 字节长度检查。 |
| [code/tools/test_lab3.c](../code/tools/test_lab3.c) | 课程原有 Trap/中断/异常测试实现，保留原测试内容。 |
| [code/tools/test_lab3.h](../code/tools/test_lab3.h) | 课程原测试的接口与配置信息。 |
| [code/tools/test_lab3_context.S](../code/tools/test_lab3_context.S) | 原 Context 评分的寄存器和上下文汇编探针。 |
| [code/tools/vector.c](../code/tools/vector.c) | 框架保留的 x86 风格向量代码生成工具；不等同于本次 RISC-V stvec 向量表实现。 |

## 报告、交接与设计说明

| 文件 | 用途与当前归属 |
| --- | --- |
| [report/checklist.md](checklist.md) | A 已完成事项、等待的具体输入及后续联调清单。 |
| [report/demo.md](demo.md) | A 的独立验证命令、讲解要点和尚未执行的 GDB 观察方案。 |
| [report/file_guide.md](file_guide.md) | 本索引，逐项说明文件用途与贡献、证据范围。 |
| [report/handoff/README.md](handoff/README.md) | 小组交接索引，指向给程子涵、于蕾的说明及共用接口。 |
| [report/archive/s-u-s/switch_contract.md](archive/s-u-s/switch_contract.md) | 已取消 Challenge 1 的历史探索材料，不属于当前课程要求、正式交接或验收待办。 |
| [report/handoff/to_member_b.md](handoff/to_member_b.md) | 给程子涵：A 已完成内容、中断返回约束及需要 B 提供的代码与结果。 |
| [report/handoff/to_member_c.md](handoff/to_member_c.md) | 给于蕾：Lab2 依赖、异常处理、正式联调与最终集成提交；不再要求用户态上层。 |
| [report/handoff/trap_return_contract.md](handoff/trap_return_contract.md) | 基础 Trap 帧传递、寄存器恢复和异常/中断返回的衔接约定。 |
| [report/notes/member_a_context_validation.md](notes/member_a_context_validation.md) | 上下文独立验证的范围、断言和 SPIE 预期修订说明。 |
| [report/archive/s-u-s/module_report.md](archive/s-u-s/module_report.md) | 已取消 Challenge 1 的历史探索材料，不属于当前课程要求、正式交接或验收待办。 |
| [report/archive/s-u-s/validation.md](archive/s-u-s/validation.md) | 已取消 Challenge 1 的历史探索材料，不属于当前课程要求、正式交接或验收待办。 |
| [report/notes/member_a_trap_context.md](notes/member_a_trap_context.md) | 基础 Trap 实现设计、寄存器槽位与偏移说明。 |
| [report/notes/prompts/member_a_context_validation.md](notes/prompts/member_a_context_validation.md) | 上下文独立验证的单独提示词分稿。 汇总入口为 report/prompt.md。 |
| [report/archive/s-u-s/prompt.md](archive/s-u-s/prompt.md) | 已取消 Challenge 1 的历史探索材料，不属于当前课程要求、正式交接或验收待办。 |
| [report/notes/prompts/member_a_trap_context.md](notes/prompts/member_a_trap_context.md) | Trap 基础实现的单独提示词分稿。 汇总入口为 report/prompt.md。 |
| [report/prompt.md](prompt.md) | 当前正式提示词汇总：杨茜茹两份完整提示词，以及程子涵、于蕾的正式任务待填位置。 |
| [report/report.md](report.md) | 当前小组报告；按最新版手册移除已取消的态切换任务，待填位置按姓名标明。 |

## 真实截图

| 文件 | 用途与当前归属 |
| --- | --- |
| [report/images/member-a-context.png](images/member-a-context.png) | A 在 WSL 运行上下文独立测试的真实截图，三场景通过。 |
| [report/images/member-a-switch.png](images/member-a-switch.png) | 已取消 Challenge 1 的历史探索材料，不属于当前课程要求、正式交接或验收待办。 |

## 运行记录与迭代快照

| 文件 | 用途与当前归属 |
| --- | --- |
| [report/logs/member-a-before-spie-fix-source/docs/member_a_context_validation.md](logs/member-a-before-spie-fix-source/docs/member_a_context_validation.md) | 首次 SPIE 预期修订前的原始文件快照；用于复查迭代，不作为当前代码或当前提示词。 |
| [report/logs/member-a-before-spie-fix-source/prompts/member_a_context_validation.md](logs/member-a-before-spie-fix-source/prompts/member_a_context_validation.md) | 首次 SPIE 预期修订前的原始文件快照；用于复查迭代，不作为当前代码或当前提示词。 |
| [report/logs/member-a-before-spie-fix-source/prompts/member_a_trap_context.md](logs/member-a-before-spie-fix-source/prompts/member_a_trap_context.md) | 首次 SPIE 预期修订前的原始文件快照；用于复查迭代，不作为当前代码或当前提示词。 |
| [report/logs/member-a-before-spie-fix-source/tests/member_a/context_fixture.c](logs/member-a-before-spie-fix-source/tests/member_a/context_fixture.c) | 首次 SPIE 预期修订前的原始文件快照；用于复查迭代，不作为当前代码或当前提示词。 |
| [report/logs/member-a-before-spie-fix-source/tests/member_a/run_context.py](logs/member-a-before-spie-fix-source/tests/member_a/run_context.py) | 首次 SPIE 预期修订前的原始文件快照；用于复查迭代，不作为当前代码或当前提示词。 |
| [report/logs/member-a-context-before-spie-fix-build.log](logs/member-a-context-before-spie-fix-build.log) | 首次 SPIE 预期不符的失败运行的编译/链接命令与输出；保留原始记录。 |
| [report/logs/member-a-context-before-spie-fix-qemu.log](logs/member-a-context-before-spie-fix-qemu.log) | 首次 SPIE 预期不符的失败运行的固件启动与测试输出；保留原始记录。 |
| [report/logs/member-a-context-before-spie-fix-result.json](logs/member-a-context-before-spie-fix-result.json) | 首次 SPIE 预期不符的失败运行的源码哈希、环境和判定结果；保留原始记录。 |
| [report/logs/member-a-context-build.log](logs/member-a-context-build.log) | 上下文独立验证的编译/链接命令与输出；保留原始记录。 |
| [report/logs/member-a-context-environment.log](logs/member-a-context-environment.log) | 上下文独立验证的实际环境及版本依据；保留原始记录。 |
| [report/logs/member-a-context-qemu.log](logs/member-a-context-qemu.log) | 上下文独立验证的固件启动与测试输出；保留原始记录。 |
| [report/logs/member-a-context-result.json](logs/member-a-context-result.json) | 上下文独立验证的源码哈希、环境和判定结果；保留原始记录。 |
| [report/logs/member-a-switch-build.log](logs/member-a-switch-build.log) | 已取消 Challenge 1 的历史探索材料，不属于当前课程要求、正式交接或验收待办。 |
| [report/logs/member-a-switch-environment.log](logs/member-a-switch-environment.log) | 已取消 Challenge 1 的历史探索材料，不属于当前课程要求、正式交接或验收待办。 |
| [report/logs/member-a-switch-qemu.log](logs/member-a-switch-qemu.log) | 已取消 Challenge 1 的历史探索材料，不属于当前课程要求、正式交接或验收待办。 |
| [report/logs/member-a-switch-result.json](logs/member-a-switch-result.json) | 已取消 Challenge 1 的历史探索材料，不属于当前课程要求、正式交接或验收待办。 |
| [report/logs/member-a-trapentry-build.log](logs/member-a-trapentry-build.log) | Trap 汇编单文件编译的编译/链接命令与输出；保留原始记录。 |

## 教师取消 Challenge 1 后的说明入口

| 文件 | 用途 |
| --- | --- |
| [report/archive/s-u-s/README.md](archive/s-u-s/README.md) | 标明旧版任务已取消；历史内容不作为现行分工和验收要求。 |
| [report/archive/s-u-s/report-before-cancellation.md](archive/s-u-s/report-before-cancellation.md) | 标明旧版任务已取消；历史内容不作为现行分工和验收要求。 |
| [report/archive/s-u-s/prompts-before-cancellation.md](archive/s-u-s/prompts-before-cancellation.md) | 标明旧版任务已取消；历史内容不作为现行分工和验收要求。 |
| [code/extensions/member_a_switch/README.md](../code/extensions/member_a_switch/README.md) | 标明旧版任务已取消；历史内容不作为现行分工和验收要求。 |
| [code/tests/member_a_switch/README.md](../code/tests/member_a_switch/README.md) | 标明旧版任务已取消；历史内容不作为现行分工和验收要求。 |
