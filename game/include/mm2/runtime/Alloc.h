#pragma once
#include "mm2/CppStdCompat.h"
#include "mm2/Config.h"

namespace mm2::runtime {

#if MM2_HOST_AMIGA
void *allocate(std::size_t bytes);
/** Decode/file scratch buffers — keep CHIP for blitter bitmaps only. */
void *allocate_fast(std::size_t bytes);
void deallocate(void *ptr, std::size_t bytes) noexcept;
void *reallocate(void *ptr, std::size_t old_bytes, std::size_t new_bytes);

#else
inline void *allocate(std::size_t bytes) { return ::operator new(bytes); }
inline void deallocate(void *ptr, std::size_t) noexcept { ::operator delete(ptr); }
inline void *reallocate(void *ptr, std::size_t, std::size_t new_bytes)
{
    return ::operator new(new_bytes);
}

#endif

}  // namespace mm2::runtime

#if MM2_HOST_AMIGA
void *operator new(std::size_t size);
void *operator new[](std::size_t size);
void operator delete(void *ptr) noexcept;
void operator delete[](void *ptr) noexcept;
void operator delete(void *ptr, std::size_t size) noexcept;
void operator delete[](void *ptr, std::size_t size) noexcept;

#endif

#ifdef __cplusplus
extern "C" {
#endif

void *mm2_malloc(std::size_t size);
void *mm2_malloc_fast(std::size_t size);
void mm2_free(void *ptr);
void *mm2_realloc(void *ptr, std::size_t size);

#ifdef __cplusplus
}
#endif
