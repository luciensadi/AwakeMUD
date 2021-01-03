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
#include <time.h>
#include <math.h>
#include <mysql/mysql.h>
#if defined(WIN32) && !defined(__CYGWIN__)
#include <process.h>
#define getpid() _getpid()
#else
#include <unistd.h>
#endif
#include <sodium.h>

/* mysql_config.h must be filled out with your own connection info. */
/* For obvious reasons, DO NOT ADD THIS FILE TO SOURCE CONTROL AFTER CUSTOMIZATION. */
#include "mysql_config.h"

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "file.h"
#include "db.h"
#include "newdb.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "house.h"
#include "newmatrix.h"
#include "memory.h"
#include "dblist.h"
#include "quest.h"
#include "newshop.h"
#include "constants.h"
#include "vtable.h"
#include "config.h"
#include "security.h"
#include "olc.h"
#include <new>
#include "transport.h"
#include "bullet_pants.h"

extern void calc_weight(struct char_data *ch);
extern void read_spells(struct char_data *ch);
extern struct obj_data *find_obj(int num);
extern void obj_to_cyberware(struct obj_data *, struct char_data *);
extern void obj_from_cyberware(struct obj_data *);
extern void obj_to_bioware(struct obj_data *, struct char_data *);
extern void obj_from_bioware(struct obj_data *);
extern char *cleanup(char *dest, const char *src);
extern void add_phone_to_list(struct obj_data *);
extern void idle_delete();
extern void clearMemory(struct char_data * ch);
extern void weight_change_object(struct obj_data * obj, float weight);
extern void generate_archetypes();
extern void populate_mobact_aggression_octets();


/**************************************************************************
*  declarations of most of the 'global' variables                         *
************************************************************************ */

// beginning of mud time
unsigned long beginning_of_time = 2157880000U;
long mud_boot_time = 0;
struct room_data *world = NULL; /* array of rooms   */
rnum_t top_of_world = 0; /* ref to top element of world  */

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
int mob_chunk_size = 100;       // default to 100
int top_of_mob_array = 0;

struct index_data *obj_index; /* index table for object file  */
struct obj_data *obj_proto; /* prototypes for objs   */
rnum_t top_of_objt = 0; /* top of object index table  */
int top_of_obj_array = 0;
int obj_chunk_size = 100;       // default to 100

struct zone_data *zone_table; /* zone table    */
rnum_t top_of_zone_table = 0;/* top element of zone tab  */
struct message_list fight_messages[MAX_MESSAGES]; /* fighting messages  */

struct quest_data *quest_table = NULL; // array of quests
rnum_t top_of_questt = 0;
int top_of_quest_array = 0;
int quest_chunk_size = 25;

struct shop_data *shop_table = NULL;   // array of shops
rnum_t top_of_shopt = 0;            // ref to top element of shop_table
int top_of_shop_array = 0;
int shop_chunk_size = 25;

int top_of_matrix_array = 0;
int top_of_ic_array = 0;
int top_of_world_array = 0;
int world_chunk_size = 100;     /* size of world to add on each reallocation */
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
int restrict = 0;               /* level of game restriction     */
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
void load_saved_veh();
void assign_mobiles(void);
void assign_objects(void);
void assign_rooms(void);
void assign_shopkeepers(void);
void assign_johnsons(void);
void randomize_shop_prices(void);
int zone_is_empty(int zone_nr);
void reset_zone(int zone, int reboot);
int file_to_string(const char *name, char *buf);
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
    log_vfprintf("ERROR: You need to run the %s migration from the SQL directory. "
                 "Probable syntax from root directory: `mysql -u YOUR_USERNAME -p AwakeMUD < %s`.",
                 table_name,
                 migration_path_from_root_directory);
    exit(ERROR_DB_TABLE_REQUIRED);
  }

  if ((row = mysql_fetch_row(res)) && mysql_field_count(mysql))
    have_table = TRUE;
    
  mysql_free_result(res);
  
  if (!have_table) {
    log_vfprintf("ERROR: You need to run the %s migration from the SQL directory. "
                 "Probable syntax from root directory: `mysql -u YOUR_USERNAME -p AwakeMUD < %s`.",
                 table_name,
                 migration_path_from_root_directory);
    exit(ERROR_DB_TABLE_REQUIRED);
  }
}

void require_that_field_exists_in_table(const char *field_name, const char *table_name, const char *migration_path_from_root_directory) {
  bool have_column = FALSE;
  
  MYSQL_RES *res;
  MYSQL_ROW row;
  
  char query_buf[1000];
  snprintf(query_buf, sizeof(query_buf), "SHOW COLUMNS FROM %s LIKE '%s';", 
           prepare_quotes(buf, table_name, sizeof(buf)),
           prepare_quotes(buf2, field_name, sizeof(buf2)));
  mysql_wrapper(mysql, query_buf);
  
  if (!(res = mysql_use_result(mysql))) {
    log_vfprintf("ERROR: You need to run the %s migration from the SQL directory. "
                 "Probable syntax from root directory: `mysql -u YOUR_USERNAME -p AwakeMUD < %s`.",
                 table_name,
                 migration_path_from_root_directory);
    exit(ERROR_DB_COLUMN_REQUIRED);
  }

  if ((row = mysql_fetch_row(res)) && mysql_field_count(mysql))
    have_column = TRUE;
    
  mysql_free_result(res);
  
  if (!have_column) {
    log_vfprintf("ERROR: You need to run the %s migration from the SQL directory! "
                 "Probable syntax from root directory: `mysql -u YOUR_USERNAME -p AwakeMUD < %s`.",
                 table_name,
                 migration_path_from_root_directory);
    exit(ERROR_DB_COLUMN_REQUIRED);
  }
}

void boot_world(void)
{
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
  
  log("Initializing libsodium for crypto functions.");
  if (sodium_init() < 0) {
    // The library could not be initialized. Fail.
    log("ERROR: Libsodium initialization failed. Terminating program.");
    exit(ERROR_LIBSODIUM_INIT_FAILED);
  }
  
#ifdef CRYPTO_DEBUG
  log("Performing crypto performance and validation tests.");
  run_crypto_tests();
#endif
  
  log("Booting MYSQL database.");
  initialize_and_connect_to_mysql();
  
  log("Verifying DB compatibility with extended-length passwords.");
  verify_db_password_column_size();
  
  log("Verifying that DB has expected migrations. Note that not all migrations are checked here.");
  require_that_sql_table_exists("pfiles_ammo", "SQL/bullet_pants.sql");
  require_that_sql_table_exists("command_fuckups", "SQL/fuckups.sql");
  require_that_field_exists_in_table("socialbonus", "pfiles", "SQL/migrations/socialize.sql");
  
  log("Handling idle deletion.");
  idle_delete();

  log("Loading zone table.");
  index_boot(DB_BOOT_ZON);

  log("Loading rooms.");
  index_boot(DB_BOOT_WLD);

  log("Checking start rooms.");
  check_start_rooms();

  log("Loading mobs and generating index.");
  index_boot(DB_BOOT_MOB);

  log("Loading objs and generating index.");
  index_boot(DB_BOOT_OBJ);

  log("Loading vehicles and generating index.");
  index_boot(DB_BOOT_VEH);

  log("Loading quests.");
  index_boot(DB_BOOT_QST);

  log("Loading shops.");
  index_boot(DB_BOOT_SHP);

  log("Loading Matrix Hosts.");
  index_boot(DB_BOOT_MTX);

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

  log("Booting world.");
  boot_world();

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

  log("Initializing transportation system:");
  TransportInit();

  for (i = 0; i <= top_of_zone_table; i++) {
    log_vfprintf("Resetting %s (rooms %d-%d).", zone_table[i].name,
        (i ? (zone_table[i - 1].top + 1) : 0), zone_table[i].top);
    reset_zone(i, 1);
    // log_vfprintf("Reset of %s complete. Writing to disk...", zone_table[i].name);
    extern void write_zone_to_disk(int vnum);
    write_zone_to_disk(zone_table[i].number);
    // log("Written.");
  }
  
  log("Loading saved vehicles.");
  load_saved_veh();
  
  log("Purging unowned vehicles.");
  purge_unowned_vehs();

  log("Booting houses.");
  House_boot();
  boot_time = time(0);
  
  log("Loading shop orders.");
  boot_shop_orders();

  log("Setting up mobact aggression octets.");
  populate_mobact_aggression_octets();
  
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
  fscanf(index, "%s\n", buf1);
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

    fscanf(index, "%s\n", buf1);
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
    memset((char *) mob_proto, 0, (sizeof(struct char_data) *
                                   (rec_count + mob_chunk_size)));

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
  fscanf(index, "%s\n", buf1);
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
    fscanf(index, "%s\n", buf1);
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
        shutdown();
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
      shutdown();
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
    shutdown();
  }
  while (virtual_nr > zone_table[zone].top)
    if (++zone > top_of_zone_table) {
      fprintf(stderr, "FATAL ERROR: Veh %ld is outside of any zone.\n", virtual_nr);
      shutdown();
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
  veh_proto[veh_nr].seating[1] = data.GetInt("Seating", 0);
  veh_proto[veh_nr].seating[0] = data.GetInt("SeatingBack", 0);
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
    snprintf(buf, sizeof(buf), "FATAL ERROR: last_seen %ld >= nr %ld.", last_seen, nr);
    log(buf);
    shutdown();
  }
  
  static DBIndex::rnum_t rnum = 0, zone = 0;
  char field[64];
  if (nr <= (zone ? zone_table[zone - 1].top : -1)) {
    log_vfprintf("FATAL ERROR: Host #%d is below zone %d.\n", nr, zone_table[zone].number);
    shutdown();
  }
  while (nr > zone_table[zone].top)
    if (++zone > top_of_zone_table) {
      log_vfprintf("FATAL ERROR: Host %d is outside of any zone.\n", nr);
      shutdown();
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
  host->colour = data.GetInt("Colour", 0);
  host->security = data.GetInt("Security", 0);
  host->intrusion = data.GetInt("Difficulty", 0);
  host->stats[ACCESS][0] = data.GetLong("Access", 0);
  host->stats[ACCESS][2] = data.GetLong("AccessScramble", 0);
  host->stats[ACCESS][5] = data.GetLong("AccessTrapdoor", 0);
  if (host->stats[ACCESS][2])
    host->stats[ACCESS][1] = 1;
  host->stats[CONTROL][0] = data.GetLong("Control", 0);
  host->stats[CONTROL][5] = data.GetLong("ControlTrapdoor", 0);
  host->stats[INDEX][0] = data.GetLong("Index", 0);
  host->stats[INDEX][5] = data.GetLong("IndexTrapdoor", 0);
  host->stats[FILES][0] = data.GetLong("Files", 0);
  host->stats[FILES][2] = data.GetLong("FilesScramble", 0);
  host->stats[FILES][5] = data.GetLong("FilesTrapdoor", 0);
  if (host->stats[FILES][2])
    host->stats[FILES][1] = 1;
  host->stats[SLAVE][0] = data.GetLong("Slave", 0);
  host->stats[SLAVE][2] = data.GetLong("SlaveScramble", 0);
  host->stats[SLAVE][5] = data.GetLong("SlaveTrapdoor", 0);
  if (host->stats[SLAVE][2])
    host->stats[SLAVE][1] = 1;
  host->type = data.LookupInt("Type", host_type, 0);
  int num_fields = data.NumSubsections("EXITS");
  for (int x = 0; x < num_fields; x++) {
    const char *name = data.GetIndexSection("EXITS", x);
    exit_data *exit = new exit_data;
    memset(exit, 0, sizeof(exit_data));
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
    memset(trigger, 0, sizeof(trigger_step));
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
  if (!host->type)
    switch (host->colour) {
    case 0:
      host->found = number(1, 6) - 1;
      break;
    case 1:
      host->found = number(1, 6) + number(1, 6) - 2;
      break;
    case 2:
      host->found = number(1, 6) + number(1, 6);
      break;
    case 3:
    case 4:
      host->found = number(1, 6) + number(1, 6) + 2;
      break;
    }
  top_of_matrix = rnum++;
}
void parse_ic(File &fl, long nr)
{
  static long last_seen = -1;
  if (last_seen == -1) {
    last_seen = nr;
  } else if (last_seen >= nr) {
    snprintf(buf, sizeof(buf), "FATAL ERROR: last_seen %ld >= nr %ld.", last_seen, nr);
    log(buf);
    shutdown();
  }
  
  static DBIndex::rnum_t rnum = 0, zone = 0;
  ic_index[rnum].vnum = nr;
  ic_index[rnum].number = 0;
  ic_index[rnum].func = NULL;

  matrix_icon *ic = ic_proto+rnum;
  clear_icon(ic);
  ic->number = rnum;

  if (nr <= (zone ? zone_table[zone - 1].top : -1)) {
    log_vfprintf("FATAL ERROR: IC #%d is below zone %d.\n", nr, zone);
    shutdown();
  }
  while (nr > zone_table[zone].top)
    if (++zone > top_of_zone_table) {
      log_vfprintf("FATAL ERROR: IC%d is outside of any zone.\n", nr);
      shutdown();
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
    snprintf(buf, sizeof(buf), "FATAL ERROR: last_seen %ld >= nr %ld.", last_seen, nr);
    log(buf);
    shutdown();
  }
  
  static DBIndex::rnum_t rnum = 0, zone = 0;

  if (nr <= (zone ? zone_table[zone - 1].top : -1)) {
    log_vfprintf("FATAL ERROR: Room #%d is below zone %d.\n", nr, zone_table[zone].number);
    shutdown();
  }
  while (nr > zone_table[zone].top)
    if (++zone > top_of_zone_table) {
      log_vfprintf("FATAL ERROR: Room %d is outside of any zone.\n", nr);
      shutdown();
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
  // read in directions
  int i;
  for (i = 0; *fulldirs[i] != '\n'; i++) {
    char sect[16];
    snprintf(sect, sizeof(sect), "EXIT %s", fulldirs[i]);

    room->dir_option[i] = NULL;

    if (data.DoesSectionExist(sect)) {
      char field[64];

      snprintf(field, sizeof(field), "%s/ToVnum", sect);
      int to_vnum = data.GetInt(field, -1);

      if (to_vnum < 0) {
        log_vfprintf("Room #%d's %s exit had invalid destination -- skipping",
            nr, fulldirs[i]);
        continue;
      }

      room->dir_option[i] = new room_direction_data;
      room_direction_data *dir = room->dir_option[i];
      memset(dir, 0, sizeof(room_direction_data));

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

  room->ex_description = NULL;

  // finally, read in extra descriptions
  for (i = 0; true; i++) {
    char sect[16];
    snprintf(sect, sizeof(sect), "EXTRADESC %d", i);

    if (data.NumFields(sect) > 0) {
      char field[64];

      snprintf(field, sizeof(field), "%s/Keywords", sect);
      char *keywords = str_dup(data.GetString(field, NULL));

      if (!*keywords) {
        log_vfprintf("Room #%d's extra description #%d had no keywords -- skipping",
            nr, i);
        DELETE_ARRAY_IF_EXTANT(keywords);
        continue;
      }


      extra_descr_data *desc = new extra_descr_data;
      memset(desc, 0, sizeof(extra_descr_data));
      desc->keyword = keywords;
      snprintf(field, sizeof(field), "%s/Desc", sect);
      desc->description = str_dup(data.GetString(field, NULL));
      desc->next = room->ex_description;
      room->ex_description = desc;
    } else
      break;
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
  memset(world[room].dir_option[dir], 0, sizeof(room_direction_data));
  world[room].dir_option[dir]->general_description = fread_string(fl, buf2);
  world[room].dir_option[dir]->keyword = fread_string(fl, buf2);

  if (!get_line(fl, line)) {
    fprintf(stderr, "FATAL ERROR: Format error, %s: Cannot get line from file.\n", buf2);
    shutdown();
  }
  if ((retval = sscanf(line, " %d %d %d %d %d %d %d", t, t + 1, t + 2, t + 3,
                       t + 4, t + 5, t + 6)) < 4) {
    fprintf(stderr, "FATAL ERROR: Format error, %s: Expected seven numbers like ' # # # # # # #'\n", buf2);
    shutdown();
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
    shutdown();
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
        case 'G':
        case 'C':
        case 'U':
        case 'I':
          a = ZCMD.arg1 = real_object(ZCMD.arg1);
          ENSURE_OBJECT_IS_KOSHER(a);
          break;
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
    snprintf(buf, sizeof(buf), "FATAL ERROR: last_seen %ld >= nr %ld.", last_seen, nr);
    log(buf);
    shutdown();
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
  GET_TITLE(mob) = NULL;

  MOB_FLAGS(mob).FromString(data.GetString("MobFlags", "0"));
  AFF_FLAGS(mob).FromString(data.GetString("AffFlags", "0"));

  GET_RACE(mob) = data.LookupInt("Race", pc_race_types, RACE_HUMAN);
  GET_SEX(mob) = data.LookupInt("Gender", genders, SEX_NEUTRAL);

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
  mob->real_abils.ess = 600;
  mob->real_abils.bod_index = 900;

  mob->aff_abils = mob->real_abils;

  GET_HEIGHT(mob) = data.GetInt("POINTS/Height", 100);
  GET_WEIGHT(mob) = data.GetInt("POINTS/Weight", 5);
  GET_LEVEL(mob) = data.GetInt("POINTS/Level", 0);
  GET_MAX_PHYSICAL(mob) = data.GetInt("POINTS/MaxPhys", 10*100);
  GET_MAX_MENTAL(mob) = data.GetInt("POINTS/MaxMent", 10*100);
  GET_BALLISTIC(mob) = data.GetInt("POINTS/Ballistic", 0);
  GET_IMPACT(mob) = data.GetInt("POINTS/Impact", 0);
  GET_NUYEN(mob) = data.GetInt("POINTS/Cash", 0);
  GET_BANK(mob) = data.GetInt("POINTS/Bank", 0);
  GET_KARMA(mob) = data.GetInt("POINTS/Karma", 0);

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
      snprintf(buf, sizeof(buf), "AMMO/%s", get_ammo_representation(wp, am, 0));
      GET_BULLETPANTS_AMMO_AMOUNT(mob, wp, am) = data.GetInt(buf, 0);
    }

  top_of_mobt = rnum++;
}

void parse_object(File &fl, long nr)
{
  static long last_seen = -1;
  if (last_seen == -1) {
    last_seen = nr;
  } else if (last_seen >= nr) {
    snprintf(buf, sizeof(buf), "FATAL ERROR: last_seen %ld >= nr %ld.", last_seen, nr);
    log(buf);
    shutdown();
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
  GET_OBJ_WEIGHT(obj) = MAX(0, data.GetFloat("POINTS/Weight", 5));
  GET_OBJ_BARRIER(obj) = data.GetInt("POINTS/Barrier", 3);
  GET_OBJ_CONDITION(obj) = GET_OBJ_BARRIER(obj);

  GET_OBJ_COST(obj) = data.GetInt("POINTS/Cost", 0);

  obj->obj_flags.quest_id = 0;

  int i;
  for (i = 0; i < NUM_VALUES; i++) {
    char field[32];
    snprintf(field, sizeof(field), "VALUES/Val%d", i);

    GET_OBJ_VAL(obj, i) = data.GetInt(field, 0);
  }
  
  // Set the do-not-touch flags for known templated items.
  if ((BOTTOM_OF_TEMPLATE_ITEMS <= nr && nr <= TOP_OF_TEMPLATE_ITEMS)
      || nr == OBJ_BLANK_MAGAZINE) {
    GET_OBJ_EXTRA(obj).SetBit(ITEM_DONT_TOUCH);
  }
  
  { // Per-type modifications and settings.
    int mult;
    const char *type_as_string = NULL;
    switch (GET_OBJ_TYPE(obj)) {
      case ITEM_CHIP:
        GET_OBJ_VAL(obj, 2) = (GET_OBJ_VAL(obj, 1) * GET_OBJ_VAL(obj, 1)) * 3;
        GET_OBJ_AVAILTN(obj) = 6;
        GET_OBJ_AVAILDAY(obj) = 4;
        if (!skills[GET_OBJ_VAL(obj, 0)].type)
          GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 2) * 100;
        else if (GET_OBJ_VAL(obj, 0) >= SKILL_ENGLISH) {
          GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 2) * 50;
          GET_OBJ_AVAILDAY(obj) = 1.5;
        } else {
          GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 2) * 150;
          GET_OBJ_AVAILTN(obj) = 5;
        }
        break;
      case ITEM_CYBERWARE:
        price_cyber(obj);
        break;
      case ITEM_BIOWARE:
        price_bio(obj);
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
      
        if (GET_AMMOBOX_WEAPON(obj) == WEAP_CANNON) {
          // Assault cannons have special ammo and special rules that aren't reflected in our normal table.
          
          // Max size 500-- otherwise it's too heavy to carry.
          GET_AMMOBOX_QUANTITY(obj) = MAX(MIN(GET_AMMOBOX_QUANTITY(obj), 500), 10);
          
          // Set values according to Assault Cannon ammo (SR3 p281).
          GET_OBJ_WEIGHT(obj) = (((float) GET_AMMOBOX_QUANTITY(obj)) * 1.25) / 10;
          GET_OBJ_COST(obj) = GET_AMMOBOX_QUANTITY(obj) * 45;
          GET_OBJ_AVAILDAY(obj) = 3;
          GET_OBJ_AVAILTN(obj) = 5;
          GET_OBJ_STREET_INDEX(obj) = 2;
          
          // They also may only be explosive (SR3 p279).
          GET_AMMOBOX_TYPE(obj) = AMMO_EXPLOSIVE;
        } else {
          // Max size 1000-- otherwise it's too heavy to carry.
          GET_AMMOBOX_QUANTITY(obj) = MAX(MIN(GET_AMMOBOX_QUANTITY(obj), 1000), 10);
          
          // Update weight and cost.
          GET_OBJ_WEIGHT(obj) = GET_AMMOBOX_QUANTITY(obj) * ammo_type[GET_AMMOBOX_TYPE(obj)].weight;
          GET_OBJ_COST(obj) = GET_AMMOBOX_QUANTITY(obj) * ammo_type[GET_AMMOBOX_TYPE(obj)].cost;
          
          // Set the TNs for this ammo per the default values.
          GET_OBJ_AVAILDAY(obj) = ammo_type[GET_AMMOBOX_TYPE(obj)].time;
          GET_OBJ_AVAILTN(obj) = ammo_type[GET_AMMOBOX_TYPE(obj)].tn;
          
          // Set the street index.
          GET_OBJ_STREET_INDEX(obj) = ammo_type[GET_AMMOBOX_TYPE(obj)].street_index;
        }
        
        // Set the strings-- we want all these things to match for simplicity's sake.
        type_as_string = get_weapon_ammo_name_as_string(GET_AMMOBOX_WEAPON(obj));
        
        snprintf(buf, sizeof(buf), "metal ammo ammunition box %s %s %d-%s %s%s",
                GET_AMMOBOX_WEAPON(obj) == WEAP_CANNON ? "normal" : ammo_type[GET_AMMOBOX_TYPE(obj)].name,
                weapon_type[GET_AMMOBOX_WEAPON(obj)],
                GET_AMMOBOX_QUANTITY(obj),
                type_as_string,
                type_as_string,
                GET_AMMOBOX_QUANTITY(obj) > 1 ? "s" : "");
        // log_vfprintf("Changing %s to %s for %ld.", obj->text.keywords, buf, nr);
        DELETE_ARRAY_IF_EXTANT(obj->text.keywords);
        obj->text.keywords = str_dup(buf);
        
        snprintf(buf, sizeof(buf), "a %d-%s box of %s %s ammunition",
                GET_AMMOBOX_QUANTITY(obj),
                type_as_string,
                GET_AMMOBOX_WEAPON(obj) == WEAP_CANNON ? "normal" : ammo_type[GET_AMMOBOX_TYPE(obj)].name,
                weapon_type[GET_AMMOBOX_WEAPON(obj)]);
        // log_vfprintf("Changing %s to %s for %ld.", obj->text.name, buf, nr);
        DELETE_ARRAY_IF_EXTANT(obj->text.name);
        obj->text.name = str_dup(buf);
        
        snprintf(buf, sizeof(buf), "A metal box of %s %s %s%s has been left here.",
                GET_AMMOBOX_WEAPON(obj) == WEAP_CANNON ? "normal" : ammo_type[GET_AMMOBOX_TYPE(obj)].name,
                weapon_type[GET_AMMOBOX_WEAPON(obj)],
                type_as_string,
                GET_AMMOBOX_QUANTITY(obj) > 1 ? "s" : "");
        // log_vfprintf("Changing %s to %s for %ld.", obj->text.room_desc, buf, nr);
        DELETE_ARRAY_IF_EXTANT(obj->text.room_desc);
        obj->text.room_desc = str_dup(buf);
        
        strcpy(buf, "A hefty box of ammunition, banded in metal and secured with flip-down hasps for transportation and storage.");
        // log_vfprintf("Changing %s to %s for %ld.", obj->text.look_desc, buf, nr);
        DELETE_ARRAY_IF_EXTANT(obj->text.look_desc);
        obj->text.look_desc = str_dup(buf);
        break;
      case ITEM_WEAPON:
        // Attempt to automatically rectify broken weapons.
        bool is_melee = FALSE;
        if (GET_WEAPON_ATTACK_TYPE(obj) > MAX_WEAP)
          switch (GET_WEAPON_SKILL(obj)) {
            case SKILL_EDGED_WEAPONS:
              GET_WEAPON_ATTACK_TYPE(obj) = WEAP_EDGED;
              is_melee = TRUE;
              break;
            case SKILL_POLE_ARMS:
              GET_WEAPON_ATTACK_TYPE(obj) = WEAP_POLEARM;
              is_melee = TRUE;
              break;
            case SKILL_WHIPS_FLAILS:
              GET_WEAPON_ATTACK_TYPE(obj) = WEAP_WHIP;
              is_melee = TRUE;
              break;
            case SKILL_CLUBS:
              GET_WEAPON_ATTACK_TYPE(obj) = WEAP_CLUB;
              is_melee = TRUE;
              break;
            case SKILL_UNARMED_COMBAT:
              GET_WEAPON_ATTACK_TYPE(obj) = WEAP_GLOVE;
              is_melee = TRUE;
              break;
            case SKILL_PISTOLS:
              GET_WEAPON_ATTACK_TYPE(obj) = WEAP_HEAVY_PISTOL;
              break;
            case SKILL_RIFLES:
              GET_WEAPON_ATTACK_TYPE(obj) = WEAP_SPORT_RIFLE;
              break;
            case SKILL_SHOTGUNS:
              GET_WEAPON_ATTACK_TYPE(obj) = WEAP_SHOTGUN;
              break;
            case SKILL_ASSAULT_RIFLES:
              GET_WEAPON_ATTACK_TYPE(obj) = WEAP_ASSAULT_RIFLE;
              break;
            case SKILL_SMG:
              GET_WEAPON_ATTACK_TYPE(obj) = WEAP_SMG;
              break;
            case SKILL_GRENADE_LAUNCHERS:
              GET_WEAPON_ATTACK_TYPE(obj) = WEAP_GREN_LAUNCHER;
              break;
            case SKILL_MISSILE_LAUNCHERS:
              GET_WEAPON_ATTACK_TYPE(obj) = WEAP_MISS_LAUNCHER;
              break;
            case SKILL_TASERS:
              GET_WEAPON_ATTACK_TYPE(obj) = WEAP_TASER;
              break;
            case SKILL_MACHINE_GUNS:
              GET_WEAPON_ATTACK_TYPE(obj) = WEAP_LMG;
              break;
            case SKILL_ASSAULT_CANNON:
              GET_WEAPON_ATTACK_TYPE(obj) = WEAP_CANNON;
              break;
          }
        if (GET_WEAPON_SKILL(obj) > 100)
          GET_WEAPON_SKILL(obj) -= 100;
          
        if (is_melee)
          GET_WEAPON_REACH(obj) = MAX(0, GET_WEAPON_REACH(obj));
          
        break;
    }
  } // End per-type modifications.

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
        log_vfprintf("Item #%d's affect #%d had no location -- skipping", nr, i);
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
        log_vfprintf("Item #%d's extra description #%d had no keywords -- skipping",
            nr, i);
        continue;
      }

      extra_descr_data *desc = new extra_descr_data;
      memset(desc, 0, sizeof(extra_descr_data));
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
  long j, t[12];
  char line[256];

  quest_table[quest_nr].vnum = virtual_nr;

  fl.GetLine(line, 256, FALSE);
  if (sscanf(line, "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
             t, t + 1, t + 2, t + 3, t+4, t+5,
             t + 6, t + 7, t + 8, t + 9, t + 10, t + 11) != 12) {
    fprintf(stderr, "FATAL ERROR: Format error in quest #%ld, expecting twelve numbers like '# # # # # # # # # # # #'.\n",
            quest_nr);
    shutdown();
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

  if (quest_table[quest_nr].num_objs > 0) {
    quest_table[quest_nr].obj = new quest_om_data[quest_table[quest_nr].num_objs];
    memset(quest_table[quest_nr].obj, 0, sizeof(quest_om_data) * quest_table[quest_nr].num_objs);
    for (j = 0; j < quest_table[quest_nr].num_objs; j++) {
      fl.GetLine(line, 256, FALSE);
      if (sscanf(line, "%ld %ld %ld %ld %ld %ld %ld %ld", t, t + 1, t + 2, t + 3,
                 t + 4, t + 5, t + 6, t + 7) != 8) {
        fprintf(stderr, "FATAL ERROR: Format error in quest #%ld, obj #%ld: expecting 8 numbers like '# # # # # # # #'\n", quest_nr, j);
        shutdown();
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
        shutdown();
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
  } else
    quest_table[quest_nr].mob = NULL;

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
    snprintf(buf, sizeof(buf), "FATAL ERROR: last_seen %ld >= virtual_nr %ld.", last_seen, virtual_nr);
    log(buf);
    shutdown();
  }
  
  static DBIndex::rnum_t rnum = 0, zone = 0;
  char field[64];
  if (virtual_nr <= (zone ? zone_table[zone - 1].top : -1)) {
    log_vfprintf("FATAL ERROR: Shop #%d is below zone %d.\n", virtual_nr, zone_table[zone].number);
    shutdown();
  }
  while (virtual_nr > zone_table[zone].top)
    if (++zone > top_of_zone_table) {
      log_vfprintf("FATAL ERROR: Shop %d is outside of any zone.\n", virtual_nr);
      shutdown();
    }
  shop_data *shop = shop_table+rnum;
  shop->vnum = virtual_nr;
  VTable data;
  data.Parse(&fl);

  shop->keeper = data.GetInt("Keeper", -1);
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
  shop->ettiquete = data.GetInt("Etiquette", SKILL_STREET_ETIQUETTE);
  int num_fields = data.NumSubsections("SELLING"), vnum;
  struct shop_sell_data *templist = NULL;
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
    memset(sell, 0, sizeof(shop_sell_data));
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
    shutdown();
  }
  fl.GetLine(buf, 256, FALSE);
  if ((ptr = strchr(buf, '~')) != NULL)   /* take off the '~' if it's there */
    *ptr = '\0';
  Z.name = str_dup(buf);

  fl.GetLine(buf, 256, FALSE);
  if (sscanf(buf, " %d %d %d %d %d %d",
             &Z.top, &Z.lifespan, &Z.reset_mode,
             &Z.security, &Z.connected, &Z.jurisdiction) < 5) {
    fprintf(stderr, "FATAL ERROR: Format error in 5-constant line of %s: Expected six numbers like ' # # # # # #'.", fl.Filename());
    shutdown();
  }

  fl.GetLine(buf, 256, FALSE);
  // This next section reads in the id nums of the players that can edit
  // this zone.
  if (sscanf(buf, "%d %d %d %d %d", &Z.editor_ids[0], &Z.editor_ids[1],
             &Z.editor_ids[2], &Z.editor_ids[3], &Z.editor_ids[4]) != NUM_ZONE_EDITOR_IDS) {
    fprintf(stderr, "FATAL ERROR: Format error in editor id list of %s: Expected five numbers like '# # # # #'", fl.Filename());
    shutdown();
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

    error = 0;
    if (strchr("MVOENPDHL", ZCMD.command) == NULL) { /* a 3-arg command */
      if (sscanf(ptr, " %d %ld %ld ", &tmp, &ZCMD.arg1, &ZCMD.arg2) != 3)
        error = 1;
    } else {
      if (sscanf(ptr, " %d %ld %ld %ld ", &tmp, &ZCMD.arg1, &ZCMD.arg2,
                 &ZCMD.arg3) != 4)
        error = 1;
    }

    ZCMD.if_flag = tmp;

    if (error) {
      fprintf(stderr, "FATAL ERROR: Format error in %s, line %d: '%s'\n",
              fl.Filename(), fl.LineNumber(), buf);
      exit(ERROR_ZONEREAD_FORMAT_ERROR);
    }
    if (ZCMD.command == 'L') {
      ZCMD.command = 'E';
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
      if (vnum_from_non_connected_zone(MOB_VNUM_RNUM(nr)))
        continue;
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
      if (vnum_from_non_connected_zone(MOB_VNUM_RNUM(nr)))
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
      if (vnum_from_non_connected_zone(MOB_VNUM_RNUM(nr)))
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

int vnum_vehicles(char *searchname, struct char_data * ch)
{
  int nr, found = 0;
  for (nr = 0; nr <= top_of_veht; nr++)
  {
    if (isname(searchname, veh_proto[nr].name)
        ||isname(searchname, veh_proto[nr].short_description)) {
      snprintf(buf, sizeof(buf), "%3d. [%5ld] %s\r\n", ++found,
              veh_index[nr].vnum,
              veh_proto[nr].short_description == NULL ? "(BUG)" :
              veh_proto[nr].short_description );
      send_to_char(buf, ch);
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
    if (isname(searchname, mob_proto[nr].player.physical_text.keywords) ||
        isname(searchname, mob_proto[nr].player.physical_text.name)) {
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
          if (IS_OBJ_STAT(&obj_proto[nr], ITEM_GODONLY))
            continue;
          if (vnum_from_non_connected_zone(OBJ_VNUM_RNUM(nr)))
            continue;

          ++found;
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%5ld -%2d] %2d%s +%d %s %s %d%s\r\n",
                  OBJ_VNUM_RNUM(nr),
                  ObjList.CountObj(nr),
                  GET_OBJ_VAL(&obj_proto[nr], 0),
                  wound_arr[GET_OBJ_VAL(&obj_proto[nr], 1)],
                  GET_OBJ_VAL(&obj_proto[nr], 2),
                  obj_proto[nr].text.name,
                  weapon_type[GET_OBJ_VAL(&obj_proto[nr], 3)],
                  GET_OBJ_VAL(&obj_proto[nr], 5),
                  obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
        }
      }
  page_string(ch->desc, buf, 1);
  return (found);
}

int vnum_object_armors(char *searchname, struct char_data * ch)
{
  char xbuf[MAX_STRING_LENGTH];
  int nr, found = 0;
  
  // List everything above 20 combined ballistic and impact.
  for (nr = 0; nr <= top_of_objt; nr++) {
    if (GET_OBJ_TYPE(&obj_proto[nr]) != ITEM_WORN)
      continue;
    if (GET_OBJ_VAL(&obj_proto[nr],0) + GET_OBJ_VAL(&obj_proto[nr],1) <= 20)
      continue;
    
    sprint_obj_mods( &obj_proto[nr], xbuf );
    
    ++found;
    snprintf(buf, sizeof(buf), "[%5ld -%2d] %2d %d %s%s%s\r\n",
            OBJ_VNUM_RNUM(nr),
            ObjList.CountObj(nr),
            GET_OBJ_VAL(&obj_proto[nr], 0),
            GET_OBJ_VAL(&obj_proto[nr], 1),
            obj_proto[nr].text.name,
            xbuf,
            obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
    send_to_char(buf, ch);
  }
  
  // List everything with 20 or less combined ballistic and impact, descending.
  for (int total = 20; total >= 0; total--) {
    for (nr = 0; nr <= top_of_objt; nr++) {
      if (GET_OBJ_TYPE(&obj_proto[nr]) != ITEM_WORN)
        continue;
      if (GET_OBJ_VAL(&obj_proto[nr],0) + GET_OBJ_VAL(&obj_proto[nr],1) != total)
        continue;
      
      sprint_obj_mods( &obj_proto[nr], xbuf );
      
      ++found;
      snprintf(buf, sizeof(buf), "[%5ld -%2d] %2d %d %s%s\r\n",
              OBJ_VNUM_RNUM(nr),
              ObjList.CountObj(nr),
              GET_OBJ_VAL(&obj_proto[nr], 0),
              GET_OBJ_VAL(&obj_proto[nr], 1),
              obj_proto[nr].text.name,
              xbuf);
      send_to_char(buf, ch);
    }
  }
  
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
        if (IS_OBJ_STAT(&obj_proto[nr], ITEM_GODONLY))
          continue;
        if (vnum_from_non_connected_zone(OBJ_VNUM_RNUM(nr)))
          continue;

        ++found;
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%5ld -%2d wt %f] %2d %3d %s%s\r\n",
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
  int nr, found = 0;

  for (nr = 0; nr <= top_of_objt; nr++)
  {
    if (GET_OBJ_TYPE(&obj_proto[nr]) == ITEM_FOCUS
        && !vnum_from_non_connected_zone(OBJ_VNUM_RNUM(nr))) {
      snprintf(buf, sizeof(buf), "%3d. [%5ld -%2d] %s %s +%2d %s%s\r\n", ++found,
              OBJ_VNUM_RNUM(nr),
              ObjList.CountObj(nr),
              vnum_from_non_connected_zone(OBJ_VNUM_RNUM(nr)) ? " " : "*",
              foci_type[GET_OBJ_VAL(&obj_proto[nr], 0)],
              GET_OBJ_VAL(&obj_proto[nr], VALUE_FOCUS_RATING),
              obj_proto[nr].text.name,
              obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
      send_to_char(buf, ch);
    }
  }
  return (found);
}

int vnum_object_type(int type, struct char_data * ch)
{
  int nr, found = 0;

  for (nr = 0; nr <= top_of_objt; nr++)
  {
    if (GET_OBJ_TYPE(&obj_proto[nr]) == type) {
      ++found;
      snprintf(buf, sizeof(buf), "[%5ld -%2d] %s %s%s\r\n",
              OBJ_VNUM_RNUM(nr),
              ObjList.CountObj(nr),
              vnum_from_non_connected_zone(OBJ_VNUM_RNUM(nr)) ? " " : "*",
              obj_proto[nr].text.name,
              obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
      send_to_char(buf, ch);
    }
  }
  return (found);
}

int vnum_object_affectloc(int type, struct char_data * ch)
{
  char xbuf[MAX_STRING_LENGTH];
  int nr, found = 0, mod;

  for( mod = 11; mod >= -11; mod -- )
    for (nr = 0; nr <= top_of_objt; nr++)
    {
      if (IS_OBJ_STAT(&obj_proto[nr], ITEM_GODONLY))
        continue;
      if (vnum_from_non_connected_zone(OBJ_VNUM_RNUM(nr)))
        continue;

      for (int i = 0; i < MAX_OBJ_AFFECT; i++)
        if (obj_proto[nr].affected[i].location == type ) {
          if (obj_proto[nr].affected[i].modifier < mod && mod != -11)
            continue;
          if (obj_proto[nr].affected[i].modifier > mod && mod != 11)
            continue;

          sprint_obj_mods( &obj_proto[nr], xbuf );

          ++found;
          snprintf(buf, sizeof(buf), "[%5ld -%2d] %s%s%s\r\n",
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
  char xbuf[MAX_STRING_LENGTH];
  int nr, found = 0;
  
  for (nr = 0; nr <= top_of_objt; nr++) {
    if (IS_OBJ_STAT(&obj_proto[nr], ITEM_GODONLY))
      continue;
    if (vnum_from_non_connected_zone(OBJ_VNUM_RNUM(nr)))
      continue;
    
    for (int i = 0; i < MAX_OBJ_AFFECT; i++) {
      if (obj_proto[nr].affected[i].modifier != 0 ) {
        sprint_obj_mods( &obj_proto[nr], xbuf );
        
        ++found;
        snprintf(buf, sizeof(buf), "[%5ld -%2d] %s%s%s\r\n",
                OBJ_VNUM_RNUM(nr),
                ObjList.CountObj(nr),
                obj_proto[nr].text.name,
                xbuf,
                obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
        page_string(ch->desc, buf, 1);
        break;
      }
    }
  }
  return (found);
}

int vnum_object_affflag(int type, struct char_data * ch)
{
  int nr, found = 0;

  for (nr = 0; nr <= top_of_objt; nr++)
    if (obj_proto[nr].obj_flags.bitvector.IsSet(type))
    {
      snprintf(buf, sizeof(buf), "[%5ld -%2d] %s%s\r\n",
              OBJ_VNUM_RNUM(nr),
              ObjList.CountObj(nr),
              obj_proto[nr].text.name,
              obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
      send_to_char(buf, ch);
      found++;
      break;
    }
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
  if (!strcmp(searchname,"armorslist"))
    return vnum_object_armors(searchname,ch);
  if (!strcmp(searchname,"magazineslist"))
    return vnum_object_magazines(searchname,ch);
  if (!strcmp(searchname,"focilist"))
    return vnum_object_foci(searchname,ch);
  if (!strcmp(arg1,"objtype"))
    return vnum_object_type(atoi(arg2),ch);
  if (!strcmp(arg1,"affectloc"))
    return vnum_object_affectloc(atoi(arg2),ch);
  if (!strcmp(arg1, "affects"))
    return vnum_object_affects(ch);
  if (!strcmp(arg1,"affflag"))
    return vnum_object_affflag(atoi(arg2),ch);
  
  // Make it easier for people to find specific types of things.
  for (int index = ITEM_LIGHT; index < NUM_ITEMS; index++) {
    if (!str_cmp(searchname, item_types[index]))
      return vnum_object_type(index, ch);
  }
  
  for (nr = 0; nr <= top_of_objt; nr++)
  {
    if (isname(searchname, obj_proto[nr].text.keywords) ||
        isname(searchname, obj_proto[nr].text.name)) {
      snprintf(buf, sizeof(buf), "%3d. [%5ld -%2d] %s %s%s\r\n", ++found,
              OBJ_VNUM_RNUM(nr),
              ObjList.CountObj(nr),
              vnum_from_non_connected_zone(OBJ_VNUM_RNUM(nr)) ? " " : "*",
              obj_proto[nr].text.name,
              obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
      send_to_char(buf, ch);
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
  mob->next = character_list;
  character_list = mob;

  mob->points.physical = mob->points.max_physical;
  mob->points.mental = mob->points.max_mental;

  mob->player.time.birth = time(0);
  mob->player.time.played = 0;
  mob->player.time.logon = time(0);
  mob->char_specials.saved.left_handed = (!number(0, 9) ? 1 : 0);

  mob_index[i].number++;

  affect_total(mob);

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
struct obj_data *read_object(int nr, int type)
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
  } else if (GET_OBJ_TYPE(obj) == ITEM_GUN_MAGAZINE)
    GET_OBJ_VAL(obj, 9) = GET_OBJ_VAL(obj, 0);
  else if (GET_OBJ_TYPE(obj) == ITEM_WEAPON) {
    int real_obj;
    for (int i = ACCESS_LOCATION_TOP; i <= ACCESS_LOCATION_UNDER; i++)
      if (GET_OBJ_VAL(obj, i) > 0 && (real_obj = real_object(GET_OBJ_VAL(obj, i))) > 0) {
        struct obj_data *mod = &obj_proto[real_obj];
        // We know the attachment code will throw a fit if we attach over the top of an 'existing' object, so wipe it out without removing it.
        GET_OBJ_VAL(obj, i) = 0;
        attach_attachment_to_weapon(mod, obj, NULL, i - ACCESS_ACCESSORY_LOCATION_DELTA);
      }
    if (IS_GUN(GET_OBJ_VAL(obj, 3))) {
      if (IS_SET(GET_OBJ_VAL(obj, 10), 1 << MODE_SS))
        GET_OBJ_VAL(obj, 11) = MODE_SS;
      else if (IS_SET(GET_OBJ_VAL(obj, 10), 1 << MODE_SA))
        GET_OBJ_VAL(obj, 11) = MODE_SA;
      else if (IS_SET(GET_OBJ_VAL(obj, 10), 1 << MODE_BF))
        GET_OBJ_VAL(obj, 11) = MODE_BF;
      else if (IS_SET(GET_OBJ_VAL(obj, 10), 1 << MODE_FA)) {
        GET_OBJ_VAL(obj, 11) = MODE_FA;
        GET_OBJ_TIMER(obj) = 10;
      }
    }
  }
  return obj;
}

SPECIAL(traffic);
void spec_update(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  int i;
  char empty_argument = '\0';
  
  // Instead of calculating the random number for every traffic room, just calc once.
  bool will_traffic = (number(0, 6) == 1);

  for (i = 0; i <= top_of_world; i++)
    if (world[i].func != NULL && (will_traffic || world[i].func != traffic))
      world[i].func (NULL, world + i, 0, &empty_argument);

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
  //   ^-- Retard

  if (((++timer * PULSE_ZONE) / PASSES_PER_SEC) >= 60) {
    timer = 0;
    for (i = 0; i <= top_of_zone_table; i++) {
      if (zone_table[i].age < zone_table[i].lifespan &&
          zone_table[i].reset_mode)
        (zone_table[i].age)++;

      if (zone_table[i].age >= MAX(zone_table[i].lifespan,5) &&
          zone_table[i].age < ZO_DEAD && zone_table[i].reset_mode &&
          (zone_table[i].reset_mode == 2 ||
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

#define ZONE_ERROR(message) {log_zone_error(zone, cmd_no, message); last_cmd = 0;}

/* execute the reset command table of a given zone */
void reset_zone(int zone, int reboot)
{
  SPECIAL(fixer);
  int cmd_no, last_cmd = 0, found = 0, no_mob = 0;
  int sig = 0, load = 0;
  static int i;
  struct char_data *mob = NULL;
  struct obj_data *obj, *obj_to, *check;
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
      if ((mob_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1) ||
          (ZCMD.arg2 == 0 && reboot)) {
        mob = read_mobile(ZCMD.arg1, REAL);
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
      break;
    case 'S':                 /* read a mobile into a vehicle */
      if (!veh)
        break;
      if ((mob_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1) ||
          (ZCMD.arg2 == 0 && reboot)) {
        mob = read_mobile(ZCMD.arg1, REAL);
        
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
            if (!mount->worn_by && mount_has_weapon(mount)) {
              mount->worn_by = mob;
              AFF_FLAGS(mob).SetBit(AFF_MANNING);
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
      break;
    case 'U':                 /* mount/upgrades a vehicle with object */
      if (!veh)
        break;
      if ((obj_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1) || (ZCMD.arg2 == 0 && reboot)) {
        obj = read_object(ZCMD.arg1, REAL);
        
        // Special case: Weapon mounts.
        if (GET_OBJ_VAL(obj, 0) == TYPE_MOUNT) {
          switch (GET_OBJ_VAL(obj, 1)) {
            case 1:
              sig = 1;
              // fall through
            case 0:
              load = 10;
              break;
            case 3:
              sig = 1;
              // fall through
            case 2:
              load = 10;
              break;
            case 4:
              sig = 1;
              load = 100;
              break;
            case 5:
              sig = 1;
              load = 25;
              break;
          }
          veh->usedload += load;
          veh->sig -= sig;

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
            veh->usedload += GET_OBJ_WEIGHT(obj);
            
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
            veh->usedload -= GET_OBJ_VAL(GET_MOD(veh, GET_OBJ_VAL(obj, 0)), 1);
            for (int j = 0; j < MAX_OBJ_AFFECT; j++)
              affect_veh(veh, GET_MOD(veh, GET_OBJ_VAL(obj, 0))->affected[j].location, -GET_MOD(veh, GET_OBJ_VAL(obj, 0))->affected[j].modifier);
            extract_obj(GET_MOD(veh, GET_OBJ_VAL(obj, 0)));
          }
          
          GET_MOD(veh, GET_OBJ_VAL(obj, 0)) = obj;
          veh->usedload += GET_OBJ_VAL(obj, 1);
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
        obj = read_object(ZCMD.arg1, REAL);
        obj_to_veh(obj, veh);
        last_cmd = 1;
      } else
        last_cmd = 0;
      break;
    case 'V':                 /* loads a vehicle */
      if ((veh_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1) || (ZCMD.arg2 == 0 && reboot)) {        
        veh = read_vehicle(ZCMD.arg1, REAL);
        veh_to_room(veh, &world[ZCMD.arg3]);
        snprintf(buf, sizeof(buf), "%s has arrived.\r\n", capitalize(GET_VEH_NAME(veh)));
        send_to_room(buf, veh->in_room);
        last_cmd = 1;
      } else
        last_cmd = 0;
      break;
    case 'H':                 /* loads a Matrix file into a host */
      if ((obj_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1) ||
          (ZCMD.arg2 == 0 && reboot)) {
        obj = read_object(ZCMD.arg1, REAL);
        obj->next_content = matrix[ZCMD.arg3].file;
        matrix[ZCMD.arg3].file = obj;
        last_cmd = 1;
      } else
        last_cmd = 0;
      break;
    case 'O':                 /* read an object */
      if ((obj_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1) || (ZCMD.arg2 == 0 && reboot)) {
        obj = read_object(ZCMD.arg1, REAL);
        obj_to_room(obj, &world[ZCMD.arg3]);
        
        act("You blink and realize that $p must have been here the whole time.", TRUE, 0, obj, 0, TO_ROOM);
        
        if (GET_OBJ_TYPE(obj) == ITEM_WORKSHOP && GET_WORKSHOP_GRADE(obj) == TYPE_WORKSHOP) {
          if (GET_WORKSHOP_TYPE(obj) == TYPE_VEHICLE && !ROOM_FLAGGED(obj->in_room, ROOM_GARAGE)) {
            // Warn the builder that they're breaking the game's rules (let it continue since it doesn't harm anything though).
            ZONE_ERROR("Zoneloading a pre-set-up vehicle workshop in a non-GARAGE room violates game convention about vehicle workshop locations. Flag the room as GARAGE.");
          }
          
          // It's a workshop, set it as unpacked already.
          GET_WORKSHOP_IS_SETUP(obj) = 1;
          
          // Handle the room's workshop[] array.
          if (obj->in_room)
            add_workshop_to_room(obj);
        }
        last_cmd = 1;
      } else
        last_cmd = 0;
      break;
    case 'P':                 /* object to object */
      if ((obj_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1) ||
          (ZCMD.arg2 == 0 && reboot)) {
        obj = read_object(ZCMD.arg1, REAL);
        if (!(obj_to = ObjList.FindObj(ZCMD.arg3))) {
          ZONE_ERROR("target obj not found");
          break;
        }
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
        if (!vnum_from_non_connected_zone(GET_OBJ_VNUM(obj)) && !zone_table[zone].connected)
          GET_OBJ_EXTRA(obj).SetBit(ITEM_VOLATILE);
        last_cmd = 1;
      } else
        last_cmd = 0;
      break;
    case 'G':                 /* obj_to_char */
      if (!mob) {
        if (!no_mob)
          ZONE_ERROR("attempt to give obj to non-existent mob");
        break;
      }
      if ((obj_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1) ||
          (ZCMD.arg2 == 0 && reboot)) {
        obj = read_object(ZCMD.arg1, REAL);
        obj_to_char(obj, mob);
        if (!vnum_from_non_connected_zone(GET_OBJ_VNUM(obj)) && !zone_table[zone].connected)
          GET_OBJ_EXTRA(obj).SetBit(ITEM_VOLATILE);
        last_cmd = 1;
      } else
        last_cmd = 0;
      break;
    case 'E':                 /* object to equipment list */
      if (!mob) {
        if (!no_mob) {
          ZONE_ERROR("trying to equip non-existent mob");
          ZCMD.command = '*';
        }
        break;
      }
      if ((obj_index[ZCMD.arg1].number < ZCMD.arg2) || (ZCMD.arg2 == -1) ||
          (ZCMD.arg2 == 0 && reboot)) {
        if (ZCMD.arg3 < 0 || ZCMD.arg3 >= NUM_WEARS) {
          ZONE_ERROR("invalid equipment pos number");
        } else {
          obj = read_object(ZCMD.arg1, REAL);
          equip_char(mob, obj, ZCMD.arg3);
          if (GET_EQ(mob, ZCMD.arg3) != obj) {
            // Equip failure; destroy the object.
            extract_obj(obj);
            last_cmd = 0;
          } else {
            if (!vnum_from_non_connected_zone(GET_OBJ_VNUM(obj)) && !zone_table[zone].connected)
              GET_OBJ_EXTRA(obj).SetBit(ITEM_VOLATILE);
            last_cmd = 1;
            
            // If it's a weapon, reload it.
            if (GET_OBJ_TYPE(obj) == ITEM_WEAPON
                && IS_GUN(GET_WEAPON_ATTACK_TYPE(obj))
                && GET_WEAPON_MAX_AMMO(obj) != -1) {
                               
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
        obj = read_object(ZCMD.arg1, REAL);
        obj_to_char(obj, mob);
        if (!vnum_from_non_connected_zone(GET_OBJ_VNUM(obj)) && !zone_table[zone].connected)
          GET_OBJ_EXTRA(obj).SetBit(ITEM_VOLATILE);
        last_cmd = 1;
      }
      break;
    case 'C': // give mob bio/cyberware
      if (!mob) {
        if (!no_mob)
          ZONE_ERROR("attempt to give obj to non-existent mob");
        break;
      }
      obj = read_object(ZCMD.arg1, REAL);
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
    case 'D':                 /* set state of door */
      if (ZCMD.arg2 < 0 || ZCMD.arg2 >= NUM_OF_DIRS ||
          (world[ZCMD.arg1].dir_option[ZCMD.arg2] == NULL)) {
        ZONE_ERROR("door does not exist");
      } else {
        bool ok = FALSE;
        struct room_data *opposite_room = world[ZCMD.arg1].dir_option[ZCMD.arg2]->to_room;
        if (!opposite_room->dir_option[rev_dir[ZCMD.arg2]]
            || (&world[ZCMD.arg1] != opposite_room->dir_option[rev_dir[ZCMD.arg2]]->to_room)) {
          snprintf(buf, sizeof(buf), "Note: Exits from %ld to %ld do not coincide (zone %d, line %d, cmd %d)",
                  opposite_room->number, world[ZCMD.arg1].number, zone_table[zone].number,
                  ZCMD.line, cmd_no);
          ZCMD.command = '*';
          mudlog(buf, NULL, LOG_ZONELOG, FALSE);
        } else
          ok = TRUE;
        // here I set the hidden flag for the door if hidden > 0
        if (world[ZCMD.arg1].dir_option[ZCMD.arg2]->hidden)
          SET_BIT(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info, EX_HIDDEN);
        if (ok && opposite_room->dir_option[rev_dir[ZCMD.arg2]]->hidden)
          SET_BIT(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->exit_info, EX_HIDDEN);
        // repair all damage
        REMOVE_BIT(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info, EX_DESTROYED);
        world[ZCMD.arg1].dir_option[ZCMD.arg2]->condition =
          world[ZCMD.arg1].dir_option[ZCMD.arg2]->barrier;
        if (ok) {
          opposite_room->dir_option[rev_dir[ZCMD.arg2]]->material = world[ZCMD.arg1].dir_option[ZCMD.arg2]->material;
          opposite_room->dir_option[rev_dir[ZCMD.arg2]]->barrier = world[ZCMD.arg1].dir_option[ZCMD.arg2]->barrier;
          opposite_room->dir_option[rev_dir[ZCMD.arg2]]->condition = world[ZCMD.arg1].dir_option[ZCMD.arg2]->condition;
          REMOVE_BIT(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->exit_info, EX_DESTROYED);
        }
        switch (ZCMD.arg3) {
          // you now only have to set one side of a door
        case 0:
            if (!IS_SET(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info, EX_HIDDEN)
                && IS_SET(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info, EX_CLOSED)) {
              snprintf(buf, sizeof(buf), "The %s to the %s swings open.\r\n",
                      world[ZCMD.arg1].dir_option[ZCMD.arg2]->keyword ? fname(world[ZCMD.arg1].dir_option[ZCMD.arg2]->keyword) : "door",
                      fulldirs[ZCMD.arg2]);
              send_to_room(buf, &world[ZCMD.arg1]);
            }
          REMOVE_BIT(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info, EX_LOCKED);
          REMOVE_BIT(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info, EX_CLOSED);
          if (ok) {
            if (!IS_SET(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->exit_info, EX_HIDDEN)
                && IS_SET(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->exit_info, EX_CLOSED)) {
              snprintf(buf, sizeof(buf), "The %s to the %s swings open.\r\n",
                      opposite_room->dir_option[rev_dir[ZCMD.arg2]]->keyword ? fname(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->keyword) : "door",
                      fulldirs[rev_dir[ZCMD.arg2]]);
              send_to_room(buf, opposite_room);
            }
            REMOVE_BIT(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->exit_info, EX_LOCKED);
            REMOVE_BIT(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->exit_info, EX_CLOSED);
          }
          break;
        case 1:
            if (!IS_SET(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info, EX_HIDDEN)
                && !IS_SET(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info, EX_CLOSED)) {
              snprintf(buf, sizeof(buf), "The %s to the %s swings closed.\r\n",
                      world[ZCMD.arg1].dir_option[ZCMD.arg2]->keyword ? fname(world[ZCMD.arg1].dir_option[ZCMD.arg2]->keyword) : "door",
                      fulldirs[ZCMD.arg2]);
              send_to_room(buf, &world[ZCMD.arg1]);
            }
          SET_BIT(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info, EX_CLOSED);
          REMOVE_BIT(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info, EX_LOCKED);
          if (ok) {
            if (!IS_SET(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->exit_info, EX_HIDDEN)
                && !IS_SET(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->exit_info, EX_CLOSED)) {
              snprintf(buf, sizeof(buf), "The %s to the %s swings closed.\r\n",
                      opposite_room->dir_option[rev_dir[ZCMD.arg2]]->keyword ? fname(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->keyword) : "door",
              fulldirs[rev_dir[ZCMD.arg2]]);
              send_to_room(buf, opposite_room);
            }
            SET_BIT(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->exit_info, EX_CLOSED);
            REMOVE_BIT(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->exit_info, EX_LOCKED);
          }
          break;
        case 2:
            if (!IS_SET(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info, EX_HIDDEN)
                && !IS_SET(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info, EX_CLOSED)) {
              snprintf(buf, sizeof(buf), "The %s to the %s swings closed.\r\n",
                      world[ZCMD.arg1].dir_option[ZCMD.arg2]->keyword ? fname(world[ZCMD.arg1].dir_option[ZCMD.arg2]->keyword) : "door",
                      fulldirs[ZCMD.arg2]);
              send_to_room(buf, &world[ZCMD.arg1]);
            }
          SET_BIT(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info, EX_LOCKED);
          SET_BIT(world[ZCMD.arg1].dir_option[ZCMD.arg2]->exit_info, EX_CLOSED);
          if (ok) {
            if (!IS_SET(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->exit_info, EX_HIDDEN)
                && !IS_SET(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->exit_info, EX_CLOSED)) {
              snprintf(buf, sizeof(buf), "The %s to the %s swings closed.\r\n",
                      opposite_room->dir_option[rev_dir[ZCMD.arg2]]->keyword ? fname(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->keyword) : "door",
                      fulldirs[rev_dir[ZCMD.arg2]]);
              send_to_room(buf, opposite_room);
            }
            SET_BIT(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->exit_info, EX_LOCKED);
            SET_BIT(opposite_room->dir_option[rev_dir[ZCMD.arg2]]->exit_info, EX_CLOSED);
          }
          break;
        }
      }
      last_cmd = 1;
      break;
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

/* for use in reset_zone; return TRUE if zone 'nr' is Free of PC's  */
int zone_is_empty(int zone_nr)
{
  struct descriptor_data *i;

  for (i = descriptor_list; i; i = i->next)
    if (!i->connected && i->character->in_room)
      if (i->character->in_room->zone == zone_nr)
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
  memset(new_world, 0, sizeof(room_data) * (top_of_world_array + world_chunk_size));

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
  memset(new_qst, 0, sizeof(quest_data) * (top_of_quest_array + quest_chunk_size));

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
      shutdown();
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
      fprintf(stderr, "FATAL SYSERR: fread_string: string too large at or near %s\n", error);
      log("FATAL SYSERR: fread_string: string too large (db.c)");
      shutdown();
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
    rslt = new char[length + 1];
    memset(rslt, 0, sizeof(char) * (length + 1));
    strcpy(rslt, buf);
  } else
    rslt = NULL;

  return rslt;
}

/* release memory allocated for a char struct */
void free_char(struct char_data * ch)
{
  int i;
  struct alias *a, *temp, *next;
  struct remem *b, *nextr;
  extern void free_alias(struct alias * a);
  
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
    for (a = GET_ALIASES(ch); a; a = next) {
      next = a->next;
      REMOVE_FROM_LIST(a, GET_ALIASES(ch), next);
      free_alias(a);
    }

    for (b = GET_MEMORY(ch); b; b = nextr) {
      nextr = b->next;
      DELETE_ARRAY_IF_EXTANT(b->mem);
      DELETE_AND_NULL(b);
    }
    
    DELETE_ARRAY_IF_EXTANT(ch->player_specials->obj_complete);
    DELETE_ARRAY_IF_EXTANT(ch->player_specials->mob_complete);
    
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
  }
  
  if (IS_NPC(ch))
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

void free_veh(struct veh_data * veh)
{
  clear_vehicle(veh);
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
  if (!icon->number) {
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

  if (file_to_string(name, temp) < 0)
    return -1;

  DELETE_ARRAY_IF_EXTANT(*buf);

  *buf = str_dup(temp);

  return 0;
}

/* read contents of a text file, and place in buf */
int file_to_string(const char *name, char *buf)
{
  FILE *fl;
  // Expanded from 128 to 129 so a max-length string (127 + '\0') after newline drop (126 + '\0\0') does not overrun when \r\n added (128 + overrun '\0')
  char tmp[129];
  memset(tmp, 0, sizeof(char) * 129);

  *buf = '\0';

  if (!(fl = fopen(name, "r"))) {
    snprintf(tmp, sizeof(tmp), "Error reading %s", name);
    perror(tmp);
    return (-1);
  }
  do {
    fgets(tmp, 128, fl);
    tmp[strlen(tmp) - 1] = '\0';/* take off the trailing \n */
    strcat(tmp, "\r\n");

    if (!feof(fl)) {
      if (strlen(buf) + strlen(tmp) + 1 > MAX_STRING_LENGTH) {
        log("SYSERR: fl->strng: string too big (db.c, file_to_string)");
        *buf = '\0';
        return (-1);
      }
      strcat(buf, tmp);
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
  ch->next = NULL;
  ch->next_fighting = NULL;
  ch->next_in_room = NULL;
  FIGHTING(ch) = NULL;
  ch->char_specials.position = POS_STANDING;
  ch->mob_specials.default_pos = POS_STANDING;
  calc_weight(ch);

  if (GET_PHYSICAL(ch) < 100)
    GET_PHYSICAL(ch) = 100;
  if (GET_MENTAL(ch) < 100)
    GET_MENTAL(ch) = 100;
}

// TODO: this doesn't even clear 50% of the working variables, wat
/* clear ALL the working variables of a char; do NOT free any space alloc'ed */
void clear_char(struct char_data * ch)
{
  memset((char *) ch, 0, sizeof(struct char_data));

  ch->in_veh = NULL;
  ch->in_room = NULL;
  GET_WAS_IN(ch) = NULL;
  GET_POS(ch) = POS_STANDING;
  ch->mob_specials.default_pos = POS_STANDING;
  if (ch->points.max_mental < 1000)
    ch->points.max_mental = 1000;
  ch->player.time.logon = time(0);
}

/* Clear ALL the vars of an object; don't free up space though */
// we do this because generally, objects which are created are off of
// prototypes, and we do not want to delete the prototype strings
void clear_object(struct obj_data * obj)
{
  memset((char *) obj, 0, sizeof(struct obj_data));
  obj->item_number = NOTHING;
  obj->in_room = NULL;
}

void clear_room(struct room_data *room)
{
  memset((char *) room, 0, sizeof(struct room_data));
}

void clear_vehicle(struct veh_data *veh)
{
  memset((char *) veh, 0, sizeof(struct veh_data));
  veh->in_room = NULL;
}

void clear_host(struct host_data *host)
{
  memset((char *) host, 0, sizeof(struct host_data));
}

void clear_icon(struct matrix_icon *icon)
{
  memset((char *) icon, 0, sizeof(struct matrix_icon));
  icon->in_host = NOWHERE;
}

/* returns the real number of the room with given virtual number */
long real_room(long virt)
{
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
long real_mobile(long virt)
{
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

long real_quest(long virt)
{
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

long real_shop(long virt)
{
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

long real_zone(long virt)
{
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

long real_vehicle(long virt)
{
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

long real_host(long virt)
{
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

long real_ic(long virt)
{
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
long real_object(long virt)
{
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
    if (does_player_exist(veh->owner)) {
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
    
    // Step 1: Dump its contents.
    snprintf(buf, sizeof(buf), "Purging contents of vehicle '%s' (%ld), owner %ld (nonexistant).", veh->short_description, veh->idnum, veh->owner);
    log(buf);
    
    while ((vict_veh = veh->carriedvehs)) {
      snprintf(buf, sizeof(buf), "- Found '%s' (%ld) owned by %ld.", vict_veh->short_description, vict_veh->idnum, vict_veh->owner);
      
      // If the vehicle is in a room, disgorge there.
      if (veh->in_room) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " Transferring to room %ld.", veh->in_room->number);
        log(buf);
        veh_from_room(vict_veh);
        veh_to_room(vict_veh, veh->in_room);
      }
      
      // If the vehicle is in another vehicle instead, disgorge there.
      else if (veh->in_veh) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " Transferring to vehicle '%s' (%ld).",
                veh->in_veh->short_description, veh->in_veh->idnum);
        log(buf);
        veh_to_veh(vict_veh, veh->in_veh);
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
      }
    }
    
    // Step 2: Purge the vehicle itself. `veh` now points to garbage.
    extract_veh(veh);
    
    // Critically, we don't iterate prior_veh if we removed veh-- that would skip the next veh in the list.
  }
}

// aka load_vehs, veh_load, and everything else I keep searching for it by --LS
void load_saved_veh()
{
  FILE *fl;
  struct veh_data *veh = NULL, *veh2 = NULL;
  long vnum, owner;
  struct obj_data *obj, *last_obj = NULL, *attach = NULL;
  long veh_room = 0;

  if (!(fl = fopen("veh/vfile", "r"))) {
    log("SYSERR: Could not open vfile for reading.");
    return;
  }
  if (!get_line(fl, buf)) {
    log("SYSERR: Invalid Entry In Vfile.");
    return;
  }
  fclose(fl);
  int num_veh = atoi(buf);
  for (int i = 0; i < num_veh; i++) {
    File file;
    snprintf(buf, sizeof(buf), "veh/%07d", i);
    if (!(file.Open(buf, "r"))) {
      log_vfprintf("Warning: Unable to open vehfile %s for reading. Skipping.", buf);
      continue;
    }

    VTable data;
    data.Parse(&file);
    file.Close();

    owner = data.GetLong("VEHICLE/Owner", 0);

    if ((vnum = data.GetLong("VEHICLE/Vnum", 0)))
      veh = read_vehicle(vnum, VIRTUAL);
    else
      continue;
      
    veh->damage = data.GetInt("VEHICLE/Damage", 0);
    veh->owner = owner;
    veh->idnum = data.GetLong("VEHICLE/Idnum", number(0, INT_MAX));
    veh->spare = data.GetLong("VEHICLE/Spare", 0);
    veh->spare2 = data.GetLong("VEHICLE/InVeh", 0);
    veh->locked = TRUE;
    veh->sub = data.GetLong("VEHICLE/Subscribed", 0);
    veh_room = data.GetLong("VEHICLE/InRoom", 0);
    if (!veh->spare2)
      veh_to_room(veh, &world[real_room(veh_room)]);
    veh->restring = str_dup(data.GetString("VEHICLE/VRestring", NULL));
    veh->restring_long = str_dup(data.GetString("VEHICLE/VRestringLong", NULL));
    int inside = 0, last_in = 0;
    int num_objs = data.NumSubsections("CONTENTS");
    for (int i = 0; i < num_objs; i++) {
      const char *sect_name = data.GetIndexSection("CONTENTS", i);
      snprintf(buf, sizeof(buf), "%s/Vnum", sect_name);
      vnum = data.GetLong(buf, 0);
      if (vnum > 0 && (obj = read_object(vnum, VIRTUAL))) {
        snprintf(buf, sizeof(buf), "%s/Name", sect_name);
        obj->restring = str_dup(data.GetString(buf, NULL));
        snprintf(buf, sizeof(buf), "%s/Cost", sect_name);
        GET_OBJ_COST(obj) = data.GetInt(buf, 0);
        snprintf(buf, sizeof(buf), "%s/Photo", sect_name);
        obj->photo = str_dup(data.GetString(buf, NULL));
        for (int x = 0; x < NUM_VALUES; x++) {
          snprintf(buf, sizeof(buf), "%s/Value %d", sect_name, x);
          GET_OBJ_VAL(obj, x) = data.GetInt(buf, GET_OBJ_VAL(obj, x));
        }
        if (GET_OBJ_TYPE(obj) == ITEM_PHONE && GET_OBJ_VAL(obj, 2))
          add_phone_to_list(obj);
        if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && IS_GUN(GET_OBJ_VAL(obj, 3))) {
          int real_obj;
          for (int q = ACCESS_LOCATION_TOP; q <= ACCESS_LOCATION_UNDER; q++)
            if (GET_OBJ_VAL(obj, q) > 0 && (real_obj = real_object(GET_OBJ_VAL(obj, q))) > 0 && 
               (attach = &obj_proto[real_obj])) {
              // The cost of the item was preserved, but nothing else was. Re-attach the item, then subtract its cost.
              // We know the attachment code will throw a fit if we attach over the top of an 'existing' object, so wipe it out without removing it.
              GET_OBJ_VAL(obj, i) = 0;
              attach_attachment_to_weapon(attach, obj, NULL, i - ACCESS_ACCESSORY_LOCATION_DELTA);
            }
        }
        snprintf(buf, sizeof(buf), "%s/Condition", sect_name);
        GET_OBJ_CONDITION(obj) = data.GetInt(buf, GET_OBJ_CONDITION(obj));
        snprintf(buf, sizeof(buf), "%s/Inside", sect_name);
        inside = data.GetInt(buf, 0);
        snprintf(buf, sizeof(buf), "%s/Front", sect_name);
        obj->vfront = data.GetInt(buf, TRUE);
        snprintf(buf, sizeof(buf), "%s/Timer", sect_name);
        GET_OBJ_TIMER(obj) = data.GetInt(buf, TRUE);
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
          else
            obj_to_veh(obj, veh);
        } else
          obj_to_veh(obj, veh);
        last_in = inside;
        last_obj = obj;
      }
    }
    int num_mods = data.NumFields("MODIS");
    for (int i = 0; i < num_mods; i++) {
      snprintf(buf, sizeof(buf), "MODIS/Mod%d", i);
      obj = read_object(data.GetLong(buf, 0), VIRTUAL);
      GET_MOD(veh, GET_OBJ_VAL(obj, 6)) = obj;
      if (GET_OBJ_VAL(obj, 0) == TYPE_ENGINECUST)
        veh->engine = GET_OBJ_VAL(obj, 2);
      veh->usedload += GET_OBJ_VAL(obj, 1);
      for (int l = 0; l < MAX_OBJ_AFFECT; l++)
        affect_veh(veh, obj->affected[l].location, obj->affected[l].modifier);
    }
    num_mods = data.NumSubsections("GRIDGUIDE");
    for (int i = 0; i < num_mods; i++) {
      struct grid_data *grid = new grid_data;
      memset(grid, 0, sizeof(grid_data));
      const char *sect_name = data.GetIndexSection("GRIDGUIDE", i);
      snprintf(buf, sizeof(buf), "%s/Name", sect_name);
      grid->name = str_dup(data.GetString(buf, NULL));
      snprintf(buf, sizeof(buf), "%s/Room", sect_name);
      grid->room = data.GetLong(buf, 0);
      grid->next = veh->grid;
      veh->grid = grid;
    }
    num_mods = data.NumSubsections("MOUNTS");
    for (int i = 0; i < num_mods; i++) {
      const char *sect_name = data.GetIndexSection("MOUNTS", i);
      snprintf(buf, sizeof(buf), "%s/MountNum", sect_name);
      obj = read_object(data.GetLong(buf, 0), VIRTUAL);
      snprintf(buf, sizeof(buf), "%s/Ammo", sect_name);
      int ammo_qty = data.GetInt(buf, 0);
      if (ammo_qty > 0) {
        struct obj_data *ammo = read_object(OBJ_BLANK_AMMOBOX, VIRTUAL);
        GET_AMMOBOX_QUANTITY(ammo) = ammo_qty;
        snprintf(buf, sizeof(buf), "%s/AmmoType", sect_name);
        GET_AMMOBOX_TYPE(ammo) = data.GetInt(buf, 0);
        snprintf(buf, sizeof(buf), "%s/AmmoWeap", sect_name);
        GET_AMMOBOX_WEAPON(ammo) = data.GetInt(buf, 0);
        ammo->restring = str_dup(get_ammobox_default_restring(ammo));
        obj_to_obj(ammo, obj);
      }
      snprintf(buf, sizeof(buf), "%s/Vnum", sect_name);
      int gun = data.GetLong(buf, 0);
      struct obj_data *weapon;
      if (gun && (weapon = read_object(gun, VIRTUAL))) {
        snprintf(buf, sizeof(buf), "%s/Condition", sect_name);
        GET_OBJ_CONDITION(weapon) = data.GetInt(buf, GET_OBJ_CONDITION(weapon));
        snprintf(buf, sizeof(buf), "%s/Name", sect_name);
        weapon->restring = str_dup(data.GetString(buf, NULL));
        for (int x = 0; x < NUM_VALUES; x++) {
          snprintf(buf, sizeof(buf), "%s/Value %d", sect_name, x);
          GET_OBJ_VAL(weapon, x) = data.GetInt(buf, GET_OBJ_VAL(weapon, x));
        }
        obj_to_obj(weapon, obj);
        veh->usedload += GET_OBJ_WEIGHT(weapon);
      }
      int subbed = 0, damage = 0;
      switch (GET_OBJ_VAL(obj, 1)) {
        case 1:
          subbed = 1;
          // fall through
        case 0:
          damage = 10;
          break;
        case 3:
          subbed = 1;
          // fall through
        case 2:
          damage = 10;
          break;
        case 4:
          subbed = 1;
          damage = 100;
          break;
        case 5:
          subbed = 1;
          damage = 25;
          break;
      }
      veh->usedload += damage;
      veh->sig -= subbed;
      if (veh->mount)
        obj->next_content = veh->mount;
      veh->mount = obj;
    }
  }
  for (veh = veh_list; veh; veh = veh->next)
    if (veh->spare2) {
      for (veh2 = veh_list; veh2 && !veh->in_veh; veh2 = veh2->next)
        if (veh->spare2 == veh2->idnum) {
          veh_to_veh(veh, veh2);
          veh->spare2 = 0;
          veh->locked = FALSE;
        }
      if (!veh->in_veh) {
        // Failure case: The containing vehicle did not exist.
        if (veh_room > 0 && real_room(veh_room) > 0) {
          snprintf(buf, sizeof(buf), "SYSERR: Attempted to restore vehicle %s (%ld) inside nonexistent carrier! Dumping to room %ld.\r\n",
                  veh->name, veh->veh_number, veh_room);
        } else {
          veh_room = RM_SEATTLE_PARKING_GARAGE;
          snprintf(buf, sizeof(buf), "SYSERR: Attempted to restore vehicle %s (%ld) inside nonexistent carrier, and no room was found! Dumping to Seattle Garage (%ld).\r\n",
                  veh->name, veh->veh_number, veh_room);
        }
        mudlog(buf, NULL, LOG_SYSLOG, TRUE);
        veh->spare2 = 0;
        veh_to_room(veh, &world[real_room(veh_room)]);
      }
    }
        
}

void load_consist(void)
{
  struct obj_data *attach;
  File file;
  
  for (int nr = 0; nr <= top_of_world; nr++)
    if (ROOM_FLAGGED(&world[nr], ROOM_STORAGE)) {
      snprintf(buf, sizeof(buf), "storage/%ld", world[nr].number);
      if (!(file.Open(buf, "r")))
        continue;
      log_vfprintf("Restoring storage contents for room %ld (%s)...", world[nr].number, buf);
      VTable data;
      data.Parse(&file);
      file.Close();
      struct obj_data *obj = NULL, *last_obj = NULL;
      int inside = 0, last_in = 0, num_objs = data.NumSubsections("HOUSE");
      long vnum;
      for (int i = 0; i < num_objs; i++) {
        const char *sect_name = data.GetIndexSection("HOUSE", i);
        snprintf(buf, sizeof(buf), "%s/Vnum", sect_name);
        vnum = data.GetLong(buf, 0);
        if (vnum > 0 && (obj = read_object(vnum, VIRTUAL))) {
          snprintf(buf, sizeof(buf), "%s/Name", sect_name);
          obj->restring = str_dup(data.GetString(buf, NULL));
          snprintf(buf, sizeof(buf), "%s/Photo", sect_name);
          obj->photo = str_dup(data.GetString(buf, NULL));
          for (int x = 0; x < NUM_VALUES; x++) {
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
          snprintf(buf, sizeof(buf), "%s/Inside", sect_name);
          
          // Handle weapon attachments.
          if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && IS_GUN(GET_OBJ_VAL(obj, 3))) {
            int real_obj;
            for (int q = ACCESS_LOCATION_TOP; q <= ACCESS_LOCATION_UNDER; q++)
              if (GET_OBJ_VAL(obj, q) > 0 && (real_obj = real_object(GET_OBJ_VAL(obj, q))) > 0 &&
                  (attach = &obj_proto[real_obj])) {
                // We know the attachment code will throw a fit if we attach over the top of an 'existing' object, so wipe it out without removing it.
                GET_OBJ_VAL(obj, i) = 0;
                attach_attachment_to_weapon(attach, obj, NULL, i - ACCESS_ACCESSORY_LOCATION_DELTA);
              }
          }
          
          inside = data.GetInt(buf, 0);
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
            obj_to_room(obj, &world[nr]);

          last_in = inside;
          last_obj = obj;
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
      memset(order, 0, sizeof(shop_order_data));
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

      if (order->item && order->player && order->timeavail && order->price && order->number) {
        order->next = list;
        list = order;
      }
    }
    shop_table[i].order = list;
  }
}

void price_cyber(struct obj_data *obj)
{
  switch (GET_OBJ_VAL(obj, 0)) {
    case CYB_CHIPJACK:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 3) * 1000;
      GET_CYBERWARE_ESSENCE_COST(obj) = 15 + (GET_OBJ_VAL(obj, 3) * 5);
      GET_OBJ_AVAILTN(obj) = 3;
      GET_OBJ_AVAILDAY(obj) = 3;
      break;
    case CYB_DATAJACK:
      GET_OBJ_VAL(obj, 1) = 0;
      if (GET_OBJ_VAL(obj, 3) == DATA_STANDARD) {
        GET_OBJ_COST(obj) = 1000;
        GET_CYBERWARE_ESSENCE_COST(obj) = 20;
        GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 0;
      } else {
        GET_OBJ_COST(obj) = 3000;
        GET_CYBERWARE_ESSENCE_COST(obj) = 30;
        GET_OBJ_AVAILTN(obj) = 5;
        GET_OBJ_AVAILDAY(obj) = 4;
      }
      break;      
    case CYB_DATALOCK:
      GET_CYBERWARE_ESSENCE_COST(obj) = 20;
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
       GET_CYBERWARE_ESSENCE_COST(obj) = 10;
       GET_OBJ_AVAILTN(obj) = 3;
       GET_OBJ_AVAILDAY(obj) = 1;
       break;
     case CYB_MEMORY:
       GET_OBJ_VAL(obj, 1) = 0;
       GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 3) * 150;
       GET_CYBERWARE_ESSENCE_COST(obj) = GET_OBJ_VAL(obj, 3) / 3;
       GET_OBJ_AVAILTN(obj) = 3;
       GET_OBJ_AVAILDAY(obj) = 1;
       break;
     case CYB_TOOTHCOMPARTMENT:
       GET_OBJ_VAL(obj, 1) = 0;
       GET_CYBERWARE_ESSENCE_COST(obj) = 0;
       GET_OBJ_AVAILDAY(obj) = 2;
       if (!GET_OBJ_VAL(obj, 3)) {
         GET_OBJ_COST(obj) = 1500;
         GET_OBJ_AVAILTN(obj) = 2;
       } else {
         GET_OBJ_COST(obj) = 750;
         GET_OBJ_AVAILTN(obj) = 3;
       }
       break;
     case CYB_PHONE:
       GET_OBJ_VAL(obj, 1) = 0;
       GET_CYBERWARE_ESSENCE_COST(obj) = 50;
       GET_OBJ_COST(obj) = 3700;
       GET_OBJ_AVAILTN(obj) = 3;
       GET_OBJ_AVAILDAY(obj) = 1; 
       break;
     case CYB_RADIO:
       GET_CYBERWARE_ESSENCE_COST(obj) = 75;
       GET_OBJ_COST(obj) = 2000 * GET_OBJ_VAL(obj, 1);
       GET_OBJ_AVAILTN(obj) = 2;
       GET_OBJ_AVAILDAY(obj) = 1;
       break;
     case CYB_BONELACING:
       switch (GET_OBJ_VAL(obj, 3)) {
         case BONE_PLASTIC:
           GET_OBJ_VAL(obj, 1) = 0;
           GET_CYBERWARE_ESSENCE_COST(obj) = 50;
           GET_OBJ_COST(obj) = 7500;
           GET_OBJ_AVAILTN(obj) = 5;
           GET_OBJ_AVAILDAY(obj) = 14;
           break;
         case BONE_ALUMINIUM:
           GET_OBJ_VAL(obj, 1) = 0;
           GET_CYBERWARE_ESSENCE_COST(obj) = 115;
           GET_OBJ_COST(obj) = 25000;
           GET_OBJ_AVAILTN(obj) = 5;
           GET_OBJ_AVAILDAY(obj) = 14;
           break;
         case BONE_TITANIUM:
           GET_OBJ_VAL(obj, 1) = 0;
           GET_CYBERWARE_ESSENCE_COST(obj) = 225;
           GET_OBJ_COST(obj) = 75000;
           GET_OBJ_AVAILTN(obj) = 5;
           GET_OBJ_AVAILDAY(obj) = 14;
           break;
         case BONE_KEVLAR:
           GET_OBJ_VAL(obj, 1) = 0;
           GET_CYBERWARE_ESSENCE_COST(obj) = 100;
           GET_OBJ_COST(obj) = 20000;
           GET_OBJ_AVAILTN(obj) = 6;
           GET_OBJ_AVAILDAY(obj) = 21;
           break;
         case BONE_CERAMIC:
           GET_OBJ_VAL(obj, 1) = 0;
           GET_CYBERWARE_ESSENCE_COST(obj) = 150;
           GET_OBJ_COST(obj) = 40000;
           GET_OBJ_AVAILTN(obj) = 6;
           GET_OBJ_AVAILDAY(obj) = 21;
           break;
       }
       break;
    case CYB_FINGERTIP:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_CYBERWARE_ESSENCE_COST(obj) = 10;
      GET_OBJ_COST(obj) = 3000;
      GET_OBJ_AVAILTN(obj) = 3;
      GET_OBJ_AVAILDAY(obj) = 1;
      break;
    case CYB_HANDBLADE:
      GET_OBJ_VAL(obj, 1) = 0;
      if (GET_OBJ_VAL(obj, 3)) {
        GET_CYBERWARE_ESSENCE_COST(obj) = 25;
        GET_OBJ_COST(obj) = 10000;
      } else {
        GET_OBJ_COST(obj) = 7500;
        GET_CYBERWARE_ESSENCE_COST(obj) = 10;
      }
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 5;
      break;
    case CYB_HANDRAZOR:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_OBJ_AVAILDAY(obj) = 3;
      if (IS_SET(GET_OBJ_VAL(obj, 3), 1 << CYBERWEAPON_RETRACTABLE)) {
        GET_CYBERWARE_ESSENCE_COST(obj) = 25;
        GET_OBJ_COST(obj) = 9000;
        GET_OBJ_AVAILTN(obj) = 5;
      } else {
        GET_OBJ_COST(obj) = 7500;
        GET_CYBERWARE_ESSENCE_COST(obj) = 10;
        GET_OBJ_AVAILTN(obj) = 3;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), 1 << CYBERWEAPON_IMPROVED)) {
        GET_OBJ_COST(obj) += 8500;
        GET_OBJ_AVAILTN(obj) = 6;
      }
      break;
    case CYB_HANDSPUR:
      GET_OBJ_VAL(obj, 1) = 0;
      if (GET_OBJ_VAL(obj, 3)) {
        GET_CYBERWARE_ESSENCE_COST(obj) = 30;
        GET_OBJ_COST(obj) = 11500;
        GET_OBJ_AVAILTN(obj) = 5;
      } else {
        GET_OBJ_COST(obj) = 7000;
        GET_CYBERWARE_ESSENCE_COST(obj) = 10;
        GET_OBJ_AVAILTN(obj) = 3;
      }
      GET_OBJ_AVAILDAY(obj) = 3;      
      break;
    case CYB_VOICEMOD:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_CYBERWARE_ESSENCE_COST(obj) = 20;
      GET_OBJ_COST(obj) = 45000;
      GET_OBJ_AVAILTN(obj) = 2;
      GET_OBJ_AVAILDAY(obj) = 1;
      break;
    case CYB_BOOSTEDREFLEXES:
      GET_OBJ_AVAILTN(obj) = 3;
      GET_OBJ_AVAILDAY(obj) = 1;
      switch (GET_OBJ_VAL(obj, 1)) {
        case 1:
          GET_CYBERWARE_ESSENCE_COST(obj) = 50;
          GET_OBJ_COST(obj) = 15000;
          break;
        case 2:
          GET_CYBERWARE_ESSENCE_COST(obj) = 125;
          GET_OBJ_COST(obj) = 40000;
          break;
        case 3:
          GET_CYBERWARE_ESSENCE_COST(obj) = 280;
          GET_OBJ_COST(obj) = 90000;
          break;
      }
      break;
    case CYB_DERMALPLATING:
      GET_CYBERWARE_ESSENCE_COST(obj) = 50 * GET_OBJ_VAL(obj, 1);
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 12;
      switch (GET_OBJ_VAL(obj, 1)) {
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
      if (!GET_OBJ_VAL(obj, 3)) {
        GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) * 15000;
        GET_CYBERWARE_ESSENCE_COST(obj) = GET_OBJ_VAL(obj, 1) * 10;
      } else {
        GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) * 10000;
        GET_CYBERWARE_ESSENCE_COST(obj) = GET_OBJ_VAL(obj, 1) * 20;
      }
      break;
    case CYB_VCR:
      GET_OBJ_AVAILDAY(obj) = 2;
      if (GET_OBJ_VAL(obj, 1) == 1) {
        GET_OBJ_COST(obj) = 12000;
        GET_OBJ_AVAILTN(obj) = 6;
        GET_CYBERWARE_ESSENCE_COST(obj) = 200;
      } else {
        GET_OBJ_AVAILTN(obj) = 8;
        if (GET_OBJ_VAL(obj, 1) == 2) {
          GET_OBJ_COST(obj) = 60000;
          GET_CYBERWARE_ESSENCE_COST(obj) = 300;
        } else {
          GET_OBJ_COST(obj) = 300000;
          GET_CYBERWARE_ESSENCE_COST(obj) = 500;
        }
      }
      break;
    case CYB_WIREDREFLEXES:
      if (GET_OBJ_VAL(obj, 1) == 3) {
        GET_OBJ_COST(obj) = 500000;
        GET_OBJ_AVAILTN(obj) = 8;
        GET_OBJ_AVAILDAY(obj) = 14;
        GET_CYBERWARE_ESSENCE_COST(obj) = 500;
      } else {
        GET_OBJ_AVAILTN(obj) = 4;
        GET_OBJ_AVAILDAY(obj) = 8;
        if (GET_OBJ_VAL(obj, 1) == 1) {
          GET_OBJ_COST(obj) = 55000;
          GET_CYBERWARE_ESSENCE_COST(obj) = 200;
        } else {
          GET_OBJ_COST(obj) = 165000;
          GET_CYBERWARE_ESSENCE_COST(obj) = 300;
        }
      }
      break;
    case CYB_REACTIONENHANCE:
      GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) * 60000;
      GET_CYBERWARE_ESSENCE_COST(obj) = 30 * GET_OBJ_VAL(obj, 1);
      GET_OBJ_AVAILDAY(obj) = 7;
      GET_OBJ_AVAILTN(obj) = 6;
      break;
    case CYB_REFLEXTRIGGER:
      GET_OBJ_COST(obj) = 13000;
      GET_CYBERWARE_ESSENCE_COST(obj) = 20;
      GET_OBJ_AVAILDAY(obj) = 8;
      GET_OBJ_AVAILTN(obj) = 4;
      if (GET_OBJ_VAL(obj, 3))
        GET_OBJ_COST(obj) = (int)(GET_OBJ_COST(obj) * 1.25);
      break;
    case CYB_BALANCEAUG:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_CYBERWARE_ESSENCE_COST(obj) = 40;
      GET_OBJ_COST(obj) = 14000;
      GET_OBJ_AVAILTN(obj) = 8;
      GET_OBJ_AVAILDAY(obj) = 16;
      break;
    case CYB_ORALDART:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_CYBERWARE_ESSENCE_COST(obj) = 25;
      GET_OBJ_COST(obj) = 3600;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 7;
      break;
    case CYB_ORALGUN:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_CYBERWARE_ESSENCE_COST(obj) = 40;
      GET_OBJ_COST(obj) = 5600;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 3;
      break;
    case CYB_ORALSLASHER:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_CYBERWARE_ESSENCE_COST(obj) = 25;
      GET_OBJ_COST(obj) = 10050;
      GET_OBJ_AVAILTN(obj) = 8;
      GET_OBJ_AVAILDAY(obj) = 7;
      break;
    case CYB_ASIST:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_CYBERWARE_ESSENCE_COST(obj) = 0;
      GET_OBJ_COST(obj) = 1000;
      GET_OBJ_AVAILTN(obj) = 3;
      GET_OBJ_AVAILDAY(obj) = 1.5;
      break;
    case CYB_CHIPJACKEXPERT:
      GET_CYBERWARE_ESSENCE_COST(obj) = 10 * GET_OBJ_VAL(obj, 1);
      GET_OBJ_COST(obj) = 5000 * GET_OBJ_VAL(obj, 1);
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 2;
      break;
    case CYB_DATACOMPACT:
      GET_CYBERWARE_ESSENCE_COST(obj) = 5 + (GET_OBJ_VAL(obj, 1) * 5);
      GET_OBJ_COST(obj) = 9500 * GET_OBJ_VAL(obj, 1);
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 2.5;
      break;
    case CYB_ENCEPHALON:
      GET_CYBERWARE_ESSENCE_COST(obj) = 75 * GET_OBJ_VAL(obj, 1);
      GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) > 1 ? 115000 : 4000;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 12;
      break;
    case CYB_MATHSSPU:
      GET_CYBERWARE_ESSENCE_COST(obj) = 5 + (GET_OBJ_VAL(obj, 1) * 5);
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 2.5;
      switch (GET_OBJ_VAL(obj, 1)) {
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
      GET_OBJ_VAL(obj, 1) = 0;
      GET_CYBERWARE_ESSENCE_COST(obj) = 50;
      GET_OBJ_COST(obj) = 10000;
      GET_OBJ_AVAILTN(obj) = 8;
      GET_OBJ_AVAILDAY(obj) = 7;
      break;
    case CYB_BODYCOMPART:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_CYBERWARE_ESSENCE_COST(obj) = 20;
      GET_OBJ_COST(obj) = 5000;
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 2;
      break;
    case CYB_FIN:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_CYBERWARE_ESSENCE_COST(obj) = 30;
      GET_OBJ_COST(obj) = 105000;
      GET_OBJ_AVAILTN(obj) = 5;
      GET_OBJ_AVAILDAY(obj) = 2;
      break;
    case CYB_SKULL:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_CYBERWARE_ESSENCE_COST(obj) = 75;
      if (GET_OBJ_VAL(obj, 3))
        GET_OBJ_COST(obj) = 55000;
      else GET_OBJ_COST(obj) = 35000;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 4;
      break;
    case CYB_TORSO:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_CYBERWARE_ESSENCE_COST(obj) = 150;
      if (GET_OBJ_VAL(obj, 3))
        GET_OBJ_COST(obj) = 120000;
      else GET_OBJ_COST(obj) = 90000;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 4;
      break;
    case CYB_DERMALSHEATHING:
      GET_CYBERWARE_ESSENCE_COST(obj) = 70 * GET_OBJ_VAL(obj, 1);
      GET_OBJ_AVAILDAY(obj) = 14;
      if (GET_OBJ_VAL(obj, 1) == 3) {
        GET_OBJ_AVAILTN(obj) = 8;
        GET_OBJ_COST(obj) = 120000;
      } else {
        GET_OBJ_AVAILTN(obj) = 6;
        if (GET_OBJ_VAL(obj, 1) == 1)
          GET_OBJ_COST(obj) = 24000;
        else GET_OBJ_COST(obj) = 60000;
      }
      if (GET_OBJ_VAL(obj, 3)) {
        GET_CYBERWARE_ESSENCE_COST(obj) += 20;
        GET_OBJ_COST(obj) += 150000;
        GET_OBJ_AVAILTN(obj) += 2;
        GET_OBJ_AVAILDAY(obj) += 4;
      }
      break;
    case CYB_FOOTANCHOR:
      GET_OBJ_VAL(obj, 1) = 0;
      GET_CYBERWARE_ESSENCE_COST(obj) = 40;
      GET_OBJ_COST(obj) = 14000;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 7;
      break;
    case CYB_HYDRAULICJACK:
      GET_CYBERWARE_ESSENCE_COST(obj) = 75 + (25 * GET_OBJ_VAL(obj, 1));
      GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) * 5000;
      GET_OBJ_AVAILTN(obj) = 5;
      GET_OBJ_AVAILDAY(obj) = 6;
      break;
    case CYB_SKILLWIRE:
      GET_CYBERWARE_ESSENCE_COST(obj) = 20 * GET_OBJ_VAL(obj, 1);
      GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) * GET_OBJ_VAL(obj, 3) * 500;
      GET_OBJ_AVAILTN(obj) = GET_OBJ_VAL(obj, 1);
      GET_OBJ_AVAILDAY(obj) = 10;
      break;
    case CYB_MOVEBYWIRE:
      GET_OBJ_COST(obj) = 125000 * (int)pow((double)2, (double)GET_OBJ_VAL(obj, 1));
      GET_CYBERWARE_ESSENCE_COST(obj) = 100 + (150 * GET_OBJ_VAL(obj, 1));
      switch (GET_OBJ_VAL(obj, 1)) {
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
      GET_OBJ_VAL(obj, 1) = 0;
      GET_CYBERWARE_ESSENCE_COST(obj) = 30;
      GET_OBJ_COST(obj) = 10000;
      GET_OBJ_AVAILTN(obj) = 5;
      GET_OBJ_AVAILDAY(obj) = 3;
      break;
    case CYB_SMARTLINK:
      GET_CYBERWARE_ESSENCE_COST(obj) = 50;
      switch (GET_OBJ_VAL(obj, 1)) {
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
      GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) * 20000;
      GET_CYBERWARE_ESSENCE_COST(obj) = GET_OBJ_VAL(obj, 1) * 100;
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 4;     
      break;
    case CYB_EYES:
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = GET_CYBERWARE_ESSENCE_COST(obj) = GET_OBJ_COST(obj) = 0;
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_CAMERA)) {
        GET_OBJ_COST(obj) = 5000;
        GET_OBJ_AVAILTN(obj) = 6;
        GET_OBJ_AVAILDAY(obj) = 1;
        GET_CYBERWARE_ESSENCE_COST(obj) = 40;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_DISPLAYLINK)) {
        GET_OBJ_COST(obj) += 1000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 4);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 1.5);
        GET_CYBERWARE_ESSENCE_COST(obj) += 10;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_FLARECOMP)) {
        GET_OBJ_COST(obj) += 2000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 5);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_ESSENCE_COST(obj) += 10;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_IMAGELINK)) {
        GET_OBJ_COST(obj) += 1600;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 4);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_ESSENCE_COST(obj) += 20;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_LOWLIGHT)) {
        GET_OBJ_COST(obj) += 3000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 4);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 1.5);
        GET_CYBERWARE_ESSENCE_COST(obj) += 20;
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
        GET_CYBERWARE_ESSENCE_COST(obj) += 10;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_RETINALDUPLICATION)) {
        GET_OBJ_COST(obj) += GET_OBJ_VAL(obj, 1) * 25000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 7);
        GET_CYBERWARE_ESSENCE_COST(obj) += 10;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_THERMO)) {
        GET_OBJ_COST(obj) += 3000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 4);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 1.5);
        GET_CYBERWARE_ESSENCE_COST(obj) += 20;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_OPTMAG1)) {
        GET_OBJ_COST(obj) += 2500;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 4);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_ESSENCE_COST(obj) += 20;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_OPTMAG2)) {
        GET_OBJ_COST(obj) += 4000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 4);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_ESSENCE_COST(obj) += 20;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_OPTMAG3)) {
        GET_OBJ_COST(obj) += 6000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 5);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_ESSENCE_COST(obj) += 20;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_ELECMAG1)) {
        GET_OBJ_COST(obj) += 3500;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 5);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_ESSENCE_COST(obj) += 10;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_ELECMAG2)) {
        GET_OBJ_COST(obj) += 7500;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 5);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_ESSENCE_COST(obj) += 10;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_ELECMAG3)) {
        GET_OBJ_COST(obj) += 11000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_ESSENCE_COST(obj) += 10;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_GUN)) {
        GET_OBJ_COST(obj) += 6400;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 6);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 1);
        GET_CYBERWARE_ESSENCE_COST(obj) += 40;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_DATAJACK)) {
        GET_OBJ_COST(obj) += 2200;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 6);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_ESSENCE_COST(obj) += 25;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_DART)) {
        GET_OBJ_COST(obj) += 4200;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 14);
        GET_CYBERWARE_ESSENCE_COST(obj) += 25;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_LASER)) {
        switch (GET_OBJ_VAL(obj, 1)) {
          case 1:
            GET_OBJ_COST(obj) += 3000;
            GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
            GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 3);
            GET_CYBERWARE_ESSENCE_COST(obj) += 20;
            break;
          case 2:
            GET_OBJ_COST(obj) += 5000;
            GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
            GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 7);
            GET_CYBERWARE_ESSENCE_COST(obj) += 30;
            break;
          case 3:
            GET_OBJ_COST(obj) += 8000;
            GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 8);
            GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 14);
            GET_CYBERWARE_ESSENCE_COST(obj) += 50;
            break;
        }
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_LIGHT)) {
        GET_OBJ_COST(obj) += 20;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 4);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 3);
        GET_CYBERWARE_ESSENCE_COST(obj) += 20;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_ULTRASOUND)) {
        GET_OBJ_COST(obj) += 10000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 6);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 2);
        GET_CYBERWARE_ESSENCE_COST(obj) += 50;
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_CYBEREYES)) {
        GET_OBJ_COST(obj) += 4000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 2);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 1);
        GET_CYBERWARE_ESSENCE_COST(obj) = 20 + MAX(0, GET_CYBERWARE_ESSENCE_COST(obj) - 50);
      }
      if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_COSMETIC)) {
        GET_OBJ_COST(obj) += 1000;
        GET_OBJ_AVAILTN(obj) = MAX(GET_OBJ_AVAILTN(obj), 2);
        GET_OBJ_AVAILDAY(obj) = MAX(GET_OBJ_AVAILDAY(obj), 1);
      }
      break;
  }
  switch (GET_OBJ_VAL(obj, 2)) {
    case GRADE_ALPHA:
      GET_OBJ_COST(obj) *= 2;
      GET_CYBERWARE_ESSENCE_COST(obj) = (int) round(GET_CYBERWARE_ESSENCE_COST(obj) * .8);
      break;
    case GRADE_BETA:
      GET_OBJ_COST(obj) *= 4;
      GET_CYBERWARE_ESSENCE_COST(obj) = (int) round(GET_CYBERWARE_ESSENCE_COST(obj) * .6);
      GET_OBJ_AVAILTN(obj) += 5;
      GET_OBJ_AVAILDAY(obj) = (int) round(GET_OBJ_AVAILDAY(obj) * 1.5);
      break;
    case GRADE_DELTA:
      GET_OBJ_COST(obj) *= 8;
      GET_CYBERWARE_ESSENCE_COST(obj) = (int) round(GET_CYBERWARE_ESSENCE_COST(obj) * .5);
      GET_OBJ_AVAILTN(obj) += 9;
      GET_OBJ_AVAILDAY(obj) *= 3;
      break;
  }

}

void price_bio(struct obj_data *obj)
{
  switch (GET_OBJ_VAL(obj, 0)) {
    case BIO_ADRENALPUMP:
      if (GET_OBJ_VAL(obj, 1) == 1)
        GET_OBJ_COST(obj) = 60000;
      else GET_OBJ_COST(obj) = 100000;
      GET_OBJ_VAL(obj, 4) = GET_OBJ_VAL(obj, 1) * 125;
      GET_OBJ_AVAILTN(obj) = 10;
      GET_OBJ_AVAILDAY(obj) = 16;
      break;
    case BIO_CATSEYES:
      GET_OBJ_COST(obj) = 15000;
      GET_OBJ_VAL(obj, 4) = 20;
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 6;
      break;
    case BIO_DIGESTIVEEXPANSION:
      GET_OBJ_COST(obj) = 80000;
      GET_OBJ_VAL(obj, 4) = 100;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 10;
      break;
    case BIO_ENHANCEDARTIC:
      GET_OBJ_COST(obj) = 40000;
      GET_OBJ_VAL(obj, 4) = 60;
      GET_OBJ_AVAILTN(obj) = 5;
      GET_OBJ_AVAILDAY(obj) = 6;
      break;
    case BIO_EXTENDEDVOLUME:
      if (GET_OBJ_VAL(obj, 1) == 1)
        GET_OBJ_COST(obj) = 8000;
      else if (GET_OBJ_VAL(obj, 1) == 2)
        GET_OBJ_COST(obj) = 15000;
      else GET_OBJ_COST(obj) = 25000;
      GET_OBJ_VAL(obj, 4) = (GET_OBJ_VAL(obj, 1) * 10) + 10;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 4;
      break;
    case BIO_METABOLICARRESTER:
      GET_OBJ_COST(obj) = 20000;
      GET_OBJ_VAL(obj, 4) = 60;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 8;
      break;
    case BIO_MUSCLEAUG:
      GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) * 20000;
      GET_OBJ_VAL(obj, 4) = GET_OBJ_VAL(obj, 1) * 40;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 6;
      break;
    case BIO_MUSCLETONER:
      GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) * 25000;
      GET_OBJ_VAL(obj, 4) = GET_OBJ_VAL(obj, 1) * 40;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 6;
      break;
    case BIO_NEPHRITICSCREEN:
      GET_OBJ_COST(obj) = 20000;
      GET_OBJ_VAL(obj, 4) = 40;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 4;
      break;
    case BIO_NICTATINGGLAND:
      GET_OBJ_COST(obj) = 8000;
      GET_OBJ_VAL(obj, 4) = 10;
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 6;
      break;
    case BIO_ORTHOSKIN:
      if (GET_OBJ_VAL(obj, 1) == 1)
        GET_OBJ_COST(obj) = 25000;
      else if (GET_OBJ_VAL(obj, 1) == 2)
        GET_OBJ_COST(obj) = 60000;
      else GET_OBJ_COST(obj) = 100000;
      GET_OBJ_VAL(obj, 4) = GET_OBJ_VAL(obj, 1) * 50;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 8;
      break;
    case BIO_PATHOGENICDEFENSE:
      GET_OBJ_COST(obj) = 24000;
      GET_OBJ_VAL(obj, 4) = GET_OBJ_VAL(obj, 1) * 20;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 4;
      break;
    case BIO_PLATELETFACTORY:
      GET_OBJ_COST(obj) = 30000;
      GET_OBJ_VAL(obj, 4) = 40;
      GET_OBJ_AVAILTN(obj) = 5;
      GET_OBJ_AVAILDAY(obj) = 8;
      break;
    case BIO_SUPRATHYROIDGLAND:
      GET_OBJ_COST(obj) = 50000;
      GET_OBJ_VAL(obj, 4) = 140;
      GET_OBJ_AVAILTN(obj) = 8;
      GET_OBJ_AVAILDAY(obj) = 12;
      break;
    case BIO_SYMBIOTES:
      if (GET_OBJ_VAL(obj, 1) == 1) {
        GET_OBJ_COST(obj) = 15000;
        GET_OBJ_VAL(obj, 4) = 40;
      } else if (GET_OBJ_VAL(obj, 1) == 2) {
        GET_OBJ_COST(obj) = 35000;
        GET_OBJ_VAL(obj, 4) = 70;
      } else {
        GET_OBJ_COST(obj) = 60000;
        GET_OBJ_VAL(obj, 4) = 100;
      }
      GET_OBJ_AVAILTN(obj) = 5;
      GET_OBJ_AVAILDAY(obj) = 10;
      break;
    case BIO_SYNTHACARDIUM:
      if (GET_OBJ_VAL(obj, 1) == 1) {
        GET_OBJ_COST(obj) = 6000;
        GET_OBJ_VAL(obj, 4) = 20;
      } else {
        GET_OBJ_COST(obj) = 15000;
        GET_OBJ_VAL(obj, 4) = 30;
      }
      GET_OBJ_AVAILTN(obj) = 4;
      GET_OBJ_AVAILDAY(obj) = 10;
      break;
    case BIO_TAILOREDPHEREMONES:
      if (GET_OBJ_VAL(obj, 1) == 1) {
        GET_OBJ_COST(obj) = 20000;
        GET_OBJ_VAL(obj, 4) = 40;
      } else {
        GET_OBJ_COST(obj) = 45000;
        GET_OBJ_VAL(obj, 4) = 60;
      }
      GET_OBJ_AVAILTN(obj) = 12;
      GET_OBJ_AVAILDAY(obj) = 14;
      break;
    case BIO_TOXINEXTRACTOR:
      GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) * 45000;
      GET_OBJ_VAL(obj, 4) = GET_OBJ_VAL(obj, 1) * 40;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 4;
      break;
    case BIO_TRACHEALFILTER:
      GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) * 60000;
      GET_OBJ_VAL(obj, 4) = GET_OBJ_VAL(obj, 1) * 40;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 4;
      break;
    case BIO_CEREBRALBOOSTER:
      if (GET_OBJ_VAL(obj, 1) == 1)
        GET_OBJ_COST(obj) = 50000;
      else GET_OBJ_COST(obj) = 110000;
      GET_OBJ_VAL(obj, 4) = GET_OBJ_VAL(obj, 1) * 40;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 14;
      break;
    case BIO_DAMAGECOMPENSATOR:
      if (GET_OBJ_VAL(obj, 1) < 3) {
        GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) * 25000;
        GET_OBJ_AVAILTN(obj) = 6;
      } else if (GET_OBJ_VAL(obj, 1) < 6) {
        GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) * 50000;
        GET_OBJ_AVAILTN(obj) = 10;
      } else {
        GET_OBJ_COST(obj) = GET_OBJ_VAL(obj, 1) * 100000;
        GET_OBJ_AVAILTN(obj) = 12;
      }
      GET_OBJ_VAL(obj, 4) = GET_OBJ_VAL(obj, 1) * 20;
      GET_OBJ_AVAILDAY(obj) = 6;
      break;
    case BIO_PAINEDITOR:
      GET_OBJ_COST(obj) = 60000;
      GET_OBJ_VAL(obj, 4) = 60;
      GET_OBJ_AVAILDAY(obj) = GET_OBJ_AVAILTN(obj) = 6;
      break;
    case BIO_REFLEXRECORDER:
      GET_OBJ_COST(obj) = 25000;
      GET_OBJ_VAL(obj, 4) = 25;
      GET_OBJ_AVAILTN(obj) = 8;
      GET_OBJ_AVAILDAY(obj) = 6;
      break;
    case BIO_SYNAPTICACCELERATOR:
      switch (GET_OBJ_VAL(obj, 1)) {
        case 1:
          GET_OBJ_COST(obj) = 75000;
          GET_OBJ_VAL(obj, 4) = 40;
          break;
        case 2:
          GET_OBJ_COST(obj) = 200000;
          GET_OBJ_VAL(obj, 4) = 100;
      }
      GET_OBJ_AVAILTN(obj) = 6; 
      GET_OBJ_AVAILDAY(obj) = 12;
      break;
    case BIO_THERMOSENSEORGAN:
      GET_OBJ_COST(obj) = 25000;
      GET_OBJ_VAL(obj, 4) = 50;
      GET_OBJ_AVAILTN(obj) = 10;
      GET_OBJ_AVAILDAY(obj) = 12;
      break;
    case BIO_TRAUMADAMPNER:
      GET_OBJ_COST(obj) = 40000;
      GET_OBJ_VAL(obj, 4) = 40;
      GET_OBJ_AVAILTN(obj) = 6;
      GET_OBJ_AVAILDAY(obj) = 8;
      break;
  }
  // Check for cultured.
  if (GET_OBJ_VAL(obj, 0) < BIO_CEREBRALBOOSTER && GET_OBJ_VAL(obj, 2)) {
    GET_OBJ_COST(obj) *= 4;
    GET_OBJ_VAL(obj, 4) = (int) round(GET_OBJ_VAL(obj, 4) * .75);
    GET_OBJ_AVAILTN(obj) += 2;
    GET_OBJ_AVAILDAY(obj) *= 5;
  }
}
