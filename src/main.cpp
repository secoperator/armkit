// PIE C++ demo — ARM64 Android
// No libc. No dynamic relocations at runtime.
// Linked as -static-pie so the static linker resolves all symbols;
// the resulting ELF runs via execve or direct kernel invocation.

#include "syscall.hpp"
#include "mem_utils.hpp"
#include "elf_utils.hpp"
#include "fmt.hpp"

// ---------------------------------------------------------------------------
// Demo: print process memory map
// ---------------------------------------------------------------------------
static void demo_maps() {
    fmt::printf("\n--- /proc/self/maps (first 10 entries) ---\n");

    constexpr int O_RDONLY = 0;
    long fd = sys_open("/proc/self/maps", O_RDONLY);
    if (fd < 0) {
        fmt::eprintf("  [!] could not open /proc/self/maps (err %ld)\n", fd);
        return;
    }

    static char buf[4096];
    static char line[512];
    int  line_pos = 0;
    int  count    = 0;

    long n;
    while (count < 10 && (n = sys_read((int)fd, buf, sizeof(buf))) > 0) {
        for (long i = 0; i < n && count < 10; ++i) {
            char c = buf[i];
            if (c == '\n' || line_pos == 511) {
                line[line_pos] = '\0';
                line_pos = 0;
                fmt::printf("  %s\n", line);
                ++count;
            } else {
                line[line_pos++] = c;
            }
        }
    }
    sys_close((int)fd);
}

// ---------------------------------------------------------------------------
// Demo: locate own base address and print ELF info
// ---------------------------------------------------------------------------
static void demo_self_elf() {
    fmt::printf("\n--- self ELF introspection ---\n");

    LibInfo info = find_library_base("armkit");  // matches the executable path
    if (!info.found) {
        // Fallback: try the generic "pie" name
        info = find_library_base("/proc/self/exe");
    }

    if (!info.found) {
        fmt::eprintf("  [!] could not locate own base via /proc/self/maps\n");
        return;
    }

    fmt::printf("  base address : %p\n", (void*)info.base);

    const auto* ehdr = reinterpret_cast<const Elf64_Ehdr*>(info.base);
    if (*reinterpret_cast<const uint32_t*>(ehdr->e_ident) != ELF_MAGIC) {
        fmt::eprintf("  [!] invalid ELF magic at base\n");
        return;
    }

    fmt::printf("  ELF type     : %u\n",  (unsigned)ehdr->e_type);
    fmt::printf("  machine      : 0x%x\n",(unsigned)ehdr->e_machine);
    fmt::printf("  entry point  : %p\n",  (void*)ehdr->e_entry);
    fmt::printf("  phnum        : %u\n",  (unsigned)ehdr->e_phnum);
    fmt::printf("  shnum        : %u\n",  (unsigned)ehdr->e_shnum);
}

// ---------------------------------------------------------------------------
// Demo: fmt::printf feature showcase
// ---------------------------------------------------------------------------
static void demo_printf() {
    fmt::printf("\n--- fmt::printf demo ---\n");
    fmt::printf("  string    : %s\n",        "hello, PIE world!");
    fmt::printf("  int       : %d\n",        -42);
    fmt::printf("  unsigned  : %u\n",        4294967295u);
    fmt::printf("  hex lower : 0x%x\n",      0xdeadbeef);
    fmt::printf("  hex upper : 0x%X\n",      0xCAFEBABE);
    fmt::printf("  pointer   : %p\n",        (void*)demo_printf);
    fmt::printf("  char      : %c\n",        'A');
    fmt::printf("  percent   : 100%%\n");
    fmt::printf("  padded    : [%8d]\n",     42);
    fmt::printf("  zero-pad  : [%08x]\n",    255);
    fmt::printf("  left-align: [%-10s]|\n",  "left");
    fmt::printf("  long      : %ld\n",       -9000000000L);
    fmt::printf("  size_t    : %zu\n",       (size_t)12345678);
}

// ---------------------------------------------------------------------------
// Demo: raw syscall wrappers
// ---------------------------------------------------------------------------
static void demo_syscalls() {
    fmt::printf("\n--- syscall wrappers demo ---\n");

    // getpid
    long pid = syscall0(syscall_nr::getpid);
    fmt::printf("  getpid()  = %ld\n", pid);

    // write to stdout directly
    const char msg[] = "  direct sys_write: works!\n";
    sys_write(1, msg, sizeof(msg) - 1);
}

// ---------------------------------------------------------------------------
// Entry point (called from entry.S)
// ---------------------------------------------------------------------------
extern "C"
__attribute__((visibility("default")))
void pie_main(int argc, char** argv, char** envp) {
    fmt::printf("=== armkit PIE demo (ARM64/Android) ===\n");
    fmt::printf("argc = %d\n", argc);
    for (int i = 0; i < argc; ++i)
        fmt::printf("argv[%d] = %s\n", i, argv[i]);

    demo_printf();
    demo_syscalls();
    demo_self_elf();
    demo_maps();

    fmt::printf("\ndone.\n");
    sys_exit(0);
}
