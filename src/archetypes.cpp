#include "types.hpp"
#include "structs.hpp"
#include "awake.hpp"
#include "comm.hpp" // for shutdown()
#include "db.hpp"
#include "archetypes.hpp"

// todo
/*
Room 90700 - Hermetic south into 90701 - Shaman south into 90705

Hermetic Room 90704 south into 90709 - Shaman Room 90708 south into 90709
*/

struct archetype_data *archetypes[NUM_CCR_ARCHETYPES];

struct archetype_data *generate_street_samurai() {
  struct archetype_data *arch = new archetype_data;
  int i = 0;

  memset(arch, 0, sizeof(struct archetype_data));

  arch->name = str_dup("Street Samurai");
  arch->race = RACE_HUMAN;
  arch->difficulty_rating = str_dup("easiest to play");

  arch->display_name = str_dup("Street Samurai (Human)");
  arch->description = str_dup("Focusing on cyberware-augmented gunplay with a side order of melee combat,"
                              " the Street Samurai is a fast-paced character that requires minimal knowledge"
                              " of the game to succeed at. This archetype is built with a Human as its base race.");

  // correct for street sam
  arch->start_room = 90599;
  arch->auth_room = 90529;
  arch->warning_room = 90526;

  // Set attributes.
  arch->attributes[BOD] = 6;
  arch->attributes[QUI] = 7;
  arch->attributes[STR] = 6;
  arch->attributes[CHA] = 3;
  arch->attributes[INT] = 5;
  arch->attributes[WIL] = 6;
  // Reaction, essence, etc is autocomputed on character creation.

  // Set magic stats. Anything not set is zero by default.
  arch->tradition = TRAD_MUNDANE;

  // Set skills.
  arch->skills[SKILL_ASSAULT_RIFLES] = 6;
  arch->skills[SKILL_CLUBS] = 6;
  arch->skills[SKILL_BIOTECH] = 5;
  arch->skills[SKILL_ELECTRONICS] = 3;
  arch->skills[SKILL_NEGOTIATION] = 3;
  arch->skills[SKILL_STREET_ETIQUETTE] = 3;
  arch->skills[SKILL_ATHLETICS] = 3;
  arch->skills[SKILL_PILOT_BIKE] = 1;

  // Inventory.
  arch->weapon = OBJ_COLT_M23;
  arch->weapon_top = OBJ_SMARTLINK_II_TOP;
  arch->weapon_barrel = OBJ_GAS_VENT_IV;
  arch->weapon_under = OBJ_FOREGRIP;

  arch->ammo_q = 500;

  arch->modulator = OBJ_DOCWAGON_PLATINUM_MODULATOR;

  // Worn equipment.
  arch->worn[WEAR_EYES] = OBJ_THERMOGRAPHIC_GOGGLES;
  arch->worn[WEAR_ABOUT] = OBJ_SECURE_JACKET;
  arch->worn[WEAR_BODY] = OBJ_PLATED_ARMOR_VEST;
  arch->worn[WEAR_UNDER] = OBJ_FORMFIT_III;
  arch->worn[WEAR_ARMS] = OBJ_FOREARM_GUARDS;
  arch->worn[WEAR_BACK] = OBJ_RIFLE_STRAP;
  arch->worn[WEAR_WAIST] = OBJ_BLACK_LEATHER_DUTY_BELT;
  arch->worn[WEAR_THIGH_L] = OBJ_SWORD_SHEATH;
  arch->worn[WEAR_LEGS] = OBJ_REAL_LEATHER_PANTS;
  arch->worn[WEAR_FEET] = OBJ_BLACK_COMBAT_BOOTS;

  i = 0;
  arch->carried[i++] = OBJ_NIGHTSTICK;
  arch->carried[i++] = OBJ_MEDKIT;
  arch->carried[i++] = OBJ_TITLE_TO_RAPIER;
  assert(i < NUM_ARCHETYPE_CARRIED);

  // Cyberware.
  i = 0;
  arch->cyberware[i++] = OBJ_CYB_ARMORED_OBV_ARMS_II;
  arch->cyberware[i++] = OBJ_CYB_SMARTLINK_II;
  assert(i < NUM_ARCHETYPE_CYBERWARE);

  // Bioware.
  i = 0;
  arch->bioware[i++] = OBJ_BIO_ENHANCED_ARTICULATION;
  arch->bioware[i++] = OBJ_BIO_METABOLIC_ARRESTER;
  arch->bioware[i++] = OBJ_BIO_PAIN_EDITOR;
  arch->bioware[i++] = OBJ_BIO_SYMBIOTES_II;
  assert(i < NUM_ARCHETYPE_BIOWARE);

  return arch;
}

#define ARCH_ADEPT_POWER(power, rating) arch->powers[i][0] = (power); arch->powers[i++][1] = (rating);
struct archetype_data *generate_adept() {
  struct archetype_data *arch = new archetype_data;
  int i = 0;

  memset(arch, 0, sizeof(struct archetype_data));

  arch->name = str_dup("Adept");
  arch->race = RACE_ORK;
  arch->difficulty_rating = str_dup("easy");

  arch->display_name = str_dup("Adept (Ork)");
  arch->description = str_dup("With a blend of ranged and melee combat, the Adept builds up personal"
                              " power by focusing their magic towards the improvement of their body."
                              " While still easy to get into, they have a slower start than Street"
                              " Samurai, but make up for it with a higher power ceiling in the endgame.");

  arch->start_room = 90699;
  arch->auth_room = 90629;
  arch->warning_room = 90626;

  // Set attributes.
  arch->attributes[BOD] = 9;
  arch->attributes[QUI] = 6;
  arch->attributes[STR] = 8;
  arch->attributes[CHA] = 1;
  arch->attributes[INT] = 3;
  arch->attributes[WIL] = 6;
  // Reaction, essence, etc is autocomputed on character creation.

  // Set magic stats.
  arch->magic = 600;
  arch->tradition = TRAD_ADEPT;

  // Set adept powers.
  i = 0;
  ARCH_ADEPT_POWER(ADEPT_COUNTERSTRIKE, 2);
  ARCH_ADEPT_POWER(ADEPT_IMPROVED_QUI, 1);
  ARCH_ADEPT_POWER(ADEPT_KINESICS, 1);
  ARCH_ADEPT_POWER(ADEPT_SIDESTEP, 6);
  assert(i < NUM_ARCHETYPE_ABILITIES);

  // Set skills.
  arch->skills[SKILL_POLE_ARMS] = 6;
  arch->skills[SKILL_RIFLES] = 6;
  arch->skills[SKILL_ELECTRONICS] = 3;
  arch->skills[SKILL_NEGOTIATION] = 3;
  arch->skills[SKILL_CORPORATE_ETIQUETTE] = 3;
  arch->skills[SKILL_STREET_ETIQUETTE] = 3;
  arch->skills[SKILL_ATHLETICS] = 3;
  arch->skills[SKILL_STEALTH] = 6;
  arch->skills[SKILL_PILOT_BIKE] = 1;

  // Inventory.
  arch->weapon = OBJ_REMINGTON_950; // vnum ?, remington 950 sport rifle, needs reference
  arch->weapon_top = OBJ_TOP_LASER_SIGHT;
  arch->weapon_barrel = OBJ_SILENCER; // vnum ?, silencer, needs reference
  arch->weapon_under = OBJ_BAYONET;

  arch->ammo_q = 500;

  arch->modulator = OBJ_DOCWAGON_BASIC_MODULATOR;

  // Worn equipment.
  arch->worn[WEAR_EYES] = OBJ_THERMOGRAPHIC_GOGGLES;
  arch->worn[WEAR_BACK] =  OBJ_POLEARM_STRAP;
  arch->worn[WEAR_ABOUT] = OBJ_SECURE_JACKET;
  arch->worn[WEAR_BODY] = OBJ_ARMOR_VEST; // armor vest (2B/1I), needs reference;
  arch->worn[WEAR_UNDER] = OBJ_FORMFIT_III;
  arch->worn[WEAR_ARMS] = OBJ_FOREARM_GUARDS;
  arch->worn[WEAR_LARM] = OBJ_RIFLE_STRAP;
  arch->worn[WEAR_WAIST] = OBJ_BLACK_LEATHER_DUTY_BELT;
  arch->worn[WEAR_LEGS] = OBJ_REAL_LEATHER_PANTS; // vnum 60572, real leather pants, needs reference
  arch->worn[WEAR_FEET] = OBJ_BLACK_COMBAT_BOOTS;

  i = 0;
  arch->carried[i++] = OBJ_BO_STAFF; // vnum ?, needs reference
  arch->carried[i++] = OBJ_TITLE_TO_SCORPION; // scorpion tit
  assert(i < NUM_ARCHETYPE_CARRIED);

  return arch;
}
#undef ARCH_ADEPT_POWER

// TODO
#define ARCH_SPELL(spell, subtype, force) arch->spells[i][0] = (spell); arch->spells[i][1] = (subtype); arch->spells[i++][2] = (force);
#define ARCH_FOCUS(vnum, type) arch->foci[i][0] = (vnum); arch->foci[i++][1] = (type);
struct archetype_data *generate_shaman() {
  struct archetype_data *arch = new archetype_data;
  int i = 0;

  memset(arch, 0, sizeof(struct archetype_data));

  arch->name = str_dup("Shaman");
  arch->race = RACE_ELF;
  arch->difficulty_rating = str_dup("moderate");

  arch->display_name = str_dup("Shaman (Elf)");
  arch->description = str_dup("As a spellcaster with no initial skill in weapons, the Shaman focuses"
                              " on taking enemies down quickly and avoiding drawn-out fights. They"
                              " have the ability to conjure spirits of nature to boost their power,"
                              " and can learn to cast higher-powered spells than any other caster type.");

  // checked for shamanic mage
  arch->start_room = 90799;
  arch->auth_room = 90738;
  arch->warning_room = 90735;

  // Set attributes.
  arch->attributes[BOD] = 5;
  arch->attributes[QUI] = 6;
  arch->attributes[STR] = 3;
  arch->attributes[CHA] = 5;
  arch->attributes[INT] = 5;
  arch->attributes[WIL] = 7;
  // Reaction, essence, etc is autocomputed on character creation.
  
  // Set magic stats.
  arch->magic = 600;
  arch->tradition = TRAD_SHAMANIC;
  arch->totem = TOTEM_WOLF;
  arch->totemspirit = SPIRIT_FOREST;
  
  // Spells.
  i = 0;
  ARCH_SPELL(SPELL_COMBATSENSE, 0, 3);
  ARCH_SPELL(SPELL_ARMOR, 0, 3);
  ARCH_SPELL(SPELL_LEVITATE, 0, 1);
  ARCH_SPELL(SPELL_STEALTH, 0, 1);
  ARCH_SPELL(SPELL_IMP_INVIS, 0, 6);
  ARCH_SPELL(SPELL_HEAL, 0, 5);
  ARCH_SPELL(SPELL_STUNBOLT, 0, 6);
  assert(i < NUM_ARCHETYPE_SPELLS);
  
  // Set skills.
  arch->skills[SKILL_SORCERY] = 6;
  arch->skills[SKILL_CONJURING] = 6;
  arch->skills[SKILL_SHOTGUNS] = 5;
  arch->skills[SKILL_POLE_ARMS] = 3;
  arch->skills[SKILL_NEGOTIATION] = 3;
  arch->skills[SKILL_STREET_ETIQUETTE] = 3;
  arch->skills[SKILL_PILOT_TRUCK] = 1;
  
  // Inventory.
  arch->weapon = OBJ_REMINGTON_990; // remington 990 shotgun, needs reference
  arch->weapon_top = OBJ_TOP_LASER_SIGHT;
  arch->weapon_under = OBJ_BAYONET;
  
  arch->ammo_q = 500;
  
  arch->modulator = OBJ_DOCWAGON_BASIC_MODULATOR;
  
  // Worn equipment.
  arch->worn[WEAR_ABOUT] = OBJ_BLACK_TRENCH_COAT;
  arch->worn[WEAR_BODY] = OBJ_ARMOR_VEST; // armor vest (2B/1I), needs reference
  arch->worn[WEAR_UNDER] = OBJ_FORMFIT_III;
  arch->worn[WEAR_ARMS] = OBJ_FOREARM_GUARDS;
  arch->worn[WEAR_LARM] = OBJ_RIFLE_STRAP;
  arch->worn[WEAR_WAIST] = OBJ_BLACK_LEATHER_DUTY_BELT;
  arch->worn[WEAR_LEGS] = OBJ_BLACK_BDU_PANTS;
  arch->worn[WEAR_FEET] = OBJ_BLACK_COMBAT_BOOTS;
  
  i = 0;
  assert(i < NUM_ARCHETYPE_CARRIED);
  
  i = 0;
  ARCH_FOCUS(OBJ_ASH_LEAF_ANKLET, SPELL_STEALTH); // r1 sustaining focus
  ARCH_FOCUS(OBJ_ORICHALCUM_BRACELET, SPELL_ARMOR); // r3 sustaining focus
  ARCH_FOCUS(OBJ_ORICHALCUM_BRACELET, SPELL_COMBATSENSE); // r3 sustaining focus
  assert(i < NUM_ARCHETYPE_FOCI);

  return arch;
}

struct archetype_data *generate_street_mage() {
  struct archetype_data *arch = new archetype_data;
  int i = 0;

  memset(arch, 0, sizeof(struct archetype_data));

  arch->name = str_dup("Street Mage");
  arch->race = RACE_HUMAN;
  arch->difficulty_rating = str_dup("moderate");

  arch->display_name = str_dup("Street Mage (Human)");
  arch->description = str_dup("As a spellcaster with no initial skill in weapons, the Street Mage focuses"
                              " on taking enemies down quickly and avoiding drawn-out fights. They are Hermetic"
                              " mages, with a focus on book learning and pre-fight preparation, and can summon"
                              " powerful elementals to sustain their spells and grant them bonuses.");

  // kosher for hermetic mage
  arch->start_room = 90799;
  arch->auth_room = 90738;
  arch->warning_room = 90735;
  
  // Set attributes.
  arch->attributes[BOD] = 4;
  arch->attributes[QUI] = 6;
  arch->attributes[STR] = 2;
  arch->attributes[CHA] = 2;
  arch->attributes[INT] = 4;
  arch->attributes[WIL] = 6;
  // Reaction, essence, etc is autocomputed on character creation.
  
  // Set magic stats.
  arch->magic = 600;
  arch->tradition = TRAD_HERMETIC;
  
  // Spells.
  i = 0;
  ARCH_SPELL(SPELL_INCATTR, BOD, 3);
  ARCH_SPELL(SPELL_ARMOR, 0, 3);
  ARCH_SPELL(SPELL_LEVITATE, 0, 1);
  ARCH_SPELL(SPELL_STEALTH, 0, 1);
  ARCH_SPELL(SPELL_IMP_INVIS, 0, 6);
  ARCH_SPELL(SPELL_STUNBOLT, 0, 6);
  ARCH_SPELL(SPELL_WATERBOLT, 0, 5);
  assert(i < NUM_ARCHETYPE_SPELLS);
  
  // Set skills.
  arch->skills[SKILL_SORCERY] = 6;
  arch->skills[SKILL_CONJURING] = 6;
  arch->skills[SKILL_TASERS] = 5;
  arch->skills[SKILL_CLUBS] = 3;
  arch->skills[SKILL_ELECTRONICS] = 3;
  arch->skills[SKILL_NEGOTIATION] = 3;
  arch->skills[SKILL_CORPORATE_ETIQUETTE] = 3;
  arch->skills[SKILL_PILOT_CAR] = 1;
  
  // Inventory.
  arch->weapon = OBJ_DEFIANCE_SUPER_SHOCK; // vnum 80211, Defiance Super Shock, needs reference
  arch->weapon_top = OBJ_SMARTLINK_II_TOP;
 
  arch->ammo_q = 500;
 
  arch->modulator = OBJ_DOCWAGON_GOLD_MODULATOR;
 
  // Worn equipment.
  arch->worn[WEAR_ABOUT] = OBJ_LONDON_FOG_MERLIN;
  arch->worn[WEAR_BODY] = OBJ_PLATED_ARMOR_VEST;
  arch->worn[WEAR_UNDER] = OBJ_FORMFIT_III;
  arch->worn[WEAR_ARMS] = OBJ_FOREARM_GUARDS;
  arch->worn[WEAR_WAIST] = OBJ_BLACK_LEATHER_DUTY_BELT;
  arch->worn[WEAR_THIGH_L] = OBJ_THIGH_HOLSTER;
  arch->worn[WEAR_LEGS] = OBJ_BLACK_SLACKS;
  arch->worn[WEAR_FEET] = OBJ_BLACK_DRESS_SHOES;
 
  i = 0;
  arch->carried[i++] = OBJ_TITLE_TO_AMERICAR;
  assert(i < NUM_ARCHETYPE_CARRIED);
 
  i = 0;
  ARCH_FOCUS(OBJ_ORICHALCUM_NECKLACE, 0); // r1 power focus, need reference
  ARCH_FOCUS(OBJ_ASH_LEAF_ANKLET, SPELL_STEALTH); // r1 sustaining focus
  ARCH_FOCUS(OBJ_ORICHALCUM_BRACELET, SPELL_ARMOR); // r3 sustaining focus
  ARCH_FOCUS(OBJ_ORICHALCUM_BRACELET, SPELL_INCATTR); // r3 sustaining focus
  assert(i < NUM_ARCHETYPE_FOCI);
  
  // Cyberware.
  i = 0;
  arch->cyberware[i++] = OBJ_CYB_SMARTLINK_II_ALPHA;
  assert(i < NUM_ARCHETYPE_CYBERWARE);
  
  // Bioware.
  i = 0;
  arch->bioware[i++] = OBJ_BIO_CEREBRAL_BOOSTER_II;
  arch->bioware[i++] = OBJ_BIO_TRAUMA_DAMPER;
  assert(i < NUM_ARCHETYPE_BIOWARE);

  return arch;
}
#undef ARCH_SPELL
#undef ARCH_FOCUS

struct archetype_data *generate_decker() {
  struct archetype_data *arch = new archetype_data;
  int i = 0;

  memset(arch, 0, sizeof(struct archetype_data));

  arch->name = str_dup("Decker");
  arch->race = RACE_HUMAN;
  arch->difficulty_rating = str_dup("difficult but rewarding");

  arch->display_name = str_dup("Decker (Human)");
  arch->description = str_dup("The Decker is a complex role to play, as it requires navigating the Matrix:"
                              " the interconnected web of virtual-reality hosts that backend modern commerce."
                              " Skilled Deckers can extract paydata from the Matrix to sell for nuyen."
                              " When the data's not selling, this archetype can fall back to firearms to get things done.");

    // correct for decker
  arch->start_room = 91099;
  arch->auth_room = 91032;
  arch->warning_room = 91029;

  // Set attributes.
  arch->attributes[BOD] = 4;
  arch->attributes[QUI] = 6;
  arch->attributes[STR] = 3;
  arch->attributes[CHA] = 1;
  arch->attributes[INT] = 6;
  arch->attributes[WIL] = 3;
  // Reaction, essence, etc is autocomputed on character creation.

  // Set magic stats.
  arch->tradition = TRAD_MUNDANE;

  // Set skills.
  arch->skills[SKILL_SMG] = 6;
  arch->skills[SKILL_BR_COMPUTER] = 6;
  arch->skills[SKILL_CLUBS] = 3;
  arch->skills[SKILL_COMPUTER] = 6;
  arch->skills[SKILL_ELECTRONICS] = 3;
  arch->skills[SKILL_NEGOTIATION] = 3;
  arch->skills[SKILL_CORPORATE_ETIQUETTE] = 3;
  arch->skills[SKILL_DATA_BROKERAGE] = 6;
  arch->skills[SKILL_ATHLETICS] = 3;
  arch->skills[SKILL_BIOTECH] = 4;
  arch->skills[SKILL_PILOT_TRUCK] = 1;

  // Inventory.
  arch->weapon = OBJ_INGRAM_SMARTGUN;
  arch->weapon_top = OBJ_SMARTLINK_II_TOP;
  arch->weapon_under = OBJ_FOREGRIP;
  
  arch->ammo_q = 500;

  arch->modulator = OBJ_DOCWAGON_GOLD_MODULATOR;

  // Worn equipment.
  arch->worn[WEAR_EYES] = OBJ_THERMOGRAPHIC_GOGGLES;
  arch->worn[WEAR_ABOUT] = OBJ_SECURE_JACKET;
  arch->worn[WEAR_BODY] = OBJ_PLATED_ARMOR_VEST;
  arch->worn[WEAR_UNDER] = OBJ_FORMFIT_III;
  arch->worn[WEAR_ARMS] = OBJ_FOREARM_GUARDS;
  arch->worn[WEAR_WAIST] = OBJ_BLACK_LEATHER_DUTY_BELT;
  arch->worn[WEAR_THIGH_L] = OBJ_THIGH_HOLSTER;
  arch->worn[WEAR_LEGS] = OBJ_BAGGY_BLUE_JEANS;
  arch->worn[WEAR_FEET] = OBJ_PAIR_OF_WHITE_TRAINERS;

  i = 0;
  arch->carried[i++] = OBJ_CYBERDECK_REPAIR_KIT;
  arch->carried[i++] = OBJ_MEDKIT;
  arch->carried[i++] = OBJ_MITSUHAMA_Z4;
  arch->carried[i++] = OBJ_NOVATECH_BURNER;
  arch->carried[i++] = OBJ_TITLE_TO_BISON;
  assert(i < NUM_ARCHETYPE_CARRIED);

  arch->cyberdeck = OBJ_CMT_AVATAR;

  i = 0;
  arch->software[i++] = OBJ_NOVATECH_SIX_SENSORS;
  arch->software[i++] = OBJ_NOVATECH_SIX_MASKING;
  arch->software[i++] = OBJ_NOVATECH_SIX_BOD;
  arch->software[i++] = OBJ_FUCHI_LTD_EVASION;
  arch->software[i++] = OBJ_NOVATECH_R5_SLEAZE;
  arch->software[i++] = OBJ_TRANSYS_SCRIBE; // r6 read/write
  arch->software[i++] = OBJ_TRANSYS_RIFFLE; // r6 browse
  arch->software[i++] = OBJ_RENRAKU_BYPASS; // r6 deception
  arch->software[i++] = OBJ_FOXFIRE_KITSUNE; // r6 analyze
  arch->software[i++] = OBJ_TRANSYS_ARMOR; // r6 armor
  arch->software[i++] = OBJ_MATRIX_SWORD; //  r4 attack
  assert(i < NUM_ARCHETYPE_SOFTWARE);

  i = 0;
  arch->cyberware[i++] = OBJ_CYB_DATAJACK;
  arch->cyberware[i++] = OBJ_CYB_ENCEPHALON_II;
  arch->cyberware[i++] = OBJ_CYB_MATH_SPU_III;
  arch->cyberware[i++] = OBJ_CYB_SMARTLINK_II;
  arch->cyberware[i++] = OBJ_CYB_DERMAL_SHEATHING_I;
  arch->cyberware[i++] = OBJ_CYB_KEVLAR_BONE_LACING;
  assert(i < NUM_ARCHETYPE_CYBERWARE);

  i = 0;
  arch->bioware[i++] = OBJ_BIO_CATS_EYES;
  arch->bioware[i++] = OBJ_BIO_CEREBRAL_BOOSTER_II;
  arch->bioware[i++] = OBJ_BIO_MUSCLE_TONER_III;
  arch->bioware[i++] = OBJ_BIO_TRAUMA_DAMPER;
  assert(i < NUM_ARCHETYPE_BIOWARE);

  return arch;
}

// TODO
struct archetype_data *generate_rigger() {
  struct archetype_data *arch = new archetype_data;
  int i = 0;

  memset(arch, 0, sizeof(struct archetype_data));

  arch->name = str_dup("Rigger");
  arch->race = RACE_HUMAN;
  arch->difficulty_rating = str_dup("tedious");

  arch->display_name = str_dup("Rigger (Human)");
  arch->description = str_dup("todo");

  // correct for rigger
  arch->start_room = 90999;
  arch->auth_room = 90933;
  arch->warning_room = 90930;

  // Set attributes.
  arch->attributes[BOD] = 6;
  arch->attributes[QUI] = 6;
  arch->attributes[STR] = 6;
  arch->attributes[CHA] = 1;
  arch->attributes[INT] = 6;
  arch->attributes[WIL] = 6;
  // Reaction, essence, etc is autocomputed on character creation.

  // Set magic stats.
  arch->tradition = TRAD_MUNDANE;

  // Set skills.
  arch->skills[SKILL_ASSAULT_RIFLES] = 6;

  // Inventory.
  arch->weapon = 838; // colt m-23, should be set to burst fire
  arch->weapon_top = OBJ_NICAMI_SCOPE; // nicami scope
  arch->weapon_barrel = OBJ_GAS_VENT_IV; // vent IV
  arch->weapon_under = OBJ_SMARTLINK_II; // smartlink II
  arch->ammo_q = 500;

  arch->nuyen = 22660;

  arch->modulator = 16208; // platinum

  arch->worn[WEAR_ABOUT] = 1833; // a black trench coat

  i = 0;
  arch->carried[i++] = 450; // medkit
  assert(i < NUM_ARCHETYPE_CARRIED);

  // Cyberware.
  i = 0;
  arch->cyberware[i++] = OBJ_CYB_CERAMIC_BONE_LACING; // ceramic bone lacing
  assert(i < NUM_ARCHETYPE_CYBERWARE);

  // Bioware.
  i = 0;
  arch->bioware[i++] = OBJ_BIO_ENHANCED_ARTICULATION; // enhanced articulation
  assert(i < NUM_ARCHETYPE_BIOWARE);

  return arch;
}

struct archetype_data *generate_archetype(int index) {
  switch(index) {
    case ARCHETYPE_STREET_SAMURAI:
      return generate_street_samurai();
    case ARCHETYPE_ADEPT:
      return generate_adept();
    case ARCHETYPE_SHAMANIC_MAGE:
      return generate_shaman();
    case ARCHETYPE_STREET_MAGE:
      return generate_street_mage();
    case ARCHETYPE_DECKER:
      return generate_decker();
    case ARCHETYPE_RIGGER:
      return generate_rigger();
    default:
      log_vfprintf("ERROR: Invalid archetype #%d passed to generate_archetypes!", index);
      shutdown();
      return NULL;
  }
}

void generate_archetypes() {
  for (int i = 0; i < NUM_CCR_ARCHETYPES; i++) {
    archetypes[i] = generate_archetype(i);

    // Give all archetypes a subsidy card with a month's rent on it.
    archetypes[i]->subsidy_card = 30000;

    // Give them all Brawling so they don't get facerolled by training dummies.
    archetypes[i]->skills[SKILL_UNARMED_COMBAT] = MAX(2, archetypes[i]->skills[SKILL_UNARMED_COMBAT]);

    log_vfprintf("... %s.", archetypes[i]->name);
  }
}
