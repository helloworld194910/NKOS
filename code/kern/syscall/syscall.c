#include <unistd.h>
#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <pmm.h>
#include <error.h>
#include <vmm.h>

//lab5 function1 2412790: system-call handlers and register dispatch.
static int sys_exit(uint64_t arg[]) {
    return do_exit((int)arg[0]);
}

static int sys_fork(uint64_t arg[]) {
    (void)arg;
    struct trapframe *tf = current->tf;
    return do_fork(0, tf->gpr.sp, tf);
}

static int sys_wait(uint64_t arg[]) {
    int pid = (int)arg[0];
    if ((uint64_t)(int64_t)pid != arg[0]) {
        return -E_INVAL;
    }
    int *code_store = (int *)(uintptr_t)arg[1];
    if (code_store != NULL &&
        (current->mm == NULL ||
         !user_mem_check(current->mm, (uintptr_t)code_store, sizeof(*code_store), 1))) {
        return -E_INVAL;
    }
    return do_wait(pid, code_store);
}

static int sys_exec(uint64_t arg[]) {
    const char *name = (const char *)(uintptr_t)arg[0];
    size_t len = (size_t)arg[1];
    unsigned char *binary = (unsigned char *)(uintptr_t)arg[2];
    size_t size = (size_t)arg[3];
    struct mm_struct *mm = current->mm;
    if (mm == NULL || !user_mem_check(mm, (uintptr_t)name, len, 0) ||
        !user_mem_check(mm, (uintptr_t)binary, size, 0)) {
        return -E_INVAL;
    }
    /* do_execve must stage these inputs while the old address space is live. */
    return do_execve(name, len, binary, size);
}

static int sys_yield(uint64_t arg[]) {
    (void)arg;
    return do_yield();
}

static int sys_kill(uint64_t arg[]) {
    int pid = (int)arg[0];
    if ((uint64_t)(int64_t)pid != arg[0]) {
        return -E_INVAL;
    }
    return do_kill(pid);
}

static int sys_getpid(uint64_t arg[]) {
    (void)arg;
    return current->pid;
}

static int sys_putc(uint64_t arg[]) {
    cputchar((unsigned char)arg[0]);
    return 0;
}

static int sys_pgdir(uint64_t arg[]) {
    (void)arg;
    print_pgdir();
    return 0;
}

static int (*syscalls[])(uint64_t arg[]) = {
    [SYS_exit] = sys_exit,
    [SYS_fork] = sys_fork,
    [SYS_wait] = sys_wait,
    [SYS_exec] = sys_exec,
    [SYS_yield] = sys_yield,
    [SYS_kill] = sys_kill,
    [SYS_getpid] = sys_getpid,
    [SYS_putc] = sys_putc,
    [SYS_pgdir] = sys_pgdir,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

void syscall(void) {
    struct trapframe *tf = current->tf;
    uintptr_t num = tf->gpr.a0;
    int ret = -E_INVAL;
    if (num < NUM_SYSCALLS && syscalls[num] != NULL) {
        uint64_t arg[] = {tf->gpr.a1, tf->gpr.a2, tf->gpr.a3, tf->gpr.a4, tf->gpr.a5};
        ret = syscalls[num](arg);
    }
    tf->gpr.a0 = (uintptr_t)(intptr_t)ret;
}
