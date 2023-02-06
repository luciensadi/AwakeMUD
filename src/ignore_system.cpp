#include <mysql/mysql.h>

#include "interpreter.hpp"
#include "newdb.hpp"
#include "comm.hpp"
#include "db.hpp" // for descriptor_list
#include "ignore_system.hpp"

extern void stop_follower(struct char_data *ch);
extern void end_sustained_spell(struct char_data *ch, struct sustain_data *sust);

// Prototypes for this file.
void send_ignore_bits_to_character(struct char_data *ch, Bitfield *already_set);
void send_ignore_usage_to_character(struct char_data *ch);
void display_characters_ignore_entries(struct char_data *viewer, struct char_data *target);

// TODO: test if ignore works for projections etc

typedef bool (IgnoreData::*method_function)(long, const char *, int);
method_function ignore_function_sorted_by_bit[NUM_IGNORE_BITS] {
  &IgnoreData::toggle_blocking_where_visibility,
  &IgnoreData::toggle_blocking_oocs,
  &IgnoreData::toggle_blocking_radios,
  &IgnoreData::toggle_blocking_mindlinks,
  &IgnoreData::toggle_blocking_tells,
  &IgnoreData::toggle_blocking_osays,
  &IgnoreData::toggle_blocking_ic_interaction,
  &IgnoreData::toggle_blocking_following,
  &IgnoreData::toggle_blocking_calls
};

ACMD(do_ignore) {
  /* if (GET_LEVEL(tch) > LVL_MORTAL)
    send_to_char("You can't ignore staff members.\r\n", ch); */
  skip_spaces(&argument);
  char ignored_bits_str[5000];

  // ignore with no arguments: display all currently-ignored characters
  if (!*argument) {
    display_characters_ignore_entries(ch, ch);
    send_ignore_bits_to_character(ch, NULL);
    send_to_char("\r\nSee ^WHELP IGNORE^n for more details.\r\n", ch);
    return;
  }

  // An argument has been supplied; split it up into its constituent parts.
  char first_argument[MAX_INPUT_LENGTH], second_argument[MAX_INPUT_LENGTH];
  char *remainder = any_one_arg(argument, first_argument);
  remainder = any_one_arg(remainder, second_argument);

  // We require at least one argument.
  if (!*first_argument) {
    send_ignore_usage_to_character(ch);
    return;
  }

  // Extract the idnum from the first argument and verify that it exists.
  idnum_t vict_idnum = get_player_id(first_argument);

  if (vict_idnum <= 0) {
    // TODO: Check people in the room and see if we can find a player that matches their description.

    send_to_char(ch, "Couldn't find any player named '%s'.\r\n", first_argument);
    return;
  }

  // You're not allowed to use this command on staff.
  if (get_player_rank(vict_idnum) > GET_LEVEL(ch)) {
    send_to_char(ch, "You can't do that to staff%s.\r\n", GET_LEVEL(ch) > 1 ? " of a higher rank than yourself.\r\n" : "");
    return;
  }

  if (vict_idnum == GET_IDNUM(ch)) {
    send_to_char("You can't ignore yourself. You'll have to live with the demons in your head a while longer.\r\n", ch);
    return;
  }

  if (!GET_IGNORE_DATA(ch)) {
    mudlog("SYSERR: Using the ignore command without ignore data! Self-recovering.", ch, LOG_SYSLOG, TRUE);
    GET_IGNORE_DATA(ch) = new IgnoreData(ch);
  }

  // Without a second argument, we display the ignores for the selected character.
  if (!*second_argument) {
    std::unordered_map<long, Bitfield>::iterator results_iterator;

    if ((results_iterator = GET_IGNORE_DATA(ch)->ignored_map.find(vict_idnum)) != GET_IGNORE_DATA(ch)->ignored_map.end()) {
      results_iterator->second.PrintBitsColorized(ignored_bits_str, sizeof(ignored_bits_str), ignored_bits_in_english, NUM_IGNORE_BITS, "^c", "^n");
      send_to_char(ch, "You've set the following ignore flags on %s: %s\r\n", capitalize(first_argument), ignored_bits_str);
      send_ignore_bits_to_character(ch, &(results_iterator->second));
    } else {
      send_to_char(ch, "You're not currently ignoring %s.\r\n", capitalize(first_argument));
      send_ignore_bits_to_character(ch, NULL);
    }


    return;
  }

  // We have both a first and second argument, so treat this as 'ignore <name> <mode>.' We do this in a loop so you can specify many bits at once.
  // Special case: '*' or 'all' applies all blocks.
  if (*second_argument == '*' || !str_cmp(second_argument, "all")) {
    send_to_char(ch, "Setting all ignore bits for %s.\r\n", capitalize(first_argument));
    for (int bit_idx = 0; bit_idx < NUM_IGNORE_BITS; bit_idx++) {
      // If not all bits are set, set all bits.
      if (!(GET_IGNORE_DATA(ch)->_ignore_bit_is_set_for(bit_idx, vict_idnum))) {
        method_function func = ignore_function_sorted_by_bit[bit_idx];
        (GET_IGNORE_DATA(ch)->*func) (vict_idnum, capitalize(first_argument), MODE_NOT_SILENT);
      }
    }
    send_to_char("All ignore bits set.\r\n", ch);
    return;
  }

  char blocks_applied[MAX_STRING_LENGTH];
  snprintf(blocks_applied, sizeof(blocks_applied), "%s modified block bits for %s: ", GET_CHAR_NAME(ch), capitalize(first_argument));
  bool wrote_applied_block = FALSE;

  while (*second_argument) {
    // Parse out the selected bit.
    int chosen_bit;
    for (chosen_bit = 0; chosen_bit < NUM_IGNORE_BITS; chosen_bit++) {
      if (is_abbrev(second_argument, ignored_bits_in_english[chosen_bit]))
        break;
    }
    if (chosen_bit >= NUM_IGNORE_BITS) {
      send_to_char(ch, "Sorry, I don't recognize the ignore bit '%s'.\r\n", second_argument);
    } else {
      // Finally, toggle the selected bit.
      method_function func = ignore_function_sorted_by_bit[chosen_bit];
      bool result = (GET_IGNORE_DATA(ch)->*func) (vict_idnum, capitalize(first_argument), MODE_NOT_SILENT);

      snprintf(ENDOF(blocks_applied), sizeof(blocks_applied) - strlen(blocks_applied), "%s%s (%s)", wrote_applied_block ? ", " : "", ignored_bits_in_english[chosen_bit], result ? "^YON^g" : "OFF");
      wrote_applied_block = TRUE;
    }

    // Did you set a remainder? Great, we'll cycle through that as well.
    remainder = any_one_arg(remainder, second_argument);
  }

  if (wrote_applied_block) {
    mudlog(blocks_applied, ch, LOG_IGNORELOG, TRUE);
  }
}

////////////// Implementation of primary manipulation functions. ///////////////
#define SEND_TO_CH_IF_NOT_SILENT(ch, message, ...)   if (mode != MODE_SILENT) { send_to_char((ch), message, ##__VA_ARGS__); }

bool IgnoreData::toggle_blocking_oocs(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_oocs_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now see OOC channel messages from %s.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_OOC_CHANNELS, vict_idnum);
    return FALSE;
  } else {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will no longer see OOC channel messages (ex: ooc, newbie, etc) from %s.", vict_name);
    if (!is_blocking_osays_from(vict_idnum)) {
      SEND_TO_CH_IF_NOT_SILENT(ch, " If you also want to block perception of their OSAYs, use 'ignore %s osays'.", vict_name);
    }
    SEND_TO_CH_IF_NOT_SILENT(ch, "\r\n");

    _set_ignore_bit_for(IGNORE_BIT_OOC_CHANNELS, vict_idnum);
    return TRUE;
  }
}

// Don't display messages from the targeted character on your radio output.
bool IgnoreData::toggle_blocking_radios(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_radios_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now see radio messages from %s.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_RADIO, vict_idnum);
    return FALSE;
  } else {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will no longer see radio messages from %s.\r\n", vict_name);
    _set_ignore_bit_for(IGNORE_BIT_RADIO, vict_idnum);
    return TRUE;
  }
}

// Prevent the target from sending you mindlinks, and breaks the mindlink if it's set.
bool IgnoreData::toggle_blocking_mindlinks(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_mindlinks_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now accept mindlinks from %s.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_MINDLINK, vict_idnum);
    return FALSE;
  } else {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will no longer accept mindlinks from %s.\r\n", vict_name);
    _set_ignore_bit_for(IGNORE_BIT_MINDLINK, vict_idnum);

    // Iterate through existing mindlinks and break any that were cast by the ignored person.
    struct sustain_data *spell = NULL, *next_spell = NULL;
    for (spell = GET_SUSTAINED(ch); spell; spell = next_spell) {
      next_spell = spell->next;
      if (spell->spell == SPELL_MINDLINK && !spell->caster && spell->other && GET_IDNUM_EVEN_IF_PROJECTING(spell->other) == vict_idnum) {
        end_sustained_spell(ch, spell);
      }
    }

    return TRUE;
  }
}

// Prevent the target from sending you tells.
bool IgnoreData::toggle_blocking_tells(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_tells_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now see OOC tells from %s.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_TELLS, vict_idnum);
    return FALSE;
  } else {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will no longer see OOC tells from %s.\r\n", vict_name);
    _set_ignore_bit_for(IGNORE_BIT_TELLS, vict_idnum);
    return TRUE;
  }
}

// Don't display the target's osays.
bool IgnoreData::toggle_blocking_osays(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_osays_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now see osays from %s.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_OSAYS, vict_idnum);
    return FALSE;
  } else {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will no longer see osays from %s.", vict_name);
    if (!is_blocking_osays_from(vict_idnum)) {
      SEND_TO_CH_IF_NOT_SILENT(ch, " If you also want to block their OOC channel messages, use 'ignore %s oocs'.", vict_name);
    }
    SEND_TO_CH_IF_NOT_SILENT(ch, "\r\n");

    _set_ignore_bit_for(IGNORE_BIT_OSAYS, vict_idnum);
    return TRUE;
  }
}

// Prevent the target from interacting with you ICly (forces on several other toggles). Includes socials, emotes/echoes, vemotes, says.
#define SEND_WITH_PRECURSOR(message, ch)  if (!printed_initial_message) { send_to_char(ch, "To ensure this change is effective, we also enabled the following ignore modes for %s:\r\n", vict_name); printed_initial_message = TRUE;} send_to_char(message, ch);
bool IgnoreData::toggle_blocking_ic_interaction(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_ic_interaction_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now see the character %s in the game world.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_IC_INTERACTION, vict_idnum);
    return FALSE;
  } else {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will no longer see %s in the game world.\r\n", vict_name);
    _set_ignore_bit_for(IGNORE_BIT_IC_INTERACTION, vict_idnum);

    bool printed_initial_message = FALSE;

    if (!is_blocking_following_from(vict_idnum)) {
      toggle_blocking_following(vict_idnum, vict_name, MODE_SILENT);
      SEND_WITH_PRECURSOR(" - Following\r\n", ch);
    }

    if (!is_blocking_mindlinks_from(vict_idnum)) {
      toggle_blocking_mindlinks(vict_idnum, vict_name, MODE_SILENT);
      SEND_WITH_PRECURSOR(" - Mindlinks\r\n", ch);
    }

    if (!is_blocking_radios_from(vict_idnum)) {
      toggle_blocking_radios(vict_idnum, vict_name, MODE_SILENT);
      SEND_WITH_PRECURSOR(" - Radios\r\n", ch);
    }

    if (!is_blocking_calls_from(vict_idnum)) {
      toggle_blocking_calls(vict_idnum, vict_name, MODE_SILENT);
      SEND_WITH_PRECURSOR(" - Calls\r\n", ch);
    }
    return TRUE;
  }
}
#undef SEND_WITH_PRECURSOR

// Prevent the target from following you, and unfollow them if they are. (Abusable in PvP)
bool IgnoreData::toggle_blocking_following(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_following_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You now allow %s to follow you.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_FOLLOWING, vict_idnum);
    return FALSE;
  } else {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will no longer allow %s to follow you.\r\n", vict_name);
    _set_ignore_bit_for(IGNORE_BIT_FOLLOWING, vict_idnum);

    // Unfollow them if they're following.
    struct follow_type *follower = NULL, *next_follower = NULL;
    for (follower = ch->followers; follower; follower = next_follower) {
      next_follower = follower->next;
      if (GET_IDNUM_EVEN_IF_PROJECTING(follower->follower) == vict_idnum)
        stop_follower(follower->follower);
    }

    return TRUE;
  }
}

// Used to block the target from seeing you with the WHERE command.
bool IgnoreData::toggle_blocking_where_visibility(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_where_visibility_for(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now allow %s to perceive you on the WHERE list.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_WHERE, vict_idnum);
    return FALSE;
  } else {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You no longer allow %s to perceive you on the WHERE list.\r\n", vict_name);
    _set_ignore_bit_for(IGNORE_BIT_WHERE, vict_idnum);
    return TRUE;
  }
}

// Block calls from the targeted person.
bool IgnoreData::toggle_blocking_calls(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_calls_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now allow %s to call you.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_CALLS, vict_idnum);
    return FALSE;
  } else {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You no longer allow %s to call you.\r\n", vict_name);
    _set_ignore_bit_for(IGNORE_BIT_CALLS, vict_idnum);
    return TRUE;
  }
}

//////////////// Implementation of private helper functions. ///////////////////
bool IgnoreData::_ignore_bit_is_set_for(int ignore_bit, long vict_idnum) {
  std::unordered_map<long, Bitfield>::iterator results_iterator;

  // If they've been ignored, return the results of the IsSet operation on their bitfield.
  if ((results_iterator = ignored_map.find(vict_idnum)) != ignored_map.end()) {
    return results_iterator->second.IsSet(ignore_bit);
  }

  // They haven't been ignored at all, so just return FALSE.
  return FALSE;
}

void IgnoreData::_set_ignore_bit_for(int ignore_bit, long vict_idnum) {
  std::unordered_map<long, Bitfield>::iterator results_iterator;

  // If they've previously been ignored, set their ignore bit.
  if ((results_iterator = ignored_map.find(vict_idnum)) != ignored_map.end()) {
    results_iterator->second.SetBit(ignore_bit);

    // Save their entry to the DB (found-entry form.)
    _save_entry_to_db(vict_idnum, results_iterator->second);
  }

  // Otherwise, write a new entry for them, then set their ignore bit.
  else {
    Bitfield new_field;
    new_field.SetBit(ignore_bit);

    ignored_map.emplace(vict_idnum, new_field);

    // Save their entry to the DB (new-bitfield form).
    _save_entry_to_db(vict_idnum, new_field);
  }
}

void IgnoreData::_remove_ignore_bit_for(int ignore_bit, long vict_idnum) {
  std::unordered_map<long, Bitfield>::iterator results_iterator;

  // If they already existed in our ignore list:
  if ((results_iterator = ignored_map.find(vict_idnum)) != ignored_map.end()) {
    results_iterator->second.RemoveBit(ignore_bit);

    // If this was the last bit set on them, remove their entry entirely.
    if (results_iterator->second.GetNumSet() == 0) {
      ignored_map.erase(results_iterator->first);

      // Delete their entry from the DB.
      _delete_entry_from_db(vict_idnum);
    } else {
      // Otherwise, save their entry.
      _save_entry_to_db(vict_idnum, results_iterator->second);
    }
  }

  // They didn't exist already? That's an error case.
  else {
    mudlog("SYSERR: Received a non-ignored vict to IgnoreData._remove_ignore_bit_for()!", NULL, LOG_SYSLOG, TRUE);
  }
}

void IgnoreData::_save_entry_to_db(long vict_idnum, Bitfield bitfield) {
  char query_buf[5000];
  snprintf(query_buf, sizeof(query_buf), "INSERT INTO pfiles_ignore_v2 (`idnum`, `vict_idnum`, `bitfield_str`) VALUES ('%ld', '%ld', '%s')"
               " ON DUPLICATE KEY UPDATE"
               "   `bitfield_str` = VALUES(`bitfield_str`)",
               GET_IDNUM_EVEN_IF_PROJECTING(ch),
               vict_idnum,
               bitfield.ToString());
  mysql_wrapper(mysql, query_buf);
}

void IgnoreData::_delete_entry_from_db(long vict_idnum) {
  char query_buf[5000];
  snprintf(query_buf, sizeof(query_buf), "DELETE FROM pfiles_ignore_v2 WHERE idnum=%ld AND vict_idnum=%ld", GET_IDNUM_EVEN_IF_PROJECTING(ch), vict_idnum);
  mysql_wrapper(mysql, query_buf);
}

void IgnoreData::load_from_db() {
  // Blow away our current map, just in case.
  if (!ignored_map.empty()) {
    mudlog("SYSERR: load_from_db() called on IgnoreData that had elements! Wiping them out and starting fresh.", ch, LOG_SYSLOG, TRUE);
    ignored_map.clear();
  }

  char query_buf[5000];
  snprintf(query_buf, sizeof(query_buf), "SELECT * FROM pfiles_ignore_v2 WHERE idnum=%ld", GET_IDNUM_EVEN_IF_PROJECTING(ch));
  mysql_wrapper(mysql, query_buf);

  MYSQL_RES *res = mysql_use_result(mysql);
  if (!res) {
    // They had no entries.
    return;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res))) {
    // Read out our bitfield and construct it.
    Bitfield *bitfield_ptr = new Bitfield;
    bitfield_ptr->FromString(row[2]);

    // Insert it into the map.
    ignored_map.emplace(atol(row[1]), *bitfield_ptr);
  }

  // Finally, clean up.
  mysql_free_result(res);
}

//////////////////////////// Non-class functions. //////////////////////////////

// This is called from DeleteChar, so we don't need to bother with cleaning up the DB etc.
void globally_remove_vict_id_from_logged_in_ignore_lists(long vict_idnum) {
  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    struct char_data *ch = d->original ? d->original : d->character;
    if (ch && GET_IGNORE_DATA(ch))
      GET_IGNORE_DATA(ch)->ignored_map.erase(vict_idnum);
  }
}

void send_ignore_bits_to_character(struct char_data *ch, Bitfield *already_set) {
  send_to_char(ch, "\r\nYou can set the following %signore bits: ", already_set ? "additional " : "");

  bool found_something = FALSE;
  for (int i = 0; i < NUM_IGNORE_BITS; i++) {
    if (!already_set || !already_set->IsSet(i)) {
      send_to_char(ch, "%s^c%s^n", !found_something ? "" : ", ", ignored_bits_in_english[i]);
      found_something = TRUE;
    }
  }

  if (!found_something)
    send_to_char("...none!\r\n", ch);
  else
    send_to_char("\r\n", ch);
}

void send_ignore_usage_to_character(struct char_data *ch) {
  send_to_char("Syntax: ^Wignore <target character> [ignore bit]^n.\r\n", ch);
  send_ignore_bits_to_character(ch, NULL);
  send_to_char("\r\nYou may also view your current ignored list with ^Wignore^n on its own, or view the ignores set on a specific character with ^Wignore <character>^n.\r\n", ch);
}

void display_characters_ignore_entries(struct char_data *viewer, struct char_data *target) {
  char ignored_bits_str[5000];
  bool self = (viewer == target);

  if (!GET_IGNORE_DATA(target) || GET_IGNORE_DATA(target)->ignored_map.empty()) {
    send_to_char(viewer, "%s %s ignoring anyone.\r\n", self ? "You" : GET_CHAR_NAME(target), self ? "aren't" : "isn't");
    return;
  }

  send_to_char(viewer, "%s %s ignoring:\r\n", self ? "You" : GET_CHAR_NAME(target), self ? "are" : "is");

  for (auto iter : GET_IGNORE_DATA(target)->ignored_map) {
    iter.second.PrintBitsColorized(ignored_bits_str, sizeof(ignored_bits_str), ignored_bits_in_english, NUM_IGNORE_BITS, "^c", "^n");

    const char *ignored_name = get_player_name(iter.first);
    send_to_char(viewer, " - %s: %s\r\n", ignored_name, ignored_bits_str);
    delete [] ignored_name;
  }
}

void log_attempt_to_bypass_ic_ignore(struct char_data *ch, struct char_data *vict, const char *function) {
  char log_buf[5000];
  snprintf(log_buf, sizeof(log_buf), "Blocked attempt by %s to bypass %s's IC ignore flag (%s).", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), function);
  mudlog(log_buf, ch, LOG_IGNORELOG, TRUE);
}
