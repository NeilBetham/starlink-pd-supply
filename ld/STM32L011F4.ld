MEMORY {
  /* These are specific to the STM32L011F4 Being used with this project */
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 512K
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 144K
}

SECTIONS {
  /* Stack starts at the end of RAM and works up towards the heap */
  PROVIDE( _stack_ptr = ORIGIN(RAM) + LENGTH(RAM));

  __text_start__ = .;     /* beginning of .text segment also called code memory */

  /* Code and constants stored in FLASH */
  .text : {
    KEEP(*(.vector_table))  /* vector table */
    *(.text*)               /* other code */
    *(.rodata*)             /* constants go here */
  } > FLASH

  /* CPP CTor and DTor code sections */
  .preinit_array ALIGN(4) : {
    __preinit_array_start__ = .;
    KEEP(*(.preinit_array) )
    __preinit_array_end__ = .;
  } > FLASH
  .init_array ALIGN(4) : {
    __init_array_start__ = .;
    KEEP(*(.init_array))
    __init_array_end__ = .;
  } > FLASH
  .fini_array ALIGN(4) : {
    __fini_array_start__ = .;
    KEEP(*(.fini_array))
    __fini_array_end__ = .;
  } > FLASH

  __text_end__ = .;       /* end of .text segment */

  /* Data and initialized variables to be copied to RAM when */
  .data : {
    __data_start__ = .;  /* beginning of .data segment */
    *(vtable)            /* CPP vtable data */
    *(.data*)            /* data goes here */
    __data_end__ = .;    /* end of .data segment */
  } > RAM AT > FLASH     /* .data segment starts directly after the .text section in FLASH */
  __data_load__ = LOADADDR(.data); /* Don't rely on text end for load pos */

  /* Uninitialized data which is initialized to 0 */
  .bss : {
    __bss_start__ = .;  /* beginning of .bss segment */
    *(.bss*)            /* .bss content goes here */
    *(COMMON)           /* common symbols */
    __bss_end__ = .;    /* end of .bss segment */
    *(.noinit*)
  } > RAM

  /* Place heap at end of constants and static data */
  . = ALIGN(4);
  end = .;
}
