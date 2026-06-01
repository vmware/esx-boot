/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * devpath.c -- EFI device path handling routines
 */

#include <string.h>
#include "efi_private.h"
#include "DevicePathToText.h"

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

   Path = malloc(s1 + instances * s2);
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

   FilePath = malloc(Size + sizeof (EFI_DEVICE_PATH));
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
   free(FilePath);

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
 *      Note: File path strings in device paths are supposed to be
 *      null-terminated, but some software has been observed not to terminate
 *      them.  This function tolerates a source string that is terminated by
 *      either the device path node length or null.
 *
 *      Note: Even if the first path is empty, this function still inserts a
 *      separator before appending (unless the second path is empty and thus
 *      nothing is appended).
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
 *      Note: If nonempty, the returned value always has a leading backslash.
 *
 *      Note: File path strings in device paths are supposed to be
 *      null-terminated, but some software has been observed not to terminate
 *      them.  This function tolerates strings that are terminated by either
 *      the device path node length or null.
 *
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

   Path = malloc(size);
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
      s = malloc(2 * size + 1);

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

/*-- asprintf_address_v4 -------------------------------------------------------
 *
 *      Wrapper around asprintf designed for ipv4.
 *
 * Parameters
 *      IN / OUT s: The first parameter to asprintf.
 *      IN addr_ptr: A pointer to the fixed length array where the ipv4 value
 *         is kept.
 *
 * Results
 *      -1 if addr_ptr is NULL, the return value of asprintf otherwise.
 *----------------------------------------------------------------------------*/
static int asprintf_address_v4(char** s, uint8_t (*addr_ptr)[4])
{
   const uint8_t* addr = NULL;

   if (addr_ptr == NULL) {
      return -1;
   }

   addr = &((*addr_ptr)[0]);

   return asprintf(
      s,
      "%d.%d.%d.%d",
      addr[0], addr[1], addr[2], addr[3]
   );
}

/*-- asprintf_address_v6 -------------------------------------------------------
 *
 *      Wrapper around asprintf designed for ipv6.
 *
 * Parameters
 *      IN / OUT s: The first parameter to asprintf.
 *      IN addr_ptr: A pointer to the fixed length array where the ipv6 value
 *         is kept.
 *
 * Results
 *      -1 if addr_ptr is NULL, the return value of asprintf otherwise.
 *----------------------------------------------------------------------------*/
static int asprintf_address_v6(char** s, uint8_t (*addr_ptr)[16])
{
   const uint8_t* addr = NULL;

   if (addr_ptr == NULL) {
      return -1;
   }

   addr = &((*addr_ptr)[0]);

   return asprintf(
      s,
      "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
      "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
      addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6],
      addr[7], addr[8], addr[9], addr[10], addr[11], addr[12], addr[13],
      addr[14], addr[15]
   );
}

/*-- get_hash_id ---------------------------------------------------------------
 *
 *      Retrieves the first 4 bytes of the given hash string in native
 *         endianness.
 *
 * Parameters
 *      IN hash: A char array that is usable for a runtime integrity check and
 *         remains unchanged after the current binary is cryptographically
 *         signed.
 *
 * Results
 *      The first 4 bytes as a uint32_t value. If hash is NULL the return value
 *         is 0.
 *----------------------------------------------------------------------------*/
uint32_t get_hash_id(const uint8_t* hash)
{
   return hash ? *((const uint32_t *)hash) : 0;
}

/*-- extract_efiboot_version ---------------------------------------------------
 *
 *      Retrieves a string variable used for versioning.
 *
 *      This function requires for asprintf to leave the out parameter
 *      unchanged on error, this is true for our asprintf implementation, but
 *      not true for glibc's asprintf implementation.
 *
 * Parameters
 *      IN hash: A char array that is usable for a runtime integrity check and
 *         remains unchanged after the current binary is cryptographically
 *         signed.
 *      OUT version: a pointer to a dynamic string specifying where the result
 *          is returned, (*version) needs to be NULL otherwise and error is
 *          reported.
 *
 * Results
 *      EFI_SUCCESS or a generic error status on failure. Regardless of the
 *      returned value, the caller is responsible for cleaning up the out
 *      parameter via free(*version) if (*version) is NULL.
 *----------------------------------------------------------------------------*/
EFI_STATUS extract_efiboot_version(const uint8_t* hash, char** version)
{
   if (version == NULL || hash == NULL) {
      return EFI_INVALID_PARAMETER;
   }

   if (*version != NULL) {
      return EFI_INVALID_PARAMETER;
   }

   /*
    * This version must be changed by hand for future releases
    */
   if (asprintf(version, "%s.%08x",
      EFIBOOT_VERSION,
      get_hash_id(hash)
   ) == -1) {
      return EFI_OUT_OF_RESOURCES;
   }

   return EFI_SUCCESS;
}

/*-- extract_vlan_id -----------------------------------------------------------
 *
 *      Retrieves the vlan id from the specified node.
 *
 *      When the node is accessed this function causes misaligned memory
 *      access as stated in UEFI spec for Device Path.
 *
 *      This function requires for asprintf to leave the out parameter
 *      unchanged on error, this is true for our asprintf implementation, but
 *      not true for glibc's asprintf implementation.
 *
 * Parameters
 *      IN node: A pointer to the node containing the vlan data, required to
 *          be different from NULL.
 *      OUT vlan_id: a pointer to a dynamic string specifying where the result
 *          is returned formatted as a base 10 integer. It must point to char*
 *          which is NULL.
 *
 * Results
 *      EFI_SUCCESS or a generic error status on failure. Regardless of the
 *      returned value, the caller is responsible for cleaning up the out
 *      parameter via free(*vlan_id) if (*vlan_id) is NULL.
 *----------------------------------------------------------------------------*/
EFI_STATUS extract_vlan_id(VLAN_DEVICE_PATH* node, char** vlan_id)
{
   if (node == NULL || vlan_id == NULL) {
      return EFI_INVALID_PARAMETER;
   }

   if (*vlan_id != NULL) {
      return EFI_INVALID_PARAMETER;
   }

   if (asprintf(vlan_id, "%d", node->VlanId) == -1) {
      return EFI_OUT_OF_RESOURCES;
   }

   return EFI_SUCCESS;
}

/*-- extract_mac ---------------------------------------------------------------
 *
 *      Retrieves the mac address from the given node.
 *
 *      When the node is accessed this function causes misaligned memory
 *      access as stated in UEFI spec for Device Path.
 *
 *      This function requires for asprintf to leave the out parameter
 *      unchanged on error, this is true for our asprintf implementation, but
 *      not true for glibc's asprintf implementation.
 *
 * Parameters
 *      IN node: A pointer to the node containing the mac data, required to
 *          be different from NULL.
 *      OUT mac: a pointer to a dynamic string specifying where the result
 *          is returned formatted as "aa:bb:cc:dd:ee:ff". It must point to
 *          char* which is NULL.
 *
 * Results
 *      EFI_SUCCESS or a generic error status on failure. Regardless of the
 *      returned value, the caller is responsible for cleaning up the out
 *      parameter via free(*mac) if (*mac) is NULL.
 *----------------------------------------------------------------------------*/
EFI_STATUS extract_mac(MAC_ADDR_DEVICE_PATH* node, char** mac)
{
   const uint8_t* mac_addr = NULL;
   if (node == NULL || mac == NULL) {
      return EFI_INVALID_PARAMETER;
   }

   if (*mac != NULL) {
      return EFI_INVALID_PARAMETER;
   }

   mac_addr = node->MacAddress.Addr;

   if (asprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
      mac_addr[0], mac_addr[1], mac_addr[2],
      mac_addr[3], mac_addr[4], mac_addr[5]) == -1) {
      return EFI_OUT_OF_RESOURCES;
   }

   return EFI_SUCCESS;
}

/*-- extract_ipv4_data ---------------------------------------------------------
 *
 *      Retrieves the ipv4 address, netmask and gatewayv4 address from the
 *      specified node.
 *
 *      When the node is accessed this function causes misaligned memory
 *      access as stated in UEFI spec for Device Path.
 *
 *      This function requires for asprintf to leave the out parameter
 *      unchanged on error, this is true for our asprintf implementation, but
 *      not true for glibc's asprintf implementation.
 *
 * Parameters
 *      IN node: A pointer to the node containing the ipv4 data, required to
 *          be different from NULL.
 *      OUT ipv4: a pointer to a dynamic string specifying where the result
 *          is returned formatted as "255.254.253.252". If ipv4 is NULL then
 *          it is unused, otherwise it must point to char* which is NULL.
 *      OUT netmask: a pointer to a dynamic string specifying where the result
 *          is returned formatted as "fffefdfc". If netmask is NULL then
 *          it is unused, otherwise it must point to char* which is NULL.
 *      OUT gatewayv4: a pointer to a dynamic string specifying where the result
 *          is returned formatted as "255.254.253.252". If gatewayv4 is NULL
 *          then it is unused, otherwise it must point to char* which is NULL.
 *
 * Results
 *      EFI_SUCCESS or a generic error status on failure. Regardless of the
 *      returned value, the caller is responsible for cleaning up the out
 *      parameters via:
 *         free(*ipv4);
 *         free(*netmask);
 *         free(*gatewayv4);
 *----------------------------------------------------------------------------*/
EFI_STATUS extract_ipv4_data(IPv4_DEVICE_PATH* ipNode, char** ipv4,
   char** netmask, char** gatewayv4)
{
   if (ipNode == NULL) {
      return EFI_INVALID_PARAMETER;
   }

   if (ipv4 != NULL) {
      if (*ipv4 != NULL) {
         return EFI_INVALID_PARAMETER;
      }

      if (asprintf_address_v4(ipv4, &(ipNode->LocalIpAddress.Addr)) == -1) {
         return EFI_OUT_OF_RESOURCES;
      }
   }

   if (netmask != NULL) {
      if (*netmask != NULL) {
         return EFI_INVALID_PARAMETER;
      }

      if (asprintf_address_v4(netmask, &(ipNode->SubnetMask.Addr)) == -1) {
         return EFI_OUT_OF_RESOURCES;
      }
   }

   if (gatewayv4 != NULL) {
      if (*gatewayv4 != NULL) {
         return EFI_INVALID_PARAMETER;
      }

      if (asprintf_address_v4(gatewayv4, &(ipNode->GatewayIpAddress.Addr))
         == -1) {
         return EFI_OUT_OF_RESOURCES;
      }
   }

   return EFI_SUCCESS;
}

/*-- extract_ipv6_data ---------------------------------------------------------
 *
 *      Retrieves the ipv6 address, prefix and gatewayv6 address from the
 *      specified node.
 *
 *      When the node is accessed this function causes misaligned memory
 *      access as stated in UEFI spec for Device Path.
 *
 *      This function requires for asprintf to leave the out parameter
 *      unchanged on error, this is true for our asprintf implementation, but
 *      not true for glibc's asprintf implementation.
 *
 * Parameters
 *      IN node: A pointer to the node containing the ipv6 data, required to
 *          be different from NULL.
 *      OUT ipv6: a pointer to a dynamic string specifying where the result
 *          is returned formatted as an ipv6 address. If ipv6 is NULL then
 *          it is unused, otherwise it must point to char* which is NULL.
 *      OUT prefix: a pointer to a dynamic string specifying where the result
 *          is returned formatted as a base 10 integer. If prefix is NULL then
 *          it is unused, otherwise it must point to char* which is NULL.
 *      OUT gatewayv6: a pointer to a dynamic string specifying where the result
 *          is returned formatted as an ipv6 address. If gatewayv6 is NULL then
 *          it is unused, otherwise it must point to char* which is NULL.
 *
 * Results
 *      EFI_SUCCESS or a generic error status on failure. Regardless of the
 *      returned value, the caller is responsible for cleaning up the out
 *      parameters via:
 *         free(*ipv6);
 *         free(*prefix);
 *         free(*gatewayv6)
 *----------------------------------------------------------------------------*/
EFI_STATUS extract_ipv6_data(IPv6_DEVICE_PATH* ipNode, char** ipv6,
   char** prefix, char** gatewayv6)
{
   if (ipNode == NULL) {
      return EFI_INVALID_PARAMETER;
   }

   if (ipv6 != NULL) {
      if (*ipv6 != NULL) {
         return EFI_INVALID_PARAMETER;
      }

      if (asprintf_address_v6(ipv6, &(ipNode->LocalIpAddress.Addr)) == -1) {
         return EFI_OUT_OF_RESOURCES;
      }
   }

   if (prefix != NULL) {
      if (*prefix != NULL) {
         return EFI_INVALID_PARAMETER;
      }

      if (asprintf(prefix, "%d", ipNode->PrefixLength) == -1) {
         return EFI_OUT_OF_RESOURCES;
      }
   }

   if (gatewayv6 != NULL) {
      if (*gatewayv6 != NULL) {
         return EFI_INVALID_PARAMETER;
      }

      if (asprintf_address_v6(gatewayv6, &(ipNode->GatewayIpAddress.Addr))
            == -1) {
         return EFI_OUT_OF_RESOURCES;
      }
   }

   return EFI_SUCCESS;
}

/*-- extract_dns_data ----------------------------------------------------------
 *
 *      Retrieves the dns addresses from the specified node.
 *
 *      This function assumes that the byte size of the formatted section after
 *      the DNS_DEVICE_PATH struct is divisible by sizeof(EFI_IP_ADDRESS). If
 *      this assumption is not true then unexpected dns servers will be
 *      returned.
 *
 *      When the node is accessed this function causes misaligned memory
 *      access as stated in UEFI spec for Device Path.
 *
 * Parameters
 *      IN node: A pointer to the node containing the dns data, required to
 *          be different from NULL.
 *      OUT dns_servers_param: a pointer to a dns_severs_t value where the
 *          result is returned, either in ipv4 or ipv6 format for each dns
 *          address. It must point to a valid and accessible dns_servers_t
 *          instance which has .length = 0 and .entries = NULL.
 *
 * Results
 *      EFI_SUCCESS or a generic error status on failure. Regardless of the
 *      returned value, the caller is responsible for cleaning up the out
 *      parameters via:
 *      for(size_t i = 0; i < dns_servers_param->length; ++i) {
 *         free((char*)(dns_servers_param->entries[i].key));
 *         free((char*)(dns_servers_param->entries[i].value));
 *      }
 *      free(dns_servers_param->entries);
 *
 *      and optionally
 *
 *      dns_servers_param->entries = NULL;
 *      dns_servers_param->length = 0;
 *
 *----------------------------------------------------------------------------*/
EXTERN EFI_STATUS extract_dns_data(DNS_DEVICE_PATH* node_dns,
   dns_servers_t* dns_servers_param)
{
   size_t byteCountRawData = 0;
   size_t count = 0;
   EFI_IP_ADDRESS* arr = NULL;
   int rc;

   if (node_dns == NULL || dns_servers_param == NULL) {
      return EFI_INVALID_PARAMETER;
   }

   if ( dns_servers_param->entries != NULL ||
      dns_servers_param->length != 0 ) {
      return EFI_INVALID_PARAMETER;
   }

   byteCountRawData = DevPathNodeLength((EFI_DEVICE_PATH*)node_dns) -
      sizeof(DNS_DEVICE_PATH);

   count = byteCountRawData / sizeof(node_dns->DnsServerIp[0]);

   if (count > 0) {
      dns_servers_param->entries =
         malloc(count * sizeof(dns_servers_param->entries[0]));

      if (dns_servers_param->entries == NULL) {
         return EFI_OUT_OF_RESOURCES;
      }

      arr = node_dns->DnsServerIp;

      for(size_t i = 0; i < count; ++i) {
         char* key;
         char* value;

         if (asprintf(&key, "dns_server_%zu", dns_servers_param->length)
            == -1) {
            return EFI_OUT_OF_RESOURCES;
         }

         if (node_dns->IsIPv6) {
            rc = asprintf_address_v6(&value, &(arr[i].v6.Addr));
         } else {
            rc = asprintf_address_v4(&value, &(arr[i].v4.Addr));
         }

         if (rc == -1) {
            free(key);
            return EFI_OUT_OF_RESOURCES;
         }
         dns_servers_param->entries[dns_servers_param->length].key = key;
         dns_servers_param->entries[dns_servers_param->length].value = value;

         dns_servers_param->length += 1;
      }
   }

   return EFI_SUCCESS;
}