/*******************************************************************************
 * Copyright (c) 2015-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * menu.c --
 *
 *      Interpret a menu and chain to another uefi bootloader or app.
 *
 *      Usage: menu.efi [options] [menufile]
 *
 *      Options:
 *         -S <1...4>     Set the default serial port (1=COM1, 2=COM2, 3=COM3,
 *                        4=COM4, 0xNNNN=hex I/O port address ).  If present,
 *                        all log messages are sent to the serial port.
 *         -s <BAUDRATE>  Set the serial port speed to BAUDRATE (in bits per
 *                        second).  Default 115200.
 *         -V             Enable verbose mode.  Causes all log messages to be
 *                        sent to the screen.  Without this option only
 *                        LOG_INFO and below are sent to the screen.
 *         -H <DIR>       Set home directory to <DIR>.  Filenames that are
 *                        neither absolute paths nor URLs are interepreted
 *                        relative to this directory.
 *         -h             Set home directory to the empty string.
 *         -D <N>         Set debug flag bits to N.  Bits include:
 *                        1: Wait for keypress before starting an app.
 *                        2: Fail with a syntax error on unrecognized keywords.
 *                        4: Wait for keypress before parsing menu.
 *                        8: Wait for keypress before displaying menu.
 *                        16: Wait for keypress before reading menu.
 *                        32: Wait for keypress before trying each location.
 *         -b <BLKSIZE>   For TFTP transfers, set the blksize option to the
 *                        given value, default 1468.
 *         -E <SECONDS>   Error timeout.  If an error occurs, such as when a
 *                        program that is chained to fails to start or exits
 *                        with an error, wait the given number of seconds for
 *                        the user to press a key before returning the error
 *                        back to the boot manager.  Default: 30.
 *         -q <key=value> Add provided data as a query string parameter to be
 *                        used each time a file is retrieved over HTTP(S).
 *                        This option may be provided multiple times to add
 *                        multiple parameters to the query string.
 *         -C             Collect SMBIOS data and add it to the query string.
 *                        By default this is disabled.
 *
 *      If no menufile argument is provided, the menu is searched for in the
 *      following locations:
 *
 *          <HOMEDIR>/<MAC>
 *          <HOMEDIR>/menuefi.d/<MAC>
 *          <HOMEDIR>/pxelinux.cfg/<MAC>
 *          <HOMEDIR>/default
 *          <HOMEDIR>/menuefi.d/default
 *          <HOMEDIR>/pxelinux.cfg/default
 *
 *      where by default <HOMEDIR> is the directory that menu.efi itself was
 *      loaded from, and <MAC> is the MAC address of the NIC it was loaded via.
 *      The syntax for <MAC> is "xx-aa-bb-cc-dd-ee-ff", where xx is the
 *      Hardware Type Number of the boot interface (see RFC 1700, usually 01)
 *      and the rest is its MAC address, using lowercase hex digits separated
 *      by hyphens (not colons).  <HOMEDIR> can be changed by the menu or a
 *      command-line option.
 *
 *      If a menufile argument is provided, some searching is still done.  In
 *      fact, this searching is done for all file loading, including uefi apps
 *      to chain to.  If the filename is an absolute pathname (starts with "/")
 *      or appears to be a URL (contains ://), the sequence is as follows:
 *
 *          filename
 *          <HOMEDIR>/menuefi.d/basename(filename)
 *          <HOMEDIR>/pxelinux.cfg/basename(filename)
 *
 *     If the filename is relative, the sequence is:
 *
 *          <HOMEDIR>/filename
 *          <HOMEDIR>/menuefi.d/basename(filename)
 *          <HOMEDIR>/pxelinux.cfg/basename(filename)
 *
 *      Some of the above searching is arguably overkill.
 *
 *      The following syntax is supported, where n, p, or b is a numeric
 *      argument, s is a string argument terminated by end of line, w is a
 *      string argument terminated by the next whitespace, and ... is one or
 *      more lines of text.  This is a subset of syslinux config file syntax
 *      (https://wiki.syslinux.org/wiki/index.php?title=Config) plus syslinux
 *      simple menu system syntax
 *      (https://wiki.syslinux.org/wiki/index.php?title=Menu), with extensions.
 *      In particular, the EFI keyword does not exist in syslinux.  This allows
 *      using the same menu with both menu.efi and pxelinux, as pxelinux
 *      ignores lines starting with an unknown keyword.  Although the keywords
 *      are shown here in all caps, they are case insensitive.
 *
 *      #s                - Comment.
 *      DEFAULT s         - Ignored if any MENU keywords occur in the file.
 *                          Otherwise gives the label of the default item
 *                          or default command line to chain to.
 *      TIMEOUT n         - Automatically boot default item in n/10 seconds.
 *      NOHALT n          - Ignored.
 *      PROMPT n          - Ignored.
 *      MENU TITLE s      - Give the menu a title.
 *      MENU HIDDEN       - Don't display the menu until a key is pressed.
 *      EFI DEBUG n       - Set debug flags to n.
 *      EFI SERIAL p b    - Debug log to serial port p, baud b (both optional).
 *      EFI VERBOSE n     - Show Log(LOG_DEBUG, ...) messages on screen.
 *      EFI HOMEDIR s     - Set the directory for interpreting relative paths.
 *                          If s is omitted, set HOMEDIR to the empty string.
 *      EFI HTTP s        - Evaluate s if HTTP loading is available.
 *      EFI NOHTTP s      - Evaluate s if HTTP loading is not available.
 *      EFI TFTPBLKSIZE n - Set the TFTP blksize option, as with -b.
 *      EFI ERRTIMEOUT n  - Set the error timeout, as with -E.
 *      EFI ARCH w s      - Evaluate s if the current architecture is w;
 *                          either arm64, riscv64, x86 (64-bit), or x86_32.
 *      EFI NOARCH w s    - Evaluate s if the current architecture is not w.
 *      EFI s             - Evaluate s.  Useful when sharing a menu between
 *                          legacy BIOS (pxelinux) and UEFI (menu.efi) booting
 *                          where some differences are needed between the two
 *                          cases, because pxelinux ignores lines starting
 *                          with an unknown keyword.
 *
 *      LABEL s           - Starts and names a menu item.  The following
 *                          keywords are recognized only within items.
 *      KERNEL s          - The EFI app (possibly with arguments) to chain to.
 *      APPEND s          - Command line arguments (added after any in KERNEL).
 *      IPAPPEND n        - Ignored.
 *      LOCALBOOT n       - Return from the menu program, with failure if
 *                          n = -1, success if n = -2.  Other values of n are
 *                          reserved but treated as -1 for now.  If menu.efi
 *                          was invoked directly by the UEFI boot manager, -1
 *                          should go on to the next boot option in the UEFI
 *                          boot order, while -2 should stop in the UEFI UI.
 *      CONFIG s          - Restart with file s as the menu.
 *      MENU HIDE         - Don't display this item.
 *      MENU LABEL s      - Display this string instead of the item's label.
 *      MENU DEFAULT      - Make this item the default.
 *      MENU SEPARATOR    - Display a blank line under this item.
 *      TEXT HELP ...     - Display text up to ENDTEXT while item is selected.
 *      ENDTEXT           - Terminates TEXT HELP.
 *
 *      If multiple KERNEL (or EFI KERNEL) options are given in an item, only
 *      the last is effective, and similarly for APPEND (or EFI APPEND).  If
 *      multiple DEFAULT or MENU DEFAULT options are given, the last of each is
 *      effective.  Used together with the "EFI s" syntax, these features can
 *      be handy to make the same menu behave differently with pxelinux versus
 *      menu.efi.
 *
 *      Certain command names are handled specially when chaining:
 *
 *      * A .c32 or .0 extension is automatically changed to .efi.
 *
 *      * menu.efi (or menu.c32) restarts the same instance of menu.efi with
 *        its argument as the menu, instead of loading a new instance.  This is
 *        essentially just an optimization.
 *
 *      * if{gpxe,vm,ver410}.c32 expects arguments of the form "s1 -- s2".  It
 *        executes s1 as either a label or command line.  s2 is ignored.  This
 *        kludge helps deal with some existing pxelinux menus used at VMware,
 *        where an "if*.c32" program is used to continue with the current
 *        version of pxelinux in the "then" (s1) case or chainload a different
 *        version of pxelinux and restart in the "else" (s2) case.
 */

#include <efiutils.h>
#include <bootlib.h>
#include <boot_services.h>
#include <ctype.h>
#include <stdlib.h>
#include <error.h>
#include <libgen.h>
#include <unistd.h>
#include <compat.h>
#include <md.h>

/*
 * Hash computed by elf2efi.  (This feature was designed for the
 * crypto module, for use in a runtime integrity check that is
 * required by FIPS, but menu.efi simply uses it as a version stamp.)
 */
#define HASH_SIZE MBEDTLS_MD_MAX_SIZE
uint8_t _expected_hash[HASH_SIZE]
   __attribute__ ((section (".integrity"))) = { 0xff, 0, /*...*/ };

/*
 * HMAC key used for the hash.  Randomly generated.
 *
 * 03c405aedf13a33d6cdb54f69bf793260f8d8b2df8e54f42e49b9b9a31621d9c
 * fc6ff1afac094f71ef756c2ecd78f634ac14f7f15da5c1ff1735169e4963dec6
 */
const uint8_t _hmac_key[HASH_SIZE] = {
   0x03, 0xc4, 0x05, 0xae, 0xdf, 0x13, 0xa3, 0x3d, 0x6c, 0xdb, 0x54, 0xf6,
   0x9b, 0xf7, 0x93, 0x26, 0x0f, 0x8d, 0x8b, 0x2d, 0xf8, 0xe5, 0x4f, 0x42,
   0xe4, 0x9b, 0x9b, 0x9a, 0x31, 0x62, 0x1d, 0x9c, 0xfc, 0x6f, 0xf1, 0xaf,
   0xac, 0x09, 0x4f, 0x71, 0xef, 0x75, 0x6c, 0x2e, 0xcd, 0x78, 0xf6, 0x34,
   0xac, 0x14, 0xf7, 0xf1, 0x5d, 0xa5, 0xc1, 0xff, 0x17, 0x35, 0x16, 0x9e,
   0x49, 0x63, 0xde, 0xc6
};

static char welcome[100];

#define LOCALBOOT_NONE 0xb0091e
#define TIMEOUT_INFINITE 30000

typedef struct MenuItem {
   struct MenuItem *next;
   struct MenuItem *prev;
   char *label;
   char *display;
   char *kernel;
   char *append;
   bool hide;
   bool spaceAfter;
   int ipappend;
   int localboot;
   int recurse;
   char *help;
} MenuItem;

typedef struct {
   // Menu contents
   bool menumode;
   bool hidden;
   char *title;
   char *deflabel;
   MenuItem *defitem;
   MenuItem *first;
   MenuItem *last;
   int timeout;
   int nohalt;
   int prompt;

   // State while parsing menu
   const char *filename;
   char *buffer;
   size_t bufsize;
   char *parse;
   size_t remaining;
} Menu;

int volid = 0;
char *homedir;
char *menuefi_d;
char *pxelinux_cfg;
Menu *rootmenu;
EFI_HANDLE Volume;
int err_timeout = 30; //seconds

/*
 * Debug bit flags
 */
#define DEBUG_PAUSE_BEFORE_START_IMAGE 1
#define DEBUG_STRICT_SYNTAX            2
#define DEBUG_PAUSE_BEFORE_PARSE       4
#define DEBUG_PAUSE_BEFORE_DISPLAY     8
#define DEBUG_PAUSE_BEFORE_READ       16
#define DEBUG_PAUSE_BEFORE_READ_TRY   32
unsigned debug = 0;
bool verbose = false;

const char bios_version_name[] = "bios-version";
const char* bios_version = NULL;

const char vendor_name[] = "vendor";
const char *vendor = NULL;

const char model_name[] = "model";
const char *model = NULL;

const char family_name[] = "family";
const char *family = NULL;

const char serial_number_name[] = "serial-number";
const char *serial_number = NULL;

const char system_uuid_name[] = "system-uuid";
const uint8_t *system_uuid = NULL;
char *system_uuid_str = NULL;

const char mac_name[] = "boot-ift-mac";
const char *mac_str = NULL;

int smbios_version_major = 0;
int smbios_version_minor = 0;
int simbios_version_doc_rev = 0;

oem_strings_t oem_strings;
bool collect_and_use_smbios_data = false;

int do_menu(const char *filename);
int do_item(Menu *menu, MenuItem *item);
int chain_to(const char *program, const char *arguments);
void set_verbose(bool verbose);

static unsigned int query_string_arguments_nr = 0;
static char **query_string_arguments = NULL;

/*-- set_homedir ---------------------------------------------------------------
 *
 *      Set home directory.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int set_homedir(const char *dir)
{
   int status;
   char *h, *m, *p;

   h = strdup(dir);
   if (h == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }
   file_sanitize_path(h);

   status = make_path(h, "menuefi.d", &m);
   if (status != ERR_SUCCESS) {
      free(h);
      return status;
   }

   status = make_path(h, "pxelinux.cfg", &p);
   if (status != ERR_SUCCESS) {
      free(h);
      free(m);
      return status;
   }

   if (homedir != NULL) {
      free(homedir);
   }
   if (menuefi_d != NULL) {
      free(menuefi_d);
   }
   if (pxelinux_cfg != NULL) {
      free(pxelinux_cfg);
   }

   homedir = h;
   menuefi_d = m;
   pxelinux_cfg = p;

   return ERR_SUCCESS;
}

/*-- await_keypress ------------------------------------------------------------
 *
 *      Prompt and wait for a keypress
 *
 * Results
 *      ERR_SUCCESS, ERR_ABORTED, or ERR_TIMEOUT
 *----------------------------------------------------------------------------*/
int await_keypress(void)
{
   key_code_t key;

   Log(LOG_NOTICE, "Press Enter to continue or Backspace to abort...");
   for (;;) {
      kbd_waitkey_timeout(&key, 300);
      if (key.sym == KEYSYM_NONE) {
         return ERR_TIMEOUT;
      } else if (key.sym == KEYSYM_ASCII) {
         if (key.ascii == '\b' || key.ascii == '\177') {
            return ERR_ABORTED;
         } else if (key.ascii == '\r' || key.ascii == '\n') {
            return ERR_SUCCESS;
         }
      }
   }
}

/*-- log_syntax_error ----------------------------------------------------------
 *
 *      Log a syntax error at this point in the parse.
 *
 * Parameters
 *      IN menu: current menu.
 *      IN msg:  additional text to appear in error message.
 *      IN msg2: additional text to appear in error message.
 *----------------------------------------------------------------------------*/
void log_syntax_error(Menu *menu, const char *msg, const char *msg2)
{
   Log(LOG_ERR, "Menu syntax error in %s, byte %tu (in %s): %s %s\n",
       menu->filename, menu->parse - menu->buffer,
       menu->last == NULL ? "top level" : menu->last->label, msg, msg2);
}

/*-- skip_white ----------------------------------------------------------------
 *
 *      Skip whitespace in the menu being parsed.
 *
 * Parameters
 *      IN/OUT menu: current menu.
 *----------------------------------------------------------------------------*/
void skip_white(Menu *menu)
{
   while (menu->remaining > 0 && isspace(*menu->parse)) {
      menu->remaining--;
      menu->parse++;
   }
}

/*-- skip_line -----------------------------------------------------------------
 *
 *      Skip the rest of the current line and any following whitespace.
 *
 * Parameters
 *      IN/OUT menu: current menu.
 *----------------------------------------------------------------------------*/
void skip_line(Menu *menu)
{
   while (menu->remaining > 0 && *menu->parse != '\n') {
      menu->remaining--;
      menu->parse++;
   }
   skip_white(menu);
}

/*-- match_token ---------------------------------------------------------------
 *
 *      If the given token is at the current parse point, move beyond
 *      it and any following whitespace.  Otherwise do not move.
 *
 * Parameters
 *      IN/OUT menu: current menu.
 *      IN token: token to match.
 *
 * Results
 *      True if the given token was found; false if not.
 *----------------------------------------------------------------------------*/
bool match_token(Menu *menu, const char *token)
{
   unsigned tlen = strlen(token);
   if (tlen > menu->remaining) {
      return false;
   }
   if (strncasecmp(token, menu->parse, tlen) == 0 &&
       (tlen == menu->remaining || isspace(menu->parse[tlen]))) {
      menu->remaining -= tlen;
      menu->parse += tlen;
      skip_white(menu);
      return true;
   }
   return false;
}

/*-- parse_int -----------------------------------------------------------------
 *
 *      Parse an integer at the current parse point, moving beyond it
 *      and any following whitespace.  Note: if there is no valid
 *      integer at the current parse point (even if there is
 *      whitespace), the parse point does not move.
 *
 * Parameters
 *      IN/OUT menu: current menu.
 *
 * Results
 *      The integer value parsed, or 0 if no valid integer found.
 *----------------------------------------------------------------------------*/
int parse_int(Menu *menu)
{
   int ret;
   char *endp;

   /*
    * Sloppy; could read beyond end of buffer if it ends in the
    * middle of a number.
    */
   ret = strtol(menu->parse, &endp, 0);
   if (endp != menu->parse) {
      menu->remaining -= (endp - menu->parse);
      menu->parse = endp;
      skip_white(menu);
   }

   return ret;
}

/*-- parse_str -----------------------------------------------------------------
 *
 *      Parse a string from the current parse point to the end of the
 *      line (or end of buffer if that comes first).  Move parse point
 *      beyond the newline and any following whitespace.
 *
 * Parameters
 *      IN/OUT menu: current menu.
 *
 * Results
 *      A copy of the string parsed.
 *----------------------------------------------------------------------------*/
char *parse_str(Menu *menu)
{
   char *start = menu->parse;
   char *ret;

   while (menu->remaining > 0 &&
          *menu->parse != '\r' && *menu->parse != '\n') {
      menu->remaining--;
      menu->parse++;
   }

   ret = malloc(menu->parse - start + 1);
   memcpy(ret, start, menu->parse - start);
   ret[menu->parse - start] = '\0';

   skip_white(menu);

   return ret;
}

/*-- parse_word ----------------------------------------------------------------
 *
 *      Parse a string from the current parse point to the next whitespace (or
 *      end of line if that comes first).  Move parse point beyond any
 *      following whitespace on the line.
 *
 * Parameters
 *      IN/OUT menu: current menu.
 *
 * Results
 *      A copy of the string parsed.
 *----------------------------------------------------------------------------*/
char *parse_word(Menu *menu)
{
   char *start = menu->parse;
   char *ret;

   while (menu->remaining > 0 && !isspace(*menu->parse)) {
      menu->remaining--;
      menu->parse++;
   }

   ret = malloc(menu->parse - start + 1);
   memcpy(ret, start, menu->parse - start);
   ret[menu->parse - start] = '\0';

   skip_white(menu);

   return ret;
}

/*-- lookup_label --------------------------------------------------------------
 *
 *      Look for an item with the given label.
 *
 * Parameters
 *      IN/OUT menu: current menu.
 *      IN label: label to look for.
 *
 * Results
 *      Pointer to the menu item with the given label, or NULL if none.
 *----------------------------------------------------------------------------*/
MenuItem *lookup_label(Menu *menu, const char *label)
{
   MenuItem *item = menu->first;
   while (item) {
      if (strcmp(item->label, label) == 0) {
         return item;
      }
      item = item->next;
   }
   return NULL;
}

/*-- parse_text_subcommand -----------------------------------------------------
 *
 *      Parse after TEXT token (next token must be HELP).
 *
 * Parameters
 *      IN/OUT menu: current menu.
 *      OUT: a copy of the help text found.
 *
 * Results
 *      ERR_SUCCESS or ERR_SYNTAX.
 *----------------------------------------------------------------------------*/
int parse_text_subcommand(Menu *menu, char **text)
{
   char *start;
   char *end;
   *text = NULL;

   if (!match_token(menu, "HELP")) {
      log_syntax_error(menu, "expected", "HELP");
      return ERR_SYNTAX;
   }
   start = menu->parse;

   while (menu->remaining > 0) {
      skip_line(menu);
      end = menu->parse;
      if (match_token(menu, "ENDTEXT")) {
         *text = malloc(end - start + 1);
         memcpy(*text, start, end - start);
         (*text)[end - start] = '\0';
         return ERR_SUCCESS;
      }
   }

   log_syntax_error(menu, "expected", "ENDTEXT");
   return ERR_SYNTAX;
}

/*-- parse_menu_subcommand -----------------------------------------------------
 *
 *      Parse after MENU token.
 *
 * Parameters
 *      IN/OUT menu: current menu.
 *
 * Results
 *      ERR_SUCCESS or ERR_SYNTAX.
 *----------------------------------------------------------------------------*/
int parse_menu_subcommand(Menu *menu)
{
   bool inItem = menu->last != NULL;

   if (match_token(menu, "TITLE")) {
      menu->title = parse_str(menu);

   } else if (match_token(menu, "HIDDEN")) {
      menu->hidden = true;

   } else if (inItem && match_token(menu, "HIDE")) {
      menu->last->hide = true;

   } else if (inItem && match_token(menu, "LABEL")) {
      menu->last->display = parse_str(menu);

   } else if (inItem && match_token(menu, "DEFAULT")) {
      menu->defitem = menu->last;

   } else if (inItem && match_token(menu, "SEPARATOR")) {
      menu->last->spaceAfter = true;

   } else {
      log_syntax_error(menu, "unexpected MENU subcommand", parse_str(menu));
      if (debug & DEBUG_STRICT_SYNTAX) {
         return ERR_SYNTAX;
      }
   }

   return ERR_SUCCESS;
}

/*-- parse_efi_subcommand ------------------------------------------------------
 *
 *      Parse after EFI token.
 *
 * Parameters
 *      IN/OUT menu: current menu.
 *
 * Results
 *      true if subcommand fully parsed
 *----------------------------------------------------------------------------*/
bool parse_efi_subcommand(Menu *menu)
{
   if (match_token(menu, "DEBUG")) {
      debug = parse_int(menu);

   } else if (match_token(menu, "SERIAL")) {
      int port = parse_int(menu);
      int baud = parse_int(menu);

      if (port == 0) {
         port = DEFAULT_SERIAL_COM;
      }
      if (baud == 0) {
         baud = DEFAULT_SERIAL_BAUDRATE;
      }
      serial_log_init(port, baud);

   } else if (match_token(menu, "VERBOSE")) {
      set_verbose(parse_int(menu));

   } else if (match_token(menu, "HOMEDIR")) {
      char *str = parse_str(menu);
      int status = set_homedir(str);
      if (status != ERR_SUCCESS) {
         Log(LOG_WARNING, "Cannot set homedir to %s: %s",
             str, error_str[status]);
      }

   } else if (match_token(menu, "HTTP")) {
      if (has_gpxe_download_proto(Volume) || has_http(Volume)) {
         return false;
      } else {
         skip_line(menu);
      }

   } else if (match_token(menu, "NOHTTP")) {
      if (has_gpxe_download_proto(Volume) || has_http(Volume)) {
         skip_line(menu);
      } else {
         return false;
      }

   } else if (match_token(menu, "TFTPBLKSIZE")) {
      tftp_set_block_size(parse_int(menu));

   } else if (match_token(menu, "ERRTIMEOUT")) {
      err_timeout = parse_int(menu);

   } else if (match_token(menu, "ARCH")) {
      if (strcasecmp(parse_word(menu), arch_name) == 0) {
         return false;
      } else {
         skip_line(menu);
      }

   } else if (match_token(menu, "NOARCH")) {
      if (strcasecmp(parse_word(menu), arch_name) == 0) {
         skip_line(menu);
      } else {
         return false;
      }

   } else {
      // This "EFI" is just hiding a command from pxelinux
      return false;
   }

   return true;
}

/*-- parse_menu ----------------------------------------------------------------
 *
 *      Parse a text menu.
 *
 * Parameters
 *      IN filename: menu filename, for use in error messages.
 *      IN buffer: contains the menu to be parsed.
 *      IN bufsize: size of buffer.
 *      OUT menuOut: result of parse.
 *
 * Results
 *      ERR_SUCCESS, ERR_SYNTAX, or ERR_ABORTED.
 *----------------------------------------------------------------------------*/
int parse_menu(const char *filename, char *buffer, size_t bufsize,
               Menu **menuOut)
{
   Menu *menu = calloc(1, sizeof(Menu));
   MenuItem *item;
   int status;

   Log(LOG_DEBUG, "parse_menu filename=%s", filename);

   if (debug & DEBUG_PAUSE_BEFORE_PARSE) {
      if (await_keypress() == ERR_ABORTED) {
         return ERR_ABORTED;
      }
   }

   *menuOut = menu;
   menu->filename = filename;
   menu->parse = menu->buffer = buffer;
   menu->remaining = menu->bufsize = bufsize;

   for (;;) {
      skip_white(menu);

      if (match_token(menu, "EFI")) {
         parse_efi_subcommand(menu);
         continue;
      }

      if (*menu->parse == '#') {
         skip_line(menu);

      } else if (match_token(menu, "DEFAULT")) {
         // used if no MENU DEFAULT given
         menu->deflabel = parse_str(menu);

      } else if (match_token(menu, "TIMEOUT")) {
         // rounded up to whole seconds; could fix this if desired
         menu->timeout = (parse_int(menu) + 9) / 10;

      } else if (match_token(menu, "NOHALT")) {
         // ignored
         menu->nohalt = parse_int(menu);

      } else if (match_token(menu, "PROMPT")) {
         // ignored
         menu->prompt = parse_int(menu);

      } else if (match_token(menu, "MENU")) {
         menu->menumode = true;
         status = parse_menu_subcommand(menu);
         if (status != ERR_SUCCESS) {
            return status;
         }

      } else if (match_token(menu, "LABEL")) {
         item = calloc(1, sizeof(MenuItem));
         item->localboot = LOCALBOOT_NONE;
         item->label = parse_str(menu);
         if (menu->first == NULL) {
            menu->first = item;
            menu->last = item;
         } else {
            menu->last->next = item;
            item->prev = menu->last;
            menu->last = item;
         }

      } else if (menu->last && match_token(menu, "KERNEL")) {
         menu->last->kernel = parse_str(menu);

      } else if (menu->last && match_token(menu, "APPEND")) {
         menu->last->append = parse_str(menu);

      } else if (menu->last && match_token(menu, "IPAPPEND")) {
         // ignored; mboot does "IPAPPEND 2" itself
         menu->last->ipappend = parse_int(menu);

      } else if (menu->last && match_token(menu, "LOCALBOOT")) {
         // argument (type of localboot)
         menu->last->localboot = parse_int(menu);

      } else if (menu->last && match_token(menu, "CONFIG")) {
         menu->last->recurse = true;
         menu->last->kernel = parse_str(menu);

      } else if (match_token(menu, "TEXT")) {
         status = parse_text_subcommand(menu, &menu->last->help);
         if (status != ERR_SUCCESS) {
            return status;
         }

      } else if (menu->remaining == 0) {
         break;

      } else {
         log_syntax_error(menu, "unexpected keyword", parse_str(menu));
         if (debug & DEBUG_STRICT_SYNTAX) {
            return ERR_SYNTAX;
         }
      }
   }

   /*
    * Do some fixup on menu.
    */

   /*
    * If there were no MENU keywords, this config file was apparently
    * written for the base pxelinux without menus.  In that case,
    * honor the plain DEFAULT keyword; othewise ignore it.
    */
   if (!menu->menumode && menu->deflabel != NULL) {
      /*
       * First try interpreting DEFAULT's argument as a label
       */
      menu->defitem = lookup_label(menu, menu->deflabel);

      if (menu->defitem == NULL) {
         /*
          * Not a label.  Assume DEFAULT was a command line and make a
          * hidden item for it at the top, where it will become the
          * default.
          */
         item = calloc(1, sizeof(MenuItem));
         item->localboot = LOCALBOOT_NONE;
         item->kernel = menu->deflabel;
         item->hide = true;
         item->next = menu->first;
         menu->first = item;
      }
   }

   /*
    * If no default item was specified, the first is the default.
    */
   if (menu->defitem == NULL) {
      menu->defitem = menu->first;
   }

   if (menu->first == NULL) {
      log_syntax_error(menu, "empty menu", menu->filename);
      return ERR_SYNTAX;
   }

   return ERR_SUCCESS;
}

/*-- file_load_wrapper ---------------------------------------------------------
 *
 *      Call file_load, with debug logs and optional await_keypress
 *
 * Parameters
 *      See file_load
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int file_load_wrapper(int volid, const char *filename, int (*callback)(size_t),
                      void **buffer, size_t *bufsize)
{
   int status;
   Log(LOG_DEBUG, "file_load %s", filename);
   if (debug & DEBUG_PAUSE_BEFORE_READ_TRY) {
      if (await_keypress() == ERR_ABORTED) {
         return ERR_ABORTED;
      }
   }
   status = file_load(volid, filename, callback, buffer, bufsize);
   Log(LOG_DEBUG, "file_load returns %d (%s)", status, error_str[status]);
   return status;
}

/*-- read_file -----------------------------------------------------------------
 *
 *      Read a file into newly allocated memory.
 *
 * Parameters
 *      IN f: name of file requested to be read.
 *      OUT fOut: actual name of file (after path search).
 *      OUT bufOut: contents of file.
 *      OUT sizeOut: size of file.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int read_file(const char *f, const char **fOut, void **bufOut, size_t *sizeOut)
{
   int status;
   const char *filename;
   const char *bname = NULL;
   char *absname = NULL;

   Log(LOG_DEBUG, "read_file %s", f);
   /*
    * Interpret relative names relative to the directory that menu.efi
    * was loaded from.  Note that make_path() considers URLs to be
    * absolute.
    */
   if (is_absolute(f)) {
      filename = f;
   } else {
      status = make_path(homedir, f, &absname);
      if (status != ERR_SUCCESS) {
         return status;
      }
      filename = absname;
   }
   status = file_load_wrapper(volid, filename, NULL, bufOut, sizeOut);

   /*
    * If file is not found, try its basename relative to the
    * pxelinux.cfg and menuefi.d directories.  This implements the
    * search for default menu in those directories.  It also helps
    * with existing pxe deployment directories that don't contain
    * mboot.efi; we'll deliver a copy from the server.
    */
   if (status != ERR_SUCCESS) {
      bname = &f[strlen(f)];
      while (bname > f && *bname != '/') {
         bname--;
      }
      if (*bname == '/') {
         bname++;
      }

      if (absname != NULL) {
         free(absname);
      }
      status = make_path(menuefi_d, bname, &absname);
      if (status != ERR_SUCCESS) {
         return status;
      }
      filename = absname;
      status = file_load_wrapper(volid, filename, NULL, bufOut, sizeOut);
   }

   if (status != ERR_SUCCESS) {
      if (absname != NULL) {
         free(absname);
      }
      status = make_path(pxelinux_cfg, bname, &absname);
      if (status != ERR_SUCCESS) {
         return status;
      }
      filename = absname;
      status = file_load_wrapper(volid, filename, NULL, bufOut, sizeOut);
   }

   if (status != ERR_SUCCESS) {
      if (absname != NULL) {
         free(absname);
      }
      return status;
   }

   if (fOut) {
      *fOut = filename;
   }
   return status;
}

/*-- get_uuid_str --------------------------------------------------------------
 *
 *      Convert a UUID to a human readable string where each byte in
 *      the UUID is seen as an unsigned char and represented by a 0-prefixed,
 *      2-characters, lower case hexadecimal string. The output string is
 *      allocated with malloc() and can be freed with free().
 *
 * Parameters
 *      IN uuid: pointer to the UUID
 *      OUT uuid_str:  pointer to the freshly allocated output string
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_uuid_str(const uint8_t uuid[16], char **uuid_str)
{
   int major = smbios_version_major;
   int minor = smbios_version_minor;

   /* if version < 2.6 , use big endian for all the fields, see PR 3347702 */
   if (major < 2 || (major == 2 && minor < 6) ) {
      return asprintf(
         uuid_str,
         "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
         uuid[0], uuid[1], uuid[2], uuid[3],
         uuid[4], uuid[5],
         uuid[6], uuid[7],
         uuid[8], uuid[9],
         uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]) < 0
            ? EFI_INVALID_PARAMETER : EFI_SUCCESS;
   } else {
      return asprintf(
         uuid_str,
         "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
         uuid[3], uuid[2], uuid[1], uuid[0],
         uuid[5], uuid[4],
         uuid[7], uuid[6],
         uuid[8], uuid[9],
         uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]) < 0
            ? EFI_INVALID_PARAMETER : EFI_SUCCESS;
   }
}

/*-- run_menu ------------------------------------------------------------------
 *
 *      Interact with the user to select from the current menu.  Then
 *      in the normal case, chainload a program associated with the
 *      selected item.
 *
 * Parameters
 *      IN menu: current menu.
 *
 * Results
 *      Does not return if an item was successfully chainloaded.
 *      ERR_SUCCESS on LOCALBOOT -2 or backspace.
 *      ERR_ABORTED on LOCALBOOT -1.
 *      Generic error status on error.
 *----------------------------------------------------------------------------*/
int run_menu(Menu *menu)
{
   key_code_t key;
   MenuItem *item;
   unsigned num;
   unsigned timeout;
   MenuItem *selection;
   bool hidden;

   hidden = menu->hidden;
   selection = menu->defitem;
   timeout = menu->timeout;

   if (debug & DEBUG_PAUSE_BEFORE_DISPLAY) {
      if (await_keypress() == ERR_ABORTED) {
         return ERR_ABORTED;
      }
   }

 redraw:
   firmware_print("\n");
   st->ConOut->ClearScreen(st->ConOut);
   firmware_print(welcome);
   firmware_print("\n");
   if (menu->title) {
      firmware_print(menu->title);
      firmware_print("\n\n");
   }
   if (!hidden) {
      item = menu->first;
      num = 0;
      while (item) {
         if (!item->hide) {
            char index[] = "x ";

            firmware_print(item == selection ? ">>> " : "    ");

            index[0] = 'a' + num++;
            firmware_print(index);

            firmware_print(item->display ? item->display : item->label);
            firmware_print("\n");
         }
         if (item->spaceAfter) {
            firmware_print("\n");
         }
         item = item->next;
      }
      if (selection->help) {
         firmware_print("\n");
         firmware_print(selection->help);
      }
      firmware_print("\nUP/DOWN/LETTER: select, ENTER: boot,\n"
                     "SPACE: pause, ESC: restart, BACKSPACE: quit\n");
   }
   hidden = false;

   for (;;) {
      kbd_waitkey_timeout(&key, timeout);
      timeout = TIMEOUT_INFINITE;

      if (key.sym == KEYSYM_NONE) {
         // Timeout occurred
         item = selection;

      } else if (key.sym == KEYSYM_ASCII &&
                 key.ascii == ' ') {
         continue;

      } else if (key.sym == KEYSYM_UP) {
         item = selection->prev;
         while (item && item->hide) {
            item = item->prev;
         }
         if (item) {
            selection = item;
            goto redraw;
         } else {
            continue;
         }

      } else if (key.sym == KEYSYM_DOWN) {
         item = selection->next;
         while (item && item->hide) {
            item = item->next;
         }
         if (item) {
            selection = item;
            goto redraw;
         } else {
            continue;
         }

      } else if (key.sym == KEYSYM_ASCII &&
                 (key.ascii == '\r' || key.ascii == '\n')) {
         item = selection;

      } else if (key.sym == KEYSYM_ASCII &&
                 (key.ascii == '\b' || key.ascii == '\177')) {
         return ERR_SUCCESS;

      } else if (key.sym == KEYSYM_ASCII &&
                 key.ascii == '\033' /*ESC*/) {
         return run_menu(rootmenu);

      } else if (key.sym == KEYSYM_ASCII) {
         num = tolower(key.ascii) - 'a';
         item = menu->first;
         while (item) {
            if (!item->hide) {
               if (num-- == 0) {
                  selection = item;
                  goto redraw;
               }
            }
            item = item->next;
         }
         continue;

      } else {
         continue;
      }

      return do_item(menu, item);
   }
}

/*-- do_item ------------------------------------------------------------------
 *
 *      Execute the selected menu item.  In the normal case,
 *      chainloads the item.
 *
 * Parameters
 *      IN menu: current menu.
 *      IN item: selected item.
 *
 * Results
 *      Does not return if an item was successfully chainloaded.
 *      ERR_SUCCESS on LOCALBOOT -2 or backspace.
 *      ERR_ABORTED on LOCALBOOT -1.
 *      Generic error status on error.
 *----------------------------------------------------------------------------*/
int do_item(Menu *menu, MenuItem *item)
{
   char *program, *arguments, *bn;
   int len;

   Log(LOG_DEBUG, "do_item label=%s display=%s kernel=%s append=%s",
       item->label, item->display, item->kernel, item->append);

   if (item->localboot == -2) {
      return ERR_SUCCESS;

   } else if (item->localboot != LOCALBOOT_NONE) {
      return ERR_ABORTED;

   } else if (item->recurse) {
      return do_menu(item->kernel);

   } else if (!item->kernel) {
      /*
       * The item is invalid: no KERNEL, LOCALBOOT, or CONFIG keyword.
       * (As we are no longer in the parsing step, calling log_syntax_error()
       * would not work correctly; it would give the end of file byte offset,
       * not the offset of this item, which we no longer know.)
       */
      Log(LOG_ERR, "Selected item %s (in menu %s) "
          "has neither KERNEL, LOCALBOOT, nor CONFIG keyword",
          item->label, menu->filename);
      return ERR_SYNTAX;
   }

   if (asprintf(&program, "%s %s", item->kernel,
               item->append ? item->append : "") == -1) {
      return ERR_OUT_OF_RESOURCES;
   }

   arguments = strchr(program, ' ');
   *arguments++ = '\0';
   while (*arguments == ' ') {
      arguments++;
   }

   Log(LOG_DEBUG, "do_item program=%s arguments=%s", program, arguments);

   /*
    * Fake some special cases of "program".
    */

   /*
    * ifgpxe.c32, ifvm.c32, and ifver410.c32 are programs sometimes
    * used in pxelinux menus at VMware to test conditions. The syntax
    * looks like:
    *
    *    ifgpxe.c32 tsel -- fsel
    *
    * Here tsel is chosen if the condition being tested is true; else
    * fsel is chosen.  Apparently tsel and fsel can be either menu
    * labels or command lines (with arguments).
    *
    * ifgpxe   - tests for gPXE/iPXE (for HTTP support)
    * ifver410 - tests for pxelinux version 4.10 (for HTTP support)
    * ifvm     - not sure; maybe it tests if we're in a VM?
    *
    * The typical usage is to chain to gPXE or iPXE on the false
    * branch.  Since we either have or fake HTTP support, and "ifvm"
    * probably doesn't matter to us, we always take the true branch.
    */
   bn = basename(program);
   if (strcmp(bn, "ifgpxe.c32") == 0 ||
       strcmp(bn, "ifver410.c32") == 0 ||
       strcmp(bn, "ifvm.c32") == 0) {
      char *p;
      MenuItem *item2;

      p = strstr(arguments, "--");
      if (p != NULL) {
         do {
            p--;
         } while (p >= arguments && *p == ' ');
         p++;
         *p = '\0';
      }

      item2 = lookup_label(menu, arguments);

      /*
       * Create a fake item if this is a command line.  This is a
       * bit ugly.  Maybe both this and the DEFAULT handling could be
       * unified and cleaned up.
       */
      if (item2 == NULL) {
         item2 = calloc(1, sizeof(MenuItem));
         item2->localboot = LOCALBOOT_NONE;
         item2->kernel = arguments;
      }
      return do_item(menu, item2);
   }

   /*
    * Avoid chainloading menu.efi itself; instead, call do_menu
    * recursively.  This is just an optimization.  We can't do it if
    * there are options on the command line.
    */
   if ((strcmp(bn, "menu.efi") == 0 ||
        strcmp(bn, "menu.c32") == 0) && arguments[0] != '-') {
      return do_menu(arguments);
   }

   len = strlen(program);
   if (len >= 4 && strcmp(&program[len - 4], ".c32") == 0) {
      /*
       * Change .c32 extension to .efi.
       */
      strcpy(&program[len - 4], ".efi");

   } else if (len >= 2 && strcmp(&program[len - 2], ".0") == 0) {
      /*
       * Change .0 extension to .efi.
       */
      char *p = malloc(len + 3);
      memcpy(p, program, len - 2);
      strcpy(p + len, ".efi");
      program = p;
   }

   return chain_to(program, arguments);
}

/*-- chain_to ------------------------------------------------------------------
 *
 *      Chainload the specified image.
 *
 * Parameters
 *      IN program: image name.
 *      IN arguments: command-line arguments.
 *
 * Results
 *      Generic error status on error.  Otherwise does not return.
 *----------------------------------------------------------------------------*/
int chain_to(const char *program, const char *arguments)
{
   int status;
   void *image;
   size_t imgsize;
   EFI_HANDLE ChildHandle;

   Log(LOG_DEBUG, "chain_to program=%s arguments=%s", program, arguments);

   /*
    * Search for and read the image into memory.  Sets "program" to the
    * filepath where the image was actually found.
    */
   status = read_file(program, &program, &image, &imgsize);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = firmware_image_load(program, arguments, image, imgsize,
                                &ChildHandle);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (debug & DEBUG_PAUSE_BEFORE_START_IMAGE) {
      if (await_keypress() == ERR_ABORTED) {
         return ERR_ABORTED;
      }
   }

   /*
    * Start the child.  Typically the child is a bootloader, in which case it
    * won't return.
    */
   status = firmware_image_start(ChildHandle);
   Log(LOG_DEBUG, "Child returned; status = %d (%s)",
       status, error_str[status]);
   return status;
}

/*-- do_menu -------------------------------------------------------------------
 *
 *      Open, read, parse, and run a menu.
 *
 * Parameters
 *      IN filename: name of file requested to be read.
 *                   Use default filename if NULL.
 *
 * Results
 *      Does not return if an item was successfully chainloaded.
 *      ERR_SUCCESS on LOCALBOOT -2 or backspace.
 *      ERR_ABORTED on LOCALBOOT -1.
 *      Generic error status on error.
 *----------------------------------------------------------------------------*/
int do_menu(const char *filename)
{
   int status;
   Menu *menu;
   void *buffer;
   size_t bufsize;

   Log(LOG_DEBUG, "do_menu filename=%s", filename);

   if (debug & DEBUG_PAUSE_BEFORE_READ) {
      if (await_keypress() == ERR_ABORTED) {
         return ERR_ABORTED;
      }
   }

   if (filename == NULL) {
      /* No menu filename given; search for one */
      const char *mac;

      status = get_mac_address(&mac);
      if (status == ERR_SUCCESS) {
         status = read_file(mac, &filename, &buffer, &bufsize);
      }
      if (status != ERR_SUCCESS) {
         status = read_file("default", &filename, &buffer, &bufsize);
      }

   } else {
      status = read_file(filename, &filename, &buffer, &bufsize);

   }
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = parse_menu(filename, buffer, bufsize, &menu);
   if (status != ERR_SUCCESS) {
      return status;
   }
   free(buffer);
   menu->buffer = NULL;
   if (rootmenu == NULL) {
      rootmenu = menu;
   }

   status = run_menu(menu);
   return status;
}

/*-- set_verbose ---------------------------------------------------------------
 *
 *      Set or clear verbose mode.  In verbose mode, LOG_DEBUG messages go to
 *      the display; otherwise they do not.
 *
 * Parameters
 *      IN state: new state of verbose mode
 *----------------------------------------------------------------------------*/
void set_verbose(bool state)
{
   verbose = state;
   log_unsubscribe(firmware_print);
   log_subscribe(firmware_print, verbose ? LOG_DEBUG : LOG_INFO);
}

/*-- add_smbios_query_string_parameters ----------------------------------------
 *
 *      Take the globally saved smbios values and add them to the query
 *      strings, as if -q was passed to main N times. If some of these fields
 *      are not found then these values can be NULL or "", and when appending
 *      will be shown as "family=" a.k.a. the value for the key family is ""
 *
 * Parameters
 *
 * Results
 *      ERR_SUCCESS on success or a generic error status
 *----------------------------------------------------------------------------*/
int add_smbios_query_string_parameters(void)
{
   int result = ERR_SUCCESS;

   key_value_t smbios_query_string_parameters[] = {
      { bios_version_name,  bios_version    },
      { vendor_name,        vendor          },
      { model_name,         model           },
      { family_name,        family          },
      { serial_number_name, serial_number   },
      { system_uuid_name,   system_uuid_str },
      { mac_name,           mac_str         },
   };

   if ((result = query_string_add_parameters(
         ARRAYSIZE(smbios_query_string_parameters),
         smbios_query_string_parameters)) != ERR_SUCCESS) {
      return result;
   }

   if ((result = query_string_add_parameters(
        oem_strings.length,
        oem_strings.entries)) != ERR_SUCCESS) {
      return result;
   }

   return result;
}

/*-- process_query_string ------------------------------------------------------
 *
 *      Take the global query string arguments from menu.c and transfer them
 *      to the global values in uri.c to make the query strings usable in
 *      URL-s
 *
 * Parameters
 *
 * Results
 *      ERR_SUCCESS on success or a generic error status
 *----------------------------------------------------------------------------*/
int process_query_string(void)
{
   int status = ERR_SUCCESS;

   if (query_string_arguments_nr > 0) {
      size_t query_string_parameters_count = 0;
      key_value_t* query_string_parameters = malloc(
         query_string_arguments_nr * sizeof(query_string_parameters[0]));

      if (query_string_parameters == NULL) {
         status = ERR_OUT_OF_RESOURCES;
         goto out;
      }

      for (size_t index = 0; index < query_string_arguments_nr; index++) {
         char* ps = strchr(query_string_arguments[index], '=');
         if (ps == NULL) {
            Log(LOG_ERR, "Query string parameter %s does not contain \"=\"",
               query_string_arguments[index]);
            status = ERR_INVALID_PARAMETER;
            free(query_string_parameters);
            goto out;
         } else {
            key_value_t query_string_parameter =
               { query_string_arguments[index], ps + 1 };
            *ps = '\0';
            query_string_parameters[query_string_parameters_count++] =
               query_string_parameter;
         }
      }
      status = query_string_add_parameters(
         query_string_parameters_count, query_string_parameters);
      free(query_string_parameters);
      if (status != ERR_SUCCESS) {
         goto out;
      }
   }
   out:
   return status;
}

/*-- fetch_smbios_data ---------------------------------------------------------
 *
 *      Extract smbios data and save it globally. Calling this function more
 *      than once will cause memory leaks in the dynamic count fields.
 *
 * Parameters
 *
 * Results
 *      ERR_SUCCESS on success or a generic error status
 *----------------------------------------------------------------------------*/
int fetch_smbios_data(void)
{
   int status;
   if ((status = smbios_get_system_info(
        &vendor, &model, NULL, &serial_number, &system_uuid, NULL, &family))
         != ERR_SUCCESS) {
      Log(LOG_ERR, "Failed to obtain SMBIOS type 1 data");
      goto out;
   }

   status = smbios_get_version(
      &smbios_version_major,
      &smbios_version_minor,
      &simbios_version_doc_rev
   );

   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Failed to obtain SMBIOS version");
      goto out;
   }

   if ((status = smbios_get_oem_strings(&oem_strings)) != ERR_SUCCESS) {
      Log(LOG_ERR, "Failed to obtain OEM strings");
      goto out;
   }

   if ((status = get_uuid_str(system_uuid, &system_uuid_str)) != ERR_SUCCESS) {
      Log(LOG_ERR, "Failed to store UUID, error code");
      goto out;
   }

   if ((status = smbios_get_firmware_info(&bios_version, NULL))
         != ERR_SUCCESS) {
      Log(LOG_ERR, "Failed to obtain Bios Version");
      goto out;
   }

   if ((status = get_mac_address(&mac_str)) != ERR_SUCCESS) {
      Log(LOG_ERR, "Failed to obtain MAC address");
      goto out;
   }



   out:
   return status;
}

/*-- main ----------------------------------------------------------------------
 *
 *      Main program.  Setup, call do_menu, handle errors.
 *
 * Parameters
 *      IN argc, argv: command line.
 *
 * Results
 *      Does not return if an item was successfully chainloaded.
 *      ERR_SUCCESS on LOCALBOOT -2 or backspace.
 *      ERR_ABORTED on LOCALBOOT -1.
 *      Generic error status on error.
 *----------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
   int status;
   char *bootfile;
   key_code_t key;
   bool serial = false;
   int port = DEFAULT_SERIAL_COM;
   int baud = DEFAULT_SERIAL_BAUDRATE;
   int i;
   EFI_STATUS Status;
   const char* query_string;

#ifdef DEBUG
  /*
   * Uncomment/modify the following when needed to debug issues prior
   * to option parsing or when options can't be given.
   */
   verbose = true;
   serial = true;
#endif /* DEBUG */

   status = log_init(verbose);
   if (status != ERR_SUCCESS) {
      goto out;
   }

    for (i = 0; i < argc; i++) {
       Log(LOG_DEBUG, "argv[%d]=%s", i, argv[i]);
    }

   if (argc > 0) {
      int opt;

      optind = 1;
      do {
         opt = getopt(argc, argv, ":D:S:s:VhH:b:E:q:C");
         switch (opt) {
         case -1:
            break;
         case 'D': /* debug flags */
            debug = atoi(optarg);
            break;
         case 'S': /* serial port */
            serial = true;
            port = atoi(optarg);
            break;
         case 's': /* serial bit rate */
            serial = true;
            baud = atoi(optarg);
            break;
         case 'V': /* verbose */
            verbose = true;
            break;
         case 'H': /* homedir */
         case 'h':
            status = set_homedir(opt == 'h' ? "" : optarg);
            if (status != ERR_SUCCESS) {
               Log(LOG_WARNING, "Cannot set homedir to \"%s\": %s",
                   opt == 'h' ? "" : optarg, error_str[status]);
            }
            break;
         case 'b': /* tftpblksize */
            tftp_set_block_size(atoi(optarg));
            break;
         case 'E': /* errtimeout */
            err_timeout = atoi(optarg);
            break;
         case 'q': {
               char **tmp = sys_realloc(
                  query_string_arguments,
                  query_string_arguments_nr * sizeof query_string_arguments[0],
                  (query_string_arguments_nr + 1) *
                     sizeof query_string_arguments[0]);
               if (tmp == NULL) {
                  free(query_string_arguments);
                  query_string_arguments = NULL;
                  query_string_arguments_nr = 0;
                  status = ERR_OUT_OF_RESOURCES;
                  goto out;
               }
               query_string_arguments = tmp;
               query_string_arguments[query_string_arguments_nr++] = optarg;
            }
            break;
         case 'C':
            collect_and_use_smbios_data = true;
            break;
         case ':': /* missing option argument */
            Log(LOG_CRIT, "Missing argument to -%c", optopt);
            status = ERR_SYNTAX;
            goto out;
         case '?': /* unknown option */
            Log(LOG_CRIT, "Unknown option -%c", optopt);
            status = ERR_SYNTAX;
            goto out;
         default: /* bug: option in string but no case for it */
            Log(LOG_CRIT, "Unknown option -%c", opt);
            status = ERR_SYNTAX;
            goto out;
         }
      } while (opt != -1);
   }

   status = log_init(verbose);
   if (status != ERR_SUCCESS) {
      goto out;
   }
   if (serial) {
      serial_log_init(port, baud);
   }

   /*
    * Log a message to show where menu.efi has been relocated to and
    * loaded, for use in debugging.  Currently the unrelocated value
    * of __executable_start is 0 for COM32, but is 0x1000 for UEFI
    * because of HEADERS_SIZE is uefi/uefi.lds.  Check the symbol
    * table in the .elf binary to be sure.
    */
   Log(LOG_DEBUG, "menu.efi __executable_start is at %p",
       __executable_start);

   if ((status = fetch_smbios_data()) != ERR_SUCCESS) {
      goto out;
   }

   /*
    * Use the first few bytes of this binary's hash as a version stamp
    * in the welcome message.
    */
   snprintf(welcome, sizeof(welcome), "vSphere Boot Manager %08x, MAC %s",
            *(uint32_t *)_expected_hash, mac_str);
   Log(LOG_DEBUG, "%s", welcome);

   if ((status = process_query_string()) != ERR_SUCCESS) {
      goto out;
   }

   if (collect_and_use_smbios_data) {
      if ((status = add_smbios_query_string_parameters()) != ERR_SUCCESS) {
         Log(LOG_ERR, "Failed to add smbios query string parameters");
         goto out;
      }
   }

   if ((status = query_string_get(&query_string)) != ERR_SUCCESS) {
      Log(LOG_ERR, "Failed to construct query string");
      goto out;
   }

   if (query_string != NULL) {
      Log(LOG_DEBUG, "Generated query_string %s", query_string);
   }

   status = get_boot_file(&bootfile);
   if (status != ERR_SUCCESS) {
      goto out;
   }

   if (homedir == NULL) {
      char *bootdir;
      status = get_boot_dir(&bootdir);
      if (status != ERR_SUCCESS) {
         goto out;
      }
      status = set_homedir(bootdir);
      if (status != ERR_SUCCESS) {
         Log(LOG_WARNING, "Cannot set homedir to \"%s\": %s",
             bootdir, error_str[status]);
      }
      free(bootdir);
   }

   Status = get_boot_volume(&Volume);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   Log(LOG_DEBUG, "main bootfile=%s homedir=%s argc=%d",
       bootfile, homedir, argc);

   status = do_menu(optind < argc ? argv[optind] : NULL);

 out:
   if (status == ERR_SUCCESS || status == ERR_ABORTED) {
      Log(LOG_DEBUG, "Returning %d (%s)", status, error_str[status]);
   } else if (status != ERR_SUCCESS) {
      Log(LOG_ERR,
          "Error %d (%s)\n"
          "Press a key to continue UEFI boot sequence...",
          status, error_str[status]);
      kbd_waitkey_timeout(&key, err_timeout);
   }
   query_string_cleanup();
   return status;
}
