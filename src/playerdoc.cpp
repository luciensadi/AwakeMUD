#include "awake.hpp"
#include "structs.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "newecho.hpp"
#include "ignore_system.hpp"
#include "db.hpp"
#include "interpreter.hpp"

extern struct remem *safe_found_mem(struct char_data *rememberer, struct char_data *ch);
extern void display_room_name(struct char_data *ch);
extern void display_room_desc(struct char_data *ch);
extern void disp_long_exits(struct char_data *ch, bool autom);
extern int isname(const char *str, const char *namelist);

const char *get_char_representation_for_docwagon(struct char_data *ch, struct char_data *plr);

#define IS_VALID_POTENTIAL_RESCUER(plr) (GET_LEVEL(plr) == LVL_MORTAL && plr->char_specials.timer < 5 && !PRF_FLAGGED(plr, PRF_AFK) && !PRF_FLAGGED(plr, PRF_QUEST))

int alert_player_doctors_of_mort(struct char_data *ch, struct obj_data *docwagon) {
  char location_info[500], speech_buf[500];
  int potential_rescuer_count = 0;
  struct room_data *in_room;
  const char *gridguide_coords;

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

  if ((gridguide_coords = get_gridguide_coordinates_for_room(in_room))) {
    snprintf(location_info, sizeof(location_info), "GridGuide coordinates [%s], AKA '%s' (%ld)", gridguide_coords, decapitalize_a_an(GET_ROOM_NAME(in_room)), GET_ROOM_VNUM(in_room));
  } else {
    snprintf(location_info, sizeof(location_info), "'%s' (%ld)", decapitalize_a_an(GET_ROOM_NAME(in_room)), GET_ROOM_VNUM(in_room));
  }

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

    // If they're not staff, AFK, idle, or participating in a PRUN, add them to the potential rescuer count that will be sent to the downed player.
    if (IS_VALID_POTENTIAL_RESCUER(plr)) {
      send_to_char("^c(Please send a ^WTELL^c or announce on ^WOOC^c if you're on your way!)^n\r\n", plr);
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

  for (struct char_data *plr = character_list; plr; plr = plr->next) {
    if (IS_NPC(plr) || !plr->desc || plr == ch)
      continue;

    if (IS_IGNORING(plr, is_blocking_ic_interaction_from, ch) || IS_IGNORING(ch, is_blocking_ic_interaction_from, plr))
      continue;

    // We didn't message this person.
    if (ch->sent_docwagon_messages_to.find(GET_IDNUM(plr)) == ch->sent_docwagon_messages_to.end()) {
      continue;
    }

    if (AFF_FLAGGED(plr, AFF_WEARING_ACTIVE_DOCWAGON_RECEIVER)) {
      const char *display_string = get_string_after_color_code_removal(CAP(get_char_representation_for_docwagon(ch, plr)), NULL);

      if (withdrawn_because_of_death) {
        snprintf(speech_buf, sizeof(speech_buf), "Contract withdrawal notice: %s no longer has viable vital signs.", display_string);

        send_to_char(plr,
                     "Your DocWagon receiver emits a sad beep and displays: \"^r%s^n\"\r\n",
                     capitalize(replace_too_long_words(plr, NULL, speech_buf, SKILL_ENGLISH, "^r")));

        if (plr->in_room) {
         act("$n's DocWagon receiver emits a sad beep.", TRUE, plr, 0, 0, TO_ROOM);
        } else if (plr->in_veh) {
         act("$n's DocWagon receiver emits a sad beep.", TRUE, plr, 0, 0, TO_VEH);
        }
      } else {
        bool in_same_room = get_ch_in_room(plr) == get_ch_in_room(ch);

        snprintf(speech_buf, sizeof(speech_buf),
                 "Contract %s notice: %s is no longer incapacitated.%s",
                 in_same_room ? "completion" : "withdrawal",
                 display_string,
                 in_same_room ? " Well done!" : "");

        send_to_char(plr,
                     "Your DocWagon receiver emits a cheery beep and displays: \"%s%s^n\"\r\n",
                     in_same_room ? "^c" : "^o",
                     capitalize(replace_too_long_words(plr, NULL, speech_buf, SKILL_ENGLISH, "^c")));

        if (plr->in_room) {
         act("$n's DocWagon receiver emits a cheery beep.", TRUE, plr, 0, 0, TO_ROOM);
        } else if (plr->in_veh) {
         act("$n's DocWagon receiver emits a cheery beep.", TRUE, plr, 0, 0, TO_VEH);
        }
      }
    }
  }

  // Purge their Docwagon list.
  ch->sent_docwagon_messages_to.clear();
}

bool handle_player_docwagon_track(struct char_data *ch, char *argument) {
  skip_spaces(&argument);

  // This only works for people with receivers.
  if (!AFF_FLAGGED(ch, AFF_WEARING_ACTIVE_DOCWAGON_RECEIVER) || !AWAKE(ch))
    return FALSE;

  for (struct char_data *vict = character_list; vict; vict = vict->next) {
    if (IS_NPC(vict) || !vict->desc || GET_POS(vict) != POS_MORTALLYW)
      continue;

    if (IS_IGNORING(vict, is_blocking_ic_interaction_from, ch) || IS_IGNORING(ch, is_blocking_ic_interaction_from, vict))
      continue;

    if (isname(argument, get_char_representation_for_docwagon(vict, ch))) {
      send_to_char("You squint at the tiny screen on your DocWagon receiver to try and get a better idea of where your client is...\r\n", ch);

      // Show them the room name, room description, and exits.
      struct room_data *was_in_room = ch->in_room;
      ch->in_room = get_ch_in_room(vict);

      // Room name.
      display_room_name(ch);

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

ACMD(do_docwagon) {
  send_to_char(ch, "Command not yet implemented.\r\n");
/*
  int mode = 0;

  skip_spaces(&arg);

  if (!*arg) {
    send_to_char("Syntax: DOCWAGON (ACCEPT|WITHDRAW) <name>\r\n", ch);
    return;
  }

  if (is_abbrev(arg, "accept") || is_abbrev(arg, "acknowledge") || is_abbrev(arg, "take")) {
    mode = 1;
  }
  else if (is_abbrev(arg, "decline") || is_abbrev(arg, "withdraw") || is_abbrev(arg, "reject") || is_abbrev(arg, "drop")) {
    mode = 2;
  }
  else {
    send_to_char("Syntax: DOCWAGON (ACCEPT|WITHDRAW) <name>\r\n", ch);
    return;
  }

  const char *remainder = any_one_arg(arg, buf);

  // Find the downed person.
  for (struct char_data *tmp = character_list; tmp; tmp = tmp->next) {
    if (!keyword_appears_in_char(buf, tmp, TRUE, TRUE, FALSE))
      continue;

    if (PRF_FLAGGED(tmp, PRF_DONT_ALERT_PLAYER_DOCTORS_ON_MORT))
      continue;

    if (IS_NPC(tmp) || !tmp->desc || tmp == ch)
      continue;

    if (IS_IGNORING(tmp, is_blocking_ic_interaction_from, ch) || IS_IGNORING(ch, is_blocking_ic_interaction_from, tmp))
      continue;

    // TODO: must have sent us a message

    // TODO: if we've already ack'd, don't ack again-- if already refused, don't refuse again

    // TODO: log it

    if (mode == 1) {
      send_to_char(tmp, "Your DocWagon modulator buzzes-- someone has acknowledged your request for assistance and is on their way!\r\n");
    } else {
      send_to_char(tmp, "Your DocWagon modulator buzzes-- someone who was on their way has dropped the contract.\r\n");
    }
    send_to_char(ch, "You anonymously notify them that you're %son the way.", mode != 1 ? "no longer " : "");
    return;
  }
*/
}
