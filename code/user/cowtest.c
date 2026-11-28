#include <ulib.h>
#include <stdio.h>
#include <string.h>

/*
 * cowtest - Lab5 模块三 / COW 验收程序
 *
 * 检查三件事：
 *   1. fork 之后子进程能看到父进程在 fork 之前写入的数据（共享语义没丢）；
 *   2. 子进程写自己的地址空间后能读回新值（写缺页能把页私有化）；
 *   3. 子进程的写不会回流到父进程（写隔离），父进程的数据保持原样。
 *
 * 用法：make run-nox-cowtest（本仓库 QEMU 测试目标统一为 run-nox-<名字>，
 *       run-<名字> 走的是 spike）。
 */

#define TEST_PAGES 3
#define PATTERN(i) ((unsigned char)((i) & 0x7f))

static int shared_value;
static unsigned char shared_buf[TEST_PAGES * 4096];

static void
fill_pattern(void) {
    int i;
    for (i = 0; i < (int)sizeof(shared_buf); i++) {
        shared_buf[i] = PATTERN(i);
    }
}

static int
check_pattern(void) {
    int i;
    for (i = 0; i < (int)sizeof(shared_buf); i++) {
        if (shared_buf[i] != PATTERN(i)) {
            return i;
        }
    }
    return -1;
}

int
main(void) {
    int pid, code, bad;

    /* fork 之前把要共享的页都建立起来 */
    shared_value = 1111;
    fill_pattern();
    if (check_pattern() >= 0) {
        panic("cowtest: memory is broken before fork");
    }

    if ((pid = fork()) == 0) {
        /* 子进程：先确认继承了父进程的数据 */
        if (shared_value != 1111) {
            panic("child sees shared_value=%d, expect 1111", shared_value);
        }
        if (check_pattern() >= 0) {
            panic("child sees a corrupted inherited buffer");
        }

        /* 再逐个写自己看到的页面：每一次写都会触发一次 COW 复制 */
        shared_value = 2222;
        memset(shared_buf, 0x5a, sizeof(shared_buf));

        if (shared_value != 2222) {
            panic("child cannot write its own page");
        }
        for (bad = 0; bad < (int)sizeof(shared_buf); bad++) {
            if (shared_buf[bad] != 0x5a) {
                panic("child write lost at offset %d", bad);
            }
        }
        cprintf("cowtest: child value=%d, first byte=0x%02x\n",
                shared_value, (unsigned)shared_buf[0]);
        exit(0);
    }
    if (pid < 0) {
        panic("cowtest: fork failed with %d", pid);
    }

    if (waitpid(pid, &code) != 0 || code != 0) {
        panic("cowtest: child failed, code=%d", code);
    }

    /* 父进程：子进程的写必须完全不可见 */
    if (shared_value != 1111) {
        panic("parent value changed to %d by the child", shared_value);
    }
    if ((bad = check_pattern()) >= 0) {
        panic("parent buffer was modified by the child at offset %d", bad);
    }

    cprintf("cowtest: parent value=%d, first byte=0x%02x\n",
            shared_value, (unsigned)shared_buf[0]);
    cprintf("cowtest pass.\n");
    return 0;
}
