# 复现说明

从仓库根目录进入 `code/` 后执行以下命令。需要 GNU Make、原生 GCC、Python 3、RISC-V bare-metal GCC/binutils，以及 QEMU 4.1.1。工具版本与源码哈希见 [environment.json](evidence/member-c/environment.json)。

```sh
cd code

# 默认 Best-Fit 和原有评分
make clean
make
make qemu
make grade

# SLUB 宿主测试
make member-c-host

# Best-Fit + SLUB + MMU 探针
make clean
make SLUB_CHECK=1 MMU_PROBE=1
python3 tools/member_c/run_qemu.py

# Buddy + SLUB
make clean
make BUDDY=1 SLUB_CHECK=1
python3 tools/member_c/run_qemu.py --manager buddy_pmm_manager --output slub-qemu-buddy.log
```

切换 `BUDDY`、`SLUB_CHECK`、`MMU_PROBE` 或编译参数前先执行 `make clean`。`run_qemu.py` 等待 12 秒收集启动与测试输出，再终止它启动的 QEMU；日志尾部的 SIGTERM 是该观察窗口的正常结束方式。手动运行 `make qemu` 时，可按 Ctrl+A 后按 X 退出。

## 双 GDB 调试

需要带调试信息的 QEMU 4.1.1、宿主 `gdb` 和客体 `gdb-multiarch`。如需自行构建 QEMU，可运行 `sh tools/member_c/build_qemu.sh`。构建后执行：

```sh
QSRC="$HOME/oslab2-member-c/qemu-4.1.1"
QDBG="$QSRC/riscv64-softmmu/qemu-system-riscv64"
make clean
make SLUB_CHECK=1 MMU_PROBE=1
python3 tools/member_c/capture_debug.py --qemu "$QDBG" --source "$QSRC"
python3 tools/member_c/capture_debug.py --qemu "$QDBG" --source "$QSRC" --step
python3 tools/member_c/record_environment.py --qemu "$QDBG" --source "$QSRC"
```

调试端口为 `127.0.0.1:12349`，须保持空闲。脚本从当前内核符号表读取地址，日志默认写入 `code/evidence/member-c/`，中间命令文件写入 `code/build/member-c-debug/`。手工双 GDB 步骤和当次地址见[实验报告](report.md)。

## 已保存结果

- Best-Fit 原有评分：20/20，见 [grade.log](evidence/member-c/grade.log)。
- Buddy 宿主测试：28/28，见 [buddy-host.log](evidence/member-c/buddy-host.log)。
- Best-Fit + SLUB：恢复 31927 页，见 [运行日志](evidence/member-c/slub-qemu-final.log)。
- Buddy + SLUB：恢复 31792 页，见 [运行日志](evidence/member-c/slub-qemu-buddy.log)。
- QEMU 双 GDB trace 与单步记录见 `evidence/member-c/`。

构建目录 `code/obj`、`code/bin` 和 `code/build` 均可用 `make clean` 清理。仓库提交仅保留源码、报告、提示词、截图与验证记录。