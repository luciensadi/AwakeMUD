#ifndef __newfight_h_
#define __newfight_h_

extern int get_weapon_damage_type(struct obj_data* weapon);
extern int check_recoil(struct char_data *ch, struct obj_data *gun, bool is_using_gyromount=FALSE);
extern int check_smartlink(struct char_data *ch, struct obj_data *weapon, bool only_check_cyberware=false);

bool does_weapon_have_bayonet(struct obj_data *weapon);
bool perform_nerve_strike(struct combat_data *att, struct combat_data *def, char *rbuf, size_t rbuf_len);

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
  int cyberfangs;
  int cyberhorns;
  bool cyberarms;
  int bone_lacing_power;
  int num_cyberweapons;
  bool cyberarm_gyromount;

  cyberware_data(struct char_data *ch) :
    climbingclaws(0), fins(0), handblades(0), handrazors(0), improved_handrazors(0),
    handspurs(0), footanchors(0), cyberfangs(0), cyberhorns(0), cyberarms(FALSE), bone_lacing_power(0), num_cyberweapons(0),
    cyberarm_gyromount(FALSE)
  {
    assert(ch != NULL);

    // Populate the data.
    for (struct obj_data *ware = ch->cyberware; ware; ware = ware->next_content) {
      if (GET_CYBERWARE_TYPE(ware) == CYB_BONELACING) {
        bone_lacing_power = MAX(bone_lacing_power, bone_lacing_power_lookup[GET_CYBERWARE_LACING_TYPE(ware)]);
      } else if (GET_CYBERWARE_TYPE(ware) == CYB_ARMS) {
        cyberarms = TRUE;
        if (IS_SET(GET_CYBERWARE_FLAGS(ware), ARMS_MOD_GYROMOUNT) && !GET_CYBERWARE_IS_DISABLED(ware)) {
          cyberarm_gyromount = TRUE;
        }
      } else if (!GET_CYBERWARE_IS_DISABLED(ware)) {
        switch (GET_CYBERWARE_TYPE(ware)) {
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
            if (IS_SET(GET_CYBERWARE_FLAGS(ware), 1 << CYBERWEAPON_IMPROVED))
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
          case CYB_FANGS:
            cyberfangs++;
            num_cyberweapons++;
            break;
          case CYB_HORNS:
            cyberhorns++;
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
  int power_before_armor;
  int dam_type;
  int damage_level;
  int unaugmented_damage_level;
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
    skill(0), power(0), power_before_armor(0), dam_type(0), damage_level(0), unaugmented_damage_level(0),
    is_physical(FALSE), tn(4), dice(0), successes(0), is_gel(FALSE), burst_count(0), recoil_comp(0),
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
      power_before_armor = GET_WEAPON_POWER(weapon);
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
      if (magazine && !IS_NPC(ch) && !access_level(ch, LVL_BUILDER) && GET_MAGAZINE_AMMO_TYPE(magazine) == AMMO_AV) {
        mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: PC %s had AV ammo in newfight struct init. Changing to APDS.", GET_CHAR_NAME(ch));
        GET_MAGAZINE_AMMO_TYPE(magazine) = AMMO_APDS;
      }

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
  int power_before_armor;
  int dam_type;
  int damage_level;
  bool is_physical;
  int tn;
  int dice;
  int successes;
  int reach_modifier;
  bool is_monowhip;
  struct obj_data *riot_shield;

  int modifiers[NUM_COMBAT_MODIFIERS];

  melee_combat_data(struct char_data *ch, struct obj_data *weapon, bool ranged_combat_mode, struct cyberware_data *cyber) :
    skill(0), skill_bonus(0), power(0), power_before_armor(0), dam_type(0), damage_level(0), is_physical(FALSE), tn(4), 
    dice(0), successes(0), reach_modifier(0), is_monowhip(FALSE), riot_shield(NULL)
  {
    assert(ch != NULL);

    for (int i = 0; i < NUM_COMBAT_MODIFIERS; i++)
      modifiers[i] = 0;

    // Set up melee combat data. This holds true for all melee combat, but can be overwritten later on.
    skill = SKILL_UNARMED_COMBAT;
    power_before_armor = GET_STR(ch);
    dam_type = TYPE_HIT;
    damage_level = MODERATE;

    if (weapon && GET_OBJ_TYPE(weapon) == ITEM_WEAPON) {
      if (ranged_combat_mode) {
        switch (GET_WEAPON_SKILL(weapon)) {
          case SKILL_PISTOLS:
          case SKILL_TASERS:
          case SKILL_SMG:
            // These weapons all grant +1 power per CC p11.
            power_before_armor += 1;
            break;
          default:
            // All others are presumed rifle-size, and get +2 power.
            power_before_armor += 2;
            break;
        }

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
        power_before_armor += GET_WEAPON_STR_BONUS(weapon);
        damage_level = GET_WEAPON_DAMAGE_CODE(weapon);

        // Weapon foci. NPC use them implicitly.
        if (is_weapon_focus_usable_by(weapon, ch)) {
          skill_bonus = MIN(4, GET_WEAPON_FOCUS_RATING(weapon));
        }

        // Monowhips.
        is_monowhip = obj_index[GET_OBJ_RNUM(weapon)].wfunc == monowhip;
      }
    } else if (cyber->num_cyberweapons > 0) {
      skill = SKILL_CYBER_IMPLANTS;

      // Select the best cyberweapon and use its stats.
      if (cyber->handblades) {
        power_before_armor += 3;
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
          power_before_armor += (int) (GET_STR(ch) / 2);
        }
      }
      else if (cyber->improved_handrazors) {
        power_before_armor += 2;
        damage_level = LIGHT;
        dam_type = TYPE_STAB;
        is_physical = TRUE;

        if (cyber->improved_handrazors >= 2) {
          // Dual cyberweapons gives a power bonus per Core p121.
          power_before_armor += (int) (GET_STR(ch) / 2);
        }
      }
      else if (cyber->handrazors) {
        damage_level = LIGHT;
        dam_type = TYPE_CLAW;
        is_physical = TRUE;

        if (cyber->handrazors >= 2) {
          // Dual cyberweapons gives a power bonus per Core p121.
          power_before_armor += (int) (GET_STR(ch) / 2);
        }
      }
      else if (cyber->fins || cyber->climbingclaws) {
        power_before_armor -= 1;
        damage_level = LIGHT;
        dam_type = TYPE_SLASH;
        is_physical = TRUE;
      }
      else if (cyber->footanchors) {
        power_before_armor -= 1;
        damage_level = LIGHT;
        dam_type = TYPE_STAB;
        is_physical = TRUE;
      }
      else if (cyber->cyberfangs) {
        power_before_armor += 1;
        damage_level = LIGHT;
        dam_type = TYPE_BITE;
        is_physical = TRUE;
        reach_modifier = -1;
      }
      else if (cyber->cyberhorns) {
        damage_level = MODERATE;
        dam_type = TYPE_GORE;
        is_physical = TRUE;
        reach_modifier = -1;
      }
      else {
        snprintf(buf, sizeof(buf), "SYSERR in hit(): num_cyberweapons %d but no weapons found.", cyber->num_cyberweapons);
        mudlog(buf, ch, LOG_SYSLOG, TRUE);
      }
    } else {
      // TODO: Implement option to use bone lacing to cause physical damage at half power.
      // No cyberweapons active-- check for bone lacing, then proceed with adept/standard slapping.
      if (cyber->bone_lacing_power) {
        power_before_armor += cyber->bone_lacing_power;
        damage_level = MODERATE;
        is_physical = FALSE;
      }

      // Add +2 to unarmed attack power for having cyberarms, per M&M p32.
      if (cyber->cyberarms) {
        power_before_armor += 2;
      }

      // Add +2 power to unarmed/melee, per MitS p147. -Vile
      if (AFF_FLAGGED(ch, AFF_FLAME_AURA) || MOB_FLAGGED(ch, MOB_FLAMEAURA)) {
        power_before_armor += 2;
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

    // Check for riot shields.
    riot_shield = GET_EQ(ch, WEAR_SHIELD);
    if (!riot_shield || GET_OBJ_TYPE(riot_shield) != ITEM_WORN) {
      riot_shield = NULL;
    }
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
  int standard_ballistic_rating;
  int standard_impact_rating;
  int hardened_armor_ballistic_rating;
  int hardened_armor_impact_rating;
  bool is_paralyzed_or_insensate;
  bool is_surprised;

  // Pool data for the things that can be temporarily overwritten mid-fight.
  int dodge_pool;
  int body_pool;

  bool using_gyro;

  combat_data(struct char_data *character, struct obj_data *weap) :
    ch(NULL),
    veh(NULL),
    weapon(NULL),
    ranged_combat_mode(FALSE),
    standard_ballistic_rating(GET_BALLISTIC(character)),
    standard_impact_rating(GET_IMPACT(character)),
    hardened_armor_ballistic_rating(0),
    hardened_armor_impact_rating(0),
    is_surprised(AFF_FLAGGED(character, AFF_SURPRISE)),
    dodge_pool(GET_DODGE(character)),
    body_pool(GET_BODY_POOL(character))
  {
    ch = character;

    assert(ch != NULL);

    weapon = weap;

    ranged_combat_mode = (weap
                          && GET_OBJ_TYPE(weap) == ITEM_WEAPON
                          && WEAPON_IS_GUN(weapon));

    cyber = new struct cyberware_data(ch);
    ranged = new struct ranged_combat_data(ch, weapon, ranged_combat_mode);
    melee = new struct melee_combat_data(ch, weapon, ranged_combat_mode, cyber);

    // Special case: Bayonet charge and/or butt strikes. Don't execute this for mounted weapons.
    if (ranged_combat_mode && !ranged->using_mounted_gun && !weapon->contains)
      ranged_combat_mode = FALSE;

    // Calculate hardened armor ratings, if any. We add all hardened items together (e.g. armor + helm)
    hardened_armor_ballistic_rating = get_hardened_ballistic_armor_rating(ch);
    hardened_armor_impact_rating = get_hardened_impact_armor_rating(ch);

    // Figure out if they're unable to move at all (paralyzed, asleep, jacked in, etc)
    is_paralyzed_or_insensate = !AWAKE(ch) || GET_QUI(ch) <= 0;

    // Check if they're using a gyromount.
    using_gyro = (ranged->gyro || cyber->cyberarm_gyromount);
  }

  ~combat_data() {
    delete cyber;
    delete ranged;
    delete melee;
  }
};

#endif // __newfight_h_
