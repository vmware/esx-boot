/*******************************************************************************
 * Copyright (c) 2008-2016,2019 VMware, Inc.  All rights reserved.
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
EXTERN int exit_boot_services(size_t desc_extra_mem, e820_range_t **mmap,
                              size_t *count, efi_info_t *efi_info);
EXTERN int get_memory_map(size_t desc_extra_mem, e820_range_t **mmap,
                          size_t *count, efi_info_t *efi_info);
EXTERN void log_memory_map(efi_info_t *efi_info);
EXTERN void free_memory_map(e820_range_t *mmap, efi_info_t *efi_info);
EXTERN int relocate_runtime_services(efi_info_t *efi_info, bool no_rts, bool no_quirks);

/*
 * System information
 */
EXTERN int get_acpi_rsdp(void **rsdp);
EXTERN int get_smbios_eps(void **eps_start);
EXTERN int get_smbios_v3_eps(void **eps_start);

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

EXTERN int get_boot_file(char **buffer);
EXTERN int get_boot_dir(char **buffer);
EXTERN int firmware_file_get_size_hint(const char *filepath, size_t *size);
EXTERN int firmware_file_read(const char *filepath, int (*callback)(size_t),
                              void **buffer, size_t *buflen);
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
 * Logging
 */
EXTERN void set_firmware_log_callback(void (*callback)(int, const char *, ...));
EXTERN int firmware_print(const char *str);

/*
 * Serial
 */
typedef enum {
   SERIAL_NS16550,
   SERIAL_PL011,
   SERIAL_LAST = SERIAL_PL011,
} serial_type_t;
#define SERIAL_BAUDRATE_UNKNOWN 0
EXTERN int get_serial_port(int com, serial_type_t *type,
                           io_channel_t *channel, uint32_t *current_baudrate);

/*
 * Misc
 */
typedef enum {
   DISPLAY_MODE_NATIVE_TEXT,
   DISPLAY_MODE_VBE
} display_mode_t;

EXTERN int set_display_mode(display_mode_t mode);

EXTERN bool secure_boot_mode(void);
EXTERN int secure_boot_check(void);

EXTERN void check_efi_quirks(efi_info_t *efi_info);
EXTERN int relocate_page_tables2(void);

#endif /* !BOOT_SERVICES_H_ */
