// file: newdb.cpp
// author: Andrew Hynek
// contents: implementation of the index classes


#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <mysql/mysql.h>

#include "structs.h"
#include "newdb.h"
#include "db.h"
#include "comm.h"       // for shutdown()
#include "file.h"
#include "utils.h"
#include "memory.h"
#include "handler.h"
#include "vtable.h"
#include "constants.h"
#include "interpreter.h" // for alias
#include "config.h"

/* mysql_config.h must be filled out with your own connection info. */
/* For obvious reasons, DO NOT ADD THIS FILE TO SOURCE CONTROL AFTER CUSTOMIZATION. */
#include "mysql_config.h"

extern void kill_ems(char *);
extern void init_char_sql(struct char_data *ch);
static const char *const INDEX_FILENAME = "etc/pfiles/index";
extern char *cleanup(char *dest, const char *src);
extern MYSQL *mysql;
extern void add_phone_to_list(struct obj_data *);
extern Playergroup *loaded_playergroups;

// ____________________________________________________________________________
//
// global variables
// ____________________________________________________________________________

PCIndex playerDB;

// ____________________________________________________________________________
//
// PCIndex -- the PC database class: implementation
// ____________________________________________________________________________

int mysql_wrapper(MYSQL *mysql, const char *query)
{
  char buf[MAX_STRING_LENGTH];
#ifdef DEBUG
  sprintf(buf, "Executing query: %s", query);
  log(buf);
#endif
  
  int result = mysql_query(mysql, query);
  int errnum = mysql_errno(mysql);
  
  if (errnum) {
    sprintf(buf, "MYSQLERROR: %s", mysql_error(mysql));
    log(buf);
    sprintf(buf, "Offending query: %s", query);
    log(buf);
    
    // Recovery procedures for certain errors.
    switch (errnum) {
      case 2006:
        // 'MySQL server has gone away.'
        log("The MySQL server connection appears to have dropped. Attempting to establish a new one.");
        mysql_close(mysql);
        mysql = mysql_init(NULL);
        if (!mysql_real_connect(mysql, mysql_host, mysql_user, mysql_password, mysql_db, 0, NULL, 0)) {
          sprintf(buf, "FATAL ERROR: %s\r\n", mysql_error(mysql));
          log(buf);
          log("Suggestion: Make sure your DB is running and that you've specified your connection info in src/mysql_config.cpp.\r\n");
          
          // High chance this won't succeed-- the calling function will likely attempt to read
          //  the results of the query, but the query had no results and will refuse a read.
          //  This is crash-inducing behavior 99% of the time.
          shutdown();
        }
        break;
      default:
        break;
    }
  }
  return result;
}

int entry_compare(const void *one, const void *two)
{
  PCIndex::entry *ptr1 = (PCIndex::entry *)one;
  PCIndex::entry *ptr2 = (PCIndex::entry *)two;

  if (ptr1->id < ptr2->id)
    return -1;
  else if (ptr1->id > ptr2->id)
    return 1;

  return 0;
}

static void init_char(struct char_data * ch)
{
  int i;

  /* create a player_special structure */
  if (ch->player_specials == NULL)
  {
    ch->player_specials = new player_special_data;
  }

  ch->player_specials->saved.freeze_level = 0;
  ch->player_specials->saved.invis_level  = 0;
  ch->player_specials->saved.incog_level  = 0;
  ch->player_specials->saved.wimp_level   = 0;
  ch->player_specials->saved.bad_pws      = 0;

  GET_LOADROOM(ch) = NOWHERE;
  GET_WAS_IN(ch) = NOWHERE;

  ch->player.time.birth = time(0);
  ch->player.time.played = 0;
  ch->player.time.lastdisc = time(0);

  ch->player.weight = (int)gen_size(GET_RACE(ch), 0, 3, GET_SEX(ch));
  ch->player.height = (int)gen_size(GET_RACE(ch), 1, 3, GET_SEX(ch));

  ch->points.max_mental = 1000;
  ch->points.max_physical = 1000;
  ch->points.mental = GET_MAX_MENTAL(ch);
  ch->points.physical = GET_MAX_PHYSICAL(ch);
  GET_BALLISTIC(ch) = GET_TOTALBAL(ch) = GET_IMPACT(ch) = GET_TOTALIMP(ch) = 0;
  ch->points.sustained[0] = 0;
  ch->points.sustained[1] = 0;
  ch->points.grade = 0;

  if (access_level(ch, LVL_VICEPRES))
  {
    GET_COND(ch, FULL) = -1;
    GET_COND(ch, THIRST) = -1;
    GET_COND(ch, DRUNK) = -1;
  } else
  {
    GET_COND(ch,FULL) = 24;
    GET_COND(ch, THIRST) = 24;
    GET_COND(ch, DRUNK) = 0;
  }

  if (!access_level(ch, LVL_VICEPRES))
    for (i = 1; i <= MAX_SKILLS; i++)
    {
      SET_SKILL(ch, i, 0)
    }
  else
    for (i = 1; i <= MAX_SKILLS; i++)
    {
      SET_SKILL(ch, i, 100);
    }

  ch->char_specials.saved.affected_by.Clear();
}

static void init_char_strings(char_data *ch)
{
  if (ch->player.physical_text.keywords)
    delete [] ch->player.physical_text.keywords;

  size_t len = strlen(GET_CHAR_NAME(ch)) + 1; // + strlen(race) + 2;
  ch->player.physical_text.keywords = new char[len];

  strcpy(ch->player.physical_text.keywords, GET_CHAR_NAME(ch));
  *(ch->player.physical_text.keywords) =
    LOWER(*ch->player.physical_text.keywords);

  if (ch->player.physical_text.name)
    delete [] ch->player.physical_text.name;

  if (ch->player.physical_text.room_desc)
    delete [] ch->player.physical_text.room_desc;

  if (ch->player.background)
    delete [] ch->player.background;

  if (ch->player.physical_text.look_desc)
    delete [] ch->player.physical_text.look_desc;

  {
    char temp[256];

    sprintf(temp, "A %s %s", genders[(int)GET_SEX(ch)], pc_race_types[(int)GET_RACE(ch)]);
    ch->player.physical_text.name = str_dup(temp);

    sprintf(temp, "A %s %s voice", genders[(int)GET_SEX(ch)], pc_race_types[(int)GET_RACE(ch)]);
    ch->player.physical_text.room_desc = str_dup(temp);

    ch->player.physical_text.look_desc = str_dup("A fairly nondescript thing.\n");    
    ch->player.background = str_dup("A boring character.\n");
  }

  set_title(ch, "^y(Newbie)^n");
  set_pretitle(ch, NULL);
  set_whotitle(ch, " New ");

  switch(GET_RACE(ch)) {
  case RACE_HUMAN:
    set_whotitle(ch, "Human");
    break;
  case RACE_DWARF:
    set_whotitle(ch, "Dwarf");
    break;
  case RACE_ELF:
    set_whotitle(ch, "Elf");
    break;
  case RACE_ORK:
    set_whotitle(ch, "Ork");
    break;
  case RACE_TROLL:
    set_whotitle(ch, "Troll");
    break;
  case RACE_CYCLOPS:
    set_whotitle(ch, "Cyclops");
    break;
  case RACE_KOBOROKURU:
    set_whotitle(ch, "Koborokuru");
    break;
  case RACE_FOMORI:
    set_whotitle(ch, "Fomori");
    break;
  case RACE_MENEHUNE:
    set_whotitle(ch, "Menehune");
    break;
  case RACE_HOBGOBLIN:
    set_whotitle(ch, "Hobgoblin");
    break;
  case RACE_GIANT:
    set_whotitle(ch, "Giant");
    break;
  case RACE_GNOME:
    set_whotitle(ch, "Gnome");
    break;
  case RACE_ONI:
    set_whotitle(ch, "Oni");
    break;
  case RACE_WAKYAMBI:
    set_whotitle(ch, "Wakyambi");
    break;
  case RACE_OGRE:
    set_whotitle(ch, "Ogre");
    break;
  case RACE_MINOTAUR:
    set_whotitle(ch, " Minotaur");
    break;
  case RACE_SATYR:
    set_whotitle(ch, "Satyr");
    break;
  case RACE_NIGHTONE:
    set_whotitle(ch, "Night-One");
    break;
  case RACE_DRYAD:
    set_whotitle(ch, "Dryad");
    break;
  case RACE_DRAGON:
    set_whotitle(ch, "Dragon");
    break;
  default:
    mudlog("No race found at set_whotitle in class.cc", NULL, LOG_SYSLOG, TRUE);
    set_whotitle(ch, "CHKLG"); /* Will set incase the players */
  }        /* race is undeterminable      */

  GET_PROMPT(ch) = str_dup("< @pP @mM > ");
  ch->player.matrixprompt = str_dup("< @pP @mM > ");
}

char *prepare_quotes(char *dest, const char *str)
{
  if (!str)
    return NULL;
  register char *temp = &dest[0];
  while (*str) {
    if (*str == '\'' || *str == '`' || *str == '"' || *str == '\\' || *str == '%')
      *temp++ = '\\';
    *temp++ = *str++;
  }
  *temp = '\0';
  return dest; 
}

/* Some initializations for characters, including initial skills */
void do_start(struct char_data * ch)
{
  void advance_level(struct char_data * ch);

  GET_LEVEL(ch) = 1;
  GET_KARMA(ch) = 0;
  GET_REP(ch) = 0;
  GET_NOT(ch) = 0;
  GET_TKE(ch) = 0;

  ch->points.max_physical = 1000;
  ch->points.max_mental = 1000;

  ch->char_specials.saved.left_handed = (!number(0, 9) ? 1 : 0);

  advance_level(ch);

  GET_PHYSICAL(ch) = GET_MAX_PHYSICAL(ch);
  GET_MENTAL(ch) = GET_MAX_MENTAL(ch);

  GET_COND(ch, THIRST) = 24;
  GET_COND(ch, FULL) = 24;
  GET_COND(ch, DRUNK) = 0;
  GET_LOADROOM(ch) = 8039;

  PLR_FLAGS(ch).SetBit(PLR_NEWBIE);
  PRF_FLAGS(ch).SetBits(PRF_AUTOEXIT, PRF_LONGEXITS, ENDBIT);
  PLR_FLAGS(ch).SetBit(PLR_AUTH);
  ch->player.time.played = 0;
  ch->player.time.lastdisc = time(0);
}

/* Gain maximum in various points */
void advance_level(struct char_data * ch)
{
  int i;

  if (IS_SENATOR(ch))
  {
    for (i = 0; i < 3; i++)
      GET_COND(ch, i) = (char) -1;
    GET_KARMA(ch) = 0;
    GET_REP(ch) = 0;
    GET_TKE(ch) = 0;
    GET_NOT(ch) = 0;
    if (PRF_FLAGGED(ch, PRF_PKER))
      PRF_FLAGS(ch).RemoveBit(PRF_PKER);
    PRF_FLAGS(ch).SetBits(PRF_HOLYLIGHT, PRF_ROOMFLAGS,
                          PRF_NOHASSLE, ENDBIT);
  }

  playerDB.SaveChar(ch);

  sprintf(buf, "%s advanced to %s.",
          GET_CHAR_NAME(ch), status_ratings[(int)GET_LEVEL(ch)]);
  mudlog(buf, ch, LOG_MISCLOG, TRUE);
}

bool load_char(const char *name, char_data *ch, bool logon)
{
  init_char(ch);
  for (int i = 1; i <= MAX_SKILLS; i++)
    GET_SKILL(ch, i) = 0;
  ch->char_specials.carry_weight = 0;
  ch->char_specials.carry_items = 0;
  GET_BALLISTIC(ch) = GET_TOTALBAL(ch) = GET_IMPACT(ch) = GET_TOTALIMP(ch) = 0;
  ch->points.init_dice = 0;
  ch->points.init_roll = 0;
  ch->points.sustained[1] = 0;
  ch->points.sustained[0] = 0;
  GET_LAST_TELL(ch) = NOBODY;
  MYSQL_RES *res;
  MYSQL_ROW row;
  sprintf(buf, "SELECT * FROM pfiles WHERE Name='%s';", name);
  mysql_wrapper(mysql, buf);
  res = mysql_use_result(mysql);
  row = mysql_fetch_row(res);
  if (!row && mysql_field_count(mysql)) {
    mysql_free_result(res);
    return FALSE;
  }
  GET_IDNUM(ch) = atoi(row[0]);
  ch->player.char_name = str_dup(row[1]);
  strcpy(GET_PASSWD(ch), row[2]);
  GET_RACE(ch) = atoi(row[3]);
  GET_SEX(ch) = atoi(row[4]);
  GET_LEVEL(ch) = atoi(row[5]);
  AFF_FLAGS(ch).FromString(row[6]);
  PLR_FLAGS(ch).FromString(row[7]);
  PRF_FLAGS(ch).FromString(row[8]);

  ch->player.physical_text.room_desc = str_dup(row[9]);
  ch->player.background = str_dup(row[10]);
  ch->player.physical_text.keywords = str_dup(row[11]);
  ch->player.physical_text.name = str_dup(row[12]);
  ch->player.physical_text.look_desc = str_dup(row[13]);

  ch->player.matrix_text.keywords = str_dup(row[14]);
  ch->player.matrix_text.name = str_dup(row[15]);
  ch->player.matrix_text.room_desc = str_dup(row[16]);
  ch->player.matrix_text.look_desc = str_dup(row[17]);

  ch->player.astral_text.keywords = str_dup(row[18]);
  ch->player.astral_text.name = str_dup(row[19]);
  ch->player.astral_text.room_desc = str_dup(row[20]);
  ch->player.astral_text.look_desc = str_dup(row[21]);
  ch->char_specials.leave = str_dup(row[22]);
  ch->char_specials.arrive = str_dup(row[23]);
  GET_TITLE(ch) = str_dup(row[24]);
  GET_PRETITLE(ch) = str_dup(row[25]);
  GET_WHOTITLE(ch) = str_dup(row[26]);
  ch->player.prompt = str_dup(row[27]);
  ch->player.matrixprompt = str_dup(row[28]);
  ch->player.host = str_dup(row[29]);
  GET_REAL_BOD(ch) = atoi(row[30]);
  GET_REAL_QUI(ch) = atoi(row[31]);
  GET_REAL_STR(ch) = atoi(row[32]);
  GET_REAL_CHA(ch) = atoi(row[33]);
  GET_REAL_INT(ch) = atoi(row[34]);
  GET_REAL_WIL(ch) = atoi(row[35]);
  ch->real_abils.ess = atoi(row[36]);
  ch->real_abils.esshole = atoi(row[37]);
  ch->real_abils.bod_index = atoi(row[38]);
  ch->real_abils.highestindex = atoi(row[39]);
  ch->real_abils.hacking_pool_max = atoi(row[40]);
  ch->real_abils.body_pool = atoi(row[41]);
  ch->real_abils.defense_pool = atoi(row[42]);
  GET_REAL_REA(ch) = (GET_REAL_QUI(ch) + GET_REAL_INT(ch)) / 2;
  ch->aff_abils = ch->real_abils;
  GET_HEIGHT(ch) = atoi(row[43]);
  GET_WEIGHT(ch) = atoi(row[44]);
  GET_TRADITION(ch) = atoi(row[45]);
  GET_NUYEN(ch) = atoi(row[46]);
  GET_BANK(ch) = atoi(row[47]);
  GET_KARMA(ch) = atoi(row[48]);
  GET_REP(ch) = atoi(row[49]);
  GET_NOT(ch) = atoi(row[50]);
  GET_TKE(ch) = atoi(row[51]);

  GET_PHYSICAL(ch) = atoi(row[54]);
  GET_PHYSICAL_LOSS(ch) = atoi(row[55]);
  GET_MENTAL(ch) = atoi(row[56]);
  GET_MENTAL_LOSS(ch) = atoi(row[57]);
  GET_PERM_BOD_LOSS(ch) = atoi(row[58]);
  ch->char_specials.saved.left_handed = atoi(row[59]);
  GET_LANGUAGE(ch) = atoi(row[60]);
  ch->player_specials->saved.wimp_level = atoi(row[61]);
  ch->player_specials->saved.load_room = atoi(row[62]);
  ch->player_specials->saved.last_in = atoi(row[63]);

  ch->player.time.lastdisc = atol(row[64]);
  ch->player.time.birth = atol(row[65]);
  ch->player.time.played = atol(row[66]);
  GET_COND(ch, FULL) = atoi(row[67]);
  GET_COND(ch, THIRST) = atoi(row[68]);
  GET_COND(ch, DRUNK) = atoi(row[69]);
  SHOTS_FIRED(ch) = atoi(row[70]);
  SHOTS_TRIGGERED(ch) = atoi(row[71]);
  GET_COST_BREAKUP(ch) = atoi(row[73]);
  GET_AVAIL_OFFSET(ch) = atoi(row[74]);
  ch->player_specials->saved.last_veh = atol(row[75]);
  mysql_free_result(res);

  if (GET_LEVEL(ch) > 0) {
    sprintf(buf, "SELECT * FROM pfiles_immortdata WHERE idnum=%ld;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    res = mysql_use_result(mysql);
    if ((row = mysql_fetch_row(res))) {
      ch->player_specials->saved.invis_level = atoi(row[1]);
      ch->player_specials->saved.incog_level = atoi(row[2]);
      ch->player_specials->saved.zonenum = atoi(row[3]);
      POOFIN(ch) = str_dup(row[4]);
      POOFOUT(ch) = str_dup(row[5]);
    }
  } else {
    sprintf(buf, "SELECT * FROM pfiles_drugs WHERE idnum=%ld;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    res = mysql_use_result(mysql);
    int x = 0;
    while ((row = mysql_fetch_row(res))) {
      x = atoi(row[1]);
      GET_DRUG_ADDICT(ch, x) = atoi(row[2]);
      GET_DRUG_DOSES(ch, x) = atoi(row[3]);
      GET_DRUG_EDGE(ch, x) = atoi(row[4]);
      GET_DRUG_LASTFIX(ch, x) = atol(row[5]);
      GET_DRUG_ADDTIME(ch, x) = atoi(row[6]);
      GET_DRUG_TOLERANT(ch, x) = atoi(row[7]);
      GET_DRUG_LASTWITH(ch, x) = atoi(row[8]);
    }
  }
  mysql_free_result(res);


  if (PLR_FLAGGED(ch, PLR_AUTH)) {
    sprintf(buf, "SELECT * FROM pfiles_chargendata WHERE idnum=%ld;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    res = mysql_use_result(mysql);
    if ((row = mysql_fetch_row(res))) {
      GET_ATT_POINTS(ch) = atoi(row[1]);
      GET_SKILL_POINTS(ch) = atoi(row[2]);
      GET_FORCE_POINTS(ch) = atoi(row[3]);
      GET_RESTRING_POINTS(ch) = atoi(row[4]);
    }
    mysql_free_result(res);
  }

  if (GET_TRADITION(ch) != TRAD_MUNDANE) {
    sprintf(buf, "SELECT * FROM pfiles_magic WHERE idnum=%ld;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    res = mysql_use_result(mysql);
    if ((row = mysql_fetch_row(res))) {
      ch->real_abils.mag = atoi(row[1]);
      ch->real_abils.casting_pool = atoi(row[2]);
      ch->real_abils.spell_defense_pool = atoi(row[3]);
      ch->real_abils.drain_pool = atoi(row[4]);
      ch->real_abils.reflection_pool = atoi(row[5]);
      GET_TOTEM(ch) = atoi(row[6]);
      GET_TOTEMSPIRIT(ch) = atoi(row[7]);
      GET_ASPECT(ch) = atoi(row[8]);
      GET_GRADE(ch) = atoi(row[9]);
      ch->points.extrapp = atoi(row[10]);
      GET_PP(ch) = atoi(row[11]);
      GET_SIG(ch) = atoi(row[12]);
      GET_MASKING(ch) = atoi(row[13]);       
    }
    mysql_free_result(res);
    if (GET_TRADITION(ch) == TRAD_ADEPT) {
      sprintf(buf, "SELECT * FROM pfiles_adeptpowers WHERE idnum=%ld;", GET_IDNUM(ch));
      mysql_wrapper(mysql, buf);
      res = mysql_use_result(mysql);
      while ((row = mysql_fetch_row(res)))
        GET_POWER_TOTAL(ch, atoi(row[1])) = atoi(row[2]);
      mysql_free_result(res);
    }
    if (GET_GRADE(ch) > 0) {
      sprintf(buf, "SELECT * FROM pfiles_metamagic WHERE idnum=%ld;", GET_IDNUM(ch));
      mysql_wrapper(mysql, buf);
      res = mysql_use_result(mysql);
      while ((row = mysql_fetch_row(res)))
        GET_METAMAGIC(ch, atoi(row[1])) = atoi(row[2]);
      mysql_free_result(res);
    }
    if (GET_TRADITION(ch) != TRAD_ADEPT) {
      sprintf(buf, "SELECT * FROM pfiles_spells WHERE idnum=%ld ORDER BY Category DESC, Name DESC;", GET_IDNUM(ch));
      mysql_wrapper(mysql, buf);
      res = mysql_use_result(mysql);
      while ((row = mysql_fetch_row(res))) {
        spell_data *spell = new spell_data;
        spell->name = str_dup(row[1]);
        spell->type = atoi(row[2]);
        spell->subtype = atoi(row[3]);
        spell->force = atoi(row[4]);
        spell->next = ch->spells;
        ch->spells = spell;
      }
      mysql_free_result(res);
    }
  
    if (GET_TRADITION(ch) == TRAD_HERMETIC) {
      sprintf(buf, "SELECT * FROM pfiles_spirits WHERE idnum=%ld;", GET_IDNUM(ch));
      mysql_wrapper(mysql, buf);
      res = mysql_use_result(mysql);
      struct spirit_data *last = NULL;
      while ((row = mysql_fetch_row(res))) {
        spirit_data *spirit = new spirit_data;
        spirit->type = atoi(row[1]);
        spirit->force = atoi(row[2]);
        spirit->services = atoi(row[3]);
        spirit->id = atoi(row[4]);
        if (last)
          last->next = spirit;
        else
          GET_SPIRIT(ch) = spirit;
        GET_NUM_SPIRITS(ch)++;
        last = spirit;
      }
      mysql_free_result(res);
    }
  }
  sprintf(buf, "SELECT * FROM pfiles_skills WHERE idnum=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  res = mysql_use_result(mysql);
  while ((row = mysql_fetch_row(res)))
    GET_SKILL(ch, atoi(row[1])) = atoi(row[2]);
  mysql_free_result(res);


  sprintf(buf, "SELECT * FROM pfiles_alias WHERE idnum=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  res = mysql_use_result(mysql);
  while ((row = mysql_fetch_row(res))) {
    alias *a = new alias;
    a->command = str_dup(row[1]);
    a->replacement = str_dup(row[2]);

    if (strchr(a->replacement, ALIAS_SEP_CHAR) ||
        strchr(a->replacement, ALIAS_VAR_CHAR))
      a->type = ALIAS_COMPLEX;
    else
      a->type = ALIAS_SIMPLE;

    a->next = GET_ALIASES(ch);
    GET_ALIASES(ch) = a;
  }
  mysql_free_result(res);

  sprintf(buf, "SELECT * FROM pfiles_memory WHERE idnum=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  res = mysql_use_result(mysql);
  while ((row = mysql_fetch_row(res))) {
    remem *a = new remem;
    a->idnum = atol(row[1]);
    a->mem = str_dup(row[2]);
    a->next = GET_MEMORY(ch);
    GET_MEMORY(ch) = a;
  }
  mysql_free_result(res);

  sprintf(buf, "SELECT * FROM pfiles_ignore WHERE idnum=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  res = mysql_use_result(mysql);
  while ((row = mysql_fetch_row(res))) {
    remem *a = new remem;
    a->idnum = atol(row[1]);
    a->next = GET_IGNORE(ch);
    GET_IGNORE(ch) = a;
  }
  mysql_free_result(res);

  sprintf(buf, "SELECT * FROM pfiles_quests WHERE idnum=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  res = mysql_use_result(mysql);
  while ((row = mysql_fetch_row(res)))
    GET_LQUEST(ch, atoi(row[1])) = atoi(row[2]);
  mysql_free_result(res);

  sprintf(buf, "SELECT * FROM pfiles_drugdata WHERE idnum=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  res = mysql_use_result(mysql);
  if ((row = mysql_fetch_row(res))) {
    GET_DRUG_AFFECT(ch) = atoi(row[1]);
    GET_DRUG_STAGE(ch) = atoi(row[2]);
    GET_DRUG_DURATION(ch) = atoi(row[3]);
    GET_DRUG_DOSE(ch) = atoi(row[4]);
  }
  mysql_free_result(res);

  {
    struct obj_data *obj, *last_obj = NULL;
    int vnum = 0, last_in = 0, inside = 0;
    sprintf(buf, "SELECT * FROM pfiles_cyberware WHERE idnum=%ld ORDER BY posi;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    res = mysql_use_result(mysql);
    while ((row = mysql_fetch_row(res))) {
      vnum = atol(row[1]);
      if (vnum > 0 && (obj = read_object(vnum, VIRTUAL))) {
        GET_OBJ_COST(obj) = atoi(row[2]);
        if (*row[3])
          obj->restring = str_dup(row[3]);
        if (*row[4])
          obj->photo = str_dup(row[4]);
        for (int x = 0, y = 5; x < NUM_VALUES; x++, y++)
          GET_OBJ_VAL(obj, x) = atoi(row[y]);
        if (GET_OBJ_VAL(obj, 0) == CYB_PHONE && GET_OBJ_VAL(obj, 7))
          add_phone_to_list(obj);
        else if (GET_OBJ_VAL(obj, 2) == 4 && GET_OBJ_VAL(obj, 7))
          GET_OBJ_VAL(obj, 9) = 1;
        inside = atoi(row[17]);
        if (inside > 0) {
          if (inside == last_in)
            last_obj = last_obj->in_obj;
          else if (inside < last_in)
            while (inside <= last_in) {
              last_obj = last_obj->in_obj;
              last_in--;
            }
          if (last_obj)
            obj_to_obj(obj, last_obj);
        } else obj_to_cyberware(obj, ch);
        last_in = inside;
        last_obj = obj;
      }
    }
    mysql_free_result(res);
  }
  {
    struct obj_data *obj;
    int vnum = 0;
    sprintf(buf, "SELECT * FROM pfiles_bioware WHERE idnum=%ld;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    res = mysql_use_result(mysql);
    while ((row = mysql_fetch_row(res))) {
      vnum = atol(row[1]);
      if (vnum > 0 && (obj = read_object(vnum, VIRTUAL))) {
        GET_OBJ_COST(obj) = atoi(row[2]);
        for (int x = 0, y = 3; x < NUM_VALUES; x++, y++) {
          GET_OBJ_VAL(obj, x) = atoi(row[y]);
        }
        obj_to_bioware(obj, ch);
      }
    }
    mysql_free_result(res);
  }

  {
    struct obj_data *obj = NULL, *last_obj = NULL, *attach = NULL;
    long vnum;
    int inside = 0, last_in = 0;
    sprintf(buf, "SELECT * FROM pfiles_worn WHERE idnum=%ld ORDER BY posi;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    res = mysql_use_result(mysql);
    while ((row = mysql_fetch_row(res))) {
      vnum = atol(row[1]);
      if (vnum > 0 && (obj = read_object(vnum, VIRTUAL))) {
        GET_OBJ_COST(obj) = atoi(row[2]);
        if (*row[3])
          obj->restring = str_dup(row[3]);
        if (*row[4])
          obj->photo = str_dup(row[4]);
        for (int x = 0, y = 5; x < NUM_VALUES; x++, y++)
          GET_OBJ_VAL(obj, x) = atoi(row[y]);
        if (GET_OBJ_TYPE(obj) == ITEM_PHONE && GET_OBJ_VAL(obj, 2))
          add_phone_to_list(obj);
        else if (GET_OBJ_TYPE(obj) == ITEM_PHONE && GET_OBJ_VAL(obj, 2))
          GET_OBJ_VAL(obj, 9) = 1;
        if (GET_OBJ_TYPE(obj) == ITEM_FOCUS && GET_OBJ_VAL(obj, 0) == FOCI_SUSTAINED)
          GET_OBJ_VAL(obj, 4) = 0;
        if (GET_OBJ_TYPE(obj) == ITEM_FOCUS && GET_OBJ_VAL(obj, 4))
          GET_FOCI(ch)++;
        if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && IS_GUN(GET_OBJ_VAL(obj, 3)))
          for (int q = 7; q < 10; q++) 
            if (GET_OBJ_VAL(obj, q) > 0 && real_object(GET_OBJ_VAL(obj, q)) > 0 && 
               (attach = &obj_proto[real_object(GET_OBJ_VAL(obj, q))])) {
              GET_OBJ_WEIGHT(obj) += GET_OBJ_WEIGHT(attach);
              if (attach->obj_flags.bitvector.IsSet(AFF_LASER_SIGHT))
                obj->obj_flags.bitvector.SetBit(AFF_LASER_SIGHT);
              if (attach->obj_flags.bitvector.IsSet(AFF_VISION_MAG_1))
                obj->obj_flags.bitvector.SetBit(AFF_VISION_MAG_1);
              if (attach->obj_flags.bitvector.IsSet(AFF_VISION_MAG_2))
                obj->obj_flags.bitvector.SetBit(AFF_VISION_MAG_2);
              if (attach->obj_flags.bitvector.IsSet(AFF_VISION_MAG_3))  
                obj->obj_flags.bitvector.SetBit(AFF_VISION_MAG_3);
            }
        inside = atoi(row[17]);
        GET_OBJ_TIMER(obj) = atoi(row[19]);
        GET_OBJ_ATTEMPT(obj) = atoi(row[21]);
        GET_OBJ_CONDITION(obj) = atoi(row[22]);
        if (inside > 0) {
          if (inside == last_in)
            last_obj = last_obj->in_obj;
          else if (inside < last_in)
            while (inside <= last_in) {
              last_obj = last_obj->in_obj;
              last_in--;
            }
          if (last_obj)
            obj_to_obj(obj, last_obj);
        } else
          equip_char(ch, obj, atoi(row[18]));
        last_in = inside;
        last_obj = obj;
      }
    }
    mysql_free_result(res);
  }

  {
    struct obj_data *last_obj = NULL, *obj, *attach = NULL;
    int vnum = 0;
    int inside = 0, last_in = 0;
    sprintf(buf, "SELECT * FROM pfiles_inv WHERE idnum=%ld ORDER BY posi;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    res = mysql_use_result(mysql);
    while ((row = mysql_fetch_row(res))) {
      vnum = atol(row[1]);
      if (vnum > 0 && (obj = read_object(vnum, VIRTUAL))) {
        GET_OBJ_COST(obj) = atoi(row[2]);
        if (*row[3])
          obj->restring = str_dup(row[3]);
        if (*row[4])
          obj->photo = str_dup(row[4]);
        for (int x = 0, y = 5; x < NUM_VALUES; x++, y++)
          GET_OBJ_VAL(obj, x) = atoi(row[y]);
        if (GET_OBJ_TYPE(obj) == ITEM_PHONE && GET_OBJ_VAL(obj, 2))
          add_phone_to_list(obj);
        else if (GET_OBJ_TYPE(obj) == ITEM_PHONE && GET_OBJ_VAL(obj, 2))
          GET_OBJ_VAL(obj, 9) = 1;
        if (GET_OBJ_TYPE(obj) == ITEM_FOCUS && GET_OBJ_VAL(obj, 0) == FOCI_SUSTAINED)
          GET_OBJ_VAL(obj, 4) = 0;
        if (GET_OBJ_TYPE(obj) == ITEM_FOCUS && GET_OBJ_VAL(obj, 4))
          GET_FOCI(ch)++;
        if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && IS_GUN(GET_OBJ_VAL(obj, 3)))
          for (int q = 7; q < 10; q++) 
            if (GET_OBJ_VAL(obj, q) > 0 && real_object(GET_OBJ_VAL(obj, q)) > 0 && 
               (attach = &obj_proto[real_object(GET_OBJ_VAL(obj, q))])) {
              GET_OBJ_WEIGHT(obj) += GET_OBJ_WEIGHT(attach);
              if (attach->obj_flags.bitvector.IsSet(AFF_LASER_SIGHT))
                obj->obj_flags.bitvector.SetBit(AFF_LASER_SIGHT);
              if (attach->obj_flags.bitvector.IsSet(AFF_VISION_MAG_1))
                obj->obj_flags.bitvector.SetBit(AFF_VISION_MAG_1);
              if (attach->obj_flags.bitvector.IsSet(AFF_VISION_MAG_2))
                obj->obj_flags.bitvector.SetBit(AFF_VISION_MAG_2);
              if (attach->obj_flags.bitvector.IsSet(AFF_VISION_MAG_3))
                obj->obj_flags.bitvector.SetBit(AFF_VISION_MAG_3);
            }
        inside = atoi(row[17]);
        GET_OBJ_TIMER(obj) = atoi(row[18]);
        GET_OBJ_ATTEMPT(obj) = atoi(row[20]);
        GET_OBJ_CONDITION(obj) = atoi(row[21]);
        if (inside > 0) {
          if (inside == last_in)
            last_obj = last_obj->in_obj;
          else if (inside < last_in)
            while (inside <= last_in) {
              last_obj = last_obj->in_obj;
              last_in--;
            }
          if (last_obj)
            obj_to_obj(obj, last_obj);
        } else
          obj_to_char(obj, ch);
        last_in = inside;
        last_obj = obj;
      }
    }
    mysql_free_result(res);
  }
  
  // Load pgroup membership data.
  sprintf(buf, "SELECT * FROM pfiles_playergroups WHERE idnum=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  res = mysql_use_result(mysql);
  if (res && (row = mysql_fetch_row(res))) {
    long pgroup_idnum = atol(row[1]);
    GET_PGROUP_DATA(ch) = new Pgroup_data();
    GET_PGROUP_DATA(ch)->rank = atoi(row[2]);
    GET_PGROUP_DATA(ch)->privileges.FromString(row[3]);
    mysql_free_result(res);
  
    // TODO: Find the pgroup in the list. If it's not there, load it.
    Playergroup *ptr = loaded_playergroups;
    while (ptr) {
      if (ptr->get_idnum() == pgroup_idnum)
        break;
      ptr = ptr->next_pgroup;
    }
    
    if (ptr == NULL) {
      // Load it from the DB and add it to the list.
      log_vfprintf("Loading playergroup %ld.", pgroup_idnum);
      ptr = new Playergroup(pgroup_idnum);
      //*ptr->next_pgroup = loaded_playergroups;
      //loaded_playergroups = *ptr;
    } else {
      log_vfprintf("Using loaded playergroup %ld.", pgroup_idnum);
    }
    
    // Initialize character pgroup struct.
    GET_PGROUP_DATA(ch)->pgroup = ptr;
  } else {
    mysql_free_result(res);
  }
  
  // Load pgroup invitation data.
  sprintf(buf, "SELECT * FROM `playergroup_invitations` WHERE `idnum`=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  res = mysql_use_result(mysql);
  Pgroup_invitation *pgi;
  time_t expiration;
  while (res && (row = mysql_fetch_row(res))) {
    expiration = atol(row[2]);
    if (!(Pgroup_invitation::is_expired(expiration))) {
      pgi = new Pgroup_invitation(atol(row[1]), expiration);
      pgi->next = ch->pgroup_invitations;
      if (ch->pgroup_invitations)
        ch->pgroup_invitations->prev = pgi;
      ch->pgroup_invitations = pgi;
    } // Expired ones are deleted when they interact with their invitations or quit.
  }
  mysql_free_result(res);

  STOP_WORKING(ch);
  AFF_FLAGS(ch).RemoveBits(AFF_MANNING, AFF_RIG, AFF_PILOT, AFF_BANISH, AFF_FEAR, AFF_STABILIZE, AFF_SPELLINVIS, AFF_SPELLIMPINVIS, AFF_DETOX, AFF_RESISTPAIN, AFF_TRACKING, AFF_TRACKED, AFF_PRONE, ENDBIT);
  PLR_FLAGS(ch).RemoveBits(PLR_REMOTE, PLR_SWITCHED, PLR_MATRIX, PLR_PROJECT, PLR_EDITING, PLR_WRITING, PLR_PERCEIVE, PLR_VISA, ENDBIT);

  for (struct obj_data *jack = ch->cyberware; jack; jack = jack->next_content)
    if (GET_OBJ_VAL(jack, 0) == CYB_CHIPJACK) {
      int max = 0;
      for (struct obj_data *wire = ch->cyberware; wire; wire = wire->next_content) 
        if (GET_OBJ_VAL(wire, 0) == CYB_SKILLWIRE) 
          max = GET_OBJ_VAL(wire, 1);
      for (struct obj_data *chip = jack->contains; chip; chip = chip->next_content)
        ch->char_specials.saved.skills[GET_OBJ_VAL(chip, 0)][1] = skills[GET_OBJ_VAL(chip, 0)].type ? GET_OBJ_VAL(chip, 1) 
                                                                                         : MIN(max, GET_OBJ_VAL(chip, 1));
      break;
    } else if (GET_OBJ_VAL(jack, 0) == CYB_MEMORY) {
      int max = 0;
      for (struct obj_data *wire = ch->cyberware; wire; wire = wire->next_content) 
        if (GET_OBJ_VAL(wire, 0) == CYB_SKILLWIRE) 
          max = GET_OBJ_VAL(wire, 1);
      for (struct obj_data *chip = jack->contains; chip; chip = chip->next_content)
        ch->char_specials.saved.skills[GET_OBJ_VAL(chip, 0)][1] = skills[GET_OBJ_VAL(chip, 0)].type ? GET_OBJ_VAL(chip, 1)
                                                                                         : MIN(max, GET_OBJ_VAL(chip, 1));
      break;
    }

  affect_total(ch);

  if ((((long) (time(0) - ch->player.time.lastdisc)) >= SECS_PER_REAL_HOUR)) {
    GET_PHYSICAL(ch) = GET_MAX_PHYSICAL(ch);
    GET_MENTAL(ch) = GET_MAX_MENTAL(ch);
    if (AFF_FLAGS(ch).IsSet(AFF_HEALED))
      AFF_FLAGS(ch).SetBit(AFF_HEALED);
  }
  if ( !IS_SENATOR(ch) )
    PRF_FLAGS(ch).RemoveBit(PRF_ROLLS);

  if (((long) (time(0) - ch->player.time.lastdisc) >= SECS_PER_REAL_HOUR * 2) ||
      (GET_LAST_IN(ch) > 599 && GET_LAST_IN(ch) < 700)) {
    GET_LAST_IN(ch) = GET_LOADROOM(ch);
    GET_PHYSICAL(ch) = GET_MAX_PHYSICAL(ch);
    GET_MENTAL(ch) = GET_MAX_MENTAL(ch);
  }
  // initialization for imms
  if(IS_SENATOR(ch)) {
    GET_COND(ch, FULL) = -1;
    GET_COND(ch, THIRST) = -1;
    GET_COND(ch, DRUNK) = -1;
  }

  switch (GET_RACE(ch)) {
  case RACE_HUMAN:
  case RACE_OGRE:
    NATURAL_VISION(ch) = NORMAL;
    break;
  case RACE_DWARF:
  case RACE_GNOME:
  case RACE_MENEHUNE:
  case RACE_KOBOROKURU:
  case RACE_TROLL:
  case RACE_CYCLOPS:
  case RACE_FOMORI:
  case RACE_GIANT:
  case RACE_MINOTAUR:
    NATURAL_VISION(ch) = THERMOGRAPHIC;
    break;
  case RACE_ORK:
  case RACE_HOBGOBLIN:
  case RACE_SATYR:
  case RACE_ONI:
  case RACE_ELF:
  case RACE_WAKYAMBI:
  case RACE_NIGHTONE:
  case RACE_DRYAD:
    NATURAL_VISION(ch) = LOWLIGHT;
    break;
  }

  return true;
}

static bool save_char(char_data *player, DBIndex::vnum_t loadroom)
{
  char buf[MAX_STRING_LENGTH*4], buf2[MAX_STRING_LENGTH*4], buf3[MAX_STRING_LENGTH*4];
  int i, q = 0;
  long inveh = 0;
  struct obj_data *char_eq[NUM_WEARS];
  struct obj_data *temp, *next_obj;

  if (IS_NPC(player))
    return false;

  MYSQL_RES *res;
  MYSQL_ROW row;
  sprintf(buf, "SELECT * FROM pfiles WHERE idnum=%ld;", GET_IDNUM(player));
  if (!mysql_wrapper(mysql, buf)) {
    res = mysql_use_result(mysql);
    row = mysql_fetch_row(res);
    if (!row && mysql_field_count(mysql))
      init_char_sql(player);
    mysql_free_result(res);
  }

  /* worn eq */
  for (i = 0; i < NUM_WEARS; i++) {
    if (player->equipment[i])
      char_eq[i] = unequip_char(player, i, FALSE);
    else
      char_eq[i] = NULL;
  }
  /* cyberware */
  for (temp = player->cyberware; temp; temp = next_obj) {
    next_obj = temp->next_content;
    obj_from_cyberware(temp);
    obj_to_char(temp, player);
  }

  /* bioware */
  for (temp = player->bioware; temp; temp = next_obj) {
    next_obj = temp->next_content;
    obj_from_bioware(temp);
    obj_to_char(temp, player);
  }

  for (struct sustain_data *sust = GET_SUSTAINED(player); sust; sust = sust->next)
    if (!sust->caster)
      spell_modify(player, sust, FALSE);

  /**************************************************/
  if (loadroom == NOWHERE)
    loadroom = GET_LOADROOM(player);

  if (player->in_room != NOWHERE) {
    if (world[player->in_room].number <= 1) {
      GET_LAST_IN(player) = world[player->was_in_room].number;
    } else {
      GET_LAST_IN(player) = world[player->in_room].number;
    }
  }

  if (player->in_veh && player->in_veh->owner == GET_IDNUM(player))
    inveh = player->in_veh->idnum;
  
  long pgroup_num = 0;
  if (GET_PGROUP_DATA(player) && GET_PGROUP(player))
    pgroup_num = GET_PGROUP(player)->get_idnum();
  
  sprintf(buf, "UPDATE pfiles SET AffFlags='%s', PlrFlags='%s', PrfFlags='%s', Bod=%d, "\
               "Qui=%d, Str=%d, Cha=%d, Intel=%d, Wil=%d, EssenceTotal=%d, EssenceHole=%d, "\
               "BiowareIndex=%d, HighestIndex=%d, Pool_MaxHacking=%d, Pool_Body=%d, "\
               "Pool_Dodge=%d, Cash=%ld, Bank=%ld, Karma=%d, Rep=%d, Notor=%d, TKE=%d, "\
               "Dead=%d, Physical=%d, PhysicalLoss=%d, Mental=%d, MentalLoss=%d, "\
               "PermBodLoss=%d, WimpLevel=%d, Loadroom=%ld, LastRoom=%ld, LastD=%ld, Hunger=%d, Thirst=%d, Drunk=%d, "\
               "ShotsFired='%d', ShotsTriggered='%d', pgroup='%ld', "\
               "Inveh=%ld WHERE idnum=%ld;",
               AFF_FLAGS(player).ToString(), PLR_FLAGS(player).ToString(), 
               PRF_FLAGS(player).ToString(), GET_REAL_BOD(player), GET_REAL_QUI(player),
               GET_REAL_STR(player), GET_REAL_CHA(player), GET_REAL_INT(player), GET_REAL_WIL(player),
               GET_REAL_ESS(player), player->real_abils.esshole, GET_INDEX(player), 
               player->real_abils.highestindex, GET_MAX_HACKING(player), GET_BODY(player),
               GET_DEFENSE(player), GET_NUYEN(player), GET_BANK(player), GET_KARMA(player),
               GET_REP(player), GET_NOT(player), GET_TKE(player),
               PLR_FLAGGED(player, PLR_JUST_DIED), GET_PHYSICAL(player), GET_PHYSICAL_LOSS(player),
               GET_MENTAL(player), GET_MENTAL_LOSS(player), GET_PERM_BOD_LOSS(player), GET_WIMP_LEV(player),
               GET_LOADROOM(player), GET_LAST_IN(player), time(0), GET_COND(player, FULL), 
               GET_COND(player, THIRST), GET_COND(player, DRUNK),
               SHOTS_FIRED(player), SHOTS_TRIGGERED(player), pgroup_num,
               inveh, GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  for (temp = player->carrying; temp; temp = next_obj) {
    next_obj = temp->next_content;
    if (GET_OBJ_TYPE(temp) == ITEM_CYBERWARE) {
      obj_from_char(temp);
      obj_to_cyberware(temp, player);
    }
    if (GET_OBJ_TYPE(temp) == ITEM_BIOWARE) {
      obj_from_char(temp);
      obj_to_bioware(temp, player);
    }
  }
  for (struct sustain_data *sust = GET_SUSTAINED(player); sust; sust = sust->next)
    if (!sust->caster)
      spell_modify(player, sust, TRUE);
  for (i = 0; i < NUM_WEARS; i++) {
    if (char_eq[i])
      equip_char(player, char_eq[i], i);
  }
  affect_total(player);
  if (PLR_FLAGGED(player, PLR_AUTH)) {
    sprintf(buf, "UPDATE pfiles_chargendata SET AttPoints=%d, SkillPoints=%d, ForcePoints=%d, RestringPoints=%d WHERE idnum=%ld;",
                 GET_ATT_POINTS(player), GET_SKILL_POINTS(player), GET_FORCE_POINTS(player), GET_RESTRING_POINTS(player),
                 GET_IDNUM(player));
    mysql_wrapper(mysql, buf);
  }
  sprintf(buf, "DELETE FROM pfiles_skills WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  strcpy(buf, "INSERT INTO pfiles_skills (idnum, skillnum, rank) VALUES (");
  for (i = 0; i <= MAX_SKILLS; i++)
    if (GET_SKILL(player, i)) {
      if (q)
        strcat(buf, "), (");
      sprintf(ENDOF(buf), "%ld, %d, %d", GET_IDNUM(player), i, REAL_SKILL(player, i));
      q = 1;
    } 
  if (q) {
    strcat(buf, ");");
    mysql_wrapper(mysql, buf);
  }
  sprintf(buf, "UPDATE pfiles_drugdata SET Affect=%d, Stage=%d, Duration=%d, Dose=%d WHERE idnum=%ld;", 
               GET_DRUG_AFFECT(player), GET_DRUG_STAGE(player), GET_DRUG_DURATION(player), GET_DRUG_DOSE(player),
               GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  
  sprintf(buf, "DELETE FROM pfiles_drugs WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  strcpy(buf, "INSERT INTO pfiles_drugs (idnum, DrugType, Addict, Doses, Edge, LastFix, Addtime, Tolerant, LastWith) VALUES (");
  for (i = 1, q = 0; i < NUM_DRUGS; i++) {    
    if (GET_DRUG_DOSES(player, i) || GET_DRUG_EDGE(player, i) || GET_DRUG_ADDICT(player, i) || GET_DRUG_LASTFIX(player, i) || 
        GET_DRUG_ADDTIME(player, i) || GET_DRUG_TOLERANT(player, i) || GET_DRUG_LASTWITH(player, i)) {
      if (q)
        strcat(buf, "), (");
      sprintf(ENDOF(buf), "%ld, %d, %d, %d, %d, %d, %d, %d, %d", GET_IDNUM(player), i, GET_DRUG_ADDICT(player, i), 
                          GET_DRUG_DOSES(player, i), GET_DRUG_EDGE(player, i), GET_DRUG_LASTFIX(player, i),
                          GET_DRUG_ADDTIME(player, i), GET_DRUG_TOLERANT(player, i), GET_DRUG_LASTWITH(player, i));
      q = 1;
    }
  }
  if (q) {
    strcat(buf, ");");
    mysql_wrapper(mysql, buf);
  }
  if (GET_TRADITION(player) != TRAD_MUNDANE) {
    sprintf(buf, "UPDATE pfiles_magic SET Mag=%d, Pool_Casting=%d, Pool_SpellDefense=%d, Pool_Drain=%d, Pool_Reflecting=%d,"\
                 "UsedGrade=%d, ExtraPower=%d, PowerPoints=%d, Sig=%d, Masking=%d WHERE idnum=%ld;", GET_REAL_MAG(player), GET_CASTING(player),
                 GET_SDEFENSE(player), GET_DRAIN(player), GET_REFLECT(player), GET_GRADE(player), player->points.extrapp, 
                 GET_PP(player), GET_SIG(player), GET_MASKING(player), GET_IDNUM(player));
    mysql_wrapper(mysql, buf);
    if (GET_TRADITION(player) == TRAD_ADEPT) {
      sprintf(buf, "DELETE FROM pfiles_adeptpowers WHERE idnum=%ld", GET_IDNUM(player));
      mysql_wrapper(mysql, buf);
      strcpy(buf, "INSERT INTO pfiles_adeptpowers (idnum, powernum, rank) VALUES (");
      for (i = 0, q = 0; i < ADEPT_NUMPOWER; i++)
        if (GET_POWER_TOTAL(player, i)) {
          if (q)
            strcat(buf, "), (");
          sprintf(ENDOF(buf), "%ld, %d, %d", GET_IDNUM(player), i, GET_POWER_TOTAL(player, i));
          q = 1;
        }
      if (q) {
        strcat(buf, ");");
        mysql_wrapper(mysql, buf);
      }
    }

    if (player->spells) {
      sprintf(buf, "DELETE FROM pfiles_spells WHERE idnum=%ld", GET_IDNUM(player));
      mysql_wrapper(mysql, buf);
      strcpy(buf, "INSERT INTO pfiles_spells (idnum, Name, Type, SubType, Rating, Category) VALUES (");
      q = 0;
      for (struct spell_data *temp = player->spells; temp; temp = temp->next) {
        if (q)
          strcat(buf, "), (");
        sprintf(ENDOF(buf), "%ld, '%s', %d, %d, %d, %d", GET_IDNUM(player), temp->name, temp->type, temp->subtype, temp->force, spells[temp->type].category);
        q = 1;
      }
      if (q) {
        strcat(buf, ");");
        mysql_wrapper(mysql, buf);
      }
    }

    if (GET_GRADE(player) > 0) {
      sprintf(buf, "DELETE FROM pfiles_metamagic WHERE idnum=%ld", GET_IDNUM(player));
      mysql_wrapper(mysql, buf);
      strcpy(buf, "INSERT INTO pfiles_metamagic (idnum, metamagicnum, rank) VALUES (");
      for (i = 0, q = 0; i < META_MAX; i++)
        if (GET_METAMAGIC(player, i)) {
          if (q)
            strcat(buf, "), (");
          sprintf(ENDOF(buf), "%ld, %d, %d", GET_IDNUM(player), i, GET_METAMAGIC(player, i));
          q = 1;
       }
      if (q) {
        strcat(buf, ");");
        mysql_wrapper(mysql, buf);
      }
    }
    if (GET_SPIRIT(player) && GET_TRADITION(player) == TRAD_HERMETIC) {
      sprintf(buf, "DELETE FROM pfiles_spirits WHERE idnum=%ld", GET_IDNUM(player));
      mysql_wrapper(mysql, buf);
      strcpy(buf, "INSERT INTO pfiles_spirits (idnum, Type, Rating, Services, SpiritID) VALUES (");
      q = 0;
      for (struct spirit_data *spirit = GET_SPIRIT(player); spirit; spirit = spirit->next, q++) {
        if (q)
          strcat(buf, "), (");
        sprintf(ENDOF(buf), "%ld, %d, %d, %d, %d", GET_IDNUM(player), spirit->type, spirit->force, spirit->services, spirit->id);
        q = 1;
      }
      if (q) {
        strcat(buf, ");");
        mysql_wrapper(mysql, buf);
      }
    }
  }
  sprintf(buf, "DELETE FROM pfiles_quests WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  strcpy(buf, "INSERT INTO pfiles_quests (idnum, number, questnum) VALUES (");
  for (i = 0, q = 0; i <= QUEST_TIMER - 1; i++) {
    if (GET_LQUEST(player ,i)) {
      if (q)
        strcat(buf, "), (");
      sprintf(ENDOF(buf), "%ld, %d, %ld", GET_IDNUM(player), i, GET_LQUEST(player, i));
      q = 1;
    }
  }
  if (q) {
    strcat(buf, ");");
    mysql_wrapper(mysql, buf);
  }

  sprintf(buf, "DELETE FROM pfiles_memory WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  strcpy(buf, "INSERT INTO pfiles_memory (idnum, remembered, asname) VALUES (");
  q = 0;
  for (struct remem *b = GET_MEMORY(player); b; b = b->next) {
    if (q)
      strcat(buf, "), (");
    sprintf(ENDOF(buf), "%ld, %ld, '%s'", GET_IDNUM(player), b->idnum, prepare_quotes(buf3, b->mem));
    q = 1;
  }
  if (q) {
    strcat(buf, ");");
    mysql_wrapper(mysql, buf);
  }

  sprintf(buf, "DELETE FROM pfiles_ignore WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  strcpy(buf, "INSERT INTO pfiles_ignore (idnum, remembered) VALUES (");
  q = 0;
  for (struct remem *b = GET_IGNORE(player); b; b = b->next) {
    if (q)
      strcat(buf, "), (");
    sprintf(ENDOF(buf), "%ld, %ld", GET_IDNUM(player), b->idnum);
    q = 1;
  }
  if (q) {
    strcat(buf, ");");
    mysql_wrapper(mysql, buf);
  }

  sprintf(buf, "DELETE FROM pfiles_alias WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  strcpy(buf, "INSERT INTO pfiles_alias (idnum, command, replacement) VALUES (");
  q = 0;
  for (struct alias *a = GET_ALIASES(player); a; a = a->next) {
    if (q)
      strcat(buf, "), (");
    sprintf(ENDOF(buf), "%ld, '%s', '%s'", GET_IDNUM(player), a->command, prepare_quotes(buf3, a->replacement));
    q = 1;
  }
  if (q) {
    strcat(buf, ");");
    mysql_wrapper(mysql, buf);
  }

  sprintf(buf, "DELETE FROM pfiles_bioware WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  if (player->bioware) {
    strcpy(buf, "INSERT INTO pfiles_bioware (idnum, Vnum, Cost, Value0, Value1, Value2, Value3, Value4, Value5, Value6,"\
                "Value7, Value8, Value9, Value10, Value11) VALUES (");
    q = 0;
    for (struct obj_data *obj = player->bioware; obj; obj = obj->next_content) {
      if (!IS_OBJ_STAT(obj, ITEM_NORENT)) {
        if (q)
          strcat(buf, "), (");
        sprintf(ENDOF(buf), "%ld, %ld, %d", GET_IDNUM(player), GET_OBJ_VNUM(obj), GET_OBJ_COST(obj));

        for (int x = 0; x < NUM_VALUES; x++)
          sprintf(ENDOF(buf), ", %d", GET_OBJ_VAL(obj, x));
        q = 1;;
      }
    }
    if (q) {
      strcat(buf, ");");
      mysql_wrapper(mysql, buf);
    }
  }

  sprintf(buf, "DELETE FROM pfiles_cyberware WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  int level = 0, posi = 0;
  if (player->cyberware) {
    for (struct obj_data *obj = player->cyberware; obj;) {
      strcpy(buf, "INSERT INTO pfiles_cyberware (idnum, Vnum, Cost, Restring, Photo, Value0, Value1, Value2, Value3, Value4, Value5, Value6,"\
                  "Value7, Value8, Value9, Value10, Value11, Level, posi) VALUES (");
      sprintf(ENDOF(buf), "%ld, %ld, %d, '%s', '%s'", GET_IDNUM(player), GET_OBJ_VNUM(obj), GET_OBJ_COST(obj), 
                          obj->restring ? prepare_quotes(buf3, obj->restring) : "", obj->photo ? prepare_quotes(buf2, obj->photo) : "");
      if (GET_OBJ_VAL(obj, 2) == 4) {
        sprintf(ENDOF(buf), "0, 0, 0, %d, 0, 0, %d, %d, %d, 0, 0, 0", GET_OBJ_VAL(obj, 3), GET_OBJ_VAL(obj, 6), 
                           GET_OBJ_VAL(obj, 7), GET_OBJ_VAL(obj, 8));
      } else
        for (int x = 0; x < NUM_VALUES; x++)
          sprintf(ENDOF(buf), ", %d", GET_OBJ_VAL(obj, x));
      sprintf(ENDOF(buf), ", %d, %d);", level, posi++);
      mysql_wrapper(mysql, buf);
      if (obj->contains) {
        obj = obj->contains;
        level++;
        continue;
      } else if (!obj->next_content && obj->in_obj)
        while (obj && !obj->next_content && level >= 0) {
          obj = obj->in_obj;
          level--;
        }
      if (obj)
        obj = obj->next_content;
    }
  }

  sprintf(buf, "DELETE FROM pfiles_worn WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  struct obj_data *obj = NULL;
  level = posi = 0;
  for (i = 0; i < NUM_WEARS; i++)
    if ((obj = GET_EQ(player, i)) && !IS_OBJ_STAT(obj, ITEM_NORENT))
      break;
  while (obj && i < NUM_WEARS) {
    if (!IS_OBJ_STAT(obj, ITEM_NORENT)) {
      strcpy(buf, "INSERT INTO pfiles_worn (idnum, Vnum, Cost, Restring, Photo, Value0, Value1, Value2, Value3, Value4, Value5, Value6,"\
              "Value7, Value8, Value9, Value10, Value11, Inside, Position, Timer, ExtraFlags, Attempt, Cond, posi) VALUES (");
      sprintf(ENDOF(buf), "%ld, %ld, %d, '%s', '%s'", GET_IDNUM(player), GET_OBJ_VNUM(obj), GET_OBJ_COST(obj),
                          obj->restring ? prepare_quotes(buf3, obj->restring) : "", obj->photo ? prepare_quotes(buf2, obj->photo) : "");
      for (int x = 0; x < NUM_VALUES; x++)
        sprintf(ENDOF(buf), ", %d", GET_OBJ_VAL(obj, x));
      sprintf(ENDOF(buf), ", %d, %d, %d, '%s', %d, %d, %d);", level, i, GET_OBJ_TIMER(obj), GET_OBJ_EXTRA(obj).ToString(), 
                          GET_OBJ_ATTEMPT(obj), GET_OBJ_CONDITION(obj), posi++);
      mysql_wrapper(mysql, buf);
    }

    if (obj->contains && !IS_OBJ_STAT(obj, ITEM_NORENT) && GET_OBJ_TYPE(obj) != ITEM_PART) {
      obj = obj->contains;
      level++;
      continue;
    } else if (!obj->next_content && obj->in_obj)
      while (obj && !obj->next_content && level >= 0) {
        obj = obj->in_obj;
        level--;
      }

    if (!obj || !obj->next_content)
      while (i < NUM_WEARS) {
        i++;
        if ((obj = GET_EQ(player, i)) && !IS_OBJ_STAT(obj, ITEM_NORENT)) {
          level = 0;
          break;
        }
      }
    else
      obj = obj->next_content;
  }
  sprintf(buf, "DELETE FROM pfiles_inv WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  level = posi = 0;
  for (obj = player->carrying; obj;) {
    if (!IS_OBJ_STAT(obj, ITEM_NORENT)) {
      strcpy(buf, "INSERT INTO pfiles_inv (idnum, Vnum, Cost, Restring, Photo, Value0, Value1, Value2, Value3, Value4, Value5, Value6,"\
              "Value7, Value8, Value9, Value10, Value11, Inside, Timer, ExtraFlags, Attempt, Cond, posi) VALUES (");
      sprintf(ENDOF(buf), "%ld, %ld, %d, '%s', '%s'", GET_IDNUM(player), GET_OBJ_VNUM(obj), GET_OBJ_COST(obj),
                          obj->restring ? prepare_quotes(buf3, obj->restring) : "", obj->photo ? prepare_quotes(buf2, obj->photo) : "");
      for (int x = 0; x < NUM_VALUES; x++)
        sprintf(ENDOF(buf), ", %d", GET_OBJ_VAL(obj, x));
      sprintf(ENDOF(buf), ", %d, %d, '%s', %d, %d, %d);", level, GET_OBJ_TIMER(obj), GET_OBJ_EXTRA(obj).ToString(), 
                          GET_OBJ_ATTEMPT(obj), GET_OBJ_CONDITION(obj), posi++);
      mysql_wrapper(mysql, buf);
    }

    if (obj->contains && !IS_OBJ_STAT(obj, ITEM_NORENT) && GET_OBJ_TYPE(obj) != ITEM_PART) {
      obj = obj->contains;
      level++;
      continue;
    } else if (!obj->next_content && obj->in_obj)
      while (obj && !obj->next_content && level >= 0) {
        obj = obj->in_obj;
        level--;
      }

    if (obj)
      obj = obj->next_content;
  }
  if (GET_LEVEL(player) > 1) {
    sprintf(buf, "UPDATE pfiles_immortdata SET InvisLevel=%d, IncogLevel=%d, Zonenumber=%d WHERE idnum=%ld;",
                 GET_INVIS_LEV(player), GET_INCOG_LEV(player), player->player_specials->saved.zonenum, GET_IDNUM(player));
    mysql_wrapper(mysql, buf);
  }
  
  // Save pgroup membership data.
  if (GET_PGROUP_DATA(player) && GET_PGROUP(player)) {
    sprintf(buf, "INSERT INTO pfiles_playergroups (`idnum`, `group`, `Rank`, `Privileges`) VALUES ('%ld', '%ld', '%d', '%s')"
                 " ON DUPLICATE KEY UPDATE"
                 "   `group` = VALUES(`group`),"
                 "   `Rank` = VALUES(`Rank`),"
                 "   `Privileges` = VALUES(`Privileges`)",
                 GET_IDNUM(player),
                 GET_PGROUP(player)->get_idnum(),
                 GET_PGROUP_DATA(player)->rank,
                 GET_PGROUP_DATA(player)->privileges.ToString());
    mysql_wrapper(mysql, buf);
  }

  return TRUE;
}

PCIndex::PCIndex()
{
  tab = NULL;
  entry_cnt = entry_size = 0;
  needs_save = false;
}

PCIndex::~PCIndex()
{
  reset();
}

bool PCIndex::Save()
{
  if (!needs_save)
    return true;

  FILE *index = fopen(INDEX_FILENAME, "w");

  if (!tab) {
    if (entry_cnt > 0)
      log("--Error: no table when there should be?!  That's fucked up, man.");
    return false;
  }

  if (!index) {
    log_vfprintf("--Error: Could not open player index file %s\n",
        INDEX_FILENAME);
    return false;
  }

  fprintf(index,
          "* Player index file\n"
          "* Generated automatically by PCIndex::Save\n\n");

  for (int i = 0; i < entry_cnt; i++) {
    entry *ptr = tab+i;

    fprintf(index,
            "%ld %d %u %lu %s\n",
            ptr->id, ptr->level, ptr->flags, ptr->last, ptr->name);
  }

  fprintf(index, "END\n");
  fclose(index);

  needs_save = false;

  return true;
}

char_data *CreateChar(char_data *ch)
{
  char buf[MAX_STRING_LENGTH];
  strcpy(buf, "SELECT idnum FROM pfiles ORDER BY idnum DESC;");
  mysql_wrapper(mysql, buf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row && mysql_field_count(mysql)) {
    log_vfprintf("%s promoted to %s by virtue of first-come, first-serve.",
        GET_CHAR_NAME(ch), status_ratings[LVL_MAX]);
    
    // TODO: Add NODELETE, OLC on

    for (int i = 0; i < 3; i++)
      GET_COND(ch, i) = (char) -1;

    GET_KARMA(ch) = 0;
    GET_LEVEL(ch) = LVL_MAX;
    GET_REP(ch) = 0;
    GET_NOT(ch) = 0;
    GET_TKE(ch) = 0;
    GET_IDNUM(ch) = 1;

    PLR_FLAGS(ch).RemoveBits(PLR_NEWBIE, PLR_AUTH, ENDBIT);
    PLR_FLAGS(ch).SetBits(PLR_OLC, PLR_NODELETE, ENDBIT);
    PRF_FLAGS(ch).SetBits(PRF_HOLYLIGHT, PRF_CONNLOG, PRF_ROOMFLAGS,
                          PRF_NOHASSLE, PRF_AUTOINVIS, PRF_AUTOEXIT, ENDBIT);
  } else {
    PLR_FLAGS(ch).SetBit(PLR_AUTH);
    GET_IDNUM(ch) = MAX(playerDB.find_open_id(), atol(row[0]) + 1);
  }
  mysql_free_result(res);

  init_char(ch);
  init_char_strings(ch);

  return ch;
}

char_data *PCIndex::CreateChar(char_data *ch)
{
  if (entry_cnt >= entry_size)
    resize_table();

  entry info;

  if (strlen(GET_CHAR_NAME(ch)) >= MAX_NAME_LENGTH) {
    log("--Fatal error: Could not fit name into player index..\n"
        "             : Inrease MAX_NAME_LENGTH");
    shutdown();
  }

  // if this is the first character, make him president
  if (entry_cnt < 1) {
    log_vfprintf("%s promoted to %s, by virtue of first-come, first-serve.",
        status_ratings[LVL_MAX], GET_CHAR_NAME(ch));
    
    // TODO: this is a duplicate of the other president code, can they be consolidated?

    for (int i = 0; i < 3; i++)
      GET_COND(ch, i) = (char) -1;

    GET_KARMA(ch) = 0;
    GET_LEVEL(ch) = LVL_MAX;
    GET_REP(ch) = 0;
    GET_NOT(ch) = 0;
    GET_TKE(ch) = 0;

    PLR_FLAGS(ch).RemoveBit(PLR_NEWBIE);
    PLR_FLAGS(ch).RemoveBit(PLR_AUTH);
    PRF_FLAGS(ch).SetBits(PRF_HOLYLIGHT, PRF_CONNLOG, PRF_ROOMFLAGS,
                          PRF_NOHASSLE, PRF_AUTOINVIS, PRF_AUTOEXIT, ENDBIT);
  } else {
    PLR_FLAGS(ch).SetBit(PLR_AUTH);
  }
  init_char(ch);
  init_char_strings(ch);

  strcpy(info.name, GET_CHAR_NAME(ch));
  info.id = find_open_id();
  info.level = GET_LEVEL(ch);
  info.flags = (PLR_FLAGGED(ch, PLR_NODELETE) ? NODELETE : 0);
  info.active_data = ch;
  info.instance_cnt = 1;

  // sync IDs
  GET_IDNUM(ch) = info.id;

  // update the logon time
  time(&info.last);

  // insert the new info in the correct place
  sorted_insert(&info);

  if (!needs_save)
    needs_save = true;

  Save();

  return ch;
}

char_data *PCIndex::LoadChar(const char *name, bool logon)
{
  // load the character data
  char_data *ch = Mem->GetCh();

  ch->player_specials = new player_special_data;
  memset(ch->player_specials, 0, sizeof(player_special_data));

  load_char(name, ch, logon);

  return ch;
}

bool PCIndex::SaveChar(char_data *ch, vnum_t loadroom)
{
  if (IS_NPC(ch))
    return false;

  bool ret = save_char(ch, loadroom);

  return ret;
}

void PCIndex::reset()
{
  if (tab != NULL) {
    delete [] tab;
    tab = NULL;
  }

  entry_cnt = entry_size = 0;
  needs_save = false;
}

bool PCIndex::load()
{
  // Sorry, PCIndex::load, but with the SQL DB in place you'll never work again.
  /*
  File index(INDEX_FILENAME, "r");

  if (!index.IsOpen()) {
    log("--WARNING: Could not open player index file %s...\n"
        "         : Starting game with NO PLAYERS!!!\n",
        INDEX_FILENAME);

    FILE *test = fopen(INDEX_FILENAME, "r");
    if (test) {
      log("_could_ open it normally, tho");
      fclose(test);
    }

    return false;
  }

  reset();

  char line[512];
  int line_num = 0;
  int idx = 0;

  entry_cnt = count_entries(&index);
  resize_table();

  line_num += index.GetLine(line, 512, FALSE);
  while (!index.EoF() && strcmp(line, "END")) {
    entry *ptr = tab+idx;
    char temp[512];           // for paranoia

    if (sscanf(line, " %ld %d %u %lu %s ",
               &ptr->id, &ptr->level, &ptr->flags, &ptr->last, temp) != 5) {
      log("--Fatal error: syntax error in player index file, line %d",
          line_num);
      shutdown();
    }

    strncpy(ptr->name, temp, MAX_NAME_LENGTH);
    *(ptr->name+MAX_NAME_LENGTH-1) = '\0';

    ptr->active_data = NULL;

    line_num += index.GetLine(line, 512, FALSE);
    idx++;
  }

  if (entry_cnt != idx)
    entry_cnt = idx;

  log("--Successfully loaded %d entries from the player index file.",
      entry_cnt);

  sort_by_id();
 // clear_by_time();
  return true;
  */
  return false;
}

int  PCIndex::count_entries(File *index)
{
  char line[512];
  int count = 0;

  index->GetLine(line, 512, FALSE);
  while (!index->EoF() && strcmp(line, "END")) {
    count++;
    index->GetLine(line, 512, FALSE);
  }

  index->Rewind();

  return count;
}

void PCIndex::sort_by_id()
{
  qsort(tab, entry_cnt, sizeof(entry), entry_compare);
}

void PCIndex::resize_table(int empty_slots)
{
  entry_size = entry_cnt + empty_slots;
  entry *new_tab = new entry[entry_size];

  if (tab) {
    for (int i = 0; i < entry_cnt; i++)
      new_tab[i] = tab[i];

    // fill empty slots with zeroes
    memset(new_tab+entry_cnt, 0, sizeof(entry)*(entry_size-entry_cnt));

    // delete the old table
    delete [] tab;
  } else {
    // fill entire table with zeros
    memset(new_tab, 0, sizeof(entry)*entry_size);
  }

  // finally, update the pointer
  tab = new_tab;
}

void PCIndex::sorted_insert(const entry *info)
{
  int i;

  // whee!! linear!

  // first, find the correct index
  for (i = 0; i < entry_cnt; i++)
    if (tab[i].id > info->id)
      break;

  // create an empty space for the new entry
  for (int j = entry_cnt; j > i; j--)
    tab[j] = tab[j-1];

  tab[i] = *info;

  log_vfprintf("--Inserting %s's (id#%d) entry into position %d",
      info->name, info->id, i);

  // update entry_cnt, and we're done
  entry_cnt++;
  needs_save = true;
}

DBIndex::vnum_t PCIndex::find_open_id()
{
  if (entry_cnt < 1)
    return 1;

  /* this won't work right now since id#s are used to store
     owners of foci, etc.  So once there's a universal id# invalidator,
     this should be put in
  */
  /*
  for (int i = 1; i < entry_cnt; i++)
    if (tab[i].id > (tab[i-1].id+1))
      return (tab[i-1].id+1);
  */

  return (tab[entry_cnt-1].id+1);
}

bool does_player_exist(char *name)
{
  char buf[MAX_STRING_LENGTH];
  if (!name || !*name)
    return FALSE;
  sprintf(buf, "SELECT * FROM pfiles WHERE Name='%s';", name);
  if (mysql_wrapper(mysql, buf))
    return FALSE;
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row && mysql_field_count(mysql)) {
    mysql_free_result(res);
    return FALSE;
  }
  mysql_free_result(res);
  return TRUE;
}

bool does_player_exist(long id)
{
  char buf[MAX_STRING_LENGTH];
  sprintf(buf, "SELECT * FROM pfiles WHERE idnum=%ld;", id);
  mysql_wrapper(mysql, buf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row && mysql_field_count(mysql)) {
    mysql_free_result(res);
    return FALSE;
  }
  mysql_free_result(res);
  return TRUE;
}

vnum_t get_player_id(char *name)
{
  char buf[MAX_STRING_LENGTH];
  sprintf(buf, "SELECT idnum FROM pfiles WHERE name=\"%s\";", name);
  mysql_wrapper(mysql, buf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row && mysql_field_count(mysql)) {
    mysql_free_result(res);
    return -1;
  }
  long x = atol(row[0]);
  mysql_free_result(res);
  return x;
}

char *get_player_name(vnum_t id)
{
  char buf[MAX_STRING_LENGTH];
  sprintf(buf, "SELECT Name FROM pfiles WHERE idnum=%ld;", id);
  mysql_wrapper(mysql, buf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row && mysql_field_count(mysql)) {
    mysql_free_result(res);
    return NULL;
  }
  char *x = str_dup(row[0]);
  mysql_free_result(res);
  return x;
}

void DeleteChar(long idx)
{
  sprintf(buf, "DELETE FROM pfiles_immortdata WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_chargendata WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_magic WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_drugdata WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_drugs WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_skills WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_adeptpowers WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_metamagic WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_quests WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_spirits WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_bioware WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_cyberware WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_inv WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_worn WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_spells WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_memory WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_alias WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "DELETE FROM pfiles_memory WHERE remembered=%ld", idx);
  mysql_wrapper(mysql, buf);
  sprintf(buf, "UPDATE pfiles SET Name='deleted', Password='', NoDelete=TRUE WHERE idnum=%ld", idx); 
//  sprintf(buf, "DELETE FROM pfiles WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
}

void idle_delete() 
{
  int deleted = 0;
  char buf[MAX_STRING_LENGTH];
  
  MYSQL *mysqlextra = mysql_init(NULL);
  if (!mysql_real_connect(mysqlextra, mysql_host, mysql_user, mysql_password, mysql_db, 0, NULL, 0)) {
    log("IDLEDELETE- Could not open extra socket, aborting");
    return;
  }
  sprintf(buf, "SELECT idnum FROM pfiles WHERE lastd <= %ld AND nodelete = 0 ORDER BY lastd ASC;", time(0) - (SECS_PER_REAL_DAY * 50));
  mysql_wrapper(mysqlextra, buf);
  MYSQL_RES *res = mysql_use_result(mysqlextra);
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(res))) {
    DeleteChar(atol(row[0]));
    deleted++;    
  }
  mysql_free_result(res);
  mysql_close(mysqlextra);
  sprintf(buf, "IDLEDELETE- %d Deleted.", deleted);
  log(buf);
}


