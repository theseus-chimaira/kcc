/* PDP-10 freestanding arena allocator, version 1.
 *
 * The host supplies a suitably aligned, writable memory region with
 * p10heap_init_v1(). No system calls, hidden static heap, or external
 * runtime functions are required. The heap is deliberately single-threaded.
 * All block lengths are expressed in aligned header units, not bytes.
 */
#include <stddef.h>

typedef union p10heap_block_v1 p10heap_block_v1;
union p10heap_block_v1 {
    struct {
        size_t units;
        int available;
    } h;
    long double align_long_double;
    void *align_pointer;
};

static p10heap_block_v1 *p10heap_start_v1;
static size_t p10heap_units_v1;

void
p10heap_init_v1(void *arena, size_t bytes)
{
    size_t units;
    p10heap_start_v1 = (p10heap_block_v1 *)0;
    p10heap_units_v1 = 0;
    if (arena == (void *)0)
        return;
    units = bytes / sizeof(p10heap_block_v1);
    if (units < 2)
        return;
    p10heap_start_v1 = (p10heap_block_v1 *)arena;
    p10heap_units_v1 = units;
    p10heap_start_v1[0].h.units = units;
    p10heap_start_v1[0].h.available = 1;
}

static void
p10heap_merge_v1(void)
{
    p10heap_block_v1 *block = p10heap_start_v1;
    p10heap_block_v1 *end;
    p10heap_block_v1 *next;
    if (block == (p10heap_block_v1 *)0)
        return;
    end = block + p10heap_units_v1;
    while (block < end) {
        next = block + block->h.units;
        if (next < end && block->h.available && next->h.available)
            block->h.units += next->h.units;
        else
            block = next;
    }
}

void *
malloc(size_t bytes)
{
    size_t units;
    p10heap_block_v1 *block;
    p10heap_block_v1 *end;
    p10heap_block_v1 *remainder;
    if (p10heap_start_v1 == (p10heap_block_v1 *)0)
        return (void *)0;
    if (bytes == 0)
        bytes = 1;
    if (bytes > (size_t)-1 - (sizeof(p10heap_block_v1) - 1))
        return (void *)0;
    units = (bytes + sizeof(p10heap_block_v1) - 1) /
            sizeof(p10heap_block_v1);
    if (units >= p10heap_units_v1)
        return (void *)0;
    ++units; /* The block header occupies one unit. */
    block = p10heap_start_v1;
    end = block + p10heap_units_v1;
    while (block < end) {
        if (block->h.available && block->h.units >= units) {
            if (block->h.units > units) {
                remainder = block + units;
                remainder->h.units = block->h.units - units;
                remainder->h.available = 1;
                block->h.units = units;
            }
            block->h.available = 0;
            return (void *)(block + 1);
        }
        block += block->h.units;
    }
    return (void *)0;
}

void
free(void *ptr)
{
    p10heap_block_v1 *block;
    if (ptr == (void *)0)
        return;
    block = (p10heap_block_v1 *)ptr - 1;
    block->h.available = 1;
    p10heap_merge_v1();
}

void *
calloc(size_t count, size_t bytes)
{
    unsigned char *p;
    size_t total;
    size_t i;
    if (bytes != 0 && count > (size_t)-1 / bytes)
        return (void *)0;
    total = count * bytes;
    p = (unsigned char *)malloc(total);
    if (p != (unsigned char *)0)
        for (i = 0; i < total; ++i)
            p[i] = 0;
    return (void *)p;
}

void *
realloc(void *ptr, size_t bytes)
{
    p10heap_block_v1 *block;
    p10heap_block_v1 *next;
    p10heap_block_v1 *end;
    p10heap_block_v1 *tail;
    unsigned char *old_data;
    unsigned char *new_data;
    size_t units;
    size_t old_bytes;
    size_t i;
    if (ptr == (void *)0)
        return malloc(bytes);
    if (bytes == 0) {
        free(ptr);
        return (void *)0;
    }
    if (bytes > (size_t)-1 - (sizeof(p10heap_block_v1) - 1))
        return (void *)0;
    units = 1 + (bytes + sizeof(p10heap_block_v1) - 1) /
            sizeof(p10heap_block_v1);
    block = (p10heap_block_v1 *)ptr - 1;
    old_bytes = (block->h.units - 1) * sizeof(p10heap_block_v1);
    if (block->h.units < units) {
        end = p10heap_start_v1 + p10heap_units_v1;
        next = block + block->h.units;
        if (next < end && next->h.available &&
            next->h.units >= units - block->h.units)
            block->h.units += next->h.units;
        else {
            new_data = (unsigned char *)malloc(bytes);
            if (new_data == (unsigned char *)0)
                return (void *)0;
            old_data = (unsigned char *)ptr;
            for (i = 0; i < old_bytes; ++i)
                new_data[i] = old_data[i];
            free(ptr);
            return (void *)new_data;
        }
    }
    if (block->h.units > units) {
        tail = block + units;
        tail->h.units = block->h.units - units;
        tail->h.available = 1;
        block->h.units = units;
        p10heap_merge_v1();
    }
    return ptr;
}
