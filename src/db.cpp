/* ************************************************************************
*   File: db.c                                          Part of CircleMUD *
*  Usage: Loading/saving chars, booting/resetting world, internal funcs   *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define __DB_CC__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <iomanip>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include <mysql/mysql.h>
#include <map>
#include <algorithm>

#if defined(WIN32) && !defined(__CYGWIN__)
#include <process.h>
#define getpid() _getpid()
#else
#include <unistd.h>
#endif

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
namespace bf = boost::filesystem;

#ifndef NOCRYPT
#include <sodium.h>
#endif

/* mysql_config.h must be filled out with your own connection info. */
/* For obvious reasons, DO NOT ADD THIS FILE TO SOURCE CONTROL AFTER CUSTOMIZATION. */
#include "mysql_config.hpp"

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "file.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "comm.hpp"
#include "handler.hpp"
#include "interpreter.hpp"
#include "house.hpp"
#include "newmatrix.hpp"
#include "memory.hpp"
#include "dblist.hpp"
#include "quest.hpp"
#include "newshop.hpp"
#include "constants.hpp"
#include "vtable.hpp"
#include "config.hpp"
#include "security.hpp"
#include "olc.hpp"
#include <new>
#include "transport.hpp"
#include "bullet_pants.hpp"
#include "lexicons.hpp"
#include "newhouse.hpp"
#include "zoomies.hpp"
#include "redit.hpp"
#include "vehicles.hpp"
#include "matrix_storage.hpp"

ACMD_DECLARE(do_reload);

extern void calc_weight(struct char_data *ch);
extern void read_spells(struct char_data *ch);
extern struct obj_data *find_obj(int num);
extern void add_phone_to_list(struct obj_data *);
extern void idle_delete();
extern void clearMemory(struct char_data * ch);
extern void generate_archetypes();
extern void populate_mobact_aggression_octets();
extern void handle_weapon_attachments(struct obj_data *obj);
extern void ensure_mob_has_ammo_for_weapon(struct char_data *ch, struct obj_data *weapon);
extern void reset_host_paydata(rnum_t rnum);
extern bool player_is_dead_hardcore(long id);
extern void load_apartment_complexes();
extern void parse_factions();
extern void initialize_policy_tree();
extern void initialize_traffic_msgs();

extern void auto_repair_obj(struct obj_data *obj, idnum_t owner);

// transport.cpp
extern void boot_escalators();

extern int max_weapon_focus_rating;


/**************************************************************************
*  declarations of most of the 'global' variables                         *
************************************************************************ */

bool _OVERRIDE_ALLOW_PLAYERS_TO_USE_ROLLS_ = FALSE;

// beginning of mud time
unsigned long beginning_of_time = 2157880000U;
long mud_boot_time = 0;
struct room_data *world = NULL; /* array of rooms   */
rnum_t top_of_world = 0; /* ref to top element of world  */

// A random number that changes once on each boot. Used for non-sensitive purposes.
int global_non_secure_random_number = dice(1, 100000);

struct host_data *matrix = NULL;
rnum_t top_of_matrix = 0;
struct matrix_icon *ic_proto;
struct index_data *ic_index;
rnum_t top_of_ic = 0;
struct matrix_icon *icon_list = NULL;

struct char_data *character_list = NULL; /* global linked list of chars  */
struct index_data *mob_index; /* index table for mobile file  */
struct char_data *mob_proto; /* prototypes for mobs   */
rnum_t top_of_mobt = 0; /* top of mobile index table  */
int mob_chunk_size = 250;       // default to 100
int top_of_mob_array = 0;

struct index_data *obj_index; /* index table for object file  */
struct obj_data *obj_proto; /* prototypes for objs   */
rnum_t top_of_objt = 0; /* top of object index table  */
int top_of_obj_array = 0;
int obj_chunk_size = 250;       // default to 100

struct zone_data *zone_table; /* zone table    */
rnum_t top_of_zone_table = 0;/* top element of zone tab  */
struct message_list fight_messages[MAX_MESSAGES]; /* fighting messages  */

struct quest_data *quest_table = NULL; // array of quests
rnum_t top_of_questt = 0;
int top_of_quest_array = 0;
int quest_chunk_size = 50;

struct shop_data *shop_table = NULL;   // array of shops
rnum_t top_of_shopt = 0;            // ref to top element of shop_table
int top_of_shop_array = 0;
int shop_chunk_size = 50;

int top_of_matrix_array = 0;
int top_of_ic_array = 0;
int top_of_world_array = 0;
int world_chunk_size = 250;     /* size of world to add on each reallocation */
int olc_state = 1;              /* current olc state */
int _NO_OOC_  = 0;  /* Disable the OOC Channel */

struct veh_data *veh_list;
struct index_data *veh_index;
struct veh_data *veh_proto;
int top_of_veht = 0;
int top_of_veh_array = 0;
int veh_chunk_size = 100;

class objList ObjList;          // contains the global list of perm objects

time_t boot_time = 0;           /* time of mud boot              */
int restrict_mud = 0;           /* level of game restriction     */
rnum_t r_mortal_start_room;     /* rnum of mortal start room     */
rnum_t r_immort_start_room;     /* rnum of immort start room     */
rnum_t r_frozen_start_room;     /* rnum of frozen start room     */
rnum_t r_newbie_start_room;     /* rnum of newbie start room     */

char *credits = NULL;           /* game credits                  */
char *news = NULL;              /* mud news                      */
char *motd = NULL;              /* message of the day - mortals */
char *imotd = NULL;             /* message of the day - immorts */
char *help = NULL;              /* help screen                   */
char *info = NULL;              /* info page                     */
char *immlist = NULL;           /* list of lower gods            */
char *background = NULL;        /* background story              */
char *handbook = NULL;          /* handbook for new immortals    */
char *policies = NULL;          /* policies page                 */

struct time_info_data time_info;/* the infomation about the time    */
struct weather_data weather_info;       /* the infomation about the weather */
struct player_special_data dummy_mob;   /* dummy spec area for mobs      */
struct phone_data *phone_list = NULL;
int market[5] = { 5000, 5000, 5000, 5000, 5000 };

MYSQL *mysql;

/* local functions */
void setup_dir(FILE * fl, int room, int dir);
void index_boot(int mode);
void discrete_load(File &fl, int mode);
void parse_room(File &in, long nr);
void parse_mobile(File &in, long nr);
void parse_object(File &in, long nr);
void parse_shop(File &fl, long virtual_nr);
void parse_quest(File &fl, long virtual_nr);
void parse_host(File &fl, long nr);
void parse_ic(File &fl, long nr);
void load_zones(File &fl);
void purge_unowned_vehs();
void load_saved_veh(bool);
void assign_mobiles(void);
void assign_objects(void);
void assign_rooms(void);
void assign_shopkeepers(void);
void assign_johnsons(void);
void randomize_shop_prices(void);
int zone_is_empty(int zone_nr);
void reset_zone(int zone, int reboot);
int file_to_string(const char *name, char *buf, size_t buf_size);
int file_to_string_alloc(const char *name, char **buf);
void check_start_rooms(void);
void renum_world(void);
void renum_zone_table(void);
void log_zone_error(int zone, int cmd_no, const char *message);
void reset_time(void);
void clear_char(struct char_data * ch);
void kill_ems(char *);
void parse_veh(File &fl, long virtual_nr);
/* external functions */
extern struct descriptor_data *descriptor_list;
void load_messages(void);
void weather_and_time(int mode);
void boot_social_messages(void);
void sort_commands(void);
void load_banned(void);
void init_quests(void);
extern void TransportInit();
extern void BoardInit();
extern void affect_total(struct char_data * ch);
void load_consist(void);
void boot_shop_orders(void);
void price_cyber(struct obj_data *obj);
void price_bio(struct obj_data *obj);
extern void verify_db_password_column_size();
extern void verify_matrix_data_file_storage();
extern void init_matrix_data_file_index();
void set_elemental_races();
void initialize_and_alphabetize_flag_maps();
void set_up_pet_dummy_mob();

/* external vars */
extern int no_specials;
/* external ascii pfile vars */
extern const char *pc_race_types[];
/* memory objects */
extern class memoryClass *Mem;
/*************************************************************************
*  routines for booting the system                                       *
*********************************************************************** */

ACMD(do_reboot)
{
  one_argument(argument, arg);

  if (!str_cmp(arg, "all") || *arg == '*') {
    file_to_string_alloc(NEWS_FILE, &news);
    file_to_string_alloc(CREDITS_FILE, &credits);
    file_to_string_alloc(MOTD_FILE, &motd);
    file_to_string_alloc(IMOTD_FILE, &imotd);
    file_to_string_alloc(INFO_FILE, &info);
    file_to_string_alloc(IMMLIST_FILE, &immlist);
    file_to_string_alloc(POLICIES_FILE, &policies);
    file_to_string_alloc(HANDBOOK_FILE, &handbook);
    file_to_string_alloc(BACKGROUND_FILE, &background);
    file_to_string_alloc(HELP_KWRD_FILE, &help);
  } else if (!str_cmp(arg, "immlist"))
    file_to_string_alloc(IMMLIST_FILE, &immlist);
  else if (!str_cmp(arg, "news"))
    file_to_string_alloc(NEWS_FILE, &news);
  else if (!str_cmp(arg, "credits"))
    file_to_string_alloc(CREDITS_FILE, &credits);
  else if (!str_cmp(arg, "motd"))
    file_to_string_alloc(MOTD_FILE, &motd);
  else if (!str_cmp(arg, "imotd"))
    file_to_string_alloc(IMOTD_FILE, &imotd);
  else if (!str_cmp(arg, "info"))
    file_to_string_alloc(INFO_FILE, &info);
  else if (!str_cmp(arg, "policy"))
    file_to_string_alloc(POLICIES_FILE, &policies);
  else if (!str_cmp(arg, "handbook"))
    file_to_string_alloc(HANDBOOK_FILE, &handbook);
  else if (!str_cmp(arg, "background"))
    file_to_string_alloc(BACKGROUND_FILE, &background);
  else {
    send_to_char("Unknown reboot option.\r\n", ch);
    return;
  }

  send_to_char(OK, ch);
}

void initialize_and_connect_to_mysql() {
  // Set up our mysql client object.
  mysql = mysql_init(NULL);

  // Configure the client to attempt auto-reconnection.
  bool reconnect = 1;
  mysql_options(mysql, MYSQL_OPT_RECONNECT, &reconnect);

  // Perform the actual connection.
  if (!mysql_real_connect(mysql, mysql_host, mysql_user, mysql_password, mysql_db, 0, NULL, 0)) {
    snprintf(buf, sizeof(buf), "FATAL ERROR: %s\r\n", mysql_error(mysql));
    log(buf);
    log("Suggestion: Make sure your DB is running and that you've specified your connection info in src/mysql_config.cpp.\r\n");
    exit(ERROR_MYSQL_DATABASE_NOT_FOUND);
  }
}

void check_for_common_fuckups() {
  // Check for invalid taxi destinations. Meaningless maximum 10k chosen here.
  for (int i = 0; i < 10000; i++) {
    if (seattle_taxi_destinations[i].vnum <= 0)
      break;

    if (real_room(seattle_taxi_destinations[i].vnum) == NOWHERE) {
      snprintf(buf, sizeof(buf), "ERROR: Seattle taxi destination '%s' (%ld) does not exist.",
               seattle_taxi_destinations[i].keyword,
               seattle_taxi_destinations[i].vnum);
      log(buf);
      seattle_taxi_destinations[i].enabled = FALSE;
    }
  }

  for (int i = 0; i < 10000; i++) {
    if (portland_taxi_destinations[i].vnum <= 0)
      break;

    if (real_room(portland_taxi_destinations[i].vnum) == NOWHERE) {
      snprintf(buf, sizeof(buf), "ERROR: Portland taxi destination '%s' (%ld) does not exist.",
               portland_taxi_destinations[i].keyword,
               portland_taxi_destinations[i].vnum);
      log(buf);
      portland_taxi_destinations[i].enabled = FALSE;
    }
  }

  for (int i = 0; i < 10000; i++) {
    if (caribbean_taxi_destinations[i].vnum <= 0)
      break;

    if (real_room(caribbean_taxi_destinations[i].vnum) == NOWHERE) {
      snprintf(buf, sizeof(buf), "ERROR: Caribbean taxi destination '%s' (%ld) does not exist.",
               caribbean_taxi_destinations[i].keyword,
               caribbean_taxi_destinations[i].vnum);
      log(buf);
      caribbean_taxi_destinations[i].enabled = FALSE;
    }
  }
}

// Kills the game if the table of the given name does not exist.
void require_that_sql_table_exists(const char *table_name, const char *migration_path_from_root_directory) {
  bool have_table = FALSE;

  MYSQL_RES *res;
  MYSQL_ROW row;

  char query_buf[1000];
  snprintf(query_buf, sizeof(query_buf), "SHOW TABLES LIKE '%s';", prepare_quotes(buf, table_name, sizeof(buf)));
  mysql_wrapper(mysql, query_buf);

  if (!(res = mysql_use_result(mysql))) {
    log_vfprintf("ERROR: You need to run a migration to add the %s table. "
                 "Probable syntax from your AwakeMUD directory: `mysql -u YOUR_USERNAME -p %s < %s`.",
                 table_name,
                 mysql_db,
                 migration_path_from_root_directory);
    exit(ERROR_DB_TABLE_REQUIRED);
  }

  if ((row = mysql_fetch_row(res)) && mysql_field_count(mysql))
    have_table = TRUE;

  mysql_free_result(res);

  if (!have_table) {
    log_vfprintf("ERROR: You need to run a migration to add the %s table. "
                 "Probable syntax from your AwakeMUD directory: `mysql -u YOUR_USERNAME -p %s < %s`.",
                 table_name,
                 mysql_db,
                 migration_path_from_root_directory);
    exit(ERROR_DB_TABLE_REQUIRED);
  }
}

// Combines the logic of field_exists_in_table with more constraints-- for things you've recently updated but already existed.
void require_that_field_meets_constraints(const char *field_name, const char *table_name, const char *migration_path_from_root_directory, int flength=0, const char *ftype=NULL, bool is_unsigned=FALSE) {
  bool have_column = FALSE;
  char migration_string[3000];

  MYSQL_RES *res;
  MYSQL_ROW row;

  char query_buf[1000];

  snprintf(migration_string, sizeof(migration_string),
           "ERROR: You need to run a migration to add or modify %s.%s. "
           "Probable syntax from your AwakeMUD directory: `mysql -u YOUR_USERNAME -p %s < %s`.",
           table_name,
           field_name,
           mysql_db,
           migration_path_from_root_directory);

  snprintf(query_buf, sizeof(query_buf), "SHOW COLUMNS FROM %s LIKE '%s';",
           prepare_quotes(buf, table_name, sizeof(buf)),
           prepare_quotes(buf2, field_name, sizeof(buf2)));
  mysql_wrapper(mysql, query_buf);

  if (!(res = mysql_use_result(mysql))) {
    log(migration_string);
    exit(ERROR_DB_COLUMN_REQUIRED);
  }

  if ((row = mysql_fetch_row(res)) && mysql_field_count(mysql)) {
    have_column = TRUE;

    // Length constraint.
    if (flength) {
      if (ftype == NULL || !*ftype) {
        log("ERROR: You need to include the field type in require_that_field_meets_constraints when using the length parameter.");
        exit(ERROR_CODER_DIDNT_SPECIFY_FIELD_TYPE);
      }

      char field_type[500];
      snprintf(field_type, sizeof(field_type), "%s(%d)%s", ftype, flength, is_unsigned ? " unsigned" : "");
      if (strcmp(field_type, row[1])) {
        log_vfprintf("%s\r\n%s.%s's type '%s' did not match expected type '%s'.",
                     migration_string,
                     table_name,
                     field_name,
                     row[1],
                     field_type);
        mysql_free_result(res);
        exit(ERROR_DB_COLUMN_REQUIRED);
      }
    }
  }

  mysql_free_result(res);

  if (!have_column) {
    log(migration_string);
    exit(ERROR_DB_COLUMN_REQUIRED);
  }
}

void require_that_field_exists_in_table(const char *field_name, const char *table_name, const char *migration_path_from_root_directory) {
  require_that_field_meets_constraints(field_name, table_name, migration_path_from_root_directory);
}

void boot_world(void)
{
  // Pre-boot tests and other things you'd like to run in the context of the game.
  //   Write your tests here, then uncomment the exit statement if all you want are those results.
  // exit(EXIT_CODE_ZERO_ALL_IS_WELL);

  // Sanity check to ensure we haven't added more bits than our bitfield can hold.
  if (Bitfield::TotalWidth() < PRF_MAX) {
    log("Error: You have more PRF flags defined than bitfield space. You'll need to either expand the size of bitfields or reduce your flag count.");
    exit(ERROR_BITFIELD_SIZE_EXCEEDED);
  }

  if (Bitfield::TotalWidth() < PLR_MAX) {
    log("Error: You have more PLR flags defined than bitfield space. You'll need to either expand the size of bitfields or reduce your flag count.");
    exit(ERROR_BITFIELD_SIZE_EXCEEDED);
  }

  if (Bitfield::TotalWidth() < AFF_MAX) {
    log("Error: You have more AFF flags defined than bitfield space. You'll need to either expand the size of bitfields or reduce your flag count.");
    exit(ERROR_BITFIELD_SIZE_EXCEEDED);
  }

  if (Bitfield::TotalWidth() < ROOM_MAX) {
    log("Error: You have more ROOM flags defined than bitfield space. You'll need to either expand the size of bitfields or reduce your flag count.");
    exit(ERROR_BITFIELD_SIZE_EXCEEDED);
  }

  if (MAX_PROTOCOL_BUFFER > MAX_RAW_INPUT_LENGTH) {
    log("Error: Your maximum protocol buffer exceeds your input length buffer, so there's a risk of overflow.");
    exit(ERROR_PROTOCOL_BUFFER_EXCEEDS_INPUT_LENGTH);
  }

  if (RAND_MAX < 2147483647) { // aka the maximum value for `long int`
    log_vfprintf("SERIOUS WARNING: Your platform's RAND_MAX is %lld, which may cause collisions with object and vehicle idnums. Recommend refactoring the code to use a more advanced random generator.", RAND_MAX);
  } else {
    log_vfprintf("RAND_MAX is %lld.", RAND_MAX);
  }

  log("Checking to see if you added an ammo type and forgot to add it to npc_ammo_usage_preferences[]...");
  for (int i = 0; i < NUM_AMMOTYPES; i++) {
    assert(npc_ammo_usage_preferences[i] >= AMMO_NORMAL && npc_ammo_usage_preferences[i] < NUM_AMMOTYPES);
  }

  log("Checking to see if you added a new flag and forgot to add it to constants.cpp...");
  for (int i = 0; i < MOB_MAX; i++) {
    if (str_str(action_bits[i], MAX_FLAG_MARKER)) {
      log("Error: You added a mob flag but didn't add it to action_bits in constants.cpp (or forgot a comma)!");
      exit(ERROR_FLAG_CONSTANT_MISSING);
    }
  }

  for (int i = 0; i < AFF_MAX; i++) {
    if (str_str(affected_bits[i], MAX_FLAG_MARKER)) {
      log("Error: You added an aff flag but didn't add it to affected_bits in constants.cpp (or forgot a comma)!");
      exit(ERROR_FLAG_CONSTANT_MISSING);
    }
  }

  for (int i = 0; i < PLR_MAX; i++) {
    if (str_str(player_bits[i], MAX_FLAG_MARKER)) {
      log("Error: You added a plr flag but didn't add it to player_bits in constants.cpp (or forgot a comma)!");
      exit(ERROR_FLAG_CONSTANT_MISSING);
    }
  }

  for (int i = 0; i < PRF_MAX; i++) {
    if (str_str(preference_bits[i], MAX_FLAG_MARKER)) {
      log("Error: You added a prf flag but didn't add it to preference_bits in constants.cpp (or forgot a comma)!");
      exit(ERROR_FLAG_CONSTANT_MISSING);
    }
  }

  for (int i = 0; i < CON_MAX; i++) {
    if (str_str(connected_types[i], MAX_FLAG_MARKER)) {
      log("Error: You added a connection type but didn't add it to connected_types in constants.cpp (or forgot a comma)!");
      exit(ERROR_FLAG_CONSTANT_MISSING);
    }
  }

  for (int i = 0; i < ROOM_MAX; i++) {
    if (str_str(room_bits[i], MAX_FLAG_MARKER)) {
      log("Error: You added a room flag but didn't add it to room_bits in constants.cpp (or forgot a comma)!");
      exit(ERROR_FLAG_CONSTANT_MISSING);
    }

    if (str_str(room_flag_explanations[i], MAX_FLAG_MARKER)) {
      log("Error: You added a room flag but didn't add it to room_flag_explanations in constants.cpp (or forgot a comma)!");
      exit(ERROR_FLAG_CONSTANT_MISSING);
    }
  }

  for (int i = 0; i < MAX_ITEM_EXTRA; i++) {
    if (str_str(extra_bits[i], MAX_FLAG_MARKER)) {
      log("Error: You added an extra bit but didn't add it to extra_bits in constants.cpp (or forgot a comma)! Don't forget pc_readable_extra_bits too.");
      exit(ERROR_FLAG_CONSTANT_MISSING);
    }
    if (str_str(pc_readable_extra_bits[i], MAX_FLAG_MARKER)) {
      log("Error: You added an extra bit but didn't add it to pc_readable_extra_bits in constants.cpp (or forgot a comma)!");
      exit(ERROR_FLAG_CONSTANT_MISSING);
    }
  }

  for (int i = 0; i < NUM_JURISDICTIONS; i++) {
    if (str_str(jurisdictions[i], MAX_FLAG_MARKER)) {
      log("Error: You added another jurisdiction but didn't add it to jurisdictions in constants.cpp (or forgot a comma)!");
      exit(ERROR_FLAG_CONSTANT_MISSING);
    }
  }

#ifndef NOCRYPT
  log("Initializing libsodium for crypto functions.");
  if (sodium_init() < 0) {
    // The library could not be initialized. Fail.
    log("ERROR: Libsodium initialization failed. Terminating program.");
    exit(ERROR_LIBSODIUM_INIT_FAILED);
  }
#endif

#ifdef CRYPTO_DEBUG
  log("Performing crypto performance and validation tests.");
  run_crypto_tests();
#endif

  log("Booting MYSQL database.");
  initialize_and_connect_to_mysql();

  log("Verifying DB compatibility with extended-length passwords.");
  verify_db_password_column_size();

  // Search terms below because it always takes me forever to ctrl-f this block -LS
  // ensure table, ensure row, ensure field, database, limits, restrictions
  log("Verifying that DB has expected migrations. Note that not all migrations are checked here.");
  require_that_sql_table_exists("pfiles_ammo", "SQL/bullet_pants.sql");
  require_that_sql_table_exists("command_fuckups", "SQL/fuckups.sql");
  require_that_field_exists_in_table("socialbonus", "pfiles", "SQL/Migrations/socialize.sql");
  require_that_field_exists_in_table("archetype", "pfiles_chargendata", "SQL/Migrations/archetypes.sql");
  require_that_field_exists_in_table("archetypal", "pfiles_chargendata", "SQL/Migrations/archetypes.sql");
  require_that_field_exists_in_table("highlight", "pfiles", "SQL/Migrations/rp_upgrade.sql");
  require_that_field_exists_in_table("email", "pfiles", "SQL/Migrations/rp_upgrade.sql");
  require_that_field_exists_in_table("multiplier", "pfiles", "SQL/Migrations/multipliers.sql");
  require_that_field_exists_in_table("Restring", "pfiles_bioware", "SQL/Migrations/bioware_restrings.sql");
  require_that_field_meets_constraints("Prompt", "pfiles", "SQL/Migrations/prompt_expansion.sql", 2001, "varchar");
  require_that_field_meets_constraints("MatrixPrompt", "pfiles", "SQL/Migrations/prompt_expansion.sql", 2001, "varchar");
  #ifdef USE_MYSQL_8
  require_that_field_meets_constraints("Attempt", "pfiles_inv", "SQL/Migrations/attempt_value_fix.sql", 0, "mediumint");
  require_that_field_meets_constraints("Attempt", "pfiles_worn", "SQL/Migrations/attempt_value_fix.sql", 0, "mediumint");
  require_that_field_meets_constraints("LastFix", "pfiles_drugs", "SQL/Migrations/lastfix_bigint.sql", 0, "bigint", TRUE);
  #else
  require_that_field_meets_constraints("Attempt", "pfiles_inv", "SQL/Migrations/attempt_value_fix.sql", 6, "mediumint");
  require_that_field_meets_constraints("Attempt", "pfiles_worn", "SQL/Migrations/attempt_value_fix.sql", 6, "mediumint");
  require_that_field_meets_constraints("LastFix", "pfiles_drugs", "SQL/Migrations/lastfix_bigint.sql", 12, "bigint", TRUE);
  #endif
  require_that_sql_table_exists("pfiles_ignore_v2", "SQL/ignore_system_v2.sql");
  require_that_field_exists_in_table("harmless", "pfiles_ammo", "SQL/Migrations/add_harmless_and_anti_vehicle.sql");
  require_that_field_exists_in_table("anti-vehicle", "pfiles_ammo", "SQL/Migrations/add_harmless_and_anti_vehicle.sql");
  require_that_field_exists_in_table("Duration", "pfiles_drugs", "SQL/Migrations/drug_overhaul.sql");
  require_that_field_exists_in_table("lifestyle_string", "pfiles", "SQL/Migrations/lifestyles.sql");
  require_that_field_exists_in_table("zone", "playergroups", "SQL/Migrations/playergroup_zone_ownership.sql");
  require_that_field_exists_in_table("completed", "pfiles_quests", "SQL/Migrations/quest_completed_fix.sql");
  require_that_field_exists_in_table("graffiti", "pfiles_worn", "SQL/Migrations/add_graffiti_field.sql");
  require_that_field_exists_in_table("prestige_alt", "pfiles_chargendata", "SQL/Migrations/prestige_races.sql");
  require_that_sql_table_exists("pfiles_named_tags", "SQL/Migrations/add_named_tags.sql");
  require_that_field_exists_in_table("Value14", "pfiles_inv", "SQL/Migrations/obj_idnums_and_vals.sql");
  require_that_sql_table_exists("pfiles_factions", "SQL/Migrations/add_factions.sql");
  require_that_sql_table_exists("pfiles_exdescs", "SQL/Migrations/add_exdescs.sql");
  require_that_field_exists_in_table("otaku_path", "pfiles", "SQL/Migrations/add_otaku.sql");
  require_that_sql_table_exists("pfiles_echoes", "SQL/Migrations/add_otaku_echoes.sql");
  require_that_sql_table_exists("matrix_files", "SQL/Migrations/add_matrix_storage.sql");

  {
    const char *object_tables[4] = {
      "pfiles_inv",
      "pfiles_worn",
      "pfiles_bioware",
      "pfiles_cyberware"
    };
    char valbuf[20];
    snprintf(valbuf, sizeof(valbuf), "Value%d", NUM_OBJ_LOADS);
    for (int idx = 0; idx < 4; idx++)
      require_that_field_exists_in_table(valbuf, object_tables[idx], "(meta check: no specific file)");
  }

  log("Wiping unused matrix data file rows.");
  verify_matrix_data_file_storage();

  log("Initializing matrix data file store index.");
  init_matrix_data_file_index();

  log("Calculating lexicon data.");
  populate_lexicon_size_table();

  log("Handling idle deletion.");
  idle_delete();

  log("Loading policies.");
  initialize_policy_tree();

  log("Loading factions.");
  parse_factions();

  log("Loading zone table.");
  index_boot(DB_BOOT_ZON);

  log("Loading rooms.");
  index_boot(DB_BOOT_WLD);

  log("Checking start rooms.");
  check_start_rooms();

  log("Loading objs and generating index.");
  index_boot(DB_BOOT_OBJ);

  log("Loading mobs and generating index.");
  index_boot(DB_BOOT_MOB);

  log("Handling special-case mobs.");
  set_elemental_races();

  log("Loading vehicles and generating index.");
  index_boot(DB_BOOT_VEH);

  log("Loading quests.");
  index_boot(DB_BOOT_QST);

  log("Loading shops.");
  index_boot(DB_BOOT_SHP);

  log("Loading Matrix Hosts.");
  index_boot(DB_BOOT_MTX);

  log("Collating Matrix Host Entrances.");
  collate_host_entrances();

  log("Loading IC.");
  index_boot(DB_BOOT_IC);

  log("Converting exit vnums to rnums.");
  renum_world();

  log("Converting zone table vnums to rnums.");
  renum_zone_table();

  // log("Creating Help Indexes.");
  // TODO: Is this supposed to actually do anything?

  log("Performing final validation checks.");
  check_for_common_fuckups();

  {
    // Cheeky little self-test. Requires that RM_ENTRANCE_TO_DANTES exists and is flagged ROAD/GARAGE and not NOGRID.
    long x = get_room_gridguide_x(RM_ENTRANCE_TO_DANTES);
    long y = get_room_gridguide_y(RM_ENTRANCE_TO_DANTES);
    vnum_t z = vnum_from_gridguide_coordinates(x, y, NULL);
    if (z != RM_ENTRANCE_TO_DANTES)
      log_vfprintf("x: %ld, y: %ld, z: %ld", x, y, z);
    assert(z == RM_ENTRANCE_TO_DANTES);
  }
}

/* body of the booting system */
void DBInit()
{
  int i;

  log("DBInit -- BEGIN.");

  log("Resetting the game time:");
  reset_time();

  log("Reading news, credits, help, bground, info & motds.");
  file_to_string_alloc(NEWS_FILE, &news);
  file_to_string_alloc(CREDITS_FILE, &credits);
  file_to_string_alloc(MOTD_FILE, &motd);
  file_to_string_alloc(IMOTD_FILE, &imotd);
  file_to_string_alloc(INFO_FILE, &info);
  file_to_string_alloc(IMMLIST_FILE, &immlist);
  file_to_string_alloc(POLICIES_FILE, &policies);
  file_to_string_alloc(HANDBOOK_FILE, &handbook);
  file_to_string_alloc(BACKGROUND_FILE, &background);
  file_to_string_alloc(HELP_KWRD_FILE, &help);

  log("Loading fight messages.");
  load_messages();

  log("Loading lifestyles.");
  load_lifestyles();

  log("Booting world.");
  boot_world();
  initialize_traffic_msgs();
  initialize_and_alphabetize_flag_maps();
  set_up_pet_dummy_mob();

  log("Loading social messages.");
  boot_social_messages();

  log("Loading player index.");
  playerDB.Load();

  log("Generating character creation archetypes.");
  generate_archetypes();

  log("Assigning function pointers:");
  if (!no_specials) {
    log("   Mobiles.");
    assign_mobiles();
    log("   Johnsons.");
    assign_johnsons();
    log("   Shopkeepers.");
    assign_shopkeepers();
    randomize_shop_prices();
    log("   Objects.");
    assign_objects();
    log("   Rooms.");
    assign_rooms();
  }

  log("Sorting command list.");
  sort_commands();

  log("Initializing board system:");
  BoardInit();

  log("Reading banned site list.");
  load_banned();
  log("Reloading consistency files.");
  load_consist();

  log("Initializing transportation system");
  TransportInit();

  log_vfprintf("Resetting %ld zones.", top_of_zone_table + 1);
  for (i = 0; i <= top_of_zone_table; i++) {
    log_vfprintf("Resetting %s (rooms %d-%d).", zone_table[i].name,
        (i ? (zone_table[i - 1].top + 1) : 0), zone_table[i].top);
    reset_zone(i, 1);
    // log_vfprintf("Reset of %s complete. Writing to disk...", zone_table[i].name);
    extern void write_zone_to_disk(int vnum);
    write_zone_to_disk(zone_table[i].number);
    // log("Written.");
  }

  log("Booting houses.");
  load_apartment_complexes();
  warn_about_apartment_deletion();
  boot_time = time(0);

  log("Loading saved vehicles.");
  load_saved_veh(FALSE);

  log("Purging unowned vehicles.");
  purge_unowned_vehs();

  log("Loading shop orders.");
  boot_shop_orders();

  log("Setting up mobact aggression octets.");
  populate_mobact_aggression_octets();

  log("Building escalator vector.");
  boot_escalators();

  log("DBInit -- DONE.");
}

/* A simple method to clean up after our DB. */
void DBFinalize() {
  if (mysql)
    mysql_close(mysql);
}

/* reset the time in the game from file
   weekday is lost on reboot.... implement
   something different if this is mission
   breaking behavior */
void reset_time(void)
{
  extern struct time_info_data mud_time_passed(time_t t2, time_t t1);

  mud_boot_time = time(0);
  time_info = mud_time_passed(beginning_of_time, time(0));

  if (time_info.year < 2064)
    time_info.year = 2064;
  time_info.minute = 0;
  time_info.weekday = 0;

  if (time_info.hours <= 5)
    weather_info.sunlight = SUN_DARK;
  else if (time_info.hours == 6)
    weather_info.sunlight = SUN_RISE;
  else if (time_info.hours <= 18)
    weather_info.sunlight = SUN_LIGHT;
  else if (time_info.hours == 19)
    weather_info.sunlight = SUN_SET;
  else
    weather_info.sunlight = SUN_DARK;

  log_vfprintf("   Current Gametime: %d/%d/%d %d:00.", time_info.month,
      time_info.day, time_info.year,
      (time_info.hours % 12) == 0 ? 12 : time_info.hours % 12);

  weather_info.pressure = 960;
  if ((time_info.month >= 7) && (time_info.month <= 12))
    weather_info.pressure += dice(1, 50);
  else
    weather_info.pressure += dice(1, 80);

  if(time_info.day < 7)
    weather_info.moonphase = MOON_NEW;
  else if(time_info.day > 7 && time_info.day < 14)
    weather_info.moonphase = MOON_WAX;
  else if(time_info.day > 14 && time_info.day < 21)
    weather_info.moonphase = MOON_FULL;
  else
    weather_info.moonphase = MOON_WANE;

  weather_info.change = 0;

  if (weather_info.pressure <= 980)
    weather_info.sky = SKY_LIGHTNING;
  else if (weather_info.pressure <= 1000)
    weather_info.sky = SKY_RAINING;
  else if (weather_info.pressure <= 1020)
    weather_info.sky = SKY_CLOUDY;
  else
    weather_info.sky = SKY_CLOUDLESS;
  weather_info.lastrain = 10;
}

/* function to count how many hash-mark delimited records exist in a file */
int count_hash_records(FILE * fl)
{
  char buf[128];
  int count = 0;

  while (fgets(buf, 128, fl))
    if (*buf == '#')
      count++;

  return count;
}

void index_boot(int mode)
{
  const char *index_filename, *prefix;
  FILE *index, *db_file;
  int rec_count = 0;

  switch (mode) {
  case DB_BOOT_WLD:
    prefix = WLD_PREFIX;
    break;
  case DB_BOOT_MOB:
    prefix = MOB_PREFIX;
    break;
  case DB_BOOT_OBJ:
    prefix = OBJ_PREFIX;
    break;
  case DB_BOOT_ZON:
    prefix = ZON_PREFIX;
    break;
  case DB_BOOT_SHP:
    prefix = SHP_PREFIX;
    break;
  case DB_BOOT_QST:
    prefix = QST_PREFIX;
    break;
  case DB_BOOT_VEH:
    prefix = VEH_PREFIX;
    break;
  case DB_BOOT_IC:
  case DB_BOOT_MTX:
    prefix = MTX_PREFIX;
    break;
  default:
    log("SYSERR: Unknown subcommand to index_boot!");
    exit(ERROR_UNKNOWN_SUBCOMMAND_TO_INDEX_BOOT);
    break;
  }

  if (mode == DB_BOOT_IC)
    index_filename = ICINDEX_FILE;
  else
    index_filename = INDEX_FILE;

  snprintf(buf2, sizeof(buf2), "%s/%s", prefix, index_filename);

  if (!(index = fopen(buf2, "r"))) {
    snprintf(buf1, sizeof(buf1), "Error opening index file '%s'", buf2);
    perror(buf1);
    exit(ERROR_OPENING_INDEX_FILE);
  }
  /* first, count the number of records in the file so we can calloc */
  fscanf(index, "%32767s\n", buf1);
  while (*buf1 != '$') {
    snprintf(buf2, sizeof(buf2), "%s/%s", prefix, buf1);
    if (!(db_file = fopen(buf2, "r"))) {
      snprintf(buf, sizeof(buf), "Unable to find file %s.", buf2);
      mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    } else {
      if (mode == DB_BOOT_ZON)
        rec_count++;
      else
        rec_count += count_hash_records(db_file);
      fclose(db_file);
    }

    fscanf(index, "%32767s\n", buf1);
  }
  if (!rec_count) {
    log("SYSERR: boot error - 0 records counted");
    exit(ERROR_BOOT_ZERO_RECORDS_COUNTED);
  }
  rec_count++;

  switch (mode) {
  case DB_BOOT_WLD:
    // here I am booting with 100 extra slots for creation
    world = new struct room_data[rec_count + world_chunk_size];
    memset((char *) world, 0, (sizeof(struct room_data) *
                               (rec_count + world_chunk_size)));
#ifdef USE_DEBUG_CANARIES
   for (int i = 0; i < rec_count + world_chunk_size; i++)
     world[i].canary = CANARY_VALUE;
#endif

    top_of_world_array = rec_count + world_chunk_size; // assign the real size of the array
    break;
  case DB_BOOT_MTX:
    // here I am booting with 100 extra slots for creation
    matrix = new struct host_data[rec_count + world_chunk_size];
    memset((char *) matrix, 0, (sizeof(struct host_data) *
                                (rec_count + world_chunk_size)));
    top_of_matrix_array = rec_count + world_chunk_size; // assign the real size of the array
    break;
  case DB_BOOT_IC:
    // here I am booting with 100 extra slots for creation
    ic_proto = new struct matrix_icon[rec_count + mob_chunk_size];
    memset((char *) ic_proto, 0, (sizeof(struct matrix_icon) *
                                  (rec_count + mob_chunk_size)));
    ic_index = new struct index_data[rec_count + mob_chunk_size];
    memset((char *) ic_index, 0, (sizeof(struct index_data) *
                                  (rec_count + mob_chunk_size)));
    top_of_ic_array = rec_count + mob_chunk_size; // assign the real size of the array
    break;
  case DB_BOOT_MOB:
    // here I am booting with 100 extra slots for creation
    mob_proto = new struct char_data[rec_count + mob_chunk_size];
    // memset((char *) mob_proto, 0, (sizeof(struct char_data) * (rec_count + mob_chunk_size)));

#ifdef USE_DEBUG_CANARIES
  for (int i = 0; i < rec_count + mob_chunk_size; i++)
    mob_proto[i].canary = CANARY_VALUE;
#endif

    mob_index = new struct index_data[rec_count + mob_chunk_size];
    memset((char *) mob_index, 0, (sizeof(struct index_data) *
                                   (rec_count + mob_chunk_size)));

    top_of_mob_array = rec_count + mob_chunk_size; // assign the real size of the array
    break;
  case DB_BOOT_OBJ:
    // here I am booting with 100 extra slots for creation
    obj_proto = new struct obj_data[rec_count + obj_chunk_size];
    memset((char *) obj_proto, 0, (sizeof(struct obj_data) *
                                   (rec_count + obj_chunk_size)));

#ifdef USE_DEBUG_CANARIES
    for (int i = 0; i < rec_count + obj_chunk_size; i++)
      obj_proto[i].canary = CANARY_VALUE;
#endif

    obj_index = new struct index_data[rec_count + obj_chunk_size];
    memset((char *) obj_index, 0, (sizeof(struct index_data) *
                                   (rec_count + obj_chunk_size)));

    top_of_obj_array = rec_count + obj_chunk_size;
    break;
  case DB_BOOT_VEH:
    veh_proto = new struct veh_data[rec_count + veh_chunk_size];
    memset((char *) veh_proto, 0, (sizeof(struct veh_data) *
                                   (rec_count + veh_chunk_size)));
    veh_index = new struct index_data[rec_count + veh_chunk_size];
    memset((char *) veh_index, 0, (sizeof(struct index_data) * (rec_count + veh_chunk_size)));
    top_of_veh_array = rec_count + veh_chunk_size;

#ifdef USE_DEBUG_CANARIES
    for (int i = 0; i < rec_count + veh_chunk_size; i++)
      veh_proto[i].canary = CANARY_VALUE;
#endif
    break;

  case DB_BOOT_ZON:
    // the zone table is pretty small, so it is no biggie
    zone_table = new struct zone_data[rec_count];
    memset((char *) zone_table, 0, (sizeof(struct zone_data) * rec_count));
    break;
  case DB_BOOT_SHP:
    shop_table = new struct shop_data[rec_count + shop_chunk_size];
    memset((char *) shop_table, 0, (sizeof(struct shop_data) *
                                    (rec_count + shop_chunk_size)));
    top_of_shop_array = rec_count + shop_chunk_size;
    break;
  case DB_BOOT_QST:
    quest_table = new struct quest_data[rec_count + quest_chunk_size];
    memset((char *) quest_table, 0, (sizeof(struct quest_data) *
                                     (rec_count + quest_chunk_size)));
    top_of_quest_array = rec_count + quest_chunk_size;
    break;
  }

  rewind(index);
  fscanf(index, "%32767s\n", buf1);
  while (*buf1 != '$') {
    snprintf(buf2, sizeof(buf2), "%s/%s", prefix, buf1);
    File in_file(buf2, "r");

    if (!in_file.IsOpen()) {
      snprintf(buf, sizeof(buf), "Unable to find file %s.", buf2);
      mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    } else {
      switch (mode) {
      case DB_BOOT_VEH:
      case DB_BOOT_WLD:
      case DB_BOOT_OBJ:
      case DB_BOOT_MTX:
      case DB_BOOT_MOB:
      case DB_BOOT_SHP:
      case DB_BOOT_QST:
      case DB_BOOT_IC:
        discrete_load(in_file, mode);
        break;
      case DB_BOOT_ZON:
        load_zones(in_file);
        break;
      }
      in_file.Close();
    }
    fscanf(index, "%32767s\n", buf1);
  }
  // Always important to clean up after yourself.
  fclose(index);
}

void discrete_load(File &fl, int mode)
{
  long nr = -1;
  char line[256];
  bool is_new = (mode == DB_BOOT_WLD || mode == DB_BOOT_MTX ||
                 mode == DB_BOOT_OBJ || mode == DB_BOOT_IC ||
                 mode == DB_BOOT_MOB || mode == DB_BOOT_VEH || mode == DB_BOOT_SHP);

  for (;;) {
    fl.GetLine(line, 256, FALSE);

    if (is_new? !str_cmp(line, "END") : *line == '$')
      return;

    if (*line == '#') {
      if (sscanf(line, "#%ld", &nr) != 1) {
        log_vfprintf("FATAL ERROR: Format error in %s, line %d: Expected '#' followed by one or more digits.",
            fl.Filename(), fl.LineNumber());
        exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
      }
      if (nr >= 99999999)
        return;
      else
        switch (mode) {
        case DB_BOOT_WLD:
          parse_room(fl, nr);
          break;
        case DB_BOOT_MOB:
          parse_mobile(fl, nr);
          break;
        case DB_BOOT_OBJ:
          parse_object(fl, nr);
          break;
        case DB_BOOT_VEH:
          parse_veh(fl, nr);
          break;
        case DB_BOOT_QST:
          parse_quest(fl, nr);
          break;
        case DB_BOOT_SHP:
          parse_shop(fl, nr);
          break;
        case DB_BOOT_MTX:
          parse_host(fl, nr);
          break;
        case DB_BOOT_IC:
          parse_ic(fl, nr);
          break;
        }
    } else {
      log_vfprintf("FATAL ERROR: Format error in %s, line %d: Expected '#' followed by one or more digits.",
          fl.Filename(), fl.LineNumber());
      exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
    }
  }
}

/* Ok around here somewhere,
 * we will need somebody to
 * code up the function to
 * load the vehicles to the
 * mud... void parse_room()
 * and stuff... I'm too new
 * at this to write anything
 * as complex as what is
 * written below for similar
 * purposes.  --Dunk */
void parse_veh(File &fl, long virtual_nr)
{
  static int veh_nr = 0, zone = 0;

  veh_index[veh_nr].vnum = virtual_nr;
  veh_index[veh_nr].number = 0;
  veh_index[veh_nr].func = NULL;

  if (virtual_nr <= (zone ? zone_table[zone - 1].top : -1)) {
    fprintf(stderr, "FATAL ERROR: Veh #%ld is below zone %d.\n", virtual_nr, zone_table[zone].number);
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }
  while (virtual_nr > zone_table[zone].top)
    if (++zone > top_of_zone_table) {
      fprintf(stderr, "FATAL ERROR: Veh %ld is outside of any zone.\n", virtual_nr);
      exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
    }
  VTable data;
  data.Parse(&fl);

  veh_proto[veh_nr].veh_number = veh_nr;
  veh_proto[veh_nr].name = str_dup(data.GetString("Name", "unnamed"));
  veh_proto[veh_nr].short_description = str_dup(data.GetString("ShortDesc", "An unnamed vehicle"));
  veh_proto[veh_nr].description = str_dup(data.GetString("RoomDesc", "An unnamed vehicle idles here."));
  veh_proto[veh_nr].long_description = str_dup(data.GetString("LongDesc", "It's a nondescript vehicle."));
  veh_proto[veh_nr].inside_description = str_dup(data.GetString("Inside", "You are inside a boring vehicle."));
  veh_proto[veh_nr].rear_description = str_dup(data.GetString("InsideRear", "You are inside a boring vehicle."));
  veh_proto[veh_nr].leave = str_dup(data.GetString("Leaving", "leaves"));
  veh_proto[veh_nr].arrive = str_dup(data.GetString("Arriving", "arrives"));
  veh_proto[veh_nr].handling = data.GetInt("Handling", 0);
  veh_proto[veh_nr].speed = data.GetInt("Speed", 0);
  veh_proto[veh_nr].accel = data.GetInt("Accel", 0);
  veh_proto[veh_nr].body = data.GetInt("Body", 0);
  veh_proto[veh_nr].armor = data.GetInt("Armour", 0);
  veh_proto[veh_nr].pilot = data.GetInt("Pilot", 0);
  veh_proto[veh_nr].sig = data.GetInt("Sig", 0);
  veh_proto[veh_nr].autonav = data.GetInt("Autonav", 0);
  veh_proto[veh_nr].seating[SEATING_FRONT] = data.GetInt("Seating", 0);
  veh_proto[veh_nr].seating[SEATING_REAR] = data.GetInt("SeatingBack", 0);
  veh_proto[veh_nr].load = data.GetFloat("Load", 0);
  veh_proto[veh_nr].cost = data.GetInt("Cost", 0);
  veh_proto[veh_nr].type = data.GetInt("Type", 0);
  veh_proto[veh_nr].flags.FromString(data.GetString("Flags", "0"));
  veh_proto[veh_nr].engine = data.GetInt("Engine", 0);
  veh_proto[veh_nr].contents = NULL;
  veh_proto[veh_nr].people = NULL;
  veh_proto[veh_nr].damage = 0;
  top_of_veht = veh_nr++;
}

void parse_host(File &fl, long nr)
{
  static long last_seen = -1;
  if (last_seen == -1) {
    last_seen = nr;
  } else if (last_seen >= nr) {
    snprintf(buf, sizeof(buf), "FATAL ERROR in parse_host(%s, %ld): last_seen %ld >= nr %ld.", fl.Filename(), nr, last_seen, nr);
    log(buf);
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }

  static DBIndex::rnum_t rnum = 0, zone = 0;
  char field[64];
  if (nr <= (zone ? zone_table[zone - 1].top : -1)) {
    log_vfprintf("FATAL ERROR: Host #%ld is below zone %d.\n", nr, zone_table[zone].number);
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }
  while (nr > zone_table[zone].top)
    if (++zone > top_of_zone_table) {
      log_vfprintf("FATAL ERROR: Host %ld is outside of any zone.\n", nr);
      exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
    }
  host_data *host = matrix+rnum;

  VTable data;
  data.Parse(&fl);

  host->vnum = nr;
  host->rnum = rnum;
  host->parent = data.GetLong("Parent", 0);
  host->name = str_dup(data.GetString("Name", "An empty host"));
  host->desc = str_dup(data.GetString("Description", "This host is empty!\n"));
  host->keywords = str_dup(data.GetString("Keywords", "host"));
  host->shutdown_start = str_dup(data.GetString("ShutdownStart", "A deep echoing voice announces a host shutdown.\r\n"));
  host->shutdown_stop = str_dup(data.GetString("ShutdownStop", "A deep echoing voice announces the shutdown has been aborted.\r\n"));
  host->color = data.GetInt("Colour", 0);
  host->security = data.GetInt("Security", 0);
  host->intrusion = data.GetInt("Difficulty", 0);
  host->stats[ACIFS_ACCESS][0] = data.GetLong("Access", 0);
  host->stats[ACIFS_ACCESS][2] = data.GetLong("AccessScramble", 0);
  host->stats[ACIFS_ACCESS][5] = data.GetLong("AccessTrapdoor", 0);
  if (host->stats[ACIFS_ACCESS][2])
    host->stats[ACIFS_ACCESS][1] = 1;
  host->stats[ACIFS_CONTROL][0] = data.GetLong("Control", 0);
  host->stats[ACIFS_CONTROL][5] = data.GetLong("ControlTrapdoor", 0);
  host->stats[ACIFS_INDEX][0] = data.GetLong("Index", 0);
  host->stats[ACIFS_INDEX][5] = data.GetLong("IndexTrapdoor", 0);
  host->stats[ACIFS_FILES][0] = data.GetLong("Files", 0);
  host->stats[ACIFS_FILES][2] = data.GetLong("FilesScramble", 0);
  host->stats[ACIFS_FILES][5] = data.GetLong("FilesTrapdoor", 0);
  if (host->stats[ACIFS_FILES][2])
    host->stats[ACIFS_FILES][1] = 1;
  host->stats[ACIFS_SLAVE][0] = data.GetLong("Slave", 0);
  host->stats[ACIFS_SLAVE][2] = data.GetLong("SlaveScramble", 0);
  host->stats[ACIFS_SLAVE][5] = data.GetLong("SlaveTrapdoor", 0);
  if (host->stats[ACIFS_SLAVE][2])
    host->stats[ACIFS_SLAVE][1] = 1;
  host->type = data.LookupInt("Type", host_type, 0);
  int num_fields = data.NumSubsections("EXITS");
  for (int x = 0; x < num_fields; x++) {
    const char *name = data.GetIndexSection("EXITS", x);
    exit_data *exit = new exit_data;
    snprintf(field, sizeof(field), "%s/Exit", name);
    exit->host = data.GetLong(field, 0);
    snprintf(field, sizeof(field), "%s/Number", name);
    exit->addresses = str_dup(data.GetString(field, "000"));
    snprintf(field, sizeof(field), "%s/RoomString", name);
    if (*(data.GetString(field, ""))) {
      exit->roomstring = str_dup(data.GetString(field, ""));
    } else {
      exit->roomstring = NULL;
    }
    snprintf(field, sizeof(field), "%s/Hidden", name);
    exit->hidden = (bool) data.GetInt(field, 1);
    if (host->exit)
      exit->next = host->exit;
    host->exit = exit;
  }
  num_fields = data.NumSubsections("TRIGGERS");
  for (int x = 0; x < num_fields; x++) {
    const char *name = data.GetIndexSection("TRIGGERS", x);
    trigger_step *trigger = new trigger_step;
    snprintf(field, sizeof(field), "%s/Step", name);
    trigger->step = data.GetInt(field, 0);
    snprintf(field, sizeof(field), "%s/Alert", name);
    trigger->alert = data.LookupInt(field, alerts, 0);
    snprintf(field, sizeof(field), "%s/IC", name);
    trigger->ic = data.GetLong(field, 0);
    if (host->trigger) {
      struct trigger_step *last = NULL, *temp;
      for (temp = host->trigger; temp && trigger->step < temp->step; temp = temp->next)
        last = temp;
      if (temp) {
        trigger->next = temp->next;
        temp->next = trigger;
      } else
        last->next = trigger;
    } else
      host->trigger = trigger;
  }
  if (host->type == HOST_DATASTORE) {
    reset_host_paydata(rnum);
  }
  top_of_matrix = rnum++;
}
void parse_ic(File &fl, long nr)
{
  static long last_seen = -1;
  if (last_seen == -1) {
    last_seen = nr;
  } else if (last_seen >= nr) {
    snprintf(buf, sizeof(buf), "FATAL ERROR in parse_ic(%s, %ld): last_seen %ld >= nr %ld.", fl.Filename(), nr, last_seen, nr);
    log(buf);
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }

  static DBIndex::rnum_t rnum = 0, zone = 0;
  ic_index[rnum].vnum = nr;
  ic_index[rnum].number = 0;
  ic_index[rnum].func = NULL;

  matrix_icon *ic = ic_proto+rnum;
  clear_icon(ic);
  ic->rnum = rnum;
  ic->vnum = nr;

  if (nr <= (zone ? zone_table[zone - 1].top : -1)) {
    log_vfprintf("FATAL ERROR: IC #%ld is below zone %ld.\n", nr, zone);
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }
  while (nr > zone_table[zone].top)
    if (++zone > top_of_zone_table) {
      log_vfprintf("FATAL ERROR: IC%ld is outside of any zone.\n", nr);
      exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
    }

  VTable data;
  data.Parse(&fl);

  ic->name = str_dup(data.GetString("Name", "An unfinished IC"));
  ic->look_desc = str_dup(data.GetString("LongDesc", "An unfinished IC guards the node."));
  ic->long_desc = str_dup(data.GetString("Description", "This IC is unfinished.\r\n"));
  ic->ic.rating = data.GetInt("Rating", 0);
  ic->ic.type = data.LookupInt("Type", ic_type, 0);
  ic->ic.subtype = data.GetInt("SubType", 0);
  ic->ic.trap = data.GetLong("Trap", 0);
  ic->ic.expert = data.GetInt("Expert", 0);
  ic->ic.options.FromString(data.GetString("Flags", "0"));
  top_of_ic = rnum++;
}
/* load the rooms */
void parse_room(File &fl, long nr)
{
  static long last_seen = -1;
  if (last_seen == -1) {
    last_seen = nr;
  } else if (last_seen >= nr) {
    snprintf(buf, sizeof(buf), "FATAL ERROR in parse_room(%s, %ld): last_seen %ld >= nr %ld.", fl.Filename(), nr, last_seen, nr);
    log(buf);
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }

  static DBIndex::rnum_t rnum = 0, zone = 0;

  if (nr <= (zone ? zone_table[zone - 1].top : -1)) {
    log_vfprintf("FATAL ERROR: Room #%ld is below zone %d.\n", nr, zone_table[zone].number);
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }
  while (nr > zone_table[zone].top)
    if (++zone > top_of_zone_table) {
      log_vfprintf("FATAL ERROR: Room %ld is outside of any zone.\n", nr);
      exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
    }

  room_data *room = world+rnum;

#ifdef USE_DEBUG_CANARIES
  room->canary = CANARY_VALUE;
#endif

  VTable data;
  data.Parse(&fl);

  room->zone = zone;
  room->number = nr;

  room->name = str_dup(data.GetString("Name", "An empty room"));
  room->description =
    str_dup(data.GetString("Desc", "This room is empty!  Boo!\n"));
  room->night_desc = str_dup(data.GetString("NightDesc", NULL));
  room->room_flags.FromString(data.GetString("Flags", "0"));
  if (room->room_flags.IsSet(ROOM_PEACEFUL))
    room->peaceful = 1;
  room->sector_type = data.LookupInt("SecType", spirit_name, DEFAULT_SECTOR_TYPE);
  room->matrix = data.GetLong("MatrixExit", 0);
  room->io = data.GetInt("IO", 0);
  room->bandwidth = data.GetInt("Bandwidth", 0);
  room->access = data.GetInt("Access", 0);
  room->trace = data.GetInt("Trace", 0);
  room->rtg = data.GetLong("RTG", 1100);
  room->jacknumber = data.GetLong("JackID", 0);
  room->address = str_dup(data.GetString("Address", "An undisclosed location"));
  room->spec = data.GetInt("POINTS/SpecIdx", 0);
  room->rating = data.GetInt("POINTS/Rating", 0);
  room->vision[0] = data.GetInt("POINTS/Light", 0);
  room->vision[1] = data.GetInt("POINTS/Smoke", 0);
  room->background[CURRENT_BACKGROUND_COUNT] = room->background[PERMANENT_BACKGROUND_COUNT] = data.GetInt("POINTS/Background", 0);
  room->background[CURRENT_BACKGROUND_TYPE] = room->background[PERMANENT_BACKGROUND_TYPE] = data.GetInt("POINTS/BackgroundType", 0);
  room->staff_level_lock = data.GetInt("POINTS/StaffLockLevel", 0);
  room->flight_code = str_dup(data.GetString("POINTS/FlightCode", INVALID_FLIGHT_CODE));
  room->latitude = data.GetFloat("POINTS/Latitude", 0.0);
  room->longitude = data.GetFloat("POINTS/Longitude", 0.0);
  if (room->vision[0] == -1) {
    if (room->room_flags.IsSet(ROOM_DARK))
      room->vision[0] = LIGHT_FULLDARK;
    else if (room->room_flags.IsSet(ROOM_LOW_LIGHT))
      room->vision[0] = LIGHT_PARTLIGHT;
    else
      room->vision[0] = LIGHT_NORMAL;
  }
  room->crowd = data.GetInt("Crowd", 0);
  room->cover = data.GetInt("Cover", 0);
  room->x = data.GetInt("X", DEFAULT_DIMENSIONS_X);
  room->y = data.GetInt("Y", DEFAULT_DIMENSIONS_Y);
  room->z = data.GetFloat("Z", DEFAULT_DIMENSIONS_Z);
  room->type = data.GetInt("RoomType", 0);

  // read in directions, but only if we're not a cab.
  if (!((GET_ROOM_VNUM(room) >= FIRST_SEATTLE_CAB && GET_ROOM_VNUM(room) <= LAST_SEATTLE_CAB)
         || (GET_ROOM_VNUM(room) >= FIRST_PORTLAND_CAB && GET_ROOM_VNUM(room) <= LAST_PORTLAND_CAB)
         || (GET_ROOM_VNUM(room) >= FIRST_CARIBBEAN_CAB && GET_ROOM_VNUM(room) <= LAST_CARIBBEAN_CAB)))
  {
    for (int i = 0; *fulldirs[i] != '\n'; i++) {
    char sect[16];
    snprintf(sect, sizeof(sect), "EXIT %s", fulldirs[i]);

    room->dir_option[i] = NULL;

    if (data.DoesSectionExist(sect)) {
      char field[64];

      snprintf(field, sizeof(field), "%s/ToVnum", sect);
      int to_vnum = data.GetInt(field, -1);

      if (to_vnum < 0
          || (to_vnum >= FIRST_SEATTLE_CAB && to_vnum <= LAST_SEATTLE_CAB)
          || (to_vnum >= FIRST_PORTLAND_CAB && to_vnum <= LAST_PORTLAND_CAB)
          || (to_vnum >= FIRST_CARIBBEAN_CAB && to_vnum <= LAST_CARIBBEAN_CAB))
      {
        log_vfprintf("Room #%ld's %s exit had invalid destination -- skipping",
            nr, fulldirs[i]);
        continue;
      }

      room->dir_option[i] = new room_direction_data;
      room_direction_data *dir = room->dir_option[i];

      dir->to_room = &world[0];
      dir->to_room_vnum = to_vnum;

      snprintf(field, sizeof(field), "%s/Keywords", sect);
      dir->keyword = str_dup(data.GetString(field, NULL));

      snprintf(field, sizeof(field), "%s/Desc", sect);
      dir->general_description = str_dup(data.GetString(field, NULL));

      snprintf(field, sizeof(field), "%s/Flags", sect);
      int flags = data.GetInt(field, 0);

      if (flags == 1)
        dir->exit_info = EX_ISDOOR;
      else if (flags == 2)
        dir->exit_info = EX_ISDOOR | EX_PICKPROOF;
      else if (flags == 3)
        dir->exit_info = EX_ISDOOR | EX_ASTRALLY_WARDED;
      else if (flags == 4)
        dir->exit_info = EX_ISDOOR | EX_PICKPROOF | EX_ASTRALLY_WARDED;
      else
        dir->exit_info = 0;

      snprintf(field, sizeof(field), "%s/MoreFlags", sect);
      int moreflags = data.GetInt(field, 0);

      if (moreflags >= 8) {
        moreflags -= 8;
        dir->exit_info |= EX_STRICT_ABOUT_KEY;
      }

      if (moreflags >= 4) {
        moreflags -= 4;
        dir->exit_info |= EX_CANT_SHOOT_THROUGH;
      }

      if (moreflags == 1)
        dir->exit_info |= EX_WINDOWED;
      else if (moreflags == 2)
        dir->exit_info |= EX_BARRED_WINDOW;

      snprintf(field, sizeof(field), "%s/Material", sect);
      dir->material = data.LookupInt(field, material_names, DEFAULT_EXIT_MATERIAL);

      snprintf(field, sizeof(field), "%s/Barrier", sect);
      dir->barrier = data.GetInt(field, DEFAULT_EXIT_BARRIER_RATING);
      dir->condition = dir->barrier;

      snprintf(field, sizeof(field), "%s/KeyVnum", sect);
      dir->key = data.GetInt(field, -1);

      snprintf(field, sizeof(field), "%s/LockRating", sect);
      dir->key_level = data.GetInt(field, 0);

      snprintf(field, sizeof(field), "%s/HiddenRating", sect);
      dir->hidden = data.GetInt(field, 0);
      if (dir->hidden > MAX_EXIT_HIDDEN_RATING) {
        dir->hidden = MAX_EXIT_HIDDEN_RATING;
      }

      snprintf(field, sizeof(field), "%s/GoIntoSecondPerson", sect);
      dir->go_into_secondperson = str_dup(data.GetString(field, NULL));

      snprintf(field, sizeof(field), "%s/GoIntoThirdPerson", sect);
      dir->go_into_thirdperson = str_dup(data.GetString(field, NULL));

      snprintf(field, sizeof(field), "%s/ComeOutOfThirdPerson", sect);
      dir->come_out_of_thirdperson = str_dup(data.GetString(field, NULL));

#ifdef USE_DEBUG_CANARIES
      dir->canary = CANARY_VALUE;
#endif
    }
  }
  }

  room->ex_description = NULL;

  // finally, read in extra descriptions
  for (int i = 0; true; i++) {
    char sect[16];
    snprintf(sect, sizeof(sect), "EXTRADESC %d", i);

    if (data.NumFields(sect) > 0) {
      char field[64];

      snprintf(field, sizeof(field), "%s/Keywords", sect);
      char *keywords = str_dup(data.GetString(field, NULL));

      if (!*keywords) {
        log_vfprintf("Room #%ld's extra description #%d had no keywords -- skipping",
            nr, i);
        DELETE_ARRAY_IF_EXTANT(keywords);
        continue;
      }


      extra_descr_data *desc = new extra_descr_data;
      desc->keyword = keywords;
      snprintf(field, sizeof(field), "%s/Desc", sect);
      desc->description = str_dup(data.GetString(field, NULL));
      desc->next = room->ex_description;
      room->ex_description = desc;
    } else
      break;
  }

  // Set background count.
  if (GET_JURISDICTION(room) == JURISDICTION_SECRET) {
    if (!ROOM_FLAGGED(room, ROOM_INDOORS)) {
      if (GET_BACKGROUND_COUNT(room) == 0) {
        room->background[CURRENT_BACKGROUND_COUNT] = room->background[PERMANENT_BACKGROUND_COUNT] = 1;
        room->background[CURRENT_BACKGROUND_TYPE] = room->background[PERMANENT_BACKGROUND_TYPE] = AURA_WRONGNESS;
      }

      if (room->sector_type == SPIRIT_HEARTH) {
        room->sector_type = SPIRIT_CITY;
      }
    }
  }

  top_of_world = rnum++;
  if (top_of_world >= top_of_world_array) {
    snprintf(buf, sizeof(buf), "WARNING: top_of_world >= top_of_world_array at %ld / %d.", top_of_world, top_of_world_array);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  }
}

/* read direction data */
void setup_dir(FILE * fl, int room, int dir)
{
  int t[7];
  char line[256];
  int retval;

  snprintf(buf2, sizeof(buf2), "room #%ld, direction D%d", world[room].number, dir);

  world[room].dir_option[dir] = new room_direction_data;
  world[room].dir_option[dir]->general_description = fread_string(fl, buf2);
  world[room].dir_option[dir]->keyword = fread_string(fl, buf2);

  if (!get_line(fl, line)) {
    fprintf(stderr, "FATAL ERROR: Format error, %s: Cannot get line from file.\n", buf2);
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }
  if ((retval = sscanf(line, " %d %d %d %d %d %d %d", t, t + 1, t + 2, t + 3,
                       t + 4, t + 5, t + 6)) < 4) {
    fprintf(stderr, "FATAL ERROR: Format error, %s: Expected seven numbers like ' # # # # # # #'\n", buf2);
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }
  if (t[0] == 1)
    world[room].dir_option[dir]->exit_info = EX_ISDOOR;
  else if (t[0] == 2)
    world[room].dir_option[dir]->exit_info = EX_ISDOOR | EX_PICKPROOF;
  else
    world[room].dir_option[dir]->exit_info = 0;

  world[room].dir_option[dir]->key = t[1];
  world[room].dir_option[dir]->to_room = &world[0]; // Will be set properly during world renumbering.
  world[room].dir_option[dir]->to_room_vnum = MAX(0, t[2]);
  world[room].dir_option[dir]->key_level = t[3];

  world[room].dir_option[dir]->material = (retval > 4) ? t[4] : 5;
  world[room].dir_option[dir]->barrier = (retval > 5) ? t[5] : 4;
  world[room].dir_option[dir]->condition = (retval > 5) ? t[5] : 4;
  world[room].dir_option[dir]->hidden = (retval > 6) ? t[6] : 0;
}

/* make sure the start rooms exist & resolve their vnums to rnums */
void check_start_rooms(void)
{
  extern vnum_t mortal_start_room;
  extern vnum_t immort_start_room;
  extern vnum_t frozen_start_room;
  extern vnum_t newbie_start_room;

  if ((r_mortal_start_room = real_room(mortal_start_room)) < 0) {
    log("FATAL SYSERR:  Mortal start room does not exist.  Change in config.c.");
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }
  if ((r_immort_start_room = real_room(immort_start_room)) < 0) {
    log("SYSERR:  Warning: Immort start room does not exist.  Change in config.c.");
    r_immort_start_room = r_mortal_start_room;
  }
  if ((r_frozen_start_room = real_room(frozen_start_room)) < 0) {
    log("SYSERR:  Warning: Frozen start room does not exist.  Change in config.c.");
    r_frozen_start_room = r_mortal_start_room;
  }
  if ((r_newbie_start_room = real_room(newbie_start_room)) < 0) {
    log("SYSERR:  Warning: Newbie start room does not exist.  Change in config.c.");
    r_newbie_start_room = r_mortal_start_room;
  }
}

/* resolve all vnums into rnums in the world */
void renum_world(void)
{
  int room, door;

  /* before renumbering the exits, copy them to to_room_vnum */
  for (room = 0; room <= top_of_world; room++)
    for (door = 0; door < NUM_OF_DIRS; door++)
      if (world[room].dir_option[door]) {
        room_direction_data *dir = world[room].dir_option[door];
        vnum_t rnum = real_room(dir->to_room_vnum);
        if (rnum != NOWHERE)
          dir->to_room = &world[rnum];
        else
          dir->to_room = &world[0];
      }
}

#define ZCMD zone_table[zone].cmd[cmd_no]

bool can_load_this_thing_in_zone_commands(DBIndex::rnum_t rnum, int zone, int cmd_no) {
  if (rnum < 0) {
    log_zone_error(zone, cmd_no, "Negative rnum.");
    return FALSE;
  }
  if (GET_OBJ_TYPE(&obj_proto[rnum]) == ITEM_MONEY) {
    // Zoneloading money is forbidden.
    log_zone_error(zone, cmd_no, "Money cannot be loaded in zone commands.");
    return FALSE;
  }
  if (GET_OBJ_VNUM(&obj_proto[rnum]) == OBJ_OLD_BLANK_MAGAZINE_FROM_CLASSIC) {
    // Zoneloading 601, which used to be a blank empty mag, is not allowed.
    log_zone_error(zone, cmd_no, "Object 601 (old Classic magazine) cannot be loaded in zone commands.");
    return FALSE;
  }
  if (GET_OBJ_VNUM(&obj_proto[rnum]) == OBJ_BLANK_MAGAZINE) {
    // Zoneloading 601, which used to be a blank empty mag, is not allowed.
    log_zone_error(zone, cmd_no, "Object 127 (blank magazine) cannot be loaded in zone commands.");
    return FALSE;
  }
  return TRUE;
}

#define ENSURE_OBJECT_IS_KOSHER(rnum)                              \
if (!can_load_this_thing_in_zone_commands((rnum), zone, cmd_no)) { \
  ZCMD.command = '*';                                              \
  continue;                                                        \
}

/* resulve vnums into rnums in the zone reset tables */
void renum_zone_table(void)
{
  int zone, cmd_no;
  long a, b;
  long arg1, arg2, arg3;

  for (zone = 0; zone <= top_of_zone_table; zone++)
    for (cmd_no = 0; cmd_no < zone_table[zone].num_cmds; cmd_no++) {
      a = b = 0;
      arg1 = ZCMD.arg1; arg2 = ZCMD.arg2; arg3 = ZCMD.arg3;
      switch (ZCMD.command) {
        /* ADDING A real_object STATEMENT? YOU MUST UPDATE IEDIT'S ZONE TABLES SWITCH */
        /* ADDING A real_mobile STATEMENT? YOU MUST UPDATE MEDIT'S ZONE TABLES SWITCH */
        /* ADDING A real_room STATEMENT? YOU MUST UPDATE REDIT'S ZONE TABLES SWITCH */
        /* ADDING A real_vehicle STATEMENT? YOU MUST UPDATE VEDIT'S ZONE TABLES SWITCH */
        /* ADDING A real_host STATEMENT? YOU MUST UPDATE HEDIT'S ZONE TABLES SWITCH */
        case 'M':
          a = ZCMD.arg1 = real_mobile(ZCMD.arg1);
          b = ZCMD.arg3 = real_room(ZCMD.arg3);
          break;
        case 'H':
          a = ZCMD.arg1 = real_object(ZCMD.arg1);
          ENSURE_OBJECT_IS_KOSHER(a);
          if (ZCMD.arg3 != NOWHERE)
            b = ZCMD.arg3 = real_host(ZCMD.arg3);
          break;
        case 'O':
          a = ZCMD.arg1 = real_object(ZCMD.arg1);
          ENSURE_OBJECT_IS_KOSHER(a);
          if (ZCMD.arg3 != NOWHERE)
            b = ZCMD.arg3 = real_room(ZCMD.arg3);
          break;
        case 'V': /* Vehicles */
          a = ZCMD.arg1 = real_vehicle(ZCMD.arg1);
          b = ZCMD.arg3 = real_room(ZCMD.arg3);
          break;
        case 'S':
          a = ZCMD.arg1 = real_mobile(ZCMD.arg1);
          break;
        /* ADDING A real_object STATEMENT? YOU MUST UPDATE IEDIT'S ZONE TABLES SWITCH */
        /* ADDING A real_mobile STATEMENT? YOU MUST UPDATE MEDIT'S ZONE TABLES SWITCH */
        /* ADDING A real_room STATEMENT? YOU MUST UPDATE REDIT'S ZONE TABLES SWITCH */
        /* ADDING A real_vehicle STATEMENT? YOU MUST UPDATE VEDIT'S ZONE TABLES SWITCH */
        /* ADDING A real_host STATEMENT? YOU MUST UPDATE HEDIT'S ZONE TABLES SWITCH */
        case 'G':
        case 'C':
        case 'U':
        case 'I':
        case 'E':
          a = ZCMD.arg1 = real_object(ZCMD.arg1);
          ENSURE_OBJECT_IS_KOSHER(a);
          break;
        case 'P':
          a = ZCMD.arg1 = real_object(ZCMD.arg1);
          ENSURE_OBJECT_IS_KOSHER(a);
          b = ZCMD.arg3 = real_object(ZCMD.arg3);
          ENSURE_OBJECT_IS_KOSHER(b);
          break;
        case 'D':
          a = ZCMD.arg1 = real_room(ZCMD.arg1);
          break;
        case 'R': /* rem obj from room */
          a = ZCMD.arg1 = real_room(ZCMD.arg1);
          b = ZCMD.arg2 = real_object(ZCMD.arg2);
          ENSURE_OBJECT_IS_KOSHER(b);
          break;
        case 'N':
          a = ZCMD.arg1 = real_object(ZCMD.arg1);
          ENSURE_OBJECT_IS_KOSHER(a);
          break;
        /* ADDING A real_object STATEMENT? YOU MUST UPDATE IEDIT'S ZONE TABLES SWITCH */
        /* ADDING A real_mobile STATEMENT? YOU MUST UPDATE MEDIT'S ZONE TABLES SWITCH */
        /* ADDING A real_room STATEMENT? YOU MUST UPDATE REDIT'S ZONE TABLES SWITCH */
        /* ADDING A real_vehicle STATEMENT? YOU MUST UPDATE VEDIT'S ZONE TABLES SWITCH */
        /* ADDING A real_host STATEMENT? YOU MUST UPDATE HEDIT'S ZONE TABLES SWITCH */
      }
      if (a < 0 || b < 0) {
        snprintf(buf, sizeof(buf), "Invalid vnum in command. Args were: %ld %ld %ld, which resolved to a=%ld and b=%ld.",
                arg1, arg2, arg3, a, b);
        log_zone_error(zone, cmd_no, buf);
        ZCMD.command = '*';
      }
    }
}

void parse_mobile(File &in, long nr)
{
  static long last_seen = -1;
  if (last_seen == -1) {
    last_seen = nr;
  } else if (last_seen >= nr) {
    snprintf(buf, sizeof(buf), "FATAL ERROR in parse_mobile(%s, %ld): last_seen %ld >= nr %ld.", in.Filename(), nr, last_seen, nr);
    log(buf);
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }

  static DBIndex::rnum_t rnum = 0;

  char_data *mob = mob_proto+rnum;

  MOB_VNUM_RNUM(rnum) = nr;
  mob_index[rnum].number = 0;
  mob_index[rnum].func = NULL;

  clear_char(mob);

  mob->player_specials = &dummy_mob;

  // heheh....sweeeeet
  VTable data;
  data.Parse(&in);

  mob->player.physical_text.keywords =
    str_dup(data.GetString("Keywords", "mob unnamed"));
  mob->player.physical_text.name =
    str_dup(data.GetString("Name", "an unnamed mob"));
  mob->player.physical_text.room_desc =
    str_dup(data.GetString("RoomDesc", "An unfinished mob stands here.\n"));
  mob->player.physical_text.look_desc =
    str_dup(data.GetString("LookDesc", "He looks pretty unfinished.\n"));

  mob->char_specials.arrive =
    str_dup(data.GetString("ArriveMsg", "arrives from"));
  mob->char_specials.leave =
    str_dup(data.GetString("LeaveMsg", "leaves"));

  {
    const char *highlight = str_dup(data.GetString("SpeechHighlight", "nothing"));
    if (!strcmp(highlight, "nothing")) {
      delete [] highlight;
    } else {
      SETTABLE_CHAR_COLOR_HIGHLIGHT(mob) = highlight;
    }
  }

  GET_TITLE(mob) = NULL;

  MOB_FLAGS(mob).FromString(data.GetString("MobFlags", "0"));
  AFF_FLAGS(mob).FromString(data.GetString("AffFlags", "0"));

  // Manifestation requires the astral bit to be set.
  if (AFF_FLAGS(mob).IsSet(AFF_MANIFEST))
    MOB_FLAGS(mob).SetBit(MOB_ASTRAL);

  GET_RACE(mob) = data.LookupInt("Race", pc_race_types, RACE_HUMAN);
  GET_PRONOUNS(mob) = data.LookupInt("Gender", genders, PRONOUNS_NEUTRAL);

  GET_POS(mob) = data.LookupInt("Position", position_types, POS_STANDING);
  GET_DEFAULT_POS(mob) =
    data.LookupInt("DefaultPos", position_types, POS_STANDING);
  mob->mob_specials.attack_type = TYPE_HIT +
                                  data.LookupInt("AttackType", attack_types, 0);

  GET_REAL_BOD(mob) = data.GetInt("ATTRIBUTES/Bod", 1);
  GET_REAL_QUI(mob) = data.GetInt("ATTRIBUTES/Qui", 1);
  GET_REAL_STR(mob) = data.GetInt("ATTRIBUTES/Str", 1);
  GET_REAL_CHA(mob) = data.GetInt("ATTRIBUTES/Cha", 1);
  GET_REAL_INT(mob) = data.GetInt("ATTRIBUTES/Int", 1);
  GET_REAL_WIL(mob) = data.GetInt("ATTRIBUTES/Wil", 1);
  GET_REAL_REA(mob) = (GET_REAL_QUI(mob) + GET_REAL_INT(mob)) / 2;


  mob->real_abils.mag = data.GetInt("ATTRIBUTES/Mag", 0) * 100;
  mob->real_abils.ess = GET_RACIAL_STARTING_ESSENCE_FOR_RACE(GET_RACE(mob));
  mob->real_abils.bod_index = 900;

  mob->aff_abils = mob->real_abils;

  GET_HEIGHT(mob) = data.GetInt("POINTS/Height", 100);
  GET_WEIGHT(mob) = data.GetInt("POINTS/Weight", 5);
  GET_LEVEL(mob) = data.GetInt("POINTS/Level", 0);
  GET_MAX_PHYSICAL(mob) = data.GetInt("POINTS/MaxPhys", 10) * 100;
  GET_MAX_MENTAL(mob) = data.GetInt("POINTS/MaxMent", 10) * 100;
  int innate_ballistic = data.GetInt("POINTS/Ballistic", 0);
  int innate_impact = data.GetInt("POINTS/Impact", 0);
  GET_NUYEN_RAW(mob) = data.GetInt("POINTS/Cash", 0);
  GET_BANK_RAW(mob) = data.GetInt("POINTS/Bank", 0);
  GET_KARMA(mob) = data.GetInt("POINTS/Karma", 0);
  GET_MOB_FACTION_IDNUM(mob) = data.GetLong("POINTS/Faction", 0);

  GET_PHYSICAL(mob) = GET_MAX_PHYSICAL(mob);
  GET_MENTAL(mob) = GET_MAX_MENTAL(mob);
  GET_SUSTAINED(mob) = 0;
  GET_GRADE(mob) = 0;

  /* set pools to 0 initially, affect total will correct them */
  mob->real_abils.astral_pool = 0;
  mob->real_abils.defense_pool = 0;
  mob->real_abils.combat_pool = 0;
  mob->real_abils.offense_pool = 0;
  mob->real_abils.hacking_pool = 0;
  mob->real_abils.magic_pool = 0;

  int j;
  for (j = 0; j < 10; j++)
    mob->mob_specials.mob_skills[j] = 0;

  int num_skills = data.NumFields("SKILLS");
  for (j = 0; j < num_skills; j++) {
    const char *skill_name = data.GetIndexField("SKILLS", j);
    int idx;

    for (idx = 0; idx < MAX_SKILLS; idx++)
      if ((idx == SKILL_UNARMED_COMBAT && !str_cmp("unarmed combat", skill_name)) || !str_cmp(skills[idx].name, skill_name))
        break;
    if (idx > 0 || idx < MAX_SKILLS) {
      SET_SKILL(mob, idx, data.GetIndexInt("SKILLS", j, 0));
      mob->mob_specials.mob_skills[j * 2] = idx;
      mob->mob_specials.mob_skills[j * 2 + 1] = data.GetIndexInt("SKILLS", j, 0);
    }
  }

  for (j = 0; j < 3; j++)
    GET_COND(mob, j) = -1;

  for (j = 0; j < NUM_WEARS; j++)
    mob->equipment[j] = NULL;

  mob->cyberware = NULL;
  mob->bioware = NULL;
  mob->nr = rnum;
  mob->desc = NULL;

  if ( 1 ) {
    extern int calc_karma(struct char_data *ch, struct char_data *vict);

    int old = GET_KARMA(mob);

    GET_KARMA(mob) = 0; // calc_karma prolly relies on this

    GET_KARMA(mob) = MIN(old, calc_karma(NULL, mob));
  }

  // Load ammo.
  for (int wp = START_OF_AMMO_USING_WEAPONS; wp <= END_OF_AMMO_USING_WEAPONS; wp++)
    for (int am = AMMO_NORMAL; am < NUM_AMMOTYPES; am++) {
      snprintf(buf, sizeof(buf), "AMMO/%s", get_ammo_representation(wp, am, 0, mob));
      GET_BULLETPANTS_AMMO_AMOUNT(mob, wp, am) = data.GetInt(buf, 0);
    }

  // Load cyberware.
  {
    char field[32];
    int num_fields = data.NumSubsections("CYBERWARE");
    vnum_t vnum;
    struct obj_data *ware = NULL;

    for (int x = 0; x < num_fields; x++) {
      const char *name = data.GetIndexSection("CYBERWARE", x);
      snprintf(field, sizeof(field), "%s/Vnum", name);
      vnum = data.GetLong(field, -1);

      if (vnum < 1 || !(ware = read_object(vnum, VIRTUAL, OBJ_LOAD_REASON_MOB_PROTO_EQ))) {
        log_vfprintf("MOB FILE ERROR: Mob %ld referenced cyberware vnum %ld (entry %d) which does not exist.", nr, vnum, x);
        continue;
      } else {
        if (GET_OBJ_TYPE(ware) != ITEM_CYBERWARE) {
          log_vfprintf("MOB FILE ERROR: Mob %ld referenced vnum %ld (entry %d) as cyberware, but it's not cyberware.", nr, vnum, x);
          extract_obj(ware);
          continue;
        }
        // log_vfprintf("debug: reading cyber %s (%ld) into prototype for %s.", GET_OBJ_NAME(ware), GET_OBJ_VNUM(ware), GET_CHAR_NAME(mob));
        obj_to_cyberware(ware, mob);
      }
    }
  }

  // Same thing for bioware. TODO: Merge this copypasta'd code into one function.
  {
    char field[32];
    int num_fields = data.NumSubsections("BIOWARE");
    vnum_t vnum;
    struct obj_data *ware = NULL;

    for (int x = 0; x < num_fields; x++) {
      const char *name = data.GetIndexSection("BIOWARE", x);
      snprintf(field, sizeof(field), "%s/Vnum", name);
      vnum = data.GetLong(field, -1);

      if (vnum == -1 || !(ware = read_object(vnum, VIRTUAL, OBJ_LOAD_REASON_MOB_PROTO_EQ))) {
        log_vfprintf("MOB FILE ERROR: Mob %ld referenced bioware vnum %ld (entry %d) which does not exist.", nr, vnum, x);
        continue;
      } else {
        if (GET_OBJ_TYPE(ware) != ITEM_BIOWARE) {
          log_vfprintf("MOB FILE ERROR: Mob %ld referenced vnum %ld (entry %d) as bioware, but it's not bioware.", nr, vnum, x);
          extract_obj(ware);
          continue;
        }
        // log_vfprintf("debug: reading bio %s (%ld) into prototype for %s.", GET_OBJ_NAME(ware), GET_OBJ_VNUM(ware), GET_CHAR_NAME(mob));
        obj_to_bioware(ware, mob);
      }
    }
  }

  {
    char field[32];
    int num_fields = data.NumSubsections("EQUIPMENT"), wearloc;
    vnum_t vnum;
    struct obj_data *eq = NULL;

    for (int x = 0; x < num_fields; x++) {
      const char *name = data.GetIndexSection("EQUIPMENT", x);
      snprintf(field, sizeof(field), "%s/Vnum", name);
      vnum = data.GetLong(field, -1);
      snprintf(field, sizeof(field), "%s/Wearloc", name);
      wearloc = data.GetLong(field, -1);

      if (!(eq = read_object(vnum, VIRTUAL, OBJ_LOAD_REASON_MOB_PROTO_EQ))) {
        log_vfprintf("MOB FILE ERROR: Mob %ld referenced equipment vnum %ld (entry %d) which does not exist.", nr, vnum, x);
        continue;
      }

      if (wearloc < 0 || wearloc >= NUM_WEARS) {
        log_vfprintf("MOB FILE ERROR: Mob %ld referenced invalid wearloc %d (entry %d).", nr, wearloc, x);
        continue;
      }

      // log_vfprintf("debug: reading eq %s (%ld) into prototype for %s.", GET_OBJ_NAME(eq), GET_OBJ_VNUM(eq), GET_CHAR_NAME(mob));
      equip_char(mob, eq, wearloc);
    }
  }

  // Equipment messed with our armor rating-- make sure our innates are the same.
  GET_INNATE_BALLISTIC(mob) = innate_ballistic;
  GET_BALLISTIC(mob) += GET_INNATE_BALLISTIC(mob);

  GET_INNATE_IMPACT(mob) = innate_impact;
  GET_IMPACT(mob) += GET_INNATE_IMPACT(mob);

  top_of_mobt = rnum++;
}

void parse_object(File &fl, long nr)
{
  static long last_seen = -1;
  if (last_seen == -1) {
    last_seen = nr;
  } else if (last_seen >= nr) {
    snprintf(buf, sizeof(buf), "FATAL ERROR in parse_object(%s, %ld): last_seen %ld >= nr %ld.", fl.Filename(), nr, last_seen, nr);
    log(buf);
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }

  static DBIndex::rnum_t rnum = 0;

  OBJ_VNUM_RNUM(rnum) = nr;
  obj_index[rnum].number = 0;
  obj_index[rnum].func = NULL;

  clear_object(obj_proto + rnum);

  obj_data *obj = obj_proto+rnum;

  obj->in_room = NULL;
  obj->item_number = rnum;

#ifdef USE_DEBUG_CANARIES
  obj->canary = CANARY_VALUE;
#endif

  VTable data;
  data.Parse(&fl);

  obj->text.keywords = str_dup(data.GetString("Keywords", "item unnamed"));
  obj->text.name = str_dup(data.GetString("Name", "an unnamed item"));
  obj->text.room_desc =
    str_dup(data.GetString("RoomDesc", "An unfinished item is here.\n"));
  obj->text.look_desc =
    str_dup(data.GetString("LookDesc", "It looks pretty unfinished.\n"));

  GET_OBJ_TYPE(obj) = data.LookupInt("Type", item_types, ITEM_OTHER);
  if (GET_OBJ_TYPE(obj) == ITEM_QUEST__DO_NOT_USE)
    GET_OBJ_TYPE(obj) = ITEM_OTHER;
  GET_OBJ_WEAR(obj).FromString(data.GetString("WearFlags", "0"));
  GET_OBJ_EXTRA(obj).FromString(data.GetString("ExtraFlags", "0"));
  GET_OBJ_AFFECT(obj).FromString(data.GetString("AffFlags", "0"));
  GET_OBJ_AVAILTN(obj) = data.GetInt("POINTS/AvailTN", 0);
  GET_OBJ_AVAILDAY(obj) = data.GetFloat("POINTS/AvailDay", 0);
  GET_LEGAL_NUM(obj) = data.GetInt("POINTS/LegalNum", 0);
  GET_LEGAL_CODE(obj) = data.GetInt("POINTS/LegalCode", 0);
  GET_LEGAL_PERMIT(obj) = data.GetInt("POINTS/LegalPermit", 0);
  GET_OBJ_STREET_INDEX(obj) = data.GetFloat("POINTS/StreetIndex", 0);
  if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
    GET_WEAPON_INTEGRAL_RECOIL_COMP(obj) = data.GetInt("POINTS/InnateRecoilComp", 0);
  obj->obj_flags.material = data.LookupInt("Material", material_names, 5);

  // No such thing as a negative-weight object.
  GET_OBJ_WEIGHT(obj) = MAX(0, data.GetFloat("POINTS/Weight", 0));
  GET_OBJ_BARRIER(obj) = data.GetInt("POINTS/Barrier", 3);
  GET_OBJ_CONDITION(obj) = GET_OBJ_BARRIER(obj);

  GET_OBJ_COST(obj) = data.GetInt("POINTS/Cost", 0);

  GET_OBJ_QUEST_CHAR_ID(obj) = 0;

  int i;
  for (i = 0; i < NUM_OBJ_VALUES; i++) {
    char field[32];
    snprintf(field, sizeof(field), "VALUES/Val%d", i);

    GET_OBJ_VAL(obj, i) = data.GetInt(field, 0);
  }

  // Set the do-not-touch flags for known templated items.
  if ((BOTTOM_OF_TEMPLATE_ITEMS <= nr && nr <= TOP_OF_TEMPLATE_ITEMS)
      || nr == OBJ_VEHCONTAINER || nr == OBJ_SHOPCONTAINER || nr == OBJ_ONE_SHOT_HEALING_INJECTOR) {
    GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_DONT_TOUCH);
  }

  // Set !DONATE / !SELL / KEPT on vehicle titles.
  if (obj_is_a_vehicle_title(obj)) {
    GET_OBJ_EXTRA(obj).SetBits(ITEM_EXTRA_NODONATE, ITEM_EXTRA_NOSELL, ITEM_EXTRA_KEPT, ENDBIT);
  }

  { // Per-type modifications and settings.
    int mult;
    const char *type_as_string = NULL;
    switch (GET_OBJ_TYPE(obj)) {
      case ITEM_KEY:
        // Remove the default soulbind bug.
        GET_KEY_SOULBOND(obj) = 0;
        break;
      case ITEM_DOCWAGON:
        GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_NODONATE);
        break;
      case ITEM_MOD:
        // Weapon mounts go on all vehicle types.
        if (GET_VEHICLE_MOD_TYPE(obj) == TYPE_MOUNT) {
          for (int bit = 0; bit < NUM_VEH_TYPES; bit++) {
            GET_VEHICLE_MOD_DESIGNED_FOR_FLAGS(obj) |= 1 << bit;
          }
          GET_VEHICLE_MOD_LOCATION(obj) = MOD_MOUNT;
        }
        break;
      case ITEM_CHIP:
        GET_CHIP_SIZE(obj) = (GET_CHIP_RATING(obj) * GET_CHIP_RATING(obj)) * 3;
        GET_OBJ_AVAILTN(obj) = 6;
        GET_OBJ_AVAILDAY(obj) = 4;
        if (!skills[GET_CHIP_SKILL(obj)].is_knowledge_skill)
          GET_OBJ_COST(obj) = GET_CHIP_SIZE(obj) * 100;
        else if (GET_CHIP_SKILL(obj) >= SKILL_ENGLISH) {
          GET_OBJ_COST(obj) = GET_CHIP_SIZE(obj) * 50;
          GET_OBJ_AVAILDAY(obj) = 1.5;
        } else {
          GET_OBJ_COST(obj) = GET_CHIP_SIZE(obj) * 150;
          GET_OBJ_AVAILTN(obj) = 5;
        }
        break;
      case ITEM_DRUG:
        if (GET_OBJ_DRUG_DOSES(obj) <= 0)
          GET_OBJ_DRUG_DOSES(obj) = 1;
        if (GET_OBJ_DRUG_TYPE(obj) < MIN_DRUG || GET_OBJ_DRUG_TYPE(obj) >= NUM_DRUGS) {
          log_vfprintf("BUILD ERROR: Drug %s had invalid type %d!", GET_OBJ_NAME(obj), GET_OBJ_DRUG_TYPE(obj));
          GET_OBJ_COST(obj) = 0;
        } else {
          GET_OBJ_COST(obj) = GET_OBJ_DRUG_DOSES(obj) * drug_types[GET_OBJ_DRUG_TYPE(obj)].cost;
          GET_OBJ_STREET_INDEX(obj) = drug_types[GET_OBJ_DRUG_TYPE(obj)].street_idx;
          GET_OBJ_WEIGHT(obj) = 0.01 * GET_OBJ_DRUG_DOSES(obj);
        }
        break;
      case ITEM_PATCH:
        switch (GET_PATCH_TYPE(obj)) {
          case PATCH_ANTIDOTE:
            // Antidote patches aren't implemented.
            obj->obj_flags.extra_flags.SetBit(ITEM_EXTRA_NERPS);
            break;
          case PATCH_TRAUMA:
            // Trauma patches are expected to apply the Stabilize flag.
            obj->obj_flags.bitvector.SetBit(AFF_STABILIZE);
            // fall through
          default:
            // All patches other than antidote are implemented, so make sure they're not flagged.
            obj->obj_flags.extra_flags.RemoveBit(ITEM_EXTRA_NERPS);
            break;
        }
        break;
      case ITEM_CYBERWARE:
        price_cyber(obj);
        obj->obj_flags.wear_flags.SetBit(ITEM_WEAR_TAKE);
        break;
      case ITEM_BIOWARE:
        price_bio(obj);
        obj->obj_flags.wear_flags.SetBit(ITEM_WEAR_TAKE);
        break;
      case ITEM_PROGRAM:
        if (GET_OBJ_VAL(obj, 0) == SOFT_ATTACK)
          mult = attack_multiplier[GET_OBJ_VAL(obj, 3) - 1];
        else
          mult = programs[GET_OBJ_VAL(obj, 0)].multiplier;
        GET_OBJ_VAL(obj, 2) = (GET_OBJ_VAL(obj, 1) * GET_OBJ_VAL(obj, 1)) * mult;
        if (GET_OBJ_VAL(obj, 1) < 4) {
          GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 2) * 100;
          GET_OBJ_AVAILTN(obj) = 2;
          GET_OBJ_AVAILDAY(obj) = 7;
        } else if (GET_OBJ_VAL(obj, 1) < 7) {
          GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 2) * 200;
          GET_OBJ_AVAILTN(obj) = 4;
          GET_OBJ_AVAILDAY(obj) = 7;
        } else if (GET_OBJ_VAL(obj, 1) < 10) {
          GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 2) * 500;
          GET_OBJ_AVAILTN(obj) = 8;
          GET_OBJ_AVAILDAY(obj) = 14;
        } else {
          GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 2) * 1000;
          GET_OBJ_AVAILTN(obj) = 16;
          GET_OBJ_AVAILDAY(obj) = 30;
        }
        GET_OBJ_WEIGHT(obj) = 0.02;
        break;
      case ITEM_SPELL_FORMULA:
        GET_OBJ_AVAILTN(obj) = GET_OBJ_VAL(obj, 0);
        GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 0);
        switch (spells[GET_OBJ_VAL(obj, 1)].draindamage) {
          case LIGHT:
            GET_OBJ_COST(obj) *= 50;
            GET_OBJ_AVAILDAY(obj) = 1;
            break;
          case MODERATE:
            GET_OBJ_COST(obj) *= 100;
            GET_OBJ_AVAILDAY(obj) = 2;
            break;
          case SERIOUS:
            GET_OBJ_COST(obj) *= 500;
            GET_OBJ_AVAILDAY(obj) = 4;
            break;
          case DEADLY:
          default:
            GET_OBJ_COST(obj) *= 1000;
            GET_OBJ_AVAILDAY(obj) = 7;
            break;
        }
        break;
      case ITEM_GUN_AMMO:
        // Magic number skip: We don't touch item 121, which is a template.
        if (GET_OBJ_VNUM(obj) == OBJ_BLANK_AMMOBOX)
          break;

        // Force all taser ammo to be normal ammo.
        if (GET_AMMOBOX_WEAPON(obj) == WEAP_TASER) {
          GET_AMMOBOX_TYPE(obj) = AMMO_NORMAL;
        }

        if (GET_AMMOBOX_WEAPON(obj) == WEAP_CANNON) {
          // Assault cannons have special ammo and special rules that aren't reflected in our normal table.

          // Max size 500-- otherwise it's too heavy to carry.
          GET_AMMOBOX_QUANTITY(obj) = MAX(MIN(GET_AMMOBOX_QUANTITY(obj), 500), 10);

          // Set values according to Assault Cannon ammo (SR3 p281).
          GET_OBJ_WEIGHT(obj) = (((float) GET_AMMOBOX_QUANTITY(obj)) * 1.25) / 10;
          GET_OBJ_AVAILDAY(obj) = 3;
          GET_OBJ_AVAILTN(obj) = 5;
          GET_OBJ_STREET_INDEX(obj) = 2;

          // Assault Cannon ammo may only ever be explosive (SR3 p279).
          GET_AMMOBOX_TYPE(obj) = AMMO_EXPLOSIVE;
        } else {
          // Max size 1000-- otherwise it's too heavy to carry.
          GET_AMMOBOX_QUANTITY(obj) = MAX(MIN(GET_AMMOBOX_QUANTITY(obj), 1000), 10);

          // Update weight.
          GET_OBJ_WEIGHT(obj) = get_ammo_weight(GET_AMMOBOX_WEAPON(obj), GET_AMMOBOX_TYPE(obj), GET_AMMOBOX_QUANTITY(obj), NULL);

          // Set the TNs for this ammo per the default values.
          GET_OBJ_AVAILDAY(obj) = ammo_type[GET_AMMOBOX_TYPE(obj)].time;
          GET_OBJ_AVAILTN(obj) = ammo_type[GET_AMMOBOX_TYPE(obj)].tn;

          // Set the street index.
          GET_OBJ_STREET_INDEX(obj) = ammo_type[GET_AMMOBOX_TYPE(obj)].street_index;
        }

        // Calculate ammo cost from its quantity, type, and weapon type (this last bit is a house rule).
        GET_OBJ_COST(obj) = GET_AMMOBOX_QUANTITY(obj) * ammo_type[GET_AMMOBOX_TYPE(obj)].cost * weapon_type_ammo_cost_multipliers[GET_AMMOBOX_WEAPON(obj)];

        // Set the strings-- we want all these things to match for simplicity's sake.
        type_as_string = get_weapon_ammo_name_as_string(GET_AMMOBOX_WEAPON(obj));

        if (GET_AMMOBOX_WEAPON(obj)) {
          snprintf(buf, sizeof(buf), "metal ammo ammunition box %s %s %d-%s %s%s",
                  GET_AMMOBOX_WEAPON(obj) == WEAP_CANNON ? "normal" : ammo_type[GET_AMMOBOX_TYPE(obj)].name,
                  weapon_types[GET_AMMOBOX_WEAPON(obj)],
                  GET_AMMOBOX_QUANTITY(obj),
                  type_as_string,
                  type_as_string,
                  GET_AMMOBOX_QUANTITY(obj) > 1 ? "s" : "");
        } else {
          strlcpy(buf, "metal ammo ammunition box nondescript", sizeof(buf));
        }
        // log_vfprintf("Changing %s to %s for %ld.", obj->text.keywords, buf, nr);
        DELETE_ARRAY_IF_EXTANT(obj->text.keywords);
        obj->text.keywords = str_dup(buf);

        if (GET_AMMOBOX_WEAPON(obj)) {
          snprintf(buf, sizeof(buf), "a %d-%s box of %s %s ammunition",
                  GET_AMMOBOX_QUANTITY(obj),
                  type_as_string,
                  ammo_type[GET_AMMOBOX_TYPE(obj)].name,
                  weapon_types[GET_AMMOBOX_WEAPON(obj)]);
        } else {
          strlcpy(buf, "a nondescript box of ammunition", sizeof(buf));
        }
        // log_vfprintf("Changing %s to %s for %ld.", obj->text.name, buf, nr);
        DELETE_ARRAY_IF_EXTANT(obj->text.name);
        obj->text.name = str_dup(buf);


        if (GET_AMMOBOX_WEAPON(obj)) {
          snprintf(buf, sizeof(buf), "A metal box of %s %s %s%s has been left here.",
                  GET_AMMOBOX_WEAPON(obj) == WEAP_CANNON ? "normal" : ammo_type[GET_AMMOBOX_TYPE(obj)].name,
                  weapon_types[GET_AMMOBOX_WEAPON(obj)],
                  type_as_string,
                  GET_AMMOBOX_QUANTITY(obj) > 1 ? "s" : "");
        } else {
          strlcpy(buf, "A metal box of ammunition has been left here.", sizeof(buf));
        }
        // log_vfprintf("Changing %s to %s for %ld.", obj->text.room_desc, buf, nr);
        DELETE_ARRAY_IF_EXTANT(obj->text.room_desc);
        obj->text.room_desc = str_dup(buf);

        // log_vfprintf("Changing %s to %s for %ld.", obj->text.look_desc, buf, nr);
        DELETE_ARRAY_IF_EXTANT(obj->text.look_desc);
        obj->text.look_desc = str_dup("A hefty box of ammunition, banded in metal and secured with flip-down hasps for transportation and storage.");
        break;
      case ITEM_WEAPON:
        {
          // Attempt to automatically rectify broken weapons.
          bool is_melee = FALSE;
          if (GET_WEAPON_ATTACK_TYPE(obj) > MAX_WEAP)
            switch (GET_WEAPON_SKILL(obj)) {
              case SKILL_EDGED_WEAPONS:
                GET_WEAPON_ATTACK_TYPE_SETTABLE(obj) = WEAP_EDGED;
                is_melee = TRUE;
                break;
              case SKILL_POLE_ARMS:
                GET_WEAPON_ATTACK_TYPE_SETTABLE(obj) = WEAP_POLEARM;
                is_melee = TRUE;
                break;
              case SKILL_WHIPS_FLAILS:
                GET_WEAPON_ATTACK_TYPE_SETTABLE(obj) = WEAP_WHIP;
                is_melee = TRUE;
                break;
              case SKILL_CLUBS:
                GET_WEAPON_ATTACK_TYPE_SETTABLE(obj) = WEAP_CLUB;
                is_melee = TRUE;
                break;
              case SKILL_UNARMED_COMBAT:
                GET_WEAPON_ATTACK_TYPE_SETTABLE(obj) = WEAP_GLOVE;
                is_melee = TRUE;
                break;
              case SKILL_PISTOLS:
                GET_WEAPON_ATTACK_TYPE_SETTABLE(obj) = WEAP_HEAVY_PISTOL;
                break;
              case SKILL_RIFLES:
                GET_WEAPON_ATTACK_TYPE_SETTABLE(obj) = WEAP_SPORT_RIFLE;
                break;
              case SKILL_SHOTGUNS:
                GET_WEAPON_ATTACK_TYPE_SETTABLE(obj) = WEAP_SHOTGUN;
                break;
              case SKILL_ASSAULT_RIFLES:
                GET_WEAPON_ATTACK_TYPE_SETTABLE(obj) = WEAP_ASSAULT_RIFLE;
                break;
              case SKILL_SMG:
                GET_WEAPON_ATTACK_TYPE_SETTABLE(obj) = WEAP_SMG;
                break;
              case SKILL_GRENADE_LAUNCHERS:
                GET_WEAPON_ATTACK_TYPE_SETTABLE(obj) = WEAP_GREN_LAUNCHER;
                break;
              case SKILL_MISSILE_LAUNCHERS:
                GET_WEAPON_ATTACK_TYPE_SETTABLE(obj) = WEAP_MISS_LAUNCHER;
                break;
              case SKILL_TASERS:
                GET_WEAPON_ATTACK_TYPE_SETTABLE(obj) = WEAP_TASER;
                break;
              case SKILL_MACHINE_GUNS:
                GET_WEAPON_ATTACK_TYPE_SETTABLE(obj) = WEAP_LMG;
                break;
              case SKILL_ASSAULT_CANNON:
                GET_WEAPON_ATTACK_TYPE_SETTABLE(obj) = WEAP_CANNON;
                break;
            }

          if (GET_WEAPON_SKILL(obj) > 100) {
            log_vfprintf("WARNING: get_weapon_skill for %s is %d. Prior logic would have made it %d.",
                        GET_OBJ_NAME(obj),
                        GET_WEAPON_SKILL(obj),
                        GET_WEAPON_SKILL(obj) - 100);
            // GET_WEAPON_SKILL(obj) -= 100;
          }

          if (is_melee)
            GET_WEAPON_REACH(obj) = MAX(0, GET_WEAPON_REACH(obj));
        }
        break;
      case ITEM_DECK_ACCESSORY:
        // Fix laptops.
        if (GET_DECK_ACCESSORY_TYPE(obj) == TYPE_COMPUTER) {
          if (GET_DECK_ACCESSORY_COMPUTER_USED_MEMORY(obj) > 0) {
            GET_DECK_ACCESSORY_COMPUTER_USED_MEMORY(obj) = 0;
            GET_DECK_ACCESSORY_COMPUTER_IS_LAPTOP(obj) = 1;
          }
        }
        break;
    }
  } // End per-type modifications.

  // Auto-repair deckbuilding tokens to look right.
  if (GET_OBJ_VNUM(obj) == OBJ_STAFF_REBATE_FOR_CRAFTING) {
    delete [] obj->text.keywords;
    obj->text.keywords = str_dup("rapid crafting token ooc item");

    delete [] obj->text.name;
    obj->text.name = str_dup("a rapid crafting token (OOC item)");

    delete [] obj->text.room_desc;
    obj->text.room_desc = str_dup("[OOC item] A rapid crafting token has been left here.\n");

    delete [] obj->text.look_desc;
    obj->text.look_desc = str_dup("If you have this in your inventory when performing any deckbuilding or spell designing action, the task will be greatly accelerated at the cost of one crafting rebate token.\n");

    GET_OBJ_TYPE(obj) = ITEM_OTHER;
  }

  // read in affects
  for (i = 0; i < MAX_OBJ_AFFECT; i++) {
    char sect[16];
    snprintf(sect, sizeof(sect), "AFFECT %d", i);

    obj->affected[i].location = APPLY_NONE;
    obj->affected[i].modifier = 0;

    if (data.NumFields(sect) > 1) {
      char field[64];
      snprintf(field, sizeof(field), "%s/Location", sect);

      int loc = data.LookupInt(field, apply_types, APPLY_NONE);

      if (loc == APPLY_NONE) {
        log_vfprintf("Item #%ld's affect #%d had no location -- skipping", nr, i);
        continue;
      }

      obj->affected[i].location = loc;

      snprintf(field, sizeof(field), "%s/Modifier", sect);
      obj->affected[i].modifier = data.GetInt(field, 0);
    }
  }

  // Read in source book data, if any.
  if (data.DoesFieldExist("SourceBook")) {
    obj->source_info = str_dup(data.GetString("SourceBook", "(none)"));
  }

  // finally, read in extra descriptions
  for (i = 0; true; i++) {
    char sect[16];
    snprintf(sect, sizeof(sect), "EXTRADESC %d", i);

    if (data.NumFields(sect) > 0) {
      char field[64];

      snprintf(field, sizeof(field), "%s/Keywords", sect);
      char *keywords = str_dup(data.GetString(field, NULL));

      if (!keywords) {
        log_vfprintf("Item #%ld's extra description #%d had no keywords -- skipping",
            nr, i);
        continue;
      }

      extra_descr_data *desc = new extra_descr_data;
      desc->keyword = keywords;
      snprintf(field, sizeof(field), "%s/Desc", sect);
      desc->description = str_dup(data.GetString(field, NULL));

      desc->next = obj->ex_description;
      obj->ex_description = desc;
    } else
      break;
  }

  top_of_objt = rnum++;
}

void parse_quest(File &fl, long virtual_nr)
{
  static long quest_nr = 0;
  long j, t[20];
  char line[256];

  memset(t, 0, sizeof(t));

  quest_table[quest_nr].vnum = virtual_nr;

  fl.GetLine(line, 256, FALSE);
  if (sscanf(line, "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
             t, t + 1, t + 2, t + 3, t + 4, t + 5,
             t + 6, t + 7, t + 8, t + 9, t + 10, t + 11, t+12, t+13, t+14, t+15, t+16, t+17, t+18) < 12) {
    fprintf(stderr, "FATAL ERROR: Format error in quest #%ld, expecting 12-19 numbers like '# # # # # # # # # # # #'. Got '%s' instead.\n",
            virtual_nr, line);
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }
  quest_table[quest_nr].johnson = t[0];
  quest_table[quest_nr].time = t[1];
  quest_table[quest_nr].min_rep = t[2];
  quest_table[quest_nr].max_rep = t[3];
  quest_table[quest_nr].nuyen = t[4];
  quest_table[quest_nr].karma = t[5];
  quest_table[quest_nr].reward = (real_object(t[6]) > -1 ? t[6] : -1);
  quest_table[quest_nr].num_objs = t[7];
  quest_table[quest_nr].num_mobs = t[8];
  quest_table[quest_nr].s_time = t[9];
  quest_table[quest_nr].e_time = t[10];
  quest_table[quest_nr].s_room = t[11];

  int num_intro_emote = t[12];
  int num_decline_emote = t[13];
  int num_quit_emote = t[14];
  int num_finish_emote = t[15];
  int num_info_emotes = t[16];
  
  quest_table[quest_nr].prerequisite_quest = t[17];
  quest_table[quest_nr].disqualifying_quest = t[18];

  if (quest_table[quest_nr].num_objs > 0) {
    quest_table[quest_nr].obj = new quest_om_data[quest_table[quest_nr].num_objs];
    memset(quest_table[quest_nr].obj, 0, sizeof(quest_om_data) * quest_table[quest_nr].num_objs);
    for (j = 0; j < quest_table[quest_nr].num_objs; j++) {
      fl.GetLine(line, 256, FALSE);
      if (sscanf(line, "%ld %ld %ld %ld %ld %ld %ld %ld", t, t + 1, t + 2, t + 3,
                 t + 4, t + 5, t + 6, t + 7) != 8) {
        fprintf(stderr, "FATAL ERROR: Format error in quest #%ld, obj #%ld: expecting 8 numbers like '# # # # # # # #'\n", quest_nr, j);
        exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
      }
      quest_table[quest_nr].obj[j].vnum = t[0];
      quest_table[quest_nr].obj[j].nuyen = t[1];
      quest_table[quest_nr].obj[j].karma = t[2];
      quest_table[quest_nr].obj[j].load = (byte) t[3];
      quest_table[quest_nr].obj[j].objective = (byte) t[4];
      quest_table[quest_nr].obj[j].l_data = t[5];
      quest_table[quest_nr].obj[j].l_data2 = t[6];
      quest_table[quest_nr].obj[j].o_data = t[7];
    }
  } else
    quest_table[quest_nr].obj = NULL;

  if (quest_table[quest_nr].num_mobs > 0) {
    quest_table[quest_nr].mob = new quest_om_data[quest_table[quest_nr].num_mobs];
    memset(quest_table[quest_nr].mob, 0, sizeof(quest_om_data) * quest_table[quest_nr].num_mobs);
    for (j = 0; j < quest_table[quest_nr].num_mobs; j++) {
      fl.GetLine(line, 256, FALSE);
      if (sscanf(line, "%ld %ld %ld %ld %ld %ld %ld %ld", t, t + 1, t + 2, t + 3,
                 t + 4, t + 5, t + 6, t + 7) != 8) {
        fprintf(stderr, "FATAL ERROR: Format error in quest #%ld, mob #%ld: expecting 8 numbers like '# # # # # # # #'\n", quest_nr, j);
        exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
      }
      quest_table[quest_nr].mob[j].vnum = t[0];
      quest_table[quest_nr].mob[j].nuyen = t[1];
      quest_table[quest_nr].mob[j].karma = t[2];
      quest_table[quest_nr].mob[j].load = (byte) t[3];
      quest_table[quest_nr].mob[j].objective = (byte) t[4];
      quest_table[quest_nr].mob[j].l_data = t[5];
      quest_table[quest_nr].mob[j].l_data2 = t[6];
      quest_table[quest_nr].mob[j].o_data = t[7];
    }
  } else {
    quest_table[quest_nr].mob = NULL;
  }

  #define READ_EMOTES(type)                                                               \
  if (num_ ## type ## _emotes > 0) {                                                      \
    quest_table[quest_nr]. type ## _emotes = new emote_vector_t;                          \
    while (num_ ## type ## _emotes-- > 0) {                                               \
      quest_table[quest_nr]. type ## _emotes->push_back(fl.ReadString("##type##_emote")); \
    }                                                                                     \
  }

  if (num_intro_emote > 0) {
    quest_table[quest_nr].intro_emote = fl.ReadString("intro_emote");
  }
  if (num_decline_emote > 0) {
    quest_table[quest_nr].decline_emote = fl.ReadString("decline_emote");
  }
  if (num_quit_emote > 0) {
    quest_table[quest_nr].quit_emote = fl.ReadString("quit_emote");
  }
  if (num_finish_emote > 0) {
    quest_table[quest_nr].finish_emote = fl.ReadString("finish_emote");
  }

  READ_EMOTES(info);

  #undef READ_EMOTES

  quest_table[quest_nr].intro = fl.ReadString("intro");
  quest_table[quest_nr].decline = fl.ReadString("decline");
  quest_table[quest_nr].quit = fl.ReadString("quit");
  quest_table[quest_nr].finish = fl.ReadString("finish");
  quest_table[quest_nr].info = fl.ReadString("info");
  quest_table[quest_nr].s_string = fl.ReadString("s_string");
  quest_table[quest_nr].e_string = fl.ReadString("e_string");
  quest_table[quest_nr].done = fl.ReadString("done");

  /* Alright, here's the situation. I was going to add in a location field for
     the quests, which would show up in the recap and help newbies out... BUT.
     Turns out we use a shit-tacular file format that's literally just 'crap out
     strings into a file and require that they exist, no defaulting allowed, or
     you can't load any quests and the game dies'. Gotta love that jank-ass code.

     This feature is disabled until someone transitions all the quests into an
     actually sensible file format. -- LS */
#ifdef USE_QUEST_LOCATION_CODE
  quest_table[quest_nr].location = NULL; //fl.ReadString("location");
#endif

  top_of_questt = quest_nr++;
}

void parse_shop(File &fl, long virtual_nr)
{
  static long last_seen = -1;
  if (last_seen == -1) {
    last_seen = virtual_nr;
  } else if (last_seen >= virtual_nr) {
    snprintf(buf, sizeof(buf), "FATAL ERROR in parse_shop(%s, %ld): last_seen %ld >= nr %ld.", fl.Filename(), virtual_nr, last_seen, virtual_nr);
    log(buf);
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }

  static DBIndex::rnum_t rnum = 0, zone = 0;
  char field[64];
  if (virtual_nr <= (zone ? zone_table[zone - 1].top : -1)) {
    log_vfprintf("FATAL ERROR: Shop #%ld is below zone %d.\n", virtual_nr, zone_table[zone].number);
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }
  while (virtual_nr > zone_table[zone].top)
    if (++zone > top_of_zone_table) {
      log_vfprintf("FATAL ERROR: Shop %ld is outside of any zone.\n", virtual_nr);
      exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
    }
  shop_data *shop = shop_table+rnum;
  shop->vnum = virtual_nr;
  VTable data;
  data.Parse(&fl);

  shop->keeper = data.GetLong("Keeper", -1);
  shop->profit_buy = data.GetFloat("ProfitBuy", 0);
  shop->profit_sell = data.GetFloat("ProfitSell", 0);
  shop->random_amount = data.GetInt("Random", 0);
  shop->open = data.GetInt("Open", 0);
  shop->close = data.GetInt("Close", 24);
  shop->type = data.LookupInt("Type", shop_type, SHOP_GREY);
  shop->no_such_itemk = str_dup(data.GetString("NoSuchItemKeeper", "I don't have that."));
  shop->no_such_itemp = str_dup(data.GetString("NoSuchItemPlayer", "You don't have that."));
  shop->not_enough_nuyen = str_dup(data.GetString("NoNuyen", "You don't have enough nuyen."));
  shop->doesnt_buy = str_dup(data.GetString("DoesntBuy", "I don't buy that."));
  shop->buy = str_dup(data.GetString("Buy", "That'll be %d nuyen."));
  shop->sell = str_dup(data.GetString("Sell", "Here, %d nuyen."));
  shop->shopname = str_dup(data.GetString("Name", "Unnamed Shop"));
  shop->flags.FromString(data.GetString("Flags", "0"));
  shop->races.FromString(data.GetString("Races", "0"));
  shop->buytypes.FromString(data.GetString("BuyTypes", "0"));
  shop->etiquette = data.GetInt("Etiquette", SKILL_STREET_ETIQUETTE);
  int num_fields = data.NumSubsections("SELLING"), vnum;
  struct shop_sell_data *templist = NULL;

  // Cap the shop multipliers.
  if (shop->flags.IsSet(SHOP_CHARGEN)) {
    shop->profit_buy = 1.0;
    shop->profit_sell = 1.0;
  } else {
    // It should be impossible to set a buy price lower than 1.0.
    shop->profit_buy = MAX(1.0, shop->profit_buy);

    // Standardize doc cyberware/bioware buy prices.
    if (shop->flags.IsSet(SHOP_DOCTOR)) {
      shop->profit_sell = CYBERDOC_MAXIMUM_SELL_TO_SHOPKEEP_MULTIPLIER;
    }
  }

  // snprintf(buf3, sizeof(buf3), "Parsing shop items for shop %ld (%d found).", virtual_nr, num_fields);
  for (int x = 0; x < num_fields; x++) {
    const char *name = data.GetIndexSection("SELLING", x);
    snprintf(field, sizeof(field), "%s/Vnum", name);
    vnum = data.GetLong(field, 0);
    // snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "\r\n - %s (%d)", name, vnum);
    if (real_object(vnum) < 1) {
      // snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), " - nonexistant! Skipping.");
      continue;
    }
    shop_sell_data *sell = new shop_sell_data;
    sell->vnum = vnum;
    snprintf(field, sizeof(field), "%s/Type", name);
    sell->type = data.LookupInt(field, selling_type, SELL_ALWAYS);
    snprintf(field, sizeof(field), "%s/Stock", name);
    sell->stock = data.GetInt(field, 0);
    if (!templist)
      templist = sell;
    else
      for (struct shop_sell_data *temp = templist;; temp = temp->next)
        if (temp->next == NULL) {
          temp->next = sell;
          break;
        }
    // snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), ": type %d, stock %d.", sell->type, sell->stock);
  }
  // mudlog(buf3, NULL, LOG_SYSLOG, TRUE);
  shop->selling = templist;
  top_of_shopt = rnum++;
}

#define Z       zone_table[zone]

/* load the zone table and command tables */
void load_zones(File &fl)
{
  static int zone = 0;
  int cmd_no = 0, tmp, error;
  char *ptr, buf[256];

  Z.num_cmds = 0;
  while (fl.GetLine(buf, 256, FALSE) && *buf != '$')
    Z.num_cmds++;

  if (Z.num_cmds < 4) {
    fprintf(stderr, "FATAL ERROR: Zone file %s is malformed and missing the initial four lines required of a zone file.", fl.Filename());
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }

  // subtract the first 4 lines
  Z.num_cmds -= 4;

  fl.Rewind();

  if (Z.num_cmds == 0)
    Z.cmd = NULL;
  else {
    Z.cmd = new struct reset_com[Z.num_cmds];
    memset(Z.cmd, 0, sizeof(reset_com) * Z.num_cmds);
  }

  fl.GetLine(buf, 256, FALSE);

  if (sscanf(buf, "#%d", &Z.number) != 1) {
    fprintf(stderr, "FATAL ERROR: Format error in %s, line %d: Expected '#' followed by a number.\n",
            fl.Filename(), fl.LineNumber());
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }
  fl.GetLine(buf, 256, FALSE);
  if ((ptr = strchr(buf, '~')) != NULL)   /* take off the '~' if it's there */
    *ptr = '\0';
  Z.name = str_dup(buf);

  fl.GetLine(buf, 256, FALSE);
  // Attempt to read our new dual-mode locks from the line.
  if (sscanf(buf, " %d %d %d %d %d %d %d %d %d %d %d",
             &Z.top, &Z.lifespan, &Z.reset_mode,
             &Z.security, &Z.editing_restricted_to_admin, &Z.approved, &Z.jurisdiction, &Z.is_pghq, &Z.locked_to_non_editors,
             &Z.default_aura_type, &Z.default_aura_force) < 11)
  {
    // Attempt to read background count from the line.
    if (sscanf(buf, " %d %d %d %d %d %d %d %d %d %d",
              &Z.top, &Z.lifespan, &Z.reset_mode,
              &Z.security, &Z.old_connected_value, &Z.jurisdiction, &Z.is_pghq, &Z.locked_to_non_editors,
              &Z.default_aura_type, &Z.default_aura_force) < 10)
    {
      // Attempt to read the locked flag from the line.
      if (sscanf(buf, " %d %d %d %d %d %d %d %d",
                &Z.top, &Z.lifespan, &Z.reset_mode,
                &Z.security, &Z.old_connected_value, &Z.jurisdiction, &Z.is_pghq, &Z.locked_to_non_editors) < 8)
      {
        // Attempt to read the new PGHQ flag from the line.
        if (sscanf(buf, " %d %d %d %d %d %d %d",
                  &Z.top, &Z.lifespan, &Z.reset_mode,
                  &Z.security, &Z.old_connected_value, &Z.jurisdiction, &Z.is_pghq) < 7)
        {
          // Fallback: Instead, read out the old format. Assume we'll save PGHQ data later.
          if (sscanf(buf, " %d %d %d %d %d %d",
                    &Z.top, &Z.lifespan, &Z.reset_mode,
                    &Z.security, &Z.old_connected_value, &Z.jurisdiction) < 5) {
            fprintf(stderr, "FATAL ERROR: Format error in 6-constant line of %s: Expected six numbers like ' # # # # # #'.", fl.Filename());
            exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
          }
          // Failed to read this, so assume it's 0.
          Z.is_pghq = 0;
        }
        // Failed to read this, so assume it's 0.
        Z.locked_to_non_editors = 0;
      }
      // Failed to read this, so assume it's 0 for both.
      Z.default_aura_type = Z.default_aura_force = 0;
    }
    // Since we read into the old connected value, clone it out to the new dual-mode locks.
    Z.approved = Z.editing_restricted_to_admin = Z.old_connected_value;
  }

  fl.GetLine(buf, 256, FALSE);
  // This next section reads in the id nums of the players that can edit
  // this zone.
  if (sscanf(buf, "%d %d %d %d %d", &Z.editor_ids[0], &Z.editor_ids[1],
             &Z.editor_ids[2], &Z.editor_ids[3], &Z.editor_ids[4]) != NUM_ZONE_EDITOR_IDS) {
    fprintf(stderr, "FATAL ERROR: Format error in editor id list of %s: Expected five numbers like '# # # # #'", fl.Filename());
    exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
  }

  cmd_no = 0;

  while (cmd_no < Z.num_cmds) {
    if (!fl.GetLine(buf, 256, FALSE)) {
      fprintf(stderr, "Format error in %s - premature end of file\n",
              fl.Filename());
      exit(ERROR_ZONEREAD_PREMATURE_EOF);
    }

    ptr = buf;
    skip_spaces(&ptr);

    if ((ZCMD.command = *ptr) == '*')
      continue;

    ptr++;

    // 'L' is the old way of writing 'E'. Adapt for it being in the file.
    if (ZCMD.command == 'L') {
      ZCMD.command = 'E';
    }

    error = 0;
    if (strchr("M", ZCMD.command)) { // 4-arg command.
      if (sscanf(ptr, " %d %ld %ld %ld %ld ", &tmp, &ZCMD.arg1, &ZCMD.arg2, &ZCMD.arg3, &ZCMD.arg4) != 5) {
        // There's a chance that we just haven't written this one out as a 5-arg yet. Assume a quantity of 1 and try again.
        if (sscanf(ptr, " %d %ld %ld %ld ", &tmp, &ZCMD.arg1, &ZCMD.arg2, &ZCMD.arg3) != 4) {
          error = 1;
        } else {
          ZCMD.arg4 = 1;
        }
      }
    } else if (strchr("VOHPDEN", ZCMD.command)) { // 3-arg.
      if (sscanf(ptr, " %d %ld %ld %ld ", &tmp, &ZCMD.arg1, &ZCMD.arg2, &ZCMD.arg3) != 4)
        error = 1;
    } else if (strchr("SUIGCR", ZCMD.command)) { // 2-arg.
      if (sscanf(ptr, " %d %ld %ld ", &tmp, &ZCMD.arg1, &ZCMD.arg2) != 3)
        error = 1;
    } else {
      fprintf(stderr, "Format error in %s - unrecognized zone command %c during load.\n", fl.Filename(), ZCMD.command);
      exit(ERROR_ZONEREAD_FORMAT_ERROR);
    }

    ZCMD.if_flag = tmp;

    if (error) {
      fprintf(stderr, "FATAL ERROR: Format error in %s, line %d: '%s'\n",
              fl.Filename(), fl.LineNumber(), buf);
      exit(ERROR_ZONEREAD_FORMAT_ERROR);
    }
    ZCMD.line = fl.LineNumber();
    cmd_no++;
  }
  top_of_zone_table = zone++;
}

#undef Z

/*************************************************************************
*  procedures for resetting, both play-time and boot-time                *
*********************************************************************** */

int vnum_mobile_karma(char *searchname, struct char_data * ch)
{
  SPECIAL(shop_keeper);
  extern int calc_karma(struct char_data *ch, struct char_data *vict);
  int nr, found = 0;
  int karma;
  int i;
  int highest_karma;

  for (i = 10000; i > 0; i = highest_karma)
  {
    highest_karma = 0;
    for (nr = 0; nr <= top_of_mobt; nr++) {
      if (mob_index[nr].func == shop_keeper
          ||mob_index[nr].sfunc == shop_keeper)
        continue;
      karma = calc_karma(NULL, &mob_proto[nr]);
      if ( karma < i ) {
        if ( karma > highest_karma )
          highest_karma = karma;
        continue;
      }
      if ( i != 10000 && karma > i )
        continue;
      if (vnum_from_non_approved_zone(MOB_VNUM_RNUM(nr))) {
#ifdef IS_BUILDPORT
        send_to_char(ch, "Allowing non-approved karma calc since we're on the buildport. This would be skipped otherwise.");
#else
        continue;
#endif
      }
      ++found;
      snprintf(buf, sizeof(buf), "[%5ld] %4.2f (%4.2f) %4dx %6.2f %s\r\n",
              MOB_VNUM_RNUM(nr),
              (float)karma/100.0,
              (float)(GET_KARMA(&mob_proto[nr]))/100.0,
              mob_proto[nr].mob_specials.count_death,
              (float)(mob_proto[nr].mob_specials.value_death_karma)/100.0,
              mob_proto[nr].player.physical_text.name);
      send_to_char(buf, ch);
    }
  }
  return (found);
}

int vnum_mobile_bonuskarma(char *searchname, struct char_data * ch)
{
  SPECIAL(shop_keeper);
  extern int calc_karma(struct char_data *ch, struct char_data *vict);
  int nr, found = 0;
  int karma;
  int i;
  int highest_karma;

  for (i = 10000; i > 0; i = highest_karma )
  {
    highest_karma = 0;
    for (nr = 0; nr <= top_of_mobt; nr++) {
      if (mob_index[nr].func == shop_keeper
          ||mob_index[nr].sfunc == shop_keeper)
        continue;
      if ( GET_KARMA(&mob_proto[nr]) < i ) {
        if ( GET_KARMA(&mob_proto[nr]) > highest_karma )
          highest_karma = GET_KARMA(&mob_proto[nr]);
        continue;
      }
      if ( i != 10000 && GET_KARMA(&mob_proto[nr]) > i )
        continue;
      if (vnum_from_non_approved_zone(MOB_VNUM_RNUM(nr))) {
#ifdef IS_BUILDPORT
        send_to_char(ch, "Allowing non-connected karma calc since we're on the buildport. This would be skipped otherwise.");
#else
        continue;
#endif
      }
      karma = calc_karma(NULL, &mob_proto[nr]);
      ++found;
      snprintf(buf, sizeof(buf), "[%5ld] %4.2f (%4.2f)%4dx %6.2f %s\r\n",
              MOB_VNUM_RNUM(nr),
              (float)karma/100.0,
              (float)(GET_KARMA(&mob_proto[nr]))/100.0,
              mob_proto[nr].mob_specials.count_death,
              (float)(mob_proto[nr].mob_specials.value_death_karma)/100.0,
              mob_proto[nr].player.physical_text.name);
      send_to_char(buf, ch);
    }
  }
  return (found);
}

int vnum_mobile_nuyen(char *searchname, struct char_data * ch)
{
  SPECIAL(shop_keeper);
  extern int calc_karma(struct char_data *ch, struct char_data *vict);
  int nr, found = 0;
  int karma;
  int i;
  int highest_nuyen;
  int nuyen;

  for (i = 1000 * 1000 * 1000; i > 0; i = highest_nuyen)
  {
    highest_nuyen = 0;
    for (nr = 0; nr <= top_of_mobt; nr++) {
      if (mob_index[nr].func == shop_keeper
          ||mob_index[nr].sfunc == shop_keeper)
        continue;
      nuyen = GET_NUYEN(&mob_proto[nr]) + GET_BANK(&mob_proto[nr]);
      if ( nuyen < i ) {
        if ( nuyen > highest_nuyen )
          highest_nuyen = nuyen;
        continue;
      }
      if ( i != 1000*1000*1000 && nuyen > i )
        continue;
      if (vnum_from_non_approved_zone(MOB_VNUM_RNUM(nr))) {
#ifdef IS_BUILDPORT
        send_to_char(ch, "Allowing non-approved nuyen calc since we're on the buildport. This would be skipped otherwise.");
#else
        continue;
#endif
      }
        continue;
      karma = calc_karma(NULL, &mob_proto[nr]);
      ++found;
      snprintf(buf, sizeof(buf), "[%5ld] %4.2f (%4.2f)%4dx [%6d] %s\r\n",
              MOB_VNUM_RNUM(nr),
              (float)karma/100.0,
              (float)(GET_KARMA(&mob_proto[nr]))/100.0,
              mob_proto[nr].mob_specials.count_death,
              nuyen,
              mob_proto[nr].player.physical_text.name);
      send_to_char(buf, ch);
    }
  }
  return (found);
}

int vnum_mobile_valuedeathkarma(char *searchname, struct char_data * ch)
{
  SPECIAL(shop_keeper);
  extern int calc_karma(struct char_data *ch, struct char_data *vict);
  extern struct char_data *mob_proto;
  int nr, found = 0;
  int karma;
  int i;
  int highest_karma;

  for (i = 10000; i > 0; i = highest_karma)
  {
    highest_karma = 0;
    for (nr = 0; nr <= top_of_mobt; nr++) {
      if (mob_index[nr].func == shop_keeper
          ||mob_index[nr].sfunc == shop_keeper)
        continue;
      if ( mob_proto[nr].mob_specials.value_death_karma < i ) {
        if ( mob_proto[nr].mob_specials.value_death_karma > highest_karma )
          highest_karma = mob_proto[nr].mob_specials.value_death_karma;
        continue;
      }
      if ( i != 10000 && mob_proto[nr].mob_specials.value_death_karma > i )
        continue;
      karma = calc_karma(NULL, &mob_proto[nr]);
      ++found;
      snprintf(buf, sizeof(buf), "[%5ld] %4.2f (%4.2f)%4dx %6.2f %s\r\n",
              MOB_VNUM_RNUM(nr),
              (float)karma/100.0,
              (float)(GET_KARMA(&mob_proto[nr]))/100.0,
              mob_proto[nr].mob_specials.count_death,
              (float)(mob_proto[nr].mob_specials.value_death_karma)/100.0,
              mob_proto[nr].player.physical_text.name);
      send_to_char(buf, ch);
    }
  }
  return (found);
}

int vnum_mobile_valuedeath(char *searchname, struct char_data * ch)
{
  SPECIAL(shop_keeper);
  extern int calc_karma(struct char_data *ch, struct char_data *vict);
  int nr, found = 0;
  int karma;
  int highest_death;
  int i;

  for (i = 1000*1000*1000; i > 0; i = highest_death )
  {
    highest_death = 0;
    for (nr = 0; nr <= top_of_mobt; nr++) {
      if (mob_index[nr].func == shop_keeper
          ||mob_index[nr].sfunc == shop_keeper)
        continue;
      if ( (mob_proto[nr].mob_specials.value_death_nuyen
            + mob_proto[nr].mob_specials.value_death_items) < i ) {
        if ( (mob_proto[nr].mob_specials.value_death_nuyen
              + mob_proto[nr].mob_specials.value_death_items) > highest_death )
          highest_death = mob_proto[nr].mob_specials.value_death_nuyen
                          + mob_proto[nr].mob_specials.value_death_items;
        continue;
      }
      if ( (mob_proto[nr].mob_specials.value_death_nuyen
            + mob_proto[nr].mob_specials.value_death_items) > i )
        continue;
      karma = calc_karma(NULL, &mob_proto[nr]);
      ++found;
      snprintf(buf, sizeof(buf), "[%5ld] (%4.2f %4.2f)%4dx [%7d %7d] %s\r\n",
              MOB_VNUM_RNUM(nr),
              (float)karma/100.0,
              (float)(GET_KARMA(&mob_proto[nr]))/100.0,
              mob_proto[nr].mob_specials.count_death,
              mob_proto[nr].mob_specials.value_death_nuyen,
              mob_proto[nr].mob_specials.value_death_items,
              mob_proto[nr].player.physical_text.name);
      send_to_char(buf, ch);
    }
  }
  return (found);
}

int vnum_mobile_valuedeathnuyen(char *searchname, struct char_data * ch)
{
  SPECIAL(shop_keeper);
  extern int calc_karma(struct char_data *ch, struct char_data *vict);
  int nr, found = 0;
  int karma;
  int highest_death;
  int i;


  for (i = 1000*1000*1000; i > 0; i = highest_death )
  {
    highest_death = 0;
    for (nr = 0; nr <= top_of_mobt; nr++) {
      if (mob_index[nr].func == shop_keeper
          ||mob_index[nr].sfunc == shop_keeper)
        continue;
      if ( (mob_proto[nr].mob_specials.value_death_nuyen) < i ) {
        if ( (mob_proto[nr].mob_specials.value_death_nuyen) > highest_death )
          highest_death = mob_proto[nr].mob_specials.value_death_nuyen;
        continue;
      }
      if ( (mob_proto[nr].mob_specials.value_death_nuyen) > i )
        continue;
      karma = calc_karma(NULL, &mob_proto[nr]);
      ++found;
      snprintf(buf, sizeof(buf), "[%5ld] (%4.2f %4.2f) [%7d %7d] %s\r\n",
              MOB_VNUM_RNUM(nr),
              (float)karma/100.0,
              (float)(GET_KARMA(&mob_proto[nr]))/100.0,
              mob_proto[nr].mob_specials.value_death_nuyen,
              mob_proto[nr].mob_specials.value_death_items,
              mob_proto[nr].player.physical_text.name);
      send_to_char(buf, ch);
    }
  }
  return (found);
}
int vnum_mobile_affflag(int i, struct char_data * ch)
{
  int nr, found = 0;

  send_to_char(ch, "The following mobs have the %s affect flag set:\r\n", affected_bits[i]);

  for (nr = 0; nr <= top_of_mobt; nr++)
    if (mob_proto[nr].char_specials.saved.affected_by.IsSet(i))
    {
      snprintf(buf, sizeof(buf), "[%5ld] %s\r\n",
              MOB_VNUM_RNUM(nr),
              mob_proto[nr].player.physical_text.name);
      send_to_char(buf, ch);
      found++;
    }
  return (found);
}

int vnum_vehicles_by_attribute(int vaff_idx, struct char_data *ch) {
  send_to_char(ch, "Displaying all vehicle prototypes sorted by %s (descending):\r\n", veh_aff[vaff_idx]);

  std::vector<struct veh_data *> veh_vect = {};

  for (rnum_t nr = 0; nr <= top_of_veht; nr++) {
    veh_vect.push_back(&veh_proto[nr]);
  }

  std::sort(veh_vect.begin(), 
            veh_vect.end(),
            [vaff_idx](struct veh_data *a, struct veh_data *b) {
              switch (vaff_idx) {
                case VAFF_HAND:
                  return a->handling > b->handling;
                case VAFF_SPD:
                  return a->speed > b->speed;
                case VAFF_ACCL:
                  return a->accel > b->accel;
                case VAFF_BOD:
                  return a->body > b->body;
                case VAFF_ARM:
                  return a->armor > b->armor;
                case VAFF_SIG:
                  return a->sig > b->sig;
                case VAFF_AUTO:
                  return a->autonav > b->autonav;
                case VAFF_SEAF:
                  // should these indexes be swapped?
                  return a->seating[0] > b->seating[0];
                case VAFF_SEAB:
                  return a->seating[1] > b->seating[1];
                case VAFF_LOAD:
                  return a->load > b->load;
                case VAFF_SEN:
                  return a->sensor > b->sensor;
                case VAFF_PILOT:
                  return a->pilot > b->pilot;
                default:
                  // Unsupported sort.
                  return TRUE;
              }
            });
  
  int found = 0, value = 0;
  for (auto *veh : veh_vect) {
    switch (vaff_idx) {
      case VAFF_HAND:
        value = veh->handling;
        break;
      case VAFF_SPD:
        value = veh->speed;
        break;
      case VAFF_ACCL:
        value = veh->accel;
        break;
      case VAFF_BOD:
        value = veh->body;
        break;
      case VAFF_ARM:
        value = veh->armor;
        break;
      case VAFF_SIG:
        value = veh->sig;
        break;
      case VAFF_AUTO:
        value = veh->autonav;
        break;
      case VAFF_SEAF:
        // should these indexes be swapped?
        value = veh->seating[0];
        break;
      case VAFF_SEAB:
        value = veh->seating[1];
        break;
      case VAFF_LOAD:
        value = veh->load;
        break;
      case VAFF_SEN:
        value = veh->sensor;
        break;
      case VAFF_PILOT:
        value = veh->pilot;
        break;
      default:
        // Unsupported sort.
        return -1;
    }
    
    snprintf(buf, MIN(sizeof(buf), 50), "%s", veh->short_description == NULL ? "(BUG)" : get_string_after_color_code_removal(veh->short_description, NULL));

    send_to_char(ch, "%3d. [%5ld] %-50s ^n(%4d)\r\n", 
              ++found,
              veh_index[veh->veh_number].vnum,
              buf,
              value);
  }

  return found;
}

int vnum_vehicle(char *searchname, struct char_data * ch)
{
  int nr, found = 0;
  char arg1[MAX_STRING_LENGTH];
  char arg2[MAX_STRING_LENGTH];

  two_arguments(searchname, arg1, arg2);

  for (int vaff_idx = VAFF_HAND; vaff_idx < VAFF_MAX; vaff_idx++) {
    if (!str_cmp(searchname, veh_aff[vaff_idx])) {
      return vnum_vehicles_by_attribute(vaff_idx, ch);
    }
  }

  for (nr = 0; nr <= top_of_veht; nr++)
  {
    bool is_keyword = isname(searchname, get_string_after_color_code_removal(veh_proto[nr].name, NULL));
    bool is_name = isname(searchname, get_string_after_color_code_removal(veh_proto[nr].short_description, NULL));
    if (is_keyword || is_name) {
      snprintf(buf, sizeof(buf), "%3d. [%5ld] %s\r\n", ++found,
              veh_index[nr].vnum,
              veh_proto[nr].short_description == NULL ? "(BUG)" :
              veh_proto[nr].short_description );
      send_to_char(buf, ch);
    }
  }

  return (found);
}

int vnum_host(char *searchname, struct char_data * ch)
{
  int nr, found = 0;
  for (nr = 0; nr <= top_of_matrix; nr++) {
    if (isname(searchname, get_string_after_color_code_removal(matrix[nr].name, NULL))) {
      snprintf(buf, sizeof(buf), "%3d. [%5ld] %s\r\n", ++found,
              matrix[nr].vnum,
              matrix[nr].name);
      send_to_char(buf, ch);
    }
  }

  return (found);
}

int vnum_ic(char *searchname, struct char_data * ch)
{
  int nr, found = 0;
  for (nr = 0; nr <= top_of_ic; nr++) {
    if (isname(searchname, get_string_after_color_code_removal(ic_proto[nr].name, NULL))) {
      snprintf(buf, sizeof(buf), "%3d. [%5ld] %s\r\n", ++found,
              ic_proto[nr].vnum,
              ic_proto[nr].name);
      send_to_char(buf, ch);
    }
  }

  return (found);
}

#define PRINT_HEADER_IF_NEEDED(text) { if (!printed_header) { strlcat(results, text, sizeof(results)); printed_header = TRUE; }}
// Search for the specified text across the whole game.
int vnum_text(char *searchname, struct char_data *ch) {
  char results[100000];
  int count = 0;
  bool printed_header = FALSE;

  strlcpy(results, "", sizeof(results));

  send_to_char("Scanning rooms, mobs, vehicles, objects, quests, hosts, and ICs for that text...\r\n", ch);

  // Rooms.
  printed_header = FALSE;
  for (int idx = 0; idx <= top_of_world; idx++) {
    struct room_data *room = &world[idx];
    const char *context = keyword_appears_in_room(searchname, room, TRUE, TRUE, TRUE);

    if (context) {
      PRINT_HEADER_IF_NEEDED("\r\n^c> Rooms: ^n\r\n");
      snprintf(ENDOF(results), sizeof(results) - strlen(results), "[%7ld]: %s^n:  %s^n\r\n",
               GET_ROOM_VNUM(room),
               GET_ROOM_NAME(room),
               context);
      count++;
    }
  }

  // Mobs.
  printed_header = FALSE;
  for (int idx = 0; idx <= top_of_mobt; idx++) {
    struct char_data *mob = &mob_proto[idx];
    const char *context = keyword_appears_in_char(searchname, mob, TRUE, TRUE, TRUE);

    if (context) {
      PRINT_HEADER_IF_NEEDED("\r\n^c> Mobs: ^n\r\n");
      snprintf(ENDOF(results), sizeof(results) - strlen(results), "[%7ld]: %s^n:  %s^n\r\n",
               GET_MOB_VNUM(mob),
               GET_CHAR_NAME(mob),
               context);
      count++;
    }
  }

  // Vehicles.
  printed_header = FALSE;
  for (int idx = 0; idx <= top_of_veht; idx++) {
    struct veh_data *veh = &veh_proto[idx];
    const char *context = keyword_appears_in_veh(searchname, veh, TRUE, TRUE, TRUE);

    if (context) {
      PRINT_HEADER_IF_NEEDED("\r\n^c> Vehicles: ^n\r\n");
      snprintf(ENDOF(results), sizeof(results) - strlen(results), "[%7ld]: %s^n:  %s^n\r\n",
               GET_VEH_VNUM(veh),
               GET_VEH_NAME(veh),
               context);
      count++;
    }
  }

  // Objects.
  printed_header = FALSE;
  for (int idx = 0; idx <= top_of_objt; idx++) {
    struct obj_data *obj = &obj_proto[idx];
    const char *context = keyword_appears_in_obj(searchname, obj, TRUE, TRUE, TRUE);

    if (context) {
      PRINT_HEADER_IF_NEEDED("\r\n^c> Objects: ^n\r\n");
      snprintf(ENDOF(results), sizeof(results) - strlen(results), "[%7ld]: %s^n:  %s^n\r\n",
               GET_OBJ_VNUM(obj),
               GET_OBJ_NAME(obj),
               context);
      count++;
    }
  }

  // Quests.
  printed_header = FALSE;
  for (int idx = 0; idx <= top_of_questt; idx++) {
    struct quest_data *qst = &quest_table[idx];
    const char *context = keyword_appears_in_quest(searchname, qst);

    if (context) {
      PRINT_HEADER_IF_NEEDED("\r\n^c> Quests: ^n\r\n");
      vnum_t rnum = real_mobile(qst->johnson);

      snprintf(ENDOF(results), sizeof(results) - strlen(results), "[%7ld]: %s^n:  %s^n\r\n",
               qst->vnum,
               rnum > 0 ? GET_CHAR_NAME(&mob_proto[rnum]) : "<bad Johnson>",
               context);
      count++;
    }
  }

  // Hosts.
  printed_header = FALSE;
  for (int idx = 0; idx <= top_of_matrix; idx++) {
    struct host_data *host = &matrix[idx];
    const char *context = keyword_appears_in_host(searchname, host, TRUE, TRUE, TRUE);

    if (context) {
      PRINT_HEADER_IF_NEEDED("\r\n^c> Hosts: ^n\r\n");
      snprintf(ENDOF(results), sizeof(results) - strlen(results), "[%7ld]: %s^n:  %s^n\r\n",
               host->vnum,
               host->name,
               context);
      count++;
    }
  }

  // ICs.
  printed_header = FALSE;
  for (int idx = 0; idx <= top_of_ic; idx++) {
    struct matrix_icon *icon = &ic_proto[idx];
    const char *context = keyword_appears_in_icon(searchname, icon, TRUE, TRUE);

    if (context) {
      PRINT_HEADER_IF_NEEDED("\r\n^c> ICs: ^n\r\n");
      snprintf(ENDOF(results), sizeof(results) - strlen(results), "[%7ld]: %s^n:  %s^n\r\n",
               icon->vnum,
               icon->name,
               context);
      count++;
    }
  }

  send_to_char("Done.\r\n", ch);
  if (*results) {
    page_string(ch->desc, results, FALSE);
  }

  return count;
}

#define SEARCH_STRING(string_name)                                                                           \
  if (quest_table[nr].string_name && *quest_table[nr].string_name && isname(searchname, get_string_after_color_code_removal(quest_table[nr].string_name, NULL))) {          \
    snprintf(ENDOF(found_in), sizeof(found_in) - strlen(found_in), "%s" #string_name, *found_in ? ", " : ""); \
  }

int vnum_quest(char *searchname, struct char_data * ch)
{
  int nr, found = 0;
  for (nr = 0; nr <= top_of_questt; nr++) {
    char found_in[10000] = { 0 };
    rnum_t johnson_rnum = real_mobile(quest_table[nr].johnson);
    snprintf(buf, sizeof(buf), "%3d. [%5ld] %s ", ++found, quest_table[nr].vnum, johnson_rnum >= 0 ? GET_CHAR_NAME(&mob_proto[johnson_rnum]) : "<n/a>");

    SEARCH_STRING(intro);
    SEARCH_STRING(intro_emote);
    SEARCH_STRING(decline);
    SEARCH_STRING(decline_emote);
    SEARCH_STRING(quit);
    SEARCH_STRING(quit_emote);
    SEARCH_STRING(finish);
    SEARCH_STRING(finish_emote);
    SEARCH_STRING(info);
    SEARCH_STRING(done);

    if (quest_table[nr].info_emotes) {
      for (auto *it : *(quest_table[nr].info_emotes)) {
        if (it && *it && isname(searchname, get_string_after_color_code_removal(it, NULL))) {
          snprintf(ENDOF(found_in), sizeof(found_in) - strlen(found_in), "%sinfo emotes", *found_in ? ", " : "");
          break;
        }
      }
    }

    if (*found_in) {
      send_to_char(ch, "%s\r\n", buf);
    }
  }

  return (found);
}

int vnum_mobile(char *searchname, struct char_data * ch)
{
  int nr, found = 0;
  char arg1[MAX_STRING_LENGTH];
  char arg2[MAX_STRING_LENGTH];
  two_arguments(searchname, arg1, arg2);

  if (!strcmp(searchname,"karmalist"))
    return vnum_mobile_karma(searchname,ch);
  if (!strcmp(searchname,"bonuskarmalist"))
    return vnum_mobile_bonuskarma(searchname,ch);
  if (!strcmp(searchname,"nuyenlist"))
    return vnum_mobile_nuyen(searchname,ch);
  if (!strcmp(searchname,"valuedeathkarma"))
    return vnum_mobile_valuedeathkarma(searchname,ch);
  if (!strcmp(searchname,"valuedeathnuyen"))
    return vnum_mobile_valuedeathnuyen(searchname,ch);
  if (!strcmp(searchname,"valuedeath"))
    return vnum_mobile_valuedeath(searchname,ch);
  if (!strcmp(arg1,"affflag"))
    return vnum_mobile_affflag(atoi(arg2),ch);
  for (nr = 0; nr <= top_of_mobt; nr++)
  {
    bool is_keyword = isname(searchname, get_string_after_color_code_removal(mob_proto[nr].player.physical_text.keywords, NULL));
    bool is_name = isname(searchname, get_string_after_color_code_removal(mob_proto[nr].player.physical_text.name, NULL));
    if (is_keyword || is_name) {
      snprintf(buf, sizeof(buf), "%3d. [%5ld] %s\r\n", ++found,
              MOB_VNUM_RNUM(nr),
              mob_proto[nr].player.physical_text.name == NULL ? "(BUG)" :
              mob_proto[nr].player.physical_text.name);
      send_to_char(buf, ch);
    }
  }

  return (found);
}

int vnum_object_weapons(char *searchname, struct char_data * ch)
{
  char buf[MAX_STRING_LENGTH*8];
  extern const char *wound_arr[];
  int nr, found = 0;
  int power, severity, strength;
  buf[0] = '\0';

  for( severity = DEADLY; severity >= LIGHT; severity -- )
    for( power = 21; power >= 0; power-- )
      for( strength = WEAPON_MAXIMUM_STRENGTH_BONUS; strength >= 0; strength-- )
      {
        for (nr = 0; nr <= top_of_objt; nr++) {
          if (GET_OBJ_TYPE(&obj_proto[nr]) != ITEM_WEAPON)
            continue;
          if (!IS_GUN(GET_WEAPON_ATTACK_TYPE(&obj_proto[nr])))
            continue;
          if (GET_WEAPON_POWER(&obj_proto[nr]) < power && power != 0)
            continue;
          if (GET_WEAPON_DAMAGE_CODE(&obj_proto[nr]) < severity && severity != 1)
            continue;
          if (GET_WEAPON_STR_BONUS(&obj_proto[nr]) < strength && strength != 0)
            continue;
          if (GET_WEAPON_POWER(&obj_proto[nr]) > power && power != 21)
            continue;
          if (GET_WEAPON_DAMAGE_CODE(&obj_proto[nr]) > severity && severity != DEADLY)
            continue;
          if (GET_WEAPON_STR_BONUS(&obj_proto[nr]) > strength && strength != 5)
            continue;
          if (IS_OBJ_STAT(&obj_proto[nr], ITEM_EXTRA_STAFF_ONLY))
            continue;
          if (!vnum_from_editing_restricted_zone(OBJ_VNUM_RNUM(nr)))
            continue;

          ++found;
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%5ld -%2d] ^c%2d%s ^y+%d^n %s (^W%s^n, ^c%d^n rounds, modes:^c%s%s%s%s^n%s)%s\r\n",
                  OBJ_VNUM_RNUM(nr),
                  ObjList.CountObj(nr),
                  GET_OBJ_VAL(&obj_proto[nr], 0),
                  wound_arr[GET_OBJ_VAL(&obj_proto[nr], 1)],
                  GET_OBJ_VAL(&obj_proto[nr], 2),
                  obj_proto[nr].text.name,
                  weapon_types[GET_OBJ_VAL(&obj_proto[nr], 3)],
                  GET_OBJ_VAL(&obj_proto[nr], 5),
                  WEAPON_CAN_USE_FIREMODE(&obj_proto[nr], MODE_SS) ? " SS" : "",
                  WEAPON_CAN_USE_FIREMODE(&obj_proto[nr], MODE_SA) ? " SA" : "",
                  WEAPON_CAN_USE_FIREMODE(&obj_proto[nr], MODE_BF) ? " BF" : "",
                  WEAPON_CAN_USE_FIREMODE(&obj_proto[nr], MODE_FA) ? " FA" : "",
                  CAN_WEAR(&obj_proto[nr], ITEM_WEAR_WIELD) ? ", ^yWieldable^n" : "",
                  obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
        }
      }
  page_string(ch->desc, buf, 1);
  return (found);
}

int vnum_object_weapons_broken(char *searchname, struct char_data * ch)
{
  char buf[MAX_STRING_LENGTH*8];
  extern const char *wound_arr[];
  int nr, found = 0;
  buf[0] = '\0';

  for (nr = 0; nr <= top_of_objt; nr++) {
    if (GET_OBJ_TYPE(&obj_proto[nr]) != ITEM_WEAPON)
      continue;

    bool bad_attack_type = GET_WEAPON_ATTACK_TYPE(&obj_proto[nr]) < 0 || GET_WEAPON_ATTACK_TYPE(&obj_proto[nr]) >= MAX_WEAP;

    if (bad_attack_type) {
      ++found;
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%6ld :%3d] %s '%s^n'  %s\r\n",
              OBJ_VNUM_RNUM(nr),
              ObjList.CountObj(nr),
              vnum_from_non_approved_zone(GET_OBJ_VNUM(&obj_proto[nr])) ? " " : "*",
              GET_OBJ_NAME(&obj_proto[nr]),
              bad_attack_type ? "(bad attack type)" : "");
    }
  }
  
  page_string(ch->desc, buf, 1);
  return (found);
}

int vnum_object_weapons_fa_pro(char *searchname, struct char_data * ch)
{
  char buf[MAX_STRING_LENGTH*8];
  extern const char *wound_arr[];
  int nr, found = 0;
  buf[0] = '\0';

  for(int power = 21; power >= 0; power-- ) {
    for (nr = 0; nr <= top_of_objt; nr++) {
      if (GET_OBJ_TYPE(&obj_proto[nr]) != ITEM_WEAPON)
        continue;
      if (!IS_GUN(GET_WEAPON_ATTACK_TYPE(&obj_proto[nr])))
        continue;
      if (GET_WEAPON_POWER(&obj_proto[nr]) < power && power != 0)
        continue;
      if (GET_WEAPON_POWER(&obj_proto[nr]) > power && power != 21)
        continue;
      if (IS_OBJ_STAT(&obj_proto[nr], ITEM_EXTRA_STAFF_ONLY))
        continue;
      if (!vnum_from_editing_restricted_zone(OBJ_VNUM_RNUM(nr)))
        continue;
      if (!WEAPON_CAN_USE_FIREMODE(&obj_proto[nr], MODE_FA))
        continue;

      ++found;
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%6ld :%3d] ^c%2d%s ^yIRC:%d^n %s (^W%s^n, ^c%d^n rounds, modes:^c%s%s%s%s^n%s)%s\r\n",
              OBJ_VNUM_RNUM(nr),
              ObjList.CountObj(nr),
              GET_WEAPON_POWER(&obj_proto[nr]),
              wound_arr[GET_WEAPON_DAMAGE_CODE(&obj_proto[nr])],
              GET_WEAPON_INTEGRAL_RECOIL_COMP(&obj_proto[nr]),
              obj_proto[nr].text.name,
              weapon_types[GET_WEAPON_ATTACK_TYPE(&obj_proto[nr])],
              GET_WEAPON_MAX_AMMO(&obj_proto[nr]),
              WEAPON_CAN_USE_FIREMODE(&obj_proto[nr], MODE_SS) ? " SS" : "",
              WEAPON_CAN_USE_FIREMODE(&obj_proto[nr], MODE_SA) ? " SA" : "",
              WEAPON_CAN_USE_FIREMODE(&obj_proto[nr], MODE_BF) ? " BF" : "",
              WEAPON_CAN_USE_FIREMODE(&obj_proto[nr], MODE_FA) ? " FA" : "",
              CAN_WEAR(&obj_proto[nr], ITEM_WEAR_WIELD) ? ", ^yWieldable^n" : "",
              obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
    }
  }
  page_string(ch->desc, buf, 1);
  return (found);
}

int vnum_object_weapons_by_type(char *searchname, struct char_data * ch)
{
  char buf[MAX_STRING_LENGTH*8];
  extern const char *wound_arr[];
  int found = 0;
  buf[0] = '\0';

  for (int weapon_type = 0; weapon_type < MAX_WEAP; weapon_type++) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n[^c%s^n]:\r\n", weapon_types[weapon_type]);

    int found_of_type = 0;

    for (int power = 21; power >= 0; power-- ) {
      for (int damage_code = DEADLY; damage_code >= LIGHT; damage_code--) {
        for (rnum_t nr = 0; nr <= top_of_objt; nr++) {
          struct obj_data *weapon = &obj_proto[nr];

          if (GET_OBJ_TYPE(weapon) != ITEM_WEAPON)
            continue;
          if (GET_WEAPON_ATTACK_TYPE(weapon) != weapon_type || GET_WEAPON_DAMAGE_CODE(weapon) != damage_code)
            continue;
          if (GET_WEAPON_POWER(weapon) < power && power != 0)
            continue;
          if (GET_WEAPON_POWER(weapon) > power && power != 21)
            continue;
          if (IS_OBJ_STAT(weapon, ITEM_EXTRA_STAFF_ONLY))
            continue;
          if (!vnum_from_editing_restricted_zone(OBJ_VNUM_RNUM(nr)))
            continue;

          found++;
          found_of_type++;

          if (IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%6ld :%3d] ^c%2d%s ^yIRC:%d^n %s (^W%s^n, ^c%d^n rounds, modes:^c%s%s%s%s^n%s)%s\r\n",
                    OBJ_VNUM_RNUM(nr),
                    ObjList.CountObj(nr),
                    GET_WEAPON_POWER(weapon),
                    wound_arr[GET_WEAPON_DAMAGE_CODE(weapon)],
                    GET_WEAPON_INTEGRAL_RECOIL_COMP(weapon),
                    obj_proto[nr].text.name,
                    weapon_types[GET_WEAPON_ATTACK_TYPE(weapon)],
                    GET_WEAPON_MAX_AMMO(weapon),
                    WEAPON_CAN_USE_FIREMODE(weapon, MODE_SS) ? " SS" : "",
                    WEAPON_CAN_USE_FIREMODE(weapon, MODE_SA) ? " SA" : "",
                    WEAPON_CAN_USE_FIREMODE(weapon, MODE_BF) ? " BF" : "",
                    WEAPON_CAN_USE_FIREMODE(weapon, MODE_FA) ? " FA" : "",
                    CAN_WEAR(weapon, ITEM_WEAR_WIELD) ? ", ^yWieldable^n" : "",
                    obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
          } else {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%6ld :%3d] ^cSTR+%d %s^n ^%sRCH:%2d^n %s (^W%s, focus rating %d^n%s)%s\r\n",
                    OBJ_VNUM_RNUM(nr),
                    ObjList.CountObj(nr),
                    GET_WEAPON_STR_BONUS(weapon),
                    wound_arr[GET_WEAPON_DAMAGE_CODE(weapon)],
                    GET_WEAPON_REACH(weapon) > 2 ? "r" : (GET_WEAPON_REACH(weapon) > 1 ? "y" : "n"),
                    GET_WEAPON_REACH(weapon),
                    obj_proto[nr].text.name,
                    weapon_types[GET_WEAPON_ATTACK_TYPE(weapon)],
                    GET_WEAPON_FOCUS_RATING(weapon),
                    CAN_WEAR(weapon, ITEM_WEAR_WIELD) ? ", ^yWieldable^n" : "",
                    obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
          }
        }
      }
    }
    if (!found_of_type) {
      strlcat(buf, "  - nothing\r\n", sizeof(buf));
    }
  }
  page_string(ch->desc, buf, 1);
  return (found);
}

int vnum_object_armors(char *searchname, struct char_data * ch)
{
  char buf[MAX_STRING_LENGTH*8];
  char xbuf[MAX_STRING_LENGTH];
  int nr, found = 0;

  buf[0] = 0;

  // List everything above 20 combined ballistic and impact.
  for (nr = 0; nr <= top_of_objt; nr++) {
    if (GET_OBJ_TYPE(&obj_proto[nr]) != ITEM_WORN)
      continue;

    float bal = GET_WORN_BALLISTIC(&obj_proto[nr]);
    float imp = GET_WORN_IMPACT(&obj_proto[nr]);
    if (GET_WORN_MATCHED_SET(&obj_proto[nr])) {
      bal /= 100;
      imp /= 100;
    }

    if (bal + imp <= 20)
      continue;

    sprint_obj_mods( &obj_proto[nr], xbuf, sizeof(xbuf));

    ++found;
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%5ld -%2d] ^c%0.2fb %0.2fi^n %s^n^y%s^n%s\r\n",
            OBJ_VNUM_RNUM(nr),
            ObjList.CountObj(nr),
            bal,
            imp,
            obj_proto[nr].text.name,
            xbuf,
            obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
  }

  // List everything with 20 or less combined ballistic and impact, descending.
  for (int total = 20; total >= 0; total--) {
    for (nr = 0; nr <= top_of_objt; nr++) {
      if (GET_OBJ_TYPE(&obj_proto[nr]) != ITEM_WORN)
        continue;

      float bal = GET_WORN_BALLISTIC(&obj_proto[nr]);
      float imp = GET_WORN_IMPACT(&obj_proto[nr]);
      if (GET_WORN_MATCHED_SET(&obj_proto[nr])) {
        bal /= 100;
        imp /= 100;
      }

      if (bal + imp != total)
        continue;

      sprint_obj_mods( &obj_proto[nr], xbuf, sizeof(xbuf) );

      ++found;
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%5ld -%2d] ^c%0.2fb %0.2fi^n %s^n^y%s^n%s\r\n",
              OBJ_VNUM_RNUM(nr),
              ObjList.CountObj(nr),
              bal,
              imp,
              obj_proto[nr].text.name,
              xbuf,
              obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
    }
  }

  page_string(ch->desc, buf, 1);

  return (found);
}

int vnum_object_magazines(char *searchname, struct char_data * ch)
{
  int nr, found = 0;
  int capacity, type;
  *buf = '0';

  for( type = 13; type <= 38; type++ )
    for( capacity = 101; capacity >= 0; capacity-- )
    {
      for (nr = 0; nr <= top_of_objt; nr++) {
        if (GET_OBJ_TYPE(&obj_proto[nr]) != ITEM_GUN_MAGAZINE)
          continue;
        if (GET_OBJ_VAL(&obj_proto[nr],0) < capacity && capacity != 0)
          continue;
        if (GET_OBJ_VAL(&obj_proto[nr],1) < type && type != 13)
          continue;
        if (GET_OBJ_VAL(&obj_proto[nr],0) > capacity && capacity != 101)
          continue;
        if (GET_OBJ_VAL(&obj_proto[nr],1) > type && type != 38)
          continue;
        if (IS_OBJ_STAT(&obj_proto[nr], ITEM_EXTRA_STAFF_ONLY))
          continue;
        if (!vnum_from_editing_restricted_zone(OBJ_VNUM_RNUM(nr)))
          continue;

        ++found;
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%5ld -%2d wt %f] %2d %3d %s^n%s\r\n",
                OBJ_VNUM_RNUM(nr),
                ObjList.CountObj(nr),
                GET_OBJ_WEIGHT(&obj_proto[nr]),
                GET_OBJ_VAL(&obj_proto[nr], 1),
                GET_OBJ_VAL(&obj_proto[nr], 0),
                obj_proto[nr].text.name,
                obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
      }
    }
  page_string(ch->desc, buf, 1);
  return (found);
}

int vnum_object_foci(char *searchname, struct char_data * ch)
{
  int found = 0;

  strlcpy(buf, "", sizeof(buf));

  for (int type_idx = 0; type_idx < NUM_FOCUS_TYPES; type_idx++) {
    for (int power = 10; power >= 0; power--) {
      for (int nr = 0; nr <= top_of_objt; nr++) {
        if (GET_OBJ_TYPE(&obj_proto[nr]) != ITEM_FOCUS)
          continue;

        if (GET_FOCUS_TYPE(&obj_proto[nr]) != type_idx)
          continue;

        // Skip anything that doesn't match our sought power. At max (10), we accept anything at or above.
        if ((power == 10 ? GET_FOCUS_FORCE(&obj_proto[nr]) < power : GET_FOCUS_FORCE(&obj_proto[nr]) != power))
          continue;

        if (!vnum_from_editing_restricted_zone(OBJ_VNUM_RNUM(nr)))
          continue;

        int count = ObjList.CountObj(nr);

        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%3d. [%5ld -%s%2d^n] %s +%2d '%s^n'%s", ++found,
                OBJ_VNUM_RNUM(nr),
                count != 0 ? "^c" : "",
                count,
                foci_type[GET_OBJ_VAL(&obj_proto[nr], 0)],
                GET_OBJ_VAL(&obj_proto[nr], VALUE_FOCUS_RATING),
                obj_proto[nr].text.name,
                obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
        
        char wear_bit_buf[10000] = { '\0' };
        obj_proto[nr].obj_flags.wear_flags.PrintBits(wear_bit_buf, sizeof(wear_bit_buf), wear_bits, NUM_WEARS);
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " [%s]\r\n", wear_bit_buf);
      }
    }
  }

  page_string(ch->desc, buf, 1);

  return (found);
}

int vnum_object_type(int type, struct char_data * ch)
{
  char buf[MAX_STRING_LENGTH * 8];
  int nr, found = 0;

  buf[0] = 0;

  for (nr = 0; nr <= top_of_objt; nr++)
  {
    if (GET_OBJ_TYPE(&obj_proto[nr]) == type) {
      ++found;
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%5ld -%2d] %s %s^n%s\r\n",
              OBJ_VNUM_RNUM(nr),
              ObjList.CountObj(nr),
              vnum_from_non_approved_zone(OBJ_VNUM_RNUM(nr)) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
              obj_proto[nr].text.name,
              obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
    }
  }
  page_string(ch->desc, buf, 1);
  return (found);
}

int vnum_object_affectloc(int type, struct char_data * ch)
{
  char xbuf[MAX_STRING_LENGTH];
  int nr, found = 0, mod;

  for( mod = 11; mod >= -11; mod -- )
    for (nr = 0; nr <= top_of_objt; nr++)
    {
      if (IS_OBJ_STAT(&obj_proto[nr], ITEM_EXTRA_STAFF_ONLY))
        continue;
      if (!vnum_from_editing_restricted_zone(OBJ_VNUM_RNUM(nr)))
        continue;

      for (int i = 0; i < MAX_OBJ_AFFECT; i++)
        if (obj_proto[nr].affected[i].location == type ) {
          if (obj_proto[nr].affected[i].modifier < mod && mod != -11)
            continue;
          if (obj_proto[nr].affected[i].modifier > mod && mod != 11)
            continue;

          sprint_obj_mods( &obj_proto[nr], xbuf, sizeof(xbuf));

          ++found;
          snprintf(buf, sizeof(buf), "[%5ld -%2d] %s^n%s%s\r\n",
                  OBJ_VNUM_RNUM(nr),
                  ObjList.CountObj(nr),
                  obj_proto[nr].text.name,
                  xbuf,
                  obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
          send_to_char(buf, ch);
          break;
        }
    }
  return (found);
}

int vnum_object_affects(struct char_data *ch) {
  char buf[MAX_STRING_LENGTH * 8];
  char xbuf[MAX_STRING_LENGTH];
  int nr, found = 0;

  buf[0] = 0;

  for (nr = 0; nr <= top_of_objt; nr++) {
    if (IS_OBJ_STAT(&obj_proto[nr], ITEM_EXTRA_STAFF_ONLY))
      continue;
    if (!vnum_from_editing_restricted_zone(OBJ_VNUM_RNUM(nr)))
      continue;

    // If it can't be used in the first place, skip it.
    if (GET_OBJ_TYPE(&obj_proto[nr]) != ITEM_GUN_ACCESSORY
        && GET_OBJ_TYPE(&obj_proto[nr]) != ITEM_CYBERWARE
        && GET_OBJ_TYPE(&obj_proto[nr]) != ITEM_BIOWARE
        && !str_cmp(obj_proto[nr].obj_flags.wear_flags.ToString(), "1"))
      continue;

    for (int i = 0; i < MAX_OBJ_AFFECT; i++) {
      if (obj_proto[nr].affected[i].modifier != 0 ) {
        sprint_obj_mods( &obj_proto[nr], xbuf, sizeof(xbuf));

        ++found;
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%5ld -%2d] %s^n%s%s\r\n",
                OBJ_VNUM_RNUM(nr),
                ObjList.CountObj(nr),
                obj_proto[nr].text.name,
                xbuf,
                obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
        break;
      }
    }
  }
  page_string(ch->desc, buf, 1);
  return (found);
}

int vnum_object_affflag(int type, struct char_data * ch)
{
  int nr, found = 0;

  send_to_char(ch, "The following objects have the %s affect flag set:\r\n", affected_bits[type]);

  for (nr = 0; nr <= top_of_objt; nr++)
    if (obj_proto[nr].obj_flags.bitvector.IsSet(type))
    {
      snprintf(buf, sizeof(buf), "[%5ld -%2d] %s^n%s\r\n",
              OBJ_VNUM_RNUM(nr),
              ObjList.CountObj(nr),
              obj_proto[nr].text.name,
              obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
      send_to_char(buf, ch);
      found++;
    }
  return (found);
}

int vnum_object_poison(struct char_data * ch)
{
  char buf[MAX_STRING_LENGTH * 8];
  int nr, found = 0;

  buf[0] = 0;

  for (nr = 0; nr <= top_of_objt; nr++)
  {
    if (GET_OBJ_TYPE(&obj_proto[nr]) == ITEM_FOUNTAIN && GET_FOUNTAIN_POISON_RATING(&obj_proto[nr]) > 0) {
      ++found;
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%5ld -%2d] %s %s^n  poison rating: %d\r\n",
              OBJ_VNUM_RNUM(nr),
              ObjList.CountObj(nr),
              vnum_from_non_approved_zone(OBJ_VNUM_RNUM(nr)) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
              obj_proto[nr].text.name,
              GET_FOUNTAIN_POISON_RATING(&obj_proto[nr]));
    }
    else if (GET_OBJ_TYPE(&obj_proto[nr]) == ITEM_DRINKCON && GET_DRINKCON_POISON_RATING(&obj_proto[nr]) > 0) {
      ++found;
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%5ld -%2d] %s %s^n  poison rating: %d\r\n",
              OBJ_VNUM_RNUM(nr),
              ObjList.CountObj(nr),
              vnum_from_non_approved_zone(OBJ_VNUM_RNUM(nr)) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
              obj_proto[nr].text.name,
              GET_DRINKCON_POISON_RATING(&obj_proto[nr]));
    }
  }
  page_string(ch->desc, buf, 1);
  return (found);
}

int vnum_object_weaponfocus(char *searchname, struct char_data * ch)
{
  char buf[MAX_STRING_LENGTH*8];
  extern const char *wound_arr[];
  int nr, found = 0;
  int severity, strength;
  buf[0] = '\0';

  for (int rating = max_weapon_focus_rating; rating >= 0; rating--) {
    for( severity = DEADLY; severity >= LIGHT; severity -- ) {
      for( strength = WEAPON_MAXIMUM_STRENGTH_BONUS; strength >= 0; strength-- ) {
        for (nr = 0; nr <= top_of_objt; nr++) {
          if (GET_OBJ_TYPE(&obj_proto[nr]) != ITEM_WEAPON)
            continue;
          if (WEAPON_IS_GUN(&obj_proto[nr]) || GET_WEAPON_FOCUS_RATING(&obj_proto[nr]) <= 0)
            continue;
          if (GET_WEAPON_FOCUS_RATING(&obj_proto[nr]) != rating)
            continue;
          if (GET_WEAPON_DAMAGE_CODE(&obj_proto[nr]) < severity && severity != 1)
            continue;
          if (GET_WEAPON_STR_BONUS(&obj_proto[nr]) < strength && strength != 0)
            continue;
          if (GET_WEAPON_DAMAGE_CODE(&obj_proto[nr]) > severity && severity != DEADLY)
            continue;
          if (GET_WEAPON_STR_BONUS(&obj_proto[nr]) > strength && strength != 5)
            continue;
          if (IS_OBJ_STAT(&obj_proto[nr], ITEM_EXTRA_STAFF_ONLY))
            continue;
          if (!vnum_from_editing_restricted_zone(OBJ_VNUM_RNUM(nr)))
            continue;

          ++found;
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%5ld -%2d] (STR^c%+d^n)^c%s^n %s (^W%s^n, ^c%d^n reach, rating ^c%d^n)%s\r\n",
                  OBJ_VNUM_RNUM(nr),
                  ObjList.CountObj(nr),
                  GET_WEAPON_STR_BONUS(&obj_proto[nr]),
                  wound_arr[GET_WEAPON_DAMAGE_CODE(&obj_proto[nr])],
                  obj_proto[nr].text.name,
                  weapon_types[GET_WEAPON_ATTACK_TYPE(&obj_proto[nr])],
                  GET_WEAPON_REACH(&obj_proto[nr]),
                  GET_WEAPON_FOCUS_RATING(&obj_proto[nr]),
                  obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
        }
      }
    }
  }
  page_string(ch->desc, buf, 1);
  return (found);
}


int vnum_object_extra_bit(char *searchname, struct char_data * ch)
{
  char buf[MAX_STRING_LENGTH*8] = {'\0'};
  int found = 0, type = 0;

  for (; type < MAX_ITEM_EXTRA; type++) {
    if (is_abbrev(searchname, extra_bits[type]))
      break;
  }

  if (type >= MAX_ITEM_EXTRA) {
    send_to_char("That's not a type. Please specify a type from constants.cpp's extra_bits[] (ex: GLOW, HUM, !RENT...)\r\n", ch);
    return 0;
  }

  send_to_char(ch, "The following items have the extra flag %s set:\r\n", extra_bits[type]);

  for (int nr = 0; nr <= top_of_objt; nr++) {
    if (IS_OBJ_STAT(&obj_proto[nr], type)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%5ld -%2d] %s^n%s\r\n",
              OBJ_VNUM_RNUM(nr),
              ObjList.CountObj(nr),
              obj_proto[nr].text.name,
              obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
      found++;
    }
  }
        
  page_string(ch->desc, buf, 1);
  return (found);
}


int vnum_object(char *searchname, struct char_data * ch)
{
  int nr, found = 0;
  char arg1[MAX_STRING_LENGTH];
  char arg2[MAX_STRING_LENGTH];

  two_arguments(searchname, arg1, arg2);

  if (!strcmp(searchname,"weaponslist"))
    return vnum_object_weapons(searchname,ch);
  if (!strcmp(searchname,"brokenweaponslist"))
    return vnum_object_weapons_broken(searchname,ch);
  if (!strcmp(searchname,"faweaponslist"))
    return vnum_object_weapons_fa_pro(searchname,ch);
  if (!strcmp(searchname,"weaponsbytype"))
    return vnum_object_weapons_by_type(searchname,ch);
  if (!strcmp(searchname,"armorslist"))
    return vnum_object_armors(searchname,ch);
  if (!strcmp(searchname,"magazineslist"))
    return vnum_object_magazines(searchname,ch);
  if (!strcmp(searchname,"focilist"))
    return vnum_object_foci(searchname,ch);
  if (!strcmp(searchname, "weaponfocus"))
    return vnum_object_weaponfocus(searchname, ch);
  if (!strcmp(arg1,"objtype"))
    return vnum_object_type(atoi(arg2),ch);
  if (!strcmp(arg1,"affectloc"))
    return vnum_object_affectloc(atoi(arg2),ch);
  if (!strcmp(arg1, "affects"))
    return vnum_object_affects(ch);
  if (!strcmp(arg1,"affflag"))
    return vnum_object_affflag(atoi(arg2),ch);
  if (!strcmp(arg1, "poison"))
    return vnum_object_poison(ch);
  if (!strcmp(arg1, "extra"))
    return vnum_object_extra_bit(arg2, ch);

  // Make it easier for people to find specific types of things.
  for (int index = ITEM_LIGHT; index < NUM_ITEMS; index++) {
    if (!str_cmp(searchname, item_types[index]))
      return vnum_object_type(index, ch);
  }

  for (nr = 0; nr <= top_of_objt; nr++)
  {
    bool is_keyword = isname(searchname, get_string_after_color_code_removal(obj_proto[nr].text.keywords, NULL));
    bool is_name = isname(searchname, get_string_after_color_code_removal(obj_proto[nr].text.name, NULL));
    if (is_keyword || is_name) {
      snprintf(buf, sizeof(buf), "%3d. [%5ld -%2d] %s %s%s\r\n", ++found,
              OBJ_VNUM_RNUM(nr),
              ObjList.CountObj(nr),
              vnum_from_non_approved_zone(OBJ_VNUM_RNUM(nr)) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
              obj_proto[nr].text.name,
              obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
      send_to_char(buf, ch);
    }
  }
  return (found);
}

int vnum_room_samename(struct char_data *ch) {
  int found = 0;
  std::unordered_map<std::string, std::vector<vnum_t>> seen_names = {};

  send_to_char(ch, "Looking for edit-locked rooms that share the same name...\r\n");

  for (rnum_t nr = 0; nr <= top_of_world; nr++) {
    struct room_data *room = &world[nr];

    if (!vnum_from_editing_restricted_zone(GET_ROOM_VNUM(room)))
      continue;

    std::string string_name = GET_ROOM_NAME(room);

    // It wasn't there, add it.
    auto iterator = seen_names.find(string_name);
    if (iterator == seen_names.end()) {
      seen_names.emplace(string_name, std::vector<vnum_t> {GET_ROOM_VNUM(room)});
    } else {
      iterator->second.emplace_back(GET_ROOM_VNUM(room));
    }
  }

  for (auto iterator : seen_names) {
    if (iterator.second.size() > 1) {
      send_to_char(ch, "%3d. '^c%s^n' is shared among ", ++found, iterator.first.c_str());

      bool printed_yet = FALSE;
      for (auto vnum_it : iterator.second) {
        send_to_char(ch, "%s^c%ld^n", printed_yet ? ", " : "", vnum_it);
        printed_yet = TRUE;
      }
      send_to_char("\r\n", ch);
    }
  }

  return found;
}

int vnum_room_flag(char *flagname, struct char_data *ch) {
  int found = 0;

  // Identify the flag.
  int flag = search_block(flagname, room_bits, FALSE);

  if (flag >= ROOM_MAX || flag < 0) {
    send_to_char(ch, "'%s' is not a valid room flag. Choices are:\r\n", flagname);
    for (flag = 0; flag < ROOM_MAX; flag++) {
      send_to_char(ch, "%s%s%s", flag == 0 ? "" : ", ", room_bits[flag], flag == ROOM_MAX-1 ? "\r\n" : "");
    }
    return 0;
  }

  for (rnum_t nr = 0; nr <= top_of_world; nr++) {
    struct room_data *room = &world[nr];

    if (ROOM_FLAGGED(room, flag)) {
      send_to_char(ch, "%3d. [%6ld] %s^n%s\r\n",
                       ++found,
                       GET_ROOM_VNUM(room),
                       GET_ROOM_NAME(room),
                       ROOM_FLAGGED(room, ROOM_ENCOURAGE_CONGREGATION) ? " ^c(social)^n" : "");
    }
  }
  return (found);
}

int vnum_room(char *searchname, struct char_data *ch) {
  int nr, found = 0;
  char arg1[MAX_STRING_LENGTH];
  char arg2[MAX_STRING_LENGTH];

  two_arguments(searchname, arg1, arg2);

  if (!strcmp(arg1, "samename"))
    return vnum_room_samename(ch);
  if (!strcmp(arg1, "flag"))
    return vnum_room_flag(arg2, ch);

  for (nr = 0; nr <= top_of_world; nr++)
  {
    struct room_data *room = &world[nr];

    bool is_name = isname(searchname, get_string_after_color_code_removal(GET_ROOM_NAME(room), NULL));

    if (is_name) {
      send_to_char(ch, "%3d. [%6ld] %s^n%s\r\n",
                       ++found,
                       GET_ROOM_VNUM(room),
                       GET_ROOM_NAME(room),
                       ROOM_FLAGGED(room, ROOM_ENCOURAGE_CONGREGATION) ? " ^c(social)^n" : "");
    }
  }
  return (found);
}


struct veh_data *read_vehicle(int nr, int type)
{
  int i;
  struct veh_data *veh;

  if (type == VIRTUAL)
  {
    if ((i = real_vehicle(nr)) < 0) {
      snprintf(buf, sizeof(buf), "Vehicle (V) %d does not exist in database.", nr);
      return (0);
    }
  } else
    i = nr;

  veh = Mem->GetVehicle();
  *veh = veh_proto[i];
  veh->next = veh_list;
  veh_list = veh;
  veh_index[i].number++;
  return veh;
}


/* create a new mobile from a prototype */
struct char_data *read_mobile(int nr, int type)
{
  int i;
  struct char_data *mob;

  if (type == VIRTUAL)
  {
    if ((i = real_mobile(nr)) < 0) {
      snprintf(buf, sizeof(buf), "Mobile (V) %d does not exist in database.", nr);
      return (0);
    }
  } else
    i = nr;

  mob = Mem->GetCh();
  *mob = mob_proto[i];
  mob->load_origin = PC_LOAD_REASON_READ_MOBILE;
  mob->load_time = time(0);
  add_ch_to_character_list(mob, "read_mobile");

  mob->points.physical = mob->points.max_physical;
  mob->points.mental = mob->points.max_mental;

  mob->player.time.birth = time(0);
  mob->player.time.played = 0;
  mob->player.time.logon = time(0);
  mob->player.tradition = mob->player.aspect = 0;
  mob->char_specials.saved.left_handed = (!number(0, 9) ? 1 : 0);

  mob_index[i].number++;

  // See utils.cpp for this.
  set_new_mobile_unique_id(mob);

  set_natural_vision_for_race(mob);

  // Copy off their cyberware from prototype.
  mob->cyberware = NULL;
  for (struct obj_data *obj = mob_proto[i].cyberware; obj; obj = obj->next_content) {
    obj_to_cyberware(read_object(GET_OBJ_VNUM(obj), VIRTUAL, OBJ_LOAD_REASON_MOB_DEFAULT_GEAR), mob);
  }

  // Same for bioware.
  mob->bioware = NULL;
  for (struct obj_data *obj = mob_proto[i].bioware; obj; obj = obj->next_content) {
    obj_to_bioware(read_object(GET_OBJ_VNUM(obj), VIRTUAL, OBJ_LOAD_REASON_MOB_DEFAULT_GEAR), mob);
  }

  // And then equipment.
  struct obj_data *eq;
  for (int wearloc = 0; wearloc < NUM_WEARS; wearloc++) {
    GET_EQ(mob, wearloc) = NULL;
    if ((eq = GET_EQ(&mob_proto[i], wearloc))) {
      equip_char(mob, read_object(GET_OBJ_VNUM(eq), VIRTUAL, OBJ_LOAD_REASON_MOB_DEFAULT_GEAR), wearloc);
    }
  }

  affect_total(mob);

  GET_POS(mob) = (GET_DEFAULT_POS(mob) > 0 ? GET_DEFAULT_POS(mob) : POS_STANDING);

  if (GET_POS(mob) == POS_STUNNED) {
    GET_MENTAL(mob) = -10 * 100;
  } else if (GET_POS(mob) == POS_MORTALLYW) {
    GET_PHYSICAL(mob) = 0;
    GET_MENTAL(mob) = 0;
  }

  if ((GET_MOB_SPEC(mob) || GET_MOB_SPEC2(mob)) && !MOB_FLAGGED(mob, MOB_SPEC))
    MOB_FLAGS(mob).SetBit(MOB_SPEC);

  return mob;
}

/* create a new mobile from a prototype */
struct matrix_icon *read_ic(int nr, int type)
{
  int i;
  struct matrix_icon *ic;

  if (type == VIRTUAL)
  {
    if ((i = real_ic(nr)) < 0) {
      snprintf(buf, sizeof(buf), "IC (V) %d does not exist in database.", nr);
      return (0);
    }
  } else
    i = nr;

  ic = Mem->GetIcon();
  *ic = ic_proto[i];
  ic_index[i].number++;
  ic->condition = 10;
  ic->idnum = number(0, INT_MAX);
  ic->next = icon_list;
  icon_list = ic;

  return ic;
}
/* create an object, and add it to the object list */
struct obj_data *create_obj(void)
{
  struct obj_data *obj;

  obj = Mem->GetObject();
  ObjList.ADD(obj);

  return obj;
}

/* create a new object from a prototype */
struct obj_data *read_object(int nr, int type, int load_origin, int pc_load_origin, idnum_t pc_load_idnum)
{
  struct obj_data *obj;
  int i;

  if (nr < 0)
  {
    log("SYSERR: trying to create obj with negative num!");
    return NULL;
  }
  if (type == VIRTUAL)
  {
    if ((i = real_object(nr)) < 0) {
      snprintf(buf, sizeof(buf), "Object (V) %d does not exist in database.", nr);
      return NULL;
    }
  } else
    i = nr;

  obj = Mem->GetObject();
  *obj = obj_proto[i];
  ObjList.ADD(obj);
  obj_index[i].number++;
  RANDOMLY_GENERATE_OBJ_IDNUM(obj);
  obj->load_origin = load_origin;
  obj->load_time = time(0);
  obj->pc_load_origin = pc_load_origin;
  obj->pc_load_idnum = pc_load_idnum;
  if (GET_OBJ_TYPE(obj) == ITEM_PHONE)
  {
    switch (GET_OBJ_VAL(obj, 0)) {
    case 0:
      GET_OBJ_VAL(obj, 8) = 1102;
      snprintf(buf, sizeof(buf), "%d206", number(0, 9));
      break;
    case 1:
      GET_OBJ_VAL(obj, 8) = 1102;
      snprintf(buf, sizeof(buf), "%d321", number(0, 9));
      break;
    case 2:
      GET_OBJ_VAL(obj, 8) = 1103;
      snprintf(buf, sizeof(buf), "%d503", number(0, 9));
      break;
    }
    GET_OBJ_VAL(obj, 0) = atoi(buf);
    GET_OBJ_VAL(obj, 1) = number(0, 9999);
  } else if (GET_OBJ_TYPE(obj) == ITEM_CYBERWARE && GET_OBJ_VAL(obj, 0) == CYB_PHONE)
  {
    switch (GET_OBJ_VAL(obj, 3)) {
    case 0:
      GET_OBJ_VAL(obj, 8) = 1102;
      snprintf(buf, sizeof(buf), "%d206", number(0, 9));
      break;
    case 1:
      GET_OBJ_VAL(obj, 8) = 1102;
      snprintf(buf, sizeof(buf), "%d321", number(0, 9));
      break;
    case 2:
      GET_OBJ_VAL(obj, 8) = 1103;
      snprintf(buf, sizeof(buf), "%d503", number(0, 9));
      break;
    }
    GET_OBJ_VAL(obj, 3) = atoi(buf);
    GET_OBJ_VAL(obj, 6) = number(0, 9999);
  } else if (GET_OBJ_TYPE(obj) == ITEM_GUN_MAGAZINE) {
    GET_OBJ_VAL(obj, 9) = GET_OBJ_VAL(obj, 0);
  } else if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
    handle_weapon_attachments(obj);

  if (GET_OBJ_TYPE(obj) == ITEM_PROGRAM
      || GET_OBJ_TYPE(obj) == ITEM_DESIGN
      || GET_OBJ_TYPE(obj) == ITEM_CHIP) {
    obj_to_matrix_file(obj, obj);
  }

  return obj;
}

SPECIAL(traffic);
extern void regenerate_traffic_msgs();
int traffic_infrequency_tick = -1;
void spec_update(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  int i;
  char empty_argument = '\0';

  // Instead of calculating the random number for every traffic room, just calc once.
  bool will_traffic = (++traffic_infrequency_tick % TRAFFIC_INFREQUENCY_CONTROL == 0);
  if (will_traffic) {
    regenerate_traffic_msgs();
    traffic_infrequency_tick = 0;
  }

  for (i = 0; i <= top_of_world; i++) {
    if (world[i].func == NULL)
      continue;

    if (world[i].func == traffic) {
      // Only print to these if we're doing traffic this pulse.
      if (!will_traffic)
        continue;
      
#ifdef IS_BUILDPORT
      // Skip any traffic-proc rooms that are !TRAFFIC. This case only happens during active building.
      if (ROOM_FLAGGED(&world[i], ROOM_NO_TRAFFIC))
        continue;
#endif

      // Fall through.
    }
    
    world[i].func (NULL, world + i, 0, &empty_argument);
  }

  ObjList.CallSpec();
}

#define ZO_DEAD  999

/* update zone ages, queue for reset if necessary, and dequeue when possible */
void zone_update(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  int i;
  static int timer = 0;
  /* Alot of good things came from 1992, like my next door neighbour's little sister for example.
     The original version of this function, however, was not one of those things - Che */
  /* jelson 10/22/92 */

  if (((++timer * PULSE_ZONE) / PASSES_PER_SEC) >= 60) {
    timer = 0;
    for (i = 0; i <= top_of_zone_table; i++) {
      if (zone_table[i].age < zone_table[i].lifespan &&
          zone_table[i].reset_mode != ZONE_RESET_NEVER)
        (zone_table[i].age)++;

      if (zone_table[i].age >= MAX(zone_table[i].lifespan,5) &&
          zone_table[i].age < ZO_DEAD && zone_table[i].reset_mode &&
          (zone_table[i].reset_mode == ZONE_RESET_ALWAYS ||
           zone_is_empty(i))) {
        reset_zone(i, 0);
      }
    }
  }
}

void log_zone_error(int zone, int cmd_no, const char *message)
{
  char buf[256];

  snprintf(buf, sizeof(buf), "error in zone file: %s", message);
  mudlog(buf, NULL, LOG_ZONELOG, TRUE);

  snprintf(buf, sizeof(buf), " ...offending cmd: '%c' cmd in zone #%d, line %d, cmd %d",
          ZCMD.command, zone_table[zone].number, ZCMD.line, cmd_no);
  mudlog(buf, NULL, LOG_ZONELOG, TRUE);
}

void zcmd_close_door(struct room_data *room, int dir, bool set_locked) {
  struct room_direction_data *door_struct = room->dir_option[dir];

  // Display a message if the door is neither hidden nor closed.
  if (!IS_SET(door_struct->exit_info, EX_HIDDEN) && !IS_SET(door_struct->exit_info, EX_CLOSED)) {
    snprintf(buf, sizeof(buf), "The %s to the %s closes.\r\n",
            door_struct->keyword ? fname(door_struct->keyword) : "door",
            fulldirs[dir]);
    send_to_room(buf, room);
  }

  SET_BIT(door_struct->exit_info, EX_CLOSED);
  if (set_locked)
    SET_BIT(door_struct->exit_info, EX_LOCKED);
}

void zcmd_open_door(struct room_data *room, int dir) {
  if (!room) {
    mudlog("SYSERR: zcmd_open_door received invalid room.", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  struct room_direction_data *door_struct = room->dir_option[dir];

  if (!door_struct) {
    mudlog("SYSERR: zcmd_open_door received invalid dir.", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // Display a message if the door is not hidden but is closed.
  if (!IS_SET(door_struct->exit_info, EX_HIDDEN) && IS_SET(door_struct->exit_info, EX_CLOSED)) {
    snprintf(buf, sizeof(buf), "The %s to the %s opens.\r\n",
            door_struct->keyword ? fname(door_struct->keyword) : "door",
            fulldirs[dir]);
    send_to_room(buf, room);
  }

  REMOVE_BIT(door_struct->exit_info, EX_CLOSED);
  REMOVE_BIT(door_struct->exit_info, EX_LOCKED);
}

void zcmd_repair_door(struct room_data *room, int dir) {
  struct room_direction_data *door_struct = room->dir_option[dir];

  if (IS_SET(door_struct->exit_info, EX_DESTROYED)) {
    snprintf(buf, sizeof(buf), "A po-faced passerby installs a new %s to the %s.\r\n",
             door_struct->keyword,
             thedirs[dir]);
    send_to_room(buf, room);
    REMOVE_BIT(door_struct->exit_info, EX_DESTROYED);
  }
  door_struct->condition = door_struct->barrier;
}

#define ZONE_ERROR(message) {log_zone_error(zone, cmd_no, message); last_cmd = 0;}

/* execute the reset command table of a given zone */
void reset_zone(int zone, int reboot)
{
  SPECIAL(fixer);
  int cmd_no, last_cmd = 0, found = 0, no_mob = 0, temp_qty = 0;
  static int i = 0;
  struct char_data *mob = NULL;
  struct obj_data *obj = NULL, *obj_to = NULL, *check = NULL;
  struct veh_data *veh = NULL;

  for (cmd_no = 0; cmd_no < zone_table[zone].num_cmds; cmd_no++) {
    /*
     snprintf(buf, sizeof(buf), "Processing ZCMD %d (zone %d): %c %s %ld %ld %ld",
            cmd_no, zone_table[zone].number, ZCMD.command, ZCMD.if_flag ? "(if last)" : "(always)", ZCMD.arg1, ZCMD.arg2, ZCMD.arg3);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
     */
    if (ZCMD.if_flag && !last_cmd)
      continue;
    found = 0;
    switch (ZCMD.command) {
    case '$':
      last_cmd = 0;
      break;
    case '*':                 /* ignore command */
      last_cmd = 0;
      break;
    case 'M':                 /* read a mobile */
      temp_qty = ZCMD.arg4;
      do {
        bool passed_global_limits = (mob_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1);
        bool passed_load_on_reboot = (ZCMD.arg2 == 0) && reboot;
        bool passed_room_limits = FALSE;

        if (ZCMD.arg2 < -1) {
          int total_seen_in_room = 0;
          int desired_total = (ZCMD.arg2 * -1) - 1;
          for (struct char_data *mob = world[ZCMD.arg3].people; mob; mob = mob->next_in_room) {
            if (GET_MOB_RNUM(mob) == ZCMD.arg1)
              total_seen_in_room++;
          }
          passed_room_limits = (total_seen_in_room < desired_total);
        }

        if (passed_global_limits || passed_load_on_reboot || passed_room_limits) {
          if (mob_index[ZCMD.arg1].vnum >= 20 && mob_index[ZCMD.arg1].vnum <= 22) {
            // Refuse to zoneload projections and matrix personas.
            char errbuf[1000];
            snprintf(errbuf, sizeof(errbuf), "Refusing to zoneload mob #%ld for zone %d-- illegal loading vnum.\r\n", mob_index[ZCMD.arg1].vnum, zone_table[zone].number);
            mudlog(errbuf, NULL, LOG_SYSLOG, TRUE);
            continue;
          }

          mob = read_mobile(ZCMD.arg1, REAL);
          mob->mob_loaded_in_room = GET_ROOM_VNUM(&world[ZCMD.arg3]);
          char_to_room(mob, &world[ZCMD.arg3]);
          act("$n has arrived.", TRUE, mob, 0, 0, TO_ROOM);
          last_cmd = 1;
        } else {
          if (ZCMD.arg2 == 0 && !reboot)
            no_mob = 1;
          else
            no_mob = 0;
          last_cmd = 0;
          mob = NULL;
        }
      } while (last_cmd && --temp_qty > 0);
      break;
    case 'S':                 /* read a mobile into a vehicle */
      if (!veh)
        break;

      {
        bool passed_global_limits = (mob_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1);
        bool passed_load_on_reboot = (ZCMD.arg2 == 0) && reboot;
        bool passed_room_limits = FALSE;

        if (ZCMD.arg2 < -1) {
          int total_seen_in_room = 0;
          int desired_total = (ZCMD.arg2 * -1) - 1;
          for (struct char_data *mob = veh->people; mob; mob = mob->next_in_veh) {
            if (GET_MOB_RNUM(mob) == ZCMD.arg1)
              total_seen_in_room++;
          }
          passed_room_limits = (total_seen_in_room < desired_total);
        }

        if (passed_global_limits || passed_load_on_reboot || passed_room_limits) {
          mob = read_mobile(ZCMD.arg1, REAL);
          mob->mob_loaded_in_room = 0;
          bool is_driver = !(veh->people);

          char_to_veh(veh, mob);

          if (is_driver) {
            // If the vehicle is empty, make the mob the driver.
            AFF_FLAGS(mob).SetBit(AFF_PILOT);
            mob->vfront = TRUE;

            veh->cspeed = SPEED_CRUISING;
          } else {
            // Look for hardpoints with weapons and man them.
            struct obj_data *mount = NULL;

            // Find an unmanned mount.
            for (mount = veh->mount; mount; mount = mount->next_content) {
              // Man the first unmanned mount we find, as long as it has a weapon in it.
              struct obj_data *weapon = get_mount_weapon(mount);
              if (!mount->worn_by && weapon) {
                mount->worn_by = mob;
                AFF_FLAGS(mob).SetBit(AFF_MANNING);

                // Ensure this mob has ammo for that weapon.
                ensure_mob_has_ammo_for_weapon(mob, weapon);
                char empty_argument[1]; *empty_argument = '\0';
                do_reload(mob, empty_argument, 0, 0);
                break;
              }
            }

            // Mount-users are all back of the bus.
            mob->vfront = FALSE;
          }

          last_cmd = 1;
        } else {
          if (ZCMD.arg2 == 0 && !reboot)
            no_mob = 1;
          else
            no_mob = 0;
          last_cmd = 0;
          mob = NULL;
        }
      }
      break;
    case 'U':                 /* mount/upgrades a vehicle with object */
      if (!veh)
        break;
      if ((obj_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1) || (ZCMD.arg2 == 0 && reboot)) {
        obj = read_object(ZCMD.arg1, REAL, OBJ_LOAD_REASON_ZONECMD);

        // Special case: Weapon mounts.
        if (GET_OBJ_VAL(obj, 0) == TYPE_MOUNT) {
          veh->usedload += get_obj_vehicle_load_usage(obj, TRUE);
          veh->sig -= get_mount_signature_penalty(obj);

          obj->next_content = veh->mount;
          veh->mount = obj;
        }

        // Special case: Weapons for mounts. Note that this ignores current vehicle load, mount size, etc.
        else if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && IS_GUN(GET_WEAPON_ATTACK_TYPE(obj))) {
          struct obj_data *mount = NULL;

          // Iterate through every mount on the vehicle.
          for (mount = veh->mount; mount; mount = mount->next_content) {
            // If we've found a weaponless mount, break out of loop.
            if (!mount_has_weapon(mount))
              break;
          }

          if (mount) {
            // We found a valid mount; attach the weapon.
            obj_to_obj(obj, mount);
            veh->usedload += get_obj_vehicle_load_usage(obj, FALSE);

            // Set the obj's firemode to the optimal one.
            if (IS_SET(GET_OBJ_VAL(obj, 10), 1 << MODE_BF))
              GET_OBJ_VAL(obj, 11) = MODE_BF;
            else if (IS_SET(GET_OBJ_VAL(obj, 10), 1 << MODE_FA)) {
              GET_OBJ_VAL(obj, 11) = MODE_FA;
              GET_OBJ_TIMER(obj) = 10;
            }
            else if (IS_SET(GET_OBJ_VAL(obj, 10), 1 << MODE_SA))
              GET_OBJ_VAL(obj, 11) = MODE_SA;
            else
              GET_OBJ_VAL(obj, 11) = MODE_SS;
          } else {
            ZONE_ERROR("Not enough mounts in target vehicle, cannot mount item");
            extract_obj(obj);
          }
        }
        else {
          if (GET_MOD(veh, GET_OBJ_VAL(obj, 0))) {
            snprintf(buf, sizeof(buf), "Warning: Double-upgrading vehicle %s with object %s (was %s). Extracting old mod.",
                     GET_VEH_NAME(veh),
                     GET_OBJ_NAME(obj),
                     GET_OBJ_NAME(GET_MOD(veh, GET_OBJ_VAL(obj, 0))));
            mudlog(buf, NULL, LOG_SYSLOG, TRUE);
            veh->usedload -= get_obj_vehicle_load_usage(GET_MOD(veh, GET_OBJ_VAL(obj, 0)), TRUE);
            for (int j = 0; j < MAX_OBJ_AFFECT; j++)
              affect_veh(veh, GET_MOD(veh, GET_OBJ_VAL(obj, 0))->affected[j].location, -GET_MOD(veh, GET_OBJ_VAL(obj, 0))->affected[j].modifier);
            
            // Remove the obj from the vehicle before extracting it so we don't crash during pointer verification.
            struct obj_data *extraction_ptr = GET_MOD(veh, GET_OBJ_VAL(obj, 0));
            GET_MOD(veh, GET_OBJ_VAL(obj, 0)) = NULL;
            extract_obj(extraction_ptr);
          }

          GET_MOD(veh, GET_OBJ_VAL(obj, 0)) = obj;
          veh->usedload += get_obj_vehicle_load_usage(obj, TRUE);
          for (int j = 0; j < MAX_OBJ_AFFECT; j++)
            affect_veh(veh, obj->affected[j].location, obj->affected[j].modifier);
        }

        last_cmd = 1;


      } else
        last_cmd = 0;
      break;
    case 'I':                 /* puts an item into vehicle */
      if (!veh)
        break;
      if ((obj_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1) ||
          (ZCMD.arg2 == 0 && reboot)) {
        obj = read_object(ZCMD.arg1, REAL, OBJ_LOAD_REASON_ZONECMD);
        obj_to_veh(obj, veh);
        last_cmd = 1;
      } else
        last_cmd = 0;
      break;
    case 'V':                 /* loads a vehicle */
      {
        bool passed_global_limits = (veh_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1);
        bool passed_load_on_reboot = (ZCMD.arg2 == 0) && reboot;
        bool passed_room_limits = FALSE;

        if (ZCMD.arg2 < -1) {
          int total_seen_in_room = 0;
          int desired_total = (ZCMD.arg2 * -1) - 1;
          for (struct veh_data *tmp_veh = world[ZCMD.arg3].vehicles; tmp_veh; tmp_veh = tmp_veh->next_veh) {
            if (GET_VEH_RNUM(tmp_veh) == ZCMD.arg1)
              total_seen_in_room++;
          }
          passed_room_limits = (total_seen_in_room < desired_total);
        }

        if (passed_global_limits || passed_load_on_reboot || passed_room_limits) {
          veh = read_vehicle(ZCMD.arg1, REAL);
          veh_to_room(veh, &world[ZCMD.arg3]);
          snprintf(buf, sizeof(buf), "%s has arrived.\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
          send_to_room(buf, veh->in_room);
          last_cmd = 1;
        } else
          last_cmd = 0;
      }
      break;
    case 'H':
      // Count the existing items in this host
      {
        // Log annoyingly if this is a bitshifted snowflake key.
        if (GET_OBJ_VNUM(&obj_proto[ZCMD.arg1]) == OBJ_SNOWFLAKE_KEY && matrix[ZCMD.arg3].vnum != HOST_SNOWFLAKE_KEY_LOCATION) {
          mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Bitshift happened! The snowflake key is attempting to load in inappropriate host %ld. Redirecting to proper host.", matrix[ZCMD.arg3].vnum);
          rnum_t actual_snowhost_rnum = real_host(HOST_SNOWFLAKE_KEY_LOCATION);
          if (actual_snowhost_rnum >= 0)
            ZCMD.arg3 = actual_snowhost_rnum;
          else
            mudlog("SYSERR: Never mind, proper host doesn't exist, guess we'll just gargle donkey balls today", NULL, LOG_SYSLOG, TRUE);
        }

        int already_there = 0;
        for (struct obj_data *contents = matrix[ZCMD.arg3].contents; contents; contents = contents->next_content) {
          if (GET_OBJ_VNUM(contents) == GET_OBJ_VNUM(&obj_proto[ZCMD.arg1]))
            already_there++;
        }

        if ((already_there < ZCMD.arg2) || (ZCMD.arg2 == -1) ||
            (ZCMD.arg2 == 0 && reboot)) {
          struct obj_data *new_obj = read_object(ZCMD.arg1, REAL, OBJ_LOAD_REASON_ZONECMD);
          struct host_data *to_host = &matrix[ZCMD.arg3];
          if (GET_OBJ_TYPE(new_obj) == ITEM_PROGRAM
            || GET_OBJ_TYPE(new_obj) == ITEM_DESIGN
            ) {
            struct matrix_file *file = obj_to_matrix_file(new_obj, NULL);
            file->in_host = to_host;
            file->next_file = to_host->files;
            to_host->files = file;
            extract_obj(new_obj);
          } else {
            new_obj->in_host = to_host;
            new_obj->next_content = to_host->contents;
            to_host->contents = new_obj;
          }
          last_cmd = 1;
        } else
          last_cmd = 0;
      }
      break;
    case 'O':                 /* read an object */
      {
        bool passed_global_limits = (obj_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1);
        bool passed_load_on_reboot = (ZCMD.arg2 == 0) && reboot;
        bool passed_room_limits = FALSE;

        if (ZCMD.arg2 < -1) {
          int total_seen_in_room = 0;
          int desired_total = (ZCMD.arg2 * -1) - 1;
          for (struct obj_data *obj = world[ZCMD.arg3].contents; obj; obj = obj->next_content) {
            if (GET_OBJ_RNUM(obj) == ZCMD.arg1)
              total_seen_in_room++;
          }
          passed_room_limits = (total_seen_in_room < desired_total);
        }

        if (passed_global_limits || passed_load_on_reboot || passed_room_limits) {
          obj = read_object(ZCMD.arg1, REAL, OBJ_LOAD_REASON_ZONECMD);
          obj_to_room(obj, &world[ZCMD.arg3]);

          act("You blink and realize that $p must have been here the whole time.", TRUE, 0, obj, 0, TO_ROOM);

          if (GET_OBJ_TYPE(obj) == ITEM_WORKSHOP && GET_WORKSHOP_GRADE(obj) == TYPE_WORKSHOP) {
            if (GET_WORKSHOP_TYPE(obj) == TYPE_VEHICLE && !ROOM_FLAGGED(obj->in_room, ROOM_GARAGE)) {
              // Warn the builder that they're breaking the game's rules (let it continue since it doesn't harm anything though).
              ZONE_ERROR("Zoneloading a pre-set-up vehicle workshop in a non-GARAGE room violates game convention about vehicle workshop locations. Flag the room as GARAGE.");
            }

            // It's a workshop, set it as unpacked already.
            GET_SETTABLE_WORKSHOP_IS_SETUP(obj) = 1;

            // Handle the room's workshop[] array.
            if (obj->in_room)
              add_workshop_to_room(obj);
          }
          else if (GET_OBJ_TYPE(obj) == ITEM_RADIO) {
            GET_RADIO_CENTERED_FREQUENCY(obj) = 8;
          }
          last_cmd = 1;
        } else
          last_cmd = 0;
      }
      break;
    case 'P':                 /* object to object */
      {
        bool passed_global_limits = (obj_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1);
        bool passed_load_on_reboot = (ZCMD.arg2 == 0) && reboot;
        bool passed_room_limits = FALSE;

        obj_to = ObjList.FindObj(ZCMD.arg3);

        if (obj_to && ZCMD.arg2 < -1) {
          int total_seen_in_room = 0;
          int desired_total = (ZCMD.arg2 * -1) - 1;
          for (struct obj_data *obj = obj_to->contains; obj; obj = obj->next_content) {
            if (GET_OBJ_RNUM(obj) == ZCMD.arg1)
              total_seen_in_room++;
          }
          passed_room_limits = (total_seen_in_room < desired_total);
        }

        if (passed_global_limits || passed_load_on_reboot || passed_room_limits) {
          if (!obj_to) {
            ZONE_ERROR("target obj not found");
            break;
          }

          obj = read_object(ZCMD.arg1, REAL, OBJ_LOAD_REASON_ZONECMD);
          
          if (obj != obj_to)
            obj_to_obj(obj, obj_to);
          if (GET_OBJ_TYPE(obj_to) == ITEM_HOLSTER) {
            GET_HOLSTER_READY_STATUS(obj_to) = 1;

            if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && IS_GUN(GET_WEAPON_ATTACK_TYPE(obj))) {
              // If it's carried by an NPC, make sure it's loaded.
              if (GET_WEAPON_MAX_AMMO(obj) > 0) {
                struct obj_data *outermost = obj;
                while (outermost && outermost->in_obj) {
                  outermost = outermost->in_obj;
                }

                struct char_data *temp_ch = NULL;
                if ((temp_ch = outermost->carried_by) || (temp_ch = outermost->worn_by)) {
                  // Reload from their ammo.
                  for (int index = 0; index < NUM_AMMOTYPES; index++) {
                    if (GET_BULLETPANTS_AMMO_AMOUNT(temp_ch, GET_WEAPON_ATTACK_TYPE(obj), npc_ammo_usage_preferences[index]) > 0) {
                      reload_weapon_from_bulletpants(temp_ch, obj, npc_ammo_usage_preferences[index]);
                      break;
                    }
                  }

                  // If they failed to reload, they have no ammo. Give them some normal and reload with it.
                  if (!obj->contains || GET_MAGAZINE_AMMO_COUNT(obj->contains) == 0) {
                    GET_BULLETPANTS_AMMO_AMOUNT(temp_ch, GET_WEAPON_ATTACK_TYPE(obj), AMMO_NORMAL) = GET_WEAPON_MAX_AMMO(obj) * NUMBER_OF_MAGAZINES_TO_GIVE_TO_UNEQUIPPED_MOBS;
                    reload_weapon_from_bulletpants(temp_ch, obj, AMMO_NORMAL);

                    // Decrement their debris-- we want this reload to not create clutter.
                    get_ch_in_room(temp_ch)->debris--;
                  }
                }
              }

              // Set the firemode.
              if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(obj), 1 << MODE_BF)) {
                GET_WEAPON_FIREMODE(obj) = MODE_BF;
              } else if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(obj), 1 << MODE_FA)) {
                GET_WEAPON_FIREMODE(obj) = MODE_FA;
                GET_OBJ_TIMER(obj) = 10;
              } else if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(obj), 1 << MODE_SA)) {
                GET_WEAPON_FIREMODE(obj) = MODE_SA;
              } else if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(obj), 1 << MODE_SS)) {
                GET_WEAPON_FIREMODE(obj) = MODE_SS;
              }
            }
          }
          last_cmd = 1;
        } else
          last_cmd = 0;
      }
      break;
    case 'G':                 /* obj_to_char */
      if (!mob) {
        if (!no_mob)
          ZONE_ERROR("attempt to give obj to non-existent mob");
        break;
      }

      {
        bool passed_global_limits = (obj_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1);
        bool passed_load_on_reboot = (ZCMD.arg2 == 0) && reboot;
        bool passed_room_limits = FALSE;

        if (obj_to && ZCMD.arg2 < -1) {
          int total_seen_in_room = 0;
          int desired_total = (ZCMD.arg2 * -1) - 1;
          for (struct obj_data *obj = mob->carrying; obj; obj = obj->next_content) {
            if (GET_OBJ_RNUM(obj) == ZCMD.arg1)
              total_seen_in_room++;
          }
          passed_room_limits = (total_seen_in_room < desired_total);
        }

        if (passed_global_limits || passed_load_on_reboot || passed_room_limits) {
          obj = read_object(ZCMD.arg1, REAL, OBJ_LOAD_REASON_ZONECMD);
          obj_to_char(obj, mob);
          last_cmd = 1;
        } else
          last_cmd = 0;
      }
      break;
    case 'E':                 /* object to equipment list */
      if (!mob) {
        if (!no_mob) {
          ZONE_ERROR("trying to equip non-existent mob");
          ZCMD.command = '*';
        }
        break;
      }
      {
        bool passed_global_limits = (obj_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1);
        bool passed_load_on_reboot = (ZCMD.arg2 == 0) && reboot;
        
        if (passed_global_limits || passed_load_on_reboot) {
          if (ZCMD.arg3 < 0 || ZCMD.arg3 >= NUM_WEARS) {
            ZONE_ERROR("invalid equipment pos number");
          } else {
            obj = read_object(ZCMD.arg1, REAL, OBJ_LOAD_REASON_ZONECMD);
            if (!equip_char(mob, obj, ZCMD.arg3) || GET_EQ(mob, ZCMD.arg3) != obj) {
              // Equip failure; destroy the object.
              extract_obj(obj);
              last_cmd = 0;
            } else {
              last_cmd = 1;

              // If it's a weapon, reload it.
              if (GET_OBJ_TYPE(obj) == ITEM_WEAPON
                  && IS_GUN(GET_WEAPON_ATTACK_TYPE(obj))
                  && GET_WEAPON_MAX_AMMO(obj) != -1) 
              {
                // Reload from their ammo.
                for (int index = 0; index < NUM_AMMOTYPES; index++) {
                  if (GET_BULLETPANTS_AMMO_AMOUNT(mob, GET_WEAPON_ATTACK_TYPE(obj), npc_ammo_usage_preferences[index]) > 0) {
                    reload_weapon_from_bulletpants(mob, obj, npc_ammo_usage_preferences[index]);
                    break;
                  }
                }

                // If they failed to reload, they have no ammo. Give them some normal and reload with it.
                if (!obj->contains || GET_MAGAZINE_AMMO_COUNT(obj->contains) == 0) {
                  GET_BULLETPANTS_AMMO_AMOUNT(mob, GET_WEAPON_ATTACK_TYPE(obj), AMMO_NORMAL) = GET_WEAPON_MAX_AMMO(obj) * NUMBER_OF_MAGAZINES_TO_GIVE_TO_UNEQUIPPED_MOBS;
                  reload_weapon_from_bulletpants(mob, obj, AMMO_NORMAL);

                  // Decrement their debris-- we want this reload to not create clutter.
                  get_ch_in_room(mob)->debris--;
                }
              }
            }
          }
        } else
          last_cmd = 0;
      }
      break;
    case 'N':  // give x number of items to a mob
      if (!mob) {
        if (!no_mob)
          ZONE_ERROR("attempt to give obj to non-existent mob");
        break;
      }
      last_cmd = 0;
      for (i = 0; (i < ZCMD.arg3) && ((obj_index[ZCMD.arg1].number < ZCMD.arg2) ||
                                      (ZCMD.arg2 == -1) || (ZCMD.arg2 == 0 && reboot)); ++i) {
        obj = read_object(ZCMD.arg1, REAL, OBJ_LOAD_REASON_ZONECMD);
        obj_to_char(obj, mob);
        last_cmd = 1;
      }
      break;
    case 'C': // give mob bio/cyberware
      if (!mob) {
        if (!no_mob)
          ZONE_ERROR("attempt to give obj to non-existent mob");
        break;
      }
      obj = read_object(ZCMD.arg1, REAL, OBJ_LOAD_REASON_ZONECMD);
      if (!ZCMD.arg2) {
        if (GET_OBJ_TYPE(obj) != ITEM_CYBERWARE) {
          ZONE_ERROR("attempt to install non-cyberware to mob");
          ZCMD.command = '*';
          extract_obj(obj);
          break;
        }
        if (GET_ESS(mob) < GET_OBJ_VAL(obj, 4)) {
          extract_obj(obj);
          break;
        }
        GET_ESS(mob) -= GET_OBJ_VAL(obj, 4);
        obj_to_cyberware(obj, mob);
      } else {
        if (GET_OBJ_TYPE(obj) != ITEM_BIOWARE) {
          ZONE_ERROR("attempt to install non-bioware to mob");
          ZCMD.command = '*';
          extract_obj(obj);
          break;
        }
        if (GET_INDEX(mob) < GET_OBJ_VAL(obj, 4)) {
          extract_obj(obj);
          break;
        }
        GET_INDEX(mob) -= GET_OBJ_VAL(obj, 4);
        for (check = mob->bioware; check && !found; check = check->next_content) {
          if ((GET_OBJ_VNUM(check) == GET_OBJ_VNUM(obj)))
            found = 1;
          if (GET_OBJ_VAL(check, 2) == GET_OBJ_VAL(obj, 2))
            found = 1;
        }
        if (GET_OBJ_VAL(obj, 2) == 2 || GET_OBJ_VAL(obj, 2) == 8 || GET_OBJ_VAL(obj, 2) == 10)
          for (check = mob->cyberware; check; check = check->next_content) {
            if (GET_OBJ_VAL(check, 2) == 23 && GET_OBJ_VAL(obj, 2) == 2)
              found = 1;
            if (GET_OBJ_VAL(check, 2) == 30 && GET_OBJ_VAL(obj, 2) == 8)
              found = 1;
            if (GET_OBJ_VAL(check, 2) == 20 && GET_OBJ_VAL(obj, 2) == 10)
              found = 1;
          }
        if (found) {
          extract_obj(obj);
          break;
        }
        if (GET_OBJ_VAL(obj, 2) == 0)
          GET_OBJ_VAL(obj, 5) = 24;
        obj_to_bioware(obj, mob);
      }
      last_cmd = 1;
      break;
    case 'R': /* rem obj from room */
      if ((obj = get_obj_in_list_num(ZCMD.arg2, world[ZCMD.arg1].contents)) != NULL) {
        act("$o is whisked away.", TRUE, 0, obj, 0, TO_ROOM);
        obj_from_room(obj);
        extract_obj(obj);
      }
      last_cmd = 1;
      break;
#define DOOR_STRUCT world[ZCMD.arg1].dir_option[ZCMD.arg2]
#define REV_DOOR_STRUCT opposite_room->dir_option[rev_dir[ZCMD.arg2]]
    case 'D':                 /* set state of door */
      {
        if (ZCMD.arg2 < 0 || ZCMD.arg2 >= NUM_OF_DIRS) {
          ZONE_ERROR("Invalid direction specified.");
          break;
        }

        if (DOOR_STRUCT == NULL) {
          snprintf(buf, sizeof(buf), "%s exit from %ld does not exist.", capitalize(dirs[ZCMD.arg2]), world[ZCMD.arg1].number);
          ZONE_ERROR(buf);
          break;
        }

        struct room_data *opposite_room = DOOR_STRUCT->to_room;

        if (!IS_SET(DOOR_STRUCT->exit_info, EX_ISDOOR)) {
          snprintf(buf, sizeof(buf), "%s exit from %ld exists but is not set to be a door.", capitalize(dirs[ZCMD.arg2]), world[ZCMD.arg1].number);
          ZONE_ERROR(buf);
          zcmd_open_door(&world[ZCMD.arg1], ZCMD.arg2);
          if (opposite_room && REV_DOOR_STRUCT && (&world[ZCMD.arg1] == REV_DOOR_STRUCT->to_room))
            zcmd_open_door(opposite_room, rev_dir[ZCMD.arg2]);
          break;
        }

        bool ok = FALSE;

        if (!opposite_room || !REV_DOOR_STRUCT || (&world[ZCMD.arg1] != REV_DOOR_STRUCT->to_room)) {
          if (!IS_SET(DOOR_STRUCT->exit_info, EX_WINDOWED) && !IS_SET(DOOR_STRUCT->exit_info, EX_BARRED_WINDOW)) {
            snprintf(buf, sizeof(buf), "Note: %s exit from %ld to %ld has no back-linked exit, so zone command to toggle its door will only work on one side. (zone %d, line %d, cmd %d)",
                    capitalize(dirs[ZCMD.arg2]), world[ZCMD.arg1].number, opposite_room->number,
                    zone_table[zone].number, ZCMD.line, cmd_no);
            mudlog(buf, NULL, LOG_ZONELOG, FALSE);
          }
        } else
          ok = TRUE;

        if (ok && !IS_SET(REV_DOOR_STRUCT->exit_info, EX_ISDOOR)) {
          snprintf(buf, sizeof(buf), "Note: %s exit from %ld to %ld: Reverse exit is not a door. Overriding reverse exit to be a door. (zone %d, line %d, cmd %d)",
                  capitalize(dirs[ZCMD.arg2]), world[ZCMD.arg1].number, opposite_room->number, zone_table[zone].number,
                  ZCMD.line, cmd_no);
          mudlog(buf, NULL, LOG_ZONELOG, FALSE);
          REV_DOOR_STRUCT->exit_info = DOOR_STRUCT->exit_info;
        }

        // here I set the hidden flag for the door if hidden > 0
        if (DOOR_STRUCT->hidden)
          SET_BIT(DOOR_STRUCT->exit_info, EX_HIDDEN);
        if (ok && REV_DOOR_STRUCT->hidden)
          SET_BIT(REV_DOOR_STRUCT->exit_info, EX_HIDDEN);

        // repair all damage
        zcmd_repair_door(&world[ZCMD.arg1], ZCMD.arg2);
        if (ok)
          zcmd_repair_door(opposite_room, rev_dir[ZCMD.arg2]);

        // Clone the exit info across to the other side, if it exists.
        if (ok) {
          REV_DOOR_STRUCT->material = DOOR_STRUCT->material;
          REV_DOOR_STRUCT->barrier = DOOR_STRUCT->barrier;
          REV_DOOR_STRUCT->condition = DOOR_STRUCT->condition;
        }

        switch (ZCMD.arg3) {
          // you now only have to set one side of a door
          case 0: // Door is open.
            zcmd_open_door(&world[ZCMD.arg1], ZCMD.arg2);
            if (ok)
              zcmd_open_door(opposite_room, rev_dir[ZCMD.arg2]);
            break;
          case 1: // Door is closed.
          case 2: // Door is closed and locked.
            zcmd_close_door(&world[ZCMD.arg1], ZCMD.arg2, (ZCMD.arg3 == 2));
            if (ok)
              zcmd_close_door(opposite_room, rev_dir[ZCMD.arg2], (ZCMD.arg3 == 2));

            break;
        }
      }
      last_cmd = 1;
      break;
#undef REV_DOOR_STRUCT
#undef DOOR_STRUCT

    default:
      snprintf(buf, sizeof(buf), "Unknown cmd '%c' in reset table; cmd disabled. Args were %ld %ld %ld.",
              ZCMD.command, ZCMD.arg1, ZCMD.arg2, ZCMD.arg3);
      ZONE_ERROR(buf);
      ZCMD.command = '*';
      break;
    }
  }

  zone_table[zone].age = 0;
  for (int counter = zone_table[zone].number * 100;
       counter <= zone_table[zone].top; counter++) {
    long rnum = real_room(counter);
    if (rnum > 0 && world[rnum].background[PERMANENT_BACKGROUND_COUNT]) {
      world[rnum].background[CURRENT_BACKGROUND_COUNT] = world[rnum].background[PERMANENT_BACKGROUND_COUNT];
      world[rnum].background[CURRENT_BACKGROUND_TYPE] = world[rnum].background[PERMANENT_BACKGROUND_TYPE];
    }
  }

}

/* for use in reset_zone; return TRUE if zone 'nr' is Free of non-idle PC's  */
int zone_is_empty(int zone_nr)
{
  struct descriptor_data *i;

  for (i = descriptor_list; i; i = i->next)
    if (!i->connected && i->character && i->character->char_specials.timer < IDLE_TIMER_ZONE_RESET_THRESHOLD)
      if (i->character->in_room && i->character->in_room->zone == zone_nr)
        return 0;

  return 1;
}


/************************************************************************
*  funcs of a (more or less) general utility nature                     *
********************************************************************** */

// These were added for OLC purposes.  They will allocate a new array of
// the old size + 100 elements, copy over, and Free up the old one.
// They return TRUE if it worked, FALSE if it did not.  I could add in some
// checks and report to folks using OLC that currently there is not enough
// room to allocate.  Obviously these belong in an object once I convert
// completely over to C++.
bool resize_world_array()
{
  int counter;
  struct room_data *new_world;

  new_world = new struct room_data[top_of_world_array + world_chunk_size];

  if (!new_world) {
    mudlog("Unable to allocate new world array.", NULL, LOG_SYSLOG, TRUE);
    mudlog("OLC temporarily unavailable.", NULL, LOG_SYSLOG, TRUE);
    olc_state = 0;  // disallow any olc from here on
    return FALSE;
  }

  // remember that top_of_world is the actual rnum in the array
  for (counter = 0; counter <= top_of_world; counter++)
    new_world[counter] = world[counter];

  // TODO: Update EVERY SINGLE ROOM POINTER IN THE GAME to match the new array.
  mudlog("The MUD will crash now, since required work is not done.", NULL, LOG_SYSLOG, TRUE);

  top_of_world_array += world_chunk_size;

  DELETE_ARRAY_IF_EXTANT(world);
  world = new_world;

  snprintf(buf, sizeof(buf), "World array resized to %d.", top_of_world_array);
  mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  return TRUE;
}

bool resize_qst_array(void)
{
  int counter;
  struct quest_data *new_qst;

  new_qst = new struct quest_data[top_of_quest_array + quest_chunk_size];

  if (!new_qst) {
    mudlog("Unable to allocate new quest array.", NULL, LOG_SYSLOG, TRUE);
    mudlog("OLC temporarily unavailable.", NULL, LOG_SYSLOG, TRUE);
    olc_state = 0;
    return FALSE;
  }

  for (counter = 0; counter <= top_of_questt; counter++)
    new_qst[counter] = quest_table[counter];

  top_of_quest_array += quest_chunk_size;

  DELETE_ARRAY_IF_EXTANT(quest_table);
  quest_table = new_qst;

  snprintf(buf, sizeof(buf), "Quest array resized to %d", top_of_quest_array);
  mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  return TRUE;
}

/* read and allocate space for a '~'-terminated string from a given file */
char *fread_string(FILE * fl, char *error)
{
  char buf[MAX_STRING_LENGTH+3], tmp[512+3], *rslt;
  char *point;
  int done = 0, length = 0, templength = 0;

  /* FULLY initialize the buffer array. This is important, because you
  can't have garbage being read in. Doing the first byte isn't really good
  enough, and using memset or bzero is something I don't like. */
  /* Yeah, except you can go fuck yourself, memset is literally designed for this task. -LS */
  memset(buf, 0, sizeof(char) * (MAX_STRING_LENGTH+3));

  do {
    if (!fgets(tmp, 512, fl)) {
      fprintf(stderr, "FATAL SYSERR: fread_string: format error at or near %s\n", error);
      exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
    }
    /* If there is a '~', end the string; else put an "\r\n" over the '\n'. */
    if ((point = strchr(tmp, '~')) != NULL) {
      *point = '\0';
      done = 1;
      /* Instead of an unconditional 'else', we only replace on what we want
      to replace, instead of acting blindly. */
    } else if ((point = strchr(tmp, '\n')) != NULL) {
      *(point++) = '\r';
      *(point++) = '\n';
      *point = '\0';
    }

    templength = strlen(tmp);

    if ((length + templength + 3) >= MAX_STRING_LENGTH) {
      log_vfprintf("FATAL SYSERR: fread_string: string too large at or near %s\n", error);
      exit(ERROR_WORLD_BOOT_FORMAT_ERROR);
    } else {

      /* Last but not least, we copy byte by byte from array to array. It's
      really safer this way, not to mention much more portable. */
      for (int y=0; y <= templength; y++) {
        buf[length + y] = tmp[y];
      }
      length += templength;
    }
  } while (!done);

  /* allocate space for the new string and copy it */
  if (strlen(buf) > 0) {
    size_t rslt_size = length + 1;
    rslt = new char[rslt_size];
    memset(rslt, 0, sizeof(char) * (rslt_size));
    strlcpy(rslt, buf, rslt_size);
  } else
    rslt = NULL;

  return rslt;
}

/* release memory allocated for a char struct */
void free_char(struct char_data * ch)
{
  int i;

  /* clean up spells */
  {
    struct spell_data *next = NULL, *temp = GET_SPELLS(ch);
    while (temp) {
      next = temp->next;
      DELETE_ARRAY_IF_EXTANT(temp->name);
      DELETE_AND_NULL(temp);
      temp = next;
    }
    GET_SPELLS(ch) = NULL;
  }

  /* Not sure if enabling this code is a good idea yet, so I'm leaving it off. -LS
  // clean up matrix data
  if (ch->persona) {
    DELETE_ARRAY_IF_EXTANT(ch->persona->long_desc);
    DELETE_ARRAY_IF_EXTANT(ch->persona->look_desc);

    // BWAAAAAAAAAAAAAAAAA (inception horn)
    if (ch->persona->decker) {
      struct seen_data *next = NULL, *temp = ch->persona->decker->seen;
      for (; temp; temp = next) {
        next = temp->next;
        delete temp;
      }
    }

    // All kinds of things are being ignored here (what happens to your phone? hitcher? etc?),
    //  but if you've called the delete-this-character-and-everything-associated-in-it-from-memory
    //  method without first handling that stuff, that's on you. Try calling extract_char() first.

    delete ch->persona;
    ch->persona = NULL;
  } */

  if (ch->player_specials != NULL && ch->player_specials != &dummy_mob)
  {
    // we have to delete these here before we delete the struct
    {
      struct alias *temp, *next;
      for (struct alias *a = GET_ALIASES(ch); a; a = next) {
        next = a->next;
        REMOVE_FROM_LIST(a, GET_ALIASES(ch), next);
        delete a;
      }
    }

    {
      struct remem *nextr;
      for (struct remem *b = GET_PLAYER_MEMORY(ch); b; b = nextr) {
        nextr = b->next;
        DELETE_ARRAY_IF_EXTANT(b->mem);
        DELETE_AND_NULL(b);
      }
    }

    DELETE_ARRAY_IF_EXTANT(ch->player_specials->obj_complete);
    DELETE_ARRAY_IF_EXTANT(ch->player_specials->mob_complete);

    delete [] ch->player_specials->saved.lifestyle_string;

    DELETE_IF_EXTANT(ch->player_specials);

    if (IS_NPC(ch))
      log("SYSERR: Mob had player_specials allocated!");
  }
  if (!IS_NPC(ch) || (IS_NPC(ch) && GET_MOB_RNUM(ch) == -1))
  {
    DELETE_ARRAY_IF_EXTANT(ch->player.char_name);
    DELETE_ARRAY_IF_EXTANT(ch->player.background);
    DELETE_ARRAY_IF_EXTANT(ch->player.title);
    DELETE_ARRAY_IF_EXTANT(ch->player.pretitle);
    DELETE_ARRAY_IF_EXTANT(ch->player.whotitle);
    DELETE_ARRAY_IF_EXTANT(ch->player.prompt);
    DELETE_ARRAY_IF_EXTANT(ch->player.matrixprompt);
    DELETE_ARRAY_IF_EXTANT(ch->player.poofin);
    DELETE_ARRAY_IF_EXTANT(ch->player.poofout);
    DELETE_ARRAY_IF_EXTANT(ch->char_specials.arrive);
    DELETE_ARRAY_IF_EXTANT(ch->char_specials.leave);
    DELETE_ARRAY_IF_EXTANT(SETTABLE_CHAR_COLOR_HIGHLIGHT(ch));
    DELETE_ARRAY_IF_EXTANT(ch->player.email);


    if(!IS_NPC(ch))
      DELETE_ARRAY_IF_EXTANT(ch->player.host);

    { // Delete physical, astral, and matrix text fields.
      text_data *tab[3] = {
                            &ch->player.physical_text,
                            &ch->player.astral_text,
                            &ch->player.matrix_text
                          };

      for (int i = 0; i < 3; i++) {
        text_data *ptr = tab[i];

        DELETE_ARRAY_IF_EXTANT(ptr->keywords);
        DELETE_ARRAY_IF_EXTANT(ptr->name);
        DELETE_ARRAY_IF_EXTANT(ptr->room_desc);
        DELETE_ARRAY_IF_EXTANT(ptr->look_desc);
      }
    }

  } else if ((i = GET_MOB_RNUM(ch)) > -1 &&
             GET_MOB_VNUM(ch) != 20 && GET_MOB_VNUM(ch) != 22)
  {
    /* otherwise, Free strings only if the string is not pointing at proto */
    text_data *tab[3] = {
                          &ch->player.physical_text,
                          &ch->player.astral_text,
                          &ch->player.matrix_text
                        };

    char_data *proto = mob_proto+i;

    text_data *proto_tab[3] = {
                                &proto->player.physical_text,
                                &proto->player.astral_text,
                                &proto->player.matrix_text
                              };

    for (int i = 0; i < 3; i++) {
      text_data *ptr = tab[i];
      text_data *proto_ptr = proto_tab[i];

      if (ptr->keywords && ptr->keywords != proto_ptr->keywords) {
        DELETE_AND_NULL_ARRAY(ptr->keywords);
      }

      if (ptr->name && ptr->name != proto_ptr->name) {
        DELETE_AND_NULL_ARRAY(ptr->name);
      }

      if (ptr->room_desc && ptr->room_desc != proto_ptr->room_desc) {
        DELETE_AND_NULL_ARRAY(ptr->room_desc);
      }

      if (ptr->look_desc && ptr->look_desc != proto_ptr->look_desc) {
        DELETE_AND_NULL_ARRAY(ptr->look_desc);
      }
    }

    if (ch->char_specials.arrive && ch->char_specials.arrive != proto->char_specials.arrive) {
      DELETE_AND_NULL_ARRAY(ch->char_specials.arrive);
    }

    if (ch->char_specials.leave && ch->char_specials.leave != proto->char_specials.leave) {
      DELETE_AND_NULL_ARRAY(ch->char_specials.leave);
    }

    if (SETTABLE_CHAR_COLOR_HIGHLIGHT(ch) && SETTABLE_CHAR_COLOR_HIGHLIGHT(ch) != SETTABLE_CHAR_COLOR_HIGHLIGHT(ch)) {
      DELETE_AND_NULL_ARRAY(SETTABLE_CHAR_COLOR_HIGHLIGHT(ch));
    }
  }

  clearMemory(ch);

  clear_char(ch);
}

void free_room(struct room_data *room)
{
  struct extra_descr_data *This, *next_one;

  // first free up the strings
  DELETE_ARRAY_IF_EXTANT(room->name);
  DELETE_ARRAY_IF_EXTANT(room->description);
  DELETE_ARRAY_IF_EXTANT(room->night_desc);
  DELETE_ARRAY_IF_EXTANT(room->address);

  // then free up the exits
  for (int counter = 0; counter < NUM_OF_DIRS; counter++)
  {
    if (room->dir_option[counter]) {
      DELETE_ARRAY_IF_EXTANT(room->dir_option[counter]->general_description);
      DELETE_ARRAY_IF_EXTANT(room->dir_option[counter]->keyword);
      DELETE_IF_EXTANT(room->dir_option[counter]);
    }
    room->dir_option[counter] = NULL;
  }
  // now the extra descriptions
  if (room->ex_description) {
    for (This = room->ex_description; This; This = next_one)
    {
      next_one = This->next;
      DELETE_ARRAY_IF_EXTANT(This->keyword);
      DELETE_ARRAY_IF_EXTANT(This->description);
      DELETE_IF_EXTANT(This);
    }
    room->ex_description = NULL;
  }

  clear_room(room);
}

void free_host(struct host_data * host)
{
  DELETE_ARRAY_IF_EXTANT(host->name);
  DELETE_ARRAY_IF_EXTANT(host->keywords);
  DELETE_ARRAY_IF_EXTANT(host->desc);
  DELETE_ARRAY_IF_EXTANT(host->shutdown_start);
  DELETE_ARRAY_IF_EXTANT(host->shutdown_stop);

  { // Clean up the trigger steps.
    struct trigger_step *trigger = NULL, *next = NULL;
    for (trigger = host->trigger; trigger; trigger = next) {
      next = trigger->next;
      delete trigger;
    }
    host->trigger = NULL;
  }

  { // Clean up associated entrances.
    for (struct exit_data *exit = host->exit; exit; exit = exit->next) {
      // Find the exit's matching entrance in the destination host.
      rnum_t dest_rnum = real_host(exit->host);
      if (dest_rnum >= 0) {
        struct entrance_data *entrance = NULL, *temp;
        for (entrance = matrix[dest_rnum].entrance; entrance; entrance = entrance->next) {
          if (entrance->host == host)
            break;
        }
        // Remove that entrance.
        if (entrance) {
          REMOVE_FROM_LIST(entrance, matrix[dest_rnum].entrance, next);
          DELETE_AND_NULL(entrance);
        } else {
          // Not an error condition. We exited hedit and are just cleaning up dangling entrances.
          // log("SYSERR: Found no associated entrance in free_host()! ('x' option during hedit?)");
        }
      }
    }
  }

  { // Clean up the exits.
    struct exit_data *exit = NULL, *next = NULL;
    for (exit = host->exit; exit; exit = next) {
      next = exit->next;
      DELETE_ARRAY_IF_EXTANT(exit->addresses);
      DELETE_ARRAY_IF_EXTANT(exit->roomstring);
      delete exit;
    }
    host->exit = NULL;
  }

  clear_host(host);
}

void free_icon(struct matrix_icon * icon)
{
  if (!icon->vnum) {
    DELETE_ARRAY_IF_EXTANT(icon->name);
    DELETE_ARRAY_IF_EXTANT(icon->long_desc);
    DELETE_ARRAY_IF_EXTANT(icon->look_desc);
  }
  clear_icon(icon);
}
/* release memory allocated for an obj struct */
void free_obj(struct obj_data * obj)
{
  int nr;
  struct extra_descr_data *this1, *next_one;
  if ((nr = GET_OBJ_RNUM(obj)) == -1)
  {
    DELETE_ARRAY_IF_EXTANT(obj->text.keywords);
    DELETE_ARRAY_IF_EXTANT(obj->text.name);
    DELETE_ARRAY_IF_EXTANT(obj->text.room_desc);
    DELETE_ARRAY_IF_EXTANT(obj->text.look_desc);

    if (obj->ex_description) {
      for (this1 = obj->ex_description; this1; this1 = next_one) {
        next_one = this1->next;
        DELETE_ARRAY_IF_EXTANT(this1->keyword);
        DELETE_ARRAY_IF_EXTANT(this1->description);
        DELETE_IF_EXTANT(this1);
      }
      obj->ex_description = NULL;
    }
  } else
  {
    if (obj->text.keywords && obj->text.keywords != obj_proto[nr].text.keywords) {
      DELETE_AND_NULL_ARRAY(obj->text.keywords);
    }

    if (obj->text.name && obj->text.name != obj_proto[nr].text.name) {
      DELETE_AND_NULL_ARRAY(obj->text.name);
    }

    if (obj->text.room_desc && obj->text.room_desc != obj_proto[nr].text.room_desc) {
      DELETE_AND_NULL_ARRAY(obj->text.room_desc);
    }

    if (obj->text.look_desc && obj->text.look_desc != obj_proto[nr].text.look_desc) {
      DELETE_AND_NULL_ARRAY(obj->text.look_desc);
    }

    if (obj->ex_description && obj->ex_description != obj_proto[nr].ex_description) {
      for (this1 = obj->ex_description; this1; this1 = next_one) {
        next_one = this1->next;
        DELETE_ARRAY_IF_EXTANT(this1->keyword);
        DELETE_ARRAY_IF_EXTANT(this1->description);
        DELETE_IF_EXTANT(this1);
      }
      obj->ex_description = NULL;
    }
  }
  DELETE_ARRAY_IF_EXTANT(obj->restring);
  DELETE_ARRAY_IF_EXTANT(obj->graffiti);
  DELETE_ARRAY_IF_EXTANT(obj->photo);
  clear_object(obj);
}

void free_quest(struct quest_data *quest)
{
  DELETE_ARRAY_IF_EXTANT(quest->obj);
  DELETE_ARRAY_IF_EXTANT(quest->mob);
  DELETE_ARRAY_IF_EXTANT(quest->intro);
  DELETE_ARRAY_IF_EXTANT(quest->decline);
  DELETE_ARRAY_IF_EXTANT(quest->quit);
  DELETE_ARRAY_IF_EXTANT(quest->finish);
  DELETE_ARRAY_IF_EXTANT(quest->info);
  DELETE_ARRAY_IF_EXTANT(quest->s_string);
  DELETE_ARRAY_IF_EXTANT(quest->e_string);
  DELETE_ARRAY_IF_EXTANT(quest->done);
}

void free_shop(struct shop_data *shop)
{
  DELETE_ARRAY_IF_EXTANT(shop->no_such_itemk);
  DELETE_ARRAY_IF_EXTANT(shop->no_such_itemp);
  DELETE_ARRAY_IF_EXTANT(shop->not_enough_nuyen);
  DELETE_ARRAY_IF_EXTANT(shop->doesnt_buy);
  DELETE_ARRAY_IF_EXTANT(shop->buy);
  DELETE_ARRAY_IF_EXTANT(shop->sell);
  DELETE_ARRAY_IF_EXTANT(shop->shopname);
}

/* read contets of a text file, alloc space, point buf to it */
int file_to_string_alloc(const char *name, char **buf)
{
  char temp[MAX_STRING_LENGTH];

  if (file_to_string(name, temp, sizeof(temp)) < 0)
    return -1;

  // log_vfprintf("file_to_string_alloc: File '%s' read out as '%s'.", name, temp);

  DELETE_ARRAY_IF_EXTANT(*buf);

  *buf = str_dup(temp);

  return 0;
}

/* read contents of a text file, and place in buf */
int file_to_string(const char *name, char *buf, size_t buf_size)
{
  FILE *fl;
  // Made it hella long. It's 2020, let clients word wrap their own shit.
  char tmp[MAX_STRING_LENGTH];
  memset(tmp, 0, sizeof(tmp));

  *buf = '\0';

  if (!(fl = fopen(name, "r"))) {
    snprintf(tmp, sizeof(tmp), "Error reading %s", name);
    perror(tmp);
    return (-1);
  }
  do {
    fgets(tmp, sizeof(tmp) - 1, fl);
    tmp[MAX(0, (int)(strlen(tmp)) - 1)] = '\0';/* take off the trailing \n */
    strlcat(tmp, "\r\n", sizeof(tmp));

    if (!feof(fl)) {
      if (strlen(buf) + strlen(tmp) + 1 > MAX_STRING_LENGTH) {
        log("SYSERR: fl->strng: string too big (db.c, file_to_string)");
        *buf = '\0';
        return (-1);
      }
      strlcat(buf, tmp, buf_size);
    }
  } while (!feof(fl));

  fclose(fl);

  return (0);
}

/* clear some of the the working variables of a char */
void reset_char(struct char_data * ch)
{
  ch->in_veh = NULL;
  ch->followers = NULL;
  ch->master = NULL;
  ch->in_room = NULL;

  remove_ch_from_character_list(ch, "reset_char");
  ch->next_in_character_list = NULL;

  ch->next_fighting = NULL;
  ch->next_in_room = NULL;
  FIGHTING(ch) = NULL;
  GET_POS(ch) = POS_STANDING;
  GET_DEFAULT_POS(ch) = POS_STANDING;
  calc_weight(ch);

  if (GET_PHYSICAL(ch) < 100)
    GET_PHYSICAL(ch) = 100;
  if (GET_MENTAL(ch) < 100)
    GET_MENTAL(ch) = 100;
}

void clear_char(struct char_data * ch)
{
  reset_char(ch);
  GET_WAS_IN(ch) = NULL;
  if (ch->points.max_mental < 1000)
    ch->points.max_mental = 1000;
  if (ch->points.max_physical < 1000)
    ch->points.max_physical = 1000;
  ch->player.time.logon = time(0);

#ifdef USE_DEBUG_CANARIES
  ch->canary = CANARY_VALUE;
#endif
}

/* Clear ALL the vars of an object; don't free up space though */
// we do this because generally, objects which are created are off of
// prototypes, and we do not want to delete the prototype strings
void clear_object(struct obj_data * obj)
{
  memset((char *) obj, 0, sizeof(struct obj_data));
  obj->item_number = NOTHING;
  obj->in_room = NULL;

#ifdef USE_DEBUG_CANARIES
  obj->canary = CANARY_VALUE;
#endif
}

void clear_room(struct room_data *room)
{
  memset((char *) room, 0, sizeof(struct room_data));
#ifdef USE_DEBUG_CANARIES
  room->canary = CANARY_VALUE;
#endif
}

void clear_vehicle(struct veh_data *veh)
{
  struct obj_data *next_obj;

  DELETE_ARRAY_IF_EXTANT(veh->restring);
  DELETE_ARRAY_IF_EXTANT(veh->restring_long);
  DELETE_ARRAY_IF_EXTANT(veh->decorate_front);
  DELETE_ARRAY_IF_EXTANT(veh->decorate_rear);

  // Remove any mounts.
  for (struct obj_data *mount = veh->mount; mount; mount = next_obj) {
    next_obj = mount->next_content;
    extract_obj(mount);
  }
  veh->mount = NULL;

  // Go through the installed mods and extract them.
  for (int mod_idx = 0; mod_idx < NUM_MODS; mod_idx++) {
    if (veh->mod[mod_idx]) {
      extract_obj(veh->mod[mod_idx]);
    }
    veh->mod[mod_idx] = NULL;
  }

  // Wipe out the veh.
  memset((char *) veh, 0, sizeof(struct veh_data));
  veh->in_room = NULL;

#ifdef USE_DEBUG_CANARIES
  veh->canary = CANARY_VALUE;
#endif
}

void clear_host(struct host_data *host)
{
  memset((char *) host, 0, sizeof(struct host_data));

#ifdef USE_DEBUG_CANARIES
  host->canary = CANARY_VALUE;
#endif
}

void clear_icon(struct matrix_icon *icon)
{
  memset((char *) icon, 0, sizeof(struct matrix_icon));
  icon->in_host = NOWHERE;

#ifdef USE_DEBUG_CANARIES
  icon->canary = CANARY_VALUE;
#endif
}

/* returns the real number of the room with given virtual number */
rnum_t real_room(vnum_t virt)
{
  if (virt < 0)
    return -1;

  long bot, top, mid;

  bot = 0;
  top = top_of_world;

  /* perform binary search on world-table */
  for (;;) {
    mid = (bot + top) >> 1;

    if (world[mid].number == virt) {
      return mid;
    }
    if (bot >= top) {
      return -1;
    }
    if (world[mid].number > virt) {
      top = mid - 1;
    }
    else {
      bot = mid + 1;
    }
  }
}

/* returns the real number of the monster with given virtual number */
rnum_t real_mobile(vnum_t virt)
{
  if (virt < 0)
    return -1;

  int bot, top, mid;

  bot = 0;
  top = top_of_mobt;

  /* perform binary search on mob-table */
  for (;;) {
    mid = (bot + top) / 2;

    if ((mob_index + mid)->vnum == virt)
      return (mid);
    if (bot >= top)
      return (-1);
    if ((mob_index + mid)->vnum > virt)
      top = mid - 1;
    else
      bot = mid + 1;
  }
}

rnum_t real_quest(vnum_t virt)
{
  if (virt < 0)
    return -1;

  int bot, top, mid;

  bot = 0;
  top = top_of_questt;

  for (;;) {
    mid = (bot + top) >> 1;

    if ((quest_table + mid)->vnum == virt)
      return mid;
    if (bot >= top)
      return -1;
    if ((quest_table + mid)->vnum > virt)
      top = mid - 1;
    else
      bot = mid + 1;
  }
}

rnum_t real_shop(vnum_t virt)
{
  if (virt < 0)
    return -1;

  int bot, top, mid;

  bot = 0;
  top = top_of_shopt;

  for (;;) {
    mid = (bot + top) / 2;

    if ((shop_table + mid)->vnum == virt)
      return (mid);
    if (bot >= top)
      return (-1);
    if ((shop_table + mid)->vnum > virt)
      top = mid - 1;
    else
      bot = mid + 1;
  }
}

rnum_t real_zone(vnum_t virt)
{
  if (virt < 0)
    return -1;

  int bot, top, mid;

  bot = 0;
  top = top_of_zone_table;

  /* perform binary search on zone-table */
  for (;;) {
    mid = (bot + top) / 2;

    if ((zone_table + mid)->number == virt)
      return (mid);
    if (bot >= top)
      return (-1);
    if ((zone_table + mid)->number > virt)
      top = mid - 1;
    else
      bot = mid + 1;
  }
}

rnum_t real_vehicle(vnum_t virt)
{
  if (virt < 0)
    return -1;

  int bot, top, mid;
  bot = 0;
  top = top_of_veht;
  for (;;) {
    mid = (bot + top) / 2;
    if ((veh_index + mid)->vnum == virt)
      return (mid);
    if (bot >= top)
      return (-1);
    if ((veh_index + mid)->vnum > virt)
      top = mid - 1;
    else
      bot = mid + 1;

  }
}

rnum_t real_host(vnum_t virt)
{
  if (virt < 0)
    return -1;

  int bot, top, mid;
  bot = 0;
  top = top_of_matrix;
  for (;;) {
    mid = (bot + top) / 2;
    if ((matrix + mid)->vnum == virt)
      return (mid);
    if (bot >= top)
      return (-1);
    if ((matrix + mid)->vnum > virt)
      top = mid - 1;
    else
      bot = mid + 1;

  }
}

rnum_t real_ic(vnum_t virt)
{
  if (virt < 0)
    return -1;

  int bot, top, mid;
  bot = 0;
  top = top_of_ic;
  for (;;) {
    mid = (bot + top) / 2;
    if ((ic_index + mid)->vnum == virt)
      return (mid);
    if (bot >= top)
      return (-1);
    if ((ic_index + mid)->vnum > virt)
      top = mid - 1;
    else
      bot = mid + 1;

  }
}

/* returns the real number of the object with given virtual number */
rnum_t real_object(vnum_t virt)
{
  if (virt < 0)
    return -1;

  long bot, top, mid;

  bot = 0;
  top = top_of_objt;

  /* perform binary search on obj-table */
  for (;;) {
    mid = (bot + top) / 2;

    if ((obj_index + mid)->vnum == virt)
      return (mid);
    if (bot >= top)
      return (-1);
    if ((obj_index + mid)->vnum > virt)
      top = mid - 1;
    else
      bot = mid + 1;
  }
}

char *short_object(int virt, int what)
{
  if (virt < 0) {
    static char error[50];
    snprintf(error, sizeof(error), "ERROR");
    mudlog("SYSERR: short_object received negative virt!", NULL, LOG_SYSLOG, TRUE);
    return error;
  }

  int bot, top, mid;

  bot = 0;
  top = top_of_objt;

  /* perform binary search on obj-table */
  for (;;) {
    mid = (bot + top) / 2;

    if ((obj_index + mid)->vnum == virt) {
      /* 1-namelist, 2-shortdescription */
      if (what == 1)
        return (obj_proto[mid].text.keywords);
      else if (what == 2)
        return (obj_proto[mid].text.name);
    }
    if (bot >= top)
      return (NULL);
    if ((obj_index + mid)->vnum > virt)
      top = mid - 1;
    else
      bot = mid + 1;
  }
}

/* remove ^M's from file output */
/* There may be a similar function in Oasis (and I'm sure
   it's part of obuild).  Remove this if you get a
   multiple definition error or if it you want to use a
   substitute
*/
void kill_ems(char *str)
{
  char *ptr1, *ptr2;

  ptr1 = str;
  ptr2 = str;

  while(*ptr1) {
    if((*(ptr2++) = *(ptr1++)) == '\r')
      if(*ptr1 == '\r')
        ptr1++;
  }
  *ptr2 = '\0';
}

/* Look for all vehicles that do not have valid owners. These need to be deleted.
 If they contain vehicles, those vehicles must be disgorged into the veh or room this one is in. */
void purge_unowned_vehs() {
  struct veh_data *prior_veh = NULL, *veh = NULL, *vict_veh = NULL;

  /*
  log("Player-owned vehicles currently in veh list:");
  int counter = 0;
  for (struct veh_data *tmp = veh_list; tmp; tmp = tmp->next) {
    if (tmp->owner != 0) {
      snprintf(buf, sizeof(buf), "'%s' (%ld) owned by %ld", tmp->short_description, tmp->idnum, tmp->owner);
      log(buf);
      counter++;
    }
  }
  snprintf(buf, sizeof(buf), "End of veh list. %d player-owned vehicles counted.", counter);
  log(buf);
  */

  prior_veh = veh_list;
  while (prior_veh) {
    // If we've hit the end of the list, evaluate for the head of the list.
    if (!(veh = prior_veh->next)) {
      veh = veh_list;
      prior_veh = NULL;
    }

    // This vehicle is owned by an NPC (zoneloaded): Do not delete.
    if (veh->owner == 0) {
      // snprintf(buf, sizeof(buf), "Skipping vehicle '%s' (%ld) since it's owned by nobody.", veh->description, veh->idnum);
      // log(buf);

      if (!prior_veh) {
        break;
      } else {
        prior_veh = prior_veh->next;
        continue;
      }
    }

    // This vehicle is owned by a valid player: Do not delete.
    if (does_player_exist(veh->owner) && !player_is_dead_hardcore(veh->owner)) {
      //snprintf(buf, sizeof(buf), "Skipping vehicle '%s' (%ld) since its owner is a valid player.", veh->short_description, veh->idnum);
      //log(buf);

      if (!prior_veh) {
        break;
      } else {
        prior_veh = prior_veh->next;
        continue;
      }
    }

    // This vehicle is owned by an invalid player. Delete.

    // Step 1: Dump its contained vehicles, since those can belong to other players.
    snprintf(buf, sizeof(buf), "Purging contents of vehicle '%s' (%ld), owner %ld (nonexistant).", veh->short_description, veh->idnum, veh->owner);
    log(buf);

    while ((vict_veh = veh->carriedvehs)) {
      snprintf(buf, sizeof(buf), "- Found '%s' (%ld) owned by %ld.", vict_veh->short_description, vict_veh->idnum, vict_veh->owner);

      // If the vehicle is in a room, disgorge there.
      if (veh->in_room) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " Transferring to room %ld.", veh->in_room->number);
        log(buf);
        veh_to_room(vict_veh, veh->in_room);
        continue;
      }

      // If the vehicle is in another vehicle instead, disgorge there.
      else if (veh->in_veh) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " Transferring to vehicle '%s' (%ld).",
                veh->in_veh->short_description, veh->in_veh->idnum);
        log(buf);
        veh_to_veh(vict_veh, veh->in_veh);
        continue;
      }

      // Failure case: Vehicle was in neither a room nor another vehicle.
      else {
        log(buf);
        snprintf(buf, sizeof(buf), "SYSERR: Attempting to disgorge '%s' (%ld) from '%s' (%ld), but the latter has no containing vehicle (%ld) or location (%ld). Putting in Dante's.",
                vict_veh->short_description, vict_veh->idnum,
                veh->short_description, veh->idnum,
                veh->in_veh ? veh->in_veh->idnum : -1,
                veh->in_room ? veh->in_room->number : -1);
        log(buf);
        veh_to_room(vict_veh, &world[real_room(RM_DANTES_GARAGE)]);
        continue;
      }
    }

    // Step 2: Destroy its contents.
    struct obj_data *nextobj;
    for (struct obj_data *obj = veh->contents; obj; obj = nextobj) {
      nextobj = obj->next_content;
      extract_obj(obj);
    }

    // Step 3: Purge the vehicle itself. `veh` now points to garbage.
    extract_veh(veh);

    // Critically, we don't iterate prior_veh if we removed veh-- that would skip the next veh in the list.
  }
}

void load_consist(void)
{
  File file;

  // First, move all storage files that aren't linked to rooms.
  {
    log("Moving orphaned storage files.");
    bf::path storage_directory = bf::path("storage");

    // Ensure our orphaned storage files directory exists if it does not already.
    bf::path orphan_storage_files = storage_directory / "orphaned";
    if (!bf::exists(orphan_storage_files)) {
      bf::create_directory(orphan_storage_files);
    }

    bf::directory_iterator end_itr; // default construction yields past-the-end
    for (bf::directory_iterator itr(storage_directory); itr != end_itr; ++itr) {
      // Skip directories.
      if (is_directory(itr->status()))
        continue;

      // Skip . and .. meta-files.
      if (itr->path().filename_is_dot() || itr->path().filename_is_dot_dot())
        continue;

      // Skip anything that's not a number.
      vnum_t vnum = atol(itr->path().filename().c_str());
      if (vnum <= 0)
        continue;

      // It's a number. Calculate its rnum.
      rnum_t rnum = real_room(vnum);

      // If the room doesn't exist, or if it's not storage-flagged, this is an orphaned file.
      if (rnum < 0 || !ROOM_FLAGGED(&world[rnum], ROOM_STORAGE)) {
        log_vfprintf(" - Marking storage file %s as orphaned: %s.",
                      itr->path().c_str(),
                      rnum < 0 ? "No matching vnum" : "Room not flagged storage");
        bf::rename(itr->path(), orphan_storage_files / itr->path().filename());
      }

      // EVENTUALTODO: Load the file here directly instead of iterating through the whole world as is done in the old code below.
    }
  }

  for (int nr = 0; nr <= top_of_world; nr++) {
    if (ROOM_FLAGGED(&world[nr], ROOM_STORAGE)) {
      snprintf(buf, sizeof(buf), "storage/%ld", world[nr].number);
      if ((file.Open(buf, "r"))) {
        log_vfprintf("Restoring storage contents for room %ld (%s)...", world[nr].number, buf);
        snprintf(buf3, sizeof(buf3), "storage room %ld (%s)", world[nr].number, buf);
        VTable data;
        data.Parse(&file);
        file.Close();

        int house_version = data.GetInt("METADATA/Version", 0);
        struct obj_data *obj = NULL, *last_obj = NULL;
        std::vector<nested_obj> contained_obj;
        struct nested_obj contained_obj_entry;

        int inside = 0, last_inside = 0, num_objs = data.NumSubsections("HOUSE");
        long vnum;
        for (int i = 0; i < num_objs; i++) {
          const char *sect_name = data.GetIndexSection("HOUSE", i);
          snprintf(buf, sizeof(buf), "%s/Vnum", sect_name);
          vnum = data.GetLong(buf, 0);
          if (vnum > 0 && (obj = read_object(vnum, VIRTUAL, OBJ_LOAD_REASON_ROOM_STORAGE_LOAD))) {
            snprintf(buf, sizeof(buf), "%s/Name", sect_name);
            obj->restring = str_dup(data.GetString(buf, NULL));
            snprintf(buf, sizeof(buf), "%s/Graffiti", sect_name);
            obj->graffiti = str_dup(data.GetString(buf, NULL));
            snprintf(buf, sizeof(buf), "%s/Photo", sect_name);
            obj->photo = str_dup(data.GetString(buf, NULL));
            for (int x = 0; x < NUM_OBJ_VALUES; x++) {
              snprintf(buf, sizeof(buf), "%s/Value %d", sect_name, x);
              GET_OBJ_VAL(obj, x) = data.GetInt(buf, GET_OBJ_VAL(obj, x));
            }
            if (GET_OBJ_TYPE(obj) == ITEM_PHONE && GET_OBJ_VAL(obj, 2))
              add_phone_to_list(obj);
            snprintf(buf, sizeof(buf), "%s/Condition", sect_name);
            GET_OBJ_CONDITION(obj) = data.GetInt(buf, GET_OBJ_CONDITION(obj));
            snprintf(buf, sizeof(buf), "%s/Timer", sect_name);
            GET_OBJ_TIMER(obj) = data.GetInt(buf, GET_OBJ_TIMER(obj));
            snprintf(buf, sizeof(buf), "%s/Attempt", sect_name);
            GET_OBJ_ATTEMPT(obj) = data.GetInt(buf, 0);
            snprintf(buf, sizeof(buf), "%s/Cost", sect_name);
            GET_OBJ_COST(obj) = data.GetInt(buf, GET_OBJ_COST(obj));

            // Handle weapon attachments.
            if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
              handle_weapon_attachments(obj);

            else if (GET_OBJ_VNUM(obj) == OBJ_SPECIAL_PC_CORPSE) {
              // Invalid belongings.
              if (GET_OBJ_VAL(obj, 5) <= 0) {
                extract_obj(obj);
                continue;
              }

              const char *player_name = get_player_name(GET_OBJ_VAL(obj, 5));
              if (!player_name || !str_cmp(player_name, "deleted")) {
                // Whoops, it belongs to a deleted character. RIP.
                extract_obj(obj);
                continue;
              }

              // Set up special corpse values. This will probably cause a memory leak. We use name instead of desc.
              snprintf(buf, sizeof(buf), "belongings %s", player_name);
              snprintf(buf1, sizeof(buf1), "^rThe belongings of %s are lying here.^n", decapitalize_a_an(player_name));
              snprintf(buf2, sizeof(buf2), "^rthe belongings of %s^n", player_name);
              strlcpy(buf3, "Looks like the DocWagon trauma team wasn't able to bring this stuff along.\r\n", sizeof(buf3));
              obj->text.keywords = str_dup(buf);
              obj->text.room_desc = str_dup(buf1);
              obj->text.name = str_dup(buf2);
              obj->text.look_desc = str_dup(buf3);

              GET_OBJ_VAL(obj, 4) = 1;
              GET_OBJ_BARRIER(obj) = PC_CORPSE_BARRIER;
              GET_OBJ_CONDITION(obj) = 100;

              delete [] player_name;
            }

            snprintf(buf, sizeof(buf), "%s/%s", sect_name, FILESTRING_OBJ_IDNUM);
            GET_OBJ_IDNUM(obj) = data.GetUnsignedLong(buf, 0);
            ENSURE_OBJ_HAS_IDNUM(obj);

            // Don't auto-repair cyberdecks until they're fully loaded.
            if (GET_OBJ_TYPE(obj) != ITEM_CYBERDECK)
              auto_repair_obj(obj, 0);

            snprintf(buf, sizeof(buf), "%s/Inside", sect_name);
            inside = data.GetInt(buf, 0);
            if (house_version == VERSION_HOUSE_FILE) {
              // Since we're now saved the obj linked lists  in reverse order, in order to fix the stupid reordering on
              // every binary execution, the previous algorithm did not work, as it relied on getting the container obj
              // first and place subsequent objects in it. Since we're now getting it last, and more importantly we get
              // objects at deeper nesting level in between our list, we save all pointers to objects along with their
              // nesting level in a vector container and once we found the next container (inside < last_inside) we
              // move the objects with higher nesting level to container and proceed. This works for any nesting depth.
              if (inside > 0 || (inside == 0 && inside < last_inside)) {
                //Found our container?
                if (inside < last_inside) {
                  if (inside == 0)
                    obj_to_room(obj, &world[nr]);

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
              }
              else
                obj_to_room(obj, &world[nr]);

              last_inside = inside;
            }
            // This handles loading old house file format prior to introduction of version number in the file.
            // Version number will always be 0 for this format.
            else if (!house_version) {
              if (inside > 0) {
                if (inside == last_inside)
                  last_obj = last_obj->in_obj;
                else if (inside < last_inside) {
                  while (inside <= last_inside) {
                    if (!last_obj) {
                      snprintf(buf2, sizeof(buf2), "Load error: Nested-item save failed for %s. Disgorging to room.", GET_OBJ_NAME(obj));
                      mudlog(buf2, NULL, LOG_SYSLOG, TRUE);
                      break;
                    }
                    last_obj = last_obj->in_obj;
                    last_inside--;
                  }
                }

                if (last_obj)
                  obj_to_obj(obj, last_obj);
                else
                  obj_to_room(obj, &world[nr]);
              } else
                obj_to_room(obj, &world[nr]);

              last_inside = inside;
              last_obj = obj;
            }
            else {
              snprintf(buf2, sizeof(buf2), "Load ERROR: Unknown file format for storage Rnum %d. Dumping valid objects to room.", nr);
              mudlog(buf2, NULL, LOG_SYSLOG, TRUE);
              obj_to_room(obj, &world[nr]);
            }
          } else {
            snprintf(buf2, sizeof(buf2), "Losing object %ld (%s / %s; )- it's not a valid object.",
                     vnum,
                     data.GetString("HOUSE/Name", "no restring"),
                     data.GetString("HOUSE/Photo", "no photo"));
            mudlog(buf2, NULL, LOG_SYSLOG, TRUE);
          }
        }
        if (house_version == VERSION_HOUSE_FILE) {
          // Failsafe. If something went wrong and we still have objects stored in the vector, dump them in the room.
          if (!contained_obj.empty()) {
            for (auto it : contained_obj)
              obj_to_room(it.obj, &world[nr]);

            contained_obj.clear();
          }
        }
      }
    }

    // Verify that any hack-flagged room has a PC corpse in it. If not, remove the flag and save it.
    if (ROOM_FLAGGED(&world[nr], ROOM_CORPSE_SAVE_HACK)) {
      bool keep_flag = FALSE;
      for (struct obj_data *tmp_obj = world[nr].contents; tmp_obj; tmp_obj = tmp_obj->next_content)
        if ((keep_flag = (GET_OBJ_VNUM(tmp_obj) == OBJ_SPECIAL_PC_CORPSE)))
          break;

      if (!keep_flag) {
        world[nr].room_flags.RemoveBit(ROOM_CORPSE_SAVE_HACK);
        world[nr].room_flags.RemoveBit(ROOM_STORAGE);

        for (int counter = 0; counter <= top_of_zone_table; counter++) {
          if ((GET_ROOM_VNUM(&world[nr]) >= (zone_table[counter].number * 100))
              && (GET_ROOM_VNUM(&world[nr]) <= (zone_table[counter].top)))
          {
            write_world_to_disk(zone_table[counter].number);
            break;
          }
        }
      }
    }
  }

  if (!(file.Open("etc/consist", "r"))) {
    log("PAYDATA CONSISTENCY FILE NOT FOUND");
    return;
  }

  VTable paydata;
  paydata.Parse(&file);
  file.Close();
  market[0] = paydata.GetInt("MARKET/Blue", 5000);
  market[1] = paydata.GetInt("MARKET/Green", 5000);
  market[2] = paydata.GetInt("MARKET/Orange", 5000);
  market[3] = paydata.GetInt("MARKET/Red", 5000);
  market[4] = paydata.GetInt("MARKET/Black", 5000);
}

void boot_shop_orders(void)
{
  File file;
  vnum_t vnum, player;
  for (int i = 0; i <= top_of_shopt; i++) {
    snprintf(buf, sizeof(buf), "order/%ld", shop_table[i].vnum);
    if (!(file.Open(buf, "r")))
      continue;
    VTable data;
    data.Parse(&file);
    file.Close();
    int num_ord = data.NumSubsections("ORDERS");
    struct shop_order_data *order, *list = NULL;

    for (int x = 0; x < num_ord; x++) {
      const char *sect_name = data.GetIndexSection("ORDERS", x);
      snprintf(buf, sizeof(buf), "%s/Item", sect_name);
      vnum = data.GetLong(buf, 0);
      snprintf(buf, sizeof(buf), "%s/Player", sect_name);
      player = data.GetLong(buf, 0);
      if (real_object(vnum) < 0 || !does_player_exist(player))
        continue;
      order = new shop_order_data;
      order->item = vnum;
      order->player = player;
      snprintf(buf, sizeof(buf), "%s/Time", sect_name);
      order->timeavail = data.GetInt(buf, 0);
      snprintf(buf, sizeof(buf), "%s/Price", sect_name);
      order->price = data.GetInt(buf, 0);
      snprintf(buf, sizeof(buf), "%s/Number", sect_name);
      order->number = data.GetInt(buf, 0);
      snprintf(buf, sizeof(buf), "%s/Sent", sect_name);
      order->sent = data.GetInt(buf, 0);
      snprintf(buf, sizeof(buf), "%s/Paid", sect_name);
      order->paid = data.GetInt(buf, 0);
      snprintf(buf, sizeof(buf), "%s/Expiration", sect_name);
      order->expiration = data.GetLong(buf, MAX(time(0), order->timeavail) + (60 * 60 * 24 * PREORDERS_ARE_GOOD_FOR_X_DAYS));

      if (order->item && order->player && order->timeavail && order->price && order->number) {
        order->next = list;
        list = order;
      }
    }
    shop_table[i].order = list;
  }
}

#define MAKE_INCOMPATIBLE_WITH_MAGIC(obj) GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_MAGIC_INCOMPATIBLE);
void price_cyber(struct obj_data *obj)
{
  bool has_strength = FALSE, has_qui = FALSE;

  switch (GET_CYBERWARE_TYPE(obj)) {
    case CYB_CUSTOM_NERPS:
      // Do absolutely nothing with it.
      return;
    case CYB_CHIPJACK:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_OBJ_COST(obj) = 1000 * GET_CYBERWARE_FLAGS(obj);
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 15 + (GET_CYBERWARE_FLAGS(obj) * 5);
      GET_OBJ_AVAILTN(obj) = 3;
      GET_OBJ_AVAILDAY(obj) = 3;
      break;
    case CYB_DATAJACK:
      GET_OBJ_VAL(obj, 1) = 0;
      if (GET_CYBERWARE_FLAGS(obj) == DATA_STANDARD) {
        GET_OBJ_COST(obj) = 1000;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 20;
        GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 0;
      } else {
        GET_OBJ_COST(obj) = 3000;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 30;
        GET_OBJ_AVAILTN(obj) = 5;
        GET_OBJ_AVAILDAY(obj) = 4;
      }
      break;
    case CYB_DATALOCK:
    GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 20;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 1.5;
      GET_OBJ_COST(obj) = 1000;
      switch (GET_OBJ_VAL(obj, 1)) {
        case 1:
        case 2:
        case 3:
          GET_OBJ_COST(obj) += GET_OBJ_VAL(obj, 1) * GET_OBJ_VAL(obj, 1) * 50;
          break;
        case 4:
        case 5:
        case 6:
          GET_OBJ_COST(obj) += GET_OBJ_VAL(obj, 1) * GET_OBJ_VAL(obj, 1) * 100;
          break;
        case 7:
        case 8:
        case 9:
          GET_OBJ_COST(obj) += GET_OBJ_VAL(obj, 1) * GET_OBJ_VAL(obj, 1) * 250;
          break;
        default:
          GET_OBJ_COST(obj) += GET_OBJ_VAL(obj, 1) * GET_OBJ_VAL(obj, 1) * 500;
          break;
      }
      break;
     case CYB_KNOWSOFTLINK:
       GET_OBJ_VAL(obj, 1) = 0;
       GET_OBJ_COST(obj) = 1000;
       GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 10;
       GET_OBJ_AVAILTN(obj) = 3;
       GET_OBJ_AVAILDAY(obj) = 1;
       break;
     case CYB_MEMORY:
       GET_OBJ_VAL(obj, 1) = 0;
       GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 3) * 150;
       GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = GET_OBJ_VAL(obj, 3) / 3;
       GET_OBJ_AVAILTN(obj) = 3;
       GET_OBJ_AVAILDAY(obj) = 1;
       break;
     case CYB_TOOTHCOMPARTMENT:
       GET_OBJ_VAL(obj, 1) = 0;
       GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 0;
       GET_OBJ_AVAILDAY(obj) = 2;
       if (!GET_CYBERWARE_FLAGS(obj)) {
         GET_OBJ_COST(obj) = 1500;
         GET_OBJ_AVAILTN(obj) = 2;
       } else {
         GET_OBJ_COST(obj) = 750;
         GET_OBJ_AVAILTN(obj) = 3;
       }
       break;
     case CYB_PHONE:
       GET_OBJ_VAL(obj, 1) = 0;
       GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 50;
       GET_OBJ_COST(obj) = 3700;
       GET_OBJ_AVAILTN(obj) = 3;
       GET_OBJ_AVAILDAY(obj) = 1;
       break;
     case CYB_RADIO:
     GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 75;
       GET_OBJ_COST(obj) = 2000 * GET_OBJ_VAL(obj, 1);
       GET_OBJ_AVAILTN(obj) = 2;
       GET_OBJ_AVAILDAY(obj) = 1;
       break;
     case CYB_BONELACING:
       switch (GET_CYBERWARE_FLAGS(obj)) {
         case BONE_PLASTIC:
           GET_OBJ_VAL(obj, 1) = 0;
           GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 50;
           GET_OBJ_COST(obj) = 7500;
           GET_OBJ_AVAILTN(obj) = 5;
           GET_OBJ_AVAILDAY(obj) = 14;
           break;
         case BONE_ALUMINIUM:
           GET_OBJ_VAL(obj, 1) = 0;
           GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 115;
           GET_OBJ_COST(obj) = 25000;
           GET_OBJ_AVAILTN(obj) = 5;
           GET_OBJ_AVAILDAY(obj) = 14;
           break;
         case BONE_TITANIUM:
           GET_OBJ_VAL(obj, 1) = 0;
           GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 225;
           GET_OBJ_COST(obj) = 75000;
           GET_OBJ_AVAILTN(obj) = 5;
           GET_OBJ_AVAILDAY(obj) = 14;
           break;
         case BONE_KEVLAR:
           GET_OBJ_VAL(obj, 1) = 0;
           GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 100;
           GET_OBJ_COST(obj) = 20000;
           GET_OBJ_AVAILTN(obj) = 6;
           GET_OBJ_AVAILDAY(obj) = 21;
           break;
         case BONE_CERAMIC:
           GET_OBJ_VAL(obj, 1) = 0;
           GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 150;
           GET_OBJ_COST(obj) = 40000;
           GET_OBJ_AVAILTN(obj) = 6;
           GET_OBJ_AVAILDAY(obj) = 21;
           break;
       }
       break;
    case CYB_FINGERTIP:
      GET_CYBERWARE_RATING(obj) = 0;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 10;
      GET_OBJ_COST(obj) = 3000;
      GET_OBJ_AVAILTN(obj) = 3;
      GET_OBJ_AVAILDAY(obj) = 1;
      break;
    case CYB_HANDBLADE:
      GET_CYBERWARE_RATING(obj) = 0;
      if (GET_CYBERWARE_FLAGS(obj)) {
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 25;
        GET_OBJ_COST(obj) = 10000;
      } else {
        GET_OBJ_COST(obj) = 7500;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 10;
      }
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 5;
      break;
    case CYB_HANDRAZOR:
      GET_CYBERWARE_RATING(obj) = 0;
      GET_OBJ_AVAILDAY(obj) = 3;
      if (IS_SET(GET_CYBERWARE_FLAGS(obj), 1 << CYBERWEAPON_RETRACTABLE)) {
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 20;
        GET_OBJ_COST(obj) = 9000;
        GET_OBJ_AVAILTN(obj) = 5;
      } else {
        GET_OBJ_COST(obj) = 7500;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 10;
        GET_OBJ_AVAILTN(obj) = 3;
      }
      if (IS_SET(GET_CYBERWARE_FLAGS(obj), 1 << CYBERWEAPON_IMPROVED)) {
        GET_OBJ_COST(obj) += 8500;
        GET_OBJ_AVAILTN(obj) = 6;
      }
      break;
    case CYB_HANDSPUR:
      GET_CYBERWARE_RATING(obj) = 0;
      if (GET_CYBERWARE_FLAGS(obj)) {
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 30;
        GET_OBJ_COST(obj) = 11500;
        GET_OBJ_AVAILTN(obj) = 5;
      } else {
        GET_OBJ_COST(obj) = 7000;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 10;
        GET_OBJ_AVAILTN(obj) = 3;
      }
      GET_OBJ_AVAILDAY(obj) = 3;
      break;
    case CYB_VOICEMOD:
      GET_CYBERWARE_RATING(obj) = 0;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 20;
      GET_OBJ_COST(obj) = 45000;
      GET_OBJ_AVAILTN(obj) = 2;
      GET_OBJ_AVAILDAY(obj) = 1;
      break;
    case CYB_BOOSTEDREFLEXES:
      GET_OBJ_AVAILTN(obj) = 3;
      GET_OBJ_AVAILDAY(obj) = 1;
      switch (GET_CYBERWARE_RATING(obj)) {
        case 1:
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 50;
          GET_OBJ_COST(obj) = 15000;
          break;
        case 2:
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 125;
          GET_OBJ_COST(obj) = 40000;
          break;
        case 3:
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 280;
          GET_OBJ_COST(obj) = 90000;
          break;
      }
      break;
    case CYB_DERMALPLATING:
    GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 50 * GET_CYBERWARE_RATING(obj);
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 12;
      switch (GET_CYBERWARE_RATING(obj)) {
        case 1:
          GET_OBJ_COST(obj) = 6000;
          break;
        case 2:
          GET_OBJ_COST(obj) = 15000;
          break;
        case 3:
          GET_OBJ_COST(obj) = 45000;
          break;
      }
      break;
    case CYB_FILTRATION:
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 4;
      if (!GET_CYBERWARE_FLAGS(obj)) {
        GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) * 15000;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = GET_OBJ_VAL(obj, 1) * 10;
      } else {
        GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) * 10000;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = GET_OBJ_VAL(obj, 1) * 20;
      }
      break;
    case CYB_VCR:
      GET_OBJ_AVAILDAY(obj) = 2;
      if (GET_CYBERWARE_RATING(obj) == 1) {
        GET_OBJ_COST(obj) = 12000;
        GET_OBJ_AVAILTN(obj) = 6;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 200;
      } else {
        GET_OBJ_AVAILTN(obj) = 8;
        if (GET_CYBERWARE_RATING(obj) == 2) {
          GET_OBJ_COST(obj) = 60000;
          GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 300;
        } else {
          GET_OBJ_COST(obj) = 300000;
          GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 500;
        }
      }
      break;
    case CYB_WIREDREFLEXES:
      if (GET_CYBERWARE_RATING(obj) == 3) {
        GET_OBJ_COST(obj) = 500000;
        GET_OBJ_AVAILTN(obj) = 8;
        GET_OBJ_AVAILDAY(obj) = 14;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 500;
      } else {
        GET_OBJ_AVAILTN(obj) = 4;
        GET_OBJ_AVAILDAY(obj) = 8;
        if (GET_CYBERWARE_RATING(obj) == 1) {
          GET_OBJ_COST(obj) = 55000;
          GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 200;
        } else {
          GET_OBJ_COST(obj) = 165000;
          GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 300;
        }
      }
      break;
    case CYB_REACTIONENHANCE:
      GET_OBJ_COST(obj) = GET_CYBERWARE_RATING(obj) * 60000;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 30 * GET_CYBERWARE_RATING(obj);
      GET_OBJ_AVAILDAY(obj) = 7;
      GET_OBJ_AVAILTN(obj) = 6;
      break;
    case CYB_REFLEXTRIGGER:
      GET_OBJ_COST(obj) = 13000;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 20;
      GET_OBJ_AVAILDAY(obj) = 8;
      GET_OBJ_AVAILTN(obj) = 4;
      if (GET_CYBERWARE_FLAGS(obj))
        GET_OBJ_COST(obj) = (int)(GET_OBJ_COST(obj) * 1.25);
      break;
    case CYB_BALANCEAUG:
      GET_CYBERWARE_RATING(obj) = 0;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 40;
      GET_OBJ_COST(obj) = 14000;
      GET_OBJ_AVAILTN(obj) = 8;
      GET_OBJ_AVAILDAY(obj) = 16;
      break;
    case CYB_ORALDART:
      GET_CYBERWARE_RATING(obj) = 0;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 25;
      GET_OBJ_COST(obj) = 3600;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 7;
      break;
    case CYB_ORALGUN:
      GET_CYBERWARE_RATING(obj) = 0;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 40;
      GET_OBJ_COST(obj) = 5600;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 3;
      break;
    case CYB_ORALSLASHER:
      GET_CYBERWARE_RATING(obj) = 0;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 25;
      GET_OBJ_COST(obj) = 10050;
      GET_OBJ_AVAILTN(obj) = 8;
      GET_OBJ_AVAILDAY(obj) = 7;
      break;
    case CYB_ASIST:
      GET_CYBERWARE_RATING(obj) = 0;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 0;
      GET_OBJ_COST(obj) = 1000;
      GET_OBJ_AVAILTN(obj) = 3;
      GET_OBJ_AVAILDAY(obj) = 1.5;
      break;
    case CYB_CHIPJACKEXPERT:
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 2;

      if (GET_CYBERWARE_FLAGS(obj) == 0) {
        // All experts are 1-slot by default unless otherwise set.
        GET_CYBERWARE_FLAGS(obj) = 1;
      }

      // Essence and nuyen costs are multiplied by the slots.
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 10 * GET_CYBERWARE_RATING(obj) * GET_CYBERWARE_FLAGS(obj);
      GET_OBJ_COST(obj) = 5000 * GET_CYBERWARE_RATING(obj) * GET_CYBERWARE_FLAGS(obj);
      break;
    case CYB_DATACOMPACT:
    GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 5 + (GET_CYBERWARE_RATING(obj) * 5);
      GET_OBJ_COST(obj) = 9500 * GET_CYBERWARE_RATING(obj);
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 2.5;
      break;
    case CYB_ENCEPHALON:
    GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 75 * GET_CYBERWARE_RATING(obj);
      GET_OBJ_COST(obj) = GET_CYBERWARE_RATING(obj) > 1 ? 115000 : 40000;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 12;
      break;
    case CYB_MATHSSPU:
    GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 5 + (GET_CYBERWARE_RATING(obj) * 5);
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 2.5;
      switch (GET_CYBERWARE_RATING(obj)) {
        case 1:
          GET_OBJ_COST(obj) = 2000;
          break;
        case 2:
          GET_OBJ_COST(obj) = 5000;
          break;
        case 3:
          GET_OBJ_COST(obj) = 11000;
          break;
      }
      break;
    case CYB_BALANCETAIL:
      GET_CYBERWARE_RATING(obj) = 0;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 50;
      GET_OBJ_COST(obj) = 10000;
      GET_OBJ_AVAILTN(obj) = 8;
      GET_OBJ_AVAILDAY(obj) = 7;
      break;
    case CYB_BODYCOMPART:
      GET_CYBERWARE_RATING(obj) = 0;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 20;
      GET_OBJ_COST(obj) = 5000;
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 2;
      break;
    case CYB_FIN:
      GET_CYBERWARE_RATING(obj) = 0;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 30;
      GET_OBJ_COST(obj) = 10500;
      GET_OBJ_AVAILTN(obj) = 5;
      GET_OBJ_AVAILDAY(obj) = 2;
      break;
    case CYB_SKULL:
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 4;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 75;
      MAKE_INCOMPATIBLE_WITH_MAGIC(obj);

      if (IS_SET(GET_CYBERWARE_FLAGS(obj), SKULL_MOD_OBVIOUS)) {
        GET_OBJ_COST(obj) = 35000;
      } else if (IS_SET(GET_CYBERWARE_FLAGS(obj), SKULL_MOD_SYNTHETIC)) {
        GET_OBJ_COST(obj) = 55000;
      } else {
        log_vfprintf("ERROR: %s is neither obvious nor synthetic!", GET_OBJ_NAME(obj));
        GET_OBJ_COST(obj) = 99999999;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 999;
      }

      if (IS_SET(GET_CYBERWARE_FLAGS(obj), SKULL_MOD_ARMOR_MOD1)) {
        GET_OBJ_COST(obj) += 6500;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 14);
      }

      if (IS_SET(GET_CYBERWARE_FLAGS(obj), SKULL_MOD_TAC_COMP)) {
        GET_OBJ_COST(obj) += 400000 + (GET_CYBERWARE_RATING(obj) * 20000);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 20 + (GET_CYBERWARE_RATING(obj) * 20);
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 12);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 60);
      }
      break;
    case CYB_TORSO:
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 4;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 150;
      MAKE_INCOMPATIBLE_WITH_MAGIC(obj);

      if (IS_SET(GET_CYBERWARE_FLAGS(obj), TORSO_MOD_OBVIOUS)) {
        GET_OBJ_COST(obj) = 90000;
      } else if (IS_SET(GET_CYBERWARE_FLAGS(obj), TORSO_MOD_SYNTHETIC)) {
        GET_OBJ_COST(obj) = 120000;
      } else {
        log_vfprintf("ERROR: %s is neither obvious nor synthetic!", GET_OBJ_NAME(obj));
        GET_OBJ_COST(obj) = 99999999;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 999;
      }

      if (IS_SET(GET_CYBERWARE_FLAGS(obj), TORSO_MOD_ARMOR_MOD1)) {
        GET_OBJ_COST(obj) += 6500;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 14);
      }
      else if (IS_SET(GET_CYBERWARE_FLAGS(obj), TORSO_MOD_ARMOR_MOD2)) {
        GET_OBJ_COST(obj) += 13000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 14);
      }
      else if (IS_SET(GET_CYBERWARE_FLAGS(obj), TORSO_MOD_ARMOR_MOD3)) {
        GET_OBJ_COST(obj) += 19500;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 14);
      }
      break;
    case CYB_LEGS:
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 4;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 200;
      MAKE_INCOMPATIBLE_WITH_MAGIC(obj);

      if (IS_SET(GET_CYBERWARE_FLAGS(obj), LEGS_MOD_OBVIOUS)) {
        GET_OBJ_COST(obj) = 150000;
      } else if (IS_SET(GET_CYBERWARE_FLAGS(obj), LEGS_MOD_SYNTHETIC)) {
        GET_OBJ_COST(obj) = 200000;
      } else {
        log_vfprintf("ERROR: %s is neither obvious nor synthetic!", GET_OBJ_NAME(obj));
        GET_OBJ_COST(obj) = 99999999;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 999;
      }

      if (IS_SET(GET_CYBERWARE_FLAGS(obj), LEGS_MOD_ARMOR_MOD1)) {
        GET_OBJ_COST(obj) += 6500;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 14);
      }

      if (IS_SET(GET_CYBERWARE_FLAGS(obj), LEGS_MOD_STRENGTH_MOD1)) {
        GET_OBJ_COST(obj) += 20000;
        has_strength = TRUE;
      }
      else if (IS_SET(GET_CYBERWARE_FLAGS(obj), LEGS_MOD_STRENGTH_MOD2)) {
        GET_OBJ_COST(obj) += 40000;
        has_strength = TRUE;
      }
      else if (IS_SET(GET_CYBERWARE_FLAGS(obj), LEGS_MOD_STRENGTH_MOD3)) {
        GET_OBJ_COST(obj) += 60000;
        has_strength = TRUE;
      }

      if (IS_SET(GET_CYBERWARE_FLAGS(obj), LEGS_MOD_QUICKNESS_MOD1)) {
        GET_OBJ_COST(obj) += 30000 * (has_strength ? 1.1 : 1);
        has_qui = TRUE;
      }
      else if (IS_SET(GET_CYBERWARE_FLAGS(obj), LEGS_MOD_QUICKNESS_MOD2)) {
        GET_OBJ_COST(obj) += 60000 * (has_strength ? 1.1 : 1);
        has_qui = TRUE;
      }
      else if (IS_SET(GET_CYBERWARE_FLAGS(obj), LEGS_MOD_QUICKNESS_MOD3)) {
        GET_OBJ_COST(obj) += 90000 * (has_strength ? 1.1 : 1);
        has_qui = TRUE;
      }

      if (has_strength || has_qui) {
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 28);
      }
      break;
    case CYB_ARMS:
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = GET_OBJ_COST(obj) = 0;

      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 4;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 200;
      MAKE_INCOMPATIBLE_WITH_MAGIC(obj);

      if (IS_SET(GET_CYBERWARE_FLAGS(obj), ARMS_MOD_OBVIOUS)) {
        GET_OBJ_COST(obj) = 75000;
      } else if (IS_SET(GET_CYBERWARE_FLAGS(obj), ARMS_MOD_SYNTHETIC)) {
        GET_OBJ_COST(obj) = 100000;
      } else {
        log_vfprintf("ERROR: %s is neither obvious nor synthetic!", GET_OBJ_NAME(obj));
        GET_OBJ_COST(obj) = 99999999;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 999;
      }

      if (IS_SET(GET_CYBERWARE_FLAGS(obj), ARMS_MOD_ARMOR_MOD1)) {
        GET_OBJ_COST(obj) += 6500;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 14);
      }

      if (IS_SET(GET_CYBERWARE_FLAGS(obj), ARMS_MOD_STRENGTH_MOD1)) {
        GET_OBJ_COST(obj) += 20000;
        has_strength = TRUE;
      } else if (IS_SET(GET_CYBERWARE_FLAGS(obj), ARMS_MOD_STRENGTH_MOD2)) {
        GET_OBJ_COST(obj) += 40000;
        has_strength = TRUE;
      } else if (IS_SET(GET_CYBERWARE_FLAGS(obj), ARMS_MOD_STRENGTH_MOD3)) {
        GET_OBJ_COST(obj) += 60000;
        has_strength = TRUE;
      }

      if (IS_SET(GET_CYBERWARE_FLAGS(obj), ARMS_MOD_QUICKNESS_MOD1)) {
        GET_OBJ_COST(obj) += 30000 * (has_strength ? 1.1 : 1);
        has_qui = TRUE;
      } else if (IS_SET(GET_CYBERWARE_FLAGS(obj), ARMS_MOD_QUICKNESS_MOD2)) {
        GET_OBJ_COST(obj) += 60000 * (has_strength ? 1.1 : 1);
        has_qui = TRUE;
      } else if (IS_SET(GET_CYBERWARE_FLAGS(obj), ARMS_MOD_QUICKNESS_MOD3)) {
        GET_OBJ_COST(obj) += 90000 * (has_strength ? 1.1 : 1);
        has_qui = TRUE;
      }

      if (has_strength || has_qui) {
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 28);
      }

      if (IS_SET(GET_CYBERWARE_FLAGS(obj), ARMS_MOD_GYROMOUNT)) {
        GET_OBJ_COST(obj) += 40000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 10);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 21);
      }
      break;
    case CYB_DERMALSHEATHING:
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 70 * GET_CYBERWARE_RATING(obj);
      GET_OBJ_AVAILDAY(obj) = 14;
      if (GET_CYBERWARE_RATING(obj) == 3) {
        GET_OBJ_AVAILTN(obj) = 8;
        GET_OBJ_COST(obj) = 120000;
      } else {
        GET_OBJ_AVAILTN(obj) = 6;
        if (GET_CYBERWARE_RATING(obj) == 1)
          GET_OBJ_COST(obj) = 24000;
        else GET_OBJ_COST(obj) = 60000;
      }
      if (GET_CYBERWARE_FLAGS(obj)) {
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 20;
        GET_OBJ_COST(obj) += 150000;
        GET_OBJ_AVAILTN(obj) += 2;
        GET_OBJ_AVAILDAY(obj) += 4;
      }
      break;
    case CYB_FOOTANCHOR:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 40;
      GET_OBJ_COST(obj) = 14000;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 7;
      break;
    case CYB_HYDRAULICJACK:
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 75 + (25 * GET_CYBERWARE_RATING(obj));
      GET_OBJ_COST(obj) = GET_CYBERWARE_RATING(obj) * 5000;
      GET_OBJ_AVAILTN(obj) = 5;
      GET_OBJ_AVAILDAY(obj) = 6;
      break;
    case CYB_SKILLWIRE:
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 20 * GET_CYBERWARE_RATING(obj);
      GET_OBJ_COST(obj) = GET_CYBERWARE_RATING(obj) * GET_OBJ_VAL(obj, 3) * 500;
      GET_OBJ_AVAILTN(obj) = GET_CYBERWARE_RATING(obj);
      GET_OBJ_AVAILDAY(obj) = 10;
      break;
    case CYB_MOVEBYWIRE:
      GET_OBJ_COST(obj) = 125000 * (int)pow((double)2, (double)GET_CYBERWARE_RATING(obj));
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 100 + (150 * GET_CYBERWARE_RATING(obj));
      switch (GET_CYBERWARE_RATING(obj)) {
        case 1:
          GET_OBJ_AVAILTN(obj) = 8;
          GET_OBJ_AVAILDAY(obj) = 10;
          break;
        case 2:
          GET_OBJ_AVAILTN(obj) = 12;
          GET_OBJ_AVAILDAY(obj) = 20;
          break;
        case 3:
          GET_OBJ_AVAILTN(obj) = 18;
          GET_OBJ_AVAILDAY(obj) = 30;
          break;
        case 4:
          GET_OBJ_AVAILTN(obj) = 20;
          GET_OBJ_AVAILDAY(obj) = 45;
          break;
      }
      break;
    case CYB_CLIMBINGCLAWS:
      GET_CYBERWARE_RATING(obj) = 0;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 30;
      GET_OBJ_COST(obj) = 10000;
      GET_OBJ_AVAILTN(obj) = 5;
      GET_OBJ_AVAILDAY(obj) = 3;
      break;
    case CYB_SMARTLINK:
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 50;
      switch (GET_CYBERWARE_RATING(obj)) {
        case 1:
          GET_OBJ_COST(obj) = 2500;
          GET_OBJ_AVAILTN(obj) = 3;
          GET_OBJ_AVAILDAY(obj) = 1.5;
          break;
        case 2:
          GET_OBJ_COST(obj) = 3500;
          GET_OBJ_AVAILTN(obj) = 6;
          GET_OBJ_AVAILDAY(obj) = 2;
          break;
      }
      break;
    case CYB_MUSCLEREP:
      GET_OBJ_COST(obj) = GET_CYBERWARE_RATING(obj) * 20000;
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = GET_CYBERWARE_RATING(obj) * 100;
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 4;
      break;
    case CYB_EYES:
      // Auto-remove the vision affs from this object.
      obj->obj_flags.bitvector.RemoveBits(AFF_LOW_LIGHT, AFF_INFRAVISION, AFF_ULTRASOUND, ENDBIT);

      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = GET_OBJ_COST(obj) = 0;
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_CAMERA)) {
        GET_OBJ_COST(obj) = 5000;
        GET_OBJ_AVAILTN(obj) = 6;
        GET_OBJ_AVAILDAY(obj) = 1;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 40;
      }

      // These are redundant items, so if both are set we take the better.
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_IMAGELINK) || IS_SET(GET_OBJ_VAL(obj, 3), EYE_DISPLAYLINK)){
        if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_IMAGELINK)) {
          GET_OBJ_COST(obj) += 1600;
          GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 4);
          GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
          GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 20;
        }
        else if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_DISPLAYLINK)) {
          GET_OBJ_COST(obj) += 1000;
          GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 4);
          GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 1.5);
          GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 10;
        }
      }

      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_FLARECOMP)) {
        GET_OBJ_COST(obj) += 2000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 5);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 10;
      }

      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_LOWLIGHT)) {
        GET_OBJ_COST(obj) += 3000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 4);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 1.5);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 20;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_PROTECTIVECOVERS)) {
        GET_OBJ_COST(obj) += 500;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 4);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_CLOCK)) {
        GET_OBJ_COST(obj) += 450;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 3);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 1);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 10;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_RETINALDUPLICATION)) {
        GET_OBJ_COST(obj) += GET_OBJ_VAL(obj, 1) * 25000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 7);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 10;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_THERMO)) {
        GET_OBJ_COST(obj) += 3000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 4);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 1.5);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 20;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_OPTMAG1)) {
        GET_OBJ_COST(obj) += 2500;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 4);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 20;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_OPTMAG2)) {
        GET_OBJ_COST(obj) += 4000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 4);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 20;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_OPTMAG3)) {
        GET_OBJ_COST(obj) += 6000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 5);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 20;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_ELECMAG1)) {
        GET_OBJ_COST(obj) += 3500;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 5);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 10;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_ELECMAG2)) {
        GET_OBJ_COST(obj) += 7500;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 5);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 10;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_ELECMAG3)) {
        GET_OBJ_COST(obj) += 11000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 10;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_GUN)) {
        GET_OBJ_COST(obj) += 6400;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 6);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 1);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 40;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_DATAJACK)) {
        GET_OBJ_COST(obj) += 2200;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 6);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 25;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_DART)) {
        GET_OBJ_COST(obj) += 4200;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 14);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 25;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_LASER)) {
        switch (GET_OBJ_VAL(obj, 1)) {
          case 1:
            GET_OBJ_COST(obj) += 3000;
            GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
            GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 3);
            GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 20;
            break;
          case 2:
            GET_OBJ_COST(obj) += 5000;
            GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
            GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 7);
            GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 30;
            break;
          case 3:
            GET_OBJ_COST(obj) += 8000;
            GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
            GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 14);
            GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 50;
            break;
        }
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_LIGHT)) {
        GET_OBJ_COST(obj) += 20;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 4);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 3);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 20;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_ULTRASOUND)) {
        GET_OBJ_COST(obj) += 10000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 6);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 50;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_CYBEREYES)) {
        GET_OBJ_COST(obj) += 4000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 2);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 1);
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) += 20;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_COSMETIC)) {
        GET_OBJ_COST(obj) += 1000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 2);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 1);
      }
      break;
    case CYB_TACTICALCOMPUTER:
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 130 + (GET_OBJ_VAL(obj, 1) * 20);
      GET_OBJ_AVAILTN(obj) = 12;
      GET_OBJ_AVAILDAY(obj) = 60;
#ifdef DIES_IRAE
      // Houserule: Tactical computers are mundane-only items.
      GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_MAGIC_INCOMPATIBLE);
#endif
      switch (GET_CYBERWARE_RATING(obj)) {
        case 1:
          GET_OBJ_COST(obj) = 400000;
          break;
        case 2:
          GET_OBJ_COST(obj) = 420000;
          break;
        case 3:
          GET_OBJ_COST(obj) = 440000;
          break;
        case 4:
          GET_OBJ_COST(obj) = 460000;
          break;
      }
      break;
    case CYB_CRD:
      GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 30;
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 3;
      switch (GET_CYBERWARE_RATING(obj)) {
        case 1:
          GET_OBJ_COST(obj) = 25000;
          break;
        case 2:
          GET_OBJ_COST(obj) = 50000;
          break;
        case 3:
          GET_OBJ_COST(obj) = 75000;
          break;
      }
      break;
    case CYB_FANGS:
      GET_CYBERWARE_RATING(obj) = 0;
      if (GET_CYBERWARE_FLAGS(obj)) {
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 15;
        GET_OBJ_COST(obj) = 8000;
        GET_OBJ_AVAILTN(obj) = 5;
      } else {
        GET_OBJ_COST(obj) = 5000;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 10;
        GET_OBJ_AVAILTN(obj) = 4;
      }
      GET_OBJ_AVAILDAY(obj) = 2;
      break;
    case CYB_HORNS:
      GET_CYBERWARE_RATING(obj) = 0;
      if (GET_CYBERWARE_FLAGS(obj)) {
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 25;
        GET_OBJ_COST(obj) = 16000;
        GET_OBJ_AVAILTN(obj) = 5;
      } else {
        GET_OBJ_COST(obj) = 12000;
        GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = 10;
        GET_OBJ_AVAILTN(obj) = 4;
      }
      GET_OBJ_AVAILDAY(obj) = 2;
      break;
  }
  float grade_essence_modifier = 1.0;
  switch (GET_CYBERWARE_GRADE(obj)) {
    case GRADE_ALPHA:
      grade_essence_modifier = 0.8;
      GET_OBJ_COST(obj) *= 2;
      break;
    case GRADE_BETA:
      grade_essence_modifier = 0.6;
      GET_OBJ_COST(obj) *= 4;
      GET_OBJ_AVAILTN(obj) += 5;
      GET_OBJ_AVAILDAY(obj) = (int) round(GET_OBJ_AVAILDAY(obj) * 1.5);
      break;
    case GRADE_DELTA:
      grade_essence_modifier = 0.5;
      GET_OBJ_COST(obj) *= 8;
      GET_OBJ_AVAILTN(obj) += 9;
      GET_OBJ_AVAILDAY(obj) *= 3;
      break;
  }
  GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = (int) round(GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) * grade_essence_modifier);
  // Finally, apply the cybereye package discount.
  if (GET_CYBERWARE_TYPE(obj) == CYB_EYES && IS_SET(GET_CYBERWARE_FLAGS(obj), EYE_CYBEREYES)) {
    GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) = MAX((int) round(20 * grade_essence_modifier),
                                          GET_CYBERWARE_SETTABLE_ESSENCE_COST(obj) - 50);
  }
}

void price_bio(struct obj_data *obj)
{
  switch (GET_BIOWARE_TYPE(obj)) {
    case BIO_CUSTOM_NERPS:
      // Do absolutely nothing with it.
      return;
    case BIO_ADRENALPUMP:
      if (GET_BIOWARE_RATING(obj) == 1)
        GET_OBJ_COST(obj) = 60000;
      else GET_OBJ_COST(obj) = 100000;
      GET_BIOWARE_ESSENCE_COST(obj) = GET_BIOWARE_RATING(obj) * 125;
      GET_OBJ_AVAILTN(obj) = 10;
      GET_OBJ_AVAILDAY(obj) = 16;
      break;
    case BIO_CATSEYES:
      GET_OBJ_COST(obj) = 15000;
      GET_BIOWARE_ESSENCE_COST(obj) = 20;
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 6;
      break;
    case BIO_DIGESTIVEEXPANSION:
      GET_OBJ_COST(obj) = 80000;
      GET_BIOWARE_ESSENCE_COST(obj) = 100;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 10;
      break;
    case BIO_ENHANCEDARTIC:
      GET_OBJ_COST(obj) = 40000;
      GET_BIOWARE_ESSENCE_COST(obj) = 60;
      GET_OBJ_AVAILTN(obj) = 5;
      GET_OBJ_AVAILDAY(obj) = 6;
      break;
    case BIO_EXTENDEDVOLUME:
      if (GET_BIOWARE_RATING(obj) == 1)
        GET_OBJ_COST(obj) = 8000;
      else if (GET_BIOWARE_RATING(obj) == 2)
        GET_OBJ_COST(obj) = 15000;
      else GET_OBJ_COST(obj) = 25000;
      GET_BIOWARE_ESSENCE_COST(obj) = (GET_BIOWARE_RATING(obj) * 10) + 10;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 4;
      break;
    case BIO_METABOLICARRESTER:
      GET_OBJ_COST(obj) = 20000;
      GET_BIOWARE_ESSENCE_COST(obj) = 60;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 8;
      break;
    case BIO_MUSCLEAUG:
      GET_OBJ_COST(obj) = GET_BIOWARE_RATING(obj) * 20000;
      GET_BIOWARE_ESSENCE_COST(obj) = GET_BIOWARE_RATING(obj) * 40;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 6;
      break;
    case BIO_MUSCLETONER:
      GET_OBJ_COST(obj) = GET_BIOWARE_RATING(obj) * 25000;
      GET_BIOWARE_ESSENCE_COST(obj) = GET_BIOWARE_RATING(obj) * 40;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 6;
      break;
    case BIO_NEPHRITICSCREEN:
      GET_OBJ_COST(obj) = 20000;
      GET_BIOWARE_ESSENCE_COST(obj) = 40;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 4;
      break;
    case BIO_NICTATINGGLAND:
      GET_OBJ_COST(obj) = 8000;
      GET_BIOWARE_ESSENCE_COST(obj) = 10;
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 6;
      break;
    case BIO_ORTHOSKIN:
      if (GET_BIOWARE_RATING(obj) == 1)
        GET_OBJ_COST(obj) = 25000;
      else if (GET_BIOWARE_RATING(obj) == 2)
        GET_OBJ_COST(obj) = 60000;
      else GET_OBJ_COST(obj) = 100000;
      GET_BIOWARE_ESSENCE_COST(obj) = GET_BIOWARE_RATING(obj) * 50;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 8;
      break;
    case BIO_PATHOGENICDEFENSE:
      GET_OBJ_COST(obj) = 24000;
      GET_BIOWARE_ESSENCE_COST(obj) = GET_BIOWARE_RATING(obj) * 20;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 4;
      break;
    case BIO_PLATELETFACTORY:
      GET_OBJ_COST(obj) = 30000;
      GET_BIOWARE_ESSENCE_COST(obj) = 40;
      GET_OBJ_AVAILTN(obj) = 5;
      GET_OBJ_AVAILDAY(obj) = 8;
      break;
    case BIO_SUPRATHYROIDGLAND:
      GET_OBJ_COST(obj) = 50000;
      GET_BIOWARE_ESSENCE_COST(obj) = 140;
      GET_OBJ_AVAILTN(obj) = 8;
      GET_OBJ_AVAILDAY(obj) = 12;
      break;
    case BIO_SYMBIOTES:
      if (GET_BIOWARE_RATING(obj) == 1) {
        GET_OBJ_COST(obj) = 15000;
        GET_BIOWARE_ESSENCE_COST(obj) = 40;
      } else if (GET_BIOWARE_RATING(obj) == 2) {
        GET_OBJ_COST(obj) = 35000;
        GET_BIOWARE_ESSENCE_COST(obj) = 70;
      } else {
        GET_OBJ_COST(obj) = 60000;
        GET_BIOWARE_ESSENCE_COST(obj) = 100;
      }
      GET_OBJ_AVAILTN(obj) = 5;
      GET_OBJ_AVAILDAY(obj) = 10;
      break;
    case BIO_SYNTHACARDIUM:
      if (GET_BIOWARE_RATING(obj) == 1) {
        GET_OBJ_COST(obj) = 6000;
        GET_BIOWARE_ESSENCE_COST(obj) = 20;
      } else {
        GET_OBJ_COST(obj) = 15000;
        GET_BIOWARE_ESSENCE_COST(obj) = 30;
      }
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 10;
      break;
    case BIO_TAILOREDPHEROMONES:
      if (GET_BIOWARE_RATING(obj) == 1) {
        GET_OBJ_COST(obj) = 20000;
        GET_BIOWARE_ESSENCE_COST(obj) = 40;
      } else {
        GET_OBJ_COST(obj) = 45000;
        GET_BIOWARE_ESSENCE_COST(obj) = 60;
      }
      GET_OBJ_AVAILTN(obj) = 12;
      GET_OBJ_AVAILDAY(obj) = 14;
      break;
    case BIO_TOXINEXTRACTOR:
      GET_OBJ_COST(obj) = GET_BIOWARE_RATING(obj) * 45000;
      GET_BIOWARE_ESSENCE_COST(obj) = GET_BIOWARE_RATING(obj) * 40;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 4;
      break;
    case BIO_TRACHEALFILTER:
      GET_OBJ_COST(obj) = GET_BIOWARE_RATING(obj) * 60000;
      GET_BIOWARE_ESSENCE_COST(obj) = GET_BIOWARE_RATING(obj) * 40;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 4;
      break;
    case BIO_CEREBRALBOOSTER:
      if (GET_BIOWARE_RATING(obj) == 1)
        GET_OBJ_COST(obj) = 50000;
      else GET_OBJ_COST(obj) = 110000;
      GET_BIOWARE_ESSENCE_COST(obj) = GET_BIOWARE_RATING(obj) * 40;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 14;
      break;
    case BIO_DAMAGECOMPENSATOR:
      if (GET_BIOWARE_RATING(obj) < 3) {
        GET_OBJ_COST(obj) = GET_BIOWARE_RATING(obj) * 25000;
        GET_OBJ_AVAILTN(obj) = 6;
      } else if (GET_BIOWARE_RATING(obj) < 6) {
        GET_OBJ_COST(obj) = GET_BIOWARE_RATING(obj) * 50000;
        GET_OBJ_AVAILTN(obj) = 10;
      } else {
        GET_OBJ_COST(obj) = GET_BIOWARE_RATING(obj) * 100000;
        GET_OBJ_AVAILTN(obj) = 12;
      }
      GET_BIOWARE_ESSENCE_COST(obj) = GET_BIOWARE_RATING(obj) * 20;
      GET_OBJ_AVAILDAY(obj) = 6;
      break;
    case BIO_PAINEDITOR:
      GET_OBJ_COST(obj) = 60000;
      GET_BIOWARE_ESSENCE_COST(obj) = 60;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 6;
      break;
    case BIO_REFLEXRECORDER:
      GET_OBJ_COST(obj) = 25000;
      GET_BIOWARE_ESSENCE_COST(obj) = 25;
      GET_OBJ_AVAILTN(obj) = 8;
      GET_OBJ_AVAILDAY(obj) = 6;
      break;
    case BIO_SYNAPTICACCELERATOR:
      switch (GET_BIOWARE_RATING(obj)) {
        case 1:
          GET_OBJ_COST(obj) = 75000;
          GET_BIOWARE_ESSENCE_COST(obj) = 40;
          break;
        case 2:
          GET_OBJ_COST(obj) = 200000;
          GET_BIOWARE_ESSENCE_COST(obj) = 100;
      }
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 12;
      break;
    case BIO_THERMOSENSEORGAN:
      GET_OBJ_COST(obj) = 25000;
      GET_BIOWARE_ESSENCE_COST(obj) = 50;
      GET_OBJ_AVAILTN(obj) = 10;
      GET_OBJ_AVAILDAY(obj) = 12;
      break;
    case BIO_TRAUMADAMPER:
      GET_OBJ_COST(obj) = 40000;
      GET_BIOWARE_ESSENCE_COST(obj) = 40;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 8;
      break;
    case BIO_ERYTHROPOITIN:
      GET_OBJ_COST(obj) = 40000;
      GET_BIOWARE_ESSENCE_COST(obj) = 40;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 30;
      MAKE_INCOMPATIBLE_WITH_MAGIC(obj);
      break;
    case BIO_CALCITONIN:
      GET_OBJ_COST(obj) = 60000;
      GET_BIOWARE_ESSENCE_COST(obj) = 40;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 30;
      MAKE_INCOMPATIBLE_WITH_MAGIC(obj);
      break;
    case BIO_PHENOTYPIC_BOD:
      GET_OBJ_COST(obj) = 65000;
      GET_BIOWARE_ESSENCE_COST(obj) = 50;
      GET_OBJ_AVAILTN(obj) = 5;
      GET_OBJ_AVAILDAY(obj) = 21;
      break;
    case BIO_PHENOTYPIC_QUI:
      GET_OBJ_COST(obj) = 65000;
      GET_BIOWARE_ESSENCE_COST(obj) = 50;
      GET_OBJ_AVAILTN(obj) = 5;
      GET_OBJ_AVAILDAY(obj) = 21;
      break;
    case BIO_PHENOTYPIC_STR:
      GET_OBJ_COST(obj) = 65000;
      GET_BIOWARE_ESSENCE_COST(obj) = 50;
      GET_OBJ_AVAILTN(obj) = 5;
      GET_OBJ_AVAILDAY(obj) = 21;
      break;
    case BIO_BIOSCULPTING:
      GET_OBJ_COST(obj) = 10000;
      GET_BIOWARE_ESSENCE_COST(obj) = 20;
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 4;
      break;
    default:
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Unknown bioware type %d passed to price_bio(%ld).", GET_BIOWARE_TYPE(obj), GET_OBJ_VNUM(obj));
      break;
  }

  // Check for cultured. Don't modify the prices of things that are cultured by default (brainware, etc)
  if ((GET_BIOWARE_TYPE(obj) < BIO_CEREBRALBOOSTER || GET_BIOWARE_TYPE(obj) >= BIO_BIOSCULPTING) && GET_SETTABLE_BIOWARE_IS_CULTURED(obj)) {
    GET_OBJ_COST(obj) *= 4;
    GET_BIOWARE_ESSENCE_COST(obj) = (int) round(GET_BIOWARE_ESSENCE_COST(obj) * .75);
    GET_OBJ_AVAILTN(obj) += 2;
    GET_OBJ_AVAILDAY(obj) *= 5;
  }
}

void set_elemental_races() {
  for (int idx = 0; idx < NUM_ELEMENTS; idx++) {
    rnum_t rnum = real_mobile(elements[idx].vnum);
    if (rnum < 0) {
      log_vfprintf("ERROR: We require that mob %ld exists as an elemental, but it's not there!", elements[idx].vnum);
      exit(ERROR_MISSING_ELEMENTALS);
    }
    GET_RACE(&mob_proto[rnum]) = RACE_PC_CONJURED_ELEMENTAL;
  }
}

// We want a way to traverse all entrances to a host, but with one-way passages, we can't use exits
// This function resets and populates entrance lists for each host
// Duplicate entries can exist where there's more than one way to get from host A to host B
void collate_host_entrances() {

  // Clear existing entrance lists
  for (rnum_t source_rnum = 0; source_rnum <= top_of_matrix; source_rnum++) {
    struct entrance_data *entrance = NULL, *next = NULL;
    for (entrance = matrix[source_rnum].entrance; entrance; entrance = next) {
      next = entrance->next;
      delete entrance;
    }
    matrix[source_rnum].entrance = NULL;
  }

  // Find exits and add to respective entrance lists
  rnum_t dest_rnum = -1;

  // The real host "source" will be added as an entrance to "dest" (if dest is real)
  #define ADD_NEW_ENTRANCE_IF_EXISTS(dest_vnum, source_rnum) { \
    if ((dest_rnum = real_host(dest_vnum)) >= 0) { \
      struct entrance_data *entrance = new entrance_data; \
      entrance->host = &matrix[source_rnum]; \
      entrance->next = matrix[dest_rnum].entrance; \
      matrix[dest_rnum].entrance = entrance; \
    } \
  }

  for (rnum_t source_rnum = 0; source_rnum <= top_of_matrix; source_rnum++) {
    // Source is an entrance to its exit destination(s)
    for (struct exit_data *exit = matrix[source_rnum].exit; exit; exit = exit->next) {
      ADD_NEW_ENTRANCE_IF_EXISTS(exit->host, source_rnum);
    }

    // Source is also an entrance to its parent (e.g., logon LTG)
    ADD_NEW_ENTRANCE_IF_EXISTS(matrix[source_rnum].parent, source_rnum);

    // Source is also an entrance to its trapdoor destination(s)
    for (int sub = ACIFS_ACCESS; sub < NUM_ACIFS; sub++) {
      ADD_NEW_ENTRANCE_IF_EXISTS(matrix[source_rnum].stats[sub][MTX_STAT_TRAPDOOR], source_rnum);
    }
  }
  #undef ADD_NEW_ENTRANCE_IF_EXISTS
}

std::map<std::string, int> room_flag_map = {};
void initialize_and_alphabetize_room_flags() {
  for (int idx = 0; idx < ROOM_MAX; idx++) {
    switch(idx) {
      // Named flags.
      case ROOM_DARK:
      case ROOM_DEATH:
      case 6:
      case 8:
      case 11:
      case 13:
      case 14:
      case ROOM_BFS_MARK:
      case ROOM_LOW_LIGHT:
      case 17:
      case 26:
      case 27:
      case ROOM_ELEVATOR_SHAFT:
      case ROOM_CORPSE_SAVE_HACK:
        continue;
    }
    room_flag_map[std::string(room_bits[idx])] = idx;
  }
}

std::map<std::string, int> item_extra_flag_map = {};
void initialize_and_alphabetize_item_extra_flags() {
  for (int idx = 0; idx < MAX_ITEM_EXTRA; idx++) {
    switch(idx) {
      case 4:
      case 5:
      case 14:
      case 22:
        continue;
    }
    item_extra_flag_map[std::string(extra_bits[idx])] = idx;
  }
}

std::map<std::string, int> mob_flag_map = {};
void initialize_and_alphabetize_mob_flags() {
  for (int idx = 0; idx < MOB_MAX; idx++) {
    switch(idx) {
      case 4:
      case 17:
      case 22:
      case 23:
      case 28:
      case 29:
        continue;
    }
    mob_flag_map[std::string(action_bits[idx])] = idx;
  }
}

std::map<std::string, int> wear_flag_map_for_exdescs = {};
void initialize_and_alphabetize_wear_flags_for_exdescs() {
  for (int idx = 0; idx < ITEM_WEAR_MAX; idx++) {
    switch(idx) {
      case ITEM_WEAR_TAKE:
      case ITEM_WEAR_BODY:
      case ITEM_WEAR_SHIELD:
      case ITEM_WEAR_ABOUT:
      case ITEM_WEAR_WIELD:
      case ITEM_WEAR_HOLD:
      case ITEM_WEAR_SOCK:
      case ITEM_WEAR_LAPEL:
        continue;
    }
    wear_flag_map_for_exdescs[std::string(wear_bits_for_pc_exdescs[idx])] = idx;
  }
}

extern void alphabetize_pet_echoes_by_name();

void initialize_and_alphabetize_flag_maps() {
  alphabetize_pet_echoes_by_name();
  initialize_and_alphabetize_room_flags();
  initialize_and_alphabetize_item_extra_flags();
  initialize_and_alphabetize_mob_flags();
  initialize_and_alphabetize_wear_flags_for_exdescs();
}