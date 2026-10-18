/* Host backing store tests real slub.c/slub_check.c through public PMM APIs. */
#include <pmm.h>
#include <slub.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
extern int vprintf(const char *, va_list);
extern void exit(int) __attribute__((noreturn));

#define POOL_PAGES 256
static unsigned char memory[POOL_PAGES * PGSIZE] __attribute__((aligned(4096)));
static struct Page descriptors[POOL_PAGES];
static unsigned char busy[POOL_PAGES];
static size_t allocation[POOL_PAGES];
static int fail_next;
struct Page *pages = descriptors;
size_t npage = 0x80000 + POOL_PAGES;
const size_t nbase = 0x80000;
uint64_t va_pa_offset;

int cprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int result = vprintf(fmt, ap);
    va_end(ap);
    return result;
}
void __panic(const char *file, int line, const char *fmt, ...) {
    cprintf("PANIC %s:%d: ", file, line);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    cprintf("\n");
    exit(1);
}
size_t nr_free_pages(void) {
    size_t i, n = 0;
    for (i = 0; i < POOL_PAGES; i++) if (!busy[i]) n++;
    return n;
}
struct Page *alloc_pages(size_t n) {
    if (fail_next) { fail_next = 0; return NULL; }
    if (n == 0 || n > POOL_PAGES) return NULL;
    size_t i, j;
    for (i = 0; i <= POOL_PAGES - n; i++) {
        for (j = 0; j < n && !busy[i + j]; j++);
        if (j == n) {
            memset(busy + i, 1, n);
            allocation[i] = n;
            return &pages[i];
        }
    }
    return NULL;
}
void free_pages(struct Page *p, size_t n) {
    size_t i = p - pages;
    assert(i < POOL_PAGES && n <= POOL_PAGES - i && allocation[i] == n);
    size_t j;
    for (j = 0; j < n; j++) assert(busy[i + j]);
    memset(busy + i, 0, n);
    allocation[i] = 0;
    memset(memory + i * PGSIZE, 0xdd, n * PGSIZE);
}
int main(void) {
    va_pa_offset = (uintptr_t)memory - 0x80000000ULL;
    slub_check();
    fail_next = 1;
    assert(slub_alloc(16) == NULL);
    assert(slub_owned_pages() == 0 && nr_free_pages() == POOL_PAGES);
    void *anchor = slub_alloc(16);
    assert(anchor != NULL);
    memset(anchor, 0x7b, 16);
    fail_next = 1;
    assert(slub_alloc(8192) == NULL);
    assert(((unsigned char *)anchor)[0] == 0x7b);
    assert(slub_free(anchor) == 0);
    /* Real PMM fragmentation: 128 free pages but no two adjacent. */
    struct Page *held[POOL_PAGES];
    size_t i;
    for (i = 0; i < POOL_PAGES; i++) {
        held[i] = alloc_pages(1);
        assert(held[i] != NULL);
    }
    assert(slub_alloc(16) == NULL && slub_owned_pages() == 0);
    for (i = 0; i < POOL_PAGES; i += 2) free_pages(held[i], 1);
    assert(slub_alloc(4096) == NULL);
    assert(nr_free_pages() == POOL_PAGES / 2 && slub_owned_pages() == 0);
    for (i = 1; i < POOL_PAGES; i += 2) free_pages(held[i], 1);
    slub_check();
    cprintf("host: injected PMM failure and fragmentation passed\n");
    return 0;
}
