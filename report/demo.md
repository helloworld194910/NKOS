# Lab3 个人部分演示与截图步骤

负责人：2412790-杨茜茹。测试为独立验证，不能解释为原评分已通过。

## 基础模块演示

在 VS Code 的 WSL 终端执行这一整行：

```bash
python3 /home/yxqjrh/lab3/lab3_new/code/tests/member_a/run_context.py
```

等脚本自动退出，确认三个 PASS 和 RESULT: 3/3 context scenarios passed。真实截图保留命令、QEMU_PROFILE 和结果，尽量缩短无关滚屏；不要手工更改输出。文件名建议 member-a-context.png。

讲解：我负责在汇编入口将当前 Trap frame 地址作为 a0 传给 C，并恢复处理后的上下文。原程序 a0、ra 已在帧中保存；sp 最后恢复，避免余下槽使用错误基址。三个测试分别检查两种长度的断点和 C 修改有效帧后的行为。

## 扩展演示

```bash
python3 /home/yxqjrh/lab3/lab3_new/code/tests/member_a_switch/run_switch.py
```

确认三行 PASS、六次 OBSERVED 及 RESULT: 3/3 switch cases passed。截图保留一轮真实 cause=8、SPP=0、用户 sp、kernel_frame 及汇总。文件名建议 member-a-switch.png。

讲解：用户栈和内核 Trap 栈分开。U 态 ecall 后，保存帧的 SPP=0、cause=8，C 回调和帧位于内核栈；第一次回调更新 a0、epc 后继续 U，用户检查结果再通过第二次 ecall 结束。结束时恢复原内核调用上下文，另有重复运行、拒绝重入和错误回调的检查。

说明边界：底层接口已由我定稿；当前用户程序、页表和 callback 是自测夹具，队友将按协议接入正式上层代码。已证明我的底层在这些场景可运行，完整内核评分与实际时钟路径仍待后续联调。

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

## 图片落点

两张真实截图最终放在工程 images/ 下，个人报告在 docs/ 下，因此链接分别应为 ../images/member-a-context.png 和 ../images/member-a-switch.png。没有图片前不写无效占位链接。可先将截图保存到 Windows 的本任务 outputs/lab3-member-a-screenshots 目录，再由后续脚本核对、导入。
