/*******************************************************************************
 * Copyright (c) 2008-2011,2015-2016,2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * efi.h --
 */

#ifndef EFI_H_
#define EFI_H_

#if defined(only_em64t)
#define EFIAPI __attribute__((ms_abi))
#endif

/* EDK2 headers redefine NULL, MIN() and MAX() */
#undef NULL
#undef MIN
#undef MAX

#include <Uefi.h>
#include <Protocol/RuntimeWatchdog.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/LoadFile.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/PxeBaseCode.h>
#include <Protocol/UgaDraw.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>
#include <Protocol/ComponentName.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/ShellParameters.h>
#include <Protocol/Tcg2Protocol.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <Guid/FileSystemVolumeLabelInfo.h>
#include <Guid/Acpi.h>
#include <Guid/Mps.h>
#include <Guid/SmBios.h>
#include <IndustryStandard/Acpi50.h>
#include <IndustryStandard/SerialPortConsoleRedirectionTable.h>
#include <IndustryStandard/SmBios.h>
#include <Guid/Fdt.h>

#endif /* !EFI_H_ */
