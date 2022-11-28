#include "awake.hpp"
#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "newhouse.hpp"

void houseedit_import() {
  // TODO
}

void houseedit_reload(struct room_data *room) {
  // TODO
}

// TODO: Question-- if we destroy leases when the player doesn't exist, but we expect updates to house structure to persist between buildport and normal, how do we handle players not existing on buildport?
// Potential ans: Ifdef buildport, don't destroy anything, just assume it's kosher and roll with it.

ACMD(do_houseedit) {
  char mode[100];
  char *remainder = one_argument(argument, mode);

  if (is_abbrev(mode, "import")) {
    // Import apartments from old format to new format. Destructive!
    FAILURE_CASE(GET_LEVEL(ch) < LVL_PRESIDENT, "You're not erudite enough to do that.");
    FAILURE_CASE(!ch->in_room || !ch->in_room->apartment, "You must be standing in an apartment for that.");
    FAILURE_CASE(str_cmp(remainder, "confirm"), "Syntax: HOUSEEDIT IMPORT CONFIRM.");

    mudlog_vfprintf(ch, LOG_SYSLOG, "House import started by %s.", GET_CHAR_NAME(ch));
    houseedit_import();
    mudlog("House import completed.", ch, LOG_SYSLOG, TRUE);
    return;
  }

  if (is_abbrev(mode, "reload")) {
    // Reload the storage for the subroom you're currently standing in.
    FAILURE_CASE(GET_LEVEL(ch) < LVL_EXECUTIVE, "You're not erudite enough to do that.");
    FAILURE_CASE(!ch->in_room || !ch->in_room->apartment, "You must be standing in an apartment for that.");
    FAILURE_CASE(str_cmp(remainder, "confirm"), "Syntax: HOUSEEDIT RELOAD CONFIRM.");

    mudlog_vfprintf(ch, LOG_SYSLOG, "House reload for %s started by %s.", ch->in_room->apartment->get_full_name(), GET_CHAR_NAME(ch));
    houseedit_reload(ch->in_room);
    mudlog("House reload completed.", ch, LOG_SYSLOG, TRUE);
    return;
  }

  if (is_abbrev(mode, "complex")) {
    /* - houseedit complex create: (OLC menu, specify name, shortname, and landlord vnum)
       - houseedit complex delete <name> (deletes a complex, but only if it has no apartments)
       - houseedit complex list: (list all complexes)
       - houseedit complex <name>: (OLC menu)
    */
    if (is_abbrev(mode, "create")) {
      // TODO: create new complex
    }
    else if (is_abbrev(mode, "delete")) {
      FAILURE_CASE(GET_LEVEL(ch) < LVL_ADMIN, "You're not erudite enough to do that.");
      // TODO: delete existing complex (empty / has no apartments)
    }
    else if (is_abbrev(mode, "list")) {
      // TODO: list complexes
    }
    else {
      // TODO: edit existing complex (or the one you're standing in if no name given)
    }

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
    if (is_abbrev(mode, "create")) {
      // TODO: create new apartment in named complex
    }
    else if (is_abbrev(mode, "delete")) {
      FAILURE_CASE(GET_LEVEL(ch) < LVL_ADMIN, "You're not erudite enough to do that.");
      // TODO: delete existing apartment (cannot be leased)
    }
    else if (is_abbrev(mode, "list")) {
      // TODO: list apartments in named complex
    }
    else {
      // TODO: edit named apartment (or the one you're standing in if no name given)
      // TODO: add room via `houseedit apartment <name> addroom <room name>`
      // TODO: delete room via `houseedit apartment <name> delroom`
    }

    return;
  }

  send_to_char(ch, "Valid modes are IMPORT, RELOAD, COMPLEX, APARTMENT.\r\n");
  return;
}
