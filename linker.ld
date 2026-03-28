bootboot    = 0xffffffffffe00000;
environment = 0xffffffffffe01000;
fb          = 0xfffffffffc000000;

ENTRY(_start)

PHDRS
{
  boot PT_LOAD;
}

SECTIONS
{
    . = 0xffffffffffe02000;

    .text : {
        KEEP(*(.text._start))
        *(.text .text.*)
    } :boot

    .rodata : {
        *(.rodata .rodata.*)
    } :boot

    .data : {
        *(.data .data.*)
    } :boot

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } :boot

    /DISCARD/ : { *(.interp .comment .note .eh_frame) }
}
