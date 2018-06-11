OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH("i386")
ENTRY(journey)
SECTIONS
{
	. = 0x100000;
	_text = .;
	.text : { *(.text) }
	_etext = .;
	.rodata : { *(.rodata .rodata.*) }
	. = ALIGN(4096);
	_data = .;
	.data : { *(.data) }
	_edata = .;
	. = ALIGN(4096);
	_bss = .;
	.bss : { *(.bss) }
	_ebss = .;
	_end = .;
}
