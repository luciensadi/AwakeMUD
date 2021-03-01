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

#if defined(WIN32) && !defined(__CYGWIN__)
#include <process.h>
#else
#include <unistd.h>
#endif

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "newdb.h"
#include "dblist.h"
#include "handler.h"
#include "interpreter.h"
#include "utils.h"
#include "constants.h"
#include "newshop.h"
#include "newmagic.h"
#include "newmail.h"
#include "config.h"
#include "transport.h"

extern class objList ObjList;
extern int modify_target(struct char_data *ch);
extern void end_quest(struct char_data *ch);
extern char *cleanup(char *dest, const char *src);
extern void damage_equip(struct char_data *ch, struct char_data *victim, int power, int type);
extern void check_adrenaline(struct char_data *, int);
extern bool House_can_enter_by_idnum(long idnum, vnum_t house);
extern int get_paydata_market_maximum(int host_color);
extern int get_paydata_market_minimum(int host_color);

void mental_gain(struct char_data * ch)
{
  int gain = 0;
  
  if (IS_PROJECT(ch))
    return;
  
  switch (GET_POS(ch))
  {
    case POS_STUNNED:
      gain = 20;
      break;
    case POS_SLEEPING:
      gain = 25;
      break;
    case POS_LYING:
    case POS_RESTING:
      gain = 20;
      break;
    case POS_SITTING:
      gain = 15;
      break;
    case POS_FIGHTING:
      gain = 5;
      break;
    case POS_STANDING:
      gain = 10;
      break;
  }
  
  if (IS_NPC(ch))
    gain *= 2;
  if (find_workshop(ch, TYPE_MEDICAL))
    gain = (int)(gain * 1.5);
  
#ifdef ENABLE_HUNGER
  if ((GET_COND(ch, COND_FULL) == MIN_FULLNESS) || (GET_COND(ch, COND_THIRST) == MIN_QUENCHED))
    gain >>= 1;
#endif

  if (ch->in_room && ROOM_FLAGGED(ch->in_room, ROOM_ENCOURAGE_CONGREGATION))
    gain *= 2;
  
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
    for (bio = ch->bioware; bio; bio = bio->next_content)
      if (GET_OBJ_VAL(bio, 0) == BIO_SYMBIOTES) {
        switch (GET_OBJ_VAL(bio, 1)) {
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
      if (GET_OBJ_VAL(bio, 0) == BIO_SYMBIOTES) {
        switch (GET_OBJ_VAL(bio, 1)) {
          case 1:
            if (GET_OBJ_VAL(bio, 6))
              value--;
            GET_OBJ_VAL(bio, 6) = !GET_OBJ_VAL(bio, 6);
            break;
          case 2:
            if (!(GET_OBJ_VAL(bio, 6) % 3))
              value--;
            if ((++GET_OBJ_VAL(bio, 6)) > 9)
              GET_OBJ_VAL(bio, 6) = 0;
            break;
          case 3:
            value--;
            break;
        }
      } else if (GET_OBJ_VAL(bio, 0) == BIO_SUPRATHYROIDGLAND) {
        value *= 2;
      } else if (GET_OBJ_VAL(bio, 0) == BIO_DIGESTIVEEXPANSION) {
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
  
  switch (GET_OBJ_VAL(patch, 0))
  {
    case 1:
      act("The effects of $p wear off, leaving you exhausted!", FALSE, ch, patch, 0, TO_CHAR);
      GET_MENTAL(ch) = MAX(0, GET_MENTAL(ch) - (GET_OBJ_VAL(patch, 1) - 1) * 100);
      if ((GET_TRADITION(ch) == TRAD_HERMETIC || GET_TRADITION(ch) == TRAD_SHAMANIC) &&
          success_test(GET_MAGIC(ch), GET_OBJ_VAL(patch, 1)) < 0) {
        magic_loss(ch, 100, TRUE);
        affect_total(ch);
      }
      update_pos(ch);
      break;
    case 2:
      stun = resisted_test(GET_OBJ_VAL(patch, 1), GET_REAL_BOD(ch) - (GET_BIOOVER(ch) > 0 ? GET_BIOOVER(ch) / 2 : 0),
                           GET_REAL_BOD(ch) - (GET_BIOOVER(ch) > 0 ? GET_BIOOVER(ch) / 2 : 0), GET_OBJ_VAL(patch, 1));
      if (stun > 0) {
        act("You feel the drugs from $p take effect.", FALSE, ch, patch, 0, TO_CHAR);
        GET_MENTAL(ch) = MAX(0, GET_MENTAL(ch) - (stun * 100));
        update_pos(ch);
      } else
        act("You resist the feeble effects of $p.", FALSE, ch, patch, 0, TO_CHAR);
      break;
    case 3:
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
      if (!(IS_SENATOR(ch) || IS_WORKING(ch) || PLR_FLAGGED(ch, PLR_NO_IDLE_OUT)) || !ch->desc) {
        if (!GET_WAS_IN(ch) && ch->in_room && ch->char_specials.timer > 15) {
          // No idling out in cabs.
          if (ROOM_VNUM_IS_CAB(GET_ROOM_VNUM(ch->in_room))) {
            send_to_char("The cabdriver stops off at Dante's long enough to kick you out.\r\n", ch);
            int rnum = real_room(RM_ENTRANCE_TO_DANTES);
            if (rnum >= 0)
              GET_WAS_IN(ch) = &world[rnum];
            else
              GET_WAS_IN(ch) = &world[1];
          } else {
            GET_WAS_IN(ch) = ch->in_room;
          }
            
          if (FIGHTING(ch))
            stop_fighting(FIGHTING(ch));
          if (CH_IN_COMBAT(ch))
            stop_fighting(ch);
          act("$n disappears into the void.", TRUE, ch, 0, 0, TO_ROOM);
          send_to_char("You have been idle, and are pulled into a void.\r\n", ch);
          char_from_room(ch);
          char_to_room(ch, &world[1]);
        }
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
      } else if (!ch->desc && ch->char_specials.timer > 15) {
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

void check_bioware(struct char_data *ch)
{
  if (!ch->desc || (ch->desc && ch->desc->connected) || PLR_FLAGGED(ch, PLR_NEWBIE))
    return;
  
  struct obj_data *bio;
  for (bio = ch->bioware; bio; bio = bio->next_content)
    if (GET_OBJ_VAL(bio, 0) == BIO_PLATELETFACTORY)
    {
      if (--GET_OBJ_VAL(bio, 5) < 1) {
        GET_OBJ_VAL(bio, 5) = 12;
        if (success_test(GET_REAL_BOD(ch), 3 + GET_OBJ_VAL(bio, 6)) < 1) {
          send_to_char("Your blood seems to erupt.\r\n", ch);
          act("$n collapses to the floor, twitching.", TRUE, ch, 0, 0, TO_ROOM);
          damage(ch, ch, 10, TYPE_BIOWARE, PHYSICAL);
        }
        GET_OBJ_VAL(bio, 6)++;
      }
      if (GET_OBJ_VAL(bio, 5) <= 4 && number(0, 4))
        send_to_char("You feel like you should be taking your medication.\r\n", ch);
      break;
    }
}

void check_swimming(struct char_data *ch)
{
  int target, skill, i, dam, test;
  
  if (IS_NPC(ch) || IS_SENATOR(ch))
    return;
  
  target = MAX(2, ch->in_room->rating);
  if (GET_POS(ch) < POS_RESTING)
  {
    target -= success_test(MAX(1, (int)(GET_REAL_BOD(ch) / 3)), target);
    dam = convert_damage(stage(target, 0));
    if (dam > 0) {
      act("$n's unconscious body is mercilessly thrown about by the current.",
          FALSE, ch, 0, 0, TO_ROOM);
      damage(ch, ch, dam, TYPE_DROWN, FALSE);
    }
    return;
  }
  skill = SKILL_ATHLETICS;
  i = get_skill(ch, skill, target);
  i = resisted_test(i, target + modify_target(ch), target, i);
  if (i < 0)
  {
    test = success_test(GET_WIL(ch), modify_target(ch) - i);
    dam = convert_damage(stage(-test, SERIOUS));
    if (dam > 0) {
      send_to_char(ch, "You struggle to prevent your lungs getting "
                   "flooded with water.\r\n");
      damage(ch, ch, dam, TYPE_DROWN, FALSE);
    }
  } else if (!i)
  {
    test = success_test(GET_WIL(ch), 3 + modify_target(ch));
    dam = convert_damage(stage(-test, MODERATE));
    if (dam > 0) {
      send_to_char(ch, "You struggle to prevent your lungs getting "
                   "flooded with water.\r\n");
      damage(ch, ch, dam, TYPE_DROWN, FALSE);
    }
  } else if (i < 3)
  {
    test = success_test(GET_WIL(ch), 5 - i + modify_target(ch));
    dam = convert_damage(stage(-test, LIGHT));
    if (dam > 0) {
      send_to_char(ch, "You struggle to prevent your lungs getting "
                   "flooded with water.\r\n");
      damage(ch, ch, dam, TYPE_DROWN, FALSE);
    }
  }
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
        check_swimming(ch);
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
        if (GET_OBJ_VAL(obj, 0) == BIO_METABOLICARRESTER) {
          if (++GET_OBJ_VAL(obj, 3) == 5) {
            GET_OBJ_VAL(obj, 3) = 0;
            dam = TRUE;
          } else
            dam = FALSE;
        }
      if (dam)
        damage(ch, ch, 1, TYPE_SUFFERING, PHYSICAL);
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
  FILE *fl;
  extern struct time_info_data time_info;
  
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
      
      if (GET_MAG(i) > 0) {
        int force = 0, total = 0;
        for (int x = 0; x < NUM_WEARS; x++)
          if (GET_EQ(i, x) && GET_OBJ_TYPE(GET_EQ(i, x)) == ITEM_FOCUS && GET_OBJ_VAL(GET_EQ(i, x), 2) == GET_IDNUM(i) && GET_OBJ_VAL(GET_EQ(i, x), 4)) {
            force += GET_OBJ_VAL(GET_EQ(i, x), 1);
            total++;
          }
        if (force * 100 > GET_REAL_MAG(i) * 2 && success_test(GET_REAL_MAG(i) / 100, force / 2) < 1) {
          int num = number(1, total);
          struct obj_data *foci = NULL;
          for (int x = 0; x < NUM_WEARS && !foci; x++)
            if (GET_EQ(i, x) && GET_OBJ_TYPE(GET_EQ(i, x)) == ITEM_FOCUS && GET_OBJ_VAL(GET_EQ(i, x), 2) == GET_IDNUM(i) && GET_OBJ_VAL(GET_EQ(i, x), 4) && !--num)
              foci = GET_EQ(i, x);
          if (foci) {
            send_to_char(i, "You feel some of your magic becoming locked in %s.\r\n", GET_OBJ_NAME(foci));
            GET_OBJ_VAL(foci, 9) = GET_IDNUM(i);
            magic_loss(i, 100, FALSE);
          }
        }
      }
      
      if (HUNTING(i) && !AFF_FLAGGED(i, AFF_TRACKING) && ++HOURS_SINCE_TRACK(i) > 8)
        HUNTING(i) = NULL;
        
      if (i->bioware)
        check_bioware(i);
        
      for (int x = MIN_DRUG; x < NUM_DRUGS; x++) {
        if (GET_DRUG_ADDICT(i, x) > 0) {
          int tsl = (time(0) - GET_DRUG_LASTFIX(i, x)) / SECS_PER_MUD_DAY;
          GET_DRUG_ADDTIME(i, x)++;
          if (!(GET_DRUG_ADDTIME(i ,x) % 168) && GET_REAL_BOD(i) == 1) {
            switch (number(0, 1)) {
              case 0:
                GET_MENTAL_LOSS(i)++;
                break;
              case 1:
                GET_PHYSICAL_LOSS(i)++;
                break;
            }
            send_to_char(i, "Your health suffers at the hand of your %s addiction.\r\n", drug_types[x].name);
          }
          if (!(GET_DRUG_ADDTIME(i ,x) % 720) && ((drug_types[x].mental_addiction && success_test(GET_WIL(i), drug_types[x].mental_addiction + GET_DRUG_EDGE(i, x)) < 1)  ||
                                                  (drug_types[x].physical_addiction && success_test(GET_BOD(i), drug_types[x].physical_addiction + GET_DRUG_EDGE(i, x)) < 1))) {
            if (GET_REAL_BOD(i) > 1)
              GET_REAL_BOD(i)--;
            else if (GET_PERM_BOD_LOSS(i) < racial_limits[(int)GET_RACE(i)][0][0])
              GET_PERM_BOD_LOSS(i)++;
            send_to_char(i, "Your health suffers at the hand of your %s addiction.\r\n", drug_types[x].name);
          }
          if (AFF_FLAGGED(i, AFF_WITHDRAWAL)) {
            if (tsl > GET_DRUG_LASTWITH(i, x) + 1) {
              GET_DRUG_LASTWITH(i, x) += 2;
              GET_DRUG_EDGE(i, x)--;
              if (!GET_DRUG_EDGE(i ,x)) {
                AFF_FLAGS(i).RemoveBit(AFF_WITHDRAWAL);
                GET_DRUG_ADDICT(i, x) = 0;
                continue;
              }
            }
            send_to_char(i, "Your body cries out for some %s.\r\n", drug_types[x].name);
          } else if (tsl >= drug_types[x].fix + 1 && !AFF_FLAGGED(i, AFF_WITHDRAWAL_FORCE)) {
            send_to_char(i, "You begin to go into %s withdrawal.\r\n", drug_types[x].name);
            AFF_FLAGS(i).SetBit(AFF_WITHDRAWAL_FORCE);
            affect_total(i);
          } else if (tsl >= drug_types[x].fix) {
            if (drug_types[x].mental_addiction ? success_test(GET_WIL(i), drug_types[x].mental_addiction + GET_DRUG_EDGE(i, x)) : 1 < 1 ||
                drug_types[x].physical_addiction ? success_test(GET_REAL_BOD(i), drug_types[x].physical_addiction + GET_DRUG_EDGE(i, x)) : 1 < 1) {
              if (AFF_FLAGGED(i, AFF_WITHDRAWAL_FORCE)) {
                snprintf(buf, sizeof(buf), "Your lack of %s is causing you great pain and discomfort.\r\n", drug_types[x].name);
                if (tsl > GET_DRUG_LASTWITH(i, x)) {
                  GET_DRUG_LASTWITH(i ,x)++;
                  GET_DRUG_EDGE(i, x)--;
                  if (!GET_DRUG_EDGE(i ,x)) {
                    AFF_FLAGS(i).RemoveBit(AFF_WITHDRAWAL_FORCE);
                    GET_DRUG_ADDICT(i, x) = 0;
                    continue;
                  }
                }
              } else
                snprintf(buf, sizeof(buf), "You crave some %s.\r\n", drug_types[x].name);
              for (struct obj_data *obj = i->carrying; obj; obj = obj->next_content) {
                if (GET_OBJ_TYPE(obj) == ITEM_DRUG && GET_OBJ_VAL(obj, 0) == x) {
                  do_use(i, obj->text.keywords, 0, 0);
                  if (GET_DRUG_DOSE(i) > GET_DRUG_TOLERANT(i, x))
                    snprintf(buf, sizeof(buf), "You satisfy your craving for %s.\r\n", drug_types[x].name);
                  else
                    snprintf(buf, sizeof(buf), "You attempt to satisfy your craving for %s.\r\n", drug_types[x].name);
                  break;
                }
              }
              send_to_char(buf, i);
            }
          }
        }
      }
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
        damage(victim, victim, (GET_BOD(victim) - 1) * 100, TYPE_SUFFERING, TRUE);
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
  {
    float totaltime = 0;
    for (int shop_nr = 0; shop_nr <= top_of_shopt; shop_nr++) {
      snprintf(buf, sizeof(buf), "order/%ld", shop_table[shop_nr].vnum);
      unlink(buf);
      if (shop_table[shop_nr].order) {
        if (!(fl = fopen(buf, "w"))) {
          perror("SYSERR: Error saving order file");
          continue;
        }
        int i = 0;
        fprintf(fl, "[ORDERS]\n");
        for (struct shop_order_data *order = shop_table[shop_nr].order; order; order = order->next, i++) {
          totaltime = order->timeavail - time(0);
          if (!order->sent && totaltime < 0) {
            int real_obj = real_object(order->item);
            snprintf(buf2, sizeof(buf2), "%s has arrived at %s and is ready for pickup for a total cost of %d nuyen.\r\n", 
                     real_obj > 0 ? CAP(obj_proto[real_obj].text.name) : "Something",
                     shop_table[shop_nr].shopname,
                     order->price
                    );
            int real_mob = real_mobile(shop_table[shop_nr].keeper);
            if (real_mob > 0)
              raw_store_mail(order->player, 0, mob_proto[real_mob].player.physical_text.name, (const char *) buf2);
            else
              raw_store_mail(order->player, 0, "An anonymous shopkeeper", (const char *) buf2);
            order->sent = TRUE;
          }
          fprintf(fl, "\t[ORDER %d]\n", i);
          fprintf(fl, "\t\tItem:\t%ld\n"
                  "\t\tPlayer:\t%ld\n"
                  "\t\tTime:\t%d\n"
                  "\t\tNumber:\t%d\n"
                  "\t\tPrice:\t%d\n"
                  "\t\tSent:\t%d\n", order->item, order->player, order->timeavail, order->number, order->price, order->sent);
        }
        fclose(fl);
      }
    }
  }
  
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
  }
  
  return TRUE;
}

void save_vehicles(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct veh_data *veh;
  FILE *fl;
  struct char_data *i;
  int v;
  struct room_data *temp_room = NULL;
  struct obj_data *obj;
  int num_veh = 0;
  
  for (veh = veh_list; veh; veh = veh->next)
    if (should_save_this_vehicle(veh))
      num_veh++;
      
  // log_vfprintf("We have %d vehicles to save.", num_veh);
  
  // Write the count of vehicles to the file.
  if (!(fl = fopen("veh/vfile", "w"))) {
    mudlog("SYSERR: Can't Open Vehicle File For Write.", NULL, LOG_SYSLOG, FALSE);
    return;
  }
  fprintf(fl, "%d\n", num_veh);
  fclose(fl);
  
  for (veh = veh_list, v = 0; veh; veh = veh->next) {
    // Skip disqualified vehicles.
    if (veh->owner <= 0)
      continue;
    
    bool send_veh_to_junkyard = FALSE;
    if (veh->damage >= VEH_DAM_THRESHOLD_DESTROYED && !(veh->in_veh || (veh->in_room && ROOM_FLAGGED(veh->in_room, ROOM_GARAGE)))) {
      // If the vehicle is wrecked and is in neither a containing vehicle nor a garage...
      if (veh_is_in_junkyard(veh)) {
        // If it's already in the junkyard, we don't save it-- they should have come and fixed it.
        continue;
      } else {
        // If it's not in the junkyard yet, flag it for moving to the junkyard.
        send_veh_to_junkyard = TRUE;
      }
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
      else if (!ROOM_FLAGGED(temp_room, ROOM_GARAGE) 
               || (ROOM_FLAGGED(temp_room, ROOM_HOUSE) 
                   && !House_can_enter_by_idnum(veh->owner, temp_room->number))) {
       snprintf(buf, sizeof(buf), "Falling back to a garage for non-garage-room veh %s (in '%s' %ld).", 
                GET_VEH_NAME(veh), GET_ROOM_NAME(temp_room), GET_ROOM_VNUM(temp_room)
              );
       log(buf);
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
    
    fprintf(fl, "[VEHICLE]\n");
    fprintf(fl, "\tVnum:\t%ld\n", veh_index[veh->veh_number].vnum);
    fprintf(fl, "\tOwner:\t%ld\n", veh->owner);
    fprintf(fl, "\tInRoom:\t%ld\n", temp_room->number);
    fprintf(fl, "\tSubscribed:\t%d\n", veh->sub);
    fprintf(fl, "\tDamage:\t%d\n", veh->damage);
    fprintf(fl, "\tSpare:\t%ld\n", veh->spare);
    fprintf(fl, "\tIdnum:\t%ld\n", veh->idnum);
    if (veh->in_veh)
      fprintf(fl, "\tInVeh:\t%ld\n", veh->in_veh->idnum);
    if (veh->restring)
      fprintf(fl, "\tVRestring:\t%s\n", veh->restring);
    if (veh->restring_long)
      fprintf(fl, "\tVRestringLong:$\n%s~\n", cleanup(buf2, veh->restring_long));
    fprintf(fl, "[CONTENTS]\n");
    int o = 0, level = 0;
    for (obj = veh->contents;obj;) {
      if (!IS_OBJ_STAT(obj, ITEM_NORENT)) {
        fprintf(fl, "\t[Object %d]\n", o);
        o++;
        fprintf(fl, "\t\tVnum:\t%ld\n", GET_OBJ_VNUM(obj));
        fprintf(fl, "\t\tInside:\t%d\n", level);
        if (GET_OBJ_TYPE(obj) == ITEM_PHONE)
          for (int x = 0; x < 4; x++)
            fprintf(fl, "\t\tValue %d:\t%d\n", x, GET_OBJ_VAL(obj, x));
        else if (GET_OBJ_TYPE(obj) != ITEM_WORN)
          for (int x = 0; x < NUM_VALUES; x++)
            fprintf(fl, "\t\tValue %d:\t%d\n", x, GET_OBJ_VAL(obj, x));
        fprintf(fl, "\t\tCondition:\t%d\n", GET_OBJ_CONDITION(obj));
        fprintf(fl, "\t\tCost:\t%d\n", GET_OBJ_COST(obj));
        fprintf(fl, "\t\tTimer:\t%d\n", GET_OBJ_TIMER(obj));
        fprintf(fl, "\t\tAttempt:\t%d\n", GET_OBJ_ATTEMPT(obj));
        fprintf(fl, "\t\tExtraFlags:\t%s\n", GET_OBJ_EXTRA(obj).ToString());
        fprintf(fl, "\t\tFront:\t%d\n", obj->vfront);
        if (obj->restring)
          fprintf(fl, "\t\tName:\t%s\n", obj->restring);
        if (obj->photo)
          fprintf(fl, "\t\tPhoto:$\n%s~\n", cleanup(buf2, obj->photo));
      }
      
      if (obj->contains && !IS_OBJ_STAT(obj, ITEM_NORENT) && GET_OBJ_TYPE(obj) != ITEM_PART) {
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
  
  if (v != num_veh) {
    snprintf(buf, sizeof(buf), "SYSERR: LOST VEHICLES: %d != %d in save_vehicles, so some vehicles ARE NOT SAVED.", v, num_veh);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  }
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
  int i, dam = 0, power = 0;
  
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
    
    if (GET_SUSTAINED_NUM(ch) && !IS_ELEMENTAL(ch)) {
      struct sustain_data *next, *temp, *nsus;
      for (struct sustain_data *sus = GET_SUSTAINED(ch); sus; sus = next) {
        next = sus->next;
        if (sus->spell > SPELL_SHADOW) {
          snprintf(buf, sizeof(buf), "COWS GO MOO %s %d", GET_CHAR_NAME(ch), sus->spell);
          log(buf);
          continue;
        }
        if (sus && sus->caster && spells[sus->spell].duration == PERMANENT)
          if (++sus->time >= (sus->spell == SPELL_IGNITE ? sus->drain : (sus->spell == SPELL_TREAT ? sus->drain * 2.5 : sus->drain * 5))) {
            if (sus->spell == SPELL_IGNITE) {
              send_to_char("Your body erupts in flames!\r\n", sus->other);
              act("$n's body suddenly bursts into flames!\r\n", TRUE, sus->other, 0, 0, TO_ROOM);
              sus->other->points.fire[0] = srdice();
              sus->other->points.fire[1] = 0;
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
      if (!CH_IN_COMBAT(ch))
        check_adrenaline(ch, 0);
      if (GET_DRUG_DOSE(ch) && --GET_DRUG_DURATION(ch) < 0 && !GET_DRUG_STAGE(ch)) {
        bool physical = TRUE;
        if (GET_DRUG_AFFECT(ch) == DRUG_HYPER || GET_DRUG_AFFECT(ch) == DRUG_BURN)
          physical = FALSE;
        if (drug_types[GET_DRUG_AFFECT(ch)].level > 0) {
          dam = drug_types[GET_DRUG_AFFECT(ch)].level + (GET_DRUG_DOSE(ch) > 1 ? 1 : 0);
          power = drug_types[GET_DRUG_AFFECT(ch)].power + (GET_DRUG_DOSE(ch) > 2 ? GET_DRUG_DOSE(ch) - 2 : 0);
          dam = convert_damage(stage(-success_test(GET_REAL_BOD(ch) - (GET_BIOOVER(ch) > 0 ? GET_BIOOVER(ch) / 2 : 0), power), dam));
          damage(ch, ch, dam, TYPE_BIOWARE, physical);
        }
        GET_DRUG_STAGE(ch) = 1;
        GET_DRUG_LASTFIX(ch, GET_DRUG_AFFECT(ch)) = time(0);
        switch (GET_DRUG_AFFECT(ch)) {
          case DRUG_ACTH:
            snprintf(buf, sizeof(buf), "You feel a brief moment of vertigo.\r\n");
            extern void check_adrenaline(struct char_data *ch, int mode);
            check_adrenaline(ch, 1);
            GET_DRUG_AFFECT(ch) = GET_DRUG_DOSE(ch) = GET_DRUG_DURATION(ch) = 0;
            break;
          case DRUG_HYPER:
            snprintf(buf, sizeof(buf), "The world seems to swirl around you as your mind is bombarded with feedback.\r\n");
            GET_DRUG_DURATION(ch) = dam * 100;
            break;
          case DRUG_JAZZ:
            snprintf(buf, sizeof(buf), "The world slows down around you.\r\n");
            GET_DRUG_DURATION(ch) = 100 * srdice();
            break;
          case DRUG_KAMIKAZE:
            snprintf(buf, sizeof(buf), "Your body feels alive with energy and the desire to fight.\r\n");
            GET_DRUG_DURATION(ch) = 100 * srdice();
            break;
          case DRUG_PSYCHE:
            snprintf(buf, sizeof(buf), "Your feel your mind racing.\r\n");
            GET_DRUG_DURATION(ch) = MAX(1, 12 - GET_REAL_BOD(ch)) * 600;
            break;
          case DRUG_BLISS:
            snprintf(buf, sizeof(buf), "The world fades into bliss as your body becomes sluggish.\r\n");
            GET_DRUG_DURATION(ch) = MAX(1, 6 - GET_REAL_BOD(ch)) * 600;
            break;
          case DRUG_BURN:
            snprintf(buf, sizeof(buf), "You suddenly feel very intoxicated.\r\n");
            GET_DRUG_AFFECT(ch) = GET_DRUG_DOSE(ch) = GET_DRUG_DURATION(ch) = 0;
            GET_COND(ch, COND_DRUNK) = FOOD_DRINK_MAX;
            break;
          case DRUG_CRAM:
            snprintf(buf, sizeof(buf), "Your body feels alive with energy.\r\n");
            GET_DRUG_DURATION(ch) = MAX(1, 12 - GET_REAL_BOD(ch)) * 600;
            break;
          case DRUG_NITRO:
            snprintf(buf, sizeof(buf), "You lose sense of yourself as your entire body comes alive with energy.\r\n");
            GET_DRUG_DURATION(ch) = 100 * srdice();
            break;
          case DRUG_NOVACOKE:
            snprintf(buf, sizeof(buf), "You feel euphoric and alert.\r\n");
            GET_DRUG_DURATION(ch) = MAX(1, 10 - GET_REAL_BOD(ch)) * 600;
            break;
          case DRUG_ZEN:
            snprintf(buf, sizeof(buf), "You start to loose your sense of reality as your sight fills with hallucinations.\r\n");
            GET_DRUG_DURATION(ch) = 100 * srdice();
            break;
        }
        send_to_char(buf, ch);
      }
      else if (GET_DRUG_STAGE(ch) == 1) {
        int toxin = 0;
        for (struct obj_data *obj = ch->bioware; obj && !toxin; obj = obj->next_content)
          if (GET_OBJ_VAL(obj, 0) == BIO_TOXINEXTRACTOR)
            toxin = GET_OBJ_VAL(obj, 1);
        if (GET_DRUG_AFFECT(ch) > 0)
          send_to_char(ch, "You begin to feel drained as the %s wears off.\r\n", drug_types[GET_DRUG_AFFECT(ch)].name);
        GET_DRUG_STAGE(ch) = 2;
        switch (GET_DRUG_AFFECT(ch)) {
          case DRUG_JAZZ:
            GET_DRUG_DURATION(ch) = 100 * srdice();
            damage(ch, ch, convert_damage(stage(-success_test(GET_REAL_BOD(ch) - (GET_BIOOVER(ch) > 0 ? GET_BIOOVER(ch) / 2 : 0), 8 - toxin), LIGHT)), TYPE_BIOWARE, 0);
            break;
          case DRUG_KAMIKAZE:
            GET_DRUG_DURATION(ch) = 100 * srdice();
            damage(ch, ch, convert_damage(stage(-success_test(GET_REAL_BOD(ch) - (GET_BIOOVER(ch) > 0 ? GET_BIOOVER(ch) / 2 : 0), 6 - toxin), MODERATE)), TYPE_BIOWARE, 0);
            break;
          case DRUG_CRAM:
            GET_DRUG_DURATION(ch) = MAX(1, 12 - GET_REAL_BOD(ch) - (GET_BIOOVER(ch) > 0 ? GET_BIOOVER(ch) / 2 : 0)) * 600;
            break;
          case DRUG_NITRO:
            damage(ch, ch, convert_damage(stage(-success_test(GET_REAL_BOD(ch) - (GET_BIOOVER(ch) > 0 ? GET_BIOOVER(ch) / 2 : 0), 8 - toxin), DEADLY)), TYPE_BIOWARE, 0);
            GET_DRUG_STAGE(ch) = GET_DRUG_DOSE(ch) = GET_DRUG_AFFECT(ch) = 0;
            break;
          case DRUG_NOVACOKE:
            GET_DRUG_DURATION(ch) = MAX(1, 10 - GET_REAL_BOD(ch) - (GET_BIOOVER(ch) > 0 ? GET_BIOOVER(ch) / 2 : 0)) * 600;
            break;
          default:
            GET_DRUG_STAGE(ch) = GET_DRUG_DOSE(ch) = GET_DRUG_AFFECT(ch) = 0;
            if (AFF_FLAGGED(ch, AFF_DETOX))
              AFF_FLAGS(ch).RemoveBit(AFF_DETOX);
        }
        if (drug_types[GET_DRUG_AFFECT(ch)].tolerance) {
          if (GET_DRUG_DOSES(ch, GET_DRUG_AFFECT(ch)) == 1 && success_test(GET_REAL_BOD(ch), drug_types[GET_DRUG_AFFECT(ch)].tolerance) < 1)
            GET_DRUG_TOLERANT(ch, GET_DRUG_AFFECT(ch))++;
          if (((!GET_DRUG_ADDICT(ch, GET_DRUG_AFFECT(ch)) && !(drug_types[GET_DRUG_AFFECT(ch)].edge_preadd % ++GET_DRUG_DOSES(ch, GET_DRUG_AFFECT(ch)))) ||
               (GET_DRUG_ADDICT(ch, GET_DRUG_AFFECT(ch)) && !(drug_types[GET_DRUG_AFFECT(ch)].edge_posadd % ++GET_DRUG_DOSES(ch, GET_DRUG_AFFECT(ch))))) &&
              success_test(GET_REAL_BOD(ch), drug_types[GET_DRUG_AFFECT(ch)].tolerance + GET_DRUG_EDGE(ch, GET_DRUG_AFFECT(ch))))
            GET_DRUG_TOLERANT(ch, GET_DRUG_AFFECT(ch))++;
        }
      } else if (GET_DRUG_STAGE(ch) == 2) {
        send_to_char(ch, "The aftereffects of the %s begin to wear off.\r\n", drug_types[GET_DRUG_AFFECT(ch)].name);
        GET_DRUG_STAGE(ch) = GET_DRUG_DOSE(ch) = GET_DRUG_AFFECT(ch) = 0;
        if (AFF_FLAGGED(ch, AFF_DETOX))
          AFF_FLAGS(ch).RemoveBit(AFF_DETOX);
      }
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
        GET_MEMORY(ch) = NULL;
        extract_char(ch);
      }
      else if (!ch->desc && GET_MOB_VNUM(ch) >= 50 && GET_MOB_VNUM(ch) < 70) {
        extract_char(ch);
      }
      else if (IS_SPIRIT(ch)) {
        if (!check_spirit_sector(ch->in_room, GET_SPARE1(ch))) {
          act("Being away from its environment, $n suddenly ceases to exist.", TRUE, ch, 0, 0, TO_ROOM);
          end_spirit_existance(ch, FALSE);
        }
      }
    }
    
    if (ch->points.fire[0] > 0) {
      if (ch->in_room->sector_type != SPIRIT_HEARTH && !ROOM_FLAGGED(ch->in_room, ROOM_INDOORS) && weather_info.sky >= SKY_RAINING)
        ch->points.fire[0] -= 3;
      else
        ch->points.fire[0]--;
      if (ch->points.fire[0] < 1) {
        act("The flames around $n die down.", FALSE, ch, 0, 0, TO_ROOM);
        act("The flames surrounding you die down.", FALSE, ch, 0, 0, TO_CHAR);
      } else {
        act("Flames continue to burn around $n!", FALSE, ch, 0, 0, TO_ROOM);
        act("^RYour body is surrounded in flames!", FALSE, ch, 0, 0, TO_CHAR);
      }
      damage_equip(ch, ch, 6 + ch->points.fire[1], TYPE_FIRE);
      int dam = convert_damage(stage(-success_test(GET_BOD(ch) + GET_POWER(ch, ADEPT_TEMPERATURE_TOLERANCE), 6 + ch->points.fire[1]++ - GET_IMPACT(ch)), MODERATE));
      ch->points.fire[1]++;
      damage(ch, ch, dam, TYPE_SUFFERING, PHYSICAL);
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
