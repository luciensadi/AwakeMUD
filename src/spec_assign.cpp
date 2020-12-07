/* ************************************************************************
*   File: spec_assign.c                                 Part of CircleMUD *
*  Usage: Functions to assign function pointers to objs/mobs/rooms        *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <stdio.h>

#include "structs.h"
#include "awake.h"
#include "db.h"
#include "interpreter.h"
#include "utils.h"

void ASSIGNMOB(long mob, SPECIAL(fname));

/* arrays for trainers/teachers/adepts/masters */

/* order: vnum, attributes, 1 - newbie trainer (else 0)     */
/* to have multiple attributes, separate them with |      */
struct train_data trainers[] = {
                       { 2506, TSTR, 0 },
                       { 4255, TBOD, 0 },
                       { 60500, TBOD | TQUI | TSTR | TCHA | TINT | TWIL, 1 },
                       { 17104, TBOD | TQUI | TSTR, 0 },
                       { 9413, TCHA | TINT | TWIL, 0 },
                       { 0, 0, 0 } /* this MUST be last */
                     };

/* order: vnum, skill1, skill2, skill3, skill4, skill5, learn message, level  */
/* if they teach < 5 skills, put a 0 in the remaining skill slots    */
/* available levels are NEWBIE (4), AMATEUR (6/7), and ADVANCED (10/11)   */
struct teach_data metamagict[] = { 
                       { 24806, { META_REFLECTING, META_CLEANSING, 0, 0, 0, 0, 0, 0 }, "", 0 },
                         { 4250, { META_ANCHORING, META_QUICKENING, 0, 0, 0, 0, 0, 0 }, "", 0 },
                         { 4251, { META_ANCHORING, META_POSSESSING, 0, 0, 0, 0, 0, 0 }, "", 0 },
                         { 10010, { META_ANCHORING, META_CENTERING, META_CLEANSING, META_INVOKING, META_MASKING, META_POSSESSING, META_QUICKENING, META_REFLECTING, META_SHIELDING }, "", 0},
                         { 35538, { META_MASKING, META_CLEANSING, META_CENTERING, 0, 0, 0, 0, 0, 0 }, "", 0},
                         { 27426, { META_SHIELDING, META_POSSESSING, META_INVOKING, META_MASKING, 0, 0, 0, 0 }, "", 0},
                         { 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "", 0}
                       };

struct teach_data teachers[] = {
                       { 1001, { SKILL_BR_EDGED, SKILL_BR_POLEARM, SKILL_BR_CLUB, SKILL_BR_THROWINGWEAPONS, SKILL_BR_WHIPS, 
                         SKILL_BR_PROJECTILES, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After about an hour of shuffling slides you feel you can now do "
                         "your job safer.\r\n", AMATEUR },
                       { 1002, { SKILL_BR_PISTOL, SKILL_BR_SHOTGUN, SKILL_BR_RIFLE, SKILL_BR_HEAVYWEAPON, SKILL_BR_SMG, 
                         SKILL_BR_ARMOUR, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After about an hour of shuffling slides you feel you can now do your job "
                         "safer.\r\n", AMATEUR }, 
                       { 1003, { SKILL_SINGING, SKILL_CHANTING, SKILL_DANCING, SKILL_INSTRUMENT, SKILL_ACTING, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "After about an hour of shuffling slides you feel you can now do your job safer.\r\n", AMATEUR },
                       { 1004, { SKILL_OFFHAND_EDGED, SKILL_OFFHAND_CLUB, SKILL_OFFHAND_CYBERIMPLANTS, SKILL_OFFHAND_WHIP, 
                         SKILL_SPRAY_WEAPONS, SKILL_GUNCANE, SKILL_BRACERGUN, SKILL_BLOWGUN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After about an hour of "
                         "shuffling slides you feel you can now do your job safer.\r\n", AMATEUR },
                       { 1005, { SKILL_NAVI_LAND, SKILL_NAVI_AIR, SKILL_NAVI_WATER, SKILL_PILOT_ROTORCRAFT, SKILL_PILOT_FIXEDWING,
                         SKILL_PILOT_VECTORTHRUST, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After about an hour of shuffling slides you feel you can now do " 
                         "your job safer.\r\n", AMATEUR },
                       { 1006, { SKILL_INTIMIDATION, SKILL_INTERROGATION, SKILL_LEADERSHIP, SKILL_SURVIVAL, SKILL_LEGERDEMAIN,
                         SKILL_TRACK, SKILL_DISGUISE, SKILL_SMALL_UNIT_TACTICS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After about an hour of shuffling slides you "
                         "feel you can now do your job safer.\r\n", AMATEUR },
                       { 1007, { SKILL_ANIMAL_HANDLING, SKILL_ANIMAL_TAMING, SKILL_CHEMISTRY, SKILL_PHARMA, 
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After about an hour of shuffling slides you feel you can now do your job safer.\r\n", AMATEUR },
                       { 1008, { SKILL_DEMOLITIONS, SKILL_DIVING, SKILL_PARACHUTING, SKILL_UNDERWATER_COMBAT, SKILL_LOCK_PICKING,
                         SKILL_RIDING, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After about an hour of shuffling slides you feel you can now do your job "
                         "safer.\r\n", AMATEUR },
                       { 2508, { SKILL_BIOTECH, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After hours of medical research and instruction, you begin "
                         "to\r\nunderstand more of the basic biotech procedures.\r\n", AMATEUR },
                       { 2701, { SKILL_ATHLETICS, SKILL_STEALTH, SKILL_UNARMED_COMBAT, SKILL_EDGED_WEAPONS,
                         SKILL_WHIPS_FLAILS, SKILL_PROJECTILES, SKILL_THROWING_WEAPONS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "Toh Li gives you the workout of your life, but you come out more learned.", AMATEUR },
                       { 3722, { SKILL_ATHLETICS, SKILL_RIFLES, SKILL_PISTOLS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "After hours of study and physical practice, you feel like you've learned\r\nsomething.\r\n", AMATEUR },
                       { 4101, { SKILL_SHOTGUNS, SKILL_PISTOLS, SKILL_RIFLES, SKILL_SMG, SKILL_ASSAULT_RIFLES, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "After hours of study and target practice, you feel like you've learned\r\nsomething.\r\n", AMATEUR },
                       { 4102, { SKILL_CLUBS, SKILL_EDGED_WEAPONS, SKILL_POLE_ARMS, SKILL_WHIPS_FLAILS,
                         SKILL_PILOT_CAR, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After hours of study and practice, you feel like you've "
                         "learned\r\nsomething.\r\n", AMATEUR },
                       { 4103, { SKILL_SHOTGUNS, SKILL_PISTOLS, SKILL_RIFLES, SKILL_SMG, SKILL_ASSAULT_RIFLES,
                         SKILL_MACHINE_GUNS, SKILL_MISSILE_LAUNCHERS, SKILL_ASSAULT_CANNON, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "After hours of study and target practice, you feel like you've learned\r\nsomething.\r\n", AMATEUR },
                       { 4104, { SKILL_CLUBS, SKILL_EDGED_WEAPONS, SKILL_POLE_ARMS, SKILL_WHIPS_FLAILS, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After hours of study and melee practice, you feel like you've learned\r\nsomething.\r\n", AMATEUR },
                       { 4250, { SKILL_SORCERY, SKILL_CONJURING, SKILL_AURA_READING, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "After hours of study and magical practice, you feel like you've learned\r\nsomething.\r\n", AMATEUR },
                       { 4251, { SKILL_CONJURING, SKILL_SORCERY, SKILL_AURA_READING, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "After hours of study and magical practice, you feel like you've learned\r\nsomething.\r\n", AMATEUR },
                       { 4257, { SKILL_ASSAULT_RIFLES, SKILL_TASERS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "After hours of study and weapon practice, you feel like you've learned\r\nsomething.\r\n", AMATEUR },
  
  /* Removed nerp skills from trainers:
   SKILL_ENCHANTING, SKILL_AURA_READING, SKILL_CENTERING, SKILL_TALISMONGERING, SKILL_PROJECTILES, SKILL_THROWING_WEAPONS SKILL_ORALSTRIKE, SKILL_MEDITATION
   */
  
  // Imm trainer (Dumbledore)
  { 10010, { SKILL_CONJURING, SKILL_SORCERY, SKILL_SPELLDESIGN,
    /*, SKILL_GETTING_THROWN_OFF_OF_A_TOWER, */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After what feels like seven years of magical study, you feel like you've learned something.\r\n", GODLY },
  
  // Newbie magic teacher
  { 60501, { SKILL_CONJURING, SKILL_SORCERY, SKILL_SPELLDESIGN, SKILL_AURA_READING,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After hours of study and practice, you feel like you've learned something.\r\n", NEWBIE },
  
  { 60502, { SKILL_COMPUTER, SKILL_BR_COMPUTER, SKILL_DATA_BROKERAGE, SKILL_CYBERTERM_DESIGN, SKILL_ELECTRONICS, SKILL_BR_ELECTRONICS,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After hours of study and practice, you feel like you've learned something.\r\n", NEWBIE },
  
  { 60503, { SKILL_ARABIC, SKILL_CHINESE, SKILL_CROW, SKILL_ENGLISH, SKILL_FRENCH, SKILL_GAELIC, SKILL_GERMAN,
    SKILL_ITALIAN, SKILL_JAPANESE, SKILL_KOREAN, SKILL_LATIN, SKILL_MAKAW, SKILL_NAVAJO, SKILL_ORZET,
    SKILL_RUSSIAN, SKILL_SALISH, SKILL_SIOUX, SKILL_SPANISH, SKILL_SPERETHIEL, SKILL_UTE },
    "Von Richter runs through basic conjugation and sentence structure with you.\r\n", NEWBIE },
  
  { 60504, { SKILL_PILOT_BIKE, SKILL_PILOT_CAR, SKILL_PILOT_TRUCK, SKILL_BR_BIKE,
    SKILL_BR_DRONE, SKILL_BR_CAR, SKILL_BR_TRUCK, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After hours of study and practice, you feel like you've learned something.\r\n", NEWBIE },
  
  { 60505, { SKILL_CLUBS, SKILL_CYBER_IMPLANTS, SKILL_EDGED_WEAPONS, SKILL_POLE_ARMS, SKILL_UNARMED_COMBAT, SKILL_WHIPS_FLAILS, // Jet Li
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After hours of study and practice, you feel like you've learned something.\r\n", NEWBIE },
  
  { 60539, { SKILL_PROJECTILES, SKILL_THROWING_WEAPONS, // Kyle the Mall Ninja
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "Kyle teaches you things you're pretty sure were more effective in the late 1800s.\r\n", NEWBIE },
  
  { 60540, { SKILL_BR_PISTOL, SKILL_BR_SHOTGUN, SKILL_BR_RIFLE, SKILL_BR_HEAVYWEAPON, SKILL_BR_SMG, // Jean Samuel Pauly
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "Jean walks you through the intricacies of handloading.\r\n", NEWBIE },
  
  { 60506, { SKILL_ASSAULT_RIFLES, SKILL_GUNNERY, SKILL_MACHINE_GUNS, SKILL_PISTOLS, SKILL_RIFLES, SKILL_SHOTGUNS, SKILL_SMG, // Mick
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After hours of study and practice, you feel like you've learned something.\r\n", NEWBIE },
  
  { 60532, { SKILL_PROGRAM_COMBAT, SKILL_PROGRAM_CYBERTERM, SKILL_PROGRAM_DEFENSIVE, SKILL_PROGRAM_OPERATIONAL, SKILL_PROGRAM_SPECIAL,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "Deadlock picks up some design plans from his desk and runs through the basic process of setting one up, you feel like you've learned something.\r\n", NEWBIE},
  
  { 60533, { SKILL_CORPORATE_ETIQUETTE, SKILL_ELF_ETIQUETTE, SKILL_MEDIA_ETIQUETTE, SKILL_NEGOTIATION, SKILL_STREET_ETIQUETTE, SKILL_TRIBAL_ETIQUETTE,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "Lucy runs through the proper decorum to use in certain situations, you feel like you've learned something.\r\n", NEWBIE},
  
  { 60534, { SKILL_ATHLETICS, SKILL_BIOTECH, SKILL_POLICE_PROCEDURES, SKILL_STEALTH,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "Steve roughly throws you a text book and tells you to read it, you feel like you've learned something.\r\n", NEWBIE},
  
                     { 30700, { SKILL_ENGLISH, SKILL_JAPANESE, SKILL_CHINESE, SKILL_KOREAN,
                         SKILL_SPERETHIEL, SKILL_SALISH, SKILL_ITALIAN, SKILL_NEGOTIATION, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Socrates shows you the intricities "
                         "of the language and you emerge with a greater understanding.\r\n", AMATEUR },
                       { 13499, { SKILL_COMPUTER, SKILL_ELECTRONICS, SKILL_BR_COMPUTER, SKILL_BR_ELECTRONICS, SKILL_PROGRAM_COMBAT, SKILL_PROGRAM_DEFENSIVE, SKILL_PROGRAM_CYBERTERM, SKILL_CYBERTERM_DESIGN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Brian explains some concepts you had yet to understand "
                         "and\r\nyou feel like you've learned something.\r\n", AMATEUR },
                       { 14608, { SKILL_PILOT_CAR, SKILL_GRENADE_LAUNCHERS, SKILL_MACHINE_GUNS,
                         SKILL_MISSILE_LAUNCHERS, SKILL_ASSAULT_CANNON, SKILL_PILOT_BIKE, SKILL_PILOT_TRUCK, SKILL_GUNNERY, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "Hal shows you a trick or two, and the rest just falls into place.\r\n", AMATEUR },
                       { 14638, { SKILL_COMPUTER, SKILL_ELECTRONICS, SKILL_BR_COMPUTER, SKILL_BIOTECH, SKILL_BR_DRONE, SKILL_PROGRAM_SPECIAL, SKILL_PROGRAM_OPERATIONAL, SKILL_DATA_BROKERAGE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Gargamel looks at you, hands you some parts, drops you a clue, and you get it.\r\n", AMATEUR},
                       { 7223, { SKILL_UNARMED_COMBAT, SKILL_THROWING_WEAPONS, SKILL_STEALTH, SKILL_ATHLETICS,
                         SKILL_PROJECTILES, SKILL_CYBER_IMPLANTS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Shing gives you a work out like none you've ever had, "
                         "but\r\nyou feel like you've learned something.\r\n", AMATEUR },
                       { 24806, { SKILL_SORCERY, SKILL_CONJURING, SKILL_CENTERING, SKILL_ENCHANTING, SKILL_NEGOTIATION,
                         SKILL_AURA_READING, SKILL_SPELLDESIGN, SKILL_TALISMONGERING, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Hermes imparts his wisdom upon you.\r\n", AMATEUR },
                       { 37500, { SKILL_BR_CAR, SKILL_BR_BIKE, SKILL_BR_DRONE, SKILL_BR_TRUCK, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Marty shows you a few tricks of the "
                         "trade and you emerge more skilled than before.\r\n", AMATEUR }, 
  
  { 35502, { SKILL_STREET_ETIQUETTE,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "You spend some time talking to the people in line.\r\n", AMATEUR },
  
  { 17111, { SKILL_CORPORATE_ETIQUETTE, SKILL_NEGOTIATION,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "You spend some time making small talk with the secretary.\r\n", AMATEUR },
  
#ifdef USE_PRIVATE_CE_WORLD
  // Jessie (SPLAT! Paintballer)
  { 70604, { SKILL_PISTOLS, SKILL_BR_PISTOL, SKILL_STEALTH, SKILL_ATHLETICS,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  "You learn a few things from the surprisingly-knowledgeable kid.\r\n", AMATEUR },
#endif
  
                    { 65106, { SKILL_MEDIA_ETIQUETTE, SKILL_STEALTH, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "You talk to the reporter about current events for"
                         " a while.\r\n", AMATEUR },
                       { 18311, { SKILL_STREET_ETIQUETTE, SKILL_MEDIA_ETIQUETTE, SKILL_ELF_ETIQUETTE, SKILL_CORPORATE_ETIQUETTE, 
                                  SKILL_TRIBAL_ETIQUETTE, SKILL_NEGOTIATION, SKILL_POLICE_PROCEDURES, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "You read through "
                         "various lifestyle books for a while.\r\n", LIBRARY },
                       { 18312, { SKILL_COMPUTER, SKILL_ELECTRONICS, SKILL_BR_COMPUTER, SKILL_CYBERTERM_DESIGN,
                                  SKILL_BR_ELECTRONICS, SKILL_DATA_BROKERAGE, SKILL_PROGRAM_CYBERTERM, SKILL_PROGRAM_SPECIAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "You read through various technological journals.\r\n", LIBRARY },
                         { 18313, { SKILL_ENGLISH, SKILL_SPERETHIEL, SKILL_SPANISH, SKILL_JAPANESE, SKILL_CHINESE,
                                  SKILL_KOREAN, SKILL_ITALIAN, SKILL_RUSSIAN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "You raid the teach yourself languages section.\r\n", LIBRARY },
                       { 778, { SKILL_SORCERY, SKILL_CONJURING, SKILL_AURA_READING, SKILL_SPELLDESIGN, SKILL_ENCHANTING, SKILL_CENTERING, SKILL_TALISMONGERING, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "The Master teaches you more than you thought possible.\r\n", ADVANCED },
                       { 5914, { SKILL_PISTOLS, SKILL_RIFLES, SKILL_SHOTGUNS, SKILL_SMG, SKILL_ASSAULT_RIFLES,
                         SKILL_GUNNERY, SKILL_MACHINE_GUNS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },  "Lucas runs through some theory with you then lets you "
                         "have a few shots on the range.\r\n", NEWBIE},
                       { 65119, { SKILL_RUSSIAN, SKILL_GERMAN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Olga Chernov brutally crams language forms and syntaxes into your skull.\r\n", AMATEUR },
                       { 33004, { SKILL_ELF_ETIQUETTE, SKILL_GAELIC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, "Milton Buckthorne drops knowledge on your patheticly underpowered brain.\r\n", AMATEUR },
                       { 60235, { SKILL_SIOUX, SKILL_MAKAW, SKILL_CROW, SKILL_UTE, SKILL_SALISH, SKILL_NAVAJO, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "Speaks-with-Many goes over his lessons with you until they become second nature.\r\n", AMATEUR },
                       { 39226, { SKILL_LATIN, SKILL_ARABIC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "The gentleman demonstrates the finer points of articulation and pronunciation until you feel you've learn something new", AMATEUR },
                       { 60540, { SKILL_BR_PISTOL, SKILL_BR_SHOTGUN, SKILL_BR_SMG, SKILL_BR_RIFLE, SKILL_BR_HEAVYWEAPON, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Gary teaches you the finer points of gunsmithing.\r\n", NEWBIE }, 
                       { 22871, { SKILL_BR_PISTOL, SKILL_BR_SHOTGUN, SKILL_BR_SMG, SKILL_BR_RIFLE, SKILL_BR_HEAVYWEAPON, 0, 0, 0 , 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, "You spend some time reading through the library.\r\n", AMATEUR }, 
  { 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Report this bug!\r\n", 0 } // this MUST be last
};

struct adept_data adepts[] = {
                     { 10011,{ 0, 1, 50, 1, 1, 50, 1, 1, 1, 50, 50, 50, 50, 50, 50, 50, 50, 1, 1, 1, 50, 50, 50, 50, 50, 1, 1, 50, 50, 50, 50, 50, 1, 50, 1, 1, 1, 1, 50, 50, 50, 50}, 0 },
                     { 60507,{ 0, 1, 3, 1, 1, 4, 1, 1, 1, 3, 6, 6, 6, 6, 6, 6, 3, 1, 1, 1, 3, 10, 3, 3, 3, 1, 1, 6, 6, 6, 6, 6, 1, 3, 1, 1, 1, 1, 3, 6, 3, 3}, 1 },
                     { 7223, { 0, 1, 3, 0, 1, 4, 1, 0, 0, 3, 0, 6, 0, 0, 6, 0, 6, 0, 0, 0, 0, 10, 0, 0, 6, 1, 1, 2, 0, 6, 0, 0, 0, 0, 0, 0, 1, 0, 3, 6, 1, 6}, 0 },
                     { 2701, { 0, 1, 3, 0, 0, 4, 0, 1, 1, 0, 6, 0, 6, 6, 0, 6, 0, 1, 1, 1, 4,  0, 3, 3, 0, 0, 0, 6, 6, 0, 0, 0, 1, 0, 0, 1, 0, 1, 2, 0, 1, 6}, 0 },
                     { 3603, { 0, 0, 3, 1, 1, 4, 1, 1, 1, 3, 6, 0, 0, 0, 0, 6, 0, 1, 1, 1, 4, 10, 6, 0, 0, 0, 0, 6, 0, 0, 6, 6, 1, 3, 0, 1, 0, 1, 0, 0, 1, 0}, 0 },
//                     { 20326,{ 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 4,  5, 2, 2, 2, 0, 1, 2, 1, 2, 6, 3, 0, 2, 1, 0, 1, 0, 3, 0, 1, 0}, 0 },
                     { 0, { 0, 0, 0, 0 }, 0 } // this MUST be last
                   };

struct spell_trainer spelltrainers[] = {
  /*
//  { 60508, SPELL_DETECTLIFE, "Detect Life", 0, 6},
  { 60508, SPELL_ARMOUR, "Armour", 0, 6},
  { 60508, SPELL_COMBATSENSE, "Combat Sense", 0, 6},
  { 60508, SPELL_HEAL, "Heal", 0, 6},
  { 60508, SPELL_INCATTR, "Increase Strength", STR, 6},
  { 60508, SPELL_INCCYATTR, "Increase (Cybered) Strength", STR, 6},
  { 60508, SPELL_INCREF1, "Increased Reflexes", 0, 6},
  { 60508, SPELL_INVIS, "Invisibility", 0, 6},
  { 60508, SPELL_LIGHTNINGBOLT, "Lightning Bolt", 0, 6},
  { 60508, SPELL_MANABOLT, "Manabolt", 0, 6 },
  { 60508, SPELL_POWERBOLT, "Powerbolt", 0, 6 },
  { 60508, SPELL_STUNBOLT, "Stunbolt", 0, 6 },
  { 60508, SPELL_TREAT, "Treat", 0, 6},
  
//{ 60509, SPELL_DETECTLIFE, "Detect Life", 0, 6},
  { 60509, SPELL_ARMOUR, "Armour", 0, 6},
  { 60509, SPELL_COMBATSENSE, "Combat Sense", 0, 6},
  { 60509, SPELL_HEAL, "Heal", 0, 6},
  { 60509, SPELL_INCATTR, "Increase Strength", ATT_STR, 6},
  { 60509, SPELL_INCCYATTR, "Increase (Cybered) Strength", ATT_STR, 6},
  { 60509, SPELL_INCREF1, "Increased Reflexes", 0, 6},
  { 60509, SPELL_INVIS, "Invisibility", 0, 6},
  { 60509, SPELL_LIGHTNINGBOLT, "Lightning Bolt", 0, 6},
  { 60509, SPELL_MANABOLT, "Manabolt", 0, 6 },
  { 60509, SPELL_POWERBOLT, "Powerbolt", 0, 6 },
  { 60509, SPELL_STUNBOLT, "Stunbolt", 0, 6 },
  { 60509, SPELL_TREAT, "Treat", 0, 6}, */

//{60600, SPELL_DEATHTOUCH, "Death Touch", 0, 6},
//{60600, SPELL_MANABALL, "Manaball", 0, 6},
//{60600, SPELL_POWERBALL, "Powerball", 0, 6},
//{60600, SPELL_STUNBALL, "Stunball", 0, 6},
  {60600, SPELL_MANABOLT, "Manabolt", 0, 6},
  {60600, SPELL_POWERBOLT, "Powerbolt", 0, 6},
  {60600, SPELL_STUNBOLT, "Stunbolt", 0, 6},
  
//{60601, SPELL_HEALTHYGLOW, "Healthy Glow", 0, 6},
//{60601, SPELL_PROPHYLAXIS, "Prophylaxis", 0, 6},
  {60601, SPELL_DECCYATTR, "Decrease (Cybered) Body", BOD, 6},
  {60601, SPELL_DECCYATTR, "Decrease (Cybered) Strength", STR, 6},
  {60601, SPELL_DECCYATTR, "Decrease (Cybered) Quickness", QUI, 6},
  {60601, SPELL_DETOX, "Detox", 0, 6},
  {60601, SPELL_HEAL, "Heal", 0, 6},
  {60601, SPELL_INCATTR, "Increase Body", BOD, 6},
  {60601, SPELL_INCATTR, "Increase Strength", STR, 6},
  {60601, SPELL_INCATTR, "Increase Quickness", QUI, 6},
  {60601, SPELL_INCCYATTR, "Increase (Cybered) Body", BOD, 6},
  {60601, SPELL_INCCYATTR, "Increase (Cybered) Strength", STR, 6},
  {60601, SPELL_INCCYATTR, "Increase (Cybered) Quickness", QUI, 6},
  {60601, SPELL_INCREA, "Increase Reaction", 0, 6},
  {60601, SPELL_INCREF1, "Increase Reflexes +1", 0, 6},
  {60601, SPELL_INCREF2, "Increase Reflexes +2", 0, 6},
  {60601, SPELL_INCREF3, "Increase Reflexes +3", 0, 6},
  {60601, SPELL_STABILIZE, "Stabilize", 0, 6},
  {60601, SPELL_TREAT, "Treat", 0, 6},
  
//{60602, SPELL_ANALYZEDEVICE, "Analyze Device", 0, 6},
//{60602, SPELL_CLAIRAUDIENCE, "Clairaudience", 0, 6},
//{60602, SPELL_CLAIRVOYANCE, "Clairvoyance", 0, 6},
//{60602, SPELL_DETECTENEMIES, "Detect Enemies", 0, 6},
//{60602, SPELL_DETECTINDIV, "Detect Individual", 0, 6},
//{60602, SPELL_DETECTLIFE, "Detect Life", 0, 6},
//{60602, SPELL_DETECTMAGIC, "Detect Magic", 0, 6},
//{60602, SPELL_DETECTOBJECT, "Detect Object", 0, 6},
  {60602, SPELL_COMBATSENSE, "Combat Sense", 0, 6},
  {60602, SPELL_MINDLINK, "Mindlink", 0, 6},
  
//{60603, SPELL_MASSCONFUSION, "Mass Confusion", 0, 6},
//{60603, SPELL_CHAOTICWORLD, "Chaotic World", 0, 6},
//{60603, SPELL_MASK, "Mask", 0, 6},
//{60603, SPELL_PHYSMASK, "Physical Mask", 0, 6},
  {60603, SPELL_CHAOS, "Chaos", 0, 6},
  {60603, SPELL_CONFUSION, "Confusion", 0, 6},
  {60603, SPELL_INVIS, "Invisibility", 0, 6},
  {60603, SPELL_IMP_INVIS, "Improved Invisibility", 0, 6},
  {60603, SPELL_SILENCE, "Silence", 0, 6},
  {60603, SPELL_STEALTH, "Stealth", 0, 6},
  
//{60604, SPELL_TOXICWAVE, "Toxic Wave", 0, 6},
//{60604, SPELL_FIREBALL, "Fireball", 0, 6},
//{60604, SPELL_BALLLIGHTNING, "Ball Lightning", 0, 6},
//{60604, SPELL_PHYSICALBARRIER, "Physical Barrier", 0, 6},
//{60604, SPELL_ASTRALBARRIER, "Astral Barrier", 0, 6},
  {60604, SPELL_ACIDSTREAM, "Acid Stream", 0, 6},
  {60604, SPELL_ARMOUR, "Armour", 0, 6},
  {60604, SPELL_CLOUT, "Clout", 0, 6},
  {60604, SPELL_FLAMETHROWER, "Flamethrower", 0, 6},
  {60604, SPELL_ICESHEET, "Icesheet", 0, 6},
  {60604, SPELL_IGNITE, "Ignite", 0, 6},
  {60604, SPELL_LIGHT, "Light", 0, 6},
  {60604, SPELL_LIGHTNINGBOLT, "Lightning Bolt", 0, 6},
  {60604, SPELL_POLTERGEIST, "Poltergeist", 0, 6},
  {60604, SPELL_SHADOW, "Shadow", 0, 6},
  
                            // Immortal HQ Trainer, not alphabetized because who has the time for that
                            {10013, SPELL_DEATHTOUCH, "Death Touch", 0, 50},
                            {10013, SPELL_MANABOLT, "Manabolt", 0, 50},
                            {10013, SPELL_MANABALL, "Manaball", 0, 50},
                            {10013, SPELL_POWERBOLT, "Powerbolt", 0, 50},
                            {10013, SPELL_POWERBALL, "Powerball", 0, 50},
                            {10013, SPELL_STUNBOLT, "Stunbolt", 0, 50},
                            {10013, SPELL_STUNBALL, "Stunball", 0, 50},
                            {10013, SPELL_DECATTR, "Decrease Body", BOD, 50},
                            {10013, SPELL_DECATTR, "Decrease Strength", STR, 50},
                            {10013, SPELL_DECATTR, "Decrease Quickness", QUI, 50},
                            {10013, SPELL_DECCYATTR, "Decrease (Cybered) Body", BOD, 50},
                            {10013, SPELL_DECCYATTR, "Decrease (Cybered) Strength", STR, 50},
                            {10013, SPELL_DECCYATTR, "Decrease (Cybered) Quickness", QUI, 50},
                            {10013, SPELL_DETOX, "Detox", 0, 50},
                            {10013, SPELL_HEAL, "Heal", 0, 50},
                            {10013, SPELL_TREAT, "Treat", 0, 50},
                            {10013, SPELL_HEALTHYGLOW, "Healthy Glow", 0, 50},
                            {10013, SPELL_INCATTR, "Increase Body", BOD, 50},
                            {10013, SPELL_INCATTR, "Increase Strength", STR, 50},
                            {10013, SPELL_INCATTR, "Increase Quickness", QUI, 50},
                            {10013, SPELL_INCCYATTR, "Increase (Cybered) Body", BOD, 50},
                            {10013, SPELL_INCCYATTR, "Increase (Cybered) Strength", STR, 50},
                            {10013, SPELL_INCCYATTR, "Increase (Cybered) Quickness", QUI, 50},
                            {10013, SPELL_INCREA, "Increase Reaction", 0, 50},
                            {10013, SPELL_INCREF1, "Increase Reflexes +1", 0, 50},
                            {10013, SPELL_INCREF2, "Increase Reflexes +2", 0, 50},
                            {10013, SPELL_INCREF3, "Increase Reflexes +3", 0, 50},
                            {10013, SPELL_PROPHYLAXIS, "Prophylaxis", 0, 50},
                            {10013, SPELL_STABILIZE, "Stabilize", 0, 50},
                            {10013, SPELL_ANALYZEDEVICE, "Analyze Device", 0, 50},
                            {10013, SPELL_CLAIRAUDIENCE, "Clairaudience", 0, 50},
                            {10013, SPELL_CLAIRVOYANCE, "Clairvoyance", 0, 50},
                            {10013, SPELL_COMBATSENSE, "Combat Sense", 0, 50},
                            {10013, SPELL_DETECTENEMIES, "Detect Enemies", 0, 50},
                            {10013, SPELL_DETECTINDIV, "Detect Individual", 0, 50},
                            {10013, SPELL_DETECTLIFE, "Detect Life", 0, 50},
                            {10013, SPELL_DETECTMAGIC, "Detect Magic", 0, 50},
                            {10013, SPELL_DETECTOBJECT, "Detect Object", 0, 50},
                            {10013, SPELL_MINDLINK, "Mindlink", 0, 50},
                            {10013, SPELL_CONFUSION, "Confusion", 0, 50},
                            {10013, SPELL_MASSCONFUSION, "Mass Confusion", 0, 50},
                            {10013, SPELL_CHAOS, "Chaos", 0, 50},
                            {10013, SPELL_CHAOTICWORLD, "Chaotic World", 0, 50},
                            {10013, SPELL_INVIS, "Invisibility", 0, 50},
                            {10013, SPELL_IMP_INVIS, "Improved Invisibility", 0, 50},
                            {10013, SPELL_MASK, "Mask", 0, 50},
                            {10013, SPELL_PHYSMASK, "Physical Mask", 0, 50},
                            {10013, SPELL_SILENCE, "Silence", 0, 50},
                            {10013, SPELL_STEALTH, "Stealth", 0, 50},
                            {10013, SPELL_LIGHT, "Light", 0, 50},
                            {10013, SPELL_ACIDSTREAM, "Acid Stream", 0, 50},
                            {10013, SPELL_TOXICWAVE, "Toxic Wave", 0, 50},
                            {10013, SPELL_FLAMETHROWER, "Flamethrower", 0, 50},
                            {10013, SPELL_FIREBALL, "Fireball", 0, 50},
                            {10013, SPELL_LIGHTNINGBOLT, "Lightning Bolt", 0, 50},
                            {10013, SPELL_BALLLIGHTNING, "Ball Lightning", 0, 50},
                            {10013, SPELL_CLOUT, "Clout", 0, 50},
                            {10013, SPELL_POLTERGEIST, "Poltergeist", 0, 50},
                            {10013, SPELL_ARMOUR, "Armour", 0, 50},
                            {10013, SPELL_PHYSICALBARRIER, "Physical Barrier", 0, 50},
                            {10013, SPELL_ASTRALBARRIER, "Astral Barrier", 0, 50},
                            {10013, SPELL_ICESHEET, "Icesheet", 0, 50},
                            {10013, SPELL_IGNITE, "Ignite", 0, 50},
                            {10013, SPELL_SHADOW, "Shadow", 0, 50},
  
                            // Final line
                            { 0, 0, "", 0, 0},
                          };


/* functions to perform assignments */

void ASSIGNMOB(long mob, SPECIAL(fname))
{
  long rnum;

  if ((rnum = real_mobile(mob)) >= 0) {
    mob_index[rnum].sfunc = mob_index[rnum].func;
    mob_index[rnum].func = fname;
  } else
    log_vfprintf("SYSERR: Attempt to assign spec to non-existent mob #%d", mob);
}

void ASSIGNOBJ(long obj, SPECIAL(fname))
{
  if (real_object(obj) >= 0)
    obj_index[real_object(obj)].func = fname;
  else
    log_vfprintf("SYSERR: Attempt to assign spec to non-existent obj #%d", obj);
}

void ASSIGNWEAPON(long weapon, WSPEC(fname))
{
  if (real_object(weapon) >= 0)
    obj_index[real_object(weapon)].wfunc = fname;
  else
    log_vfprintf("SYSERR: Attempt to assign spec to non-existent weapon #%d", weapon);
}

void ASSIGNROOM(long room, SPECIAL(fname))
{
  if (real_room(room) >= 0)
    world[real_room(room)].func = fname;
  else
    log_vfprintf("SYSERR: Attempt to assign spec to non-existent rm. #%d", room);
}


/* ********************************************************************
*  Assignments                                                        *
******************************************************************** */

/* assign special procedures to mobiles */
void assign_mobiles(void)
{
  int i;

  SPECIAL(postmaster);
  SPECIAL(generic_guard);
  SPECIAL(receptionist);
  SPECIAL(cryogenicist);
  SPECIAL(teacher);
  SPECIAL(metamagic_teacher);
  SPECIAL(trainer);
  SPECIAL(adept_trainer);
  SPECIAL(spell_trainer);
//  SPECIAL(puff);
  SPECIAL(janitor);
  SPECIAL(snake);
  SPECIAL(thief);
  SPECIAL(pike);
  SPECIAL(jeff);
  SPECIAL(captain);
  SPECIAL(rodgers);
  SPECIAL(delaney);
  SPECIAL(lone_star_park);
  SPECIAL(mugger_park);
  SPECIAL(gate_guard_park);
  SPECIAL(squirrel_park);
  SPECIAL(sick_ork);
//  SPECIAL(mage_messenger);
//  SPECIAL(malinalxochi);
  SPECIAL(adept_guard);
  SPECIAL(takehero_tsuyama);
  SPECIAL(bio_secretary);
  SPECIAL(pool);
  SPECIAL(harlten);
  SPECIAL(branson);
  SPECIAL(bio_guard);
  SPECIAL(worker);
//  SPECIAL(dwarf);
//  SPECIAL(elf);
//  SPECIAL(george);
//  SPECIAL(wendigo);
//  SPECIAL(pimp);
//  SPECIAL(prostitute);
//  SPECIAL(heinrich);
//  SPECIAL(ignaz);
//  SPECIAL(waitress);
//  SPECIAL(dracula);
//  SPECIAL(tunnel_rat);
  SPECIAL(saeder_guard);
  SPECIAL(fixer);
  SPECIAL(hacker);
  SPECIAL(fence);
  SPECIAL(taxi);
  SPECIAL(crime_mall_guard);
  SPECIAL(doctor_scriptshaw);
  SPECIAL(huge_troll);
//  SPECIAL(roots_receptionist);
  SPECIAL(aegnor);
  SPECIAL(purple_haze_bartender);
  SPECIAL(yukiya_dahoto);
  SPECIAL(smelly);
  SPECIAL(terell_davis);
  SPECIAL(smiths_bouncer);
  SPECIAL(bouncer_gentle);
  SPECIAL(matchsticks);
  SPECIAL(painter);
  SPECIAL(multnomah_guard);
  SPECIAL(nerp_skills_teacher);
  SPECIAL(orkish_truckdriver);
  SPECIAL(Janis_Meet);
  SPECIAL(Janis_Amer_Door);
  SPECIAL(Janis_Amer_Girl);
  SPECIAL(Janis_Amer_Jerry);
  SPECIAL(Janis_Captive);
  SPECIAL(mageskill_anatoly);
  SPECIAL(mageskill_herbie);
  SPECIAL(mageskill_trainer);
  SPECIAL(mageskill_nightwing);
  SPECIAL(bouncer_troll);
  SPECIAL(knightcenter_bouncer);
  SPECIAL(cybered_yakuza);
  SPECIAL(airport_guard);

  /* trainers */
  for (i = 0; trainers[i].vnum != 0; i++)
    ASSIGNMOB(trainers[i].vnum, trainer);

  /* teachers */
  for (i = 0; teachers[i].vnum != 0; i++)
    ASSIGNMOB(teachers[i].vnum, teacher);
  
  ASSIGNMOB(60541, nerp_skills_teacher);

  /* metamagic trainers */
  for (i = 0; metamagict[i].vnum != 0; i++)
    ASSIGNMOB(metamagict[i].vnum, metamagic_teacher);

  /* adept trainer */
  for (i = 0; adepts[i].vnum != 0; i++)
    ASSIGNMOB(adepts[i].vnum, adept_trainer);

  for (i = 0; spelltrainers[i].teacher; i++)
    if (mob_index[real_mobile(spelltrainers[i].teacher)].func != spell_trainer)
      ASSIGNMOB(spelltrainers[i].teacher, spell_trainer);

  /* cab drivers */
  ASSIGNMOB(600, taxi);
  ASSIGNMOB(650, taxi);

  /* Immortal HQ */
  ASSIGNMOB(1002, janitor);
  ASSIGNMOB(1005, postmaster);

  ASSIGNMOB(1151, terell_davis);
  /* SWU */
  ASSIGNMOB(60526, postmaster);

  /* Various Tacoma */
  ASSIGNMOB(1823, aegnor);
  ASSIGNMOB(1832, fixer);
  ASSIGNMOB(1833, purple_haze_bartender);
  ASSIGNMOB(1900, postmaster);
  ASSIGNMOB(1902, receptionist);
  ASSIGNMOB(1916, generic_guard);
  
  /* Tacoma */
  ASSIGNMOB(2013, generic_guard);
  ASSIGNMOB(2020, hacker);
  ASSIGNMOB(2022, janitor);
  ASSIGNMOB(2023, fence);

  /* club penumbra */
  ASSIGNMOB(2112, bouncer_troll);
  ASSIGNMOB(2115, receptionist);

  /* Puyallup */
  ASSIGNMOB(2306, pike);
  ASSIGNMOB(2305, jeff);

  /*Lone Star 17th Precinct*/
  ASSIGNMOB(3700, captain);
  ASSIGNMOB(3708, rodgers);
  ASSIGNMOB(3709, delaney);
  ASSIGNMOB(3703, generic_guard);
  ASSIGNMOB(3720, generic_guard);
  ASSIGNMOB(3829, janitor);
  ASSIGNMOB(3712, janitor);

  /*Seattle State Park*/
  ASSIGNMOB(4001, lone_star_park);
  ASSIGNMOB(4003, mugger_park);
  ASSIGNMOB(4008, janitor);
  ASSIGNMOB(4009, gate_guard_park);
  ASSIGNMOB(4012, squirrel_park);
  ASSIGNMOB(4013, sick_ork);
  ASSIGNMOB(4016, adept_guard);
  ASSIGNMOB(4019, takehero_tsuyama);

  /* Various - zone 116 */
  ASSIGNMOB(4100, postmaster);

  /*BioHyde Complex*/
  ASSIGNMOB(4202, bio_secretary);
  ASSIGNMOB(4206, harlten);
  ASSIGNMOB(4209, branson);
  ASSIGNMOB(4200, bio_guard);
  ASSIGNMOB(4203, worker);

  /* Tarislar */
  ASSIGNMOB(4912, saeder_guard);

  /* Seattle */
  ASSIGNMOB(5100, generic_guard);
  ASSIGNMOB(5101, janitor);

  /* Neophytic Guild */
  ASSIGNMOB(8010, postmaster);
  ASSIGNMOB(60523, receptionist);

  /* Council Island */
  ASSIGNMOB(9202, doctor_scriptshaw);
  ASSIGNMOB(9410, huge_troll);

  /* Ork Underground */
  ASSIGNMOB(9913, receptionist);

  /* Crime Mall */
  ASSIGNMOB(100022, crime_mall_guard);

  /* Bradenton */
  ASSIGNMOB(14515, hacker);
  ASSIGNMOB(14516, fence);

  /* Portland */
  ASSIGNMOB(14605, receptionist);
  ASSIGNMOB(1102, postmaster);

  /* Mitsuhama */
  ASSIGNMOB(17112, yukiya_dahoto);

  /* Seattle 2.0 */
  ASSIGNMOB(30500, receptionist);

  /* Telestrian */
  ASSIGNMOB(18902, smelly);
  ASSIGNMOB(18910, receptionist);

  /* Everett */
  ASSIGNMOB(39247, cybered_yakuza);
  ASSIGNMOB(39209, receptionist);

  /* Smiths Pub */
  ASSIGNMOB(38027, smiths_bouncer);
  ASSIGNMOB(31701, matchsticks);
  ASSIGNMOB(5029, orkish_truckdriver);
  ASSIGNMOB(30701, receptionist);
  ASSIGNMOB(5031, Janis_Amer_Door);
  ASSIGNMOB(5032, Janis_Amer_Jerry);
  ASSIGNMOB(5033, Janis_Amer_Girl);
  ASSIGNMOB(5034, Janis_Meet);
  ASSIGNMOB(5030, Janis_Captive);
  ASSIGNMOB(37507, painter);
  ASSIGNMOB(17510, multnomah_guard);
  ASSIGNMOB(17103, mageskill_herbie);
  ASSIGNMOB(65121, mageskill_anatoly);
  ASSIGNMOB(777, mageskill_trainer);
  ASSIGNMOB(2430, mageskill_nightwing);
  ASSIGNMOB(31704, bouncer_troll);
  ASSIGNMOB(32300, bouncer_troll);
  ASSIGNMOB(3122, bouncer_troll);
  ASSIGNMOB(5000, bouncer_troll);
  ASSIGNMOB(35500, bouncer_troll);
  ASSIGNMOB(35505, bouncer_troll);
  ASSIGNMOB(700000, bouncer_troll);
  ASSIGNMOB(70400, bouncer_troll);
  ASSIGNMOB(705005, bouncer_troll);
  ASSIGNMOB(15824, bouncer_troll);
  ASSIGNMOB(71000, bouncer_troll);
  ASSIGNMOB(19888, knightcenter_bouncer);
  ASSIGNMOB(62115, receptionist);
  ASSIGNMOB(62135, airport_guard);
  ASSIGNMOB(62100, airport_guard);
  
  /* Star Sapphire */
  ASSIGNMOB(70302, bouncer_gentle);
}

/* assign special procedures to objects */

void assign_objects(void)
{
  SPECIAL(bank);
  SPECIAL(gen_board);
  SPECIAL(vendtix);
  SPECIAL(clock);
  SPECIAL(vending_machine);
  SPECIAL(hand_held_scanner);
  SPECIAL(anticoagulant);
  SPECIAL(toggled_invis);
  SPECIAL(desktop);
  SPECIAL(portable_gridguide);
  SPECIAL(pocket_sec);
  SPECIAL(locker);
  SPECIAL(chargen_hopper);
  SPECIAL(quest_debug_scanner);
  SPECIAL(taxi_sign);
  SPECIAL(weapon_dominator);
  SPECIAL(trideo);
  SPECIAL(spraypaint);
  SPECIAL(restoration_button);

  ASSIGNOBJ(3, gen_board);  /* Rift's Board */
  ASSIGNOBJ(4, gen_board);  /* Pook's Board */
  ASSIGNOBJ(12, gen_board);  /* Dunk's Board */
  ASSIGNOBJ(22, gen_board);   /* old Changelogs */
  ASSIGNOBJ(26, gen_board);  /* RP Board */
  ASSIGNOBJ(28, gen_board);  /* Quest Board */
  ASSIGNOBJ(31, gen_board);  /* War Room Board */
  ASSIGNOBJ(50, gen_board);  /* Harl's Board */
  ASSIGNOBJ(66, gen_board);             /* Lofwyr's Board */
  ASSIGNOBJ(1006, gen_board);  /* Builder Board  */
  ASSIGNOBJ(1038, gen_board);  /* Coders Board */
  ASSIGNOBJ(1074, gen_board);  /* Administration Board */
  ASSIGNOBJ(10018, gen_board);  /* ImmHQ Board */
#ifdef USE_PRIVATE_CE_WORLD
  ASSIGNOBJ(10034, gen_board); /* Mortal Relations Board */
#endif
  ASSIGNOBJ(65126, gen_board);
  ASSIGNOBJ(26150, gen_board);
  ASSIGNOBJ(11482, gen_board);
  ASSIGNOBJ(70405, gen_board);
  ASSIGNOBJ(1117, desktop);
  ASSIGNOBJ(1118, desktop);
  ASSIGNOBJ(1119, desktop);
  ASSIGNOBJ(1120, desktop);
  ASSIGNOBJ(1121, desktop);
  ASSIGNOBJ(1122, desktop);
  ASSIGNOBJ(1123, desktop);
  ASSIGNOBJ(1124, desktop);
  ASSIGNOBJ(1125, desktop);
  ASSIGNOBJ(1126, desktop);
  ASSIGNOBJ(1845, desktop);
  ASSIGNOBJ(1846, desktop);
  ASSIGNOBJ(1847, desktop);
  ASSIGNOBJ(1848, desktop);
  ASSIGNOBJ(1904, clock);  // clock
  ASSIGNOBJ(1946, bank);  // atm
  ASSIGNOBJ(2104, gen_board);     // mortal board
  ASSIGNOBJ(2106, gen_board);      // immortal board
  ASSIGNOBJ(2107, gen_board);      // freeze board
  ASSIGNOBJ(2108, bank);         // atm
  ASSIGNOBJ(2506, anticoagulant);
  ASSIGNOBJ(3012, vendtix);
  ASSIGNOBJ(4006, clock);
  ASSIGNOBJ(4216, clock);
  ASSIGNOBJ(8001, gen_board);  // newbie board
  ASSIGNOBJ(8002, gen_board);  // newbie board
  ASSIGNOBJ(8003, gen_board);  // newbie board
  ASSIGNOBJ(8004, gen_board);  // newbie board
  ASSIGNOBJ(8013, gen_board);  // newbie board
  ASSIGNOBJ(9329, vending_machine);
  ASSIGNOBJ(10011, desktop);
  ASSIGNOBJ(10108, vending_machine);
  ASSIGNOBJ(42130, anticoagulant);
  ASSIGNOBJ(10025, pocket_sec);
  ASSIGNOBJ(9406, hand_held_scanner);
  ASSIGNOBJ(29997, gen_board);  // Loki's Board
  ASSIGNOBJ(22445, bank);
  ASSIGNOBJ(14645, clock);
  ASSIGNOBJ(16234, clock);
  ASSIGNOBJ(18884, clock);
  ASSIGNOBJ(18950, bank);
  ASSIGNOBJ(14726, bank);
  ASSIGNOBJ(64900, gen_board); // RPE Board
  ASSIGNOBJ(65207, gen_board); // Backstage Group's Board
  ASSIGNOBJ(4603, gen_board);
  ASSIGNOBJ(50301, toggled_invis);
  ASSIGNOBJ(50305, desktop);
  ASSIGNOBJ(8458, desktop);
  ASSIGNOBJ(8459, desktop);
  ASSIGNOBJ(65214, desktop);
  ASSIGNOBJ(4607, desktop);
  ASSIGNOBJ(35026, clock);
  ASSIGNOBJ(42118, clock);
  ASSIGNOBJ(18825, portable_gridguide);
  ASSIGNOBJ(39864, portable_gridguide);
  ASSIGNOBJ(671, portable_gridguide);
  ASSIGNOBJ(31542, pocket_sec);
  ASSIGNOBJ(39865, pocket_sec);
  ASSIGNOBJ(31701, trideo);
  ASSIGNOBJ(14859, trideo);
  ASSIGNOBJ(8498, trideo);
  ASSIGNOBJ(35532, trideo);
  ASSIGNOBJ(31701, trideo);
  ASSIGNOBJ(61536, spraypaint);
  ASSIGNOBJ(39392, bank);
  ASSIGNOBJ(62129, bank);
  ASSIGNOBJ(62372, bank);
  ASSIGNOBJ(25562, bank);
  ASSIGNOBJ(25551, clock);
  ASSIGNOBJ(25556, clock);
  ASSIGNOBJ(9826, locker);
  ASSIGNOBJ(60500, chargen_hopper);
  ASSIGNOBJ(10033, quest_debug_scanner);
  #ifdef USE_PRIVATE_CE_WORLD
  ASSIGNOBJ(70605, desktop);
  ASSIGNOBJ(10036, restoration_button);
  #endif
  
  ASSIGNOBJ(OBJ_SEATTLE_TAXI_SIGN, taxi_sign);
  ASSIGNOBJ(OBJ_PORTLAND_TAXI_SIGN, taxi_sign);
  
  ASSIGNOBJ(10005, weapon_dominator);
  
  WSPEC(monowhip);

  ASSIGNWEAPON(660, monowhip);
}

/* assign special procedures to rooms */

void assign_rooms(void)
{
  SPECIAL(car_dealer);
  SPECIAL(oceansounds);
  SPECIAL(escalator);
  SPECIAL(neophyte_entrance);
  SPECIAL(simulate_bar_fight);
  SPECIAL(crime_mall_blockade);
  SPECIAL(waterfall);
  /*SPECIAL(climb_down_junk_pile);
  SPECIAL(climb_up_junk_pile);
  SPECIAL(junk_pile_fridge); */
  SPECIAL(slave);
  SPECIAL(circulation_fan);
  SPECIAL(traffic);
  SPECIAL(newbie_car);
  SPECIAL(auth_room);
  SPECIAL(room_damage_radiation);
  SPECIAL(bouncy_castle);
  SPECIAL(rpe_room);
  SPECIAL(multnomah_gate);
  SPECIAL(mageskill_hermes);
  SPECIAL(mageskill_moore);
  SPECIAL(newbie_housing);
  SPECIAL(sombrero_bridge);
  SPECIAL(sombrero_infinity);
  SPECIAL(airport_gate);
  SPECIAL(chargen_south_from_trainer);
  SPECIAL(chargen_skill_annex);
  SPECIAL(chargen_unpractice_skill);
  SPECIAL(floor_has_glass_shards);
  SPECIAL(chargen_career_archetype_paths);
  SPECIAL(chargen_spirit_combat_west);

  /* Limbo/God Rooms */
  ASSIGNROOM(8, oceansounds);
  ASSIGNROOM(9, oceansounds);
  //ASSIGNROOM(5, room_damage_radiation);
  ASSIGNROOM(1000, sombrero_bridge);
  ASSIGNROOM(1001, sombrero_infinity);
  
  /* Carbanado */
  ASSIGNROOM(4477, waterfall);

  /* CharGen */
  ASSIGNROOM(60505, chargen_south_from_trainer);
  ASSIGNROOM(60506, chargen_skill_annex);
  ASSIGNROOM(60562, auth_room);
  ASSIGNROOM(60514, chargen_career_archetype_paths);
  ASSIGNROOM(60594, chargen_spirit_combat_west);
  /* CharGen - allow forgetting skills */
  ASSIGNROOM(60507, chargen_unpractice_skill);
  ASSIGNROOM(60508, chargen_unpractice_skill);
  ASSIGNROOM(60509, chargen_unpractice_skill);
  ASSIGNROOM(60513, chargen_unpractice_skill);
  ASSIGNROOM(60512, chargen_unpractice_skill);
  ASSIGNROOM(60511, chargen_unpractice_skill);
  ASSIGNROOM(60510, chargen_unpractice_skill);
  ASSIGNROOM(60591, chargen_unpractice_skill);
  ASSIGNROOM(60592, chargen_unpractice_skill);
  ASSIGNROOM(60593, chargen_unpractice_skill);
  ASSIGNROOM(60626, chargen_unpractice_skill);
  ASSIGNROOM(60627, chargen_unpractice_skill);
  ASSIGNROOM(60628, chargen_unpractice_skill);
  
  /* Neophyte Guild */
  ASSIGNROOM(60585, neophyte_entrance);
  ASSIGNROOM(60586, newbie_car);
  ASSIGNROOM(60548, newbie_housing); 

  /* Ork Underground */
  ASSIGNROOM(9978, simulate_bar_fight);

  /* Tacoma Mall */
  ASSIGNROOM(1904, escalator);
  ASSIGNROOM(1920, escalator);
  ASSIGNROOM(1923, escalator);
  ASSIGNROOM(1937, escalator);

  /* Crime Mall */
  ASSIGNROOM(100075, crime_mall_blockade);
  ASSIGNROOM(100077, crime_mall_blockade);


  ASSIGNROOM(1399, car_dealer);
  ASSIGNROOM(10021, car_dealer);
  ASSIGNROOM(14798, car_dealer);
  ASSIGNROOM(14796, car_dealer);
  ASSIGNROOM(37505, car_dealer);
  ASSIGNROOM(37507, car_dealer);
  ASSIGNROOM(37509, car_dealer);
  ASSIGNROOM(37511, car_dealer);
  ASSIGNROOM(37513, car_dealer);
  ASSIGNROOM(12301, car_dealer);
  ASSIGNROOM(12311, car_dealer);
  ASSIGNROOM(12313, car_dealer);
  ASSIGNROOM(39297, car_dealer);  
  ASSIGNROOM(39299, car_dealer);  
  ASSIGNROOM(62501, car_dealer);
  ASSIGNROOM(62201, car_dealer);
  ASSIGNROOM(62331, car_dealer);

  /* Mitsuhama */
  ASSIGNROOM(17171, circulation_fan);

  /* Portland */
  ASSIGNROOM(18860, escalator);
  ASSIGNROOM(18859, escalator);

  /* Kuroda's Bouncy Castle */
  ASSIGNROOM(65075, bouncy_castle);
  ASSIGNROOM(35611, rpe_room);

  ASSIGNROOM(17598, multnomah_gate);
  ASSIGNROOM(17599, multnomah_gate);
  
  ASSIGNROOM(24810, mageskill_hermes);
  ASSIGNROOM(35610, mageskill_moore);
  ASSIGNROOM(62166, airport_gate);
  ASSIGNROOM(62173, airport_gate);
  
  // Junkyard
  ASSIGNROOM(70504, floor_has_glass_shards);

  log("Assigning traffic messages...");
  for (long x = 0; x <= top_of_world; x++)
    if (ROOM_FLAGGED(&world[x], ROOM_ROAD) 
        && !ROOM_FLAGGED(&world[x], ROOM_NO_TRAFFIC) 
        && !ROOM_FLAGGED(&world[x], ROOM_GARAGE) 
        && !world[x].func 
        && SECT(&world[x]) == SPIRIT_CITY)
      ASSIGNROOM(world[x].number, traffic);
}
