#include "interpreter.h"
#include "newdb.h"
#include "comm.h"
#include "ignore_system.h"

extern void stop_follower(struct char_data *ch);
extern void end_sustained_spell(struct char_data *ch, struct sustain_data *sust);

void send_ignore_usage_to_character(struct char_data *ch) {
  send_to_char("Syntax: ^Wignore <target character> [mode]^n, where [mode] is any of:\r\n", ch);
  for (int i = 0; i < NUM_IGNORE_BITS; i++)
    send_to_char(ch, " - %s\r\n", ignored_bits_in_english[i]);
  send_to_char("\r\nYou may also view your current ignored list with ^Wignore^n on its own, or view the ignores set on a specific character with ^Wignore <character>^n.\r\n", ch);
}

typedef void (IgnoreData::*method_function)(long, const char *, int);
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
    if (GET_IGNORE_DATA(ch)->ignored_map.empty()) {
      send_to_char(ch, "You aren't ignoring anyone.\r\n");
      return;
    }

    send_to_char("You are ignoring:\r\n", ch);

    for (auto iter : GET_IGNORE_DATA(ch)->ignored_map) {
      iter.second.PrintBits(ignored_bits_str, sizeof(ignored_bits_str), ignored_bits_in_english, NUM_IGNORE_BITS);

      const char *ignored_name = get_player_name(iter.first);
      send_to_char(ch, " - ^c%s^n: %s\r\n", ignored_name, ignored_bits_str);
      delete [] ignored_name;
    }

    return;
  }

  // An argument has been supplied; split it up into its constituent parts.
  char first_argument[MAX_INPUT_LENGTH], second_argument[MAX_INPUT_LENGTH];
  char *remainder = any_one_arg(argument, first_argument);
  remainder = any_one_arg(argument, second_argument);

  // We require at least one argument.
  if (!*first_argument) {
    send_ignore_usage_to_character(ch);
    return;
  }

  // Extract the idnum from the first argument and verify that it exists.
  long vict_idnum = get_player_id(first_argument);

  if (vict_idnum <= 0) {
    send_to_char(ch, "Couldn't find any player named '%s'.\r\n", first_argument);
    return;
  }

  // You're not allowed to use this command on staff.
  if (get_player_rank(vict_idnum) > GET_LEVEL(ch)) {
    send_to_char(ch, "You can't do that to staff%s.\r\n", GET_LEVEL(ch) > 1 ? " of a higher rank than yourself.\r\n" : "");
    return;
  }

  // Without a second argument, we display the ignores for the selected character.
  if (!*second_argument) {
    std::unordered_map<long, Bitfield>::iterator results_iterator;

    if ((results_iterator = GET_IGNORE_DATA(ch)->ignored_map.find(vict_idnum)) != GET_IGNORE_DATA(ch)->ignored_map.end()) {
      results_iterator->second.PrintBits(ignored_bits_str, sizeof(ignored_bits_str), ignored_bits_in_english, NUM_IGNORE_BITS);
      send_to_char(ch, "You've set the following ignore flags on %s: %s\r\n", capitalize(second_argument), ignored_bits_str);
    } else {
      send_to_char(ch, "You're not currently ignoring %s.", capitalize(second_argument));
    }

    return;
  }

  // We have both a first and second argument, so treat this as 'ignore <name> <mode>.'

  // Parse out the selected bit.
  int chosen_bit;
  for (chosen_bit = 0; chosen_bit < NUM_IGNORE_BITS; chosen_bit++) {
    if (is_abbrev(second_argument, ignored_bits_in_english[chosen_bit]))
      break;
  }
  if (chosen_bit >= NUM_IGNORE_BITS) {
    send_ignore_usage_to_character(ch);
    return;
  }

  // Finally, toggle the selected bit.
  method_function func = ignore_function_sorted_by_bit[chosen_bit];
  (GET_IGNORE_DATA(ch)->*func) (vict_idnum, capitalize(second_argument), MODE_NOT_SILENT);
}

////////////// Implementation of primary manipulation functions. ///////////////
#define SEND_TO_CH_IF_NOT_SILENT(ch, message, ...)   if (mode != MODE_SILENT) { send_to_char((ch), message, ##__VA_ARGS__); }

void IgnoreData::toggle_blocking_oocs(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_oocs_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now see OOC channel messages from %s.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_OOC_CHANNELS, vict_idnum);
  } else {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will no longer see OOC channel messages (ex: ooc, newbie, etc) from %s. If you also want to block perception of their OSAYs, use 'ignore %s osays'.\r\n", vict_name, vict_name);
    _set_ignore_bit_for(IGNORE_BIT_OOC_CHANNELS, vict_idnum);
  }
}

// Don't display messages from the targeted character on your radio output.
void IgnoreData::toggle_blocking_radios(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_radios_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now see radio messages from %s.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_RADIO, vict_idnum);
  } else {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will no longer see radio messages from %s.\r\n", vict_name);
    _set_ignore_bit_for(IGNORE_BIT_RADIO, vict_idnum);
  }
}

// Prevent the target from sending you mindlinks, and breaks the mindlink if it's set.
void IgnoreData::toggle_blocking_mindlinks(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_mindlinks_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now accept mindlinks from %s.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_MINDLINK, vict_idnum);
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
  }
}

// Prevent the target from sending you tells.
void IgnoreData::toggle_blocking_tells(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_tells_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now see OOC tells from %s.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_TELLS, vict_idnum);
  } else {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will no longer see OOC tells from %s.\r\n", vict_name);
    _set_ignore_bit_for(IGNORE_BIT_TELLS, vict_idnum);
  }
}

// Don't display the target's osays.
void IgnoreData::toggle_blocking_osays(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_osays_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now see osays from %s.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_OSAYS, vict_idnum);
  } else {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will no longer see osays from %s. If you also want to block their OOC channel messages, use 'ignore %s oocs'.\r\n", vict_name, vict_name);
    _set_ignore_bit_for(IGNORE_BIT_OSAYS, vict_idnum);
  }
}

// Prevent the target from interacting with you ICly (forces on several other toggles). Includes socials, emotes/echoes, vemotes, says.
#define SEND_WITH_PRECURSOR(message, ch)  if (!printed_initial_message) { send_to_char(ch, "To ensure this change is effective, we also enabled the following ignore modes for %s:\r\n", vict_name); printed_initial_message = TRUE;} send_to_char(message, ch);
void IgnoreData::toggle_blocking_ic_interaction(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_ic_interaction_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now see the character %s in the game world.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_IC_INTERACTION, vict_idnum);
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
  }
}
#undef SEND_WITH_PRECURSOR

// Prevent the target from following you, and unfollow them if they are. (Abusable in PvP)
void IgnoreData::toggle_blocking_following(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_following_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You now allow %s to follow you.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_FOLLOWING, vict_idnum);
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
  }
}

// Used to block the target from seeing you with the WHERE command.
void IgnoreData::toggle_blocking_where_visibility(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_where_visibility_for(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now allow %s to perceive you on the WHERE list.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_WHERE, vict_idnum);
  } else {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You no longer allow %s to perceive you on the WHERE list.\r\n", vict_name);
    _set_ignore_bit_for(IGNORE_BIT_WHERE, vict_idnum);
  }
}

// Block calls from the targeted person.
void IgnoreData::toggle_blocking_calls(long vict_idnum, const char *vict_name, int mode) {
  if (is_blocking_calls_from(vict_idnum)) {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You will now allow %s to call you.\r\n", vict_name);
    _remove_ignore_bit_for(IGNORE_BIT_CALLS, vict_idnum);
  } else {
    SEND_TO_CH_IF_NOT_SILENT(ch, "You no longer allow %s to call you.\r\n", vict_name);
    _set_ignore_bit_for(IGNORE_BIT_CALLS, vict_idnum);
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
  // todo
}

void IgnoreData::_delete_entry_from_db(long vict_idnum) {
  // todo
}

void IgnoreData::load_from_db() {
  // todo
}
