#include "awake.hpp"
#include "structs.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "newecho.hpp"
#include "ignore_system.hpp"
#include "db.hpp"

extern struct remem *safe_found_mem(struct char_data *rememberer, struct char_data *ch);
const char *get_char_representation_for_docwagon(struct char_data *ch, struct char_data *plr);

int alert_player_doctors_of_mort(struct char_data *ch, struct obj_data *docwagon) {
  char location_info[500], speech_buf[500];
  int potential_rescuer_count = 0;
  struct room_data *in_room;
  const char *gridguide_coords;

  if (!ch || !(in_room = get_ch_in_room(ch))) {
    mudlog("SYSERR: NULL or missing char to alert_player_doctors_of_mort()!", ch, LOG_SYSLOG, TRUE);
    return 0;
  }

  if (!docwagon) {
    mudlog("SYSERR: NULL docwagon to alert_player_doctors_of_mort()!", ch, LOG_SYSLOG, TRUE);
    return 0;
  }

  if (PRF_FLAGGED(ch, PRF_DONT_ALERT_PLAYER_DOCTORS_ON_MORT))
    return 0;

  // If we were doing this right, instead of setting this bit, we'd have a vector on each PC that tracked who we've received alerts from.
  // I don't have the spoons to code that right now, though. Something to improve on later. -LS
  PLR_FLAGS(ch).SetBit(PLR_SENT_DOCWAGON_PLAYER_ALERT);

  if ((gridguide_coords = get_gridguide_coordinates_for_room(in_room))) {
    snprintf(location_info, sizeof(location_info), "GridGuide coordinates [%s], AKA '%s'", gridguide_coords, decapitalize_a_an(GET_ROOM_NAME(in_room)));
  } else {
    snprintf(location_info, sizeof(location_info), "'%s'", decapitalize_a_an(GET_ROOM_NAME(in_room)));
  }

  for (struct char_data *plr = character_list; plr; plr = plr->next) {
    if (IS_NPC(plr) || !plr->desc || plr == ch)
      continue;

    if (IS_IGNORING(plr, is_blocking_ic_interaction_from, ch) || IS_IGNORING(ch, is_blocking_ic_interaction_from, plr))
      continue;

    if (AFF_FLAGGED(plr, AFF_WEARING_ACTIVE_DOCWAGON_RECEIVER) && AWAKE(plr)) {
      const char *display_string = decapitalize_a_an(get_char_representation_for_docwagon(ch, plr));

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

      // If they're not AFK or idle, let the player know that other players were pinged and may be coming.
      if (plr->char_specials.timer < 5 && !PRF_FLAGGED(plr, PRF_AFK)) {
        potential_rescuer_count++;
      }
    }
  }

  return potential_rescuer_count;
}

void alert_player_doctors_of_contract_withdrawal(struct char_data *ch, bool withdrawn_because_of_death) {
  char speech_buf[500];

  if (!ch) {
    mudlog("SYSERR: NULL char to alert_player_doctors_of_contract_withdrawal()!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  if (!PLR_FLAGGED(ch, PLR_SENT_DOCWAGON_PLAYER_ALERT)) {
    // They hadn't actually alerted yet.
    return;
  }
  PLR_FLAGS(ch).RemoveBit(PLR_SENT_DOCWAGON_PLAYER_ALERT);

  if (!find_best_active_docwagon_modulator(ch)) {
    return;
  }

  for (struct char_data *plr = character_list; plr; plr = plr->next) {
    if (IS_NPC(plr) || !plr->desc || plr == ch)
      continue;

    if (IS_IGNORING(plr, is_blocking_ic_interaction_from, ch) || IS_IGNORING(ch, is_blocking_ic_interaction_from, plr))
      continue;

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
