#include <mysql/mysql.h>

#include "types.h"
#include "awake.h"
#include "db.h"
#include "comm.h"
#include "constants.h"
#include "interpreter.h"
#include "structs.h"
#include "utils.h"
#include "handler.h"
#include "bullet_pants.h"

extern MYSQL *mysql;

extern int mysql_wrapper(MYSQL *mysql, const char *buf);
extern void calc_weight(struct char_data *ch);
extern void increase_debris(struct room_data *rm);

#define POCKETS_USAGE_STRING "Syntax: POCKETS (put|get) <quantity> <ammotype> <weapontype>. Ex: pockets get 100 apds pistol\r\n"
/* The POCKETS command, used to interact with the character's ammo supply. */
ACMD(do_pockets) {
  char mode_buf[MAX_INPUT_LENGTH];
  char quantity_buf[MAX_INPUT_LENGTH];
  char ammotype_buf[MAX_INPUT_LENGTH];
  
  char *weapon_buf = NULL;
  
  int quantity, ammotype;
  
  *mode_buf = *quantity_buf = *ammotype_buf = '\0';
  /*
  pocket[s]: displays your current bullet pants contents
  pocket[s] <weapon>: displays your current bullet pants contents for the chosen weapon
  pocket[s] put|store|add <quantity> <weapon> <type>: add ammo from boxes to your bullet pants, junking empties
  pocket[s] add <box>: dump a box into your pockets and junk it
  pocket[s] get|remove|retrieve <quantity> <weapon> <type>: create a new ammo box from your bullet pants with the specified amount in it
  */
  
  skip_spaces(&argument);
  
  /****************************
  * pockets: display all ammo *
  *****************************/
  if (!*argument) {
    display_pockets_to_char(ch, ch);
    return;
  }
  
  /**************************************************
  * pockets <weapontype>: display ammo of that type *
  ***************************************************/
  for (int i = START_OF_AMMO_USING_WEAPONS; i <= END_OF_AMMO_USING_WEAPONS; i++) {
    if (!strn_cmp(argument, weapon_type[i], strlen(argument)) 
        || (*(weapon_type_aliases[i]) && str_cmp(argument, weapon_type_aliases[i]) == 0)) {
      if (print_one_weapontypes_ammo_to_string(ch, i, buf, sizeof(buf)))
        send_to_char(ch, "You have the following %s ammunition secreted about your person:%s%s\r\n",
                     weapon_type[i], 
                     PRF_FLAGGED(ch, PRF_SCREENREADER) ? " " : "\r\n",
                     buf);
      else
        send_to_char(ch, "You don't have any %s ammunition secreted about your person.\r\n", weapon_type[i]);
      return;
    }
  }
  
  // Special case was not tripped. Break out the rest of the arguments.
  weapon_buf = one_argument(one_argument(one_argument(argument, mode_buf), quantity_buf), ammotype_buf);
  
  // We split up the various buf checks, which lets stubborn people feel their way through the command.
  // ex: 'pockets add' 'pockets add 123' 'pockets add 123 apds' etc
  if (!*mode_buf) {
    send_to_char(POCKETS_USAGE_STRING, ch);
    return;
  }
  if (!is_valid_pockets_get_command(mode_buf) && !is_valid_pockets_put_command(mode_buf)) {
    send_to_char(ch, "'%s' is not a valid mode. Try 'pockets get' or 'pockets put'.\r\n", mode_buf);
    return;
  }
  
  // Sanity check quantity.
  if (!*quantity_buf) {
    send_to_char(POCKETS_USAGE_STRING, ch);
    return;
  } 
  if ((quantity = atoi(quantity_buf)) <= 0) {
    // Special case: 'pockets put <ammobox>'. Check for that here.
    if (is_valid_pockets_put_command(mode_buf)) {
      struct obj_data *ammobox;
      int quantity;
      
      if ((ammobox = get_obj_in_list_vis(ch, quantity_buf, ch->carrying))) {
        if (GET_OBJ_TYPE(ammobox) != ITEM_GUN_AMMO) {
          send_to_char(ch, "%s is not an ammo box.\r\n", capitalize(GET_OBJ_NAME(ammobox)));
          return;
        }
        
        if ((quantity = GET_AMMOBOX_QUANTITY(ammobox)) <= 0) {
          send_to_char(ch, "%s is already empty.\r\n", capitalize(GET_OBJ_NAME(ammobox)));
          return;
        }
        
        int weapontype = GET_AMMOBOX_WEAPON(ammobox);
        int ammotype = GET_AMMOBOX_TYPE(ammobox);
        
        if (quantity + GET_BULLETPANTS_AMMO_AMOUNT(ch, weapontype, ammotype) > MAX_NUMBER_OF_BULLETS_IN_PANTS) {
          send_to_char(ch, "Your pockets aren't big enough to hold everything in %s.\r\n", GET_OBJ_NAME(ammobox));
          return;
        }
        
        update_bulletpants_ammo_quantity(ch, weapontype, ammotype, quantity);
        update_ammobox_ammo_quantity(ammobox, -quantity);
        
        if (!ammobox->restring || strcmp(ammobox->restring, get_ammobox_default_restring(ammobox)) == 0) {
          send_to_char(ch, "You take %d %s from %s and secrete them about your person, then junk the empty box.\r\n", 
                       quantity, get_ammo_representation(weapontype, ammotype, quantity), GET_OBJ_NAME(ammobox));
          extract_obj(ammobox);
        } else {
          send_to_char(ch, "You take %d %s from %s and secrete them about your person.\r\n", 
                       quantity, get_ammo_representation(weapontype, ammotype, quantity), GET_OBJ_NAME(ammobox));
        }
        
        return;
      }
    }
    send_to_char("The quantity must be greater than zero.\r\n", ch);
    return;
  }
  
  // Sanity check ammotype. We run it backwards so 'explosive' does not override 'ex'.
  if (!*ammotype_buf) {
    send_to_char(POCKETS_USAGE_STRING, ch);
    return;
  }
  ammotype = -1;
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
  if (!weapon_buf || !*weapon_buf) {
    send_to_char(POCKETS_USAGE_STRING, ch);
    return;
  }
  
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
  
  /****************************************************
  * pockets put: add ammo from boxes to bullet pants. *
  *****************************************************/
  if (is_valid_pockets_put_command(mode_buf)) {
    struct obj_data *ammobox = NULL;
    
    // Cap input to pocket capacity.
    if (quantity + GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon, ammotype) > MAX_NUMBER_OF_BULLETS_IN_PANTS) {
      int ammo_delta = MAX_NUMBER_OF_BULLETS_IN_PANTS - GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon, ammotype);
      
      if (ammo_delta == 0) {
        send_to_char(ch, "You can't fit any more %s in your pockets!\r\n", 
                     get_ammo_representation(weapon, ammotype, quantity));
      } else {
        send_to_char(ch, "Your pockets aren't that big! They can only take %d more %s.\r\n", 
                     ammo_delta, get_ammo_representation(weapon, ammotype, quantity));
      }
      return;
    }
    
    // Require they have an ammo box that matches quantity.
    for (ammobox = ch->carrying; ammobox; ammobox = ammobox->next_content)
      if (GET_OBJ_TYPE(ammobox) == ITEM_GUN_AMMO 
          && GET_AMMOBOX_WEAPON(ammobox) == weapon
          && GET_AMMOBOX_TYPE(ammobox) == ammotype
          && GET_AMMOBOX_QUANTITY(ammobox) >= quantity)
            break;
            
    if (!ammobox) {
      send_to_char(ch, "You don't have an ammo box containing %d %s.",
                   quantity, get_ammo_representation(weapon, ammotype, quantity));
      return;
    }
    
    // Deduct from the ammo box.
    update_ammobox_ammo_quantity(ammobox, -quantity);
    update_bulletpants_ammo_quantity(ch, weapon, ammotype, quantity);
    
    // Message char, and ditch box if it's empty and not customized.
    if (!ammobox->restring || strcmp(ammobox->restring, get_ammobox_default_restring(ammobox)) == 0) {
      send_to_char(ch, "You take %d %s from %s and secrete them about your person, then junk the empty box.\r\n", 
                   quantity, get_ammo_representation(weapon, ammotype, quantity), GET_OBJ_NAME(ammobox));
      extract_obj(ammobox);
    } else {
      send_to_char(ch, "You take %d %s from %s and secrete them about your person.\r\n", 
                   quantity, get_ammo_representation(weapon, ammotype, quantity), GET_OBJ_NAME(ammobox));
    }
  }
  
  /******************************************************
  * pockets get: create new ammo box from bullet pants. *
  *******************************************************/
  if (is_valid_pockets_get_command(mode_buf)) {
    // We must have that much ammo to begin with.
    if (GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon, ammotype) < quantity) {
      quantity = GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon, ammotype);
      send_to_char(ch, "You only have %d %s, so you package them up and put them into an ammo box.\r\n",
                   quantity, get_ammo_representation(weapon, ammotype, quantity));
    } else {
      send_to_char(ch, "You package up %d %s and put them into a spare ammo box.\r\n", 
                   quantity, get_ammo_representation(weapon, ammotype, quantity));
    }
                 
    // Generate an ammo box and give it to them.
    struct obj_data *ammobox = read_object(OBJ_BLANK_AMMOBOX, VIRTUAL);
    GET_AMMOBOX_WEAPON(ammobox) = weapon;
    GET_AMMOBOX_TYPE(ammobox) = ammotype;
    
    // Reset the ammobox values in preparation for filling it.
    GET_OBJ_WEIGHT(ammobox) = 0.0;
    GET_OBJ_COST(ammobox) = 0;
    GET_AMMOBOX_QUANTITY(ammobox) = 0;
    
    // Fill it.
    update_ammobox_ammo_quantity(ammobox, quantity);
    update_bulletpants_ammo_quantity(ch, weapon, ammotype, -quantity);
    
    // Restring the box.
    ammobox->restring = str_dup(get_ammobox_default_restring(ammobox));
    
    // Give it to them.
    obj_to_char(ammobox, ch);
    
    return;
  }
  
  send_to_char(POCKETS_USAGE_STRING, ch);
  return;
}

/*******************
* Database helpers *
********************/

// Checks to see if the BP table is in yet.
bool have_bullet_pants_table() {  
  bool have_table = FALSE;
  
  MYSQL_RES *res;
  MYSQL_ROW row;
  
  mysql_wrapper(mysql, "SHOW TABLES LIKE 'pfiles_ammo';");
  
  if (!(res = mysql_use_result(mysql)))
    return FALSE;
  
  // Iterate through all the results and update the respective ammo values.
  if ((row = mysql_fetch_row(res)) && mysql_field_count(mysql))
    have_table = TRUE;
    
  mysql_free_result(res);
  
  return have_table;
}

// Saves the ch's bullet pants to db.
void save_bullet_pants(struct char_data *ch) {  
  if (!ch || IS_NPC(ch)) {
    mudlog("SYSERR: Null or NPC character passed to save_bullet_pants!", ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  // Delete their current entries from the DB.
  snprintf(buf, sizeof(buf), "DELETE FROM pfiles_ammo WHERE idnum = %ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  
  // table design: idnum weapon normal apds explosive ex flechette gel
  
  // For each weapon, write a line to the DB if they have ammo for that weapon.
  for (int wp = START_OF_AMMO_USING_WEAPONS; wp <= END_OF_AMMO_USING_WEAPONS; wp++) {
    for (int am = AMMO_NORMAL; am < NUM_AMMOTYPES; am++) {
      // Do we need to save data for this particular weapon?
      if (GET_BULLETPANTS_AMMO_AMOUNT(ch, wp, am) > 0) {
        snprintf(buf, sizeof(buf), "INSERT INTO pfiles_ammo (idnum, weapon, normal, apds, explosive, ex, flechette, gel) "
                                   " VALUES (%ld, %d, %u, %u, %u, %u, %u, %u);", 
                                   GET_IDNUM(ch),
                                   wp,
                                   GET_BULLETPANTS_AMMO_AMOUNT(ch, wp, AMMO_NORMAL),
                                   GET_BULLETPANTS_AMMO_AMOUNT(ch, wp, AMMO_APDS),
                                   GET_BULLETPANTS_AMMO_AMOUNT(ch, wp, AMMO_EXPLOSIVE),
                                   GET_BULLETPANTS_AMMO_AMOUNT(ch, wp, AMMO_EX),
                                   GET_BULLETPANTS_AMMO_AMOUNT(ch, wp, AMMO_FLECHETTE),
                                   GET_BULLETPANTS_AMMO_AMOUNT(ch, wp, AMMO_GEL));
        mysql_wrapper(mysql, buf);
        
        // Done saving all ammo types for this weapon.
        break;
      }
    }
  }
}

// Loads the ch's bullet pants from DB.
void load_bullet_pants(struct char_data *ch) {
  MYSQL_RES *res;
  MYSQL_ROW row;
  
  int weapon;
  
  if (!ch || IS_NPC(ch)) {
    mudlog("SYSERR: Null or NPC character passed to load_bullet_pants!", ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  // For each weapon, read their ammo from the DB (if any).
  snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_ammo WHERE idnum = %ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  
  // If they have no ammo at all, bail out.
  if (!(res = mysql_use_result(mysql))) {
    return;
  }
  
  // Iterate through all the results and update the respective ammo values.
  while ((row = mysql_fetch_row(res))) {
    weapon = atoi(row[1]);
    GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon, AMMO_NORMAL) = atoi(row[2]);
    GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon, AMMO_APDS) = atoi(row[3]);
    GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon, AMMO_EXPLOSIVE) = atoi(row[4]);
    GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon, AMMO_EX) = atoi(row[5]);
    GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon, AMMO_FLECHETTE) = atoi(row[6]);
    GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon, AMMO_GEL) = atoi(row[7]);
  }
  
  mysql_free_result(res);
  
  // Note: We don't update their carry weight-- the caller is assumed to do that after all loading is complete.
}

/*******************
 * Everything Else *
 *******************/

// Returns TRUE on success, FALSE otherwise. Add quantity to ch's bullet pants.
bool update_bulletpants_ammo_quantity(struct char_data *ch, int weapon, int ammotype, int quantity) {
  if (!ch) {
    mudlog("SYSERR: update_bulletpants_ammo_quantity received NULL ch.", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  if (weapon < START_OF_AMMO_USING_WEAPONS || weapon > END_OF_AMMO_USING_WEAPONS) {
    snprintf(buf, sizeof(buf), "SYSERR: update_bulletpants_ammo_quantity received invalid weapon %d.", weapon);
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  if (ammotype < AMMO_NORMAL || ammotype >= NUM_AMMOTYPES) {
    snprintf(buf, sizeof(buf), "SYSERR: update_bulletpants_ammo_quantity received invalid ammotype %d.", ammotype);
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  if (quantity > MAX_NUMBER_OF_BULLETS_IN_PANTS || quantity < -MAX_NUMBER_OF_BULLETS_IN_PANTS) {
    snprintf(buf, sizeof(buf), "SYSERR: update_bulletpants_ammo_quantity received invalid quantity %d.", quantity);
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon, ammotype) += quantity;
  calc_weight(ch);
  return TRUE;
}

// Calculates the BP weight of the character.
float get_bulletpants_weight(struct char_data *ch) {
  float weight = 0.0;
  
  for (int wp = START_OF_AMMO_USING_WEAPONS; wp <= END_OF_AMMO_USING_WEAPONS; wp++)
    for (int am = AMMO_NORMAL; am < NUM_AMMOTYPES; am++)
      weight += GET_BULLETPANTS_AMMO_AMOUNT(ch, wp, am) * ammo_type[am].weight;
  
  return weight;
}

// I went kinda crazy with the aliases for these things, so I put them into functions to avoid code reuse.
bool is_valid_pockets_put_command(char *mode_buf) {
  return str_str(mode_buf, "put") || str_str(mode_buf, "store") || str_str(mode_buf, "add") || str_str(mode_buf, "stow") || str_str(mode_buf, "load");
}
bool is_valid_pockets_get_command(char *mode_buf) {
  return str_str(mode_buf, "get") || str_str(mode_buf, "remove") || str_str(mode_buf, "retrieve");
}

// Returns 'APDS heavy pistol rounds' or equivalent.
const char *get_ammo_representation(int weapon, int ammotype, int quantity) {
  static char results_buf[500];
  
  if (weapon < START_OF_AMMO_USING_WEAPONS || weapon > END_OF_AMMO_USING_WEAPONS) {
    snprintf(buf, sizeof(buf), "SYSERR: get_ammo_representation: %d is out of bounds for weapon type.", weapon);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return "ERROR";
  }
  
  if (ammotype < 0 || ammotype >= NUM_AMMOTYPES) {
    snprintf(buf, sizeof(buf), "SYSERR: get_ammo_representation: %d is out of bounds for ammo type.", ammotype);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return "ERROR";
  }
  
  if (quantity < 0 || quantity > MAX_NUMBER_OF_BULLETS_IN_PANTS) {
    snprintf(buf, sizeof(buf), "SYSERR: get_ammo_representation: %d is out of bounds for quantity.", quantity);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return "ERROR";
  }
  
  snprintf(results_buf, sizeof(results_buf), "%s %s %s%s",
           ammo_type[ammotype].name,
           weapon_type[weapon],
           get_weapon_ammo_name_as_string(weapon), 
           quantity != 1 ? "s" : "");
           
  return results_buf;
}

// Prints to buf all the ammotypes the character has for one weapon.
bool print_one_weapontypes_ammo_to_string(struct char_data *ch, int wp, char *buf, int bufsize) {
  unsigned short amount;
  bool printed_anything_yet = FALSE;
  
  if (!ch || !buf) {
    mudlog("SYSERR: print_one_weapontypes_ammo_to_string received NULL ch or buf", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  // Zero the first char of the buf to prevent adding our good data to garbage.
  *buf = '\0';
  
  // We skip the header and assume the caller will print it. Just print the ammo.
  for (int am = AMMO_NORMAL; am < NUM_AMMOTYPES; am++) {
    if ((amount = GET_BULLETPANTS_AMMO_AMOUNT(ch, wp, am))) {
      snprintf(ENDOF(buf), bufsize - strlen(buf), "%s%u %s %s%s%s",
               PRF_FLAGGED(ch, PRF_SCREENREADER) && printed_anything_yet ? ", " : "    ",
               amount,
               ammo_type[am].name,
               get_weapon_ammo_name_as_string(wp),
               amount != 1 ? "s" : "",
               PRF_FLAGGED(ch, PRF_SCREENREADER) ? "" : "\r\n");
               
      printed_anything_yet = TRUE;
    }
  }
    
  return printed_anything_yet;
}

// Show the victim's pockets to the character.
void display_pockets_to_char(struct char_data *ch, struct char_data *vict) {  
  if (!ch || !vict) {
    mudlog("SYSERR: display_pockets_to_char received a null ch or vict.", ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  snprintf(buf, sizeof(buf), "%s %s the following ammunition secreted about %s person:\r\n",
           ch == vict ? "You"  : GET_CHAR_NAME(vict),
           ch == vict ? "have" : "has",
           ch == vict ? "your" : HSHR(vict));
  
  bool have_something_to_print = FALSE;
  
  for (int wp = START_OF_AMMO_USING_WEAPONS; wp <= END_OF_AMMO_USING_WEAPONS; wp++) {
    if (print_one_weapontypes_ammo_to_string(vict, wp, buf2, sizeof(buf2) - 1)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  ^c%s^n:%s%s\r\n", 
               weapon_type[wp],
               PRF_FLAGGED(ch, PRF_SCREENREADER) ? " " : "\r\n",
               buf2);
      have_something_to_print = TRUE;
    }
  }
  
  if (!have_something_to_print) {
    snprintf(buf, sizeof(buf), "%s %s no ammunition secreted about %s person.\r\n",
             ch == vict ? "You"  : GET_CHAR_NAME(vict),
             ch == vict ? "have" : "has",
             ch == vict ? "your" : HSHR(vict));
  } else {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n%s %s carrying %.2f kilos of ammunition.", 
             ch == vict ? "You"  : GET_CHAR_NAME(vict),
             ch == vict ? "are"  : "is",
             get_bulletpants_weight(vict));
  }
  
  send_to_char(buf, ch);
}

// Compose and return the default restring for an ammobox.
const char *get_ammobox_default_restring(struct obj_data *ammobox) {
  static char restring[500];
  snprintf(restring, sizeof(restring), "a box of %s %s ammunition", 
    ammo_type[GET_AMMOBOX_TYPE(ammobox)].name,
    weapon_type[GET_AMMOBOX_WEAPON(ammobox)]
  );
  return restring;
}

// Reloads a weapon from a character's ammo stores.
bool reload_weapon_from_bulletpants(struct char_data *ch, struct obj_data *weapon, int ammotype) {
  struct obj_data *magazine = NULL;
  
  if (!ch) {
    mudlog("SYSERR: Invalid character in reload_weapon_from_bulletpants.", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  if ((ammotype < AMMO_NORMAL || ammotype >= NUM_AMMOTYPES) && ammotype != -1) {
    snprintf(buf, sizeof(buf), "SYSERR: Ammotype %d out of bounds in reload_weapon_from_bulletpants.", ammotype);
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  // TODO: Do prelim checks like 'is it a gun' etc
  
  // Ensure it has a magazine.
  if (!(magazine = weapon->contains)) {
    magazine = read_object(OBJ_BLANK_MAGAZINE, VIRTUAL);
    // Set the max ammo and weapon type here.
    GET_MAGAZINE_BONDED_MAXAMMO(magazine) = GET_WEAPON_MAX_AMMO(weapon);
    GET_MAGAZINE_BONDED_ATTACKTYPE(magazine) = GET_WEAPON_ATTACK_TYPE(weapon);
    
    // No specified ammotype means 'reload with whatever's in the gun'. In this case, nothing was there, so we go with normal.
    GET_MAGAZINE_AMMO_TYPE(magazine) = (ammotype == -1 ? AMMO_NORMAL : ammotype);
    
    // Restring it, although I don't think anyone will ever see the restring. Better safe than sorry.
    snprintf(buf, sizeof(buf), "a %d-round %s magazine", GET_MAGAZINE_BONDED_MAXAMMO(magazine), weapon_type[GET_MAGAZINE_BONDED_ATTACKTYPE(magazine)]);
    DELETE_ARRAY_IF_EXTANT(magazine->restring);
    magazine->restring = strdup(buf);
    
    // Load the weapon with the magazine.
    obj_to_obj(magazine, weapon);
  }
  
  // Rectify ammotype if none was provided.
  if (ammotype == -1)
    ammotype = GET_MAGAZINE_AMMO_TYPE(magazine);
    
  int weapontype = GET_WEAPON_ATTACK_TYPE(weapon);
    
  int have_ammo_quantity = GET_BULLETPANTS_AMMO_AMOUNT(ch, weapontype, ammotype);
  
  // No ammo at all...
  if (have_ammo_quantity == 0) {
    send_to_char(ch, "You don't have any %s in your pockets to reload %s with.\r\n", get_ammo_representation(weapontype, ammotype, 0), GET_OBJ_NAME(weapon));
    return FALSE;
  }
  
  // Changing ammotypes? Put the old ammo back in your pants and update the magazine..
  if (GET_MAGAZINE_AMMO_TYPE(magazine) != ammotype) {
    if (GET_MAGAZINE_AMMO_COUNT(magazine) > 0) {
      update_bulletpants_ammo_quantity(ch, weapontype, GET_MAGAZINE_AMMO_TYPE(magazine), GET_MAGAZINE_AMMO_COUNT(magazine));
      send_to_char(ch, "You stow %d %s back into your pockets.\r\n", 
                   GET_MAGAZINE_AMMO_COUNT(magazine), 
                   get_ammo_representation(weapontype, GET_MAGAZINE_AMMO_TYPE(magazine), 0));
    }
    GET_MAGAZINE_AMMO_TYPE(magazine) = ammotype;
    GET_MAGAZINE_AMMO_COUNT(magazine) = 0;
  }
  
  int required_ammo_quantity = GET_MAGAZINE_BONDED_MAXAMMO(magazine) - GET_MAGAZINE_AMMO_COUNT(magazine);
  
  // Try to fill it back up with bullets from pants.
  if (have_ammo_quantity >= required_ammo_quantity) {
    // ezpz, reload it and done.
    send_to_char(ch, "You reload %s with %s.\r\n", 
                 GET_OBJ_NAME(weapon), 
                 get_ammo_representation(weapontype, ammotype, required_ammo_quantity));
    act("$n reloads $p.", TRUE, ch, weapon, NULL, TO_ROOM);
    update_bulletpants_ammo_quantity(ch, weapontype, ammotype, -required_ammo_quantity);
    
    // Does doing it this way mean the amount of ammo we load into the weapon becomes weightless? Yes. Do I care? Ehhhhhhh. TODO
    GET_MAGAZINE_AMMO_COUNT(magazine) += required_ammo_quantity;
  } else {
    // Just put in whatever you have.
    send_to_char(ch, "You reload %s with the last of your %s.\r\n", GET_OBJ_NAME(weapon), get_ammo_representation(weapontype, ammotype, 0));
    act("$n reloads $p.", TRUE, ch, weapon, NULL, TO_ROOM);
    update_bulletpants_ammo_quantity(ch, weapontype, ammotype, -have_ammo_quantity);
    GET_MAGAZINE_AMMO_COUNT(magazine) += have_ammo_quantity;
  }
  
  // Update weapons debris, but only if in combat. Presumably they chuck it out the window if they're in a car.
  if (FIGHTING(ch) || FIGHTING_VEH(ch))
    increase_debris(get_ch_in_room(ch));
  return TRUE;
}

// Shotguns fire shells, launchers fire missiles, etc.
const char *get_weapon_ammo_name_as_string(int weapon_type) {
  switch (weapon_type) {
    case WEAP_SHOTGUN:
    case WEAP_CANNON:
      return "shell";
    case WEAP_MISS_LAUNCHER:
      return "missile";
    case WEAP_GREN_LAUNCHER:
      return "grenade";
    case WEAP_TASER:
      return "dart";
    default:
      return "round";
  }
}

// Shorthand for various weapon types. "" for nothing.
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

// The order in which an NPC will prefer to use ammo types.
int npc_ammo_usage_preferences[] = {
  AMMO_APDS,
  AMMO_EX,
  AMMO_EXPLOSIVE,
  AMMO_NORMAL,
  AMMO_GEL,
  AMMO_FLECHETTE
};

// things left to implement:
/* 
 - reloading of mounted weapons (this code is probably totes broken right now)
 - staff bullet pants set command mode
 - write pockets help file
 - better formatting for 'pockets <weapon>'
 - add ability to split apart ammo boxes
 - pockets add <box>: put box in pockets, then junk box
 - shopkeepers sell ammo that goes straight into your pockets
 - enable loading from boxes on ground / in vehicle
 - figure out what happens to pockets on death
 - pockets put all / pockets get all
*/
