
/* ************************************************************************
 *   File: limits.c                                      Part of CircleMUD *
 *  Usage: limits & gain funcs for HMV, exp, hunger/thirst, idle time      *
 *                                                                         *
 *  All rights reserved.  See license.doc for complete information.        *
 *                                                                         *
 *  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 ************************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <vector>
#include <sstream>
#include <algorithm>

#if defined(WIN32) && !defined(__CYGWIN__)
#include <process.h>
#else
#include <unistd.h>
#endif

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "dblist.hpp"
#include "handler.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "constants.hpp"
#include "newshop.hpp"
#include "newmagic.hpp"
#include "newmail.hpp"
#include "config.hpp"
#include "transport.hpp"
#include "memory.hpp"

extern class objList ObjList;
extern int modify_target(struct char_data *ch);
extern void end_quest(struct char_data *ch);
extern char *cleanup(char *dest, const char *src);
extern void damage_equip(struct char_data *ch, struct char_data *victim, int power, int type);
extern bool check_adrenaline(struct char_data *, int);
extern bool House_can_enter_by_idnum(long idnum, vnum_t house);
extern int get_paydata_market_maximum(int host_color);
extern int get_paydata_market_minimum(int host_color);
extern void wire_nuyen(struct char_data *ch, int amount, vnum_t character_id);
extern void save_shop_orders();
extern bool docwagon(struct char_data *ch);

void mental_gain(struct char_data * ch)
{
  int gain = 0;

  if (IS_PROJECT(ch))
    return;

  switch (GET_POS(ch))
  {
    case POS_SLEEPING:
      gain = 25;
      break;
    case POS_STUNNED:
    case POS_LYING:
    case POS_RESTING:
      gain = 20;
      break;
    case POS_SITTING:
      gain = 15;
      break;
    case POS_STANDING:
      gain = 10;
      break;
    case POS_FIGHTING:
      gain = 5;
      break;
  }

  if (IS_NPC(ch)) {
    gain *= 2;
  } else {
    gain *= get_drug_heal_multiplier(ch);
  }

  if (find_workshop(ch, TYPE_MEDICAL))
    gain = (int)(gain * 1.5);

#ifdef ENABLE_HUNGER
  if ((GET_COND(ch, COND_FULL) == MIN_FULLNESS) || (GET_COND(ch, COND_THIRST) == MIN_QUENCHED))
    gain >>= 1;
#endif

  if (ch->in_room && ROOM_FLAGGED(ch->in_room, ROOM_ENCOURAGE_CONGREGATION))
    gain *= 2;
  else if (ch->in_room && ROOM_FLAGGED(ch->in_room, ROOM_STERILE))
    gain *= 1.5;

  if (GET_TRADITION(ch) == TRAD_ADEPT)
    gain *= GET_POWER(ch, ADEPT_HEALING) + 1;
  if (GET_BIOOVER(ch) > 0)
    gain /= GET_BIOOVER(ch);

  gain = MAX(1, gain);

  GET_MENTAL(ch) = MIN(GET_MAX_MENTAL(ch), GET_MENTAL(ch) + gain);

  update_pos(ch);
}

void physical_gain(struct char_data * ch)
{
  int gain = 0;
  struct obj_data *bio;

  if (IS_PROJECT(ch))
    return;

  switch (GET_POS(ch))
  {
    case POS_STUNNED:
      gain = 13;
      break;
    case POS_SLEEPING:
      gain = 15;
      break;
    case POS_LYING:
    case POS_RESTING:
      gain = 13;
      break;
    case POS_SITTING:
      gain = 10;
      break;
    case POS_FIGHTING:
      gain = 5;
      break;
    case POS_STANDING:
      gain = 7;
      break;
  }

#ifdef ENABLE_HUNGER
  if ((GET_COND(ch, COND_FULL) == MIN_FULLNESS) || (GET_COND(ch, COND_THIRST) == MIN_QUENCHED))
    gain >>= 1;
#endif

  if (find_workshop(ch, TYPE_MEDICAL))
    gain = (int)(gain * 1.8);

  if (ch->in_room && ROOM_FLAGGED(ch->in_room, ROOM_ENCOURAGE_CONGREGATION))
    gain *= 2;

  if (IS_NPC(ch))
    gain *= 2;
  else
  {
    gain = MAX(1, gain);
    for (bio = ch->bioware; bio; bio = bio->next_content) {
      if (GET_BIOWARE_TYPE(bio) == BIO_SYMBIOTES) {
        switch (GET_BIOWARE_RATING(bio)) {
          case 1:
            gain = (int)(gain * 10/9);
            break;
          case 2:
            gain = (int)(gain * 7/5);
            break;
          case 3:
            gain *= 2;
            break;
        }
        break;
      }
    }

    gain *= get_drug_heal_multiplier(ch);
  }
  if (GET_TRADITION(ch) == TRAD_ADEPT)
    gain *= GET_POWER(ch, ADEPT_HEALING) + 1;
  if (GET_BIOOVER(ch) > 0)
    gain /= GET_BIOOVER(ch);
  GET_PHYSICAL(ch) = MIN(GET_MAX_PHYSICAL(ch), GET_PHYSICAL(ch) + gain);
  update_pos(ch);
}

char* get_new_kosherized_title(const char *title, unsigned short max_length) {
  if (title == NULL)
    title = "";

  // Uses NEW.
  char* mutable_title = str_dup(title);

  if (strlen(mutable_title) > max_length)
    mutable_title[max_length] = '\0';

  return mutable_title;
}

void set_title(struct char_data * ch, const char *title)
{
  DELETE_ARRAY_IF_EXTANT(GET_TITLE(ch));
  GET_TITLE(ch) = get_new_kosherized_title(title, MAX_TITLE_LENGTH);
}


void set_whotitle(struct char_data * ch, const char *title)
{
  DELETE_ARRAY_IF_EXTANT(GET_WHOTITLE(ch));
  GET_WHOTITLE(ch) = get_new_kosherized_title(title, MAX_TITLE_LENGTH);
}

void set_pretitle(struct char_data * ch, const char *title)
{
  DELETE_ARRAY_IF_EXTANT(GET_PRETITLE(ch));
  GET_PRETITLE(ch) = get_new_kosherized_title(title, MAX_TITLE_LENGTH);
}

// The centralized karma gain func. Includes code in support of multipliers.
int gain_karma(struct char_data * ch, int gain, bool rep, bool limits, bool multiplier)
{
  if (IS_PROJECT(ch))
    ch = ch->desc->original;

  int old = (int)(GET_KARMA(ch) / 100);

  // Non-NPC level 0? Not sure how that's a thing. Also, staff get no karma.
  if (!IS_NPC(ch) && ((GET_LEVEL(ch) < 1 || IS_SENATOR(ch))))
    return 0;

  // NPCs have a standard gain formula, no frills. No multiplier either.
  if (IS_NPC(ch)) {
    GET_KARMA(ch) += (int)(gain / 10);
    return (int)(gain / 10);
  }

  // If we've gotten here, it's a player. Mult it up.
  if (multiplier)
    gain *= KARMA_GAIN_MULTIPLIER * ((float) GET_CHAR_MULTIPLIER(ch) / 100);

  if (gain != 0) {
    if (limits) {
      if (GET_TKE(ch) >= 0 && GET_TKE(ch) < 100) {
        gain = MIN(MAX_NEWCHAR_GAIN, gain);
      } else if (GET_TKE(ch) >= 100 && GET_TKE(ch) < 500) {
        gain = MIN(MAX_MIDCHAR_GAIN, gain);
      } else {
        gain = MIN(MAX_OLDCHAR_GAIN, gain);
      }
    }

    if (multiplier && GET_CONGREGATION_BONUS(ch) > 0) {
      int karma_delta = MAX(CONGREGATION_MIN_KARMA_GAIN_PER_ACTION, gain * CONGREGATION_MULTIPLIER);
      karma_delta = MIN(CONGREGATION_MAX_KARMA_GAIN_PER_ACTION, karma_delta);
      gain += karma_delta;
      if ((--GET_CONGREGATION_BONUS(ch)) <= 0) {
        GET_CONGREGATION_BONUS(ch) = 0;
        send_to_char("The last of your extra energy from socializing runs out.\r\n", ch);
      }
    }

    if (gain < 0) {
      char message_buf[500];
      snprintf(message_buf, sizeof(message_buf),
               "Info: gain_karma processing negative karma amount %d for %s.",
               gain, GET_CHAR_NAME(ch));
      mudlog(message_buf, ch, LOG_SYSLOG, TRUE);
    }

    GET_KARMA(ch) += gain;

    GET_TKE(ch) += (int)(GET_KARMA(ch) / 100) - old;

    if (rep)
      GET_REP(ch) += (int)(GET_KARMA(ch) / 100) - old;
    else
      GET_NOT(ch) += (int)(GET_KARMA(ch) / 100) - old;
  }

  return gain;
}

// While nuyen gain is not multiplied by the global value, we still want to centralize it here and track it.
void _raw_gain_nuyen(struct char_data *ch, long amount, int category, bool bank, struct obj_data *credstick) {
  // Log errors if needed.
  if (category < 0 || category >= NUM_OF_TRACKED_NUYEN_INCOME_SOURCES) {
    char errbuf[500];
    snprintf(errbuf, sizeof(errbuf), "SYSERR: Invalid category %d sent to gain_nuyen! Still awarding the cash.", category);
    mudlog(errbuf, ch, LOG_SYSLOG, TRUE);
  }

  // Track the nuyen gain for metrics.
  if (ch->desc)
    GET_NUYEN_INCOME_THIS_PLAY_SESSION(ch, category) += abs(amount);

  // Apply the change
  if (credstick)
    GET_ITEM_MONEY_VALUE(credstick) += amount;
  else if (bank)
    GET_BANK_RAW(ch) += amount;
  else
    GET_NUYEN_RAW(ch) += amount;
}

void gain_nuyen(struct char_data *ch, long amount, int category) {
  _raw_gain_nuyen(ch, amount, category, FALSE, NULL);
}

void lose_nuyen(struct char_data *ch, long amount, int category) {
  _raw_gain_nuyen(ch, -amount, category, FALSE, NULL);
}

void gain_bank(struct char_data *ch, long amount, int category) {
  _raw_gain_nuyen(ch, amount, category, TRUE, NULL);
}

void lose_bank(struct char_data *ch, long amount, int category) {
  _raw_gain_nuyen(ch, -amount, category, TRUE, NULL);
}

void gain_nuyen_on_credstick(struct char_data *ch, struct obj_data *credstick, long amount, int category) {
  _raw_gain_nuyen(ch, amount, category, FALSE, credstick);
}

void lose_nuyen_from_credstick(struct char_data *ch, struct obj_data *credstick, long amount, int category) {
  _raw_gain_nuyen(ch, -amount, category, FALSE, credstick);
}

// only the pcs should need to access this
void gain_condition(struct char_data * ch, int condition, int value)
{
  bool intoxicated;
  struct obj_data *bio;

  if (GET_COND(ch, condition) == -1)    /* No change */
    return;

  intoxicated = (GET_COND(ch, COND_DRUNK) > MIN_DRUNK);

  if (value == -1) {
    for (bio = ch->bioware; bio; bio = bio->next_content) {
      if (GET_BIOWARE_TYPE(bio) == BIO_SYMBIOTES) {
        switch (GET_BIOWARE_RATING(bio)) {
          case 1:
            // Every other evaluation tick, remove an extra point of this condition.
            if (GET_BIOWARE_SYMBIOTE_CONDITION_DATA(bio))
              value--;
            GET_BIOWARE_SYMBIOTE_CONDITION_DATA(bio) = !GET_BIOWARE_SYMBIOTE_CONDITION_DATA(bio);
            break;
          case 2:
            // Every two out of three ticks, remove an extra point of this condition.
            if (!(GET_BIOWARE_SYMBIOTE_CONDITION_DATA(bio) % 3))
              value--;
            if ((++GET_BIOWARE_SYMBIOTE_CONDITION_DATA(bio)) > 9)
              GET_BIOWARE_SYMBIOTE_CONDITION_DATA(bio) = 0;
            break;
          case 3:
            // Every tick, remove an extra point.
            value--;
            break;
        }
      } else if (GET_BIOWARE_TYPE(bio) == BIO_SUPRATHYROIDGLAND) {
        value *= 2;
      } else if (GET_BIOWARE_TYPE(bio) == BIO_DIGESTIVEEXPANSION) {
        value = (int)((float)value / .8);
      }
    }
  }

  GET_COND(ch, condition) += value;

  GET_COND(ch, condition) = MAX(0, GET_COND(ch, condition));
  if ( condition == COND_DRUNK )
    GET_COND(ch, condition) = MIN(FOOD_DRINK_MAX, GET_COND(ch, condition));
  else
    GET_COND(ch, condition) = MIN(FOOD_DRINK_MAX, GET_COND(ch, condition));

  if (GET_COND(ch, condition) || PLR_FLAGGED(ch, PLR_CUSTOMIZE) ||
      PLR_FLAGGED(ch, PLR_WRITING) || PLR_FLAGGED(ch, PLR_MAILING) || PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED) || PLR_FLAGGED(ch, PLR_MATRIX))
    return;

  switch (condition)
  {
#ifdef ENABLE_HUNGER
    case COND_FULL:
      send_to_char("Your stomach growls.\r\n", ch);
      return;
    case COND_THIRST:
      send_to_char("Your mouth is dry.\r\n", ch);
      return;
#endif
    case COND_DRUNK:
      if (intoxicated)
        send_to_char("You feel a little less drunk.\r\n", ch);
      return;
    default:
      break;
  }
}

void remove_patch(struct char_data *ch)
{
  struct obj_data *patch = GET_EQ(ch, WEAR_PATCH);
  int stun;

  if (!patch)
    return;

  switch (GET_PATCH_TYPE(patch))
  {
    case PATCH_STIM:
      act("The effects of $p wear off, leaving you exhausted!", FALSE, ch, patch, 0, TO_CHAR);
      GET_MENTAL(ch) = MAX(0, GET_MENTAL(ch) - (GET_OBJ_VAL(patch, 1) - 1) * 100);
      if ((GET_TRADITION(ch) == TRAD_HERMETIC || GET_TRADITION(ch) == TRAD_SHAMANIC) &&
          success_test(GET_MAGIC(ch), GET_OBJ_VAL(patch, 1)) < 0) {
        magic_loss(ch, 100, TRUE);
        affect_total(ch);
      }
      update_pos(ch);
      break;
    case PATCH_TRANQ:
      stun = resisted_test(GET_OBJ_VAL(patch, 1), GET_REAL_BOD(ch) - (GET_BIOOVER(ch) > 0 ? GET_BIOOVER(ch) / 2 : 0),
                           GET_REAL_BOD(ch) - (GET_BIOOVER(ch) > 0 ? GET_BIOOVER(ch) / 2 : 0), GET_OBJ_VAL(patch, 1));
      if (stun > 0) {
        act("You feel the drugs from $p take effect.", FALSE, ch, patch, 0, TO_CHAR);
        GET_MENTAL(ch) = MAX(0, GET_MENTAL(ch) - (stun * 100));
        update_pos(ch);
      } else
        act("You resist the feeble effects of $p.", FALSE, ch, patch, 0, TO_CHAR);
      break;
    case PATCH_TRAUMA:
      if (success_test(GET_REAL_BOD(ch) - (GET_BIOOVER(ch) > 0 ? GET_BIOOVER(ch) / 2 : 0), GET_OBJ_VAL(patch, 1)) > 0)
        AFF_FLAGS(ch).RemoveBit(AFF_STABILIZE);
      break;
  }
  GET_EQ(ch, WEAR_PATCH) = NULL;
  patch->worn_by = NULL;
  patch->worn_on = -1;
  extract_obj(patch);
}

void check_idling(void)
{
  void perform_immort_invis(struct char_data *ch, int level);
  ACMD_CONST(do_return);
  ACMD_DECLARE(do_disconnect);
  struct char_data *ch, *next;

  for (ch = character_list; ch; ch = next) {
    next = ch->next;

    if (IS_NPC(ch) && ch->desc && ch->desc->original) {
      if (ch->desc->original->char_specials.timer > 10)
        do_return(ch, "", 0, 0);
    } else if (!IS_NPC(ch)) {
      ch->char_specials.timer++;
      ch->char_specials.last_social_action++;

      // Staff and busy PCs never idle out or get dropped from being LD.
      if (IS_SENATOR(ch) || IS_WORKING(ch))
        continue;

      // We allow people prf-flagged appropriately to not idle out, but only if they have a link.
      if (ch->desc && (PLR_FLAGGED(ch, PLR_NO_IDLE_OUT) || PRF_FLAGGED(ch, PRF_NO_VOID_ON_IDLE)))
        continue;

#ifdef VOID_IDLE_PCS
      if (!GET_WAS_IN(ch) && ch->in_room && ch->char_specials.timer > 15) {
        if (FIGHTING(ch))
          stop_fighting(FIGHTING(ch));
        if (CH_IN_COMBAT(ch))
          stop_fighting(ch);
        if (ch->master)
          stop_follower(ch);

        // No idling out in cabs.
        if (ROOM_VNUM_IS_CAB(GET_ROOM_VNUM(ch->in_room))) {
          if (ch->desc)
            send_to_char("The cabdriver stops off at Dante's long enough to kick you out.\r\n", ch);
          int rnum = real_room(RM_ENTRANCE_TO_DANTES);
          if (rnum >= 0)
            GET_WAS_IN(ch) = &world[rnum];
          else
            GET_WAS_IN(ch) = &world[1];
        } else {
          GET_WAS_IN(ch) = ch->in_room;
        }

        act("$n disappears into the void.", TRUE, ch, 0, 0, TO_ROOM);
        if (ch->desc)
          send_to_char("You have been idle, and are pulled into a void.\r\n", ch);
        char_from_room(ch);
        char_to_room(ch, &world[1]);
      }
#endif
        /* Disabled-- I get protecting them by moving them to the void, but why DC them?
        else if (ch->char_specials.timer > 30) {
          if (ch->in_room)
            char_from_room(ch);
          char_to_room(ch, &world[1]);
          if (GET_QUEST(ch))
            end_quest(ch);
          if (ch->desc)
            close_socket(ch->desc);
          ch->desc = NULL;
          snprintf(buf, sizeof(buf), "%s force-rented and extracted (idle).",
                  GET_CHAR_NAME(ch));
          mudlog(buf, ch, LOG_CONNLOG, TRUE);
          extract_char(ch);
        }
        */

      else if (!ch->desc && !PLR_FLAGGED(ch, PLR_PROJECT) && ch->char_specials.timer > NUM_MINUTES_BEFORE_LINKDEAD_EXTRACTION) {
        snprintf(buf, sizeof(buf), "%s removed from game (no link).", GET_CHAR_NAME(ch));
        mudlog(buf, ch, LOG_CONNLOG, TRUE);
        extract_char(ch);
      } else if (IS_SENATOR(ch) && ch->char_specials.timer > 15 &&
                 GET_INVIS_LEV(ch) < 2 &&
                 access_level(ch, LVL_EXECUTIVE) &&
                 PRF_FLAGGED(ch, PRF_AUTOINVIS))
        perform_immort_invis(ch, 2);
    }
  }
}

bool check_bioware(struct char_data *ch)
{
  if (!ch->desc
      || (ch->desc && ch->desc->connected)
      || ch->char_specials.timer >= 10
      || PLR_FLAGGED(ch, PLR_NEWBIE)
      || PRF_FLAGGED(ch, PRF_AFK)
      || AFF_FLAGS(ch).AreAnySet(BR_TASK_AFF_FLAGS, ENDBIT) // See awake.h for the definition of these aff flags.
  ) {
    return FALSE;
  }

  struct obj_data *bio;
  for (bio = ch->bioware; bio; bio = bio->next_content) {
    if (GET_BIOWARE_TYPE(bio) == BIO_PLATELETFACTORY)
    {
      if (--GET_BIOWARE_PLATELETFACTORY_DATA(bio) < 1) {
        GET_BIOWARE_PLATELETFACTORY_DATA(bio) = 12;
        if (success_test(GET_REAL_BOD(ch), 3 + GET_BIOWARE_PLATELETFACTORY_DIFFICULTY(bio)) < 1) {
          send_to_char("Your blood seems to erupt.\r\n", ch);
          act("$n collapses to the floor, twitching.", TRUE, ch, 0, 0, TO_ROOM);
          if (damage(ch, ch, 10, TYPE_BIOWARE, PHYSICAL))
            return TRUE;
        } else {
          send_to_char("Your heart strains, and you have a feeling of impending doom. Your need for blood thinners is dire!\r\n", ch);
        }
        GET_BIOWARE_PLATELETFACTORY_DIFFICULTY(bio)++;
      }
      if (GET_BIOWARE_PLATELETFACTORY_DATA(bio) == 4)
        send_to_char("You kinda feel like you should be taking some aspirin.\r\n", ch);
      else if (GET_BIOWARE_PLATELETFACTORY_DATA(bio) == 3)
        send_to_char("You could definitely go for some aspirin right now.\r\n", ch);
      else if (GET_BIOWARE_PLATELETFACTORY_DATA(bio) <= 2)
        send_to_char("You really feel like you need to take some aspirin.\r\n", ch);
      else if (GET_BIOWARE_PLATELETFACTORY_DATA(bio) == 1)
        send_to_char("Your heart strains, and you have a feeling of impending doom. Your need for blood thinners is dire!\r\n", ch);
      break;
    }
  }
  return FALSE;
}

int calculate_swim_successes(struct char_data *ch) {
  int swim_test_target, skill_dice, opposing_dice, successes, water_wings_bonus = 0, fin_bonus = 0;

  if (IS_NPC(ch) || IS_SENATOR(ch) || IS_AFFECTED(ch, AFF_LEVITATE) || !ch->in_room)
    return 20;

  for (int x = WEAR_LIGHT; x < NUM_WEARS; x++)
    if (GET_EQ(ch, x) && GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_CLIMBING && GET_OBJ_VAL(GET_EQ(ch, x), 1) == CLIMBING_TYPE_WATER_WINGS)
      water_wings_bonus += GET_OBJ_VAL(GET_EQ(ch, x), 0);

  for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content)
    if (GET_OBJ_VAL(cyber, 0) == CYB_FIN && !GET_OBJ_VAL(cyber, 9)) {
      fin_bonus = GET_QUI(ch) / 2;
      break;
    }

  snprintf(buf, sizeof(buf), "modify_target results: ");
  swim_test_target = MAX(2, ch->in_room->rating) + modify_target_rbuf_raw(ch, buf, sizeof(buf), 4, FALSE) - water_wings_bonus;
  act(buf, FALSE, ch, 0, 0, TO_ROLLS);

  if (GET_POS(ch) < POS_RESTING)
    skill_dice = MAX(1, (int)(GET_REAL_BOD(ch) / 3));
  else
    skill_dice = get_skill(ch, SKILL_ATHLETICS, swim_test_target);

  // Fins only matter if you're conscious and able to use them.
  if (GET_POS(ch) >= POS_RESTING)
    skill_dice += fin_bonus;

  opposing_dice = MAX(2, ch->in_room->rating);

  successes = success_test(skill_dice, swim_test_target);
  successes -= success_test(opposing_dice, skill_dice);

  snprintf(buf, sizeof(buf), "%s rolled %d dice against TN %d (lowered from %d by WW %d), opposed by %d dice at TN %d: %d net success%s.\r\n",
           GET_CHAR_NAME(ch),
           skill_dice,
           swim_test_target,
           swim_test_target + water_wings_bonus,
           water_wings_bonus,
           opposing_dice,
           skill_dice,
           successes,
           successes != 1 ? "es" : ""
         );
  act(buf, FALSE, ch, 0, 0, TO_ROLLS);

  return successes;
}

void analyze_swim_successes(struct char_data *temp_char) {
  struct room_data *room = temp_char->in_room;
  int old_sector_type = room->sector_type;
  room->sector_type = SPIRIT_SEA;

  int old_ath_skill = GET_SKILL(temp_char, SKILL_ATHLETICS);
  int old_level = GET_LEVEL(temp_char);

  GET_LEVEL(temp_char) = 1;

  GET_BOD(temp_char) = 6;
  GET_STR(temp_char) = 6;
  GET_WIL(temp_char) = 6;
  GET_QUI(temp_char) = 6;
  GET_INT(temp_char) = 6;
  GET_CHA(temp_char) = 6;


  for (int ath_skill_level = 0; ath_skill_level <= 12; ath_skill_level++) {
    GET_SKILL(temp_char, SKILL_ATHLETICS) = ath_skill_level;
    for (int water_rating = 2; water_rating <= 10; water_rating++) {
      room->rating = water_rating;

      int results[100];
      memset(results, 0, sizeof(results));

      for (int iteration_counter = 0; iteration_counter < 10000; iteration_counter++) {
        int successes = calculate_swim_successes(temp_char);
        results[successes + 50]++;
      }

      log_vfprintf("Results at skill %d, rating %d:", ath_skill_level, water_rating);
      for (int i = 0; i < 100; i++) {
        if (results[i])
          log_vfprintf("%d: %d", i - 50, results[i]);
      }
    }
  }
  GET_SKILL(temp_char, SKILL_ATHLETICS) = old_ath_skill;
  room->sector_type = old_sector_type;
  GET_LEVEL(temp_char) = old_level;
}

bool check_swimming(struct char_data *ch)
{
  int target, i, dam, test;

  if (!ch || IS_NPC(ch) || IS_SENATOR(ch) || !ch->in_room)
    return FALSE;

  i = calculate_swim_successes(ch);
  target = MAX(2, ch->in_room->rating) - i;

  if (GET_POS(ch) < POS_RESTING) {
    dam = convert_damage(stage(target, 0));
    if (dam > 0) {
      send_to_char("The feeling of dull impacts makes its way through the haze in your mind.\r\n", ch);
      act("$n's unconscious body is mercilessly thrown about by the current.",
          FALSE, ch, 0, 0, TO_ROOM);
      if (damage(ch, ch, dam, TYPE_DROWN, FALSE))
        return TRUE;
    } else {
      act("$n's unconscious body bobs in the current.", FALSE, ch, 0, 0, TO_ROOM);
    }
    return FALSE;
  }

  // Roll their damage test, in case it matters here.
  test = success_test(GET_WIL(ch), target);

  snprintf(buf, sizeof(buf), "%s rolled %d against TN %d: %d successes for staging down damage.\r\n",
           GET_CHAR_NAME(ch),
           GET_WIL(ch),
           target,
           test
         );
  act(buf, FALSE, ch, 0, 0, TO_ROLLS);

  if (i < 0) {
    dam = convert_damage(stage(-test, SERIOUS));
    if (dam > 0) {
      send_to_char(ch, "You fail to keep the water out of your lungs, and a racking cough seizes you.\r\n");
      if (damage(ch, ch, dam, TYPE_DROWN, FALSE))
        return TRUE;
    }
  } else if (!i) {
    dam = convert_damage(stage(-test, MODERATE));
    if (dam > 0) {
      send_to_char(ch, "Your head briefly slips below the surface, and you struggle to keep from inhaling water.\r\n");
      if (damage(ch, ch, dam, TYPE_DROWN, FALSE))
        return TRUE;
    }
  } else if (i < 3) {
    dam = convert_damage(stage(-test, LIGHT));
    if (dam > 0) {
      send_to_char(ch, "You struggle to keep your head above the water.\r\n");
      if (damage(ch, ch, dam, TYPE_DROWN, FALSE))
        return TRUE;
    }
  }
  return FALSE;
}

void process_regeneration(int half_hour)
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct char_data *ch, *next_char;

  for (ch = character_list; ch; ch = next_char) {
    next_char = ch->next;
    if (GET_TEMP_QUI_LOSS(ch) > 0) {
      GET_TEMP_QUI_LOSS(ch)--;
      affect_total(ch);
    }
    if (GET_POS(ch) >= POS_STUNNED) {
      physical_gain(ch);
      mental_gain(ch);
      if (!IS_NPC(ch) && IS_WATER(ch->in_room) && half_hour)
        if (check_swimming(ch))
          continue;
      if (GET_POS(ch) == POS_STUNNED)
        update_pos(ch);
      if (GET_PHYSICAL(ch) >= GET_MAX_PHYSICAL(ch)) {
        if (AFF_FLAGS(ch).IsSet(AFF_HEALED))
          AFF_FLAGS(ch).RemoveBit(AFF_HEALED);
        if (AFF_FLAGS(ch).IsSet(AFF_RESISTPAIN))
          AFF_FLAGS(ch).RemoveBit(AFF_RESISTPAIN);
      }
      if (GET_PHYSICAL(ch) > 0) {
        if (AFF_FLAGS(ch).IsSet(AFF_STABILIZE))
          AFF_FLAGS(ch).RemoveBit(AFF_STABILIZE);
      }
    }
    if (GET_POS(ch) == POS_MORTALLYW && !AFF_FLAGS(ch).IsSet(AFF_STABILIZE) && half_hour) {
      bool dam = TRUE;
      for (struct obj_data *obj = ch->bioware; obj; obj = obj->next_content)
        if (GET_BIOWARE_TYPE(obj) == BIO_METABOLICARRESTER) {
          if (++GET_BIOWARE_IS_ACTIVATED(obj) == 5) {
            GET_BIOWARE_IS_ACTIVATED(obj) = 0;
            dam = TRUE;
          } else
            dam = FALSE;
        }
      if (dam) {
        if (damage(ch, ch, 1, TYPE_SUFFERING, PHYSICAL))
          continue;
      } else {
        // Attempt to docwagon rescue without dealing damage.
        if (docwagon(ch))
          continue;
      }
    }
  }

  /* world updates for things like blood */
  for (int i = 0; i <= top_of_world; i++) {
    if (half_hour) {
      if (world[i].blood > 0)
        world[i].blood--;
      if (world[i].debris > 0)
        world[i].debris--;
    }
    if (world[i].icesheet[0])
      if (!--world[i].icesheet[1])
        world[i].icesheet[0] = 0;

    // Decrement player combat and player death auras.
    if (world[i].background[CURRENT_BACKGROUND_COUNT] && world[i].background[CURRENT_BACKGROUND_TYPE] >= AURA_BLOOD_MAGIC)
      world[i].background[CURRENT_BACKGROUND_COUNT]--;
  }
}

/* Update PCs, NPCs, and objects */
void point_update(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  ACMD_DECLARE(do_use);
  struct char_data *i, *next_char;

  // Generate the wholist file.
  ACMD_CONST(do_who);
  if (character_list)
    do_who(character_list, "", 0, 1);

  /* characters */
  bool is_npc = FALSE;
  for (i = character_list; i; i = next_char) {
    next_char = i->next;

    is_npc = IS_NPC(i);

    if (!is_npc) {
      playerDB.SaveChar(i, GET_LOADROOM(i));

      AFF_FLAGS(i).RemoveBit(AFF_DAMAGED);

#ifdef ENABLE_HUNGER
      if (!GET_POWER(i, ADEPT_SUSTENANCE) || !(time_info.hours % 3)) {
        // Leave this so that people's stomachs empty over time (can eat/drink more if they want to).
        gain_condition(i, COND_FULL, -1);
        gain_condition(i, COND_THIRST, -1);
      }
#endif
      gain_condition(i, COND_DRUNK, -1);

      if (GET_TEMP_ESSLOSS(i) > 0)
        GET_TEMP_ESSLOSS(i) = MAX(0, GET_TEMP_ESSLOSS(i) - 100);

#ifdef USE_PRIVATE_CE_WORLD
      if (SHOTS_FIRED(i) >= MARKSMAN_QUEST_SHOTS_FIRED_REQUIREMENT && SHOTS_TRIGGERED(i) != -1) {
        bool has_a_quest_item = FALSE;
        struct obj_data *tmp = NULL;

        // Check carried.
        for (tmp = i->carrying; !has_a_quest_item && tmp; tmp = tmp->next_content)
          has_a_quest_item = GET_OBJ_VNUM(tmp) == OBJ_MARKSMAN_LETTER || GET_OBJ_VNUM(tmp) == OBJ_MARKSMAN_BADGE;

        // Check worn.
        for (int worn = 0; !has_a_quest_item && worn < NUM_WEARS; worn++)
          has_a_quest_item = GET_EQ(i, worn) && GET_OBJ_VNUM(GET_EQ(i, worn)) == OBJ_MARKSMAN_BADGE;

        SHOTS_TRIGGERED(i) = (SHOTS_TRIGGERED(i) + 1) % 20;

        if (!has_a_quest_item && SHOTS_TRIGGERED(i) == 0) {
          send_to_char("^GYou feel you could benefit with some time at a shooting range.^n\r\n", i);
        }
      }
#else
      if (SHOTS_FIRED(i) >= 10000 && !SHOTS_TRIGGERED(i) && !number(0, 3)) {
        SHOTS_TRIGGERED(i)++;
        send_to_char("You feel you could benefit with some time at a shooting range.\r\n", i);
      }
#endif

      // Geas check from focus / foci overuse.
      if (GET_MAG(i) > 0) {
        int force = 0, total = 0;
        struct obj_data *focus;
        for (int x = 0; x < NUM_WEARS; x++) {
          if (!(focus = GET_EQ(i, x)))
            continue;

          if (GET_OBJ_TYPE(focus) == ITEM_FOCUS && GET_FOCUS_BONDED_TO(focus) == GET_IDNUM(i) && GET_FOCUS_ACTIVATED(focus)) {
            force += GET_FOCUS_FORCE(focus);
            total++;
          }

          else if ((x == WEAR_WIELD || x == WEAR_HOLD) && GET_OBJ_TYPE(focus) == ITEM_WEAPON && WEAPON_IS_FOCUS(focus) && WEAPON_FOCUS_USABLE_BY(focus, i)) {
            force += GET_WEAPON_FOCUS_RATING(focus);
            total++;
          }
        }
        if (GET_REAL_MAG(i) * 2 < 0) {
          mudlog("^RSYSERR: Multiplying magic for focus addiction check gave a NEGATIVE number! Increase the size of the variable!^n", i, LOG_SYSLOG, TRUE);
        } else {
          if (force * 100 > GET_REAL_MAG(i) * 2 && success_test(GET_REAL_MAG(i) / 100, force / 2) < 1) {
            int num = number(1, total);
            struct obj_data *foci = NULL;
            for (int x = 0; x < NUM_WEARS && !foci; x++)
              if (GET_EQ(i, x) && GET_OBJ_TYPE(GET_EQ(i, x)) == ITEM_FOCUS && GET_FOCUS_BONDED_TO(GET_EQ(i, x)) == GET_IDNUM(i) && GET_FOCUS_ACTIVATED(GET_EQ(i, x)) && !GET_FOCUS_BOND_TIME_REMAINING(GET_EQ(i, x)) && !--num)
                foci = GET_EQ(i, x);
            if (foci) {
              send_to_char(i, "^RYou feel some of your magic becoming locked in %s!^n Quick, take off all your foci before it happens again!\r\n", GET_OBJ_NAME(foci));
              GET_OBJ_VAL(foci, 9) = GET_IDNUM(i);
              magic_loss(i, 100, FALSE);
            }
          }
        }
      }

      if (HUNTING(i) && !AFF_FLAGGED(i, AFF_TRACKING) && ++HOURS_SINCE_TRACK(i) > 8)
        HUNTING(i) = NULL;

      if (i->bioware)
        if (check_bioware(i))
          continue;

      // Every hour, we check for withdrawal.
      process_withdrawal(i);
    }

    if (i->desc && IS_PROJECT(i)) {
      if (AFF_FLAGGED(i->desc->original, AFF_TRACKING) && HUNTING(i->desc->original) && !--HOURS_LEFT_TRACK(i->desc->original)) {
        act("The astral signature leads you to $N.", FALSE, i, 0, HUNTING(i->desc->original), TO_CHAR);
        char_from_room(i);
        char_to_room(i, HUNTING(i->desc->original)->in_room);
        act("$n enters the area.", TRUE, i, 0, 0, TO_ROOM);
        AFF_FLAGS(i->desc->original).RemoveBit(AFF_TRACKING);
        AFF_FLAGS(HUNTING(i->desc->original)).RemoveBit(AFF_TRACKED);
        HUNTING(i->desc->original) = NULL;
      }
      GET_ESS(i) -= 100;
      if (GET_ESS(i) <= 0) {
        struct char_data *victim = i->desc->original;
        send_to_char("As you feel the attachment to your physical body fade, you quickly return. The backlash from the fading connection rips through you...\r\n", i);
        PLR_FLAGS(i->desc->original).RemoveBit(PLR_PROJECT);
        i->desc->character = i->desc->original;
        i->desc->original = NULL;
        // GET_PHYSICAL(i->desc->character) = -(GET_BOD(i->desc->character) - 1) * 100;
        // act("$n collapses in a heap.", TRUE, i->desc->character, 0, 0, TO_ROOM);
        // update_pos(i->desc->character);
        i->desc->character->desc = i->desc;
        i->desc = NULL;
        extract_char(i);

        // Deal the damage instead of setting it.
        if (damage(victim, victim, GET_BOD(victim) - 1, TYPE_SUFFERING, TRUE))
          continue;

        // Continue regardless- i no longer exists.
        continue;
      } else if (GET_ESS(i) <= 100)
        send_to_char("You feel memories of your physical body slipping away.\r\n", i);
    }

    if (is_npc || !PLR_FLAGGED(i, PLR_JUST_DIED)) {
      if (LAST_HEAL(i) > 0)
        LAST_HEAL(i)--;
      else if (LAST_HEAL(i) < 0)
        LAST_HEAL(i)++;
      if (GET_EQ(i, WEAR_PATCH))
        remove_patch(i);
    }
  }

  /* save shop order updates */
  save_shop_orders();

  /* update object counters */
  ObjList.UpdateCounters();
}

vnum_t junkyard_room_numbers[] = {
  RM_JUNKYARD_GATES, // Just Inside the Gates
  RM_JUNKYARD_PARTS, // Rounding a Tottering Pile of Drone Parts
  RM_JUNKYARD_GLASS, // Beside a Mound of Glass
  RM_JUNKYARD_APPLI, // Amidst Aging Appliances
  RM_JUNKYARD_ELECT  // The Electronics Scrapheap
};

// Returns TRUE if vehicle is in one of the Junkyard vehicle-depositing rooms, FALSE otherwise.
bool veh_is_in_junkyard(struct veh_data *veh) {
  if (!veh || !veh->in_room)
    return FALSE;

  for (int index = 0; index < NUM_JUNKYARD_ROOMS; index++)
    if (veh->in_room->number == junkyard_room_numbers[index])
      return TRUE;

  return FALSE;
}

bool should_save_this_vehicle(struct veh_data *veh) {
  // Must be a PC vehicle. We specifically don't check for PC exist-- that's handled in LOADING, not saving.
  if (veh->owner <= 0)
    return FALSE;

  /* log_vfprintf("Evaluating save info for PC veh '%s' at '%ld' (damage %d/10)",
               GET_VEH_NAME(veh), get_veh_in_room(veh) ? get_veh_in_room(veh)->number : -1, veh->damage); */

  // If it's thrashed, it must be in the proper location.
  if (veh->damage >= VEH_DAM_THRESHOLD_DESTROYED) {
    // It was in the Junkyard and nobody came to fix it.
    if (veh_is_in_junkyard(veh))
      return FALSE;

    // It's being taken care of.
    if (veh->in_veh || (veh->in_room && ROOM_FLAGGED(veh->in_room, ROOM_GARAGE)))
      return TRUE;

    // It's towed.
    if (!veh->in_veh && !veh->in_room)
      return TRUE;

    // We don't preserve damaged watercraft, etc.
    switch (veh->type) {
      case VEH_CAR:
      case VEH_BIKE:
      case VEH_DRONE:
      case VEH_TRUCK:
        break;
      default:
        return FALSE;
    }
  }

  return TRUE;
}

void save_vehicles(bool fromCopyover)
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct veh_data *veh;
  FILE *fl;
  struct char_data *i;
  int v;
  struct room_data *temp_room = NULL;
  struct obj_data *obj;
  int num_veh = 0;

  for (veh = veh_list, v = 0; veh; veh = veh->next) {
    // Skip disqualified vehicles.
    if (!should_save_this_vehicle(veh))
      continue;

    num_veh++;

    bool send_veh_to_junkyard = FALSE;
    if (veh->damage >= VEH_DAM_THRESHOLD_DESTROYED && !(fromCopyover || veh->in_veh || (veh->in_room && ROOM_FLAGGED(veh->in_room, ROOM_GARAGE)))) {
      // If we've gotten here, the vehicle is not in the Junkyard-- save it and send it there.
      send_veh_to_junkyard = TRUE;
    }

    /* Disabling this code-- we want to save ownerless vehicles so that they can disgorge their contents when they load in next.
    if (!does_player_exist(veh->owner)) {
      veh->owner = 0;
      continue;
    }
     */

    snprintf(buf, sizeof(buf), "veh/%07d", v);
    v++;
    if (!(fl = fopen(buf, "w"))) {
      mudlog("SYSERR: Can't Open Vehicle File For Write.", NULL, LOG_SYSLOG, FALSE);
      return;
    }

    if (veh->sub)
      for (i = character_list; i; i = i->next)
        if (GET_IDNUM(i) == veh->owner) {
          struct veh_data *f = NULL;
          for (f = i->char_specials.subscribe; f; f = f->next_sub)
            if (f == veh)
              break;
          if (!f) {
            veh->next_sub = i->char_specials.subscribe;

            // Doubly link it into the list.
            if (i->char_specials.subscribe)
              i->char_specials.subscribe->prev_sub = veh;

            i->char_specials.subscribe = veh;
          }
          break;
        }

    if (send_veh_to_junkyard) {
      // Pick a spot and put the vehicle there. Sort roughly based on type.
      int junkyard_number;
      switch (veh->type) {
        case VEH_DRONE:
          // Drones in the drone spot.
          junkyard_number = RM_JUNKYARD_PARTS;
          break;
        case VEH_BIKE:
          // Bikes in the bike spot.
          junkyard_number = RM_JUNKYARD_BIKES;
          break;
        case VEH_CAR:
        case VEH_TRUCK:
          // Standard vehicles just inside the gates.
          junkyard_number = RM_JUNKYARD_GATES;
          break;
        default:
          // Pick a random one to scatter them about.
          junkyard_number = junkyard_room_numbers[number(0, NUM_JUNKYARD_ROOMS - 1)];
          break;
      }
      temp_room = &world[real_room(junkyard_number)];
    } else {
      // If veh is not in a garage (or the owner is not allowed to enter the house anymore), send it to a garage.
      temp_room = get_veh_in_room(veh);

      // No temp room means it's towed. Yolo it into the Seattle garage.
      if (!temp_room) {
        // snprintf(buf, sizeof(buf), "Falling back to Seattle garage for non-veh, non-room veh %s.", GET_VEH_NAME(veh));
        // log(buf);
        temp_room = &world[real_room(RM_SEATTLE_PARKING_GARAGE)];
      }

      // Otherwise, derive the garage from its location.
      else if (!fromCopyover
              && (!ROOM_FLAGGED(temp_room, ROOM_GARAGE)
              || (ROOM_FLAGGED(temp_room, ROOM_HOUSE)
              && !House_can_enter_by_idnum(veh->owner, temp_room->number))))
      {
       /* snprintf(buf, sizeof(buf), "Falling back to a garage for non-garage-room veh %s (in '%s' %ld).",
                   GET_VEH_NAME(veh), GET_ROOM_NAME(temp_room), GET_ROOM_VNUM(temp_room));
       log(buf); */
        switch (GET_JURISDICTION(temp_room)) {
          case ZONE_PORTLAND:
            switch (number(0, 2)) {
              case 0:
                temp_room = &world[real_room(RM_PORTLAND_PARKING_GARAGE)];
                break;
              case 1:
                temp_room = &world[real_room(RM_PORTLAND_PARKING_GARAGE)];
                break;
              case 2:
                temp_room = &world[real_room(RM_PORTLAND_PARKING_GARAGE)];
                break;
            }
            break;
          case ZONE_SEATTLE:
            temp_room = &world[real_room(RM_SEATTLE_PARKING_GARAGE)];
            break;
          case ZONE_CARIB:
            temp_room = &world[real_room(RM_CARIB_PARKING_GARAGE)];
            break;
          case ZONE_OCEAN:
            temp_room = &world[real_room(RM_OCEAN_PARKING_GARAGE)];
            break;
        }
      }
    }
    fprintf(fl, "[METADATA]\n");
    fprintf(fl, "\tVersion:\t%d\n", VERSION_VEH_FILE);
    fprintf(fl, "[VEHICLE]\n");
    fprintf(fl, "\tVnum:\t%ld\n", veh_index[veh->veh_number].vnum);
    fprintf(fl, "\tOwner:\t%ld\n", veh->owner);
    fprintf(fl, "\tInRoom:\t%ld\n", temp_room->number);
    fprintf(fl, "\tSubscribed:\t%d\n", veh->sub);
    fprintf(fl, "\tDamage:\t%d\n", veh->damage);
    fprintf(fl, "\tSpare:\t%ld\n", veh->spare);
    fprintf(fl, "\tIdnum:\t%ld\n", veh->idnum);

    rnum_t rnum = real_vehicle(GET_VEH_VNUM(veh));
    if (rnum < 0 || veh->flags != veh_proto[rnum].flags)
      fprintf(fl, "\tFlags:\t%s\n", veh->flags.ToString());
    if (veh->in_veh)
      fprintf(fl, "\tInVeh:\t%ld\n", veh->in_veh->idnum);
    if (veh->restring)
      fprintf(fl, "\tVRestring:\t%s\n", veh->restring);
    if (veh->restring_long)
      fprintf(fl, "\tVRestringLong:$\n%s~\n", cleanup(buf2, veh->restring_long));
    fprintf(fl, "[CONTENTS]\n");
    int o = 0, level = 0;
    std::vector<std::string> obj_strings;
    std::stringstream obj_string_buf;
    for (obj = veh->contents;obj;) {
      if (!IS_OBJ_STAT(obj, ITEM_EXTRA_NORENT)) {
        obj_string_buf << "\t\tVnum:\t" << GET_OBJ_VNUM(obj) << "\n";
        obj_string_buf << "\t\tInside:\t" << level << "\n";
        if (GET_OBJ_TYPE(obj) == ITEM_PHONE)
          for (int x = 0; x < 4; x++)
            obj_string_buf << "\t\tValue " << x << ":\t" << GET_OBJ_VAL(obj, x) <<"\n";
        else if (GET_OBJ_TYPE(obj) != ITEM_WORN)
          for (int x = 0; x < NUM_VALUES; x++)
            obj_string_buf << "\t\tValue " << x << ":\t" << GET_OBJ_VAL(obj, x) << "\n";

        obj_string_buf << "\t\tCondition:\t" << (int) GET_OBJ_CONDITION(obj) << "\n";
        obj_string_buf << "\t\tCost:\t" << GET_OBJ_COST(obj) << "\n";
        obj_string_buf << "\t\tTimer:\t" << GET_OBJ_TIMER(obj) << "\n";
        obj_string_buf << "\t\tAttempt:\t" << GET_OBJ_ATTEMPT(obj) << "\n";
        obj_string_buf << "\t\tExtraFlags:\t" << GET_OBJ_EXTRA(obj).ToString() << "\n";
        obj_string_buf <<  "\t\tFront:\t" << obj->vfront << "\n";
        if (obj->restring)
          obj_string_buf << "\t\tName:\t" << obj->restring << "\n";
        if (obj->photo)
          obj_string_buf << "\t\tPhoto:$\n" << cleanup(buf2, obj->photo) << "~\n";

        obj_strings.push_back(obj_string_buf.str());
        obj_string_buf.str(std::string());
      }

      if (obj->contains && !IS_OBJ_STAT(obj, ITEM_EXTRA_NORENT) && GET_OBJ_TYPE(obj) != ITEM_PART) {
        obj = obj->contains;
        level++;
        continue;
      } else if (!obj->next_content && obj->in_obj)
        while (obj && !obj->next_content && level >= 0) {
          obj = obj->in_obj;
          level--;
        }

      if (obj)
        obj = obj->next_content;
    }

    if (!obj_strings.empty()) {
      int i = 0;
      for(std::vector<std::string>::reverse_iterator rit = obj_strings.rbegin(); rit != obj_strings.rend(); rit++ ) {
        fprintf(fl, "\t[Object %d]\n", i);
        fprintf(fl, "%s", rit->c_str());
        i++;
      }
      obj_strings.clear();
    }

    fprintf(fl, "[MODIS]\n");
    for (int x = 0, v = 0; x < NUM_MODS - 1; x++)
      if (GET_MOD(veh, x)) {
        fprintf(fl, "\tMod%d:\t%ld\n", v, GET_OBJ_VNUM(GET_MOD(veh, x)));
        v++;
      }
    fprintf(fl, "[MOUNTS]\n");
    int m = 0;
    for (obj = veh->mount; obj; obj = obj->next_content, m++) {
      struct obj_data *ammo = NULL;
      struct obj_data *gun = NULL;

      fprintf(fl, "\t[Mount %d]\n", m);
      fprintf(fl, "\t\tMountNum:\t%ld\n", GET_OBJ_VNUM(obj));
      if ((ammo = get_mount_ammo(obj)) && GET_AMMOBOX_QUANTITY(ammo) > 0) {
        fprintf(fl, "\t\tAmmo:\t%d\n", GET_AMMOBOX_QUANTITY(ammo));
        fprintf(fl, "\t\tAmmoType:\t%d\n", GET_AMMOBOX_TYPE(ammo));
        fprintf(fl, "\t\tAmmoWeap:\t%d\n", GET_AMMOBOX_WEAPON(ammo));
      }
      if ((gun = get_mount_weapon(obj))) {
        fprintf(fl, "\t\tVnum:\t%ld\n", GET_OBJ_VNUM(gun));
        fprintf(fl, "\t\tCondition:\t%d\n", GET_OBJ_CONDITION(gun));
        if (gun->restring)
          fprintf(fl, "\t\tName:\t%s\n", gun->restring);
        for (int x = 0; x < NUM_VALUES; x++)
          fprintf(fl, "\t\tValue %d:\t%d\n", x, GET_OBJ_VAL(gun, x));
      }
    }
    fprintf(fl, "[GRIDGUIDE]\n");
    o = 0;
    for (struct grid_data *grid = veh->grid; grid; grid = grid->next) {
      fprintf(fl, "\t[GRID %d]\n", o++);
      fprintf(fl, "\t\tName:\t%s\n", grid->name);
      fprintf(fl, "\t\tRoom:\t%ld\n", grid->room);
    }
    fclose(fl);
  }

  // Write the count of vehicles to the file.
  if (!(fl = fopen("veh/vfile", "w"))) {
    mudlog("SYSERR: Can't Open Vehicle File For Write.", NULL, LOG_SYSLOG, FALSE);
    return;
  }
  fprintf(fl, "%d\n", num_veh);
  fclose(fl);
}

void update_paydata_market() {
  FILE *fl;

  // Update paydata markets.
  if (!(fl = fopen("etc/consist", "w"))) {
    mudlog("SYSERR: Can't Open Consistency File For Write.", NULL, LOG_SYSLOG, FALSE);
    return;
  }

  strncpy(buf, "Updating paydata markets:", sizeof(buf) - 1);
  bool something_changed = FALSE;
  UNUSED(something_changed);

  for (int m = 0; m < 5; m++) {
    int old_market = market[m];
    int proposed_market = market[m] + number(MIN_PAYDATA_MARKET_INCREASE_PER_TICK, MAX_PAYDATA_MARKET_INCREASE_PER_TICK);
    market[m] = MIN(get_paydata_market_maximum(m), proposed_market);
    if (market[m] < get_paydata_market_minimum(m))
      market[m] = get_paydata_market_minimum(m);

    if (market[m] != old_market) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %s^g: %d to %d.", host_sec[m], old_market, market[m]);
      something_changed = TRUE;
    }
    else {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %s^g: stays at %d.", host_sec[m], market[m]);
    }
  }

#ifdef LOG_PAYDATA_MARKET_CHANGES
  if (something_changed)
    mudlog(buf, NULL, LOG_ECONLOG, TRUE);
#endif

  fprintf(fl, "[MARKET]\r\n");
  fprintf(fl, "\tBlue:\t%d\n", market[0]);
  fprintf(fl, "\tGreen:\t%d\n", market[1]);
  fprintf(fl, "\tOrange:\t%d\n", market[2]);
  fprintf(fl, "\tRed:\t%d\n", market[3]);
  fprintf(fl, "\tBlack:\t%d\n", market[4]);
  fclose(fl);
}

void misc_update(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct char_data *ch, *next_ch;
  struct obj_data *obj, *o = NULL;
  int i;

  // loop through all the characters
  for (ch = character_list; ch; ch = next_ch) {

    next_ch = ch->next;

    if (AFF_FLAGGED(ch, AFF_FEAR)) {
      if (!number(0, 2)) {
        extern ACMD_CONST(do_flee);
        do_flee(ch, "", 0, 0);
        continue;
      } else {
        AFF_FLAGS(ch).RemoveBit(AFF_FEAR);
        send_to_char("You feel calmer.\r\n", ch);
      }
    }

    if (!CH_IN_COMBAT(ch) && AFF_FLAGGED(ch, AFF_ACID))
      AFF_FLAGS(ch).RemoveBit(AFF_ACID);

    if (GET_SUSTAINED_NUM(ch) && !IS_ANY_ELEMENTAL(ch)) {
      struct sustain_data *next, *temp, *nsus;
      for (struct sustain_data *sus = GET_SUSTAINED(ch); sus; sus = next) {
        next = sus->next;
        if (sus->spell >= MAX_SPELLS) {
          snprintf(buf, sizeof(buf), "COWS GO MOO %s %d", GET_CHAR_NAME(ch), sus->spell);
          log(buf);
          continue;
        }
        if (sus && sus->caster && spells[sus->spell].duration == PERMANENT) {
          int time_to_take_effect = sus->time_to_take_effect;
          if (sus->spell == SPELL_IGNITE) {
            // no-op
          } else if (sus->spell == SPELL_TREAT) {
            time_to_take_effect *= 2.5;
          } else {
            time_to_take_effect *= 5;
          }

          if (++sus->time >= time_to_take_effect) {
            if (sus->spell == SPELL_IGNITE) {
              send_to_char("Your body erupts in flames!\r\n", sus->other);
              act("$n's body suddenly bursts into flames!\r\n", TRUE, sus->other, 0, 0, TO_ROOM);
              GET_CHAR_FIRE_DURATION(sus->other) = srdice();
              GET_CHAR_FIRE_BONUS_DAMAGE(sus->other) = 0;
              GET_CHAR_FIRE_CAUSED_BY_PC(sus->other) = !IS_NPC(ch);
            } else {
              strcpy(buf, spells[sus->spell].name);
              send_to_char(ch, "The effects of %s become permanent.\r\n", buf);
            }
            GET_SUSTAINED_NUM(ch)--;
            if (next && sus->idnum == next->idnum)
              next = next->next;
            if (sus->other)
              for (struct sustain_data *vsus = GET_SUSTAINED(sus->other); vsus; vsus = nsus) {
                nsus = vsus->next;
                if (vsus->other == ch && vsus->idnum == sus->idnum && (sus->other == ch ? vsus != sus : 1)) {
                  REMOVE_FROM_LIST(vsus, GET_SUSTAINED(sus->other), next);
                  delete vsus;
                }
              }
            REMOVE_FROM_LIST(sus, GET_SUSTAINED(ch), next);
            delete sus;
          }
        }
      }
    }

    if (affected_by_spell(ch, SPELL_CONFUSION) || affected_by_spell(ch, SPELL_CHAOS) || affected_by_power(ch, CONFUSION)) {
      if ((i = number(1, 15)) >= 5) {
        switch(i) {
          case 5:
            send_to_char("Lovely weather today.\r\n", ch);
            break;
          case 6:
            send_to_char("Is that who I think it is? ...Nah, my mistake.\r\n", ch);
            break;
          case 7:
            send_to_char("Now, where did I leave my car keys...\r\n", ch);
            break;
          case 8:
            send_to_char("Over There!\r\n", ch);
            break;
          case 9:
            send_to_char("x + 2dy divided by 3 is... no wait CARRY THE 1!\r\n", ch);
            break;
          case 10:
            send_to_char("Skibby dibby dibby do-ah.\r\n", ch);
            break;
          case 11:
            if (ch->carrying)
              send_to_char(ch, "You stare blankly at %s. What is it? What could it mean?\r\n", GET_OBJ_NAME(ch->carrying));
            break;
          case 12:
            send_to_char("The energies of the chaos spell continue to swirl around you.\r\n", ch);
            break;
          case 13:
            send_to_char("You struggle to concentrate through the haze of the chaos spell.\r\n", ch);
            break;
          case 14:
            send_to_char("The chaos spell drags your attention away from what you're doing.\r\n", ch);
            break;
        }
      }
      if (number(0, 30)) {
        // TODO: Make the chaos spell wear off.
      }
    }

    if (!IS_NPC(ch)) {
      // Burn down adrenaline. This can kill the target, so break out if it returns true.
      if (check_adrenaline(ch, 0))
        continue;

      // Apply new doses of everything. If they die, bail out.
      if (process_drug_point_update_tick(ch))
        continue;

      affect_total(ch);
    }
    else { // NPC checks.
      if (!ch->desc && GET_MOB_VNUM(ch) >= 20 && GET_MOB_VNUM(ch) <= 22) {
        act("$n dissolves into the background and is no more.",
            TRUE, ch, 0, 0, TO_ROOM);
        for (i = 0; i < NUM_WEARS; i++)
          if (ch->equipment[i])
            extract_obj(ch->equipment[i]);
        for (obj = ch->carrying; obj; obj = o) {
          o = obj->next_content;
          extract_obj(obj);
        }
        GET_PLAYER_MEMORY(ch) = NULL;
        extract_char(ch);
        continue;
      }
      else if (!ch->desc && GET_MOB_VNUM(ch) >= 50 && GET_MOB_VNUM(ch) < 70) {
        extract_char(ch);
        continue;
      }
      else if (IS_SPIRIT(ch)) {
        if (!check_spirit_sector(ch->in_room, GET_SPARE1(ch))) {
          act("Being away from its environment, $n suddenly ceases to exist.", TRUE, ch, 0, 0, TO_ROOM);
          end_spirit_existance(ch, FALSE);
          continue;
        }
      }
    }

    if (GET_CHAR_FIRE_DURATION(ch) > 0) {
      GET_CHAR_FIRE_DURATION(ch)--;
      // If you're outside and it's raining, you get less fire.
      if (ch->in_room) {
        // Outdoors in the rain? Fire goes out faster.
        if (ch->in_room->sector_type != SPIRIT_HEARTH && !ROOM_FLAGGED(ch->in_room, ROOM_INDOORS) && weather_info.sky >= SKY_RAINING) {
          GET_CHAR_FIRE_DURATION(ch) -= 2;
        }

        // In water? Fire goes out much faster.
        if (IS_WATER(ch->in_room)) {
          GET_CHAR_FIRE_DURATION(ch) -= 4;
        }
      }

      if (GET_CHAR_FIRE_DURATION(ch) < 1) {
        act("The flames around $n die down.", FALSE, ch, 0, 0, TO_ROOM);
        act("The flames surrounding you die down.", FALSE, ch, 0, 0, TO_CHAR);
        GET_CHAR_FIRE_DURATION(ch) = 0;
        GET_CHAR_FIRE_CAUSED_BY_PC(ch) = FALSE;
        continue;
      }

      act("Flames continue to burn around $n!", FALSE, ch, 0, 0, TO_ROOM);
      act("^RYour body is surrounded in flames!", FALSE, ch, 0, 0, TO_CHAR);

      // Only damage equipment in PvE scenarios.
      if (IS_NPC(ch) || !GET_CHAR_FIRE_CAUSED_BY_PC(ch)) {
        damage_equip(ch, ch, 6 + GET_CHAR_FIRE_BONUS_DAMAGE(ch), TYPE_FIRE);
      }

      int dam = convert_damage(stage(-success_test(GET_BOD(ch) + GET_BODY(ch) + GET_POWER(ch, ADEPT_TEMPERATURE_TOLERANCE), 6 + GET_CHAR_FIRE_BONUS_DAMAGE(ch) - GET_IMPACT(ch)), MODERATE));
      GET_CHAR_FIRE_BONUS_DAMAGE(ch)++;
      if (damage(ch, ch, dam, TYPE_SUFFERING, PHYSICAL))
        continue;
    }
  }
}

float gen_size(int race, bool height, int size, int sex)
{
  float mod;
  switch (size) {
    case 1:
      mod = 0.75;
      break;
    case 2:
      mod = 0.88;
      break;
    case 3:
    default:
      mod = 1;
      break;
    case 4:
      mod = 1.13;
      break;
    case 5:
      mod = 1.25;
      break;

  }
  switch (race) {
    case RACE_HUMAN:
      if (sex == SEX_MALE) {
        if (height)
          return number(160, 187) * mod;
        else
          return number(65, 77) * mod;
      } else {
        if (height)
          return number(145, 175) * mod;
        else
          return number(56, 69) * mod;
      }
      break;
    case RACE_DWARF:
      if (sex == SEX_MALE) {
        if (height)
          return number(115, 133) * mod;
        else
          return number(50, 62) * mod;
      } else {
        if (height)
          return number(80, 115) * mod;
        else
          return number(45, 56) * mod;
      }
      break;
    case RACE_ELF:
      if (sex == SEX_MALE) {
        if (height)
          return number(180, 205) * mod;
        else
          return number(70, 82) * mod;
      } else {
        if (height)
          return number(175, 195) * mod;
        else
          return number(60, 75) * mod;
      }
      break;
    case RACE_ORK:
      if (sex == SEX_MALE) {
        if (height)
          return number(185, 210) * mod;
        else
          return number(90, 105) * mod;
      } else {
        if (height)
          return number(178, 195) * mod;
        else
          return number(90, 105) * mod;
      }
      break;
    case RACE_TROLL:
      if (sex == SEX_MALE) {
        if (height)
          return number(270, 295) * mod;
        else
          return number(215, 245) * mod;

      } else {
        if (height)
          return number(255, 280) * mod;
        else
          return number(200, 230) * mod;
      }
      break;
    case RACE_CYCLOPS:
      if (sex == SEX_MALE) {
        if (height)
          return number(290, 340) * mod;
        else
          return number(240, 350) * mod;
      } else {
        if (height)
          return number(275, 320) * mod;
        else
          return number(220, 340) * mod;
      }
      break;
    case RACE_KOBOROKURU:
      if (sex == SEX_MALE) {
        if (height)
          return number(115, 133) * mod;
        else
          return number(50, 62) * mod;
      } else {
        if (height)
          return number(80, 112) * mod;
        else
          return number(45, 56) * mod;
      }
      break;
    case RACE_FOMORI:
      if (sex == SEX_MALE) {
        if (height)
          return number(270, 295) * mod;
        else
          return number(215, 245) * mod;
      } else {
        if (height)
          return number(255, 280) * mod;
        else
          return number(200, 230) * mod;
      }
      break;
    case RACE_MENEHUNE:
      if (sex == SEX_MALE) {
        if (height)
          return number(115, 133) * mod;
        else
          return number(50, 62) * mod;
      } else {
        if (height)
          return number(80, 112) * mod;
        else
          return number(45, 56) * mod;
      }
      break;
    case RACE_HOBGOBLIN:
      if (sex == SEX_MALE) {
        if (height)
          return number(185, 210) * mod;
        else
          return number(90, 105) * mod;
      } else {
        if (height)
          return number(178, 195) * mod;
        else
          return number(85, 95) * mod;
      }
      break;
    case RACE_GIANT:
      if (sex == SEX_MALE) {
        if (height)
          return number(300, 450) * mod;
        else
          return number(380, 477) * mod;
      } else {
        if (height)
          return number(296, 369) * mod;
        else
          return number(380, 430) * mod;
      }
      break;
    case RACE_GNOME:
      if (sex == SEX_MALE) {
        if (height)
          return number(85, 137) * mod;
        else
          return number(45, 59) * mod;
      } else {
        if (height)
          return number(75, 95) * mod;
        else
          return number(39, 52) * mod;
      }
      break;
    case RACE_ONI:
      if (sex == SEX_MALE) {
        if (height)
          return number(185, 215) * mod;
        else
          return number(90, 105) * mod;
      } else {
        if (height)
          return number(178, 195) * mod;
        else
          return number(80, 95) * mod;
      }
      break;
    case RACE_WAKYAMBI:
      if (sex == SEX_MALE) {
        if (height)
          return number(180, 205) * mod;
        else
          return number(70, 82) * mod;
      } else {
        if (height)
          return number(175, 195) * mod;
        else
          return number(60, 75) * mod;
      }
      break;
    case RACE_OGRE:
      if (sex == SEX_MALE) {
        if (height)
          return number(185, 235) * mod;
        else
          return number(90, 105) * mod;
      } else {
        if (height)
          return number(175, 195) * mod;
        else
          return number(85, 96) * mod;
      }
      break;
    case RACE_MINOTAUR:
      if (sex == SEX_MALE) {
        if (height)
          return number(200, 255) * mod;
        else
          return number(100, 145) * mod;
      } else {
        if (height)
          return number(145, 180) * mod;
        else
          return number(95, 120) * mod;
      }
      break;
    case RACE_SATYR:
      if (sex == SEX_MALE) {
        if (height)
          return number(180, 217) * mod;
        else
          return number(90, 105) * mod;
      } else {
        if (height)
          return number(175, 195) * mod;
        else
          return number(80, 95) * mod;
      }
      break;
    case RACE_NIGHTONE:
      if (sex == SEX_MALE) {
        if (height)
          return number(185, 227) * mod;
        else
          return number(90, 108) * mod;
      } else {
        if (height)
          return number(180, 188) * mod;
        else
          return number(80, 91) * mod;
      }
      break;
    case RACE_DRAGON:
      if (sex == SEX_MALE) {
        if (height)
          return number(300, 400) * mod;
        else
          return number(1900, 2100) * mod;
      } else {
        if (height)
          return number(400, 500) * mod;
        else
          return number(1950, 2300) * mod;
      }
      break;
    default:
      if (sex == SEX_MALE) {
        if (height)
          return number(160, 187) * mod;
        else
          return number(65, 77) * mod;
      } else {
        if (height)
          return number(145, 175) * mod;
        else
          return number(56, 69) * mod;
      }
  }
}
