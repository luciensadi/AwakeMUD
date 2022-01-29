/* ************************************************************************
*   File: db.h                                          Part of CircleMUD *
*  Usage: header file for database handling                               *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#ifndef _db_h_
#define _db_h_

#include <stdio.h>
#include "bitfield.h"

/*
 * Current versions are defined in VERSION_HOUSE_FILE and VERSION_VEH_FILE
 * Making changes to the formats you need to change the current version macro
 * to the new version, which is what is written to file,  add a new macro for the new
 * version, link current macro to it, document the format changes here and handle
 * them accordingly in code.
 */
 
#define VERSION_HOUSE_FILE    VERSION_HOUSE_FILEV1
#define VERSION_VEH_FILE           VERSION_VEH_FILEV1

// Saves the objects in reverse with the container last. Then reads objects in turn, saves
// anything with a higher nesting level than 0 in a vector, then once a container is found
// places all objects with nesting level +1 into it.
#define VERSION_HOUSE_FILEV1    1
// Original logic, saves containers last and reads them first, then places subsequent objects
// with nesting level +1 into them. No version number written in file so this defaults to 0.
#define VERSION_HOUSE_FILEV0    0

// Saves the objects in reverse with the container last. Then reads objects in turn, saves
// anything with a higher nesting level than 0 in a vector, then once a container is found
// places all objects with nesting level +1 into it.
#define VERSION_VEH_FILEV1    1
// Original logic, saves containers last and reads them first, then places subsequent objects
// with nesting level +1 into them. No version number written in file so this defaults to 0.
#define VERSION_VEH_FILEV0    0

/* names of various files and directories */
#define INDEX_FILE      "index"         /* index of world files         */
#define MINDEX_FILE     "index.mini"    /* ... and for mini-mud-mode    */
#define ICINDEX_FILE    "index.ic"    /* For matrix IC */
#define WLD_PREFIX      "world/wld"     /* room definitions             */
#define MOB_PREFIX      "world/mob"     /* monster prototypes           */
#define OBJ_PREFIX      "world/obj"     /* object prototypes            */
#define ZON_PREFIX      "world/zon"     /* zon defs & command tables    */
#define SHP_PREFIX      "world/shp"     /* shop definitions             */
#define QST_PREFIX      "world/qst"     /* autoquests                   */
#define VEH_PREFIX      "world/veh"     /* vehicles */
#define MTX_PREFIX      "world/mtx"     /* Matrix			*/

#define CREDITS_FILE    "text/credits"  /* for the 'credits' command    */
#define NEWS_FILE       "text/news"     /* for the 'news' command       */
#define MOTD_FILE       "text/motd"     /* messages of the day / mortal */
#define IMOTD_FILE      "text/imotd"    /* messages of the day / immort */
#define HELP_KWRD_FILE  "text/helplist" /* for HELP <keywrd>           */
#define HELP_PAGE_FILE  "text/help"     /* for HELP <CR>                */
#define INFO_FILE       "text/info"     /* for INFO                     */
#define IMMLIST_FILE    "text/immlist"  /* for IMMLIST                  */
#define BACKGROUND_FILE "text/background" /* for the background story   */
#define POLICIES_FILE   "text/policies" /* player policies/rules        */
#define HANDBOOK_FILE   "text/handbook" /* handbook for new immorts     */
#define WIZHELP_FILE    "text/wizhelp" /* for WIZHELP <keywrd> */

#define IDEA_FILE       "text/ideas"/* for the 'idea'-command       */
#define TYPO_FILE       "text/typos"/*         'typo'               */
#define BUG_FILE        "text/bugs" /*         'bug'                */
#define PRAISE_FILE     "text/praise" /*       'praise'             */
#define MESS_FILE       "misc/messages" /* damage messages              */
#define SOCMESS_FILE    "misc/socials"  /* messgs for social acts       */

#define PLAYER_FILE     "etc/players"   /* the player database          */
#define MAIL_FILE       "etc/plrmail"   /* for the mudmail system       */
#define BAN_FILE        "etc/badsites"  /* for the siteban system       */
#define HCONTROL_FILE   "etc/houses"  /* for the house system         */
#define PLR_PREFIX "etc/pfiles"
#define SLASH           "/"
/* change these if you want to put all files in the same directory (or if
   you just like big file names
*/
#define PLR_SUFFIX ""

/* new bitvector data for use in player_index_element */
#define PINDEX_DELETED  11 /* deleted player */
#define PINDEX_NODELETE  12 /* protected player */
#define PINDEX_SELFDELETE 11 /* player is selfdeleting*/


/* public procedures in db.c */
void    DBInit();
int     create_entry(char *name);
void ensure_entry(struct char_data *player);
void    zone_update(void);
long     real_room(long virt);
char    *fread_string(FILE *fl, char *error);
long    get_id_by_name(char *name);
char    *get_name_by_id(long id);
int  load_char(char *name, struct char_data *ch);
struct char_data *read_mobile(int nr, int type);
struct veh_data *read_vehicle(int nr, int type);
long     real_mobile(long virt);
int     vnum_mobile(char *searchname, struct char_data *ch);
void    clear_char(struct char_data *ch);
void    reset_char(struct char_data *ch);
void    free_char(struct char_data *ch);

struct obj_data *create_obj(void);
void    clear_object(struct obj_data *obj);
void    clear_room(struct room_data *room);
void    free_obj(struct obj_data *obj);
void    free_room(struct room_data *room);
long     real_object(long virt);
struct obj_data *read_object(int nr, int type);
int     vnum_object(char *searchname, struct char_data *ch);

void free_veh(struct veh_data *veh);
void clear_vehicle(struct veh_data *veh);
long real_vehicle(long virt);

void free_host(struct host_data *host);
void clear_host(struct host_data *veh);
long real_host(long virt);
void free_icon(struct matrix_icon *icon);
void clear_icon(struct matrix_icon *icon);
long real_ic(long virt);
struct matrix_icon *read_ic(int nr, int type);

long     real_shop(long virt);
long     real_quest(long virt);
long  real_zone(long virt);

/* structure for the reset commands */
struct reset_com
{
  char command;   /* current command                      */

  bool if_flag;        /* if TRUE: exe only if preceding exe'd */
  long arg1;           /*                                      */
  long arg2;           /* Arguments to the command             */
  long arg3;           /*                                      */
  int line;            /* line number this command appears on  */

  /*
       *  Commands:              *
       *  'M': Read a mobile     *
       *  'O': Read an object    *
       *  'G': Give obj to mob   *
       *  'P': Put obj in obj    *
       *  'G': Obj to char       *
       *  'E': Obj to char equip *
       *  'D': Set state of door *
  */
};

/* zone definition structure. for the 'zone-table'   */
struct zone_data
{
  char *name;              /* name of this zone                  */
  int lifespan;            /* how long between resets (minutes)  */
  int age;                 /* current age of this zone (minutes) */
  int top;                 /* upper limit for rooms in this zone */
  int security;            /* security rating for node */
  int connected;           /* zone is connected */

  int reset_mode;          /* conditions for reset (see below)   */
  int number;              /* virtual number of this zone        */
  int num_cmds;         // number of commands in the table
  int editor_ids[NUM_ZONE_EDITOR_IDS];       // array of zone editors
  int jurisdiction;       // Abstract zone this zone belongs to
  int is_pghq;
  struct reset_com *cmd;   /* command table for reset            */

  /*
       *  Reset mode:                              *
       *  0: Don't reset, and don't update age.    *
       *  1: Reset if no PC's are located in zone. *
       *  2: Just reset.                           *
  */
};

struct help_index_data
{
  char *keyword;
  char *filename;
  struct help_index_data *next;
  help_index_data ():
    keyword(NULL), filename(NULL), next(NULL)
  {}
};

/* global buffering system */

#ifdef __DB_CC__
char    buf[MAX_STRING_LENGTH];
char    buf1[MAX_STRING_LENGTH];
char    buf2[MAX_STRING_LENGTH];
char    buf3[MAX_STRING_LENGTH]; // NOW WITH EVEN MOAR BUFFERS
char    arg[MAX_STRING_LENGTH];
#else
extern struct room_data *world;
extern rnum_t top_of_world;

extern struct zone_data *zone_table;
extern rnum_t top_of_zone_table;

extern struct descriptor_data *descriptor_list;
extern struct char_data *character_list;
extern struct player_special_data dummy_mob;
extern struct player_index_element *player_table;
extern int top_of_p_table;

extern struct index_data *mob_index;
extern struct char_data *mob_proto;
extern rnum_t top_of_mobt;
extern int top_of_mob_array;
extern int mob_chunk_size;

extern struct index_data *obj_index;
extern struct obj_data *obj_proto;
extern rnum_t top_of_objt;
extern int top_of_obj_array;
extern int obj_chunk_size;

extern struct shop_data *shop_table;
extern rnum_t top_of_shopt;
extern int top_of_shop_array;

extern struct quest_data *quest_table;
extern rnum_t top_of_questt;
extern int top_of_quest_array;

extern rnum_t top_of_world;
extern int top_of_world_array;
extern int world_chunk_size;

extern struct veh_data *veh_proto;
extern int      top_of_veht;
extern struct index_data *veh_index;
extern struct veh_data *veh_list;
extern struct phone_data *phone_list;

extern struct host_data *matrix;
extern int top_of_matrix;
extern struct matrix_icon *icon_list;
extern struct matrix_icon *ic_proto;
extern struct index_data *ic_index;
extern int top_of_ic;
extern int market[5];

extern char buf[MAX_STRING_LENGTH];
extern char buf1[MAX_STRING_LENGTH];
extern char buf2[MAX_STRING_LENGTH];
extern char buf3[MAX_STRING_LENGTH];
extern char arg[MAX_STRING_LENGTH];
extern char *credits;
extern char *news;
extern char *info;
extern char *motd;
extern char *imotd;
extern char *immlist;
extern char *policies;
extern char *handbook;
extern char *background;
extern char *hindex;


extern int olc_state;
#endif

#ifndef __CONFIG_CC__
extern const char *OK;
extern const char	*TOOBUSY;
#endif

#endif

extern void save_etext(struct char_data *ch);
