/* lab2 Member C: 2413921 */
#include <slub.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>

static void *objects[320];
static const size_t requests[] = {
    1, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129,
    255, 256, 257, 511, 512, 513, 1023, 1024, 1025,
    2047, 2048, 2049, 4095, 4096, 4097, 8193
};

/* lab2 Challenge: tests exercise only the public API and observable memory. */
void slub_check(void) {
    slub_init();
    size_t baseline = nr_free_pages(), initial_owned = slub_owned_pages();
    assert(initial_owned == 0);
    size_t count = sizeof(requests) / sizeof(requests[0]);
    size_t i, j, k;
    assert(slub_alloc(0) == NULL);
    assert(slub_alloc((size_t)-1) == NULL);
    assert(slub_free(NULL) == 0);
    int outside;
    assert(slub_free(&outside) == -1);
    for (i = 0; i < count; i++) {
        objects[i] = slub_alloc(requests[i]);
        assert(objects[i] != NULL && ((uintptr_t)objects[i] & 15) == 0);
        size_t expected = 16;
        while (expected < requests[i] && expected < 2048) expected *= 2;
        if (requests[i] > 2048) expected = requests[i];
        assert(slub_object_size(objects[i]) == expected);
        for (j = 0; j < i; j++) {
            uintptr_t a = (uintptr_t)objects[i], b = (uintptr_t)objects[j];
            assert(a + requests[i] <= b || b + requests[j] <= a);
        }
        memset(objects[i], (int)(i + 1), requests[i]);
    }
    slub_init(); /* Reinitialization must preserve all live allocations. */
    for (i = 0; i < count; i++) {
        for (k = 0; k < requests[i]; k++)
            assert(((unsigned char *)objects[i])[k] == i + 1);
        assert(slub_free((char *)objects[i] + 1) == -1);
        assert(slub_free(objects[i]) == 0);
        assert(slub_free(objects[i]) == -1);
    }
    assert(nr_free_pages() == baseline && slub_owned_pages() == 0);
    cprintf("slub: size boundaries, alignment, non-overlap and contents passed\n");

    /* Keep one object alive so its page cannot be returned to PMM. */
    void *anchor = slub_alloc(32), *hole = slub_alloc(32);
    assert(anchor != NULL && hole != NULL);
    memset(anchor, 0x5a, 32);
    assert(slub_free(hole) == 0);
    void *reuse = slub_alloc(32);
    assert(reuse == hole);
    assert(slub_free(reuse) == 0);
    for (k = 0; k < 32; k++) assert(((unsigned char *)anchor)[k] == 0x5a);
    assert(slub_free(anchor) == 0);

    /* 320 x 16 bytes exceeds one page even without any metadata. */
    unsigned int round;
    for (round = 0; round < 4; round++) {
        for (i = 0; i < 320; i++) {
            objects[i] = slub_alloc(16);
            assert(objects[i] != NULL);
            for (j = 0; j < i; j++) assert(objects[i] != objects[j]);
            memset(objects[i], (int)(i & 255), 16);
        }
        assert(slub_owned_pages() >= 2);
        for (i = 0; i < 320; i++)
            for (k = 0; k < 16; k++)
                assert(((unsigned char *)objects[i])[k] == (i & 255));
        for (i = 0; i < 320; i += 2) assert(slub_free(objects[i]) == 0);
        for (i = 1; i < 320; i += 2) assert(slub_free(objects[i]) == 0);
        assert(nr_free_pages() == baseline && slub_owned_pages() == 0);
    }
    /* More pages than PMM owns: deterministic failure, no exhaustion loop. */
    assert(baseline < ((size_t)-1 / PGSIZE) - 2);
    assert(slub_alloc((baseline + 1) * PGSIZE) == NULL);
    assert(nr_free_pages() == baseline && slub_owned_pages() == 0);
    cprintf("slub: reuse, cross-page growth, reclamation and failure passed\n");
    cprintf("slub_check() succeeded! free pages restored: %lu\n", baseline);
}
