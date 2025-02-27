/* ************************************************************************
*   File: act.comm.c                                    Part of CircleMUD *
*  Usage: Player-level communication commands                             *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <iostream>

// using namespace std;

#include "structs.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "screen.hpp"
#include "awake.hpp"
#include "newmatrix.hpp"
#include "newdb.hpp"
#include "constants.hpp"
#include "newecho.hpp"
#include "ignore_system.hpp"
#include "config.hpp"
#include "moderation.hpp"

/* extern variables */
extern struct skill_data skills[];

extern void respond(struct char_data *ch, struct char_data *mob, char *str);
extern bool can_send_act_to_target(struct char_data *ch, bool hide_invisible, struct obj_data * obj, void *vict_obj, struct char_data *to, int type);
extern char *how_good(int skill, int percent);
extern const char *get_voice_perceived_by(struct char_data *speaker, struct char_data *listener, bool invis_staff_should_be_identified);
int find_skill_num(char *name);
void ring_phone(struct phone_data *k);


ACMD_DECLARE(do_say);

ACMD_CONST(do_say) {
  static char not_const[MAX_STRING_LENGTH];
  strlcpy(not_const, argument, sizeof(not_const));
  do_say(ch, not_const, cmd, subcmd);
}

ACMD(do_say)
{
  struct char_data *tmp, *to = NULL;

  int language = !SKILL_IS_LANGUAGE(GET_LANGUAGE(ch)) ? SKILL_ENGLISH : GET_LANGUAGE(ch);

  skip_spaces(&argument);

  FAILURE_CASE(!*argument, "Yes, but WHAT do you want to say?");

  // If they trigger automod with this, bail out.
  if (check_for_banned_content(argument, ch))
    return;

  if (ch->desc && subcmd != SCMD_OSAY && !PLR_FLAGGED(ch, PLR_MATRIX) && !char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
    return;

  FAILURE_CASE(IS_RIGGING(ch), "You have no mouth.");

  char arg_known_size[MAX_INPUT_LENGTH + 1];
  strlcpy(arg_known_size, argument, sizeof(arg_known_size));

  if (PLR_FLAGGED(ch, PLR_MATRIX)) {
    if (subcmd != SCMD_OSAY && !has_required_language_ability_for_sentence(ch, arg_known_size, language))
      return;

    // We specifically do not use color highlights in the Matrix. Some people want their mtx persona distinct from their normal one.

    if (ch->persona) {
      // Send the self-referencing message to the decker and store it in their history.
      snprintf(buf, sizeof(buf), "You say, \"%s^n\"\r\n", capitalize(arg_known_size));
      send_to_icon(ch->persona, buf);
      store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, buf);

      // Send the message to the rest of the host. Store it to the recipients' says history.
      snprintf(buf, sizeof(buf), "%s^n says, \"%s^n\"\r\n", ch->persona->name, capitalize(arg_known_size));
      // send_to_host(ch->persona->in_host, buf, ch->persona, TRUE);
      for (struct matrix_icon *i = matrix[ch->persona->in_host].icons; i; i = i->next_in_host) {
        if (ch->persona != i && i->decker && has_spotted(i, ch->persona)) {
          // We don't need an if (i->decker->ch) here-- it's implied by IS_IGNORING.
          if (!IS_IGNORING(i->decker->ch, is_blocking_ic_interaction_from, ch)) {
            send_to_icon(i, buf);

            if (i->decker->ch)
              store_message_to_history(i->decker->ch->desc, COMM_CHANNEL_SAYS, buf);
          }
        }
      }
    } else {
      for (struct char_data *targ = get_ch_in_room(ch)->people; targ; targ = targ->next_in_room) {
        if (targ == ch
            || !PLR_FLAGGED(targ, PLR_MATRIX)
            || !targ->persona
            || !targ->persona->decker
            || targ->persona->decker->hitcher != ch
          ) {
          continue;
        }
        if (!IS_IGNORING(targ, is_blocking_ic_interaction_from, ch)) {// Gag check for ignored characters
          // We found our hitcher
          snprintf(buf, sizeof(buf), "Your hitcher says, \"%s^n\"\r\n", capitalize(arg_known_size));
          send_to_char(buf, targ);
          store_message_to_history(targ->desc, COMM_CHANNEL_SAYS, buf);
        }

        // Send and store.
        snprintf(buf, sizeof(buf), "You send, down the line, \"%s^n\"\r\n", capitalize(arg_known_size));
        send_to_char(buf, ch);
        store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, buf);
      }
    }
    return;
  }

  if (subcmd == SCMD_SAYTO) {
    half_chop(arg_known_size, buf, buf2, sizeof(buf2));
    strlcpy(arg_known_size, buf2, sizeof(arg_known_size));

    if (ch->in_veh)
      to = get_char_veh(ch, buf, ch->in_veh);
    else
      to = get_char_room_vis(ch, buf);

    if (to == ch) {
      send_to_char("You probably shouldn't talk to yourself in public.\r\n", ch);
      return;
    }
  }

  // This is down here to handle speech after sayto. Note that matrix has no sayto, so we did it there as well.
  if (subcmd != SCMD_OSAY && !has_required_language_ability_for_sentence(ch, arg_known_size, language))
    return;

  if (subcmd == SCMD_OSAY) {
    // No color highlights for osay.
    snprintf(buf, sizeof(buf), "$n^n says ^mOOCly^n, \"%s%s^n\"", capitalize(arg_known_size), get_final_character_from_string(arg_known_size) == '^' ? "^" : "");
    for (tmp = ch->in_room ? ch->in_room->people : ch->in_veh->people; tmp; tmp = ch->in_room ? tmp->next_in_room : tmp->next_in_veh) {
      // Replicate act() in a way that lets us capture the message.
      if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM) && !IS_IGNORING(tmp, is_blocking_osays_from, ch)) {
        // They're a valid target, so send the message with a raw perform_act() call.
        store_message_to_history(tmp->desc, COMM_CHANNEL_OSAYS, perform_act(buf, ch, NULL, NULL, tmp, FALSE));
      }
    }
    // Send it to anyone who's rigging a vehicle here.
    for (struct veh_data *veh = ch->in_room ? ch->in_room->vehicles : ch->in_veh->carriedvehs;
         veh;
         veh = veh->next_veh)
    {
      if (veh->rigger && veh->rigger->desc && !IS_IGNORING(veh->rigger, is_blocking_osays_from, ch))
        store_message_to_history(veh->rigger->desc, COMM_CHANNEL_OSAYS, perform_act(buf, ch, NULL, NULL, veh->rigger, FALSE));
    }
  } else {
    // Speech gives you 5 minutes of grace period. Emotes give you more.
    ch->char_specials.last_social_action = LAST_SOCIAL_ACTION_REQUIREMENT_FOR_CONGREGATION_BONUS - SOCIAL_ACTION_GRACE_PERIOD_GRANTED_BY_SPEECH;

    for (tmp = ch->in_room ? ch->in_room->people : ch->in_veh->people; tmp; tmp = ch->in_room ? tmp->next_in_room : tmp->next_in_veh) {
      if (tmp == ch || IS_IGNORING(tmp, is_blocking_ic_interaction_from, ch))
        continue;

      // Generate prepend for sayto.
      if (to) {
        if (to == tmp)
          snprintf(buf2, MAX_STRING_LENGTH, " to you");
        else
          snprintf(buf2, MAX_STRING_LENGTH, " to %s^n", CAN_SEE(tmp, to) ? (safe_found_mem(tmp, to) ? CAP(safe_found_mem(tmp, to)->mem)
                  : GET_NAME(to)) : "someone");
      }

      snprintf(buf, sizeof(buf), "$z^n says%s in %s, \"%s%s%s^n\"",
              (to ? buf2 : ""),
              (IS_NPC(tmp) || GET_SKILL(tmp, language) > 0) ? skills[language].name : "an unknown language",
              (PRF_FLAGGED(tmp, PRF_NOHIGHLIGHT) || PRF_FLAGGED(tmp, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
              capitalize(replace_too_long_words(tmp, ch, arg_known_size, language, GET_CHAR_COLOR_HIGHLIGHT(ch))),
              ispunct(get_final_character_from_string(arg_known_size)) ? "" : "."
            );

      // Note: includes act()
      store_message_to_history(tmp->desc, COMM_CHANNEL_SAYS, act(buf, FALSE, ch, NULL, tmp, TO_VICT));
    }
    // Send it to anyone who's rigging a vehicle here.
    for (struct veh_data *veh = ch->in_room ? ch->in_room->vehicles : ch->in_veh->carriedvehs;
         veh;
         veh = veh->next_veh)
    {
      if (veh->rigger && veh->rigger->desc && !IS_IGNORING(veh->rigger, is_blocking_ic_interaction_from, ch)) {
        // Generate prepend for sayto.
        if (to) {
          snprintf(buf2, MAX_STRING_LENGTH, " to %s^n", CAN_SEE(veh->rigger, to) ? (safe_found_mem(veh->rigger, to) ? CAP(safe_found_mem(veh->rigger, to)->mem)
                   : GET_NAME(to)) : "someone");
        }

        snprintf(buf, sizeof(buf), "$z^n says%s in %s, \"%s%s%s^n\"",
                (to ? buf2 : ""),
                (IS_NPC(veh->rigger) || GET_SKILL(veh->rigger, language) > 0) ? skills[language].name : "an unknown language",
                (PRF_FLAGGED(veh->rigger, PRF_NOHIGHLIGHT) || PRF_FLAGGED(veh->rigger, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
                capitalize(replace_too_long_words(veh->rigger, ch, arg_known_size, language, GET_CHAR_COLOR_HIGHLIGHT(ch))),
                ispunct(get_final_character_from_string(arg_known_size)) ? "" : "."
              );
        store_message_to_history(veh->rigger->desc, COMM_CHANNEL_SAYS, perform_act(buf, ch, NULL, NULL, veh->rigger, FALSE));
      }
    }
  }

  // Acknowledge transmission of message.
  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);

  else {
    delete_doubledollar(arg_known_size);
    if(subcmd == SCMD_OSAY) {
      snprintf(buf, sizeof(buf), "You say ^mOOCly^n, \"%s^n\"\r\n", capitalize(arg_known_size));
      send_to_char(buf, ch);
      store_message_to_history(ch->desc, COMM_CHANNEL_OSAYS, buf);
    }

    else {
      if (to)
        snprintf(buf2, MAX_STRING_LENGTH, " to %s^n", CAN_SEE(ch, to) ? (safe_found_mem(ch, to) ?
                                                   CAP(safe_found_mem(ch, to)->mem) : GET_NAME(to)) : "someone");
      snprintf(buf, sizeof(buf), "You say%s in %s, \"%s%s%s^n\"\r\n",
               (to ? buf2 : ""),
               skills[language].name,
               (PRF_FLAGGED(ch, PRF_NOHIGHLIGHT) || PRF_FLAGGED(ch, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
               capitalize(arg_known_size),
               ispunct(get_final_character_from_string(arg_known_size)) ? "" : ".");
      send_to_char(buf, ch);
      store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, buf);
    }
  }
}

ACMD(do_exclaim)
{
  skip_spaces(&argument);

  if (!*argument) {
    send_to_char(ch, "Yes, but WHAT do you like to exclaim?\r\n");
    return;
  }

  // If they trigger automod with this, bail out.
  if (check_for_banned_content(argument, ch))
    return;

  if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
    return;

  int language = !SKILL_IS_LANGUAGE(GET_LANGUAGE(ch)) ? SKILL_ENGLISH : GET_LANGUAGE(ch);
  if (!has_required_language_ability_for_sentence(ch, argument, language))
    return;

  for (struct char_data *tmp = (ch->in_veh ? ch->in_veh->people : ch->in_room->people);
       tmp;
       tmp = (ch->in_veh ? tmp->next_in_veh : tmp->next_in_room))
  {
    // Replicate act() in a way that lets us capture the message.
    if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM) && !IS_IGNORING(tmp, is_blocking_ic_interaction_from, ch)) {
      snprintf(buf, sizeof(buf), "$z^n exclaims in %s, \"%s%s!^n\"",
               (IS_NPC(tmp) || GET_SKILL(tmp, language) > 0) ? skills[language].name : "an unknown language",
               (PRF_FLAGGED(tmp, PRF_NOHIGHLIGHT) || PRF_FLAGGED(tmp, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
               capitalize(replace_too_long_words(tmp, ch, argument, language, GET_CHAR_COLOR_HIGHLIGHT(ch))));

      // They're a valid target, so send the message with a raw perform_act() call.
      store_message_to_history(tmp->desc, COMM_CHANNEL_SAYS, perform_act(buf, ch, NULL, NULL, tmp, FALSE));
    }
  }

  for (struct veh_data *veh = ch->in_room ? ch->in_room->vehicles : ch->in_veh->carriedvehs;
       veh;
       veh = veh->next_veh)
  {
    if (veh->rigger && veh->rigger->desc && !IS_IGNORING(veh->rigger, is_blocking_ic_interaction_from, ch)) {
      // No need to check for act vailidity, just send it
      snprintf(buf, sizeof(buf), "$z^n exclaims in %s, \"%s%s!^n\"",
               (IS_NPC(veh->rigger) || GET_SKILL(veh->rigger, language) > 0) ? skills[language].name : "an unknown language",
               (PRF_FLAGGED(veh->rigger, PRF_NOHIGHLIGHT) || PRF_FLAGGED(veh->rigger, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
               capitalize(replace_too_long_words(veh->rigger, ch, argument, language, GET_CHAR_COLOR_HIGHLIGHT(ch))));

      store_message_to_history(veh->rigger->desc, COMM_CHANNEL_SAYS, perform_act(buf, ch, NULL, NULL, veh->rigger, FALSE));
    }
  }

  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else {
    snprintf(buf, sizeof(buf), "You exclaim in %s, \"%s%s!^n\"\r\n",
             skills[language].name,
             (PRF_FLAGGED(ch, PRF_NOHIGHLIGHT) || PRF_FLAGGED(ch, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
             capitalize(argument));
    send_to_char(buf, ch);
    store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, buf);
  }
  // Speech gives you 5 minutes of grace period. Emotes give you more.
  ch->char_specials.last_social_action = LAST_SOCIAL_ACTION_REQUIREMENT_FOR_CONGREGATION_BONUS - SOCIAL_ACTION_GRACE_PERIOD_GRANTED_BY_SPEECH;
}

void perform_tell(struct char_data *ch, struct char_data *vict, char *arg)
{
  delete_doubledollar(arg);

  snprintf(buf, sizeof(buf), "^c%s%s^c tells you ^mOOCly^c, '%s^c'^n\r\n",
           GET_CHAR_NAME(ch),
           IS_SENATOR(ch) ? " (staff)" : "",
           capitalize(arg));
  send_to_char(buf, vict);
  store_message_to_history(vict->desc, COMM_CHANNEL_TELLS, buf);

  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else
  {
    snprintf(buf, sizeof(buf), "^cYou tell %s%s ^mOOCly^c, '%s^c'^n\r\n",
             GET_CHAR_NAME(vict),
             IS_SENATOR(vict) ? " (staff)" : "",
             capitalize(arg));
    send_to_char(buf, ch);
    store_message_to_history(ch->desc, COMM_CHANNEL_TELLS, buf);
  }

  if (!IS_NPC(ch))
    GET_LAST_TELL(vict) = GET_IDNUM(ch);
  else
    GET_LAST_TELL(vict) = NOBODY;
}

ACMD(do_tell)
{
  struct char_data *vict = NULL;
  SPECIAL(johnson);

  half_chop(argument, buf, buf2, sizeof(buf2));

  if (!*buf || !*buf2) {
    send_to_char("Who do you wish to tell what??\r\n", ch);
    return;
  }

  // If they trigger automod with this, bail out.
  if (check_for_banned_content(buf2, ch))
    return;

  if (!(vict = get_player_vis(ch, buf, 0))) {
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf);
    return;
  }

  if ((PLR_FLAGGED(ch, PLR_NOSHOUT) || PLR_FLAGGED(ch, PLR_TELLS_MUTED)) && !IS_SENATOR(vict)) {
    send_to_char(ch, "You can only send tells to staff.\r\n");
    return;
  }

  if (!IS_NPC(vict) && !vict->desc) {      /* linkless */
    act("$E's linkless at the moment.", FALSE, ch, 0, vict, TO_CHAR);
    return;
  }

  // Prevent chargen reaching people with tells.
  if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED) && !IS_SENATOR(vict)) {
    send_to_char("You'll need to finish the tutorial to send tells to other players.\r\n", ch);
    return;
  }

  // Prevent people reaching chargen with tells.
  if (PLR_FLAGGED(vict, PLR_NOT_YET_AUTHED) && !IS_SENATOR(ch)) {
    send_to_char("You can't send tells to them until they finish character creation.\r\n", ch);
    return;
  }

  if (IS_IGNORING(ch, is_blocking_tells_from, vict)) {
    send_to_char("You can't send tells to someone you're blocking tells from.\r\n"
                 "(Please note that disabling your block, sending a tell, and immediately re-enabling it will be considered abuse.)\r\n", ch);
    return;
  }

  // Enable blocking of tells from everyone except staff.
  if ((!access_level(ch, LVL_BUILDER) && PRF_FLAGGED(vict, PRF_NOTELL)) || IS_IGNORING(vict, is_blocking_tells_from, ch)) {
    act("$E has disabled tells.", FALSE, ch, 0, vict, TO_CHAR);
    return;
  }

  else if (PLR_FLAGGED(vict, PLR_EDITING)) {
    act("$E's editing right now, try again later.", FALSE, ch, 0, vict, TO_CHAR);
    return;
  }

  if (PRF_FLAGGED(vict, PRF_AFK)) {
    act("$E's AFK, so your message may be missed.", FALSE, ch, 0, vict, TO_CHAR);
  }

  perform_tell(ch, vict, buf2);
}

ACMD(do_reply)
{
  struct char_data *tch = NULL;

  skip_spaces(&argument);

  if (GET_LAST_TELL(ch) == NOBODY) {
    send_to_char("You have no-one to reply to!\r\n", ch);
    return;
  }

  if (!*argument) {
    send_to_char("What is your reply?\r\n", ch);
    return;
  }

  // If they trigger automod with this, bail out.
  if (check_for_banned_content(argument, ch))
    return;

  /* Find the person you're trying to reply to. */
  for (struct descriptor_data *desc = descriptor_list; desc; desc = desc->next) {
    tch = desc->original ? desc->original : desc->character;

    if (tch && !IS_NPC(tch) && GET_IDNUM(tch) == GET_LAST_TELL(ch))
      break;
  }

  if (tch == NULL || (tch && GET_IDNUM(tch) != GET_LAST_TELL(ch))) {
    send_to_char("They are no longer playing.\r\n", ch);
    return;
  }

  if ((PLR_FLAGGED(ch, PLR_NOSHOUT) || PLR_FLAGGED(ch, PLR_TELLS_MUTED)) && !IS_SENATOR(tch)) {
    send_to_char(ch, "You can only send tells to staff.\r\n");
    return;
  }

  if (PRF_FLAGGED(tch, PRF_NOTELL) || IS_IGNORING(tch, is_blocking_tells_from, ch)) {
    act("$E has disabled tells.", FALSE, ch, 0, tch, TO_CHAR);
    return;
  }

  perform_tell(ch, tch, argument);
}

ACMD(do_ask)
{
  skip_spaces(&argument);

  if (!*argument) {
    send_to_char(ch, "Yes, but WHAT do you like to ask?\r\n");
    return;
  }

  // If they trigger automod with this, bail out.
  if (check_for_banned_content(argument, ch))
    return;

  if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
    return;

  int language = !SKILL_IS_LANGUAGE(GET_LANGUAGE(ch)) ? SKILL_ENGLISH : GET_LANGUAGE(ch);
  if (!has_required_language_ability_for_sentence(ch, argument, language))
    return;

  for (struct char_data *tmp = (ch->in_veh ? ch->in_veh->people : ch->in_room->people); tmp; tmp = (ch->in_veh ? tmp->next_in_veh : tmp->next_in_room)) {
    // Replicate act() in a way that lets us capture the message.
    if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM) && !IS_IGNORING(tmp, is_blocking_ic_interaction_from, ch)) {
      snprintf(buf, sizeof(buf), "$z^n asks in %s, \"%s%s?^n\"",
               (IS_NPC(tmp) || GET_SKILL(tmp, language) > 0) ? skills[language].name : "an unknown language",
               (PRF_FLAGGED(tmp, PRF_NOHIGHLIGHT) || PRF_FLAGGED(tmp, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
               capitalize(replace_too_long_words(tmp, ch, argument, language, GET_CHAR_COLOR_HIGHLIGHT(ch))));

      // They're a valid target, so send the message with a raw perform_act() call.
      store_message_to_history(tmp->desc, COMM_CHANNEL_SAYS, perform_act(buf, ch, NULL, NULL, tmp, FALSE));
    }
  }

  for (struct veh_data *veh = ch->in_room ? ch->in_room->vehicles : ch->in_veh->carriedvehs;
       veh;
       veh = veh->next_veh)
  {
    if (veh->rigger && veh->rigger->desc) {
      // No need to check for act vailidity, just send it
      snprintf(buf, sizeof(buf), "$z^n asks in %s, \"%s%s?^n\"",
               (IS_NPC(veh->rigger) || GET_SKILL(veh->rigger, language) > 0) ? skills[language].name : "an unknown language",
               (PRF_FLAGGED(veh->rigger, PRF_NOHIGHLIGHT) || PRF_FLAGGED(veh->rigger, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
               capitalize(replace_too_long_words(veh->rigger, ch, argument, language, GET_CHAR_COLOR_HIGHLIGHT(ch))));

      store_message_to_history(veh->rigger->desc, COMM_CHANNEL_SAYS, perform_act(buf, ch, NULL, NULL, veh->rigger, FALSE));
    }
  }

  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else {
    snprintf(buf, sizeof(buf), "You ask, \"%s%s?^n\"\r\n",
             (PRF_FLAGGED(ch, PRF_NOHIGHLIGHT) || PRF_FLAGGED(ch, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
             capitalize(argument));
    send_to_char(buf, ch);
    store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, buf);
  }
  // Speech gives you 5 minutes of grace period. Emotes give you more.
  ch->char_specials.last_social_action = LAST_SOCIAL_ACTION_REQUIREMENT_FOR_CONGREGATION_BONUS - SOCIAL_ACTION_GRACE_PERIOD_GRANTED_BY_SPEECH;
}

ACMD(do_spec_comm)
{
  struct veh_data *veh = NULL;
  struct char_data *vict;
  const char *action_sing, *action_plur, *action_others;
  if (subcmd == SCMD_WHISPER) {
    action_sing = "whisper to";
    action_plur = "whispers to";
    action_others = "$z whispers something to $N.";
  } else {
    action_sing = "ask";
    action_plur = "asks";
    action_others = "$z asks $N something.";
  }

  // If they trigger automod with this, bail out.
  if (check_for_banned_content(argument, ch))
    return;

  half_chop(argument, buf, buf2, sizeof(buf2));

  if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
    return;

  if (!*buf || !*buf2) {
    send_to_char(ch, "Whom do you want to %s... and what??\r\n", action_sing);
    return;
  }

  int language = !SKILL_IS_LANGUAGE(GET_LANGUAGE(ch)) ? SKILL_ENGLISH : GET_LANGUAGE(ch);
  if (!has_required_language_ability_for_sentence(ch, buf2, language))
    return;

  if (ch->in_veh) {
    if (ch->in_veh->cspeed > SPEED_IDLE) {
      send_to_char("You're going to hit your head on a lamppost if you try that.\r\n", ch);
      return;
    }

    ch->in_room = ch->in_veh->in_room;
    struct veh_data *last_veh = ch->in_veh;
    ch->in_veh = ch->in_veh->in_veh;

    if (!(vict = get_char_room_vis(ch, buf))) {
      send_to_char("You don't see them here.\r\n", ch);
      ch->in_room = NULL;
      ch->in_veh = last_veh;
      return;
    }

    if (vict == ch) {
      send_to_char(ch, "Why would you want to %s yourself?\r\n", action_sing);
      ch->in_room = NULL;
      ch->in_veh = last_veh;
      return;
    }

    // Message the whisper-er.
    snprintf(buf, sizeof(buf), "You lean out towards $N^n and say in %s, \"%s%s%s\"",
             skills[language].name,
             (PRF_FLAGGED(ch, PRF_NOHIGHLIGHT) || PRF_FLAGGED(ch, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
             capitalize(buf2),
             ispunct(get_final_character_from_string(buf2)) ? "" : ".");

    store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, act(buf, FALSE, ch, NULL, vict, TO_CHAR));

    if (!IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
      // Message the whisper-ee.
      snprintf(buf, sizeof(buf), "From within %s^n, $z^n says to you in %s, \"%s%s%s^n\"\r\n",
              GET_VEH_NAME(last_veh),
              (IS_NPC(vict) || GET_SKILL(vict, language) > 0) ? skills[language].name : "an unknown language",
              (PRF_FLAGGED(vict, PRF_NOHIGHLIGHT) || PRF_FLAGGED(vict, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
              capitalize(replace_too_long_words(vict, ch, buf2, language, GET_CHAR_COLOR_HIGHLIGHT(ch))),
              ispunct(get_final_character_from_string(buf2)) ? "" : ".");

      store_message_to_history(vict->desc, COMM_CHANNEL_SAYS, act(buf, FALSE, ch, NULL, vict, TO_VICT));
    }


    ch->in_room = NULL;
    ch->in_veh = last_veh;
    return;
  }

  if (!(vict = get_char_room_vis(ch, buf))
      && !((veh = get_veh_list(buf, ch->in_room->vehicles, ch)) && subcmd == SCMD_WHISPER)) {
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", capitalize(buf));
    return;
  }

  if (vict == ch) {
    send_to_char("You can't get your mouth close enough to your ear...\r\n", ch);
    return;
  }

  if (IS_ASTRAL(ch) && vict && !SEES_ASTRAL(vict)) {
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf);
    return;
  }

  if (veh) {
    if (veh->cspeed > SPEED_IDLE) {
      send_to_char(ch, "It's moving too fast for you to lean in.\r\n");
      return;
    }

    snprintf(buf, sizeof(buf), "You lean into %s^n and say in %s, \"%s%s%s\"",
             GET_VEH_NAME(veh),
             skills[language].name,
             (PRF_FLAGGED(ch, PRF_NOHIGHLIGHT) || PRF_FLAGGED(ch, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
             capitalize(buf2),
             ispunct(get_final_character_from_string(buf2)) ? "" : ".");
    send_to_char(buf, ch);
    store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, buf);

    for (vict = veh->people; vict; vict = vict->next_in_veh) {
      if (!IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
        snprintf(buf, sizeof(buf), "From outside, $z^n says into the vehicle in %s, \"%s%s%s^n\"\r\n",
                 (IS_NPC(vict) || GET_SKILL(vict, language) > 0) ? skills[language].name : "an unknown language",
                 (PRF_FLAGGED(vict, PRF_NOHIGHLIGHT) || PRF_FLAGGED(vict, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
                 capitalize(replace_too_long_words(vict, ch, buf2, language, GET_CHAR_COLOR_HIGHLIGHT(ch))),
                 ispunct(get_final_character_from_string(buf2)) ? "" : ".");

        store_message_to_history(vict->desc, COMM_CHANNEL_SAYS, act(buf, FALSE, ch, NULL, vict, TO_VICT));
      }
    }
    return;
  }

  if (!IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
    snprintf(buf, sizeof(buf), "$z^n %s you in %s, \"%s%s%s^n\"\r\n",
             action_plur,
             (IS_NPC(vict) || GET_SKILL(vict, language) > 0) ? skills[language].name : "an unknown language",
             (PRF_FLAGGED(vict, PRF_NOHIGHLIGHT) || PRF_FLAGGED(vict, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
             capitalize(replace_too_long_words(vict, ch, buf2, language, GET_CHAR_COLOR_HIGHLIGHT(ch))),
             ispunct(get_final_character_from_string(buf2)) ? "" : ".");

    store_message_to_history(vict->desc, COMM_CHANNEL_SAYS, act(buf, FALSE, ch, 0, vict, TO_VICT));
  }

  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else {
    snprintf(buf, sizeof(buf), "You %s $N^n in %s, \"%s%s%s^n\"",
             action_sing,
             skills[language].name,
             (PRF_FLAGGED(ch, PRF_NOHIGHLIGHT) || PRF_FLAGGED(ch, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
             capitalize(buf2),
             (subcmd == SCMD_WHISPER) ? (ispunct(get_final_character_from_string(buf2)) ? "" : ".") : "?");
    store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, act(buf, FALSE, ch, 0, vict, TO_CHAR));
  }
  // Speech gives you 5 minutes of grace period. Emotes give you more.
  ch->char_specials.last_social_action = LAST_SOCIAL_ACTION_REQUIREMENT_FOR_CONGREGATION_BONUS - SOCIAL_ACTION_GRACE_PERIOD_GRANTED_BY_SPEECH;

  // TODO: Should this be stored to message history? It's super nondescript.
  // TODO: How can we prevent blocking people from seeing this?
  act(action_others, FALSE, ch, 0, vict, TO_NOTVICT);
}

ACMD(do_page)
{
  struct char_data *vict;

  // If they trigger automod with this, bail out.
  if (check_for_banned_content(argument, ch))
    return;

  half_chop(argument, arg, buf2, sizeof(buf2));

  if (IS_NPC(ch))
    send_to_char("NPCs can't page.. go away.\r\n", ch);
  else if (!*arg)
    send_to_char("Whom do you wish to page?\r\n", ch);
  else {
    snprintf(buf, sizeof(buf), "\007\007Page from *%s*: %s", GET_CHAR_NAME(ch), buf2);
    if ((vict = get_char_vis(ch, arg)) != NULL) {
      if (vict == ch) {
        send_to_char("What's the point of that?\r\n", ch);
        return;
      }
      store_message_to_history(vict->desc, COMM_CHANNEL_PAGES, act(buf, FALSE, ch, 0, vict, TO_VICT));
      if (PRF_FLAGGED(ch, PRF_NOREPEAT))
        send_to_char(OK, ch);
      else
        store_message_to_history(ch->desc, COMM_CHANNEL_PAGES, act(buf, FALSE, ch, 0, vict, TO_CHAR));
      return;
    } else
      send_to_char("There is no such person in the game!\r\n", ch);
  }
}

ACMD(do_radio)
{
  struct obj_data *obj, *radio = NULL;
  char one[MAX_INPUT_LENGTH], two[MAX_INPUT_LENGTH];
  int i;
  bool cyberware = FALSE, vehicle = FALSE;

  if (ch->in_veh && (radio = GET_MOD(ch->in_veh, MOD_RADIO)))
    vehicle = 1;

  for (obj = ch->carrying; !radio && obj; obj = obj->next_content)
    if (GET_OBJ_TYPE(obj) == ITEM_RADIO)
      radio = obj;

  for (i = 0; !radio && i < NUM_WEARS; i++)
    if (GET_EQ(ch, i)) {
      if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_RADIO)
        radio = GET_EQ(ch, i);
      else if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_WORN && GET_EQ(ch, i)->contains)
        for (struct obj_data *obj2 = GET_EQ(ch, i)->contains; obj2; obj2 = obj2->next_content)
          if (GET_OBJ_TYPE(obj2) == ITEM_RADIO)
            radio = obj2;
    }
  for (obj = ch->cyberware; !radio && obj; obj = obj->next_content)
    if (GET_OBJ_VAL(obj, 0) == CYB_RADIO) {
      radio = obj;
      cyberware = 1;
    }

  if (ch->in_room)
    for (obj = ch->in_room->contents; obj && !radio; obj = obj->next_content)
      if (GET_OBJ_TYPE(obj) == ITEM_RADIO)
        radio = obj;

  if (!radio) {
    send_to_char("You don't have a radio.\r\n", ch);
    return;
  }
  any_one_arg(any_one_arg(argument, one), two);

  int *freq;
  if (cyberware) {
    freq = &GET_CYBERWARE_RADIO_FREQ(radio);
  } else if (vehicle) {
    freq = &GET_VEHICLE_MOD_RADIO_FREQ(radio);
  } else {
    freq = &GET_RADIO_CENTERED_FREQUENCY(radio);
  }

#ifdef ENABLE_RADIO_CRYPT
  int *crypt_rating;
  int max_crypt = 0;

  if (cyberware) {
    crypt_rating = &GET_CYBERWARE_RADIO_CRYPT(radio);
    max_crypt = GET_CYBERWARE_RADIO_MAX_CRYPT(radio);
  } else if (vehicle) {
    crypt_rating = &GET_VEHICLE_MOD_RADIO_CRYPT(radio);
    max_crypt = GET_VEHICLE_MOD_RADIO_MAX_CRYPT(radio);
  } else {
    crypt_rating = &GET_RADIO_CURRENT_CRYPT(radio);
    max_crypt = GET_RADIO_MAX_CRYPT(radio);
  }
#endif

  if (!*one) {
    act("$p:", FALSE, ch, radio, 0, TO_CHAR);
    if (*freq == -1)
      send_to_char("  Mode: scan\r\n", ch);
    else if (!*freq)
      send_to_char("  Mode: off\r\n", ch);
    else
      send_to_char(ch, "  Mode: center @ %d MHz\r\n", *freq);

#ifdef ENABLE_RADIO_CRYPT
    if (*crypt_rating)
      send_to_char(ch, "  Crypt (max %d): on (level %d)\r\n", max_crypt, *crypt_rating);
    else
      send_to_char(ch, "  Crypt (max %d): off\r\n", max_crypt);
    return;
#endif
  } else if (!str_cmp(one, "off")) {
    act("You turn $p off.", FALSE, ch, radio, 0, TO_CHAR);
    *freq = 0;
  } else if (!str_cmp(one, "scan")) {
    act("You set $p to scanning mode.", FALSE, ch, radio, 0, TO_CHAR);
    *freq = -1;
    WAIT_STATE(ch, 16); /* Takes time to switch it */
  } else if (!str_cmp(one, "center")) {
    i = atoi(two);
    if (i > MAX_RADIO_FREQUENCY) {
      snprintf(buf, sizeof(buf), "$p cannot center a frequency higher than %d MHz.", MAX_RADIO_FREQUENCY);
      act(buf, FALSE, ch, radio, 0, TO_CHAR);
    }
    else if (i < MIN_RADIO_FREQUENCY) {
      snprintf(buf, sizeof(buf), "$p cannot center a frequency lower than %d MHz.", MIN_RADIO_FREQUENCY);
      act(buf, FALSE, ch, radio, 0, TO_CHAR);
    }
    else {
      snprintf(buf, sizeof(buf), "$p is now centered at %d MHz.", i);
      act(buf, FALSE, ch, radio, 0, TO_CHAR);
      *freq = i;
      WAIT_STATE(ch, 16); /* Takes time to adjust */
    }
  } else if (!str_cmp(one, "crypt")) {
#ifdef ENABLE_RADIO_CRYPT
    if ((i = atoi(two))) {
      if (i > max_crypt) {
        snprintf(buf, sizeof(buf), "$p's max crypt rating is %d.", max_crypt);
        act(buf, FALSE, ch, radio, 0, TO_CHAR);
      }
      else if (i < 0) {
        send_to_char(ch, "A negative crypt rating?\r\n");
        return;
      }
      else {
        send_to_char(ch, "Crypt mode enabled at rating %d.\r\n", i);
        *crypt_rating = i;
      }
    }
    else {
      if (!*crypt_rating)
        act("$p's crypt mode is already disabled.", FALSE, ch, radio, 0, TO_CHAR);
      else {
        send_to_char("Crypt mode disabled.\r\n", ch);
        *crypt_rating = 0;
      }
    }
#else
    send_to_char(ch, "Your broadcasts are encrypted by default with secure Shadowlands protocols. There's no need to configure them anymore.");
#endif // radio crypt
  } else if (!str_cmp(one, "mode")) {
    if (*freq == -1)
      send_to_char(ch, "Your radio is currently scanning all frequencies. You can change the mode with ^WRADIO CENTER <frequency>, or turn it off with ^WRADIO OFF^n^n.\r\n");
    else if (!*freq)
      send_to_char(ch, "Your radio is currently off. You can turn it on with ^WRADIO CENTER <frequency>^n or ^WRADIO SCAN^n.\r\n");
    else
      send_to_char(ch, "Your radio is currently centered at %d MHz. You can change the mode with ^WRADIO SCAN^n, or turn it off with ^WRADIO OFF^n.\r\n",
                   *freq);
  } else {
#ifdef ENABLE_RADIO_CRYPT
    send_to_char("Valid commands are ^WRADIO OFF^n, ^WRADIO SCAN^n, ^WRADIO CENTER <frequency>^n, ^WRADIO CRYPT <level>^n, and ^WRADIO MODE^n. See ^WHELP RADIO^n for more.\r\n", ch);
#else
    send_to_char("Valid commands are ^WRADIO OFF^n, ^WRADIO SCAN^n, ^WRADIO CENTER <frequency>^n, and ^WRADIO MODE^n. See ^WHELP RADIO^n for more.\r\n", ch);
#endif
  }
}

struct obj_data *find_radio(struct char_data *ch, bool *is_cyberware, bool *is_vehicular, bool must_be_on=FALSE) {
  struct obj_data *obj;

  if (!ch)
    return NULL;

  *is_cyberware = FALSE;
  *is_vehicular = FALSE;

  // If you've got a vehicle radio installed, we use that.
  if (ch->in_veh && GET_MOD(ch->in_veh, MOD_RADIO)) {
    *is_vehicular = TRUE;
    return GET_MOD(ch->in_veh, MOD_RADIO);
  }

  // Check your inventory.
  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (GET_OBJ_TYPE(obj) == ITEM_RADIO && (must_be_on ? GET_OBJ_VAL(obj, 0) != 0 : TRUE))
      return obj;

  // Check your gear.
  for (int i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i)) {
      if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_RADIO && (must_be_on ? GET_OBJ_VAL(GET_EQ(ch, i), 0) != 0 : TRUE)) {
        return GET_EQ(ch, i);
      } else if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_WORN && GET_EQ(ch, i)->contains) {
        for (struct obj_data *obj = GET_EQ(ch, i)->contains; obj; obj = obj->next_content)
          if (GET_OBJ_TYPE(obj) == ITEM_RADIO && (must_be_on ? GET_OBJ_VAL(obj, 0) != 0 : TRUE))
            return obj;
      }
    }

  // Check your cyberware.
  for (obj = ch->cyberware; obj; obj = obj->next_content)
    if (GET_CYBERWARE_TYPE(obj) == CYB_RADIO && (must_be_on ? GET_CYBERWARE_FLAGS(obj) != 0 : TRUE)) {
      *is_cyberware = TRUE;
      return obj;
    }

  // Check for radios in the room.
  if (ch->in_room)
    for (obj = ch->in_room->contents; obj; obj = obj->next_content)
      if (GET_OBJ_TYPE(obj) == ITEM_RADIO && (must_be_on ? GET_RADIO_CENTERED_FREQUENCY(obj) != 0 : TRUE))
        return obj;

  return NULL;
}

ACMD(do_broadcast)
{
  // If they trigger automod with this, bail out.
  if (check_for_banned_content(argument, ch))
    return;

  // No color highlights over the radio. It's already colored.

  struct obj_data *radio = NULL;
  struct descriptor_data *d;
  int frequency, crypt_lvl, receiver_freq, receiver_freq_range, decrypt;
  char voice[16] = "$v";
  bool cyberware = FALSE, vehicle = FALSE;

  if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
    send_to_char("You must be Authorized to do that. Until then, you can use the ^WQUESTION^n channel if you need help.\r\n", ch);
    return;
  }

  if (PLR_FLAGGED(ch, PLR_RADIO_MUTED)) {
    send_to_char("You're not able to broadcast at this time.\r\n", ch);
    return;
  }

  if (IS_ASTRAL(ch)) {
    send_to_char("You can't manipulate electronics from the astral plane.\r\n", ch);
    return;
  }

  radio = find_radio(ch, &cyberware, &vehicle);

  if (IS_NPC(ch) || IS_SENATOR(ch)) {
    argument = any_one_arg(argument, arg);
    if (!str_cmp(arg,"all") && !IS_NPC(ch))
      frequency = -1;
    else {
      frequency = atoi(arg);
      if (frequency < MIN_RADIO_FREQUENCY || frequency > MAX_RADIO_FREQUENCY) {
        send_to_char(ch, "Frequency must range between %d and %d.\r\n", MIN_RADIO_FREQUENCY, MAX_RADIO_FREQUENCY);
        return;
      }
    }
    crypt_lvl = 0;
  } else if (!radio) {
    send_to_char("You have to have a radio to do that!\r\n", ch);
    return;
  } else {
    // Player character with radio
    if (cyberware) {
      frequency = GET_CYBERWARE_RADIO_FREQ(radio);
      crypt_lvl = GET_CYBERWARE_RADIO_CRYPT(radio);
    } else if (vehicle) {
      frequency = GET_VEHICLE_MOD_RADIO_FREQ(radio);
      crypt_lvl = GET_VEHICLE_MOD_RADIO_CRYPT(radio);
    } else {
      frequency = GET_RADIO_CENTERED_FREQUENCY(radio);
      crypt_lvl = GET_RADIO_CURRENT_CRYPT(radio);
    }
    
    if (!frequency) {
      act("$p must be on in order to broadcast.", FALSE, ch, radio, 0, TO_CHAR);
      return;
    }
    if (frequency == -1) {
      act("$p can't broadcast while scanning.", FALSE, ch, radio, 0, TO_CHAR);
      return;
    }
  }

  if (PLR_FLAGGED(ch, PLR_NOSHOUT)) {
    send_to_char("You aren't allowed to broadcast!\r\n", ch);
    return;
  }

  skip_spaces(&argument);

  if (!*argument) {
    send_to_char("What do you want to broadcast?\r\n", ch);
    return;
  }

  if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
    return;

#ifndef ENABLE_RADIO_CRYPT
  // Disable crypt.
  crypt_lvl = 0;
#endif

  // Forbid usage of common smilies.
  FAILURE_CASE(str_str(argument, ":)") || str_str(argument, ":D") || str_str(argument, ":("), "The radio is for in-character voice comms. Please refrain from using smilies etc.");

  int language = !SKILL_IS_LANGUAGE(GET_LANGUAGE(ch)) ? SKILL_ENGLISH : GET_LANGUAGE(ch);
  if (!has_required_language_ability_for_sentence(ch, argument, language))
    return;

  char untouched_message[MAX_STRING_LENGTH], capitalized_and_punctuated[MAX_STRING_LENGTH], color_stripped_arg[MAX_STRING_LENGTH];
  strlcpy(color_stripped_arg, get_string_after_color_code_removal(argument, ch), sizeof(color_stripped_arg));
  snprintf(capitalized_and_punctuated, sizeof(capitalized_and_punctuated), "%s%s", capitalize(color_stripped_arg), ispunct(get_final_character_from_string(argument)) ? "" : ".");
  if (frequency > 0) {
    if (crypt_lvl)
      snprintf(untouched_message, sizeof(untouched_message), "^y\\%s^y/[%d MHz, %s](CRYPTO-%d): %s^N", voice, frequency, skills[language].name, crypt_lvl, capitalized_and_punctuated);
    else
      snprintf(untouched_message, sizeof(untouched_message), "^y\\%s^y/[%d MHz, %s]: %s^N", voice, frequency, skills[language].name, capitalized_and_punctuated);
  } else {
    if (crypt_lvl)
      snprintf(untouched_message, sizeof(untouched_message), "^y\\%s^y/[All Frequencies, %s](CRYPTO-%d): %s^N", voice, skills[language].name, crypt_lvl, capitalized_and_punctuated);
    else
      snprintf(untouched_message, sizeof(untouched_message), "^y\\%s^y/[All Frequencies, %s]: %s^N", voice, skills[language].name, capitalized_and_punctuated);
  }

  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else
    store_message_to_history(ch->desc, COMM_CHANNEL_RADIO, act(untouched_message, FALSE, ch, 0, 0, TO_CHAR));

  char radlog_string[MAX_STRING_LENGTH];
  snprintf(radlog_string, sizeof(radlog_string), "%s (%d MHz, crypt %d, in %s): %s^N",
           GET_CHAR_NAME(ch),
           frequency,
           crypt_lvl,
           skills[language].name,
           capitalized_and_punctuated
          );
  mudlog(radlog_string, NULL, LOG_RADLOG, TRUE);

  if (!ROOM_FLAGGED(get_ch_in_room(ch), ROOM_SOUNDPROOF)) {
    for (d = descriptor_list; d; d = d->next) {
      if (d != ch->desc && d->character && 
          (IS_VALID_STATE_TO_RECEIVE_COMMS(d->connected) && !(d->connected != CON_PLAYING && PRF_FLAGGED(d->character, PRF_MENUGAG)))
          && !PLR_FLAGGED(d->character, PLR_MATRIX)
          && !IS_PROJECT(d->character) &&
          !ROOM_FLAGGED(get_ch_in_room(d->character), ROOM_SOUNDPROOF) &&
          !ROOM_FLAGGED(get_ch_in_room(d->character), ROOM_BFS_MARK))
      {
        if (!IS_NPC(d->character) && (!access_level(d->character, LVL_FIXER) || PRF_FLAGGED(d->character, PRF_SUPPRESS_STAFF_RADIO))) {
          if (!(radio = find_radio(d->character, &cyberware, &vehicle, TRUE)))
            continue;

          /*
          if (CAN_WEAR(radio, ITEM_WEAR_EAR) || cyberware || vehicle)
            to_room = 0;
          else
            to_room = 1;
          */
          if (cyberware) {
            receiver_freq = GET_CYBERWARE_RADIO_FREQ(radio);
            receiver_freq_range = GET_CYBERWARE_RATING(radio);
            decrypt = GET_CYBERWARE_RADIO_MAX_CRYPT(radio);
          } else if (vehicle) {
            receiver_freq = GET_VEHICLE_MOD_RADIO_FREQ(radio);
            receiver_freq_range = GET_VEHICLE_MOD_RATING(radio);
            decrypt = GET_VEHICLE_MOD_RADIO_MAX_CRYPT(radio);
          } else {
            receiver_freq = GET_RADIO_CENTERED_FREQUENCY(radio);
            receiver_freq_range = GET_RADIO_FREQ_RANGE(radio);
            decrypt = GET_RADIO_MAX_CRYPT(radio);
          }

          if (receiver_freq == 0 
              || ((receiver_freq != -1 && frequency != -1) && !(frequency >= (receiver_freq - receiver_freq_range) && frequency <= (receiver_freq + receiver_freq_range))))
            continue;

          // Skip message-send for anyone who's blocked you.
          if (IS_IGNORING(d->character, is_blocking_radios_from, ch))
            continue;

          char message[MAX_STRING_LENGTH];
          char radio_string[1000];

          // If char's decrypt is insufficient, just give them the static.
          if (decrypt < crypt_lvl) {
            if (frequency <= 0) {
              snprintf(message, sizeof(message), "^y\\Garbled Static^y/[All Frequencies, Unknown](CRYPTO-%d): ***ENCRYPTED DATA***", crypt_lvl);
            } else {
              snprintf(message, sizeof(message), "^y\\Garbled Static^y/[%d MHz, Unknown](CRYPTO-%d): ***ENCRYPTED DATA***", frequency, crypt_lvl);
            }
            store_message_to_history(d, COMM_CHANNEL_RADIO, act(message, FALSE, ch, 0, d->character, TO_VICT));
            continue;
          }

          // Copy in the message body, mangling it as needed for language skill issues.
          snprintf(message, sizeof(message), "%s%s",
                   capitalize(replace_too_long_words(d->character, ch, color_stripped_arg, language, "^y", PRF_FLAGGED(d->character, PRF_SUPPRESS_STAFF_RADIO))),
                   ispunct(get_final_character_from_string(argument)) ? "" : ".");

          // Add in interference if there is any.
          bool bad_reception;
          if ((bad_reception = (ROOM_FLAGGED(get_ch_in_room(ch), ROOM_NO_RADIO) || ROOM_FLAGGED(get_ch_in_room(d->character), ROOM_NO_RADIO)))) {
            // Compose the voice portion of the string.
            strlcpy(radio_string, "^y\\Unintelligible Voice^y/", sizeof(radio_string));

            // Starting at end of buf, work backwards and fuzz out the message.
            for (int len = strlen(message) - 1; len >= 0; len--) {
              switch (number(0, 2)) {
                // case 0 does nothing here- leave the letter intact.
                case 1:
                  message[len] = '.';
                  break;
                case 2:
                  message[len] = ' ';
                  break;
              }
            }
          } else {
            snprintf(radio_string, sizeof(radio_string), "^y\\%s^y/", voice);
          }

          // Append frequency info to radio string.
          if (frequency <= 0) {
            strlcat(radio_string, "[All Frequencies, ", sizeof(radio_string));
          } else {
            snprintf(ENDOF(radio_string), sizeof(radio_string) - strlen(radio_string), "[%d MHz, ", frequency);
          }

          // Append language info to radio string.
          if (GET_SKILL(d->character, language) > 0 && (language == SKILL_ENGLISH || !PRF_FLAGGED(d->character, PRF_SUPPRESS_STAFF_RADIO))) {
            snprintf(ENDOF(radio_string), sizeof(radio_string) - strlen(radio_string), "%s]", skills[language].name);
          } else {
            strlcat(radio_string, "Unknown]", sizeof(radio_string));
          }

          // Append crypt info to radio string (if any).
          if (crypt_lvl) {
            snprintf(ENDOF(radio_string), sizeof(radio_string) - strlen(radio_string), "(CRYPTO-%d)", crypt_lvl);
          }

          // If we have bad reception, add the static modifier.
          if (bad_reception)
            strlcat(radio_string, ": *static* ", sizeof(radio_string));
          else
            strlcat(radio_string, ": ", sizeof(radio_string));

          // Finally(!), add the message itself.
          strlcat(radio_string, message, sizeof(radio_string));

          // Punctuate it and seal off any color code leakage.
          if (!ispunct(get_final_character_from_string(radio_string)))
            strlcat(radio_string, ".^n", sizeof(radio_string));
          else
            strlcat(radio_string, "^n", sizeof(radio_string));

          store_message_to_history(d, COMM_CHANNEL_RADIO, act(radio_string, FALSE, ch, 0, d->character, TO_VICT));

        } else if (access_level(d->character, LVL_FIXER) && !PRF_FLAGGED(d->character, PRF_NORADIO)) {
          store_message_to_history(d, COMM_CHANNEL_RADIO, act(untouched_message, FALSE, ch, 0, d->character, TO_VICT));
        }
      }
    }
  }

  for (d = descriptor_list; d; d = d->next)
    if (IS_VALID_STATE_TO_RECEIVE_COMMS(d->connected) &&
        !(d->connected != CON_PLAYING && PRF_FLAGGED(d->character, PRF_MENUGAG)) &&
        d->character &&
        ROOM_FLAGGED(get_ch_in_room(d->character), ROOM_BFS_MARK))
      ROOM_FLAGS(get_ch_in_room(d->character)).RemoveBit(ROOM_BFS_MARK);
}

/**********************************************************************
 * generalized communication func, originally by Fred C. Merkel (Torg) *
  *********************************************************************/

extern int _NO_OOC_;

ACMD(do_gen_comm)
{
  // If they trigger automod with this, bail out.
  if (check_for_banned_content(argument, ch))
    return;

  // No color highlights over these channels. They're either OOC or already colored.
  struct veh_data *veh;
  struct descriptor_data *i;
  struct descriptor_data *d;
  int channel = 0;

  static int channels[] = {
                            PRF_DEAF,
                            PRF_NOQUESTIONS,
                            PRF_NOOOC,
                            PRF_NORPE,
                            PRF_NOHIRED
                          };

  /*
   * com_msgs: [0] Message if you can't perform the action because of noshout
   *           [1] name of the action
   *           [2] message if you're not on the channel
   *           [3] a color string.
   */
  static const char *com_msgs[][7] = {
                                       {"You cannot shout!!\r\n",
                                        "shout",
                                        "Turn off your noshout flag first!\r\n",
                                        "^Y",
                                        B_YELLOW
                                       },

                                       {"You can't use the question channel!\r\n",
                                        "question",
                                        "You've turned that channel off!\r\n",
                                        "^G",
                                        B_GREEN,
                                        "question",
                                        "Question"
                                       },

                                       {"You can't use the OOC channel!\r\n",
                                        "ooc",
                                        "You have the OOC channel turned off.\n\r",
                                        "^n",
                                        KNUL,
                                        "OOC",
                                        "OOC"
                                       },

                                       {"You can't use the RPE channel!\r\n",
                                        "rpe",
                                        "You have the RPE channel turned off.\r\n",
                                        B_RED
                                       },

                                       {"You can't use the hired channel!\r\n",
                                        "hired",
                                        "You have the hired channel turned off.\r\n",
                                        B_YELLOW
                                       }
                                     };

  /* to keep pets, etc from being ordered to shout */
  if (!ch->desc && !MOB_FLAGGED(ch, MOB_SPEC))
    return;

  if(PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED) && subcmd != SCMD_QUESTION) {
    send_to_char(ch, "You must be Authorized to use that command. Until then, you can use the ^WQUESTION^n channel if you need help.\r\n");
    return;
  }

  skip_spaces(&argument);

  /* make sure that there is something there to say! */
  if (!*argument) {
    send_to_char(ch, "Yes, %s, fine, %s we must, but WHAT???\r\n", com_msgs[subcmd][1], com_msgs[subcmd][1]);
    return;
  }

  // off/on toggles
  if (subcmd == SCMD_QUESTION || subcmd == SCMD_OOC) {
    if (!str_cmp(argument, "off") || !str_cmp(argument, "disable") || !str_cmp(argument, "mute")) {
      send_to_char(ch, "OK, your %s channel is now disabled. You can re-enable it with ^W%s ON^n.\r\n", com_msgs[subcmd][5], com_msgs[subcmd][6]);
      PRF_FLAGS(ch).SetBit(channels[subcmd]);
      return;
    }
    else if (!str_cmp(argument, "on") || !str_cmp(argument, "enable") || !str_cmp(argument, "unmute")) {
      send_to_char(ch, "OK, your %s channel is now enabled. You can disable it again with ^W%s OFF^n.\r\n", com_msgs[subcmd][5], com_msgs[subcmd][6]);
      PRF_FLAGS(ch).RemoveBit(channels[subcmd]);
      return;
    }
  }

  if (subcmd == SCMD_SHOUT) {
    if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
      return;

    if (PLR_FLAGGED(ch, PLR_NOSHOUT)) {
      send_to_char(com_msgs[subcmd][0], ch);
      return;
    }

    if (ROOM_FLAGGED(get_ch_in_room(ch), ROOM_SOUNDPROOF)) {
      send_to_char("The walls seem to absorb your words.\r\n", ch);
      return;
    }
  }

  if (IS_ASTRAL(ch)) {
    send_to_char(ch, "Astral beings cannot %s.\r\n", com_msgs[subcmd][1]);
    return;
  }

  if ( _NO_OOC_ && subcmd == SCMD_OOC ) {
    send_to_char("This command has been disabled.\r\n",ch);
    return;
  }

  if ((subcmd == SCMD_OOC && PLR_FLAGGED(ch, PLR_NOOOC))
       || (subcmd == SCMD_RPETALK && !(PLR_FLAGGED(ch, PLR_RPE) || IS_SENATOR(ch)))
       || (subcmd == SCMD_HIREDTALK && !(PRF_FLAGGED(ch, PRF_HIRED) || IS_SENATOR(ch)))) {
    send_to_char(com_msgs[subcmd][0], ch);
    return;
  }

  /* make sure the char is on the channel */
  if (PRF_FLAGGED(ch, channels[subcmd])) {
    send_to_char(com_msgs[subcmd][2], ch);
    return;
  }

  // <channel> history prompts you to use the HISTORY <channel> command
  if (!str_cmp(argument, "history")) {
    send_to_char("Please use the ^WHISTORY^n command to see a list of channels you can pull up info on.\r\n", ch);
    return;
  }

  if (subcmd == SCMD_QUESTION) {
    // Remove the doubled dollar signs.
    delete_doubledollar(argument);

    if (IS_NPC(ch)) {
      send_to_char("NPCs can't use the Q&A channel.\r\n", ch);
      return;
    }
    else if (PLR_FLAGGED(ch, PLR_QUESTIONS_MUTED)) {
      send_to_char("You can't talk on that channel.\r\n", ch);
      return;
    }
    /* Anyone can send to the newbie channel now.
    else if (!IS_SENATOR(ch) && !PLR_FLAGGED(ch, PLR_NEWBIE) && !PRF_FLAGGED(ch, PRF_NEWBIEHELPER)) {
      send_to_char("You are too experienced to use the newbie channel.\r\n", ch);
      return;
    }
    */

    // Add a nudge to not just say 'help x' on the newbie channel.
    if (strlen(argument) > 6) {
      if (tolower(argument[0]) == 'h' && tolower(argument[1]) == 'e' && tolower(argument[2]) == 'l' && tolower(argument[3]) == 'p' && tolower(argument[4]) == ' ') {
        bool is_help_x = TRUE;
        for (size_t i = 5; i < strlen(argument) && is_help_x; i++) {
          is_help_x = !isspace(argument[i]);
        }
        if (is_help_x) {
          send_to_char("^c(Gentle reminder: It's not a great newbie experience to be told 'help x' with no context! Please explain a little more if you can.)^n\r\n", ch);
        }
      }
    }
  }

  // Returning command to handle shout.
  if (subcmd == SCMD_SHOUT) {
    // Keep the doubled dollar signs.
    struct room_data *was_in = NULL;
    struct char_data *tmp;

    int language = !SKILL_IS_LANGUAGE(GET_LANGUAGE(ch)) ? SKILL_ENGLISH : GET_LANGUAGE(ch);
    if (!has_required_language_ability_for_sentence(ch, argument, language))
      return;

    // Same room shout.
    for (tmp = ch->in_veh ? ch->in_veh->people : ch->in_room->people; tmp; tmp = (ch->in_veh ? tmp->next_in_veh : tmp->next_in_room)) {
      if (tmp != ch && !IS_IGNORING(tmp, is_blocking_ic_interaction_from, ch)) {
        snprintf(buf, sizeof(buf), "%s$z%s shouts in %s, \"%s%s%s\"^n",
                 com_msgs[subcmd][3],
                 com_msgs[subcmd][3],
                 (IS_NPC(tmp) || GET_SKILL(tmp, language) > 0) ? skills[language].name : "an unknown language",
                 capitalize(replace_too_long_words(tmp, ch, argument, language, com_msgs[subcmd][3])),
                 ispunct(get_final_character_from_string(argument)) ? "" : "!",
                 com_msgs[subcmd][3]);

        // Note that this line invokes act().
        store_message_to_history(tmp->desc, COMM_CHANNEL_SHOUTS, act(buf, FALSE, ch, NULL, tmp, TO_VICT));
      }
    }

    snprintf(buf1, sizeof(buf1),  "%sYou shout in %s, \"%s%s%s\"^n",
            com_msgs[subcmd][3],
            skills[language].name,
            capitalize(argument),
            ispunct(get_final_character_from_string(argument)) ? "" : "!",
            com_msgs[subcmd][3]);

    if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
      store_message_to_history(ch->desc, COMM_CHANNEL_SHOUTS, buf1);
      send_to_char(OK, ch);
    } else {
      // Note that this line invokes act().
      store_message_to_history(ch->desc, COMM_CHANNEL_SHOUTS, act(buf1, FALSE, ch, 0, 0, TO_CHAR));
    }
    // Speech gives you 5 minutes of grace period. Emotes give you more.
    ch->char_specials.last_social_action = LAST_SOCIAL_ACTION_REQUIREMENT_FOR_CONGREGATION_BONUS - SOCIAL_ACTION_GRACE_PERIOD_GRANTED_BY_SPEECH;

    was_in = ch->in_room;
    if (ch->in_veh) {
      ch->in_room = get_ch_in_room(ch);
      for (tmp = ch->in_room->people; tmp; tmp = tmp->next_in_room) {
        if (!IS_IGNORING(tmp, is_blocking_ic_interaction_from, ch)) {
          snprintf(buf1, sizeof(buf1), "%sFrom inside %s^n, $z^n shouts in %s, \"%s%s%s\"^n",
                   com_msgs[subcmd][3],
                   GET_VEH_NAME(ch->in_veh),
                   (IS_NPC(tmp) || GET_SKILL(tmp, language) > 0) ? skills[language].name : "an unknown language",
                   capitalize(replace_too_long_words(tmp, ch, argument, language, com_msgs[subcmd][3])),
                   ispunct(get_final_character_from_string(argument)) ? "" : "!",
                   com_msgs[subcmd][3]);

          // Replicate act() in a way that lets us capture the message.
          if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
            // They're a valid target, so send the message with a raw perform_act() call.
            store_message_to_history(tmp->desc, COMM_CHANNEL_SHOUTS, perform_act(buf1, ch, NULL, NULL, tmp, FALSE));
          }
        }
      }
    } else {
      for (veh = ch->in_room->vehicles; veh; veh = veh->next_veh) {
        ch->in_veh = veh;
        for (tmp = ch->in_veh->people; tmp; tmp = tmp->next_in_veh) {
          // Replicate act() in a way that lets us capture the message.
          if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM) && !IS_IGNORING(tmp, is_blocking_ic_interaction_from, ch)) {
            snprintf(buf1, sizeof(buf1), "%s$z%s shouts in %s, \"%s%s%s\"^n",
                     com_msgs[subcmd][3],
                     com_msgs[subcmd][3],
                     (IS_NPC(tmp) || GET_SKILL(tmp, language) > 0) ? skills[language].name : "an unknown language",
                     capitalize(replace_too_long_words(tmp, ch, argument, language, com_msgs[subcmd][3])),
                     ispunct(get_final_character_from_string(argument)) ? "" : "!",
                     com_msgs[subcmd][3]);

            // They're a valid target, so send the message with a raw perform_act() call.
            store_message_to_history(tmp->desc, COMM_CHANNEL_SHOUTS, perform_act(buf1, ch, NULL, NULL, tmp, FALSE));
          }
        }
        ch->in_veh = NULL;
      }
    }

    for (int door = 0; door < NUM_OF_DIRS; door++)
      if (CAN_GO(ch, door)) {
        ch->in_room = ch->in_room->dir_option[door]->to_room;
        for (tmp = get_ch_in_room(ch)->people; tmp; tmp = tmp->next_in_room)
          if (tmp != ch && !IS_IGNORING(tmp, is_blocking_ic_interaction_from, ch)) {
            snprintf(buf, sizeof(buf), "%s$v%s shouts in %s, \"%s%s%s\"^n",
                     com_msgs[subcmd][3],
                     com_msgs[subcmd][3],
                     (IS_NPC(tmp) || GET_SKILL(tmp, language) > 0) ? skills[language].name : "an unknown language",
                     capitalize(replace_too_long_words(tmp, ch, argument, language, com_msgs[subcmd][3])),
                     ispunct(get_final_character_from_string(argument)) ? "" : "!",
                     com_msgs[subcmd][3]);

            // Store to their history.
            store_message_to_history(tmp->desc, COMM_CHANNEL_SHOUTS, act(buf, FALSE, ch, NULL, tmp, TO_VICT));
          }
        ch->in_room = was_in;
      }
    if (ch->in_veh)
      ch->in_room = NULL;

    return;
  }

  // Returning command to handle OOC.
  if(subcmd == SCMD_OOC) {
    // Remove the doubled dollar signs.
    delete_doubledollar(argument);

    /* Check for non-switched mobs */
    if ( IS_NPC(ch) && ch->desc == NULL )
      return;
    for ( d = descriptor_list; d != NULL; d = d->next ) {
      // Skip anyone without a descriptor, and any non-NPC that ignored the speaker.
      if (!d->character || IS_IGNORING(d->character, is_blocking_oocs_from, ch))
        continue;

      // Skip chargen chars (they never see OOC)
      if (PLR_FLAGGED(d->character, PLR_NOT_YET_AUTHED) || PLR_FLAGGED(d->character, PLR_IN_CHARGEN))
        continue;

      // Skip anyone in the login menus.
      if (!IS_VALID_STATE_TO_RECEIVE_COMMS(d->connected))
        continue;

      // Skip anyone who's opted out of OOC.
      if (PRF_FLAGGED(d->character, PRF_NOOOC))
        continue;

      // Skip anyone who's editing and has flagged themselves as menu-gagged.
      if (d->connected != CON_PLAYING && PRF_FLAGGED(d->character, PRF_MENUGAG))
        continue;

      // No autopunct for channels.
      if (!access_level(d->character, GET_INCOG_LEV(ch)))
        snprintf(buf, sizeof(buf), "^m[^nA Staff Member^m]^n ^R(^nOOC^R)^n, \"%s%s^n\"\r\n", capitalize(argument), get_final_character_from_string(argument) == '^' ? "^" : "");
      else
        snprintf(buf, sizeof(buf), "^m[^n%s^m]^n ^R(^nOOC^R)^n, \"%s%s^n\"\r\n", IS_NPC(ch) ? GET_NAME(ch) : GET_CHAR_NAME(ch), capitalize(argument), get_final_character_from_string(argument) == '^' ? "^" : "");

      store_message_to_history(d, COMM_CHANNEL_OOC, buf);

      if (d->character == ch && PRF_FLAGGED(ch, PRF_NOREPEAT)) {
        send_to_char(OK, ch);
        continue;
      } else {
        send_to_char(buf, d->character);
      }
    }

    return;
  }

  // The commands after this line don't return-- they just set things and follow the loop at the end.
  if (subcmd == SCMD_RPETALK) {
    // Remove the doubled dollar signs.
    delete_doubledollar(argument);

    snprintf(buf, sizeof(buf), "^W%s [^CRPEtalk^n]^c %s^n\r\n", GET_CHAR_NAME(ch), capitalize(argument));
    send_to_char(buf, ch);

    channel = COMM_CHANNEL_RPE;
    store_message_to_history(ch->desc, channel, buf);
  } else if (subcmd == SCMD_HIREDTALK) {
    // Remove the doubled dollar signs.
    delete_doubledollar(argument);

    snprintf(buf, sizeof(buf), "%s%s ^y[^YHIRED^y]^Y %s^n\r\n", com_msgs[subcmd][3], GET_CHAR_NAME(ch), capitalize(argument));
    send_to_char(buf, ch);

    channel = COMM_CHANNEL_HIRED;
    store_message_to_history(ch->desc, channel, buf);
  } else {
    // Remove the doubled dollar signs.
    delete_doubledollar(argument);

    snprintf(buf, sizeof(buf), "%s%s |]Question[| %s^n\r\n", com_msgs[subcmd][3], GET_CHAR_NAME(ch), capitalize(argument));
    send_to_char(buf, ch);

    channel = COMM_CHANNEL_QUESTIONS;
    store_message_to_history(ch->desc, channel, buf);
  }

  // The loop for the above channels.
  for (i = descriptor_list; i; i = i->next) {
    if (i == ch->desc || !i->character || IS_PROJECT(i->character))
      continue;

    // Skip anyone who disabled the command.
    if (PRF_FLAGGED(i->character, channels[subcmd]))
      continue;

    // Non-staff who don't meet specific criteria are skipped.
    if (!IS_SENATOR(i->character)) {
      if (i->connected || PLR_FLAGS(i->character).AreAnySet(PLR_EDITING, ENDBIT))
        continue;
    }

    switch (subcmd) {
      case SCMD_SHOUT:
        // Shouts don't propagate into soundproof rooms.
        if (ROOM_FLAGGED(get_ch_in_room(i->character), ROOM_SOUNDPROOF))
          continue;

        // Skip anyone who's ignored the speaker.
        if (IS_IGNORING(i->character, is_blocking_ic_interaction_from, ch))
          continue;

        break;
      case SCMD_QUESTION:
        // Newbie can be disabled or muted.
        if (PRF_FLAGGED(i->character, PRF_NOQUESTIONS) || PLR_FLAGGED(i->character, PLR_QUESTIONS_MUTED))
          continue;

        // Skip anyone who's ignored the speaker.
        if (IS_IGNORING(i->character, is_blocking_oocs_from, ch))
          continue;

        break;
      case SCMD_RPETALK:
        // RPE talk only shows up to RPE members.
        if (!(PLR_FLAGGED(i->character, PLR_RPE) || IS_SENATOR(i->character)))
          continue;

        // Skip anyone who's ignored the speaker.
        if (IS_IGNORING(i->character, is_blocking_oocs_from, ch))
          continue;

        break;
      case SCMD_HIREDTALK:
        // Hired talk only shows up to hired members.
        if (!(PRF_FLAGGED(i->character, PRF_HIRED) || IS_SENATOR(i->character)))
          continue;

        // Skip anyone who's ignored the speaker.
        if (IS_IGNORING(i->character, is_blocking_oocs_from, ch))
          continue;

        break;
    }

    if (i->character == ch && PRF_FLAGGED(ch, PRF_NOREPEAT))
      send_to_char(OK, ch);
    else
      send_to_char(buf, i->character);

    store_message_to_history(i, channel, buf);
  }

  /* now send all the strings out
  for (i = descriptor_list; i; i = i->next)
    if (!i->connected && i != ch->desc && i->character &&
        !PRF_FLAGGED(i->character, channels[subcmd]) &&
        !PLR_FLAGS(i->character).AreAnySet(PLR_EDITING, ENDBIT) &&
        !IS_PROJECT(i->character) &&
        !(ROOM_FLAGGED(get_ch_in_room(i->character), ROOM_SOUNDPROOF) && subcmd == SCMD_SHOUT)) {
      if (subcmd == SCMD_QUESTION && !(PLR_FLAGGED(i->character, PLR_NEWBIE) ||
                                     IS_SENATOR(i->character) || PRF_FLAGGED(i->character, PRF_NEWBIEHELPER)))
        continue;
      else if (subcmd == SCMD_RPETALK && !(PLR_FLAGGED(i->character, PLR_RPE) ||
                                           IS_SENATOR(i->character)))
        continue;
      else if (subcmd == SCMD_HIREDTALK && !(PRF_FLAGGED(i->character, PRF_HIRED) ||
                                             IS_SENATOR(i->character)))
        continue;
      else {
        // Note that this invokes act().
        store_message_to_history(i, channel, act(buf, FALSE, ch, 0, i->character, TO_VICT | TO_SLEEP));
      }
    } */
}

ACMD(do_language)
{
  int i, lannum;
  one_argument(argument, arg);

  if (!*arg) {
    send_to_char("You know the following languages:\r\n", ch);
    for (i = SKILL_ENGLISH; i < MAX_SKILLS; i++) {
      if (!SKILL_IS_LANGUAGE(i))
        continue;

      if ((GET_SKILL(ch, i)) > 0) {
        snprintf(buf, sizeof(buf), "%-20s %-17s", skills[i].name, how_good(i, GET_SKILL(ch, i)));
        if (GET_LANGUAGE(ch) == i)
          strlcat(buf, " ^Y(Speaking)^n", sizeof(buf));
        strlcat(buf, "\r\n", sizeof(buf));
        send_to_char(buf, ch);
      }
    }
    return;
  }

  for (lannum = SKILL_ENGLISH; lannum < MAX_SKILLS; lannum++) {
    if (SKILL_IS_LANGUAGE(lannum) && is_abbrev(arg, skills[lannum].name)) {
      break;
    }
  }

  if (lannum < MAX_SKILLS && SKILL_IS_LANGUAGE(lannum)) {
    if (GET_SKILL(ch, lannum) > 0 || IS_NPC(ch)) {
      GET_LANGUAGE(ch) = lannum;
      send_to_char(ch, "You will now speak %s.\r\n", skills[lannum].name);
    } else
      send_to_char("You don't know how to speak that language.\r\n", ch);
  } else {
    send_to_char(ch, "You don't know any language called '%s'.\r\n", arg);
  }
}

void add_phone_to_list(struct obj_data *obj)
{
  struct phone_data *k;
  bool cyber = GET_OBJ_TYPE(obj) == ITEM_CYBERWARE;

  // Compose the phone number string.
  snprintf(buf, sizeof(buf), "%04d%04d", GET_OBJ_VAL(obj, (cyber ? 3 : 0)), GET_OBJ_VAL(obj, (cyber ? 6 : 1)));

  // Search the phone list and see if it's already in there.
  for (struct phone_data *j = phone_list; j; j = j->next)
    if (j->number == atoi(buf)) {
      const char *phone_representation = generate_new_loggable_representation(j->phone);
      const char *obj_representation = generate_new_loggable_representation(obj);
      char logstring[MAX_STRING_LENGTH];
      snprintf(logstring, sizeof(logstring), "SYSERR: Attempted to add phone number %s to the phone list, but it's already there! Refusing to replace '%s' with '%s'.",
             buf,
             phone_representation,
             obj_representation);
      mudlog(logstring, NULL, LOG_SYSLOG, TRUE);
      DELETE_ARRAY_IF_EXTANT(phone_representation);
      DELETE_ARRAY_IF_EXTANT(obj_representation);
      return;
    }

  // It wasn't, so we can add it freely.
  k = new phone_data;
  k->number = atoi(buf);
  k->phone = obj;
  if (phone_list)
    k->next = phone_list;
  phone_list = k;
}

ACMD(do_phone)
{
  struct obj_data *obj;
  struct char_data *tch = FALSE;
  struct phone_data *k = NULL, *phone, *temp;
  bool cyber = FALSE;
  sh_int active = 0;
  int ring = 0;

  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (GET_OBJ_TYPE(obj) == ITEM_PHONE)
      break;
  for (int x = 0; !obj && x < NUM_WEARS; x++)
    if (GET_EQ(ch, x) && GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_PHONE)
      obj = GET_EQ(ch, x);
  if (!obj)
    for (obj = ch->cyberware; obj; obj = obj->next_content)
      if (GET_OBJ_VAL(obj, 0) == CYB_PHONE) {
        cyber = TRUE;
        break;
      }
  if (!obj) {
    send_to_char("But you don't have a phone.\r\n", ch);
    return;
  }

  // If the phone is already on, this finds its phone_data record.
  for (phone = phone_list; phone; phone = phone->next)
    if (phone->phone == obj)
      break;

  if (subcmd == SCMD_ANSWER) {
    if (!phone) {
      send_to_char("Try switching it on first.\r\n", ch);
      return;
    }
    if (phone->connected) {
      send_to_char("But you already have a call connected!\r\n", ch);
      return;
    }
    if (!phone->dest) {
      send_to_char("No one is calling you, what a shame.\r\n", ch);
      return;
    }
    if (phone->dest->persona)
      send_to_icon(phone->dest->persona, "The call is answered.\r\n");
    else {
      tch = phone->dest->phone->carried_by;
      if (!tch)
        tch = phone->dest->phone->worn_by;
      if (!tch && phone->dest->phone->in_obj)
        tch = phone->dest->phone->in_obj->carried_by;
      if (!tch && phone->dest->phone->in_obj)
        tch = phone->dest->phone->in_obj->worn_by;
      if (tch)
        send_to_char("The phone is answered.\r\n", tch);
    }
    send_to_char("You answer the phone.\r\n", ch);
    phone->connected = TRUE;
    phone->dest->connected = TRUE;
  } else if (subcmd == SCMD_RING) {
    if (!phone) {
      send_to_char("Try switching it on first.\r\n", ch);
      return;
    }
    if (phone->dest) {
      send_to_char("But you already have a call connected!\r\n", ch);
      return;
    }
    any_one_arg(argument, arg);
    if (!*arg) {
      send_to_char("Ring what number?\r\n", ch);
      return;
    }

    char parsed_number[MAX_INPUT_LENGTH];
    int parsed_idx = 0;
    for (size_t arg_idx = 0; arg_idx < strlen(arg); arg_idx++) {
      if (isdigit(arg[arg_idx]))
        parsed_number[parsed_idx++] = arg[arg_idx];
    }
    parsed_number[parsed_idx] = 0;

    if (!(ring = atoi(parsed_number))) {
      send_to_char("That is not a valid number.\r\n", ch);
      return;
    }

    for (k = phone_list; k; k = k->next)
      if (k->number == ring)
        break;

    if (!k) {
      send_to_char("The phone is picked up and a polite female voice says, \"The number you have dialed is not in"
                   " service. Please check your number and try again.\"\r\n", ch);
      return;
    }

    if (k == phone) {
      send_to_char("Why would you want to call yourself?\r\n", ch);
      return;
    }

    struct char_data *possessor = get_obj_possessor(k->phone);

    if (k->dest || (possessor && IS_IGNORING(possessor, is_blocking_calls_from, ch))) {
      send_to_char("You hear the busy signal.\r\n", ch);
      return;
    }

    phone->dest = k;
    phone->connected = TRUE;
    k->dest = phone;

    ring_phone(k);
    send_to_char("It begins to ring.\r\n", ch);
  } else if (subcmd == SCMD_HANGUP) {
    if (!phone) {
      send_to_char("Try switching it on first.\r\n", ch);
      return;
    }
    if (!phone->dest) {
      send_to_char("But you're not talking to anyone.\r\n", ch);
      return;
    }
    if (phone->dest->persona)
      send_to_icon(phone->dest->persona, "The flashing phone icon fades from view, and \"Missed Call: %.4d-%.4d\" flashes briefly.\r\n", (int) phone->number / 10000, (int) phone->number % 10000);
    else {
      tch = get_obj_possessor(phone->dest->phone);
      if (tch) {
        char ended_call_buf[1000];
        if (phone->dest->connected) {
          snprintf(ended_call_buf, sizeof(ended_call_buf), "The phone is hung up from the other side. A mechanical voice notes, \"Call ended: %.4d-%.4d\".\r\n", (int) phone->number / 10000, (int) phone->number % 10000);
          act(ended_call_buf, FALSE, tch, 0, 0, TO_CHAR);
        } else {
          snprintf(ended_call_buf, sizeof(ended_call_buf), "%s stops ringing, and \"Missed Call: %.4d-%.4d\" flashes briefly on its display.\r\n",
                   capitalize(GET_OBJ_NAME(phone->dest->phone)),
                   (int) phone->number / 10000,
                   (int) phone->number % 10000);
          act(ended_call_buf, FALSE, tch, 0, 0, TO_CHAR);
        }
      }
    }
    phone->connected = FALSE;
    phone->dest->dest = NULL;
    phone->dest->connected = FALSE;
    phone->dest = NULL;
    send_to_char("You end the call.\r\n", ch);
  } else if (subcmd == SCMD_TALK) {
    if (!phone) {
      send_to_char("Try switching it on first.\r\n", ch);
      return;
    }
    if (!phone->connected) {
      send_to_char("But you don't currently have a call.\r\n", ch);
      return;
    }
    if (!phone->dest->connected) {
      send_to_char(ch, "No one has answered it yet.\r\n");
      return;
    }
    if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
      return;

    skip_spaces(&argument);

    if (!*argument) {
      send_to_char("Yes, but what do you want to say?\r\n", ch);
      return;
    }

    int language = !SKILL_IS_LANGUAGE(GET_LANGUAGE(ch)) ? SKILL_ENGLISH : GET_LANGUAGE(ch);
    if (!has_required_language_ability_for_sentence(ch, argument, language))
      return;

    #define VOICE_BUF_SIZE 20
    char voice[VOICE_BUF_SIZE] = {"$v"};

    for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content)
      if (GET_CYBERWARE_TYPE(obj) == CYB_VOICEMOD && GET_CYBERWARE_FLAGS(obj))
        strlcpy(voice, "A masked voice", VOICE_BUF_SIZE);

    if (AFF_FLAGGED(ch, AFF_VOICE_MODULATOR))
      strlcpy(voice, "A masked voice", VOICE_BUF_SIZE);

    snprintf(buf3, MAX_STRING_LENGTH, "^YYou say into your phone in %s, \"%s%s^Y\"\r\n",
             skills[language].name,
             capitalize(argument),
             ispunct(get_final_character_from_string(argument)) ? "" : ".");

    send_to_char(buf3, ch);

    store_message_to_history(ch->desc, COMM_CHANNEL_PHONE, buf3);

    bool tch_is_matrix = FALSE;

    if (phone->dest->persona && phone->dest->persona->decker && phone->dest->persona->decker->ch) {
      tch = phone->dest->persona->decker->ch;
      tch_is_matrix = TRUE;
    } else {
      tch = get_obj_possessor(phone->dest->phone);
    }

    if (tch) {
      snprintf(buf, sizeof(buf), "^Y%s^Y on the other end of the line says in %s, \"%s%s^Y\"",
              tch_is_matrix ? get_voice_perceived_by(ch, tch, FALSE) : voice,
              (IS_NPC(tch) || GET_SKILL(tch, language) > 0) ? skills[language].name : "an unknown language",
              capitalize(replace_too_long_words(tch, ch, argument, language, "^Y")),
              ispunct(get_final_character_from_string(argument)) ? "" : ".");

      if (tch_is_matrix) {
        // Re-send it to the Matrix folks.
        store_message_to_history(tch->desc, COMM_CHANNEL_PHONE, buf);
        send_to_char(tch, "%s\r\n", buf);
      } else {
        store_message_to_history(tch->desc, COMM_CHANNEL_PHONE, act(buf, FALSE, ch, 0, tch, TO_VICT));
      }
    }
    // Send to nearby people.
    if (!cyber) {
      for (tch = ch->in_veh ? ch->in_veh->people : ch->in_room->people; tch; tch = ch->in_veh ? tch->next_in_veh : tch->next_in_room) {
        if (tch != ch) {
          snprintf(buf2, MAX_STRING_LENGTH, "$z^n says into $s phone in %s, \"%s%s%s^n\"",
                  (IS_NPC(tch) || GET_SKILL(tch, language) > 0) ? skills[language].name : "an unknown language",
                  (PRF_FLAGGED(tch, PRF_NOHIGHLIGHT) || PRF_FLAGGED(tch, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
                  capitalize(replace_too_long_words(tch, ch, argument, language, GET_CHAR_COLOR_HIGHLIGHT(ch))),
                  ispunct(get_final_character_from_string(argument)) ? "" : ".");

          store_message_to_history(tch->desc, COMM_CHANNEL_SAYS, act(buf2, FALSE, ch, 0, tch, TO_VICT));
        }
      }
    }
  } else {
    any_one_arg(argument, arg);
    if (!str_cmp(arg, "off"))
      active = 1;
    else if (!str_cmp(arg, "on"))
      active = 2;
    else if (!str_cmp(arg, "ring"))
      ring = 1;
    else if (!str_cmp(arg, "silent"))
      ring = 2;
    if (ring) {
      ring--;
      if (GET_OBJ_VAL(obj, (cyber ? 8 : 3)) == ring) {
        send_to_char(ch, "It's already set to %s.\r\n", ring ? "silent" : "ring");
        return;
      }
      send_to_char(ch, "You set %s to %s.\r\n", GET_OBJ_NAME(obj), ring ? "silent" : "ring");
      GET_OBJ_VAL(obj, (cyber ? 8 : 3)) = ring;
      return;
    }
    if (active) {
      if (phone && phone->dest) {
        send_to_char("You might want to hang up first.\r\n", ch);
        return;
      }
      active--;
      if (GET_OBJ_VAL(obj, (cyber ? 7 : 2)) == active) {
        send_to_char(ch, "It's already switched %s.\r\n", active ? "on" : "off");
        return;
      }
      send_to_char(ch, "You switch %s %s.\r\n", GET_OBJ_NAME(obj), active ? "on" : "off");
      GET_OBJ_VAL(obj, (cyber ? 7 : 2)) = active;
      if (active)
        add_phone_to_list(obj);
      else {
        if (!phone) {
          send_to_char("Uh WTF M8?\r\n", ch);
          snprintf(buf, sizeof(buf), "%s would have crashed the mud, whats up with their phone.", GET_CHAR_NAME(ch));
          mudlog(buf, ch, LOG_WIZLOG, TRUE);
          return;
        }
        REMOVE_FROM_LIST(phone, phone_list, next);
        DELETE_AND_NULL(phone);
      }
      return;
    }
    snprintf(buf, MAX_STRING_LENGTH,
            "%s:\r\n"
            "Phone Number: %04d-%04d\r\n"
            "Switched: %s\r\n"
            "Ringing: %s\r\n",
            GET_OBJ_NAME(obj),
            GET_OBJ_VAL(obj, (cyber ? 3 : 0)),
            GET_OBJ_VAL(obj, (cyber ? 6 : 1)),
            GET_OBJ_VAL(obj, (cyber ? 7 : 2)) ? "On" : "Off",
            GET_OBJ_VAL(obj, (cyber ? 8 : 3)) ? "Off": "On"
    );

    if (phone && phone->dest) {
      if (phone->dest->connected && phone->connected)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Connected to: %d\r\n", phone->dest->number);
      else if (!phone->dest->connected)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Calling: %d\r\n", phone->dest->number);
      else snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Incoming call from: %08d\r\n", phone->dest->number);
    }
    send_to_char(buf, ch);
  }
}

ACMD(do_phonelist)
{
  struct char_data *tch = NULL;
  char identifier[1000];
  int i = 0;

  if (!phone_list)
    send_to_char(ch, "The phone list is empty.\r\n");

  for (struct phone_data *k = phone_list; k; k = k->next) {
    if (k->persona && k->persona->decker) {
      tch = k->persona->decker->ch;
    } else if (k->phone) {
      tch = get_obj_possessor(k->phone);
    }

    if (tch) {
      if (IS_NPC(tch))
        strlcpy(identifier, GET_NAME(tch), sizeof(identifier));
      else
        strlcpy(identifier, GET_CHAR_NAME(tch), sizeof(identifier));
    } else {
      struct room_data *in_room = get_obj_in_room(k->phone);
      if (in_room) {
        snprintf(identifier, sizeof(identifier), "in room %s (%ld)", GET_ROOM_NAME(in_room), GET_ROOM_VNUM(in_room));
      } else {
        strlcpy(identifier, "nowhere!", sizeof(identifier));
      }
    }

    send_to_char(ch, "%2d) %d (%s) (%s)\r\n", i++, k->number, (k->dest ? "Busy" : "Free"), identifier);
  }
}

void ring_phone(struct phone_data *k) {
  // Matrix call?
  if (k->persona) {
    send_to_icon(k->persona, "A small telephone symbol blinks in the top left of your view.\r\n");
  }

  // RL call.
  else {
    struct char_data *tch = get_obj_possessor(k->phone);

    // If it's being carried, notify the carrier, and potentially their room.
    if (tch) {
      // Wake 'em up.
      if (GET_POS(tch) == POS_SLEEPING) {
        GET_POS(tch) = POS_RESTING;
        send_to_char("You are woken by your phone ringing.\r\n", tch);
        if (!GET_OBJ_VAL(k->phone, 3))
          act("$n is startled awake by the ringing of $s phone.", FALSE, tch, 0, 0, TO_ROOM);
      }

      // If the ringer is on, notify the room, otherwise just the carrier.
      if (!GET_OBJ_VAL(k->phone, 3)) {
        struct room_data *in_room = get_ch_in_room(tch);
        act("$p rings.", FALSE, tch, k->phone, 0, TO_CHAR);
        if (in_room && GET_ROOM_VNUM(in_room) > 1)
          act("$n's phone rings.", FALSE, tch, 0, 0, TO_ROOM);
      }

      // Just the carrier, then.
      else {
        act("You feel your phone ring.", FALSE, tch, 0, 0, TO_CHAR);
      }
    }
    // Phone is on the ground. Ring there.
    else {
      snprintf(buf, sizeof(buf), "%s rings.", GET_OBJ_NAME(k->phone));
      if (k->phone->in_room || k->phone->in_veh)
        act(buf, FALSE, NULL, k->phone, 0, TO_ROOM);
    }
  }
}

void phone_check()
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct char_data *originator;
  for (struct phone_data *k = phone_list; k; k = k->next) {
    if (k->dest && !k->connected) {
      ring_phone(k);

      if (k->dest->phone && (originator = get_obj_possessor(k->dest->phone)))
        send_to_char("The other end continues to ring.\r\n", originator);
    }
  }
}

void send_message_history_to_descriptor(struct descriptor_data *d, int channel, int maximum, const char* channel_string) {
  // Precondition: No screwy pointers. Allow for NPCs to be passed (we ignore them).
  if (d == NULL) {
    //mudlog("SYSERR: Null descriptor passed to send_message_history_to_descriptor.", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // Precondition: Channel must be a valid index (0 ≤ channel < number of channels defined in awake.h).
  if (channel < 0 || channel >= NUM_COMMUNICATION_CHANNELS) {
    snprintf(buf, sizeof(buf), "SYSERR: Channel %d is not within bounds 0 <= channel < %d.", channel, NUM_COMMUNICATION_CHANNELS);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return;
  }

  if (maximum > NUM_MESSAGES_TO_RETAIN) {
    snprintf(buf, sizeof(buf), "SYSERR: send_message_history_to_descriptor asked to send %d messages, but max message history is %d.",
            maximum, NUM_MESSAGES_TO_RETAIN);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return;
  }

  if (maximum <= 0) {
    snprintf(buf, sizeof(buf), "SYSERR: send_message_history_to_descriptor asked to send %d messages, but minimum is 1.", maximum);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return;
  }

  if (d->message_history[channel].NumItems() == 0) {
    snprintf(buf, sizeof(buf), "You have no messages %s yet.\r\n", channel_string);
    write_to_output(buf, d);
    return;
  }

  int skip = d->message_history[channel].NumItems() - maximum;

  snprintf(buf, sizeof(buf), "The last %d messages you've %s are:\r\n", maximum ? maximum : NUM_MESSAGES_TO_RETAIN, channel_string);
  write_to_output(buf, d);

  // For every message in their history, print the list from oldest to newest.
  for (nodeStruct<const char*> *currnode = d->message_history[channel].Tail(); currnode; currnode = currnode->prev) {
    // If they've requested a maximum of X, then skip 20-X of the oldest messages before we start sending them.
    if (skip-- > 0)
      continue;

    snprintf(buf, sizeof(buf), "  %s", currnode->data);
    int size = strlen(buf);
    write_to_output(ProtocolOutput(d, buf, &size, DONT_APPEND_GA), d);
  }
}

void display_message_history_help(struct char_data *ch) {
  send_to_char("You have past messages available for the following channels:\r\n", ch);

  // Send the names of any channels that have messages waiting.
  for (int channel = 0; channel < NUM_COMMUNICATION_CHANNELS; channel++)
    if (ch->desc->message_history[channel].NumItems() > 0)
      send_to_char(ch, "  %s\r\n", message_history_channels[channel]);

  send_to_char("\r\nSyntax: HISTORY <channel name> [optional number of messages to retrieve]\r\n", ch);
}

void raw_message_history(struct char_data *ch, int channel, int quantity) {
  switch (channel) {
    case COMM_CHANNEL_HIRED:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "heard over the Hired Talk channel");
      break;
    case COMM_CHANNEL_QUESTIONS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "seen on the Question channel");
      break;
    case COMM_CHANNEL_OOC:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "seen on the OOC channel");
      break;
    case COMM_CHANNEL_OSAYS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "heard said OOCly");
      break;
    case COMM_CHANNEL_PAGES:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "seen over pages");
      break;
    case COMM_CHANNEL_PHONE:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "heard over the phone");
      break;
    case COMM_CHANNEL_RPE:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "seen over the RPE Talk channel");
      break;
    case COMM_CHANNEL_RADIO:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "heard over the radio");
      break;
    case COMM_CHANNEL_SAYS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "heard spoken aloud");
      break;
    case COMM_CHANNEL_SHOUTS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "heard from nearby shouts");
      break;
    case COMM_CHANNEL_TELLS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "seen over tells");
      break;
    case COMM_CHANNEL_WTELLS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "seen over wiztells");
      break;
    case COMM_CHANNEL_EMOTES:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "seen via emotes");
      break;
    case COMM_CHANNEL_LOCAL:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "seen locally");
      break;
    case COMM_CHANNEL_ROLEPLAY:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "seen ICly");
      break;
    case COMM_CHANNEL_ALL:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "seen across all channels");
      break;
    case COMM_CHANNEL_DOCWAGON_CHAT:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "seen on the OOC Docwagon Chat channel");
      break;
    default:
      snprintf(buf, sizeof(buf), "SYSERR: Unrecognized channel/subcmd %d provided to raw_message_history's channel switch.", channel);
      mudlog(buf, NULL, LOG_SYSLOG, TRUE);
      send_to_char(ch, "Sorry, this command appears to have an error. Staff have been notified.\r\n");
      break;
  }
}

ACMD(do_switched_message_history) {
  int channel = subcmd, quantity = NUM_MESSAGES_TO_RETAIN;

  assert(channel >= 0 && channel < NUM_COMMUNICATION_CHANNELS);

  if (*argument) {
    if ((quantity = atoi(argument)) > NUM_MESSAGES_TO_RETAIN) {
      send_to_char(ch, "Sorry, the system only retains up to %d messages.\r\n", NUM_MESSAGES_TO_RETAIN);
      // This is not a fatal error.
    }

    if (quantity < 1) {
      send_to_char("You must specify a number of history messages greater than 0.\r\n", ch);
      return;
    }
  }

  raw_message_history(ch, channel, quantity);
}

ACMD(do_message_history) {
  int channel, quantity = NUM_MESSAGES_TO_RETAIN;

  if (!*argument) {
    display_message_history_help(ch);
    return;
  }

  half_chop(argument, buf, buf2, sizeof(buf2));

  // Find the channel referenced in first parameter.
  for (channel = 0; channel < NUM_COMMUNICATION_CHANNELS; channel++)
    if (is_abbrev(buf, message_history_channels[channel]) || (channel == COMM_CHANNEL_QUESTIONS && is_abbrev(buf, "newbie")))
      break;

  // No channel found? Fail.
  if (channel >= NUM_COMMUNICATION_CHANNELS) {
    if (is_abbrev(buf, "echoes") || is_abbrev(buf, "echos")) {
      channel = COMM_CHANNEL_EMOTES;
    } else {
      send_to_char(ch, "Sorry, '%s' is not a recognized channel.\r\n", buf);
      display_message_history_help(ch);
      return;
    }
  }

  if (*buf2) {
    if ((quantity = atoi(buf2)) > NUM_MESSAGES_TO_RETAIN) {
      send_to_char(ch, "Sorry, the system only retains up to %d messages.\r\n", NUM_MESSAGES_TO_RETAIN);
      // This is not a fatal error.
    }

    if (quantity < 1) {
      send_to_char("You must specify a number of history messages greater than 0.\r\n", ch);
      return;
    }
  }

  raw_message_history(ch, channel, quantity);
}
