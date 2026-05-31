OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
    tls PT_TLS FLAGS(4);
}

SECTIONS
{
    . = 0x400000;

    .text : ALIGN(0x1000)
    {
        *(.text*)
    } :text

    .rodata : ALIGN(0x1000)
    {
        *(.rodata*)
    } :rodata

    .data : ALIGN(0x1000)
    {
        *(.data*)
    } :data

    .init_array : ALIGN(0x10)
    {
        PROVIDE(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE(__init_array_end = .);
    } :data

    .fini_array : ALIGN(0x10)
    {
        PROVIDE(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE(__fini_array_end = .);
    } :data

    .tdata : ALIGN(0x10)
    {
        *(.tdata*)
    } :data :tls

    .tbss : ALIGN(0x10)
    {
        *(.tbss*)
    } :data :tls

    .bss : ALIGN(0x1000)
    {
        *(COMMON)
        *(.bss*)
    } :data
}
