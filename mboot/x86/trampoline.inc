;;******************************************************************************
;; Copyright (c) 2008-2012,2015-2016,2021-2023 VMware, Inc.
;; All rights reserved.
;; SPDX-License-Identifier: GPL-2.0
;;******************************************************************************

;;
;; trampoline.inc
;;

%ifidn __OUTPUT_FORMAT__, win32
%define IS_64_BIT 0
%elifidn __OUTPUT_FORMAT__, elf32
%define IS_64_BIT 0
%elifidn __OUTPUT_FORMAT__, win64
%define IS_64_BIT 1
%elifidn __OUTPUT_FORMAT__, elf64
%define IS_64_BIT 1
%else
%error "Unsupported output format"
%endif

%if IS_64_BIT
[BITS 64]
;;
;; Macro to push a 64-bit immediate value when executing in 64-bit mode.
;; Warning: this macro overwrites RAX.
;;
%macro pushq 1
      mov rax, %1
      push rax
%endmacro
%else
[BITS 32]

;;
;; Macro to push a 64-bit immediate value when executing in 32-bit mode.
;;
%macro pushq 1
      push DWORD %1 >> 32
      push DWORD %1 & 0xffffffff
%endmacro

%endif

;;
;; Size of types
;;
%define SIZEOF_PTR         (__BITS__ / 8)       ; sizeof ((void *))
%define SIZEOF_RUNADDR     8                    ; sizeof (run_addr_t)

;;
;; handoff_t internal offsets
;; (Must be updated if the handoff_t structure is modified)
;;
%define TRAMPOLINE_STACK_SIZE    0x2000

%define OFFSET_STACK       (0 * SIZEOF_RUNADDR) ; handoff->stack
%define OFFSET_RELOCS      (1 * SIZEOF_RUNADDR) ; handoff->relocs
%define OFFSET_RELOCATE    (2 * SIZEOF_RUNADDR) ; handoff->relocate
%define OFFSET_EBI         (3 * SIZEOF_RUNADDR) ; handoff->ebi
%define OFFSET_KERNEL      (4 * SIZEOF_RUNADDR) ; handoff->kernel
%define OFFSET_TRLOW       (5 * SIZEOF_RUNADDR) ; handoff->trampo_low
%define OFFSET_MAGIC       (6 * SIZEOF_RUNADDR) ; handoff->ebi_magic

;;
;; Long Mode control
;;
%define MSR_EFER           0xc0000080           ; Select EFER MSR
%define MSR_EFER_LME_BIT   8                    ; Long mode enabled if set

;;
;; Paging control
;;
%define CR0_PAGING_BIT     31                   ; Paging enabled if set
%define CR4_LA57_BIT       12                   ; 5-level paging enabled if set

;;
;; Segment selectors
;;
%define BOOT_CS            0x08                 ; Index 1 in the GDT (RPL=0)
%define BOOT_DS            0x10                 ; Index 2 in the GDT (RPL=0)

;;
;; Global Descriptor table (GDT)
;;
%define GDT_DESC_SIZE            8
%define GDT_DESC_NULL            0x0
%define GDT_DESC_4GB_FLAT_CODE32 0x00cf9a000000ffff
%define GDT_DESC_4GB_FLAT_DATA32 0x00cf92000000ffff
%define GDT_LIMIT                (3 * GDT_DESC_SIZE - 1)

;;
;; CPUID
;;

%define CPUID_INTEL_TDX_CAPS    0x21
%define INTEL_TDX_VENDOR_ID_EBX 0x65746e49      ; "Inte"
%define INTEL_TDX_VENDOR_ID_ECX 0x20202020      ; "    "
%define INTEL_TDX_VENDOR_ID_EDX 0x5844546c      ; "lTDX"
