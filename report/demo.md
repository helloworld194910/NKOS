# Lab3 个人部分演示与截图步骤

负责人：2412790-杨茜茹。测试为独立验证，不能解释为原评分已通过。

## 基础模块演示

在 VS Code 的 WSL 终端执行这一整行：

```bash
python3 /home/yxqjrh/lab3/lab3_new/code/tests/member_a/run_context.py
```

等脚本自动退出，确认三个 PASS 和 RESULT: 3/3 context scenarios passed。真实截图保留命令、QEMU_PROFILE 和结果，尽量缩短无关滚屏；不要手工更改输出。文件名建议 member-a-context.png。

讲解：我负责在汇编入口将当前 Trap frame 地址作为 a0 传给 C，并恢复处理后的上下文。原程序 a0、ra 已在帧中保存；sp 最后恢复，避免余下槽使用错误基址。三个测试分别检查两种长度的断点和 C 修改有效帧后的行为。

## 可先准备的 GDB 观察（不要求为了截图全部执行）

原 Context 相关代码在 tools/test_lab3_context.S。正式内核可启动后，原评分依赖的异常跳转由正式 handler 处理；独立测试中使用明确的返回标签。两种结果分别记录。

若要先观察基础恢复，可先运行上面的基础脚本完成构建，再在一个 WSL 终端启动该镜像的调试模式：

```bash
qemu-system-riscv64 -machine virt -nographic -bios default -m 128M -smp 1 -device loader,file=/home/yxqjrh/lab3/lab3_new/code/obj/member-a-context/ucore.img,addr=0x80200000 -s -S
```

在另一个 WSL 终端执行：

```bash
riscv64-unknown-elf-gdb /home/yxqjrh/lab3/lab3_new/code/obj/member-a-context/kernel
```

GDB 内按顺序逐条输入：

```gdb
set remotetimeout unlimited
target remote :1234
break trap
continue
p sizeof(*tf)
p/x tf->gpr.sp
p/x tf->epc
p/x tf->cause
break __trapret
continue
p/x ((struct trapframe *)$sp)->epc
break member_a_resume_word
continue
info registers sp ra a0 s0 s1
```

这些是按当前源码准备的观察步骤，尚未执行，不记为额外调试通过。若已有其他 QEMU 占用 1234，不同时启动第二个调试实例。异常发生时断点进入 C 后可读 tf；__trapret 处 sp 仍为帧基址；返回标签处可以检查恢复后的寄存器。

## 图片与正式验收

上下文独立验证截图位于 `report/images/member-a-context.png`，从本目录报告引用时使用 `images/member-a-context.png`。旧版态切换 Challenge 1 已取消，其演示不属于本次要求。

正式 PMM、异常和中断实现齐备后，再在于蕾最终集成的代码上验证原 Context、两种长度断点与持续中断返回。程子涵组织完整评分和截图，于蕾最终整合与提交。
