#include "types.h"
#include "structs.h"
#include "awake.h"
#include "comm.h" // for shutdown()
#include "db.h"
#include "archetypes.h"

struct archetype_data *archetypes[NUM_CCR_ARCHETYPES];

#define MAG 6
struct archetype_data *generate_street_samurai() {
  struct archetype_data *arch = new archetype_data;
  int i = 0;
  
  memset(arch, 0, sizeof(struct archetype_data));
  
  arch->name = str_dup("Street Samurai");
  arch->race = RACE_HUMAN;
  
  // Set attributes.
  arch->attributes[BOD] = 6;
  arch->attributes[QUI] = 6;
  arch->attributes[STR] = 6;
  arch->attributes[CHA] = 1;
  arch->attributes[INT] = 6;
  arch->attributes[WIL] = 6;
  // Reaction, essence, etc is autocomputed on character creation.
  
  // Set magic stats.
  arch->magic = 0;
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
  arch->weapon = 838; // colt m-23, should be set to burst fire
  arch->weapon_top = 28702; // nicami scope
  arch->weapon_barrel = 80403; // vent IV
  arch->weapon_under = 31111; // smartlink II
  arch->ammo_q = 500;
  
  arch->nuyen = 22660;
  
  arch->modulator = 16208; // platinum
  
  arch->worn[WEAR_ABOUT] = 1833; // a black trench coat
  arch->worn[WEAR_BODY] = 64903; // a 100% cotton t-shirt
  arch->worn[WEAR_UNDER] = 80734; // formfit III
  arch->worn[WEAR_ARMS] = 700; // forearm guards
  arch->worn[WEAR_BACK] = 2059; // rifle strap
  arch->worn[WEAR_WAIST] = 9402; // a black leather duty belt
  arch->worn[WEAR_LEGS] = 14618; // a pair of black BDU pants
  arch->worn[WEAR_FEET] = 29010; // a pair of black combat boots
  
  i = 0;
  arch->carried[i++] = 450; // medkit
  arch->carried[i++] = 47397; // vehicle toolkit
  arch->carried[i++] = 60532; // electroncis kit
  arch->carried[i++] = 60530; // cell phone
  arch->carried[i++] = OBJ_MAP_OF_SEATTLE;
  arch->carried[i++] = 892; // scorpion tit
  arch->carried[i++] = 897; // bison tit
  arch->carried[i++] = 31542; // secretary
  arch->carried[i++] = 60535; // duffelbag
  assert(i < NUM_ARCHETYPE_CARRIED);
  
  // Cyberware.
  i = 0;
  arch->cyberware[i++] = 85066; // ceramic bone lacing
  arch->cyberware[i++] = 85012; // datajack
  arch->cyberware[i++] = 85270; // boosted reflexes III -alpha
  arch->cyberware[i++] = 85053; // thermographic vision
  arch->cyberware[i++] = 85318; // smartlink 2 --alpha
  assert(i < NUM_ARCHETYPE_CYBERWARE);
  
  // Bioware.
  i = 0;
  arch->bioware[i++] = 85803; // enhanced articulation
  arch->bioware[i++] = 85812; // muscle toner iv
  arch->bioware[i++] = 85940; // trauma damper
  arch->bioware[i++] = 85939; // synamptic accelerator II
  arch->bioware[i++] = 85927; // cerebral booster II
  assert(i < NUM_ARCHETYPE_BIOWARE);
  
  return arch;
}

// TODO
struct archetype_data *generate_adept() {
  struct archetype_data *arch = new archetype_data;
  int i = 0;
  
  memset(arch, 0, sizeof(struct archetype_data));
  
  arch->name = str_dup("Adept");
  arch->race = RACE_HUMAN;
  
  // Set attributes.
  arch->attributes[BOD] = 6;
  arch->attributes[QUI] = 6;
  arch->attributes[STR] = 6;
  arch->attributes[CHA] = 1;
  arch->attributes[INT] = 6;
  arch->attributes[WIL] = 6;
  // Reaction, essence, etc is autocomputed on character creation.
  
  // Set magic stats.
  arch->magic = 0;
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
struct archetype_data *generate_shaman() {
  struct archetype_data *arch = new archetype_data;
  int i = 0;
  
  memset(arch, 0, sizeof(struct archetype_data));
  
  arch->name = str_dup("Adept");
  arch->race = RACE_HUMAN;
  
  // Set attributes.
  arch->attributes[BOD] = 6;
  arch->attributes[QUI] = 6;
  arch->attributes[STR] = 6;
  arch->attributes[CHA] = 1;
  arch->attributes[INT] = 6;
  arch->attributes[WIL] = 6;
  // Reaction, essence, etc is autocomputed on character creation.
  
  // Set magic stats.
  arch->magic = 0;
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
struct archetype_data *generate_decker() {
  struct archetype_data *arch = new archetype_data;
  int i = 0;
  
  memset(arch, 0, sizeof(struct archetype_data));
  
  arch->name = str_dup("Adept");
  arch->race = RACE_HUMAN;
  
  // Set attributes.
  arch->attributes[BOD] = 6;
  arch->attributes[QUI] = 6;
  arch->attributes[STR] = 6;
  arch->attributes[CHA] = 1;
  arch->attributes[INT] = 6;
  arch->attributes[WIL] = 6;
  // Reaction, essence, etc is autocomputed on character creation.
  
  // Set magic stats.
  arch->magic = 0;
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

#undef MAG

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
    default:
      log_vfprintf("ERROR: Invalid archetype #%d passed to generate_archetypes!", index);
      shutdown();
      return NULL;
  }
}

void generate_archetypes() {
  for (int i = 0; i < NUM_CCR_ARCHETYPES; i++) {
    archetypes[i] = generate_archetype(i);
    archetypes[i]->subsidy_card = 30000;
    log_vfprintf("... %s.", archetypes[i]->name);
  }
}
