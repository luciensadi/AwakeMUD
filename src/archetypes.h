#ifndef __ARCHETYPES_H
#define __ARCHETYPES_H

#define NUM_ARCHETYPE_CYBERWARE 10
#define NUM_ARCHETYPE_BIOWARE   10
#define NUM_ARCHETYPE_CARRIED   20

struct archetype_data {
  const char *name;
  int race;
  int attributes[6];
  int skills[MAX_SKILLS];
  
  int magic;
  int tradition;
  int aspect;
  int powerpoints;
  int forcepoints;
  int abilities[1];
  
  vnum_t weapon;
  vnum_t weapon_top;
  vnum_t weapon_barrel;
  vnum_t weapon_under;
  
  int ammo_q;
  int nuyen;
  int subsidy_card;
  int modulator;
  
  vnum_t worn[NUM_WEARS];
  vnum_t carried[NUM_ARCHETYPE_CARRIED];
  vnum_t cyberware[NUM_ARCHETYPE_CYBERWARE];
  vnum_t bioware[NUM_ARCHETYPE_BIOWARE];
};


#define ARCHETYPE_STREET_SAMURAI 0

#define NUM_CCR_ARCHETYPES 1

extern struct archetype_data *archetypes[NUM_CCR_ARCHETYPES];

#endif // __ARCHETYPES_H
