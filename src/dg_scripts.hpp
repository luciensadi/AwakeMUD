/**
* @file dg_scripts.hpp
* Header file for script structures, constants, and function prototypes for
* the DG Scripts trigger system.
*
* DG Scripts is part of the core tbaMUD source code distribution, which is a
* derivative of, and continuation of, CircleMUD. AwakeMUD CE shares that
* lineage, so the trigger language, the trigger type bits and the %variable%
* syntax here are all the ones tbaMUD builders already know.
*
* Original DG Scripts authors:
* $Author: Mark A. Heilpern/egreen/Welcor $
* $Date: 2004/10/11 12:07:00$
* $Revision: 1.0.14 $
*
* Ported to AwakeMUD CE by Fizban.
*/
#ifndef _dg_scripts_hpp_
#define _dg_scripts_hpp_

#include "types.hpp"
#include "structs.hpp"
#include "awake.hpp"       /* RL_SEC */
#include "interpreter.hpp" /* To make sure ACMD is defined */

class File;

/* Triggers attach to one of three things. */
#define MOB_TRIGGER   0
#define OBJ_TRIGGER   1
#define WLD_TRIGGER   2

/* %actor.room% yields a room variable rather than a bare vnum, matching
 * tbaMUD patchlevel 8 and later. */
#define ACTOR_ROOM_IS_UID 1

/* mob trigger types */
#define MTRIG_GLOBAL           (1 << 0)      /* check even if zone empty     */
#define MTRIG_RANDOM           (1 << 1)      /* checked randomly             */
#define MTRIG_COMMAND          (1 << 2)      /* character types a command    */
#define MTRIG_SPEECH           (1 << 3)      /* a char says a word/phrase    */
#define MTRIG_ACT              (1 << 4)      /* word or phrase sent to act   */
#define MTRIG_DEATH            (1 << 5)      /* character dies               */
#define MTRIG_GREET            (1 << 6)      /* something enters room seen   */
#define MTRIG_GREET_ALL        (1 << 7)      /* anything enters room         */
#define MTRIG_ENTRY            (1 << 8)      /* the mob enters a room        */
#define MTRIG_RECEIVE          (1 << 9)      /* character is given obj       */
#define MTRIG_FIGHT            (1 << 10)     /* each pulse while fighting    */
#define MTRIG_HITPRCNT         (1 << 11)     /* fighting and below some hp   */
#define MTRIG_BRIBE            (1 << 12)     /* nuyen is given to mob        */
#define MTRIG_LOAD             (1 << 13)     /* the mob is loaded            */
#define MTRIG_MEMORY           (1 << 14)     /* mob sees someone remembered  */
#define MTRIG_CAST             (1 << 15)     /* mob targetted by spell       */
#define MTRIG_LEAVE            (1 << 16)     /* someone leaves room seen     */
#define MTRIG_DOOR             (1 << 17)     /* door manipulated in room     */
#define MTRIG_DAMAGE           (1 << 18)     /* mob takes damage             */
#define MTRIG_TIME             (1 << 19)     /* trigger based on game hour   */

/* obj trigger types */
#define OTRIG_GLOBAL           (1 << 0)      /* unused                       */
#define OTRIG_RANDOM           (1 << 1)      /* checked randomly             */
#define OTRIG_COMMAND          (1 << 2)      /* character types a command    */

#define OTRIG_TIMER            (1 << 5)      /* item's timer expires         */
#define OTRIG_GET              (1 << 6)      /* item is picked up            */
#define OTRIG_DROP             (1 << 7)      /* character tries to drop obj  */
#define OTRIG_GIVE             (1 << 8)      /* character tries to give obj  */
#define OTRIG_WEAR             (1 << 9)      /* character tries to wear obj  */
#define OTRIG_REMOVE           (1 << 11)     /* character tries to remove it */

#define OTRIG_LOAD             (1 << 13)     /* the object is loaded         */

#define OTRIG_CAST             (1 << 15)     /* object targetted by spell    */
#define OTRIG_LEAVE            (1 << 16)     /* someone leaves room seen     */

#define OTRIG_CONSUME          (1 << 18)     /* char tries to eat/drink obj  */
#define OTRIG_TIME             (1 << 19)     /* trigger based on game hour   */

/* wld trigger types */
#define WTRIG_GLOBAL           (1 << 0)      /* check even if zone empty     */
#define WTRIG_RANDOM           (1 << 1)      /* checked randomly             */
#define WTRIG_COMMAND          (1 << 2)      /* character types a command    */
#define WTRIG_SPEECH           (1 << 3)      /* a char says word/phrase      */

#define WTRIG_RESET            (1 << 5)      /* zone has been reset          */
#define WTRIG_ENTER            (1 << 6)      /* character enters room        */
#define WTRIG_DROP             (1 << 7)      /* something dropped in room    */

#define WTRIG_CAST             (1 << 15)     /* spell cast in room           */
#define WTRIG_LEAVE            (1 << 16)     /* character leaves the room    */
#define WTRIG_DOOR             (1 << 17)     /* door manipulated in room     */
#define WTRIG_LOGIN            (1 << 18)     /* character logs into the MUD  */
#define WTRIG_TIME             (1 << 19)     /* trigger based on game hour   */

/* obj command trigger types */
#define OCMD_EQUIP             (1 << 0)      /* obj must be in char's equip  */
#define OCMD_INVEN             (1 << 1)      /* obj must be in char's inven  */
#define OCMD_ROOM              (1 << 2)      /* obj must be in char's room   */

/* obj consume trigger commands */
#define OCMD_EAT    1
#define OCMD_DRINK  2
#define OCMD_QUAFF  3

#define TRIG_NEW                0            /* trigger starts from top      */
#define TRIG_RESTART            1            /* trigger restarting           */

/* Offset from PULSE_MOBILE so the two don't land on the same pulse. */
#define PULSE_DG_SCRIPT         (13 RL_SEC)

#define MAX_SCRIPT_DEPTH      10   /* how deep triggers may recurse */

#define SCRIPT_ERROR_CODE     -9999999

/* one line of the trigger */
struct cmdlist_element {
  char *cmd;                            /* one line of a trigger */
  struct cmdlist_element *original;
  struct cmdlist_element *next;
  int loops;                            /* runs so far in a while loop */

  cmdlist_element() : cmd(NULL), original(NULL), next(NULL), loops(0) {}
};

struct trig_var_data {
  char *name;                           /* name of variable  */
  char *value;                          /* value of variable */
  long context;                         /* 0: global context */

  struct trig_var_data *next;

  trig_var_data() : name(NULL), value(NULL), context(0), next(NULL) {}
};

/** structure for triggers */
struct trig_data {
  rnum_t nr;                            /**< trigger's rnum                  */
  byte attach_type;                     /**< mob/obj/wld intentions          */
  byte data_type;                       /**< type of game_data for trig      */
  char *name;                           /**< name of trigger                 */
  long trigger_type;                    /**< type of trigger (bitvector)     */
  struct cmdlist_element *cmdlist;      /**< top of command list             */
  struct cmdlist_element *curr_state;   /**< current line of trigger         */
  int narg;                             /**< numerical argument              */
  char *arglist;                        /**< argument list                   */
  int depth;                            /**< depth into nested ifs/whiles    */
  int loops;                            /**< loop iteration counter          */
  struct dg_event *wait_event;          /**< event pausing the trigger       */
  ubyte purged;                         /**< trigger is set to be purged     */
  struct trig_var_data *var_list;       /**< local vars for this trigger     */

  struct trig_data *next;
  struct trig_data *next_in_world;      /**< next in the global trigger list */

  trig_data() : nr(-1), attach_type(MOB_TRIGGER), data_type(MOB_TRIGGER), name(NULL),
      trigger_type(0), cmdlist(NULL), curr_state(NULL), narg(0), arglist(NULL),
      depth(0), loops(0), wait_event(NULL), purged(0), var_list(NULL),
      next(NULL), next_in_world(NULL)
  {}
};

/** a complete script (composed of several triggers) */
struct script_data {
  long types;                        /**< bitvector of trigger types */
  struct trig_data *trig_list;       /**< list of triggers           */
  struct trig_var_data *global_vars; /**< list of global variables   */
  ubyte purged;                      /**< script is set to be purged */
  long context;                      /**< current context for statics */

  struct script_data *next;          /**< used for purged_scripts    */

  script_data() : types(0), trig_list(NULL), global_vars(NULL), purged(0),
      context(0), next(NULL)
  {}
};

/* The event data for the wait command */
struct wait_event_data {
  struct trig_data *trigger;
  void *go;
  int type;
};

/* used for actor memory triggers */
struct script_memory {
  long id;                  /* id of who to remember */
  char *cmd;                /* command, or NULL for generic */
  struct script_memory *next;

  script_memory() : id(0), cmd(NULL), next(NULL) {}
};

/* The prototype list a mob/obj/room carries before its script is built. */
struct trig_proto_list {
  vnum_t vnum;
  struct trig_proto_list *next;

  trig_proto_list() : vnum(NOTHING), next(NULL) {}
};

/* function prototypes from dg_triggers.cpp */
char *one_phrase(char *arg, char *first_arg);
int is_substring(const char *sub, const char *string);
int word_check(const char *str, const char *wordlist);

void act_mtrigger(const struct char_data *ch, const char *str,
    struct char_data *actor, struct char_data *victim,
    struct obj_data *object, struct obj_data *target, const char *arg);
void speech_mtrigger(struct char_data *actor, const char *str);
void speech_wtrigger(struct char_data *actor, const char *str);
void greet_memory_mtrigger(struct char_data *ch);
int greet_mtrigger(struct char_data *actor, int dir);
int entry_mtrigger(struct char_data *ch);
void entry_memory_mtrigger(struct char_data *ch);
int enter_wtrigger(struct room_data *room, struct char_data *actor, int dir);
int drop_otrigger(struct obj_data *obj, struct char_data *actor);
void timer_otrigger(struct obj_data *obj);
int get_otrigger(struct obj_data *obj, struct char_data *actor);
int drop_wtrigger(struct obj_data *obj, struct char_data *actor);
int give_otrigger(struct obj_data *obj, struct char_data *actor, struct char_data *victim);
int receive_mtrigger(struct char_data *ch, struct char_data *actor, struct obj_data *obj);
void bribe_mtrigger(struct char_data *ch, struct char_data *actor, int amount);
int wear_otrigger(struct obj_data *obj, struct char_data *actor, int where);
int remove_otrigger(struct obj_data *obj, struct char_data *actor);

int cmd_otrig(struct obj_data *obj, struct char_data *actor, char *cmd, char *argument, int type);
int command_mtrigger(struct char_data *actor, char *cmd, char *argument);
int command_otrigger(struct char_data *actor, char *cmd, char *argument);
int command_wtrigger(struct char_data *actor, char *cmd, char *argument);

int death_mtrigger(struct char_data *ch, struct char_data *actor);
void fight_mtrigger(struct char_data *ch);
void hitprcnt_mtrigger(struct char_data *ch);

void random_mtrigger(struct char_data *ch);
void random_otrigger(struct obj_data *obj);
void random_wtrigger(struct room_data *room);
void reset_wtrigger(struct room_data *room);

void load_mtrigger(struct char_data *ch);
void load_otrigger(struct obj_data *obj);

int cast_mtrigger(struct char_data *actor, struct char_data *ch, int spellnum);
int cast_otrigger(struct char_data *actor, struct obj_data *obj, int spellnum);
int cast_wtrigger(struct char_data *actor, struct char_data *vict, struct obj_data *obj, int spellnum);

int leave_mtrigger(struct char_data *actor, int dir);
int leave_wtrigger(struct room_data *room, struct char_data *actor, int dir);
int leave_otrigger(struct room_data *room, struct char_data *actor, int dir);

int door_mtrigger(struct char_data *actor, int subcmd, int dir);
int door_wtrigger(struct char_data *actor, int subcmd, int dir);

int consume_otrigger(struct obj_data *obj, struct char_data *actor, int cmd);

void time_mtrigger(struct char_data *ch);
void time_otrigger(struct obj_data *obj);
void time_wtrigger(struct room_data *room);

int login_wtrigger(struct room_data *room, struct char_data *actor);

int damage_mtrigger(struct char_data *ch, struct char_data *victim, int dam, int attacktype);

/* function prototypes from dg_scripts.cpp */
ACMD_DECLARE(do_attach);
ACMD_DECLARE(do_detach);
ACMD_DECLARE(do_tstat);
ACMD_DECLARE(do_vdelete);
char *dg_str_str(char *cs, const char *ct);
int find_eq_pos_script(char *arg);
struct char_data *find_char(long n);
struct obj_data *find_obj(long n);
struct room_data *find_room(long n);
struct char_data *get_char(const char *name);
struct char_data *get_char_near_obj(struct obj_data *obj, const char *name);
struct char_data *get_char_in_room(struct room_data *room, const char *name);
struct obj_data *get_obj_near_obj(struct obj_data *obj, const char *name);
struct obj_data *get_obj(const char *name);
struct room_data *get_room(const char *name);
struct char_data *get_char_by_obj(struct obj_data *obj, const char *name);
struct char_data *get_char_by_room(struct room_data *room, const char *name);
struct obj_data *get_obj_by_obj(struct obj_data *obj, const char *name);
struct obj_data *get_obj_in_room(struct room_data *room, const char *name);
struct obj_data *get_obj_by_room(struct room_data *room, const char *name);
int trgvar_in_room(vnum_t vnum);
/* get_obj_in_list() is declared in handler.hpp; dg_scripts.cpp defines it. */
struct obj_data *get_object_in_equip(struct char_data *ch, const char *name);
void script_trigger_check(void);
void check_time_triggers(void);
void find_uid_name(const char *uid, char *name, size_t nlen);
void do_sstat_room(struct char_data *ch, struct room_data *r);
void do_sstat_object(struct char_data *ch, struct obj_data *j);
void do_sstat_character(struct char_data *ch, struct char_data *k);
void add_trigger(struct script_data *sc, struct trig_data *t, int loc);
void script_log(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
char *matching_quote(char *p);
struct room_data *dg_room_of_obj(struct obj_data *obj);
int trig_is_attached(struct script_data *sc, vnum_t trig_num);
void dg_flush_pending_extractions(void);
const char *dg_edit_door(char *argument, char *errbuf, size_t errbuf_size);
void dg_note_char_extraction(struct char_data *ch);
void dg_note_obj_extraction(struct obj_data *obj);

/* To maintain strict aliasing the caller hands us the address of its pointer. */
int script_driver(void *go_adress, struct trig_data *trig, int type, int mode);
/* As script_driver, but says what a script that never runs `return` should
 * yield. script_driver passes 1, which every trigger type but one reads as
 * "allow"; the damage trigger reads its return as an amount and passes the
 * damage that was incoming, so that not returning leaves the hit alone. */
int script_driver_default(void *go_adress, struct trig_data *trig, int type, int mode,
                          int default_ret);
rnum_t real_trigger(vnum_t vnum);
void process_eval(void *go, struct script_data *sc, struct trig_data *trig,
                  int type, char *cmd);
void init_lookup_table(void);
void add_to_lookup_table(long uid, void *c);
void remove_from_lookup_table(long uid);

/* from dg_db_scripts.cpp */
void parse_trigger(File &fl, long nr);
struct trig_data *read_trigger(rnum_t nr);
void trig_data_copy(struct trig_data *this_data, const struct trig_data *trg);
void dg_read_trigger_list(const char *line, void *proto, int type);
void assign_triggers(void *i, int type);
void write_trigs_to_disk(vnum_t zone_vnum);
const char *dg_render_proto_list(void *proto, int type);

/* From dg_variables.cpp */
void add_var(struct trig_var_data **var_list, const char *name, const char *value, long id);
int item_in_list(const char *item, struct obj_data *list);
int char_has_item(const char *item, struct char_data *ch);
void var_subst(void *go, struct script_data *sc, struct trig_data *trig,
               int type, char *line, char *buf);
int text_processed(const char *field, const char *subfield, struct trig_var_data *vd,
                   char *str, size_t slen);
void find_replacement(void *go, struct script_data *sc, struct trig_data *trig,
                      int type, char *var, char *field, char *subfield, char *str, size_t slen);

/* From dg_handler.cpp */
void free_var_el(struct trig_var_data *var);
void free_varlist(struct trig_var_data *vd);
int remove_var(struct trig_var_data **var_list, const char *name);
void free_trigger(struct trig_data *trig);
void extract_trigger(struct trig_data *trig);
void extract_script(void *thing, int type);
void extract_script_mem(struct script_memory *sc);
void free_proto_script(void *thing, int type);
void copy_proto_script(void *source, void *dest, int type);
void update_wait_events(struct room_data *to, struct room_data *from);

/* from dg_comm.cpp */
char *any_one_name(char *argument, char *first_arg);
void sub_write(char *arg, struct char_data *ch, byte find_invis, int targets);
void dg_send_to_zone(const char *messg, int zone);

/* from dg_misc.cpp */
void do_dg_affect(void *go, struct script_data *sc, struct trig_data *trig,
                  int type, char *cmd);
int valid_dg_target(struct char_data *ch, int bitvector);
void script_damage(struct char_data *vict, int dam);

/* from dg_mobcmd.cpp */
void mob_command_interpreter(struct char_data *ch, char *argument);

/* from dg_objcmd.cpp */
struct room_data *obj_room(struct obj_data *obj);
void obj_command_interpreter(struct obj_data *obj, char *argument);

/* from dg_wldcmd.cpp */
void wld_command_interpreter(struct room_data *room, char *argument);

/* from trigedit.cpp */
void trigedit_parse(struct descriptor_data *d, const char *arg);
void trigedit_save(struct descriptor_data *d);
void trigedit_string_cleanup(struct descriptor_data *d, int terminator);
int format_script(struct descriptor_data *d);
void trigedit_setup_existing(struct descriptor_data *d, rnum_t rtrg_num);
void trigedit_setup_new(struct descriptor_data *d);
void trigedit_disp_menu(struct descriptor_data *d);

/* defines for valid_dg_target */
#define DG_ALLOW_GODS (1 << 0)

/* Macros for scripts */
#define UID_CHAR   '}'
#define GET_TRIG_NAME(t)          ((t)->name)
#define GET_TRIG_RNUM(t)          ((t)->nr)
#define GET_TRIG_VNUM(t)          (trig_index[(t)->nr]->vnum)
#define GET_TRIG_TYPE(t)          ((t)->trigger_type)
#define GET_TRIG_DATA_TYPE(t)     ((t)->data_type)
#define GET_TRIG_NARG(t)          ((t)->narg)
#define GET_TRIG_ARG(t)           ((t)->arglist)
#define GET_TRIG_VARS(t)          ((t)->var_list)
#define GET_TRIG_WAIT(t)          ((t)->wait_event)
#define GET_TRIG_DEPTH(t)         ((t)->depth)
#define GET_TRIG_LOOPS(t)         ((t)->loops)

/* player ids: 0 to MOB_ID_BASE - 1
 * mob ids: MOB_ID_BASE to ROOM_ID_BASE - 1
 * room ids: ROOM_ID_BASE to OBJ_ID_BASE - 1
 * object ids: OBJ_ID_BASE and higher */
#define MOB_ID_BASE     10000000  /* 10000000 player IDNUMs should suffice */
#define ROOM_ID_BASE    (10000000 + MOB_ID_BASE) /* 10000000 mobs */
#define OBJ_ID_BASE     (10000000 + ROOM_ID_BASE) /* 10000000 rooms */

#define SCRIPT(o)                 ((o)->script)
#define SCRIPT_MEM(c)             ((c)->script_memory)

#define SCRIPT_TYPES(s)           ((s)->types)
#define TRIGGERS(s)               ((s)->trig_list)

#define SCRIPT_CHECK(go, type)   (SCRIPT(go) && \
                                  IS_SET(SCRIPT_TYPES(SCRIPT(go)), type))
#define TRIGGER_CHECK(t, type)   (IS_SET(GET_TRIG_TYPE(t), type) && \
                                  !GET_TRIG_DEPTH(t))

#define ADD_UID_VAR(buf, trig, id, name, context) do { \
                           snprintf(buf, sizeof(buf), "%c%ld", UID_CHAR, id); \
                           add_var(&GET_TRIG_VARS(trig), name, buf, context); } while (0)

/* id helpers */
extern long char_script_id(struct char_data *ch);
extern long obj_script_id(struct obj_data *obj);

#define room_script_id(room)  ((long)(room)->number + ROOM_ID_BASE)

/* Triggers get their own index type: unlike mob_index and obj_index, an entry
 * has to carry the prototype itself, since there is no trig_proto array. */
struct trig_index_data {
  vnum_t vnum;                 /* virtual number of this trigger  */
  long number;                 /* how many of it are live         */
  struct trig_data *proto;     /* the prototype                   */

  trig_index_data() : vnum(NOTHING), number(0), proto(NULL) {}
};

/* The trigger index, and the head of the world-wide trigger list. */
extern struct trig_index_data **trig_index;
extern struct trig_data *trigger_list;
extern rnum_t top_of_trigt;

/* Set while a purge command destroys the very thing running the script. */
extern int dg_owner_purged;

#endif /* _dg_scripts_hpp_ */
