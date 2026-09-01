/* Layout of an EPOC32 E32Image's code section, as tools/e32/e32link.mts
 * needs it: entry point first (the loader enters at offset 0), then the
 * rest of the code and the constants, then the import address table —
 * the header's iTextSize is the IAT's offset, so nothing may come
 * between them. Uninitialised data follows the code in memory, which is
 * what iDataBase = iCodeBase + iCodeSize means in the header.
 *
 * BASE is supplied by the build: the image is linked twice at two
 * different bases so the packager can find the words that need
 * relocating (tools/romdump/build.sh).
 */

ENTRY(_entry)

SECTIONS
{
    . = BASE;

    .text : {
        *(.text.entry)
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    }

    .iat : {
        __iat_start = .;
        KEEP(*(.iat))
        __iat_end = .;
    }

    __code_end = .;

    /* There is no data section in the image (iDataSize is 0), so writable
     * initialised data would be dropped on the floor. Fail the link
     * instead: everything belongs in .rodata or on the stack. */
    .data : {
        __data_start = .;
        *(.data .data.* .got .got.plt)
        __data_end = .;
    }
    ASSERT(__data_end == __data_start, "romdump: initialised data is not supported")

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.attributes)
        *(.comment)
        *(.note*)
    }
}
