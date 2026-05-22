# armkit — PIE C++ for ARM64 Android

A self-contained, position-independent executable for ARM64 Android that:

- **Has no libc dependency** — zero runtime shared-library loads.
- **Runs directly via `execve`** without a dynamic linker (`PT_INTERP` stripped).
- **All code is in relative offsets** — GOT/PLT entries are filled by the *static* linker, not `ld.so`.
- **Parses its own `/proc/self/maps`** to locate library base addresses.
- **Implements `printf`** via raw `write(2)` syscalls.

## Project layout

```
armkit/
├── include/
│   ├── syscall.hpp    — ARM64 svc #0 wrappers (no libc)
│   ├── mem_utils.hpp  — memset/memcpy/strcmp/strlen without libc
│   ├── elf_utils.hpp  — /proc/self/maps parser + ELF symbol lookup
│   └── fmt.hpp        — printf implementation over sys_write
├── src/
│   ├── entry.S        — raw _start: reads argc/argv/envp from stack
│   └── main.cpp       — pie_main() demo
├── linker/
│   └── pie.ld         — linker script (no PT_INTERP, no copy-relocs)
├── CMakeLists.txt
└── build.sh           — NDK build convenience script
```

## Building

### Prerequisites

- Android NDK r25+ (or any NDK with `aarch64-linux-android-clang++`)
- CMake ≥ 3.22
- Ninja

### Quick build

```bash
export ANDROID_NDK_HOME=/path/to/ndk
./build.sh
```

Or manually:

```bash
cmake -B build-arm64 \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DANDROID_STL=none \
  -DCMAKE_BUILD_TYPE=Release \
  -G Ninja
ninja -C build-arm64
```

### Running on a device

```bash
adb push build-arm64/armkit /data/local/tmp/armkit
adb shell chmod +x /data/local/tmp/armkit
adb shell /data/local/tmp/armkit
```

## Architecture

### `syscall.hpp` — raw svc wrappers

`syscall0` through `syscall6` emit a single `svc #0` instruction each.
All arguments are passed in `x0-x5`, the syscall number in `x8`.
No libc symbols are referenced anywhere.

```cpp
long pid = syscall0(syscall_nr::getpid);
sys_write(1, "hello\n", 6);
sys_exit(0);
```

### `elf_utils.hpp` — process memory introspection

**`find_library_base(name)`** opens `/proc/self/maps` via `sys_open` and
parses each line looking for a path whose suffix matches `name`.  Returns
the lowest mapped address for that file (the load base / slide).

**`find_symbol_in_memory(base, sym_name)`** walks the in-memory ELF
`PT_DYNAMIC` segment to locate `DT_SYMTAB` / `DT_STRTAB` and iterates
the symbol table, returning the symbol's absolute address.

### `fmt.hpp` — printf over sys_write

`fmt::printf`, `fmt::eprintf`, `fmt::fdprintf` support:

| specifier | meaning |
|-----------|---------|
| `%s`      | string |
| `%d` / `%i` | signed decimal (also `%ld`, `%zu`) |
| `%u`      | unsigned decimal |
| `%x` / `%X` | hex (lower/upper) |
| `%p`      | pointer (prefixed `0x`) |
| `%c`      | character |
| `%%`      | literal `%` |
| `%Nd`     | width, `%0Nd` zero-pad, `%-Ns` left-align |

### `entry.S` — raw `_start`

Reads `argc`, `argv`, `envp` directly from the kernel-provided stack
layout, aligns the stack, and jumps to `pie_main`.  If `pie_main`
returns, calls `exit_group(0)`.

### `linker/pie.ld`

Explicitly discards `.interp` so no `PT_INTERP` program header is
written.  This means the kernel maps the binary directly and jumps to
`_start` — there is no dynamic linker step.

## Extending

| Goal | What to add |
|------|-------------|
| More syscalls | Add a `static constexpr long` in `syscall_nr` + a wrapper in `syscall.hpp` |
| Find libc.so at runtime | Call `find_library_base("libc.so")` then `find_symbol_in_memory(base, "malloc")` |
| Heap allocator | Implement `sys_mmap` based slab in a new `alloc.hpp` |
| String formatting | Extend the `switch` in `fmt::vfdprintf` |
| New demo | Add a `static void demo_*()` in `main.cpp` and call it from `pie_main` |
