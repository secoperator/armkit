// ARM64 raw entry point for a PIE executable.
// The kernel places the process stack like this:
//   sp -> argc
//         argv[0..argc]
//         NULL
//         envp[0..]
//         NULL
//         auxv[0..]
//
// We set up a minimal C ABI frame and jump to pie_main(argc, argv, envp).
// No dynamic linker is assumed; this object is linked with -static-pie so
// the linker itself fills in the GOT and applies RELA relocations before
// _start runs, meaning all absolute addresses are already resolved.

.section .text._start, "ax", %progbits
.global _start
.type   _start, %function
_start:
    // x0 = argc, x1 = argv, x2 = envp
    ldr  x0, [sp]          // argc
    add  x1, sp, #8        // argv
    add  x2, x1, x0, lsl #3
    add  x2, x2, #8        // envp (skip NULL terminator)

    // Align stack to 16 bytes (already aligned after kernel setup,
    // but be explicit for ABI correctness).
    and  sp, sp, #~15

    bl   pie_main

    // pie_main should never return, but in case it does, call exit(0).
    mov  x0, #0
    mov  x8, #94           // exit_group
    svc  #0
    // Should not reach here
1:  b    1b
.size _start, . - _start
