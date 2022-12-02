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
void houseedit_reload(struct char_data *ch, const char *filename);

ACMD(do_houseedit) {
  char mode[100], func[100];
  char *mode_remainder = one_argument(argument, mode);
  char *func_remainder = one_argument(mode_remainder, func);

  if (is_abbrev(mode, "import")) {
    // Import apartments from old format to new format. Destructive!
    FAILURE_CASE(GET_LEVEL(ch) < LVL_PRESIDENT, "You're not erudite enough to do that.");
    FAILURE_CASE(!ch->in_room || !GET_APARTMENT(ch->in_room), "You must be standing in an apartment for that.");
    FAILURE_CASE(str_cmp(func, "confirm"), "To blow away existing apartments and load from old files, HOUSEEDIT IMPORT CONFIRM.");


    houseedit_import(ch);
    return;
  }

  if (is_abbrev(mode, "reload")) {
    // Reload the storage for the subroom you're currently standing in.
    FAILURE_CASE(GET_LEVEL(ch) < LVL_EXECUTIVE, "You're not erudite enough to do that.");
    FAILURE_CASE(!ch->in_room || !GET_APARTMENT_SUBROOM(ch->in_room), "You must be standing in an apartment for that.");

    houseedit_reload(ch, func_remainder);
    return;
  }

  if (is_abbrev(mode, "complex")) {
    // List the complexes in game.
    if (is_abbrev(func, "list")) {
      houseedit_list_complexes(ch, func_remainder);
      return;
    }

    // Create a new complex.
    if (is_abbrev(func, "create")) {
      FAILURE_CASE(!PLR_FLAGGED(ch, PLR_OLC) && !access_level(ch, LVL_PRESIDENT), YOU_NEED_OLC_FOR_THAT);

      houseedit_create_complex(ch);
      return;
    }

    // Delete an existing empty complex.
    if (is_abbrev(func, "delete")) {
      FAILURE_CASE(!PLR_FLAGGED(ch, PLR_OLC) && !access_level(ch, LVL_PRESIDENT), YOU_NEED_OLC_FOR_THAT);
      FAILURE_CASE(GET_LEVEL(ch) < LVL_ADMIN, "You're not erudite enough to do that.");

      houseedit_delete_complex(ch, func_remainder);
      return;
    }

    // Edit an existing complex.
    if (is_abbrev(func, "edit")) {
      FAILURE_CASE(!PLR_FLAGGED(ch, PLR_OLC) && !access_level(ch, LVL_PRESIDENT), YOU_NEED_OLC_FOR_THAT);

      houseedit_edit_existing_complex(ch, func_remainder);
      return;
    }

    send_to_char("Valid modes are COMPLEX LIST / CREATE / DELETE / EDIT.\r\n", ch);
    return;
  }

  else if (is_abbrev(mode, "apartment")) {
    // List existing apartments in the given complex.
    if (is_abbrev(func, "list")) {
      houseedit_list_apartments(ch, func_remainder);
      return;
    }

    // Create a new apartment in the complex you're standing in, or the named one provided
    if (is_abbrev(func, "create")) {
      FAILURE_CASE(!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC), YOU_NEED_OLC_FOR_THAT);

      houseedit_create_apartment(ch, func_remainder);
      return;
    }

    // Delete an empty, non-leased apartment you're standing in, or one matching the full name provided.
    if (is_abbrev(func, "delete")) {
      FAILURE_CASE(!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC), YOU_NEED_OLC_FOR_THAT);
      FAILURE_CASE(GET_LEVEL(ch) < LVL_ADMIN, "You're not erudite enough to do that.");

      houseedit_delete_apartment(ch, func_remainder);
      return;
    }

    // Edit the apartment you're standing in, or one matching the full name provided.
    if (is_abbrev(func, "edit")) {
      FAILURE_CASE(!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC), YOU_NEED_OLC_FOR_THAT);

      houseedit_edit_apartment(ch, func_remainder);
      return;
    }

    send_to_char("Valid modes are APARTMENT LIST / CREATE / DELETE / EDIT.\r\n", ch);
    return;
  }

  send_to_char(ch, "Valid modes are IMPORT, RELOAD, COMPLEX, APARTMENT.\r\n");
  return;
}

// Overwrite this room's storage with what's on disk.
void houseedit_reload(struct char_data *ch, const char *filename) {
  FAILURE_CASE(!ch->in_room || !GET_APARTMENT_SUBROOM(ch->in_room), "You must be standing in an apartment for that.");
  FAILURE_CASE(!filename || !*filename, "Syntax: ^WHOUSEEDIT RELOAD <filename to load from>^n");

  // Sanitize filename.
  for (const char *c = filename; *c; c++)
    FAILURE_CASE(!string_is_valid_for_paths(filename), "That's not a filename. You must specify a filename that exists at this subroom's base directory.");

  //  Verify that file exists.
  bf::path new_path = GET_APARTMENT_SUBROOM(ch->in_room)->get_base_directory() / filename;
  if (!exists(new_path)) {
    send_to_char(ch, "There is no file at path '%s'.\r\n", new_path.string().c_str());
    return;
  }

  // Log and load.
  mudlog_vfprintf(ch, LOG_SYSLOG, "House reload for %s started by %s.", GET_APARTMENT_SUBROOM(ch->in_room)->get_full_name(), GET_CHAR_NAME(ch));
  GET_APARTMENT_SUBROOM(ch->in_room)->load_storage_from_specified_path(new_path);
  mudlog("House reload completed.", ch, LOG_SYSLOG, TRUE);
}

// Load old house files, parse, and transfer to new format.
void houseedit_import(struct char_data *ch) {
  mudlog_vfprintf(ch, LOG_SYSLOG, "House import started by %s.", GET_CHAR_NAME(ch));
  // TODO
  mudlog("House import completed.", ch, LOG_SYSLOG, TRUE);
}
