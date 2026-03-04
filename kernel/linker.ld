/* kernel/linker.ld */
/* Load kernel at 1MB (0x100000) */
ENTRY(kernel_main)

SECTIONS {
    . = 0x100000;

    .text : {
        /* Ensure kernel_main is placed first */
        *(.text.kernel_main)
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(COMMON)
        *(.bss*)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
