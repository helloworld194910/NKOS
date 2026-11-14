#include <buddy_pmm.h>
#include <list.h>
#include <stdio.h>

/* lab2 Buddy System 2412790 杨茜茹 */
/* Private BSS storage: no dependency on the allocator being initialized. */
#define BUDDY_CAPACITY ((PHYSICAL_MEMORY_END - DRAM_BASE) / PGSIZE)
#define BUDDY_ORDER_SLOTS (sizeof(size_t) * 8)

enum buddy_state {
    BUDDY_UNMANAGED = 0,
    BUDDY_FREE_HEAD,
    BUDDY_FREE_TAIL,
    BUDDY_ALLOC_HEAD,
    BUDDY_ALLOC_TAIL
};

struct buddy_record {
    size_t requested;
    unsigned int order;
    unsigned int state;
};

static struct buddy_record records[BUDDY_CAPACITY];
static unsigned char check_seen[BUDDY_CAPACITY];
static list_entry_t order_lists[BUDDY_ORDER_SLOTS];
static size_t free_count;
static unsigned int max_order;
static bool initialized;

static size_t
order_size(unsigned int order) {
    assert(order < BUDDY_ORDER_SLOTS);
    return (size_t)1 << order;
}

/* Validate integer addresses before doing any descriptor pointer arithmetic. */
static size_t
descriptor_index(const struct Page *page) {
    assert(initialized);
    assert(pages != NULL && npage > nbase);
    size_t count = npage - nbase;
    assert(count <= BUDDY_CAPACITY);
    uintptr_t address = (uintptr_t)page;
    uintptr_t start = (uintptr_t)pages;
    assert(page != NULL && address >= start);
    uintptr_t offset = address - start;
    assert(offset % sizeof(struct Page) == 0);
    size_t index = offset / sizeof(struct Page);
    assert(index < count);
    return index;
}

/* Return a bounded descriptor index for the absolute-PPN buddy, if present. */
static bool
partner_index(size_t index, unsigned int order, size_t *partner) {
    size_t ppn = nbase + index;
    size_t other = ppn ^ order_size(order);
    if (other < nbase || other >= npage) {
        return 0;
    }
    *partner = other - nbase;
    return 1;
}

/* The caller has removed any old list heads covering this entire block. */
static void
mark_block(size_t index, unsigned int order, bool allocated, size_t requested) {
    size_t size = order_size(order);
    assert(index < npage - nbase && size <= npage - nbase - index);
    for (size_t i = index; i < index + size; ++i) {
        records[i].state = allocated ? BUDDY_ALLOC_TAIL : BUDDY_FREE_TAIL;
        records[i].order = 0;
        records[i].requested = 0;
        ClearPageReserved(&pages[i]);
        ClearPageProperty(&pages[i]);
        pages[i].property = 0;
        set_page_ref(&pages[i], 0);
    }
    records[index].state = allocated ? BUDDY_ALLOC_HEAD : BUDDY_FREE_HEAD;
    records[index].order = order;
    records[index].requested = requested;
    if (!allocated) {
        pages[index].property = size;
        SetPageProperty(&pages[index]);
    }
}

/* Coalesce an unlisted free block; free_count is maintained by the caller. */
static void
return_block(size_t index, unsigned int order) {
    size_t partner;
    while (order < max_order && partner_index(index, order, &partner)) {
        if (records[partner].state != BUDDY_FREE_HEAD ||
            records[partner].order != order) {
            break;
        }
        list_del(&pages[partner].page_link);
        if (partner < index) {
            index = partner;
        }
        ++order;
    }
    mark_block(index, order, 0, 0);
    list_add(&order_lists[order], &pages[index].page_link);
}

static void
buddy_init(void) {
    free_count = 0;
    max_order = 0;
    while (max_order + 1 < BUDDY_ORDER_SLOTS &&
           order_size(max_order + 1) <= BUDDY_CAPACITY) {
        ++max_order;
    }
    for (unsigned int order = 0; order < BUDDY_ORDER_SLOTS; ++order) {
        list_init(&order_lists[order]);
    }
    for (size_t i = 0; i < BUDDY_CAPACITY; ++i) {
        records[i].state = BUDDY_UNMANAGED;
        records[i].order = 0;
        records[i].requested = 0;
        check_seen[i] = 0;
    }
    initialized = 1;
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    size_t index = descriptor_index(base);
    size_t count = npage - nbase;
    assert(n > 0 && n <= count - index);
    /* Validate the whole input before changing a single page or list. */
    for (size_t i = index; i < index + n; ++i) {
        assert(records[i].state == BUDDY_UNMANAGED);
        assert(PageReserved(&pages[i]));
    }
    size_t cursor = index, remaining = n;
    while (remaining != 0) {
        unsigned int order = 0;
        size_t ppn = nbase + cursor;
        while (order < max_order) {
            size_t larger = order_size(order + 1);
            if (larger > remaining || ppn % larger != 0) {
                break;
            }
            ++order;
        }
        size_t size = order_size(order);
        return_block(cursor, order);
        cursor += size;
        remaining -= size;
    }
    free_count += n;
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(initialized && n > 0);
    if (n > free_count || n > order_size(max_order)) {
        return NULL;
    }
    unsigned int wanted = 0;
    while (order_size(wanted) < n) {
        ++wanted;
    }
    unsigned int order = wanted;
    while (order <= max_order && list_empty(&order_lists[order])) {
        ++order;
    }
    if (order > max_order) {
        return NULL;
    }
    struct Page *page = le2page(list_next(&order_lists[order]), page_link);
    size_t index = descriptor_index(page);
    list_del(&page->page_link);
    /* The low half is kept for the request; upper halves become free blocks. */
    while (order > wanted) {
        --order;
        size_t upper = index + order_size(order);
        mark_block(upper, order, 0, 0);
        list_add(&order_lists[order], &pages[upper].page_link);
    }
    mark_block(index, wanted, 1, n);
    free_count -= order_size(wanted);
    return page;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    size_t index = descriptor_index(base);
    assert(n > 0);
    assert(records[index].state == BUDDY_ALLOC_HEAD);
    assert(records[index].requested == n);
    unsigned int order = records[index].order;
    assert(order <= max_order);
    size_t size = order_size(order);
    assert(size <= npage - nbase - index);
    /* Reject the entire invalid operation before touching allocator state. */
    for (size_t i = index; i < index + size; ++i) {
        assert(!PageReserved(&pages[i]) && !PageProperty(&pages[i]));
        if (i != index) {
            assert(records[i].state == BUDDY_ALLOC_TAIL);
        }
    }
    return_block(index, order);
    free_count += size;
}

static size_t
buddy_nr_free_pages(void) {
    assert(initialized);
    return free_count;
}

static void
buddy_check(void) {
    assert(initialized && pages != NULL && npage > nbase);
    size_t count = npage - nbase;
    assert(count <= BUDDY_CAPACITY);
    for (size_t i = 0; i < count; ++i) {
        check_seen[i] = 0;
    }
    size_t listed = 0, nodes = 0;
    for (unsigned int order = 0; order <= max_order; ++order) {
        list_entry_t *head = &order_lists[order];
        list_entry_t *le = list_next(head), *previous = head;
        while (le != head) {
            assert(++nodes <= count);
            /* Check address before dereferencing a possible descriptor node. */
            uintptr_t address = (uintptr_t)le;
            assert(address >= offsetof(struct Page, page_link));
            struct Page *page = (struct Page *)(address - offsetof(struct Page, page_link));
            size_t index = descriptor_index(page);
            assert(!check_seen[index]);
            check_seen[index] = 1;
            assert(list_prev(le) == previous);
            assert(records[index].state == BUDDY_FREE_HEAD);
            assert(records[index].order == order);
            assert(PageProperty(page) && page->property == order_size(order));
            listed += order_size(order);
            previous = le;
            le = list_next(le);
        }
        assert(list_prev(head) == previous);
    }
    size_t described = 0;
    for (size_t i = 0; i < count;) {
        unsigned int state = records[i].state;
        if (state == BUDDY_UNMANAGED) {
            /* Other managers may be present; only registered pages belong here. */
            ++i;
            continue;
        }
        assert(state == BUDDY_FREE_HEAD || state == BUDDY_ALLOC_HEAD);
        unsigned int order = records[i].order;
        assert(order <= max_order);
        size_t size = order_size(order);
        assert(size <= count - i && (nbase + i) % size == 0);
        bool free = state == BUDDY_FREE_HEAD;
        if (free) {
            assert(check_seen[i] && PageProperty(&pages[i]));
            assert(records[i].requested == 0 && pages[i].property == size);
            described += size;
            size_t partner;
            if (order < max_order && partner_index(i, order, &partner)) {
                assert(records[partner].state != BUDDY_FREE_HEAD ||
                       records[partner].order != order);
            }
        } else {
            assert(!check_seen[i] && !PageProperty(&pages[i]));
            size_t request = records[i].requested;
            assert(request > 0 && request <= size);
            assert(size == 1 || request > size / 2);
        }
        for (size_t j = i; j < i + size; ++j) {
            assert(!PageReserved(&pages[j]) && page_ref(&pages[j]) == 0);
            if (j != i) {
                assert(records[j].state == (free ? BUDDY_FREE_TAIL : BUDDY_ALLOC_TAIL));
                assert(!PageProperty(&pages[j]) && !check_seen[j]);
            }
        }
        i += size;
    }
    assert(listed == free_count && described == free_count);
    /* Scratch bytes are reset so the check leaves even private storage intact. */
    for (size_t i = 0; i < count; ++i) {
        check_seen[i] = 0;
    }
    cprintf("buddy_check() basic invariants succeeded!\n");
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
