#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <queue>

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "db.h"
#include "screen.h"
#include "newmagic.h"
#include "constants.h"
#include "config.h"
#include "memory.h"
#include "bullet_pants.h"

extern int check_recoil(struct char_data *ch, struct obj_data *gun);
extern void die(struct char_data * ch);
extern bool astral_fight(struct char_data *ch, struct char_data *vict);
extern void dominator_mode_switch(struct char_data *ch, struct obj_data *obj, int mode);
extern int calculate_vision_penalty(struct char_data *ch, struct char_data *victim);
extern int find_sight(struct char_data *ch);
extern int find_weapon_range(struct char_data *ch, struct obj_data *weapon);
extern bool has_ammo(struct char_data *ch, struct obj_data *wielded);
extern bool has_ammo_no_deduct(struct char_data *ch, struct obj_data *wielded);
extern void combat_message(struct char_data *ch, struct char_data *victim, struct obj_data *weapon, int damage, int burst);
extern int check_smartlink(struct char_data *ch, struct obj_data *weapon);
extern bool can_hurt(struct char_data *ch, struct char_data *victim, int attacktype, bool include_func_protections);
extern int get_weapon_damage_type(struct obj_data* weapon);
extern bool is_char_too_tall(struct char_data *ch);
extern bool damage(struct char_data *ch, struct char_data *victim, int dam, int attacktype, bool is_physical);
extern bool damage_without_message(struct char_data *ch, struct char_data *victim, int dam, int attacktype, bool is_physical);

void engage_close_combat_if_appropriate(struct combat_data *att, struct combat_data *def, int net_reach);

bool handle_flame_aura(struct combat_data *att, struct combat_data *def);

bool does_weapon_have_bayonet(struct obj_data *weapon);
bool perform_nerve_strike(struct combat_data *att, struct combat_data *def, char *rbuf, size_t rbuf_len);

#define SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER {act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS ); if (att->ch->in_room != def->ch->in_room) act( rbuf, 1, def->ch, NULL, NULL, TO_ROLLS );}

#define IS_RANGED(eq)   (GET_OBJ_TYPE(eq) == ITEM_FIREWEAPON || \
(GET_OBJ_TYPE(eq) == ITEM_WEAPON && \
(IS_GUN(GET_OBJ_VAL(eq, 3)))))

SPECIAL(weapon_dominator);
WSPEC(monowhip);

struct cyberware_data {
  int climbingclaws;
  int fins;
  int handblades;
  int handrazors;
  int improved_handrazors;
  int handspurs;
  int footanchors;
  bool cyberarms;
  int bone_lacing_power;
  int num_cyberweapons;
  bool cyberarm_gyromount;

  cyberware_data(struct char_data *ch) :
    climbingclaws(0), fins(0), handblades(0), handrazors(0), improved_handrazors(0),
    handspurs(0), footanchors(0), cyberarms(FALSE), bone_lacing_power(0), num_cyberweapons(0),
    cyberarm_gyromount(FALSE)
  {
    assert(ch != NULL);

    // Populate the data.
    for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content) {
      if (GET_CYBERWARE_TYPE(obj) == CYB_BONELACING) {
        switch (GET_CYBERWARE_LACING_TYPE(obj)) {
          case BONE_PLASTIC:
            bone_lacing_power = MAX(bone_lacing_power, 2);
            break;
          case BONE_ALUMINIUM:
          case BONE_CERAMIC:
            bone_lacing_power = MAX(bone_lacing_power, 3);
            break;
          case BONE_TITANIUM:
            bone_lacing_power = MAX(bone_lacing_power, 4);
            break;
        }
      } else if (GET_CYBERWARE_TYPE(obj) == CYB_ARMS) {
        cyberarms = TRUE;
        if (IS_SET(GET_CYBERWARE_FLAGS(obj), ARMS_MOD_GYROMOUNT)) {
          cyberarm_gyromount = TRUE;
        }
      } else if (!GET_CYBERWARE_IS_DISABLED(obj)) {
        switch (GET_CYBERWARE_TYPE(obj)) {
          case CYB_CLIMBINGCLAWS:
            climbingclaws++;
            num_cyberweapons++;
            break;
          case CYB_FIN:
            fins++;
            num_cyberweapons++;
            break;
          case CYB_HANDBLADE:
            handblades++;
            num_cyberweapons++;
            break;
          case CYB_HANDRAZOR:
            if (IS_SET(GET_CYBERWARE_FLAGS(obj), 1 << CYBERWEAPON_IMPROVED))
              improved_handrazors++;
            else
              handrazors++;
            num_cyberweapons++;
            break;
          case CYB_HANDSPUR:
            handspurs++;
            num_cyberweapons++;
            break;
          case CYB_FOOTANCHOR:
            footanchors++;
            num_cyberweapons++;
            break;
        }
      }
    }
  }
};

struct ranged_combat_data {
  int skill;
  int power;
  int dam_type;
  int damage_level;
  bool is_physical;
  int tn;
  int dice;
  int successes;
  bool is_gel;

  int burst_count;
  int recoil_comp;

  bool using_mounted_gun;

  int modifiers[NUM_COMBAT_MODIFIERS];

  struct obj_data *magazine;
  struct obj_data *gyro;

  ranged_combat_data(struct char_data *ch, struct obj_data *weapon, bool ranged_combat_mode) :
    skill(0), power(0), dam_type(0), damage_level(0), is_physical(FALSE),
    tn(4), dice(0), successes(0), is_gel(FALSE), burst_count(0), recoil_comp(0),
    using_mounted_gun(FALSE), magazine(NULL), gyro(NULL)
  {
    for (int i = 0; i < NUM_COMBAT_MODIFIERS; i++)
      modifiers[i] = 0;

    assert(ch != NULL);

    // Setup: Cyclops suffer a +2 ranged-combat modifier.
    if (GET_RACE(ch) == RACE_CYCLOPS)
      modifiers[COMBAT_MOD_DISTANCE] += 2;

    if (weapon && ranged_combat_mode) {
      // Extract our various fields from the weapon.
      skill = GET_WEAPON_SKILL(weapon);
      power = GET_WEAPON_POWER(weapon);
      dam_type = get_weapon_damage_type(weapon);
      damage_level = GET_WEAPON_DAMAGE_CODE(weapon);
      is_physical = IS_DAMTYPE_PHYSICAL(dam_type);

      // Gunners use the gunnery skill.
      if ((using_mounted_gun = (AFF_FLAGGED(ch, AFF_MANNING) || AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE)))) {
        skill = SKILL_GUNNERY;
      } else {
        // Since they're not using a mounted weapon, check for a gyro.
        for (int q = 0; q < NUM_WEARS; q++) {
          if (GET_EQ(ch, q) && GET_OBJ_TYPE(GET_EQ(ch, q)) == ITEM_GYRO) {
            gyro = GET_EQ(ch, q);
            break;
          }
        }
      }

      // Get a pointer to the magazine.
      magazine = weapon->contains;

      // Determine the initial burst value of the weapon.
      if (WEAPON_IS_BF(weapon))
        burst_count = 3;
      else if (WEAPON_IS_FA(weapon))
        burst_count = GET_OBJ_TIMER(weapon);

      // Calculate recoil comp.
      recoil_comp = check_recoil(ch, weapon);

      // Setup: If you're dual-wielding, take that penalty, otherwise you get your smartlink bonus.
      if (GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD))
        modifiers[COMBAT_MOD_DUAL_WIELDING] = 2;
      else
        modifiers[COMBAT_MOD_SMARTLINK] -= check_smartlink(ch, weapon);
    }
  }
};

struct melee_combat_data {
  int skill;
  int skill_bonus;
  int power;
  int dam_type;
  int damage_level;
  bool is_physical;
  int tn;
  int dice;
  int successes;
  bool is_monowhip;

  int modifiers[NUM_COMBAT_MODIFIERS];

  melee_combat_data(struct char_data *ch, struct obj_data *weapon, bool ranged_combat_mode, struct cyberware_data *cyber) :
    skill(0), skill_bonus(0), power(0), dam_type(0), damage_level(0), is_physical(FALSE), tn(4), dice(0), successes(0), is_monowhip(FALSE)
  {
    assert(ch != NULL);

    for (int i = 0; i < NUM_COMBAT_MODIFIERS; i++)
      modifiers[i] = 0;

    // Set up melee combat data. This holds true for all melee combat, but can be overwritten later on.
    skill = SKILL_UNARMED_COMBAT;
    power = GET_STR(ch);
    dam_type = TYPE_HIT;
    damage_level = MODERATE;

    if (weapon) {
      if (ranged_combat_mode) {
        if (does_weapon_have_bayonet(weapon)) {
          dam_type = TYPE_PIERCE;
          skill = SKILL_POLE_ARMS;
        } else {
          dam_type = TYPE_POUND;
          skill = SKILL_CLUBS;
        }
      } else {
        dam_type = get_weapon_damage_type(weapon);
        skill = GET_WEAPON_SKILL(weapon);
        power = GET_WEAPON_POWER(weapon);
        damage_level = GET_WEAPON_DAMAGE_CODE(weapon);

        // Weapon foci. NPC use them implicitly.
        if (IS_NPC(ch) || WEAPON_FOCUS_USABLE_BY(weapon, ch)) {
          skill_bonus = MIN(4, GET_WEAPON_FOCUS_RATING(weapon));
        }

        // Monowhips.
        is_monowhip = obj_index[GET_OBJ_RNUM(weapon)].wfunc == monowhip;
      }
    } else if (cyber->num_cyberweapons > 0) {
      skill = SKILL_CYBER_IMPLANTS;

      // Select the best cyberweapon and use its stats.
      if (cyber->handblades) {
        power += 3;
        damage_level = LIGHT;
        dam_type = TYPE_STAB;
        is_physical = TRUE;
      }
      else if (cyber->handspurs) {
        damage_level = MODERATE;
        dam_type = TYPE_SLASH;
        is_physical = TRUE;

        if (cyber->handspurs >= 2) {
          // Dual cyberweapons gives a power bonus per Core p121.
          power += (int) (GET_STR(ch) / 2);
        }
      }
      else if (cyber->improved_handrazors) {
        power += 2;
        damage_level = LIGHT;
        dam_type = TYPE_STAB;
        is_physical = TRUE;

        if (cyber->improved_handrazors >= 2) {
          // Dual cyberweapons gives a power bonus per Core p121.
          power += (int) (GET_STR(ch) / 2);
        }
      }
      else if (cyber->handrazors) {
        damage_level = LIGHT;
        dam_type = TYPE_STAB;
        is_physical = TRUE;

        if (cyber->handrazors >= 2) {
          // Dual cyberweapons gives a power bonus per Core p121.
          power += (int) (GET_STR(ch) / 2);
        }
      }
      else if (cyber->fins || cyber->climbingclaws) {
        power -= 1;
        damage_level = LIGHT;
        dam_type = TYPE_SLASH;
        is_physical = TRUE;
      }
      else if (cyber->footanchors) {
        power -= 1;
        damage_level = MODERATE;
        dam_type = TYPE_STAB;
        is_physical = TRUE;
      }
      else {
        snprintf(buf, sizeof(buf), "SYSERR in hit(): num_cyberweapons %d but no weapons found.", cyber->num_cyberweapons);
        mudlog(buf, ch, LOG_SYSLOG, TRUE);
      }
    } else {
      // TODO: Implement option to use bone lacing to cause physical damage at half power.
      // No cyberweapons active-- check for bone lacing, then proceed with adept/standard slapping.
      if (cyber->bone_lacing_power) {
        power += cyber->bone_lacing_power;
        damage_level = MODERATE;
        is_physical = FALSE;
      }

      // Add +2 to unarmed attack power for having cyberarms, per M&M p32.
      if (cyber->cyberarms) {
        power += 2;
      }

      // Add +2 power to unarmed/melee, per MitS p147. -Vile
      if (AFF_FLAGGED(ch, AFF_FLAME_AURA) || MOB_FLAGGED(ch, MOB_FLAMEAURA)) {
        power += 2;
      }

      // Check for Adept powers.
      // TODO: Elemental Strike (SotA64 p65)

      // Apply Killing Hands.
      if (GET_POWER(ch, ADEPT_KILLING_HANDS)) {
        damage_level = GET_POWER(ch, ADEPT_KILLING_HANDS);
        is_physical = TRUE;
      }
    }

    is_physical = is_physical || IS_DAMTYPE_PHYSICAL(dam_type);
  }
};

/* Combat data. */
struct combat_data
{
  // Pointers.
  struct char_data *ch;
  struct veh_data *veh;
  struct obj_data *weapon;

  // Gun data.
  bool ranged_combat_mode;

  struct cyberware_data *cyber;
  struct ranged_combat_data *ranged;
  struct melee_combat_data *melee;

  // Generic combat data.
  bool too_tall;

  combat_data(struct char_data *character, struct obj_data *weap) :
    ch(NULL),
    veh(NULL),
    weapon(NULL),
    ranged_combat_mode(FALSE),
    too_tall(FALSE)
  {
    ch = character;

    assert(ch != NULL);

    too_tall = is_char_too_tall(ch);

    weapon = weap;
    ranged_combat_mode = weapon
                    && IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))
                    && (GET_WEAPON_SKILL(weapon) >= SKILL_PISTOLS
                        && GET_WEAPON_SKILL(weapon) <= SKILL_ASSAULT_CANNON);

    cyber = new struct cyberware_data(ch);
    ranged = new struct ranged_combat_data(ch, weapon, ranged_combat_mode);
    melee = new struct melee_combat_data(ch, weapon, ranged_combat_mode, cyber);

    // Special case: Bayonet charge.
    if (ranged_combat_mode && !weapon->contains && does_weapon_have_bayonet(weapon))
      ranged_combat_mode = FALSE;
  }

  ~combat_data() {
    delete cyber;
    delete ranged;
    delete melee;
  }
};

bool hit_with_multiweapon_toggle(struct char_data *attacker, struct char_data *victim, struct obj_data *weap, struct obj_data *vict_weap, struct obj_data *weap_ammo, bool multi_weapon_modifier)
{
  int net_successes, successes_for_use_in_monowhip_test_check;
  assert(attacker != NULL);
  assert(victim != NULL);

  // Initialize our data structures for holding this round's fight-related data.
  struct combat_data attacker_data(attacker, weap);
  struct combat_data defender_data(victim, vict_weap);

  // Allows for switching roles, which can happen during melee counterattacks.
  struct combat_data *att = &attacker_data;
  struct combat_data *def = &defender_data;

  char rbuf[MAX_STRING_LENGTH];
  memset(rbuf, 0, sizeof(rbuf));

  snprintf(rbuf, sizeof(rbuf), ">> ^cCombat eval: %s vs %s.", GET_CHAR_NAME(attacker), GET_CHAR_NAME(victim));
  SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;

  // Short-circuit: If you're wielding an activated Dominator, you don't care about all these pesky rules.
  if (att->weapon && GET_OBJ_SPEC(att->weapon) == weapon_dominator) {
    if (GET_LEVEL(def->ch) > GET_LEVEL(att->ch)) {
      send_to_char(att->ch, "As you aim your weapon at %s, a dispassionate feminine voice states: \"^cThe target's Crime Coefficient is below 100. %s is not a target for enforcement. The trigger has been locked.^n\"\r\n", GET_CHAR_NAME(def->ch), HSSH(def->ch));
      dominator_mode_switch(att->ch, att->weapon, DOMINATOR_MODE_DEACTIVATED);
      return FALSE;
    }
    switch (GET_WEAPON_ATTACK_TYPE(att->weapon)) {
      case WEAP_TASER:
        // Paralyzer? Stun your target.
        act("Your crackling shot of energy slams into $N, disabling $M.", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("A crackling shot of energy erupts from $n's Dominator and slams into $N, disabling $M!", FALSE, att->ch, 0, def->ch, TO_NOTVICT);
        act("A crackling shot of energy erupts from $n's Dominator and slams into you! Your vision fades as your muscles lock up.", FALSE, att->ch, 0, def->ch, TO_VICT);
        GET_MENTAL(def->ch) = -10;
        return FALSE;
      case WEAP_HEAVY_PISTOL:
        // Lethal? Kill your target.
        act("A ball of coherent light leaps from your Dominator, tearing into $N. With a scream, $E crumples, bubbles, and explodes in a shower of gore!", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("A ball of coherent light leaps from $n's Dominator, tearing into $N. With a scream, $E crumples, bubbles, and explodes in a shower of gore!", FALSE, att->ch, 0, def->ch, TO_NOTVICT);
        act("A ball of coherent light leaps from $n's Dominator, tearing into you! A horrible rending sensation tears through you as your vision fades.", FALSE, att->ch, 0, def->ch, TO_VICT);
        die(def->ch);
        return TRUE;
      case WEAP_CANNON:
        // Decomposer? Don't just kill your target-- if they're a player, disconnect them.
        act("A roaring column of force explodes from your Dominator, erasing $N from existence!", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("A roaring column of force explodes from $n's Dominator, erasing $N from existence!", FALSE, att->ch, 0, def->ch, TO_NOTVICT);
        act("A roaring column of force explodes from $n's Dominator, erasing you from existence!", FALSE, att->ch, 0, def->ch, TO_VICT);
        die(def->ch);
        if (def->ch->desc) {
          STATE(def->ch->desc) = CON_CLOSE;
          close_socket(def->ch->desc);
        }
        return TRUE;
      default:
        send_to_char(att->ch, "Dominator code fault.\r\n");
        break;
    }
    return FALSE;
  }

  // Precondition: If you're wielding a non-weapon, back out.
  if (att->weapon && (GET_OBJ_TYPE(att->weapon) != ITEM_WEAPON)) {
    send_to_char(att->ch, "You struggle to figure out how to attack while using %s as a weapon!\r\n", decapitalize_a_an(GET_OBJ_NAME(att->weapon)));
    return FALSE;
  }

  // Precondition: If you're asleep or paralyzed, you don't get to fight, and also your opponent closes immediately.
  if (!AWAKE(att->ch) || GET_QUI(att->ch) <= 0) {
    AFF_FLAGS(att->ch).RemoveBit(AFF_APPROACH);
    AFF_FLAGS(def->ch).RemoveBit(AFF_APPROACH);

    if (AWAKE(att->ch)) {
      send_to_char("You can't react-- you're paralyzed!\r\n", att->ch);
    }
    return FALSE;
  }

  // Precondition: If you're out of ammo, you don't get to fight. Note the use of the deducting has_ammo here.
  if (att->weapon && !has_ammo(att->ch, att->weapon))
    return FALSE;

  // Precondition: If your foe is astral (ex: a non-manifested projection, a dematerialized spirit), you don't belong here.
  if (IS_ASTRAL(def->ch)) {
    if (IS_DUAL(att->ch) || IS_ASTRAL(att->ch)) {
      return astral_fight(att->ch, def->ch);
    } else {
      mudlog("SYSERR: Entered hit() with an non-astrally-reachable character attacking an astral character.", att->ch, LOG_SYSLOG, TRUE);
      act("Unable to hit $N- $E's astral and $n can't touch that.", FALSE, att->ch, 0, def->ch, TO_ROLLS);
    }
    return FALSE;
  }

  // Precondition: Same for if you're an astral being and your target isn't.
  if (IS_ASTRAL(att->ch)) {
    if (IS_DUAL(def->ch) || IS_ASTRAL(def->ch)) {
      astral_fight(att->ch, def->ch);
    } else {
      mudlog("SYSERR: Entered hit() with an astral character attacking a non-astrally-reachable character.", att->ch, LOG_SYSLOG, TRUE);
      act("Unable to hit $N- $E's unreachable from the astral plane and $n can't touch that.", FALSE, att->ch, 0, def->ch, TO_ROLLS);
    }
    return FALSE;
  }

  // Precondition: If you're in melee combat and your foe isn't present, stop fighting.
  if (!att->ranged_combat_mode && att->ch->in_room != def->ch->in_room) {
    send_to_char(att->ch, "You relax with the knowledge that your opponent is no longer present.\r\n");
    stop_fighting(att->ch);
    return FALSE;
  }

  // Remove closing flags if both are melee.
  if ((!att->ranged_combat_mode || AFF_FLAGGED(att->ch, AFF_APPROACH))
      && (!def->ranged_combat_mode || AFF_FLAGGED(def->ch, AFF_APPROACH)))
  {
    AFF_FLAGS(att->ch).RemoveBit(AFF_APPROACH);
    AFF_FLAGS(def->ch).RemoveBit(AFF_APPROACH);
  }

  // Setup: Calculate sight penalties.
  att->melee->modifiers[COMBAT_MOD_VISIBILITY] += calculate_vision_penalty(att->ch, def->ch);
  att->ranged->modifiers[COMBAT_MOD_VISIBILITY] += calculate_vision_penalty(att->ch, def->ch);
  def->melee->modifiers[COMBAT_MOD_VISIBILITY] += calculate_vision_penalty(def->ch, att->ch);
  def->ranged->modifiers[COMBAT_MOD_VISIBILITY] += calculate_vision_penalty(def->ch, att->ch);

  // Setup: If the character is rigging a vehicle or is in a vehicle, set veh to that vehicle.
  RIG_VEH(att->ch, att->veh);

  // Setup: If the character is firing multiple rigged weapons, apply the dual-weapon penalty.
  if (multi_weapon_modifier) {
    att->ranged->modifiers[COMBAT_MOD_DUAL_WIELDING] = 2;
    att->ranged->modifiers[COMBAT_MOD_SMARTLINK] = 0;
  }

  if (att->veh && !att->weapon) {
    mudlog("SYSERR: Somehow, we ended up in a vehicle attacking someone with no weapon!", att->ch, LOG_SYSLOG, TRUE);
    send_to_char("You'll have to leave your vehicle for that.\r\n", att->ch);
    return FALSE;
  }

  // Setup for ranged combat. We assume that if you're here, you have a loaded ranged weapon and are not a candidate for receiving a counterstrike.
  if (att->weapon && att->ranged_combat_mode) {
    // Precondition: If you're using a heavy weapon, you must be strong enough to wield it, or else be using a gyro. CC p99
    if (!IS_NPC(att->ch)
        && !att->ranged->using_mounted_gun
        && !att->ranged->gyro
        && !att->cyber->cyberarm_gyromount
        && (att->ranged->skill >= SKILL_MACHINE_GUNS && att->ranged->skill <= SKILL_ASSAULT_CANNON)
        && (GET_STR(att->ch) < 8 || GET_BOD(att->ch) < 8)
        && !(AFF_FLAGGED(att->ch, AFF_PRONE)))
    {
      send_to_char(att->ch, "You can't lift the barrel high enough to fire! You'll have to go ^WPRONE^n to use %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(att->weapon)));
      return FALSE;
    }

    // Setup: Limit the burst of the weapon to the available ammo, and decrement ammo appropriately.
    // Emplaced mobs act as if they have unlimited ammo (technically draining 1 per shot) and no recoil.
    if (att->ranged->burst_count && !MOB_FLAGGED(att->ch, MOB_EMPLACED)) {
      if (weap_ammo || att->ranged->magazine) {
        // When we called has_ammo() earlier, we decremented their ammo by one. Give it back to true up the equation.
        int ammo_available = weap_ammo ? ++GET_AMMOBOX_QUANTITY(weap_ammo) : ++GET_MAGAZINE_AMMO_COUNT(att->ranged->magazine);

        // Cap their burst to their magazine's ammo.
        att->ranged->burst_count = MIN(att->ranged->burst_count, ammo_available);

        // Subtract the full ammo count.
        if (weap_ammo) {
          update_ammobox_ammo_quantity(weap_ammo, -(att->ranged->burst_count));
        } else {
          GET_MAGAZINE_AMMO_COUNT(att->ranged->magazine) -= (att->ranged->burst_count);
        }
      }

      // SR3 p151: Mounted weapons get halved recoil.
      int recoil = att->ranged->burst_count;
      if (att->ranged->using_mounted_gun)
        recoil /= 2;
      att->ranged->modifiers[COMBAT_MOD_RECOIL] += MAX(0, recoil - att->ranged->recoil_comp);

      switch (att->ranged->skill) {
        case SKILL_SHOTGUNS:
        case SKILL_MACHINE_GUNS:
        case SKILL_ASSAULT_CANNON:
          // Uncompensated recoil from high-recoil weapons is doubled.
          att->ranged->modifiers[COMBAT_MOD_RECOIL] *= 2;
      }
    }

    // Setup: Modify recoil based on vehicular stats.
    if (att->veh) {
      if (!att->ranged->using_mounted_gun) {
        // Core p152: Unmounted weapons get +2 TN.
        att->ranged->modifiers[COMBAT_MOD_MOVEMENT] += 2;
      } else {
        // TODO: Sensor stuff. It's a bitch to write due to all the various things that go into it, so it'll be done later.
        // Sensor tests per Core p152 and p135.
        // TN modifiers for sensor test per Core p154.
        // Success? You get half the sensor rating, rounded down, added to your dice.
      }

      // We assume all targets are standing still.
      // Per Core p153, movement gunnery modifier is +1 per 30m/CT.
      att->ranged->modifiers[COMBAT_MOD_MOVEMENT] += (int) (get_speed(att->veh) / 30);

      // Penalty for damaged veh.
      if ((att->veh)->damage > 0) {
        if ((att->veh)->damage <= 2)
          att->ranged->modifiers[COMBAT_MOD_VEHICLE_DAMAGE] += 1;
        else if ((att->veh)->damage <= 5)
          att->ranged->modifiers[COMBAT_MOD_VEHICLE_DAMAGE] += 2;
        else
          att->ranged->modifiers[COMBAT_MOD_VEHICLE_DAMAGE] += 3;
      }
    }

    // Setup: Compute modifiers to the TN based on the def->ch's current state.
    if (!AWAKE(def->ch) || IS_JACKED_IN(def->ch))
      att->ranged->modifiers[COMBAT_MOD_POSITION] -= 6;
    else if (AFF_FLAGGED(def->ch, AFF_PRONE)) {
      // Prone next to you is a bigger / easier target, prone far away is a smaller / harder one.
      if (def->ch->in_room == att->ch->in_room)
        att->ranged->modifiers[COMBAT_MOD_POSITION]--;
      else
        att->ranged->modifiers[COMBAT_MOD_POSITION]++;
    }

    // Setup: Determine distance penalties.
    if (!att->veh && att->ch->in_room != def->ch->in_room) {
      struct char_data *vict;
      bool vict_found = FALSE;
      struct room_data *room = NULL, *nextroom = NULL;

      int weapon_range;
      if (att->weapon && IS_RANGED(att->weapon)) {
        weapon_range = MIN(find_sight(att->ch), find_weapon_range(att->ch, att->weapon));
        for (int dir = 0; dir < NUM_OF_DIRS && !vict_found; dir++) {
          room = att->ch->in_room;
          if (CAN_GO2(room, dir))
            nextroom = EXIT2(room, dir)->to_room;
          else
            nextroom = NULL;
          for (int distance = 1; nextroom && (distance <= weapon_range) && !vict_found; distance++) {
            for (vict = nextroom->people; vict; vict = vict->next_in_room) {
              if (vict == def->ch) {
                att->ranged->modifiers[COMBAT_MOD_DISTANCE] += 2 * distance;
                vict_found = TRUE;
                break;
              }
            }
            room = nextroom;
            if (CAN_GO2(room, dir))
              nextroom = EXIT2(room, dir)->to_room;
            else
              nextroom = NULL;
          }
        }
      }
      if (!vict_found) {
        send_to_char(att->ch, "You squint around, but you can't find your opponent anywhere.\r\n");
        stop_fighting(att->ch);
        return FALSE;
      }
    }

    // Setup: Sniper rifle ranged penalty modifiers.
    if (IS_OBJ_STAT(att->weapon, ITEM_EXTRA_SNIPER)) {
      // If you're in the same room, you take a penalty. This is non-canon to reduce twinkery.
      if (att->ch->in_room == def->ch->in_room || att->ranged->using_mounted_gun) {
        // NPCs don't take the penalty because their weapon selection is at the mercy of the builders.
        if (!IS_NPC(att->ch)) {
          att->ranged->modifiers[COMBAT_MOD_DISTANCE] += SAME_ROOM_SNIPER_RIFLE_PENALTY;
        }
      }
      // However, using it at a distance gives a bonus due to it being a long-distance weapon.
      else {
        att->ranged->modifiers[COMBAT_MOD_DISTANCE] -= 2;
      }
    }

    // Setup: If your attacker is closing the distance (running), take a penalty per Core p112.
    if (AFF_FLAGGED(def->ch, AFF_APPROACH))
      att->ranged->modifiers[COMBAT_MOD_DEFENDER_MOVING] += 2;
    else if (!def->ranged_combat_mode && def->ch->in_room == att->ch->in_room && !IS_JACKED_IN(def->ch))
      att->ranged->modifiers[COMBAT_MOD_IN_MELEE_COMBAT] += 2; // technically supposed to be +2 per attacker, but ehhhh.

    // Setup: If you have a gyro mount, it negates recoil and movement penalties up to its rating.
    if (!att->ranged->using_mounted_gun) {
      int maximum_recoil_comp_from_gyros = att->ranged->modifiers[COMBAT_MOD_MOVEMENT] + att->ranged->modifiers[COMBAT_MOD_RECOIL];
      if (att->ranged->gyro) {
        att->ranged->modifiers[COMBAT_MOD_GYRO] -= MIN(maximum_recoil_comp_from_gyros, GET_OBJ_VAL(att->ranged->gyro, 0));
      } else if (att->cyber->cyberarm_gyromount) {
        switch (GET_WEAPON_ATTACK_TYPE(att->weapon)) {
          case WEAP_MMG:
          case WEAP_HMG:
          case WEAP_CANNON:
            break;
          default:
            att->ranged->modifiers[COMBAT_MOD_GYRO] -= MIN(maximum_recoil_comp_from_gyros, 3);
            break;
        }
      }
    }

    // Calculate and display pre-success-test information.
    snprintf(rbuf, sizeof(rbuf), "%s's burst/compensation info is ^c%d^n/^c%d^n. Additional modifiers: ",
             GET_CHAR_NAME( att->ch ),
             att->ranged->burst_count,
             MOB_FLAGGED(att->ch, MOB_EMPLACED) ? 10 : att->ranged->recoil_comp);

    att->ranged->tn += modify_target_rbuf_raw(att->ch, rbuf, sizeof(rbuf), att->ranged->modifiers[COMBAT_MOD_VISIBILITY], FALSE);
    for (int mod_index = 0; mod_index < NUM_COMBAT_MODIFIERS; mod_index++) {
      // Ranged-specific modifiers.
      buf_mod(rbuf, sizeof(rbuf), combat_modifiers[mod_index], att->ranged->modifiers[mod_index]);
      att->ranged->tn += att->ranged->modifiers[mod_index];
    }

    // Calculate the attacker's total skill (this modifies TN)
    att->ranged->dice = get_skill(att->ch, att->ranged->skill, att->ranged->tn);

    // Minimum TN is 2.
    att->ranged->tn = MAX(att->ranged->tn, 2);

    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "\r\nAfter get_skill(), attacker's ranged TN is ^c%d^n.", att->ranged->tn);
    SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;

    int bonus_if_not_too_tall = MIN(GET_SKILL(att->ch, att->ranged->skill), GET_OFFENSE(att->ch));
#ifdef USE_SLOUCH_RULES
    // Height penalty.
    if (!att->too_tall) {
      snprintf(rbuf, sizeof(rbuf), "Not too tall, so will roll %d + %d dice... ", att->ranged->dice, bonus_if_not_too_tall);
      att->ranged->dice += bonus_if_not_too_tall;
    } else {
      snprintf(rbuf, sizeof(rbuf), "Too tall, so will roll just %d dice... ", att->ranged->dice);
    }
#else
    att->ranged->dice += bonus_if_not_too_tall;
    snprintf(rbuf, sizeof(rbuf), "Rolling %d + %d dice... ", att->ranged->dice, bonus_if_not_too_tall);
#endif

    att->ranged->successes = success_test(att->ranged->dice, att->ranged->tn);
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "%d successes.", att->ranged->successes);
    SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;

    // Dodge test.
    if (AWAKE(def->ch)
        && !IS_JACKED_IN(def->ch)
        && !AFF_FLAGGED(def->ch, AFF_SURPRISE)
        && !def->too_tall
        && !AFF_FLAGGED(def->ch, AFF_PRONE))
    {
      // Previous code only allowed you to sidestep if you had also allocated at least one normal dodge die. Why?
      // We use the ranged slots here since we're positive they won't get used in a counterattack.
      def->ranged->dice = GET_DEFENSE(def->ch) + GET_POWER(def->ch, ADEPT_SIDESTEP);

      // Set up the defender's modifiers.
      def->ranged->modifiers[COMBAT_MOD_OPPONENT_BURST_COUNT] = (int)(att->ranged->burst_count / 3);
      def->ranged->modifiers[COMBAT_MOD_FOOTANCHORS] = def->cyber->footanchors;

      // Set up the defender's TN. Apply their modifiers.
      strlcpy(rbuf, "Defender's dodge roll modifiers: ", sizeof(rbuf));
      def->ranged->tn += modify_target_rbuf_raw(def->ch, rbuf, sizeof(rbuf), def->ranged->modifiers[COMBAT_MOD_VISIBILITY], FALSE);
      for (int mod_index = 0; mod_index < NUM_COMBAT_MODIFIERS; mod_index++) {
        buf_mod(rbuf, sizeof(rbuf), combat_modifiers[mod_index], def->ranged->modifiers[mod_index]);
        def->ranged->tn += def->ranged->modifiers[mod_index];
      }
      SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;

      // Minimum TN is 2.
      def->ranged->tn = MAX(def->ranged->tn, 2);

      def->ranged->successes = MAX(success_test(def->ranged->dice, def->ranged->tn), 0);
      att->ranged->successes -= def->ranged->successes;

      snprintf(rbuf, sizeof(rbuf), "Dodge: Dice %d (%d pool + %d sidestep), TN %d, Successes ^c%d^n.  This means attacker's net successes = ^c%d^n.",
               def->ranged->dice,
               GET_DEFENSE(def->ch),
               GET_POWER(def->ch, ADEPT_SIDESTEP),
               def->ranged->tn,
               def->ranged->successes,
               att->ranged->successes);
    } else {
      // Surprised, oversized, unconscious, or prone? No dodge test for you.
      att->ranged->successes = MAX(att->ranged->successes, 0);
      snprintf(rbuf, sizeof(rbuf), "Opponent unable to dodge, attacker's successes will remain at ^c%d^n.", att->ranged->successes);
      if (GET_DEFENSE(def->ch) > 0 && AFF_FLAGGED(def->ch, AFF_PRONE) && !IS_JACKED_IN(def->ch))
        send_to_char(def->ch, "^yYou're unable to dodge while prone!^n\r\n");
    }
    SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;

    // If the ranged attack failed, print the relevant message and terminate.
    if (att->ranged->successes < 1) {
      snprintf(rbuf, sizeof(rbuf), "%s failed to achieve any net successes, so we're bailing out.", GET_CHAR_NAME(attacker));
      SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;

      combat_message(att->ch, def->ch, att->weapon, -1, att->ranged->burst_count);
      bool target_died = 0;
      target_died = damage(att->ch, def->ch, -1, att->ranged->dam_type, 0);

      //Handle suprise attack/alertness here -- ranged attack failed.
      if (!target_died && IS_NPC(def->ch)) {
        if (AFF_FLAGGED(def->ch, AFF_SURPRISE))
          AFF_FLAGS(def->ch).RemoveBit(AFF_SURPRISE);

        GET_MOBALERT(def->ch) = MALERT_ALARM;
        GET_MOBALERTTIME(def->ch) = 30;
      }
      return target_died;
    }

    // Calculate the power of the attack.
    att->ranged->power = GET_WEAPON_POWER(att->weapon) + att->ranged->burst_count;
    att->ranged->damage_level = GET_WEAPON_DAMAGE_CODE(att->weapon) + (int)(att->ranged->burst_count / 3);

    // Calculate effects of armor on the power of the attack.
    if (att->ranged->magazine) {
      switch (GET_MAGAZINE_AMMO_TYPE(att->ranged->magazine)) {
        case AMMO_APDS:
          att->ranged->power -= (int)(GET_BALLISTIC(def->ch) / 2);
          break;
        case AMMO_EX:
          att->ranged->power++;
          // fall through
        case AMMO_EXPLOSIVE:
          att->ranged->power++;
          att->ranged->power -= GET_BALLISTIC(def->ch);
          break;
        case AMMO_FLECHETTE:
          if (!GET_IMPACT(def->ch) && !GET_BALLISTIC(def->ch))
            att->ranged->damage_level++;
          else {
            att->ranged->power -= MAX(GET_BALLISTIC(def->ch), GET_IMPACT(def->ch) * 2);
          }
          break;
        case AMMO_HARMLESS:
          att->ranged->power = 0;
          // fall through
        case AMMO_GEL:
          // Errata: Add the following after the third line: "Impact armor, not Ballistic, applies."
          att->ranged->power -= GET_IMPACT(def->ch) + 2;
          att->ranged->is_gel = TRUE;
          break;
        default:
          att->ranged->power -= GET_BALLISTIC(def->ch);
      }
    }
    // Weapon fired without a magazine (probably by an NPC)-- we assume its ammo type is normal.
    else {
      att->ranged->power -= GET_BALLISTIC(def->ch);
    }

    // Increment character's shots_fired. This is used for internal tracking of eligibility for a skill quest.
    if (GET_SKILL(att->ch, att->ranged->skill) >= 8 && SHOTS_FIRED(att->ch) < 10000)
      SHOTS_FIRED(att->ch)++;

    // The power of an attack can't be below 2 from ammo changes.
    att->ranged->power = MAX(att->ranged->power, 2);

    // Handle spirits and elementals being divas, AKA having Immunity to Normal Weapons (SR3 p188, 264).
    // Namely: We require that the attack's power is greater than double the spirit's force, otherwise it takes no damage.
    // If the attack's power is greater, subtract double the level from it.
    // There's no checking for weapon foci here since there are no ranged weapon foci.
    if (IS_SPIRIT(def->ch) || IS_ANY_ELEMENTAL(def->ch)) {
      int minimum_power_to_damage_opponent = (GET_LEVEL(def->ch) * 2) + 1;
      if (att->ranged->power < minimum_power_to_damage_opponent) {
        bool target_died = 0;

        combat_message(att->ch, def->ch, att->weapon, 0, att->ranged->burst_count);
        send_to_char(att->ch, "^o(OOC: %s is immune to normal weapons! You need at least ^O%d^o weapon power to damage %s, and you only have %d.)^n\r\n",
                     decapitalize_a_an(GET_CHAR_NAME(def->ch)),
                     minimum_power_to_damage_opponent,
                     HMHR(def->ch),
                     att->ranged->power
                    );
        target_died = damage(att->ch, def->ch, 0, att->ranged->dam_type, att->ranged->is_physical);

        //Handle suprise attack/alertness here -- spirits ranged.
        if (!target_died && IS_NPC(def->ch)) {
          if (AFF_FLAGGED(def->ch, AFF_SURPRISE))
            AFF_FLAGS(def->ch).RemoveBit(AFF_SURPRISE);

          GET_MOBALERT(def->ch) = MALERT_ALARM;
          GET_MOBALERTTIME(def->ch) = 30;
        }
        return target_died;
      } else
        att->ranged->power -= GET_LEVEL(def->ch) * 2;
    }

    // Core p113.
    att->ranged->power = MAX(att->ranged->power, 2);
  }
  // Melee combat. If you're here, we don't care about your ranged combat setup-- it's face-beating time. All calculations here are done for both attacker and defender (in case of defender counterstriking)
  else {
    // Ensure that neither combatant has the closing flag set.
    AFF_FLAGS(att->ch).RemoveBit(AFF_APPROACH);
    AFF_FLAGS(def->ch).RemoveBit(AFF_APPROACH);

    // Setup: Calculate position modifiers.
    // It's hard for you to fight while prone. (SR3 p123)
    if (AFF_FLAGGED(att->ch, AFF_PRONE)) {
      send_to_char(att->ch, "You struggle to fight while prone!\r\n");
      def->melee->modifiers[COMBAT_MOD_POSITION] += 2;
    }

    // Treat unconscious as being a position mod of -6 (reflects ease of coup de grace)
    if (!AWAKE(def->ch))
      att->melee->modifiers[COMBAT_MOD_POSITION] -= 6;
    else if (AFF_FLAGGED(def->ch, AFF_PRONE))
      att->melee->modifiers[COMBAT_MOD_POSITION] -= 2;

    // If we're doing nerve strike, wrap that up and break out early.
    if (!att->weapon && IS_NERVE(att->ch)) {
      if (perform_nerve_strike(att, def, rbuf, sizeof(rbuf)))
        return FALSE;
    }
    // Spirits use different dice than the rest of us plebs.
    // Disabled this portion for now-- it looks like the original intent was to implement a clash of wills, but the code does not support this at the moment.
    /*
    if (IS_SPIRIT(def->ch) || IS_ANY_ELEMENTAL(def->ch)) {
      act("Defender is a spirit, so attacker uses wil and defender uses reaction for dice pool.", 1, att->ch, NULL, NULL, TO_ROLLS);
      att->dice = GET_WIL(att->ch);
      def->dice = GET_REA(def->ch);
    } else if (IS_SPIRIT(att->ch) || IS_ANY_ELEMENTAL(att->ch)) {
      act("Attacker is a spirit, so attacker uses reaction and defender uses wil for dice pool.", 1, att->ch, NULL, NULL, TO_ROLLS);
      att->dice = GET_REA(att->ch);
      def->dice = GET_WIL(def->ch);
    } else {
    */

    strlcpy(rbuf, "Computing dice for attacker...", sizeof(rbuf));
    SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;
    att->melee->dice = att->melee->skill_bonus + get_skill(att->ch, att->melee->skill, att->melee->tn);
    if (!att->too_tall)
      att->melee->dice += MIN(GET_SKILL(att->ch, att->melee->skill) + att->melee->skill_bonus, GET_OFFENSE(att->ch));

    strlcpy(rbuf, "Computing dice for defender...", sizeof(rbuf));
    SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;
    def->melee->dice = def->melee->skill_bonus + get_skill(def->ch, def->melee->skill, def->melee->tn);
    if (!def->too_tall)
      def->melee->dice += MIN(GET_SKILL(def->ch, def->melee->skill) + def->melee->skill_bonus, GET_OFFENSE(def->ch));

    // }

    // Adepts get bonus dice when counterattacking.
    if (GET_POWER(def->ch, ADEPT_COUNTERSTRIKE) > 0) {
      def->melee->dice += GET_POWER(def->ch, ADEPT_COUNTERSTRIKE);
      snprintf(rbuf, sizeof(rbuf), "Defender counterstrike dice bonus is %d.", GET_POWER(def->ch, ADEPT_COUNTERSTRIKE));
      SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;
    }

    // Bugfix: If you're unconscious or mortally wounded, you don't get to counterattack.
    if (GET_PHYSICAL(def->ch) <= 0 || GET_MENTAL(def->ch) <= 0 || IS_JACKED_IN(def->ch)) {
      def->melee->dice = 0;
      strlcpy(rbuf, "^yDefender incapped, dice capped to zero.^n", sizeof(rbuf));
      SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;
    }

    snprintf(rbuf, sizeof(rbuf), "^g%s's dice: ^W%d^g, %s's dice: ^W%d^g.^n", GET_CHAR_NAME(att->ch), att->melee->dice, GET_CHAR_NAME(def->ch), def->melee->dice);
    SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;

    // Calculate the net reach.
    int net_reach = GET_REACH(att->ch) - GET_REACH(def->ch);

    // Skilled NPCs get to switch to close combat mode at this time (those cheating bastards.)
    engage_close_combat_if_appropriate(att, def, net_reach);
    engage_close_combat_if_appropriate(def, att, -net_reach);

    if (!GET_POWER(att->ch, ADEPT_PENETRATINGSTRIKE) && GET_POWER(att->ch, ADEPT_DISTANCE_STRIKE)) {
      // MitS 149: Ignore reach modifiers.
      net_reach = 0;
    }

    if (AFF_FLAGGED(att->ch, AFF_CLOSECOMBAT) || AFF_FLAGGED(def->ch, AFF_CLOSECOMBAT)) {
      // CC p99: Ignore reach modifiers, decrease user's power by one.
      net_reach = 0;

      if (AFF_FLAGGED(att->ch, AFF_CLOSECOMBAT)) {
        att->melee->power -= 1;
        act("Decreased melee power by 1 and negated net reach due to attacker's close combat toggle.", TRUE, att->ch, NULL, NULL, TO_ROLLS);
      } else {
        act("Negated net reach due to defender's close combat toggle.", TRUE, att->ch, NULL, NULL, TO_ROLLS);
      }
    }

    // Reach is always used offensively. TODO: Add option to use it defensively instead.
    if (net_reach > 0)
      att->melee->modifiers[COMBAT_MOD_REACH] -= net_reach;
    else
      def->melee->modifiers[COMBAT_MOD_REACH] -= -net_reach;

    // -------------------------------------------------------------------------------------------------------
    // Calculate and display pre-success-test information.
    snprintf(rbuf, sizeof(rbuf), "^cCalculating melee combat modifiers. %s's TN modifiers: ", GET_CHAR_NAME(att->ch) );
    // This feels a little shitty, but we know that if we're in melee mode, the TN has not been touched yet, and if we're not then it's already been calculated.
    att->melee->tn += modify_target_rbuf_raw(att->ch, rbuf, sizeof(rbuf), att->melee->modifiers[COMBAT_MOD_VISIBILITY], FALSE);
    for (int mod_index = 0; mod_index < NUM_COMBAT_MODIFIERS; mod_index++) {
      buf_mod(rbuf, sizeof(rbuf), combat_modifiers[mod_index], att->melee->modifiers[mod_index]);
      att->melee->tn += att->melee->modifiers[mod_index];
    }
    SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;

    snprintf(rbuf, sizeof(rbuf), "^c%s%s's TN modifiers: ", GET_CHAR_NAME( def->ch ),
            (GET_PHYSICAL(def->ch) <= 0 || GET_MENTAL(def->ch) <= 0) ? " (incap)" : "" );
    def->melee->tn += modify_target_rbuf_raw(def->ch, rbuf, sizeof(rbuf), def->melee->modifiers[COMBAT_MOD_VISIBILITY], FALSE);
    for (int mod_index = 0; mod_index < NUM_COMBAT_MODIFIERS; mod_index++) {
      buf_mod(rbuf, sizeof(rbuf), combat_modifiers[mod_index], def->melee->modifiers[mod_index]);
      def->melee->tn += def->melee->modifiers[mod_index];
    }
    SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;

    // Minimum TN is 2.
    att->melee->tn = MAX(att->melee->tn, 2);
    def->melee->tn = MAX(def->melee->tn, 2);

    // Calculate the clash, unless there's some surprise involved (hitting someone unconscious is technically surprising for them)
    if (AWAKE(def->ch) && !AFF_FLAGGED(def->ch, AFF_SURPRISE)) {
      att->melee->successes = success_test(att->melee->dice, att->melee->tn);
      def->melee->successes = success_test(def->melee->dice, def->melee->tn);
      net_successes = att->melee->successes - def->melee->successes;
    } else {
      strlcpy(rbuf, "Surprised-- defender gets no roll.", sizeof(rbuf));
      SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;
      att->melee->successes = MAX(1, success_test(att->melee->dice, att->melee->tn));
      net_successes = att->melee->successes;
    }

    // Store our successes for the monowhip test, since there's a chance it'll be flipped in counterattack.
    successes_for_use_in_monowhip_test_check = att->melee->successes;

    if (def->weapon && GET_OBJ_TYPE(def->weapon) != ITEM_WEAPON) {
      // Defender's wielding a non-weapon? Whoops, net successes will never be less than 0.
      strlcpy(rbuf, "Defender wielding non-weapon-- cannot win clash.", sizeof(rbuf));
      SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;
      net_successes = MAX(0, net_successes);
    }

    // Compose and send various messages.
    snprintf(rbuf, sizeof(rbuf), "^g%s got ^W%d^g success%s from ^W%d^g dice at TN ^W%d^g.^n\r\n",
             GET_CHAR_NAME(att->ch),
             att->melee->successes,
             att->melee->successes != 1 ? "s" : "",
             att->melee->dice,
             att->melee->tn);
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "^g%s got ^W%d^g success%s from ^W%d^g dice at TN ^W%d^g.^n\r\n",
             GET_CHAR_NAME(def->ch),
             def->melee->successes,
             def->melee->successes != 1 ? "s" : "",
             def->melee->dice,
             def->melee->tn);

    if (net_successes < 0) {
      snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "^yNet successes is ^W%d^y, which will cause a counterattack.^n\r\n", net_successes);
    } else
      snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "Net successes is ^W%d^n.\r\n", net_successes);
    SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;

    act("$n clashes with $N in melee combat.", FALSE, att->ch, 0, def->ch, TO_ROOM);
    act("You clash with $N in melee combat.", FALSE, att->ch, 0, def->ch, TO_CHAR);

    // If your enemy got more successes than you, guess what? You're the one who gets their face caved in.
    if (net_successes < 0) {
      if (!GET_POWER(att->ch, ADEPT_PENETRATINGSTRIKE) && GET_POWER(att->ch, ADEPT_DISTANCE_STRIKE)) {
        // MitS 149: You cannot be counterstriked while using distance strike.
        net_successes = 0;
      }
      // This messaging gets a little annoying.
      act("You successfully counter $N's attack!", FALSE, def->ch, 0, att->ch, TO_CHAR);
      act("$n deflects your attack and counterstrikes!", FALSE, def->ch, 0, att->ch, TO_VICT);
      act("$n deflects $N's attack and counterstrikes!", FALSE, def->ch, 0, att->ch, TO_NOTVICT);

      // We're swapping the attacker and defender here, but this isn't an issue: we did all the defender's melee setup already.
      struct combat_data *temp_att = att;
      att = def;
      def = temp_att;

      // Flip the successes. Now that we've bifurcated the modifiers between ranged and melee, we no longer need to worry about those here.
      att->melee->successes = -1 * net_successes;

      // Prevent ranged combat messaging.
      att->ranged_combat_mode = FALSE;

      // Nerve-strike counterattacks are a thing.
      if (!att->weapon && IS_NERVE(att->ch)) {
        if (perform_nerve_strike(att, def, rbuf, sizeof(rbuf)))
          return FALSE;
      }
    } else {
      att->melee->successes = net_successes;
    }

    // Calculate the power of the attack.
    if (att->weapon) {
      // Monowhips deal flat damage.
      if (GET_OBJ_RNUM(att->weapon) >= 0 && obj_index[GET_OBJ_RNUM(att->weapon)].wfunc == monowhip) {
        att->melee->power = 10;
        att->melee->damage_level = SERIOUS;

        att->melee->power -= GET_IMPACT(def->ch) / 2;
      }
      // Because we swap att and def pointers if defender wins the clash we need to make sure attacker gets proper values
      // if they're using a ranged weapon in clash instead of setting their melee power to the damage code of the ranged
      // weapon as it was happening.
      else if (IS_RANGED(att->weapon)) {
        att->melee->power = GET_STR(att->ch);
        att->melee->damage_level = MODERATE;
        att->melee->power -= GET_IMPACT(def->ch);
      }
      // Non-monowhips behave normally.
      else {
        att->melee->power = GET_WEAPON_STR_BONUS(att->weapon) + GET_STR(att->ch);
        att->melee->power -= GET_IMPACT(def->ch);
      }
    }
    // Cyber and unarmed combat.
    else {
      // Most of our melee combat fields were set during setup, so we're only here for the effects of armor.
      if (att->cyber->num_cyberweapons <= 0
          && GET_POWER(att->ch, ADEPT_PENETRATINGSTRIKE)
          && !GET_POWER(att->ch, ADEPT_DISTANCE_STRIKE))
      {
        att->melee->power -= MAX(0, GET_IMPACT(def->ch) - GET_POWER(att->ch, ADEPT_PENETRATINGSTRIKE));
      } else {
        att->melee->power -= GET_IMPACT(def->ch);
      }
    }

    // Handle spirits and elementals being divas, AKA having Immunity to Normal Weapons (SR3 p188, 264).
    // Namely: We require that the attack's power is greater than double the spirit's force, otherwise it takes no damage.
    // If the attack's power is greater, subtract double the level from it.
    if ((IS_SPIRIT(def->ch) || IS_ANY_ELEMENTAL(def->ch))
        && (!att->weapon || GET_WEAPON_FOCUS_RATING(att->weapon) == 0 || !WEAPON_FOCUS_USABLE_BY(att->weapon, att->ch)))
    {
      int minimum_power_to_damage_opponent = (GET_LEVEL(def->ch) * 2) + 1;
      if (att->melee->power < minimum_power_to_damage_opponent) {
        bool target_died = 0;

        send_to_char(att->ch, "^o(OOC: %s is immune to normal weapons! You need at least ^O%d^o weapon power to damage %s, and you only have %d.)^n\r\n",
                     decapitalize_a_an(GET_CHAR_NAME(def->ch)),
                     minimum_power_to_damage_opponent,
                     HMHR(def->ch),
                     att->melee->power
                    );
        target_died = damage(att->ch, def->ch, 0, att->melee->dam_type, att->melee->is_physical);

        //Handle suprise attack/alertness here -- spirits melee.
        if (!target_died && IS_NPC(def->ch)) {
          if (AFF_FLAGGED(def->ch, AFF_SURPRISE))
            AFF_FLAGS(def->ch).RemoveBit(AFF_SURPRISE);

          GET_MOBALERT(def->ch) = MALERT_ALARM;
          GET_MOBALERTTIME(def->ch) = 30;

          // Process flame aura here since the spirit will otherwise end combat evaluation here.
          handle_flame_aura(att, def);
        }
        return target_died;
      } else {
        att->melee->power -= GET_LEVEL(def->ch) * 2;
      }
    }

    // Core p113.
    att->melee->power = MAX(att->melee->power, 2);
  }
  // End melee-only calculations. Code beyond here is unified for both ranged and melee.

  // Perform body test for damage resistance.
  int bod_success = 0, bod_dice = 0;

  // Put your body dice pool in, if applicable.
#ifndef USE_SLOUCH_RULES
  if (def->too_tall || att->too_tall) {
    mudlog("SYSERR: Someone is too_tall when USE_SLOUCH_RULES is disabled!", att->ch, LOG_SYSLOG, TRUE);
  }
  bod_dice += GET_BODY(def->ch);
#else
  if (!def->too_tall) {
    bod_dice += GET_BODY(def->ch);
  }
#endif

  // Unconscious? No pool dice for you.
  if (!AWAKE(def->ch))
    bod_dice = 0;

  // Add your attribute.
  // If you're a spirit attacking someone who has the conjuring skill, they can opt to use that instead of their body if it's higher.
  if (IS_SPIRIT(att->ch) && GET_MAG(def->ch) > 0 && GET_SKILL(def->ch, SKILL_CONJURING))
    bod_dice += MAX(GET_BOD(def->ch), GET_SKILL(def->ch, SKILL_CONJURING));
  else
    bod_dice += GET_BOD(def->ch);

  // Declare our staged_damage variable, which is modified in the upcoming bod test and staging code.
  int staged_damage = 0;

  // Nothing can raise our damage level past deadly or below none-- cap it.
  att->ranged->damage_level = MAX(0, MIN(DEADLY, att->ranged->damage_level));
  att->melee->damage_level = MAX(0, MIN(DEADLY, att->melee->damage_level));

  // Roll the bod test and apply necessary staging.
  if (att->ranged_combat_mode) {
    bod_success = success_test(GET_BOD(def->ch) + bod_dice, att->ranged->power);
    att->ranged->successes -= bod_success;

    // Harmless ammo never deals damage.
    if (att->ranged->magazine && GET_MAGAZINE_AMMO_TYPE(att->ranged->magazine) == AMMO_HARMLESS) {
      staged_damage = -1;
      strlcpy(rbuf, "Damage reduced to -1 due to use of harmless ammo.", sizeof(rbuf));
      SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;
    } else if (can_hurt(att->ch, def->ch, att->ranged->dam_type, TRUE)) {
      // You're able to hurt them? Proceed as normal.
      staged_damage = stage(att->ranged->successes, att->ranged->damage_level);
    } else {
      // You're unable to hurt them. Adjust messaging for unkillable enemies.
      staged_damage = -1;
      strlcpy(rbuf, "Damage reduced to -1 due to failure of CAN_HURT().", sizeof(rbuf));
      SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;
    }
  } else {
    bod_success = success_test(bod_dice, att->melee->power);
    att->melee->successes -= bod_success;

    // Adjust messaging for unkillable entities (melee stanza)
    if (can_hurt(att->ch, def->ch, att->melee->dam_type, TRUE)) {
      staged_damage = stage(att->melee->successes, att->melee->damage_level);
    } else {
      staged_damage = -1;
      strlcpy(rbuf, "Damage reduced to -1 due to failure of CAN_HURT().", sizeof(rbuf));
      SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;
    }
  }

  int damage_total = convert_damage(staged_damage);

  {
    int net_attack_power = att->ranged_combat_mode ? att->ranged->power : att->melee->power;
    int net_successes = att->ranged_combat_mode ? att->ranged->successes : att->melee->successes;
    int damage_level = att->ranged_combat_mode ? att->ranged->damage_level : att->melee->damage_level;
    bool damage_is_physical = att->ranged_combat_mode ? att->ranged->is_physical : att->melee->is_physical;

    snprintf(rbuf, sizeof(rbuf), "^cDefender rolls %d bod dice vs TN %d, getting %d success%s; attacker now has %d net success%s.\r\n^CDamage stages from %s(%d) to %s(%d), aka %d boxes of %c.^n",
             bod_dice,  // bod dice
             net_attack_power,  // TN
             bod_success, // success
             bod_success == 1 ? "" : "es", // success plural
             net_successes, // net success
             net_successes == 1 ? "" : "es", // net success plural
             GET_WOUND_NAME(damage_level), // damage stage from (word)
             damage_level, // damage stage from (int)
             GET_WOUND_NAME(staged_damage), // damage stage to (word)
             staged_damage, // damage stage to (int)
             damage_total, // actual boxes of damage done
             damage_is_physical ? 'P' : 'M' // mental or physical
            );
    SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;
  }


  bool defender_died, defender_was_npc = IS_NPC(def->ch);
  if (att->ranged_combat_mode) {
    combat_message(att->ch, def->ch, att->weapon, MAX(0, damage_total), att->ranged->burst_count);
    defender_died = damage_without_message(att->ch, def->ch, damage_total, att->ranged->dam_type, att->ranged->is_physical);

    if (!defender_died && damage_total > 0)
      perform_knockdown_test(def->ch, (GET_WEAPON_POWER(att->weapon) + att->ranged->burst_count) / (att->ranged->is_gel ? 1 : 2));
  } else {
    // Process flame aura right before damage.
    if (handle_flame_aura(att, def)) {
      // They died! Bail out.
      return TRUE;
    }

    defender_died = damage(att->ch, def->ch, damage_total, att->melee->dam_type, att->melee->is_physical);

    if (!defender_died) {
      if (damage_total > 0)
        perform_knockdown_test(def->ch, GET_STR(att->ch));

      if (successes_for_use_in_monowhip_test_check <= 0 && (net_successes < 0 ? def->melee->is_monowhip : att->melee->is_monowhip)) {
        struct char_data *attacker = net_successes < 0 ? def->ch : att->ch;
        struct char_data *defender = net_successes < 0 ? att->ch : def->ch;

        int target = 6 + modify_target(attacker);
        int skill = get_skill(attacker, SKILL_WHIPS_FLAILS, target);
        int successes = success_test(skill, target);
        snprintf(rbuf, sizeof(rbuf), "Monowhip 'flailure' test: Skill of %d, target of %d, successes is %d.", skill, target, successes);
        SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;
        if (successes <= 0) {
          act("^yYour monowhip flails out of control, striking you instead of $N!^n", FALSE, attacker, 0, defender, TO_CHAR);
          act("$n's monowhip completely misses and recoils to hit $m!", TRUE, attacker, 0, 0, TO_ROOM);
          int dam_total = convert_damage(stage(-1 * success_test(GET_BOD(attacker) + (successes == 0 ? GET_DEFENSE(attacker) : 0), 10), SERIOUS));


          //Handle suprise attack/alertness here -- attacker can die here, we remove the surprise flag anyhow
          //prior to handling the damage and we don't alter alert state at all because if defender is a quest target
          //they will be extracted. If the attacker actually dies and it's a normal mob, they won't be surprised anymore
          //and alertness will trickle down on its own with update cycles.
          if (IS_NPC(def->ch) && AFF_FLAGGED(def->ch, AFF_SURPRISE))
            AFF_FLAGS(def->ch).RemoveBit(AFF_SURPRISE);
          // If the attacker dies from backlash, bail out.
          if (damage(attacker, attacker, dam_total, TYPE_RECOIL, PHYSICAL))
            return TRUE;
        }
      }
      //Handle suprise attack/alertness here -- defender didn't die.
      if (IS_NPC(def->ch)) {
        if (AFF_FLAGGED(def->ch, AFF_SURPRISE))
          AFF_FLAGS(def->ch).RemoveBit(AFF_SURPRISE);

        GET_MOBALERT(def->ch) = MALERT_ALERT;
        GET_MOBALERTTIME(def->ch) = 20;
      }
    }
  }

  if (defender_died) {
    // Fixes edge case where attacking quest NPC kills its hunter with a heavy weapon, is extracted, then tries to check recoil.
    if (!defender_was_npc)
      return TRUE;
    // Clear out the defending character's pointer since it now points to a nulled character struct.
    else
      def->ch = NULL;
  } else if (!IS_NPC(att->ch) && IS_NPC(def->ch)) {
    GET_LASTHIT(def->ch) = GET_IDNUM(att->ch);
  }

  // If you're firing a heavy weapon without a gyro, you need to test against the damage of the recoil.
  if (!IS_NPC(att->ch)
      && att->ranged_combat_mode
      && !att->ranged->using_mounted_gun
      && !att->ranged->gyro
      && !att->cyber->cyberarm_gyromount
      && (att->ranged->skill >= SKILL_MACHINE_GUNS && att->ranged->skill <= SKILL_ASSAULT_CANNON)
      && !AFF_FLAGGED(att->ch, AFF_PRONE))
  {
    int weapon_power = GET_WEAPON_POWER(att->weapon) + att->ranged->burst_count;
    int recoil_successes = success_test(GET_BOD(att->ch) + GET_BODY(att->ch), weapon_power / 2);
    int staged_dam = stage(-recoil_successes, LIGHT);
    snprintf(rbuf, sizeof(rbuf), "Heavy Recoil: %d successes, L->%s wound.", recoil_successes, staged_dam == LIGHT ? "L" : "no");
    // SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );

    //Handle suprise attack/alertness here -- attacker can die here, we remove the surprise flag anyhow
    //prior to handling the damage and we don't alter alert state at all because if defender is a quest target
    //they will be extracted. If the attacker actually dies and it's a normal mob, they won't be surprised anymore
    //and alertness will trickle down on its own with update cycles.
    if (!defender_died && IS_NPC(def->ch) && AFF_FLAGGED(def->ch, AFF_SURPRISE))
      AFF_FLAGS(def->ch).RemoveBit(AFF_SURPRISE);

    // If the attacker dies from recoil, bail out.
    if (damage(att->ch, att->ch, convert_damage(staged_dam), TYPE_HIT, FALSE))
      return TRUE;

    // Next, knockdown test vs half the weapon's power, on 0 successes you're knocked down.
    // No need to subtract things like gyro from this recoil number-- prereq for getting here is that there's no gyro or mount.
    perform_knockdown_test(att->ch, weapon_power / 2, att->ranged->modifiers[COMBAT_MOD_RECOIL]);
  }

  // Set the violence background count.
  struct room_data *room = get_ch_in_room(att->ch);
  if (room && !GET_BACKGROUND_COUNT(room)) {
    GET_SETTABLE_BACKGROUND_COUNT(room) = 1;
    GET_SETTABLE_BACKGROUND_AURA(room) = AURA_PLAYERCOMBAT;
  }
  return FALSE;
}

bool hit(struct char_data *attacker, struct char_data *victim, struct obj_data *weap, struct obj_data *vict_weap, struct obj_data *weap_ammo) {
  return hit_with_multiweapon_toggle(attacker, victim, weap, vict_weap, weap_ammo, FALSE);
}

bool does_weapon_have_bayonet(struct obj_data *weapon) {
  // Precondition: Weapon must exist, must be a weapon-class item, and must be a gun.
  if (!(weapon && GET_OBJ_TYPE(weapon) == ITEM_WEAPON && IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))))
    return FALSE;

  struct obj_data *attach_proto = get_obj_proto_for_vnum(GET_WEAPON_ATTACH_UNDER_VNUM(weapon));

  /*
  if (!attach_proto)
    log_vfprintf("No proto found for vnum %d", GET_WEAPON_ATTACH_UNDER_VNUM(weapon));
  if (GET_ACCESSORY_TYPE(attach_proto) != ACCESS_BAYONET)
    log_vfprintf("Vnum %d is not a bayonet", GET_WEAPON_ATTACH_UNDER_VNUM(weapon));
  */

  return (attach_proto && GET_ACCESSORY_TYPE(attach_proto) == ACCESS_BAYONET);
}

void engage_close_combat_if_appropriate(struct combat_data *att, struct combat_data *def, int net_reach) {
  if (IS_NPC(att->ch) && net_reach != 0 && AFF_FLAGGED(att->ch, AFF_SMART_ENOUGH_TO_TOGGLE_CLOSECOMBAT)) {
    // If the net reach does not favor the NPC, switch on close combat.
    if (net_reach < 0 && !AFF_FLAGGED(att->ch, AFF_CLOSECOMBAT)) {
      AFF_FLAGS(att->ch).SetBit(AFF_CLOSECOMBAT);
      if (att->weapon) {
        act("$n shifts $s grip on $p, trying to get inside $N's guard!", TRUE, att->ch, att->weapon, def->ch, TO_NOTVICT);
        act("$n shifts $s grip on $p, trying to get inside your guard!", TRUE, att->ch, att->weapon, def->ch, TO_VICT);
      } else {
        act("$n ducks in close, trying to get inside $N's guard!", TRUE, att->ch, NULL, def->ch, TO_NOTVICT);
        act("$n ducks in close, trying to get inside your guard!", TRUE, att->ch, NULL, def->ch, TO_NOTVICT);
      }
    }

    // Otherwise, switch it off.
    else if (net_reach > 0 && AFF_FLAGGED(att->ch, AFF_CLOSECOMBAT)) {
      AFF_FLAGS(att->ch).RemoveBit(AFF_CLOSECOMBAT);
      if (att->weapon) {
        act("$n shifts $s grip on $p, trying to keep $N outside $s guard!", TRUE, att->ch, att->weapon, def->ch, TO_NOTVICT);
        act("$n shifts $s grip on $p, trying to keep you outside $s guard!", TRUE, att->ch, att->weapon, def->ch, TO_VICT);
      } else {
        act("$n backs up, trying to keep $N outside $s guard!", TRUE, att->ch, NULL, def->ch, TO_NOTVICT);
        act("$n backs up, trying to keep you outside $s guard!", TRUE, att->ch, NULL, def->ch, TO_VICT);
      }
    }
  }
}

bool handle_flame_aura(struct combat_data *att, struct combat_data *def) {
  // no-op: defender has no aura
  if (!AFF_FLAGGED(def->ch, AFF_FLAME_AURA) && !MOB_FLAGGED(def->ch, MOB_FLAMEAURA))
    return FALSE;

  // no-op: you have a weapon
  if (att->ranged_combat_mode || att->weapon)
    return FALSE;

  int force = -1;

  // Flameaura-flagged NPCs just use the larger of half their magic or their own full level as the force.
  if (MOB_FLAGGED(def->ch, MOB_FLAMEAURA)) {
    force = MAX(GET_MAG(def->ch) / 200, GET_LEVEL(def->ch));
  } else {
    // Iterate through their spells, find the flame aura that's applied to them, and extract its force.
    for (struct sustain_data *sust = GET_SUSTAINED(def->ch); sust; sust = sust->next) {
      if (!sust->caster && sust->spell == SPELL_FLAME_AURA) {
        force = sust->force;
        break;
      }
    }

    // Account for edge case of not finding the spell.
    if (force == -1) {
      mudlog("SYSERR: Unable to find flameaura spell! Using default of 6 for force.", def->ch, LOG_SYSLOG, TRUE);
      force = 6;
    }
  }

  char rolls_buf[1000];
  snprintf(rolls_buf, sizeof(rolls_buf), "^oFlame Aura (%s hitting %s): Force starts at %d.",
           GET_CHAR_NAME(att->ch),
           GET_CHAR_NAME(def->ch),
           force
         );

  force -= GET_POWER(att->ch, ADEPT_TEMPERATURE_TOLERANCE);
  force -= GET_POWER(att->ch, ADEPT_MYSTIC_ARMOR);

  snprintf(ENDOF(rolls_buf), sizeof(rolls_buf) - strlen(rolls_buf), " After adept powers, it's %d.", force);

  force -= affected_by_spell(att->ch, SPELL_ARMOR);

  snprintf(ENDOF(rolls_buf), sizeof(rolls_buf) - strlen(rolls_buf), " After armor spell, it's %d.", force);

  for (struct obj_data *cyber = att->ch->cyberware; cyber; cyber = cyber->next_content) {
    if (GET_CYBERWARE_TYPE(cyber) == CYB_DERMALPLATING) {
      force -= GET_CYBERWARE_RATING(cyber);
    }
    else if (GET_CYBERWARE_TYPE(cyber) == CYB_DERMALSHEATHING) {
      force -= GET_CYBERWARE_RATING(cyber);
    }
    else if (GET_CYBERWARE_TYPE(cyber) == CYB_ARMS) {
      if (IS_SET(GET_CYBERWARE_FLAGS(cyber), ARMS_MOD_ARMOR_MOD1)) {
        force -= 1;
      }
    }
    else if (GET_CYBERWARE_TYPE(cyber) == CYB_LEGS) {
      if (IS_SET(GET_CYBERWARE_FLAGS(cyber), LEGS_MOD_ARMOR_MOD1)) {
        force -= 1;
      }
    }
  }

  snprintf(ENDOF(rolls_buf), sizeof(rolls_buf) - strlen(rolls_buf), " After cyberware, it's %d.", force);

  for (struct obj_data *bio = att->ch->bioware; bio; bio = bio->next_content) {
    if (GET_BIOWARE_TYPE(bio) == BIO_ORTHOSKIN) {
      force -= GET_BIOWARE_RATING(bio);
    }
  }

  snprintf(ENDOF(rolls_buf), sizeof(rolls_buf) - strlen(rolls_buf), " After bioware, it's %d.", force);

  // Roll a body check, idk.
  int dice = GET_BOD(att->ch);
  int successes = success_test(dice, force);
  int staged_damage = stage(-successes, MODERATE);

  snprintf(ENDOF(rolls_buf), sizeof(rolls_buf) - strlen(rolls_buf), "\r\n^oRolled %d successes on %d dice, staged damage Moderate->%s.",
           successes, dice, GET_WOUND_NAME(staged_damage));
  act(rolls_buf, FALSE, att->ch, 0, 0, TO_ROLLS);

  if (staged_damage <= 0) {
    // Do nothing-- they took no damage.
    return FALSE;
  }

  // Convert light/medium/serious/deadly to boxes.
  int converted_damage = convert_damage(staged_damage);

  // Remember that damage() returns true if it's killed them-- if this happens, you'll need to abort handling, because att->ch is already nulled out.
  char death_msg[500];
  snprintf(death_msg, sizeof(death_msg), "^O%s^O's flames consume %s^O, leaving %s body a smoking heap!^n",
           capitalize(GET_CHAR_NAME(def->ch)),
           decapitalize_a_an(GET_CHAR_NAME(att->ch)),
           HSHR(att->ch));
  // Deal damage based on boxes instead of wound levels.
  if (damage(def->ch, att->ch, converted_damage, TYPE_SUFFERING, PHYSICAL)) {
    // We killed them! Write our pre-written message and bail out.
    act(death_msg, TRUE, 0, 0, def->ch, TO_VICT);
    act(death_msg, TRUE, def->ch, 0, 0, TO_ROOM);

    // Attacker died, so return TRUE here.
    return TRUE;
  } else {
    // They lived-- give a standard message.
    switch (MAX(LIGHT, MIN(DEADLY, staged_damage))) {
      case LIGHT:
        act("^oYou singe yourself on $S flames.^n", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("^o$n singes $mself on the flames.^n", FALSE, att->ch, 0, def->ch, TO_ROOM);
        break;
      case MODERATE:
        act("^oYou burn yourself on $S flames.^n", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("^o$n burns $mself on the flames.^n", FALSE, att->ch, 0, def->ch, TO_ROOM);
        break;
      case SERIOUS:
        act("^OYou catch a deep burn from $S flames.^n", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("^O$n^O catches a deep burn from the flames.^n", FALSE, att->ch, 0, def->ch, TO_ROOM);
        break;
      case DEADLY:
        act("^R$S flames burn you to the bone!^n", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("^RThe flames burn $n^R to the bone!^n", FALSE, att->ch, 0, def->ch, TO_ROOM);
        break;
      default:
        mudlog("SYSERR: Received invalid code to switch in handle_flame_aura()!", att->ch, LOG_SYSLOG, TRUE);
        break;
    }
  }

  // We survived! Return FALSE to indicate attacker did not die.
  return FALSE;
}

// Returns true if we struck, false otherwise.
bool perform_nerve_strike(struct combat_data *att, struct combat_data *def, char *rbuf, size_t rbuf_len) {
  if (!att || !def) {
    mudlog("SYSERR: Received NULL struct(s) to perform_nerve_strike!", att->ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (att->weapon || !IS_NERVE(att->ch)) {
    mudlog("SYSERR: Somehow, we reached perform_nerve_strike with invalid preconditions.", att->ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (IS_SPIRIT(def->ch) || IS_ANY_ELEMENTAL(def->ch) || (IS_NPC(def->ch) && MOB_FLAGGED(def->ch, MOB_INANIMATE))) {
    send_to_char(att->ch, "You can't find any nerves to strike!\r\n");
    return FALSE;
  }

  // Ensure that neither combatant has the closing flag set.
  AFF_FLAGS(att->ch).RemoveBit(AFF_APPROACH);
  AFF_FLAGS(def->ch).RemoveBit(AFF_APPROACH);

  if (GET_QUI(def->ch) <= 0) {
    send_to_char(att->ch, "Your victim is already paralyzed, so you hold your attack.\r\n");
    return TRUE;
  }

  int impact_armor = GET_IMPACT(def->ch);
  // Apply Penetrating Strike to the nerve strike.
  if (GET_POWER(att->ch, ADEPT_PENETRATINGSTRIKE) && !GET_POWER(att->ch, ADEPT_DISTANCE_STRIKE)) {
    impact_armor = MAX(0, GET_IMPACT(def->ch) - GET_POWER(att->ch, ADEPT_PENETRATINGSTRIKE));
  }

  // Calculate and display pre-success-test information.
  snprintf(rbuf, rbuf_len, "%s VS %s: Nerve Strike target is 4 + impact %d (PS: -%d) + modifiers: ",
           GET_CHAR_NAME(att->ch),
           GET_CHAR_NAME(def->ch),
           GET_IMPACT(def->ch),
           GET_POWER(att->ch, ADEPT_PENETRATINGSTRIKE)
         );

  att->melee->tn += impact_armor + modify_target_rbuf_raw(att->ch, rbuf, rbuf_len, att->melee->modifiers[COMBAT_MOD_VISIBILITY], FALSE);

  for (int mod_index = 0; mod_index < NUM_COMBAT_MODIFIERS; mod_index++) {
    buf_mod(rbuf, rbuf_len, combat_modifiers[mod_index], att->melee->modifiers[mod_index]);
    att->melee->tn += att->melee->modifiers[mod_index];
  }

  snprintf(ENDOF(rbuf), rbuf_len - strlen(rbuf), ". Total TN is %d.", att->melee->tn);
  SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;

  // Calculate the attacker's total skill and execute a success test.
  att->melee->dice = get_skill(att->ch, SKILL_UNARMED_COMBAT, att->melee->tn);
  if (!att->too_tall) {
    int bonus = MIN(GET_SKILL(att->ch, SKILL_UNARMED_COMBAT), GET_OFFENSE(att->ch));
    snprintf(rbuf, rbuf_len, "Attacker is rolling %d + %d dice", att->melee->dice, bonus);
    att->melee->dice += bonus;
  } else {
    snprintf(rbuf, rbuf_len, "Attacker is rolling %d dice (no bonus: too tall)", att->melee->dice);
  }

  att->melee->successes = success_test(att->melee->dice, att->melee->tn);
  int temp_qui_loss = (int) (att->melee->successes / 2);
  snprintf(ENDOF(rbuf), rbuf_len - strlen(rbuf), ", and got %d successes, which translates to %d qui loss.",
           att->melee->successes,
           temp_qui_loss);
  SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER;

  if (att->melee->successes > 1) {
    GET_TEMP_QUI_LOSS(def->ch) += temp_qui_loss * TEMP_QUI_LOSS_DIVISOR;
    affect_total(def->ch);

    if (access_level(att->ch, LVL_ADMIN) && PRF_FLAGGED(att->ch, PRF_ROLLS)) {
      send_to_char(att->ch, "[Remaining qui: %d (TQL %d).]\r\n",
               GET_QUI(def->ch),
               GET_TEMP_QUI_LOSS(def->ch));
    }

    char msg_buf[500];
    if (GET_QUI(def->ch) <= 0) {
      snprintf(msg_buf, sizeof(msg_buf), "You hit $N's pressure points successfully, %s %s paralyzed!", HSSH(def->ch), HSSH_SHOULD_PLURAL(def->ch) ? "is" : "are");
      act(msg_buf, FALSE, att->ch, 0, def->ch, TO_CHAR);
      act("As $n hits you, you feel your body freeze up!", FALSE, att->ch, 0, def->ch, TO_VICT);
      act("$N freezes as $n's attack lands successfully!", FALSE, att->ch, 0, def->ch, TO_NOTVICT);
    } else {
      snprintf(msg_buf, sizeof(msg_buf), "You hit $N's pressure points successfully, %s seem%s to slow down!", HSSH(def->ch), HSSH_SHOULD_PLURAL(def->ch) ? "s" : "");
      act(msg_buf, FALSE, att->ch, 0, def->ch, TO_CHAR);
      act("$n's blows seem to bring great pain and you find yourself moving slower!", FALSE, att->ch, 0, def->ch, TO_VICT);
      act("$n's attack hits $N, who seems to move slower afterwards.", FALSE, att->ch, 0, def->ch, TO_NOTVICT);
    }
  } else {
    act("You fail to hit any of the needed pressure points on $N.", FALSE, att->ch, 0, def->ch, TO_CHAR);
    act("$n fails to land any blows on you.", FALSE, att->ch, 0, def->ch, TO_VICT);
    act("$n's unarmed attack misses $N completely.", TRUE, att->ch, 0, def->ch, TO_NOTVICT);
  }

  handle_flame_aura(att, def);

  return TRUE;
}

#undef SEND_RBUF_TO_ROLLS_FOR_BOTH_ATTACKER_AND_DEFENDER
