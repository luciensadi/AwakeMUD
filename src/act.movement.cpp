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

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "house.hpp"
#include "transport.hpp"
#include "constants.hpp"
#include "newmagic.hpp"
#include "config.hpp"
#include "ignore_system.hpp"
#include "perception_tests.hpp"

/* external functs */
int special(struct char_data * ch, int cmd, char *arg);
void death_cry(struct char_data * ch);
bool perform_fall(struct char_data *);
bool check_fall(struct char_data *, int, bool need_to_send_fall_message);
extern int modify_target(struct char_data *);
extern int convert_damage(int);
extern void check_quest_destination(struct char_data *ch, struct char_data *mob);
extern bool memory(struct char_data *ch, struct char_data *vict);
extern bool is_escortee(struct char_data *mob);
extern bool hunting_escortee(struct char_data *ch, struct char_data *vict);
extern void death_penalty(struct char_data *ch);
extern int get_vehicle_modifier(struct veh_data *veh);
extern int calculate_vehicle_entry_load(struct veh_data *veh);
extern bool passed_flee_success_check(struct char_data *ch);
extern int calculate_swim_successes(struct char_data *ch);
extern void send_mob_aggression_warnings(struct char_data *pc, struct char_data *mob);
extern bool vict_is_valid_aggro_target(struct char_data *ch, struct char_data *vict);

extern sh_int mortal_start_room;
extern sh_int frozen_start_room;
extern vnum_t newbie_start_room;

extern int num_elevators;
extern struct elevator_data *elevator;

ACMD_DECLARE(do_prone);
ACMD_DECLARE(do_look);

#define GET_DOOR_NAME(ch, door) (EXIT(ch, (door))->keyword ? (*(fname(EXIT(ch, (door))->keyword)) ? fname(EXIT(ch, (door))->keyword) : "door") : "door")

/* can_move determines if a character can move in the given direction, and
   generates the appropriate message if not */
int can_move(struct char_data *ch, int dir, int extra)
{
  SPECIAL(escalator);
  int dam;

  char empty_argument = '\0';
  if (IS_SET(extra, CHECK_SPECIAL) && special(ch, convert_dir[dir], &empty_argument))
    return 0;

  if (ch->in_room && ch->in_room->icesheet[0] && !IS_ASTRAL(ch) && !IS_PROJECT(ch) && !IS_AFFECTED(ch, AFF_LEVITATE)) {
    if (FIGHTING(ch) && success_test(GET_QUI(ch), ch->in_room->icesheet[0] + modify_target(ch)) < 1)
    {
      send_to_char("The ice at your feet causes you to trip and fall!\r\n", ch);
      return 0;
    } else {
      send_to_char("You step cautiously across the ice sheet, keeping yourself from falling.\r\n", ch);
    }
  }
  if (IS_AFFECTED(ch, AFF_CHARM) && ch->master && ((ch->in_room && (ch->in_room == ch->master->in_room)) || ((ch->in_veh && ch->in_veh == ch->master->in_veh))))
  {
    send_to_char("The thought of leaving your master makes you weep.\r\n", ch);
    act("$n bursts into tears.", FALSE, ch, 0, 0, TO_ROOM);
    return 0;
  }
  // Builders are restricted to their zone.
  if (builder_cant_go_there(ch, EXIT(ch, dir)->to_room)) {
    send_to_char("Sorry, as a first-level builder you're only able to move to rooms you have edit access for.\r\n", ch);
    return 0;
  }
  if (ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_FREEWAY) && GET_LEVEL(ch) == 1) {
    send_to_char("Walking across the freeway would spell instant death.\r\n", ch);
    return 0;
  }
  if (ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_HOUSE))
    if (!House_can_enter(ch, EXIT(ch, dir)->to_room->number))
    {
      send_to_char("That's private property -- no trespassing!\r\n", ch);
      return 0;
    }
  if (ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_TUNNEL) && !IS_ASTRAL(ch) && !IS_PROJECT(ch)) {
    int num_occupants = 0;
    for (struct char_data *in_room_ptr = EXIT(ch, dir)->to_room->people; in_room_ptr && num_occupants < 2; in_room_ptr = in_room_ptr->next_in_room) {
      if (!IS_ASTRAL(in_room_ptr) && !IS_PROJECT(in_room_ptr) && !access_level(in_room_ptr, LVL_BUILDER))
        num_occupants++;
    }
    if (num_occupants >= 2) {
      if (access_level(ch, LVL_BUILDER)) {
        send_to_char("You use your staff powers to bypass the tunnel restriction.\r\n", ch);
      } else {
        send_to_char("There isn't enough room there for another person!\r\n", ch);
        return 0;
      }
    }
  }
  if (ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_TOO_CRAMPED_FOR_CHARACTERS) && !IS_ASTRAL(ch) && !IS_PROJECT(ch)) {
    if (access_level(ch, LVL_BUILDER)) {
      send_to_char("You use your staff powers to bypass the cramped-space restriction.\r\n", ch);
    } else {
      send_to_char("Try as you might, you can't fit in there!\r\n", ch);
      return 0;
    }
  }

  /*
  if (ch->in_room && ch->in_room->func && ch->in_room->func == escalator)
  {
    if (access_level(ch, LVL_BUILDER)) {
      send_to_char("You use your staff powers to get off the moving escalator.\r\n", ch);
    } else {
      send_to_char("You can't get off a moving escalator!\r\n", ch);
      return 0;
    }
  }
  */

  if (ch->in_room && IS_WATER(ch->in_room) && !IS_NPC(ch) && !IS_SENATOR(ch))
  {
    int swimming_successes = calculate_swim_successes(ch);
    int target = MAX(2, ch->in_room->rating) + modify_target(ch) - swimming_successes;
    int test = success_test(GET_WIL(ch), target);

    if (swimming_successes < 0) {
      dam = convert_damage(stage(-test, SERIOUS));
      send_to_char(ch, "You struggle to retain consciousness as the current resists your every move.\r\n");
      if (dam > 0 && (damage(ch, ch, dam, TYPE_DROWN, FALSE) || GET_POS(ch) < POS_STANDING))
        return 0;
    } else if (!swimming_successes) {
      dam = convert_damage(stage(-test, MODERATE));
      send_to_char(ch, "The current resists your movements.\r\n");
      if (dam > 0 && (damage(ch, ch, dam, TYPE_DROWN, FALSE) || GET_POS(ch) < POS_STANDING))
        return 0;
    } else if (swimming_successes < 3) {
      dam = convert_damage(stage(-test, LIGHT));
      send_to_char(ch, "The current weakly resists your efforts to move.\r\n");
      if (dam > 0 && (damage(ch, ch, dam, TYPE_DROWN, FALSE) || GET_POS(ch) < POS_STANDING))
        return 0;
    }
  }

  return 1;
}

bool should_tch_see_chs_movement_message(struct char_data *tch, struct char_data *ch) {
  if (!tch) {
    mudlog("SYSERR: Received null tch to s_v_o_s_m_m!", tch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (!ch) {
    mudlog("SYSERR: Received null ch to s_v_o_s_m_m!", tch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  // Absolutely can't see for whatever reason.
  if (tch == ch || PRF_FLAGGED(tch, PRF_MOVEGAG) || !AWAKE(tch) || !CAN_SEE(tch, ch) || IS_IGNORING(tch, is_blocking_ic_interaction_from, ch))
    return FALSE;

  // Failed to see from vehicle.
  if (tch->in_veh && (tch->in_veh->cspeed > SPEED_IDLE && success_test(GET_INT(tch), 4) <= 0)) {
    return FALSE;
  }

  // Check for stealth and other person-to-person modifiers.
  if (IS_AFFECTED(ch, AFF_SNEAK)) {
    int dummy_tn = 0;
    char rbuf[1000];
    struct room_data *in_room = get_ch_in_room(ch);

    // Get the skill dice to roll.
    snprintf(rbuf, sizeof(rbuf), "Sneak perception test: %s vs %s. get_skill: ", GET_CHAR_NAME(tch), GET_CHAR_NAME(ch));
    int skill_dice = get_skill(ch, SKILL_STEALTH, dummy_tn);

    // Make an open test to determine the TN for the perception test to notice you.
    strlcat(rbuf, ". get_vision_penalty: ", sizeof(rbuf));
    int open_test_result = open_test(skill_dice);
    int vision_penalty = get_vision_penalty(tch, in_room, rbuf, sizeof(rbuf));
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), ". TN is %d (OT) + %d (vis)", open_test_result, vision_penalty);

    int test_tn = open_test_result + vision_penalty;

    // House rule: Stealth/silence spells add a TN penalty to the spotter, up to 4.
    {
      int stealth_spell_tn = get_spell_affected_successes(ch, SPELL_STEALTH);
      int silence_tn = in_room->silence[ROOM_NUM_SPELLS_OF_TYPE] > 0 ? in_room->silence[ROOM_HIGHEST_SPELL_FORCE] : 0;
      int magic_tn_modifier = MIN(stealth_spell_tn + silence_tn, 4);

      if (magic_tn_modifier > 0) {
        test_tn += magic_tn_modifier;
        snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), " + %d (magic)", magic_tn_modifier);
      }
    }

    // Roll the perception test.
    int perception_result = success_test(GET_INT(tch), test_tn);
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "Result: %d hits.", perception_result);
    act(rbuf, FALSE, ch, 0, 0, TO_ROLLS);

    // If the result met or beat the TN, we're good.
    return perception_result > 0;
  }

  // If we got here, we can see it.
  return TRUE;
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
  // This function is exempt from get_ch_in_room() requirements since it is only ever invoked by a character standing in a room.
  struct char_data *tch;
  struct veh_data *tveh;
  struct room_data *was_in = NULL;
  if (!can_move(ch, dir, extra))
    return 0;

  GET_LASTROOM(ch) = ch->in_room->number;

  if (room_is_a_taxicab(ch->in_room->dir_option[dir]->to_room->number))
    snprintf(buf2, sizeof(buf2), "$n gets into the taxi.");
  else if (vict)
  {
    snprintf(buf1, sizeof(buf1), "$n drag $N %s.", fulldirs[dir]);
    snprintf(buf2, sizeof(buf2), "$n drags %s %s.", GET_NAME(vict), fulldirs[dir]);
    act(buf1, FALSE, ch, 0, vict, TO_CHAR);
  }
  else if (ch->in_room->dir_option[dir]->go_into_thirdperson)
    strcpy(buf2, ch->in_room->dir_option[dir]->go_into_thirdperson);
  else if (IS_WATER(ch->in_room)) {
    if (!IS_WATER(EXIT(ch, dir)->to_room))
      snprintf(buf2, sizeof(buf2), "$n climbs out of the water to the %s.", fulldirs[dir]);
    else
      snprintf(buf2, sizeof(buf2), "$n swims %s.", fulldirs[dir]);
  }
  else if (!IS_WATER(ch->in_room) && IS_WATER(EXIT(ch, dir)->to_room))
    snprintf(buf2, sizeof(buf2), "$n jumps into the water to the %s.", fulldirs[dir]);
  else if (ch->char_specials.leave)
    snprintf(buf2, sizeof(buf2), "$n %s %s.", ch->char_specials.leave, fulldirs[dir]);
  else
    snprintf(buf2, sizeof(buf2), "$n leaves %s.", fulldirs[dir]);

  if (ch->in_room->dir_option[dir]->go_into_secondperson)
    send_to_char(ch, "%s\r\n", ch->in_room->dir_option[dir]->go_into_secondperson);

  // People in the room.
  for (tch = ch->in_room->people; tch; tch = tch->next_in_room) {
    if (should_tch_see_chs_movement_message(tch, ch))
      act(buf2, TRUE, ch, 0, tch, TO_VICT);
  }

  // Vehicle occupants, including riggers.
  for (tveh = ch->in_room->vehicles; tveh; tveh = tveh->next_veh) {
    for (tch = tveh->people; tch; tch = tch->next_in_veh) {
      if (should_tch_see_chs_movement_message(tch, ch))
        act(buf2, TRUE, ch, 0, tch, TO_VICT);
    }

    if (tveh->rigger && should_tch_see_chs_movement_message(tveh->rigger, ch))
      act(buf2, TRUE, ch, 0, tveh->rigger, TO_VICT | TO_REMOTE | TO_SLEEP);
  }

  // Watchers.
  if (ch->in_room->watching) {
    for (struct char_data *tch = ch->in_room->watching; tch; tch = tch->next_watching)
      if (should_tch_see_chs_movement_message(tch, ch))
        act(buf2, TRUE, ch, 0, tch, TO_VICT);
  }

  was_in = ch->in_room;
  STOP_WORKING(ch);
  char_from_room(ch);
  char_to_room(ch, was_in->dir_option[dir]->to_room);

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
    snprintf(buf, sizeof(buf), "You step out into the %s. ", time_of_day[weather_info.sunlight]);
    if (weather_info.sunlight == SUN_DARK && weather_info.sky == SKY_CLOUDLESS)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "You see the %s moon in the cloudless sky.\r\n", moon[weather_info.moonphase]);
    else
      strcat(buf, weather_line[weather_info.sky]);
    send_to_char(buf, ch);
  }
#ifdef USE_SLOUCH_RULES
  if (ROOM_FLAGGED(ch->in_room, ROOM_INDOORS) && GET_HEIGHT(ch) >= ch->in_room->z * 100)
    send_to_char("You have to slouch to fit in here.\r\n", ch);
#endif
  if (ch->desc != NULL)
    look_at_room(ch, 0, 0);

  if (room_is_a_taxicab(was_in->number))
    snprintf(buf2, sizeof(buf2), "$n gets out of the taxi.");
  else if (vict)
    snprintf(buf2, sizeof(buf2), "$n drags %s in from %s.", GET_NAME(vict), thedirs[rev_dir[dir]]);
  else if (ch->in_room->dir_option[rev_dir[dir]] && ch->in_room->dir_option[rev_dir[dir]]->come_out_of_thirdperson)
    strcpy(buf2, ch->in_room->dir_option[rev_dir[dir]]->come_out_of_thirdperson);
  else if (IS_WATER(was_in)) {
    if (!IS_WATER(ch->in_room))
      snprintf(buf2, sizeof(buf2), "$n climbs out of the water from %s.", thedirs[rev_dir[dir]]);
    else
      snprintf(buf2, sizeof(buf2), "$n swims in from %s.", thedirs[rev_dir[dir]]);
  }
  else if (!IS_WATER(was_in) && IS_WATER(ch->in_room))
    snprintf(buf2, sizeof(buf2), "$n jumps into the water from %s.", thedirs[rev_dir[dir]]);
  else if (ch->char_specials.arrive)
    snprintf(buf2, sizeof(buf2), "$n %s %s.", ch->char_specials.arrive, thedirs[rev_dir[dir]]);
  else
    snprintf(buf2, sizeof(buf2), "$n arrives from %s.", thedirs[rev_dir[dir]]);


  // People in the room.
  for (tch = ch->in_room->people; tch; tch = tch->next_in_room) {
    if (should_tch_see_chs_movement_message(tch, ch)) {
      act(buf2, TRUE, ch, 0, tch, TO_VICT);

      if (IS_NPC(tch) && !FIGHTING(tch)) {
        if (hunting_escortee(tch, ch)) {
          set_fighting(tch, ch);
        } else {
          if (AFF_FLAGGED(ch, AFF_SPELLINVIS) || AFF_FLAGGED(ch, AFF_SPELLIMPINVIS)) {
            process_spotted_invis(tch, ch);
          }

          if (vict_is_valid_aggro_target(tch, ch)) {
            stop_fighting(tch);
            send_mob_aggression_warnings(ch, tch);
            set_fighting(tch, ch);
          }
        }
      }
    }
  }

  // Vehicle occupants.
  for (tveh = ch->in_room->vehicles; tveh; tveh = tveh->next_veh) {
    for (tch = tveh->people; tch; tch = tch->next_in_veh) {
      if (should_tch_see_chs_movement_message(tch, ch))
        act(buf2, TRUE, ch, 0, tch, TO_VICT);
    }

    if (tveh->rigger && should_tch_see_chs_movement_message(tveh->rigger, ch))
      act(buf2, TRUE, ch, 0, tveh->rigger, TO_VICT | TO_REMOTE | TO_SLEEP);
  }

  // Watchers.
  if (ch->in_room->watching) {
    for (struct char_data *tch = ch->in_room->watching; tch; tch = tch->next_watching)
      if (should_tch_see_chs_movement_message(tch, ch))
        act(buf2, TRUE, ch, 0, tch, TO_VICT);
  }

#ifdef DEATH_FLAGS
  if (ROOM_FLAGGED(ch->in_room, ROOM_DEATH) && !IS_NPC(ch) && !IS_SENATOR(ch)) {
    send_to_char("You feel the world slip into darkness, you better hope a wandering DocWagon finds you.\r\n", ch);
    death_cry(ch);
    act("$n vanishes into thin air.", FALSE, ch, 0, 0, TO_ROOM);
    death_penalty(ch);
    for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content)
      if (GET_OBJ_VAL(bio, 0) == BIO_PLATELETFACTORY)
        GET_OBJ_VAL(bio, 5) = 24;
      else if (GET_OBJ_VAL(bio, 0) == BIO_ADRENALPUMP) {
        if (GET_OBJ_VAL(bio, 5) > 0)
          for (int i = 0; i < MAX_OBJ_AFFECT; i++)
            affect_modify(ch,
                          bio->affected[i].location,
                          bio->affected[i].modifier,
                          bio->obj_flags.bitvector, FALSE);
        GET_OBJ_VAL(bio, 5) = 0;
      }
    PLR_FLAGS(ch).SetBit(PLR_JUST_DIED);
    if (CH_IN_COMBAT(ch))
      stop_fighting(ch);
    log_death_trap(ch);
    char_from_room(ch);
    char_to_room(ch, real_room(RM_SEATTLE_DOCWAGON));
    extract_char(ch);
   return 1;
  }
#endif

  if (ROOM_FLAGGED(ch->in_room, ROOM_FALL) && !IS_ASTRAL(ch) && !IS_PROJECT(ch) && !IS_AFFECTED(ch, AFF_LEVITATE) && !(IS_SENATOR(ch) && PRF_FLAGGED(ch, PRF_NOHASSLE))) {
    bool character_died;
    // We break the return code paradigm here to avoid having the code check follower data for a dead NPC.
    if (IS_NPC(ch) && (character_died = perform_fall(ch))) {
      return 0;
    }
    return 1;
  }

  if (IS_NPC(ch) && ch->master && !IS_NPC(ch->master) && GET_QUEST(ch->master) && ch->in_room == ch->master->in_room) {
    check_quest_destination(ch->master, ch);
    return 1;
  }

  return 1;
}

// check fall returns TRUE if the player FELL, FALSE if (s)he did not
bool check_fall(struct char_data *ch, int modifier, const char *fall_message)
{
  int base_target = ch->in_room->rating + modify_target(ch);
  int i, autosucc = 0, dice, success;
  char roll_buf[10000];

  snprintf(roll_buf, sizeof(roll_buf), "Computing fall test for %s vs initial TN %d and modifier %d.\r\n", GET_CHAR_NAME(ch), base_target, modifier);

  for (i = WEAR_LIGHT; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_CLIMBING && GET_OBJ_VAL(GET_EQ(ch, i), 1) == CLIMBING_TYPE_JUST_CLIMBING) {
      snprintf(ENDOF(roll_buf), sizeof(roll_buf) - strlen(roll_buf), " - %s lowers TN by %d.\r\n", GET_OBJ_NAME(GET_EQ(ch, i)), GET_OBJ_VAL(GET_EQ(ch, i), 0));
      base_target -= GET_OBJ_VAL(GET_EQ(ch, i), 0);
    }
  for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content)
    if (GET_CYBERWARE_TYPE(cyber) == CYB_BALANCETAIL || GET_CYBERWARE_TYPE(cyber) == CYB_BALANCEAUG) {
      snprintf(ENDOF(roll_buf), sizeof(roll_buf) - strlen(roll_buf), " - %s lowers TN by 2.\r\n", GET_OBJ_NAME(cyber));
      base_target -= 2;
    }

  base_target += modifier;
  base_target = MIN(base_target, 52);
  if (base_target < 2)
  {
    autosucc = 2 - base_target;
    base_target = 2;
  }

  dice = get_skill(ch, SKILL_ATHLETICS, base_target);
  snprintf(ENDOF(roll_buf), sizeof(roll_buf) - strlen(roll_buf), "Athletics check: Rolling %d + %d dice against a final TN of %d results in ", dice, GET_REA(ch), base_target);
  dice += GET_REA(ch);
  success = success_test(dice, base_target) + autosucc;
  snprintf(ENDOF(roll_buf), sizeof(roll_buf) - strlen(roll_buf), "^c%d^n success%s.", success, success == 1 ? "" : "es");
  act(roll_buf, FALSE, ch, 0, 0, TO_ROLLS);

  if (success < 1)
    return TRUE;

  // They've succeeded in stopping themselves, but we still need to send a fall message before the recovery message.
  if (fall_message) {
    send_to_char(fall_message, ch);

    send_to_char("You manage to catch a handhold mid-fall, jerking yourself painfully to a stop.\r\n", ch);
    act("$n scrabbles for a handhold and manages to arrest $s fall!", TRUE, ch, NULL, NULL, TO_ROOM);

    if (!PRF_FLAGGED(ch, PRF_SCREENREADER))
      look_at_room(ch, 0, 0);
  } else {
    send_to_char("You grab on to the wall and keep yourself from falling!\r\n", ch);
    act("$n grabs onto the wall and keeps $mself from falling!", TRUE, ch, NULL, NULL, TO_ROOM);
  }

  return FALSE;
}

bool perform_fall(struct char_data *ch)
{
  int levels = 0;
  bool character_died = FALSE;
  float meters = 0;
  bool sent_fall_message = FALSE;
  const char *fall_message = NULL;
  struct room_data *tmp_room = NULL, *was_in = NULL;

  char impact_noise[50];
  char splat_msg[150];
  char long_fall_message[150];

  snprintf(long_fall_message, sizeof(long_fall_message), "\r\n^RYour fingers slip, sending your stomach rising into your throat as you plummet towards the %s!^n\r\n\r\n",
          ROOM_FLAGGED(ch->in_room, ROOM_ELEVATOR_SHAFT) ? "bottom of the shaft" : (ROOM_FLAGGED(ch->in_room, ROOM_INDOORS) ? "floor" : "ground"));

  // run a loop and drop them through each room
  // known bug: you can fall through closed doors, this is used in some zones for hidden features
  while (EXIT(ch, DOWN) && ROOM_FLAGGED(ch->in_room, ROOM_FALL) && levels < 20)
  {
    // Compose the message that check_fall will send if they succeed.
    if (levels == 0) {
      fall_message = NULL;
    } else if (levels == 1) {
      fall_message = "\r\n^RYou fall!^n\r\n\r\n";
    } else if (levels > 1 && !sent_fall_message) {
      fall_message = long_fall_message;
    }

    // check_fall has them make a test to not fall / stop falling.
    // If they succeed their check, precede their success message with a fall message proportional to the distance they fell.
    // Note that they don't take damage unless they actually hit the ground. This makes elevator shafts a lot less dangerous than originally expected.
    if (!check_fall(ch, levels * 4, fall_message))
      return FALSE;

    levels++;
    meters += ch->in_room->z;
    was_in = ch->in_room;
    snprintf(buf, sizeof(buf), "^R$n %s away from you%s!^n", levels > 1 ? "plummets" : "falls", levels > 1 ? " with a horrified scream" : "");
    act(buf, TRUE, ch, 0, 0, TO_ROOM);
    char_from_room(ch);
    char_to_room(ch, was_in->dir_option[DOWN]->to_room);
    snprintf(buf, sizeof(buf), "^R$n %s in from above!^n", levels > 1 ? "plummets" : "falls");
    act(buf, TRUE, ch, 0, 0, TO_ROOM);
#ifdef DEATH_FLAGS
    if (ROOM_FLAGGED(ch->in_room, ROOM_DEATH) && !IS_NPC(ch) &&
        !IS_SENATOR(ch)) {
      send_to_char("You feel the world slip into darkness, you better hope a wandering DocWagon finds you.\r\n", ch);
      death_cry(ch);
      act("$n vanishes into thin air.", FALSE, ch, 0, 0, TO_ROOM);
      death_penalty(ch);
      for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content)
        if (GET_OBJ_VAL(bio, 0) == BIO_PLATELETFACTORY)
          GET_OBJ_VAL(bio, 5) = 24;
        else if (GET_OBJ_VAL(bio, 0) == BIO_ADRENALPUMP) {
          if (GET_OBJ_VAL(bio, 5) > 0)
            for (int i = 0; i < MAX_OBJ_AFFECT; i++)
              affect_modify(ch,
                            bio->affected[i].location,
                            bio->affected[i].modifier,
                            bio->obj_flags.bitvector, FALSE);
          GET_OBJ_VAL(bio, 5) = 0;
        }
      PLR_FLAGS(ch).SetBit(PLR_JUST_DIED);
      if (CH_IN_COMBAT(ch))
        stop_fighting(ch);
      snprintf(buf, sizeof(buf), "%s ran into DeathTrap at %ld",
              GET_CHAR_NAME(ch), ch->in_room->number);
      mudlog(buf, ch, LOG_DEATHLOG, TRUE);
      char_from_room(ch);
      char_to_room(ch, real_room(RM_SEATTLE_DOCWAGON));

      extract_char(ch);
      return TRUE;
    }
#endif
  }

  if (levels)
  {
    // Make a note of their current room, for use if they hit hard enough to make noise.
    struct room_data *in_room = ch->in_room;
    tmp_room = ch->in_room;

    // Send the fall message if we haven't already.
    if (!sent_fall_message) {
      if (levels == 1) {
        send_to_char("\r\n^RYou fall!^n\r\n\r\n", ch);
      } else {
        send_to_char(long_fall_message, ch);
      }
    }

    if (!PRF_FLAGGED(ch, PRF_SCREENREADER))
      look_at_room(ch, 0, 0);

    if (GET_TRADITION(ch) == TRAD_ADEPT)
      meters -= GET_POWER(ch, ADEPT_FREEFALL) * 2;
    if (meters <= 0) {
      send_to_char(ch, "You manage to land on your feet.\r\n");
      char fall_str[250];
      snprintf(fall_str, sizeof(fall_str), "$e manage%s to land on $s feet.", HSSH_SHOULD_PLURAL(ch) ? "s" : "");
      act(fall_str, FALSE, ch, 0, 0, TO_ROOM);
      return FALSE;
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
          act("$e manages to land on $s feet, a hydraulic whoosh coming from $s legs!", FALSE, ch, 0, 0, TO_ROOM);
          send_to_char(ch, "You manage to land on your feet, your hydraulic jacks absorbing some of the impact!\r\n");
          power -= GET_OBJ_VAL(cyber, 1);
        }
      }
    if (GET_SKILL(ch, SKILL_ATHLETICS))
      power -= success_test(GET_SKILL(ch, SKILL_ATHLETICS), (int)meters);

    int success = success_test(GET_BOD(ch), MAX(2, power) + modify_target(ch));
    int dam = convert_damage(stage(-success, MIN(levels + 1, 4)));

    struct room_data *in_room_at_impact = ch->in_room;
    character_died = damage(ch, ch, dam, TYPE_FALL, TRUE);

    if (character_died) {
      // RIP, they died!
      strcpy(impact_noise, "sickeningly wet ");
    } else {
      if (dam < 2) {
        strcpy(impact_noise, "muted ");
      } else if (dam < 5) {
        strcpy(impact_noise, "");
      } else if (dam < 8) {
        strcpy(impact_noise, "loud ");
      } else {
        strcpy(impact_noise, "crunching ");
      }
    }

    // Message everyone in the fall rooms above, just because flavor is great.
    if (dam > 0) {
      snprintf(splat_msg, sizeof(splat_msg), "^rA %sthud %s from below.^n\r\n", impact_noise, tmp_room->room_flags.IsSet(ROOM_ELEVATOR_SHAFT) ? "echoes" : "emanates");

      while (in_room) {
        if (in_room != in_room_at_impact)
          send_to_room(splat_msg, in_room);

        if (EXIT2(in_room, UP)) {
          in_room = EXIT2(in_room, UP)->to_room;
        } else
          break;
      }
    }
  }
  return character_died;
}

void move_vehicle(struct char_data *ch, int dir)
{
  struct char_data *tch;
  struct room_data *was_in = NULL;
  struct veh_data *veh;
  struct veh_follow *v, *nextv;
  extern void crash_test(struct char_data *);
  char empty_argument = '\0';

  RIG_VEH(ch, veh);
  if (!veh || veh->damage >= VEH_DAM_THRESHOLD_DESTROYED)
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
  if (veh->in_veh || !veh->in_room) {
    send_to_char("You aren't the Kool-Aid Man, so you decide against ramming your way out of here.\r\n", ch);
    return;
  }
  if (!EXIT(veh, dir) || !EXIT(veh, dir)->to_room || EXIT(veh, dir)->to_room == &world[0] || IS_SET(EXIT(veh, dir)->exit_info, EX_CLOSED)) {
    send_to_char(CANNOT_GO_THAT_WAY, ch);
    return;
  }

#ifdef DEATH_FLAGS
  if (ROOM_FLAGGED(EXIT(veh, dir)->to_room, ROOM_DEATH)) {
    send_to_char(CANNOT_GO_THAT_WAY, ch);
    return;
  }
#endif

  // Flying vehicles can traverse any terrain.
  if (!ROOM_FLAGGED(EXIT(veh, dir)->to_room, ROOM_ALL_VEHICLE_ACCESS) && !veh_can_traverse_air(veh)) {
    // Non-flying vehicles can't pass fall rooms.
    if (ROOM_FLAGGED(EXIT(veh, dir)->to_room, ROOM_FALL)) {
      send_to_char(ch, "%s would plunge to its destruction!\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
      return;
    }

    // Check to see if your vehicle can handle the terrain type you're giving it.
    if (IS_WATER(EXIT(veh, dir)->to_room)) {
      if (!veh_can_traverse_water(veh)) {
        send_to_char(ch, "%s would sink!\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
        return;
      }
    } else {
      if (!veh_can_traverse_land(veh)) {
        send_to_char(ch, "You'll have a hard time getting %s on land.\r\n", GET_VEH_NAME(veh));
        return;
      }
    }
  }

  if (special(ch, convert_dir[dir], &empty_argument))
    return;

  if (ROOM_FLAGGED(EXIT(veh, dir)->to_room, ROOM_HOUSE) && !House_can_enter(ch, EXIT(veh, dir)->to_room->number)) {
    send_to_char("You can't use other people's garages without permission.\r\n", ch);
    return;
  }

  if ((!ROOM_FLAGGED(EXIT(veh, dir)->to_room, ROOM_ROAD) && !ROOM_FLAGGED(EXIT(veh, dir)->to_room, ROOM_GARAGE))
      && (veh->type != VEH_DRONE && veh->type != VEH_BIKE))
  {
    send_to_char("That's not an easy path-- only drones and bikes have a chance of making it through.\r\n", ch);
    return;
  }

  if (veh->type == VEH_BIKE && ROOM_FLAGGED(EXIT(veh, dir)->to_room, ROOM_NOBIKE)) {
    send_to_char(CANNOT_GO_THAT_WAY, ch);
    return;
  }

  if (ROOM_FLAGGED(EXIT(veh, dir)->to_room, ROOM_TOO_CRAMPED_FOR_CHARACTERS) && (veh->body > 1 || veh->type != VEH_DRONE)) {
    send_to_char("Your vehicle is too big to fit in there, but a small drone might make it in.\r\n", ch);
    return;
  }

  if (ROOM_FLAGGED(EXIT(veh, dir)->to_room, ROOM_STAFF_ONLY)) {
    for (struct char_data *tch = veh->people; tch; tch = tch->next_in_veh) {
      if (!IS_NPC(tch) && !access_level(tch, LVL_BUILDER)) {
        send_to_char("Everyone in the vehicle must be a member of the game's administration to go there.\r\n", ch);
        return;
      }
    }
  }

  // Sanity check: Did you update the impassibility code without updating this?
  if (!room_accessible_to_vehicle_piloted_by_ch(EXIT(veh, dir)->to_room, veh, ch)) {
    mudlog("SYSERR: room_accessible_to_vehicle() does not match move_vehicle() constraints!", ch, LOG_SYSLOG, TRUE);
    send_to_char(CANNOT_GO_THAT_WAY, ch);
    return;
  }

  snprintf(buf2, sizeof(buf2), "%s %s from %s.", GET_VEH_NAME(veh), veh->arrive, thedirs[rev_dir[dir]]);
  snprintf(buf1, sizeof(buf1), "%s %s to %s.", GET_VEH_NAME(veh), veh->leave, thedirs[dir]);

  /* Known issue: If you are in a vehicle, and nobody is in the room, and another vehicle drives in, you won't see it. */
  if (veh->in_room->people)
  {
    act(buf1, FALSE, veh->in_room->people, 0, 0, TO_ROOM);
    act(buf1, FALSE, veh->in_room->people, 0, 0, TO_CHAR);
  }

  for (struct char_data *tch = veh->in_room->watching; tch; tch = tch->next_watching)
    act(buf2, FALSE, ch, 0, 0, TO_CHAR);
  // for (int r = 1; r >= 0; r--)        <-- Why.
  //  veh->lastin[r+1] = veh->lastin[r];
  veh->lastin[2] = veh->lastin[1];
  veh->lastin[1] = veh->lastin[0];

  was_in = EXIT(veh, dir)->to_room;
  veh_from_room(veh);
  veh_to_room(veh, was_in);
  veh->lastin[0] = veh->in_room;
  if (veh->in_room->people) {
    act(buf2, FALSE, veh->in_room->people, 0, 0, TO_ROOM);
    act(buf2, FALSE, veh->in_room->people, 0, 0, TO_CHAR);
  }
  for (struct char_data *tch = veh->in_room->watching; tch; tch = tch->next_watching)
    act(buf2, FALSE, ch, 0, 0, TO_CHAR);
  stop_fighting(ch);
  for (v = veh->followers; v; v = nextv)
  {
    nextv = v->next;
    bool found = FALSE;
    sh_int r;
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
    for (r = 0; r <= 2; r++)
      if (v->follower->in_room == veh->lastin[r]) {
        found = TRUE;
        break;
      }
    r--;
    if (!found || v->follower->cspeed <= SPEED_IDLE) {
      stop_chase(v->follower);
      send_to_char(pilot, "You seem to have lost the %s.\r\n", GET_VEH_NAME(veh));
      continue;
    }
    int target = get_speed(v->follower) - get_speed(veh), target2 = 0;
    target = (int)(target / 20);
    target2 = target + veh->handling + get_vehicle_modifier(veh) + modify_target(ch);
    target = -target + v->follower->handling + get_vehicle_modifier(v->follower) + modify_target(pilot);

    int follower_dice = veh_skill(pilot, v->follower, &target);
    int lead_dice = veh_skill(ch, veh, &target2);
    int success = resisted_test(follower_dice, target, lead_dice, target2);
    if (success > 0) {
      send_to_char(pilot, "You gain ground.\r\n");
      send_to_char(ch, "%s seems to catch up.\r\n", capitalize(GET_VEH_NAME_NOFORMAT(v->follower)));
      for (int x = 0; r >= 0 && x < 2; r-- && x++) {
        for (door = 0; door < NUM_OF_DIRS; door++)
          if (EXIT(v->follower, door))
            if (EXIT(v->follower, door)->to_room == veh->lastin[r])
              break;
        perform_move(pilot, door, LEADER, NULL);
      }
    } else if (success == 0) {
      send_to_char(pilot, "You chase a %s.\r\n", GET_VEH_NAME(veh));
      send_to_char(ch, "A %s maintains it's distance.\r\n", GET_VEH_NAME(v->follower));
      for (door = 0; door < NUM_OF_DIRS; door++)
        if (EXIT(v->follower, door))
          if (EXIT(v->follower, door)->to_room == veh->lastin[r])
            break;
      perform_move(pilot, door, LEADER, NULL);
    } else {
      send_to_char(pilot, "You lose ground.\r\n");
      send_to_char(ch, "A %s falls behind.\r\n", GET_VEH_NAME(v->follower));
    }
    if ((get_speed(v->follower) > 80 && SECT(v->follower->in_room) == SPIRIT_CITY) || v->follower->in_room->icesheet[0] || SECT(v->follower->in_room) == SPIRIT_HEARTH)
    {
      crash_test(pilot);
      chkdmg(v->follower);
    }
  }

  if (PLR_FLAGGED(ch, PLR_REMOTE))
    was_in = ch->in_room;
  ch->in_room = veh->in_room;
  if (!veh->dest)
    look_at_room(ch, 0, 0);
  for (tch = veh->in_room->people; tch; tch = tch->next_in_room)
    if (IS_NPC(tch) && AWAKE(tch) && MOB_FLAGGED(tch, MOB_AGGRESSIVE) &&
        !CH_IN_COMBAT(tch) && !IS_ASTRAL(tch) && !IS_PROJECT(tch))
      set_fighting(tch, veh);
  if (PLR_FLAGGED(ch, PLR_REMOTE))
    ch->in_room = was_in;
  else
    ch->in_room = NULL;

  if ((get_speed(veh) > 80 && SECT(veh->in_room) == SPIRIT_CITY) || veh->in_room->icesheet[0])
  {
    crash_test(ch);
    chkdmg(veh);
    if (!veh->people)
      load_vehicle_brain(veh);
  }
}

int perform_move(struct char_data *ch, int dir, int extra, struct char_data *vict)
{
  struct room_data *was_in = NULL;
  struct follow_type *k, *next;

  if (GET_WATCH(ch)) {
    struct char_data *temp;
    REMOVE_FROM_LIST(ch, GET_WATCH(ch)->watching, next_watching);
    GET_WATCH(ch) = NULL;
  }
  if (ch == NULL || dir < 0 || dir >= NUM_OF_DIRS)
    return 0;

  if (ch->in_veh || ch->char_specials.rigging) {
    move_vehicle(ch, dir);
    return 0;
  }

  if (GET_POS(ch) < POS_FIGHTING || (AFF_FLAGGED(ch, AFF_PRONE) && !IS_NPC(ch))) {
    send_to_char("Maybe you should get on your feet first?\r\n", ch);
    return 0;
  }

  if (GET_POS(ch) >= POS_FIGHTING && FIGHTING(ch)) {
    WAIT_STATE(ch, PULSE_VIOLENCE * 2);
    if (passed_flee_success_check(ch)
        && (CAN_GO(ch, dir)
            && (!IS_NPC(ch) || !ROOM_FLAGGED(ch->in_room->dir_option[dir]->to_room, ROOM_NOMOB)))) {
      act("$n searches for a quick escape!", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char("You start moving away for a clever escape.\r\n", ch);
    } else {
      act("$n attempts a retreat, but fails.", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char("PANIC! You couldn't escape!\r\n", ch);
      return 0;
    }
  }

  if (!EXIT(ch, dir) || !EXIT(ch, dir)->to_room || EXIT(ch, dir)->to_room == &world[0])
  {
    if (!LIGHT_OK(ch))
      send_to_char("Something seems to be in the way...\r\n", ch);
    else
      send_to_char("You cannot go that way...\r\n", ch);
    return 0;
  }

  if (IS_SET(EXIT(ch, dir)->exit_info, EX_CLOSED) &&
            !(((IS_ASTRAL(ch) || IS_PROJECT(ch)) && !IS_SET(EXIT(ch, dir)->exit_info, EX_ASTRALLY_WARDED)) /* door is astrally passable and char ia astral */
              || GET_REAL_LEVEL(ch) >= LVL_BUILDER)) /* char is staff */
  {
    if (!LIGHT_OK(ch))
      send_to_char("Something seems to be in the way...\r\n", ch);
    else if (IS_SET(EXIT(ch, dir)->exit_info, EX_HIDDEN))
      send_to_char("You cannot go that way...\r\n", ch);
    else {
      if (IS_ASTRAL(ch) || IS_PROJECT(ch)) {
        send_to_char("As you approach, the cobalt flash of an astral barrier warns you back.\r\n", ch);
      } else if (EXIT(ch, dir)->keyword) {
        bool plural = !strcmp(fname(EXIT(ch, dir)->keyword), "doors");
        send_to_char(ch, "The %s seem%s to be closed.\r\n", fname(EXIT(ch, dir)->keyword), plural ? "" : "s");
      } else {
        send_to_char("It seems to be closed.\r\n", ch);
      }
    }
    return 0;
  }

  if (ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_STAFF_ONLY) && GET_REAL_LEVEL(ch) < LVL_BUILDER) {
    send_to_char("Sorry, that area is for game administration only.\r\n", ch);
    return 0;
  }

  if (EXIT(ch, dir)->to_room->staff_level_lock > GET_REAL_LEVEL(ch)) {
    if (GET_REAL_LEVEL(ch) == 0) {
      send_to_char("NPCs aren't allowed in there.\r\n", ch);
    } else if (GET_REAL_LEVEL(ch) == 1) {
      send_to_char("Sorry, that area is for game administration only.\r\n", ch);
    } else {
      send_to_char(ch, "Sorry, you need to be a level-%d immortal to go there.\r\n", EXIT(ch, dir)->to_room->staff_level_lock);
    }
    return 0;
  }

  // Don't let people move past elevator cars.
  if (!IS_ASTRAL(ch) && ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_ELEVATOR_SHAFT)) {
    bool to_room_found = FALSE, in_room_found = FALSE;
    for (int index = 0; !(to_room_found && in_room_found) && index < num_elevators; index++) {
      int car_rating = world[real_room(elevator[index].room)].rating;
      // Check to see if they're leaving a moving elevator car's room.
      if (!in_room_found && elevator[index].floor[car_rating].shaft_vnum == ch->in_room->number) {
        if (elevator[index].is_moving) {
          send_to_char("All you can do is cling to the shaft and hope for the best!\r\n", ch); // giggity
          return 0;
        } else {
          // We found the shaft room and it's okay.
          in_room_found = TRUE;
        }
      }
      // Check to see if they're moving into a moving elevator car's room.
      if (!to_room_found && elevator[index].floor[car_rating].shaft_vnum == EXIT(ch, dir)->to_room->number) {
        // Check for the car being at this floor.
        if (elevator[index].is_moving) {
          send_to_char("Are you crazy? There's a moving elevator car there!\r\n", ch);
          return 0;
        } else {
          // We found the shaft room and it's okay.
          to_room_found = TRUE;
        }
      }
    }
  }

  if (get_armor_penalty_grade(ch) == ARMOR_PENALTY_TOTAL) {
    send_to_char("You are wearing too much armor to move!\r\n", ch);
    return 0;
  }
  if (AFF_FLAGGED(ch, AFF_BINDING)) {
    if (success_test(GET_STR(ch), ch->points.binding) > 0) {
      act("$n breaks the bindings at $s feet!", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char("You break through the bindings at your feet!\r\n", ch);
      AFF_FLAGS(ch).RemoveBit(AFF_BINDING);
    } else {
      act("$n struggles against the bindings at $s feet, but can't seem to break them.", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char("You struggle against the bindings at your feet but get nowhere!\r\n", ch);
      return 0;
    }
  }
  if (IS_SET(EXIT(ch, dir)->exit_info, EX_CLOSED)) {
    if (EXIT(ch, dir)->keyword)
      send_to_char(ch, "You pass through the closed %s.\r\n", fname(EXIT(ch, dir)->keyword));
    else
      send_to_char("You pass through the closed door.\r\n", ch);
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
            !IS_SET(EXIT(ch, door)->exit_info, EX_DESTROYED) &&
            !IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN))
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
      send_to_char(ch, "What is it you want to %s?\r\n", cmdname);
      return -1;
    }

    // Check for directionals.
    char non_const_type[MAX_INPUT_LENGTH];
    strncpy(non_const_type, type, sizeof(non_const_type));
    if ((door = search_block(non_const_type, lookdirs, FALSE)) == -1) {       /* Partial Match */
      // No direction? Check for keywords.
      for (door = 0; door < NUM_OF_DIRS; door++)
        if (EXIT(ch, door))
          if (EXIT(ch, door)->keyword)
            if (isname(type, EXIT(ch, door)->keyword) &&
                !IS_SET(EXIT(ch, door)->exit_info, EX_DESTROYED) &&
                !IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN))
              return door;

      send_to_char(ch, "There doesn't seem to be %s %s here.\r\n", AN(type), type);
      return -1;
    }

    door = convert_look[door];

    if (EXIT(ch, door) && !IS_SET(EXIT(ch, door)->exit_info, EX_DESTROYED) && !IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN)) {
      return door;
    } else {
      send_to_char(ch, "There doesn't appear to be a door to the %s.\r\n", dirs[door]);
      return -1;
    }
  }
}

int has_key(struct char_data *ch, int key_vnum)
{
  struct obj_data *o, *key;

  // Check carried items.
  for (o = ch->carrying; o; o = o->next_content) {
    if (GET_OBJ_VNUM(o) == key_vnum)
      return 1;

    if (GET_OBJ_TYPE(o) == ITEM_KEYRING) {
      for (key = o->contains; key; key = key->next_content) {
        if (GET_OBJ_VNUM(key) == key_vnum)
          return 1;
      }
    }
  }

  // Check worn items.
  for (int x = 0; x < NUM_WEARS; x++) {
    // Must exist.
    if (!GET_EQ(ch, x))
      continue;

    // Direct match?
    if (GET_OBJ_VNUM(GET_EQ(ch, x)) == key_vnum)
      return 1;

    // Keyring match?
    if (GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_KEYRING) {
      for (key = GET_EQ(ch, x)->contains; key; key = key->next_content) {
        if (GET_OBJ_VNUM(key) == key_vnum)
          return 1;
      }
    }
  }

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

#define EXITN(room, door)               ((room)->dir_option[door])
#define OPEN_DOOR(room, obj, door)      ((obj) ? (TOGGLE_BIT(GET_OBJ_VAL(obj, 1), CONT_CLOSED)) : (TOGGLE_BIT(EXITN(room, door)->exit_info, EX_CLOSED)))
#define LOCK_DOOR(room, obj, door)      ((obj) ? (TOGGLE_BIT(GET_OBJ_VAL(obj, 1), CONT_LOCKED)) : (TOGGLE_BIT(EXITN(room, door)->exit_info, EX_LOCKED)))

#define DOOR_IS_OPENABLE(ch, obj, door) ((obj) ? ((GET_OBJ_TYPE(obj) == ITEM_CONTAINER) && (IS_SET(GET_OBJ_VAL(obj, 1), CONT_CLOSEABLE))) : (IS_SET(EXIT(ch, door)->exit_info, EX_ISDOOR) && !IS_SET(EXIT(ch, door)->exit_info, EX_DESTROYED) && !IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN)))
#define DOOR_IS_OPEN(ch, obj, door)     ((obj) ? (!IS_SET(GET_OBJ_VAL(obj, 1), CONT_CLOSED)) : (!IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED)))
#define DOOR_IS_UNLOCKED(ch, obj, door) ((obj) ? (!IS_SET(GET_OBJ_VAL(obj, 1), CONT_LOCKED)) : (!IS_SET(EXIT(ch, door)->exit_info, EX_LOCKED)))
#define DOOR_IS_PICKPROOF(ch, obj, door) ((obj) ? (IS_SET(GET_OBJ_VAL(obj, 1), CONT_PICKPROOF)) : (IS_SET(EXIT(ch, door)->exit_info, EX_PICKPROOF)))

#define DOOR_IS_CLOSED(ch, obj, door)   (!(DOOR_IS_OPEN(ch, obj, door)))
#define DOOR_IS_LOCKED(ch, obj, door)   (!(DOOR_IS_UNLOCKED(ch, obj, door)))
#define DOOR_KEY(ch, obj, door)         ((obj) ? (GET_OBJ_VAL(obj, 2)) : (EXIT(ch, door)->key))
#define DOOR_LOCK(ch, obj, door)        ((obj) ? (GET_OBJ_VAL(obj, 1)) : (EXIT(ch, door)->exit_info))

void do_doorcmd(struct char_data *ch, struct obj_data *obj, int door, int scmd, bool print_message)
{
  struct room_data *other_room = NULL;
  struct room_direction_data *back = NULL;


  snprintf(buf, sizeof(buf), "$n %ss ", cmd_door[scmd]);
  if (!obj && ((other_room = EXIT(ch, door)->to_room)))
    if ((back = other_room->dir_option[rev_dir[door]]))
      if (back->to_room != ch->in_room)
        back = 0;

  // Hidden value goes away if it's a door. Hard to keep it hidden when shit's happening.
  if (door >= 0) {
    REMOVE_BIT(EXITN(ch->in_room, door)->exit_info, EX_HIDDEN);
    if (back)
      REMOVE_BIT(EXITN(other_room, rev_dir[door])->exit_info, EX_HIDDEN);
  }

  switch (scmd)
  {
  case SCMD_OPEN:
    // Forcibly unlock it when we open it.
    if (DOOR_IS_LOCKED(ch, obj, door)) {
      LOCK_DOOR(ch->in_room, obj, door);
      if (back)
        LOCK_DOOR(other_room, obj, rev_dir[door]);
    }
    // fallthrough
  case SCMD_CLOSE:
    OPEN_DOOR(ch->in_room, obj, door);
    if (back)
      OPEN_DOOR(other_room, obj, rev_dir[door]);
    if (print_message) {
      if (obj)
        send_to_char(ch, "You %s %s.\r\n", scmd == SCMD_OPEN ? "open" : "close", GET_OBJ_NAME(obj));
      else
        send_to_char(ch, "You %s the %s to %s.\r\n", scmd == SCMD_OPEN ? "open" : "close", GET_DOOR_NAME(ch, door), thedirs[door]);
    }
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
    if (obj) {
      send_to_char(ch, "You knock experimentally on %s, but nothing happens.\r\n", GET_OBJ_NAME(obj));
      return;
    }
    send_to_char(ch, "You knock on the %s to %s.\r\n", GET_DOOR_NAME(ch, door), thedirs[door]);
    break;
  }

  /* Notify the room */
  if (obj) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "$p.");
  } else {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "the %s to %s.",
            EXIT(ch, door)->keyword ? "$F" : "door",
            thedirs[door]);
  }

  if (!(obj) || (obj->in_room))
    act(buf, FALSE, ch, obj, obj ? 0 : EXIT(ch, door)->keyword, TO_ROOM);

  /* Notify the other room */
  if (((scmd == SCMD_OPEN) || (scmd == SCMD_CLOSE) || (scmd == SCMD_KNOCK)) && (back))
  {
    snprintf(buf, sizeof(buf), "The %s to %s is %s%s from the other side.\r\n",
            (back->keyword ? fname(back->keyword) : "door"),
            thedirs[rev_dir[door]],
            cmd_door[scmd],
            (scmd == SCMD_CLOSE) ? "d" : "ed");
    send_to_room(buf, EXIT(ch, door)->to_room);
  }
}

int ok_pick(struct char_data *ch, int keynum, int pickproof, int scmd, int lock_level)
{
  if (scmd == SCMD_PICK)
  {
    if (keynum <= 0) {
      if (access_level(ch, LVL_BUILDER)) {
        send_to_char(ch, "That door can't be bypassed since its key vnum is %d. Give it a key vnum to make it bypassable.\r\n", keynum);
      } else {
        send_to_char("Odd - you can't seem to find an electronic lock.\r\n", ch);
      }
      return 0;
    } else {
      WAIT_STATE(ch, PULSE_VIOLENCE);

      // Require a kit.
      if (!has_kit(ch, TYPE_ELECTRONIC)) {
        send_to_char("You need an electronic kit to bypass electronic locks.\r\n", ch);
        return 0;
      }

      // Don't allow bypassing pickproof doors.
      if (pickproof) {
        send_to_char("This lock's far too well shielded- it doesn't look like you'll ever be able to bypass it.\r\n", ch);
        return 0;
      }

      int skill = get_skill(ch, SKILL_ELECTRONICS, lock_level);

      if (success_test(skill, lock_level) < 1) {
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
  ACMD_DECLARE(do_gen_door);
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
    send_to_char("Astral projections tend to have a hard time interacting with doors.\r\n", ch);
    return;
  }

  if (!*arg) {
    send_to_char(ch, "%s what?\r\n", capitalize(cmd_door[subcmd]));
    return;
  }
  if (!LIGHT_OK(ch)) {
    send_to_char("How do you expect to do that when you can't see anything?\r\n", ch);
    return;
  }
  two_arguments(argument, type, dir);
  RIG_VEH(ch, veh);

  // 'unlock 1' etc for subscribed vehs
  if (*type && is_number(type)) {
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

  // TODO: Short circuit directions would probably go here.

  // Check for an object or vehicle nearby that matches the keyword.
  if (!generic_find(type, FIND_OBJ_EQUIP | FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &victim, &obj) && !veh &&
           !(veh = get_veh_list(type, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch)))
    door = find_door(ch, type, dir, cmd_door[subcmd]);

  // Lock / unlock a vehicle. Can't open / close them.
  if (veh && subcmd != SCMD_OPEN && subcmd != SCMD_CLOSE) {
    if (GET_IDNUM(ch) != veh->owner) {
      if (access_level(ch, LVL_ADMIN)) {
        send_to_char("You use your staff powers to summon the key.\r\n", ch);
      } else {
        send_to_char(ch, "You don't have the key for %s.\r\n", GET_VEH_NAME(veh));
        return;
      }
    }
    if (subcmd == SCMD_UNLOCK) {
      if (!veh->locked) {
        send_to_char(ch, "%s is already unlocked.\r\n", GET_VEH_NAME(veh));
        return;
      }
      veh->locked = FALSE;
      snprintf(buf, sizeof(buf), "You unlock %s.\r\n", GET_VEH_NAME(veh));
      if (veh->type == VEH_BIKE)
        send_to_veh("The ignition clicks.\r\n", veh, NULL, FALSE);
      else if (veh->type == VEH_DRONE)
        send_to_veh("The drone chirps, acknowledging that it is now unlocked.\r\n", veh, NULL, FALSE);
      else
        send_to_veh("The doors click open.\r\n", veh, NULL, FALSE);
      send_to_char(buf, ch);
      return;
    } else if (subcmd == SCMD_LOCK) {
      if (veh->locked) {
        send_to_char(ch, "%s is already locked.\r\n", GET_VEH_NAME(veh));
        return;
      }
      veh->locked = TRUE;
      snprintf(buf, sizeof(buf), "You lock %s.\r\n", GET_VEH_NAME(veh));
      if (veh->type == VEH_BIKE)
        send_to_veh("The ignition clicks.\r\n", veh , NULL, FALSE);
      else if (veh->type == VEH_DRONE)
        send_to_veh("The drone chirps, acknowledging that it is now locked.\r\n", veh, NULL, FALSE);
      else
        send_to_veh("The doors click locked.\r\n", veh, NULL, FALSE);
      send_to_char(buf, ch);
      return;
    } else {
      snprintf(buf, sizeof(buf), "Unknown subcmd %d received to veh section of do_gen_door.", subcmd);
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      send_to_char("Sorry, that action is not supported.\r\n", ch);
      return;
    }
  }

  // You're doing an object or a door.
  if ((obj) || (door >= 0)) {
    keynum = DOOR_KEY(ch, obj, door);

    // Trying to do some fuckery with a door that doesn't exist?
    if (door >= 0) {
      if (!IS_SET(EXIT(ch, door)->exit_info, EX_ISDOOR)) {
        if (!*dir) {
          send_to_char(ch, "There doesn't seem to be a %s here.\r\n", type);
          return;
        } else {
          send_to_char(ch, "You don't see a %s to %s.\r\n", type, thedirs[door]);
          return;
        }
      }

      if (IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN)) {
        if (!(access_level(ch, LVL_BUILDER) || success_test(GET_INT(ch) + GET_POWER(ch, ADEPT_IMPROVED_PERCEPT), EXIT(ch, door)->hidden))) {
          if (!*dir) {
            send_to_char(ch, "There doesn't seem to be a %s here.\r\n", type);
            return;
          } else {
            send_to_char(ch, "You don't see a %s to %s.\r\n", type, thedirs[door]);
            return;
          }
        }
      }
    }

    // Can't open it? Give a proper message.
    if (!(DOOR_IS_OPENABLE(ch, obj, door))) {
      char temp[50];
      if (obj) {
        snprintf(temp, sizeof(temp), "You can't %s $p.", cmd_door[subcmd]);
      } else {
        snprintf(temp, sizeof(temp), "You can't %s the %s to %s.", cmd_door[subcmd], GET_DOOR_NAME(ch, door), thedirs[door]);
      }
      act(temp, FALSE, ch, obj, 0, TO_CHAR);
      return;
    }

    // Trying to close it when it's already closed.
    if (!DOOR_IS_OPEN(ch, obj, door) && IS_SET(flags_door[subcmd], NEED_OPEN)) {
      if (obj)
        send_to_char(ch, "%s is already closed!\r\n", capitalize(GET_OBJ_NAME(obj)));
      else
        send_to_char(ch, "The %s to %s is already closed!\r\n", GET_DOOR_NAME(ch, door), thedirs[door]);
      return;
    }

    // Almost anything you can do to a door (except close it) fails if it's open.
    if (!DOOR_IS_CLOSED(ch, obj, door) && IS_SET(flags_door[subcmd], NEED_CLOSED)) {
      if (obj)
        send_to_char(ch, "%s is already open!\r\n", capitalize(GET_OBJ_NAME(obj)));
      else
        send_to_char(ch, "But the %s to %s is already open...\r\n", GET_DOOR_NAME(ch, door), thedirs[door]);
      return;
    }

    if (!(DOOR_IS_LOCKED(ch, obj, door)) && IS_SET(flags_door[subcmd], NEED_LOCKED)) {
      if (obj)
        send_to_char(ch, "%s isn't locked.\r\n", capitalize(GET_OBJ_NAME(obj)));
      else
        send_to_char(ch, "The %s to %s isn't locked.\r\n", GET_DOOR_NAME(ch, door), thedirs[door]);
      return;
    }

    if (!(DOOR_IS_UNLOCKED(ch, obj, door)) && IS_SET(flags_door[subcmd], NEED_UNLOCKED)) {
      if (subcmd == SCMD_OPEN) {
        // Trying to open a locked thing without the key.
        if (!(has_key(ch, keynum) || access_level(ch, LVL_ADMIN))) {
          if (obj)
            send_to_char(ch, "You can't open %s-- it's locked and you don't have the proper key.\r\n", GET_OBJ_NAME(obj));
          else
            send_to_char(ch, "You can't open the %s to %s-- it's locked and you don't have the proper key.\r\n", GET_DOOR_NAME(ch, door), thedirs[door]);
          return;
        }
        // They're trying to open a locked thing with the key.
        else {
          if (obj)
            send_to_char(ch, "You unlock and open %s.\r\n", GET_OBJ_NAME(obj));
          else
            send_to_char(ch, "You unlock and open the %s to %s.\r\n", GET_DOOR_NAME(ch, door), thedirs[door]);
          do_doorcmd(ch, obj, door, subcmd, FALSE);
          return;
        }
      } else {
        // They're trying to lock it.
        if (obj)
          send_to_char(ch, "%s is already locked.\r\n", capitalize(GET_OBJ_NAME(obj)));
        else
          send_to_char(ch, "The %s to %s is already locked.\r\n", GET_DOOR_NAME(ch, door), thedirs[door]);
        return;
      }
    }

    if (!has_key(ch, keynum) && !access_level(ch, LVL_ADMIN)
             && ((subcmd == SCMD_LOCK) || (subcmd == SCMD_UNLOCK))) {
      send_to_char("You don't seem to have the proper key.\r\n", ch);
      return;
    }

    if (ok_pick(ch, keynum, DOOR_IS_PICKPROOF(ch, obj, door), subcmd, LOCK_LEVEL(ch, obj, door)))
      do_doorcmd(ch, obj, door, subcmd, TRUE);
  }

  return;
}

void enter_veh(struct char_data *ch, struct veh_data *found_veh, const char *argument, bool drag)
{
  struct veh_data *inveh = NULL;
  RIG_VEH(ch, inveh);
  bool front = TRUE;
  struct room_data *door;
  struct follow_type *k, *next;
  if (*argument && is_abbrev(argument, "rear"))
    front = FALSE;

  // Too damaged? Can't (unless admin).
  if (found_veh->damage >= VEH_DAM_THRESHOLD_DESTROYED) {
    if (access_level(ch, LVL_ADMIN)) {
      send_to_char("You use your staff powers to enter the destroyed vehicle.\r\n", ch);
    } else {
      send_to_char("It's too damaged to enter.\r\n", ch);
      return;
    }
  }

  // Too fast? Can't (unless admin).
  if (found_veh->cspeed > SPEED_IDLE) {
    if (access_level(ch, LVL_ADMIN)) {
      send_to_char("You use your staff powers to match its speed as you board.\r\n", ch);
    } else if (IS_ASTRAL(ch)) {
      send_to_char("You mentally latch on to the speeding vehicle and draw yourself towards it.\r\n", ch);
    }
    else {
      send_to_char("It's moving too fast for you to board!\r\n", ch);
      return;
    }
  }

  // Locked? Can't (unless admin)
  if ((found_veh->type != VEH_BIKE && found_veh->type != VEH_MOTORBOAT) && found_veh->locked) {
    if (access_level(ch, LVL_ADMIN)) {
      send_to_char("You use your staff powers to bypass the locked doors.\r\n", ch);
    } else if (IS_ASTRAL(ch)) {
      // No message-- it just works. Astrals don't care about locks.
    } else {
      send_to_char("It's locked.\r\n", ch);
      return;
    }
  }

  if (inveh && (AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE))) {

    if (inveh->in_veh)
      send_to_char("You are already inside a vehicle.\r\n", ch);
    else if (inveh == found_veh)
      send_to_char("It'll take a smarter mind than yours to figure out how to park your vehicle inside itself.\r\n", ch);
    else if (found_veh->load - found_veh->usedload < calculate_vehicle_entry_load(inveh))
      send_to_char("There is not enough room in there for that.\r\n", ch);
    else {
      strcpy(buf3, GET_VEH_NAME(inveh));
      snprintf(buf, sizeof(buf), "%s drives into the back of %s.", buf3, GET_VEH_NAME(found_veh));
      snprintf(buf2, sizeof(buf2), "You drive into the back of %s.\r\n", GET_VEH_NAME(found_veh));
      if (inveh->in_room->people)
        act(buf, 0, inveh->in_room->people, 0, 0, TO_ROOM);
      send_to_veh(buf2, inveh, NULL, TRUE);
      veh_to_veh(inveh, found_veh);
    }
    return;
  }

  // No space? Can't (unless admin)
  if (!found_veh->seating[front]) {
    if (access_level(ch, LVL_ADMIN)) {
      send_to_char("You use your staff powers to force your way in despite the lack of seating.\r\n", ch);
    } else {
      send_to_char(ch, "There is no room in the %s of that vehicle.\r\n", front ? "front" : "rear");
      return;
    }
  }

  if (inveh && ch->vfront) {
    send_to_char("You have to be in the back to get into that.\r\n", ch);
    return;
  }
  door = ch->in_room;
  if (drag)
    snprintf(buf2, sizeof(buf2), "$n is dragged into %s.\r\n", GET_VEH_NAME(found_veh));
  else if (found_veh->type == VEH_BIKE)
    snprintf(buf2, sizeof(buf2), "$n gets on %s.\r\n", GET_VEH_NAME(found_veh));
  else
    snprintf(buf2, sizeof(buf2), "$n climbs into %s.\r\n", GET_VEH_NAME(found_veh));
  act(buf2, FALSE, ch, 0, 0, TO_ROOM);
  ch->vfront = front;
  char_to_veh(found_veh, ch);
  if (drag)
    act("$n is dragged in.", FALSE, ch, 0, 0, TO_VEH);
  else {
    if (found_veh->type == VEH_BIKE) {
      act("$n gets on.", FALSE, ch, 0, 0, TO_VEH);
      send_to_char(ch, "You get on %s.\r\n", GET_VEH_NAME(found_veh));
      GET_POS(ch) = POS_SITTING;
    } else {
      act("$n climbs in.", FALSE, ch, 0, 0, TO_VEH);
      send_to_char(ch, "You climb into %s.\r\n", GET_VEH_NAME(found_veh));
      GET_POS(ch) = POS_SITTING;
    }
  }
  DELETE_ARRAY_IF_EXTANT(GET_DEFPOS(ch));
  for (k = ch->followers; k; k = next)
  {
    next = k->next;
    if ((door && door == k->follower->in_room) && (GET_POS(k->follower) >= POS_STANDING)) {
      act("You follow $N.\r\n", FALSE, k->follower, 0, ch, TO_CHAR);
      if (!found_veh->seating[front]) {
        strcpy(buf3, "rear");
      } else {
        strcpy(buf3, argument);
      }
      enter_veh(k->follower, found_veh, buf3, FALSE);
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

  if (IS_ASTRAL(ch)) {
    send_to_char("Astral projections aren't really known for their ability to drag things.\r\n", ch);
    return;
  }

  if (!(vict = get_char_room_vis(ch, buf1)) || IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf1);
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
  dir = search_block(buf2, lookdirs, FALSE);
  if (ch->in_veh) {
    dir = -1;
    veh = get_veh_list(buf2, ch->in_veh->carriedvehs, ch);
  } else {
    dir = search_block(buf2, lookdirs, FALSE);
    veh = get_veh_list(buf2, ch->in_room->vehicles, ch);
  }
  if (dir == -1 && !veh) {
    send_to_char("What direction?\r\n", ch);
    return;
  }
  if (!veh)
    dir = convert_look[dir];

  if (IS_NPC(vict) && vict->master != ch) {
    // You may only drag friendly NPCs. This prevents a whole lot of abuse options.
    send_to_char("You may only drag NPCs who are following you already.\r\n", ch);
    return;
  }

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

void try_to_enter_elevator_car(struct char_data *ch) {
  // Iterate through elevators to find one that contains this shaft.
  for (int index = 0; index < num_elevators; index++) {
    int car_rating = world[real_room(elevator[index].room)].rating;
    // Check for the car being at this floor.
    if (elevator[index].floor[car_rating].shaft_vnum == ch->in_room->number) {
      if (IS_ASTRAL(ch)) {
        send_to_char(ch, "You phase into the %selevator car.\r\n", elevator[index].is_moving ? "moving " : "");
        char_from_room(ch);
        char_to_room(ch, &world[real_room(elevator[index].room)]);
        act("$n phases in through the wall.\r\n", TRUE, ch, NULL, NULL, TO_ROOM);
      } else {
        if (elevator[index].is_moving) {
          send_to_char("You can't enter a moving elevator car!\r\n", ch);
          return;
        }

        send_to_char("You jimmy open the access hatch and drop into the elevator car. The hatch locks closed behind you.\r\n", ch);
        char_from_room(ch);
        char_to_room(ch, &world[real_room(elevator[index].room)]);
        act("The access hatch in the ceiling squeaks briefly open and $n drops into the car.", FALSE, ch, NULL, NULL, TO_ROOM);
      }
      return;
    }
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

    struct veh_data *veh;
    RIG_VEH(ch, veh);
    if (veh) {
      found_veh = get_veh_list(buf, veh->in_veh ? veh->in_veh->carriedvehs : veh->in_room->vehicles, ch);
    } else {
      if (ch->in_veh && !AFF_FLAGGED(ch, AFF_PILOT)) {
        // We are in a vehicle-in-a-room, but not driving it. Look for vehicles to enter.
        found_veh = get_veh_list(buf, ch->in_veh->carriedvehs, ch);
      } else {
        // We are either in a vehicle-in-a-room and are driving it, or are not in a vehicle. Either way, look in the room.
        found_veh = get_veh_list(buf, get_ch_in_room(ch)->vehicles, ch);
      }
    }

    if (found_veh) {
      enter_veh(ch, found_veh, buf2, FALSE);
      return;
    }

    for (door = 0; door < NUM_OF_DIRS; door++)
      if (EXIT(ch, door))
        if (EXIT(ch, door)->keyword && !IS_SET(EXIT(ch, door)->exit_info, EX_DESTROYED) && !IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN))
          if (!str_cmp(EXIT(ch, door)->keyword, buf)) {
            perform_move(ch, door, CHECK_SPECIAL | LEADER, NULL);
            return;
          }

    if (ROOM_FLAGGED(ch->in_room, ROOM_ELEVATOR_SHAFT) && (!str_cmp("elevator", buf) || !str_cmp("car", buf))) {
      try_to_enter_elevator_car(ch);
    } else {
      send_to_char(ch, "There is no '%s' here.\r\n", buf);
    }
    return;
  }

  // Is there an elevator car here?
  if (ROOM_FLAGGED(ch->in_room, ROOM_ELEVATOR_SHAFT)) {
    try_to_enter_elevator_car(ch);
    return;
  }

  if (ROOM_FLAGGED(get_ch_in_room(ch), ROOM_INDOORS)) {
    send_to_char("You are already indoors.\r\n", ch);
    return;
  }

  /* try to locate an entrance */
  for (door = 0; door < NUM_OF_DIRS; door++)
    if (EXIT(ch, door))
      if (EXIT(ch, door)->to_room)
        if (!IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED) &&
            ROOM_FLAGGED(EXIT(ch, door)->to_room, ROOM_INDOORS)) {
          perform_move(ch, door, CHECK_SPECIAL | LEADER, NULL);
          return;
        }
  send_to_char("You can't seem to find anything to enter.\r\n", ch);
}

void leave_veh(struct char_data *ch)
{
  struct follow_type *k, *next;
  struct veh_data *veh = NULL;
  struct obj_data *mount = NULL;
  struct room_data *door = NULL;

  if (AFF_FLAGGED(ch, AFF_RIG)) {
    send_to_char(ch, "Try returning to your senses first.\r\n");
    return;
  }

  RIG_VEH(ch, veh);

  if ((AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE)) && veh->in_veh) {
    if (veh->in_veh->in_veh) {
      send_to_char("There is not enough room to drive out of here.\r\n", ch);
      return;
    }
    snprintf(buf, sizeof(buf), "You drive out of the back of %s.\r\n", GET_VEH_NAME(veh->in_veh));
    snprintf(buf2, sizeof(buf2), "%s drives out of the back.", GET_VEH_NAME(veh));
    send_to_veh(buf, veh, NULL, TRUE);
    send_to_veh(buf2, veh->in_veh, NULL, FALSE);
    strcpy(buf3, GET_VEH_NAME(veh));
    snprintf(buf, sizeof(buf), "%s drives out of the back of %s.", buf3, GET_VEH_NAME(veh->in_veh));
    // get_veh_in_room not needed here since the if-check guarantees that veh->in_veh->in_room is valid.
    struct room_data *room = veh->in_veh->in_room;
    veh_to_room(veh, room);
    if (veh->in_room->people) {
      act(buf, 0, veh->in_room->people, 0, 0, TO_ROOM);
      act(buf, 0, veh->in_room->people, 0, 0, TO_CHAR);
    }
    return;
  }

  if (veh->cspeed > SPEED_IDLE) {
    if (access_level(ch, LVL_ADMIN)) {
      send_to_char("You use your staff powers to exit the moving vehicle safely.\r\n", ch);
    } else {
      send_to_char("Your vehicle's moving! That would just be crazy.\r\n", ch);
      return;
    }
  }

  if (AFF_FLAGGED(ch, AFF_PILOT)) {
    act("$n climbs out of the drivers seat and into the street.", FALSE, ch, 0, 0, TO_VEH);
    AFF_FLAGS(ch).ToggleBit(AFF_PILOT);
    veh->cspeed = SPEED_OFF;
    stop_chase(veh);
  } else if ((mount = stop_manning_weapon_mounts(ch, FALSE))) {
    act("$n stops manning $p and climbs out into the street.", FALSE, ch, mount, 0, TO_ROOM);
  } else
    act("$n climbs out into the street.", FALSE, ch, 0, 0, TO_VEH);

  door = get_veh_in_room(veh);
  char_from_room(ch);
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
  snprintf(buf1, sizeof(buf1), "$n climbs out of %s.", GET_VEH_NAME(veh));
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

  struct room_data *in_room = get_ch_in_room(ch);
  if (!in_room) {
    send_to_char("Panic strikes you-- you're floating in a void!\r\n", ch);
    mudlog("SYSERR: Char has no in_room!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  // Leaving an elevator shaft is handled in the button panel's spec proc code. See transport.cpp.
  if (!ROOM_FLAGGED(in_room, ROOM_INDOORS)) {
    send_to_char("You are outside.. where do you want to go?\r\n", ch);
    return;
  }

  // If you're in an apartment, you're able to leave to the atriun no matter what. Prevents lockin.
  if (ROOM_FLAGGED(in_room, ROOM_HOUSE)) {
    for (door = 0; door < NUM_OF_DIRS; door++) {
      if (EXIT(ch, door) && EXIT(ch, door)->to_room && ROOM_FLAGGED(EXIT(ch, door)->to_room, ROOM_ATRIUM)) {
        send_to_char(ch, "You make your way out of the residence through the door to %s, leaving it locked behind you.\r\n", thedirs[door]);
        act("$n leaves the residence.", TRUE, ch, 0, 0, TO_ROOM);

        // Transfer the char.
        struct room_data *target_room = EXIT(ch, door)->to_room;
        char_from_room(ch);
        char_to_room(ch, target_room);

        // Message the room.
        snprintf(buf, sizeof(buf), "$n enters from %s.", thedirs[door]);
        act(buf, TRUE, ch, 0, 0, TO_ROOM);

        // If not screenreader, look.
        if (!PRF_FLAGGED(ch, PRF_SCREENREADER))
          look_at_room(ch, 0, 0);
        return;
      }
    }
    // If we got here, there was no valid exit. Error condition.
    snprintf(buf, sizeof(buf), "WARNING: %s attempted to leave apartment, but there was no valid exit! Rescuing to Dante's.",
             GET_CHAR_NAME(ch));

    struct room_data *room = &world[real_room(RM_DANTES_GARAGE)];
    if (room) {
      act("$n leaves the residence.", TRUE, ch, 0, 0, TO_ROOM);
      char_from_room(ch);
      char_to_room(ch, room);
      act("$n steps out of the shadows, looking slightly confused.", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char("(System message: Something went wrong with getting you out of the apartment. Staff has been notified, and you have been relocated to Dante's Inferno.)\r\n", ch);
      return;
    } else {
      snprintf(buf, sizeof(buf), "^RERROR:^g %s attempted to leave apartment, but there was no valid exit, and Dante's was unreachable! They're FUCKED.",
               GET_CHAR_NAME(ch));
    }
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
  }

  // If you're in a PGHQ, you teleport to the first room of the PGHQ's zone.
  if (in_room->zone >= 0 && zone_table[in_room->zone].is_pghq) {
    rnum_t entrance_rnum = real_room(zone_table[in_room->zone].number * 100);
    if (entrance_rnum) {
      send_to_char("You glance around to get your bearings, then head for the entrance.\r\n\r\n", ch);
      char_from_room(ch);
      char_to_room(ch, &world[entrance_rnum]);
    } else {
      mudlog("SYSERR: Could not get player to PGHQ entrance, does it not exist?", ch, LOG_SYSLOG, TRUE);
      send_to_char("The walls feel like they're closing in on you, and you panic! A few minutes of breathless flight later, you find yourself somewhere else...\r\n\r\n", ch);
      char_from_room(ch);
      char_to_room(ch, &world[real_room(RM_ENTRANCE_TO_DANTES)]);
    }
    strlcpy(buf, "", sizeof(buf));
    do_look(ch, buf, 0, 0);
    return;
  }

  // Standard leave from indoors to outdoors.
  for (door = 0; door < NUM_OF_DIRS; door++)
    if (EXIT(ch, door) && EXIT(ch, door)->to_room) {
      if (!IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED) && !ROOM_FLAGGED(EXIT(ch, door)->to_room, ROOM_INDOORS)) {
        perform_move(ch, door, CHECK_SPECIAL | LEADER, NULL);
        return;
      }
    }

  send_to_char("You see no obvious exits to the outside.\r\n", ch);
}

ACMD(do_stand)
{
  if (ch->in_veh) {
    if (GET_POS(ch) < POS_SITTING && GET_POS(ch) > POS_SLEEPING) {
      ACMD_DECLARE(do_sit);
      do_sit(ch, argument, 0, 0);
    } else {
      send_to_char("You can't stand up in here.\r\n", ch);
    }
    return;
  }
  if (IS_WORKING(ch)) {
    send_to_char(ch, "You stop working.\r\n");
    STOP_WORKING(ch);
  }
  if (AFF_FLAGGED(ch, AFF_PRONE)) {
    do_prone(ch, argument, 0, 0);
    return;
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
  if (ch->in_room && IS_WATER(ch->in_room)) {
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
      snprintf(buf, sizeof(buf), "%s stops resting.\r\n", GET_NAME(ch));
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
  if (ch->in_room && IS_WATER(ch->in_room)) {
    send_to_char("Dry land would be helpful to do that.\r\n", ch);
    return;
  }
  if (AFF_FLAGGED(ch, AFF_PILOT)) {
    send_to_char("Trying to rest while driving? You ARE mad!\r\n", ch);
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
      snprintf(buf, sizeof(buf), "%s rests.\r\n", GET_NAME(ch));
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
  if (ch->in_room && IS_WATER(ch->in_room)) {
    send_to_char("Dry land would be helpful to do that.\r\n", ch);
    return;
  }
  if (AFF_FLAGGED(ch, AFF_PILOT)) {
    send_to_char("Trying to lay down while driving? You ARE mad!\r\n", ch);
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
      snprintf(buf, sizeof(buf), "%s puts the seat back.\r\n", GET_NAME(ch));
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
    send_to_char("Sleeping while driving? ARE YOU CRAZY!?\r\n", ch);
    return;
  }
  if (ch->in_room && IS_WATER(ch->in_room)) {
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
      snprintf(buf, sizeof(buf), "%s lies down and falls asleep.\r\n", GET_NAME(ch));
      send_to_veh(buf, ch->in_veh, ch, FALSE);
    } else
      act("$n lies down and falls asleep.", TRUE, ch, 0, 0, TO_ROOM);
    GET_POS(ch) = POS_SLEEPING;
    break;
  case POS_SLEEPING:
    send_to_char("You are already sound asleep.\r\n", ch);
    break;
  case POS_FIGHTING:
    send_to_char("Sleep while fighting? Are you MAD?\r\n", ch);
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
      send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
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
      snprintf(buf, sizeof(buf), "%s awakens.\r\n", GET_NAME(ch));
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
      send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf);
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

      if (IS_IGNORING(leader, is_blocking_following_from, ch)) {
        send_to_char("You can't do that.\r\n", ch);
        return;
      }

      if (ch->master)
        stop_follower(ch);
      AFF_FLAGS(ch).RemoveBit(AFF_GROUP);
      add_follower(ch, leader);
    }
  }
}

ACMD(do_stuck) {
  skip_spaces(&argument);

  const char *reason = one_argument(argument, arg);

  if (!*arg || !is_abbrev("confirm", arg)) {
    send_to_char("You've invoked the ^WSTUCK^n command, which is designed to teleport you to safety if"
                 " you end up trapped somewhere in the game. Note that usage of this command is"
                 " logged and reviewed, so using it in any circumstances other than being legitimately"
                 " trapped is not advisable. If you're sure you want to continue, type ^WSTUCK CONFIRM"
                 " <reason for being trapped>^n (ex: stuck confirm the key despawned).\r\n", ch);
    return;
  }

  if (!*reason) {
    send_to_char("You must specify a reason for being stuck. Ex: ^WSTUCK CONFIRM the screwdriver key despawned while I was in here^n.\r\n", ch);
    return;
  }

  if (!ch->in_room && !ch->in_veh) {
    send_to_char(ch, "Wow, you really ARE stuck, aren't you?\r\n");
    snprintf(buf, sizeof(buf), "STUCK COMMAND USED: %s ended up with no vehicle or room! Rationale given is '%s'. Will teleport to Mortal Relations.",
             GET_CHAR_NAME(ch),
             reason
           );
  } else {
    struct room_data *room = get_ch_in_room(ch);
    snprintf(buf, sizeof(buf), "STUCK COMMAND USED: %s claims to be stuck in %s'%s'^g (%ld). Rationale given is '%s'. Will teleport to Mortal Relations.",
             GET_CHAR_NAME(ch),
             ch->in_veh ? "a vehicle in " : "",
             GET_ROOM_NAME(room),
             GET_ROOM_VNUM(room),
             reason
           );
  }

  mudlog(buf, ch, LOG_CHEATLOG, TRUE);

  rnum_t mortal_relations_rnum = real_room(RM_MORTAL_RELATIONS_CONFERENCE_ROOM);

  if (mortal_relations_rnum == NOWHERE) {
    mudlog("SYSERR: Mortal Relations conference room not defined! You must build it before the stuck command can be used.", ch, LOG_SYSLOG, TRUE);
    return;
  }

  if (ch->in_room || ch->in_veh)
    char_from_room(ch);
  char_to_room(ch, &world[mortal_relations_rnum]);
  send_to_char(ch, "Thanks for your error report! You have been teleported to the Mortal Relations Conference Room, and may leave after the ten-second delay passes.\r\n");
  WAIT_STATE(ch, 10 RL_SEC);
}
