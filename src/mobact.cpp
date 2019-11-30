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
#include "act.drive.h"

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

void mobact_change_firemode(struct char_data *ch) {
  struct obj_data *weapon;
  
  // Precheck: Weapon must exist and must be a gun.
  if (!(weapon = GET_EQ(ch, WEAR_WIELD)) || !IS_GUN(GET_OBJ_VAL(weapon, 3)))
    return;
  
  // Set up info.
  int prev_value = GET_OBJ_VAL(weapon, 11);
  int mode_count = 0;
  
  // Reload the weapon if possible.
  if (!weapon->contains)
    attempt_reload(ch, WEAR_WIELD);
  
  // Lowest-priority mode is single-shot.
  if (IS_SET(GET_OBJ_VAL(weapon, 10), 1 << MODE_SS)) {
    mode_count += 1;
    GET_OBJ_VAL(weapon, 11) = MODE_SS;
  }
  
  // Semi-automatic is superior to single shot, so set that if we can.
  if (IS_SET(GET_OBJ_VAL(weapon, 10), 1 << MODE_SA)) {
    mode_count += 1;
    GET_OBJ_VAL(weapon, 11) = MODE_SA;
  }
  
  // Full automatic with a 10-round spray deals more damage than the others.
  if (IS_SET(GET_OBJ_VAL(weapon, 10), 1 << MODE_FA)) {
    mode_count += 1;
    GET_OBJ_VAL(weapon, 11) = MODE_FA;
    
    // Set the rounds-to-fire count to 10.
    GET_OBJ_TIMER(weapon) = 10;
  }
  
  // For some reason, NPCs were set to prefer burst fire above all other modes. Preserving this logic for now.
  if (IS_SET(GET_OBJ_VAL(weapon, 10), 1 << MODE_BF)) {
    mode_count += 1;
    GET_OBJ_VAL(weapon, 11) = MODE_BF;
  }
  
  // Send a message to the room, but only if the weapon has received a new fire selection mode and has more than one available.
  if (prev_value != GET_OBJ_VAL(weapon, 11) && mode_count > 1) {
    act("$n flicks the fire selector switch on $p.", TRUE, ch, weapon, 0, TO_ROOM);
  }
}

// Check if an NPC successfully fires a spec-proc. Returns TRUE if so, FALSE otherwise.
bool mobact_evaluate_spec_proc(struct char_data *ch) {
  char empty = '\0';
  if (mob_index[GET_MOB_RNUM(ch)].func == NULL) {
    log_vfprintf("%s (#%d): Attempting to call non-existing mob func",
                 GET_NAME(ch), GET_MOB_VNUM(ch));
    
    MOB_FLAGS(ch).RemoveBit(MOB_SPEC);
  }
  
  else if ((mob_index[GET_MOB_RNUM(ch)].func) (ch, ch, 0, &empty)) {
    return true;
  }
  
  else if (mob_index[GET_MOB_RNUM(ch)].sfunc != NULL) {
    if ((mob_index[GET_MOB_RNUM(ch)].sfunc) (ch, ch, 0, &empty)) {
      return true;
    }
  }
  
  return false;
}

bool mobact_process_in_vehicle_guard(struct char_data *ch) {
  struct veh_data *tveh = NULL;
  struct char_data *vict = NULL;
  
  // Precondition: Vehicle must exist, we must be manning or driving, and we must not be astral.
  if (!ch->in_veh || !(AFF_FLAGGED(ch, AFF_PILOT) || AFF_FLAGGED(ch, AFF_MANNING)) || IS_ASTRAL(ch))
    return FALSE;
  
  // Precondition: If our vehicle is nested, just give up.
  if (ch->in_veh->in_veh)
    return FALSE;
  
  // Peaceful room, or I'm not actually a guard? Bail out.
  if (ROOM_FLAGGED(ch->in_veh->in_room, ROOM_PEACEFUL) || !(MOB_FLAGGED(ch, MOB_GUARD)))
    return FALSE;
  
  /* Guard NPCs. */
      
  // If we're not in a road or garage, we expect to see no vehicles and will attack any that we see.
  if (!(ROOM_FLAGGED(ch->in_veh->in_room, ROOM_ROAD) || ROOM_FLAGGED(ch->in_veh->in_room, ROOM_GARAGE))) {
    for (tveh = ch->in_veh->in_room->vehicles; tveh; tveh = tveh->next_veh) {
      // No attacking your own vehicle.
      if (tveh == ch->in_veh)
        continue;
      
      // TODO: Only attack player-owned vehicles and vehicles that have player occupants or drivers.
      if (tveh->damage < 10 && tveh->owner > 0) {
        // Found a target, stop processing vehicles.
        break;
      }
    }
  }
  
  if (!tveh) {
    // No vehicular targets? Check players.
    for (vict = ch->in_veh->in_room->people; vict; vict = vict->next_in_room) {
      // Skip over invalid targets (NPCs, no-hassle imms, invisibles, and downed).
      if (IS_NPC(vict) || PRF_FLAGGED(vict, PRF_NOHASSLE) || !CAN_SEE_ROOM_SPECIFIED(ch, vict, ch->in_veh->in_room) || GET_PHYSICAL(vict) <= 0)
        continue;
      
      for (int i = 0; i < NUM_WEARS; i++) {
        // If victim's equipment is illegal here, blast them.
        if (GET_EQ(vict, i) && violates_zsp(GET_SECURITY_LEVEL(ch->in_veh->in_room), vict, i, ch)) {
          // Target found, stop processing.
          break;
        }
      }
    }
  }
  
  // No targets? Bail out.
  if (!tveh && !vict)
    return FALSE;
  
  sprintf(buf, "guard %s: ch = %s, tveh = %s, vict = %s", AFF_FLAGGED(ch, AFF_PILOT) ? "driver" : "gunner",
          GET_NAME(ch), tveh ? tveh->name : "none", vict ? GET_NAME(vict) : "none");
  mudlog(buf, ch, LOG_SYSLOG, TRUE);
  
  // Driver? It's rammin' time.
  if (AFF_FLAGGED(ch, AFF_PILOT)) {
    do_raw_ram(ch, ch->in_veh, tveh, vict);
    
    return TRUE;
  }
  
  // Dakka o'clock.
  else if (AFF_FLAGGED(ch, AFF_MANNING)) {
    struct obj_data *mount = get_mount_manned_by_ch(ch);
    if (mount && mount_has_weapon(mount)) {
      do_raw_target(ch, ch->in_veh, tveh, vict, FALSE, mount);
      return TRUE;
    }
    
    mudlog("SYSERR: Could not find mount manned by NPC for mobact_process_in_vehicle_aggro().", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  return FALSE;
}

bool mobact_process_in_vehicle_aggro(struct char_data *ch) {
  struct veh_data *tveh = NULL;
  struct char_data *vict = NULL;
  
  // Precondition: Vehicle must exist, we must be manning or driving, and we must not be astral.
  if (!ch->in_veh || !(AFF_FLAGGED(ch, AFF_PILOT) || AFF_FLAGGED(ch, AFF_MANNING)) || IS_ASTRAL(ch))
    return FALSE;
  
  // Precondition: If our vehicle is nested, just give up.
  if (ch->in_veh->in_veh)
    return FALSE;
  
  // Peaceful room, or I'm not actually aggro or alarmed? Bail out.
  if (ROOM_FLAGGED(ch->in_veh->in_room, ROOM_PEACEFUL) ||
      !(MOB_FLAGS(ch).AreAnySet(MOB_AGGRESSIVE, MOB_AGGR_TO_RACE, ENDBIT) || GET_MOBALERT(ch) == MALERT_ALARM))
    return FALSE;
  
  // Target selection. We disallow targeting of unowned vehicles so our guards don't Thunderdome each other before players even show up.
  for (tveh = ch->in_veh->in_room->vehicles; tveh; tveh = tveh->next_veh) {
    if (tveh != ch->in_veh && tveh->damage < 10 && tveh->owner > 0) {
      // Found a valid target, stop looking.
      break;
    }
  }
  
  // Select a non-vehicle target.
  if (!tveh) {
    // If we've gotten here, character is either astral or is not willing to / failed to attack a vehicle.
    for (vict = ch->in_veh->in_room->people; vict; vict = vict->next_in_room) {
      // Skip conditions: Invisible, no-hassle, already downed, or is an NPC who is neither a player's astral body nor a player's escortee.
      if ((IS_NPC(vict) && !IS_PROJECT(vict) && !is_escortee(vict)) || !CAN_SEE_ROOM_SPECIFIED(ch, vict, ch->in_veh->in_room) || PRF_FLAGGED(vict, PRF_NOHASSLE) || GET_PHYSICAL(vict) <= 0)
        continue;
      
      // Attack the escortee if we're hunting it specifically.
      if (hunting_escortee(ch, vict)) {
        break;
      }
      
      // If NPC is aggro or alarmed, or...
      if (!MOB_FLAGS(ch).AreAnySet(MOB_AGGR_TO_RACE, ENDBIT) ||
          // If NPC is aggro towards elves, and victim is an elf subrace, or...
          (MOB_FLAGGED(ch, MOB_AGGR_ELF) &&
           (GET_RACE(vict) == RACE_ELF || GET_RACE(vict) == RACE_WAKYAMBI || GET_RACE(vict) == RACE_NIGHTONE || GET_RACE(vict) == RACE_DRYAD)) ||
          // If NPC is aggro towards dwarves, and victim is a dwarf subrace, or...
          (MOB_FLAGGED(ch, MOB_AGGR_DWARF) &&
           (GET_RACE(vict) == RACE_DWARF || GET_RACE(vict) == RACE_GNOME || GET_RACE(vict) == RACE_MENEHUNE || GET_RACE(vict) == RACE_KOBOROKURU)) ||
          // If NPC is aggro towards humans, and victim is human, or...
          (MOB_FLAGGED(ch, MOB_AGGR_HUMAN) &&
           GET_RACE(vict) == RACE_HUMAN) ||
          // If NPC is aggro towards orks, and victim is an ork subrace, or...
          (MOB_FLAGGED(ch, MOB_AGGR_ORK) &&
           (GET_RACE(vict) == RACE_ORK || GET_RACE(vict) == RACE_HOBGOBLIN || GET_RACE(vict) == RACE_OGRE || GET_RACE(vict) == RACE_SATYR || GET_RACE(vict) == RACE_ONI)) ||
          // If NPC is aggro towards trolls, and victim is a troll subrace:
          (MOB_FLAGGED(ch, MOB_AGGR_TROLL) &&
           (GET_RACE(vict) == RACE_TROLL || GET_RACE(vict) == RACE_CYCLOPS || GET_RACE(vict) == RACE_FOMORI || GET_RACE(vict) == RACE_GIANT || GET_RACE(vict) == RACE_MINOTAUR)))
        // Kick their ass.
      {
        break;
      }
    }
  }
  
  if (!tveh && !vict)
    return FALSE;
  
  sprintf(buf, "aggro %s: ch = %s, tveh = %s, vict = %s", AFF_FLAGGED(ch, AFF_PILOT) ? "driver" : "gunner",
          GET_NAME(ch), tveh ? tveh->name : "none", vict ? GET_NAME(vict) : "none");
  mudlog(buf, ch, LOG_SYSLOG, TRUE);
  
  // Driver? It's rammin' time.
  if (AFF_FLAGGED(ch, AFF_PILOT)) {
    do_raw_ram(ch, ch->in_veh, tveh, vict);
    
    return TRUE;
  }
  
  // Dakka o'clock.
  else if (AFF_FLAGGED(ch, AFF_MANNING)) {
    struct obj_data *mount = get_mount_manned_by_ch(ch);
    if (mount && mount_has_weapon(mount)) {
      do_raw_target(ch, ch->in_veh, tveh, vict, FALSE, mount);
      return TRUE;
    }
    
    mudlog("SYSERR: Could not find mount manned by NPC for mobact_process_in_vehicle_aggro().", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  return FALSE;
}

// TODO: Fix alarmed NPCs attacking non-hostile vehicles.
bool mobact_process_aggro(struct char_data *ch, struct room_data *room) {
  struct char_data *vict = NULL;
  struct veh_data *veh = NULL;
  
  // Vehicle code is separate.
  if (ch->in_veh && ch->in_veh->in_room->number == room->number)
    return mobact_process_in_vehicle_aggro(ch);
 
  
  if (!ROOM_FLAGGED(room, ROOM_PEACEFUL) &&
      (MOB_FLAGS(ch).AreAnySet(MOB_AGGRESSIVE, MOB_AGGR_TO_RACE, ENDBIT) || GET_MOBALERT(ch) == MALERT_ALARM)) {
  
    // If I am not astral, am in the same room, and am willing to attack a vehicle this round (coin flip), pick a fight with a vehicle.
    if (ch->in_room->number == room->number && !IS_ASTRAL(ch) && number(0, 1)) {
      for (veh = room->vehicles; veh; veh = veh->next_veh) {
        if (veh->damage < 10) {
          stop_fighting(ch);
          set_fighting(ch, veh);
          return true;
        }
      }
    }
  
    // If we've gotten here, character is either astral or is not willing to / failed to attack a vehicle.
    for (vict = room->people; vict; vict = vict->next_in_room) {
      // Skip conditions: Invisible, no-hassle, already downed, or is an NPC who is neither a player's astral body nor a player's escortee.
      if ((IS_NPC(vict) && !IS_PROJECT(vict) && !is_escortee(vict)) || !CAN_SEE(ch, vict) || PRF_FLAGGED(vict, PRF_NOHASSLE) || GET_PHYSICAL(vict) <= 0)
        continue;
      
      // Attack the escortee if we're hunting it specifically.
      if (hunting_escortee(ch, vict)) {
        stop_fighting(ch);
        set_fighting(ch, vict);
        return true;
      }
      
      // If NPC is aggro or alarmed, or...
      if (!MOB_FLAGS(ch).AreAnySet(MOB_AGGR_TO_RACE, ENDBIT) ||
          // If NPC is aggro towards elves, and victim is an elf subrace, or...
          (MOB_FLAGGED(ch, MOB_AGGR_ELF) &&
           (GET_RACE(vict) == RACE_ELF || GET_RACE(vict) == RACE_WAKYAMBI || GET_RACE(vict) == RACE_NIGHTONE || GET_RACE(vict) == RACE_DRYAD)) ||
          // If NPC is aggro towards dwarves, and victim is a dwarf subrace, or...
          (MOB_FLAGGED(ch, MOB_AGGR_DWARF) &&
           (GET_RACE(vict) == RACE_DWARF || GET_RACE(vict) == RACE_GNOME || GET_RACE(vict) == RACE_MENEHUNE || GET_RACE(vict) == RACE_KOBOROKURU)) ||
          // If NPC is aggro towards humans, and victim is human, or...
          (MOB_FLAGGED(ch, MOB_AGGR_HUMAN) &&
           GET_RACE(vict) == RACE_HUMAN) ||
          // If NPC is aggro towards orks, and victim is an ork subrace, or...
          (MOB_FLAGGED(ch, MOB_AGGR_ORK) &&
           (GET_RACE(vict) == RACE_ORK || GET_RACE(vict) == RACE_HOBGOBLIN || GET_RACE(vict) == RACE_OGRE || GET_RACE(vict) == RACE_SATYR || GET_RACE(vict) == RACE_ONI)) ||
          // If NPC is aggro towards trolls, and victim is a troll subrace:
          (MOB_FLAGGED(ch, MOB_AGGR_TROLL) &&
           (GET_RACE(vict) == RACE_TROLL || GET_RACE(vict) == RACE_CYCLOPS || GET_RACE(vict) == RACE_FOMORI || GET_RACE(vict) == RACE_GIANT || GET_RACE(vict) == RACE_MINOTAUR)))
        // Kick their ass.
      {
        stop_fighting(ch);
        set_fighting(ch, vict);
        return true;
      }
    }
  }
  return false;
}

bool mobact_process_memory(struct char_data *ch, struct room_data *room) {
  struct char_data *vict = NULL;
  
  /* Mob Memory */
  if (MOB_FLAGGED(ch, MOB_MEMORY) && MEMORY(ch)) {
    for (vict = room->people; vict; vict = vict->next_in_room) {
      // Skip NPCs, invisibles, and nohassle targets.
      if (IS_NPC(vict) || !CAN_SEE(ch, vict) || PRF_FLAGGED(vict, PRF_NOHASSLE))
        continue;
      
      // If we remember them, fuck 'em up.
      if (memory(ch, vict)) {
        act("'Hey! You're the fragger that attacked me!!!', exclaims $n.", FALSE, ch, 0, 0, TO_ROOM);
        stop_fighting(ch);
        set_fighting(ch, vict);
        return true;
      }
    }
  }
  return false;
}

bool mobact_process_helper(struct char_data *ch) {
  struct char_data *vict = NULL;
  
  /* Helper Mobs */
  if (MOB_FLAGGED(ch, MOB_HELPER)) {
    for (vict = ch->in_room->people; vict; vict = vict->next_in_room) {
      // Ensure we're neither of the fighting parties. This check should be redundant since no fighting NPC can proceed through mobile_activity().
      if (ch == vict || !FIGHTING(vict) || ch == FIGHTING(vict))
        continue;
      
      // If victim is an NPC who is fighting a player, and I can see the player, assist the NPC.
      if (IS_NPC(vict) && !IS_NPC(FIGHTING(vict)) && CAN_SEE(ch, FIGHTING(vict))) {
        // The player is in my room, so I can fight them up-close.
        if (FIGHTING(vict)->in_room == ch->in_room) {
          act("$n jumps to the aid of $N!", FALSE, ch, 0, vict, TO_ROOM);
          stop_fighting(ch);
          
          // Close-ranged response.
          set_fighting(ch, FIGHTING(vict));
        }
        
        // The player is not in my room, so I have to do a ranged response.
        else {
          // Long-ranged response.
          if (ranged_response(FIGHTING(vict), ch)) {
            // TODO: This doesn't fire a message if the NPC is wielding a ranged weapon.
            act("$n jumps to $N's aid against $S distant attacker!",
                FALSE, ch, 0, vict, TO_ROOM);
          }
        }
        
        return true;
      }
      
      // If the victim is a player who is fighting an NPC, and I can see the player, assist the NPC.
      if (!IS_NPC(vict) && IS_NPC(FIGHTING(vict)) && CAN_SEE(ch, vict)) {
        // A player in my area is attacking an NPC.
        act("$n jumps to the aid of $N!", FALSE, ch, 0, FIGHTING(vict), TO_ROOM);
        stop_fighting(ch);
        
        // Close-ranged response.
        set_fighting(ch, vict);
        
        return true;
      }
    }
  }
  return false;
}

bool mobact_process_guard(struct char_data *ch, struct room_data *room) {
  struct char_data *vict = NULL;
  struct veh_data *veh = NULL;
  
  // Vehicle code is separate.
  if (ch->in_veh && ch->in_veh->in_room == room)
    return mobact_process_in_vehicle_guard(ch);
  
  int i = 0;
  
  /* Guard NPCs. */
  if (MOB_FLAGGED(ch, MOB_GUARD)) {
    // Check vehicles, but only if they're in the same room as the guard.
    if (ch->in_room == room) {
      for (veh = room->vehicles; veh; veh = veh->next_veh) {
        // If the room we're in is neither a road nor a garage, attack any vehicles we see.
        // NOTE: Previous logic required that the vehicle be damaged to be a valid attack target.
        if (!(ROOM_FLAGGED(room, ROOM_ROAD) || ROOM_FLAGGED(room, ROOM_GARAGE))) {
          // TODO: Only attack player-owned vehicles and vehicles that have player occupants or drivers.
          if (veh->damage < 10 && veh->owner > 0) {
            stop_fighting(ch);
            set_fighting(ch, veh);
            return true;
          }
        }
      }
    }
    
    // Check players.
    for (vict = room->people; vict; vict = vict->next_in_room) {
      // Skip over invalid targets (NPCs, no-hassle imms, invisibles, and downed).
      if (IS_NPC(vict) || PRF_FLAGGED(vict, PRF_NOHASSLE) || !CAN_SEE(ch, vict) || GET_PHYSICAL(vict) <= 0)
        continue;
      
      for (i = 0; i < NUM_WEARS; i++) {
        // If victim's equipment is illegal here, blast them.
        if (GET_EQ(vict, i) && violates_zsp(GET_SECURITY_LEVEL(room), vict, i, ch)) {
          stop_fighting(ch);
          set_fighting(ch, vict);
          return true;
        }
      }
    }
  } /* End Guard NPC section. */
  return false;
}

bool mobact_process_self_buff(struct char_data *ch) {
  if (GET_SKILL(ch, SKILL_SORCERY) && !MOB_FLAGGED(ch, MOB_SPEC)) {
    
    // Always self-heal if able.
    if (GET_PHYSICAL(ch) < GET_MAX_PHYSICAL(ch) && !AFF_FLAGGED(ch, AFF_HEALED))
      cast_health_spell(ch, SPELL_HEAL, 0, number(1, GET_MAG(ch)/100), NULL, ch);
    
    // Buff self, but only act one out of every 16 ticks (on average).
    if (number(0, 15) == 0) {
      // Apply armor to self.
      if (!affected_by_spell(ch, SPELL_ARMOUR)) {
        cast_manipulation_spell(ch, SPELL_ARMOUR, number(1, GET_MAG(ch)/100), NULL, ch);
      }
      
      // If not invisible already, apply an invisibility spell based on my magic rating and sorcery skill.
      else if (!affected_by_spell(ch, SPELL_INVIS) && !affected_by_spell(ch, SPELL_IMP_INVIS)) {
        if (MIN(GET_SKILL(ch, SKILL_SORCERY), GET_MAG(ch)/100) <= 5) {
          // Lower skill means standard invisibility. Gotta make thermographic vision useful somehow.
          cast_illusion_spell(ch, SPELL_INVIS, number(1, GET_MAG(ch)/100), NULL, ch);
        } else {
          // Look out, we've got a badass over here.
          cast_illusion_spell(ch, SPELL_IMP_INVIS, number(1, GET_MAG(ch)/100), NULL, ch);
        }
      }
      
      // Apply combat sense to self.
      else if (!affected_by_spell(ch, SPELL_COMBATSENSE)) {
        cast_detection_spell(ch, SPELL_COMBATSENSE, number(1, GET_MAG(ch)/100), NULL, ch);
      }
      
      // We're dead-set on casting a spell, so try to boost attributes.
      else {
        switch (number(1, 3)) {
          case 1:
            cast_health_spell(ch, SPELL_INCATTR, STR, number(1, GET_MAG(ch)/100), NULL, ch);
            break;
          case 2:
            cast_health_spell(ch, SPELL_INCATTR, QUI, number(1, GET_MAG(ch)/100), NULL, ch);
            break;
          case 3:
            cast_health_spell(ch, SPELL_INCATTR, BOD, number(1, GET_MAG(ch)/100), NULL, ch);
            break;
        }
      }
      
      // We've spent our action casting a spell, so time to stop acting.
      return true;
    }
  } /* End spellcasting NPCs */
  return false;
}

bool mobact_process_scavenger(struct char_data *ch) {
  /* Scavenger (picking up objects) */
  if (MOB_FLAGGED(ch, MOB_SCAVENGER)) {
    if (ch->in_room->contents && !number(0, 10)) {
      struct obj_data *obj, *best_obj = NULL;
      int max = 1;
      
      // Find the most valuable object in the room (ignoring worthless things):
      for (obj = ch->in_room->contents; obj; obj = obj->next_content) {
        if (CAN_GET_OBJ(ch, obj) && GET_OBJ_COST(obj) > max && GET_OBJ_TYPE(obj) != ITEM_WORKSHOP) {
          best_obj = obj;
          max = GET_OBJ_COST(obj);
        }
      }
      
      // Get the most valuable thing we've found.
      if (best_obj != NULL) {
        obj_from_room(best_obj);
        obj_to_char(best_obj, ch);
        act("$n gets $p.", FALSE, ch, best_obj, 0, TO_ROOM);
        return true;
      }
    }
  } /* End scavenger NPC. */
  return false;
}

bool mobact_process_movement(struct char_data *ch) {
  int door;
  
  if (MOB_FLAGGED(ch, MOB_SENTINEL) || CH_IN_COMBAT(ch))
    return FALSE;
  
  if (GET_POS(ch) != POS_STANDING && !AFF_FLAGGED(ch, AFF_PILOT))
    return FALSE;
  
  // Slow down movement-- this way they'll only try to move every other tick on average.
  if (number(0, 1) == 0)
    return FALSE;
    
  // NPC in a vehicle that's not in another vehicle? Have them drive and obey the rules of the road.
  if (ch->in_veh) {
    // Give up if our vehicle is nested.
    if (ch->in_veh->in_veh)
      return FALSE;
    
    // Passengers are passive.
    if (!AFF_FLAGGED(ch, AFF_PILOT))
      return FALSE;
    
    // Pick a door. If it's a valid exit for this vehicle, vroom vroom away.
    for (int tries = 0; tries < 5; tries++) {
      door = number(NORTH, DOWN);
      if (EXIT(ch->in_veh, door) &&
          ROOM_FLAGS(EXIT(ch->in_veh, door)->to_room).AreAnySet(ROOM_ROAD, ROOM_GARAGE, ENDBIT) &&
          !ROOM_FLAGS(EXIT(ch->in_veh, door)->to_room).AreAnySet(ROOM_NOMOB, ROOM_DEATH, ENDBIT)) {
        sprintf(buf3, "Driver %s attempted to move %s from %s [%ld].", GET_NAME(ch), fulldirs[door], GET_ROOM_NAME(ch->in_veh->in_room), GET_ROOM_VNUM(ch->in_veh->in_room));
        perform_move(ch, door, LEADER, NULL);
        sprintf(ENDOF(buf3), "  After move, driver now in %s [%ld].", GET_ROOM_NAME(ch->in_veh->in_room), GET_ROOM_VNUM(ch->in_veh->in_room));
        mudlog(buf3, ch, LOG_SYSLOG, TRUE);
        return TRUE;
      }
    }
  } /* End NPC movement in vehicle. */
  
  // NPC not in a vehicle (walking).
  else {
    // Skip NOWHERE-located NPCs since they'll break things.
    if (!ch->in_room)
      return FALSE;
    
    for (int tries = 0; tries < 5; tries++) {
      // Select an exit, and bail out if they can't move that way.
      door = number(NORTH, DOWN);
      if (!CAN_GO(ch, door) || ROOM_FLAGS(EXIT(ch, door)->to_room).AreAnySet(ROOM_NOMOB, ROOM_DEATH, ENDBIT))
        continue;
      
      // If their exit leads to a different zone, check if they're allowed to wander.
      if (MOB_FLAGGED(ch, MOB_STAY_ZONE) && (EXIT(ch, door)->to_room->zone != ch->in_room->zone))
        continue;
      
      // Looks like they can move. Make it happen.
      perform_move(ch, door, CHECK_SPECIAL | LEADER, NULL);
      return TRUE;
    }
  } /* End NPC movement outside a vehicle. */
  return false;
}

void mobile_activity(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct char_data *ch, *next_ch;
  int dir, distance;
  struct room_data *current_room = NULL;

  extern int no_specials;
  
  ACMD_DECLARE(do_get);

  // Iterate through all characters in the game.
  for (ch = character_list; ch; ch = next_ch) {
    next_ch = ch->next;
    
    // Skip them if they're a player character, are being possessed, are sleeping, or have no current room.
    if (!IS_MOB(ch) || !AWAKE(ch) || ch->desc || !get_ch_in_room(ch))
      continue;

    // Skip NPCs that are currently fighting someone in their room, or are fighting a vehicle.
    if ((FIGHTING(ch)
          && (FIGHTING(ch)->in_room == ch->in_room
            || (ch->in_veh && FIGHTING(ch)->in_room == ch->in_veh->in_room)))
        || FIGHTING_VEH(ch))
      continue;

    // Cool down mob alert status.
    if (GET_MOBALERT(ch) > MALERT_CALM && --GET_MOBALERTTIME(ch) <= 0)
      GET_MOBALERT(ch) = MALERT_CALM;
    
    // Manipulate wielded weapon (reload, set fire mode, etc).
    mobact_change_firemode(ch);

    // Look for special procedures. If one fires successfully, do nothing else for this NPC.
    if (!no_specials && MOB_FLAGGED(ch, MOB_SPEC) && mobact_evaluate_spec_proc(ch)) {
      continue;
    }
    
    // Character in nested vehicle? Stop processing.
    if (ch->in_veh && ch->in_veh->in_veh)
      continue;
    
    // All these aggressive checks require the character to not be in a peaceful room.
    if (!ROOM_FLAGGED(get_ch_in_room(ch), ROOM_PEACEFUL)) {
      // Handle aggressive mobs.
      if (mobact_process_aggro(ch, get_ch_in_room(ch))) {
        continue;
      }
      
      // Guard NPCs.
      if (mobact_process_guard(ch, get_ch_in_room(ch))) {
        continue;
      }
      
      // These checks additionally require that the NPC is not in a vehicle.
      if (!ch->in_veh) {
        if (mobact_process_memory(ch, ch->in_room)) {
          continue;
        }
        
        if (mobact_process_helper(ch)) {
          continue;
        }
        
        // Sniper? Check surrounding players and re-apply above for each applicable room.
        if (MOB_FLAGGED(ch, MOB_SNIPER) && GET_EQ(ch, WEAR_WIELD)) {
          // When this is true, we'll stop evaluating sniper and continue to next NPC.
          bool has_acted = FALSE;
          
          // Calculate their maximum firing range (lesser of vision range and weapon range).
          int max_distance = MIN(find_sight(ch), find_weapon_range(ch, GET_EQ(ch, WEAR_WIELD)));
          
          for (dir = 0; !has_acted && !FIGHTING(ch) && dir < NUM_OF_DIRS; dir++) {
            current_room = ch->in_room;
            
            // Check each room in a straight line until we are either out of range or cannot go further.
            for (distance = 1; !has_acted && distance <= max_distance; distance++) {
              // Exit must be valid, and room must belong to same zone as character's room.
              if (CAN_GO2(current_room, dir) && EXIT2(current_room, dir)->to_room->zone == ch->in_room->zone) {
                current_room = EXIT2(current_room, dir)->to_room;
              } else {
                // If we can't get to a further room, stop and move to next direction in for loop.
                break;
              }
              
              // Aggro sniper.
              if ((has_acted = mobact_process_aggro(ch, current_room))) {
                break;
              }
              
              // Memory sniper.
              if ((has_acted = mobact_process_memory(ch, current_room))) {
                break;
              }
              
              // No such thing as a helper sniper.
              
              // Guard sniper.
              if ((has_acted = mobact_process_guard(ch, current_room))) {
                break;
              }
            }
          }
        }
      }
    } /* End NPC-is-not-in-a-vehicle section. */
    
    /**********************************************************************\
      If we've gotten here, the NPC has not acted and has no valid targets.
      Add passive things like self-buffing and idle actions here.
    \**********************************************************************/
    
    // Cast spells to buff self. NPC must have sorcery skill and must not have any special procedures attached to it.
    if (mobact_process_self_buff(ch)) {
      continue;
    }
    
    if (mobact_process_scavenger(ch)) {
      continue;
    }
    
    if (mobact_process_movement(ch)) {
      continue;
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
      DELETE_ARRAY_IF_EXTANT(magazine->restring);
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

