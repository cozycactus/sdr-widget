MEMORY
{
	FLASH (rx)  : ORIGIN = 0x00400000, LENGTH = 1024K
	RAM   (rwx) : ORIGIN = 0x20400000, LENGTH = 384K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
	. = ORIGIN(FLASH);

	.isr_vector :
	{
		KEEP(*(.isr_vector))
	} > FLASH

	.text :
	{
		*(.text*)
		*(.rodata*)
		KEEP(*(.init))
		KEEP(*(.fini))
		. = ALIGN(4);
	} > FLASH

	.ARM.exidx :
	{
		*(.ARM.exidx*)
		*(.gnu.linkonce.armexidx.*)
		. = ALIGN(4);
	} > FLASH

	_sidata = LOADADDR(.data);

	.data :
	{
		_sdata = .;
		*(.data*)
		. = ALIGN(4);
		_edata = .;
	} > RAM AT > FLASH

	.bss (NOLOAD) :
	{
		_sbss = .;
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
	} > RAM
}
