/*******************************************************************************
 * Copyright (c) 2008-2016,2018-2019 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * bootlib.h -- High level (platform independent) boot utils routines
 */

#ifndef BOOTLIB_H_
#define BOOTLIB_H_

#include <compat.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <fb.h>
#include <syslog.h>
#include <e820.h>
#include <disk.h>
#include <mbr.h>
#include <md5.h>
#include <cpu.h>
#include <sm_bios.h>
#include <acpi.h>

#define ceil(x,y)                (((x) + (y) - 1) / (y))
#define MILLISECS_IN_ONE_SEC     1000
#define MICROSECS_IN_ONE_SEC     1000000
#define SECS_PER_HOUR            3600
#define SECS_PER_DAY             (24 * SECS_PER_HOUR)

#define IS_LEAP_YEAR(yr) \
   (((yr) % 4) == 0 && (((yr) % 100) != 0 || ((yr) % 400) == 0))

#define MILLISEC_TO_SEC_SIGNIFICAND(time) \
   ((time) / MILLISECS_IN_ONE_SEC)
#define MILLISEC_TO_SEC_FRACTIONAL(time) \
   (((time) % MILLISECS_IN_ONE_SEC) / 100)

#define BYTES_TO_KB(_bytes_)     ((_bytes_) >> 10)
#define BYTES_TO_MB(_bytes_)     ((_bytes_) >> 20)
#define BYTES_TO_GB(_bytes_)     ((_bytes_) >> 30)

#define FAKE_ARGV0               "#"

typedef struct {
   mbr_part_t info;
   int id;
} partition_t;

/*
 * alloc.c
 */

#define ALLOC_32BIT     0
#define ALLOC_FIXED     1
#define ALLOC_FORCE     2
#if only_ia32
#define ALLOC_ANY       ALLOC_32BIT
#else
#define ALLOC_ANY       3
#endif

#define ALIGN_ANY       1
#define ALIGN_STR       1
#define ALIGN_PAGE      PAGE_SIZE
#define ALIGN_FUNC      16
#define ALIGN_PTR       (sizeof (void *))

#define MAX_32_BIT_ADDR 0xffffffffULL
#define MAX_64_BIT_ADDR 0xffffffffffffffffULL

#define PAGE_ADDR(_addr_)      ((_addr_) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(_addr_)  PAGE_ADDR((_addr_) + PAGE_SIZE - 1)

void alloc_sanity_check(void);
int alloc(uint64_t *addr, uint64_t size, size_t align, int option);

#define runtime_alloc_fixed(_addr_, _size_)                          \
   alloc((_addr_), (_size_), ALIGN_ANY, ALLOC_FIXED)

#define runtime_alloc(_addr_, _size_, _align_, _alloc_)              \
   alloc((_addr_), (_size_), (_align_), (_alloc_))

/*-- blacklist_runtime_mem ----------------------------------------------------
 *
 *      Reserve an explicit range of memory so it will not be allocated later
 *      for run-time memory.
 *
 * Parameters
 *      IN addr: memory range starting address
 *      IN size: memory range size
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static INLINE int blacklist_runtime_mem(uint64_t addr, uint64_t size)
{
   uint64_t page_start;

   page_start = PAGE_ADDR(addr);
   size = PAGE_ALIGN_UP(size + (addr - page_start));

   return alloc(&page_start, size, ALIGN_ANY, ALLOC_FORCE);
}

/*
 * e820.c
 */

EXTERN void e820_mmap_merge(e820_range_t *mmap, size_t *count);
EXTERN bool is_mergeable(uint64_t a1, uint64_t l1, uint64_t a2, uint64_t l2);
EXTERN bool is_overlap(uint64_t a1, uint64_t l1, uint64_t a2, uint64_t l2);
EXTERN int e820_to_blacklist(e820_range_t *mmap, size_t count);

/*
 * string.c
 */
static INLINE bool is_absolute(const char *filepath)
{
   return (filepath[0] == '/' || filepath[0] == '\\' ||
           strstr(filepath, "://") != NULL);
}

EXTERN int str_alloc(size_t length, char **str);
EXTERN void mem_swap(void *m1, void *m2, size_t n);
EXTERN char *mem_strcasestr(const void *src, const char *str, size_t n);
EXTERN char *str_merge_spaces(char *str);
EXTERN int str_to_argv(char *cmdline, int *argc, char ***argv);
EXTERN int argv_to_str(int argc, char **argv, char **s);
EXTERN bool is_number(const char *str);
EXTERN int make_path(const char *root, const char *filepath, char **buffer);
EXTERN int insert_char(char *strbuf, char c, size_t n);
EXTERN int delete_char(char *str, size_t n);

/*
 * gzip.c
 */
EXTERN bool is_gzip(const void *buffer, size_t size, int *status);
EXTERN int gzip_extract(const void *src, size_t src_size, void **dest,
                        size_t *dest_size);

/*
 * file.c
 */
#define MAX_PATH_LEN 2048

EXTERN int file_get_size_hint(int volid, const char *filename,
                              size_t *filesize);
EXTERN int file_load(int volid, const char *filename, int (*callback)(size_t),
                     void **buffer, size_t *bufsize);
EXTERN int file_overwrite(int volid, const char *filepath, void *buffer,
                          size_t size);
EXTERN int file_sanitize_path(char *filepath);

/*
 * net.c
 */
EXTERN int get_mac_address(const char **mac);

/*
 * volume.c
 */
#define FIRMWARE_BOOT_VOLUME 0
EXTERN int get_max_volume(disk_t *disk, int *max);
EXTERN int volume_read(int volid, void *dest, uint64_t offset, size_t size);
EXTERN int get_volume_info(disk_t *disk, int part_id, partition_t *partition);

/*
 * mbr.c
 */
EXTERN int mbr_get_max_part(disk_t *disk, char *mbr, int *max);
EXTERN int mbr_get_part_info(disk_t *disk, char *mbr, int part_id,
                             partition_t *partition);

/*
 * gpt.c
 */
EXTERN int gpt_get_max_part(disk_t *disk, int *max);
EXTERN int gpt_get_part_info(disk_t *disk, int part_id, partition_t *partition);

/*
 * log.c
 */
#define IS_SYSLOG_LEVEL(_level_)                                        \
   ((_level_) >= 0 && (_level_) <= LOG_DEBUG)

#define IS_SYSLOG_LEVEL_STR(_level_) IS_SYSLOG_LEVEL((_level_) - '0')

static INLINE bool is_syslog_message(const char *str)
{
   return str[0] == '<' && IS_SYSLOG_LEVEL_STR(str[1]) && str[2] == '>';
}

typedef int (*log_callback_t)(const char *msg);

EXTERN void Log(int level, const char *fmt, ...)
   __attribute__ ((format (__printf__, 2, 3)));

#ifdef DEBUG
#define LOG(level, ...) Log(level, ## __VA_ARGS__)
#else
#define LOG(level, ...)
#endif /* DEBUG */

EXTERN int log_subscribe(log_callback_t callback, int maxlevel);
EXTERN void log_unsubscribe(log_callback_t callback);
EXTERN const char *log_buffer_addr(void);
EXTERN int log_init(bool verbose);
EXTERN int syslog_get_message_level(const char *msg, int *level);

/*
 * Serial port
 */
EXTERN int serial_log_init(int com, uint32_t baudrate);

/*
 * sort.c
 */
EXTERN void bubble_sort(void *base, size_t nmemb, size_t size,
                        int (*compar)(const void *, const void *));

/*
 * fbcon.c
 */
EXTERN int fbcon_init(framebuffer_t *fbinfo, font_t *cons_font, int x, int y,
                      unsigned int width, unsigned int height, bool verbose);
EXTERN void fbcon_reset(void);
EXTERN void fbcon_clear(void);
EXTERN void fbcon_shutdown(void);
EXTERN int fbcon_set_verbosity(bool verbose);

/*
 * parse.c
 */
typedef enum {
   OPT_STRING,
   OPT_INTEGER,
   OPT_INVAL
} option_type_t;

typedef union {
   char *str;              /* For storing an OPT_STRING option */
   int integer;            /* For storing an OPT_INTEGER option */
} option_value_t;

typedef struct {
   const char *key;
   const char *separator;
   option_value_t value;
   option_type_t type;
} option_t;

EXTERN int parse_config_file(int volid, const char *filename,
                             option_t *options);

/*
 * video.c
 */
typedef struct {
   vbe_t controller;            /* VBE controller info */
   vbe_mode_t mode;             /* Current VBE mode info */
   vbe_mode_id_t *modes_list;   /* Supported VBE modes list */
   vbe_mode_id_t current_mode;  /* Current VBE mode ID */
   uintptr_t fb_addr;           /* Current mode framebuffer address */
} vbe_info_t;

EXTERN int video_check_support(void);
EXTERN int video_get_vbe_info(vbe_info_t *vbe_info);
EXTERN int video_set_mode(framebuffer_t *fb, unsigned int width,
                          unsigned int height, unsigned int depth,
                          unsigned int min_width, unsigned int min_height,
                          unsigned int min_depth, bool debug);
EXTERN int video_set_text_mode(void);

/*
 * acpi.c
 */
EXTERN void acpi_init(void);
EXTERN int acpi_is_present(void);
EXTERN acpi_sdt *acpi_find_sdt(const char *sig);

/*
 * smbios.c
 */
EXTERN int smbios_get_info(void **eps_start, size_t *eps_length,
                           void **table_start, size_t *table_length);
EXTERN int smbios_get_v3_info(void **eps_start, size_t *eps_length,
                              void **table_start, size_t *table_length);
EXTERN int smbios_get_struct(smbios_entry ptr, smbios_entry end,
                             uint8_t type, smbios_entry *entry);
EXTERN char *smbios_get_string(smbios_entry ptr, smbios_entry end,
                               unsigned index);
EXTERN int smbios_get_platform_info(const char **manufacturer,
                                    const char **product,
                                    const char **bios_ver,
                                    const char **bios_date);

/*-- is_valid_firmware_table ---------------------------------------------------
 *
 *      Firmware tables sanity check: all bytes in the table must add up to
 *      zero. This is used for validating ACPI/SMBIOS checksums.
 *
 * Parameters
 *      IN base: pointer to the table
 *      IN size: table size, in bytes
 *
 * Results
 *      True if the table is valid, false otherwise.
 *----------------------------------------------------------------------------*/
static INLINE bool is_valid_firmware_table(void *base, size_t size)
{
   uint8_t *p;
   uint8_t checksum;

   p = base;
   checksum = 0;

   while (size--) {
      checksum = checksum + *p;
      p++;
   }

   return !(checksum > 0);
}

/*
 * error.c
 */

EXTERN const char *error_str[];

/*
 * Utilities
 */

/*-- roundup64 ----------------------------------------------------------------
 *
 *      Round n up to the nearest multiple of unit.
 *
 * Parameters
 *      IN n:    input value
 *      IN unit: round up to a multiple of this much
 *
 * Results
 *      The rounded value.
 *----------------------------------------------------------------------------*/
static INLINE uint64_t roundup64(uint64_t n, uint64_t unit)
{
   uint64_t u;

   u = n / unit;
   if (n % unit > 0) {
      u++;
   }

   return u * unit;
}

#define SANITIZE_STRP(x) (x = (x == NULL) ? "" : x)

/*
 *  For casting safely between 32-bit and 64-bit values
 */
#define PTR_TO_UINT(_ptr_)    ((uintptr_t)(_ptr_))
#define PTR_TO_UINT64(_ptr_)  ((uint64_t)PTR_TO_UINT(_ptr_))
#define UINT_TO_PTR(_ui_)     ((void *)(uintptr_t)(_ui_))
#define UINT32_TO_PTR(_ui32_) UINT_TO_PTR(_ui32_)

#define IS_UINT32(_ui32_)     (((uint64_t)(_ui32_) & 0xffffffff00000000) == 0)
#define IS_PTR32(_ptr_)       IS_UINT32(PTR_TO_UINT(_ptr_))
#define IS_REGION32(_ptr_, _size_)                                \
  (IS_PTR32(_ptr_) && IS_UINT32(PTR_TO_UINT(_ptr_) + (_size_)))

#define PTR_TO_UINT32(_ptr_)  ((uint32_t)PTR_TO_UINT(_ptr_))
#define UINT64_TO_PTR(_ui64_) UINT_TO_PTR(_ui64_)

#endif /* !BOOTLIB_H_ */
