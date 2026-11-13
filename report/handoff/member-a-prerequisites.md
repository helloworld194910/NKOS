# 杨茜茹：Lab4 前置迁移说明

## 本次范围

迁移参考：NKOS lab3 提交 `6d8bc3cc39a3e5751aa3ce9dda7b6464773e5f07`。物理内存初始化、Best-Fit 与同步异常原实现来自于蕾；时钟处理中重装和计数的原实现来自程子涵；Trap 入口与返回原实现来自杨茜茹。此次 Lab4 适配由杨茜茹完成。

| 文件 | 本次改动 |
| --- | --- |
| code/kern/mm/default_pmm.c | 补齐 Best-Fit，使用框架声明的 default_pmm_manager；修正不存在的头文件引用。原两项自检保留。 |
| code/kern/mm/pmm.c | 补齐 page_init，按 Lab4 映射上界裁剪 RAM，初始化增加的 Page 字段。 |
| code/kern/trap/trapentry.S | 补齐恢复宏及传参/调用，保留 SAVE_ALL、sret 与 forkrets。 |
| code/kern/trap/trap.c | 补齐中断/异常处理；去掉旧实验的定时自动关机策略，其他中断使用现场诊断。 |

注意：运行时名称 default_pmm_manager 对应的是 Best-Fit 算法，不是依据文件名推定的 First-Fit。Lab4 的 Page 大小不同，空闲页数不能照抄 Lab3。

## 验证与边界

安装脚本先核对四份原文件，再修改，遇到不匹配立即停止。使用 --build 时强制编译四个对象，输出保存在 report/logs/member-a-prerequisites-build.log。本文不预填编译成功；以实际日志为准。

尚未完成 page_insert/page_remove，不能据此宣称完整内核可运行。Lab3 的上下文测试也不能原样当成 Lab4 的验证记录：当前 SAVE_ALL 没有初始化 x0 槽，sscratch 处理不同。

## 对接

- 进程创建负责人：保持 trapframe 布局和 forkrets 接口。你可按原接口继续 copy_thread 等任务；本次不需要你提供文件才能完成迁移。集成时提供进程创建实现及测试记录。
- 调度与集成负责人：公共 PMM 接口和中断保存包装不变；timer 持续重装，不在1000 ticks关机。后续提供调度实现及集成分支，联合验证线程执行。最终页操作交接等杨茜茹完成 page_insert/page_remove 后补充。
- 本次不要求队友实现已取消的用户态切换，也不移入 Buddy、SLUB 或 Lab3 的整套目录。

## 下一步

杨茜茹完成 page_insert、page_remove 和相关独立测试，补齐页表思考题及正式模板报告；随后与进程创建、调度代码联合构建、运行原评分并保留真实截图。
