#pragma once
#include <cstdint>
#include <cstddef>

// Minimal memory utilities — no libc.

__attribute__((always_inline))
static inline void* mem_set(void* dst, int c, size_t n) {
    auto* p = static_cast<unsigned char*>(dst);
    while (n--) *p++ = static_cast<unsigned char>(c);
    return dst;
}

__attribute__((always_inline))
static inline void* mem_copy(void* dst, const void* src, size_t n) {
    auto* d = static_cast<unsigned char*>(dst);
    const auto* s = static_cast<const unsigned char*>(src);
    while (n--) *d++ = *s++;
    return dst;
}

__attribute__((always_inline))
static inline int mem_cmp(const void* a, const void* b, size_t n) {
    const auto* p = static_cast<const unsigned char*>(a);
    const auto* q = static_cast<const unsigned char*>(b);
    for (size_t i = 0; i < n; ++i) {
        if (p[i] != q[i]) return (int)p[i] - (int)q[i];
    }
    return 0;
}

__attribute__((always_inline))
static inline size_t str_len(const char* s) {
    size_t n = 0;
    while (*s++) ++n;
    return n;
}

__attribute__((always_inline))
static inline int str_cmp(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return (unsigned char)*a - (unsigned char)*b;
}

__attribute__((always_inline))
static inline int str_ncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if ((unsigned char)a[i] != (unsigned char)b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i]) break;
    }
    return 0;
}

// Copy at most (n-1) chars then NUL-terminate.
static inline char* str_copy(char* dst, const char* src, size_t n) {
    size_t i = 0;
    while (i + 1 < n && src[i]) { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
    return dst;
}
