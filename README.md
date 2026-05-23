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
│   ├── syscall.hpp     — ARM64 svc #0 wrappers (no libc)
│   ├── mem_utils.hpp   — memset/memcpy/strcmp/strlen without libc
│   ├── elf_utils.hpp   — /proc/self/maps parser + ELF symbol lookup
│   └── fmt.hpp         — printf implementation over sys_write
├── src/
│   ├── entry.S         — raw _start: reads argc/argv/envp from stack
│   ├── main.cpp        — pie_main() demo for the standalone executable
│   ├── payload.cpp     — payload_entry() callable from a raw blob
│   └── loader.c        — mmap+memcpy+mprotect+function-ptr loader
├── linker/
│   ├── pie.ld          — script for the standalone PIE (no PT_INTERP)
│   └── payload.ld      — script for the blob (no relocations at all)
├── CMakeLists.txt
└── build.sh            — NDK build convenience script
```

## Build outputs

| File | What it is |
|------|------------|
| `armkit` | Standalone PIE executable. Runs via `execve`; ships its own `_start`. |
| `payload.elf` | Same code, linked statically with zero runtime relocations. Not meant to run on its own. |
| `armkit_payload.bin` | Flat blob: `.text + .rodata + .data` of `payload.elf` concatenated by `objcopy -O binary`. Entry point is at offset 0. |
| `loader` | Normal Android executable that loads `armkit_payload.bin` into RWX memory and jumps to it. |

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

Standalone PIE:

```bash
adb push build-arm64/armkit /data/local/tmp/
adb shell chmod +x /data/local/tmp/armkit
adb shell /data/local/tmp/armkit
```

Loader + blob:

```bash
adb push build-arm64/loader              /data/local/tmp/
adb push build-arm64/armkit_payload.bin  /data/local/tmp/
adb shell chmod +x /data/local/tmp/loader
adb shell /data/local/tmp/loader /data/local/tmp/armkit_payload.bin
```

## The blob + loader pipeline

### Producing `armkit_payload.bin`

`linker/payload.ld` lays out the payload so that:

1. `payload_entry` (tagged with `__attribute__((section(".text.entry")))`)
   is `KEEP`-ed first inside `.text`, landing at offset `0x0`.
2. `.rodata` follows immediately, then `.data`.
3. Everything that would need runtime relocation processing
   (`.interp`, `.dynamic`, `.got`, `.plt`, `.rela.*`) is `/DISCARD/`-ed.
   If the linker fails because one of those isn't empty, the source did
   something that needs a relocation — fix the source.

The CMake build then runs:

```
objcopy -O binary \
        --only-section=.text \
        --only-section=.rodata \
        --only-section=.data \
        payload.elf armkit_payload.bin
```

The resulting `.bin` is a pure image: byte 0 is the first instruction of
`payload_entry`, byte N is wherever `.data` ended.

### Loading it from `loader.c`

```c
void* mem = mmap(NULL, pages,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
memcpy(mem, blob, blob_size);
mprotect(mem, pages, PROT_READ | PROT_EXEC);
__builtin___clear_cache(mem, (char*)mem + pages);

int (*entry)(int, char**) = (int(*)(int,char**))mem;
int rc = entry(argc, argv);
```

This is the whole runtime: get RW pages, copy bytes in, flip to RX, flush
the I-cache (mandatory on ARM after writing code), call offset 0.

### Why this works without relocations

The payload is compiled with `-fPIC -fvisibility=hidden`. On AArch64
that means every reference inside the blob is materialised by:

* `adrp` + `add` for `.rodata` / `.data` symbols (PC-relative within ±4 GiB)
* `b` / `bl` for local function calls (PC-relative ±128 MiB)

No `.got`, no absolute-address constants in `.data`, no vtables, no
exception unwind tables. The static linker resolves every offset at link
time; nothing remains for a dynamic linker to do, so the blob can be
relocated to any address by `mmap` and still execute correctly.

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
