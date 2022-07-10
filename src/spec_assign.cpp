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

#include "structs.hpp"
#include "awake.hpp"
#include "db.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "archetypes.hpp"

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
                       { 24806, { META_MASKING, 0, 0, 0, 0, 0, 0, 0 }, "", 0 },
                         { 4250, { META_REFLECTING, 0, 0, 0, 0, 0, 0, 0 }, "", 0 },
                         { 4251, { META_REFLECTING, 0, 0, 0, 0, 0, 0, 0 }, "", 0 },
                         { 10010, { META_ANCHORING, META_CENTERING, META_CLEANSING, META_INVOKING, META_MASKING, META_POSSESSING, META_QUICKENING, META_REFLECTING, META_SHIELDING }, "", 0},
                         { 35538, { META_MASKING, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "", 0},
                         { 27426, { META_CLEANSING, 0, 0, 0, 0, 0, 0, 0 }, "", 0},
#ifdef USE_PRIVATE_CE_WORLD
                         { 94313, { META_ANCHORING, META_CENTERING, META_INVOKING, META_POSSESSING, META_QUICKENING, META_SHIELDING, 0, 0 }, "", 0}, // NERP metamagic trainer
#endif
                         { 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "", 0}
                       };

struct teach_data teachers[] = {
                      // Begin NERP teachers.
                       { 1001, { SKILL_BR_EDGED, SKILL_BR_POLEARM, SKILL_BR_CLUB, SKILL_BR_THROWINGWEAPONS, SKILL_BR_WHIPS,
                         SKILL_BR_PROJECTILES, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After about an hour of shuffling slides you feel you can now do "
                         "your job safer.\r\n", ADVANCED },
                       /*{ 1002, { SKILL_BR_PISTOL, SKILL_BR_SHOTGUN, SKILL_BR_RIFLE, SKILL_BR_HEAVYWEAPON, SKILL_BR_SMG,
                         SKILL_BR_ARMOR, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After about an hour of shuffling slides you feel you can now do your job "
                         "safer.\r\n", AMATEUR },  */
                       { 1003, { SKILL_SINGING, SKILL_CHANTING, SKILL_DANCING, SKILL_INSTRUMENT, SKILL_ACTING, SKILL_MEDITATION, SKILL_ARCANELANGUAGE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "After about an hour of shuffling slides you feel you can now do your job safer.\r\n", ADVANCED },
                       { 1004, { SKILL_OFFHAND_EDGED, SKILL_OFFHAND_CLUB, SKILL_OFFHAND_CYBERIMPLANTS, SKILL_OFFHAND_WHIP,
                         SKILL_SPRAY_WEAPONS, SKILL_GUNCANE, SKILL_BRACERGUN, SKILL_BLOWGUN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After about an hour of "
                         "shuffling slides you feel you can now do your job safer.\r\n", ADVANCED },
                       { 1005, { SKILL_NAVI_LAND, SKILL_NAVI_AIR, SKILL_NAVI_WATER,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After about an hour of shuffling slides you feel you can now do "
                         "your job safer.\r\n", ADVANCED },
                       { 1006, { SKILL_INTIMIDATION, SKILL_INTERROGATION, SKILL_LEADERSHIP, SKILL_SURVIVAL, SKILL_LEGERDEMAIN,
                         SKILL_TRACK, SKILL_DISGUISE, SKILL_SMALL_UNIT_TACTICS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After about an hour of shuffling slides you "
                         "feel you can now do your job safer.\r\n", ADVANCED },
                       { 1007, { SKILL_ANIMAL_HANDLING, SKILL_ANIMAL_TAMING, SKILL_CHEMISTRY, SKILL_PHARMA,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After about an hour of shuffling slides you feel you can now do your job safer.\r\n", ADVANCED },
                       { 1008, { SKILL_DEMOLITIONS, SKILL_DIVING, SKILL_PARACHUTING, SKILL_UNDERWATER_COMBAT, SKILL_LOCK_PICKING,
                         SKILL_RIDING, SKILL_THROWING_WEAPONS, SKILL_PROJECTILES, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After about an hour of shuffling slides you feel you can now do your job "
                         "safer.\r\n", ADVANCED },
                       { 1009, { SKILL_CENTERING, SKILL_ENCHANTING, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After about an hour of shuffling slides you feel you can now do your job "
                         "safer.\r\n", ADVANCED },
                      // End NERP teachers.

#ifdef USE_PRIVATE_CE_WORLD
                       { 780, { SKILL_ASSAULT_RIFLES, SKILL_MACHINE_GUNS, SKILL_PISTOLS, SKILL_RIFLES, SKILL_SHOTGUNS,
                         SKILL_SMG, SKILL_ASSAULT_CANNON, SKILL_BR_PISTOL, SKILL_BR_SHOTGUN, SKILL_BR_RIFLE,
                         SKILL_BR_HEAVYWEAPON, SKILL_BR_SMG, SKILL_TASERS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "Ma Deuce puts you through firearm drills that make you piss blood when you're done.\r\n", ADVANCED },

  { 782, { SKILL_CLUBS, SKILL_CYBER_IMPLANTS, SKILL_EDGED_WEAPONS, SKILL_POLE_ARMS, SKILL_UNARMED_COMBAT,
    SKILL_WHIPS_FLAILS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "You polish and hone your melee skills and learn to flow like water.\r\n", ADVANCED },
  { 784, { SKILL_PILOT_BIKE, SKILL_PILOT_CAR, SKILL_PILOT_TRUCK, SKILL_BR_BIKE,
           SKILL_BR_DRONE, SKILL_BR_CAR, SKILL_BR_TRUCK, SKILL_GUNNERY,
#ifdef WE_HAVE_VEHICLE_QUEST
           SKILL_PILOT_ROTORCRAFT, SKILL_PILOT_FIXEDWING, SKILL_PILOT_VECTORTHRUST,
           SKILL_BR_FIXEDWING, SKILL_BR_ROTORCRAFT, SKILL_BR_VECTORTHRUST, SKILL_BR_HOVERCRAFT,
           SKILL_BR_MOTORBOAT, SKILL_BR_SHIP, SKILL_BR_LTA, SKILL_PILOT_HOVERCRAFT,
           SKILL_PILOT_MOTORBOAT, SKILL_PILOT_SHIP, SKILL_PILOT_LTA, 0, 0, 0 },
#else
           0, 0, 0,
           0, 0, 0, 0,
           0, 0, 0, 0,
           0, 0, 0, 0, 0, 0 },
#endif
    "You pick up a few new tricks of the trade and emerge more skilled than before.\r\n", ADVANCED },
  { 786, { SKILL_COMPUTER, SKILL_BR_COMPUTER, SKILL_DATA_BROKERAGE, SKILL_CYBERTERM_DESIGN,
    SKILL_ELECTRONICS, SKILL_BR_ELECTRONICS, SKILL_PROGRAM_COMBAT, SKILL_PROGRAM_CYBERTERM,
    SKILL_PROGRAM_DEFENSIVE, SKILL_PROGRAM_OPERATIONAL, SKILL_PROGRAM_SPECIAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "You spend hours training yourself in various electronic protocols.\r\n", ADVANCED },
  { 788, { SKILL_CORPORATE_ETIQUETTE, SKILL_ELF_ETIQUETTE, SKILL_MEDIA_ETIQUETTE, SKILL_NEGOTIATION,
    SKILL_STREET_ETIQUETTE, SKILL_TRIBAL_ETIQUETTE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "You spend a long time learning how to make small talk and directing conversations.\r\n", ADVANCED },
  { 790, { SKILL_ATHLETICS, SKILL_BIOTECH, SKILL_POLICE_PROCEDURES, SKILL_STEALTH, SKILL_MEDICINE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After hours of study and physical practice, you feel like you've learned\r\nsomething.\r\n.", ADVANCED },
#endif

                       { 2508, { SKILL_BIOTECH, SKILL_MEDICINE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After hours of medical research and instruction, you begin "
                         "to\r\nunderstand more of the basic biotech procedures.\r\n", AMATEUR },
                       { 2701, { SKILL_ATHLETICS, SKILL_STEALTH, SKILL_UNARMED_COMBAT, SKILL_EDGED_WEAPONS,
                         SKILL_WHIPS_FLAILS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "Toh Li gives you the workout of your life, but you come out more learned.", AMATEUR },
                       { 3722, { SKILL_ATHLETICS, SKILL_RIFLES, SKILL_PISTOLS, SKILL_POLICE_PROCEDURES, SKILL_TASERS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "After hours of study and physical practice, you feel like you've learned\r\nsomething.\r\n", AMATEUR },
                       { 4101, { SKILL_SHOTGUNS, SKILL_PISTOLS, SKILL_RIFLES, SKILL_SMG, SKILL_ASSAULT_RIFLES, SKILL_TASERS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "After hours of study and target practice, you feel like you've learned\r\nsomething.\r\n", AMATEUR },
                       { 4102, { SKILL_CLUBS, SKILL_EDGED_WEAPONS, SKILL_POLE_ARMS, SKILL_WHIPS_FLAILS,
                         SKILL_PILOT_CAR, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "After hours of study and practice, you feel like you've "
                         "learned\r\nsomething.\r\n", AMATEUR },
                       { 4250, { SKILL_SORCERY, SKILL_CONJURING, SKILL_AURA_READING, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "After hours of study and magical practice, you feel like you've learned\r\nsomething.\r\n", AMATEUR },
                       { 4251, { SKILL_CONJURING, SKILL_SORCERY, SKILL_AURA_READING, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                         "After hours of study and magical practice, you feel like you've learned\r\nsomething.\r\n", AMATEUR },


// These trainers aren't in the game.
/*
  { 4103, { SKILL_SHOTGUNS, SKILL_PISTOLS, SKILL_RIFLES, SKILL_SMG, SKILL_ASSAULT_RIFLES, SKILL_MACHINE_GUNS, SKILL_ASSAULT_CANNON, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After hours of study and target practice, you feel like you've learned\r\nsomething.\r\n", AMATEUR },
  { 4104, { SKILL_CLUBS, SKILL_EDGED_WEAPONS, SKILL_POLE_ARMS, SKILL_WHIPS_FLAILS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After hours of study and melee practice, you feel like you've learned\r\nsomething.\r\n", AMATEUR },
  { 4257, { SKILL_ASSAULT_RIFLES, SKILL_TASERS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After hours of study and weapon practice, you feel like you've learned\r\nsomething.\r\n", AMATEUR },
*/

  /* Removed nerp skills from trainers:
   SKILL_ENCHANTING, SKILL_AURA_READING, SKILL_CENTERING, SKILL_TALISMONGERING, SKILL_PROJECTILES, SKILL_THROWING_WEAPONS SKILL_ORALSTRIKE, SKILL_MEDITATION
   */

  // Imm trainer (Dumbledore)
  { 10010, { SKILL_CONJURING, SKILL_SORCERY, SKILL_SPELLDESIGN,
    /*, SKILL_GETTING_THROWN_OFF_OF_A_TOWER, */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After what feels like seven years of magical study, you feel like you've learned something.\r\n", GODLY },

  // Newbie magic teacher
  { 60501, { SKILL_CONJURING, SKILL_SORCERY, SKILL_SPELLDESIGN, SKILL_AURA_READING,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After hours of study and practice, you feel like you've learned something.\r\n", NEWBIE },

  { 60502, { SKILL_COMPUTER, SKILL_BR_COMPUTER, SKILL_DATA_BROKERAGE, SKILL_CYBERTERM_DESIGN, SKILL_ELECTRONICS,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After hours of study and practice, you feel like you've learned something.\r\n", NEWBIE },

  { 60503, { SKILL_ARABIC, SKILL_CHINESE, SKILL_CROW, SKILL_ENGLISH, SKILL_FRENCH, SKILL_GAELIC, SKILL_GERMAN,
    SKILL_ITALIAN, SKILL_JAPANESE, SKILL_KOREAN, SKILL_LATIN, SKILL_MAKAW, SKILL_NAVAJO, SKILL_ORZET,
    SKILL_RUSSIAN, SKILL_SALISH, SKILL_SIOUX, SKILL_SPANISH, SKILL_SPERETHIEL, SKILL_UTE, SKILL_HEBREW, SKILL_IROQUOIS, 0, 0, 0 },
    "Von Richter runs through basic conjugation and sentence structure with you.\r\n", NEWBIE },

  { 60504, { SKILL_PILOT_BIKE, SKILL_PILOT_CAR, SKILL_PILOT_TRUCK, SKILL_BR_BIKE,
    SKILL_BR_DRONE, SKILL_BR_CAR, SKILL_BR_TRUCK, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After hours of study and practice, you feel like you've learned something.\r\n", NEWBIE },

  { 60505, { SKILL_CLUBS, SKILL_CYBER_IMPLANTS, SKILL_EDGED_WEAPONS, SKILL_POLE_ARMS, SKILL_UNARMED_COMBAT, SKILL_WHIPS_FLAILS, // Jet Li
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After hours of study and practice, you feel like you've learned something.\r\n", NEWBIE },

  /*
  { 60539, { SKILL_PROJECTILES, SKILL_THROWING_WEAPONS, // Kyle the Mall Ninja
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "Kyle teaches you things you're pretty sure were more effective in the late 1800s.\r\n", NEWBIE },
  */

  { 60540, { SKILL_BR_PISTOL, SKILL_BR_SHOTGUN, SKILL_BR_RIFLE, SKILL_BR_HEAVYWEAPON, SKILL_BR_SMG, // Gary the Gunsmith
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "Gary walks you through the intricacies of handloading.\r\n", NEWBIE },

  { 60506, { SKILL_ASSAULT_RIFLES, SKILL_GUNNERY, SKILL_MACHINE_GUNS, SKILL_PISTOLS, SKILL_RIFLES, SKILL_SHOTGUNS, SKILL_SMG, // Mick
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "After hours of study and practice, you feel like you've learned something.\r\n", NEWBIE },

  { 60532, { SKILL_PROGRAM_COMBAT, SKILL_PROGRAM_CYBERTERM, SKILL_PROGRAM_DEFENSIVE, SKILL_PROGRAM_OPERATIONAL, SKILL_PROGRAM_SPECIAL,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "Deadlock picks up some design plans from his desk and runs through the basic process of setting one up, you feel like you've learned something.\r\n", NEWBIE},

  { 60533, { SKILL_CORPORATE_ETIQUETTE, SKILL_ELF_ETIQUETTE, SKILL_MEDIA_ETIQUETTE, SKILL_NEGOTIATION, SKILL_STREET_ETIQUETTE, SKILL_TRIBAL_ETIQUETTE,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "Lucy runs through the proper decorum to use in certain situations, you feel like you've learned something.\r\n", NEWBIE},

  { 60534, { SKILL_ATHLETICS, SKILL_BIOTECH, SKILL_POLICE_PROCEDURES, SKILL_STEALTH,
    SKILL_MEDICINE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "Steve roughly throws you a text book and tells you to read it, you feel like you've learned something.\r\n", NEWBIE},
  // End newbie teachers.

   { 30700, { SKILL_ARABIC, SKILL_CHINESE, SKILL_CROW, SKILL_ENGLISH, SKILL_FRENCH, SKILL_GAELIC, SKILL_GERMAN,
     SKILL_ITALIAN, SKILL_JAPANESE, SKILL_KOREAN, SKILL_LATIN, SKILL_MAKAW, SKILL_NAVAJO, SKILL_ORZET,
     SKILL_RUSSIAN, SKILL_SALISH, SKILL_SIOUX, SKILL_SPANISH, SKILL_SPERETHIEL, SKILL_UTE, SKILL_HEBREW, SKILL_IROQUOIS, 0, 0, 0 }, "Socrates shows you the intricities "
       "of the language and you emerge with a greater understanding.\r\n", ADVANCED },

   { 3125, { SKILL_COMPUTER, SKILL_ELECTRONICS, SKILL_BR_COMPUTER, SKILL_BR_ELECTRONICS, SKILL_PROGRAM_COMBAT, SKILL_PROGRAM_DEFENSIVE, SKILL_PROGRAM_CYBERTERM, SKILL_CYBERTERM_DESIGN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Brian explains some concepts you had yet to understand "
     "and\r\nyou feel like you've learned something.\r\n", AMATEUR },

   { 14608, { SKILL_PILOT_CAR, SKILL_MACHINE_GUNS, SKILL_ASSAULT_CANNON, SKILL_PILOT_BIKE, SKILL_PILOT_TRUCK, SKILL_GUNNERY, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
     "Hal shows you a trick or two, and the rest just falls into place.\r\n", AMATEUR },

   { 14638, { SKILL_COMPUTER, SKILL_ELECTRONICS, SKILL_BR_COMPUTER, SKILL_BR_DRONE, SKILL_PROGRAM_SPECIAL, SKILL_PROGRAM_OPERATIONAL, SKILL_DATA_BROKERAGE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Gargamel looks at you, hands you some parts, drops you a clue, and you get it.\r\n", AMATEUR},

   { 7223, { SKILL_UNARMED_COMBAT, SKILL_STEALTH, SKILL_ATHLETICS, SKILL_CYBER_IMPLANTS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Shing gives you a work out like none you've ever had, "
     "but\r\nyou feel like you've learned something.\r\n", AMATEUR },

   { 24806, { SKILL_SORCERY, SKILL_CONJURING, SKILL_NEGOTIATION, SKILL_AURA_READING, SKILL_SPELLDESIGN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Hermes imparts his wisdom upon you.\r\n", AMATEUR },

   { 37500, { SKILL_BR_CAR, SKILL_BR_BIKE, SKILL_BR_DRONE, SKILL_BR_TRUCK, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Marty shows you a few tricks of the "
     "trade and you emerge more skilled than before.\r\n", AMATEUR },

  { 35502, { SKILL_STREET_ETIQUETTE,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "You spend some time talking to the people in line.\r\n", AMATEUR },

  { 17111, { SKILL_CORPORATE_ETIQUETTE, SKILL_NEGOTIATION,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    "You spend some time making small talk with the secretary.\r\n", AMATEUR },

#ifdef USE_PRIVATE_CE_WORLD
  // Jessie (SPLAT! Paintballer)
  { 70604, { SKILL_PISTOLS, SKILL_BR_PISTOL, SKILL_STEALTH, SKILL_ATHLETICS,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  "You learn a few things from the surprisingly-knowledgeable kid.\r\n", AMATEUR },
#endif

   { 65106, { SKILL_MEDIA_ETIQUETTE, SKILL_STEALTH, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
     "You talk to the reporter about current events for a while.\r\n", AMATEUR },

   { 18311, { SKILL_STREET_ETIQUETTE, SKILL_MEDIA_ETIQUETTE, SKILL_ELF_ETIQUETTE, SKILL_CORPORATE_ETIQUETTE,
              SKILL_TRIBAL_ETIQUETTE, SKILL_NEGOTIATION, SKILL_POLICE_PROCEDURES, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
     "You read through various lifestyle books for a while.\r\n", LIBRARY },

   { 18312, { SKILL_COMPUTER, SKILL_ELECTRONICS, SKILL_BR_COMPUTER, SKILL_CYBERTERM_DESIGN,
              SKILL_BR_ELECTRONICS, SKILL_DATA_BROKERAGE, SKILL_PROGRAM_CYBERTERM, SKILL_PROGRAM_SPECIAL, SKILL_BIOTECH, SKILL_MEDICINE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
     "You read through various academic journals.\r\n", LIBRARY },

   { 18313, { SKILL_ARABIC, SKILL_CHINESE, SKILL_CROW, SKILL_ENGLISH, SKILL_FRENCH, SKILL_GAELIC, SKILL_GERMAN,
              SKILL_ITALIAN, SKILL_JAPANESE, SKILL_KOREAN, SKILL_LATIN, SKILL_MAKAW, SKILL_NAVAJO, SKILL_ORZET,
              SKILL_RUSSIAN, SKILL_SALISH, SKILL_SIOUX, SKILL_SPANISH, SKILL_SPERETHIEL, SKILL_UTE, SKILL_HEBREW, SKILL_IROQUOIS, 0, 0, 0 },
     "You raid the teach-yourself-languages section.\r\n", LIBRARY },

#ifdef USE_PRIVATE_CE_WORLD
   { 18317, { SKILL_PILOT_BIKE, SKILL_PILOT_CAR, SKILL_PILOT_TRUCK, SKILL_BR_BIKE,
              SKILL_BR_DRONE, SKILL_BR_CAR, SKILL_BR_TRUCK, SKILL_GUNNERY,
              SKILL_PILOT_ROTORCRAFT, SKILL_PILOT_FIXEDWING, SKILL_PILOT_VECTORTHRUST,
              SKILL_BR_FIXEDWING, SKILL_BR_ROTORCRAFT, SKILL_BR_VECTORTHRUST, SKILL_BR_HOVERCRAFT,
              SKILL_BR_MOTORBOAT, SKILL_BR_SHIP, SKILL_BR_LTA, SKILL_PILOT_HOVERCRAFT,
              SKILL_PILOT_MOTORBOAT, SKILL_PILOT_SHIP, SKILL_PILOT_LTA, 0, 0, 0},
     "You read through trade magazines and come away with a better understanding of vehicles.\r\n", LIBRARY },
#endif

   { 778, { SKILL_SORCERY, SKILL_CONJURING, SKILL_AURA_READING, SKILL_SPELLDESIGN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
     "The Master teaches you more than you thought possible.\r\n", ADVANCED },

   { 5914, { SKILL_PISTOLS, SKILL_RIFLES, SKILL_SHOTGUNS, SKILL_SMG, SKILL_ASSAULT_RIFLES,
             SKILL_GUNNERY, SKILL_MACHINE_GUNS, SKILL_BR_RIFLE, SKILL_BR_SMG, SKILL_BR_SHOTGUN,
             SKILL_TASERS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
     "Lucas runs through some theory with you then lets you have a few shots on the range.\r\n", NEWBIE},

   { 65119, { SKILL_RUSSIAN, SKILL_GERMAN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Olga Chernov brutally crams language forms and syntaxes into your skull.\r\n", AMATEUR },

   { 33004, { SKILL_ELF_ETIQUETTE, SKILL_GAELIC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, "Milton Buckthorne drops knowledge on your patheticly underpowered brain.\r\n", AMATEUR },

   { 60235, { SKILL_SIOUX, SKILL_MAKAW, SKILL_CROW, SKILL_UTE, SKILL_SALISH, SKILL_NAVAJO, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
     "Speaks-with-Many goes over his lessons with you until they become second nature.\r\n", AMATEUR },

   { 39226, { SKILL_LATIN, SKILL_ARABIC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "The gentleman demonstrates the finer points of articulation and pronunciation until you feel you've learn something new", AMATEUR },

   { 60540, { SKILL_BR_PISTOL, SKILL_BR_SHOTGUN, SKILL_BR_SMG, SKILL_BR_RIFLE, SKILL_BR_HEAVYWEAPON, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Gary teaches you the finer points of gunsmithing.\r\n", NEWBIE },

   { 22871, { SKILL_BR_PISTOL, SKILL_BR_SHOTGUN, SKILL_BR_SMG, SKILL_BR_RIFLE, SKILL_BR_HEAVYWEAPON, 0, 0, 0, 0, 0, 0, 0, 0 , 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, "You spend some time reading through the library.\r\n", AMATEUR },

   { 62803, { SKILL_PILOT_CAR, SKILL_PILOT_BIKE, SKILL_PISTOLS, SKILL_UNARMED_COMBAT, SKILL_EDGED_WEAPONS, SKILL_STREET_ETIQUETTE, SKILL_STEALTH, SKILL_NEGOTIATION, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, "You spend some time listening to Axehead's stories, and you feel like you've learned something.\r\n", AMATEUR },

  { 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, "Report this bug!\r\n", 0 } // this MUST be last
};

struct adept_data adepts[] = {//0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31  32  33  34  35  36  37  38  39  40  41  42  43  44  45  46  47  48  49  50  51  52  53  54  55  56  57  58  59  60  61  62  63  64  65  66  67  68  69  70  71  72  73  74  75  76  77  79  80  81  82  83  84  85
                     { 10011,{  0,  1, 50,  1,  1, 50,  1,  1,  1, 50, 50, 50, 50, 50, 50, 50, 50,  1,  1,  1, 50, 50, 50, 50, 50,  1,  1, 50, 50, 50, 50, 50,  1, 50,  1,  1,  1,  1, 50, 50, 50, 50, 50, 50,  1, 50,  1,  1,  1,  1,  1, 50,  1, 50,  1,  1, 50,  1, 50,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, 50, 50,  1,  1,  1,  1,  1, 50, 50, 50,  1,  1,  1,  1,  1}, 0 },
                     { 60507,{  0,  1,  3,  1,  1,  4,  1,  1,  1,  3,  6,  6,  6,  6,  6,  6,  3,  1,  1,  1,  3, 10,  3,  0,  3,  0,  0,  6,  6,  6,  0,  0,  1,  3,  0,  1,  0,  1,  3,  6,  0,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, 1 },
                     { 7223, {  0,  1,  3,  0,  1,  4,  1,  0,  0,  3,  0,  6,  0,  0,  6,  0,  6,  0,  0,  0,  0, 10,  0,  0,  6,  0,  0,  2,  0,  6,  0,  0,  0,  0,  0,  0,  0,  0,  3,  6,  0,  6,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, 0 },
                     { 2701, {  0,  1,  3,  0,  0,  4,  0,  1,  1,  0,  6,  0,  6,  6,  0,  6,  0,  1,  1,  1,  4,  0,  3,  0,  0,  0,  0,  6,  6,  0,  0,  0,  1,  0,  0,  1,  0,  1,  2,  0,  0,  6,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, 0 },
                     { 3603, {  0,  0,  3,  1,  1,  4,  1,  1,  1,  3,  6,  0,  0,  0,  0,  6,  0,  1,  1,  1,  4, 10,  6,  0,  0,  0,  0,  6,  0,  0,  0,  0,  1,  3,  0,  1,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, 0 },
#ifdef USE_PRIVATE_CE_WORLD
                     { 1011, {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  3,  0,  1,  1,  0,  0,  0,  6,  6,  0,  0,  0,  0,  1,  0,  0,  0,  3,  0,  6,  6,  1,  6,  1,  1,  1,  1,  1,  6,  1,  2,  1,  1,  2,  1,  6,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  6,  6,  1,  1,  1,  1,  1,  6,  6,  6,  1,  1,  1,  1,  1}, 0 },
#endif
//                     { 20326,{ 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 4,  5, 2, 2, 2, 0, 1, 2, 1, 2, 6, 3, 0, 2, 1, 0, 1, 0, 3, 0, 1, 0}, 0 },
                     { 0, { 0, 0, 0, 0 }, 0 } // this MUST be last
                   };

struct spell_trainer spelltrainers[] = {
  /*
//  { 60508, SPELL_DETECTLIFE, 0, 6},
  { 60508, SPELL_ARMOR, 0, 6},
  { 60508, SPELL_COMBATSENSE, 0, 6},
  { 60508, SPELL_HEAL, 0, 6},
  { 60508, SPELL_INCATTR, STR, 6},
  { 60508, SPELL_INCCYATTR, STR, 6},
  { 60508, SPELL_INCREF1, 0, 6},
  { 60508, SPELL_INVIS, 0, 6},
  { 60508, SPELL_LIGHTNINGBOLT, 0, 6},
  { 60508, SPELL_MANABOLT, 0, 6 },
  { 60508, SPELL_POWERBOLT, 0, 6 },
  { 60508, SPELL_STUNBOLT, 0, 6 },
  { 60508, SPELL_TREAT, 0, 6},

//{ 60509, SPELL_DETECTLIFE, 0, 6},
  { 60509, SPELL_ARMOR, 0, 6},
  { 60509, SPELL_COMBATSENSE, 0, 6},
  { 60509, SPELL_HEAL, 0, 6},
  { 60509, SPELL_INCATTR, ATT_STR, 6},
  { 60509, SPELL_INCCYATTR, ATT_STR, 6},
  { 60509, SPELL_INCREF1, 0, 6},
  { 60509, SPELL_INVIS, 0, 6},
  { 60509, SPELL_LIGHTNINGBOLT, 0, 6},
  { 60509, SPELL_MANABOLT, 0, 6 },
  { 60509, SPELL_POWERBOLT, 0, 6 },
  { 60509, SPELL_STUNBOLT, 0, 6 },
  { 60509, SPELL_TREAT, 0, 6}, */

//{60600, SPELL_DEATHTOUCH, 0, 6},
//{60600, SPELL_MANABALL, 0, 6},
//{60600, SPELL_POWERBALL, 0, 6},
//{60600, SPELL_STUNBALL, 0, 6},
  {60600, SPELL_MANABOLT, 0, 6},
  {60600, SPELL_POWERBOLT, 0, 6},
  {60600, SPELL_STUNBOLT, 0, 6},

//{60601, SPELL_HEALTHYGLOW, 0, 6},
//{60601, SPELL_PROPHYLAXIS, 0, 6},
  {60601, SPELL_DECCYATTR, BOD, 6},
  {60601, SPELL_DECCYATTR, STR, 6},
  {60601, SPELL_DECCYATTR, QUI, 6},
  {60601, SPELL_DETOX, 0, 6},
  {60601, SPELL_HEAL, 0, 6},
  {60601, SPELL_INCATTR, BOD, 6},
  {60601, SPELL_INCATTR, STR, 6},
  {60601, SPELL_INCATTR, QUI, 6},
  {60601, SPELL_INCCYATTR, BOD, 6},
  {60601, SPELL_INCCYATTR, STR, 6},
  {60601, SPELL_INCCYATTR, QUI, 6},
  {60601, SPELL_INCREA, 0, 6},
  {60601, SPELL_INCREF1, 0, 6},
  {60601, SPELL_INCREF2, 0, 6},
  {60601, SPELL_INCREF3, 0, 6},
  {60601, SPELL_STABILIZE, 0, 6},
  {60601, SPELL_TREAT, 0, 6},

//{60602, SPELL_ANALYZEDEVICE, 0, 6},
//{60602, SPELL_CLAIRAUDIENCE, 0, 6},
//{60602, SPELL_CLAIRVOYANCE, 0, 6},
//{60602, SPELL_DETECTENEMIES, 0, 6},
//{60602, SPELL_DETECTINDIV, 0, 6},
//{60602, SPELL_DETECTLIFE, 0, 6},
//{60602, SPELL_DETECTMAGIC, 0, 6},
//{60602, SPELL_DETECTOBJECT, 0, 6},
  {60602, SPELL_COMBATSENSE, 0, 6},
  {60602, SPELL_MINDLINK, 0, 6},
  {60602, SPELL_NIGHTVISION, 0, 6},
  {60602, SPELL_INFRAVISION, 0, 6},

//{60603, SPELL_MASSCONFUSION, 0, 6},
//{60603, SPELL_CHAOTICWORLD, 0, 6},
//{60603, SPELL_MASK, 0, 6},
//{60603, SPELL_PHYSMASK, 0, 6},
  {60603, SPELL_CHAOS, 0, 6},
  {60603, SPELL_CONFUSION, 0, 6},
  {60603, SPELL_INVIS, 0, 6},
  {60603, SPELL_IMP_INVIS, 0, 6},
  {60603, SPELL_SILENCE, 0, 6},
  {60603, SPELL_STEALTH, 0, 6},

//{60604, SPELL_TOXICWAVE, 0, 6},
//{60604, SPELL_FIREBALL, 0, 6},
//{60604, SPELL_BALLLIGHTNING, 0, 6},
//{60604, SPELL_PHYSICALBARRIER, 0, 6},
//{60604, SPELL_ASTRALBARRIER, 0, 6},
  {60604, SPELL_ACIDSTREAM, 0, 6},
  {60604, SPELL_ARMOR, 0, 6},
  {60604, SPELL_CLOUT, 0, 6},
  {60604, SPELL_FLAMETHROWER, 0, 6},
  {60604, SPELL_ICESHEET, 0, 6},
  {60604, SPELL_IGNITE, 0, 6},
  {60604, SPELL_LIGHT, 0, 6},
  {60604, SPELL_LIGHTNINGBOLT, 0, 6},
  {60604, SPELL_POLTERGEIST, 0, 6},
  {60604, SPELL_SHADOW, 0, 6},
  {60604, SPELL_LASER, 0, 6},
  {60604, SPELL_STEAM, 0, 6},
  {60604, SPELL_THUNDERBOLT, 0, 6},
  {60604, SPELL_WATERBOLT, 0, 6},
  {60604, SPELL_LEVITATE, 0, 6},
  {60604, SPELL_FLAME_AURA, 0, 6},

  // A throwaway spell teacher slot for Jarmine so it can let people learn force.
  { 60531, SPELL_MANABOLT, 0, 6 },

                            // Immortal HQ Trainer, not alphabetized because who has the time for that
                            {10013, SPELL_DEATHTOUCH, 0, 50},
                            {10013, SPELL_MANABOLT, 0, 50},
                            {10013, SPELL_MANABALL, 0, 50},
                            {10013, SPELL_POWERBOLT, 0, 50},
                            {10013, SPELL_POWERBALL, 0, 50},
                            {10013, SPELL_STUNBOLT, 0, 50},
                            {10013, SPELL_STUNBALL, 0, 50},
                            {10013, SPELL_DECATTR, BOD, 50},
                            {10013, SPELL_DECATTR, STR, 50},
                            {10013, SPELL_DECATTR, QUI, 50},
                            {10013, SPELL_DECCYATTR, BOD, 50},
                            {10013, SPELL_DECCYATTR, STR, 50},
                            {10013, SPELL_DECCYATTR, QUI, 50},
                            {10013, SPELL_DETOX, 0, 50},
                            {10013, SPELL_HEAL, 0, 50},
                            {10013, SPELL_TREAT, 0, 50},
                            {10013, SPELL_HEALTHYGLOW, 0, 50},
                            {10013, SPELL_INCATTR, BOD, 50},
                            {10013, SPELL_INCATTR, STR, 50},
                            {10013, SPELL_INCATTR, QUI, 50},
                            {10013, SPELL_INCCYATTR, BOD, 50},
                            {10013, SPELL_INCCYATTR, STR, 50},
                            {10013, SPELL_INCCYATTR, QUI, 50},
                            {10013, SPELL_INCREA, 0, 50},
                            {10013, SPELL_INCREF1, 0, 50},
                            {10013, SPELL_INCREF2, 0, 50},
                            {10013, SPELL_INCREF3, 0, 50},
                            {10013, SPELL_PROPHYLAXIS, 0, 50},
                            {10013, SPELL_STABILIZE, 0, 50},
                            {10013, SPELL_ANALYZEDEVICE, 0, 50},
                            {10013, SPELL_CLAIRAUDIENCE, 0, 50},
                            {10013, SPELL_CLAIRVOYANCE, 0, 50},
                            {10013, SPELL_COMBATSENSE, 0, 50},
                            {10013, SPELL_DETECTENEMIES, 0, 50},
                            {10013, SPELL_DETECTINDIV, 0, 50},
                            {10013, SPELL_DETECTLIFE, 0, 50},
                            {10013, SPELL_DETECTMAGIC, 0, 50},
                            {10013, SPELL_DETECTOBJECT, 0, 50},
                            {10013, SPELL_MINDLINK, 0, 50},
                            {10013, SPELL_NIGHTVISION, 0, 50},
                            {10013, SPELL_INFRAVISION, 0, 50},
                            {10013, SPELL_LEVITATE, 0, 50},
                            {10013, SPELL_CONFUSION, 0, 50},
                            {10013, SPELL_MASSCONFUSION, 0, 50},
                            {10013, SPELL_CHAOS, 0, 50},
                            {10013, SPELL_CHAOTICWORLD, 0, 50},
                            {10013, SPELL_INVIS, 0, 50},
                            {10013, SPELL_IMP_INVIS, 0, 50},
                            {10013, SPELL_MASK, 0, 50},
                            {10013, SPELL_PHYSMASK, 0, 50},
                            {10013, SPELL_SILENCE, 0, 50},
                            {10013, SPELL_STEALTH, 0, 50},
                            {10013, SPELL_LIGHT, 0, 50},
                            {10013, SPELL_ACIDSTREAM, 0, 50},
                            {10013, SPELL_TOXICWAVE, 0, 50},
                            {10013, SPELL_FLAMETHROWER, 0, 50},
                            {10013, SPELL_FIREBALL, 0, 50},
                            {10013, SPELL_LIGHTNINGBOLT, 0, 50},
                            {10013, SPELL_BALLLIGHTNING, 0, 50},
                            {10013, SPELL_CLOUT, 0, 50},
                            {10013, SPELL_LASER, 0, 50},
                            {10013, SPELL_NOVA, 0, 50},
                            {10013, SPELL_STEAM, 0, 50},
                            {10013, SPELL_THUNDERBOLT, 0, 50},
                            {10013, SPELL_THUNDERCLAP, 0, 50},
                            {10013, SPELL_WATERBOLT, 0, 50},
                            {10013, SPELL_SPLASH, 0, 50},
                            {10013, SPELL_POLTERGEIST, 0, 50},
                            {10013, SPELL_ARMOR, 0, 50},
                            {10013, SPELL_PHYSICALBARRIER, 0, 50},
                            {10013, SPELL_ASTRALBARRIER, 0, 50},
                            {10013, SPELL_ICESHEET, 0, 50},
                            {10013, SPELL_IGNITE, 0, 50},
                            {10013, SPELL_SHADOW, 0, 50},
                            {10013, SPELL_FLAME_AURA, 0, 50},

                            // Final line
                            { 0, 0, 0, 0},
                          };


/* functions to perform assignments */

void ASSIGNMOB(long mob, SPECIAL(fname))
{
  long rnum;

  if ((rnum = real_mobile(mob)) >= 0) {
    if (mob_index[rnum].sfunc)
      log_vfprintf("SYSERR: Assigning too many specs to mob #%d.", mob);
    mob_index[rnum].sfunc = mob_index[rnum].func;
    mob_index[rnum].func = fname;
  } else
    log_vfprintf("SYSERR: Attempt to assign spec to non-existent mob #%d", mob);
}

void ASSIGNOBJ(long obj, SPECIAL(fname))
{
  int real_obj;
  if ((real_obj = real_object(obj)) >= 0)
    obj_index[real_obj].func = fname;
  else
    log_vfprintf("SYSERR: Attempt to assign spec to non-existent obj #%d", obj);
}

void ASSIGNWEAPON(long weapon, WSPEC(fname))
{
  int real_obj;
  if ((real_obj = real_object(weapon)) >= 0)
    obj_index[real_obj].wfunc = fname;
  else
    log_vfprintf("SYSERR: Attempt to assign spec to non-existent weapon #%d", weapon);
}

void ASSIGNROOM(long room, SPECIAL(fname))
{
  int real_rm;
  if ((real_rm = real_room(room)) >= 0)
    world[real_rm].func = fname;
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
  SPECIAL(Trogatron);
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
  SPECIAL(axehead);
  SPECIAL(chargen_docwagon_checker);
  SPECIAL(fatcop);

#ifdef USE_PRIVATE_CE_WORLD
  SPECIAL(marksmanship_first);
  SPECIAL(marksmanship_second);
  SPECIAL(marksmanship_third);
  SPECIAL(marksmanship_fourth);
  SPECIAL(marksmanship_master);
  SPECIAL(marksmanship_trainer);

  ASSIGNMOB(22870, marksmanship_first);
  ASSIGNMOB(4101, marksmanship_second);
  ASSIGNMOB(24501, marksmanship_third);
  ASSIGNMOB(3722, marksmanship_fourth);
  ASSIGNMOB(5914, marksmanship_master);
  ASSIGNMOB(779, marksmanship_trainer);
#endif

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

  for (i = 0; spelltrainers[i].teacher; i++) {
    int real_mob = real_mobile(spelltrainers[i].teacher);
    if (real_mob >= 0 && mob_index[real_mob].func != spell_trainer)
      ASSIGNMOB(spelltrainers[i].teacher, spell_trainer);
  }

  /* cab drivers */
  ASSIGNMOB(600, taxi);
  ASSIGNMOB(650, taxi);
  #ifdef USE_PRIVATE_CE_WORLD
  ASSIGNMOB(640, taxi);
  #endif

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

  /* Puyallup */
  ASSIGNMOB(35200, Trogatron);

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
  // ASSIGNMOB(30701, receptionist);
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

#ifdef USE_PRIVATE_CE_WORLD
  /* Slitch Pit */
  ASSIGNMOB(62803, axehead);
  ASSIGNMOB(62807, receptionist);

  /* Neophyte expansion */
  ASSIGNMOB(60599, chargen_docwagon_checker);

  /* Homewood */
  ASSIGNMOB(30705, fatcop);

  /* Secret areas */
  perform_secret_mob_assignments();
#endif
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
  SPECIAL(nerpcorpolis_button);
  SPECIAL(floor_usable_radio);
  SPECIAL(medical_workshop);
  SPECIAL(toggled_voice_modulator);
  SPECIAL(initiative_tracker);

  ASSIGNOBJ(26, gen_board);
  ASSIGNOBJ(10018, gen_board);  /* ImmHQ Board */
#ifdef USE_PRIVATE_CE_WORLD
  ASSIGNOBJ(10034, gen_board); /* Mortal Relations Board */
#endif
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
  ASSIGNOBJ(2108, bank);         // atm
  ASSIGNOBJ(2506, anticoagulant);
  ASSIGNOBJ(3012, vendtix);
  ASSIGNOBJ(4006, clock);
  ASSIGNOBJ(4216, clock);
  ASSIGNOBJ(9329, vending_machine);
  ASSIGNOBJ(10011, desktop);
  ASSIGNOBJ(10108, vending_machine);
  ASSIGNOBJ(42130, anticoagulant);
  ASSIGNOBJ(10025, pocket_sec);
  ASSIGNOBJ(9406, hand_held_scanner);
  ASSIGNOBJ(22445, bank);
  ASSIGNOBJ(14645, clock);
  ASSIGNOBJ(16234, clock);
  ASSIGNOBJ(18884, clock);
  ASSIGNOBJ(18950, bank);
  ASSIGNOBJ(14726, bank);
  ASSIGNOBJ(722, toggled_invis);
  ASSIGNOBJ(25519, toggled_invis);
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
  ASSIGNOBJ(6996, initiative_tracker);
#ifdef USE_PRIVATE_CE_WORLD
  ASSIGNOBJ(94331, trideo);
  ASSIGNOBJ(70605, desktop);
  // ASSIGNOBJ(10036, restoration_button);
  ASSIGNOBJ(39799, bank);
  ASSIGNOBJ(60620, bank); // Neophyte ATM
  ASSIGNOBJ(94329, bank); // NERPcropolis ATM
  ASSIGNOBJ(5498, bank); // Vancouver ATM
  ASSIGNOBJ(16297, bank);
  ASSIGNOBJ(6997, nerpcorpolis_button);
  ASSIGNOBJ(16298, floor_usable_radio);
  ASSIGNOBJ(26104, toggled_voice_modulator);

  perform_secret_obj_assignments();
#endif

  ASSIGNOBJ(OBJ_SEATTLE_TAXI_SIGN, taxi_sign);
  ASSIGNOBJ(OBJ_PORTLAND_TAXI_SIGN, taxi_sign);
  ASSIGNOBJ(OBJ_CARIBBEAN_TAXI_SIGN, taxi_sign);

  ASSIGNOBJ(10005, weapon_dominator);

  ASSIGNOBJ(15811, medical_workshop);
  ASSIGNOBJ(16242, medical_workshop);
  ASSIGNOBJ(70611, medical_workshop);
#ifdef USE_PRIVATE_CE_WORLD
  ASSIGNOBJ(10039, medical_workshop);
#endif

  WSPEC(monowhip);

  ASSIGNWEAPON(660, monowhip);
#ifdef USE_PRIVATE_CE_WORLD
  ASSIGNWEAPON(25626, monowhip);
#endif
  ASSIGNWEAPON(80108, monowhip);
}

/* assign special procedures to rooms */

void assign_rooms(void)
{
  SPECIAL(car_dealer);
  SPECIAL(oceansounds);
  SPECIAL(escalator);
  SPECIAL(neophyte_salvation_army);
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
  SPECIAL(south_of_chargen_skill_annex);
  SPECIAL(chargen_unpractice_skill);
  SPECIAL(floor_has_glass_shards);
  SPECIAL(chargen_career_archetype_paths);
  SPECIAL(chargen_spirit_combat_west);
  SPECIAL(archetype_chargen_magic_split);
  SPECIAL(archetype_chargen_reverse_magic_split);
  SPECIAL(chargen_language_annex);
  SPECIAL(nerpcorpolis_lobby);
  SPECIAL(troll_barrier);
  SPECIAL(chargen_points_check);
  SPECIAL(purge_prevented);

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
  ASSIGNROOM(60513, south_of_chargen_skill_annex);
  ASSIGNROOM(60562, auth_room);
  ASSIGNROOM(60514, chargen_career_archetype_paths);
  ASSIGNROOM(60594, chargen_spirit_combat_west);

  /* CharGen - allow forgetting skills */
  ASSIGNROOM(60507, chargen_unpractice_skill);
  ASSIGNROOM(60508, chargen_unpractice_skill);
  ASSIGNROOM(60509, chargen_unpractice_skill);
  ASSIGNROOM(60510, chargen_unpractice_skill);
  ASSIGNROOM(60511, chargen_unpractice_skill);
  #ifdef REQUIRE_LANGUAGE_SKILL_POINTS_BE_USED_ONLY_FOR_LANGUAGE
  ASSIGNROOM(60512, chargen_language_annex);
  #else
  ASSIGNROOM(60512, chargen_unpractice_skill);
  #endif
  ASSIGNROOM(60591, chargen_unpractice_skill);
  ASSIGNROOM(60592, chargen_unpractice_skill);
  ASSIGNROOM(60593, chargen_unpractice_skill);
  ASSIGNROOM(60626, chargen_unpractice_skill);
  ASSIGNROOM(60627, chargen_unpractice_skill);
  ASSIGNROOM(60628, chargen_unpractice_skill);
  ASSIGNROOM(60661, chargen_unpractice_skill);
  ASSIGNROOM(60662, chargen_unpractice_skill);
  /* CharGen - auto-set archetype end rooms etc */
  for (int i = 0; i < NUM_CCR_ARCHETYPES; i++) {
    ASSIGNROOM(archetypes[i]->auth_room, auth_room);
    ASSIGNROOM(archetypes[i]->warning_room, chargen_points_check);
  }

  /* Neophyte Guild */
  ASSIGNROOM(60563, neophyte_salvation_army);
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

  /* Archetypal chargen. */
  ASSIGNROOM(90700, archetype_chargen_magic_split);
  ASSIGNROOM(90709, archetype_chargen_reverse_magic_split);

  // Junkyard
  ASSIGNROOM(70504, floor_has_glass_shards);

  // Purge prevention for the veh storage room.
  ASSIGNROOM(RM_PORTABLE_VEHICLE_STORAGE, purge_prevented);

#ifdef USE_PRIVATE_CE_WORLD
  // Nerpcorpolis
  ASSIGNROOM(RM_NERPCORPOLIS_LOBBY, nerpcorpolis_lobby);

  // Wither's troll_barrier
  ASSIGNROOM(15600, troll_barrier);

  // Read the secrets file to learn more about these.
  perform_secret_room_assignments();
#endif

  log("Assigning traffic messages...");
  for (long x = 0; x <= top_of_world; x++)
    if (ROOM_FLAGGED(&world[x], ROOM_ROAD)
        && !ROOM_FLAGGED(&world[x], ROOM_NO_TRAFFIC)
        && !ROOM_FLAGGED(&world[x], ROOM_GARAGE)
        && !world[x].func
        && SECT(&world[x]) == SPIRIT_CITY)
      ASSIGNROOM(world[x].number, traffic);
}
