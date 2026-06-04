#include "mm2/runtime/Alloc.h"

#if MM2_HOST_AMIGA

#include <ace/managers/memory.h>
#include <mini_std/stdlib.h>

/* ACE memFree requires the original allocation size. We store it in a ULONG
 * header immediately before the returned pointer so unsized delete/free can
 * recover it. ULONG (4 bytes) satisfies m68k's maximum alignment requirement. */
static const ULONG kHeaderSize = sizeof(ULONG);

static void *alloc_with_header(ULONG bytes)
{
    ULONG total = bytes + kHeaderSize;
    ULONG *block = static_cast<ULONG *>(memAllocFastClear(total));
    if (!block) {
        return nullptr;
    }
    block[0] = total;
    return block + 1;
}

static void free_with_header(void *ptr) noexcept
{
    if (!ptr) {
        return;
    }
    ULONG *block = static_cast<ULONG *>(ptr) - 1;
    memFree(block, block[0]);
}

namespace mm2::runtime {

void *allocate(std::size_t bytes)
{
    return alloc_with_header(static_cast<ULONG>(bytes));
}

void deallocate(void *ptr, std::size_t /*bytes*/) noexcept
{
    free_with_header(ptr);
}

void *reallocate(void *ptr, std::size_t old_bytes, std::size_t new_bytes)
{
    void *n = allocate(new_bytes);
    if (!n) {
        return nullptr;
    }
    if (ptr && old_bytes) {
        std::size_t copy = old_bytes < new_bytes ? old_bytes : new_bytes;
        memcpy(n, ptr, copy);
        deallocate(ptr, old_bytes);
    }
    return n;
}

}  // namespace mm2::runtime

void *operator new(std::size_t size) { return mm2::runtime::allocate(size); }

void *operator new[](std::size_t size) { return mm2::runtime::allocate(size); }

void operator delete(void *ptr) noexcept { free_with_header(ptr); }

void operator delete[](void *ptr) noexcept { free_with_header(ptr); }

void operator delete(void *ptr, std::size_t /*size*/) noexcept { free_with_header(ptr); }

void operator delete[](void *ptr, std::size_t /*size*/) noexcept { free_with_header(ptr); }

extern "C" void *mm2_malloc(std::size_t size) { return mm2::runtime::allocate(size); }

extern "C" void mm2_free(void *ptr) { free_with_header(ptr); }

extern "C" void *mm2_realloc(void *ptr, std::size_t size)
{
    if (!ptr) {
        return mm2::runtime::allocate(size);
    }
    ULONG *block = static_cast<ULONG *>(ptr) - 1;
    std::size_t old_bytes = block[0] - kHeaderSize;
    return mm2::runtime::reallocate(ptr, old_bytes, size);
}

#else

#include <cstdlib>

extern "C" void *mm2_malloc(std::size_t size) { return std::malloc(size); }

extern "C" void mm2_free(void *ptr) { std::free(ptr); }

extern "C" void *mm2_realloc(void *ptr, std::size_t size) { return std::realloc(ptr, size); }

#endif
