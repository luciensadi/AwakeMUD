#ifndef _STRUCTS_H_
#define _STRUCTS_H_

#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h> // for crypto_pwhash_STRBYTES

#include "types.h"
#include "awake.h"
#include "list.h"
#include "bitfield.h"
#include "utils.h"
#include "playergroup_classes.h"
#include "protocol.h"
#include "chargen.h"

#define SPECIAL(name) \
   int (name)(struct char_data *ch, void *me, int cmd, char *argument)
#define WSPEC(name) \
   int (name)(struct char_data *ch, struct char_data *vict, struct obj_data \
              *weapon, int dam)

/***********************************************************************
 * Structures                                                          *
 **********************************************************************/

#define NOWHERE (-1)
#define NOTHING (-1)
#define NOBODY  (-1)

/* Extra description: used in objects, mobiles, and rooms */
struct extra_descr_data
{
  char *keyword;                 /* Keyword in look/examine          */
  char *description;             /* What to see                      */
  struct extra_descr_data *next; /* Next in list                     */

  extra_descr_data() :
      keyword(NULL), description(NULL), next(NULL)
  {}
}
;

struct text_data
{
  char *keywords;
  char *name;
  char *room_desc;
  char *look_desc;

  text_data() :
      keywords(NULL), name(NULL), room_desc(NULL), look_desc(NULL)
  {}
}
;

/* object-related structures ******************************************/

/* object flags; used in obj_data */
struct obj_flag_data
{
  int value[NUM_VALUES];       /* Values of the item (see list)      */
  byte type_flag;      /* Type of item                       */
  Bitfield wear_flags;  /* Where you can wear it              */
  Bitfield extra_flags;  /* If it hums, glows, etc.            */
  float weight;          /* Weigt what else                    */
  int cost;            /* Value when sold (gp.)              */
  int timer;           /* Timer for object                   */
  Bitfield bitvector;       /* To set chars bits                  */
  byte material;       /* Type of material item is made from */
  byte barrier;        // barrier rating of object if any
  byte condition;      // current barrier rating of the object
  int availtn;
  float availdays;
  float street_index;  // A multiplier on cost if bought through black or grey markets.
  int legality[3];
  long quest_id;
  int attempt;  // ITEM_MONEY: Number of failed cracks. ITEM_WEAPON: Built-in recoil comp.
};

/* Used in obj_file_elem *DO*NOT*CHANGE* */
struct obj_affected_type
{
  byte location;      /* Which ability to change (APPLY_XXX) */
  sbyte modifier;     /* How much it changes by              */
};

/* ================== Memory Structure for Objects ================== */
struct obj_data
{
  rnum_t item_number;         /* Where in data-base                   */
  struct room_data *in_room;      /* Pointer to the room the object is in. */
  struct veh_data *in_veh;
  bool vfront;

  struct obj_flag_data obj_flags; /* Object information                 */
  struct obj_affected_type affected[MAX_OBJ_AFFECT];  /* affects */

  text_data text;
  struct extra_descr_data *ex_description; /* extra descriptions     */
  char *restring;
  char *photo;
  char *graffiti;
  
  char *source_info; // Where in the source books did this come from?
  
  struct char_data *carried_by;   /* Carried by :NULL in room/conta   */
  struct char_data *worn_by;      /* Worn by?                         */
  sh_int worn_on;                 /* Worn where?                      */

  struct obj_data *in_obj;        /* In what object NULL when none    */
  struct obj_data *contains;      /* Contains objects                 */
  struct obj_data *next_content;  /* For 'contains' lists             */
  
  struct char_data *targ;	  /* Data for mounts */
  struct veh_data *tveh;
  
#ifdef USE_DEBUG_CANARIES
  // No sense in initializing the value since it's memset to 0 in most invocations.
  int canary;
#endif
  obj_data() :
      in_room(NULL), in_veh(NULL), ex_description(NULL), restring(NULL), photo(NULL), graffiti(NULL), source_info(NULL),
      carried_by(NULL), worn_by(NULL), in_obj(NULL), contains(NULL), next_content(NULL), targ(NULL), tveh(NULL)
  {}
};
/* ======================================================================= */

/* room-related structures ************************************************/

struct room_direction_data
{
  char *general_description;       /* When look DIR.                   */

  char *keyword;               /* for open/close                       */

  sh_int exit_info;            /* Exit info                            */
  vnum_t key;                 /* Key's number (-1 for no key)         */
  struct room_data *to_room;            /* Where direction leads (NOWHERE)      */
  sh_int key_level;            /* Level of electronic lock             */
  int ward;
  long idnum;
  byte hidden;                 /* target number for exit if hidden     */
  byte material;               /* material                             */
  byte barrier;                /* barrier rating                       */
  byte condition;      // current barrier rating
  vnum_t to_room_vnum;       /* the vnum of the room. Used for OLC   */
  
  const char *go_into_secondperson;
  const char *go_into_thirdperson;
  const char *come_out_of_thirdperson;
  
#ifdef USE_DEBUG_CANARIES
  // No sense in initializing the value since it's memset to 0 in most invocations.
  int canary;
#endif
  room_direction_data() :
      general_description(NULL), keyword(NULL), exit_info(0), key(0), to_room(NULL),
      key_level(0), ward(0), idnum(0), hidden(0), material(0), barrier(0), condition(0),
      to_room_vnum(NOWHERE), go_into_secondperson(NULL), go_into_thirdperson(NULL),
      come_out_of_thirdperson(NULL)
  {}
}
;

/* ================== Memory Structure for room ======================= */
struct room_data
{
  vnum_t number;             /* Rooms number (vnum)                */
  int zone;                 /* Room zone (for resetting)          */
  int  sector_type;            /* sector type (move/hide)            */
  char *name;                  /* Rooms name 'You are ...'           */
  char *description;           /* Shown when entered                 */
  char *night_desc;
  struct extra_descr_data *ex_description; /* for examine/look       */
  struct room_direction_data *dir_option[NUM_OF_DIRS]; /* Directions */
  struct room_direction_data *temporary_stored_exit[NUM_OF_DIRS]; // Stores exits that elevators or spec procs overwrote
  vnum_t matrix;		/* Matrix exit */
  int access;
  int io;
  int trace;
  int bandwidth;
  vnum_t rtg;
  int jacknumber;
  char *address;
  Bitfield room_flags;     /* DEATH,DARK ... etc                 */
  byte blood;         /* mmmm blood, addded by root        */
  byte debris;
  int vision[3];
  int background[4];
  byte spec;            // auto-assigns specs
  sbyte rating;                 // rating of room for various things
  int cover, crowd, type, x, y;
  float z;
  byte light[3];                  /* Number of lightsources in room     */
  byte peaceful;
  byte poltergeist[2];
  byte icesheet[2];
  byte shadow[2];
  byte silence[2];
  SPECIAL(*func);
  
  int staff_level_lock;
  int elevator_number;

  struct obj_data *contents;   /* List of items in room              */
  struct char_data *people;    /* List of NPC / PC in room           */
  struct veh_data *vehicles;
  struct char_data *watching;
  
  struct obj_data *best_workshop[NUM_WORKSHOP_TYPES];
  
#ifdef USE_DEBUG_CANARIES
  // No sense in initializing the value since it's memset to 0 in most invocations.
  int canary;
#endif
  room_data() :
      name(NULL), description(NULL), night_desc(NULL), ex_description(NULL),
      matrix(0), access(0), io(0), trace(0),
      bandwidth(0), jacknumber(0), address(NULL), peaceful(0), func(NULL),
      staff_level_lock(0), contents(NULL), people(NULL), vehicles(NULL), watching(NULL)
  {
    for (int i = 0; i < NUM_OF_DIRS; i++) {
      dir_option[i] = NULL;
      temporary_stored_exit[i] = NULL;
    }
    
    for (int i = 0; i < NUM_WORKSHOP_TYPES; i++)
      best_workshop[i] = NULL;
  }
}
;
/* ====================================================================== */

/* char-related structures ************************************************/

struct spell_data
{
  char *name;                           // spell name
  ubyte type;
  ubyte force;
  ubyte subtype;
  struct spell_data *next;              // pointer to next spell in list

  spell_data() :
      name(NULL), next(NULL)
  {}
};

struct spell_types
{
  const char *name;
  bool physical;
  sh_int category;
  sh_int vector;
  sh_int target;
  sh_int duration;
  sh_int drainpower;
  sh_int draindamage;
};

struct train_data
{
  vnum_t vnum;
  int attribs;
  byte is_newbie;
};

struct teach_data
{
  vnum_t vnum;
  sh_int s[NUM_TEACHER_SKILLS];
  const char *msg;
  sh_int type;
};

struct adept_data
{
  vnum_t vnum;
  sh_int skills[ADEPT_NUMPOWER];
  byte is_newbie;
};

struct spell_trainer
{
  vnum_t teacher;
  int type;
  char name[120];
  int subtype;
  int force;
};

/* memory structure for characters */
struct memory_rec_struct
{
  long id;
  struct memory_rec_struct *next;

  memory_rec_struct() :
      next(NULL)
  {}
};

typedef struct memory_rec_struct memory_rec;

/* This structure is purely intended to be an easy way to transfer */
/* and return information about time (real or mudwise).            */
struct time_info_data
{
  byte minute, hours, day, month, weekday;
  sh_int year;
};

/* These data contain information about a players time data */
struct time_data
{
  time_t birth;    /* This represents the characters age                */
  time_t logon;    /* Time of the last logon (used to calculate played) */
  time_t lastdisc;
  int  played;     /* This is the total accumulated time played in secs */
};

/* general player-related info, usually PC's and NPC's */
struct char_player_data
{
  char *char_name;
  char passwd[crypto_pwhash_STRBYTES + 1];     /* character's password         */

  text_data physical_text;
  text_data astral_text;
  text_data matrix_text;

  char *background;
  char *title;               /* PC / NPC's title                     */
  char *pretitle, *whotitle; /* PC's pre/whotitles                   */
  char *prompt, *matrixprompt;              /* PC's customized prompt               */
  char *poofin, *poofout;    /* PC's poofin/poofout                  */
  
  const char *highlight_color_code;
  const char *email;

  byte sex;                  /* PC / NPC's sex                       */
  byte level;
  long last_room;              /* PC s Hometown (zone)                 */
  struct time_data time;     /* PC's AGE in days                     */
  ush_int weight;              /* PC / NPC's weight                    */
  ush_int height;              /* PC / NPC's height                    */
  byte race;                 /* PC / NPC's race                      */
  byte tradition;            /* PC / NPC's tradition                       */
  ubyte aspect;
  char *host;   /* player host    */

  char_player_data() :
      char_name(NULL), background(NULL), title(NULL), pretitle(NULL), whotitle(NULL),
      prompt(NULL), matrixprompt(NULL), poofin(NULL), poofout(NULL), highlight_color_code(NULL),
      email(NULL), tradition(2), host(NULL)
  {}
}
;

/* Char's abilities.  Used in char_file_u *DO*NOT*CHANGE* */
struct char_ability_data
{
  sbyte attributes[7];
  sh_int mag;
  sh_int bod_index;
  sh_int ess;
  sh_int esshole;
  sh_int highestindex;
  sh_int astral_pool;
  sh_int combat_pool;
  sh_int hacking_pool;
  sh_int hacking_pool_max;
  sh_int hacking_pool_remaining;
  sh_int magic_pool;
  sh_int casting_pool;
  sh_int drain_pool;
  sh_int spell_defense_pool;
  sh_int reflection_pool;  
  sh_int offense_pool;
  sh_int defense_pool;
  sh_int body_pool;
  sh_int control_pool;
  sh_int task_pool[7];
};

struct char_point_data
{
  sh_int mental;
  sh_int max_mental;   /* Max move for PC/NPC                     */
  sh_int physical;
  sh_int max_physical; /* Max hit for PC/NPC                      */

  sh_int ballistic[2];    /* Ballistic armor class for bullets and such */
  sh_int impact[2];       /* Impact armor class for clubs and such   */
  long nuyen;           /* Money carried */
  long bank;           /* Nuyen the char has in a bank account */
  int karma;            /* The experience of the player */
  unsigned int rep;              /* the reputation of the player  (karma earned via runs) */
  unsigned int noto;              /* the reputation of the player  (karma earned via kills) */
  sh_int tke;              /* the reputation of the player  (karma earned total) */
  ush_int sig;
  sbyte init_dice;     /* Bonuses for initiative dice             */
  sh_int init_roll;     /* Total of init roll                      */
  int grade;      /* Initiate Grade        */
  ubyte extrapp;
  ubyte sustained[2];     /* total of sustained spells               */
  int extra;
  ubyte magic_loss;
  ubyte ess_loss;
  ubyte domain;
  ubyte resistpain;
  ubyte lastdamage;
  int track[2];
  byte vision[2];
  ubyte fire[2];
  ubyte binding;
  ubyte reach[2];
  int extras[2];
};

struct char_special_data_saved
{
  sh_int powerpoints;
  bool left_handed;
  sh_int cur_lang;         /* # of language char is currently speaking */
  Bitfield act;    /* act flag for NPC's; player flag for PC's */

  Bitfield affected_by;        /* Bitvector for spells/skills affected by */
  byte skills[MAX_SKILLS+1][2];   /* array of skills plus skill 0         */
  byte powers[ADEPT_NUMPOWER+1][2];
  unsigned char metamagic[META_MAX+1];
  ush_int centeringskill;
  ush_int boosted[3][2];           /* str/qui/bod timeleft/amount		*/
  ubyte masking;
  int points;
  
  bool dirty;
  
  char_special_data_saved() :
      dirty(FALSE)
  {}
};

struct char_special_data
{
  struct veh_data *fight_veh;
  struct char_data *fighting;  /* Opponent                             */
  struct char_data *hunting;   /* Char hunted by this char             */
  struct obj_data *programming; /* Program char is currently designing/programming */
  int conjure[4];
  int num_spirits;
  long idnum;
  bool nervestrike;
  int tempquiloss;
  int cost_breakup;
  int avail_offset;
  int shooting_dir;

  byte position;               /* Standing, fighting, sleeping, etc.   */
  char *defined_position;
  char *leave;             // leave keywords 'mob flies south'
  char *arrive;            // arrive keywords
  int target_mod;

  float carry_weight;           /* Carried weight                       */
  byte carry_items;            /* Number of items carried              */
  sh_int foci;
  sh_int last_healed;
  int timer;                  /* Timer for update                     */
  int actions;
  int coord[3];

  struct veh_data *subscribe;   /* subscriber list */
  struct veh_data *rigging;     /* Vehicle char is controlling remotely */
  struct char_data *mindlink;
  struct spirit_data *spirits;
  
  struct char_special_data_saved saved; /* constants saved in plrfile  */

  char_special_data() :
      fight_veh(NULL), fighting(NULL), hunting(NULL), programming(NULL),
      position(POS_STANDING), defined_position(NULL), leave(NULL), arrive(NULL), subscribe(NULL), rigging(NULL),
      mindlink(NULL), spirits(NULL)
  {
    for (int i = 0; i < 3; i++)
      conjure[i] = 0;
    
    for (int i = 0; i < 3; i++)
      coord[i] = 0;
  }
};

struct player_special_data_saved
{
  int wimp_level;              /* Below this # of hit points, flee!    */
  byte freeze_level;           /* Level of god who froze char, if any  */
  ubyte invis_level;          /* level of invisibility                */
  ubyte incog_level;          /* level of incognito         */
  vnum_t load_room;          /* Which room to place char in          */
  vnum_t last_in;            /* Room to start them in, if 2 hours isn't up */
  vnum_t last_veh;
  Bitfield pref;        /* preference flags for PC's.           */
  ubyte bad_pws;               /* number of bad password attemps       */
  sbyte conditions[3];         /* Drunk, full, thirsty                 */

  ubyte totem;                 /* totem player chooses                 */
  ubyte totemspirit;
  ush_int att_points;              /* attrib points for when you first create */
  ush_int skill_points;            /* starting skill points                   */
  unsigned char force_points;
  unsigned char restring_points;
  int zonenum;
  
  // Chargen data for archetypes.  
  sh_int archetype;
  bool archetypal;

  int system_points;
};

struct player_special_data
{
  struct player_special_data_saved saved;

  struct alias *aliases;       /* Character's aliases                  */
  struct remem *remem;         /* Character's Remembers          */
  long last_tell;              /* idnum of last tell from              */
  sh_int  questnum;
  sh_int *obj_complete;
  sh_int *mob_complete;
  long last_quest[QUEST_TIMER];
  ush_int drugs[NUM_DRUGS+1][7];
  sh_int drug_affect[5];
  ubyte mental_loss;
  ubyte physical_loss;
  ubyte perm_bod;
  struct room_data *watching;
  struct remem *ignored;

  player_special_data() :
      aliases(NULL), remem(NULL), last_tell(0), questnum(0), obj_complete(NULL), 
      mob_complete(NULL), watching(NULL), ignored(NULL)
  {
    for (int i = 0; i < NUM_DRUGS+1; i++)
      for (int j = 0; j < 7; j++)
        drugs[i][j] = 0;
    
    for (int i = 0; i < 5; i++)
      drug_affect[i] = 0;
  }
}
;

/* Specials used by NPCs, not PCs */
struct mob_special_data
{
  byte last_direction;     /* The last direction the monster went     */
  int attack_type;         /* The Attack Type Bitvector for NPC's     */
  byte default_pos;        /* Default position for NPC                */
  long active;
  memory_rec *memory;      /* List of attackers to remember           */
  sh_int mob_skills[10];   /* loaded up skills, I gues this is acceptable */
  int wait_state;          /* Wait state for bashed mobs              */
  int quest_id;
  ush_int value_death_nuyen;
  ush_int value_death_items;
  ush_int value_death_karma;
  ush_int count_death;
  byte alert;
  int alerttime;
  vnum_t spare1, spare2;
  long lasthit;

  mob_special_data() :
      memory(NULL), alert(0), alerttime(0), lasthit(0)
  {
    for (int i = 0; i < 10; i++)
      mob_skills[i] = 0;
  }
}
;

struct veh_follow
{
  struct veh_data *follower;
  struct veh_follow *next;

  veh_follow() :
      follower(NULL), next(NULL)
  {}
};

/* Structure used for chars following other chars */
struct follow_type
{
  struct char_data *follower;
  struct follow_type *next;

  follow_type() :
      follower(NULL), next(NULL)
  {}
};

struct grid_data 
{
  char *name;
  vnum_t room;
  struct grid_data *next;

  grid_data() :
      name(NULL), next(NULL)
  {}
};
struct veh_data
{
  vnum_t veh_number;         /* Where in data-base                   */
  struct room_data *in_room;            /* In what room */

  char *name;
  char *description;              /* When in room (without driver)    */
  char *short_description;        /* when worn/carry/in cont.         */
  char *restring;
  char *long_description;         /* long description when examined   */
  char *restring_long;
  char *inside_description;       /*  Description inside the car      */
  char *rear_description;
  sbyte handling;
  ush_int speed;
  ubyte accel;
  ubyte body;
  ubyte armor;
  sh_int sig;
  ubyte autonav;
  ubyte seating[2];
  float load, usedload;
  ubyte sensor;
  ubyte pilot;
  ubyte engine;
  int cost;
  bool hood;

  ubyte type;
  int damage;
  sh_int cspeed;

  struct veh_follow *followers;
  struct veh_data *following;
  struct char_data *followch;
  struct room_data *lastin[3];

  struct obj_data *mount;
  struct obj_data *mod[NUM_MODS];
  bool sub;

  long idnum;
  long owner;
  long spare, spare2;
  bool locked;
  struct room_data *dest;
  Bitfield flags;

  struct obj_data *contents;
  struct char_data *people;
  struct char_data *rigger;
  struct char_data *fighting;
  struct veh_data *fight_veh;
  struct veh_data *next_veh;
  struct veh_data *next_sub;
  struct veh_data *prev_sub;
  struct veh_data *carriedvehs;
  struct veh_data *in_veh;
  struct veh_data *towing;
  struct grid_data *grid;
  char *leave;
  char *arrive;
  struct veh_data *next;
  

  veh_data() :
      in_room(NULL), name(NULL), description(NULL), short_description(NULL), restring(NULL),
      long_description(NULL), restring_long(NULL), inside_description(NULL), rear_description(NULL),
      followers(NULL), following(NULL), followch(NULL), mount(NULL),
      idnum(0), owner(0), spare(0), spare2(0), dest(NULL),
      contents(NULL), people(NULL), rigger(NULL), fighting(NULL), fight_veh(NULL), next_veh(NULL),
      next_sub(NULL), prev_sub(NULL), carriedvehs(NULL), in_veh(NULL), towing(NULL), grid(NULL), 
      leave(NULL), arrive(NULL), next(NULL)
  {
    for (int i = 0; i < NUM_MODS; i++)
      mod[i] = NULL;
  }
}
;

/* ================== Structure for player/non-player ===================== */
struct char_data
{
  long nr;                            /* Mob's rnum                    */
  // the previous will be DEFUNCT once MobIndex is written
  struct room_data *in_room;                     /* Location */
  struct room_data *was_in_room;                 /* location for linkdead people  */

  struct char_player_data player;       /* Normal data                   */
  struct char_ability_data real_abils;  /* Abilities without modifiers   */
  struct char_ability_data aff_abils;   /* Abils with spells/stones/etc  */
  struct char_point_data points;        /* Points                        */
  struct char_special_data char_specials;      /* PC/NPC specials        */
  struct player_special_data *player_specials; /* PC specials            */
  struct mob_special_data mob_specials;        /* NPC specials           */
  struct veh_data *in_veh;
  bool vfront;
  struct matrix_icon *persona;

  struct spell_queue *squeue;
  struct sustain_data *sustained;
  struct spirit_sustained *ssust;
 
  struct obj_data *equipment[NUM_WEARS];/* Equipment array               */

  struct obj_data *carrying;            /* Head of list                  */
  struct descriptor_data *desc;         /* NULL for mobiles              */
  struct obj_data *cyberware;           /* Head of list of cyberware     */
  struct obj_data *bioware;             /* Head of list of bioware       */

  struct char_data *next_in_room;     /* For room->people - list         */
  struct char_data *next;             /* For either monster or ppl-list  */
  struct char_data *next_fighting;    /* For fighting list               */
  struct char_data *next_in_zone;     /* for zone->people - list         */
  struct char_data *next_in_veh;      /* For veh->people - list          */
  struct char_data *next_watching;

  struct follow_type *followers;        /* List of chars followers       */
  struct char_data *master;             /* Who is char following?        */
  struct spell_data *spells;                     /* linked list of spells          */
  
  Pgroup_data *pgroup;                   /* Data concerning the player group this char is part of. */
  Pgroup_invitation *pgroup_invitations; /* The list of open group invitations associated with this player. */
  
  int congregation_bonus_pool;         /* Bonuses accrued from spending time in a congregation room */
  
  /* Named after 'magic bullet pants', the 'technology' in FPS games that allows you to never have to worry about which mag has how much ammo in it. */
  unsigned short bullet_pants[(END_OF_AMMO_USING_WEAPONS + 1) - START_OF_AMMO_USING_WEAPONS][NUM_AMMOTYPES];
  
  /* Adding a field here? If it's a pointer, add it to utils.cpp's copy_over_necessary_info() to avoid breaking mdelete etc. */

  char_data() :
      in_room(NULL), was_in_room(NULL), player_specials(NULL), in_veh(NULL), persona(NULL), squeue(NULL), sustained(NULL),
      ssust(NULL), carrying(NULL), desc(NULL), cyberware(NULL), bioware(NULL), next_in_room(NULL), next(NULL),
      next_fighting(NULL), next_in_zone(NULL), next_in_veh(NULL), next_watching(NULL), followers(NULL),
      master(NULL), spells(NULL), pgroup(NULL), pgroup_invitations(NULL)
  {
    for (int i = 0; i < NUM_WEARS; i++)
      equipment[i] = NULL;
    
    // Initialize our bullet pants. Note that we index from 0 here.
    for (int wp = 0; wp <= END_OF_AMMO_USING_WEAPONS - START_OF_AMMO_USING_WEAPONS; wp++)
      for (int am = AMMO_NORMAL; am < NUM_AMMOTYPES; am++)
        bullet_pants[wp][am] = 0;
  }
};
/* ====================================================================== */

/* descriptor-related structures ******************************************/

struct txt_block
{
  char *text;
  int aliased;
  struct txt_block *next;

  txt_block() :
      text(NULL), next(NULL)
  {}
}
;

struct txt_q
{
  struct txt_block *head;
  struct txt_block *tail;

  txt_q() :
      head(NULL), tail(NULL)
  {}
}
;

struct ccreate_t
{
  sh_int mode;
  sh_int pr[NUM_CCR_PR_POINTS];
  sh_int force_points;
  sh_int temp;
  int points;
};

struct descriptor_data
{
  int descriptor;                         /* file descriptor for socket         */
  u_short peer_port;            /* port of peer                         */
  char host[HOST_LENGTH+1];       /* hostname                           */
  byte bad_pws;                   /* number of bad pw attemps this login        */
  byte idle_tics;               /* tics idle at password prompt         */
  byte invalid_name;              /* number of invalid name attempts    */
  int connected;                          /* mode of 'connectedness'            */
  int wait;                               /* wait for how many loops            */
  int desc_num;                   /* unique num assigned to desc                */
  time_t login_time;              /* when the person connected          */
  char *showstr_head;
  char *showstr_point;
  char **str;                     /* for the modify-str system          */
  int   max_str;                        /*              -                       */
  long mail_to;                 /* name for mail system                 */
  int   prompt_mode;              /* control of prompt-printing         */
  char inbuf[MAX_RAW_INPUT_LENGTH];  /* buffer for raw input            */
  char last_input[MAX_INPUT_LENGTH]; /* the last input                  */
  char last_tell[MAX_INPUT_LENGTH];
  char small_outbuf[SMALL_BUFSIZE];  /* standard output buffer          */
  char *output;                 /* ptr to the current output buffer     */
  int bufptr;                     /* ptr to end of current output               */
  int bufspace;                 /* space left in the output buffer      */
  struct txt_block *large_outbuf; /* ptr to large buffer, if we need it */
  struct txt_q input;             /* q of unprocessed input             */
  struct char_data *character;  /* linked to char                       */
  struct char_data *original;   /* original char if switched            */
  struct descriptor_data *snooping; /* Who is this char snooping        */
  struct descriptor_data *snoop_by; /* And who is snooping this char    */
  struct descriptor_data *next; /* link to next descriptor              */
  struct ccreate_t ccr;
  int invalid_command_counter;
  
  listClass<const char *> message_history[NUM_COMMUNICATION_CHANNELS];

  // all this from here down is stuff for on-line creation
  int edit_mode;                /* editing sub mode */
  bool edit_convert_color_codes; /* if this is true, display color codes in descs as ^^ for copy-paste */
  long edit_number;              /* virtual num of thing being edited */
  long edit_number2;             /* misc number for editing */
  long edit_number3;             /* misc number for editing */
  int edit_zone;                /* which zone object is part of      */
  int iedit_limit_edits;        /* Used in iedit to let you cut out of g-menus early. */
  void **misc_data;             /* misc data, usually for extra data crap */
  struct obj_data *edit_obj;    /* iedit */
  struct room_data *edit_room;  /* redit */
  struct char_data *edit_mob;   /* medit */
  struct quest_data *edit_quest;/* qedit */
  struct shop_data *edit_shop;  /* sedit */
  struct zone_data *edit_zon;   /* zedit */
  struct reset_com *edit_cmd;   /* zedit command */
  struct veh_data *edit_veh;    /* vedit */
  struct host_data *edit_host;  /* hedit */
  struct matrix_icon *edit_icon; /* icedit */
  struct help_data *edit_helpfile;
  
  Playergroup *edit_pgroup; /* playergroups */
  
  protocol_t *pProtocol;
  
  // this is for spell creation

  descriptor_data() :
      showstr_head(NULL), showstr_point(NULL), str(NULL), output(NULL),
      large_outbuf(NULL), character(NULL), original(NULL), snooping(NULL),
      snoop_by(NULL), next(NULL), invalid_command_counter(0), iedit_limit_edits(0), misc_data(NULL),
      edit_obj(NULL), edit_room(NULL), edit_mob(NULL), edit_quest(NULL), edit_shop(NULL),
      edit_zon(NULL), edit_cmd(NULL), edit_veh(NULL), edit_host(NULL), edit_icon(NULL),
      edit_helpfile(NULL), edit_pgroup(NULL), pProtocol(NULL)
  {
    // Zero out the communication history for all channels.
    for (int channel = 0; channel < NUM_COMMUNICATION_CHANNELS; channel++)
      message_history[channel] = listClass<const char *>();
  }
}
;


/* other miscellaneous structures ***************************************/

struct msg_type
{
  const char *attacker_msg;  /* message to attacker */
  const char *victim_msg;    /* message to victim   */
  const char *room_msg;      /* message to room     */

  msg_type() :
      attacker_msg(NULL), victim_msg(NULL), room_msg(NULL)
  {}
}
;

struct message_type
{
  struct msg_type die_msg;       /* messages when death                  */
  struct msg_type miss_msg;      /* messages when miss                   */
  struct msg_type hit_msg;       /* messages when hit                    */
  struct msg_type god_msg;       /* messages when hit on god             */
  struct message_type *next;     /* to next messages of this kind.       */
  
  message_type():
    next(NULL)
  {}
};

struct message_list
{
  int a_type;                    /* Attack type                          */
  int number_of_attacks;         /* How many attack messages to chose from. */
  struct message_type *msg;      /* List of messages.                    */
  
  message_list() :
    msg(NULL)
  {}
};

struct weather_data
{
  int pressure;       /* How is the pressure ( Mb ) */
  int change;         /* How fast and what way does it change. */
  int sky;            /* How is the sky. */
  int sunlight;       /* And how much sun. */
  int moonphase;      /* What is the Moon Phae */
  int lastrain;       /* cycles since last rain */
};

/* element in monster and object index-tables   */
struct index_data
{
  vnum_t   vnum;    /* virtual number of this mob/obj           */
  long   number;     /* number of existing units of this mob/obj */
  SPECIAL(*func);
  SPECIAL(*sfunc);
  WSPEC(*wfunc);

  index_data() :
      func(NULL), sfunc(NULL), wfunc(NULL)
  {}
}
;

struct remem
{
  long idnum;
  char *mem;
  struct remem *next;

  remem() :
    mem(NULL), next(NULL)
  {}
};

struct phone_data
{
  int number;
  bool connected;
  vnum_t rtg;
  struct phone_data *dest;  
  struct obj_data *phone;
  struct matrix_icon *persona;
  struct phone_data *next;
  phone_data() :
      number(0), connected(0), rtg(1102), dest(NULL), phone(NULL), persona(NULL), next(NULL)
  {}
};

struct skill_data {
  char name[50];
  sh_int attribute;
  bool type;
  bool requires_magic;
};

struct part_data {
  char name[30];
  sh_int tools;
  unsigned char software;
  signed char design;
};

struct program_data {
  char name[30];
  unsigned char multiplier;
};

struct drug_data {
  char name[9];
  unsigned char power;
  unsigned char level;
  unsigned char mental_addiction;
  unsigned char physical_addiction;
  unsigned char tolerance;
  unsigned char edge_preadd;
  unsigned char edge_posadd;
  unsigned char fix;
};

struct spirit_table {
  char name[50];
  vnum_t vnum;
};

struct spirit_data {
  sh_int type;
  signed char force;
  signed char services;
  int id;
  bool called;
  struct spirit_data *next;
  spirit_data() :
    called(0), next(NULL)
  {}
};

struct order_data {
  char name[50];
  void (*func)(struct char_data *ch, struct char_data *spirit, struct spirit_data *spiritdata, char *arg);
  unsigned char type;
};

struct sustain_data {
  ush_int spell;
  unsigned char subtype;
  unsigned char force;
  unsigned char success;
  int idnum;
  int time;
  unsigned char drain;
  bool caster;
  struct obj_data *focus;
  struct char_data *spirit;
  struct char_data *other;
  struct sustain_data *next;
  sustain_data() :
    spell(0), subtype(0), force(0), success(0), idnum(0), time(0), drain(0), caster(0),
    focus(NULL), spirit(NULL), other(NULL), next(NULL)
  {}
};

struct attack_hit_type
{
  const char *singular;
  const char *plural;
  const char *different;
};

struct spirit_sustained
{
  int type;
  bool caster;
  struct char_data *target;
  struct spirit_sustained *next;
  spirit_sustained() :
    target(NULL), next(NULL)
  {}
};

struct mod_data
{
  const char *name;
  int tools;
};

struct spell_queue
{
  ush_int spell;
  ush_int sub;
  unsigned char force;
  char *arg;

  spell_queue() :
   arg(NULL)
  {}
};

struct ammo_data
{
  const char *name;
  unsigned char tn;
  float time;
  float weight;
  unsigned char cost;
  float street_index;
};

/* Combat data. */
struct combat_data
{
  // Generic combat data.
  int modifiers[NUM_COMBAT_MODIFIERS];
  bool too_tall;
  int skill;
  int tn;
  int dice;
  int successes;
  
  // Weapon / unarmed damage data.
  int dam_type;
  bool is_physical;
  int power;
  int damage_level; // Light/Med/etc
  
  // Gun data.
  bool weapon_is_gun;
  bool weapon_has_bayonet;
  int burst_count;
  int recoil_comp;
  
  // Cyberware data.
  int climbingclaws;
  int fins;
  int handblades;
  int handrazors;
  int improved_handrazors;
  int handspurs;
  int footanchors;
  int bone_lacing_power;
  int num_cyberweapons;
  
  // Pointers.
  struct char_data *ch;
  struct veh_data *veh;
  struct obj_data *weapon;
  struct obj_data *magazine;
  struct obj_data *gyro;
  
  combat_data(struct char_data *character, struct obj_data *weap) :
    too_tall(FALSE), skill(0), tn(0), dice(0), successes(0), dam_type(0), is_physical(FALSE), power(0), damage_level(0),
    weapon_is_gun(FALSE), weapon_has_bayonet(FALSE), burst_count(0), recoil_comp(0), climbingclaws(0), fins(0),
    handblades(0), handrazors(0), improved_handrazors(0), handspurs(0), footanchors(0), bone_lacing_power(0), num_cyberweapons(0),
    ch(NULL), veh(NULL), weapon(NULL), magazine(NULL), gyro(NULL)
  {
    for (int i = 0; i < NUM_COMBAT_MODIFIERS; i++)
      modifiers[i] = 0;
    
    ch = character;
    weapon = weap;
    
    weapon_is_gun = (weapon 
                     && IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon)) 
                     && (GET_WEAPON_SKILL(weapon) >= SKILL_PISTOLS
                         && GET_WEAPON_SKILL(weapon) <= SKILL_ASSAULT_CANNON));
    if (weapon_is_gun)
      magazine = weapon->contains;
  }
};

struct help_data {
  // title: varchar 128
  const char *original_title;
  const char *title;
  // body: text
  char *body;
  int category; // currently unused
  // links: varchar 200
  char *links; // currently unused
  
  // Set title_to_keep to NULL for a new helpfile.
  help_data(const char *title_to_keep_for_sql) :
    original_title(NULL), title(NULL), body(NULL), category(0), links(NULL)
  {
    original_title = title_to_keep_for_sql;
  }
};

struct preference_bit_struct {
  const char *name; // the name of the bit (ex: "PACIFY")
  bool staff_only;  // only displays to level > 1?
  bool on_off;      // true for ONOFF, false for YESNO
};

/* ban struct */
struct ban_list_element
{
  char site[BANNED_SITE_LENGTH+1];
  int  type;
  time_t date;
  char name[MAX_NAME_LENGTH+1];
  struct ban_list_element *next;
};

#endif
