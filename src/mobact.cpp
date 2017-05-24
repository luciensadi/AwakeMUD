/* ************************************************************************
*   File: mobact.c                                      Part of CircleMUD *
*  Usage: Functions for generating intelligent (?) behavior in mobiles    *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <stdio.h>
#include <stdlib.h>

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "db.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "constants.h"

/* external structs */
extern void resist_drain(struct char_data *ch, int power, int drain_add, int wound);
extern int find_sight(struct char_data *ch);
extern int find_weapon_range(struct char_data *ch, struct obj_data *weapon);

extern void cast_detection_spell(struct char_data *ch, int spell, int force, char *arg, char_data *mob);
extern void cast_manipulation_spell(struct char_data *ch, int spell, int force, char *arg, char_data *mob);
extern void cast_illusion_spell(struct char_data *ch, int spell, int force, char *arg, char_data *mob);
extern void cast_health_spell(struct char_data *ch, int spell, int sub, int force, char *arg, char_data *mob);

extern void perform_wear(struct char_data *, struct obj_data *, int);
extern void perform_remove(struct char_data *, int);

extern bool is_escortee(struct char_data *mob);
extern bool hunting_escortee(struct char_data *ch, struct char_data *vict);

extern bool ranged_response(struct char_data *ch, struct char_data *vict);

bool memory(struct char_data *ch, struct char_data *vict);
int violates_zsp(int security, struct char_data *ch, int pos, struct char_data *mob);
bool attempt_reload(struct char_data *mob, int pos);

#define MOB_AGGR_TO_RACE MOB_AGGR_ELF, MOB_AGGR_DWARF, MOB_AGGR_HUMAN, MOB_AGGR_ORK, MOB_AGGR_TROLL

void mobile_activity(void)
{
  struct char_data *ch, *next_ch, *vict;
  struct veh_data *veh;
  struct obj_data *obj, *best_obj;
  int door, max, i, dir, distance, room, nextroom, has_acted;

  extern int no_specials;

  ACMD(do_get);

  for (ch = character_list; ch; ch = next_ch) {
    next_ch = ch->next;
    if (!IS_MOB(ch) || !AWAKE(ch) || ch->desc)
      continue;
    has_acted = 0;

    if ((FIGHTING(ch) && FIGHTING(ch)->in_room == ch->in_room) || FIGHTING_VEH(ch))
      continue;

    if (GET_MOBALERT(ch) > MALERT_CALM && --GET_MOBALERTTIME(ch) <= 0)
      GET_MOBALERT(ch) = MALERT_CALM;

    /* Examine call for special procedure */
    if (MOB_FLAGGED(ch, MOB_SPEC) && !no_specials) {
      if (mob_index[GET_MOB_RNUM(ch)].func == NULL) {
        log("%s (#%d): Attempting to call non-existing mob func",
            GET_NAME(ch), GET_MOB_VNUM(ch));

        MOB_FLAGS(ch).RemoveBit(MOB_SPEC);
      } else if ((mob_index[GET_MOB_RNUM(ch)].func) (ch, ch, 0, ""))
        continue;
      else if (mob_index[GET_MOB_RNUM(ch)].sfunc != NULL)
        if ((mob_index[GET_MOB_RNUM(ch)].sfunc) (ch, ch, 0, ""))
          continue;
    }

    /* Scavenger (picking up objects) */
    if (!has_acted && MOB_FLAGGED(ch, MOB_SCAVENGER))
      if (world[ch->in_room].contents && !number(0, 10)) {
        max = 1;
        best_obj = NULL;
        for (obj = world[ch->in_room].contents; obj; obj = obj->next_content)
          if (CAN_GET_OBJ(ch, obj) && GET_OBJ_COST(obj) > max && GET_OBJ_TYPE(obj) != ITEM_WORKSHOP) {
            best_obj = obj;
            max = GET_OBJ_COST(obj);
          }
        if (best_obj != NULL) {
          obj_from_room(best_obj);
          obj_to_char(best_obj, ch);
          act("$n gets $p.", FALSE, ch, best_obj, 0, TO_ROOM);
          has_acted = TRUE;
        }
      }

    /* Mob Movement */
    if (!has_acted && !MOB_FLAGGED(ch, MOB_SENTINEL) && !FIGHTING(ch) &&
        GET_POS(ch) == POS_STANDING && ch->in_room != NOWHERE &&
        (door = number(0, 18)) < NUM_OF_DIRS && CAN_GO(ch, door) &&
        !ROOM_FLAGS(EXIT(ch, door)->to_room).AreAnySet(ROOM_NOMOB,
            ROOM_DEATH, ENDBIT) &&
        (!MOB_FLAGGED(ch, MOB_STAY_ZONE) ||
         (world[EXIT(ch, door)->to_room].zone == world[ch->in_room].zone))) {
      perform_move(ch, door, CHECK_SPECIAL | LEADER, NULL);
      has_acted = TRUE;
    }
    
    /* Mob Driving */
    if (!has_acted && ch->in_veh && AFF_FLAGGED(ch, AFF_PILOT)
        && (door = number(0, 18)) < NUM_OF_DIRS && EXIT(ch->in_veh, door) &&
        (ROOM_FLAGGED(EXIT(ch->in_veh, door)->to_room, ROOM_ROAD) ||
         ROOM_FLAGGED(EXIT(ch->in_veh, door)->to_room, ROOM_GARAGE)) && !ROOM_FLAGGED(EXIT(ch->in_veh, door)->to_room, ROOM_NOMOB)) {
      perform_move(ch, door, LEADER, NULL);
      has_acted = TRUE;
    }
    
    /* Aggressive Mobs */
    if (!ch->in_veh && !ROOM_FLAGGED(ch->in_room, ROOM_PEACEFUL))
      if (!has_acted &&
          (MOB_FLAGS(ch).AreAnySet(MOB_AGGRESSIVE, MOB_AGGR_TO_RACE, ENDBIT) || GET_MOBALERT(ch) == MALERT_ALARM)) {
        bool vehicle = number(0, 1);
        if (vehicle && !IS_ASTRAL(ch)) {
          for (veh = world[ch->in_room].vehicles; veh; veh = veh->next_veh)
            if (veh->damage < 10) {
              stop_fighting(ch);
              set_fighting(ch, veh);
              has_acted = TRUE;
              break;
            }
        } else if (!vehicle || !has_acted) {
          for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room) {
            if ((IS_NPC(vict) && !IS_PROJECT(vict) && !is_escortee(vict)) ||
                !CAN_SEE(ch, vict) || PRF_FLAGGED(vict, PRF_NOHASSLE) || GET_PHYSICAL(vict) < 0)
              continue;
            if (MOB_FLAGGED(ch, MOB_WIMPY) && AWAKE(vict))
              continue;
            if (hunting_escortee(ch, vict)) {
              stop_fighting(ch);
              set_fighting(ch, vict);
              has_acted = TRUE;
              break;
            } else if (!MOB_FLAGS(ch).AreAnySet(MOB_AGGR_TO_RACE, ENDBIT) ||
                       (MOB_FLAGGED(ch, MOB_AGGR_ELF) &&
                        (GET_RACE(vict) == RACE_ELF || GET_RACE(vict) == RACE_WAKYAMBI || GET_RACE(vict) == RACE_NIGHTONE || GET_RACE(vict) == RACE_DRYAD)) ||
                       (MOB_FLAGGED(ch, MOB_AGGR_DWARF) &&
                        (GET_RACE(vict) == RACE_DWARF || GET_RACE(vict) == RACE_GNOME || GET_RACE(vict) == RACE_MENEHUNE || GET_RACE(vict) == RACE_KOBOROKURU)) ||
                       (MOB_FLAGGED(ch, MOB_AGGR_HUMAN) &&
                        GET_RACE(vict) == RACE_HUMAN) ||
                       (MOB_FLAGGED(ch, MOB_AGGR_ORK) &&
                        (GET_RACE(vict) == RACE_ORK || GET_RACE(vict) == RACE_HOBGOBLIN || GET_RACE(vict) == RACE_OGRE || GET_RACE(vict) == RACE_SATYR || GET_RACE(vict) == RACE_ONI)) ||
                       (MOB_FLAGGED(ch, MOB_AGGR_TROLL) &&
                        (GET_RACE(vict) == RACE_TROLL || GET_RACE(vict) == RACE_CYCLOPS || GET_RACE(vict) == RACE_FOMORI || GET_RACE(vict) == RACE_GIANT || GET_RACE(vict) == RACE_MINOTAUR))) {
              stop_fighting(ch);
              set_fighting(ch, vict);
              has_acted = TRUE;
              break;
            }
          }
        }
      }

    /* Mob Memory */
    if (!has_acted && MOB_FLAGGED(ch, MOB_MEMORY) && MEMORY(ch)) {
      for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room) {
        if (IS_NPC(vict) ||
            !CAN_SEE(ch, vict) ||
            PRF_FLAGGED(vict, PRF_NOHASSLE))
          continue;
        if (memory(ch, vict)) {
          act("'Hey!  You're the fiend that attacked me!!!', exclaims $n.",
              FALSE, ch, 0, 0, TO_ROOM);
          stop_fighting(ch);
          set_fighting(ch, vict);
          has_acted = TRUE;
          break;
        }
      }
    }

    /* Helper Mobs */
    if (!has_acted && MOB_FLAGGED(ch, MOB_HELPER)) {
      for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room) {
        if (ch != vict &&
            FIGHTING(vict) &&
            ch != FIGHTING(vict) &&
            CAN_SEE(ch, FIGHTING(vict))) {
          if (IS_NPC(vict) && !IS_NPC(FIGHTING(vict))) {
            // An NPC in my area is being attacked by a player.
            if (FIGHTING(vict)->in_room == ch->in_room) {
              act("$n jumps to the aid of $N!",
                  FALSE, ch, 0, vict, TO_ROOM);
              stop_fighting(ch);
              
              // Close-ranged response.
              set_fighting(ch, FIGHTING(vict));
            } else {
              // Long-ranged response.
              if (ranged_response(FIGHTING(vict), ch)) {
                // TODO: This doesn't fire a message if the NPC is wielding a ranged weapon.
                act("$n jumps to $N's aid against $S distant attacker!",
                    FALSE, ch, 0, vict, TO_ROOM);
              }
            }
            has_acted = TRUE;
            break;
          } else if (!IS_NPC(vict) && IS_NPC(FIGHTING(vict))) {
            // A player in my area is attacking an NPC.
            act("$n jumps to the aid of $N!", FALSE, ch, 0, FIGHTING(vict), TO_ROOM);
            stop_fighting(ch);
              
            // Close-ranged response.
            set_fighting(ch, vict);
              
            has_acted = TRUE;
            break;
          }
        }
      }
    }

    if (!ch->in_veh)
      if (!has_acted && MOB_FLAGGED(ch, MOB_GUARD)) {
        for (veh = world[ch->in_room].vehicles; veh; veh = veh->next_veh)
          if (veh->type == VEH_DRONE || (!(ROOM_FLAGGED(ch->in_room, ROOM_ROAD) || ROOM_FLAGGED(ch->in_room, ROOM_GARAGE)) && veh->type == VEH_BIKE))
            if (veh->damage < 10) {
              set_fighting(ch, veh);
              has_acted = TRUE;
              break;
            }
        for (vict = world[ch->in_room].people;
             vict && !has_acted;
             vict = vict->next_in_room)
          if (!IS_NPC(vict) &&
              !PRF_FLAGGED(vict, PRF_NOHASSLE) &&
              CAN_SEE(ch, vict) && GET_PHYSICAL(vict) > 0)
            for (i = 0; i < NUM_WEARS; i++)
              if (GET_EQ(vict, i) &&
                  violates_zsp(zone_table[world[ch->in_room].zone].security,
                               vict, i, ch)) {
                stop_fighting(ch);
                set_fighting(ch, vict);
                has_acted = TRUE;
                break;
              }
      }
    if (!has_acted &&
        MOB_FLAGGED(ch, MOB_SNIPER) &&
        GET_EQ(ch, WEAR_WIELD) &&
        MOB_FLAGS(ch).AreAnySet(MOB_GUARD, MOB_AGGRESSIVE,
                                MOB_AGGR_TO_RACE, MOB_MEMORY, ENDBIT)) {
      for (dir = 0; !FIGHTING(ch) && dir < NUM_OF_DIRS; dir++) {
        room = ch->in_room;

        if (CAN_GO2(room, dir) &&
            world[EXIT2(room, dir)->to_room].zone == world[ch->in_room].zone)
          nextroom = EXIT2(room, dir)->to_room;
        else
          nextroom = NOWHERE;

        for (distance = 1;
             nextroom != NOWHERE &&
             distance <= find_sight(ch) &&
             distance <= find_weapon_range(ch, GET_EQ(ch, WEAR_WIELD));
             distance++) {
          for (vict = world[nextroom].people;
               vict;
               vict = vict->next_in_room) {
            if (!IS_NPC(vict) &&
                !PRF_FLAGGED(vict, PRF_NOHASSLE) &&
                CAN_SEE(ch, vict)) {
              if (MOB_FLAGGED(ch, MOB_GUARD))
                for (i = 0; i < NUM_WEARS; i++)
                  if (GET_EQ(vict, i) &&
                      violates_zsp(zone_table[world[ch->in_room].zone].security, vict, i, ch)) {
                    stop_fighting(ch);
                    set_fighting(ch, vict);
                    has_acted = TRUE;
                    break;
                  }
              if (!has_acted &&
                  MOB_FLAGS(ch).AreAnySet(MOB_AGGRESSIVE,
                                          MOB_AGGR_TO_RACE, ENDBIT))
                if (!(MOB_FLAGGED(ch, MOB_WIMPY) && AWAKE(vict)) &&
                    (!MOB_FLAGS(ch).AreAnySet(MOB_AGGR_TO_RACE, ENDBIT) ||
                     (MOB_FLAGGED(ch, MOB_AGGR_ELF) &&
                      (GET_RACE(vict) == RACE_ELF || RACE_DRYAD || RACE_WAKYAMBI || RACE_NIGHTONE)) ||
                     (MOB_FLAGGED(ch, MOB_AGGR_DWARF) &&
                      (GET_RACE(vict) == RACE_DWARF || RACE_MENEHUNE || RACE_KOBOROKURU || RACE_GNOME)) ||
                     (MOB_FLAGGED(ch, MOB_AGGR_HUMAN) &&
                      GET_RACE(vict) == RACE_HUMAN) ||
                     (MOB_FLAGGED(ch, MOB_AGGR_ORK) &&
                      (GET_RACE(vict) == RACE_ORK || RACE_HOBGOBLIN || RACE_SATYR || RACE_ONI || RACE_OGRE)) ||
                     (MOB_FLAGGED(ch, MOB_AGGR_TROLL) &&
                      (GET_RACE(vict) == RACE_TROLL || RACE_FOMORI || RACE_GIANT || RACE_MINOTAUR || RACE_CYCLOPS)))) {
                  stop_fighting(ch);
                  set_fighting(ch, vict);
                  has_acted = TRUE;
                }
              if (!has_acted &&
                  MOB_FLAGGED(ch, MOB_MEMORY) &&
                  MEMORY(ch) &&
                  memory(ch, vict)) {
                act("'Hey!  You're the fiend that attacked me!!!', "
                    "exclaims $n.", FALSE, ch, 0, 0, TO_ROOM);
                act("'Hey!  You're the fiend that attacked me!!!', "
                    "exclaims $N.", FALSE, vict, 0, ch, TO_ROOM);
                stop_fighting(ch);
                set_fighting(ch, vict);
                has_acted = TRUE;
              }
            }
            if (has_acted)
              break;
          }
          room = nextroom;
          if (CAN_GO2(room, dir) &&
              world[EXIT2(room, dir)->to_room].zone == world[ch->in_room].zone)
            nextroom = EXIT2(room, dir)->to_room;
          else
            nextroom = NOWHERE;
          if (has_acted)
            break;
        }
        if (has_acted)
          break;
      }
    }
    if (GET_EQ(ch, WEAR_WIELD) && IS_GUN(GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3))) {
      if (!GET_EQ(ch, WEAR_WIELD)->contains)
        attempt_reload(ch, WEAR_WIELD);
      if (!GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 11)) {
        if (IS_SET(GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 10), 1 << MODE_SS))
          GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 11) = MODE_SS;
        if (IS_SET(GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 10), 1 << MODE_SA))
          GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 11) = MODE_SA;
        if (IS_SET(GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 10), 1 << MODE_FA)) {
          GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 11) = MODE_FA;
          GET_OBJ_TIMER(GET_EQ(ch, WEAR_WIELD)) = 10;
        }
        if (IS_SET(GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 10), 1 << MODE_BF))
          GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 11) = MODE_BF;
      }
    }
    if (!has_acted && GET_SKILL(ch, SKILL_SORCERY) && !MOB_FLAGGED(ch, MOB_SPEC)) {
      if (GET_PHYSICAL(ch) < GET_MAX_PHYSICAL(ch) && !AFF_FLAGGED(ch, AFF_HEALED))
        cast_health_spell(ch, SPELL_HEAL, 0, number(1, GET_MAG(ch)/100), NULL, ch);
      else if (!(affected_by_spell(ch, SPELL_ARMOUR) || affected_by_spell(ch, SPELL_INVIS) || affected_by_spell(ch, SPELL_IMP_INVIS) 
          || affected_by_spell(ch, SPELL_INCATTR) || affected_by_spell(ch, SPELL_COMBATSENSE)))
        switch (number(0, 100)) {
          case 1:
            cast_detection_spell(ch, SPELL_COMBATSENSE, number(1, GET_MAG(ch)/100), NULL, ch);
            break;
          case 2:
            cast_health_spell(ch, SPELL_INCATTR, STR, number(1, GET_MAG(ch)/100), NULL, ch);
            break;
          case 3:
            cast_health_spell(ch, SPELL_INCATTR, QUI, number(1, GET_MAG(ch)/100), NULL, ch);
            break;
          case 4:
            cast_health_spell(ch, SPELL_INCATTR, BOD, number(1, GET_MAG(ch)/100), NULL, ch);
            break;
          case 5:
            cast_illusion_spell(ch, SPELL_INVIS, number(1, GET_MAG(ch)/100), NULL, ch);
            break;
          case 6:
            cast_illusion_spell(ch, SPELL_IMP_INVIS, number(1, GET_MAG(ch)/100), NULL, ch);
            break;
          case 7:
            cast_manipulation_spell(ch, SPELL_ARMOUR, number(1, GET_MAG(ch)/100), NULL, ch);
            break;
        }
    }
    /* Add new mobile actions here */

  }                             /* end for() */
}

int violates_zsp(int security, struct char_data *ch, int pos, struct char_data *mob)
{
  struct obj_data *obj = GET_EQ(ch, pos);
  int i = 0;

  if (!obj)
    return 0;

  if ((pos == WEAR_HOLD || pos == WEAR_WIELD) && GET_OBJ_TYPE(obj) == ITEM_WEAPON)
    return 1;
  if (!GET_LEGAL_NUM(obj))
    return 0;

  switch (pos) {
  case WEAR_UNDER:
    if (GET_EQ(ch, WEAR_ABOUT) || GET_EQ(ch, WEAR_BODY))
      if (success_test(GET_INT(mob), 6 +
                       (GET_EQ(ch, WEAR_ABOUT) ? GET_OBJ_VAL(GET_EQ(ch, WEAR_ABOUT), 7) : 0) +
                       (GET_EQ(ch, WEAR_BODY) ? GET_OBJ_VAL(GET_EQ(ch, WEAR_BODY), 7) : 0)) < 2)
        return 0;
    break;
  case WEAR_LARM:
  case WEAR_RARM:
  case WEAR_WAIST:
  case WEAR_BODY:
    if (GET_EQ(ch, WEAR_ABOUT))
      if (success_test(GET_INT(mob), 4 + GET_OBJ_VAL(GET_EQ(ch, WEAR_ABOUT), 7)) < 2)
        return 0;
    break;
  case WEAR_LEGS:
    if (GET_EQ(ch, WEAR_ABOUT))
      if (success_test(GET_INT(mob), 2 + GET_OBJ_VAL(GET_EQ(ch, WEAR_ABOUT), 7)) < 2)
        return 0;
    break;
  case WEAR_LANKLE:
  case WEAR_RANKLE:
    if (GET_EQ(ch, WEAR_ABOUT) || GET_EQ(ch, WEAR_LEGS))
      if (success_test(GET_INT(mob), 5 +
                       (GET_EQ(ch, WEAR_ABOUT) ? GET_OBJ_VAL(GET_EQ(ch, WEAR_ABOUT), 7) : 0) +
                       (GET_EQ(ch, WEAR_LEGS) ? GET_OBJ_VAL(GET_EQ(ch, WEAR_LEGS), 7) : 0)) < 2)
        return 0;
    break;
  }

  if (GET_OBJ_TYPE(obj) == ITEM_HOLSTER && obj->contains)
    obj = obj->contains;

  int skill = get_skill(mob, SKILL_POLICE_PROCEDURES, i);
  if (success_test(skill, GET_LEGAL_NUM(obj)))
    return 1;
  else
    return 0;
}

/* Mob Memory Routines */

/* make ch remember victim */
void remember(struct char_data * ch, struct char_data * victim)
{
  memory_rec *tmp;
  bool present = FALSE;

  if (!IS_NPC(ch))
    return;

  for (tmp = MEMORY(ch); tmp && !present; tmp = tmp->next)
    if (tmp->id == GET_IDNUM(victim))
      present = TRUE;

  if (!present)
  {
    tmp = new memory_rec;
    tmp->next = MEMORY(ch);
    tmp->id = GET_IDNUM(victim);
    MEMORY(ch) = tmp;
  }
}

/* make ch forget victim */
void forget(struct char_data * ch, struct char_data * victim)
{
  memory_rec *curr, *prev = NULL;

  if (!(curr = MEMORY(ch)))
    return;

  while (curr && curr->id != GET_IDNUM(victim))
  {
    prev = curr;
    curr = curr->next;
  }

  if (!curr)
    return;                     /* person wasn't there at all. */

  if (curr == MEMORY(ch))
    MEMORY(ch) = curr->next;
  else
    prev->next = curr->next;

  delete curr;
}

/* check ch's memory for victim */
bool memory(struct char_data *ch, struct char_data *vict)
{
  memory_rec *names;

  for (names = MEMORY(ch); names; names = names->next)
    if (names->id == GET_IDNUM(vict))
      return(TRUE);

  return(FALSE);
}

/* erase ch's memory */
void clearMemory(struct char_data * ch)
{
  memory_rec *curr, *next;

  curr = MEMORY(ch);

  while (curr)
  {
    next = curr->next;
    delete curr;
    curr = next;
  }

  MEMORY(ch) = NULL;
}
bool attempt_reload(struct char_data *mob, int pos)
{
  // I would call the reload routine for players, but this is slightly faster
  struct obj_data *magazine, *gun = NULL;
  bool found = FALSE;

  if (!(gun = GET_EQ(mob, pos)) || GET_OBJ_TYPE(GET_EQ(mob, pos)) != ITEM_WEAPON)
    return FALSE;

  for (magazine = mob->carrying; magazine; magazine = magazine->next_content)
  {
    if (GET_OBJ_TYPE(magazine) == ITEM_GUN_MAGAZINE &&
        !GET_OBJ_VAL(magazine, 0)) {
      GET_OBJ_VAL(magazine, 9) = GET_OBJ_VAL(magazine, 0) = GET_OBJ_VAL(gun, 5);
      GET_OBJ_VAL(magazine, 1) = GET_OBJ_VAL(gun, 3);
      sprintf(buf, "a %d-round %s magazine", GET_OBJ_VAL(magazine, 0), weapon_type[GET_OBJ_VAL(magazine, 1)]);
      if (magazine->restring)
        delete [] magazine->restring;
      magazine->restring = strdup(buf);
      found = TRUE;
      break;      
    } else if (GET_OBJ_TYPE(magazine) == ITEM_GUN_MAGAZINE &&
        GET_OBJ_VAL(magazine, 0) == GET_OBJ_VAL(gun, 5) &&
        GET_OBJ_VAL(magazine, 1) == (GET_OBJ_VAL(gun, 3))) {
      found = TRUE;
      break;
    } 

  }

  if (!found)
    return FALSE;

  if (gun->contains)
  {
    struct obj_data *tempobj = gun->contains;
    obj_from_obj(tempobj);
    obj_to_room(tempobj, mob->in_room);
  }
  obj_from_char(magazine);
  obj_to_obj(magazine, gun);
  GET_OBJ_VAL(gun, 6) = GET_OBJ_VAL(magazine, 9);

  act("$n reloads $p.", TRUE, mob, gun, 0, TO_ROOM);
  act("You reload $p.", FALSE, mob, gun, 0, TO_CHAR);
  return TRUE;
}

void switch_weapons(struct char_data *mob, int pos)
{
  struct obj_data *i, *temp = NULL, *temp2 = NULL;

  if (!GET_EQ(mob, pos) || GET_OBJ_TYPE(GET_EQ(mob, pos)) != ITEM_WEAPON)
    return;

  for (i = mob->carrying; i && !temp; i = i->next_content)
  {
    // search for a new gun
    if (GET_OBJ_TYPE(i) == ITEM_WEAPON)
      if (GET_OBJ_VAL(i, 6) > 0)
        temp = i;
    if (GET_OBJ_TYPE(i) == ITEM_WEAPON)
      if (GET_OBJ_VAL(i, 5) == -1)
        temp2 = i;
  }

  perform_remove(mob, pos);

  if (temp)
    perform_wear(mob, temp, pos);
  else if (temp2)
    perform_wear(mob, temp2, pos);
}

