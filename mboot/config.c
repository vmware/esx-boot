/*******************************************************************************
 * Copyright (c) 2008-2014,2016 VMware, Inc.  All rights reserved.
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
#define MODULE_SEPARATOR        "---"

/*
 * mboot configuration file options.
 */
static option_t mboot_options[] = {
   {"kernel", "=", {NULL}, OPT_STRING},
   {"kernelopt", "=", {NULL}, OPT_STRING},
   {"modules", "=", {NULL}, OPT_STRING},
   {"title", "=", {NULL}, OPT_STRING},
   {"prefix", "=", {NULL}, OPT_STRING},
   {"nobootif", "=", {NULL}, OPT_INTEGER},
   {"timeout", "=", {.integer = 5}, OPT_INTEGER}, // default to 5 seconds
   {"noquirks", "=", {.integer = 0}, OPT_INTEGER},
   {"norts", "=", {.integer = 0}, OPT_INTEGER},
   {NULL, NULL, {NULL}, OPT_INVAL}
};

/*-- config_usage --------------------------------------------------------------
 *
 *      Print the bootloader configuration file syntax.
 *----------------------------------------------------------------------------*/
static void config_usage(void)
{
   Log(LOG_INFO, "Configuration file syntax:\n"
       "\n"
       "   kernel=<FILEPATH>\n"
       "   kernelopt=<OPTIONS_STRING>\n"
       "   modules=<FILEPATH1 --- FILEPATH2... --- FILEPATHn>\n"
       "   title=<TITLE>\n"
       "   prefix=<DIRECTORY>\n"
       "   nobootif=<0...1>\n"
       "   timeout=<SECONDS>"
       "\n"
       "   kernel=<FILEPATH>\n"
       "      Set the kernel path.\n"
       "\n"
       "   kernelopt=<OPTION_STRING>\n"
       "      Append OPTION_STRING to the kernel command line.\n"
       "\n"
       "   modules=<FILEPATH1 --- FILEPATH2... --- FILEPATHn>\n"
       "      List of modules separated by \"---\" (three hyphens).\n"
       "\n"
       "   title=<TITLE>\n"
       "      Set the bootloader banner title.\n"
       "\n"
       "   prefix=<DIRECTORY>\n"
       "      Set the default directory from which the kernel and modules are\n"
       "      loaded (if their paths are relative). By default, prefix is set\n"
       "      to directory where the configuration file is located.\n"
       "\n"
       "   nobootif=<0...1>\n"
       "      If N is set to 1, do not force to append the BOOTIF=<MAC_addr>\n"
       "      option to the kernel command line. By default, nobootif=0.\n"
       "\n"
       "   timeout=<SECONDS>\n"
       "      Set the bootloader autoboot timeout, in units of seconds.\n"
       "      By default, timeout=5.\n"
       "\n"
       "   noquirks=<0...1>\n"
       "      If N is set to 1, disable workarounds for platform quirks. By\n"
       "      default, noquirks=0 unless -Q is on the command line."
       "\n"
       "   norts=<0...1>\n"
       "      If N is set to 1, disable support for UEFI Runtime Services. By\n"
       "      default, norts=0 unless -U is on the command line."
       );
}

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

/*-- find_next_mod -------------------------------------------------------------
 *
 *      Return a pointer to the next module string within the given module list.
 *      The module list is a string formated as follow:
 *
 *      MOD_STR1 --- MOD_STR2... --- MOD_STRn
 *
 * Parameters
 *      IN  mod_list: pointer to the module list
 *      OUT mod_list: pointer to the updated module list (to be passed at next
 *                    call to this function)
 *
 * Results
 *      A pointer to the next module string, or NULL.
 *----------------------------------------------------------------------------*/
static char *find_next_mod(char **mod_list)
{
   char *s, *separator;
   const size_t sep_len = strlen(MODULE_SEPARATOR);

   if (mod_list == NULL || *mod_list == NULL) {
      return NULL;
   }

   for (s = *mod_list; *s != '\0'; s++) {
      if (!isspace(*s)) {
         separator = strstr(s, MODULE_SEPARATOR);

         if (separator == NULL) {
            *mod_list = s + strlen(s);
            return s;
         } else if (separator == s) {
            s += sep_len;
            continue;
         } else {
            *mod_list = separator + sep_len;
            return s;
         }
      }
   }

   *mod_list = s;

   return NULL;
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
   char *delim, *p, *parameter;
   unsigned int i;
   int status;
   char c;

   for (i = 0; ; i++) {
      parameter = find_next_mod(&mod_list);
      if (parameter == NULL) {
         return ERR_SUCCESS;
      }

      delim = strstr(parameter, MODULE_SEPARATOR);
      if (delim == NULL) {
         delim = parameter + strlen(parameter);
      }

      p = parameter;
      while (p < delim && !isspace(*p)) {
         p++;
      }

      c = *p;
      *p = '\0';
      status = make_path(prefix_dir, parameter, &modules[i].filename);
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
         modules[i].options = strdup(p);
         *delim = c;
         if (modules[i].options == NULL) {
            status = ERR_OUT_OF_RESOURCES;
            break;
         }
      }
   }

   do {
      sys_free(modules[i].filename);
      sys_free(modules[i].options);
   } while (i-- > 0);

   return status;
}

/*-- parse_cmdlines ------------------------------------------------------------
 *
 *      Parse the kernel and modules command lines.
 *
 * Parameters
 *      IN prefix_dir: relative paths are relative to there
 *      IN kernel:     path to the kernel
 *      IN options:    option string to append to the kernel command line
 *      IN mod_list:   list of modules separated by "---"
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *
 * Side effects
 *      Update the bootloader info structure with the kernel and modules info.
 *----------------------------------------------------------------------------*/
static int parse_cmdlines(const char *prefix_dir, const char *kernel,
                          const char *options, char *mod_list)
{
   char *kname, *kopts, *m;
   unsigned int count;
   module_t *modules;
   int status;

   status = make_path(prefix_dir, kernel, &kname);
   if (status != ERR_SUCCESS) {
      return status;
   }

   kopts = (options != NULL) ? strdup(options) : NULL;
   if (options != NULL && kopts == NULL) {
      sys_free(kname);
      return ERR_OUT_OF_RESOURCES;
   }

   m = mod_list;
   for (count = 1; find_next_mod(&m) != NULL; count++) {
      ;
   }

   modules = sys_malloc(count * sizeof (module_t));
   if (modules == NULL) {
      sys_free(kname);
      sys_free(kopts);
      return ERR_OUT_OF_RESOURCES;
   }
   memset(modules, 0, count * sizeof (module_t));

   if (count > 1) {
      status = parse_modules(mod_list, prefix_dir, &modules[1]);
      if (status != ERR_SUCCESS) {
         sys_free(kname);
         sys_free(kopts);
         sys_free(modules);
         return status;
      }
   }

   modules[0].filename = kname;
   modules[0].options = kopts;
   boot.modules = modules;
   boot.modules_nr = count;

   return ERR_SUCCESS;
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
            Log(LOG_DEBUG, "MAC address not found. Loading configuration from"
                " %s/%s\n", bootdir, DEFAULT_CFGFILE);
         } else {
            if (asprintf(&relpath, "%s/%s", mac, DEFAULT_CFGFILE) == -1) {
               sys_free(bootdir);
               return ERR_OUT_OF_RESOURCES;
            }

            status = make_path(bootdir, relpath, &cfgpath);
            sys_free(relpath);
            if (status != ERR_SUCCESS) {
               sys_free(bootdir);
               return status;
            }

            status = firmware_file_read(cfgpath, NULL, &buf, &buflen);
            if (status == ERR_SUCCESS) {
               sys_free(buf);
               sys_free(bootdir);
               *path = cfgpath;
               return status;
            } else {
               Log(LOG_DEBUG, "Could not read config file %s. Loading"
                   " configuration from %s/%s\n",
                   cfgpath, bootdir, DEFAULT_CFGFILE);
               sys_free(cfgpath);
            }
         }
      }
   } else {
      bootdir = NULL;
   }

   status = make_path(bootdir, filename, &cfgfile);
   sys_free(bootdir);
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
         sys_free(buf);
      } else {
         Log(LOG_DEBUG, "Could not read config file %s. Loading"
             " configuration from /%s\n", cfgfile, filename);
         sys_free(cfgfile);
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
   char *mod_list, *title, *prefix, *kernel, *kopts;
   char *path = NULL;
   int status;

   status = locate_config_file(filename, &path);
   if (status != ERR_SUCCESS) {
      return status;
   }

   Log(LOG_DEBUG, "Config: %s\n", path);

   status = parse_config_file(boot.volid, path, mboot_options);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Configuration error while parsing %s\n", path);
      sys_free(path);
      if (status == ERR_SYNTAX) {
         config_usage();
      }
      return status;
   }

   kernel   = mboot_options[0].value.str;    /* Kernel name */
   kopts    = mboot_options[1].value.str;    /* Kernel options */
   mod_list = mboot_options[2].value.str;    /* List of modules */
   title    = mboot_options[3].value.str;    /* Title string */
   prefix   = mboot_options[4].value.str;    /* Path prefix */
   if (mboot_options[5].value.integer > 0) { /* no bootif */
      boot.bootif = false;
   }
   boot.timeout = mboot_options[6].value.integer;
   boot.no_quirks |= mboot_options[7].value.integer;
   boot.no_rts |= mboot_options[8].value.integer;

   if (kernel == NULL) {
      config_usage();
      Log(LOG_ERR, "kernel=<FILEPATH> must be set in %s\n", path);
      status = ERR_SYNTAX;
      goto error;
   }

   if (title != NULL) {
      gui_set_title(title);
   }

   if (prefix == NULL) {
      prefix = path;
      status = strip_basename(prefix);
      if (status != ERR_SUCCESS) {
         goto error;
      }
   }

   Log(LOG_DEBUG, "Prefix: %s\n", (prefix[0] != '\0') ? prefix : "(None)");
   status = parse_cmdlines(prefix, kernel, kopts, mod_list);

 error:
   sys_free(path);
   sys_free(mboot_options[0].value.str);   /* Kernel name */
   sys_free(mboot_options[1].value.str);   /* Kernel options */
   sys_free(mboot_options[2].value.str);   /* List of modules */
   sys_free(mboot_options[3].value.str);   /* Title string */
   sys_free(mboot_options[4].value.str);   /* Path prefix */

   if (status == ERR_SUCCESS) {
      status = get_load_size_hint();
      if (status != ERR_SUCCESS) {
         Log(LOG_DEBUG, "The underlying protocol does not report module sizes\n");
         Log(LOG_DEBUG, "Continuing boot process\n");
         status = ERR_SUCCESS;
      }
   }

   return status;
}

/*-- config_clear --------------------------------------------------------------
 *
 *      Clear the kernel/modules information.
 *----------------------------------------------------------------------------*/
void config_clear(void)
{
   while (boot.modules_nr > 0) {
      boot.modules_nr--;
      sys_free(boot.modules[boot.modules_nr].filename);
      sys_free(boot.modules[boot.modules_nr].options);
   }

   sys_free(boot.modules);
   boot.modules = NULL;
   boot.load_size = 0;
}
