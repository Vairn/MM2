#pragma once

#include "mm2/Config.h"

#if MM2_NO_STL

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <initializer_list>
#if MM2_HOST_AMIGA
#include <mini_std/stdio.h>
#else
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
#include <string.h>
#ifdef __cplusplus
}
#endif

extern "C" void *mm2_malloc(size_t size);
extern "C" void mm2_free(void *ptr);
extern "C" void *mm2_realloc(void *ptr, size_t size);
extern "C" void *mm2_realloc(void *ptr, size_t size);

inline void *operator new(size_t, void *ptr) noexcept { return ptr; }
inline void operator delete(void *, void *) noexcept {}

namespace mm2_freestanding {

template <typename T>
class vector {
public:
    vector() = default;
    ~vector() { clear(); }

    vector(const vector &) = delete;
    vector &operator=(const vector &) = delete;

    vector(vector &&other) noexcept
        : data_(other.data_), size_(other.size_), cap_(other.cap_)
    {
        other.data_ = nullptr;
        other.size_ = 0;
        other.cap_ = 0;
    }

    vector &operator=(vector &&other) noexcept
    {
        if (this != &other) {
            clear();
            data_ = other.data_;
            size_ = other.size_;
            cap_ = other.cap_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.cap_ = 0;
        }
        return *this;
    }

    void clear()
    {
        for (size_t i = 0; i < size_; ++i) {
            data_[i].~T();
        }
        size_ = 0;
    }

    void push_back(const T &value)
    {
        if (size_ >= cap_) {
            grow();
        }
        new (&data_[size_]) T(value);
        ++size_;
    }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    T &operator[](size_t i) { return data_[i]; }
    const T &operator[](size_t i) const { return data_[i]; }

    T *begin() { return data_; }
    T *end() { return data_ + size_; }
    const T *begin() const { return data_; }
    const T *end() const { return data_ + size_; }

private:
    T *data_ = nullptr;
    size_t size_ = 0;
    size_t cap_ = 0;

    void grow()
    {
        size_t ncap = cap_ ? cap_ * 2 : 8;
        T *n = static_cast<T *>(mm2_malloc(ncap * sizeof(T)));
        for (size_t i = 0; i < size_; ++i) {
            new (&n[i]) T(data_[i]);
            data_[i].~T();
        }
        mm2_free(data_);
        data_ = n;
        cap_ = ncap;
    }
};

template <typename T>
class unique_ptr {
public:
    unique_ptr() = default;
    explicit unique_ptr(T *p) : ptr_(p) {}
    ~unique_ptr() { delete ptr_; }

    unique_ptr(const unique_ptr &) = delete;
    unique_ptr &operator=(const unique_ptr &) = delete;

    unique_ptr(unique_ptr &&other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    unique_ptr &operator=(unique_ptr &&other) noexcept
    {
        if (this != &other) {
            delete ptr_;
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    template <typename U>
    unique_ptr(unique_ptr<U> &&other) noexcept : ptr_(static_cast<T *>(other.release())) {}

    T *release()
    {
        T *p = ptr_;
        ptr_ = nullptr;
        return p;
    }

    T *get() const { return ptr_; }
    T &operator*() const { return *ptr_; }
    T *operator->() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }

    void reset(T *p = nullptr)
    {
        delete ptr_;
        ptr_ = p;
    }

private:
    T *ptr_ = nullptr;
};

template <typename T, typename... Args>
unique_ptr<T> make_unique(Args &&...args)
{
    return unique_ptr<T>(new T(static_cast<Args &&>(args)...));
}

template <typename T>
const T &min(const T &a, const T &b)
{
    return (b < a) ? b : a;
}

template <typename T>
const T &max(const T &a, const T &b)
{
    return (a < b) ? b : a;
}

inline int toupper_ascii(unsigned char c)
{
    if (c >= 'a' && c <= 'z') {
        return static_cast<int>(c - static_cast<unsigned char>('a') + static_cast<unsigned char>('A'));
    }
    return static_cast<int>(c);
}

inline int strncmp(const char *s1, const char *s2, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        const unsigned char a = static_cast<unsigned char>(s1[i]);
        const unsigned char b = static_cast<unsigned char>(s2[i]);
        if (a != b) {
            return static_cast<int>(a) - static_cast<int>(b);
        }
        if (a == '\0') {
            return 0;
        }
    }
    return 0;
}

inline char *strncat(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (*d != '\0') {
        ++d;
    }
    size_t i = 0;
    while (i < n && src[i] != '\0') {
        d[i] = src[i];
        ++i;
    }
    d[i] = '\0';
    return dest;
}

/* mini_std string.h has no strstr. */
inline const char *strstr(const char *haystack, const char *needle)
{
    if (!haystack || !needle) {
        return nullptr;
    }
    if (needle[0] == '\0') {
        return haystack;
    }
    for (const char *h = haystack; *h != '\0'; ++h) {
        const char *a = h;
        const char *b = needle;
        while (*a != '\0' && *b != '\0' && *a == *b) {
            ++a;
            ++b;
        }
        if (*b == '\0') {
            return h;
        }
    }
    return nullptr;
}

inline char *strstr(char *haystack, const char *needle)
{
    return const_cast<char *>(strstr(static_cast<const char *>(haystack), needle));
}

template <typename T, size_t N>
constexpr size_t size(const T (&)[N])
{
    return N;
}

template <typename T, size_t N>
struct array {
    T elems[N];

    T &operator[](size_t i) { return elems[i]; }
    const T &operator[](size_t i) const { return elems[i]; }

    T *data() { return elems; }
    const T *data() const { return elems; }
    T *begin() { return elems; }
    T *end() { return elems + N; }
    const T *begin() const { return elems; }
    const T *end() const { return elems + N; }

    T &at(size_t i) { return elems[i]; }
    const T &at(size_t i) const { return elems[i]; }

    constexpr size_t size() const { return N; }
    bool empty() const { return N == 0; }

    void fill(const T &value)
    {
        for (size_t i = 0; i < N; ++i) {
            elems[i] = value;
        }
    }
};

}  // namespace mm2_freestanding

namespace std {

using mm2_freestanding::array;
using mm2_freestanding::max;
using mm2_freestanding::min;
using mm2_freestanding::size;
using mm2_freestanding::strncat;
using mm2_freestanding::strncmp;
using mm2_freestanding::strstr;
using mm2_freestanding::unique_ptr;
using mm2_freestanding::vector;
using mm2_freestanding::make_unique;

using ::FILE;
using ::fclose;
using ::feof;
using ::fflush;
using ::fopen;
using ::fread;
using ::fseek;
using ::ftell;
using ::fwrite;
using ::memcpy;
using ::memmove;
using ::memcmp;
using ::memset;
using ::printf;
using ::size_t;
using ::snprintf;
using ::sprintf;
using ::strcat;
using ::strchr;
using ::strcmp;
using ::strcpy;
using ::strlen;
using ::strncpy;
using ::strrchr;
using ::strtoul;
using ::vsnprintf;

inline void *malloc(size_t n) { return mm2_malloc(n); }
inline void free(void *p) { mm2_free(p); }
inline void *realloc(void *p, size_t n) { return mm2_realloc(p, n); }

inline int toupper(int c) { return mm2_freestanding::toupper_ascii(static_cast<unsigned char>(c)); }
inline int tolower(int c)
{
    const unsigned char u = static_cast<unsigned char>(c);
    if (u >= 'A' && u <= 'Z') {
        return static_cast<int>(u - static_cast<unsigned char>('A') + static_cast<unsigned char>('a'));
    }
    return static_cast<int>(u);
}

}  // namespace std

#else  // !MM2_NO_STL

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <array>
#include <memory>
#include <vector>

#endif
