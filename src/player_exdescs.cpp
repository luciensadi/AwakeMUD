#include <algorithm>
#include <map>
#include <vector>

#include "structs.hpp"
#include "utils.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "newdb.hpp"
#include "constants.hpp"
#include "olc.hpp"
#include "moderation.hpp"
#include "player_exdescs.hpp"

/* 
  PCs can have exdescs. They're edited during CUSTOMIZE PHYSICAL with a new 'extra descriptions' menu.

  - Each has a name, a long desc, and a wear slot (stretch: set of wear slots); 'EXDESCS' and its abbreviations are reserved names.
  - Viewed with LOOK/EXAMINE <name> <keyword>
  - Listed with LOOK/EXAMINE <name> EXDESCS
  - If the wear slot has anything in it, you can't look at this exdesc
  - (stretch) When you reveal a wear slot that reveals an exdesc, add "This reveals <exdesc>"

  To answer:
    We want folks to pay syspoints for exdesc usage, when does that happen? Pay sysp to increase your exdesc quota

  // TODO: Add visibility filters for looking at someone with descs, listing them, etc: if covered, don't show
  // TODO STRETCH: Alter wear/remove to add reveal/hide for exdescs.


  table: pfiles_exdescs
  idnum
  keyword
  name
  desc
  wearslots
  pk: idnum, keyword; idnum is an indexed column
*/

#define SYSP_EXDESC_MAX_PURCHASE_COST 10
#define SYSP_EXDESC_MAX_PURCHASE_GETS_YOU_X_SLOTS 1

extern MYSQL *mysql;
extern std::map<std::string, int> wear_flag_map_for_exdescs;

extern void cedit_disp_menu(struct descriptor_data *d, int mode);
extern const char *remove_final_punctuation(const char *str);

// Prototypes.
void list_exdescs(struct char_data *viewer, struct char_data *vict, bool list_is_for_editing=FALSE);
class PCExDesc *find_exdesc_by_keyword(struct char_data *ch, const char *keyword, bool allow_fuzzy_match=TRUE);
bool delete_exdesc_by_keyword(struct char_data *ch, const char *keyword);
void save_pc_exdesc_max(struct char_data *ch);

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
      send_to_char(ch, "\r\nTo inspect a specific one, use ^WEXDESC SHOW %s <keyword>^n\r\n", GET_CHAR_NAME(vict));
      return;
    }

    // Keyword specified
    FAILURE_CASE_PRINTF(!(exdesc = find_exdesc_by_keyword(vict, keyword)), "%s is not a keyword of an exdesc on %s.", keyword, GET_CHAR_NAME(vict));

    send_to_char(ch, "%s's %s:\r\n", GET_CHAR_NAME(vict), exdesc->get_keyword());
    send_to_char(ch, "%s\r\n", exdesc->get_name());
    send_to_char(ch, "%s\r\n", exdesc->get_desc());

    if (strcmp(exdesc->get_wear_slots()->ToString(), "0") != 0) {
      char bitprintbuf[MAX_STRING_LENGTH];
      exdesc->get_wear_slots()->PrintBits(bitprintbuf, MAX_STRING_LENGTH, wear_bits, NUM_WEARS);
      send_to_char(ch, "It is visible on the following locations (bitstring %s):\r\n  ^c%s^n\r\n",
                   exdesc->get_wear_slots()->ToString(),
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

      GET_CHAR_EXDESCS(vict).push_back(exdesc);
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
  int existing_max = GET_CHAR_MAX_EXDESCS(ch);
  set_exdesc_max(ch, existing_max + SYSP_EXDESC_MAX_PURCHASE_GETS_YOU_X_SLOTS, TRUE);

  GET_SYSTEM_POINTS(ch) -= SYSP_EXDESC_MAX_PURCHASE_COST;

  mudlog_vfprintf(ch, LOG_GRIDLOG, "%s spent %d syspoints to increase exdesc max (%d -> %d), now has %d sysp",
                  GET_CHAR_NAME(ch),
                  SYSP_EXDESC_MAX_PURCHASE_COST,
                  existing_max,
                  GET_CHAR_MAX_EXDESCS(ch),
                  GET_SYSTEM_POINTS(ch));
  
  playerDB.SaveChar(ch);

  send_to_char(ch, "OK, your exdesc maximum has increased from %d to %d. You have %d syspoints remaining.",
               existing_max,
               GET_CHAR_MAX_EXDESCS(ch),
               GET_SYSTEM_POINTS(ch));
}

// Given a character and an amount, set their exdesc maximum to that amount. save_to_db is only false when loading from DB.
void set_exdesc_max(struct char_data *ch, int amount, bool save_to_db) {
  if (!ch || IS_NPC(ch)) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Attempted to set_exdesc_max on invalid char %s.", GET_CHAR_NAME(ch));
    return;
  }

  if (amount < 2) {
    // Silently make it 2.
    amount = 2;
  }

  GET_CHAR_MAX_EXDESCS(ch) = amount;

  if (save_to_db) {
    save_pc_exdesc_max(ch);
  }
}

/*
 * Code for display of exdescs.
 */
bool can_see_exdesc(struct char_data *viewer, struct char_data *vict, PCExDesc *exdesc, bool without_staff_override=FALSE) {
  if (!vict->player_specials)
    return FALSE;

  if (viewer && IS_SENATOR(viewer) && !without_staff_override)
    return TRUE;

  Bitfield comparison_field;
  comparison_field.SetAll(*(exdesc->get_wear_slots()));
  comparison_field.RemoveAll(GET_CHAR_COVERED_WEARLOCS(vict));

  // Additional filter-downs
  if (comparison_field.IsSet(ITEM_WEAR_UNDERWEAR)) {
    if (GET_CHAR_COVERED_WEARLOCS(vict).AreAnySet(ITEM_WEAR_UNDER, ITEM_WEAR_LEGS, ITEM_WEAR_BODY, ITEM_WEAR_ABOUT, ENDBIT)) {
      comparison_field.RemoveBit(ITEM_WEAR_UNDERWEAR);
    }
  }

  if (comparison_field.AreAnySet(ITEM_WEAR_CHEST, ITEM_WEAR_BACK, ITEM_WEAR_BELLY, ENDBIT)) {
    if (GET_CHAR_COVERED_WEARLOCS(vict).AreAnySet(ITEM_WEAR_UNDER, ITEM_WEAR_BODY, ITEM_WEAR_ABOUT, ENDBIT)) {
      comparison_field.RemoveBits(ITEM_WEAR_CHEST, ITEM_WEAR_BACK, ITEM_WEAR_BELLY, ENDBIT);
    }
  }

  return comparison_field.HasAnythingSetAtAll();
}

std::vector<PCExDesc *> exdescs_with_visibility_changed_by_wearloc(struct char_data *vict, int wearloc, bool check_for_reveal) {
  static std::vector<PCExDesc *> result = {0};
  result.clear();

  // send_to_char(vict, "exdescs_with_visibility_changed_by_wearloc(%s, %d, %s)\r\n", GET_CHAR_NAME(vict), wearloc, check_for_reveal ? "T" : "F");

  if (!vict->player_specials) {
    // send_to_char(vict, "no specials\r\n");
    return result;
  }

  // Check for precondition failures (you're wearing something else that occludes this, so change will not happen)
  // Precondition: it's underwear and you're wearing something that occludes underwear
  if (wearloc == ITEM_WEAR_UNDERWEAR) {
    if (GET_CHAR_COVERED_WEARLOCS(vict).AreAnySet(ITEM_WEAR_UNDER, ITEM_WEAR_LEGS, ITEM_WEAR_BODY, ITEM_WEAR_ABOUT, ENDBIT)) {
#ifdef IS_BUILDPORT
      send_to_char(vict, "debug: no alteration to exdesc reveal, concealed by other slots\r\n");
#endif
      return result;
    }
  }

  // Precondition: it's chest and you're wearing something that occludes chest
  if (wearloc == ITEM_WEAR_CHEST || wearloc == ITEM_WEAR_BACK || wearloc == ITEM_WEAR_BELLY) {
    if (GET_CHAR_COVERED_WEARLOCS(vict).AreAnySet(ITEM_WEAR_UNDER, ITEM_WEAR_BODY, ITEM_WEAR_ABOUT, ENDBIT)) {
#ifdef IS_BUILDPORT
      send_to_char(vict, "debug: no alteration to exdesc reveal, concealed by other slots\r\n");
#endif
      return result;
    }
  }
  
  for (auto exdesc : GET_CHAR_EXDESCS(vict)) {
    Bitfield comparison_field;
    comparison_field.SetAll(*(exdesc->get_wear_slots()));
    comparison_field.RemoveAll(GET_CHAR_COVERED_WEARLOCS(vict));

    int num_already_set = comparison_field.GetNumSet();

    send_to_char(vict, "DEBUG: Comparing '%s's wearlocs of %s to your already-covered wearlocs of %s yields %s (%d set).\r\n",
                 exdesc->get_keyword(),
                 exdesc->get_wear_slots()->ToString(),
                 GET_CHAR_COVERED_WEARLOCS(vict).ToString(),
                 comparison_field.ToString(),
                 num_already_set
                );

    // Check for something that's being revealed.
    if (check_for_reveal) {
      // It's already revealed, so we won't reveal it with this action.
      if (num_already_set > 0) {
        // send_to_char(vict, "debug: won't show %s, already set is too high at %d\r\n", exdesc->get_keyword(), num_already_set);
        continue;
      }
      
      // Check to see if the wearloc we're revealing is both in the exdesc's set and not already revealed.
      if (exdesc->get_wear_slots()->IsSet(wearloc) && !comparison_field.IsSet(wearloc)) {
        // send_to_char(vict, "debug: found %s\r\n", exdesc->get_keyword());
        result.push_back(exdesc);
      }
    }
    // Checking for something that's being hidden.
    else {
      // It's already hidden.
      if (num_already_set == 0) {
        // send_to_char(vict, "debug: won't show %s, already set is too low at %d\r\n", exdesc->get_keyword(), num_already_set);
        continue;
      }

      // It's revealed, but changing a single wearloc won't change this.
      if (num_already_set > 1) {
        // send_to_char(vict, "debug: won't show %s, already set is too high at %d (v2)\r\n", exdesc->get_keyword(), num_already_set);
        continue;
      }

      // Check to see if the sole set wearloc is the one we're covering.
      if (comparison_field.IsSet(wearloc)) {
        // send_to_char(vict, "debug: found %s\r\n", exdesc->get_keyword());
        result.push_back(exdesc);
      }
    }
  }

  return result;
}

void exdesc_conceal_reveal(struct char_data *vict, int wearloc, bool check_for_reveal) {
  auto vec = exdescs_with_visibility_changed_by_wearloc(vict, wearloc, check_for_reveal);

  if (vec.empty()) {
    return;
  }

  char exdesc_buf[10000] = {0};
  int remaining_items = vec.size();
  for (auto exdesc : vec) {
    remaining_items--;

    if (*exdesc_buf) {
      strlcat(exdesc_buf, ",", sizeof(exdesc_buf));
      if (remaining_items == 0) {
        strlcat(exdesc_buf, " and", sizeof(exdesc_buf));
      }
    } else {
      strlcpy(exdesc_buf, check_for_reveal ? "This reveals" : "This conceals", sizeof(exdesc_buf));
    }

    // todo the multi slot isn't working right, something with more than one still shows/hides when just one is toggled. Also, update instructions for exdesc customization to show that it should be a noun/name instead of a description or sentence. "A tattoo frozen across his back" is fine, "a tattoo is frozen" is wrong.
    snprintf(ENDOF(exdesc_buf), sizeof(exdesc_buf) - strlen(exdesc_buf), " %s^n", decapitalize_a_an(remove_final_punctuation(exdesc->get_name())));
  }
  strlcat(exdesc_buf, ".", sizeof(exdesc_buf));

  act(exdesc_buf, TRUE, vict, 0, 0, TO_CHAR);
  act(exdesc_buf, TRUE, vict, 0, 0, TO_ROOM);
}

bool viewer_can_see_at_least_one_exdesc_on_vict(struct char_data *viewer, struct char_data *victim) {
  for (auto &exdesc : GET_CHAR_EXDESCS(victim)) {
    if (can_see_exdesc(viewer, victim, exdesc))
      return TRUE;
  }

  return FALSE;
}

void list_exdescs(struct char_data *viewer, struct char_data *vict, bool list_is_for_editing) {
  bool printed_header = list_is_for_editing;

  for (auto &exdesc : GET_CHAR_EXDESCS(vict)) {
    bool can_see = can_see_exdesc(viewer, vict, exdesc, TRUE);
    bool can_ever_see = strcmp(exdesc->get_wear_slots()->ToString(), "0");
    if (list_is_for_editing || can_see || access_level(viewer, LVL_BUILDER)) {
      if (!printed_header) {
        if (vict == viewer) {
          send_to_char("You have the following extra descriptions:\r\n", viewer);
        } else {
          act("$N has the following extra descriptions:\r\n", FALSE, viewer, NULL, vict, TO_CHAR);
        }
        printed_header = TRUE;
      }
      if (list_is_for_editing) {
        send_to_char(viewer, "^W%s^n: %s^n%s\r\n", exdesc->get_keyword(), exdesc->get_name(), !can_see ? (can_ever_see ? " (concealed by clothing)" : " ^y(no wearslot set)^n") : "");
      } else {
        send_to_char(viewer, " %s^n (^W%s^n)%s\r\n", exdesc->get_name(), exdesc->get_keyword(), !can_see ? (can_ever_see ? " (concealed by clothing)" : " ^y(no wearslot set)^n") : "");
      }
    }
  }

  if (!printed_header) {
    if (vict == viewer) {
      send_to_char("You have no extra descriptions set.\r\n", viewer);
    } else {
      act("$N has no extra descriptions set.", FALSE, viewer, NULL, vict, TO_CHAR);
    }
  }
}

void send_exdescs_on_look(struct char_data *viewer, struct char_data *vict, const char *used_keyword) {
  if (!CHAR_HAS_EXDESCS(vict))
    return;
  
  if (!viewer_can_see_at_least_one_exdesc_on_vict(viewer, vict))
    return;
  
  char uppercase[strlen(used_keyword) + 1];
  for (size_t idx = 0; idx < strlen(used_keyword); idx++) { uppercase[idx] = toupper(used_keyword[idx]); }
  uppercase[strlen(used_keyword)] = '\0';

  // Too many exdescs to append to list.
  if (GET_CHAR_EXDESCS(vict).size() >= 10) {
    send_to_char(viewer, "%s %s extra descriptions set. Use ^WLOOK %s EXDESCS^n for more.\r\n",
                CAP(HSSH(vict)),
                HASHAVE(vict),
                uppercase);
  } else {
    list_exdescs(viewer, vict, FALSE);
    send_to_char(viewer, "Use ^WLOOK %s <keyword>^n to see more.\r\n", uppercase);
  }
}

// Given a target and a specified keyword, search for that keyword first exactly, then as an abbreviation. Returns desc ptr if found, NULL otherwise. Does NOT do visibility checks.
class PCExDesc *find_exdesc_by_keyword(struct char_data *target, const char *keyword, bool allow_fuzzy_match) {
  // Look for an exact match first.
  for (auto &exdesc : GET_CHAR_EXDESCS(target)) {
    if (!str_cmp(keyword, exdesc->get_keyword())) {
      return exdesc;
    }
  }

  // Failed to find an exact match. Look for a fuzzy match by abbreviation.
  if (allow_fuzzy_match) {
    for (auto &exdesc : GET_CHAR_EXDESCS(target)) {
      if (is_abbrev(keyword, exdesc->get_keyword())) {
        return exdesc;
      }
    }
  }

  // Found nothing.
  return NULL;
}

// Given a target and a specified keyword, search for that keyword exactly. If found, delete it and return TRUE, otherwise FALSE.
bool delete_exdesc_by_keyword(struct char_data *target, const char *keyword) {
  auto it = std::remove_if(GET_CHAR_EXDESCS(target).begin(), GET_CHAR_EXDESCS(target).end(), 
    [keyword](class PCExDesc * exdesc) {
      return !str_cmp(keyword, exdesc->get_keyword());
    }
  );
  auto r = GET_CHAR_EXDESCS(target).end() - it;
  GET_CHAR_EXDESCS(target).erase(it, GET_CHAR_EXDESCS(target).end());
  return r;
}

// Search vict's exdescs for the specified keyword. If found, display it and return TRUE. Return FALSE otherwise.
bool display_exdesc(struct char_data *viewer, struct char_data *vict, const char *keyword) {
  class PCExDesc *exdesc;

  if ((exdesc = find_exdesc_by_keyword(vict, keyword)) && can_see_exdesc(viewer, vict, exdesc)) {
    send_to_char(viewer, "%s^n\r\n", exdesc->get_desc());
    return TRUE;
  }

  return FALSE;
}

// #define EXDESCS_DEBUG(...) {                          \
//   if (viewer && IS_SENATOR(viewer)) {                 \
//     send_to_char(viewer, __VA_ARGS__);                \
//     send_to_char(viewer, "\r\n"); /*force a newline*/ \
//   }                                                   \
// }

#define EXDESCS_DEBUG(...)

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
 * Code for exdesc editing.
 */
#define CH d->character

void pc_exdesc_edit_disp_main_menu(struct descriptor_data *d) {
  CLS(CH);

  // If they have no exdescs, tell them so, then offer to create or quit/qws.
  if (GET_CHAR_EXDESCS(d->edit_mob).empty()) {
    send_to_char(CH, "You have no extra descriptions set at the moment, but have ^c%d^n slots available.\r\n", GET_CHAR_MAX_EXDESCS(CH));
  } else {
    list_exdescs(CH, d->edit_mob, TRUE);
  }

  if (GET_CHAR_EXDESCS(d->edit_mob).size() < (unsigned long) GET_CHAR_MAX_EXDESCS(CH)) {
    send_to_char(CH, "\r\n^cC^n) Create a new exdesc\r\n");
  }
  
  if (!GET_CHAR_EXDESCS(d->edit_mob).empty()) {
    send_to_char(CH, "^cE^n) Edit an exdesc\r\n");
    send_to_char(CH, "^cD^n) Delete an exdesc\r\n");
  }

  send_to_char(CH, "^cQ^n) Return to Customize Menu\r\n");

  d->edit_mode = PC_EXDESC_EDIT_MAIN_MENU;
}

void _pc_exdesc_edit_olc_menu(struct descriptor_data *d) {
  CLS(CH);
  
  // Render the exdesc's wearslots.
  char exdesc_wearslot_string[10000] = { '\0' };
  if (!strcmp(d->edit_exdesc->get_wear_slots()->ToString(), "0")) {
    strlcpy(exdesc_wearslot_string, "^y<nothing set, exdesc is always hidden>^n", sizeof(exdesc_wearslot_string));
  } else {
    d->edit_exdesc->get_wear_slots()->PrintBits(exdesc_wearslot_string, sizeof(exdesc_wearslot_string), wear_bits, NUM_WEARS);
  }

  // Print the menu.
  send_to_char(CH, "1) Keyword: ^W%s^n\r\n", d->edit_exdesc->get_keyword() ? d->edit_exdesc->get_keyword() : "<not set>");
  send_to_char(CH, "2) Title:   %s^n\r\n", d->edit_exdesc->get_name() ? d->edit_exdesc->get_name() : "<not set>");
  send_to_char(CH, "3) Desc:%s%s^n\r\n", 
                   d->edit_exdesc->get_desc() ? "\r\n" : "    <not set>",
                   d->edit_exdesc->get_desc() ? d->edit_exdesc->get_desc() : "");
  send_to_char(CH, "4) Visible if any of these are uncovered: ^c%s\r\n", exdesc_wearslot_string);
  send_to_char(CH, "\r\n");
  send_to_char(CH, "q) Save and quit\r\n");
  send_to_char(CH, "x) Exit without saving\r\n");

  d->edit_mode = PC_EXDESC_EDIT_OLC_MENU;
}

void pc_exdesc_edit_disp_edit_menu(struct descriptor_data *d) {
  CLS(CH);
  
  // List exdescs with an index, ask which one they want to edit.
  list_exdescs(CH, d->edit_mob, TRUE);

  send_to_char("\r\nEnter the keyword you'd like to edit, or 'q' to abort: ", CH);
  d->edit_mode = PC_EXDESC_EDIT_EDIT_MENU;
  d->edit_number2 = PC_EXDESC_EDIT_OLC_FROM_EDIT;
}

void pc_exdesc_edit_disp_delete_menu(struct descriptor_data *d) {
  CLS(CH);
  
  // List exdescs with an index, ask which one they want to delete.
  list_exdescs(CH, d->edit_mob, TRUE);

  send_to_char("\r\nEnter the keyword you'd like to delete, or 'q' to abort: ", CH);
  d->edit_mode = PC_EXDESC_EDIT_DELETE_MENU;
}

#define EXDESC_EDIT_SYNTAX_WITH_EXISTING "That's not a choice. Please select from ^CC^create^n, ^CE^cdit^n, ^CD^celete^n, and ^CQ^cuit and save^n.\r\n"
#define EXDESC_EDIT_SYNTAX_NO_EXISTING "That's not a choice. Please select from ^CC^create^n or ^CQ^cuit^n.\r\n"
void pc_exdesc_edit_parse_main_menu(struct descriptor_data *d, const char *arg) {
  switch (*arg) {
    case 'c':
    case 'C':
      if (GET_CHAR_EXDESCS(d->edit_mob).size() < (unsigned long) GET_CHAR_MAX_EXDESCS(CH)) {
        send_to_char("OK, here's your new exdesc to edit:\r\n", CH);
        d->edit_exdesc = new PCExDesc(GET_IDNUM(CH));
        d->edit_number2 = PC_EXDESC_EDIT_OLC_FROM_CREATE;
        _pc_exdesc_edit_olc_menu(d);
      } else {
        // TODO STRETCH: Offer to auto-buy more exdescs with syspoints.
        send_to_char(CH, "You can only have %d exdescs. Please edit or delete an existing one.\r\n", GET_CHAR_MAX_EXDESCS(CH));
      }
      break;
    case 'e':
    case 'E':
      if (GET_CHAR_EXDESCS(d->edit_mob).empty()) {
        send_to_char(EXDESC_EDIT_SYNTAX_NO_EXISTING, CH);
        return;
      }
      pc_exdesc_edit_disp_edit_menu(d);
      break;
    case 'd':
    case 'D':
      if (GET_CHAR_EXDESCS(d->edit_mob).empty()) {
        send_to_char(EXDESC_EDIT_SYNTAX_NO_EXISTING, CH);
        return;
      }
      pc_exdesc_edit_disp_delete_menu(d);
      break;
    case 'q':
    case 'Q':
      STATE(d) = CON_FCUSTOMIZE;
      cedit_disp_menu(d, 0);
      break;
    default:
      if (GET_CHAR_EXDESCS(d->edit_mob).empty()) {
        send_to_char(EXDESC_EDIT_SYNTAX_NO_EXISTING, CH);
      } else {
        send_to_char(EXDESC_EDIT_SYNTAX_WITH_EXISTING, CH);
      }
      return;
  }
}

void _pc_exdesc_edit_wear_menu(struct descriptor_data *d) {
  int counter = 1;

  CLS(CH);
  send_to_char("Extra descriptions can be put on the following locations:\r\n", CH);

  for (auto itr : wear_flag_map_for_exdescs) {
    send_to_char(CH, "%2d) ^c%-9s^n\r\n", counter++, itr.first.c_str());
  }

  char exdesc_wear_bits[1000];
  d->edit_exdesc->get_wear_slots()->PrintBits(exdesc_wear_bits, sizeof(exdesc_wear_bits), wear_bits, ITEM_WEAR_MAX);
  send_to_char(CH, "This desc is visible on: ^c%s^n\r\nSelect a wear location to toggle, or enter 'q' to quit:", exdesc_wear_bits);

  d->edit_mode = PC_EXDESC_EDIT_OLC_WEAR_MENU;
}

void pc_exdesc_edit_parse_olc_menu(struct descriptor_data *d, const char *arg) {
  switch (*arg) {
    case '1':  // Edit keyword.
      send_to_char("Enter a new keyword for this exdesc: ", CH);
      d->edit_mode = PC_EXDESC_EDIT_OLC_SET_KEYWORD;
      break;
    case '2':  // Edit name.
      send_to_char("Good exdesc names are full sentences with capitalization and punctuation, like 'An inky dragon tattoo is frozen in a roar across his back.'\r\nEnter a new name for this exdesc: ", CH);
      d->edit_mode = PC_EXDESC_EDIT_OLC_SET_NAME;
      break;
    case '3':  // Edit desc.
      d->edit_mode = PC_EXDESC_EDIT_OLC_SET_DESC;
      DELETE_D_STR_IF_EXTANT(d);
      INITIALIZE_NEW_D_STR(d);
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      send_to_char("Enter a new description for this exdesc:\r\n", CH);
      break;
    case '4':  // Edit wearslots.
      _pc_exdesc_edit_wear_menu(d);
      break;
    case 'q':
    case 'Q': // Save and quit.
      {
        // If edit_number2 is FROM_EDIT, overwrite existing in our edit vector.
        if (d->edit_number2 == PC_EXDESC_EDIT_OLC_FROM_EDIT) {
          d->edit_exdesc->overwrite_editing_clone();
          delete d->edit_exdesc;
        }
        // If edit_number2 is FROM_CREATE, append this entry to our edit vector.
        if (d->edit_number2 == PC_EXDESC_EDIT_OLC_FROM_CREATE) {
          GET_CHAR_EXDESCS(d->edit_mob).push_back(d->edit_exdesc);
        }
        
        d->edit_exdesc = NULL;
        pc_exdesc_edit_disp_main_menu(d);
      }
      break;
    case 'x':
    case 'X': // Exit without saving.
      {
        send_to_char(CH, "OK, discarding changes.\r\n");
        delete d->edit_exdesc;
        d->edit_exdesc = NULL;
        pc_exdesc_edit_disp_main_menu(d);
      }
      break;
    default:
      send_to_char("That's not an option. Select from one of the items on the menu (e.g. ^W1^n for keyword, ^Wq^n to save and quit): ", CH);
      break;
  }
}

void pc_exdesc_edit_parse(struct descriptor_data *d, const char *arg) {
  PCExDesc *exdesc = NULL;
  int number;

  switch (d->edit_mode) {
    case PC_EXDESC_EDIT_MAIN_MENU:
      pc_exdesc_edit_parse_main_menu(d, arg);
      break;
    case PC_EXDESC_EDIT_DELETE_MENU:
      {
        if (!str_cmp(arg, "q")) {
          pc_exdesc_edit_disp_main_menu(d);
          return;
        }

        if (delete_exdesc_by_keyword(d->edit_mob, arg)) {
          send_to_char(CH, "OK.\r\n");
          pc_exdesc_edit_disp_delete_menu(d);
        } else {
          send_to_char(CH, "The keyword '%s' doesn't match any of your existing exdescs. Please type the full keyword you want to delete, or 'q' to abort.\r\n", arg);
        }
      }
      break;
    case PC_EXDESC_EDIT_EDIT_MENU:
      {
        if (!str_cmp(arg, "q")) {
          pc_exdesc_edit_disp_main_menu(d);
          return;
        }

        // Find the keyword they've entered. Don't accept abbreviations. If it doesn't exist, re-show the menu and exit.
        if (!(exdesc = find_exdesc_by_keyword(d->edit_mob, arg, FALSE))) {
          send_to_char(CH, "The keyword '%s' doesn't match any of your existing exdescs. Please type the full keyword you want to edit, or 'q' to abort.\r\n", arg);
        } else {
          d->edit_exdesc = new PCExDesc(exdesc);
          _pc_exdesc_edit_olc_menu(d);
        }
      }
      break;
    case PC_EXDESC_EDIT_OLC_MENU:
      pc_exdesc_edit_parse_olc_menu(d, arg);
      break;
    case PC_EXDESC_EDIT_OLC_WEAR_MENU:
      {
        number = 0;

        int counter = 1;
        int entry = atoi(arg);

        // They entered 0 or a non-number, go back to main OLC menu.
        if (entry == 0) {
          _pc_exdesc_edit_olc_menu(d);
          return;
        }

        for (auto itr : wear_flag_map_for_exdescs) {
          if (entry == counter++) {
            number = itr.second;
            break;
          }
        }

        if ((number < 0) || (number > ITEM_WEAR_MAX)) {
          send_to_char("That's not a valid choice. Select an index number from the menu, or enter ^Wq^n to quit this menu.\r\n", d->character);
        }

        d->edit_exdesc->get_wear_slots()->ToggleBit(number);
        _pc_exdesc_edit_wear_menu(d);
      }
      break;
    case PC_EXDESC_EDIT_OLC_SET_KEYWORD:
      {
        if (!str_cmp(arg, "q")) {
          send_to_char("OK, leaving it unchanged.\r\n", CH);
          _pc_exdesc_edit_olc_menu(d);
          return;
        }

        if (strlen(arg) > MAX_EXDESC_KEYWORD_LENGTH) {
          send_to_char(CH, "Sorry, keywords must be %ld characters or shorter, and that was %ld. Try again: ", MAX_EXDESC_KEYWORD_LENGTH, strlen(arg));
          return;
        }

        for (size_t idx = 0; idx < strlen(arg); idx++) {
          if (arg[idx] && !isalpha(arg[idx]) && arg[idx] != '_') {
            send_to_char(CH, "Sorry, keywords can only contain letters and underscores ('%c' is invalid). Try again: ", arg[idx]);
            return;
          }
        }

        if (check_for_banned_content(arg, CH, MODERATION_MODE_DESCRIPTIONS)) {
          _pc_exdesc_edit_olc_menu(d);
          return;
        }

        // If the keyword already exists in the vector, disallow. Exception: If it's case-insensitively the same as what it already is, don't check for this.
        if (str_cmp(d->edit_exdesc->get_keyword(), arg) && find_exdesc_by_keyword(d->edit_mob, arg, FALSE)) {
          send_to_char(CH, "You already have an exdesc with the keyword '%s', so you'll need to pick another.\r\n", arg);
          _pc_exdesc_edit_olc_menu(d);
          return;
        }

        // Set it.
        d->edit_exdesc->set_keyword(arg);
        _pc_exdesc_edit_olc_menu(d);
      }
      break;
    case PC_EXDESC_EDIT_OLC_SET_NAME:
      {
        if (strlen(arg) > MAX_EXDESC_NAME_LENGTH) {
          send_to_char(CH, "Sorry, names must be %ld characters or shorter, and that was %ld. Try again: ", MAX_EXDESC_NAME_LENGTH, strlen(arg));
          return;
        }

        if (!str_cmp(arg, "q")) {
          send_to_char("OK, leaving it unchanged.\r\n", CH);
          _pc_exdesc_edit_olc_menu(d);
          return;
        }

        if (!check_for_banned_content(arg, CH, MODERATION_MODE_DESCRIPTIONS)) {
          d->edit_exdesc->set_name(arg);
        }
        _pc_exdesc_edit_olc_menu(d);
      }
      break;
    case PC_EXDESC_EDIT_OLC_SET_DESC:
      // You should never get here. This is handled in modify.cpp.
      send_to_char(CH, "error: got to set_desc in parse");
      break;
  }
}

#undef CH

/*
 * Code for handling database-related things.
 */

// Saving happens here. Requires pc_idnum and keyword to be set. Invoke during editing.
void PCExDesc::save_to_db() {
  char query_buf[MAX_STRING_LENGTH];
  char keyword_buf[MAX_STRING_LENGTH];
  char name_buf[MAX_STRING_LENGTH];
  char desc_buf[MAX_STRING_LENGTH];

  snprintf(query_buf, sizeof(query_buf),
           "INSERT INTO pfiles_exdescs (`idnum`, `keyword`, `name`, `desc`, `wearslots`)" /* continues to next line*/
           "\n VALUES (%ld, '%s', '%s', '%s', '%s');",
           pc_idnum,
           prepare_quotes(keyword_buf, keyword, sizeof(keyword_buf)),
           prepare_quotes(name_buf, name, sizeof(name_buf)),
           prepare_quotes(desc_buf, desc, sizeof(desc_buf)),
           wear_slots.ToString());

  mysql_wrapper(mysql, query_buf);
}

// Call this to delete this entry. Requires pc_idnum and keyword to be set. Invoke during editing when char chooses to delete.
void PCExDesc::delete_from_db() {
  char query_buf[MAX_STRING_LENGTH];
  char keyword_buf[MAX_STRING_LENGTH];

  snprintf(query_buf, sizeof(query_buf),
           "DELETE FROM pfiles_exdescs WHERE `idnum`=%ld AND `keyword`='%s';",
           pc_idnum,
           prepare_quotes(keyword_buf, keyword, sizeof(keyword_buf)));

  mysql_wrapper(mysql, query_buf);
}

void load_exdescs_from_db(struct char_data *ch) {
  char query_buf[MAX_STRING_LENGTH];

  snprintf(query_buf, sizeof(query_buf), "SELECT * FROM pfiles_exdescs WHERE `idnum`=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, query_buf);

  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row;

  while ((row = mysql_fetch_row(res))) {
    PCExDesc *desc = new PCExDesc(
      GET_IDNUM(ch),
      row[1],
      row[2],
      row[3],
      row[4]
    );

    GET_CHAR_EXDESCS(ch).push_back(desc);
  }

  mysql_free_result(res);
}

void delete_all_exdescs_from_db(struct char_data *ch) {
  char query_buf[1000];
  snprintf(query_buf, sizeof(query_buf), "DELETE FROM pfiles_exdescs WHERE `idnum`=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, query_buf);
}

void write_all_exdescs_to_db(struct char_data *ch) {
  for (auto &exdesc : GET_CHAR_EXDESCS(ch)) {
    exdesc->save_to_db();
  }
}

void save_pc_exdesc_max(struct char_data *ch) {
  char query_buf[1000];
  snprintf(query_buf, sizeof(query_buf), "UPDATE pfiles SET `exdesc_max`=%d WHERE `idnum`=%ld;", GET_CHAR_MAX_EXDESCS(ch), GET_IDNUM(ch));
  mysql_wrapper(mysql, query_buf);
}

void clone_exdesc_vector_to_edit_mob_for_editing(struct descriptor_data *d) {
  for (auto &exdesc : GET_CHAR_EXDESCS(d->original ? d->original : d->character)) {
    GET_CHAR_EXDESCS(d->edit_mob).push_back(new PCExDesc(exdesc));
  }
  assert(GET_CHAR_EXDESCS(d->edit_mob).size() == GET_CHAR_EXDESCS(d->original ? d->original : d->character).size());
}

void overwrite_pc_exdescs_with_edit_mob_exdescs_and_then_save_to_db(struct descriptor_data *d) {
  struct char_data *ch = d->original ? d->original : d->character;

  if (!ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Called overwrite_pc_exdescs on descriptor with no character");
    return;
  }

  for (auto it = GET_CHAR_EXDESCS(ch).begin(); it != GET_CHAR_EXDESCS(ch).end(); ++it) {
    delete *it;
  }
  GET_CHAR_EXDESCS(ch).clear();

  for (auto &exdesc : GET_CHAR_EXDESCS(d->edit_mob)) {
    GET_CHAR_EXDESCS(ch).push_back(new PCExDesc(exdesc));
  }

  // Clear out PC exdescs in DB and save the new ones.
  delete_all_exdescs_from_db(ch);
  write_all_exdescs_to_db(ch);
}

/*
 * Code for implementing methods from the class.
 */

void PCExDesc::overwrite_editing_clone() {
  if (!editing_clone_of) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: overwite_editing_clone() called on PCExDesc with no editing clone!");
    return;
  }

  editing_clone_of->set_keyword(keyword);
  editing_clone_of->set_name(name);
  editing_clone_of->set_desc(desc);
  editing_clone_of->set_wear_slots(wear_slots.ToString());
}
