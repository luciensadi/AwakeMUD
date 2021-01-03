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
  
  arch->subsidy_card = 30000;
  
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
  arch->carried[i++] = OBJ_MAP_OF_SEATTLE;
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
  ARCH_ADEPT_POWER(ADEPT_MYSTIC_ARMOUR, 2);
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
  
  arch->subsidy_card = 30000;
  
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
  arch->carried[i++] = OBJ_MAP_OF_SEATTLE;
  arch->carried[i++] = OBJ_TITLE_TO_SCORPION; // scorpion tit
  arch->carried[i++] = OBJ_POCKET_SECRETARY; // secretary
  arch->carried[i++] = OBJ_NEOPHYTE_DUFFELBAG; // duffelbag
  arch->carried[i++] = OBJ_STEEL_COMBAT_AXE;
  assert(i < NUM_ARCHETYPE_CARRIED);
  
  return arch;
}
#undef ARCH_ADEPT_POWER

// TODO
#define ARCH_SPELL(spell, force) arch->spells[i][0] = (spell); arch->spells[i++][1] = (force);
struct archetype_data *generate_shaman() {
  struct archetype_data *arch = new archetype_data;
  int i = 0;
  
  memset(arch, 0, sizeof(struct archetype_data));
  
  arch->name = str_dup("Shaman");
  arch->race = RACE_HUMAN;
  
  arch->start_room = 90800;
  arch->auth_room = 90800; // todo
  
  // Set attributes.
  arch->attributes[BOD] = 6;
  arch->attributes[QUI] = 6;
  arch->attributes[STR] = 6;
  arch->attributes[CHA] = 1;
  arch->attributes[INT] = 6;
  arch->attributes[WIL] = 6;
  // Reaction, essence, etc is autocomputed on character creation.
  
  // Set magic stats.
  arch->magic = 600;
  arch->tradition = TRAD_SHAMANIC;
  arch->forcepoints = 25;
  
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

// TODO
struct archetype_data *generate_hermetic() {
  struct archetype_data *arch = new archetype_data;
  int i = 0;
  
  memset(arch, 0, sizeof(struct archetype_data));
  
  arch->name = str_dup("Hermetic");
  arch->race = RACE_HUMAN;
  
  // kosher for hermetic mage
  arch->start_room = 90700;
  arch->auth_room = 90738;
  
  // Set attributes.
  arch->attributes[BOD] = 6;
  arch->attributes[QUI] = 6;
  arch->attributes[STR] = 6;
  arch->attributes[CHA] = 1;
  arch->attributes[INT] = 6;
  arch->attributes[WIL] = 6;
  // Reaction, essence, etc is autocomputed on character creation.
  
  // Set magic stats.
  arch->magic = 600;
  arch->tradition = TRAD_HERMETIC;
  arch->forcepoints = 25;
  
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
#undef ARCH_SPELL

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
    case ARCHETYPE_DECKER:
      return generate_decker();
    case ARCHETYPE_HERMETIC:
      return generate_hermetic();
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
    log_vfprintf("... %s.", archetypes[i]->name);
  }
}
