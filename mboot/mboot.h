/*******************************************************************************
 * Copyright (c) 2008-2017,2021,2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * mboot.h -- ESXBootInfo loader header file
 */

#ifndef MBOOT_H_
#define MBOOT_H_

#include <bootlib.h>
#include <multiboot.h>
#include <esxbootinfo.h>
#include <elf.h>
#include <vbe.h>
#include <e820.h>
#include <error.h>
#include <efi_info.h>
#include <md5.h>

/*
 * trampoline.s
 */
typedef struct handoff_s handoff_t;

#define TRAMPOLINE_STACK_SIZE 0x2000 /* 2 pages (much more than necessary) */

EXTERN char _trampoline_start[];
EXTERN char _trampoline_end[];

/*
 * Attributes of the trampoline functions:
 *   - parameters passed following the CDECL convention
 *   - function linked in the .trampoline section
 */
#define TRAMPOLINE CDECL SECTION(".trampoline")

typedef void (CDECL *trampoline_t)(handoff_t *);

void TRAMPOLINE trampoline(handoff_t *handoff);

#define TRAMPOLINE_OFFSETOF(_sym_)                                \
   ((ptrdiff_t)(_sym_) - (ptrdiff_t)_trampoline_start)

#define TRAMPOLINE_SIZE() TRAMPOLINE_OFFSETOF(_trampoline_end)

/*
 * system.c
 */
int dump_firmware_info(void);
int firmware_shutdown(e820_range_t **mmap, size_t *count, efi_info_t *efi_info);

/*
 * reloc.c
 */

#define add_kernel_object(_src_, _size_, _dest_)                     \
   add_runtime_object('k', (_src_), (_size_), (_dest_), ALIGN_ANY)

#define add_sysinfo_object(_src_, _size_, _align_)                   \
   add_runtime_object('s', (_src_), (_size_), 0, (_align_))

#define add_module_object(_src_, _size_)                             \
   add_runtime_object('m', (_src_), (_size_), 0, ALIGN_PAGE)

#define add_safe_object(_src_, _size_, _align_)                      \
   add_runtime_object('t', (_src_), (_size_), 0, (_align_))

typedef uint64_t run_addr_t;  /* post-relocation object address */

typedef struct {
   run_addr_t dest;     /* Relocation destination */
   char *src;           /* Data source for memmove() or 0 for bzero() */
   uint64_t size;       /* Relocation length */
   size_t align;        /* Destination must be align on this size */
   char type;           /* Relocation type */
   char visited;        /* Visit counter for detecting circular dependencies */
} reloc_t;

int add_runtime_object(char type, void *src, uint64_t size, run_addr_t dest,
                       size_t align);
int compute_relocations(e820_range_t *mmap, size_t count);
int runtime_addr(const void *addr, run_addr_t *runaddr);
int install_trampoline(trampoline_t *run_trampo, handoff_t **run_handoff);

/*
 * trampoline.c
 */
void TRAMPOLINE do_reloc(reloc_t *reloc);

/*
 * elf.c
 */
int elf_check_headers(void *buffer, size_t buflen, Elf_CommonAddr *base);
int elf_register(void *buffer, Elf_CommonAddr *entry);

/*
 * mboot.c
 */
#define MBOOT_ID_STR    "MBOOT"
#define MBOOT_ID_SIZE   6
#define TITLE_MAX_LEN   128

static inline void PANIC(void)
{
   for (;;) {
      HLT();
   }
}

#define NOT_REACHED() PANIC()

typedef struct {
   Elf_CommonAddr entry;      /* Run-time entry point address */
} kernel_t;

typedef struct {
   char *filename;            /* Module file name */
   char *options;             /* Module option string */
   md5_t md5_compressed;      /* md5sum compressed module */
   md5_t md5_uncompressed;    /* md5sum uncompressed module */
   void *addr;                /* Load address */
   size_t load_size;          /* Compressed module size (in bytes) */
   size_t size;               /* Decompressed module size (in bytes) */
   bool is_loaded;            /* True if the module has been entirely loaded */
   uint64_t load_time;        /* Time(ms) to load the module */
} module_t;

typedef struct {
   char *filename;            /* ACPI table file name */
   bool is_installed;         /* True if the ACPI table has been installed */
   unsigned int key;          /* Firmware key used to uninstall table */
} acpitab_t;

typedef struct {
   char name[MBOOT_ID_SIZE];  /* Bootloader identification string */
   char title[TITLE_MAX_LEN]; /* Title string */
   char *cfgfile;             /* Configuration filename */
   char *prefix;              /* Module path prefix */
   char *crypto;              /* Crypto module filename */
   int volid;                 /* Volume to load the kernel/modules from */
   kernel_t kernel;           /* Kernel information */
   unsigned int modules_nr;   /* Number of modules */
   module_t *modules;         /* Modules cmdlines, addresses and sizes */
   unsigned int acpitab_nr;   /* Number of ACPI tables to load */
   acpitab_t *acpitab;        /* ACPI tables to load */
   e820_range_t *mmap;        /* E820 Memory map */
   size_t mmap_count;         /* Number of entries in the memory map */
   framebuffer_t fb;          /* Framebuffer properties */
   efi_info_t efi_info;       /* EFI-specific information */
   uint64_t load_size;        /* Total size to load (in bytes) */
   uint64_t load_offset;      /* Current amount of loaded memory (in bytes) */
   uint64_t load_time;        /* Total time(ms) to load modules */
   char *recovery_cmd;        /* Command to be executed on <SHIFT+R> */
   bool verbose;              /* Verbose mode (true = on, false = off) */
   bool debug;                /* Debug mode (true = on, false = off) */
   bool headless;             /* True if no video adapter is found */
   bool exit_on_errors;       /* Exit on transient errors */
   bool bootif;               /* Force BOOTIF= on the kernel command line */
   bool no_mem_attr;          /* Do not pass extended attr in Multiboot mmap */
   bool is_network_boot;      /* Is the boot process via network? */
   bool is_esxbootinfo;       /* Is the kernel an ESXBootInfo kernel? */
   bool no_quirks;            /* Do not work around platform quirks */
   bool no_rts;               /* Disable UEFI runtime services support */
   bool serial;               /* Is the serial log enabled? */
   bool tpm_measure;          /* Should TPM measurements be made? */
   uint32_t timeout;          /* Autoboot timeout in units of seconds */
   bool runtimewd;            /* Is there a hardware runtime watchdog? */
   uint32_t runtimewd_timeout;/* Hardware runtime watchdog timeout, seconds */
} boot_info_t;

EXTERN boot_info_t boot;

/*
 * Offsets in trampoline.inc must be updated if this structure is modified.
 */
#pragma pack (1)
struct handoff_s {
   run_addr_t stack;          /* Trampoline stack */
   run_addr_t relocs;         /* Table of relocations */
   run_addr_t relocate;       /* Relocation routine */
   run_addr_t ebi;            /* EFIBootInfo or Multiboot Info structure addr */
   run_addr_t kernel;         /* Kernel entry point */
   run_addr_t trampo_low;     /* Low memory trampoline code copy */
   uint32_t   ebi_magic;      /* EFIBootInfo or Multiboot magic */
};
#pragma pack()

int progress(void);

/*
 * load.c
 */

/*
 * Size Unit type enumeration
 */
typedef enum {
   UNSUPPORTED_UNIT = -1,
   BYTES,
   KILOBYTES,
   MEGABYTES,
   GIGABYTES,
} size_unit_t;

static INLINE const char *size_unit_to_str(size_unit_t size_unit)
{
  static const char *size_units_in_str[] = {" bytes", "KiB", "MiB", "GiB"};

  if (size_unit >= BYTES && size_unit <= GIGABYTES) {
     return size_units_in_str[size_unit];
  }

  return "?";
}

int get_load_size_hint(void);
int load_boot_modules(void);
void unload_boot_modules(void);

/*
 * acpi.c
 */
int install_acpi_tables(void);
void uninstall_acpi_tables(void);

/*
 * config.c
 */
int append_kernel_options(const char *options);
int measure_kernel_options(void);
int parse_config(const char *filename);
void config_clear(void);

/*
 * fdt.c
 */
int fdt_blacklist_memory(void *fdt);

/*
 * multiboot.c
 */
int check_multiboot_kernel(void *kbuf, size_t ksize);
size_t mb_mmap_desc_size(void);
int multiboot_set_runtime_pointers(run_addr_t *run_ebi);
int multiboot_init(void);
int multiboot_register(void);

/*
 * esxbootinfo.c
 */
int check_esxbootinfo_kernel(void *kbuf, size_t ksize);
int esxbootinfo_set_runtime_pointers(run_addr_t *run_ebi);
int esxbootinfo_init(void);
int esxbootinfo_register(void);
uint32_t esxbootinfo_arch_supported_req_flags(void);
bool esxbootinfo_arch_check_kernel(ESXBootInfo_Header *ebh);

/*
 * mboot supports both the Multiboot format and the homegrown ESXBootInfo
 * one. Let's abstract this here instead of special casing all the other
 * code.
 */

static INLINE size_t boot_mmap_desc_size(void)
{
   if (boot.is_esxbootinfo) {
      return 0;
   } else {
      return mb_mmap_desc_size();
   }
}

static INLINE int boot_set_runtime_pointers(run_addr_t *run_ebi)
{
   if (boot.is_esxbootinfo) {
      return esxbootinfo_set_runtime_pointers(run_ebi);
   } else {
      return multiboot_set_runtime_pointers(run_ebi);
   }
}

static INLINE int boot_init(void)
{
   if (boot.is_esxbootinfo) {
      return esxbootinfo_init();
   } else {
      return multiboot_init();
   }
}

static INLINE int boot_register(void)
{
   if (boot.is_esxbootinfo) {
      return esxbootinfo_register();
   } else {
      return multiboot_register();
   }
}


/*
 * gui.c
 */
int gui_init(void);
int gui_edit_kernel_options(void);
void gui_refresh(void);
void gui_set_title(const char *title);
int gui_resize(unsigned int width, unsigned int height, unsigned int depth,
               unsigned int min_width, unsigned int min_height,
               unsigned int min_depth);
int gui_text(void);
bool gui_exit(void);

#endif /* !MBOOT_H_ */
