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

using namespace std;

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "screen.h"
#include "awake.h"
#include "newmatrix.h"
#include "newdb.h"
#include "constants.h"

/* extern variables */
extern struct skill_data skills[];

extern void respond(struct char_data *ch, struct char_data *mob, char *str);
extern bool can_send_act_to_target(struct char_data *ch, bool hide_invisible, struct obj_data * obj, void *vict_obj, struct char_data *to, int type);
extern char *how_good(int percent);
extern char *colorize(struct descriptor_data *d, const char *str, bool skip_check = FALSE);
int find_skill_num(char *name);


ACMD(do_say);

ACMD_CONST(do_say) {
  static char not_const[MAX_STRING_LENGTH];
  strcpy(not_const, argument);
  do_say(ch, not_const, cmd, subcmd);
}

ACMD(do_say)
{
  int success, suc;
  struct char_data *tmp, *to = NULL;
  
  skip_spaces(&argument);
  if (!*argument)
    send_to_char(ch, "Yes, but WHAT do you want to say?\r\n");
  else if (subcmd != SCMD_OSAY && !PLR_FLAGGED(ch, PLR_MATRIX) && !char_can_make_noise(ch))
    send_to_char("You can't seem to make any noise.\r\n", ch);
  else {
    if (AFF_FLAGGED(ch, AFF_RIG)) {
      send_to_char(ch, "You have no mouth.\r\n");
      return;
    } else if (PLR_FLAGGED(ch, PLR_MATRIX)) {
      if (ch->persona) {
        // Send the self-referencing message to the decker and store it in their history.
        sprintf(buf, "You say, \"%s^n\"\r\n", argument);
        send_to_icon(ch->persona, buf);
        store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, str_dup(buf));
        
        // Send the message to the rest of the host. Store it to the recipients' says history.
        sprintf(buf, "%s says, \"%s^n\"\r\n", ch->persona->name, argument);
        send_to_host(ch->persona->in_host, buf, ch->persona, TRUE);
        for (struct matrix_icon *i = matrix[ch->persona->in_host].icons; i; i = i->next_in_host) {
          if (ch->persona != i && i->decker && has_spotted(i, ch->persona)) {
            send_to_icon(i, buf);
            if (i->decker && i->decker->ch)
              store_message_to_history(i->decker->ch->desc, COMM_CHANNEL_SAYS, str_dup(buf));
          }
        }
      } else {
        for (struct char_data *targ = get_ch_en_room(ch)->people; targ; targ = targ->next_en_room)
          if (targ != ch && PLR_FLAGGED(targ, PLR_MATRIX)) {
            // Send and store.
            sprintf(buf, "Your hitcher says, \"%s^n\"\r\n", argument);
            send_to_char(targ, buf);
            store_message_to_history(targ->desc, COMM_CHANNEL_SAYS, str_dup(buf));
          }
        // Send and store.
        sprintf(buf, "You send, down the line, \"%s^n\"\r\n", argument);
        send_to_char(ch, buf);
        store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, str_dup(buf));
      }
      return;
    }
    if (subcmd == SCMD_SAYTO) {
      half_chop(argument, buf, buf2);
      strcpy(argument, buf2);
      if (ch->in_veh)
        to = get_char_veh(ch, buf, ch->in_veh);
      else
        to = get_char_room_vis(ch, buf);
    }
    if (ch->in_veh) {
      if(subcmd == SCMD_OSAY) {
        sprintf(buf,"%s says ^mOOCly^n, \"%s^n\"\r\n",GET_NAME(ch), argument);
        for (tmp = ch->in_veh->people; tmp; tmp = tmp->next_in_veh) {
          // Replicate act() in a way that lets us capture the message.
          if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
            // They're a valid target, so send the message with a raw perform_act() call.
            store_message_to_history(tmp->desc, COMM_CHANNEL_OSAYS, str_dup(perform_act(buf, ch, NULL, NULL, tmp)));
          }
        }
      } else {
        success = success_test(GET_SKILL(ch, GET_LANGUAGE(ch)), 4);
        for (tmp = ch->in_veh->people; tmp; tmp = tmp->next_in_veh)
          if (tmp != ch) {
            if (to) {
              if (to == tmp)
                sprintf(buf2, " to you");
              else
                sprintf(buf2, " to %s", CAN_SEE(tmp, to) ? (found_mem(GET_MEMORY(tmp), to) ? CAP(found_mem(GET_MEMORY(tmp), to)->mem)
                        : GET_NAME(to)) : "someone");
            }
            if (success > 0) {
              suc = success_test(GET_SKILL(tmp, GET_LANGUAGE(ch)), 4);
              if (suc > 0 || IS_NPC(tmp))
                sprintf(buf, "$z says%s in %s, \"%s^n\"",
                        (to ? buf2 : ""), skills[GET_LANGUAGE(ch)].name, argument);
              else
                sprintf(buf, "$z speaks%s in a language you don't understand.", (to ? buf2 : ""));
            } else
              sprintf(buf, "$z mumbles incoherently.");
            if (IS_NPC(ch))
              sprintf(buf, "$z says%s, \"%s^n\"", (to ? buf2 : ""), argument);
            // Note: includes act()
            store_message_to_history(tmp->desc, COMM_CHANNEL_SAYS, str_dup(act(buf, FALSE, ch, NULL, tmp, TO_VICT)));
          }
      }
    } else {
      /** new code by WASHU **/
      if(subcmd == SCMD_OSAY) {
        sprintf(buf,"$z ^nsays ^mOOCly^n, \"%s^n\"",argument);
        for (tmp = get_ch_en_room(ch)->people; tmp; tmp = tmp->next) {
          // Replicate act() in a way that lets us capture the message.
          if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
            // They're a valid target, so send the message with a raw perform_act() call.
            store_message_to_history(tmp->desc, COMM_CHANNEL_OSAYS, str_dup(perform_act(buf, ch, NULL, NULL, tmp)));
          }
        }
      } else {
        success = success_test(GET_SKILL(ch, GET_LANGUAGE(ch)), 4);
        for (tmp = get_ch_en_room(ch)->people; tmp; tmp = tmp->next_en_room)
          if (tmp != ch && !(IS_ASTRAL(ch) && !CAN_SEE(tmp, ch))) {
            if (to) {
              if (to == tmp)
                sprintf(buf2, " to you");
              else
                sprintf(buf2, " to %s", CAN_SEE(tmp, to) ? (found_mem(GET_MEMORY(tmp), to) ? CAP(found_mem(GET_MEMORY(tmp), to)->mem)
                        : GET_NAME(to)) : "someone");
            }
            if (success > 0) {
              suc = success_test(GET_SKILL(tmp, GET_LANGUAGE(ch)), 4);
              if (suc > 0 || IS_NPC(tmp))
                sprintf(buf, "$z says%s in %s, \"%s^n\"",
                        (to ? buf2 : ""), skills[GET_LANGUAGE(ch)].name, argument);
              else
                sprintf(buf, "$z speaks%s in a language you don't understand.", (to ? buf2 : ""));
            } else
              sprintf(buf, "$z mumbles incoherently.");
            if (IS_NPC(ch))
              sprintf(buf, "$z says%s, \"%s^n\"", (to ? buf2 : ""), argument);
            // Invokes act().
            store_message_to_history(tmp->desc, COMM_CHANNEL_SAYS, str_dup(act(buf, FALSE, ch, NULL, tmp, TO_VICT)));
          }
      }
    }
    // Acknowledge transmission of message.
    if (PRF_FLAGGED(ch, PRF_NOREPEAT))
      send_to_char(OK, ch);
    else {
      delete_doubledollar(argument);
      if(subcmd == SCMD_OSAY) {
        sprintf(buf, "You say ^mOOCly^n, \"%s^n\"\r\n", argument);
        send_to_char(ch, buf);
        store_message_to_history(ch->desc, COMM_CHANNEL_OSAYS, str_dup(buf));
      } else {
        if (to)
          sprintf(buf2, " to %s", CAN_SEE(ch, to) ? (found_mem(GET_MEMORY(ch), to) ?
                                                     CAP(found_mem(GET_MEMORY(ch), to)->mem) : GET_NAME(to)) : "someone");
        sprintf(buf, "You say%s, \"%s^n\"\r\n", (to ? buf2 : ""), argument);
        send_to_char(ch, buf);
        store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, str_dup(buf));
      }
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
  
  if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
    return;
  
  sprintf(buf, "$z ^nexclaims, \"%s!^n\"", argument);
  if (ch->in_veh) {
    for (struct char_data *tmp = ch->in_veh->people; tmp; tmp = tmp->next_in_veh) {
      // Replicate act() in a way that lets us capture the message.
      if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
        // They're a valid target, so send the message with a raw perform_act() call.
        store_message_to_history(tmp->desc, COMM_CHANNEL_SAYS, str_dup(perform_act(buf, ch, NULL, NULL, tmp)));
      }
    }
  } else {
    for (struct char_data *tmp = get_ch_en_room(ch)->people; tmp; tmp = tmp->next) {
      // Replicate act() in a way that lets us capture the message.
      if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
        // They're a valid target, so send the message with a raw perform_act() call.
        store_message_to_history(tmp->desc, COMM_CHANNEL_SAYS, str_dup(perform_act(buf, ch, NULL, NULL, tmp)));
      }
    }
  }
  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else {
    sprintf(buf, "You exclaim, \"%s!^n\"\r\n", argument);
    send_to_char(ch, buf);
    store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, str_dup(buf));
  }
}

void perform_tell(struct char_data *ch, struct char_data *vict, char *arg)
{
  sprintf(buf, "^r$n tells you, '%s^r'^n", arg);
  store_message_to_history(vict->desc, COMM_CHANNEL_TELLS, str_dup(act(buf, FALSE, ch, 0, vict, TO_VICT)));

  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else
  {
    sprintf(buf, "^rYou tell $n, '%s'^n", arg);
    store_message_to_history(ch->desc, COMM_CHANNEL_TELLS, str_dup(act(buf, FALSE, vict, 0, ch, TO_VICT)));
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

  half_chop(argument, buf, buf2);

  if (!*buf || !*buf2) {
    send_to_char("Who do you wish to tell what??\r\n", ch);
    return;
  }
  if (!(vict = get_player_vis(ch, buf, 0))) {
    send_to_char(NOPERSON, ch);
    return;
  }
  if (!IS_NPC(vict) && !vict->desc)      /* linkless */
    act("$E's linkless at the moment.", FALSE, ch, 0, vict, TO_CHAR);
  if (!IS_SENATOR(ch) && !IS_SENATOR(vict)) {
    send_to_char("Tell is for communicating with immortals only.\r\n", ch);
    return;
  }

  if (PRF_FLAGGED(vict, PRF_NOTELL))
    act("$E can't hear you.", FALSE, ch, 0, vict, TO_CHAR);
  else if (PLR_FLAGGED(vict, PLR_WRITING) ||
           PLR_FLAGGED(vict, PLR_MAILING))
    act("$E's writing a message right now; try again later.", FALSE, ch, 0, vict, TO_CHAR);
  else if (PRF_FLAGGED(vict, PRF_AFK))
    act("$E's AFK at the moment.", FALSE, ch, 0, vict, TO_CHAR);
  else if (PLR_FLAGGED(vict, PLR_EDITING))
    act("$E's editing right now, try again later.", FALSE, ch, 0, vict, TO_CHAR);
  else
    perform_tell(ch, vict, buf2);
}

ACMD(do_reply)
{
  struct char_data *tch = character_list;

  skip_spaces(&argument);

  if (GET_LAST_TELL(ch) == NOBODY)
    send_to_char("You have no-one to reply to!\r\n", ch);
  else if (!*argument)
    send_to_char("What is your reply?\r\n", ch);
  else {
    /* Make sure the person you're replying to is still playing by searching
     * for them.  Note, this will break in a big way if I ever implement some
     * scheme where it keeps a pool of char_data structures for reuse.
     */

    for (; tch != NULL; tch = tch->next)
      if (!IS_NPC(tch) && GET_IDNUM(tch) == GET_LAST_TELL(ch))
        break;

    if (tch == NULL || (tch && GET_IDNUM(tch) != GET_LAST_TELL(ch)))
      send_to_char("They are no longer playing.\r\n", ch);
    else
      perform_tell(ch, tch, argument);
  }
}

ACMD(do_ask)
{
  skip_spaces(&argument);

  if (!*argument) {
    send_to_char(ch, "Yes, but WHAT do you like to ask?\r\n");
    return;
  }
  
  if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
    return;
  
  sprintf(buf, "$z asks, \"%s?^n\"", argument);
  if (ch->in_veh) {
    for (struct char_data *tmp = ch->in_veh->people; tmp; tmp = tmp->next_in_veh) {
      // Replicate act() in a way that lets us capture the message.
      if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
        // They're a valid target, so send the message with a raw perform_act() call.
        store_message_to_history(tmp->desc, COMM_CHANNEL_SAYS, str_dup(perform_act(buf, ch, NULL, NULL, tmp)));
      }
    }
  } else {
    for (struct char_data *tmp = get_ch_en_room(ch)->people; tmp; tmp = tmp->next) {
      // Replicate act() in a way that lets us capture the message.
      if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
        // They're a valid target, so send the message with a raw perform_act() call.
        store_message_to_history(tmp->desc, COMM_CHANNEL_SAYS, str_dup(perform_act(buf, ch, NULL, NULL, tmp)));
      }
    }
  }
  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else {
    // TODO
    sprintf(buf, "You ask, \"%s?^n\"\r\n", argument);
    send_to_char(ch, buf);
    store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, str_dup(buf));
  }
}

ACMD(do_spec_comm)
{
  struct veh_data *veh = NULL;
  struct char_data *vict;
  const char *action_sing, *action_plur, *action_others;
  int success, suc;
  if (subcmd == SCMD_WHISPER) {
    action_sing = "whisper to";
    action_plur = "whispers to";
    action_others = "$z whispers something to $N.";
  } else {
    action_sing = "ask";
    action_plur = "asks";
    action_others = "$z asks $N something.";
  }

  half_chop(argument, buf, buf2);
  success = success_test(GET_SKILL(ch, GET_LANGUAGE(ch)), 4);

  if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
    return;
  
  if (!*buf || !*buf2) {
    sprintf(buf, "Whom do you want to %s... and what??\r\n", action_sing);
    send_to_char(buf, ch);
  } else if (ch->in_veh) {
    if (ch->in_veh->cspeed > SPEED_IDLE) {
      send_to_char("You're going to hit your head on a lamppost if you try that.\r\n", ch);
      return;
    }
    ch->en_room = ch->in_veh->en_room;
    if ((vict = get_char_room_vis(ch, buf))) {
      if (success > 0) {
        sprintf(buf, "You lean out towards $N and say, \"%s\"", buf2);
        store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, str_dup(act(buf, FALSE, ch, NULL, vict, TO_CHAR)));
        suc = success_test(GET_SKILL(vict, GET_LANGUAGE(ch)), 4);
        if (suc > 0)
          sprintf(buf, "From within %s^n, $z says to you in %s, \"%s^n\"\r\n",
                  GET_VEH_NAME(ch->in_veh), skills[GET_LANGUAGE(ch)].name, buf2);
        else
          sprintf(buf, "From within %s^n, $z speaks in a language you don't understand.\r\n", GET_VEH_NAME(ch->in_veh));
      } else
        sprintf(buf, "$z mumbles incoherently from %s.\r\n", GET_VEH_NAME(ch->in_veh));
      store_message_to_history(vict->desc, COMM_CHANNEL_SAYS, str_dup(act(buf, FALSE, ch, NULL, vict, TO_VICT)));
    }
    ch->en_room = NULL;
  } else if (!(vict = get_char_room_vis(ch, buf)) &&
             !((veh = get_veh_list(buf, get_ch_en_room(ch)->vehicles, ch)) && subcmd == SCMD_WHISPER))
    send_to_char(NOPERSON, ch);
  else if (vict == ch)
    send_to_char("You can't get your mouth close enough to your ear...\r\n", ch);
  else if (IS_ASTRAL(ch) && vict &&
           !(IS_ASTRAL(vict) ||
             PLR_FLAGGED(vict, PLR_PERCEIVE) ||
             IS_DUAL(vict)))
    send_to_char("That is most assuredly not possible.\r\n", ch);
  else {
    if (veh) {
      if (veh->cspeed > SPEED_IDLE) {
        send_to_char(ch, "It's moving too fast for you to lean in.\r\n");
        return;
      }

      sprintf(buf, "You lean into a %s and say, \"%s\"", GET_VEH_NAME(veh), buf2);
      send_to_char(buf, ch);
      store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, str_dup(buf));
      for (vict = veh->people; vict; vict = vict->next_in_veh) {
        if (success > 0) {
          suc = success_test(GET_SKILL(vict, GET_LANGUAGE(ch)), 4);
          if (suc > 0)
            sprintf(buf, "From outside, $z^n says into the vehicle in %s, \"%s^n\"\r\n", skills[GET_LANGUAGE(ch)].name, buf2);
          else
            sprintf(buf, "From outside, $z^n speaks into the vehicle in a language you don't understand.\r\n");
        } else
          sprintf(buf, "From outside, $z mumbles incoherently into the vehicle.\r\n");
        store_message_to_history(vict->desc, COMM_CHANNEL_SAYS, str_dup(act(buf, FALSE, ch, NULL, vict, TO_VICT)));
      }
      return;
    }
    if (success > 0) {
      suc = success_test(GET_SKILL(vict, GET_LANGUAGE(ch)), 4);
      if (suc > 1)
        sprintf(buf, "$z %s you in %s, \"%s^n\"\r\n", action_plur, skills[GET_LANGUAGE(ch)].name, buf2);
      else if (suc == 1)
        sprintf(buf, "$z %s in %s, but you don't understand.\r\n", action_plur, skills[GET_LANGUAGE(ch)].name);
      else
        sprintf(buf, "$z %s you in a language you don't understand.\r\n", action_plur);
    } else
      sprintf(buf, "$z %s to you incoherently.\r\n", action_plur);
    store_message_to_history(vict->desc, COMM_CHANNEL_SAYS, str_dup(act(buf, FALSE, ch, 0, vict, TO_VICT)));
    if (PRF_FLAGGED(ch, PRF_NOREPEAT))
      send_to_char(OK, ch);
    else {
      sprintf(buf, "You %s $N, \"%s%s^n\"", action_sing, buf2, (subcmd == SCMD_WHISPER) ? "" : "?");
      store_message_to_history(ch->desc, COMM_CHANNEL_SAYS, str_dup(act(buf, FALSE, ch, 0, vict, TO_CHAR)));
    }
    // TODO: Should this be stored to message history? It's super nondescript.
    act(action_others, FALSE, ch, 0, vict, TO_NOTVICT);
  }
}

ACMD(do_page)
{
  struct char_data *vict;

  half_chop(argument, arg, buf2);

  if (IS_NPC(ch))
    send_to_char("Monsters can't page.. go away.\r\n", ch);
  else if (!*arg)
    send_to_char("Whom do you wish to page?\r\n", ch);
  else {
    sprintf(buf, "\007\007*%s* %s", GET_CHAR_NAME(ch), buf2);
    if ((vict = get_char_vis(ch, arg)) != NULL) {
      if (vict == ch) {
        send_to_char("What's the point of that?\r\n", ch);
        return;
      }
      store_message_to_history(vict->desc, COMM_CHANNEL_PAGES, str_dup(act(buf, FALSE, ch, 0, vict, TO_VICT)));
      if (PRF_FLAGGED(ch, PRF_NOREPEAT))
        send_to_char(OK, ch);
      else
        store_message_to_history(ch->desc, COMM_CHANNEL_PAGES, str_dup(act(buf, FALSE, ch, 0, vict, TO_CHAR)));
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

  if (!radio) {
    send_to_char("You have to have a radio to do that!\r\n", ch);
    return;
  }
  any_one_arg(any_one_arg(argument, one), two);

  if (!*one) {
    act("$p:", FALSE, ch, radio, 0, TO_CHAR);
    if (GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))) == -1)
      send_to_char("  Mode: scan\r\n", ch);
    else if (!GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))))
      send_to_char("  Mode: off\r\n", ch);
    else
      send_to_char(ch, "  Mode: center @ %d MHz\r\n",
                   GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))));
    if (GET_OBJ_VAL(radio, (cyberware ? 6 : (vehicle ? 5 : 3))))
      send_to_char(ch, "  Crypt: on (level %d)\r\n",
                   GET_OBJ_VAL(radio, (cyberware ? 6 : (vehicle ? 5 : 3))));
    else
      send_to_char("  Crypt: off\r\n", ch);
    return;
  } else if (!str_cmp(one, "off")) {
    act("You turn $p off.", FALSE, ch, radio, 0, TO_CHAR);
    GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))) = 0;
  } else if (!str_cmp(one, "scan")) {
    act("You set $p to scanning mode.", FALSE, ch, radio, 0, TO_CHAR);
    GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))) = -1;
    WAIT_STATE(ch, 16); /* Takes time to switch it */
  } else if (!str_cmp(one, "center")) {
    i = atoi(two);
    if (i > 24)
      act("$p cannot center a frequency higher than 24 MHz.", FALSE, ch, radio,
          0, TO_CHAR);
    else if (i < 6)
      act("$p cannot center a frequency lower than 6 MHz.", FALSE, ch, radio,
          0, TO_CHAR);
    else {
      sprintf(buf, "$p is now centered at %d MHz.", i);
      act(buf, FALSE, ch, radio, 0, TO_CHAR);
      GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))) = i;
      WAIT_STATE(ch, 16); /* Takes time to adjust */
    }
  } else if (!str_cmp(one, "crypt")) {
    if ((i = atoi(two)))
      if (i > (GET_OBJ_VAL(radio, (cyberware ? 5 : (vehicle ? 3 : 2)))))
        act("$p's crypt mode isn't that high.", FALSE, ch, radio, 0, TO_CHAR);
      else {
        send_to_char("Crypt mode enabled.\r\n", ch);
        GET_OBJ_VAL(radio, (cyberware ? 6 : (vehicle ? 5 : 3))) = i;
      }
    else {
      if (!GET_OBJ_VAL(radio, (cyberware ? 6 : (vehicle ? 5 : 3))))
        act("$p's crypt mode is already disabled.", FALSE, ch, radio, 0, TO_CHAR);
      else {
        send_to_char("Crypt mode disabled.\r\n", ch);
        GET_OBJ_VAL(radio, (cyberware ? 6 : (vehicle ? 5 : 3))) = 0;
      }
    }
  } else
    send_to_char("That's not a valid option.\r\n", ch);
}

ACMD(do_broadcast)
{
  struct obj_data *obj, *radio = NULL;
  struct descriptor_data *d;
  int i, j, frequency, to_room, crypt, decrypt, success, suc;
  char buf3[MAX_STRING_LENGTH], buf4[MAX_STRING_LENGTH], voice[16] = "$v"; 
  bool cyberware = FALSE, vehicle = FALSE;
  if (PLR_FLAGGED(ch, PLR_AUTH)) {
    send_to_char("You must be Authorized to do that.\r\n", ch);
    return;
  }
  if (IS_ASTRAL(ch)) {
    send_to_char("You can't manipulate electronics from the astral plane.\r\n", ch);
    return;
  }

  if (ch->in_veh && (radio = GET_MOD(ch->in_veh, MOD_RADIO)))
    vehicle = TRUE;

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
  for (obj = ch->cyberware; obj; obj = obj->next_content)
    if (GET_OBJ_VAL(obj, 0) == CYB_RADIO && !radio) {
      radio = obj;
      cyberware = 1;
    } else if (GET_OBJ_VAL(obj, 0) == CYB_VOICEMOD && GET_OBJ_VAL(obj, 3))
      sprintf(voice, "A masked voice");

  if (IS_NPC(ch) || IS_SENATOR(ch)) {
    argument = any_one_arg(argument, arg);
    if (!str_cmp(arg,"all") && !IS_NPC(ch))
      frequency = -1;
    else {
      frequency = atoi(arg);
      if (frequency < 6 || frequency > 24) {
        send_to_char("Frequency must range between 6 and 24.\r\n", ch);
        return;
      }
    }
  } else if (!radio) {
    send_to_char("You have to have a radio to do that!\r\n", ch);
    return;
  } else if (!GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0)))) {
    act("$p must be on in order to broadcast.", FALSE, ch, radio, 0, TO_CHAR);
    return;
  } else if (GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0))) == -1) {
    act("$p can't broadcast while scanning.", FALSE, ch, radio, 0, TO_CHAR);
    return;
  } else
    frequency = GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0)));

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

  if (radio && GET_OBJ_VAL(radio, (cyberware ? 6 : (vehicle ? 5 : 3))))
    crypt = GET_OBJ_VAL(radio, (cyberware ? 6 : (vehicle ? 5 : 3)));
  else
    crypt = 0;

  success = success_test(GET_SKILL(ch, GET_LANGUAGE(ch)), 4);

  strcpy(buf4, argument);
  // Starting at end of buf, work backwards and fuzz out the message.
  for (int len = strlen(buf4) - 1; len >= 0; len--)
    switch (number(0, 2)) {
    case 1:
      buf4[len] = '.';
      break;
    case 2:
      buf4[len] = ' ';
      break;
    }
  sprintf(buf3, "*static* %s", buf4);
  if (ROOM_FLAGGED(ch->en_room, ROOM_NO_RADIO))
    strcpy(argument, buf3);

  
  if ( frequency > 0 ) {
    if(crypt) {
      sprintf(buf, "^y\\%s^y/[%d MHz, %s](CRYPTO-%d): %s^N", voice, frequency, skills[GET_LANGUAGE(ch)].name, crypt, argument);
      sprintf(buf2, "^y\\Garbled Static^y/[%d MHz, Unknown](CRYPTO-%d): ***ENCRYPTED DATA***^N", frequency, crypt);
      sprintf(buf4, "^y\\Unintelligible Voice^y/[%d MHz, Unknown](CRYPTO-%d): %s", frequency, crypt, buf3);
      sprintf(buf3, "^y\\%s^y/[%d MHz, Unknown](CRYPTO-%d): (something incoherent...)^N", voice, frequency, crypt);
    } else {
      sprintf(buf, "^y\\%s^y/[%d MHz, %s]: %s^N", voice, frequency, skills[GET_LANGUAGE(ch)].name, argument);
      sprintf(buf2, "^y\\%s^y/[%d MHz, Unknown]: %s^N", voice, frequency, argument);
      sprintf(buf4, "^y\\Unintelligible Voice^y/[%d MHz, Unknown]: %s", frequency, buf3);
      sprintf(buf3, "^y\\%s^y/[%d MHz, Unknown]: (something incoherent...)^N", voice, frequency);
    }

  } else {
    if(crypt) {
      sprintf(buf, "^y\\%s^y/[All Frequencies, %s](CRYPTO-%d): %s^N", voice, skills[GET_LANGUAGE(ch)].name, crypt, argument);
      sprintf(buf2, "^y\\Garbled Static^y/[All Frequencies, Unknown](CRYPTO-%d): ***ENCRYPTED DATA***^N", crypt);
      sprintf(buf4, "^y\\Unintelligible Voice^y/[All Frequencies, Unknown](CRYPTO-%d): %s", crypt, buf3);
      sprintf(buf3, "^y\\%s^y/[All Frequencies, Unknown](CRYPTO-%d): (something incoherent...)^N", voice, crypt);
    } else {
      sprintf(buf, "^y\\%s^y/[All Frequencies, %s]: %s^N", voice, skills[GET_LANGUAGE(ch)].name, argument);
      sprintf(buf2, "^y\\%s^y/[All Frequencies, Unknown]: %s^N", voice, argument);
      sprintf(buf4, "^y\\Unintelligible Voice^y/[All Frequencies, Unknown]: %s", buf3);
      sprintf(buf3, "^y\\%s^y/[All Frequencies, Unknown]: (something incoherent...)^N", voice);
    }
  }


  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else
    store_message_to_history(ch->desc, COMM_CHANNEL_RADIO, str_dup(act(buf, FALSE, ch, 0, 0, TO_CHAR)));
  if (!ROOM_FLAGGED(ch->en_room, ROOM_SOUNDPROOF))
    for (d = descriptor_list; d; d = d->next) {
      if (!d->connected && d != ch->desc && d->character &&
          !PLR_FLAGS(d->character).AreAnySet(PLR_WRITING,
                                             PLR_MAILING,
                                             PLR_EDITING, PLR_MATRIX, ENDBIT)
          && !IS_PROJECT(d->character) &&
          !ROOM_FLAGGED(d->character->en_room, ROOM_SOUNDPROOF) &&
          !ROOM_FLAGGED(d->character->en_room, ROOM_SENT)) {
        if (!IS_NPC(d->character) && !IS_SENATOR(d->character)) {
          radio = NULL;
          cyberware = FALSE;
          vehicle = FALSE;

          if (d->character->in_veh && (radio = GET_MOD(d->character->in_veh, MOD_RADIO)))
            vehicle = TRUE;

          for (obj = d->character->cyberware; obj && !radio; obj = obj->next_content)
            if (GET_OBJ_VAL(obj, 0) == CYB_RADIO) {
              radio = obj;
              cyberware = 1;
            }

          for (obj = d->character->carrying; obj && !radio; obj = obj->next_content)
            if (GET_OBJ_TYPE(obj) == ITEM_RADIO)
              radio = obj;

          for (i = 0; !radio && i < NUM_WEARS; i++)
            if (GET_EQ(d->character, i)) {
              if (GET_OBJ_TYPE(GET_EQ(d->character, i)) == ITEM_RADIO)
                radio = GET_EQ(d->character, i);
              else if (GET_OBJ_TYPE(GET_EQ(d->character, i)) == ITEM_WORN
                       && GET_EQ(d->character, i)->contains)
                for (struct obj_data *obj2 = GET_EQ(d->character, i)->contains;
                     obj2; obj2 = obj2->next_content)
                  if (GET_OBJ_TYPE(obj2) == ITEM_RADIO)
                    radio = obj2;
            }
          for (obj = get_ch_en_room(ch)->contents; obj && !radio;
               obj = obj->next_content)
            if (GET_OBJ_TYPE(obj) == ITEM_RADIO && !CAN_WEAR(obj, ITEM_WEAR_TAKE))
              radio = obj;

          if (radio) {
            suc = success_test(GET_SKILL(d->character, GET_LANGUAGE(ch)), 4);
            if (CAN_WEAR(radio, ITEM_WEAR_EAR) || cyberware || vehicle)
              to_room = 0;
            else
              to_room = 1;

            i = GET_OBJ_VAL(radio, (cyberware ? 3 : (vehicle ? 4 : 0)));
            j = GET_OBJ_VAL(radio, (cyberware ? 5 : (vehicle ? 2 : 1)));
            decrypt = GET_OBJ_VAL(radio, (cyberware ? 5 : (vehicle ? 3 : 2)));
            if (i == 0 || ((i != -1 && frequency != -1) && !(frequency >= (i - j) && frequency <= (i + j))))
              continue;
            if (decrypt >= crypt) {
              if (to_room) {
                if (success > 0 || IS_NPC(ch))
                  if (suc > 0 || IS_NPC(ch))
                    if (ROOM_FLAGGED(d->character->en_room, ROOM_NO_RADIO))
                      store_message_to_history(d, COMM_CHANNEL_RADIO, str_dup(act(buf4, FALSE, ch, 0, d->character, TO_VICT)));
                    else
                      store_message_to_history(d, COMM_CHANNEL_RADIO, str_dup(act(buf, FALSE, ch, 0, d->character, TO_VICT)));
                  else
                    store_message_to_history(d, COMM_CHANNEL_RADIO, str_dup(act(buf3, FALSE, ch, 0, d->character, TO_VICT)));
                else
                  store_message_to_history(d, COMM_CHANNEL_RADIO, str_dup(act(buf3, FALSE, ch, 0, d->character, TO_VICT)));
              } else {
                if (success > 0 || IS_NPC(ch))
                  if (suc > 0 || IS_NPC(ch))
                    if (ROOM_FLAGGED(d->character->en_room, ROOM_NO_RADIO))
                      store_message_to_history(d, COMM_CHANNEL_RADIO, str_dup(act(buf4, FALSE, ch, 0, d->character, TO_VICT)));
                    else 
                      store_message_to_history(d, COMM_CHANNEL_RADIO, str_dup(act(buf, FALSE, ch, 0, d->character, TO_VICT)));
                  else
                    store_message_to_history(d, COMM_CHANNEL_RADIO, str_dup(act(buf3, FALSE, ch, 0, d->character, TO_VICT)));
                else
                  store_message_to_history(d, COMM_CHANNEL_RADIO, str_dup(act(buf3, FALSE, ch, 0, d->character, TO_VICT)));
              }
            } else
              store_message_to_history(d, COMM_CHANNEL_RADIO, str_dup(act(buf2, FALSE, ch, 0, d->character, TO_VICT)));
          }
        } else if (IS_SENATOR(d->character) && !PRF_FLAGGED(d->character, PRF_NORADIO))
          store_message_to_history(d, COMM_CHANNEL_RADIO, str_dup(act(buf, FALSE, ch, 0, d->character, TO_VICT)));
      }
    }

  for (d = descriptor_list; d; d = d->next)
    if (!d->connected &&
        d->character &&
        ROOM_FLAGGED(d->character->en_room, ROOM_SENT))
      ROOM_FLAGS(d->character->en_room).RemoveBit(ROOM_SENT);
}

/**********************************************************************
 * generalized communication func, originally by Fred C. Merkel (Torg) *
  *********************************************************************/

extern int _NO_OOC_;

ACMD(do_gen_comm)
{
  struct veh_data *veh;
  struct descriptor_data *i;
  struct descriptor_data *d;
  int channel = 0;
  char *str_to_add_return_to = NULL;

  static int channels[] = {
                            PRF_DEAF,
                            PRF_NONEWBIE,
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
  static const char *com_msgs[][6] = {
                                       {"You cannot shout!!\r\n",
                                        "shout",
                                        "Turn off your noshout flag first!\r\n",
                                        "^Y",
                                        B_YELLOW
                                       },

                                       {"You can't use the newbie channel!\r\n",
                                        "newbie",
                                        "You've turned that channel off!\r\n",
                                        "^G",
                                        B_GREEN},

                                       {"You can't use the OOC channel!\r\n",
                                        "ooc",
                                        "You have the OOC channel turned off.\n\r",
                                        "^n",
                                        KNUL},

                                       {"You can't use the RPE channel!\r\n",
                                        "rpe",
                                        "You have the RPE channel turned off.\r\n",
                                        B_RED},

                                       {"You can't use the hired channel!\r\n",
                                        "hired",
                                        "You have the hired channel turned off.\r\n",
                                        B_YELLOW}
                                     };

  /* to keep pets, etc from being ordered to shout */
  if (!ch->desc && !MOB_FLAGGED(ch, MOB_SPEC))
    return;

  if(PLR_FLAGGED(ch, PLR_AUTH) && subcmd != SCMD_NEWBIE) {
    send_to_char(ch, "You must be Authorized to use that command.\r\n");
    return;
  }

  if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
    return;

  if (IS_ASTRAL(ch)) {
    send_to_char(ch, "Astral beings cannot %s.\r\n", com_msgs[subcmd][1]);
    return;
  }

  if ( _NO_OOC_ && subcmd == SCMD_OOC ) {
    send_to_char("This command has been disabled.\n\r",ch);
    return;
  }

  if ((PLR_FLAGGED(ch, PLR_NOSHOUT) && subcmd == SCMD_SHOUT) ||
      (PLR_FLAGGED(ch, PLR_NOOOC) && subcmd == SCMD_OOC) ||
      (!(PLR_FLAGGED(ch, PLR_RPE) || IS_SENATOR(ch))  && subcmd == SCMD_RPETALK) ||
      (!(PRF_FLAGGED(ch, PRF_QUEST) || IS_SENATOR(ch))  && subcmd == SCMD_HIREDTALK)) {
    send_to_char(com_msgs[subcmd][0], ch);
    return;
  }

  if (ROOM_FLAGGED(ch->en_room, ROOM_SOUNDPROOF) && subcmd == SCMD_SHOUT) {
    send_to_char("The walls seem to absorb your words.\r\n", ch);
    return;
  }

  /* make sure the char is on the channel */
  if (PRF_FLAGGED(ch, channels[subcmd])) {
    send_to_char(com_msgs[subcmd][2], ch);
    return;
  }

  if (subcmd == SCMD_NEWBIE) {
    if (IS_NPC(ch)) {
      send_to_char("No.\r\n", ch);
      return;
    } else if (!IS_SENATOR(ch) && !PLR_FLAGGED(ch, PLR_NEWBIE) && !PRF_FLAGGED(ch, PRF_NEWBIEHELPER)) {
      send_to_char("You are too experienced to use the newbie channel.\r\n", ch);
      return;
    }
  }

  skip_spaces(&argument);

  /* make sure that there is something there to say! */
  if (!*argument) {
    sprintf(buf1, "Yes, %s, fine, %s we must, but WHAT???\r\n", com_msgs[subcmd][1], com_msgs[subcmd][1]);
    send_to_char(buf1, ch);
    return;
  }

  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else if (subcmd == SCMD_SHOUT) {
    struct room_data *was_in;
    struct char_data *tmp;
    int success = success_test(GET_SKILL(ch, GET_LANGUAGE(ch)), 4);
    for (tmp = get_ch_en_room(ch)->people; tmp; tmp = tmp->next_en_room)
      if (tmp != ch) {
        if (success > 0) {
          int suc = success_test(GET_SKILL(tmp, GET_LANGUAGE(ch)), 4);
          if (suc > 0 || IS_NPC(tmp))
            sprintf(buf, "%s$z shouts in %s, \"%s\"^N", com_msgs[subcmd][3], skills[GET_LANGUAGE(ch)].name, argument);
          else
            sprintf(buf, "%s$z shouts in a language you don't understand.", com_msgs[subcmd][3]);
        } else
          sprintf(buf, "$z shouts incoherently.");
        if (IS_NPC(ch))
          sprintf(buf, "%s$z shouts, \"%s^n\"", com_msgs[subcmd][3], argument);
        
        // Note that this line invokes act().
        store_message_to_history(tmp->desc, COMM_CHANNEL_SHOUTS, str_dup(act(buf, FALSE, ch, NULL, tmp, TO_VICT)));
      }

    sprintf(buf1, "%sYou shout, '%s'^N", com_msgs[subcmd][3], argument);
    // Note that this line invokes act().
    store_message_to_history(ch->desc, COMM_CHANNEL_SHOUTS, str_dup(act(buf1, FALSE, ch, 0, 0, TO_CHAR)));

    if (ch->in_veh && ch->in_veh->en_room != NULL) {
      was_in = ch->in_veh->en_room;
      ch->en_room = was_in;
      sprintf(buf1, "%sFrom inside %s, $z %sshouts, '%s'^N", com_msgs[subcmd][3], GET_VEH_NAME(ch->in_veh),
              com_msgs[subcmd][3], argument);
      for (tmp = get_ch_en_room(ch)->people; tmp; tmp = tmp->next) {
        // Replicate act() in a way that lets us capture the message.
        if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
          // They're a valid target, so send the message with a raw perform_act() call.
          store_message_to_history(tmp->desc, COMM_CHANNEL_SHOUTS, str_dup(perform_act(buf1, ch, NULL, NULL, tmp)));
        }
      }
    } else {
      was_in = ch->en_room;
      for (veh = get_ch_en_room(ch)->vehicles; veh; veh = veh->next_veh) {
        ch->in_veh = veh;
        for (tmp = ch->in_veh->people; tmp; tmp = tmp->next_in_veh) {
          // Replicate act() in a way that lets us capture the message.
          if (can_send_act_to_target(ch, FALSE, NULL, NULL, tmp, TO_ROOM)) {
            // They're a valid target, so send the message with a raw perform_act() call.
            store_message_to_history(tmp->desc, COMM_CHANNEL_SHOUTS, str_dup(perform_act(buf, ch, NULL, NULL, tmp)));
          }
        }
        ch->in_veh = NULL;
      }
    }

    for (int door = 0; door < NUM_OF_DIRS; door++)
      if (CAN_GO(ch, door)) {
        ch->en_room = &world[was_in->dir_option[door]->to_room];
        for (tmp = get_ch_en_room(ch)->people; tmp; tmp = tmp->next_en_room)
          if (tmp != ch) {
            if (success > 0) {
              int suc = success_test(GET_SKILL(tmp, GET_LANGUAGE(ch)), 4);
              if (suc > 0 || IS_NPC(tmp))
                sprintf(buf, "%s$z shouts in %s, \"%s\"^N", com_msgs[subcmd][3], skills[GET_LANGUAGE(ch)].name, argument);
              else
                sprintf(buf, "%s$z shouts in a language you don't understand.", com_msgs[subcmd][3]);
            } else
              sprintf(buf, "$z shouts incoherently.");
            if (IS_NPC(ch))
              sprintf(buf, "%s$z shouts, \"%s^n\"", com_msgs[subcmd][3], argument);
            
            // If it sent successfully, store to their history.
            store_message_to_history(tmp->desc, COMM_CHANNEL_SHOUTS, str_dup(act(buf, FALSE, ch, NULL, tmp, TO_VICT)));
          }
        ch->en_room = was_in;
      }
    if (ch->in_veh)
      ch->en_room = NULL;

    return;
  } else if(subcmd == SCMD_OOC) {
    /* Check for non-switched mobs */
    if ( IS_NPC(ch) && ch->desc == NULL )
      return;
    delete_doubledollar(argument);
    for ( d = descriptor_list; d != NULL; d = d->next ) {
      if ( !d->character || ( d->connected != CON_PLAYING && !PRF_FLAGGED(d->character, PRF_MENUGAG)) ||
           PLR_FLAGGED( d->character, PLR_WRITING) ||
           PRF_FLAGGED( d->character, PRF_NOOOC) || PLR_FLAGGED(d->character, PLR_AUTH) || found_mem(GET_IGNORE(d->character), ch))
        continue;

      if (!access_level(d->character, GET_INCOG_LEV(ch)))
        sprintf(buf, "^m[^nSomeone^m]^n ^R(^nOOC^R)^n, \"%s^n\"\r\n", argument );
      else
        sprintf(buf, "^m[^n%s^m]^n ^R(^nOOC^R)^n, \"%s^n\"\r\n", IS_NPC(ch) ? GET_NAME(ch) : GET_CHAR_NAME(ch), argument );
      
      store_message_to_history(d, COMM_CHANNEL_OOC, str_dup(buf));
      send_to_char(buf, d->character);
    }

    return;
  } else if (subcmd == SCMD_RPETALK) {
    sprintf(buf, "%s%s ^W[^rRPE^W]^r %s^n", com_msgs[subcmd][3], GET_CHAR_NAME(ch), argument);
    send_to_char(ch, "%s\r\n", buf);
    
    channel = COMM_CHANNEL_RPE;
    str_to_add_return_to = str_dup(buf);
    strcat(str_to_add_return_to, "\r\n");
    store_message_to_history(ch->desc, channel, str_to_add_return_to);
  } else if (subcmd == SCMD_HIREDTALK) {
    sprintf(buf, "%s%s ^y[^YHIRED^y]^Y %s^n", com_msgs[subcmd][3], GET_CHAR_NAME(ch), argument);
    send_to_char(ch, "%s\r\n", buf);
    channel = COMM_CHANNEL_HIRED;
    str_to_add_return_to = str_dup(buf);
    strcat(str_to_add_return_to, "\r\n");
    store_message_to_history(ch->desc, channel, str_to_add_return_to);
  } else {
    sprintf(buf, "%s%s |]newbie[| %s^N", com_msgs[subcmd][3], GET_CHAR_NAME(ch), argument);
    send_to_char(ch, "%s\r\n", buf);
    channel = COMM_CHANNEL_NEWBIE;
    str_to_add_return_to = str_dup(buf);
    strcat(str_to_add_return_to, "\r\n");
    store_message_to_history(ch->desc, channel, str_to_add_return_to);
  }

  /* now send all the strings out */
  for (i = descriptor_list; i; i = i->next)
    if (!i->connected && i != ch->desc && i->character &&
        !PRF_FLAGGED(i->character, channels[subcmd]) &&
        !PLR_FLAGS(i->character).AreAnySet(PLR_WRITING,
                                           PLR_MAILING,
                                           PLR_EDITING, ENDBIT) &&
        !IS_PROJECT(i->character) &&
        !(ROOM_FLAGGED(i->character->en_room, ROOM_SOUNDPROOF) && subcmd == SCMD_SHOUT)) {
      if (subcmd == SCMD_NEWBIE && !(PLR_FLAGGED(i->character, PLR_NEWBIE) ||
                                     IS_SENATOR(i->character) || PRF_FLAGGED(i->character, PRF_NEWBIEHELPER)))
        continue;
      else if (subcmd == SCMD_RPETALK && !(PLR_FLAGGED(i->character, PLR_RPE) ||
                                           IS_SENATOR(i->character)))
        continue;
      else if (subcmd == SCMD_HIREDTALK && !(PRF_FLAGGED(i->character, PRF_QUEST) ||
                                             IS_SENATOR(i->character)))
        continue;
      else {
        // Note that this invokes act().
        store_message_to_history(i, channel, str_dup(act(buf, FALSE, ch, 0, i->character, TO_VICT | TO_SLEEP)));
      }
    }
}

ACMD(do_language)
{
  int i, lannum;
  one_argument(argument, arg);

  if (!*arg) {
    send_to_char("You know the following languages:\r\n", ch);
    for (i = SKILL_ENGLISH; i <= SKILL_FRENCH; i++)
      if ((GET_SKILL(ch, i)) > 0) {
        sprintf(buf, "%-20s %-17s", skills[i].name, how_good(GET_SKILL(ch, i)));
        if (GET_LANGUAGE(ch) == i)
          strcat(buf, " ^Y(Speaking)^n");
        strcat(buf, "\r\n");
        send_to_char(buf, ch);
      }
    return;
  }

  if ((lannum = find_skill_num(arg)) && (lannum >= SKILL_ENGLISH && lannum <= SKILL_FRENCH))
    if (GET_SKILL(ch, lannum) > 0) {
      GET_LANGUAGE(ch) = lannum;
      sprintf(buf, "You will now speak %s.\r\n", skills[lannum].name);
      send_to_char(buf, ch);
    } else
      send_to_char("You don't know how to speak that language.\r\n", ch);
  else
    send_to_char("Invalid Language.\r\n", ch);

}

void add_phone_to_list(struct obj_data *obj)
{
  struct phone_data *k;
  bool found = FALSE, cyber = FALSE;
  if (GET_OBJ_TYPE(obj) == ITEM_CYBERWARE)
    cyber = TRUE;
  sprintf(buf, "%04d%04d", GET_OBJ_VAL(obj, (cyber ? 3 : 0)), GET_OBJ_VAL(obj, (cyber ? 6 : 1)));
  for (struct phone_data *j = phone_list; j; j = j->next)
    if (j->number == atoi(buf)) {
      found = TRUE;
      break;
    }
  if (!found) {
    k = new phone_data;
    k->number = atoi(buf);
    k->phone = obj;
    if (phone_list)
      k->next = phone_list;
    phone_list = k;
  }  
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
      send_to_char("Ring what number?", ch);
      return;
    }
    if (!(ring = atoi(arg))) {
      send_to_char("That is not a valid number.\r\n", ch);
      return;
    }

    for (k = phone_list; k; k = k->next)
      if (k->number == ring)
        break;

    if (!k) {
      send_to_char("The phone is picked up and a polite female voice says, \"This number is not in"
                   " service, please check your number and try again.\"\r\n", ch);
      return;
    }
    if (k == phone) {
      send_to_char("Why would you want to call yourself?\r\n", ch);
      return;
    }
    if (k->dest) {
      send_to_char("You hear the busy signal.\r\n", ch);
      return;
    }
    phone->dest = k;
    phone->connected = TRUE;
    k->dest = phone;

    if (k->persona) {
      send_to_icon(k->persona, "A small telephone symbol blinks in the top left of your view.\r\n");
    } else {
      tch = k->phone->carried_by ? k->phone->carried_by : k->phone->worn_by;
      if (!tch && k->phone->in_obj)
        tch = k->phone->in_obj->carried_by ? k->phone->in_obj->carried_by : k->phone->in_obj->worn_by;
      if (tch) {
        if (GET_POS(tch) == POS_SLEEPING) {
          if (success_test(GET_WIL(tch), 4)) { // Why does it take a successful Willpower test to hear your phone?
            GET_POS(tch) = POS_RESTING;
            send_to_char("You are woken by your phone ringing.\r\n", tch);
            if (!GET_OBJ_VAL(k->phone, 3))
              act("$n is startled awake by the ringing of $s phone.", FALSE, tch, 0, 0, TO_ROOM);
          } else if (!GET_OBJ_VAL(k->phone, 3))
            act("$n's phone rings.", FALSE, tch, 0, 0, TO_ROOM);
        } else if (!GET_OBJ_VAL(k->phone, 3)) {
          act("Your phone rings.", FALSE, tch, 0, 0, TO_CHAR);
          act("$n's phone rings.", FALSE, tch, NULL, NULL, TO_ROOM);
        } else {
          if (GET_OBJ_TYPE(k->phone) == ITEM_CYBERWARE || success_test(GET_INT(tch), 4))
            act("You feel your phone ring.", FALSE, tch, 0, 0, TO_CHAR);
        }
      } else {
        sprintf(buf, "%s rings.", GET_OBJ_NAME(k->phone));
        if (k->phone->en_room || k->phone->in_veh)
          act(buf, FALSE, NULL, k->phone, 0, TO_ROOM);
        // Edge case: A phone inside a container inside a container won't ring. But do we even want it to?
      }
    }
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
      send_to_icon(phone->dest->persona, "The flashing phone icon fades from view.\r\n");
    else {
      tch = phone->dest->phone->carried_by;
      if (!tch)
        tch = phone->dest->phone->worn_by;
      if (!tch && phone->dest->phone->in_obj)
        tch = phone->dest->phone->in_obj->carried_by;
      if (!tch && phone->phone->in_obj)
        tch = phone->dest->phone->in_obj->worn_by;
      if (tch) {
        if (phone->dest->connected)
          send_to_char("The phone is hung up from the other side.\r\n", tch);
        else
          act("Your phone stops ringing.\r\n", FALSE, tch, 0, 0, TO_CHAR);
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
    char voice[20] = "$v";
    for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content)
      if (GET_OBJ_VAL(obj, 0) == CYB_VOICEMOD && GET_OBJ_VAL(obj, 3))
        sprintf(voice, "A masked voice");
    
    if (success_test(GET_SKILL(ch, GET_LANGUAGE(ch)), 4) > 0) {
      sprintf(buf, "^Y%s on the other end of the line says in %s, \"%s\"", voice, skills[GET_LANGUAGE(ch)].name, argument);
      sprintf(buf2, "$z says into $s phone in %s, \"%s\"", skills[GET_LANGUAGE(ch)].name, argument);
    } else {
      sprintf(buf, "^Y$v on the other end of the line mumbles incoherently.");
      sprintf(buf2, "$z mumbles incoherently into $s phone.\r\n");
    }
    sprintf(buf3, "^YYou say, \"%s\"\r\n", argument);
    send_to_char(ch, buf3);
    store_message_to_history(ch->desc, COMM_CHANNEL_PHONE, str_dup(buf3));
    if (phone->dest->persona && phone->dest->persona->decker && phone->dest->persona->decker->ch)
      store_message_to_history(ch->desc, COMM_CHANNEL_PHONE, str_dup(act(buf, FALSE, ch, 0, phone->dest->persona->decker->ch, TO_DECK)));
    else {
      tch = phone->dest->phone->carried_by;
      if (!tch)
        tch = phone->dest->phone->worn_by;
      if (!tch && phone->dest->phone->in_obj)
        tch = phone->dest->phone->in_obj->carried_by;
      if (!tch && phone->dest->phone->in_obj)
        tch = phone->dest->phone->in_obj->worn_by;
    }
    if (tch) {
      if (success_test(GET_SKILL(tch, GET_LANGUAGE(ch)), 4) > 0)
        store_message_to_history(tch->desc, COMM_CHANNEL_PHONE, str_dup(act(buf, FALSE, ch, 0, tch, TO_VICT)));
      else
        store_message_to_history(tch->desc, COMM_CHANNEL_PHONE, str_dup(act("^Y$v speaks in a language you don't understand.", FALSE, ch, 0, tch, TO_VICT)));
    }
    if (!cyber) {
      for (tch = ch->in_veh ? ch->in_veh->people : get_ch_en_room(ch)->people; tch; tch = ch->in_veh ? tch->next_in_veh : tch->next_en_room)
        if (tch != ch) {
          if (success_test(GET_SKILL(tch, GET_LANGUAGE(ch)), 4) > 0)
            store_message_to_history(tch->desc, COMM_CHANNEL_SAYS, str_dup(act(buf2, FALSE, ch, 0, tch, TO_VICT)));
          else
            store_message_to_history(tch->desc, COMM_CHANNEL_SAYS, str_dup(act("$z speaks into $s phone in a language you don't understand.", FALSE, ch, 0, tch, TO_VICT)));
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
          sprintf(buf, "%s would have crashed the mud, whats up with their phone.", GET_CHAR_NAME(ch));
          mudlog(buf, ch, LOG_WIZLOG, TRUE);
          return;
        }
        REMOVE_FROM_LIST(phone, phone_list, next);
        DELETE_AND_NULL(phone);
      }
      return;
    }
    sprintf(buf, "%s:\r\n", GET_OBJ_NAME(obj));
    sprintf(ENDOF(buf), "Phone Number: %04d-%04d\r\n", GET_OBJ_VAL(obj, (cyber ? 3 : 0)),
            GET_OBJ_VAL(obj, (cyber ? 6 : 1)));
    sprintf(ENDOF(buf), "Switched: %s\r\n", GET_OBJ_VAL(obj, (cyber ? 7 : 2)) ? "On" : "Off");
    sprintf(ENDOF(buf), "Ringing: %s\r\n", GET_OBJ_VAL(obj, (cyber ? 8 : 3)) ? "Off": "On");
    if (phone && phone->dest) {
      if (phone->dest->connected && phone->connected)
        sprintf(ENDOF(buf), "Connected to: %d\r\n", phone->dest->number);
      else if (!phone->dest->connected)
        sprintf(ENDOF(buf), "Calling: %d\r\n", phone->dest->number);
      else sprintf(ENDOF(buf), "Incoming call from: %08d\r\n", phone->dest->number);
    }
    send_to_char(buf, ch);
  }
}


ACMD(do_phonelist)
{
  struct phone_data *k;
  struct char_data *tch = NULL;
  int i = 0;

  if (!phone_list)
    send_to_char(ch, "The phone list is empty.\r\n");

  for (k = phone_list; k; k = k->next) {
    if (k->persona && k->persona->decker) {
      tch = k->persona->decker->ch;
    } else if (k->phone) {
      tch = k->phone->carried_by;
      if (!tch)
        tch = k->phone->worn_by;
      if (!tch)
        tch = k->phone->worn_by;
      if (!tch && k->phone->in_obj)
        tch = k->phone->in_obj->carried_by;
      if (!tch && k->phone->in_obj)
        tch = k->phone->in_obj->worn_by;
    }
    send_to_char(ch, "%2d) %d (%s) (%s)\r\n", i, k->number, (k->dest ? "Busy" : "Free"),
            (tch ? (IS_NPC(tch) ? GET_NAME(tch) : GET_CHAR_NAME(tch)) : "no one"));
    i++;
  }
}

void phone_check()
{
  struct char_data *tch;
  struct phone_data *k;
  for (k = phone_list; k; k = k->next) {
    if (k->dest && !k->connected) {
      if (k->persona) {
        send_to_icon(k->persona, "A small telephone symbol blinks in the top left of your view.\r\n");
        continue;
      }
      tch = k->phone->carried_by;
      if (!tch)
        tch = k->phone->worn_by;
      if (!tch && k->phone->in_obj)
        tch = k->phone->in_obj->carried_by;
      if (!tch && k->phone->in_obj)
        tch = k->phone->in_obj->worn_by;
      if (tch) {
        if (GET_POS(tch) == POS_SLEEPING) {
          if (GET_POS(tch) == POS_SLEEPING) {
            if (success_test(GET_WIL(tch), 4)) { // Why does it take a successful Willpower test to hear your phone?
              GET_POS(tch) = POS_RESTING;
              send_to_char("You are woken by your phone ringing.\r\n", tch);
              if (!GET_OBJ_VAL(k->phone, 3))
                act("$n is startled awake by the ringing of $s phone.", FALSE, tch, 0, 0, TO_ROOM);
            } else if (!GET_OBJ_VAL(k->phone, 3))
              act("$n's phone rings.", FALSE, tch, 0, 0, TO_ROOM);
          } else if (!GET_OBJ_VAL(k->phone, 3)) {
            act("$n's phone rings.", FALSE, tch, 0, 0, TO_ROOM);
            continue;
          } else
            continue;
        }
        if (!GET_OBJ_VAL(k->phone, 3)) {
          act("Your phone rings.", FALSE, tch, 0, 0, TO_CHAR);
          act("$n's phone rings.", FALSE, tch, 0, 0, TO_ROOM);
        } else {
          if (GET_OBJ_TYPE(k->phone) == ITEM_CYBERWARE || success_test(GET_INT(tch), 4))
            act("You feel your phone ring.", FALSE, tch, 0, 0, TO_CHAR);
        }
      } else {
        sprintf(buf, "%s rings.", GET_OBJ_NAME(k->phone));
        if (k->phone->en_room || k->phone->in_veh)
          act(buf, FALSE, NULL, k->phone, 0, TO_ROOM);
      }
    }
  }
}

ACMD(do_ignore)
{
  if (!*argument) {
    send_to_char("You are currently ignoring: \r\n", ch);
    for (struct remem *a = GET_IGNORE(ch); a; a = a->next) {
      char *name = get_player_name(a->idnum);
      if (name) {
        send_to_char(ch, "%s\r\n", name);
        DELETE_AND_NULL_ARRAY(name);
      }
    }
  } else {
    struct remem *list;
    skip_spaces(&argument);
    if (struct char_data *tch = get_player_vis(ch, argument, FALSE)) {
      if (GET_LEVEL(tch) > LVL_MORTAL)
        send_to_char("You can't ignore immortals.\r\n", ch);
      else if ((list = found_mem(GET_IGNORE(ch), tch))) {
        struct remem *temp;
        REMOVE_FROM_LIST(list, GET_IGNORE(ch), next);
        DELETE_AND_NULL(list);
        send_to_char(ch, "You can now hear %s.\r\n", GET_CHAR_NAME(tch));
      } else {
        list = new remem;
        list->idnum = GET_IDNUM(tch);
        list->next = GET_IGNORE(ch);
        GET_IGNORE(ch) = list;
        send_to_char(ch, "You will no longer hear %s.\r\n", GET_CHAR_NAME(tch));
      }
    } else send_to_char("They are not logged on.\r\n", ch);
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
    sprintf(buf, "SYSERR: Channel %d is not within bounds 0 <= channel < %d.", channel, NUM_COMMUNICATION_CHANNELS);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return;
  }
  
  if (maximum > NUM_MESSAGES_TO_RETAIN) {
    sprintf(buf, "SYSERR: send_message_history_to_descriptor asked to send %d messages, but max message history is %d.",
            maximum, NUM_MESSAGES_TO_RETAIN);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return;
  }
  
  if (maximum <= 0) {
    sprintf(buf, "SYSERR: send_message_history_to_descriptor asked to send %d messages, but minimum is 1.", maximum);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return;
  }
  
  if (d->message_history[channel].NumItems() == 0) {
    sprintf(buf, "You haven't heard any messages %s yet.\r\n", channel_string);
    write_to_output(buf, d);
    return;
  }
  
  int skip = d->message_history[channel].NumItems() - maximum;
  
  sprintf(buf, "The last %d messages you've heard %s are:\r\n", maximum ? maximum : NUM_MESSAGES_TO_RETAIN, channel_string);
  write_to_output(buf, d);
  
  // For every message in their history, print the list from oldest to newest.
  for (nodeStruct<const char*> *currnode = d->message_history[channel].Tail(); currnode; currnode = currnode->prev) {
    // If they've requested a maximum of X, then skip 20-X of the oldest messages before we start sending them.
    if (skip-- > 0)
      continue;
    
    sprintf(buf, "  %s", currnode->data);
    write_to_output(colorize(d, buf, TRUE), d);
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
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over the Hired Talk channel");
      break;
    case COMM_CHANNEL_NEWBIE:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over the Newbie channel");
      break;
    case COMM_CHANNEL_OOC:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over the OOC channel");
      break;
    case COMM_CHANNEL_OSAYS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "said OOCly");
      break;
    case COMM_CHANNEL_PAGES:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over pages");
      break;
    case COMM_CHANNEL_PHONE:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over the phone");
      break;
    case COMM_CHANNEL_RPE:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over the RPE Talk channel");
      break;
    case COMM_CHANNEL_RADIO:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over the radio");
      break;
    case COMM_CHANNEL_SAYS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "spoken aloud");
      break;
    case COMM_CHANNEL_SHOUTS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "from nearby shouts");
      break;
    case COMM_CHANNEL_TELLS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over tells");
      break;
    case COMM_CHANNEL_WTELLS:
      send_message_history_to_descriptor(ch->desc, channel, quantity, "over wiztells");
      break;
    default:
      sprintf(buf, "SYSERR: Unrecognized channel/subcmd %d provided to raw_message_history's channel switch.", channel);
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
  
  half_chop(argument, buf, buf2);
  
  // Find the channel referenced in first parameter.
  for (channel = 0; channel < NUM_COMMUNICATION_CHANNELS; channel++)
    if (is_abbrev(buf, message_history_channels[channel]))
      break;
  
  // No channel found? Fail.
  if (channel >= NUM_COMMUNICATION_CHANNELS) {
    send_to_char(ch, "Sorry, '%s' is not a recognized channel.\r\n", buf);
    display_message_history_help(ch);
    return;
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
