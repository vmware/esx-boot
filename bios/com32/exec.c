/*******************************************************************************
 * Copyright (c) 2008-2011,2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * exec.c -- Binary chainloading
 */

#include <string.h>
#include "com32_private.h"

#define KT_COM32        7

/*-- com32_run_command ---------------------------------------------------------
 *
 *      Wrapper for the 'Run command' COM32 service. This API call does not
 *      return.
 *
 * Parameters
 *      IN command: null-terminated command string
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int com32_run_command(const char *command)
 {
    com32sys_t iregs;
    farptr_t fptr;
    void *buffer;

    if (command == NULL || strlen(command) >= get_bounce_buffer_size()) {
       return ERR_INVALID_PARAMETER;
    }

    buffer = get_bounce_buffer();
    strcpy(buffer, command);

    memset(&iregs, 0, sizeof (iregs));
    fptr = virtual_to_real(buffer);
    iregs.eax.w[0] = 0x03;
    iregs.es = fptr.real.segment;
    iregs.ebx.w[0] = fptr.real.offset;
    intcall_check_CF(COM32_INT, &iregs, NULL);

    return ERR_UNKNOWN;
 }

/*-- com32_run_default ---------------------------------------------------------
 *
 *      Wrapper for the 'Run default command' COM32 service. This API call does
 *      not return.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int com32_run_default(void)
{
   com32sys_t iregs;

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.w[0] = 0x04;
   intcall_check_CF(COM32_INT, &iregs, NULL);

   return ERR_UNKNOWN;
}

/*-- com32_run_kernel_image ----------------------------------------------------
 *
 *      Wrapper for the 'Run kernel image' COM32 service.
 *
 * Parameters
 *      IN filepath:       absolute path to the file
 *      IN options:        pointer to the command line options
 *      IN ipappend_flags: IPAPPEND flags [PXELINUX]
 *      IN type:           Type of file
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int com32_run_kernel_image(const char *filepath, const char *options,
                                  uint32_t ipappend_flags, uint32_t type)
{
   char *bounce_path, *bounce_cmd;
   com32sys_t iregs;
   size_t path_size;
   farptr_t fptr;

   path_size = strlen(filepath) + 1;

   if (path_size + strlen(options) + 1 > get_bounce_buffer_size()) {
      return ERR_INVALID_PARAMETER;
   }

   bounce_path = get_bounce_buffer();
   bounce_cmd = bounce_path + path_size;

   strcpy(bounce_path, filepath);
   strcpy(bounce_cmd, options);

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.w[0] = 0x16;
   fptr = virtual_to_real(bounce_path);
   iregs.ds = fptr.real.segment;
   iregs.esi.w[0] = fptr.real.offset;
   fptr = virtual_to_real(bounce_cmd);
   iregs.es = fptr.real.segment;
   iregs.ebx.w[0] = fptr.real.offset;
   iregs.ecx.l = ipappend_flags;
   iregs.edx.l = type;

   return intcall_check_CF(COM32_INT, &iregs, NULL);
}

/*-- firmware_file_exec --------------------------------------------------------
 *
 *      Execute a COM32 module. If found, the COM32 module is not supposed to
 *      return.
 *
 *      Limitation: COM32 modules can only be executed from the boot volume.
 *
 * Parameters
 *      IN filepath: absolute path to the file
 *      IN options:  pointer to the command line options (not including
 *                   the command name).
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int firmware_file_exec(const char *filepath, const char *options)
{
   if (options == NULL) {
      options = "";
   }

   com32_run_kernel_image(filepath, options, 0, KT_COM32);

   return ERR_NOT_FOUND;
}
