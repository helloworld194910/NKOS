/* Public-interface tests. Link the real allocator as a separate translation unit. */
#include <buddy_pmm.h>

extern void host_note(const char *message);

#define TEST_CAPACITY ((PHYSICAL_MEMORY_END - DRAM_BASE) / PGSIZE)
#define CONTENT_PAGES 64

struct Page *pages;
size_t npage;
const size_t nbase = DRAM_BASE / PGSIZE;
uint64_t va_pa_offset;

static struct Page fixture[TEST_CAPACITY + 1];
static unsigned char managed[TEST_CAPACITY + 1];
static unsigned char occupied[TEST_CAPACITY + 1];
static unsigned char content[CONTENT_PAGES][PGSIZE];
static struct Page saved[CONTENT_PAGES];
static size_t expected_free;
static const struct pmm_manager *manager = &buddy_pmm_manager;

struct allocation {
    struct Page *base;
    size_t request;
    size_t size;
};

static bool equal(const char *a, const char *b) {
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

static void setup(size_t count) {
    assert(count <= TEST_CAPACITY + 1);
    pages = fixture;
    npage = nbase + count;
    expected_free = 0;
    for (size_t i = 0; i < count; ++i) {
        pages[i].flags = 0;
        SetPageReserved(&pages[i]);
        pages[i].ref = 0;
        pages[i].property = 0;
        list_init(&pages[i].page_link);
        managed[i] = occupied[i] = 0;
    }
    manager->init();
    assert(manager->nr_free_pages() == 0);
}

static void check_model(void) {
    assert(manager->nr_free_pages() == expected_free);
    manager->check();
    size_t actual_free = 0;
    for (size_t i = 0; i < npage - nbase; ++i) {
        if (!managed[i]) {
            assert(PageReserved(&pages[i]));
        } else {
            assert(!PageReserved(&pages[i]) && page_ref(&pages[i]) == 0);
            if (occupied[i]) {
                assert(!PageProperty(&pages[i]));
            } else {
                ++actual_free;
            }
        }
    }
    assert(actual_free == expected_free);
    assert(manager->nr_free_pages() == expected_free);
}

static void register_range(size_t first, size_t count) {
    manager->init_memmap(pages + first, count);
    for (size_t i = first; i < first + count; ++i) {
        assert(!managed[i]);
        managed[i] = 1;
    }
    expected_free += count;
    check_model();
}

static struct allocation take(size_t request) {
    struct allocation result = {manager->alloc_pages(request), request, 1};
    /* Independent expected size: multiply, no allocator helper or private data. */
    while (result.size < request) {
        result.size *= 2;
    }
    if (result.base != NULL) {
        uintptr_t address = (uintptr_t)result.base, start = (uintptr_t)pages;
        assert(address >= start && (address - start) % sizeof(struct Page) == 0);
        size_t first = (address - start) / sizeof(struct Page);
        assert(first < npage - nbase && result.size <= npage - nbase - first);
        assert((nbase + first) % result.size == 0);
        for (size_t i = first; i < first + result.size; ++i) {
            assert(managed[i] && !occupied[i]);
            occupied[i] = 1;
        }
        assert(expected_free >= result.size);
        expected_free -= result.size;
    }
    check_model();
    return result;
}

static void release(struct allocation *allocation) {
    assert(allocation->base != NULL);
    size_t first = allocation->base - pages;
    manager->free_pages(allocation->base, allocation->request);
    for (size_t i = first; i < first + allocation->size; ++i) {
        assert(occupied[i]);
        occupied[i] = 0;
    }
    expected_free += allocation->size;
    allocation->base = NULL;
    check_model();
}

static void save_descriptors(void) {
    assert(npage - nbase <= CONTENT_PAGES);
    for (size_t i = 0; i < npage - nbase; ++i) {
        saved[i] = pages[i];
    }
}

static void unchanged(void) {
    for (size_t i = 0; i < npage - nbase; ++i) {
        assert(saved[i].ref == pages[i].ref);
        assert(saved[i].flags == pages[i].flags);
        assert(saved[i].property == pages[i].property);
        assert(saved[i].page_link.next == pages[i].page_link.next);
        assert(saved[i].page_link.prev == pages[i].page_link.prev);
    }
    check_model();
}

static void case_empty(void) {
    setup(64);
    save_descriptors();
    assert(manager->alloc_pages(1) == NULL);
    unchanged();
}

static void case_orders(void) {
    /* All orders through the full current framework capacity, not only 64 pages. */
    setup(TEST_CAPACITY);
    assert((TEST_CAPACITY & (TEST_CAPACITY - 1)) == 0);
    assert(nbase % TEST_CAPACITY == 0);
    register_range(0, TEST_CAPACITY);
    for (size_t size = 1; size <= TEST_CAPACITY; size *= 2) {
        struct allocation allocation = take(size);
        assert(allocation.base != NULL && allocation.size == size);
        release(&allocation);
    }
}

static void case_rounding(void) {
    const size_t requests[] = {3, 5, 9};
    const size_t sizes[] = {4, 8, 16};
    setup(64);
    register_range(0, 64);
    for (size_t i = 0; i < 3; ++i) {
        struct allocation allocation = take(requests[i]);
        assert(allocation.base != NULL && allocation.size == sizes[i]);
        assert(manager->nr_free_pages() == 64 - sizes[i]);
        release(&allocation);
    }
}

static void case_split(void) {
    struct allocation allocations[32];
    setup(64);
    register_range(0, 32);
    for (size_t i = 0; i < 32; ++i) {
        allocations[i] = take(1);
        assert(allocations[i].base != NULL);
    }
    save_descriptors();
    assert(manager->alloc_pages(1) == NULL);
    unchanged();
    /* Odd multiplier gives a permutation, without assuming allocation order. */
    for (size_t i = 0; i < 32; ++i) {
        release(&allocations[(i * 13) % 32]);
    }
    struct allocation whole = take(32);
    assert(whole.base == pages);
    release(&whole);
}

static void case_merge(void) {
    struct allocation allocations[8], by_address[8];
    setup(64);
    register_range(0, 8);
    for (size_t i = 0; i < 8; ++i) {
        allocations[i] = take(1);
        assert(allocations[i].base != NULL);
        by_address[allocations[i].base - pages] = allocations[i];
    }
    release(&by_address[0]);
    release(&by_address[1]);
    struct allocation pair = take(2);
    assert(pair.base == pages);
    release(&pair);
    release(&by_address[2]);
    release(&by_address[3]);
    struct allocation four = take(4);
    assert(four.base == pages);
    release(&four);
    for (size_t i = 4; i < 8; ++i) {
        release(&by_address[i]);
    }
    struct allocation eight = take(8);
    assert(eight.base == pages);
    release(&eight);
}

static void case_nonpartners(void) {
    struct allocation by_address[4];
    setup(64);
    assert(nbase % 8 == 0);
    register_range(0, 8);
    for (size_t i = 0; i < 4; ++i) {
        struct allocation allocation = take(2);
        assert(allocation.base != NULL);
        by_address[(allocation.base - pages) / 2] = allocation;
    }
    release(&by_address[1]);
    release(&by_address[2]);
    assert(expected_free == 4);
    save_descriptors();
    assert(manager->alloc_pages(4) == NULL);
    unchanged();
    release(&by_address[0]);
    release(&by_address[3]);
    struct allocation whole = take(8);
    assert(whole.base == pages);
    release(&whole);
}

static void exhaust_range(size_t first, size_t count) {
    struct allocation allocations[64];
    setup(64);
    register_range(first, count);
    for (size_t i = 0; i < count; ++i) {
        allocations[i] = take(1);
        assert(allocations[i].base != NULL);
        assert(allocations[i].base >= pages + first);
        assert(allocations[i].base < pages + first + count);
    }
    assert(manager->alloc_pages(1) == NULL);
    check_model();
    for (size_t i = count; i > 0; --i) {
        release(&allocations[i - 1]);
    }
    assert(expected_free == count);
}

static void case_irregular(void) {
    exhaust_range(3, 13);
    exhaust_range(5, 14);  /* Both beginning and end are unaligned. */
}

static void case_registration(void) {
    setup(64);
    assert(nbase % 8 == 0);
    register_range(20, 4);
    register_range(16, 4);
    struct allocation whole = take(8);
    assert(whole.base == pages + 16);
    release(&whole);
    setup(64);
    register_range(32, 2);
    register_range(36, 2);
    save_descriptors();
    assert(manager->alloc_pages(4) == NULL);
    unchanged();
    struct allocation a = take(2), b = take(2);
    assert(a.base != NULL && b.base != NULL);
    release(&a);
    release(&b);
}

static void case_failure(void) {
    setup(64);
    register_range(0, 16);
    struct allocation live = take(3);
    assert(live.base != NULL);
    save_descriptors();
    assert(manager->alloc_pages(17) == NULL);
    unchanged();
    assert(manager->alloc_pages((size_t)-1) == NULL);
    unchanged();
    struct allocation other = take(8);
    assert(other.base != NULL);
    release(&other);
    release(&live);
}

static void verify_content(const struct allocation *slots) {
    for (size_t slot = 0; slot < 16; ++slot) {
        if (slots[slot].base == NULL) {
            continue;
        }
        size_t first = slots[slot].base - pages;
        for (size_t page = first; page < first + slots[slot].size; ++page) {
            for (size_t byte = 0; byte < PGSIZE; ++byte) {
                assert(content[page][byte] == (unsigned char)(slot + 1));
            }
        }
    }
}

static void case_interleave(void) {
    /* Fixed LCG seed, unsigned arithmetic: deterministic across 64-bit hosts. */
    uint32_t random = 0x2badd00dU;
    host_note("INTERLEAVE_SEED=0x2badd00d");
    for (size_t round = 0; round < 4; ++round) {
        struct allocation slots[16] = {{0}};
        setup(64);
        register_range(0, 64);
        for (size_t step = 0; step < 256; ++step) {
            random = random * 1664525U + 1013904223U;
            size_t slot = (random >> 16) % 16;
            if (slots[slot].base != NULL) {
                release(&slots[slot]);
            } else {
                size_t request = 1 + ((random >> 8) % 17);
                save_descriptors();
                slots[slot] = take(request);
                if (slots[slot].base == NULL) {
                    unchanged();
                } else {
                    size_t first = slots[slot].base - pages;
                    for (size_t page = first; page < first + slots[slot].size; ++page) {
                        for (size_t byte = 0; byte < PGSIZE; ++byte) {
                            content[page][byte] = (unsigned char)(slot + 1);
                        }
                    }
                }
            }
            verify_content(slots);
        }
        for (size_t slot = 0; slot < 16; ++slot) {
            if (slots[slot].base != NULL) {
                release(&slots[slot]);
            }
        }
        struct allocation whole = take(64);
        assert(whole.base == pages);
        release(&whole);
    }
}

/* Arm only immediately before the intended invalid operation. */
static void arm(void) { host_note("EXPECTED_OPERATION"); }

static void negative(const char *name) {
    setup(64);
    if (equal(name, "capacity")) {
        npage = nbase + TEST_CAPACITY + 1;
        arm(); manager->init_memmap(pages, 1);
    } else if (equal(name, "register_zero")) {
        arm(); manager->init_memmap(pages, 0);
    } else if (equal(name, "register_start")) {
        arm(); manager->init_memmap(pages + 64, 1);
    } else if (equal(name, "register_end")) {
        arm(); manager->init_memmap(pages + 60, 5);
    } else if (equal(name, "register_nonreserved")) {
        ClearPageReserved(pages + 5);
        arm(); manager->init_memmap(pages + 4, 4);
    } else if (equal(name, "register_twice") || equal(name, "register_overlap")) {
        register_range(8, 4);
        arm(); manager->init_memmap(pages + (equal(name, "register_twice") ? 8 : 10), 4);
    } else {
        register_range(0, 32);
        struct allocation allocation = take(3);
        assert(allocation.base != NULL);
        if (equal(name, "alloc_zero")) {
            arm(); (void)manager->alloc_pages(0);
        } else if (equal(name, "free_zero")) {
            arm(); manager->free_pages(allocation.base, 0);
        } else if (equal(name, "free_null")) {
            arm(); manager->free_pages(NULL, 3);
        } else if (equal(name, "free_outside")) {
            arm(); manager->free_pages(pages + 64, 3);
        } else if (equal(name, "free_unaligned")) {
            arm(); manager->free_pages((struct Page *)((uintptr_t)allocation.base + 1), 3);
        } else if (equal(name, "free_interior")) {
            arm(); manager->free_pages(allocation.base + 1, 3);
        } else if (equal(name, "free_length")) {
            arm(); manager->free_pages(allocation.base, 2);
        } else if (equal(name, "free_rounded")) {
            arm(); manager->free_pages(allocation.base, 4);
        } else if (equal(name, "free_twice")) {
            struct Page *old = allocation.base;
            release(&allocation);
            arm(); manager->free_pages(old, 3);
        } else if (equal(name, "free_reserved")) {
            arm(); manager->free_pages(pages + 48, 1);
        } else if (equal(name, "free_unregistered")) {
            ClearPageReserved(pages + 48);
            arm(); manager->free_pages(pages + 48, 1);
        } else {
            host_note("UNKNOWN_CASE");
            assert(0);
        }
    }
    host_note("INVALID_OPERATION_RETURNED");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        host_note("Usage: buddy-test CASE");
        return 2;
    }
    const char *name = argv[1];
    if (equal(name, "empty")) case_empty();
    else if (equal(name, "orders")) case_orders();
    else if (equal(name, "rounding")) case_rounding();
    else if (equal(name, "split")) case_split();
    else if (equal(name, "merge")) case_merge();
    else if (equal(name, "nonpartners")) case_nonpartners();
    else if (equal(name, "irregular")) case_irregular();
    else if (equal(name, "registration")) case_registration();
    else if (equal(name, "failure")) case_failure();
    else if (equal(name, "interleave")) case_interleave();
    else { negative(name); return 1; }
    host_note("NORMAL_CASE_PASS");
    return 0;
}
