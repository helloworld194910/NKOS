#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}

//LAB4 function3 2411824
/* schedule - 选出一个就绪线程并切换过去（FIFO/轮转策略） */
void schedule(void)
{
    bool intr_flag;
    list_entry_t *le, *last;
    struct proc_struct *next = NULL;

    local_intr_save(intr_flag);
    {
        // 1. 我先不要求被调度了
        current->need_resched = 0;

        // 2. 确定查找起点：
        //    - 当前是 idleproc：从链表头开始，给所有线程同等机会
        //    - 当前是普通线程：从自己的下一个位置开始，实现轮转(RR)
        last = (current == idleproc) ? &proc_list : &(current->list_link);
        le = last;

        // 3. 沿链表找下一个 PROC_RUNNABLE 的线程
        do
        {
            if ((le = list_next(le)) != &proc_list)
            {
                next = le2proc(le, list_link);
                if (next->state == PROC_RUNNABLE)
                {
                    break;
                }
            }
        } while (le != last);

        // 4. 一个就绪线程都没有时，退回 idleproc 保底
        if (next == NULL || next->state != PROC_RUNNABLE)
        {
            next = idleproc;
        }

        // 5. 统计运行次数并真正切换
        next->runs++;
        if (next != current)
        {
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}


