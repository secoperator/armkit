#pragma once
#include <cstdint>
#include <cstddef>

// ARM64 Linux syscall numbers
namespace syscall_nr {
    static constexpr long read        = 63;
    static constexpr long write       = 64;
    static constexpr long open        = 56;
    static constexpr long close       = 57;
    static constexpr long mmap        = 222;
    static constexpr long munmap      = 215;
    static constexpr long mprotect    = 226;
    static constexpr long exit        = 93;
    static constexpr long exit_group  = 94;
    static constexpr long getpid      = 172;
    static constexpr long openat      = 56;
    static constexpr long lseek       = 62;
    static constexpr long fstat       = 80;
    static constexpr long pread64     = 67;
}

// Raw syscall wrappers — no libc, no relocations, pure inline asm.
// Each overload maps to the ARM64 svc #0 calling convention:
//   x8 = syscall number
//   x0-x5 = arguments
//   x0 = return value (negative errno on error)

__attribute__((always_inline))
static inline long syscall0(long nr) {
    long ret;
    asm volatile(
        "mov x8, %1\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "r"(nr)
        : "x0", "x8", "memory"
    );
    return ret;
}

__attribute__((always_inline))
static inline long syscall1(long nr, long a1) {
    long ret;
    asm volatile(
        "mov x0, %2\n"
        "mov x8, %1\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "r"(nr), "r"(a1)
        : "x0", "x8", "memory"
    );
    return ret;
}

__attribute__((always_inline))
static inline long syscall2(long nr, long a1, long a2) {
    long ret;
    asm volatile(
        "mov x0, %2\n"
        "mov x1, %3\n"
        "mov x8, %1\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "r"(nr), "r"(a1), "r"(a2)
        : "x0", "x1", "x8", "memory"
    );
    return ret;
}

__attribute__((always_inline))
static inline long syscall3(long nr, long a1, long a2, long a3) {
    long ret;
    asm volatile(
        "mov x0, %2\n"
        "mov x1, %3\n"
        "mov x2, %4\n"
        "mov x8, %1\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "r"(nr), "r"(a1), "r"(a2), "r"(a3)
        : "x0", "x1", "x2", "x8", "memory"
    );
    return ret;
}

__attribute__((always_inline))
static inline long syscall4(long nr, long a1, long a2, long a3, long a4) {
    long ret;
    asm volatile(
        "mov x0, %2\n"
        "mov x1, %3\n"
        "mov x2, %4\n"
        "mov x3, %5\n"
        "mov x8, %1\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "r"(nr), "r"(a1), "r"(a2), "r"(a3), "r"(a4)
        : "x0", "x1", "x2", "x3", "x8", "memory"
    );
    return ret;
}

__attribute__((always_inline))
static inline long syscall6(long nr, long a1, long a2, long a3,
                             long a4, long a5, long a6) {
    long ret;
    asm volatile(
        "mov x0, %2\n"
        "mov x1, %3\n"
        "mov x2, %4\n"
        "mov x3, %5\n"
        "mov x4, %6\n"
        "mov x5, %7\n"
        "mov x8, %1\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "r"(nr), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6)
        : "x0", "x1", "x2", "x3", "x4", "x5", "x8", "memory"
    );
    return ret;
}

// Convenience wrappers
__attribute__((always_inline))
static inline long sys_write(int fd, const void* buf, size_t len) {
    return syscall3(syscall_nr::write, (long)fd, (long)buf, (long)len);
}

__attribute__((always_inline))
static inline long sys_read(int fd, void* buf, size_t len) {
    return syscall3(syscall_nr::read, (long)fd, (long)buf, (long)len);
}

__attribute__((always_inline))
static inline long sys_open(const char* path, int flags, int mode = 0) {
    return syscall3(syscall_nr::open, (long)path, (long)flags, (long)mode);
}

__attribute__((always_inline))
static inline long sys_close(int fd) {
    return syscall1(syscall_nr::close, (long)fd);
}

__attribute__((always_inline))
static inline long sys_lseek(int fd, long offset, int whence) {
    return syscall3(syscall_nr::lseek, (long)fd, offset, (long)whence);
}

__attribute__((always_inline))
static inline long sys_pread64(int fd, void* buf, size_t len, long offset) {
    return syscall4(syscall_nr::pread64, (long)fd, (long)buf, (long)len, offset);
}

__attribute__((always_inline))
static inline void sys_exit(int code) {
    syscall1(syscall_nr::exit_group, (long)code);
    __builtin_unreachable();
}

__attribute__((always_inline))
static inline long sys_mmap(void* addr, size_t len, int prot, int flags, int fd, long off) {
    return syscall6(syscall_nr::mmap,
                    (long)addr, (long)len, (long)prot, (long)flags, (long)fd, off);
}
