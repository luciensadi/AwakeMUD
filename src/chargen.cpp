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

#define CH d->character

extern MYSQL *mysql;
extern int mysql_wrapper(MYSQL *mysql, const char *buf);
extern char *prepare_quotes(char *dest, const char *str);
extern void display_help(char *help, const char *arg);

int get_minimum_attribute_points_for_race(int race);

const char *pc_race_types[] =
  {
    "Undef",
    "Undefined",
    "Human",
    "Dwarf",
    "Elf",
    "Ork",
    "Troll",
    "Cyclops",
    "Koborokuru",
    "Fomori",
    "Menehune",
    "Hobgoblin",
    "Giant",
    "Gnome",
    "Oni",
    "Wakyambi",
    "Ogre",
    "Minotaur",
    "Satyr",
    "Night-One",
    "Dragon",
    "Elemental",
    "Spirit",
    "\n"
  };

const char *race_menu =
  "\r\nSelect a race:\r\n"
  "  [1] Human\r\n"
  "  [2] Dwarf\r\n"
  "  [3] Elf\r\n"
  "  [4] Ork\r\n"
  "  [5] Troll\r\n"
  "  [6] Cyclops\r\n"
  "  [7] Koborokuru\r\n"
  "  [8] Fomori\r\n"
  "  [9] Menehune\r\n"
  "  [A] Hobgoblin\r\n"
  "  [B] Giant\r\n"
  "  [C] Gnome\r\n"
  "  [D] Oni\r\n"
  "  [E] Wakyambi\r\n"
  "  [F] Ogre\r\n"
  "  [G] Minotaur\r\n"
  "  [H] Satyr\r\n"
  "  [I] Night-One\r\n"
  "  ?# (for help on a particular race)\r\n";

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
  
  // Set all of the character's stats to 1.
  for (int attr = BOD; attr <= WIL; attr++) {
    GET_REAL_ATT(ch, attr) = 1;
  }
  
  // Subtract the cost of making all stats 1 from the character's available attribute-training points.
  int attribute_point_cost = get_minimum_attribute_points_for_race(GET_RACE(ch));
  GET_ATT_POINTS(ch) -= attribute_point_cost;
  send_to_char(ch, "You spend %d attribute points to raise your attributes to their minimums.\r\n", attribute_point_cost);

  ch->aff_abils = ch->real_abils;
}

void init_create_vars(struct descriptor_data *d)
{
  int i;

  d->ccr.mode = CCR_SEX;
  for (i = 0; i < 5; i++)
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
      display_help(buf2, "human");
      break;
    case '2':
      display_help(buf2, "dwarf");
      break;
    case '3':
      display_help(buf2, "elf");
      break;
    case '4':
      display_help(buf2, "ork");
      break;
    case '5':
      display_help(buf2, "troll");
      break;
    case '6':
      display_help(buf2, "cyclops");
      break;
    case '7':
      display_help(buf2, "koborokuru");
      break;
    case '8':
      display_help(buf2, "fomori");
      break;
    case '9':
      display_help(buf2, "menehune");
      break;
    case 'a':
      display_help(buf2, "hobgoblin");
      break;
    case 'b':
      display_help(buf2, "giant");
      break;
    case 'c':
      display_help(buf2, "gnome");
      break;
    case 'd':
      display_help(buf2, "oni");
      break;
    case 'e':
      display_help(buf2, "wakyambi");
      break;
    case 'f':
      display_help(buf2, "ogre");
      break;
    case 'g':
      display_help(buf2, "minotaur");
      break;
    case 'h':
      display_help(buf2, "satyr");
      break;
    case 'i':
      display_help(buf2, "night one");
      break;
    default:
      return RACE_UNDEFINED;
    }
    strcat(buf2, "\r\n Press [return] to continue");
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
  strcpy(parsebuf, arg);
  char *temp = parsebuf;
  int i;

  if (*temp == '?')
  {
    i = atoi(++temp);
    display_help(buf2, (char *)totem_types[i]);
    strcat(buf2, "\r\n Press [return] to continue ");
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

  switch (*arg)
  {
  case '1':
  case '2':
  case '3':
  case '4':
    for (i = 0; i < 5; i++)
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
  for (i = 0; i < 5; i++)
  {
    sprintf(buf2, "%-10c", 'A' + i);
    switch (d->ccr.pr[i]) {
    case PR_NONE:
      sprintf(buf2, "%s?           %-2d           %-2d        %d \xC2\xA5 / %d\r\n",
              buf2, attrib_vals[i], skill_vals[i], nuyen_vals[i], force_vals[i]);
      break;
    case PR_RACE:
      if (GET_RACE(d->character) == RACE_ELF)
        strcat(buf2, "Elf         -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_DRAGON)
        strcat(buf2, "Dragon     -            -        -\r\n");
      else if (GET_RACE(d->character) == RACE_TROLL)
        strcat(buf2, "Troll       -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_ORK)
        strcat(buf2, "Ork         -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_DWARF)
        strcat(buf2, "Dwarf       -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_CYCLOPS)
        strcat(buf2, "Cyclops     -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_KOBOROKURU)
        strcat(buf2, "Koborokuru  -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_FOMORI)
        strcat(buf2, "Fomori      -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_MENEHUNE)
        strcat(buf2, "Menehune    -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_HOBGOBLIN)
        strcat(buf2, "Hobgoblin   -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_GIANT)
        strcat(buf2, "Giant       -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_GNOME)
        strcat(buf2, "Gnome       -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_ONI)
        strcat(buf2, "Oni         -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_WAKYAMBI)
        strcat(buf2, "Wakyambi    -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_OGRE)
        strcat(buf2, "Ogre        -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_MINOTAUR)
        strcat(buf2, "Minotaur    -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_SATYR)
        strcat(buf2, "Satyr       -            -         -\r\n");
      else if (GET_RACE(d->character) == RACE_NIGHTONE)
        strcat(buf2, "Night-One   -            -         -\r\n");
      else
        strcat(buf2, "Human       -            -         -\r\n");
      break;
    case PR_MAGIC:
      if ( i == 0 )
        strcat(buf2, "Full Mage   -            -         -\r\n");
      else if ( i == 1 )
        strcat(buf2, "Adept/Aspect-            -         -\r\n");
      else
        strcat(buf2, "Mundane     -            -         -\r\n");
      break;
    case PR_ATTRIB:
      sprintf(buf2, "%sAttributes  %-2d           -         -\r\n", buf2,
              attrib_vals[i]);
      break;
    case PR_SKILL:
      sprintf(buf2, "%sSkills      -            %-2d        -\r\n", buf2,
              skill_vals[i]);
      break;
    case PR_RESOURCE:
      sprintf(buf2, "%sResources   -            -         %d \xC2\xA5 / %d\r\n",
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
  sprintf(buf, "INSERT INTO pfiles (idnum, name, password, race, gender, Rank, Voice,"\
               "Physical_Keywords, Physical_Name, Whotitle, Height, Weight, Host,"\
               "Tradition, Born, Background, Physical_LookDesc, Matrix_LookDesc, Astral_LookDesc, LastD) VALUES ('%ld', '%s', '%s', %d, '%d',"\
               "'%d', '%s', '%s', '%s', '%s', '%d', '%d', '%s', '%d', '%ld', '%s', '%s', '%s', '%s', %ld);", GET_IDNUM(ch),
               GET_CHAR_NAME(ch), GET_PASSWD(ch), GET_RACE(ch), GET_SEX(ch), MAX(1, GET_LEVEL(ch)),
               prepare_quotes(buf2, ch->player.physical_text.room_desc), GET_KEYWORDS(ch), GET_NAME(ch), GET_WHOTITLE(ch),
               GET_HEIGHT(ch), GET_WEIGHT(ch), ch->player.host, GET_TRADITION(ch), ch->player.time.birth, "A blank slate.",
               "A nondescript person.\r\n", "A nondescript entity.\r\n", "A nondescript entity.\r\n", time(0));
  mysql_wrapper(mysql, buf);
  if (PLR_FLAGGED(ch, PLR_AUTH)) {
    sprintf(buf, "INSERT INTO pfiles_chargendata (idnum, AttPoints, SkillPoints, ForcePoints) VALUES"\
               "('%ld', '%d', '%d', '%d');", GET_IDNUM(ch), GET_ATT_POINTS(ch), GET_SKILL_POINTS(ch), GET_FORCE_POINTS(ch));
    mysql_wrapper(mysql, buf);
  }
  if (GET_TRADITION(ch) != TRAD_MUNDANE) {
    sprintf(buf, "INSERT INTO pfiles_magic (idnum, Totem, TotemSpirit, Aspect) VALUES"\
               "('%ld', '%d', '%d', '%d');", GET_IDNUM(ch), GET_TOTEM(ch), GET_TOTEMSPIRIT(ch), GET_ASPECT(ch));
    mysql_wrapper(mysql, buf);
  }
  sprintf(buf, "INSERT INTO pfiles_drugdata (idnum) VALUES (%ld)", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  if (GET_LEVEL(ch) > 0) {
    sprintf(buf, "INSERT INTO pfiles_immortdata (idnum, InvisLevel, IncogLevel, Zonenumber, Poofin, Poofout) VALUES ("\
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

  GET_SKILL(d->character, SKILL_ENGLISH) = 10;
  GET_LANGUAGE(d->character) = SKILL_ENGLISH;
  GET_RESTRING_POINTS(d->character) = 5;
  GET_LOADROOM(d->character) = 60500;

  init_char_sql(d->character);
  if(PLR_FLAGGED(d->character,PLR_AUTH)) {
    sprintf(buf, "%s [%s] new player.",
            GET_CHAR_NAME(d->character), d->host);
    mudlog(buf, d->character, LOG_CONNLOG, TRUE);
    SEND_TO_Q(motd, d);
    SEND_TO_Q("\r\n\n*** PRESS RETURN: ", d);
    STATE(d) = CON_RMOTD;

    init_create_vars(d);
  } else {
    SEND_TO_Q(motd, d);
    SEND_TO_Q("\r\n\n*** PRESS RETURN: ", d);
    STATE(d) = CON_RMOTD;

    init_create_vars(d);

    sprintf(buf, "%s [%s] new player.",
            GET_CHAR_NAME(d->character), d->host);
    mudlog(buf, d->character, LOG_CONNLOG, TRUE);
  }
}

void ccr_totem_menu(struct descriptor_data *d)
{
  sprintf(buf, "Select your totem: ");
  for (int i = 1; i < NUM_TOTEMS; i++) {
    sprintf(ENDOF(buf), "\r\n  %s[%2d] %-20s^n", valid_totem(CH, i) ? "" : "^R", i, totem_types[i]);
    if (++i < NUM_TOTEMS)
      sprintf(ENDOF(buf), "%s[%2d] %-20s^n", valid_totem(CH, i) ? "" : "^R", i, totem_types[i]);
    if (++i < NUM_TOTEMS)
      sprintf(ENDOF(buf), "%s[%2d] %-20s^n", valid_totem(CH, i) ? "" : "^R", i, totem_types[i]);
  }
  strcat(buf, "\r\n");
  SEND_TO_Q(buf, d);
  SEND_TO_Q("\r\nTotem (?# for help): ", d);
  d->ccr.mode = CCR_TOTEM;
}

void ccr_totem_spirit_menu(struct descriptor_data *d)
{
  sprintf(buf, "Select your totem spirit bonus: \r\n");
  switch (GET_TOTEM(CH))
  {
  case TOTEM_GATOR:
    strcat(buf, "  [1] Swamp\r\n  [2] Lake\r\n  [3] River\r\n  [4] City\r\n");
    break;
  case TOTEM_SNAKE:
  case TOTEM_FOX:
    strcat(buf, "  [1] City\r\n  [2] Field\r\n  [3] Hearth\r\n  [4] Desert\r\n  [5] Forest\r\n  [6] Mountain\r\n  [7] Prairie\r\n");
    break;
  case TOTEM_WOLF:
    strcat(buf, "  [1] Forest\r\n  [2] Prairie\r\n  [3] Mountain\r\n");
    break;
  case TOTEM_FISH:
  case TOTEM_TURTLE:
    strcat(buf, "  [1] River\r\n  [2] Sea\r\n  [3] Lake\r\n  [4] Swamp\r\n");
    break;
  case TOTEM_GECKO:
    strcat(buf, "  [1] Illusion\r\n  [2] Manipulation\r\n");
    break;
  case TOTEM_GOOSE:
    strcat(buf, "  [ 1] Desert    [ 2] Forest\r\n"
                "  [ 3] Mountain  [ 4] Prairie\r\n"
                "  [ 5] Mist      [ 6] Storm\r\n"
                "  [ 7] River     [ 8] Sea\r\n"
                "  [ 9] Wind      [10] Lake\r\n"
                "  [11] Swamp\r\n");
    break;
  case TOTEM_HORSE:
    strcat(buf, "  [1] Combat\r\n  [2] Illusion\r\n");
    break;
  case TOTEM_LIZARD:
    strcat(buf, "  [1] Desert\r\n  [2] Forest\r\n  [3] Mountain\r\n");
    break;
  case TOTEM_OTTER:
    strcat(buf, "  [1] River\r\n  [2] Sea\r\n");
    break;
  }

  SEND_TO_Q(buf, d);
  SEND_TO_Q("\r\nTotem Spirit Bonus: ", d);
  d->ccr.mode = CCR_TOTEM2;
}

void ccr_aspect_menu(struct descriptor_data *d)
{
  sprintf(buf, "Select your aspect: \r\n");
  sprintf(ENDOF(buf), "  [1] Conjurer\r\n"
          "  [2] Sorcerer\r\n");
  if (GET_TRADITION(d->character) == TRAD_SHAMANIC)
    sprintf(ENDOF(buf), "  [3] Shamanist\r\n");
  else
    sprintf(ENDOF(buf), "  [3] Elementalist (Earth)\r\n"
            "  [4] Elementalist (Air)\r\n"
            "  [5] Elementalist (Fire)\r\n"
            "  [6] Elementalist (water)\r\n");
  sprintf(ENDOF(buf), "\r\nAspect: ");
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
  sprintf(buf, "  1) Attributes : ^c%8d^n (^c%2d^n Points)\r\n"
               "  2) Skills     : ^c%8d^n (^c%2d^n Points)\r\n"
               "  3) Resources  : ^c%8d^n (^c%2d^n Points)\r\n"
               "  4) Magic      : ^c%8s^n (^c%2d^n Points)\r\n"
               "     Race       : ^c%8s^n (^c%2d^n Points)\r\n"
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

int get_maximum_attribute_points_for_race(int race) {
  int amount = 0;
  for (int attr = BOD; attr <= WIL; attr++) {
    amount += racial_limits[race][0][attr] - (max(1, racial_attribute_modifiers[race][attr] + 1));
  }
  return amount;
}

void create_parse(struct descriptor_data *d, const char *arg)
{
  int i = MIN(120, atoi(arg)), ok, available_attribute_points = 0;
  long shirts[5] = { 70000, 70001, 70002, 70003, 70004 };
  long pants[5] = { 70005, 70006, 70007, 70008, 70009 };
  long shoes[5] = { 70010, 70011, 70012, 70013, 70014};
  
  int minimum_attribute_points = get_minimum_attribute_points_for_race(GET_RACE(CH));
  int maximum_attribute_points = get_maximum_attribute_points_for_race(GET_RACE(CH)) + minimum_attribute_points;

  switch (d->ccr.mode)
  {
  case CCR_PO_ATTR:
    available_attribute_points = max(minimum_attribute_points, (int) (d->ccr.points / 2));
    if (i < minimum_attribute_points) {
      send_to_char(CH, "You need a minimum of %d points in attributes. Enter desired number of attribute points (^c%d^n available):",
                   minimum_attribute_points, available_attribute_points);
      break;
    } else if (i * 2 > d->ccr.points) {
      send_to_char(CH, "You do not have enough points for that. Enter desired number of attribute points (^c%d^n possible):", available_attribute_points);
      break;
    }
    
    if (i > maximum_attribute_points) {
      send_to_char(CH, "Good news-- you only need %d attribute points to max out your character's stats! Updated sheet.\r\n", maximum_attribute_points);
      i = maximum_attribute_points;
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
      send_to_char(CH, "Invalid number. Enter desired magic level (^c%d^n available): ", d->ccr.points);
    else if (magic_cost[i] > d->ccr.points)
      send_to_char(CH, "You do not have enough points for that. Enter desired magic level(^c%d^n available):", d->ccr.points);
    else {
      d->ccr.pr[PO_MAGIC] = i;
      d->ccr.points -= magic_cost[d->ccr.pr[PO_MAGIC]];
      points_menu(d);
    }
    break;
  case CCR_POINTS:
    switch (*arg) {
      case '1':
        d->ccr.points += d->ccr.pr[PO_ATTR];
        available_attribute_points = max(get_minimum_attribute_points_for_race(GET_RACE(CH)), (int) (d->ccr.points / 2));
        send_to_char(CH, "Enter desired number of attribute points (^c%d^n available, minimum %d): ",
                     available_attribute_points, get_minimum_attribute_points_for_race(GET_RACE(CH)));
        d->ccr.mode = CCR_PO_ATTR;
        break;
      case '2':
        d->ccr.points += d->ccr.pr[PO_SKILL];
        send_to_char(CH, "Enter desired number of skill points (^c%d^n available): ", d->ccr.points);
        d->ccr.mode = CCR_PO_SKILL;
        break;
      case '3':
        d->ccr.points += resource_table[1][d->ccr.pr[PO_RESOURCES]];
        sprintf(buf, " ");
        for (int x = 0; x < 8; x++)
          sprintf(ENDOF(buf), " %d) %8d\xC2\xA5   (%2d points)\r\n ", x+1, resource_table[0][x], resource_table[1][x]);
        SEND_TO_Q(buf, d);
        send_to_char(CH, "Enter desired amount of nuyen points (^c%d^n available): ", d->ccr.points);
        d->ccr.mode = CCR_PO_RESOURCES;
        break;
      case '4':
        d->ccr.points += magic_cost[d->ccr.pr[PO_MAGIC]];
        sprintf(buf, " ");
        for (int x = 0; x < 4; x++)
          sprintf(ENDOF(buf), " %d) %18s (%2d points)\r\n ", x+1, magic_table[x], magic_cost[x]);
        SEND_TO_Q(buf, d);
        send_to_char(CH, "Enter desired magic level (^c%d^n available): ", d->ccr.points);
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
          sprintf(buf, "SYSERR: Character %s attained negative point balance %d during creation.", GET_NAME(CH), d->ccr.points);
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
            SEND_TO_Q("\r\nFollow [h]ermetic, [s]hamanic tradition? ", d);
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
            d->ccr.pr[PO_RACE] = 10;
            break;
          case RACE_CYCLOPS:
          case RACE_FOMORI:
          case RACE_GIANT:
          case RACE_MINOTAUR:
          case RACE_MENEHUNE:
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
      SEND_TO_Q(race_menu, d);
      SEND_TO_Q("\r\nRace: ", d);
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
  case CCR_SEX:
    switch (*arg) {
    case 'm':
    case 'M':
      d->character->player.sex = SEX_MALE;
      break;
    case 'f':
    case 'F':
      d->character->player.sex = SEX_FEMALE;
      break;
    default:
      SEND_TO_Q("That is not a sex..\r\nWhat IS your sex? ", d);
      return;
    }
    SEND_TO_Q(race_menu, d);
    SEND_TO_Q("\r\nRace: ", d);
    d->ccr.mode = CCR_RACE;
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
    // Check to see if the radio exists (real_object() returns -1 if it can't find an object with this vnum).
    if (real_object(60531) > -1) {
      // Read the radio into a temporary object pointer so we can reference it.
      struct obj_data *radio = read_object(60531, VIRTUAL);
        
      // Set the channel to 8 MHz.
      GET_OBJ_VAL(radio, 0) = 8;
        
      // Give the radio to the character via the radio pointer.
      obj_to_char(radio, d->character);
    }
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
        if (i < 4)
          GET_REAL_CHA(CH) = 4;
        GET_ATT_POINTS(CH) -= 4 - i;
        break;
      case TOTEM_CHEETAH:
        i = GET_REAL_QUI(CH);
        if (i < 4)
          GET_REAL_QUI(CH) = 4;
        GET_ATT_POINTS(CH) -= 4 - i;
        i = GET_REAL_INT(CH);
        if (i < 4)
          GET_REAL_INT(CH) = 4;
        GET_ATT_POINTS(CH) -= 4 - i;
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
            SEND_TO_Q("\r\nFollow [h]ermetic, [s]hamanic or som[a]tic (Adept) tradition? ", d);
          }
            return;
        } else
          GET_TRADITION(d->character) = TRAD_MUNDANE;
        start_game(d);
      } else
        priority_menu(d);
      break;
    case '?':
      display_help(buf2, "priorities");
      strcat(buf2, "\r\n Press [return] to continue ");
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
      SEND_TO_Q("\r\nThat's not a tradition!\r\n"
                "Follow [h]ermetic, [s]hamanic or som[a]tic tradition? ", d);
      break;
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
        SEND_TO_Q("\r\nInvalid Aspect! Aspect: ", d);
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
        SEND_TO_Q("\r\nInvalid Aspect! Aspect: ", d);
        return;
      }
    start_game(d);
    break;
  }
}

