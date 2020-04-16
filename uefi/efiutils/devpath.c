/*******************************************************************************
 * Copyright (c) 2008-2011,2016,2019 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * devpath.c -- EFI device path handling routines
 */

#include <string.h>
#include "efi_private.h"
#include "DevicePathToText.h"

static EFI_GUID DevicePathProto = DEVICE_PATH_PROTOCOL;
static EFI_GUID DevicePathToTextProto = EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;

/*-- devpath_get ---------------------------------------------------------------
 *
 *      Get the device path of a given handle.
 *
 * Parameters
 *      IN  Handle:  handle to query
 *      OUT DevPath: pointer to the device path
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS devpath_get(EFI_HANDLE Handle, EFI_DEVICE_PATH **DevPath)
{
   return get_protocol_interface(Handle, &DevicePathProto, (void **)DevPath);
}

/*-- devpath_handle ------------------------------------------------------------
 *
 *      Locate the handle pointed to by the given device path.
 *
 * Parameters
 *      IN  DevPath: pointer to the device path
 *      OUT Handle:  the handle
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS devpath_handle(EFI_DEVICE_PATH *DevPath, EFI_HANDLE *Handle)
{
   EFI_DEVICE_PATH *Path = DevPath;
   EFI_HANDLE Hdl;
   EFI_STATUS Status;

   EFI_ASSERT(bs != NULL);
   EFI_ASSERT_FIRMWARE(bs->LocateDevicePath != NULL);

   Status = bs->LocateDevicePath(&DevicePathProto, &Path, &Hdl);

   if (!EFI_ERROR(Status)) {
      *Handle = Hdl;
   }

   return Status;
}

/*-- devpath_size --------------------------------------------------------------
 *
 *      Return the size of a device path.
 *
 * Parameters
 *      IN  DevPath:   pointer to the device path to query
 *      OUT size:      the size, in bytes (optional)
 *      OUT instances: number of instances in the device path (optional)
 *----------------------------------------------------------------------------*/
static void devpath_size(const EFI_DEVICE_PATH *DevPath, size_t *size,
                         unsigned int *instances)
{
   const EFI_DEVICE_PATH *node;
   unsigned int i;

   EFI_ASSERT_PARAM(DevPath != NULL);

   i = 1;
   FOREACH_DEVPATH_NODE(DevPath, node) {
      if (IsDevPathEndType(node)) {
         i++;
      }
   }

   if (size != NULL) {
      *size = (char *)node - (char *)DevPath + sizeof (EFI_DEVICE_PATH);
   }
   if (instances != NULL) {
      *instances = i;
   }
}

/*-- devpath_instance_size -----------------------------------------------------
 *
 *      Return the size of a device path instance, not including the terminating
 *      node.
 *
 * Parameters
 *      IN  instances: pointer to the device path instance
 *      OUT size:      the size, in bytes
 *----------------------------------------------------------------------------*/
static void devpath_instance_size(const EFI_DEVICE_PATH *instance, size_t *size)
{
   const EFI_DEVICE_PATH *node;

   EFI_ASSERT_PARAM(instance != NULL);
   EFI_ASSERT_PARAM(size != NULL);

   FOREACH_DEVPATH_NODE(instance, node) {
      if (IsDevPathEndType(node)) {
         break;
      }
   }

   *size = (char *)node - (char *)instance;
}

/*-- devpath_append ------------------------------------------------------------
 *
 *      Append a device path to every instance of another device path. The
 *      device path to append may not have several instances.
 *
 * Parameters
 *      IN  Multi:   pointer to the device path to be appended
 *      IN  Single:  pointer to the device path to append
 *      OUT DevPath: pointer to the freshly allocated resulting device path
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS devpath_append(const EFI_DEVICE_PATH *Multi,
                                 const EFI_DEVICE_PATH *Single,
                                 EFI_DEVICE_PATH **DevPath)
{
   EFI_DEVICE_PATH *p, *Path;
   unsigned int instances;
   size_t s1, s2;

   EFI_ASSERT_PARAM(Multi != NULL);
   EFI_ASSERT_PARAM(Single != NULL);
   EFI_ASSERT_PARAM(DevPath != NULL);

   devpath_size(Multi, &s1, &instances);
   devpath_instance_size(Single, &s2);

   Path = sys_malloc(s1 + instances * s2);
   if (Path == NULL) {
      return EFI_OUT_OF_RESOURCES;
   }

   for (p = Path; instances > 0; instances--) {
      devpath_instance_size(Multi, &s1);

      memcpy(p, Multi, s1);
      p = (EFI_DEVICE_PATH *)((char *)p + s1);

      memcpy(p, Single, s2);
      p = (EFI_DEVICE_PATH *)((char *)p + s2);

      if (instances > 1) {
         p->Type = END_DEVICE_PATH_TYPE;
         p->SubType = END_INSTANCE_DEVICE_PATH_SUBTYPE;
         SetDevPathNodeLength(p, sizeof (EFI_DEVICE_PATH));

         p++;
         Multi = (EFI_DEVICE_PATH *)((char *)Multi + s1 + sizeof (EFI_DEVICE_PATH));
      } else {
         SetDevPathEndNode(p);
      }
   }

   *DevPath = Path;

   return EFI_SUCCESS;
}

/*-- make_file_devpath ---------------------------------------------------------
 *
 *      Convert a file path string to a device path of type MEDIA_FILEPATH_DP.
 *      The output device path is typically appended to a volume device path to
 *      get an absolute device path to a file.
 *
 * Parameters
 *      IN  PathName: string containing a file path
 *      OUT DevPath:  pointer to the freshly allocated file path device path
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS make_file_devpath(const CHAR16 *PathName,
                                    EFI_DEVICE_PATH **DevPath)
{
   UINTN                   Size;
   FILEPATH_DEVICE_PATH    *FilePath;
   EFI_DEVICE_PATH         *Eop;

   Size = sizeof (FilePath->Header) + UCS2SIZE(PathName);

   FilePath = sys_malloc(Size + sizeof (EFI_DEVICE_PATH));
   if (FilePath == NULL) {
      return EFI_OUT_OF_RESOURCES;
   }

   FilePath->Header.Type = MEDIA_DEVICE_PATH;
   FilePath->Header.SubType = MEDIA_FILEPATH_DP;
   SetDevPathNodeLength(&FilePath->Header, Size);
   ucs2_strcpy(FilePath->PathName, PathName);

   Eop = NextDevPathNode(&FilePath->Header);
   Eop->Type = END_DEVICE_PATH_TYPE;
   Eop->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
   SetDevPathNodeLength(Eop, sizeof (EFI_DEVICE_PATH));

   *DevPath = &FilePath->Header;

   return EFI_SUCCESS;
}

/*-- file_devpath --------------------------------------------------------------
 *
 *      If Device is a valid device handle, then a device path for the file
 *      specified by FileName is allocated and appended to the device path
 *      associated with the given handle. If Device is not a valid device
 *      handle, then a device path for the file specified by FileName is
 *      allocated and returned.
 *
 * Parameters
 *      IN  Device:   handle to the device to query
 *      IN  Filename: file path on the device
 *      OUT DevPath:  pointer to the freshly allocated device path
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS file_devpath(EFI_HANDLE Device, const CHAR16 *FileName,
                        EFI_DEVICE_PATH **FileDevPath)
{
   EFI_DEVICE_PATH *DevPath, *FilePath;
   EFI_STATUS Status;

   FilePath = NULL;
   Status = make_file_devpath(FileName, &FilePath);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = devpath_get(Device, &DevPath);
   if (EFI_ERROR(Status)) {
      *FileDevPath = FilePath;
      return EFI_SUCCESS;
   }

   Status = devpath_append(DevPath, FilePath, FileDevPath);
   sys_free(FilePath);

   return Status;
}

/*-- efi_path_concat -----------------------------------------------------------
 *
 *      Concatenate two EFI file paths.
 *
 *      UEFI Specification 2.3, Rules for Path Name conversion, section 9.3.6.4:
 *
 *       - "When concatenating two Path Names, ensure that the resulting string
 *         does not contain a double separator '\\'. If it does, convert that
 *         double-separator to a single-separator.
 *
 *       - In the case where a Path Name which has no end separator is being
 *         concatenated to a Path Name with no beginning separator, a separator
 *         will need to be inserted between the Path Names.
 *
 *       - Single file path nodes with no directory path data are presumed to
 *         have their files located in the root directory of the device."
 *
 *      Note: file path strings in device paths are supposed to be
 *      null-terminated, but some software has been observed not to terminate
 *      them.  This function tolerates a source string that is terminated by
 *      either the device path node length or null.
 *
 * Parameters
 *      IN  Dest: pointer to the path to be appended
 *      IN  Src:  pointer to the path to append
 *----------------------------------------------------------------------------*/
static void efi_path_concat(CHAR16 *Dest, const FILEPATH_DEVICE_PATH *SrcDP)
{
   size_t i, len;
   bool sep;
   size_t SrcMaxLen = (DevPathNodeLength(&SrcDP->Header) -
                       sizeof(SrcDP->Header)) / sizeof(CHAR16);
   const CHAR16 *Src = SrcDP->PathName;

   while (SrcMaxLen > 0 && IS_PATH_SEPARATOR(*Src)) {
      Src++;
      SrcMaxLen--;
   }
   len = ucs2_strnlen(Src, SrcMaxLen);
   while (len > 0 && IS_PATH_SEPARATOR(Src[len - 1])) {
      len--;
   }

   if (len != 0) {
      Dest += ucs2_strlen(Dest);
      *Dest++ = L'\\';

      sep = false;
      for (i = 0; i < len; i++) {
         if (IS_PATH_SEPARATOR(Src[i])) {
            if (!sep) {
               sep = true;
               *Dest++ = L'\\';
            }
         } else {
            sep = false;
            *Dest++ = Src[i];
         }
      }

      *Dest = L'\0';
   }
}

/*-- devpath_get_filepath ------------------------------------------------------
 *
 *      Extract the file path string of a file pointed to by the given device
 *      path. This function returns a freshly allocated empty string if there
 *      is no file portion of the path.
 *
 *      Note: file path strings in device paths are supposed to be
 *      null-terminated, but some software has been observed not to terminate
 *      them.  This function tolerates strings that are terminated by either
 *      the device path node length or null.
 *
 * Parameters
 *      IN  DevPath:  pointer to the device path to query
 *      OUT FilePath: pointer to the freshly allocated file path string
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS devpath_get_filepath(const EFI_DEVICE_PATH *DevPath,
                                CHAR16 **FilePath)
{
   const FILEPATH_DEVICE_PATH *pathnode;
   const EFI_DEVICE_PATH *node;
   CHAR16 *Str;
   size_t len;
   EFI_STATUS Status;

   FOREACH_DEVPATH_NODE(DevPath, node) {
      len = 0;
      FOREACH_FILEPATH_NODE(node, pathnode) {
         len += ucs2_strnlen(pathnode->PathName,
                             (DevPathNodeLength(&pathnode->Header) -
                              sizeof(pathnode->Header)) / sizeof(CHAR16)) + 1;
      }

      if (len > 0) {
         Status = ucs2_alloc(len, &Str);
         if (EFI_ERROR(Status)) {
            return Status;
         }

         FOREACH_FILEPATH_NODE(node, pathnode) {
            efi_path_concat(Str, pathnode);
         }

         *FilePath = Str;
         return EFI_SUCCESS;
      }
   }

   return ucs2_alloc(0, FilePath);
}

/*-- devpath_duplicate -------------------------------------------------------
 *
 *      Duplicate a device path.
 *
 * Parameters
 *      IN  Devpath:   pointer to the device path to duplicate
 *      OUT Duplicate: pointer to the freshly allocated duplicated device path
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS devpath_duplicate(const EFI_DEVICE_PATH *DevPath,
                             EFI_DEVICE_PATH **Duplicate)
{
   EFI_DEVICE_PATH *Path;
   size_t size;

   devpath_size(DevPath, &size, NULL);

   Path = sys_malloc(size);
   if (Path == NULL) {
      return EFI_OUT_OF_RESOURCES;
   }

   memcpy(Path, DevPath, size);
   *Duplicate = Path;

   return EFI_SUCCESS;
}

/*-- devpath_is_parent ---------------------------------------------------------
 *
 *      Compare two device paths and check if the second one derives from the
 *      first one. This is particularly useful for checking whether a file
 *      belongs to a partition, or whether a partition belongs to a drive.
 *
 * Parameters
 *      IN parent: pointer to the first device path
 *      IN child:  pointer to the second device path
 *
 * Results
 *      true if the first device path is a parent of the second one, false
 *      otherwise.
 *----------------------------------------------------------------------------*/
bool devpath_is_parent(const EFI_DEVICE_PATH *parent,
                       const EFI_DEVICE_PATH *child)
{
   const EFI_DEVICE_PATH *node;

   FOREACH_DEVPATH_NODE(parent, node) {
      if (DevPathNodeLength(node) != DevPathNodeLength(child) ||
          memcmp(node, child, DevPathNodeLength(node)) != 0) {
         return false;
      }
      child = NextDevPathNode(child);
   }

   return true;
}

/*-- devpath_text --------------------------------------------------------------
 *
 *      Convert a devpath to ASCII text.
 *
 * Parameters
 *      IN  DevPath:        pointer to the device path
 *      IN  displayOnly:    use shorter non-parseable text representation
 *      IN  allowShortcuts: use shortcut forms in text representation
 *
 * Results
 *      pointer to freshly allocated ASCII string, or NULL on error
 *----------------------------------------------------------------------------*/
char *devpath_text(const EFI_DEVICE_PATH *DevPath,
                   bool displayOnly, bool allowShortcuts)
{
   EFI_STATUS Status;
   EFI_DEVICE_PATH_TO_TEXT_PROTOCOL *dptt;
   CHAR16 *ws;

   Status = LocateProtocol(&DevicePathToTextProto, (void **)&dptt);
   if (EFI_ERROR(Status)) {
      /*
       * No DevicePathToTextProto.  Show as a byte string in hex instead.
       */
      size_t size, i;
      char *s;
      uint8_t *p = (uint8_t *)DevPath;

      devpath_size(DevPath, &size, NULL);
      s = sys_malloc(2 * size + 1);

      for (i = 0; i < size; i++) {
         snprintf(&s[2 * i], 3, "%02x", p[i]);
      }

      return s;
   }

   ws = dptt->ConvertDevicePathToText(DevPath, displayOnly, allowShortcuts);
   if (ws == NULL) {
      return NULL;
   }
   ucs2_to_ascii(ws, (char **)&ws, false);
   return (char *)ws;
}
