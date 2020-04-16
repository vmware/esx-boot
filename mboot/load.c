/*******************************************************************************
 * Copyright (c) 2008-2015,2017-2019 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * load.c -- Kernel/modules loading
 */

#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include <boot_services.h>
#include "mboot.h"
#include <md5.h>

static void load_sanity_check(void)
{
   uint64_t load_size, offset;
   const char *filename, *msg;
   bool error, is_previous_loaded, is_loaded;
   unsigned int i;

   error = false;

   if (boot.modules_nr == 0 || boot.modules == NULL) {
      error = true;
      Log(LOG_ERR, "Nothing to load.\n");
   } else {
      is_previous_loaded = true;
      offset = 0;

      for (i = 0; i < boot.modules_nr; i++) {
         msg = NULL;
         filename = boot.modules[i].filename;
         load_size = boot.modules[i].load_size;
         is_loaded = boot.modules[i].is_loaded;

         if (filename == NULL) {
            msg = "Module has no filename";
         } else if (!is_previous_loaded && is_loaded) {
            msg = "Previous module has been skipped";
         }

         if (msg != NULL) {
            error = true;
            Log(LOG_ERR, "Mod[%u]: %s.\n", i, msg);
         }

         is_previous_loaded = is_loaded;
         offset += load_size;
      }

      if (offset != boot.load_offset) {
         error = true;
         Log(LOG_ERR, "Inconsistent loading offset.\n");
      }
   }

   if (error) {
      Log(LOG_ERR, "Modules are corrupted.\n");
      while (1);
   }
}

/*-- load_callback -------------------------------------------------------------
 *
 *      Increment the load offset with a given amount of freshly loaded memory.
 *      This function is a callback for the file_load() function.
 *
 * Parameters
 *      IN chunk_size: amount of loaded memory, in bytes, since the last call to
 *                     this function
 *
 * Results
 *      ERR_SUCCESS
 *----------------------------------------------------------------------------*/
static int load_callback(size_t chunk_size)
{
   boot.load_offset += chunk_size;
   gui_refresh();

   return ERR_SUCCESS;
}

/*-- get_module_size -----------------------------------------------------------
 *
 *      Get the size of a module.
 *
 *      In some circumstances, it is not possible to determine the full size of
 *      a file without loading it. If that is the case, then this function
 *      will return ERR_UNSUPPORTED.
 *
 * Parameters
 *      IN  n:    module id
 *      OUT size: the module size, in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int get_module_size(unsigned int n, size_t *size)
{
   const char *filepath;
   size_t filesize;
   int status;

  filepath = boot.modules[n].filename;

   status = file_get_size_hint(boot.volid, filepath, &filesize);
   if (status != ERR_SUCCESS) {
      if (status == ERR_NOT_FOUND) {
         Log(LOG_DEBUG, "%s: file not found.\n", filepath);
      } else {
         Log(LOG_DEBUG, "%s: unknown file size.\n", filepath);
      }

      return status;
   }

   *size = filesize;

   return ERR_SUCCESS;
}

/*-- get_load_size_hint --------------------------------------------------------
 *
 *      Get the total size of the data to be loaded.
 *
 *      In some circumstances, it is not possible to determine the full size of
 *      a file without loading it. If that is the case, then this function
 *      will return ERR_UNSUPPORTED.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_load_size_hint(void)
{
   size_t filesize = 0;
   uint64_t bytes;
   unsigned int i;
   int status;

   boot.load_size = 0;
   bytes = 0;

   for (i = 0; i < boot.modules_nr; i++) {
      status = get_module_size(i, &filesize);
      if (status != ERR_SUCCESS) {
         return status;
      }

      bytes += filesize;
   }

   boot.load_size = bytes;

   return ERR_SUCCESS;
}

/*-- unload_boot_modules -------------------------------------------------------
 *
 *      Unload previously loaded boot modules.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
void unload_boot_modules(void)
{
   unsigned int i;

   memset(&boot.kernel, 0, sizeof (kernel_t));

   for (i = 0; i < boot.modules_nr; i++) {
      sys_free(boot.modules[i].addr);
      boot.modules[i].addr = NULL;
      boot.modules[i].load_size = 0;
      boot.modules[i].size = 0;
      boot.modules[i].is_loaded = false;
   }
}

/*-- modify_size_units ----------------------------------------------------------
 *
 *      Converts an input in bytes to a different binary metric based on
 *      its range i.e. to Gb, Mb or Kb.
 *
 * Parameters
 *      IN/OUT size:        Value in bytes, this contains the converted
 *                          value upon return.
 *      OUT size_unit_type: The size units associated with the converted
 *                          value.
 *
 * Results
 *      Returns the size unit type enumeration object, indicating the units
 *      of the size.
 *----------------------------------------------------------------------------*/
static size_unit_t modify_size_units(uint64_t *size)
{
   uint64_t n;

   n = BYTES_TO_GB(*size);
   if (n > 0) {
      *size = n;
      return GIGABYTES;
   }

   n = BYTES_TO_MB(*size);
   if (n > 0) {
      *size = n;
      return MEGABYTES;
   }

   n = BYTES_TO_KB(*size);
   if (n > 0) {
      *size = n;
      return KILOBYTES;
   }

   return BYTES;
}

/*-- get_transfer_bandwidth ---------------------------------------------------
 *
 *      Return the transfer bandwidth when modules are loaded over the network.
 *
 * Parameters
 *      IN size:            amount of loaded memory, in bytes.
 *      IN time:            amount of time taken to load the above
 *                          said memory in milliseconds
 *      OUT bandwidth_unit: bandwidth units i.e. whether
 *                          Gbps/Mbps/Kbps etc
 *
 * Results
 *      The value of time should at least be 1s or greater for this function
 *      to return a bandwidth value, otherwise 0 is returned.
 *---------------------------------------------------------------------------*/
static uint64_t get_transfer_bandwidth(uint64_t size, uint64_t time,
                                       size_unit_t *bandwidth_unit)
{
   uint64_t bandwidth;

   bandwidth = 0;

   if (MILLISEC_TO_SEC_SIGNIFICAND(time) > 0) {
      bandwidth = size / MILLISEC_TO_SEC_SIGNIFICAND(time);
      *bandwidth_unit = modify_size_units(&bandwidth);
   }

   return bandwidth;
}

/*-- log_module_transfer_stats ------------------------------------------------
 *
 *      Log module transfer statistics.
 *
 * Parameters
 *      IN n:  Module ID
 *----------------------------------------------------------------------------*/
static void log_module_transfer_stats(unsigned int n)
{
   const char *path, *pretty_unit_str;
   char *filepath;
   char md5str[MD5_STRING_LEN];
   uint64_t bandwidth, seconds, tenths_of_second, load_size, extracted_size,
            pretty_size;
   size_unit_t bandwidth_unit = UNSUPPORTED_UNIT;
   size_unit_t pretty_unit;
   const module_t *mod;

   mod = &boot.modules[n];
   filepath = strdup(mod->filename);
   path = (filepath == NULL) ? mod->filename : basename(filepath);

   load_size = mod->load_size;
   pretty_size = load_size;
   pretty_unit = modify_size_units(&pretty_size);
   pretty_unit_str = size_unit_to_str(pretty_unit);
   md5_to_str(&mod->md5_compressed, md5str, sizeof(md5str));

   seconds = MILLISEC_TO_SEC_SIGNIFICAND(mod->network_load_time);
   tenths_of_second = MILLISEC_TO_SEC_FRACTIONAL(mod->network_load_time);

   if (boot.is_network_boot) {
      bandwidth = get_transfer_bandwidth(load_size, mod->network_load_time,
                                         &bandwidth_unit);
      if (bandwidth > 0) {
         if (pretty_unit > BYTES) {
            Log(LOG_DEBUG, "%s (MD5: %s): transferred %"PRIu64
                "%s (%"PRIu64" bytes) in"
                " %"PRIu64".%"PRIu64" seconds (%"PRIu64"%s/s)\n",
                path, md5str, pretty_size, pretty_unit_str, load_size, seconds,
                tenths_of_second, bandwidth, size_unit_to_str(bandwidth_unit));
         } else {
            Log(LOG_DEBUG, "%s (MD5: %s): transferred %"PRIu64
                " bytes in %"PRIu64".%"PRIu64""
                " seconds (%"PRIu64"%s/s)\n",
                path, md5str, load_size, seconds, tenths_of_second, bandwidth,
                size_unit_to_str(bandwidth_unit));
         }
      } else {
         if (pretty_unit > BYTES) {
            Log(LOG_DEBUG, "%s (MD5: %s): transferred %"PRIu64
                "%s (%"PRIu64" bytes) in less than 1 second\n",
                path, md5str, pretty_size, pretty_unit_str, load_size);
         } else {
            Log(LOG_DEBUG, "%s (MD5: %s): transferred %"PRIu64
                " bytes in less than 1 second\n", path, md5str, load_size);
         }
      }
   } else {
      if (pretty_unit > BYTES) {
         Log(LOG_DEBUG, "%s (MD5: %s): transferred %"PRIu64
             "%s (%"PRIu64" bytes)\n",
             path, md5str, pretty_size, pretty_unit_str, load_size);
      } else {
         Log(LOG_DEBUG, "%s (MD5: %s): transferred %"PRIu64" bytes\n",
             path, md5str, load_size);
      }
   }

   extracted_size = mod->size;
   pretty_size = extracted_size;
   pretty_unit = modify_size_units(&pretty_size);
   pretty_unit_str = size_unit_to_str(pretty_unit);
   md5_to_str(&mod->md5_uncompressed, md5str, sizeof(md5str));

   if (pretty_unit > BYTES) {
      Log(LOG_DEBUG, "%s (MD5: %s): extracted %"PRIu64"%s (%"PRIu64" bytes)\n",
          path, md5str, pretty_size, pretty_unit_str, extracted_size);
   } else {
      Log(LOG_DEBUG, "%s (MD5: %s): extracted %"PRIu64" bytes\n",
          path, md5str, extracted_size);
   }

   sys_free(filepath);
}

/*-- extract_cksum_module ------------------------------------------------------
 *
 *      Extract and calculate md5 checksums for incoming compressed module
 *
 * Parameters
 *      IN     modulename:       name of the compressed module
 *      IN/OUT buffer:           incoming compressed buffer is replaced with
 *                               newly allocated outgoing uncompressed buffer.
 *                               Incoming buffer is freed in this routine.
 *      IN     isize:            size of incoming compressed buffer
 *      OUT    osize:            size of outgoing uncompressed buffer
 *      OUT    md5_compressed:   MD5 sum of compressed buffer.
 *      OUT    md5_uncompressed: MD5 sum of uncompressed buffer.
 *
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int extract_cksum_module(const char *modname, void **buffer,
                                size_t *bufsize, md5_t *md5_compressed,
                                md5_t *md5_uncompressed)
{
   void *data = NULL;
   size_t size = *bufsize;
   int status;

   md5_compute(*buffer, size, md5_compressed);

   if (!is_gzip(*buffer, size, &status)) {
      return status;
   }

   status = gzip_extract(*buffer, size, &data, &size);
   sys_free(*buffer);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "gzip_extract failed for %s (size %zu): %s\n",
          modname, size, error_str[status]);
      return status;
   }

   md5_compute(data, size, md5_uncompressed);

   *bufsize = size;
   *buffer = data;

   return status;
}

/*-- load_module --------------------------------------------------------------
 *
 *      Load a boot module.
 *
 * Parameters
 *      IN n: module id
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int load_module(unsigned int n)
{
   const char *filepath;
   size_t load_size, size;
   void *addr;
   int status;
   uint64_t start_time, end_time;
   bool is_network_boot;

   is_network_boot = boot.is_network_boot;
   filepath = boot.modules[n].filename;
   Log(LOG_INFO, "Loading %s\n", filepath);

   if (is_network_boot) {
      start_time = firmware_get_time_ms(false);
   }
   status = file_load(boot.volid, filepath,
                      (boot.load_size > 0) ? load_callback : NULL,
                      &addr, &load_size);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (is_network_boot) {
      end_time = firmware_get_time_ms(true);
   }

   /* Boot modules should be in compressed(gzip) format. */
   size = load_size;
   status = extract_cksum_module(filepath, &addr, &size,
                                 &boot.modules[n].md5_compressed,
                                 &boot.modules[n].md5_uncompressed);

   if (status != ERR_SUCCESS) {
      const module_t *mod = &boot.modules[n];
      char md5str[MD5_STRING_LEN];

      md5_to_str(&mod->md5_compressed, md5str, sizeof(md5str));

      if (status == ERR_BAD_TYPE) {
         /*
          * Allow uncompressed modules for Dell, PR 2273023.  Issue a
          * warning, because in other cases if an ESXi bootbank module
          * is not compressed, that generally means it was corrupted
          * and contains garbage.
          */
         Log(LOG_WARNING, "Warning: uncompressed module %s\n", filepath);
         Log(LOG_WARNING, "MD5: %s, size %zu\n", md5str, load_size);
      } else {
         Log(LOG_ERR, "Error %d (%s) while loading module: %s\n",
             status, error_str[status], filepath);
         Log(LOG_ERR, "Compressed MD5: %s\n", md5str);
         md5_to_str(&mod->md5_uncompressed, md5str, sizeof(md5str));
         Log(LOG_ERR, "Decompressed MD5: %s\n", md5str);
         return status;
      }
   }

   if (is_network_boot) {
      boot.modules[n].network_load_time = (end_time > start_time) ?
         (end_time - start_time) : 0;
      boot.network_load_time += boot.modules[n].network_load_time;
   }

   if (n == 0) {
      /*
       * On x86, kernel can be Multiboot or Mutiboot.
       * On AArch64, kernel can only be Mutiboot.
       */
      status = check_mutiboot_kernel(addr, size);
      if (status != ERR_SUCCESS) {
         status = check_multiboot_kernel(addr, size);
         if (status != ERR_SUCCESS) {
            sys_free(addr);
            Log(LOG_ERR, "Error %d (%s) while loading kernel: %s. "
                         "kernel is either invalid or corrupted.\n",
                status, error_str[status], filepath);
            return status;
         } else {
            boot.is_mutiboot = false;
         }
      } else {
         boot.is_mutiboot = true;
      }
   }

   boot.modules[n].addr = addr;
   boot.modules[n].load_size = load_size;
   boot.modules[n].size = size;
   boot.modules[n].is_loaded = true;

   if (boot.load_size == 0) {
      gui_refresh();
   }

   log_module_transfer_stats(n);

   return ERR_SUCCESS;
}

/*-- log_transfer_stats -------------------------------------------------------
 *
 *      Log transfer statistics after all modules have been loaded.
 *
 * Parameters
 *      IN num_modules loaded: Number of modules loaded
 *      IN size_transferred:   Number of bytes transferred in total.
 *      IN size_extracted:     Number of bytes extracted in total.
 *----------------------------------------------------------------------------*/
static void log_transfer_stats(unsigned int num_modules_loaded,
                               uint64_t size_transferred,
                               uint64_t size_extracted)
{
   uint64_t avg_bandwidth, pretty_size;
   uint64_t seconds, tenths_of_second;
   size_unit_t bandwidth_unit = UNSUPPORTED_UNIT;
   size_unit_t pretty_unit;
   const char *pretty_unit_str;

   seconds = MILLISEC_TO_SEC_SIGNIFICAND(boot.network_load_time);
   tenths_of_second = MILLISEC_TO_SEC_FRACTIONAL(boot.network_load_time);

   pretty_size = size_transferred;
   pretty_unit = modify_size_units(&pretty_size);
   pretty_unit_str = size_unit_to_str(pretty_unit);

   Log(LOG_DEBUG, "Loaded %u/%u modules\n",
       num_modules_loaded, boot.modules_nr);

   if (boot.is_network_boot) {
      avg_bandwidth = get_transfer_bandwidth(size_transferred,
                                             boot.network_load_time,
                                             &bandwidth_unit);

      if (avg_bandwidth > 0) {
         Log(LOG_DEBUG, "Total transferred: %"PRIu64
             "%s (%"PRIu64" bytes) in %"PRIu64".%"PRIu64""
             " seconds (average speed %"PRIu64"%s/s)\n",
             pretty_size, pretty_unit_str, size_transferred, seconds,
             tenths_of_second, avg_bandwidth, size_unit_to_str(bandwidth_unit));
      } else {
         Log(LOG_DEBUG, "Total transferred: %"PRIu64
             "%s (%"PRIu64" bytes) in %"PRIu64".%"PRIu64""
             " seconds\n",
             pretty_size, pretty_unit_str, size_transferred, seconds,
             tenths_of_second);
      }
   } else {
      Log(LOG_DEBUG, "Total transferred: %"PRIu64"%s (%"PRIu64" bytes)\n",
          pretty_size, pretty_unit_str, size_transferred);
   }

   pretty_size = size_extracted;
   pretty_unit = modify_size_units(&pretty_size);
   pretty_unit_str = size_unit_to_str(pretty_unit);

   Log(LOG_DEBUG, "Total extracted: %"PRIu64"%s (%"PRIu64" bytes)\n",
       pretty_size, pretty_unit_str, size_extracted);
}

/*-- load_boot_modules----------------------------------------------------------
 *
 *      Load kernel and modules into memory (do not relocate them).
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int load_boot_modules(void)
{
   unsigned int i;
   int status;
   unsigned int num_modules_loaded;
   uint64_t size_transferred, size_extracted;

   boot.load_offset = 0;
   boot.network_load_time = 0;
   num_modules_loaded = 0;
   size_transferred = 0;
   size_extracted = 0;

   for (i = 0; i < boot.modules_nr; i++) {
      if (!boot.modules[i].is_loaded) {
         boot.modules[i].load_size = 0;
         break;
      }
      boot.load_offset += boot.modules[i].load_size;
   }

   load_sanity_check();

   for ( ; i < boot.modules_nr; i++) {
      status = load_module(i);
      if (status != ERR_SUCCESS) {
         return status;
      }
      if (boot.modules[i].is_loaded) {
         num_modules_loaded++;
         size_transferred += boot.modules[i].load_size;
         size_extracted += boot.modules[i].size;
      }
   }

   log_transfer_stats(num_modules_loaded, size_transferred, size_extracted);

   return ERR_SUCCESS;
}
