/* Linker script for a -static-pie ARM64 binary.
 *
 * Goals:
 *  1. No PT_INTERP — the kernel loads this directly, no ld.so involved.
 *  2. No copy relocations; everything resolves at static-link time.
 *  3. RELRO-friendly: read-only data is placed before writable data.
 *  4. Small, single-file output; no shared library dependencies.
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

SECTIONS {
    /* Load base: kernel picks the actual address for PIE. */
    . = 0;

    /* ------------------------------------------------------------------
     * Read-only: code + rodata
     * ------------------------------------------------------------------ */
    .text : {
        KEEP(*(.text._start))        /* entry point first */
        *(.text .text.*)
        *(.gnu.linkonce.t.*)
    }

    . = ALIGN(4096);
    .rodata : {
        *(.rodata .rodata.*)
        *(.gnu.linkonce.r.*)
    }

    /* ------------------------------------------------------------------
     * Relocation / dynamic info (filled by static linker for -static-pie)
     * ------------------------------------------------------------------ */
    . = ALIGN(8);
    .rela.dyn : { *(.rela.dyn) }

    . = ALIGN(8);
    .dynamic : { *(.dynamic) }

    . = ALIGN(8);
    .got : { *(.got) *(.got.plt) }

    /* ------------------------------------------------------------------
     * Writable: data + bss
     * ------------------------------------------------------------------ */
    . = ALIGN(4096);
    .data : {
        *(.data .data.*)
        *(.gnu.linkonce.d.*)
    }

    . = ALIGN(8);
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        __bss_end = .;
    }

    /* ------------------------------------------------------------------
     * Discard everything we don't need
     * ------------------------------------------------------------------ */
    /DISCARD/ : {
        *(.interp)          /* no dynamic linker */
        *(.comment)
        *(.note.*)
        *(.eh_frame_hdr)
        *(.eh_frame)
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}
