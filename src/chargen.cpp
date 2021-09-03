#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <mysql/mysql.h>
#include "structs.h"
#include "db.h"
#include "dblist.h"
#include "newdb.h"
#include "utils.h"
#include "interpreter.h"
#include "comm.h"
#include "handler.h"
#include "newmagic.h"
#include "awake.h"
#include "constants.h"
#include "config.h"
#include "chargen.h"
#include "archetypes.h"
#include "bullet_pants.h"

#define CH d->character

extern MYSQL *mysql;
extern int mysql_wrapper(MYSQL *mysql, const char *buf);
extern void display_help(char *help, int help_len, const char *arg, struct char_data *ch);
extern void initialize_pocket_secretary(struct obj_data *sec);
extern void add_phone_to_list(struct obj_data *obj);
extern void perform_wear(struct char_data * ch, struct obj_data * obj, int where, bool print_messages);
extern int find_eq_pos(struct char_data * ch, struct obj_data * obj, char *arg);
extern void do_start(struct char_data * ch, bool wipe_skills);

SPECIAL(pocket_sec);

static void start_game(descriptor_data *d);

int get_minimum_attribute_points_for_race(int race);
void init_char_sql(struct char_data *ch);
void init_create_vars(struct descriptor_data *d);

ACMD_DECLARE(do_help);

/*********
EXPECTED FLOW:
- name
- password
- pronouns
- archetype or custom?
ARCHETYPE:
- brief descs of archetypes, then archetype selection menu
- help available through arch selection menu
- once selected, put into abbreviated chargen (basics, then special info) with all skills/etc already set, and a bail-out-and-play exit
CUSTOM:
- standard AW chargen tree
*/

// Invoked from interpreter, this menu puts the character in a state to select their sex.
void ccr_pronoun_menu(struct descriptor_data *d) {
  SEND_TO_Q("What pronouns will your character use? (M)ale, (F)emale, (N)eutral: ", d);
  d->ccr.mode = CCR_PRONOUNS;
}

void ccr_race_menu(struct descriptor_data *d) {
  SEND_TO_Q("\r\nSelect a race:"
            "\r\n  [1] Human"
            "\r\n  [2] Dwarf"
            "\r\n  [3] Elf"
            "\r\n  [4] Ork"
            "\r\n  [5] Troll"
            "\r\n  [6] Cyclops"
            "\r\n  [7] Koborokuru"
            "\r\n  [8] Fomori"
            "\r\n  [9] Menehune"
            "\r\n  [A] Hobgoblin"
            "\r\n  [B] Giant"
            "\r\n  [C] Gnome"
            "\r\n  [D] Oni"
            "\r\n  [E] Wakyambi"
            "\r\n  [F] Ogre"
            "\r\n  [G] Minotaur"
            "\r\n  [H] Satyr"
            "\r\n  [I] Night-One"
            "\r\n  ?# (for help on a particular race)"
            "\r\n"
            "\r\nRace: ", d);
  d->ccr.mode = CCR_RACE;
}

void ccr_archetype_selection_menu(struct descriptor_data *d) {
  SEND_TO_Q("\r\nThe following archetypes are available:"
            "\r\n", d);
  
  for (int i = 0; i < NUM_CCR_ARCHETYPES; i++) {
    snprintf(buf, sizeof(buf), "\r\n%d) %-15s (%s)", i + 1, archetypes[i]->name, archetypes[i]->difficulty_rating);
    SEND_TO_Q(buf, d);
  }
  
  SEND_TO_Q("\r\n\r\nC) Advanced (custom) creation for experienced players\r\n", d);
  
  SEND_TO_Q("\r\n\r\nEnter the number of the archetype you'd like to play, or 'help' for more info: ", d);
  
  d->ccr.mode = CCR_ARCHETYPE_SELECTION_MODE;
}

void ccr_confirm_switch_to_custom(struct descriptor_data *d) {
  SEND_TO_Q("Are you sure you want to enter custom creation? This process will take a bit longer than the archetypal creation process, and we recommend having the Shadowrun 3 sourcebook on hand for reference.\r\n", d);
  d->ccr.mode = CCR_ARCHETYPE_MODE;
}

void ccr_confirm_switch_to_custom_parse(struct descriptor_data *d, const char *arg) {
  switch (*arg) {
    case 'y':
    case 'Y':
      ccr_race_menu(d);
      break;
    case 'n':
    case 'N':
      ccr_archetype_selection_menu(d);
      break;
    default:
      SEND_TO_Q("Please enter 'yes' or 'no': ", d);
      break;
  }
}

#define ATTACH_IF_EXISTS(vnum, location) \
if ((vnum) > 0) { \
  if ((temp_obj = read_object((vnum), VIRTUAL))) { \
    attach_attachment_to_weapon(temp_obj, weapon, NULL, (location)); \
    extract_obj(temp_obj); \
  } else { \
    snprintf(buf, sizeof(buf), "SYSERR: Attempting to attach nonexistent item %ld to %s of weapon for archetype %s.", (vnum), gun_accessory_locations[(location)], archetypes[i]->name); \
    mudlog(buf, CH, LOG_SYSLOG, TRUE); \
  } \
}

void archetype_selection_parse(struct descriptor_data *d, const char *arg) {  
  if (!*arg) {
    ccr_archetype_selection_menu(d);
    return;
  }
  
  // Help mode.
  if (*arg == '?') {
    SEND_TO_Q("\r\nYou can get help on any archetype by typing HELP <archetype>. If you have more questions, feel free to ask in our Discord channel, linked from https://awakemud.com!", d);
    return;
  }
  if (!strncmp("help", arg, MIN(strlen("help"), strlen(arg)))) {
    char helpbuf[100];
    char non_const_arg[MAX_INPUT_LENGTH];
    strncpy(non_const_arg, arg, sizeof(non_const_arg) - 1);
    char *line = any_one_arg(non_const_arg, helpbuf);
    if (!*line) {
      strncpy(helpbuf, " archetypes", sizeof(helpbuf) - 1);
      do_help(d->character, helpbuf, 0, 0);
    } else {
      // Check for numbers, like 'help 1'. Skip the first char of line since it's expected to be a space.
      if (*(line) && *(line + 1)) {
        switch ((int) (*(line + 1) - '0') - 1) {
          case ARCHETYPE_STREET_SAMURAI:
            strncpy(line, " street samurai", sizeof(line) - 1);
            break;
          case ARCHETYPE_ADEPT:
            strncpy(line, " adepts", sizeof(line) - 1);
            break;
          case ARCHETYPE_SHAMANIC_MAGE:
            strncpy(line, " shamans", sizeof(line) - 1);
            break;
          case ARCHETYPE_STREET_MAGE:
            strncpy(line, " mages", sizeof(line) - 1);
            break;
          case ARCHETYPE_DECKER:
            strncpy(line, " deckers", sizeof(line) - 1);
            break;
        }
      }
      
      do_help(d->character, line, 0, 0);
    }
    SEND_TO_Q("\r\n\r\nYou can use 'HELP <topic>' to get more info on a topic, or hit return to see the list of archetypes again.\r\n", d);
    return;
  }
  
  // Custom creation mode.
  if (*arg == 'c' || *arg == 'C') {
    ccr_confirm_switch_to_custom(d);
    return;
  }
  
  // Account for the incrementing we did when presenting the options.
  int i = atoi(arg) - 1;
  
  // Selection mode.
  if (i < 0 || i >= NUM_CCR_ARCHETYPES) {
    SEND_TO_Q("\r\nThat's not a valid archetype. Enter the number of the archetype you'd like to play as: ", d);
    return;
  }
  
  // Valid archetype number was input, so set them up with it. First, declare temp vars.
  struct obj_data *temp_obj = NULL;
  
  // Set race.
  GET_RACE(CH) = archetypes[i]->race;
  
  // Set attributes.
  for (int attr = BOD; attr <= WIL; attr++)
    GET_REAL_ATT(CH, attr) = archetypes[i]->attributes[attr];
      
  // Set magic data.
  GET_TRADITION(CH) = archetypes[i]->tradition;
  GET_REAL_MAG(CH) = archetypes[i]->magic;
  GET_ASPECT(CH) = archetypes[i]->aspect;
  
  if (GET_TRADITION(CH) == TRAD_SHAMANIC) {
    GET_TOTEM(CH) = archetypes[i]->totem;
    GET_TOTEMSPIRIT(CH) = archetypes[i]->totemspirit;
  }
    
  
  // Grant forcepoints for bonding purposes.
  GET_FORCE_POINTS(CH) = archetypes[i]->forcepoints;
  
  // Set spells, if any.
  for (int spell_idx = 0; spell_idx < NUM_ARCHETYPE_SPELLS; spell_idx++)
    if (archetypes[i]->spells[spell_idx][0]) {
      struct spell_data *spell = new spell_data;
      spell->name = str_dup(spells[archetypes[i]->spells[spell_idx][0]].name);
      spell->type = archetypes[i]->spells[spell_idx][0]; // ex: SPELL_MANABOLT
      spell->subtype = archetypes[i]->spells[spell_idx][1]; // ex: BOD QUI STR - used for incattr
      spell->force = archetypes[i]->spells[spell_idx][2];
      spell->next = GET_SPELLS(CH);
      GET_SPELLS(CH) = spell;
      GET_SPELLS_DIRTY_BIT(CH) = TRUE;
    } else {
      break;
    }
  
  // Assign adept abilities.
  for (int power = 0; power < NUM_ARCHETYPE_ABILITIES; power++)
    if (archetypes[i]->powers[power][0]) {
      SET_POWER_TOTAL(CH, archetypes[i]->powers[power][0], archetypes[i]->powers[power][1]);
    } else
      break;
  
  // Equip weapon.
  if (archetypes[i]->weapon > 0) {
    // snprintf(buf, sizeof(buf), "Attempting to attach %lu %lu %lu...", archetypes[i]->weapon_top, archetypes[i]->weapon_barrel, archetypes[i]->weapon_under);
    // log(buf);
    struct obj_data *weapon = read_object(archetypes[i]->weapon, VIRTUAL);

    ATTACH_IF_EXISTS(archetypes[i]->weapon_top, ACCESS_ACCESSORY_LOCATION_TOP);
    ATTACH_IF_EXISTS(archetypes[i]->weapon_barrel, ACCESS_ACCESSORY_LOCATION_BARREL);
    ATTACH_IF_EXISTS(archetypes[i]->weapon_under, ACCESS_ACCESSORY_LOCATION_UNDER);

    // Fill pockets.
    if (archetypes[i]->ammo_q)
      update_bulletpants_ammo_quantity(CH, GET_WEAPON_ATTACK_TYPE(weapon), AMMO_NORMAL, archetypes[i]->ammo_q);
    
    // Put the weapon in their inventory.
    obj_to_char(weapon, CH);
  }
  
  // Grant modulator (unbonded, unworn).
  if (archetypes[i]->modulator > 0) {
    if ((temp_obj = read_object(archetypes[i]->modulator, VIRTUAL))) {
      obj_to_char(temp_obj, CH);
    } else {
      snprintf(buf, sizeof(buf), "SYSERR: Invalid modulator %ld specified for archetype %s.", archetypes[i]->modulator, archetypes[i]->name);
      mudlog(buf, CH, LOG_SYSLOG, TRUE);
    }
  }
    
  
  // Equip worn items.
  for (int wearloc = 0; wearloc < NUM_WEARS; wearloc++)
    if (archetypes[i]->worn[wearloc] > 0) {
      if ((temp_obj = read_object(archetypes[i]->worn[wearloc], VIRTUAL))) {
        equip_char(CH, temp_obj, wearloc);
      } else {
        snprintf(buf, sizeof(buf), "SYSERR: Invalid worn item %ld specified for archetype %s's wearloc %s (%d).", 
                 archetypes[i]->worn[wearloc], archetypes[i]->name, where[wearloc], wearloc);
        mudlog(buf, CH, LOG_SYSLOG, TRUE);
      }
    }
  
  // Give carried items.
  for (int carried = 0; carried < NUM_ARCHETYPE_CARRIED; carried++)
    if (archetypes[i]->carried[carried] > 0) {
      if ((temp_obj = read_object(archetypes[i]->carried[carried], VIRTUAL))) {
        // Set up carried items, if needed.
        if (GET_OBJ_SPEC(temp_obj) == pocket_sec)
          initialize_pocket_secretary(temp_obj);
          
        if (GET_OBJ_TYPE(temp_obj) == ITEM_PHONE) {
          GET_OBJ_VAL(temp_obj, 2) = 1;
          add_phone_to_list(temp_obj);
        }
          
        obj_to_char(temp_obj, CH);
      } else {
        snprintf(buf, sizeof(buf), "SYSERR: Invalid carried item %ld specified for archetype %s.", 
                 archetypes[i]->carried[carried], archetypes[i]->name);
        mudlog(buf, CH, LOG_SYSLOG, TRUE);
      }
    }
  
  // Give cyberdeck with software installed.
  if (archetypes[i]->cyberdeck > 0) {
    if ((temp_obj = read_object(archetypes[i]->cyberdeck, VIRTUAL))) {
      obj_to_char(temp_obj, CH);
    } else {
      snprintf(buf, sizeof(buf), "SYSERR: Invalid cyberdeck %ld specified for archetype %s.",
               archetypes[i]->cyberdeck, archetypes[i]->name);
      mudlog(buf, CH, LOG_SYSLOG, TRUE);
    }

    for (int j = 0; j < NUM_ARCHETYPE_SOFTWARE; j++) {
      struct obj_data *program;
      if ((program = read_object(archetypes[i]->software[j], VIRTUAL))) {
        // Default the program, but only if it's not bod/sens/mask/evas.
        switch (GET_OBJ_VAL(program, 0)) {
          case SOFT_BOD:
          case SOFT_SENSOR:
          case SOFT_MASKING:
          case SOFT_EVASION:
            break;
          default:
            GET_OBJ_VAL(program, 4)++;
            break;
        }  
        
        // Install it to the deck.
        obj_to_obj(program, temp_obj);
        
        // Take up space on the deck.
        GET_CYBERDECK_USED_STORAGE(temp_obj) += GET_DECK_ACCESSORY_FILE_SIZE(program);
      } else {
        snprintf(buf, sizeof(buf), "SYSERR: Invalid software %ld specified for archetype %s.",
                 archetypes[i]->software[j], archetypes[i]->name);
        mudlog(buf, CH, LOG_SYSLOG, TRUE);
      }
    }
    if (GET_CYBERDECK_FREE_STORAGE(temp_obj) < 300) {
      snprintf(buf, sizeof(buf), "SYSERR: Too many programs on deck for archetype %s-- we have %d storage space left!",
               archetypes[i]->name,
               GET_CYBERDECK_FREE_STORAGE(temp_obj));
      mudlog(buf, CH, LOG_SYSLOG, TRUE);
    }
  }
  
  // Give them a map.
  if ((temp_obj = read_object(OBJ_MAP_OF_SEATTLE, VIRTUAL)))
    obj_to_char(temp_obj, CH);
  else {
    snprintf(buf, sizeof(buf), "SYSERR: Invalid map %d specified for archetype %s.", 
             OBJ_MAP_OF_SEATTLE, archetypes[i]->name);
    mudlog(buf, CH, LOG_SYSLOG, TRUE);
  }
      
  // Set their index and essence. Everyone starts with 0 bioware index and 6.00 essence.
  GET_INDEX(CH) = 0;
  GET_REAL_ESS(CH) = 600;
  
  // Equip cyberware (deduct essence and modify stats as appropriate)
  for (int cyb = 0; cyb < NUM_ARCHETYPE_CYBERWARE; cyb++) {
    if (archetypes[i]->cyberware[cyb]) {
      if (!(temp_obj = read_object(archetypes[i]->cyberware[cyb], VIRTUAL))) {
        snprintf(buf, sizeof(buf), "SYSERR: Invalid cyberware item %ld specified for archetype %s.", 
                 archetypes[i]->cyberware[cyb], archetypes[i]->name);
        mudlog(buf, CH, LOG_SYSLOG, TRUE);
        continue;
      }
      
      int esscost = GET_CYBERWARE_ESSENCE_COST(temp_obj);
      
      if (GET_TRADITION(CH) != TRAD_MUNDANE) {
        if (GET_TOTEM(CH) == TOTEM_EAGLE)
          esscost *= 2;
        magic_loss(CH, esscost, TRUE);
      }
      
      GET_REAL_ESS(CH) -= esscost;
      
      obj_to_cyberware(temp_obj, CH);
    }
  }
  
  // Equip bioware (deduct essence and modify stats as appropriate)
  for (int bio = 0; bio < NUM_ARCHETYPE_BIOWARE; bio++) {
    if (archetypes[i]->bioware[bio]) {
      if (!(temp_obj = read_object(archetypes[i]->bioware[bio], VIRTUAL))) {
        snprintf(buf, sizeof(buf), "SYSERR: Invalid bioware item %ld specified for archetype %s.", 
                 archetypes[i]->bioware[bio], archetypes[i]->name);
        mudlog(buf, CH, LOG_SYSLOG, TRUE);
        continue;
      }
      
      int esscost = GET_OBJ_VAL(temp_obj, 4); 
      
      GET_INDEX(CH) += esscost;
      if (GET_TRADITION(CH) != TRAD_MUNDANE) {
        magic_loss(CH, esscost / 2, TRUE);
      }
      
      GET_HIGHEST_INDEX(CH) = GET_INDEX(CH);
      obj_to_bioware(temp_obj, CH);
    }
  }
  
  /*
     The code below is a mangled version of start_game(); we must CreateChar before setting certain things.
  */
  
  // Sets up the character's idnum, wipes out their skills, etc.
  CreateChar(d->character);
  
  d->character->player.host = str_dup(d->host);

  set_character_skill(d->character, SKILL_ENGLISH, STARTING_LANGUAGE_SKILL_LEVEL, FALSE);
  GET_LANGUAGE(d->character) = SKILL_ENGLISH;
  GET_RESTRING_POINTS(d->character) = STARTING_RESTRING_POINTS;
  GET_LOADROOM(d->character) = archetypes[i]->start_room;

  init_char_sql(d->character);
  GET_CHAR_MULTIPLIER(d->character) = 100;
  snprintf(buf, sizeof(buf), "%s new character (archetypal %s).", GET_CHAR_NAME(d->character), archetypes[i]->name);
  mudlog(buf, d->character, LOG_CONNLOG, TRUE);
  log_vfprintf("[CONNLOG: %s connecting from %s]", GET_CHAR_NAME(d->character), d->host);
  SEND_TO_Q(motd, d);
  SEND_TO_Q("\r\n\n*** PRESS RETURN: ", d);
  STATE(d) = CON_RMOTD;
  
  // Set skills. Has to be after CreateChar so they don't get wiped.
  for (int skill = 0; skill < MAX_SKILLS; skill++)
    if (archetypes[i]->skills[skill])
      set_character_skill(CH, skill, archetypes[i]->skills[skill], FALSE);
      
  // Grant subsidy card (bonded to ID). Has to be after CreateChar so the idnum doesn't change.
  temp_obj = read_object(OBJ_NEOPHYTE_SUBSIDY_CARD, VIRTUAL);
  GET_OBJ_VAL(temp_obj, 0) = GET_IDNUM(CH);
  GET_OBJ_VAL(temp_obj, 1) = archetypes[i]->subsidy_card;
  obj_to_char(temp_obj, CH);
  
  // Bond and equip foci. Has to be after CreateChar so the idnum doesn't change.
  for (int focus = 0; focus < NUM_ARCHETYPE_FOCI; focus++) {
    if (archetypes[i]->foci[focus][0] > 0) {
      if ((temp_obj = read_object(archetypes[i]->foci[focus][0], VIRTUAL))) {
        GET_OBJ_VAL(temp_obj, 2) = GET_IDNUM(CH);
        GET_OBJ_VAL(temp_obj, 3) = (int) archetypes[i]->foci[focus][1];
        GET_OBJ_VAL(temp_obj, 5) = GET_TRADITION(CH) == TRAD_HERMETIC ? 1 : 0;
        GET_OBJ_VAL(temp_obj, 9) = 0;
        obj_to_char(temp_obj, CH);
        
        int wearloc;
        if ((wearloc = find_eq_pos(CH, temp_obj, NULL)) > -1)
          perform_wear(CH, temp_obj, wearloc, FALSE);
        else {
          snprintf(buf, sizeof(buf), "SYSERR: Focus %ld specified for archetype %s's foci slot #%d cannot be worn in default position: No slot available.", 
                   archetypes[i]->foci[focus][0], archetypes[i]->name, focus);
          mudlog(buf, CH, LOG_SYSLOG, TRUE);
        }
      } else {
        snprintf(buf, sizeof(buf), "SYSERR: Invalid focus %ld specified for archetype %s's foci slot #%d.", 
                 archetypes[i]->foci[focus][0], archetypes[i]->name, focus);
        mudlog(buf, CH, LOG_SYSLOG, TRUE);
      }
    }
  }
  
  GET_ARCHETYPAL_MODE(CH) = TRUE;
  GET_ARCHETYPAL_TYPE(CH) = i;

  // Wipe their remaining ccr data.
  init_create_vars(d);
  
  // Set up the character and save them.
  do_start(CH, FALSE);
  playerDB.SaveChar(d->character, archetypes[i]->start_room);
}

#undef ATTACH_IF_EXISTS

void parse_pronouns(struct descriptor_data *d, const char *arg) {
  switch (tolower(*arg)) {
    case 'm':
      d->character->player.sex = SEX_MALE;
      break;
    case 'f':
      d->character->player.sex = SEX_FEMALE;
      break;
    case 'n':
      d->character->player.sex = SEX_NEUTRAL;
      break;
    default:
      SEND_TO_Q("That is not a valid pronoun set.\r\nEnter M for male, F for female, or N for neutral. ", d);
      return;
  }
  ccr_archetype_selection_menu(d);
}

// BELOW THIS IS UNTOUCHED CODE.

const char *assign_menu =
  "\r\nSelect a priority to assign:\r\n"
  "  [1] Attributes\r\n"
  "  [2] Magic\r\n"
  "  [3] Resources\r\n"
  "  [4] Skills\r\n";

int attrib_vals[5] = { 30, 27, 24, 21, 18 };
int skill_vals[5] = { 50, 40, 34, 30, 27 };
int nuyen_vals[5] = { 1000000, 400000, 90000, 20000, 5000 };
int force_vals[5] = { 25, 25, 25, 25, 25 };
int resource_table[2][8] = {{ 500, 5000, 20000, 90000, 200000, 400000, 650000, 1000000 }, { -5, 0, 5, 10, 15, 20, 25, 30 }};
int magic_cost[4] = { 0, 30, 25, 25 };
const char *magic_table[4] = { "None", "Full Magician", "Aspected Magician", "Adept" };

void set_attributes(struct char_data *ch, int magic)
{
  // If the character is a magic user, their magic is equal to their essence (this is free).
  if (magic) {
    GET_REAL_MAG(ch) = 600;
  } else {
    GET_REAL_MAG(ch) = 0;
  }
  
  // Everyone starts with 0 bioware index and 6.00 essence.
  GET_INDEX(ch) = 0;
  GET_REAL_ESS(ch) = 600;
  
  // Set all of the character's stats to their racial minimums (1 + racial modifier, min 1)
  for (int attr = BOD; attr <= WIL; attr++) {
    GET_REAL_ATT(ch, attr) = MAX(1, racial_attribute_modifiers[(int)GET_RACE(ch)][attr] + 1);
  }
  
  // Subtract the cost of making all stats 1 from the character's available attribute-training points.
  int attribute_point_cost = get_minimum_attribute_points_for_race(GET_RACE(ch));
  GET_ATT_POINTS(ch) -= attribute_point_cost;
  send_to_char(ch, "You spend %d attribute points to raise your attributes to their minimums.\r\n", attribute_point_cost);
  
  if (GET_ATT_POINTS(ch) > 1000) {
    snprintf(buf, sizeof(buf), "Somehow, %s managed to get %d attribute points in chargen. Resetting to 0.", GET_CHAR_NAME(ch), GET_ATT_POINTS(ch));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    GET_ATT_POINTS(ch) = 0;
  }

  ch->aff_abils = ch->real_abils;
  
  // Set their natural vision.
  set_natural_vision_for_race(ch);
}

void init_create_vars(struct descriptor_data *d)
{
  int i;

  for (i = 0; i < NUM_CCR_PR_POINTS - 1; i++)
    d->ccr.pr[i] = PR_NONE;
  d->ccr.force_points = 0;
  d->ccr.temp = 0;
}

int parse_race(struct descriptor_data *d, const char *arg)
{
  switch (LOWER(*arg))
  {
  case '1':
    return RACE_HUMAN;
  case '2':
    return RACE_DWARF;
  case '3':
    return RACE_ELF;
  case '4':
    return RACE_ORK;
  case '5':
    return RACE_TROLL;
  case '6':
    return RACE_CYCLOPS;
  case '7':
    return RACE_KOBOROKURU;
  case '8':
    return RACE_FOMORI;
  case '9':
    return RACE_MENEHUNE;
  case 'a':
    return RACE_HOBGOBLIN;
  case 'b':
    return RACE_GIANT;
  case 'c':
    return RACE_GNOME;
  case 'd':
    return RACE_ONI;
  case 'e':
    return RACE_WAKYAMBI;
  case 'f':
    return RACE_OGRE;
  case 'g':
    return RACE_MINOTAUR;
  case 'h':
    return RACE_SATYR;
  case 'i':
    return RACE_NIGHTONE;
  case '?':
    switch (LOWER(*(arg+1))) {
    case '1':
      display_help(buf2, MAX_STRING_LENGTH, "human", d->character);
      break;
    case '2':
      display_help(buf2, MAX_STRING_LENGTH, "dwarf", d->character);
      break;
    case '3':
      display_help(buf2, MAX_STRING_LENGTH, "elf", d->character);
      break;
    case '4':
      display_help(buf2, MAX_STRING_LENGTH, "orks", d->character);
      break;
    case '5':
      display_help(buf2, MAX_STRING_LENGTH, "troll", d->character);
      break;
    case '6':
      display_help(buf2, MAX_STRING_LENGTH, "cyclops", d->character);
      break;
    case '7':
      display_help(buf2, MAX_STRING_LENGTH, "koborokuru", d->character);
      break;
    case '8':
      display_help(buf2, MAX_STRING_LENGTH, "fomori", d->character);
      break;
    case '9':
      display_help(buf2, MAX_STRING_LENGTH, "menehune", d->character);
      break;
    case 'a':
      display_help(buf2, MAX_STRING_LENGTH, "hobgoblin", d->character);
      break;
    case 'b':
      display_help(buf2, MAX_STRING_LENGTH, "giant", d->character);
      break;
    case 'c':
      display_help(buf2, MAX_STRING_LENGTH, "gnome", d->character);
      break;
    case 'd':
      display_help(buf2, MAX_STRING_LENGTH, "oni", d->character);
      break;
    case 'e':
      display_help(buf2, MAX_STRING_LENGTH, "wakyambi", d->character);
      break;
    case 'f':
      display_help(buf2, MAX_STRING_LENGTH, "ogre", d->character);
      break;
    case 'g':
      display_help(buf2, MAX_STRING_LENGTH, "minotaur", d->character);
      break;
    case 'h':
      display_help(buf2, MAX_STRING_LENGTH, "satyr", d->character);
      break;
    case 'i':
      display_help(buf2, MAX_STRING_LENGTH, "night one", d->character);
      break;
    default:
      return RACE_UNDEFINED;
    }
    strlcat(buf2, "\r\n Press [return] to continue", sizeof(buf2));
    SEND_TO_Q(buf2, d);
    d->ccr.temp = CCR_RACE;
    d->ccr.mode = CCR_AWAIT_CR;
    return RETURN_HELP;
  default:
    return RACE_UNDEFINED;
  }
}

bool valid_totem(struct char_data *ch, int i)
{
  if (GET_ASPECT(ch) == ASPECT_SHAMANIST)
    switch (i) {
      case TOTEM_OWL:
      case TOTEM_COYOTE:
        return FALSE;
    }
  return TRUE;
}

int parse_totem(struct descriptor_data *d, const char *arg)
{
  char parsebuf[MAX_STRING_LENGTH];
  strlcpy(parsebuf, arg, sizeof(parsebuf));
  char *temp = parsebuf;
  int i;

  if (*temp == '?')
  {
    i = atoi(++temp);
    display_help(buf2, MAX_STRING_LENGTH, totem_types[i], d->character);
    strlcat(buf2, "\r\n Press [return] to continue ", sizeof(buf2));
    SEND_TO_Q(buf2, d);
    d->ccr.temp = CCR_TOTEM;
    d->ccr.mode = CCR_AWAIT_CR;
    return RETURN_HELP;
  }
  i = atoi(arg);
  if (i < 1 || i >= NUM_TOTEMS)
    i = TOTEM_UNDEFINED;
  return i;
}

int parse_assign(struct descriptor_data *d, const char *arg)
{
  int i;
  
  // Magic is only okay if placed in 0, 1, or l_k_m_s.
  int lowest_kosher_magic_slot = 4;
  if (d->ccr.pr[lowest_kosher_magic_slot] == PR_RACE)
    lowest_kosher_magic_slot--;
  
  if (*arg == '2' && (d->ccr.temp > 1 && d->ccr.temp != lowest_kosher_magic_slot)) {
    char kosher_slot = 'A' + lowest_kosher_magic_slot;
    snprintf(buf2, sizeof(buf2), "Magic can only fit in slots A, B, or %c for %s characters.", 
            kosher_slot, 
            pc_race_types[(int)GET_RACE(d->character)]);
    SEND_TO_Q(buf2, d);
    return 0;
  }

  switch (*arg)
  {
  case '1':
  case '2':
  case '3':
  case '4':
    for (i = 0; i < NUM_CCR_PR_POINTS - 1; i++)
      if (d->ccr.pr[i] == (int)(*arg - '0')) {
        switch (d->ccr.pr[i]) {
        case PR_ATTRIB:
          GET_ATT_POINTS(d->character) = 0;
          break;
        case PR_MAGIC:
          break;
        case PR_RESOURCE:
          GET_NUYEN(d->character) = 0;
          d->ccr.force_points = 0;
          break;
        case PR_SKILL:
          GET_SKILL_POINTS(d->character) = 0;
          break;
        }
        d->ccr.pr[i] = PR_NONE;
      }
    d->ccr.pr[d->ccr.temp] = (int)(*arg - '0');
    switch (d->ccr.pr[d->ccr.temp]) {
    case PR_ATTRIB:
      GET_ATT_POINTS(d->character) = attrib_vals[d->ccr.temp];
      break;
    case PR_MAGIC:
      break;
    case PR_RESOURCE:
      GET_NUYEN(d->character) = nuyen_vals[d->ccr.temp];
      d->ccr.force_points = force_vals[d->ccr.temp];
      break;
    case PR_SKILL:
      GET_SKILL_POINTS(d->character) = skill_vals[d->ccr.temp];
      break;
    }
    d->ccr.temp = 0;
    GET_PP(d->character) = 600;
    return 1;
  case 'c':
    d->ccr.pr[d->ccr.temp] = PR_NONE;
    d->ccr.temp = 0;
    return 1;
  }
  return 0;
}

void priority_menu(struct descriptor_data *d)
{
  int i;

  d->ccr.mode = CCR_PRIORITY;
  SEND_TO_Q("\r\nPriority  Setting     Attributes   Skills Resources\r\n", d);
  for (i = 0; i < NUM_CCR_PR_POINTS - 1; i++)
  {
    snprintf(buf2, sizeof(buf2), "%-10c", 'A' + i);
    switch (d->ccr.pr[i]) {
    case PR_NONE:
      snprintf(buf2, sizeof(buf2), "%s?           %-2d           %-2d        %d nuyen / %d\r\n",
              buf2, attrib_vals[i], skill_vals[i], nuyen_vals[i], force_vals[i]);
      break;
    case PR_RACE:
      if (GET_RACE(d->character) == RACE_ELF)
        strlcat(buf2, "Elf         -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_DRAGON)
        strlcat(buf2, "Dragon     -            -        -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_TROLL)
        strlcat(buf2, "Troll       -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_ORK)
        strlcat(buf2, "Ork         -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_DWARF)
        strlcat(buf2, "Dwarf       -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_CYCLOPS)
        strlcat(buf2, "Cyclops     -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_KOBOROKURU)
        strlcat(buf2, "Koborokuru  -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_FOMORI)
        strlcat(buf2, "Fomori      -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_MENEHUNE)
        strlcat(buf2, "Menehune    -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_HOBGOBLIN)
        strlcat(buf2, "Hobgoblin   -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_GIANT)
        strlcat(buf2, "Giant       -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_GNOME)
        strlcat(buf2, "Gnome       -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_ONI)
        strlcat(buf2, "Oni         -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_WAKYAMBI)
        strlcat(buf2, "Wakyambi    -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_OGRE)
        strlcat(buf2, "Ogre        -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_MINOTAUR)
        strlcat(buf2, "Minotaur    -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_SATYR)
        strlcat(buf2, "Satyr       -            -         -\r\n", sizeof(buf2));
      else if (GET_RACE(d->character) == RACE_NIGHTONE)
        strlcat(buf2, "Night-One   -            -         -\r\n", sizeof(buf2));
      else
        strlcat(buf2, "Human       -            -         -\r\n", sizeof(buf2));
      break;
    case PR_MAGIC:
      if ( i == 0 )
        strlcat(buf2, "Full Mage   -            -         -\r\n", sizeof(buf2));
      else if ( i == 1 )
        strlcat(buf2, "Adept/Aspect-            -         -\r\n", sizeof(buf2));
      else
        strlcat(buf2, "Mundane     -            -         -\r\n", sizeof(buf2));
      break;
    case PR_ATTRIB:
      snprintf(buf2, sizeof(buf2), "%sAttributes  %-2d           -         -\r\n", buf2,
              attrib_vals[i]);
      break;
    case PR_SKILL:
      snprintf(buf2, sizeof(buf2), "%sSkills      -            %-2d        -\r\n", buf2,
              skill_vals[i]);
      break;
    case PR_RESOURCE:
      snprintf(buf2, sizeof(buf2), "%sResources   -            -         %d nuyen / %d\r\n",
              buf2, nuyen_vals[i], force_vals[i]);
      break;
    }
    SEND_TO_Q(buf2, d);
  }
  SEND_TO_Q("\r\nChoose a priority (A-E) to set (? for help, p to continue): ", d);
}

void init_char_sql(struct char_data *ch)
{
  char buf2[MAX_STRING_LENGTH];
  snprintf(buf, sizeof(buf), "INSERT INTO pfiles (idnum, name, password, race, gender, `Rank`, Voice,"\
               "Physical_Keywords, Physical_Name, Whotitle, Height, Weight, Host,"\
               "Tradition, Born, Background, Physical_LookDesc, Matrix_LookDesc, Astral_LookDesc, LastD, multiplier) VALUES ('%ld', '%s', '%s', %d, '%d',"\
               "'%d', '%s', '%s', '%s', '%s', '%d', '%d', '%s', '%d', '%ld', '%s', '%s', '%s', '%s', %ld, 100);", GET_IDNUM(ch),
               GET_CHAR_NAME(ch), GET_PASSWD(ch), GET_RACE(ch), GET_SEX(ch), MAX(1, GET_LEVEL(ch)),
               prepare_quotes(buf2, ch->player.physical_text.room_desc, sizeof(buf2) / sizeof(buf2[0])), GET_KEYWORDS(ch), GET_NAME(ch), GET_WHOTITLE(ch),
               GET_HEIGHT(ch), GET_WEIGHT(ch), ch->player.host, GET_TRADITION(ch), ch->player.time.birth, "A blank slate.",
               "A nondescript person.\r\n", "A nondescript entity.\r\n", "A nondescript entity.\r\n", time(0));
  mysql_wrapper(mysql, buf);
  if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
    snprintf(buf, sizeof(buf), "INSERT INTO pfiles_chargendata (idnum, AttPoints, SkillPoints, ForcePoints, archetypal, archetype) VALUES"\
               "('%ld', '%d', '%d', '%d', '%d', '%d');", GET_IDNUM(ch), GET_ATT_POINTS(ch), GET_SKILL_POINTS(ch), GET_FORCE_POINTS(ch), GET_ARCHETYPAL_MODE(ch), GET_ARCHETYPAL_TYPE(ch));
    mysql_wrapper(mysql, buf);
  }
  if (GET_TRADITION(ch) != TRAD_MUNDANE) {
    snprintf(buf, sizeof(buf), "INSERT INTO pfiles_magic (idnum, Totem, TotemSpirit, Aspect) VALUES"\
               "('%ld', '%d', '%d', '%d');", GET_IDNUM(ch), GET_TOTEM(ch), GET_TOTEMSPIRIT(ch), GET_ASPECT(ch));
    mysql_wrapper(mysql, buf);
  }
  snprintf(buf, sizeof(buf), "INSERT INTO pfiles_drugdata (idnum) VALUES (%ld)", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  if (GET_LEVEL(ch) > 0) {
    snprintf(buf, sizeof(buf), "INSERT INTO pfiles_immortdata (idnum, InvisLevel, IncogLevel, Zonenumber, Poofin, Poofout) VALUES ("\
                 "%ld, %d, %d, %d, '%s', '%s');",
                 GET_IDNUM(ch), GET_INVIS_LEV(ch), GET_INCOG_LEV(ch), ch->player_specials->saved.zonenum,
                (POOFIN(ch) ? POOFIN(ch) : DEFAULT_POOFIN_STRING),
                (POOFOUT(ch) ? POOFOUT(ch) : DEFAULT_POOFOUT_STRING));
    mysql_wrapper(mysql, buf);
  }
}

static void start_game(descriptor_data *d)
{
  CreateChar(d->character);
  d->character->player.host = str_dup(d->host);

  set_character_skill(d->character, SKILL_ENGLISH, STARTING_LANGUAGE_SKILL_LEVEL, FALSE);
  GET_LANGUAGE(d->character) = SKILL_ENGLISH;
  GET_RESTRING_POINTS(d->character) = STARTING_RESTRING_POINTS;
  GET_LOADROOM(d->character) = RM_CHARGEN_START_ROOM;

  init_char_sql(d->character);
  GET_CHAR_MULTIPLIER(d->character) = 100;
  if(PLR_FLAGGED(d->character,PLR_NOT_YET_AUTHED)) {
    snprintf(buf, sizeof(buf), "%s new character.",
            GET_CHAR_NAME(d->character));
    mudlog(buf, d->character, LOG_CONNLOG, TRUE);
    log_vfprintf("[CONNLOG: %s connecting from %s]", GET_CHAR_NAME(d->character), d->host);
    SEND_TO_Q(motd, d);
    SEND_TO_Q("\r\n\n*** PRESS RETURN: ", d);
    STATE(d) = CON_RMOTD;

    init_create_vars(d);
  } else {
    SEND_TO_Q(motd, d);
    SEND_TO_Q("\r\n\n*** PRESS RETURN: ", d);
    STATE(d) = CON_RMOTD;

    init_create_vars(d);

    snprintf(buf, sizeof(buf), "%s new character.",
            GET_CHAR_NAME(d->character));
    mudlog(buf, d->character, LOG_CONNLOG, TRUE);
    log_vfprintf("[CONNLOG: %s connecting from %s]", GET_CHAR_NAME(d->character), d->host);
  }
}

void ccr_totem_menu(struct descriptor_data *d)
{
  snprintf(buf, sizeof(buf), "Select your totem: ");
  for (int i = 1; i < NUM_TOTEMS; i++) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n  %s[%2d] %-20s^n", valid_totem(CH, i) ? "" : "^R", i, totem_types[i]);
    if (++i < NUM_TOTEMS)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s[%2d] %-20s^n", valid_totem(CH, i) ? "" : "^R", i, totem_types[i]);
    if (++i < NUM_TOTEMS)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s[%2d] %-20s^n", valid_totem(CH, i) ? "" : "^R", i, totem_types[i]);
  }
  strlcat(buf, "\r\n", sizeof(buf));
  SEND_TO_Q(buf, d);
  SEND_TO_Q("\r\nTotem (?# for help): ", d);
  d->ccr.mode = CCR_TOTEM;
}

void ccr_totem_spirit_menu(struct descriptor_data *d)
{
  snprintf(buf, sizeof(buf), "Select your totem spirit bonus: \r\n");
  switch (GET_TOTEM(CH))
  {
  case TOTEM_GATOR:
    strlcat(buf, "  [1] Swamp\r\n  [2] Lake\r\n  [3] River\r\n  [4] City\r\n", sizeof(buf));
    break;
  case TOTEM_SNAKE:
  case TOTEM_FOX:
    strlcat(buf, "  [1] City\r\n  [2] Field\r\n  [3] Hearth\r\n  [4] Desert\r\n  [5] Forest\r\n  [6] Mountain\r\n  [7] Prairie\r\n", sizeof(buf));
    break;
  case TOTEM_WOLF:
    strlcat(buf, "  [1] Forest\r\n  [2] Prairie\r\n  [3] Mountain\r\n", sizeof(buf));
    break;
  case TOTEM_FISH:
  case TOTEM_TURTLE:
    strlcat(buf, "  [1] River\r\n  [2] Sea\r\n  [3] Lake\r\n  [4] Swamp\r\n", sizeof(buf));
    break;
  case TOTEM_GECKO:
    strlcat(buf, "  [1] Illusion\r\n  [2] Manipulation\r\n", sizeof(buf));
    break;
  case TOTEM_GOOSE:
    strlcat(buf, "  [ 1] Desert    [ 2] Forest\r\n"
                "  [ 3] Mountain  [ 4] Prairie\r\n"
                "  [ 5] Mist      [ 6] Storm\r\n"
                "  [ 7] River     [ 8] Sea\r\n"
                "  [ 9] Wind      [10] Lake\r\n"
                "  [11] Swamp\r\n", sizeof(buf));
    break;
  case TOTEM_HORSE:
    strlcat(buf, "  [1] Combat\r\n  [2] Illusion\r\n", sizeof(buf));
    break;
  case TOTEM_LIZARD:
    strlcat(buf, "  [1] Desert\r\n  [2] Forest\r\n  [3] Mountain\r\n", sizeof(buf));
    break;
  case TOTEM_OTTER:
    strlcat(buf, "  [1] River\r\n  [2] Sea\r\n", sizeof(buf));
    break;
  }

  SEND_TO_Q(buf, d);
  SEND_TO_Q("\r\nTotem Spirit Bonus: ", d);
  d->ccr.mode = CCR_TOTEM2;
}

void ccr_aspect_menu(struct descriptor_data *d)
{
  strncpy(buf,   "As an aspected mage, you must select your aspect: \r\n"
                 "  [1] Conjurer\r\n"
                 "  [2] Sorcerer\r\n", sizeof(buf) - strlen(buf) - 1);
                 
  if (GET_TRADITION(d->character) == TRAD_SHAMANIC)
    strlcat(buf, "  [3] Shamanist\r\n", sizeof(buf));
  else
    strlcat(buf, "  [3] Elementalist (Earth)\r\n"
                 "  [4] Elementalist (Air)\r\n"
                 "  [5] Elementalist (Fire)\r\n"
                 "  [6] Elementalist (water)\r\n", sizeof(buf));
                 
  strlcat(buf,   "  [?] Help\r\n\r\nAspect: ", sizeof(buf));
  SEND_TO_Q(buf, d);
  d->ccr.mode = CCR_ASPECT;
}

void ccr_type_menu(struct descriptor_data *d)
{
  SEND_TO_Q("\r\nSelect Type of Character Creation:\r\n"
            "  [1] Priority Based (Easy)\r\n"
            "  [2] Points Based   (Advanced)\r\n"
            "Enter creation method: ", d);
  d->ccr.mode = CCR_TYPE;
}

void points_menu(struct descriptor_data *d)
{
  d->ccr.mode = CCR_POINTS;
  snprintf(buf, sizeof(buf), "  1) Attributes: ^c%14d^n (^c%3d^n Points)\r\n"
               "  2) Skills    : ^c%14d^n (^c%3d^n Points)\r\n"
               "  3) Resources : ^c%14d^n (^c%3d^n Points)\r\n"
               "  4) Magic     : ^c%14s^n (^c%3d^n Points)\r\n"
               "     Race      : ^c%14s^n (^c%3d^n Points)\r\n"
               "  Points Remaining: ^c%d^n\r\n"
               "Choose an area to change points on(p to continue): ", d->ccr.pr[PO_ATTR]/2, d->ccr.pr[PO_ATTR],
               d->ccr.pr[PO_SKILL], d->ccr.pr[PO_SKILL], resource_table[0][d->ccr.pr[PO_RESOURCES]], 
               resource_table[1][d->ccr.pr[PO_RESOURCES]], magic_table[d->ccr.pr[PO_MAGIC]], 
               magic_cost[d->ccr.pr[PO_MAGIC]], pc_race_types[(int)GET_RACE(d->character)], d->ccr.pr[PO_RACE], d->ccr.points);
  SEND_TO_Q(buf, d);
}

// Returns the minimum attribute points you must spend for your race.
int get_minimum_attribute_points_for_race(int race) {
  // Initialize the returned value to 6, which is the minimum the player must spend to raise all from 0 to 1.
  int minimum_attribute_points = 6;
  
  // Cycle through all attributes.
  for (int attr = BOD; attr <= WIL; attr++) {
    if (racial_attribute_modifiers[race][attr] < 0) {
      // If the attribute modifier is negative, the player must spend that many points to raise it to 0.
      minimum_attribute_points += -racial_attribute_modifiers[race][attr];
    }
  }
  
  // Return our calculated number.
  return minimum_attribute_points;
}

// Returns the maximum attribute points you can spend on a given race to max them out.
int get_maximum_attribute_points_for_race(int race) {
  int amount = 0;
  for (int attr = BOD; attr <= WIL; attr++) {
    amount += racial_limits[race][0][attr] - (max(1, racial_attribute_modifiers[race][attr] + 1));
  }
  return amount;
}

void create_parse(struct descriptor_data *d, const char *arg)
{
  int i = MIN(120, atoi(arg)), ok;
  int minimum_attribute_points, maximum_attribute_points, available_attribute_points;
  long shirts[5] = { 70000, 70001, 70002, 70003, 70004 };
  long pants[5] = { 70005, 70006, 70007, 70008, 70009 };
  long shoes[5] = { 70010, 70011, 70012, 70013, 70014};

  switch (d->ccr.mode)
  {
  case CCR_PO_ATTR:
      // Calculate the working variables.
      minimum_attribute_points = get_minimum_attribute_points_for_race(GET_RACE(CH));
      maximum_attribute_points = get_maximum_attribute_points_for_race(GET_RACE(CH)) + minimum_attribute_points;
      available_attribute_points = MIN((int) (d->ccr.points / 2), get_maximum_attribute_points_for_race(GET_RACE(CH)));
      
    if (i < minimum_attribute_points) {
      send_to_char(CH, "The minimum of attribute points for your race is %d.\r\n", minimum_attribute_points);
      i = minimum_attribute_points;
    }
    
    else if (i > maximum_attribute_points) {
      send_to_char(CH, "The maximum number of attribute points for your race is %d.\r\n", maximum_attribute_points);
      i = maximum_attribute_points;
    }
    
    if (i * 2 > d->ccr.points) {
      send_to_char(CH, "You do not have enough points for that.\r\n"
                   "Enter desired number of attribute points (^c%d^n available, minimum %d, maximum %d):",
                   available_attribute_points, minimum_attribute_points, maximum_attribute_points);
      break;
    }
  
    d->ccr.points -= d->ccr.pr[PO_ATTR] = i * 2;
    points_menu(d);
    break;
  case CCR_PO_SKILL:
    if (i < 0)
      send_to_char(CH, "You cannot have less than 0 points of skill. Enter desired number of skill points (^c%d^n available):",d->ccr.points);
    else if (i > d->ccr.points)
      send_to_char(CH, "You do not have enough points for that. Enter desired number of skill points (^c%d^n available):", d->ccr.points);
    else {
      d->ccr.points -= d->ccr.pr[PO_SKILL] = i;
      points_menu(d);
    }
    break;
  case CCR_PO_RESOURCES:
    i--;
    if (i > 7 || i < 0)
      send_to_char(CH, "Invalid number. Enter desired amount of nuyen (^c%d^n available): ", d->ccr.points);
    else if (resource_table[1][i] > d->ccr.points)
      send_to_char(CH, "You do not have enough points for that. Enter desired amount of nuyen (^c%d^n available):", d->ccr.points);
    else {
      d->ccr.pr[PO_RESOURCES] = i;
      d->ccr.points -= resource_table[1][d->ccr.pr[PO_RESOURCES]];
      points_menu(d);
    }
    break;
  case CCR_PO_MAGIC:
    i--;
    if (i > 3 || i < 0)
      send_to_char(CH, "Invalid number. Enter desired type of magic (^c%d^n points available): ", d->ccr.points);
    else if (magic_cost[i] > d->ccr.points)
      send_to_char(CH, "You do not have enough points for that. Enter desired type of magic (^c%d^n points available):", d->ccr.points);
    else {
      d->ccr.pr[PO_MAGIC] = i;
      d->ccr.points -= magic_cost[d->ccr.pr[PO_MAGIC]];
      points_menu(d);
    }
    break;
  case CCR_POINTS:
    switch (*arg) {
      case '1':
        // Unset their previous attribute investment.
        d->ccr.points += d->ccr.pr[PO_ATTR];
        
        // Calculate their available stat points and minimum/maximums.
        minimum_attribute_points = get_minimum_attribute_points_for_race(GET_RACE(CH));
        maximum_attribute_points = get_maximum_attribute_points_for_race(GET_RACE(CH)) + minimum_attribute_points;
        available_attribute_points = (int) (d->ccr.points / 2);
        
        send_to_char(CH, "Enter desired number of attribute points (^c%d^n available, minimum %d, maximum %d): ",
                     available_attribute_points, minimum_attribute_points, maximum_attribute_points);
        d->ccr.mode = CCR_PO_ATTR;
        break;
      case '2':
        d->ccr.points += d->ccr.pr[PO_SKILL];
        send_to_char(CH, "Enter desired number of skill points (^c%d^n available): ", d->ccr.points);
        d->ccr.mode = CCR_PO_SKILL;
        break;
      case '3':
        d->ccr.points += resource_table[1][d->ccr.pr[PO_RESOURCES]];
        snprintf(buf, sizeof(buf), " ");
        for (int x = 0; x < 8; x++)
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %d) %8d nuyen   (%2d points)\r\n ", x+1, resource_table[0][x], resource_table[1][x]);
        SEND_TO_Q(buf, d);
        send_to_char(CH, "Enter desired amount of nuyen points (^c%d^n available): ", d->ccr.points);
        d->ccr.mode = CCR_PO_RESOURCES;
        break;
      case '4':
        d->ccr.points += magic_cost[d->ccr.pr[PO_MAGIC]];
        snprintf(buf, sizeof(buf), " ");
        for (int x = 0; x < 4; x++)
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %d) %18s (%2d points)\r\n ", x+1, magic_table[x], magic_cost[x]);
        SEND_TO_Q(buf, d);
        send_to_char(CH, "Enter desired type of magic (^c%d^n points available): ", d->ccr.points);
        d->ccr.mode = CCR_PO_MAGIC;
        break;
      case 'p':
      case 'P':
        // Safeguard: You can't continue without spending all your points.
        if (d->ccr.points > 0) {
          send_to_char("You still have points remaining to allocate! You should spend these before finishing creation.\r\n\r\n", CH);
          points_menu(d);
          break;
        }
        
        // Defense: You can't continue if you have negative points.
        if (d->ccr.points < 0) {
          send_to_char("You cannot finish creation with a negative point balance. Please lower one of your selections first.\r\n\r\n", CH);
          snprintf(buf, sizeof(buf), "SYSERR: Character %s attained negative point balance %d during creation.", GET_NAME(CH), d->ccr.points);
          mudlog(buf, NULL, LOG_SYSLOG, TRUE);
          points_menu(d);
          break;
        }
        
        GET_NUYEN(CH) = resource_table[0][d->ccr.pr[PO_RESOURCES]];
        GET_SKILL_POINTS(CH) = d->ccr.pr[PO_SKILL];
        GET_ATT_POINTS(CH) = d->ccr.pr[PO_ATTR]/2;
        if (d->ccr.pr[PO_MAGIC] > 0) {
          set_attributes(CH, 1);
          if (GET_RACE(CH) == RACE_GNOME) {
            GET_TRADITION(CH) = TRAD_SHAMANIC;
            if (d->ccr.pr[PO_MAGIC] == 1) {
              GET_FORCE_POINTS(CH) = 25;
              ccr_totem_menu(d);
            } else {
              GET_FORCE_POINTS(CH) = 35;
              ccr_aspect_menu(d);
             }
          } else if (d->ccr.pr[PO_MAGIC] == 3) {
            GET_TRADITION(CH) = TRAD_ADEPT;
            GET_PP(CH) = 600;
            start_game(d);
          } else {
            d->ccr.mode = CCR_TRADITION;
            SEND_TO_Q("\r\nFollow [h]ermetic or [s]hamanic magical tradition? (enter ? for help) ", d);
          }
        } else {
          GET_TRADITION(CH) = TRAD_MUNDANE;
          set_attributes(CH, 0);
          start_game(d);
        }
      break;
    }
    break;
  case CCR_TYPE:
    switch (*arg) {
      case '1':
        priority_menu(d);
        d->ccr.mode = CCR_PRIORITY;
        break;
      case '2':
        d->ccr.points = 120;
        
        // Assign racial minimums and subtract them from the point value.
        d->ccr.pr[PO_ATTR] = get_minimum_attribute_points_for_race(GET_RACE(CH)) * 2;
        d->ccr.points -= d->ccr.pr[PO_ATTR];
        
        d->ccr.pr[PO_SKILL] = 0;
        d->ccr.pr[PO_RESOURCES] = 1;
        d->ccr.pr[PO_MAGIC] = 0;
        
        // Assign racial costs and subtract them from the point value.
        switch (GET_RACE(CH)) {
          case RACE_HUMAN:
            d->ccr.pr[PO_RACE] = 0;
            break;
          case RACE_DWARF:
          case RACE_ORK:
            d->ccr.pr[PO_RACE] = 5;
            break;
          case RACE_OGRE:
          case RACE_HOBGOBLIN:
          case RACE_GNOME:
          case RACE_ONI:
          case RACE_SATYR:
          case RACE_KOBOROKURU:
          case RACE_TROLL:
          case RACE_ELF:
          case RACE_MENEHUNE:
            d->ccr.pr[PO_RACE] = 10;
            break;
          case RACE_CYCLOPS:
          case RACE_FOMORI:
          case RACE_GIANT:
          case RACE_MINOTAUR:
          case RACE_NIGHTONE:
          case RACE_WAKYAMBI:
            d->ccr.pr[PO_RACE] = 15;
            break;
        }
        d->ccr.points -= d->ccr.pr[PO_RACE];
        d->ccr.pr[5] = -1;
        
        points_menu(d);
        break;
      default:
        ccr_type_menu(d);
        break;
    }
    break;
  case CCR_AWAIT_CR:
    d->ccr.mode = d->ccr.temp;
    d->ccr.temp = 0;
    switch (d->ccr.mode) {
    case CCR_RACE:
      ccr_race_menu(d);
      break;
    case CCR_PRIORITY:
      priority_menu(d);
      break;
    case CCR_TOTEM:
      ccr_totem_menu(d);
      break;
    default:
      log("Invalid mode in AWAIT_CR in chargen.");
    }
    break;
  case CCR_PRONOUNS:
    parse_pronouns(d, arg);
    break;
  case CCR_ARCHETYPE_MODE:
    ccr_confirm_switch_to_custom_parse(d, arg);
    break;
  case CCR_ARCHETYPE_SELECTION_MODE:
    archetype_selection_parse(d, arg);
    break;
  case CCR_RACE:
    if ((GET_RACE(d->character) = parse_race(d, arg)) == RACE_UNDEFINED) {
      SEND_TO_Q("\r\nThat's not a race.\r\nRace: ", d);
      return;
    } else if (GET_RACE(d->character) == RETURN_HELP) // for when they use help
      return;

    if (GET_RACE(d->character) == RACE_DRAGON)
      d->ccr.pr[0] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_HUMAN)
      d->ccr.pr[4] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_DWARF)
      d->ccr.pr[3] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_ORK)
      d->ccr.pr[3] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_TROLL)
      d->ccr.pr[2] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_ELF)
      d->ccr.pr[2] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_CYCLOPS)
      d->ccr.pr[2] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_KOBOROKURU)
      d->ccr.pr[2] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_FOMORI)
      d->ccr.pr[2] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_MENEHUNE)
      d->ccr.pr[2] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_HOBGOBLIN)
      d->ccr.pr[2] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_GIANT)
      d->ccr.pr[2] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_GNOME)
      d->ccr.pr[2] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_ONI)
      d->ccr.pr[2] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_WAKYAMBI)
      d->ccr.pr[2] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_OGRE)
      d->ccr.pr[2] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_MINOTAUR)
      d->ccr.pr[2] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_SATYR)
      d->ccr.pr[2] = PR_RACE;
    else if (GET_RACE(d->character) == RACE_NIGHTONE)
      d->ccr.pr[2] = PR_RACE;
    
    if (real_object(OBJ_MAP_OF_SEATTLE) > -1)
      obj_to_char(read_object(OBJ_MAP_OF_SEATTLE, VIRTUAL), d->character);
    GET_EQ(d->character, WEAR_BODY) = read_object(shirts[number(0, 4)], VIRTUAL);
    GET_EQ(d->character, WEAR_LEGS) = read_object(pants[number(0, 4)], VIRTUAL);
    GET_EQ(d->character, WEAR_FEET) = read_object(shoes[number(0, 4)], VIRTUAL);
    ccr_type_menu(d);
    break;
  case CCR_TOTEM:
    if ((i = parse_totem(d, arg)) == TOTEM_UNDEFINED) {
      SEND_TO_Q("\r\nThat's not a totem.\r\nTotem (?# for help): ", d);
      return;
    } else if (!valid_totem(CH, i)) {
      SEND_TO_Q("\r\nShamanists cannot pick that totem.\r\nTotem: ", d);
      return;
    } else if (i == RETURN_HELP) // for when they use help
      return;
    GET_TOTEM(d->character) = (ubyte) i;
    switch (GET_TOTEM(CH)) {
      case TOTEM_BULL:
      case TOTEM_PRAIRIEDOG:
        i = GET_REAL_CHA(CH);
        if (i < 4) {
          if (GET_ATT_POINTS(CH) < 4 - i) {
            SEND_TO_Q("\r\nYou don't have enough attribute points available to pick that totem.\r\nTotem: ", d);
            return;
          }
          GET_REAL_CHA(CH) = 4;
          GET_ATT_POINTS(CH) -= 4 - i;
        }
        break;
		      case TOTEM_SUN:
        i = GET_REAL_CHA(CH);
        if (i < 4) {
          if (GET_ATT_POINTS(CH) < 4 - i) {
            SEND_TO_Q("\r\nYou don't have enough attribute points available to pick that totem.\r\nTotem: ", d);
            return;
          }
          GET_REAL_CHA(CH) = 4;
          GET_ATT_POINTS(CH) -= 4 - i;
        }
        break;
      case TOTEM_LOVER:
        i = GET_REAL_CHA(CH);
        if (i < 6) {
          if (GET_ATT_POINTS(CH) < 6 - i) {
            SEND_TO_Q("\r\nYou don't have enough attribute points available to pick that totem.\r\nTotem: ", d);
            return;
          }
          GET_REAL_CHA(CH) = 6;
          GET_ATT_POINTS(CH) -= 6 - i;
        }
        break;
      case TOTEM_SEDUCTRESS:
        i = GET_REAL_CHA(CH);
        if (i < 6) {
          if (GET_ATT_POINTS(CH) < 6 - i) {
            SEND_TO_Q("\r\nYou don't have enough attribute points available to pick that totem.\r\nTotem: ", d);
            return;
          }
          GET_REAL_CHA(CH) = 6;
          GET_ATT_POINTS(CH) -= 6 - i;
        }
        break;
      case TOTEM_SIREN:
        i = GET_REAL_CHA(CH);
        if (i < 6) {
          if (GET_ATT_POINTS(CH) < 6 - i) {
            SEND_TO_Q("\r\nYou don't have enough attribute points available to pick that totem.\r\nTotem: ", d);
            return;
          }
          GET_REAL_CHA(CH) = 6;
          GET_ATT_POINTS(CH) -= 6 - i;
        }
        break;
      case TOTEM_OAK:
        if (GET_REAL_BOD(CH) < 4 || GET_REAL_STR(CH) < 4) {  // 4 bod, 4 str
          i = GET_REAL_BOD(CH) + GET_REAL_STR(CH);
          if (GET_ATT_POINTS(CH) < 8 - i) {
            SEND_TO_Q("\r\nYou don't have enough attribute points available to pick that totem.\r\nTotem: ", d);
            return;
          }
          GET_REAL_BOD(CH) = 4;
          GET_REAL_STR(CH) = 4;
          GET_ATT_POINTS(CH) -= 8 - i;
        }
        break;
      case TOTEM_CHEETAH:
        if (GET_REAL_QUI(CH) < 4 || GET_REAL_INT(CH) < 4) {
          if (GET_ATT_POINTS(CH) < 8 - (GET_REAL_QUI(CH) + GET_REAL_INT(CH))) {
            SEND_TO_Q("\r\nYou don't have enough attribute points available to pick that totem.\r\nTotem: ", d);
            return;
          }
        }
        i = GET_REAL_QUI(CH);
        if (i < 4) {
          GET_REAL_QUI(CH) = 4;
          GET_ATT_POINTS(CH) -= 4 - i;
        }
        i = GET_REAL_INT(CH);
        if (i < 4) {
          GET_REAL_INT(CH) = 4;
          GET_ATT_POINTS(CH) -= 4 - i;
        }
        break;
    }
    if (GET_TOTEM(CH) == TOTEM_WOLF || GET_TOTEM(CH) == TOTEM_SNAKE || GET_TOTEM(CH) == TOTEM_GATOR || GET_TOTEM(CH) == TOTEM_FOX ||
        GET_TOTEM(CH) == TOTEM_FISH || GET_TOTEM(CH) == TOTEM_TURTLE || GET_TOTEM(CH) == TOTEM_GECKO ||
        GET_TOTEM(CH) == TOTEM_GOOSE || GET_TOTEM(CH) == TOTEM_HORSE || GET_TOTEM(CH) == TOTEM_LIZARD || GET_TOTEM(CH) == TOTEM_OTTER)
      ccr_totem_spirit_menu(d);
    else
      start_game(d);
    break;
  case CCR_TOTEM2:
    switch (GET_TOTEM(CH)) {
    case TOTEM_GATOR:
      if (i < 1 || i > 4) {
        SEND_TO_Q("Invalid spirit bonus:\r\nTotem Spirit Bonus: ", d);
        return;
      }
      switch (i) {
      case 1:
        GET_TOTEMSPIRIT(CH) = SPIRIT_SWAMP;
        break;
      case 2:
        GET_TOTEMSPIRIT(CH) = SPIRIT_LAKE;
        break;
      case 3:
        GET_TOTEMSPIRIT(CH) = SPIRIT_RIVER;
        break;
      case 4:
        GET_TOTEMSPIRIT(CH) = SPIRIT_CITY;
        break;
      }
      break;
    case TOTEM_SNAKE:
    case TOTEM_FOX:
      if (i < 1 || i > 7) {
        SEND_TO_Q("Invalid spirit bonus:\r\nTotem Spirit Bonus: ", d);
        return;
      }
      switch (i) {
      case 1:
        GET_TOTEMSPIRIT(CH) = SPIRIT_CITY;
        break;
      case 2:
        GET_TOTEMSPIRIT(CH) = SPIRIT_FIELD;
        break;
      case 3:
        GET_TOTEMSPIRIT(CH) = SPIRIT_HEARTH;
        break;
      case 4:
        GET_TOTEMSPIRIT(CH) = SPIRIT_DESERT;
        break;
      case 5:
        GET_TOTEMSPIRIT(CH) = SPIRIT_FOREST;
        break;
      case 6:
        GET_TOTEMSPIRIT(CH) = SPIRIT_MOUNTAIN;
        break;
      case 7:
        GET_TOTEMSPIRIT(CH) = SPIRIT_PRAIRIE;
        break;
      }
      break;
    case TOTEM_WOLF:
      if (i < 1 || i > 3) {
        SEND_TO_Q("Invalid spirit bonus:\r\nTotem Spirit Bonus: ", d);
        return;
      }
      switch (i) {
      case 1:
        GET_TOTEMSPIRIT(CH) = SPIRIT_FOREST;
        break;
      case 2:
        GET_TOTEMSPIRIT(CH) = SPIRIT_PRAIRIE;
        break;
      case 3:
        GET_TOTEMSPIRIT(CH) = SPIRIT_MOUNTAIN;
        break;
      }
      break;
    case TOTEM_FISH:
    case TOTEM_TURTLE:
      if (i < 1 || i > 4) {
        SEND_TO_Q("Invalid spirit bonus:\r\nTotem Spirit Bonus: ", d);
        return;
      }
      switch (i) {
      case 1:
        GET_TOTEMSPIRIT(CH) = SPIRIT_RIVER;
        break;
      case 2:
        GET_TOTEMSPIRIT(CH) = SPIRIT_SEA;
        break;
      case 3:
        GET_TOTEMSPIRIT(CH) = SPIRIT_LAKE;
        break;
      case 4:
        GET_TOTEMSPIRIT(CH) = SPIRIT_SWAMP;
        break;
      }
      break;
    case TOTEM_GECKO:
      if (i < 1 || i > 2) {
        SEND_TO_Q("Invalid totem advantage:\r\nTotem Advantage: ", d);
        return;
      }
      switch (i) {
      case 1:
        GET_TOTEMSPIRIT(CH) = ILLUSION;
        break;
      case 2:
        GET_TOTEMSPIRIT(CH) = MANIPULATION;
        break;
      }
      break;
    case TOTEM_GOOSE:
      if (i < 1 || i > 11) {
        SEND_TO_Q("Invalid spirit bonus:\r\nTotem Spirit Bonus: ", d);
        return;
      }
      switch (i) {
      case 1:
        GET_TOTEMSPIRIT(CH) = SPIRIT_DESERT;
        break;
      case 2:
        GET_TOTEMSPIRIT(CH) = SPIRIT_FOREST;
        break;
      case 3:
        GET_TOTEMSPIRIT(CH) = SPIRIT_MOUNTAIN;
        break;
      case 4:
        GET_TOTEMSPIRIT(CH) = SPIRIT_PRAIRIE;
        break;
      case 5:
        GET_TOTEMSPIRIT(CH) = SPIRIT_MIST;
        break;
      case 6:
        GET_TOTEMSPIRIT(CH) = SPIRIT_STORM;
        break;
      case 7:
        GET_TOTEMSPIRIT(CH) = SPIRIT_RIVER;
        break;
      case 8:
        GET_TOTEMSPIRIT(CH) = SPIRIT_SEA;
        break;
      case 9:
        GET_TOTEMSPIRIT(CH) = SPIRIT_WIND;
        break;
      case 10:
        GET_TOTEMSPIRIT(CH) = SPIRIT_LAKE;
        break;
      case 11:
        GET_TOTEMSPIRIT(CH) = SPIRIT_SWAMP;
        break;

      }
    break;
  case TOTEM_HORSE:
      if (i < 1 || i > 2) {
        SEND_TO_Q("Invalid totem disadvantage:\r\nTotem Disadvantage: ", d);
        return;
      }
      switch (i) {
      case 1:
        GET_TOTEMSPIRIT(CH) = COMBAT;
        break;
      case 2:
        GET_TOTEMSPIRIT(CH) = ILLUSION;
        break;
      }
      break;
    case TOTEM_LIZARD:
      if (i < 1 || i > 3) {
        SEND_TO_Q("Invalid spirit bonus:\r\nTotem Spirit Bonus: ", d);
        return;
      }
      switch (i) {
      case 1:
        GET_TOTEMSPIRIT(CH) = SPIRIT_DESERT;
        break;
      case 2:
        GET_TOTEMSPIRIT(CH) = SPIRIT_FOREST;
        break;
      case 3:
        GET_TOTEMSPIRIT(CH) = SPIRIT_MOUNTAIN;
        break;
      }
      break;
    case TOTEM_OTTER:
      if (i < 1 || i > 2) {
        SEND_TO_Q("Invalid spirit bonus:\r\nTotem Spirit Bonus: ", d);
        return;
      }
      switch (i) {
      case 1:
        GET_TOTEMSPIRIT(CH) = SPIRIT_RIVER;
        break;
      case 2:
        GET_TOTEMSPIRIT(CH) = SPIRIT_SEA;
        break;
      }
      break;
    }
    start_game(d);
    break;
  case CCR_PRIORITY:
    switch (LOWER(*arg)) {
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
      if (d->ccr.pr[(int)(LOWER(*arg)-'a')] == PR_RACE)
        priority_menu(d);
      else {
        d->ccr.temp = (int)(LOWER(*arg)-'a');
        SEND_TO_Q(assign_menu, d);
        SEND_TO_Q("\r\nPriority to assign (c to clear): ", d);
        d->ccr.mode = CCR_ASSIGN;
      }
      break;
    case 'p':
      ok = TRUE;
      for (i = 0; ok && i < 5; i++)
        ok = (d->ccr.pr[i] != PR_NONE);
      if (ok) {
        if (d->ccr.pr[0] == PR_MAGIC || d->ccr.pr[1] == PR_MAGIC)
          set_attributes(d->character, 1);
        else
          set_attributes(d->character, 0);
        if (d->ccr.pr[0] == PR_MAGIC || d->ccr.pr[1] == PR_MAGIC) {
          if (GET_RACE(d->character) == RACE_GNOME) {
            GET_TRADITION(d->character) = TRAD_SHAMANIC;
            if (d->ccr.pr[1] == PR_MAGIC) {
              GET_FORCE_POINTS(CH) = 35;
              ccr_aspect_menu(d);
            } else {
              GET_FORCE_POINTS(CH) = 25;
              ccr_totem_menu(d);
            }
          } else {
            d->ccr.mode = CCR_TRADITION;
            SEND_TO_Q("\r\nFollow [h]ermetic, [s]hamanic or som[a]tic (Adept) magical tradition?  Enter '?' for help. ", d);
          }
            return;
        } else
          GET_TRADITION(d->character) = TRAD_MUNDANE;
        start_game(d);
      } else
        priority_menu(d);
      break;
    case '?':
      display_help(buf2, MAX_STRING_LENGTH, "priorities", d->character);
      strlcat(buf2, "\r\n Press [return] to continue ", sizeof(buf2));
      SEND_TO_Q(buf2, d);
      d->ccr.temp = CCR_PRIORITY;
      d->ccr.mode = CCR_AWAIT_CR;
      break;
    default:
      priority_menu(d);
    }
    break;
  case CCR_ASSIGN:
    if (!(i = parse_assign(d, arg)))
      SEND_TO_Q("\r\nThat's not a valid priority!\r\nPriority to assign (c to clear): ", d);
    else
      priority_menu(d);
    break;

  case CCR_TRADITION:
  #define TRADITION_HELP_STRING "\r\nHermetic mages focus on magic as a science. Their conjured elementals are powerful, but expensive to bring forth." \
            "\r\n" \
            "\r\nShamanic mages focus on magic as a part of nature. Their conjured spirits are weaker than elementals, but free to conjure." \
            "\r\n" \
            "\r\nSomatic Adepts focus on magic as a way to improve the physical form. They cannot cast spells or conjure elementals, but their bodies are strengthened by their magic." \
            "\r\n" \
            "\r\nEnter H to select Hermetic, S to select Shamanic, or A to select Somatic Adept: "
    if (isalpha(*arg) && isalpha(*(arg+1))) {
      snprintf(buf, sizeof(buf), "\r\nARG: '%s'\r\n", arg);
      SEND_TO_Q(buf, d);
      SEND_TO_Q(TRADITION_HELP_STRING, d);
    } else {
      switch (LOWER(*arg)) {
      case 'h':
        GET_TRADITION(d->character) = TRAD_HERMETIC;
        if ((d->ccr.pr[5] = -1 && d->ccr.pr[PO_MAGIC] == 2) || (d->ccr.pr[5] != -1 && d->ccr.pr[1] == PR_MAGIC)) {
          GET_FORCE_POINTS(CH) = 35;
          ccr_aspect_menu(d);
        } else {
          GET_FORCE_POINTS(CH) = 25;
          start_game(d);
        }
        break;
      case 's':
        GET_TRADITION(d->character) = TRAD_SHAMANIC;
        if ((d->ccr.pr[5] = -1 && d->ccr.pr[PO_MAGIC] == 2) || (d->ccr.pr[5] != -1 && d->ccr.pr[1] == PR_MAGIC)) {
          GET_FORCE_POINTS(CH) = 35;
          ccr_aspect_menu(d);
        } else {
          GET_FORCE_POINTS(CH) = 25;
          ccr_totem_menu(d);
        }
        break;
      case 'a':
        GET_TRADITION(d->character) = TRAD_ADEPT;
        start_game(d);
        break;
      default:
        SEND_TO_Q(TRADITION_HELP_STRING, d);
        break;
      }
    }
    break;
  case CCR_ASPECT:
    if (GET_TRADITION(d->character) == TRAD_SHAMANIC) {
      switch (i) {
      case 1:
        GET_ASPECT(d->character) = ASPECT_CONJURER;
        break;
      case 2:
        GET_ASPECT(d->character) = ASPECT_SORCERER;
        break;
      case 3:
        GET_ASPECT(d->character) = ASPECT_SHAMANIST;
        break;
      default:
        SEND_TO_Q("\r\nConjurers focus on conjuring spirits, but cannot cast spells. Sorcerers are the opposite of this. Shamanists are able to cast spells and conjure spirits, but are limited by their totem in the spells and spirits they can cast.\r\n\r\nSelect your aspect (1 for Conjurer, 2 for Sorcerer, 3 for Shamanist): ", d);
        return;
      }
      ccr_totem_menu(d);
      return;
    } else
      switch (i) {
      case 1:
        GET_ASPECT(d->character) = ASPECT_CONJURER;
        break;
      case 2:
        GET_ASPECT(d->character) = ASPECT_SORCERER;
        break;
      case 3:
        GET_ASPECT(d->character) = ASPECT_ELEMEARTH;
        break;
      case 4:
        GET_ASPECT(d->character) = ASPECT_ELEMAIR;
        break;
      case 5:
        GET_ASPECT(d->character) = ASPECT_ELEMFIRE;
        break;
      case 6:
        GET_ASPECT(d->character) = ASPECT_ELEMWATER;
        break;
      default:
        SEND_TO_Q("\r\nConjurers focus on conjuring spirits, but cannot cast spells. Sorcerers are the opposite of this. Elemental aspects are restricted to working with elementals of that flavor and certain classes of spells (Earth is Manipulation, Air is Detection, Fire is Combat, Water is Illusion).\r\n\r\nSelect your aspect (1 for Conjurer, 2 for Sorcerer, 3-6 for Earth, Air, Fire, Water.) ", d);
        return;
      }
    start_game(d);
    break;
  }
}
