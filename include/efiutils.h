/*******************************************************************************
 * Copyright (c) 2008-2016,2018-2019 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * efiutils.h -- EFI utils declarations
 */

#ifndef EFIUTILS_H_
#define EFIUTILS_H_

#include <compat.h>
#include <sys/types.h>
#include <syslog.h>
#include <stdbool.h>
#include <efi.h>

/*
 * Debugging
 */
#define EFI_ASSERT(_expr_)                                              \
   ASSERT_GENERIC((_expr_), efi_assert, "UEFI Lib error")

#define EFI_ASSERT_FIRMWARE(_expr_)                                     \
   ASSERT_GENERIC((_expr_), efi_assert, "UEFI firmware error")

#define EFI_ASSERT_PARAM(_expr_)                                        \
   ASSERT_GENERIC((_expr_), efi_assert, "invalid parameter")

#ifdef DEBUG
void efi_assert(const char *msg, ...);
#endif

/*
 * console.c
 */

/*-- efi_log -------------------------------------------------------------------
 *
 *      Log a message to the EFI native text console.  The message may or may
 *      not also be written to serial by the firmware.  If the console has been
 *      set to graphic mode (see set_display_mode), the message is not logged
 *      anywhere, not even to serial.
 *
 * Parameters
 *      IN _level_: log level
 *      IN ...:     printf-style arguments
 *
 * Results
 *      None.  Syntactically a statement, not a function.
 *----------------------------------------------------------------------------*/
#define efi_log(_level_, ...)                                     \
   do {                                                           \
      if (efi_log_callback != NULL) {                             \
         efi_log_callback((_level_), __VA_ARGS__);                \
      }                                                           \
   } while (0)

EXTERN void (*efi_log_callback)(int level, const char *, ...);

/*
 * error.c
 */
#undef EFI_ERROR

static INLINE BOOLEAN EFI_ERROR(EFI_STATUS Status)
{
   return (INTN)Status < 0;
}

EXTERN int error_efi_to_generic(EFI_STATUS Status);
EXTERN EFI_STATUS error_generic_to_efi(int err);

/*
 * guid.c
 */
EXTERN int efi_guid_cmp(EFI_GUID *guid1, EFI_GUID *guid2);

/*
 * protocol.c
 */
EXTERN EFI_GUID ComponentNameProto;
EXTERN EFI_GUID DriverBindingProto;

EXTERN EFI_STATUS get_protocol_interface(EFI_HANDLE Handle, EFI_GUID *Protocol,
                                         void **Interface);
EXTERN EFI_STATUS LocateHandleByProtocol(EFI_GUID *Protocol, UINTN *count,
                                         EFI_HANDLE **Handles);
EXTERN EFI_STATUS LocateProtocol(EFI_GUID *Protocol, void **Interface);

/*
 * init.c
 */
EXTERN EFI_SYSTEM_TABLE *st;
EXTERN EFI_BOOT_SERVICES *bs;
EXTERN EFI_RUNTIME_SERVICES *rs;
EXTERN EFI_HANDLE ImageHandle;
EXTERN EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE *acpi_spcr;

EXTERN EFI_STATUS efi_set_watchdog_timer(UINTN Timeout);

/*
 * devpath.c
 */
#define DevPathNodeLength(_Node_)                              \
   (((_Node_)->Length[0]) | ((_Node_)->Length[1] << 8))

#define IsDevPathEndType(_Node_)                               \
   (((_Node_)->Type) == END_DEVICE_PATH_TYPE)

#define IsDevPathEnd(_Node_)                                   \
   (IsDevPathEndType(_Node_) &&                                \
    (_Node_)->SubType == END_ENTIRE_DEVICE_PATH_SUBTYPE)

#define NextDevPathNode(a)                                     \
   ((EFI_DEVICE_PATH_PROTOCOL *)(((UINT8 *)(a)) + DevPathNodeLength(a)))

static INLINE void SetDevPathNodeLength(EFI_DEVICE_PATH *Node, UINTN Len)
{
   Node->Length[0] = (UINT8)Len;
   Node->Length[1] = (UINT8)(Len >> 8);
}

static INLINE void SetDevPathEndNode(EFI_DEVICE_PATH *Node)
{
   Node->Type = END_DEVICE_PATH_TYPE;
   Node->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
   Node->Length[0] = sizeof (EFI_DEVICE_PATH);
   Node->Length[1] = 0;
}

#define IS_PATH_SEPARATOR(_c_) ((_c_) == L'\\' || (_c_) == L'/')

#define FOREACH_DEVPATH_NODE(_DevPath_, _Node_)                \
   for ((_Node_) = (_DevPath_);                                \
        !IsDevPathEnd((_Node_));                               \
        (_Node_) = NextDevPathNode((_Node_)))

#define FOREACH_FILEPATH_NODE(_DevPath_, _Node_)               \
   for ((_Node_) = (FILEPATH_DEVICE_PATH *)(_DevPath_);        \
        (_Node_)->Header.Type == MEDIA_DEVICE_PATH &&          \
        (_Node_)->Header.SubType == MEDIA_FILEPATH_DP;         \
        (_Node_) = (FILEPATH_DEVICE_PATH *)NextDevPathNode(&((_Node_)->Header)))

EXTERN EFI_STATUS devpath_get(EFI_HANDLE Handle, EFI_DEVICE_PATH **DevPath);
EXTERN EFI_STATUS devpath_handle(EFI_DEVICE_PATH *DevPath, EFI_HANDLE *Handle);
EXTERN EFI_STATUS file_devpath(EFI_HANDLE Device, const CHAR16 *FileName,
                               EFI_DEVICE_PATH **FileDevPath);
EXTERN EFI_STATUS devpath_get_filepath(const EFI_DEVICE_PATH *DevPath,
                                       CHAR16 **FilePath);
EXTERN EFI_STATUS devpath_duplicate(const EFI_DEVICE_PATH *DevPath,
                                    EFI_DEVICE_PATH **Dup);
EXTERN bool devpath_is_parent(const EFI_DEVICE_PATH *parent,
                              const EFI_DEVICE_PATH *child);
EXTERN char *devpath_text(const EFI_DEVICE_PATH *DevPath,
                          bool displayOnly, bool allowShortcuts);

/*
 * volume.c
 */
EXTERN EFI_GUID BlockIoProto;
EXTERN EFI_GUID DiskIoProto;

EXTERN EFI_STATUS get_boot_volume(EFI_HANDLE *Volume);
EXTERN EFI_STATUS get_boot_device(EFI_HANDLE *device);

/*
 * memory.c
 */
EXTERN VOID *efi_malloc(UINTN size);
EXTERN VOID *efi_calloc(UINTN nmemb, UINTN size);
EXTERN VOID *efi_realloc(VOID *ptr, UINTN oldsize, UINTN newsize);
EXTERN VOID efi_free(VOID *ptr);

/*
 * simplefile.c
 */
EXTERN EFI_GUID SimpleFileSystemProto;
EXTERN EFI_GUID GenericFileInfoId;
EXTERN EFI_GUID FileSystemInfoId;
EXTERN EFI_GUID FileSystemVolumeLabelInfoId;

EXTERN EFI_STATUS simple_file_get_volume_info(EFI_HANDLE Volume,
                                              EFI_FILE_SYSTEM_INFO **Info);
EXTERN EFI_STATUS simple_file_get_size(EFI_HANDLE Volume,
                                       const CHAR16 *FilePath, UINTN *FileSize);
EXTERN EFI_STATUS simple_file_load(EFI_HANDLE Volume, const CHAR16 *FilePath,
                                   int (*callback)(size_t), VOID **Buffer,
                                   UINTN *BufSize);
EFI_STATUS simple_file_overwrite(EFI_HANDLE Volume, const CHAR16 *FilePath,
                                 VOID *Buffer, UINTN BufSize);

/*
 * gpxefile.c
 */
EXTERN EFI_STATUS gpxe_file_load(EFI_HANDLE Volume, const char *filepath,
                                 int (*callback)(size_t), VOID **Buffer,
                                 UINTN *BufSize);
EXTERN bool has_gpxe_download_proto(EFI_HANDLE Volume);

/*
 * loadfile.c
 */
EXTERN EFI_STATUS load_file_get_size(EFI_HANDLE Volume, const CHAR16 *FilePath,
                                     UINTN *FileSize);
EXTERN EFI_STATUS load_file_load(EFI_HANDLE Volume, const CHAR16 *FilePath,
                                 int (*callback)(size_t), VOID **Buffer,
                                 UINTN *BufSize);

/*
 * tftpfile.c
 */
EXTERN EFI_STATUS tftp_file_get_size(EFI_HANDLE Volume, const char *filepath,
                                     UINTN *FileSize);
EXTERN EFI_STATUS tftp_file_load(EFI_HANDLE Volume, const char *filepath,
                                 int (*callback)(size_t), VOID **Buffer,
                                 UINTN *BufSize);
EXTERN EFI_STATUS get_pxe_boot_file(EFI_PXE_BASE_CODE *Pxe, CHAR16 **BootFile);
EXTERN bool is_pxe_boot(EFI_PXE_BASE_CODE **Pxe);

/*
 * file.c
 */
EXTERN EFI_STATUS efi_file_read(EFI_HANDLE Volume, const CHAR16 *FilePath,
                                int (*callback)(size_t), VOID **Buffer,
                                UINTN *BufLen);

/*
 * image.c
 */
EXTERN EFI_STATUS image_get_info(EFI_HANDLE Handle, EFI_LOADED_IMAGE **Image);
EXTERN EFI_STATUS image_load(EFI_HANDLE Volume, const CHAR16 *FilePath,
                             VOID *OptBuf, UINT32 OptSize,
                             EFI_HANDLE *DrvHandle, EFI_STATUS *RetVal);

/*
 * ucs2.c
 */
#define UCS2SIZE(_Str_) ((ucs2_strlen((_Str_)) + 1) * sizeof (CHAR16))

EXTERN size_t ucs2_strlen(const CHAR16 *Str);
EXTERN size_t ucs2_strnlen(const CHAR16 *Str, size_t maxlen);
EXTERN CHAR16 *ucs2_strcpy(CHAR16 *Dest, const CHAR16 *Src);
EXTERN EFI_STATUS ucs2_to_ascii(const CHAR16 *Src, char **dest, bool strict);
EXTERN EFI_STATUS ascii_to_ucs2(const char *src, CHAR16 **Dest);
EXTERN EFI_STATUS ucs2_alloc(size_t length, CHAR16 **Str);
EXTERN EFI_STATUS ucs2_strdup(const CHAR16 *Str, CHAR16 **Duplicate);
EXTERN EFI_STATUS argv_to_ucs2(int argc, char **argv, CHAR16 **ArgStr);
EXTERN CHAR16 ucs2_toupper(CHAR16 C);
EXTERN int ucs2_strcmp(const CHAR16 *Str1, const CHAR16 *Str2);

#endif /* !EFIUTILS_H_ */
