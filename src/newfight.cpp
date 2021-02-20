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
extern void astral_fight(struct char_data *ch, struct char_data *vict);
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

bool does_weapon_have_bayonet(struct obj_data *weapon);

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
  int bone_lacing_power;
  int num_cyberweapons;
  
  cyberware_data(struct char_data *ch) :
    climbingclaws(0), fins(0), handblades(0), handrazors(0), improved_handrazors(0),
    handspurs(0), footanchors(0), bone_lacing_power(0), num_cyberweapons(0)
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
            if (IS_SET(GET_CYBERWARE_FLAGS(obj), CYBERWEAPON_IMPROVED))
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
  
  int burst_count;
  int recoil_comp;
  
  bool using_mounted_gun;
  
  int modifiers[NUM_COMBAT_MODIFIERS];
  
  struct obj_data *magazine;
  struct obj_data *gyro;
  
  ranged_combat_data(struct char_data *ch, struct obj_data *weapon, bool weapon_is_gun) : 
    skill(0), power(0), dam_type(0), damage_level(0), is_physical(FALSE),
    burst_count(0), recoil_comp(0), using_mounted_gun(FALSE),
    magazine(NULL), gyro(NULL)
  {
    memset(modifiers, 0, sizeof(modifiers));
    
    assert(ch != NULL);
    
    // Setup: Cyclops suffer a +2 ranged-combat modifier.
    if (GET_RACE(ch) == RACE_CYCLOPS)
      modifiers[COMBAT_MOD_DISTANCE] += 2;
    
    if (weapon && weapon_is_gun) {
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
        modifiers[COMBAT_MOD_DUAL_WIELDING] += 2;
      else
        modifiers[COMBAT_MOD_SMARTLINK] -= check_smartlink(ch, weapon);
    }
  }
};

struct melee_combat_data {
  int skill;
  int power;
  int dam_type;
  int damage_level;
  bool is_physical;
  
  int modifiers[NUM_COMBAT_MODIFIERS];
  
  melee_combat_data(struct char_data *ch, struct obj_data *weapon, bool weapon_is_gun, struct cyberware_data *cyber) :
    skill(0), power(0), dam_type(0), damage_level(0), is_physical(FALSE)
  {
    assert(ch != NULL);
    
    memset(modifiers, 0, sizeof(modifiers));
    
    // Set up melee combat data. This holds true for all melee combat, but can be overwritten later on.
    skill = SKILL_UNARMED_COMBAT;
    power = GET_STR(ch);
    dam_type = TYPE_HIT;
    damage_level = MODERATE;
    
    if (weapon) {
      if (weapon_is_gun) {
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
      }
    } else if (cyber->num_cyberweapons > 0) {
      skill = SKILL_CYBER_IMPLANTS;
      
      if (cyber->num_cyberweapons >= 2) {
        // Dual cyberweapons gives a power bonus per Core p121.
        power += (int) (GET_STR(ch) / 2);
      }
      
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
      }
      else if (cyber->improved_handrazors) {
        power += 2;
        damage_level = LIGHT;
        dam_type = TYPE_STAB;
        is_physical = TRUE;
      }
      else if (cyber->handrazors) {
        damage_level = LIGHT;
        dam_type = TYPE_STAB;
        is_physical = TRUE;
      }
      else if (cyber->fins || cyber->climbingclaws) {
        power -= 1;
        damage_level = LIGHT;
        dam_type = TYPE_SLASH;
        is_physical = TRUE;
      }
      else if (cyber->footanchors) {
        power -= 1;
        damage_level = LIGHT;
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
  bool weapon_is_gun;
  
  struct cyberware_data *cyber;
  struct ranged_combat_data *ranged;
  struct melee_combat_data *melee;
  
  // Generic combat data.
  bool too_tall;
  int skill;
  int tn;
  int dice;
  int successes;
  
  int modifiers[NUM_COMBAT_MODIFIERS];
  
  combat_data(struct char_data *character, struct obj_data *weap) :
    ch(NULL), 
    veh(NULL), 
    weapon(NULL),
    weapon_is_gun(FALSE),
    too_tall(FALSE), skill(0), tn(4), dice(0), successes(0)
  {    
    memset(modifiers, 0, sizeof(modifiers));
    
    ch = character;
    
    assert(ch != NULL);
    
    too_tall = is_char_too_tall(ch);
    
    weapon = weap;
    weapon_is_gun = weapon
                    && IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))
                    && (GET_WEAPON_SKILL(weapon) >= SKILL_PISTOLS
                        && GET_WEAPON_SKILL(weapon) <= SKILL_ASSAULT_CANNON);
    
    cyber = new struct cyberware_data(ch);
    ranged = new struct ranged_combat_data(ch, weapon, weapon_is_gun);
    melee = new struct melee_combat_data(ch, weapon, weapon_is_gun, cyber);
  }
  
  ~combat_data() {
    delete cyber;
    delete ranged;
    delete melee;
  }
};

void hit(struct char_data *attacker, struct char_data *victim, struct obj_data *weap, struct obj_data *vict_weap, struct obj_data *weap_ammo)
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
  
  // Precondition: If you're wielding a non-weapon, back out.
  if (att->weapon && (GET_OBJ_TYPE(att->weapon) != ITEM_WEAPON)) {
    send_to_char(att->ch, "You struggle to figure out how to attack while using %s as a weapon!\r\n", decapitalize_a_an(GET_OBJ_NAME(att->weapon)));
    return;
  }
  
  // Precondition: If you're asleep or paralyzed, you don't get to fight, and also your opponent closes immediately.
  if (!AWAKE(att->ch) || GET_QUI(att->ch) <= 0) {
    AFF_FLAGS(att->ch).RemoveBit(AFF_APPROACH);
    AFF_FLAGS(def->ch).RemoveBit(AFF_APPROACH);
    
    if (AWAKE(att->ch)) {
      send_to_char("You can't react-- you're paralyzed!\r\n", att->ch);
    }
    return;
  }
  
  // Precondition: If you're out of ammo, you don't get to fight. Note the use of the deducting has_ammo here.
  if (att->weapon && !has_ammo(att->ch, att->weapon))
    return;
  
  // Short-circuit: If you're wielding an activated Dominator, you don't care about all these pesky rules.
  if (att->weapon && GET_OBJ_SPEC(att->weapon) == weapon_dominator) {
    if (GET_LEVEL(def->ch) > GET_LEVEL(att->ch)) {
      send_to_char(att->ch, "As you aim your weapon at %s, a dispassionate feminine voice states: \"^cThe target's Crime Coefficient is below 100. %s is not a target for enforcement. The trigger has been locked.^n\"\r\n", GET_CHAR_NAME(def->ch), HSSH(def->ch));
      dominator_mode_switch(att->ch, att->weapon, DOMINATOR_MODE_DEACTIVATED);
      return;
    }
    switch (GET_WEAPON_ATTACK_TYPE(att->weapon)) {
      case WEAP_TASER:
        // Paralyzer? Stun your target.
        act("Your crackling shot of energy slams into $N, disabling $M.", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("A crackling shot of energy erupts from $n's Dominator and slams into $N, disabling $M!", FALSE, att->ch, 0, def->ch, TO_NOTVICT);
        act("A crackling shot of energy erupts from $n's Dominator and slams into you! Your vision fades as your muscles lock up.", FALSE, att->ch, 0, def->ch, TO_VICT);
        GET_MENTAL(def->ch) = -10;
        break;
      case WEAP_HEAVY_PISTOL:
        // Lethal? Kill your target.
        act("A ball of coherent light leaps from your Dominator, tearing into $N. With a scream, $E crumples, bubbles, and explodes in a shower of gore!", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("A ball of coherent light leaps from $n's Dominator, tearing into $N. With a scream, $E crumples, bubbles, and explodes in a shower of gore!", FALSE, att->ch, 0, def->ch, TO_NOTVICT);
        act("A ball of coherent light leaps from $n's Dominator, tearing into you! A horrible rending sensation tears through you as your vision fades.", FALSE, att->ch, 0, def->ch, TO_VICT);
        die(def->ch);
        break;
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
        break;
      default:
        send_to_char(att->ch, "Dominator code fault.\r\n");
        break;
    }
    return;
  }
  
  // Precondition: If your foe is astral, you don't belong here.
  if (IS_ASTRAL(def->ch)) {
    if (IS_DUAL(att->ch) || IS_ASTRAL(att->ch))
      astral_fight(att->ch, def->ch);
    return;
  }
  
  // Precondition: If you're in melee combat and your foe isn't present, stop fighting.
  if (!att->weapon_is_gun && att->ch->in_room != def->ch->in_room) {
    send_to_char(att->ch, "You relax with the knowledge that your opponent is no longer present.\r\n");
    stop_fighting(att->ch);
    return;
  }
  
  // Remove closing flags if both are melee.
  if ((!att->weapon_is_gun || AFF_FLAGGED(att->ch, AFF_APPROACH))
      && (!def->weapon_is_gun || AFF_FLAGGED(def->ch, AFF_APPROACH))) 
  {
    AFF_FLAGS(att->ch).RemoveBit(AFF_APPROACH);
    AFF_FLAGS(def->ch).RemoveBit(AFF_APPROACH);
  }
  
  // Setup: Calculate sight penalties.
  att->modifiers[COMBAT_MOD_VISIBILITY] += calculate_vision_penalty(att->ch, def->ch);    
  def->modifiers[COMBAT_MOD_VISIBILITY] += calculate_vision_penalty(def->ch, att->ch);
  
  // Early execution: Nerve strike doesn't require as much setup, so perform it here to save on resources.
  if (!att->weapon 
      && IS_NERVE(att->ch) 
      && !IS_SPIRIT(def->ch) 
      && !IS_ELEMENTAL(def->ch) 
      && !(IS_NPC(def->ch) && MOB_FLAGGED(def->ch, MOB_INANIMATE))) 
  {
    // Calculate and display pre-success-test information.
    snprintf(rbuf, sizeof(rbuf), "%s VS %s: Nerve Strike target is 4 + impact (%d) + modifiers: ",
             GET_CHAR_NAME(att->ch), 
             GET_CHAR_NAME(def->ch),
             GET_IMPACT(def->ch));
    
    att->tn += GET_IMPACT(def->ch) + modify_target_rbuf_raw(att->ch, rbuf, sizeof(rbuf), att->modifiers[COMBAT_MOD_VISIBILITY]);
    
    for (int mod_index = 0; mod_index < NUM_COMBAT_MODIFIERS; mod_index++) {
      buf_mod(rbuf, sizeof(rbuf), combat_modifiers[mod_index], att->modifiers[mod_index]);
      att->tn += att->modifiers[mod_index];
    }
    
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), ". Total TN is %d.", att->tn);
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    
    // Calculate the attacker's total skill and execute a success test.
    att->dice = get_skill(att->ch, SKILL_UNARMED_COMBAT, att->tn);
    if (!att->too_tall) {
      int bonus = MIN(GET_SKILL(att->ch, SKILL_UNARMED_COMBAT), GET_OFFENSE(att->ch));
      snprintf(rbuf, sizeof(rbuf), "Attacker is rolling %d + %d dice", att->dice, bonus);
      att->dice += bonus;
    } else {
      snprintf(rbuf, sizeof(rbuf), "Attacker is rolling %d dice (no bonus: too tall)", att->dice);
    }
    
    if (GET_QUI(def->ch) <= 0) {
      strncat(rbuf, "-- but we're zeroing out successes since the defender is paralyzed.", sizeof(rbuf) - strlen(rbuf) - 1);
      att->successes = 0;
    }
    else {
      att->successes = success_test(att->dice, att->tn);
      snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), ", and got %d successes, which translates to %d qui loss.", 
               att->successes,
               (int) (att->successes / 2));
    }
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
      
    if (att->successes > 1) {
      GET_TEMP_QUI_LOSS(def->ch) += (int) (att->successes / 2); // This used to be * 2!
      affect_total(def->ch);
      if (GET_QUI(def->ch) <= 0) {
        act("You hit $N's pressure points successfully, $e is paralyzed!", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("As $n hits you, you feel your body freeze up!", FALSE, att->ch, 0, def->ch, TO_VICT);
        act("$N freezes as $n's attack lands successfully!", FALSE, att->ch, 0, def->ch, TO_NOTVICT);
      } else {
        act("You hit $N's pressure points successfully, $e seems to slow down!", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("$n's blows seem to bring great pain and you find yourself moving slower!", FALSE, att->ch, 0, def->ch, TO_VICT);
        act("$n's attack hits $N, who seems to move slower afterwards.", FALSE, att->ch, 0, def->ch, TO_NOTVICT);
      }
    } else {
      act("You fail to hit any of the needed pressure points on $N.", FALSE, att->ch, 0, def->ch, TO_CHAR);
      act("$n fails to land any blows on you.", FALSE, att->ch, 0, def->ch, TO_VICT);
      act("$n's unarmed attack misses $N completely.", TRUE, att->ch, 0, def->ch, TO_NOTVICT);
    }
    return;
  }
  
  // Setup: If the character is rigging a vehicle or is in a vehicle, set veh to that vehicle.
  RIG_VEH(att->ch, att->veh);
  
  if (att->veh && !att->weapon) {
    mudlog("SYSERR: Somehow, we ended up in a vehicle attacking someone with no weapon!", att->ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  // Setup for ranged combat.
  if (att->weapon_is_gun) {
    // Precondition: If you're using a heavy weapon, you must be strong enough to wield it, or else be using a gyro. CC p99
    if (!att->ranged->using_mounted_gun
        && !att->ranged->gyro 
        && !IS_NPC(att->ch)
        && (att->ranged->skill >= SKILL_MACHINE_GUNS && att->ranged->skill <= SKILL_ASSAULT_CANNON)
        && (GET_STR(att->ch) < 8 || GET_BOD(att->ch) < 8)
        && !(AFF_FLAGGED(att->ch, AFF_PRONE)))
    {
      send_to_char("You can't lift the barrel high enough to fire.\r\n", att->ch);
      return;
    }
    
    // Setup: Limit the burst of the weapon to the available ammo, and decrement ammo appropriately.
    if (att->ranged->burst_count) {
      if (weap_ammo || att->ranged->magazine) {
        int ammo_available = weap_ammo ? ++GET_AMMOBOX_QUANTITY(weap_ammo) : ++GET_MAGAZINE_AMMO_COUNT(att->ranged->magazine);
        
        // Cap their burst to their magazine's ammo.
        att->ranged->burst_count = MIN(att->ranged->burst_count, ammo_available);
        
        // When we called has_ammo() earlier, we decremented their ammo by one. Give it back to true up the equation.
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
    
    // Setup: Trying to fire a sniper rifle at close range is tricky. This is non-canon to reduce twinkery.
    if (IS_OBJ_STAT(att->weapon, ITEM_SNIPER) 
        && (att->ch->in_room == def->ch->in_room 
            || att->ranged->using_mounted_gun))
    {
      att->ranged->modifiers[COMBAT_MOD_DISTANCE] += 6;
    }
    
    // Setup: Compute modifiers to the TN based on the def->ch's current state.
    if (!AWAKE(def->ch))
      att->ranged->modifiers[COMBAT_MOD_POSITION] -= 6;
    else if (AFF_FLAGGED(def->ch, AFF_PRONE)) {
      // Prone next to you is a bigger target, prone far away is a smaller one.
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
        return;
      }
    }
    
    // Setup: If your attacker is closing the distance (running), take a penalty per Core p112.
    if (AFF_FLAGGED(def->ch, AFF_APPROACH))
      att->ranged->modifiers[COMBAT_MOD_DEFENDER_MOVING] += 2;
    else if (!def->weapon_is_gun && def->ch->in_room == att->ch->in_room)
      att->ranged->modifiers[COMBAT_MOD_IN_MELEE_COMBAT] += 2; // technically supposed to be +2 per attacker, but ehhhh.
    
    // Setup: If you have a gyro mount, it negates recoil and movement penalties up to its rating.
    if (att->ranged->gyro && !att->ranged->using_mounted_gun)
      att->ranged->modifiers[COMBAT_MOD_GYRO] -= MIN(att->ranged->modifiers[COMBAT_MOD_MOVEMENT] + att->ranged->modifiers[COMBAT_MOD_RECOIL], GET_OBJ_VAL(att->ranged->gyro, 0));
    
    // Calculate and display pre-success-test information.
    snprintf(rbuf, sizeof(rbuf), "%s's burst/compensation info is %d/%d. Additional modifiers: ", 
             GET_CHAR_NAME( att->ch ),
             att->ranged->burst_count, 
             att->ranged->recoil_comp);
             
    att->tn += modify_target_rbuf_raw(att->ch, rbuf, sizeof(rbuf), att->modifiers[COMBAT_MOD_VISIBILITY]);
    for (int mod_index = 0; mod_index < NUM_COMBAT_MODIFIERS; mod_index++) {
      // Ranged-specific modifiers.
      buf_mod(rbuf, sizeof(rbuf), combat_modifiers[mod_index], att->ranged->modifiers[mod_index]);
      att->tn += att->ranged->modifiers[mod_index];
      
      // General modifiers.
      buf_mod(rbuf, sizeof(rbuf), combat_modifiers[mod_index], att->modifiers[mod_index]);
      att->tn += att->modifiers[mod_index];
    }
    
    // Calculate the attacker's total skill (this modifies TN)
    att->dice = get_skill(att->ch, att->ranged->skill, att->tn);
    
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "\r\nThus, attacker's TN is: %d.", att->tn);
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    
    // Execute skill test
    if (!att->too_tall) {
      int bonus = MIN(GET_SKILL(att->ch, att->ranged->skill), GET_OFFENSE(att->ch));
      snprintf(rbuf, sizeof(rbuf), "Not too tall, so will roll %d + %d dice... ", att->dice, bonus);
      att->dice += bonus;
    } else {
      snprintf(rbuf, sizeof(rbuf), "Too tall, so will roll just %d dice... ", att->dice);
    }
    
    att->successes = success_test(att->dice, att->tn);
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "%d successes.", att->successes);
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    
    // Dodge test.
    if (AWAKE(def->ch) && !AFF_FLAGGED(def->ch, AFF_SURPRISE) && !def->too_tall && !AFF_FLAGGED(def->ch, AFF_PRONE)) {
      // Previous code only allowed you to sidestep if you had also allocated at least one normal dodge die. Why?
      def->dice = GET_DEFENSE(def->ch) + GET_POWER(def->ch, ADEPT_SIDESTEP);
      def->tn += damage_modifier(def->ch, buf, sizeof(buf)) + (int)(att->ranged->burst_count / 3) + def->cyber->footanchors;
      def->successes = MAX(success_test(def->dice, def->tn), 0);
      att->successes -= def->successes;
      
      snprintf(rbuf, sizeof(rbuf), "Dodge: Dice %d, TN %d, Successes %d.  This means attacker's net successes = %d.", def->dice, def->tn, def->successes, att->successes);
    } else {
      att->successes = MAX(att->successes, 1);
      snprintf(rbuf, sizeof(rbuf), "Opponent unable to dodge, successes confirmed as %d.", att->successes);
    }
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    
    // If the ranged attack failed, print the relevant message and terminate.
    if (att->successes < 1) {
      snprintf(rbuf, sizeof(rbuf), "%s failed to have net successes, so we're bailing out.", GET_CHAR_NAME(attacker));
      act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
      
      combat_message(att->ch, def->ch, att->weapon, -1, att->ranged->burst_count);
      damage(att->ch, def->ch, -1, att->ranged->dam_type, 0);
      return;
    }
    // But wait-- there's more! Next we jump down to dealing damage.
  }
  // Melee combat.
  
  // Setup for melee combat. Used for counterstrike.
  // todo asdf this whole setup is wrong, what about counterstriking someone who's wielding a ranged weapon? You never get your melee setup, so you're attacking, what, barehanded? And then the weapon code below could potentially just fall back to using your gun anyways    
  // Setup: Calculate position modifiers.
  if (AFF_FLAGGED(att->ch, AFF_PRONE))
    def->melee->modifiers[COMBAT_MOD_POSITION] -= 2;
  if (AFF_FLAGGED(def->ch, AFF_PRONE))
    att->melee->modifiers[COMBAT_MOD_POSITION] -= 2;
  
  // Spirits use different dice than the rest of us plebs.
  if (IS_SPIRIT(def->ch) || IS_ELEMENTAL(def->ch)) {
    att->dice = GET_WIL(att->ch);
    def->dice = GET_REA(def->ch);
  } else if (IS_SPIRIT(att->ch) || IS_ELEMENTAL(att->ch)) {
    att->dice = GET_REA(att->ch);
    def->dice = GET_WIL(def->ch);
  } else {
    act("Computing dice for attacker...", 1, att->ch, NULL, NULL, TO_ROLLS );
    att->dice = get_skill(att->ch, att->melee->skill, att->tn) + (att->too_tall ? 0 : MIN(GET_SKILL(att->ch, att->melee->skill), GET_OFFENSE(att->ch)));
    act("Computing dice for defender...", 1, att->ch, NULL, NULL, TO_ROLLS );
    def->dice = get_skill(def->ch, def->melee->skill, def->tn) + (def->too_tall ? 0 : MIN(GET_SKILL(def->ch, def->melee->skill), GET_OFFENSE(def->ch)));
  }
  
  // Adepts get bonus dice when counterattacking. Ip Man approves.
  if (GET_POWER(def->ch, ADEPT_COUNTERSTRIKE) > 0) {
    def->dice += GET_POWER(def->ch, ADEPT_COUNTERSTRIKE);
    snprintf(rbuf, sizeof(rbuf), "Defender counterstrike dice bonus is %d.", GET_POWER(def->ch, ADEPT_COUNTERSTRIKE));
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
  }
  
  // Bugfix: If you're unconscious or mortally wounded, you don't get to counterattack.
  if (GET_PHYSICAL(def->ch) <= 0 || GET_MENTAL(def->ch) <= 0) {
    def->dice = 0;
    act("^yDefender incapped, dice capped to zero.^n", 1, att->ch, NULL, NULL, TO_ROLLS );
  }
    
  snprintf(rbuf, sizeof(rbuf), "^g%s's dice: ^W%d^g, %s's dice: ^W%d^g.^n", GET_CHAR_NAME(att->ch), att->dice, GET_CHAR_NAME(def->ch), def->dice);
  act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
  
  // Calculate the net reach.
  int net_reach = GET_REACH(att->ch) - GET_REACH(def->ch);
  
  if (!GET_POWER(att->ch, ADEPT_PENETRATINGSTRIKE) && GET_POWER(att->ch, ADEPT_DISTANCE_STRIKE)) {
    // MitS 149: Ignore reach modifiers.
    net_reach = 0;
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
  att->tn += modify_target_rbuf_raw(att->ch, rbuf, sizeof(rbuf), att->melee->modifiers[COMBAT_MOD_VISIBILITY]);
  for (int mod_index = 0; mod_index < NUM_COMBAT_MODIFIERS; mod_index++) {
    // Melee-specific modifiers.
    buf_mod(rbuf, sizeof(rbuf), combat_modifiers[mod_index], att->melee->modifiers[mod_index]);
    att->tn += att->melee->modifiers[mod_index];
    
    // General modifiers.
    buf_mod(rbuf, sizeof(rbuf), combat_modifiers[mod_index], att->modifiers[mod_index]);
    att->tn += att->modifiers[mod_index];
  }
  act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
  
  snprintf(rbuf, sizeof(rbuf), "^c%s%s's TN modifiers: ", GET_CHAR_NAME( def->ch ),
          (GET_PHYSICAL(def->ch) <= 0 || GET_MENTAL(def->ch) <= 0) ? " (incap)" : "" );
  def->tn += modify_target_rbuf_raw(def->ch, rbuf, sizeof(rbuf), def->melee->modifiers[COMBAT_MOD_VISIBILITY]);
  for (int mod_index = 0; mod_index < NUM_COMBAT_MODIFIERS; mod_index++) {
    // Melee-specific modifiers.
    buf_mod(rbuf, sizeof(rbuf), combat_modifiers[mod_index], def->melee->modifiers[mod_index]);
    def->tn += def->melee->modifiers[mod_index];
    
    // General modifiers.
    buf_mod(rbuf, sizeof(rbuf), combat_modifiers[mod_index], def->modifiers[mod_index]);
    def->tn += def->modifiers[mod_index];
  }
  act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
  
  att->tn = MAX(att->tn, 2);
  def->tn = MAX(def->tn, 2);

  // ----------------------
  if (!att->weapon_is_gun) {
    if (AWAKE(def->ch) && !AFF_FLAGGED(def->ch, AFF_SURPRISE)) {
      att->successes = success_test(att->dice, att->tn);
      def->successes = success_test(def->dice, def->tn);
      net_successes = att->successes - def->successes;
    } else {
      act("Surprised-- defender gets no roll.", FALSE, att->ch, NULL, NULL, TO_ROLLS);
      att->successes = MAX(1, success_test(att->dice, att->tn));
      net_successes = att->successes;
    }
    
    successes_for_use_in_monowhip_test_check = att->successes;
    
    if (def->weapon && GET_OBJ_TYPE(def->weapon) != ITEM_WEAPON) {
      // Defender's wielding a non-weapon? Whoops, net successes will never be less than 0.
      act("Defender wielding non-weapon-- cannot win clash.", FALSE, att->ch, NULL, NULL, TO_ROLLS);
      net_successes = MAX(0, net_successes);
    }
    
    snprintf(rbuf, sizeof(rbuf), "^g%s got ^W%d^g success%s from ^W%d^g dice at TN ^W%d^g.^n\r\n",
             GET_CHAR_NAME(att->ch),
             att->successes,
             att->successes != 1 ? "s" : "",
             att->dice,
             att->tn);
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "^g%s got ^W%d^g success%s from ^W%d^g dice at TN ^W%d^g.^n\r\n",
             GET_CHAR_NAME(def->ch),
             def->successes,
             def->successes != 1 ? "s" : "",
             def->dice,
             def->tn);
             
    if (net_successes < 0) {
      snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "^yNet successes is ^W%d^y, which will cause a counterattack.^n\r\n", net_successes);
    } else
      snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "Net successes is ^W%d^n.\r\n", net_successes);
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    
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
            
      att->successes = -1 * net_successes;
      
      // Prevent ranged combat messaging.
      att->weapon_is_gun = FALSE;
    } else
      att->successes = net_successes;
  }
  
  // Calculate the power of the attack (applies to both melee and ranged).
  if (att->weapon) {
    // Ranged weapon?
    if (att->weapon_is_gun) {
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
          case AMMO_GEL:
            att->ranged->power -= GET_BALLISTIC(def->ch) + 2;
            break;
          default:
            att->ranged->power -= GET_BALLISTIC(def->ch);
        }
      }
      // Weapon fired without a magazine (probably by an NPC)-- we assume its ammo type is normal.
      else {
        att->ranged->power -= GET_BALLISTIC(def->ch);
      }
      
      // Increment character's shots_fired.
      if (GET_SKILL(att->ch, att->ranged->skill) >= 8 && SHOTS_FIRED(att->ch) < 10000)
        SHOTS_FIRED(att->ch)++;
    }
    // Melee weapon.
    else {
      if (att->weapon 
          && GET_OBJ_RNUM(att->weapon) >= 0
          && obj_index[GET_OBJ_RNUM(att->weapon)].wfunc == monowhip) {
        att->melee->power = 10;
        att->melee->damage_level = SERIOUS;
        
        att->melee->power -= GET_IMPACT(def->ch) / 2;
      }
      else {
        att->melee->power = GET_WEAPON_STR_BONUS(att->weapon) + GET_STR(att->ch);
        att->melee->power -= GET_IMPACT(def->ch);
      }        
    }
    
    // Core p113.
    att->ranged->power = MAX(att->ranged->power, 2);
    att->melee->power = MAX(att->melee->power, 2);
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
  
  // Handle spirits and elementals being little divas with their special combat rules.
  if (IS_SPIRIT(def->ch) || IS_ELEMENTAL(def->ch)) {
    if (att->weapon_is_gun) {
      if (att->ranged->power <= GET_LEVEL(def->ch) * 2)
        damage(att->ch, def->ch, 0, att->ranged->dam_type, att->ranged->is_physical);
      else
        att->ranged->power -= GET_LEVEL(def->ch) * 2;
      att->ranged->power = MAX(2, att->melee->power);
    } else {
      if (att->ranged->power <= GET_LEVEL(def->ch) * 2)
        combat_message(att->ch, def->ch, att->weapon, 0, 0);
      else
        att->melee->power -= GET_LEVEL(def->ch) * 2;
      att->melee->power = MAX(2, att->ranged->power);
    }
  }
  
  // Perform body test for damage resistance.
  int bod_success=0, bod = GET_BOD(def->ch) + (def->too_tall ? 0 : GET_BODY(def->ch));
  if (IS_SPIRIT(att->ch) && GET_MAG(def->ch) > 0 && GET_SKILL(def->ch, SKILL_CONJURING))
    bod_success = success_test(MAX(bod, GET_SKILL(def->ch, SKILL_CONJURING)), att->weapon_is_gun ? att->ranged->power : att->melee->power);
  else if (!AWAKE(def->ch))
    bod_success = 0;
  else {
    if (att->weapon_is_gun)
      bod_success = success_test(bod, att->ranged->power);
    else
      bod_success = success_test(bod, att->melee->power);
  }
  
  att->successes -= bod_success;
  
  int staged_damage = 0;
  
  // Adjust messaging for unkillable entities.
  if (can_hurt(att->ch, def->ch, att->weapon_is_gun ? att->ranged->dam_type : att->melee->dam_type, TRUE)) {
    if (att->weapon_is_gun)
      staged_damage = stage(att->successes, att->ranged->damage_level);
    else
      staged_damage = stage(att->successes, att->melee->damage_level);
  } else {
    staged_damage = -1;
    act("Damage reduced to -1 due to failure of CAN_HURT().", TRUE, att->ch, NULL, NULL, TO_ROLLS);
  }
  
  int damage_total = convert_damage(staged_damage);
  
  snprintf(rbuf, sizeof(rbuf), "^CBod dice %d, attack power after armor %d, BodSuc %d, ResSuc %d: Dam %s->%s. %d%c.^n",
          bod, 
          att->weapon_is_gun ? att->ranged->power : att->melee->power, 
          bod_success,
          att->successes,
          wound_name[MIN(DEADLY, MAX(0,att->weapon_is_gun ? att->ranged->damage_level : att->melee->damage_level))],
          wound_name[MIN(DEADLY, MAX(0, staged_damage))],
          damage_total, 
          (att->weapon_is_gun ? att->ranged->is_physical : att->melee->is_physical) ? 'P' : 'M');
  act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
  
  if (att->weapon_is_gun) {
    combat_message(att->ch, def->ch, att->weapon, MAX(0, damage_total), att->ranged->burst_count);
    damage(att->ch, def->ch, damage_total, att->ranged->dam_type, att->ranged->is_physical);
  } else {
    damage(att->ch, def->ch, damage_total, att->melee->dam_type, att->melee->is_physical);
  
    if (successes_for_use_in_monowhip_test_check <= 0) {
      struct obj_data *weapon = net_successes < 0 ? def->weapon : att->weapon;
      if (weapon && obj_index[GET_OBJ_RNUM(weapon)].wfunc == monowhip) {
        struct char_data *attacker = net_successes < 0 ? def->ch : att->ch;
        struct char_data *defender = net_successes < 0 ? att->ch : def->ch;
      
        int target = 6 + modify_target(attacker);
        int skill = get_skill(attacker, SKILL_WHIPS_FLAILS, target);
        int successes = success_test(skill, target);
        snprintf(rbuf, sizeof(rbuf), "Monowhip 'flailure' test: Skill of %d, target of %d, successes is %d.", skill, target, successes);
        act(rbuf, FALSE, attacker, NULL, NULL, TO_ROLLS);
        if (successes <= 0) {
          act("Your monowhip flails out of control, striking you instead of $N!", FALSE, attacker, 0, defender, TO_CHAR);
          act("$n's monowhip completely misses and recoils to hit $m!", TRUE, attacker, 0, 0, TO_ROOM);
          int dam_total = convert_damage(stage(-1 * success_test(GET_BOD(attacker) + (successes == 0 ? GET_DEFENSE(attacker) : 0), 10), SERIOUS));

          damage(attacker, attacker, dam_total, TYPE_RECOIL, PHYSICAL);
          return;
        }
      }
    }
  }
  
  if (!IS_NPC(att->ch) && IS_NPC(def->ch)) {
    GET_LASTHIT(def->ch) = GET_IDNUM(att->ch);
  }
  
  // If you're firing a heavy weapon without a gyro, you need to test against the damage of the recoil.
  if (att->weapon_is_gun
      && !att->ranged->using_mounted_gun
      && !IS_NPC(att->ch) 
      && !att->ranged->gyro 
      && att->ranged->skill >= SKILL_MACHINE_GUNS 
      && att->ranged->skill <= SKILL_ASSAULT_CANNON)
  {
    int recoil_successes = success_test(GET_BOD(att->ch) + GET_BODY(att->ch), GET_WEAPON_POWER(att->weapon) / 2 + modify_target(att->ch) + att->ranged->burst_count);
    int staged_dam = stage(-recoil_successes, LIGHT);
    snprintf(rbuf, sizeof(rbuf), "Heavy Recoil: %d successes, L->%s wound.", recoil_successes, staged_dam == LIGHT ? "L" : "no");
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    damage(att->ch, att->ch, convert_damage(staged_dam), TYPE_HIT, FALSE);
  }
  
  // Set the violence background count.
  struct room_data *room = get_ch_in_room(att->ch);
  if (room && !GET_BACKGROUND_COUNT(room)) {
    GET_SETTABLE_BACKGROUND_COUNT(room) = 1;
    GET_SETTABLE_BACKGROUND_AURA(room) = AURA_PLAYERCOMBAT;
  }
  
}

bool does_weapon_have_bayonet(struct obj_data *weapon) {
  // Precondition: Weapon must exist, must be a weapon-class item, and must be a gun.
  if (!(weapon && GET_OBJ_TYPE(weapon) == ITEM_WEAPON && IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))))
    return FALSE;

  long attach_obj;
  struct obj_data *attach_proto = NULL;
  
  return (GET_WEAPON_ATTACH_UNDER_VNUM(weapon)
          && (attach_obj = real_object(GET_WEAPON_ATTACH_UNDER_VNUM(weapon))) > 0
          && (attach_proto = &obj_proto[attach_obj])
          && GET_OBJ_VAL(attach_proto, 1) == ACCESS_BAYONET);
}
