# Lab3 功能模块一：基础实现 Spec / Prompt

负责人：2412790 杨茜茹。日期：2026-10-27。本轮用于生成基础汇编实现；不包含用户态扩展，运行验证单独记录。

[PROMPT]
任务：在真实 Lab3 工程 kern/trap/trapentry.S 的两个 lab3 function1 标记处，实现 RESTORE_ALL 和 __alltraps 到 trap(tf) 的参数传递、调用，形成 SAVE_ALL -> trap -> RESTORE_ALL -> sret 闭环。必须修改真实文件，不只展示代码。两处 YOUR CODE 改成 2412790，保留实验标记和既有代码注释。

开始前核对 SAVE_ALL、trap.h、riscv.h、trap.c、Makefile 和 Context 测试。仅修改上述两处；不改 SAVE_ALL、trap.h、trap.c、PMM、启动汇编或原测试评分。不替 B/C 完成依赖，不自行提交推送。允许新增本人的 prompts/docs 说明。

[RELY]
- RV64：REGBYTES=8，LOAD=ld，STORE=sd。
- pushregs 按 x0～x31 排列，trapframe 之后为 status、epc、badvaddr、cause，共 36 槽、288 字节。
- xN 偏移为 N*8；原始 sp 在 16；status=256，epc=264，badvaddr=272，cause=280。
- SAVE_ALL 先暂存原始 sp 到 sscratch，再分配 frame；s0～s4 原值先保存，之后借用来读 CSR；csrrw 取回原始 sp 后把 sscratch 清零。
- void trap(struct trapframe *tf) 是真实 C 接口，首参数在 a0；处理函数可能合法修改 tf->epc，返回必须采用处理后的 frame。
- C 调用遵守 ABI，sp 按调用约定保持；ra、a0、临时寄存器可能被改变，原程序状态已经存在 frame。
- __trapret 已有 RESTORE_ALL 和 sret，必须保留。基础入口支持内核栈场景，没有完整用户态切栈，扩展独立设计。

[GUARANTEE]
必须实现：RESTORE_ALL 宏体；__alltraps 中调用 trap 的代码；两处学号署名。
必须保持：原 SAVE_ALL、结构体、接口和既有测试不变；GPR 恢复与槽一一对应；x0 恒零，无需无意义加载；恢复处理后的 sstatus 和 sepc；badvaddr/cause 留作诊断，不要求全部写回；原始 sp 最后恢复；恢复 CSR 使用的临时 GPR 最终恢复其原槽；不硬编码 SPP、SPIE、epc；不破坏 16 字节调用栈对齐。288 是 16 的倍数。

[SPECIFICATION]
## RESTORE_ALL
Pre：sp 指向已保存且经 C 处理后的有效 frame。
Post：x1、x3～x31 采用相应槽值，sp 采用第2槽；sstatus/sepc 采用 status/epc；不写坏 frame，随后可执行既有 sret。
Case 1：C 不修改 frame，恢复槽中的原值。
Case 2：C 更新 epc，采用更新值；汇编不自行添加固定指令偏移。
Case 3：C 合法更新某 GPR 槽，恢复采用该槽的新值。
Case 4：来自 S 态的 Trap，保持 frame 中返回 S 的意图，不强行切 U。
Requirements：先处理返回 CSR，再恢复借用的通用寄存器，最后恢复 sp；不能提前丢失 frame 基址或将 badvaddr/cause 当作 epc。

## __alltraps
Pre：硬件转入入口，当前为基础框架支持的内核栈场景。
Post：SAVE_ALL 后将 frame 的当前地址作为 trap 首参；普通调用返回后仍能定位同一 frame，再到 __trapret。
Case 1：同步异常，经 C 分类处理和 epc 调整后返回。
Case 2：异步中断，经 C 处理后正确返回原执行流。
Requirements：a0 不是原始程序 sp 或 CSR；调用应能返回，不用 tail；不能只靠 caller-saved 临时寄存器跨 C 调用保存 frame 地址。

## 验证
单文件汇编编译仅证明编译单元成立，不代表完整内核通过。静态检查偏移、GPR 槽、CSR、sp 顺序和修改边界。PMM 与 B/C handlers 不完整时明确待联调，不修改队友任务来伪造通过。后续运行 Context，并补查原始 sp/ra、a0 传参、处理后 epc、2/4 字节断点和持续时钟返回。未实跑不填写通过。