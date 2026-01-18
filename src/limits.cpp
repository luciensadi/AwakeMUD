
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
#include "newhouse.hpp"
#include "house.hpp"
#include "gmcp.hpp"

extern class objList ObjList;
extern int modify_target(struct char_data *ch);
extern void end_quest(struct char_data *ch, bool succeeded);
extern void damage_equip(struct char_data *ch, struct char_data *victim, int power, int type);
extern bool check_adrenaline(struct char_data *, int);
extern int get_paydata_market_maximum(int host_color);
extern int get_paydata_market_minimum(int host_color);
extern void save_shop_orders();
extern bool docwagon(struct char_data *ch);
extern struct time_info_data time_info;

void mental_gain(struct char_data * ch)
{
  int gain = 0;

  // Not injured? Skip.
  if (GET_MENTAL(ch) == GET_MAX_MENTAL(ch)) {
    return;
  }

  // Can't regenerate? Skip.
  if (IS_NPC(ch) && GET_DEFAULT_POS(ch) == POS_STUNNED) {
    return;
  }

  bool is_npc = IS_NPC(ch);

  if (is_npc && ch->desc && ch->desc->original && PLR_FLAGS(ch->desc->original).IsSet(PLR_PROJECT) && GET_MOB_VNUM(ch) == 22) {
    /* aka an optimized IS_PROJECT(ch) */
    return;
  }

  // Base recovery
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

  // Character related bonuses
  if (is_npc) {
    // NPCs get enhanced gain with none of the extra calculations.
    gain *= 2.5;
  } else {
    // For PCs, we have to calculate.

    // Augmentations
    for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content) {
      if (GET_BIOWARE_TYPE(bio) == BIO_SYMBIOTES) {
        switch (GET_BIOWARE_RATING(bio)) {
          case 1:
            gain *= 1.1;
            break;
          case 2:
            gain *= 1.4;
            break;
          case 3:
            gain *= 2;
            break;
        }
        break;
      }
    }

    // Adept rapid healing increases recovery by +50% per rank
    if (GET_TRADITION(ch) == TRAD_ADEPT && GET_POWER(ch, ADEPT_HEALING) > 0)
      gain *= (((float) GET_POWER(ch, ADEPT_HEALING) / 2) + 1);

    // Lifestyle boost: The better-fed and better-rested you are, the more you heal.
    gain *= 1 + MAX(0.0, 0.1 * (GET_BEST_LIFESTYLE(ch) - LIFESTYLE_SQUATTER));

    // Room related bonuses
    if (find_workshop(ch, TYPE_MEDICAL))
      gain *= 1.5;

    if (char_is_in_social_room(ch))
      gain *= 2;

    if (ROOM_FLAGGED(ch->in_room, ROOM_STERILE)) {
      if (GET_APARTMENT(ch->in_room)) {
        // To get the benefit, the apartment must be owned with a valid lease, and you must be the owner or a guest.
        if (GET_APARTMENT(ch->in_room)->is_owner_or_guest_with_valid_lease(ch)) {
          gain *= 1.5;
        }
      } else {
        gain *= 1.5;
      }
    }

    // Penalties happen last, to avoid the possibility of truncating to zero too early
    if (PLR_FLAGS(ch).IsSet(PLR_ENABLED_DRUGS)) {
      // PCs have drug heal multipliers, NPCs don't
      gain *= get_drug_heal_multiplier(ch);
    }

    // Biosystem overstress reduces healing rate by 10% per point
    if (GET_BIOOVER(ch) > 0)
      gain *= MIN(1.0, MAX(0.1, 1 - (0.1 * GET_BIOOVER(ch))));
  }

#ifdef ENABLE_HUNGER
  if ((GET_COND(ch, COND_FULL) == MIN_FULLNESS) || (GET_COND(ch, COND_THIRST) == MIN_QUENCHED))
    gain >>= 1;
#endif

  // Prevent reaching 0 gain
  gain = MAX(1, gain);
  GET_MENTAL(ch) = MIN(GET_MAX_MENTAL(ch), GET_MENTAL(ch) + gain);
  update_pos(ch);
}

void physical_gain(struct char_data * ch)
{
  // Not injured? Skip.
  if (GET_PHYSICAL(ch) == GET_MAX_PHYSICAL(ch)) {
    return;
  }

  // Can't regenerate? Skip.
  if (IS_NPC(ch) && GET_DEFAULT_POS(ch) == POS_MORTALLYW) {
    return;
  }

  int gain = 0;
  bool is_npc = IS_NPC(ch);

  if (is_npc && ch->desc && ch->desc->original && PLR_FLAGS(ch->desc->original).IsSet(PLR_PROJECT) && GET_MOB_VNUM(ch) == 22) {
    /* aka an optimized IS_PROJECT(ch) */
    return;
  }

  // Base recovery
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

  // Character related bonuses

  if (is_npc) {
    // NPCs are easy: boost them by a set amount.
    gain *= 2.5;
  } else {
    // PCs have more checks.

    // Augmentations
    for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content) {
      if (GET_BIOWARE_TYPE(bio) == BIO_SYMBIOTES) {
        switch (GET_BIOWARE_RATING(bio)) {
          case 1:
            gain *= 1.1;
            break;
          case 2:
            gain *= 1.4;
            break;
          case 3:
            gain *= 2;
            break;
        }
        break;
      }
    }

    // Adept rapid healing increases recovery by +50% per rank
    if (GET_TRADITION(ch) == TRAD_ADEPT && GET_POWER(ch, ADEPT_HEALING) > 0)
      gain *= (((float) GET_POWER(ch, ADEPT_HEALING) / 2) + 1);

    // Lifestyle boost: The better-fed and better-rested you are, the more you heal.
    gain *= 1 + MAX(0.0, 0.1 * (GET_BEST_LIFESTYLE(ch) - LIFESTYLE_SQUATTER));
      
    // Room related bonuses
    if (find_workshop(ch, TYPE_MEDICAL))
      gain *= 1.8;

    if (char_is_in_social_room(ch))
      gain *= 2;

    if (ROOM_FLAGGED(ch->in_room, ROOM_STERILE)) {
      if (GET_APARTMENT(ch->in_room)) {
        // To get the benefit, the apartment must be owned with a valid lease, and you must be the owner or a guest.
        if (GET_APARTMENT(ch->in_room)->is_owner_or_guest_with_valid_lease(ch)) {
          gain *= 1.8;
        }
      } else {
        gain *= 1.8;
      }
    }

    // Penalties happen last, to avoid the possibility of truncating to zero too early
    if (PLR_FLAGS(ch).IsSet(PLR_ENABLED_DRUGS)) {
      // PCs have drug heal multipliers, NPCs don't
      gain *= get_drug_heal_multiplier(ch);
    }

    // Biosystem overstress reduces healing rate by 10% per point
    if (GET_BIOOVER(ch) > 0)
      gain *= MIN(1.0, MAX(0.1, 1 - (0.1 * GET_BIOOVER(ch))));
  }

#ifdef ENABLE_HUNGER
  if ((GET_COND(ch, COND_FULL) == MIN_FULLNESS) || (GET_COND(ch, COND_THIRST) == MIN_QUENCHED))
    gain >>= 1;
#endif

  // Prevent reaching 0 gain
  gain = MAX(1, gain);
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

void set_title(struct char_data * ch, const char *title, bool save_to_db)
{
  DELETE_ARRAY_IF_EXTANT(GET_TITLE(ch));
  GET_TITLE(ch) = get_new_kosherized_title(title, MAX_TITLE_LENGTH);

  if (save_to_db) {
    char query_buf[MAX_INPUT_LENGTH];
    char prepare_quotes_buf[MAX_INPUT_LENGTH];
    snprintf(query_buf, sizeof(query_buf),
            "UPDATE pfiles SET Title='%s' WHERE idnum=%ld;",
            prepare_quotes(prepare_quotes_buf, GET_TITLE(ch), sizeof(prepare_quotes_buf) / sizeof(prepare_quotes_buf[0])), GET_IDNUM(ch));
    mysql_wrapper(mysql, query_buf);
  }
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

// Returns the amount after garnishment.
long garnish_karma(struct char_data *ch, long amount, bool is_rep) {
  // If they have a garnishment, they lose 20% of the gain. This doesn't hit bank withdrawal, wires, or player handoffs.
  // Note to the devious: If you decide to bypass garnishment by use of other players, you'll be escalating your garnishment to a ban.
  long *garnishment_var = &(is_rep ? GET_GARNISHMENT_REP(ch) : GET_GARNISHMENT_NOTOR(ch));
  if (*garnishment_var > 0) {
    long garnished_amount = MIN(*garnishment_var, amount * 0.25);
    amount -= garnished_amount;
    *garnishment_var -= garnished_amount;
    if (*garnishment_var <= 0) {
      *garnishment_var = 0;
      send_to_char(ch, "^gOOC Notice:^n Your %s garnishment has been paid off.\r\n", is_rep ? "reputation" : "notoriety");
      mudlog_vfprintf(ch, LOG_WIZLOG, "%s(%ld) has paid off their %s garnishment.", GET_CHAR_NAME(ch), GET_IDNUM(ch), is_rep ? "reputation" : "notoriety");
    }
  }
  return amount;
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

  // We only suppress idle payouts for people who are able to move.
  if (ch->char_specials.timer >= IDLE_TIMER_PAYOUT_THRESHOLD && GET_POS(ch) > POS_STUNNED) {
    send_to_char("Your karma gain was suppressed due to you being idle.\r\n", ch);
    return 0;
  }

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
        gain = MIN((int) (MAX_OLDCHAR_GAIN), gain);
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

    // Garnish it if needed.
    gain = garnish_karma(ch, gain, rep);

    GET_KARMA(ch) += gain;

    GET_TKE(ch) += (int)(GET_KARMA(ch) / 100) - old;

    if (rep)
      GET_REP(ch) += (int)(GET_KARMA(ch) / 100) - old;
    else
      GET_NOT(ch) += (int)(GET_KARMA(ch) / 100) - old;

    SendGMCPCharInfo(ch);
    SendGMCPCharVitals(ch);
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
  if (credstick) {
    GET_ITEM_MONEY_VALUE(credstick) += amount;
    if (GET_ITEM_MONEY_VALUE(credstick) < 0) {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Deducted more from credstick than it had on it! _raw_gain_nuyen(%s, %ld, %d, %s, %s)",
                      GET_CHAR_NAME(ch),
                      amount,
                      category,
                      bank ? "BANK" : "F",
                      GET_OBJ_NAME(credstick));
      GET_ITEM_MONEY_VALUE(credstick) = 0;
    }
  } else if (bank) {
    GET_BANK_RAW(ch) += amount;
    if (GET_BANK_RAW(ch) < 0) {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Deducted more from bank than was available! _raw_gain_nuyen(%s, %ld, %d, %s, %s)",
                      GET_CHAR_NAME(ch),
                      amount,
                      category,
                      bank ? "BANK" : "F",
                      GET_OBJ_NAME(credstick));
      GET_BANK_RAW(ch) = 0;
    }
  } else {
    GET_NUYEN_RAW(ch) += amount;
    if (GET_NUYEN_RAW(ch) < 0) {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Deducted more from carried cash than was available! _raw_gain_nuyen(%s, %ld, %d, %s, %s)",
                      GET_CHAR_NAME(ch),
                      amount,
                      category,
                      bank ? "BANK" : "F",
                      GET_OBJ_NAME(credstick));
      GET_NUYEN_RAW(ch) = 0;
    }
  }
  SendGMCPCharVitals(ch);
}

// Returns the amount after garnishment.
long garnish_income(struct char_data *ch, long amount) {
  // If they have a garnishment, they lose 20% of the gain. This doesn't hit bank withdrawal, wires, or player handoffs.
  // Note to the devious: If you decide to bypass garnishment by use of other players, you'll be escalating your garnishment to a ban.
  if (!IS_NPC(ch) && GET_GARNISHMENT_NUYEN(ch) > 0) {
    long garnished_amount = MIN(GET_GARNISHMENT_NUYEN(ch), amount * 0.25);
    amount -= garnished_amount;
    GET_GARNISHMENT_NUYEN(ch) -= garnished_amount;
    if (GET_GARNISHMENT_NUYEN(ch) <= 0) {
      GET_GARNISHMENT_NUYEN(ch) = 0;
      send_to_char(ch, "^gOOC Notice:^n Your nuyen garnishment has been paid off.\r\n");
      mudlog_vfprintf(ch, LOG_WIZLOG, "%s has paid off their nuyen garnishment.", GET_CHAR_NAME(ch));
    }
  }
  return amount;
}

long gain_nuyen(struct char_data *ch, long amount, int category) {
  amount = garnish_income(ch, amount);
  _raw_gain_nuyen(ch, amount, category, FALSE, NULL);
  return amount;
}

long lose_nuyen(struct char_data *ch, long amount, int category) {
  _raw_gain_nuyen(ch, -amount, category, FALSE, NULL);
  return amount;
}

long gain_bank(struct char_data *ch, long amount, int category) {
  amount = garnish_income(ch, amount);
  _raw_gain_nuyen(ch, amount, category, TRUE, NULL);
  return amount;
}

long lose_bank(struct char_data *ch, long amount, int category) {
  _raw_gain_nuyen(ch, -amount, category, TRUE, NULL);
  return amount;
}

// only the pcs should need to access this
void gain_condition(struct char_data * ch, int condition, int value)
{
  bool intoxicated;
  struct obj_data *bio;

  if (GET_COND(ch, condition) == -1)    /* No change */
    return;

  intoxicated = (GET_COND(ch, COND_DRUNK) > MIN_DRUNK) && !affected_by_spell(ch, SPELL_DETOX);

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
      GET_MENTAL(ch) = MAX(0, GET_MENTAL(ch) - (GET_PATCH_RATING(patch) - 1) * 100);
      if ((GET_TRADITION(ch) == TRAD_HERMETIC || GET_TRADITION(ch) == TRAD_SHAMANIC) &&
          success_test(GET_MAGIC_POOL(ch), GET_PATCH_RATING(patch)) < 0) {
        magic_loss(ch, 100, TRUE);
        affect_total(ch);
      }
      update_pos(ch);
      break;
    case PATCH_TRANQ:
      stun = resisted_test(GET_OBJ_VAL(patch, 1), GET_REAL_BOD(ch) - (GET_BIOOVER(ch) > 0 ? (GET_BIOOVER(ch) + 1) / 2 : 0),
                           GET_REAL_BOD(ch) - (GET_BIOOVER(ch) > 0 ? (GET_BIOOVER(ch) + 1) / 2 : 0), GET_PATCH_RATING(patch));
      if (stun > 0) {
        act("You feel the drugs from $p take effect.", FALSE, ch, patch, 0, TO_CHAR);
        GET_MENTAL(ch) = MAX(0, GET_MENTAL(ch) - (stun * 100));
        update_pos(ch);
      } else
        act("You resist the feeble effects of $p.", FALSE, ch, patch, 0, TO_CHAR);
      break;
    case PATCH_TRAUMA:
      // Houserule: Trauma patches are generally effective.
      if (success_test(GET_PATCH_RATING(patch), 4) <= 0) {
        send_to_char("The effects of the trauma patch wear off.\r\n", ch);
        AFF_FLAGS(ch).RemoveBit(AFF_STABILIZE);
      }
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
    next = ch->next_in_character_list;

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
            end_quest(ch, FALSE);
          if (ch->desc)
            close_socket(ch->desc);
          ch->desc = NULL;
          snprintf(buf, sizeof(buf), "%s force-rented and extracted (idle).",
                  GET_CHAR_NAME(ch));
          mudlog(buf, ch, LOG_CONNLOG, TRUE);
          extract_char(ch);
        }
        */

      #define VNUM_IS_NERPCORPOLIS(vnum) (vnum >= 6900 && vnum <= 6999)

      if (!ch->desc && !PLR_FLAGGED(ch, PLR_PROJECT) && (ch->char_specials.timer > NUM_MINUTES_BEFORE_LINKDEAD_EXTRACTION || PLR_FLAGGED(ch, PLR_IN_CHARGEN))) {
        // If they're a PC in an apartment that they own, set their loadroom there.
        if (!IS_NPC(ch)) {
          struct room_data *in_room = get_ch_in_room(ch);
          if (in_room && GET_APARTMENT(in_room) && GET_APARTMENT(in_room)->get_owner_id() == GET_IDNUM(ch) && !VNUM_IS_NERPCORPOLIS(GET_ROOM_VNUM(in_room))) {
            GET_LOADROOM(ch) = GET_ROOM_VNUM(in_room);
          }
        }
        snprintf(buf, sizeof(buf), "%s removed from game (no link).", GET_CHAR_NAME(ch));
        mudlog(buf, ch, LOG_CONNLOG, TRUE);
        extract_char(ch);

        // You must either RETURN here or do a while-loop that restarts on removal, because if the next character
        // depends on this one (elemental, quest, etc), it will be deleted memory when referenced.
        return;
      }

      // Auto-invis idle staff.
      if (access_level(ch, LVL_EXECUTIVE) && ch->char_specials.timer > 15 &&
          GET_INVIS_LEV(ch) < 2 &&
          PRF_FLAGGED(ch, PRF_AUTOINVIS))
      {
        perform_immort_invis(ch, 2);
      }
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
      || char_is_in_social_room(ch)
      || AFF_FLAGS(ch).AreAnySet(BR_TASK_AFF_FLAGS, ENDBIT) // See awake.h for the definition of these aff flags.
  ) {
    return FALSE;
  }

  struct obj_data *bio = NULL, *platelets = NULL;
  int synthacardium = 0;
  for (bio = ch->bioware; bio; bio = bio->next_content) {
    // Find the 'ware we care about
    if (GET_BIOWARE_TYPE(bio) == BIO_PLATELETFACTORY) {
      platelets = bio;
    } else if (GET_BIOWARE_TYPE(bio) == BIO_SYNTHACARDIUM) {
      synthacardium = GET_BIOWARE_RATING(bio);
    }

    if (platelets && synthacardium)
      break;
  }

  if (platelets) {
    // Embolism
    if (--GET_BIOWARE_PLATELETFACTORY_DATA(platelets) < 1) {
      GET_BIOWARE_PLATELETFACTORY_DATA(platelets) = 12;
      if (success_test(GET_REAL_BOD(ch) + synthacardium, 3 + GET_BIOWARE_PLATELETFACTORY_DIFFICULTY(platelets)) < 1) {
        send_to_char("Your blood seems to erupt.\r\n", ch);
        act("$n collapses to the floor, twitching.", TRUE, ch, 0, 0, TO_ROOM);
        if (damage(ch, ch, 10, TYPE_BIOWARE, PHYSICAL))
          return TRUE;
      } else {
        send_to_char("Your heart strains, and you have a feeling of impending doom. Your need for blood thinners is dire!\r\n", ch);
      }
      GET_BIOWARE_PLATELETFACTORY_DIFFICULTY(platelets)++;
    }

    // Warning messages start 4 mud hours out
    if (GET_BIOWARE_PLATELETFACTORY_DATA(platelets) == 4)
      send_to_char("You kinda feel like you should be taking some aspirin.\r\n", ch);
    else if (GET_BIOWARE_PLATELETFACTORY_DATA(platelets) == 3)
      send_to_char("You could definitely go for some aspirin right now.\r\n", ch);
    else if (GET_BIOWARE_PLATELETFACTORY_DATA(platelets) == 2)
      send_to_char("You really feel like you need to take some aspirin.\r\n", ch);
    else if (GET_BIOWARE_PLATELETFACTORY_DATA(platelets) == 1)
      send_to_char("Your heart strains, and you have a feeling of impending doom. Your need for blood thinners is dire!\r\n", ch);
  }
  return FALSE;
}

int calculate_swim_successes(struct char_data *ch) {
  int swim_test_target, skill_dice, opposing_dice, successes, water_wings_bonus = 0, levitate_bonus = 0, fin_bonus = 0;

  if (IS_NPC(ch) || IS_SENATOR(ch) || !ch->in_room)
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

  snprintf(buf, sizeof(buf), "%s rolled %d", GET_CHAR_NAME(ch), skill_dice);

  // Levitate always applies.
  if ((levitate_bonus = affected_by_spell(ch, SPELL_LEVITATE))) {
    skill_dice += levitate_bonus;
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " + %d (levitate)", levitate_bonus);
  }

  // Fins only matter if you're conscious and able to use them.
  if (fin_bonus && GET_POS(ch) >= POS_RESTING) {
    skill_dice += fin_bonus;
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " + %d (fins)", fin_bonus);
  }

  opposing_dice = MAX(2, ch->in_room->rating);

  successes = success_test(skill_dice, swim_test_target);
  successes -= success_test(opposing_dice, skill_dice);

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " dice against TN %d (lowered from %d by WW %d), opposed by %d dice at TN %d: %d net success%s.\r\n",
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

  {
    bool should_loop = TRUE;
    int loop_rand = rand();
    int loop_counter = 0;

    while (should_loop) {
      should_loop = FALSE;
      loop_counter++;

      for (struct char_data *ch = character_list; ch; ch = ch->next_in_character_list) {
        bool is_npc = IS_NPC(ch);

        if (ch->last_loop_rand == loop_rand) {
          continue;
        } else {
          ch->last_loop_rand = loop_rand;
        }
        
        // Tick down temp qui loss (nerve strike, etc)
        if (GET_TEMP_QUI_LOSS(ch) > 0) {
          int old_qui = GET_QUI(ch);

          GET_TEMP_QUI_LOSS(ch)--;
          affect_total(ch);

          if (old_qui <= 0 && GET_QUI(ch) > 0) {
            send_to_char("Your muscles unlock, and you find you can move again.\r\n", ch);
          }
        }

        if (GET_POS(ch) >= POS_STUNNED) {
          // Apply healing.
          {
            bool has_pedit = FALSE;
            for (struct obj_data *obj = ch->bioware; obj; obj = obj->next_content) {
              if (GET_BIOWARE_TYPE(obj) == BIO_PAINEDITOR) {
                has_pedit = GET_BIOWARE_IS_ACTIVATED(obj);
                break;
              }
            }

            int old_phys = GET_PHYSICAL(ch), old_ment = GET_MENTAL(ch);

            physical_gain(ch);
            if (!has_pedit && old_phys != GET_MAX_PHYSICAL(ch) && GET_PHYSICAL(ch) == GET_MAX_PHYSICAL(ch)) {
              send_to_char("The last of your injuries has healed.\r\n", ch);
            }

            mental_gain(ch);
            if (!has_pedit && old_ment != GET_MAX_MENTAL(ch)) {
              if (GET_MENTAL(ch) == GET_MAX_MENTAL(ch)) {
                send_to_char("You feel fully alert again.\r\n", ch);
              } else if (old_ment < 100 && GET_MENTAL(ch) >= 100){
                send_to_char("You regain consciousness.\r\n", ch);
              }
            }
          }

          if (!is_npc && IS_WATER(ch->in_room) && half_hour) {
            if (check_swimming(ch)) {
              // They died. Stop evaluating and start again.
              log_vfprintf("process_regeneration(): recycling loop due to check_swimming() death");
              should_loop = TRUE;
              break;
            }
          }

          if (GET_POS(ch) == POS_STUNNED)
            update_pos(ch);

          if (!is_npc && GET_PHYSICAL(ch) >= GET_MAX_PHYSICAL(ch)) {
            if (AFF_FLAGS(ch).IsSet(AFF_HEALED)) {
              AFF_FLAGS(ch).RemoveBit(AFF_HEALED);
              send_to_char("You can now be affected by the Heal spell again.\r\n", ch);
            }
            if (AFF_FLAGS(ch).IsSet(AFF_RESISTPAIN)) {
              AFF_FLAGS(ch).RemoveBit(AFF_RESISTPAIN);
              send_to_char("Your magical pain resistance wears off.\r\n", ch);
              end_all_sustained_spells_of_type_affecting_ch(SPELL_RESISTPAIN, 0, ch);
            }
          }
          if (GET_PHYSICAL(ch) > 0) {
            AFF_FLAGS(ch).RemoveBit(AFF_STABILIZE);
          }
        }

        if (GET_POS(ch) == POS_MORTALLYW && half_hour) {
          bool dam = !AFF_FLAGS(ch).IsSet(AFF_STABILIZE);

          if (dam) {
            // A ticking metabolic arrester prevents damage.
            for (struct obj_data *obj = ch->bioware; obj; obj = obj->next_content) {
              if (GET_BIOWARE_TYPE(obj) == BIO_METABOLICARRESTER) {
                if (++GET_BIOWARE_IS_ACTIVATED(obj) == 5) {
                  // If it's hit 5 ticks, then it resets to 0 and doesn't prevent damage this tick.
                  GET_BIOWARE_IS_ACTIVATED(obj) = 0;
                } else {
                  // Otherwise, prevent.
                  dam = FALSE;
                }
                break;
              }
            }
          }

          if (dam) {
            // Deal a box of physical damage.
            if (damage(ch, ch, 1, TYPE_SUFFERING, PHYSICAL)) {
              // They died. Loop again.
              log_vfprintf("process_regeneration(): recycling loop due to bleedout() death");
              should_loop = TRUE;
              break;
            }
          } else {
            // Otherwise, just check for docwagon pulse.
            docwagon(ch);
          }
        }
      }
    }

    if (loop_counter > 1) {
      log_vfprintf("Ran process_regeneration() %d times due to mid-run alterations and extractions.", loop_counter);
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
    if (world[i].background[CURRENT_BACKGROUND_COUNT] && world[i].background[CURRENT_BACKGROUND_TYPE] >= AURA_PLAYERCOMBAT)
      world[i].background[CURRENT_BACKGROUND_COUNT]--;

    // Re-increment standard auras, but only if there are no player auras in place.
    if (world[i].background[CURRENT_BACKGROUND_TYPE] != AURA_POWERSITE
        && (world[i].background[CURRENT_BACKGROUND_TYPE] < AURA_PLAYERCOMBAT || world[i].background[CURRENT_BACKGROUND_COUNT] <= 0))
    {
      if (world[i].background[CURRENT_BACKGROUND_COUNT] < world[i].background[PERMANENT_BACKGROUND_COUNT])
        world[i].background[CURRENT_BACKGROUND_COUNT]++;
      world[i].background[CURRENT_BACKGROUND_TYPE] = world[i].background[PERMANENT_BACKGROUND_TYPE];
    }
  }
}

/* Update PCs, NPCs, and objects, called every mud hour */
void point_update(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  ACMD_DECLARE(do_use);

  // Generate the wholist file.
  ACMD_CONST(do_who);
  if (character_list)
    do_who(character_list, "", 0, 1);

  /* characters */
  bool is_npc = FALSE;

  {
    bool should_loop = TRUE;
    int loop_rand = rand();
    int loop_counter = 0;

    while (should_loop) {
      should_loop = FALSE;
      loop_counter++;

      for (struct char_data *i = character_list; i; i = i->next_in_character_list) {
        if (i->last_loop_rand == loop_rand) {
          // mudlog("SYSERR: Encountered someone who's already been point_updated. Fix point_update().", i, LOG_SYSLOG, TRUE);
          continue;
        } else {
          i->last_loop_rand = loop_rand;
        }

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

          if (IS_GHOUL(i) || IS_DRAGON(i))
            PLR_FLAGS(i).SetBit(PLR_PERCEIVE);

          // Temp magic loss (banishing) regains 1/hour, SR3 pg 189
          if (GET_TEMP_MAGIC_LOSS(i) > 0) {
            GET_TEMP_MAGIC_LOSS(i)--;
          }

          // Geas check from focus / foci overuse.
          if (GET_MAG(i) > 0) {
            int total = 0;
            int force = get_total_active_focus_rating(i, total);   

            if (GET_REAL_MAG(i) * 2 < 0) {
              mudlog("^RSYSERR: Multiplying magic for focus addiction check gave a NEGATIVE number! Increase the size of the variable!^n", i, LOG_SYSLOG, TRUE);
            } else {
              if (force * 100 > GET_REAL_MAG(i) * 2 && success_test(GET_REAL_MAG(i) / 100, force / 2) < 1) {

    #ifdef USE_OLD_FOCUS_ADDICTION_RULES
                int num = number(1, total);
                struct obj_data *focus_geas = NULL;
                for (int x = 0; x < NUM_WEARS && !focus_geas; x++) {
                  if (!(focus = GET_EQ(i, x)))
                    continue;

                  if (GET_OBJ_TYPE(focus) == ITEM_FOCUS && GET_FOCUS_BONDED_TO(focus) == GET_IDNUM(i) && GET_FOCUS_ACTIVATED(focus)
                      && !GET_FOCUS_BOND_TIME_REMAINING(focus) && !GET_FOCUS_GEAS(focus) && !--num) {
                    focus_geas = focus;
                  }
                  else if ((x == WEAR_WIELD || x == WEAR_HOLD) && GET_OBJ_TYPE(focus) == ITEM_WEAPON && WEAPON_IS_FOCUS(focus)
                      && is_weapon_focus_usable_by(focus, i) && !GET_WEAPON_FOCUS_GEAS(focus) && !--num) {
                    focus_geas = focus;
                  }
                }

                if (focus_geas) {
                  send_to_char(i, "^RYou feel some of your magic becoming locked in %s!^n Quick, take off all your foci before it happens again!\r\n", GET_OBJ_NAME(focus));
                  GET_FOCUS_GEAS(focus_geas) = GET_IDNUM(i);
                  magic_loss(i, 100, FALSE);
                }
    #else
                mudlog_vfprintf(i, LOG_SYSLOG, "Damaging %s and breaking sustained spells due to focus overuse (%d foci; %d > %d).", GET_CHAR_NAME(i), total, force * 100, GET_REAL_MAG(i) * 2);
                send_to_char(i, "^RThe backlash of focus overuse rips through you!^n\r\n");

                // Disarm them.
                {
                  struct obj_data *weap = GET_EQ(i, WEAR_WIELD);
                  if (weap && GET_OBJ_TYPE(weap) == ITEM_WEAPON && !WEAPON_IS_GUN(weap) && GET_WEAPON_FOCUS_RATING(weap) > 0) {
                    send_to_char(i, "Your fingers spasm from the pain, and you almost drop %s!\r\n", GET_OBJ_NAME(weap));
                    unequip_char(i, weap->worn_on, TRUE);
                    obj_to_char(weap, i);
                  }
                }

                // Damage them. This also strips all sustained spells.
                if (damage(i, i, convert_damage(DEADLY) - 1, TYPE_FOCUS_OVERUSE, TRUE)) {
                  // They died? Time to restart the loop.
                  log_vfprintf("point_update(): recycling loop due to old focus addiction death");
                  should_loop = TRUE;
                  break;
                }
    #endif
              }
            }
          }

          if (HUNTING(i) && !AFF_FLAGGED(i, AFF_TRACKING) && ++HOURS_SINCE_TRACK(i) > 8)
            HUNTING(i) = NULL;

          if (i->bioware)
            if (check_bioware(i)) {
              // They died? Time to restart the loop.
              log_vfprintf("point_update(): recycling loop due to check_bioware() death");
              should_loop = TRUE;
              break;
            }

          // Every hour, we check for withdrawal.
          process_withdrawal(i);
        }

        if (i->desc && IS_PROJECT(i)) {
          if (AFF_FLAGGED(i->desc->original, AFF_TRACKING) && HUNTING(i->desc->original) && !--HOURS_LEFT_TRACK(i->desc->original)) {
            if (!HUNTING(i->desc->original)->in_room || !CH_CAN_ENTER_APARTMENT(HUNTING(i->desc->original)->in_room, i)) {
              send_to_char("The astral signature fades... you can't follow it all the way back.\r\n", i);
            } else {
              act("The astral signature leads you to $N.", FALSE, i, 0, HUNTING(i->desc->original), TO_CHAR);
              char_from_room(i);
              char_to_room(i, HUNTING(i->desc->original)->in_room);
              act("$n enters the area.", TRUE, i, 0, 0, TO_ROOM);
            }
            // Clear hunting data structs.
            AFF_FLAGS(i->desc->original).RemoveBit(AFF_TRACKING);
            AFF_FLAGS(HUNTING(i->desc->original)).RemoveBit(AFF_TRACKED);
            HUNTING(i->desc->original) = NULL;
          }

          if ((++GET_PROJECTION_ESSLOSS_TICK(i) % PROJECTION_LENGTH_MULTIPLIER) == 0) {
            GET_ESS(i) -= 100;
            if (i->desc && i->desc->original) {
              // Tick up temporary essence loss.
              GET_TEMP_ESSLOSS(i->desc->original) = GET_ESS(i->desc->original) - GET_ESS(i);
              affect_total(i->desc->original);
            }

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

              // First, nuke their health.
              GET_PHYSICAL(victim) = -50;
              GET_MENTAL(victim) = 0;
              // Next, remove their death saves.
              GET_PC_SALVATION_TICKS(victim) = 0;
              // Finally, deal them massive damage.
              damage(victim, victim, 100, TYPE_SUFFERING, TRUE);

              // Restart the loop: We extracted someone.
              log_vfprintf("point_update(): recycling loop due to projection snapback");
              should_loop = TRUE;
              break;
            } else if (GET_ESS(i) <= 100) {
              send_to_char("^RYou feel memories of your physical body slipping away. Better ^WRETURN^R to it soon...^n\r\n", i);
            } else if (GET_ESS(i) <= 200) {
              send_to_char("^rYour link to your physical form grows tenuous.\r\n", i);
            }
          }
        }

        if (is_npc || !PLR_FLAGGED(i, PLR_JUST_DIED)) {
          if (LAST_HEAL(i) > 0)
            LAST_HEAL(i)--;
          else if (LAST_HEAL(i) < 0)
            LAST_HEAL(i)++;
          if (GET_EQ(i, WEAR_PATCH) && GET_OBJ_TYPE(GET_EQ(i, WEAR_PATCH)) == ITEM_PATCH && --GET_PATCH_TICKS_LEFT(GET_EQ(i, WEAR_PATCH)) <= 0)
            remove_patch(i);
        }

        // Clear their assense records on every MUD day.
        if (time_info.hours == 0) {
          i->assense_recency.clear();
        }
      }

    }
  
    if (loop_counter > 1) {
      log_vfprintf("Ran point_update() %d times due to mid-run alterations and extractions.", loop_counter);
    }
  }

  /* save shop order updates */
  save_shop_orders();

  if (world[0].contents) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Room 0 already contains objects before UpdateCounters(), they will be destroyed now. First content is %s.", GET_OBJ_NAME(world[0].contents));
  }

  /* update object counters */
  ObjList.UpdateCounters();

  // Purge out room 0.
  while (world[0].contents) {
    extract_obj(world[0].contents);
  }
}

void update_paydata_market() {
  FILE *fl;

  // Update paydata markets.
  if (!(fl = fopen("etc/consist", "w"))) {
    mudlog("SYSERR: Can't Open Consistency File For Write.", NULL, LOG_SYSLOG, TRUE);
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
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %s^g: %d to %d.", host_color[m], old_market, market[m]);
      something_changed = TRUE;
    }
    else {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %s^g: stays at %d.", host_color[m], market[m]);
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
  struct obj_data *obj, *o = NULL;
  int i;

  {
    bool should_loop = TRUE;
    int loop_rand = rand();
    int loop_counter = 0;

    while (should_loop) {
      should_loop = FALSE;
      loop_counter++;

      for (struct char_data *ch = character_list, *next_ch; ch; ch = next_ch) {
        next_ch = ch->next_in_character_list;
        
        if (ch->last_loop_rand == loop_rand) {
          // No error here, I don't want the spam.
          continue;
        } else {
          ch->last_loop_rand = loop_rand;
        }
        
        bool is_npc = IS_NPC(ch);
        
        if (AFF_FLAGGED(ch, AFF_FEAR)) {
          if (!number(0, 2)) {
            extern ACMD_CONST(do_flee);
            do_flee(ch, "", 0, 0);
            continue;
          }

          AFF_FLAGS(ch).RemoveBit(AFF_FEAR);
          send_to_char("You feel calmer.\r\n", ch);
        }

        if (!CH_IN_COMBAT(ch)) {
          if (is_npc) {
            // We just remove the acid with no checking since speed matters for NPCs.
            AFF_FLAGS(ch).RemoveBit(AFF_ACID);
          } else {
            // With PCs, we check for the flag and message about it.
            if (AFF_FLAGGED(ch, AFF_ACID)) {
              AFF_FLAGS(ch).RemoveBit(AFF_ACID);
              send_to_char(ch, "The choking clouds of acid dissipate.\r\n");
            }
          }
        }

        if (GET_SUSTAINED_NUM(ch) && !IS_ANY_ELEMENTAL_NO_ISNPC(ch)) {
          struct sustain_data *next, *temp, *nsus;
          for (struct sustain_data *sus = GET_SUSTAINED(ch); sus; sus = next) {
            next = sus->next;
            if (sus->spell >= MAX_SPELLS) {
              mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Hit a sustained spell %d that is higher than max %d!", sus->spell, MAX_SPELLS);
              continue;
            }

            if (sus && sus->is_caster_record && spells[sus->spell].duration == PERMANENT) {
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
                  GET_CHAR_FIRE_CAUSED_BY_PC(sus->other) = !is_npc;
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

        if (GET_SUSTAINED(ch)) {
          bool affected_by_chaos_or_confusion = FALSE;

          // This is an optimized affected_by_spell() call.
          for (struct sustain_data *hjp = GET_SUSTAINED(ch); hjp; hjp = hjp->next) {
            if (hjp->is_caster_record == FALSE && (hjp->spell == SPELL_CONFUSION || hjp->spell == SPELL_CHAOS)) {
              affected_by_chaos_or_confusion = TRUE;
              break;

              // Elementals shouldn't have anything cast on them.
              // if (IS_PC_CONJURED_ELEMENTAL(ch))
              //  break;
            }
          }
          
          if (affected_by_chaos_or_confusion || affected_by_power(ch, CONFUSION)) {
            if (ch->desc && (i = number(1, 15)) >= 5) {
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
            if (!number(0, 30)) {
              end_all_sustained_spells_of_type_affecting_ch(SPELL_CONFUSION, 0, ch);
              end_all_sustained_spells_of_type_affecting_ch(SPELL_CHAOS, 0, ch);
              send_to_char("Your head seems to clear.\r\n", ch);

              // TODO: Make the confusion power wear off.
            }
          }
        }

        if (!is_npc) {
          // Burn down adrenaline. This can kill the target, so break out if it returns true.
          if (check_adrenaline(ch, 0)) {
            // They died. Start the loop again.
            log_vfprintf("misc_update(): recycling loop due to check_adrenaline() death");
            should_loop = TRUE;
            break;
          }

          // Apply new doses of everything. If they die, bail out.
          if (process_drug_point_update_tick(ch)) {
            // They died. Start the loop again.
            log_vfprintf("misc_update(): recycling loop due to process_drug_point_update_tick() death");
            should_loop = TRUE;
            break;
          }

          affect_total(ch);
        }
        else { // NPC checks.
          // Clear out unpiloted personas and projections. TODO: What is 21 "a dim reflection" used for?
          if (!ch->desc && GET_MOB_VNUM(ch) >= 20 && GET_MOB_VNUM(ch) <= 22) {
            act("$n dissolves into the background and is no more.", TRUE, ch, 0, 0, TO_ROOM);
            for (i = 0; i < NUM_WEARS; i++)
              if (ch->equipment[i])
                extract_obj(ch->equipment[i]);
            for (obj = ch->carrying; obj; obj = o) {
              o = obj->next_content;
              extract_obj(obj);
            }
            GET_PLAYER_MEMORY(ch) = NULL;
            extract_char(ch);

            // They died or were extracted. Start the loop again.
            log_vfprintf("misc_update(): recycling loop due to astral projection cleanup");
            should_loop = TRUE;
            break;
          }
/* This code feels stale, like it was an old Circle thing for possessing an animal or something.
          else if (!ch->desc && GET_MOB_VNUM(ch) >= 50 && GET_MOB_VNUM(ch) < 70) {
            extract_char(ch);
            // They died or were extracted. Start the loop again.
            should_loop = TRUE;
            break;
          }
*/
          else if (GET_RACE(ch) == RACE_SPIRIT) { /* aka an optimized IS_SPIRIT(ch) */
            if (!check_spirit_sector(ch->in_room, GET_SPARE1(ch))) {
              act("Being away from its environment, $n suddenly ceases to exist.", TRUE, ch, 0, 0, TO_ROOM);
              end_spirit_existance(ch, FALSE);
              // They died or were extracted, BUT we don't need to re-loop since spirits have no hangers-on.
              continue;
            }
            if (GET_ACTIVE(ch)) {
              if (!ch->master || (ch->master->in_room ? (ch->master->in_room != ch->in_room) : (ch->master->in_veh != ch->in_veh))) {
                act("Being away from its master, $n suddenly ceases to exist.", TRUE, ch, 0, 0, TO_ROOM);
                end_spirit_existance(ch, FALSE);
                // They died or were extracted, BUT we don't need to re-loop since spirits have no hangers-on.
                continue;
              }
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
          if (is_npc || !GET_CHAR_FIRE_CAUSED_BY_PC(ch)) {
            damage_equip(ch, ch, 6 + GET_CHAR_FIRE_BONUS_DAMAGE(ch), TYPE_FIRE);
          }

          int dam = convert_damage(stage(-success_test(GET_BOD(ch) + GET_BODY_POOL(ch) + GET_POWER(ch, ADEPT_TEMPERATURE_TOLERANCE), 6 + GET_CHAR_FIRE_BONUS_DAMAGE(ch) - GET_IMPACT(ch)), MODERATE));
          GET_CHAR_FIRE_BONUS_DAMAGE(ch)++;
          if (damage(ch, ch, dam, TYPE_SUFFERING, PHYSICAL)) {
            // They died or were extracted. Start the loop again.
            log_vfprintf("misc_update(): recycling loop due to flame-induced death");
            should_loop = TRUE;
            break;
          }
        }
      }
    }

    if (loop_counter > 1) {
      log_vfprintf("Ran misc_update() %d times due to mid-run alterations and extractions.", loop_counter);
    }
  }

  // loop through all the characters
}

float gen_size(int race, bool height, int size, int pronouns)
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
        if (height)
          return number(270, 295) * mod;
        else
          return number(100, 115) * mod;
      } else {
        if (height)
          return number(255, 280) * mod;
        else
          return number(90, 105) * mod;
      }
      break;
    case RACE_OGRE:
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
      if (pronouns == PRONOUNS_MASCULINE) {
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
    case RACE_DRYAD:
      if (pronouns == PRONOUNS_MASCULINE) {
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
    case RACE_WESTERN_DRAGON:
      if (pronouns == PRONOUNS_MASCULINE) {
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
    case RACE_EASTERN_DRAGON:
      if (pronouns == PRONOUNS_MASCULINE) {
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
    case RACE_FEATHERED_SERPENT:
      if (pronouns == PRONOUNS_MASCULINE) {
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
    case RACE_DRAKE_HUMAN:
      if (pronouns == PRONOUNS_MASCULINE) {
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
    case RACE_DRAKE_DWARF:
      if (pronouns == PRONOUNS_MASCULINE) {
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
    case RACE_DRAKE_ELF:
      if (pronouns == PRONOUNS_MASCULINE) {
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
    case RACE_DRAKE_ORK:
      if (pronouns == PRONOUNS_MASCULINE) {
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
    case RACE_DRAKE_TROLL:
      if (pronouns == PRONOUNS_MASCULINE) {
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
    case RACE_GHOUL_HUMAN:
      if (pronouns == PRONOUNS_MASCULINE) {
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
    case RACE_GHOUL_DWARF:
      if (pronouns == PRONOUNS_MASCULINE) {
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
    case RACE_GHOUL_ELF:
      if (pronouns == PRONOUNS_MASCULINE) {
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
    case RACE_GHOUL_ORK:
      if (pronouns == PRONOUNS_MASCULINE) {
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
    case RACE_GHOUL_TROLL:
      if (pronouns == PRONOUNS_MASCULINE) {
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
    default:
      if (pronouns == PRONOUNS_MASCULINE) {
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
