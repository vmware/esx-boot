/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * bootbank.c -- ESXi bootbanks management
 *
 * Bootbank states
 *
 *   BANK_STATE_VALID (0)
 *      This state indicates that a boot bank has been successfully upgraded and
 *      that it is eligible as a boot selection. BANK_STATE_VALID is set by
 *      either the ESXi installer (on fresh install), or by the upgrading tools
 *      (on upgrades).
 *
 *   BANK_STATE_UPGRADING (1)
 *      This state is set by the upgrade tools to indicate that a bootbank has
 *      been upgraded. At the next reboot, safeboot finds the bootbank in the
 *      BANK_STATE_UPGRADING state and updates it to the BANK_STATE_DIRTY state.
 *      If the system boots up properly, upgrade tools will eventually set the
 *      bootbank state to BANK_STATE_VALID; otherwise, the bootbank state will
 *      remain set to BANK_STATE_DIRTY until another reboot occurs.
 *
 *   BANK_STATE_DIRTY (2)
 *      This state is set by safeboot to indicate that it is booting for the
 *      first time on a bootbank which was upgraded on the previous boot (the
 *      bootbank is in the BANK_STATE_UPGRADING state at startup). If the system
 *      boots up successfully, upgrade tools will update the bootbank state to
 *      BANK_STATE_VALID. On boot failure, at the next reboot, safeboot will
 *      find the bootbank in the BANK_STATE_DIRTY state and will consider that
 *      the upgrade has failed, setting the corrupted bootbank state to
 *      BANK_STATE_INVALID.
 *
 *   BANK_STATE_INVALID (3)
 *      This state indicates that a bootbank is either empty or invalid. Such a
 *      bootbank is ignored by safeboot.
 *
 *   BANK_STATE_UNDEFINED (-1)
 *      This state indicates that the 'bootstate' option could not be found. In
 *      this case, the corrupted bootbank is ignored by safeboot.
 */

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <boot_services.h>
#include "safeboot.h"

/*
 * Assume the ESXi image contains at most three bootbank partitions.  Two is
 * normal, but upgrade scenarios can temporarily have three (PR 2449652).
 */
#define BOOTBANKS_NR 3

static bootbank_t banks[BOOTBANKS_NR];

/*-- bootstate_to_str ----------------------------------------------------------
 *
 *      Return a user-friendly boot state info string.
 *
 * Parameters
 *      IN bootstate: the boot state value
 *----------------------------------------------------------------------------*/
const char *bootstate_to_str(int bootstate)
{
   static const char *states[] = {"UNDEFINED", "VALID", "UPGRADING",
                                  "DIRTY", "INVALID", "CORRUPTED"};

   if (bootstate < BANK_STATE_UNDEFINED || bootstate > BANK_STATE_INVALID) {
      bootstate = 4;
   }

   return states[bootstate + 1];
}

/*-- bank_dump -----------------------------------------------------------------
 *
 *      Print boot bank info.
 *
 * Parameters
 *      IN bank: pointer to the boot bank info structure
 *----------------------------------------------------------------------------*/
static void bank_dump(const bootbank_t *bank)
{
   char *uuid;
   int status;

   status = vmfat_uuid_to_str(bank->uuid, &uuid);
   if (status != ERR_SUCCESS) {
      uuid = NULL;
   }

   Log(LOG_DEBUG,
       "BANK%d: state=%s build=%s updated=%u quickboot=%u UUID=%s\n",
       bank->volid, bootstate_to_str(bank->bootstate), bank->build,
       bank->updated, bank->quickboot,
       (uuid != NULL) ? uuid : "(Failed to get UUID)");

   free(uuid);
}

/*-- bank_scan -----------------------------------------------------------------
 *
 *      Get information from the given bootbank, and update its
 *      bootstate if needed.
 *
 * Parameters
 *      IN volid: MBR/GPT partition number of the bootbank to scan
 *      OUT bank: pointer to a bootbank info structure to fill
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int bank_scan(int volid, bootbank_t *bank)
{
   int new_state, status;
   bool is_valid_bootbank;

   ASSERT(volid != FIRMWARE_BOOT_VOLUME);
   ASSERT(bank != NULL);

   /* Initialize bank info */
   memset(bank, 0, sizeof (bootbank_t));
   bank->volid = volid;
   bank->bootstate = BANK_STATE_UNDEFINED;

   /* Get bank UUID */
   status = vmfat_get_uuid(volid, bank->uuid, VMWARE_FAT_UUID_LEN);
   if (status != ERR_SUCCESS) {
      if (status != ERR_NO_MEDIA) { // don't log unused partition table entries
         Log(LOG_DEBUG, "BANK%d: no bank UUID: %s.\n",
             volid, error_str[status]);
      }
      return status;
   }

   is_valid_bootbank = true;

   /* Get bank state & configuration */
   status = bank_get_config(bank);
   if (status != ERR_SUCCESS) {
      Log(LOG_DEBUG, "BANK%d: no valid configuration file.\n", volid);
      return status;
   }

   bank_dump(bank);

   if (bank->updated <= 0) {
      is_valid_bootbank = false;
      Log(LOG_ERR, "BANK%d: invalid update counter.\n", volid);
   }
   if (bank->build == NULL || bank->build[0] == '\0') {
      is_valid_bootbank = false;
      Log(LOG_ERR, "BANK%d: invalid build number.\n", volid);
   }

   /* Update bank state if needed */
   switch (bank->bootstate) {
      case BANK_STATE_UPGRADING:
         bank->upgrading = true;
         new_state = BANK_STATE_DIRTY;
         break;
      case BANK_STATE_DIRTY:
         bank->upgrading = true;
         new_state = BANK_STATE_INVALID;
         break;
      case BANK_STATE_VALID:
      case BANK_STATE_INVALID:
         bank->upgrading = false;
         new_state = bank->bootstate;
         break;
      default:
         Log(LOG_ERR, "BANK%d: invalid boot state.\n", volid);
         is_valid_bootbank = false;
         bank->upgrading = false;
         new_state = BANK_STATE_INVALID;
         break;
   }

   if (bank->quickboot) {
      /* This bootbank was only for one-time use by QuickBoot */
      is_valid_bootbank = false;
      bank->upgrading = false;
      new_state = BANK_STATE_INVALID;
   }

   if (bank->upgrading && !is_valid_bootbank) {
      Log(LOG_ERR, "BANK%d: system has failed to upgrade.\n", volid);
      safeboot.rollback = true;
      new_state = BANK_STATE_INVALID;
   }

   if (new_state != bank->bootstate) {
      status = bank_set_bootstate(bank, new_state);
      if (status != ERR_SUCCESS) {
         gui_resume_default_boot();
         return status;
      }
   }

   if (bank->bootstate != BANK_STATE_VALID &&
       bank->bootstate != BANK_STATE_DIRTY) {
      is_valid_bootbank = false;
   }

   bank->valid = is_valid_bootbank;

   return ERR_SUCCESS;
}

/*-- bank_auto_select ----------------------------------------------------------
 *
 *      Figure the bootbank to boot from. The default is to pick the bank based
 *      on the updated counter. This can be a time, or just a sequence number
 *      for default order.
 *
 * Parameters
 *      OUT bootbank: pointer to the selected boot bank info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int bank_auto_select(bootbank_t **bootbank)
{
   uint32_t max_updates;
   int i, latest;

   ASSERT(bootbank != NULL);

   max_updates = 0;
   latest = -1;

   for (i = 0; i < BOOTBANKS_NR; i++) {
      if (banks[i].valid) {
         if ((max_updates == 0) || (banks[i].updated > max_updates)) {
            max_updates = banks[i].updated;
            latest = i;
         }
      }
   }

   if (latest == -1) {
      return ERR_NOT_FOUND;
   }

   Log(LOG_DEBUG, "BANK%d: default boot bank.\n", banks[latest].volid);

   *bootbank = &banks[latest];

   return ERR_SUCCESS;
}

/*-- bank_kill -----------------------------------------------------------------
 *
 *      Invalidate a bootbank. The boot bank state is turned to
 *      BANK_STATE_INVALID.
 *
 * Parameters
 *      IN bank: pointer to the info structure of the bank to invalidate
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int bank_kill(bootbank_t *bank)
{
   const char *errmsg;
   unsigned int i;
   int nvalid = 0;

   Log(LOG_INFO, "Installed hypervisors:\n\n");
   for (i = 0; i < BOOTBANKS_NR; i++) {
      if (banks[i].valid) {
         nvalid++;
         Log(LOG_INFO, "   BANK%d: %s%s\n", banks[i].volid, banks[i].build,
             banks[i].upgrading ? " (Upgrading...)" :
             ((&banks[i] == bank) ? " (Default)" : ""));
      }
   }
   Log(LOG_INFO, "\n");

   if (nvalid < 2) {
      errmsg = "No alternate hypervisor to roll back to.";
   } else {
      errmsg = NULL;

      if (gui_rollback()) {
         Log(LOG_DEBUG, "Rolling back (invalidating BANK%d).\n", bank->volid);

         if (bank_set_bootstate(bank, BANK_STATE_INVALID) != ERR_SUCCESS) {
            errmsg = "System has failed to roll back.";
         } else {
            bank->valid = false;
            return ERR_SUCCESS;
         }
      } else {
         Log(LOG_DEBUG, "Roll back cancelled by user.\n");
      }
   }

   if (errmsg != NULL) {
      Log(LOG_ERR, "%s\n", errmsg);
      gui_resume_default_boot();
   }

   return ERR_ABORTED;
}

/*-- get_boot_bank -------------------------------------------------------------
 *
 *      List all the bootbanks, and select the default one to boot from.
 *
 * Parameters
 *      IN  shift_r:  if true, the user has requested to kill the current
 *                    default bootbank in order to force a rollback to occur
 *      OUT bootbank: pointer to the default boot bank info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_boot_bank(bool shift_r, bootbank_t **bootbank)
{
   disk_t disk;
   bootbank_t *bank;
   int status;
   int volid;
   int nvols;
   int nfound;

   status = get_boot_disk(&disk);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Error getting boot disk: %d", status);
      return status;
   }

   status = get_max_volume(&disk, &nvols);
   if (status != ERR_SUCCESS) {
      Log(LOG_DEBUG, "Error while getting max volid: %d", status);
   }
   Log(LOG_DEBUG, "max volid = %d", nvols);

   nfound = 0;
   for (volid = 1; volid <= nvols; volid++) {
      status = bank_scan(volid, &banks[nfound]);
      if (status == ERR_SUCCESS) {
         nfound++;
         if (nfound >= BOOTBANKS_NR) {
            break;
         }
      }
   }

   if (shift_r) {
      Log(LOG_DEBUG, "Roll back requested by user.\n");

      status = bank_auto_select(&bank);
      if (status != ERR_SUCCESS) {
         bank_clean();
         return status;
      }

      safeboot.rollback = (bank_kill(bank) == ERR_SUCCESS);
   }

   status = bank_auto_select(&bank);
   if (status != ERR_SUCCESS) {
      bank_clean();
      return status;
   }

   ASSERT(bank->bootstate == BANK_STATE_VALID ||
          bank->bootstate == BANK_STATE_DIRTY);

   *bootbank = bank;

   return ERR_SUCCESS;
}

/*-- bank_clean ----------------------------------------------------------------
 *
 *      Free the memory that was allocated for holding the bootbanks
 *      configuration.
 *----------------------------------------------------------------------------*/
void bank_clean(void)
{
   int i;

   for (i = 0; i < BOOTBANKS_NR; i++) {
      free(banks[i].build);
   }
}
