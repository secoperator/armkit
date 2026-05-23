/* Payload linker script — produces an ELF whose sections can be flattened
 * into a raw .bin via `objcopy -O binary`.
 *
 * Layout:
 *   offset 0x000 : .text  (payload_entry is the first symbol, see KEEP below)
 *                  rest of .text functions
 *   .............: .rodata  (string literals, format constants)
 *   .............: .data    (initialised globals — none expected)
 *
 * Everything that would normally require runtime relocation processing
 * (PT_INTERP, .dynamic, .got, .plt, .rela.*) is /DISCARD/ed. If the static
 * linker complains, it means the source code did something that needs a
 * relocation — fix the source, not the script.
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(payload_entry)

SECTIONS {
    . = 0;

    .text : ALIGN(16) {
        __payload_start = .;
        KEEP(*(.text.entry))     /* payload_entry FIRST */
        *(.text .text.*)
        *(.gnu.linkonce.t.*)
    }

    . = ALIGN(8);
    .rodata : {
        *(.rodata .rodata.*)
        *(.gnu.linkonce.r.*)
    }

    . = ALIGN(8);
    .data : {
        *(.data .data.*)
        *(.gnu.linkonce.d.*)
    }

    . = ALIGN(8);
    __payload_end = .;

    /DISCARD/ : {
        *(.interp)
        *(.dynamic) *(.dynsym) *(.dynstr) *(.hash) *(.gnu.hash)
        *(.got) *(.got.plt) *(.plt) *(.iplt) *(.igot.plt)
        *(.rela.dyn) *(.rela.plt) *(.rela.*)
        *(.eh_frame) *(.eh_frame_hdr)
        *(.comment) *(.note.*)
        *(.ARM.exidx*) *(.ARM.extab*)
        *(.bss) *(.bss.*)            /* payload must not use .bss */
        *(COMMON)
    }
}
