;;******************************************************************************
;; Copyright (c) 2008-2012,2015-2016,2020-2023 VMware, Inc. All rights reserved.
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
;;      The safe memory copy of the trampoline may reside in memory above 4GB,
;;      so a copy of it is made to low memory (at handoff->trampo_low) after the
;;      relocations have completed, from which it continues to execute. This is
;;      required because a far jump to 32-bit code from 64-bits needs to be in
;;      low memory.
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

                                          ; Copy trampoline to low-mem
      call    .copy_trampoline            ; Push RIP
.copy_trampoline:
      mov     rsi, [rsp]                  ; Src = trampoline address
      sub     rsi,  .copy_trampoline - trampoline
      mov     rdi, [r12 + OFFSET_TRLOW]   ; Dest = handoff->trampo_low
      mov     rcx, .trampoline_end - trampoline
      rep movsb                           ; Memcpy(RSI -> RDI, RCX)

      mov     rax, [r12 + OFFSET_TRLOW]
      add     rax, .lowmem_rip - trampoline
      jmp     rax                         ; Jump to code in low-mem
.lowmem_rip:

      mov     rbx, [r12 + OFFSET_EBI]     ; RBX = handoff->ebi
      mov     rbp, [r12 + OFFSET_KERNEL]  ; RBP = handoff->kernel
      mov     rdx, [r12 + OFFSET_MAGIC]   ; RDX = handoff->ebi_magic

      mov     rsp, [r12 + OFFSET_TRLOW]   ; Switch stack to low-mem
      add     rsp, .trampoline_end - trampoline + TRAMPOLINE_STACK_SIZE

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
.get_current_rip:
      mov     r9d, .disable_paging - .get_current_rip
      mov     r10d, .reload_segments - .get_current_rip
      call    .tdx_enabled                ; Check if TDX is enabled
      test    r8d, r8d
      cmovnz  r9d, r10d                   ; Skip clearing CR0.PG and EFER.LME
      add     DWORD [rsp], r9d            ; FarPtr.offset = R9
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
;;      EDX = ESXBootInfo or Multiboot magic
;;      EBP = kernel entry point
;;------------------------------------------------------------------------------
.disable_paging:
[BITS 32]
      mov      eax, cr0                   ; EAX = CR0
      btr      eax, CR0_PAGING_BIT        ; Clear (disable) paging bit
      mov      cr0, eax                   ; Disable paging and clear EFER.LMA
      mov      eax, cr4                   ; EAX = CR4
      btr      eax, CR4_LA57_BIT          ; Clear (disable) 5-level paging bit
      mov      cr4, eax

%if IS_64_BIT
      mov      esi, edx                   ; Save ESXBootInfo or Multiboot magic
                                          ; EDX via rdmsr for 64-bit.
      mov      ecx, MSR_EFER              ; ECX selects MSR EFER
      rdmsr                               ; EDX:EAX = MSR EFER
      btr      eax, MSR_EFER_LME_BIT      ; Clear (disable) Long Mode bit
      wrmsr                               ; MSR EFER = EDX:EAX
      mov      edx, esi                   ; Restore ESXBootInfo magic
%endif

.reload_segments:
;;
;; Here we are in protected mode, paging disabled.
;; On Intel TDX, we are still in compatibility mode.
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

%if IS_64_BIT
[BITS 64]
;;
;; Returns 1 in R8 if Intel TDX is enabled and 0 otherwise.
;;
.tdx_enabled:
      push    rax                         ; Save registers clobbered by CPUID
      push    rbx
      push    rcx
      push    rdx
      xor     r8d, r8d                    ; R8 = false
      xor     eax, eax                    ; EAX = Max Leaf/Vendor String
      cpuid                               ; Check if leaf 0x21 is supported
      cmp     eax, CPUID_INTEL_TDX_CAPS
      jb      .tdx_enabled_ret
      mov     eax, CPUID_INTEL_TDX_CAPS   ; EAX = Intel TDX Capabilities
      xor     ecx, ecx                    ; ECX = Max Leaf/Vendor String
      cpuid                               ; Check if Intel TDX is enabled
      cmp     ebx, INTEL_TDX_VENDOR_ID_EBX
      jne     .tdx_enabled_ret
      cmp     ecx, INTEL_TDX_VENDOR_ID_ECX
      jne     .tdx_enabled_ret
      cmp     edx, INTEL_TDX_VENDOR_ID_EDX
      inc     r8d                         ; R8 = true
.tdx_enabled_ret:
      pop     rdx                         ; Restore registers clobbered by CPUID
      pop     rcx
      pop     rbx
      pop     rax
      ret
%endif

.trampoline_end:
