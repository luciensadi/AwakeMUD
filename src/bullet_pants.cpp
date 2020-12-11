#include "types.h"
#include "awake.h"
#include "db.h"
#include "comm.h"
#include "constants.h"
#include "interpreter.h"
#include "structs.h"
#include "utils.h"
#include "bullet_pants.h"

#define POCKETS_USAGE_STRING "Syntax: POCKETS (put|get) <quantity> <ammotype> <weapontype>. Ex: pockets get 100 apds pistol\r\n"
/* The POCKETS command, used to interact with the character's ammo supply. */
ACMD(do_pockets) {
  char mode_buf[MAX_INPUT_LENGTH];
  char quantity_buf[MAX_INPUT_LENGTH];
  char ammotype_buf[MAX_INPUT_LENGTH];
  
  char *weapon_buf = NULL;
  
  *mode_buf = *quantity_buf = *ammotype_buf = '\0';
  /*
  pocket[s]: displays your current bullet pants contents
  pocket[s] put|store|add <quantity> <weapon> <type>: add ammo from boxes to your bullet pants
  pocket[s] get|remove|retrieve <quantity> <weapon> <type>: create a new ammo box from your bullet pants with the specified amount in it
  */
  
  skip_spaces(&argument);
  
  // no arg: Display current bullet pants contents.
  if (!*argument) {
    display_pockets_to_char(ch, ch);
    return;
  }
  
  // Looks a bit weird, but this pulls one word each for mode, qty, and ammotype, then puts the rest in weapon_buf.
  weapon_buf = one_argument(one_argument(one_argument(argument, mode_buf), quantity_buf), ammotype_buf);
  
  if (!*mode_buf || !*quantity_buf || !weapon_buf || !*weapon_buf || !*ammotype_buf) {
    send_to_char(POCKETS_USAGE_STRING, ch);
    return;
  }
  
  // Sanity check quantity.
  int quantity = atoi(quantity_buf);
  if (quantity <= 0) {
    send_to_char("The quantity must be greater than zero.", ch);
    return;
  }
  
  // Sanity check ammotype. We run it backwards so 'explosive' does not override 'ex'.
  int ammotype = -1;
  for (int i = NUM_AMMOTYPES - 1; i >= AMMO_NORMAL ; i--) {
    if (str_str(ammo_type[i].name, ammotype_buf)) {
      ammotype = i;
      break;
    }
  }
  if (ammotype == -1) {
    send_to_char(ch, "'%s' is not a recognized ammo type. Valid types are: ", ammotype_buf);
    bool is_first = TRUE;
    for (int i = AMMO_NORMAL; i < NUM_AMMOTYPES; i++) {
      send_to_char(ch, "%s%s", is_first ? "" : ", ", ammo_type[i].name);
      is_first = FALSE;
    }
    send_to_char(".\r\n", ch);
    return;
  }
  
  // Sanity check weapon.
  int weapon = -1;
  for (int i = START_OF_AMMO_USING_WEAPONS; i <= END_OF_AMMO_USING_WEAPONS; i++) {
    if (!strn_cmp(weapon_buf, weapon_type[i], strlen(weapon_buf)) 
        || (*(weapon_type_aliases[i]) && str_cmp(weapon_buf, weapon_type_aliases[i]) == 0)) {
      weapon = i;
      break;
    }
  }
  if (weapon == -1) {
    send_to_char(ch, "'%s' is not a recognized weapon type. Valid types are: ", weapon_buf);
    bool is_first = TRUE;
    for (int i = START_OF_AMMO_USING_WEAPONS; i <= END_OF_AMMO_USING_WEAPONS; i++) {
      send_to_char(ch, "%s%s", is_first ? "" : ", ", weapon_type[i]);
      if (*(weapon_type_aliases[i]))
        send_to_char(ch, " (%s)", weapon_type_aliases[i]);
      is_first = FALSE;
    }
    send_to_char(".\r\n", ch);
    return;
  }
  
  // put <quantity> <weapon> <type>: add ammo from boxes to bullet pants.
  if (str_str(mode_buf, "put") || str_str(mode_buf, "store") || str_str(mode_buf, "add") || str_str(mode_buf, "stow")) {
    // TODO: Require that they have an ammo box with at least quantity of rounds in it.
    
    GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon, ammotype) += quantity;
    send_to_char(ch, "You secrete %d %s %s %s%s about your person.\r\n",
                 quantity, 
                 ammo_type[ammotype].name,
                 weapon_type[weapon],
                 get_weapon_ammo_name_as_string(weapon), 
                 quantity != 1 ? "s" : "");
    return;
  }
  
  // get <quantity> <weapon> <type>: create new ammo box from bullet pants.
  if (str_str(mode_buf, "get") || str_str(mode_buf, "remove") || str_str(mode_buf, "retrieve")) {
    // We must have that much ammo to begin with.
    if (GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon, ammotype) < quantity)
      quantity = GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon, ammotype);
    
    GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon, ammotype) -= quantity;
    send_to_char(ch, "You package up %d %s %s %s%s and put them into an ammo box.\r\n",
                 quantity,
                 ammo_type[ammotype].name,
                 weapon_type[weapon],
                 get_weapon_ammo_name_as_string(weapon), 
                 quantity != 1 ? "s" : "");
                 
    // TODO: Generate an ammo box.
    return;
  }
  
  if (!*mode_buf || !*quantity_buf || !*weapon_buf || !*ammotype_buf) {
    send_to_char(POCKETS_USAGE_STRING, ch);
    return;
  }
}

/**********************************
* Helper functions and constants. *
**********************************/

void display_pockets_to_char(struct char_data *ch, struct char_data *vict) {
  int amount, wp, am;
  bool printed_for_this_weapon_yet;
  
  if (!ch || !vict) {
    mudlog("SYSERR: display_pockets_to_char received a null ch or vict.", ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  snprintf(buf, sizeof(buf), "%s %s the following ammunition secreted about %s person:\r\n",
           ch == vict ? "You"  : GET_CHAR_NAME(vict),
           ch == vict ? "have" : "has",
           ch == vict ? "your" : HSHR(vict));
  
  bool have_something_to_print = FALSE;
  
  for (wp = START_OF_AMMO_USING_WEAPONS; wp < END_OF_AMMO_USING_WEAPONS; wp++) {
    printed_for_this_weapon_yet = FALSE;
    snprintf(buf2, sizeof(buf2), "%18s: ", weapon_type[wp]);
    
    for (am = AMMO_NORMAL; am < NUM_AMMOTYPES; am++) {
      if ((amount = GET_BULLETPANTS_AMMO_AMOUNT(vict, wp, am))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%d %s %s%s",
                 printed_for_this_weapon_yet ? ", " : buf2,
                 amount,
                 ammo_type[am].name,
                 get_weapon_ammo_name_as_string(wp),
                 amount != 1 ? "s" : "");          
                 
        have_something_to_print = TRUE;
        printed_for_this_weapon_yet = TRUE;
      }
    }
    
    if (printed_for_this_weapon_yet)
      strncat(buf, "\r\n", sizeof(buf) - strlen(buf) - 1);
  }
  
  if (have_something_to_print)
    send_to_char(buf, ch);
  else {
    snprintf(buf, sizeof(buf), "%s %s no ammunition secreted about %s person.\r\n",
             ch == vict ? "You"  : GET_CHAR_NAME(vict),
             ch == vict ? "have" : "has",
             ch == vict ? "your" : HSHR(vict));
  }
}

const char *get_weapon_ammo_name_as_string(int weapon_type) {
  switch (weapon_type) {
    case WEAP_SHOTGUN:
    case WEAP_CANNON:
      return "shell";
    case WEAP_MISS_LAUNCHER:
      return "rocket";
    case WEAP_GREN_LAUNCHER:
      return "grenade";
    case WEAP_TASER:
      return "dart";
    default:
      return "round";
  }
}

const char *weapon_type_aliases[] =
{
  "",
  "",
  "",
  "",
  "",
  "holdout",
  "LP",
  "MP",
  "HP",
  "",
  "SMG",
  "",
  "",
  "AR",
  "SG",
  "LMG",
  "MMG",
  "HMG",
  "cannon",
  "",
  "",
  "rocket",
  ""
};

// things left to implement:
/* 
 - room debris counter, its display
 - purging of magazines once ejected (in that case, do we even want to eject the mag, or just keep it there and change its values)
 - saving / loading for PC bullet pants
 - medit of NPC bullet pants
 - resetting of NPC bullet pants
 - reloading from bullet pants (PC / NPC)
 - - handheld weapons
 - - mounted weapons? Or just use boxes?
 - use of ammo boxes with PC bullet pants
 - weight calculations should take into account PC bullet pants
 - staff bullet pants set command mode
 - staff bullet pants show command mode
*/
