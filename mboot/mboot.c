/*******************************************************************************
 * Copyright (c) 2008-2018 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * mboot.c -- Mu(l)tiboot loader
 *
 *   mboot [aSstRpeVDQU] -c <FILEPATH> [KERNEL_OPTIONS]
 *
 *      OPTIONS
 *         -a             Do not pass extended attributes in the Mu(l)tiboot info
 *                        memory map. This is necessary for kernels which do not
 *                        support Mu(l)tiboot memory map extensions, and which
 *                        hardcode the memory map entry size.
 *         -c <FILEPATH>  Set the configuration file to FILEPATH.
 *         -S <1...4>     Set the default serial port (1=COM1, 2=COM2, 3=COM3,
 *                        4=COM4, 0xNNNN=hex I/O port address ).
 *         -s <BAUDRATE>  Set the serial port speed to BAUDRATE (in bits per
 *                        second).
 *         -t <TITLE>     Set the bootloader banner title.
 *         -R <CMDLINE>   Set the command to be executed when <SHIFT+R> is
 *                        pressed. <CMDLINE> is only executed if the underlying
 *                        firmware library does not allow to return from main().
 *         -p <0...n>     Set the boot partition to boot from.
 *         -e             Exit on transient errors.
 *         -V             Enable verbose mode.  Causes all log messages to be
 *                        sent to the GUI, once the GUI is sufficiently
 *                        initialized.  Without this option only LOG_INFO and
 *                        below are sent to the GUI.
 *         -D             Enable additional debug logging; see code for details.
 *         -H             Ignore graphical framebuffer and boot ESXi headless.
 *         -Q             Disable workarounds for platform quirks.
 *         -U             Disable UEFI runtime services support.
 *
 * Note: if you add more options that take arguments, be sure to update
 * safeboot.c so that safeboot can pass them through to mboot.
 */

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <boot_services.h>
#include "mboot.h"

/* Bootloader main info structure */
boot_info_t boot;

static char *kopts = NULL;

/*-- clean ---------------------------------------------------------------------
 *
 *      Clean up everything so mboot can return properly.
 *----------------------------------------------------------------------------*/
static int clean(int status)
{
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Fatal error: %d (%s)\n", status, error_str[status]);
      if (boot.exit_on_errors && ((status == ERR_LOAD_ERROR)   ||
                                  (status == ERR_DEVICE_ERROR) ||
                                  (status == ERR_NOT_FOUND)    ||
                                  (status == ERR_NO_RESPONSE)  ||
                                  (status == ERR_TIMEOUT)      ||
                                  (status == ERR_TFTP_ERROR)   ||
                                  (status == ERR_END_OF_FILE)  ||
                                  (status == ERR_UNEXPECTED_EOF))) {
         if (!gui_exit()) {
            while (1);
         }
      } else {
         while (1);
      }
   }

   sys_free(kopts);
   sys_free(boot.cfgfile);
   unload_boot_modules();
   config_clear();

   return status;
}

/*-- mboot_init ----------------------------------------------------------------
 *
 *      Early bootloader initialization
 *
 * Parameters
 *      IN argc: number of command line arguments
 *      IN argv: pointer to arguments array
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int mboot_init(int argc, char **argv)
{
   int opt, serial_com, serial_speed, status;

   boot.serial = false;
   serial_com = DEFAULT_SERIAL_COM;
   serial_speed = DEFAULT_SERIAL_BAUDRATE;

   memset(&boot, 0, sizeof (boot_info_t));
   boot.bootif = true;

   if (argc < 1) {
      Log(LOG_DEBUG, "Command line is empty.\n");
   }

   optind = 1;

   do {
      opt = getopt(argc, argv, ":ac:R:p:S:s:t:VeDHQUF");
      switch (opt) {
         case -1:
            break;
         case 'a':
            boot.no_mem_attr = true;
            break;
         case 'c':
            boot.cfgfile = strdup(optarg);
            if (boot.cfgfile == NULL) {
               return ERR_OUT_OF_RESOURCES;
            }
            break;
         case 'p':
            if (!is_number(optarg)) {
               Log(LOG_CRIT, "Nonnumeric argument to -%c: %s", opt, optarg);
               return ERR_SYNTAX;
            }
            boot.volid = atoi(optarg);
            break;
         case 'V':
            boot.verbose = true;
            break;
         case 'R':
            boot.recovery_cmd = strdup(optarg);
            if (boot.recovery_cmd == NULL) {
               return ERR_OUT_OF_RESOURCES;
            }
            break;
         case 'S':
            boot.serial = true;
            serial_com = strtol(optarg, NULL, 0);
            break;
         case 's':
            if (!is_number(optarg)) {
               Log(LOG_CRIT, "Nonnumeric argument to -%c: %s", opt, optarg);
               return ERR_SYNTAX;
            }
            boot.serial = true;
            serial_speed = atoi(optarg);
            break;
         case 't':
            gui_set_title(optarg);
            break;
         case 'e':
            boot.exit_on_errors = true;
            break;
         case 'D':
            boot.debug = true;
            break;
         case 'H':
            boot.headless = true;
            break;
         case 'Q':
            boot.no_quirks = true;
            break;
         case 'U':
            boot.no_rts = true;
            break;
         case 'd':
            /*
             * XXX: 'drive number/signature' (To be implemented)
             *      mboot currently loads modules from the boot media. This
             *      option will allow to specify an alternate boot drive to
             *      boot modules from.
             */
         case ':':
            /* Missing option argument */
            Log(LOG_CRIT, "Missing argument to -%c", optopt);
            return ERR_SYNTAX;
         case '?':
            /* Unknown option */
            Log(LOG_CRIT, "Unknown option -%c", optopt);
            return ERR_SYNTAX;
         default:
            /* Bug: option in string but no case for it */
            Log(LOG_CRIT, "Unknown option -%c", opt);
            return ERR_SYNTAX;
      }
   } while (opt != -1);

#ifdef DEBUG
   boot.verbose = true;
   boot.debug = true;
   boot.serial = true;
#endif /* DEBUG */

   if (boot.verbose) {
      log_unsubscribe(firmware_print);
      status = log_subscribe(firmware_print, LOG_DEBUG);
      if (status != ERR_SUCCESS) {
         return status;
      }
   }

   if (boot.serial) {
      status = serial_log_init(serial_com, serial_speed);
      boot.serial = (status == ERR_SUCCESS);
   }

   argc -= optind;
   argv += optind;

   if (argc > 0) {
      /*
       * Remaining arguments are treated as kernel options. This is required
       * for compatibility with the IPAPPEND pxelinux option which automatically
       * appends the BOOTIF=<MAC_ADDR> option to mboot's command line.
       */
      status = argv_to_str(argc, argv, &kopts);
      if (status != ERR_SUCCESS) {
         return status;
      }
   }

#ifdef DEBUG
   /*
    * Date/time can't be embedded in official builds, as that would break our
    * process for signing with UEFI-CA keys.  Keys from sigcache would never
    * match a new build, because the new build would have a different date/time
    * embedded than the one that was signed.  See PR 2110648 update #5 and
    * https://wiki.eng.vmware.com/ESXSecureBoot/UEFI-CA-Signing.
    */
   Log(LOG_DEBUG, "Built on %s/%s\n", __DATE__, __TIME__);
#endif
   snprintf(boot.name, MBOOT_ID_SIZE, "%s", MBOOT_ID_STR);

   return ERR_SUCCESS;
}

/*-- ipappend_2 ----------------------------------------------------------------
 *
 *      Detect the boot interface MAC address and force appending the
 *      'BOOTIF=xx-aa-bb-cc-dd-ee-ff' to the kernel command line, where xx is
 *      the Hardware Type Number of the boot interface (see RFC 1700), and
 *      aa:bb:cc:dd:ee:ff is its MAC address.
 *
 *      This avoids us to depend on the 'IPAPPEND' pxelinux option (which has no
 *      UEFI equivalent).
 *
 *      This function does nothing if the BOOTIF= option is already present on
 *      the kernel command line (for instance when IPAPPEND 2 is actually set in
 *      the pxelinux configuration file).
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int ipappend_2(void)
{
   const char *bootif, *opt;
   int status;

   if (boot.modules[0].options != NULL) {
      opt = strstr(boot.modules[0].options, "BOOTIF=");
      if (opt != NULL) {
         if (opt == boot.modules[0].options || isspace(*(--opt))) {
            return ERR_SUCCESS;
         }
      }
   }

   status = get_bootif_option(&bootif);
   if (status != ERR_SUCCESS) {
      Log(LOG_WARNING, "Network boot: MAC address not found.\n");
      Log(LOG_WARNING, "Add \'BOOTIF=01-aa-bb-cc-dd-ee-ff\' manually to the "
          "boot options (required if booting from gPXE).\n");
      Log(LOG_WARNING, "Replace aa-bb-cc-dd-ee-ff with the MAC address of the "
          "boot interface\n");

      return ERR_SUCCESS;
   }

   Log(LOG_DEBUG, "Network boot: %s\n", bootif);

   return append_kernel_options(bootif);
}

/*-- main ----------------------------------------------------------------------
 *
 *      Mu(l)tiboot loader main function.
 *
 * Parameters
 *      IN argc: number of arguments on the command line
 *      IN argv: the command line
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *
 * Side Effects
 *      Upon success, should never return.
 *----------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
   trampoline_t jump_to_trampoline;
   handoff_t *handoff;
   run_addr_t mbi;
   int status;

   status = log_init(false);
   if (status != ERR_SUCCESS) {
      return clean(status);
   }

   status = mboot_init(argc, argv);
   if (status != ERR_SUCCESS) {
      return clean(status);
   }

   status = dump_firmware_info();
   if (status != ERR_SUCCESS) {
      return clean(status);
   }

   /*
    * Log the initial state of the firmware memory map.  This logging
    * occurs prior to GUI initialization (causing spew on the screen
    * on some machines), and the log is generally much more than a
    * screenful, so it's disabled by default.  Use the -D and -S flags
    * or a DEBUG build to see the log.
    */
   if (boot.debug) {
      Log(LOG_DEBUG, "Logging initial memory map\n");
      log_memory_map(&boot.efi_info);
   }

#ifndef __COM32__
   if (boot.no_quirks) {
      Log(LOG_DEBUG, "Skipping quirks...\n");
   } else {
      Log(LOG_DEBUG, "Processing quirks...\n");
      check_efi_quirks(&boot.efi_info);
   }

   if ((boot.efi_info.quirks & EFI_FB_BROKEN) != 0) {
      boot.headless = true;
   }
#endif

   if (!boot.headless &&
       gui_init() != ERR_SUCCESS) {
      boot.headless = true;
   }

   status = parse_config(boot.cfgfile);
   if (status != ERR_SUCCESS) {
      return clean(status);
   }

   if (kopts != NULL) {
      status = append_kernel_options(kopts);
      if (status != ERR_SUCCESS) {
         return clean(status);
      }
   }

   if (boot.bootif && is_network_boot()) {
      status = ipappend_2();
      if (status != ERR_SUCCESS) {
         return clean(status);
      }
      boot.is_network_boot = true;
   }

   if (!boot.headless) {
      status = gui_edit_kernel_options();
      if (status == ERR_ABORTED) {
         clean(ERR_SUCCESS);
         status = chainload_parent(boot.recovery_cmd);
         return (status == ERR_SUCCESS) ? ERR_ABORTED : status;
      } else if (status != ERR_SUCCESS) {
         return clean(status);
      }
   }

   status = load_boot_modules();
   if (status != ERR_SUCCESS) {
      return clean(status);
   }

#ifdef SECURE_BOOT
   boot.efi_info.secure_boot = secure_boot_mode();
   if (boot.efi_info.secure_boot) {
      Log(LOG_INFO, "UEFI Secure Boot in progress\n");
   } else {
      Log(LOG_INFO, "UEFI Secure Boot is not enabled\n");
   }

   status = secure_boot_check();
   if (status != ERR_SUCCESS) {
      if (status == ERR_NOT_FOUND) {
         Log(LOG_INFO, "Boot modules are not signed\n");
      } else {
         Log(LOG_CRIT, "Boot module signatures are not valid\n");
      }
      if (boot.efi_info.secure_boot) {
         return clean(ERR_INSECURE);
      }
   }
#endif

   Log(LOG_DEBUG, "Initializing Mu%stiboot standard...\n",
       boot.is_mutiboot ? "" : "l");

   status = boot_init();
   if (status != ERR_SUCCESS) {
      return clean(status);
   }

   Log(LOG_INFO, "Shutting down firmware services...\n");

   if (firmware_shutdown(&boot.mmap, &boot.mmap_count,
                         &boot.efi_info)                 != ERR_SUCCESS
    || boot_register()                                   != ERR_SUCCESS
    || compute_relocations(boot.mmap, boot.mmap_count)   != ERR_SUCCESS
    || boot_set_runtime_pointers(&mbi)                   != ERR_SUCCESS
    || relocate_runtime_services(&boot.efi_info,
                                 boot.no_rts, boot.no_quirks) != ERR_SUCCESS
    || install_trampoline(&jump_to_trampoline, &handoff) != ERR_SUCCESS) {
      /* Cannot return because Boot Services have been shutdown. */
      Log(LOG_EMERG, "Unrecoverable error\n");
      PANIC();
   }

   Log(LOG_INFO, "Relocating modules and starting up the kernel...\n");
   handoff->mbi = mbi;
   handoff->kernel = boot.kernel.entry;
   handoff->mbi_magic = boot.is_mutiboot ? MUTIBOOT_MAGIC : MBI_MAGIC;
   jump_to_trampoline(handoff);

   NOT_REACHED();

   return ERR_UNKNOWN;
}
