/*******************************************************************************
 * Copyright (c) 2015-2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * menu.c --
 *
 *      Interpret a subset of pxelinux simple menus and chain to
 *      another uefi bootloader or app.
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
 *         -D <N>         Set debug flag bits to N.  Bits include:
 *                        1: Wait for keypress before starting an app.
 *                        2: Fail with a syntax error on unrecognized keywords.
 *                        4: Wait for keypress before parsing menu.
 *                        8: Wait for keypress before displaying menu.
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
 *      <HOMEDIR> can be changed by the menu or a command-line option.
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
 *      XXX Some of the above searching is probably overkill.
 *
 *      The following syntax subset is supported, where n is a numeric
 *      argument, s is a string argument to end of line, and ... is one or more
 *      lines of text.  Note that the EFI keyword does not exist in pxelinux,
 *      but it seems to be safe to put it into menus that are shared with
 *      pxelinux, as pxelinux ignores lines starting with an unknown keyword.
 *
 *      #s             - Comment.
 *      DEFAULT s      - Ignored if any MENU keywords occur in the file.
 *                         Otherwise gives label of default item
 *                         or default command line to chain to.
 *      TIMEOUT n      - Automatically boot the default item in n/10 seconds.
 *      NOHALT n       - Ignored.
 *      PROMPT n       - Ignored.
 *      MENU TITLE s   - Give the menu a title.
 *      MENU HIDDEN    - Don't display the menu until a key is pressed.
 *      EFI DEBUG n    - Set debug flags to n.
 *      EFI SERIAL p b - Debug log to serial port p, baud b (both optional).
 *      EFI VERBOSE n  - Show Log(LOG_DEBUG, ...) messages on screen.
 *      EFI HOMEDIR s  - Change the directory for interpreting relative paths.
 *      EFI HTTP s     - Evaluate s if HTTP loading is available.
 *      EFI NOHTTP s   - Evaluate s if HTTP loading is not available.
 *      EFI s          - Evaluate s (ignored if pxelinux parses the menu).
 *
 *      LABEL s        - Starts and names a menu item.  The following
 *                         keywords are recognized only within items.
 *      KERNEL s       - The EFI app (possibly with arguments) to chain to.
 *      APPEND s       - Command line arguments (added after any in KERNEL).
 *      IPAPPEND n     - Ignored.
 *      LOCALBOOT n    - This item will exit back to the EFI boot manager.
 *      CONFIG s       - This item will restart with s as the menu.
 *      MENU HIDE      - Don't display this item.
 *      MENU LABEL s   - Display this string instead of the item's label.
 *      MENU DEFAULT   - Make this item the default.
 *      MENU SEPARATOR - Display a blank line under this item.
 *      TEXT HELP ...  - Display text up to ENDTEXT while this item is selected.
 *      ENDTEXT        - Terminates TEXT HELP.
 *
 *      If multiple KERNEL (or EFI KERNEL) options are given in an item, only
 *      the last is effective, and similarly for APPEND (or EFI APPEND).  If
 *      multiple DEFAULT or MENU DEFAULT options are given, the last of each is
 *      effective.
 *
 *      Certain command names are handled specially when chaining:
 *
 *      * A .c32 or .0 extension is automatically changed to .efi.
 *
 *      * menu.efi (or menu.c32) restarts the same instance of menu.efi
 *        with its argument as the menu, instead of loading a new instance.
 *        This is essentially just an optimization.
 *
 *      * if{gpxe,vm,ver410}.{efi,c32} expects arguments of the form
 *        "s1 -- s2".  It executes s1 as either a label or command
 *        line.  s2 is ignored.  This kludge helps deal with some
 *        existing pxelinux menus used at VMware, where an "if*.c32"
 *        program is used to continue with the current version of
 *        pxelinux in the "then" (s1) case or chainload a different
 *        version of pxelinux and restart in the "else" (s2) case.
 *
 *      * ipxe-undionly.0 is automatically changed to ipxe-snponly.efi.
 */

#include <efiutils.h>
#include <bootlib.h>
#include <boot_services.h>
#include <ctype.h>
#include <stdlib.h>
#include <error.h>
#include <libgen.h>
#include <unistd.h>

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
Menu *rootmenu;
EFI_HANDLE Volume;

/*
 * Debug bit flags
 */
#define DEBUG_PAUSE_BEFORE_START_IMAGE 1
#define DEBUG_STRICT_SYNTAX            2
#define DEBUG_PAUSE_BEFORE_PARSE       4
#define DEBUG_PAUSE_BEFORE_DISPLAY     8
unsigned debug = 0;
bool verbose = false;

#define DEFAULT_SERIAL_COM      1       /* Default serial port (COM1) */
#define DEFAULT_SERIAL_BAUDRATE 115200  /* Default serial baud rate */

int do_menu(const char *filename);
int do_item(Menu *menu, MenuItem *item);
int chain_to(const char *program, const char *arguments);
void set_verbose(bool verbose);


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
      menu->hidden = TRUE;

   } else if (inItem && match_token(menu, "HIDE")) {
      menu->last->hide = TRUE;

   } else if (inItem && match_token(menu, "LABEL")) {
      menu->last->display = parse_str(menu);

   } else if (inItem && match_token(menu, "DEFAULT")) {
      menu->defitem = menu->last;

   } else if (inItem && match_token(menu, "SEPARATOR")) {
      menu->last->spaceAfter = TRUE;

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
   bool inItem = menu->last != NULL;

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

   } else if (match_token(menu, "HTTP")) {
      if (has_gpxe_download_proto(Volume)) {
         return false;
      } else {
         skip_line(menu);
      }

   } else if (match_token(menu, "NOHTTP")) {
      if (has_gpxe_download_proto(Volume)) {
         skip_line(menu);
      } else {
         return false;
      }

   } else if (inItem && match_token(menu, "HOMEDIR")) {
      homedir = parse_str(menu);

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
 *      ERR_SUCCESS or ERR_SYNTAX.
 *----------------------------------------------------------------------------*/
int parse_menu(const char *filename, char *buffer, size_t bufsize,
               Menu **menuOut)
{
   Menu *menu = calloc(1, sizeof(Menu));
   MenuItem *item;
   int status;

   Log(LOG_DEBUG, "parse_menu filename=%s", filename);

   if (debug & DEBUG_PAUSE_BEFORE_PARSE) {
      key_code_t key;
      Log(LOG_NOTICE, "Press a key to continue...");
      kbd_waitkey_timeout(&key, 300);
   }

   *menuOut = menu;
   menu->filename = filename;
   menu->parse = menu->buffer = buffer;
   menu->remaining = menu->bufsize = bufsize;

   for (;;) {
      skip_white(menu);

      if (match_token(menu, "EFI")) {
         if (parse_efi_subcommand(menu)) {
            continue;
         }
         skip_white(menu);
      }

      if (*menu->parse == '#') {
         skip_line(menu);

      } else if (match_token(menu, "DEFAULT")) {
         // used if no MENU DEFAULT given
         menu->deflabel = parse_str(menu);

      } else if (match_token(menu, "TIMEOUT")) {
         // rounded up to whole seconds; could fix this if desired
         menu->timeout = parse_int(menu);

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
         // argument (type of localboot) ignored
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
       * First try interpreting DEFAULT as a label
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
 *      ERR_SUCCESS or generic error.
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
    * was loaded from.  Note that is_absolute() considers URLs to be
    * absolute.
    */
   if (is_absolute(f)) {
      filename = f;
   } else {
      if (asprintf(&absname, "%s/%s", homedir, f) == -1) {
         return ERR_OUT_OF_RESOURCES;
      }
      filename = absname;
   }
   Log(LOG_DEBUG, "file_load %s", filename);
   status = file_load(volid, filename, NULL, bufOut, sizeOut);
   Log(LOG_DEBUG, "file_load returns %d (%s)", status, error_str[status]);

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
      if (asprintf(&absname, "%s/menuefi.d/%s", homedir, bname) == -1) {
         return ERR_OUT_OF_RESOURCES;
      }
      filename = absname;
      Log(LOG_DEBUG, "file_load %s", filename);
      status = file_load(volid, filename, NULL, bufOut, sizeOut);
      Log(LOG_DEBUG, "file_load returns %d (%s)", status, error_str[status]);
   }

   if (status != ERR_SUCCESS) {
      if (absname != NULL) {
         free(absname);
      }
      if (asprintf(&absname, "%s/pxelinux.cfg/%s", homedir, bname) == -1) {
         return ERR_OUT_OF_RESOURCES;
      }
      filename = absname;
      Log(LOG_DEBUG, "file_load %s", filename);
      status = file_load(volid, filename, NULL, bufOut, sizeOut);
      Log(LOG_DEBUG, "file_load returns %d (%s)", status, error_str[status]);
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
 *      ERR_SUCCESS if the user selected a LOCALBOOT item.
 *      Generic error code on error.
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
   timeout = (menu->timeout + 9) / 10;

   if (debug & DEBUG_PAUSE_BEFORE_DISPLAY) {
      key_code_t key;
      Log(LOG_NOTICE, "Press a key to continue...");
      kbd_waitkey_timeout(&key, 300);
   }

 redraw:
   firmware_print("\n");
   st->ConOut->ClearScreen(st->ConOut);
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
                 key.ascii == '\b') {
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
 *      ERR_SUCCESS if the user selected a LOCALBOOT item.
 *      Generic error code on error.
 *----------------------------------------------------------------------------*/
int do_item(Menu *menu, MenuItem *item)
{
   char *program, *arguments, *bn;
   int len;

   Log(LOG_DEBUG, "do_item label=%s display=%s kernel=%s append=%s",
       item->label, item->display, item->kernel, item->append);

   if (item->localboot != LOCALBOOT_NONE) {
      return ERR_SUCCESS;

   } else if (item->recurse) {
      return do_menu(item->kernel);

   } else if (!item->kernel) {
      // Could have detected this earlier...
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
    * Change ipxe-undionly.0 to ipxe-snponly.efi.  This helps when
    * chainloading iPXE at VMware.
    */
   bn = basename(program);
   if (strcmp(bn, "ipxe-undionly.0") == 0) {
      program = strdup("ipxe-snponly.efi");
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

   /*
    * Avoid chainloading menu.efi itself; instead, call do_menu
    * recursively.  This is just an optimization.  We can't do it if
    * there are options on the command line.
    */
   bn = basename(program);
   if (strcmp(bn, "menu.efi") == 0 && arguments[0] != '-') {
      return do_menu(arguments);
   }

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
    * ifgpxe   - effectively tests whether HTTP support is available.
    * ifvm     - not sure; maybe it tests if we're in a VM?
    * ifver410 - not sure; maybe tests if this is pxelinux version 4.10.
    *
    * The typical usage is to chain to gpxelinux.0 on the false
    * branch.  Since we fake HTTP support and the other conditions
    * probably don't matter to us, we always take the true branch.
    */
   if (strcmp(bn, "ifgpxe.efi") == 0 ||
       strcmp(bn, "ifvm.efi") == 0 ||
       strcmp(bn, "ifver410.efi") == 0) {
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
       * Create a fake item if this is a command line.  XXX This is a
       * bit ugly.  Maybe both this and the DEFAULT handling can be
       * unified and cleaned up?
       */
      if (item2 == NULL) {
         item2 = calloc(1, sizeof(MenuItem));
         item2->localboot = LOCALBOOT_NONE;
         item2->kernel = arguments;
      }
      return do_item(menu, item2);
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
 *      Generic error code on error.  Otherwise does not return.
 *----------------------------------------------------------------------------*/
int chain_to(const char *program, const char *arguments)
{
   int status;
   void *image;
   size_t imgsize;
   EFI_HANDLE ChildHandle;
   EFI_LOADED_IMAGE *Child;
   EFI_STATUS Status;
   CHAR16 *LoadOptions;
   UINTN ExitDataSize;
   CHAR16 *ExitData;
   CHAR16 *Program;
   EFI_DEVICE_PATH *ProgramPath;

   Log(LOG_DEBUG, "chain_to program=%s arguments=%s", program, arguments);

   /* This is messy because the Load File Protocol can't load files
    * from the network; see comment at top of tftpfile.c.  Instead, we
    * read the file into memory using our own function, then invoke
    * bs->LoadImage on that.
    */
   LoadOptions = NULL;
   Status = ascii_to_ucs2(arguments, &LoadOptions);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   /* Read the image into memory */
   status = read_file(program, &program, &image, &imgsize);
   if (status != ERR_SUCCESS) {
      return status;
   }

   /*
    * The 3rd argument to LoadImage (IN EFI_DEVICE_PATH_PROTOCOL
    * *DevicePath) is not marked OPTIONAL, and trying to pass NULL has
    * been observed to cause a hang later in bs->StartImage in some
    * cases.  So generate something to put there.
    */
   Program = NULL;
   Status = ascii_to_ucs2(program, &Program);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }
   Status = file_devpath(Volume, Program, &ProgramPath);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   /* Use the form of LoadImage that takes a memory buffer */
   ChildHandle = NULL;
   Status = bs->LoadImage(FALSE, ImageHandle, ProgramPath,
                          image, imgsize, &ChildHandle);
   if (EFI_ERROR(Status)) {
      if (ChildHandle != NULL) {
         bs->UnloadImage(ChildHandle);
      }
      return error_efi_to_generic(Status);
   }

   /* Pass the command line, system table, and boot volume to the child */
   Status = image_get_info(ChildHandle, &Child);
   if (EFI_ERROR(Status)) {
      bs->UnloadImage(ChildHandle);
      return error_efi_to_generic(Status);
   }

   Child->LoadOptions = LoadOptions;
   Child->LoadOptionsSize = UCS2SIZE(LoadOptions);
   Child->SystemTable = st;
   Child->DeviceHandle = Volume;

   Log(LOG_DEBUG, "Image loaded at %p (size 0x%"PRIx64")",
       Child->ImageBase, Child->ImageSize);

   if (debug & DEBUG_PAUSE_BEFORE_START_IMAGE) {
      key_code_t key;
      Log(LOG_NOTICE, "Press a key to continue...");
      kbd_waitkey_timeout(&key, 300);
   }

   /* Transfer control to the Child */
   Status = bs->StartImage(ChildHandle, &ExitDataSize, &ExitData);
   if (EFI_ERROR(Status) && ExitData != NULL) {
      Log(LOG_ERR, "StartImage returned Status 0x%zx", Status);
      st->ConOut->OutputString(st->ConOut, ExitData);
   }
   efi_free(ExitData);

   /*
    * Typically the child is a bootloader, in which case we won't get
    * here.
    */
   return error_efi_to_generic(Status);
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
 *      ERR_SUCCESS if the user selected a LOCALBOOT item.
 *      Generic error code on error.
 *----------------------------------------------------------------------------*/
int do_menu(const char *filename)
{
   int status;
   Menu *menu;
   void *buffer;
   size_t bufsize;

   Log(LOG_DEBUG, "do_menu filename=%s", filename);

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

/*-- main ----------------------------------------------------------------------
 *
 *      Main program.  Setup, call do_menu, handle errors.
 *
 * Parameters
 *      IN argc, argv: command line.
 *
 * Results
 *      Does not return if an item was successfully chainloaded.
 *      ERR_SUCCESS if the user selected a LOCALBOOT item.
 *      Generic error code on error.
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
   
   /*
    * Uncomment/modify the following when needed to debug issues prior
    * to option parsing or when options can't be given.
    */
   //verbose = TRUE;
   //debug = DEBUG_PAUSE_BEFORE_PARSE;

   status = log_init(verbose);
   if (status != ERR_SUCCESS) {
      goto out;
   }

   status = get_boot_file(&bootfile);
   if (status != ERR_SUCCESS) {
      goto out;
   }
   homedir = dirname(strdup(bootfile));

   Status = get_boot_volume(&Volume);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   if (argc > 0) {
      int opt;

      optind = 1;
      do {
         opt = getopt(argc, argv, "D:S:s:VH:");
         switch (opt) {
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
            set_verbose(true);
            break;
         case 'H': /* homedir */
            homedir = optarg;
            break;
         case -1:
            break;
         case '?':
         default:
            status = ERR_SYNTAX;
            goto out;
         }
      } while (opt != -1);
   }

   if (serial) {
      serial_log_init(port, baud);
   }

   Log(LOG_DEBUG, "main bootfile=%s homedir=%s argc=%d",
       bootfile, homedir, argc);
   for (i = 0; i < argc; i++) {
      Log(LOG_DEBUG, "argv[%d]=%s", i, argv[i]);
   }

   status = do_menu(optind < argc ? argv[optind] : NULL);

 out:
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Error %d (%s); press a key to continue...",
          status, error_str[status]);
      kbd_waitkey_timeout(&key, 300);
   }
   return status;
}
