# Trap 上下文独立验证提示词
负责人：2412790 杨茜茹

[PROMPT]
必须修改工程内真实文件：只新增 tests/member_a/ 下的独立测试和本任务的说明。为真实 kern/trap/trapentry.S 建立可运行的 QEMU 测试，不等待其他成员的 PMM 和异常分派实现。不能用测试替身替换被测的汇编。

[RELY]
读取实际 trap.h、riscv.h、entry.S、trapentry.S 和 tools/kernel.ld。RV64 的 trapframe 为 36 个八字节槽；GPR 占前 32 槽，随后是 status、epc、badvaddr、cause。真实入口先 SAVE_ALL，再将帧地址作为 a0 调用 trap，最后 RESTORE_ALL 和 sret。启动页表只提供内核高地址映射。

[GUARANTEE]
不修改真实 trap.c、trap.h、SAVE_ALL、PMM、Makefile、评分脚本与原有测试。测试专用 C 文件可提供 kern_init 和 trap，但只能链接到隔离测试镜像。探针必须保存并恢复调用者的寄存器，不破坏 C ABI。产物放 obj/member-a-context，日志放 logs。测试失败、超时、缺少通过标记均返回非零退出码。

[SPECIFICATION]
Case 1：显式编码四字节 ebreak。为除 sp、x0 外全部通用寄存器设置不同的已知值；在 C 中检查帧地址、原始 sp、所有 GPR、SPP、SIE、cause 和 epc。令 epc 指向下一条指令，返回后再次检查所有寄存器，确认能正常返回 C 调用者。
Case 2：显式编码两字节 c.ebreak，执行同样检查。返回地址必须按实际两字节长度前进，不能笼统加四。
Case 3：C 修改帧内 t0、a0、s2、epc 和 SPIE；跳过一条故意改变 t0 的指令，返回后检查指定修改和其他寄存器，检查 sret 后 SIE 与 SPIE。sie 保持为零，避免引入中断源。
失败时输出场景和实际值；成功时逐项输出 PASS 和最终汇总后请求固件关机。记录真实执行结果，不把这些结果写成原有 Context 评分、整机评分、时钟中断或 U 态切换验证。
