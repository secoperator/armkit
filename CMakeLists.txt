cmake_minimum_required(VERSION 3.22)
project(armkit LANGUAGES C CXX ASM)

# ---------------------------------------------------------------------------
# Toolchain must be aarch64-linux-android* (NDK) or aarch64-linux-gnu*.
# Pass -DCMAKE_TOOLCHAIN_FILE=<ndk>/build/cmake/android.toolchain.cmake
# and -DANDROID_ABI=arm64-v8a, or set CMAKE_C_COMPILER / CMAKE_CXX_COMPILER
# manually to your cross-compiler.
# ---------------------------------------------------------------------------

set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# Locate objcopy — needed to flatten the payload ELF into a .bin
if(NOT CMAKE_OBJCOPY)
    find_program(CMAKE_OBJCOPY
        NAMES
            ${ANDROID_TOOLCHAIN_PREFIX}objcopy
            llvm-objcopy
            aarch64-linux-android-objcopy
            aarch64-linux-gnu-objcopy
            objcopy
        HINTS
            ${ANDROID_TOOLCHAIN_ROOT}/bin
            ${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin
    )
endif()
if(NOT CMAKE_OBJCOPY)
    message(FATAL_ERROR "objcopy not found — needed to produce armkit_payload.bin")
endif()
message(STATUS "Using objcopy: ${CMAKE_OBJCOPY}")

# Flags shared by the no-libc PIE / payload code
set(NOLIBC_FLAGS
    -fPIE
    -fno-exceptions
    -fno-rtti
    -fno-stack-protector
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fvisibility=hidden
    -ffreestanding
    -mcmodel=small
    -fno-builtin
    -ffunction-sections
    -fdata-sections
    -Wall -Wextra -Wpedantic
)

# ---------------------------------------------------------------------------
# Target 1 : armkit — standalone PIE executable
#            Runs directly via execve; has its own _start.
# ---------------------------------------------------------------------------
add_executable(armkit
    src/entry.S
    src/main.cpp
)
target_include_directories(armkit PRIVATE include)
target_compile_options(armkit PRIVATE ${NOLIBC_FLAGS})
target_link_options(armkit PRIVATE
    -static-pie
    -nostdlib -nostartfiles -nodefaultlibs
    -Wl,--gc-sections
    -Wl,-T,${CMAKE_SOURCE_DIR}/linker/pie.ld
    -Wl,-z,relro -Wl,-z,now
)

# ---------------------------------------------------------------------------
# Target 2 : payload_elf — relocation-free blob, no entry stub.
#            Linked statically so the static linker resolves everything.
#            We then objcopy out .text + .rodata + .data into a flat .bin.
# ---------------------------------------------------------------------------
# Use -fPIC (not -fPIE) so the compiler never assumes the load address.
set(PAYLOAD_FLAGS
    -fPIC
    -fno-exceptions
    -fno-rtti
    -fno-stack-protector
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fvisibility=hidden
    -ffreestanding
    -mcmodel=small
    -fno-builtin
    -ffunction-sections
    -fdata-sections
    -Wall -Wextra -Wpedantic
)

add_executable(payload_elf src/payload.cpp)
target_include_directories(payload_elf PRIVATE include)
target_compile_options(payload_elf PRIVATE ${PAYLOAD_FLAGS})
target_link_options(payload_elf PRIVATE
    -static
    -nostdlib -nostartfiles -nodefaultlibs
    -Wl,--gc-sections
    -Wl,--no-dynamic-linker
    -Wl,-e,payload_entry
    -Wl,-T,${CMAKE_SOURCE_DIR}/linker/payload.ld
    -Wl,-z,noexecstack
    -Wl,--build-id=none
)
set_target_properties(payload_elf PROPERTIES OUTPUT_NAME "payload.elf")

# Flatten payload.elf -> armkit_payload.bin
set(PAYLOAD_BIN "${CMAKE_BINARY_DIR}/armkit_payload.bin")
add_custom_command(
    OUTPUT  ${PAYLOAD_BIN}
    COMMAND ${CMAKE_OBJCOPY} -O binary
            --only-section=.text
            --only-section=.rodata
            --only-section=.data
            $<TARGET_FILE:payload_elf>
            ${PAYLOAD_BIN}
    DEPENDS payload_elf
    COMMENT "Flattening payload.elf -> armkit_payload.bin"
    VERBATIM
)
add_custom_target(payload_bin ALL DEPENDS ${PAYLOAD_BIN})

# ---------------------------------------------------------------------------
# Target 3 : loader — standard Android executable (links against bionic).
#            mmap + memcpy + mprotect + function pointer.
# ---------------------------------------------------------------------------
add_executable(loader src/loader.c)
target_compile_options(loader PRIVATE -Wall -Wextra -O2)
