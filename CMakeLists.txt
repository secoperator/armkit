cmake_minimum_required(VERSION 3.22)
project(armkit LANGUAGES CXX ASM)

# ---------------------------------------------------------------------------
# Toolchain must be aarch64-linux-android* (NDK) or aarch64-linux-gnu*.
# Pass -DCMAKE_TOOLCHAIN_FILE=<ndk>/build/cmake/android.toolchain.cmake
# and -DANDROID_ABI=arm64-v8a, or set CMAKE_C_COMPILER / CMAKE_CXX_COMPILER
# manually to your cross-compiler.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

# Flags that matter for a no-libc PIE
set(COMMON_FLAGS
    -fPIE
    -fno-exceptions
    -fno-rtti
    -fno-stack-protector
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fvisibility=hidden
    -ffreestanding
    # keep relative branches compact
    -mcmodel=small
    # no built-in printf/memcpy etc.
    -fno-builtin
    # strip dead code
    -ffunction-sections
    -fdata-sections
    # warnings
    -Wall -Wextra -Wpedantic
)

add_executable(armkit
    src/entry.S
    src/main.cpp
)

target_include_directories(armkit PRIVATE include)
target_compile_options(armkit PRIVATE ${COMMON_FLAGS})

# Linker flags: static-pie, no libc, custom linker script
target_link_options(armkit PRIVATE
    -static-pie
    -nostdlib
    -nostartfiles
    -nodefaultlibs
    -Wl,--gc-sections
    -Wl,-T,${CMAKE_SOURCE_DIR}/linker/pie.ld
    -Wl,-z,relro
    -Wl,-z,now
)

# Required because we use <cstdint> etc. from libstdc++ or libc++ headers
# but DO NOT link against them at runtime — only the headers are used.
target_compile_definitions(armkit PRIVATE
    _LIBCPP_HAS_NO_THREADS
    _LIBCPP_DISABLE_AVAILABILITY
    _LIBCPP_NO_EXCEPTIONS
)
