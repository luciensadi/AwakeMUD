/* ************************************************************************
*   File: act.movement.c                                Part of CircleMUD *
*  Usage: movement commands, door handling, & sleep/rest/etc state        *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <stdio.h>
#include <string.h>

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "house.h"
#include "transport.h"
#include "constants.h"
#include "newmagic.h"

/* external functs */
int special(struct char_data * ch, int cmd, char *arg);
void death_cry(struct char_data * ch);
int find_eq_pos(struct char_data * ch, struct obj_data * obj, char *arg);
void perform_fall(struct char_data *);
bool check_fall(struct char_data *, int);
extern int modify_target(struct char_data *);
extern int convert_damage(int);
extern void check_quest_destination(struct char_data *ch, struct char_data *mob);
extern bool memory(struct char_data *ch, struct char_data *vict);
extern bool is_escortee(struct char_data *mob);
extern bool hunting_escortee(struct char_data *ch, struct char_data *vict);
extern void death_penalty(struct char_data *ch);
extern int modify_veh(struct veh_data *veh);
extern void make_desc(struct char_data *ch, struct char_data *i);

extern sh_int mortal_start_room;
extern sh_int frozen_start_room;
extern vnum_t newbie_start_room;

/* can_move determines if a character can move in the given direction, and
   generates the appropriate message if not */
int can_move(struct char_data *ch, int dir, int extra)
{
  SPECIAL(escalator);
  int test, i, target, skill, dam;

  if (IS_AFFECTED(ch, AFF_PETRIFY))
    return 0;

  char empty_argument = '\0';
  if (IS_SET(extra, CHECK_SPECIAL) && special(ch, convert_dir[dir], &empty_argument))
    return 0;

  if (world[ch->in_room].icesheet[0] && !IS_ASTRAL(ch))
    if (success_test(GET_QUI(ch), world[ch->in_room].icesheet[0] + modify_target(ch)) < 1)
    {
      send_to_char("The ice at your feet causes you to trip and fall!\r\n", ch);
      return 0;
    }
  if (IS_AFFECTED(ch, AFF_CHARM) && ch->master && ch->in_room == ch->master->in_room)
  {
    send_to_char("The thought of leaving your master makes you weep.\r\n", ch);
    act("$n bursts into tears.", FALSE, ch, 0, 0, TO_ROOM);
    return 0;
  }
  if (ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_FREEWAY) && GET_LEVEL(ch) == 1) {
    send_to_char("Walking across the freeway would spell instant death.\r\n", ch);
    return 0;
  }
  if (ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_HOUSE))
    if (!House_can_enter(ch, world[EXIT(ch, dir)->to_room].number))
    {
      send_to_char("That's private property -- no trespassing!\r\n", ch);
      return 0;
    }
  if (ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_TUNNEL) && !IS_ASTRAL(ch))
    if (world[EXIT(ch, dir)->to_room].people &&
        world[EXIT(ch, dir)->to_room].people->next_in_room)
    {
      send_to_char("There isn't enough room there for another person!\r\n", ch);
      return 0;
    }

  if (world[ch->in_room].func && world[ch->in_room].func == escalator)
  {
    send_to_char("You can't get off a moving escalator!\r\n", ch);
    return 0;
  }

  if (IS_WATER(ch->in_room) && !IS_NPC(ch) &&
      !IS_SENATOR(ch))
  {
    target = MAX(2, world[ch->in_room].rating);
    skill = SKILL_ATHLETICS;
        
    i = get_skill(ch, skill, target);
    for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content)
      if (GET_OBJ_VAL(cyber, 0) == CYB_FIN && !GET_OBJ_VAL(cyber, 9)) {
        i += GET_QUI(ch) / 2;
        break;
      }
    i = resisted_test(i, target + modify_target(ch), target, i);
    if (i < 0) {
      test = success_test(GET_WIL(ch), modify_target(ch) - i);
      dam = convert_damage(stage(-test, SERIOUS));
      send_to_char(ch, "You struggle to retain consciousness as the current "
                   "resists your every move.\r\n");
      if (dam > 0)
        damage(ch, ch, dam, TYPE_DROWN, FALSE);
      if (GET_POS(ch) < POS_STANDING)
        return 0;
    } else if (!i) {
      test = success_test(GET_WIL(ch), 3 + modify_target(ch));
      dam = convert_damage(stage(-test, MODERATE));
      send_to_char(ch, "The current resists your movements.\r\n");
      if (dam > 0)
        damage(ch, ch, dam, TYPE_DROWN, FALSE);
      if (GET_POS(ch) < POS_STANDING)
        return 0;
    } else if (i < 3) {
      test = success_test(GET_WIL(ch), 5 - i + modify_target(ch));
      dam = convert_damage(stage(-test, LIGHT));
      send_to_char(ch, "The current weakly resists your efforts to move.\r\n");
      if (dam > 0)
        damage(ch, ch, dam, TYPE_DROWN, FALSE);
      if (GET_POS(ch) < POS_STANDING)
        return 0;
    }
  }

  return 1;
}

/* do_simple_move assumes
 *      1. That there is no master and no followers.
 *      2. That the direction exists.
 *
 *   Returns :
 *   1 : If success.
 *   0 : If fail
 */
int do_simple_move(struct char_data *ch, int dir, int extra, struct char_data *vict)
{
  int saw_sneaker, was_in, skill, target, i;
  struct obj_data *bio;
  struct char_data *tch;
  struct veh_data *tveh;
  if (!can_move(ch, dir, extra))
    return 0;

  GET_LASTROOM(ch) = world[ch->in_room].number;

  if (world[ch->in_room].dir_option[dir]->to_room >= real_room(FIRST_CAB) &&
      world[ch->in_room].dir_option[dir]->to_room <= real_room(LAST_CAB))
    sprintf(buf2, "$n gets into the taxi.");
  else if (vict)
  {
    sprintf(buf1, "$n drag $N %s.", fulldirs[dir]);
    sprintf(buf2, "$n drags %s %s.", GET_NAME(vict), fulldirs[dir]);
    act(buf1, FALSE, ch, 0, vict, TO_CHAR);
  } else if (ch->char_specials.leave)
    sprintf(buf2, "$n %s %s.", ch->char_specials.leave, fulldirs[dir]);
  else if (IS_WATER(ch->in_room) && !IS_WATER(EXIT(ch, dir)->to_room))
    sprintf(buf2, "$n climbs out of the water to the %s.", fulldirs[dir]);
  else if (!IS_WATER(ch->in_room) && IS_WATER(EXIT(ch, dir)->to_room))
    sprintf(buf2, "$n jumps into the water to the %s.", fulldirs[dir]);
  else
    sprintf(buf2, "$n %s %s.", (IS_WATER(ch->in_room) ? "swims" : "leaves"), fulldirs[dir]);

  if (IS_AFFECTED(ch, AFF_SNEAK))
  {
    target = GET_SKILL(ch, SKILL_STEALTH) ? 0 : 4;
    skill = get_skill(ch, SKILL_STEALTH, target);
    int tchtarg = skill;
    if (affected_by_spell(ch, SPELL_STEALTH))
      tchtarg += 4;
    if (world[ch->in_room].silence[0])
      tchtarg += world[ch->in_room].silence[1];
    for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room) {
      if (AWAKE(tch) && CAN_SEE(tch, ch) && (tch != ch) && !PRF_FLAGGED(tch, PRF_MOVEGAG)) {
        saw_sneaker = resisted_test(GET_INT(tch), tchtarg, skill, (GET_INT(tch) + target));
        if (saw_sneaker >= 0)
          act(buf2, TRUE, ch, 0, tch, TO_VICT);
      }
    }
  } else
  {
    for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room)
      if (tch != ch && !PRF_FLAGGED(tch, PRF_MOVEGAG))
        act(buf2, TRUE, ch, 0, tch, TO_VICT);
    for (tveh = world[ch->in_room].vehicles; tveh; tveh = tveh->next_veh)
      for (tch = tveh->people; tch; tch = tch->next_in_veh)
        if (tveh->cspeed <= SPEED_IDLE || (tveh->cspeed > SPEED_IDLE && success_test(GET_INT(tch), 4)))
          act(buf2, TRUE, ch, 0, tch, TO_VICT);
    if (world[ch->in_room].watching)
      for (struct char_data *tch = world[ch->in_room].watching; tch; tch = tch->next_watching)
          act(buf2, TRUE, ch, 0, tch, TO_VICT);
  }
  was_in = ch->in_room;
  STOP_WORKING(ch);
  char_from_room(ch);
  char_to_room(ch, world[was_in].dir_option[dir]->to_room);

  if (ROOM_FLAGGED(was_in, ROOM_INDOORS) && !ROOM_FLAGGED(ch->in_room, ROOM_INDOORS))
  {
    extern const char *moon[];
    const char *time_of_day[] = {
                                  "night air",
                                  "dawn light",
                                  "daylight",
                                  "dim dusk light"
                                };
    const char *weather_line[] = {
                                   "The sky is clear.\r\n",
                                   "Clouds fill the sky.\r\n",
                                   "Rain falls around you.\r\n",
                                   "Lightning flickers as the rain falls.\r\n"
                                 };
    sprintf(buf, "You step out into the %s. ", time_of_day[weather_info.sunlight]);
    if (weather_info.sunlight == SUN_DARK && weather_info.sky == SKY_CLOUDLESS)
      sprintf(ENDOF(buf), "You see the %s moon in the cloudless sky.\r\n", moon[weather_info.moonphase]);
    else
      strcat(buf, weather_line[weather_info.sky]);
    send_to_char(buf, ch);
  }
  if (ROOM_FLAGGED(ch->in_room, ROOM_INDOORS) && GET_HEIGHT(ch) >= world[ch->in_room].z * 100)
    send_to_char("You have to slouch to fit in here.\r\n", ch);
  if (ch->desc != NULL)
    look_at_room(ch, 0);
  if (was_in >= real_room(FIRST_CAB) && was_in <= real_room(LAST_CAB))
    sprintf(buf2, "$n gets out of the taxi.");
  else if (was_in == real_room(1004))
    sprintf(buf2, "$n arrives from dennis.");
  else if (vict)
    sprintf(buf2, "$n drags %s in from %s.", GET_NAME(vict), thedirs[rev_dir[dir]]);
  else if (ch->char_specials.arrive)
    sprintf(buf2, "$n %s %s.", ch->char_specials.arrive, thedirs[rev_dir[dir]]);
  else if (IS_WATER(was_in) && !IS_WATER(ch->in_room))
    sprintf(buf2, "$n climbs out of the water from %s.", thedirs[rev_dir[dir]]);
  else if (!IS_WATER(was_in) && IS_WATER(ch->in_room))
    sprintf(buf2, "$n jumps into the water from %s.", thedirs[rev_dir[dir]]);
  else
    sprintf(buf2, "$n %s %s.", (IS_WATER(ch->in_room) ?
                                "swims in from" : "arrives from"), thedirs[rev_dir[dir]]);

  if (IS_AFFECTED(ch, AFF_SNEAK))
  {
    target = (GET_SKILL(ch, SKILL_STEALTH) ? 0 : 4);
    skill = get_skill(ch, SKILL_STEALTH, target);
    int tchtarg = skill;
    if (affected_by_spell(ch, SPELL_STEALTH))
      tchtarg += 4;
    if (world[ch->in_room].silence[0])
      tchtarg += world[ch->in_room].silence[1];
    for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room) {
      if (AWAKE(tch) && CAN_SEE(tch, ch) && (tch != ch)) {
        saw_sneaker = resisted_test(GET_INT(tch), tchtarg, skill, (GET_INT(tch) + target));
        if (saw_sneaker >= 0) {
          if (!PRF_FLAGGED(tch, PRF_MOVEGAG))
            act(buf2, TRUE, ch, 0, tch, TO_VICT);
          if (IS_NPC(tch)) {
            if ((!IS_NPC(ch) || IS_PROJECT(ch)) &&
                !PRF_FLAGGED(ch, PRF_NOHASSLE) && !FIGHTING(tch) &&
                MOB_FLAGGED(tch, MOB_AGGRESSIVE) && CAN_SEE(tch, ch) &&
                IS_SET(extra, LEADER) && AWAKE(tch))
              set_fighting(tch, ch);
            GET_MOBALERT(tch) = MALERT_ALERT;
            GET_MOBALERTTIME(tch) = 20;
          }
        }
      }
    }
  } else
  {
    for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room)
      if (tch != ch && !PRF_FLAGGED(tch, PRF_MOVEGAG))
        act(buf2, TRUE, ch, 0, tch, TO_VICT);
    for (tveh = world[ch->in_room].vehicles; tveh; tveh = tveh->next_veh)
      for (tch = tveh->people; tch; tch = tch->next_in_veh)
        if (tveh->cspeed <= SPEED_IDLE || (tveh->cspeed > SPEED_IDLE && success_test(GET_INT(tch), 4)))
          act(buf2, TRUE, ch, 0, tch, TO_VICT);
    if (world[ch->in_room].watching)
      for (struct char_data *tch = world[ch->in_room].watching; tch; tch = tch->next_watching)
        act(buf2, TRUE, ch, 0, tch, TO_VICT);
    for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room) {
      if (!FIGHTING(tch) && CAN_SEE(tch, ch) && hunting_escortee(tch, ch))
        set_fighting(tch, ch);
      else if (IS_NPC(tch)) {
        if ((!IS_NPC(ch) || IS_PROJECT(ch) || is_escortee(ch)) &&
               !PRF_FLAGGED(ch, PRF_NOHASSLE) && !FIGHTING(tch) &&
               MOB_FLAGGED(tch, MOB_AGGRESSIVE) && CAN_SEE(tch, ch) &&
               IS_SET(extra, LEADER) && AWAKE(tch))
          set_fighting(tch, ch);
          GET_MOBALERT(tch) = MALERT_ALERT;
          GET_MOBALERTTIME(tch) = 20;      
      }
    }
  }

  if (ROOM_FLAGGED(ch->in_room, ROOM_DEATH) && !IS_NPC(ch) &&
      !IS_SENATOR(ch))
  {
    send_to_char("You feel the world slip in to darkness, you better hope a wandering Docwagon finds you.\r\n", ch);
    death_cry(ch);
    act("$n vanishes into thin air.", FALSE, ch, 0, 0, TO_ROOM);
    death_penalty(ch);
    for (bio = ch->bioware; bio; bio = bio->next_content)
      if (GET_OBJ_VAL(bio, 0) == BIO_PLATELETFACTORY)
        GET_OBJ_VAL(bio, 5) = 24;
      else if (GET_OBJ_VAL(bio, 0) == BIO_ADRENALPUMP) {
        if (GET_OBJ_VAL(bio, 5) > 0)
          for (i = 0; i < MAX_OBJ_AFFECT; i++)
            affect_modify(ch,
                          bio->affected[i].location,
                          bio->affected[i].modifier,
                          bio->obj_flags.bitvector, FALSE);
        GET_OBJ_VAL(bio, 5) = 0;
      }
    PLR_FLAGS(ch).SetBit(PLR_JUST_DIED);
    if (FIGHTING(ch))
      stop_fighting(ch);
    log_death_trap(ch);
    char_from_room(ch);
    char_to_room(ch, real_room(16295));
    extract_char(ch);
  } else if (ROOM_FLAGGED(ch->in_room, ROOM_FALL) && !IS_ASTRAL(ch) &&
             !IS_SENATOR(ch))
  {
    perform_fall(ch);
  } else if (IS_NPC(ch) && ch->master && !IS_NPC(ch->master) &&
             GET_QUEST(ch->master) && ch->in_room == ch->master->in_room)
    check_quest_destination(ch->master, ch);

  return 1;
}

// check fall returns TRUE if the player FELL, FALSE if (s)he did not
bool check_fall(struct char_data *ch, int modifier)
{
  int base_target = world[ch->in_room].rating + modify_target(ch);
  int i, autosucc = 0, dice, success;

  for (i = WEAR_LIGHT; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_CLIMBING)
      base_target -= GET_OBJ_VAL(GET_EQ(ch, i), 0);
  for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content)
    if (GET_OBJ_VAL(cyber, 0) == CYB_BALANCETAIL)
      base_target -= 2;
    else if (GET_OBJ_VAL(cyber, 0) == CYB_BALANCEAUG)
      base_target -= 2;

  base_target += modifier;
  base_target = MIN(base_target, 52);
  if (base_target < 2)
  {
    autosucc = 2 - base_target;
    base_target = 2;
  }

  dice = get_skill(ch, SKILL_ATHLETICS, base_target);
  dice += GET_REA(ch);
  success = success_test(dice + autosucc, base_target);

  if (success < 1)
    return TRUE;

  send_to_char("You grab on to the wall and keep yourself from falling!\r\n", ch);
  return FALSE;
}

void perform_fall(struct char_data *ch)
{
  int i, levels = 0, was_in;
  float meters = 0;
  struct obj_data *bio;

  // run a loop and drop them through each room
  while (EXIT(ch, DOWN) && ROOM_FLAGGED(ch->in_room, ROOM_FALL) &&
         levels < 10)
  {
    if (!check_fall(ch, levels * 4))
      return;
    levels++;
    meters += world[ch->in_room].z;
    was_in = ch->in_room;
    act("^R$n falls down below!^n", TRUE, ch, 0, 0, TO_ROOM);
    char_from_room(ch);
    send_to_char("^RYou fall!^n\r\n", ch);
    char_to_room(ch, world[was_in].dir_option[DOWN]->to_room);
    look_at_room(ch, 0);
    act("^R$n falls in from above!^n", TRUE, ch, 0, 0, TO_ROOM);
    if (ROOM_FLAGGED(ch->in_room, ROOM_DEATH) && !IS_NPC(ch) &&
        !IS_SENATOR(ch)) {
      send_to_char("You feel the world slip in to darkness, you better hope a wandering Docwagon finds you.\r\n", ch);
      death_cry(ch);
      act("$n vanishes into thin air.", FALSE, ch, 0, 0, TO_ROOM);
      death_penalty(ch);
      for (bio = ch->bioware; bio; bio = bio->next_content)
        if (GET_OBJ_VAL(bio, 0) == BIO_PLATELETFACTORY)
          GET_OBJ_VAL(bio, 5) = 24;
        else if (GET_OBJ_VAL(bio, 0) == BIO_ADRENALPUMP) {
          if (GET_OBJ_VAL(bio, 5) > 0)
            for (i = 0; i < MAX_OBJ_AFFECT; i++)
              affect_modify(ch,
                            bio->affected[i].location,
                            bio->affected[i].modifier,
                            bio->obj_flags.bitvector, FALSE);
          GET_OBJ_VAL(bio, 5) = 0;
        }
      PLR_FLAGS(ch).SetBit(PLR_JUST_DIED);
      if (FIGHTING(ch))
        stop_fighting(ch);
      sprintf(buf, "%s ran into DeathTrap at %ld",
              GET_CHAR_NAME(ch), world[ch->in_room].number);
      mudlog(buf, ch, LOG_DEATHLOG, TRUE);
      char_from_room(ch);
      char_to_room(ch, real_room(16295));

      extract_char(ch);
      return;
    }
  }

  if (levels)
  {
    if (GET_TRADITION(ch) == TRAD_ADEPT)
      meters -= GET_POWER(ch, ADEPT_FREEFALL) * 2;
    if (meters <= 0) {
      send_to_char(ch, "You manage to land on your feet.\r\n");
      act("$e manages to land on $s feet.", FALSE, ch, 0, 0, TO_ROOM);
      return;
    }
    int power = (int)(meters / 2); // then divide by two to find power of damage
    power -= GET_IMPACT(ch) / 2; // subtract 1/2 impact armor
    for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content)
      if (GET_OBJ_VAL(cyber, 0) == CYB_BALANCETAIL)
        power -= 2;
      else if (GET_OBJ_VAL(cyber, 0) == CYB_HYDRAULICJACK) {
        int feettarg = 5;
        feettarg += (int)((meters - 5) / 4);
        if (success_test(GET_QUI(ch), feettarg)) {
          act("$e manages to land on $s feet, an hydraulic woosh coming from $s legs!", FALSE, ch, 0, 0, TO_ROOM);
          send_to_char(ch, "You manage to land on your feet, your hydraulic jacks absorbing some from of the fall!\r\n");
          power -= GET_OBJ_VAL(cyber, 1);
        }
      }
    if (GET_SKILL(ch, SKILL_ATHLETICS))
      power -= success_test(GET_SKILL(ch, SKILL_ATHLETICS), (int)meters);

    int success = success_test(GET_BOD(ch), MAX(2, power) + modify_target(ch));
    int dam = convert_damage(stage(-success, MIN(levels + 1, 4)));
    damage(ch, ch, dam, TYPE_FALL, TRUE);
  }
  return;
}

void move_vehicle(struct char_data *ch, int dir)
{
  struct char_data *tch;
  vnum_t was_in;
  struct veh_data *veh;
  struct veh_follow *v, *nextv;
  extern void crash_test(struct char_data *);

  RIG_VEH(ch, veh);
  if (!veh || veh->damage >= 10)
    return;
  if (ch && !(AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE)) && !veh->dest)
  {
    send_to_char("You're not driving...\r\n", ch);
    return;
  }
  if (veh->cspeed <= SPEED_IDLE)
  {
    send_to_char("You might want to speed up a little.\r\n", ch);
    return;
  }
  if (!EXIT(veh, dir) ||
 EXIT(veh, dir)->to_room == NOWHERE ||
      (!ROOM_FLAGGED(EXIT(veh, dir)->to_room, ROOM_ROAD) && 
!ROOM_FLAGGED(EXIT(veh, dir)->to_room, ROOM_GARAGE) && 
(veh->type != VEH_DRONE && veh->type != VEH_BIKE)) ||
      IS_SET(EXIT(veh, dir)->exit_info, EX_CLOSED) || 
(veh->type == VEH_BIKE && ROOM_FLAGGED(EXIT(veh, dir)->to_room,  ROOM_NOBIKE)) ||
 ROOM_FLAGGED(EXIT(veh, dir)->to_room, ROOM_DEATH) ||
 ROOM_FLAGGED(EXIT(veh, dir)->to_room, ROOM_FALL))
  {
    send_to_char("You cannot go that way...\r\n", ch);
    return;
  }
  char empty_argument = '\0';
  if (special(ch, convert_dir[dir], &empty_argument))
    return;
  if (ROOM_FLAGGED(EXIT(veh, dir)->to_room, ROOM_HOUSE) && !House_can_enter(ch, world[EXIT(veh, dir)->to_room].number)) {
    send_to_char("You can't use other people's garages without permission.\r\n", ch);
    return;
  }

  sprintf(buf2, "%s %s from %s.", GET_VEH_NAME(veh), veh->arrive, thedirs[rev_dir[dir]]);
  sprintf(buf1, "%s %s to %s.", GET_VEH_NAME(veh), veh->leave, thedirs[dir]);
  if (world[veh->in_room].people)
  {
    act(buf1, FALSE, world[veh->in_room].people, 0, 0, TO_ROOM);
    act(buf1, FALSE, world[veh->in_room].people, 0, 0, TO_CHAR);
  }
  if (world[veh->in_room].watching)
    for (struct char_data *tch = world[veh->in_room].watching; tch; tch = tch->next_watching)
      act(buf2, FALSE, ch, 0, 0, TO_CHAR);
  for (int r = 1; r >= 0; r--)
    veh->lastin[r+1] = veh->lastin[r];
  was_in = EXIT(veh, dir)->to_room;
  veh_from_room(veh);
  veh_to_room(veh, was_in);
  veh->lastin[0] = veh->in_room;
  if (world[veh->in_room].people)
  {
    act(buf2, FALSE, world[veh->in_room].people, 0, 0, TO_ROOM);
    act(buf2, FALSE, world[veh->in_room].people, 0, 0, TO_CHAR);
  }
  if (world[veh->in_room].watching)
    for (struct char_data *tch = world[veh->in_room].watching; tch; tch = tch->next_watching)
      act(buf2, FALSE, ch, 0, 0, TO_CHAR);
  stop_fighting(ch);
  for (v = veh->followers; v; v = nextv)
  {
    nextv = v->next;
    bool found = FALSE;
    sh_int r = 0;
    sh_int door = 0;
    struct char_data *pilot;
    if (v->follower->rigger)
      pilot = v->follower->rigger;
    else
      for (pilot = v->follower->people; pilot; pilot = pilot->next_in_veh)
        if (AFF_FLAGGED(ch, AFF_PILOT))
          break;
    if (!pilot)
      return;
    for (; r <= 2; r++)
      if (v->follower->in_room == veh->lastin[r]) {
        found = TRUE;
        break;
      }
    r--;
    if (!found || v->follower->cspeed <= SPEED_IDLE) {
      stop_chase(v->follower);
      sprintf(buf, "You seem to have lost the %s.\r\n", GET_VEH_NAME(veh));
      send_to_char(buf, pilot);
      continue;
    }
    int target = get_speed(v->follower) - get_speed(veh), target2 = 0;
    target = (int)(target / 20);
    target2 = target + veh->handling + modify_veh(veh) + modify_target(ch);
    target = -target + v->follower->handling + modify_veh(v->follower) + modify_target(pilot);
    int success = resisted_test(veh_skill(pilot, v->follower), target, veh_skill(ch, veh), target2);
    if (success > 0) {
      send_to_char(pilot, "You gain ground.\r\n");
      sprintf(buf, "A %s seems to catch up.\r\n", GET_VEH_NAME(v->follower));
      send_to_char(buf, ch);
      for (int x = 0; r >= 0 && x < 2; r-- && x++) {
        for (door = 0; door < NUM_OF_DIRS; door++)
          if (EXIT(v->follower, door))
            if (EXIT(v->follower, door)->to_room == veh->lastin[r])
              break;
        perform_move(pilot, door, LEADER, NULL);
      }
    } else if (success == 0) {
      sprintf(buf, "You chase a %s.\r\n", GET_VEH_NAME(veh));
      send_to_char(buf, pilot);
      sprintf(buf, "A %s maintains it's distance.\r\n", GET_VEH_NAME(v->follower));
      send_to_char(buf, ch);
      for (door = 0; door < NUM_OF_DIRS; door++)
        if (EXIT(v->follower, door))
          if (EXIT(v->follower, door)->to_room == veh->lastin[r])
            break;
      perform_move(pilot, door, LEADER, NULL);
    } else {
      send_to_char(pilot, "You lose ground.\r\n");
      sprintf(buf, "A %s falls behind.\r\n", GET_VEH_NAME(v->follower));
      send_to_char(buf, ch);
    }
    if ((get_speed(v->follower) > 80 && SECT(v->follower->in_room) == SPIRIT_CITY) || world[v->follower->in_room].icesheet[0] || SECT(v->follower->in_room) == SPIRIT_HEARTH) 
    {
      crash_test(pilot);
      chkdmg(v->follower);
    }
  }

  if (PLR_FLAGGED(ch, PLR_REMOTE))
    was_in = ch->in_room;
  ch->in_room = veh->in_room;
  if (!veh->dest)
    look_at_room(ch, 0);
  for (tch = world[veh->in_room].people; tch; tch = tch->next_in_room)
    if (IS_NPC(tch) && AWAKE(tch) && MOB_FLAGGED(tch, MOB_AGGRESSIVE) &&
        !FIGHTING(tch) && !FIGHTING_VEH(tch) && !IS_ASTRAL(tch))
      set_fighting(tch, veh);
  if (PLR_FLAGGED(ch, PLR_REMOTE))
    ch->in_room = was_in;
  else
    ch->in_room = NOWHERE;

  if ((get_speed(veh) > 80 && SECT(veh->in_room) == SPIRIT_CITY) || world[veh->in_room].icesheet[0])
  {
    crash_test(ch);
    chkdmg(veh);
  }
}

int perform_move(struct char_data *ch, int dir, int extra, struct char_data *vict)
{
  int was_in;
  struct follow_type *k, *next;

  if (GET_WATCH(ch)) {
    struct char_data *temp;
    REMOVE_FROM_LIST(ch, world[GET_WATCH(ch)].watching, next_watching);
    GET_WATCH(ch) = 0;
  }
  if (ch == NULL || dir < 0 || dir >= NUM_OF_DIRS)
    return 0;
  else if (ch->in_veh || ch->char_specials.rigging) {
    move_vehicle(ch, dir);
    return 0;
  } else if (GET_POS(ch) < POS_FIGHTING || AFF_FLAGGED(ch, AFF_PRONE)) {
    send_to_char("Maybe you should get on your feet first?\r\n", ch);
    return 0;
  } else if (GET_POS(ch) >= POS_FIGHTING && FIGHTING(ch) && !AFF_FLAGGED(ch, AFF_PRONE)) {
    WAIT_STATE(ch, PULSE_VIOLENCE * 2);
    if (success_test(GET_QUI(ch), GET_QUI(FIGHTING(ch))) && (CAN_GO(ch, dir) && (!IS_NPC(ch) ||
        !ROOM_FLAGGED(world[ch->in_room].dir_option[dir]->to_room, ROOM_NOMOB)))) {
      act("$n searches for a quick escape!", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char("You start moving away for a clever escape.\r\n", ch);
    } else {
      act("$n attempts a retreat, but fails.", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char("PANIC!  You couldn't escape!\r\n", ch);
      return 0;
    }
  }
  if (!EXIT(ch, dir) || EXIT(ch, dir)->to_room == NOWHERE)
  {
    if (!LIGHT_OK(ch))
      send_to_char("Something seems to be in the way...\r\n", ch);
    else
      send_to_char("You cannot go that way...\r\n", ch);
  } else if (IS_SET(EXIT(ch, dir)->exit_info, EX_CLOSED) &&
             !(EXIT(ch, dir)->to_room != 0 && (IS_ASTRAL(ch) ||
                                               GET_REAL_LEVEL(ch) >= LVL_BUILDER)))
  {
    if (!LIGHT_OK(ch))
      send_to_char("Something seems to be in the way...\r\n", ch);
    else if (IS_SET(EXIT(ch, dir)->exit_info, EX_HIDDEN))
      send_to_char("You cannot go that way...\r\n", ch);
    else {
      if (EXIT(ch, dir)->keyword) {
        sprintf(buf2, "The %s seems to be closed.\r\n", fname(EXIT(ch, dir)->keyword));
        send_to_char(buf2, ch);
      } else
        send_to_char("It seems to be closed.\r\n", ch);
    }
  } else
  {
    int total = 0;
    if (GET_TOTALBAL(ch) > GET_QUI(ch))
      total += GET_TOTALBAL(ch) - GET_QUI(ch);
    if (GET_TOTALIMP(ch) > GET_QUI(ch))
      total += GET_TOTALIMP(ch) - GET_QUI(ch);
    if (total >= GET_QUI(ch)) {
      send_to_char("You are wearing too much armour to move!\r\n", ch);
      return 0;
    }
    if (AFF_FLAGGED(ch, AFF_BINDING)) {
      if (success_test(GET_STR(ch), ch->points.binding) > 0) {
        act("$n breaks the bindings at $s feet!", TRUE, ch, 0, 0, TO_ROOM);
        send_to_char("You break through the bindings at your feet!", ch);
        AFF_FLAGS(ch).RemoveBit(AFF_BINDING);
      } else {
        act("$n struggles against the bindings at $s feet, but can't seem to break them.", TRUE, ch, 0, 0, TO_ROOM);
        send_to_char("You struggle against the bindings at your feet but get nowhere!", ch);
        return FALSE;
      }
    }
    if (IS_SET(EXIT(ch, dir)->exit_info, EX_CLOSED)) {
      if (EXIT(ch, dir)->keyword)
        sprintf(buf2, "You pass through the closed %s.\r\n", fname(EXIT(ch, dir)->keyword));
      else
        sprintf(buf2, "You pass through the closed door.\r\n");
      send_to_char(buf2, ch);
    }
    if (!IS_SET(extra, LEADER))
      return (do_simple_move(ch, dir, extra, NULL));

    was_in = ch->in_room;
    if (!do_simple_move(ch, dir, extra | LEADER, vict))
      return 0;

    for (k = ch->followers; k; k = next) {
      next = k->next;
      if ((was_in == k->follower->in_room) && (GET_POS(k->follower) >= POS_STANDING)) {
        act("You follow $N.\r\n", FALSE, k->follower, 0, ch, TO_CHAR);
        perform_move(k->follower, dir, CHECK_SPECIAL, NULL);
      }
    }
    return 1;
  }
  return 0;
}

ACMD(do_move)
{
  /*
   * This is basically a mapping of cmd numbers to perform_move indices.
   * It cannot be done in perform_move because perform_move is called
   * by other functions which do not require the remapping.
   */
  perform_move(ch, subcmd - 1, LEADER, NULL);
}

int find_door(struct char_data *ch, const char *type, char *dir, const char *cmdname)
{
  int door;

  if (*dir)
  {                   /* a direction was specified */
    if ((door = search_block(dir, lookdirs, FALSE)) == -1) {       /* Partial Match */
      send_to_char("That's not a direction.\r\n", ch);
      return -1;
    }
    door = convert_look[door];
    if (EXIT(ch, door)) {
      if (EXIT(ch, door)->keyword) {
        if (isname(type, EXIT(ch, door)->keyword) &&
            !IS_SET(EXIT(ch, door)->exit_info, EX_DESTROYED))
          return door;
        else {
          send_to_char(ch, "I see no %s there.\r\n", type);
          return -1;
        }

      } else
        return door;
    } else {
      send_to_char(ch, "I really don't see how you can %s anything there.\r\n",
                   cmdname);
      return -1;
    }
  } else
  {                      /* try to locate the keyword */
    if (!*type) {
      sprintf(buf2, "What is it you want to %s?\r\n", cmdname);
      send_to_char(buf2, ch);
      return -1;
    }

    for (door = 0; door < NUM_OF_DIRS; door++)
      if (EXIT(ch, door))
        if (EXIT(ch, door)->keyword)
          if (isname(type, EXIT(ch, door)->keyword) &&
              !IS_SET(EXIT(ch, door)->exit_info, EX_DESTROYED))
            return door;

    sprintf(buf2, "There doesn't seem to be %s %s here.\r\n", AN(type), type);
    send_to_char(buf2, ch);
    return -1;
  }
}

int has_key(struct char_data *ch, int key)
{
  struct obj_data *o;

  for (o = ch->carrying; o; o = o->next_content)
    if (GET_OBJ_VNUM(o) == key)
      return 1;

  if (GET_EQ(ch, WEAR_HOLD))
    if (GET_OBJ_VNUM(GET_EQ(ch, WEAR_HOLD)) == key)
      return 1;

  return 0;
}

#define NEED_OPEN       1
#define NEED_CLOSED     2
#define NEED_UNLOCKED   4
#define NEED_LOCKED     8

const char *cmd_door[] = {
                     "open",
                     "close",
                     "unlock",
                     "lock",
                     "bypass",
                     "knock"
                   };

const int flags_door[] =
  {
    NEED_CLOSED | NEED_UNLOCKED,
    NEED_OPEN,
    NEED_CLOSED | NEED_LOCKED,
    NEED_CLOSED | NEED_UNLOCKED,
    NEED_CLOSED,
    NEED_CLOSED
  };

#define EXITN(room, door)               (world[room].dir_option[door])
#define OPEN_DOOR(room, obj, door)      ((obj) ? (TOGGLE_BIT(GET_OBJ_VAL(obj, 1), CONT_CLOSED)) : (TOGGLE_BIT(EXITN(room, door)->exit_info, EX_CLOSED)))
#define LOCK_DOOR(room, obj, door)      ((obj) ? (TOGGLE_BIT(GET_OBJ_VAL(obj, 1), CONT_LOCKED)) : (TOGGLE_BIT(EXITN(room, door)->exit_info, EX_LOCKED)))

#define DOOR_IS_OPENABLE(ch, obj, door) ((obj) ? ((GET_OBJ_TYPE(obj) == ITEM_CONTAINER) && (IS_SET(GET_OBJ_VAL(obj, 1), CONT_CLOSEABLE))) : (IS_SET(EXIT(ch, door)->exit_info, EX_ISDOOR) && !IS_SET(EXIT(ch, door)->exit_info, EX_DESTROYED)))
#define DOOR_IS_OPEN(ch, obj, door)     ((obj) ? (!IS_SET(GET_OBJ_VAL(obj, 1), CONT_CLOSED)) : (!IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED)))
#define DOOR_IS_UNLOCKED(ch, obj, door) ((obj) ? (!IS_SET(GET_OBJ_VAL(obj, 1), CONT_LOCKED)) : (!IS_SET(EXIT(ch, door)->exit_info, EX_LOCKED)))
#define DOOR_IS_PICKPROOF(ch, obj, door) ((obj) ? (IS_SET(GET_OBJ_VAL(obj, 1), CONT_PICKPROOF)) : (IS_SET(EXIT(ch, door)->exit_info, EX_PICKPROOF)))

#define DOOR_IS_CLOSED(ch, obj, door)   (!(DOOR_IS_OPEN(ch, obj, door)))
#define DOOR_IS_LOCKED(ch, obj, door)   (!(DOOR_IS_UNLOCKED(ch, obj, door)))
#define DOOR_KEY(ch, obj, door)         ((obj) ? (GET_OBJ_VAL(obj, 2)) : (EXIT(ch, door)->key))
#define DOOR_LOCK(ch, obj, door)        ((obj) ? (GET_OBJ_VAL(obj, 1)) : (EXIT(ch, door)->exit_info))

void do_doorcmd(struct char_data *ch, struct obj_data *obj, int door, int scmd)
{
  int other_room = 0;
  struct room_direction_data *back = 0;

  sprintf(buf, "$n %ss ", cmd_door[scmd]);
  if (!obj && ((other_room = EXIT(ch, door)->to_room) != NOWHERE))
    if ((back = world[other_room].dir_option[rev_dir[door]]))
      if (back->to_room != ch->in_room)
        back = 0;

  switch (scmd)
  {
  case SCMD_OPEN:
  case SCMD_CLOSE:
    OPEN_DOOR(ch->in_room, obj, door);
    if (back)
      OPEN_DOOR(other_room, obj, rev_dir[door]);
    send_to_char(OK, ch);
    break;
  case SCMD_UNLOCK:
  case SCMD_LOCK:
    LOCK_DOOR(ch->in_room, obj, door);
    if (back)
      LOCK_DOOR(other_room, obj, rev_dir[door]);
    send_to_char("*Click*\r\n", ch);
    break;
  case SCMD_PICK:
    LOCK_DOOR(ch->in_room, obj, door);
    if (back)
      LOCK_DOOR(other_room, obj, rev_dir[door]);
    if (DOOR_IS_UNLOCKED(ch, obj, door)) {
      send_to_char("The lights on the maglock switch from red to green.\r\n", ch);
      strcpy(buf, "The lights on the maglock switch from red to green as $n bypasses ");
    } else {
      send_to_char("The lights on the maglock switch from green to red.\r\n", ch);
      strcpy(buf, "The lights on the maglock switch from green to red as $n bypasses ");
    }
    break;
  case SCMD_KNOCK:
    send_to_char(OK, ch);
    break;
  }

  /* Notify the room */
  sprintf(buf + strlen(buf), "%s%s.", ((obj) ? "" : "the "), (obj) ? "$p" :
          (EXIT(ch, door)->keyword ? "$F" : "door"));
  if (!(obj) || (obj->in_room != NOWHERE))
    act(buf, FALSE, ch, obj, obj ? 0 : EXIT(ch, door)->keyword, TO_ROOM);

  /* Notify the other room */
  if (((scmd == SCMD_OPEN) || (scmd == SCMD_CLOSE) || (scmd == SCMD_KNOCK)) && (back))
  {
    sprintf(buf, "The %s is %s%s from the other side.\r\n",
            (back->keyword ? fname(back->keyword) : "door"), cmd_door[scmd],
            (scmd == SCMD_CLOSE) ? "d" : "ed");
    send_to_room(buf, EXIT(ch, door)->to_room);
  }
}

int ok_pick(struct char_data *ch, int keynum, int pickproof, int scmd, int lock_level)
{
  if (scmd == SCMD_PICK)
  {
    if (keynum <= 0) {
      send_to_char("Odd - you can't seem to find an electronic lock.\r\n", ch);
      return 0;
    } else {
      WAIT_STATE(ch, PULSE_VIOLENCE);
      if (!has_kit(ch, TYPE_ELECTRONIC)) {
        send_to_char("You need an electronic kit to bypass electronic locks.\r\n", ch);
        return 0;
      }
      int skill = get_skill(ch, SKILL_ELECTRONICS, lock_level);
      if (pickproof)
        send_to_char("You can't seem to bypass the electronic lock.\r\n", ch);
      else if (success_test(skill, lock_level) < 1) {
        act("$n attempts to bypass the electronic lock.", TRUE, ch, 0, 0, TO_ROOM);
        send_to_char("You fail to bypass the electronic lock.\r\n", ch);
      } else
        return 1;
      return 0;
    }
  }
  return 1;
}

ACMD_CONST(do_gen_door) {
  char not_const[MAX_STRING_LENGTH];
  strcpy(not_const, argument);
  ACMD(do_gen_door);
  do_gen_door(ch, not_const, cmd, subcmd);
}

ACMD(do_gen_door)
{
  int door = -1, keynum, num = 0;;
  char type[MAX_INPUT_LENGTH], dir[MAX_INPUT_LENGTH];
  struct obj_data *obj = NULL;
  struct char_data *victim = NULL;
  struct veh_data *veh = NULL;
  
  if (IS_ASTRAL(ch)) {
    send_to_char("You are not able to do that.\r\n", ch);
    return;
  }

  if (*arg) {
    if (!LIGHT_OK(ch)) {
      send_to_char("How do you expect to do that when you can't see anything?\r\n", ch);
      return;
    }
    two_arguments(argument, type, dir);
    RIG_VEH(ch, veh);
    if (is_number(type)) {
      num = atoi(type);
      bool has_deck = FALSE;
      for (obj = ch->carrying; obj; obj = obj->next_content)
        if (GET_OBJ_TYPE(obj) == ITEM_RCDECK)
          has_deck = TRUE;
  
      if (!has_deck)
        for (int x = 0; x < NUM_WEARS; x++)
          if (GET_EQ(ch, x))
            if (GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_RCDECK)
              has_deck = TRUE;
      if (has_deck) {
        for (veh = ch->char_specials.subscribe; veh; veh = veh->next_sub)
          if (--num < 0)
            break;
      }
    }
    if (!generic_find(type, FIND_OBJ_EQUIP | FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &victim, &obj) && !veh &&
             !(veh = get_veh_list(type, world[ch->in_room].vehicles, ch)))
      door = find_door(ch, type, dir, cmd_door[subcmd]);
    if (veh && subcmd != SCMD_OPEN && subcmd != SCMD_CLOSE) {
      if (GET_IDNUM(ch) != veh->owner) {
        sprintf(buf, "You don't have the key for %s.\r\n", GET_VEH_NAME(veh));
        send_to_char(buf, ch);
        return;
      }
      if (veh->type == VEH_DRONE) {
        send_to_char("You can't lock that.\r\n", ch);
        return;
      }
      if (subcmd == SCMD_UNLOCK) {
        if (!veh->locked) {
          sprintf(buf, "%s is already unlocked.\r\n", GET_VEH_NAME(veh));
          send_to_char(buf, ch);
          return;
        }
        veh->locked = FALSE;
        sprintf(buf, "You unlock %s.\r\n", GET_VEH_NAME(veh));
        if (veh->type == VEH_BIKE)
          send_to_veh("The ignition clicks.\r\n", veh , NULL, FALSE);
        else
          send_to_veh("The doors click open.\r\n", veh, NULL, FALSE);
        send_to_char(buf, ch);
        return;
      } else if (subcmd == SCMD_LOCK) {
        if (veh->locked) {
          sprintf(buf, "%s is already locked.\r\n", GET_VEH_NAME(veh));
          send_to_char(buf, ch);
          return;
        }
        veh->locked = TRUE;
        sprintf(buf, "You lock %s.\r\n", GET_VEH_NAME(veh));
        if (veh->type == VEH_BIKE)
          send_to_veh("The ignition clicks.\r\n", veh , NULL, FALSE);
        else
          send_to_veh("The doors click locked.\r\n", veh, NULL, FALSE);
        send_to_char(buf, ch);
        return;
      }
    }

    if ((obj) || (door >= 0)) {
      keynum = DOOR_KEY(ch, obj, door);
      if (!(DOOR_IS_OPENABLE(ch, obj, door))) {
        char temp[50];
        sprintf(temp, "You can't %s that!", cmd_door[subcmd]);
        act(temp, FALSE, ch, 0, 0, TO_CHAR);
      }
      else if (!DOOR_IS_OPEN(ch, obj, door) && IS_SET(flags_door[subcmd], NEED_OPEN))
        send_to_char("But it's already closed!\r\n", ch);
      else if (!DOOR_IS_CLOSED(ch, obj, door) && IS_SET(flags_door[subcmd], NEED_CLOSED))
        send_to_char("But it's currently open!\r\n", ch);
      else if (!(DOOR_IS_LOCKED(ch, obj, door)) && IS_SET(flags_door[subcmd], NEED_LOCKED))
        send_to_char("Oh.. it wasn't locked, after all..\r\n", ch);
      else if (!(DOOR_IS_UNLOCKED(ch, obj, door)) && IS_SET(flags_door[subcmd], NEED_UNLOCKED))
        send_to_char("It seems to be locked.\r\n", ch);
      else if (!has_key(ch, keynum) && !access_level(ch, LVL_ADMIN)
               && ((subcmd == SCMD_LOCK) || (subcmd == SCMD_UNLOCK)))
        send_to_char("You don't seem to have the proper key.\r\n", ch);
      else if (ok_pick(ch, keynum, DOOR_IS_PICKPROOF(ch, obj, door), subcmd, LOCK_LEVEL(ch, obj, door)))
        do_doorcmd(ch, obj, door, subcmd);
    }
    return;
  }
  sprintf(buf, "%s what?\r\n", cmd_door[subcmd]);
  send_to_char(CAP(buf), ch);

}

void enter_veh(struct char_data *ch, struct veh_data *found_veh, const char *argument, bool drag)
{
  struct veh_data *inveh = NULL;
  RIG_VEH(ch, inveh);
  bool front = TRUE;
  long door;
  struct follow_type *k, *next;
  if (*argument && is_abbrev(argument, "rear"))
    front = FALSE;
  if (found_veh->damage >= 10)
    send_to_char("It's too damaged to enter.\r\n", ch);
  else if (found_veh->type != VEH_BIKE && found_veh->locked)
    send_to_char("The door is locked.\r\n", ch);
  else if (found_veh->cspeed > SPEED_IDLE)
    send_to_char("It's moving too fast for you to board!\r\n", ch);
  else if (inveh && (AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE))) {
    int mult;
    switch (inveh->type) {
      case VEH_DRONE:
        mult = 100;
        break;
      case VEH_TRUCK:
        mult = 1500;
        break;
      default:
        mult = 500;
        break;
    }
    if (inveh->in_veh)
      send_to_char("You are already inside a vehicle.\r\n", ch);
    else if (found_veh->load - found_veh->usedload < inveh->body * mult)
      send_to_char("There is not enough room in there for that.\r\n", ch);
    else {
      sprintf(buf, "%s drives into the back of %s.", GET_VEH_NAME(inveh), GET_VEH_NAME(found_veh));
      sprintf(buf2, "You drive into the back of %s.\r\n", GET_VEH_NAME(found_veh));
      if (world[inveh->in_room].people)
        act(buf, 0, world[inveh->in_room].people, 0, 0, TO_ROOM);
      send_to_veh(buf2, inveh, NULL, TRUE);
      veh_to_veh(inveh, found_veh);
    }
    return;
  } else if (!found_veh->seating[front])
    send_to_char(ch, "There is no room in the %s of that vehicle.\r\n", front ? "front" : "rear");
  else {
    if (inveh && ch->vfront) {
      send_to_char("You have to be in the back to get into a that.\r\n", ch);
      return;
    }
    door = ch->in_room;
    if (drag)
      sprintf(buf2, "$n is dragged into %s.\r\n", GET_VEH_NAME(found_veh));
    else
      sprintf(buf2, "$n climbs into %s.\r\n", GET_VEH_NAME(found_veh));
    act(buf2, FALSE, ch, 0, 0, TO_ROOM);
    ch->vfront = front;
    char_to_veh(found_veh, ch);
    if (drag)
      act("$n is dragged in.", FALSE, ch, 0, 0, TO_VEH);
    else {
      act("$n climbs in.", FALSE, ch, 0, 0, TO_VEH);
      sprintf(buf2, "You climb into %s.\r\n", GET_VEH_NAME(found_veh));
      send_to_char(buf2, ch);
      GET_POS(ch) = POS_SITTING;
    }
    DELETE_ARRAY_IF_EXTANT(GET_DEFPOS(ch));
    for (k = ch->followers; k; k = next)
    {
      next = k->next;
      if ((door > 0 && door == k->follower->in_room) && (GET_POS(k->follower) >= POS_STANDING)) {
        act("You follow $N.\r\n", FALSE, k->follower, 0, ch, TO_CHAR);
        if (!found_veh->seating[front])
          argument = "rear";
        enter_veh(k->follower, found_veh, argument, FALSE);
      }
    }
  }
}

ACMD(do_drag)
{
  struct char_data *vict = NULL;
  struct veh_data *veh = NULL;
  int dir;

  two_arguments(argument, buf1, buf2);

  if (!*buf1 || !*buf2) {
    send_to_char("Who do you want to drag where?\r\n", ch);
    return;
  }

  if (!(vict = get_char_room_vis(ch, buf1))) {
    send_to_char(NOPERSON, ch);
    return;
  }
  if (AFF_FLAGGED(vict, AFF_BINDING)) {
    act("$n grabs a hold of $N and attempts to help break the bindings!", FALSE, vict, 0, ch, TO_ROOM);
    act("You attempt to break $n's bindings!", FALSE, vict, 0, ch, TO_VICT);
    if (success_test(GET_STR(ch) + (GET_STR(vict) / 2), vict->points.binding) > 0) {
      act("You pull $m free!", FALSE, ch, 0, vict, TO_VICT);
      act("$n pulls $M free, breaking the bindings!", FALSE, ch, 0, vict, TO_ROOM);
      AFF_FLAGS(vict).RemoveBit(AFF_BINDING);
    } else {
      act("You can't seem to pull $m free.", FALSE, ch, 0, vict, TO_VICT);
      act("$n doesn't seem to be able to pull $N free though.", FALSE, ch, 0, vict, TO_ROOM);
    }
    return;
  }
  if ((dir = search_block(buf2, lookdirs, FALSE)) == -1 && !(veh = get_veh_list(buf2, world[ch->in_room].vehicles, ch))) {
    send_to_char("What direction?\r\n", ch);
    return;
  }
  if (!veh)
    dir = convert_look[dir];

  if (GET_POS(vict) > POS_LYING) {
    act("You can't drag $M off in $S condition!", FALSE, ch, 0, vict, TO_CHAR);
    return;
  }

  if ((int)((GET_STR(ch)*10) * 3/2) < (GET_WEIGHT(vict) +
                                       IS_CARRYING_W(vict))) {
    act("$N is too heavy for you to drag!", FALSE, ch, 0, vict, TO_CHAR);
    return;
  }

  if (veh) {
    enter_veh(ch, veh, "rear", FALSE);
    enter_veh(vict, veh, "rear", TRUE);    
  } else {
    perform_move(ch, dir, LEADER, vict);
    char_from_room(vict);
    char_to_room(vict, ch->in_room);
  }
}

ACMD(do_enter)
{
  int door;
  struct veh_data *found_veh = NULL;
  struct obj_data *found_obj = NULL;
  struct char_data *dummy = NULL;

  two_arguments(argument, buf, buf2);
  if (*buf) {                   /* an argument was supplied, search for door keyword */
    generic_find(buf, FIND_OBJ_ROOM, ch, &dummy, &found_obj);
    if (AFF_FLAGGED(ch, AFF_PILOT) && ch->in_veh && ch->in_veh->in_veh) {
      send_to_char("There is not enough room to maneuver in here.\r\n", ch);
      return;
    }
    found_veh = get_veh_list(buf, (ch->in_veh && !AFF_FLAGGED(ch, AFF_PILOT)) ? ch->in_veh->carriedvehs : world[(ch->in_veh ? ch->in_veh->in_room : ch->in_room)].vehicles, ch);

    if (found_veh) {
      enter_veh(ch, found_veh, buf2, FALSE);
      return;
    }

    if (ch->in_room != NOWHERE) {
      for (door = 0; door < NUM_OF_DIRS; door++)
        if (EXIT(ch, door))
          if (EXIT(ch, door)->keyword && !IS_SET(EXIT(ch, door)->exit_info, EX_DESTROYED))
            if (!str_cmp(EXIT(ch, door)->keyword, buf)) {
              perform_move(ch, door, CHECK_SPECIAL | LEADER, NULL);
              return;
            }
    }

    sprintf(buf2, "There is no %s here.\r\n", buf);
    send_to_char(buf2, ch);
  } else if (ROOM_FLAGGED(ch->in_room, ROOM_INDOORS))
    send_to_char("You are already indoors.\r\n", ch);
  else {
    /* try to locate an entrance */
    for (door = 0; door < NUM_OF_DIRS; door++)
      if (EXIT(ch, door))
        if (EXIT(ch, door)->to_room != NOWHERE)
          if (!IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED) &&
              ROOM_FLAGGED(EXIT(ch, door)->to_room, ROOM_INDOORS)) {
            perform_move(ch, door, CHECK_SPECIAL | LEADER, NULL);
            return;
          }
    send_to_char("You can't seem to find anything to enter.\r\n", ch);
  }
}

void leave_veh(struct char_data *ch)
{
  struct follow_type *k, *next;
  struct veh_data *veh;
  long door;
  if (AFF_FLAGGED(ch, AFF_RIG))
  {
    send_to_char(ch, "Try returning to your senses first.\r\n");
    return;
  }
  RIG_VEH(ch, veh);
  if ((AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE)) && veh->in_veh) {
    if (veh->in_veh->in_veh) {
      send_to_char("There is not enough room to drive out of here.\r\n", ch);
      return;
    }
    sprintf(buf, "You drive out of the back of %s.\r\n", GET_VEH_NAME(veh->in_veh));
    sprintf(buf2, "%s drives out of the back.", GET_VEH_NAME(veh));
    send_to_veh(buf, veh, NULL, TRUE);
    send_to_veh(buf2, veh->in_veh, NULL, FALSE);
    sprintf(buf, "%s drives out of the back of %s.", GET_VEH_NAME(veh), GET_VEH_NAME(veh->in_veh));
    int room = veh->in_veh->in_room;
    veh_from_room(veh);
    veh_to_room(veh, room);
    if (world[veh->in_room].people) {
      act(buf, 0, world[veh->in_room].people, 0, 0, TO_ROOM);
      act(buf, 0, world[veh->in_room].people, 0, 0, TO_CHAR);
    }
    return;
  }

  if (veh->cspeed > SPEED_IDLE) {
    send_to_char("That would just be crazy.\r\n", ch);
    return;
  }
  if (AFF_FLAGGED(ch, AFF_PILOT)) {
    act("$n climbs out of the drivers seat and into the street.", FALSE, ch, 0, 0, TO_VEH);
    AFF_FLAGS(ch).ToggleBit(AFF_PILOT);
    veh->cspeed = SPEED_OFF;
    stop_chase(veh);
  } else if (AFF_FLAGGED(ch, AFF_MANNING))
  {
    struct obj_data *mount;
    for (mount = veh->mount; mount; mount = mount->next_content)
      if (mount->worn_by == ch)
        break;
    mount->worn_by = NULL;
    AFF_FLAGS(ch).ToggleBit(AFF_MANNING);
    act("$n stops manning $o and climbs out into the street.", FALSE, ch, mount, 0, TO_ROOM);
  } else
    act("$n climbs out into the street.", FALSE, ch, 0, 0, TO_VEH);
  door = veh->in_room;
  char_from_room(ch);
  AFF_FLAGS(ch).RemoveBit(AFF_MANNING);
  if (IS_WORKING(ch))
  {
    STOP_WORKING(ch);
    send_to_char(ch, "You stop working.\r\n");
  }
  if (veh->in_veh) {
    send_to_char(ch, "You climb out into %s.\r\n", GET_VEH_NAME(veh->in_veh));
    ch->vfront = FALSE;
    char_to_veh(veh->in_veh, ch);
  } else {
    send_to_char("You climb out into the street.\r\n", ch);
    char_to_room(ch, door);
    GET_POS(ch) = POS_STANDING;
  }
  sprintf(buf1, "$n climbs out of %s.", GET_VEH_NAME(veh));
  act(buf1, FALSE, ch, 0, 0, TO_ROOM);
  for (k = ch->followers; k; k = next)
  {
    next = k->next;
    if ((veh == k->follower->in_veh) && (GET_POS(k->follower) >= POS_SITTING)) {
      act("You follow $N.\r\n", FALSE, k->follower, 0, ch, TO_CHAR);
      leave_veh(k->follower);
    }
  }
}
ACMD(do_leave)
{
  int door;
  if (ch->in_veh || (ch->char_specials.rigging && ch->char_specials.rigging->in_veh)) {
    leave_veh(ch);
    return;
  }
  if (GET_POS(ch) < POS_STANDING) {
    send_to_char("Maybe you should get on your feet first?\r\n", ch);
    return;
  }
  if (!ROOM_FLAGGED(ch->in_room, ROOM_INDOORS))
    send_to_char("You are outside.. where do you want to go?\r\n", ch);
  else {
    for (door = 0; door < NUM_OF_DIRS; door++)
      if (EXIT(ch, door))
        if (EXIT(ch, door)->to_room != NOWHERE)
          if (!IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED) &&
              !ROOM_FLAGGED(EXIT(ch, door)->to_room, ROOM_INDOORS)) {
            perform_move(ch, door, CHECK_SPECIAL | LEADER, NULL);
            return;
          }
    send_to_char("I see no obvious exits to the outside.\r\n", ch);
  }
}

ACMD(do_stand)
{
  if (ch->in_veh) {
    send_to_char("You can't stand up in here.\r\n", ch);
    return;
  }
  if (IS_WORKING(ch)) {
    send_to_char(ch, "You stop working.\r\n");
    STOP_WORKING(ch);
  }
  switch (GET_POS(ch)) {
  case POS_STANDING:
    act("You are already standing.", FALSE, ch, 0, 0, TO_CHAR);
    break;
  case POS_SITTING:
    act("You stand up.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n clambers to $s feet.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_STANDING;
    break;
  case POS_RESTING:
  case POS_LYING:
    act("You stop resting, and stand up.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n stops resting, and clambers on $s feet.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_STANDING;
    break;
  case POS_SLEEPING:
    act("You have to wake up first!", FALSE, ch, 0, 0, TO_CHAR);
    break;
  case POS_FIGHTING:
    act("Do you not consider fighting as standing?", FALSE, ch, 0, 0, TO_CHAR);
    break;
  default:
    act("You stop floating around, and put your feet on the ground.",
        FALSE, ch, 0, 0, TO_CHAR);
    act("$n stops floating around, and puts $s feet on the ground.",
        TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_STANDING;
    break;
  }
  DELETE_ARRAY_IF_EXTANT(GET_DEFPOS(ch));
}

ACMD(do_sit)
{
  if (IS_WATER(ch->in_room)) {
    send_to_char("Sit down while swimming?\r\n", ch);
    return;
  }

  switch (GET_POS(ch)) {
  case POS_STANDING:
    act("You sit down.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n sits down.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_SITTING;
    break;
  case POS_SITTING:
    send_to_char("You're sitting already.\r\n", ch);
    break;
  case POS_LYING:
  case POS_RESTING:
    act("You stop resting, and sit up.", FALSE, ch, 0, 0, TO_CHAR);
    if (ch->in_veh) {
      sprintf(buf, "%s stops resting.\r\n", GET_NAME(ch));
      send_to_veh(buf, ch->in_veh, ch, FALSE);
    } else
      act("$n stops resting.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_SITTING;
    break;
  case POS_SLEEPING:
    act("You have to wake up first.", FALSE, ch, 0, 0, TO_CHAR);
    break;
  case POS_FIGHTING:
    act("Sit down while fighting? Are you MAD?", FALSE, ch, 0, 0, TO_CHAR);
    break;
  default:
    act("You stop floating around, and sit down.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n stops floating around, and sits down.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_SITTING;
    break;
  }
  AFF_FLAGS(ch).RemoveBit(AFF_SNEAK);
  DELETE_ARRAY_IF_EXTANT(GET_DEFPOS(ch));
}

ACMD(do_rest)
{
  if (IS_WATER(ch->in_room)) {
    send_to_char("Dry land would be helpful to do that.\r\n", ch);
    return;
  }
  if (IS_WORKING(ch)) {
    send_to_char(ch, "You stop working.\r\n");
    STOP_WORKING(ch);
  }
  switch (GET_POS(ch)) {
  case POS_STANDING:
    act("You sit down and rest your tired bones.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n sits down and rests.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_RESTING;
    break;
  case POS_SITTING:
  case POS_LYING:
    act("You rest your tired bones.", FALSE, ch, 0, 0, TO_CHAR);
    if (ch->in_veh) {
      sprintf(buf, "%s rests.\r\n", GET_NAME(ch));
      send_to_veh(buf, ch->in_veh, ch, FALSE);
    } else
      act("$n rests.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_RESTING;
    break;
  case POS_RESTING:
    act("You are already resting.", FALSE, ch, 0, 0, TO_CHAR);
    break;
  case POS_SLEEPING:
    act("You have to wake up first.", FALSE, ch, 0, 0, TO_CHAR);
    break;
  case POS_FIGHTING:
    act("Rest while fighting?  Are you MAD?", FALSE, ch, 0, 0, TO_CHAR);
    break;
  default:
    act("You stop floating around, and stop to rest your tired bones.",
        FALSE, ch, 0, 0, TO_CHAR);
    act("$n stops floating around, and rests.", FALSE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_SITTING;
    break;
  }
  DELETE_ARRAY_IF_EXTANT(GET_DEFPOS(ch));
}

ACMD(do_lay)
{
  if (IS_WATER(ch->in_room)) {
    send_to_char("Dry land would be helpful to do that.\r\n", ch);
    return;
  }
  if (IS_WORKING(ch)) {
    send_to_char(ch, "You stop working.\r\n");
    STOP_WORKING(ch);
  }
  switch (GET_POS(ch)) {
  case POS_STANDING:
    act("You fall to the ground and stretch out.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n falls to the ground and stretches out.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_LYING;
    break;
  case POS_SITTING:
    act("You lean back until you're flat on the ground.", FALSE, ch, 0, 0, TO_CHAR);
    if (ch->in_veh) {
      sprintf(buf, "%s puts the seat back.\r\n", GET_NAME(ch));
      send_to_veh(buf, ch->in_veh, ch, FALSE);
    } else
      act("$n lays on the ground.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_LYING;
    break;
  case POS_RESTING:
    act("You lay on the ground.", FALSE, ch, 0, 0, TO_CHAR);
    act("$n lays on the ground.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_LYING;
    break;
  case POS_SLEEPING:
    act("You have to wake up first.", FALSE, ch, 0, 0, TO_CHAR);
    break;
  case POS_FIGHTING:
    act("Rest while fighting?  Are you MAD?", FALSE, ch, 0, 0, TO_CHAR);
    break;
  default:
    act("You stop floating around, and stop to lay down.",
        FALSE, ch, 0, 0, TO_CHAR);
    act("$n stops floating around, and lies down.", FALSE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_LYING;
    break;
  }
  DELETE_ARRAY_IF_EXTANT(GET_DEFPOS(ch));
}

ACMD(do_sleep)
{
  if (AFF_FLAGGED(ch, AFF_PILOT)) {
    send_to_char("ARE YOU CRAZY!?\r\n", ch);
    return;
  }
  if (IS_WATER(ch->in_room)) {
    send_to_char("Sleeping while swimming can be hazardous to your health.\r\n", ch);
    return;
  }
  if (IS_WORKING(ch)) {
    send_to_char(ch, "You stop working.\r\n");
    STOP_WORKING(ch);
  }
  switch (GET_POS(ch)) {
  case POS_STANDING:
  case POS_SITTING:
  case POS_RESTING:
  case POS_LYING:
    send_to_char("You go to sleep.\r\n", ch);
    if (ch->in_veh) {
      sprintf(buf, "%s lies down and falls asleep.\r\n", GET_NAME(ch));
      send_to_veh(buf, ch->in_veh, ch, FALSE);
    } else
      act("$n lies down and falls asleep.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_SLEEPING;
    break;
  case POS_SLEEPING:
    send_to_char("You are already sound asleep.\r\n", ch);
    break;
  case POS_FIGHTING:
    send_to_char("Sleep while fighting?  Are you MAD?\r\n", ch);
    break;
  default:
    act("You stop floating around, and lie down to sleep.",
        FALSE, ch, 0, 0, TO_CHAR);
    act("$n stops floating around, and lie down to sleep.",
        TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_SLEEPING;
    break;
  }
  if (GET_POS(ch) == POS_SLEEPING) {
    struct sustain_data *next;
    for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = next) {
      next = sust->next;
      if (sust->caster && !sust->focus && !sust->spirit)
        end_sustained_spell(ch, sust);
    }
  }
  DELETE_ARRAY_IF_EXTANT(GET_DEFPOS(ch));
}

ACMD(do_wake)
{
  struct char_data *vict;
  int self = 0;

  one_argument(argument, arg);
  if (*arg) {
    if (GET_POS(ch) == POS_SLEEPING)
      send_to_char("You can't wake people up if you're asleep yourself!\r\n", ch);
    else if ((vict = get_char_room_vis(ch, arg)) == NULL)
      send_to_char(NOPERSON, ch);
    else if (vict == ch)
      self = 1;
    else if (GET_POS(vict) > POS_SLEEPING)
      act("$E is already awake.", FALSE, ch, 0, vict, TO_CHAR);
    else if (GET_POS(vict) <= POS_STUNNED || GET_PHYSICAL(vict) < 100)
      act("You can't wake $M up!", FALSE, ch, 0, vict, TO_CHAR);
    else {
      act("You wake $M up.", FALSE, ch, 0, vict, TO_CHAR);
      act("You are awakened by $n.", FALSE, ch, 0, vict, TO_VICT | TO_SLEEP);
      GET_POS(vict) = POS_SITTING;
    }
    if (!self)
      return;
  }
  if (GET_POS(ch) > POS_SLEEPING)
    send_to_char("You are already awake...\r\n", ch);
  else {
    send_to_char("You awaken, and sit up.\r\n", ch);
    if (ch->in_veh) {
      sprintf(buf, "%s awakens.\r\n", GET_NAME(ch));
      send_to_veh(buf, ch->in_veh, ch, FALSE);
    } else
      act("$n awakens.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_SITTING;
  }
  DELETE_ARRAY_IF_EXTANT(GET_DEFPOS(ch));
}

ACMD(do_follow)
{
  struct char_data *leader;

  void stop_follower(struct char_data *ch);
  void add_follower(struct char_data *ch, struct char_data *leader);

  one_argument(argument, buf);

  if (!*buf ) {
    if (IS_AFFECTED(ch, AFF_CHARM) && ch->master) {
      act("You can't help but follow $n.", FALSE, ch->master, 0, ch, TO_VICT);
      return;
    }
    if (ch->master) {
      stop_follower(ch);
      AFF_FLAGS(ch).RemoveBit(AFF_GROUP);
    } else
      send_to_char("You are already following yourself.\r\n", ch);
    return;
  }

  if (*buf) {
    if (!(leader = get_char_room_vis(ch, buf))) {
      send_to_char(NOPERSON, ch);
      return;
    }
  } else {
    send_to_char("Whom do you wish to follow?\r\n", ch);
    return;
  }

  if (ch->master == leader) {
    act("You are already following $M.", FALSE, ch, 0, leader, TO_CHAR);
    return;
  }
  if (IS_ASTRAL(leader) && !IS_ASTRAL(ch)) {
    send_to_char("You can't do that.\r\n", ch);
    return;
  }
  if (IS_AFFECTED(ch, AFF_CHARM) && (ch->master)) {
    act("But you only feel like following $N!", FALSE, ch, 0, ch->master, TO_CHAR);
  } else {                      /* Not Charmed follow person */
    if (leader == ch) {
      if (!ch->master) {
        send_to_char("You are already following yourself.\r\n", ch);
        return;
      }
      stop_follower(ch);
      AFF_FLAGS(ch).RemoveBit(AFF_GROUP);
    } else {
      if (circle_follow(ch, leader)) {
        act("Sorry, but following in loops is not allowed.", FALSE, ch, 0, 0, TO_CHAR);
        return;
      }
      if (ch->master)
        stop_follower(ch);
      AFF_FLAGS(ch).RemoveBit(AFF_GROUP);
      add_follower(ch, leader);
    }
  }
}
