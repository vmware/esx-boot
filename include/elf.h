/*******************************************************************************
 * Portions Copyright (c) 2009-2011,2015,2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*-
 * Copyright (c) 1996-1997 John D. Polstra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/i386/include/elf.h,v 1.16 2004/08/02 19:12:17 dfr Exp $
 * $FreeBSD: src/sys/sys/elf_common.h,v 1.15 2004/05/05 02:38:54 marcel Exp $
 * $FreeBSD: src/sys/sys/elf32.h,v 1.8 2002/05/30 08:32:18 dfr Exp $
 * $FreeBSD: src/sys/sys/elf64.h,v 1.10 2002/05/30 08:32:18 dfr Exp $
 * $FreeBSD: src/sys/sys/elf_generic.h,v 1.6 2002/07/20 02:56:11 peter Exp $
 */

#ifndef ELF_H_
#define ELF_H_

/* *****************************************************************************
 * src/sys/sys/elf_common.h
 * ****************************************************************************/

#include <compat.h>
#include <stdbool.h>

#ifndef _SYS_ELF_COMMON_H_
#define _SYS_ELF_COMMON_H_

#pragma pack(1)

/*
 * ELF definitions that are independent of architecture or word size.
 */

/*
 * Note header.  The ".note" section contains an array of notes.  Each
 * begins with this header, aligned to a word boundary.  Immediately
 * following the note header is n_namesz bytes of name, padded to the
 * next word boundary.  Then comes n_descsz bytes of descriptor, again
 * padded to a word boundary.  The values of n_namesz and n_descsz do
 * not include the padding.
 */
typedef struct {
   uint32_t n_namesz;               /* Length of name */
   uint32_t n_descsz;               /* Length of descriptor */
   uint32_t n_type;                 /* Type of this note */
} Elf_Note;

/*
 * Indexes into the e_ident array.  Keep synced with
 * http://www.sco.com/developer/gabi/ch4.eheader.html
 */
#define EI_MAG0            0        /* Magic number, byte 0 */
#define EI_MAG1            1        /* Magic number, byte 1 */
#define EI_MAG2            2        /* Magic number, byte 2 */
#define EI_MAG3            3        /* Magic number, byte 3 */
#define EI_CLASS           4        /* Class of machine */
#define EI_DATA            5        /* Data format */
#define EI_VERSION         6        /* ELF format version */
#define EI_OSABI           7        /* Operating system / ABI identification */
#define EI_ABIVERSION      8        /* ABI version */
#define OLD_EI_BRAND       8        /* Start of architecture identification */
#define EI_PAD	            9        /* Start of padding (per SVR4 ABI) */
#define EI_NIDENT          16       /* Size of e_ident array */

/* Values for the magic number bytes */
#define ELFMAG0            0x7f
#define ELFMAG1            'E'
#define ELFMAG2            'L'
#define ELFMAG3            'F'
#define ELFMAG	            "\177ELF"   /* magic string */
#define SELFMAG            4           /* magic string size */

/* Values for e_ident[EI_VERSION] and e_version */
#define EV_NONE            0
#define EV_CURRENT         1

/* Values for e_ident[EI_CLASS] */
#define ELFCLASSNONE       0        /* Unknown class */
#define ELFCLASS32         1        /* 32-bit architecture */
#define ELFCLASS64         2        /* 64-bit architecture */

/* Values for e_ident[EI_DATA] */
#define ELFDATANONE        0        /* Unknown data format */
#define ELFDATA2LSB        1        /* 2's complement little-endian */
#define ELFDATA2MSB        2        /* 2's complement big-endian */

/* Values for e_ident[EI_OSABI] */
#define ELFOSABI_SYSV      0              /* UNIX System V ABI */
#define ELFOSABI_NONE      ELFOSABI_SYSV  /* symbol used in old spec */
#define ELFOSABI_HPUX      1              /* HP-UX operating system */
#define ELFOSABI_NETBSD    2              /* NetBSD */
#define ELFOSABI_LINUX     3              /* GNU/Linux */
#define ELFOSABI_HURD      4              /* GNU/Hurd */
#define ELFOSABI_86OPEN    5              /* 86Open common IA32 ABI */
#define ELFOSABI_SOLARIS   6              /* Solaris */
#define ELFOSABI_MONTEREY  7              /* Monterey */
#define ELFOSABI_IRIX      8              /* IRIX */
#define ELFOSABI_FREEBSD   9              /* FreeBSD */
#define ELFOSABI_TRU64     10             /* TRU64 UNIX */
#define ELFOSABI_MODESTO   11             /* Novell Modesto */
#define ELFOSABI_OPENBSD   12             /* OpenBSD */
#define ELFOSABI_ARM       97             /* ARM */
#define ELFOSABI_STANDALONE   255         /* Standalone (embedded) application */

/* e_ident */
#define IS_ELF(ehdr)                         \
   ((ehdr).e_ident[EI_MAG0] == ELFMAG0 &&    \
   (ehdr).e_ident[EI_MAG1] == ELFMAG1  &&    \
   (ehdr).e_ident[EI_MAG2] == ELFMAG2  &&    \
   (ehdr).e_ident[EI_MAG3] == ELFMAG3)

/* Values for e_type */
#define ET_NONE            0        /* Unknown type */
#define ET_REL             1        /* Relocatable */
#define ET_EXEC            2        /* Executable */
#define ET_DYN             3        /* Shared object */
#define ET_CORE            4        /* Core file */

/* Values for e_machine */
#define EM_NONE            0        /* Unknown machine */
#define EM_386             3        /* Intel i386 */
#define EM_X86_64          62       /* Advanced Micro Devices x86-64 */
#define EM_AARCH64         183      /* 64-bit Arm Architecture */
#define EM_RISCV64         243      /* 64-bit RISC-V Architecture */

/* Special section indexes */
#define SHN_UNDEF          0        /* Undefined, missing, irrelevant */
#define SHN_LORESERVE      0xff00   /* First of reserved range */
#define SHN_LOPROC         0xff00   /* First processor-specific */
#define SHN_HIPROC         0xff1f   /* Last processor-specific */
#define SHN_ABS            0xfff1   /* Absolute values */
#define SHN_COMMON         0xfff2   /* Common data */
#define SHN_HIRESERVE      0xffff   /* Last of reserved range */

/* sh_type */
#define SHT_NULL           0           /* inactive */
#define SHT_PROGBITS       1           /* program defined information */
#define SHT_SYMTAB         2           /* symbol table section */
#define SHT_STRTAB         3           /* string table section */
#define SHT_RELA           4           /* relocation section with addends */
#define SHT_HASH           5           /* symbol hash table section */
#define SHT_DYNAMIC        6           /* dynamic section */
#define SHT_NOTE           7           /* note section */
#define SHT_NOBITS         8           /* no space section */
#define SHT_REL            9           /* relocation section - no addends */
#define SHT_SHLIB          10          /* reserved - purpose unknown */
#define SHT_DYNSYM         11          /* dynamic symbol table section */
#define SHT_NUM            12          /* number of section types */
#define SHT_LOOS           0x60000000  /* First of OS specific semantics */
#define SHT_HIOS           0x6fffffff  /* Last of OS specific semantics */
#define SHT_LOPROC         0x70000000  /* reserved range for processor */
#define SHT_HIPROC         0x7fffffff  /* specific section header types */
#define SHT_LOUSER         0x80000000  /* reserved range for application */
#define SHT_HIUSER         0xffffffff  /* specific indexes */

/* Flags for sh_flags */
#define SHF_WRITE          0x1         /* Section contains writable data */
#define SHF_ALLOC          0x2         /* Section occupies memory */
#define SHF_EXECINSTR      0x4         /* Section contains instructions */
#define SHF_TLS            0x400       /* Section contains TLS data */
#define SHF_MASKPROC       0xf0000000  /* Reserved for processor-specific */

/* Values for p_type */
#define PT_NULL            0  /* Unused entry */
#define PT_LOAD            1  /* Loadable segment */
#define PT_DYNAMIC         2  /* Dynamic linking information segment */
#define PT_INTERP          3  /* Pathname of interpreter */
#define PT_NOTE            4  /* Auxiliary information */
#define PT_SHLIB           5  /* Reserved (not used) */
#define PT_PHDR            6  /* Location of program header itself */
#define PT_TLS             7  /* Thread local storage segment */

#define PT_COUNT           8  /* Number of defined p_type values */

#define PT_LOOS            0x60000000  /* OS-specific */
#define PT_HIOS            0x6fffffff  /* OS-specific */
#define PT_LOPROC          0x70000000  /* First processor-specific type */
#define PT_HIPROC          0x7fffffff  /* Last processor-specific type */

/* Values for p_flags */
#define PF_X               0x1   /* Executable */
#define PF_W               0x2   /* Writable */
#define PF_R               0x4   /* Readable */

/* Values for d_tag */
#define DT_NULL            0  /* Terminating entry */
#define DT_NEEDED          1  /* String table offset of a needed shared library */
#define DT_PLTRELSZ        2  /* Total size in bytes of PLT relocations */
#define DT_PLTGOT          3  /* Processor-dependent address */
#define DT_HASH            4  /* Address of symbol hash table */
#define DT_STRTAB          5  /* Address of string table */
#define DT_SYMTAB          6  /* Address of symbol table */
#define DT_RELA            7  /* Address of ElfNN_Rela relocations */
#define DT_RELASZ          8  /* Total size of ElfNN_Rela relocations */
#define DT_RELAENT         9  /* Size of each ElfNN_Rela relocation entry */
#define DT_STRSZ           10 /* Size of string table */
#define DT_SYMENT          11 /* Size of each symbol table entry */
#define DT_INIT            12 /* Address of initialization function */
#define DT_FINI            13 /* Address of finalization function */
#define DT_SONAME          14 /* String table offset of shared object name */
#define DT_RPATH           15 /* String table offset of library path. [sup] */
#define DT_SYMBOLIC        16 /* Indicates "symbolic" linking. [sup] */
#define DT_REL             17 /* Address of ElfNN_Rel relocations */
#define DT_RELSZ           18 /* Total size of ElfNN_Rel relocations */
#define DT_RELENT          19 /* Size of each ElfNN_Rel relocation */
#define DT_PLTREL          20 /* Type of relocation used for PLT */
#define DT_DEBUG           21 /* Reserved (not used) */
#define DT_TEXTREL         22 /* Indicates there may be relocations in
                                 non-writable segments. [sup] */
#define DT_JMPREL          23 /* Address of PLT relocations */
#define DT_BIND_NOW        24 /* [sup] */
#define DT_INIT_ARRAY      25 /* Address of the array of pointers to
                                 initialization functions */
#define DT_FINI_ARRAY      26 /* Address of the array of pointers to
                                 termination functions */
#define DT_INIT_ARRAYSZ    27 /* Size in bytes of the array of
                                 initialization functions */
#define DT_FINI_ARRAYSZ    28 /* Size in bytes of the array of termination
                                 functions */
#define DT_RUNPATH         29 /* String table offset of a null-terminated
                                 library search path string */
#define DT_FLAGS           30 /* Object specific flag values */
#define DT_ENCODING        32 /* Values greater than or equal to DT_ENCODING
                                 and less than DT_LOOS follow the rules for
                                 the interpretation of the d_un union
                                 as follows: even == 'd_ptr', even == 'd_val'
                                 or none */
#define DT_PREINIT_ARRAY   32 /* Address of the array of pointers to
                                 pre-initialization functions */
#define DT_PREINIT_ARRAYSZ 33 /* Size in bytes of the array of
                                 pre-initialization functions */

#define DT_COUNT           33 /* Number of defined d_tag values */

#define DT_LOOS            0x6000000d  /* First OS-specific */
#define DT_HIOS            0x6fff0000  /* Last OS-specific */
#define DT_LOPROC          0x70000000  /* First processor-specific type */
#define DT_HIPROC          0x7fffffff  /* Last processor-specific type */

/* Values for DT_FLAGS */
#define DF_ORIGIN          0x0001   /* Indicates that the object being loaded
                                       may make reference to the $ORIGIN
                                       substitution string */
#define DF_SYMBOLIC        0x0002   /* Indicates "symbolic" linking */
#define DF_TEXTREL         0x0004   /* Indicates there may be relocations in
                                       non-writable segments */
#define DF_BIND_NOW        0x0008   /* Indicates that the dynamic linker should
                                       process all relocations for the object
                                       containing this entry before transferring
                                       control to the program */
#define DF_STATIC_TLS      0x0010   /* Indicates that the shared object or
                                       executable contains code using a static
                                       thread-local storage scheme */

/* Values for n_type. Used in core files */
#define NT_PRSTATUS        1  /* Process status */
#define NT_FPREGSET        2  /* Floating point registers */
#define NT_PRPSINFO        3  /* Process state info */
#define NT_AUXV            6  /* Auxiliary info */

/* Symbol Binding - ELFNN_ST_BIND - st_info */
#define STB_LOCAL          0  /* Local symbol */
#define STB_GLOBAL         1  /* Global symbol */
#define STB_WEAK           2  /* like global - lower precedence */
#define STB_LOPROC         13 /* reserved range for processor */
#define STB_HIPROC         15 /*  specific symbol bindings */

/* Symbol type - ELFNN_ST_TYPE - st_info */
#define STT_NOTYPE         0  /* Unspecified type */
#define STT_OBJECT         1  /* Data object */
#define STT_FUNC           2  /* Function */
#define STT_SECTION        3  /* Section */
#define STT_FILE           4  /* Source file */
#define STT_TLS            6  /* TLS object */
#define STT_LOPROC         13 /* reserved range for processor */
#define STT_HIPROC         15 /*  specific symbol types */

/* Special symbol table indexes */
#define STN_UNDEF          0  /* Undefined symbol index */

#endif /* !_SYS_ELF_COMMON_H_ */

/* *****************************************************************************
 * src/sys/sys/elf32.h
 * ****************************************************************************/

#ifndef _SYS_ELF32_H_
#define _SYS_ELF32_H_

/*
 * ELF definitions common to all 32-bit architectures.
 */

typedef uint32_t  Elf32_Addr;
typedef uint16_t  Elf32_Half;
typedef uint32_t  Elf32_Off;
typedef int32_t   Elf32_Sword;
typedef uint32_t  Elf32_Word;
typedef uint32_t  Elf32_Size;
typedef Elf32_Off Elf32_Hashelt;

/*
 * ELF header.
 */
typedef struct {
   unsigned char e_ident[EI_NIDENT];   /* File identification */
   Elf32_Half e_type;                  /* File type */
   Elf32_Half e_machine;               /* Machine architecture */
   Elf32_Word e_version;               /* ELF format version */
   Elf32_Addr e_entry;                 /* Entry point */
   Elf32_Off e_phoff;                  /* Program header file offset */
   Elf32_Off e_shoff;                  /* Section header file offset */
   Elf32_Word e_flags;                 /* Architecture-specific flags */
   Elf32_Half e_ehsize;                /* Size of ELF header in bytes */
   Elf32_Half e_phentsize;             /* Size of program header entry */
   Elf32_Half e_phnum;                 /* Number of program header entries */
   Elf32_Half e_shentsize;             /* Size of section header entry */
   Elf32_Half e_shnum;                 /* Number of section header entries */
   Elf32_Half e_shstrndx;              /* Section name strings section */
} Elf32_Ehdr;

/*
 * Section header.
 */
typedef struct {
   Elf32_Word sh_name;        /* Section name (index into the section header
                                 string table) */
   Elf32_Word sh_type;        /* Section type */
   Elf32_Word sh_flags;       /* Section flags */
   Elf32_Addr sh_addr;        /* Address in memory image */
   Elf32_Off sh_offset;       /* Offset in file */
   Elf32_Size sh_size;        /* Size in bytes */
   Elf32_Word sh_link;        /* Index of a related section */
   Elf32_Word sh_info;        /* Depends on section type */
   Elf32_Size sh_addralign;   /* Alignment in bytes */
   Elf32_Size sh_entsize;     /* Size of each entry in section */
} Elf32_Shdr;

/*
 * Program header.
 */
typedef struct {
   Elf32_Word p_type;         /* Entry type */
   Elf32_Off p_offset;        /* File offset of contents */
   Elf32_Addr p_vaddr;        /* Virtual address in memory image */
   Elf32_Addr p_paddr;        /* Physical address (not used) */
   Elf32_Size p_filesz;       /* Size of contents in file */
   Elf32_Size p_memsz;        /* Size of contents in memory */
   Elf32_Word p_flags;        /* Access permission flags */
   Elf32_Size p_align;        /* Alignment in memory and file */
} Elf32_Phdr;

/*
 * Dynamic structure.  The ".dynamic" section contains an array of them.
 */
typedef struct {
   Elf32_Sword d_tag;         /* Entry type */
   union {
      Elf32_Size d_val;       /* Integer value */
      Elf32_Addr d_ptr;       /* Address value */
   } d_un;
} Elf32_Dyn;

/*
 * Relocation entries.
 */

/* Relocations that don't need an addend field */
typedef struct {
   Elf32_Addr r_offset;       /* Location to be relocated */
   Elf32_Word r_info;         /* Relocation type and symbol index */
} Elf32_Rel;

/* Relocations that need an addend field */
typedef struct {
   Elf32_Addr r_offset;       /* Location to be relocated */
   Elf32_Word r_info;         /* Relocation type and symbol index */
   Elf32_Sword r_addend;      /* Addend */
} Elf32_Rela;

/* Macros for accessing the fields of r_info */
#define ELF32_R_SYM(info)     ((info) >> 8)
#define ELF32_R_TYPE(info)    ((unsigned char)(info))

/* Macro for constructing r_info from field values */
#define ELF32_R_INFO(sym, type)  (((sym) << 8) + (unsigned char)(type))

/*
 * Symbol table entries.
 */
typedef struct {
   Elf32_Word st_name;        /* String table index of name */
   Elf32_Addr st_value;       /* Symbol value */
   Elf32_Size st_size;        /* Size of associated object */
   unsigned char st_info;     /* Type and binding information */
   unsigned char st_other;    /* Reserved (not used) */
   Elf32_Half st_shndx;       /* Section index of symbol */
} Elf32_Sym;

/* Macros for accessing the fields of st_info */
#define ELF32_ST_BIND(info)   ((info) >> 4)
#define ELF32_ST_TYPE(info)   ((info) & 0xf)

/* Macro for constructing st_info from field values */
#define ELF32_ST_INFO(bind, type)   (((bind) << 4) + ((type) & 0xf))

#endif /* !_SYS_ELF32_H_ */


/* *****************************************************************************
 * src/sys/sys/elf64.h
 * ****************************************************************************/

#ifndef _SYS_ELF64_H_
#define _SYS_ELF64_H_

/*
 * ELF definitions common to all 64-bit architectures.
 */
typedef uint64_t  Elf64_Addr;
typedef uint32_t  Elf64_Half;
typedef uint64_t  Elf64_Off;
typedef int64_t   Elf64_Sword;
typedef uint64_t  Elf64_Word;
typedef uint64_t  Elf64_Size;
typedef uint16_t  Elf64_Quarter;

/*
 * Types of dynamic symbol hash table bucket and chain elements.
 *
 * This is inconsistent among 64 bit architectures, so a machine dependent
 * typedef is required.
 */

#ifdef __alpha__
typedef Elf64_Off	Elf64_Hashelt;
#else
typedef Elf64_Half   Elf64_Hashelt;
#endif

/*
 * ELF header.
 */
typedef struct {
   unsigned char e_ident[EI_NIDENT];   /* File identification */
   Elf64_Quarter e_type;               /* File type */
   Elf64_Quarter e_machine;            /* Machine architecture */
   Elf64_Half e_version;               /* ELF format version */
   Elf64_Addr e_entry;                 /* Entry point */
   Elf64_Off e_phoff;                  /* Program header file offset */
   Elf64_Off e_shoff;                  /* Section header file offset */
   Elf64_Half e_flags;                 /* Architecture-specific flags */
   Elf64_Quarter e_ehsize;             /* Size of ELF header in bytes */
   Elf64_Quarter e_phentsize;          /* Size of program header entry */
   Elf64_Quarter e_phnum;              /* Number of program header entries */
   Elf64_Quarter e_shentsize;          /* Size of section header entry */
   Elf64_Quarter e_shnum;              /* Number of section header entries */
   Elf64_Quarter e_shstrndx;           /* Section name strings section */
} Elf64_Ehdr;

/*
 * Section header.
 */
typedef struct {
   Elf64_Half sh_name;        /* Section name (index into the section header
                                 string table) */
   Elf64_Half sh_type;        /* Section type */
   Elf64_Size sh_flags;       /* Section flags */
   Elf64_Addr sh_addr;        /* Address in memory image */
   Elf64_Off sh_offset;       /* Offset in file */
   Elf64_Size sh_size;        /* Size in bytes */
   Elf64_Half sh_link;        /* Index of a related section */
   Elf64_Half sh_info;        /* Depends on section type */
   Elf64_Size sh_addralign;   /* Alignment in bytes */
   Elf64_Size sh_entsize;     /* Size of each entry in section */
} Elf64_Shdr;

/*
 * Program header.
 */
typedef struct {
   Elf64_Half p_type;         /* Entry type */
   Elf64_Half p_flags;        /* Access permission flags */
   Elf64_Off p_offset;        /* File offset of contents */
   Elf64_Addr p_vaddr;        /* Virtual address in memory image */
   Elf64_Addr p_paddr;        /* Physical address (not used) */
   Elf64_Size p_filesz;       /* Size of contents in file */
   Elf64_Size p_memsz;        /* Size of contents in memory */
   Elf64_Size p_align;        /* Alignment in memory and file */
} Elf64_Phdr;

/*
 * Dynamic structure.  The ".dynamic" section contains an array of them.
 */
typedef struct {
   Elf64_Size d_tag;          /* Entry type */
   union {
      Elf64_Size d_val;       /* Integer value */
      Elf64_Addr d_ptr;       /* Address value */
   } d_un;
} Elf64_Dyn;

/*
 * Relocation entries.
 */

/* Relocations that don't need an addend field */
typedef struct {
   Elf64_Addr r_offset;       /* Location to be relocated */
   Elf64_Size r_info;         /* Relocation type and symbol index */
} Elf64_Rel;

/* Relocations that need an addend field */
typedef struct {
   Elf64_Addr r_offset;       /* Location to be relocated */
   Elf64_Size r_info;         /* Relocation type and symbol index */
   Elf64_Off r_addend;        /* Addend */
} Elf64_Rela;

/* Macros for accessing the fields of r_info */
#define ELF64_R_SYM(info)     ((info) >> 32)
#define ELF64_R_TYPE(info)    ((unsigned char)(info))

/* Macro for constructing r_info from field values */
#define ELF64_R_INFO(sym, type)  (((sym) << 32) + (unsigned char)(type))

/*
 * Symbol table entries.
 */
typedef struct {
   Elf64_Half st_name;        /* String table index of name */
   unsigned char st_info;     /* Type and binding information */
   unsigned char st_other;    /* Reserved (not used) */
   Elf64_Quarter st_shndx;    /* Section index of symbol */
   Elf64_Addr st_value;       /* Symbol value */
   Elf64_Size st_size;        /* Size of associated object */
} Elf64_Sym;

/* Macros for accessing the fields of st_info */
#define ELF64_ST_BIND(info)   ((info) >> 4)
#define ELF64_ST_TYPE(info)   ((info) & 0xf)

/* Macro for constructing st_info from field values */
#define ELF64_ST_INFO(bind, type)   (((bind) << 4) + ((type) & 0xf))

#endif /* !_SYS_ELF64_H_ */

/* *****************************************************************************
 * src/sys/sys/elf_generic.h
 * ****************************************************************************/

#ifndef _SYS_ELF_GENERIC_H_
#define _SYS_ELF_GENERIC_H_

/*
 * Calling CONCAT1 from CONCAT forces argument expansion.
 */
#ifdef __CONCAT
#undef __CONCAT
#endif

#define __CONCAT1(x, y)    x ## y
#define __CONCAT(x, y)     __CONCAT1(x, y)

#endif /* !_SYS_ELF_GENERIC_H_ */

/*
 * Auxiliary vector entries for passing information to the interpreter.
 *
 * The i386 supplement to the SVR4 ABI specification names this "auxv_t",
 * but POSIX lays claim to all symbols ending with "_t".
 */
typedef struct {              /* Auxiliary vector entry on initial stack */
   int a_type;                /* Entry type */
   union {
      int a_val;              /* Integer value */
   } a_un;
} Elf32_Auxinfo;

typedef struct {              /* Auxiliary vector entry on initial stack */
   long a_type;               /* Entry type */
   union {
      long a_val;             /* Integer value */
      void *a_ptr;            /* Address */
      void (*a_fcn)(void);    /* Function pointer (not used) */
   } a_un;
} Elf64_Auxinfo;

/* Values for a_type */
#define AT_NULL            0  /* Terminates the vector */
#define AT_IGNORE          1  /* Ignored entry */
#define AT_EXECFD          2  /* File descriptor of program to load */
#define AT_PHDR            3  /* Program header of program already loaded */
#define AT_PHENT           4  /* Size of each program header entry */
#define AT_PHNUM           5  /* Number of program header entries */
#define AT_PAGESZ          6  /* Page size in bytes */
#define AT_BASE            7  /* Interpreter's base address */
#define AT_FLAGS           8  /* Flags (unused for i386) */
#define AT_ENTRY           9  /* Where interpreter should transfer control */

/*
 * The following non-standard values are used for passing information
 * from John Polstra's testbed program to the dynamic linker.  These
 * are expected to go away soon.
 *
 * Unfortunately, these overlap the Linux non-standard values, so they
 * must not be used in the same context.
 */
#define AT_BRK             10 /* Starting point for sbrk and brk */
#define AT_DEBUG           11 /* Debugging level */

/*
 * The following non-standard values are used in Linux ELF binaries.
 */
#define AT_NOTELF          10 /* Program is not ELF ?? */
#define AT_UID             11 /* Real uid */
#define AT_EUID            12 /* Effective uid */
#define AT_GID             13 /* Real gid */
#define AT_EGID            14 /* Effective gid */
#define AT_PLATFORM        15 /* string identifying CPU for optimizations */
#define AT_HWCAP           16 /* arch dependent hints at CPU capabilities */
#define AT_CLKTCK          17 /* frequency at which times() increments */
                              /* 18..22 = ? */
#define AT_SECURE          23	/* secure mode boolean */
#define AT_COUNT           (AT_SECURE + 1)   /* Count of defined aux entry types */

/*
 * 32-bit Relocation types.
 */
#define R_386_NONE         0  /* No relocation */
#define R_386_32           1  /* Add symbol value */
#define R_386_PC32         2  /* Add PC-relative symbol value */
#define R_386_GOT32        3  /* Add PC-relative GOT offset */
#define R_386_PLT32        4  /* Add PC-relative PLT offset */
#define R_386_COPY         5  /* Copy data from shared object */
#define R_386_GLOB_DAT     6  /* Set GOT entry to data address */
#define R_386_JMP_SLOT     7  /* Set GOT entry to code address */
#define R_386_RELATIVE     8  /* Add load address of shared object */
#define R_386_GOTOFF       9  /* Add GOT-relative symbol address */
#define R_386_GOTPC        10 /* Add PC-relative GOT table address */
#define R_386_TLS_TPOFF    14 /* Negative offset in static TLS block */
#define R_386_TLS_IE       15 /* Absolute address of GOT for -ve static TLS */
#define R_386_TLS_GOTIE    16 /* GOT entry for negative static TLS block */
#define R_386_TLS_LE       17 /* Negative offset relative to static TLS */
#define R_386_TLS_GD       18 /* 32 bit offset to GOT (index,off) pair */
#define R_386_TLS_LDM      19 /* 32 bit offset to GOT (index,zero) pair */
#define R_386_TLS_GD_32    24 /* 32 bit offset to GOT (index,off) pair */
#define R_386_TLS_GD_PUSH  25 /* pushl instruction for Sun ABI GD sequence */
#define R_386_TLS_GD_CALL  26 /* call instruction for Sun ABI GD sequence */
#define R_386_TLS_GD_POP   27 /* popl instruction for Sun ABI GD sequence */
#define R_386_TLS_LDM_32   28 /* 32 bit offset to GOT (index,zero) pair */
#define R_386_TLS_LDM_PUSH 29 /* pushl instruction for Sun ABI LD sequence */
#define R_386_TLS_LDM_CALL 30 /* call instruction for Sun ABI LD sequence */
#define R_386_TLS_LDM_POP  31 /* popl instruction for Sun ABI LD sequence */
#define R_386_TLS_LDO_32   32 /* 32 bit offset from start of TLS block */
#define R_386_TLS_IE_32    33 /* 32 bit offset to GOT static TLS offset entry */
#define R_386_TLS_LE_32    34 /* 32 bit offset within static TLS block */
#define R_386_TLS_DTPMOD32 35 /* GOT entry containing TLS index */
#define R_386_TLS_DTPOFF32 36 /* GOT entry containing TLS offset */
#define R_386_TLS_TPOFF32  37 /* GOT entry of -ve static TLS offset */
#define R_386_COUNT        38 /* Count of defined relocation types */

/*
 * 64-bit Relocation types.
 */
#define R_X86_64_NONE      0  /* No relocation */
#define R_X86_64_64        1  /* Add 64 bit symbol value */
#define R_X86_64_PC32      2  /* PC-relative 32 bit signed sym value */
#define R_X86_64_GOT32     3  /* PC-relative 32 bit GOT offset */
#define R_X86_64_PLT32     4  /* PC-relative 32 bit PLT offset */
#define R_X86_64_COPY      5  /* Copy data from shared object */
#define R_X86_64_GLOB_DAT  6  /* Set GOT entry to data address */
#define R_X86_64_JMP_SLOT  7  /* Set GOT entry to code address */
#define R_X86_64_RELATIVE  8  /* Add load address of shared object */
#define R_X86_64_GOTPCREL  9  /* Add 32 bit signed pcrel offset to GOT */
#define R_X86_64_32        10 /* Add 32 bit zero extended symbol value */
#define R_X86_64_32S       11 /* Add 32 bit sign extended symbol value */
#define R_X86_64_16        12 /* Add 16 bit zero extended symbol value */
#define R_X86_64_PC16      13 /* Add 16 bit signed extended pc relative symbol
                                 value */
#define R_X86_64_8         14 /* Add 8 bit zero extended symbol value */
#define R_X86_64_PC8       15 /* Add 8 bit signed extended pc relative symbol
                                 value */
#define R_X86_64_DTPMOD64  16 /* ID of module containing symbol */
#define R_X86_64_DTPOFF64  17 /* Offset in TLS block */
#define R_X86_64_TPOFF64   18 /* Offset in static TLS block */
#define R_X86_64_TLSGD     19 /* PC relative offset to GD GOT entry */
#define R_X86_64_TLSLD     20 /* PC relative offset to LD GOT entry */
#define R_X86_64_DTPOFF32  21 /* Offset in TLS block */
#define R_X86_64_GOTTPOFF  22 /* PC relative offset to IE GOT entry */
#define R_X86_64_TPOFF32   23 /* Offset in static TLS block */
#define R_X86_64_COUNT     24 /* Count of defined relocation types */

typedef union Elf_CommonPhdr {
   Elf32_Phdr phdr32;
   Elf64_Phdr phdr64;
} Elf_CommonPhdr;

typedef union Elf_CommonEhdr {
   struct {
      unsigned char e_ident[EI_NIDENT]; /* File identification */
      Elf64_Quarter e_type;             /* File type */
      Elf64_Quarter e_machine;          /* Machine architecture */
      Elf64_Half e_version;             /* ELF format version */
   };
   Elf32_Ehdr ehdr32;
   Elf64_Ehdr ehdr64;
} Elf_CommonEhdr;

typedef union Elf_CommonShdr {
   Elf32_Shdr shdr32;
   Elf64_Shdr shdr64;
} Elf_CommonShdr;

typedef union Elf_CommonSym {
   Elf32_Sym sym32;
   Elf64_Sym sym64;
} Elf_CommonSym;

typedef Elf64_Addr Elf_CommonAddr;

typedef union Elf_CommonRel {
   Elf32_Rel  rel32;
   Elf32_Rela rela32;
   Elf64_Rel  rel64;
   Elf64_Rela rela64;
} Elf_CommonRel;

#pragma pack()

static INLINE size_t Elf_CommonEhdrSize(bool is64)
{
   return (is64) ? sizeof(Elf64_Ehdr) : sizeof(Elf32_Ehdr);
}

static INLINE size_t Elf_CommonPhdrSize(bool is64)
{
   return (is64) ? sizeof(Elf64_Phdr) : sizeof(Elf32_Phdr);
}

static INLINE void Elf_CommonPhdrSetAlign(bool is64, uint32_t align,
                                          Elf_CommonPhdr *phdr)
{
   if (is64) {
      phdr->phdr64.p_align = align;
   } else {
      phdr->phdr32.p_align = align;
   }
}

static INLINE bool Elf_CommonEhdrIs64(Elf_CommonEhdr *ehdr)
{
   return ehdr->e_machine == EM_X86_64 ||
      ehdr->e_machine == EM_AARCH64 ||
      ehdr->e_machine == EM_RISCV64;
}

static INLINE Elf64_Addr Elf_CommonEhdrGetEntry(Elf_CommonEhdr *ehdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return ehdr->ehdr64.e_entry;
   } else {
      return ehdr->ehdr32.e_entry;
   }
}

static INLINE Elf64_Quarter Elf_CommonEhdrGetType(Elf_CommonEhdr *ehdr)
{
   return ehdr->e_type;
}

static INLINE Elf64_Quarter Elf_CommonEhdrGetPhEntSize(Elf_CommonEhdr *ehdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return ehdr->ehdr64.e_phentsize;
   } else {
      return ehdr->ehdr32.e_phentsize;
   }
}

static INLINE Elf64_Quarter Elf_CommonEhdrGetShEntSize(Elf_CommonEhdr *ehdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return ehdr->ehdr64.e_shentsize;
   } else {
      return ehdr->ehdr32.e_shentsize;
   }
}

static INLINE Elf64_Quarter Elf_CommonEhdrGetPhNum(Elf_CommonEhdr *ehdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return ehdr->ehdr64.e_phnum;
   } else {
      return ehdr->ehdr32.e_phnum;
   }
}

static INLINE Elf64_Off Elf_CommonEhdrGetShOff(Elf_CommonEhdr *ehdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return ehdr->ehdr64.e_shoff;
   } else {
      return ehdr->ehdr32.e_shoff;
   }
}

static INLINE Elf64_Quarter Elf_CommonEhdrGetShNum(Elf_CommonEhdr *ehdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return ehdr->ehdr64.e_shnum;
   } else {
      return ehdr->ehdr32.e_shnum;
   }
}

static INLINE Elf64_Quarter Elf_CommonEhdrGetShStrNdx(Elf_CommonEhdr *ehdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return ehdr->ehdr64.e_shstrndx;
   } else {
      return ehdr->ehdr32.e_shstrndx;
   }
}

static INLINE Elf64_Quarter Elf_CommonEhdrGetVersion(Elf_CommonEhdr *ehdr)
{
   return ehdr->e_version;
}

static INLINE Elf64_Off Elf_CommonEhdrGetMachine(Elf_CommonEhdr *ehdr)
{
   return ehdr->e_machine;
}

static INLINE Elf64_Off Elf_CommonEhdrGetPhOff(Elf_CommonEhdr *ehdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return ehdr->ehdr64.e_phoff;
   } else {
      return ehdr->ehdr32.e_phoff;
   }
}

static INLINE Elf_CommonPhdr *Elf_CommonEhdrGetPhdr(Elf_CommonEhdr *ehdr,
                                                    Elf_CommonPhdr *phdr,
                                                    uint32_t which)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return (Elf_CommonPhdr *)((uint8_t *)phdr + (which * sizeof(Elf64_Phdr)));
   } else {
      return (Elf_CommonPhdr *)((uint8_t *)phdr + (which * sizeof(Elf32_Phdr)));
   }
}

static INLINE Elf64_Half Elf_CommonPhdrGetType(Elf_CommonEhdr *ehdr,
                                               Elf_CommonPhdr *phdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return phdr->phdr64.p_type;
   } else {
      return phdr->phdr32.p_type;
   }
}

static INLINE Elf64_Off Elf_CommonPhdrGetOffset(Elf_CommonEhdr *ehdr,
                                                Elf_CommonPhdr *phdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return phdr->phdr64.p_offset;
   } else {
      return phdr->phdr32.p_offset;
   }
}

static INLINE Elf64_Addr Elf_CommonPhdrGetPaddr(Elf_CommonEhdr *ehdr,
                                                Elf_CommonPhdr *phdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return phdr->phdr64.p_paddr;
   } else {
      return phdr->phdr32.p_paddr;
   }
}

static INLINE Elf64_Addr Elf_CommonPhdrGetVaddr(Elf_CommonEhdr *ehdr,
                                                Elf_CommonPhdr *phdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return phdr->phdr64.p_vaddr;
   } else {
      return phdr->phdr32.p_vaddr;
   }
}

static INLINE Elf64_Size Elf_CommonPhdrGetFilesz(Elf_CommonEhdr *ehdr,
                                                 Elf_CommonPhdr *phdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return phdr->phdr64.p_filesz;
   } else {
      return phdr->phdr32.p_filesz;
   }
}

static INLINE Elf64_Size Elf_CommonPhdrGetMemsz(Elf_CommonEhdr *ehdr,
                                                Elf_CommonPhdr *phdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return phdr->phdr64.p_memsz;
   } else {
      return phdr->phdr32.p_memsz;
   }
}

static INLINE Elf64_Half Elf_CommonPhdrGetFlags(Elf_CommonEhdr *ehdr,
                                                Elf_CommonPhdr *phdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return phdr->phdr64.p_flags;
   } else {
      return phdr->phdr32.p_flags;
   }
}

static INLINE Elf64_Size Elf_CommonPhdrGetAlign(Elf_CommonEhdr *ehdr,
                                                Elf_CommonPhdr *phdr)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return phdr->phdr64.p_align;
   } else {
      return phdr->phdr32.p_align;
   }
}

static INLINE Elf_CommonShdr *Elf_CommonShdrGet(Elf_CommonEhdr *ehdr,
                                                int section)
{
   if (Elf_CommonEhdrIs64(ehdr)) {
      return (Elf_CommonShdr *)(char *)((char *)ehdr +
                                        Elf_CommonEhdrGetShOff(ehdr) +
                                        section * sizeof(Elf64_Shdr));
   } else {
      return (Elf_CommonShdr *)(char *)((char *)ehdr +
                                        Elf_CommonEhdrGetShOff(ehdr) +
                                        section * sizeof(Elf32_Shdr));
   }

}

static INLINE Elf64_Off Elf_CommonShdrGetOff(Elf_CommonEhdr *ehdr, int section)
{
   Elf_CommonShdr *shdr = Elf_CommonShdrGet(ehdr, section);
   if (Elf_CommonEhdrIs64(ehdr)) {
      return shdr->shdr64.sh_offset;
   } else {
      return shdr->shdr32.sh_offset;
   }
}

static INLINE Elf64_Half Elf_CommonShdrGetType(Elf_CommonEhdr *ehdr,
                                               int section)
{
   Elf_CommonShdr *shdr = Elf_CommonShdrGet(ehdr, section);
   if (Elf_CommonEhdrIs64(ehdr)) {
      return shdr->shdr64.sh_type;
   } else {
      return shdr->shdr32.sh_type;
   }
}

static INLINE Elf64_Half Elf_CommonShdrGetName(Elf_CommonEhdr *ehdr,
                                               int section)
{
   Elf_CommonShdr *shdr = Elf_CommonShdrGet(ehdr, section);
   if (Elf_CommonEhdrIs64(ehdr)) {
      return shdr->shdr64.sh_name;
   } else {
      return shdr->shdr32.sh_name;
   }
}

static INLINE Elf64_Addr Elf_CommonShdrGetAddr(Elf_CommonEhdr *ehdr,
                                               int section)
{
   Elf_CommonShdr *shdr = Elf_CommonShdrGet(ehdr, section);
   if (Elf_CommonEhdrIs64(ehdr)) {
      return shdr->shdr64.sh_addr;
   } else {
      return shdr->shdr32.sh_addr;
   }
}

static INLINE void Elf_CommonShdrSetAddr(Elf_CommonEhdr *ehdr, int section,
                                         Elf64_Addr addr)
{
   Elf_CommonShdr *shdr = Elf_CommonShdrGet(ehdr, section);
   if (Elf_CommonEhdrIs64(ehdr)) {
      shdr->shdr64.sh_addr = addr;
   } else {
      shdr->shdr32.sh_addr = (Elf32_Addr)addr;
   }
}

static INLINE Elf64_Size Elf_CommonShdrGetSize(Elf_CommonEhdr *ehdr,
                                               int section)
{
   Elf_CommonShdr *shdr = Elf_CommonShdrGet(ehdr, section);
   if (Elf_CommonEhdrIs64(ehdr)) {
      return shdr->shdr64.sh_size;
   } else {
      return shdr->shdr32.sh_size;
   }
}

static INLINE Elf64_Size Elf_CommonShdrGetAddrAlign(Elf_CommonEhdr *ehdr,
                                                    int section)
{
   Elf_CommonShdr *shdr = Elf_CommonShdrGet(ehdr, section);
   if (Elf_CommonEhdrIs64(ehdr)) {
      return shdr->shdr64.sh_addralign;
   } else {
      return shdr->shdr32.sh_addralign;
   }
}

static INLINE void Elf_CommonShdrSetAddrAlign(Elf_CommonEhdr *ehdr,
                                              int section, Elf64_Size align)
{
   Elf_CommonShdr *shdr = Elf_CommonShdrGet(ehdr, section);
   if (Elf_CommonEhdrIs64(ehdr)) {
      shdr->shdr64.sh_addralign = align;
   } else {
      shdr->shdr32.sh_addralign = (Elf32_Size)align;
   }
}

static INLINE Elf64_Size Elf_CommonShdrGetLink(Elf_CommonEhdr *ehdr,
                                               int section)
{
   Elf_CommonShdr *shdr = Elf_CommonShdrGet(ehdr, section);
   if (Elf_CommonEhdrIs64(ehdr)) {
      return shdr->shdr64.sh_link;
   } else {
      return shdr->shdr32.sh_link;
   }
}

static INLINE Elf64_Half Elf_CommonShdrGetInfo(Elf_CommonEhdr *ehdr,
                                               int section)
{
   Elf_CommonShdr *shdr = Elf_CommonShdrGet(ehdr, section);
   if (Elf_CommonEhdrIs64(ehdr)) {
      return shdr->shdr64.sh_info;
   } else {
      return shdr->shdr32.sh_info;
   }
}

static INLINE void *Elf_CommonShdrGetContents(Elf_CommonEhdr *ehdr,
                                              int section)
{
   return (void *)(char *)((char *)ehdr + Elf_CommonShdrGetOff(ehdr, section));
}

static INLINE char *Elf_GetStrSection(Elf_CommonEhdr *ehdr)
{
   return Elf_CommonShdrGetContents(ehdr, Elf_CommonEhdrGetShStrNdx(ehdr));
}

static INLINE char *Elf_GetSectionName(Elf_CommonEhdr *ehdr, int section)
{
   char *strTab = Elf_GetStrSection(ehdr);
   return &strTab[Elf_CommonShdrGetName(ehdr, section)];
}

static INLINE Elf_CommonEhdr *Elf_CommonEhdrFromEhdr32(Elf32_Ehdr *ehdr32)
{
   return (Elf_CommonEhdr*)ehdr32;
}

static INLINE Elf_CommonPhdr *Elf_CommonPhdrFromPhdr32(Elf32_Phdr *phdr32)
{
   return (Elf_CommonPhdr*)phdr32;
}

/*
 * Elf note in memory. Common for 32- and 64-bit.
 */
typedef struct ElfCore_MemElfNote {
   const char *name;
   int type;
   unsigned int datasz;
   void *data;
} ElfCore_MemElfNote;

typedef struct ElfCore_Noteset {
   struct ElfCore_MemElfNote prstatus;
   struct ElfCore_MemElfNote prpsinfo;
   struct ElfCore_MemElfNote prfpreg;
} ElfCore_NoteSet;

#endif /* !ELF_H_ */
