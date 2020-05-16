;;******************************************************************************
;; Copyright (c) 2008-2012,2015-2016,2020 VMware, Inc.  All rights reserved.
;; SPDX-License-Identifier: GPL-2.0
;;******************************************************************************

;; trampoline.s
;;
;;      All the code in this module will be relocated in order to execute from
;;      safe memory. That is to ensure that it will never get overwritten when
;;      processing the relocations.
;;
;;      - THIS CODE MUST LIVE IN CONTIGUOUS MEMORY
;;      - THIS CODE MUST BE POSITION-INDEPENDENT
;;      - THIS CODE MUST BE RELOCATED BELOW 4-GB
;;      - THIS CODE ASSUMES THAT HARDWARE INTERRUPTS ARE DISABLED

DEFAULT REL

%include "x86/trampoline.inc"

SECTION .trampoline

;;-- trampoline ----------------------------------------------------------------
;;
;;      This is the function that actually processes the relocations, sets up an
;;      ESXBootInfo or Multiboot compliant environment and jumps to the kernel.
;;
;; STACK USAGE
;;      The trampoline allocates dynamic memory from the stack. This memory is
;;      used to hold the platform-specific data. On x86, platform-specific data
;;      include:
;;
;;      +------------------------+
;;      |          DATA          |             1. A new GDT
;;      +------------------------+
;;      |          CODE          |
;;      +------------------------+
;;      |          NULL          |
;;      +------------------------+  <--.
;;                                     |
;;      +------------------------+     |
;;      |    GDT Base Address  --|-----'       2. A GDTR that points to the
;;      +-----------+------------+                first entry of the new GDT
;;      | GDT_LIMIT |
;;      +-----------+              ESP + 6
;;
;;                   +-----------+
;;                   |  BOOT_CS  |             3. A far pointer which is used to
;;      +------------+-----------+                reload CS after switching GDT.
;;      |    Far Jump Address    |
;;      +------------------------+ ESP
;;
;; Prototype
;;      void relocate(handoff_t *handoff);
;;
;; Side effects:
;;      - Setup a new GDT
;;      - Disable paging
;;      - Disable Long Mode (switch to compatibility mode)
;;------------------------------------------------------------------------------
GLOBAL trampoline
trampoline:

%if IS_64_BIT
[BITS 64]
      mov     r12, rdi                    ; R12 = handoff (R12 is callee-safe)
      mov     rsp, [r12 + OFFSET_STACK]   ; RSP = handoff->stack (new stack)
      add     rsp, TRAMPOLINE_STACK_SIZE  ; x86 stack grows downward

      sub     rsp, 4 * SIZEOF_PTR         ; Home allocation

      mov     rdi, [r12 + OFFSET_RELOCS]  ; RDI = handoff->reloc_table
      call    [r12 + OFFSET_RELOCATE]     ; handoff->do_reloc(RDI)

      mov     rbx, [r12 + OFFSET_EBI]     ; RBX = handoff->ebi
      mov     rbp, [r12 + OFFSET_KERNEL]  ; RBP = handoff->kernel
      mov     rdx, [r12 + OFFSET_MAGIC]   ; RDX = handoff->ebi_magic

      and     rsp, ~(GDT_DESC_SIZE - 1)   ; Align the GDT on 8 bytes
      pushq   GDT_DESC_4GB_FLAT_DATA32    ; GDT[2] = Data Segment Descriptor
      pushq   GDT_DESC_4GB_FLAT_CODE32    ; GDT[1] = Code Segment Descriptor
      pushq   GDT_DESC_NULL               ; GDT[0] = Null Segment Descriptor
      mov     rax, rsp                    ; RAX = &GDT[0]

      sub     rsp, 8                      ; Allocate the GDTR on the stack
      mov     [rsp + 2], eax              ; GTDR[16:47] = RAX
      mov     WORD [rsp], GDT_LIMIT       ; GDTR[0:15] = GDT_LIMIT
      mov     rax, rsp                    ; RAX = GDTR base address

      sub     rsp, 2                      ; Allocate far pointer CS on the stack
      call    .get_current_rip            ; Push RIP
.get_current_rip:                         ; FarPtr.offset = .disable_paging
      add     DWORD [rsp], .disable_paging - .get_current_rip
      mov     WORD [rsp + 4], BOOT_CS     ; FarPtr.cs = BOOT_CS

      lgdt    [rax]                       ; Load 32-bit (compatibility mode) GDT
      jmp far DWORD [rsp]                 ; Reload CS and serialize the CPU
                                          ; We use DWORD, because jmp far QWORD
                                          ; with absolute indirect addressing is
                                          ; not supported on AMD platforms.
%else
[BITS 32]
      mov     esi, [esp + SIZEOF_PTR]     ; ESI = handoff
      mov     esp, [esi + OFFSET_STACK]   ; ESP = handoff->stack (new stack)
      add     esp, TRAMPOLINE_STACK_SIZE  ; x86 stack grows downward

      push    esi                         ; Save ESI on the new stack
      push    DWORD [esi + OFFSET_RELOCS] ; ARG1 = handoff->reloc_table
      call    [esi + OFFSET_RELOCATE]     ; handoff->do_reloc(ARG1)
      add     esp, 4                      ; Adjust the stack (pop ARG1)
      pop     esi                         ; Restore ESI from the stack

      mov     ebx, [esi + OFFSET_EBI]     ; EBX = handoff->ebi
      mov     ebp, [esi + OFFSET_KERNEL]  ; EBP = handoff->kernel
      mov     edx, [esi + OFFSET_MAGIC]   ; EDX = handoff->ebi_magic

      and     esp, ~(GDT_DESC_SIZE - 1)   ; Align the GDT on 8 bytes
      pushq   GDT_DESC_4GB_FLAT_DATA32    ; GDT[2] = Data Segment Descriptor
      pushq   GDT_DESC_4GB_FLAT_CODE32    ; GDT[1] = Code Segment Descriptor
      pushq   GDT_DESC_NULL               ; GDT[0] = Null Segment Descriptor

      mov     eax, esp                    ; EAX = &GDT[0]
      push    eax                         ; GDTR.base  = &GDT[0]
      push    WORD GDT_LIMIT              ; GDTR.limit = GDT_LIMIT

      push    WORD BOOT_CS                ; Push the code segment
      call    .get_current_eip            ; push EIP
.get_current_eip:                         ; [ESP] [47:32] = BOOT_CS
      add     DWORD [esp], .disable_paging - .get_current_eip

      lgdt    [esp + 6]                   ; Load 32-bit GDT
      jmp far DWORD [esp]                 ; Reload CS and serialize the CPU
%endif

;;-- AT THIS POINT -------------------------------------------------------------
;;
;;      We are either in compatibility mode, or in protected mode.
;;      New GDT is up, and describes a 4-Gb flat 32-bit memory.
;;
;;      EBX = ESXBootInfo or Multiboot Info structure address
;;      EBP = kernel entry point
;;------------------------------------------------------------------------------
.disable_paging:
[BITS 32]
      mov      eax, cr0                   ; EAX = CR0
      btr      eax, CR0_PAGING_BIT        ; Clear (disable) paging bit
      mov      cr0, eax                   ; Disable paging and clear EFER.LMA

%if IS_64_BIT
      mov      esi, edx                   ; Save ESXBootInfo or Multiboot magic
                                          ; EDX via rdmsr for 64-bit.
      mov      ecx, MSR_EFER              ; ECX selects MSR EFER
      rdmsr                               ; EDX:EAX = MSR EFER
      btr      eax, MSR_EFER_LME_BIT      ; Clear (disable) Long Mode bit
      wrmsr                               ; MSR EFER = EDX:EAX
      mov      edx, esi                   ; Restore ESXBootInfo magic
%endif

;;
;; Here we are in protected mode, paging disabled.
;;
      mov      ax, BOOT_DS                ; For reloading data segments
      mov      ds, ax                     ; DS = BOOT_DS
      mov      es, ax                     ; ES = BOOT_DS
      mov      fs, ax                     ; FS = BOOT_DS
      mov      gs, ax                     ; GS = BOOT_DS
      mov      ss, ax                     ; SS = BOOT_DS

      mov      eax, edx                   ; EAX = ESXBootInfo or Multiboot magic
                                          ; EBI address is already in EBX
      call     ebp                        ; handoff->kernel()

;;
;; Not supposed to be reached.
;;
.not_reached:                             ; Kernel is not supposed to return!
      hlt
      jmp     .not_reached
