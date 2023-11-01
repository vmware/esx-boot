/*******************************************************************************
 * Copyright (c) 2008-2016,2019-2023 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * boot_services.h -- Low level (platform-dependent) boot services API
 */

#ifndef BOOT_SERVICES_H_
#define BOOT_SERVICES_H_

#include <e820.h>
#include <vbe.h>
#include <efi_info.h>
#include <disk.h>
#include <io.h>

EXTERN char __executable_start[];  /* Beginning of the binary image */
EXTERN char _end[];                /* End of the binary image */
EXTERN char _etext[];              /* End of the .text segment */

/*
 * Firmware
 */
enum firmware_interface_t {
   FIRMWARE_INTERFACE_EFI,
   FIRMWARE_INTERFACE_COM32
};

typedef union {
   struct {
      uint16_t major;
      uint16_t minor;
   } efi;
   struct {
      uint8_t major;
      uint8_t minor;
   } com32;
} firmware_version_t;

typedef struct {
   enum firmware_interface_t interface;
   firmware_version_t version;
   char *vendor;
   uint32_t revision;
} firmware_t;

EXTERN int get_firmware_info(firmware_t *firmware);
EXTERN int chainload_parent(const char *cmdline);
EXTERN bool in_boot_services(void);
EXTERN int exit_boot_services(size_t desc_extra_mem, e820_range_t **mmap,
                              size_t *count, efi_info_t *efi_info);
EXTERN int get_memory_map(size_t desc_extra_mem, e820_range_t **mmap,
                          size_t *count, efi_info_t *efi_info);
EXTERN void log_memory_map(efi_info_t *efi_info);
EXTERN void free_memory_map(e820_range_t *mmap, efi_info_t *efi_info);
EXTERN int relocate_runtime_services(efi_info_t *efi_info, bool no_rts, bool no_quirks);
EXTERN void firmware_reset_watchdog(void);

/*
 * System information
 */
EXTERN int get_acpi_rsdp(void **rsdp);
EXTERN int get_smbios_eps(void **eps_start);
EXTERN int get_smbios_v3_eps(void **eps_start);
EXTERN int get_fdt(void **fdt_start);
EXTERN int get_tcg2_final_events(void **final_events_start);

/*
 * Memory allocation
 */
EXTERN void *sys_malloc(size_t size);
EXTERN void *sys_realloc(void *ptr, size_t oldsize, size_t newsize);
EXTERN void sys_free(void *ptr);

/*
 * Network
 */
EXTERN bool is_network_boot(void);
EXTERN int get_bootif_option(const char **bootif);

/*
 * File system
 */
#define READ_CHUNK_SIZE (1024 * 1024)
#define WRITE_CHUNK_SIZE (1024 * 1024)

EXTERN int get_boot_file(char **buffer);
EXTERN int get_boot_dir(char **buffer);
EXTERN int firmware_file_get_size_hint(const char *filepath, size_t *size);
EXTERN int firmware_file_read(const char *filepath, int (*callback)(size_t),
                              void **buffer, size_t *buflen);
EXTERN int firmware_file_write(const char *filepath, int (*callback)(size_t),
                               void *buffer, size_t buflen);
EXTERN int firmware_file_exec(const char *filepath, const char *options);

/*
 *  Timer
 */
EXTERN uint64_t firmware_get_time_ms(bool consider_timer_overflow);

/*
 * Block devices
 */
EXTERN int get_boot_disk(disk_t *disk);
EXTERN int disk_read(const disk_t *disk, void *buf, uint64_t lba,
                     size_t count);
EXTERN int disk_write(const disk_t *disk, void *buf, uint64_t lba,
                      size_t count);

/*
 * VESA BIOS Extension (VBE)
 */
EXTERN int vbe_get_info(vbe_t *vbe, vbe_mode_id_t **modes);
EXTERN int vbe_get_current_mode(vbe_mode_id_t *id);
EXTERN int vbe_get_mode_info(vbe_mode_id_t id, vbe_mode_t *mode, uintptr_t *fb_addr);
EXTERN int vbe_set_mode(vbe_mode_id_t id);
EXTERN int vbe_force_vga_text(vbe_mode_id_t *id, vbe_mode_t *mode);

/*
 * Keyboard input
 */
typedef enum {
   KEYSYM_NONE,   /* For reporting that no key was pressed */
   KEYSYM_UP,
   KEYSYM_DOWN,
   KEYSYM_RIGHT,
   KEYSYM_LEFT,
   KEYSYM_HOME,
   KEYSYM_END,
   KEYSYM_INSERT,
   KEYSYM_PAGE_UP,
   KEYSYM_PAGE_DOWN,
   KEYSYM_F1,
   KEYSYM_F2,
   KEYSYM_F3,
   KEYSYM_F4,
   KEYSYM_F5,
   KEYSYM_F6,
   KEYSYM_F7,
   KEYSYM_F8,
   KEYSYM_F9,
   KEYSYM_F10,
   KEYSYM_F11,
   KEYSYM_F12,
   KEYSYM_ASCII  /* For keys that represent an ASCII character */
} key_sym_t;

typedef struct {
   key_sym_t sym;
   char ascii;
} key_code_t;

EXTERN int kbd_waitkey(key_code_t *key);
EXTERN int kbd_waitkey_timeout(key_code_t *key, uint16_t nsec);
EXTERN int kbd_init(void);

/*
 * TPM
 */
#ifdef __COM32__
static INLINE void
tpm_init(void)
{}
#else
EXTERN void tpm_init(void);
#endif

typedef struct {
   const uint8_t *address;
   uint32_t size;
   bool truncated;
} tpm_event_log_t;

#ifdef __COM32__
static INLINE int
tpm_get_event_log(tpm_event_log_t *log)
{
   log->address = NULL;
   log->size = 0;
   log->truncated = false;
   return ERR_NOT_FOUND;
}
#else
EXTERN int tpm_get_event_log(tpm_event_log_t *log);
#endif

#ifdef __COM32__
static INLINE int
tpm_extend_module(const char *filename, const void *addr, size_t size)
{
   (void)filename;
   (void)addr;
   (void)size;
   return ERR_SUCCESS; // No-op
}
#else
EXTERN int tpm_extend_module(const char *filename, const void *addr,
                             size_t size);
#endif

#ifdef __COM32__
static INLINE int
tpm_extend_signer(const unsigned char *certData, uint16_t certLength)
{
   (void)certData;
   (void)certLength;
   return ERR_SUCCESS; // No-op
}
#else
EXTERN int tpm_extend_signer(const unsigned char *certData,
                             uint16_t certLength);
#endif

#ifdef __COM32__
static INLINE int
tpm_extend_cmdline(const char *filename, const char *options)
{
   (void)filename;
   (void)options;
   return ERR_SUCCESS; // No-op
}
#else
EXTERN int tpm_extend_cmdline(const char *filename, const char *cmdline);
#endif

#ifdef __COM32__
static INLINE int
tpm_extend_asset_tag(void)
{
   return ERR_SUCCESS; // No-op
}
#else
EXTERN int tpm_extend_asset_tag(void);
#endif

/*
 * runtime_watchdog.c
 */
#ifdef __COM32__
static INLINE int
set_runtime_watchdog(unsigned int timeout)
{
   (void)timeout;
   return ERR_SUCCESS; // No-op
}

static INLINE int
dump_runtime_watchdog(unsigned int *minTimeoutSec,
                      unsigned int *maxTimeoutSec,
                      int *watchdogType,
                      uint64_t *baseAddr)
{
   (void)minTimeoutSec;
   (void)maxTimeoutSec;
   (void)watchdogType;
   (void)baseAddr;
   return ERR_SUCCESS; //No-op
}

static INLINE int
init_runtime_watchdog(void)
{
   return ERR_SUCCESS; //No-op
}
#else
EXTERN int set_runtime_watchdog(unsigned int timeout);
EXTERN void dump_runtime_watchdog(unsigned int *minTimeoutSec,
                                 unsigned int *maxTimeoutSec,
                                 int *watchdogType,
                                 uint64_t *baseAddr);
EXTERN int init_runtime_watchdog(void);
#endif

/*
 * Logging
 */
EXTERN void set_firmware_log_callback(void (*callback)(int, const char *, ...));
EXTERN int firmware_print(const char *str);

/*
 * Serial
 */
#define SERIAL_TYPE_DEFS                       \
   SERIAL_TYPE_DEF(NS16550)                    \
   SERIAL_TYPE_DEF(PL011)                      \
   SERIAL_TYPE_DEF(TMFIFO)                     \
   SERIAL_TYPE_DEF(AAPL_S5L)

typedef enum {
#define SERIAL_TYPE_DEF(x) SERIAL_##x,
   SERIAL_TYPE_DEFS
#undef SERIAL_TYPE_DEF
   SERIAL_COUNT,
} serial_type_t;
#define SERIAL_BAUDRATE_UNKNOWN 0
EXTERN int get_serial_port(int com, serial_type_t *type,
                           io_channel_t *channel, uint32_t *current_baudrate);

/*
 * ACPI
 */
#ifdef __COM32__
static INLINE void
firmware_init_acpi_table(void)
{
}

static INLINE int
firmware_install_acpi_table(void *buffer, size_t size, unsigned int *key)
{
   (void)buffer;
   (void)size;
   (void)key;
   return ERR_UNSUPPORTED;
}

static INLINE int
firmware_uninstall_acpi_table(unsigned int key)
{
   (void)key;
   return ERR_UNSUPPORTED;
}
#else
extern void firmware_init_acpi_table(void);
EXTERN int firmware_install_acpi_table(void *buffer, size_t size,
                                       unsigned int *key);
EXTERN int firmware_uninstall_acpi_table(unsigned int key);
#endif

/*
 * Misc
 */
EXTERN int set_graphic_mode(void);

EXTERN bool secure_boot_mode(void);
EXTERN int secure_boot_check(bool crypto_module);

EXTERN void check_efi_quirks(efi_info_t *efi_info);
EXTERN int relocate_page_tables2(void);

typedef enum {
   http_never = 0,
   http_if_http_booted = 1,
   http_if_plain_http_allowed = 2,
   http_always = 3,
} http_criteria_t;

#ifdef __COM32__
#   define set_http_criteria(mode)
#   define tftp_set_block_size(size)
#else
EXTERN void set_http_criteria(http_criteria_t mode);
EXTERN void tftp_set_block_size(size_t blksize);
#endif

/*
 * Log buffer UEFI protocol interfaces.
 */
#ifndef __COM32__
struct syslogbuffer;
EXTERN int logbuf_proto_init(struct syslogbuffer *syslogbuf);
EXTERN int logbuf_proto_get(struct syslogbuffer **syslogbuf);
#endif

#endif /* !BOOT_SERVICES_H_ */
