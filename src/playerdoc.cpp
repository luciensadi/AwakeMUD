#include "awake.hpp"
#include "structs.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "newecho.hpp"
#include "ignore_system.hpp"
#include "db.hpp"
#include "interpreter.hpp"

extern int global_non_secure_random_number;

extern struct remem *safe_found_mem(struct char_data *rememberer, struct char_data *ch);
extern void display_room_name(struct char_data *ch, struct room_data *in_room, bool in_veh);
extern void display_room_desc(struct char_data *ch);
extern void disp_long_exits(struct char_data *ch, bool autom);
extern int isname(const char *str, const char *namelist);

const char *get_char_representation_for_docwagon(struct char_data *ch, struct char_data *plr);

#define IS_VALID_POTENTIAL_RESCUER(plr) (GET_LEVEL(plr) == LVL_MORTAL && plr->char_specials.timer < 5 && !PRF_FLAGGED(plr, PRF_AFK) && !PRF_FLAGGED(plr, PRF_QUEST))

// Returns a 5-digit faux ID to help tell characters apart in anonymous messages.
int get_docwagon_faux_id(struct char_data *ch) {
  return (((GET_IDNUM(ch) * 217 + global_non_secure_random_number) + 29783) / 3) % 99999;
}

const char *get_location_string_for_room(struct room_data *in_room) {
  static char location_info[1000] = { 0 };
  const char *gridguide_coords = get_gridguide_coordinates_for_room(in_room);

  if (!in_room) {
    mudlog("SYSERR: Received invalid in_room to get_location_string_for_room()!", NULL, LOG_SYSLOG, TRUE);
    return "";
  }

  if (gridguide_coords) {
    snprintf(location_info, sizeof(location_info), "GridGuide coordinates [%s], AKA '%s' (%ld)", gridguide_coords, decapitalize_a_an(GET_ROOM_NAME(in_room)), GET_ROOM_VNUM(in_room));
  } else {
    snprintf(location_info, sizeof(location_info), "'%s' (%ld)", decapitalize_a_an(GET_ROOM_NAME(in_room)), GET_ROOM_VNUM(in_room));
  }

  return location_info;
}

int alert_player_doctors_of_mort(struct char_data *ch, struct obj_data *docwagon) {
  char speech_buf[500];
  int potential_rescuer_count = 0;
  struct room_data *in_room;
  const char *location_info;

  if (!ch || !(in_room = get_ch_in_room(ch))) {
    mudlog("SYSERR: NULL or missing char to alert_player_doctors_of_mort()!", ch, LOG_SYSLOG, TRUE);
    return 0;
  }

  // They don't have a modulator-- not an error, just bail.
  if (!docwagon && !(docwagon = find_best_active_docwagon_modulator(ch)))
    return 0;

  // They don't want to participate-- not an error, just bail.
  if (PRF_FLAGGED(ch, PRF_DONT_ALERT_PLAYER_DOCTORS_ON_MORT))
    return 0;

  location_info = get_location_string_for_room(in_room);

  for (struct char_data *plr = character_list; plr; plr = plr->next) {
    if (IS_NPC(plr) || !plr->desc || plr == ch)
      continue;

    if (!AFF_FLAGGED(plr, AFF_WEARING_ACTIVE_DOCWAGON_RECEIVER) || !AWAKE(plr))
      continue;

    if (IS_IGNORING(plr, is_blocking_ic_interaction_from, ch) || IS_IGNORING(ch, is_blocking_ic_interaction_from, plr))
      continue;

    // Compose the display string.
    const char *display_string = decapitalize_a_an(get_char_representation_for_docwagon(ch, plr));

    // We already sent this person a message, so just prompt them instead of doing the whole thing.
    if (ch->sent_docwagon_messages_to.find(GET_IDNUM(plr)) != ch->sent_docwagon_messages_to.end()) {
      send_to_char(plr, "Your DocWagon receiver vibrates, indicating that %s still needs assistance.\r\n", display_string);

      if (IS_VALID_POTENTIAL_RESCUER(plr)) {
        potential_rescuer_count++;
      }

      continue;
    }

    switch (GET_DOCWAGON_CONTRACT_GRADE(docwagon)) {
      case DOCWAGON_GRADE_PLATINUM:
        snprintf(speech_buf, sizeof(speech_buf),
                 "Any available unit, we have a Platinum-grade contractee downed at %s."
                 " Records show them as %s. This is a highest-priority recovery!",
                 location_info,
                 get_string_after_color_code_removal(display_string, NULL));

        send_to_char(plr,
                     "Your DocWagon receiver emits a shrill alarm, followed by a brusque human voice: \"^Y%s^n\"\r\n",
                     capitalize(replace_too_long_words(plr, NULL, speech_buf, SKILL_ENGLISH, "^Y")));

        if (plr->in_room) {
          act("$n's DocWagon receiver emits a shrill alarm.", TRUE, plr, 0, 0, TO_ROOM);
          for (struct char_data *mob = plr->in_room->people; mob; mob = mob->next_in_room) {
            if (IS_NPC(mob) && !mob->desc) {
              GET_MOBALERT(mob) = MAX(GET_MOBALERT(mob), MALERT_ALERT);
              GET_MOBALERTTIME(mob) = MAX(GET_MOBALERTTIME(mob), 20);
            }
          }
        } else if (plr->in_veh) {
          act("$n's DocWagon receiver emits a shrill alarm.", TRUE, plr, 0, 0, TO_VEH);
        }
        break;
      case DOCWAGON_GRADE_GOLD:
        snprintf(speech_buf, sizeof(speech_buf),
                 "Recovery specialist, a Gold-grade contractee has been downed at %s. Records show them as %s. Render aid if possible.",
                 location_info,
                 get_string_after_color_code_removal(display_string, NULL));

        send_to_char(plr,
                     "Your DocWagon receiver beeps loudly, followed by an automated voice: \"^y%s^n\"\r\n",
                     capitalize(replace_too_long_words(plr, NULL, speech_buf, SKILL_ENGLISH, "^y")));

        if (plr->in_room) {
          act("$n's DocWagon receiver beeps loudly.", TRUE, plr, 0, 0, TO_ROOM);
          for (struct char_data *mob = plr->in_room->people; mob; mob = mob->next_in_room) {
            if (IS_NPC(mob) && !mob->desc) {
              GET_MOBALERT(mob) = MAX(GET_MOBALERT(mob), MALERT_ALERT);
              GET_MOBALERTTIME(mob) = MAX(GET_MOBALERTTIME(mob), 20);
            }
          }
        } else if (plr->in_veh) {
          act("$n's DocWagon receiver beeps loudly.", TRUE, plr, 0, 0, TO_VEH);
        }
        break;
      case DOCWAGON_GRADE_BASIC:
      default:
        if (GET_DOCWAGON_CONTRACT_GRADE(docwagon) != DOCWAGON_GRADE_BASIC) {
          char oopsbuf[500];
          snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Unknown DocWagon modulator grade %d for %s (%ld)!", GET_DOCWAGON_CONTRACT_GRADE(docwagon), GET_OBJ_NAME(docwagon), GET_OBJ_VNUM(docwagon));
          mudlog(oopsbuf, ch, LOG_SYSLOG, TRUE);
        }

        snprintf(speech_buf, sizeof(speech_buf),
                 "Notice: Basic-grade contractee downed at %s. Recover if safe to do so.",
                 location_info);

        send_to_char(plr,
                     "Your DocWagon receiver vibrates, and text flashes up on its screen: \"^W%s^n\" An accompanying image of %s^n is displayed.\r\n",
                     capitalize(replace_too_long_words(plr, NULL, speech_buf, SKILL_ENGLISH, "^W")),
                     display_string);
        break;
    }

    if (!IS_SENATOR(plr)) {
      send_to_char(plr, "^c(Please announce on ^WOOC^c if you're on your way! Alternatively, use ^WDOCWAGON ACCEPT %s^c for anonymous response.)^n\r\n", GET_CHAR_NAME(ch));
    }

    // If they're not staff, AFK, idle, or participating in a PRUN, add them to the potential rescuer count that will be sent to the downed player.
    if (IS_VALID_POTENTIAL_RESCUER(plr)) {
      potential_rescuer_count++;
    }

    // Add them to our sent-to list.
    ch->sent_docwagon_messages_to.insert(std::make_pair(GET_IDNUM(plr), TRUE));
  }

  return potential_rescuer_count;
}

void alert_player_doctors_of_contract_withdrawal(struct char_data *ch, bool withdrawn_because_of_death) {
  char speech_buf[500];

  if (!ch) {
    mudlog("SYSERR: NULL char to alert_player_doctors_of_contract_withdrawal()!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  if (ch->sent_docwagon_messages_to.empty()) {
    // They hadn't actually alerted yet.
    return;
  }

  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    if (!d->character || d->character == ch)
      continue;

    if (IS_IGNORING(d->character, is_blocking_ic_interaction_from, ch) || IS_IGNORING(ch, is_blocking_ic_interaction_from, d->character))
      continue;

    // We didn't message this person.
    if (ch->sent_docwagon_messages_to.find(GET_IDNUM(d->character)) == ch->sent_docwagon_messages_to.end()) {
      continue;
    }

    if (AFF_FLAGGED(d->character, AFF_WEARING_ACTIVE_DOCWAGON_RECEIVER)) {
      const char *display_string = get_string_after_color_code_removal(CAP(get_char_representation_for_docwagon(ch, d->character)), NULL);

      if (withdrawn_because_of_death) {
        snprintf(speech_buf, sizeof(speech_buf), "Contract withdrawal notice: %s no longer has viable vital signs.", display_string);

        send_to_char(d->character,
                     "Your DocWagon receiver emits a sad beep and displays: \"^r%s^n\"\r\n",
                     capitalize(replace_too_long_words(d->character, NULL, speech_buf, SKILL_ENGLISH, "^r")));

        if (d->character->in_room) {
         act("$n's DocWagon receiver emits a sad beep.", TRUE, d->character, 0, 0, TO_ROOM);
        } else if (d->character->in_veh) {
         act("$n's DocWagon receiver emits a sad beep.", TRUE, d->character, 0, 0, TO_VEH);
        }
      } else {
        bool in_same_room = get_ch_in_room(d->character) == get_ch_in_room(ch);

        snprintf(speech_buf, sizeof(speech_buf),
                 "Contract %s notice: %s is no longer incapacitated.%s",
                 in_same_room ? "completion" : "withdrawal",
                 display_string,
                 in_same_room ? " Well done!" : "");

        send_to_char(d->character,
                     "Your DocWagon receiver emits a cheery beep and displays: \"%s%s^n\"\r\n",
                     in_same_room ? "^c" : "^o",
                     capitalize(replace_too_long_words(d->character, NULL, speech_buf, SKILL_ENGLISH, "^c")));

        if (d->character->in_room) {
         act("$n's DocWagon receiver emits a cheery beep.", TRUE, d->character, 0, 0, TO_ROOM);
        } else if (d->character->in_veh) {
         act("$n's DocWagon receiver emits a cheery beep.", TRUE, d->character, 0, 0, TO_VEH);
        }
      }
    }
  }

  // Purge their Docwagon lists.
  ch->sent_docwagon_messages_to.clear();
  ch->received_docwagon_ack_from.clear();
}

bool handle_player_docwagon_track(struct char_data *ch, char *argument) {
  skip_spaces(&argument);

  // This only works for people with receivers.
  if (!AFF_FLAGGED(ch, AFF_WEARING_ACTIVE_DOCWAGON_RECEIVER) || !AWAKE(ch))
    return FALSE;

  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    // Invalid person?
    if (!d->character || d->character == ch || GET_POS(d->character) != POS_MORTALLYW)
      continue;

    if (IS_IGNORING(d->character, is_blocking_ic_interaction_from, ch) || IS_IGNORING(ch, is_blocking_ic_interaction_from, d->character))
      continue;

    if (isname(argument, get_char_representation_for_docwagon(d->character, ch))) {
      send_to_char("You squint at the tiny screen on your DocWagon receiver to try and get a better idea of where your client is...\r\n", ch);

      // Show them the room name, room description, and exits.
      struct room_data *was_in_room = ch->in_room;
      ch->in_room = get_ch_in_room(d->character);

      // Room name.
      display_room_name(ch, ch->in_room, FALSE);

      // Room desc.
      display_room_desc(ch);

      // Room exits.
      disp_long_exits(ch, TRUE);

      // Reset their in_room to the stored value.
      ch->in_room = was_in_room;

      return TRUE;
    }
  }

  send_to_char(ch, "You don't see any DocWagon clients named '%s' available to be tracked.\r\n", argument);
  return TRUE;
}

const char *get_char_representation_for_docwagon(struct char_data *ch, struct char_data *plr) {
  // Compose the perceived description.
  struct remem *mem_record;
  static char display_string[500];

  strlcpy(display_string, decapitalize_a_an(GET_NAME(ch)), sizeof(display_string));

  if ((mem_record = safe_found_mem(plr, ch)))
    snprintf(ENDOF(display_string), sizeof(display_string) - strlen(display_string), "^n (%s)", CAP(mem_record->mem));
  else if (IS_SENATOR(plr))
    snprintf(ENDOF(display_string), sizeof(display_string) - strlen(display_string), "^n (%s)", GET_CHAR_NAME(ch));

  return display_string;
}

#define MODE_ACCEPT  1
#define MODE_DECLINE 2
#define MODE_LIST    3

ACMD(do_docwagon) {
  int mode = 0;
  char mode_switch[MAX_INPUT_LENGTH] = { 0 };
  char output[MAX_STRING_LENGTH] = { 0 };

  // This only works for people with receivers.
  FAILURE_CASE(!AFF_FLAGGED(ch, AFF_WEARING_ACTIVE_DOCWAGON_RECEIVER), "You need to be wearing a DocWagon receiver to use this command-- modulators don't work here.");

  skip_spaces(&argument);
  char *name = one_argument(argument, mode_switch);

  if (!*arg || !*mode_switch) {
    send_to_char("Syntax: ^WDOCWAGON ACCEPT <name>^n, ^WDOCWAGON WITHDRAW <name>^n, ^WDOCWAGON LIST^n, or ^WDOCWAGON LOCATE <name>^n.\r\n", ch);
    return;
  }

  if (is_abbrev(mode_switch, "accept") || is_abbrev(mode_switch, "acknowledge") || is_abbrev(mode_switch, "take")) {
    FAILURE_CASE(!*name, "Syntax: DOCWAGON ACCEPT <name>");
      mode = MODE_ACCEPT;
  }
  else if (is_abbrev(mode_switch, "decline") || is_abbrev(mode_switch, "withdraw") || is_abbrev(mode_switch, "reject") || is_abbrev(mode_switch, "drop")) {
    FAILURE_CASE(!*name, "Syntax: DOCWAGON DECLINE <name>");
    mode = MODE_DECLINE;
  }
  else if (is_abbrev(mode_switch, "list")) {
    mode = MODE_LIST;
  }
  else if (is_abbrev(mode_switch, "show") || is_abbrev(mode_switch, "locate") || is_abbrev(mode_switch, "track")) {
    FAILURE_CASE(!*name, "Syntax: DOCWAGON LOCATE <name>");
    handle_player_docwagon_track(ch, name);
    return;
  }
  else {
    send_to_char("Syntax: DOCWAGON (ACCEPT|WITHDRAW) <name>\r\n", ch);
    return;
  }

  // Find the downed person.
  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    // Invalid person?
    if (!d->character || d->character == ch || GET_POS(d->character) != POS_MORTALLYW)
      continue;

    // Couldn't have alerted in the first place?
    if (PRF_FLAGGED(d->character, PRF_DONT_ALERT_PLAYER_DOCTORS_ON_MORT))
      continue;

    // Being ignored?
    if (IS_IGNORING(d->character, is_blocking_ic_interaction_from, ch) || IS_IGNORING(ch, is_blocking_ic_interaction_from, d->character))
      continue;

    // Short circuit: LIST has no further logic to evaluate, so just print.
    if (mode == MODE_LIST) {
      snprintf(ENDOF(output), sizeof(output) - strlen(output), " - %s: %s\r\n",
               get_char_representation_for_docwagon(ch, d->character),
               get_location_string_for_room(get_ch_in_room(d->character)));
      continue;
    }

    // Wrong person? (does not apply to DOCWAGON LIST)
    if (!keyword_appears_in_char(name, d->character, TRUE, TRUE, FALSE))
      continue;

    // They have not yet received a message from us. ACCEPT is valid, WITHDRAW is not.
    if (d->character->received_docwagon_ack_from.find(GET_IDNUM(ch)) == d->character->received_docwagon_ack_from.end()) {
      if (mode == MODE_DECLINE) {
        send_to_char(ch, "You haven't messaged %s yet. Use DOCWAGON ACCEPT instead.\r\n", GET_CHAR_NAME(d->character));
        return;
      }
      send_to_char("You anonymously notify them that you're on the way.", ch);
      send_to_char(d->character, "Your DocWagon modulator buzzes-- someone with the DocWagon ID %5d has acknowledged your request for assistance and is on their way!\r\n", get_docwagon_faux_id(ch));
      d->character->received_docwagon_ack_from.insert(std::make_pair(GET_IDNUM(ch), TRUE));
      mudlog_vfprintf(ch, LOG_GRIDLOG, "%s has accepted %s's DocWagon contract.", GET_CHAR_NAME(ch), GET_CHAR_NAME(d->character));
      return;
    }
    // They have already received a message. WITHDRAW is valid, ACCEPT is not.
    else {
      if (mode == MODE_ACCEPT) {
        send_to_char(ch, "You've already messaged %s.\r\n", GET_CHAR_NAME(d->character));
        return;
      }
      send_to_char("You anonymously notify them that you're no longer on the way.", ch);
      send_to_char(d->character, "Your DocWagon modulator buzzes-- someone with the DocWagon ID %5d is no longer able to respond to your contract.\r\n", get_docwagon_faux_id(ch));
      d->character->received_docwagon_ack_from.erase(d->character->received_docwagon_ack_from.find(GET_IDNUM(ch)));
      mudlog_vfprintf(ch, LOG_GRIDLOG, "%s has dropped %s's DocWagon contract (command).", GET_CHAR_NAME(ch), GET_CHAR_NAME(d->character));
      return;
    }
  }

  if (mode == MODE_LIST) {
    if (*output) {
      send_to_char(ch, "Patients in need of assistance:\r\n%s\r\n", output);
    } else {
      send_to_char("Your receiver isn't picking up any distress signals.\r\n", ch);
    }
    return;
  }

  send_to_char(ch, "Your DocWagon receiver can't find anyone in need of assistance named '%s^n'.\r\n", name);
}
