#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <mysql/mysql.h>
#include "structs.hpp"
#include "db.hpp"
#include "dblist.hpp"
#include "newdb.hpp"
#include "utils.hpp"
#include "interpreter.hpp"
#include "comm.hpp"
#include "handler.hpp"
#include "newmagic.hpp"
#include "awake.hpp"
#include "constants.hpp"
#include "config.hpp"
#include "chargen.hpp"
#include "archetypes.hpp"
#include "bullet_pants.hpp"
#include "security.hpp"
#include "metrics.hpp"

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

static void start_game(descriptor_data *d, const char *origin);

int get_minimum_attribute_points_for_race(int race);
void init_char_sql(struct char_data *ch, const char *origin);
void init_create_vars(struct descriptor_data *d);

ACMD_DECLARE(do_help);

void echo_on(struct descriptor_data * d);
void echo_off(struct descriptor_data * d);

int get_ccr_race_point_cost(int race_int);

#define PRIORITY_E 4
#define PRIORITY_D 3
#define PRIORITY_C 2
#define PRIORITY_B 1
#define PRIORITY_A 0

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

// Invoked from interpreter, this menu puts the character in a state to select their pronouns.
void ccr_pronoun_menu(struct descriptor_data *d) {
  SEND_TO_Q("What pronouns will your character use? (M)ale, (F)emale, (N)eutral: ", d);
  d->ccr.mode = CCR_PRONOUNS;
}

void display_prestige_race_menu(struct descriptor_data *d) {
  char msg_buf[10000];
  snprintf(msg_buf, sizeof(msg_buf), 
            "\r\nPrestige races and classes cost system points, which you will draw from an existing character."
            "\r\n"
            "\r\nThe following races are available:"
            "\r\n"
            "\r\n--- Dryads are elves with a +1 TN allergy to cities. ---"
            "\r\n 1) Dryad            (%4d sysp, 15 build points / slot B)",
            PRESTIGE_RACE_DRYAD_COST);

  snprintf(ENDOF(msg_buf), sizeof(msg_buf) - strlen(msg_buf), 
            "\r\n"
            "\r\n^c--- Ghouls take SIGNIFICANT penalties! Choose for RP flavor only. ---^n"
            "\r\n 2) Ghoul [Human]    (%4d sysp, 10 build points / slot C)"
            "\r\n 3) Ghoul [Dwarf]    (%4d sysp, 15 build points / slot B)"
            "\r\n 4) Ghoul [Elf]      (%4d sysp, 20 build points / slot B)"
            "\r\n 5) Ghoul [Ork]      (%4d sysp, 15 build points / slot B)"
            "\r\n 6) Ghoul [Troll]    (%4d sysp, 20 build points / slot B)",
            PRESTIGE_RACE_GHOUL_COST, PRESTIGE_RACE_GHOUL_COST, PRESTIGE_RACE_GHOUL_COST, PRESTIGE_RACE_GHOUL_COST, PRESTIGE_RACE_GHOUL_COST);

  snprintf(ENDOF(msg_buf), sizeof(msg_buf) - strlen(msg_buf), 
            "\r\n"
            "\r\n^c--- Drakes take SIGNIFICANT penalties! Choose for RP flavor only. ---^n"
            "\r\n 7) Drake [Human]    (%4d sysp, 15 build points / slot B)"
            "\r\n 8) Drake [Dwarf]    (%4d sysp, 20 build points / slot B)"
            "\r\n 9) Drake [Elf]      (%4d sysp, 25 build points / slot B)"
            "\r\n 0) Drake [Ork]      (%4d sysp, 20 build points / slot B)"
            "\r\n A) Drake [Troll]    (%4d sysp, 25 build points / slot B)",
            PRESTIGE_RACE_DRAKE_COST, PRESTIGE_RACE_DRAKE_COST, PRESTIGE_RACE_DRAKE_COST, PRESTIGE_RACE_DRAKE_COST, PRESTIGE_RACE_DRAKE_COST);

  snprintf(ENDOF(msg_buf), sizeof(msg_buf) - strlen(msg_buf), 
            "\r\n"
            "\r\n^c--- Dragons can't use 'ware, but have high stat caps. ---^n"
            "\r\n B) Western Dragon    (%4d sysp, 30 build points / slot B)"
            "\r\n C) Eastern Dragon    (%4d sysp, 30 build points / slot B)"
            "\r\n D) Feathered Serpent (%4d sysp, 30 build points / slot B)"
            "\r\n"
            "\r\nThe following classes are available (Toggle only one):"
            "\r\n"
            "\r\n--- Otaku are a magic variant of Deckers. ---"
            "\r\n O) Otaku [%s]        (%4d sys, 30 build points / slot A)",
            PRESTIGE_RACE_DRAGON_COST, PRESTIGE_RACE_DRAGON_COST, PRESTIGE_RACE_DRAGON_COST,
            d->ccr.is_otaku ? "^g ON^n" : "^rOFF^n", PRESTIGE_CLASS_OTAKU_COST);

  strlcat(msg_buf, 
            "\r\n"
            "\r\nSelect a prestige race, or X to go back to standard races: ", sizeof(msg_buf));
  
  SEND_TO_Q(msg_buf, d);
  d->ccr.mode = CCR_PRESTIGE_RACE;
}

void ccr_race_menu(struct descriptor_data *d) {
  snprintf(buf, sizeof(buf), "\r\nSelect a race:"
            "\r\n Base Races (no shop penalties):"
            "\r\n  [1] Human       ( 0 points / slot E)"
            "\r\n  [2] Dwarf       ( 5 points / slot D)"
            "\r\n  [3] Elf         (10 points / slot C)"
            "\r\n  [4] Ork         ( 5 points / slot D)"
            "\r\n  [5] Troll       (10 points / slot C)"
            "\r\n"
            "\r\n Metavariants (shop penalties):"
            "\r\n  [6] Cyclops     (15 points / slot B)"
            "\r\n  [7] Koborokuru  (10 points / slot C)"
            "\r\n  [8] Fomori      (15 points / slot B)"
            "\r\n  [9] Menehune    (10 points / slot C)"
            "\r\n  [A] Hobgoblin   (10 points / slot C)"
            "\r\n  [B] Giant       (15 points / slot B)"
            "\r\n  [C] Gnome       (10 points / slot C)"
            "\r\n  [D] Oni         (10 points / slot C)"
            "\r\n  [E] Wakyambi    (15 points / slot B)"
            "\r\n  [F] Ogre        (10 points / slot C)"
            "\r\n  [G] Minotaur    (15 points / slot B)"
            "\r\n  [H] Satyr       (10 points / slot C)"
            "\r\n  [I] Night-One   (15 points / slot B)"
            "\r\n"
#ifdef ALLOW_PRESTIGE_RACES
            "\r\n Special (has prerequisites):"
            "\r\n  [*] Prestige Race (costs %d - %d system points)"
            "\r\n"
            "\r\n  ?# (for help on a particular race), ex: ?A"
            "\r\n"
            "\r\nRace: ", MIN_PRESTIGE_RACE_COST, MAX_PRESTIGE_RACE_COST);
#else
            "\r\n  ?# (for help on a particular race), ex: ?A"
            "\r\n"
            "\r\nRace: ");
#endif
  SEND_TO_Q(buf, d);
  d->ccr.mode = CCR_RACE;
}

void ccr_archetype_selection_menu(struct descriptor_data *d) {
  SEND_TO_Q("\r\nThe following archetypes are available:"
            "\r\n", d);

  for (int i = 0; i < NUM_CCR_ARCHETYPES; i++) {
    snprintf(buf, sizeof(buf), "\r\n%d) %-25s (%s)", i + 1, archetypes[i]->display_name, archetypes[i]->difficulty_rating);
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
  if ((temp_obj = read_object((vnum), VIRTUAL, OBJ_LOAD_REASON_EDITING_EPHEMERAL_LOOKUP))) { \
    attach_attachment_to_weapon(temp_obj, weapon, NULL, (location), TRUE); \
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
    char helpbuf[MAX_INPUT_LENGTH];
    char non_const_arg[MAX_INPUT_LENGTH];
    strlcpy(non_const_arg, arg, sizeof(non_const_arg) - 1);
    char *line = any_one_arg(non_const_arg, helpbuf);
    if (!*line) {
      strlcpy(helpbuf, " archetypes", sizeof(helpbuf) - 1);
      do_help(d->character, helpbuf, 0, 0);
    } else {
      // Check for numbers, like 'help 1'. Skip the first char of line since it's expected to be a space.
      if (*(line) && *(line + 1)) {
        switch ((int) (*(line + 1) - '0') - 1) {
          case ARCHETYPE_STREET_SAMURAI:
            strlcpy(helpbuf, " street samurai", sizeof(helpbuf));
            break;
          case ARCHETYPE_ADEPT:
            strlcpy(helpbuf, " adepts", sizeof(helpbuf));
            break;
          case ARCHETYPE_SHAMANIC_MAGE:
            strlcpy(helpbuf, " shamans", sizeof(helpbuf));
            break;
          case ARCHETYPE_STREET_MAGE:
            strlcpy(helpbuf, " mages", sizeof(helpbuf));
            break;
          case ARCHETYPE_DECKER:
            strlcpy(helpbuf, " deckers", sizeof(helpbuf));
            break;
          default:
            strlcpy(helpbuf, line, sizeof(helpbuf));
            break;
        }
      }

      do_help(d->character, helpbuf, 0, 0);
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
  GET_SETTABLE_REAL_MAG(CH) = archetypes[i]->magic;
  GET_ASPECT(CH) = archetypes[i]->aspect;

  if (GET_TRADITION(CH) == TRAD_SHAMANIC) {
    GET_TOTEM(CH) = archetypes[i]->totem;
    GET_TOTEMSPIRIT(CH) = archetypes[i]->totemspirit;
  }

  // Foci are auto-bonded, so no need to give force points for this.
  GET_FORCE_POINTS(CH) = 0;

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
    struct obj_data *weapon = read_object(archetypes[i]->weapon, VIRTUAL, OBJ_LOAD_REASON_ARCHETYPE);

    if (weapon) {
      // snprintf(buf, sizeof(buf), "Attempting to attach %lu %lu %lu...", archetypes[i]->weapon_top, archetypes[i]->weapon_barrel, archetypes[i]->weapon_under);
      // log(buf);

      ATTACH_IF_EXISTS(archetypes[i]->weapon_top, ACCESS_ACCESSORY_LOCATION_TOP);
      ATTACH_IF_EXISTS(archetypes[i]->weapon_barrel, ACCESS_ACCESSORY_LOCATION_BARREL);
      ATTACH_IF_EXISTS(archetypes[i]->weapon_under, ACCESS_ACCESSORY_LOCATION_UNDER);

      // Fill pockets.
      if (archetypes[i]->ammo_q) {
        update_bulletpants_ammo_quantity(CH, GET_WEAPON_ATTACK_TYPE(weapon), AMMO_NORMAL, archetypes[i]->ammo_q);
        AMMOTRACK(CH, GET_WEAPON_ATTACK_TYPE(weapon), AMMO_NORMAL, AMMOTRACK_CHARGEN, archetypes[i]->ammo_q);
      }

      // Put the weapon in their inventory.
      obj_to_char(weapon, CH);
    } else {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Chargen archetype %d has invalid weapon vnum %ld!", i, archetypes[i]->weapon);
    }
  }

  // Grant modulator (unbonded, unworn).
  if (archetypes[i]->modulator > 0) {
    if ((temp_obj = read_object(archetypes[i]->modulator, VIRTUAL, OBJ_LOAD_REASON_ARCHETYPE))) {
      obj_to_char(temp_obj, CH);
    } else {
      snprintf(buf, sizeof(buf), "SYSERR: Invalid modulator %ld specified for archetype %s.", archetypes[i]->modulator, archetypes[i]->name);
      mudlog(buf, CH, LOG_SYSLOG, TRUE);
    }
  }


  // Equip worn items.
  for (int wearloc = 0; wearloc < NUM_WEARS; wearloc++)
    if (archetypes[i]->worn[wearloc] > 0) {
      if ((temp_obj = read_object(archetypes[i]->worn[wearloc], VIRTUAL, OBJ_LOAD_REASON_ARCHETYPE))) {
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
      if ((temp_obj = read_object(archetypes[i]->carried[carried], VIRTUAL, OBJ_LOAD_REASON_ARCHETYPE))) {
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
    if ((temp_obj = read_object(archetypes[i]->cyberdeck, VIRTUAL, OBJ_LOAD_REASON_ARCHETYPE))) {
      obj_to_char(temp_obj, CH);
    } else {
      snprintf(buf, sizeof(buf), "SYSERR: Invalid cyberdeck %ld specified for archetype %s.",
               archetypes[i]->cyberdeck, archetypes[i]->name);
      mudlog(buf, CH, LOG_SYSLOG, TRUE);
    }

    for (int j = 0; j < NUM_ARCHETYPE_SOFTWARE; j++) {
      struct obj_data *program;
      if ((program = read_object(archetypes[i]->software[j], VIRTUAL, OBJ_LOAD_REASON_ARCHETYPE))) {
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

  // Give them a set of default gear.
  {
    #define NUM_ARCH_GEAR_ENTRIES  4
    vnum_t default_arch_gear[NUM_ARCH_GEAR_ENTRIES] = {
      OBJ_NEOPHYTE_DUFFELBAG,
      OBJ_CELL_PHONE,
      OBJ_ELECTRONICS_KIT,
      OBJ_MAP_OF_SEATTLE
    };

    for (int idx = 0; idx < NUM_ARCH_GEAR_ENTRIES; idx++) {
      if ((temp_obj = read_object(default_arch_gear[idx], VIRTUAL, OBJ_LOAD_REASON_ARCHETYPE)))
        obj_to_char(temp_obj, CH);
      else {
        mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: Invalid item %ld specified at index %d for default archetype gear set.", default_arch_gear[idx], idx);
      }
    }
    #undef NUM_ARCH_GEAR_ENTRIES
  }

  // Set their index and essence. Everyone starts with 0 bioware index and max natural essence.
  GET_INDEX(CH) = 0;
  GET_REAL_ESS(CH) = GET_RACIAL_STARTING_ESSENCE_FOR_RACE(GET_RACE(CH));

  // Equip cyberware (deduct essence and modify stats as appropriate)
  for (int cyb = 0; cyb < NUM_ARCHETYPE_CYBERWARE; cyb++) {
    if (archetypes[i]->cyberware[cyb]) {
      if (!(temp_obj = read_object(archetypes[i]->cyberware[cyb], VIRTUAL, OBJ_LOAD_REASON_ARCHETYPE))) {
        snprintf(buf, sizeof(buf), "SYSERR: Invalid cyberware item %ld specified for archetype %s.",
                 archetypes[i]->cyberware[cyb], archetypes[i]->name);
        mudlog(buf, CH, LOG_SYSLOG, TRUE);
        continue;
      }

      int esscost = GET_CYBERWARE_ESSENCE_COST(temp_obj);

      if (IS_GHOUL(CH) || IS_DRAKE(CH))
        esscost *= 2;

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
      if (!(temp_obj = read_object(archetypes[i]->bioware[bio], VIRTUAL, OBJ_LOAD_REASON_ARCHETYPE))) {
        snprintf(buf, sizeof(buf), "SYSERR: Invalid bioware item %ld specified for archetype %s.",
                 archetypes[i]->bioware[bio], archetypes[i]->name);
        mudlog(buf, CH, LOG_SYSLOG, TRUE);
        continue;
      }

      int esscost = GET_BIOWARE_ESSENCE_COST(temp_obj);

      if (IS_DRAKE(CH))
        esscost *= 2;

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

  set_character_skill(d->character, SKILL_ENGLISH, STARTING_LANGUAGE_SKILL_LEVEL, FALSE, FALSE);
  GET_LANGUAGE(d->character) = SKILL_ENGLISH;
  GET_RESTRING_POINTS(d->character) = STARTING_RESTRING_POINTS;
  GET_LOADROOM(d->character) = archetypes[i]->start_room;

  GET_ARCHETYPAL_MODE(d->character) = TRUE;
  GET_ARCHETYPAL_TYPE(d->character) = i;
  init_char_sql(d->character, "archetype_selection_parse()");

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
      set_character_skill(CH, skill, archetypes[i]->skills[skill], FALSE, FALSE);

  // Grant subsidy card (bonded to ID). Has to be after CreateChar so the idnum doesn't change.
  temp_obj = read_object(OBJ_NEOPHYTE_SUBSIDY_CARD, VIRTUAL, OBJ_LOAD_REASON_ARCHETYPE);
  GET_OBJ_VAL(temp_obj, 0) = GET_IDNUM(CH);
  GET_OBJ_VAL(temp_obj, 1) = archetypes[i]->subsidy_card;
  obj_to_char(temp_obj, CH);

  // Bond and equip foci. Has to be after CreateChar so the idnum doesn't change.
  for (int focus = 0; focus < NUM_ARCHETYPE_FOCI; focus++) {
    if (archetypes[i]->foci[focus][0] > 0) {
      if ((temp_obj = read_object(archetypes[i]->foci[focus][0], VIRTUAL, OBJ_LOAD_REASON_ARCHETYPE))) {
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
  GET_LOADROOM(d->character) = archetypes[i]->start_room;
  playerDB.SaveChar(d->character, archetypes[i]->start_room);
}

#undef ATTACH_IF_EXISTS

void parse_pronouns(struct descriptor_data *d, const char *arg) {
  switch (tolower(*arg)) {
    case 'm':
      d->character->player.pronouns = PRONOUNS_MASCULINE;
      break;
    case 'f':
      d->character->player.pronouns = PRONOUNS_FEMININE;
      break;
    case 'n':
      d->character->player.pronouns = PRONOUNS_NEUTRAL;
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
vnum_t otaku_cyberware[2] = {
  OBJ_CYB_DATAJACK,  
  OBJ_CYB_ASIST_CONVERTER
};
#define OTAKU_CYBERWARE_NUM 2

#define CCR_MAGIC_NONE     0
#define CCR_MAGIC_FULL     1
#define CCR_MAGIC_ASPECTED 2
#define CCR_MAGIC_ADEPT    3
const char *magic_table[4] = { "None", "Full Magician", "Aspected Magician", "Adept" };
const char *gnome_magic_table[4] = { "None", "Full Shaman", "Aspected Shaman", "ERROR" };

int cg_nuyen(struct descriptor_data *d, int x)
{
  int starting_nuyen = resource_table[0][x];
  if (d->ccr.is_otaku)
    return MIN(5000, starting_nuyen);
  return starting_nuyen;
}

void set_attributes(struct char_data *ch, int magic)
{
  // Everyone starts with 0 bioware index and max natural essence.
  GET_INDEX(ch) = 0;
  GET_REAL_ESS(ch) = GET_RACIAL_STARTING_ESSENCE_FOR_RACE(GET_RACE(ch));

  // If the character is a magic user, their magic is equal to their essence (this is free).
  if (magic) {
    GET_SETTABLE_REAL_MAG(ch) = GET_REAL_ESS(ch);
  } else {
    GET_SETTABLE_REAL_MAG(ch) = 0;
  }

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

#ifdef ALLOW_PRESTIGE_RACES
  case '*':
    display_prestige_race_menu(d);
    return RETURN_HELP;
#endif

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
    case '*':
      display_help(buf2, MAX_STRING_LENGTH, "prestige races", d->character);
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

int parse_prestige_race(struct descriptor_data *d, const char *arg)
{
  switch (LOWER(*arg))
  {
  case '1':
    return RACE_DRYAD;
  case '2':
    return RACE_GHOUL_HUMAN;
  case '3':
    return RACE_GHOUL_DWARF;
  case '4':
    return RACE_GHOUL_ELF;
  case '5':
    return RACE_GHOUL_ORK;
  case '6':
    return RACE_GHOUL_TROLL;
  case '7':
    return RACE_DRAKE_HUMAN;
  case '8':
    return RACE_DRAKE_DWARF;
  case '9':
    return RACE_DRAKE_ELF;
  case '0':
    return RACE_DRAKE_ORK;
  case 'a':
  case 'A':
    return RACE_DRAKE_TROLL;
  case 'b':
  case 'B':
    return RACE_WESTERN_DRAGON;
  case 'c':
  case 'C':
    return RACE_EASTERN_DRAGON;
  case 'd':
  case 'D':
    return RACE_FEATHERED_SERPENT;
  case 'o':
  case 'O':
    d->ccr.is_otaku = !d->ccr.is_otaku;
    ccr_race_menu(d);
    return RETURN_HELP;
  case 'X':
  case 'x':
  case 'Q':
  case 'q':
  case '*':
    ccr_race_menu(d);
    return RETURN_HELP;
  case '?':
    switch (LOWER(*(arg+1))) {
    case '1':
      display_help(buf2, MAX_STRING_LENGTH, "dryads", d->character);
      break;
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
      display_help(buf2, MAX_STRING_LENGTH, "ghouls", d->character);
      break;
    case '7':
    case '8':
    case '9':
    case '0':
    case 'a':
    case 'A':
      display_help(buf2, MAX_STRING_LENGTH, "drakes", d->character);
      break;
    case 'b':
    case 'B':
    case 'c':
    case 'C':
    case 'd':
    case 'D':
      display_help(buf2, MAX_STRING_LENGTH, "dragons", d->character);
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

  // Except for otaku who are always mundane lol
  if (*arg == 2 && d->ccr.is_otaku && d->ccr.temp <= 1) {
    snprintf(buf2, sizeof(buf2), "Magic cannot be assigned on Otaku characters, they are always mundane.");
    SEND_TO_Q(buf2, d);
    return 0;
  }

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
          GET_NUYEN_RAW(d->character) = 0;
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
      GET_NUYEN_RAW(d->character) = MIN(nuyen_vals[d->ccr.temp], d->ccr.is_otaku ? 5000 : nuyen_vals[0]);
      d->ccr.force_points = force_vals[d->ccr.temp];
      break;
    case PR_SKILL:
      GET_SKILL_POINTS(d->character) = skill_vals[d->ccr.temp];
      break;
    }
    d->ccr.temp = 0;
    GET_PP(d->character) = (IS_GHOUL(CH) ? 500 : (IS_DRAGON(CH) ? 700 : 600));
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
      snprintf(buf3, sizeof(buf3), "%s?           %-2d           %-2d        %d nuyen / %d\r\n",
               buf2, attrib_vals[i], skill_vals[i], nuyen_vals[i], force_vals[i]);
      strlcpy(buf2, buf3, sizeof(buf2));
      break;
    case PR_RACE:
      snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "%-11s -            -         -\r\n", pc_race_types[(int) GET_RACE(d->character)]);
      break;
    case PR_OTAKU:
      strlcat(buf2, "Otaku       -            -         -\r\n", sizeof(buf2));
      break;
    case PR_MAGIC:
      if ( i == 0 ) {
        if (GET_RACE(CH) == RACE_GNOME || GET_RACE(CH) == RACE_DRYAD) {
          strlcat(buf2, "Full Shaman -            -         -\r\n", sizeof(buf2));
        } else {
          strlcat(buf2, "Full Mage   -            -         -\r\n", sizeof(buf2));
        }
      } else if ( i == 1 ) {
        if (GET_RACE(CH) == RACE_GNOME || GET_RACE(CH) == RACE_DRYAD) {
          strlcat(buf2, "Asp. Shaman -            -         -\r\n", sizeof(buf2));
        } else {
          strlcat(buf2, "Adept/Aspect-            -         -\r\n", sizeof(buf2));
        }
      } else
        strlcat(buf2, "Mundane     -            -         -\r\n", sizeof(buf2));
      break;
    case PR_ATTRIB:
      snprintf(buf3, sizeof(buf3), "%sAttributes  %-2d           -         -\r\n",
               buf2,
               attrib_vals[i]);
      strlcpy(buf2, buf3, sizeof(buf2));
      break;
    case PR_SKILL:
      snprintf(buf3, sizeof(buf3), "%sSkills      -            %-2d        -\r\n",
               buf2,
               skill_vals[i]);
      strlcpy(buf2, buf3, sizeof(buf2));
      break;
    case PR_RESOURCE:
      snprintf(buf3, sizeof(buf3), "%sResources   -            -         %d nuyen / %d\r\n",
               buf2,
               MIN(nuyen_vals[i], d->ccr.is_otaku ? 5000 : nuyen_vals[0]),
               force_vals[i]);
      strlcpy(buf2, buf3, sizeof(buf2));
      break;
    }
    SEND_TO_Q(buf2, d);
  }
  SEND_TO_Q("\r\nChoose a priority (A-E) to set (? for help, p to continue): ", d);
}

void init_char_sql(struct char_data *ch, const char *call_origin)
{
  log_vfprintf("init_char_sql(%s, %s)", GET_CHAR_NAME(ch), call_origin);
  char buf2[MAX_STRING_LENGTH];
  snprintf(buf, sizeof(buf), "INSERT INTO pfiles (`idnum`, `name`, `password`, `race`, `gender`, `Rank`, `Voice`,"\
               "`Physical_Keywords`, `Physical_Name`, `Whotitle`, `Height`, `Weight`, `Host`,"\
               "`Tradition`, `Otaku_Path`, `Born`, `Background`, `Physical_LookDesc`, `Matrix_LookDesc`, `Astral_LookDesc`, `LastD`, `multiplier`) VALUES ('%ld', '%s', '%s', %d, '%d',"\
               "'%d', '%s', '%s', '%s', '%s', '%d', '%d', '%s', '%d', '%d', '%ld', '%s', '%s', '%s', '%s', %ld, 100);", GET_IDNUM(ch),
               GET_CHAR_NAME(ch), GET_PASSWD(ch), GET_RACE(ch), GET_PRONOUNS(ch), MAX(1, GET_LEVEL(ch)),
               prepare_quotes(buf2, ch->player.physical_text.room_desc, sizeof(buf2) / sizeof(buf2[0])), GET_KEYWORDS(ch), GET_NAME(ch), GET_WHOTITLE(ch),
               GET_HEIGHT(ch), GET_WEIGHT(ch), ch->player.host, GET_TRADITION(ch), GET_OTAKU_PATH(ch), ch->player.time.birth, "A blank slate.",
               "A nondescript person.\r\n", "A nondescript entity.\r\n", "A nondescript entity.\r\n", time(0));
  mysql_wrapper(mysql, buf);
  if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
    snprintf(buf, sizeof(buf), "INSERT INTO pfiles_chargendata (`idnum`, `AttPoints`, `SkillPoints`, `ForcePoints`, `archetypal`, `archetype`, `prestige_alt`) VALUES"\
               "('%ld', '%d', '%d', '%d', '%d', '%d', '%ld');", 
               GET_IDNUM(ch), GET_ATT_POINTS(ch), GET_SKILL_POINTS(ch), GET_FORCE_POINTS(ch), GET_ARCHETYPAL_MODE(ch) ? 1 : 0, GET_ARCHETYPAL_TYPE(ch), GET_PRESTIGE_ALT_ID(ch));
    mysql_wrapper(mysql, buf);
  }
  if (GET_TRADITION(ch) != TRAD_MUNDANE) {
    snprintf(buf, sizeof(buf), "INSERT INTO pfiles_magic (`idnum`, `Totem`, `TotemSpirit`, `Aspect`) VALUES"\
               "('%ld', '%d', '%d', '%d');", GET_IDNUM(ch), GET_TOTEM(ch), GET_TOTEMSPIRIT(ch), GET_ASPECT(ch));
    mysql_wrapper(mysql, buf);
  }
  if (GET_LEVEL(ch) > 0) {
    snprintf(buf, sizeof(buf), "INSERT INTO pfiles_immortdata (`idnum`, `InvisLevel`, `IncogLevel`, `Zonenumber`, `Poofin`, `Poofout`) VALUES ("\
                 "%ld, %d, %d, %d, '%s', '%s');",
                 GET_IDNUM(ch), GET_INVIS_LEV(ch), GET_INCOG_LEV(ch), ch->player_specials->saved.zonenum,
                (POOFIN(ch) ? POOFIN(ch) : DEFAULT_POOFIN_STRING),
                (POOFOUT(ch) ? POOFOUT(ch) : DEFAULT_POOFOUT_STRING));
    mysql_wrapper(mysql, buf);
  }
}

static void start_game(descriptor_data *d, const char *origin)
{
  log_vfprintf("start_game(%s, %s)", GET_CHAR_NAME(d->character), origin);
  CreateChar(d->character);
  d->character->player.host = str_dup(d->host);

  set_character_skill(d->character, SKILL_ENGLISH, STARTING_LANGUAGE_SKILL_LEVEL, FALSE);
  GET_LANGUAGE(d->character) = SKILL_ENGLISH;
  GET_RESTRING_POINTS(d->character) = STARTING_RESTRING_POINTS;

  if (GET_ARCHETYPAL_MODE(CH)) {
    GET_LOADROOM(d->character) = archetypes[GET_ARCHETYPAL_TYPE(CH)]->start_room;
  } else {
    GET_LOADROOM(d->character) = RM_CHARGEN_START_ROOM;
  }

  // Otaku get some starting gear, so assign that here
  if (d->ccr.is_otaku) {
    // Equip cyberware (deduct essence and modify stats as appropriate)
    obj_data *temp_obj;
    for (int cyb = 0; cyb < OTAKU_CYBERWARE_NUM; cyb++) {
      if (otaku_cyberware[cyb]) {
        if (!(temp_obj = read_object(otaku_cyberware[cyb], VIRTUAL, OBJ_LOAD_REASON_ARCHETYPE))) {
          snprintf(buf, sizeof(buf), "SYSERR: Invalid cyberware item %ld specified for otaku class %s.",
                  otaku_cyberware[cyb]);
          mudlog(buf, CH, LOG_SYSLOG, TRUE);
          continue;
        }

        int esscost = GET_CYBERWARE_ESSENCE_COST(temp_obj);

        if (IS_GHOUL(CH) || IS_DRAKE(CH))
          esscost *= 2;

        if (GET_TRADITION(CH) != TRAD_MUNDANE) {
          if (GET_TOTEM(CH) == TOTEM_EAGLE)
            esscost *= 2;
          magic_loss(CH, esscost, TRUE);
        }

        GET_REAL_ESS(CH) -= esscost;

        obj_to_cyberware(temp_obj, CH);
      }
    }
  }

  init_char_sql(d->character, "start_game()");
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
  if (GET_RACE(CH) == RACE_DRYAD) {
    GET_TOTEM(CH) = TOTEM_FATHERTREE;
    start_game(d, "ccr_totem_menu");
    return;
  }
  
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
  strlcpy(buf,   "As an aspected mage, you must select your aspect: \r\n"
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

void ccr_mage_menu(struct descriptor_data *d)
{
  strlcpy(buf,   "As a hermetic mage, you must select your aspect: \r\n"
                 "  [1] Full Mage (Jack of All Trades)\r\n"
                 "  [2] Earth Mage (Manipulation Bonus / Detection Penalty)\r\n"
                 "  [3] Air Mage (Detection Bonus / Manipulation Penalty)\r\n"
                 "  [4] Fire Mage (Combat Bonus / Illusion Penalty)\r\n"
                 "  [5] Water Mage (Illusion Bonus / Combat Penalty)\r\n", sizeof(buf));

  strlcat(buf,   "  [?] Help\r\n\r\nAspect: ", sizeof(buf));
  SEND_TO_Q(buf, d);
  d->ccr.mode = CCR_MAGE;
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
  const char **magic_table_ptr;
  d->ccr.mode = CCR_POINTS;
  if (GET_RACE(CH) == RACE_GNOME || GET_RACE(CH) == RACE_DRYAD) {
    magic_table_ptr = gnome_magic_table;
  } else {
    magic_table_ptr = magic_table;
  }
  snprintf(buf, sizeof(buf), "  1) Attributes: ^c%15d^n (^c%3d^n Points)\r\n"
               "  2) Skills    : ^c%15d^n (^c%3d^n Points)\r\n"
               "  3) Resources : ^c%15d^n (^c%3d^n Points)\r\n"
               "  4) Magic     : ^c%15s^n (^c%3d^n Points)\r\n"
               "     Race      : ^c%15s^n (^c%3d^n Points)\r\n"
               "     Class     : ^c%15s^n (^c%3d^n Points)\r\n"
               "  Points Remaining: ^c%d^n\r\n"
               "Choose an area to change points on(p to continue): ", d->ccr.pr[PO_ATTR]/2, d->ccr.pr[PO_ATTR],
               d->ccr.pr[PO_SKILL], d->ccr.pr[PO_SKILL], cg_nuyen(d, d->ccr.pr[PO_RESOURCES]),
               resource_table[1][d->ccr.pr[PO_RESOURCES]], magic_table_ptr[d->ccr.pr[PO_MAGIC]],
               magic_cost[d->ccr.pr[PO_MAGIC]], pc_race_types[(int)GET_RACE(d->character)], d->ccr.pr[PO_RACE], 
               d->ccr.is_otaku ? "Otaku" : "None", d->ccr.is_otaku ? 30 : 0, d->ccr.points);
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
    amount += racial_limits[race][RACIAL_LIMITS_NORMAL][attr] - (MAX(1, racial_attribute_modifiers[race][attr] + 1));
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
    if (i > ((GET_RACE(CH) == RACE_GNOME || GET_RACE(CH) == RACE_DRYAD) ? CCR_MAGIC_ASPECTED : CCR_MAGIC_ADEPT) || i < CCR_MAGIC_NONE)
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
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %d) %8d nuyen   (%2d points)\r\n ", x+1, cg_nuyen(d, x), resource_table[1][x]);
        if (d->ccr.is_otaku)
          send_to_char(CH, "^wNOTE:^n Otaku are always limited to 5000 nuyen on start.\r\n ");
        SEND_TO_Q(buf, d);
        send_to_char(CH, "Enter desired amount of nuyen points (^c%d^n available): ", d->ccr.points);
        d->ccr.mode = CCR_PO_RESOURCES;
        break;
      case '4':
        if (d->ccr.is_otaku) {
          send_to_char(CH, "Otaku are always considered mundane.\r\n");
          break;
        }
        d->ccr.points += magic_cost[d->ccr.pr[PO_MAGIC]];
        snprintf(buf, sizeof(buf), " ");
        if (GET_RACE(CH) == RACE_GNOME || GET_RACE(CH) == RACE_DRYAD) {
          for (int x = 0; x < 3; x++)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %d) %18s (%2d points)\r\n ", x+1, gnome_magic_table[x], magic_cost[x]);
        } else {
          for (int x = 0; x < 4; x++)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %d) %18s (%2d points)\r\n ", x+1, magic_table[x], magic_cost[x]);
        }
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

        GET_NUYEN_RAW(CH) = cg_nuyen(d, d->ccr.pr[PO_RESOURCES]);
        GET_SKILL_POINTS(CH) = d->ccr.pr[PO_SKILL];
        GET_ATT_POINTS(CH) = d->ccr.pr[PO_ATTR]/2;

        if (d->ccr.is_otaku) {
          d->ccr.mode = CCR_OTAKU_PATH;
          SEND_TO_Q("\r\nFollow the [c]yberadept, or [t]echnoshaman path?  Enter '?' for help. ", d);
        } else if (d->ccr.pr[PO_MAGIC] > CCR_MAGIC_NONE) {
          set_attributes(CH, 1);
          if (GET_RACE(CH) == RACE_GNOME || GET_RACE(CH) == RACE_DRYAD) {
            GET_TRADITION(CH) = TRAD_SHAMANIC;
            if (d->ccr.pr[PO_MAGIC] == CCR_MAGIC_FULL) {
              GET_FORCE_POINTS(CH) = 25;
              ccr_totem_menu(d);
            } else {
              GET_FORCE_POINTS(CH) = 35;
              ccr_aspect_menu(d);
            }
          } else if (d->ccr.pr[PO_MAGIC] == CCR_MAGIC_ADEPT) {
            GET_TRADITION(CH) = TRAD_ADEPT;
            GET_PP(CH) = (IS_GHOUL(CH) ? 500 : (IS_DRAGON(CH) ? 700 : 600));
            start_game(d, "create_parse trad_adept");
          } else {
            d->ccr.mode = CCR_TRADITION;
            SEND_TO_Q("\r\nFollow [h]ermetic or [s]hamanic magical tradition? (enter ? for help) ", d);
          }
        } else {
          GET_TRADITION(CH) = TRAD_MUNDANE;
          set_attributes(CH, 0);
          start_game(d, "create_parse trad_mundane");
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
        d->ccr.pr[PO_MAGIC] = CCR_MAGIC_NONE;

        // Assign racial costs and subtract them from the point value.
        d->ccr.pr[PO_RACE] = get_ccr_race_point_cost(GET_RACE(CH));
      
        d->ccr.points -= d->ccr.pr[PO_RACE];
        d->ccr.pr[5] = -1;

        // Otaku hardcode removing points
        if (d->ccr.is_otaku) d->ccr.points -= 30;

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
    case CCR_PRESTIGE_RACE:
      display_prestige_race_menu(d);
      break;
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
  case CCR_PRESTIGE_RACE:
    if ((d->ccr.prestige_race = parse_prestige_race(d, arg)) == RACE_UNDEFINED) {
      SEND_TO_Q("\r\nThat's not a prestige race.\r\nRace: ", d);
      return;
    } else if (d->ccr.prestige_race == RETURN_HELP) // for when they use help
      return;

    // Set costs.
    switch (d->ccr.prestige_race) {
      case RACE_DRYAD:
        d->ccr.prestige_cost = PRESTIGE_RACE_DRYAD_COST;
        break;
      case RACE_DRAKE_HUMAN:
      case RACE_DRAKE_DWARF:
      case RACE_DRAKE_ELF:
      case RACE_DRAKE_ORK:
      case RACE_DRAKE_TROLL:
        d->ccr.prestige_cost = PRESTIGE_RACE_DRAKE_COST;
        break;
      case RACE_GHOUL_HUMAN:
      case RACE_GHOUL_DWARF:
      case RACE_GHOUL_ELF:
      case RACE_GHOUL_ORK:
      case RACE_GHOUL_TROLL:
        d->ccr.prestige_cost = PRESTIGE_RACE_GHOUL_COST;
        break;
      case RACE_EASTERN_DRAGON:
      case RACE_WESTERN_DRAGON:
      case RACE_FEATHERED_SERPENT:
        d->ccr.prestige_cost = PRESTIGE_RACE_DRAGON_COST;
        break;
      default:
        mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Unknown race %d in prestige race cost setting! Bailing.", d->ccr.prestige_race);
        SEND_TO_Q("Unrecognized race. Try again: ", d);
        d->ccr.prestige_race = 0;
        return;
    }

    // TODO: disabled for testing
    // if (d->ccr.is_otaku) d->ccr.prestige_cost += PRESTIGE_CLASS_OTAKU_COST;

    SEND_TO_Q("Enter the name of the character to deduct syspoints from: ", d);
    d->ccr.mode = CCR_PRESTIGE_PAYMENT_GET_NAME;
    break;
  case CCR_PRESTIGE_PAYMENT_GET_NAME:
    if (!str_cmp(arg, "abort")) {
      d->ccr.prestige_race = d->ccr.prestige_bagholder = 0;
      display_prestige_race_menu(d);
      return;
    }

    d->ccr.prestige_bagholder = get_player_id(arg);
    if (d->ccr.prestige_bagholder <= 0) {
      SEND_TO_Q("There is no such player. Enter a valid name, or ABORT to abort: ", d);
      return;
    }

    snprintf(buf, sizeof(buf), "Enter %s's password, or ABORT to abort: ", arg);
    SEND_TO_Q(buf, d);
    echo_off(d);
    d->ccr.mode = CCR_PRESTIGE_PAYMENT_GET_PASS;
    break;
  case CCR_PRESTIGE_PAYMENT_GET_PASS:
    echo_on(d);
    if (!str_cmp(arg, "abort")) {
      d->ccr.prestige_race = d->ccr.prestige_bagholder = 0;
      display_prestige_race_menu(d);
      return;
    }

    if (!validate_password_for_idnum(arg, d->ccr.prestige_bagholder)) {
      snprintf(buf, sizeof(buf), "Bad PW: %s [%s]", GET_CHAR_NAME(d->character), d->host);
      mudlog_vfprintf(NULL, LOG_CONNLOG, "Bad PW in prestige chargen: %s for %ld", d->host, d->ccr.prestige_bagholder);
      SEND_TO_Q("That's not the right password.\r\nEnter the target character's password, or type ABORT: ", d);
      echo_off(d);
      return;
    }

    // They're authorized to deduct from this character.
    {
      struct char_data *victim = find_or_load_ch(NULL, d->ccr.prestige_bagholder, "chargen prestige deduction", d->character); 
      bool online = FALSE;

      // Ensure they have enough
      if (GET_SYSTEM_POINTS(victim) < d->ccr.prestige_cost) {
        snprintf(buf, sizeof(buf), "%s doesn't have enough system points. You need %d, but %s (%s) only has %d.\r\n", 
                 GET_CHAR_NAME(victim),
                 d->ccr.prestige_cost,
                 GET_CHAR_NAME(victim),
                 online ? "online" : "offline",
                 GET_SYSTEM_POINTS(victim));
        SEND_TO_Q(buf, d);

        find_or_load_ch_cleanup(victim);

        d->ccr.prestige_race = 0;
        display_prestige_race_menu(d);
        return;
      }
    
      // Deduct, notify, log, and save
      GET_SYSTEM_POINTS(victim) -= d->ccr.prestige_cost;
      
      if (victim->desc)
        send_to_char(victim, "^RYou've just spent %d system points on a prestige race/class. If this is not correct, change your password and notify staff immediately.^n\r\n", d->ccr.prestige_cost);

      mudlog_vfprintf(d->character, LOG_CHEATLOG, "%s/%s spent %d of %s's syspoints to become a %s.", 
                      d->host,
                      GET_CHAR_NAME(d->character),
                      d->ccr.prestige_cost,
                      GET_CHAR_NAME(victim),
                      pc_race_types[d->ccr.prestige_race]);

      playerDB.SaveChar(victim);

      SEND_TO_Q("Nice, you're all paid up.", d);
      GET_RACE(d->character) = d->ccr.prestige_race;
      GET_PRESTIGE_ALT_ID(d->character) = d->ccr.prestige_bagholder;
      d->ccr.mode = CCR_PRESTIGE_RACE_PAID_FOR;

      find_or_load_ch_cleanup(victim);
    }
    
    // Send them right back through the parser.
    create_parse(d, arg);
    break;
  case CCR_RACE:
    if ((GET_RACE(d->character) = parse_race(d, arg)) == RACE_UNDEFINED) {
      SEND_TO_Q("\r\nThat's not a race.\r\nRace: ", d);
      return;
    } else if (GET_RACE(d->character) == RETURN_HELP) // for when they use help
      return;
    // fall through
  case CCR_PRESTIGE_RACE_PAID_FOR:
    {
      if (d->ccr.is_otaku) d->ccr.pr[0] = PR_OTAKU;
      switch (GET_RACE(CH)) {
        case RACE_HUMAN:
          d->ccr.pr[4] = PR_RACE;
          break;
        case RACE_DWARF:
        case RACE_ORK:
          d->ccr.pr[3] = PR_RACE;
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
          d->ccr.pr[2] = PR_RACE;
          break;
        case RACE_CYCLOPS:
        case RACE_FOMORI:
        case RACE_GIANT:
        case RACE_MINOTAUR:
        case RACE_NIGHTONE:
        case RACE_WAKYAMBI:
        case RACE_GHOUL_DWARF:
        case RACE_GHOUL_ORK:
        case RACE_DRAKE_HUMAN:
        case RACE_DRYAD:
        case RACE_GHOUL_ELF:
        case RACE_GHOUL_TROLL:
        case RACE_DRAKE_DWARF:
        case RACE_DRAKE_ORK:
        case RACE_DRAKE_ELF:
        case RACE_DRAKE_TROLL:
        case RACE_WESTERN_DRAGON:
        case RACE_EASTERN_DRAGON:
        case RACE_FEATHERED_SERPENT:
        default:
          d->ccr.pr[1] = PR_RACE;
          break;
      }
    }

    if (real_object(OBJ_MAP_OF_SEATTLE) > -1)
      obj_to_char(read_object(OBJ_MAP_OF_SEATTLE, VIRTUAL, OBJ_LOAD_REASON_CHARGEN_CLOTHES), d->character);
    GET_EQ(d->character, WEAR_BODY) = read_object(shirts[number(0, 4)], VIRTUAL, OBJ_LOAD_REASON_CHARGEN_CLOTHES);
    GET_EQ(d->character, WEAR_LEGS) = read_object(pants[number(0, 4)], VIRTUAL, OBJ_LOAD_REASON_CHARGEN_CLOTHES);
    GET_EQ(d->character, WEAR_FEET) = read_object(shoes[number(0, 4)], VIRTUAL, OBJ_LOAD_REASON_CHARGEN_CLOTHES);
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
      start_game(d, "create_parse totem-else");
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
    start_game(d, "create_parse after totem_otter");
    break;
  case CCR_PRIORITY:
    switch (LOWER(*arg)) {
    case 'a':
      if (d->ccr.is_otaku) {
        send_to_char(CH, "Sorry, you cannot change this due to code limitations. If you need to change it, please reconnect and restart creation.\r\n");
        priority_menu(d);
        break;
      }
    case 'b':
    case 'c':
    case 'd':
    case 'e':
      if (d->ccr.pr[(int)(LOWER(*arg)-'a')] == PR_RACE) {
        send_to_char(CH, "Sorry, your race choice is locked due to code limitations. If you need to change it, please reconnect and restart creation.\r\n");
        priority_menu(d);
      } else {
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
        if (d->ccr.is_otaku) {
          d->ccr.mode = CCR_OTAKU_PATH;
          SEND_TO_Q("\r\nFollow the [c]yberadept, or [t]echnoshaman path?  Enter '?' for help. ", d);
          return;
        }
        if (d->ccr.pr[0] == PR_MAGIC || d->ccr.pr[1] == PR_MAGIC) {
          if (GET_RACE(d->character) == RACE_GNOME || GET_RACE(CH) == RACE_DRYAD) {
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
        start_game(d, "create_parse 'p'");
      } else {
        send_to_char("You need to finish setting your priorities first.\r\n", CH);
        priority_menu(d);
      }
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

  case CCR_OTAKU_PATH:
  #define PATH_HELP_STRING "\r\nCyberadepts express their connection to the matrix in a formulaic and rational way. Their complex forms are more potent." \
            "\r\n" \
            "\r\nTechnoshamans commune with the matrix in a more magical way. The natural way in which they interact with the matrix makes it easier to channel and command." \
            "\r\n" \
            "\r\nEnter C to select Cyberadept, or T to select Technoshaman: "
    if (isalpha(*arg) && isalpha(*(arg+1))) {
      snprintf(buf, sizeof(buf), "\r\nARG: '%s'\r\n", arg);
      SEND_TO_Q(buf, d);
      SEND_TO_Q(PATH_HELP_STRING, d);
    } else {
      switch (LOWER(*arg)) {
        case 'c':
          GET_OTAKU_PATH(d->character) = OTAKU_PATH_CYBERADEPT;
          start_game(d, "create_parse otaku_path 'c'");
          break;
        case 't':
          GET_OTAKU_PATH(d->character) = OTAKU_PATH_TECHNOSHAM;
          start_game(d, "create_parse otaku_path 't'");
          break;
        default:
          SEND_TO_Q(PATH_HELP_STRING, d);
          break;
      }
    }
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
        if ((d->ccr.pr[5] == -1 && d->ccr.pr[PO_MAGIC] == CCR_MAGIC_ASPECTED) || (d->ccr.pr[5] != -1 && d->ccr.pr[1] == PR_MAGIC)) {
          GET_FORCE_POINTS(CH) = 35;
          ccr_aspect_menu(d);
        } else {
          GET_FORCE_POINTS(CH) = 25;
          ccr_mage_menu(d);
        }
        break;
      case 's':
        GET_TRADITION(d->character) = TRAD_SHAMANIC;
        if ((d->ccr.pr[5] == -1 && d->ccr.pr[PO_MAGIC] == CCR_MAGIC_ASPECTED) || (d->ccr.pr[5] != -1 && d->ccr.pr[1] == PR_MAGIC)) {
          GET_FORCE_POINTS(CH) = 35;
          ccr_aspect_menu(d);
        } else {
          GET_FORCE_POINTS(CH) = 25;
          ccr_totem_menu(d);
        }
        break;
      case 'a':
        GET_TRADITION(d->character) = TRAD_ADEPT;
        GET_PP(CH) = (IS_GHOUL(CH) ? 500 : (IS_DRAGON(CH) ? 700 : 600));
        start_game(d, "create_parse 'a'");
        break;
      default:
        SEND_TO_Q(TRADITION_HELP_STRING, d);
        break;
      }
    }
    break;
  case CCR_MAGE:
    if (GET_TRADITION(d->character) == TRAD_HERMETIC) {
      switch (i) {
      case 1:
        GET_ASPECT(d->character) = ASPECT_FULL;
        break;
      case 2:
        GET_ASPECT(d->character) = ASPECT_EARTHMAGE;
        break;
      case 3:
        GET_ASPECT(d->character) = ASPECT_AIRMAGE;
        break;
      case 4:
        GET_ASPECT(d->character) = ASPECT_FIREMAGE;
        break;
      case 5:
        GET_ASPECT(d->character) = ASPECT_WATERMAGE;
        break;
      default:
        SEND_TO_Q("\r\nFull Mages are general practitioners of all elements and the standard Hermetic experience. Elemental Mages specialize in a specific element, which they gain an advantage in at a disadvantage to their opposing element.\r\n\r\nSelect your aspect (1 for Full Mage, 2 for Earth Mage, 3 for Air Mage, 4 for Fire Mage or 5 for Water Mage.) ", d);
        return;
      }
    }
    start_game(d, "create_parse ccr_mage aspect select");
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
    start_game(d, "create_parse aspect select");
    break;
  }
}

void refund_chargen_prestige_syspoints_if_needed(struct char_data *ch) {
  int refund_amount = 0;
  if (IS_GHOUL(ch)) {
    refund_amount = PRESTIGE_RACE_GHOUL_COST;
  } else if (IS_DRAKE(ch)) {
    refund_amount = PRESTIGE_RACE_DRAKE_COST;
  } else if (IS_DRAGON(ch)) {
    refund_amount = PRESTIGE_RACE_DRAGON_COST;
  } else if (GET_RACE(ch) == RACE_DRYAD) {
    refund_amount = PRESTIGE_RACE_DRYAD_COST;
  } else if (IS_OTAKU(ch)) {
    refund_amount = PRESTIGE_CLASS_OTAKU_COST;
  }

  if (!refund_amount)
    return;

  // You must be in chargen to qualify for a refund.
  if (!PLR_FLAGGED(ch, PLR_IN_CHARGEN) && !PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "Refusing to refund %s's %d syspoints to %ld: Neither in character creation nor chargen.", GET_CHAR_NAME(ch), refund_amount, GET_PRESTIGE_ALT_ID(ch));
    return;
  }

  // You qualify, but can't be refunded - no alt known.
  if (!GET_PRESTIGE_ALT_ID(ch)) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Refusing to refund %s's %d prestige-race syspoints: No prestige alt ID (\?\?\?)", GET_CHAR_NAME(ch), refund_amount);
    send_to_char(ch, "You created this character before the prestige alt ID tracking patch (or had staff set your race manually), so you have no listed alt to refund %d syspoints. Contact Staff with the name of the character to transfer your points to.\r\n", refund_amount);
    return;
  }

  struct char_data *payer = find_or_load_ch(NULL, GET_PRESTIGE_ALT_ID(ch), "syspoints refund during prestige character deletion", ch);

  // You qualify, but can't be refunded - your alt was deleted.
  if (!payer) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: %s is an in-chargen prestige character that needs to refund %d sysp, but their parent %ld does not exist!.",
                    GET_CHAR_NAME(ch), refund_amount, GET_PRESTIGE_ALT_ID(ch));
    send_to_char(ch, "Your alt %ld no longer exists, so your refund of %d syspoints WAS NOT PROCESSED. Contact Staff with the name of the character to transfer your points to.\r\n", GET_PRESTIGE_ALT_ID(ch), refund_amount);
    return;
  }
  
  // Refund the character.
  int old_sysp = GET_SYSTEM_POINTS(payer);
  GET_SYSTEM_POINTS(payer) += refund_amount;
  playerDB.SaveChar(payer, GET_LOADROOM(payer));

  mudlog_vfprintf(ch, LOG_CHEATLOG, "Refunded %d prestige-purchase syspoints to %s due to %s deleting in character generation (%d -> %d)",
                  refund_amount, GET_CHAR_NAME(payer), GET_CHAR_NAME(ch), old_sysp, GET_SYSTEM_POINTS(payer));
  snprintf(buf, sizeof(buf), "Refunded %d prestige-purchase syspoints to %s due to you deleting during character generation.", refund_amount, GET_CHAR_NAME(payer));
  SEND_TO_Q(buf, ch->desc);

  find_or_load_ch_cleanup(payer);
}

int get_ccr_race_priority(int race_int) {
  switch (get_ccr_race_point_cost(race_int)) {
    case 0:
      return PRIORITY_E;
    case 5:
      return PRIORITY_D;
    case 10:
      return PRIORITY_C;
    case 15:
      return PRIORITY_B;
    default:
      return PRIORITY_A;
  }
}

int get_ccr_race_point_cost(int race_int) {
  switch (race_int) {
    case RACE_HUMAN:
      return 0;
    case RACE_DWARF:
    case RACE_ORK:
      return 5;
    case RACE_OGRE:
    case RACE_HOBGOBLIN:
    case RACE_GNOME:
    case RACE_ONI:
    case RACE_SATYR:
    case RACE_KOBOROKURU:
    case RACE_TROLL:
    case RACE_ELF:
    case RACE_MENEHUNE:
    case RACE_GHOUL_HUMAN:
      return 10;
    case RACE_CYCLOPS:
    case RACE_FOMORI:
    case RACE_GIANT:
    case RACE_MINOTAUR:
    case RACE_NIGHTONE:
    case RACE_WAKYAMBI:
    case RACE_GHOUL_DWARF:
    case RACE_GHOUL_ORK:
    case RACE_DRAKE_HUMAN:
    case RACE_DRYAD:
      return 15;
    case RACE_GHOUL_ELF:
    case RACE_GHOUL_TROLL:
    case RACE_DRAKE_DWARF:
    case RACE_DRAKE_ORK:
      return 20;
    case RACE_DRAKE_ELF:
    case RACE_DRAKE_TROLL:
      return 25;
    case RACE_WESTERN_DRAGON:
    case RACE_EASTERN_DRAGON:
    case RACE_FEATHERED_SERPENT:
      return 30;
    default:
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Race %d has no point value in chargen.", race_int);
      return 50;
  }
}