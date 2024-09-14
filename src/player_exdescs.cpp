#include "structs.hpp"
#include "utils.hpp"
#include "interpreter.hpp"
#include "player_exdescs.hpp"

/* 
  PCs can have exdescs. They're edited during CUSTOMIZE PHYSICAL with a new 'extra descriptions' menu.

  - Each has a name, a long desc, and a wear slot (stretch: set of wear slots); 'EXDESCS' and its abbreviations are reserved names.
  - Viewed with LOOK/EXAMINE <name> <keyword>
  - Listed with LOOK/EXAMINE <name> EXDESCS
  - If the wear slot has anything in it, you can't look at this exdesc
  - (stretch) When you reveal a wear slot that reveals an exdesc, add "This reveals <exdesc>"

  To answer:
    We want folks to pay syspoints for exdesc usage, when does that happen? Pay sysp to increse your exdesc quota

  // TODO: Write hooks in customize physical for exdescs.



  table: pfiles_exdescs
  idnum
  keyword
  name
  desc
  pk: idnum, keyword; idnum is an indexed column
*/

void syspoints_purchase_exdescs(struct char_data *ch) {
  // TODO
}

int get_purchased_exdesc_max(struct char_data *ch) {
  // TODO
}

void load_exdescs_from_db(struct char_data *ch) {
  // TODO
}

void list_exdescs(struct char_data *viewer, struct char_data *vict) {
  for (auto &exdesc : GET_CHAR_EXDESCS(vict)) {
    send_to_char(viewer, "%s^n (^W%s^n)\r\n", exdesc->get_name(), exdesc->get_desc());
  }
}

bool display_exdesc(struct char_data *viewer, struct char_data *vict, char *keyword) {
  // Search their exdescs for this keyword. If found, display it. Return TRUE.
  for (auto &exdesc : GET_CHAR_EXDESCS(vict)) {
    if (is_abbrev(keyword, exdesc->get_keyword())) {
      send_to_char(viewer, "%s^n:\r\n%s^n\r\n", exdesc->get_name(), exdesc->get_desc());
      return TRUE;
    }
  }

  // If not found, return FALSE.
  return FALSE;
}

// Given an arg in the format of 'Lucien hands', drop the first word and process the subsequent for exdesc viewing.
bool look_at_exdescs(struct char_data *viewer, struct char_data *vict, char *arg) {
  // Case 0: Empty string, or vict has no exdescs. Return FALSE.
  if (!arg || !*arg || !CHAR_HAS_EXDESCS(vict)) {
    return FALSE;
  }
  
  // Case 1: String does not have another argument in it. Return FALSE.
  char discard[MAX_INPUT_LENGTH];
  char *keyword = any_one_arg(arg, discard);
  if (!keyword || !*keyword) {
    return FALSE;
  }

  // Case 2: String has EXDESC as its keyword. List exdescs. Return TRUE.
  if (is_abbrev(keyword, "exdescs")) {
    send_to_char(viewer, "%s has the following extra descriptions:\r\n", CAP(HSSH(vict)));
    list_exdescs(viewer, vict);
    return TRUE;
  }

  // Case 3: String has a valid keyword. Show that exdesc. Return TRUE.
  if (display_exdesc(viewer, vict, keyword)) {
    return TRUE;
  }

  // Default case: String has an invalid keyword. Give an error and list exdescs. Return TRUE.
  send_to_char(viewer, "'%s' isn't a valid exdesc. You can pick one from the following list.\r\n", keyword);
  list_exdescs(viewer, vict);
  return TRUE;
}