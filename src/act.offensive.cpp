/* ************************************************************************
*   File: act.offensive.c                               Part of CircleMUD *
*  Usage: player-level commands of an offensive nature                    *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <stdio.h>
#include <string.h>

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "newmagic.h"
#include "awake.h"
#include "constants.h"

/* extern variables */
extern struct room_data *world;
extern struct index_data *mob_index;
extern struct descriptor_data *descriptor_list;
extern const char *spirit_powers[];
extern int convert_look[];
extern const char *KILLER_FLAG_MESSAGE;

/* extern functions */
void raw_kill(struct char_data * ch);
extern void range_combat(struct char_data *ch, char target[MAX_INPUT_LENGTH],
                           struct obj_data *weapon, int range, int dir);
extern int find_weapon_range(struct char_data *ch, struct obj_data *weapon);
extern void roll_individual_initiative(struct char_data *ch);
extern bool has_ammo(struct char_data *ch, struct obj_data *wielded);
extern void damage_door(struct char_data *ch, struct room_data *room, int dir, int power, int type);
extern void perform_wear(struct char_data *, struct obj_data *, int);
extern void perform_get_from_container(struct char_data *, struct obj_data *, struct obj_data *, int);
extern int can_wield_both(struct char_data *, struct obj_data *, struct obj_data *);
extern void draw_weapon(struct char_data *);

ACMD(do_assist)
{
  struct char_data *helpee, *opponent;

  if (CH_IN_COMBAT(ch)) {
    send_to_char("You're already fighting!  How can you assist someone else?\r\n", ch);
    return;
  }
  one_argument(argument, arg);

  if (!*arg)
    send_to_char("Whom do you wish to assist?\r\n", ch);
  else if (!(helpee = get_char_room_vis(ch, arg)))
    send_to_char(NOPERSON, ch);
  else if (helpee == ch)
    send_to_char("You can't help yourself any more than this!\r\n", ch);
  else {
    for (opponent = ch->in_room->people; opponent && (FIGHTING(opponent) != helpee);
         opponent = opponent->next_in_room)
      ;

    if (!opponent)
      opponent = FIGHTING(helpee);
    if (!opponent)
      act("But nobody is fighting $M!", FALSE, ch, 0, helpee, TO_CHAR);
    else if (!CAN_SEE(ch, opponent))
      act("You can't see who is fighting $M!", FALSE, ch, 0, helpee, TO_CHAR);
    else {
      send_to_char("You join the fight!\r\n", ch);
      act("$N assists you!", FALSE, helpee, 0, ch, TO_CHAR);
      act("$n assists $N.", FALSE, ch, 0, helpee, TO_NOTVICT);
      // here we add the chars to the respective lists
      set_fighting(ch, opponent);
      if (!FIGHTING(opponent) && AWAKE(opponent))
        set_fighting(opponent, ch);
    }
  }
}

int messageless_find_door(struct char_data *ch, char *type, char *dir, const char *cmdname)
{
  int door;

  if (*dir)
  {
    if ((door = search_block(dir, lookdirs, FALSE)) == -1)
      return -1;

    door = convert_look[door];
    if (EXIT(ch, door)) {
      if (EXIT(ch, door)->keyword) {
        if (isname(type, EXIT(ch, door)->keyword) &&
            !IS_SET(EXIT(ch, door)->exit_info, EX_DESTROYED))
          return door;
        else
          return -1;
      } else
        return door;
    } else
      return -1;
  } else
  {                      /* try to locate the keyword */
    if (!*type)
      return -1;

    for (door = 0; door < NUM_OF_DIRS; door++)
      if (EXIT(ch, door))
        if (EXIT(ch, door)->keyword)
          if (isname(type, EXIT(ch, door)->keyword) &&
              !IS_SET(EXIT(ch, door)->exit_info, EX_DESTROYED))
            return door;

    return -1;
  }
}

bool perform_hit(struct char_data *ch, char *argument, const char *cmdname)
{
  //  extern struct index_data *mob_index;
  struct char_data *vict = NULL;
  struct obj_data *wielded = NULL;
  struct veh_data *veh = NULL;
  int dir, type = DAMOBJ_CRUSH;

  if ((ch->char_specials.rigging ? ch->char_specials.rigging->in_room : ch->in_room)->peaceful)
  {
    send_to_char("This room just has a peaceful, easy feeling...\r\n", ch);
    return TRUE;
  }
  two_arguments(argument, arg, buf2);

  if (ch->in_veh)
  {
    send_to_char(ch, "You can only drive by or use mounts inside a vehicle.\r\n");
    return TRUE;
  }
  
  // If you're wielding a non-weapon, give an error message and bail.
  struct obj_data *weapon = GET_EQ(ch, WEAR_WIELD) ? GET_EQ(ch, WEAR_WIELD) : GET_EQ(ch, WEAR_HOLD);
  if (weapon && (GET_OBJ_TYPE(weapon) != ITEM_WEAPON && GET_OBJ_TYPE(weapon) != ITEM_FIREWEAPON)) {
    send_to_char(ch, "You can't figure out how to attack while using %s as a weapon.\r\n", decapitalize_a_an(GET_OBJ_NAME(weapon)));
    return TRUE;
  }
  
  if (!*arg)
  {
    sprintf(buf, "%s what?\r\n", cmdname);
    send_to_char(ch, CAP(buf));
    return TRUE;
  } else if (!(vict = get_char_room_vis(ch, arg)) && !(veh = get_veh_list(arg, ch->in_room->vehicles, ch)))
  {
    if ((dir = messageless_find_door(ch, arg, buf2, cmdname)) < 0)
      return FALSE;
    if (!str_cmp(cmdname, "kill") || !str_cmp(cmdname, "murder")) {
      send_to_char(ch, "How would you go about %sing an object?\r\n",
                   cmdname);
      return TRUE;
    } else if (CH_IN_COMBAT(ch)) {
      send_to_char("Maybe you'd better wait...\r\n", ch);
      return TRUE;
    } else if (!LIGHT_OK(ch)) {
      send_to_char("How do you expect to do that when you can't see anything?\r\n", ch);
      return TRUE;
    } else if (!EXIT(ch,dir)->keyword) {
      return FALSE;
    } else if (!isname(arg, EXIT(ch, dir)->keyword)) {
      return FALSE;
    } else if (!IS_SET(EXIT(ch, dir)->exit_info, EX_CLOSED)) {
      send_to_char("You can only damage closed doors!\r\n", ch);
      return TRUE;
    }

    if (IS_AFFECTED(ch, AFF_CHARM) && IS_SPIRIT(ch) && ch->master &&
        !IS_NPC(ch->master))
      GET_ACTIVE(ch)--;

    if (GET_EQ(ch, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_WEAPON)
      wielded = GET_EQ(ch, WEAR_WIELD);
    else if (GET_EQ(ch, WEAR_HOLD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) == ITEM_WEAPON)
      wielded = GET_EQ(ch, WEAR_HOLD);
    if (!wielded) {
      draw_weapon(ch);
      if (GET_EQ(ch, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_WEAPON)
        wielded = GET_EQ(ch, WEAR_WIELD);
      else if (GET_EQ(ch, WEAR_HOLD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) == ITEM_WEAPON)
        wielded = GET_EQ(ch, WEAR_HOLD);
    }
    if (wielded) {
      if (GET_OBJ_TYPE(wielded) == ITEM_FIREWEAPON) {
        if (!has_ammo(ch, wielded)) 
          return TRUE;
        WAIT_STATE(ch, PULSE_VIOLENCE * 2);
        if (EXIT(ch, dir)->keyword) {
          sprintf(buf, "$n attacks the %s.", fname(EXIT(ch, dir)->keyword));
          act(buf, TRUE, ch, 0, 0, TO_ROOM);
          sprintf(buf, "You aim $p at the %s!", fname(EXIT(ch, dir)->keyword));
          act(buf, FALSE, ch, wielded, 0, TO_CHAR);
        } else {
          act("$n attacks the door.", TRUE, ch, 0, 0, TO_ROOM);
          act("You aim $p at the door!", FALSE, ch, wielded, 0, TO_CHAR);
        }
        damage_door(ch, ch->in_room, dir, (int)(GET_WEAPON_POWER(wielded) / 2), DAMOBJ_PIERCE);
        return TRUE;
      } else if (GET_WEAPON_ATTACK_TYPE(wielded) >= WEAP_HOLDOUT) {
        if (!has_ammo(ch, wielded))
          return TRUE;
        WAIT_STATE(ch, PULSE_VIOLENCE * 2);
        if (EXIT(ch, dir)->keyword) {
          sprintf(buf, "$n attacks the %s.", fname(EXIT(ch, dir)->keyword));
          act(buf, TRUE, ch, 0, 0, TO_ROOM);
          sprintf(buf, "You aim $p at the %s!", fname(EXIT(ch, dir)->keyword));
          act(buf, FALSE, ch, wielded, 0, TO_CHAR);
        } else {
          act("$n attacks the door.", TRUE, ch, 0, 0, TO_ROOM);
          act("You aim $p at the door!", FALSE, ch, wielded, 0, TO_CHAR);
        }
        damage_door(ch, ch->in_room, dir, (int)(GET_WEAPON_POWER(wielded) / 2), DAMOBJ_PROJECTILE);
        return TRUE;
      } else
        switch (GET_WEAPON_ATTACK_TYPE(wielded)) {
          case WEAP_EDGED:
          case WEAP_POLEARM:
          case WEAP_WHIP:
            type = DAMOBJ_SLASH;
            break;
          case WEAP_CLUB:
            type = DAMOBJ_CRUSH;
            break;
        default:
          if (EXIT(ch, dir)->keyword) {
            sprintf(buf, "You can't damage the %s with $p!",
                    fname(EXIT(ch, dir)->keyword));
            act(buf, FALSE, ch, wielded, 0, TO_CHAR);
          } else
            act("You can't damage the door with $p!",
                FALSE, ch, wielded, 0, TO_CHAR);
          return TRUE;
        }
      WAIT_STATE(ch, PULSE_VIOLENCE * 2);
      if (EXIT(ch, dir)->keyword) {
        sprintf(buf, "$n attacks the %s.", fname(EXIT(ch, dir)->keyword));
        act(buf, TRUE, ch, 0, 0, TO_ROOM);
        sprintf(buf, "You aim $p at the %s!", fname(EXIT(ch, dir)->keyword));
        act(buf, FALSE, ch, wielded, 0, TO_CHAR);
      } else {
        act("$n attacks the door.", TRUE, ch, 0, 0, TO_ROOM);
        act("You aim $p at the door!", FALSE, ch, wielded, 0, TO_CHAR);
      }
      damage_door(ch, ch->in_room, dir, (int)((GET_STR(ch) + GET_WEAPON_POWER(wielded)) / 2), type);
    } else {

      WAIT_STATE(ch, PULSE_VIOLENCE * 2);
      if (EXIT(ch, dir)->keyword) {
        send_to_char(ch, "You take a swing at the %s!\r\n",
                     fname(EXIT(ch, dir)->keyword));
        sprintf(buf, "$n attacks the %s.", fname(EXIT(ch, dir)->keyword));
        act(buf, TRUE, ch, 0, 0, TO_ROOM);
      } else {
        act("You take a swing at the door!", FALSE, ch, 0, 0, TO_CHAR);
        act("$n attacks the door.", FALSE, ch, 0, 0, TO_ROOM);
      }
      damage_door(ch, ch->in_room, dir, (int)(GET_STR(ch) / 2), DAMOBJ_CRUSH);
    }
    return TRUE;
  }

  if (veh)
  {
    if (FIGHTING_VEH(ch)) {
      if (FIGHTING_VEH(ch) == veh) {
        send_to_char(ch, "But you're already attacking it.\r\n");
        return TRUE;
      } else {
        // They're switching targets.
        stop_fighting(ch);
      }
    }
    
    if (!PRF_FLAGGED(ch, PRF_PKER) && veh->owner && GET_IDNUM(ch) != veh->owner) {
      PLR_FLAGS(ch).SetBit(PLR_KILLER);
      send_to_char(KILLER_FLAG_MESSAGE, ch);
    }
    
    if (!FIGHTING(ch)) {
      if (!(GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD)))
        draw_weapon(ch);
      
      if (GET_EQ(ch, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_WEAPON) {
        send_to_char(ch, "You aim %s at %s!\r\n", GET_OBJ_NAME(GET_EQ(ch, WEAR_WIELD)), GET_VEH_NAME(veh));
        sprintf(buf, "%s aims %s right at your ride!\r\n", GET_NAME(ch), GET_OBJ_NAME(GET_EQ(ch, WEAR_WIELD)));
        send_to_veh(buf, veh, NULL, TRUE);
      } else if (GET_EQ(ch, WEAR_HOLD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) == ITEM_WEAPON) {
        send_to_char(ch, "You aim %s at %s!\r\n", GET_OBJ_NAME(GET_EQ(ch, WEAR_HOLD)), GET_VEH_NAME(veh));
        sprintf(buf, "%s aims %s right at your ride!\r\n", GET_NAME(ch), GET_OBJ_NAME(GET_EQ(ch, WEAR_HOLD)));
        send_to_veh(buf, veh, NULL, TRUE);
      } else {
        send_to_char(ch, "You prepare to take a swing at %s!\r\n", GET_VEH_NAME(veh));
        if (get_speed(veh) > 10)
          sprintf(buf, "%s throws %sself out in front of you!\r\n", GET_NAME(ch), thrdgenders[(int)GET_SEX(ch)]);
        else
          sprintf(buf, "%s winds up to take a swing at your ride!\r\n", GET_NAME(ch));
        send_to_veh(buf, veh, NULL, TRUE);
      }
      
      set_fighting(ch, veh);
      return TRUE;
    }
  }
  if (vict == ch)
  {
    send_to_char("You hit yourself...OUCH!.\r\n", ch);
    act("$n hits $mself, and says OUCH!", FALSE, ch, 0, vict, TO_ROOM);
  } else if (IS_AFFECTED(ch, AFF_CHARM) && (ch->master == vict))
    act("$N is just such a good friend, you simply can't hit $M.",
        FALSE, ch, 0, vict, TO_CHAR);
  else
  {
    if (IS_ASTRAL(ch) && !(IS_DUAL(vict) || IS_ASTRAL(vict) || PLR_FLAGGED(vict, PLR_PERCEIVE))) {
      send_to_char("You can't attack someone who isn't astrally active.\r\n", ch);
      return TRUE;
    }
    if (IS_ASTRAL(vict) && !(IS_DUAL(ch) || IS_ASTRAL(ch) || PLR_FLAGGED(ch, PLR_PERCEIVE))) {
      send_to_char("They are nothing but a figment of your imagination.\r\n", ch);
      return TRUE;
    }

    if (!(GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD)))
      draw_weapon(ch);

    if (IS_AFFECTED(ch, AFF_CHARM) && IS_SPIRIT(ch) && ch->master && !IS_NPC(ch->master))
      GET_ACTIVE(ch)--;
    if (!CH_IN_COMBAT(ch)) {
      set_fighting(ch, vict);
      if (!FIGHTING(vict) && AWAKE(vict))
        set_fighting(vict, ch);
      WAIT_STATE(ch, PULSE_VIOLENCE + 2);
      act("$n attacks $N.", TRUE, ch, 0, vict, TO_NOTVICT);
      if (GET_EQ(ch, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_WEAPON) {
        act("You aim $p at $N!", FALSE, ch, GET_EQ(ch, WEAR_WIELD), vict, TO_CHAR);
        act("$n aims $p straight at you!", FALSE, ch, GET_EQ(ch, WEAR_WIELD), vict, TO_VICT);
      } else if (GET_EQ(ch, WEAR_HOLD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) == ITEM_WEAPON) {
        act("You aim $p at $N!", FALSE, ch, GET_EQ(ch, WEAR_HOLD), vict, TO_CHAR);
        act("$n aims $p straight at you!", FALSE, ch, GET_EQ(ch, WEAR_HOLD), vict, TO_VICT);
      } else {
        act("You take a swing at $N!", FALSE, ch, 0, vict, TO_CHAR);
        act("$n prepares to take a swing at you!", FALSE, ch, 0, vict, TO_VICT);
      }
    } else if (FIGHTING(ch) && vict != FIGHTING(ch)) {
      char name[80];
      strcpy(name, GET_NAME(FIGHTING(ch)));
      stop_fighting(ch);
      set_fighting(ch, vict);
      if (!CH_IN_COMBAT(vict) && AWAKE(vict))
        set_fighting(vict, ch);
      WAIT_STATE(ch, PULSE_VIOLENCE + 2);
      sprintf(buf, "$n stops fighting %s and attacks $N!", name);
      act(buf, TRUE, ch, 0, vict, TO_NOTVICT);
      act("You focus on attacking $N.", FALSE, ch, 0, vict, TO_CHAR);
      sprintf(buf, "$n stops fighting %s and aims at you!", name);
      act(buf, TRUE, ch, 0, vict, TO_VICT);
    } else
      send_to_char("You do the best you can!\r\n", ch);
  }
  return TRUE;
}

ACMD(do_hit)
{
  if (!perform_hit(ch, argument, (char *)(subcmd == SCMD_HIT ? "hit" :
                                          (subcmd == SCMD_KILL ? "kill" : "murder"))))
    send_to_char("You can't seem to find the target you're looking for.\r\n", ch);
}

/* so they have to type out 'kill' */
ACMD(do_kil)
{
  send_to_char("You need to type it out if you wanna kill something.\n\r",ch);
}

ACMD(do_kill)
{
  struct char_data *vict;

  if ((!access_level(ch, LVL_VICEPRES)) || IS_NPC(ch)) {
    do_hit(ch, argument, cmd, subcmd);
    return;
  }
  one_argument(argument, arg);

  if (!*arg) {
    send_to_char("Kill who?\r\n", ch);
  } else {
    if (!(vict = get_char_room_vis(ch, arg)))
      send_to_char("They aren't here.\r\n", ch);
    else if (ch == vict)
      send_to_char("Your mother would be so sad.. :(\r\n", ch);
    else {
      act("You chop $M to pieces!  Ah!  The blood!", FALSE, ch, 0, vict, TO_CHAR);
      act("$N chops you to pieces!", FALSE, vict, 0, ch, TO_CHAR);
      act("$n brutally slays $N!", FALSE, ch, 0, vict, TO_NOTVICT);
      if (!IS_NPC(vict)) {
        sprintf(buf2, "%s raw killed by %s. {%s (%ld)}", GET_CHAR_NAME(vict),
                GET_NAME(ch), vict->in_room->name,
                vict->in_room->number);

        mudlog(buf2, vict, LOG_DEATHLOG, TRUE);
      }

      raw_kill(vict);
    }
  }
}

ACMD(do_shoot)
{
  struct obj_data *weapon = NULL;
  char direction[MAX_INPUT_LENGTH];
  char target[MAX_INPUT_LENGTH];
  int dir, i, pos = 0, range = -1;

  if (!(GET_EQ(ch, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_WEAPON) && 
      !(GET_EQ(ch, WEAR_HOLD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) == ITEM_WEAPON)) {
    send_to_char("Wielding a weapon generally helps.\r\n", ch);
    return;
  }

  two_arguments(argument, target, direction);

  if (*direction && AFF_FLAGGED(ch, AFF_DETECT_INVIS)) {
    send_to_char(ch, "The ultrasound distorts your vision.\r\n");
    return;
  }

  for (i = WEAR_WIELD; i <= WEAR_HOLD; i++)
    if ((weapon = GET_EQ(ch, i)) &&
        (GET_OBJ_TYPE(weapon) == ITEM_FIREWEAPON ||
         (GET_OBJ_TYPE(weapon) == ITEM_WEAPON && (IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))
                                                  || GET_WEAPON_ATTACK_TYPE(weapon) == WEAP_GREN_LAUNCHER
                                                  || GET_WEAPON_ATTACK_TYPE(weapon) == WEAP_MISS_LAUNCHER))))
      if (find_weapon_range(ch, weapon) > range)
        pos = i;

  if (!pos) {
    send_to_char("Normally guns or bows are used to do that.\r\n", ch);
    return;
  }

  if (perform_hit(ch, argument, "shoot"))
    return;

  weapon = GET_EQ(ch, pos);
  range = find_weapon_range(ch, weapon);

  if (!*target) {
    send_to_char("Syntax: shoot <target>\r\n"
                 "        shoot <target> <direction>\r\n", ch);
    return;
  }

  if ((dir = search_block(direction, lookdirs, FALSE)) == -1) {
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

  range_combat(ch, target, weapon, range, dir);
}

ACMD(do_throw)
{
  struct obj_data *weapon = NULL;
  int i, dir;
  char arg2[MAX_INPUT_LENGTH];
  char arg1[MAX_INPUT_LENGTH];
  two_arguments(argument, arg1, arg2);

  if (!(GET_EQ(ch, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_WEAPON) && 
      !(GET_EQ(ch, WEAR_HOLD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) == ITEM_WEAPON)) {
    send_to_char("You're not wielding anything!\r\n", ch);
    return;
  }

  for (i = WEAR_WIELD; i <= WEAR_HOLD; i++)
    if ((weapon = GET_EQ(ch, i)) &&
        (GET_WEAPON_ATTACK_TYPE(weapon) == TYPE_SHURIKEN ||
         GET_WEAPON_ATTACK_TYPE(weapon) == TYPE_THROWING_KNIFE ||
         GET_WEAPON_ATTACK_TYPE(weapon) == TYPE_HAND_GRENADE))
      break;

  if (i > WEAR_HOLD || !weapon) {
    send_to_char("You should wield a throwing weapon first!\r\n", ch);
    return;
  }

  if (GET_WEAPON_ATTACK_TYPE(weapon) == TYPE_HAND_GRENADE) {
    if (!*arg1) {
      send_to_char("Syntax: throw <direction>\r\n", ch);
      return;
    }
    if ((dir = search_block(arg1, lookdirs, FALSE)) == -1) {
      send_to_char("What direction?\r\n", ch);
      return;
    }
    dir = convert_look[dir];
  } else {
    if (!*arg1 || !*arg2) {
      send_to_char("Syntax: throw <target> <direction>\r\n", ch);
      return;
    }

    if ((dir = search_block(arg2, lookdirs, FALSE)) == -1) {
      send_to_char("What direction?\r\n", ch);
      return;
    }
    dir = convert_look[dir];
  }

  if (dir == NUM_OF_DIRS) {
    send_to_char("What direction?\r\n", ch);
    return;
  }

  if (!CAN_GO(ch, dir)) {
    send_to_char("There seems to be something in the way...\r\n", ch);
    return;
  }
  if (GET_WEAPON_ATTACK_TYPE(weapon) == TYPE_HAND_GRENADE)
    range_combat(ch, NULL, weapon, 1, dir);
  else
    range_combat(ch, arg1, weapon, 1, dir);
}

ACMD_CONST(do_flee) {
  ACMD(do_flee);
  do_flee(ch, NULL, cmd, subcmd);
}

ACMD(do_flee)
{
  // You get six tries to escape per flee command.
  for (int tries = 0; tries < 6; tries++) {
    int attempt = number(0, NUM_OF_DIRS - 2);       /* Select a random direction */
    if (CAN_GO(ch, attempt) && (!IS_NPC(ch) || !ROOM_FLAGGED(ch->in_room->dir_option[attempt]->to_room, ROOM_NOMOB))) {
      // Supply messaging and put the character into a wait state to match wait state in perform_move.
      act("$n panics, and attempts to flee!", TRUE, ch, 0, 0, TO_ROOM);
      WAIT_STATE(ch, PULSE_VIOLENCE * 2);
      
      // If the character is fighting in melee combat, they must pass a test to escape.
      if (GET_POS(ch) >= POS_FIGHTING && FIGHTING(ch) && !AFF_FLAGGED(ch, AFF_PRONE)) {
        if (!success_test(GET_QUI(ch), GET_QUI(FIGHTING(ch)))) {
          act("$N cuts you off as you try to escape!", TRUE, ch, 0, FIGHTING(ch), TO_CHAR);
          act("You lunge forward and block $n's escape.", TRUE, ch, 0, FIGHTING(ch), TO_VICT);
          act("$N lunges forward and blocks $n's escape.", TRUE, ch, 0, FIGHTING(ch), TO_NOTVICT);
          return;
        }
      }
      
      // Attempt to move through the selected exit.
      if (do_simple_move(ch, attempt, CHECK_SPECIAL | LEADER, NULL)) {
        send_to_char("You flee head over heels.\r\n", ch);
      } else {
        act("$n tries to flee, but can't!", TRUE, ch, 0, 0, TO_ROOM);
      }
      
      // Once we've selected a valid direction,
      return;
    }
  }
  send_to_char("PANIC! You couldn't escape!\r\n", ch);
  WAIT_STATE(ch, PULSE_VIOLENCE * 2);
}

ACMD(do_kick)
{
  struct char_data *vict;
  int dir;

  any_one_arg(argument, arg);

  if (!*arg) {
    send_to_char("Kick who or what?\r\n", ch);
    return;
  }

  for (dir = 0; dir < NUM_OF_DIRS; dir++)
    if (EXIT(ch, dir) && EXIT(ch, dir)->keyword &&
        isname(arg, EXIT(ch, dir)->keyword) &&
        !IS_SET(EXIT(ch, dir)->exit_info, EX_DESTROYED))
      break;
  if (dir >= NUM_OF_DIRS) {
    if (!(vict = get_char_room_vis(ch, arg)))
      send_to_char("They aren't here.\r\n", ch);
    else if (vict == ch) {
      send_to_char("Are you flexible enough to do that?\r\n", ch);
      act("$n tries to kick $s own ass... not too bright, is $e?",
          TRUE, ch, 0, 0, TO_ROOM);
    } else {
      act("You sneak up behind $N and kick $M straight in the ass!",
          FALSE, ch, 0, vict, TO_CHAR);
      act("$n sneaks up behind you and kicks you in the ass!\r\n"
          "That's gonna leave a mark...", FALSE, ch, 0, vict, TO_VICT);
      act("$n sneaks up behind $N and gives $M a swift kick in the ass!",
          TRUE, ch, 0, vict, TO_NOTVICT);
    }
    return;
  }

  if (CH_IN_COMBAT(ch))
    send_to_char("Maybe you'd better wait...\r\n", ch);
  else if (!IS_SET(EXIT(ch, dir)->exit_info, EX_CLOSED))
    send_to_char("You can only damage closed doors!\r\n", ch);
  else if (!LIGHT_OK(ch))
    send_to_char("How do you expect to do that when you can't see anything?\r\n", ch);
  else if (ch->in_room->peaceful)
    send_to_char("Nah - leave it in peace.\r\n", ch);
  else {
    WAIT_STATE(ch, PULSE_VIOLENCE * 2);
    if (EXIT(ch, dir)->keyword) {
      send_to_char(ch, "You show the %s the bottom of %s!\r\n",
                   fname(EXIT(ch, dir)->keyword), GET_EQ(ch, WEAR_FEET) ?
                   GET_OBJ_NAME(GET_EQ(ch, WEAR_FEET)) : "your foot");
      sprintf(buf, "$n kicks the %s.", fname(EXIT(ch, dir)->keyword));
      act(buf, TRUE, ch, 0, 0, TO_ROOM);
    } else {
      send_to_char(ch, "You show the door the bottom of %s!\r\n",
                   GET_EQ(ch, WEAR_FEET) ?
                   GET_EQ(ch, WEAR_FEET)->text.name : "your foot");
      act("$n kicks the door.", TRUE, ch, 0, 0, TO_ROOM);
    }
    damage_door(ch, ch->in_room, dir, (int)(GET_STR(ch) / 2), DAMOBJ_CRUSH);
  }
}

ACMD(do_retract)
{
  skip_spaces(&argument);
  struct obj_data *cyber = NULL;  
  if (*argument)
    cyber = get_obj_in_list_vis(ch, argument, ch->cyberware);
  else {
    for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content)
      switch (GET_CYBERWARE_TYPE(obj)) {
        case CYB_HANDRAZOR:  
        case CYB_HANDBLADE:  
        case CYB_HANDSPUR:   
          if (!IS_SET(GET_OBJ_VAL(obj, 3), CYBERWEAPON_RETRACTABLE))  
            continue;
          // fall through
        case CYB_CLIMBINGCLAWS:
        case CYB_FOOTANCHOR:  
        case CYB_FIN:  
          cyber = obj;
          break;     
      }
  }  
  if (!cyber) {
    if (!*argument) 
      send_to_char("You don't have any retractable cyberware.\r\n", ch);
    else send_to_char("You don't have that cyberware.\r\n", ch);
  } else if (!(GET_CYBERWARE_TYPE(cyber) == CYB_HANDRAZOR || GET_CYBERWARE_TYPE(cyber) == CYB_HANDSPUR || GET_CYBERWARE_TYPE(cyber) == CYB_HANDBLADE || GET_CYBERWARE_TYPE(cyber) == CYB_FOOTANCHOR || GET_CYBERWARE_TYPE(cyber) == CYB_FIN))
    send_to_char("That cyberware isn't retractable.\r\n",ch ); 
  else if ((GET_CYBERWARE_TYPE(cyber) == CYB_HANDRAZOR || GET_CYBERWARE_TYPE(cyber) == CYB_HANDSPUR || GET_CYBERWARE_TYPE(cyber) == CYB_HANDBLADE) && !IS_SET(GET_CYBERWARE_FLAGS(cyber), CYBERWEAPON_RETRACTABLE))
    send_to_char("That cyberweapon isn't retractable.\r\n",ch );
  else {
    if (GET_CYBERWARE_IS_DISABLED(cyber)) {
      GET_CYBERWARE_IS_DISABLED(cyber) = FALSE;
      sprintf(buf, "$n extends %s.", GET_OBJ_NAME(cyber));
      act(buf, TRUE, ch, 0, 0, TO_ROOM);
      send_to_char(ch, "You extend %s.\r\n", GET_OBJ_NAME(cyber));
    } else {
      GET_CYBERWARE_IS_DISABLED(cyber) = TRUE;
      sprintf(buf, "$n retracts %s.", GET_OBJ_NAME(cyber));
      act(buf, TRUE, ch, 0, 0, TO_ROOM);
      send_to_char(ch, "You retract %s.\r\n", GET_OBJ_NAME(cyber));
    }
    affect_total(ch);
  }
}

ACMD(do_mode)
{
  struct obj_data *weapon = NULL;
  int mode = 0;
  
  if (!(weapon = GET_EQ(ch, WEAR_WIELD)) || GET_OBJ_TYPE(weapon) != ITEM_WEAPON || !IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon)))
    send_to_char("You aren't wielding a firearm.\r\n", ch);
  else if (!*argument) {
    send_to_char(ch, "%s's available fire modes: \r\n", CAP(GET_OBJ_NAME(weapon)));
    if (WEAPON_CAN_USE_FIREMODE(weapon, MODE_SS))
      send_to_char(ch, "%s%s^n\r\n", GET_WEAPON_FIREMODE(weapon) == MODE_SS ? "^R" : "", fire_mode[MODE_SS]);
    if (WEAPON_CAN_USE_FIREMODE(weapon, MODE_SA))
      send_to_char(ch, "%s%s^n\r\n", GET_WEAPON_FIREMODE(weapon) == MODE_SA ? "^R" : "", fire_mode[MODE_SA]);
    if (WEAPON_CAN_USE_FIREMODE(weapon, MODE_BF))
      send_to_char(ch, "%s%s^n\r\n", GET_WEAPON_FIREMODE(weapon) == MODE_BF ? "^R" : "", fire_mode[MODE_BF]);
    if (WEAPON_CAN_USE_FIREMODE(weapon, MODE_FA))
      send_to_char(ch, "%s%s (%d rounds)^n\r\n", GET_WEAPON_FIREMODE(weapon) == MODE_FA ? "^R" : "", fire_mode[MODE_FA], GET_WEAPON_FULL_AUTO_COUNT(weapon));
  } else {
    skip_spaces(&argument);
    two_arguments(argument, arg, buf1);
    if (!str_cmp(arg, "SS") && WEAPON_CAN_USE_FIREMODE(weapon, MODE_SS))
      mode = MODE_SS;
    else if (!str_cmp(arg, "SA") && WEAPON_CAN_USE_FIREMODE(weapon, MODE_SA))
      mode = MODE_SA;
    else if (!str_cmp(arg, "BF") && WEAPON_CAN_USE_FIREMODE(weapon, MODE_BF))
      mode = MODE_BF;
    else if (!str_cmp(arg, "FA") && WEAPON_CAN_USE_FIREMODE(weapon, MODE_FA)) {
      mode = MODE_FA;
      if (*buf1)
        GET_WEAPON_FULL_AUTO_COUNT(weapon) = MIN(10, MAX(3, atoi(buf1)));
      else
        GET_WEAPON_FULL_AUTO_COUNT(weapon) = 10;
    }
    if (!mode)
      send_to_char(ch, "You can't set %s to that firing mode.\r\n", GET_OBJ_NAME(weapon));
    else {
      send_to_char(ch, "You set %s to %s.\r\n", GET_OBJ_NAME(weapon), fire_mode[mode]);
      GET_WEAPON_FIREMODE(weapon) = mode;
      act("$n flicks the fire selector switch on $p.", TRUE, ch, weapon, 0, TO_ROOM);
    }
  }
}

ACMD(do_prone)
{
  if (AFF_FLAGGED(ch, AFF_PRONE)) {
    GET_INIT_ROLL(ch) -= 10;
    act("$n gets to $s feet.", TRUE, ch, 0, 0, TO_ROOM);
    send_to_char("You get to your feet.\r\n", ch);
  } else {
    act("$n drops into a prone position.", TRUE, ch, 0, 0, TO_ROOM);
    send_to_char("You drop prone.\r\n", ch);   
  }
  AFF_FLAGS(ch).ToggleBit(AFF_PRONE);
}
