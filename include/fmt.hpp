#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdarg>
#include "syscall.hpp"
#include "mem_utils.hpp"

// Minimal printf-like formatter that writes directly via sys_write.
// No heap, no libc. Supports: %s %d %u %x %X %p %c %% %zu %ld %lu %lx

namespace fmt {

static constexpr int STDOUT = 1;
static constexpr int STDERR = 2;

// Write a single character
static inline void put_char(int fd, char c) {
    sys_write(fd, &c, 1);
}

// Write a NUL-terminated string
static inline void put_str(int fd, const char* s) {
    if (!s) s = "(null)";
    sys_write(fd, s, str_len(s));
}

// Convert an unsigned 64-bit value to a string in the given base.
// Writes into caller-supplied buf (must be >= 66 bytes).
// Returns pointer to the start of the string within buf.
static inline char* u64_to_str(uint64_t val, int base, bool upper, char* buf, size_t bufsz) {
    static const char digits_lo[] = "0123456789abcdef";
    static const char digits_hi[] = "0123456789ABCDEF";
    const char* digits = upper ? digits_hi : digits_lo;

    char* end = buf + bufsz - 1;
    *end = '\0';
    char* p = end;

    if (val == 0) {
        *--p = '0';
        return p;
    }
    while (val && p > buf) {
        *--p = digits[val % (uint64_t)base];
        val /= (uint64_t)base;
    }
    return p;
}

// Core vprintf implementation writing to fd.
static inline int vfdprintf(int fd, const char* fmt_str, va_list args) {
    int written = 0;
    char tmp[72];

    for (const char* p = fmt_str; *p; ++p) {
        if (*p != '%') {
            put_char(fd, *p);
            ++written;
            continue;
        }

        ++p;
        if (!*p) break;

        // Flags
        bool zero_pad = false;
        bool left_align = false;
        while (*p == '0' || *p == '-') {
            if (*p == '0') zero_pad = true;
            if (*p == '-') left_align = true;
            ++p;
        }

        // Width
        int width = 0;
        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            ++p;
        }

        // Length modifier
        bool is_long = false;
        bool is_size = false;
        if (*p == 'l') { is_long = true; ++p; }
        else if (*p == 'z') { is_size = true; ++p; }

        char spec = *p;

        auto print_padded = [&](const char* s, int len) {
            int pad = width > len ? width - len : 0;
            if (!left_align && !zero_pad)
                for (int i = 0; i < pad; ++i) { put_char(fd, ' '); ++written; }
            else if (!left_align && zero_pad)
                for (int i = 0; i < pad; ++i) { put_char(fd, '0'); ++written; }
            sys_write(fd, s, (size_t)len);
            written += len;
            if (left_align)
                for (int i = 0; i < pad; ++i) { put_char(fd, ' '); ++written; }
        };

        switch (spec) {
        case '%':
            put_char(fd, '%'); ++written;
            break;

        case 'c': {
            char c = (char)va_arg(args, int);
            put_char(fd, c); ++written;
            break;
        }

        case 's': {
            const char* s = va_arg(args, const char*);
            if (!s) s = "(null)";
            int len = (int)str_len(s);
            print_padded(s, len);
            break;
        }

        case 'd':
        case 'i': {
            int64_t val;
            if (is_long || is_size) val = va_arg(args, long);
            else                    val = (int64_t)va_arg(args, int);
            bool neg = val < 0;
            uint64_t uval = neg ? (uint64_t)(-val) : (uint64_t)val;
            char* s = u64_to_str(uval, 10, false, tmp, sizeof(tmp));
            // prepend sign into tmp if needed
            if (neg && s > tmp) *--s = '-';
            print_padded(s, (int)str_len(s));
            break;
        }

        case 'u': {
            uint64_t val;
            if (is_long || is_size) val = (uint64_t)va_arg(args, unsigned long);
            else                    val = (uint64_t)va_arg(args, unsigned int);
            char* s = u64_to_str(val, 10, false, tmp, sizeof(tmp));
            print_padded(s, (int)str_len(s));
            break;
        }

        case 'x':
        case 'X': {
            uint64_t val;
            if (is_long || is_size) val = (uint64_t)va_arg(args, unsigned long);
            else                    val = (uint64_t)va_arg(args, unsigned int);
            char* s = u64_to_str(val, 16, spec == 'X', tmp, sizeof(tmp));
            print_padded(s, (int)str_len(s));
            break;
        }

        case 'p': {
            uint64_t val = (uint64_t)va_arg(args, void*);
            // print as 0x...
            put_char(fd, '0'); put_char(fd, 'x');
            written += 2;
            char* s = u64_to_str(val, 16, false, tmp, sizeof(tmp));
            sys_write(fd, s, str_len(s));
            written += (int)str_len(s);
            break;
        }

        default:
            // Unknown specifier — echo literally
            put_char(fd, '%'); ++written;
            put_char(fd, spec); ++written;
            break;
        }
    }
    return written;
}

// Public API

__attribute__((format(printf, 1, 2)))
static inline int printf(const char* fmt_str, ...) {
    va_list args;
    va_start(args, fmt_str);
    int r = vfdprintf(STDOUT, fmt_str, args);
    va_end(args);
    return r;
}

__attribute__((format(printf, 1, 2)))
static inline int eprintf(const char* fmt_str, ...) {
    va_list args;
    va_start(args, fmt_str);
    int r = vfdprintf(STDERR, fmt_str, args);
    va_end(args);
    return r;
}

__attribute__((format(printf, 2, 3)))
static inline int fdprintf(int fd, const char* fmt_str, ...) {
    va_list args;
    va_start(args, fmt_str);
    int r = vfdprintf(fd, fmt_str, args);
    va_end(args);
    return r;
}

// Write a raw string + newline
static inline void puts(const char* s) {
    put_str(STDOUT, s);
    put_char(STDOUT, '\n');
}

} // namespace fmt
