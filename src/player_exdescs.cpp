#include <algorithm>

#include "structs.hpp"
#include "utils.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "newdb.hpp"
#include "constants.hpp"
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
  // TODO: Editing flow. Do we allow changes of keywords, which are primary keys?
  // TODO: Add visibility filters for looking at someone with descs, listing them, etc: if covered, don't show
  // TODO STRETCH: Alter wear/remove to add reveal/hide for exdescs.


  table: pfiles_exdescs
  idnum
  keyword
  name
  desc
  pk: idnum, keyword; idnum is an indexed column
*/

#define SYSP_EXDESC_MAX_PURCHASE_COST 10
#define SYSP_EXDESC_MAX_PURCHASE_GETS_YOU_X_SLOTS 5

// Prototypes.
void list_exdescs(struct char_data *viewer, struct char_data *vict);
class PCExDesc *find_exdesc_by_keyword(struct char_data *ch, const char *keyword);

/*
 * Staff DO_EXDESC command.
 */
#define DO_EXDESC_USAGE_STRING "Syntax:\r\n  EXDESC LIST <target>\r\n  EXDESC SHOW <target> [keyword]\r\n  EXDESC DELETE <target> <keyword>"
ACMD(do_exdesc) {
  char command[MAX_INPUT_LENGTH] = { '\0' };
  char target_name[MAX_INPUT_LENGTH] = { '\0' };
  char keyword[MAX_INPUT_LENGTH] = { '\0' };
  
  struct char_data *vict = NULL;
  char *remainder;
  class PCExDesc *exdesc;
  
  FAILURE_CASE(!*argument, DO_EXDESC_USAGE_STRING);

  // Parse out command.
  remainder = any_one_arg(argument, command);
  FAILURE_CASE(!remainder || !*remainder, DO_EXDESC_USAGE_STRING);

  // Select target.
  remainder = any_one_arg(remainder, target_name);
  FAILURE_CASE_PRINTF(!(vict = get_player_vis(ch, target_name, FALSE)), "You don't see an online character named %s.", target_name);
  FAILURE_CASE_PRINTF(IS_NPC(vict), "%s is an NPC, and this is for PCs only.", GET_CHAR_NAME(vict));
  FAILURE_CASE_PRINTF(GET_LEVEL(vict) > GET_LEVEL(ch), "%s is above your level.", GET_CHAR_NAME(vict));

  // EXDESC LIST <target>
  if (is_abbrev(command, "list")) {
    list_exdescs(ch, vict);
    return;
  }

  remainder = any_one_arg(remainder, keyword);

  // EXDESC SHOW <target> [keyword].
  if (is_abbrev(command, "show") || is_abbrev(command, "view") || is_abbrev(command, "look")) {
    // No keyword
    if (!*keyword || *keyword == '*' || !str_cmp(keyword, "all")) {
      list_exdescs(ch, vict);
      send_to_char(ch, "\r\nTo inspect a specific one, use ^WEXDESC SHOW %s <desc>^n\r\n", GET_CHAR_NAME(vict));
      return;
    }

    // Keyword specified
    FAILURE_CASE_PRINTF(!(exdesc = find_exdesc_by_keyword(vict, keyword)), "%s is not a keyword of an exdesc on %s.", keyword, GET_CHAR_NAME(vict));

    send_to_char(ch, "%s's %s:\r\n", GET_CHAR_NAME(vict), exdesc->get_keyword());
    send_to_char(ch, "%s\r\n", exdesc->get_name());
    send_to_char(ch, "%s\r\n", exdesc->get_desc());

    if (strcmp(exdesc->get_wear_slots().ToString(), "0") != 0) {
      char bitprintbuf[MAX_STRING_LENGTH];
      exdesc->get_wear_slots().PrintBits(bitprintbuf, MAX_STRING_LENGTH, wear_bits, NUM_WEARS);
      send_to_char(ch, "It is visible on the following locations (bitstring %s):\r\n  ^c%s^n\r\n",
                   exdesc->get_wear_slots().ToString(),
                   bitprintbuf);
    } else {
      send_to_char("It has ^yno locations^n (bitstring 0).\r\n", ch);
    }
    return;
  }

  // EXDESC SET <target> <keyword> [wearloc_bitstring] <desc>. This is only used for debugging.
  if (is_abbrev(command, "set") || is_abbrev(command, "edit")) {
    char wearloc_bitstring[MAX_INPUT_LENGTH] = { '\0' };

    FAILURE_CASE_PRINTF(!*keyword, "Syntax: EXDESC SET %s <keyword> [wearloc bitstring] <desc>", GET_CHAR_NAME(vict))
    FAILURE_CASE_PRINTF(!*remainder, "Syntax: EXDESC SET %s %s [wearloc bitstring] <desc>", GET_CHAR_NAME(vict), keyword);

    skip_spaces(&remainder);

    if (*remainder == '0' || *remainder == '1') {
      remainder = any_one_arg(remainder, wearloc_bitstring);
      skip_spaces(&remainder);
    }

    FAILURE_CASE_PRINTF(!*remainder, "Syntax: EXDESC SET %s %s%s%s <desc>",
                        GET_CHAR_NAME(vict),
                        keyword,
                        *wearloc_bitstring ? " " : "",
                        *wearloc_bitstring ? wearloc_bitstring : "");

    // Look up the desc to edit.
    exdesc = find_exdesc_by_keyword(vict, keyword);

    // If it didn't exist, create a new one
    if (!exdesc) {
      FAILURE_CASE(!*wearloc_bitstring, "You must specify the bitstring when creating a new exdesc like this (sorry)");

      exdesc = new PCExDesc(GET_IDNUM(vict));

      exdesc->set_keyword(keyword);
      exdesc->set_name("An unnamed exdesc");
      exdesc->set_desc(remainder);
      exdesc->set_wear_slots(wearloc_bitstring);

      exdesc->save_to_db();

      GET_CHAR_EXDESCS(ch).push_back(exdesc);
    }
    // It did exist, so edit it instead.
    else {
      exdesc->set_keyword(keyword);
      // No name change.
      exdesc->set_desc(remainder);

      if (*wearloc_bitstring)
        exdesc->set_wear_slots(wearloc_bitstring);

      exdesc->save_to_db();
    }
    send_to_char(OK, ch);
    return;
  }

  // EXDESC DELETE <target> <keyword>
  if (is_abbrev(command, "delete") || is_abbrev(command, "drop") || is_abbrev(command, "remove")) {
    FAILURE_CASE_PRINTF(!*keyword, "Syntax: EXDESC DELETE %s <keyword>", GET_CHAR_NAME(vict))
    FAILURE_CASE_PRINTF(!(exdesc = find_exdesc_by_keyword(vict, keyword)), "%s has no exdesc with the keyword %s.", GET_CHAR_NAME(vict), keyword);

    mudlog_vfprintf(ch, LOG_WIZLOG, "%s removing exdesc %s (%s) from %s.", GET_CHAR_NAME(ch), exdesc->get_keyword(), exdesc->get_name(), GET_CHAR_NAME(vict));

    exdesc->delete_from_db();

    GET_CHAR_EXDESCS(vict).erase(
      std::remove(GET_CHAR_EXDESCS(vict).begin(), GET_CHAR_EXDESCS(vict).end(), exdesc),
      GET_CHAR_EXDESCS(vict).end()
    );

    send_to_char(OK, ch);
    return;
  }

  send_to_char(ch, "%s\r\n", DO_EXDESC_USAGE_STRING);
  return;
}

/*
 * Code for handling exdesc maximums.
 */

// Called from do_syspoints() when the PC enters SYSPOINTS EXDESCS. Set is_confirmed if invoking from customize physical.
void syspoints_purchase_exdescs(struct char_data *ch, char *buf, bool is_confirmed) {
  // Can they afford it?
  FAILURE_CASE_PRINTF(GET_SYSTEM_POINTS(ch) < SYSP_EXDESC_MAX_PURCHASE_COST,
                      "You can spend %d system points to increase your extra description maximum by %d. You don't have enough yet (only %d on hand)",
                      SYSP_EXDESC_MAX_PURCHASE_COST, SYSP_EXDESC_MAX_PURCHASE_GETS_YOU_X_SLOTS, GET_SYSTEM_POINTS(ch));

  // Have they entered the confirmation command?
  FAILURE_CASE_PRINTF(!is_confirmed && !is_abbrev(buf, "confirm"),
                      "You can spend %d system points to increase your extra description maximum by %d. To do so, type SYSPOINT PURCHASE CONFIRM.",
                      SYSP_EXDESC_MAX_PURCHASE_COST, SYSP_EXDESC_MAX_PURCHASE_GETS_YOU_X_SLOTS);

  // Do it.
  int existing_max = get_exdesc_max(ch);
  set_exdesc_max(ch, existing_max + SYSP_EXDESC_MAX_PURCHASE_GETS_YOU_X_SLOTS, TRUE);

  GET_SYSTEM_POINTS(ch) -= SYSP_EXDESC_MAX_PURCHASE_COST;

  mudlog_vfprintf(ch, LOG_GRIDLOG, "%s spent %d syspoints to increase exdesc max (%d -> %d), now has %d sysp",
                  GET_CHAR_NAME(ch),
                  SYSP_EXDESC_MAX_PURCHASE_COST,
                  existing_max,
                  get_exdesc_max(ch),
                  GET_SYSTEM_POINTS(ch));
  
  playerDB.SaveChar(ch);

  send_to_char(ch, "OK, your exdesc maximum has increased from %d to %d. You have %d syspoints remaining.",
               existing_max,
               get_exdesc_max(ch),
               GET_SYSTEM_POINTS(ch));
}

// Given a character and an amount, set their exdesc maximum to that amount. save_to_db is only false when loading from DB.
void set_exdesc_max(struct char_data *ch, int amount, bool save_to_db) {
  // TODO
}

int get_exdesc_max(struct char_data *ch) {
  // TODO
  return 100;
}

/*
 * Code for display of exdescs.
 */

void list_exdescs(struct char_data *viewer, struct char_data *vict) {
  if (GET_CHAR_EXDESCS(vict).empty()) {
    if (vict == viewer) {
      send_to_char("You have no extra descriptions set.\r\n", viewer);
    } else {
      act("$N has no extra descriptions set.", FALSE, viewer, NULL, vict, TO_CHAR);
    }
    return;
  }

  if (vict == viewer) {
    send_to_char("You have the following extra descriptions:\r\n", viewer);
  } else {
    act("$N has the following extra descriptions:\r\n", FALSE, viewer, NULL, vict, TO_CHAR);
  }

  for (auto &exdesc : GET_CHAR_EXDESCS(vict)) {
    send_to_char(viewer, "%s^n (^W%s^n)\r\n", exdesc->get_name(), exdesc->get_keyword());
  }
}

// Given a target and a specified keyword, search for that keyword first exactly, then as an abbreviation. Returns desc ptr if found, NULL otherwise.
class PCExDesc *find_exdesc_by_keyword(struct char_data *ch, const char *keyword) {
  // Look for an exact match first.
  for (auto &exdesc : GET_CHAR_EXDESCS(ch)) {
    if (!str_cmp(keyword, exdesc->get_keyword())) {
      return exdesc;
    }
  }

  // Failed to find an exact match. Look for a fuzzy match by abbreviation.
  for (auto &exdesc : GET_CHAR_EXDESCS(ch)) {
    if (is_abbrev(keyword, exdesc->get_keyword())) {
      return exdesc;
    }
  }

  // Found nothing.
  return NULL;
}

// Search vict's exdescs for the specified keyword. If found, display it and return TRUE. Return FALSE otherwise.
bool display_exdesc(struct char_data *viewer, struct char_data *vict, const char *keyword) {
  class PCExDesc *exdesc;;

  if ((exdesc = find_exdesc_by_keyword(vict, keyword))) {
    send_to_char(viewer, "%s^n:\r\n%s^n\r\n", exdesc->get_name(), exdesc->get_desc());
    return TRUE;
  }

  return FALSE;
}

#define EXDESCS_DEBUG(...) {                          \
  if (viewer && IS_SENATOR(viewer)) {                 \
    send_to_char(viewer, __VA_ARGS__);                \
    send_to_char(viewer, "\r\n"); /*force a newline*/ \
  }                                                   \
}                                                     \
// Given an arg in the format of 'hands' or 'exdesc', drop the first word and process the subsequent for exdesc viewing.
bool look_at_exdescs(struct char_data *viewer, struct char_data *vict, char *keyword) {
  EXDESCS_DEBUG("Entering look_at_exdescs(%s, %s, '%s')", GET_CHAR_NAME(viewer), GET_CHAR_NAME(vict), keyword);

  // Case 1: Empty string, or vict has no exdescs. Return FALSE.
  if (!keyword || !*keyword) {
    EXDESCS_DEBUG("No keyword provided");
    return FALSE;
  }

  // Case 2: String has EXDESC as its keyword. List exdescs. Return TRUE.
  if (is_abbrev(keyword, "exdescs")
      || is_abbrev(keyword, "extras")
      || (is_abbrev(keyword, "descriptions") && str_cmp(keyword, "desc") && str_cmp(keyword, "description"))
     )
  {
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

/*
 * Code for handling database-related things.
 */

// Saving happens here. Requires pc_idnum and keyword to be set. Invoke during editing.
void PCExDesc::save_to_db() {

}

// Call this to delete this entry. Requires pc_idnum and keyword to be set. Invoke during editing when char chooses to delete.
void PCExDesc::delete_from_db() {

}

void load_exdescs_from_db(struct char_data *ch) {
  // TODO
}