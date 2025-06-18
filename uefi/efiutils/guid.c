/*******************************************************************************
 * Copyright (c) 2020 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * guid.c -- Global variables and support functions for GUIDs.
 */

#include <string.h>
#include "efi_private.h"
#include "protocol/gpxe_download.h"

EFI_GUID BlockIoProto = BLOCK_IO_PROTOCOL;
EFI_GUID ComponentNameProto = EFI_COMPONENT_NAME_PROTOCOL_GUID;
EFI_GUID DevicePathProto = DEVICE_PATH_PROTOCOL;
EFI_GUID DiskIoProto = DISK_IO_PROTOCOL;
EFI_GUID DriverBindingProto = EFI_DRIVER_BINDING_PROTOCOL_GUID;
EFI_GUID FileSystemInfoId = EFI_FILE_SYSTEM_INFO_ID;
EFI_GUID FileSystemVolumeLabelInfoId = EFI_FILE_SYSTEM_VOLUME_LABEL_ID;
EFI_GUID GenericFileInfoId = EFI_FILE_INFO_ID;
EFI_GUID GpxeDownloadProto = GPXE_DOWNLOAD_PROTOCOL_GUID;
EFI_GUID LoadFileProto = LOAD_FILE_PROTOCOL;
EFI_GUID SimpleFileSystemProto = SIMPLE_FILE_SYSTEM_PROTOCOL;

/*-- efi_guid_cmp --------------------------------------------------------------
 *
 *      Compare to EFI GUID.
 *
 * Parameters
 *      IN Guid1: pointer to the first GUID
 *      IN Guid2: pointer to the second GUID
 *
 * Results
 *      0 if both GUID's are equal, a non-zero value otherwise.
 *----------------------------------------------------------------------------*/
int efi_guid_cmp(EFI_GUID *guid1, EFI_GUID *guid2)
{
    return memcmp(guid1, guid2, sizeof (EFI_GUID)) ? 1 : 0;
}
