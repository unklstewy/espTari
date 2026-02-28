ENTRY(component_entry)
SECTIONS {
    . = 0;
    .text : { *(.text.component_entry) *(.text .text.*) }
    . = ALIGN(4);
    .rodata : { *(.rodata .rodata.*) }
    . = ALIGN(4);
    .data : { *(.data .data.* .sdata .sdata.*) }
    . = ALIGN(4);
    .bss : { *(.bss .bss.* .sbss .sbss.* COMMON) }
    /DISCARD/ : { *(.comment) *(.note.*) *(.eh_frame*) *(.debug*) }
}
