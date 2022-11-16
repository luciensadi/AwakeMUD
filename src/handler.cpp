/**************************************************************************
 *   File: handler.c                                     Part of CircleMUD *
 *  Usage: internal funcs: moving and finding chars/objs                   *
 *                                                                         *
 *  All rights reserved.  See license.doc for complete information.        *
 *                                                                         *
 *  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 ************************************************************************ */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <vector>

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "handler.hpp"
#include "interpreter.hpp"
#include "memory.hpp"
#include "dblist.hpp"
#include "constants.hpp"
#include "newmatrix.hpp"
#include "newmagic.hpp"
#include "playergroups.hpp"
#include "config.hpp"

/* external functions */
extern void stop_fighting(struct char_data * ch);
extern void remove_follower(struct char_data * ch);
extern void clearMemory(struct char_data * ch);
extern void print_object_location(int, struct obj_data *, struct char_data *, int);
extern void calc_weight(struct char_data *);
extern int skill_web(struct char_data *, int);
extern int return_general(int skill_num);
extern int can_wield_both(struct char_data *, struct obj_data *, struct obj_data *);
extern int max_ability(int i);
extern int calculate_vehicle_entry_load(struct veh_data *veh);
extern void end_quest(struct char_data *ch);

int get_skill_num_in_use_for_weapons(struct char_data *ch);
int get_skill_dice_in_use_for_weapons(struct char_data *ch);

void _char_with_spell_to_room(struct char_data *ch, int spell_num, room_spell_t *room_spell_tracker);
void _char_with_spell_from_room(struct char_data *ch, int spell_num, room_spell_t *room_spell_tracker);
void _char_with_light_to_room(struct char_data *ch);
void _char_with_light_from_room(struct char_data *ch);

struct obj_data *find_obj(struct char_data *ch, char *name, int num);

char *fname(char *namelist)
{
  static char holder[50];
  char *point;

  if (!namelist || !*namelist) {
    mudlog("SYSERR: fname received null namelist!", NULL, LOG_SYSLOG, TRUE);
    strlcpy(holder, "error-report-this-to-staff", sizeof(holder));
    return holder;
  }

  for (point = holder; isalpha(*namelist); namelist++, point++)
    *point = *namelist;

  *point = '\0';

  return (holder);
}

char *fname_allchars(char *namelist)
{
  static char holder[50];
  char *point;

  for (point = holder; *namelist && *namelist != ' ' && *namelist != '\r' && *namelist != '\n' && *namelist != '\t'; namelist++, point++)
    *point = *namelist;

  *point = '\0';

  return (holder);
}


int isname(const char *str, const char *namelist)
{
  if(namelist == NULL)
    return 0;
  if(*namelist == '\0')
    return 0;
  if (namelist[0] == '\0')
    return 0;

  const char *curname, *curstr;

  curname = namelist;
  for (;;) {
    for (curstr = str;; curstr++, curname++) {
      if ((!*curstr && !isalpha(*curname)) || is_abbrev(curstr, curname))
      // if (!*curstr && !isalpha(*curname))
        return (1);

      if (!*curname)
        return (0);

      if (!*curstr || *curname == ' ')
        break;

      if (LOWER(*curstr) != LOWER(*curname))
        break;
    }

    // skip to next name

    for (; isalpha(*curname); curname++)
      ;
    if (!*curname)
      return (0);
    curname++;                  // first char of new name
  }
}


void affect_modify(struct char_data * ch,
                   byte loc,
                   sbyte mod,
                   const Bitfield &bitv,
                   bool add
                   )
{
  if (add) {
    AFF_FLAGS(ch).SetAll(bitv);
  }
  else {
    AFF_FLAGS(ch).RemoveAll(bitv);
    mod = -mod;
  }

  switch (loc) {
    case APPLY_NONE:
      break;

    case APPLY_STR:
      GET_STR(ch) += mod;
      break;
    case APPLY_QUI:
      GET_QUI(ch) += mod;
      break;
    case APPLY_INT:
      GET_INT(ch) += mod;
      break;
    case APPLY_WIL:
      GET_WIL(ch) += mod;
      break;
    case APPLY_BOD:
      GET_BOD(ch) += mod;
      break;
    case APPLY_CHA:
      GET_CHA(ch) += mod;
      break;
    case APPLY_MAG:
      GET_MAG(ch) += (mod * 100);
      break;
    case APPLY_ESS:
      GET_ESS(ch) += (mod * 100);
      break;
    case APPLY_REA:
      GET_REA(ch) += mod;
      break;

    case APPLY_AGE:
      ch->player.time.birth -= (mod * SECS_PER_MUD_YEAR);
      break;

    case APPLY_CHAR_WEIGHT:
      GET_WEIGHT(ch) += mod;
      break;

    case APPLY_CHAR_HEIGHT:
      GET_HEIGHT(ch) += mod;
      break;

    case APPLY_MENTAL:
      GET_MAX_MENTAL(ch) += mod * 100;
      break;

    case APPLY_PHYSICAL:
      GET_MAX_PHYSICAL(ch) += mod * 100;
      break;

    case APPLY_BALLISTIC:
      GET_BALLISTIC(ch) += mod;
      break;

    case APPLY_IMPACT:
      GET_IMPACT(ch) += mod;
      break;

    case APPLY_ASTRAL_POOL:
      GET_ASTRAL(ch) += mod;
      break;

    case APPLY_COMBAT_POOL:
      GET_COMBAT(ch) += mod;
      break;

    case APPLY_HACKING_POOL:
      GET_HACKING(ch) += mod;
      break;

    case APPLY_CONTROL_POOL:
      GET_CONTROL(ch) += mod;
      break;

    case APPLY_MAGIC_POOL:
      GET_MAGIC(ch) += mod;   /* GET_MAGIC gets their magic pool, GET_MAG is for attribute*/
      break;

    case APPLY_INITIATIVE_DICE:
      GET_INIT_DICE(ch) += mod;
      break;

    case APPLY_TARGET:
      GET_TARGET_MOD(ch) += mod;
      break;
    case APPLY_TASK_BOD:
      GET_TASK_POOL(ch, BOD) += mod;
      break;
    case APPLY_TASK_QUI:
      GET_TASK_POOL(ch, QUI) += mod;
      break;
    case APPLY_TASK_STR:
      GET_TASK_POOL(ch, STR) += mod;
      break;
    case APPLY_TASK_CHA:
      GET_TASK_POOL(ch, CHA) += mod;
      break;
    case APPLY_TASK_INT:
      GET_TASK_POOL(ch, INT) += mod;
      break;
    case APPLY_TASK_WIL:
      GET_TASK_POOL(ch, WIL) += mod;
      break;
    case APPLY_TASK_REA:
      GET_TASK_POOL(ch, REA) += mod;
      break;
    default:
      log_vfprintf("SYSLOG: Unknown apply adjust: %s/%d.", GET_NAME(ch), loc);
      break;
  }                             /* switch */
}

void apply_focus_effect( struct char_data *ch, struct obj_data *object )
{
  if (GET_FOCUS_BONDED_TO(object) != GET_IDNUM(ch) || !GET_FOCUS_ACTIVATED(object))
    return;
  if (GET_OBJ_VAL(object, 9) == GET_IDNUM(ch))
    GET_MAG(ch) += 100;
  if (GET_FOCUS_TYPE(object) == FOCI_POWER)
  {
    GET_MAG(ch) += GET_OBJ_VAL(object, 1) * 100;
    GET_MAGIC(ch) += GET_OBJ_VAL(object, 1);
  }
}

void remove_focus_effect( struct char_data *ch, struct obj_data *object )
{
  if (GET_FOCUS_BONDED_TO(object) != GET_IDNUM(ch) || !GET_FOCUS_ACTIVATED(object))
    return;
  if (GET_OBJ_VAL(object, 9) == GET_IDNUM(ch))
    GET_MAG(ch) -= 100;
  if (GET_FOCUS_TYPE(object) == FOCI_POWER)
  {
    GET_MAG(ch) -= GET_OBJ_VAL(object, 1) * 100;
    GET_MAGIC(ch) -= GET_OBJ_VAL(object, 1);
  }

}

void affect_veh(struct veh_data *veh, byte loc, sbyte mod)
{
  switch (loc)
  {
    case VAFF_NONE:
      break;
    case VAFF_HAND:
      veh->handling += mod;
      break;
    case VAFF_SPD:
      veh->speed += mod;
      break;
    case VAFF_ACCL:
      veh->accel += mod;
      break;
    case VAFF_BOD:
      veh->body += mod;
      break;
    case VAFF_ARM:
      veh->armor += mod;
      break;
    case VAFF_SEN:
      veh->sensor = mod;
      break;
    case VAFF_SIG:
      veh->sig += mod;
      break;
    case VAFF_AUTO:
      veh->autonav = mod;
      break;
    case VAFF_SEAF:
      veh->seating[SEATING_FRONT] += mod;
      break;
    case VAFF_SEAB:
      veh->seating[SEATING_REAR] += mod;
      break;
    case VAFF_LOAD:
      veh->load += mod;
      break;
    case VAFF_PILOT:
      veh->pilot = mod;
      break;
    case VAFF_ULTRASONIC:
      if (mod > 0)
        veh->flags.SetBit(VFLAG_ULTRASOUND);
      else
        veh->flags.RemoveBit(VFLAG_ULTRASOUND);
      break;
    default:
      log_vfprintf("SYSLOG: Unknown apply adjust: %s/%d.", veh->short_description, loc);
      break;


  }
}
void spell_modify(struct char_data *ch, struct sustain_data *sust, bool add)
{
  int mod = add ? 1 : -1;
  int tmp;
  switch (sust->spell)
  {
    case SPELL_INCATTR:
    case SPELL_INCCYATTR:
      tmp = MIN(sust->force, sust->success / 2);
      GET_ATT(ch, sust->subtype) += mod * tmp;
      break;
    case SPELL_DECATTR:
    case SPELL_DECCYATTR:
      tmp = MIN(sust->force, sust->success / 2);
      GET_ATT(ch, sust->subtype) -= mod * tmp;
      break;
    case SPELL_HEAL:
    case SPELL_TREAT:
        // Restrict max HP change to the lesser of the force or successes.
        tmp = MIN(sust->force, sust->success) * 100;
        // Further restrict max HP change to the character's max_physical value.
        tmp = MIN(tmp, GET_MAX_PHYSICAL(ch));

        // Now that we meet 0 ≤ hp change ≤ max_phys, apply the add/subtract multiplier.
        mod *= tmp;

        // Finally, apply it to character, capping at their max physical. No negative cap is applied.
        GET_PHYSICAL(ch) = MIN(GET_MAX_PHYSICAL(ch), GET_PHYSICAL(ch) + mod);
      break;
    case SPELL_STABILIZE:
      if (mod == 1)
        AFF_FLAGS(ch).SetBit(AFF_STABILIZE);
      else
        AFF_FLAGS(ch).RemoveBit(AFF_STABILIZE);
      break;
    case SPELL_DETOX:
      if (mod == 1)
        AFF_FLAGS(ch).SetBit(AFF_DETOX);
      else
        AFF_FLAGS(ch).RemoveBit(AFF_DETOX);
      break;
    case SPELL_INCREA:
      mod *= sust->success;
      GET_REA(ch) += MIN(sust->force, mod);
      break;
    case SPELL_INCREF1:
      GET_INIT_DICE(ch) += mod;
      break;
    case SPELL_INCREF2:
      mod *= 2;
      GET_INIT_DICE(ch) += mod;
      break;
    case SPELL_INCREF3:
      mod *= 3;
      GET_INIT_DICE(ch) += mod;
      break;
    case SPELL_RESISTPAIN:
      if (mod == 1)
        AFF_FLAGS(ch).SetBit(AFF_RESISTPAIN);
      else
        AFF_FLAGS(ch).RemoveBit(AFF_RESISTPAIN);
      break;
    case SPELL_ARMOR:
      mod *= sust->force;
      GET_IMPACT(ch) += mod;
      GET_BALLISTIC(ch) += mod;
      break;
    case SPELL_POLTERGEIST:
      if (ch->in_room) {
        if (mod == 1) {
          _char_with_spell_to_room(ch, SPELL_POLTERGEIST, ch->in_room->poltergeist);
        } else {
          _char_with_spell_from_room(ch, SPELL_POLTERGEIST, ch->in_room->poltergeist);
        }
      }
      break;
    case SPELL_LIGHT:
      if (ch->in_room) {
        if (mod == 1) {
          _char_with_spell_to_room(ch, SPELL_LIGHT, ch->in_room->light);
        } else {
          _char_with_spell_from_room(ch, SPELL_LIGHT, ch->in_room->light);
        }
      }
      break;
    case SPELL_SHADOW:
      if (ch->in_room) {
        if (mod == 1) {
          _char_with_spell_to_room(ch, SPELL_SHADOW, ch->in_room->shadow);
        } else {
          _char_with_spell_from_room(ch, SPELL_SHADOW, ch->in_room->shadow);
        }
      }
      break;
    case SPELL_INVIS:
      if (mod == 1)
        AFF_FLAGS(ch).SetBit(AFF_SPELLINVIS);
      else
        AFF_FLAGS(ch).RemoveBit(AFF_SPELLINVIS);
      break;
    case SPELL_IMP_INVIS:
      if (mod == 1)
        AFF_FLAGS(ch).SetBit(AFF_SPELLIMPINVIS);
      else
        AFF_FLAGS(ch).RemoveBit(AFF_SPELLIMPINVIS);
      break;
    case SPELL_SILENCE:
      if (ch->in_room) {
        if (mod == 1) {
          _char_with_spell_to_room(ch, SPELL_SILENCE, ch->in_room->silence);
        } else {
          _char_with_spell_from_room(ch, SPELL_SILENCE, ch->in_room->silence);
        }
      }
      break;
    case SPELL_COMBATSENSE:
      mod *= MIN(sust->force, sust->success / 2);
      GET_COMBAT(ch) += mod;
      break;
    case SPELL_NIGHTVISION:
      if (mod == 1)
        set_vision_bit(ch, VISION_LOWLIGHT, VISION_BIT_IS_SPELL);
      else
        remove_vision_bit(ch, VISION_LOWLIGHT, VISION_BIT_IS_SPELL);
      break;
    case SPELL_INFRAVISION:
      if (mod == 1)
        set_vision_bit(ch, VISION_THERMOGRAPHIC, VISION_BIT_IS_SPELL);
      else
        remove_vision_bit(ch, VISION_THERMOGRAPHIC, VISION_BIT_IS_SPELL);
      break;
    case SPELL_LEVITATE:
      if (mod == 1)
        AFF_FLAGS(ch).SetBit(AFF_LEVITATE);
      else
        AFF_FLAGS(ch).RemoveBit(AFF_LEVITATE);
      break;
    case SPELL_FLAME_AURA:
      if (mod == 1)
        AFF_FLAGS(ch).SetBit(AFF_FLAME_AURA);
      else
        AFF_FLAGS(ch).RemoveBit(AFF_FLAME_AURA);
      break;
  }
}

void affect_total_veh(struct veh_data * veh)
{
  int i, j;
  for (i = 0; i < (NUM_MODS - 1); i++)
  {
    if (GET_MOD(veh, i)) {
      for (j = 0; j < MAX_OBJ_AFFECT; j++) {
        affect_veh(veh, GET_MOD(veh, i)->affected[j].location,
                   GET_MOD(veh, i)->affected[j].modifier);
      }
    }
  }
}

void affect_total(struct char_data * ch)
{
  struct obj_data *cyber, *obj;
  struct sustain_data *sust;
  sh_int i, j, skill_dice;
  int has_rig = 0, has_trigger = -1, has_wired = 0, has_mbw = 0;
  bool wearing = FALSE;
  bool ch_is_npc = IS_NPC(ch);

  if (IS_PROJECT(ch))
    return;

  int old_max_hacking = GET_MAX_HACKING(ch);
  int old_rem_hacking = GET_REM_HACKING(ch);


  /* remove the effects of used equipment */
  for (i = 0; i < (NUM_WEARS - 1); i++)
  {
    if (GET_EQ(ch, i)) {
      if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_WEAPON && i != WEAR_WIELD)
        continue;
      if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_FOCUS)
        remove_focus_effect(ch, GET_EQ(ch, i));
      else
        for (j = 0; j < MAX_OBJ_AFFECT; j++)
          affect_modify(ch,
                        GET_EQ(ch, i)->affected[j].location,
                        GET_EQ(ch, i)->affected[j].modifier,
                        GET_EQ(ch, i)->obj_flags.bitvector, FALSE);
    }
  }

  // remove the effects of foci
  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (GET_OBJ_TYPE(obj) == ITEM_FOCUS)
      remove_focus_effect(ch, obj);

  /* remove the effects of cyberware */
  for (cyber = ch->cyberware; cyber; cyber = cyber->next_content)
  {
    for (j = 0; j < MAX_OBJ_AFFECT; j++)
      affect_modify(ch,
                    cyber->affected[j].location,
                    cyber->affected[j].modifier,
                    cyber->obj_flags.bitvector, FALSE);
  }

  /* remove the effects of bioware */
  for (cyber = ch->bioware; cyber; cyber = cyber->next_content)
  {
    if ((GET_OBJ_VAL(cyber, 0) != BIO_ADRENALPUMP || (GET_OBJ_VAL(cyber, 0) == BIO_ADRENALPUMP))
        && GET_OBJ_VAL(cyber, 5) > 0)
      for (j = 0; j < MAX_OBJ_AFFECT; j++)
        affect_modify(ch,
                      cyber->affected[j].location,
                      cyber->affected[j].modifier,
                      cyber->obj_flags.bitvector, FALSE);
  }

  // remove the effects of spells
  AFF_FLAGS(ch).RemoveBit(AFF_INVISIBLE);
  for (sust = GET_SUSTAINED(ch); sust; sust = sust->next)
    if (!sust->caster)
      spell_modify(ch, sust, FALSE);

  // Because equipment-granted vision AFFs are notoriously sticky, clear them deliberately.
  if (!IS_NPC(ch))
    AFF_FLAGS(ch).RemoveBits(AFF_LOW_LIGHT, AFF_INFRAVISION, AFF_ULTRASOUND, AFF_VISION_MAG_1, AFF_VISION_MAG_2, AFF_VISION_MAG_3, ENDBIT);

  // Wipe out vision bits, resetting them to race-only bits.
  clear_ch_vision_bits(ch);

  // reset the affected ability scores
  ch->aff_abils = ch->real_abils;

  /* calculate reaction before you add eq, cyberware, etc so that things *
   * such as wired reflexes work properly (as they only modify reaction  *
   * and not intelligence and quickness).            -cjd                */
  GET_REAL_REA(ch) = (GET_REAL_INT(ch) + GET_REAL_QUI(ch)) / 2;
  GET_REA(ch) = 0;

  // Apply newbie flag removal.
  if (PLR_FLAGGED(ch, PLR_NEWBIE) && GET_TKE(ch) > NEWBIE_KARMA_THRESHOLD)
    PLR_FLAGS(ch).RemoveBit(PLR_NEWBIE);

  /* set the dice pools before equip so that they can be affected */
  /* combat pool is equal to quickness, wil, and int divided by 2 */
  GET_COMBAT(ch) = 0;
  GET_HACKING(ch) = 0;
  GET_ASTRAL(ch) = 0;
  GET_MAGIC(ch) = 0;
  GET_CONTROL(ch) = 0;

  // Set reach, depending on race. Stripped out the 'you only get it at X height' thing since it's not canon and a newbie trap.
  if ((GET_RACE(ch) == RACE_TROLL || GET_RACE(ch) == RACE_CYCLOPS || GET_RACE(ch) == RACE_FOMORI ||
       GET_RACE(ch) == RACE_GIANT || GET_RACE(ch) == RACE_MINOTAUR) /* && GET_HEIGHT(ch) > 260 */)
    GET_REACH(ch) = 1;
  else
    GET_REACH(ch) = 0;
  // reset initiative dice
  GET_INIT_DICE(ch) = 0;
  /* reset # of foci char has */

  // Reset armor-related stats.
  {
    if (IS_SPIRIT(ch) || IS_ANY_ELEMENTAL(ch)) {
      GET_INNATE_IMPACT(ch) = GET_INNATE_BALLISTIC(ch) = GET_SPARE1(ch) * 2;
    } else if (ch_is_npc) {
      GET_INNATE_BALLISTIC(ch) = GET_INNATE_BALLISTIC(&mob_proto[GET_MOB_RNUM(ch)]);
      GET_INNATE_IMPACT(ch) = GET_INNATE_IMPACT(&mob_proto[GET_MOB_RNUM(ch)]);
    } else {
      GET_INNATE_BALLISTIC(ch) = GET_INNATE_IMPACT(ch) = 0;
    }
    GET_BALLISTIC(ch) = GET_INNATE_BALLISTIC(ch);
    GET_IMPACT(ch) = GET_INNATE_IMPACT(ch);

    GET_TOTALBAL(ch) = GET_TOTALIMP(ch) = 0;
  }

  for (obj = ch->carrying; obj; obj = obj->next_content) {
    if (GET_OBJ_TYPE(obj) == ITEM_FOCUS)
      apply_focus_effect(ch, obj);
  }

  /* effects of equipment */
  {
    struct obj_data *worn_item = NULL;
    // Matched set values: ballistic, impact, and the numerical index of the suit itself.
    int suitbal = 0, suitimp = 0, suittype = 0;
    // Maximums tracking for the /2 logic.
    int highestbal = 0, highestimp = 0;
    // Track the total for worn items.
    int totalbal = 0, totalimp = 0;
    // Track the total for formfit.
    int formfitbal = 0, formfitimp = 0;

    for (i = 0; i < (NUM_WEARS - 1); i++) {
      if ((worn_item = GET_EQ(ch, i))) {
        wearing = TRUE;

        // Foci don't do aff_modify for some reason?
        if (GET_OBJ_TYPE(worn_item) == ITEM_FOCUS) {
          apply_focus_effect(ch, worn_item);
        } else {
          for (j = 0; j < MAX_OBJ_AFFECT; j++)
            affect_modify(ch,
                          worn_item->affected[j].location,
                          worn_item->affected[j].modifier,
                          worn_item->obj_flags.bitvector, TRUE);
        }

        if (GET_OBJ_TYPE(worn_item) == ITEM_WORN) {
          // If it's part of an armor set we're already wearing, add it directly.
          // KNOWN ISSUE: If it finds your single Vashon item, it will check for Vashon only, even if you're wearing a full suit of Trog Moxie as well.
          if (GET_WORN_MATCHED_SET(worn_item) && (!suittype || suittype == GET_WORN_MATCHED_SET(worn_item))) {
            suitbal += GET_WORN_BALLISTIC(worn_item);
            suitimp += GET_WORN_IMPACT(worn_item);
            suittype = GET_WORN_MATCHED_SET(worn_item);
          }

          // Otherwise, if it has an armor rating, handle that.
          else if (GET_WORN_BALLISTIC(worn_item) || GET_WORN_IMPACT(worn_item)) {
            int bal = 0, imp = 0;

            // Remember that matched set items are 100x their expected value-- compensate for this.
            if (GET_WORN_MATCHED_SET(worn_item)) {
              bal = (int)(GET_WORN_BALLISTIC(worn_item) / 100);
              imp = (int)(GET_WORN_IMPACT(worn_item) / 100);
            } else {
              bal = GET_WORN_BALLISTIC(worn_item);
              imp = GET_WORN_IMPACT(worn_item);
            }

            // Keep track of which worn item provides the highest total value.
            if (bal + imp > highestbal + highestimp) {
              highestbal = bal;
              highestimp = imp;
            }

            // Add the value to the current worn armor total.
            totalbal += bal;
            totalimp += imp;

            // If it's formfit, track it there for later math.
            if (IS_OBJ_STAT(worn_item, ITEM_EXTRA_FORMFIT)) {
              formfitbal += bal;
              formfitimp += imp;
            }
          }
        }
      }
    }

    // Add armor clothing set, if any.
    // KNOWN ISSUE: If the sum of bal or imp is x.5, the .5 is dropped here.
    if (suitbal || suitimp) {
      suitbal /= 100;
      suitimp /= 100;

      if (suitbal + suitimp > highestbal + highestimp) {
        highestbal = suitbal;
        highestimp = suitimp;
      }

      totalbal += suitbal;
      totalimp += suitimp;
    }

    GET_IMPACT(ch) += highestimp + (int)((totalimp - highestimp) / 2);
    GET_BALLISTIC(ch) += highestbal + (int)((totalbal - highestbal) / 2);

    GET_TOTALIMP(ch) += totalimp - formfitimp;
    GET_TOTALBAL(ch) += totalbal - formfitbal;

    // CC p45: If the only thing you're wearing that gives armor is your matched set, it doesn't apply penalties.
    /*  Still under discussion, so not enabled yet.
    if ((suitimp || suitbal) && GET_TOTALIMP(ch) == suitimp && GET_TOTALBAL(ch) == suitbal)
      GET_TOTALIMP(ch) = GET_TOTALBAL(ch) = 0;
    */
  }

  if (GET_RACE(ch) == RACE_TROLL || GET_RACE(ch) == RACE_MINOTAUR)
    GET_IMPACT(ch)++;

  /* effects of cyberware */
  for (cyber = ch->cyberware; cyber; cyber = cyber->next_content)
  {
    switch (GET_CYBERWARE_TYPE(cyber)) {
      case CYB_VCR:
        has_rig = GET_CYBERWARE_RATING(cyber);
        break;
      case CYB_REFLEXTRIGGER:
        has_trigger = GET_CYBERWARE_RATING(cyber);
        break;
      case CYB_WIREDREFLEXES:
        has_wired = GET_CYBERWARE_RATING(cyber);
        break;
      case CYB_MOVEBYWIRE:
        has_mbw = GET_CYBERWARE_RATING(cyber);
        break;
      case CYB_DERMALSHEATHING:
      case CYB_DERMALPLATING:
        // Ruthenium sheathing.
        if (GET_CYBERWARE_TYPE(cyber) == CYB_DERMALSHEATHING && GET_OBJ_VAL(cyber, 3) == 1 && !wearing)
          AFF_FLAGS(ch).SetBit(AFF_INVISIBLE);
        // todo
        break;
      case CYB_EYES:
        apply_vision_bits_from_implant(ch, cyber);
        break;
    }

    for (j = 0; j < MAX_OBJ_AFFECT; j++) {
      affect_modify(ch,
                    cyber->affected[j].location,
                    cyber->affected[j].modifier,
                    cyber->obj_flags.bitvector, TRUE);
    }
  }
  if (has_wired && has_trigger) {
    if (has_trigger == -1)
      has_trigger = 3;
    has_wired = MIN(has_wired, has_trigger);
    GET_INIT_DICE(ch) += has_wired;
    GET_REA(ch) += has_wired * 2;
  }
  /* effects of bioware */
  for (cyber = ch->bioware; cyber; cyber = cyber->next_content)
  {
    if (GET_OBJ_VAL(cyber, 0) != BIO_ADRENALPUMP || (GET_OBJ_VAL(cyber, 0) == BIO_ADRENALPUMP &&
                                                     GET_OBJ_VAL(cyber, 5) > 0))
      for (j = 0; j < MAX_OBJ_AFFECT; j++)
        affect_modify(ch,
                      cyber->affected[j].location,
                      cyber->affected[j].modifier,
                      cyber->obj_flags.bitvector, TRUE);
  }

  for (sust = GET_SUSTAINED(ch); sust; sust = sust->next)
    if (!sust->caster)
      spell_modify(ch, sust, TRUE);

  if (GET_TEMP_QUI_LOSS(ch))
    GET_QUI(ch) = MAX(0, GET_QUI(ch) - (GET_TEMP_QUI_LOSS(ch) / TEMP_QUI_LOSS_DIVISOR));

  for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content) {
    switch (GET_BIOWARE_TYPE(bio)) {
      case BIO_PAINEDITOR:
        if (GET_BIOWARE_IS_ACTIVATED(bio)) {
          GET_WIL(ch) += 1;
          GET_INT(ch) -= 1;
        }
        break;
      case BIO_CATSEYES:
        // M&M p45
        apply_vision_bits_from_implant(ch, bio);
        break;
    }
  }

  i = ((ch_is_npc || (GET_LEVEL(ch) >= LVL_ADMIN)) ? 50 : 20);
  GET_REA(ch) += (GET_INT(ch) + GET_QUI(ch)) >> 1;
  GET_QUI(ch) = MAX(0, MIN(GET_QUI(ch), i));
  GET_CHA(ch) = MAX(1, MIN(GET_CHA(ch), i));
  GET_INT(ch) = MAX(1, MIN(GET_INT(ch), i));
  GET_WIL(ch) = MAX(1, MIN(GET_WIL(ch), i));
  GET_BOD(ch) = MAX(1, MIN(GET_BOD(ch), i));
  GET_STR(ch) = MAX(1, MIN(GET_STR(ch), i));
  GET_MAG(ch) = MAX(0, MIN(GET_MAG(ch), i * 100));
  GET_ESS(ch) = MAX(0, MIN(GET_ESS(ch), 600));
  GET_REA(ch) = MAX(1, MIN(GET_REA(ch), i));
  GET_MAG(ch) -= MIN(GET_MAG(ch), GET_TEMP_MAGIC_LOSS(ch) * 100);
  GET_ESS(ch) -= GET_TEMP_ESSLOSS(ch);
  GET_MAX_MENTAL(ch) = 1000;
  GET_MAX_PHYSICAL(ch) = 1000;
  GET_TARGET_MOD(ch) = 0;
  GET_CONCENTRATION_TARGET_MOD(ch) = 0;
  GET_MAX_MENTAL(ch) -= GET_MENTAL_LOSS(ch) * 100;
  GET_MAX_PHYSICAL(ch) -= GET_PHYSICAL_LOSS(ch) * 100;

  if (GET_TRADITION(ch) == TRAD_ADEPT)
  {
    if (GET_INIT_DICE(ch) == 0)
      GET_INIT_DICE(ch) += MIN(3, GET_POWER(ch, ADEPT_REFLEXES));
    if (GET_REAL_REA(ch) == GET_REA(ch))
      GET_REA(ch) += 2*MIN(3, GET_POWER(ch, ADEPT_REFLEXES));
    if (GET_POWER(ch, ADEPT_IMPROVED_QUI)) {
      GET_REA(ch) -= (GET_QUI(ch) + GET_INT(ch)) / 2;
      GET_QUI(ch) += GET_POWER(ch, ADEPT_IMPROVED_QUI);
      GET_REA(ch) += (GET_QUI(ch) + GET_INT(ch)) / 2;
    }
    GET_BOD(ch) += GET_POWER(ch, ADEPT_IMPROVED_BOD);
    GET_STR(ch) += GET_POWER(ch, ADEPT_IMPROVED_STR);
    if (BOOST(ch)[STR][0] > 0)
      GET_STR(ch) += BOOST(ch)[STR][1];
    if (BOOST(ch)[QUI][0] > 0)
      GET_QUI(ch) += BOOST(ch)[QUI][1];
    if (BOOST(ch)[BOD][0] > 0)
      GET_BOD(ch) += BOOST(ch)[BOD][1];
    GET_COMBAT(ch) += MIN(3, GET_POWER(ch, ADEPT_COMBAT_SENSE));
    if (GET_POWER(ch, ADEPT_LOW_LIGHT)) {
      set_vision_bit(ch, VISION_LOWLIGHT, VISION_BIT_IS_ADEPT_POWER);
    }
    if (GET_POWER(ch, ADEPT_THERMO)) {
      set_vision_bit(ch, VISION_THERMOGRAPHIC, VISION_BIT_IS_ADEPT_POWER);
    }
    if (GET_POWER(ch, ADEPT_IMAGE_MAG)) {
      AFF_FLAGS(ch).SetBit(AFF_VISION_MAG_2);
    }
  }

  apply_drug_modifiers_to_ch(ch);

  skill_dice = get_skill_dice_in_use_for_weapons(ch);

  GET_COMBAT(ch) += (GET_QUI(ch) + GET_WIL(ch) + GET_INT(ch)) / 2;
  if (GET_TOTALBAL(ch) > GET_QUI(ch))
    GET_COMBAT(ch) -= (GET_TOTALBAL(ch) - GET_QUI(ch)) / 2;
  if (GET_TOTALIMP(ch) > GET_QUI(ch))
    GET_COMBAT(ch) -= (GET_TOTALIMP(ch) - GET_QUI(ch)) / 2;
  if (GET_COMBAT(ch) < 0)
    GET_COMBAT(ch) = 0;
  if (GET_TRADITION(ch) == TRAD_ADEPT)
    GET_IMPACT(ch) += GET_POWER(ch, ADEPT_MYSTIC_ARMOR);

  // Apply gyromount penalties, but only if you're wielding a gun.
  // TODO: Ideally, this would only apply if you have uncompensated recoil, but that's a looot of code.
  struct obj_data *wielded = GET_EQ(ch, WEAR_WIELD);
  if (wielded && GET_OBJ_TYPE(wielded) == ITEM_WEAPON && IS_GUN(GET_WEAPON_ATTACK_TYPE(wielded))) {
    bool added_gyro_penalty = FALSE;
    for (i = 0; !added_gyro_penalty && i < NUM_WEARS; i++) {
      if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_GYRO) {
        added_gyro_penalty = TRUE;
        GET_COMBAT(ch) /= 2;
      }
    }

    if (!added_gyro_penalty) {
      for (struct obj_data *cyb = ch->cyberware; !added_gyro_penalty && cyb; cyb = cyb->next_content) {
        if (GET_CYBERWARE_TYPE(cyb) == CYB_ARMS && IS_SET(GET_CYBERWARE_FLAGS(cyb), ARMS_MOD_GYROMOUNT) && !GET_CYBERWARE_IS_DISABLED(cyb)) {
          added_gyro_penalty = TRUE;
          GET_COMBAT(ch) /= 2;
        }
      }
    }
  }

  GET_DEFENSE(ch) = MIN(GET_DEFENSE(ch), GET_COMBAT(ch));
  GET_BODY(ch) = MIN(GET_BODY(ch), GET_COMBAT(ch) - GET_DEFENSE(ch));
  GET_OFFENSE(ch) = GET_COMBAT(ch) - GET_DEFENSE(ch) - GET_BODY(ch);
  if (GET_OFFENSE(ch) > skill_dice)
  {
    GET_DEFENSE(ch) += GET_OFFENSE(ch) - skill_dice;
    GET_OFFENSE(ch) = skill_dice;
  }

  // NPCs specialize their defenses: unless they've got crazy dodge dice, they'll want to just soak.
  // AKA, 'your average wage slave is not going to try to do the Matrix bullet dodge when he sees a gun.'
  if (ch_is_npc) {
    if (GET_DEFENSE(ch) < 10) {
      GET_BODY(ch) += GET_DEFENSE(ch);
      GET_DEFENSE(ch) = 0;
    }

    // NPC spirits set their astral pools here as well.
    if (IS_SPIRIT(ch) || IS_ANY_ELEMENTAL(ch)) {
      GET_ASTRAL(ch) = 1.5 * GET_LEVEL(ch);
    }
  }

  // Set up magic pool info correctly.
  if (GET_MAG(ch) > 0) {
    // Astral pools are based off their init grade.
    GET_ASTRAL(ch) += GET_GRADE(ch);

    // Set magic pools from (int + wil + mag) / 3.
    GET_MAGIC(ch) += (GET_INT(ch) + GET_WIL(ch) + (int)(GET_MAG(ch) / 100))/3;

    if (ch_is_npc) {
      // For NPCs, we zero all the components of the magic pool, then re-set them.
      GET_REFLECT(ch) = GET_CASTING(ch) = GET_DRAIN(ch) = GET_SDEFENSE(ch) = 0;

      // If they plan to cast, they split their dice evenly.
      if (GET_SKILL(ch, SKILL_SORCERY)) {
        GET_CASTING(ch) = GET_DRAIN(ch) = GET_SDEFENSE(ch) = GET_MAGIC(ch) / 3;
      }
      // Otherwise, they put it all in defense.
      else {
        GET_SDEFENSE(ch) = GET_MAGIC(ch);
      }

      // High-grade NPC mages get reflect instead of spell defense.
      if (MAX(GET_SKILL(ch, SKILL_SORCERY), (int) (GET_MAGIC(ch) / 100)) >= 9) {
        GET_REFLECT(ch) = GET_SDEFENSE(ch);
        GET_SDEFENSE(ch) = 0;
      }
    } else {
      // Only Shamans and Hermetics get these pools.
      if (GET_TRADITION(ch) == TRAD_SHAMANIC || GET_TRADITION(ch) == TRAD_HERMETIC) {
        GET_SDEFENSE(ch) = MIN(GET_MAGIC(ch), GET_SDEFENSE(ch));
        GET_DRAIN(ch) = MIN(GET_MAGIC(ch), GET_DRAIN(ch));
        GET_REFLECT(ch) = MIN(GET_MAGIC(ch), GET_REFLECT(ch));

        // Chuck the remaining dice into the casting pool.
        GET_CASTING(ch) = MAX(0, GET_MAGIC(ch) - GET_DRAIN(ch) - GET_REFLECT(ch) - GET_SDEFENSE(ch));
      } else {
        GET_CASTING(ch) = GET_MAGIC(ch) = GET_SDEFENSE(ch) = GET_DRAIN(ch) = GET_REFLECT(ch) = 0;
      }
    }
  }

  if (REAL_SKILL(ch, SKILL_COMPUTER) > 0)
  {
    int mpcp = 0;
    if (PLR_FLAGGED(ch, PLR_MATRIX) && ch->persona) {
      GET_HACKING(ch) += (int)((GET_INT(ch) + ch->persona->decker->mpcp) / 3);
    } else {
      for (struct obj_data *deck = ch->carrying; deck; deck = deck->next_content)
        if (GET_OBJ_TYPE(deck) == ITEM_CYBERDECK || GET_OBJ_TYPE(deck) == ITEM_CUSTOM_DECK) {
          mpcp = GET_OBJ_VAL(deck, 0);
          break;
        }
      mpcp += GET_INT(ch);
      GET_HACKING(ch) += (int)(mpcp / 3);
    }
  }

  if (has_rig)
  {
    int reaction = GET_REAL_INT(ch) + GET_REAL_QUI(ch);
    for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content)
      if (GET_OBJ_VAL(bio, 0) == BIO_CEREBRALBOOSTER)
        reaction += GET_OBJ_VAL(bio, 1);
    GET_CONTROL(ch) += (reaction / 2) + (int)(2 * has_rig);
    GET_HACKING(ch) -= has_rig;
    if (GET_HACKING(ch) < 0)
      GET_HACKING(ch) = 0;
  }

  // Restore their max_hacking and rem_hacking, which were wiped out in the earlier aff_abils = real_abils.
  GET_REM_HACKING(ch) = MIN(old_rem_hacking, GET_HACKING(ch));
  GET_MAX_HACKING(ch) = MIN(old_max_hacking, GET_HACKING(ch));

  if (has_mbw) {
    GET_QUI(ch) += has_mbw;
    GET_REA(ch) += has_mbw * 2;
    GET_INIT_DICE(ch) += has_mbw;
  }

  // Update current vision to match what's being worn.
  if (AFF_FLAGGED(ch, AFF_INFRAVISION)) {
    set_vision_bit(ch, VISION_THERMOGRAPHIC, VISION_BIT_FROM_EQUIPMENT);
  }
  if (AFF_FLAGGED(ch, AFF_LOW_LIGHT)) {
    set_vision_bit(ch, VISION_LOWLIGHT, VISION_BIT_FROM_EQUIPMENT);
  }
  if (AFF_FLAGGED(ch, AFF_ULTRASOUND)) {
    set_vision_bit(ch, VISION_ULTRASONIC, VISION_BIT_FROM_EQUIPMENT);
  }

  // Strip invisibility from ruthenium etc if you're wearing about or body items that aren't also ruthenium.
  if (AFF_FLAGGED(ch, AFF_INVISIBLE) || AFF_FLAGGED(ch, AFF_IMP_INVIS))
  {
    if (GET_EQ(ch, WEAR_ABOUT)) {
      if (!(GET_OBJ_AFFECT(GET_EQ(ch, WEAR_ABOUT)).IsSet(AFF_INVISIBLE)
            || GET_OBJ_AFFECT(GET_EQ(ch, WEAR_ABOUT)).IsSet(AFF_IMP_INVIS))) {
        AFF_FLAGS(ch).RemoveBits(AFF_INVISIBLE, AFF_IMP_INVIS, ENDBIT);
      }
    }
    else if (GET_EQ(ch, WEAR_BODY)
             && (!(GET_OBJ_AFFECT(GET_EQ(ch, WEAR_BODY)).IsSet(AFF_INVISIBLE)
                   || GET_OBJ_AFFECT(GET_EQ(ch, WEAR_BODY)).IsSet(AFF_IMP_INVIS))))
    {
      AFF_FLAGS(ch).RemoveBits(AFF_INVISIBLE, AFF_IMP_INVIS, ENDBIT);
    }
  }

  // Apply reach from weapon, if any.
  struct obj_data *weapon = GET_EQ(ch, WEAR_WIELD);
  if (weapon && GET_OBJ_TYPE(weapon) == ITEM_WEAPON) {
    // Melee weapons grant reach according to their set stat.
    if (!IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))) {
      if (GET_WEAPON_REACH(weapon) > 0)
        GET_REACH(ch) += GET_WEAPON_REACH(weapon);
    }
    // Ranged weapons grant 0 reach if pistol, 2 reach if anything else with a bayonet, 1 reach for all other categories.
    else {
      if (GET_WEAPON_SKILL(weapon) != SKILL_PISTOLS && GET_WEAPON_SKILL(weapon) != SKILL_TASERS && GET_WEAPON_SKILL(weapon) != SKILL_SMG) // Anything under rifle-sized has 0 reach. - Vile
        GET_REACH(ch)++;
      struct obj_data *attach = get_obj_proto_for_vnum(GET_WEAPON_ATTACH_UNDER_VNUM(weapon));
      if (attach && GET_ACCESSORY_TYPE(attach) == ACCESS_BAYONET)
        GET_REACH(ch)++;
    }
  }
}

/*
 * Return if a char is affected by a spell (SPELL_XXX), NULL indicates
 * not affected
 */
int affected_by_spell(struct char_data * ch, int type)
{
  if (!GET_SUSTAINED(ch))
    return 0;

  for (struct sustain_data *hjp = GET_SUSTAINED(ch); hjp; hjp = hjp->next)
    if ((hjp->spell == type) && (hjp->caster == FALSE))
      return hjp->force;

  return 0;
}

int affected_by_power(struct char_data *ch, int type)
{
  for (struct spirit_sustained *sust = SPIRIT_SUST(ch); sust ; sust = sust->next)
    if (sust->type == type)
      return MAX(1, sust->force); // MAX here guarantees this matches the previous bool return.
  return FALSE;
}
void veh_from_room(struct veh_data * veh)
{
  struct veh_data *temp;
  if (veh == NULL) {
    log("SYSERR: Null vehicle passed to veh_from_room. Terminating.");
    exit(ERROR_NULL_VEHICLE_VEH_FROM_ROOM);
  }
  if (!veh->in_room && !veh->in_veh) {
    log("SYSERR: veh->in_room and veh->in_veh are both null; did you call veh_from_room twice?");
    return;
  }
  if (veh->in_veh) {
    REMOVE_FROM_LIST(veh, veh->in_veh->carriedvehs, next_veh);
    int mult;
    switch (veh->type) {
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
    veh->in_veh->usedload -= veh->body * mult;
  } else {
    REMOVE_FROM_LIST(veh, veh->in_room->vehicles, next_veh);
    veh->in_room->light[ROOM_LIGHT_HEADLIGHTS_AND_FLASHLIGHTS]--;
  }
  veh->in_room = NULL;
  veh->next_veh = NULL;
  veh->in_veh = NULL;
}

/* move a player out of a room */
void char_from_room(struct char_data * ch)
{
  struct char_data *temp;

  if (ch == NULL || (!ch->in_room && !ch->in_veh)) {
    log("SYSERR: NULL in handler.c, char_from_room");
    shutdown();
  }

  if (IS_WORKING(ch)) {
    // send_to_char("You stop working.\r\n", ch);
    STOP_WORKING(ch);
  }

  if (CH_IN_COMBAT(ch))
    stop_fighting(ch);

  if (ch->in_room) {
    // Character is in a room. Clean up room effects sourced by character.
    _char_with_light_from_room(ch);

    _char_with_spell_from_room(ch, SPELL_SILENCE, ch->in_room->silence);
    _char_with_spell_from_room(ch, SPELL_SHADOW, ch->in_room->shadow);
    _char_with_spell_from_room(ch, SPELL_LIGHT, ch->in_room->light);
    _char_with_spell_from_room(ch, SPELL_POLTERGEIST, ch->in_room->poltergeist);

    if (IS_SENATOR(ch) && PRF_FLAGGED(ch, PRF_PACIFY) && ch->in_room->peaceful > 0)
      ch->in_room->peaceful--;

    // Remove them from the room.
    REMOVE_FROM_LIST(ch, ch->in_room->people, next_in_room);
    ch->in_room = NULL;
    ch->next_in_room = NULL;
    CHAR_X(ch) = CHAR_Y(ch) = 0;
  }

  if (ch->in_veh) {
    // Character is in a vehicle. Remove them from it.
    REMOVE_FROM_LIST(ch, ch->in_veh->people, next_in_veh);
    stop_manning_weapon_mounts(ch, TRUE);
    ch->in_veh->seating[ch->vfront]++;
    ch->in_veh = NULL;
    ch->next_in_veh = NULL;
    AFF_FLAGS(ch).RemoveBits(AFF_PILOT, AFF_RIG, ENDBIT);
  }
}

void char_to_veh(struct veh_data * veh, struct char_data * ch)
{
  if (!veh || !ch)
    mudlog("SYSLOG: Illegal value(s) passed to char_to_veh", NULL, LOG_SYSLOG, TRUE);
  else {
    if (ch->in_room || ch->in_veh)
      char_from_room(ch);

    // Remove vehicle brain if one exists.
    remove_vehicle_brain(veh);

    ch->next_in_veh = veh->people;
    veh->people = ch;
    ch->in_veh = veh;
    veh->seating[ch->vfront]--;
    GET_POS(ch) = POS_SITTING;
  }
}

void veh_to_room(struct veh_data * veh, struct room_data *room)
{
  if (!veh || !room)
    mudlog("SYSLOG: Illegal value(s) passed to veh_to_room", NULL, LOG_SYSLOG, TRUE);
  else
  {
    if (veh->in_veh || veh->in_room)
      veh_from_room(veh);

    veh->next_veh = room->vehicles;
    room->vehicles = veh;
    veh->in_room = room;
    room->light[ROOM_LIGHT_HEADLIGHTS_AND_FLASHLIGHTS]++; // headlights
  }
}

void veh_to_veh(struct veh_data *veh, struct veh_data *dest)
{
  if (!veh || !dest)
    mudlog("SYSLOG: Illegal value(s) passed to veh_to_veh", NULL, LOG_SYSLOG, TRUE);
  else {
    if (veh->in_veh || veh->in_room)
      veh_from_room(veh);

    veh->next_veh = dest->carriedvehs;
    veh->in_veh = dest;
    dest->carriedvehs = veh;
    dest->usedload += calculate_vehicle_entry_load(veh);
  }
}
void icon_to_host(struct matrix_icon *icon, vnum_t to_host)
{
  extern void make_seen(struct matrix_icon *icon, int idnum);
  if (!icon || to_host < 0 || to_host > top_of_matrix)
    mudlog("SYSLOG: Illegal value(s) passed to icon_to_host", NULL, LOG_SYSLOG, TRUE);
  else
  {
    if (icon->decker)
      for (struct matrix_icon *icon2 = matrix[to_host].icons; icon2; icon2 = icon2->next_in_host)
        if (icon2->decker) {
          int target = icon->decker->masking;
          for (struct obj_data *soft = icon->decker->software; soft; soft = soft->next_content)
            if (GET_OBJ_VAL(soft, 0) == SOFT_SLEAZE)
              target += GET_OBJ_VAL(soft, 1);
          if (success_test(icon2->decker->sensor, target) > 0) {
            make_seen(icon2, icon->idnum);
            send_to_icon(icon2, "%s enters the host.\r\n", icon->name);
          }
        }
    icon->next_in_host = matrix[to_host].icons;
    matrix[to_host].icons = icon;
    icon->in_host = to_host;
  }
}

void icon_from_host(struct matrix_icon *icon)
{
  struct matrix_icon *temp;
  if (icon == NULL || icon->in_host == NOWHERE)
  {
    log("If thats happens to much im going to make it intentionally segfault.\r\nPS. Don't you just love my pointless debugging messages :)");
    return;
  }
  REMOVE_FROM_LIST(icon, matrix[icon->in_host].icons, next_in_host);
  REMOVE_FROM_LIST(icon, matrix[icon->in_host].fighting, next_fighting);
  temp = NULL;
  for (struct matrix_icon *icon2 = matrix[icon->in_host].icons; icon2; icon2 = icon2->next_in_host)
  {
    if (icon2->fighting == icon) {
      icon2->fighting = NULL;
      REMOVE_FROM_LIST(icon2, matrix[icon->in_host].fighting, next_fighting);
    }
  }

  // Clean up their uploads.
  if (icon->decker && icon->decker->deck) {
    for (struct obj_data *soft = icon->decker->deck->contains; soft; soft = soft->next_content)
      if (GET_OBJ_TYPE(soft) != ITEM_PART && GET_OBJ_VAL(soft, 9) > 0 && GET_OBJ_VAL(soft, 8) == 1)
        GET_OBJ_VAL(soft, 9) = GET_OBJ_VAL(soft, 8) = 0;
  }

  // Unlink any files that have been claimed by this icon.
  if (icon->idnum) {
    for (struct obj_data *soft = matrix[icon->in_host].file, *next_file; soft; soft = next_file) {
      next_file = soft->next_content;

      if (GET_OBJ_TYPE(soft) == ITEM_DECK_ACCESSORY || GET_OBJ_TYPE(soft) == ITEM_PROGRAM) {
        // Stop in-progress downloads.
        if (GET_DECK_ACCESSORY_FILE_WORKER_IDNUM(soft) == icon->idnum) {
          GET_DECK_ACCESSORY_FILE_WORKER_IDNUM(soft) = 0;
          GET_DECK_ACCESSORY_FILE_REMAINING(soft) = 0;
        }

        // Unlock found items so others can find them.
        if (GET_DECK_ACCESSORY_FILE_FOUND_BY(soft) == icon->idnum) {
          GET_DECK_ACCESSORY_FILE_FOUND_BY(soft) = 0;
          GET_DECK_ACCESSORY_FILE_WORKER_IDNUM(soft) = 0;
          GET_DECK_ACCESSORY_FILE_REMAINING(soft) = 0;

          // If it's paydata, we extract it entirely, then potentially put it back in the paydata queue to be rediscovered.
          if (GET_OBJ_TYPE(soft) == ITEM_DECK_ACCESSORY
              && GET_DECK_ACCESSORY_TYPE(soft) == TYPE_FILE
              && GET_DECK_ACCESSORY_FILE_HOST_VNUM(soft) == matrix[icon->in_host].vnum)
          {
            // 66% chance of being rediscoverable.
            if (number(0, 2)) {
              matrix[icon->in_host].undiscovered_paydata++;
            }

            extract_obj(soft);
            continue;
          }
        }
      }
    }
  }

  icon->in_host = NOWHERE;
  icon->next_in_host = NULL;
  icon->fighting = NULL;
}

/* place a character in a room */
void char_to_room(struct char_data * ch, struct room_data *room)
{
  if (!ch || !room)
  {
    log("SYSLOG: Illegal value(s) passed to char_to_room");
    room = &world[0];
  }

  // Warn on exceeding privileges, but don't fail.
  if (builder_cant_go_there(ch, room)) {
    mudlog("Warning: Builder exceeding allowed bounds. Make sure their loadroom etc is set properly.", ch, LOG_WIZLOG, TRUE);
  }

  ch->next_in_room = room->people;
  room->people = ch;
  ch->in_room = room;
  room->dirty_bit = TRUE;
  if (IS_SENATOR(ch) && PRF_FLAGGED(ch, PRF_PACIFY))
    room->peaceful++;

  if (GET_TRADITION(ch) == TRAD_SHAMANIC)
    GET_DOMAIN(ch) = SECT(ch->in_room);

  _char_with_light_to_room(ch);

  _char_with_spell_to_room(ch, SPELL_SILENCE, ch->in_room->silence);
  _char_with_spell_to_room(ch, SPELL_SHADOW, ch->in_room->shadow);
  _char_with_spell_to_room(ch, SPELL_LIGHT, ch->in_room->light);
  _char_with_spell_to_room(ch, SPELL_POLTERGEIST, ch->in_room->poltergeist);
}

#define IS_INVIS(o) IS_OBJ_STAT(o, ITEM_EXTRA_INVISIBLE)

// Checks obj_to_x preconditions for common errors. Overwrites buf3. Returns TRUE for kosher, FALSE otherwise.
bool check_obj_to_x_preconditions(struct obj_data * object, struct char_data *ch) {
  if (!object) {
    mudlog("ERROR: Null object passed to check_obj_to_x_preconditions().", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  // Pre-compose our message header.
  snprintf(buf3, sizeof(buf3), "ERROR: check_obj_to_x_preconditions() failure for %s (%ld): \r\n", GET_OBJ_NAME(object), GET_OBJ_VNUM(object));
  size_t starting_strlen = strlen(buf3);

  // We can't give an object away that's already someone else's possession.
  if (object->carried_by) {
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "- Object already carried by %s.\r\n", GET_CHAR_NAME(object->carried_by));
  }

  // We can't give an object away if it's sitting in a room.
  if (object->in_room) {
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "- Object is already in room %ld.\r\n", object->in_room->number);
  }

  // We can't give an object away if it's in a vehicle.
  if (object->in_veh) {
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "- Object is already in vehicle %s.\r\n", GET_VEH_NAME(object->in_veh));
  }

  // We can't give an object away if it's in another object.
  if (object->in_obj) {
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "- Object is already in object %s.\r\n", GET_OBJ_NAME(object->in_obj));
  }

  // We can't give an object away if it's in a host.
  if (object->in_host) {
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "- Object is already in host %ld.\r\n", object->in_host->vnum);
  }

  // Fail if the object already has next_content. This implies that it's part of someone else's linked list-- never merge them!
  if (object->next_content) {
    strlcat(buf3, "- It's already part of a next_content linked list.\r\n", sizeof(buf3));
  }

  // Can't give away something that's worn by someone else.
  if (object->worn_by) {
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "- Object already carried by %s.\r\n", GET_CHAR_NAME(object->carried_by));
  }

  // If we added anything, log it.
  if (starting_strlen != strlen(buf3)) {
    const char *representation = generate_new_loggable_representation(object);
    strlcat(buf3, representation, sizeof(buf3));
    mudlog(buf3, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  strlcpy(buf3, "", sizeof(buf3));
  return TRUE;
}

/* give an object to a char   */
void obj_to_char(struct obj_data * object, struct char_data * ch)
{
  struct obj_data *i = NULL, *op = NULL;

  // Check our object-related preconditions. All error logging is done there.
  if (!check_obj_to_x_preconditions(object, ch)) {
    return;
  }

  // Precondition: The character in question must exist.
  if (!ch) {
    mudlog("SYSLOG: NULL char passed to obj_to_char", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // Iterate over the objects that the character already has.
  for (i = ch->carrying; i; i = i->next_content) {
    // Attempt at additional error detection.
    if (object == i) {
      mudlog("ERROR: i == object in obj_to_char(). How we even got here, I don't know.", ch, LOG_SYSLOG, TRUE);
      return;
    }

    // If their inventory list has turned into an infinite loop due to a bug, warn about it and bail out here instead of hanging the MUD.
    if (i == i->next_content) {
      snprintf(buf3, sizeof(buf3), "ERROR: Infinite loop detected in obj_to_char. Looping object is %s (%ld). Bailing out, %s is not getting %s %s (%ld).",
              GET_OBJ_NAME(i), GET_OBJ_VNUM(i),
              GET_CHAR_NAME(ch) ? GET_CHAR_NAME(ch) : GET_NAME(ch), HSHR(ch),
              GET_OBJ_NAME(object), GET_OBJ_VNUM(object));
      mudlog(buf3, ch, LOG_SYSLOG, TRUE);
      return;
    }
    // If they already have a copy of this object, break out of the loop while preserving i as pointing to that copy and op as pointing to the last thing we processed (if anything).
    if (i->item_number == object->item_number && !strcmp(i->text.room_desc, object->text.room_desc)) {
      break;
    }
    op = i;
  }

  // If i exists (e.g. we broke out of the loop early by finding a match to this item we're trying to give the character)...
  if (i) {
    // Set our to-give object's next_content to the matching item i.
    object->next_content = i;
    // If op exists (which points to the object we evaluated immediately before i), point its next_content to object. Essentially this is a linked list insert.
    if (op) {
      op->next_content = object;
    }
    // Otherwise, we know our object that we're giving them is the head of their carrying list-- set it as such.
    else {
      ch->carrying = object;
    }
  }
  // Otherwise, stick the new object at the head of their inventory list.
  else {
    object->next_content = ch->carrying;
    ch->carrying = object;
  }

  // Set the object as being carried by this character, and increase their carry weight and carry number accordingly.
  object->carried_by = ch;
  IS_CARRYING_W(ch) += GET_OBJ_WEIGHT(object);
  IS_CARRYING_N(ch)++;

  // Apply focus effects as needed.
  if (GET_OBJ_TYPE(object) == ITEM_FOCUS) {
    apply_focus_effect(ch, object);
  }
}

void obj_to_cyberware(struct obj_data * object, struct char_data * ch)
{
  // Check our object-related preconditions. All error logging is done there.
  if (!check_obj_to_x_preconditions(object, ch))
    return;

  // Precondition: The character in question must exist.
  if (!ch) {
    mudlog("SYSLOG: NULL char passed to obj_to_cyberware", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // It's gotta be cyberware in order to be a valid obj_to_cyberware target.
  if (GET_OBJ_TYPE(object) != ITEM_CYBERWARE) {
    log("Non-cyberware object type passed to obj_to_cyberware.");
    return;
  }

  object->next_content = ch->cyberware;
  ch->cyberware = object;
  object->carried_by = ch;
  object->in_room = NULL;
  object->in_veh = NULL;
  affect_total(ch);
}

void obj_to_bioware(struct obj_data * object, struct char_data * ch)
{
  int temp;

  // Check our object-related preconditions. All error logging is done there.
  if (!check_obj_to_x_preconditions(object, ch))
    return;

  // Precondition: The character in question must exist.
  if (!ch) {
    mudlog("SYSLOG: NULL char passed to obj_to_bioware", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // It's gotta be bioware to be a valid obj_to_bioware target.
  if (GET_OBJ_TYPE(object) != ITEM_BIOWARE) {
    log("Non-bioware object type passed to obj_to_bioware.");
    return;
  }

  object->next_content = ch->bioware;
  ch->bioware = object;
  object->carried_by = ch;
  object->in_room = NULL;
  object->in_veh = NULL;

  if (GET_OBJ_VAL(object, 0) != BIO_ADRENALPUMP || GET_OBJ_VAL(object, 5) > 0)
    for (temp = 0; temp < MAX_OBJ_AFFECT; temp++)
      affect_modify(ch,
                    object->affected[temp].location,
                    object->affected[temp].modifier,
                    object->obj_flags.bitvector, TRUE);

  affect_total(ch);
}

void obj_from_bioware(struct obj_data *bio)
{
  struct obj_data *temp;

  if (bio == NULL)
  {
    mudlog("SYSERR: NULL bioware passed to obj_from_bioware!", NULL, LOG_SYSLOG, TRUE);
    return;
  }
  if (!bio->carried_by) {
    mudlog("SYSERR: Bioware with NO carried_by passed to obj_from_bioware!", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  struct char_data *ch = bio->carried_by;

  /* remove the effects of bioware */
  for (struct obj_data *tmp_bio = ch->bioware; tmp_bio; tmp_bio = tmp_bio->next_content)
  {
    // Remove the effects of this bioware OR activated adrenal pump.
    if (GET_BIOWARE_TYPE(tmp_bio) != BIO_ADRENALPUMP || GET_OBJ_VAL(tmp_bio, 5) > 0) {
      for (int j = 0; j < MAX_OBJ_AFFECT; j++)
        affect_modify(ch,
                      tmp_bio->affected[j].location,
                      tmp_bio->affected[j].modifier,
                      tmp_bio->obj_flags.bitvector, FALSE);
    }
  }

  REMOVE_FROM_LIST(bio, ch->bioware, next_content);
  bio->carried_by = NULL;
  bio->next_content = NULL;

  affect_total(ch);
}

/* take an object from a char */
void obj_from_char(struct obj_data * object)
{
  struct obj_data *temp = NULL;

  if (object == NULL)
  {
    mudlog("ERROR: NULL object passed to obj_from_char", NULL, LOG_SYSLOG, TRUE);
    return;
  }
  if (object->carried_by == NULL) {
    mudlog("ERROR: obj_from_char() called on obj that had no carried_by!", NULL, LOG_SYSLOG, TRUE);
    return;
  }
  if (object->in_obj)
  {
    snprintf(buf, sizeof(buf), "DEBUG: %s removed from char (%s), also in obj (%s). Removing.", object->text.name,
            GET_CHAR_NAME(object->carried_by) ? GET_CHAR_NAME(object->carried_by) :
            GET_NAME(object->carried_by), object->in_obj->text.name);
    mudlog(buf, object->carried_by, LOG_SYSLOG, TRUE);
    obj_from_obj(object);
  }
  REMOVE_FROM_LIST(object, object->carried_by->carrying, next_content);

  IS_CARRYING_W(object->carried_by) -= GET_OBJ_WEIGHT(object);
  IS_CARRYING_N(object->carried_by)--;
  object->carried_by = NULL;
  object->next_content = NULL;
}

/* Removes a piece of cyberware from the cyberware list */
void obj_from_cyberware(struct obj_data * cyber)
{
  struct obj_data *temp;
  if (cyber == NULL)
  {
    mudlog("SYSERR: NULL object passed to obj_from_cyberware!", NULL, LOG_SYSLOG, TRUE);
    return;
  }
  if (!cyber->carried_by) {
    mudlog("SYSERR: Cyberware with NO carried_by passed to obj_from_cyberware!", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  struct char_data *ch = cyber->carried_by;

  /* remove the effects of cyberware */
  for (struct obj_data *tmp_cyber = cyber->carried_by->cyberware; tmp_cyber; tmp_cyber = tmp_cyber->next_content)
  {
    for (int j = 0; j < MAX_OBJ_AFFECT; j++)
      affect_modify(ch,
                    tmp_cyber->affected[j].location,
                    tmp_cyber->affected[j].modifier,
                    tmp_cyber->obj_flags.bitvector, FALSE);
  }

  REMOVE_FROM_LIST(cyber, ch->cyberware, next_content);
  cyber->carried_by = NULL;
  cyber->next_content = NULL;

  affect_total(ch);
}

bool equip_char(struct char_data * ch, struct obj_data * obj, int pos)
{
  int j;

  if (!obj) {
    mudlog("SYSERR: Null object passed to equip_char.", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (!ch) {
    mudlog("SYSERR: Null character passed to equip_char.", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (GET_EQ(ch, pos))
  {
    char errbuf[1000];
    snprintf(errbuf, sizeof(errbuf), "SYSERR: Char is already equipped: %s, %s",
                 GET_NAME(ch), obj->text.name);
    mudlog(errbuf, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  if (obj->carried_by)
  {
    mudlog("SYSERR: EQUIP: Obj is carried_by when equip.", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  if (obj->in_room)
  {
      mudlog("SYSERR: EQUIP: Obj is in_room when equip.", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  if (IS_OBJ_STAT(obj, ITEM_EXTRA_STAFF_ONLY) && !IS_NPC(ch) && !IS_SENATOR(ch))
  {
    act("You are zapped by $p and instantly let go of it.", FALSE, ch, obj, 0, TO_CHAR);
    act("$n is zapped by $p and instantly lets go of it.", FALSE, ch, obj, 0, TO_ROOM);
    mudlog("WARNING: A staff-only item was obtained by a player.", ch, LOG_SYSLOG, TRUE);
    obj_to_room(obj, get_ch_in_room(ch));     /* changed to drop in inventory instead of
                               * ground */  // and now I've changed it back, who wants morts running around with god-only keys
    return FALSE;
  }

  GET_EQ(ch, pos) = obj;
  obj->worn_by = ch;
  obj->worn_on = pos;

  if (ch->in_room) {
    _char_with_light_to_room(ch);
  }

  for (j = 0; j < MAX_OBJ_AFFECT; j++)
    affect_modify(ch,
                  obj->affected[j].location,
                  obj->affected[j].modifier,
                  obj->obj_flags.bitvector, TRUE);

  affect_total(ch);
  calc_weight(ch);
  return TRUE;
}

struct obj_data *unequip_char(struct char_data * ch, int pos, bool focus)
{
  int j;
  struct obj_data *obj;

  if (pos < 0 || pos >= NUM_WEARS) {
    char errbuf[1000];
    snprintf(errbuf, sizeof(errbuf), "SYSERR: pos < 0 || pos >= NUM_WEARS, %s - %d", GET_NAME(ch), pos);
    mudlog(errbuf, ch, LOG_SYSLOG, TRUE);
    return NULL;
  }

  if (!GET_EQ(ch, pos)) {
    char errbuf[1000];
    snprintf(errbuf, sizeof(errbuf), "SYSERR: Trying to remove non-existent item from %s at %d", GET_NAME(ch), pos);
    mudlog(errbuf, ch, LOG_SYSLOG, TRUE);
    return NULL;
  }

  obj = GET_EQ(ch, pos);
  obj->worn_by = NULL;
  obj->worn_on = -1;

  if (ch->in_room)
  {
    _char_with_light_from_room(ch);
  }

  if (pos == WEAR_HOLD || pos == WEAR_WIELD)
  {
    if (FIGHTING(ch))
      AFF_FLAGS(ch).SetBit(AFF_APPROACH);
  }

  GET_EQ(ch, pos) = NULL;

  for (j = 0; j < MAX_OBJ_AFFECT; j++)
    affect_modify(ch,
                  obj->affected[j].location,
                  obj->affected[j].modifier,
                  obj->obj_flags.bitvector, FALSE);

  if (GET_OBJ_TYPE(obj) == ITEM_FOCUS && GET_OBJ_VAL(obj, 4) && focus)
  {
    remove_focus_effect(ch, obj);
    if (GET_OBJ_VAL(obj, 0) == FOCI_SUSTAINED) {
      for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = sust->next)
        if (sust->focus == obj) {
          end_sustained_spell(ch, sust);
          break;
        }
    }
    GET_FOCI(ch)--;
    GET_OBJ_VAL(obj, 4) = 0;
  }
  affect_total(ch);
  calc_weight(ch);
  return (obj);
}

int get_number(char **name, size_t name_len)
{
  int i;
  char *ppos;
  char number[MAX_INPUT_LENGTH];
  char source[MAX_INPUT_LENGTH];

  *number = '\0';
  strlcpy(source, *name, sizeof(source));

  if ((ppos = strchr(source, '.'))) {
    *ppos++ = '\0';
    strlcpy(number, source, sizeof(number));
    strlcpy(*name, ppos, name_len);

    for (i = 0; *(number + i); i++)
      if (!isdigit(*(number + i)))
        return 0;

    return (atoi(number));
  }
  return 1;
}

struct veh_data *get_veh_list(char *name, struct veh_data *list, struct char_data *ch)
{
  struct veh_data *i;
  int j = 0, number;
  bool mine = FALSE;
  char tmpname[MAX_INPUT_LENGTH];
  char *tmp = tmpname;

  if (!list)
    return NULL;
  strlcpy(tmp, get_string_after_color_code_removal(name, ch), sizeof(tmpname));
  if (!strncmp(tmp, "my.", 3)) {
    mine = TRUE;
    strlcpy(tmp, name+3, sizeof(tmpname));
  }
  if (!(number = get_number(&tmp, sizeof(tmpname))))
    return NULL;
  for (i = list; i && (j <= number); i = i->next_veh)
    if (isname(tmp, get_string_after_color_code_removal(GET_VEH_NAME(i), NULL))
        || isname(tmp, get_string_after_color_code_removal(i->name, NULL)))
    {
      if (ch && mine && i->owner == GET_IDNUM(ch))
        return i;
      else if (!mine && ++j == number)
        return i;
    }
  return NULL;
}

/* Search a given list for an object number, and return a ptr to that obj */
struct obj_data *get_obj_in_list_num(int num, struct obj_data * list)
{
  struct obj_data *i;

  for (i = list; i; i = i->next_content)
    if (GET_OBJ_RNUM(i) == num)
      return i;

  return NULL;
}

int vnum_from_non_connected_zone(int vnum)
{
  int counter;
  if (vnum == -1)  // obj made using create_obj, like mail and corpses
    return 0;
  else if (vnum < 0 || vnum > (zone_table[top_of_zone_table].top))
    return 1;

  for (counter = 0; counter <= top_of_zone_table; counter++)
    if (!(zone_table[counter].connected) && vnum >= (zone_table[counter].number * 100) &&
        vnum <= zone_table[counter].top)
      return 1;

  return 0;
}

/* search a room for a char, and return a pointer if found..  */
struct char_data *get_char_room(const char *name, struct room_data *room)
{
  struct char_data *i;
  int j = 0, number;
  char tmpname[MAX_INPUT_LENGTH];
  char *tmp = tmpname;

  if (!room || !name) {
    log("SYSLOG: Illegal value(s) passed get_char_room");
    return NULL;
  }

  strlcpy(tmp, name, sizeof(tmpname));
  if (!(number = get_number(&tmp, sizeof(tmpname))))
    return NULL;

  for (i = room->people; i && (j <= number); i = i->next_in_room)
    if (isname(tmp, get_string_after_color_code_removal(GET_KEYWORDS(i), NULL))
        || isname(tmp, get_string_after_color_code_removal(GET_NAME(i), NULL))
        || isname(tmp, GET_CHAR_NAME(i)))
    {
      if (++j == number)
        return i;
    }

  return NULL;
}

void obj_to_veh(struct obj_data * object, struct veh_data * veh)
{
  struct obj_data *i = NULL, *op = NULL;

  // Check our object-related preconditions. All error logging is done there.
  if (!check_obj_to_x_preconditions(object, NULL))
    return;

  // Precondition: The vehicle in question must exist.
  if (!veh) {
    mudlog("SYSLOG: NULL vehicle passed to obj_to_veh.", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  for (i = veh->contents; i; i = i->next_content) {
    if (i->item_number == object->item_number &&
        !strcmp(i->text.room_desc, object->text.room_desc) &&
        IS_INVIS(i) == IS_INVIS(object))
      break;

    op = i;
  }

  if (i) {
    object->next_content = i;
    if (op)
      op->next_content = object;
    else
      veh->contents = object;
  } else {
    object->next_content = veh->contents;
    veh->contents = object;
  }

  veh->usedload += GET_OBJ_WEIGHT(object);
  object->in_veh = veh;
  object->in_room = NULL;
  object->carried_by = NULL;
}
/* put an object in a room */
void obj_to_room(struct obj_data * object, struct room_data *room)
{
  struct obj_data *i = NULL, *op = NULL;

  // Check our object-related preconditions. All error logging is done there.
  if (!check_obj_to_x_preconditions(object, NULL))
    return;

  // Precondition: The room in question must exist.
  if (!room) {
    mudlog("SYSLOG: NULL room passed to obj_to_room.", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  room->dirty_bit = TRUE;

  for (i = room->contents; i; i = i->next_content) {
    if (i->item_number == object->item_number &&
        !strcmp(i->text.room_desc, object->text.room_desc) &&
        IS_INVIS(i) == IS_INVIS(object))
      break;

    op = i;
  }

  if (op == object) {
    log("SYSLOG: WTF? ^.^");
    return;
  }
  if (i) {
    object->next_content = i;
    if (op)
      op->next_content = object;
    else
      room->contents = object;
  } else {
    object->next_content = room->contents;
    room->contents = object;
  }

  object->in_room = room;
  object->carried_by = NULL;

  // If it's a workshop, make sure the room's workshops[] table reflects this.
  if (GET_OBJ_TYPE(object) == ITEM_WORKSHOP)
    add_workshop_to_room(object);
}


/* Take an object from a room */
void obj_from_room(struct obj_data * object)
{
  struct obj_data *temp;
  if (!object || (!object->in_room && !object->in_veh)) {
    log("SYSLOG: NULL object or obj not in a room passed to obj_from_room");
    return;
  }

  if (object->in_veh) {
    object->in_veh->usedload -= GET_OBJ_WEIGHT(object);
    REMOVE_FROM_LIST(object, object->in_veh->contents, next_content);
  }

  if (object->in_room) {
    // Set the room's dirty bit.
    object->in_room->dirty_bit = TRUE;

    // Handle workshop removal.
    if (GET_OBJ_TYPE(object) == ITEM_WORKSHOP)
      remove_workshop_from_room(object);

    // Strip it out of the room's contents.
    REMOVE_FROM_LIST(object, object->in_room->contents, next_content);
  }

  // Clear its pointers.
  object->in_veh = NULL;
  object->in_room = NULL;
  object->next_content = NULL;
}

/* Put an object in a Matrix host. */
void obj_to_host(struct obj_data *obj, struct host_data *host) {
  if (!host) {
    mudlog("SYSERR: Null host given to obj_to_host!", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // Check our object-related preconditions. All error logging is done there.
  if (!check_obj_to_x_preconditions(obj, NULL))
    return;

  obj->in_host = host;
  obj->next_content = host->file;
  host->file = obj;
}

/* Remove an object from a Matrix host. */
void obj_from_host(struct obj_data *obj) {
  if (!obj) {
    mudlog("SYSERR: Null obj given to obj_from_host!", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  if (!obj->in_host) {
    mudlog("SYSERR: Non-hosted obj given to obj_from_host!", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  struct obj_data *temp;
  REMOVE_FROM_LIST(obj, obj->in_host->file, next_content);
  obj->in_host = NULL;
}

/* put an object in an object (quaint)  */
void obj_to_obj(struct obj_data * obj, struct obj_data * obj_to)
{
  struct obj_data *tmp_obj = NULL;
  struct obj_data *i = NULL, *op = NULL;

  // Check our object-related preconditions. All error logging is done there.
  if (!check_obj_to_x_preconditions(obj, NULL))
    return;

  // Precondition: The object we're putting it in must exist.
  if (!obj_to) {
    mudlog("SYSLOG: NULL obj_to passed to obj_to_obj", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // Precondition: We can't fold it in on itself.
  if (obj == obj_to) {
    mudlog("ERROR: Attempting to put an object inside of itself in obj_to_obj().", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // Scan through the contents for something that matches this item. If we find it, that's where we can nest this item.
  for (i = obj_to->contains; i; i = i->next_content)
  {
    if (i->item_number == obj->item_number &&
        !strcmp(i->text.room_desc, obj->text.room_desc) &&
        IS_INVIS(i) == IS_INVIS(obj))
      break;
    // op points to the last thing we saw-- if a match is found, op is the thing immediately before it. Otherwise, op is the last thing in the list.
    op=i;
  }

  if (i)
  {
    // i points to a similar object to obj.
    obj->next_content = i;

    // with i existing, op must point to the object immediately before i in the list.
    if (op)
      op->next_content = obj;

    // op will be null if the very first object in the list was a match for obj.
    else
      obj_to->contains = obj;
  } else
  {
    // No matches, so stick it at the top of the list.
    obj->next_content = obj_to->contains;
    obj_to->contains = obj;
  }

  obj->in_obj = obj_to;

  // Cascade the weight change all the way up to the second-highest containing object (handles bag-in-bag-in-bag situations).
  for (tmp_obj = obj->in_obj; tmp_obj->in_obj; tmp_obj = tmp_obj->in_obj)
    GET_OBJ_WEIGHT(tmp_obj) += GET_OBJ_WEIGHT(obj);

  // Update the highest container's weight as well, provided it's not a deck or a computer.
  if (GET_OBJ_TYPE(tmp_obj) != ITEM_CYBERDECK || GET_OBJ_TYPE(tmp_obj) != ITEM_CUSTOM_DECK || GET_OBJ_TYPE(tmp_obj) != ITEM_DECK_ACCESSORY) {
    GET_OBJ_WEIGHT(tmp_obj) += GET_OBJ_WEIGHT(obj);

    // If someone's carrying or wearing the highest container, increment their carry weight by the weight of the obj we just put in.
    if (tmp_obj->carried_by)
      IS_CARRYING_W(tmp_obj->carried_by) += GET_OBJ_WEIGHT(obj);
    if (tmp_obj->worn_by)
      IS_CARRYING_W(tmp_obj->worn_by) += GET_OBJ_WEIGHT(obj);
  }

  if (tmp_obj && tmp_obj->in_room)
    tmp_obj->in_room->dirty_bit = TRUE;
}


/* remove an object from an object */
void obj_from_obj(struct obj_data * obj)
{
  struct obj_data *temp, *obj_from;

  if (obj->in_obj == NULL)
  {
    log("error (handler.c): trying to illegally extract obj from obj");
    return;
  }
  obj_from = obj->in_obj;
  REMOVE_FROM_LIST(obj, obj_from->contains, next_content);

  // Unready the holster it's in.
  if (GET_OBJ_TYPE(obj_from) == ITEM_HOLSTER && GET_OBJ_VAL(obj_from, 3))
    GET_OBJ_VAL(obj_from, 3) = 0;

  // Remove weight from whatever's containing this (and its container, and its container...)
  // temp->in_obj as the check is required here, we keep processing after this!
  for (temp = obj->in_obj; temp->in_obj; temp = temp->in_obj)
    GET_OBJ_WEIGHT(temp) -= GET_OBJ_WEIGHT(obj);

  // Decks don't get their weight deducted from.
  if (GET_OBJ_TYPE(temp) != ITEM_CYBERDECK || GET_OBJ_TYPE(temp) != ITEM_DECK_ACCESSORY || GET_OBJ_TYPE(temp) != ITEM_CUSTOM_DECK) {
    GET_OBJ_WEIGHT(temp) -= GET_OBJ_WEIGHT(obj);

    // Recalculate the bearer's weight.
    if (temp->carried_by)
      IS_CARRYING_W(temp->carried_by) -= GET_OBJ_WEIGHT(obj);
    if (temp->worn_by)
      IS_CARRYING_W(temp->worn_by) -= GET_OBJ_WEIGHT(obj);
  }

  obj->in_obj = NULL;
  obj->next_content = NULL;

  if (temp && temp->in_room)
    temp->in_room->dirty_bit = TRUE;
}

void extract_icon(struct matrix_icon * icon)
{
  struct matrix_icon *temp;

  // Clean up phone entries.
  for (struct phone_data *k = phone_list; k; k = k->next) {
    if (k->persona == icon)
    {
      struct phone_data *temp;
      if (k->dest) {
        if (k->dest->persona)
          send_to_icon(k->dest->persona, "The call is terminated from the other end.\r\n");
        else if (k->dest->phone) {
          if (k->dest->connected) {
            if (k->dest->phone->carried_by)
              send_to_char("The phone is hung up from the other side.\r\n", k->dest->phone->carried_by);
          } else {
            if (k->dest->phone->carried_by)
              send_to_char("Your phone stops ringing.\r\n", k->dest->phone->carried_by);
            else if (k->dest->phone->in_obj && k->dest->phone->in_obj->carried_by)
              send_to_char("Your phone stops ringing.\r\n", k->dest->phone->in_obj->carried_by);
            else {
              snprintf(buf, sizeof(buf), "%s stops ringing.\r\n", k->dest->phone->text.name);
              act(buf, FALSE, 0, k->dest->phone, 0, TO_ROOM);
            }
          }
        }
        k->dest->connected = FALSE;
        k->dest->dest = NULL;
      }
      REMOVE_FROM_LIST(k, phone_list, next);
      delete k;
      break;
    }
  }

  // Clean up host data.
  if (icon->in_host) {
    // Wipe out combat info.
    if (icon->fighting) {
      for (struct matrix_icon *vict = matrix[icon->in_host].icons; vict; vict = vict->next_in_host)
        if (vict->fighting == icon) {
          REMOVE_FROM_LIST(vict, matrix[icon->in_host].fighting, next_fighting);
          vict->fighting = NULL;
        }
      REMOVE_FROM_LIST(icon, matrix[icon->in_host].fighting, next_fighting);
    }

    // Extract from host.
    if (icon->in_host != NOWHERE)
      icon_from_host(icon);
  }

  if (icon->decker) {
    if (icon->decker->hitcher) {
      PLR_FLAGS(icon->decker->hitcher).RemoveBit(PLR_MATRIX);
      send_to_char(icon->decker->hitcher, "You return to your senses.\r\n");
      icon->decker->hitcher = NULL;
    }
    struct obj_data *temp;
    for (struct obj_data *obj = icon->decker->software; obj; obj = temp) {
      temp = obj->next_content;
      extract_obj(obj);
    }
    struct seen_data *temp2;
    for (struct seen_data *seen = icon->decker->seen; seen; seen = temp2) {
      temp2 = seen->next;
      delete seen;
    }
    DELETE_AND_NULL(icon->decker);
  } else {
    ic_index[icon->rnum].number--;
  }

  REMOVE_FROM_LIST(icon, icon_list, next);
  Mem->DeleteIcon(icon);
}

void extract_veh(struct veh_data * veh)
{
  if (veh->in_room == NULL && veh->in_veh == NULL) {
    if (veh->carriedvehs || veh->people) {
      strncpy(buf, "SYSERR: extract_veh called on vehicle-with-contents without containing room or veh! The game will likely now shit itself and die; GLHF.", sizeof(buf));
      mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    }
  }

  // Find the online owner of the vehicle for future modifications and notifications.


  if (veh->prev_sub) {
    // If there is a prior entry in the subscriber doubly-linked list, just strip us out.
    veh->prev_sub->next_sub = veh->next_sub;
  } else {
    // Veh is the list head. Look for an online owner– they need their list's head updated to point to our next_sub value.
    for (struct char_data *owner = character_list; owner; owner = owner->next) {
      if (owner->char_specials.subscribe == veh) {
        owner->char_specials.subscribe = veh->next_sub;
        break;
      }
    }
  }

  // If there's a vehicle after us in the list, make sure its prev reflects our prev.
  if (veh->next_sub)
    veh->next_sub->prev_sub = veh->prev_sub;

  // If any vehicles are inside, drop them where the vehicle is.
  struct veh_data *temp = NULL;
  while ((temp = veh->carriedvehs)) {
    snprintf(buf, sizeof(buf), "As %s disintegrates, %s falls out!\r\n", veh->short_description, temp->short_description);
    snprintf(buf2, sizeof(buf2), "You feel the sickening sensation of falling as %s disintegrates around your vehicle.\r\n", veh->short_description);
    send_to_veh(buf2, temp, NULL, FALSE);
    if (veh->in_room) {
      send_to_room(buf, veh->in_room);
      veh_to_room(temp, veh->in_room);
    } else if (veh->in_veh) {
      send_to_veh(buf, veh, NULL, FALSE);
      veh_to_veh(temp, veh->in_veh);
    } else {
      veh_to_room(temp, &world[real_room(RM_DANTES_GARAGE)]);
    }
  }

  // If any players are inside, drop them where the vehicle is.
  struct char_data *ch = NULL;
  while ((ch = veh->people)) {
    send_to_char(ch, "%s disintegrates around you!\r\n", veh->short_description);
    if (veh->in_room) {
      char_from_room(ch);
      char_to_room(ch, veh->in_room);
      snprintf(buf, sizeof(buf), "As %s disintegrates, $n falls out!", veh->short_description);
      act(buf, FALSE, ch, 0, 0, TO_ROOM);
    } else if (veh->in_veh) {
      char_to_veh(veh->in_veh, ch);
    } else {
      char_from_room(ch);
      char_to_room(ch, &world[real_room(RM_DANTES_GARAGE)]);
    }
  }

  // Unhitch its tows.
  if (veh->towing) {
    snprintf(buf, sizeof(buf), "%s falls from %s's towing equipment.\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh->towing)), GET_VEH_NAME(veh));
    if (veh->in_room) {
      act(buf, FALSE, veh->in_room->people, 0, 0, TO_ROOM);
      act(buf, FALSE, veh->in_room->people, 0, 0, TO_CHAR);
      veh_to_room(veh->towing, veh->in_room);
    } else if (veh->in_veh){
      send_to_veh(buf, veh->in_veh, ch, TRUE);
      veh_to_veh(veh->towing, veh->in_veh);
    } else {
      snprintf(buf, sizeof(buf), "SYSERR: Veh %s (%ld) has neither in_room nor in_veh! Dropping towed veh in Dante's Garage.", GET_VEH_NAME(veh), veh->idnum);
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      // Can't stop, we're already blowing up the vehicle. Drop the towed one in Dante's garage.
      veh_to_room(veh->towing, &world[real_room(RM_DANTES_GARAGE)]);
    }
    veh->towing = NULL;
  }

  // Disgorge its contents.
  struct obj_data *nextobj;
  for (struct obj_data *obj = veh->contents; obj; obj = nextobj) {
    nextobj = obj->next_content;
    obj_from_room(obj);
    if (veh->in_room) {
      obj_to_room(obj, veh->in_room);
      snprintf(buf, sizeof(buf), "As %s disintegrates, %s falls out!\r\n", veh->short_description, GET_OBJ_NAME(obj));
      send_to_room(buf, veh->in_room);
    } else if (veh->in_veh) {
      obj_to_veh(obj, veh->in_veh);
      snprintf(buf, sizeof(buf), "As %s disintegrates, %s falls out!\r\n", veh->short_description, GET_OBJ_NAME(obj));
      send_to_veh(buf, veh, NULL, FALSE);
    } else {
      extract_obj(obj);
    }
  }

  // Remove its gridguide info.
  while (veh->grid) {
    struct grid_data *grid = veh->grid;
    veh->grid = veh->grid->next;
    DELETE_ARRAY_IF_EXTANT(grid->name);
    delete grid;
  }

  // Perform actual vehicle extraction.
  REMOVE_FROM_LIST(veh, veh_list, next);
  if (veh->in_room || veh->in_veh)
    veh_from_room(veh);
  veh_index[veh->veh_number].number--;
  Mem->DeleteVehicle(veh);
}

/* Extract an object from the world */
void extract_obj(struct obj_data * obj)
{
  struct phone_data *phone, *temp;
  bool set = FALSE;

  if (IS_OBJ_STAT(obj, ITEM_EXTRA_KEPT)) {
    const char *representation = generate_new_loggable_representation(obj);
    snprintf(buf, sizeof(buf), "extract_obj: Destroying KEPT item: %s", representation);
    delete [] representation;
    mudlog(buf, NULL, LOG_PURGELOG, TRUE);
  }

  if (GET_OBJ_VNUM(obj) == OBJ_VEHCONTAINER && (GET_VEHCONTAINER_VEH_VNUM(obj) || GET_VEHCONTAINER_VEH_IDNUM(obj) || GET_VEHCONTAINER_VEH_OWNER(obj))) {
    const char *owner = get_player_name(GET_VEHCONTAINER_VEH_OWNER(obj));
    snprintf(buf, sizeof(buf), "extract_obj: Destroying NON-ZEROED vehicle container %s (%d, idnum %d) for vehicle belonging to %s (%d).",
             GET_OBJ_NAME(obj),
             GET_VEHCONTAINER_VEH_VNUM(obj),
             GET_VEHCONTAINER_VEH_IDNUM(obj),
             owner,
             GET_VEHCONTAINER_VEH_OWNER(obj)
            );
    DELETE_ARRAY_IF_EXTANT(owner);
    mudlog(buf, NULL, LOG_CHEATLOG, TRUE);
    mudlog(buf, NULL, LOG_PURGELOG, TRUE);
  }

  if (obj->in_room)
    obj->in_room->dirty_bit = TRUE;

  if (obj->worn_by != NULL)
    if (unequip_char(obj->worn_by, obj->worn_on, TRUE) != obj)
      log("SYSLOG: Inconsistent worn_by and worn_on pointers!!");
  if (GET_OBJ_TYPE(obj) == ITEM_PHONE ||
      (GET_OBJ_TYPE(obj) == ITEM_CYBERWARE && GET_OBJ_VAL(obj, 0) == CYB_PHONE))
  {
    for (phone = phone_list; phone; phone = phone->next) {
      if (phone->phone == obj) {
        if (phone->dest) {
          phone->dest->dest = NULL;
          phone->dest->connected = FALSE;
          if (phone->dest->persona)
            send_to_icon(phone->dest->persona, "The connection is closed from the other side.\r\n");
          else {
            if (phone->dest->phone->carried_by) {
              if (phone->dest->connected) {
                if (phone->dest->phone->carried_by)
                  send_to_char("The phone is hung up from the other side.\r\n", phone->dest->phone->carried_by);
              } else {
                if (phone->dest->phone->carried_by)
                  send_to_char("Your phone stops ringing.\r\n", phone->dest->phone->carried_by);
                else if (phone->dest->phone->in_obj && phone->dest->phone->in_obj->carried_by)
                  send_to_char("Your phone stops ringing.\r\n", phone->dest->phone->in_obj->carried_by);
                else {
                  snprintf(buf, sizeof(buf), "%s stops ringing.\r\n", phone->dest->phone->text.name);
                  act(buf, FALSE, 0, phone->dest->phone, 0, TO_ROOM);
                }
              }
            }

          }
        }
        REMOVE_FROM_LIST(phone, phone_list, next);
        delete phone;
        break;
      }
    }
  }

  if (obj->in_room || obj->in_veh != NULL) {
    obj_from_room(obj);
    set = TRUE;
  }

  if (obj->in_host) {
    obj_from_host(obj);
    set = TRUE;
  }

  if (obj->carried_by) {
    obj_from_char(obj);
    if (set)
      log("SYSLOG: More than one list pointer set!");
    set = TRUE;
  }

  if (obj->in_obj) {
    obj_from_obj(obj);
    if (set)
      log("SYSLOG: More than one list pointer set!");
    set = TRUE;
  }

  /* Get rid of the contents of the object, as well. */
  while (obj->contains && GET_OBJ_TYPE(obj) != ITEM_PART)
    extract_obj(obj->contains);

  if (!ObjList.Remove(obj))
    log_vfprintf("ObjList.Remove returned FALSE!  (%d)", GET_OBJ_VNUM(obj));

  if (GET_OBJ_RNUM(obj) >= 0)
    (obj_index[GET_OBJ_RNUM(obj)].number)--;

  Mem->DeleteObject(obj);
}

/* Extract a ch completely from the world, and leave his stuff behind */
void extract_char(struct char_data * ch)
{
  struct char_data *k, *temp;
  struct descriptor_data *t_desc;
  struct obj_data *obj, *next;
  int i;
  struct veh_data *veh;

  extern struct char_data *combat_list;

  void die_follower(struct char_data * ch);

  ACMD_CONST(do_return);

  if (ch->in_room)
    ch->in_room->dirty_bit = TRUE;

  if ((ch->in_veh && AFF_FLAGGED(ch, AFF_PILOT)) || PLR_FLAGGED(ch, PLR_REMOTE)) {
    RIG_VEH(ch, veh);

    send_to_veh("Now driverless, the vehicle slows to a stop.\r\n", veh, ch, FALSE);
    AFF_FLAGS(ch).RemoveBits(AFF_PILOT, AFF_RIG, ENDBIT);
    stop_chase(veh);
    if (!veh->dest)
      veh->cspeed = SPEED_OFF;
  }

  if (GET_WATCH(ch)) {
    // char_data *temp already exists.
    REMOVE_FROM_LIST(ch, GET_WATCH(ch)->watching, next_watching);
    GET_WATCH(ch) = NULL;
  }

  if (!IS_NPC(ch)) {
    // Terminate the player's quest, if any. Realistically, we shouldn't ever trigger this code, but if it happens we're ready for it.
    if (GET_QUEST(ch)) {
      mudlog("Warning: extract_char received PC with quest still active.", ch, LOG_SYSLOG, TRUE);
      end_quest(ch);
    }

    // Save the player.
    playerDB.SaveChar(ch, GET_LOADROOM(ch));

    // Hollow player body? Figure out who this was supposed to belong to and return them.
    if (!ch->desc) {
      for (t_desc = descriptor_list; t_desc; t_desc = t_desc->next)
        if (t_desc->original == ch)
          do_return(t_desc->character, "", 0, 0);
    }
  }

  if (ch->followers)
  {
    struct follow_type *nextfollow;
    for (struct follow_type *follow = ch->followers; follow; follow = nextfollow) {
      nextfollow = follow->next;
      if (IS_SPIRIT(follow->follower) || IS_PC_CONJURED_ELEMENTAL(follow->follower)) {
        act("$n vanishes with a sound like a bursting bubble.", TRUE, follow->follower, 0, 0, TO_ROOM);
        extract_char(follow->follower);
      }
    }
  }
  if (ch->followers || ch->master)
    die_follower(ch);

  /* Forget snooping, if applicable */
  if (ch->desc)
  {
    if (ch->desc->snooping) {
      ch->desc->snooping->snoop_by = NULL;
      ch->desc->snooping = NULL;
    }
    if (ch->desc->snoop_by) {
      SEND_TO_Q("Your victim is no longer among us.\r\n", ch->desc->snoop_by);
      ch->desc->snoop_by->snooping = NULL;
      ch->desc->snoop_by = NULL;
    }
  }

  /* transfer objects to room, if any */
  while (ch->carrying)
  {
    obj = ch->carrying;
    obj_from_char(obj);

    // Zero out veh containers so we don't get nagged about destroyed vehicles every time someone quits.
    if (GET_OBJ_VNUM(obj) == OBJ_VEHCONTAINER)
      GET_VEHCONTAINER_VEH_VNUM(obj) = GET_VEHCONTAINER_VEH_IDNUM(obj) = GET_VEHCONTAINER_VEH_OWNER(obj) = 0;

    // Un-keep items for the same reason.
    if (IS_OBJ_STAT(obj, ITEM_EXTRA_KEPT))
      GET_OBJ_EXTRA(obj).RemoveBit(ITEM_EXTRA_KEPT);

    extract_obj(obj);
  }

  /* extract all cyberware from NPC's since it can't be reused */
  for (obj = ch->cyberware; obj; obj = next)
  {
    next = obj->next_content;
    obj_from_cyberware(obj);
    extract_obj(obj);
  }
  for (obj = ch->bioware; obj; obj = next)
  {
    next = obj->next_content;
    obj_from_bioware(obj);
    extract_obj(obj);
  }

  /* transfer equipment to room, if any */
  for (i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i))
      extract_obj(unequip_char(ch, i, TRUE));

  /* stop this char from fighting anyone else */
  if (CH_IN_COMBAT(ch))
    stop_fighting(ch);

  /* stop everyone else from fighting this char */
  for (k = combat_list; k; k = temp)
  {
    temp = k->next_fighting;
    if (FIGHTING(k) == ch)
      stop_fighting(k);
  }

  /* handle vehicle cases */
  if (ch->in_veh) {
    if (AFF_FLAGGED(ch, AFF_PILOT)) {
      send_to_veh("The vehicle suddenly comes to a stop.\r\n", ch->in_veh, ch, FALSE);
      ch->in_veh->cspeed = SPEED_OFF;
    }
    stop_manning_weapon_mounts(ch, TRUE);
  }

  /* untarget char from vehicles */
  if (ch->in_room)
    for (veh = ch->in_room->vehicles; veh; veh = veh->next_veh)
      for (obj = veh->mount; obj; obj = obj->next_content)
        if (obj->targ == ch)
          obj->targ = NULL;

  /* clear spirit sustained spells */
  {
    struct spirit_sustained *next;
    if (IS_PC_CONJURED_ELEMENTAL(ch) || IS_SPIRIT(ch)) {
      for (struct spirit_sustained *ssust = SPIRIT_SUST(ch); ssust; ssust = next) {
        next = ssust->next;
        if (ssust->caster)
          stop_spirit_power(ch, ssust->type);
        else
          stop_spirit_power(ssust->target, ssust->type);
      }
    } else {
      for (struct spirit_sustained *ssust = SPIRIT_SUST(ch); ssust; ssust = next) {
        next = ssust->next;
        stop_spirit_power(ssust->target, ssust->type);
      }
    }
  }

  /* continue clearing spirit sustained spells */
  if (IS_PC_CONJURED_ELEMENTAL(ch) && GET_SUSTAINED_NUM(ch))
  {
    for (struct descriptor_data *d = descriptor_list; d; d = d->next)
      if (d->character && GET_IDNUM(d->character) == GET_ACTIVE(ch)) {
        for (struct sustain_data *sust = GET_SUSTAINED(d->character); sust; sust = sust->next)
          if (sust->spirit == ch) {
            end_sustained_spell(d->character, sust);
            break;
          }
        break;
      }
  }

  /* clear any sustained spells (but only if they're not a conjured elemental) */
  else if (GET_SUSTAINED(ch))
  {
    struct sustain_data *next;
    for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = next) {
      next = sust->next;
      if (next && sust->idnum == next->idnum) {
        next = next->next;
      }
      end_sustained_spell(ch, sust);
    }

    // Sanity check: They must not have any spells left after this.
    if (GET_SUSTAINED(ch)) {
      mudlog("SYSERR: We still have spells after extract_char spell purge!!", ch, LOG_SYSLOG, TRUE);
    }
  }

  /* wipe out matrix info */
  if (ch->persona)
  {
    snprintf(buf, sizeof(buf), "%s depixelizes and vanishes from the host.\r\n", ch->persona->name);
    send_to_host(ch->persona->in_host, buf, ch->persona, TRUE);
    extract_icon(ch->persona);
    ch->persona = NULL;
    PLR_FLAGS(ch).RemoveBit(PLR_MATRIX);
  } else if (PLR_FLAGGED(ch, PLR_MATRIX) && ch->in_room) {
    for (struct char_data *temp = ch->in_room->people; temp; temp = temp->next_in_room)
      if (PLR_FLAGGED(temp, PLR_MATRIX))
        temp->persona->decker->hitcher = NULL;
  }

  /* end astral tracking */
  if (AFF_FLAGGED(ch, AFF_TRACKED))
    for (struct descriptor_data *d = descriptor_list; d; d = d->next)
      if (d->original && HUNTING(d->original) == ch)
      {
        HUNTING(d->original) = NULL;
        AFF_FLAGS(d->original).RemoveBit(AFF_TRACKING);
        send_to_char("You suddenly lose the trail.\r\n", d->character);
      }

  /* end rigging */
  if (PLR_FLAGGED(ch, PLR_REMOTE))
  {
    ch->char_specials.rigging->rigger = NULL;
    ch->char_specials.rigging->cspeed = SPEED_OFF;
    stop_chase(ch->char_specials.rigging);
    send_to_veh("You slow to a halt.\r\n", ch->char_specials.rigging, NULL, 0);
    ch->char_specials.rigging = NULL;
    PLR_FLAGS(ch).RemoveBit(PLR_REMOTE);
  }

  // Clean up playergroup info.
  if (GET_PGROUP_MEMBER_DATA(ch)) {
    delete GET_PGROUP_MEMBER_DATA(ch);
    GET_PGROUP_MEMBER_DATA(ch) = NULL;
  }

  /* pull the char from the list */
  REMOVE_FROM_LIST(ch, character_list, next);

  /* return people who were possessing or projecting */
  if (ch->desc && ch->desc->original)
    do_return(ch, "", 0, 0);

  /* remove the character from their room (use in_room to reference ch->in_room after this) */
  struct room_data *in_room = ch->in_room;
  if (ch->in_room || ch->in_veh)
    char_from_room(ch);

  /* wipe out their memory struct since PCs can have memory as well now */
  clearMemory(ch);

  if (!IS_NPC(ch))
  {
    /* clean up their flags */
    PLR_FLAGS(ch).RemoveBits(PLR_MATRIX, PLR_PROJECT, PLR_SWITCHED,
                             PLR_WRITING, PLR_MAILING, PLR_EDITING,
                             PLR_SPELL_CREATE, PLR_PROJECT, PLR_CUSTOMIZE,
                             PLR_REMOTE, ENDBIT);

    /* restore them to their room, because corpses love rooms */
    ch->in_room = in_room;

    /* throw them back in the menus */
    if (ch->desc) {
      if (STATE(ch->desc) == CON_QMENU)
        SEND_TO_Q(QMENU, ch->desc);
      else {
        STATE(ch->desc) = CON_MENU;
        SEND_TO_Q(MENU, ch->desc);
      }
    } else {
      Mem->DeleteCh(ch);
    }
  } else
  {
    if (GET_MOB_RNUM(ch) > -1)          /* if mobile */
      mob_index[GET_MOB_RNUM(ch)].number--;
    Mem->DeleteCh(ch);
  }
}

/* ***********************************************************************
 Here follows high-level versions of some earlier routines, ie functions
 which incorporate the actual player-data.
 *********************************************************************** */

struct char_data *get_player_vis(struct char_data * ch, char *name, int inroom)
{
  std::vector<struct char_data *> pcs = {};

  // Compile a list of PCs, checking for exact name matches as we go.
  for (struct char_data *i = character_list; i; i = i->next) {
    if (IS_NPC(i) || (inroom && i->in_room != ch->in_room) || GET_LEVEL(ch) < GET_INCOG_LEV(i))
      continue;

    // Exact name match found-- we're done.
    if (!str_cmp(GET_CHAR_NAME(i), name))
      return i;

    pcs.push_back(i);
  }

  // Next, check for remember matches and partial name matches.
  for (const auto& pc: pcs) {
    if (isname(name, get_string_after_color_code_removal(GET_CHAR_NAME(pc), NULL)) || recog(ch, pc, name))
      return pc;
  }

  // Finally, check for loose keyword matches.
  for (const auto& pc: pcs) {
    if (isname(name, get_string_after_color_code_removal(GET_KEYWORDS(pc), NULL)))
      return pc;
  }

  return NULL;
}

struct char_data *get_char_veh(struct char_data * ch, char *name, struct veh_data * veh)
{
  struct char_data *i;

  for (i = veh->people; i; i = i->next_in_veh)
    if ((isname(name, get_string_after_color_code_removal(GET_KEYWORDS(i), NULL))
         || isname(name, get_string_after_color_code_removal(GET_CHAR_NAME(i), NULL))
         || recog(ch, i, name))
        && CAN_SEE(ch, i))
    {
      return i;
    }

  return NULL;
}

struct char_data *get_char_room_vis(struct char_data * ch, char *name)
{
  struct char_data *i;
  int j = 0, number;
  char tmpname[MAX_INPUT_LENGTH];
  char *tmp = tmpname;

  /* JE 7/18/94 :-) :-) */
  if (!str_cmp(name, "self") || !str_cmp(name, "me") || !str_cmp(name, "myself"))
    return ch;

  /* 0.<name> means PC with name */
  strlcpy(tmp, name, sizeof(tmpname));
  if (!(number = get_number(&tmp, sizeof(tmpname))))
    return get_player_vis(ch, tmp, 1);

  if (ch->in_veh)
    if ((i = get_char_veh(ch, name, ch->in_veh)))
      return i;

  for (i = ch->in_veh ? ch->in_veh->people : ch->in_room->people; i && j <= number; i = ch->in_veh ? i->next_in_veh : i->next_in_room) {
    if ((isname(tmp, get_string_after_color_code_removal(GET_KEYWORDS(i), NULL))
          || isname(tmp, get_string_after_color_code_removal(GET_NAME(i), NULL))
          || recog(ch, i, name))
        && CAN_SEE(ch, i))
    {
      if (++j == number)
        return i;
    }
  }

  return NULL;
}

struct char_data *get_char_in_list_vis(struct char_data * ch, char *name, struct char_data *list)
{
  int j = 0, number;
  char tmpname[MAX_INPUT_LENGTH];
  char *tmp = tmpname;

  if (!str_cmp(name, "self") || !str_cmp(name, "me") || !str_cmp(name, "myself"))
    return ch;

  /* 0.<name> means PC with name */
  strlcpy(tmp, name, sizeof(tmpname));
  if (!(number = get_number(&tmp, sizeof(tmpname))))
    return get_player_vis(ch, tmp, 1);

  for (; list && j <= number; list = list->next_in_room)
    if ((isname(tmp, get_string_after_color_code_removal(GET_KEYWORDS(list), NULL))
         || isname(tmp, get_string_after_color_code_removal(GET_NAME(list), NULL))
         || recog(ch, list, name))
        && CAN_SEE(ch, list))
    {
      if (++j == number)
        return list;
    }

  return NULL;
}

struct char_data *get_char_vis(struct char_data * ch, char *name)
{
  struct char_data *i;
  int j = 0, number;
  char tmpname[MAX_INPUT_LENGTH];
  char *tmp = tmpname;

  // Short circuit: If you're looking for yourself, we don't have to search very hard.
  if (!str_cmp(name, "self") || !str_cmp(name, "me") || !str_cmp(name, "myself"))
    return ch;

  /* check the room first */
  if (ch->in_veh)
    if ((i = get_char_veh(ch, name, ch->in_veh)))
      return i;
  if ((i = get_char_room_vis(ch, name)) != NULL)
    return i;

  strlcpy(tmp, name, sizeof(tmpname));
  if (!(number = get_number(&tmp, sizeof(tmpname))))
    return get_player_vis(ch, tmp, 0);

  for (i = character_list; i && (j <= number); i = i->next)
    if ((isname(tmp, get_string_after_color_code_removal(GET_KEYWORDS(i), NULL))
         || recog(ch, i, name))
        && CAN_SEE(ch, i))
    {
      if (++j == number)
        return i;
    }

  return NULL;
}

struct obj_data *get_obj_in_list_vis(struct char_data * ch, char *name, struct obj_data * list)
{
  struct obj_data *i;
  int j = 0, number;
  char tmpname[MAX_INPUT_LENGTH];
  char *tmp = tmpname;

  // No list, no worries.
  if (!list)
    return NULL;

  strlcpy(tmp, name, sizeof(tmpname));
  if (!(number = get_number(&tmp, sizeof(tmpname))))
    return NULL;

  for (i = list; i && (j <= number); i = i->next_content) {
    if (ch->in_veh && i->in_veh && i->vfront != ch->vfront)
      continue;
    if (isname(tmp, i->text.keywords)
        || isname(tmp, get_string_after_color_code_removal(i->text.name, NULL))
        || (i->restring && isname(tmp, get_string_after_color_code_removal(i->restring, ch))))
      if (++j == number)
        return i;
  }
  return NULL;
}

/* search the entire world for an object, and return a pointer  */
struct obj_data *get_obj_vis(struct char_data * ch, char *name)
{
  struct obj_data *i;
  int number;
  char tmpname[MAX_INPUT_LENGTH];
  char *tmp = tmpname;

  /* scan items carried */
  if ((i = get_obj_in_list_vis(ch, name, ch->carrying)))
    return i;

  /* scan room */
  if (ch->in_room && (i = get_obj_in_list_vis(ch, name, ch->in_room->contents)))
    return i;

  strlcpy(tmp, name, sizeof(tmpname));
  if (!(number = get_number(&tmp, sizeof(tmpname))))
    return NULL;

  //  return find_obj(ch, tmp, number);
  return ObjList.FindObj(ch, tmp, number);
}

struct obj_data *get_object_in_equip_vis(struct char_data * ch,
                                         char *arg, struct obj_data * equipment[], int *j)
{
  char tmpname[MAX_INPUT_LENGTH];
  char *tmp = tmpname;
  int i = 0, number;

  strlcpy(tmp, arg, sizeof(tmpname));
  if (!(number = get_number(&tmp, sizeof(tmpname))))
    return NULL;

  for ((*j) = 0; (*j) < NUM_WEARS && i <= number; (*j)++)
    if (equipment[(*j)])
    {
      if (isname(tmp, equipment[(*j)]->text.keywords)
          || isname(tmp, get_string_after_color_code_removal(equipment[(*j)]->text.name, ch))
          || (equipment[(*j)]->restring && isname(tmp, get_string_after_color_code_removal(equipment[(*j)]->restring, ch))))
      {
        if (++i == number)
          return (equipment[(*j)]);
      }

      if (GET_OBJ_TYPE(equipment[(*j)]) == ITEM_WORN && equipment[(*j)]->contains)
        for (struct obj_data *obj = equipment[(*j)]->contains; obj; obj = obj->next_content)
          if (isname(tmp, obj->text.keywords)
              || isname(tmp, get_string_after_color_code_removal(obj->text.name, ch))
              || (obj->restring && isname(tmp, get_string_after_color_code_removal(obj->restring, ch))))
          {
            if (++i == number)
              return (obj);
          }
    }

  return NULL;
}

int belongs_to(struct char_data *ch, struct obj_data *obj)
{
  if (GET_OBJ_TYPE(obj) == ITEM_MONEY && GET_OBJ_VAL(obj, 1) == 1 &&
      (IS_NPC(ch) ? GET_OBJ_VAL(obj, 3) == 0 : GET_OBJ_VAL(obj, 3) == 1) &&
      GET_OBJ_VAL(obj, 4) == (IS_NPC(ch) ? ch->nr : GET_IDNUM(ch)))
    return 1;
  else
    return 0;
}

struct obj_data *get_first_credstick(struct char_data *ch, const char *arg)
{
  struct obj_data *obj;
  int i;

  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (belongs_to(ch, obj) && isname(arg, obj->text.keywords))
      return obj;

  for (i = 0; i < NUM_WEARS - 1; i++)
    if (GET_EQ(ch, i) && belongs_to(ch, GET_EQ(ch, i)) &&
        isname(arg, GET_EQ(ch, i)->text.keywords))
      return GET_EQ(ch, i);

  return NULL;
}

struct obj_data *create_credstick(struct char_data *ch, int amount)
{
  struct obj_data *obj;
  int num;

  if (amount <= 0)
  {
    log("SYSLOG: Try to create negative or 0 nuyen.");
    return NULL;
  } else if (!IS_NPC(ch))
  {
    log("SYSLOG: Creating a credstick for a PC corpse.");
    return NULL;
  }

  if (amount < 5000)
    num = 100;     // plastic credstick
  else if (amount < 20000)
    num = 101;     // steel credstick
  else if (amount < 40000)
    num = 102;     // silver credstick
  else if (amount < 150000)
    num = 103;     // titanium credstick
  else if (amount < 500000)
    num = 104;     // platinum credstick
  else
    num = 105;  // emerald credstick

  obj = read_object(num, VIRTUAL);

  GET_OBJ_VAL(obj, 0) = amount;
  GET_OBJ_VAL(obj, 3) = 0;
  GET_OBJ_VAL(obj, 4) = ch->nr;
  if (num < 102)
    GET_OBJ_VAL(obj, 5) = (number(1, 9) * 100000) + (number(0, 9) * 10000) +
    (number(0, 9) * 1000) + (number(0, 9) * 100) +
    (number(0, 9) * 10) + number(0, 9);

  return obj;
}

struct obj_data *create_nuyen(int amount)
{
  struct obj_data *obj;

  if (amount <= 0)
  {
    log("SYSLOG: Try to create negative or 0 nuyen.");
    return NULL;
  }
  obj = read_object(110, VIRTUAL);

  GET_OBJ_VAL(obj, 0) = amount;

  return obj;
}

int find_skill_num(char *name)
{
  char *rest_of_name;
  char word_to_evaluate[256];

  for (int index = 1; index < MAX_SKILLS; index++) {
    if (is_abbrev(name, skills[index].name))
      return index;

    // Go through the individual words of the skill's name and try to match them. Ex: "Assault Cannons" -> "assault", "cannons"
    rest_of_name = any_one_arg(skills[index].name, word_to_evaluate);
    while (*word_to_evaluate) {
      if (is_abbrev(name, word_to_evaluate))
        return index;

      rest_of_name = any_one_arg(rest_of_name, word_to_evaluate);
    }
  }

  return -1;
}

int find_spell_num(char *name)
{
  char first[256], first2[256];

  for (int index = 0; index < MAX_SPELLS; index++) {
    // Check for exact matches first.
    if (!str_cmp(name, spells[index].name))
      return index;
  }

  // Check for abbreviations next.
  for (int index = 0; index < MAX_SPELLS; index++) {
    // Check for exact matches first.
    if (is_abbrev(name, spells[index].name))
      return index;
  }

  // Fall back to split abbreviation matching.
  for (int index = 0; index < MAX_SPELLS; index++) {
    int ok = 1;
    const char *temp = any_one_arg_const(spells[index].name, first);
    const char *temp2 = any_one_arg(name, first2);

    while (*first && *first2 && ok) {
      if (!is_abbrev(first2, first))
        ok = 0;
      temp = any_one_arg_const(temp, first);
      temp2 = any_one_arg_const(temp2, first2);
    }

    if (ok && !*first2)
      return index;
  }

  return -1;
}

int find_ability_num(char *name)
{
  int index = 0, ok;
  char *temp, *temp2;
  char first[256], first2[256], powername[256];

  strlcpy(powername, adept_powers[index], sizeof(powername));

  while (++index < ADEPT_NUMPOWER) {
    if (is_abbrev(name, adept_powers[index]))
      return index;

    ok = 1;
    temp = any_one_arg(powername, first);
    temp2 = any_one_arg(name, first2);
    while (*first && *first2 && ok) {
      if (!is_abbrev(first2, first))
        ok = 0;
      temp = any_one_arg(temp, first);
      temp2 = any_one_arg(temp2, first2);
    }

    if (ok && !*first2)
      return index;
  }

  return -1;
}


/* Generic Find, designed to find any object/character                    */
/* Calling :                                                              */
/*  *arg     is the sting containing the string to be searched for.       */
/*           This string doesn't have to be a single word, the routine    */
/*           extracts the next word itself.                               */
/*  bitv..   All those bits that you want to "search through".            */
/*           Bit found will be result of the function                     */
/*  *ch      This is the person that is trying to "find"                  */
/*  **tar_ch Will be NULL if no character was found, otherwise points     */
/* **tar_obj Will be NULL if no object was found, otherwise points        */
/*                                                                        */
/* The routine returns a pointer to the next word in *arg (just like the  */
/* one_argument routine).                                                 */

int generic_find(char *arg, int bitvector, struct char_data * ch,
                 struct char_data ** tar_ch, struct obj_data ** tar_obj)
{
  int i;
  char name[MAX_INPUT + 1];

  *tar_ch = NULL;
  *tar_obj = NULL;
  struct veh_data *rigged_veh = (ch)->char_specials.rigging;

  one_argument(arg, name);

  if (!*name)
    return (0);

  /* Technically, this is intended to find characters outside of vehicles... but this is broken.
     Fixing it lets you do stupid shit like manabolting from inside a car. -- LS */
  if (IS_SET(bitvector, FIND_CHAR_ROOM))
  {      /* Find person in room */
    if (ch->in_veh) {
      if ((*tar_ch = get_char_veh(ch, name, ch->in_veh)))
        return (FIND_CHAR_ROOM);
      else {
        struct room_data *was_in = ch->in_room;
        ch->in_room = ch->in_veh->in_room;
        if ((*tar_ch = get_char_room_vis(ch, name))) {
          ch->in_room = was_in;
          return (FIND_CHAR_ROOM);
        }
        ch->in_room = was_in;
      }
    } else {
      if ((*tar_ch = get_char_room_vis(ch, name)))
        return (FIND_CHAR_ROOM);
    }
  }
  if (IS_SET(bitvector, FIND_CHAR_VEH_ROOM)) {
    struct char_data *i;
    int j = 0, number;
    char tmpname[MAX_INPUT_LENGTH];
    char *tmp = tmpname;

    /* 0.<name> means PC with name-- except here we're overriding that because I cannot be bothered right now. TODO. --LS */
    strlcpy(tmp, name, sizeof(tmpname));
    number = MAX(1, get_number(&tmp, sizeof(tmpname)));

    for (i = get_ch_in_room(ch)->people; i && j <= number; i = i->next_in_room)
      if ((isname(tmp, get_string_after_color_code_removal(GET_KEYWORDS(i), NULL))
           || isname(tmp, get_string_after_color_code_removal(GET_NAME(i), NULL))
           || recog(ch, i, name))
          && CAN_SEE(ch, i))
      {
        if (++j == number) {
          *tar_ch = i;
          return (FIND_CHAR_VEH_ROOM);
        }
      }
  }
  if (IS_SET(bitvector, FIND_CHAR_WORLD))
  {
    if ((*tar_ch = get_char_vis(ch, name))) {
      return (FIND_CHAR_WORLD);
    }
  }
  if (IS_SET(bitvector, FIND_OBJ_EQUIP))
  {
    if ((*tar_obj = get_object_in_equip_vis(ch, name, ch->equipment, &i)))
      return (FIND_OBJ_EQUIP);
  }
  if (IS_SET(bitvector, FIND_OBJ_INV))
  {
    if ((*tar_obj = get_obj_in_list_vis(ch, name, ch->carrying))) {
      return (FIND_OBJ_INV);
    }
  }
  if (IS_SET(bitvector, FIND_OBJ_ROOM))
  {
    if (rigged_veh) {
      if (rigged_veh->in_veh) {
        if ((*tar_obj = get_obj_in_list_vis(ch, name, rigged_veh->in_veh->contents)))
          return (FIND_OBJ_ROOM);
      } else if (rigged_veh->in_room) {
        if ((*tar_obj = get_obj_in_list_vis(ch, name, rigged_veh->in_room->contents))) {
          return (FIND_OBJ_ROOM);
        }
      } else {
        mudlog("SYSERR: Rigged_veh had no in_veh or in_room in generic_find()!", ch, LOG_SYSLOG, TRUE);
      }
    }
    else {
      if (ch->in_veh) {
        if ((*tar_obj = get_obj_in_list_vis(ch, name, ch->in_veh->contents)))
          return (FIND_OBJ_ROOM);
      }
      else if ((*tar_obj = get_obj_in_list_vis(ch, name, ch->in_room->contents))) {
        return (FIND_OBJ_ROOM);
      }
    }
  }
  if (IS_SET(bitvector, FIND_OBJ_VEH_ROOM))
  {
    if (rigged_veh) {
      if ((*tar_obj = get_obj_in_list_vis(ch, name, get_veh_in_room(rigged_veh)->contents))) {
        return (FIND_OBJ_VEH_ROOM);
      }
    }
    else {
      if ((*tar_obj = get_obj_in_list_vis(ch, name, get_ch_in_room(ch)->contents))) {
        return (FIND_OBJ_VEH_ROOM);
      }
    }
  }
  if (IS_SET(bitvector, FIND_OBJ_WORLD))
  {
    if ((*tar_obj = get_obj_vis(ch, name))) {
      return (FIND_OBJ_WORLD);
    }
  }
  return (0);
}

/* a function to scan for "all" or "all.x" */
int find_all_dots(char *arg, size_t arg_size)
{
  if (!strcmp(arg, "all"))
    return FIND_ALL;
  else if (!strncmp(arg, "all.", 4)) {
    strlcpy(buf, arg + 4, sizeof(buf));
    strlcpy(arg, buf, arg_size);
    return FIND_ALLDOT;
  } else
    return FIND_INDIV;
}

#define DEFAULT_TO(skill_name) {if (!dice) {dice = GET_SKILL(ch, (skill_name)); defaulting = TRUE;}}
int veh_skill(struct char_data *ch, struct veh_data *veh, int *tn)
{
  int dice = 0;

  bool defaulting = FALSE;

  switch (veh->type)
  {
    case VEH_CAR:
      dice = GET_SKILL(ch, SKILL_PILOT_CAR);
      DEFAULT_TO(SKILL_PILOT_TRUCK);
      break;
    case VEH_BIKE:
      dice = GET_SKILL(ch, SKILL_PILOT_BIKE);
      break;
    case VEH_TRUCK:
      dice = GET_SKILL(ch, SKILL_PILOT_TRUCK);
      DEFAULT_TO(SKILL_PILOT_CAR);
      break;
    case VEH_FIXEDWING:
      dice = GET_SKILL(ch, SKILL_PILOT_FIXEDWING);
      DEFAULT_TO(SKILL_PILOT_VECTORTHRUST);
      DEFAULT_TO(SKILL_PILOT_ROTORCRAFT);
      DEFAULT_TO(SKILL_PILOT_LTA);
      break;
    case VEH_VECTORTHRUST:
      dice = GET_SKILL(ch, SKILL_PILOT_VECTORTHRUST);
      DEFAULT_TO(SKILL_PILOT_FIXEDWING);
      DEFAULT_TO(SKILL_PILOT_ROTORCRAFT);
      DEFAULT_TO(SKILL_PILOT_LTA);
      break;
    case VEH_ROTORCRAFT:
      dice = GET_SKILL(ch, SKILL_PILOT_ROTORCRAFT);
      DEFAULT_TO(SKILL_PILOT_VECTORTHRUST);
      DEFAULT_TO(SKILL_PILOT_FIXEDWING);
      DEFAULT_TO(SKILL_PILOT_LTA);
      break;
    case VEH_HOVERCRAFT:
      dice = GET_SKILL(ch, SKILL_PILOT_HOVERCRAFT);
      break;
    case VEH_MOTORBOAT:
      dice = GET_SKILL(ch, SKILL_PILOT_MOTORBOAT);
      DEFAULT_TO(SKILL_PILOT_SHIP);
      break;
    case VEH_SHIP:
      dice = GET_SKILL(ch, SKILL_PILOT_SHIP);
      DEFAULT_TO(SKILL_PILOT_MOTORBOAT);
      break;
    case VEH_LTA:
      dice = GET_SKILL(ch, SKILL_PILOT_LTA);
      DEFAULT_TO(SKILL_PILOT_VECTORTHRUST);
      DEFAULT_TO(SKILL_PILOT_FIXEDWING);
      DEFAULT_TO(SKILL_PILOT_ROTORCRAFT);
      break;
  }

  // Assume any NPC with a vehicle and no skill dice has a minimum dice of 4 and that the builder just forgot to set it.
  if (IS_NPC(ch) && dice == 0) {
    dice = 4;
    defaulting = FALSE;
  }

  // Skill-to-skill default.
  if (defaulting && dice > 0) {
    *tn += 2;
  }

  // Skill-to-attribute default.
  else if (dice <= 0) {
    dice = GET_REA(ch);

    bool has_vcr = FALSE;
    for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content) {
      if (GET_CYBERWARE_TYPE(cyber) == CYB_VCR) {
        has_vcr = TRUE;
        break;
      }
    }

    if (has_vcr) {
      *tn += 2;
    } else {
      *tn += 4;
    }
  }

  if (AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE))
    dice += GET_CONTROL(ch);

  return dice;
}
#undef DEFAULT_TO

int get_skill_num_in_use_for_weapons(struct char_data *ch) {
  struct obj_data *one, *two;
  int skill_num;

  if (AFF_FLAGGED(ch, AFF_MANNING) || AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE)) {
    skill_num = SKILL_GUNNERY;
  } else {
    one = (GET_EQ(ch, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_WEAPON) ? GET_EQ(ch, WEAR_WIELD) :
           (struct obj_data *) NULL;
    two = (GET_EQ(ch, WEAR_HOLD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) == ITEM_WEAPON) ? GET_EQ(ch, WEAR_HOLD) :
           (struct obj_data *) NULL;

    if (!one && !two) {
      if(has_cyberweapon(ch))
        skill_num = SKILL_CYBER_IMPLANTS;
      else
        skill_num = SKILL_UNARMED_COMBAT;
    }

    else if (one) {
      if (!GET_SKILL(ch, GET_OBJ_VAL(one, 4)))
        skill_num = return_general(GET_OBJ_VAL(one, 4));
      else
        skill_num = GET_OBJ_VAL(one, 4);
    }

    else if (two) {
      if (!GET_SKILL(ch, GET_OBJ_VAL(two, 4)))
        skill_num = return_general(GET_OBJ_VAL(two, 4));
      else
        skill_num = GET_OBJ_VAL(two, 4);
    }

    // This broken-ass code never worked. "If neither one or two, or if one, or if two, or..." no, that's a full logical stop.
    else {
      if (GET_SKILL(ch, GET_OBJ_VAL(one, 4)) <= GET_SKILL(ch, GET_OBJ_VAL(two, 4))) {
        if (!GET_SKILL(ch, GET_OBJ_VAL(one, 4)))
          skill_num = return_general(GET_OBJ_VAL(one, 4));
        else
          skill_num = GET_OBJ_VAL(one, 4);
      } else {
        if (!GET_SKILL(ch, GET_OBJ_VAL(two, 4)))
          skill_num = return_general(GET_OBJ_VAL(two, 4));
        else
          skill_num = GET_OBJ_VAL(two, 4);
      }
    }
  }

  return skill_num;
}

int _get_weapon_focus_bonus_dice(struct char_data *ch, struct obj_data *weapon) {
  if (!ch || !weapon)
    return 0;

  if (weapon && GET_OBJ_TYPE(weapon) == ITEM_WEAPON && WEAPON_IS_FOCUS(weapon) && WEAPON_FOCUS_USABLE_BY(weapon, ch)) {
    return GET_WEAPON_FOCUS_RATING(GET_EQ(ch, WEAR_WIELD));
  }

  return 0;
}

int get_skill_dice_in_use_for_weapons(struct char_data *ch) {
  int skill_num = get_skill_num_in_use_for_weapons(ch);
  int skill_dice = GET_SKILL(ch, skill_num);

  skill_dice += _get_weapon_focus_bonus_dice(ch, GET_EQ(ch, WEAR_WIELD));
  skill_dice += _get_weapon_focus_bonus_dice(ch, GET_EQ(ch, WEAR_HOLD));

  return skill_dice;
}

void _char_with_spell_to_room(struct char_data *ch, int spell_num, room_spell_t *room_spell_tracker) {
  if (affected_by_spell(ch, spell_num)) {
    room_spell_tracker[ROOM_NUM_SPELLS_OF_TYPE] = MIN(room_spell_tracker[ROOM_NUM_SPELLS_OF_TYPE] + 1, 1);
    for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = sust->next) {
      if (sust->spell == spell_num) {
        int force = MIN(sust->force, sust->success);
        room_spell_tracker[ROOM_HIGHEST_SPELL_FORCE] = MAX(room_spell_tracker[ROOM_HIGHEST_SPELL_FORCE], force);
      }
    }
  }
}

void _char_with_spell_from_room(struct char_data *ch, int spell_num, room_spell_t *room_spell_tracker) {
  if (affected_by_spell(ch, spell_num)) {
    // Clear the room's spell value.
    room_spell_tracker[ROOM_NUM_SPELLS_OF_TYPE] = MAX(room_spell_tracker[ROOM_NUM_SPELLS_OF_TYPE] - 1, 0);
    room_spell_tracker[ROOM_HIGHEST_SPELL_FORCE] = 0;

    // If we still have a spell, search everyone here to re-establish it.
    if (room_spell_tracker[ROOM_NUM_SPELLS_OF_TYPE] > 0) {
      for (struct char_data *tmp_ch = ch->in_room->people; tmp_ch; tmp_ch = tmp_ch->next_in_room) {
        if (tmp_ch == ch)
          continue;

        for (struct sustain_data *sust = GET_SUSTAINED(tmp_ch); sust; sust = sust->next) {
          if (sust->spell == spell_num && sust->caster) {
            int force = MIN(sust->force, sust->success);
            room_spell_tracker[ROOM_HIGHEST_SPELL_FORCE] = MAX(room_spell_tracker[ROOM_HIGHEST_SPELL_FORCE], force);
          }
        }
      }
    }
  }
}

void _handle_char_with_light(struct char_data *ch, bool add) {
  if (!ch->in_room) {
    mudlog("SYSERR: Got NULL ch->in_room to _handle_char_with_light()!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  struct obj_data *light = GET_EQ(ch, WEAR_LIGHT);
  if (light && GET_OBJ_TYPE(light) == ITEM_LIGHT) {
    if (add) {
      ch->in_room->light[ROOM_LIGHT_HEADLIGHTS_AND_FLASHLIGHTS]++;
    } else {
      ch->in_room->light[ROOM_LIGHT_HEADLIGHTS_AND_FLASHLIGHTS]--;
    }
  }
}

void _char_with_light_to_room(struct char_data *ch) {
  _handle_char_with_light(ch, TRUE);
}

void _char_with_light_from_room(struct char_data *ch) {
  _handle_char_with_light(ch, FALSE);
}
