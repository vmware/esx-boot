/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * config.c -- Kernel/modules configuration parsing
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <boot_services.h>
#include <libgen.h>
#include "mboot.h"

#define DEFAULT_CFGFILE         "boot.cfg"      /* Default configuration file */
#define LISTITEM_SEPARATOR      "---"

/*
 * mboot configuration file options.
 *
 * Note: If you add more options, esximage should be updated to know about
 * them.  Otherwise it will print warnings when upgrading from a boot.cfg file
 * that contains the new options, though it will still copy the options to the
 * new boot.cfg.  See bora/apps/pythonroot/vmware/esximage/Utils/BootCfg.py.
 *
 * Configuration file syntax:
 * kernel=<FILEPATH>
 *    Kernel filename.
 * kernelopt=<OPTION_STRING>
 *    Append OPTION_STRING to kernel command line.
 * modules=<FILEPATH1 --- FILEPATH2... --- FILEPATHn>
 *    Module list separated by \"---\".
 * title=<TITLE>
 *    Bootloader banner title (-t option).
 * prefix=<DIRECTORY>
 *    Directory from which kernel and modules are loaded (if filenames are
 *    relative). Default: directory containing this configuration file.
 * nobootif=<0|1>
 *    1: do not add BOOTIF=<MAC_addr> to kernel command line. Default: 0.
 * timeout=<SECONDS>
 *    Bootloader autoboot timeout, in seconds. Default: 5.
 * noquirks=<0|1>
 *    1: disable workarounds for platform quirks (-Q option). Default: 0.
 * norts=<0|1>
 *    1: disable support for UEFI Runtime Services (-U option). Default: 0.
 * crypto=<FILEPATH>
 *    Crypto module filename.
 * runtimewd=<0|1>
 *    1: Enable hardware runtime watchdog. Default: 0.
 * tftpblksize=<BYTES>
 *    For TFTP transfers, set the blksize option to the given value, default
 *    1468.  UEFI only.
 * acpitables=<FILEPATH1 --- FILEPATH2... --- FILEPATHn>
 *    ACPI table list separated by \"---\".
 * runtimewdtimeout=<SECONDS>
 *    Timeout in seconds before watchdog resets. Default: 0.
 * skip=<0|1>
 *    If skip=1, exit with a fatal error.  If booting from the system's
 *    configured UEFI boot order, the error will cause the UEFI boot manager to
 *    move on and try booting the next boot option in the boot order.
 * errtimeout=<SECONDS>
 *    When a fatal error occurs: if SECONDS >= 0, exit with error status after
 *    SECONDS, or immediately if in headless mode; if SECONDS < 0, hang.
 *    Default: -1 if skip=0, 5 if skip=1.
 */

/* Type  Name               Default value */
#define MBOOT_OPTIONS             \
   STR(  kernel,            NULL) \
   STR(  kernelopt,         NULL) \
   STR(  modules,           NULL) \
   STR(  title,             NULL) \
   STR(  prefix,            NULL) \
   INT(  nobootif,          0)    \
   INT(  timeout,           5)    \
   INT(  noquirks,          0)    \
   INT(  norts,             0)    \
   STR(  crypto,            NULL) \
   INT(  runtimewd,         0)    \
   INT(  tftpblksize,       0)    \
   STR(  acpitables,        NULL) \
   INT(  runtimewdtimeout,  0)    \
   INT(  skip,              0)    \
   INT(  errtimeout,        -1)   \

#define STR(name, defval) \
   { #name, "=", { .str = defval }, OPT_STRING, { .str = NULL } },
#define INT(name, defval) \
   { #name, "=", { .integer = defval }, OPT_INTEGER, { .integer = 0 } },
static option_t mboot_options[] = {
   MBOOT_OPTIONS
   { NULL, NULL, { NULL }, OPT_INVAL, { NULL } }
};
#undef STR
#undef INT

#define STR(name, defval) opt_ ## name,
#define INT(name, defval) opt_ ## name,
enum mboot_optname {
   MBOOT_OPTIONS
};
#undef STR
#undef INT

/*-- append_kernel_options -----------------------------------------------------
 *
 *      Append extra options to the kernel command line.
 *
 * Parameters
 *      IN options: pointer to the extra options string
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int append_kernel_options(const char *options)
{
   char *optline;
   size_t size;

   if (options != NULL && options[0] != '\0') {
      optline = boot.modules[0].options;

      if (optline == NULL) {
         optline = strdup(options);
         if (optline == NULL) {
            return ERR_OUT_OF_RESOURCES;
         }
      } else {
         size = STRSIZE(optline);
         optline = sys_realloc(optline, size, size + strlen(options) + 1);
         if (optline == NULL) {
            return ERR_OUT_OF_RESOURCES;
         }

         strcat(optline, " ");
         strcat(optline, options);
      }

      boot.modules[0].options = optline;
   }

   return ERR_SUCCESS;
}

/*-- measure_kernel_options ----------------------------------------------------
 *
 *      Measure the kernel command line into the TPM. This should be
 *      called once after the command line is fully formed.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int measure_kernel_options(void)
{
   return tpm_extend_cmdline(boot.modules[0].filename, boot.modules[0].options);
}

/*-- find_next_listitem --------------------------------------------------------
 *
 *      Return a pointer to the next list item string within the given list.
 *      The list is a string formated as follow:
 *
 *      STR1 --- STR2... --- STRn
 *
 * Parameters
 *      IN  list: pointer to the item list
 *      OUT list: pointer to the updated item list (to be passed at next
 *                call to this function)
 *
 * Results
 *      A pointer to the next item string, or NULL.
 *----------------------------------------------------------------------------*/
static char *find_next_listitem(char **list)
{
   char *s, *separator;
   const size_t sep_len = strlen(LISTITEM_SEPARATOR);

   if (list == NULL || *list == NULL) {
      return NULL;
   }

   for (s = *list; *s != '\0'; s++) {
      if (!isspace(*s)) {
         separator = strstr(s, LISTITEM_SEPARATOR);

         if (separator == NULL) {
            *list = s + strlen(s);
            return s;
         } else if (separator == s) {
            s += sep_len;
            continue;
         } else {
            *list = separator + sep_len;
            return s;
         }
      }
   }

   *list = s;

   return NULL;
}

/*-- parse_filelist ------------------------------------------------------------
 *
 *      Parse the list of files and populate the descriptors table.
 *
 * Parameters
 *      IN list:       list of files separated by "---"
 *      IN prefix_dir: relative paths are relative to there
 *      IN context:    callback context
 *      IN setitem:    callback to set filename and options for a list item
 *      IN clearitem:  callback to free filename and options for a list item
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int parse_filelist(char *list, const char *prefix_dir, void *context,
                          void (*setitem)(void *, unsigned int, char *, char *),
                          void (*clearitem)(void *, unsigned int))
{
   char *delim, *p, *parameter, *filename, *options;
   unsigned int i;
   int status;
   char c;

   for (i = 0; ; i++) {
      parameter = find_next_listitem(&list);
      if (parameter == NULL) {
         return ERR_SUCCESS;
      }

      delim = strstr(parameter, LISTITEM_SEPARATOR);
      if (delim == NULL) {
         delim = parameter + strlen(parameter);
      }

      p = parameter;
      while (p < delim && !isspace(*p)) {
         p++;
      }

      c = *p;
      *p = '\0';
      status = make_path(prefix_dir, parameter, &filename);
      *p = c;
      if (status != ERR_SUCCESS) {
         break;
      }

      while (p < delim && isspace(*p)) {
         p++;
      }
      while (delim > p && isspace(*(delim - 1))) {
         delim--;
      }

      if (delim > p) {
         c = *delim;
         *delim = '\0';
         options = strdup(p);
         *delim = c;
         if (options == NULL) {
            free(filename);
            status = ERR_OUT_OF_RESOURCES;
            break;
         }
      } else {
         options = NULL;
      }

      setitem(context, i, filename, options);
   }

   do {
      clearitem(context, i);
   } while (i-- > 0);

   return status;
}

/*-- parse_modules_setitem -----------------------------------------------------
 *
 *      Populate a single module descriptor table entry.
 *
 * Parameters
 *      IN context:    pointer to the module info array
 *      IN index:      table entry offset
 *      IN filename:   module filename
 *      IN options:    module options
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static void parse_modules_setitem(void *context, unsigned int index,
                                  char *filename, char *options)
{
   module_t *modules = context;

   modules[index].filename = filename;
   modules[index].options = options;
}

/*-- parse_modules_clearitem ---------------------------------------------------
 *
 *      Free resources and reset a single module descriptor table entry.
 *
 * Parameters
 *      IN context:    pointer to the module info array
 *      IN index:      table entry offset
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static void parse_modules_clearitem(void *context, unsigned int index)
{
   module_t *modules = context;

   free(modules[index].filename);
   free(modules[index].options);
   modules[index].filename = NULL;
   modules[index].options = NULL;
}

/*-- parse_modules -------------------------------------------------------------
 *
 *      Parse the modules list and populate the module descriptors table.
 *
 * Parameters
 *      IN mod_list:   list of modules separated by "---"
 *      IN prefix_dir: relative paths are relative to there
 *      IN modules:    pointer to the module info array
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int parse_modules(char *mod_list, const char *prefix_dir,
                         module_t *modules)
{
   return parse_filelist(mod_list, prefix_dir, modules,
                         parse_modules_setitem, parse_modules_clearitem);
}

/*-- parse_acpitab_setitem -----------------------------------------------------
 *
 *      Populate a single acpitab descriptor table entry.
 *
 * Parameters
 *      IN context:    pointer to the acpitab info array
 *      IN index:      table entry offset
 *      IN filename:   ACPI table filename
 *      IN options:    ignored
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static void parse_acpitab_setitem(void *context, unsigned int index,
                                  char *filename, char *options)
{
   acpitab_t *acpitab = context;

   acpitab[index].filename = filename;
   free(options);
}

/*-- parse_acpitab_clearitem ---------------------------------------------------
 *
 *      Free resources and reset a single acpitab descriptor table entry.
 *
 * Parameters
 *      IN context:    pointer to the acpitab info array
 *      IN index:      table entry offset
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static void parse_acpitab_clearitem(void *context, unsigned int index)
{
   acpitab_t *acpitab = context;

   free(acpitab[index].filename);
   acpitab[index].filename = NULL;
}

/*-- parse_acpitab -------------------------------------------------------------
 *
 *      Parse the acpitab list and populate the acpitab descriptors table.
 *
 * Parameters
 *      IN acpitab_list: list of ACPI tables separated by "---"
 *      IN prefix_dir:   relative paths are relative to there
 *      IN acpitab:      pointer to the acpitab info array
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int parse_acpitab(char *acpitab_list, const char *prefix_dir,
                         acpitab_t *acpitab)
{
   return parse_filelist(acpitab_list, prefix_dir, acpitab,
                         parse_acpitab_setitem, parse_acpitab_clearitem);
}

/*-- parse_cmdlines ------------------------------------------------------------
 *
 *      Parse the kernel, modules, and ACPI table command lines.
 *
 * Parameters
 *      IN prefix_dir:    relative paths are relative to there
 *      IN kernel:        path to the kernel
 *      IN options:       option string to append to the kernel command line
 *      IN mod_list:      list of modules separated by "---"
 *      IN acpitab_list:  list of ACPI tables separated by "---"
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *
 * Side effects
 *      Update the bootloader info structure with the kernel and modules info.
 *----------------------------------------------------------------------------*/
static int parse_cmdlines(const char *prefix_dir, const char *kernel,
                          const char *options, char *mod_list,
                          char *acpitab_list)
{
   char *m;
   char *kname = NULL, *kopts = NULL;
   unsigned int mod_count, acpitab_count;
   module_t *modules = NULL;
   acpitab_t *acpitab = NULL;
   int status;

   status = make_path(prefix_dir, kernel, &kname);
   if (status != ERR_SUCCESS) {
      goto error;
   }

   kopts = (options != NULL) ? strdup(options) : NULL;
   if (options != NULL && kopts == NULL) {
      status = ERR_OUT_OF_RESOURCES;
      goto error;
   }

   m = mod_list;
   for (mod_count = 1; find_next_listitem(&m) != NULL; mod_count++) {
      ;
   }

   m = acpitab_list;
   for (acpitab_count = 0; find_next_listitem(&m) != NULL; acpitab_count++) {
      ;
   }

   modules = malloc(mod_count * sizeof (module_t));
   if (modules == NULL) {
      status = ERR_OUT_OF_RESOURCES;
      goto error;
   }
   memset(modules, 0, mod_count * sizeof (module_t));

   if (mod_count > 1) {
      status = parse_modules(mod_list, prefix_dir, &modules[1]);
      if (status != ERR_SUCCESS) {
         goto error;
      }
   }

   if (acpitab_count > 0) {
      acpitab = malloc(acpitab_count * sizeof (acpitab_t));
      if (acpitab == NULL) {
         status = ERR_OUT_OF_RESOURCES;
         goto error;
      }
      memset(acpitab, 0, acpitab_count * sizeof (acpitab_t));

      status = parse_acpitab(acpitab_list, prefix_dir, acpitab);
      if (status != ERR_SUCCESS) {
         goto error;
      }
   }

   modules[0].filename = kname;
   modules[0].options = kopts;
   boot.modules = modules;
   boot.modules_nr = mod_count;
   boot.acpitab = acpitab;
   boot.acpitab_nr = acpitab_count;

   return ERR_SUCCESS;

 error:
   free(kname);
   free(kopts);
   free(modules);
   free(acpitab);

   return status;
}

/*-- strip_basename ------------------------------------------------------------
 *
 *      Returns the string up to, but not including, the last '/' delimiters.
 *
 * Parameters
 *      IN filepath: pointer to the file path
 *
 * Results
 *      A pointer to the output directory path.
 *
 * Side Effects
 *      Input file path is modified in place.
 *----------------------------------------------------------------------------*/
static int strip_basename(char *filepath)
{
   char *p;

   if (filepath == NULL || *filepath == '\0') {
      return ERR_INVALID_PARAMETER;
   }

   p = filepath + strlen(filepath) - 1;
   if (*p == '/') {
      /* filepath points to a directory */
      return ERR_INVALID_PARAMETER;
   }

   while (p >= filepath && *p != '/') {
      p--;
   }
   while (p >= filepath && *p == '/') {
      p--;
   }

   p[1] = '\0';

   return ERR_SUCCESS;
}

/*-- locate_config_file --------------------------------------------------------
 *
 *      Returns an absolute path to mboot's configuration file. By default, the
 *      configuration file is named 'boot.cfg' and is located in the boot
 *      directory. The default file path can be changed to <Path_To_CfgFile>
 *      with the '-c <Path_To_CfgFile>' command line option. If
 *      <Path_To_CfgFile> is a relative path, it is interpreted relatively to
 *      the boot directory.
 *
 * Parameters
 *      IN  filename: <Path_To_CfgFile>, or NULL for the default configuration
 *      OUT path:     absolute path to mboot's configuration file
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int locate_config_file(const char *filename, char **path)
{
   char *relpath, *cfgpath, *bootdir, *cfgfile;
   const char *mac;
   void *buf;
   size_t buflen;
   int status;
   bool use_default_config = false;

   if (filename == NULL || filename[0] == '\0') {
      filename = DEFAULT_CFGFILE;
      use_default_config = true;
   }

   if (!is_absolute(filename)) {
      status = get_boot_dir(&bootdir);
      if (status != ERR_SUCCESS) {
         return status;
      }

      if (is_network_boot() && use_default_config) {
         status = get_mac_address(&mac);
         if (status != ERR_SUCCESS) {
            Log(LOG_DEBUG, "MAC address not found");
         } else {
            if (asprintf(&relpath, "%s/%s", mac, DEFAULT_CFGFILE) == -1) {
               free(bootdir);
               return ERR_OUT_OF_RESOURCES;
            }

            status = make_path(bootdir, relpath, &cfgpath);
            free(relpath);
            if (status != ERR_SUCCESS) {
               free(bootdir);
               return status;
            }

            status = firmware_file_read(cfgpath, NULL, &buf, &buflen);
            if (status == ERR_SUCCESS) {
               free(buf);
               free(bootdir);
               *path = cfgpath;
               return status;
            } else {
               Log(LOG_DEBUG, "Could not read config from %s: %s",
                   cfgpath, error_str[status]);
               free(cfgpath);
            }
         }
      }
   } else {
      bootdir = NULL;
   }

   status = make_path(bootdir, filename, &cfgfile);
   free(bootdir);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (!is_absolute(filename) && !use_default_config) {
      /*
       * Backward compatibility workaround.  Old versions of the
       * bootloader could not always determine the boot directory and
       * in those cases would interpret a relative -c argument as
       * absolute.  To support old use cases that relied on that bug,
       * if the file can't be found relative to the boot directory,
       * interpret the name as absolute instead.
       */
      status = firmware_file_read(cfgfile, NULL, &buf, &buflen);
      if (status == ERR_SUCCESS) {
         free(buf);
      } else {
         Log(LOG_DEBUG, "Could not read config from %s: %s",
             cfgfile, error_str[status]);
         free(cfgfile);
         status = make_path("/", filename, &cfgfile);
         if (status != ERR_SUCCESS) {
            return status;
         }
      }
   }

   *path = cfgfile;

   return ERR_SUCCESS;
}

/*-- parse_config --------------------------------------------------------------
 *
 *      Parse the bootloader configuration file.
 *
 * Parameters
 *      IN filename: configuration file filename
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int parse_config(const char *filename)
{
   char *mod_list, *acpitab_list, *title, *prefix, *kernel, *kopts;
   char *path = NULL;
   int status;
   bool skip = false;

   status = locate_config_file(filename, &path);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Could not locate config file %s: %s",
          filename, error_str[status]);
      return status;
   }

   Log(LOG_INFO, "Loading %s", path);

   status = parse_config_file(boot.volid, path, mboot_options);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Configuration error while parsing %s", path);
      free(path);
      return status;
   }

   kernel   = mboot_options[opt_kernel].value.str;
   kopts    = mboot_options[opt_kernelopt].value.str;
   mod_list = mboot_options[opt_modules].value.str;
   title    = mboot_options[opt_title].value.str;
   prefix   = mboot_options[opt_prefix].value.str;
   if (mboot_options[opt_nobootif].value.integer > 0) {
      boot.bootif = false;
   }
   boot.timeout = mboot_options[opt_timeout].value.integer;
   boot.no_quirks |= mboot_options[opt_noquirks].value.integer;
   boot.no_rts |= mboot_options[opt_norts].value.integer;
   boot.crypto = mboot_options[opt_crypto].value.str;
   boot.runtimewd = mboot_options[opt_runtimewd].value.integer;
   if (mboot_options[opt_tftpblksize].value.integer != 0) {
      tftp_set_block_size(mboot_options[opt_tftpblksize].value.integer);
   }
   acpitab_list = mboot_options[opt_acpitables].value.str;
   boot.runtimewd_timeout = mboot_options[opt_runtimewdtimeout].value.integer;
   skip = mboot_options[opt_skip].value.integer;
   if (mboot_options[opt_errtimeout].value.integer !=
       mboot_options[opt_errtimeout].default_value.integer) {
      boot.err_timeout = mboot_options[opt_errtimeout].value.integer;
   }

   if (skip && boot.err_timeout == -1) {
      boot.err_timeout = 5;
   }

   if (title != NULL) {
      gui_set_title(title);
      gui_refresh();
   }

   if (skip) {
      Log(LOG_WARNING, "Skipping boot from this configuration");
      status = ERR_ABORTED;
      goto error;
   }

   if (kernel == NULL) {
      Log(LOG_ERR, "kernel=<FILEPATH> must be set");
      status = ERR_SYNTAX;
      goto error;
   }

   if (prefix == NULL) {
      prefix = path;
      status = strip_basename(prefix);
      if (status != ERR_SUCCESS) {
         goto error;
      }
   }

   Log(LOG_DEBUG, "Prefix: %s", (prefix[0] != '\0') ? prefix : "(None)");
   boot.prefix = prefix;
   status = parse_cmdlines(prefix, kernel, kopts, mod_list, acpitab_list);

 error:
   if (boot.prefix != path) {
      free(path);   // Only free if prefix wasn't derived from path.
   }
   free(kernel);
   free(kopts);
   free(mod_list);
   free(title);
   free(acpitab_list);

   if (status == ERR_SUCCESS) {
      status = get_load_size_hint();
      if (status != ERR_SUCCESS) {
         Log(LOG_DEBUG, "The underlying protocol does not report module sizes");
         Log(LOG_DEBUG, "Continuing boot process");
         status = ERR_SUCCESS;
      }
   }

   return status;
}

/*-- config_clear --------------------------------------------------------------
 *
 *      Clear the kernel/modules/ACPI table information.
 *----------------------------------------------------------------------------*/
void config_clear(void)
{
   while (boot.modules_nr > 0) {
      boot.modules_nr--;
      free(boot.modules[boot.modules_nr].filename);
      free(boot.modules[boot.modules_nr].options);
   }

   free(boot.modules);
   boot.modules = NULL;

   while (boot.acpitab_nr > 0) {
      boot.acpitab_nr--;
      free(boot.acpitab[boot.acpitab_nr].filename);
   }

   free(boot.acpitab);
   boot.acpitab = NULL;

   boot.load_size = 0;
}
