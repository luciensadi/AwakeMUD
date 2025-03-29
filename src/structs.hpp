#ifndef _STRUCTS_H_
#define _STRUCTS_H_

#define NOWHERE (-1)
#define NOTHING (-1)
#define NOBODY  (-1)

#define SPECIAL(name) \
   int (name)(struct char_data *ch, void *me, int cmd, char *argument)
#define WSPEC(name) \
   int (name)(struct char_data *ch, struct char_data *vict, struct obj_data \
              *weapon, int dam)

#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#include <vector>

#ifndef NOCRYPT
#include <sodium.h> // for crypto_pwhash_STRBYTES
#endif

#include "types.hpp"
#include "awake.hpp"
#include "list.hpp"
#include "bitfield.hpp"
#include "utils.hpp"
#include "playergroup_classes.hpp"
#include "protocol.hpp"
#include "chargen.hpp"
#include "vision_overhaul.hpp"
#include "drugs.hpp"

class Faction;
class ApartmentComplex;
class Apartment;
class ApartmentRoom;
class PCExDesc;
#ifdef TEMPORARY_COMPILATION_GUARD
class MinigameContainer;
class MinigameModule;
#endif

/***********************************************************************
 * Structures                                                          *
 **********************************************************************/

#define ZERO_OUT_ARRAY(array_name, array_size) { for (int array_zero_idx = 0; array_zero_idx < (array_size); array_zero_idx++) { (array_name)[array_zero_idx] = 0; } }

class IgnoreData;

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
  // Adding more strings? Make sure you update mclone, medit, etc.

  text_data() :
      keywords(NULL), name(NULL), room_desc(NULL), look_desc(NULL)
  {}
}
;

/* object-related structures ******************************************/

/* object flags; used in obj_data */
struct obj_flag_data
{
  int value[NUM_OBJ_VALUES];       /* Values of the item (see list)      */
  byte type_flag;      /* Type of item                       */
  Bitfield wear_flags;  /* Where you can wear it              */
  Bitfield extra_flags;  /* If it hums, glows, etc.            */
  float weight;          /* Weigt what else                    */
  int cost;            /* Value when sold (gp.)              */
  int timer;           /* Timer for object                   */
  time_t expiration_timestamp; /* Expiration for things left on the ground. Not saved. */
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

  obj_flag_data() :
    type_flag(0), weight(0), cost(0), timer(0), expiration_timestamp(0), material(0), barrier(0), condition(0),
    availtn(0), availdays(0), street_index(0), quest_id(0), attempt(0)
  {
    ZERO_OUT_ARRAY(value, NUM_OBJ_VALUES);
    ZERO_OUT_ARRAY(legality, 3);

    // No need to do anything with the bitfields-- they auto-clear.
  }
};

/* Used in obj_file_elem *DO*NOT*CHANGE* */
struct obj_affected_type
{
  byte location;      /* Which ability to change (APPLY_XXX) */
  sbyte modifier;     /* How much it changes by              */

  obj_affected_type() :
    location(0), modifier(0)
  {}
};

/* ================== Memory Structure for Objects ================== */
struct obj_data
{
  rnum_t item_number;         /* Where in data-base                   */
  struct room_data *in_room;      /* Pointer to the room the object is in. */
  struct veh_data *in_veh;
  bool vfront;

  // Debug fields
  char load_origin;                              /* Identifies what loaded this. */
  time_t load_time;                              /* Used to track when this was loaded. */
  char pc_load_origin;                           /* If this was loaded alongside a PC, stores their load reason. */
  idnum_t pc_load_idnum;                         /* If this was loaded alongside a PC, stores their idnum. */

  struct obj_flag_data obj_flags; /* Object information                 */
  struct obj_affected_type affected[MAX_OBJ_AFFECT];  /* affects */

  text_data text;
  struct extra_descr_data *ex_description; /* extra descriptions     */
  char *restring;
  char *matrix_restring;
  char *photo;
  char *graffiti;

  char *source_info; // Where in the source books did this come from?

  struct char_data *carried_by;   /* Carried by :NULL in room/conta   */
  struct char_data *worn_by;      /* Worn by?                         */
  sh_int worn_on;                 /* Worn where?                      */

  struct obj_data *in_obj;        /* In what object NULL when none    */
  struct obj_data *contains;      /* Contains objects                 */
  struct obj_data *next_content;  /* For 'contains' lists             */
  struct host_data *in_host;      /* For tracking if the object is in a Matrix host. */
  struct matrix_file *files;      /* All the matrix files stored on obj */

  struct obj_data *cyberdeck_part_pointer;

  struct char_data *targ;	  /* Data for mounts */
  struct veh_data *tveh;

  const char *dropped_by_host;
  idnum_t dropped_by_char;

  unsigned long idnum;

  // Adding new fields? Add them to dblist's UpdateObjs too to avoid iedit breaking things.

#ifdef USE_DEBUG_CANARIES
  int canary;
#endif
  obj_data() :
      item_number(0), in_room(NULL), in_veh(NULL), vfront(FALSE), load_origin(0), load_time(0), pc_load_origin(0), pc_load_idnum(0),
      ex_description(NULL), restring(NULL), matrix_restring(NULL), photo(NULL), graffiti(NULL), source_info(NULL), carried_by(NULL),
      worn_by(NULL), worn_on(0), in_obj(NULL), contains(NULL), next_content(NULL),
      in_host(NULL), files(NULL), cyberdeck_part_pointer(NULL), targ(NULL), tveh(NULL), 
      dropped_by_host(NULL), dropped_by_char(0), idnum(0)
  {
    #ifdef USE_DEBUG_CANARIES
      canary = CANARY_VALUE;
    #endif
  }
};

// Struct for preserving order of objects.
struct nested_obj {
  int level;
  struct obj_data* obj;

  nested_obj() :
    level(0), obj(NULL)
  {}
};

// Comparator for preserving order of objects.
struct find_level
{
    int level;
    find_level(int level) : level(level) {}
    bool operator () ( const nested_obj& o ) const
    {
        return o.level == level;
    }
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
  int canary;
#endif
  room_direction_data() :
      general_description(NULL), keyword(NULL), exit_info(0), key(0), to_room(NULL),
      key_level(0), ward(0), idnum(0), hidden(0), material(0), barrier(0), condition(0),
      to_room_vnum(NOWHERE), go_into_secondperson(NULL), go_into_thirdperson(NULL),
      come_out_of_thirdperson(NULL)
  {
    #ifdef USE_DEBUG_CANARIES
      canary = CANARY_VALUE;
    #endif
  }
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
  char *flight_code;
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
  room_spell_t light[3];                  /* Number of lightsources in room     */
  byte peaceful;
  room_spell_t poltergeist[2];
  room_spell_t icesheet[2];
  room_spell_t shadow[2];
  room_spell_t silence[2];
  SPECIAL(*func);

  bool dirty_bit;

  int staff_level_lock;
  int elevator_number;

  struct obj_data *contents;   /* List of items in room              */
  struct char_data *people;    /* List of NPC / PC in room           */
  struct veh_data *vehicles;
  struct char_data *watching;

  struct obj_data *best_workshop[NUM_WORKSHOP_TYPES];

  // Geolocation information for flight.
  float latitude, longitude;

  // Apartment/housing information.
  class Apartment *apartment;
  class ApartmentRoom *apartment_room;

  // Temp desc info.
  char *temp_desc;
  int temp_desc_timeout;
  idnum_t temp_desc_author_idnum;

#ifdef USE_DEBUG_CANARIES
  int canary;
#endif
  room_data() :
      number(0), zone(0), sector_type(0), name(NULL), description(NULL),
      night_desc(NULL), flight_code(NULL), ex_description(NULL), matrix(0), access(0), io(0),
      trace(0), bandwidth(0), rtg(0), jacknumber(0),
      address(NULL), blood(0), debris(0), spec(0), rating(0), cover(0), crowd(0),
      type(0), x(0), y(0), z(0), peaceful(0), func(NULL), dirty_bit(FALSE),
      staff_level_lock(0), elevator_number(0), contents(NULL), people(NULL),
      vehicles(NULL), watching(NULL), latitude(0), longitude(0), apartment(NULL), apartment_room(NULL),
      temp_desc(NULL), temp_desc_timeout(0), temp_desc_author_idnum(0)
  {
    ZERO_OUT_ARRAY(dir_option, NUM_OF_DIRS);
    ZERO_OUT_ARRAY(temporary_stored_exit, NUM_OF_DIRS);
    ZERO_OUT_ARRAY(vision, 3);
    ZERO_OUT_ARRAY(background, 4);
    ZERO_OUT_ARRAY(light, 3);
    ZERO_OUT_ARRAY(poltergeist, 2);
    ZERO_OUT_ARRAY(icesheet, 2);
    ZERO_OUT_ARRAY(shadow, 2);
    ZERO_OUT_ARRAY(silence, 2);
    ZERO_OUT_ARRAY(best_workshop, NUM_WORKSHOP_TYPES);

#ifdef USE_DEBUG_CANARIES
    canary = CANARY_VALUE;
#endif
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
      name(NULL), type(0), force(0), subtype(0), next(NULL)
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
  int subtype;
  int force;
};

/* memory structure for characters */
struct memory_rec_struct
{
  long id;
  struct memory_rec_struct *next;

  memory_rec_struct() :
      id(0), next(NULL)
  {}
};

typedef struct memory_rec_struct memory_rec;

/* This structure is purely intended to be an easy way to transfer */
/* and return information about time (real or mudwise).            */
struct time_info_data
{
  byte minute, hours, day, month, weekday;
  sh_int year;

  time_info_data() :
    minute(0), hours(0), day(0), month(0), weekday(0)
  {}
};

/* These data contain information about a players time data */
struct time_data
{
  time_t birth;    /* This represents the characters age                */
  time_t logon;    /* Time of the last logon (used to calculate played) */
  time_t lastdisc;
  int  played;     /* This is the total accumulated time played in secs */

  time_data() :
    birth(0), logon(0), lastdisc(0), played(0)
  {}
};

/* general player-related info, usually PC's and NPC's */
#define YES_PLEASE  0  /* Austin Powers, anyone? */
struct char_player_data
{
  char *char_name;

#ifndef NOCRYPT
  char passwd[crypto_pwhash_STRBYTES + 1];     /* character's password         */
#else
  char passwd[10000];
#endif

  text_data physical_text;
  text_data astral_text;
  text_data matrix_text;

  char *background;
  char *title;               /* PC / NPC's title                     */
  char *pretitle, *whotitle; /* PC's pre/whotitles                   */
  char *prompt, *matrixprompt;              /* PC's customized prompt               */
  char *poofin, *poofout;    /* PC's poofin/poofout                  */

  const char *email;

  int multiplier;
  int salvation_ticks;

  byte pronouns;                  /* PC / NPC's pronoun set and gender appearance  */
  byte level;
  long last_room;              /* PC s Hometown (zone)                 */
  struct time_data time;     /* PC's AGE in days                     */
  ush_int weight;              /* PC / NPC's weight                    */
  ush_int height;              /* PC / NPC's height                    */
  byte race;                 /* PC / NPC's race                      */
  byte otaku_path;            /* PC / NPC's otaku status                       */
  byte tradition;            /* PC / NPC's tradition                       */
  ubyte aspect;
  char *host;   /* player host    */

  char_player_data() :
      char_name(NULL), background(NULL), title(NULL), pretitle(NULL), whotitle(NULL),
      prompt(NULL), matrixprompt(NULL), poofin(NULL), poofout(NULL), email(NULL),
      multiplier(0), salvation_ticks(5), pronouns(PRONOUNS_NEUTRAL), level(0), last_room(NOWHERE),
      weight(0), height(0), race(0), otaku_path(OTAKU_PATH_NORMIE), tradition(TRAD_MUNDANE), aspect(0), host(NULL)
  {
    memset(passwd, 0, sizeof(passwd));
  }
}
;
#undef YES_PLEASE

/* Char's abilities.  Used in char_file_u *DO*NOT*CHANGE* */
struct char_ability_data
{
  sbyte attributes[NUM_ATTRIBUTES];
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
  sh_int task_pool[NUM_ATTRIBUTES];

  char_ability_data() :
    mag(0), bod_index(0), ess(0), esshole(0), highestindex(0), astral_pool(0),
    combat_pool(0), hacking_pool(0), hacking_pool_max(0), hacking_pool_remaining(0),
    magic_pool(0), casting_pool(0), drain_pool(0), spell_defense_pool(0),
    reflection_pool(0), offense_pool(0), defense_pool(0), body_pool(0), control_pool(0)
  {
    ZERO_OUT_ARRAY(attributes, NUM_ATTRIBUTES);
    ZERO_OUT_ARRAY(task_pool, NUM_ATTRIBUTES);
  }
};

struct char_point_data
{
  sh_int mental;
  sh_int max_mental;   /* Max move for PC/NPC                     */
  sh_int physical;
  sh_int max_physical; /* Max hit for PC/NPC                      */

  sh_int ballistic[3];    /* Ballistic armor class for bullets and such */
  sh_int impact[3];       /* Impact armor class for clubs and such   */
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
  int extra2;
  ubyte magic_loss;
  ubyte ess_loss;
  ubyte domain;
  ubyte resistpain;
  ubyte lastdamage;
  int track[2];
  Bitfield vision[NUM_VISION_TYPES];
  ubyte fire[3];
  ubyte binding;
  ubyte reach[2];
  int extras[2];
  sh_int projection_ticks;

  /* Adding a field to this struct? If it's a pointer, or if it's important, add it to utils.cpp's copy_over_necessary_info() to avoid breaking mdelete etc. */

  char_point_data() :
    mental(0), max_mental(0), physical(0), max_physical(10), nuyen(0), bank(0), karma(0), rep(0),
    noto(0), tke(0), sig(0), init_dice(0), init_roll(0), grade(0), extrapp(0), extra(0), extra2(0), 
    magic_loss(0), ess_loss(0), domain(0), resistpain(0), lastdamage(0), projection_ticks(0)
  {
    ZERO_OUT_ARRAY(ballistic, 3);
    ZERO_OUT_ARRAY(impact, 3);
    ZERO_OUT_ARRAY(sustained, 2);
    ZERO_OUT_ARRAY(track, 2);
    ZERO_OUT_ARRAY(fire, 3);
    ZERO_OUT_ARRAY(reach, 2);
    ZERO_OUT_ARRAY(extras, 2);
  }

  // Adding something important? If it needs to be replicated to medited mobs, also update
  // utils.cpp's copy_over_necessary_info().
};

struct char_special_data_saved
{
  sh_int powerpoints;
  bool left_handed;
  sh_int cur_lang;         /* # of language char is currently speaking */
  Bitfield act;    /* act flag for NPC's; player flag for PC's */

  Bitfield affected_by;        /* Bitvector for spells/skills affected by */
  byte skills[MAX_SKILLS+1][3];   /* array of skills plus skill 0. Slot 0 is unaltered, */
  byte powers[ADEPT_NUMPOWER+1][2];
  unsigned char metamagic[META_MAX+1];
  unsigned char echoes[ECHO_MAX+1];
  ush_int centeringskill;
  ush_int boosted[3][2];           /* str/qui/bod timeleft/amount		*/
  ubyte masking;
  int points;

  char_special_data_saved() :
    powerpoints(0), left_handed(0), cur_lang(0), centeringskill(0), masking(0), points(0)
  {
    for (int i = 0; i < MAX_SKILLS + 1; i++) {
      ZERO_OUT_ARRAY(skills[i], 3);
    }

    for (int i = 0; i < ADEPT_NUMPOWER + 1; i++) {
      ZERO_OUT_ARRAY(powers[i], 2);
    }

    ZERO_OUT_ARRAY(metamagic, META_MAX + 1);
    ZERO_OUT_ARRAY(echoes, ECHO_MAX + 1);

    for (int i = 0; i < 3; i++) {
      ZERO_OUT_ARRAY(boosted[i], 2);
    }
  }
};

struct char_special_data
{
  struct veh_data *fight_veh;
  struct char_data *fighting;  /* Opponent                             */
  struct char_data *hunting;   /* Char hunted by this char             */
  struct obj_data *building;
  struct matrix_file *programming; /* Program char is currently designing/programming */
  int conjure[4];
  int num_spirits;
  idnum_t idnum;
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
  int concentration_target_mod;

  float carry_weight;           /* Carried weight                       */
  byte carry_items;            /* Number of items carried              */
  sh_int foci;
  sh_int last_healed;
  int timer;                  /* Timer for update                     */
  int last_timer;             /* Last timer, which is restored on actions that don't block idle nuyen rewards */
  int last_social_action;     /* Set whenever you emote or speak. Used to check that people in social rooms aren't just hanging out idle and ignoring each other. */
  int actions;
  int coord[3];

  bool dirty_bits[NUM_DIRTY_BITS];

  struct veh_data *subscribe;   /* subscriber list */
  struct veh_data *rigging;     /* Vehicle char is controlling remotely */
  struct char_data *mindlink;
  struct spirit_data *spirits;

  struct char_special_data_saved saved; /* constants saved in plrfile  */

  idnum_t npc_faction_membership;

  const char *highlight_color_code;
  /* Adding a field to this struct? If it's a pointer, or if it's important, add it to utils.cpp's copy_over_necessary_info() to avoid breaking mdelete etc. */
  // Touch olc's mclone, medit etc if it's a string.

  char_special_data() :
      fight_veh(NULL), fighting(NULL), hunting(NULL), programming(NULL), num_spirits(0), idnum(0),
      nervestrike(FALSE), tempquiloss(0), cost_breakup(0), avail_offset(0), shooting_dir(0),
      position(POS_STANDING), defined_position(NULL), leave(NULL), arrive(NULL), target_mod(0),
      concentration_target_mod(0), carry_weight(0), carry_items(0), foci(0), last_healed(0), timer(0),
      last_timer(0), last_social_action(0), actions(0), subscribe(NULL), rigging(NULL),
      mindlink(NULL), spirits(NULL), npc_faction_membership(0), highlight_color_code(NULL)
  {
    ZERO_OUT_ARRAY(conjure, 4);
    ZERO_OUT_ARRAY(coord, 3);
    ZERO_OUT_ARRAY(dirty_bits, NUM_DIRTY_BITS);
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
  ubyte automod_counter;
  sbyte conditions[3];         /* Drunk, full, thirsty                 */

  ubyte totem;                 /* totem player chooses                 */
  ubyte totemspirit;
  ush_int att_points;              /* attrib points for when you first create */
  ush_int skill_points;            /* starting skill points                   */
  ush_int channel_points;          /* starting channel skill points for otaku */
  unsigned char force_points;
  unsigned char restring_points;
  int zonenum;

  // Chargen data for archetypes.
  sh_int archetype;
  bool archetypal;

  // Alt info.
  idnum_t prestige_alt_id;

  int system_points;

  int best_lifestyle;
  const char *lifestyle_string;

  std::vector<PCExDesc *> exdescs;

  player_special_data_saved() :
    wimp_level(0), freeze_level(0), invis_level(0), incog_level(0), load_room(NOWHERE),
    last_in(0), last_veh(NOTHING), bad_pws(0), automod_counter(0), totem(0), totemspirit(0),
    att_points(0), skill_points(0), channel_points(0), force_points(0), restring_points(0), zonenum(0),
    archetype(0), archetypal(FALSE), prestige_alt_id(0), system_points(0), 
    best_lifestyle(LIFESTYLE_SQUATTER), lifestyle_string(NULL)
  {
    ZERO_OUT_ARRAY(conditions, 3);
  }
};

struct player_special_data
{
  struct player_special_data_saved saved;

  struct alias *aliases;       /* Character's aliases                  */
  struct remem *remem;         /* Character's Remembers          */
  idnum_t last_tell;              /* idnum of last tell from              */
  sh_int  questnum;
  sh_int *obj_complete;
  sh_int *mob_complete;
  long last_quest[QUEST_TIMER];
  long completed_quest[QUEST_TIMER];
  ush_int drugs[NUM_DRUGS][NUM_DRUG_PLAYER_SPECIAL_FIELDS];
  time_t drug_last_fix[NUM_DRUGS];
  ubyte mental_loss;
  ubyte physical_loss;
  ubyte perm_bod;
  struct room_data *watching;
  int wherelist_checks;
  sh_int max_exdescs;

  Bitfield covered_wearlocs;

  player_special_data() :
      aliases(NULL), remem(NULL), last_tell(0), questnum(0), obj_complete(NULL),
      mob_complete(NULL), mental_loss(0), physical_loss(0),
      perm_bod(0), watching(NULL), wherelist_checks(0), max_exdescs(0)
  {
    ZERO_OUT_ARRAY(last_quest, QUEST_TIMER);
    ZERO_OUT_ARRAY(completed_quest, QUEST_TIMER);
    ZERO_OUT_ARRAY(drug_last_fix, NUM_DRUGS);

    for (int i = 0; i < NUM_DRUGS; i++) {
      ZERO_OUT_ARRAY(drugs[i], NUM_DRUG_PLAYER_SPECIAL_FIELDS);
    }

    covered_wearlocs.FromString("0");
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
  vnum_t spare1, spare2;
  long lasthit;

  std::unordered_map<idnum_t, time_t> alarm_map = {};

  /* Adding a field to this struct? If it's a pointer, or if it's important, add it to utils.cpp's copy_over_necessary_info() to avoid breaking mdelete etc. */

  mob_special_data() :
    last_direction(NORTH), attack_type(0), default_pos(POS_STANDING),
    active(0), memory(NULL), wait_state(0), quest_id(0), value_death_nuyen(0),
    value_death_items(0), value_death_karma(0), count_death(0), spare1(0), spare2(0), lasthit(0)
  {
    ZERO_OUT_ARRAY(mob_skills, 10);
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
      name(NULL), room(NOWHERE), next(NULL)
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
  char *decorate_front;
  char *decorate_rear;
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
  int veh_destruction_timer;

  struct veh_follow *followers;
  struct veh_data *following;
  struct char_data *followch;
  struct room_data *lastin[3];

  struct obj_data *mount;
  struct obj_data *mod[NUM_MODS];
  bool sub;
  int sub_rank;

  idnum_t idnum;
  idnum_t owner;
  long spare, spare2;
  bool locked;
  struct room_data *dest;
  Bitfield flags;
  char *defined_position;

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
  
  // Flight info.
  struct room_data *flight_target;
  int flight_duration;

  vnum_t desired_in_room_on_load;

#ifdef USE_DEBUG_CANARIES
  int canary;
#endif

  veh_data() :
      in_room(NULL), name(NULL), description(NULL), short_description(NULL), restring(NULL),
      long_description(NULL), restring_long(NULL), decorate_front(NULL), decorate_rear(NULL),
      inside_description(NULL), rear_description(NULL), veh_destruction_timer(0), 
      followers(NULL), following(NULL), followch(NULL), mount(NULL), sub(FALSE), sub_rank(0),
      idnum(0), owner(0), spare(0), spare2(0), dest(NULL), defined_position(NULL),
      contents(NULL), people(NULL), rigger(NULL), fighting(NULL), fight_veh(NULL), next_veh(NULL),
      next_sub(NULL), prev_sub(NULL), carriedvehs(NULL), in_veh(NULL), towing(NULL), grid(NULL),
      leave(NULL), arrive(NULL), next(NULL), flight_target(NULL), flight_duration(0),
      desired_in_room_on_load(0)
  {
    for (int i = 0; i < NUM_MODS; i++)
      mod[i] = NULL;

#ifdef USE_DEBUG_CANARIES
    canary = CANARY_VALUE;
#endif
  }
}
;

/* ================== Structure for player/non-player ===================== */
struct char_data
{
  long nr;                            /* Mob's rnum                    */
  mob_unique_id_t unique_id;
  // the previous will be DEFUNCT once MobIndex is written
  struct room_data *in_room;                     /* Location */
  struct room_data *was_in_room;                 /* location for linkdead people  */

  // Debug fields
  int load_origin;                               /* Identifies what loaded this. */
  time_t load_time;                              /* Used to track when this was loaded. */

  struct char_player_data player;       /* Normal data                   */
  struct char_ability_data real_abils;  /* Abilities without modifiers   */
  struct char_ability_data aff_abils;   /* Abils with spells/stones/etc  */
  struct char_point_data points;        /* Points                        */
  struct char_special_data char_specials;      /* PC/NPC specials        */
  struct player_special_data *player_specials; /* PC specials            */
  struct mob_special_data mob_specials;        /* NPC specials           */
  struct veh_data *in_veh;
  bool vfront;
  struct char_data *hitched_to;         /* When using hitching, this is who we're connected to */
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
  struct char_data *next_in_character_list;             /* For either monster or ppl-list  */
  struct char_data *next_fighting;    /* For fighting list               */
  struct char_data *next_in_zone;     /* for zone->people - list         */
  struct char_data *next_in_veh;      /* For veh->people - list          */
  struct char_data *next_watching;

  struct follow_type *followers;        /* List of chars followers       */
  struct char_data *master;             /* Who is char following?        */
  struct spell_data *spells;                     /* linked list of spells          */

  IgnoreData *ignore_data;

  Pgroup_data *pgroup;                   /* Data concerning the player group this char is part of. */
  Pgroup_invitation *pgroup_invitations; /* The list of open group invitations associated with this player. */

  int congregation_bonus_pool;         /* Bonuses accrued from spending time in a congregation room */

  int last_loop_rand;

  // See invis_resistance_tests.cpp for details.
  std::unordered_map<idnum_t, bool> *pc_invis_resistance_test_results = {};
  std::unordered_map<idnum_t, bool> *mob_invis_resistance_test_results = {};
  std::unordered_map<idnum_t, int> assense_recency = {};

  // Another unordered map to track who we've sent docwagon alerts to.
  std::unordered_map<idnum_t, bool> sent_docwagon_messages_to = {};
  std::unordered_map<idnum_t, bool> received_docwagon_ack_from = {};

  // Track faction reputation points.
  std::unordered_map<idnum_t, int> faction_rep = {};

  bool alias_dirty_bit;

  /* Named after 'magic bullet pants', the 'technology' in FPS games that allows you to never have to worry about which mag has how much ammo in it. */
  unsigned short bullet_pants[(END_OF_AMMO_USING_WEAPONS + 1) - START_OF_AMMO_USING_WEAPONS][NUM_AMMOTYPES];

  vnum_t mob_loaded_in_room;

  // Some NPCs have pre-cast spells on them. We store those here. PROTO, DON'T DELETE!
  std::vector<sustain_data *> *precast_spells;

  bool is_carrying_vehicle;

  /* Adding a field to this struct? If it's a pointer, or if it's important, add it to utils.cpp's copy_over_necessary_info() to avoid breaking mdelete etc. */
#ifdef USE_DEBUG_CANARIES
  int canary;
#endif

  char_data() :
      nr(0), unique_id(0), in_room(NULL), was_in_room(NULL), load_origin(0), load_time(time(0)), player_specials(NULL), in_veh(NULL),
      vfront(FALSE), hitched_to(NULL), persona(NULL), squeue(NULL), sustained(NULL), ssust(NULL), carrying(NULL), desc(NULL),
      cyberware(NULL), bioware(NULL), next_in_room(NULL), next_in_character_list(NULL), next_fighting(NULL), next_in_zone(NULL),
      next_in_veh(NULL), next_watching(NULL), followers(NULL), master(NULL), spells(NULL), ignore_data(NULL), pgroup(NULL),
      pgroup_invitations(NULL), congregation_bonus_pool(0), last_loop_rand(0), pc_invis_resistance_test_results(NULL),
      mob_invis_resistance_test_results(NULL), alias_dirty_bit(FALSE), mob_loaded_in_room(NULL), precast_spells(NULL),
      is_carrying_vehicle(FALSE)
  {
    ZERO_OUT_ARRAY(equipment, NUM_WEARS);

    // Initialize our bullet pants. Note that we index from 0 here.
    for (int wp = 0; wp <= END_OF_AMMO_USING_WEAPONS - START_OF_AMMO_USING_WEAPONS; wp++) {
      ZERO_OUT_ARRAY(bullet_pants[wp], NUM_AMMOTYPES);
    }

#ifdef USE_DEBUG_CANARIES
    canary = CANARY_VALUE;
#endif
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
  sh_int prestige_race;
  idnum_t prestige_bagholder;
  int prestige_cost;
  bool is_otaku;
};

struct descriptor_data
{
  int descriptor;                         /* file descriptor for socket         */
  u_short peer_port;            /* port of peer                         */
  char host[HOST_LENGTH+1];       /* hostname                           */
  byte bad_pws;                   /* number of bad pw attemps this login        */
  byte idle_ticks;               /* tics idle at password prompt         */
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
  int inbuf_canary;
  char last_input[MAX_INPUT_LENGTH]; /* the last input                  */
  int last_input_canary;
  char small_outbuf[SMALL_BUFSIZE];  /* standard output buffer          */
  int small_outbuf_canary;
  char *output;                 /* ptr to the current output buffer     */
  int output_canary;
  int bufptr;                     /* ptr to end of current output               */
  int bufspace;                 /* space left in the output buffer      */
  struct txt_block *large_outbuf; /* ptr to large buffer, if we need it */
  struct txt_q input;             /* q of unprocessed input             */
  int input_and_character_canary;
  struct char_data *character;  /* linked to char                       */
  struct char_data *original;   /* original char if switched            */
  struct descriptor_data *snooping; /* Who is this char snooping        */
  struct descriptor_data *snoop_by; /* And who is snooping this char    */
  struct descriptor_data *next; /* link to next descriptor              */
  struct descriptor_data *watching; /* Who is this char snooping        */
  struct descriptor_data *watcher; /* And who is snooping this char    */
  struct ccreate_t ccr;
  int invalid_command_counter;
  char last_sprayed[MAX_INPUT_LENGTH];

  long nuyen_paid_for_wheres_my_car;
  long nuyen_income_this_play_session[NUM_OF_TRACKED_NUYEN_INCOME_SOURCES];

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
  struct matrix_file *edit_matrix_file; /* edit target for pedit */
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
  ApartmentComplex *edit_complex;
  ApartmentComplex *edit_complex_original;
  Apartment *edit_apartment;
  Apartment *edit_apartment_original;
  ApartmentRoom *edit_apartment_room;
  ApartmentRoom *edit_apartment_room_original;
  Faction *edit_faction;
  PCExDesc *edit_exdesc;
  Playergroup *edit_pgroup; /* playergroups */
  #ifdef TEMPORARY_COMPILATION_GUARD
  MinigameModule *edit_minigame_module;
  MinigameContainer *edit_minigame_container;
  #endif
  // If you add more of these edit_whatevers, touch comm.cpp's free_editing_structs and add them!

  int canary;
  protocol_t *pProtocol;

  // this is for spell creation

  // This is mostly a just-in-case section. Descriptors are zeroed out when created in comm.cpp's new_descriptor().
  descriptor_data() :
      showstr_head(NULL), showstr_point(NULL), str(NULL),
      output(NULL), output_canary(CANARY_VALUE), large_outbuf(NULL), input_and_character_canary(CANARY_VALUE),
      character(NULL), original(NULL), snooping(NULL), snoop_by(NULL), next(NULL), watching(NULL), watcher(NULL),
      invalid_command_counter(0), iedit_limit_edits(0), misc_data(NULL),
      edit_obj(NULL), edit_room(NULL), edit_mob(NULL), edit_quest(NULL), edit_shop(NULL),
      edit_zon(NULL), edit_cmd(NULL), edit_veh(NULL), edit_host(NULL), edit_icon(NULL),
      edit_helpfile(NULL), edit_complex(NULL), edit_complex_original(NULL),
      edit_apartment(NULL), edit_apartment_original(NULL), edit_apartment_room(NULL),
      edit_apartment_room_original(NULL), edit_faction(NULL), edit_pgroup(NULL),
#ifdef TEMPORARY_COMPILATION_GUARD
      edit_minigame_module(NULL), edit_minigame_container(NULL),
#endif
      canary(CANARY_VALUE), pProtocol(NULL)
  {
    // Zero out the communication history for all channels.
    for (int channel = 0; channel < NUM_COMMUNICATION_CHANNELS; channel++)
      message_history[channel] = listClass<const char *>();

    // Zero out our metrics.
    for (int i = 0; i < NUM_OF_TRACKED_NUYEN_INCOME_SOURCES; i++) {
      nuyen_income_this_play_session[i] = 0;
    }

    // Wipe last_sprayed.
    last_sprayed[0] = '\0';
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
  idnum_t idnum;
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
  bool is_knowledge_skill;
  bool requires_magic;
  bool requires_resonance;
  sh_int group;
  bool reflex_recorder_compatible;
  bool no_defaulting_allowed;
  bool is_nerps;
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

struct spirit_table {
  char name[50];
  vnum_t vnum;
};

struct nuyen_faucet_or_sink {
  char name[100];
  byte type;
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
  int idnum; // This is distinct from caster idnum etc, so does not necessarily need to be idnum_t.
  int time;
  unsigned char time_to_take_effect;
  bool caster;
  struct obj_data *focus;
  struct char_data *spirit;
  struct char_data *other;
  struct sustain_data *next;
  sustain_data() :
    spell(0), subtype(0), force(0), success(0), idnum(0), time(0), time_to_take_effect(0), caster(0),
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
  int force;
  spirit_sustained() :
    type(0), caster(FALSE), target(NULL), next(NULL), force(0)
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
  float _weight;  // Use bullet_pants.cpp's get_ammo_weight() instead.
  unsigned char cost;
  float street_index;
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

// For listing allowed max values for weapon types. Has settings for both ranged and melee.
struct kosher_weapon_values_struct {
    // Shared
    int power;
    int damage_code;
    int skill;
    int conceal;

    // Ranged
    int max_ammo;
    bool can_ss;
    bool can_sa;
    bool can_bf;
    bool can_fa;
    int recoil_comp;
    bool can_attach_bottom;
    bool can_attach_barrel;
    bool can_attach_top;

    // Melee
    int str_bonus;
    int reach;
};


#define WORK_PHASE_NONE          0
#define WORK_PHASE_READY         1
#define WORK_PHASE_IN_PROGRESS   2
#define WORK_PHASE_COMPLETE      3

#define MATRIX_FILE_DATA             0
#define MATRIX_FILE_DESIGN           1
#define MATRIX_FILE_SOURCE_CODE      2 // Represents an uncooked program
#define MATRIX_FILE_PROGRAM          3
#define MATRIX_FILE_FIRMWARE         4
#define MATRIX_FILE_PAYDATA          5
#define MATRIX_FILE_SKILLSOFT        6
#define MATRIX_FILE_PHOTO            7
#define MATRIX_FILE_POCSEC_NOTE      8
#define MATRIX_FILE_POCSEC_PHONENUM  9
#define MATRIX_FILE_POCSEC_MAIL      10


/* ================== Memory Structure for Matrix Files ================== */
struct matrix_file {
  // SQL fields
  rnum_t idnum;                                  /* Stored in database identifier */
  char* name;                                    /* The user-friendly text for this file */
  int file_type;
  int program_type;                              /* If this is a file_type PROGRAM, this will be the type of program it is */
  int rating;
  int size;                                      /* File size in megapulses. This is the value used when adjusting storage values */
  int original_size;                             /* A variable that can be used for various overloads/alternating sizes on files */
  int compression_factor;                        /* If a file is compressed, the factor goes here */
  int wound_category;                            /* File's attack damage if damaging program */
  int is_default;                                /* Whether or not to load the program by default */
  idnum_t creator_idnum;
  long creation_time;                            /* When was this file created? */
  char *content;                                 /* Free form field for having text content; used for photos */

  long last_decay_time;     
  int quest_id; 
  struct obj_data *in_obj;                       /* In what object NULL when none    */       

  // Fields used by skillsofts
  int skill;      
  rnum_t linked;                                 /* If this is non-zero, it's the idnum of the ch */

  // Fields used by paydata
  rnum_t from_host_vnum;                         /* If this file came from a host, which host?  */
  int from_host_color;

  // Non-SQL fields
  struct matrix_file *next_file;                 /* For 'files' lists             */
  struct host_data *in_host;

  // Non-SQL stateful vars for other nonsense
  idnum_t found_by;
  idnum_t file_worker;
  struct obj_data *transferring_to;
  struct host_data *transferring_to_host;
  int transfer_remaining;
  int file_protection;
  int resonant;                                  /* Is this a resonant file for otaku? */

  // Non-SQL fields used for keeping track of B/R
  int work_phase;                               
  int work_ticks_left;
  int work_original_ticks_left;
  int work_successes;  
  
  // Debug fields
  char load_origin;                              /* Identifies what loaded this. */
  unsigned long loaded_with_obj_idnum;

  // Operation non-SQL fields
  bool dirty_bit;                                /* Set to TRUE when you change a field so it will be saved */

  /* Adding a field? Make sure to adjust dbnew for SQL and clone_matrix_file in matrix_storage */
  matrix_file() : 
      idnum(0), name(0), file_type(0), program_type(0), rating(0), size(0), original_size(0),
      compression_factor(0), wound_category(0), is_default(0), creator_idnum(0), content(0),

      last_decay_time(0), quest_id(0), in_obj(0), skill(0), linked(0), from_host_vnum(0),
      from_host_color(0),

      next_file(0), in_host(0),

      found_by(0), file_worker(0), transferring_to(0), transferring_to_host(0), transfer_remaining(0),
      file_protection(0), resonant(0),

      work_phase(0), work_ticks_left(0), work_original_ticks_left(0), work_successes(0),

      load_origin(0), loaded_with_obj_idnum(0), dirty_bit (0)
  {

  }
};

#endif
