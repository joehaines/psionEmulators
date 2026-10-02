/* Layout of an EPOC32 E32Image's code section, as tools/e32/e32link.mts
 * needs it: entry point first (the loader enters at offset 0), then the
 * code and constants, then the import address table (the header's
 * iTextSize is the IAT's offset, so nothing may come between them).
 *
 * Unlike tools/lemmings, Doom has writable initialised data. The image has
 * no data section, so the initial values travel at the end of the code
 * section (the "data image") and start.S copies them to where the loader
 * zeroes memory for the program: straight after the code, which is where
 * .data and then .bss are linked to run. DATASIZE is the size of .data,
 * measured by a first link (build.sh).
 *
 * BASE is supplied by the build: the image is linked twice at two
 * different bases so the packager can find the words that need
 * relocating (tools/romdump-er1/build.sh).
 */

ENTRY(_entry)

SECTIONS
{
    . = BASE;

    .text : {
        *(.text.entry)
        *(.text .text.*)
        . = ALIGN(4);
        __rodata_start = .;
        *(.rodata .rodata.*)
        . = ALIGN(4);
    }

    .iat : {
        __iat_start = .;
        KEEP(*(.iat))
        __iat_end = .;
    }

    /* keep the code section a multiple of 8 bytes even when .data is empty:
     * the loader puts .data and .bss right after it */
    /* ... and start the data (written all the time) on a page of its own, so no
     * page holds both code being run and data being written */
    .codepad : { . = ALIGN(8); BYTE(0); BYTE(0); BYTE(0); BYTE(0); . = ALIGN(4096); }
    __data_lma = .;
    __code_end = __data_lma + DATASIZE;

    .data __code_end : AT(__data_lma) {
        __data_start = .;
        *(.data .data.* .got .got.plt)
        . = ALIGN(8);
        __data_end = .;
    }
    ASSERT(__data_end - __data_start == DATASIZE, "doom: .data changed size between links")

    __bss_start = __data_start;

    .bss (NOLOAD) : {
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.attributes)
        *(.comment)
        *(.note*)
    }
}
