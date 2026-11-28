#include <defs.h>
#include <unistd.h>
#include <syscall.h>

//lab5 function1 2412790: RV64 a0 carries the number and result, a1-a5 carry arguments.
static inline int
syscall(uintptr_t num, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2,
        uintptr_t arg3, uintptr_t arg4) {
    register uintptr_t a0 asm("a0") = num;
    register uintptr_t a1 asm("a1") = arg0;
    register uintptr_t a2 asm("a2") = arg1;
    register uintptr_t a3 asm("a3") = arg2;
    register uintptr_t a4 asm("a4") = arg3;
    register uintptr_t a5 asm("a5") = arg4;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5) : "memory");
    return (int)a0;
}

int
sys_exit(int64_t error_code) {
    return syscall(SYS_exit, (uintptr_t)error_code, 0, 0, 0, 0);
}

int
sys_fork(void) {
    return syscall(SYS_fork, 0, 0, 0, 0, 0);
}

int
sys_wait(int64_t pid, int *store) {
    return syscall(SYS_wait, (uintptr_t)pid, (uintptr_t)store, 0, 0, 0);
}

int
sys_exec(const char *name, size_t len, const unsigned char *binary, size_t size) {
    return syscall(SYS_exec, (uintptr_t)name, len, (uintptr_t)binary, size, 0);
}

int
sys_yield(void) {
    return syscall(SYS_yield, 0, 0, 0, 0, 0);
}

int
sys_kill(int64_t pid) {
    return syscall(SYS_kill, (uintptr_t)pid, 0, 0, 0, 0);
}

int
sys_getpid(void) {
    return syscall(SYS_getpid, 0, 0, 0, 0, 0);
}

int
sys_putc(int64_t c) {
    return syscall(SYS_putc, (uintptr_t)c, 0, 0, 0, 0);
}

int
sys_pgdir(void) {
    return syscall(SYS_pgdir, 0, 0, 0, 0, 0);
}

