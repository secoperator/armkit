// armkit payload — position-independent code designed to be loaded as a
// raw .bin blob by an external loader.
//
// Contract with the loader:
//   * The first byte of .text is the entry function `payload_entry`.
//   * Calling convention: standard AAPCS64 — (int argc, char** argv).
//   * Return value is propagated by the loader.
//   * Payload must not depend on any relocation being applied. All
//     references inside the blob are PC-relative (adrp/add, b/bl).
//
// Anything that would normally need a relocation (global function pointers,
// arrays of pointers, vtables, exception tables) is forbidden here.

#include "syscall.hpp"
#include "fmt.hpp"

// Force this function to land in its own section so the linker script can
// place it at offset 0 of the final .text segment.
extern "C"
__attribute__((used, section(".text.entry"), visibility("default")))
int payload_entry(int argc, char** argv) {
    fmt::printf("\n[payload] ==== injected code is running ====\n");
    fmt::printf("[payload] argc = %d\n", argc);
    for (int i = 0; i < argc && i < 4; ++i)
        fmt::printf("[payload]   argv[%d] = %s\n", i, argv[i]);

    // Prove we are PIC: print our own address — must differ from build-time 0.
    uintptr_t self = reinterpret_cast<uintptr_t>(&payload_entry);
    fmt::printf("[payload] payload_entry address = %p\n", (void*)self);

    // Direct syscall — no libc involved.
    long pid = syscall0(syscall_nr::getpid);
    fmt::printf("[payload] getpid() = %ld\n", pid);

    // Format showcase to exercise .rodata strings (which must be reachable
    // via PC-relative addressing only).
    fmt::printf("[payload] %-10s %d %x %s %c%c\n",
                "values:", -7, 0xABCD, "PIC!", 'O', 'K');

    fmt::printf("[payload] returning control to loader\n\n");
    return 42;
}
