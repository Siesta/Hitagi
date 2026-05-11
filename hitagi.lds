/*
 * About:
 *   Template for the linker script for RAMDLDs for Motorola P2K phones on the various Neptune SoCs.
 *
 * Notes:
 *   <ORIGIN>: Origin entry point address.
 *   <LENGTH>: Memory chunk length size.
 *
 * Author:
 *   EXL, Motorola Inc.
 *
 * License:
 *   MIT
 */

MEMORY {
	PATCH (RWX) : ORIGIN = %ORIGIN%, LENGTH = %LENGTH%
}

/* Generate target-specific ARM ELFs. */
OUTPUT_FORMAT("%OUTPUT_FORMAT%")

/* Entry point function. */
ENTRY(_start)

/* Combine all relevant section to one. */
SECTIONS {
	.text : {
		KEEP(*(.startup.entry))
		KEEP(*(.startup.cache))
		*(.startup .startup.*)
		*(.text .text.*)
		*(.rodata .rodata.*)
		*(.data .data.*)
		*(.bss .bss.*)
		*(COMMON COMMON.*)
	} > PATCH
}
