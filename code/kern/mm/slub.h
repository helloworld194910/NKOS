#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__
#include <defs.h>

/* lab2 Challenge, member C: single-hart, non-interrupt allocation only. */
void slub_init(void);
void *slub_alloc(size_t size);
/* NULL succeeds; invalid/interior/double frees return -1. */
int slub_free(void *ptr);
size_t slub_object_size(const void *ptr);
size_t slub_owned_pages(void);
void slub_check(void);
#endif
