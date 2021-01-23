/*******************************************************************************
 * Copyright (c) 2008-2012,2015,2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/* uefi.lds -- Linker script for UEFI images
 *
 *    - Image starts at address 0x0
 *    - Image entry point is efi_main()
 *    - The first 4-kb are reserverd for the file headers
 *    - Sections are contiguous and 32-bit aligned
 *
 *   Once linked using this script, the obtained ELF binary still has to be
 *   converted to the UEFI PE/COFF format using the elf2efi program.
 */

ENTRY(efi_main)

SECTIONS {
   HEADERS_SIZE = 0x1000;
   SECTION_ALIGNMENT = 32;

   . = HEADERS_SIZE;

   . = ALIGN(SECTION_ALIGNMENT);
   .text : {
      PROVIDE(__executable_start = .);
      PROVIDE(_text = .);
      *(.text)
      *(.text.*)
      PROVIDE(_trampoline_start = .);
      *(.trampoline)
      PROVIDE(_trampoline_end = .);
      PROVIDE(_etext = .);
   }

   . = ALIGN(SECTION_ALIGNMENT);
   .rodata : {
      PROVIDE(_rodata = .);
      *(.rodata)
      *(.rodata.*)

      /*
       * Place the expected value of the integrity hash, if any, at the
       * end of the .rodata section, so that it's easy to skip while
       * computing the actual value of the hash.
       */
      PROVIDE(_integrity = .);
      *(.integrity)

      /*
       * The linker output had better not contain any .got* sections.
       * ld -q does not emit any relocations for GOT entries, so it
       * doesn't work to simply emit the GOT into the .rodata section,
       * as elf2efi won't generate fixups for the GOT entries and thus
       * they'll point to the wrong place after runtime relocation.
       * There also had better not be a PLT (though the dummy PLT that
       * is sometimes seen, consisting of 24 bytes of 00's, should be
       * harmless), for even stronger reasons.
       */
      PROVIDE(_erodata = .);
      *(.got)
      ASSERT(. == _erodata, "elf2efi cannot relocate GOT entries");
      *(.got.plt)
      ASSERT(. == _erodata, "elf2efi cannot handle a PLT");
   }

   . = ALIGN(SECTION_ALIGNMENT);
   .data : {
      PROVIDE(_data = .);
      *(.data)
      *(.data.*)
      PROVIDE(_edata = .);
   }

   . = ALIGN(SECTION_ALIGNMENT);
   .bss : {
      PROVIDE(_bss = .);
      *(.bss)
      *(.bss.*)
      *(COMMON)
      PROVIDE(_ebss = .);
   }
   PROVIDE(_end = .);

   /* Discard other sections */
    /DISCARD/ : {
       *(.comment)
       *(.comment.*)
       *(.note)
       *(.note.*)
       *(.eh_frame)
       *(.eh_frame.*)
       *(.rel)
       *(.rel.*)
       *(.discard)
    }
}
