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
#include "invis_resistance_tests.hpp"
#include "newhouse.hpp"
#include "quest.hpp"
#include "zoomies.hpp"
#include "elevators.hpp"

/* external functs */
int special(struct char_data * ch, int cmd, char *arg);
void death_cry(struct char_data * ch, idnum_t cause_of_death_idnum);
bool perform_fall(struct char_data *);
bool check_fall(struct char_data *, int, bool need_to_send_fall_message);
extern int modify_target(struct char_data *);
extern int convert_damage(int);
extern void check_quest_destination(struct char_data *ch, struct char_data *mob);
extern bool memory(struct char_data *ch, struct char_data *vict);
extern bool is_escortee(struct char_data *mob);
extern bool hunting_escortee(struct char_data *ch, struct char_data *vict);
extern void death_penalty(struct char_data *ch);
extern int get_vehicle_modifier(struct veh_data *veh, bool include_weather=TRUE);
extern int calculate_vehicle_entry_load(struct veh_data *veh);
extern int calculate_swim_successes(struct char_data *ch);
extern void send_mob_aggression_warnings(struct char_data *pc, struct char_data *mob);
extern bool vict_is_valid_aggro_target(struct char_data *ch, struct char_data *vict);
extern struct char_data *find_a_character_that_blocks_fleeing_for_ch(struct char_data *ch);
extern bool precipitation_is_snow(int jurisdiction);
extern int calculate_vision_penalty(struct char_data *ch, struct char_data *victim);
extern bool can_hurt(struct char_data *ch, struct char_data *victim, int attacktype, bool include_func_protections);
extern bool npc_vs_vehicle_blocked_by_quest_protection(idnum_t quest_id, struct veh_data *veh);

extern sh_int mortal_start_room;
extern sh_int frozen_start_room;
extern vnum_t newbie_start_room;

extern int num_elevators;
extern struct elevator_data *elevator;

ACMD_DECLARE(do_prone);
ACMD_DECLARE(do_look);

void set_sneaking_wait_state(struct char_data *ch);

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

  if (ch->in_room && ch->in_room->icesheet[0] && !IS_ASTRAL(ch) && !IS_AFFECTED(ch, AFF_LEVITATE)) {
    if (FIGHTING(ch) && success_test(GET_QUI(ch), ch->in_room->icesheet[0] + modify_target(ch)) < 1)
    {
      send_to_char("The ice at your feet causes you to trip and fall!\r\n", ch);
      return 0;
    } else {
      send_to_char("You step cautiously across the ice sheet, keeping yourself from falling.\r\n", ch);
    }
  }

  // Prevent PCs from doing wonky things.
  if (ch->desc) {
    // Builders are restricted to their zone.
    if (builder_cant_go_there(ch, EXIT(ch, dir)->to_room)) {
      send_to_char("Sorry, as a first-level builder you're only able to move to rooms you have edit access for.\r\n", ch);
      return 0;
    }
    // Everyone is restricted from edit-locked zones that aren't connected.
    FALSE_CASE(!ch_can_bypass_edit_lock(ch, EXIT(ch, dir)->to_room), "Sorry, that zone is locked.");
  }

  if (ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_FREEWAY) && !IS_PROJECT(ch)) {
    if (GET_LEVEL(ch) > 1) {
      send_to_char("You abuse your staff powers to part traffic around you.\r\n", ch);
    } else if (IS_NPNPC(ch) && !ch->desc) {
      send_to_char("You abuse your NPC powers to part traffic around you.\r\n", ch);
    } else {
      bool has_vehicle_to_go_to = FALSE;

      for (struct veh_data *veh = EXIT(ch, dir)->to_room->vehicles; veh; veh = veh->next_veh) {
        if (veh->owner == GET_IDNUM(ch)) {
          send_to_char(ch, "You hotfoot towards %s through the speeding traffic, playing the world's scariest game of Frogger.\r\n", GET_VEH_NAME(veh));
          has_vehicle_to_go_to = TRUE;
          break;
        }
      }

      if (!has_vehicle_to_go_to) {
        send_to_char("Walking across the freeway would spell instant death.\r\n", ch);
        return 0;
      }
    }
  }

  if (!CH_CAN_ENTER_APARTMENT(EXIT(ch, dir)->to_room, ch)) {
    send_to_char("That's private property -- no trespassing!\r\n", ch);
    return 0;
  }
  
  if (ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_TUNNEL) && !IS_ASTRAL(ch)) {
    int num_occupants = 0;
    for (struct char_data *in_room_ptr = EXIT(ch, dir)->to_room->people; in_room_ptr && num_occupants < 2; in_room_ptr = in_room_ptr->next_in_room) {
      if (!IS_ASTRAL(in_room_ptr) && !access_level(in_room_ptr, LVL_BUILDER) && GET_POS(in_room_ptr) > POS_STUNNED)
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

  if (ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_TOO_CRAMPED_FOR_CHARACTERS) && !IS_ASTRAL(ch)) {
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

/* We expect to see movement messages under the following conditions:

*/
bool should_tch_see_chs_movement_message(struct char_data *viewer, struct char_data *actor, bool should_roll_perception, bool is_arriving) {
  if (!viewer) {
    mudlog("SYSERR: Received null viewer to s_t_s_c_m_m!", viewer, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (!actor) {
    mudlog("SYSERR: Received null actor to s_t_s_c_m_m!", viewer, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  // You can't see your own movement messages, and can't see any if you're not awake.
  if (viewer == actor || !AWAKE(viewer)) {
    return FALSE;
  }

  // Viewer is not allowed to see this movement (staff fiat)
  if (IS_SENATOR(actor) && !access_level(viewer, GET_INVIS_LEV(actor))) {
    return FALSE;
  }

  // Viewer has codedly prevented themselves from seeing this movement.
  if (PRF_FLAGGED(viewer, PRF_MOVEGAG) || IS_IGNORING(viewer, is_blocking_ic_interaction_from, actor)) {
    return FALSE;
  }

  // Actor is astral and not manifest and viewer can't see astral.
  if (IS_ASTRAL(actor) && !AFF_FLAGGED(actor, AFF_MANIFEST) && !SEES_ASTRAL(viewer)) {
    return FALSE;
  }

  // If both viewer and actor are NPCs, then we don't care.
  // Note: player projections are flagged as NPCs.
  if (IS_NPC(viewer) && IS_NPC(actor) && !IS_PROJECT(viewer) && !IS_PROJECT(actor)) {
    return FALSE;
  }

  // Tuned out doing other things.
  if (PLR_FLAGGED(viewer, PLR_REMOTE) || PLR_FLAGGED(viewer, PLR_MATRIX)) {
    return FALSE;
  }

  // Failed to see from moving vehicle.
  if (viewer->in_veh && (viewer->in_veh->cspeed > SPEED_IDLE && success_test(GET_INT(viewer), 4) <= 0)) {
    return FALSE;
  }

  // Check for stealth and other person-to-person modifiers.
  if (should_roll_perception && (IS_AFFECTED(actor, AFF_SNEAK) || !CAN_SEE(viewer, actor))) {
    int open_test_result = 0;
    char rbuf[1000] = { '\0' };
    struct room_data *in_room = get_ch_in_room(actor);

    // If you're sneaking, make an open test to determine the TN for the perception test to notice you.
    if (IS_AFFECTED(actor, AFF_SNEAK)) {
      int dummy_tn = 2, original_dummy_tn = 2;

      // Get the skill dice to roll.
      snprintf(rbuf, sizeof(rbuf), "Sneak perception test: %s vs %s. get_skill: ", GET_CHAR_NAME(viewer), GET_CHAR_NAME(actor));
      int skill_dice = get_skill(actor, SKILL_STEALTH, dummy_tn);

      // Make the roll.
      open_test_result = open_test(skill_dice);

      // If we've defaulted, this is reflected by an increased TN-- if the TN went up, cap successes.
      // Only apply to PCs, because builders haven't been setting NPC stealth skills and we don't want to rewrite the world.
      int defaulting_amount = (dummy_tn - original_dummy_tn);
      if (defaulting_amount > 0 && (!IS_NPC(viewer) || IS_PROJECT(viewer))) {
        if (defaulting_amount <= 2) {
          // +2 means we defaulted to a linked skill. Mild penalty.
          open_test_result = MIN(open_test_result - 2, 10);
          strlcat(rbuf, "(defaulted: OT -2, cap 10!)", sizeof(rbuf));
        } else {
          // +4 means we defaulted to an attribute. Harsh penalty.
          open_test_result = MIN(open_test_result - 4, 6);
          strlcat(rbuf, "(defaulted: OT -4, cap 6!)", sizeof(rbuf));
        }
      }
    } else {
      snprintf(rbuf, sizeof(rbuf), "Non-sneak perception test: %s vs %s", GET_CHAR_NAME(viewer), GET_CHAR_NAME(actor));
    }

    // Don't allow negative results.
    open_test_result = MAX(open_test_result, 0);

    // Calculate vision TN penalties.
    int vis_tn_modifier = calculate_vision_penalty(viewer, actor);

    // Calculate other TN penalties (wounds, lighting, etc), mitigated by existing vision TN penalty.
    int phys_tn_modifier = modify_target_rbuf_raw(viewer, rbuf, sizeof(rbuf), vis_tn_modifier, FALSE);

    int stealth_and_silence_tn_modifier = 0;
    {
      // House rule: Stealth/silence spells add a TN penalty to the spotter, up to 4.
      int stealth_spell_tn = get_spell_affected_successes(actor, SPELL_STEALTH);
      int silence_tn = in_room->silence[ROOM_NUM_SPELLS_OF_TYPE] > 0 ? in_room->silence[ROOM_HIGHEST_SPELL_FORCE] : 0;
      stealth_and_silence_tn_modifier = MIN(stealth_spell_tn + silence_tn, 4);
    }

    // Spirit Conceal power adds to the TN.
    int spirit_conceal_tn_modifier = affected_by_power(actor, CONCEAL);

    // Calculate TN.
    int test_tn = open_test_result + vis_tn_modifier + phys_tn_modifier + stealth_and_silence_tn_modifier + spirit_conceal_tn_modifier;

    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), ". TN is ^C%d^n (^c%d^n OT + ^c%d^n vision + ^c%d^n other phys + ^c%d^n stealth/silence + ^c%d^n conceal). ",
             test_tn,
             open_test_result,
             vis_tn_modifier,
             phys_tn_modifier,
             stealth_and_silence_tn_modifier,
             spirit_conceal_tn_modifier);

    // Roll the perception test.
    int perception_result = success_test(GET_INT(viewer) + GET_POWER(viewer, ADEPT_IMPROVED_PERCEPT), test_tn);
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "Result: ^C%d^n hit%s.\r\n", perception_result, perception_result == 1 ? "" : "s");

    // Print the message, but only to staff, and only if it's not about a higher-level staff.
    for (struct char_data *staff = get_ch_in_room(viewer)->people; staff; staff = staff->next_in_room) {
      if (IS_SENATOR(staff) && PRF_FLAGGED(staff, PRF_ROLLS) && (!IS_SENATOR(actor) || access_level(staff, GET_INVIS_LEV(actor)))) {
        send_to_char(rbuf, staff);
      }
    }

    bool spotted_movement = perception_result > 0;

    // Failed to notice them.
    if (!spotted_movement) {
      return FALSE;
    }

    // They were sneaking or invisible but we noticed them enter.
    if ((IS_AFFECTED(actor, AFF_SNEAK) || !CAN_SEE(viewer, actor))) {
      // Spotting someone sneaking around puts NPCs on edge.
      if (IS_NPC(viewer) && !IS_PROJECT(viewer)) {
        // Extend their alert time and ensure they're at least Alert.
        extend_mob_alarm_time(viewer, actor, 10);
        
        // Send messages, but only for the room you're walking into.
        if (is_arriving) {
          if (CAN_SEE(viewer, actor)) {
            act("You notice $N sneaking around.", FALSE, viewer, 0, actor, TO_CHAR);
            if (can_hurt(actor, viewer, 0, TRUE)) {
              act("^T$n^T scowls at you-- your sneaking around has not gone unnoticed.^n", TRUE, viewer, 0, actor, TO_VICT);
              act("$n scowls at $N-- $s sneaking around has not gone unnoticed.", TRUE, viewer, 0, actor, TO_NOTVICT);
            }
          } else {
            act("You startle and look around-- you could have sworn you heard something.", FALSE, viewer, 0, 0, TO_CHAR);
            if (can_hurt(actor, viewer, 0, TRUE)) {
              act("$n startles and looks in your general direction.", TRUE, viewer, 0, actor, TO_VICT);
              act("$n startles and looks in $N's general direction.", TRUE, viewer, 0, actor, TO_NOTVICT);
            }
          }
        }
      }
    }
  }

  // If we got here, we noticed them.
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

  // Sneaking characters incur a slight delay, mitigated by their quickness and athletics.
  if (AFF_FLAGGED(ch, AFF_SNEAK)) {
    set_sneaking_wait_state(ch);
  }

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
    strlcpy(buf2, ch->in_room->dir_option[dir]->go_into_thirdperson, sizeof(buf2));
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
    if (should_tch_see_chs_movement_message(tch, ch, TRUE, FALSE))
      act(buf2, TRUE, ch, 0, tch, TO_VICT);
  }

  // Vehicle occupants, including riggers.
  for (tveh = ch->in_room->vehicles; tveh; tveh = tveh->next_veh) {
    for (tch = tveh->people; tch; tch = tch->next_in_veh) {
      if (should_tch_see_chs_movement_message(tch, ch, TRUE, FALSE))
        act(buf2, TRUE, ch, 0, tch, TO_VICT);
    }

    if (tveh->rigger && should_tch_see_chs_movement_message(tveh->rigger, ch, TRUE, FALSE))
      act(buf2, TRUE, ch, 0, tveh->rigger, TO_VICT | TO_REMOTE | TO_SLEEP);
  }

  // Watchers.
  for (struct char_data *tch = ch->in_room->watching; tch; tch = tch->next_watching) {
    if (should_tch_see_chs_movement_message(tch, ch, TRUE, FALSE)) {
      send_to_char("From a distance, you see:\r\n", tch);
      act(buf2, TRUE, ch, 0, tch, TO_VICT);
    }
  }

  was_in = ch->in_room;
  STOP_WORKING(ch);
  char_from_room(ch);
  char_to_room(ch, was_in->dir_option[dir]->to_room);

  if (ROOM_FLAGGED(was_in, ROOM_INDOORS) && !ROOM_FLAGGED(ch->in_room, ROOM_INDOORS))
  {
    bool should_be_snowy = precipitation_is_snow(GET_JURISDICTION(ch->in_room));

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
    const char *snow_line[] = {
                                "The sky is clear.\r\n",
                                "Clouds fill the sky.\r\n",
                                "Snow drifts down around you.\r\n",
                                "Heavy snow falls about you.\r\n"
                              };
    snprintf(buf, sizeof(buf), "You step out into the %s%s. ", should_be_snowy ? "cool " : "", time_of_day[weather_info.sunlight]);

    if (!PRF_FLAGGED(ch, PRF_NO_WEATHER)) {
      if (weather_info.sunlight == SUN_DARK && weather_info.sky == SKY_CLOUDLESS && GET_JURISDICTION(ch->in_room) != JURISDICTION_SECRET)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "You see the %s moon in the cloudless sky.\r\n", moon[weather_info.moonphase]);
      else {
        if (should_be_snowy) {
          strlcat(buf, snow_line[weather_info.sky], sizeof(buf));
        } else {
          strlcat(buf, weather_line[weather_info.sky], sizeof(buf));
        }
      }
    }
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
    strlcpy(buf2, ch->in_room->dir_option[rev_dir[dir]]->come_out_of_thirdperson, sizeof(buf2));
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

  // Clear their position string, if any.
  DELETE_AND_NULL_ARRAY(GET_DEFPOS(ch));

  // People in the room.
  for (tch = ch->in_room->people; tch; tch = tch->next_in_room) {
    if (should_tch_see_chs_movement_message(tch, ch, TRUE, TRUE)) {
      act(buf2, TRUE, ch, 0, tch, TO_VICT);

      if (IS_NPNPC(tch) && !FIGHTING(tch)) {
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

    // Additionally, process these actions no matter if ch was seen or not.
    {
      if (tch->master == ch && is_escortee(tch)) {
        act("$N begins following you.", FALSE, ch, 0, tch, TO_CHAR);
        act("$N begins following $n.", FALSE, ch, 0, tch, TO_ROOM);
        act("You begin following $n", FALSE, ch, 0, tch, TO_VICT);
      }
    }
  }

  // Vehicle occupants.
  for (tveh = ch->in_room->vehicles; tveh; tveh = tveh->next_veh) {
    for (tch = tveh->people; tch; tch = tch->next_in_veh) {
      if (should_tch_see_chs_movement_message(tch, ch, TRUE, TRUE))
        act(buf2, TRUE, ch, 0, tch, TO_VICT);
    }

    if (tveh->rigger && should_tch_see_chs_movement_message(tveh->rigger, ch, TRUE, TRUE))
      act(buf2, TRUE, ch, 0, tveh->rigger, TO_VICT | TO_REMOTE | TO_SLEEP);
  }

  // Watchers.
  for (struct char_data *tch = ch->in_room->watching; tch; tch = tch->next_watching) {
    if (should_tch_see_chs_movement_message(tch, ch, TRUE, TRUE)) {
      send_to_char("From a distance, you see:\r\n", tch);
      act(buf2, TRUE, ch, 0, tch, TO_VICT);
    }
  }

#ifdef DEATH_FLAGS
  if (ROOM_FLAGGED(ch->in_room, ROOM_DEATH) && !IS_NPC(ch) && !IS_SENATOR(ch)) {
    send_to_char("You feel the world slip into darkness, you better hope a wandering DocWagon finds you.\r\n", ch);
    death_cry(ch, 0);
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
    PLR_FLAGS(ch).RemoveBit(PLR_DOCWAGON_READY);
    if (CH_IN_COMBAT(ch))
      stop_fighting(ch);
    log_death_trap(ch);
    char_from_room(ch);
    char_to_room(ch, real_room(RM_SEATTLE_DOCWAGON));
    extract_char(ch);
   return 1;
  }
#endif

  if (ROOM_FLAGGED(ch->in_room, ROOM_FALL) && !IS_ASTRAL(ch) && !IS_AFFECTED(ch, AFF_LEVITATE) && !(IS_SENATOR(ch) && PRF_FLAGGED(ch, PRF_NOHASSLE))) {
    bool character_died;
    // We break the return code paradigm here to avoid having the code check follower data for a dead NPC.
    if (IS_NPC(ch) && (character_died = perform_fall(ch))) {
      return 0;
    }
    return 1;
  }

  if (IS_NPC(ch) && ch->master && ch->in_room == ch->master->in_room && COULD_BE_ON_QUEST(ch->master)) {
    check_quest_destination(ch->master, ch);
  }

  return 1;
}

// check fall returns TRUE if the player FELL, FALSE if (s)he did not
bool check_fall(struct char_data *ch, int modifier, const char *fall_message)
{
  int base_target = ch->in_room->rating + modify_target(ch);
  int i, autosucc = 0, dice, success;
  char roll_buf[10000];

  if (!AWAKE(ch)) {
    act("$n can't pass fall test: Not conscious.", FALSE, ch, 0, 0, TO_ROLLS);
    return FALSE;
  }

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
  snprintf(ENDOF(roll_buf), sizeof(roll_buf) - strlen(roll_buf), "Athletics check: Rolling %d dice against a final TN of %d results in ", dice, base_target);
  // dice += GET_REA(ch);
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
      death_cry(ch, 0);
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

    for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content) {
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
    }

    if (GET_SKILL(ch, SKILL_ATHLETICS))
      power -= success_test(GET_SKILL(ch, SKILL_ATHLETICS), (int)meters);

    int success = success_test(GET_BOD(ch), MAX(2, power) + modify_target(ch));
    int dam = convert_damage(stage(-success, MIN(levels + 1, 4)));

    struct room_data *in_room_at_impact = ch->in_room;
    character_died = damage(ch, ch, dam, TYPE_FALL, TRUE);

    if (character_died) {
      // RIP, they died!
      strlcpy(impact_noise, "sickeningly wet ", sizeof(impact_noise));
    } else {
      if (dam < 2) {
        strlcpy(impact_noise, "muted ", sizeof(impact_noise));
      } else if (dam < 5) {
        strlcpy(impact_noise, "", sizeof(impact_noise));
      } else if (dam < 8) {
        strlcpy(impact_noise, "loud ", sizeof(impact_noise));
      } else {
        strlcpy(impact_noise, "crunching ", sizeof(impact_noise));
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
  struct room_data *was_in = NULL;
  struct veh_data *veh;
  struct veh_follow *v, *nextv;
  extern void crash_test(struct char_data *, bool no_driver);
  char empty_argument = '\0';

  RIG_VEH(ch, veh);
  if (!veh || veh->damage >= VEH_DAM_THRESHOLD_DESTROYED)
    return;
  
  FAILURE_CASE(ch && !(AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE)) && !veh->dest, "You're not driving...");
  FAILURE_CASE(veh->cspeed <= SPEED_IDLE, "You might want to speed up a little.");
  FAILURE_CASE(veh->in_veh || !veh->in_room, "You aren't the Kool-Aid Man, so you decide against ramming your way out of here.");
  FAILURE_CASE(!EXIT(veh, dir) || !EXIT(veh, dir)->to_room || EXIT(veh, dir)->to_room == &world[0], "You can't go that way.");
  FAILURE_CASE(veh_is_currently_flying(veh), "Deviating from your flight plan could have nasty consequences, so you decide to stay the course.");

  struct room_data *room = EXIT(veh, dir)->to_room;

  if (IS_SET(EXIT(veh, dir)->exit_info, EX_CLOSED)) {
    if (GET_APARTMENT(EXIT(veh, dir)->to_room) || GET_APARTMENT(veh->in_room)) {
      FAILURE_CASE(IS_SET(EXIT(veh, dir)->exit_info, EX_LOCKED) && !has_key(ch, (EXIT(veh, dir)->key)), "You need the key in your inventory to use the garage door opener.");
      FAILURE_CASE(!CH_CAN_ENTER_APARTMENT(EXIT(veh, dir)->to_room, ch), "That's private property-- no trespassing.");

      send_to_char("The remote on your key beeps, and the door swings open just enough to let you through.\r\n", ch);
      snprintf(buf, sizeof(buf), "A door beeps before briefly opening just enough to allow %s through.\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
      send_to_room(buf, get_veh_in_room(veh), veh);
    } else {
      send_to_char(CANNOT_GO_THAT_WAY, ch);
      return;
    }
  }

  // Error messages presumably sent in-function.
  if (special(ch, convert_dir[dir], &empty_argument))
    return;

  // Error messages sent in-function.
  if (!room_accessible_to_vehicle_piloted_by_ch(room, veh, ch, TRUE))
    return;

  // Clear their position string, if any.
  DELETE_AND_NULL_ARRAY(GET_VEH_DEFPOS(veh));

  snprintf(buf2, sizeof(buf2), "%s %s from %s.\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)), veh->arrive, thedirs[rev_dir[dir]]);
  snprintf(buf1, sizeof(buf1), "%s %s to %s.\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)), veh->leave, thedirs[dir]);

  // People in the room.
  for (struct char_data *tch = veh->in_room->people; tch; tch = tch->next_in_room) {
    if (should_tch_see_chs_movement_message(tch, ch, FALSE, FALSE))
      act(buf1, TRUE, ch, 0, tch, TO_VICT);
  }

  // Vehicle occupants, including riggers.
  for (struct veh_data *tveh = veh->in_room->vehicles; tveh; tveh = tveh->next_veh) {
    if (tveh == veh)
      continue;
    
    for (struct char_data *tch = tveh->people; tch; tch = tch->next_in_veh) {
      if (should_tch_see_chs_movement_message(tch, ch, FALSE, FALSE))
        act(buf1, TRUE, ch, 0, tch, TO_VICT);
    }

    if (tveh->rigger && should_tch_see_chs_movement_message(tveh->rigger, ch, FALSE, FALSE))
      act(buf1, TRUE, ch, 0, tveh->rigger, TO_VICT | TO_REMOTE | TO_SLEEP);
  }

  // Watchers.
  for (struct char_data *tch = veh->in_room->watching; tch; tch = tch->next_watching) {
    if (should_tch_see_chs_movement_message(tch, ch, FALSE, FALSE)) {
      send_to_char("From a distance, you see:\r\n", tch);
      act(buf1, TRUE, ch, 0, tch, TO_VICT);
    }
  }

  // for (int r = 1; r >= 0; r--)        <-- Why.
  //  veh->lastin[r+1] = veh->lastin[r];
  veh->lastin[2] = veh->lastin[1];
  veh->lastin[1] = veh->lastin[0];

  was_in = EXIT(veh, dir)->to_room;
  veh_from_room(veh);
  veh_to_room(veh, was_in);
  veh->lastin[0] = veh->in_room;

  // People in the room.
  for (struct char_data *tch = veh->in_room->people; tch; tch = tch->next_in_room) {
    if (should_tch_see_chs_movement_message(tch, ch, FALSE, TRUE))
      act(buf2, TRUE, ch, 0, tch, TO_VICT);
  }

  // Vehicle occupants, including riggers.
  for (struct veh_data *tveh = veh->in_room->vehicles; tveh; tveh = tveh->next_veh) {
    if (tveh == veh)
      continue;
    
    for (struct char_data *tch = tveh->people; tch; tch = tch->next_in_veh) {
      if (should_tch_see_chs_movement_message(tch, ch, FALSE, TRUE))
        act(buf2, TRUE, ch, 0, tch, TO_VICT);
    }

    if (tveh->rigger && should_tch_see_chs_movement_message(tveh->rigger, ch, FALSE, TRUE))
      act(buf2, TRUE, ch, 0, tveh->rigger, TO_VICT | TO_REMOTE | TO_SLEEP);
  }

  // Watchers.
  for (struct char_data *tch = veh->in_room->watching; tch; tch = tch->next_watching) {
    if (should_tch_see_chs_movement_message(tch, ch, FALSE, TRUE)) {
      send_to_char("From a distance, you see:\r\n", tch);
      act(buf2, TRUE, ch, 0, tch, TO_VICT);
    }
  }

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
      crash_test(pilot, FALSE);
      chkdmg(v->follower);
    }
  }

  if (PLR_FLAGGED(ch, PLR_REMOTE))
    was_in = ch->in_room;
  ch->in_room = veh->in_room;
  if (!veh->dest)
    look_at_room(ch, 0, 0);
  for (struct char_data *tch = veh->in_room->people; tch; tch = tch->next_in_room)
    if (IS_NPC(tch) && AWAKE(tch) && MOB_FLAGGED(tch, MOB_AGGRESSIVE) &&
        !CH_IN_COMBAT(tch) && !IS_ASTRAL(tch) && !npc_vs_vehicle_blocked_by_quest_protection(GET_MOB_QUEST_CHAR_ID(tch), veh))
      set_fighting(tch, veh);
  if (PLR_FLAGGED(ch, PLR_REMOTE))
    ch->in_room = was_in;
  else
    ch->in_room = NULL;

  if ((get_speed(veh) > 80 && SECT(veh->in_room) == SPIRIT_CITY) || veh->in_room->icesheet[0])
  {
    crash_test(ch, FALSE);
    chkdmg(veh);
    if (!veh->people)
      load_vehicle_brain(veh);
  }
}

int perform_move(struct char_data *ch, int dir, int extra, struct char_data *vict, struct veh_data *vict_veh)
{
  struct room_data *was_in = NULL;
  struct follow_type *k, *next;

  if (ch == NULL || dir < 0 || dir >= NUM_OF_DIRS)
    return 0;

  stop_watching(ch);

  if (ch->in_veh || ch->char_specials.rigging) {
    move_vehicle(ch, dir);
    return 0;
  }

  FALSE_CASE(GET_POS(ch) < POS_FIGHTING || (AFF_FLAGGED(ch, AFF_PRONE) && !IS_NPC(ch)), "Maybe you should get on your feet first?");
  FALSE_CASE(PLR_FLAGGED(ch, PLR_MATRIX), "It will be difficult to do that while tethered to the matrix."); // This is for hitchers, primarily

  if (GET_POS(ch) >= POS_FIGHTING && FIGHTING(ch)) {
    WAIT_STATE(ch, PULSE_VIOLENCE * 2);
    struct char_data *blocker = find_a_character_that_blocks_fleeing_for_ch(ch);
    if (!blocker
        && (CAN_GO(ch, dir)
            && (!IS_NPC(ch) || !ROOM_FLAGGED(ch->in_room->dir_option[dir]->to_room, ROOM_NOMOB)))) {
      act("$n searches for a quick escape!", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char("You start moving away for a clever escape.\r\n", ch);
    } else if (blocker) {
      act("PANIC! $N caught you before you could escape!\r\n", FALSE, ch, 0, blocker, TO_CHAR);
      act("You lunge forward and block $n's escape.", FALSE, ch, 0, blocker, TO_VICT);
      act("$N lunges forward and blocks $n's escape.", FALSE, ch, 0, blocker, TO_NOTVICT);
      return 0;
    } else {
      act("PANIC! You couldn't escape!\r\n", FALSE, ch, 0, 0, TO_CHAR);
      return 0;
    }
  }

  // The exit does not exist or does not point to a valid room.
  if (!EXIT(ch, dir) || !EXIT(ch, dir)->to_room || EXIT(ch, dir)->to_room == &world[0]) {
    FALSE_CASE(!LIGHT_OK(ch), "Something seems to be in the way...");
    send_to_char("You cannot go that way...\r\n", ch);
    return 0;
  }

  // The exit is hidden and you're not staff.
  if (IS_SET(EXIT(ch, dir)->exit_info, EX_HIDDEN) && !IS_SENATOR(ch)) {
    FALSE_CASE(!LIGHT_OK(ch), "Something seems to be in the way...");
    send_to_char("You cannot go that way...\r\n", ch);
    return 0;
  }

  if (IS_SET(EXIT(ch, dir)->exit_info, EX_CLOSED)                                  /* The door is closed, and... */
      && !IS_SENATOR(ch)                                                           /* You're not staff, and... */
      && (!IS_ASTRAL(ch) || IS_SET(EXIT(ch, dir)->exit_info, EX_ASTRALLY_WARDED))) /* You're either not an astral being, or the door is warded. */
  {
    if (!LIGHT_OK(ch))
      send_to_char("Something seems to be in the way...\r\n", ch);
    else if (IS_SET(EXIT(ch, dir)->exit_info, EX_HIDDEN))
      send_to_char("You cannot go that way...\r\n", ch);
    else {
      if (IS_ASTRAL(ch)) {
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

  FALSE_CASE(ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_STAFF_ONLY) && GET_REAL_LEVEL(ch) < LVL_BUILDER,
             "Sorry, that area is for game administration only.");

  if (EXIT(ch, dir)->to_room->staff_level_lock > GET_REAL_LEVEL(ch)) {
    if (GET_REAL_LEVEL(ch) == 0) {
      send_to_char("NPCs aren't allowed in there.\r\n", ch);
    } else if (GET_REAL_LEVEL(ch) == 1) {
      send_to_char("Sorry, that area is for game administration only.\r\n", ch);
    } else {
      send_to_char(ch, "Sorry, you need to be a level-%d staff member to go there.\r\n", EXIT(ch, dir)->to_room->staff_level_lock);
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

  FALSE_CASE(ch->is_carrying_vehicle && ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_NOBIKE), "You'll have to drop any vehicles before going there. (If you can't drop them and are completely stuck, use the STUCK command.)");
  FALSE_CASE(get_armor_penalty_grade(ch) == ARMOR_PENALTY_TOTAL, "You're wearing too much armor to move!");

  // Flying vehicles can traverse any terrain.
  if (vict_veh && !ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_ALL_VEHICLE_ACCESS) && !veh_can_traverse_air(vict_veh)) {
    // Non-flying vehicles can't pass fall rooms.
    FALSE_CASE_PRINTF(ROOM_FLAGGED(EXIT(ch, dir)->to_room, ROOM_FALL), "%s would plunge to its destruction!\r\n", capitalize(GET_VEH_NAME_NOFORMAT(vict_veh)));

    // Check to see if your vehicle can handle the terrain type you're giving it.
    if (IS_WATER(EXIT(ch, dir)->to_room)) {
      FALSE_CASE_PRINTF(!veh_can_traverse_water(vict_veh), "%s would sink!\r\n", capitalize(GET_VEH_NAME_NOFORMAT(vict_veh)));
    } 
    
    /*  Do nothing-- you can put boats on wheels for the purpose of dragging them.
    else {
      FALSE_CASE_PRINTF(!veh_can_traverse_land(vict_veh), "You'll have a hard time getting %s on land.\r\n", GET_VEH_NAME(vict_veh));
    }
    */
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

  // Strict exits are used for things like invitation passes etc. You must have one to pass, and it gets bound to you on use.
  if (IS_SET(EXIT(ch, dir)->exit_info, EX_STRICT_ABOUT_KEY)){
    struct obj_data *key = get_carried_vnum(ch, EXIT(ch, dir)->key, TRUE);
    FALSE_CASE(!IS_SENATOR(ch) && !key, "You can't go there without having the right key or pass in your inventory.");
    if (key) {
      soulbind_obj_to_char(key, ch, FALSE);
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
    if ((door = search_block(dir, exitdirs, TRUE)) == -1
        && (door = search_block(dir, fulldirs, TRUE)) == -1) {
      send_to_char("That's not a direction.\r\n", ch);
      return -1;
    }
    if (EXIT(ch, door) && !IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN)) {
      if (IS_SET(EXIT(ch, door)->exit_info, EX_DESTROYED)) {
        send_to_char(ch, "You're not going to have much luck-- the door to the %s has been destroyed.\r\n", fulldirs[door]);
        return -1;
      }

      if (EXIT(ch, door)->keyword) {
        if (isname(type, EXIT(ch, door)->keyword)) {
          return door;
        } else {
          send_to_char(ch, "I see no %s there.\r\n", type);
          return -1;
        }
      } else {
        return door;
      }
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
    if ((door = search_block(non_const_type, exitdirs, TRUE)) == -1
        && (door = search_block(non_const_type, fulldirs, TRUE)) == -1) {
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

    if (EXIT(ch, door) && !IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN)) {
      if (IS_SET(EXIT(ch, door)->exit_info, EX_DESTROYED)) {
        send_to_char(ch, "You're not going to have much luck-- the door to the %s has been destroyed.\r\n", fulldirs[door]);
        return -1;
      }
      return door;
    } else {
      send_to_char(ch, "There doesn't appear to be a door to the %s.\r\n", fulldirs[door]);
      return -1;
    }
  }
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
      strlcpy(buf, "The lights on the maglock switch from red to green as $n bypasses ", sizeof(buf));
    } else {
      send_to_char("The lights on the maglock switch from green to red.\r\n", ch);
      strlcpy(buf, "The lights on the maglock switch from green to red as $n bypasses ", sizeof(buf));
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
  strlcpy(not_const, argument, sizeof(not_const));
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

  // If it's a cardinal direction, check for a door.
  /* for (int dir_idx = NORTH; dir_idx <= DOWN; dir_idx++) {
    if (!str_cmp(type, exit_dirs[dir_idx]) || !str_cmp(type, fulldirs[dir_idx])) {
      asdf hidden destroyed
      door = dir_idx;
      break;
    }
  } */

  // Check for an object or vehicle nearby that matches the keyword.
  if (!generic_find(type, FIND_OBJ_EQUIP | FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &victim, &obj)
      && !veh
      && !(veh = get_veh_list(type, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch))) {
    if ((door = find_door(ch, type, dir, cmd_door[subcmd])) == -1) {
      // Already gave an error message in find_door, bail out.
      return;
    }
  }

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

  else {
    send_to_char(ch, "You don't see anything you can %s there.\r\n", subcmd == SCMD_CLOSE ? "close" : "open");
    if (ch->in_veh) {
      send_to_char("(Maybe you should ^WLEAVE^n your vehicle?)\r\n", ch);
    }
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
      send_to_char(ch, "%s is too damaged to enter.\r\n", CAP(GET_VEH_NAME_NOFORMAT(found_veh)));
      return;
    }
  }

  // Too fast? Can't (unless admin).
  if (found_veh->cspeed > SPEED_IDLE) {
    if (access_level(ch, LVL_ADMIN)) {
      send_to_char("You use your staff powers to match its speed as you board.\r\n", ch);
    } else if (IS_ASTRAL(ch)) {
      send_to_char(ch, "You mentally latch on to %s and draw yourself towards it.\r\n", GET_VEH_NAME(found_veh));
    }
    else {
      send_to_char(ch, "%s is moving too fast for you to board!\r\n", CAP(GET_VEH_NAME_NOFORMAT(found_veh)));
      return;
    }
  }

#ifdef USE_OLD_VEHICLE_LOCKING_SYSTEM
  // Locked? Can't (unless admin)
  if ((found_veh->type != VEH_BIKE && found_veh->type != VEH_MOTORBOAT) && found_veh->locked) {
    if (access_level(ch, LVL_ADMIN)) {
      send_to_char("You use your staff powers to bypass the locked doors.\r\n", ch);
    } else if (IS_ASTRAL(ch)) {
      // No message-- it just works. Astrals don't care about locks.
    } else if (GET_IDNUM(ch) == found_veh->owner) {
      send_to_char("You unlock the doors.\r\n", ch);
      found_veh->locked = FALSE;
    } else {
      send_to_char("It's locked.\r\n", ch);
      return;
    }
  }
#else
  // You can only enter PC-owned vehicles if you're the owner, the owner is in control, or you're staff.
  if (found_veh->owner > 0 || found_veh->locked) {
    bool is_staff = access_level(ch, LVL_CONSPIRATOR);
    bool is_owner = GET_IDNUM_EVEN_IF_PROJECTING(ch) == found_veh->owner;
    bool is_following_owner = (ch->master && GET_IDNUM(ch->master) == found_veh->owner && AFF_FLAGGED(ch, AFF_GROUP)); // todo: make this grouped with (owner can follow you)
    bool owner_is_remote_rigging = found_veh->rigger && GET_IDNUM(found_veh->rigger);
    bool owner_is_driving = FALSE;

    for (struct char_data *occupant = found_veh->people; occupant; occupant = occupant->next_in_veh) {
      if (GET_IDNUM(occupant) > 0 && GET_IDNUM(occupant) == found_veh->owner) {
        owner_is_driving = AFF_FLAGGED(occupant, AFF_PILOT) || AFF_FLAGGED(occupant, AFF_RIG);
        break;
      }
    }

    if (!is_owner && !is_following_owner && (found_veh->locked || (!owner_is_remote_rigging && !owner_is_driving))) {
      if (is_staff) {
        send_to_char("You staff-override the fact that the owner isn't explicitly letting you in.\r\n", ch);
      } else {
        // This error message kinda sucks to write.
        if (!ch->master || GET_IDNUM(ch->master) != found_veh->owner) {
          send_to_char(ch, "You need to be following the owner to enter %s.\r\n", GET_VEH_NAME(found_veh));
        } else if (!AFF_FLAGGED(ch, AFF_GROUP)) {
          send_to_char(ch, "The owner needs to group you before you can enter %s.\r\n", GET_VEH_NAME(found_veh));
        }
        return;
      }
    }
  }
#endif

  if (inveh && (AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE))) {

    if (inveh->in_veh)
      send_to_char("You are already inside a vehicle.\r\n", ch);
    else if (inveh == found_veh)
      send_to_char("It'll take a smarter mind than yours to figure out how to park your vehicle inside itself.\r\n", ch);
    else if (found_veh->load - found_veh->usedload < calculate_vehicle_entry_load(inveh))
      send_to_char("There is not enough room in there for that.\r\n", ch);
    else {
      strlcpy(buf3, GET_VEH_NAME(inveh), sizeof(buf3));
      snprintf(buf, sizeof(buf), "%s drives into the back of %s.", buf3, GET_VEH_NAME(found_veh));
      snprintf(buf2, sizeof(buf2), "You drive into the back of %s. (Use the PUSH or LEAVE command to get the vehicle out later.)\r\n", GET_VEH_NAME(found_veh));
      if (inveh->in_room->people)
        act(buf, 0, inveh->in_room->people, 0, 0, TO_ROOM);
      send_to_veh(buf2, inveh, NULL, TRUE);
      veh_to_veh(inveh, found_veh);
    }
    return;
  }

  // No space? Can't (unless admin)
  if (!found_veh->seating[front] && !(repair_vehicle_seating(found_veh) && found_veh->seating[front])) {
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
    snprintf(buf2, sizeof(buf2), "$n is dragged into %s.", GET_VEH_NAME(found_veh));
  else if (found_veh->type == VEH_BIKE)
    snprintf(buf2, sizeof(buf2), "$n gets on %s.", GET_VEH_NAME(found_veh));
  else
    snprintf(buf2, sizeof(buf2), "$n climbs into %s.", GET_VEH_NAME(found_veh));
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
      act("You follow $N.", FALSE, k->follower, 0, ch, TO_CHAR);
      if (!found_veh->seating[1] && found_veh->seating[0]) {
        strlcpy(buf3, "rear", sizeof(buf3));
      } else {
        strlcpy(buf3, argument, sizeof(buf3));
      }
      enter_veh(k->follower, found_veh, buf3, FALSE);
    }
  }
}

ACMD(do_drag)
{
  struct char_data *vict = NULL;
  struct veh_data *veh, *drag_veh = NULL;
  int dir;

  two_arguments(argument, buf1, buf2);

  if (!*buf1 || !*buf2) {
    send_to_char("Who do you want to drag where?\r\n", ch);
    return;
  }

  FAILURE_CASE(IS_ASTRAL(ch), "Astral projections aren't really known for their ability to drag things.\r\n");
  FAILURE_CASE(IS_WORKING(ch), "You're too busy to do that.\r\n");

  // First check for a character to drag.
  if (!(vict = get_char_room_vis(ch, buf1)) || IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
    // No character? Try for a vehicle.
    if (ch->in_room && (drag_veh = get_veh_list(buf1, ch->in_room->vehicles, ch))) {
      if (drag_veh->owner != GET_IDNUM(ch) && drag_veh->locked) {
        send_to_char(ch, "%s's anti-theft measures beep loudly.\r\n", capitalize(GET_VEH_NAME_NOFORMAT(drag_veh)));
        return;
      }

      if ((IS_CARRYING_W(ch) + calculate_vehicle_weight(drag_veh)) > CAN_CARRY_W(ch) * 2.5) {
        send_to_char(ch, "%s is too heavy for you to move!\r\n", capitalize(GET_VEH_NAME_NOFORMAT(drag_veh)));
        return;
      }
    } else {
      // Found no vehicle either.
      send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf1);
      return;
    }
  }

  // Attempt to break bindings.
  if (vict && AFF_FLAGGED(vict, AFF_BINDING)) {
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
  if (dir == -1)
    dir = search_block(buf2, fulllookdirs, FALSE);

  if (ch->in_veh) {
    dir = -1;
    veh = get_veh_list(buf2, ch->in_veh->carriedvehs, ch);
  } else {
    veh = get_veh_list(buf2, ch->in_room->vehicles, ch);
  }
  if (dir == -1 && !veh) {
    send_to_char("What direction?\r\n", ch);
    return;
  }
  if (!veh)
    dir = convert_look[dir];

  if (vict) {
    if (IS_NPC(vict) && vict->master != ch) {
      // You may only drag friendly NPCs. This prevents a whole lot of abuse options.
      send_to_char("You may only drag NPCs who are following you already.\r\n", ch);
      return;
    }

    if (GET_POS(vict) > POS_LYING) {
      act("You can't drag $M off in $S condition!", FALSE, ch, 0, vict, TO_CHAR);
      return;
    }

    if (!affected_by_spell(vict, SPELL_LEVITATE) && (int)((GET_STR(ch)*10) * 3/2) < (GET_WEIGHT(vict) + IS_CARRYING_W(vict))) {
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
    return;
  }

  else if (drag_veh) {
    if (drag_veh == veh) {
      send_to_char(ch, "...into itself?\r\n");
      return;
    }

    char drag_veh_name[500], act_buf[1000];
    strlcpy(drag_veh_name, GET_VEH_NAME(drag_veh), sizeof(drag_veh_name));

    if (dir == -1 && veh) {
      // Does this veh fit in there?
      if (veh->usedload + calculate_vehicle_entry_load(drag_veh) > veh->load) {
        send_to_char(ch, "%s won't fit in %s!", CAP(GET_VEH_NAME_NOFORMAT(drag_veh)), GET_VEH_NAME(veh));
        return;
      }

      enter_veh(ch, veh, "rear", FALSE);
      if (ch->in_veh == veh) {
        send_to_char(ch, "Heaving and straining, you drag %s into %s.\r\n", drag_veh_name, GET_VEH_NAME(veh));
        snprintf(act_buf, sizeof(act_buf), "Heaving and straining, $n drags %s into %s.\r\n", drag_veh_name, GET_VEH_NAME(veh));
        act(act_buf, TRUE, ch, 0, 0, TO_ROOM);
        veh_from_room(drag_veh);
        veh_to_veh(drag_veh, veh);
      } else {
        send_to_char("You can't get in there yourself.\r\n", ch);
        return;
      }
    } else {
      struct room_data *in_room = ch->in_room;

      if (!EXIT2(in_room, dir)) {
        send_to_char("You can't go that way.\r\n", ch);
        return;
      }

      // Check to make sure the vehicle is allowed there.
      struct room_data *target_room = EXIT2(in_room, dir)->to_room;
      if (!room_accessible_to_vehicle_piloted_by_ch(target_room, drag_veh, ch, TRUE)) {
        // Error message was already sent in room_accessible.
        return;
      }

      perform_move(ch, dir, 0, NULL, veh);
      // Message only if we succeeded.
      if (in_room != ch->in_room) {
        send_to_char(ch, "Heaving and straining, you drag %s along with you.\r\n", drag_veh_name);
        snprintf(act_buf, sizeof(act_buf), "Heaving and straining, $n drags %s into the room.\r\n", drag_veh_name);
        act(act_buf, TRUE, ch, 0, 0, TO_ROOM);
      }
      veh_from_room(drag_veh);
      veh_to_room(drag_veh, ch->in_room);
    }
    WAIT_STATE(ch, 3 RL_SEC);
    return;
  }
}

ACMD(do_enter)
{
  int door;
  struct veh_data *found_veh = NULL;
  struct obj_data *found_obj = NULL;
  struct char_data *dummy = NULL;

  FAILURE_CASE(CH_IN_COMBAT(ch), "Not while you're fighting!");

  two_arguments(argument, buf, buf2);
  if (*buf) {                   /* an argument was supplied, search for door keyword */
    generic_find(buf, FIND_OBJ_ROOM, ch, &dummy, &found_obj);
    if (AFF_FLAGGED(ch, AFF_PILOT) && ch->in_veh && ch->in_veh->in_veh) {
      send_to_char("There is not enough room to maneuver in here.\r\n", ch);
      return;
    }

    struct veh_data *veh;
    RIG_VEH(ch, veh);
    if (veh && (AFF_FLAGGED(ch, AFF_PILOT) || IS_RIGGING(ch))) {
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
      if (veh == found_veh) {
        send_to_char("You can't re-enter the same vehicle!\r\n", ch);
      } else {
        enter_veh(ch, found_veh, buf2, FALSE);
      }
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

  RIG_VEH(ch, veh);

  bool is_in_control_of_veh = AFF_FLAGGED(ch, AFF_PILOT) || IS_RIGGING(ch);

  if (veh_is_currently_flying(veh) || (is_in_control_of_veh && veh->in_veh && veh_is_currently_flying(veh->in_veh))) {
    send_to_char("You take one look at the far-distant ground and reconsider your plan of action.\r\n", ch);
    return;
  }

  if (is_in_control_of_veh && veh->in_veh) {
    if (veh->in_veh->in_veh) {
      send_to_char("There is not enough room to drive out of here.\r\n", ch);
      return;
    }
    snprintf(buf, sizeof(buf), "You drive out of the back of %s.\r\n", GET_VEH_NAME(veh->in_veh));
    snprintf(buf2, sizeof(buf2), "%s drives out of the back.", GET_VEH_NAME(veh));
    send_to_veh(buf, veh, NULL, TRUE);
    send_to_veh(buf2, veh->in_veh, NULL, FALSE);
    strlcpy(buf3, GET_VEH_NAME(veh), sizeof(buf3));
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

  if (IS_RIGGING(ch)) {
    send_to_char(ch, "%s isn't in a vehicle.\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
    return;
  }

  if (veh->cspeed > SPEED_IDLE && !AFF_FLAGGED(ch, AFF_PILOT)) {
    if (access_level(ch, LVL_ADMIN)) {
      send_to_char("You use your staff powers to exit the moving vehicle safely.\r\n", ch);
    } else {
      send_to_char("Your vehicle's moving! That would just be crazy.\r\n", ch);
      return;
    }
  }

  if (AFF_FLAGGED(ch, AFF_PILOT)) {
    if (veh->cspeed > SPEED_IDLE) {
      act("$n slams on the brake, then climbs out of the drivers seat and into the street.", FALSE, ch, 0, 0, TO_VEH);
    } else {
      act("$n climbs out of the drivers seat and into the street.", FALSE, ch, 0, 0, TO_VEH);
    }
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
  if (ch->in_veh || IS_RIGGING(ch)) {
    leave_veh(ch);
    return;
  }

  struct room_data *in_room = get_ch_in_room(ch);
  if (!in_room) {
    send_to_char("Panic strikes you-- you're floating in a void!\r\n", ch);
    mudlog("SYSERR: Char has no in_room!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  // If you're in an apartment, you're able to leave to the atriun no matter what. Prevents lockin.
  if (GET_APARTMENT(in_room)) {
    rnum_t atrium_rnum = real_room(GET_APARTMENT(in_room)->get_atrium_vnum());

    if (atrium_rnum < 0) {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Atrium for %s was inaccessible! Using A Bright Light.", GET_APARTMENT(in_room)->get_full_name());
      atrium_rnum = 0; // A Bright Light
    }

    act("You leave the residence.", TRUE, ch, 0, 0, TO_CHAR);
    act("$n leaves the residence.", TRUE, ch, 0, 0, TO_ROOM);

    // Transfer the char.
    struct room_data *target_room = &world[atrium_rnum];
    char_from_room(ch);
    char_to_room(ch, target_room);

    act("$n arrives.", TRUE, ch, 0, 0, TO_ROOM);

    if (GET_POS(ch) > POS_STUNNED && GET_POS(ch) != POS_FIGHTING)
      GET_POS(ch) = POS_STANDING;

    // If not screenreader, look.
    if (!PRF_FLAGGED(ch, PRF_SCREENREADER))
      look_at_room(ch, 0, 0);
    return;
  }

  // If you're in a PGHQ, you teleport to the first room of the PGHQ's zone.
  if (in_room->zone >= 0 && zone_table[in_room->zone].is_pghq) {
    rnum_t entrance_rnum = real_room(zone_table[in_room->zone].number * 100);
    if (entrance_rnum >= 0) {
      struct room_data *entrance = &world[entrance_rnum];

      // Attempt to find the exit to a non-HQ room from there.
      for (int dir = 0; dir < NUM_OF_DIRS; dir++) {
        if (EXIT2(entrance, dir) && EXIT2(entrance, dir)->to_room && EXIT2(entrance, dir)->to_room->zone != in_room->zone) {
          entrance = EXIT2(entrance, dir)->to_room;
          break;
        }
      }

      send_to_char("You glance around to get your bearings, then head for the exit.\r\n\r\n", ch);
      char_from_room(ch);
      char_to_room(ch, entrance);
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

  // Movement restriction: Must be standing and not fighting.
  FAILURE_CASE(GET_POS(ch) < POS_FIGHTING, "Maybe you should get on your feet first?");
  FAILURE_CASE(FIGHTING(ch) || FIGHTING_VEH(ch), "You'll have to FLEE if you want to escape from combat!");

  // Leaving an elevator shaft is handled in the button panel's spec proc code. See transport.cpp.
  FAILURE_CASE(!ROOM_FLAGGED(in_room, ROOM_INDOORS), "You're already outside... where do you want to go?");

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
    act("You are already on your feet.", FALSE, ch, 0, 0, TO_CHAR);
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
    end_all_caster_records(ch, TRUE);
  }
  DELETE_ARRAY_IF_EXTANT(GET_DEFPOS(ch));
}

ACMD(do_wake)
{
  struct char_data *vict;
  int self = 0;

  one_argument(argument, arg);
  if (*arg) {
    vict = get_char_room_vis(ch, arg);

    // 'wake up'
    if (!vict && !str_cmp(arg, "up"))
      vict = ch;
      
    if (GET_POS(ch) == POS_SLEEPING && vict != ch)
      send_to_char("You can't wake people up if you're asleep yourself!\r\n", ch);
    else if (vict == NULL)
      send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
    else if (vict == ch) {
      // This is a weird way to break, but it means that the code immediately below doesn't fire, and instead the self-wake version fires.
      self = 1;
    } else if (GET_POS(vict) > POS_SLEEPING)
      act("$E is already awake.", FALSE, ch, 0, vict, TO_CHAR);
    else if (GET_POS(vict) <= POS_STUNNED || GET_PHYSICAL(vict) < 100)
      act("You can't wake $M up!", FALSE, ch, 0, vict, TO_CHAR);
    else {
      act("You wake $M up.", FALSE, ch, 0, vict, TO_CHAR);
      act("You are awakened by $n.", FALSE, ch, 0, vict, TO_VICT | TO_SLEEP);
      GET_POS(vict) = POS_STANDING;
    }
    if (!self)
      return;
  }
  if (GET_POS(ch) > POS_SLEEPING)
    send_to_char("You are already awake...\r\n", ch);
  else {
    if (ch->in_veh) {
      send_to_char("You awaken and sit up.\r\n", ch);
      snprintf(buf, sizeof(buf), "%s awakens.\r\n", GET_NAME(ch));
      send_to_veh(buf, ch->in_veh, ch, FALSE);
      GET_POS(ch) = POS_SITTING;
    } else {
      send_to_char("You awaken and stand up.\r\n", ch);
      act("$n awakens.", TRUE, ch, 0, 0, TO_ROOM);
      GET_POS(ch) = POS_STANDING;
    }
  }
  DELETE_ARRAY_IF_EXTANT(GET_DEFPOS(ch));
}

void perform_unfollow(struct char_data *ch) {
  if (ch->master) {
    stop_follower(ch);
    AFF_FLAGS(ch).RemoveBit(AFF_GROUP);
  } else {
    send_to_char("You are already following yourself.\r\n", ch);
  }
}

ACMD(do_unfollow) {
  // First form: UNFOLLOW with no arguments has you stop following.
  if (!*argument) {
    if (ch->master) {
      stop_follower(ch);
      AFF_FLAGS(ch).RemoveBit(AFF_GROUP);
    } else {
      send_to_char("You are already following yourself. If you want to drop a follower, use UNFOLLOW <target>.\r\n", ch);
    }
    return;
  }

  // Second form: UNFOLLOW <leader>, which will stop you from following X.
  skip_spaces(&argument);
  if (ch->master && keyword_appears_in_char(argument, ch->master)) {
    stop_follower(ch);
    AFF_FLAGS(ch).RemoveBit(AFF_GROUP);
    return;
  }

  // Third form: UNFOLLOW <follower>, which loses an existing follower (grouped or not).
  for (follow_type *f = ch->followers; f; f = f->next) {
    if (keyword_appears_in_char(argument, f->follower)) {
      act("You move around abruptly and lose $N.", FALSE, ch, 0, f->follower, TO_CHAR);
      act("$n moves around abruptly and loses $N.", FALSE, ch, 0, f->follower, TO_ROOM);
      AFF_FLAGS(f->follower).RemoveBit(AFF_GROUP);
      stop_follower(f->follower);
      return;
    }
  }

  send_to_char(ch, "You're neither following nor being followed by anyone matching the keyword '%s'.\r\n", argument);
}

ACMD(do_follow)
{
  struct char_data *leader;

  void stop_follower(struct char_data *ch);
  void add_follower(struct char_data *ch, struct char_data *leader);

  one_argument(argument, buf);

  if (!*buf ) {
    perform_unfollow(ch);
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

  FAILURE_CASE_PRINTF(ch->master == leader, "You are already following %s.", HMHR(leader));
  FAILURE_CASE(IS_ASTRAL(leader) && !IS_ASTRAL(ch), "Non-astral beings can't follow astral beings.");
  
  // Special case: FOLLOW SELF unfollows anyone else.
  if (leader == ch) {
    if (!ch->master) {
      send_to_char("You are already following yourself.\r\n", ch);
      return;
    }
    stop_follower(ch);
    AFF_FLAGS(ch).RemoveBit(AFF_GROUP);
    return;
  }
  
  FAILURE_CASE(circle_follow(ch, leader), "Sorry, but following in loops is not allowed.");

  FAILURE_CASE(PRF_FLAGGED(leader, PRF_NOFOLLOW) && GET_LEVEL(ch) < LVL_ADMIN, "You can't: They've set the NOFOLLOW flag.");
  // Mimic other failure message to lessen info leakage.
  FAILURE_CASE(IS_IGNORING(leader, is_blocking_following_from, ch), "You can't: They've set the NOFOLLOW flag.");

  // Do the following.
  if (ch->master)
    stop_follower(ch);
  AFF_FLAGS(ch).RemoveBit(AFF_GROUP);
  add_follower(ch, leader);
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
    mudlog_vfprintf(ch, LOG_CHEATLOG, "STUCK COMMAND USED: %s ended up with no vehicle or room! Rationale given is '%s'. Will teleport to Mortal Relations.",
                    GET_CHAR_NAME(ch),
                    reason);
  } else {
    struct room_data *room = get_ch_in_room(ch);
    mudlog_vfprintf(ch, LOG_CHEATLOG, "STUCK COMMAND USED: %s claims to be stuck in %s'%s'^g (%ld). Rationale given is '%s'. Will teleport to Mortal Relations.",
                    GET_CHAR_NAME(ch),
                    ch->in_veh ? "a vehicle in " : "",
                    GET_ROOM_NAME(room),
                    GET_ROOM_VNUM(room),
                    reason);
  }

  rnum_t mortal_relations_rnum = real_room(RM_MORTAL_RELATIONS_CONFERENCE_ROOM);

  if (mortal_relations_rnum == NOWHERE) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Mortal Relations conference room %ld not defined! Sending %s to 0.", RM_MORTAL_RELATIONS_CONFERENCE_ROOM, GET_CHAR_NAME(ch));
    mortal_relations_rnum = 0;
  }

  if (ch->in_room || ch->in_veh)
    char_from_room(ch);
  char_to_room(ch, &world[mortal_relations_rnum]);
  send_to_char(ch, "Thanks for your error report! You have been teleported to the Mortal Relations Conference Room, and may leave after the ten-second delay passes.\r\n");
  WAIT_STATE(ch, 10 RL_SEC);
}

void set_sneaking_wait_state(struct char_data *ch) {
  if (!ch || ch->in_veh || !AFF_FLAGGED(ch, AFF_SNEAK))
    return;

  int wait_divisor = MAX(1, (GET_QUI((ch)) + GET_SKILL(ch, SKILL_ATHLETICS)) / 4);
  float ameliorated_wait_state = (float) MAX_SNEAKING_WAIT_STATE / wait_divisor;
  float calculated_wait_length = MAX(MIN_SNEAKING_WAIT_STATE, ameliorated_wait_state);

  send_to_char("^[F222]You sneak around.^n\r\n", ch);

  WAIT_STATE(ch, (int) calculated_wait_length);
}