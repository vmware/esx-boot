/*******************************************************************************
 * Copyright (c) 2008-2023 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * mboot.c -- ESXBootInfo (and Multiboot) loader
 *
 *   mboot [aSstRpeVDQU] -c <FILEPATH> [KERNEL_OPTIONS]
 *
 *      OPTIONS
 *         -a             Do not pass extended attributes in the Multiboot
 *                        memory map. This is necessary for kernels which do not
 *                        support Multiboot memory map extensions, and which
 *                        hardcode the memory map entry size.  This option is
 *                        not meaningful for ESXBootInfo kernels.
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
 *         -E <TIMEOUT>   When a fatal error occurs: if TIMEOUT >= 0, exit with
 *                        error status after TIMEOUT seconds, or immediately if
 *                        in headless mode; if TIMEOUT < 0, hang.  Default: -1.
 *         -e             Exit on errors after 30 seconds.  Same as -E 30.
 *         -V             Enable verbose mode.  Causes all log messages to be
 *                        sent to the GUI, once the GUI is sufficiently
 *                        initialized.  Without this option only LOG_INFO and
 *                        below are sent to the GUI.
 *         -D             Enable additional debug logging; see code for details.
 *         -L             Final log expansion size can be configured with this
 *                        option [size in bytes].
 *         -H             Ignore graphical framebuffer and boot ESXi headless.
 *         -Q             Disable workarounds for platform quirks.
 *         -U             Disable UEFI runtime services support.
 *         -N <N>         Set criteria for using native UEFI HTTP to N.
 *                        0: Never use native UEFI HTTP.
 *                        1: Use native UEFI HTTP if mboot itself was
 *                           loaded via native UEFI HTTP (default).
 *                        2: Use native UEFI HTTP if it allows plain http URLs.
 *                        3: Always use native UEFI HTTP.
 *         -r             Enable the hardware runtime watchdog.
 *         -b <BLKSIZE>   For TFTP transfers, set the blksize option to the
 *                        given value, default 1468.  UEFI only.
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
#include <system_int.h>
#include "mboot.h"

#if defined(SECURE_BOOT) && defined(CRYPTO_MODULE)
   #if defined(only_arm64) || defined(only_em64t) || defined(only_riscv64)
      #define CRYPTO_DRIVER "crypto64.efi"
   #else
      #define CRYPTO_DRIVER "crypto32.efi"
   #endif
#endif

/* Bootloader main info structure */
boot_info_t boot;

static char *kopts = NULL;
static uint32_t expand_size = 0;

/*-- clean ---------------------------------------------------------------------
 *
 *      Clean up everything so mboot can return properly.
 *----------------------------------------------------------------------------*/
static int clean(int status)
{
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Fatal error: %d (%s)", status, error_str[status]);
      gui_refresh();
      if (boot.err_timeout >= 0) {
         if (!boot.headless && !gui_exit(boot.err_timeout)) {
            while (1);
         }
      } else {
         while (1);
      }
   }

   sys_free(kopts);
   sys_free(boot.cfgfile);
   uninstall_acpi_tables();
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

   memset(&boot, 0, sizeof(boot));
   boot.bootif = true;
   boot.err_timeout = -1;
#ifdef DEBUG
   boot.verbose = true;
   boot.debug = true;
   boot.serial = true;
#endif /* DEBUG */
   serial_com = DEFAULT_SERIAL_COM;
   serial_speed = DEFAULT_SERIAL_BAUDRATE;

   status = log_init(boot.verbose);
   if (status != ERR_SUCCESS) {
      return status;
   }
   syslogbuf_expand_enable();

   if (argc < 1) {
      Log(LOG_DEBUG, "Command line is empty.");
   }

   optind = 1;

   do {
      opt = getopt(argc, argv, ":ac:R:p:S:s:t:VeDL:HQUN:rb:");
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
         case 'E':
            if (!is_number(optarg)) {
               Log(LOG_CRIT, "Nonnumeric argument to -%c: %s", opt, optarg);
               return ERR_SYNTAX;
            }
            boot.err_timeout = atoi(optarg);
            break;
         case 'e':
            boot.err_timeout = 30;
            break;
         case 'L':
            if (!is_number(optarg)) {
               Log(LOG_CRIT, "Nonnumeric argument to -%c: %s", opt, optarg);
               return ERR_SYNTAX;
            }
            expand_size = atoi(optarg);
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
         case 'N':
            if (!is_number(optarg)) {
               Log(LOG_CRIT, "Nonnumeric argument to -%c: %s", opt, optarg);
               return ERR_SYNTAX;
            }
            set_http_criteria(atoi(optarg));
            break;
         case 'r':
            boot.runtimewd = true;
            break;
         case 'b':
            if (!is_number(optarg)) {
               Log(LOG_CRIT, "Nonnumeric argument to -%c: %s", opt, optarg);
               return ERR_SYNTAX;
            }
            tftp_set_block_size(atoi(optarg));
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

   log_init(boot.verbose);

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
   Log(LOG_DEBUG, "Built on %s/%s", __DATE__, __TIME__);
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
      Log(LOG_WARNING, "Network boot: MAC address not found.");
      Log(LOG_WARNING, "Add \'BOOTIF=01-aa-bb-cc-dd-ee-ff\' manually to the "
          "boot options (required if booting from gPXE).");
      Log(LOG_WARNING, "Replace aa-bb-cc-dd-ee-ff with the MAC address of the "
          "boot interface");

      return ERR_SUCCESS;
   }

   Log(LOG_DEBUG, "Network boot: %s", bootif);

   return append_kernel_options(bootif);
}

/*-- start_runtimewd -----------------------------------------------------------
 *
 *      Start the hardware runtime watchdog.
 *
 * Parameters
 *      None.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int start_runtimewd(void)
{
   int status;
   unsigned int minTimeoutSec = 0;
   unsigned int maxTimeoutSec = 0;
   int watchdogType;
   uint64_t baseAddr;
   unsigned int timeoutSec;

   status = init_runtime_watchdog();

   /*
    * If watchdog option is set on firmware that does not support the protocol
    * then we disable the option and boot without the watchdog.
    */
   if (status != ERR_SUCCESS) {
      Log(LOG_INFO, "Failed to locate runtime watchdog protocol. "
          "Continue boot without watchdog.");
      boot.runtimewd = false;
      return ERR_SUCCESS;
   }

   dump_runtime_watchdog(&minTimeoutSec, &maxTimeoutSec, &watchdogType,
                         &baseAddr);
   if (boot.runtimewd_timeout > 0) {
      Log(LOG_INFO, "Setting runtime watchdog timeout based on cfg: %u seconds",
          boot.runtimewd_timeout);
      timeoutSec = boot.runtimewd_timeout;
   } else {
      Log(LOG_INFO, "Setting runtime watchdog timeout based on max: %u seconds",
          maxTimeoutSec);
      timeoutSec = maxTimeoutSec;
   }
   status = set_runtime_watchdog(timeoutSec);
   if (status != ERR_SUCCESS) {
      return status;
   }

   // TODO: Add a refresh timer here
   return ERR_SUCCESS;
}

#if defined(SECURE_BOOT) && defined(CRYPTO_MODULE)
/*-- load_crypto_module --------------------------------------------------------
 *
 *      Load an EFI driver module with crypto functions for use by Secure Boot.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int load_crypto_module(void)
{
   /*
    * Search in several places for the crypto module.
    *
    * In most cases the crypto module can be found by looking for CRYPTO_DRIVER
    * in the same directory as mboot itself.
    *
    * In the Auto Deploy case, the crypto module has a different directory and
    * base filename.  In this case, boot.crypto (obtained from the key crypto=
    * in boot.cfg) gives the filename.  If boot.crypto is not an absolute
    * pathname, boot.prefix is prepended.
    *
    * When mboot is loaded directly by an iPXE script, it cannot determine what
    * directory it was loaded from.  Some deployment methods place the crypto
    * module in the same directory as the boot modules, while other deployment
    * methods (such as copying an ISO installer image) place the crypto module
    * in a subdirectory named efi/boot.
    */
   int status, final_status;
   unsigned i;
   struct {
      char *dir;
      const char *base;
   } try[] = {
        { NULL /* bootdir */, CRYPTO_DRIVER },     // Normal case
        { boot.prefix, boot.crypto },              // Auto Deploy
        { boot.prefix, CRYPTO_DRIVER },            // iPXE
        { boot.prefix, "efi/boot/"CRYPTO_DRIVER }, // iPXE + ISO copy
   };
   char *modpath;

   status = get_boot_dir(&try[0].dir); // fill in bootdir above
   if (status != ERR_SUCCESS) {
      return status;
   }

   final_status = ERR_UNKNOWN;
   for (i = 0; i < ARRAYSIZE(try); i++) {
      status = make_path(try[i].dir, try[i].base, &modpath);
      if (status != ERR_SUCCESS) {
         Log(LOG_DEBUG, "make_path(%s, %s): %s",
             try[i].dir, try[i].base, error_str[status]);
         if (status > final_status) {
            final_status = status;
         }
         continue;
      }
      status = firmware_file_exec(modpath, "");
      if (status == ERR_SUCCESS || status > final_status) {
         final_status = status;
      }
      if (status != ERR_SUCCESS) {
         /*
          * LOG_DEBUG so that users don't see the failure and fallback.  A more
          * user-friendly message at LOG_INFO or LOG_CRIT is generated below
          * after the whole search succeeds or fails.
          */
         Log(LOG_DEBUG, "%s: %s", modpath, error_str[status]);
         free(modpath);
         continue;
      }
      break;
   }

   if (final_status == ERR_SUCCESS) {
      Log(LOG_INFO, "Loading %s", modpath);
      free(modpath);
   } else {
      Log(LOG_WARNING, "Failed to load %s: %s",
          CRYPTO_DRIVER, error_str[final_status]);
   }
   return final_status;
}
#endif

/*-- main ----------------------------------------------------------------------
 *
 *      ESXBootInfo loader main function.
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
   run_addr_t ebi;
   int status;
#ifdef SECURE_BOOT
   bool crypto_module = false;
#endif

   status = mboot_init(argc, argv);
   if (status != ERR_SUCCESS) {
      return clean(status);
   }

   status = dump_firmware_info();
   if (status != ERR_SUCCESS) {
      return clean(status);
   }

   /*
    * Log a message to show where mboot has been relocated to and
    * loaded, for use in debugging.  Currently the unrelocated value
    * of __executable_start is 0 for COM32, but is 0x1000 for UEFI
    * because of HEADERS_SIZE is uefi/uefi.lds.  Check the symbol
    * table in the .elf binary to be sure.
    */
   Log(LOG_DEBUG, "mboot __executable_start is at %p", __executable_start);

   /*
    * Log the initial state of the firmware memory map.  This logging
    * occurs prior to GUI initialization (causing spew on the screen
    * on some machines), and the log is generally much more than a
    * screenful, so it's disabled by default.  Use the -D and -S flags
    * or a DEBUG build to see the log.
    */
   if (boot.debug) {
      Log(LOG_DEBUG, "Logging initial memory map");
      log_memory_map(&boot.efi_info);
   }

#ifndef __COM32__
   if (boot.no_quirks) {
      Log(LOG_DEBUG, "Skipping quirks...");
   } else {
      Log(LOG_DEBUG, "Processing quirks...");
      check_efi_quirks(&boot.efi_info);
   }

   if ((boot.efi_info.quirks & EFI_FB_BROKEN) != 0) {
      boot.headless = true;
   }
#endif

   Log(LOG_DEBUG, "Processing CPU quirks...");
   check_cpu_quirks();

   tpm_init();

   if (!boot.headless &&
       gui_init() != ERR_SUCCESS) {
      boot.headless = true;
   }

   status = parse_config(boot.cfgfile);
   if (status != ERR_SUCCESS) {
      return clean(status);
   }

   if (boot.runtimewd) {
      Log(LOG_DEBUG, "Initializing hardware runtime watchdog...");
      status = start_runtimewd();

      if (status != ERR_SUCCESS) {
         return clean(status);
      }
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

#ifdef SECURE_BOOT
   boot.efi_info.secure_boot = secure_boot_mode();
   if (boot.efi_info.secure_boot) {
      Log(LOG_INFO, "UEFI Secure Boot in progress");
   } else {
      Log(LOG_INFO, "UEFI Secure Boot is not enabled");
   }
#ifdef CRYPTO_MODULE
   status = load_crypto_module();
   if (status == ERR_SUCCESS) {
      crypto_module = true;
   } else if (boot.efi_info.secure_boot && status != ERR_NOT_FOUND) {
      /*
       * In secure boot mode, fail boot except for the ERR_NOT_FOUND
       * case.  We allow internal crypto in that case to help recover
       * from install/upgrade bug PR 2727885 failing to install the
       * crypto module.
       */
      return clean(status);
   } else {
      Log(LOG_WARNING, "Falling back to internal crypto suite");
   }
#endif
#endif

   status = install_acpi_tables();
   if (status != ERR_SUCCESS) {
      return clean(status);
   }

   status = load_boot_modules();
   if (status != ERR_SUCCESS) {
      return clean(status);
   }

#ifdef SECURE_BOOT
   status = secure_boot_check(crypto_module);
   if (status != ERR_SUCCESS) {
      if (status == ERR_NOT_FOUND) {
         Log(LOG_INFO, "Boot modules are not signed");
      } else if (status == ERR_LOAD_ERROR) {
         Log(LOG_WARNING, "Boot module signatures cannot be checked");
      } else {
         Log(LOG_CRIT, "Boot module signatures are not valid");
      }
      if (boot.efi_info.secure_boot) {
         return clean(ERR_SECURITY_VIOLATION);
      }
   }
#endif

   /* Must be before boot_init, where the event log is captured. */
   if (boot.tpm_measure) {
      status = measure_kernel_options();
      if (status != ERR_SUCCESS) {
         Log(LOG_WARNING, "Failed to measure command line into TPM: %s",
             error_str[status]);
      }

      status = tpm_extend_asset_tag();
      if (status != ERR_SUCCESS) {
         Log(LOG_WARNING, "Failed to measure asset tag into TPM: %s",
             error_str[status]);
      }
   }

   Log(LOG_DEBUG, "Initializing %s standard...",
       boot.is_esxbootinfo ? "ESXBootInfo" : "Multiboot");

   status = boot_init();
   if (status != ERR_SUCCESS) {
      return clean(status);
   }

   firmware_reset_watchdog();

   Log(LOG_INFO, "Shutting down firmware services...");

   log_unsubscribe(firmware_print);

   /* disable syslog buffer and expand the buffer one last time */
   if (expand_size == 0) {
      if (boot.debug) {
         expand_size = SYSLOGBUF_LAST_EXP_SIZE;
      } else {
         expand_size = PAGE_SIZE;
      }
   }
   syslogbuf_expand_disable(expand_size);

   if (firmware_shutdown(&boot.mmap, &boot.mmap_count,
                         &boot.efi_info)                 != ERR_SUCCESS
    || boot_register()                                   != ERR_SUCCESS
    || compute_relocations(boot.mmap, boot.mmap_count)   != ERR_SUCCESS
    || boot_set_runtime_pointers(&ebi)                   != ERR_SUCCESS
    || relocate_runtime_services(&boot.efi_info,
                                 boot.no_rts, boot.no_quirks) != ERR_SUCCESS
    || install_trampoline(&jump_to_trampoline, &handoff) != ERR_SUCCESS) {
      /* Cannot return because Boot Services have been shutdown. */
      Log(LOG_EMERG, "Unrecoverable error");
      PANIC();
   }

   Log(LOG_INFO, "Relocating modules and starting up the kernel...");
   handoff->ebi = ebi;
   handoff->kernel = boot.kernel.entry;
   handoff->ebi_magic = boot.is_esxbootinfo ? ESXBOOTINFO_MAGIC : MBI_MAGIC;
   jump_to_trampoline(handoff);

   NOT_REACHED();

   return ERR_UNKNOWN;
}
