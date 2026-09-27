/* Layout of ROMDUMP.APP's code section — romdump.ld's, for a DLL with
 * no static data (see start_app.S).
 *
 * The globals still have to be laid out somewhere, so they are: in a
 * .bss that starts at address 0 in a segment of its own and is never
 * loaded. romdump.c is compiled with -frwpi, so what the code holds for
 * each global is its offset from the start of that segment, and
 * NewApplication adds r9 — a block on its stack — at run time.
 * __rw_size is how big that block has to be. The image's own
 * iBssSize is 0, because __bss_start and __bss_end, which is what
 * tools/e32/e32link.mts reads, are both the end of the code.
 */

ENTRY(_entry)

PHDRS
{
    code PT_LOAD;
    rw   PT_LOAD;
}

SECTIONS
{
    . = BASE;

    .text : {
        *(.text.entry)
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } :code

    .iat : {
        __iat_start = .;
        KEEP(*(.iat))
        __iat_end = .;
    } :code

    __code_end = .;
    __bss_start = .;
    __bss_end = .;

    .data : {
        __data_start = .;
        *(.data .data.* .got .got.plt)
        __data_end = .;
    } :code
    ASSERT(__data_end == __data_start, "romdump-er1: initialised data is not supported")

    .bss 0 (NOLOAD) : {
        __rw_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        __rw_end = .;
    } :rw
    __rw_size = __rw_end - __rw_start;
    ASSERT(__rw_start == 0, "romdump-er1: the r9 block must start at offset 0")

    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.attributes)
        *(.comment)
        *(.note*)
    }
}
