#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:

  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  +
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : get the process's pid

*/

// the process set's list
list_entry_t proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

// set_proc_name - set the name of proc
char *
set_proc_name(struct proc_struct *proc, const char *name)
{
    size_t len = strnlen(name, PROC_NAME_LEN);
    memmove(proc->name, name, len);
    memset(proc->name + len, 0, sizeof(proc->name) - len);
    return proc->name;
}

// get_proc_name - get the name of proc
char *
get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// get_pid - alloc a unique pid for process
static int
get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++last_pid >= MAX_PID)
    {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid)
            {
                if (++last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}


// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void
forkret(void)
{
    forkrets(current->tf);
}

// hash_proc - add proc into proc hash_list
static void
hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct *
find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list)
        {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid)
            {
                return proc;
            }
        }
    }
    return NULL;
}


// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int
setup_kstack(struct proc_struct *proc)
{
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
static void
put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}


// do_exit - called by sys_exit
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask parent reclaim itself.
//   3. call scheduler to switch to other process
int do_exit(int error_code)
{
    panic("process exit!!.\n");
}

// init_main - the second kernel thread used to create user_main kernel threads
static int
init_main(void *arg)
{
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
    cprintf("To U: \"%s\".\n", (const char *)arg);
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");
    return 0;
}

//LAB4 function2 2413921
// alloc_proc - 分配并清零一个进程控制块(PCB)，返回它的内核虚拟地址
//             此时 PCB 处于 PROC_UNINIT 状态，除了"身份字段"外不做进一步初始化
static struct proc_struct *
alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL)
    {
        proc->state = PROC_UNINIT; // 刚创建，尚未进入调度
        proc->pid = -1;            // 身份还未分配
        proc->runs = 0;            // 还没运行过
        proc->kstack = 0;          // 内核栈稍后由 setup_kstack 分配
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL; // 内核线程没有用户地址空间
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf = NULL;
        proc->pgdir = boot_pgdir_pa; // 内核线程共享内核页表(物理地址，给 satp 用)
        proc->flags = 0;
        memset(proc->name, 0, sizeof(proc->name));
        list_init(&proc->list_link);
        list_init(&proc->hash_link);
    }
    return proc;
}

// proc_init - 初始化进程管理全局结构，并把"当前执行现场"包装成 idleproc，
//             然后创建第一个真正的内核线程 initproc
void proc_init(void)
{
    int i;

    // 初始化进程链表与哈希表（按 pid 索引）
    list_init(&proc_list);
    for (i = 0; i < HASH_LIST_SIZE; i++)
    {
        list_init(hash_list + i);
    }

    // idleproc 不是 fork 出来的，而是直接复用 kern_init 至今的执行现场
    if ((idleproc = alloc_proc()) == NULL)
    {
        panic("cannot alloc idleproc.\n");
    }
    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack; // 直接使用启动阶段的内核栈
    idleproc->need_resched = 1;              // 尽快让出 CPU
    set_proc_name(idleproc, "idle");
    nr_process++;
    // 注意：idleproc 不挂入 proc_list，它只是调度器的"保底线程"
    current = idleproc;

    // 第一个真正的内核线程：init_main
    int pid = kernel_thread(init_main, "Hello world!!", 0);
    if (pid <= 0)
    {
        panic("create init_main failed.\n");
    }
    initproc = find_proc(pid);

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
    set_proc_name(initproc, "init");
}

// kernel_thread - 把一个普通函数包装成内核线程
//                 fn: 线程入口，arg: 传给线程的参数，clone_flags: fork 标志
//                 返回新线程的 pid（失败为负）
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    // 手工构造一个"模板中断帧"，因为创建第一个线程时没有父进程现场可以复制
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));

    // kernel_thread_entry 会执行：a0 = s1; jalr s0
    // 所以把入口函数放 s0、参数放 s1
    tf.gpr.s0 = (uintptr_t)fn;
    tf.gpr.s1 = (uintptr_t)arg;

    // 恢复现场时先保持 SIE=0，sret 再按 SPP 返回 S 态并从 SPIE 恢复 SIE。
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;

    // 新线程恢复现场后从这里开始执行
    tf.epc = (uintptr_t)kernel_thread_entry;

    // 内核线程共享内核地址空间，所以带上 CLONE_VM
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

// copy_mm - 复制/共享地址空间
//            本实验创建的都是内核线程，共享内核地址空间，current->mm 本来就是 NULL，
//            因此这里只做接口占位，真正的地址空间复制留到后续实验
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    assert(current->mm == NULL);
    proc->mm = NULL;
    return 0;
}

// copy_thread - 把中断帧和上下文安放到新线程自己的内核栈上，组织出首次运行的现场
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)
{
    // 内核栈顶向下留出一个 trapframe 的空间
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    *(proc->tf) = *tf;

    // a0 是返回值：0 表示"我是子线程"，这样 fork 的父子返回值不同
    proc->tf->gpr.a0 = 0;
    // 用户态栈指针；内核线程传进来的 esp 为 0，就用中断帧自身的位置
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;

    // 关键：上下文 ra 指向 forkret，sp 指向中断帧。
    // 新线程第一次被 switch_to 切换进来时，ret 会跳到 forkret，
    // forkret 调 forkrets(current->tf) 把 sp 换成中断帧，再由 __trapret 恢复所有寄存器
    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)(proc->tf);
}

// do_fork - 创建线程/进程的主体流程
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;

    if (nr_process >= MAX_PROCESS)
    {
        goto fork_out;
    }
    // 1. 分配 PCB
    if ((proc = alloc_proc()) == NULL)
    {
        ret = -E_NO_MEM;
        goto fork_out;
    }
    // 子线程的父线程就是当前线程
    proc->parent = current;

    // 2. 分配内核栈（KSTACKPAGE 个连续物理页）
    ret = setup_kstack(proc);
    if (ret != 0)
    {
        goto bad_fork_cleanup_proc;
    }

    // 3. 处理地址空间（本实验内核线程共享内核地址空间）
    ret = copy_mm(clone_flags, proc);
    if (ret != 0)
    {
        goto bad_fork_cleanup_kstack;
    }

    // 4. 设置中断帧与上下文
    copy_thread(proc, stack, tf);

    // 5. 在同一临界区内登记并唤醒已准备好的线程。
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (nr_process >= MAX_PROCESS)
        {
            ret = -E_NO_FREE_PROC;
        }
        else
        {
            proc->pid = get_pid();
            hash_proc(proc);
            list_add(&proc_list, &(proc->list_link));
            nr_process++;
            wakeup_proc(proc);
            ret = proc->pid;
        }
    }
    local_intr_restore(intr_flag);
    if (ret < 0)
    {
        goto bad_fork_cleanup_kstack;
    }

fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}

//LAB4 function3 2411824
// cpu_idle - idleproc 的执行主体：没有别的线程可以运行时在这里空转，
//            一旦发现自己需要被重新调度就让出 CPU
void cpu_idle(void)
{
    while (1)
    {
        if (current->need_resched)
        {
            schedule();
        }
    }
}

// proc_run - 让 proc 成为当前线程并真正切换过去
void proc_run(struct proc_struct *proc)
{
    if (proc != current)
    {
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;

        // 切换过程中 current、satp、执行现场必须保持一致，所以要关中断
        local_intr_save(intr_flag);
        {
            // 1. 更新当前线程
            current = proc;
            // 2. 切换页表（内核线程 pgdir 都是 boot_pgdir_pa，切换是幂等的，
            //    但保留这一步才能在后续实验支持用户进程）
            lsatp(next->pgdir);
            // 3. 保存 prev 的现场、恢复 next 的现场
            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag);
    }
}
