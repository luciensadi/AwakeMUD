/* *******************************************
 *                                            *
 *                  iedit.cc                  *
 *              Part of AwakeOLC,             *
 *          a component of AwakeMUD           *
 *                                            *
 *       (c)2001 Andrew Hynek, and the        *
 *             AwakeMUD Consortium            *
 *                                            *
 ******************************************* */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.hpp"
#include "awake.hpp"
#include "interpreter.hpp"
#include "comm.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "dblist.hpp"
#include "boards.hpp"
#include "screen.hpp"
#include "olc.hpp"
#include "memory.hpp"
#include "newmagic.hpp"
#include "constants.hpp"
#include "handler.hpp"

#define OBJ     d->edit_obj


void write_objs_to_disk(vnum_t zone);


// extern funcs
extern char *cleanup(char *dest, const char *src);

#define NUM_WEAPON_TYPES        27
#define NUM_SKILL_TYPES         20
void iedit_disp_container_flags_menu(struct descriptor_data * d);
void iedit_disp_extradesc_menu(struct descriptor_data * d);
void iedit_disp_weapon_menu(struct descriptor_data * d);
void iedit_disp_val1_menu(struct descriptor_data * d);
void iedit_disp_val2_menu(struct descriptor_data * d);
void iedit_disp_val3_menu(struct descriptor_data * d);
void iedit_disp_val4_menu(struct descriptor_data * d);
void iedit_disp_val5_menu(struct descriptor_data * d);
void iedit_disp_val6_menu(struct descriptor_data * d);
void iedit_disp_val7_menu(struct descriptor_data * d);
void iedit_disp_val8_menu(struct descriptor_data * d);
void iedit_disp_val9_menu(struct descriptor_data * d);
void iedit_disp_val10_menu(struct descriptor_data * d);
void iedit_disp_val11_menu(struct descriptor_data * d);
void iedit_disp_val12_menu(struct descriptor_data * d);


void iedit_disp_type_menu(struct descriptor_data * d);
void iedit_disp_extra_menu(struct descriptor_data * d);
void iedit_disp_wear_menu(struct descriptor_data * d);
void iedit_disp_menu(struct descriptor_data * d);
void iedit_disp_drinktype_menu(struct descriptor_data *d);

void iedit_disp_patch_menu(struct descriptor_data *d);
void iedit_parse(struct descriptor_data * d, const char *arg);
void iedit_disp_spells_menu(struct descriptor_data * d);
void iedit_disp_cybertype_menu(struct descriptor_data * d);
void iedit_program_types_menu(struct descriptor_data *d);
void iedit_disp_biotype_menu(struct descriptor_data * d);


/**************************************************************************
 Menu functions
 **************************************************************************/
/* For container flags */
void iedit_disp_container_flags_menu(struct descriptor_data * d)
{
  sprintbit(GET_OBJ_VAL(d->edit_obj, 1), container_bits, buf1, sizeof(buf1));
  send_to_char(CH, "1) Closeable\r\n"
               "2) Pickproof\r\n"
               "3) Closed\r\n"
               "4) Locked\r\n"
               "Container flags: %s%s%s\r\n"
               "Enter flag, 0 to quit:", CCCYN(CH, C_CMP), buf1,
               CCNRM(CH, C_CMP));
}

/* For extra descriptions */
void iedit_disp_extradesc_menu(struct descriptor_data * d)
{
  struct extra_descr_data *extra_desc =
  (struct extra_descr_data *) * d->misc_data;

  send_to_char(CH, "Extra desc menu\r\n"
               "0) Quit\r\n"
               "1) Keyword: %s%s%s\r\n"
               "2) Description:\r\n%s\r\n"
               "3) %s%s%s\r\n",
               CCCYN(CH, C_CMP), (extra_desc->keyword ?  extra_desc->keyword : "(none)"),
               CCNRM(CH, C_CMP), (extra_desc->description ? extra_desc->description : "(none)"),
               CCCYN(CH, C_CMP), (!extra_desc->next ? "(not set)" : "Set (not viewed)."),
               CCNRM(CH, C_CMP));
  d->edit_mode = IEDIT_EXTRADESC_MENU;
}

/* Ask for *which* apply to edit */
void iedit_disp_prompt_apply_menu(struct descriptor_data * d)
{
  int             counter;

  CLS(CH);
  for (counter = 0; counter < MAX_OBJ_AFFECT; counter++)
  {
    if (d->edit_obj->affected[counter].modifier) {
      if (GET_OBJ_TYPE(d->edit_obj) == ITEM_MOD)
        sprinttype(d->edit_obj->affected[counter].location, veh_aff, buf2, sizeof(buf2));
      else
        sprinttype(d->edit_obj->affected[counter].location, apply_types, buf2, sizeof(buf2));
      send_to_char(CH, " %d) %+d to %s%s%s\r\n", counter + 1,
                   d->edit_obj->affected[counter].modifier, CCCYN(CH, C_CMP), buf2,
                   CCNRM(CH, C_CMP));
    } else
      send_to_char(CH, " %d) None.\r\n", counter + 1);
  }
  send_to_char("Enter affection to modify (0 to quit):", d->character);
  d->edit_mode = IEDIT_PROMPT_APPLY;
}

/* The actual apply to set */
void iedit_disp_apply_menu(struct descriptor_data * d)
{
  int             counter;
  if (GET_OBJ_TYPE(d->edit_obj) == ITEM_MOD)
  {
    for (counter = 0; counter < VAFF_MAX; counter++) {
      send_to_char(CH, "%2d) %-18s%s",
                    counter,
                    veh_aff[counter],
                    counter % 2 == 1 ? "    " : "\r\n"
                  );
    }
  } else {
    for (counter = 0; counter < APPLY_MAX; counter ++) {
      send_to_char(CH, "%2d) %-18s%s",
                    counter,
                    apply_types[counter],
                    counter % 2 == 1 ? "    " : "\r\n"
                  );
    }
  }
  send_to_char("\r\nEnter apply type (0 is no apply):", d->character);
  d->edit_mode = IEDIT_APPLY;
}

/* skill needed in weapon */
void iedit_disp_skill_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char( " 1) Edged Weapons\r\n"
               " 2) Pole Arms\r\n"
               " 3) Whips and flails\r\n"
               " 4) Clubs\r\n"
               " 5) Pistols\r\n"
               " 6) Rifles\r\n"
               " 7) Shotguns\r\n"
               " 8) Assault Rifles\r\n"
               " 9) Submachine Guns\r\n"
               "10) Grenade Launchers\r\n"
               "11) Tasers\r\n"
               "12) Machine Guns\r\n"
               "13) Missile Launchers\r\n"
               "14) Assault Cannons\r\n"
               "15) Artillery\r\n"
               "16) Bows\r\n"
               "17) Crossbows\r\n"
               "18) Non-aerodynamic throwing weapons\r\n"
               "19) Aerodynamic throwing weapons\r\n"
               "20) Unarmed Combat\r\n"
               "Enter skill needed for weapon:\r\n", CH);
}

/* weapon type */
void iedit_disp_weapon_menu(struct descriptor_data * d)
{
  CLS(CH);
  for (int counter = 0; counter < MAX_WEAP; counter += 2)
    send_to_char(CH, "%2d) %-18s %2d) %-18s\r\n",
                 counter, weapon_types[counter],
                 counter + 1, counter + 1 < MAX_WEAP ? weapon_types[counter + 1] : "");

  send_to_char("Enter weapon type:\r\n", CH);
}

/* spell type */
void iedit_disp_spells_menu(struct descriptor_data * d)
{
  CLS(CH);
  for (int counter = 1; counter < MAX_SPELLS; counter += 3)
    send_to_char(CH, "%2d) %-18s %2d) %-18s %2d) %-18s\r\n",
                 counter, spells[counter].name,
                 counter + 1, counter + 1 < MAX_SPELLS ? spells[counter + 1].name : "",
                 counter + 2, counter + 2 < MAX_SPELLS ? spells[counter + 2].name : "");

  send_to_char("Enter spell:\r\n", CH);
}

void iedit_disp_cybereyes_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int y = 0; y < NUM_EYEMODS; y++)
    send_to_char(CH, "%2d) %-20s\r\n", y+1, eyemods[y]);
  sprintbit(GET_CYBERWARE_FLAGS(OBJ), eyemods, buf1, sizeof(buf1));
  send_to_char(CH, "Set Options: ^c%s^n\r\nEnter options (0 to quit): ", buf1);
}

void iedit_disp_cyberarms_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int y = 0; y < NUM_ARMS_MODS; y++)
    send_to_char(CH, "%2d) %-20s\r\n", y+1, armsmods[y]);
  sprintbit(GET_CYBERWARE_FLAGS(OBJ), armsmods, buf1, sizeof(buf1));
  send_to_char(CH, "Set Options: ^c%s^n\r\nEnter options (0 to quit): ", buf1);
}

void iedit_disp_cyberlegs_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int y = 0; y < NUM_LEGS_MODS; y++)
    send_to_char(CH, "%2d) %-20s\r\n", y+1, legsmods[y]);
  sprintbit(GET_CYBERWARE_FLAGS(OBJ), legsmods, buf1, sizeof(buf1));
  send_to_char(CH, "Set Options: ^c%s^n\r\nEnter options (0 to quit): ", buf1);
}

void iedit_disp_cyberskull_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int y = 0; y < NUM_SKULL_MODS; y++)
    send_to_char(CH, "%2d) %-20s\r\n", y+1, skullmods[y]);
  sprintbit(GET_CYBERWARE_FLAGS(OBJ), skullmods, buf1, sizeof(buf1));
  send_to_char(CH, "Set Options: ^c%s^n\r\nEnter options (0 to quit): ", buf1);
}

void iedit_disp_cybertorso_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int y = 0; y < NUM_TORSO_MODS; y++)
    send_to_char(CH, "%2d) %-20s\r\n", y+1, torsomods[y]);
  sprintbit(GET_CYBERWARE_FLAGS(OBJ), torsomods, buf1, sizeof(buf1));
  send_to_char(CH, "Set Options: ^c%s^n\r\nEnter options (0 to quit): ", buf1);
}

void iedit_disp_firemodes_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int y = MODE_SS; y <= MODE_FA; y++)
    send_to_char(CH, "  %d) %s\r\n", y, fire_mode[y]);
  sprintbit(GET_OBJ_VAL(OBJ, 10), fire_mode, buf1, sizeof(buf1));
  send_to_char(CH, "Set Options: ^c%s^n\r\nEnter options (0 to quit): ", buf1);
}

void iedit_disp_mod_engine_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int engine_type = ENGINE_ELECTRIC; engine_type <= ENGINE_DIESEL; engine_type++)
    send_to_char(CH, "  %d) %s\r\n", engine_type, engine_types[engine_type]);
  sprintbit(GET_OBJ_VAL(OBJ, 5), engine_types, buf1, sizeof(buf1));
  send_to_char(CH, "Set Options: ^c%s^n\r\nEnter options (0 to quit): ", buf1);
}

void iedit_disp_mod_vehicle_type_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int veh_type = 0; veh_type < NUM_VEH_TYPES; veh_type++)
    send_to_char(CH, " %2d) %s\r\n", veh_type + 1, veh_types[veh_type]);
  sprintbit(GET_VEHICLE_MOD_DESIGNED_FOR_FLAGS(OBJ), veh_types, buf1, sizeof(buf1));
  send_to_char(CH, "Set Compatible Vehicle Types: ^c%s^n\r\nEnter options (0 to quit): ", buf1);
}

/* object value 1 */
void iedit_disp_val1_menu(struct descriptor_data * d)
{
  if (d->iedit_limit_edits > 0) {
    // Cut out when the decremented counter hits zero.
    if (--d->iedit_limit_edits == 0) {
      iedit_disp_menu(d);
      return;
    }
  }
  d->edit_mode = IEDIT_VALUE_1;
  int c, line = 0;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_DRUG:
      for (c = 0; c < NUM_DRUGS; c++)
        send_to_char(CH, "%d) %s\r\n", c, drug_types[c].name);
      send_to_char(CH, "Drug Type: ");
      break;
    case ITEM_WORKSHOP:
      send_to_char(CH, "0) General\r\n1) Electronic\r\n2) Microtronic\r\n3) Cyberware\r\n4) Vehicle\r\n5) Weaponry\r\n6) Medical\r\n7) Ammunition\r\n8) Gunsmithing\r\nWorkshop Type: ");
      break;
    case ITEM_CAMERA:
      send_to_char("0) Camera\r\n1) Photo\r\nObject Type: ", CH);
      break;
    case ITEM_WEAPON:
    case ITEM_FIREWEAPON:
      send_to_char("Power (0 for melee weapon or bow):", d->character);
      break;
    case ITEM_MISSILE:
      send_to_char("\r\n  0) Arrow     1) Bolt\r\nMissile type: ", CH);
      break;
    case ITEM_GYRO:
      send_to_char("RATING: ", d->character);
      break;
    case ITEM_MAGIC_TOOL:
      send_to_char(CH, "0) Conjuring Library\r\n1) Sorcery Library\r\n2) Hermetic Circle\r\n3) Shamanic Lodge\r\n4) Summoning Materials\r\nMagic Tool Type: ");
      break;
    case ITEM_QUIVER:
      send_to_char("Max projectiles to contain: ", CH);
      break;
    case ITEM_CONTAINER:
      send_to_char("Max weight to contain: ", d->character);
      break;
    case ITEM_DRINKCON:
    case ITEM_FOUNTAIN:
      send_to_char("Max drink units: ", d->character);
      break;
    case ITEM_FOOD:
      send_to_char("Hours to fill stomach: ", d->character);
      break;
    case ITEM_MONEY:
      send_to_char("Number of nuyen: ", d->character);
      break;
    case ITEM_CYBERWARE:
      iedit_disp_cybertype_menu(d);
      break;
    case ITEM_BIOWARE:
      iedit_disp_biotype_menu(d);
      break;
    case ITEM_CYBERDECK:
      send_to_char("MPCP Rating: ", CH);
      break;
    case ITEM_PROGRAM:
      iedit_program_types_menu(d);
      break;
    case ITEM_GUN_MAGAZINE:
      send_to_char("Number of bullets magazine contains (must match max load of weapon): ", CH);
      break;
    case ITEM_GUN_AMMO:
      send_to_char("Number of bullets ammo box contains: ", CH);
      break;
    case ITEM_DOCWAGON:
      send_to_char("  1) Basic contract\r\n  2) Gold contract\r\n  3) Platinum contract\r\nDocWagon type: ", CH);
      break;
    case ITEM_PATCH:
      iedit_disp_patch_menu(d);
      break;
    case ITEM_SPELL_FORMULA:
      send_to_char("Force: ", CH);
      break;
    case ITEM_DECK_ACCESSORY:
      send_to_char("  0) Data file\r\n  1) Upgrade\r\n  2) Computer\r\n  3) Parts\r\n  4) Chip Cooker\r\n"
                   "Accessory type: ", CH);
      break;
    case ITEM_GUN_ACCESSORY:
      send_to_char("  0) Top\r\n  1) Barrel\r\n 2) Under\r\nLocation to mount accessory: ", CH);
      break;
    case ITEM_FOCUS:
      send_to_char("  0) Expendable Spell\r\n  1) Specific Spell\r\n  2) Spell Category\r\n  3) Spirit\r\n"
                   "  4) Power\r\n  5) Sustaining\r\n  6) Weapon\r\n  7) Spell Defense\r\nFocus type: ", CH);
      break;
    case ITEM_CLIMBING:
      send_to_char("Enter rating (1-3): ", CH);
      break;
    case ITEM_RADIO:
      // Skipping this field while doing nothing? Re-increment our counter.
      if (d->iedit_limit_edits)
        d->iedit_limit_edits++;
      iedit_disp_val2_menu(d);
      break;
    case ITEM_MOD:
      for (c = 1; c < NUM_MODTYPES; c++)
        send_to_char(CH, " %d) %s\r\n", c, mod_types[c].name);
      send_to_char("Enter type of Mod: ", CH);
      break;
    case ITEM_HOLSTER:
      send_to_char("Type (0 - Pistol/SMG, 1 - Sword, 2 - Rifle (or larger)): ", d->character);
      break;
    case ITEM_WORN:
      send_to_char("Holster/Sheath/Quiver Space: ", d->character);
      break;
    case ITEM_RCDECK:
      send_to_char("Rating: ", d->character);
      break;
    case ITEM_PHONE:
      send_to_char("Area Code (0 - Seattle, 1 - Tacoma, 2 - Portland)): ", d->character);
      break;
    case ITEM_CHIP:
      CLS(CH);
      for (c = 0; c < MAX_SKILLS; c++) {
        line++;
        if ((line % 3) == 1)
          snprintf(buf, sizeof(buf), "%2d) %-20s ", c, skills[c].name);
        else
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%2d) %-20s ", c, skills[c].name);
        if (!(line % 3)) {
          strlcat(buf, "\r\n", sizeof(buf));
          send_to_char(buf, CH);
        }
      }
      if ((line % 3) != 0) {
        strlcat(buf, "\r\nEnter a skill (0 to quit): ", sizeof(buf));
        send_to_char(buf, CH);
      }
      break;
    case ITEM_OTHER:
      send_to_char("Enter Value 0: ", CH);
      break;
    default:
      iedit_disp_menu(d);
  }
}

/* object value 2 */
void iedit_disp_val2_menu(struct descriptor_data * d)
{
  if (d->iedit_limit_edits > 0) {
    // Cut out when the decremented counter hits zero.
    if (--d->iedit_limit_edits == 0) {
      iedit_disp_menu(d);
      return;
    }
  }
  d->edit_mode = IEDIT_VALUE_2;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_DRUG:
      send_to_char(CH, "Doses: ");
      break;
    case ITEM_CLIMBING:
      send_to_char("Is this a pair of water wings? (1 for yes, 0 for no): ", CH);
      break;
    case ITEM_WORKSHOP:
      send_to_char(CH, "1) Kit\r\n2) Workshop\r\n3) Facility\r\nEnter Type: ");
      break;
    case ITEM_CHIP:
      send_to_char(CH, "Enter skillsoft rating: ");
      break;
    case ITEM_WEAPON:
    case ITEM_FIREWEAPON:
      send_to_char("Base Damage (1 - Light, 2 - Moderate, 3 - Serious, 4 - Deadly): ", d->character);
      break;
    case ITEM_DRINKCON:
    case ITEM_FOUNTAIN:
      send_to_char("Drink units left: ", CH);
      break;
    case ITEM_FOOD:
      /* values 2 and 3 are unused, jump to 4. how odd */
      // Skipping this field while doing nothing? Re-increment our counter.
      if (d->iedit_limit_edits)
        d->iedit_limit_edits++;
      iedit_disp_val4_menu(d);
      return;
    case ITEM_SPELL_FORMULA:
      iedit_disp_spells_menu(d);
      break;
    case ITEM_CONTAINER:
      /* these are flags, needs a bit of special handling */
      iedit_disp_container_flags_menu(d);
      break;
    case ITEM_QUIVER:
      send_to_char("  0) Arrows\r\n  1) Bolts\r\n  2) Shurikens\r\n  3) Throwing knives\r\n"
                   "Quiver type: ", CH);
      break;
    case ITEM_CYBERWARE:
      send_to_char("Rating: ", CH);
      break;
    case ITEM_MAGIC_TOOL:
      send_to_char("Force/Rating: ", CH);
      break;
    case ITEM_BIOWARE:
      send_to_char("Rating: ", CH);
      break;
    case ITEM_CYBERDECK:
      send_to_char("Hardening: ", CH);
      break;
    case ITEM_PROGRAM:
      send_to_char("Program Rating: ", CH);
      break;
    case ITEM_PATCH:
      send_to_char("Rating: ", CH);
      break;
    case ITEM_DECK_ACCESSORY:
      switch (GET_OBJ_VAL(OBJ, 0)) {
        case TYPE_FILE:
          // Skipping this field while doing nothing? Re-increment our counter.
          if (d->iedit_limit_edits)
            d->iedit_limit_edits++;
          iedit_disp_val3_menu(d);
          return;
        case TYPE_UPGRADE:
          send_to_char("  0) MPCP (replacement)\r\n  1) Active memory\r\n"
                       "  2) Storage memory\r\n  3) Hitcher Jack\r\n  4) IO speed\r\n"
                       "  5) Reaction increase\r\nUpgrade type: ", CH);
          break;
        case TYPE_COMPUTER:
          send_to_char("Active Memory: ", CH);
          break;
        case TYPE_PARTS:
          send_to_char("  0) General Parts\r\n  1) Memory Chips\r\n", CH);
          break;
        case TYPE_COOKER:
          send_to_char("Rating: ", CH);
          break;
        default:
          iedit_disp_menu(d);
          break;
      }
      break;
    case ITEM_GUN_ACCESSORY:
      send_to_char("  1) Smartlink\r\n  2) Scope\r\n  3) Gas vent\r\n  4) Shock pad\r\n"
                   "  5) Pistol Silencer\r\n  6) Rifle/AR/SMG Sound Suppressor\r\n  7) Smart Goggles\r\n  8) Bipod\r\n  9) Tripod\r\n"
                   " 10) Bayonet\r\nAccessory type: ", CH);
      break;
    case  ITEM_FOCUS:
      send_to_char("Focus force: ", CH);
      break;
    case ITEM_GUN_AMMO:
    case ITEM_GUN_MAGAZINE:
      iedit_disp_weapon_menu(d);
      break;
    case ITEM_RADIO:
      send_to_char("Enter radio's range (0-5): ", CH);
      break;
    case ITEM_MONEY:
      send_to_char("Type (1 - cash, 2 - credstick): ", d->character);
      break;
    case ITEM_MOD:
      if (GET_VEHICLE_MOD_TYPE(OBJ) == TYPE_MOUNT)
        send_to_char("  0) Firmpoint Internal Fixed Mount\r\n"
                     "  1) Firmpoint External Fixed Mount\r\n"
                     "  2) Hardpoint Internal Fixed Mount\r\n"
                     "  3) Hardpoint External Fixed Mount\r\n"
                     "  4) Turret\r\n"
                     "  5) Mini-Turret\r\n"
                     "Type of Mount: ", CH);
      else
        send_to_char("Load space taken up: ", d->character);
      break;
    case ITEM_WORN:
      // Skipping this field while doing nothing? Re-increment our counter.
      if (d->iedit_limit_edits)
        d->iedit_limit_edits++;
      iedit_disp_val3_menu(d);
      // send_to_char("Space for magazines: ", CH);
      return;
    case ITEM_OTHER:
      send_to_char("Enter Value 0: ", CH);
      break;
    default:
      iedit_disp_menu(d);
  }
}

/* object value 3 */
void iedit_disp_val3_menu(struct descriptor_data * d)
{
  if (d->iedit_limit_edits > 0) {
    // Cut out when the decremented counter hits zero.
    if (--d->iedit_limit_edits == 0) {
      iedit_disp_menu(d);
      return;
    }
  }
  d->edit_mode = IEDIT_VALUE_3;
  int c;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WORKSHOP:
      if (GET_OBJ_VAL(OBJ, 0) == TYPE_AMMO && GET_OBJ_VAL(OBJ, 1) == TYPE_KIT) {
        for (c = AMMO_NORMAL; c < NUM_AMMOTYPES; c++)
          send_to_char(CH, "%d) %s\r\n", c, ammo_type[c].name);
        send_to_char("Select ammunition type: ", CH);
      } else iedit_disp_menu(d);

      break;
    case ITEM_WEAPON:
    case ITEM_FIREWEAPON:
      send_to_char("Strength bonus for melee (0 for non-melee weapons): ", CH);
      break;
    case ITEM_CONTAINER:
      send_to_char("Vnum of key to open container (-1 for no key): ", d->character);
      break;
    case ITEM_DRINKCON:
    case ITEM_FOUNTAIN:
      iedit_disp_drinktype_menu(d);
      break;
    case ITEM_CYBERWARE:
      for (int x = 0; x < 4; x++)
        send_to_char(CH, "  %d) %s\r\n", x, cyber_grades[x]);
      send_to_char("Enter Cyberware Grade: ", CH);
      break;
    case ITEM_BIOWARE:
      if (GET_OBJ_VAL(OBJ, 0) < BIO_CEREBRALBOOSTER)
        send_to_char(CH, "Cultured (%d - Yes, %d - No): ", BIOWARE_CULTURED, BIOWARE_STANDARD);
      else {
        GET_OBJ_VAL(OBJ, 2) = BIOWARE_CULTURED;
        iedit_disp_menu(d);
      }
      break;
    case ITEM_GUN_AMMO:
    case ITEM_GUN_MAGAZINE:
      for (int i = 0; i < NUM_AMMOTYPES; i++) {
        send_to_char(CH, "%d) %s\r\n", i, CAP(ammo_type[i].name));
      }
      send_to_char("Select Type: ", CH);
      break;
    case ITEM_CYBERDECK:
      send_to_char("Active: ", CH);
      break;
    case ITEM_PROGRAM:
      send_to_char("Program size: ", CH);
      break;
    case ITEM_SPELL_FORMULA:
      send_to_char("0 for Mage, 1 for Shaman: ", CH);
      break;
    case ITEM_DECK_ACCESSORY:
      switch (GET_OBJ_VAL(OBJ, 0)) {
        case TYPE_FILE:
          send_to_char("File size: ", CH);
          break;
        case TYPE_UPGRADE:
          switch (GET_OBJ_VAL(OBJ, 1)) {
            case 0:
              send_to_char("MPCP Rating: ", CH);
              break;
            case 1:
              send_to_char("Active memory increase: ", CH);
              break;
            case 2:
              send_to_char("Storage memory increase: ", CH);
              break;
            case 3:
              iedit_disp_menu(d);
              break;
            case 4:
              send_to_char("I/O speed increase: ", CH);
              break;
            case 5:
              send_to_char("Reaction increase level: ", CH);
              break;
          }
          break;
        case TYPE_COMPUTER:
          send_to_char("Storage Memory: ", CH);
          break;
        default:
          iedit_disp_menu(d);
          break;
      }
      break;
    case ITEM_GUN_ACCESSORY:
      if (GET_OBJ_VAL(OBJ, 1) == 1)
        send_to_char("Smartlink Rating: ", CH);
      else if (GET_OBJ_VAL(OBJ, 1) == 3)
        send_to_char("Recoil modifier: ", CH);
      else
        iedit_disp_menu(d);
      break;
    case ITEM_RADIO:
      send_to_char("Enter encryption/decryption rating (1-6): ", CH);
      break;
    case ITEM_MONEY:
      if (GET_OBJ_VAL(OBJ, 1))
        send_to_char("  1) 6-digit code\r\n  2) thumbprint\r\n  3) retinal scan\r\n"
                     "Enter credstick security: ", d->character);
      else
        iedit_disp_menu(d);
      break;
    case ITEM_MOD:
      switch (GET_VEHICLE_MOD_TYPE(OBJ)) {
        case TYPE_MOUNT:
          // Mounts don't have ratings. Set it to 1.
          GET_VEHICLE_MOD_RATING(OBJ) = 1;
          // Skipping this field? Re-increment our counter.
          if (d->iedit_limit_edits)
            d->iedit_limit_edits++;
          iedit_disp_val4_menu(d);
          return;
        case TYPE_ENGINECUST:
          CLS(CH);
          for (int engine_type = ENGINE_ELECTRIC; engine_type <= ENGINE_DIESEL; engine_type++)
            send_to_char(CH, " %d) %s\r\n", engine_type, engine_types[engine_type]);
          send_to_char("Engine type: ", CH);
          break;
        case TYPE_MISC:
          send_to_char("Radio Range (0-5): ", CH);
          break;
        default:
          send_to_char("Rating: ", CH);
          break;
      }
      break;
    case ITEM_WORN:
      // Skipping this field while doing nothing? Re-increment our counter.
      if (d->iedit_limit_edits)
        d->iedit_limit_edits++;
      iedit_disp_val4_menu(d);
      //send_to_char("Space for grenades: ", CH);
      return;
    default:
      iedit_disp_menu(d);
  }
}

/* object value 4 */
void iedit_disp_val4_menu(struct descriptor_data * d)
{
  if (d->iedit_limit_edits > 0) {
    // Cut out when the decremented counter hits zero.
    if (--d->iedit_limit_edits == 0) {
      iedit_disp_menu(d);
      return;
    }
  }
  d->edit_mode = IEDIT_VALUE_4;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WEAPON:
    case ITEM_FIREWEAPON:
      iedit_disp_weapon_menu(d);
      break;
    case ITEM_CONTAINER:
      send_to_char("Lock level on container: ", d->character);
      break;
    case ITEM_DRINKCON:
    case ITEM_FOUNTAIN:
    case ITEM_FOOD:
      send_to_char("Poison Damage (0 = none, 1 = Light, 2 = Moderate, 3 = Serious, 4 = Deadly): ", d->character);
      break;
    case ITEM_CYBERWARE:
      switch (GET_OBJ_VAL(OBJ, 0)) {
        case CYB_DATAJACK:
          send_to_char("0) Standard\r\n1) Induction\r\nEnter Datajack Type: ", CH);
          break;
        case CYB_CHIPJACK:
          send_to_char("Slots for chips: ", CH);
          break;
        case CYB_MEMORY:
          send_to_char("Size in MP: ", CH);
          break;
        case CYB_TOOTHCOMPARTMENT:
          send_to_char("0) Standard\r\n1) Breakable\r\nEnter Tooth Compartment Type: ", CH);
          break;
        case CYB_HANDRAZOR:
          sprintbit(GET_OBJ_VAL(OBJ, 3), hand_razors, buf1, sizeof(buf1));
          send_to_char(CH, "1) Retractable\r\n2) Improved\r\nCurrent Flags: ^c%s^n\r\nEnter options (0 to quit): ", buf1);
          break;
        case CYB_HANDBLADE:
        case CYB_HANDSPUR:
        case CYB_CLIMBINGCLAWS:
        case CYB_FANGS:
        case CYB_HORNS:
          send_to_char("Retractable? (1 for Yes, 0 for No): ", CH);
          break;
        case CYB_BONELACING:
          for (int x = 0; x < 5; x++)
            send_to_char(CH, "%d) %s\r\n", x, bone_lacing[x]);
          send_to_char("Enter type of lacing: ", CH);
          break;
        case CYB_REFLEXTRIGGER:
          send_to_char("0) Normal\r\n1) Stepped\r\n", CH);
          break;
        case CYB_FILTRATION:
          send_to_char("0) Air\r\n1) Blood\r\n2) Ingested\r\nEnter type of filtration: ", CH);
          break;
        case CYB_CRANIALCYBER:
          send_to_char("Vnum of cyberdeck to be included in headware (0 to quit): ", CH);
          break;
        case CYB_EYES:
          iedit_disp_cybereyes_menu(d);
          break;
        case CYB_RADIO:
          send_to_char("Enter radio range: ", CH);
          break;
        case CYB_DERMALSHEATHING:
          send_to_char("Ruthenium Coated (1 for Yes 0 For No): ", CH);
          break;
        case CYB_ARMS:
          iedit_disp_cyberarms_menu(d);
          break;
        case CYB_LEGS:
          iedit_disp_cyberlegs_menu(d);
          break;
        case CYB_SKULL:
          iedit_disp_cyberskull_menu(d);
          break;
        case CYB_TORSO:
          iedit_disp_cybertorso_menu(d);
          break;
        case CYB_SKILLWIRE:
          send_to_char("Enter MP limit of skillwires: ", CH);
          break;
        default:
          iedit_disp_menu(d);
          return;
      }
      break;
    case ITEM_CYBERDECK:
      send_to_char("Storage: ", CH);
      break;
    case ITEM_DECK_ACCESSORY:
      switch (GET_OBJ_VAL(OBJ, 0)) {
        case TYPE_FILE:
          iedit_disp_menu(d);
          break;
        case TYPE_UPGRADE:
          if (GET_OBJ_VAL(OBJ, 1) == 5)
            send_to_char("MPCP to enhance: ", CH);
          else
            iedit_disp_menu(d);
          break;
        default:
          iedit_disp_menu(d);
          return;
      }
      break;
    case ITEM_PROGRAM:
      if (GET_OBJ_VAL(d->edit_obj, 0) == SOFT_ATTACK)
        send_to_char("Damage Rating (1-Light 2-Moderate 3-Serious 4-Deadly): ", CH);
      else
        iedit_disp_menu(d);
      break;
    case ITEM_WORN:
      // Skipping this field while doing nothing? Re-increment our counter.
      if (d->iedit_limit_edits)
        d->iedit_limit_edits++;
      iedit_disp_val5_menu(d);
      //send_to_char("Space for shuriken: ", CH);
      return;
    case ITEM_MOD:
      if (GET_OBJ_VAL(d->edit_obj, 0) == MOD_RADIO)
        send_to_char("Crypt Level (0-6): ", CH);
      else {
        // Skipping this field while doing nothing? Re-increment our counter.
        if (d->iedit_limit_edits)
          d->iedit_limit_edits++;
        iedit_disp_val5_menu(d);
        return;
      }
      break;
    default:
      iedit_disp_menu(d);
  }
}

/* object value 5 */
void iedit_disp_val5_menu(struct descriptor_data * d)
{
  if (d->iedit_limit_edits > 0) {
    // Cut out when the decremented counter hits zero.
    if (--d->iedit_limit_edits == 0) {
      iedit_disp_menu(d);
      return;
    }
  }
  d->edit_mode = IEDIT_VALUE_5;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WEAPON:
    case ITEM_FIREWEAPON:
      if (GET_WEAPON_ATTACK_TYPE(d->edit_obj) == WEAP_GRENADE) {
        // Skipping this field while doing nothing? Re-increment our counter.
        if (d->iedit_limit_edits)
          d->iedit_limit_edits++;
        iedit_disp_val6_menu(d);
      }
      iedit_disp_skill_menu(d);
      break;
    case ITEM_CYBERDECK:
      send_to_char("Load: ", CH);
      break;
    case ITEM_WORN:
      send_to_char("Space for Misc small items: ", CH);
      break;
    case ITEM_MOD:
      if (GET_OBJ_VAL(d->edit_obj, 0) == TYPE_MOUNT) {
        // Mounts automatically fit both types of vehicle.
        GET_VEHICLE_MOD_DESIGNED_FOR_FLAGS(d->edit_obj) = -1;
        // Skipping this field while doing nothing? Re-increment our counter.
        if (d->iedit_limit_edits)
          d->iedit_limit_edits++;
        iedit_disp_val6_menu(d);
        return;
      }
      iedit_disp_mod_vehicle_type_menu(d);
      break;
    default:
      iedit_disp_menu(d);
  }
}

/* object value 6 */
void iedit_disp_val6_menu(struct descriptor_data * d)
{
  if (d->iedit_limit_edits > 0) {
    // Cut out when the decremented counter hits zero.
    if (--d->iedit_limit_edits == 0) {
      iedit_disp_menu(d);
      return;
    }
  }
  d->edit_mode = IEDIT_VALUE_6;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WEAPON:
      if (GET_WEAPON_ATTACK_TYPE(d->edit_obj) == WEAP_GRENADE) {
        // Skipping this field while doing nothing? Re-increment our counter.
        if (d->iedit_limit_edits)
          d->iedit_limit_edits++;
        iedit_disp_val7_menu(d);
      }
      send_to_char("Max Ammo (-1 if doesn't use ammo): ", CH);
      break;
    case ITEM_FIREWEAPON:
      send_to_char("  0) Bow\r\n  1) Crossbow\r\nType: ", CH);
      break;
    case ITEM_WORN:
      send_to_char("Ballistic Rating: ", CH);
      break;
    case ITEM_MOD:
      if (GET_OBJ_VAL(d->edit_obj, 0) == TYPE_MOUNT) {
        // Mounts automatically fit all engine classes.
        for (int number = ENGINE_ELECTRIC; number < NUM_ENGINE_TYPES; number++) {
          TOGGLE_BIT(GET_OBJ_VAL(OBJ, 5), 1 << number);
        }
        iedit_disp_menu(d);
        return;
      }
      iedit_disp_mod_engine_menu(d);
      break;
    case ITEM_CYBERDECK:
      // Skipping this field while doing nothing? Re-increment our counter.
      if (d->iedit_limit_edits)
        d->iedit_limit_edits++;
      iedit_disp_val7_menu(d);
      return;
    default:
      iedit_disp_menu(d);
  }
}

/* object value 7 */
void iedit_disp_val7_menu(struct descriptor_data * d)
{
  if (d->iedit_limit_edits > 0) {
    // Cut out when the decremented counter hits zero.
    if (--d->iedit_limit_edits == 0) {
      iedit_disp_menu(d);
      return;
    }
  }
  d->edit_mode = IEDIT_VALUE_7;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WEAPON:
      if (!IS_GUN(GET_OBJ_VAL(d->edit_obj, 3)))
        send_to_char("Reach: ", CH);
      else {
        // Skipping this field while doing nothing? Re-increment our counter.
        if (d->iedit_limit_edits)
          d->iedit_limit_edits++;
        iedit_disp_val8_menu(d);
        return;
      }
      break;
    case ITEM_FIREWEAPON:
      send_to_char("Enter strength minimum: ", CH);
      break;
    case ITEM_WORN:
      send_to_char("Impact Rating: ", CH);
      break;
    case ITEM_CYBERDECK:
      send_to_char("Response Increase: ", CH);
      break;
    case ITEM_MOD:
      if (GET_OBJ_VAL(d->edit_obj, 0) == TYPE_MOUNT) {
        // Mounts go in the mount position.
        GET_OBJ_VAL(OBJ, 6) = MOD_MOUNT;
      }
      for (int c = 1; c < NUM_MODS; c++)
        send_to_char(CH, "  %d) %s\r\n", c, mod_name[c]);
      send_to_char("Enter attachable position: ", CH);
      break;
    default:
      iedit_disp_menu(d);
  }
}

/* object value 8 */
void iedit_disp_val8_menu(struct descriptor_data * d)
{
  if (d->iedit_limit_edits > 0) {
    // Cut out when the decremented counter hits zero.
    if (--d->iedit_limit_edits == 0) {
      iedit_disp_menu(d);
      return;
    }
  }
  d->edit_mode = IEDIT_VALUE_8;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WEAPON:
      if (IS_GUN(GET_WEAPON_ATTACK_TYPE(OBJ))) {
        if (GET_WEAPON_ATTACH_TOP_VNUM(OBJ) >= -1)
          send_to_char("Enter vnum of object to attach on top: ", CH);
        else {
          // Skipping this field while doing nothing? Re-increment our counter.
          if (d->iedit_limit_edits)
            d->iedit_limit_edits++;
          iedit_disp_val9_menu(d);
          return;
        }
      } else if (GET_WEAPON_ATTACK_TYPE(OBJ) == WEAP_GRENADE) {
        send_to_char("Is this an IPE (-3) or a Flashbang (-4) grenade?: ", CH);
      } else {
        send_to_char("Enter weapon focus rating (0 for no focus, up to 4): ", CH);
      }
      break;
    case ITEM_WORN:
      send_to_char("Concealability rating (+harder -easier): ", CH);
      break;
    default:
      iedit_disp_menu(d);
  }
}

/* object value 9 */
void iedit_disp_val9_menu(struct descriptor_data * d)
{
  if (d->iedit_limit_edits > 0) {
    // Cut out when the decremented counter hits zero.
    if (--d->iedit_limit_edits == 0) {
      iedit_disp_menu(d);
      return;
    }
  }
  d->edit_mode = IEDIT_VALUE_9;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WORN:
      send_to_char("Armored clothing set (0 for no set): ", CH);
      break;
    case ITEM_WEAPON:
      if (IS_GUN(GET_WEAPON_ATTACK_TYPE(OBJ))) {
        if (GET_WEAPON_ATTACH_BARREL_VNUM(OBJ) >= -1)
          send_to_char("Enter vnum of object to attach on barrel: ", CH);
        else {
          // Skipping this field while doing nothing? Re-increment our counter.
          if (d->iedit_limit_edits)
            d->iedit_limit_edits++;
          iedit_disp_val10_menu(d);
          return;
        }
      } else
        iedit_disp_menu(d);
      break;
    default:
      iedit_disp_menu(d);
  }
}

/* object value 10 */
void iedit_disp_val10_menu(struct descriptor_data * d)
{
  if (d->iedit_limit_edits > 0) {
    // Cut out when the decremented counter hits zero.
    if (--d->iedit_limit_edits == 0) {
      iedit_disp_menu(d);
      return;
    }
  }
  d->edit_mode = IEDIT_VALUE_10;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WEAPON:
      if (GET_WEAPON_ATTACH_UNDER_VNUM(OBJ) >= -1)
        send_to_char("Enter vnum of object to attach underneath: ", CH);
      else {
        // Skipping this field while doing nothing? Re-increment our counter.
        if (d->iedit_limit_edits)
          d->iedit_limit_edits++;
        iedit_disp_val11_menu(d);
        return;
      }
      break;
    default:
      iedit_disp_menu(d);
  }
}

void iedit_disp_val11_menu(struct descriptor_data * d)
{
  if (d->iedit_limit_edits > 0) {
    // Cut out when the decremented counter hits zero.
    if (--d->iedit_limit_edits == 0) {
      iedit_disp_menu(d);
      return;
    }
  }
  d->edit_mode = IEDIT_VALUE_11;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WEAPON:
      iedit_disp_firemodes_menu(d);
      break;
    default:
      iedit_disp_menu(d);
  }
}

void iedit_disp_val12_menu(struct descriptor_data * d)
{
  if (d->iedit_limit_edits > 0) {
    // Cut out when the decremented counter hits zero.
    if (--d->iedit_limit_edits == 0) {
      iedit_disp_menu(d);
      return;
    }
  }
  d->edit_mode = IEDIT_VALUE_12;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WEAPON:
      send_to_char("Enter the amount of built-in recoil compensation this weapon has (negative numbers add recoil): ", CH);
      break;
    default:
      iedit_disp_menu(d);
  }
}

/* object type */
void iedit_disp_type_menu(struct descriptor_data * d)
{
  CLS(CH);
  for (int counter = 1; counter < NUM_ITEMS; counter++)
  {
    send_to_char(CH, "%s%2d) %-20s%s",
                 counter % 2 == 0 ? " " : "",
                 counter,
                 item_types[counter],
                 (counter % 2 == 0 || counter == NUM_ITEMS - 1) ? "\r\n" : ""
                );
  }
  send_to_char("Enter object type (0 to quit): ", d->character);
}

void iedit_disp_drinktype_menu(struct descriptor_data *d)
{
  int counter;

  CLS(CH);
  for (counter = 0; counter < NUM_DRINK_TYPES; ++counter)
    send_to_char(CH, "%2d) %s\r\n", counter + 1, drinks[counter]);
  send_to_char("Enter drink type: ", CH);
}

void iedit_disp_material_menu(struct descriptor_data *d)
{
  CLS(CH);

  for (int counter = 0; counter < NUM_MATERIALS; ++counter)
    send_to_char(CH, "%2d) %s\r\n", counter + 1, material_names[counter]);
  send_to_char("Enter material type, 0 to return: ", CH);
}

void iedit_disp_patch_menu(struct descriptor_data *d)
{
  CLS(CH);

  for (int counter = 0; counter < NUM_PATCHES; ++counter)
    send_to_char(CH, "%2d) %s\r\n", counter + 1, patch_names[counter]);
  send_to_char("Enter patch type, 0 to return: ", CH);
}

void iedit_disp_spell_type(struct descriptor_data *d)
{
  CLS(CH);
  for (int counter = 0; counter < MAX_SPELLS; counter += 2)
  {
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n",
                 counter + 1, spells[counter].name,
                 counter + 2, counter + 1 < MAX_SPELLS ?
                 spells[counter + 1].name : "");
  }
  send_to_char("Enter spell, 0 to return: ", CH);
}

void iedit_disp_cybertype_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int counter = 0; counter < NUM_CYBER; counter += 2)
  {
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n",
                 counter + 1, cyber_types[counter],
                 counter + 2, counter + 1 < NUM_CYBER ?
                 cyber_types[counter + 1] : "");
  }
  send_to_char("Enter cyberware type: ", CH);
}

void iedit_disp_biotype_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int counter = 0; counter < NUM_BIOWARE; counter += 2)
  {
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n",
                 counter + 1, bio_types[counter],
                 counter + 2, counter + 1 < NUM_BIOWARE ?
                 bio_types[counter + 1] : "");
  }
  send_to_char("Enter bioware type, 0 to return: ", CH);
}

void iedit_program_types_menu(struct descriptor_data *d)
{
  int counter;

  CLS(CH);
  for (counter = 1; counter < NUM_PROGRAMS; counter += 3)
  {
    send_to_char(CH, "  %2d) %-15s %2d) %-15s %2d) %-15s\r\n",
                 counter, programs[counter].name,
                 counter + 1, counter + 1 < NUM_PROGRAMS ?
                 programs[counter + 1].name : "", counter + 2, counter + 2 < NUM_PROGRAMS ?
                 programs[counter + 2].name : "");
  }
  send_to_char("Program type: ", d->character);
}

void iedit_disp_rating_menu(struct descriptor_data *d)
{
  CLS(CH);

  for (int counter = 0; counter < NUM_BARRIERS; ++counter)
    send_to_char(CH, "%2d) %s\r\n", counter + 1, barrier_names[counter]);
  send_to_char("Enter construction category, 0 to return: ", CH);
}

/* object extra flags */
void iedit_disp_extra_menu(struct descriptor_data * d)
{
  int             counter;

  CLS(CH);
  for (counter = 0; counter < MAX_ITEM_EXTRA; counter += 2)
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n",
                 counter + 1, extra_bits[counter],
                 counter + 2, counter + 1 < MAX_ITEM_EXTRA ?
                 extra_bits[counter + 1] : "");

  GET_OBJ_EXTRA(d->edit_obj).PrintBits(buf1, MAX_STRING_LENGTH,
                                       extra_bits, MAX_ITEM_EXTRA);
  send_to_char(CH, "Object flags: %s%s%s\r\n"
               "Enter object extra flag, 0 to quit: ",
               CCCYN(CH, C_CMP), buf1, CCNRM(CH, C_CMP));
}

void iedit_disp_aff_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int counter = 0; counter < AFF_MAX; counter += 2)
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n", counter + 1,
                 affected_bits[counter], counter + 2, counter + 1 < AFF_MAX ?
                 affected_bits[counter + 1] : "");

  OBJ->obj_flags.bitvector.PrintBits(buf1, MAX_STRING_LENGTH,
                                     affected_bits, AFF_MAX);
  send_to_char(CH, "Object flags: %s%s%s\r\n"
               "Enter affected flags, 0 to quit: ", CCCYN(CH, C_CMP),
               buf1, CCNRM(CH, C_CMP));
}

/* object wear flags */
void iedit_disp_wear_menu(struct descriptor_data * d)
{
  CLS(CH);
  for (int counter = 0; counter < ITEM_WEAR_MAX; counter += 2) {
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n",
                 counter + 1, wear_bits[counter],
                 counter + 2, counter + 1 < ITEM_WEAR_MAX ? wear_bits[counter + 1] : "");
  }

  GET_OBJ_WEAR(d->edit_obj).PrintBits(buf1, MAX_STRING_LENGTH,
                                      wear_bits, ITEM_WEAR_MAX);
  send_to_char(CH, "Wear flags: %s%s%s\r\n"
               "Enter wear flag, 0 to quit:", CCCYN(CH, C_CMP),
               buf1, CCNRM(CH, C_CMP));
}

void iedit_disp_legality_menu(struct descriptor_data *d) {
  CLS(CH);
  if (d->edit_mode == IEDIT_LEGAL1) {
    send_to_char("1) Small Blade                              (A)\r\n"
                 "2) Large Blade                              (B)\r\n"
                 "3) Blunt Weapon                             (C)\r\n"
                 "4) Projectile                               (D)\r\n"
                 "5) Pistol                                   (E)\r\n"
                 "6) Rifle                                    (F)\r\n"
                 "7) Automatic Weapon                         (G)\r\n"
                 "8) Heavy Weapon                             (H)\r\n"
                 "9) Explosive                                (J)\r\n"
                 "10) Military Weapon                         (K)\r\n"
                 "11) Military Armor                          (L)\r\n"
                 "12) Military Ammunition                     (M)\r\n"
                 "13) Class A (Paralegal) Cyberware           (N)\r\n"
                 "14) Class B (Security Grade) Cyberware      (Q)\r\n"
                 "15) Class C (Military Grade) Cyberware      (R)\r\n"
                 "16) Class D Matrix                          (S)\r\n"
                 "17) Class E Magic                           (T)\r\n"
                 "18) Class A (Paralegal) Equipment           (U)\r\n"
                 "19) Class B (Security Grade) Equipment      (V)\r\n"
                 "20) Class C (Military Grade) Equipment      (W)\r\n"
                 "21) Class A (Pharmaceuticals) Controlled    (X)\r\n"
                 "22) Class B (BTL) Controlled                (Y)\r\n"
                 "23) Class C (Biological Agents) Controlled  (Z)\r\n"
                 "\r\nSelect Security Category (0 for other): ", CH);
    d->edit_mode = IEDIT_LEGAL2;
  } else if (d->edit_mode == IEDIT_LEGAL2) {
    send_to_char("Permit available (0 for no, 1 for yes): ", CH);
    d->edit_mode = IEDIT_LEGAL3;
  } else {
    send_to_char("Enter legality TN (0 for legal): ", CH);
    d->edit_mode = IEDIT_LEGAL1;
  }
}
/* display main menu */
void iedit_disp_menu(struct descriptor_data * d)
{
  CLS(CH);
  if (IS_OBJ_STAT(OBJ, ITEM_EXTRA_DONT_TOUCH)) {
    send_to_char(CH, "^RThis is a templated object. You are unable to edit it while the game is running.\r\n");
  }
  send_to_char(CH, "Item number: ^c%d^n\r\n", d->edit_number);
  send_to_char(CH, "1) Item keywords: ^c%s^n\r\n", d->edit_obj->text.keywords);
  send_to_char(CH, "2) Item name: ^c%s^n\r\n", d->edit_obj->text.name);
  send_to_char(CH, "3) Room description (terminal color code unnecessary):\r\n^g%s^n\r\n", d->edit_obj->text.room_desc);
  send_to_char(CH, "4) Look description: \r\n%s\r\n",
               d->edit_obj->text.look_desc ? d->edit_obj->text.look_desc :
               "(not set)");
  sprinttype(GET_OBJ_TYPE(d->edit_obj), item_types, buf1, sizeof(buf1));
  send_to_char(CH, "5) Item type: ^c%s^n\r\n", buf1);
  GET_OBJ_EXTRA(d->edit_obj).PrintBits(buf1, MAX_STRING_LENGTH,
                                       extra_bits, MAX_ITEM_EXTRA);
  send_to_char(CH, "6) Item extra flags: ^c%s^n\r\n", buf1);
  OBJ->obj_flags.bitvector.PrintBits(buf1, MAX_STRING_LENGTH,
                                     affected_bits, AFF_MAX);
  send_to_char(CH, "7) Item affection flags: ^c%s^n\r\n", buf1);
  OBJ->obj_flags.wear_flags.PrintBits(buf1, MAX_STRING_LENGTH,
                                      wear_bits, NUM_WEARS);
  send_to_char(CH, "8) Item wear flags: ^c%s^n\r\n", buf1);
  send_to_char(CH, "9) Item weight: ^c%.2f^n\r\n", GET_OBJ_WEIGHT(d->edit_obj));
  send_to_char(CH, "a) Item cost: ^c%d^n\r\n", GET_OBJ_COST(d->edit_obj));
  int availhrs = GET_OBJ_AVAILDAY(d->edit_obj) * 24;
  if (availhrs <= 48) {
  send_to_char(CH, "b) Item availability: ^c%d^n/^c%d hour%s^n\r\n",
               GET_OBJ_AVAILTN(d->edit_obj), availhrs, availhrs > 1 ? "s" : "");
  } else {
    send_to_char(CH, "b) Item availability: ^c%d^n/^c%.2f day%s^n\r\n",
                 GET_OBJ_AVAILTN(d->edit_obj), GET_OBJ_AVAILDAY(d->edit_obj), GET_OBJ_AVAILDAY(d->edit_obj) > 1 ? "s" : "s");
  }
  send_to_char(CH, "c) Item timer: ^c%d^n\r\n", GET_OBJ_TIMER(d->edit_obj));
  send_to_char(CH, "d) Item Material: ^c%s^n\r\n", material_names[(int)GET_OBJ_MATERIAL(d->edit_obj)]);
  send_to_char(CH, "e) Item Barrier Rating: ^c%d^n\r\n", GET_OBJ_BARRIER(d->edit_obj));
  send_to_char(CH, "f) Item Legality: ^c%d%s^n-^c%s^n\r\n", GET_LEGAL_NUM(d->edit_obj), GET_LEGAL_PERMIT(d->edit_obj) ? "P" : "",
               legality_codes[GET_LEGAL_CODE(d->edit_obj)][0]);
  if (GET_OBJ_TYPE(d->edit_obj) == ITEM_WEAPON)
    send_to_char(CH, "g) Item values: ^c%d %d %d %d %d %d %d %d %d %d %d %d (%d)^n\r\n",
    GET_OBJ_VAL(d->edit_obj, 0), GET_OBJ_VAL(d->edit_obj, 1),
    GET_OBJ_VAL(d->edit_obj, 2), GET_OBJ_VAL(d->edit_obj, 3),
    GET_OBJ_VAL(d->edit_obj, 4), GET_OBJ_VAL(d->edit_obj, 5),
    GET_OBJ_VAL(d->edit_obj, 6), GET_OBJ_VAL(d->edit_obj, 7),
    GET_OBJ_VAL(d->edit_obj, 8), GET_OBJ_VAL(d->edit_obj, 9),
    GET_OBJ_VAL(d->edit_obj, 10), GET_OBJ_VAL(d->edit_obj, 11),
    GET_WEAPON_INTEGRAL_RECOIL_COMP(d->edit_obj));
  else
    send_to_char(CH, "g) Item values: ^c%d %d %d %d %d %d %d %d %d %d %d %d^n\r\n",
                 GET_OBJ_VAL(d->edit_obj, 0), GET_OBJ_VAL(d->edit_obj, 1),
                 GET_OBJ_VAL(d->edit_obj, 2), GET_OBJ_VAL(d->edit_obj, 3),
                 GET_OBJ_VAL(d->edit_obj, 4), GET_OBJ_VAL(d->edit_obj, 5),
                 GET_OBJ_VAL(d->edit_obj, 6), GET_OBJ_VAL(d->edit_obj, 7),
                 GET_OBJ_VAL(d->edit_obj, 8), GET_OBJ_VAL(d->edit_obj, 9),
                 GET_OBJ_VAL(d->edit_obj, 10), GET_OBJ_VAL(d->edit_obj, 11));
  send_to_char(CH, "h) Item applies:\r\n"
               "i) Item extra descriptions:\r\n"
               "j) Source book: ^c%s^n\r\n"
               "k) Street index: ^c%.2f\r\n^n",  d->edit_obj->source_info ? d->edit_obj->source_info : "<none>", GET_OBJ_STREET_INDEX(d->edit_obj));
  if (!IS_OBJ_STAT(OBJ, ITEM_EXTRA_DONT_TOUCH)) {
    send_to_char(CH, "q) Quit and save\r\n");
  }
  send_to_char(CH, "x) Exit and abort\r\n"
               "Enter your choice:\r\n");
  d->edit_mode = IEDIT_MAIN_MENU;
  if (IS_OBJ_STAT(OBJ, ITEM_EXTRA_DONT_TOUCH)) {
    send_to_char(CH, "^RThis is a templated object. You are unable to edit it while the game is running.\r\n");
  }
}

/***************************************************************************
 main loop (of sorts).. basically interpreter throws all input to here
 ***************************************************************************/


void iedit_parse(struct descriptor_data * d, const char *arg)
{
  long             number, j;
  long            obj_number;   /* the RNUM */
  float fnumber;
  int real_obj;
  bool modified = FALSE;
  switch (d->edit_mode)
  {

    case IEDIT_CONFIRM_EDIT:
      /* if player hits 'Y' then edit obj */
      switch (*arg) {
        case 'y':
        case 'Y':
          iedit_disp_menu(d);
          break;
        case 'n':
        case 'N':
          STATE(d) = CON_PLAYING;
          /* free up the editing object */
          if (d->edit_obj)
            Mem->DeleteObject(d->edit_obj);
          d->edit_obj = NULL;
          d->edit_number = 0;
          PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
          break;
        default:
          send_to_char("That's not a valid choice!\r\n", d->character);
          send_to_char("Do you wish to edit it?\r\n", d->character);
          break;
      }
      break;                      /* end of IEDIT_CONFIRM_EDIT */

    case IEDIT_CONFIRM_SAVESTRING:
      switch (*arg) {
        case 'y':
        case 'Y': {
          /* write to internal tables */
#ifdef ONLY_LOG_BUILD_ACTIONS_ON_CONNECTED_ZONES
          if (!vnum_from_non_connected_zone(d->edit_number)) {
#else
          {
#endif
            snprintf(buf, sizeof(buf),"%s wrote new obj #%ld",
                    GET_CHAR_NAME(d->character), d->edit_number);
            mudlog(buf, d->character, LOG_WIZLOG, TRUE);
          }

          obj_number = real_object(d->edit_number);
          if (obj_number > 0) {
            /* we need to run through each and every object currently in the
             * game to see which ones are pointing to this prototype */
            struct extra_descr_data *This, *next_one;

            /* if object is pointing to this prototype, then we need to replace
             * with the new one */
            // this function updates pointers to the active list of objects
            // in the mud
            ObjList.UpdateObjs(d->edit_obj, obj_number);
            /* now safe to free old proto and write over */
            DELETE_ARRAY_IF_EXTANT(obj_proto[obj_number].text.keywords);
            DELETE_ARRAY_IF_EXTANT(obj_proto[obj_number].text.name);
            DELETE_ARRAY_IF_EXTANT(obj_proto[obj_number].text.room_desc);
            DELETE_ARRAY_IF_EXTANT(obj_proto[obj_number].text.look_desc);

            if (obj_proto[obj_number].ex_description) {
              for (This = obj_proto[obj_number].ex_description; This; This = next_one) {
                next_one = This->next;
                DELETE_ARRAY_IF_EXTANT(This->keyword);
                DELETE_ARRAY_IF_EXTANT(This->description);
                DELETE_AND_NULL(This);
              }
              obj_proto[obj_number].ex_description = NULL;
            }

            obj_proto[obj_number] = *d->edit_obj;
            obj_proto[obj_number].item_number = obj_number;
          } else {
            /* uhoh.. need to make a new place in the object prototype table */
            long             counter;
            int             found = FALSE;

            struct obj_data *new_obj_proto;
            struct index_data *new_obj_index;

            /* + 2.. strange but true */
            new_obj_index = new struct index_data[top_of_objt + 2];
            new_obj_proto = new struct obj_data[top_of_objt + 2];
            /* start counting through both tables */
            for (counter = 0; counter <= top_of_objt; counter++) {
              /* if we haven't found it */
              if (!found) {
                /* check if current virtual is bigger than our virtual */
                if (OBJ_VNUM_RNUM(counter) > d->edit_number) {
                  /* eureka. insert now */
                  /*---------*/
                  new_obj_index[counter].vnum = d->edit_number;
                  new_obj_index[counter].number = 0;
                  new_obj_index[counter].func = NULL;
                  /*---------*/
                  new_obj_proto[counter] = *(d->edit_obj);
                  new_obj_proto[counter].in_room = NULL;
                  /* it is now safe (and necessary!) to assign real number to
                   * the edit_obj, which has been -1 all this time */
                  d->edit_obj->item_number = counter;
                  /* and assign to prototype as well */
                  new_obj_proto[counter].item_number = counter;
                  found = TRUE;
                  /* insert the other proto at this point */
                  new_obj_index[counter + 1] = obj_index[counter];
                  new_obj_proto[counter + 1] = obj_proto[counter];
                  new_obj_proto[counter + 1].item_number = counter + 1;
                } else {
                  /* just copy from old to new, no num change */
                  new_obj_proto[counter] = obj_proto[counter];
                  new_obj_index[counter] = obj_index[counter];
                }
              } else {
                /* we HAVE already found it.. therefore copy to object + 1 */
                new_obj_index[counter + 1] = obj_index[counter];
                new_obj_proto[counter + 1] = obj_proto[counter];
                new_obj_proto[counter + 1].item_number = counter + 1;
              }
            }
            /* if we STILL haven't found it, means the object was > than all
             * the other objects.. so insert at end */
            if (!found) {
              new_obj_index[top_of_objt + 1].vnum = d->edit_number;
              new_obj_index[top_of_objt + 1].number = 0;
              new_obj_index[top_of_objt + 1].func = NULL;

              clear_object(new_obj_proto + top_of_objt + 1);
              new_obj_proto[top_of_objt + 1] = *(d->edit_obj);
              new_obj_proto[top_of_objt + 1].in_room = NULL;
              new_obj_proto[top_of_objt + 1].item_number = top_of_objt + 1;
              /* it is now safe (and necessary!) to assign real number to
               * the edit_obj, which has been -1 all this time */
              d->edit_obj->item_number = top_of_objt + 1;
            }
            top_of_objt++;


            /* we also have to renumber all the objects currently
             existing in the world. This is because when I start
             extracting objects, bad things will happen! */

            ObjList.UpdateNums(d->edit_obj->item_number);

            /* free and replace old tables */
            DELETE_AND_NULL_ARRAY(obj_proto);
            DELETE_AND_NULL_ARRAY(obj_index);
            obj_proto = new_obj_proto;
            obj_index = new_obj_index;

            /* RENUMBER ZONE TABLES HERE, only
             because I ADDED an object!
             This code shamelessly ripped off from db.c */
            {
              #define UPDATE_VALUE(value) {(value) = ((value) >= d->edit_obj->item_number ? (value) + 1 : (value));}
              for (int zone = 0; zone <= top_of_zone_table; zone++) {
                for (int cmd_no = 0; cmd_no < zone_table[zone].num_cmds; cmd_no++) {
                  switch (ZCMD.command) {
                    case 'G':
                    case 'O':
                    case 'E':
                    case 'C':
                    case 'U':
                    case 'I':
                    case 'H':
                    case 'N':
                      UPDATE_VALUE(ZCMD.arg1);
                      break;
                    case 'P':
                      UPDATE_VALUE(ZCMD.arg1);
                      UPDATE_VALUE(ZCMD.arg3);
                      break;
                    case 'R': /* rem obj from room */
                      UPDATE_VALUE(ZCMD.arg2);
                      break;
                  }
                }
              }
              #undef UPDATE_VALUE
            }
            /* this fixes the creeping board problems */
            // not needed since real nums are not used with boards anymore
            /*        realcounter = real_object (d->edit_number);
             for (counter = 0; counter < NUM_OF_BOARDS; counter++)
             {
             if (BOARD_RNUM (counter) >= realcounter)
             BOARD_RNUM (counter) = BOARD_RNUM (counter) + 1;
             } */
          }  /* end of obj insertion */

          send_to_char("Writing object to disk..", d->character);
          write_objs_to_disk(d->character->player_specials->saved.zonenum);
          // do not wanna nuke the strings, so we use ClearObject first
          if (d->edit_obj) {
            clear_object(d->edit_obj);
            delete d->edit_obj;
          }
          d->edit_obj = NULL;
          STATE(d) = CON_PLAYING;
          PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
          send_to_char("Done.\r\n", d->character);
        }
          break;
        case 'n':
        case 'N':
          send_to_char("Object not saved, aborting.\r\n", d->character);
          STATE(d) = CON_PLAYING;
          /* free up the editing object. free_obj *is* safe since
           it checks against prototype table */
          if (d->edit_obj)
            Mem->DeleteObject(d->edit_obj);
          d->edit_obj = NULL;
          d->edit_number = 0;
          PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
          break;
        default:
          send_to_char("Invalid choice!\r\n", d->character);
          send_to_char("Do you wish to save this object internally?\r\n", d->character);
          break;
      }
      break;                      /* end of IEDIT_CONFIRM_SAVESTRING */

    case IEDIT_MAIN_MENU:
      /* throw us out to whichever edit mode based on user input */
      switch (*arg) {
        case 'q':
        case 'Q':
          if (IS_OBJ_STAT(OBJ, ITEM_EXTRA_DONT_TOUCH)) {
            send_to_char("You can't save this object! Edit it directly in the world files.\r\n", d->character);
            break;
          }
          d->edit_mode = IEDIT_CONFIRM_SAVESTRING;
          iedit_parse(d, "y");
          break;
        case 'x':
        case 'X':
          d->edit_mode = IEDIT_CONFIRM_SAVESTRING;
          iedit_parse(d, "n");
          break;
        case '1':
          send_to_char("Enter namelist:", d->character);
          d->edit_mode = IEDIT_EDIT_NAMELIST;
          break;
        case '2':
          send_to_char("Enter short desc:", d->character);
          d->edit_mode = IEDIT_SHORTDESC;
          break;
        case '3':
          send_to_char("Enter normal desc:\r\n", d->character);
          d->edit_mode = IEDIT_DESC;
          break;
        case '4':
          /* let's go out to modify.c */
          send_to_char("Enter long desc:\r\n", d->character);
          d->edit_mode = IEDIT_LONGDESC;
          DELETE_D_STR_IF_EXTANT(d);
          INITIALIZE_NEW_D_STR(d);
          d->max_str = MAX_MESSAGE_LENGTH;
          d->mail_to = 0;
          break;
        case '5':
          iedit_disp_type_menu(d);
          d->edit_mode = IEDIT_TYPE;
          break;
        case '6':
          iedit_disp_extra_menu(d);
          d->edit_mode = IEDIT_EXTRAS;
          break;
        case '7':
          iedit_disp_aff_menu(d);
          d->edit_mode = IEDIT_AFF_BITS;
          break;
        case '8':
          iedit_disp_wear_menu(d);
          d->edit_mode = IEDIT_WEAR;
          break;
        case '9':
          if (GET_OBJ_TYPE(d->edit_obj) == ITEM_KEYRING) {
            send_to_char("Keyrings are weightless by design.\r\n", d->character);
            iedit_disp_menu(d);
          } else {
            send_to_char("Enter weight:", d->character);
            d->edit_mode = IEDIT_WEIGHT;
          }
          break;
        case 'a':
        case 'A':
          send_to_char("Enter cost:", d->character);
          d->edit_mode = IEDIT_COST;
          break;
        case 'b':
        case 'B':
          send_to_char(CH, "Enter Availability TN: ");
          d->edit_mode = IEDIT_AVAILTN;
          break;
        case 'c':
        case 'C':
          send_to_char("Enter timer:", d->character);
          d->edit_mode = IEDIT_TIMER;
          break;
        case 'd':
        case 'D':
          iedit_disp_material_menu(d);
          d->edit_mode = IEDIT_MATERIAL;
          break;
        case 'e':
        case 'E':
          iedit_disp_rating_menu(d);
          d->edit_mode = IEDIT_RATING;
          break;
        case 'f':
        case 'F':
          iedit_disp_legality_menu(d);
          break;
        case 'g':
        case 'G':
          // Check for a specific-number bypass.
          if (*(arg+1) && isdigit(*(arg+1))) {
            // Single-digit
            int bypass_num = (int)(*(arg+1) - int('0'));
            if (*(arg+2) && isdigit(*(arg+2))) {
              // Double-digit
              bypass_num = bypass_num * 10 + (int)(*(arg+2) - int('0'));
            }
            d->iedit_limit_edits = 2;
            switch (bypass_num) {
              case 0: iedit_disp_val1_menu(d); break;
              case 1: iedit_disp_val2_menu(d); break;
              case 2: iedit_disp_val3_menu(d); break;
              case 3: iedit_disp_val4_menu(d); break;
              case 4: iedit_disp_val5_menu(d); break;
              case 5: iedit_disp_val6_menu(d); break;
              case 6: iedit_disp_val7_menu(d); break;
              case 7: iedit_disp_val8_menu(d); break;
              case 8: iedit_disp_val9_menu(d); break;
              case 9: iedit_disp_val10_menu(d); break;
              case 10: iedit_disp_val11_menu(d); break;
              case 11: iedit_disp_val12_menu(d); break;
              default:
                send_to_char(d->character, "Invalid G-menu bypass specified. Must be in range G0-G11, but got G%d instead.\r\n", bypass_num);
                d->iedit_limit_edits = 0;
                break;
            }
            return;
          }
          iedit_disp_val1_menu(d);
          return;
        case 'h':
        case 'H':
          iedit_disp_prompt_apply_menu(d);
          break;
        case 'i':
        case 'I':
          /* if extra desc doesn't exist . */
          if (!d->edit_obj->ex_description) {
            d->edit_obj->ex_description = new extra_descr_data;
            memset((char *) d->edit_obj->ex_description, 0, sizeof(extra_descr_data));
          }
          /* There is a reason I need the double pointer. If at the extra desc
           * menu user presses '0' then I need to free the extra description.
           * Since it's at the end of list it's pointer must be pointing to
           * NULL.. thus the double pointer */
          d->misc_data = (void **) &(d->edit_obj->ex_description);
          iedit_disp_extradesc_menu(d);
          break;
        case 'j':
        case 'J':
          send_to_char("Enter sourcebook info (abbreviation and pages, ex: MitS p34, 37-48):", d->character);
          d->edit_mode = IEDIT_SOURCEINFO;
          break;
        case 'k':
        case 'K':
          send_to_char("Enter street index:", d->character);
          d->edit_mode = IEDIT_STREETINDEX;
          break;
        default:
          /* hm, i just realized you probably can't see this.. maybe prompt for
           * an extra RETURN. well, maybe later */
          send_to_char("That's not a valid choice!\r\n", d->character);
          iedit_disp_menu(d);
          break;
      }
      break;                      /* end of IEDIT_MAIN_MENU */

    case IEDIT_EDIT_NAMELIST:
      DELETE_ARRAY_IF_EXTANT(d->edit_obj->text.keywords);
      d->edit_obj->text.keywords = str_dup(arg);
      iedit_disp_menu(d);
      break;
    case IEDIT_SHORTDESC:
      DELETE_ARRAY_IF_EXTANT(d->edit_obj->text.name);
      d->edit_obj->text.name = str_dup(arg);
      iedit_disp_menu(d);
      break;
    case IEDIT_DESC:
      DELETE_ARRAY_IF_EXTANT(d->edit_obj->text.room_desc);
      d->edit_obj->text.room_desc = str_dup(arg);
      iedit_disp_menu(d);
      break;
    case IEDIT_LONGDESC:
      // we should not get here
      break;
    case IEDIT_SOURCEINFO:
      DELETE_ARRAY_IF_EXTANT(d->edit_obj->source_info);
      if (*arg)
        d->edit_obj->source_info = str_dup(arg);
      else
        d->edit_obj->source_info = NULL;
      iedit_disp_menu(d);
      break;
    case IEDIT_TYPE:
      number = atoi(arg);
      if (number == 0) {
        // Back out.
        iedit_disp_menu(d);
        return;
      }
      if (number < 1 || number >= NUM_ITEMS) {
        send_to_char(d->character, "That's not a valid choice! You must choose something between 1 and %d.\r\n", NUM_ITEMS - 1);
        iedit_disp_type_menu(d);
        return;
      }

      switch (number) {
        case ITEM_CYBERWARE:
        case ITEM_BIOWARE:
          if (!access_level(CH, LVL_ADMIN)) {
            send_to_char("Sorry, only Admin and higher staff members can create those.\r\n", d->character);
            iedit_disp_type_menu(d);
            return;
          }
          break;
        case ITEM_SHOPCONTAINER:
        case ITEM_VEHCONTAINER:
          if (!access_level(CH, LVL_PRESIDENT)) {
            send_to_char("Sorry, shop and vehicle containers are for code use only and can't be created.\r\n", d->character);
            iedit_disp_type_menu(d);
            return;
          }
          send_to_char("WARNING: Shop and vehicle containers are for code use only! Hope you know what you're doing...\r\n", d->character);
          break;
        case ITEM_GUN_MAGAZINE:
          send_to_char("Sorry, gun magazines are for code use only and can't be created.\r\n", d->character);
          iedit_disp_type_menu(d);
          return;
      }

      if (number != 0 && GET_OBJ_TYPE(d->edit_obj) != number) {
        GET_OBJ_TYPE(d->edit_obj) = number;
        for (int index = 0; index < NUM_VALUES; index++)
          GET_OBJ_VAL(d->edit_obj, index) = 0;
      }

      iedit_disp_menu(d);
      break;
    case IEDIT_EXTRAS:
      number = atoi(arg);
      if ((number < 0) || (number > MAX_ITEM_EXTRA)) {
        send_to_char("That's not a valid choice!\r\n", d->character);
        iedit_disp_extra_menu(d);
      } else if (number == ITEM_EXTRA_DONT_TOUCH + 1) {
        send_to_char("You can't set the Don't Touch flag here. Do it in the world files.\r\n", d->character);
        iedit_disp_extra_menu(d);
      } else if (number == ITEM_EXTRA_KEPT + 1) {
        send_to_char("You can't set the Kept flag, it's only settable by code.\r\n", d->character);
        iedit_disp_extra_menu(d);
      } else {
        /* if 0, quit */
        if (number == 0)
          iedit_disp_menu(d);
        /* if already set.. remove */
        else {
          GET_OBJ_EXTRA(d->edit_obj).ToggleBit(number-1);
          iedit_disp_extra_menu(d);
        }
      }
      break;
    case IEDIT_AFF_BITS:
      number = atoi(arg);
      if ((number > 0) && (number <= AFF_MAX)) {
        OBJ->obj_flags.bitvector.ToggleBit(number-1);
      } else if (number == 0) {
        iedit_disp_menu(d);
        return;
      }
      iedit_disp_aff_menu(d);

      break;
    case IEDIT_WEAR:
      number = atoi(arg);
      if ((number < 0) || (number > ITEM_WEAR_MAX)) {
        send_to_char("That's not a valid choice!\r\n", d->character);
        iedit_disp_wear_menu(d);
      } else {
        /* if 0, quit */
        if (number == 0)
          iedit_disp_menu(d);
        /* if already set.. remove */
        else {
          GET_OBJ_WEAR(d->edit_obj).ToggleBit(number-1);
          iedit_disp_wear_menu(d);
        }
      }
      break;
    case IEDIT_WEIGHT:
      GET_OBJ_WEIGHT(d->edit_obj) = atof(arg);
      iedit_disp_menu(d);
      break;
    case IEDIT_LEGAL1:
      GET_LEGAL_NUM(d->edit_obj) = atoi(arg);
      iedit_disp_legality_menu(d);
      break;
    case IEDIT_LEGAL2:
      number = atoi(arg);
      if (number < 0 || number > 23) {
        send_to_char("Category must be between 0 and 23. Enter Category: ", CH);
        return;
      }
      GET_LEGAL_CODE(d->edit_obj) = atoi(arg);
      iedit_disp_legality_menu(d);
      break;
    case IEDIT_LEGAL3:
      number = atoi(arg);
      if (number < 0 || number > 1) {
        send_to_char("Enter 1 or 0: ", CH);
        return;
      }
      GET_LEGAL_PERMIT(d->edit_obj) = atoi(arg);
      iedit_disp_menu(d);
      break;
    case IEDIT_AVAILTN:
      GET_OBJ_AVAILTN(d->edit_obj) = atoi(arg);
      send_to_char(CH, "Enter Availabilty Time in Days: ");
      d->edit_mode = IEDIT_AVAILDAY;
      break;
    case IEDIT_AVAILDAY:
      GET_OBJ_AVAILDAY(d->edit_obj) = atof(arg);
      iedit_disp_menu(d);
      break;
    case IEDIT_COST:
      GET_OBJ_COST(d->edit_obj) = atoi(arg);
      iedit_disp_menu(d);
      break;
    case IEDIT_TIMER:
      GET_OBJ_TIMER(d->edit_obj) = atoi(arg);
      iedit_disp_menu(d);
      break;
    case IEDIT_VALUE_1:
      /* lucky, I don't need to check any of these for outofrange values */
      number = atoi(arg);
      switch (GET_OBJ_TYPE(d->edit_obj)) {
        case ITEM_DRUG:
          if (number < 0 || number > NUM_DRUGS) {
            send_to_char(CH, "Invalid value. Enter Drug Type: ");
            return;
          }
          break;
        case ITEM_WORKSHOP:
          if (number < 0 || number > NUM_WORKSHOP_TYPES) {
            send_to_char(CH, "Invalid value. Enter Workshop Type: ");
            return;
          }
          break;
        case ITEM_CAMERA:
          if (number < 0 || number > 1) {
            send_to_char(CH,"Value must be 0 or 1: \r\n");
            return;
          }
          break;
        case ITEM_WEAPON:
          if (number < 0 || number > 50) {
            send_to_char("Value must be between 0 and 50:\r\n"
                         "Power (0 for melee weapon): ", d->character);
            return;
          }
          break;
        case ITEM_FIREWEAPON:
          if (number < 0 || number > 50) {
            send_to_char("Value must be between 0 and 50:\r\n"
                         "Power (0 for melee weapon): ", d->character);
            return;
          }
          number = 0;
          break;
        case ITEM_MISSILE:
          if (number < 0 || number > 1) {
            send_to_char("Value must be 0 or 1!\r\nMissile type: ", CH);
            return;
          }
          break;
        case ITEM_MAGIC_TOOL:
          if (number < 0 || number > 4) {
            send_to_char("Invalid Selection!\r\nEnter Type: ", CH);
            return;
          }
          break;
        case ITEM_BIOWARE:
          if (number < 1 || number > NUM_BIOWARE) {
            iedit_disp_cybertype_menu(d);
            return;
          }
          number--;
          break;
        case ITEM_CYBERWARE:
          if (number < 1 || number > NUM_CYBER) {
            iedit_disp_cybertype_menu(d);
            return;
          }
          number--;
          break;
        case ITEM_CYBERDECK:
          if (number < 1 || number > 30) {
            send_to_char("Value must be between 1 and 30.\r\nMPCP Rating: ", CH);
            return;
          }
          break;
        case ITEM_PROGRAM:
          if (number < 1 || number > 30) {
            iedit_program_types_menu(d);
            return;
          }
          break;
        case ITEM_DECK_ACCESSORY:
          if (number < 0 || number > 4) {
            send_to_char("Invalid option!  Accessory type: ", CH);
            return;
          }
          break;
        case ITEM_GUN_ACCESSORY:
          if (number < 0 || number > 2) {
            send_to_char("Invalid option!  Accessory location: ", CH);
            return;
          }
          break;
        case ITEM_DOCWAGON:
          if (number < 1 || number > 3) {
            send_to_char("Value must be between 1 and 3.  DocWagon type: ", CH);
            return;
          }
          break;
        case ITEM_PATCH:
          if (number < 1 || number > NUM_PATCHES) {
            iedit_disp_patch_menu(d);
            return;
          } else
            number--;
          break;
        case ITEM_FOCUS:
          if (number < 0 || number > FOCI_SPELL_DEFENSE) {
            send_to_char("Invalid option.\r\nFocus type: ", CH);
            return;
          }
          break;
        case ITEM_CLIMBING:
          if (number < 1 || number > 3) {
            send_to_char("Rating must range from 1 to 3.  Rating: ", CH);
            return;
          }
          break;
        case ITEM_MONEY:
          if (number < 0 || number > 25000) {
            send_to_char("Value must range between 0 and 25000 nuyen.\r\nValue: ", CH);
            return;
          }
          break;
        case ITEM_CHIP:
          if ((number < 0) || (number > MAX_SKILLS)) {
            send_to_char(CH, "Value must range between 1 and %d.\r\n", MAX_SKILLS);
            send_to_char("Enter a skill (0 to quit): ", CH);
            return;
          }
          break;
        case ITEM_SPELL_FORMULA:
          if ((number < 1) || (number > 50)) {
            send_to_char("Force must be between 1 and 50.\r\n", CH);
            return;
          }
          GET_OBJ_VAL(d->edit_obj, 3) = GET_OBJ_VAL(d->edit_obj, 4) = GET_OBJ_VAL(d->edit_obj, 5) = GET_OBJ_VAL(d->edit_obj, 6) =
          GET_OBJ_VAL(d->edit_obj, 7) = GET_OBJ_VAL(d->edit_obj, 8) = GET_OBJ_VAL(d->edit_obj, 9) = 0;
          break;
      }
      GET_OBJ_VAL(d->edit_obj, 0) = number;
      /* proceed to menu 2 */
      iedit_disp_val2_menu(d);
      break;

    case IEDIT_VALUE_2:
      /* here, I do need to check for outofrange values */
      number = atoi(arg);
      switch (GET_OBJ_TYPE(d->edit_obj)) {
        case ITEM_DRUG:
          if (number < 1 || number > MAX_DRUG_DOSE_COUNT) {
            send_to_char(CH, "You must specify a dose quantity between 1 and %d. Doses: ", MAX_DRUG_DOSE_COUNT);
            return;
          }
          break;
        case ITEM_CLIMBING:
          if (number < CLIMBING_TYPE_JUST_CLIMBING || number > CLIMBING_TYPE_WATER_WINGS) {
            send_to_char("Invalid choice. Enter 1 for water wings, 0 for climbing gear: ", CH);
            return;
          }
          break;
        case ITEM_WORKSHOP:
          if (number < 1 || number > 3) {
            send_to_char(CH, "Invalid value. Enter Type: ");
            return;
          }
          break;
        case ITEM_CHIP:
          if (number < 0 || number > 15) {
            send_to_char(CH, "Rating must be between 0 and 15. Enter skillsoft rating: ");
            return;
          }
          break;
        case ITEM_CONTAINER:
          number = atoi(arg);
          if (number < 0 || number > 4) {
            send_to_char("Invalid choice!\r\n", d->character);
            iedit_disp_container_flags_menu(d);
          } else {
            /* if 0, quit */
            if (number != 0) {
              TOGGLE_BIT(GET_OBJ_VAL(d->edit_obj, 1), 1 << (number - 1));
              iedit_disp_val2_menu(d);
              return;
            } else {
              iedit_disp_val3_menu(d);
              return;
            }
          }
          break;
        case ITEM_QUIVER:
          if (number < 0 || number > 3) {
            send_to_char("Invalid input!\r\nQuiver type: ", CH);
            return;
          }
          break;
        case ITEM_MAGIC_TOOL:
          if (number < 0 || number > 15) {
            send_to_char("Rating must be between 1 and 15!\r\nForce/Rating: ", CH);
            return;
          }
          break;
        case ITEM_WEAPON:
        case ITEM_FIREWEAPON:
          if ((number < 1) || (number > 4)) {
            send_to_char("Value must be between 1 and 4.\r\n"
                         "Base damage (1 - Light, 2 - Moderate, 3 - Serious, 4 - Deadly): ", d->character);
            return;
          }
          break;
        case ITEM_PROGRAM:
          if (number < 1 || number > 20) {
            send_to_char("Rating must be between 1 and 20.\r\nProgram Rating: ", CH);
            return;
          }
          break;
        case ITEM_BIOWARE:
          if (number < 0 || number > 10) {
            send_to_char("Rating must be less than 10.\r\nRating: ", CH);
            return;
          }
          break;
        case ITEM_CYBERWARE:
          if (number < 0 || number > 10) {
            send_to_char("Rating must be less than 10.\r\nRating: ", CH);
            return;
          }
          break;
        case ITEM_DECK_ACCESSORY:
          switch (GET_OBJ_VAL(OBJ, 0)) {
            case TYPE_FILE:
              if (number <= 0) {
                send_to_char("Size must be greater than 0.  File size: ", CH);
                return;
              }
              break;
            case TYPE_UPGRADE:
              if (number < 0 || number > 5) {
                send_to_char("Invalid choice!  Upgrade type: ", CH);
                return;
              }
              break;
            case TYPE_PARTS:
              if (number < 0 || number > 1) {
                send_to_char("Invalid choice!  Part type:", CH);
                return;
              }
              break;
            case TYPE_COOKER:
              if (number < 0 || number > 10) {
                send_to_char("Rating must be between 0 and 10. Rating: ", CH);
                return;
              }
              break;
          }
          break;
        case ITEM_PATCH:
          if ((number < 1) || (number > 10)) {
            send_to_char("Rating must be between 1 and 10.\r\n", CH);
            return;
          }
          break;
        case ITEM_GUN_ACCESSORY:
          if (number < 1 || number > 10) {
            send_to_char("Invalid option!  Accessory type: ", CH);
            return;
          }
          break;
        case ITEM_FOCUS:
          if (number < 1 || number > 10) {
            send_to_char("Force must be between 1 and 10.\r\n", CH);
            return;
          }
          break;
        case ITEM_GUN_AMMO:
          if (number < 1 || number > MAX_WEAP) {
            send_to_char("Invalid option!\r\nAmmunition type: ", CH);
            return;
          }
          break;
        case ITEM_GUN_MAGAZINE:
          if (number < 1 || number > MAX_WEAP) {
            send_to_char("Invalid option!\r\nMagazine type: ", CH);
            return;
          }
          break;
        case ITEM_RADIO:
          if (number < 0 || number > 5) {
            send_to_char("Enter radio's range (0-5): ", CH);
            return;
          }
          break;
        case ITEM_MONEY:
          if (number < 1 || number > 2) {
            iedit_disp_val2_menu(d);
            return;
          }
          number--;
          break;
        case ITEM_SPELL_FORMULA:
          if (number <= 0 || number > MAX_SPELLS) {
            iedit_disp_spells_menu(d);
            return;
          }
          break;
        default:
          break;
      }
      GET_OBJ_VAL(d->edit_obj, 1) = number;
      iedit_disp_val3_menu(d);
      break;

    case IEDIT_VALUE_3:
      number = atoi(arg);
      switch (GET_OBJ_TYPE(d->edit_obj)) {
        case ITEM_WORKSHOP:
          if (number < 1 || number >= NUM_AMMOTYPES) {
            send_to_char(CH, "Invalid value. Enter Ammunition Type: ");
            return;
          }
          break;
        case ITEM_DRINKCON:
        case ITEM_FOUNTAIN:
          number--;
          if (number < 0 || number >= NUM_DRINK_TYPES) {
            send_to_char("Invalid choice.\r\n", CH);
            iedit_disp_drinktype_menu(d);
            return;
          }
          break;
        case ITEM_WEAPON:
        case ITEM_FIREWEAPON:
          if (number < -WEAPON_MAXIMUM_STRENGTH_BONUS || number > WEAPON_MAXIMUM_STRENGTH_BONUS) {
            send_to_char(CH, "Value must be between %d and %d.\r\n", -WEAPON_MAXIMUM_STRENGTH_BONUS, WEAPON_MAXIMUM_STRENGTH_BONUS);
            iedit_disp_val3_menu(d);
            return;
          }
          break;
        case ITEM_BIOWARE:
          if (number != BIOWARE_STANDARD && number != BIOWARE_CULTURED) {
            send_to_char(CH, "Invalid choice. %d for yes, %d for no: ", BIOWARE_CULTURED, BIOWARE_STANDARD);
            return;
          }
          break;
        case ITEM_CYBERWARE:
          if (number < GRADE_STANDARD || number > GRADE_DELTA) {
            send_to_char("Please select grade from list.\r\nGrade: ", CH);
            return;
          }
          break;
        case ITEM_PROGRAM:
          if (number < 0) {
            send_to_char("Invalid input.\r\nProgram Size: ", CH);
            return;
          }
          break;
        case ITEM_DECK_ACCESSORY:
          if (GET_OBJ_VAL(OBJ, 0) == TYPE_FILE) {
            if (number <= 0) {
              send_to_char("Size must be greater than 0. File size in megapulses: ", CH);
              return;
            }
          }
          switch (GET_OBJ_VAL(OBJ, 1)) {
            case 0:
              if (number < 1 || number > 15) {
                send_to_char("MPCP must be between 1 and 15.  MPCP Rating: ", CH);
                return;
              }
              break;
            case 1:
              if (number < 1 || number > 1000) {
                send_to_char("Invalid value!  Active memory increase: ", CH);
                return;
              }
              break;
            case 2:
              if (number < 1 || number > 20000) {
                send_to_char("Invalid value!  Storage memory increase: ", CH);
                return;
              }
              break;
            case 3:
              if (number < 1 || number > 100) {
                send_to_char("Invalid value!  Load speed increase: ", CH);
                return;
              }
              break;
            case 4:
              if (number < 1 || number > 50) {
                send_to_char("Invalid value!  I/O speed increase: ", CH);
                return;
              }
              break;
            case 5:
              if (number < 1 || number > 3) {
                send_to_char("Values must range from 1 to 3.  Reaction increase level: ", CH);
                return;
              }
          }
          break;
        case ITEM_GUN_ACCESSORY:
          if (GET_OBJ_VAL(OBJ, 1) == 3 && (number < -4 || number > -1)) {
            send_to_char("Modifier must be between -4 and -1.  Recoil modifier: ", CH);
            return;
          }
          break;
        case ITEM_FOCUS:
          if (GET_OBJ_VAL(OBJ, 0) == FOCI_SPIRIT && (number < 1 || number > NUM_SPIRITS)) {
            send_to_char("Invalid response.  Focus spirit type: ", CH);
            return;
          } else if (GET_OBJ_VAL(OBJ, 0) == FOCI_SPEC_SPELL && (number < 1 || number > MAX_SPELLS)) {
            send_to_char("Invalid choice!\r\n", CH);
            iedit_disp_val3_menu(d);
            return;
          } else if (GET_OBJ_VAL(OBJ, 0) == FOCI_SPELL_CAT) {
            if (number < 1 || number > 5) {
              send_to_char("Invalid choice!  Focus spell category: ", CH);
              return;
            } else
              number--;
          }
          break;
        case ITEM_RADIO:
          if (number < 0 || number > 20) {
            send_to_char("Enter encryption/decryption rating (1-6): ", CH);
            return;
          }
          break;
        case ITEM_MONEY:
          if (number < 1 || number > 3) {
            iedit_disp_val3_menu(d);
            return;
          }
          break;
        case ITEM_GUN_AMMO:
        case ITEM_GUN_MAGAZINE:
          if (number < 0 || number >= NUM_AMMOTYPES) {
            send_to_char("INVALID TYPE!\r\nSelect Type: ", CH);
            return;
          }
          break;
        case ITEM_SPELL_FORMULA:
          if (number < 0 || number > 1) {
            send_to_char("0 for mage, 1 for shaman: ", CH);
            return;
          }
          break;

        default:
          break;
      }
      GET_OBJ_VAL(d->edit_obj, 2) = number;
      iedit_disp_val4_menu(d);
      break;

    case IEDIT_VALUE_4:
      number = atoi(arg);
      switch (GET_OBJ_TYPE(d->edit_obj)) {
        case ITEM_FOOD:
          if (number < 0 || number > 4) {
            send_to_char("Invalid choice! Poison damage: 0=none, 1=light, 2=moderate, 3=serious, 4=deadly: ", d->character);
            return;
          }
        case ITEM_WEAPON:
          if (number < 0 || number > MAX_WEAP) {
            send_to_char("Invalid choice!\r\n", d->character);
            iedit_disp_weapon_menu(d);
            return;
          }
          switch (number) {
              // Determine what attachment locations are available by default based on weapon type. 0 = available, -1 = not.
            case WEAP_LIGHT_PISTOL:
            case WEAP_HEAVY_PISTOL:
            case WEAP_MACHINE_PISTOL:
            case WEAP_REVOLVER:
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_TOP) = 0;
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_BARREL) = 0;
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_UNDER) = -2;
              break;
            case WEAP_TASER:
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_TOP) = 0;
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_BARREL) = -2;
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_UNDER) = -2;
              break;
            case WEAP_SMG:
            case WEAP_LMG:
            case WEAP_MMG:
            case WEAP_HMG:
            case WEAP_MINIGUN:
            case WEAP_MISS_LAUNCHER:
            case WEAP_GREN_LAUNCHER:
            case WEAP_SPORT_RIFLE:
            case WEAP_SNIPER_RIFLE:
            case WEAP_SHOTGUN:
            case WEAP_ASSAULT_RIFLE:
            case WEAP_CANNON:
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_TOP) = 0;
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_BARREL) = 0;
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_UNDER) = 0;
              break;
            default: // includes holdout pistols and all melee weapons
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_TOP) = -2;
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_BARREL) = -2;
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_UNDER) = -2;
          }
          break;
        case ITEM_FIREWEAPON:
          if (number < 1 || (!access_level(CH, LVL_ADMIN) && number == TYPE_BIFURCATE) || number > NUM_WEAPON_TYPES) {
            send_to_char("Invalid choice!\r\n", d->character);
            iedit_disp_weapon_menu(d);
            return;
          }
          number = TYPE_ARROW;
          break;
        case ITEM_PROGRAM:
          if (number < 1 || number > 4) {
            send_to_char("Must be between 1 and 4: ", d->character);
            iedit_disp_val4_menu(d);
            return;
          }
          break;
        case ITEM_DECK_ACCESSORY:
          if (number < 1 || number > 15) {
            send_to_char("MPCP value must be between 1 and 15!\r\nMPCP to enhance: ", CH);
            return;
          }
          break;
        case ITEM_CYBERWARE:
          switch (GET_CYBERWARE_TYPE(OBJ)) {
            case CYB_DATAJACK:
            case CYB_TOOTHCOMPARTMENT:
            case CYB_HANDBLADE:
            case CYB_HANDSPUR:
            case CYB_CLIMBINGCLAWS:
            case CYB_FANGS:
            case CYB_HORNS:
            case CYB_REFLEXTRIGGER:
              if (number < 0 || number > 1) {
                send_to_char("Invalid Input! Enter 1 or 0: ", CH);
                return;
              }
              TOGGLE_BIT(GET_CYBERWARE_FLAGS(OBJ), 1 << number);
              break;
            case CYB_ARMS:
              if (number < 0 || number > NUM_ARMS_MODS) {
                send_to_char("Invalid Input! Enter options (0 to quit): ", CH);
                return;
              }
              if (number == 0) {
                if (!(IS_SET(GET_CYBERWARE_FLAGS(OBJ), ARMS_MOD_OBVIOUS) || IS_SET(GET_CYBERWARE_FLAGS(OBJ), ARMS_MOD_SYNTHETIC))) {
                  send_to_char("Defaulting to Obvious.\r\n", CH);
                  SET_BIT(GET_CYBERWARE_FLAGS(OBJ), ARMS_MOD_OBVIOUS);
                }
                iedit_disp_menu(d);
                return;
              }
              number--;
              TOGGLE_BIT(GET_CYBERWARE_FLAGS(OBJ), 1 << number);
              CLS(CH);
              sprintbit(GET_CYBERWARE_FLAGS(OBJ), armsmods, buf1, sizeof(buf1));
              for (int mod_num = 0; mod_num < NUM_ARMS_MODS; mod_num++)
                send_to_char(CH, " %d) %s\r\n", mod_num + 1, armsmods[mod_num]);
              send_to_char(CH, "Current Flags: ^c%s^n\r\n Enter options (0 to quit): ", buf1);
              return;
            case CYB_LEGS:
              if (number < 0 || number > NUM_LEGS_MODS) {
                send_to_char("Invalid Input! Enter options (0 to quit): ", CH);
                return;
              }
              if (number == 0) {
                if (!(IS_SET(GET_CYBERWARE_FLAGS(OBJ), LEGS_MOD_OBVIOUS) || IS_SET(GET_CYBERWARE_FLAGS(OBJ), LEGS_MOD_SYNTHETIC))) {
                  send_to_char("Defaulting to Obvious.\r\n", CH);
                  SET_BIT(GET_CYBERWARE_FLAGS(OBJ), LEGS_MOD_OBVIOUS);
                }
                iedit_disp_menu(d);
                return;
              }
              number--;
              TOGGLE_BIT(GET_CYBERWARE_FLAGS(OBJ), 1 << number);
              CLS(CH);
              sprintbit(GET_CYBERWARE_FLAGS(OBJ), legsmods, buf1, sizeof(buf1));
              for (int mod_num = 0; mod_num < NUM_LEGS_MODS; mod_num++)
                send_to_char(CH, " %d) %s\r\n", mod_num + 1, legsmods[mod_num]);
              send_to_char(CH, "Current Flags: ^c%s^n\r\n Enter options (0 to quit): ", buf1);
              return;
            case CYB_SKULL:
              if (number < 0 || number > NUM_SKULL_MODS) {
                send_to_char("Invalid Input! Enter options (0 to quit): ", CH);
                return;
              }
              if (number == 0) {
                if (!(IS_SET(GET_CYBERWARE_FLAGS(OBJ), SKULL_MOD_OBVIOUS) || IS_SET(GET_CYBERWARE_FLAGS(OBJ), SKULL_MOD_SYNTHETIC))) {
                  send_to_char("Defaulting to Obvious.\r\n", CH);
                  SET_BIT(GET_CYBERWARE_FLAGS(OBJ), SKULL_MOD_OBVIOUS);
                }
                iedit_disp_menu(d);
                return;
              }
              number--;
              TOGGLE_BIT(GET_CYBERWARE_FLAGS(OBJ), 1 << number);
              CLS(CH);
              sprintbit(GET_CYBERWARE_FLAGS(OBJ), skullmods, buf1, sizeof(buf1));
              for (int mod_num = 0; mod_num < NUM_SKULL_MODS; mod_num++)
                send_to_char(CH, " %d) %s\r\n", mod_num + 1, skullmods[mod_num]);
              send_to_char(CH, "Current Flags: ^c%s^n\r\n Enter options (0 to quit): ", buf1);
              return;
            case CYB_TORSO:
              if (number < 0 || number > NUM_TORSO_MODS) {
                send_to_char("Invalid Input! Enter options (0 to quit): ", CH);
                return;
              }
              if (number == 0) {
                if (!(IS_SET(GET_CYBERWARE_FLAGS(OBJ), TORSO_MOD_OBVIOUS) || IS_SET(GET_CYBERWARE_FLAGS(OBJ), TORSO_MOD_SYNTHETIC))) {
                  send_to_char("Defaulting to Obvious.\r\n", CH);
                  SET_BIT(GET_CYBERWARE_FLAGS(OBJ), TORSO_MOD_OBVIOUS);
                }
                iedit_disp_menu(d);
                return;
              }
              number--;
              TOGGLE_BIT(GET_CYBERWARE_FLAGS(OBJ), 1 << number);
              CLS(CH);
              sprintbit(GET_CYBERWARE_FLAGS(OBJ), torsomods, buf1, sizeof(buf1));
              for (int mod_num = 0; mod_num < NUM_TORSO_MODS; mod_num++)
                send_to_char(CH, " %d) %s\r\n", mod_num + 1, torsomods[mod_num]);
              send_to_char(CH, "Current Flags: ^c%s^n\r\n Enter options (0 to quit): ", buf1);
              return;
            case CYB_DERMALSHEATHING:
              if (number < 0 || number > 1) {
                send_to_char("Invalid Input! Enter 1 or 0: ", CH);
                return;
              }
              break;
            case CYB_CHIPJACK:
              if (number < 1 || number > 4) {
                send_to_char("Must have between 1 and 4 slots: ", CH);
                return;
              }
              break;
            case CYB_HANDRAZOR:
              if (number < 0 || number > 2) {
                send_to_char("Invalid Input! Enter options (0 to quit): ", CH);
                return;
              }
              if (number == 0) {
                iedit_disp_menu(d);
                return;
              }
              number--;
              TOGGLE_BIT(GET_OBJ_VAL(OBJ, 3), 1 << number);
              CLS(CH);
              sprintbit(GET_OBJ_VAL(OBJ, 3), hand_razors, buf1, sizeof(buf1));
              send_to_char(CH, "1) Retractable\r\n2) Improved\r\nCurrent Flags: ^c%s^n\r\nEnter options (0 to quit): ", buf1);
              return;
            case CYB_BONELACING:
              if (number < 0 || number > 4) {
                send_to_char("Invalid Type! Enter type of lacing: ", CH);
                return;
              }
              break;
            case CYB_FILTRATION:
              if (number < 0 || number > 2) {
                send_to_char("Invalid Input! Enter type of filtration: ", CH);
                return;
              }
              break;
            case CYB_CRANIALCYBER:
              if ((real_obj = real_object(number)) < 1|| obj_proto[real_obj].obj_flags.type_flag != ITEM_CYBERDECK) {
                send_to_char("Object doesn't exist or is not a cyberdeck! Vnum of cyberdeck to be included in headware (0 to quit): ", CH);
                return;
              }
              break;
            case CYB_EYES:
              if (number < 0 || number > NUM_EYEMODS+1) {
                send_to_char("Invalid Input! Enter options (0 to quit): ", CH);
                return;
              }
              if (number == 0) {
                iedit_disp_menu(d);
                return;
              }
              number--;
              TOGGLE_BIT(GET_OBJ_VAL(OBJ, 3), 1 << number);
              iedit_disp_cybereyes_menu(d);
              return;
          }
          break;
        default:
          break;
      }
      GET_OBJ_VAL(d->edit_obj, 3) = number;
      iedit_disp_val5_menu(d);
      break;

    case IEDIT_VALUE_5:
      number = atoi(arg);
      switch (GET_OBJ_TYPE(d->edit_obj)) {
        case ITEM_WEAPON:
        case ITEM_FIREWEAPON:
          if (number < 1 || number > NUM_SKILL_TYPES) {
            send_to_char("Invalid choice!\r\n", d->character);
            iedit_disp_skill_menu(d);
          } else {
            // this is to skip these general skills which are not in the list
            number += 2;
            if (number == 22) {
              number = SKILL_UNARMED_COMBAT;
              break;
            }
            if (number >= SKILL_UNARMED_COMBAT)
              number += 3;
            if (number >= SKILL_FIREARMS)
              number++;
            if (number >= SKILL_GUNNERY)
              number++;
            if (number >= SKILL_PROJECTILES)
              number++;
            if (number >= SKILL_THROWING_WEAPONS)
              number++;
          }
          break;
        case ITEM_MOD:
          if (number < 0 || number > NUM_VEH_TYPES) {
            send_to_char("Invalid Input! Enter options (0 to quit): ", CH);
            return;
          }
          if (number == 0) {
            iedit_disp_val6_menu(d);
            return;
          }
          TOGGLE_BIT(GET_VEHICLE_MOD_DESIGNED_FOR_FLAGS(OBJ), 1 << (number - 1));
          iedit_disp_mod_vehicle_type_menu(d);
          return;
        default:
          break;
      }
      GET_OBJ_VAL(d->edit_obj, 4) = number;
      iedit_disp_val6_menu(d);
      break;

    case IEDIT_VALUE_6:
      number = atoi(arg);
      switch (GET_OBJ_TYPE(d->edit_obj)) {
        case ITEM_WEAPON:
          break;
        case ITEM_FIREWEAPON:
          if (number < 0 || number > 1) {
            send_to_char("Value must be 0 or 1!\r\nType: ", CH);
            return;
          }
          break;
        case ITEM_MOD:
          if (number < 0 || number > ENGINE_DIESEL) {
            send_to_char("Invalid Input! Enter options (0 to quit): ", CH);
            return;
          }
          if (number == 0) {
            iedit_disp_val7_menu(d);
            return;
          }
          TOGGLE_BIT(GET_OBJ_VAL(OBJ, 5), 1 << number);
          iedit_disp_mod_engine_menu(d);
          return;
      }
      GET_OBJ_VAL(d->edit_obj, 5) = number;
      iedit_disp_val7_menu(d);
      break;
    case IEDIT_VALUE_7:
      number = atoi(arg);
      switch (GET_OBJ_TYPE(d->edit_obj)) {
        case ITEM_WEAPON:
          break;
        case ITEM_FIREWEAPON:
          if (number < 1 || number > 10) {
            send_to_char("Strength min. must be between 1 and 10.  Enter strength minimum: ", CH);
            return;
          }
          break;
        case ITEM_CYBERDECK:
          if (number < 0 || (number > 1 && number > (int)(GET_OBJ_VAL(d->edit_obj, 0) / 3))) {
            send_to_char(CH, "Reaction upgrade must be between 0 and %d. Reaction Upgrade: ", (int)(GET_OBJ_VAL(d->edit_obj, 0) / 3));
            return;
          }
          break;
        default:
          break;
      }
      GET_OBJ_VAL(d->edit_obj, 6) = number;
      iedit_disp_val8_menu(d);
      break;

    case IEDIT_VALUE_8:
      number = atoi(arg);
      switch (GET_OBJ_TYPE(d->edit_obj)) {
        case ITEM_WEAPON:
          if (IS_GUN(GET_WEAPON_ATTACK_TYPE(OBJ))) {
            if (number != -1) {
              struct obj_data *accessory = read_object(number, VIRTUAL);
              if (!accessory) {
                send_to_char("Invalid vnum.\r\n", CH);
                iedit_disp_val8_menu(d);
                return;
              } else {
                // Cascade affects (take the first one and glue it to the weapon).
                for (j = 0; (j < MAX_OBJ_AFFECT && !modified); j++) {
                  if (!(OBJ->affected[j].modifier)) {
                    OBJ->affected[j].location = accessory->affected[0].location;
                    OBJ->affected[j].modifier = accessory->affected[0].modifier;
                    break;
                  }
                }
                extract_obj(accessory);
              }
            }
          } else if (GET_WEAPON_ATTACK_TYPE(OBJ) == WEAP_GRENADE) {
            if (number != GRENADE_TYPE_EXPLOSIVE && number != GRENADE_TYPE_FLASHBANG) {
              send_to_char(CH, "You must enter either %d for explosive or %d for flashbang: ", GRENADE_TYPE_EXPLOSIVE, GRENADE_TYPE_FLASHBANG);
              iedit_disp_val8_menu(d);
              return;
            }
          } else {
            if (number < 0 || number > 4) {
              send_to_char("Weapon focus rating must be between 0 (no focus) and 4.\r\n", CH);
              iedit_disp_val8_menu(d);
              return;
            }
          }
          break;
        default:
          break;
      }
      GET_OBJ_VAL(d->edit_obj, 7) = number;
      iedit_disp_val9_menu(d);
      break;

    case IEDIT_VALUE_9:
      number = atoi(arg);
      switch (GET_OBJ_TYPE(d->edit_obj)) {
        case ITEM_WEAPON:
          {
            if (number != -1) {
              struct obj_data *accessory = read_object(number, VIRTUAL);
              if (!accessory) {
                send_to_char("Invalid vnum.\r\n", CH);
                iedit_disp_val9_menu(d);
                return;
              } else {
                // Cascade affects (take the first one and glue it to the weapon).
                for (j = 0; (j < MAX_OBJ_AFFECT && !modified); j++) {
                  if (!(OBJ->affected[j].modifier)) {
                    OBJ->affected[j].location = accessory->affected[0].location;
                    OBJ->affected[j].modifier = accessory->affected[0].modifier;
                    break;
                  }
                }
                extract_obj(accessory);
              }
            }
          }
          break;
        default:
          break;
      }
      GET_OBJ_VAL(d->edit_obj, 8) = number;
      iedit_disp_val10_menu(d);
      break;

    case IEDIT_VALUE_10:
      number = atoi(arg);
      switch (GET_OBJ_TYPE(d->edit_obj)) {
        case ITEM_WEAPON:
          {
            if (number != -1) {
              struct obj_data *accessory = read_object(number, VIRTUAL);
              if (!accessory) {
                send_to_char("Invalid vnum.\r\n", CH);
                iedit_disp_val10_menu(d);
                return;
              } else {
                // Cascade affects (take the first one and glue it to the weapon).
                for (j = 0; (j < MAX_OBJ_AFFECT && !modified); j++) {
                  if (!(OBJ->affected[j].modifier)) {
                    OBJ->affected[j].location = accessory->affected[0].location;
                    OBJ->affected[j].modifier = accessory->affected[0].modifier;
                    break;
                  }
                }
                extract_obj(accessory);
              }
            }
          }
          break;
        default:
          break;
      }
      GET_OBJ_VAL(d->edit_obj, 9) = number;
      iedit_disp_val11_menu(d);
      break;

    case IEDIT_VALUE_11:
      number = atoi(arg);
      switch (GET_OBJ_TYPE(d->edit_obj)) {
        case ITEM_WEAPON:
          if (number < 0 || number > MODE_FA) {
            send_to_char("Invalid input! Enter options (0 to quit): ", CH);
            return;
          }
          if (number == 0) {
            // Move on to recoil comp.
            iedit_disp_val12_menu(d);
            return;
          }
          TOGGLE_BIT(GET_OBJ_VAL(OBJ, 10), 1 << number);
          iedit_disp_firemodes_menu(d);
          return;
        default:
          break;
      }
      GET_OBJ_VAL(d->edit_obj, 10) = number;
      iedit_disp_val12_menu(d);
      break;

    case IEDIT_VALUE_12:
      number = atoi(arg);
      switch (GET_OBJ_TYPE(d->edit_obj)) {
          // TODO: implement item_weapon here.
        case ITEM_WEAPON:
          // NOTE: This does NOT actually modify value '12' (GET_OBJ_VAL(obj, 11)). It changes the attempts field instead.
          // For weapons, value '12' is currently the selected firemode of the weapon.
          if (number < -2 || number > 4) {
            send_to_char("Integral recoil compensation must be between -2 and 4 (inclusive). Enter recoil comp: ", CH);
            return;
          }

          // Set the recoil comp here.
          GET_WEAPON_INTEGRAL_RECOIL_COMP(d->edit_obj) = number;

          iedit_disp_menu(d);
          return;
        default:
          break;
      }
      GET_OBJ_VAL(d->edit_obj, 11) = number;
      iedit_disp_menu(d);
      break;

    case IEDIT_PROMPT_APPLY:
      number = atoi(arg);
      if (number == 0) {
        iedit_disp_menu(d);
        return;
      } else if (number < 0 || number > MAX_OBJ_AFFECT) {
        send_to_char("Invalid choice!\r\n", d->character);
        iedit_disp_prompt_apply_menu(d);
      } else {
        d->edit_number2 = number - 1;
        d->edit_mode = IEDIT_APPLY;
        iedit_disp_apply_menu(d);
      }
      break;
    case IEDIT_APPLY:
      number = atoi(arg);
      if (number == 0) {
        d->edit_obj->affected[d->edit_number2].location = 0;
        d->edit_obj->affected[d->edit_number2].modifier = 0;
        iedit_disp_prompt_apply_menu(d);
      } else if ((GET_OBJ_TYPE(d->edit_obj) == ITEM_MOD && (number < 0 || number >= VAFF_MAX)) ||
                 (number < 0 || number >= AFF_MAX)) {
        send_to_char("Invalid choice!\r\n", d->character);
        iedit_disp_apply_menu(d);
      } else {
        d->edit_obj->affected[d->edit_number2].location = number;
        send_to_char("Modifier:", d->character);
        d->edit_mode = IEDIT_APPLYMOD;
      }
      break;
    case IEDIT_APPLYMOD:
      number = atoi(arg);
      d->edit_obj->affected[d->edit_number2].modifier = number;
      iedit_disp_prompt_apply_menu(d);
      break;
    case IEDIT_EXTRADESC_KEY:
      DELETE_ARRAY_IF_EXTANT(((struct extra_descr_data *) *d->misc_data)->keyword);
      ((struct extra_descr_data *) * d->misc_data)->keyword = str_dup(arg);
      iedit_disp_extradesc_menu(d);
      break;
    case IEDIT_MATERIAL:
      number = atoi(arg);
      if (number == 0)
        iedit_disp_menu(d);
      else if (number > 0 && number < NUM_MATERIALS + 1) {
        GET_OBJ_MATERIAL(OBJ) = number - 1;
        iedit_disp_menu(d);
      } else
        iedit_disp_material_menu(d);
      break;
    case IEDIT_RATING:
      number = atoi(arg);
      if (number == 0)
        iedit_disp_menu(d);
      else if (number > 0 && number < NUM_BARRIERS + 1) {
        GET_OBJ_CONDITION(OBJ) = GET_OBJ_BARRIER(OBJ) = barrier_ratings[number - 1];
        iedit_disp_menu(d);
      } else
        iedit_disp_rating_menu(d);
      break;
    case IEDIT_STREETINDEX:
      fnumber = atof(arg);
      if (fnumber <= 0 || fnumber > 10) {
        send_to_char("Street index (black market cost multiplier) must be between 0 and 10. Enter street index: ", CH);
      } else {
        GET_OBJ_STREET_INDEX(OBJ) = fnumber;
        iedit_disp_menu(d);
      }
      break;
    case IEDIT_EXTRADESC_MENU:
      number = atoi(arg);
      switch (number) {
        case 0: {
#define MISCDATA ((struct extra_descr_data *) * d->misc_data)
          /* if something got left out, delete the extra desc
           when backing out to menu */
          if (!MISCDATA->keyword || !MISCDATA->description) {
            DELETE_ARRAY_IF_EXTANT(MISCDATA->keyword);
            DELETE_ARRAY_IF_EXTANT(MISCDATA->description);

            /* Null out the ex_description linked list pointer to this object. */
            struct extra_descr_data *temp = d->edit_obj->ex_description, *next = NULL;
            if (temp == MISCDATA) {
              d->edit_obj->ex_description = NULL;
            } else {
              for (; temp; temp = next) {
                next = temp->next;
                if (next == MISCDATA) {
                  if (next->next) {
                    temp->next = next->next;
                  } else {
                    temp->next = NULL;
                  }
                  break;
                }
              }
            }

            delete MISCDATA;
            *d->misc_data = NULL;
          }
#undef MISCDATA

          iedit_disp_menu(d);
        }
          break;
        case 1:
          d->edit_mode = IEDIT_EXTRADESC_KEY;
          send_to_char("Enter keywords, separated by spaces:", d->character);
          break;
        case 2:
          d->edit_mode = IEDIT_EXTRADESC_DESCRIPTION;
          send_to_char("Enter description:\r\n", d->character);
          /* send out to modify.c */
          DELETE_D_STR_IF_EXTANT(d);
          INITIALIZE_NEW_D_STR(d);
          d->max_str = MAX_MESSAGE_LENGTH;
          d->mail_to = 0;
          break;
        case 3:
          /* if keyword or description has not been changed don't allow person to
           * edit next */
          if (!((struct extra_descr_data *) *d->misc_data)->keyword ||
              !((struct extra_descr_data *) *d->misc_data)->description) {
            send_to_char("You can't edit the next extra desc  without completing this one.\r\n", d->character);
            iedit_disp_extradesc_menu(d);
          } else {
            struct extra_descr_data *new_extra;

            if (((struct extra_descr_data *) * d->misc_data)->next)
              d->misc_data = (void **) &((struct extra_descr_data *) * d->misc_data)->next;
            else {
              /* make new extra, attach at end */
              new_extra = new extra_descr_data;
              memset((char *) new_extra, 0, sizeof(extra_descr_data));

              ((struct extra_descr_data *) * d->misc_data)->next = new_extra;
              /* edit new extra, we NEED double pointer because i will set
               * *d->misc_data to NULL later */
              d->misc_data =
              (void **) &((struct extra_descr_data *) * d->misc_data)->next;

            }
            iedit_disp_extradesc_menu(d);

          }
          break;
        default:
          send_to_char("Invalid choice!\r\n", d->character);
          iedit_disp_extradesc_menu(d);
          break;
      }
      break;
    default:
      break;
  }
}

#undef OBJ

/*
 Modify this code for new obj formats
 */

#define WRITE_IF_CHANGED(string, thing, default) if ((thing) != (default)) { fprintf(fp, (string), (thing)); }
#define WRITE_IF_CHANGED_STR(string, thing, default) if (str_cmp((thing), (default))) { fprintf(fp, (string), (thing)); }
void write_objs_to_disk(vnum_t zonenum)
{
  int counter, counter2, realcounter, count = 0;
  FILE *fp;
  struct obj_data *obj;
  struct extra_descr_data *ex_desc;

  rnum_t zone = real_zone(zonenum);

  // ideally, this would just fill a VTable with vals...maybe one day

  snprintf(buf, sizeof(buf), "%s/%d.obj", OBJ_PREFIX, zone_table[zone].number);
  fp = fopen(buf, "w+");

  bool wrote_something = FALSE;

  /* start running through all objects in this zone */
  for (counter = zone_table[zone].number * 100;
       counter <= zone_table[zone].top;
       counter++) {
    /* write object to disk */
    realcounter = real_object(counter);

    if (realcounter >= 0) {
      obj = obj_proto+realcounter;

      /*
      if (!strcmp("an unfinished object", obj->text.name))
        continue;
      */

      wrote_something = TRUE;

      fprintf(fp, "#%ld\n", GET_OBJ_VNUM(obj));

      fprintf(fp,
              "Keywords:\t%s\n"
              "Name:\t%s\n"
              "RoomDesc:$\n%s~\n"
              "LookDesc:$\n%s~\n",
              obj->text.keywords? obj->text.keywords : "unnamed",
              obj->text.name? obj->text.name : "an unnamed object",
              obj->text.room_desc? obj->text.room_desc
              : "An unnamed object sits here",
              obj->text.look_desc? cleanup(buf2, obj->text.look_desc)
              : "You see an uncreative object.\n");

      fprintf(fp, "Type:\t%s\n", item_types[(int)GET_OBJ_TYPE(obj)]);

      WRITE_IF_CHANGED_STR("WearFlags:\t%s\n", GET_OBJ_WEAR(obj).ToString(), "0");
      WRITE_IF_CHANGED_STR("ExtraFlags:\t%s\n", GET_OBJ_EXTRA(obj).ToString(), "0");
      WRITE_IF_CHANGED_STR("AffFlags:\t%s\n", obj->obj_flags.bitvector.ToString(), "0");
      WRITE_IF_CHANGED_STR("Material:\t%s\n", material_names[(int)GET_OBJ_MATERIAL(obj)], material_names[5]);

      fprintf(fp, "[POINTS]\n");
      WRITE_IF_CHANGED("\tWeight:\t%.2f\n", GET_OBJ_WEIGHT(obj), 0);
      WRITE_IF_CHANGED("\tBarrier:\t%d\n", GET_OBJ_BARRIER(obj), 3);
      WRITE_IF_CHANGED("\tCost:\t%d\n", GET_OBJ_COST(obj), 0);
      WRITE_IF_CHANGED("\tAvailTN:\t%d\n", GET_OBJ_AVAILTN(obj), 0);
      WRITE_IF_CHANGED("\tAvailDay:\t%.2f\n", GET_OBJ_AVAILDAY(obj), 0);
      WRITE_IF_CHANGED("\tLegalNum:\t%d\n", GET_LEGAL_NUM(obj), 0);
      WRITE_IF_CHANGED("\tLegalCode:\t%d\n", GET_LEGAL_CODE(obj), 0);
      WRITE_IF_CHANGED("\tLegalPermit:\t%d\n", GET_LEGAL_PERMIT(obj), 0);
      WRITE_IF_CHANGED("\tStreetIndex:\t%.2f\n", GET_OBJ_STREET_INDEX(obj), 0.0);

      if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && GET_WEAPON_INTEGRAL_RECOIL_COMP(obj))
        fprintf(fp, "\tInnateRecoilComp:\t%d\n", GET_WEAPON_INTEGRAL_RECOIL_COMP(obj));

      bool print_vals = false;
      int i;
      for (i = 0; i < NUM_VALUES; i++)
        if (GET_OBJ_VAL(obj, i) != 0) {
          print_vals = true;
          break;
        }

      if (print_vals) {
        fprintf(fp, "[VALUES]\n");
        for (i = 0; i < NUM_VALUES; i++)
          if (GET_OBJ_VAL(obj, i) != 0)
            fprintf(fp, "\tVal%d:\t%d\n", i, GET_OBJ_VAL(obj, i));
      }

      /* do we have extra descriptions? */
      count = 0;
      if (obj->ex_description)
        for (ex_desc = obj->ex_description; ex_desc; ex_desc = ex_desc->next) {
          if (ex_desc->keyword && ex_desc->description) {
            fprintf(fp,
                    "[EXTRADESC %d]\n"
                    "\tKeywords:\t%s\n"
                    "\tDesc:$\n%s~\n", count,
                    ex_desc->keyword, cleanup(buf2, ex_desc->description));
            count++;
          }
        }

      /* do we have affects? */
      for (counter2 = 0; counter2 < MAX_OBJ_AFFECT; counter2++) {
        obj_affected_type *af = obj->affected+counter2;

        if (af->location != APPLY_NONE && af->modifier != 0) {
          fprintf(fp,
                  "[AFFECT %d]\n"
                  "\tLocation:\t%s\n"
                  "\tModifier:\t%d\n", counter2,
                  apply_types[(int)af->location], af->modifier);
        }
      }

      /* do we have source book info? */
      if (obj->source_info) {
        fprintf(fp, "SourceBook:\t%s\n", obj->source_info);
      }

      fprintf(fp, "BREAK\n");
    }
  }
  /* write final line, close */
  fprintf(fp, "END\n");
  fclose(fp);

  if (wrote_something)
    write_index_file("obj");
  else
    remove(buf);
}
