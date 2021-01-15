#include "types.h"
#include "structs.h"
#include "awake.h"
#include "comm.h" // for shutdown()
#include "db.h"
#include "archetypes.h"

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
  
  // correct for street sam
  arch->start_room = 90500;
  arch->auth_room = 90529;
  
  // Set attributes.
  arch->attributes[BOD] = 6;
  arch->attributes[QUI] = 6;
  arch->attributes[STR] = 6;
  arch->attributes[CHA] = 1;
  arch->attributes[INT] = 6;
  arch->attributes[WIL] = 6;
  // Reaction, essence, etc is autocomputed on character creation.
  
  // Set magic stats. Anything not set is zero by default.
  arch->tradition = TRAD_MUNDANE;
  
  // Set skills.
  arch->skills[SKILL_ASSAULT_RIFLES] = 6;
  arch->skills[SKILL_CLUBS] = 6;
  arch->skills[SKILL_NEGOTIATION] = 3;
  arch->skills[SKILL_ATHLETICS] = 3;
  arch->skills[SKILL_ELECTRONICS] = 3;
  arch->skills[SKILL_PILOT_BIKE] = 3;
  arch->skills[SKILL_BR_BIKE] = 3;
  arch->skills[SKILL_STREET_ETIQUETTE] = 1;
  
  // Inventory.
  arch->weapon = OBJ_COLT_M23; // colt m-23, TODO: should be set to burst fire
  arch->weapon_top = OBJ_NICAMI_SCOPE; // nicami scope
  arch->weapon_barrel = OBJ_VENT_IV; // vent IV
  arch->weapon_under = OBJ_SMARTLINK_II; // smartlink II
  
  arch->ammo_q = 500;
  
  arch->nuyen = 20460;
  
  arch->modulator = OBJ_DOCWAGON_PLATINUM_MODULATOR; // platinum
  
  arch->worn[WEAR_EYES] = OBJ_THERMOGRAPHIC_GOGGLES; // thermographic goggles
  arch->worn[WEAR_ABOUT] = OBJ_BLACK_TRENCH_COAT; // a black trench coat
  arch->worn[WEAR_BODY] = OBJ_100_PCT_COTTON_SHIRT; // a 100% cotton t-shirt
  arch->worn[WEAR_UNDER] = OBJ_FORMFIT_III; // formfit III
  arch->worn[WEAR_ARMS] = OBJ_FOREARM_GUARDS; // forearm guards
  arch->worn[WEAR_BACK] = OBJ_RIFLE_STRAP; // rifle strap
  arch->worn[WEAR_WAIST] = OBJ_BLACK_LEATHER_DUTY_BELT; // a black leather duty belt
  arch->worn[WEAR_LEGS] = OBJ_BLACK_BDU_PANTS; // a pair of black BDU pants
  arch->worn[WEAR_FEET] = OBJ_BLACK_COMBAT_BOOTS; // a pair of black combat boots
  
  i = 0;
  arch->carried[i++] = OBJ_MEDKIT; // medkit
  arch->carried[i++] = OBJ_VEHICLE_TOOLKIT; // vehicle toolkit
  arch->carried[i++] = OBJ_ELECTRONICS_KIT; // electronics kit
  arch->carried[i++] = OBJ_CELL_PHONE; // cell phone
  arch->carried[i++] = OBJ_TITLE_TO_SCORPION; // scorpion tit
  arch->carried[i++] = OBJ_TITLE_TO_BISON; // bison tit
  arch->carried[i++] = OBJ_POCKET_SECRETARY; // secretary
  arch->carried[i++] = OBJ_NEOPHYTE_DUFFELBAG; // duffelbag
  assert(i < NUM_ARCHETYPE_CARRIED);
  
  // Cyberware.
  i = 0;
  arch->cyberware[i++] = OBJ_CYB_CERAMIC_BONE_LACING; // ceramic bone lacing
  arch->cyberware[i++] = OBJ_CYB_DATAJACK; // datajack
  arch->cyberware[i++] = OBJ_CYB_BOOSTED_REFLEXES_III_ALPHA; // boosted reflexes III -alpha
  arch->cyberware[i++] = OBJ_CYB_THERMOGRAPHIC_VISION; // thermographic vision
  arch->cyberware[i++] = OBJ_CYB_SMARTLINK_II_ALPHA; // smartlink 2 --alpha
  assert(i < NUM_ARCHETYPE_CYBERWARE);
  
  // Bioware.
  i = 0;
  arch->bioware[i++] = OBJ_BIO_ENHANCED_ARTICULATION; // enhanced articulation
  arch->bioware[i++] = OBJ_BIO_MUSCLE_TONER_IV; // muscle toner iv
  arch->bioware[i++] = OBJ_BIO_TRAUMA_DAMPER; // trauma damper
  arch->bioware[i++] = OBJ_BIO_SYNAPTIC_ACCELERATOR_II; // synamptic accelerator II
  arch->bioware[i++] = OBJ_BIO_CEREBRAL_BOOSTER_II; // cerebral booster II
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
  
  arch->start_room = 90600;
  arch->auth_room = 90629;
  
  // Set attributes.
  arch->attributes[BOD] = 6;
  arch->attributes[QUI] = 6;
  arch->attributes[STR] = 8;
  arch->attributes[CHA] = 1;
  arch->attributes[INT] = 5;
  arch->attributes[WIL] = 6;
  // Reaction, essence, etc is autocomputed on character creation.
  
  // Set magic stats.
  arch->magic = 600;
  arch->tradition = TRAD_ADEPT;
  
  // Set adept powers.
  i = 0;
  ARCH_ADEPT_POWER(ADEPT_COUNTERSTRIKE, 2);
  ARCH_ADEPT_POWER(ADEPT_IMPROVED_QUI, 2);
  ARCH_ADEPT_POWER(ADEPT_IMPROVED_BOD, 2);
  ARCH_ADEPT_POWER(ADEPT_MYSTIC_ARMOR, 2);
  ARCH_ADEPT_POWER(ADEPT_SIDESTEP, 2);
  ARCH_ADEPT_POWER(ADEPT_MAGIC_RESISTANCE, 1);
  assert(i < NUM_ARCHETYPE_ABILITIES);
  
  // Set skills.
  arch->skills[SKILL_ASSAULT_RIFLES] = 6;
  arch->skills[SKILL_POLE_ARMS] = 6;
  arch->skills[SKILL_ATHLETICS] = 3;
  arch->skills[SKILL_NEGOTIATION] = 3;
  arch->skills[SKILL_ELECTRONICS] = 1;
  arch->skills[SKILL_PILOT_BIKE] = 1;
  arch->skills[SKILL_BR_BIKE] = 1;
  arch->skills[SKILL_STREET_ETIQUETTE] = 1;
  
  // Inventory.
  arch->weapon = OBJ_COLT_M23; // colt m-23, should be set to burst fire
  arch->weapon_top = OBJ_TOP_LASER_SIGHT; // laser sight
  arch->weapon_barrel = OBJ_VENT_IV; // vent IV
  arch->weapon_under = OBJ_BAYONET; // bayonet
  
  arch->ammo_q = 500;
  
  arch->nuyen = 36550;
  
  arch->modulator = OBJ_DOCWAGON_GOLD_MODULATOR; // gold
  
  // actually kosher
  arch->worn[WEAR_EYES] = OBJ_THERMOGRAPHIC_GOGGLES; // thermographic goggles
  arch->worn[WEAR_BACK] =  OBJ_POLEARM_STRAP; // a polearm strap
  arch->worn[WEAR_ABOUT] = OBJ_SECURE_JACKET; // a secure jacket
  arch->worn[WEAR_BODY] = OBJ_100_PCT_COTTON_SHIRT; // a 100% cotton t-shirt
  arch->worn[WEAR_UNDER] = OBJ_FORMFIT_III; // formfit III
  arch->worn[WEAR_ARMS] = OBJ_FOREARM_GUARDS; // forearm guards
  arch->worn[WEAR_LARM] = OBJ_RIFLE_STRAP; // rifle strap
  arch->worn[WEAR_WAIST] = OBJ_BLACK_LEATHER_DUTY_BELT; // a black leather duty belt
  arch->worn[WEAR_LEGS] = OBJ_BLACK_BDU_PANTS; // a pair of black BDU pants
  arch->worn[WEAR_FEET] = OBJ_BLACK_COMBAT_BOOTS; // a pair of black combat boots
  
  i = 0;
  arch->carried[i++] = OBJ_MEDKIT; // medkit
  arch->carried[i++] = OBJ_VEHICLE_TOOLKIT; // vehicle toolkit
  arch->carried[i++] = OBJ_ELECTRONICS_KIT; // electronics kit
  arch->carried[i++] = OBJ_CELL_PHONE; // cell phone
  arch->carried[i++] = OBJ_TITLE_TO_SCORPION; // scorpion tit
  arch->carried[i++] = OBJ_POCKET_SECRETARY; // secretary
  arch->carried[i++] = OBJ_NEOPHYTE_DUFFELBAG; // duffelbag
  arch->carried[i++] = OBJ_STEEL_COMBAT_AXE;
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
  
  arch->start_room = 90700;
  arch->auth_room = 90738;
  
  // Set attributes.
  arch->attributes[BOD] = 4;
  arch->attributes[QUI] = 7;
  arch->attributes[STR] = 2;
  arch->attributes[CHA] = 3;
  arch->attributes[INT] = 6;
  arch->attributes[WIL] = 6;
  // Reaction, essence, etc is autocomputed on character creation.
  
  // Set magic stats.
  arch->magic = 600;
  arch->tradition = TRAD_SHAMANIC;
  arch->forcepoints = 25;
  arch->totem = TOTEM_WOLF;
  arch->totemspirit = SPIRIT_FOREST;
  
  // Spells.
  i = 0;
  ARCH_SPELL(SPELL_TREAT, 0, 3);
  ARCH_SPELL(SPELL_CLOUT, 0, 6);
  ARCH_SPELL(SPELL_ARMOR, 0, 3);
  ARCH_SPELL(SPELL_IMP_INVIS, 0, 1);
  ARCH_SPELL(SPELL_HEAL, 0, 6);
  ARCH_SPELL(SPELL_STUNBOLT, 0, 6);
  assert(i < NUM_ARCHETYPE_SPELLS);
  
  // Set skills.
  arch->skills[SKILL_SORCERY] = 6;
  arch->skills[SKILL_CONJURING] = 5;
  arch->skills[SKILL_NEGOTIATION] = 2;
  arch->skills[SKILL_STREET_ETIQUETTE] = 2;
  arch->skills[SKILL_AURA_READING] = 2;
  arch->skills[SKILL_STEALTH] = 3;
  
  // Inventory.  
  // arch->nuyen = 22660;
  
  arch->modulator = OBJ_DOCWAGON_BASIC_MOD;
  
  arch->worn[WEAR_ABOUT] = OBJ_LONDON_FOG_COUNT;
  arch->worn[WEAR_BACK] = OBJ_NEOPHYTE_DUFFELBAG;
  arch->worn[WEAR_BODY] = OBJ_PLATED_ARMOR_VEST;
  arch->worn[WEAR_UNDER] = OBJ_FORMFIT_III;
  arch->worn[WEAR_ARMS] = OBJ_FOREARM_GUARDS;
  arch->worn[WEAR_WAIST] = OBJ_BLACK_LEATHER_DUTY_BELT;
  arch->worn[WEAR_LEGS] = OBJ_BLACK_BDU_PANTS;
  arch->worn[WEAR_FEET] = OBJ_BLACK_COMBAT_BOOTS;
  
  i = 0;
  arch->carried[i++] = OBJ_ELECTRONICS_KIT;
  arch->carried[i++] = OBJ_POCKET_SECRETARY;
  arch->carried[i++] = OBJ_CELL_PHONE;
  assert(i < NUM_ARCHETYPE_CARRIED);
  
  i = 0;
  ARCH_FOCUS(OBJ_ASH_LEAF_ANKLET, SPELL_IMP_INVIS);
  ARCH_FOCUS(OBJ_ORICHALCUM_BRACELET, SPELL_ARMOR);
  assert(i < NUM_ARCHETYPE_FOCI);
  
  return arch;
}

// TODO
struct archetype_data *generate_street_mage() {
  struct archetype_data *arch = new archetype_data;
  int i = 0;
  
  memset(arch, 0, sizeof(struct archetype_data));
  
  arch->name = str_dup("Street Mage");
  arch->race = RACE_HUMAN;
  
  // kosher for hermetic mage
  arch->start_room = 90700;
  arch->auth_room = 90738;
  
  // Set attributes.
  arch->attributes[BOD] = 3;
  arch->attributes[QUI] = 6;
  arch->attributes[STR] = 2;
  arch->attributes[CHA] = 2;
  arch->attributes[INT] = 6;
  arch->attributes[WIL] = 6;
  // Reaction, essence, etc is autocomputed on character creation.
  
  // Set magic stats.
  arch->magic = 600;
  arch->tradition = TRAD_HERMETIC;
  arch->forcepoints = 25;
  
  // Set skills.
  arch->skills[SKILL_SORCERY] = 6;
  arch->skills[SKILL_CONJURING] = 4;
  arch->skills[SKILL_AURA_READING] = 3;
  arch->skills[SKILL_STREET_ETIQUETTE] = 1;
  arch->skills[SKILL_CORPORATE_ETIQUETTE] = 2;
  arch->skills[SKILL_NEGOTIATION] = 2;
  arch->skills[SKILL_STEALTH] = 3;
  arch->skills[SKILL_SPELLDESIGN] = 4;
  
  ARCH_SPELL(SPELL_HEAL, 0, 5);
  ARCH_SPELL(SPELL_COMBATSENSE, 0, 3);
  ARCH_SPELL(SPELL_FLAMETHROWER, 0, 6);
  ARCH_SPELL(SPELL_MINDLINK, 0, 1);
  ARCH_SPELL(SPELL_ARMOR, 0, 3);
  ARCH_SPELL(SPELL_IMP_INVIS, 0, 1);
  ARCH_SPELL(SPELL_STUNBOLT, 0, 6);
  
  // arch->nuyen = 22660;
  
  arch->modulator = OBJ_DOCWAGON_GOLD_MODULATOR;
  
  arch->worn[WEAR_EYES] = OBJ_THERMOGRAPHIC_GOGGLES;
  arch->worn[WEAR_BACK] = OBJ_NEOPHYTE_DUFFELBAG;
  arch->worn[WEAR_ABOUT] = OBJ_LONDON_FOG_MERLIN;
  arch->worn[WEAR_BODY] = OBJ_PLATED_ARMOR_VEST;
  arch->worn[WEAR_UNDER] = OBJ_FORMFIT_III;
  arch->worn[WEAR_ARMS] = OBJ_FOREARM_GUARDS;
  arch->worn[WEAR_WAIST] = OBJ_BLACK_LEATHER_DUTY_BELT;
  arch->worn[WEAR_LEGS] = OBJ_BLACK_SLACKS;
  arch->worn[WEAR_FEET] = OBJ_BLACK_DRESS_SHOES;
  
  i = 0;
  arch->carried[i++] = OBJ_ELECTRONICS_KIT;
  arch->carried[i++] = OBJ_TITLE_TO_AMERICAR;
  arch->carried[i++] = OBJ_POCKET_SECRETARY;
  arch->carried[i++] = OBJ_CELL_PHONE;
  assert(i < NUM_ARCHETYPE_CARRIED);
  
  i = 0;
  ARCH_FOCUS(OBJ_ASH_LEAF_ANKLET, SPELL_IMP_INVIS);
  ARCH_FOCUS(OBJ_ORICHALCUM_BRACELET, SPELL_ARMOR);
  ARCH_FOCUS(OBJ_ORICHALCUM_BRACELET, SPELL_COMBATSENSE);
  assert(i < NUM_ARCHETYPE_FOCI);
  
  return arch;
}
#undef ARCH_SPELL
#undef ARCH_FOCUS

// TODO
struct archetype_data *generate_decker() {
  struct archetype_data *arch = new archetype_data;
  int i = 0;
  
  memset(arch, 0, sizeof(struct archetype_data));
  
  arch->name = str_dup("Decker");
  arch->race = RACE_HUMAN;
  
  // correct for decker
  arch->start_room = 91000;
  arch->auth_room = 91032;
  
  // Set attributes.
  arch->attributes[BOD] = 3;
  arch->attributes[QUI] = 5;
  arch->attributes[STR] = 2;
  arch->attributes[CHA] = 1;
  arch->attributes[INT] = 6;
  arch->attributes[WIL] = 6;
  // Reaction, essence, etc is autocomputed on character creation.
  
  // Set magic stats.
  arch->tradition = TRAD_MUNDANE;
  
  // Set skills.
  arch->skills[SKILL_SMG] = 6;
  arch->skills[SKILL_CYBERTERM_DESIGN] = 6;
  arch->skills[SKILL_COMPUTER] = 6;
  arch->skills[SKILL_ELECTRONICS] = 3;
  arch->skills[SKILL_BR_COMPUTER] = 4;
  arch->skills[SKILL_NEGOTIATION] = 2;
  arch->skills[SKILL_CORPORATE_ETIQUETTE] = 2;
  arch->skills[SKILL_STREET_ETIQUETTE] = 1;
  arch->skills[SKILL_PROGRAM_COMBAT] = 2;
  arch->skills[SKILL_PROGRAM_DEFENSIVE] = 2;
  arch->skills[SKILL_PROGRAM_OPERATIONAL] = 2;
  arch->skills[SKILL_PROGRAM_SPECIAL] = 2;
  arch->skills[SKILL_PROGRAM_CYBERTERM] = 2;
  arch->skills[SKILL_DATA_BROKERAGE] = 4;
  
  // Inventory.
  arch->weapon = OBJ_SCK_MODEL_100;
  arch->weapon_barrel = OBJ_VENT_IV;
  arch->ammo_q = 500;
  
  arch->nuyen = 22660;
  
  arch->modulator = OBJ_DOCWAGON_BASIC_MOD;
  
  arch->worn[WEAR_ABOUT] = OBJ_LONDON_FOG_PROFESSIONAL;
  arch->worn[WEAR_BACK] = OBJ_NEOPHYTE_DUFFELBAG;
  arch->worn[WEAR_BODY] = OBJ_PLATED_ARMOR_VEST;
  arch->worn[WEAR_UNDER] = OBJ_FORMFIT_III;
  arch->worn[WEAR_ARMS] = OBJ_FOREARM_GUARDS;
  arch->worn[WEAR_LEGS] = OBJ_BLACK_BDU_PANTS;
  arch->worn[WEAR_FEET] = OBJ_PAIR_OF_WHITE_TRAINERS;
  arch->worn[WEAR_THIGH_L] = OBJ_THIGH_HOLSTER;

  
  i = 0;
  arch->carried[i++] = OBJ_CYBERDECK_REPAIR_KIT;
  arch->carried[i++] = OBJ_TITLE_TO_BISON;
  arch->carried[i++] = OBJ_NOVATECH_BURNER;
  arch->carried[i++] = OBJ_MITSUHAMA_Z4;
  arch->carried[i++] = OBJ_ELECTRONICS_KIT;
  arch->carried[i++] = OBJ_POCKET_SECRETARY;
  arch->carried[i++] = OBJ_CELL_PHONE;
  assert(i < NUM_ARCHETYPE_CARRIED);
  
  arch->cyberdeck = OBJ_CMT_AVATAR;
  
  i = 0;
  arch->software[i++] = OBJ_NOVATECH_SIX_SENSORS;
  arch->software[i++] = OBJ_NOVATECH_SIX_MASKING;
  arch->software[i++] = OBJ_NOVATECH_SIX_BOD;
  arch->software[i++] = OBJ_FUCHI_LTD_EVASION;
  arch->software[i++] = OBJ_CATCO_SLEAZE;
  arch->software[i++] = OBJ_TRANSYS_SCRIBE;
  arch->software[i++] = OBJ_TRANSYS_RIFFLE;
  arch->software[i++] = OBJ_RENRAKU_BYPASS;
  arch->software[i++] = OBJ_FOXFIRE_KITSUNE;
  arch->software[i++] = OBJ_TRANSYS_ARMOR;
  arch->software[i++] = OBJ_MATRIX_SWORD;
  assert(i < NUM_ARCHETYPE_SOFTWARE);
  
  i = 0;
  arch->cyberware[i++] = OBJ_CYB_DATAJACK;
  arch->cyberware[i++] = OBJ_CYB_ENCEPHALON_II;
  arch->cyberware[i++] = OBJ_CYB_MATH_SPU_III;
  arch->cyberware[i++] = OBJ_CYB_SMARTLINK_II;
  arch->cyberware[i++] = OBJ_CYB_EYE_PACKAGE_LL_TH_FC_ALPHA;
  assert(i < NUM_ARCHETYPE_CYBERWARE);
  
  i = 0;
  arch->bioware[i++] = OBJ_BIO_MUSCLE_TONER_III;
  arch->bioware[i++] = OBJ_BIO_CEREBRAL_BOOSTER_II;
  arch->bioware[i++] = OBJ_BIO_ENHANCED_ARTICULATION;
  arch->bioware[i++] = OBJ_BIO_MUSCLE_AUGMENTATION_II;
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
  
  // correct for rigger
  arch->start_room = 90900;
  arch->auth_room = 90933;
  
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
  arch->weapon_top = 28702; // nicami scope
  arch->weapon_barrel = 80403; // vent IV
  arch->weapon_under = 31111; // smartlink II
  arch->ammo_q = 500;
  
  arch->nuyen = 22660;
  
  arch->modulator = 16208; // platinum
  
  arch->worn[WEAR_ABOUT] = 1833; // a black trench coat
  
  i = 0;
  arch->carried[i++] = 450; // medkit
  assert(i < NUM_ARCHETYPE_CARRIED);
  
  // Cyberware.
  i = 0;
  arch->cyberware[i++] = 85066; // ceramic bone lacing
  assert(i < NUM_ARCHETYPE_CYBERWARE);
  
  // Bioware.
  i = 0;
  arch->bioware[i++] = 85803; // enhanced articulation
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
    archetypes[i]->skills[SKILL_UNARMED_COMBAT] = 2;
    
    log_vfprintf("... %s.", archetypes[i]->name);
  }
}
