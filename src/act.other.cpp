/*************************************************************************
*   File: act.other.c                                   Part of CircleMUD *
*  Usage: Miscellaneous player-level commands                             *
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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "newdb.h"
#include "house.h"
#include "newmagic.h"
#include "olc.h"
#include "boards.h"
#include "screen.h"
#include "memory.h"
#include "awake.h"
#include "constants.h"
#include "bullet_pants.h"

#ifdef GITHUB_INTEGRATION
#include <curl/curl.h>
#include "github_config.h"
#endif

bool is_reloadable_weapon(struct obj_data *weapon, int ammotype);

/* extern variables */
extern const char *ctypes[];
extern int ident;
extern class memoryClass *Mem;

/* extern procedures */
ACMD_CONST(do_say);
SPECIAL(shop_keeper);
SPECIAL(spraypaint);
SPECIAL(johnson);
extern char *how_good(int skill, int percent);
extern void perform_tell(struct char_data *, struct char_data *, char *);
extern void obj_magic(struct char_data * ch, struct obj_data * obj, char *argument);
extern void end_quest(struct char_data *ch);
extern bool can_take_obj(struct char_data *ch, struct obj_data *obj);
extern char *short_object(int virt, int where);
extern bool read_extratext(struct char_data * ch);
extern int return_general(int skill_num);
extern int belongs_to(struct char_data *ch, struct obj_data *obj);
extern char *make_desc(struct char_data *ch, struct char_data *i, char *buf, int act, bool dont_capitalize_a_an);
extern void weight_change_object(struct obj_data * obj, float weight);

extern bool restring_with_args(struct char_data *ch, char *argument, bool using_sysp);

ACMD(do_quit)
{

  if (subcmd != SCMD_QUIT && !IS_SENATOR(ch)) {
    send_to_char("You have to type quit - no less, to quit!\r\n", ch);
    return;
  }
  void die(struct char_data * ch);
  struct descriptor_data *d, *next_d;

  if (IS_NPC(ch) || !ch->desc)
    return;
  
  if (ch->in_veh)
    ch->in_room = get_ch_in_room(ch);

  if (GET_POS(ch) == POS_FIGHTING)
    send_to_char("No way!  You're fighting for your life!\r\n", ch);
  else if (ROOM_FLAGGED(ch->in_room, ROOM_NOQUIT))
    send_to_char("You can't quit here!\r\n", ch);
  else if (GET_POS(ch) <= POS_STUNNED) {
    send_to_char("You die before your time...\r\n", ch);
    act("$n gives up the struggle to live...", TRUE, ch, 0, 0, TO_ROOM);
    die(ch);
  } else {
    GET_LAST_IN(ch) = GET_ROOM_VNUM(ch->in_room);
    struct room_data *save_room = ch->in_room;
    if (GET_QUEST(ch))
      end_quest(ch);
    
    if (ROOM_FLAGGED(save_room, ROOM_HOUSE) && House_can_enter(ch, save_room->number)) {
      // Only guests and owners can load back into an apartment.
      GET_LOADROOM(ch) = save_room->number;
    } else {
      // Your room was invalid for reloading in (not an apartment, etc).
      ch->in_room = save_room;
      if (!GET_LOADROOM(ch)) {
        if (PLR_FLAGGED(ch, PLR_NEWBIE))
          GET_LOADROOM(ch) = RM_NEWBIE_LOADROOM;
        else
          GET_LOADROOM(ch) = RM_ENTRANCE_TO_DANTES;
      }
    }
    
    /*
     * Get the last room they were in, in case they try to come in before the time
     * limit is up.
     */
    if (ROOM_FLAGGED(ch->in_room, ROOM_STAFF_ONLY) && !access_level(ch, LVL_BUILDER)) {
      // Quitting out in a staff-only area? You won't load back there.
      GET_LAST_IN(ch) = RM_ENTRANCE_TO_DANTES;
      snprintf(buf, sizeof(buf), "%s (%ld) quitting out in staff-only room '%s^n' (%ld); they will load at Dante's instead.",
              GET_CHAR_NAME(ch), GET_IDNUM(ch), GET_ROOM_NAME(ch->in_room), GET_ROOM_VNUM(ch->in_room));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    } else
      GET_LAST_IN(ch) = GET_ROOM_VNUM(ch->in_room);
    if(!ch->in_veh)
      act("$n has left the game.", TRUE, ch, 0, 0, TO_ROOM);
    else {
      snprintf(buf, sizeof(buf), "%s has left the game.\r\n", GET_NAME(ch));
      send_to_veh(buf, ch->in_veh, ch, FALSE);
      if (AFF_FLAGGED(ch, AFF_PILOT)) {
        send_to_veh("You get a mild case of whiplash as you come to a sudden stop.\r\n",
                    ch->in_veh, ch, FALSE);
        ch->in_veh->cspeed = SPEED_OFF;
        stop_chase(ch->in_veh);
      }
    }
    snprintf(buf, sizeof(buf), "%s has quit the game.", GET_CHAR_NAME(ch));
    mudlog(buf, ch, LOG_CONNLOG, FALSE);
    send_to_char("Later, chummer.\r\n", ch);

    /*
     * kill off all sockets connected to the same player as the one who is
     * trying to quit.  Helps to maintain sanity as well as prevent duping.
     */
    for (d = descriptor_list; d; d = next_d) {
      next_d = d->next;
      if (d == ch->desc)
        continue;
      if (d->character && (GET_IDNUM(d->character) == GET_IDNUM(ch)))
        close_socket(d);
    }

    /* extract_char */
    if (ch->desc)
      STATE(ch->desc) = CON_QMENU;
    extract_char(ch);           /* Char is saved in extract char */
    return;
  }
  
  // Restore their in_room to NULL since we set it at the beginning of this.
  if (ch->in_veh)
    ch->in_room = NULL;
}

/* generic function for commands which are normally overridden by
   special procedures - i.e., shop commands, mail commands, etc. */
ACMD_CONST(do_not_here) {
  ACMD_DECLARE(do_not_here);
  do_not_here(ch, NULL, 0, 0);
}

ACMD(do_not_here)
{
  send_to_char("Sorry, but you cannot do that here!\r\n", ch);
}

ACMD(do_sneak)
{
  if (IS_AFFECTED(ch, AFF_SNEAK)) {
    AFF_FLAGS(ch).RemoveBit(AFF_SNEAK);
    send_to_char("You stop sneaking around.\r\n", ch);
    return;
  }

  send_to_char("You begin to move with stealth.\r\n", ch);
  AFF_FLAGS(ch).SetBit(AFF_SNEAK);
}

ACMD(do_steal)
{
  struct char_data *vict;
  struct obj_data *obj;
  char vict_name[MAX_INPUT_LENGTH];
  char obj_name[MAX_INPUT_LENGTH];
  int eq_pos;

  if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
    send_to_char(ch, "You cannot steal items until you are authed.\r\n");
    return;
  }

  ACMD_DECLARE(do_gen_comm);

  argument = one_argument(argument, obj_name);
  one_argument(argument, vict_name);

  if (!(vict = get_char_room_vis(ch, vict_name)))
    send_to_char("Steal what from who?\r\n", ch);
  else if (vict == ch)
    send_to_char("Come on now, that's rather stupid!\r\n", ch);
  else if (!IS_NPC(vict) && GET_LEVEL(ch) < GET_LEVEL(vict))
    send_to_char("You can't steal from someone that powerful.\r\n", ch);
  else if (!IS_SENATOR(ch) && IS_NPC(vict) && (mob_index[GET_MOB_RNUM(vict)].func == shop_keeper 
                                               || mob_index[GET_MOB_RNUM(vict)].sfunc == shop_keeper))
    send_to_char(ch, "%s slaps your hand away.\r\n", CAP(GET_NAME(vict)));
  else if (!IS_SENATOR(ch) && AWAKE(vict))
    send_to_char("That would be quite a feat.\r\n", ch);
  else if (!IS_SENATOR(ch) && (!PRF_FLAGGED(ch, PRF_PKER) || !PRF_FLAGGED(vict, PRF_PKER)))
    send_to_char(ch, "Both you and %s need to be toggled PK for that.\r\n", GET_NAME(vict));
  else {
    if (!(obj = get_obj_in_list_vis(vict, obj_name, vict->carrying))) {
      if (!IS_SENATOR(ch)) {
        send_to_char(ch, "You can't take something the person is wearing.\r\n");
        return;
      }
      for (eq_pos = 0; eq_pos < NUM_WEARS; eq_pos++)
        if (GET_EQ(vict, eq_pos) &&
            (isname(obj_name, GET_EQ(vict, eq_pos)->text.keywords)) &&
            CAN_SEE_OBJ(ch, GET_EQ(vict, eq_pos))) {
          obj = GET_EQ(vict, eq_pos);
          break;
        }
      if (!obj) {
        snprintf(buf, sizeof(buf), "$E hasn't got %s.", obj_name);
        act(buf, FALSE, ch, 0, vict, TO_CHAR);
        return;
      } else {                  /* It is equipment */
        send_to_char(ch, "You unequip %s and steal it.\r\n", GET_OBJ_NAME(obj));
        obj_to_char(unequip_char(vict, eq_pos, TRUE), ch);
        char *representation = generate_new_loggable_representation(obj);
        snprintf(buf, sizeof(buf), "%s steals from %s: %s", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), representation);
        mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
        delete [] representation;
      }
    } else {                    /* obj found in inventory */
      if ((IS_CARRYING_N(ch) + 1 < CAN_CARRY_N(ch))) {
        if ((IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj)) < CAN_CARRY_W(ch)) {
          if (!(CAN_WEAR(obj, ITEM_WEAR_TAKE)))
            send_to_char("You can't take that.\r\n", ch);
          else {
            char *representation = generate_new_loggable_representation(obj);
            snprintf(buf, sizeof(buf), "%s steals from %s: %s", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), representation);
            mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
            delete [] representation;
            
            obj_from_char(obj);
            obj_to_char(obj, ch);
            send_to_char(ch, "You filch %s from %s!\r\n", GET_OBJ_NAME(obj), GET_NAME(vict));
            if (GET_LEVEL(ch) < LVL_BUILDER)
              act("$n steals $p from $o's unconscious body.", TRUE, ch, obj, vict, TO_ROOM);
          }
        }
      } else
        send_to_char("You cannot carry that much.\r\n", ch);
    }
  }
}

ACMD(do_practice)
{
  if (subcmd == SCMD_UNPRACTICE)
    send_to_char("Sorry, that feature is only available in the skills annex of character generation.\r\n", ch);
  else
    send_to_char("You can only practice skills through a teacher.  (Use the 'skills' command\r\nto see which skills you know)\r\n", ch);
}

ACMD(do_train)
{
  if (subcmd == SCMD_UNTRAIN)
    send_to_char("Sorry, that feature is only available when standing by Neil the Trainer during character generation.\r\n", ch);
  else
    send_to_char("You can only train with a trainer.\r\n", ch);
}

ACMD(do_visible)
{
  void appear(struct char_data * ch);
  
  bool has_become_visible = FALSE;

  if (IS_AFFECTED(ch, AFF_INVISIBLE) || IS_AFFECTED(ch, AFF_IMP_INVIS)) {
    appear(ch);
    send_to_char("You break the spell of invisibility.\r\n", ch);
    has_become_visible = TRUE;
  }
  
  if (GET_INVIS_LEV(ch) > 0) {
    GET_INVIS_LEV(ch) = 0;
    send_to_char("You release your staff invisibility.\r\n", ch);
    has_become_visible = TRUE;
  }
  
  if (!has_become_visible)
    send_to_char("You are already visible.\r\n", ch);
}

ACMD(do_title)
{
  skip_spaces(&argument);
  delete_doubledollar(argument);

  if (IS_NPC(ch))
    send_to_char("Your title is fine... go away.\r\n", ch);
  else if (PLR_FLAGGED(ch, PLR_NOTITLE))
    send_to_char("You can't title yourself.\r\n", ch);
  else if (strstr((const char *)argument, "(") || strstr((const char *)argument, ")"))
    send_to_char("Titles can't contain the ( or ) characters.\r\n", ch);
  else if (strlen(argument) > (MAX_TITLE_LENGTH - 2)) {
    snprintf(buf, sizeof(buf), "Sorry, titles can't be longer than %d characters.\r\n",
            MAX_TITLE_LENGTH - 2);
    send_to_char(buf, ch);
  } else {
    skip_spaces(&argument);
    strcat(argument, "^n");
    set_title(ch, argument);
    send_to_char(ch, "Okay, you're now %s %s.\r\n", GET_CHAR_NAME(ch), GET_TITLE(ch));
    snprintf(buf, sizeof(buf), "UPDATE pfiles SET Title='%s' WHERE idnum=%ld;", prepare_quotes(buf2, GET_TITLE(ch), sizeof(buf2) / sizeof(buf2[0])), GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
  }
}

int perform_group(struct char_data *ch, struct char_data *vict)
{
  if (IS_AFFECTED(vict, AFF_GROUP) || !CAN_SEE(ch, vict) || (!IS_SENATOR(ch) &&
      IS_SENATOR(vict)))
    return 0;

  AFF_FLAGS(vict).SetBit(AFF_GROUP);
  if (ch != vict)
    act("$N is now a member of your group.", FALSE, ch, 0, vict, TO_CHAR);
  act("You are now a member of $n's group.", FALSE, ch, 0, vict, TO_VICT);
  act("$N is now a member of $n's group.", FALSE, ch, 0, vict, TO_NOTVICT);
  return 1;
}

void print_group(struct char_data *ch)
{
  struct char_data *k;
  struct follow_type *f;

  if (!IS_AFFECTED(ch, AFF_GROUP))
    send_to_char("But you are not the member of a group!\r\n", ch);
  else
  {
    send_to_char("Your group consists of:\r\n", ch);

    k = (ch->master ? ch->master : ch);

    if (IS_AFFECTED(k, AFF_GROUP)) {
      snprintf(buf, sizeof(buf), "     [%3dP %3dM] %-20s (Head of group)\r\n",
              (int)(GET_PHYSICAL(k) / 100), (int)(GET_MENTAL(k) / 100),
              GET_NAME(k));
      send_to_char(buf, ch);
    }

    for (f = k->followers; f; f = f->next) {
      if (!IS_AFFECTED(f->follower, AFF_GROUP))
        continue;

      snprintf(buf, sizeof(buf), "     [%3dP %3dM] $N",
              (int)(GET_PHYSICAL(f->follower) / 100),
              (int)(GET_MENTAL(f->follower) / 100));
      act(buf, FALSE, ch, 0, f->follower, TO_CHAR);
    }
  }
}

ACMD(do_group)
{
  struct char_data *vict;
  struct follow_type *f;
  int found;

  one_argument(argument, buf);

  if (!*buf) {
    print_group(ch);
    return;
  }

  if (ch->master) {
    send_to_char("You can not enroll group members without being head of the group.\r\n", ch);
    return;
  }

  if (!str_cmp(buf, "all")) {
    perform_group(ch, ch);
    for (found = 0, f = ch->followers; f; f = f->next)
      found += perform_group(ch, f->follower);
    if (!found)
      send_to_char("Everyone following you is already in your group.\r\n", ch);
    return;
  }

  if (!(vict = get_char_room_vis(ch, buf)))
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf);
  else if ((vict->master != ch) && (vict != ch))
    act("$N must follow you to enter your group.", FALSE, ch, 0, vict, TO_CHAR);
  else {
    if (!IS_AFFECTED(vict, AFF_GROUP)) {
      perform_group(ch, ch);
      perform_group(ch, vict);
    } else {
      if (ch != vict)
        act("$N is no longer a member of your group.", FALSE, ch, 0, vict, TO_CHAR);
      act("You have been kicked out of $n's group!", FALSE, ch, 0, vict, TO_VICT);
      act("$N has been kicked out of $n's group!", FALSE, ch, 0, vict, TO_NOTVICT);
      AFF_FLAGS(vict).RemoveBit(AFF_GROUP);
    }
  }
}

ACMD(do_ungroup)
{
  struct follow_type *f, *next_fol;
  struct char_data *tch;
  void stop_follower(struct char_data * ch);

  one_argument(argument, buf);

  if (!*buf) {
    if (ch->master || !(IS_AFFECTED(ch, AFF_GROUP))) {
      send_to_char("But you lead no group!\r\n", ch);
      return;
    }
    snprintf(buf2, sizeof(buf2), "%s has disbanded the group.\r\n", GET_NAME(ch));
    for (f = ch->followers; f; f = next_fol) {
      next_fol = f->next;
      if (IS_AFFECTED(f->follower, AFF_GROUP)) {
        AFF_FLAGS(f->follower).RemoveBit(AFF_GROUP);
        send_to_char(buf2, f->follower);
        if (!IS_AFFECTED(f->follower, AFF_CHARM))
          stop_follower(f->follower);
      }
    }

    AFF_FLAGS(ch).RemoveBit(AFF_GROUP);
    send_to_char("You disband the group.\r\n", ch);
    return;
  }
  if (!(tch = get_char_room_vis(ch, buf))) {
    send_to_char("There is no such person!\r\n", ch);
    return;
  }
  if (tch->master != ch) {
    send_to_char(ch, "%s is not following you!\r\n", capitalize(GET_CHAR_NAME(tch)));
    return;
  }

  if (!IS_AFFECTED(tch, AFF_GROUP)) {
    send_to_char(ch, "%s isn't in your group.\r\n", capitalize(GET_CHAR_NAME(tch)));
    return;
  }

  AFF_FLAGS(tch).RemoveBit(AFF_GROUP);

  act("$N is no longer a member of your group.", FALSE, ch, 0, tch, TO_CHAR);
  act("You have been kicked out of $n's group!", FALSE, ch, 0, tch, TO_VICT);
  act("$N has been kicked out of $n's group!", FALSE, ch, 0, tch, TO_NOTVICT);

  if (!IS_AFFECTED(tch, AFF_CHARM))
    stop_follower(tch);
}

ACMD(do_report)
{
  struct char_data *k;
  struct follow_type *f;

  if (!IS_AFFECTED(ch, AFF_GROUP)) {
    send_to_char("But you are not a member of any group!\r\n", ch);
    return;
  }
  snprintf(buf, sizeof(buf), "%s reports: %d/%dP, %d/%dM\r\n", GET_NAME(ch),
          (int)(GET_PHYSICAL(ch) / 100), (int)(GET_MAX_PHYSICAL(ch) / 100),
          (int)(GET_MENTAL(ch) / 100), (int)(GET_MAX_MENTAL(ch) / 100));

  CAP(buf);

  k = (ch->master ? ch->master : ch);

  for (f = k->followers; f; f = f->next)
    if (IS_AFFECTED(f->follower, AFF_GROUP) && f->follower != ch)
      send_to_char(buf, f->follower);
  if (k != ch)
    send_to_char(buf, k);
  send_to_char("You report to the group.\r\n", ch);
}

ACMD(do_patch)
{
  struct char_data *vict;
  struct obj_data *patch;
  half_chop(argument, arg, buf);

  if (!*arg || !*buf) {
    send_to_char("Who do you want to patch and with what?\r\n", ch);
    return;
  }

  if (!(patch = get_obj_in_list_vis(ch, arg, ch->carrying))) {
    send_to_char(ch, "You don't seem to have a '%s'.\r\n", arg);
    return;
  }
  if (!(vict = get_char_room_vis(ch, buf))) {
    send_to_char(ch, "There doesn't seem to be a '%s' here.\r\n", buf);
    return;
  }
  if (GET_EQ(vict, WEAR_PATCH)) {
    act("$N already has a patch applied.", FALSE, ch, 0, vict, TO_CHAR);
    return;
  }
  if (IS_NPC(vict) && (mob_index[GET_MOB_RNUM(vict)].func == shop_keeper 
                       || mob_index[GET_MOB_RNUM(vict)].sfunc == shop_keeper 
                       || mob_index[GET_MOB_RNUM(vict)].func == johnson 
                       || mob_index[GET_MOB_RNUM(vict)].sfunc == johnson
                       || MOB_FLAGGED(vict,MOB_NOKILL))) {
    send_to_char("You can't patch that...\r\n", ch);
    return;
  }
  if (GET_OBJ_VAL(patch, 1) < 1) {
    send_to_char(ch, "%s seems to be defective...\r\n", capitalize(GET_OBJ_NAME(patch)));
    return;
  }
  if (GET_OBJ_TYPE(patch) != ITEM_PATCH) {
    send_to_char(ch, "%s is not a patch.\r\n", capitalize(GET_OBJ_NAME(patch)));
    return;
  }  
  switch (GET_OBJ_VAL(patch, 0)) {
  case 0:                          // antidote
    if (vict == ch)
      act("You slap $p on your shoulder.", FALSE, ch, patch, 0, TO_CHAR);
    else {
      act("You slap $p on $N's shoulder.", FALSE, ch, patch, vict, TO_CHAR);
      act("$n slaps $p on your shoulder.", FALSE, ch, patch, vict, TO_VICT);
      act("$n slaps $p on $N's shoulder.", FALSE, ch, patch, vict, TO_NOTVICT);
    }
    obj_from_char(patch);
    GET_EQ(vict, WEAR_PATCH) = patch;
    patch->worn_by = vict;
    patch->worn_on = WEAR_PATCH;
    break;
  case 1:                          // stim
    if (vict != ch) {
      send_to_char("You can only use stim patches on yourself.\r\n", ch);
      return;
    }
    act("You slap $p on your shoulder and feel more aware.", FALSE, ch, patch, 0, TO_CHAR);
    act("$n slaps $p on $s shoulder and appears more aware.", TRUE, ch, patch, 0, TO_ROOM);
    GET_OBJ_VAL(patch,5) = GET_MENTAL(ch);
    GET_MENTAL(ch) = MIN(GET_MAX_MENTAL(ch), GET_MENTAL(ch) + (GET_OBJ_VAL(patch, 1) * 100));
    obj_from_char(patch);
    GET_EQ(vict, WEAR_PATCH) = patch;
    patch->worn_by = vict;
    patch->worn_on = WEAR_PATCH;
    break;
  case 2:                          // tranq
    if (GET_POS(vict) == POS_FIGHTING) {
      send_to_char("You can't put a tranq patch on a fighting person!\r\n", ch);
      return;
    }
    if (vict == ch) {
      send_to_char("Now why would you do that?\r\n", ch);
      return;
    }
    if (GET_POS(vict) > POS_SLEEPING) {
      if (resisted_test(GET_QUI(ch), GET_QUI(vict) - GET_POS(vict) + POS_STANDING, GET_QUI(vict),
                        GET_QUI(ch) - GET_POS(ch) + POS_STANDING) <= 0) {
        act("$N nimbly dodges your attempt to put $p on $M.", FALSE, ch, patch, vict, TO_CHAR);
        act("You nimbly dodge $n's attempt to put $p on you.", FALSE, ch, patch, vict, TO_VICT);
        act("$N nimbly dodges $n's attempt to put $p on $M.", FALSE, ch, patch, vict, TO_NOTVICT);
        if (IS_NPC(vict) || (IS_NPC(ch) && !IS_NPC(vict))) {
          set_fighting(vict, ch);
          set_fighting(ch, vict);
        }
        return;
      } else {
        act("You slap $p on $N before $E has a chance to move!", FALSE, ch, patch, vict, TO_CHAR);
        act("$n slaps $p on you before you can get out of the way!", FALSE, ch, patch, vict, TO_VICT);
        act("$n slaps $p on $N before $E has a chance to move!", FALSE, ch, patch, vict, TO_NOTVICT);
        if (IS_NPC(vict) || (IS_NPC(ch) && !IS_NPC(vict))) {
          set_fighting(vict, ch);
          set_fighting(ch, vict);
        }
      }
    }
    obj_from_char(patch);
    GET_EQ(vict, WEAR_PATCH) = patch;
    patch->worn_by = vict;
    patch->worn_on = WEAR_PATCH;
    break;
  case 3:                          // trauma
    if (GET_POS(vict) >= POS_STUNNED) {
      send_to_char("Now where's the sense in that?\r\n", ch);
      return;
    }
    act("You slap $p over $N's heart.", FALSE, ch, patch, vict, TO_CHAR);
    act("$n slaps $p over $N's heart.", FALSE, ch, patch, vict, TO_NOTVICT);
    obj_from_char(patch);
    GET_EQ(vict, WEAR_PATCH) = patch;
    patch->worn_by = vict;
    patch->worn_on = WEAR_PATCH;
    break;
  default:
    act("$p seems to be defective.", FALSE, ch, patch, 0, TO_CHAR);
    snprintf(buf, sizeof(buf), "Illegal patch type - object #%ld", GET_OBJ_VNUM(patch));
    mudlog(buf, ch, LOG_SYSLOG, FALSE);
    break;
  }
}

void do_drug_take(struct char_data *ch, struct obj_data *obj)
{
    int drugval = GET_OBJ_VAL(obj, 0);
    if ((GET_DRUG_AFFECT(ch) && GET_DRUG_DOSE(ch) > GET_DRUG_TOLERANT(ch, drugval)) || GET_DRUG_STAGE(ch) > 1) {
      send_to_char(ch, "Maybe you should wait.\r\n");
      return;
    }
    act("$n takes $p.", TRUE, ch, obj, 0, TO_ROOM);
    GET_DRUG_AFFECT(ch) = drugval;
    GET_DRUG_DOSES(ch, drugval)++;
    if (GET_DRUG_DOSES(ch, drugval) == 1) {
      if ((GET_DRUG_ADDICT(ch, drugval) != 1 || GET_DRUG_ADDICT(ch, drugval) != 3) &&
          (drug_types[drugval].mental_addiction ? success_test(GET_WIL(ch), drug_types[drugval].mental_addiction) : 1) < 1) {
        if (GET_DRUG_ADDICT(ch, drugval) == 2)
          GET_DRUG_ADDICT(ch, drugval) = 3;
        else
          GET_DRUG_ADDICT(ch, drugval) = 1;
        GET_DRUG_EDGE(ch, drugval) = 1;
      }
      if ((GET_DRUG_ADDICT(ch, drugval) != 2 || GET_DRUG_ADDICT(ch, drugval) != 3) &&
          (drug_types[drugval].physical_addiction ? success_test(GET_REAL_BOD(ch), drug_types[drugval].physical_addiction) : 1) < 1) {
        if (GET_DRUG_ADDICT(ch, drugval) == 1)
          GET_DRUG_ADDICT(ch, drugval) = 3;
        else
          GET_DRUG_ADDICT(ch, drugval) = 2;
        GET_DRUG_EDGE(ch, drugval) = 1;
      }
    } else if ((GET_DRUG_DOSES(ch, drugval) > 0 && ((!GET_DRUG_ADDICT(ch, drugval) && !(GET_DRUG_DOSES(ch, drugval) % drug_types[drugval].edge_preadd)))) ||
               (GET_DRUG_ADDICT(ch, drugval) && !(GET_DRUG_DOSES(ch, drugval) % drug_types[drugval].edge_posadd))) {
      GET_DRUG_EDGE(ch, drugval)++;
      if ((drug_types[drugval].mental_addiction ? success_test(GET_WIL(ch), drug_types[drugval].mental_addiction + GET_DRUG_EDGE(ch, drugval)): 1) < 1) {
        GET_DRUG_ADDICT(ch,drugval) = 1;
        GET_DRUG_EDGE(ch, drugval) = 1;
      }
      if ((drug_types[drugval].physical_addiction ? success_test(GET_REAL_BOD(ch), drug_types[drugval].physical_addiction + GET_DRUG_EDGE(ch, drugval)) : 1) < 1) {
        GET_DRUG_ADDICT(ch, drugval) = 2;
        GET_DRUG_EDGE(ch, drugval) = 1;
      }
    }
    send_to_char(ch, "You take %s.\r\n", GET_OBJ_NAME(obj));
    GET_DRUG_DOSE(ch)++;
    if (GET_DRUG_DOSE(ch) > GET_DRUG_TOLERANT(ch, drugval)) {
      GET_DRUG_STAGE(ch) = 0;
      if (AFF_FLAGS(ch).AreAnySet(AFF_WITHDRAWAL, AFF_WITHDRAWAL_FORCE, ENDBIT) && GET_DRUG_ADDICT(ch, drugval)) {
        GET_DRUG_EDGE(ch, drugval)++;
        AFF_FLAGS(ch).RemoveBits(AFF_WITHDRAWAL, AFF_WITHDRAWAL_FORCE, ENDBIT);
      }
      // Drug tick is every 2 seconds (a MUD minute), so most of these were over in a flash. Let's crank it up so that these values are in IRL minutes instead.
      switch (drugval) {
      case DRUG_PSYCHE:
      case DRUG_CRAM:
      case DRUG_BURN:
        GET_DRUG_DURATION(ch) = 50 * (60 / SECS_PER_MUD_MINUTE); 
        break;
      case DRUG_ZEN:
        GET_DRUG_DURATION(ch) = 25 * srdice() * (60 / SECS_PER_MUD_MINUTE);
        break;
      default:
        GET_DRUG_DURATION(ch) = 0;
        break;
      }
    } else {
      GET_DRUG_STAGE(ch) = -1;
      GET_DRUG_DURATION(ch) = 20;
    }
    extract_obj(obj);
}
ACMD(do_use)
{
  struct obj_data *obj, *corpse;
  struct char_data *tmp_char;

  half_chop(argument, arg, buf);
  if (!*arg) {
    send_to_char(ch, "What do you want to %s?\r\n", CMD_NAME);
    return;
  }

  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (isname(arg, obj->text.keywords))
      break;
  if (!obj) {
    send_to_char(ch, "You don't have a '%s'.\r\n", arg);
    return;
  }

  if (GET_OBJ_TYPE(obj) == ITEM_MONEY && GET_OBJ_VAL(obj, 1)) {
    generic_find(buf, FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &tmp_char, &corpse);
    if (!corpse)
      act("Use $p on what?", FALSE, ch, obj, 0, TO_CHAR);
    else if (GET_OBJ_VAL(obj, 2) == 1)
      send_to_char("You can't use that type of credstick.\r\n", ch);
    else if (!GET_OBJ_VAL(obj, 4) || belongs_to(ch, obj))
      send_to_char("And why would you need to do that?\r\n", ch);
    else if (!IS_OBJ_STAT(corpse, ITEM_CORPSE))
      send_to_char("And how, pray tell, would that work?\r\n", ch);
    else if (GET_OBJ_VAL(obj, 3) || GET_OBJ_VAL(obj, 4) != GET_OBJ_VAL(corpse, 5)) {
      if (GET_OBJ_VAL(obj, 2) == 2) {
        act("You press $p against its thumb, but the display flashes red.",
            FALSE, ch, obj, 0, TO_CHAR);
        act("$n holds $p against $P's thumb.", TRUE, ch, obj, corpse, TO_ROOM);
      } else {
        act("After scanning the retina, $p's display flashes red.", FALSE,
            ch, obj, 0, TO_CHAR);
        act("$n scans $P's retina with $p.", TRUE, ch, obj, corpse, TO_ROOM);
      }
    } else {
      if (GET_OBJ_VAL(obj, 2) == 2) {
        act("You press $p against its thumb, and the display flashes green.", FALSE,
            ch, obj, 0, TO_CHAR);
        act("$n holds $p against $P's thumb.", TRUE, ch, obj, corpse, TO_ROOM);
      } else {
        act("After scanning the retina, $p's display flashes green.", FALSE,
            ch, obj, 0, TO_CHAR);
        act("$n scans $P's retina with $p.", TRUE, ch, obj, corpse, TO_ROOM);
      }
      GET_OBJ_VAL(obj, 3) = 0;
      GET_OBJ_VAL(obj, 4) = 0;
    }
    return;
  } else if (GET_OBJ_TYPE(obj) == ITEM_DRUG)
    do_drug_take(ch, obj);
  else send_to_char("You can't do anything different with that.\r\n", ch);
}

ACMD(do_wimpy)
{
  int wimp_lev;

  one_argument(argument, arg);

  if (!*arg) {
    if (GET_WIMP_LEV(ch)) {
      snprintf(buf, sizeof(buf), "Your current wimp level is %d hit points.\r\n",
              GET_WIMP_LEV(ch));
      send_to_char(buf, ch);
      return;
    } else {
      send_to_char("You aren't currently planning to wimp out of fights. You can change this by typing WIMPY [health level to flee at].\r\n", ch);
      return;
    }
  }
  if (isdigit(*arg)) {
    if ((wimp_lev = atoi(arg))) {
      if (wimp_lev < 0) {
        send_to_char("You're not 100% certain, but you're pretty sure you won't be able to run away when you're dead.\r\n", ch);
        return;
      }
      
      if (wimp_lev > ((int)(GET_MAX_PHYSICAL(ch) / 100) >> 1)) {
        send_to_char(ch, "You can't set your wimp level above %d.\r\n", ((int)(GET_MAX_PHYSICAL(ch) / 100) >> 1));
        return;
      }
      
      snprintf(buf, sizeof(buf), "Okay, you'll wimp out if you drop below %d physical or mental.\r\n",
              wimp_lev);
      send_to_char(buf, ch);
      GET_WIMP_LEV(ch) = wimp_lev;
    } else {
      send_to_char("Okay, you'll now tough out fights to the bitter end.\r\n", ch);
      GET_WIMP_LEV(ch) = 0;
    }
    snprintf(buf, sizeof(buf), "UPDATE pfiles SET WimpLevel=%d WHERE idnum=%ld;", GET_WIMP_LEV(ch), GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
  } else
    send_to_char("Specify the threshold you want to wimp out at. (0 to disable)\r\n", ch);
  
  return;

}

ACMD(do_display)
{
  struct char_data *tch;
  if (IS_NPC(ch) && !ch->desc->original) {
    send_to_char("Monsters don't need displays.  Go away.\r\n", ch);
    return;
  } else
    tch = (ch->desc->original ? ch->desc->original : ch);

  skip_spaces(&argument);
  delete_doubledollar(argument);
  prepare_quotes(buf, argument, sizeof(buf) / sizeof(buf[0]));

  if (!*buf) {
    send_to_char(ch, "Current prompt:\r\n%s\r\n", GET_PROMPT(tch));
    return;
  } else if (strlen(buf) > LINE_LENGTH - 1) {
    send_to_char(ch, "Customized prompts are limited to %d characters.\r\n",
                 LINE_LENGTH - 1);
    return;
  } else {
    DELETE_ARRAY_IF_EXTANT(GET_PROMPT(tch));
    GET_PROMPT(tch) = str_dup(buf);
    send_to_char(OK, ch);
    snprintf(buf, sizeof(buf), "UPDATE pfiles SET%sPrompt='%s' WHERE idnum=%ld;", PLR_FLAGGED((ch), PLR_MATRIX) ? " Matrix" : " ", GET_PROMPT(ch), GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
  }
}

ACMD(do_gen_write)
{
  FILE *fl;
  char *tmp;
  const char *filename;
  struct stat fbuf;
  extern int max_filesize;
  time_t ct;

  if(PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
    send_to_char("You must be Authorized to use that command.\r\n", ch);
    return;
  }

  switch (subcmd) {
  case SCMD_BUG:
    filename = BUG_FILE;
    break;
  case SCMD_TYPO:
    filename = TYPO_FILE;
    break;
  case SCMD_IDEA:
    filename = IDEA_FILE;
    break;
  default:
    return;
  }

  ct = time(0);
  tmp = asctime(localtime(&ct));

  if (IS_NPC(ch) && !ch->desc) {
    send_to_char("Monsters can't have ideas - Go away.\r\n", ch);
    return;
  }

  skip_spaces(&argument);
  delete_doubledollar(argument);

  if (!*argument) {
    send_to_char("That must be a mistake...\r\n", ch);
    return;
  }
  snprintf(buf, sizeof(buf), "%s %s: %s", (ch->desc->original ? GET_CHAR_NAME(ch->desc->original) : GET_CHAR_NAME(ch)),
          CMD_NAME, argument);
  mudlog(buf, ch, LOG_MISCLOG, FALSE);

  if (stat(filename, &fbuf) < 0) {
    perror("Error statting file");
    return;
  }
  if (fbuf.st_size >= max_filesize) {
    send_to_char("Sorry, the file is full right now.. try again later.\r\n", ch);
    return;
  }
  if (!(fl = fopen(filename, "a"))) {
    perror("do_gen_write");
    send_to_char("Could not open the file.  Sorry.\r\n", ch);
    return;
  }
  fprintf(fl, "%-8s (%6.6s) [%5ld] %s\n", GET_CHAR_NAME(ch), (tmp + 4), GET_ROOM_VNUM(get_ch_in_room(ch)), argument);
  fclose(fl);
  
#ifdef GITHUB_INTEGRATION
  CURL *curl;
  CURLcode res;
  
  // If we're running in Windows, this initializes winsock-related things.
  curl_global_init(CURL_GLOBAL_ALL);
  
  // Get our curl instance.
  curl = curl_easy_init();
  if (curl) {
    char title[100];
    char body[MAX_STRING_LENGTH];
    memset(body, 0, sizeof(body));
    char temp_body[strlen(argument) + 1];
    char labels[256];
    
    // Select our issue label.
    switch (subcmd) {
      case SCMD_BUG:
        strcpy(title, "In-Game Bug Report: ");
        strcpy(labels, "\"bug\", \"autogenerated\"");
        break;
      case SCMD_IDEA:
        strcpy(title, "In-Game Idea: ");
        strcpy(labels, "\"idea\", \"autogenerated\"");
        break;
      case SCMD_TYPO:
        strcpy(title, "In-Game Typo Report: ");
        strcpy(labels, "\"typo\", \"autogenerated\"");
        break;
      default:
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return;
    }
    
    // Compose the issue title and body.
    char *title_ptr = ENDOF(title), *body_ptr = temp_body, *argument_ptr = argument;
    while (*argument_ptr) {
      if (title_ptr - &(title[0]) > 80) {
        // Escape reserved characters.
        if (*argument_ptr == '\\' || *argument_ptr == '"')
          *(body_ptr++) = '\\';
        *(body_ptr++) = *(argument_ptr++);
      } else {
        // Escape reserved characters.
        if (*argument_ptr == '\\' || *argument_ptr == '"')
          *(title_ptr++) = '\\';
        *(title_ptr++) = *(argument_ptr++);
      }
    }
    // Add ellipsis for overflowing titles.
    if (body_ptr != &(temp_body[0])) {
      // We're currently pointing to the uninitialized character directly after the end of the string. Go back one.
      title_ptr--;
      
      // Track back to discard spaces from title_ptr's end.
      while (isspace(*title_ptr) && title_ptr != &(title[0]))
        title_ptr--;
      
      // Restore our title pointer to point to the character right after the text.
      title_ptr++;
      
      // Add the ellipsis to the title.
      for (int i = 0; i < 3; i++)
        *(title_ptr++) = '.';
    }
    // Null-terminate both body and title strings.
    *title_ptr = '\0';
    *body_ptr = '\0';
    
    // Format the temp body with preceding ellipsis if it exists.
    if (*temp_body) {
      snprintf(body, sizeof(body), "...%s\\r\\n\\r\\n", temp_body);
    }
    
    // Fill out the character and location details where applicable.
    snprintf(ENDOF(body), sizeof(body) - strlen(body), "Filed by %s (id %ld)", GET_CHAR_NAME(ch), (ch->desc->original ? GET_IDNUM(ch->desc->original) : GET_IDNUM(ch)));
    if (ch->in_room) {
      snprintf(ENDOF(body), sizeof(body) - strlen(body), " from room %ld.", GET_ROOM_VNUM(ch->in_room));
    } else if (ch->in_veh) {
      snprintf(ENDOF(body), sizeof(body) - strlen(body), " from the %s of a vehicle of vnum %ld", ch->vfront ? "front" : "rear", veh_index[ch->in_veh->veh_number].vnum);
      if (ch->in_veh->in_veh) {
        snprintf(ENDOF(body), sizeof(body) - strlen(body), ", located inside another vehicle of vnum %ld.", veh_index[ch->in_veh->in_veh->veh_number].vnum);
      } else if (ch->in_veh->in_room) {
        snprintf(ENDOF(body), sizeof(body) - strlen(body), ", located in room %ld.", GET_ROOM_VNUM(ch->in_veh->in_room));
      } else {
        snprintf(ENDOF(body), sizeof(body) - strlen(body), ".");
      }
    } else {
      snprintf(ENDOF(body), sizeof(body) - strlen(body), ".");
    }
    
    // Compose our post body.
    char post_body[MAX_STRING_LENGTH];
    snprintf(post_body, sizeof(post_body),
            "{\r\n"
            "  \"title\": \"%s\",\r\n"
            "  \"body\": \"%s\",\r\n"
            "  \"labels\": [\r\n"
            "    %s\r\n"
            "  ]\r\n"
            "}", title, body, labels);
    log(post_body);
    
    // Set our URL and auth (github_config.cpp)
    curl_easy_setopt(curl, CURLOPT_URL, github_issues_url);
    curl_easy_setopt(curl, CURLOPT_USERPWD, github_authentication);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "AwakeMUD CE GitHub Integration/1.0");
    
    // Specify our POST data.
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body);
    
    // Perform POST request, storing the curl response code.
    res = curl_easy_perform(curl);
    // Check for errors
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\r\n", curl_easy_strerror(res));
    
    // always cleanup
    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();
#endif

  send_to_char("Okay. Thanks!\r\n", ch);
}

const char *tog_messages[][2] = {
                            {"You should never see this.  Use the \"bug\" command to report!\r\n",
                             "You are AFK.\r\n"},
                            {"Autoexits disabled.\r\n",
                             "Autoexits enabled.\r\n"},
                            {"Compact mode off.\r\n",
                             "Compact mode on.\r\n"},
                            {"You will now have your communication repeated.\r\n",
                             "You will no longer have your communication repeated.\r\n"},
                            {"Fightgag off.\r\n",
                             "Fightgag on.\r\n"},                               // 5
                            {"HolyLight mode off.\r\n",
                             "HolyLight mode on.\r\n"},
                            {"Movegag off.\r\n",
                             "Movegag on.\r\n"},
                            {"You can now hear radio communications.\r\n",
                             "You are now deaf to radio communications.\r\n"},
                            {"Nohassle disabled.\r\n",
                             "Nohassle enabled.\r\n"},                          // 10
                            {"You can now hear the newbie channel again.\r\n",
                             "You are now deaf to the newbie channel.\r\n"},
                            {"You can now hear shouts.\r\n",
                             "You are now deaf to shouts.\r\n"},
                            {"You can now hear tells.\r\n",
                             "You are now deaf to tells.\r\n"},
                            {"You can now hear the Wiz-channel.\r\n",
                             "You are now deaf to the Wiz-channel.\r\n"},       // 15
                            {"But you are already a pk'er!\r\n",
                             "You're now a pk'er...better watch your back!\r\n"},
                            {"You are no longer on a job.\r\n",
                             "Okay, consider yourself hired!\r\n"},
                            {"You will no longer see the room flags.\r\n",
                             "You will now see the room flags.\r\n"},
                            {"Long exit display disabled.\r\n",
                             "Long exit display enabled.\r\n"},
                            {"Details on rolls disabled.\r\n",
                             "Details on rolls enabled.\r\n"},
                            {"You can now hear the OOC channel.\r\n",
                             "You are now deaf to the OOC channel.\r\n"},
                            {"Auto-invisibility disabled.\r\n",
                             "Auto-invisibility enabled.\r\n"},
                            {"Auto-Assist disabled.\r\n",
                             "Auto-Assist enabled.\r\n"},
                            {"RPE Channel enabled.\r\n",
                             "RPE Channel disabled.\r\n"},
                            {"Hired Channel enabled.\r\n",
                             "Hired Channel disabled.\r\n"},
                            {"Pacify disabled.\r\n",
                             "Pacify enabled.\r\n"},
                            {"You're already Hardcore.\r\n",
                             "Come on baby, don't fear The Reaper...\r\n"},
                            {"You will now see OOC in menus.\r\n",
                             "You will no longer see OOC in menus.\r\n"},
                            {"You will no longer see if people are wielding weapons in room descriptions.\r\n",
                             "You will now see weapons in room descriptions.\r\n"},
                            {"You will no longer display your playergroup affiliation in the wholist.\r\n",
                             "You will now display your playergroup affiliation in the wholist.\r\n"},
                            {"You will no longer receive the keepalive pulses from the MUD.\r\n",
                             "You will now receive keepalive pulses from the MUD.\r\n"},
                            {"Screenreader mode disabled. Your TOGGLE NOCOLOR and TOGGLE NOPROMPT settings have not been altered.\r\n",
                             "Screenreader mode enabled. Extraneous text will be reduced. Color and prompts have been disabled too, you may TOGGLE NOCOLOR and TOGGLE NOPROMPT to restore them.\r\n"},
                            {"You will now receive ANSI color codes again.\r\n",
                             "You will no longer receive ANSI color codes.\r\n"},
                            {"You will now receive prompts.\r\n",
                             "You will no longer receive prompts automatically.\r\n"},
                            {"You will no longer autokill NPCs, and will instead stop when they're downed.\r\n",
                             "You will now continue attacking downed NPCs.\r\n"},
                            {"You will now see names auto-appended to voices.\r\n",
                             "You will no longer see names auto-appended to voices.\r\n"},
                            
                          };

ACMD(do_toggle)
{
  long result = 0;
  int mode = 0;

  if (IS_NPC(ch)) {
    send_to_char("You cannot view or change your preferences in your current form.\r\n", ch);
    return;
  }
  skip_spaces(&argument);
  if (!*argument) {
    // Wipe out our string.
    strcpy(buf, "");
    
      int printed = 0;
    for (int i = 0; i < PRF_MAX; i++) {
      // Skip the unused holes in our preferences.
      if (i == PRF_UNUSED1_PLS_REPLACE || i == PRF_UNUSED2_PLS_REPLACE) {
        continue;
      }
      
      // Skip the things that morts should not see.
      if (!IS_SENATOR(ch) && preference_bits_v2[i].staff_only) {
        continue;
      }
      
      // Select ONOFF or YESNO display type based on field 2.
      if (preference_bits_v2[i].on_off) {
        strcpy(buf2, ONOFF(PRF_FLAGGED(ch, i)));
      } else {
        strcpy(buf2, YESNO(PRF_FLAGGED(ch, i)));
      }
      
      // Compose and append our line.
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), 
              "%18s: %-3s%s", 
              preference_bits_v2[i].name, 
              buf2, 
              printed%3 == 2 ? "\r\n" : "");
      
      // Increment our spacer.
      printed++;
      
    }
    
    // Calculate and add the wimpy level.
    if (GET_WIMP_LEV(ch) == 0) {
      strcpy(buf2, "OFF");
    } else {
      snprintf(buf2, sizeof(buf2), "%-3d", GET_WIMP_LEV(ch));
    }
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%18s: %-3s", 
      printed%3 == 0 ? "\r\n" : "", 
      "Wimpy", 
      buf2
    );
    send_to_char(buf, ch);
  } else {
    if (is_abbrev(argument, "afk"))
      result = PRF_TOG_CHK(ch, PRF_AFK);
    else if (is_abbrev(argument, "autoexits")) {
      result = PRF_TOG_CHK(ch, PRF_AUTOEXIT);
      mode = 1;
    } else if (is_abbrev(argument, "compact")) {
      result = PRF_TOG_CHK(ch, PRF_COMPACT);
      mode = 2;
    } else if (is_abbrev(argument, "echo")) {
      result = PRF_TOG_CHK(ch, PRF_NOREPEAT);
      mode = 3;
    } else if (is_abbrev(argument, "fightgag")) {
      result = PRF_TOG_CHK(ch, PRF_FIGHTGAG);
      mode = 4;
    } else if (is_abbrev(argument, "holylight") && IS_SENATOR(ch)) {
      result = PRF_TOG_CHK(ch, PRF_HOLYLIGHT);
      mode = 5;
    } else if (is_abbrev(argument, "movegag")) {
      result = PRF_TOG_CHK(ch, PRF_MOVEGAG);
      mode = 6;
    } else if (is_abbrev(argument, "noradio") && IS_SENATOR(ch)) {
      result = PRF_TOG_CHK(ch, PRF_NORADIO);
      mode = 7;
    } else if (is_abbrev(argument, "nohassle") && IS_SENATOR(ch)) {
      result = PRF_TOG_CHK(ch, PRF_NOHASSLE);
      mode = 8;
    } else if (is_abbrev(argument, "nonewbie") && (IS_SENATOR(ch) || PRF_FLAGGED(ch, PRF_NEWBIEHELPER))) {
      result = PRF_TOG_CHK(ch, PRF_NONEWBIE);
      mode = 9;
    } else if (is_abbrev(argument, "noshout")) {
      result = PRF_TOG_CHK(ch, PRF_DEAF);
      mode = 10;
    } else if (is_abbrev(argument, "notell") && IS_SENATOR(ch)) {
      result = PRF_TOG_CHK(ch, PRF_NOTELL);
      mode = 11;
    } else if (is_abbrev(argument, "ooc")) {
      result = PRF_TOG_CHK(ch, PRF_NOOOC);
      mode = 18;
    } else if (is_abbrev(argument, "menugag")) {
      result = PRF_TOG_CHK(ch, PRF_MENUGAG);
      mode = 25;
    } else if (is_abbrev(argument, "pk")) {
      if (PLR_FLAGGED(ch, PLR_NEWBIE)) {
        send_to_char("You are not yet able to become a pk'er.\r\n", ch);
        return;
      }
      if (PRF_FLAGGED(ch, PRF_PKER)) {
        send_to_char("You are already a pk'er!\r\n", ch);
        return;
      }
      if (!PRF_FLAGGED(ch, PRF_PKER))
        PRF_FLAGS(ch).SetBit(PRF_PKER);
      mode = 13;
      result = 1;
    } else if (is_abbrev(argument, "hired")) {
      result = PRF_TOG_CHK(ch, PRF_QUEST);
      mode = 14;
#ifndef MORTS_CAN_SEE_ROLLS
    } else if (IS_SENATOR(ch) && is_abbrev(argument, "rolls")) {
#else
    } else if (is_abbrev(argument, "rolls")) {
#endif
      result = PRF_TOG_CHK(ch, PRF_ROLLS);
      mode = 17;
    } else if (is_abbrev(argument, "roomflags") && IS_SENATOR(ch)) {
      result = PRF_TOG_CHK(ch, PRF_ROOMFLAGS);
      mode = 15;
    } else if (is_abbrev(argument, "longexits")) {
      result = PRF_TOG_CHK(ch, PRF_LONGEXITS);
      mode = 16;
    } else if (is_abbrev(argument, "autoinvis")) {
      result = PRF_TOG_CHK(ch, PRF_AUTOINVIS);
      mode = 19;
    } else if (is_abbrev(argument, "autoassist")) {
      result = PRF_TOG_CHK(ch, PRF_ASSIST);
      mode = 20;
    } else if (is_abbrev(argument, "norpe")) {
      result = PRF_TOG_CHK(ch, PRF_NORPE);
      mode = 21;
    } else if (is_abbrev(argument, "nohired")) {
      result = PRF_TOG_CHK(ch, PRF_NOHIRED);
      mode = 22;
    } else if (is_abbrev(argument, "pacify") && IS_SENATOR(ch)) {
      result = PRF_TOG_CHK(ch, PRF_PACIFY);
      mode = 23;
   } else if (is_abbrev(argument, "longweapon")) {
      result = PRF_TOG_CHK(ch, PRF_LONGWEAPON);
      mode = 26;
    } else if (is_abbrev(argument, "hardcore")) {
      if (!PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
        send_to_char("It's too late for you to toggle Hardcore Mode.\r\n", ch);
        return;
      }
      if (GET_LEVEL(ch) > 1) {
        send_to_char("Being an Immortal is Hardcore enough.\r\n", ch);
        return;
      }
      if (!PRF_FLAGGED(ch, PRF_HARDCORE)) {
        PRF_FLAGS(ch).SetBit(PRF_HARDCORE);
        PLR_FLAGS(ch).SetBit(PLR_NODELETE);
        snprintf(buf, sizeof(buf), "UPDATE pfiles SET Hardcore=1, NoDelete=1 WHERE idnum=%ld;", GET_IDNUM(ch));
        mysql_wrapper(mysql, buf);
      }
      mode = 24;
      result = 1;
    } else if (is_abbrev(argument, "showpgtags")) {
      result = PRF_TOG_CHK(ch, PRF_SHOWGROUPTAG);
      mode = 27;
    } else if (is_abbrev(argument, "keepalives")) {
      result = PRF_TOG_CHK(ch, PRF_KEEPALIVE);
      mode = 28;
    } else if (is_abbrev(argument, "screenreader")) {
      result = PRF_TOG_CHK(ch, PRF_SCREENREADER);
      
      // Turning on the screenreader? Color and prompt goes off.
      if (result) {
        PRF_FLAGS(ch).SetBit(PRF_NOCOLOR);
        PRF_FLAGS(ch).SetBit(PRF_NOPROMPT);
      }
      mode = 29;
    } else if (is_abbrev(argument, "nocolors") || is_abbrev(argument, "colors") || is_abbrev(argument, "colours")) {
      result = PRF_TOG_CHK(ch, PRF_NOCOLOR);
      mode = 30;
    } else if (is_abbrev(argument, "noprompts") || is_abbrev(argument, "prompts")) {
      result = PRF_TOG_CHK(ch, PRF_NOPROMPT);
      mode = 31;
    } else if (is_abbrev(argument, "autokill")) {
      result = PRF_TOG_CHK(ch, PRF_AUTOKILL);
      mode = 32;
    } else if (IS_SENATOR(ch) && is_abbrev(argument, "radionames")) {
      result = PRF_TOG_CHK(ch, PRF_NO_RADIO_NAMES);
      mode = 33;
    } else {
      send_to_char("That is not a valid toggle option.\r\n", ch);
      return;
    }
    if (result)
      send_to_char(tog_messages[mode][1], ch);
    else
      send_to_char(tog_messages[mode][0], ch);
  }
}

ACMD(do_slowns)
{
  extern int nameserver_is_slow;
  int result = (nameserver_is_slow = !nameserver_is_slow);

  if (result)
    send_to_char("Nameserver_is_slow changed to YES; sitenames will no longer be resolved.\r\n", ch);
  else
    send_to_char("Nameserver_is_slow changed to NO; IP addresses will now be resolved.\r\n", ch);
}

/* Assumes that *argument does start with first letter of chopped string */
ACMD(do_skills)
{
  int i;
  bool mode_all = FALSE;
  
  one_argument(argument, arg);
  
  if (!*arg) {
    snprintf(buf, sizeof(buf), "You know the following %s:\r\n", subcmd == SCMD_SKILLS ? "skills" : "abilities");
    mode_all = TRUE;
  } else {
    snprintf(buf, sizeof(buf), "You know the following %s that start with '%s':\r\n", subcmd == SCMD_SKILLS ? "skills" : "abilities", arg);
    mode_all = FALSE;
  }
  
  if (subcmd == SCMD_SKILLS) {
    // Append skills.
    for (i = MIN_SKILLS; i < MAX_SKILLS; i++) {
      if (!mode_all && *arg && !is_abbrev(arg, skills[i].name))
        continue;
      
      if (i == SKILL_ENGLISH)
        i = SKILL_ANIMAL_HANDLING;
      if ((GET_SKILL(ch, i)) > 0) {
        snprintf(buf2, sizeof(buf2), "%-30s %s\r\n", skills[i].name, how_good(i, GET_SKILL(ch, i)));
        strcat(buf, buf2);
      }
    }
    
    // Append languages.
    if (!*arg)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n\r\nYou know the following languages:\r\n");
    else
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n\r\nYou know the following languages that start with '%s':\r\n", arg);
    
    for (i = SKILL_ENGLISH; i <= SKILL_FRENCH; i++) {
      if (!mode_all && *arg && !is_abbrev(arg, skills[i].name))
        continue;
      
      if ((GET_SKILL(ch, i)) > 0) {
        snprintf(buf2, sizeof(buf2), "%-30s %s\r\n", skills[i].name, how_good(i, GET_SKILL(ch, i)));
        strcat(buf, buf2);
      }
    }
  } else {
    if (!IS_NPC(ch) && GET_TRADITION(ch) != TRAD_ADEPT) {
      send_to_char("You do not have any abilities.\r\n", ch);
      return;
    }
    extern int max_ability(int i);
    for (i = 1; i < ADEPT_NUMPOWER; i++) {
      if (!mode_all && *arg && !is_abbrev(arg, adept_powers[i]))
        continue;
      
      if (GET_POWER_TOTAL(ch, i) > 0) {
        snprintf(buf2, sizeof(buf2), "%-20s", adept_powers[i]);
        if (max_ability(i) > 1)
          switch (i) {
          case ADEPT_KILLING_HANDS:
            snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), " %-8s", wound_name[MIN(4, GET_POWER_TOTAL(ch, i))]);
            if (GET_POWER_ACT(ch, i))
              snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), " ^Y(%-8s)^n", wound_name[MIN(4, GET_POWER_ACT(ch, i))]);
            strcat(buf2, "\r\n");
            break;
          default:
            snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), " +%d", GET_POWER_TOTAL(ch, i));
            if (GET_POWER_ACT(ch, i))
              snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), " ^Y(%d)^n", GET_POWER_ACT(ch, i)); 
            strcat(buf2, "\r\n");
            break;
          }
        else
          strcat(buf2, "\r\n");
        strcat(buf, buf2);
      }
    }
    snprintf(buf2, sizeof(buf2), "You have %.2f powerpoints remaining and %.2f points of powers activated.\r\n", (float)GET_PP(ch) / 100, 
                  (float)GET_POWER_POINTS(ch) / 100);
    strcat(buf, buf2);
  }
  send_to_char(buf, ch);
}

struct obj_data * find_magazine(struct obj_data *gun, struct obj_data *i)
{
  for (; i; i = i->next_content)
  {
    if (GET_OBJ_TYPE(i) == ITEM_GUN_MAGAZINE) {
      if (GET_MAGAZINE_BONDED_MAXAMMO(i) == GET_WEAPON_MAX_AMMO(gun) && 
          GET_MAGAZINE_BONDED_ATTACKTYPE(i) == GET_WEAPON_ATTACK_TYPE(gun))
        return i;
    }
    if (i->contains && GET_OBJ_TYPE(i) == ITEM_WORN) {
      struct obj_data *found;
      found = find_magazine(gun, i->contains);
      if ( found )
        return found;
    }
  }
  return NULL;
}


ACMD(do_reload)
{
  struct obj_data *i, *gun = NULL, *m = NULL, *ammo = NULL; /* Appears unused:  *bin = NULL; */
  int n, def = 0, mount = 0;
  struct veh_data *veh = NULL;
  int ammotype = -1;

  two_arguments(argument, buf, buf1);

  // Disqualifying condition: If you're in combat and wielding an assault cannon, you don't get to reload it. (TODO: what stops someone from unwield, reload, wield?)
  if (GET_POS(ch) == POS_FIGHTING && GET_EQ(ch, WEAR_WIELD) && GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 4) == SKILL_ASSAULT_CANNON) {
    send_to_char("You have no free hands to reload with.\r\n", ch);
    return;
  }
  
  // TODO: I stopped halfway through editing this. It is BROKEN.
  {
    // In-vehicle reloading with mounts.
    if (ch->in_veh) {
      // In-vehicle mounted-weapon reloading via 'reload mount [x]'
      if (is_abbrev(buf, "mount")) {
        veh = ch->in_veh;
        mount = atoi(buf1);
        
        for (m = veh->mount; m; m = m->next_content)
          if (--mount < 0)
            break;
            
        if (!m) {
          send_to_char("There aren't that many mounts in your vehicle.\r\n", ch);
          return;
        }
      } 
      
      // No argument and manning? Reload the turret.
      else if (!*buf && (m = get_mount_manned_by_ch(ch))) {
        veh = ch->in_veh;
      }
    }
    
    // Mounted weapon reloading via 'reload <vehicle> [x]'
    else if ((veh = get_veh_list(buf, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch))) {
      if (veh->type != VEH_DRONE) {
        send_to_char("You have to be inside a vehicle to reload the mounts.\r\n", ch);
        return;
      }
      mount = atoi(buf1);
      
      for (m = veh->mount; m; m = m->next_content)
        if (--mount < 0)
          break;
          
      if (!m) {
        send_to_char(ch, "There aren't that many mounts on %s.\r\n", GET_VEH_NAME(veh));
        return;
      }
    }
    
    // Process mount reloading.
    if (veh && m) {
      // Iterate over the mount and look for its weapon and ammo box.
          
      if (!(gun = get_mount_weapon(m))) {
        send_to_char("There is no weapon attached to that mount.\r\n", ch);
        return;
      }
      
      ammo = get_mount_ammo(m);
      
      for (i = ch->carrying; i; i = i->next_content) {
        if (GET_OBJ_TYPE(i) == ITEM_GUN_AMMO &&
            GET_AMMOBOX_WEAPON(i) == GET_WEAPON_ATTACK_TYPE(gun)) {
          int max = GET_WEAPON_MAX_AMMO(gun) * 2;   
          
          // Reload an ammo box directly.
          if (ammo) {
            if (GET_AMMOBOX_QUANTITY(ammo) > 0
                && (GET_AMMOBOX_WEAPON(i) != GET_AMMOBOX_WEAPON(ammo) || 
                    GET_AMMOBOX_TYPE(i) != GET_AMMOBOX_TYPE(ammo))) {
              send_to_char(ch, "You cannot mix ammunition types in ammunition bins. The bin is currently loaded with %s, while you're using %s.\r\n", 
                           get_ammo_representation(GET_AMMOBOX_WEAPON(ammo), GET_AMMOBOX_TYPE(ammo), 0),
                           get_ammo_representation(GET_AMMOBOX_WEAPON(i), GET_AMMOBOX_TYPE(i), 0));
              return;
            }
            max -= GET_AMMOBOX_QUANTITY(ammo);
            if (max < 1) {
              send_to_char("The ammunition bin on that mount is already full.\r\n", ch);
              return;

            } else {
              max = MIN(max, GET_AMMOBOX_QUANTITY(i));
              update_ammobox_ammo_quantity(ammo, max);
              update_ammobox_ammo_quantity(i, -max);
              GET_AMMOBOX_WEAPON(ammo) = GET_AMMOBOX_WEAPON(i);
              GET_AMMOBOX_TYPE(ammo) = GET_AMMOBOX_TYPE(i);
              send_to_char(ch, "You insert %d rounds of ammunition into %s.\r\n", max, GET_OBJ_NAME(m));
              return;
            }            
          } 
          
          // No ammo box-- add a new one.
          else {
            max = MIN(max, GET_AMMOBOX_QUANTITY(i));
            ammo = read_object(OBJ_BLANK_AMMOBOX, VIRTUAL);
            GET_AMMOBOX_WEAPON(ammo) = GET_AMMOBOX_WEAPON(i);
            GET_AMMOBOX_TYPE(ammo) = GET_AMMOBOX_TYPE(i);
            GET_AMMOBOX_QUANTITY(ammo) = 0;
            update_ammobox_ammo_quantity(ammo, max);
            update_ammobox_ammo_quantity(i, -max);
            obj_to_obj(ammo, m);
            send_to_char(ch, "You insert %d rounds of ammunition into %s.\r\n", max, GET_OBJ_NAME(m));
            return;
          }
        }
      }

      send_to_char("You don't have an ammobox containing the right ammunition for that mount.\r\n", ch); 
      return;
    } /* End of mount evaluation. */
  } /* end of mounts */
  
  // Compare their argument with ammo types.
  if (*buf) {
    for (int i = NUM_AMMOTYPES - 1; i >= AMMO_NORMAL ; i--) {
      if (str_str(ammo_type[i].name, buf)) {
        ammotype = i;
        *buf = '\0';
        break;
      }
    }
  }
  
  // Reload with no argument.
  if (!*buf) {
    // Check wielded weapon.
    if (is_reloadable_weapon(GET_EQ(ch, WEAR_WIELD), ammotype))
      gun = GET_EQ(ch, WEAR_WIELD);
    
    // Check held weapon.
    else if (is_reloadable_weapon(GET_EQ(ch, WEAR_HOLD), ammotype))
      gun = GET_EQ(ch, WEAR_HOLD);
      
    // Start scanning.
    else {
      // Scan equipped items, excluding light.
      for (n = 0; n < (NUM_WEARS - 1) && !gun; n++)
        if (is_reloadable_weapon(GET_EQ(ch, n), ammotype))
          gun = GET_EQ(ch, n);
      
      // Scan carried items.
      for (i = ch->carrying; i && !gun; i = i->next_content)
        if (is_reloadable_weapon(i, ammotype))
          gun = i;
    }
    
    if (!gun) {
      send_to_char("No weapons in need of reloading found.\r\n", ch);
      return;
    }
    def = 1;
  } 
  
  else if (!(gun = get_object_in_equip_vis(ch, buf, ch->equipment, &n)))
    if (!(gun = get_obj_in_list_vis(ch, buf, ch->carrying))) {
      send_to_char(ch, "You don't have a '%s'.\r\n", buf);
      return;
    }

  if (GET_OBJ_TYPE(gun) != ITEM_WEAPON
      || !IS_GUN(GET_WEAPON_ATTACK_TYPE(gun))
      || GET_WEAPON_MAX_AMMO(gun) <= 0) {
    send_to_char(ch, "%s is not a reloadable weapon!\r\n", capitalize(GET_OBJ_NAME(gun)));
    return;
  }
  
  // At this point, we know we have a reloadable weapon. All we have to do is parse out the ammotype.
  
  // No ammotype? Reload with whatever's in it (normal if nothing).
  if (!*buf1) {
    reload_weapon_from_bulletpants(ch, gun, ammotype);
    return;
  }
  
  // Compare their buf1 against the valid ammo types.
  for (int am = NUM_AMMOTYPES - 1; am >= AMMO_NORMAL ; am--) {
    if (str_str(ammo_type[am].name, buf1)) {
      ammotype = am;
      break;
    }
  }
  
  // They entered something, but it wasn't kosher.
  if (ammotype == -1) {
    send_to_char(ch, "'%s' is not a recognized ammo type. Valid types are: ", buf1);
    bool is_first = TRUE;
    for (int am = AMMO_NORMAL; am < NUM_AMMOTYPES; am++) {
      send_to_char(ch, "%s%s", is_first ? "" : ", ", ammo_type[am].name);
      is_first = FALSE;
    }
    send_to_char(".\r\n", ch);
    return;
  }
  
  // Good to go.
  reload_weapon_from_bulletpants(ch, gun, ammotype);
}

ACMD(do_eject)
{
  if (GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_WIELD)->contains) {
    struct obj_data *magazine = GET_EQ(ch, WEAR_WIELD)->contains;
    
    // Strip out the ammo and put it in your bullet pants, then destroy the mag.
    update_bulletpants_ammo_quantity(ch, GET_MAGAZINE_BONDED_ATTACKTYPE(magazine), GET_MAGAZINE_AMMO_TYPE(magazine), GET_MAGAZINE_AMMO_COUNT(magazine));
    obj_from_obj(magazine);
    extract_obj(magazine);
    act("$n ejects and pockets a magazine from $p.", FALSE, ch, GET_EQ(ch, WEAR_WIELD), NULL, TO_ROOM);
    act("You eject and pocket a magazine from $p.", FALSE, ch, GET_EQ(ch, WEAR_WIELD), NULL, TO_CHAR);
  } else {
    send_to_char(ch, "But it's already empty.\r\n");
  }
}

ACMD(do_attach)
{
  struct veh_data *veh = NULL;
  struct obj_data *item = NULL, *item2 = NULL;
  int j;

  argument = any_one_arg(argument, buf1);
  argument = any_one_arg(argument, buf2);
  argument = one_argument(argument, arg);

  if (!*buf1 || !*buf2) {
    send_to_char("You need to attach something to something else.\r\n", ch);
    return;
  }
  if (*arg) {
    if (ch->in_veh || !(veh = get_veh_list(buf1, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch))) {
      send_to_char(ch, "You don't see any vehicles named '%s' here.\r\n", buf1);
      return;
    }
    if (veh->type != VEH_DRONE) {
      send_to_char("You have to be inside to attach that.\r\n", ch);
      return;
    }
    strcpy(buf1, buf2);
    strcpy(buf2, arg);
  } else
    veh = ch->in_veh;
  if (veh && (j = atoi(buf2)) >= 0) {
    if (!(item = get_obj_in_list_vis(ch, buf1, ch->carrying))) {
      send_to_char("Attach what?\r\n", ch);
      return;
    }
    if (GET_OBJ_TYPE(item) != ITEM_WEAPON) {
      send_to_char(ch, "%s is not a weapon you can attach to %s.\r\n", capitalize(GET_OBJ_NAME(item)), GET_VEH_NAME(veh));
      return;
    }
    for (item2 = veh->mount; item2; item2 = item2->next_content)
      if (--j < 0)
        break;
    if (!item2) {
      send_to_char("There aren't that many mounts.\r\n", ch);
      return;
    }
    if (mount_has_weapon(item2)) {
      send_to_char("There is already a weapon mounted on it.\r\n", ch);
      return;
    }
    if (!IS_GUN(GET_OBJ_VAL(item, 3)) || veh->usedload + GET_OBJ_WEIGHT(item) > veh->load) {
      send_to_char("You can't seem to fit it on.\r\n", ch);
      return;
    }
    switch (GET_OBJ_VAL(item, 4)) {
      case SKILL_GUNNERY:
      case SKILL_MACHINE_GUNS:
      case SKILL_MISSILE_LAUNCHERS:
      case SKILL_ASSAULT_CANNON:
        if (GET_OBJ_VAL(item2, 1) < 2) {
          send_to_char("You need a hardpoint or larger to mount this weapon in.\r\n", ch);
          return;
        }
    }
    veh->usedload += GET_OBJ_WEIGHT(item);
    obj_from_char(item);
    obj_to_obj(item, item2);
    send_to_char(ch, "You mount %s on %s.\r\n", GET_OBJ_NAME(item), GET_OBJ_NAME(item2));
    act("$n mounts $p on $P.", FALSE, ch, item, item2, TO_ROOM);
    return;
  } // End of vehicle weapon attachment.

  if (!(item = get_obj_in_list_vis(ch, buf1, ch->carrying))) {
    send_to_char(ch, "You don't seem to have any '%s'.\r\n", buf1);
    return;
  }
  
  if (!(item2 = get_obj_in_list_vis(ch, buf2, ch->carrying))) {
    if (!(item2 = get_obj_in_list_vis(ch, buf2, GET_EQ(ch, WEAR_WIELD))) && !(item2 = get_obj_in_list_vis(ch, buf2, GET_EQ(ch, WEAR_HOLD)))) {
      send_to_char(ch, "You don't seem to have any '%s'.\r\n", buf2);
      return;
    } else {
      send_to_char(ch, "You'll have a hard time attaching anything to %s while you're wielding it.\r\n", GET_OBJ_NAME(item2));
      return;
    }
  }

  // If we failed to attach it, don't destroy the attachment.
  if (!attach_attachment_to_weapon(item, item2, ch, GET_ACCESSORY_ATTACH_LOCATION(item)))
    return;
  
  // Trash the actual accessory object-- the game will look it up by vnum if it's ever needed.
  obj_from_char(item);
  extract_obj(item);
}

ACMD(do_unattach)
{
  struct veh_data *veh;
  struct obj_data *item, *gun;
  int i, j;
  bool found = FALSE;

  argument = any_one_arg(argument, buf1);
  argument = any_one_arg(argument, buf2);
  if (!ch->in_veh && (veh = get_veh_list(buf1, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch))) {
    if (veh->type != VEH_DRONE) {
      send_to_char("You have to be inside to unattach that.\r\n", ch);
      return;
    }
  } else
    veh = ch->in_veh;

  if (veh && (j = atoi(veh->type == VEH_DRONE ? buf2 : buf1)) >= 0) {
    for (item = veh->mount; item; item = item->next_content)
      if (--j < 0)
        break;
    if (!item)
      send_to_char("There aren't that many mounts.\r\n", ch);
    else if (item->worn_by)
      send_to_char(ch, "Someone is manning that mount.\r\n");
    else if (!((gun = get_mount_weapon(item)) || (gun = item->contains)))
      send_to_char("There isn't anything mounted on it.\r\n", ch);
    else {
      if (can_take_obj(ch, gun)) {
        obj_from_obj(gun);
        obj_to_char(gun, ch);
        send_to_char(ch, "You remove %s from %s.\r\n", GET_OBJ_NAME(gun), GET_OBJ_NAME(item));
        act("$n removes $p from $P.", FALSE, ch, gun, item, TO_ROOM);
        // TODO: Will this cause the vehicle load to go negative in the case of a gun ammo box?
        veh->usedload -= GET_OBJ_WEIGHT(gun);
        
        while (item->contains) {
          struct obj_data *removed_thing = item->contains;
          obj_from_obj(removed_thing);
          obj_to_char(removed_thing, ch);
          send_to_char(ch, "You also remove %s from %s.\r\n", GET_OBJ_NAME(removed_thing), GET_OBJ_NAME(item));
        }
      } else
        send_to_char(ch, "You can't seem to hold it.\r\n");
    }
    return;
  }
  if (!*buf1 || !*buf2) {
    send_to_char("You need to unattach something from something else.\r\n", ch);
    return;
  }
  if (!(gun = get_obj_in_list_vis(ch, buf2, ch->carrying))) {
    send_to_char(ch, "You don't seem to have a '%s'.\r\n", buf2);
    return;
  }

  if (GET_OBJ_TYPE(gun) != ITEM_WEAPON) {
    send_to_char("You can only unattach accessories from weapons.\r\n", ch);
    return;
  }

  for (i = ACCESS_LOCATION_TOP; i <= ACCESS_LOCATION_UNDER && !found; i++)
    if (GET_OBJ_VAL(gun, i) > 0 && isname(buf1, short_object(GET_OBJ_VAL(gun, i), 1))) {
      found = TRUE;
      break;
    }

  if (!found) {
    snprintf(buf, sizeof(buf), "There doesn't seem to be %s %s attached to $p.", AN(buf1), buf1);
    act(buf, FALSE, ch, gun, 0, TO_CHAR);
    return;
  }

  item = unattach_attachment_from_weapon(i, gun, ch);
  if (item)
    obj_to_char(item, ch);
}

ACMD(do_treat)
{
  struct char_data *vict;
  struct obj_data *obj;
  int target = 0, i, found = 0, shop = 0;

  if (subcmd && (!IS_NPC(ch) || !(GET_MOB_SPEC(ch) || GET_MOB_SPEC2(ch))))
    return;

  if (IS_ASTRAL(ch)) {
    send_to_char("You can't physically treat someone while projecting.\r\n", ch);
    return;
  }
  if (CH_IN_COMBAT(ch)) {
    send_to_char("Administer first aid while fighting?!?\r\n", ch);
    return;
  }

  if (!*argument) {
    send_to_char("Treat who?!\r\n", ch);
    return;
  }

  any_one_arg(argument, arg);
  if (!(vict = get_char_room_vis(ch, arg))) {
    send_to_char(ch, "You can't seem to find a '%s' here.\r\n", arg);
    return;
  }

  if (vict == ch) {
    send_to_char("You can't treat yourself!\r\n", ch);
    return;
  } else if (CH_IN_COMBAT(vict)) {
    act("Not while $E's fighting!", FALSE, ch, 0, vict, TO_CHAR);
    return;
  } else if (GET_POS(vict) > POS_LYING && !subcmd) {
    act("$N must at least be lying down to receive first aid.", FALSE, ch, 0, vict, TO_CHAR);
    return;
  } else if (LAST_HEAL(vict) != 0
             || (!IS_NPC(vict)
                 && !IS_SENATOR(vict)
                 && IS_SENATOR(ch)
                 && !access_level(ch, LVL_ADMIN))) {
    act("Treating $N will not do $M any good.", FALSE, ch, 0, vict, TO_CHAR);
    return;
  }

  if (GET_PHYSICAL(vict) < 100)
    target = 10;
  else if (GET_PHYSICAL(vict) <= (GET_MAX_PHYSICAL(vict) * 2/5))
    target = 8;
  else if (GET_PHYSICAL(vict) <= (GET_MAX_PHYSICAL(vict) * 7/10))
    target = 6;
  else if (GET_PHYSICAL(vict) <= (GET_MAX_PHYSICAL(vict) * 9/10))
    target = 4;
  else {
    act("$N doesn't need to be treated.", FALSE, ch, 0, vict, TO_CHAR);
    if (subcmd) {
      char buf[400];
      snprintf(buf, sizeof(buf), "%s Treatment will do you no good.", GET_CHAR_NAME(vict));
      do_say(ch, buf, 0, SCMD_SAYTO);
    }
    return;
  }
  i = get_skill(ch, SKILL_BIOTECH, target);

  if (find_workshop(ch, TYPE_MEDICAL))
    shop = 1;
  for (obj = ch->carrying; obj && !found; obj = obj->next_content)
    if (GET_OBJ_TYPE(obj) == ITEM_WORKSHOP && GET_OBJ_VAL(obj, 1) == 0 && GET_OBJ_VAL(obj, 0) == 6)
      found = 1;
  for (i = 0; !found && i < (NUM_WEARS - 1); i++)
    if ((obj = GET_EQ(ch, i)) && GET_OBJ_TYPE(obj) == ITEM_WORKSHOP && GET_OBJ_VAL(obj, 1) == 0 && GET_OBJ_VAL(obj, 0) == 6)
      found = 1;
  if (!found && !subcmd && !shop)
    target += 4;

  if (!shop)
    target++;
  if (GET_REAL_BOD(vict) >= 10)
    target -= 3;
  else if (GET_REAL_BOD(vict) >= 7)
    target -= 2;
  else if (GET_REAL_BOD(vict) >= 4)
    target--;

  if (vict->real_abils.mag > 0)
    target += 2;

  act("$n begins to treat $N.", TRUE, ch, 0, vict, TO_NOTVICT);
  if (success_test(i, target)) {
    act("$N appears better.", FALSE, ch, 0, vict, TO_CHAR);
    act("The pain seems significantly less after $n's treatment.",
        FALSE, ch, 0, vict, TO_VICT);
    if (GET_PHYSICAL(vict) < 100) {
      GET_PHYSICAL(vict) = MIN(GET_MAX_PHYSICAL(vict), 100);
      GET_MENTAL(vict) = 0;
      GET_POS(vict) = POS_STUNNED;
      LAST_HEAL(vict) = MAX(1, (int)(GET_MAX_PHYSICAL(vict) / 100));
    } else if (GET_PHYSICAL(vict) <= (GET_MAX_PHYSICAL(vict) * 2/5)) {
      GET_PHYSICAL(vict) += (int)(GET_MAX_PHYSICAL(vict) * 3/1000);
      LAST_HEAL(vict) = (int)(GET_MAX_PHYSICAL(vict) * 3/1000);
    } else if (GET_PHYSICAL(vict) <= (GET_MAX_PHYSICAL(vict) * 7/10)) {
      GET_PHYSICAL(vict) += (int)(GET_MAX_PHYSICAL(vict) / 500);
      LAST_HEAL(vict) = (int)(GET_MAX_PHYSICAL(vict) / 500);
    } else if (GET_PHYSICAL(vict) <= (GET_MAX_PHYSICAL(vict) * 9/10)) {
      GET_PHYSICAL(vict) += (int)(GET_MAX_PHYSICAL(vict) / 1000);
      LAST_HEAL(vict) = (int)(GET_MAX_PHYSICAL(vict) / 1000);
    }
  } else {
    act("Your treatment does nothing for $N.", FALSE, ch, 0, vict, TO_CHAR);
    act("$n's treatment doesn't help your wounds.", FALSE, ch, 0, vict, TO_VICT);
    LAST_HEAL(vict) = 3;
  }
}

ACMD(do_astral)
{
  struct char_data *astral;
  int r_num;

  one_argument(argument, arg);

  if (GET_TRADITION(ch) != TRAD_SHAMANIC && GET_TRADITION(ch) != TRAD_HERMETIC &&
      !access_level(ch, LVL_ADMIN) && !(GET_TRADITION(ch) == TRAD_ADEPT &&
                                        GET_POWER(ch, ADEPT_PERCEPTION) > 0 && subcmd == SCMD_PERCEIVE)) {
    send_to_char("You can't do that!\r\n", ch);
    return;
  }
  if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }
  if (IS_NPC(ch))
    return;
  if (GET_ASPECT(ch) != ASPECT_FULL && subcmd == SCMD_PROJECT) {
    send_to_char("You do not have enough control over the astral plane to do that.\r\n", ch);
    return;
  }
  if (IS_PROJECT(ch)) {
    send_to_char("But you are already projecting!\r\n", ch);
    return;
  }

  if (AFF_FLAGGED(ch, AFF_PILOT)) {
    send_to_char("While driving? That would be a nice trick!\r\n", ch);
    return;
  }
  if (subcmd == SCMD_PERCEIVE) {
    if (PLR_FLAGGED(ch, PLR_PERCEIVE)) {
      PLR_FLAGS(ch).RemoveBit(PLR_PERCEIVE);
      send_to_char("You return to your physical senses.\r\n", ch);
    } else {
      PLR_FLAGS(ch).SetBit(PLR_PERCEIVE);
      send_to_char("Your physical body seems distant, as the astral plane slides into view.\r\n", ch);
    }
    return;
  }

  if (ch->desc->original) {
    send_to_char("You can't project now.\r\n", ch);
    return;
  } else if (CH_IN_COMBAT(ch)) {
    send_to_char("You can't project while fighting!\r\n", ch);
    return;
  } else if (!ch->player_specials || ch->player_specials == &dummy_mob) {
    send_to_char("That won't work, for some reason.\r\n", ch);
    return;
  }

  if ((r_num = real_mobile(22)) < 0) {
    log("No astral mob");
    send_to_char("The astral plane is strangely inaccessible...\r\n", ch);
    return;
  }

  if (PLR_FLAGGED(ch, PLR_PERCEIVE)) {
    PLR_FLAGS(ch).RemoveBit(PLR_PERCEIVE);
    send_to_char("You briefly return your perception to your physical senses.\r\n", ch);
  }

  if (!ch->player.astral_text.keywords)
    ch->player.astral_text.keywords = str_dup("reflection");
  if (!ch->player.astral_text.name)
    ch->player.astral_text.name = str_dup("a reflection");
  if (!ch->player.astral_text.room_desc)
    ch->player.astral_text.room_desc = str_dup("The reflection of some physical being stands here.\r\n");

  GET_POS(ch) = POS_SITTING;
  astral = read_mobile(r_num, REAL);

  astral->player.physical_text.keywords  = ch->player.astral_text.keywords;
  astral->player.physical_text.name      = ch->player.astral_text.name;
  astral->player.physical_text.room_desc = ch->player.astral_text.room_desc;
  astral->player.physical_text.look_desc = ch->player.astral_text.look_desc;

  GET_PHYSICAL(astral) = GET_PHYSICAL(ch);
  GET_MENTAL(astral) = GET_MENTAL(ch);

  GET_REAL_STR(astral) = GET_REAL_CHA(ch);
  GET_STR(astral) = GET_REAL_CHA(ch);
  GET_REAL_QUI(astral) = GET_REAL_INT(ch);
  GET_QUI(astral) = GET_REAL_INT(ch);
  GET_REAL_BOD(astral) = GET_REAL_WIL(ch);
  GET_BOD(astral) = GET_REAL_WIL(ch);
  GET_REAL_REA(astral) = GET_REAL_INT(ch);
  GET_REA(astral) = GET_REAL_INT(ch);
  astral->real_abils.mag = GET_MAG(ch);
  GET_MAG(astral) = GET_MAG(ch);
  GET_REAL_INT(astral) = GET_INT(ch);
  GET_INT(astral) = GET_INT(ch);
  GET_REAL_WIL(astral) = GET_WIL(ch);
  GET_WIL(astral) = GET_WIL(ch);
  GET_REAL_CHA(astral) = GET_REAL_CHA(ch);
  GET_CHA(astral) = GET_REAL_CHA(ch);
  astral->real_abils.ess = GET_ESS(ch);
  GET_ESS(astral) = GET_ESS(ch);
  GET_TRADITION(astral) = GET_TRADITION(ch);
  GET_TOTEM(astral) = GET_TOTEM(ch);
  GET_ALIASES(astral) = GET_ALIASES(ch);

  for (int i = 0; i < MAX_SKILLS; i++)
    astral->char_specials.saved.skills[i][0] = ch->char_specials.saved.skills[i][0];
  for (int i = 0; i < META_MAX; i++)
    GET_METAMAGIC(astral, i) = GET_METAMAGIC(ch, i);
  GET_GRADE(astral) = GET_GRADE(ch);
  GET_ASTRAL(astral) = GET_ASTRAL(ch);
  GET_COMBAT(astral) = GET_ASTRAL(ch);
  GET_MAGIC(astral) = GET_MAGIC(ch);
  GET_MEMORY(astral) = GET_MEMORY(ch);

  if (ch->in_veh)
    char_to_room(astral, ch->in_veh->in_room);
  else
    char_to_room(astral, ch->in_room);

  int i = 0;
  for (; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_FOCUS && GET_OBJ_VAL(GET_EQ(ch, i), 2) == GET_IDNUM(ch) &&
        GET_OBJ_VAL(GET_EQ(ch, i), 4)) {
      struct obj_data *obj = read_object(GET_OBJ_VNUM(GET_EQ(ch, i)), VIRTUAL);
      for (int x = 0; x < NUM_VALUES; x++)
        GET_OBJ_VAL(obj, x) = GET_OBJ_VAL(GET_EQ(ch, i), x);
      if (GET_EQ(ch, i)->restring)
        obj->restring = str_dup(GET_EQ(ch, i)->restring);
      equip_char(astral, obj, i);
    }
  affect_total(astral);
  ch->desc->character = astral;
  ch->desc->original = ch;

  astral->desc = ch->desc;
  ch->desc = NULL;

  act("$n leaves $s body behind, entering a deep trance.", TRUE, ch, 0, astral, TO_NOTVICT);
  act("You enter the Astral Plane.", FALSE, astral, 0, 0, TO_CHAR);
  act("$n swirls into view.", TRUE, astral, 0, 0, TO_ROOM);

  PLR_FLAGS(ch).SetBit(PLR_PROJECT);
  look_at_room(astral, 1);
}

ACMD(do_customize)
{
  struct obj_data *cyber;
  int found = 0;

  if (CH_IN_COMBAT(ch)) {
    send_to_char("You can't customize your descriptions while fighting!\r\n", ch);
    return;
  }

  if (IS_NPC(ch)) {
    send_to_char("You can't right now.\r\n", ch);
    return;
  }

  skip_spaces(&argument);
  if (!*argument) {
     send_to_char(ch, "Usage: customize <persona/physical%s%s/background>\r\n", ch, (GET_TRADITION(ch) == TRAD_SHAMANIC || GET_TRADITION(ch) == TRAD_HERMETIC) && GET_ASPECT(ch) == ASPECT_FULL ? "/reflection" : "",
                 GET_TRADITION(ch) == TRAD_HERMETIC || GET_TRADITION(ch) == TRAD_SHAMANIC ? "/magic" : "");
    return;
  }
  if (is_abbrev(argument, "magic")) {}
  else if (is_abbrev(argument, "physical"))
    STATE(ch->desc) = CON_FCUSTOMIZE;
  else if (is_abbrev(argument, "persona")) {
    for (cyber = ch->cyberware; !found && cyber; cyber = cyber->next_content)
      if (GET_OBJ_VAL(cyber, 0) == CYB_DATAJACK || (GET_OBJ_VAL(cyber, 0) == CYB_EYES && IS_SET(GET_OBJ_VAL(cyber, 3), EYE_DATAJACK)))
        found = 1;
    if (!found) {
      send_to_char("But you don't even have a datajack?!\r\n", ch);
      return;
    }
    STATE(ch->desc) = CON_PCUSTOMIZE;
  } else if (is_abbrev(argument, "reflection")) {
    if (!IS_SENATOR(ch) && GET_TRADITION(ch) != TRAD_SHAMANIC &&
        GET_TRADITION(ch) != TRAD_HERMETIC) {
      send_to_char("And just why would you need to do that?!\r\n", ch);
      return;
    }
    STATE(ch->desc) = CON_ACUSTOMIZE;
  } else if (is_abbrev(argument, "background"))
    STATE(ch->desc) = CON_BCUSTOMIZE;
  else {
    send_to_char("Usage: customize <persona/reflection/physical/background>\r\n", ch);
    return;
  }

  PLR_FLAGS(ch).SetBit(PLR_CUSTOMIZE);

  send_to_char("Do you wish to customize your descriptions?\r\n", ch);

  ch->desc->edit_mode = CEDIT_CONFIRM_EDIT;
}

void cedit_disp_menu(struct descriptor_data *d, int mode)
{
  CLS(CH);
  if (STATE(d) == CON_BCUSTOMIZE) {
    send_to_char(CH, "1) Character Background: ^c%s^n", d->edit_mob->player.background);
  } else {
    send_to_char(CH, "1) Alias: %s%s%s\r\n", CCCYN(CH, C_CMP),
                 d->edit_mob->player.physical_text.keywords, CCNRM(CH, C_CMP));
    send_to_char(CH, "2) Short Description: %s%s%s\r\n", CCCYN(CH, C_CMP),
                 d->edit_mob->player.physical_text.name, CCNRM(CH, C_CMP));
    if (STATE(d) == CON_FCUSTOMIZE)
      send_to_char(CH, "3) Voice: %s%s%s\r\n", CCCYN(CH, C_CMP),
                   d->edit_mob->player.physical_text.room_desc, CCNRM(CH, C_CMP));
    else
      send_to_char(CH, "3) Room description: %s%s%s\r\n", CCCYN(CH, C_CMP),
                   d->edit_mob->player.physical_text.room_desc, CCNRM(CH, C_CMP));
    send_to_char(CH, "4) Look description: %s%s%s\r\n", CCCYN(CH, C_CMP),
                 d->edit_mob->player.physical_text.look_desc, CCNRM(CH, C_CMP));
    if (STATE(d) == CON_FCUSTOMIZE)
    {
      send_to_char(CH, "5) Arriving Text: ^c%s^n\r\n", d->edit_mob->char_specials.arrive);
      send_to_char(CH, "6) Leaving Text:  ^c%s^n\r\n", d->edit_mob->char_specials.leave);
      if (PLR_FLAGGED(CH, PLR_NEWBIE)) {
        send_to_char(CH, "7) Change Height: ^c%dcm^n\r\n", GET_HEIGHT(CH));
        send_to_char(CH, "8) Change Weight: ^c%dkg^n\r\n", GET_WEIGHT(CH));
      }
      send_to_char(CH, "Examples:\r\n  %s %s the north.\r\n", d->edit_mob->player.physical_text.name, d->edit_mob->char_specials.arrive);
      send_to_char(CH, "  %s %s north.\r\n", d->edit_mob->player.physical_text.name, d->edit_mob->char_specials.leave);
    }
  }
  if (mode)
    send_to_char(CH, "q) Quit\r\nLine too long (max %d characters); function aborted.\r\n"
                 "Enter your choice:\r\n", LINE_LENGTH - 1);
  else
    send_to_char("q) Quit\r\nEnter your choice:\r\n", CH);
  d->edit_mode = CEDIT_MAIN_MENU;
}

void cedit_parse(struct descriptor_data *d, char *arg)
{
  char buf3[MAX_STRING_LENGTH*10];
  switch (d->edit_mode)
  {
  case CEDIT_CONFIRM_EDIT:
    switch (*arg) {
    case 'y':
    case 'Y':
      d->edit_mob = Mem->GetCh();
      d->edit_mob->player_specials = &dummy_mob;

      if (STATE(d) == CON_BCUSTOMIZE)
        d->edit_mob->player.background = str_dup(CH->player.background);
      else if (STATE(d) == CON_FCUSTOMIZE) {
        d->edit_mob->player.physical_text.keywords =
          str_dup(CH->player.physical_text.keywords);
        d->edit_mob->player.physical_text.name =
          str_dup(CH->player.physical_text.name);
        d->edit_mob->player.physical_text.room_desc =
          str_dup(CH->player.physical_text.room_desc);
        d->edit_mob->player.physical_text.look_desc =
          str_dup(CH->player.physical_text.look_desc);
        d->edit_mob->char_specials.arrive = str_dup(CH->char_specials.arrive);
        d->edit_mob->char_specials.leave = str_dup(CH->char_specials.leave);
      } else if (STATE(d) == CON_PCUSTOMIZE) {
        d->edit_mob->player.physical_text.keywords =
          str_dup(CH->player.matrix_text.keywords);
        d->edit_mob->player.physical_text.name =
          str_dup(CH->player.matrix_text.name);
        d->edit_mob->player.physical_text.room_desc =
          str_dup(CH->player.matrix_text.room_desc);
        d->edit_mob->player.physical_text.look_desc =
          str_dup(CH->player.matrix_text.look_desc);
      } else {
        d->edit_mob->player.physical_text.keywords =
          str_dup(CH->player.astral_text.keywords);
        d->edit_mob->player.physical_text.name =
          str_dup(CH->player.astral_text.name);
        d->edit_mob->player.physical_text.room_desc =
          str_dup(CH->player.astral_text.room_desc);
        d->edit_mob->player.physical_text.look_desc =
          str_dup(CH->player.astral_text.look_desc);
      }

      cedit_disp_menu(d, 0);

      break;

    case 'n':
    case 'N':
      STATE(d) = CON_PLAYING;
      d->edit_mode = 0;
      PLR_FLAGS(CH).RemoveBit(PLR_CUSTOMIZE);

      break;

    default:
      send_to_char("That's not a valid choice.\r\n", CH);
      send_to_char("Do you wish to customize your descriptions? (y/n)\r\n", CH);
      break;
    }
    break;

  case CEDIT_CONFIRM_SAVESTRING:
    switch (*arg) {
    case 'y':
    case 'Y':
      PLR_FLAGS(CH).RemoveBit(PLR_CUSTOMIZE);
      strcpy(buf2, "UPDATE pfiles SET ");
      if (STATE(d) == CON_BCUSTOMIZE) {
        DELETE_ARRAY_IF_EXTANT(CH->player.background);
        CH->player.background = str_dup(d->edit_mob->player.background);
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "background='%s'", prepare_quotes(buf3, CH->player.background, sizeof(buf3) / sizeof(buf3[0])));
      } else if (STATE(d) == CON_FCUSTOMIZE) {
        
        DELETE_ARRAY_IF_EXTANT(CH->player.physical_text.keywords);
        if (!strstr(GET_KEYWORDS(d->edit_mob), GET_CHAR_NAME(d->character))) {
          snprintf(buf, sizeof(buf), "%s %s", GET_KEYWORDS(d->edit_mob), GET_CHAR_NAME(d->character));
          CH->player.physical_text.keywords = str_dup(buf);
        } else if (strstr(GET_KEYWORDS(d->edit_mob), "LS!TB") && (SHFT(CH)<<=4)) {
          snprintf(buf, sizeof(buf), "%s", GET_CHAR_NAME(d->character));
          CH->player.physical_text.keywords = str_dup(buf);
        } else
          CH->player.physical_text.keywords = str_dup(GET_KEYWORDS(d->edit_mob));
        
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Physical_Keywords='%s'", prepare_quotes(buf3, CH->player.physical_text.keywords, sizeof(buf3) / sizeof(buf3[0])));

        DELETE_ARRAY_IF_EXTANT(CH->player.physical_text.name);
        CH->player.physical_text.name = str_dup(d->edit_mob->player.physical_text.name);
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), ", Physical_Name='%s'", prepare_quotes(buf3, CH->player.physical_text.name, sizeof(buf3) / sizeof(buf3[0])));

        DELETE_ARRAY_IF_EXTANT(CH->player.physical_text.room_desc);
        CH->player.physical_text.room_desc = str_dup(d->edit_mob->player.physical_text.room_desc);
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), ", Voice='%s'", prepare_quotes(buf3, CH->player.physical_text.room_desc, sizeof(buf3) / sizeof(buf3[0])));

        DELETE_ARRAY_IF_EXTANT(CH->player.physical_text.look_desc);
        CH->player.physical_text.look_desc = str_dup(d->edit_mob->player.physical_text.look_desc);
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), ", Physical_LookDesc='%s'", prepare_quotes(buf3, CH->player.physical_text.look_desc, sizeof(buf3) / sizeof(buf3[0])));
        
        DELETE_ARRAY_IF_EXTANT(CH->char_specials.arrive);
        CH->char_specials.arrive = str_dup(d->edit_mob->char_specials.arrive);
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), ", EnterMsg='%s'", prepare_quotes(buf3, CH->char_specials.arrive, sizeof(buf3) / sizeof(buf3[0])));

        DELETE_ARRAY_IF_EXTANT(CH->char_specials.leave);
        CH->char_specials.leave = str_dup(d->edit_mob->char_specials.leave);
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), ", LeaveMsg='%s', Height=%d, Weight=%d", prepare_quotes(buf3, CH->char_specials.leave, sizeof(buf3) / sizeof(buf3[0])),
                GET_HEIGHT(CH), GET_WEIGHT(CH));
      } else if (STATE(d) == CON_PCUSTOMIZE) {
        DELETE_ARRAY_IF_EXTANT(CH->player.matrix_text.keywords);
        CH->player.matrix_text.keywords = str_dup(GET_KEYWORDS(d->edit_mob));
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Matrix_Keywords='%s'", prepare_quotes(buf3, CH->player.matrix_text.keywords, sizeof(buf3) / sizeof(buf3[0])));

        DELETE_ARRAY_IF_EXTANT(CH->player.matrix_text.name);
        CH->player.matrix_text.name = str_dup(d->edit_mob->player.physical_text.name);
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), ", Matrix_Name='%s'", prepare_quotes(buf3, CH->player.matrix_text.name, sizeof(buf3) / sizeof(buf3[0])));

        DELETE_ARRAY_IF_EXTANT(CH->player.matrix_text.room_desc);
        CH->player.matrix_text.room_desc = str_dup(d->edit_mob->player.physical_text.room_desc);
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), ", Matrix_RoomDesc='%s'", prepare_quotes(buf3, CH->player.matrix_text.room_desc, sizeof(buf3) / sizeof(buf3[0])));

        DELETE_ARRAY_IF_EXTANT(CH->player.matrix_text.look_desc);
        CH->player.matrix_text.look_desc = str_dup(d->edit_mob->player.physical_text.look_desc);
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), ", Matrix_LookDesc='%s'", prepare_quotes(buf3, CH->player.matrix_text.look_desc, sizeof(buf3) / sizeof(buf3[0])));
      } else {
        DELETE_ARRAY_IF_EXTANT(CH->player.astral_text.keywords);
        CH->player.astral_text.keywords = str_dup(GET_KEYWORDS(d->edit_mob));
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Astral_Keywords='%s'", prepare_quotes(buf3, CH->player.astral_text.keywords, sizeof(buf3) / sizeof(buf3[0])));

        DELETE_ARRAY_IF_EXTANT(CH->player.astral_text.name);
        CH->player.astral_text.name = str_dup(d->edit_mob->player.physical_text.name);
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), ", Astral_Name='%s'", prepare_quotes(buf3, CH->player.astral_text.name, sizeof(buf3) / sizeof(buf3[0])));

        DELETE_ARRAY_IF_EXTANT(CH->player.astral_text.room_desc);
        CH->player.astral_text.room_desc = str_dup(d->edit_mob->player.physical_text.room_desc);
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), ", Astral_RoomDesc='%s'", prepare_quotes(buf3, CH->player.astral_text.room_desc, sizeof(buf3) / sizeof(buf3[0])));

        DELETE_ARRAY_IF_EXTANT(CH->player.astral_text.look_desc);
        CH->player.astral_text.look_desc = str_dup(d->edit_mob->player.physical_text.look_desc);
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), ", Astral_LookDesc='%s'", prepare_quotes(buf3, CH->player.astral_text.look_desc, sizeof(buf3) / sizeof(buf3[0])));
      }

      if (d->edit_mob)
        Mem->DeleteCh(d->edit_mob);

      d->edit_mob = NULL;
      d->edit_mode = 0;
      STATE(d) = CON_PLAYING;
      playerDB.SaveChar(CH);
      snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), " WHERE idnum=%ld;", GET_IDNUM(CH));
      mysql_wrapper(mysql, buf2);
      break;

    case 'n':
    case 'N':
      PLR_FLAGS(CH).RemoveBit(PLR_CUSTOMIZE);

      if (d->edit_mob)
        Mem->DeleteCh(d->edit_mob);

      d->edit_mob = NULL;
      d->edit_mode = 0;

      STATE(d) = CON_PLAYING;

      break;

    default:
      send_to_char("Invalid choice, please enter yes or no.\r\n"
                   "Do you wish to save the changes?\r\n", CH);
      break;
    }
    break;
  case CEDIT_MAIN_MENU:
    switch (*arg) {
    case 'q':
    case 'Q':
      send_to_char("Do you wish to save the changes?\r\n", CH);
      d->edit_mode = CEDIT_CONFIRM_SAVESTRING;
      break;

    case '1':
      if (STATE(d) == CON_BCUSTOMIZE) {
        d->edit_mode = CEDIT_DESC;
        DELETE_D_STR_IF_EXTANT(d);
        INITIALIZE_NEW_D_STR(d);
        d->max_str = EXDSCR_LENGTH;
        d->mail_to = 0;
        return;
      } else if (STATE(d) == CON_PCUSTOMIZE)
        send_to_char("Enter persona aliases: ", CH);
      else if (STATE(d) == CON_ACUSTOMIZE)
        send_to_char("Enter reflection aliases: ", CH);
      else
        send_to_char("Enter physical aliases: ", CH);
      d->edit_mode = CEDIT_ALIAS;
      break;
    case '2':
      send_to_char("Enter short description: ", CH);
      d->edit_mode = CEDIT_SHORT_DESC;
      break;
    case '3':
      if (STATE(d) == CON_PCUSTOMIZE)
        send_to_char("Enter persona description: ", CH);
      else if (STATE(d) == CON_ACUSTOMIZE)
        send_to_char("Enter reflection description: ", CH);
      else
        send_to_char("Enter voice description: ", CH);

      if (STATE(d) == CON_FCUSTOMIZE)
        d->edit_mode = CEDIT_VOICE;
      else {
        d->edit_mode = CEDIT_DESC;
        DELETE_D_STR_IF_EXTANT(d);
        INITIALIZE_NEW_D_STR(d);
        d->max_str = EXDSCR_LENGTH;
        d->mail_to = 0;
      }
      break;

    case '4':
      send_to_char("Enter long (active) description (@ on a blank line to end):\r\n", CH);
      d->edit_mode = CEDIT_LONG_DESC;
      DELETE_D_STR_IF_EXTANT(d);
      INITIALIZE_NEW_D_STR(d);
      d->max_str = EXDSCR_LENGTH;
      d->mail_to = 0;

      break;
    case '5':
      send_to_char("Enter arrival message: ", CH);
      d->edit_mode = CEDIT_ARRIVE;
      break;
    case '6':
      send_to_char("Enter leaving message: ", CH);
      d->edit_mode = CEDIT_LEAVE;
      break;
    case '7':
      if (!PLR_FLAGGED(CH, PLR_NEWBIE))
        cedit_disp_menu(d, 0);
      else {
        CLS(CH);
        send_to_char(CH, "1) Tiny\r\n2) Small\r\n3) Average\r\n4) Large\r\n5) Huge\r\nEnter desired height range: ");
        d->edit_mode = CEDIT_HEIGHT;
      }
      break;
    case '8':
      if (!PLR_FLAGGED(CH, PLR_NEWBIE))
        cedit_disp_menu(d, 0);
      else {
        CLS(CH);
        send_to_char(CH, "1) Tiny\r\n2) Small\r\n3) Average\r\n4) Large\r\n5) Huge\r\nEnter desired weight range: ");
        d->edit_mode = CEDIT_WEIGHT;
      }
      break;
    default:
      cedit_disp_menu(d, 0);
      break;
    }

    break;
  case CEDIT_HEIGHT:
    GET_HEIGHT(CH) = (int)gen_size(GET_RACE(CH), 1, atoi(arg), GET_SEX(CH));
    cedit_disp_menu(d, 0);
    break;
  case CEDIT_WEIGHT:
    GET_WEIGHT(CH) = (int)gen_size(GET_RACE(CH), 0, atoi(arg), GET_SEX(CH));
    cedit_disp_menu(d, 0);
    break;
  case CEDIT_ALIAS:
    if (strlen(arg) >= LINE_LENGTH) {
      cedit_disp_menu(d, 1);
      return;
    }

    DELETE_ARRAY_IF_EXTANT(d->edit_mob->player.physical_text.keywords);
    d->edit_mob->player.physical_text.keywords = str_dup(arg);
    cedit_disp_menu(d, 0);

    break;

  case CEDIT_VOICE:
    if (strlen(arg) >= LINE_LENGTH) {
      cedit_disp_menu(d, 1);
      return;
    }

    DELETE_ARRAY_IF_EXTANT(d->edit_mob->player.physical_text.room_desc);
    d->edit_mob->player.physical_text.room_desc = str_dup(arg);
    cedit_disp_menu(d, 0);

    break;
  case CEDIT_ARRIVE:
    if (strlen(arg) >= LINE_LENGTH) {
      cedit_disp_menu(d, 1);
      return;
    }
    DELETE_ARRAY_IF_EXTANT(d->edit_mob->char_specials.arrive);
    d->edit_mob->char_specials.arrive = str_dup(arg);
    cedit_disp_menu(d, 0);
    break;
  case CEDIT_LEAVE:
    if (strlen(arg) >= LINE_LENGTH) {
      cedit_disp_menu(d, 1);
      return;
    }
    DELETE_ARRAY_IF_EXTANT(d->edit_mob->char_specials.leave);
    d->edit_mob->char_specials.leave = str_dup(arg);
    cedit_disp_menu(d, 0);
    break;
  case CEDIT_SHORT_DESC:
    if (strlen(arg) >= LINE_LENGTH) {
      cedit_disp_menu(d, 1);
      return;
    }
    DELETE_ARRAY_IF_EXTANT(d->edit_mob->player.physical_text.name);
    d->edit_mob->player.physical_text.name = str_dup(arg);
    cedit_disp_menu(d, 0);
    break;
  default:
    break;
  }
}

ACMD(do_remember)
{
  struct char_data *vict = NULL;
  struct remem *m, *temp;

  argument = any_one_arg(argument, buf1);
  argument = one_argument(argument, buf2);

  if (!*buf1 || !*buf2) {
    send_to_char(ch, "Remember Who as What?\r\n");
    return;
  }
  
  if (ch->in_veh)
    vict = get_char_veh(ch, buf1, ch->in_veh);
  else
    vict = get_char_room_vis(ch, buf1);
  
  if (!vict)
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf1);
  else if (IS_NPC(vict))
    send_to_char(ch, "You cannot remember NPCs.\r\n");
  else if (ch == vict)
    send_to_char(ch, "You should have no problem remembering who you are.\r\n");
  else if (IS_SENATOR(vict))
    send_to_char(ch, "Try as you might, you can't seem to remember them.\r\n");
  else {
    for (temp = GET_MEMORY(ch); temp; temp = temp->next)
      if (GET_IDNUM(vict) == temp->idnum) {
        DELETE_AND_NULL_ARRAY(temp->mem);
        temp->mem = str_dup(buf2);
        send_to_char(ch, "Remembered %s as %s\r\n", GET_NAME(vict), buf2);
        return;
      }

    m = new remem;
    m->mem = str_dup(buf2);
    m->idnum = GET_IDNUM(vict);
    m->next = GET_MEMORY(ch);
    GET_MEMORY(ch) = m;
    send_to_char(ch, "Remembered %s as %s\r\n", GET_NAME(vict), buf2);
  }
}

struct remem *found_mem(struct remem *mem, struct char_data *ch)
{
  if (IS_NPC(ch))
    return NULL;
  for (;mem; mem = mem->next)
    if (GET_IDNUM(ch) == mem->idnum)
      return mem;
  return NULL;
}

int recog(struct char_data *ch, struct char_data *i, char *name)
{
  struct remem *mem;

  if (!(mem = found_mem(GET_MEMORY(ch), i)))
    return 0;

  if (!str_cmp(name, mem->mem))
    return 1;

  return 0;

}

ACMD(do_photo)
{
  struct obj_data *camera = NULL, *photo, *mem = NULL, *temp;
  char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH], buf3[MAX_STRING_LENGTH];
  
  // Select the camera from their wielded or holded items.
  if (GET_EQ(ch, WEAR_HOLD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) == ITEM_CAMERA)
    camera = GET_EQ(ch, WEAR_HOLD);
  else if (GET_EQ(ch, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_CAMERA)
    camera = GET_EQ(ch, WEAR_WIELD);
  
  // Check for a cyberware camera.
  if (!camera) {
    for (temp = ch->cyberware; temp; temp = temp->next_content)
      if (GET_OBJ_VAL(temp, 0) == CYB_EYES && IS_SET(GET_OBJ_VAL(temp, 3), EYE_CAMERA)) 
        camera = temp;
      else if (GET_OBJ_VAL(temp, 0) == CYB_MEMORY)  
        mem = temp;
    if (camera) {
      if (!mem) {
        send_to_char("You need headware memory to take a picture with an eye camera.\r\n", ch);
        return;
      }
      if (GET_OBJ_VAL(mem, 3) - GET_OBJ_VAL(mem, 5) < 1) {
        send_to_char("You don't have enough headware memory to take a photo now.\r\n", ch);
        return;
      }
    }
  } 
  if (!camera) {
    send_to_char("You need to be holding a camera to take photos.\r\n", ch);
    return;
  }
  skip_spaces(&argument);
  if (*argument) {
    bool found = FALSE;
    struct char_data *i;
    struct obj_data *found_obj;
    struct veh_data *found_veh;
    char *desc;
    extern char *find_exdesc(char *word, struct extra_descr_data * list);
    generic_find(argument, FIND_OBJ_ROOM | FIND_CHAR_ROOM, ch, &i, &found_obj);
    
    // Look for a targeted vehicle.
    if ((found_veh = get_veh_list(argument, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch))) {
      snprintf(buf2, sizeof(buf2), "a photo of %s^n", decapitalize_a_an(GET_VEH_NAME(found_veh)));
      snprintf(buf, sizeof(buf), "^c%s^c in %s%s^n\r\n%s",
              GET_VEH_NAME(found_veh),
              found_veh->in_veh ? "the rear of " : "",
              found_veh->in_veh ? GET_VEH_NAME(found_veh->in_veh) : GET_ROOM_NAME(found_veh->in_room),
              GET_VEH_DESC(found_veh));
      found = TRUE;
    }
    
    if (!found) {
      // Look for a targeted person.
      if (i) {
        // This does not cause info disclosure because generic_find respects CAN_SEE().
        if (AFF_FLAGGED(i, AFF_IMP_INVIS) || AFF_FLAGGED(i, AFF_SPELLIMPINVIS)) {
          send_to_char(ch, "You don't seem to see them through the viewfinder.\r\n");
          return;
        }
        snprintf(buf2, sizeof(buf2), "a photo of %s^n", make_desc(ch, i, buf, 2, TRUE));
        if (i->in_veh) {
          snprintf(buf, sizeof(buf), "^c%s^c sitting in the %s of %s^n\r\n%s",
                  make_desc(ch, i, buf3, 2, FALSE),
                  i->vfront ? "front" : "back",
                  decapitalize_a_an(GET_VEH_NAME(i->in_veh)),
                  i->player.physical_text.look_desc);
        } else {
          snprintf(buf, sizeof(buf), "^c%s^c in %s^n\r\n%s", make_desc(ch, i, buf3, 2, FALSE),
                  GET_ROOM_NAME(ch->in_room), i->player.physical_text.look_desc);
        }
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s is using:\r\n", make_desc(ch, i, buf3, 2, FALSE));
        for (int j = 0; j < NUM_WEARS; j++)
          if (GET_EQ(i, j) && CAN_SEE_OBJ(ch, GET_EQ(i, j))) {
            // Describe special-case wielded/held objects.
            if (j == WEAR_WIELD || j == WEAR_HOLD) {
              if (IS_OBJ_STAT(GET_EQ(i, j), ITEM_TWOHANDS))
                strcat(buf, hands[2]);
              else if (j == WEAR_WIELD)
                strcat(buf, hands[(int)i->char_specials.saved.left_handed]);
              else
                strcat(buf, hands[!i->char_specials.saved.left_handed]);
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\t%s\r\n", GET_OBJ_NAME(GET_EQ(i, j)));
              continue;
            }
            
            // Concealed by enveloping clothing? Succeed on a test or you don't see it.
            if ((j == WEAR_BODY || j == WEAR_LARM || j == WEAR_RARM || j == WEAR_WAIST) && GET_EQ(i, WEAR_ABOUT)) {
              if (success_test(GET_INT(ch), 4 + GET_OBJ_VAL(GET_EQ(i, WEAR_ABOUT), 7)) < 2)
                continue;
            }
            
            // Underclothing? Succeed on a test or you don't see it.
            if (j == WEAR_UNDER && (GET_EQ(i, WEAR_ABOUT) || GET_EQ(i, WEAR_BODY))) {
              if (success_test(GET_INT(ch), 6 +
                               (GET_EQ(i, WEAR_ABOUT) ? GET_OBJ_VAL(GET_EQ(i, WEAR_ABOUT), 7) : 0) +
                               (GET_EQ(i, WEAR_BODY) ? GET_OBJ_VAL(GET_EQ(i, WEAR_BODY), 7) : 0)) < 2)
                continue;
            }
            
            // Pants under enveloping clothing? Succeed on an easier test.
            if (j == WEAR_LEGS && GET_EQ(i, WEAR_ABOUT)) {
              if (success_test(GET_INT(ch), 2 + GET_OBJ_VAL(GET_EQ(i, WEAR_ABOUT), 7)) < 2)
                continue;
            }
            
            // Anklets under pants or enveloping clothing? Succeed on a harder test.
            if ((j == WEAR_RANKLE || j == WEAR_LANKLE) && (GET_EQ(i, WEAR_ABOUT) || GET_EQ(i, WEAR_LEGS))) {
              if (success_test(GET_INT(ch), 5 +
                               (GET_EQ(i, WEAR_ABOUT) ? GET_OBJ_VAL(GET_EQ(i, WEAR_ABOUT), 7) : 0) +
                               (GET_EQ(i, WEAR_LEGS) ? GET_OBJ_VAL(GET_EQ(i, WEAR_LEGS), 7) : 0)) < 2)
                continue;
            }
            
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%s\r\n", where[j], GET_OBJ_NAME(GET_EQ(i, j)));
          }
        found = TRUE;
      } else if (found_obj && GET_OBJ_VNUM(found_obj) != OBJ_BLANK_PHOTO) {
        snprintf(buf2, sizeof(buf2), "a photo of %s", decapitalize_a_an(GET_OBJ_NAME(found_obj)));
        if (ch->in_veh) {
          snprintf(buf, sizeof(buf), "^c%s^c in %s^n\r\n%s",
                  GET_OBJ_NAME(found_obj),
                  decapitalize_a_an(GET_VEH_NAME(i->in_veh)),
                  found_obj->photo ? found_obj->photo : found_obj->text.look_desc);
        } else {
          snprintf(buf, sizeof(buf), "^c%s^c in %s^n\r\n%s", GET_OBJ_NAME(found_obj), GET_ROOM_NAME(ch->in_room), found_obj->photo ? found_obj->photo : found_obj->text.look_desc);
        }
        found = TRUE;
      } else if (ch->in_veh) {
        if ((i = get_char_veh(ch, arg, ch->in_veh))) {
          snprintf(buf2, sizeof(buf2), "a photo of %s", decapitalize_a_an(GET_NAME(i)));
          snprintf(buf, sizeof(buf), "^c%s in %s^n\r\n%s", GET_NAME(i), GET_VEH_NAME(ch->in_veh), i->player.physical_text.look_desc);
          found = TRUE;
        }
      } else if (ch->in_room && (desc = find_exdesc(arg, ch->in_room->ex_description))) {
        snprintf(buf2, sizeof(buf2), "a photo of %s", decapitalize_a_an(argument));
        snprintf(buf, sizeof(buf), "^c%s in %s^n\r\n%s", argument, GET_ROOM_NAME(ch->in_room), desc);
        found = TRUE;
      }
    }
    if (!found) {
      send_to_char(ch, "You don't see anything named '%s' here.\r\n", argument);
      return;
    }
  } else {
    if (ch->in_veh)
      ch->in_room = get_ch_in_room(ch);
    snprintf(buf2, sizeof(buf2), "a photo of %s", GET_ROOM_NAME(ch->in_room));
    snprintf(buf, sizeof(buf), "^c%s^n\r\n%s", GET_ROOM_NAME(ch->in_room), GET_ROOM_DESC(ch->in_room));
    for (struct char_data *tch = ch->in_room->people; tch; tch = tch->next_in_room)
      if (tch != ch && !(AFF_FLAGGED(tch, AFF_IMP_INVIS) || AFF_FLAGGED(tch, AFF_SPELLIMPINVIS)) && GET_INVIS_LEV(tch) < 2) {
        if (IS_NPC(tch) && tch->player.physical_text.room_desc &&
            GET_POS(tch) == GET_DEFAULT_POS(tch)) {
          strcat(buf, tch->player.physical_text.room_desc);
          continue;
        }
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s", make_desc(ch, tch, buf3, 2, FALSE));
        if (CH_IN_COMBAT(tch)) {
          strcat(buf, " is here, fighting ");
          if (FIGHTING(tch) == ch)
            strcat(buf, "the photographer");
          else if (FIGHTING_VEH(tch)) {
            strcat(buf, GET_VEH_NAME(FIGHTING_VEH(tch)));
          } else {
            if (tch->in_room == FIGHTING(tch)->in_room)
              if (AFF_FLAGGED(FIGHTING(tch), AFF_IMP_INVIS)) {
                strcat(buf, "someone");
              } else
                strcat(buf, GET_NAME(FIGHTING(tch)));
            else
              strcat(buf, "someone in the distance");
          }
          strcat(buf, "!\r\n");
        } else {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s", positions[(int)GET_POS(tch)]);
          if (GET_DEFPOS(tch))
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", %s", GET_DEFPOS(tch));
          strcat(buf, ".\r\n");
        }
      }
    struct obj_data *obj;
    FOR_ITEMS_AROUND_CH(ch, obj) {
      int num = 0;
      while (obj->next_content) {
        if (obj->item_number != obj->next_content->item_number || obj->restring)
          break;
        num++;
        obj = obj->next_content;
      }
      if (num > 1)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "(%d) ", num);
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^g%s^n\r\n", obj->text.room_desc);
    }

    for (struct veh_data *vehicle = ch->in_room->vehicles; vehicle; vehicle = vehicle->next_veh) {
      if (ch->in_veh != vehicle) {
        strcat(buf, "^y");
        if (vehicle->damage >= VEH_DAM_THRESHOLD_DESTROYED) {
          strcat(buf, GET_VEH_NAME(vehicle));
          strcat(buf, " lies here wrecked.\r\n");
        } else {
          if (vehicle->type == VEH_BIKE && vehicle->people)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s sitting on ", GET_NAME(vehicle->people));
          switch (vehicle->cspeed) {
          case SPEED_OFF:
            if (vehicle->type == VEH_BIKE && vehicle->people) {
              strcat(buf, GET_VEH_NAME(vehicle));
              strcat(buf, " waits here.\r\n");
            } else
              strcat(buf, vehicle->description);
            break;
          case SPEED_IDLE:
            strcat(buf, GET_VEH_NAME(vehicle));
            strcat(buf, " idles here.\r\n");
            break;
          case SPEED_CRUISING:
            strcat(buf, GET_VEH_NAME(vehicle));
            strcat(buf, " cruises through here.\r\n");
            break;
          case SPEED_SPEEDING:
            strcat(buf, GET_VEH_NAME(vehicle));
            strcat(buf, " speeds past.\r\n");
            break;
          case SPEED_MAX:
            strcat(buf, GET_VEH_NAME(vehicle));
            strcat(buf, " zooms by.\r\n");
            break;
          }
        }
      }
    }
    if (ch->in_veh)
      ch->in_room = NULL;
  }
  photo = read_object(OBJ_BLANK_PHOTO, VIRTUAL);
  if (!mem)
    act("$n takes a photo with $p.", TRUE, ch, camera, NULL, TO_ROOM);
  send_to_char(ch, "You take a photo.\r\n");
  photo->photo = str_dup(buf);
  if (strlen(buf2) >= LINE_LENGTH)
    buf2[300] = '\0';
  photo->restring = str_dup(buf2);
  if (mem) {
    obj_to_obj(photo, mem);
    GET_OBJ_VAL(mem, 5) += 1;
  } else obj_to_char(photo, ch);
}

ACMD(do_boost)
{
  int suc;
  extern void nonsensical_reply(struct char_data *ch, const char *arg);
  if (GET_TRADITION(ch) != TRAD_ADEPT) {
    nonsensical_reply(ch, NULL);
    return;
  }
  skip_spaces(&argument);
  if (is_abbrev(argument, "strength")) {
    if (!GET_POWER(ch, ADEPT_BOOST_STR)) {
      send_to_char("You don't have that power activated.\r\n", ch);
      return;
    }
    if (BOOST(ch)[STR][0]) {
      send_to_char(ch, "You are already boosting that attribute.\r\n");
      return;
    }
    suc = success_test(GET_MAG(ch) / 100, GET_REAL_STR(ch) / 2);
    if (suc < 1) {
      send_to_char(ch, "You can't get your power to bond with your lifeforce.\r\n");
      return;
    }
    BOOST(ch)[STR][0] = suc + 1;
    BOOST(ch)[STR][1] = GET_POWER(ch, ADEPT_BOOST_STR);
    send_to_char(ch, "You feel stronger.\r\n");
    affect_total(ch);
  } else if (is_abbrev(argument, "quickness")) {
    if (!GET_POWER(ch, ADEPT_BOOST_QUI)) {
      send_to_char("You don't have that power activated.\r\n", ch);
      return;
    }
    if (BOOST(ch)[QUI][0]) {
      send_to_char(ch, "You are already boosting that attribute.\r\n");
      return;
    }
    suc = success_test(GET_MAG(ch) / 100, GET_REAL_QUI(ch) / 2);
    if (suc < 1) {
      send_to_char(ch, "You can't get your power to bond with your lifeforce.\r\n");
      return;
    }
    BOOST(ch)[QUI][0] = suc + 1;
    BOOST(ch)[QUI][1] = GET_POWER(ch, ADEPT_BOOST_QUI);
    send_to_char(ch, "You feel quicker.\r\n");
    affect_total(ch);
  } else if (is_abbrev(argument, "body")) {
    if (!GET_POWER(ch, ADEPT_BOOST_BOD)) {
      send_to_char("You don't have that power activated.\r\n", ch);
      return;
    }
    if (BOOST(ch)[BOD][0]) {
      send_to_char(ch, "You are already boosting that attribute.\r\n");
      return;
    }
    suc = success_test(GET_MAG(ch) / 100, GET_REAL_BOD(ch) / 2);
    if (suc < 1) {
      send_to_char(ch, "You can't get your power to bond with your lifeforce.\r\n");
      return;
    }
    BOOST(ch)[BOD][0] = suc + 1;
    BOOST(ch)[BOD][1] = GET_POWER(ch, ADEPT_BOOST_BOD);
    send_to_char(ch, "You feel hardier.\r\n");
    affect_total(ch);
  } else {
    send_to_char(ch, "Boost which attribute?\r\n");
    return;
  }
}

void process_boost()
{
  PERF_PROF_SCOPE(pr_, __func__);
  int power, damage;
  struct char_data *next;
  for (struct char_data *i = character_list; i; i = next) {
    next = i->next;
    if (GET_TRADITION(i) == TRAD_ADEPT) {
      if (BOOST(i)[STR][0] > 0) {
        BOOST(i)[STR][0]--;
        if (!BOOST(i)[STR][0]) {
          send_to_char(i, "You feel weaker as your boost wears off.\r\n");
          power = GET_STR(i);
          if (GET_STR(i) <= racial_limits[(int)GET_RACE(i)][0][2])
            damage = LIGHT;
          else if (GET_STR(i) < racial_limits[(int)GET_RACE(i)][1][2])
            damage = MODERATE;
          else
            damage = SERIOUS;
          spell_drain(i, 0, power, damage);
        }
      }
      if (BOOST(i)[QUI][0] > 0) {
        BOOST(i)[QUI][0]--;
        if (!BOOST(i)[QUI][0]) {
          send_to_char(i, "You feel slower as your boost wears off.\r\n");
          power = GET_QUI(i);
          if (GET_QUI(i) <= racial_limits[(int)GET_RACE(i)][0][1])
            damage = LIGHT;
          else if (GET_QUI(i) < racial_limits[(int)GET_RACE(i)][1][1])
            damage = MODERATE;
          else
            damage = SERIOUS;
          spell_drain(i, 0, power, damage);
        }
      }
      if (BOOST(i)[BOD][0] > 0) {
        BOOST(i)[BOD][0]--;
        if (!BOOST(i)[BOD][0]) {
          send_to_char(i, "You feel less hardy as your boost wears off.\r\n");
          power = GET_BOD(i);
          if (GET_BOD(i) <= racial_limits[(int)GET_RACE(i)][0][0])
            damage = LIGHT;
          else if (GET_BOD(i) < racial_limits[(int)GET_RACE(i)][1][0])
            damage = MODERATE;
          else
            damage = SERIOUS;
          spell_drain(i, 0, power, damage);
        }
      }
      if (i)
        affect_total(i);
    }
  }
}

ACMD(do_assense)
{
  struct char_data *vict;
  struct obj_data *obj;
  struct remem *mem;
  if (!*argument) {
    send_to_char(ch, "Who's aura do you wish to assense?\r\n");
    return;
  }
  if (!PLR_FLAGS(ch).AreAnySet(PLR_PERCEIVE, PLR_PROJECT, ENDBIT) && !IS_PROJECT(ch)) {
    send_to_char(ch, "You have no sense of the astral plane.\r\n");
    return;
  }
  skip_spaces(&argument);
  if (!generic_find(argument,  FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP |
                    FIND_CHAR_ROOM, ch, &vict, &obj)) {
    send_to_char(ch, "You don't see them here.\r\n");
    return;
  }
  int skill = GET_INT(ch), target = 4;
  if (GET_BACKGROUND_AURA(ch->in_room) == AURA_POWERSITE)
    skill += GET_BACKGROUND_COUNT(ch->in_room);
  else target += GET_BACKGROUND_COUNT(ch->in_room);
  int success = success_test(skill, target);
  success += (int)(success_test(GET_SKILL(ch, SKILL_AURA_READING), 4) / 2);
  if (success < 1) {
    send_to_char(ch, "You fail to notice anything about that aura.\r\n");
    return;
  }
  if (vict) {
    obj = NULL;
    two_arguments(argument, arg, buf1);
    int i = 0;
    if (*buf1) {
      if (is_abbrev(buf1, "spell")) {
        bool found = FALSE;
        for (struct sustain_data *sus = GET_SUSTAINED(vict); sus; sus = sus->next)
          if (!sus->caster) {
            found = TRUE;
            success = success_test(GET_INT(ch), 4) + (int)(success_test(GET_SKILL(ch, SKILL_AURA_READING), 4) / 2);
            if (success > 0)
              snprintf(buf, sizeof(buf), "They are affected by a %s spell", spell_category[spells[sus->spell].category]);
            if (success > 5)
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " cast at force %d", sus->force);
            else if (success > 3) {
              if (sus->force > GET_MAG(ch) / 100)
                strcat(buf, " cast at a force higher than");
              if (sus->force == GET_MAG(ch) / 100)
                strcat(buf, " cast at a force equal to");
              else
                strcat(buf, " cast at a force lower than");
              strcat(buf, " your magic");
            }
            if (success >= 3) {
              if (GET_IDNUM(ch) == GET_IDNUM(sus->other))
                strcat(buf, ". It was cast by you");
              else {
                for (mem = GET_MEMORY(ch); mem; mem = mem->next)
                  if (mem->idnum == GET_IDNUM(sus->other))
                    break;
                if (mem)
                  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ". It seems to have been cast by %s", mem->mem);
                else
                  strcat(buf, ". The astral signature is unfamiliar to you");
              }
            }
            strcat(buf, ".\r\n");
            if (success > 0)
              send_to_char(buf, ch);
            else
              send_to_char("You sense nothing special about that spell.\r\n", ch);
          }
        if (!found)
          send_to_char("They are not affected by any spells.\r\n", ch);
        return;
      } else if (!(obj = get_object_in_equip_vis(vict, buf1, vict->equipment, &i))) {
        send_to_char("They don't seem to be wearing that.\r\n", ch);
        return;
      }
    }
    if (!obj) {
      bool init = FALSE, comp = FALSE;
      if (GET_METAMAGIC(vict, META_MASKING) > 1 && GET_MASKING(vict)) {
        int targ = MAX(1, GET_GRADE(vict) - GET_GRADE(ch));
        if (!GET_GRADE(ch) || success_test(GET_MAG(ch) / 100, GET_MAG(vict) / 100) < targ) {
          if (IS_SET(GET_MASKING(vict), MASK_COMPLETE))
            comp = TRUE;
          else if (IS_SET(GET_MASKING(vict), MASK_INIT))
            init = TRUE;
        }
      }
      int mag = GET_MAG(vict);
      if (comp)
        mag = 0;
      else if (init)
        mag -= GET_GRADE(vict) * 100;
      strcpy(buf, make_desc(ch, vict, buf2, 2, FALSE));
      if (success < 3) {
        if (vict->cyberware) {
          if (GET_SEX(vict) != SEX_NEUTRAL || (IS_NPC(vict) && MOB_FLAGGED(vict, MOB_INANIMATE)))
            strcat(buf, " has cyberware present and");
          else
            strcat(buf, " have cyberware present and");
        }
        if (IS_NPC(vict)) {
          if (IS_SPIRIT(vict))
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " is a %s spirit", spirit_name[GET_SPARE1(vict)]);
          else if (IS_ELEMENTAL(vict))
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " is a %s elemental", elements[GET_SPARE1(vict)].name);
          else if (GET_MAG(vict) > 0)
            strcat(buf, " is awakened");
          else
            strcat(buf, " is mundane");
        } else if (GET_TRADITION(vict) != TRAD_MUNDANE && !comp)
          strcat(buf, " is awakened");
        else
          strcat(buf, " is mundane");
        strcat(buf, ".\r\n");
      } else if (success < 5) {
        if (GET_ESS(vict) < GET_ESS(ch))
          strcat(buf, " has less astral presence");
        else if (GET_ESS(vict) == GET_ESS(ch))
          strcat(buf, " has the same amount of astral presence");
        else
          strcat(buf, " has more astral presence");
        strcat(buf, " and ");
        if (mag < GET_MAG(ch))
          strcat(buf, "less magic than you. ");
        else if (mag == GET_MAG(ch))
          strcat(buf, "the same amount of magic as you. ");
        else
          strcat(buf, "more magic than you. ");
        strcat(buf, CAP(HSSH(vict)));
        if (IS_NPC(vict)) {
          if (IS_SPIRIT(vict))
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " is a %s spirit", spirits[GET_SPARE1(vict)].name);
          else if (IS_ELEMENTAL(vict))
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " is a %s elemental", elements[GET_SPARE1(vict)].name);
          else if (GET_MAG(vict) > 0)
            strcat(buf, " is awakened");
          else
            strcat(buf, " is mundane");
        } else if (GET_TRADITION(vict) != TRAD_MUNDANE && !comp)
          strcat(buf, " is awakened");
        else
          strcat(buf, " is mundane");
        if (vict->cyberware) {
          int locs[] = { 0, 0, 0, 0}, numloc = 0;
          for (struct obj_data *cyber = vict->cyberware; cyber; cyber = cyber->next_content)
            switch (GET_OBJ_VAL(cyber, 0)) {
            case CYB_CHIPJACK:
            case CYB_DATAJACK:
            case CYB_DATALOCK:
            case CYB_KNOWSOFTLINK:
            case CYB_MEMORY:
            case CYB_TOOTHCOMPARTMENT:
            case CYB_PHONE:
            case CYB_RADIO:
            case CYB_EYES:
            case CYB_VOICEMOD:
            case CYB_BOOSTEDREFLEXES:
            case CYB_SIMRIG:
            case CYB_SKILLWIRE:
            case CYB_VCR:
            case CYB_REFLEXTRIGGER:
            case CYB_BALANCEAUG:
            case CYB_ORALDART:
            case CYB_ORALGUN:
            case CYB_ORALSLASHER:
            case CYB_ASIST:
            case CYB_CHIPJACKEXPERT:
            case CYB_CRANIALCYBER:
            case CYB_DATACOMPACT:
            case CYB_ENCEPHALON:
            case CYB_MATHSSPU:
            case CYB_SKULL:
            case CYB_SMARTLINK:
              locs[0] = 1;
              break;
            case CYB_DERMALPLATING:
            case CYB_FILTRATION:
            case CYB_BONELACING:
            case CYB_WIREDREFLEXES:
            case CYB_REACTIONENHANCE:
            case CYB_AUTOINJECT:
            case CYB_BALANCETAIL:
            case CYB_BODYCOMPART:
            case CYB_TORSO:
            case CYB_DERMALSHEATHING:
            case CYB_MOVEBYWIRE:
            case CYB_MUSCLEREP:
              locs[1] = 1;
              break;
            case CYB_FINGERTIP:
            case CYB_HANDBLADE:
            case CYB_HANDRAZOR:
            case CYB_HANDSPUR:
            case CYB_FIN:
            case CYB_CLIMBINGCLAWS:
              locs[2] = 1;
              break;
            case CYB_FOOTANCHOR:
            case CYB_HYDRAULICJACK:
              locs[3] = 1;
              break;
            }
          if (locs[0])
            numloc++;
          if (locs[1])
            numloc++;
          if (locs[2])
            numloc++;
          if (locs[3])
            numloc++;
          strcat(buf, " and has cyberware in ");
          strcat(buf, HSHR(vict));
          if (locs[0]) {
            numloc--;
            strcat(buf, " head");
            if (numloc == 1)
              strcat(buf, " and");
            else if (numloc > 1) 
              strcat(buf, ",");
          }
          if (locs[1]) {
            strcat(buf, " arms");
            numloc--;
            if (numloc == 1)
              strcat(buf, " and");
            else if (numloc > 1) 
              strcat(buf, ",");
          }
          if (locs[2]) {
            strcat(buf, " torso");
            if (locs[3])
              strcat(buf, " and");
          }
          if (locs[3])
            strcat(buf, " feet");
        }
        strcat(buf, ".\r\n");
      } else {
        snprintf(buf2, sizeof(buf2), " has %.2f essence", ((float)GET_ESS(vict) / 100));
        strcat(buf, buf2);
        if (IS_NPC(vict)) {
          if (IS_SPIRIT(vict))
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " is a %s spirit of force %d", spirits[GET_SPARE1(vict)].name, GET_LEVEL(vict));
          else if (IS_ELEMENTAL(vict))
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " is a %s elemental of force %d", elements[GET_SPARE1(vict)].name, GET_LEVEL(vict));
          else if (GET_MAG(vict) > 0)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " and has %d magic", (int)(GET_MAG(vict) / 100));
          else
            strcat(buf, " and is mundane");
        } else if (GET_TRADITION(vict) != TRAD_MUNDANE && !comp)
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " and %d magic", (int)(mag / 100));
        else
          strcat(buf, " and is mundane");
        if (vict->cyberware) {
          strcat(buf, ". ");
          strcat(buf, CAP(HSSH(vict)));
          strcat(buf, " has cyberware in ");
          strcat(buf, HSHR(vict));
          int numleft = 0;
          int locs[] = { 0, 0, 0, 0, 0, 0, 0 };
          for (struct obj_data *cyber = vict->cyberware; cyber; cyber = cyber->next_content)
            if (GET_OBJ_VAL(cyber, 2) <= 18 || (GET_OBJ_VAL(cyber, 2) >= 32 && GET_OBJ_VAL(cyber, 2) <= 34) || GET_OBJ_VAL(cyber, 2) == 28)
              locs[0] = 1;
            else if (GET_OBJ_VAL(cyber, 2) == 32 || (GET_OBJ_VAL(cyber, 2) >= 8 && GET_OBJ_VAL(cyber, 2) <= 10))
              locs[1] = 1;
            else if (GET_OBJ_VAL(cyber, 2) >= 19 && GET_OBJ_VAL(cyber, 2) <= 22)
              locs[2] = 1;
            else if (GET_OBJ_VAL(cyber, 2) == 20 || (GET_OBJ_VAL(cyber, 2) >= 24 && GET_OBJ_VAL(cyber, 2) <= 27))
              locs[3] = 1;
            else if (GET_OBJ_VAL(cyber, 2) == 23)
              locs[4] = 1;
            else if (GET_OBJ_VAL(cyber, 2) == 29)
              locs[5] = 1;
            else if (GET_OBJ_VAL(cyber, 2) == 30)
              locs[6] = 1;
          for (int i = 0; i < 7; i++)
            if (locs[i])
              numleft++;
          if (locs[0]) {
            strcat(buf, " head");
            numleft--;
            if (numleft == 1)
              strcat(buf, " and");
            else if (numleft > 1) 
              strcat(buf, ",");
          }
          if (locs[1]) {
            strcat(buf, " eyes");
            numleft--;
            if (numleft == 1)
              strcat(buf, " and");
            else if (numleft > 1) 
              strcat(buf, ",");
          }
          if (locs[2]) {
            strcat(buf, " hands");
            numleft--;
            if (numleft == 1)
              strcat(buf, " and");
            else if (numleft > 1) 
              strcat(buf, ",");
          }
          if (locs[3]) {
            strcat(buf, " body");
            numleft--;
            if (numleft == 1)
              strcat(buf, " and");
            else if (numleft > 1) 
              strcat(buf, ",");
          }
          if (locs[4]) {
            strcat(buf, " skin");
            numleft--;
            if (numleft == 1)
              strcat(buf, " and");
            else if (numleft > 1) 
              strcat(buf, ",");
          }
          if (locs[5]) {
            strcat(buf, " bones");
            if (locs[6])
              strcat(buf, " and");
          }
          if (locs[6])
            strcat(buf, " nervous system");
        }
        strcat(buf, ".\r\n");
      }
    }
  }
  if (obj) {
    snprintf(buf, sizeof(buf), "%s is ", CAP(GET_OBJ_NAME(obj)));
    if (GET_OBJ_TYPE(obj) == ITEM_FOCUS) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "a %s focus", foci_type[GET_OBJ_VAL(obj, 0)]);
      if (success >= 5) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ". It is of force %d", GET_OBJ_VAL(obj, 1));
        if (GET_OBJ_VAL(obj, 2)) {
          switch (GET_OBJ_VAL(obj, 0)) {
          case FOCI_EXPENDABLE:
          case FOCI_SPELL_CAT:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ". It has been bonded to help with %s spells", spell_category[GET_OBJ_VAL(obj, 3)]);
            break;
          case FOCI_SPEC_SPELL:
          case FOCI_SUSTAINED:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ". It has been bonded to help a %s spell", spell_category[spells[GET_OBJ_VAL(obj, 3)].category]);
            break;
          case FOCI_SPIRIT:
            if (GET_OBJ_VAL(obj, 5))
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ". It as been bonded with a %s elemental", elements[GET_OBJ_VAL(obj, 3)].name);
            else
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ". It as been bonded with a %s spirit", spirits[GET_OBJ_VAL(obj, 3)].name);
            break;
          }
        }
      } else if (success >= 3) {
        if (GET_OBJ_VAL(obj, 1) < GET_MAG(ch) / 100)
          strcat(buf, ". It has less astral presence than you");
        else if (GET_OBJ_VAL(obj, 1) == GET_MAG(ch) / 100)
          strcat(buf, ". It has the same amount of astral presence as you");
        else
          strcat(buf, ". It has more astral presence than you");
      }
      if (success >= 3) {
        if (GET_IDNUM(ch) == GET_OBJ_VAL(obj, 2))
          strcat(buf, ", it is bonded to you");
        else {
          for (mem = GET_MEMORY(ch); mem; mem = mem->next)
            if (mem->idnum == GET_OBJ_VAL(obj, 2))
              break;
          if (mem)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", it seems to have been bonded by %s", mem->mem);
          else
            strcat(buf, ", though the astral signature is unfamiliar to you");
        }
      }

    } else if (GET_OBJ_TYPE(obj) == ITEM_MAGIC_TOOL && GET_OBJ_VAL(obj, 0) == TYPE_CIRCLE) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "dedicated to %s", elements[GET_OBJ_VAL(obj, 2)].name);
      if (success >= 5)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ". It is of force %d", GET_OBJ_VAL(obj, 1));
      else if (success >= 3) {
        if (GET_OBJ_VAL(obj, 1) < GET_MAG(ch) / 100)
          strcat(buf, ". It has less astral presence than you");
        else if (GET_OBJ_VAL(obj, 1) == GET_MAG(ch) / 100)
          strcat(buf, ". It has the same amount of astral presence as you");
        else
          strcat(buf, ". It has more astral presence than you");
      }
      if (success >= 3) {
        for (mem = GET_MEMORY(ch); mem; mem = mem->next)
          if (mem->idnum == GET_OBJ_VAL(obj, 3))
            break;
        if (mem)
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", and is seems to have been drawn by %s", mem->mem);
      }
    } else if (GET_OBJ_TYPE(obj) == ITEM_MAGIC_TOOL && GET_OBJ_VAL(obj, 0) == TYPE_LODGE) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "dedicated to %s", totem_types[GET_OBJ_VAL(obj, 2)]);
      if (success >= 5)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ". It is of force %d", GET_OBJ_VAL(obj, 1));
      else if (success >= 3) {
        if (GET_OBJ_VAL(obj, 1) < GET_MAG(ch) / 100)
          strcat(buf, ". It has less astral presence than you");
        else if (GET_OBJ_VAL(obj, 1) == GET_MAG(ch) / 100)
          strcat(buf, ". It has the same amount of astral presence as you");
        else
          strcat(buf, ". It has more astral presence than you");
      }
      if (success >= 3) {
        for (mem = GET_MEMORY(ch); mem; mem = mem->next)
          if (mem->idnum == GET_OBJ_VAL(obj, 3))
            break;
        if (mem)
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", and is seems to have been built by %s", mem->mem);
      }
    } else
      strcat(buf, "a mundane object");
    strcat(buf, ".\r\n");
  }
  send_to_char(buf, ch);
}

ACMD(do_unpack)
{
  struct obj_data *shop = NULL;
  if (ch->in_veh) {
    if (!ch->in_veh->flags.IsSet(VFLAG_WORKSHOP)) {
      send_to_char("This vehicle isn't large enough to set up a workshop in.\r\n", ch);
      return;
    } else if (ch->vfront) {
      send_to_char("You can't set up a vehicle in the front seat.\r\n", ch);
    }
  }
  FOR_ITEMS_AROUND_CH(ch, shop) {
    if (GET_OBJ_TYPE(shop) == ITEM_WORKSHOP && GET_WORKSHOP_GRADE(shop) == TYPE_WORKSHOP) {
      if (GET_WORKSHOP_IS_SETUP(shop) || GET_WORKSHOP_UNPACK_TICKS(shop)) {
        send_to_char("There is already a workshop set up here.\r\n", ch);
        return;
      } else
        break;
    }
  }
  if (!shop)
    send_to_char(ch, "There is no workshop here to set up.\r\n");
  else {
    if (!ch->in_veh && GET_OBJ_VAL(shop, 0) == TYPE_VEHICLE && !ROOM_FLAGGED(ch->in_room, ROOM_GARAGE)) {
      send_to_char("You can't unpack that here.\r\n", ch);
      return;
    }
    send_to_char(ch, "You begin to set up %s here.\r\n", GET_OBJ_NAME(shop));
    act("$n begins to set up $P.", FALSE, ch, 0, shop, TO_ROOM);
    if (access_level(ch, LVL_BUILDER)) {
      send_to_char("You use your staff powers to greatly accelerate the process.\r\n", ch);
      GET_WORKSHOP_UNPACK_TICKS(shop) = 1;
    } else
      GET_WORKSHOP_UNPACK_TICKS(shop) = 3;
    AFF_FLAGS(ch).SetBit(AFF_PACKING);
  }
}

ACMD(do_packup)
{
  struct obj_data *shop = NULL;
  FOR_ITEMS_AROUND_CH(ch, shop) {
    if (GET_OBJ_TYPE(shop) == ITEM_WORKSHOP && GET_OBJ_VAL(shop, 1) > 1) {
      if (GET_OBJ_VAL(shop, 3)) {
        send_to_char(ch, "Someone is already working on the workshop.\r\n");
        return;
      } else if (GET_OBJ_VAL(shop, 2))
        break;
    }
  }
  if (!shop)
    send_to_char(ch, "There is no workshop here to pack up.\r\n");
  else {
    send_to_char(ch, "You begin to pack up %s here.\r\n", GET_OBJ_NAME(shop));
    act("$n begins to pack up $P.", FALSE, ch, 0, shop, TO_ROOM);
    if (access_level(ch, LVL_BUILDER)) {
      send_to_char("You use your staff powers to greatly accelerate the process.\r\n", ch);
      GET_OBJ_VAL(shop, 3) = 1;
    } else
      GET_OBJ_VAL(shop, 3) = 3;
    AFF_FLAGS(ch).SetBit(AFF_PACKING);
  }
}

ACMD(do_jack)
{
  struct obj_data *chip = NULL, *jack;
  int chipnum = 0;
  for (jack = ch->cyberware; jack; jack = jack->next_content)
    if (GET_OBJ_VAL(jack, 0) == CYB_CHIPJACK)
      break;
  if (!jack) {
    send_to_char(ch, "You don't have a chipjack.\r\n");
    return;
  }
  skip_spaces(&argument);
  for (chip = jack->contains; chip; chip = chip->next_content)
    chipnum++;
  if (subcmd) {
    if (chipnum) {
      int x = atoi(argument);
      if (x > chipnum) {
        send_to_char("You don't have that many chips in there.\r\n", ch);
        return;
      }
      for (chip = jack->contains; chip && --x >= 1; chip = chip->next_content)
        ;
      obj_from_obj(chip);
      obj_to_char(chip, ch);
      send_to_char(ch, "You remove %s from your chipjack.\r\n", GET_OBJ_NAME(chip));
      ch->char_specials.saved.skills[GET_OBJ_VAL(chip, 0)][1] = 0;
      act("$n removes a chip from their chipjack.", TRUE, ch, 0, 0, TO_ROOM);
    } else
      send_to_char(ch, "But you don't have anything installed in it.\r\n");
    return;
  }
  if (!*argument) {
    if (chipnum) {
      int i = 1;
      send_to_char(ch, "Currently installed chips:\r\n");
      for (chip = jack->contains; chip; chip = chip->next_content)
        send_to_char(ch, "%d) %20s Rating: %d\r\n", i++, GET_OBJ_NAME(chip), GET_OBJ_VAL(chip, 1));
    } else send_to_char(ch, "Your chipjack is currently empty.\r\n");
    return;
  }
  chip = get_obj_in_list_vis(ch, argument, ch->carrying);
  if (chipnum >= GET_OBJ_VAL(jack, 3))
    send_to_char(ch, "You don't have any space left in your chipjack.\r\n");
  else if (!chip)
    send_to_char(ch, "But you don't have that chip.\r\n");
  else if (GET_OBJ_TYPE(chip) != ITEM_CHIP)
    send_to_char(ch, "But that isn't a chip.\r\n");
  else {
    int max = 0;
    if (skills[GET_OBJ_VAL(chip, 0)].type)
      max = GET_OBJ_VAL(chip, 1);
    else
      for (struct obj_data *wires = ch->cyberware; wires; wires = wires->next_content)
        if (GET_OBJ_VAL(wires, 0) == CYB_SKILLWIRE) {
          max = GET_OBJ_VAL(wires, 1);
          break;
        }
    send_to_char(ch, "You slip %s into your chipjack.\r\n", GET_OBJ_NAME(chip));
    obj_from_char(chip);
    obj_to_obj(chip, jack);
    ch->char_specials.saved.skills[GET_OBJ_VAL(chip, 0)][1] = MIN(max, GET_OBJ_VAL(chip, 1));
    act("$n puts a chip into their chipjack.", TRUE, ch, 0, 0, TO_ROOM);
  }
}

ACMD(do_chipload)
{
  struct obj_data *memory = NULL, *jack = NULL, *chip = NULL;
  skip_spaces(&argument);
  if (!*argument) {
    send_to_char("Which chip do you wish to load into headware memory?\r\n", ch);
    return;
  }
  for (struct obj_data *obj = ch->cyberware; obj && !(memory && jack); obj = obj->next_content)
    if (GET_OBJ_VAL(obj, 0) == CYB_MEMORY)
      memory = obj;
    else if (GET_OBJ_VAL(obj, 0) == CYB_CHIPJACK)
      jack = obj;
  if (!memory || !jack) {
    send_to_char("You need a chipjack and headware memory to load skillsofts into.\r\n", ch);
    return;
  }
  if ((chip = get_obj_in_list_vis(ch, argument, jack->contains))) {
    if (GET_OBJ_VAL(chip, 2) > GET_OBJ_VAL(memory, 3) - GET_OBJ_VAL(memory, 5)) {
      send_to_char("You don't have enough headware memory left.\r\n", ch);
      return;
    }
    struct obj_data *newchip = read_object(GET_OBJ_RNUM(chip), REAL); 
    obj_to_obj(newchip, memory);
    GET_OBJ_VAL(memory, 5) += GET_OBJ_VAL(chip, 2);
    send_to_char(ch, "You upload %s to your headware memory.\r\n", GET_OBJ_NAME(chip));
  } else send_to_char("You don't have any chips in your chipjack.\r\n", ch);
}

ACMD(do_link)
{
  skip_spaces(&argument);
  if (!*argument) {
    send_to_char(ch, "Which chip do you wish to %slink?\r\n", subcmd ? "un" : "");
    return;
  }
  int wires = 0;
  struct obj_data *memory = NULL, *link = NULL;
  for (struct obj_data *obj = ch->cyberware; obj && !(memory && link && wires); obj = obj->next_content)
    if (GET_OBJ_VAL(obj, 0) == CYB_MEMORY)
      memory = obj;
    else if (GET_OBJ_VAL(obj, 0) == CYB_KNOWSOFTLINK)
      link = obj;
    else if (GET_OBJ_VAL(obj, 0) == CYB_SKILLWIRE)
      wires = GET_OBJ_VAL(obj, 1);
  if (!memory) {
    send_to_char("You need headware memory to link a skillsoft.\r\n", ch);
    return;
  }
  int x = atoi(argument);
  struct obj_data *obj = memory->contains;
  for (; obj; obj = obj->next_content)
    if (!--x)
      break;
  if (!obj)
    send_to_char("You don't have that many files in your memory.\r\n", ch);
  else if (GET_OBJ_TYPE(obj) != ITEM_CHIP)
    send_to_char("That is not a skillsoft.\r\n", ch);
  else if (skills[GET_OBJ_VAL(obj, 0)].type && !link)
    send_to_char("You need a knowsoft link to link knowsofts from headware memory.\r\n", ch);
  else if (!skills[GET_OBJ_VAL(obj, 0)].type && !wires)
    send_to_char("You need skill wires to link activesofts from headware memory.\r\n", ch);
  else if (GET_OBJ_VAL(obj, 8))
    send_to_char("You must decompress this skillsoft before you link it.\r\n", ch);
  else if ((GET_OBJ_VAL(obj, 9) && !subcmd) || (!GET_OBJ_VAL(obj, 9) && subcmd))
    send_to_char(ch, "That program is already %slinked.\r\n", subcmd ? "un" : "");
  else {
    if (subcmd) {
      GET_OBJ_VAL(obj, 9) = 0;
      send_to_char(ch, "You unlink %s.\r\n", GET_OBJ_NAME(obj));
      ch->char_specials.saved.skills[GET_OBJ_VAL(obj, 0)][1] = 0;
    } else {
      GET_OBJ_VAL(obj, 9) = 1;
      send_to_char(ch, "You link %s to your %s.\r\n", GET_OBJ_NAME(obj), skills[GET_OBJ_VAL(obj, 0)].type ? "knowsoft link" : "skillwires");

      if (!skills[GET_OBJ_VAL(obj, 0)].type)
        ch->char_specials.saved.skills[GET_OBJ_VAL(obj, 0)][1] = MIN(wires, GET_OBJ_VAL(obj, 1));
      else ch->char_specials.saved.skills[GET_OBJ_VAL(obj, 0)][1] = GET_OBJ_VAL(obj, 1);
    }
  }
}

ACMD(do_memory)
{
  struct obj_data *memory = ch->cyberware;
  for (; memory; memory = memory->next_content)
    if (GET_OBJ_VAL(memory, 0) == CYB_MEMORY)
      break;
  if (!memory) {
    send_to_char("You have no headware memory.\r\n", ch);
    return;
  }
  send_to_char(ch, "Headware Memory Contents (%d/%d):\r\n", GET_OBJ_VAL(memory, 3) - GET_OBJ_VAL(memory, 5), GET_OBJ_VAL(memory, 3));
  if (!memory->contains)
    send_to_char("  Empty\r\n", ch);
  else {
    int i = 0;
    for (struct obj_data *obj = memory->contains; obj; obj = obj->next_content) {
      i++;
      send_to_char(ch, "  %d) %s%-40s^n (%d MP) %s\r\n", i, GET_OBJ_VAL(obj, 8) ? "^r" : "", GET_OBJ_NAME(obj), 
                       GET_OBJ_VAL(obj, 2) - GET_OBJ_VAL(obj, 8), GET_OBJ_VAL(obj, 9) ? "^Y<LINKED>^N" : "");
    }
  }
}

ACMD(do_delete)
{
  struct obj_data *memory = ch->cyberware;
  for (; memory; memory = memory->next_content)
    if (GET_OBJ_VAL(memory, 0) == CYB_MEMORY)
      break;
  if (!memory) {
    send_to_char("You have no headware memory.\r\n", ch);
    return;
  }
  int x = atoi(argument);
  struct obj_data *obj = memory->contains;
  for (; obj; obj = obj->next_content)
    if (!--x)
      break;
  if (!obj)
    send_to_char("You don't have that many files in your memory.\r\n", ch);
  else {
    send_to_char(ch, "You delete %s from your headware memory.\r\n", GET_OBJ_NAME(obj));
    if (GET_OBJ_VAL(obj, 9))
      ch->char_specials.saved.skills[GET_OBJ_VAL(obj, 0)][1] = 0;
    obj_from_obj(obj);
    GET_OBJ_VAL(memory, 5) -= GET_OBJ_VAL(obj, 2) + GET_OBJ_VAL(obj, 8);
    extract_obj(obj);
  }
}

ACMD(do_imagelink)
{
  skip_spaces(&argument);
  if (!*argument) {
    send_to_char("Which file do you wish to view?\r\n", ch);
    return;
  }
  struct obj_data *memory = NULL, *link = NULL;
  for (struct obj_data *obj = ch->cyberware; obj && !(memory && link); obj = obj->next_content)
    if (GET_OBJ_VAL(obj, 0) == CYB_MEMORY)
      memory = obj;
    else if (GET_OBJ_VAL(obj, 0) == CYB_EYES && IS_SET(GET_OBJ_VAL(obj, 3), EYE_IMAGELINK))
      link = obj;
  if (!memory || !link) {
    send_to_char("You need headware memory and an image link to do this.\r\n", ch);
    return;
  }
  int x = atoi(argument);
  struct obj_data *obj = memory->contains;
  for (; obj; obj = obj->next_content)
    if (!--x)
      break;
  if (!obj)
    send_to_char("You don't have that many files in your memory.\r\n", ch);
  else send_to_char(obj->photo ? obj->photo : obj->text.look_desc, ch);
}

ACMD(do_reflex)
{
  struct obj_data *trigger = NULL, *wired = NULL;
  for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content)
    if (GET_OBJ_VAL(obj, 0) == CYB_REFLEXTRIGGER)
      trigger = obj;
    else if (GET_OBJ_VAL(obj, 0) == CYB_WIREDREFLEXES)
      wired = obj;
  if (!wired || !trigger) {
    send_to_char("You need wired reflexes and a reflex trigger to do that.\r\n", ch);
    return;
  }
  if (!GET_OBJ_VAL(trigger, 3)) {
    if (GET_OBJ_VAL(trigger, 1)) {
      send_to_char("You feel the world speed as you switch your wired reflexes off.\r\n", ch);
      GET_OBJ_VAL(trigger, 1) = 0;
    } else {
      send_to_char("You feel the world slow down around you as you switch your wired reflexes on.\r\n", ch);
      GET_OBJ_VAL(trigger, 1) = 3;
    }
  } else {
    skip_spaces(&argument);
    if (!*argument) {
      send_to_char("What level do you wish to set your wired reflexes to?\r\n", ch);
      return;
    }
    if (!str_cmp(argument, "off")) {
      if (!GET_OBJ_VAL(trigger, 1))
        send_to_char("Your wired reflexes are already switched off.\r\n", ch);
      else {
        GET_OBJ_VAL(trigger, 1) = 0;
        send_to_char("You feel the world speed as you switch your wired reflexes off.\r\n", ch);
      }
    } else {
      int i = atoi(argument);
      if (i < 1 || i > GET_OBJ_VAL(wired, 1))
        send_to_char(ch, "You can only set your wired reflexes to off, or between 1 and %d.\r\n", GET_OBJ_VAL(wired, 1));
      else if (GET_OBJ_VAL(trigger, 1) == 1)
        send_to_char("Your wired reflexes are already at that level.\r\n", ch);
      else {
        if (GET_OBJ_VAL(trigger, 1) < i)
          send_to_char("You feel the world slow down as you step your wired reflexes up.\r\n", ch);
        else send_to_char("You feel the world speed up as you step your wired reflexes down.\r\n", ch);
        GET_OBJ_VAL(trigger, 1) = i;
      }
    }
  }
  affect_total(ch);  
}

ACMD(do_flip)
{
  if (GET_NUYEN(ch) < 0) {
    send_to_char("You don't have any coins to flip.\r\n", ch);
    return;
  }
  bool heads = number(0, 1);
  snprintf(buf, sizeof(buf), "$n flips a coin, it shows up %s.", heads ? "heads" : "tails");
  act(buf, TRUE, ch, 0, 0, TO_ROOM);
  snprintf(buf, sizeof(buf), "$n flip a coin, it shows up %s.", heads ? "heads" : "tails");
  act(buf, TRUE, ch, 0, 0, TO_CHAR);
}

ACMD(do_dice)
{
  int dice = 0, tn = 0, suc = 0, roll = 0, tot = 0;
  two_arguments(argument, buf, buf1);
  if (!*buf) {
    send_to_char("Roll how many dice?\r\n", ch);
    return;
  }
  dice = atoi(buf);
  snprintf(buf, sizeof(buf), "%d dice are rolled by $n ", dice);
  if (*buf1) {
    tn = MAX(2, atoi(buf1));
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "against a TN of %d ", tn);
  }
  if (dice <= 0) {
    send_to_char("You have to roll at least 1 die.\r\n", ch);
  } else if (dice > 30) {
    send_to_char("You can't roll that many dice.\r\n", ch); 
  } else {
    strcat(buf, "and scores:");
    for (;dice > 0; dice--) {
       roll = tot = number(1, 6);
       while (roll == 6) {
         tot += roll = number(1, 6);
       }
       if (tn > 0)
         if (tot >= tn) {
           suc++;
           strcat(buf, "^W");
         }
       snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %d^n", tot);
    }    
    strcat(buf, ".");
    if (tn > 0)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %d successes.", suc);
    act(buf, FALSE, ch, 0, 0, TO_ROOM);
    act(buf, FALSE, ch, 0, 0, TO_CHAR);
  }
}

ACMD(do_survey)
{
  struct room_data *room = get_ch_in_room(ch);

  if (ROOM_FLAGGED(room, ROOM_INDOORS))
    strcpy(buf, "The room is ");
  else strcpy(buf, "The area is ");
  if (!room->x)
    strcat(buf, "pretty big. ");
  else {
    int x = room->x, y = room->y, t = room->y;
    if (y > x) {
      y = x;
      x = t;
    }
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "about %d meters long%s %d meters wide", x, ROOM_FLAGGED(room, ROOM_INDOORS) ? "," : " and", y);
    if (ROOM_FLAGGED(room, ROOM_INDOORS))
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " and the ceiling is %.1f meters high", room->z);
    strcat(buf, ". ");
  }
  switch (room->crowd) {
    case 10:
    case 9:
    case 8:
      strcat(buf, "It's nearly impossible to move for people");
      break;
    case 7:
    case 6:
    case 5:
    case 4:
      strcat(buf, "There are a lot of people about");
      break;
    case 3:
    case 2:
    case 1:
      strcat(buf, "There are a few people about");
      break;
  }
  if (room->crowd)
    strcat(buf, " and t");
  else
    strcat(buf, "T");
  int cover = (room->cover + room->crowd) / 2;
  switch (cover) {
    case 10:
    case 9:
    case 8:
      strcat(buf, "here is cover everywhere..");
      break;
    case 7:
    case 6:
    case 5:
    case 4:
      strcat(buf, "here is a lot of cover.");
      break;
    case 3:
    case 2:
    case 1:
    case 0:
      strcat(buf, "here is not much to take cover behind.");
      break;
  }
  strcat(buf, "\r\n");
  send_to_char(buf, ch);
}

extern ACMD_DECLARE(do_pool);

ACMD(do_cpool)
{
  int dodge = 0, bod = 0, off = 0, total = GET_COMBAT(ch), low = 0;
  struct obj_data *one, *two;

  if (!*argument) {
    do_pool(ch, argument, 0, 0);
    return;
  }
  half_chop(argument, arg, buf);
  dodge = atoi(arg);
  half_chop(buf, argument, arg);
  bod = atoi(argument);
  off = atoi(arg);

  total -= ch->real_abils.defense_pool = GET_DEFENSE(ch) = MIN(dodge, total);
  total -= ch->real_abils.body_pool = GET_BODY(ch) = MIN(bod, total);
  
  one = (GET_EQ(ch, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_WEAPON) ? GET_EQ(ch, WEAR_WIELD) :
         (struct obj_data *) NULL;
  two = (GET_EQ(ch, WEAR_HOLD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) == ITEM_WEAPON) ? GET_EQ(ch, WEAR_HOLD) :
         (struct obj_data *) NULL;
  if (!one && !two) {
    if(has_cyberweapon(ch))
      low = GET_SKILL(ch, SKILL_CYBER_IMPLANTS);
    else low = GET_SKILL(ch, SKILL_UNARMED_COMBAT);
  } else if (one) {
    if (!GET_SKILL(ch, GET_OBJ_VAL(one, 4)))
      low = GET_SKILL(ch, return_general(GET_OBJ_VAL(one, 4)));
    else low = GET_SKILL(ch, GET_OBJ_VAL(one, 4));
  } else if (two) {
    if (!GET_SKILL(ch, GET_OBJ_VAL(two, 4)))
      low = GET_SKILL(ch, return_general(GET_OBJ_VAL(two, 4)));
    else low = GET_SKILL(ch, GET_OBJ_VAL(two, 4));
  } else {
    if (GET_SKILL(ch, GET_OBJ_VAL(one, 4)) <= GET_SKILL(ch, GET_OBJ_VAL(two, 4))) {
      if (!GET_SKILL(ch, GET_OBJ_VAL(one, 4)))
        low = GET_SKILL(ch, return_general(GET_OBJ_VAL(one, 4)));
      else low = GET_SKILL(ch, GET_OBJ_VAL(one, 4));
    } else {
      if (!GET_SKILL(ch, GET_OBJ_VAL(two, 4)))
        low = GET_SKILL(ch, return_general(GET_OBJ_VAL(two, 4)));
      else low = GET_SKILL(ch, GET_OBJ_VAL(two, 4));
    }
  }
  total -= ch->real_abils.offense_pool = GET_OFFENSE(ch) = MIN(total, MIN(off, low));
  if (total > 0)
    GET_DEFENSE(ch) += total;
  send_to_char(ch, "Pools set as: Dodge-%d Body-%d Offense-%d\r\n", GET_DEFENSE(ch), GET_BODY(ch), GET_OFFENSE(ch));
}

ACMD(do_spool)
{
  int cast = 0, drain = 0, def = 0, reflect = 0, total = GET_MAGIC(ch);
  half_chop(argument, arg, buf);
  cast = atoi(arg);
  half_chop(buf, argument, arg);
  drain = atoi(argument);
  half_chop(arg, argument, buf);
  def = atoi(argument);
  reflect = atoi(buf);

  total -= ch->real_abils.casting_pool = GET_CASTING(ch) = MIN(cast, total);
  total -= ch->real_abils.drain_pool = GET_DRAIN(ch) = MIN(drain, total);
  total -= ch->real_abils.spell_defense_pool = GET_SDEFENSE(ch) = MIN(def, total);
  if (GET_METAMAGIC(ch, META_REFLECTING) == 2)
    total -= ch->real_abils.reflection_pool = GET_REFLECT(ch) = MIN(reflect, total);
  if (total > 0)
    GET_CASTING(ch) += total;
  snprintf(buf, sizeof(buf), "Pools set as: Casting-%d Drain-%d Defense-%d", GET_CASTING(ch), GET_DRAIN(ch), GET_SDEFENSE(ch));
  if (GET_REFLECT(ch))
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " Reflect-%d", GET_REFLECT(ch));
  strcat(buf, "\r\n");
  send_to_char(buf, ch);
}

ACMD(do_watch)
{
  if (GET_WATCH(ch)) {
    send_to_char("You stop scanning into the distance.\r\n", ch);
    struct char_data *temp;
    REMOVE_FROM_LIST(ch, GET_WATCH(ch)->watching, next_watching);
    GET_WATCH(ch) = NULL;
  } else {
    int dir;
    skip_spaces(&argument);
    if ((dir = search_block(argument, lookdirs, FALSE)) == -1) {
      send_to_char("What direction?\r\n", ch);
      return;
    }
    dir = convert_look[dir];
        
    if (dir == NUM_OF_DIRS) {
      send_to_char("What direction?\r\n", ch);
      return;
    }
      
    if (!CAN_GO(ch, dir)) {
      send_to_char("There seems to be something in the way...\r\n", ch);
      return;
    }
    GET_WATCH(ch) = EXIT2(ch->in_room, dir)->to_room;
    ch->next_watching = GET_WATCH(ch)->watching;
    GET_WATCH(ch)->watching = ch;
    send_to_char(ch, "You focus your attention to %s.\r\n", thedirs[dir]);
  }
}

ACMD(do_trade)
{
  skip_spaces(&argument);
  if (PLR_FLAGGED(ch, PLR_NEWBIE) || PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED))
    send_to_char("You are not ready to use this command.\r\n", ch);
  else if (is_abbrev(argument, "karma")) {
    if (GET_KARMA(ch) < 100)
      send_to_char("You don't have enough karma to trade.\r\n", ch);
    else {
      int amount = dice(2, 6) * 100;
      GET_NUYEN(ch) += amount;
      GET_KARMA(ch) -= 100;
      send_to_char(ch, "You trade in 1 Karma for %d nuyen.\r\n", amount);
    }
  } else if (is_abbrev(argument, "nuyen")) {
    if (GET_NUYEN(ch) < 1800)
      send_to_char("You need to have at least 1,800 nuyen to trade for karma.\r\n", ch);
    else {
      int amount = dice(3, 6) * 100;
      GET_NUYEN(ch) -= amount;
      GET_KARMA(ch) += 100;
      send_to_char(ch, "You spend %d nuyen to buy 1 point of karma.\r\n", amount);
    }
  } else send_to_char("What do you wish to trade?\r\n", ch);
}

ACMD(do_tridlog)
{
  MYSQL_RES *res;
  MYSQL_ROW row;
  if (!*argument) {
    send_to_char("Usage: tridlog [display] [add] [delete]\r\n", ch);
    return;
  }
  two_arguments(argument, arg, buf2);
  if (is_abbrev(arg, "display")) {
    send_to_char("Idnum	Author		Message\r\n"
                 "--------------------------------------------------------------------------------------\r\n", ch);
    if (mysql_wrapper(mysql, "SELECT * FROM trideo_broadcast ORDER BY idnum"))
      return;
    if (!(res = mysql_use_result(mysql)))
      return;
    while ((row = mysql_fetch_row(res)))
      send_to_char(ch, "%ld	%ld		%s\r\n", atol(row[0]), atol(row[1]), row[2]);
    mysql_free_result(res);
  } else if (is_abbrev(arg, "add")) {
    send_to_char("Enter message to be displayed. (Insert Line Breaks With \\r\\n):\r\n", ch);
    STATE(ch->desc) = CON_TRIDEO;
    DELETE_D_STR_IF_EXTANT(ch->desc);
    INITIALIZE_NEW_D_STR(ch->desc);
    ch->desc->max_str = MAX_MESSAGE_LENGTH;
    ch->desc->mail_to = 0;
  } else if (is_abbrev(arg, "delete")) {
    snprintf(buf, sizeof(buf), "DELETE FROM trideo_broadcast WHERE idnum=%d", atoi(buf2));
    mysql_wrapper(mysql, buf);
    send_to_char("Deleted.\r\n", ch);
  }
}

ACMD(do_spray)
{
  skip_spaces(&argument);
  if (!*argument) {
    send_to_char("What do you want to spray?\r\n", ch);
    return;
  }
  for (struct obj_data *obj = ch->carrying; obj; obj = obj->next_content)
    if (GET_OBJ_SPEC(obj) && GET_OBJ_SPEC(obj) == spraypaint) {
      if (strlen(argument) >= LINE_LENGTH) {
        send_to_char("There isn't that much paint in there.\r\n", ch);
        return;
      }
      struct obj_data *paint = read_object(OBJ_GRAFFITI, VIRTUAL);
      snprintf(buf, sizeof(buf), "Someone has sprayed \"%s^g\" here.", argument);
      paint->restring = str_dup(buf);
      paint->graffiti = str_dup(buf);
      obj_to_room(paint, ch->in_room);
      if (++GET_OBJ_TIMER(obj) == 3) {
        send_to_char("The spray can is now empty, so you throw it away.\r\n", ch);
        extract_obj(obj);
      }
      return;
    }


  send_to_char("You don't have anything to spray with.\r\n", ch);
}

ACMD(do_costtime)
{
  if (*argument)
    GET_COST_BREAKUP(ch) = MAX(0, MIN(100, atoi(argument)));
  send_to_char(ch, "Your current cost/time allocation is: %d/%d\r\n", GET_COST_BREAKUP(ch), 100 - GET_COST_BREAKUP(ch));
  snprintf(buf, sizeof(buf), "UPDATE pfiles SET CostTime=%d WHERE idnum=%ld;", GET_COST_BREAKUP(ch), GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
}

ACMD(do_availoffset)
{
  if (*argument)
    GET_AVAIL_OFFSET(ch) = MAX(0, MIN(5, atoi(argument)));
  send_to_char(ch, "Your current availability offset is: %d\r\n", GET_AVAIL_OFFSET(ch));
  snprintf(buf, sizeof(buf), "UPDATE pfiles SET AvailOffset=%d WHERE idnum=%ld;", GET_AVAIL_OFFSET(ch), GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
}

ACMD(do_ammo) {
  struct obj_data *primary = GET_EQ(ch, WEAR_WIELD);
  struct obj_data *secondary = GET_EQ(ch, WEAR_HOLD);
  
  bool sent_a_message = FALSE;
  
  if (primary && IS_GUN(GET_WEAPON_ATTACK_TYPE(primary))) {
    if (primary->contains) {
      send_to_char(ch, "Primary: %d / %d rounds of ammunition.\r\n",
                   MIN(GET_WEAPON_MAX_AMMO(primary), GET_OBJ_VAL(primary->contains, 9)),
                   GET_OBJ_VAL(primary, 5));
    } else {
      send_to_char(ch, "Primary: 0 / %d rounds of ammunition.\r\n", GET_WEAPON_MAX_AMMO(primary));
    }
    sent_a_message = TRUE;
  } else if (primary) {
    send_to_char(ch, "Your primary weapon does not take ammunition.\r\n");
    sent_a_message = TRUE;
  }
  
  if (secondary && IS_GUN(GET_WEAPON_ATTACK_TYPE(secondary))) {
    if (secondary->contains) {
      send_to_char(ch, "Secondary: %d / %d rounds of ammunition.\r\n",
                   MIN(GET_WEAPON_MAX_AMMO(secondary), GET_OBJ_VAL(secondary->contains, 9)),
                   GET_WEAPON_MAX_AMMO(secondary));
    } else {
      send_to_char(ch, "Secondary: 0 / %d rounds of ammunition.\r\n", GET_WEAPON_MAX_AMMO(primary));
    }
    sent_a_message = TRUE;
  } else if (secondary) {
    send_to_char(ch, "Your secondary weapon does not take ammunition.\r\n");
    sent_a_message = TRUE;
  }
  
  if (!sent_a_message) {
    send_to_char(ch, "You're not wielding anything.\r\n");
  }
}

ACMD(do_syspoints) {
  struct char_data *vict;
  char target[MAX_STRING_LENGTH];
  char amt[MAX_STRING_LENGTH];
  char reason[MAX_STRING_LENGTH];
  
  if (IS_NPC(ch)) {
    send_to_char(ch, "NPCs don't get system points, go away.\r\n");
    return;
  }
  
  // Morts can only view their own system points.
  if (!access_level(ch, LVL_ADMIN)) {
    if (!*argument) {
      send_to_char(ch, "You have %d system points.\r\n", GET_SYSTEM_POINTS(ch));
      return;
    }
    
    half_chop(argument, arg, buf);
    
    if (!*arg) {
      send_to_char("Syntax: syspoints restring <item> <string>\r\n", ch);
      return;
    }
    
    // Restring mode.
    if (is_abbrev(arg, "restring")) {
      restring_with_args(ch, buf, TRUE);
      return;
    }
    
    send_to_char("Syntax: syspoints restring <item> <string>\r\n", ch);
    return;
  }
  
  // Viable modes: award penalize show
  half_chop(argument, arg, buf);
  
  if (!*arg) {
    send_to_char("Syntax: syspoints <award|penalize|show> <target>\r\n", ch);
    return;
  }
  
  if (is_abbrev(arg, "show")) {
    // No target? Show your own.
    if (!*buf) {
      send_to_char(ch, "You have %d system points.\r\n", GET_SYSTEM_POINTS(ch));
      return;
    }
    
    // Target specified?
    if (!(vict = get_char_vis(ch, buf))) {
      send_to_char(ch, "You don't see '%s' here.\r\n", buf);
      return;
    }
    
    // Target cannot be NPC.
    if (IS_NPC(vict)) {
      send_to_char(ch, "Not on NPCs.\r\n");
      return;
    }
    
    send_to_char(ch, "%s has %d system points.\r\n", GET_CHAR_NAME(vict), GET_SYSTEM_POINTS(vict));
    return;
  }
  
  // Penalize and award modes.
  half_chop(buf, target, buf2); 
  half_chop(buf2, amt, reason);

  int k = atoi(amt);

  // Require all arguments.
  if (!*arg || !*target || !*amt || !*reason || k <= 0 ) {
    send_to_char("Syntax: syspoints <award|penalize> <player> <amount> <Reason for award>.\r\n", ch);
    return;
  }
  
  // Check mode.
  bool award_mode;
  if (!str_cmp(arg, "award")) {
    award_mode = TRUE;
  } else if (!str_cmp(arg, "penalize")) {
    award_mode = FALSE;
    k *= -1;
  } else {
    send_to_char("Syntax: syspoints <award|penalize> <player> <amount> <Reason for award>.\r\n", ch);
    return;
  }
  
  // Find the target.
  if (!(vict = get_char_vis(ch, target))) {
    send_to_char(ch, "You don't see '%s' here.\r\n", target);
    return;
  }
  
  // Perform the awarding / penalizing.
  if (vict->desc && vict->desc->original)
    vict = vict->desc->original;
    
  if (IS_NPC(vict)) {
    send_to_char("Not on NPCs.\r\n", ch);
    return;
  }

  // Penalizing? It was inverted earlier, so no worries.
  GET_SYSTEM_POINTS(vict) += k;

  snprintf(buf, sizeof(buf), "You have been %s %d system points for %s.\r\n",
          (award_mode ? "awarded" : "penalized"),
          k, 
          reason);
  send_to_char(buf, vict);

  snprintf(buf, sizeof(buf), "You %s %d system points %s %s for %s.\r\n", 
          (award_mode ? "awarded" : "penalized"),
          k, 
          (award_mode ? "to" : "from"),
          GET_CHAR_NAME(vict), 
          reason);
  send_to_char(buf, ch);

  snprintf(buf, sizeof(buf), "%s %s %d system points %s %s for %s (%d to %d).",
          GET_CHAR_NAME(ch), 
          (award_mode ? "awarded" : "penalized"),
          k,
          (award_mode ? "to" : "from"),
          GET_CHAR_NAME(vict), 
          reason, 
          GET_SYSTEM_POINTS(vict) - k, 
          GET_SYSTEM_POINTS(vict));
  mudlog(buf, ch, LOG_WIZLOG, TRUE);
  
  playerDB.SaveChar(vict);
}

bool is_reloadable_weapon(struct obj_data *weapon, int ammotype) {
  // It must exist.
  if (!weapon)
    return FALSE;
    
  // It must be a gun. Bows etc can choke on it.
  if (GET_OBJ_TYPE(weapon) != ITEM_WEAPON || !IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon)))
    return FALSE;
    
  // It must take ammo.
  if (GET_WEAPON_MAX_AMMO(weapon) <= 0)
    return FALSE;
    
  // If it's loaded, the magazine in it must be missing at least one round, or the magazine ammo type can't match.
  if (weapon->contains 
      && GET_MAGAZINE_AMMO_COUNT(weapon->contains) >= GET_MAGAZINE_BONDED_MAXAMMO(weapon->contains)
      && (ammotype == -1 || GET_MAGAZINE_AMMO_TYPE(weapon->contains) == ammotype))
    return FALSE;
    
  // Good to go.
  return TRUE;
}

ACMD(do_save) {
  send_to_char("Worry not! The game auto-saves all characters frequently, and also saves on quit and copyover, so there's no need to manually save.\r\n", ch);
}
