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

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "constants.hpp"
#include "act.drive.hpp"
#include "transport.hpp"
#include "quest.hpp"
#include "bullet_pants.hpp"
#include "bitfield.hpp"
#include "config.hpp"

ACMD_DECLARE(do_say);
ACMD_DECLARE(do_prone);
ACMD_DECLARE(do_reload);

// Note: If you want mobact debugging, add -DMOBACT_DEBUG to your makefile.
// #define MOBACT_DEBUG

/* external structs */
extern void resist_drain(struct char_data *ch, int power, int drain_add, int wound);
extern int find_sight(struct char_data *ch);
extern int find_weapon_range(struct char_data *ch, struct obj_data *weapon);

extern void cast_detection_spell(struct char_data *ch, int spell, int force, char *arg, char_data *mob);
extern void cast_manipulation_spell(struct char_data *ch, int spell, int force, char *arg, char_data *mob);
extern void cast_illusion_spell(struct char_data *ch, int spell, int force, char *arg, char_data *mob);
extern void cast_health_spell(struct char_data *ch, int spell, int sub, int force, char *arg, char_data *mob);
extern void end_sustained_spell(struct char_data *ch, struct sustain_data *sust);
extern bool can_hurt(struct char_data *ch, struct char_data *victim, int attacktype, bool include_func_protections);


extern void perform_wear(struct char_data *, struct obj_data *, int, bool);
extern void perform_remove(struct char_data *, int);

extern bool is_escortee(struct char_data *mob);
extern bool hunting_escortee(struct char_data *ch, struct char_data *vict);

extern bool ranged_response(struct char_data *ch, struct char_data *vict);
extern bool does_weapon_have_bayonet(struct obj_data *weapon);

SPECIAL(elevator_spec);
SPECIAL(call_elevator);
extern int num_elevators;
extern struct elevator_data *elevator;
extern int process_elevator(struct room_data *room, struct char_data *ch, int cmd, char *argument);

bool memory(struct char_data *ch, struct char_data *vict);
int violates_zsp(int security, struct char_data *ch, int pos, struct char_data *mob);
bool attempt_reload(struct char_data *mob, int pos);
bool vehicle_is_valid_mob_target(struct veh_data *veh, bool alarmed, int quest_id);
void switch_weapons(struct char_data *mob, int pos);
void send_mob_aggression_warnings(struct char_data *pc, struct char_data *mob);
bool mob_cannot_be_aggressive(struct char_data *ch);
bool mob_is_aggressive_towards_race(struct char_data *ch, int race);

// This takes up a significant amount of processing time, so let's precompute it.
#define NUM_AGGRO_OCTETS 3
int AGGRESSION_OCTETS[NUM_AGGRO_OCTETS];

char bitbuffer[100];
const char *print_bits(unsigned int bits)
{
  char *buf_ptr = bitbuffer;

  for (int i = BITFIELD_BITS_PER_VAR-1; i >= 0; i--)
    if (bits & ((unsigned int) 1 << i))
      *(buf_ptr++) = '1';
    else
      *(buf_ptr++) = '0';

  if (buf_ptr == bitbuffer) {
    // if no flags were set, just set it to "0":
    strcpy(bitbuffer, "0");
  } else
    *buf_ptr = '\0';

  return (const char *)bitbuffer;
}

#define ASSIGN_AGGRO_OCTET(bit_to_set) AGGRESSION_OCTETS[(int) ((bit_to_set) / BITFIELD_BITS_PER_VAR)] |= 1 << ((bit_to_set) % BITFIELD_BITS_PER_VAR)
void populate_mobact_aggression_octets() {
  for (int i = 0; i < NUM_AGGRO_OCTETS; i++)
    AGGRESSION_OCTETS[i] = 0;

  ASSIGN_AGGRO_OCTET(MOB_AGGRESSIVE);
  ASSIGN_AGGRO_OCTET(MOB_AGGR_ELF);
  ASSIGN_AGGRO_OCTET(MOB_AGGR_DWARF);
  ASSIGN_AGGRO_OCTET(MOB_AGGR_ORK);
  ASSIGN_AGGRO_OCTET(MOB_AGGR_TROLL);
  ASSIGN_AGGRO_OCTET(MOB_AGGR_HUMAN);
#ifdef MOBACT_DEBUG
  strncpy(buf, "Static mobact aggr info:", sizeof(buf) - 1);
  for (int i = 0; i < NUM_AGGRO_OCTETS; i++)
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %d=%s (%d)", i, print_bits(AGGRESSION_OCTETS[i]), AGGRESSION_OCTETS[i]);
  log(buf);

  int expected_output = 1 << MOB_AGGRESSIVE | 1 << MOB_AGGR_ELF | 1 << MOB_AGGR_DWARF | 1 << MOB_AGGR_ORK | 1 << MOB_AGGR_TROLL | 1 << MOB_AGGR_HUMAN;
  log_vfprintf("Expected output: %s (%d)", print_bits(expected_output), expected_output);
#endif
}
#undef ASSIGN_AGGRO_OCTET

bool mob_is_aggressive(struct char_data *ch, bool include_base_aggression) {
#ifdef MOBACT_DEBUG
  snprintf(buf2, sizeof(buf2), "According to old logic, $n should be %s.",
           MOB_FLAGS(ch).AreAnySet(MOB_AGGRESSIVE, MOB_AGGR_ELF, MOB_AGGR_DWARF, MOB_AGGR_ORK, MOB_AGGR_TROLL, MOB_AGGR_HUMAN, ENDBIT) ? "aggro" : "calm");

  act(buf2, FALSE, ch, NULL, NULL, TO_ROLLS);
#endif

  // Escortees can never be aggressive.
  if (is_escortee(ch))
    return FALSE;

  // If we're aggressive by nature, and the check calls for it, return true.
  if (include_base_aggression && MOB_FLAGS(ch).IsSet(MOB_AGGRESSIVE))
    return TRUE;

  // Check for the racial aggression octets.
  for (int i = 0; i < NUM_AGGRO_OCTETS; i++) {
    if (MOB_FLAGS(ch).IsSetPrecomputed(i, AGGRESSION_OCTETS[i])) {
#ifdef MOBACT_DEBUG
      snprintf(buf2, sizeof(buf2), "Found match in octet %d.", i);
      act(buf2, FALSE, ch, NULL, NULL, TO_ROLLS);
#endif
      return TRUE;
    }
  }

  return FALSE;
}

bool mobact_holster_weapon(struct char_data *ch) {
  if (!ch)
    return FALSE;

  // Not wielding anything? Can't holster.
  if (!GET_EQ(ch, WEAR_WIELD))
    return FALSE;

  // Find the first holster-type thing and shove this in there. Bypass all the checks.
  for (int i = 0; i < NUM_WEARS; i++) {
    if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_HOLSTER && !(GET_EQ(ch, i)->contains)) {
      // NPCs don't give a fuck about sheath/holster differences or any of that. They'll shove an assault cannon in a bootknife sheath.
      // TODO: Write the logic to make them care, but it's low priority.
      struct obj_data *obj = unequip_char(ch, WEAR_WIELD, TRUE);
      obj_to_obj(obj, GET_EQ(ch, i));
      send_to_char(ch, "You slip %s into %s.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(GET_EQ(ch, i)));
      act("$n slips $p into $P.", FALSE, ch, obj, GET_EQ(ch, i), TO_ROOM);
      GET_HOLSTER_READY_STATUS(GET_EQ(ch, i)) = TRUE;
      return TRUE;
    }
  }
  return FALSE;
}

bool vict_is_valid_target(struct char_data *ch, struct char_data *vict) {
  // Missing var? Failure.
  if (!ch || !vict || ch == vict)
    return FALSE;

  // Idle PC? Failure.
  if (!vict->desc || vict->char_specials.timer >= IDLE_TIMER_AGGRO_THRESHOLD) {
#ifdef MOBACT_DEBUG
    snprintf(buf3, sizeof(buf3), "vict_is_valid_target: Skipping %s - Idle / linkdead PC.", GET_CHAR_NAME(vict));
    do_say(ch, buf3, 0, 0);
#endif
    return FALSE;
  }

  // Can't see? Failure.
  if (!CAN_SEE_ROOM_SPECIFIED(ch, vict, get_ch_in_room(ch))) {
#ifdef MOBACT_DEBUG
    snprintf(buf3, sizeof(buf3), "vict_is_valid_target: Skipping %s - I can't see them.", GET_CHAR_NAME(vict));
    do_say(ch, buf3, 0, 0);
#endif
    return FALSE;
  }

  // Already downed? Failure.
  if (GET_PHYSICAL(vict) <= 0) {
#ifdef MOBACT_DEBUG
    snprintf(buf3, sizeof(buf3), "vict_is_valid_target: Skipping %s - they're already mortally wounded.", GET_CHAR_NAME(vict));
    do_say(ch, buf3, 0, 0);
#endif
    return FALSE;
  }

  // Astral mismatch? Failure.
  if ((IS_ASTRAL(ch) && !SEES_ASTRAL(vict)) || (IS_ASTRAL(vict) && !SEES_ASTRAL(ch))) {
    #ifdef MOBACT_DEBUG
        snprintf(buf3, sizeof(buf3), "vict_is_valid_target: Skipping %s - astral states do not match.", GET_CHAR_NAME(vict));
        do_say(ch, buf3, 0, 0);
    #endif
    return FALSE;
  }

  // Vehicle state mismatch (vict in veh that ch is not in)? Failure and bug report.
  if (vict->in_veh && vict->in_veh != ch->in_veh) {
    #ifdef MOBACT_DEBUG
      snprintf(buf3, sizeof(buf3), "vict_is_valid_target: %s is not a valid target-- they're in a vehicle I'm not in.", GET_CHAR_NAME(vict));
      do_say(ch, buf3, 0, 0);
    #endif
    snprintf(buf3, sizeof(buf3), "SYSERR: %s is in a vehicle that %s is not in during NPC aggro check!", GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
    mudlog(buf3, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  // Vict is an NPC.
  if (IS_NPC(vict)) {
    // Am I a quest mob hunting this mob?
    if (hunting_escortee(ch, vict)) {
#ifdef MOBACT_DEBUG
      snprintf(buf3, sizeof(buf3), "vict_is_valid_target: NPC %s is a valid target (escortee I am hunting).", GET_CHAR_NAME(vict));
      do_say(ch, buf3, 0, 0);
#endif
      return TRUE;
    }

    // Is this NPC protected by spec?
    if (npc_is_protected_by_spec(vict) || MOB_FLAGGED(vict, MOB_NOKILL) || vict->mob_specials.quest_id) {
      #ifdef MOBACT_DEBUG
        snprintf(buf3, sizeof(buf3), "vict_is_valid_target: NPC %s is not a valid target (protected by spec, !kill, or quest flag).", GET_CHAR_NAME(vict));
        do_say(ch, buf3, 0, 0);
      #endif
      return FALSE;
    }

    // Is this an astral projection?
    if (!IS_PROJECT(vict)) {
      // Nope. Is this a race I'm aggressive towards?
      if (!MOB_FLAGGED(ch, MOB_RACIAL_AGGR_VS_MOBS) || !mob_is_aggressive_towards_race(ch, GET_RACE(vict))) {
        #ifdef MOBACT_DEBUG
              snprintf(buf3, sizeof(buf3), "vict_is_valid_target: Skipping NPC %s - not hunted escortee, projection, or racial target.", GET_CHAR_NAME(vict));
              do_say(ch, buf3, 0, 0);
        #endif

        return FALSE;
      }
    }
  }

  // Vict is a PC.
  else {
    // Are they a staff with nohassle? Failure.
    if (PRF_FLAGGED(vict, PRF_NOHASSLE)) {
#ifdef MOBACT_DEBUG
      snprintf(buf3, sizeof(buf3), "vict_is_valid_target: Skipping PC %s - nohassle flag is set.", GET_CHAR_NAME(vict));
      do_say(ch, buf3, 0, 0);
#endif
      return FALSE;
    }

    // Are they our prior master (quest)? Failure.
    if (GET_QUEST(vict)) {
      int quest_idx = GET_QUEST(vict);

      for (int mob_idx = 0; mob_idx < quest_table[quest_idx].num_mobs; mob_idx++) {
        if (quest_table[quest_idx].mob[mob_idx].vnum == GET_MOB_VNUM(ch)
            && quest_table[quest_idx].mob[mob_idx].objective == QMO_LOCATION) {
#ifdef MOBACT_DEBUG
          snprintf(buf3, sizeof(buf3), "vict_is_valid_target: I am a prior escortee of PC %s, skipping.", GET_CHAR_NAME(vict));
          do_say(ch, buf3, 0, 0);
#endif
          return FALSE;
        }
      }
    }

    // Am I a quest mob and they're not part of it?
    if (GET_MOB_QUEST_CHAR_ID(ch) && GET_IDNUM(vict) != GET_MOB_QUEST_CHAR_ID(ch)) {
#ifdef MOBACT_DEBUG
      snprintf(buf3, sizeof(buf3), "vict_is_valid_target: PC %s is not on the quest that spawned me, skipping.", GET_CHAR_NAME(vict));
      do_say(ch, buf3, 0, 0);
#endif
      return FALSE;
    }

    // Can they hurt me?
    if (!can_hurt(vict, ch, 0, TRUE)) {
  #ifdef MOBACT_DEBUG
      snprintf(buf3, sizeof(buf3), "vict_is_valid_target: PC %s cannot hurt me, skipping.", GET_CHAR_NAME(vict));
      do_say(ch, buf3, 0, 0);
  #endif
      return FALSE;
    }
  }

  // They're a valid target, but I don't feel like fighting.
#ifdef MOBACT_DEBUG
  snprintf(buf3, sizeof(buf3), "vict_is_valid_target: %s is a valid target, provided I am aggressive.", GET_CHAR_NAME(vict));
  do_say(ch, buf3, 0, 0);
#endif
  return TRUE;
}

bool vict_is_valid_aggro_target(struct char_data *ch, struct char_data *vict) {
  if (!vict_is_valid_target(ch, vict))
    return FALSE;

  bool is_aggressive = MOB_FLAGS(ch).IsSet(MOB_AGGRESSIVE);
  bool is_alarmed_guard = MOB_FLAGGED(ch, MOB_GUARD) && GET_MOBALERT(ch) == MALERT_ALARM;
  bool is_alarmed_racist = mob_is_aggressive(ch, FALSE) && GET_MOBALERT(ch) == MALERT_ALARM;
  bool is_racially_motivated = mob_is_aggressive_towards_race(ch, GET_RACE(vict));

  if (is_aggressive || is_alarmed_guard || is_alarmed_racist || is_racially_motivated) {
#ifdef MOBACT_DEBUG
    snprintf(buf3, sizeof(buf3), "vict_is_valid_aggro_target: Target found (conditions: %s/%s/%s/%s): %s.",
             is_aggressive ? "base aggro" : "",
             is_alarmed_guard ? "alarmed guard" : "",
             is_alarmed_racist ? "alarmed racist" : "",
             is_racially_motivated ? "racial violence" : "",
             GET_CHAR_NAME(vict));
    do_say(ch, buf3, 0, 0);
#endif
    return TRUE;
  }

  // We allow alarmed, non-aggro NPCs to attack, but only if the victim could otherwise hurt them, and only if they're otherwise aggressive (guard, helper, etc)
  if (GET_MOBALERT(ch) == MALERT_ALARM && (MOB_FLAGGED(ch, MOB_HELPER) || MOB_FLAGGED(ch, MOB_GUARD))) {
#ifdef MOBACT_DEBUG
    snprintf(buf3, sizeof(buf3), "vict_is_valid_aggro_target: I am alarmed, so %s is a valid aggro target.", GET_CHAR_NAME(vict));
    do_say(ch, buf3, 0, 0);
#endif
    return TRUE;
  }

  return FALSE;
}

bool vict_is_valid_guard_target(struct char_data *ch, struct char_data *vict) {
  const char *guard_messages[] = {
/*  0 */ "%s Hey! You can't have %s here!",
         "%s Drop %s, slitch!",
         "%s Get out of here with %s!",
         "%s You can't have %s here!",
         "%s %s isn't allowed here! Leave!",
/*  5 */ "%s You're really bringing %s here? Really?",
         "%s Bringing %s in here was the last mistake you'll ever make.",
         "%s %s? Brave of you.",
         "%s Call the DocWagon. Maybe they can use %s to scrape you off the ground.",
         "%s You think %s is going to save you?",
/* 10 */ "%s I hope %s was worth all this pain.",
         "%s %s won't save you from me.",
         "%s Who told you you could have %s here??",
         "%s I'm keeping %s for myself, just watch.",
         "%s %s?? What is that, your safety blanket? Shadows too scary for you?",
/* 15 */ "%s If you think %s is going to make a difference here, you've got another think coming.",
         "%s I'm gonna feed you %s!",
         "%s I'm gonna break %s off in your hoop!",
         "%s Frag off, and take %s with you!"
  };
  #define NUM_GUARD_MESSAGES 19

  if (!vict_is_valid_target(ch, vict))
    return FALSE;

  int security_level = GET_SECURITY_LEVEL(get_ch_in_room(ch));
  for (int i = 0; i < NUM_WEARS; i++) {
    // If victim's equipment is illegal here, blast them.
    if (GET_EQ(vict, i) && violates_zsp(security_level, vict, i, ch)) {
      // Target found, stop processing.
      if (MOB_FLAGGED(ch, MOB_INANIMATE)) {
        act("$n swivels towards you threateningly, its cameras zooming in on $p.", TRUE, ch, GET_EQ(vict, i), vict, TO_VICT);
        act("$n swivels towards $N threateningly, its cameras zooming in on $p.", TRUE, ch, GET_EQ(vict, i), vict, TO_NOTVICT);
      } else {
        snprintf(buf3, sizeof(buf3), guard_messages[number(0, NUM_GUARD_MESSAGES - 1)], GET_CHAR_NAME(vict), decapitalize_a_an(GET_OBJ_NAME(GET_EQ(vict, i))));
        do_say(ch, buf3, 0, SCMD_SAYTO);
      }
      return TRUE;
    }
  }
  return FALSE;
}

void mobact_change_firemode(struct char_data *ch) {
  struct obj_data *weapon;
  int proning_desire = 0;

  // Precheck: Weapon must exist and must be a gun.
  if (!(weapon = GET_EQ(ch, WEAR_WIELD)) || !IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))) {
    // Melee fighters never want to be prone, so they'll stand up from that.
    if (AFF_FLAGGED(ch, AFF_PRONE)) {
#ifdef MOBACT_DEBUG
      act("$n is prone with a non-gun weapon; standing.", FALSE, ch, NULL, NULL, TO_ROLLS);
#endif
      strncpy(buf3, "", sizeof(buf3));
      do_prone(ch, buf3, 0, 0);
    }
    return;
  }

  // Empty? Reload the weapon if possible. If not, swap weapons.
  if (!weapon->contains && GET_WEAPON_MAX_AMMO(weapon) > 0 && !attempt_reload(ch, WEAR_WIELD)) {
    switch_weapons(ch, WEAR_WIELD);
    return;
  }

  // Otherwise, set up info: We're now checking for firemode changes.
  int prev_value = GET_WEAPON_FIREMODE(weapon);
  int standing_recoil_comp = MAX(GET_WEAPON_INTEGRAL_RECOIL_COMP(weapon), 0);
  int prone_recoil_comp = 0, total_recoil_comp = 0;
  int real_obj;
  struct obj_data *access;

  // Total up their recoil compensation.
  for (int i = ACCESS_LOCATION_TOP; i <= ACCESS_LOCATION_UNDER; i++) {
    if (GET_OBJ_VAL(weapon, i) > 0
        && (real_obj = real_object(GET_OBJ_VAL(weapon, i))) > 0
        && (access = &obj_proto[real_obj])) {

      switch (GET_ACCESSORY_TYPE(access)) {
        case ACCESS_GASVENT:
          standing_recoil_comp -= GET_ACCESSORY_RATING(access);
          break;
        case ACCESS_SHOCKPAD:
          standing_recoil_comp++;
          break;
        case ACCESS_BIPOD:
          prone_recoil_comp += RECOIL_COMP_VALUE_BIPOD;
          break;
        case ACCESS_TRIPOD:
          prone_recoil_comp += RECOIL_COMP_VALUE_TRIPOD;
          break;
      }
    }
  }

  total_recoil_comp = standing_recoil_comp + prone_recoil_comp;

  // Emplaced enemies have unlimited standing recoil comp, and thus don't ever want to prone.
  if (MOB_FLAGGED(ch, MOB_EMPLACED)) {
    standing_recoil_comp = 10;
    total_recoil_comp = 10;
    prone_recoil_comp = 0;
    proning_desire = -1000;
  }

  // You can't prone in vehicles.
  if (ch->in_veh)
    proning_desire = -1000;

  // We're inclined to stand up if we're not in combat or are in combat with someone in our same room.
  if ((GET_MOBALERT(ch) == MALERT_CALM && !FIGHTING(ch)) || (FIGHTING(ch) && FIGHTING(ch)->in_room == ch->in_room)) {
    // Note the -3 here-- someone with FA and a tripod will stay prone.
    proning_desire -= 3;
  }

  // We want to go prone more if we've got enough recoil comp for it. Note that we only consider positive proning desire if we have FA.
  proning_desire += MIN(prone_recoil_comp, 10);

  // Special case: If we're using a high-recoil weapon, that factors into our proning desire.
  if (GET_WEAPON_INTEGRAL_RECOIL_COMP(weapon) < 0)
    proning_desire += MIN(-GET_WEAPON_INTEGRAL_RECOIL_COMP(weapon), prone_recoil_comp);

  // Don't @ me about this, if I didn't take this shortcut then this would be a giant block of if-statements.
  int mode = GET_WEAPON_POSSIBLE_FIREMODES(weapon);
  bool has_multiple_modes = (mode != (1 << MODE_SS) && mode != (1 << MODE_SA) && mode != (1 << MODE_BF) && mode != (1 << MODE_FA));

  // If our weapon is FA-capable, let's set ourselves up for success by configuring it to the maximum value it can handle.
  if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(weapon), 1 << MODE_FA)) {
    // If we desire to go prone, do so-- we're guaranteed at least 3 points of comp from our proning_desire formula.
    if (proning_desire > 0 && total_recoil_comp >= 3) {
      GET_WEAPON_FIREMODE(weapon) = MODE_FA;
      GET_OBJ_TIMER(weapon) = total_recoil_comp;

      // Go prone.
      if (!AFF_FLAGGED(ch, AFF_PRONE)) {
        strlcpy(buf3, "", sizeof(buf3));
        do_prone(ch, buf3, 0, 0);
      }
    }
    // Otherwise, we're inclined to stand and deliver, provided we have the recoil comp for it.
    else {
      // Stand up.
      if (AFF_FLAGGED(ch, AFF_PRONE)) {
        strlcpy(buf3, "", sizeof(buf3));
        do_prone(ch, buf3, 0, 0);
      }

      // We've got the standing comp to support full auto-- do so!
      if (standing_recoil_comp >= 4) {
        GET_WEAPON_FIREMODE(weapon) = MODE_FA;
        GET_OBJ_TIMER(weapon) = standing_recoil_comp;
      }

      // Otherwise, if we only have 3 points of comp, just do burst fire.
      else if (standing_recoil_comp == 3) {
        GET_WEAPON_FIREMODE(weapon) = MODE_BF;
      }

      // We don't have enough recoil comp for a multi-shot burst. Set to SA if we can.
      else if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(weapon), 1 << MODE_SA)) {
        GET_WEAPON_FIREMODE(weapon) = MODE_SA;
      }

      // SS is an acceptable fallback.
      else if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(weapon), 1 << MODE_SS)) {
        GET_WEAPON_FIREMODE(weapon) = MODE_SS;
      }

      // We're in the shit zone now: Not enough comp, but no gentler modes. BF is the lesser of the two evils.
      else if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(weapon), 1 << MODE_BF)) {
        GET_WEAPON_FIREMODE(weapon) = MODE_BF;
      }

      // We have no choice. Minimize the harm to us with FA-3.
      else {
        GET_WEAPON_FIREMODE(weapon) = MODE_FA;
        GET_OBJ_TIMER(weapon) = 3;
      }
    }
  } // End check for weapon with FA.
  else {
    // If we're prone, we almost certainly don't want to be. Stand back up. We only consume proning_desire in MODE_FA above.
    if (AFF_FLAGGED(ch, AFF_PRONE)) {
      strlcpy(buf3, "", sizeof(buf3));
      do_prone(ch, buf3, 0, 0);
    }

    // Set to SA, or fallback to SS.
    if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(weapon), 1 << MODE_SA)) {
      GET_WEAPON_FIREMODE(weapon) = MODE_SA;
    } else if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(weapon), 1 << MODE_SS)) {
      GET_WEAPON_FIREMODE(weapon) = MODE_SS;
    }

    // Now, if we have enough recoil comp, override to BF.
    if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(weapon), 1 << MODE_BF) && standing_recoil_comp >= 3) {
      GET_WEAPON_FIREMODE(weapon) = MODE_BF;
    }

    // FA is not an option in this else-block, so we're done with our logic.
  }

  // Send a message to the room, but only if the weapon has received a new fire selection mode and has more than one available.
  if (prev_value != GET_WEAPON_FIREMODE(weapon) && has_multiple_modes) {
    if (!MOB_FLAGGED(ch, MOB_INANIMATE))
      act("$n flicks the fire selector switch on $p.", TRUE, ch, weapon, 0, TO_ROOM);

    #ifdef MOBACT_DEBUG
      snprintf(buf3, sizeof(buf3), "Changed firemode to %s. I have recoil comp %d standing + %d prone.", fire_mode[GET_WEAPON_FIREMODE(weapon)], standing_recoil_comp, prone_recoil_comp);
      do_say(ch, buf3, 0, 0);
    #endif
  }
}

// Check if an NPC successfully fires a spec-proc. Returns TRUE if so, FALSE otherwise.
bool mobact_evaluate_spec_proc(struct char_data *ch) {
  char empty = '\0';
  if (mob_index[GET_MOB_RNUM(ch)].func == NULL && mob_index[GET_MOB_RNUM(ch)].sfunc == NULL) {
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
  struct room_data *in_room;

  if (mob_cannot_be_aggressive(ch)) {
    #ifdef MOBACT_DEBUG
      strncpy(buf3, "m_p_i_v_g: I cannot be aggressive.", sizeof(buf));
      do_say(ch, buf3, 0, 0);
    #endif
    return FALSE;
  }

  // Precondition: Vehicle must exist, we must be manning or driving, and we must not be astral.
  if (!ch->in_veh || !(AFF_FLAGGED(ch, AFF_PILOT) || AFF_FLAGGED(ch, AFF_MANNING)) || IS_ASTRAL(ch)) {
#ifdef MOBACT_DEBUG
    strncpy(buf3, "m_p_i_v_g: no veh, or not pilot/manning, or am astral.", sizeof(buf));
    do_say(ch, buf3, 0, 0);
#endif
    return FALSE;
  }

  // Precondition: If our vehicle is nested, just give up.
  if (ch->in_veh->in_veh) {
#ifdef MOBACT_DEBUG
    strncpy(buf3, "m_p_i_v_g: nested vehicle.", sizeof(buf));
    do_say(ch, buf3, 0, 0);
#endif
    return FALSE;
  }

  in_room = get_ch_in_room(ch);

  // Peaceful room, or I'm not actually a guard? Bail out.
  if (in_room->peaceful || !(MOB_FLAGGED(ch, MOB_GUARD))) {
#ifdef MOBACT_DEBUG
    strncpy(buf3, "m_p_i_v_g: peaceful or not guard.", sizeof(buf));
    do_say(ch, buf3, 0, 0);
#endif
    return FALSE;
  }

  /* Guard NPCs. */

  // If we're not in a road or garage, we expect to see no vehicles and will attack any that we see.
  if (!(ROOM_FLAGGED(in_room, ROOM_ROAD) || ROOM_FLAGGED(in_room, ROOM_GARAGE))) {
    bool area_has_pc_occupants = FALSE;
    for (struct char_data *check_ch = in_room->people; !area_has_pc_occupants && check_ch; check_ch = check_ch->next_in_room)
      area_has_pc_occupants = !IS_NPC(check_ch);

    for (tveh = in_room->vehicles; tveh; tveh = tveh->next_veh) {
      // No attacking your own vehicle.
      if (tveh == ch->in_veh)
        continue;

      // If nobody's in the room or in the vehicle, you can't attack unless it's a drone.
      if (tveh->type != VEH_DRONE && !area_has_pc_occupants && !tveh->people && !tveh->rigger)
        continue;

      // Check for our usual conditions.
      if (vehicle_is_valid_mob_target(tveh, GET_MOBALERT(ch) == MALERT_ALARM, ch->mob_specials.quest_id) && (!GET_MOB_QUEST_CHAR_ID(ch) || !tveh->owner || tveh->owner == GET_MOB_QUEST_CHAR_ID(ch))) {
        // Found a target, stop processing vehicles.
#ifdef MOBACT_DEBUG
        snprintf(buf3, sizeof(buf), "m_p_i_v_g: Target found, attacking veh %s.", GET_VEH_NAME(tveh));
        do_say(ch, buf3, 0, 0);
#endif
        break;
      }
    }
  }

  // No vehicular targets? Check players.
  if (!tveh)
    for (vict = in_room->people; vict; vict = vict->next_in_room)
      if (vict_is_valid_guard_target(ch, vict))
        break;

  // No targets? Bail out.
  if (!tveh && !vict) {
#ifdef MOBACT_DEBUG
    strncpy(buf3, "m_p_i_v_g: No targets, stopping.", sizeof(buf));
    do_say(ch, buf3, 0, 0);
#endif
    return FALSE;
  }

  // Driver? It's rammin' time.
  if (AFF_FLAGGED(ch, AFF_PILOT)) {
#ifdef MOBACT_DEBUG
    strncpy(buf3, "m_p_i_v_g: Ramming.", sizeof(buf));
    do_say(ch, buf3, 0, 0);
#endif
    do_raw_ram(ch, ch->in_veh, tveh, vict);

    return TRUE;
  }

  // Dakka o'clock.
  else if (AFF_FLAGGED(ch, AFF_MANNING)) {
    struct obj_data *mount = get_mount_manned_by_ch(ch);
    if (mount && mount_has_weapon(mount)) {
      struct obj_data *ammobox = get_mount_ammo(mount);
      if (!ammobox || GET_AMMOBOX_QUANTITY(ammobox) <= 0) {
        char empty_argument[1];
        *empty_argument = '\0';
        do_reload(ch, empty_argument, 0, 0);
      }
#ifdef MOBACT_DEBUG
      strncpy(buf3, "m_p_i_v_g: Firing.", sizeof(buf));
      do_say(ch, buf3, 0, 0);
#endif
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
  struct room_data *in_room;

  if (mob_cannot_be_aggressive(ch)) {
    #ifdef MOBACT_DEBUG
      strncpy(buf3, "m_p_i_v_a: I cannot be aggressive.", sizeof(buf));
      do_say(ch, buf3, 0, 0);
    #endif
    return FALSE;
  }

  // Precondition: Vehicle must exist, we must be manning or driving, and we must not be astral.
  if (!ch->in_veh || !(AFF_FLAGGED(ch, AFF_PILOT) || AFF_FLAGGED(ch, AFF_MANNING)) || IS_ASTRAL(ch)) {
#ifdef MOBACT_DEBUG
    strncpy(buf3, "m_p_i_v_a: no veh, not pilot/manning, or am astral.", sizeof(buf));
    do_say(ch, buf3, 0, 0);
#endif
    return FALSE;
  }

  // Precondition: If our vehicle is nested, just give up.
  if (ch->in_veh->in_veh) {
#ifdef MOBACT_DEBUG
    strncpy(buf3, "m_p_i_v_a: nested veh.", sizeof(buf));
    do_say(ch, buf3, 0, 0);
#endif
    return FALSE;
  }

  in_room = get_ch_in_room(ch);

  if (in_room->peaceful) {
#ifdef MOBACT_DEBUG
    strncpy(buf3, "m_p_i_v_a: Room is peaceful.", sizeof(buf));
    do_say(ch, buf3, 0, 0);
#endif
    return FALSE;
  }

  if (!mob_is_aggressive(ch, TRUE) && GET_MOBALERT(ch) != MALERT_ALARM) {
#ifdef MOBACT_DEBUG
    strncpy(buf3, "m_p_i_v_a: I am neither aggressive nor alarmed.", sizeof(buf));
    do_say(ch, buf3, 0, 0);
#endif
    return FALSE;
  }

  // Attack vehicles (but not for aggr-to-race)
  if (MOB_FLAGGED(ch, MOB_AGGRESSIVE) || (GET_MOBALERT(ch) == MALERT_ALARM && !MOB_FLAGGED(ch, MOB_WIMPY))) {
    bool area_has_pc_occupants = FALSE;
    for (struct char_data *check_ch = in_room->people; !area_has_pc_occupants && check_ch; check_ch = check_ch->next_in_room)
      area_has_pc_occupants = !IS_NPC(check_ch);

    for (tveh = in_room->vehicles; tveh; tveh = tveh->next_veh) {
      // No attacking your own vehicle.
      if (tveh == ch->in_veh)
        continue;

      // If nobody's in the room or in the vehicle, you can't attack unless it's a drone.
      if (tveh->type != VEH_DRONE && !area_has_pc_occupants && !tveh->people && !tveh->rigger)
        continue;

      if (vehicle_is_valid_mob_target(tveh, TRUE, ch->mob_specials.quest_id) && (!GET_MOB_QUEST_CHAR_ID(ch) || !tveh->owner || tveh->owner == GET_MOB_QUEST_CHAR_ID(ch))) {
        // Found a valid target, stop looking.
#ifdef MOBACT_DEBUG
        snprintf(buf3, sizeof(buf), "m_p_i_v_a: Target found, attacking veh %s.", GET_VEH_NAME(tveh));
        do_say(ch, buf3, 0, 0);
#endif
        break;
      }
    }
  }

  // No vehicle target found. Check character targets.
  if (!tveh)
    for (vict = in_room->people; vict; vict = vict->next_in_room)
      if (vict_is_valid_aggro_target(ch, vict))
        break;

  if (!tveh && !vict) {
#ifdef MOBACT_DEBUG
    strncpy(buf3, "m_p_i_v_a: No target.", sizeof(buf));
    do_say(ch, buf3, 0, 0);
#endif
    return FALSE;
  }

  // Driver? It's rammin' time.
  if (AFF_FLAGGED(ch, AFF_PILOT)) {
#ifdef MOBACT_DEBUG
    strncpy(buf3, "m_p_i_v_a: Ramming.", sizeof(buf));
    do_say(ch, buf3, 0, 0);
#endif
    do_raw_ram(ch, ch->in_veh, tveh, vict);

    return TRUE;
  }

  // Dakka o'clock.
  else if (AFF_FLAGGED(ch, AFF_MANNING)) {
    struct obj_data *mount = get_mount_manned_by_ch(ch);
    if (mount && mount_has_weapon(mount)) {
      struct obj_data *ammobox = get_mount_ammo(mount);
      if (!ammobox || GET_AMMOBOX_QUANTITY(ammobox) <= 0) {
        char empty_argument[1];
        *empty_argument = '\0';
        do_reload(ch, empty_argument, 0, 0);
      }
#ifdef MOBACT_DEBUG
      strncpy(buf3, "m_p_i_v_a: Firing.", sizeof(buf));
      do_say(ch, buf3, 0, 0);
#endif
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

  // Vehicle code is separate. Vehicles only attack same room.
  if (ch->in_veh) {
    if (ch->in_veh->in_room->number == room->number)
      return mobact_process_in_vehicle_aggro(ch);
    else
      return FALSE;
  }

  if (room->peaceful) {
#ifdef MOBACT_DEBUG
    strncpy(buf3, "m_p_a: Room is peaceful.", sizeof(buf));
    do_say(ch, buf3, 0, 0);
#endif
    return FALSE;
  }

  if (mob_cannot_be_aggressive(ch)) {
    #ifdef MOBACT_DEBUG
      strncpy(buf3, "m_p_a: I cannot be aggressive.", sizeof(buf));
      do_say(ch, buf3, 0, 0);
    #endif
    return FALSE;
  }

  if (!mob_is_aggressive(ch, TRUE) && GET_MOBALERT(ch) != MALERT_ALARM) {
#ifdef MOBACT_DEBUG
    strncpy(buf3, "m_p_a: I am neither aggressive nor alarmed.", sizeof(buf));
    do_say(ch, buf3, 0, 0);
#endif
    return FALSE;
  }

  // Attack vehicles (but not for aggr-to-race)
  if (MOB_FLAGGED(ch, MOB_AGGRESSIVE) || (GET_MOBALERT(ch) == MALERT_ALARM && !MOB_FLAGGED(ch, MOB_WIMPY))) {
    // If I am not astral, am in the same room, and am willing to attack a vehicle this round (coin flip), pick a fight with a vehicle.
    if (ch->in_room->number == room->number && !IS_ASTRAL(ch) && number(0, 1)) {
      bool area_has_pc_occupants = FALSE;

      for (struct char_data *check_ch = room->people; !area_has_pc_occupants && check_ch; check_ch = check_ch->next_in_room)
        area_has_pc_occupants = !IS_NPC(check_ch);

      for (veh = room->vehicles; veh; veh = veh->next_veh) {
        // If nobody's in the room or in the vehicle, you can't attack unless it's a drone.
        if (veh->type != VEH_DRONE && !area_has_pc_occupants && !veh->people && !veh->rigger)
          continue;

        // Aggros don't care about road/garage status, so they act as if always alarmed.
        if (vehicle_is_valid_mob_target(veh, TRUE, ch->mob_specials.quest_id) && (!GET_MOB_QUEST_CHAR_ID(ch) || !veh->owner || veh->owner == GET_MOB_QUEST_CHAR_ID(ch))) {
          stop_fighting(ch);

          if (MOB_FLAGGED(ch, MOB_INANIMATE)) {
            snprintf(buf, sizeof(buf), "%s$n swivels aggressively towards %s!", GET_MOBALERT(ch) == MALERT_ALARM ? "Searching for more aggressors, " : "", GET_VEH_NAME(veh));
            snprintf(buf2, sizeof(buf2), "%s%s swivels aggressively towards your vehicle!\r\n", GET_MOBALERT(ch) == MALERT_ALARM ? "Searching for more aggressors, " : "" , GET_CHAR_NAME(ch));
            send_to_char(ch, "You prepare to attack %s!\r\n", GET_VEH_NAME(veh));
          } else {
            snprintf(buf, sizeof(buf), "%s$n glares at %s, preparing to attack it!", GET_MOBALERT(ch) == MALERT_ALARM ? "Searching for more aggressors, " : "", GET_VEH_NAME(veh));
            snprintf(buf2, sizeof(buf2), "%s%s glares at your vehicle, preparing to attack!\r\n", GET_MOBALERT(ch) == MALERT_ALARM ? "Searching for more aggressors, " : "" , GET_CHAR_NAME(ch));
            send_to_char(ch, "You prepare to attack %s!\r\n", GET_VEH_NAME(veh));
          }


          act(buf, TRUE, ch, NULL, NULL, TO_ROOM);
          send_to_veh(buf2, veh, NULL, TRUE);

          set_fighting(ch, veh);

          return TRUE;
        }
      }
    }
  }

  // If we've gotten here, character is either astral or is not willing to / failed to attack a vehicle.
  for (vict = room->people; vict; vict = vict->next_in_room) {
    if (vict_is_valid_aggro_target(ch, vict)) {
      stop_fighting(ch);
      send_mob_aggression_warnings(vict, ch);
      set_fighting(ch, vict);
      return TRUE;
    }
  }

  return FALSE;
}

void send_mob_aggression_warnings(struct char_data *pc, struct char_data *mob) {
  int mob_tn = 4;
  int mob_stealth_dice = get_skill(mob, SKILL_STEALTH, mob_tn);
  int mob_stealth_successes = success_test(mob_stealth_dice, mob_tn);

  if (!pc) {
    mudlog("SYSERR: Received null PC to send_mob_aggression_warnings().", mob, LOG_SYSLOG, TRUE);
    return;
  }

  if (!mob) {
    mudlog("SYSERR: Received null mob to send_mob_aggression_warnings().", pc, LOG_SYSLOG, TRUE);
    return;
  }

  int pc_tn = 4;
  if (pc->in_room == mob->in_room)
    pc_tn = 2;

  strlcpy(buf, "send_mob_aggression_warnings rbuf: ", sizeof(buf));
  pc_tn += modify_target_rbuf(pc, buf, sizeof(buf));
  act(buf, FALSE, pc, NULL, NULL, TO_ROLLS);

  int pc_dice = GET_INT(pc) + GET_POWER(pc, ADEPT_IMPROVED_PERCEPT);
  int pc_percept_successes = success_test(pc_dice, pc_tn);

  int net_succ = pc_percept_successes - mob_stealth_successes;

  snprintf(buf2, sizeof(buf2), "Aggression perception check: $n rolled %d success%s (%d dice, TN %d) vs $N's %d success%s (%d dice, TN %d). Net = %d, so PC %s.",
           pc_percept_successes,
           pc_percept_successes != 1 ? "es" : "",
           pc_dice,
           pc_tn,
           mob_stealth_successes,
           mob_stealth_successes != 1 ? "es" : "",
           mob_stealth_dice,
           mob_tn,
           net_succ,
           net_succ > 0 ? "succeeded" : "failed");

  act(buf2, TRUE, pc, NULL, mob, TO_ROLLS);

  // Message the PC.
  if (net_succ > 0) {
    if (pc->in_room == mob->in_room) {
      if (MOB_FLAGGED(mob, MOB_INANIMATE)) {
        snprintf(buf, sizeof(buf), "^y%s$n swivels towards you threateningly!^n", GET_MOBALERT(mob) == MALERT_ALARM ? "Searching for more aggressors, " : "");
      } else {
        snprintf(buf, sizeof(buf), "^y%s$n glares at you, preparing to attack!^n", GET_MOBALERT(mob) == MALERT_ALARM ? "Searching for more aggressors, " : "");
      }
      act(buf, TRUE, mob, NULL, pc, TO_VICT);
    } else {
      if (GET_EQ(mob, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(mob, WEAR_WIELD)) == ITEM_WEAPON && IS_GUN(GET_WEAPON_ATTACK_TYPE(GET_EQ(mob, WEAR_WIELD)))) {
        send_to_char("^yThe glint of a scope aiming your way catches your eye!^n\r\n", pc);
      } else {
        switch(number(0, 2)) {
          case 0: send_to_char("^yA sudden chill runs down your spine.^n\r\n", pc); break;
          case 1: send_to_char("^yThe hair on the back of your neck stands up.^n\r\n", pc); break;
          case 2: send_to_char("^yYou feel unfriendly eyes watching you.^n\r\n", pc); break;
          case 3: send_to_char("^yYou feel inexplicably uneasy.^n\r\n", pc); break;
        }
      }
    }
  }

  // Message the NPC.
  send_to_char(mob, "You prepare to attack %s!\r\n", GET_CHAR_NAME(pc));

  // Message bystanders.
  if (pc->in_room == mob->in_room) {
    if (MOB_FLAGGED(mob, MOB_INANIMATE)) {
      act("$n swivels threateningly towards $N!", TRUE, mob, NULL, pc, TO_NOTVICT);
    } else {
      act("$n glares at $N, preparing to attack!", TRUE, mob, NULL, pc, TO_NOTVICT);
    }
  }
  else {
    if (MOB_FLAGGED(mob, MOB_INANIMATE)) {
      act("$n swivels aggressively to point at a distant threat!", TRUE, mob, NULL, NULL, TO_ROOM);
    } else {
      act("$n glares off into the distance, preparing to attack an intruder!", TRUE, mob, NULL, NULL, TO_ROOM);
    }
  }
}

bool mobact_process_memory(struct char_data *ch, struct room_data *room) {
  struct char_data *vict = NULL;

  if (is_escortee(ch))
    return FALSE;

  /* Mob Memory */
  if (MOB_FLAGGED(ch, MOB_MEMORY) && GET_MOB_MEMORY(ch)) {
    for (vict = room->people; vict; vict = vict->next_in_room) {
      // Skip NPCs, invisibles, and nohassle targets.
      if (IS_NPC(vict) || !CAN_SEE_ROOM_SPECIFIED(ch, vict, room) || PRF_FLAGGED(vict, PRF_NOHASSLE))
        continue;

      // If we remember them, fuck 'em up.
      if (memory(ch, vict)) {
        act("'Hey! You're the fragger that attacked me!!!', exclaims $n.", FALSE, ch, 0, 0, TO_ROOM);
        stop_fighting(ch);
        send_mob_aggression_warnings(vict, ch);
        set_fighting(ch, vict);
        return true;
      }
    }
  }
  return false;
}

bool mobact_process_helper(struct char_data *ch) {
  struct char_data *vict = NULL;

  if (is_escortee(ch))
    return FALSE;

  /* Helper Mobs - guards are always helpers */
  if (MOB_FLAGGED(ch, MOB_HELPER) || MOB_FLAGGED(ch, MOB_GUARD)) {
    for (vict = ch->in_room->people; vict; vict = vict->next_in_room) {
      // Ensure we're neither of the fighting parties. This check should be redundant since no fighting NPC can proceed through mobile_activity().
      if (ch == vict || !FIGHTING(vict) || ch == FIGHTING(vict))
        continue;

      // If victim is a visible NPC who is fighting a player, and the player can hurt me, assist the NPC.
      if (IS_NPC(vict) && !IS_NPC(FIGHTING(vict)) && CAN_SEE(ch, vict) && can_hurt(FIGHTING(vict), ch, 0, TRUE)) {
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

        return TRUE;
      }

      // If the victim is a player who is fighting a visible NPC, and the player can hurt me, assist the NPC.
      if (!IS_NPC(vict) && IS_NPC(FIGHTING(vict)) && CAN_SEE(ch, FIGHTING(vict)) && can_hurt(vict, ch, 0, TRUE)) {
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

bool vehicle_is_valid_mob_target(struct veh_data *veh, bool alarmed, int quest_id) {
  struct room_data *room = veh->in_room;

  // Vehicle's not in a room? Skip.
  if (!room)
    return FALSE;

  // We don't attack vehicles in their proper places-- unless we're alarmed.
  if (!alarmed && (ROOM_FLAGGED(room, ROOM_ROAD) || ROOM_FLAGGED(room, ROOM_GARAGE)))
    return FALSE;

  // We don't attack destroyed vehicles.
  if (veh->damage >= VEH_DAM_THRESHOLD_DESTROYED)
    return FALSE;

  if (quest_id) {
    // We attack questor-owned vehicles that are occupied or rigged.
    if (veh->owner == quest_id && (veh->people || veh->rigger))
      return TRUE;

    // We attack any vehicle rigged by our questor.
    if (veh->rigger && GET_IDNUM(veh->rigger) == quest_id)
      return TRUE;

    // We attack questor-occupied vehicles.
    for (struct char_data *vict = veh->people; vict; vict = vict->next_in_veh)
      if (!IS_NPC(vict) && GET_IDNUM(vict) == quest_id)
        return TRUE;
  } else {
    // We attack player-owned vehicles that are occupied or rigged.
    if (veh->owner > 0 && (veh->people || veh->rigger))
      return TRUE;

    // We attack player-occupied vehicles.
    for (struct char_data *vict = veh->people; vict; vict = vict->next_in_veh)
      if (!IS_NPC(vict))
        return TRUE;
  }

  // Otherwise, no good.
  return FALSE;
}

bool mobact_process_guard(struct char_data *ch, struct room_data *room) {
  struct char_data *vict = NULL;
  struct veh_data *veh = NULL;

  if (is_escortee(ch))
    return FALSE;

  // Vehicle code is separate.
  if (ch->in_veh) {
    if (ch->in_veh->in_room == room)
      return mobact_process_in_vehicle_guard(ch);
    else
      return FALSE;
  }

  if (mob_cannot_be_aggressive(ch)) {
    #ifdef MOBACT_DEBUG
      strncpy(buf3, "m_p_g: I cannot be aggressive.", sizeof(buf));
      do_say(ch, buf3, 0, 0);
    #endif
    return FALSE;
  }

  // Check vehicles, but only if they're in the same room as the guard.
  if (ch->in_room == room) {
    bool area_has_pc_occupants = FALSE;
    for (struct char_data *check_ch = ch->in_room->people; !area_has_pc_occupants && check_ch; check_ch = check_ch->next_in_room)
      area_has_pc_occupants = !IS_NPC(check_ch);

    for (veh = room->vehicles; veh; veh = veh->next_veh) {
      // If nobody's in the room or in the vehicle, you can't attack unless it's a drone.
      if (veh->type != VEH_DRONE && !area_has_pc_occupants && !veh->people && !veh->rigger)
        continue;

      // If the room we're in is neither a road nor a garage, attack any vehicles we see. Never attack vehicles in a garage.
      if (vehicle_is_valid_mob_target(veh, GET_MOBALERT(ch) == MALERT_ALARM && !ROOM_FLAGGED(room, ROOM_GARAGE) && !MOB_FLAGGED(ch, MOB_WIMPY), ch->mob_specials.quest_id)) {
        stop_fighting(ch);

        if (MOB_FLAGGED(ch, MOB_INANIMATE)) {
          snprintf(buf, sizeof(buf), "%s$n swivels threateningly towards %s, preparing to attack it for security infractions!", GET_MOBALERT(ch) == MALERT_ALARM ? "Searching for more aggressors, " : "", GET_VEH_NAME(veh));
          snprintf(buf2, sizeof(buf2), "%s%s swivels threateningly towards your vehicle, preparing to attack over security infractions!\r\n", GET_MOBALERT(ch) == MALERT_ALARM ? "Searching for more aggressors, " : "", GET_CHAR_NAME(ch));
          send_to_char(ch, "You prepare to attack %s for security infractions!\r\n", GET_VEH_NAME(veh));
        } else {
          snprintf(buf, sizeof(buf), "%s$n glares at %s, preparing to attack it for security infractions!", GET_MOBALERT(ch) == MALERT_ALARM ? "Searching for more aggressors, " : "", GET_VEH_NAME(veh));
          snprintf(buf2, sizeof(buf2), "%s%s glares at your vehicle, preparing to attack over security infractions!\r\n", GET_MOBALERT(ch) == MALERT_ALARM ? "Searching for more aggressors, " : "", GET_CHAR_NAME(ch));
          send_to_char(ch, "You prepare to attack %s for security infractions!\r\n", GET_VEH_NAME(veh));
        }

        act(buf, TRUE, ch, NULL, NULL, TO_ROOM);
        send_to_veh(buf2, veh, NULL, TRUE);

        set_fighting(ch, veh);
        return TRUE;
      }
    }
  }

  // Check players.
  for (vict = room->people; vict; vict = vict->next_in_room) {
    if (vict_is_valid_guard_target(ch, vict)) {
      stop_fighting(ch);
      send_mob_aggression_warnings(vict, ch);
      set_fighting(ch, vict);
      return TRUE;
    }
  }

  return FALSE;
}

bool mobact_process_self_buff(struct char_data *ch) {
  if (!GET_SKILL(ch, SKILL_SORCERY) || GET_MAG(ch) < 100  || MOB_FLAGGED(ch, MOB_SPEC))
    return FALSE;

  // Skip broken-ass characters.
  if (!ch->in_room && !ch->in_veh) {
    snprintf(buf, sizeof(buf), "FAILSAFE SYSERR: Encountered char '%s' with no room, no veh in mobact_process_self_buff().", GET_CHAR_NAME(ch));
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  // Always self-heal if able.
  if (GET_PHYSICAL(ch) < GET_MAX_PHYSICAL(ch) && !AFF_FLAGGED(ch, AFF_HEALED) && GET_MENTAL(ch) >= 800) {
    cast_health_spell(ch, SPELL_HEAL, 0, number(1, GET_MAG(ch)/100), NULL, ch);
    return TRUE;
  }

  // Buff self, but only act one out of every 11 ticks (on average), and only if we're not going to put ourselves in a drain death loop.
  if (GET_MENTAL(ch) >= 1000 && GET_PHYSICAL(ch) >= 1000 && GET_MOBALERT(ch) != MALERT_ALARM) {
    bool imp_invis = IS_AFFECTED(ch, AFF_SPELLIMPINVIS) || affected_by_spell(ch, SPELL_IMP_INVIS);
    bool std_invis = IS_AFFECTED(ch, AFF_SPELLINVIS) || affected_by_spell(ch, SPELL_INVIS);
    int max_force = GET_MAG(ch) / 100;
    int min_force = MIN(4, max_force);

    // If not invisible already, apply an invisibility spell based on my magic rating and sorcery skill.
    if (!imp_invis && !std_invis && !IS_ASTRAL(ch)) {
      if (MIN(GET_SKILL(ch, SKILL_SORCERY), GET_MAG(ch)/100) < MOB_IMP_INVIS_MAGIC_FLOOR) {
        // Lower skill means standard invisibility. Gotta make thermographic vision useful somehow.
        cast_illusion_spell(ch, SPELL_INVIS, number(min_force, max_force), NULL, ch);
      } else {
        // Look out, we've got a badass over here.
        cast_illusion_spell(ch, SPELL_IMP_INVIS, number(min_force, max_force), NULL, ch);
      }
      return TRUE;
    }

#ifdef DIES_IRAE
    // If we've got Improved Invis on, we want to go completely stealth if we can.
    // Gating this behind Improved Invis means that only powerful mage characters (Sorcery and Magic both 6+) will do this.
    if (imp_invis && !affected_by_spell(ch, SPELL_STEALTH) && !IS_ASTRAL(ch)) {
      cast_illusion_spell(ch, SPELL_STEALTH, number(1, MIN(4, max_force)), NULL, ch);
      return TRUE;
    }
#endif

    if (number(0, 10) == 0) {
      // Apply armor to self.
      if (!affected_by_spell(ch, SPELL_ARMOR) && !IS_ASTRAL(ch)) {
        cast_manipulation_spell(ch, SPELL_ARMOR, number(min_force, max_force), NULL, ch);
        return TRUE;
      }

      if (number(0, 5) == 0) {
        // Apply combat sense to self.
        if (GET_MAG(ch) >= 4 && !affected_by_spell(ch, SPELL_COMBATSENSE)) {
          cast_detection_spell(ch, SPELL_COMBATSENSE, number(min_force, max_force), NULL, ch);
          return TRUE;
        }

        // Apply the appropriate increased reflexes spell.
        if (!affected_by_spell(ch, SPELL_INCREF3) && !affected_by_spell(ch, SPELL_INCREF2) && !affected_by_spell(ch, SPELL_INCREF1)) {
          if (GET_MAG(ch) >= 8) {
            cast_health_spell(ch, SPELL_INCREF3, 0, number(min_force, max_force), NULL, ch);
            return TRUE;
          }
          else if (GET_MAG(ch) >= 6) {
            cast_health_spell(ch, SPELL_INCREF2, 0, number(min_force, max_force), NULL, ch);
            return TRUE;
          }
          else if (GET_MAG(ch) >= 4) {
            cast_health_spell(ch, SPELL_INCREF1, 0, number(min_force, max_force), NULL, ch);
            return TRUE;
          }
        }

        // We're dead-set on casting a spell, so try to boost attributes.
        min_force = min_force % 2 == 0 ? min_force : min_force - 1;
        max_force = max_force % 2 == 0 ? max_force : max_force - 1;
        switch (number(1, 5)) {
          case 1:
            cast_health_spell(ch, ch->cyberware || ch->bioware ? SPELL_INCCYATTR : SPELL_INCATTR, STR, number(min_force, max_force), NULL, ch);
            return TRUE;
          case 2:
            cast_health_spell(ch, ch->cyberware || ch->bioware ? SPELL_INCCYATTR : SPELL_INCATTR, QUI, number(min_force, max_force), NULL, ch);
            return TRUE;
          case 3:
            cast_health_spell(ch, ch->cyberware || ch->bioware ? SPELL_INCCYATTR : SPELL_INCATTR, BOD, number(min_force, max_force), NULL, ch);
            return TRUE;
          case 4:
            cast_health_spell(ch, ch->cyberware || ch->bioware ? SPELL_INCCYATTR : SPELL_INCATTR, INT, number(min_force, max_force), NULL, ch);
            return TRUE;
          case 5:
            cast_health_spell(ch, ch->cyberware || ch->bioware ? SPELL_INCCYATTR : SPELL_INCATTR, WIL, number(min_force, max_force), NULL, ch);
            return TRUE;
        }
      }
    }
  }

  return FALSE;
}

bool mobact_process_scavenger(struct char_data *ch) {
  if (!ch || !ch->in_room)
    return FALSE;

  /* Scavenger (picking up objects) */
  if (MOB_FLAGGED(ch, MOB_SCAVENGER)) {
    if (ch->in_room->contents && !number(0, 10)) {
      struct obj_data *obj, *best_obj = NULL;
      int max = 1;

      // Find the most valuable object in the room (ignoring worthless things):
      FOR_ITEMS_AROUND_CH(ch, obj) {
        // No stealing workshops or corpses.
        if (GET_OBJ_TYPE(obj) == ITEM_WORKSHOP || GET_OBJ_TYPE(obj) == ITEM_EXTRA_CORPSE)
          continue;

        // No scavenging people's quest items.
        if (obj->obj_flags.quest_id)
          continue;

        // Only grab things we can actually get.
        if (!CAN_GET_OBJ(ch, obj))
          continue;

        // We always go for keys.
        if (GET_OBJ_TYPE(obj) == ITEM_KEY) {
          best_obj = obj;
          break;
        }

        // Otherwise, we only want it if it's worth more than the others.
        if (GET_OBJ_COST(obj) > max) {
          best_obj = obj;
          max = GET_OBJ_COST(obj);
        }
      }

      // Get the most valuable thing we've found.
      if (best_obj != NULL) {
        obj_from_room(best_obj);
        if (GET_OBJ_TYPE(best_obj) == ITEM_KEY) {
          // "Accidentally."
          act("$n accidentally breaks $p.", FALSE, ch, best_obj, 0, TO_ROOM);
          extract_obj(best_obj);
        } else {
          obj_to_char(best_obj, ch);
          act("$n gets $p.", FALSE, ch, best_obj, 0, TO_ROOM);
        }

        return TRUE;
      }
    }
  } /* End scavenger NPC. */
  return FALSE;
}

// Find where the 'push' command is in the command index. This is SUCH a hack.
int global_push_command_index = -1;
int get_push_command_index() {
  if (global_push_command_index != -1)
    return global_push_command_index;

  global_push_command_index = find_command("push");

  if (global_push_command_index == -1) {
    mudlog("FATAL ERROR: Unable to find index of PUSH command for NPC elevator usage. Put it back.", NULL, LOG_SYSLOG, TRUE);
    exit(ERROR_UNABLE_TO_FIND_PUSH_COMMAND);
  }

  return global_push_command_index;
}

bool mobact_process_movement(struct char_data *ch) {
  int door;

  if (is_escortee(ch))
    return FALSE;

  // If you can't move for a variety of reasons, bail out.
  if (MOB_FLAGGED(ch, MOB_SENTINEL) || CH_IN_COMBAT(ch) || AFF_FLAGGED(ch, AFF_PRONE))
    return FALSE;

  // The only way you can move while not standing is if you're driving a vehicle.
  if (GET_POS(ch) != POS_STANDING && !AFF_FLAGGED(ch, AFF_PILOT))
    return FALSE;

  // Slow down movement.
  if (number(0, 3) == 0)
    return FALSE;

  // NPC in a vehicle that's not in another vehicle? Have them drive and obey the rules of the road.
  if (ch->in_veh) {
    // Guard against a weird crash that happens occasionally.
    ch->in_room = NULL;

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
        // snprintf(buf3, sizeof(buf3), "Driver %s attempted to move %s from %s [%ld].", GET_NAME(ch), fulldirs[door], GET_ROOM_NAME(ch->in_veh->in_room), GET_ROOM_VNUM(ch->in_veh->in_room));
        perform_move(ch, door, LEADER, NULL);
        // spnrintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "  After move, driver now in %s [%ld].", GET_ROOM_NAME(ch->in_veh->in_room), GET_ROOM_VNUM(ch->in_veh->in_room));
        // mudlog(buf3, ch, LOG_SYSLOG, TRUE);
        return TRUE;
      }
    }
  } /* End NPC movement in vehicle. */

  // NPC not in a vehicle (walking).
  else {
    // Skip NOWHERE-located NPCs since they'll break things.
    if (!ch->in_room)
      return FALSE;

    // NPC standing outside an elevator? Maybe they want to call it.
    if (ch->in_room->func == call_elevator
        && !mob_is_aggressive(ch, TRUE)
        && number(0, ELEVATOR_BUTTON_PRESS_CHANCE) == 0)
    {
      char argument[500];
      strlcpy(argument, "button", 10);
      ch->in_room->func(ch, ch->in_room, find_command("push"), argument);
      return TRUE;
    }

    // NPC in an elevator? Maybe they feel like pressing buttons.
    if (ch->in_room->func == elevator_spec && number(0, 6) == 0) {
      // Yeah, they do. Push that thang.
      int num;
      for (num = 0; num < num_elevators; num++) {
        if (ch->in_room->number == elevator[num].room)
          break;
      }
      if (num == num_elevators) {
        mudlog("ERROR: Failed to find the elevator an NPC was standing in.", ch, LOG_SYSLOG, TRUE);
        return FALSE;
      }

      // If the elevator's not moving...
      if (elevator[num].dir != UP && elevator[num].dir != DOWN) {
        // Select the floor. Make sure it's not the current floor.
        int target_floor = -31337; // yes, it's a magic number, hush.
        for (int loop_limit = 10; loop_limit > 0; loop_limit--) {
          target_floor = number(elevator[num].start_floor, elevator[num].start_floor + elevator[num].num_floors);
          if (target_floor != ch->in_room->rating)
            break;
          target_floor = -31337;
        }

        if (target_floor != -31337) {
          // We found a valid button to push. Push it.
          char push_arg[10];
          if (target_floor == 0)
            strcpy(push_arg, "g");
          else if (target_floor < 0)
            snprintf(push_arg, sizeof(push_arg), "b%d", -target_floor);
          else
            snprintf(push_arg, sizeof(push_arg), "%d", target_floor);

          process_elevator(ch->in_room, ch, get_push_command_index(), push_arg);
          return TRUE;
        }
      }
    }

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
  return FALSE;
}

bool mob_has_ammo_for_weapon(struct char_data *ch, struct obj_data *weapon) {
  for (int am = 0; am < NUM_AMMOTYPES; am++)
    if (GET_BULLETPANTS_AMMO_AMOUNT(ch, GET_WEAPON_ATTACK_TYPE(weapon), am) > 0) {
      return TRUE;
    }

  return FALSE;
}

// Precondition: ch and weapon exist, and weapon is a firearm.
void ensure_mob_has_ammo_for_weapon(struct char_data *ch, struct obj_data *weapon) {
  // Check all ammotypes for this weapon. If we have any, refill it to full, then return.
  for (int am = 0; am < NUM_AMMOTYPES; am++)
    if (GET_BULLETPANTS_AMMO_AMOUNT(ch, GET_WEAPON_ATTACK_TYPE(weapon), am) > 0) {
      GET_BULLETPANTS_AMMO_AMOUNT(ch, GET_WEAPON_ATTACK_TYPE(weapon), am) = MAX(GET_BULLETPANTS_AMMO_AMOUNT(ch, GET_WEAPON_ATTACK_TYPE(weapon), am),
                                                                                GET_WEAPON_MAX_AMMO(weapon) * NUMBER_OF_MAGAZINES_TO_GIVE_TO_UNEQUIPPED_MOBS);
      return;
    }

  // If it's an assault cannon, give them EX.
  GET_BULLETPANTS_AMMO_AMOUNT(ch, GET_WEAPON_ATTACK_TYPE(weapon), AMMO_EX) = GET_WEAPON_MAX_AMMO(weapon) * NUMBER_OF_MAGAZINES_TO_GIVE_TO_UNEQUIPPED_MOBS;

  // If we had none, give them normal.
  GET_BULLETPANTS_AMMO_AMOUNT(ch, GET_WEAPON_ATTACK_TYPE(weapon), AMMO_NORMAL) = GET_WEAPON_MAX_AMMO(weapon) * NUMBER_OF_MAGAZINES_TO_GIVE_TO_UNEQUIPPED_MOBS;
}

ACMD_DECLARE(do_get);
void mobile_activity(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct char_data *ch, *next_ch;
  int dir, distance;
  struct room_data *current_room = NULL;

  extern int no_specials;

  // Iterate through all characters in the game.
  for (ch = character_list; ch; ch = next_ch) {
    next_ch = ch->next;

    // Skip them if they're a player character, are sleeping, or are operated by a player (ex: projections, possessed).
    if (!IS_NPC(ch) || !AWAKE(ch) || ch->desc)
      continue;

    // Skip broken-ass characters.
    if (!ch->in_room && !ch->in_veh) {
      snprintf(buf, sizeof(buf), "SYSERR: Encountered char '%s' with no room, no veh in mobile_activity().", GET_CHAR_NAME(ch));
      mudlog(buf, NULL, LOG_SYSLOG, TRUE);
      continue;
    }

    if (ch->nr == 0) {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Encountered zeroed char %s (%ld) in mobile_activity().", GET_CHAR_NAME(ch), GET_MOB_VNUM(ch));
      continue;
    }

    current_room = get_ch_in_room(ch);

    // Skip them if they have no current room.
    if (!current_room)
      continue;

    // Skip NPCs that are currently fighting. This state is removed in the combat code.
    if (FIGHTING(ch) || FIGHTING_VEH(ch))
      continue;

    // Cool down mob alert status.
    if (GET_MOBALERT(ch) > MALERT_CALM && --GET_MOBALERTTIME(ch) <= 0) {
      GET_MOBALERT(ch) = MALERT_CALM;

      // If you just entered calm state and have a weapon, you get your ammo back.
      // check if they have ammo in proto-- if so, paste in proto ammo, if not, give max * 3 normal ammo
      // real_mobile is guaranteed to resolve here since we're referencing a vnum from an existing NPC
      struct char_data *proto_mob = &mob_proto[real_mobile(GET_MOB_VNUM(ch))];

      // Copy over their ammo data. We scan the whole thing instead of just their weapon to prevent someone
      // giving them a holdout pistol so they never regain their ammo stores.
      for (int wp = START_OF_AMMO_USING_WEAPONS; wp <= END_OF_AMMO_USING_WEAPONS; wp++)
        for (int am = 0; am < NUM_AMMOTYPES; am++)
          GET_BULLETPANTS_AMMO_AMOUNT(ch, wp, am) = GET_BULLETPANTS_AMMO_AMOUNT(proto_mob, wp, am);

      // Carried weapons.
      for (struct obj_data *weapon = ch->carrying; weapon; weapon = weapon->next_content)
        if (GET_OBJ_TYPE(weapon) == ITEM_WEAPON && IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon)) && GET_WEAPON_MAX_AMMO(weapon) > 0)
          ensure_mob_has_ammo_for_weapon(ch, weapon);

      // Wielded weapons.
      for (int index = 0; index < NUM_WEARS; index++)
        if (GET_EQ(ch, index)
            && GET_OBJ_TYPE(GET_EQ(ch, index)) == ITEM_WEAPON
            && IS_GUN(GET_WEAPON_ATTACK_TYPE(GET_EQ(ch, index)))
            && GET_WEAPON_MAX_AMMO(GET_EQ(ch, index)) > 0)
          ensure_mob_has_ammo_for_weapon(ch, GET_EQ(ch, index));
    }

    // Confirm we have the skills to wield our current weapon, otherwise ditch it.
#ifndef SUPPRESS_MOB_SKILL_ERRORS
    if (GET_EQ(ch, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_WEAPON) {
      char build_err_msg[2000];

      int weapon_skill = GET_WEAPON_SKILL(GET_EQ(ch, WEAR_WIELD));
      int melee_skill = does_weapon_have_bayonet(GET_EQ(ch, WEAR_WIELD)) ? SKILL_POLE_ARMS : SKILL_CLUBS;

      int weapon_skill_dice = GET_SKILL(ch, weapon_skill) ? GET_SKILL(ch, weapon_skill) : GET_SKILL(ch, return_general(weapon_skill));
      int melee_skill_dice = GET_SKILL(ch, melee_skill) ? GET_SKILL(ch, melee_skill) : GET_SKILL(ch, return_general(melee_skill));

      int indexed_attack_type = MAX(0, MIN(MAX_WEAP - 1, GET_WEAPON_ATTACK_TYPE(GET_EQ(ch, WEAR_WIELD))));

      if (weapon_skill_dice <= 0) {
        #ifndef SUPPRESS_BUILD_ERROR_MESSAGES
        snprintf(build_err_msg, sizeof(build_err_msg), "CONTENT ERROR: %s (%ld) is wielding %s %s, but has no weapon skill in %s!",
                 GET_CHAR_NAME(ch),
                 GET_MOB_VNUM(ch),
                 AN(weapon_types[indexed_attack_type]),
                 weapon_types[indexed_attack_type],
                 skills[GET_WEAPON_SKILL(GET_EQ(ch, WEAR_WIELD))].name
               );
        mudlog(build_err_msg, ch, LOG_MISCLOG, TRUE);
        #endif

        switch_weapons(ch, WEAR_WIELD);
      } else if (IS_GUN(GET_WEAPON_ATTACK_TYPE(GET_EQ(ch, WEAR_WIELD))) && melee_skill_dice <= 0 && weapon_skill_dice >= 5) {
        #ifndef SUPPRESS_BUILD_ERROR_MESSAGES
        snprintf(build_err_msg, sizeof(build_err_msg), "CONTENT ERROR: Skilled mob %s (%ld) is wielding %s %s%s, but has no melee skill in %s!",
                 GET_CHAR_NAME(ch),
                 GET_MOB_VNUM(ch),
                 AN(weapon_types[indexed_attack_type]),
                 weapon_types[indexed_attack_type],
                 melee_skill == SKILL_POLE_ARMS ? " (with bayonet)" : "",
                 skills[melee_skill].name
               );
        mudlog(build_err_msg, ch, LOG_MISCLOG, TRUE);
        #endif
      }
    }
#endif

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
    if (!current_room->peaceful) {
      // Handle aggressive mobs.
      if (mobact_process_aggro(ch, current_room)) {
        continue;
      }

      // Guard NPCs.
      if (MOB_FLAGGED(ch, MOB_GUARD) && mobact_process_guard(ch, current_room)) {
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
            // Check each room in a straight line until we are either out of range or cannot go further.
            current_room = get_ch_in_room(ch);

            for (distance = 1; !has_acted && distance <= max_distance; distance++) {
              // Exit must be valid, and room must belong to same zone as character's room.
              if (CAN_GO2(current_room, dir) && EXIT2(current_room, dir)->to_room->zone == ch->in_room->zone) {
                current_room = EXIT2(current_room, dir)->to_room;
              } else {
                // If we can't get to a further room, stop and move to next direction in for loop.
                break;
              }

              // No shooting into peaceful rooms.
              if (current_room->peaceful)
                continue;

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

          if (has_acted)
            continue;
        }
      }
    } /* End NPC-is-not-in-a-vehicle section. */

    /**********************************************************************\
      If we've gotten here, the NPC has not acted and has no valid targets.
      Add passive things like self-buffing and idle actions here.
    \**********************************************************************/

    // Not in combat? Put your weapon away, assuming you can.
    if (mobact_holster_weapon(ch)) {
      continue;
    }

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
  if (success_test(skill, GET_LEGAL_NUM(obj)) > 0)
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

  for (tmp = GET_MOB_MEMORY(ch); tmp && !present; tmp = tmp->next)
    if (tmp->id == GET_IDNUM(victim))
      present = TRUE;

  if (!present)
  {
    tmp = new memory_rec;
    tmp->next = GET_MOB_MEMORY(ch);
    tmp->id = GET_IDNUM(victim);
    GET_MOB_MEMORY(ch) = tmp;
  }
}

/* make ch forget victim */
void forget(struct char_data * ch, struct char_data * victim)
{
  memory_rec *curr, *prev = NULL;

  if (!(curr = GET_MOB_MEMORY(ch)))
    return;

  while (curr && curr->id != GET_IDNUM(victim))
  {
    prev = curr;
    curr = curr->next;
  }

  if (!curr)
    return;                     /* person wasn't there at all. */

  if (curr == GET_MOB_MEMORY(ch))
    GET_MOB_MEMORY(ch) = curr->next;
  else
    prev->next = curr->next;

  delete curr;
}

/* check ch's memory for victim */
bool memory(struct char_data *ch, struct char_data *vict)
{
  memory_rec *names;

  for (names = GET_MOB_MEMORY(ch); names; names = names->next)
    if (names->id == GET_IDNUM(vict))
      return(TRUE);

  return(FALSE);
}

/* erase ch's memory */
void clearMemory(struct char_data * ch)
{
  memory_rec *curr, *next;

  curr = GET_MOB_MEMORY(ch);

  while (curr)
  {
    next = curr->next;
    delete curr;
    curr = next;
  }

  GET_MOB_MEMORY(ch) = NULL;
}
bool attempt_reload(struct char_data *mob, int pos)
{
  // I would call the reload routine for players, but this is slightly faster
  struct obj_data *gun = NULL;

  if (!(gun = GET_EQ(mob, pos))) {
    snprintf(buf, sizeof(buf), "SYSERR: attempt_reload received invalid wield position %d for %s.", pos, GET_CHAR_NAME(mob));
    mudlog(buf, mob, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (GET_OBJ_TYPE(gun) != ITEM_WEAPON) {
    // act("$n can't reload weapon- $o is not a weapon.", TRUE, mob, gun, NULL, TO_ROLLS);
    return FALSE;
  }

  // Unlimited ammo weapons don't need reloads.
  if (GET_WEAPON_MAX_AMMO(gun) == -1) {
    return TRUE;
  }

  // Reload with whatever we have on hand.
  for (int index = 0; index < NUM_AMMOTYPES; index++)
    if (GET_BULLETPANTS_AMMO_AMOUNT(mob, GET_WEAPON_ATTACK_TYPE(gun), npc_ammo_usage_preferences[index]) > 0) {
      // act("$n has ammo, attempting to reload $o.", TRUE, mob, gun, NULL, TO_ROLLS);
      return reload_weapon_from_bulletpants(mob, gun, npc_ammo_usage_preferences[index]);
    }

  // We had nothing.
  // act("$n can't reload weapon- have no ammo for $o.", TRUE, mob, gun, NULL, TO_ROLLS);
  return FALSE;
}

void switch_weapons(struct char_data *mob, int pos)
{
  struct obj_data *i, *unlimited_ammo_weapon = NULL, *limited_ammo_weapon = NULL, *melee_weapon = NULL;

  if (!GET_EQ(mob, pos)) {
    act("$n won't switch weapons- not equipped.", TRUE, mob, NULL, NULL, TO_ROLLS);
    return;
  }

  for (i = mob->carrying; i; i = i->next_content)
  {
    /* No idea what these used to be, but now they're max ammo and reach.
    // search for a new gun
    if (GET_OBJ_TYPE(i) == ITEM_WEAPON)
      if (GET_OBJ_VAL(i, 6) > 0)
        temp = i;
    if (GET_OBJ_TYPE(i) == ITEM_WEAPON)
      if (GET_OBJ_VAL(i, 5) == -1)
        temp2 = i;
    */


    // Classify our weapons by ammo usage and type. Only allow weapons we can actually use, though.
    if (GET_OBJ_TYPE(i) == ITEM_WEAPON && (GET_SKILL(mob, GET_WEAPON_SKILL(i)) > 0 || GET_SKILL(mob, return_general(GET_WEAPON_SKILL(i))) > 0)) {
      if (IS_GUN(GET_WEAPON_ATTACK_TYPE(i))) {
        // Check for an unlimited-ammo weapon.
        if (GET_WEAPON_MAX_AMMO(i) == -1) {
          // TODO: Check if it's a better weapon.
          unlimited_ammo_weapon = i;
          continue;
        }

        // It's a limited-ammo weapon. Check to see if we have ammo for it.
        if ((i->contains && GET_MAGAZINE_AMMO_COUNT(i->contains) > 0)
            || mob_has_ammo_for_weapon(mob, i)) {
              // TODO: Check if it's a better weapon.
              limited_ammo_weapon = i;
              break;
            }

      } else {
        // It's a melee weapon (or a bow, grenade, etc that we don't support).
        // TODO: Check for a better weapon.
        melee_weapon = i;
        continue;
      }
    }
  }

  perform_remove(mob, pos);

  // We want to wield limited-ammo weapons first to burn those ammo counts down.
  if (limited_ammo_weapon)
    perform_wear(mob, limited_ammo_weapon, pos, TRUE);
  else if (unlimited_ammo_weapon)
    perform_wear(mob, unlimited_ammo_weapon, pos, TRUE);
  else if (melee_weapon)
    perform_wear(mob, melee_weapon, pos, TRUE);
  else
    act("$n won't wield a new weapon- no alternative weapon found.", TRUE, mob, GET_EQ(mob, pos), NULL, TO_ROLLS);
}

bool mob_cannot_be_aggressive(struct char_data *ch) {
  // Conjured spirits and elementals are never aggressive.
  if ((IS_PC_CONJURED_ELEMENTAL(ch) || IS_SPIRIT(ch)) && GET_ACTIVE(ch))
    return TRUE;

  // Anything that's spec-protected can't fight (shopkeepers, Johnsons, etc).
  if (npc_is_protected_by_spec(ch))
    return TRUE;

  // Escortees don't hit first.
  if (is_escortee(ch))
    return TRUE;

  return FALSE;
}

bool mob_is_aggressive_towards_race(struct char_data *ch, int race) {
  switch (race) {
    case RACE_ELF:
    case RACE_WAKYAMBI:
    case RACE_NIGHTONE:
    case RACE_DRYAD:
      return MOB_FLAGGED(ch, MOB_AGGR_ELF);
    case RACE_DWARF:
    case RACE_GNOME:
    case RACE_MENEHUNE:
    case RACE_KOBOROKURU:
      return MOB_FLAGGED(ch, MOB_AGGR_DWARF);
    case RACE_HUMAN:
      return MOB_FLAGGED(ch, MOB_AGGR_HUMAN);
    case RACE_ORK:
    case RACE_HOBGOBLIN:
    case RACE_OGRE:
    case RACE_SATYR:
    case RACE_ONI:
      return MOB_FLAGGED(ch, MOB_AGGR_ORK);
    case RACE_TROLL:
    case RACE_CYCLOPS:
    case RACE_FOMORI:
    case RACE_GIANT:
    case RACE_MINOTAUR:
      return MOB_FLAGGED(ch, MOB_AGGR_TROLL);
  }

  return FALSE;
}
