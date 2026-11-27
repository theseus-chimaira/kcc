#ifndef PDP10_HEAP_ARENA_V1_H
#define PDP10_HEAP_ARENA_V1_H
#include <stddef.h>
/* Arena must be aligned for long double and pointer storage.
 * Reinitializing invalidates all existing allocations.
 * This allocator is not thread-safe. */
void p10heap_init_v1(void *arena, size_t bytes);
#endif
