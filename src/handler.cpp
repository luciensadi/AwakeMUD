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

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "newdb.h"
#include "handler.h"
#include "interpreter.h"
#include "memory.h"
#include "dblist.h"
#include "constants.h"
#include "newmatrix.h"
#include "newmagic.h"
#include "playergroups.h"
#include "config.h"

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

struct obj_data *find_obj(struct char_data *ch, char *name, int num);

char *fname(char *namelist)
{
  static char holder[50];
  char *point;
  
  if (!namelist || !*namelist) {
    mudlog("SYSERR: fname received null namelist!", NULL, LOG_SYSLOG, TRUE);
    strcpy(holder, "error-report-this-to-staff");
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
  if (GET_OBJ_VAL(object, 2) != GET_IDNUM(ch) || !GET_OBJ_VAL(object, 4))
    return;
  if (GET_OBJ_VAL(object, 9) == GET_IDNUM(ch))
    GET_MAG(ch) += 100;
  if (GET_OBJ_VAL(object, 0) == FOCI_POWER)
  {
    GET_MAG(ch) += GET_OBJ_VAL(object, 1) * 100;
    GET_MAGIC(ch) += GET_OBJ_VAL(object, 1);
  }
}

void remove_focus_effect( struct char_data *ch, struct obj_data *object )
{
  if (GET_OBJ_VAL(object, 2) != GET_IDNUM(ch) || !GET_OBJ_VAL(object, 4))
    return;
  if (GET_OBJ_VAL(object, 9) == GET_IDNUM(ch))
    GET_MAG(ch) -= 100;
  if (GET_OBJ_VAL(object, 0) == FOCI_POWER)
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
      veh->seating[1] += mod;
      break;
    case VAFF_SEAB:
      veh->seating[0] += mod;
      break;
    case VAFF_LOAD:
      veh->load += mod;
      break;
    case VAFF_PILOT:
      veh->pilot = mod;
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
      mod *= sust->success;
      GET_ATT(ch, sust->subtype) += MIN(sust->force, mod / 2);
      break;
    case SPELL_DECATTR:
    case SPELL_DECCYATTR:
      mod *= sust->success;
      GET_ATT(ch, sust->subtype) -= MIN(sust->force, mod / 2);
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
          ch->in_room->poltergeist[0]++;
          if (sust->force > ch->in_room->poltergeist[1])
            ch->in_room->poltergeist[1] = sust->force;
        } else if (!--ch->in_room->poltergeist[0])
          ch->in_room->poltergeist[1] = 0;
      }
      break;
    case SPELL_LIGHT:
      if (ch->in_room) {
        if (mod == 1) {
          ch->in_room->light[0]++;
          if (sust->force > ch->in_room->light[1])
            ch->in_room->light[1] = MIN(sust->force, sust->success);
        } else if (!--ch->in_room->light[0])
          ch->in_room->light[1] = 0;
      }
      break;
    case SPELL_SHADOW:
      if (ch->in_room) {
        if (mod == 1) {
          ch->in_room->shadow[0]++;
          if (sust->force > ch->in_room->shadow[1])
            ch->in_room->shadow[1] = MIN(sust->force, sust->success);
        } else if (!--ch->in_room->shadow[0])
          ch->in_room->shadow[1] = 0;
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
          ch->in_room->silence[0]++;
          if (sust->force > ch->in_room->silence[1])
            ch->in_room->silence[1] = MIN(sust->force, sust->success);
        } else if (!--ch->in_room->silence[0])
          ch->in_room->silence[1] = 0;
      }
      break;
    case SPELL_COMBATSENSE:
      mod *= MIN(sust->force, sust->success / 2);
      GET_COMBAT(ch) += mod;
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
  struct obj_data *cyber, *one, *two, *obj;
  struct sustain_data *sust;
  sh_int i, j, skill;
  int has_rig = 0, has_trigger = -1, has_wired = 0, has_mbw = 0;
  bool wearing = FALSE;
  
  if (IS_PROJECT(ch))
    return;
  
  /* effects of used equipment */
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
  
  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (GET_OBJ_TYPE(obj) == ITEM_FOCUS)
      remove_focus_effect(ch, obj);
  
  /* effects of cyberware */
  for (cyber = ch->cyberware; cyber; cyber = cyber->next_content)
  {
    for (j = 0; j < MAX_OBJ_AFFECT; j++)
      affect_modify(ch,
                    cyber->affected[j].location,
                    cyber->affected[j].modifier,
                    cyber->obj_flags.bitvector, FALSE);
  }
  
  /* effects of bioware */
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
  
  AFF_FLAGS(ch).RemoveBit(AFF_INVISIBLE);
  for (sust = GET_SUSTAINED(ch); sust; sust = sust->next)
    if (!sust->caster)
      spell_modify(ch, sust, FALSE);
      
  int old_max_hacking = GET_MAX_HACKING(ch);
  int old_rem_hacking = GET_REM_HACKING(ch);
  ch->aff_abils = ch->real_abils;
  
  /* calculate reaction before you add eq, cyberware, etc so that things *
   * such as wired reflexes work properly (as they only modify reaction  *
   * and not intelligence and quickness).            -cjd                */
  GET_REAL_REA(ch) = (GET_REAL_INT(ch) + GET_REAL_QUI(ch)) >> 1;
  GET_REA(ch) = 0;
  if (PLR_FLAGGED(ch, PLR_NEWBIE) && GET_TKE(ch) > NEWBIE_KARMA_THRESHOLD)
    PLR_FLAGS(ch).RemoveBit(PLR_NEWBIE);
  
  /* set the dice pools before equip so that they can be affected */
  /* combat pool is equal to quickness, wil, and int divided by 2 */
  GET_COMBAT(ch) = 0;
  GET_HACKING(ch) = 0;
  GET_ASTRAL(ch) = 0;
  GET_MAGIC(ch) = 0;
  GET_CONTROL(ch) = 0;
  if ((GET_RACE(ch) == RACE_TROLL || GET_RACE(ch) == RACE_CYCLOPS || GET_RACE(ch) == RACE_FOMORI ||
       GET_RACE(ch) == RACE_GIANT || GET_RACE(ch) == RACE_MINOTAUR) && GET_HEIGHT(ch) > 260)
    GET_REACH(ch) = 1;
  else GET_REACH(ch) = 0;
  // reset initiative dice
  GET_INIT_DICE(ch) = 0;
  /* reset # of foci char has */
  
  if (IS_SPIRIT(ch) || IS_ELEMENTAL(ch))
    GET_IMPACT(ch) = GET_BALLISTIC(ch) = GET_SPARE1(ch) * 2;
  else if (IS_NPC(ch))
  {
    GET_BALLISTIC(ch) = mob_proto[GET_MOB_RNUM(ch)].points.ballistic[0];
    GET_IMPACT(ch) = mob_proto[GET_MOB_RNUM(ch)].points.impact[0];
  } else
    GET_BALLISTIC(ch) = GET_IMPACT(ch) = 0;
  
  GET_TOTALBAL(ch) = GET_TOTALIMP(ch) = 0;
  
  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (GET_OBJ_TYPE(obj) == ITEM_FOCUS)
      apply_focus_effect(ch, obj);
  /* effects of equipment */
  {
    struct obj_data *worn_item = NULL;
    int suitbal = 0, suitimp = 0, suittype = 0,  highestbal = 0, highestimp = 0;
    for (i = 0; i < (NUM_WEARS - 1); i++) {
      if ((worn_item = GET_EQ(ch, i))) {
        wearing = TRUE;
        if (GET_OBJ_TYPE(worn_item) == ITEM_FOCUS)
          apply_focus_effect(ch, worn_item);
        else
          for (j = 0; j < MAX_OBJ_AFFECT; j++)
            affect_modify(ch,
                          worn_item->affected[j].location,
                          worn_item->affected[j].modifier,
                          worn_item->obj_flags.bitvector, TRUE);
        if (GET_OBJ_TYPE(worn_item) == ITEM_WORN) {
          if (GET_WORN_MATCHED_SET(worn_item) && (!suittype || suittype == GET_WORN_MATCHED_SET(worn_item))) {
            suitbal += GET_WORN_BALLISTIC(worn_item);
            suitimp += GET_WORN_IMPACT(worn_item);
            suittype = GET_WORN_MATCHED_SET(worn_item);
          } else if (GET_WORN_BALLISTIC(worn_item) || GET_WORN_IMPACT(worn_item)) {
            int bal = 0, imp = 0;
            if (GET_WORN_MATCHED_SET(worn_item)) {
              bal = (int)(GET_WORN_BALLISTIC(worn_item) / 100);
              imp = (int)(GET_WORN_IMPACT(worn_item) / 100);
            } else {
              bal = GET_WORN_BALLISTIC(worn_item);
              imp = GET_WORN_IMPACT(worn_item);
            }
            struct obj_data *temp_item = NULL;
            for (j = 0; j < (NUM_WEARS - 1); j++) {
              if ((temp_item = GET_EQ(ch, j)) && j != i && GET_OBJ_TYPE(temp_item) == ITEM_WORN) {
                if ((highestbal || GET_WORN_BALLISTIC(worn_item) <
                     (GET_WORN_MATCHED_SET(temp_item) ? GET_WORN_BALLISTIC(temp_item) / 100 : GET_WORN_BALLISTIC(temp_item))) &&
                    bal == GET_WORN_BALLISTIC(worn_item))
                  bal /= 2;
                if ((highestimp || GET_WORN_IMPACT(worn_item) <
                     (GET_WORN_MATCHED_SET(temp_item) ? GET_WORN_IMPACT(temp_item) / 100 : GET_WORN_IMPACT(temp_item))) &&
                    imp == GET_WORN_IMPACT(worn_item))
                  imp /= 2;
              }
            }
            if (bal == GET_WORN_BALLISTIC(worn_item))
              highestbal = bal;
            if (imp == GET_WORN_IMPACT(worn_item))
              highestimp = imp;
            GET_IMPACT(ch) += imp;
            GET_BALLISTIC(ch) += bal;
            if (!IS_OBJ_STAT(worn_item, ITEM_FORMFIT)) {
              GET_TOTALIMP(ch) += GET_WORN_BALLISTIC(worn_item) / (GET_WORN_MATCHED_SET(worn_item) ? 100 : 1);
              GET_TOTALBAL(ch) += GET_WORN_IMPACT(worn_item) / (GET_WORN_MATCHED_SET(worn_item) ? 100 : 1);
            }
          }
        }
      }
    }
    if (suitbal || suitimp) {
      suitimp /= 100;
      suitbal /= 100;
      if (GET_IMPACT(ch) == 0)
        GET_IMPACT(ch) += suitimp;
      else {
        if (suitimp >= highestimp) {
          GET_IMPACT(ch) -= highestimp;
          GET_IMPACT(ch) += highestimp / 2;
          GET_IMPACT(ch) += suitimp;
        } else GET_IMPACT(ch) += suitimp / 2;
        GET_TOTALIMP(ch) += suitimp;
      }
      if (GET_BALLISTIC(ch) == 0)
        GET_BALLISTIC(ch) += suitbal;
      else {
        if (suitbal >= highestbal) {
          GET_BALLISTIC(ch) -= highestbal;
          GET_BALLISTIC(ch) += highestbal / 2;
          GET_BALLISTIC(ch) += suitbal;
        } else GET_BALLISTIC(ch) += suitbal / 2;
        GET_TOTALBAL(ch) += suitbal;
      }
      
    }
  }
  if (GET_RACE(ch) == RACE_TROLL || GET_RACE(ch) == RACE_MINOTAUR)
    GET_IMPACT(ch)++;
  
  /* effects of cyberware */
  for (cyber = ch->cyberware; cyber; cyber = cyber->next_content)
  {
    if (GET_OBJ_VAL(cyber, 0) == CYB_VCR)
      has_rig = GET_OBJ_VAL(cyber, 1);
    else if (GET_OBJ_VAL(cyber, 0) == CYB_REFLEXTRIGGER)
      has_trigger = GET_OBJ_VAL(cyber, 1);
    else if (GET_OBJ_VAL(cyber, 0) == CYB_WIREDREFLEXES)
      has_wired = GET_OBJ_VAL(cyber, 1);
    else if (GET_OBJ_VAL(cyber, 0) == CYB_MOVEBYWIRE)
      has_mbw = GET_OBJ_VAL(cyber, 1);
    else if (GET_OBJ_VAL(cyber, 0) == CYB_DERMALSHEATHING || GET_OBJ_VAL(cyber, 0) == CYB_DERMALPLATING) {
      if (GET_OBJ_VAL(cyber, 0) == CYB_DERMALSHEATHING && GET_OBJ_VAL(cyber, 3) == 1 && !wearing)
        AFF_FLAGS(ch).SetBit(AFF_INVISIBLE);
      // todo
    }
    for (j = 0; j < MAX_OBJ_AFFECT; j++)
      affect_modify(ch,
                    cyber->affected[j].location,
                    cyber->affected[j].modifier,
                    cyber->obj_flags.bitvector, TRUE);
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
    GET_QUI(ch) = MAX(0, GET_QUI(ch) - (GET_TEMP_QUI_LOSS(ch) / 4));
  i = ((IS_NPC(ch) || (GET_LEVEL(ch) >= LVL_ADMIN)) ? 50 : 20);
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
    if (GET_POWER(ch, ADEPT_LOW_LIGHT))
      NATURAL_VISION(ch) = LOWLIGHT;
    if (GET_POWER(ch, ADEPT_THERMO))
      NATURAL_VISION(ch) = THERMOGRAPHIC;
    if (GET_POWER(ch, ADEPT_IMAGE_MAG))
      AFF_FLAGS(ch).SetBit(AFF_VISION_MAG_2);
  }
  if (!AFF_FLAGGED(ch, AFF_DETOX)) {
    if (GET_DRUG_STAGE(ch) == 1)
    {
      switch (GET_DRUG_AFFECT(ch)) {
        case DRUG_HYPER:
          GET_TARGET_MOD(ch)++;
          break;
        case DRUG_JAZZ:
          GET_QUI(ch) += 2;
          GET_INIT_DICE(ch)++;
          break;
        case DRUG_KAMIKAZE:
          GET_BOD(ch)++;
          GET_QUI(ch)++;
          GET_STR(ch) += 2;
          GET_WIL(ch)++;
          GET_INIT_DICE(ch)++;
          break;
        case DRUG_PSYCHE:
          GET_INT(ch)++;
          break;
        case DRUG_BLISS:
          GET_TARGET_MOD(ch)++;
          GET_REA(ch)--;
          break;
        case DRUG_CRAM:
          GET_REA(ch)++;
          GET_INIT_DICE(ch)++;
          break;
        case DRUG_NITRO:
          GET_STR(ch) += 2;
          GET_WIL(ch) += 2;
          break;
        case DRUG_NOVACOKE:
          GET_REA(ch)++;
          GET_CHA(ch)++;
          break;
        case DRUG_ZEN:
          GET_REA(ch) -= 2;
          GET_TARGET_MOD(ch)++;
          GET_WIL(ch)++;
          break;
      }
    } else if (GET_DRUG_STAGE(ch) == 2)
    {
      switch (GET_DRUG_AFFECT(ch)) {
        case DRUG_JAZZ:
          GET_TARGET_MOD(ch)++;
          GET_QUI(ch)--;
          break;
        case DRUG_KAMIKAZE:
          GET_QUI(ch)--;
          GET_WIL(ch)--;
          break;
        case DRUG_NOVACOKE:
          GET_CHA(ch) = 1;
          GET_WIL(ch) /= 2;
          break;
      }
    }
  }
  if (AFF_FLAGGED(ch, AFF_WITHDRAWAL_FORCE))
  {
    GET_MAX_MENTAL(ch) -= 300;
    GET_TARGET_MOD(ch) += 6;
  } else if (AFF_FLAGGED(ch, AFF_WITHDRAWAL))
  {
    GET_TARGET_MOD(ch) += 4;
  }
  
  if (AFF_FLAGGED(ch, AFF_MANNING) || AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE)) {
    skill = GET_SKILL(ch, SKILL_GUNNERY);
  } else {
    one = (GET_EQ(ch, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_WEAPON) ? GET_EQ(ch, WEAR_WIELD) :
           (struct obj_data *) NULL;
    two = (GET_EQ(ch, WEAR_HOLD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) == ITEM_WEAPON) ? GET_EQ(ch, WEAR_HOLD) :
           (struct obj_data *) NULL;
           
    if (!one && !two) {
      if(has_cyberweapon(ch))
        skill = GET_SKILL(ch, SKILL_CYBER_IMPLANTS);
      else 
        skill = GET_SKILL(ch, SKILL_UNARMED_COMBAT);
    } 
    
    else if (one) {
      if (!GET_SKILL(ch, GET_OBJ_VAL(one, 4)))
        skill = GET_SKILL(ch, return_general(GET_OBJ_VAL(one, 4)));
      else 
        skill = GET_SKILL(ch, GET_OBJ_VAL(one, 4));
    } 
    
    else if (two) {
      if (!GET_SKILL(ch, GET_OBJ_VAL(two, 4)))
        skill = GET_SKILL(ch, return_general(GET_OBJ_VAL(two, 4)));
      else 
        skill = GET_SKILL(ch, GET_OBJ_VAL(two, 4));
    } 
    
    // This broken-ass code never worked. "If neither one or two, or if one, or if two, or..." no, that's a full logical stop.
    else {
      if (GET_SKILL(ch, GET_OBJ_VAL(one, 4)) <= GET_SKILL(ch, GET_OBJ_VAL(two, 4))) {
        if (!GET_SKILL(ch, GET_OBJ_VAL(one, 4)))
          skill = GET_SKILL(ch, return_general(GET_OBJ_VAL(one, 4)));
        else 
          skill = GET_SKILL(ch, GET_OBJ_VAL(one, 4));
      } else {
        if (!GET_SKILL(ch, GET_OBJ_VAL(two, 4)))
          skill = GET_SKILL(ch, return_general(GET_OBJ_VAL(two, 4)));
        else 
          skill = GET_SKILL(ch, GET_OBJ_VAL(two, 4));
      }
    }
  }
  
  GET_COMBAT(ch) += (GET_QUI(ch) + GET_WIL(ch) + GET_INT(ch)) / 2;
  if (GET_TOTALBAL(ch) > GET_QUI(ch))
    GET_COMBAT(ch) -= (GET_TOTALBAL(ch) - GET_QUI(ch)) / 2;
  if (GET_TOTALIMP(ch) > GET_QUI(ch))
    GET_COMBAT(ch) -= (GET_TOTALIMP(ch) - GET_QUI(ch)) / 2;
  if (GET_COMBAT(ch) < 0)
    GET_COMBAT(ch) = 0;
  if (GET_TRADITION(ch) == TRAD_ADEPT)
    GET_IMPACT(ch) += GET_POWER(ch, ADEPT_MYSTIC_ARMOR);
  for (i = 0; i < (NUM_WEARS -1); i++)
    if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_GYRO)
    {
      GET_COMBAT(ch) /= 2;
      break;
    }
  GET_DEFENSE(ch) = MIN(GET_DEFENSE(ch), GET_COMBAT(ch));
  GET_BODY(ch) = MIN(GET_BODY(ch), GET_COMBAT(ch) - GET_DEFENSE(ch));
  GET_OFFENSE(ch) = GET_COMBAT(ch) - GET_DEFENSE(ch) - GET_BODY(ch);
  if (GET_OFFENSE(ch) > skill)
  {
    GET_DEFENSE(ch) += GET_OFFENSE(ch) - skill;
    GET_OFFENSE(ch) = skill;
  }
  if ((IS_NPC(ch) && GET_MAG(ch) > 0) || (GET_TRADITION(ch) == TRAD_SHAMANIC ||
                                          GET_TRADITION(ch) == TRAD_HERMETIC))
  {
    GET_ASTRAL(ch) += GET_GRADE(ch);
    GET_MAGIC(ch) += (GET_INT(ch) + GET_WIL(ch) + (int)(GET_MAG(ch) / 100))/3;
    if (IS_NPC(ch)) {
      GET_DRAIN(ch) = GET_SDEFENSE(ch) = GET_MAGIC(ch) / 3;
    } else {
      GET_SDEFENSE(ch) = MIN(GET_MAGIC(ch), GET_SDEFENSE(ch));
      GET_DRAIN(ch) = MIN(GET_MAGIC(ch), GET_DRAIN(ch));
      GET_REFLECT(ch) = MIN(GET_MAGIC(ch), GET_REFLECT(ch));
    }
    GET_CASTING(ch) = MAX(0, GET_MAGIC(ch) - GET_DRAIN(ch) - GET_REFLECT(ch) - GET_SDEFENSE(ch));
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
  if (AFF_FLAGGED(ch, AFF_INFRAVISION))
    CURRENT_VISION(ch) = THERMOGRAPHIC;
  else if (AFF_FLAGGED(ch, AFF_LOW_LIGHT))
    CURRENT_VISION(ch) = LOWLIGHT;
  else
    CURRENT_VISION(ch) = NATURAL_VISION(ch);
  if (AFF_FLAGGED(ch, AFF_INVISIBLE) || AFF_FLAGGED(ch, AFF_IMP_INVIS))
  {
    if (GET_EQ(ch, WEAR_ABOUT)) {
      if (!(GET_OBJ_AFFECT(GET_EQ(ch, WEAR_ABOUT)).IsSet(AFF_INVISIBLE) || GET_OBJ_AFFECT(GET_EQ(ch, WEAR_ABOUT)).IsSet(AFF_IMP_INVIS)))
        AFF_FLAGS(ch).RemoveBits(AFF_INVISIBLE, AFF_IMP_INVIS, ENDBIT);
      else
        return;
    }
    if (GET_EQ(ch, WEAR_BODY) &&
        (!(GET_OBJ_AFFECT(GET_EQ(ch, WEAR_BODY)).IsSet(AFF_INVISIBLE) || GET_OBJ_AFFECT(GET_EQ(ch, WEAR_BODY)).IsSet(AFF_IMP_INVIS))))
      AFF_FLAGS(ch).RemoveBits(AFF_INVISIBLE, AFF_IMP_INVIS, ENDBIT);
  }
  if (GET_EQ(ch, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_WEAPON) {
    if (!IS_GUN(GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3)) && GET_WEAPON_REACH(GET_EQ(ch, WEAR_WIELD)) > 0)
      GET_REACH(ch) += GET_WEAPON_REACH(GET_EQ(ch, WEAR_WIELD));
    else if (IS_GUN(GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3))) {
      if (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 4) != SKILL_PISTOLS)
        GET_REACH(ch)++;
      struct obj_data *attach = NULL;
      if (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 9) &&
          real_object(GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 9)) > 0 &&
          (attach = &obj_proto[real_object(GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 9))]) &&
          GET_OBJ_VAL(attach, 1) == ACCESS_BAYONET)
        GET_REACH(ch)++;
    }
  }
}

/*
 * Return if a char is affected by a spell (SPELL_XXX), NULL indicates
 * not affected
 */
bool affected_by_spell(struct char_data * ch, int type)
{
  if (!GET_SUSTAINED(ch))
    return FALSE;
    
  for (struct sustain_data *hjp = GET_SUSTAINED(ch); hjp; hjp = hjp->next)
    if ((hjp->spell == type) && (hjp->caster == FALSE))
      return TRUE;
  
  return FALSE;
}

bool affected_by_power(struct char_data *ch, int type)
{
  for (struct spirit_sustained *sust = SPIRIT_SUST(ch); sust ; sust = sust->next)
    if (sust->type == type)
      return TRUE;
  return FALSE;
}
void veh_from_room(struct veh_data * veh)
{
  struct veh_data *temp;
  if (veh == NULL) {
    log("SYSERR: Null vehicle passed to veh_from_room. Terminating.");
    exit(1);
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
    veh->in_room->light[0]--;
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
  
  if (CH_IN_COMBAT(ch))
    stop_fighting(ch);
  
  if (ch->in_room) {
    // Character is in a room. Clean up room effects sourced by character.
    if (GET_EQ(ch, WEAR_LIGHT) != NULL)
      if (GET_OBJ_TYPE(GET_EQ(ch, WEAR_LIGHT)) == ITEM_LIGHT)
        if (GET_OBJ_VAL(GET_EQ(ch, WEAR_LIGHT), 2))       /* Light is ON */
          ch->in_room->light[0]--;
    if (affected_by_spell(ch, SPELL_LIGHT) && !--ch->in_room->light[0])
      ch->in_room->light[1] = 0;
    if (affected_by_spell(ch, SPELL_SHADOW) && !--ch->in_room->shadow[0])
      ch->in_room->shadow[1] = 0;
    if (affected_by_spell(ch, SPELL_POLTERGEIST) && !--ch->in_room->poltergeist[0])
      ch->in_room->poltergeist[1] = 0;
    if (affected_by_spell(ch, SPELL_SILENCE) && !--ch->in_room->silence[0])
      ch->in_room->silence[1] = 0;
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
    AFF_FLAGS(ch).RemoveBit(AFF_PILOT);
  }
}

void char_to_veh(struct veh_data * veh, struct char_data * ch)
{
  if (!veh || !ch)
    mudlog("SYSLOG: Illegal value(s) passed to char_to_veh", NULL, LOG_SYSLOG, TRUE);
  else {
    if (ch->in_room || ch->in_veh)
      char_from_room(ch);
    
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
    room->light[0]++; // headlights
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
    dest->usedload += veh->body * mult;
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
  ch->next_in_room = room->people;
  room->people = ch;
  ch->in_room = room;
  if (IS_SENATOR(ch) && PRF_FLAGGED(ch, PRF_PACIFY))
    room->peaceful++;
  
  if (GET_EQ(ch, WEAR_LIGHT))
    if (GET_OBJ_TYPE(GET_EQ(ch, WEAR_LIGHT)) == ITEM_LIGHT)
      if (GET_OBJ_VAL(GET_EQ(ch, WEAR_LIGHT), 2))     /* Light ON */
        room->light[0]++;
  if (GET_TRADITION(ch) == TRAD_SHAMANIC)
    GET_DOMAIN(ch) = SECT(ch->in_room);
  if (affected_by_spell(ch, SPELL_SILENCE))
  {
    room->silence[0]++;
    for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = sust->next)
      if (sust->spell == SPELL_SILENCE && MIN(sust->force, sust->success) > room->silence[1]) {
        room->silence[1] = MIN(sust->force, sust->success);
        break;
      }
  }
  if (affected_by_spell(ch, SPELL_SHADOW))
  {
    room->shadow[0]++;
    for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = sust->next)
      if (sust->spell == SPELL_SHADOW && MIN(sust->force, sust->success) > room->shadow[1]) {
        room->shadow[1] = MIN(sust->force, sust->success);
        break;
      }
  }
  if (affected_by_spell(ch, SPELL_LIGHT))
  {
    room->light[0]++;
    for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = sust->next)
      if (sust->spell == SPELL_LIGHT && MIN(sust->force, sust->success) > room->light[1]) {
        room->light[1] = MIN(sust->force, sust->success);
        break;
      }
  }
  if (affected_by_spell(ch, SPELL_POLTERGEIST))
  {
    room->poltergeist[0]++;
    for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = sust->next)
      if (sust->spell == SPELL_POLTERGEIST)
        if (sust->force > room->poltergeist[1]) {
          room->poltergeist[1] = sust->force;
          break;
        }
  }
}

#define IS_INVIS(o) IS_OBJ_STAT(o, ITEM_INVISIBLE)

// Checks obj_to_x preconditions for common errors. Overwrites buf3. Returns TRUE for kosher, FALSE otherwise.
bool check_obj_to_x_preconditions(struct obj_data * object, struct char_data *ch) {
  if (!object) {
    mudlog("ERROR: Null object passed to check_obj_to_x_preconditions().", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  // Pre-compose our message header.
  snprintf(buf3, sizeof(buf3), "ERROR: check_obj_to_x_preconditions() failure for %s (%ld): ", GET_OBJ_NAME(object), GET_OBJ_VNUM(object));
  
  // Fail if the object already has next_content. This implies that it's part of someone else's linked list-- never merge them!
  if (object->next_content) {
    strcat(ENDOF(buf3), "It's already part of a next_content linked list.");
    mudlog(buf3, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  // We can't give an object away that's already someone else's possession.
  if (object->carried_by) {
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "Object already belongs to %s.", GET_CHAR_NAME(object->carried_by));
    mudlog(buf3, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  // We can't give an object away if it's sitting in a room.
  if (object->in_room) {
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "Object is already in room %ld.", object->in_room->number);
    mudlog(buf3, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  // We can't give an object away if it's in a vehicle.
  if (object->in_veh) {
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "Object is already in vehicle %s.", GET_VEH_NAME(object->in_veh));
    mudlog(buf3, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  strcpy(buf3, "");
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
  int i;
  
  if (bio == NULL)
  {
    log("SYSLOG: NULL object passed to obj_from_bioware");
    return;
  }
  if (GET_OBJ_VAL(bio, 0) == BIO_ADRENALPUMP && GET_OBJ_VAL(bio, 5) < 1)
    for (i = 0; i < MAX_OBJ_AFFECT; i++)
      affect_modify(bio->carried_by,
                    bio->affected[i].location,
                    bio->affected[i].modifier,
                    bio->obj_flags.bitvector, FALSE);
  
  affect_total(bio->carried_by);
  
  REMOVE_FROM_LIST(bio, bio->carried_by->bioware, next_content);
  bio->carried_by = NULL;
  bio->next_content = NULL;
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
    log("SYSLOG: NULL object passed to obj_from_cyberware");
    return;
  }
  REMOVE_FROM_LIST(cyber, cyber->carried_by->cyberware, next_content);
  cyber->carried_by = NULL;
  cyber->next_content = NULL;
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
  if (IS_OBJ_STAT(obj, ITEM_GODONLY) && !IS_NPC(ch) && !IS_SENATOR(ch))
  {
    act("You are zapped by $p and instantly let go of it.", FALSE, ch, obj, 0, TO_CHAR);
    act("$n is zapped by $p and instantly lets go of it.", FALSE, ch, obj, 0, TO_ROOM);
    obj_to_room(obj, get_ch_in_room(ch));     /* changed to drop in inventory instead of
                               * ground */  // and now I've changed it back, who wants morts running around with god-only keys
    return FALSE;
  }
  
  GET_EQ(ch, pos) = obj;
  obj->worn_by = ch;
  obj->worn_on = pos;
  
  if (ch->in_room)
  {
    if (pos == WEAR_LIGHT && GET_OBJ_TYPE(obj) == ITEM_LIGHT)
      if (GET_OBJ_VAL(obj, 2))  /* if light is ON */
        ch->in_room->light[0]++;
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
    if (pos == WEAR_LIGHT && GET_OBJ_TYPE(obj) == ITEM_LIGHT)
      if (GET_OBJ_VAL(obj, 2))  /* if light is ON */
        ch->in_room->light[0]--;
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

int get_number(char **name)
{
  int i;
  char *ppos;
  char number[MAX_INPUT_LENGTH];
  
  *number = '\0';
  
  if ((ppos = strchr(*name, '.'))) {
    *ppos++ = '\0';
    strcpy(number, *name);
    strcpy(*name, ppos);
    
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
  strcpy(tmp, name);
  if (!strncmp(tmp, "my.", 3)) {
    mine = TRUE;
    strcpy(tmp, name+3);
  }
  if (!(number = get_number(&tmp)))
    return NULL;
  for (i = list; i && (j <= number); i = i->next_veh)
    if (isname(tmp, GET_VEH_NAME(i)) || isname(tmp, i->name)) {
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
  
  strcpy(tmp, name);
  if (!(number = get_number(&tmp)))
    return NULL;
  
  for (i = room->people; i && (j <= number); i = i->next_in_room)
    if (isname(tmp, GET_KEYWORDS(i)) ||
        isname(tmp, GET_NAME(i)) || isname(tmp, GET_CHAR_NAME(i)))
      if (++j == number)
        return i;
  
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
  if (!object || (!object->in_room && !object->in_veh))
  {
    log("SYSLOG: NULL object or obj not in a room passed to obj_from_room");
    return;
  }
  if (object->in_veh)
  {
    object->in_veh->usedload -= GET_OBJ_WEIGHT(object);
    REMOVE_FROM_LIST(object, object->in_veh->contents, next_content);
  } else {
    // Handle workshop removal.
    if (GET_OBJ_TYPE(object) == ITEM_WORKSHOP)
      remove_workshop_from_room(object);
    REMOVE_FROM_LIST(object, object->in_room->contents, next_content);
  }
  
  object->in_veh = NULL;
  object->in_room = NULL;
  object->next_content = NULL;
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
  
  // Update the highest container's weight as well.
  if (GET_OBJ_TYPE(tmp_obj) != ITEM_CYBERDECK || GET_OBJ_TYPE(tmp_obj) != ITEM_CUSTOM_DECK || GET_OBJ_TYPE(tmp_obj) != ITEM_DECK_ACCESSORY)
    GET_OBJ_WEIGHT(tmp_obj) += GET_OBJ_WEIGHT(obj);
  
  // If someone's carrying or wearing the highest container, increment their carry weight by the weight of the obj we just put in.
  if (tmp_obj->carried_by)
    IS_CARRYING_W(tmp_obj->carried_by) += GET_OBJ_WEIGHT(obj);
  if (tmp_obj->worn_by)
    IS_CARRYING_W(tmp_obj->worn_by) += GET_OBJ_WEIGHT(obj);
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
  if (GET_OBJ_TYPE(temp) != ITEM_CYBERDECK || GET_OBJ_TYPE(temp) != ITEM_DECK_ACCESSORY || GET_OBJ_TYPE(temp) != ITEM_CUSTOM_DECK)
    GET_OBJ_WEIGHT(temp) -= GET_OBJ_WEIGHT(obj);
  
  obj->in_obj = NULL;
  obj->next_content = NULL;
  
  // Recalculate the bearer's weight.
  if (temp->carried_by)
    IS_CARRYING_W(temp->carried_by) -= GET_OBJ_WEIGHT(obj);
  if (temp->worn_by)
    IS_CARRYING_W(temp->worn_by) -= GET_OBJ_WEIGHT(obj);
}

void extract_icon(struct matrix_icon * icon)
{
  struct matrix_icon *temp;
  for (struct phone_data *k = phone_list; k; k = k->next)
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
  
  if (icon->in_host)
  {
    if (icon->fighting) {
      for (struct matrix_icon *vict = matrix[icon->in_host].icons; vict; vict = vict->next_in_host)
        if (vict->fighting == icon) {
          REMOVE_FROM_LIST(vict, matrix[icon->in_host].fighting, next_fighting);
          vict->fighting = NULL;
        }
      REMOVE_FROM_LIST(icon, matrix[icon->in_host].fighting, next_fighting);
    }
    if (icon->in_host != NOWHERE)
      icon_from_host(icon);
  }
  if (icon->decker)
  {
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
  } else
  {
    ic_index[icon->number].number--;
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
    strcpy(buf3, GET_VEH_NAME(veh));
    snprintf(buf, sizeof(buf), "%s falls from %s's towing equipment.\r\n", GET_VEH_NAME(veh->towing), buf3);
    if (ch->in_veh->in_room) {
      act(buf, FALSE, ch->in_veh->in_room->people, 0, 0, TO_ROOM);
      act(buf, FALSE, ch->in_veh->in_room->people, 0, 0, TO_CHAR);
      veh_to_room(veh->towing, veh->in_room);
    } else if (ch->in_veh->in_veh){
      send_to_veh(buf, ch->in_veh->in_veh, ch, TRUE);
      veh_to_veh(veh->towing, veh->in_veh);
    } else {
      snprintf(buf, sizeof(buf), "SYSERR: Veh %s (%ld) has neither in_room nor in_veh! Dropping towed veh in Dante's Garage.", GET_VEH_NAME(ch->in_veh), ch->in_veh->idnum);
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      // Can't stop, we're already blowing up the vehicle. Drop it in Dante's garage.
      veh_to_room(veh->towing, &world[real_room(RM_DANTES_GARAGE)]);
    }
    veh->towing = NULL;
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
  if (veh->in_room)
    veh_from_room(veh);
  veh_index[veh->veh_number].number--;
  Mem->DeleteVehicle(veh);
}

/* Extract an object from the world */
void extract_obj(struct obj_data * obj)
{
  struct phone_data *phone, *temp;
  bool set = FALSE;
  if (obj->worn_by != NULL)
    if (unequip_char(obj->worn_by, obj->worn_on, TRUE) != obj)
      log("SYSLOG: Inconsistent worn_by and worn_on pointers!!");
  if (GET_OBJ_TYPE(obj) == ITEM_PHONE ||
      (GET_OBJ_TYPE(obj) == ITEM_CYBERWARE && GET_OBJ_VAL(obj, 0) == CYB_PHONE)) {
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
  
  ACMD_CONST(do_return);
  void die_follower(struct char_data * ch);
  
  if (!IS_NPC(ch))
    playerDB.SaveChar(ch, GET_LOADROOM(ch));
  
  if (!IS_NPC(ch) && !ch->desc)
  {
    for (t_desc = descriptor_list; t_desc; t_desc = t_desc->next)
      if (t_desc->original == ch)
        do_return(t_desc->character, "", 0, 0);
  }
  if (ch->followers)
  {
    struct follow_type *nextfollow;
    for (struct follow_type *follow = ch->followers; follow; follow = nextfollow) {
      nextfollow = follow->next;
      if (IS_SPIRIT(follow->follower) || IS_ELEMENTAL(follow->follower))
        extract_char(follow->follower);
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
    if (IS_ELEMENTAL(ch) || IS_SPIRIT(ch)) {
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
  if (IS_ELEMENTAL(ch) && GET_SUSTAINED_NUM(ch))
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
  
  /* clear any sustained spells */
  else if (GET_SUSTAINED(ch))
  {
    struct sustain_data *next;
    for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = next) {
      next = sust->next;
      if (next && sust->idnum == next->idnum)
        next = next->next;
      end_sustained_spell(ch, sust);
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
    clearMemory(ch);            /* Only NPC's can have memory */
    Mem->DeleteCh(ch);
  }
}

/* ***********************************************************************
 Here follows high-level versions of some earlier routines, ie functions
 which incorporate the actual player-data.
 *********************************************************************** */

struct char_data *get_player_vis(struct char_data * ch, char *name, int inroom)
{
  struct char_data *i;
  
  for (i = character_list; i; i = i->next)
    if (!IS_NPC(i) && (!inroom || i->in_room == ch->in_room) &&
        (isname(name, GET_KEYWORDS(i)) || isname(name, GET_CHAR_NAME(i)) || recog(ch, i, name))
        && GET_LEVEL(ch) >= GET_INCOG_LEV(i))
      return i;
  
  return NULL;
}

struct char_data *get_char_veh(struct char_data * ch, char *name, struct veh_data * veh)
{
  struct char_data *i;
  
  for (i = veh->people; i; i = i->next_in_veh)
    if ((isname(name, GET_KEYWORDS(i)) || isname(name, GET_CHAR_NAME(i)) || recog(ch, i, name))
        && CAN_SEE(ch, i))
      return i;
  
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
  strcpy(tmp, name);
  if (!(number = get_number(&tmp)))
    return get_player_vis(ch, tmp, 1);
  
  if (ch->in_veh)
    if ((i = get_char_veh(ch, name, ch->in_veh)))
      return i;
  
  for (i = ch->in_veh ? ch->in_veh->people : ch->in_room->people; i && j <= number; i = ch->in_veh ? i->next_in_veh : i->next_in_room)
    if ((isname(tmp, GET_KEYWORDS(i)) ||
         isname(tmp, GET_NAME(i)) || recog(ch, i, name)) &&
        CAN_SEE(ch, i))
      if (++j == number)
        return i;
  
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
  strcpy(tmp, name);
  if (!(number = get_number(&tmp)))
    return get_player_vis(ch, tmp, 1);
  
  for (; list && j <= number; list = list->next_in_room)
    if ((isname(tmp, GET_KEYWORDS(list)) ||
         isname(tmp, GET_NAME(list)) || recog(ch, list, name)) &&
        CAN_SEE(ch, list))
      if (++j == number)
        return list;
  
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
  
  strcpy(tmp, name);
  if (!(number = get_number(&tmp)))
    return get_player_vis(ch, tmp, 0);
  
  for (i = character_list; i && (j <= number); i = i->next)
    if ((isname(tmp, GET_KEYWORDS(i)) || recog(ch, i, name)) &&
        CAN_SEE(ch, i))
      if (++j == number)
        return i;
  
  return NULL;
}

struct obj_data *get_obj_in_list_vis(struct char_data * ch, char *name, struct obj_data * list)
{
  struct obj_data *i;
  int j = 0, number;
  char tmpname[MAX_INPUT_LENGTH];
  char *tmp = tmpname;
  
  strcpy(tmp, name);
  if (!(number = get_number(&tmp)))
    return NULL;
  
  for (i = list; i && (j <= number); i = i->next_content) {
    if (ch->in_veh && i->in_veh && i->vfront != ch->vfront)
      continue;
    if (isname(tmp, i->text.keywords) || isname(tmp, i->text.name) || (i->restring && isname(tmp, i->restring)))
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
  
  strcpy(tmp, name);
  if (!(number = get_number(&tmp)))
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
  
  strcpy(tmp, arg);
  if (!(number = get_number(&tmp)))
    return NULL;
  
  for ((*j) = 0; (*j) < NUM_WEARS && i <= number; (*j)++)
    if (equipment[(*j)])
    {
      if (isname(tmp, equipment[(*j)]->text.keywords) || isname(tmp, equipment[(*j)]->text.name)  ||
          (equipment[(*j)]->restring && isname(tmp, equipment[(*j)]->restring)))
        if (++i == number)
          return (equipment[(*j)]);
      if (GET_OBJ_TYPE(equipment[(*j)]) == ITEM_WORN && equipment[(*j)]->contains)
        for (struct obj_data *obj = equipment[(*j)]->contains; obj; obj = obj->next_content)
          if (isname(tmp, obj->text.keywords) || isname(tmp, obj->text.name) ||
              (obj->restring && isname(tmp, obj->restring)))
            if (++i == number)
              return (obj);
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
  
  if (amount < 2500)
    num = 100;     // plastic credstick
  else if (amount < 10000)
    num = 101;     // steel credstick
  else if (amount < 50000)
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
  int index = 0, ok;
  char *temp, *temp2;
  char first[256], first2[256];
  
  while (++index < MAX_SKILLS) {
    if (is_abbrev(name, skills[index].name))
      return index;
    
    ok = 1;
    temp = any_one_arg(skills[index].name, first);
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

int find_spell_num(char *name)
{
  int index = 0, ok;
  const char *temp, *temp2;
  char first[256], first2[256];
  
  while (++index < MAX_SPELLS) {
    if (is_abbrev(name, spells[index].name))
      return index;
    
    ok = 1;
    temp = any_one_arg_const(spells[index].name, first);
    temp2 = any_one_arg(name, first2);
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
  char name[256];
  
  one_argument(arg, name);
  
  if (!*name)
    return (0);
  
  *tar_ch = NULL;
  *tar_obj = NULL;
  
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
    strcpy(tmp, name);
    number = MAX(1, get_number(&tmp));
    
    for (i = get_ch_in_room(ch)->people; i && j <= number; i = i->next_in_room)
      if ((isname(tmp, GET_KEYWORDS(i)) ||
           isname(tmp, GET_NAME(i)) || recog(ch, i, name)) &&
          CAN_SEE(ch, i))
        if (++j == number) {
          *tar_ch = i;
          return (FIND_CHAR_VEH_ROOM);
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
    if (ch->in_veh) {
      if ((*tar_obj = get_obj_in_list_vis(ch, name, ch->in_veh->contents)))
        return (FIND_OBJ_ROOM);
    } else if ((*tar_obj = get_obj_in_list_vis(ch, name, ch->in_room->contents)))
      return (FIND_OBJ_ROOM);
  }
  if (IS_SET(bitvector, FIND_OBJ_VEH_ROOM))
  {
    if (ch->in_veh && (*tar_obj = get_obj_in_list_vis(ch, name, (get_ch_in_room(ch))->contents)))
      return (FIND_OBJ_VEH_ROOM);
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
int find_all_dots(char *arg)
{
  if (!strcmp(arg, "all"))
    return FIND_ALL;
  else if (!strncmp(arg, "all.", 4)) {
    strcpy(buf, arg + 4);
    strcpy(arg, buf);
    return FIND_ALLDOT;
  } else
    return FIND_INDIV;
}

int veh_skill(struct char_data *ch, struct veh_data *veh)
{
  int skill = 0;
  
  switch (veh->type)
  {
    case VEH_CAR:
      skill = GET_SKILL(ch, SKILL_PILOT_CAR);
      if (!skill)
        skill = (int)(GET_SKILL(ch, SKILL_PILOT_TRUCK) / 2);
      if (!skill)
        skill = (int)(GET_SKILL(ch, SKILL_PILOT_CAR) / 2);
      break;
    case VEH_BIKE:
      skill = GET_SKILL(ch, SKILL_PILOT_BIKE);
      if (!skill)
        skill = (int)(GET_SKILL(ch, SKILL_PILOT_CAR) / 2);
      if (!skill)
        skill = (int)(GET_SKILL(ch, SKILL_PILOT_TRUCK) / 2);
      break;
    case VEH_TRUCK:
      skill = GET_SKILL(ch, SKILL_PILOT_TRUCK);
      if (!skill)
        skill = (int)(GET_SKILL(ch, SKILL_PILOT_CAR) / 2);
      if (!skill)
        skill = (int)(GET_SKILL(ch, SKILL_PILOT_BIKE) / 2);
      break;
  }
  if (AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE))
    skill += GET_CONTROL(ch);
  
  // Assume any NPC with a vehicle and no skill has a minimum skill of 4 and that the builder just forgot to set it.
  if (IS_NPC(ch) && skill == 0)
    skill = MAX(skill, 4);
  
  return skill;
}
