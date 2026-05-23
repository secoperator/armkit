/*
 * loader.c — load a raw position-independent blob and execute it.
 *
 * Steps:
 *   1. Read armkit_payload.bin into a heap buffer.
 *   2. mmap an anonymous PROT_READ|PROT_WRITE region big enough to hold it.
 *   3. memcpy the blob into the region.
 *   4. mprotect the region to PROT_READ|PROT_EXEC (W^X discipline).
 *   5. Flush the instruction cache (required on ARM after writing code).
 *   6. Cast the base of the region to a function pointer and call it.
 *
 * The payload's entry function lives at offset 0 of its own .text — the
 * linker script puts it there via KEEP(*(.text.entry)).
 *
 * Builds against bionic (Android NDK) or glibc (Linux). Pure libc, no NDK
 * extensions, no syscall wrappers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef int (*payload_fn)(int argc, char** argv);

static const char* DEFAULT_BIN = "armkit_payload.bin";

static long page_size(void) {
    long ps = sysconf(_SC_PAGESIZE);
    return (ps > 0) ? ps : 4096;
}

static void* slurp(const char* path, size_t* out_size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open(payload)"); return NULL; }

    struct stat st;
    if (fstat(fd, &st) < 0) { perror("fstat"); close(fd); return NULL; }

    size_t size = (size_t)st.st_size;
    void* buf = malloc(size);
    if (!buf) { perror("malloc"); close(fd); return NULL; }

    char* p = (char*)buf;
    size_t left = size;
    while (left) {
        ssize_t n = read(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("read"); free(buf); close(fd); return NULL;
        }
        if (n == 0) break;
        p    += n;
        left -= (size_t)n;
    }
    close(fd);
    *out_size = size;
    return buf;
}

int main(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : DEFAULT_BIN;

    fprintf(stderr, "[loader] reading payload : %s\n", path);

    size_t blob_size = 0;
    void*  blob      = slurp(path, &blob_size);
    if (!blob) return 1;

    fprintf(stderr, "[loader] blob size       : %zu bytes\n", blob_size);

    // Round to page size for mprotect
    long ps      = page_size();
    size_t pages = (blob_size + (size_t)ps - 1) & ~((size_t)ps - 1);

    // Step 1: get a writable region
    void* mem = mmap(NULL, pages,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS,
                     -1, 0);
    if (mem == MAP_FAILED) { perror("mmap"); free(blob); return 1; }

    fprintf(stderr, "[loader] mmap region     : %p (%zu bytes, RW)\n", mem, pages);

    // Step 2: copy the code/data in
    memcpy(mem, blob, blob_size);
    free(blob);

    // Step 3: flip to executable. We deliberately DROP write permission.
    if (mprotect(mem, pages, PROT_READ | PROT_EXEC) < 0) {
        perror("mprotect(RX)");
        munmap(mem, pages);
        return 1;
    }
    fprintf(stderr, "[loader] mprotect        : RX\n");

    // Step 4: flush I-cache so the CPU sees the freshly written instructions.
    __builtin___clear_cache((char*)mem, (char*)mem + pages);

    // Step 5: cast to function pointer and call.
    payload_fn entry = (payload_fn)mem;
    fprintf(stderr, "[loader] jumping into    : %p\n", (void*)entry);
    fflush(stderr);

    int rc = entry(argc, argv);

    fprintf(stderr, "[loader] payload returned %d\n", rc);

    munmap(mem, pages);
    return rc;
}
