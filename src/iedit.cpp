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

#include "structs.h"
#include "awake.h"
#include "interpreter.h"
#include "comm.h"
#include "utils.h"
#include "db.h"
#include "dblist.h"
#include "boards.h"
#include "screen.h"
#include "olc.h"
#include "memory.h"
#include "newmagic.h"
#include "constants.h"
#include "handler.h"

#define OBJ     d->edit_obj


void write_objs_to_disk(int zone);


// extern funcs
extern char *cleanup(char *dest, const char *src);

#define NUM_WEAPON_TYPES        27
#define NUM_SKILL_TYPES         20
#define NUM_DRINK_TYPES         33
#define NUM_PATCHES             4
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
  sprintbit(GET_OBJ_VAL(d->edit_obj, 1), container_bits, buf1);
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
        sprinttype(d->edit_obj->affected[counter].location, veh_aff, buf2);
      else
        sprinttype(d->edit_obj->affected[counter].location, apply_types, buf2);
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
    for (counter = 0; counter < VAFF_MAX; counter += 2)
      send_to_char(CH, "%2d) %-18s    %2d) %-18s\r\n",
                   counter, veh_aff[counter],
                   counter + 1, counter + 1 < VAFF_MAX ?
                   veh_aff[counter + 1] : "");
  } else
  {
    for (counter = 0; counter < APPLY_MAX; counter += 2)
      send_to_char(CH, "%2d) %-18s    %2d) %-18s\r\n",
                   counter, apply_types[counter],
                   counter + 1, counter + 1 < APPLY_MAX ?
                   apply_types[counter + 1] : "");
  }
  send_to_char("Enter apply type (0 is no apply):", d->character);
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
                 counter, weapon_type[counter],
                 counter + 1, counter + 1 <= MAX_WEAP ? weapon_type[counter + 1] : "");
  
  send_to_char("Enter weapon type:\r\n", CH);
}

/* spell type */
void iedit_disp_spells_menu(struct descriptor_data * d)
{
  CLS(CH);
  for (int counter = 1; counter <= MAX_SPELLS; counter += 3)
    send_to_char(CH, "%2d) %-18s %2d) %-18s %2d) %-18s\r\n",
                 counter, spells[counter].name,
                 counter + 1, counter + 1 <= MAX_SPELLS ? spells[counter + 1].name : "",
                 counter + 2, counter + 2 <= MAX_SPELLS ? spells[counter + 2].name : "");
  
  send_to_char("Enter spell:\r\n", CH);
}

void iedit_disp_cybereyes_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int y = 0; y < NUM_EYEMODS+1; y += 2)
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n", y+1, eyemods[y], y+2, y+1 < NUM_EYEMODS+1 ? eyemods[y+1] : "");
  sprintbit(GET_OBJ_VAL(OBJ, 3), eyemods, buf1);
  send_to_char(CH, "Set Options: ^c%s^n\r\nEnter options (0 to quit): ", buf1);
}

void iedit_disp_firemodes_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int y = 1; y < 5; y++)
    send_to_char(CH, "  %d) %s\r\n", y, fire_mode[y]);
  sprintbit(GET_OBJ_VAL(OBJ, 10), fire_mode, buf1);
  send_to_char(CH, "Set Options: ^c%s^n\r\nEnter options (0 to quit): ", buf1);
}

void iedit_disp_mod_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int y = 1; y <= ENGINE_DIESEL; y++)
    send_to_char(CH, "  %d) %s\r\n", y, engine_type[y]);
  sprintbit(GET_OBJ_VAL(OBJ, 5), engine_type, buf1);
  send_to_char(CH, "Set Options: ^c%s^n\r\nEnter options (0 to quit): ", buf1);
}

/* object value 1 */
void iedit_disp_val1_menu(struct descriptor_data * d)
{
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
      send_to_char(CH, "0) General\r\n1) Electronic\r\n2) Microtronic\r\n3) Cyberware\r\n4) Vehicle\r\n5) Weaponry\r\n6) Medical\r\n7) Ammunition\r\nWorkshop Type: ");
      break;
    case ITEM_CAMERA:
      send_to_char("0) Camera\r\n1) Photo\r\nObject Type: ", CH);
      break;
    case ITEM_LIGHT:
      /* values 0 and 1 are unused.. jump to 2 */
      iedit_disp_val3_menu(d);
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
                   "  4) Power\r\n  5) Sustaining\r\n  6) Weapon\r\nFocus type: ", CH);
      break;
    case ITEM_CLIMBING:
      send_to_char("Enter rating (1-3): ", CH);
      break;
    case ITEM_RADIO:
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
          sprintf(buf, "%2d) %-20s ", c, skills[c].name);
        else
          sprintf(buf, "%s%2d) %-20s ", buf, c, skills[c].name);
        if (!(line % 3)) {
          sprintf(buf, "%s\r\n", buf);
          send_to_char(buf, CH);
        }
      }
      if ((line % 3) != 0) {
        sprintf(buf, "%s\r\nEnter a skill (0 to quit): ", buf);
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
  d->edit_mode = IEDIT_VALUE_2;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
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
      iedit_disp_val4_menu(d);
      break;
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
          iedit_disp_val3_menu(d);
          break;
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
        default:
          iedit_disp_menu(d);
          break;
      }
      break;
    case ITEM_GUN_ACCESSORY:
      send_to_char("  1) Smartlink\r\n  2) Scope\r\n  3) Gas vent\r\n  4) Shock pad\r\n"
                   "  5) Silencer\r\n  6) Sound Suppressor\r\n  7) Smart Goggles\r\n  8) Bipod\r\n  9) Tripod"
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
      if (GET_OBJ_VAL(d->edit_obj, 0) == TYPE_MOUNT)
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
      send_to_char("Space for magazines: ", CH);
      break;
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
  d->edit_mode = IEDIT_VALUE_3;
  int c;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WORKSHOP:
      if (GET_OBJ_VAL(OBJ, 0) == TYPE_AMMO && GET_OBJ_VAL(OBJ, 1) == TYPE_KIT) {
        for (c = AMMO_NORMAL; c <= AMMO_GEL; c++)
          send_to_char(CH, "%d) %s\r\n", c, ammo_type[c].name);
        send_to_char("Select ammunition type: ", CH);
      } else iedit_disp_menu(d);
      
      break;
    case ITEM_LIGHT:
      send_to_char("Number of hours (0 = burnt, -1 is infinite): ", d->character);
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
        send_to_char("Cultured (1 - Yes, 0 - No): ", CH);
      else iedit_disp_menu(d);
      break;
    case ITEM_GUN_AMMO:
    case ITEM_GUN_MAGAZINE:
      send_to_char("  0) Normal             1) APDS\r\n"
                   "  2) Explosive          3) EX\r\n"
                   "  4) Flechette          5) Gel\r\n"
                   "Select Type: ", CH);
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
      if (GET_OBJ_VAL(d->edit_obj, 0) == TYPE_MOUNT) {
        // Mounts don't have ratings. Set it to 1.
        GET_OBJ_VAL(d->edit_obj, 2) = 1;
        iedit_disp_val4_menu(d);
        return;
      }
      else if (GET_OBJ_VAL(d->edit_obj, 0) == MOD_ENGINE) {
        CLS(CH);
        for (c = 1; c <= ENGINE_DIESEL; c++)
          send_to_char(CH, " %d) %s\r\n", c, engine_type[c]);
        send_to_char("Engine type: ", CH);
      } else if (GET_OBJ_VAL(d->edit_obj, 0) == MOD_RADIO)
        send_to_char("Radio Range (0-5): ", CH);
      else
        send_to_char("Rating: ", CH);
      break;
    case ITEM_WORN:
      send_to_char("Space for grenades: ", CH);
      break;
    default:
      iedit_disp_menu(d);
  }
}

/* object value 4 */
void iedit_disp_val4_menu(struct descriptor_data * d)
{
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
      send_to_char("Poison Rating (0 = not poison): ", d->character);
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
          sprintbit(GET_OBJ_VAL(OBJ, 3), hand_razors, buf1);
          send_to_char(CH, "1) Retractable\r\n2) Improved\r\nCurrent Flags: ^c%s^n\r\nEnter options (0 to quit): ", buf1);
          break;
        case CYB_HANDBLADE:
        case CYB_HANDSPUR:
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
        case CYB_SKULL:
        case CYB_TORSO:
          send_to_char("0) Obvious\r\n1) Synthetic\r\nEnter Type: ", CH);
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
      send_to_char("Space for shuriken: ", CH);
      break;
    case ITEM_MOD:
      if (GET_OBJ_VAL(d->edit_obj, 0) == MOD_RADIO)
        send_to_char("Crypt Level (0-6): ", CH);
      else {
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
  d->edit_mode = IEDIT_VALUE_5;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WEAPON:
    case ITEM_FIREWEAPON:
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
        GET_OBJ_VAL(d->edit_obj, 4) = 2;
        iedit_disp_val6_menu(d);
        return;
      }
      send_to_char("Designed For (0 - Vehicle, 1 - Drone, 2 - Both): ", CH);
      break;
    default:
      iedit_disp_menu(d);
  }
}

/* object value 6 */
void iedit_disp_val6_menu(struct descriptor_data * d)
{
  d->edit_mode = IEDIT_VALUE_6;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WEAPON:
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
      iedit_disp_mod_menu(d);
      break;
    case ITEM_CYBERDECK:
      iedit_disp_val7_menu(d);
      break;
    default:
      iedit_disp_menu(d);
  }
}

/* object value 7 */
void iedit_disp_val7_menu(struct descriptor_data * d)
{
  d->edit_mode = IEDIT_VALUE_7;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WEAPON:
      if (!IS_GUN(GET_OBJ_VAL(d->edit_obj, 3)))
        send_to_char("Reach: ", CH);
      else iedit_disp_val8_menu(d);
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
  d->edit_mode = IEDIT_VALUE_8;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WEAPON:
      if (access_level(CH, LVL_ADMIN) && IS_GUN(GET_OBJ_VAL(OBJ, 3))) {
        if (GET_OBJ_VAL(OBJ, 7) >= -1)
          send_to_char("Enter vnum of object to attach on top: ", CH);
        else
          iedit_disp_val9_menu(d);
      } else
        iedit_disp_menu(d);
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
  d->edit_mode = IEDIT_VALUE_9;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WORN:
      send_to_char("Armoured clothing set (0 for no set): ", CH);
      break;
    case ITEM_WEAPON:
      if (access_level(CH, LVL_ADMIN)) {
        if (GET_OBJ_VAL(OBJ, 8) >= -1)
          send_to_char("Enter vnum of object to attach on barrel: ", CH);
        else
          iedit_disp_val10_menu(d);
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
  d->edit_mode = IEDIT_VALUE_10;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    case ITEM_WEAPON:
      if (GET_OBJ_VAL(OBJ, 9) >= -1)
        send_to_char("Enter vnum of object to attach under: ", CH);
      else
        iedit_disp_val11_menu(d);
      break;
    default:
      iedit_disp_menu(d);
  }
}

void iedit_disp_val11_menu(struct descriptor_data * d)
{
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
  d->edit_mode = IEDIT_VALUE_12;
  switch (GET_OBJ_TYPE(d->edit_obj))
  {
    default:
      iedit_disp_menu(d);
  }
}

/* object type */
void iedit_disp_type_menu(struct descriptor_data * d)
{
  int counter;
  
  CLS(CH);
  for (counter = 1; counter < NUM_ITEMS; counter += 2)
  {
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n",
                 counter, item_types[counter],
                 counter + 1, counter + 1 <= NUM_ITEMS ?
                 item_types[counter + 1] : "");
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
  for (int counter = 0; counter <= MAX_SPELLS; counter += 2)
  {
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n",
                 counter + 1, spells[counter].name,
                 counter + 2, counter + 1 <= MAX_SPELLS ?
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
  for (counter = 0; counter < ITEM_EXTRA_MAX; counter += 2)
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n",
                 counter + 1, extra_bits[counter],
                 counter + 2, counter + 1 < ITEM_EXTRA_MAX ?
                 extra_bits[counter + 1] : "");
  
  GET_OBJ_EXTRA(d->edit_obj).PrintBits(buf1, MAX_STRING_LENGTH,
                                       extra_bits, ITEM_EXTRA_MAX);
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
  int             counter;
  
  CLS(CH);
  for (counter = 0; counter < ITEM_WEAR_MAX; counter += 2)
  {
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n",
                 counter + 1, wear_bits[counter],
                 counter + 2, counter + 1 < ITEM_WEAR_MAX ?
                 wear_bits[counter + 1] : "");
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
    send_to_char("1) Small Blade\r\n"
                 "2) Large Blade\r\n"
                 "3) Blunt Weapon\r\n"
                 "4) Projectile\r\n"
                 "5) Pistol\r\n"
                 "6) Rifle\r\n"
                 "7) Automatic Weapon\r\n"
                 "8) Heavy Weapon\r\n"
                 "9) Explosive\r\n"
                 "10) Military Weapon\r\n"
                 "11) Military Armour\r\n"
                 "12) Military Ammunition\r\n"
                 "13) Class A (Paralegal) Cyberware\r\n"
                 "14) Class B (Security Grade) Cyberware\r\n"
                 "15) Class C (Military Grade) Cyberware\r\n"
                 "16) Class D Matrix\r\n"
                 "17) Class E Magic\r\n"
                 "18) Class A (Paralegal) Equipment\r\n"
                 "19) Class B (Security Grade) Equipment\r\n"
                 "20) Class C (Military Grade) Equipment\r\n"
                 "21) Class A (Pharmaceuticals) Controlled\r\n"
                 "22) Class B (BTL) Controlled\r\n"
                 "23) Class C (Biological Agents) Controlled\r\n"
                 "Select Security Category (0 for other): ", CH);
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
  send_to_char(CH, "Item number: %s%d%s\r\n", CCCYN(CH, C_CMP), d->edit_number,
               CCNRM(CH, C_CMP));
  send_to_char(CH, "1) Item keywords: %s%s%s\r\n",
               CCCYN(CH, C_CMP), d->edit_obj->text.keywords,
               CCNRM(CH, C_CMP));
  send_to_char(CH, "2) Item name: %s%s%s\r\n",
               CCCYN(CH, C_CMP), d->edit_obj->text.name,
               CCNRM(CH, C_CMP));
  send_to_char(CH, "3) Room description:\r\n%s%s%s\r\n",
               CCCYN(CH, C_CMP), d->edit_obj->text.room_desc,
               CCNRM(CH, C_CMP));
  send_to_char(CH, "4) Look description: \r\n%s\r\n",
               d->edit_obj->text.look_desc ? d->edit_obj->text.look_desc :
               "(not set)");
  sprinttype(GET_OBJ_TYPE(d->edit_obj), item_types, buf1);
  send_to_char(CH, "5) Item type: %s%s%s\r\n", CCCYN(CH, C_CMP), buf1,
               CCNRM(CH, C_CMP));
  GET_OBJ_EXTRA(d->edit_obj).PrintBits(buf1, MAX_STRING_LENGTH,
                                       extra_bits, ITEM_EXTRA_MAX);
  send_to_char(CH, "6) Item extra flags: %s%s%s\r\n", CCCYN(CH, C_CMP), buf1,
               CCNRM(CH, C_CMP));
  OBJ->obj_flags.bitvector.PrintBits(buf1, MAX_STRING_LENGTH,
                                     affected_bits, AFF_MAX);
  send_to_char(CH, "7) Item affection flags: ^c%s^n\r\n", buf1);
  OBJ->obj_flags.wear_flags.PrintBits(buf1, MAX_STRING_LENGTH,
                                      wear_bits, NUM_WEARS);
  send_to_char(CH, "8) Item wear flags: %s%s%s\r\n", CCCYN(CH, C_CMP), buf1,
               CCNRM(CH, C_CMP));
  send_to_char(CH, "9) Item weight: %s%.2f%s\r\n", CCCYN(CH, C_CMP),
               GET_OBJ_WEIGHT(d->edit_obj), CCNRM(CH, C_CMP));
  send_to_char(CH, "a) Item cost: %s%d%s\r\n", CCCYN(CH, C_CMP),
               GET_OBJ_COST(d->edit_obj), CCNRM(CH, C_CMP));
  send_to_char(CH, "b) Item availability: ^c%d^n/^c%.2f^n days\r\n", GET_OBJ_AVAILTN(d->edit_obj), GET_OBJ_AVAILDAY(d->edit_obj));
  send_to_char(CH, "c) Item timer: ^c%d^n\r\n", GET_OBJ_TIMER(d->edit_obj));
  send_to_char(CH, "d) Item Material: ^c%s^n\r\n", material_names[(int)GET_OBJ_MATERIAL(d->edit_obj)]);
  send_to_char(CH, "e) Item Barrier Rating: ^c%d^n\r\n", GET_OBJ_BARRIER(d->edit_obj));
  send_to_char(CH, "f) Item Legality: ^c%d%s^n-^c%s^n\r\n", GET_LEGAL_NUM(d->edit_obj), GET_LEGAL_PERMIT(d->edit_obj) ? "P" : "",
               legality_codes[GET_LEGAL_CODE(d->edit_obj)][0]);
  send_to_char(CH, "g) Item values: ^c%d %d %d %d %d %d %d %d %d %d %d %d^n\r\n",
               GET_OBJ_VAL(d->edit_obj, 0), GET_OBJ_VAL(d->edit_obj, 1),
               GET_OBJ_VAL(d->edit_obj, 2), GET_OBJ_VAL(d->edit_obj, 3),
               GET_OBJ_VAL(d->edit_obj, 4), GET_OBJ_VAL(d->edit_obj, 5),
               GET_OBJ_VAL(d->edit_obj, 6), GET_OBJ_VAL(d->edit_obj, 7),
               GET_OBJ_VAL(d->edit_obj, 8), GET_OBJ_VAL(d->edit_obj, 9),
               GET_OBJ_VAL(d->edit_obj, 10), GET_OBJ_VAL(d->edit_obj, 11));
  send_to_char("h) Item applies:\r\n"
               "i) Item extra descriptions:\r\n"
               "q) Quit and save\r\n"
               "x) Exit and abort\r\n"
               "Enter your choice:\r\n", CH);
  d->edit_mode = IEDIT_MAIN_MENU;
}

/***************************************************************************
 main loop (of sorts).. basically interpreter throws all input to here
 ***************************************************************************/


void iedit_parse(struct descriptor_data * d, const char *arg)
{
  long             number, j;
  long            obj_number;   /* the RNUM */
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
          if (!from_ip_zone(d->edit_number)) {
            sprintf(buf,"%s wrote new obj #%ld",
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
            for (counter = 0; counter < top_of_objt + 1; counter++) {
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
                  new_obj_proto[counter].in_room = NOWHERE;
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
              new_obj_proto[top_of_objt + 1].in_room = NOWHERE;
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
              int zone, cmd_no;
              for (zone = 0; zone <= top_of_zone_table; zone++)
                for (cmd_no = 0; cmd_no < zone_table[zone].num_cmds; cmd_no++)
                {
                  switch (ZCMD.command) {
                    case 'O':
                      ZCMD.arg1 =
                      (ZCMD.arg1 >= d->edit_obj->item_number ?
                       ZCMD.arg1 + 1 : ZCMD.arg1);
                      break;
                    case 'G':
                      ZCMD.arg1 =
                      (ZCMD.arg1 >= d->edit_obj->item_number ?
                       ZCMD.arg1 + 1 : ZCMD.arg1);
                      break;
                    case 'E':
                      ZCMD.arg1 =
                      (ZCMD.arg1 >= d->edit_obj->item_number ?
                       ZCMD.arg1 + 1 : ZCMD.arg1);
                      break;
                    case 'P':
                      ZCMD.arg1 =
                      (ZCMD.arg1 >= d->edit_obj->item_number ?
                       ZCMD.arg1 + 1 : ZCMD.arg1);
                      ZCMD.arg3 =
                      (ZCMD.arg3 >= d->edit_obj->item_number ?
                       ZCMD.arg3 + 1 : ZCMD.arg3);
                      break;
                    case 'R': /* rem obj from room */
                      ZCMD.arg2 =
                      (ZCMD.arg2 >= d->edit_obj->item_number ?
                       ZCMD.arg2 + 1 : ZCMD.arg2);
                      break;
                  }
                }
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
          // do not wanna nuke the strings, so we use ClearObject
          if (d->edit_obj)
            Mem->ClearObject(d->edit_obj);
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
          send_to_char("Enter weight:", d->character);
          d->edit_mode = IEDIT_WEIGHT;
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
          iedit_disp_val1_menu(d);
          break;
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
    case IEDIT_TYPE:
      number = atoi(arg);
      if ((number < 1) || (number > NUM_ITEMS) || (number == ITEM_CYBERWARE && !access_level(CH, LVL_ADMIN)) ||
          (number == ITEM_BIOWARE && !access_level(CH, LVL_ADMIN))) {
        send_to_char("That's not a valid choice!\r\n", d->character);
        iedit_disp_type_menu(d);
      } else if (number != 0 && GET_OBJ_TYPE(d->edit_obj) != number) {
        GET_OBJ_TYPE(d->edit_obj) = number;
        for (int index = 0; index < NUM_VALUES; index++)
          GET_OBJ_VAL(d->edit_obj, index) = 0;
      }
      
      if (number == ITEM_GUN_MAGAZINE) {
        send_to_char("NOTICE: Gun magazines were deprecated with the creation of the new ammo system. You probably don't need to make one.\r\n", d->character);
      }
      
      iedit_disp_menu(d);
      break;
    case IEDIT_EXTRAS:
      number = atoi(arg);
      if ((number < 0) || (number > ITEM_EXTRA_MAX)) {
        send_to_char("That's not a valid choice!\r\n", d->character);
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
          if (number < 0 || number > 7) {
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
          if (number < 0 || number > 6) {
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
              if (number < 0) {
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
          if (number < 1 || number > AMMO_GEL) {
            send_to_char(CH, "Invalid value. Enter Ammunition Type: ");
            return;
          }
          break;
        case ITEM_DRINKCON:
        case ITEM_FOUNTAIN:
          if (number < 1 || number > NUM_DRINK_TYPES) {
            send_to_char("Invalid choice.", CH);
            iedit_disp_val4_menu(d);
            return;
          }
          number--; // to give the correct value to the object
          break;
        case ITEM_WEAPON:
        case ITEM_FIREWEAPON:
          if (number < 0 || number > 4) {
            send_to_char("Value must be between 0 and 4.\r\n", CH);
            iedit_disp_val3_menu(d);
            return;
          }
          break;
        case ITEM_BIOWARE:
          if (number < 0 || number > 1) {
            send_to_char("Invalid choice. 1 for yes, 0 for no: ", CH);
            return;
          }
          break;
        case ITEM_CYBERWARE:
          if (number < 0 || number > 3) {
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
          if (number < 0 || number > 5) {
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
        case ITEM_WEAPON:
          if (number < 0 || number > MAX_WEAP) {
            send_to_char("Invalid choice!", d->character);
            iedit_disp_weapon_menu(d);
            return;
          }
          switch (number) {
              // Determine what attachment locations are available by default based on weapon type. 0 = available, -1 = not.
            case WEAP_LIGHT_PISTOL:
            case WEAP_HEAVY_PISTOL:
            case WEAP_MACHINE_PISTOL:
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_TOP) = 0;
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_BARREL) = 0;
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_UNDER) = -1;
              break;
            case WEAP_TASER:
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_TOP) = 0;
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_BARREL) = -1;
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_UNDER) = -1;
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
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_TOP) = -1;
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_BARREL) = -1;
              GET_OBJ_VAL(OBJ, ACCESS_LOCATION_UNDER) = -1;
          }
          break;
        case ITEM_FIREWEAPON:
          if (number < 1 || (!access_level(CH, LVL_ADMIN) && number == TYPE_BIFURCATE) || number > NUM_WEAPON_TYPES) {
            send_to_char("Invalid choice!", d->character);
            iedit_disp_weapon_menu(d);
            return;
          }
          number = TYPE_ARROW;
          break;
        case ITEM_PROGRAM:
          if (number < 1 || number > 4) {
            send_to_char("Must be between 1 and 4: ", d->character);
            iedit_disp_val4_menu(d);
          }
          break;
        case ITEM_DECK_ACCESSORY:
          if (number < 1 || number > 15) {
            send_to_char("MPCP value must be between 1 and 15!\r\nMPCP to enhance: ", CH);
            return;
          }
          break;
        case ITEM_CYBERWARE:
          switch (GET_OBJ_VAL(OBJ, 0)) {
            case CYB_DATAJACK:
            case CYB_TOOTHCOMPARTMENT:
            case CYB_HANDBLADE:
            case CYB_HANDSPUR:
            case CYB_REFLEXTRIGGER:
            case CYB_SKULL:
            case CYB_TORSO:
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
              sprintbit(GET_OBJ_VAL(OBJ, 3), hand_razors, buf1);
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
              if (real_object(number) < 1|| obj_proto[real_object(number)].obj_flags.type_flag != ITEM_CYBERDECK) {
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
            send_to_char("Invalid choice!", d->character);
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
          iedit_disp_mod_menu(d);
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
          if (number <= 0 || (number > 1 && number > (int)(GET_OBJ_VAL(d->edit_obj, 0) / 3))) {
            send_to_char(CH, "Reaction upgrade must be between 1 and %d. Reaction Upgrade: ", (int)(GET_OBJ_VAL(d->edit_obj, 0) / 3));
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
          if (!access_level(CH, LVL_ADMIN))
            if (!read_object(number, VIRTUAL)) {
              send_to_char("Invalid vnum.\r\n", CH);
              iedit_disp_val8_menu(d);
            }
          if (number > 0) {
            modified = FALSE;
            for (j = 0; (j < MAX_OBJ_AFFECT && !modified); j++)
              if (!(OBJ->affected[j].modifier)) {
                OBJ->affected[j].location = read_object(number, VIRTUAL)->affected[0].location;
                OBJ->affected[j].modifier = read_object(number, VIRTUAL)->affected[0].modifier;
                modified = TRUE;
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
          if (!access_level(CH, LVL_ADMIN))
            if (!read_object(number, VIRTUAL)) {
              send_to_char("Invalid vnum.\r\n", CH);
              iedit_disp_val9_menu(d);
              return;
            }
          if (number > 0) {
            modified = FALSE;
            for (j = 0; (j < MAX_OBJ_AFFECT && !modified); j++)
              if (!(OBJ->affected[j].modifier)) {
                OBJ->affected[j].location = read_object(number, VIRTUAL)->affected[0].location;
                OBJ->affected[j].modifier = read_object(number, VIRTUAL)->affected[0].modifier;
                modified = TRUE;
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
          if (!access_level(CH, LVL_ADMIN))
            if (!read_object(number, VIRTUAL)) {
              send_to_char("Invalid vnum.\r\n", CH);
              iedit_disp_val10_menu(d);
              return;
            }
          if (number > 0) {
            modified = FALSE;
            for (j = 0; (j < MAX_OBJ_AFFECT && !modified); j++)
              if (!(OBJ->affected[j].modifier)) {
                OBJ->affected[j].location = read_object(number, VIRTUAL)->affected[0].location;
                OBJ->affected[j].modifier = read_object(number, VIRTUAL)->affected[0].modifier;
                modified = TRUE;
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
          if (number < 0 || number > 4) {
            send_to_char("Invalid Input! Enter options (0 to quit): ", CH);
            return;
          }
          if (number == 0) {
            iedit_disp_menu(d);
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

void write_objs_to_disk(int zone)
{
  int counter, counter2, realcounter, count = 0;
  FILE *fp;
  struct obj_data *obj;
  struct extra_descr_data *ex_desc;
  
  zone = real_zone(zone);
  
  // ideally, this would just fill a VTable with vals...maybe one day
  
  sprintf(buf, "%s/%d.obj", OBJ_PREFIX, zone_table[zone].number);
  fp = fopen(buf, "w+");
  
  /* start running through all objects in this zone */
  for (counter = zone_table[zone].number * 100;
       counter <= zone_table[zone].top;
       counter++) {
    /* write object to disk */
    realcounter = real_object(counter);
    
    if (realcounter >= 0) {
      obj = obj_proto+realcounter;
      
      if (!strcmp("an unfinished object", obj->text.name))
        continue;
      
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
      
      fprintf(fp,
              "Type:\t%s\n"
              "WearFlags:\t%s\n"
              "ExtraFlags:\t%s\n"
              "AffFlags:\t%s\n"
              "Material:\t%s\n",
              item_types[(int)GET_OBJ_TYPE(obj)],
              GET_OBJ_WEAR(obj).ToString(),
              GET_OBJ_EXTRA(obj).ToString(),
              obj->obj_flags.bitvector.ToString(),
              material_names[(int)GET_OBJ_MATERIAL(obj)]);
      
      fprintf(fp,
              "[POINTS]\n"
              "\tWeight:\t%.2f\n"
              "\tBarrier:\t%d\n"
              "\tCost:\t%d\n"
              "\tAvailTN:\t%d\n"
              "\tAvailDay:\t%.2f\n"
              "\tLegalNum:\t%d\n"
              "\tLegalCode:\t%d\n"
              "\tLegalPermit:\t%d\n",
              GET_OBJ_WEIGHT(obj), GET_OBJ_BARRIER(obj), GET_OBJ_COST(obj), GET_OBJ_AVAILTN(obj), GET_OBJ_AVAILDAY(obj), 
              GET_LEGAL_NUM(obj), GET_LEGAL_CODE(obj), GET_LEGAL_PERMIT(obj));
      
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
      
      fprintf(fp, "BREAK\n");
    }
  }
  /* write final line, close */
  fprintf(fp, "END\n");
  fclose(fp);
  
  write_index_file("obj");
}
