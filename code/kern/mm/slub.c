/* lab2 Member C: 2413921 */
#include <slub.h>
#include <pmm.h>
#include <string.h>
#include <stdio.h>

/* lab2 Challenge, member C. No private PMM state or Page fields are used. */
#define CACHE_COUNT 8
#define ALIGNMENT 16
#define BITMAP_WORDS 4
struct slab {
    struct slab *next;
    struct Page *page;
    size_t npages;
    size_t object_size;
    unsigned int capacity, used;
    uint64_t bitmap[BITMAP_WORDS];
};
static const size_t sizes[CACHE_COUNT] = {16, 32, 64, 128, 256, 512, 1024, 2048};
/* Last list contains individually allocated multi-page objects. */
static struct slab *heads[CACHE_COUNT + 1];
static size_t owned;
static int initialized;

static size_t header_size(void) {
    return ROUNDUP(sizeof(struct slab), ALIGNMENT);
}

void slub_init(void) {
    if (!initialized) {
        memset(heads, 0, sizeof(heads));
        owned = 0;
        initialized = 1;
    }
}

static int occupied(struct slab *s, size_t i) {
    return (s->bitmap[i / 64] >> (i % 64)) & 1;
}

static struct slab *new_slab(unsigned int cls, size_t bytes) {
    size_t hdr = header_size();
    if (bytes > (size_t)-1 - hdr - (PGSIZE - 1)) return NULL;
    size_t n = cls < CACHE_COUNT ? 1 : (bytes + hdr + PGSIZE - 1) / PGSIZE;
    if (n > nr_free_pages()) return NULL;
    struct Page *page = alloc_pages(n);
    if (page == NULL) return NULL;
    struct slab *s = (struct slab *)(page2pa(page) + va_pa_offset);
    memset(s, 0, sizeof(*s));
    s->page = page;
    s->npages = n;
    s->object_size = bytes;
    s->capacity = cls < CACHE_COUNT ? (PGSIZE - hdr) / bytes : 1;
    assert(s->capacity > 0 && s->capacity <= BITMAP_WORDS * 64);
    s->next = heads[cls];
    heads[cls] = s;
    owned += n;
    return s;
}

void *slub_alloc(size_t size) {
    if (size == 0) return NULL;
    slub_init();
    unsigned int cls = 0;
    while (cls < CACHE_COUNT && sizes[cls] < size) cls++;
    struct slab *s = heads[cls];
    if (cls == CACHE_COUNT) {
        s = new_slab(cls, size);
    } else {
        while (s != NULL && s->used == s->capacity) s = s->next;
        if (s == NULL) s = new_slab(cls, sizes[cls]);
    }
    if (s == NULL) return NULL;
    size_t i;
    for (i = 0; i < s->capacity; i++) {
        if (!occupied(s, i)) break;
    }
    assert(i < s->capacity);
    s->bitmap[i / 64] |= 1ULL << (i % 64);
    s->used++;
    return (char *)s + header_size() + i * s->object_size;
}

/* Search active slabs before dereferencing caller-controlled memory. */
static struct slab **find_object(const void *ptr, size_t *slot) {
    uintptr_t p = (uintptr_t)ptr;
    unsigned int cls;
    for (cls = 0; cls <= CACHE_COUNT; cls++) {
        struct slab **link = &heads[cls];
        while (*link != NULL) {
            struct slab *s = *link;
            uintptr_t start = (uintptr_t)s + header_size();
            if (p >= start && p - start < s->object_size * s->capacity) {
                size_t offset = p - start;
                if (offset % s->object_size != 0) return NULL;
                *slot = offset / s->object_size;
                return occupied(s, *slot) ? link : NULL;
            }
            link = &s->next;
        }
    }
    return NULL;
}

int slub_free(void *ptr) {
    if (ptr == NULL) return 0;
    size_t slot;
    struct slab **link = find_object(ptr, &slot);
    if (link == NULL) return -1;
    struct slab *s = *link;
    s->bitmap[slot / 64] &= ~(1ULL << (slot % 64));
    s->used--;
    if (s->used == 0) {
        struct Page *page = s->page;
        size_t n = s->npages;
        *link = s->next;
        owned -= n;
        free_pages(page, n); /* Do not access s after returning its pages. */
    }
    return 0;
}

size_t slub_object_size(const void *ptr) {
    size_t slot;
    struct slab **link = find_object(ptr, &slot);
    return link == NULL ? 0 : (*link)->object_size;
}

size_t slub_owned_pages(void) { return owned; }
