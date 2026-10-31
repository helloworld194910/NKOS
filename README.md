# Lab3：中断与异常

小组成员：杨茜茹（2412790）、程子涵（2413921）、于蕾（2411824）。实验分支：lab3_yang。

- [code/](code/)：完整内核、公共库、原构建和评分工具。
- [report/report.md](report/report.md)：小组实验报告、实际测试结果和截图。
- [report/prompt.md](report/prompt.md)：提示词与规格记录。
- [report/images/](report/images/)：报告使用的原始截图。

在 WSL/Linux 中进入 code/，执行 make、make grade；评分结束后重新 make、make qemu 可验证正常运行与自动关机。

当前基础内核评分 100/100，正常模式十次统计后在 ticks=1000 自动关机。已取消用户态切换 Challenge，相关材料不作为本次提交。报告内 A/C 的个人过程与最终提交信息由对应成员继续补齐。
