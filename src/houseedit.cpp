#include "awake.hpp"
#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "newhouse.hpp"
#include "houseedit_complex.hpp"
#include "houseedit_apartment.hpp"

void houseedit_import(struct char_data *ch);
void houseedit_reload(struct char_data *ch);

ACMD(do_houseedit) {
  char mode[100], func[100];
  char *mode_remainder = one_argument(argument, mode);
  char *func_remainder = one_argument(mode_remainder, func);

  if (is_abbrev(mode, "import")) {
    // Import apartments from old format to new format. Destructive!
    FAILURE_CASE(GET_LEVEL(ch) < LVL_PRESIDENT, "You're not erudite enough to do that.");
    FAILURE_CASE(!ch->in_room || !ch->in_room->apartment, "You must be standing in an apartment for that.");
    FAILURE_CASE(str_cmp(func, "confirm"), "To blow away existing apartments and load from old files, HOUSEEDIT IMPORT CONFIRM.");


    houseedit_import(ch);
    return;
  }

  if (is_abbrev(mode, "reload")) {
    // Reload the storage for the subroom you're currently standing in.
    FAILURE_CASE(GET_LEVEL(ch) < LVL_EXECUTIVE, "You're not erudite enough to do that.");
    FAILURE_CASE(str_cmp(func, "confirm"), "To reload apartment storage for your current room, HOUSEEDIT RELOAD CONFIRM.");

    houseedit_reload(ch);
    return;
  }

  if (is_abbrev(mode, "complex")) {
    /* - houseedit complex create: (OLC menu, specify name, shortname, and landlord vnum)
       - houseedit complex delete <name> (deletes a complex, but only if it has no apartments)
       - houseedit complex list: (list all complexes)
       - houseedit complex <name>: (OLC menu)
    */
    // You can list even without OLC.
    if (is_abbrev(func, "list")) {
      houseedit_list_complexes(ch, func_remainder);
      return;
    }

    if (is_abbrev(func, "create")) {
      FAILURE_CASE(!PLR_FLAGGED(ch, PLR_OLC) && !access_level(ch, LVL_PRESIDENT), YOU_NEED_OLC_FOR_THAT);

      houseedit_create_complex(ch);
      return;
    }

    if (is_abbrev(func, "delete")) {
      FAILURE_CASE(!PLR_FLAGGED(ch, PLR_OLC) && !access_level(ch, LVL_PRESIDENT), YOU_NEED_OLC_FOR_THAT);
      FAILURE_CASE(GET_LEVEL(ch) < LVL_ADMIN, "You're not erudite enough to do that.");

      houseedit_delete_complex(ch, func_remainder);
      return;
    }

    if (is_abbrev(func, "edit")) {
      FAILURE_CASE(!PLR_FLAGGED(ch, PLR_OLC) && !access_level(ch, LVL_PRESIDENT), YOU_NEED_OLC_FOR_THAT);

      houseedit_edit_existing_complex(ch, func_remainder);
      return;
    }

    send_to_char("Valid modes are COMPLEX LIST / CREATE / DELETE / EDIT.\r\n", ch);
    return;
  }

  else if (is_abbrev(mode, "apartment")) {
    /* - houseedit apartment create <complex>: (OLC menu)
       - houseedit apartment list <complex>
       - houseedit apartment delete <name> (deletes apartment, but only if not leased)
       - houseedit apartment <name>: (OLC menu)
       - houseedit apartment <name> addroom <room name>: (adds/overrides current room, sets current desc)
       - houseedit apartment <name> delroom: (deletes current room from apartment)
    */

    if (is_abbrev(func, "list")) {
      // TODO: list apartments in named complex
      return;
    }

    if (is_abbrev(func, "create")) {
      FAILURE_CASE(!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC), YOU_NEED_OLC_FOR_THAT);
      // TODO: create new apartment in named complex
      return;
    }

    if (is_abbrev(func, "delete")) {
      FAILURE_CASE(!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC), YOU_NEED_OLC_FOR_THAT);
      FAILURE_CASE(GET_LEVEL(ch) < LVL_ADMIN, "You're not erudite enough to do that.");
      // TODO: delete existing apartment (cannot be leased)
      return;
    }

    if (is_abbrev(func, "edit")) {
      FAILURE_CASE(!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC), YOU_NEED_OLC_FOR_THAT);
      // TODO: edit named apartment (or the one you're standing in if no name given)
      // TODO: add room via `houseedit apartment <name> addroom <room name>`
      // TODO: delete room via `houseedit apartment <name> delroom`
      return;
    }

    send_to_char("Valid modes are APARTMENT LIST / CREATE / DELETE / EDIT.\r\n", ch);
    return;
  }

  send_to_char(ch, "Valid modes are IMPORT, RELOAD, COMPLEX, APARTMENT.\r\n");
  return;
}

void houseedit_import(struct char_data *ch) {
  FAILURE_CASE(!ch->in_room || !ch->in_room->apartment, "You must be standing in an apartment for that.");

  mudlog_vfprintf(ch, LOG_SYSLOG, "House import started by %s.", GET_CHAR_NAME(ch));
  // TODO
  mudlog("House import completed.", ch, LOG_SYSLOG, TRUE);
}

void houseedit_reload(struct char_data *ch) {
  FAILURE_CASE(!ch->in_room || !ch->in_room->apartment, "You must be standing in an apartment for that.");

  // TODO: Verify that file exists.

  mudlog_vfprintf(ch, LOG_SYSLOG, "House reload for %s started by %s.", ch->in_room->apartment->get_full_name(), GET_CHAR_NAME(ch));
  // TODO: Read from file via load_storage_from_specified_path().
  mudlog("House reload completed.", ch, LOG_SYSLOG, TRUE);
}
