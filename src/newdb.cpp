// file: newdb.cpp
// author: Andrew Hynek
// contents: implementation of the index classes

#include <cmath>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <vector>
#include <utility>
#include <algorithm>
#include <mysql/mysql.h>

#include "structs.hpp"
#include "newdb.hpp"
#include "db.hpp"
#include "comm.hpp"       // for shutdown()
#include "file.hpp"
#include "utils.hpp"
#include "memory.hpp"
#include "handler.hpp"
#include "vtable.hpp"
#include "constants.hpp"
#include "interpreter.hpp" // for alias
#include "config.hpp"
#include "bullet_pants.hpp"
#include "ignore_system.hpp"
#include "lifestyles.hpp"
#include "chipjacks.hpp"
#include "quest.hpp"
#include "vehicles.hpp"
#include "newmail.hpp"
#include "player_exdescs.hpp"
#include "gmcp.hpp"

/* mysql_config.h must be filled out with your own connection info. */
/* For obvious reasons, DO NOT ADD THIS FILE TO SOURCE CONTROL AFTER CUSTOMIZATION. */
#include "mysql_config.hpp"

char buf4[MAX_STRING_LENGTH];

extern void kill_ems(char *);
extern void init_char_sql(struct char_data *ch, const char *origin);
static const char *const INDEX_FILENAME = "etc/pfiles/index";
extern void add_phone_to_list(struct obj_data *);
extern Playergroup *loaded_playergroups;
extern int get_cyberware_install_cost(struct obj_data *ware);

extern void save_bullet_pants(struct char_data *ch);
extern void load_bullet_pants(struct char_data *ch);
extern void handle_weapon_attachments(struct obj_data *obj);
extern int get_deprecated_cybereye_essence_cost(struct char_data *ch, struct obj_data *obj);
extern void price_cyber(struct obj_data *obj);
extern int get_skill_price(struct char_data *ch, int i);
extern int get_max_skill_for_char(struct char_data *ch, int skill, int type);

void auto_repair_obj(struct obj_data *obj, idnum_t owner);

void save_adept_powers_to_db(struct char_data *player);
void save_spells_to_db(struct char_data *player);
void save_quests_to_db(struct char_data *player);
void save_inventory_to_db(struct char_data *player);
void save_worn_equipment_to_db(struct char_data *player);
void save_metamagic_to_db(struct char_data *player);
void save_elementals_to_db(struct char_data *player);
void save_echoes_to_db(struct char_data *player);
void save_pc_memory_to_db(struct char_data *player);
void save_drug_data_to_db(struct char_data *player);
void save_skills_to_db(struct char_data *player);
void save_aliases_to_db(struct char_data *player);
void save_bioware_to_db(struct char_data *player);
void save_cyberware_to_db(struct char_data *player);
void fix_ghoul_index(struct char_data *ch);
void fix_character_essence_after_cybereye_migration(struct char_data *ch);
void fix_character_essence_after_expert_driver_change(struct char_data *ch);
void recalculate_character_magic_rating(struct char_data *ch);
void save_player_faction_info(struct char_data *player);
void load_player_faction_info(struct char_data *player);

SPECIAL(weapon_dominator);
SPECIAL(pocket_sec);

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
#ifdef QUERY_DEBUG
  snprintf(buf, sizeof(buf), "Executing query: %s", query);
  log(buf);
#endif

  int result = mysql_query(mysql, query);

  if (mysql_errno(mysql)) {
    snprintf(buf, sizeof(buf), "MYSQLERROR: %s", mysql_error(mysql));
    log(buf);
    snprintf(buf, sizeof(buf), "Offending query: %s", query);
    log(buf);
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

  GET_LOADROOM(ch) = RM_NEWBIE_LOADROOM;
  GET_WAS_IN(ch) = NULL;

  ch->player.time.birth = time(0);
  ch->player.time.played = 0;
  ch->player.time.lastdisc = time(0);

  ch->player.weight = (int)gen_size(GET_RACE(ch), 0, 3, GET_PRONOUNS(ch));
  ch->player.height = (int)gen_size(GET_RACE(ch), 1, 3, GET_PRONOUNS(ch));

  ch->points.max_mental = 1000;
  ch->points.max_physical = 1000;
  ch->points.mental = GET_MAX_MENTAL(ch);
  ch->points.physical = GET_MAX_PHYSICAL(ch);
  GET_BALLISTIC(ch) = GET_TOTALBAL(ch) = GET_INNATE_BALLISTIC(ch) = GET_IMPACT(ch) = GET_TOTALIMP(ch) = GET_INNATE_IMPACT(ch) = 0;
  ch->points.sustained[0] = 0;
  ch->points.sustained[1] = 0;
  ch->points.grade = 0;

  if (access_level(ch, LVL_VICEPRES))
  {
    GET_COND(ch, COND_FULL) = -1;
    GET_COND(ch, COND_THIRST) = -1;
    GET_COND(ch, COND_DRUNK) = -1;
  } else
  {
    GET_COND(ch, COND_FULL) = 24;
    GET_COND(ch, COND_THIRST) = 24;
    GET_COND(ch, COND_DRUNK) = 0;
  }

  if (!access_level(ch, LVL_VICEPRES))
    for (i = MIN_SKILLS; i < MAX_SKILLS; i++) {
      set_character_skill(ch, i, 0, FALSE);
    }
  else
    for (i = MIN_SKILLS; i < MAX_SKILLS; i++) {
      set_character_skill(ch, i, MAX_SKILL_LEVEL_FOR_IMMS, FALSE);
    }

  ch->char_specials.saved.affected_by.Clear();

  GET_IGNORE_DATA(ch) = new IgnoreData(ch);
}

static void init_char_strings(char_data *ch)
{
  char temp[256];
  DELETE_ARRAY_IF_EXTANT(ch->player.physical_text.keywords);

  size_t len = strlen(GET_CHAR_NAME(ch)) + 1; // + strlen(race) + 2;
  ch->player.physical_text.keywords = new char[len];

  strcpy(ch->player.physical_text.keywords, GET_CHAR_NAME(ch));
  *(ch->player.physical_text.keywords) = LOWER(*ch->player.physical_text.keywords);

  delete [] ch->player.physical_text.name;
  snprintf(temp, sizeof(temp), "an average %s %s", genders_decap[(int)GET_PRONOUNS(ch)], pc_race_types_decap[(int)GET_RACE(ch)]);
  ch->player.physical_text.name = str_dup(temp);

  delete [] ch->player.physical_text.room_desc;
  snprintf(temp, sizeof(temp), "A %s %s voice", genders_decap[(int)GET_PRONOUNS(ch)], pc_race_types_decap[(int)GET_RACE(ch)]);
  ch->player.physical_text.room_desc = str_dup(temp);

  delete [] ch->player.physical_text.look_desc;
  ch->player.physical_text.look_desc = str_dup("A fairly nondescript person.\n");

  delete [] ch->player.background;
  ch->player.background = str_dup("(no background provided yet)\n");

  delete [] ch->char_specials.arrive;
  ch->char_specials.arrive = str_dup("arrives from");

  delete [] ch->char_specials.leave;
  ch->char_specials.leave = str_dup("leaves");

  set_title(ch, "", FALSE);
  set_pretitle(ch, NULL);

  if (GET_RACE(ch) >= MINIMUM_VALID_PLAYER_RACE && GET_RACE(ch) <= MAXIMUM_VALID_PLAYER_RACE) {
    set_whotitle(ch, pc_race_types_for_wholist[(int)GET_RACE(ch)]);
  } else {
    mudlog("No valid race found at set_whotitle in class.cc", NULL, LOG_SYSLOG, TRUE);
    set_whotitle(ch, "New");
  }

  DELETE_ARRAY_IF_EXTANT(ch->player.prompt);
  ch->player.prompt = str_dup("< @pP @mM > ");
  DELETE_ARRAY_IF_EXTANT(ch->player.matrixprompt);
  ch->player.matrixprompt = str_dup("< @pP @mM > ");
}

// Eventual TODO: https://dev.mysql.com/doc/refman/5.7/en/mysql-real-escape-string-quote.html
char *prepare_quotes(char *dest, const char *str, size_t size_of_dest, bool include_newline_processing, bool is_like)
{
  if (!str)
    return NULL;
  char *temp = &dest[0];
  while (*str) {
    if ((unsigned long) (temp - dest) >= size_of_dest - 2) {
      // Die to protect memory / database.
      char error_buf[MAX_STRING_LENGTH];
      snprintf(error_buf, sizeof(error_buf), "prepare_quotes(dest, '%s', %lu) would overflow dest buf", str, size_of_dest);
      terminate_mud_process_with_message(error_buf, ERROR_ARRAY_OUT_OF_BOUNDS);
    }
    // Special case handling: Newlines to \r\n.
    if (include_newline_processing) {
      if (*str == '\r' || *str == '\n') {
        *temp++ = '\\';
        *temp++ = *str++ == '\r' ? 'r' : 'n';
        continue;
      }
    }

    if (*str == '\'' || *str == '`' || *str == '"' || *str == '\\') {
      *temp++ = '\\';
    }
    // Special case handling: % has special meaning in LIKE statements.
    if (is_like) {
      if (*str == '%' || *str == '?' || *str == '.' || *str == '*' || *str == '+' || *str == '(' || *str == ')' || *str == '[' || *str == ']') {
        *temp++ = '\\';
        *temp++ = '\\';
      }
    }
    *temp++ = *str++;
  }
  *temp = '\0';
  dest[size_of_dest - 1] = '\0';
  return dest;
}

/* Some initializations for characters, including initial skills */
void do_start(struct char_data * ch, bool wipe_skills)
{
  void advance_level(struct char_data * ch);

  GET_LEVEL(ch) = 1;
  GET_KARMA(ch) = 0;
  GET_REP(ch) = 0;
  GET_NOT(ch) = 0;
  GET_TKE(ch) = 0;

  GET_INVIS_LEV(ch) = 0;

  ch->points.max_physical = 1000;
  ch->points.max_mental = 1000;

  ch->char_specials.saved.left_handed = (!number(0, 9) ? 1 : 0);

  GET_PHYSICAL(ch) = GET_MAX_PHYSICAL(ch);
  GET_MENTAL(ch) = GET_MAX_MENTAL(ch);

  GET_COND(ch, COND_THIRST) = FOOD_DRINK_MAX;
  GET_COND(ch, COND_FULL) = FOOD_DRINK_MAX;
  GET_COND(ch, COND_DRUNK) = 0;
  GET_LOADROOM(ch) = RM_NEWBIE_LOADROOM;

  // Wipe out their flags, if they have any.
  AFF_FLAGS(ch).Clear();
  PLR_FLAGS(ch).Clear();
  PRF_FLAGS(ch).Clear();

  // Set the appropriate flags.
  PLR_FLAGS(ch).SetBits(PLR_NEWBIE, PLR_NOT_YET_AUTHED, PLR_RECEIVED_CYBEREYE_ESSENCE_DELTA, ENDBIT);
  PRF_FLAGS(ch).SetBits(PRF_AUTOEXIT, PRF_LONGEXITS, PRF_SEE_TIPS, PRF_AUTOSTAND, ENDBIT);

  // PLR_FLAGS(ch).SetBit(PLR_NOT_YET_AUTHED);
  ch->player.time.played = 0;
  ch->player.time.lastdisc = time(0);

  // Clear all their skills except for English.
  if (wipe_skills) {
    for (int i = MIN_SKILLS; i < MAX_SKILLS; i++) {
      if (i == SKILL_ENGLISH)
        set_character_skill(ch, i, STARTING_LANGUAGE_SKILL_LEVEL, FALSE);
      else
        set_character_skill(ch, i, 0, FALSE);
    }
  }

  SendGMCPCharVitals(ch); 

  // For morts, this just saves them and prints a message about their new level.
  advance_level(ch);
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

  snprintf(buf, sizeof(buf), "%s [%s] advanced to %s.",
          GET_CHAR_NAME(ch), ch->desc && *ch->desc->host ? ch->desc->host : "<no host>", status_ratings[(int)GET_LEVEL(ch)]);
  mudlog(buf, ch, LOG_MISCLOG, TRUE);
}

bool load_char(const char *name, char_data *ch, bool logon, int pc_load_origin)
{
  init_char(ch);
  for (int i = MIN_SKILLS; i < MAX_SKILLS; i++)
    ch->char_specials.saved.skills[i][SKILLARRAY_LEARNED_VALUE] = 0;
  ch->char_specials.carry_weight = 0;
  ch->char_specials.carry_items = 0;
  GET_BALLISTIC(ch) = GET_TOTALBAL(ch) = GET_INNATE_BALLISTIC(ch) = GET_IMPACT(ch) = GET_TOTALIMP(ch) = GET_INNATE_IMPACT(ch) = 0;
  ch->points.init_dice = 0;
  ch->points.init_roll = 0;
  ch->points.sustained[1] = 0;
  ch->points.sustained[0] = 0;
  GET_LAST_TELL(ch) = NOBODY;
  MYSQL_RES *res;
  MYSQL_ROW row;

  snprintf(buf, sizeof(buf), "SELECT * FROM pfiles WHERE Name='%s';", prepare_quotes(buf3, name, sizeof(buf3) / sizeof(buf3[0])));
  mysql_wrapper(mysql, buf);
  if (!(res = mysql_use_result(mysql))) {
    log_vfprintf("Refusing to load character '%s' (quoted from '%s'): No result on select-star query.", buf3, name);
    return FALSE;
  }
  row = mysql_fetch_row(res);
  if (!row && mysql_field_count(mysql)) {
    log_vfprintf("Refusing to load character '%s' (quoted from '%s'): No row found on select-star query.", buf3, name);
    mysql_free_result(res);
    return FALSE;
  }
  GET_IDNUM(ch) = atol(row[0]);
  ch->player.char_name = str_dup(row[1]);
  strcpy(GET_PASSWD(ch), row[2]);
  GET_RACE(ch) = atoi(row[3]);
  GET_PRONOUNS(ch) = atoi(row[4]);
  GET_LEVEL(ch) = atoi(row[5]);
  AFF_FLAGS(ch).FromString(row[6]);
  PLR_FLAGS(ch).FromString(row[7]);
  PRF_FLAGS(ch).FromString(row[8]);

  // Unset the cyberdoc flag on load.
  PRF_FLAGS(ch).RemoveBit(PRF_TOUCH_ME_DADDY);

  // Remove the PK flag if that system is disabled.
#ifndef ENABLE_PK
  PRF_FLAGS(ch).RemoveBit(PRF_PKER);
#endif

  // Clear temporary bits.
  AFF_FLAGS(ch).RemoveBit(AFF_TEMPORARY_MARK_DO_NOT_SET_PERSISTENTLY);

  // Warn if we have the chargen flag, then unset it (this is an error case)
  if (PLR_FLAGGED(ch, PLR_IN_CHARGEN)) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: %s still had the chargen bit on load! Removing it.", GET_CHAR_NAME(ch));
    PLR_FLAGS(ch).RemoveBit(PLR_IN_CHARGEN);
  }

  ch->player.physical_text.room_desc = str_dup(get_string_after_color_code_removal(row[9], ch));
  ch->player.background = str_dup(cleanup_invalid_color_codes(row[10]));
  ch->player.physical_text.keywords = str_dup(cleanup_invalid_color_codes(row[11]));
  ch->player.physical_text.name = str_dup(cleanup_invalid_color_codes(row[12]));
  ch->player.physical_text.look_desc = str_dup(cleanup_invalid_color_codes(row[13]));

  ch->player.matrix_text.keywords = str_dup(cleanup_invalid_color_codes(row[14]));
  ch->player.matrix_text.name = str_dup(cleanup_invalid_color_codes(row[15]));
  ch->player.matrix_text.room_desc = str_dup(cleanup_invalid_color_codes(row[16]));
  ch->player.matrix_text.look_desc = str_dup(cleanup_invalid_color_codes(row[17]));

  ch->player.astral_text.keywords = str_dup(cleanup_invalid_color_codes(row[18]));
  if (!str_str(ch->player.astral_text.keywords, GET_CHAR_NAME(ch))) {
    char new_keywords[MAX_INPUT_LENGTH + 100];
    snprintf(new_keywords, sizeof(new_keywords), "%s %s", ch->player.astral_text.keywords, GET_CHAR_NAME(ch));
    delete [] ch->player.astral_text.keywords;
    ch->player.astral_text.keywords = str_dup(new_keywords);
  }
  ch->player.astral_text.name = str_dup(cleanup_invalid_color_codes(row[19]));
  ch->player.astral_text.room_desc = str_dup(cleanup_invalid_color_codes(row[20]));
  ch->player.astral_text.look_desc = str_dup(cleanup_invalid_color_codes(row[21]));
  ch->char_specials.leave = str_dup(cleanup_invalid_color_codes(row[22]));
  ch->char_specials.arrive = str_dup(cleanup_invalid_color_codes(row[23]));
  GET_TITLE(ch) = str_dup(cleanup_invalid_color_codes(row[24]));
  GET_PRETITLE(ch) = str_dup(cleanup_invalid_color_codes(row[25]));
  GET_WHOTITLE(ch) = str_dup(cleanup_invalid_color_codes(row[26]));
  ch->player.prompt = str_dup(cleanup_invalid_color_codes(row[27]));
  ch->player.matrixprompt = str_dup(cleanup_invalid_color_codes(row[28]));
  ch->player.host = str_dup(cleanup_invalid_color_codes(row[29]));
  GET_REAL_BOD(ch) = atoi(row[30]);
  GET_REAL_QUI(ch) = atoi(row[31]);
  GET_REAL_STR(ch) = atoi(row[32]);
  GET_REAL_CHA(ch) = atoi(row[33]);
  GET_REAL_INT(ch) = atoi(row[34]);
  GET_REAL_WIL(ch) = atoi(row[35]);
  GET_REAL_ESS(ch) = atoi(row[36]);
  GET_ESSHOLE(ch) = atoi(row[37]);
  GET_INDEX(ch) = atoi(row[38]);
  GET_HIGHEST_INDEX(ch) = atoi(row[39]);
  ch->real_abils.hacking_pool_max = atoi(row[40]);
  ch->real_abils.body_pool = atoi(row[41]);
  ch->real_abils.defense_pool = atoi(row[42]);
  GET_REAL_REA(ch) = (GET_REAL_QUI(ch) + GET_REAL_INT(ch)) / 2;
  ch->aff_abils = ch->real_abils;
  GET_HEIGHT(ch) = atoi(row[43]);
  GET_WEIGHT(ch) = atoi(row[44]);
  GET_TRADITION(ch) = atoi(row[45]);
  GET_NUYEN_RAW(ch) = atol(row[46]);
  GET_BANK_RAW(ch) = atol(row[47]);
  GET_KARMA(ch) = atol(row[48]);
  GET_REP(ch) = atol(row[49]);
  GET_NOT(ch) = atol(row[50]);
  GET_TKE(ch) = atol(row[51]);

  GET_PHYSICAL(ch) = atoi(row[54]);
  GET_PHYSICAL_LOSS(ch) = atoi(row[55]);
  GET_MENTAL(ch) = atoi(row[56]);
  GET_MENTAL_LOSS(ch) = atoi(row[57]);
  ch->char_specials.saved.left_handed = atoi(row[59]);
  GET_LANGUAGE(ch) = atoi(row[60]);
  ch->player_specials->saved.wimp_level = atoi(row[61]);
  ch->player_specials->saved.load_room = atoi(row[62]);
  ch->player_specials->saved.last_in = atoi(row[63]);

  ch->player.time.lastdisc = atol(row[64]);
  ch->player.time.birth = atol(row[65]);
  ch->player.time.played = atol(row[66]);
  GET_COND(ch, COND_FULL) = atoi(row[67]);
  GET_COND(ch, COND_THIRST) = atoi(row[68]);
  GET_COND(ch, COND_DRUNK) = atoi(row[69]);
  SHOTS_FIRED(ch) = atoi(row[70]);
  SHOTS_TRIGGERED(ch) = atoi(row[71]);
  GET_COST_BREAKUP(ch) = atoi(row[73]);
  GET_AVAIL_OFFSET(ch) = atoi(row[74]);
  ch->player_specials->saved.last_veh = atol(row[75]);
  GET_SYSTEM_POINTS(ch) = atoi(row[76]);
  GET_CONGREGATION_BONUS(ch) = atoi(row[77]);
  // note that pgroup is 78
  SETTABLE_CHAR_COLOR_HIGHLIGHT(ch) = str_dup(cleanup_invalid_color_codes(row[79]));
  SETTABLE_EMAIL(ch) = str_dup(cleanup_invalid_color_codes(row[80]));
  GET_CHAR_MULTIPLIER(ch) = atoi(row[81]);
  const char *lifestyle_string = str_dup(cleanup_invalid_color_codes(row[82]));
  set_exdesc_max(ch, atoi(row[83]), FALSE);
  GET_OTAKU_PATH(ch) = atoi(row[84]);
  GET_GRADE(ch) = atoi(row[85]);
  mysql_free_result(res);

  // Update lifestyle information.
  calculate_best_lifestyle(ch);
  set_lifestyle_string(ch, lifestyle_string);
  delete [] lifestyle_string;

  if (GET_LEVEL(ch) <= 1) {
    for (int i = 0; i <= WIL; i++) {
      bool exceeding_limits = FALSE;
      if (i == BOD && (GET_REAL_BOD(ch)) > get_attr_max(ch, i)) {
        exceeding_limits = TRUE;
      }

      else if (GET_REAL_ATT(ch, i) > get_attr_max(ch, i)) {
        exceeding_limits = TRUE;
      }

      if (exceeding_limits){
        snprintf(buf, sizeof(buf), "^YSomehow, %s's %s is %d (racial max is %d.) Resetting to racial max.^n",
                 GET_CHAR_NAME(ch),
                 attributes[i],
                 GET_REAL_ATT(ch, i),
                 get_attr_max(ch, i));
        mudlog(buf, ch, LOG_SYSLOG, TRUE);

        GET_REAL_ATT(ch, i) = get_attr_max(ch, i);
      }
    }
  }

  if (GET_LEVEL(ch) > LVL_MORTAL) {
    snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_immortdata WHERE idnum=%ld;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    res = mysql_use_result(mysql);
    if ((row = mysql_fetch_row(res))) {
      ch->player_specials->saved.invis_level = atoi(row[1]);
      ch->player_specials->saved.incog_level = atoi(row[2]);
      ch->player_specials->saved.zonenum = atoi(row[3]);
      POOFIN(ch) = str_dup((strcmp(row[4], "(null)") == 0 ? DEFAULT_POOFIN_STRING : row[4]));
      POOFOUT(ch) = str_dup((strcmp(row[5], "(null)") == 0 ? DEFAULT_POOFOUT_STRING : row[5]));
    }
    mysql_free_result(res);
  }

  // Pull drug info.
  {
    snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_drugs WHERE idnum=%ld;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    res = mysql_use_result(mysql);
    int x = 0;
    while ((row = mysql_fetch_row(res))) {
      x = atoi(row[1]);
      GET_DRUG_ADDICT(ch, x) = atoi(row[2]);
      GET_DRUG_LIFETIME_DOSES(ch, x) = atoi(row[3]);
      GET_DRUG_ADDICTION_EDGE(ch, x) = atoi(row[4]);
      GET_DRUG_LAST_FIX(ch, x) = atol(row[5]);
      GET_DRUG_ADDICTION_TICK_COUNTER(ch, x) = atoi(row[6]);
      GET_DRUG_TOLERANCE_LEVEL(ch, x) = atoi(row[7]);
      GET_DRUG_LAST_WITHDRAWAL_TICK(ch, x) = atoi(row[8]);
      GET_DRUG_DURATION(ch, x) = atoi(row[9]);
      GET_DRUG_DOSE(ch, x) = atoi(row[10]);
      GET_DRUG_STAGE(ch, x) = atoi(row[11]);
    }
    mysql_free_result(res);
  }

  if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
    snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_chargendata WHERE idnum=%ld;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    res = mysql_use_result(mysql);
    if ((row = mysql_fetch_row(res))) {
      GET_ATT_POINTS(ch) = atoi(row[1]);
      GET_SKILL_POINTS(ch) = atoi(row[2]);
      GET_FORCE_POINTS(ch) = atoi(row[3]);
      GET_RESTRING_POINTS(ch) = atoi(row[4]);
      GET_ARCHETYPAL_MODE(ch) = (bool) atoi(row[5]);
      GET_ARCHETYPAL_TYPE(ch) = atoi(row[6]);
      GET_PRESTIGE_ALT_ID(ch) = atol(row[7]);
      GET_CHANNEL_POINTS(ch) = atoi(row[8]);
    }
    mysql_free_result(res);
  }

  if (IS_OTAKU(ch) && GET_GRADE(ch) > 0) {
    // Otaku echoes loading
    snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_echoes WHERE idnum=%ld;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    res = mysql_use_result(mysql);
    while ((row = mysql_fetch_row(res))) {
      SET_ECHO(ch, atoi(row[1]), atoi(row[2]));
    }
    mysql_free_result(res);
  }

  if (GET_TRADITION(ch) != TRAD_MUNDANE) {
    snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_magic WHERE idnum=%ld;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    res = mysql_use_result(mysql);
    if ((row = mysql_fetch_row(res))) {
      GET_SETTABLE_REAL_MAG(ch) = atoi(row[1]);
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
      snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_adeptpowers WHERE idnum=%ld;", GET_IDNUM(ch));
      mysql_wrapper(mysql, buf);
      res = mysql_use_result(mysql);
      while ((row = mysql_fetch_row(res))) {
        SET_POWER_TOTAL(ch, atoi(row[1]), atoi(row[2]));
      }
      mysql_free_result(res);
    }
    if (GET_GRADE(ch) > 0) {
      // Metamagic loading
      snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_metamagic WHERE idnum=%ld;", GET_IDNUM(ch));
      mysql_wrapper(mysql, buf);
      res = mysql_use_result(mysql);
      while ((row = mysql_fetch_row(res))) {
        SET_METAMAGIC(ch, atoi(row[1]), atoi(row[2]));
      }
      mysql_free_result(res);
    }
    if (GET_TRADITION(ch) != TRAD_ADEPT) {
      snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_spells WHERE idnum=%ld ORDER BY Category DESC, Name DESC;", GET_IDNUM(ch));
      mysql_wrapper(mysql, buf);
      res = mysql_use_result(mysql);
      while ((row = mysql_fetch_row(res))) {
        spell_data *spell = new spell_data;
        // spell->name = str_dup(row[1]);
        spell->type = atoi(row[2]);
        spell->subtype = atoi(row[3]);
        spell->force = atoi(row[4]);

        // Compose a new name. This column could probably be removed from the DB altogether.
        spell->name = str_dup(compose_spell_name(spell->type, spell->subtype));

        spell->next = ch->spells;
        ch->spells = spell;
      }
      mysql_free_result(res);
    }

    if (GET_TRADITION(ch) == TRAD_HERMETIC) {
      snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_spirits WHERE idnum=%ld;", GET_IDNUM(ch));
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

  snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_skills WHERE idnum=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  res = mysql_use_result(mysql);
  while ((row = mysql_fetch_row(res)))
    set_character_skill(ch, atoi(row[1]), atoi(row[2]), FALSE, FALSE);
  mysql_free_result(res);

  snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_alias WHERE idnum=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  res = mysql_use_result(mysql);
  while ((row = mysql_fetch_row(res))) {
    alias *a = new alias;
    a->command = str_dup(cleanup_invalid_color_codes(row[1]));
    a->replacement = str_dup(cleanup_invalid_color_codes(row[2]));

    if (strchr(a->replacement, ALIAS_SEP_CHAR) ||
        strchr(a->replacement, ALIAS_VAR_CHAR))
      a->type = ALIAS_COMPLEX;
    else
      a->type = ALIAS_SIMPLE;

    a->next = GET_ALIASES(ch);
    GET_ALIASES(ch) = a;
  }
  mysql_free_result(res);

  snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_memory WHERE idnum=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  res = mysql_use_result(mysql);
  while ((row = mysql_fetch_row(res))) {
    remem *a = new remem;
    a->idnum = atol(row[1]);
    a->mem = str_dup(cleanup_invalid_color_codes(row[2]));
    a->next = GET_PLAYER_MEMORY(ch);
    GET_PLAYER_MEMORY(ch) = a;
  }
  mysql_free_result(res);

  load_player_faction_info(ch);

  snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_quests WHERE idnum=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  res = mysql_use_result(mysql);

  int cquest_idx = 0;
  while ((row = mysql_fetch_row(res))) {
    vnum_t vnum = atoi(row[2]);

    GET_LQUEST(ch, atoi(row[1])) = vnum;

    if (atoi(row[3]))
      GET_CQUEST(ch, cquest_idx++) = vnum; 
  }
  mysql_free_result(res);

  {
    #define PFILES_CYBERWARE_PC_IDNUM row[0]
    #define PFILES_CYBERWARE_VNUM row[1]
    #define PFILES_CYBERWARE_COST row[2]
    #define PFILES_CYBERWARE_RESTRING row[3]
    #define PFILES_CYBERWARE_PHOTO row[4]
    #define PFILES_CYBERWARE_LEVEL row[5 + NUM_OBJ_VALUES]
    #define PFILES_CYBERWARE_POSI row[6 + NUM_OBJ_VALUES]
    #define PFILES_CYBERWARE_GRAFFITI row[7 + NUM_OBJ_VALUES]
    #define PFILES_CYBERWARE_OBJ_IDNUM row[8 + NUM_OBJ_VALUES]

    struct obj_data *obj = NULL;
    int vnum = 0, last_inside = 0, inside = 0;
    std::vector<nested_obj> contained_obj;
    struct nested_obj contained_obj_entry;
    snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_cyberware WHERE idnum=%ld ORDER BY posi DESC;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    snprintf(buf3, sizeof(buf3), "pfiles_cyberware load for %s (%ld)", GET_CHAR_NAME(ch), GET_IDNUM(ch));
    res = mysql_use_result(mysql);
    while ((row = mysql_fetch_row(res))) {
      vnum = atol(PFILES_CYBERWARE_VNUM);
      if (vnum > 0 && (obj = read_object(vnum, VIRTUAL, OBJ_LOAD_REASON_FROM_DB, pc_load_origin, GET_IDNUM(ch)))) {
        GET_OBJ_COST(obj) = atoi(PFILES_CYBERWARE_COST);
        if (*PFILES_CYBERWARE_RESTRING)
          obj->restring = str_dup(cleanup_invalid_color_codes(PFILES_CYBERWARE_RESTRING));
        if (*PFILES_CYBERWARE_PHOTO)
          obj->photo = str_dup(cleanup_invalid_color_codes(PFILES_CYBERWARE_PHOTO));
        for (int x = 0; x < NUM_OBJ_VALUES; x++)
          GET_OBJ_VAL(obj, x) = atoi(row[x + 5]);
        if (GET_CYBERWARE_TYPE(obj) == CYB_PHONE && GET_OBJ_VAL(obj, 7))
          add_phone_to_list(obj);
        /* TODO: Not sure what this code used to do. It checked obj val 2 == 4 and obj val 7 as boolean, then disabled the ware if both were true.
        else if (GET_OBJ_VAL(obj, 2) == 4 && GET_OBJ_VAL(obj, 7))
          GET_CYBERWARE_IS_DISABLED(obj) = 1;
        */
        inside = atoi(PFILES_CYBERWARE_LEVEL);

        if (PFILES_CYBERWARE_GRAFFITI && *PFILES_CYBERWARE_GRAFFITI)
          obj->graffiti = str_dup(cleanup_invalid_color_codes(PFILES_CYBERWARE_GRAFFITI));

        GET_OBJ_IDNUM(obj) = strtoul(PFILES_CYBERWARE_OBJ_IDNUM, NULL, 0);

        auto_repair_obj(obj, GET_IDNUM(ch));

        // Since we're now reading rows from the db in reverse order, in order to fix the stupid reordering on
        // every binary execution, the previous algorithm did not work, as it relied on getting the container obj
        // first and place subsequent objects in it. Since we're now getting it last, and more importantly we get
        // objects at deeper nesting level in between our list, we save all pointers to objects along with their
        // nesting level in a vector container and once we found the next container (inside < last_inside) we
        // move the objects with higher nesting level to container and proceed. This works for any nesting depth.
        if (inside > 0 || (inside == 0 && inside < last_inside)) {
          //Found our container?
          if (inside < last_inside) {
            if (inside == 0)
              obj_to_cyberware(obj, ch);

            auto it = std::find_if(contained_obj.begin(), contained_obj.end(), find_level(inside+1));
            while (it != contained_obj.end()) {
              obj_to_obj(it->obj, obj);
              it = contained_obj.erase(it);
            }

            if (inside > 0) {
              contained_obj_entry.level = inside;
              contained_obj_entry.obj = obj;
              contained_obj.push_back(contained_obj_entry);
            }
          }
          else {
            contained_obj_entry.level = inside;
            contained_obj_entry.obj = obj;
            contained_obj.push_back(contained_obj_entry);
          }
          last_inside = inside;
        } else
          obj_to_cyberware(obj, ch);

        last_inside = inside;
      }
    }
    //Failsafe. If something went wrong and we still have objects stored in the vector, dump them in inventory.
    if (!contained_obj.empty()) {
      for (auto it : contained_obj)
        obj_to_char(it.obj, ch);

      contained_obj.clear();
      snprintf(buf2, sizeof(buf2), "Load error:  Objects in ware containers found with invalid containers for Char ID: %ld. Dumped in inventory.", GET_IDNUM(ch));
      mudlog(buf2, NULL, LOG_SYSLOG, TRUE);
    }
    mysql_free_result(res);
  }

  {
    #define PFILES_BIOWARE_PC_IDNUM row[0]
    #define PFILES_BIOWARE_VNUM row[1]
    #define PFILES_BIOWARE_COST row[2]
    #define PFILES_BIOWARE_RESTRING row[3 + NUM_OBJ_VALUES]
    #define PFILES_BIOWARE_GRAFFITI row[4 + NUM_OBJ_VALUES]
    #define PFILES_BIOWARE_OBJ_IDNUM row[5 + NUM_OBJ_VALUES]

    struct obj_data *obj;
    int vnum = 0;
    snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_bioware WHERE idnum=%ld ORDER BY Vnum;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    res = mysql_use_result(mysql);
    while ((row = mysql_fetch_row(res))) {
      vnum = atol(PFILES_BIOWARE_VNUM);
      if (vnum > 0 && (obj = read_object(vnum, VIRTUAL, OBJ_LOAD_REASON_FROM_DB, pc_load_origin, GET_IDNUM(ch)))) {
        GET_OBJ_COST(obj) = atoi(PFILES_BIOWARE_COST);
        for (int x = 0, y = 3; x < NUM_OBJ_VALUES; x++, y++) {
          GET_OBJ_VAL(obj, x) = atoi(row[y]);
        }
        if (PFILES_BIOWARE_RESTRING && *PFILES_BIOWARE_RESTRING)
          obj->restring = str_dup(cleanup_invalid_color_codes(PFILES_BIOWARE_RESTRING));
        if (PFILES_BIOWARE_GRAFFITI && *PFILES_BIOWARE_GRAFFITI)
          obj->graffiti = str_dup(cleanup_invalid_color_codes(PFILES_BIOWARE_GRAFFITI));
        
        GET_OBJ_IDNUM(obj) = strtoul(PFILES_BIOWARE_OBJ_IDNUM, NULL, 0);
        auto_repair_obj(obj, GET_IDNUM(ch));
        obj_to_bioware(obj, ch);
      }
    }
    mysql_free_result(res);
  }

  {
    #define PFILES_WORN_PC_IDNUM row[0]
    #define PFILES_WORN_VNUM row[1]
    #define PFILES_WORN_COST row[2]
    #define PFILES_WORN_RESTRING row[3]
    #define PFILES_WORN_PHOTO row[4]
    #define PFILES_WORN_INSIDE row[5 + NUM_OBJ_VALUES]
    #define PFILES_WORN_POSITION row[6 + NUM_OBJ_VALUES]
    #define PFILES_WORN_TIMER row[7 + NUM_OBJ_VALUES]
    #define PFILES_WORN_EXFLAGS row[8 + NUM_OBJ_VALUES]
    #define PFILES_WORN_ATTEMPT row[9 + NUM_OBJ_VALUES]
    #define PFILES_WORN_COND row[10 + NUM_OBJ_VALUES]
    #define PFILES_WORN_POSI row[11 + NUM_OBJ_VALUES]
    #define PFILES_WORN_GRAFFITI row[12 + NUM_OBJ_VALUES]
    #define PFILES_WORN_OBJ_IDNUM row[13 + NUM_OBJ_VALUES]

    struct obj_data *obj = NULL;
    long vnum;
    int inside = 0, last_inside = 0;
    std::vector<nested_obj> contained_obj;
    struct nested_obj contained_obj_entry;
    snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_worn WHERE idnum=%ld ORDER BY posi DESC;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    snprintf(buf3, sizeof(buf3), "pfiles_worn load for %s (%ld)", GET_CHAR_NAME(ch), GET_IDNUM(ch));
    res = mysql_use_result(mysql);
    while ((row = mysql_fetch_row(res))) {
      vnum = atol(PFILES_WORN_VNUM);
      if (vnum > 0 && (obj = read_object(vnum, VIRTUAL, OBJ_LOAD_REASON_FROM_DB, pc_load_origin, GET_IDNUM(ch)))) {
        GET_OBJ_COST(obj) = atoi(PFILES_WORN_COST);
        if (*PFILES_WORN_RESTRING)
          obj->restring = str_dup(cleanup_invalid_color_codes(PFILES_WORN_RESTRING));
        if (*PFILES_WORN_PHOTO)
          obj->photo = str_dup(cleanup_invalid_color_codes(PFILES_WORN_PHOTO));
        for (int x = 0, y = 5; x < NUM_OBJ_VALUES; x++, y++)
          GET_OBJ_VAL(obj, x) = atoi(row[y]);
        if (GET_OBJ_TYPE(obj) == ITEM_PHONE && GET_ITEM_PHONE_SWITCHED_ON(obj))
          add_phone_to_list(obj);
        if (GET_OBJ_TYPE(obj) == ITEM_FOCUS && GET_FOCUS_TYPE(obj) == FOCI_SUSTAINED)
          GET_FOCUS_ACTIVATED(obj) = 0;
        if (GET_OBJ_TYPE(obj) == ITEM_FOCUS && GET_FOCUS_ACTIVATED(obj))
          GET_FOCI(ch)++;
        if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
          handle_weapon_attachments(obj);
        inside = atoi(PFILES_WORN_INSIDE);
        GET_OBJ_TIMER(obj) = atoi(PFILES_WORN_TIMER);

        // row 20: extra flags. We want to retain the proto's flags but also persist anti-cheat flags and other necessary ones.
        Bitfield temp_extra_flags;
        temp_extra_flags.FromString(PFILES_WORN_EXFLAGS);
        if (temp_extra_flags.IsSet(ITEM_EXTRA_WIZLOAD))
          GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_WIZLOAD);
        if (temp_extra_flags.IsSet(ITEM_EXTRA_KEPT))
          GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_KEPT);
        if (temp_extra_flags.IsSet(ITEM_EXTRA_CONCEALED_IN_EQ))
          GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_CONCEALED_IN_EQ);

        GET_OBJ_ATTEMPT(obj) = atoi(PFILES_WORN_ATTEMPT);
        GET_OBJ_CONDITION(obj) = atoi(PFILES_WORN_COND);

        if (PFILES_WORN_GRAFFITI && *PFILES_WORN_GRAFFITI)
          obj->graffiti = str_dup(cleanup_invalid_color_codes(PFILES_WORN_GRAFFITI));

        GET_OBJ_IDNUM(obj) = strtoul(PFILES_WORN_OBJ_IDNUM, NULL, 0);

        auto_repair_obj(obj, GET_IDNUM(ch));

        // Since we're now reading rows from the db in reverse order, in order to fix the stupid reordering on
        // every binary execution, the previous algorithm did not work, as it relied on getting the container obj
        // first and place subsequent objects in it. Since we're now getting it last, and more importantly we get
        // objects at deeper nesting level in between our list, we save all pointers to objects along with their
        // nesting level in a vector container and once we found the next container (inside < last_inside) we
        // move the objects with higher nesting level to container and proceed. This works for any nesting depth.
        if (inside > 0 || (inside == 0 && inside < last_inside)) {
          //Found our container?
          if (inside < last_inside) {
            if (inside == 0)
              equip_char(ch, obj, atoi(PFILES_WORN_POSITION));

            auto it = std::find_if(contained_obj.begin(), contained_obj.end(), find_level(inside+1));
            while (it != contained_obj.end()) {
              obj_to_obj(it->obj, obj);
              it = contained_obj.erase(it);
            }

            if (inside > 0) {
              contained_obj_entry.level = inside;
              contained_obj_entry.obj = obj;
              contained_obj.push_back(contained_obj_entry);
            }
          }
          else {
            contained_obj_entry.level = inside;
            contained_obj_entry.obj = obj;
            contained_obj.push_back(contained_obj_entry);
          }
          last_inside = inside;
        } else
           equip_char(ch, obj, atoi(PFILES_WORN_POSITION));

        last_inside = inside;
      }
    }
    //Failsafe. If something went wrong and we still have objects stored in the vector, dump them in inventory.
    if (!contained_obj.empty()) {
      for (auto it : contained_obj)
        obj_to_char(it.obj, ch);

      contained_obj.clear();
      snprintf(buf2, sizeof(buf2), "Load error: Worn objects found with invalid containers for Char ID: %ld. Dumped in inventory.", GET_IDNUM(ch));
      mudlog(buf2, NULL, LOG_SYSLOG, TRUE);
    }
    mysql_free_result(res);
  }

  {
    #define PFILES_INV_PC_IDNUM row[0]
    #define PFILES_INV_VNUM row[1]
    #define PFILES_INV_COST row[2]
    #define PFILES_INV_RESTRING row[3]
    #define PFILES_INV_PHOTO row[4]
    // Value0 - Value14, aka +15
    #define PFILES_INV_INSIDE row[5 + NUM_OBJ_VALUES]
    #define PFILES_INV_TIMER row[6 + NUM_OBJ_VALUES]
    #define PFILES_INV_EXFLAGS row[7 + NUM_OBJ_VALUES]
    #define PFILES_INV_ATTEMPT row[8 + NUM_OBJ_VALUES]
    #define PFILES_INV_COND row[9 + NUM_OBJ_VALUES]
    #define PFILES_INV_POSI row[10 + NUM_OBJ_VALUES]
    #define PFILES_INV_GRAFFITI row[11 + NUM_OBJ_VALUES]
    #define PFILES_INV_OBJ_IDNUM row[12 + NUM_OBJ_VALUES]

    struct obj_data *obj = NULL;
    vnum_t vnum = 0;
    int inside = 0, last_inside = 0;
    std::vector<nested_obj> contained_obj;
    struct nested_obj contained_obj_entry;
    snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_inv WHERE idnum=%ld ORDER BY posi DESC;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    snprintf(buf3, sizeof(buf3), "pfiles_inv load for %s (%ld)", GET_CHAR_NAME(ch), GET_IDNUM(ch));
    res = mysql_use_result(mysql);
    while ((row = mysql_fetch_row(res))) {
      vnum = atol(PFILES_INV_VNUM);
      if (vnum > 0 && (obj = read_object(vnum, VIRTUAL, OBJ_LOAD_REASON_FROM_DB, pc_load_origin, GET_IDNUM(ch)))) {
        GET_OBJ_COST(obj) = atoi(PFILES_INV_COST);
        if (*PFILES_INV_RESTRING)
          obj->restring = str_dup(cleanup_invalid_color_codes(PFILES_INV_RESTRING));
        if (*PFILES_INV_PHOTO)
          obj->photo = str_dup(cleanup_invalid_color_codes(PFILES_INV_PHOTO));
        for (int x = 0, y = 5; x < NUM_OBJ_VALUES; x++, y++)
          GET_OBJ_VAL(obj, x) = atoi(row[y]);

        switch (GET_OBJ_TYPE(obj)) {
          case ITEM_PHONE:
            if (GET_ITEM_PHONE_SWITCHED_ON(obj))
              add_phone_to_list(obj);
            // TODO: What was the purpose of the broken if check to set the phone's value 9 to 1?
            break;
          case ITEM_FOCUS:
            if (GET_FOCUS_TYPE(obj) == FOCI_SUSTAINED)
              GET_FOCUS_ACTIVATED(obj) = 0;
            else if (GET_FOCUS_ACTIVATED(obj))
              GET_FOCI(ch)++;
            break;
          case ITEM_WEAPON:
            handle_weapon_attachments(obj);
            break;
          case ITEM_GUN_AMMO:
            // Process weight.
            GET_OBJ_WEIGHT(obj) = get_ammo_weight(GET_AMMOBOX_WEAPON(obj), GET_AMMOBOX_TYPE(obj), GET_AMMOBOX_QUANTITY(obj), ch);
            break;
          case ITEM_VEHCONTAINER:
            // We did some hacky shit and force-saved the weight of the container to value 11. Pull it back out.
            GET_OBJ_WEIGHT(obj) = GET_VEHCONTAINER_WEIGHT(obj);
            break;
        }
        // This is badly named and at first reading it seems like it holds a vnum to parent container
        // which made the algorithm below harder to read but it is in fact nesting level.
        // I am not refactoring it to nesting_level though as it then wouldn't match the database column.
        // This serves as a reminder to do so if I push a db update and for others to figure out easier what it actually is. -- Nodens
        inside = atoi(PFILES_INV_INSIDE);
        GET_OBJ_TIMER(obj) = atoi(PFILES_INV_TIMER);

        // row 19: extra flags. We want to retain the proto's flags but also persist anti-cheat flags and other necessary ones.
        Bitfield temp_extra_flags;
        temp_extra_flags.FromString(PFILES_INV_EXFLAGS);
        if (temp_extra_flags.IsSet(ITEM_EXTRA_WIZLOAD))
          GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_WIZLOAD);
        if (temp_extra_flags.IsSet(ITEM_EXTRA_KEPT))
          GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_KEPT);
        if (temp_extra_flags.IsSet(ITEM_EXTRA_CONCEALED_IN_EQ))
          GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_CONCEALED_IN_EQ);

        GET_OBJ_ATTEMPT(obj) = atoi(PFILES_INV_ATTEMPT);
        GET_OBJ_CONDITION(obj) = atoi(PFILES_INV_COND);

        if (PFILES_INV_GRAFFITI && *PFILES_INV_GRAFFITI)
          obj->graffiti = str_dup(cleanup_invalid_color_codes(PFILES_INV_GRAFFITI));

        GET_OBJ_IDNUM(obj) = strtoul(PFILES_INV_OBJ_IDNUM, NULL, 0);

        auto_repair_obj(obj, GET_IDNUM(ch));

        // Since we're now reading rows from the db in reverse order, in order to fix the stupid reordering on
        // every binary execution, the previous algorithm did not work, as it relied on getting the container obj
        // first and place subsequent objects in it. Since we're now getting it last, and more importantly we get
        // objects at deeper nesting level in between our list, we save all pointers to objects along with their
        // nesting level in a vector container and once we found the next container (inside < last_inside) we
        // move the objects with higher nesting level to container and proceed. This works for any nesting depth.
        if (inside > 0 || (inside == 0 && inside < last_inside)) {
          //Found our container?
          if (inside < last_inside) {
            if (inside == 0)
              obj_to_char(obj, ch);

            auto it = std::find_if(contained_obj.begin(), contained_obj.end(), find_level(inside+1));
            while (it != contained_obj.end()) {
              obj_to_obj(it->obj, obj);
              it = contained_obj.erase(it);
            }

            if (inside > 0) {
              contained_obj_entry.level = inside;
              contained_obj_entry.obj = obj;
              contained_obj.push_back(contained_obj_entry);
            }
          }
          else {
            contained_obj_entry.level = inside;
            contained_obj_entry.obj = obj;
            contained_obj.push_back(contained_obj_entry);
          }
          last_inside = inside;
        } else
          obj_to_char(obj, ch);

        last_inside = inside;
      } else {
        snprintf(buf, sizeof(buf), "SYSERR: Could not load object %ld when restoring %s (%ld)!", vnum, GET_CHAR_NAME(ch), GET_IDNUM(ch));
        mudlog(buf, ch, LOG_SYSLOG, TRUE);
      }
    }
    //Failsafe. If something went wrong and we still have objects stored in the vector, dump them in inventory.
    if (!contained_obj.empty()) {
      for (auto it : contained_obj)
        obj_to_char(it.obj, ch);

      contained_obj.clear();
      snprintf(buf2, sizeof(buf2), "Load error: Inventory objects found with invalid containers for Char ID: %ld. Dumped in inventory.", GET_IDNUM(ch));
      mudlog(buf2, NULL, LOG_SYSLOG, TRUE);
    }
    mysql_free_result(res);
  }

  // Load bullet pants.
  load_bullet_pants(ch);

  // Load pgroup membership data.
  snprintf(buf, sizeof(buf), "SELECT * FROM pfiles_playergroups WHERE idnum=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  res = mysql_use_result(mysql);
  if (res && (row = mysql_fetch_row(res))) {
    long pgroup_idnum = atol(row[1]);
    GET_PGROUP_MEMBER_DATA(ch) = new Pgroup_data();
    GET_PGROUP_MEMBER_DATA(ch)->rank = atoi(row[2]);
    GET_PGROUP_MEMBER_DATA(ch)->privileges.FromString(row[3]);

    // You MUST free the result before using this call, otherwise it breaks.
    mysql_free_result(res);
    GET_PGROUP(ch) = Playergroup::find_pgroup(pgroup_idnum);
  } else {
    mysql_free_result(res);
    GET_PGROUP_MEMBER_DATA(ch) = NULL;
  }

  // Load pgroup invitation data.
  snprintf(buf, sizeof(buf), "SELECT * FROM `playergroup_invitations` WHERE `idnum`=%ld;", GET_IDNUM(ch));
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

  // Load their ignore data (the structure was already initialized in init_char().)
  GET_IGNORE_DATA(ch)->load_from_db();

  // Load their exdescs.
  load_exdescs_from_db(ch);

  STOP_WORKING(ch);
  AFF_FLAGS(ch).RemoveBits(AFF_MANNING, AFF_RIG, AFF_PILOT, AFF_BANISH, AFF_FEAR, AFF_STABILIZE, AFF_SPELLINVIS, AFF_SPELLIMPINVIS, AFF_DETOX, AFF_RESISTPAIN, AFF_TRACKING, AFF_TRACKED, AFF_PRONE, ENDBIT);
  PLR_FLAGS(ch).RemoveBits(PLR_REMOTE, PLR_SWITCHED, PLR_MATRIX, PLR_PROJECT, PLR_EDITING, PLR_WRITING, PLR_PERCEIVE, PLR_VISA, ENDBIT);

  initialize_chipjack_and_memory_for_character(ch);

  // Self-repair their gear. Don't worry about contents- it's recursive.
  for (struct obj_data *obj = ch->carrying; obj; obj = obj->next_content) {
    auto_repair_obj(obj, GET_IDNUM(ch));
  }

  for (int i = 0; i < NUM_WEARS; i++) {
    if (GET_EQ(ch, i))
      auto_repair_obj(GET_EQ(ch, i), GET_IDNUM(ch));
  }

  set_natural_vision_for_race(ch);
  affect_total(ch);

  long time_since_last_disconnect = time(0) - ch->player.time.lastdisc;

  if (time_since_last_disconnect >= SECS_PER_REAL_HOUR) {
    GET_PHYSICAL(ch) = GET_MAX_PHYSICAL(ch);
    GET_MENTAL(ch) = GET_MAX_MENTAL(ch);
    if (AFF_FLAGS(ch).IsSet(AFF_HEALED))
      AFF_FLAGS(ch).RemoveBit(AFF_HEALED);
  }
  if ( !IS_SENATOR(ch) && !PLR_FLAGGED(ch, PLR_PAID_FOR_ROLLS) )
    PRF_FLAGS(ch).RemoveBit(PRF_ROLLS);

  // If you logged out in a cab or have been away more than 2 hours, snap back to home.
  if ((time_since_last_disconnect  >= SECS_PER_REAL_HOUR * 2) ||
      (GET_LAST_IN(ch) > 599 && GET_LAST_IN(ch) < 670)) {
    GET_LAST_IN(ch) = GET_LOADROOM(ch);
    GET_PHYSICAL(ch) = GET_MAX_PHYSICAL(ch);
    GET_MENTAL(ch) = GET_MAX_MENTAL(ch);
  }

  // initialization for imms
  if (GET_LEVEL(ch) >= LVL_BUILDER) {
    GET_COND(ch, COND_FULL) = -1;
    GET_COND(ch, COND_THIRST) = -1;
    GET_COND(ch, COND_DRUNK) = -1;
  } else {
    // Clamp skills to maximums and refund them.
    bool need_save = FALSE;

    for (int skill_idx = 0; skill_idx < MAX_SKILLS; skill_idx++) {
      int max_skill_level = get_max_skill_for_char(ch, skill_idx, IS_SENATOR(ch) ? GODLY : ADVANCED);

      if (GET_RAW_SKILL(ch, skill_idx) > LEARNED_LEVEL) {
        // Error case, we shouldn't be here with someone with staff skills.
        mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Got a staff-grade character to the mortal stanza of resetting skills: %s's %s = %d",
                        GET_CHAR_NAME(ch),
                        skills[skill_idx].name,
                        GET_RAW_SKILL(ch, skill_idx));
        break;
      }

      if (GET_RAW_SKILL(ch, skill_idx) > max_skill_level) {
        mudlog_vfprintf(ch, LOG_SYSLOG, "%s's %s is %d, which exceeds their max of %d. Capping and refunding...",
                        GET_CHAR_NAME(ch),
                        skills[skill_idx].name,
                        GET_RAW_SKILL(ch, skill_idx),
                        max_skill_level);
        
        int old_too_high_skill_level = GET_RAW_SKILL(ch, skill_idx);

        // Count up the refund.
        int skill_karma_price = 0;
        int skill_nuyen_price = 0;
        for (int idx = 0; idx < (old_too_high_skill_level - max_skill_level); idx++) {
          GET_RAW_SKILL(ch, skill_idx) = max_skill_level + idx;
          skill_karma_price += get_skill_price(ch, skill_idx);
          skill_nuyen_price += GET_RAW_SKILL(ch, skill_idx) * 5000;
        }

        // Cap the skill.
        set_character_skill(ch, skill_idx, max_skill_level, FALSE, FALSE);

        // Issue the refund.
        GET_KARMA(ch) += skill_karma_price * 100;
        GET_NUYEN_RAW(ch) += skill_nuyen_price;
        GET_SYSTEM_POINTS(ch) += 5;
        mudlog_vfprintf(ch, LOG_SYSLOG, "Refunded %s %d karma, %d nuyen; added 5 syspoints (%d -> %d).",
                        GET_CHAR_NAME(ch),
                        skill_karma_price,
                        skill_nuyen_price,
                        GET_SYSTEM_POINTS(ch) - 5,
                        GET_SYSTEM_POINTS(ch));

        snprintf(buf, sizeof(buf), "Due to balance changes, your skill in %s has been automatically set to the new cap of ^c%d^n. You have been refunded ^c%d^n karma and ^c%d^n nuyen to compensate for your training costs, and have received ^c5^n syspoints as a bonus.^n\r\n",
                 skills[skill_idx].name,
                 max_skill_level,
                 skill_karma_price,
                 skill_nuyen_price);
        raw_store_mail(GET_IDNUM(ch), 0, "(OOC System Message)", buf);

        need_save = TRUE;
      }
    }

    if (need_save) {
      playerDB.SaveChar(ch);
    }
  }

  return true;
}

#define SAVE_IF_DIRTY_BIT_SET(dirty_bit_accessor, save_method) { \
  if (dirty_bit_accessor(player)) {                              \
    save_method(player);                                         \
    dirty_bit_accessor(player) = FALSE;                          \
  }                                                              \
}

static bool save_char(char_data *player, DBIndex::vnum_t loadroom, bool fromCopyover = FALSE)
{
  PERF_PROF_SCOPE(pr_, __func__);
  char buf[MAX_STRING_LENGTH*4], buf2[MAX_STRING_LENGTH*4], buf3[MAX_STRING_LENGTH*4];
  int i;
  long inveh = 0;
  struct obj_data *char_eq[NUM_WEARS];
  struct obj_data *temp, *next_obj;

  if (IS_NPC(player))
    return false;

  // Write a single log message each time we save. This will help trace how often we're doing this and if there are excessive calls.
  log_vfprintf("Saving %s to DB.", GET_CHAR_NAME(player));

  // Check all their stuff over. If it's ALL empty, this is a bugged character and should not be saved.
  {
    bool has_any_item = player->cyberware || player->bioware || player->carrying;
    
    for (int wear_idx = 0; !has_any_item && wear_idx < NUM_WEARS; wear_idx++) {
      if (GET_EQ(player, wear_idx)) {
        has_any_item = TRUE;
        break;
      }
    }

#ifdef TRY_TO_SAVE_GEAR_FROM_THAT_ONE_BUG_THAT_I_THINK_I_FIXED_ALREADY_ANYWAYS
    if (!has_any_item && !PLR_FLAGGED(player, PLR_JUST_DIED)) {
      // Friendly notice: If you're reading this and going "oh hey, I could use this to drop all my stuff and quit to dupe my gear!" etc, that's code abuse and will result in a purge and ban. As you can see, it's logged ;)
      mudlog_vfprintf(player, LOG_SYSLOG, "SYSERR: Got completely nude character to save_char(). Refusing to save them under the assumption that this is a stripped char from that untraced bug that sometimes destroys gear.\r\n");
      return false;
    }
#endif
  }

  MYSQL_RES *res;
  MYSQL_ROW row;
  snprintf(buf, sizeof(buf), "SELECT `idnum` FROM pfiles WHERE `idnum`=%ld;", GET_IDNUM(player));
  if (!mysql_wrapper(mysql, buf)) {
    res = mysql_use_result(mysql);
    row = mysql_fetch_row(res);
    if (!row && mysql_field_count(mysql)) {
      mysql_free_result(res);
      init_char_sql(player, "save_char");
    } else {
      mysql_free_result(res);
    }
  }

  // Set their last disconnect time as right now.
  player->player.time.lastdisc = time(0);

  /* Remove their worn equipment to inventory. */
  for (i = 0; i < NUM_WEARS; i++) {
    if (player->equipment[i])
      char_eq[i] = unequip_char(player, i, FALSE, FALSE);
    else
      char_eq[i] = NULL;
  }

  /* Remove their cyberware to inventory. */
  for (temp = player->cyberware; temp; temp = next_obj) {
    next_obj = temp->next_content;
    obj_from_cyberware(temp, FALSE);
    obj_to_char(temp, player);
  }

  /* Remove their bioware to inventory. */
  for (temp = player->bioware; temp; temp = next_obj) {
    next_obj = temp->next_content;
    obj_from_bioware(temp, FALSE);
    obj_to_char(temp, player);
  }

  /* Strip off any spell affects. */
  for (struct sustain_data *sust = GET_SUSTAINED(player); sust; sust = sust->next)
    if (!sust->is_caster_record)
      spell_modify(player, sust, FALSE);

  /**************************************************/
  /* Default their loadroom if it wasn't provided specially. */
  if (loadroom == NOWHERE)
    loadroom = GET_LOADROOM(player);
  // If that didn't fix it, set it to Dante's to prevent failed SQL saves.
  if (loadroom == NOWHERE)
    loadroom = RM_ENTRANCE_TO_DANTES;

  /* Figure out what room to load them in. */
  if (player->in_room) {
    if (GET_ROOM_VNUM(player->in_room) <= 1) {
      // If their current room is invalid for save/load:
      if (player->was_in_room) {
        if (GET_ROOM_VNUM(player->was_in_room) <= 1) {
          // Their was_in_room is invalid; put them at Dante's.
          snprintf(buf, sizeof(buf), "SYSERR: save_char(): %s is at %ld and has was_in_room (world array index) %ld.",
                  GET_CHAR_NAME(player), GET_ROOM_VNUM(player->in_room), GET_ROOM_VNUM(player->was_in_room));
          mudlog(buf, NULL, LOG_SYSLOG, TRUE);
          GET_LAST_IN(player) = RM_ENTRANCE_TO_DANTES;
        } else {
          // Their was_in_room is valid, so put them there.
          GET_LAST_IN(player) = GET_ROOM_VNUM(player->was_in_room);
        }
      } else {
        // They have no was_in_room, so put them at Dante's.
        GET_LAST_IN(player) = RM_ENTRANCE_TO_DANTES;
      }
    } else {
      // Their in_room is valid, so put them there.
      GET_LAST_IN(player) = GET_ROOM_VNUM(player->in_room);
    }
  }

  // Ensure that they're not snapping back to a staff room.
  {
    rnum_t last_in_rnum = real_room(GET_LAST_IN(player));
    if (last_in_rnum >= 0 && ROOM_FLAGGED(&world[last_in_rnum], ROOM_STAFF_ONLY) && !access_level(player, LVL_BUILDER)) {
      // Snap to Dante's instead.
      GET_LAST_IN(player) = RM_ENTRANCE_TO_DANTES;
    }
  }

  /* Figure out their vehicle-- they can only load in it if they own it.  Unless we're calling from copyover.*/
  if (player->in_veh && (fromCopyover || player->in_veh->owner == GET_IDNUM(player)))
    inveh = player->in_veh->idnum;

  /* Figure out their pgroup num-- we only want to access this if the group is valid. */
  long pgroup_num = 0;
  if (GET_PGROUP_MEMBER_DATA(player) && GET_PGROUP(player))
    pgroup_num = GET_PGROUP(player)->get_idnum();
  
  // Remove their temp-load flag if it's set (but set it again after)
  bool is_temp_load = PLR_FLAGGED(player, PLR_IS_TEMPORARILY_LOADED);
  PLR_FLAGS(player).RemoveBit(PLR_IS_TEMPORARILY_LOADED);

  /* Compose the initial giant update. */
  snprintf(buf, sizeof(buf), "UPDATE pfiles SET AffFlags='%s', PlrFlags='%s', PrfFlags='%s', Bod=%d, "\
               "Qui=%d, Str=%d, Cha=%d, Intel=%d, Wil=%d, EssenceTotal=%d, EssenceHole=%d, "\
               "BiowareIndex=%d, HighestIndex=%d, Pool_MaxHacking=%d, Pool_Body=%d, "\
               "Pool_Dodge=%d, Cash=%ld, Bank=%ld, Karma=%ld, Rep=%ld, Notor=%ld, TKE=%ld, "\
               "Dead=%d, Physical=%d, PhysicalLoss=%d, Mental=%d, MentalLoss=%d, "\
               "PermBodLoss=%d, WimpLevel=%d, Loadroom=%ld, LastRoom=%ld, LastD=%ld, Hunger=%d, Thirst=%d, Drunk=%d, " \
               "ShotsFired='%d', ShotsTriggered='%d', Tradition=%d, pgroup='%ld', "\
               "Inveh=%ld, `rank`=%d, gender=%d, SysPoints=%d, socialbonus=%d, email='%s', highlight='%s',"
               "multiplier=%d, lifestyle_string='%s', nodelete=%d WHERE idnum=%ld;",
               AFF_FLAGS(player).ToString(),
               PLR_FLAGS(player).ToString(),
               PRF_FLAGS(player).ToString(),
               GET_REAL_BOD(player),
               GET_REAL_QUI(player),
               GET_REAL_STR(player),
               GET_REAL_CHA(player),
               GET_REAL_INT(player),
               GET_REAL_WIL(player),
               GET_REAL_ESS(player),
               player->real_abils.esshole,
               GET_INDEX(player),
               player->real_abils.highestindex,
               GET_MAX_HACKING(player),
               GET_BODY_POOL(player),
               GET_DODGE(player),
               GET_NUYEN(player),
               GET_BANK(player),
               GET_KARMA(player),
               MAX(0, GET_REP(player)),
               MAX(0, GET_NOT(player)),
               MAX(0, GET_TKE(player)),
               PLR_FLAGGED(player, PLR_JUST_DIED),
               MAX(0, GET_PHYSICAL(player)),
               GET_PHYSICAL_LOSS(player),
               MAX(0, GET_MENTAL(player)),
               GET_MENTAL_LOSS(player), 0, GET_WIMP_LEV(player),
               GET_LOADROOM(player),
               GET_LAST_IN(player),
               time(0),
               GET_COND(player, COND_FULL),
               GET_COND(player, COND_THIRST),
               GET_COND(player, COND_DRUNK),
               SHOTS_FIRED(player),
               SHOTS_TRIGGERED(player),
               GET_TRADITION(player),
               pgroup_num,
               inveh,
               GET_LEVEL(player),
               GET_PRONOUNS(player),
               GET_SYSTEM_POINTS(player),
               MIN(GET_CONGREGATION_BONUS(player), MAX_CONGREGATION_BONUS),
               prepare_quotes(buf1, GET_EMAIL(player), sizeof(buf1) / sizeof(char)),
               prepare_quotes(buf2, GET_CHAR_COLOR_HIGHLIGHT(player), sizeof(buf2) / sizeof(char)),
               GET_CHAR_MULTIPLIER(player),
               prepare_quotes(buf3, get_lifestyle_string(player), sizeof(buf3) / sizeof(char)),
               PLR_FLAGGED(player, PLR_NODELETE) ? 1 : 0,
               GET_IDNUM(player));
  mysql_wrapper(mysql, buf);

  if (is_temp_load)
    PLR_FLAGS(player).SetBit(PLR_IS_TEMPORARILY_LOADED);

  /* Re-equip cyberware and bioware. */
  for (temp = player->carrying; temp; temp = next_obj) {
    next_obj = temp->next_content;
    if (GET_OBJ_TYPE(temp) == ITEM_CYBERWARE) {
      obj_from_char(temp);
      obj_to_cyberware(temp, player, FALSE);
    }
    if (GET_OBJ_TYPE(temp) == ITEM_BIOWARE) {
      obj_from_char(temp);
      obj_to_bioware(temp, player, FALSE);
    }
  }

  /* Re-apply spells. */
  for (struct sustain_data *sust = GET_SUSTAINED(player); sust; sust = sust->next)
    if (!sust->is_caster_record)
      spell_modify(player, sust, TRUE);

  /* Re-equip equipment. */
  for (i = 0; i < NUM_WEARS; i++) {
    if (char_eq[i])
      equip_char(player, char_eq[i], i, FALSE);
  }

  /* Re-calculate affects. */
  affect_total(player);

  /* Save chargen data.*/
  if (PLR_FLAGGED(player, PLR_NOT_YET_AUTHED)) {
    snprintf(buf, sizeof(buf), "UPDATE pfiles_chargendata SET AttPoints=%d, SkillPoints=%d, ForcePoints=%d, RestringPoints=%d WHERE idnum=%ld;",
                 GET_ATT_POINTS(player), GET_SKILL_POINTS(player), GET_FORCE_POINTS(player), GET_RESTRING_POINTS(player),
                 GET_IDNUM(player));
    mysql_wrapper(mysql, buf);
  }

  /* Save skills, if they've changed their skills since last save. */
  SAVE_IF_DIRTY_BIT_SET(GET_SKILL_DIRTY_BIT, save_skills_to_db);

  // SAVE_IF_DIRTY_BIT_SET(GET_DRUG_DIRTY_BIT, save_drug_data_to_db);
  save_drug_data_to_db(player);

  /* Save magic info. */
  if (GET_TRADITION(player) != TRAD_MUNDANE) {
    // The use of GET_SETTABLE_REAL_MAG() is intentional here, we don't want to cap the saved value to 20 magic.
    snprintf(buf, sizeof(buf), "UPDATE pfiles_magic SET Mag=%d, Pool_Casting=%d, Pool_SpellDefense=%d, Pool_Drain=%d, Pool_Reflecting=%d,"\
                 "UsedGrade=%d, ExtraPower=%d, PowerPoints=%d, Sig=%d, Masking=%d, Totem=%d, TotemSpirit=%d, Aspect=%d WHERE idnum=%ld;", GET_SETTABLE_REAL_MAG(player), GET_CASTING(player),
                 GET_SDEFENSE(player), GET_DRAIN(player), GET_REFLECT(player), GET_GRADE(player), player->points.extrapp,
                 GET_PP(player), GET_SIG(player), GET_MASKING(player), GET_TOTEM(player), GET_TOTEMSPIRIT(player), GET_ASPECT(player),
                 GET_IDNUM(player));
    mysql_wrapper(mysql, buf);

    /* Save various magic-related things. */
    SAVE_IF_DIRTY_BIT_SET(GET_ADEPT_POWER_DIRTY_BIT, save_adept_powers_to_db);
    SAVE_IF_DIRTY_BIT_SET(GET_SPELLS_DIRTY_BIT, save_spells_to_db);
    SAVE_IF_DIRTY_BIT_SET(GET_METAMAGIC_DIRTY_BIT, save_metamagic_to_db);
    SAVE_IF_DIRTY_BIT_SET(GET_ELEMENTALS_DIRTY_BIT, save_elementals_to_db);
  }

  /* Save otaku info. */
  if (IS_OTAKU(player)) {
    snprintf(buf, sizeof(buf), "UPDATE pfiles SET submersion_grade=%d WHERE idnum=%ld;", GET_GRADE(player), GET_IDNUM(player));
    mysql_wrapper(mysql, buf);

    /* Save various otaku-related things. */
    SAVE_IF_DIRTY_BIT_SET(GET_ECHOES_DIRTY_BIT, save_echoes_to_db);
  }

  /* Save data for quests the player has run. */
  SAVE_IF_DIRTY_BIT_SET(GET_QUEST_DIRTY_BIT, save_quests_to_db);

  /* Wipe out their memory, then re-write it. */
  SAVE_IF_DIRTY_BIT_SET(GET_MEMORY_DIRTY_BIT, save_pc_memory_to_db);

  SAVE_IF_DIRTY_BIT_SET(GET_ALIAS_DIRTY_BIT, save_aliases_to_db);

  // Save bioware and cyberware.
  save_bioware_to_db(player);
  save_cyberware_to_db(player);

  // Save gear.
  save_worn_equipment_to_db(player);
  save_inventory_to_db(player);

  save_player_faction_info(player);

  // Save bullet pants if they've changed.
  SAVE_IF_DIRTY_BIT_SET(GET_BULLETPANTS_DIRTY_BIT, save_bullet_pants);

  if (GET_LEVEL(player) > 1) {
    snprintf(buf, sizeof(buf),  "INSERT INTO pfiles_immortdata (idnum, InvisLevel, IncogLevel, Zonenumber) VALUES (%ld, %d, %d, %d)"
                  " ON DUPLICATE KEY UPDATE"
                  "   `InvisLevel` = VALUES(`InvisLevel`),"
                  "   `IncogLevel` = VALUES(`IncogLevel`),"
                  "   `Zonenumber` = VALUES(`Zonenumber`)",
                 GET_IDNUM(player), GET_INVIS_LEV(player), GET_INCOG_LEV(player), player->player_specials->saved.zonenum);
    mysql_wrapper(mysql, buf);
  }

  // Save pgroup membership data.
  if (GET_PGROUP_MEMBER_DATA(player) && GET_PGROUP(player)) {
    snprintf(buf, sizeof(buf), "INSERT INTO pfiles_playergroups (`idnum`, `group`, `Rank`, `Privileges`) VALUES ('%ld', '%ld', '%d', '%s')"
                 " ON DUPLICATE KEY UPDATE"
                 "   `group` = VALUES(`group`),"
                 "   `Rank` = VALUES(`Rank`),"
                 "   `Privileges` = VALUES(`Privileges`)",
                 GET_IDNUM(player),
                 GET_PGROUP(player)->get_idnum(),
                 GET_PGROUP_MEMBER_DATA(player)->rank,
                 GET_PGROUP_MEMBER_DATA(player)->privileges.ToString());
    mysql_wrapper(mysql, buf);
  } else {
    // Wipe any existing pgroup membership data.
    snprintf(buf, sizeof(buf), "DELETE FROM pfiles_playergroups WHERE idnum=%ld", GET_IDNUM(player));
    mysql_wrapper(mysql, buf);
  }

  // Exdescs are not saved here, and are instead only saved when updated.

  return TRUE;
}

#undef SAVE_IF_DIRTY_BIT_SET

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
    fclose(index);
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

// You motherfuckers better sanitize your queries that you pass to this, because it's going in RAW.
// Only used for idnums right now, so -1 is the error code.
idnum_t get_one_number_from_query(const char *query) {
  idnum_t value = -1;

  char buf[MAX_STRING_LENGTH];
  strcpy(buf, query);
  mysql_wrapper(mysql, buf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (row && mysql_field_count(mysql)) {
    value = atol(row[0]);
  }
  mysql_free_result(res);

  return value;
}

idnum_t get_highest_idnum_in_use() {
  char buf[MAX_STRING_LENGTH];
  idnum_t new_number = 0;

  const char *tables[] = {
    "pfiles_adeptpowers",
    "pfiles_alias",
    "pfiles_ammo",
    "pfiles_bioware",
    "pfiles_chargendata",
    "pfiles_cyberware",
    "pfiles_drugs",
    "pfiles_ignore",
    "pfiles_immortdata",
    "pfiles_inv",
    "pfiles_magic",
    "pfiles_memory",
    "pfiles_metamagic",
    "pfiles_named_tags",
    "pfiles_playergroups",
    "pfiles_quests",
    "pfiles_skills",
    "pfiles_spells",
    "pfiles_spirits",
    "pfiles_worn",
    "you-deleted-something-and-didn't-change-the-table-length-dumbass" // must be last for obvious reasons
  };

  #define NUM_IDNUM_TABLES 20

  // Get the pfiles idnum. This SHOULD be the only one we need.
  idnum_t highest_pfiles_idnum = get_one_number_from_query("SELECT idnum FROM pfiles ORDER BY idnum DESC LIMIT 1;");

  // Check for idnums in standard tables.
  for (int i = 0; i < NUM_IDNUM_TABLES; i++) {
    snprintf(buf, sizeof(buf), "SELECT idnum FROM %s ORDER BY idnum DESC LIMIT 1;", tables[i]);
    new_number = get_one_number_from_query(buf);
    if (highest_pfiles_idnum < new_number) {
      snprintf(buf3, sizeof(buf3), "^RSYSERR: SQL database corruption (pfiles idnum %ld lower than '%s' idnum %ld). Auto-correcting.^g", highest_pfiles_idnum, tables[i], new_number);
      mudlog(buf3, NULL, LOG_SYSLOG, TRUE);
      highest_pfiles_idnum = new_number;
    }
  }

  // Check for both senders and recipients in mail.
  new_number = get_one_number_from_query("SELECT sender_id FROM pfiles_mail ORDER BY idnum DESC LIMIT 1;");
  if (highest_pfiles_idnum < new_number) {
    snprintf(buf3, sizeof(buf3), "^RSYSERR: SQL database corruption (pfiles idnum %ld lower than pfiles_mail sender_id %ld). Auto-correcting.^g", highest_pfiles_idnum, new_number);
    mudlog(buf3, NULL, LOG_SYSLOG, TRUE);
    highest_pfiles_idnum = new_number;
  }

  new_number = get_one_number_from_query("SELECT recipient FROM pfiles_mail ORDER BY idnum DESC LIMIT 1;");
  if (highest_pfiles_idnum < new_number) {
    snprintf(buf3, sizeof(buf3), "^RSYSERR: SQL database corruption (pfiles idnum %ld lower than pfiles_mail recipient %ld). Auto-correcting.^g", highest_pfiles_idnum, new_number);
    mudlog(buf3, NULL, LOG_SYSLOG, TRUE);
    highest_pfiles_idnum = new_number;
  }

  // Finally, scan through the online characters.
  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    if (d->character && GET_IDNUM(d->character) > highest_pfiles_idnum) {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "Multiple simultaneous chargen detected, fixing idnum.");
      highest_pfiles_idnum = GET_IDNUM(d->character);
    }
    if (d->original && GET_IDNUM(d->original) > highest_pfiles_idnum) {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "Multiple simultaneous chargen detected, fixing idnum.");
      highest_pfiles_idnum = GET_IDNUM(d->original);
    }
  }

  // Send it on home.
  return highest_pfiles_idnum;
}

char_data *CreateChar(char_data *ch)
{
  idnum_t highest_idnum_in_use = get_highest_idnum_in_use();

  if (highest_idnum_in_use <= -1) {
    log_vfprintf("%s promoted to %s by virtue of first-come, first-serve.",
        GET_CHAR_NAME(ch), status_ratings[LVL_MAX]);

    for (int i = 0; i < 3; i++)
      GET_COND(ch, i) = (char) -1;

    GET_KARMA(ch) = 0;
    GET_LEVEL(ch) = LVL_MAX;
    GET_REP(ch) = 0;
    GET_NOT(ch) = 0;
    GET_TKE(ch) = 0;
    GET_IDNUM(ch) = 1;

    PLR_FLAGS(ch).RemoveBits(PLR_NEWBIE, PLR_NOT_YET_AUTHED, ENDBIT);
    PLR_FLAGS(ch).SetBits(PLR_OLC, PLR_NODELETE, ENDBIT);
    PRF_FLAGS(ch).SetBits(PRF_HOLYLIGHT, PRF_CONNLOG, PRF_ROOMFLAGS,
                          PRF_NOHASSLE, PRF_AUTOINVIS, PRF_AUTOEXIT, ENDBIT);
  } else {
    PLR_FLAGS(ch).SetBit(PLR_NOT_YET_AUTHED);
    GET_IDNUM(ch) = MAX(playerDB.find_open_id(), highest_idnum_in_use + 1);
  }

  // Ensure we don't run this character through any rectifying functions.
  PLR_FLAGS(ch).SetBits(PLR_COMPLETED_EXPERT_DRIVER_OVERHAUL,
                        PLR_RECEIVED_CYBEREYE_ESSENCE_DELTA,
                        PLR_RECEIVED_GHOUL_INDEX_DELTA,
                        ENDBIT);
  
  // They're no longer in the creation menus.
  PLR_FLAGS(ch).RemoveBit(PLR_IN_CHARGEN);

  init_char(ch);
  init_char_strings(ch);

  return ch;
}

/*
char_data *PCIndex::CreateChar(char_data *ch)
{
  if (entry_cnt >= entry_size)
    resize_table();

  entry info;

  if (strlen(GET_CHAR_NAME(ch)) >= MAX_NAME_LENGTH) {
    log("--Fatal error: Could not fit name into player index..\n"
        "             : Increase MAX_NAME_LENGTH");
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

    PLR_FLAGS(ch).RemoveBits(PLR_NEWBIE, PLR_NOT_YET_AUTHED, ENDBIT);
    PLR_FLAGS(ch).SetBits(PLR_OLC, PLR_NODELETE, ENDBIT);
    PRF_FLAGS(ch).SetBits(PRF_HOLYLIGHT, PRF_CONNLOG, PRF_ROOMFLAGS,
                          PRF_NOHASSLE, PRF_AUTOINVIS, PRF_AUTOEXIT, ENDBIT);
  } else {
    PLR_FLAGS(ch).SetBit(PLR_NOT_YET_AUTHED);
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
*/

char_data *PCIndex::LoadChar(const char *name, bool logon, int load_origin)
{
  // load the character data
  char_data *ch = Mem->GetCh();

  ch->player_specials = new player_special_data;
  ch->load_origin = load_origin;
  ch->load_time = time(0);

  load_char(name, ch, logon, load_origin);

  fix_character_essence_after_expert_driver_change(ch);

  recalculate_character_magic_rating(ch);

  fix_ghoul_index(ch);

  // At this point, cybereye migration has been done for over a year. Disabled.
  // fix_character_essence_after_cybereye_migration(ch);

  return ch;
}

bool PCIndex::SaveChar(char_data *ch, vnum_t loadroom, bool fromCopyover)
{
  if (IS_NPC(ch))
    return false;

  bool ret = save_char(ch, loadroom, fromCopyover);

  // Also regenerate their subscriber list rankings while you're at it.
  regenerate_subscriber_list_rankings(ch);

  // Make sure they're marked as existing in our global cache.
  if (GET_CHAR_NAME(ch)) {
    global_existing_player_cache[std::string(string_to_lowercase(GET_CHAR_NAME(ch)))] = TRUE;
  }

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

    // delete the old table
    delete [] tab;
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

  log_vfprintf("--Inserting %s's (id#%ld) entry into position %d",
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

std::unordered_map<std::string, bool> global_existing_player_cache = {};
bool does_player_exist(const char *unknown_case_name)
{
  const char *composed_name = string_to_lowercase(unknown_case_name);
  std::string string_name(composed_name);

  // Look them up in our cache.
  try {
    log_vfprintf("does_player_exist(%s -> %s): cache = %s", unknown_case_name, composed_name, global_existing_player_cache.at(string_name) ? "TRUE" : "FALSE");
    // return global_existing_player_cache.at(string_name);
  } catch (std::out_of_range) {
    log_vfprintf("Failed to find player '%s' in does_player_exist()'s global cache, falling back to database.", composed_name);
  }

  char buf[MAX_STRING_LENGTH];
  char prepare_quotes_buf[250];
  if (!composed_name || !*composed_name || !str_cmp(composed_name, CHARACTER_DELETED_NAME_FOR_SQL))
    return FALSE;
  snprintf(buf, sizeof(buf), "SELECT idnum FROM pfiles WHERE `Name`='%s' LIMIT 1;", prepare_quotes(prepare_quotes_buf, composed_name, 250));
  if (mysql_wrapper(mysql, buf))
    return FALSE;
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row && mysql_field_count(mysql)) {
    mysql_free_result(res);
    return (global_existing_player_cache[string_name] = FALSE);
  }
  mysql_free_result(res);
  return (global_existing_player_cache[string_name] = TRUE);
}

bool does_player_exist(long id)
{
  char buf[MAX_STRING_LENGTH];
  snprintf(buf, sizeof(buf), "SELECT idnum FROM pfiles WHERE idnum=%ld AND name != '%s';", id, CHARACTER_DELETED_NAME_FOR_SQL);
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

idnum_t get_player_id(const char *name)
{
  if (!name || !*name || !str_cmp(name, CHARACTER_DELETED_NAME_FOR_SQL))
    return -1;

  char buf[MAX_STRING_LENGTH], sanitized_buf[strlen(name) * 2 + 2];
  snprintf(buf, sizeof(buf), "SELECT idnum FROM pfiles WHERE name=\"%s\" LIMIT 1;", prepare_quotes(sanitized_buf, name, sizeof(sanitized_buf)));
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

int get_player_rank(long idnum) {
  char buf[MAX_STRING_LENGTH];
  snprintf(buf, sizeof(buf), "SELECT `rank` FROM pfiles WHERE idnum=%ld;", idnum);
  mysql_wrapper(mysql, buf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row && mysql_field_count(mysql)) {
    mysql_free_result(res);
    return -1;
  }
  int x = atoi(row[0]);
  mysql_free_result(res);
  return x;
}

char *get_player_name(vnum_t id)
{
  char buf[MAX_STRING_LENGTH];
  snprintf(buf, sizeof(buf), "SELECT Name FROM pfiles WHERE idnum=%ld;", id);
  mysql_wrapper(mysql, buf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row && mysql_field_count(mysql)) {
    mysql_free_result(res);
    return NULL;
  }
  char *x = str_dup(cleanup_invalid_color_codes(row[0]));
  mysql_free_result(res);
  return x;
}

bool _get_flag_is_set_by_idnum(int flag, vnum_t id, int mode) {
  char buf[MAX_STRING_LENGTH];

  switch (mode) {
    case 0: snprintf(buf, sizeof(buf), "SELECT PlrFlags FROM pfiles WHERE idnum=%ld;", id); break;
    case 1: snprintf(buf, sizeof(buf), "SELECT PrfFlags FROM pfiles WHERE idnum=%ld;", id); break;
    case 2: snprintf(buf, sizeof(buf), "SELECT AffFlags FROM pfiles WHERE idnum=%ld;", id); break;
    default: return FALSE;
  }

  mysql_wrapper(mysql, buf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row && mysql_field_count(mysql)) {
    mysql_free_result(res);
    return FALSE;
  }

  Bitfield flags;
  flags.FromString(row[0]);

  mysql_free_result(res);

  return flags.IsSet(flag);
}

bool get_plr_flag_is_set_by_idnum(int flag, vnum_t id) {
  return _get_flag_is_set_by_idnum(flag, id, 0);
}

bool get_prf_flag_is_set_by_idnum(int flag, vnum_t id) {
  return _get_flag_is_set_by_idnum(flag, id, 1);
}

bool get_aff_flag_is_set_by_idnum(int flag, vnum_t id) {
  return _get_flag_is_set_by_idnum(flag, id, 2);
}

bool player_is_dead_hardcore(long id) {
  Bitfield plr_flags, prf_flags;

  if (!does_player_exist(id)) {
    return FALSE;
  }

  // Check if they're a dead hardcore char.
  return get_plr_flag_is_set_by_idnum(PLR_JUST_DIED, id) && get_prf_flag_is_set_by_idnum(PRF_HARDCORE, id);
}

void DeleteChar(long idx)
{
  /* Delete a character in such a way that it can be restored if desired. */
  MYSQL_RES *res;
  MYSQL_ROW row;
  FILE *fl;
  char prepare_quotes_buf[MAX_STRING_LENGTH * 2 + 1];

  const char *table_names[] = {
    "pfiles                  ", // 0. IF YOU CHANGE THIS, CHANGE PFILES_INDEX
    "pfiles_adeptpowers      ",
    "pfiles_alias            ",
    "pfiles_ammo             ",
    "pfiles_bioware          ",
    "pfiles_chargendata      ", // 5
    "pfiles_cyberware        ",
    "pfiles_drugs            ",
    "pfiles_echoes           ",
    "pfiles_exdescs          ",
    "pfiles_ignore           ", // 10. IF YOU CHANGE THIS, CHANGE PFILES_IGNORE_INDEX
    "pfiles_ignore_v2        ", // 11. IF YOU CHANGE THIS, CHANGE PFILES_IGNORE_V2_INDEX
    "pfiles_immortdata       ",
    "pfiles_inv              ",
    "pfiles_magic            ",
    "pfiles_mail             ", // 15
    "pfiles_memory           ", // 16. IF YOU CHANGE THIS, CHANGE PFILES_MEMORY_INDEX
    "pfiles_metamagic        ",
    "pfiles_named_tags       ",
    // pfiles_playergroups handled as a special case below
    "pfiles_quests           ",
    "pfiles_skills           ", // 20
    "pfiles_spells           ",
    "pfiles_spirits          ",
    "pfiles_worn             ",
    "playergroup_invitations ",
    "pocsec_phonebook        "  // 25
  };
  #define NUM_SQL_TABLE_NAMES     26
  #define PFILES_INDEX            0
  #define PFILES_IGNORE_INDEX     10
  #define PFILES_IGNORE_V2_INDEX  11
  #define PFILES_MEMORY_INDEX     16

  // Figure out the filename for this character.
  const char *name = get_player_name(idx);
  snprintf(buf, sizeof(buf), "idledeleted/%s-%ld", name, idx);

  // Drop them from the table of folks who exist.
  global_existing_player_cache[std::string(string_to_lowercase(name))] = FALSE;
  delete[] name;

  // Ensure we can open the file.
  if (!(fl = fopen(buf, "w"))) {
    perror(buf);
    return;
  }

  // Prepend the file with the statement to remove this character's [Deleted] entry.
  fprintf(fl, "DELETE FROM pfiles WHERE idnum=%ld;\r\n", idx);

  // Write the info for each section to the file, then delete the relevant DB info.
  for (int table_idx = 0; table_idx < NUM_SQL_TABLE_NAMES; table_idx++) {
    // Get the table schema.
    snprintf(buf, sizeof(buf), "DESCRIBE %s", table_names[table_idx]);
    mysql_wrapper(mysql, buf);
    res = mysql_use_result(mysql);

    // Compose our table schema string.
    snprintf(buf2, sizeof(buf2), "INSERT INTO %s (", table_names[table_idx]);
    bool first_pass_of_table_description = TRUE;
    while ((row = mysql_fetch_row(res))) {
      // Describe the table schema into buf2.
      prepare_quotes(prepare_quotes_buf, row[0], sizeof(prepare_quotes_buf), TRUE);
      snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "%s`%s`", first_pass_of_table_description ? "" : ", ", prepare_quotes_buf);
      first_pass_of_table_description = FALSE;
    }
    mysql_free_result(res);

    if (first_pass_of_table_description) {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Failed to describe table %s! This puts us in a fucky state, %ld is only partially deleted.", table_names[table_idx], idx);
      return;
    }
    strlcat(buf2, ") VALUES (", sizeof(buf2));

    // Get the values.
    if (table_idx == PFILES_IGNORE_INDEX || table_idx == PFILES_MEMORY_INDEX) {
      snprintf(buf, sizeof(buf), "SELECT * FROM %s WHERE idnum=%ld OR remembered=%ld", table_names[table_idx], idx, idx);
    }
    else if (table_idx == PFILES_IGNORE_V2_INDEX) {
      snprintf(buf, sizeof(buf), "SELECT * FROM %s WHERE idnum=%ld OR vict_idnum=%ld", table_names[table_idx], idx, idx);
    }
    else {
      snprintf(buf, sizeof(buf), "SELECT * FROM %s WHERE idnum=%ld", table_names[table_idx], idx);
    }

    mysql_wrapper(mysql, buf);

    if ((res = mysql_use_result(mysql))) {
      while ((row = mysql_fetch_row(res))) {
        // Populate buf3 with the table schema.
        strlcpy(buf3, buf2, sizeof(buf3));

        // Fill out data in buf3.
        for (unsigned int row_idx = 0; row_idx < mysql_num_fields(res); row_idx++) {
          prepare_quotes(prepare_quotes_buf, row[row_idx], sizeof(prepare_quotes_buf), TRUE);
          snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "'%s'%s", prepare_quotes_buf, row_idx == mysql_num_fields(res) - 1 ? ")" : ", ");
        }
        // Write the values to the file, assuming there are values to write here.
        fprintf(fl, "%s;\r\n", buf3);
      }
      mysql_free_result(res);
    }

    // Finally, delete the table entry, unless we're at index 0-- that's the pfile table.
    if (table_idx != PFILES_INDEX) {
      if (table_idx == PFILES_IGNORE_INDEX || table_idx == PFILES_MEMORY_INDEX) {
        snprintf(buf, sizeof(buf), "DELETE FROM %s WHERE idnum=%ld OR remembered=%ld", table_names[table_idx], idx, idx);
      }
      else if (table_idx == PFILES_IGNORE_V2_INDEX) {
        snprintf(buf, sizeof(buf), "DELETE FROM %s WHERE idnum=%ld OR vict_idnum=%ld", table_names[table_idx], idx, idx);
      }
      else {
        snprintf(buf, sizeof(buf), "DELETE FROM %s WHERE idnum=%ld", table_names[table_idx], idx);
      }
      mysql_wrapper(mysql, buf);
    }
  }
  fclose(fl);

  // Update playergroup info, write a log, and delete their info and invitations.
  snprintf(buf, sizeof(buf), "SELECT `group` FROM pfiles_playergroups WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);
  if ((res = mysql_use_result(mysql))) {
    if ((row = mysql_fetch_row(res))) {
      mysql_free_result(res);
      char *cname = get_player_name(idx);
      snprintf(buf, sizeof(buf), "INSERT INTO pgroup_logs (idnum, message, redacted) VALUES (%ld, \"%s has left the group. (Reason: deletion)\", 0)", idx, cname);
      delete [] cname;
      mysql_wrapper(mysql, buf);
      snprintf(buf, sizeof(buf), "DELETE FROM pfiles_playergroups WHERE idnum=%ld", idx);
      mysql_wrapper(mysql, buf);
    } else {
      mysql_free_result(res);
    }
  }

  snprintf(buf, sizeof(buf), "DELETE FROM playergroup_invitations WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);

  // Wipe out their name and password so they can't log in anymore.
  snprintf(buf, sizeof(buf), "UPDATE pfiles SET Name='%s', Password='', NoDelete=TRUE WHERE idnum=%ld", CHARACTER_DELETED_NAME_FOR_SQL, idx);
//  snprintf(buf, sizeof(buf), "DELETE FROM pfiles WHERE idnum=%ld", idx);
  mysql_wrapper(mysql, buf);

  // Drop them from the ignore lists of in-game players.
  globally_remove_vict_id_from_logged_in_ignore_lists(idx);
}

time_t get_idledelete_days_left(time_t lastd, int tke, int race, int rank) {
  // Set the base idle deletion days.
  time_t base_value;

  if (rank > LVL_MORTAL) {
    base_value = IDLE_DELETE_BASE_DAYS_FOR_STAFF;
  } else if (IS_PRESTIGE_RACE(race)) {
    base_value = IDLE_DELETE_BASE_DAYS_FOR_PRESTIGE;
  } else {
    base_value = IDLE_DELETE_BASE_DAYS_FOR_PCS;
  }

  time_t tke_days = base_value + (tke / NUMBER_OF_TKE_POINTS_PER_REAL_DAY_OF_EXTRA_IDLE_DELETE_GRACE_PERIOD);
  time_t seconds_left = lastd - (time(0) - (SECS_PER_REAL_DAY * tke_days));
  time_t days_left = floor(seconds_left / SECS_PER_REAL_DAY);

  return days_left;
}

bool pc_active_in_last_30_days(idnum_t owner_id) {
  snprintf(buf, sizeof(buf), "SELECT idnum FROM pfiles WHERE lastd >= %ld AND name != '%s';", time(0) - (SECS_PER_REAL_DAY * 30), CHARACTER_DELETED_NAME_FOR_SQL);
  mysql_wrapper(mysql, buf);
  MYSQL_RES *res;

  if (!(res = mysql_use_result(mysql)))
    return FALSE;
  
  MYSQL_ROW row = mysql_fetch_row(res);
  mysql_free_result(res);

  return (row != NULL);
}

// Defining this for now until I have a chance to fix the nodelete logic. Currently, it sets a bit, but checks for a boolean. Need to run a migration on existing DB to apply that boolean value on everyone with the bit set.
#define IDLEDELETE_DRYRUN

void idle_delete()
{
  int deleted = 0;
  char buf[MAX_STRING_LENGTH];

  MYSQL *mysqlextra = mysql_init(NULL);
  if (!mysql_real_connect(mysqlextra, mysql_host, mysql_user, mysql_password, mysql_db, GAME_MYSQL_PORT, NULL, 0)) {
    log("IDLEDELETE- Could not open extra socket, aborting");
    return;
  }
  snprintf(buf, sizeof(buf), "SELECT `idnum`, `lastd`, `tke`, `race`, `rank` FROM pfiles WHERE lastd <= %ld AND nodelete=0 AND name != '%s' ORDER BY lastd ASC;", time(0) - (SECS_PER_REAL_DAY * 50), CHARACTER_DELETED_NAME_FOR_SQL);
  mysql_wrapper(mysqlextra, buf);
  MYSQL_RES *res;
  MYSQL_ROW row;

  if ((res = mysql_use_result(mysqlextra))) {
    while ((row = mysql_fetch_row(res))) {
      int tke = atoi(row[2]);
      time_t lastd = atoi(row[1]);
      int race = atoi(row[3]);
      int rank = atoi(row[4]);
      if (get_idledelete_days_left(lastd, tke, race, rank) < 0) {
#ifndef IDLEDELETE_DRYRUN
        // TODO: Pull their PLR bitstring and validate that their nodelete bit is set to off.
        DeleteChar(atol(row[0]));
        deleted++;
#else
        log_vfprintf("IDLEDELETE- Would delete %s, but IDLEDELETE_DRYRUN is enabled.", row[0]);
#endif
      }
    }
    mysql_free_result(res);
  }

  mysql_close(mysqlextra);
  snprintf(buf, sizeof(buf), "IDLEDELETE- %d Deleted.", deleted);
  log(buf);
}


void verify_db_password_column_size() {
  // show columns in pfiles like 'password';
  mysql_wrapper(mysql, "SHOW COLUMNS IN `pfiles` LIKE 'password';");
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (strncmp(row[1], "varchar(200)", sizeof("varchar(200)")) != 0) {
    log("FATAL ERROR: Your pfiles table's password column needs to be reconfigured.");
    log("Execute this command on your database:  ALTER TABLE `pfiles` MODIFY `Password` VARCHAR(200);");
    exit(ERROR_DATABASE_CHANGES_NEEDED);
  }
  mysql_free_result(res);
}

#define CLAMP_VALUE(itemtype, field, minv, maxv, fieldname)                                                            \
prior_data = (field);                                                                                                  \
(field) = MIN((maxv), MAX((minv), (field)));                                                                           \
if (prior_data != (field)) {                                                                                           \
  snprintf(buf, sizeof(buf), "INFO: System self-healed " #itemtype " %s (%ld), whose " #fieldname " was %d (now %d).", \
           GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj), prior_data, field);                                                   \
  mudlog(buf, obj->carried_by, LOG_SYSLOG, TRUE);                                                                      \
}

#define FORCE_PROTO_VALUE(itemtype, value, proto_value)                                                                \
if (proto_value != value) {                                                                                            \
  snprintf(buf, sizeof(buf), "INFO: System self-healed " #itemtype " %s (%ld), whose " #value " was %d (becomes %d)",  \
           GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj), value, proto_value);                                                  \
  mudlog(buf, obj->carried_by, LOG_SYSLOG, TRUE);                                                                      \
  value = proto_value;                                                                                                 \
}

#define FORCE_PROTO_VALUE_INDEXED_BY_I(itemtype, value, proto_value)                                                        \
if (proto_value != value) {                                                                                                 \
  snprintf(buf, sizeof(buf), "INFO: System self-healed " #itemtype " %s (%ld), whose " #value " (%d) was %d (becomes %d)",  \
           GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj), i, value, proto_value);                                                    \
  mudlog(buf, obj->carried_by, LOG_SYSLOG, TRUE);                                                                           \
  value = proto_value;                                                                                                      \
}

void auto_repair_obj(struct obj_data *obj, idnum_t owner) {
  // Go through its contents first and rectify them.
  for (struct obj_data *contents = obj->contains; contents; contents = contents->next_content) {
    auto_repair_obj(contents, owner);
  }

  ENSURE_OBJ_HAS_IDNUM(obj);

  int old_storage;
  int rnum = real_object(GET_OBJ_VNUM(obj));

  if (rnum < 0) {
    mudlog("SYSERR: Received INVALID rnum when loading object!", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // We rely on the object type being set correctly for further code, so we must force-rectify it.
  if (GET_OBJ_SPEC(obj) != weapon_dominator)
    FORCE_PROTO_VALUE("all", GET_OBJ_TYPE(obj), GET_OBJ_TYPE(&obj_proto[rnum]));

  // Now that any changes have bubbled up, rectify this object too.
  switch (GET_OBJ_TYPE(obj)) {
    case ITEM_KEY:
      // Soulbind any keys that came from quest rewards.
      for (rnum_t quest_idx = 0; quest_idx <= top_of_questt; quest_idx++) {
        if (quest_table[quest_idx].reward && quest_table[quest_idx].reward == GET_OBJ_VNUM(obj)) {
          soulbind_obj_to_char_by_idnum(obj, owner, FALSE);
          break;
        }
      }

      // For some reason, a lot of keys are bonded to 1. Fix this.
      if (GET_KEY_SOULBOND(obj) == 1)
        GET_KEY_SOULBOND(obj) = 0;
      break;
    case ITEM_DRUG:
      {
        int prior_data;
        FORCE_PROTO_VALUE("drug", GET_OBJ_DRUG_TYPE(obj), GET_OBJ_DRUG_TYPE(&obj_proto[rnum]));
        CLAMP_VALUE("drug", GET_OBJ_DRUG_DOSES(obj), 1, MAX_DRUG_DOSE_COUNT, "doses");
      }
      break;
    case ITEM_CYBERDECK:
    case ITEM_CUSTOM_DECK:
      {
        // Rectify the memory.
        old_storage = GET_CYBERDECK_USED_STORAGE(obj);
        GET_CYBERDECK_USED_STORAGE(obj) = 0;
        for (struct obj_data *installed = obj->contains; installed; installed = installed->next_content) {
          if (GET_OBJ_TYPE(installed) == ITEM_DECK_ACCESSORY) {
            switch (GET_DECK_ACCESSORY_TYPE(installed)) {
              case TYPE_FILE:
                GET_CYBERDECK_USED_STORAGE(obj) += GET_DECK_ACCESSORY_FILE_SIZE(installed);
                break;
              case TYPE_UPGRADE:
                GET_PART_BUILDER_IDNUM(obj) = 0;
                break;
            }
          }
          // Personas don't take up storage in store-bought decks (personas are not programs in custom decks)
          if ((GET_OBJ_TYPE(installed) == ITEM_PROGRAM) && !((GET_OBJ_TYPE(obj) == ITEM_CYBERDECK) && (GET_PROGRAM_TYPE(installed) <= SOFT_SENSOR))) {
            GET_CYBERDECK_USED_STORAGE(obj) += GET_PROGRAM_SIZE(installed);
          }
        }
        if (old_storage != GET_CYBERDECK_USED_STORAGE(obj)) {
          snprintf(buf, sizeof(buf), "INFO: System self-healed mismatching cyberdeck used storage for %s (was %d, should have been %d)",
                  GET_OBJ_NAME(obj),
                  old_storage,
                  GET_CYBERDECK_USED_STORAGE(obj)
          );
          mudlog(buf, obj->carried_by, LOG_SYSLOG, TRUE);
        }
        if (GET_CYBERDECK_USED_STORAGE(obj) > GET_CYBERDECK_TOTAL_STORAGE(obj)) {
          mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: After self-heal, deck %s owned by %ld is overloaded on programs! Setting to 0 free space, this will cause problems.", GET_OBJ_NAME(obj), owner);
          GET_CYBERDECK_USED_STORAGE(obj) = GET_CYBERDECK_TOTAL_STORAGE(obj);
        }
      }
      break;
    case ITEM_FOCUS:
      FORCE_PROTO_VALUE("focus", GET_FOCUS_TYPE(obj), GET_FOCUS_TYPE(&obj_proto[rnum]));
      FORCE_PROTO_VALUE("focus", GET_FOCUS_FORCE(obj), GET_FOCUS_FORCE(&obj_proto[rnum]));

      // Geas now uses its own field instead of reusing GET_FOCUS_BOND_TIME_REMAINING
      if (GET_FOCUS_BOND_TIME_REMAINING(obj) == owner) {
        GET_FOCUS_GEAS(obj) = owner;
        GET_FOCUS_BOND_TIME_REMAINING(obj) = 0;
      }
      break;
    case ITEM_MOD:
      // Mods don't ever get changed, so we clamp them aggressively.
      for (int i = 0; i < NUM_OBJ_VALUES; i++) {
        FORCE_PROTO_VALUE_INDEXED_BY_I("vehicle mod", GET_OBJ_VAL(obj, i), GET_OBJ_VAL(&obj_proto[rnum], i));
      }
      break;
    case ITEM_WEAPON:
      {
        // We perform extensive clamping and checks on weapons due to prior issues with corrupted weapon data.
        int prior_data;

        FORCE_PROTO_VALUE("weapon", GET_WEAPON_POWER(obj), GET_WEAPON_POWER(&obj_proto[rnum]));
        FORCE_PROTO_VALUE("weapon", GET_WEAPON_DAMAGE_CODE(obj), GET_WEAPON_DAMAGE_CODE(&obj_proto[rnum]));
        FORCE_PROTO_VALUE("weapon", GET_WEAPON_STR_BONUS(obj), GET_WEAPON_STR_BONUS(&obj_proto[rnum]));
        FORCE_PROTO_VALUE("weapon", GET_WEAPON_ATTACK_TYPE_SETTABLE(obj), GET_WEAPON_ATTACK_TYPE_SETTABLE(&obj_proto[rnum]));
        FORCE_PROTO_VALUE("weapon", GET_WEAPON_SKILL(obj), GET_WEAPON_SKILL(&obj_proto[rnum]));

        if (WEAPON_IS_GUN(obj)) {
          FORCE_PROTO_VALUE("weapon", GET_WEAPON_MAX_AMMO(obj), GET_WEAPON_MAX_AMMO(&obj_proto[rnum]));
          FORCE_PROTO_VALUE("weapon", GET_WEAPON_POSSIBLE_FIREMODES(obj), GET_WEAPON_POSSIBLE_FIREMODES(&obj_proto[rnum]));
          FORCE_PROTO_VALUE("weapon", GET_WEAPON_INTEGRAL_RECOIL_COMP(obj), GET_WEAPON_INTEGRAL_RECOIL_COMP(&obj_proto[rnum]));

          CLAMP_VALUE("weapon", GET_WEAPON_FIREMODE(obj), MODE_SS, MODE_FA, "firemode");
          CLAMP_VALUE("weapon", GET_WEAPON_FULL_AUTO_COUNT(obj), 0, 10, "full auto count");

          // Specific edge case: Coerce the Victory/Vigilant autocannons to SA firemode.
          if (GET_OBJ_VNUM(obj) == 901 || GET_OBJ_VNUM(obj) == 902 || GET_OBJ_VNUM(obj) == 903) {
            GET_WEAPON_FIREMODE(obj) = MODE_SA;
          }

          rnum_t attach_rnum;

          if (GET_WEAPON_ATTACH_TOP_VNUM(obj) != 0) {
            attach_rnum = real_object(GET_WEAPON_ATTACH_TOP_VNUM(obj));
            if (attach_rnum < 0 || GET_OBJ_TYPE(&obj_proto[attach_rnum]) != ITEM_GUN_ACCESSORY) {
              FORCE_PROTO_VALUE("weapon w/ bad attach vnum", GET_WEAPON_ATTACH_TOP_VNUM(obj), GET_WEAPON_ATTACH_TOP_VNUM(&obj_proto[rnum]));
            }
          }

          if (GET_WEAPON_ATTACH_BARREL_VNUM(obj) != 0) {
            attach_rnum = real_object(GET_WEAPON_ATTACH_BARREL_VNUM(obj));
            if (attach_rnum < 0 || GET_OBJ_TYPE(&obj_proto[attach_rnum]) != ITEM_GUN_ACCESSORY) {
              FORCE_PROTO_VALUE("weapon w/ bad attach vnum", GET_WEAPON_ATTACH_BARREL_VNUM(obj), GET_WEAPON_ATTACH_BARREL_VNUM(&obj_proto[rnum]));
            }
          }

          if (GET_WEAPON_ATTACH_UNDER_VNUM(obj) != 0) {
            attach_rnum = real_object(GET_WEAPON_ATTACH_UNDER_VNUM(obj));
            if (attach_rnum < 0 || GET_OBJ_TYPE(&obj_proto[attach_rnum]) != ITEM_GUN_ACCESSORY) {
              FORCE_PROTO_VALUE("weapon w/ bad attach vnum", GET_WEAPON_ATTACH_UNDER_VNUM(obj), GET_WEAPON_ATTACH_UNDER_VNUM(&obj_proto[rnum]));
            }
          }
        } else {
          FORCE_PROTO_VALUE("weapon", GET_WEAPON_REACH(obj), GET_WEAPON_REACH(&obj_proto[rnum]));
          FORCE_PROTO_VALUE("weapon", GET_WEAPON_FOCUS_RATING(obj), GET_WEAPON_FOCUS_RATING(&obj_proto[rnum]));
        }

        // Warn on any non-magazine items. I would dump them out, but the weapon is currently in the void waiting to be placed somewhere.
        struct obj_data *next;
        for (struct obj_data *contents = obj->contains; contents; contents = next) {
          next = contents->next_content;
          if (GET_OBJ_TYPE(contents) != ITEM_GUN_MAGAZINE) {
            snprintf(buf, sizeof(buf), "^RSYSERR^g: While loading weapon %s, came across non-magazine object contained by it. [OBJ_LOAD_ERROR_GREP_STRING]", GET_OBJ_NAME(obj));
            mudlog(buf, NULL, LOG_SYSLOG, TRUE);
            const char *representation = generate_new_loggable_representation(obj);
            mudlog(representation, NULL, LOG_SYSLOG, TRUE);
            delete [] representation;
            break;
          }
        }
      }

      break;
    case ITEM_WORKSHOP:
      FORCE_PROTO_VALUE("workshop", GET_WORKSHOP_TYPE(obj), GET_WORKSHOP_TYPE(&obj_proto[rnum]));
      FORCE_PROTO_VALUE("workshop", GET_WORKSHOP_GRADE(obj), GET_WORKSHOP_GRADE(&obj_proto[rnum]));
      // Repair ammo kit values to fix the ones that were "unpacked"
      if (GET_WORKSHOP_TYPE(obj) == TYPE_GUNSMITHING && GET_WORKSHOP_GRADE(obj) == TYPE_KIT)
        FORCE_PROTO_VALUE("ammo kit", GET_WORKSHOP_AMMOKIT_TYPE(obj), GET_WORKSHOP_AMMOKIT_TYPE(&obj_proto[rnum]));
      break;
    case ITEM_WORN:
    /*  // Edit: Don't do this, you end up giving people infinite pockets.
      FORCE_PROTO_VALUE("worn", GET_WORN_POCKETS_HOLSTERS(obj), GET_WORN_POCKETS_HOLSTERS(&obj_proto[rnum]));
      FORCE_PROTO_VALUE("worn", GET_WORN_POCKETS_MISC(obj), GET_WORN_POCKETS_MISC(&obj_proto[rnum]));
    */
      FORCE_PROTO_VALUE("worn", GET_WORN_BALLISTIC(obj), GET_WORN_BALLISTIC(&obj_proto[rnum]));
      FORCE_PROTO_VALUE("worn", GET_WORN_IMPACT(obj), GET_WORN_IMPACT(&obj_proto[rnum]));
      FORCE_PROTO_VALUE("worn", GET_WORN_CONCEAL_RATING(obj), GET_WORN_CONCEAL_RATING(&obj_proto[rnum]));
      FORCE_PROTO_VALUE("worn", GET_WORN_MATCHED_SET(obj), GET_WORN_MATCHED_SET(&obj_proto[rnum]));

      // Specific object repair: Military helmet.
      if (GET_OBJ_VNUM(obj) == OBJ_HEAVY_MILITARY_HELMET) {
        if (GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(obj) == 0) {
          mudlog_vfprintf(NULL, LOG_SYSLOG, "INFO: System self-healed old military helmet '%s' to be unbonded again.", GET_OBJ_NAME(obj));
          GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(obj) = -1;
        }

        GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_HARDENED_ARMOR);
      }

      // Fix soulbound non-hardened items.
      if (!IS_OBJ_STAT(obj, ITEM_EXTRA_HARDENED_ARMOR))
        FORCE_PROTO_VALUE("worn", GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(obj), GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(&obj_proto[rnum]));
        
      break;
    case ITEM_DOCWAGON:
      FORCE_PROTO_VALUE("docwagon modulator", GET_DOCWAGON_CONTRACT_GRADE(obj), GET_DOCWAGON_CONTRACT_GRADE(&obj_proto[rnum]));
      break;
    case ITEM_BIOWARE:
      FORCE_PROTO_VALUE("bioware", GET_BIOWARE_TYPE(obj), GET_BIOWARE_TYPE(&obj_proto[rnum]));
      FORCE_PROTO_VALUE("bioware", GET_BIOWARE_RATING(obj), GET_BIOWARE_RATING(&obj_proto[rnum]));
      FORCE_PROTO_VALUE("bioware", GET_SETTABLE_BIOWARE_IS_CULTURED(obj), GET_SETTABLE_BIOWARE_IS_CULTURED(&obj_proto[rnum]));

      if (GET_CYBERWARE_TYPE(obj) != BIO_CUSTOM_NERPS) {
        FORCE_PROTO_VALUE("bioware", GET_BIOWARE_ESSENCE_COST(obj), GET_BIOWARE_ESSENCE_COST(&obj_proto[rnum]));
      } else {
        GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_WIZLOAD);
        GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_NERPS);
      }
      break;
    case ITEM_CYBERWARE:
      FORCE_PROTO_VALUE("cyberware", GET_CYBERWARE_TYPE(obj), GET_CYBERWARE_TYPE(&obj_proto[rnum]));
      FORCE_PROTO_VALUE("cyberware", GET_CYBERWARE_RATING(obj), GET_CYBERWARE_RATING(&obj_proto[rnum]));
      FORCE_PROTO_VALUE("cyberware", GET_CYBERWARE_GRADE(obj), GET_CYBERWARE_GRADE(&obj_proto[rnum]));

      if (GET_CYBERWARE_TYPE(obj) != CYB_CUSTOM_NERPS) {
        FORCE_PROTO_VALUE("cyberware", GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj), GET_CYBERWARE_ESSENCE_COST_RO(&obj_proto[rnum]));
      } else {
        GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_WIZLOAD);
        GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_NERPS);
      }

      if (GET_CYBERWARE_TYPE(obj) == CYB_EYES) {
        FORCE_PROTO_VALUE("cybereyes", GET_CYBERWARE_FLAGS(obj), GET_CYBERWARE_FLAGS(&obj_proto[rnum]));
      }
      break;
    case ITEM_GUN_ACCESSORY:
      FORCE_PROTO_VALUE("gun accessory", GET_ACCESSORY_ATTACH_LOCATION(obj), GET_ACCESSORY_ATTACH_LOCATION(&obj_proto[rnum]));
      FORCE_PROTO_VALUE("gun accessory", GET_ACCESSORY_TYPE(obj), GET_ACCESSORY_TYPE(&obj_proto[rnum]));
      FORCE_PROTO_VALUE("gun accessory", GET_ACCESSORY_RATING(obj), GET_ACCESSORY_RATING(&obj_proto[rnum]));
      break;
  }
}
#undef CLAMP_VALUE
#undef FORCE_PROTO_VALUE
#undef FORCE_PROTO_VALUE_INDEXED_BY_I

// Allows you to supply an email address.
ACMD(do_register) {
  if (!argument) {
    send_to_char(ch, "Syntax: register <email address>. Your current email is %s.\r\n", GET_EMAIL(ch));
    return;
  }

  skip_spaces(&argument);

  if (!*argument) {
    send_to_char(ch, "Syntax: register <email address>. Your current email is %s.\r\n", GET_EMAIL(ch));
    return;
  }

  DELETE_ARRAY_IF_EXTANT(SETTABLE_EMAIL(ch));
  SETTABLE_EMAIL(ch) = str_dup(cleanup_invalid_color_codes(prepare_quotes(buf, argument, sizeof(buf) * sizeof(buf[0]))));

  send_to_char(ch, "OK, your email address has been set to '%s'.\r\n", GET_EMAIL(ch));

  playerDB.SaveChar(ch);
}

void save_adept_powers_to_db(struct char_data *player) {
  if (GET_TRADITION(player) == TRAD_ADEPT) {
    snprintf(buf, sizeof(buf), "DELETE FROM pfiles_adeptpowers WHERE idnum=%ld", GET_IDNUM(player));
    mysql_wrapper(mysql, buf);
    strcpy(buf, "INSERT INTO pfiles_adeptpowers (idnum, powernum, `rank`) VALUES (");

    // Iterate over all powers, adding them to the list if needed.
    int q = 0;
    for (int i = 0; i < ADEPT_NUMPOWER; i++)
      if (GET_POWER_TOTAL(player, i)) {
        if (q)
          strcat(buf, "), (");
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld, %d, %d", GET_IDNUM(player), i, GET_POWER_TOTAL(player, i));
        q = 1;
      }
    if (q) {
      strcat(buf, ");");
      mysql_wrapper(mysql, buf);
    }
  }
}

/* Save spells. */
void save_spells_to_db(struct char_data *player) {
  if (player->spells) {
    snprintf(buf, sizeof(buf), "DELETE FROM pfiles_spells WHERE idnum=%ld", GET_IDNUM(player));
    mysql_wrapper(mysql, buf);
    strcpy(buf, "INSERT INTO pfiles_spells (idnum, Name, Type, SubType, Rating, Category) VALUES (");
    int q = 0;
    for (struct spell_data *temp = player->spells; temp; temp = temp->next) {
      if (q)
        strcat(buf, "), (");
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld, '%s', %d, %d, %d, %d", GET_IDNUM(player), temp->name, temp->type, temp->subtype, temp->force, spells[temp->type].category);
      q = 1;
    }
    if (q) {
      strcat(buf, ");");
      mysql_wrapper(mysql, buf);
    }
  }
}

/* Save echoes. */
void save_echoes_to_db(struct char_data *player) {
  if (GET_GRADE(player) > 0) {
    snprintf(buf, sizeof(buf), "DELETE FROM pfiles_echoes WHERE idnum=%ld", GET_IDNUM(player));
    mysql_wrapper(mysql, buf);
    strcpy(buf, "INSERT INTO pfiles_echoes (idnum, echonum, `rank`) VALUES (");
    int q = 0;
    for (int i = ECHO_UNDEFINED; i < ECHO_MAX; i++)
      if (GET_ECHO(player, i)) {
        if (q)
          strcat(buf, "), (");
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld, %d, %d", GET_IDNUM(player), i, GET_ECHO(player, i));
        q = 1;
     }
    if (q) {
      strcat(buf, ");");
      mysql_wrapper(mysql, buf);
    }
  }
}

/* Save metamagic. */
void save_metamagic_to_db(struct char_data *player) {
  PERF_PROF_SCOPE(pr_, __func__);
  if (GET_GRADE(player) > 0) {
    snprintf(buf, sizeof(buf), "DELETE FROM pfiles_metamagic WHERE idnum=%ld", GET_IDNUM(player));
    mysql_wrapper(mysql, buf);
    strcpy(buf, "INSERT INTO pfiles_metamagic (idnum, metamagicnum, `rank`) VALUES (");
    int q = 0;
    for (int i = 0; i < META_MAX; i++)
      if (GET_METAMAGIC(player, i)) {
        if (q)
          strcat(buf, "), (");
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld, %d, %d", GET_IDNUM(player), i, GET_METAMAGIC(player, i));
        q = 1;
     }
    if (q) {
      strcat(buf, ");");
      mysql_wrapper(mysql, buf);
    }
  }
}

void save_quests_to_db(struct char_data *player) {
  PERF_PROF_SCOPE(pr_, __func__);
  int i, q;

  snprintf(buf, sizeof(buf), "DELETE FROM pfiles_quests WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  strlcpy(buf, "INSERT INTO pfiles_quests (idnum, number, questnum, completed) VALUES (", sizeof(buf));
  for (i = 0, q = 0; i <= QUEST_TIMER - 1; i++) {
    if (GET_LQUEST(player ,i)) {
      bool found = FALSE;
      // Check to see if it's in the CQUEST list. If it is, store it as completed.
      for (int c_idx = 0; c_idx <= QUEST_TIMER - 1; c_idx++) {
        if (GET_LQUEST(player, i) == GET_CQUEST(player, c_idx)) {
          found = TRUE;
          break;
        }
      }

      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%ld, %d, %ld, %s", 
               q ? "), (" : "",
               GET_IDNUM(player), i, GET_LQUEST(player, i), found ? "TRUE" : "FALSE");
      q = 1;
    }
  }
  if (q) {
    strcat(buf, ");");
    mysql_wrapper(mysql, buf);
  }
}

void save_elementals_to_db(struct char_data *player) {
  PERF_PROF_SCOPE(pr_, __func__);
  if (GET_TRADITION(player) == TRAD_HERMETIC) {
    snprintf(buf, sizeof(buf), "DELETE FROM pfiles_spirits WHERE idnum=%ld", GET_IDNUM(player));
    mysql_wrapper(mysql, buf);
    if (GET_SPIRIT(player)) {
      strcpy(buf, "INSERT INTO pfiles_spirits (idnum, Type, Rating, Services, SpiritID) VALUES (");
      int q = 0;
      for (struct spirit_data *spirit = GET_SPIRIT(player); spirit; spirit = spirit->next, q++) {
        if (q)
          strcat(buf, "), (");
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld, %d, %d, %d, %d", GET_IDNUM(player), spirit->type, spirit->force, spirit->services, spirit->id);
        q = 1;
      }
      if (q) {
        strcat(buf, ");");
        mysql_wrapper(mysql, buf);
      }
    }
  }
}

void save_pc_memory_to_db(struct char_data *player) {
  PERF_PROF_SCOPE(pr_, __func__);
  snprintf(buf, sizeof(buf), "DELETE FROM pfiles_memory WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  strcpy(buf, "INSERT INTO pfiles_memory (idnum, remembered, asname) VALUES (");
  int q = 0;
  for (struct remem *b = GET_PLAYER_MEMORY(player); b; b = b->next) {
    if (q)
      strcat(buf, "), (");
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld, %ld, '%s'", GET_IDNUM(player), b->idnum, prepare_quotes(buf3, b->mem, sizeof(buf3) / sizeof(buf3[0])));
    q = 1;
  }
  if (q) {
    strcat(buf, ");");
    mysql_wrapper(mysql, buf);
  }
}

void save_drug_data_to_db(struct char_data *player) {
  PERF_PROF_SCOPE(pr_, __func__);
  snprintf(buf, sizeof(buf), "DELETE FROM pfiles_drugs WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  strcpy(buf, "INSERT INTO pfiles_drugs (idnum, DrugType, Addict, Doses, Edge, LastFix, Addtime, Tolerant, LastWith, Duration, Dose, Stage) VALUES (");
  int q = 0;
  for (int i = MIN_DRUG; i < NUM_DRUGS; i++) {
    if (GET_DRUG_LIFETIME_DOSES(player, i) || GET_DRUG_ADDICTION_EDGE(player, i) || GET_DRUG_ADDICT(player, i) || GET_DRUG_LAST_FIX(player, i) ||
        GET_DRUG_ADDICTION_TICK_COUNTER(player, i) || GET_DRUG_TOLERANCE_LEVEL(player, i) || GET_DRUG_LAST_WITHDRAWAL_TICK(player, i) || GET_DRUG_DURATION(player, i) ||
        GET_DRUG_DOSE(player, i) || GET_DRUG_STAGE(player, i))
    {
      if (q)
        strcat(buf, "), (");
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld, %d, %d, %d, %d, %ld, %d, %d, %d, %d, %d, %d", GET_IDNUM(player), i, GET_DRUG_ADDICT(player, i),
                          GET_DRUG_LIFETIME_DOSES(player, i), GET_DRUG_ADDICTION_EDGE(player, i), GET_DRUG_LAST_FIX(player, i),
                          GET_DRUG_ADDICTION_TICK_COUNTER(player, i), GET_DRUG_TOLERANCE_LEVEL(player, i), GET_DRUG_LAST_WITHDRAWAL_TICK(player, i),
                          GET_DRUG_DURATION(player, i), GET_DRUG_DOSE(player, i), GET_DRUG_STAGE(player, i)
                        );
      q = 1;
    }
  }
  if (q) {
    strcat(buf, ");");
    mysql_wrapper(mysql, buf);
  }
}

void save_skills_to_db(struct char_data *player) {
  PERF_PROF_SCOPE(pr_, __func__);
  snprintf(buf, sizeof(buf), "DELETE FROM pfiles_skills WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  strcpy(buf, "INSERT INTO pfiles_skills (idnum, skillnum, `rank`) VALUES (");
  int q = 0;
  for (int i = MIN_SKILLS; i < MAX_SKILLS; i++) {
    if (GET_RAW_SKILL(player, i)) {
      if (q)
        strcat(buf, "), (");
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld, %d, %d", GET_IDNUM(player), i, GET_RAW_SKILL(player, i));
      q = 1;
    }
  }
  if (q) {
    strcat(buf, ");");
    mysql_wrapper(mysql, buf);
  }
}

void save_aliases_to_db(struct char_data *player) {
  PERF_PROF_SCOPE(pr_, __func__);
  snprintf(buf, sizeof(buf), "DELETE FROM pfiles_alias WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, buf);
  strcpy(buf, "INSERT INTO pfiles_alias (idnum, command, replacement) VALUES (");
  int q = 0;
  char cmd_buf[500];
  for (struct alias *a = GET_ALIASES(player); a; a = a->next) {
    if (q)
      strcat(buf, "), (");
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld, '%s', '%s'",
             GET_IDNUM(player),
             prepare_quotes(cmd_buf, a->command, sizeof(cmd_buf) / sizeof(cmd_buf[0])),
             prepare_quotes(buf3, a->replacement, sizeof(buf3) / sizeof(buf3[0])));
    q = 1;
  }
  if (q) {
    strcat(buf, ");");
    mysql_wrapper(mysql, buf);
  }
}

void save_inventory_to_db(struct char_data *player) {
  PERF_PROF_SCOPE(pr_, __func__);
  int level = 0, posi = 0;

  // Deliberately shadow external scope's 'buf' with a larger character buffer.
  char *query_buf = new char[CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE]; // this is almost 10 megabytes of text. If they exceed it, wiping their inventory is fine by me.

  // Blow away their existing inventory. Too much can change between savings for us to try to modify in place.
  snprintf(query_buf, CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE, "DELETE FROM pfiles_inv WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, query_buf);

  // Compose the first part of our bulk save string.
  assert(NUM_OBJ_VALUES == 18); // If this assertion tripped, you need to rewrite object saving here, then UPDATE the assertion.
  strlcpy(query_buf, "INSERT INTO pfiles_inv ("
               "idnum, Vnum, Cost, Restring, Photo, graffiti, Inside, Timer, ExtraFlags, Attempt, Cond, posi, obj_idnum,"
               "Value0, Value1, Value2, Value3, Value4, Value5, Value6, Value7, Value8, Value9, Value10, Value11, Value12, Value13, Value14, Value15, Value16, Value17"
               ") VALUES ",
          CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE);

  bool wrote_anything = FALSE;
  for (struct obj_data *obj = player->carrying; obj;) {
    if (!IS_OBJ_STAT(obj, ITEM_EXTRA_NORENT)) {
      snprintf(ENDOF(query_buf), CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE - strlen(query_buf),
               //, id   vn   co   re    ph    gr   in  ti   ex   at  cd  po  oid
               "%s(%ld, %ld, %d, '%s', '%s', '%s', %d, %d, '%s', %d, %d, %d, %lu,"
               // object values 0-17
               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)",
               wrote_anything ? "," : "",
               GET_IDNUM(player),
               GET_OBJ_VNUM(obj),
               GET_OBJ_COST(obj),
               obj->restring ? prepare_quotes(buf3, obj->restring, sizeof(buf3) / sizeof(buf3[0])) : "",
               obj->photo ? prepare_quotes(buf2, obj->photo, sizeof(buf2) / sizeof(buf2[0])) : "",
               obj->graffiti ? prepare_quotes(buf4, obj->graffiti, sizeof(buf4) / sizeof(buf4[0])) : "",
               level,
               GET_OBJ_TIMER(obj),
               GET_OBJ_EXTRA(obj).ToString(),
               GET_OBJ_ATTEMPT(obj),
               GET_OBJ_CONDITION(obj),
               posi++,
               GET_OBJ_IDNUM(obj),
               // I hate this, but having it prewritten saves 18 string concatenations _per object_ compared to doing it in a forloop.
               GET_OBJ_VAL(obj, 0),
               GET_OBJ_VAL(obj, 1),
               GET_OBJ_VAL(obj, 2),
               GET_OBJ_VAL(obj, 3),
               GET_OBJ_VAL(obj, 4),
               GET_OBJ_VAL(obj, 5),
               GET_OBJ_VAL(obj, 6),
               GET_OBJ_VAL(obj, 7),
               GET_OBJ_VAL(obj, 8),
               GET_OBJ_VAL(obj, 9),
               GET_OBJ_VAL(obj, 10),
               GET_OBJ_VAL(obj, 11),
               GET_OBJ_VAL(obj, 12),
               GET_OBJ_VAL(obj, 13),
               GET_OBJ_VAL(obj, 14),
               GET_OBJ_VAL(obj, 15),
               GET_OBJ_VAL(obj, 16),
               GET_OBJ_VAL(obj, 17));
      wrote_anything = TRUE;

      // Continue down into nested objects. Don't continue into parts though, since those use the contains variable weirdly.
      if (obj->contains && GET_OBJ_TYPE(obj) != ITEM_PART) {
        obj = obj->contains;
        level++;
        continue;
      }
    }

    // If we've hit the end of this object's linked list, and it's in an object, back out until we hit the top.
    if (!obj->next_content && obj->in_obj) {
      while (obj && !obj->next_content && level >= 0) {
        obj = obj->in_obj;
        level--;
      }
    }

    if (obj)
      obj = obj->next_content;
  }

  // Finally, execute our query.
  if (wrote_anything) {
    if (mysql_wrapper(mysql, query_buf)) {
      if (strlen(query_buf) >= CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE - 2) {
        mudlog_vfprintf(player, LOG_SYSLOG, "SYSERR: Failed to save inventory for %s due to having too much crap.", GET_CHAR_NAME(player));
        send_to_char(player, "^RWARNING: You have so much inventory that the game is unable to save your profile. Sell or junk nested items immediately to avoid loss of data.^n\r\n");
      } else {
        mudlog_vfprintf(player, LOG_SYSLOG, "SYSERR: Failed to save inventory for %s due to database error. Page Lucien to diagnose.", GET_CHAR_NAME(player));
      }
    }
  }
}

void save_worn_equipment_to_db(struct char_data *player) {
  PERF_PROF_SCOPE(pr_, __func__);
  struct obj_data *obj = NULL;
  int level = 0, posi = 0, i = 0;

  char *query_buf = new char[CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE]; // this is almost 10 megabytes of text. If they exceed it, wiping their inventory is fine by me.

  // Blow away their existing gear. Too much can change between savings for us to try to modify in place.
  snprintf(query_buf, CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE, "DELETE FROM pfiles_worn WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, query_buf);

  // Compose the first part of our bulk save string.
  assert(NUM_OBJ_VALUES == 18); // If this assertion tripped, you need to rewrite object saving here, then UPDATE the assertion.
  strlcpy(query_buf, "INSERT INTO pfiles_worn ("
               "idnum, Vnum, Cost, Restring, Photo, graffiti, Inside, Position, Timer, ExtraFlags, Attempt, Cond, posi, obj_idnum,"
               "Value0, Value1, Value2, Value3, Value4, Value5, Value6, Value7, Value8, Value9, Value10, Value11, Value12, Value13, Value14, Value15, Value16, Value17"
               ") VALUES ",
          CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE);

  // Iterate to the first saveable worn item they have.
  for (i = 0; i < NUM_WEARS; i++) {
    if ((obj = GET_EQ(player, i)) && !IS_OBJ_STAT(obj, ITEM_EXTRA_NORENT))
      break;
  }

  bool wrote_anything = FALSE;
  while (obj && i < NUM_WEARS) {
    // Only save things that are not NORENT. Despite being written into the forloop, this is necessary to cover NORENT items _in_ worn things.
    if (!IS_OBJ_STAT(obj, ITEM_EXTRA_NORENT)) {
      snprintf(ENDOF(query_buf), CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE - strlen(query_buf), 
              //   id vn  co  re   ph   gr  in po ti exf  at co ps oid
              "%s(%ld,%ld,%d,'%s','%s','%s',%d,%d,%d,'%s',%d,%d,%d,%lu,"
              // object values 0-17
              "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)",
              wrote_anything ? "," : "", // comma separator for subsequent items
              GET_IDNUM(player),
              GET_OBJ_VNUM(obj),
              GET_OBJ_COST(obj),
              obj->restring ? prepare_quotes(buf3, obj->restring, sizeof(buf3) / sizeof(buf3[0])) : "",
              obj->photo ? prepare_quotes(buf2, obj->photo, sizeof(buf2) / sizeof(buf2[0])) : "",
              obj->graffiti ? prepare_quotes(buf4, obj->graffiti, sizeof(buf4) / sizeof(buf4[0])) : "",
              level,
              i,
              GET_OBJ_TIMER(obj),
              GET_OBJ_EXTRA(obj).ToString(),
              GET_OBJ_ATTEMPT(obj),
              GET_OBJ_CONDITION(obj),
              posi++,
              GET_OBJ_IDNUM(obj),
              // I hate this, but having it prewritten saves 18 string concatenations _per object_ compared to doing it in a forloop.
              GET_OBJ_VAL(obj, 0),
              GET_OBJ_VAL(obj, 1),
              GET_OBJ_VAL(obj, 2),
              GET_OBJ_VAL(obj, 3),
              GET_OBJ_VAL(obj, 4),
              GET_OBJ_VAL(obj, 5),
              GET_OBJ_VAL(obj, 6),
              GET_OBJ_VAL(obj, 7),
              GET_OBJ_VAL(obj, 8),
              GET_OBJ_VAL(obj, 9),
              GET_OBJ_VAL(obj, 10),
              GET_OBJ_VAL(obj, 11),
              GET_OBJ_VAL(obj, 12),
              GET_OBJ_VAL(obj, 13),
              GET_OBJ_VAL(obj, 14),
              GET_OBJ_VAL(obj, 15),
              GET_OBJ_VAL(obj, 16),
              GET_OBJ_VAL(obj, 17));
      wrote_anything = TRUE;

      // Since this is a rentable object, we want to dive into the contents if they exist.
      // Never dive into ITEM_PART, since it can point to a cyberdeck that's not part of this list.
      if (obj->contains && GET_OBJ_TYPE(obj) != ITEM_PART) {
        obj = obj->contains;
        level++;
        continue;
      }
    }

    // If we're saving contents and have hit the end of our contents, we want to bounce up a level.
    if (!obj->next_content && obj->in_obj) {
      while (obj && !obj->next_content && level >= 0) {
        obj = obj->in_obj;
        level--;
      }
    }

    // If we've exhausted this wearloc, move on to the next one.
    if (!obj || !obj->next_content)
      while (i < NUM_WEARS) {
        i++;
        if ((obj = GET_EQ(player, i)) && !IS_OBJ_STAT(obj, ITEM_EXTRA_NORENT)) {
          level = 0;
          break;
        }
      }
    else
      obj = obj->next_content;
  }

  // Finally, execute our query.
  if (wrote_anything) {
    if (mysql_wrapper(mysql, query_buf)) {
      if (strlen(query_buf) >= CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE - 2) {
        mudlog_vfprintf(player, LOG_SYSLOG, "SYSERR: Failed to save equipment for %s due to having too much crap.", GET_CHAR_NAME(player));
        send_to_char(player, "^RWARNING: You have so much equipment that the game is unable to save your profile. Sell or junk nested items immediately to avoid loss of data.^n\r\n");
      } else {
        mudlog_vfprintf(player, LOG_SYSLOG, "SYSERR: Failed to save equipment for %s due to database error. Page Lucien to diagnose.", GET_CHAR_NAME(player));
      }
    }
  }
}

void save_bioware_to_db(struct char_data *player) {
  PERF_PROF_SCOPE(pr_, __func__);
  
  char *query_buf = new char[CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE]; // this is almost 10 megabytes of text. If they exceed it, wiping their inventory is fine by me.

  // Blow away their existing bioware.
  snprintf(query_buf, CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE, "DELETE FROM pfiles_bioware WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, query_buf);

  if (!player->bioware)
    return;

  // Compose the first part of our bulk save string.
  assert(NUM_OBJ_VALUES == 18); // If this assertion tripped, you need to rewrite object saving here, then UPDATE the assertion.
  strlcpy(query_buf,
          "INSERT INTO pfiles_bioware ("
          "idnum, Vnum, Cost, Restring, graffiti, obj_idnum,"
          "Value0, Value1, Value2, Value3, Value4, Value5, Value6, Value7, Value8, Value9, Value10, Value11, Value12, Value13, Value14, Value15, Value16, Value17"
          ") VALUES ",
          CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE);

  bool wrote_anything = FALSE;
  for (struct obj_data *obj = player->bioware; obj; obj = obj->next_content) {
    if (!IS_OBJ_STAT(obj, ITEM_EXTRA_NORENT)) {
      snprintf(ENDOF(query_buf), CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE - strlen(query_buf),
               //  id  vn  co  re   gr  oid
               "%s(%ld,%ld,%d,'%s','%s',%lu,"
               // object values 0-17
              "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)",
               wrote_anything ? "," : "",
               GET_IDNUM(player),
               GET_OBJ_VNUM(obj),
               GET_OBJ_COST(obj),
               obj->restring ? prepare_quotes(buf3, obj->restring, sizeof(buf3) / sizeof(buf3[0])) : "",
               obj->graffiti ? prepare_quotes(buf4, obj->graffiti, sizeof(buf4) / sizeof(buf4[0])) : "",
               GET_OBJ_IDNUM(obj),
               // I hate this, but having it prewritten saves 18 string concatenations _per object_ compared to doing it in a forloop.
               GET_OBJ_VAL(obj, 0),
               GET_OBJ_VAL(obj, 1),
               GET_OBJ_VAL(obj, 2),
               GET_OBJ_VAL(obj, 3),
               GET_OBJ_VAL(obj, 4),
               GET_OBJ_VAL(obj, 5),
               GET_OBJ_VAL(obj, 6),
               GET_OBJ_VAL(obj, 7),
               GET_OBJ_VAL(obj, 8),
               GET_OBJ_VAL(obj, 9),
               GET_OBJ_VAL(obj, 10),
               GET_OBJ_VAL(obj, 11),
               GET_OBJ_VAL(obj, 12),
               GET_OBJ_VAL(obj, 13),
               GET_OBJ_VAL(obj, 14),
               GET_OBJ_VAL(obj, 15),
               GET_OBJ_VAL(obj, 16),
               GET_OBJ_VAL(obj, 17));
      wrote_anything = TRUE;
    }
  }
  // Finally, execute our query.
  if (mysql_wrapper(mysql, query_buf)) {
    mudlog_vfprintf(player, LOG_SYSLOG, "SYSERR: Failed to save bioware for %s due to database error. (Do they have too much crap?)", GET_CHAR_NAME(player));
  }

  delete [] query_buf;
}

void save_cyberware_to_db(struct char_data *player) {
  PERF_PROF_SCOPE(pr_, __func__);
  int level = 0, posi = 0;

  char *query_buf = new char[CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE]; // this is almost 10 megabytes of text. If they exceed it, wiping their inventory is fine by me.

  // Blow away their existing gear. Too much can change between savings for us to try to modify in place.
  snprintf(query_buf, CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE, "DELETE FROM pfiles_cyberware WHERE idnum=%ld", GET_IDNUM(player));
  mysql_wrapper(mysql, query_buf);

  if (!player->cyberware)
    return;

  // Compose the first part of our bulk save string.
  assert(NUM_OBJ_VALUES == 18); // If this assertion tripped, you need to rewrite object saving here, then UPDATE the assertion.
  strlcpy(query_buf, "INSERT INTO pfiles_cyberware ("
               "idnum, Vnum, Cost, Restring, Photo, graffiti, Level, posi, obj_idnum,"
               "Value0, Value1, Value2, Value3, Value4, Value5, Value6, Value7, Value8, Value9, Value10, Value11, Value12, Value13, Value14, Value15, Value16, Value17"
               ") VALUES ",
          CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE);

  bool wrote_anything = FALSE;
  for (struct obj_data *obj = player->cyberware; obj;) {
    // Only save things that are not NORENT. Despite being written into the forloop, this is necessary to cover NORENT items _in_ worn things.
    if (!IS_OBJ_STAT(obj, ITEM_EXTRA_NORENT)) {
      snprintf(ENDOF(query_buf), CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE - strlen(query_buf), 
              //   id vn  co  re   ph   gr  lv po oid
              "%s(%ld,%ld,%d,'%s','%s','%s',%d,%d,%lu,"
              // object values 0-17
              "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)",
              wrote_anything ? "," : "", // comma separator for subsequent items
              GET_IDNUM(player),
              GET_OBJ_VNUM(obj),
              GET_OBJ_COST(obj),
              obj->restring ? prepare_quotes(buf3, obj->restring, sizeof(buf3) / sizeof(buf3[0])) : "",
              obj->photo ? prepare_quotes(buf2, obj->photo, sizeof(buf2) / sizeof(buf2[0])) : "",
              obj->graffiti ? prepare_quotes(buf4, obj->graffiti, sizeof(buf4) / sizeof(buf4[0])) : "",
              level,
              posi++,
              GET_OBJ_IDNUM(obj),
              // I hate this, but having it prewritten saves 18 string concatenations _per object_ compared to doing it in a forloop.
              GET_OBJ_VAL(obj, 0),
              GET_OBJ_VAL(obj, 1),
              GET_OBJ_VAL(obj, 2),
              GET_OBJ_VAL(obj, 3),
              GET_OBJ_VAL(obj, 4),
              GET_OBJ_VAL(obj, 5),
              GET_OBJ_VAL(obj, 6),
              GET_OBJ_VAL(obj, 7),
              GET_OBJ_VAL(obj, 8),
              GET_OBJ_VAL(obj, 9),
              GET_OBJ_VAL(obj, 10),
              GET_OBJ_VAL(obj, 11),
              GET_OBJ_VAL(obj, 12),
              GET_OBJ_VAL(obj, 13),
              GET_OBJ_VAL(obj, 14),
              GET_OBJ_VAL(obj, 15),
              GET_OBJ_VAL(obj, 16),
              GET_OBJ_VAL(obj, 17));
      wrote_anything = TRUE;

      // Since this is a rentable object, we want to dive into the contents if they exist.
      // Never dive into ITEM_PART, since it can point to a cyberdeck that's not part of this list.
      if (obj->contains && GET_OBJ_TYPE(obj) != ITEM_PART) {
        obj = obj->contains;
        level++;
        continue;
      }
    }

    // If we've hit the end of this object's linked list, and it's in an object, back out until we hit the top.
    if (!obj->next_content && obj->in_obj) {
      while (obj && !obj->next_content && level >= 0) {
        obj = obj->in_obj;
        level--;
      }
    }

    if (obj)
      obj = obj->next_content;
  }

  // Finally, execute our query.
  if (mysql_wrapper(mysql, query_buf)) {
    if (strlen(query_buf) >= CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE) {
      mudlog_vfprintf(player, LOG_SYSLOG, "SYSERR: Failed to save cyberware for %s due to having too much crap.", GET_CHAR_NAME(player));
      send_to_char(player, "^RWARNING: You have so much cyberware that the game is unable to save your profile. Clear out your headware memory immediately to avoid loss of data.^n\r\n");
    } else {
      mudlog_vfprintf(player, LOG_SYSLOG, "SYSERR: Failed to save cyberware for %s due to database error. Page Lucien to diagnose.", GET_CHAR_NAME(player));
    }
  }
}

void fix_ghoul_index(struct char_data *ch) {
  // Only apply to ghouls who haven't had this done yet.
  if (!IS_GHOUL(ch) || PLR_FLAGGED(ch, PLR_RECEIVED_GHOUL_INDEX_DELTA))
    return;

  if (GET_TRADITION(ch) != TRAD_MUNDANE) {
    // Calculate magic refund amount. Bioware is half magic.
    int delta = GET_HIGHEST_INDEX(ch) / 2;
    int magic_refund_amount = delta / 2;
    
    // Apply it. Refund PP as well if they're adepts.
    GET_SETTABLE_REAL_MAG(ch) += magic_refund_amount;
    if (GET_TRADITION(ch) == TRAD_ADEPT) {
      GET_PP(ch) += magic_refund_amount;
    }
  }

  mudlog_vfprintf(ch, LOG_SYSLOG, "Ghoul bioware index refund: Index %d -> %d, highest %d -> %d.", GET_INDEX(ch), GET_INDEX(ch) / 2, GET_HIGHEST_INDEX(ch), GET_HIGHEST_INDEX(ch) / 2);

  GET_INDEX(ch) /= 2;
  GET_HIGHEST_INDEX(ch) /= 2;

  PLR_FLAGS(ch).SetBit(PLR_RECEIVED_GHOUL_INDEX_DELTA);

  raw_store_mail(GET_IDNUM(ch), 0, "System Message (OOC)", 
                 "Good news: You have been refunded half of your bioware index and highest bioware index rating."
                 " If you're a mage/adept, you've regained the relevant amount of magic and powerpoints."
                 " Going forward, ghouls will no longer pay double for bioware index costs.");
}

/* Because we've changed the essence cost of cybereyes, we need to refund the difference to people. */
void fix_character_essence_after_cybereye_migration(struct char_data *ch) {
  int old_essence_cost, new_essence_cost, essence_delta;

  // First, check for the flag. If it's set, we already did this-- skip.
  if (PLR_FLAGGED(ch, PLR_RECEIVED_CYBEREYE_ESSENCE_DELTA))
    return;

  // Next, ensure that we won't be sending them below 0 essence. If we would, log and abort.
  {
    int total_essence_delta = 0;

    struct obj_data fake_cyber;

    // Total up all the expected changes.
    for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content) {
      if (GET_CYBERWARE_TYPE(obj) == CYB_EYES) {
        // Construct a fake cyberware item, then price it. This gives us an item that reflects the new values.
        GET_CYBERWARE_TYPE(&fake_cyber) = GET_CYBERWARE_TYPE(obj);
        GET_CYBERWARE_FLAGS(&fake_cyber) = GET_CYBERWARE_FLAGS(obj);
        GET_CYBERWARE_GRADE(&fake_cyber) = GET_CYBERWARE_GRADE(obj);
        price_cyber(&fake_cyber);

        old_essence_cost = get_deprecated_cybereye_essence_cost(ch, obj);
        new_essence_cost = fake_cyber.obj_flags.value[4];
        essence_delta = old_essence_cost - new_essence_cost;
        total_essence_delta += essence_delta;
      }
    }

    // Deduct their essence hole from the total.
    total_essence_delta -= GET_ESSHOLE(ch);

    // If there's a remainder after essence hole, ensure it won't kill them or wipe their magic.
    if (total_essence_delta > 0) {
      if (GET_REAL_ESS(ch) + total_essence_delta > GET_RACIAL_STARTING_ESSENCE_FOR_RACE(GET_RACE(ch))) {
        snprintf(buf, sizeof(buf), "%s refusing to perform cybereye rectification: it would put me above %d essence!", capitalize(GET_CHAR_NAME(ch)), (int) (GET_RACIAL_STARTING_ESSENCE_FOR_RACE(GET_RACE(ch)) / 100));
        mudlog(buf, ch, LOG_SYSLOG, TRUE);
        return;
      }
    }
    else if (total_essence_delta < 0) {
      if (GET_TRADITION(ch) != TRAD_MUNDANE && GET_REAL_MAG(ch) + total_essence_delta < 100) {
        snprintf(buf, sizeof(buf), "%s refusing to perform cybereye rectification: it would put me below 1 magic!", capitalize(GET_CHAR_NAME(ch)));
        mudlog(buf, ch, LOG_SYSLOG, TRUE);
        return;
      }

      if (GET_REAL_ESS(ch) + total_essence_delta <= 0) {
        snprintf(buf, sizeof(buf), "%s refusing to perform cybereye rectification: it would put me at or below 0 essence!", capitalize(GET_CHAR_NAME(ch)));
        mudlog(buf, ch, LOG_SYSLOG, TRUE);
        return;
      }
    }
  }

  // Otherwise, perform all the expected changes.
  {
    int total_magic_delta = 0, total_essence_delta = 0, total_esshole_delta = 0;

    for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content) {
      if (GET_CYBERWARE_TYPE(obj) == CYB_EYES) {
        // Update the item.
        price_cyber(obj);

        // Calculate the old values and delta.
        old_essence_cost = get_deprecated_cybereye_essence_cost(ch, obj);
        new_essence_cost = calculate_ware_essence_or_index_cost(ch, obj);
        essence_delta = old_essence_cost - new_essence_cost;

        // If there are changes to make, write a log entry so we can trace things later.
        if (essence_delta != 0) {
          snprintf(buf, sizeof(buf), "Processing %s's cybereye migration for %s (%ld): Delta %d.", GET_CHAR_NAME(ch), GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj), essence_delta);
          mudlog(buf, ch, LOG_SYSLOG, TRUE);
        } else {
          snprintf(buf, sizeof(buf), "Processing %s's cybereye migration for %s (%ld): No delta.", GET_CHAR_NAME(ch), GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj));
          mudlog(buf, ch, LOG_SYSLOG, TRUE);
          // Otherwise, just skip this one.
          continue;
        }

        if (essence_delta > 0) {
          // The new one costs less than the old one did. Refund the essence.
          total_essence_delta += essence_delta;
          GET_REAL_ESS(ch) += essence_delta;

          // Next, handle magic restoration if needed.
          if (GET_TRADITION(ch) != TRAD_MUNDANE) {
            total_magic_delta += essence_delta;
            GET_SETTABLE_REAL_MAG(ch) += essence_delta;
          }
        }

        else if (essence_delta < 0) {
          // The new one costs more than the old one did. Whoops...
          int ess_cost_after_esshole = 0;

          // Make the delta positive for better readability.
          essence_delta *= -1;

          // If their essence hole can cover it, deduct from that alone.
          if (GET_ESSHOLE(ch) >= essence_delta) {
            total_esshole_delta -= essence_delta;
            GET_ESSHOLE(ch) -= essence_delta;
          }
          // Otherwise, we have to pull from real essence too. Now we're dealing with magic loss.
          else {
            ess_cost_after_esshole = essence_delta - GET_ESSHOLE(ch);

            // Wipe their esshole for them. We here at Awakened Worlds use only the finest 2-ply code for this process.
            total_esshole_delta -= GET_ESSHOLE(ch);
            GET_ESSHOLE(ch) = 0;

            // Deduct the remaining cost from their essence.
            total_essence_delta -= essence_delta;
            GET_REAL_ESS(ch) -= essence_delta;

            // Cause magic loss, BUT not in the standard way. Instead of stripping powers etc, we just reduce their magic stat.
            if (GET_TRADITION(ch) != TRAD_MUNDANE) {
              total_magic_delta -= ess_cost_after_esshole;
              GET_SETTABLE_REAL_MAG(ch) -= ess_cost_after_esshole;
            }
          }
        }
      }
    } /* end for loop */

    // Message them if they had a change.
    if (total_magic_delta != 0 || total_essence_delta != 0 || total_esshole_delta != 0) {
      send_to_char(ch, "^WSYSTEM NOTICE:^n Cybereye essence costs were fixed, and your character was included in the cleanup process. The following changes have been automatically made to your character to true things up:\r\n"
                       "^W- Essence Hole:^c %d^n\r\n"
                       "^W- Essence:^c %d^n\r\n"
                       "^W- Magic:^c %d^n\r\n", total_esshole_delta, total_essence_delta, total_magic_delta);

      // Write a log, too.
      snprintf(buf, sizeof(buf), "Post-trueup deltas for %s: EH %d, ES %d, MG %d.", GET_CHAR_NAME(ch), total_esshole_delta, total_essence_delta, total_magic_delta);
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    }
  }

  // Flag them so they won't go through this again.
  PLR_FLAGS(ch).SetBit(PLR_RECEIVED_CYBEREYE_ESSENCE_DELTA);

  // Finally, save them. TODO: Does saving them in the middle of the load process break things?
  save_char(ch, GET_LOADROOM(ch));
}

extern struct obj_data *shop_package_up_ware(struct obj_data *obj);
void uninstall_and_refund_ware(struct char_data *ch, struct obj_data *ware, char *msg_buf, size_t msg_buf_sz) {
  if (!ch || !ware)
    return;

  // We refund their essence / index as well.
  int essence_cost = calculate_ware_essence_or_index_cost(ch, ware);

  // And of course, magic.
  int magic_refund_amount = 0;

  // Remove cyberware.
  if (GET_OBJ_TYPE(ware) == ITEM_CYBERWARE) {
    obj_from_cyberware(ware);

    // Sanity checks.
    if (GET_REAL_ESS(ch) + essence_cost > 600) {
      int delta = GET_REAL_ESS(ch) + essence_cost - 600;
      essence_cost -= delta;
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: While uninstalling and refunding %s (%ld), we went over the ess limit by %d! Capped essence back down.",
                      GET_OBJ_NAME(ware),
                      GET_OBJ_VNUM(ware),
                      delta);
    }

    // Refund essence. Leave esshole untouched.
    GET_REAL_ESS(ch) += essence_cost;

    // Calculate magic refund amount. Cyberware is full magic.
    magic_refund_amount = essence_cost;
  } 
  // Remove bioware.
  else if (GET_OBJ_TYPE(ware) == ITEM_BIOWARE) {
    obj_from_bioware(ware);

    // Sanity checks.
    if (GET_INDEX(ch) + essence_cost > 900) {
      int delta = GET_INDEX(ch) + essence_cost - 900;
      essence_cost -= delta;
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: While uninstalling and refunding %s (%ld), we went over the ess limit by %d! Capped essence back down.",
                      GET_OBJ_NAME(ware),
                      GET_OBJ_VNUM(ware),
                      delta);
    }

    // Refund index and reduce max index.
    GET_INDEX(ch) -= essence_cost;
    GET_HIGHEST_INDEX(ch) -= essence_cost;

    // Calculate magic refund amount. Bioware is half magic.
    magic_refund_amount = essence_cost / 2;
  }

  // Restore magic and add back lost PP.
  GET_SETTABLE_REAL_MAG(ch) += magic_refund_amount;
  if (GET_TRADITION(ch) == TRAD_ADEPT) {
    GET_PP(ch) += magic_refund_amount;
  }

  // We want to refund the cost of installation to the character. Currently, that's 10%.
  int nuyen_refund = get_cyberware_install_cost(ware);
  GET_NUYEN_RAW(ch) += nuyen_refund;
    
  // Hand it off in a package.
  obj_to_char(shop_package_up_ware(ware), ch);

  // Compose message. Caller to send.
  snprintf(ENDOF(msg_buf), msg_buf_sz - strlen(buf), " - %s: refunded %.2f %s, %.2f magic%s, and %d nuyen to cover installation fees\r\n", 
           decapitalize_a_an(ware),
           (float) essence_cost / 100,
           GET_OBJ_TYPE(ware) == ITEM_CYBERWARE ? "essence" : "index and highestindex",
           (float) magic_refund_amount / 100,
           GET_TRADITION(ch) == TRAD_ADEPT ? " and powerpoints" : "",
           nuyen_refund);

  // Log it.
  mudlog_vfprintf(ch, LOG_SYSLOG, "Expert Driver Patch refund: %s^n (%ld) refunds %.2f %s, %.2f magic%s, and %d nuyen",
           decapitalize_a_an(ware),
           GET_OBJ_VNUM(ware),
           (float) essence_cost / 100,
           GET_OBJ_TYPE(ware) == ITEM_CYBERWARE ? "essence" : "index and highestindex",
           (float) magic_refund_amount / 100,
           GET_TRADITION(ch) == TRAD_ADEPT ? " and powerpoints" : "",
           nuyen_refund);
}

#define UNINSTALL_ALL_WARE(ware_type) while ((ware = find_cyberware(ch, ware_type))) { uninstall_and_refund_ware(ch, ware, buf, sizeof(buf)); changed_anything = TRUE; }
/* Because we've changed the essence cost of cybereyes, we need to refund the difference to people. */
void fix_character_essence_after_expert_driver_change(struct char_data *ch) {
  // First, check for the flag. If it's set, we already did this-- skip.
  if (PLR_FLAGGED(ch, PLR_COMPLETED_EXPERT_DRIVER_OVERHAUL))
    return;

  PLR_FLAGS(ch).SetBit(PLR_COMPLETED_EXPERT_DRIVER_OVERHAUL);

  // Mundanes are not affected by this change.
  if (GET_TRADITION(ch) == TRAD_MUNDANE) {
    return;
  }

  strlcpy(buf, "^WSYSTEM NOTICE:^n The functionality of Chipjack Expert Drivers has been changed,"
               " making them less valuable in certain builds. To ensure this doesn't break your"
               " build, we have uninstalled the following 'ware and placed it in your inventory:\r\n", sizeof(buf));

  bool changed_anything = FALSE;
  struct obj_data *ware = NULL;

  // Remove chipjacks, unjacking in the process.
  while ((ware = find_cyberware(ch, CYB_CHIPJACK))) {
    for (struct obj_data *chip = ware->contains, *next_obj; chip; chip = next_obj) {
      next_obj = chip->next_content;
      deactivate_single_skillsoft(chip, ch, FALSE);
      obj_from_obj(chip);
      obj_to_char(chip, ch);
    }

    uninstall_and_refund_ware(ch, ware, buf, sizeof(buf));
    changed_anything = TRUE;
  }

  // Remove skillwires and expert drivers.
  UNINSTALL_ALL_WARE(CYB_SKILLWIRE);
  UNINSTALL_ALL_WARE(CYB_CHIPJACKEXPERT);
  
  if (changed_anything) {
    raw_store_mail(GET_IDNUM(ch), 0, "(OOC System Message)", buf);
  }

  // Finally, save them.
  save_char(ch, GET_LOADROOM(ch));
}

// Check if a PC has a specified DB tag applied to them.
bool player_has_db_tag(idnum_t idnum, const char *tag_name) {
  char prepare_quotes_buf[1000];
  char query_buf[2000];
  bool result;
  
  MYSQL_RES *res;
  MYSQL_ROW row;

  snprintf(query_buf, sizeof(query_buf), "SELECT idnum FROM pfiles_named_tags WHERE idnum=%ld AND tag_name='%s';", 
           idnum,
           prepare_quotes(prepare_quotes_buf, tag_name, sizeof(prepare_quotes_buf) / sizeof(prepare_quotes_buf[0])));

  mysql_wrapper(mysql, query_buf);

  if (!(res = mysql_use_result(mysql))) {
    result = FALSE;
  } else if (!(row = mysql_fetch_row(res)) && mysql_field_count(mysql)) {
    result = FALSE;
  } else {
    result = TRUE;
  }
  
  mysql_free_result(res);
  return result;
}

void set_db_tag(idnum_t idnum, const char *tag_name) {
  char prepare_quotes_buf[1000];
  char query_buf[2000];

  snprintf(query_buf, sizeof(query_buf),  "INSERT IGNORE INTO pfiles_named_tags (idnum, tag_name) VALUES (%ld, '%s')",
           idnum,
           prepare_quotes(prepare_quotes_buf, tag_name, sizeof(prepare_quotes_buf) / sizeof(prepare_quotes_buf[0])));
  
  mysql_wrapper(mysql, query_buf);
}

void remove_db_tag(idnum_t idnum, const char *tag_name) {
  char prepare_quotes_buf[1000];
  char query_buf[2000];

  snprintf(query_buf, sizeof(query_buf),  "DELETE FROM pfiles_named_tags WHERE idnum=%ld AND tag_name='%s'",
           idnum,
           prepare_quotes(prepare_quotes_buf, tag_name, sizeof(prepare_quotes_buf) / sizeof(prepare_quotes_buf[0])));
  
  mysql_wrapper(mysql, query_buf);
}

void save_all_pcs() {
  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    if (d->character && !IS_NPC(d->character)) {
      log_vfprintf("Saving %s (%ld).", GET_CHAR_NAME(d->character), GET_IDNUM(d->character));
      save_char(d->character, GET_LOADROOM(d->character));
    }

    if (d->original && !IS_NPC(d->original)) {
      log_vfprintf("Saving %s (%ld).", GET_CHAR_NAME(d->original), GET_IDNUM(d->original));
      save_char(d->original, GET_LOADROOM(d->original));
    }
  }
  log("Finished saving all PCs.");
}

void verify_db_max_packet_constraints() {
  unsigned long max_allowed_packet_dest = 0;

  // Check at the session level.
  if (mysql_get_option(mysql, MYSQL_OPT_MAX_ALLOWED_PACKET, &max_allowed_packet_dest)) {
    log("Failed to get max allowed packet from session-- bailing out.");
    exit(ERROR_MYSQL_MAX_ALLOWED_PACKET_CHECK_FAILURE);
  }

  if (max_allowed_packet_dest == 0) {
    // Check at the global level.
    if (mysql_get_option(NULL, MYSQL_OPT_MAX_ALLOWED_PACKET, &max_allowed_packet_dest)) {
      log("Failed to get max allowed packet from global values-- bailing out.");
      exit(ERROR_MYSQL_MAX_ALLOWED_PACKET_CHECK_FAILURE);
    }

    if (max_allowed_packet_dest < CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE) {
      log_vfprintf("MySQL max allowed packet size is too low at the global level at %lu. Please ensure it's at least %lu.", max_allowed_packet_dest, CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE);
      exit(ERROR_MYSQL_MAX_ALLOWED_PACKET_CHECK_FAILURE);
    }
  } else {
    if (max_allowed_packet_dest < CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE) {
      log_vfprintf("MySQL max allowed packet size is too low at the session level at %lu. Please ensure it's at least %lu.", max_allowed_packet_dest, CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE);
      exit(ERROR_MYSQL_MAX_ALLOWED_PACKET_CHECK_FAILURE);
    }
  }
  
  
}

void recalculate_character_magic_rating(struct char_data *ch) {
  if (GET_SETTABLE_REAL_MAG(ch) <= 0)
    return;

  // todo: set it to their race's base mag

  // remove any mag loss from installed cyberware

  // remove any mag loss from essence hole

  // remove any mag loss from bioware

  // remove any mag loss from bio hole
}

// Call this to convert pfiles_inv and pfiles_worn pocket secretary files to DB entries.
void migrate_pocket_secretaries_in_database() {
  // todo
}