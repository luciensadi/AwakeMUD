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
#include <vector>
#include <utility>

#include "structs.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "newmagic.hpp"
#include "awake.hpp"
#include "constants.hpp"
#include "newdb.hpp"
#include "ignore_system.hpp"

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
extern void perform_get_from_container(struct char_data *, struct obj_data *, struct obj_data *, int);
extern int can_wield_both(struct char_data *, struct obj_data *, struct obj_data *);
extern void find_and_draw_weapon(struct char_data *);
extern bool can_hurt(struct char_data *ch, struct char_data *victim, int attacktype, bool include_func_protections);
extern bool does_weapon_have_bayonet(struct obj_data *weapon);


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
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
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
#ifdef IGNORING_IC_ALSO_IGNORES_COMBAT
      if (IS_IGNORING(opponent, is_blocking_ic_interaction_from, ch)) {
        act("You can't see who is fighting $M!", FALSE, ch, 0, helpee, TO_CHAR);
        log_attempt_to_bypass_ic_ignore(ch, opponent, "do_assist");
        return;
      }

      if (IS_IGNORING(ch, is_blocking_ic_interaction_from, opponent)) {
        send_to_char("You can't attack someone you've blocked IC interaction with.\r\n", ch);
        return;
      }
#endif
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
        if (isname(type, EXIT(ch, door)->keyword)
            && !IS_SET(EXIT(ch, door)->exit_info, EX_DESTROYED)
            && !IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN))
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
          if (isname(type, EXIT(ch, door)->keyword)
              && !IS_SET(EXIT(ch, door)->exit_info, EX_DESTROYED)
              && !IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN))
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

  if (!*arg) {
    send_to_char(ch, "%s what?\r\n", capitalize(cmdname));
    return TRUE;
  }

  vict = get_char_room_vis(ch, arg);
  veh = get_veh_list(arg, ch->in_room->vehicles, ch);

  if (!vict && !veh) {
    if ((dir = messageless_find_door(ch, arg, buf2, cmdname)) < 0)
      return FALSE;
    if (!str_cmp(cmdname, "kill") || !str_cmp(cmdname, "murder")) {
      send_to_char(ch, "How would you go about %sing an object?\r\n", cmdname);
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
      find_and_draw_weapon(ch);
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
          snprintf(buf, sizeof(buf), "$n attacks the %s to %s.", fname(EXIT(ch, dir)->keyword), thedirs[dir]);
          act(buf, TRUE, ch, 0, 0, TO_ROOM);
          snprintf(buf, sizeof(buf), "You aim $p at the %s to %s!", fname(EXIT(ch, dir)->keyword), thedirs[dir]);
          act(buf, FALSE, ch, wielded, 0, TO_CHAR);
        } else {
          snprintf(buf, sizeof(buf), "$n attacks the door to %s.", thedirs[dir]);
          act(buf, TRUE, ch, 0, 0, TO_ROOM);
          act("You aim $p at the door!", FALSE, ch, wielded, 0, TO_CHAR);
        }
        damage_door(ch, ch->in_room, dir, (int)(GET_WEAPON_POWER(wielded) / 2), DAMOBJ_PIERCE);
        return TRUE;
      } else if (GET_WEAPON_ATTACK_TYPE(wielded) >= WEAP_HOLDOUT) {
        if (!has_ammo(ch, wielded))
          return TRUE;
        WAIT_STATE(ch, PULSE_VIOLENCE * 2);
        if (EXIT(ch, dir)->keyword) {
          snprintf(buf, sizeof(buf), "$n attacks the %s to %s.", fname(EXIT(ch, dir)->keyword), thedirs[dir]);
          act(buf, TRUE, ch, 0, 0, TO_ROOM);
          snprintf(buf, sizeof(buf), "You aim $p at the %s to %s!", fname(EXIT(ch, dir)->keyword), thedirs[dir]);
          act(buf, FALSE, ch, wielded, 0, TO_CHAR);
        } else {
          snprintf(buf, sizeof(buf), "$n attacks the door to %s.", thedirs[dir]);
          act(buf, TRUE, ch, 0, 0, TO_ROOM);
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
            snprintf(buf, sizeof(buf), "You can't damage the %s with $p!",
                    fname(EXIT(ch, dir)->keyword));
            act(buf, FALSE, ch, wielded, 0, TO_CHAR);
          } else
            act("You can't damage the door with $p!",
                FALSE, ch, wielded, 0, TO_CHAR);
          return TRUE;
        }
      WAIT_STATE(ch, PULSE_VIOLENCE * 2);
      if (EXIT(ch, dir)->keyword) {
        snprintf(buf, sizeof(buf), "$n attacks the %s to %s.", fname(EXIT(ch, dir)->keyword), thedirs[dir]);
        act(buf, TRUE, ch, 0, 0, TO_ROOM);
        snprintf(buf, sizeof(buf), "You aim $p at the %s!", fname(EXIT(ch, dir)->keyword));
        act(buf, FALSE, ch, wielded, 0, TO_CHAR);
      } else {
        snprintf(buf, sizeof(buf), "$n attacks the door to %s.", thedirs[dir]);
        act(buf, TRUE, ch, 0, 0, TO_ROOM);
        act("You aim $p at the door!", FALSE, ch, wielded, 0, TO_CHAR);
      }
      damage_door(ch, ch->in_room, dir, (int)((GET_STR(ch) + GET_WEAPON_POWER(wielded)) / 2), type);
    } else {

      WAIT_STATE(ch, PULSE_VIOLENCE * 2);
      if (EXIT(ch, dir)->keyword) {
        send_to_char(ch, "You take a swing at the %s!\r\n",
                     fname(EXIT(ch, dir)->keyword));
        snprintf(buf, sizeof(buf), "$n attacks the %s to %s.", fname(EXIT(ch, dir)->keyword), thedirs[dir]);
        act(buf, TRUE, ch, 0, 0, TO_ROOM);
      } else {
        act("You take a swing at the door!", FALSE, ch, 0, 0, TO_CHAR);
        snprintf(buf, sizeof(buf), "$n attacks the door to %s.", thedirs[dir]);
        act(buf, FALSE, ch, 0, 0, TO_ROOM);
      }
      damage_door(ch, ch->in_room, dir, (int)(GET_STR(ch) / 2), DAMOBJ_CRUSH);
    }
    return TRUE;
  }

  if (veh)
  {
    if (veh->damage >= VEH_DAM_THRESHOLD_DESTROYED) {
      send_to_char(ch, "%s is already destroyed, better find something else to %s.\r\n", GET_VEH_NAME(veh), cmdname);
      return TRUE;
    }

    if (FIGHTING_VEH(ch)) {
      if (FIGHTING_VEH(ch) == veh) {
        send_to_char(ch, "But you're already attacking it.\r\n");
        return TRUE;
      } else {
        // They're switching targets.
        stop_fighting(ch);
      }
    }

    if (FIGHTING(ch)) {
      send_to_char("You're already in combat!\r\n", ch);
      return TRUE;
    }

    // Abort for PvP damage failure.
    if (!can_damage_vehicle(ch, veh))
      return TRUE;

    if (!(GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD)))
      find_and_draw_weapon(ch);

    if (GET_EQ(ch, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_WEAPON) {
      send_to_char(ch, "You aim %s at %s!\r\n", GET_OBJ_NAME(GET_EQ(ch, WEAR_WIELD)), GET_VEH_NAME(veh));
      snprintf(buf, sizeof(buf), "%s aims %s right at your ride!\r\n", GET_NAME(ch), GET_OBJ_NAME(GET_EQ(ch, WEAR_WIELD)));
      send_to_veh(buf, veh, NULL, TRUE);
    } else if (GET_EQ(ch, WEAR_HOLD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) == ITEM_WEAPON) {
      send_to_char(ch, "You aim %s at %s!\r\n", GET_OBJ_NAME(GET_EQ(ch, WEAR_HOLD)), GET_VEH_NAME(veh));
      snprintf(buf, sizeof(buf), "%s aims %s right at your ride!\r\n", GET_NAME(ch), GET_OBJ_NAME(GET_EQ(ch, WEAR_HOLD)));
      send_to_veh(buf, veh, NULL, TRUE);
    } else {
      send_to_char(ch, "You prepare to take a swing at %s!\r\n", GET_VEH_NAME(veh));
      if (get_speed(veh) > 10)
        snprintf(buf, sizeof(buf), "%s throws %sself out in front of you!\r\n", GET_NAME(ch), thrdgenders[(int)GET_SEX(ch)]);
      else
        snprintf(buf, sizeof(buf), "%s winds up to take a swing at your ride!\r\n", GET_NAME(ch));
      send_to_veh(buf, veh, NULL, TRUE);
    }

    set_fighting(ch, veh);
    return TRUE;
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
    if (!IS_NPC(ch) && !IS_NPC(vict) && (!PRF_FLAGGED(ch, PRF_PKER) || !PRF_FLAGGED(vict, PRF_PKER))) {
      send_to_char("You and your opponent must both be toggled PK for that.\r\n", ch);
      return TRUE;
    }

#ifdef IGNORING_IC_ALSO_IGNORES_COMBAT
    if (IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
      send_to_char("They are nothing but a figment of your imagination.\r\n", ch);
      log_attempt_to_bypass_ic_ignore(ch, vict, "perform_hit");
      return TRUE;
    }

    if (IS_IGNORING(ch, is_blocking_ic_interaction_from, vict)) {
      send_to_char("You can't attack someone you've blocked IC interaction with.\r\n", ch);
      return TRUE;
    }
#endif

    if (!(GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD)))
      find_and_draw_weapon(ch);

    if (IS_AFFECTED(ch, AFF_CHARM) && IS_SPIRIT(ch) && ch->master && !IS_NPC(ch->master))
      GET_ACTIVE(ch)--;
    if (!CH_IN_COMBAT(ch)) {
      set_fighting(ch, vict);
      if (!FIGHTING(vict) && AWAKE(vict))
        set_fighting(vict, ch);
      WAIT_STATE(ch, PULSE_VIOLENCE + 2);
      act("$n attacks $N.", TRUE, ch, 0, vict, TO_NOTVICT);
      if (GET_EQ(ch, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_WEAPON) {
        if (!GET_EQ(ch, WEAR_WIELD)->contains && does_weapon_have_bayonet(GET_EQ(ch, WEAR_WIELD))) {
          act("With your weapon empty, you aim the bayonet on $p at $N!", FALSE, ch, GET_EQ(ch, WEAR_WIELD), vict, TO_CHAR);
          act("$n aims the bayonet on $p straight at you!", FALSE, ch, GET_EQ(ch, WEAR_WIELD), vict, TO_VICT);
        }
        else {
          act("You aim $p at $N!", FALSE, ch, GET_EQ(ch, WEAR_WIELD), vict, TO_CHAR);
          act("$n aims $p straight at you!", FALSE, ch, GET_EQ(ch, WEAR_WIELD), vict, TO_VICT);
        }
      } else if (GET_EQ(ch, WEAR_HOLD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) == ITEM_WEAPON) {
        if (!GET_EQ(ch, WEAR_HOLD)->contains && does_weapon_have_bayonet(GET_EQ(ch, WEAR_HOLD))) {
          act("With your weapon empty, you aim the bayonet on $p at $N!", FALSE, ch, GET_EQ(ch, WEAR_HOLD), vict, TO_CHAR);
          act("$n aims the bayonet on $p straight at you!", FALSE, ch, GET_EQ(ch, WEAR_HOLD), vict, TO_VICT);
        }
        else {
          act("You aim $p at $N!", FALSE, ch, GET_EQ(ch, WEAR_HOLD), vict, TO_CHAR);
          act("$n aims $p straight at you!", FALSE, ch, GET_EQ(ch, WEAR_HOLD), vict, TO_VICT);
        }
      } else {
        act("You take a swing at $N!", FALSE, ch, 0, vict, TO_CHAR);
        act("$n prepares to take a swing at you!", FALSE, ch, 0, vict, TO_VICT);
      }
    } else if ((FIGHTING(ch) && vict != FIGHTING(ch)) || FIGHTING_VEH(ch)) {
      char name[200];
      strcpy(name, FIGHTING(ch) ? GET_NAME(FIGHTING(ch)) : GET_VEH_NAME(FIGHTING_VEH(ch)));
      stop_fighting(ch);
      set_fighting(ch, vict);
      if (!CH_IN_COMBAT(vict) && AWAKE(vict))
        set_fighting(vict, ch);
      WAIT_STATE(ch, PULSE_VIOLENCE + 2);
      snprintf(buf, sizeof(buf), "$n stops fighting %s and attacks $N!", name);
      act(buf, TRUE, ch, 0, vict, TO_NOTVICT);
      act("You focus on attacking $N.", FALSE, ch, 0, vict, TO_CHAR);
      snprintf(buf, sizeof(buf), "$n stops fighting %s and aims at you!", name);
      act(buf, TRUE, ch, 0, vict, TO_VICT);
    } else
      send_to_char("You do the best you can!\r\n", ch);
  }
  return TRUE;
}

ACMD(do_hit)
{
  if (!perform_hit(ch, argument, (subcmd == SCMD_HIT ? "hit" : (subcmd == SCMD_KILL ? "kill" : "murder"))))
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
        snprintf(buf2, sizeof(buf2), "%s raw killed by %s. {%s (%ld)}", GET_CHAR_NAME(vict),
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

  if (((weapon = GET_EQ(ch, WEAR_WIELD)) || (weapon = GET_EQ(ch, WEAR_HOLD))) && GET_OBJ_TYPE(weapon) == ITEM_WEAPON && !weapon->contains) {
    send_to_char("You should probably load it first.\r\n", ch);
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
  ACMD_DECLARE(do_flee);
  do_flee(ch, NULL, cmd, subcmd);
}

bool passed_flee_success_check(struct char_data *ch) {
  if (GET_POS(ch) < POS_FIGHTING)
    return TRUE;

  if (!FIGHTING(ch))
    return TRUE;

  if (ch->in_room != FIGHTING(ch)->in_room)
    return TRUE;

  if (AFF_FLAGGED(ch, AFF_APPROACH) || AFF_FLAGGED(FIGHTING(ch), AFF_APPROACH))
    return TRUE;

  if (!can_hurt(ch, FIGHTING(ch), TRUE, 0))
    return TRUE;

  int racial_flee_modifier = 0; // Satyrs have a x4 run multiplier and smaller races have x2, so we're houseruling a +1/-1 TN here.
  switch (GET_RACE(ch)) {
    case RACE_SATYR:
      racial_flee_modifier--;
      break;
    case RACE_DWARF:
    case RACE_GNOME:
    case RACE_MENEHUNE:
      racial_flee_modifier++;
      break;
      }
  return success_test(GET_QUI(ch), (GET_REA(FIGHTING(ch)) + racial_flee_modifier)) > 0;

  }

bool _sort_pairs_by_weight(std::pair<int, int> a, std::pair<int, int> b) {
  return std::get<1>(a) > std::get<1>(b);
}

ACMD(do_flee)
{
  if (AFF_FLAGGED(ch, AFF_PRONE)) {
    send_to_char("It's a struggle to flee while prone!\r\n", ch);
    return;
  }

  std::vector<std::pair<int, int>> valid_directions = {};

  for (int dir = 0; dir <= DOWN; dir++) {
    if (CAN_GO(ch, dir)
        && (!IS_NPC(ch) || !ROOM_FLAGGED(ch->in_room->dir_option[dir]->to_room, ROOM_NOMOB))
        && (!ROOM_FLAGGED(ch->in_room->dir_option[dir]->to_room, ROOM_FALL)))
    {
      // Add it to our valid directions list with a random weight.
      valid_directions.push_back(std::pair<int, int>(dir, number(0, 10000)));
    }
  }

  if (valid_directions.empty()) {
    send_to_char("PANIC! There's nowhere you can flee to!\r\n", ch);
    WAIT_STATE(ch, PULSE_VIOLENCE * 2);
  } else {
    // Sort our list of valid directions by weight.
    std::sort(valid_directions.begin(), valid_directions.end(), _sort_pairs_by_weight);

    // Get the first valid direction from the sorted list.
    int dir = std::get<0>(valid_directions.front());

    // Supply messaging and put the character into a wait state to match wait state in perform_move.
    act("$n panics, and attempts to flee!", TRUE, ch, 0, 0, TO_ROOM);
    WAIT_STATE(ch, PULSE_VIOLENCE * 2);

    // If the character is fighting in melee combat with someone they can hurt, they must pass a test to escape.
    if (!passed_flee_success_check(ch)) {
      act("$N cuts you off as you try to escape!", TRUE, ch, 0, FIGHTING(ch), TO_CHAR);
      act("You lunge forward and block $n's escape.", TRUE, ch, 0, FIGHTING(ch), TO_VICT);
      act("$N lunges forward and blocks $n's escape.", TRUE, ch, 0, FIGHTING(ch), TO_NOTVICT);
      return;
    }

    // Attempt to move through the selected exit.
    if (do_simple_move(ch, dir, CHECK_SPECIAL | LEADER, NULL)) {
      send_to_char("You flee head over heels.\r\n", ch);
    } else {
      act("$n tries to flee, but can't!", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char("You can't get away!\r\n", ch);
      snprintf(buf, sizeof(buf), "Error case in do_flee: do_simple_move failure for %s exit.", dirs[dir]);
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    }
  }
}

ACMD(do_kick)
{
  struct char_data *vict;
  int dir;

  two_arguments(argument, arg, buf2);

  if (!*arg) {
    send_to_char("Kick who or what?\r\n", ch);
    return;
  }

  if ((dir = messageless_find_door(ch, arg, buf2, "do_kick")) < 0) {
    if (!(vict = get_char_room_vis(ch, arg)))
      send_to_char("They aren't here.\r\n", ch);
    else if (vict == ch) {
      send_to_char("Are you flexible enough to do that?\r\n", ch);
      act("$n tries to kick $s own ass... not too bright, is $e?",
          TRUE, ch, 0, 0, TO_ROOM);
    } else {
      act("You sneak up behind $N and kick $M straight in the ass!",
          FALSE, ch, 0, vict, TO_CHAR);
      if (!IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
        act("$n sneaks up behind you and kicks you in the ass!\r\n"
            "That's gonna leave a mark...", FALSE, ch, 0, vict, TO_VICT);
        log_attempt_to_bypass_ic_ignore(ch, vict, "kick");
      }
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
      snprintf(buf, sizeof(buf), "$n kicks the %s to %s.", fname(EXIT(ch, dir)->keyword), thedirs[dir]);
      act(buf, TRUE, ch, 0, 0, TO_ROOM);
    } else {
      send_to_char(ch, "You show the door the bottom of %s!\r\n",
                   GET_EQ(ch, WEAR_FEET) ?
                   GET_EQ(ch, WEAR_FEET)->text.name : "your foot");
      snprintf(buf, sizeof(buf), "$n kicks the door to %s.", thedirs[dir]);
      act(buf, TRUE, ch, 0, 0, TO_ROOM);
    }
    damage_door(ch, ch->in_room, dir, (int)(GET_STR(ch) / 2), DAMOBJ_CRUSH);
  }
}

bool cyber_is_retractable(struct obj_data *cyber) {
  if (GET_OBJ_TYPE(cyber) != ITEM_CYBERWARE) {
    mudlog("SYSERR: Received non-cyberware to cyber_is_retractable!", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  switch (GET_CYBERWARE_TYPE(cyber)) {
    case CYB_HANDRAZOR:
    case CYB_HANDBLADE:
    case CYB_HANDSPUR:
    case CYB_CLIMBINGCLAWS:
      if (!IS_SET(GET_CYBERWARE_FLAGS(cyber), 1 << CYBERWEAPON_RETRACTABLE))
        return FALSE;
      // fallthrough
    case CYB_FOOTANCHOR:
    case CYB_FIN:
      return TRUE;
  }

  return FALSE;
}

ACMD(do_retract)
{
  skip_spaces(&argument);
  struct obj_data *cyber = NULL;
  if (*argument)
    cyber = get_obj_in_list_vis(ch, argument, ch->cyberware);
  else {
    for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content) {
      if (cyber_is_retractable(obj)) {
        cyber = obj;
        break;
      }
    }
  }

  // Found nothing?
  if (!cyber) {
    if (!*argument) {
      send_to_char("You don't have any retractable cyberware.\r\n", ch);
    } else {
      send_to_char("You don't have that cyberware.\r\n", ch);
    }
    return;
  }

  // Found something, but it's not flagged retractable.
  if (!cyber_is_retractable(cyber)) {
    send_to_char(ch, "%s isn't retractable.\r\n", capitalize(GET_OBJ_NAME(cyber)));
    return;
  }

  if (GET_CYBERWARE_IS_DISABLED(cyber)) {
    GET_CYBERWARE_IS_DISABLED(cyber) = FALSE;
    snprintf(buf, sizeof(buf), "$n extends %s.", decapitalize_a_an(GET_OBJ_NAME(cyber)));
    act(buf, TRUE, ch, 0, 0, TO_ROOM);
    send_to_char(ch, "You extend %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(cyber)));
  } else {
    GET_CYBERWARE_IS_DISABLED(cyber) = TRUE;
    snprintf(buf, sizeof(buf), "$n retracts %s.", decapitalize_a_an(GET_OBJ_NAME(cyber)));
    act(buf, TRUE, ch, 0, 0, TO_ROOM);
    send_to_char(ch, "You retract %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(cyber)));
  }
  affect_total(ch);
}

void display_firemodes_to_character(struct char_data *ch, struct obj_data *weapon) {
  send_to_char(ch, "%s's available fire modes: \r\n", CAP(GET_OBJ_NAME(weapon)));
  if (WEAPON_CAN_USE_FIREMODE(weapon, MODE_SS))
    send_to_char(ch, "%s%s^n\r\n", GET_WEAPON_FIREMODE(weapon) == MODE_SS ? "^R" : "", fire_mode[MODE_SS]);
  if (WEAPON_CAN_USE_FIREMODE(weapon, MODE_SA))
    send_to_char(ch, "%s%s^n\r\n", GET_WEAPON_FIREMODE(weapon) == MODE_SA ? "^R" : "", fire_mode[MODE_SA]);
  if (WEAPON_CAN_USE_FIREMODE(weapon, MODE_BF))
    send_to_char(ch, "%s%s^n\r\n", GET_WEAPON_FIREMODE(weapon) == MODE_BF ? "^R" : "", fire_mode[MODE_BF]);
  if (WEAPON_CAN_USE_FIREMODE(weapon, MODE_FA))
    send_to_char(ch, "%s%s (%d rounds)^n\r\n", GET_WEAPON_FIREMODE(weapon) == MODE_FA ? "^R" : "", fire_mode[MODE_FA], GET_WEAPON_FULL_AUTO_COUNT(weapon));
}

ACMD(do_mode)
{
  skip_spaces(&argument);

  // Always try to take their wielded weapon.
  struct obj_data *weapon = GET_EQ(ch, WEAR_WIELD), *mount = NULL;

  // Override: You're in a vehicle and manning a mount. Take that mount's weapon.
  if (ch->in_veh && AFF_FLAGGED(ch, AFF_MANNING)) {
    struct obj_data *mount = get_mount_manned_by_ch(ch);
    if (mount) {
      weapon = get_mount_weapon(mount);
    } else {
      mudlog("SYSERR: AFF_MANNING but no mount manned!", ch, LOG_SYSLOG, TRUE);
      AFF_FLAGS(ch).RemoveBit(AFF_MANNING);
    }

    if (!weapon) {
      send_to_char("Your mount has no weapon.\r\n", ch);
      return;
    }
  }

  // Override: You're rigging/remote controlling. You have access to all mounts.
  else if (AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE)) {
    struct veh_data *veh;
    int num_mounts = 0, mount_num;

    RIG_VEH(ch, veh);

    for (mount = veh->mount; mount; mount = mount->next_content) {
      num_mounts++;
    }

    if (!num_mounts) {
      send_to_char("This vehicle has no mounts.\r\n", ch);
      return;
    }

    if (!*argument) {
      send_to_char("Syntax: MODE <mount number> [mode]\r\n", ch);
      return;
    } else {
      char mount_num_string[MAX_INPUT_LENGTH];
      argument = one_argument(argument, mount_num_string);
      mount_num = atoi(mount_num_string);

      if (mount_num < 0 || !isdigit(*mount_num_string)) {
        send_to_char("Syntax: MODE <mount number> [mode]\r\n", ch);
        return;
      }
    }

    for (mount = veh->mount; mount && mount_num > 0; mount = mount->next_content) {
      mount_num--;
    }

    if (!mount) {
      send_to_char("There aren't that many mounts in the vehicle.\r\n", ch);
      return;
    }

    weapon = get_mount_weapon(mount);

    if (!weapon) {
      send_to_char("That mount has no weapon.\r\n", ch);
      return;
    }
  }

  if (!weapon) {
    send_to_char("You're not wielding a firearm.\r\n", ch);
    return;
  }

  if (GET_OBJ_TYPE(weapon) != ITEM_WEAPON || !IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))) {
    send_to_char(ch, "%s isn't a firearm.\r\n", CAP(GET_OBJ_NAME(weapon)));
    return;
  }

  if (!*argument) {
    display_firemodes_to_character(ch, weapon);
    return;
  }

  two_arguments(argument, arg, buf1);
  if ((!str_cmp(arg, "SS") || str_str(fire_mode[MODE_SS], arg))) {
    if (!WEAPON_CAN_USE_FIREMODE(weapon, MODE_SS)) {
      send_to_char(ch, "%s is more advanced than that! Single Shot is for revolvers, bolt-action rifles, etc.\r\n", capitalize(GET_OBJ_NAME(weapon)));
      return;
    }
    GET_WEAPON_FIREMODE(weapon) = MODE_SS;
  }
  else if ((!str_cmp(arg, "SA") || str_str(fire_mode[MODE_SA], arg))) {
    if (!WEAPON_CAN_USE_FIREMODE(weapon, MODE_SA)) {
      send_to_char(ch, "%s isn't capable of semi-automatic fire.\r\n", capitalize(GET_OBJ_NAME(weapon)));
      return;
    }
    GET_WEAPON_FIREMODE(weapon) = MODE_SA;
  }
  else if ((!str_cmp(arg, "BF") || str_str(fire_mode[MODE_BF], arg))) {
    if (!WEAPON_CAN_USE_FIREMODE(weapon, MODE_BF)) {
      send_to_char(ch, "%s isn't capable of burst fire.\r\n", capitalize(GET_OBJ_NAME(weapon)));
      return;
    }
    GET_WEAPON_FIREMODE(weapon) = MODE_BF;
  }
  else if ((!str_cmp(arg, "FA") || str_str(fire_mode[MODE_FA], arg))) {
    if (!WEAPON_CAN_USE_FIREMODE(weapon, MODE_FA)) {
      send_to_char(ch, "%s isn't capable of full auto.\r\n", capitalize(GET_OBJ_NAME(weapon)));
      return;
    }

    if (*buf1)
      GET_WEAPON_FULL_AUTO_COUNT(weapon) = MIN(10, MAX(3, atoi(buf1)));
    else {
      GET_WEAPON_FULL_AUTO_COUNT(weapon) = 10;
      send_to_char("Using default FA value of 10. You can change this with 'mode FA X' where X is the number of bullets to fire.\r\n", ch);
    }
    GET_WEAPON_FIREMODE(weapon) = MODE_FA;
  }
  else {
    send_to_char("That's not a recognized mode.\r\n", ch);
    display_firemodes_to_character(ch, weapon);
    return;
  }

  // Message them about the change.
  if (GET_WEAPON_FIREMODE(weapon) == MODE_FA)
    send_to_char(ch, "You set %s to %s (%d rounds per firing).\r\n", decapitalize_a_an(GET_OBJ_NAME(weapon)), fire_mode[MODE_FA], GET_WEAPON_FULL_AUTO_COUNT(weapon));
  else
    send_to_char(ch, "You set %s to %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(weapon)), fire_mode[GET_WEAPON_FIREMODE(weapon)]);
  act("$n flicks the fire selector switch on $p.", TRUE, ch, weapon, 0, TO_ROOM);
}

ACMD(do_prone)
{
  // Lose half an init pass to simulate taking a simple action.
  GET_INIT_ROLL(ch) -= 5;

  // Set wait state.
  WAIT_STATE(ch, 0.5 RL_SEC);

  if (AFF_FLAGGED(ch, AFF_PRONE)) {
    // Wounded? You gotta pass a Will test to stand (SR3 p106)
    if (GET_PHYSICAL(ch) < 10 || GET_MENTAL(ch) < 10) {
      char pronebuf[1000];
      strlcpy(pronebuf, "^ostand-while-wounded will test: ", sizeof(pronebuf));

      int dice = GET_WIL(ch) + GET_TASK_POOL(ch, WIL);
      int tn = 2 + modify_target_rbuf_raw(ch, pronebuf, sizeof(pronebuf), 8, FALSE);
      int successes = success_test(dice, tn);

      snprintf(ENDOF(pronebuf), sizeof(pronebuf) - strlen(pronebuf), "; %d dice vs TN %d: %d success%s", dice, tn, successes, successes == 1 ? "" : "s");
      act(pronebuf, FALSE, ch, 0, 0, TO_ROLLS);

      if (successes <= 0) {
        send_to_char("You can't quite overcome the pain of your injuries, and collapse back into the prone position.\r\n", ch);
        act("$n struggles to rise.", TRUE, ch, 0, 0, TO_ROOM);
        return;
      }

      act("$n fights through the pain to get back on $s feet.", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char("You fight through the pain to get back to your feet.\r\n", ch);
    } else {
      act("$n gets to $s feet.", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char("You get to your feet.\r\n", ch);
    }
    AFF_FLAGS(ch).RemoveBit(AFF_PRONE);
    return;
  }

  else {
    // Modify messaging for people wielding guns with bipods / tripods.
    struct obj_data *weapon = GET_EQ(ch, WEAR_WIELD);
    struct obj_data *attach = NULL;

    if (WEAPON_IS_GUN(weapon) && GET_WEAPON_ATTACH_UNDER_VNUM(weapon) > 0) {
      rnum_t attach_rnum = real_object(GET_WEAPON_ATTACH_UNDER_VNUM(weapon));
      if (attach_rnum >= 0) {
        attach = &obj_proto[attach_rnum];

        // We only allow bipods and tripods here.
        if (GET_OBJ_TYPE(attach) != ITEM_GUN_ACCESSORY || !(GET_ACCESSORY_TYPE(attach) == ACCESS_TRIPOD || GET_ACCESSORY_TYPE(attach) == ACCESS_BIPOD)) {
          attach = NULL;
        }
      }
    }

    if (!attach) {
      act("$n settles into a prone position.", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char("You settle into a prone position.\r\n", ch);
    } else {
      if (GET_ACCESSORY_TYPE(attach) == ACCESS_TRIPOD) {
        act("$n settles into a prone position, deploying $p's tripod.", TRUE, ch, weapon, 0, TO_ROOM);
        send_to_char("You settle into a prone position, taking an additional moment to deploy your weapon's tripod.\r\n", ch);
        // Lose an additional half an init pass to simulate the simple action to deploy the tripod.
        GET_INIT_ROLL(ch) -= 5;
        WAIT_STATE(ch, 1 RL_SEC);
      } else {
        act("$n settles into a prone position, resting $p on its bipod.", TRUE, ch, weapon, 0, TO_ROOM);
        send_to_char("You settle into a prone position, resting your weapon on its bipod.\r\n", ch);
      }
    }

    AFF_FLAGS(ch).SetBit(AFF_PRONE);
    return;

    /*  After writing this, I realized it only applies when you DROP prone instead of taking a simple action. -LS

    // SR3 p105: Must make a Willpower(Force) test to avoid dropping a sustained spell when dropping prone.
    struct sustain_data *next;
    for (struct sustain_data *sust = GET_SUSTAINED(victim); sust; sust = next) {
      next = sust->next;
      if (sust->caster && !sust->focus && !sust->spirit) {
        strlcat(buf, "Maintain-sustain-while-prone test: ", sizeof(buf));

        int dice = GET_WILL(ch);
        int tn = MAX(2, sust->force + modify_target_rbuf_raw(ch, buf, sizeof(buf), 8, TRUE));
        int successes = success_test(dice, tn);
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  %d dice vs TN %d, %d successes.", dice, tn, successes);
        act(buf, FALSE, ch, 0, 0, TO_ROLLS);

        if (successes <= 0) {
          end_sustained_spell(victim, sust);
        }
      }
    }
    */
  }
}
