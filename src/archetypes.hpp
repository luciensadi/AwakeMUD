#ifndef __ARCHETYPES_H
#define __ARCHETYPES_H

#define NUM_ARCHETYPE_SPELLS    10
#define NUM_ARCHETYPE_ABILITIES 10
#define NUM_ARCHETYPE_CYBERWARE 10
#define NUM_ARCHETYPE_BIOWARE   10
#define NUM_ARCHETYPE_CARRIED   20
#define NUM_ARCHETYPE_SOFTWARE  20
#define NUM_ARCHETYPE_FOCI      10

struct archetype_data {
  const char *name;
  const char *difficulty_rating;
  int race;
  int attributes[6];
  int skills[MAX_SKILLS];
  
  vnum_t start_room;
  vnum_t auth_room;
  vnum_t warning_room;
  
  int magic;
  int tradition;
  int aspect;
  int totem;
  int totemspirit;
  
  int powers[NUM_ARCHETYPE_ABILITIES][2]; // 0 is power, 1 is rating
  int spells[NUM_ARCHETYPE_SPELLS][3]; // 0 is spell ID, 1 is subtype, 2 is force
  vnum_t foci[NUM_ARCHETYPE_FOCI][2]; // 0 is vnum, 1 is category type (NULL if n/a)
  int forcepoints; // for bonding foci
  
  vnum_t cyberdeck;
  vnum_t software[NUM_ARCHETYPE_SOFTWARE];
  
  vnum_t weapon;
  vnum_t weapon_top;
  vnum_t weapon_barrel;
  vnum_t weapon_under;
  
  int ammo_q;
  int nuyen;
  int subsidy_card;
  vnum_t modulator;
  
  vnum_t worn[NUM_WEARS];
  vnum_t carried[NUM_ARCHETYPE_CARRIED];
  vnum_t cyberware[NUM_ARCHETYPE_CYBERWARE];
  vnum_t bioware[NUM_ARCHETYPE_BIOWARE];
};

#define ARCHETYPE_STREET_SAMURAI 0
#define ARCHETYPE_ADEPT          1
#define ARCHETYPE_SHAMANIC_MAGE  2
#define ARCHETYPE_STREET_MAGE    3
#define ARCHETYPE_DECKER         4
#define ARCHETYPE_RIGGER         5

// TODO: Increment this as the archetypes complete.
#define NUM_CCR_ARCHETYPES 5

extern struct archetype_data *archetypes[NUM_CCR_ARCHETYPES];

#endif // __ARCHETYPES_H
