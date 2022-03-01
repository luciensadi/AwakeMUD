/* ************************************************************************
*   File: utils.h                                       Part of CircleMUD *
*  Usage: header file: utility macros and prototypes of utility funcs     *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#ifndef _utils_h_
#define _utils_h_

#include <stdio.h>
#include "bitfield.h"

#if defined(osx)
/* crypt() is defined in unistd.h in OSX. */
#include <unistd.h>
#endif

/* external declarations and prototypes **********************************/

extern struct weather_data weather_info;

struct char_data;
struct obj_data;

/* public functions in utils.c */
bool    has_cyberweapon(struct char_data *ch);
char *  str_dup(const char *source);
char *  str_str( const char *str1, const char *str2 );
int     str_cmp(const char *arg1, const char *arg2);
int     strn_cmp(const char *arg1, const char *arg2, int n);
size_t  strlcpy(char *buf, const char *src, size_t bufsz);
size_t  strlcat(char *buf, const char *src, size_t bufsz);
void    log(const char *str);
void    log_vfprintf(const char *format, ...);
void    mudlog(const char *str, struct char_data *ch, int log, bool file);
void    log_death_trap(struct char_data *ch);
int     number(int from, int to);
int     dice(int number, int size);
void    sprintbit(long vektor, const char *names[], char *result, size_t result_size);
void    sprinttype(int type, const char *names[], char *result, size_t result_size);
void    sprint_obj_mods(struct obj_data *obj, char *result, size_t result_size);
int     get_line(FILE *fl, char *buf);
struct  time_info_data age(struct char_data *ch);
int     convert_damage(int damage);
int     srdice(void);
int     success_test(int number, int target);
int     resisted_test(int num4ch, int tar4ch, int num4vict, int tar4vict);
int     stage(int successes, int wound);
bool    access_level(struct char_data *ch, int level);
char *  buf_mod(char *buf, int buf_len, const char *name, int bonus);
//char *  buf_roll(char *buf, const char *name, int bonus);
int     modify_target_rbuf_raw(struct char_data *ch, char *rbuf, int rbuf_len, int current_visibility_penalty, bool skill_is_magic);
int     modify_target_rbuf(struct char_data *ch, char *rbuf, int rbuf_len);
int     modify_target(struct char_data *ch);
int     damage_modifier(struct char_data *ch, char *rbuf, int rbuf_len);
int     sustain_modifier(struct char_data *ch, char *rbuf, size_t rbuf_len, bool minus_one_sustained=0);
char *  capitalize(const char *source);
char *  decapitalize_a_an(const char *source);
char *  string_to_uppercase(const char *source);
char *  string_to_lowercase(const char *source);
int     get_speed(struct veh_data *veh);
int     negotiate(struct char_data *ch, struct char_data *tch, int comp, int basevalue, int mod, bool buy, bool include_metavariant_penalty=1);
float   gen_size(int race, bool height, int size, int sex);
int     get_skill(struct char_data *ch, int skill, int &target);
void    add_follower(struct char_data *ch, struct char_data *leader);
int     light_level(struct room_data *room);
bool    biocyber_compatibility(struct obj_data *obj1, struct obj_data *obj2, struct char_data *ch);
void    magic_loss(struct char_data *ch, int magic, bool msg);
bool    has_kit(struct char_data *ch, int type);
struct  obj_data *find_workshop(struct char_data *ch, int type);
void    add_workshop_to_room(struct obj_data *obj);
void    remove_workshop_from_room(struct obj_data *obj);
bool    mount_has_weapon(struct obj_data *mount);
struct  obj_data *get_mount_weapon(struct obj_data *mount);
struct  obj_data *get_mount_ammo(struct obj_data *mount);
struct  obj_data *stop_manning_weapon_mounts(struct char_data *ch, bool send_message);
struct  obj_data *get_mount_manned_by_ch(struct char_data *ch);
void    terminate_mud_process_with_message(const char *message, int error_code);
bool    char_can_make_noise(struct char_data *ch, const char *message = NULL);
struct  char_data *get_driver(struct veh_data *veh);
struct  obj_data *find_matching_obj_in_container(struct obj_data *container, vnum_t vnum);
bool    attach_attachment_to_weapon(struct obj_data *attachment, struct obj_data *weapon, struct char_data *ch, int location);
struct  obj_data *unattach_attachment_from_weapon(int location, struct obj_data *weapon, struct char_data *ch);
void    copy_over_necessary_info(struct char_data *original, struct char_data *clone);
void    clear_editing_data(struct descriptor_data *d);
char *  double_up_color_codes(const char *string);
struct  char_data *get_obj_carried_by_recursive(struct obj_data *obj);
struct  char_data *get_obj_worn_by_recursive(struct obj_data *obj);
struct  char_data *get_obj_possessor(struct obj_data *obj);
char *  generate_new_loggable_representation(struct obj_data *obj);
void    purgelog(struct veh_data *veh);
char *  replace_substring(char *source, char *dest, const char *replace_target, const char *replacement);
bool    combine_ammo_boxes(struct char_data *ch, struct obj_data *from, struct obj_data *into, bool print_messages);
void    update_ammobox_ammo_quantity(struct obj_data *ammobox, int amount);
void    destroy_door(struct room_data *room, int dir);
bool    spell_is_nerp(int spell_num);
char    get_final_character_from_string(const char *str);
bool    builder_cant_go_there(struct char_data *ch, struct room_data *room);
bool    get_and_deduct_one_deckbuilding_token_from_char(struct char_data *ch);
bool    program_can_be_copied(struct obj_data *prog);
struct  obj_data *get_obj_proto_for_vnum(vnum_t vnum);
void    set_natural_vision_for_race(struct char_data *ch);
int     get_string_length_after_color_code_removal(const char *str, struct char_data *ch_to_notify_of_failure_reason);
char *  get_string_after_color_code_removal(const char *str, struct char_data *ch);
int     count_color_codes_in_string(const char *str);
bool    npc_is_protected_by_spec(struct char_data *npc);
bool    can_damage_vehicle(struct char_data *ch, struct veh_data *veh);
char *  compose_spell_name(int type, int subtype = -1);
bool    obj_contains_kept_items(struct obj_data *obj);
void    send_gamewide_annoucement(const char *msg, bool prepend_announcement_string);
char *  get_printable_mob_unique_id(struct char_data *ch);
bool    mob_unique_id_matches(mob_unique_id_t id1, mob_unique_id_t id2);
void    set_new_mobile_unique_id(struct char_data *ch);
int     return_general(int skill_num);
bool    perform_knockdown_test(struct char_data *ch, int initial_tn, int successes_to_avoid_knockback=0);

// Skill-related.
char *how_good(int skill, int rank);
const char *skill_rank_name(int rank, bool knowledge);
void set_character_skill(struct char_data *ch, int skill_num, int new_value, bool send_message);

// Message history management and manipulation.
void    store_message_to_history(struct descriptor_data *d, int channel, const char *mallocd_message);
void    delete_message_history(struct descriptor_data *d);

// Room finders.
struct room_data *get_veh_in_room(struct veh_data *veh);
struct room_data *get_ch_in_room(struct char_data *ch);
struct room_data *get_obj_in_room(struct obj_data *obj);

// Visibility functions.
bool invis_ok(struct char_data *ch, struct char_data *vict);

/* undefine MAX and MIN so that our functions are used instead */
#ifdef MAX
#undef MAX
#endif

#ifdef MIN
#undef MIN
#endif

#ifdef __GNUG__
#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x > y ? x : y)
#else
// Word to the wise: If you use this with a float, etc: I will cut you. -LS
int MAX(long a, long b);
int MIN(long a, long b);
#endif

#define RM_BLOOD(rm) ((rm).blood)
/* Ok im done here -- root */

/* in magic.c */
bool    circle_follow(struct char_data *ch, struct char_data * victim);

/* in act.informative.c */
void    look_at_room(struct char_data *ch, int mode, int is_quicklook);

/* in act.movmement.c */
int     do_simple_move(struct char_data *ch, int dir, int extra, struct
                       char_data *vict);
int perform_move(struct char_data *ch, int dir, int extra, struct char_data
                 *vict);

// Currently not used anywhere.
void    reverse_obj_list(struct obj_data **obj);

void    mental_gain(struct char_data *ch);
void    physical_gain(struct char_data *ch);
void    advance_level(struct char_data *ch);
void    set_title(struct char_data *ch, const char *title);
void    set_pretitle(struct char_data *ch, const char *title);
void    set_whotitle(struct char_data *ch, const char *title);
int     gain_karma(struct char_data * ch, int gain, bool rep, bool limits, bool multiplier);
void    gain_condition(struct char_data *ch, int condition, int value);
void    update_pos(struct char_data *victim);


/* various constants *****************************************************/


/* defines for damage catagories */
#define LIGHT         1
#define MODERATE      2
#define SERIOUS       3
#define DEADLY        4

/* breadth-first searching */
#define BFS_ERROR               -1
#define BFS_ALREADY_THERE       -2
#define BFS_NO_PATH             -3

/* mud-life time */
#define SECS_PER_MUD_MINUTE 2
#define SECS_PER_MUD_HOUR       (60*SECS_PER_MUD_MINUTE)
#define SECS_PER_MUD_DAY        (24*SECS_PER_MUD_HOUR)
#define SECS_PER_MUD_MONTH      (30*SECS_PER_MUD_DAY)
#define SECS_PER_MUD_YEAR       (12*SECS_PER_MUD_MONTH)

/* real-life time (remember Real Life?) */
#define SECS_PER_REAL_MIN       60
#define SECS_PER_REAL_HOUR      (60*SECS_PER_REAL_MIN)
#define SECS_PER_REAL_DAY       (24*SECS_PER_REAL_HOUR)
#define SECS_PER_REAL_YEAR      (365*SECS_PER_REAL_DAY)

/* string utils **********************************************************/

#define ENDOF(buffer) ((buffer) + strlen(buffer))
#define YESNO(a) ((a) ? "YES" : "NO")
#define ONOFF(a) ((a) ? "ON" : "OFF")

#define LOWER(c)   (((c)>='A'  && (c) <= 'Z') ? ((c)+('a'-'A')) : (c))
#define UPPER(c)   (((c)>='a'  && (c) <= 'z') ? ((c)+('A'-'a')) : (c) )

#define ISNEWL(ch) ((ch) == '\n' || (ch) == '\r')
#define IF_STR(st) ((st) ? (st) : "\0")

// Calls a function that uses a static buffer.  Good.
#define CAP(st)  capitalize((st))

#define AN(string) (strchr("aeiouAEIOU", *string) ? "an" : "a")


#define CREATE(result, type, number)  do {\
 if ((number) * sizeof(type) <= 0) \
  log_vfprintf("SYSERR: Zero bytes or less requested at %s:%d.", __FILE__, __LINE__); \
 if (!((result) = (type *) calloc ((number), sizeof(type)))) \
  { perror("SYSERR: malloc failure"); exit(ERROR_MALLOC_FAILED_IN_CREATE_MACRO); } } while(0)

/*
 * the source previously used the same code in many places to remove an item
 * from a list: if it's the list head, change the head, else traverse the
 * list looking for the item before the one to be removed.  Now, we have a
 * macro to do this.  To use, just make sure that there is a variable 'temp'
 * declared as the same type as the list to be manipulated.  BTW, this is
 * a great application for C++ templates but, alas, this is not C++.  Maybe
 * CircleMUD 4.0 will be...
 */
#define REMOVE_FROM_LIST(item, head, nextc)      \
{  if ((item) == (head))                         \
      head = (item)->nextc;                      \
   else {                                        \
      temp = head;                               \
      while (temp && (temp->nextc != (item)))    \
         temp = temp->nextc;                     \
      if (temp)                                  \
         temp->nextc = (item)->nextc;            \
   }                                             \
   (item)->nextc = NULL;                         \
}


/* basic bitvector utils *************************************************/

#define EXIT2(room, door) ((room)->dir_option[(door)])
#define CAN_GO2(room, door) (EXIT2(room, door) && \
        (EXIT2(room, door)->to_room && EXIT2(room, door)->to_room != &world[0]) && \
        !IS_SET(EXIT2(room,door)->exit_info, EX_CLOSED))

#define VEH_FLAGS(ch) ((ch).flags)
#define MOB_FLAGS(ch) ((ch)->char_specials.saved.act)
#define PLR_FLAGS(ch) ((ch)->char_specials.saved.act)
#define PRF_FLAGS(ch) ((ch)->player_specials->saved.pref)
#define AFF_FLAGS(ch) ((ch)->char_specials.saved.affected_by)
#define ROOM_FLAGS(loc) ((loc)->room_flags)

#define IS_NPC(ch)  (MOB_FLAGS(ch).IsSet(MOB_ISNPC))
#define IS_MOB(ch)  (IS_NPC(ch) && (GET_MOB_RNUM((ch)) >-1))
#define IS_PROJECT(ch) (IS_NPC(ch) && ch->desc && ch->desc->original && \
  PLR_FLAGS(ch->desc->original).IsSet(PLR_PROJECT) && \
  GET_MOB_VNUM(ch) == 22)
#define IS_SPIRIT(ch) (IS_NPC(ch) && GET_RACE(ch) == RACE_SPIRIT)
#define IS_ANY_ELEMENTAL(ch) (IS_NPC(ch) && (GET_RACE(ch) == RACE_ELEMENTAL || GET_RACE(ch) == RACE_PC_CONJURED_ELEMENTAL))
#define IS_WILD_ELEMENTAL(ch) (IS_NPC(ch) && GET_RACE(ch) == RACE_ELEMENTAL)
#define IS_PC_CONJURED_ELEMENTAL(ch) (IS_NPC(ch) && GET_RACE(ch) == RACE_PC_CONJURED_ELEMENTAL)
#define IS_ASTRAL(ch) (MOB_FLAGGED(ch, MOB_ASTRAL) || IS_PROJECT(ch))
#define IS_DUAL(ch)   (MOB_FLAGGED(ch, MOB_DUAL_NATURE) || PLR_FLAGGED(ch, PLR_PERCEIVE) || access_level(ch, LVL_ADMIN))
#define IS_SENATOR(ch) (access_level((ch), LVL_BUILDER))

// ONLY for use on non-Bitfield bitvectors:
#define IS_SET(set, flag)     ((set) & (flag))
#define SET_BIT(set, flag)    ((set) |= (flag))
#define REMOVE_BIT(set, flag) ((set) &= ~(flag))
#define TOGGLE_BIT(set, flag) ((set) ^= (flag))

#define MOB_FLAGGED(ch, flag) (IS_NPC(ch) && MOB_FLAGS(ch).IsSet((flag)))
#define PLR_FLAGGED(ch, flag) (!IS_NPC(ch) && PLR_FLAGS(ch).IsSet((flag)))
#define AFF_FLAGGED(ch, flag) (AFF_FLAGS(ch).IsSet((flag)))
#define VEH_FLAGGED(ch, flag) (VEH_FLAGS(ch).IsSet((flag)))
#define PRF_FLAGGED(ch, flag) \
  ((ch->desc && ch->desc->original) \
   ? PRF_FLAGS(ch->desc->original).IsSet((flag)) \
   : PRF_FLAGS(ch).IsSet((flag)))
#define D_PRF_FLAGGED(d, flag)                   \
((d)->original                                   \
  ? PRF_FLAGS((d)->original).IsSet((flag))       \
  : ((d)->character                              \
      ? PRF_FLAGS((d)->character).IsSet((flag))  \
      : FALSE                                    \
    )                                            \
)
#define ROOM_FLAGGED(loc, flag) ((loc) ? (loc)->room_flags.IsSet((flag)) : FALSE)

/* IS_AFFECTED for backwards compatibility */
#define IS_AFFECTED(ch, skill) (AFF_FLAGGED((ch), (skill)))

extern bool PRF_TOG_CHK(char_data *ch, dword offset);
extern bool PLR_TOG_CHK(char_data *ch, dword offset);

/* room utils ************************************************************/


#define SECT(room) ((room) ? (room)->sector_type : SPIRIT_HEARTH)
#define IS_WATER(room) (SECT((room)) == SPIRIT_LAKE || SECT((room)) == SPIRIT_RIVER || SECT((room)) == SPIRIT_SEA)

#define IS_DARK(room)  (light_level((room)) == ITEM_FULLDARK)
#define IS_LIGHT(room)  (light_level((room)) <= LIGHT_NORMALNOLIT || light_level((room)) == LIGHT_PARTLIGHT)
#define IS_LOW(room)	(light_level((room)) == LIGHT_MINLIGHT || light_level((room)) == LIGHT_PARTLIGHT)

#define GET_ROOM_NAME(room) ((room) ? (room)->name : "(null room name)")
#define GET_ROOM_DESC(room) ((room) ? ((room)->night_desc && weather_info.sunlight == SUN_DARK ? (room)->night_desc : (room)->description) : "(null room desc)")

#define VALID_ROOM_RNUM(rnum) ((rnum) != NOWHERE && (rnum) <= top_of_world)

#define GET_ROOM_SPEC(room) \
 ((room) ? (room)->func : NULL)

#define GET_ROOM_VNUM(room) \
 ((vnum_t)((room) ? (room)->number : NOWHERE))

#define GET_BACKGROUND_COUNT(room) ((room) ? (room)->background[0] : 0)
#define GET_BACKGROUND_AURA(room)  ((room) ? (room)->background[1] : 0)
#define GET_SETTABLE_BACKGROUND_COUNT(room) ((room)->background[0])
#define GET_SETTABLE_BACKGROUND_AURA(room)  ((room)->background[1])

/* zone utils ************************************************************/

#define GET_JURISDICTION(room) (zone_table[(room)->zone].jurisdiction)
#define GET_SECURITY_LEVEL(room) (zone_table[(room)->zone].security)

/* char utils ************************************************************/

#define GET_WAS_IN(ch)  ((ch)->was_in_room)

#define GET_VEH_NAME(veh) (decapitalize_a_an((veh)->restring ? (veh)->restring : ((veh)->short_description ? (veh)->short_description : "an ERRONEOUS VEHICLE")))
#define GET_VEH_DESC(veh) ((veh)->restring_long ? (veh)->restring_long : (veh)->long_description)
#define GET_VEH_RNUM(veh) ((veh)->veh_number)
#define GET_VEH_VNUM(veh) (GET_VEH_RNUM(veh) >= 0 ? veh_index[GET_VEH_RNUM(veh)].vnum : -1)
#define GET_OBJ_NAME(obj) ((obj)->restring ? (obj)->restring : (obj)->text.name)
#define GET_OBJ_DESC(obj) ((obj)->photo ? (obj)->photo : (obj)->text.look_desc)
#define GET_KEYWORDS(ch)  ((ch)->player.physical_text.keywords)
#define GET_NAME(ch)      ((ch)->player.physical_text.name)
#define GET_CHAR_NAME(ch) \
  (((ch)->desc && (ch)->desc->original) ? \
   (ch)->desc->original->player.char_name : ((ch)->player.char_name ? (ch)->player.char_name : GET_NAME((ch))))
#define GET_TITLE(ch)   ((ch)->player.title)
#define GET_PRETITLE(ch) ((ch)->player.pretitle)
#define GET_WHOTITLE(ch) ((ch)->player.whotitle)
#define GET_LEVEL(ch)   ((ch)->player.level)
#define GET_PASSWD(ch)  ((ch)->player.passwd)
#define GET_EMAIL(ch)   ((ch)->player.email ? (ch)->player.email : "not set")
#define SETTABLE_EMAIL(ch)   ((ch)->player.email)
#define GET_CHAR_MULTIPLIER(ch) ((ch)->player.multiplier)

/*
 * I wonder if this definition of GET_REAL_LEVEL should be the definition
 * of GET_LEVEL?  JE
 */
#define GET_REAL_LEVEL(ch) \
   (ch->desc && ch->desc->original ? GET_LEVEL(ch->desc->original) : \
    GET_LEVEL(ch))

#define GET_DESC_LEVEL(d)  ((d)->original ? GET_LEVEL((d)->original) : ((d)->character ? GET_LEVEL((d)->character) : 0))

#define GET_RACE(ch)          ((ch)->player.race)
#define GET_TRADITION(ch)       ((ch)->player.tradition)
#define GET_ASPECT(ch)		((ch)->player.aspect)
#define GET_LASTROOM(ch)          ((ch)->player.last_room)
#define GET_HEIGHT(ch)        ((ch)->player.height)
#define GET_WEIGHT(ch)        ((ch)->player.weight)
#define GET_SEX(ch)           ((ch)->player.sex)

#define GET_ATT(ch, i)        ((ch)->aff_abils.attributes[(i)])
#define GET_REAL_ATT(ch, i)   ((ch)->real_abils.attributes[(i)])
#define GET_STR(ch)           (GET_ATT((ch), STR))
#define GET_QUI(ch)           (GET_ATT((ch), QUI))
#define GET_INT(ch)           (GET_ATT((ch), INT))
#define GET_WIL(ch)           (GET_ATT((ch), WIL))
#define GET_BOD(ch)           (GET_ATT((ch), BOD))
#define GET_CHA(ch)           (GET_ATT((ch), CHA))
#define GET_REA(ch)           (GET_ATT((ch), REA))
#define GET_MAG(ch)           ((ch)->aff_abils.mag)
#define GET_ESS(ch)           ((ch)->aff_abils.ess)
#define GET_ESSHOLE(ch)       ((ch)->real_abils.esshole)
#define GET_INDEX(ch)         ((ch)->real_abils.bod_index)
#define GET_HIGHEST_INDEX(ch)      ((ch)->real_abils.highestindex)
#define GET_BIOOVER(ch)       (-((int)((GET_ESS((ch)) + 300) - GET_INDEX((ch))) / 100))
#define GET_TEMP_MAGIC_LOSS(ch)	((ch)->points.magic_loss)
#define GET_TEMP_ESSLOSS(ch)	((ch)->points.ess_loss)

#define GET_REAL_ATT(ch, i)        ((ch)->real_abils.attributes[(i)])
#define GET_REAL_STR(ch)           (GET_REAL_ATT((ch), STR))
#define GET_REAL_QUI(ch)           (GET_REAL_ATT((ch), QUI))
#define GET_REAL_INT(ch)           (GET_REAL_ATT((ch), INT))
#define GET_REAL_WIL(ch)           (GET_REAL_ATT((ch), WIL))
#define GET_REAL_BOD(ch)           (GET_REAL_ATT((ch), BOD))
#define GET_REAL_CHA(ch)           (GET_REAL_ATT((ch), CHA))
#define GET_REAL_REA(ch)           (GET_REAL_ATT((ch), REA))
#define GET_REAL_MAG(ch)           ((ch)->real_abils.mag)
#define GET_REAL_ESS(ch)           ((ch)->real_abils.ess)

#define GET_KARMA(ch)         ((ch)->points.karma)
#define GET_REP(ch)           ((ch)->points.rep)
#define GET_NOT(ch)           ((ch)->points.noto)
#define GET_TKE(ch)           ((ch)->points.tke)
#define GET_SIG(ch)	      ((ch)->points.sig)

#define GET_INNATE_BALLISTIC(ch)       ((ch)->points.ballistic[0])
#define GET_TOTALBAL(ch)               ((ch)->points.ballistic[1])
#define GET_BALLISTIC(ch)              ((ch)->points.ballistic[2])
#define GET_INNATE_IMPACT(ch)          ((ch)->points.impact[0])
#define GET_TOTALIMP(ch)               ((ch)->points.impact[1])
#define GET_IMPACT(ch)                 ((ch)->points.impact[2])
int get_armor_penalty_grade(struct char_data *ch);

#define GET_PHYSICAL(ch)        ((ch)->points.physical)
#define GET_MAX_PHYSICAL(ch)  ((ch)->points.max_physical)
#define GET_GRADE(ch)   	((ch)->points.grade)
#define GET_MENTAL(ch)          ((ch)->points.mental)
#define GET_MAX_MENTAL(ch)      ((ch)->points.max_mental)

#define SHOULD_SEE_TIPS(ch)     (PRF_FLAGGED((ch), PRF_SEE_TIPS))

// Changed to a non-assignable expression to cause all code that sets this value to not compile.
// This makes it easier to find outliers when looking to track nuyen inflow / outflows.
#define GET_NUYEN(ch)           ((ch)->points.nuyen != 0 ? (ch)->points.nuyen : 0)
#define GET_NUYEN_RAW(ch)       ((ch)->points.nuyen)

// Same deal here.
#define GET_BANK(ch)            ((ch)->points.bank != 0 ? (ch)->points.bank : 0)
#define GET_BANK_RAW(ch)        ((ch)->points.bank)

#define GET_INIT_DICE(ch)       ((ch)->points.init_dice)
#define GET_INIT_ROLL(ch)       ((ch)->points.init_roll)
#define GET_SUSTAINED_NUM(ch)	((ch)->points.sustained[0])
#define GET_SUSTAINED_FOCI(ch)  ((ch)->points.sustained[1])
#define GET_SUSTAINED(ch)	((ch)->sustained)
#define SPIRIT_SUST(ch)		((ch)->ssust)
#define GET_SPELLS(ch)          ((ch)->desc && (ch)->desc->original ? (ch)->desc->original->spells : (ch)->spells)
#define GET_REACH(ch)		((ch)->points.reach[0])
#define GET_REACH_USE(ch)	((ch)->points.reach[1])

#define GET_POS(ch)             ((ch)->char_specials.position)
#define GET_DEFPOS(ch)          ((ch)->char_specials.defined_position)
#define GET_IDNUM(ch)           ((ch)->char_specials.idnum)
#define GET_IDNUM_EVEN_IF_PROJECTING(ch)  ((((ch) && (ch)->desc && (ch)->desc->original) ? (ch)->desc->original : (ch))->char_specials.idnum)
#define IS_CARRYING_W(ch)       ((ch)->char_specials.carry_weight)
#define IS_CARRYING_N(ch)       ((ch)->char_specials.carry_items)
#define FIGHTING(ch)            ((ch)->char_specials.fighting)
#define FIGHTING_VEH(ch)        ((ch)->char_specials.fight_veh)
#define CH_IN_COMBAT(ch)        (FIGHTING(ch) || FIGHTING_VEH(ch))
#define HUNTING(ch)             ((ch)->char_specials.hunting)
#define IS_NERVE(ch)		((ch)->char_specials.nervestrike)
#define GET_TEMP_QUI_LOSS(ch)	((ch)->char_specials.tempquiloss)
#define GET_COST_BREAKUP(ch)	((ch)->char_specials.cost_breakup)
#define GET_AVAIL_OFFSET(ch)	((ch)->char_specials.avail_offset)
#define CHAR_X(ch)		((ch)->char_specials.coord[0])
#define CHAR_Y(ch)		((ch)->char_specials.coord[1])
#define CHAR_Z(ch)		((ch)->char_specials.coord[2])

#define SHOOTING_DIR(ch)        ((ch)->char_specials.shooting_dir)

#define GET_LANGUAGE(ch)      ((ch)->char_specials.saved.cur_lang)
#define GET_NUM_FIGHTING(ch)  ((ch)->char_specials.fightList.NumItems())
#define GET_NUM_ATTACKING(ch) ((ch)->char_specials.defendList.NumItems())

#define GET_COND(ch, i)         ((ch)->player_specials->saved.conditions[(i)])
#define GET_LOADROOM(ch)        ((ch)->player_specials->saved.load_room)
#define GET_LAST_IN(ch)         ((ch)->player_specials->saved.last_in)
#define GET_PRACTICES(ch)       ((ch)->player_specials->saved.spells_to_learn)
#define GET_INVIS_LEV(ch)       ((ch)->player_specials->saved.invis_level)
#define GET_INCOG_LEV(ch)       ((ch)->player_specials->saved.incog_level)
#define GET_WIMP_LEV(ch)        ((ch)->player_specials->saved.wimp_level)
#define GET_FREEZE_LEV(ch)      ((ch)->player_specials->saved.freeze_level)
#define GET_BAD_PWS(ch)         ((ch)->player_specials->saved.bad_pws)
#define GET_SYSTEM_POINTS(ch)   ((ch)->player_specials->saved.system_points)
#define GET_WATCH(ch)		        ((ch)->player_specials->watching)
#define GET_ASTRAL(ch)          ((ch)->aff_abils.astral_pool)
#define GET_DEFENSE(ch)         ((ch)->aff_abils.defense_pool)
#define GET_BODY(ch)            ((ch)->aff_abils.body_pool)
#define GET_COMBAT(ch)          ((ch)->aff_abils.combat_pool)
#define GET_OFFENSE(ch)         ((ch)->aff_abils.offense_pool)
#define GET_TASK_POOL(ch, attr) ((ch)->aff_abils.task_pool[attr])
#define GET_CONTROL(ch)         ((ch)->aff_abils.control_pool)
#define GET_HACKING(ch)         ((ch)->aff_abils.hacking_pool)
#define GET_MAX_HACKING(ch)     ((ch)->aff_abils.hacking_pool_max)
#define GET_REM_HACKING(ch)     ((ch)->aff_abils.hacking_pool_remaining)
#define GET_MAGIC(ch)           ((ch)->aff_abils.magic_pool)
#define GET_CASTING(ch)		      ((ch)->aff_abils.casting_pool)
#define GET_DRAIN(ch)	        	((ch)->aff_abils.drain_pool)
#define GET_SDEFENSE(ch)	      ((ch)->aff_abils.spell_defense_pool)
#define GET_REFLECT(ch)		      ((ch)->aff_abils.reflection_pool)
#define GET_SPIRIT(ch)          ((ch)->char_specials.spirits)
#define GET_NUM_SPIRITS(ch)     ((ch)->char_specials.num_spirits)
#define GET_DOMAIN(ch)		      ((ch)->points.domain)
#define GET_ATT_POINTS(ch)	    ((ch)->player_specials->saved.att_points)
#define GET_SKILL_POINTS(ch)  	((ch)->player_specials->saved.skill_points)
#define GET_FORCE_POINTS(ch)	  ((ch)->player_specials->saved.force_points)
#define GET_RESTRING_POINTS(ch)	((ch)->player_specials->saved.restring_points)
#define GET_ARCHETYPAL_TYPE(ch)	((ch)->player_specials->saved.archetype)
#define GET_ARCHETYPAL_MODE(ch)	((ch)->player_specials->saved.archetypal)
#define GET_TARGET_MOD(ch)	((ch)->char_specials.target_mod)
#define LAST_HEAL(ch)		((ch)->char_specials.last_healed)
#define GET_FOCI(ch)		((ch)->char_specials.foci)
#define GET_QUEST(ch)		((ch)->desc && (ch)->desc->original ? (ch)->desc->original->player_specials->questnum : \
                                                                      (ch)->player_specials->questnum)
#define GET_LQUEST(ch, i)	      ((ch)->player_specials->last_quest[i])
#define POOFIN(ch)              ((ch)->player.poofin)
#define POOFOUT(ch)             ((ch)->player.poofout)
#define GET_PROMPT(ch)          ((PLR_FLAGGED((ch), PLR_MATRIX) ? (ch)->player.matrixprompt : (ch)->player.prompt))
#define GET_ALIASES(ch)         ((ch)->desc && (ch)->desc->original ? (ch)->desc->original->player_specials->aliases : (ch)->player_specials->aliases)
#define GET_PLAYER_MEMORY(ch)   ((ch)->player_specials->remem)
#define GET_LAST_TELL(ch)	((ch)->player_specials->last_tell)
#define GET_LAST_DAMAGETIME(ch)	((ch)->points.lastdamage)
#define HOURS_LEFT_TRACK(ch)	((ch)->points.track[0])
#define HOURS_SINCE_TRACK(ch)	((ch)->points.track[1])
#define NATURAL_VISION(ch)	((ch)->points.vision[0])
#define CURRENT_VISION(ch)	((ch)->points.vision[1])
#define SHOTS_FIRED(ch)		((ch)->points.extras[0])
#define SHOTS_TRIGGERED(ch)	((ch)->points.extras[1])
#define RIG_VEH(ch, veh)	((veh) = ((ch)->char_specials.rigging ? (ch)->char_specials.rigging : (ch)->in_veh));


/* the skills structure was moved to char_specials so that mobs could
 * have access to them also, ie load them up from mob files and use
 * the skills.  Sure love corrupting the p-file! -Flynn */
#define GET_SKILL(ch, i)        ((ch)->char_specials.saved.skills[i][1] > 0 ? (ch)->char_specials.saved.skills[i][1] : (ch)->char_specials.saved.skills[i][0])
#define REAL_SKILL(ch, i)       ((ch)->char_specials.saved.skills[i][1] > 0 ? 0 : (ch)->char_specials.saved.skills[i][0])
// SET_SKILL is used only in medit.cpp for NPCs. Set char skills with utils.cpp's set_character_skill().
#define SET_SKILL(ch, i, pct)   {(ch)->char_specials.saved.skills[i][0] = pct; GET_SKILL_DIRTY_BIT((ch)) = TRUE;}

#define GET_POWER(ch, i)	((ch)->char_specials.saved.powers[i][1] ? \
                                 MIN((ch)->char_specials.saved.powers[i][1], (ch)->char_specials.saved.powers[i][0]) : 0)

#define GET_POWER_TOTAL(ch, i)	    ((ch)->char_specials.saved.powers[i][0] != 0 ? (ch)->char_specials.saved.powers[i][0] : 0)
#define SET_POWER_TOTAL(ch, i, amt)	{(ch)->char_specials.saved.powers[i][0] = amt; GET_ADEPT_POWER_DIRTY_BIT(ch) = TRUE;}

#define GET_POWER_ACT(ch, i)	((ch)->char_specials.saved.powers[i][1])
#define GET_POWER_POINTS(ch)    ((ch)->char_specials.saved.points)

#define GET_METAMAGIC(ch, i)    ((ch)->char_specials.saved.metamagic[i] != 0 ? (ch)->char_specials.saved.metamagic[i] : 0)
#define SET_METAMAGIC(ch, i, amt)    {(ch)->char_specials.saved.metamagic[i] = amt; GET_METAMAGIC_DIRTY_BIT(ch) = TRUE;}

#define GET_MASKING(ch)		((ch)->char_specials.saved.masking)
#define GET_CENTERINGSKILL(ch)	((ch)->char_specials.saved.centeringskill)
#define GET_PP(ch)		((ch)->char_specials.saved.powerpoints)
#define BOOST(ch)               ((ch)->char_specials.saved.boosted)
#define GET_EQ(ch, i)         ((ch)->equipment[i])

#define SKILL_IS_LANGUAGE(skill) (((skill) >= SKILL_ENGLISH && (skill) <= SKILL_FRENCH) || ((skill) >= SKILL_HEBREW && (skill) <= SKILL_IROQUOIS))

#define GET_SKILL_DIRTY_BIT(ch)         ((ch)->char_specials.dirty_bits[DIRTY_BIT_SKILLS])
#define GET_ADEPT_POWER_DIRTY_BIT(ch)   ((ch)->char_specials.dirty_bits[DIRTY_BIT_POWERS])
#define GET_SPELLS_DIRTY_BIT(ch)        ((ch)->char_specials.dirty_bits[DIRTY_BIT_SPELLS])
#define GET_METAMAGIC_DIRTY_BIT(ch)     ((ch)->char_specials.dirty_bits[DIRTY_BIT_METAMAGIC])
#define GET_ELEMENTALS_DIRTY_BIT(ch)    ((ch)->char_specials.dirty_bits[DIRTY_BIT_ELEMENTALS])
#define GET_MEMORY_DIRTY_BIT(ch)        ((ch)->char_specials.dirty_bits[DIRTY_BIT_MEMORY])
// #define GET_DRUG_DIRTY_BIT(ch)          ((ch)->char_specials.dirty_bits[DIRTY_BIT_DRUG])
#define GET_ALIAS_DIRTY_BIT(ch)         ((ch)->char_specials.dirty_bits[DIRTY_BIT_ALIAS])

#define GET_CONGREGATION_BONUS(ch) ((ch)->congregation_bonus_pool)

#define GET_MOB_RNUM(mob)       ((mob)->nr)
#define GET_MOB_VNUM(mob)       (IS_MOB(mob) ? mob_index[GET_MOB_RNUM(mob)].vnum : -1)
#define MOB_VNUM_RNUM(rnum) ((mob_index[rnum]).vnum)
#define GET_MOB_UNIQUE_ID(mob)  ((mob)->unique_id)

#define GET_MOB_SPEC(ch)       (IS_MOB(ch) ? (mob_index[GET_MOB_RNUM((ch))].func) : NULL)
#define GET_MOB_SPEC2(ch)      (IS_MOB(ch) ? (mob_index[GET_MOB_RNUM((ch))].sfunc) : NULL)
#define MOB_HAS_SPEC(ch, spec) (mob_index[GET_MOB_RNUM((ch))].func == spec || mob_index[GET_MOB_RNUM((ch))].sfunc == spec)

#define GET_MOB_WAIT(ch)      ((ch)->mob_specials.wait_state)
#define GET_DEFAULT_POS(ch)   ((ch)->mob_specials.default_pos)
#define GET_MOB_MEMORY(ch)    ((ch)->mob_specials.memory)
#define GET_LASTHIT(ch)	      ((ch)->mob_specials.lasthit)

#define GET_SPARE1(ch)  ((ch)->mob_specials.spare1)
#define GET_SPARE2(ch)  ((ch)->mob_specials.spare2)
#define GET_ACTIVE(ch)  ((ch)->mob_specials.active)
#define GET_EXTRA(ch)	((ch)->points.extra)
#define GET_MOBALERT(ch)	((ch)->mob_specials.alert)
#define GET_MOBALERTTIME(ch)	((ch)->mob_specials.alerttime)

#define CAN_CARRY_W(ch)       ((GET_STR(ch) * 10) + 30)
#define CAN_CARRY_N(ch)       (8 + GET_QUI(ch) + (GET_REAL_LEVEL(ch)>=LVL_BUILDER?50:0))
#define AWAKE(ch)             (GET_POS(ch) > POS_SLEEPING && GET_QUI(ch) > 0)
#define CAN_SEE_IN_DARK(ch)   ((IS_ASTRAL(ch) || IS_DUAL(ch) || \
    CURRENT_VISION(ch) == THERMOGRAPHIC || PRF_FLAGGED((ch), PRF_HOLYLIGHT)))
#define GET_BUILDING(ch)	((ch)->char_specials.programming)
#define IS_WORKING(ch)        ((AFF_FLAGS(ch).AreAnySet(AFF_PROGRAM, AFF_DESIGN, AFF_PART_BUILD, AFF_PART_DESIGN, AFF_PILOT, AFF_RIG, AFF_BONDING, AFF_CONJURE, AFF_PACKING, AFF_LODGE, AFF_CIRCLE, AFF_SPELLDESIGN, AFF_AMMOBUILD, ENDBIT)))
#define STOP_WORKING(ch)      {AFF_FLAGS((ch)).RemoveBits(AFF_PROGRAM, AFF_DESIGN, AFF_PART_BUILD, AFF_PART_DESIGN, AFF_BONDING, AFF_CONJURE, AFF_PACKING, AFF_LODGE, AFF_CIRCLE, AFF_SPELLDESIGN, AFF_AMMOBUILD, ENDBIT); \
                               GET_BUILDING((ch)) = NULL;}

#define GET_TOTEM(ch)                              (ch->player_specials->saved.totem)
#define GET_TOTEMSPIRIT(ch)                        (ch->player_specials->saved.totemspirit)

#define GET_DRUG_AFFECT(ch)                        (ch->player_specials->drug_affect[0])
#define GET_DRUG_DURATION(ch)                      (ch->player_specials->drug_affect[1])
#define GET_DRUG_DOSE(ch)                          (ch->player_specials->drug_affect[2])
#define GET_DRUG_STAGE(ch)                         (ch->player_specials->drug_affect[3])
#define GET_DRUG_EDGE(ch, i)                       (ch->player_specials->drugs[i][0])
#define GET_DRUG_ADDICT(ch, i)                     (ch->player_specials->drugs[i][1])
#define GET_DRUG_DOSES(ch, i)                      (ch->player_specials->drugs[i][2])
#define GET_DRUG_LASTFIX(ch, i)                    (ch->player_specials->drugs[i][3])
#define GET_DRUG_ADDTIME(ch, i)                    (ch->player_specials->drugs[i][4])
#define GET_DRUG_TOLERANT(ch, i)                   (ch->player_specials->drugs[i][5])
#define GET_DRUG_LASTWITH(ch, i)                   (ch->player_specials->drugs[i][6])
#define GET_MENTAL_LOSS(ch)                        (ch->player_specials->mental_loss)
#define GET_PHYSICAL_LOSS(ch)                      (ch->player_specials->physical_loss)
#define GET_PERM_BOD_LOSS(ch)                      (ch->player_specials->perm_bod)
#define GET_NUYEN_PAID_FOR_WHERES_MY_CAR(ch)       (ch->desc->nuyen_paid_for_wheres_my_car)
#define GET_NUYEN_INCOME_THIS_PLAY_SESSION(ch, i)  (ch->desc->nuyen_income_this_play_session[i])
/* descriptor-based utils ************************************************/

#define WAIT_STATE(ch, cycle) ({ \
        if ((ch)->desc) { \
          if (GET_LEVEL(ch) <= 1) { (ch)->desc->wait = (cycle); } else { send_to_char((ch), "^L(Skipping wait state of %d ticks: Staff.)^n\r\n", cycle); } \
        } else if (IS_NPC(ch)) { \
          GET_MOB_WAIT(ch) = (cycle); \
        }  \
      })

#define CHECK_WAIT(ch)        (((ch)->desc) ? ((ch)->desc->wait > 1) : 0)
#define STATE(d)                    ((d)->connected)

#define DESCRIPTOR_CONN_STATE_NOT_PLAYING(x) (((x)->connected >= CON_CLOSE && (x)->connected <= CON_MENU) || ((x)->connected >= CON_CHPWD_GETOLD && (x)->connected <= CON_QDELCONF2) || (x)->connected == CON_ASKNAME)

/* object utils **********************************************************/

/*
 * Check for NOWHERE or the top array index?
 * If using unsigned types, the top array index will catch everything.
 * If using signed types, NOTHING will catch the majority of bad accesses.
 */
#define VALID_OBJ_RNUM(obj) (GET_OBJ_RNUM(obj) <= top_of_objt && \
     GET_OBJ_RNUM(obj) != NOTHING)

#define GET_OBJ_AVAILTN(obj)    ((obj)->obj_flags.availtn)
#define GET_OBJ_AVAILDAY(obj)   ((obj)->obj_flags.availdays)
#define GET_LEGAL_NUM(obj)	((obj)->obj_flags.legality[0])
#define GET_LEGAL_CODE(obj)	((obj)->obj_flags.legality[1])
#define GET_LEGAL_PERMIT(obj)	((obj)->obj_flags.legality[2])
#define GET_OBJ_TYPE(obj)       ((obj)->obj_flags.type_flag)
#define GET_OBJ_COST(obj)       ((obj)->obj_flags.cost)
#define GET_OBJ_AFFECT(obj) ((obj)->obj_flags.bitvector)
#define GET_OBJ_EXTRA(obj)      ((obj)->obj_flags.extra_flags)
#define GET_OBJ_WEAR(obj)       ((obj)->obj_flags.wear_flags)
#define GET_OBJ_VAL(obj, val)   ((obj)->obj_flags.value[(val)])
#define GET_OBJ_WEIGHT(obj)     ((obj)->obj_flags.weight)
float get_proto_weight(struct obj_data *obj);
#define GET_OBJ_TIMER(obj)      ((obj)->obj_flags.timer)
#define GET_OBJ_ATTEMPT(obj)    ((obj)->obj_flags.attempt)
#define GET_OBJ_RNUM(obj)       ((obj)->item_number)
#define GET_OBJ_MATERIAL(obj)   ((obj)->obj_flags.material)
#define GET_OBJ_CONDITION(obj) ((obj)->obj_flags.condition)
#define GET_OBJ_BARRIER(obj)    ((obj)->obj_flags.barrier)
#define GET_OBJ_VNUM(obj) (VALID_OBJ_RNUM(obj) ? \
    obj_index[GET_OBJ_RNUM(obj)].vnum : NOTHING)
#define GET_OBJ_KEYWORDS(obj)   ((obj)->text.keywords)
#define IS_OBJ_STAT(obj, stat)  ((obj)->obj_flags.extra_flags.IsSet(stat))
#define OBJ_VNUM_RNUM(rnum) ((obj_index[rnum]).vnum)
#define VEH_VNUM_RNUM(rnum) ((veh_index[rnum]).vnum)
#define GET_MOD(veh, i) ((veh)->mod[i])
#define GET_OBJ_SPEC(obj) (VALID_OBJ_RNUM(obj) ? \
    obj_index[GET_OBJ_RNUM(obj)].func : NULL)
#define GET_WSPEC(obj) ((obj)->item_number >= 0 ? \
        (obj_index[GET_OBJ_RNUM(obj)].wfunc) : NULL)
#define GET_OBJ_STREET_INDEX(obj)   ((obj)->obj_flags.street_index)

#define CAN_WEAR(obj, part) ((obj)->obj_flags.wear_flags.IsSet((part)))

#define IS_WEAPON(type) (((type) >= TYPE_HIT) && ((type) < TYPE_SUFFERING))
#define IS_GUN(type) ((((type) >= WEAP_HOLDOUT) && ((type) < WEAP_GREN_LAUNCHER)) || (type) == WEAP_REVOLVER)
#define WEAPON_IS_GUN(weapon) ((weapon) && IS_GUN(GET_WEAPON_ATTACK_TYPE((weapon))) && (GET_WEAPON_SKILL((weapon)) >= SKILL_PISTOLS && GET_WEAPON_SKILL((weapon)) <= SKILL_ASSAULT_CANNON))

#define IS_MONOWHIP(obj) (GET_OBJ_RNUM((obj)) >= 0 && obj_index[GET_OBJ_RNUM((obj))].wfunc == monowhip)

// Give this a weapon type straight from the weapon-- ex WEAP_SMG. It will convert it for you.
#define GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon_type, ammo_type) ((ch)->bullet_pants[(weapon_type) - START_OF_AMMO_USING_WEAPONS][(ammo_type)])


/* compound utilities and other macros **********************************/

#define HSHR(ch)    (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "his":"her") : ((IS_NPC(ch) && MOB_FLAGGED(ch, MOB_INANIMATE)) ? "its": "their"))
#define HSSH(ch)    (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "he" :"she") : ((IS_NPC(ch) && MOB_FLAGGED(ch, MOB_INANIMATE)) ? "it" : "they" ))
#define HMHR(ch)    (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "him":"her") : ((IS_NPC(ch) && MOB_FLAGGED(ch, MOB_INANIMATE)) ? "it" : "them" ))
#define HASHAVE(ch) (GET_SEX(ch) != SEX_NEUTRAL ?              "has"     : ((IS_NPC(ch) && MOB_FLAGGED(ch, MOB_INANIMATE)) ? "has": "have" ))
#define ISARE(ch)   (GET_SEX(ch) != SEX_NEUTRAL ?              "is"      : ((IS_NPC(ch) && MOB_FLAGGED(ch, MOB_INANIMATE)) ? "is" : "are"  ))
#define HSSH_SHOULD_PLURAL(ch) (GET_SEX(ch) || (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_INANIMATE)))

#define ANA(obj) (strchr((const char *)"aeiouyAEIOUY", *(obj)->text.keywords) ? "An" : "A")
#define SANA(obj) (strchr((const char *)"aeiouyAEIOUY", *(obj)->text.keywords) ? "an" : "a")

/* Various macros building up to CAN_SEE */

#define HOLYLIGHT_OK(sub)      (GET_REAL_LEVEL(sub) >= LVL_BUILDER && \
   PRF_FLAGGED((sub), PRF_HOLYLIGHT))

bool LIGHT_OK_ROOM_SPECIFIED(struct char_data *sub, struct room_data *room);
#define LIGHT_OK(sub)          LIGHT_OK_ROOM_SPECIFIED(sub, get_ch_in_room(sub))
#define SELF(sub, obj)         ((sub) == (obj))

#define SEE_ASTRAL(sub, obj)   (!IS_ASTRAL(obj) || IS_ASTRAL(sub) || \
                                IS_DUAL(sub) || AFF_FLAGGED(obj, AFF_MANIFEST))

// Replaced these macros so we can actually get stack traces on failures.
bool CAN_SEE(struct char_data *subj, struct char_data *obj);
bool CAN_SEE_ROOM_SPECIFIED(struct char_data *subj, struct char_data *obj, struct room_data *room_specified);

#define CHAR_ONLY_SEES_VICT_WITH_ULTRASOUND(ch, vict) (ch != vict && (IS_AFFECTED((vict), AFF_IMP_INVIS) || IS_AFFECTED((vict), AFF_SPELLIMPINVIS)) && !(IS_DUAL((ch)) || IS_PROJECT((ch)) || IS_ASTRAL((ch))))

#define INVIS_OK_OBJ(sub, obj) (!IS_OBJ_STAT((obj), ITEM_EXTRA_INVISIBLE) || \
   IS_AFFECTED((sub), AFF_ULTRASOUND) || IS_ASTRAL(sub) || \
   IS_DUAL(sub) || HOLYLIGHT_OK(sub))

#define CAN_SEE_CARRIER(sub, obj) \
   ((!(obj)->carried_by || CAN_SEE((sub), (obj)->carried_by)) || (!(obj)->worn_by || CAN_SEE((sub), (obj)->worn_by)))

#define CAN_SEE_OBJ(sub, obj) (LIGHT_OK(sub) && INVIS_OK_OBJ((sub), (obj)) && CAN_SEE_CARRIER((sub), (obj)))

#define CAN_CARRY_OBJ(ch,obj)  \
   (((IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj)) <= CAN_CARRY_W(ch)) &&   \
    ((IS_CARRYING_N(ch) + 1) <= CAN_CARRY_N(ch)))

#define CAN_GET_OBJ(ch, obj)   \
   (CAN_WEAR((obj), ITEM_WEAR_TAKE) && CAN_CARRY_OBJ((ch),(obj)) && \
    CAN_SEE_OBJ((ch),(obj)))

#define PERS(ch, vict)   (CAN_SEE(vict, ch)? GET_NAME(ch) : "someone")
#define WPERS(ch, vict)  (CAN_SEE(vict, ch)? GET_CHAR_NAME(ch) : "someone")

#define PERS2(ch, vict)  ((GET_INVIS_LEV(ch) <= 0 \
      || access_level(vict, GET_INVIS_LEV(ch))) ? GET_NAME(ch) : "Someone" )

#define OBJS(obj, vict) (CAN_SEE_OBJ((vict), (obj)) ? \
        GET_OBJ_NAME((obj)) : "something")

#define OBJN(obj, vict) (CAN_SEE_OBJ((vict), (obj)) ? \
        fname((obj)->text.keywords) : "something")

// Accepts characters or vehicles.
#define EXIT(thing, door)  (thing->in_room ? thing->in_room->dir_option[door] : NULL)

#define LOCK_LEVEL(ch, obj, door) ((obj) ? GET_OBJ_VAL(obj, 4) : \
           get_ch_in_room(ch)->dir_option[door]->key_level)

#define CAN_GO(ch, door)     ( EXIT(ch,door) &&                                                                                 \
                               (EXIT(ch,door)->to_room && EXIT(ch,door)->to_room != &world[0]) &&                               \
                               !(IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED)) &&                                               \
                               !(IS_ASTRAL(ch) && IS_SET(EXIT(ch, door)->exit_info, EX_ASTRALLY_WARDED)) &&                     \
                               !(ROOM_FLAGGED(EXIT(ch, door)->to_room, ROOM_STAFF_ONLY) && GET_REAL_LEVEL(ch) < LVL_BUILDER) && \
                               (EXIT(ch, door)->to_room->staff_level_lock <= GET_REAL_LEVEL(ch))    \
                             )

#define OUTSIDE(ch)           (!ROOM_FLAGGED(get_ch_in_room(ch), ROOM_INDOORS))

/* Memory management *****************************************************/

// This guarantees that the pointer will be null after deletion (something that's plagued this codebase for years).
#define DELETE_AND_NULL_ARRAY(target) {delete [] (target); (target) = NULL;}
#define DELETE_AND_NULL(target) {delete (target); (target) = NULL;}

// Checks for existence and deletes the pointed value if extant.
#define DELETE_ARRAY_IF_EXTANT(target) {if ((target)) delete [] (target); (target) = NULL;}
#define DELETE_IF_EXTANT(target) {if ((target)) delete (target); (target) = NULL;}

// Getting tired of dealing with this special snowflake double pointer.
#define DELETE_D_STR_IF_EXTANT(d) {                                                    \
  if ((d)) {                                                                           \
    if ((d)->str) {                                                                    \
      DELETE_ARRAY_IF_EXTANT(*((d)->str));                                             \
    }                                                                                  \
    DELETE_IF_EXTANT((d)->str)                                                         \
  }                                                                                    \
}

#define INITIALIZE_NEW_D_STR(d) {                                                          \
  if ((d)) {                                                                           \
    (d)->str = new (char *);                                                           \
    if (!(d)->str) {                                                                   \
      mudlog("Malloc failed!", NULL, LOG_SYSLOG, TRUE);                                \
      shutdown();                                                                      \
    }                                                                                  \
    *((d)->str) = NULL;                                                                \
  }                                                                                    \
}

/* OS compatibility ******************************************************/


/* there could be some strange OS which doesn't have NULL... */
#ifndef NULL
#define NULL (void *)0
#endif

#if !defined(FALSE)
#define FALSE 0
#endif

#if !defined(TRUE)
#define TRUE  (!FALSE)
#endif

/* defines for fseek */
#ifndef SEEK_SET
#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2
#endif

/* Specific value invocations and macro defines for item types *****************************/

// ITEM_LIGHT convenience defines

// ITEM_WORKSHOP convenience defines
#define GET_WORKSHOP_TYPE(workshop)              (GET_OBJ_VAL((workshop), 0))
#define GET_WORKSHOP_GRADE(workshop)             (GET_OBJ_VAL((workshop), 1))
#define GET_WORKSHOP_IS_SETUP(workshop)          (GET_WORKSHOP_GRADE(workshop) == TYPE_WORKSHOP && GET_OBJ_VAL((workshop), 2))
#define GET_SETTABLE_WORKSHOP_IS_SETUP(workshop) (GET_OBJ_VAL((workshop), 2))
#define GET_WORKSHOP_AMMOKIT_TYPE(workshop)      (GET_OBJ_VAL((workshop), 2))
#define GET_WORKSHOP_UNPACK_TICKS(workshop)      (GET_OBJ_VAL((workshop), 3))

// ITEM_CAMERA convenience defines

// ITEM_PART convenience defines
#define GET_PART_TYPE(part)                    (GET_OBJ_VAL((part), 0))
#define GET_PART_RATING(part)                  (GET_OBJ_VAL((part), 1))
#define GET_PART_TARGET_MPCP(part)             (GET_OBJ_VAL((part), 2))
#define GET_PART_DESIGN_COMPLETION(part)       (GET_OBJ_VAL((part), 3)) //0 = done, -1 = required ?
#define GET_PART_BUILDER_IDNUM(part)           (GET_OBJ_VAL((part), 7))
#define GET_PART_PART_COST(part)               (GET_OBJ_VAL((part), 8))
#define GET_PART_CHIP_COST(part)               (GET_OBJ_VAL((part), 9))

// ITEM_WEAPON convenience defines
#define GET_WEAPON_POWER(weapon)               (GET_OBJ_VAL((weapon), 0))
#define GET_WEAPON_DAMAGE_CODE(weapon)         (GET_OBJ_VAL((weapon), 1))
#define GET_WEAPON_STR_BONUS(weapon)           (GET_OBJ_VAL((weapon), 2))
#define GET_WEAPON_ATTACK_TYPE(weapon)         (GET_OBJ_VAL((weapon), 3))
#define GET_WEAPON_SKILL(weapon)               (GET_OBJ_VAL((weapon), 4))
#define GET_WEAPON_MAX_AMMO(weapon)            (GET_OBJ_VAL((weapon), 5))
#define GET_WEAPON_REACH(weapon)               (GET_OBJ_VAL((weapon), 6))
#define GET_WEAPON_ATTACH_TOP_VNUM(weapon)     (GET_OBJ_VAL((weapon), 7))
#define GET_WEAPON_FOCUS_RATING(weapon)        (GET_OBJ_VAL((weapon), 7)) /* Only melee weapons can be weapon foci, so reusing attach is OK. */
#define GET_WEAPON_ATTACH_BARREL_VNUM(weapon)  (GET_OBJ_VAL((weapon), 8))
#define GET_WEAPON_FOCUS_BOND_STATUS(weapon)   (GET_OBJ_VAL((weapon), 8)) /* Only melee weapons can be weapon foci, so reusing attach is OK. */
#define GET_WEAPON_ATTACH_UNDER_VNUM(weapon)   (GET_OBJ_VAL((weapon), 9))
#define GET_WEAPON_FOCUS_BONDED_BY(weapon)     (GET_OBJ_VAL((weapon), 9)) /* Only melee weapons can be weapon foci, so reusing attach is OK. */
#define GET_WEAPON_POSSIBLE_FIREMODES(weapon)  (GET_OBJ_VAL((weapon), 10))
#define GET_WEAPON_FIREMODE(weapon)            (GET_OBJ_VAL((weapon), 11))
#define GET_WEAPON_INTEGRAL_RECOIL_COMP(weap)  (GET_OBJ_ATTEMPT((weap)))
#define GET_WEAPON_FULL_AUTO_COUNT(weapon)     (GET_OBJ_TIMER((weapon)))
#define GET_WEAPON_ATTACH_LOC(weapon, loc)     (((loc) >= ACCESS_LOCATION_TOP && (loc) <= ACCESS_LOCATION_UNDER) ? \
                                                    GET_OBJ_VAL((weapon), (loc)) : 0)

#define WEAPON_IS_FOCUS(obj)                   (GET_OBJ_TYPE((obj)) == ITEM_WEAPON && !IS_GUN(GET_WEAPON_ATTACK_TYPE((obj))) && GET_WEAPON_FOCUS_RATING((obj)) > 0)
bool WEAPON_FOCUS_USABLE_BY(struct obj_data *focus, struct char_data *ch);

#define WEAPON_CAN_USE_FIREMODE(weapon, mode)  (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(weapon), 1 << (mode)))
#define WEAPON_IS_SS(eq)  (GET_OBJ_TYPE(eq) == ITEM_WEAPON && GET_OBJ_VAL(eq, 11) == MODE_SS)
#define WEAPON_IS_SA(eq)    (GET_OBJ_TYPE(eq) == ITEM_WEAPON && GET_OBJ_VAL(eq, 11) == MODE_SA)
#define WEAPON_IS_BF(eq)   (GET_OBJ_TYPE(eq) == ITEM_WEAPON && GET_OBJ_VAL(eq, 11) == MODE_BF)
#define WEAPON_IS_FA(eq)    (GET_OBJ_TYPE(eq) == ITEM_WEAPON && GET_OBJ_VAL(eq, 11) == MODE_FA)

// ITEM_FIREWEAPON convenience defines

// ITEM_MISSILE convenience defines

// ITEM_CUSTOM_DECK convenience defines

// ITEM_GYRO convenience defines

// ITEM_DRUG convenience defines

// ITEM_WORN convenience defines
#define GET_WORN_POCKETS_HOLSTERS(worn)           (GET_OBJ_VAL((worn), 0))
#define GET_WORN_POCKETS_MISC(worn)               (GET_OBJ_VAL((worn), 4))
#define GET_WORN_BALLISTIC(worn)                  (GET_OBJ_VAL((worn), 5))
#define GET_WORN_IMPACT(worn)                     (GET_OBJ_VAL((worn), 6))
#define GET_WORN_CONCEAL_RATING(worn)             (GET_OBJ_VAL((worn), 7))
#define GET_WORN_MATCHED_SET(worn)                (GET_OBJ_VAL((worn), 8))

// ITEM_OTHER convenience defines
#define GET_VEHCONTAINER_VEH_VNUM(cont)           (GET_OBJ_VAL((cont), 1))
#define GET_VEHCONTAINER_VEH_IDNUM(cont)          (GET_OBJ_VAL((cont), 2))
#define GET_VEHCONTAINER_VEH_OWNER(cont)          (GET_OBJ_VAL((cont), 3))
#define GET_VEHCONTAINER_WEIGHT(cont)             (GET_OBJ_VAL((cont), 11))

// ITEM_MAGIC_TOOL convenience defines
#define GET_MAGIC_TOOL_TYPE(tool)                 (GET_OBJ_VAL((tool), 0))
#define GET_MAGIC_TOOL_RATING(tool)               (GET_OBJ_VAL((tool), 1))
#define GET_MAGIC_TOOL_TOTEM_OR_ELEMENT(tool)     (GET_OBJ_VAL((tool), 2))
#define GET_MAGIC_TOOL_OWNER(tool)                (GET_OBJ_VAL((tool), 3))
#define GET_MAGIC_TOOL_BUILD_TIME_LEFT(tool)      (GET_OBJ_VAL((tool), 9))

// ITEM_DOCWAGON convenience defines
#define GET_DOCWAGON_CONTRACT_GRADE(modulator)    (GET_OBJ_VAL((modulator), 0))
#define GET_DOCWAGON_BONDED_IDNUM(modulator)      (GET_OBJ_VAL((modulator), 1))

// ITEM_CONTAINER convenience defines

// ITEM_RADIO convenience defines

// ITEM_DRINKCON convenience defines

// ITEM_KEY convenience defines

// ITEM_FOOD convenience defines

// ITEM_MONEY convenience defines
#define GET_ITEM_MONEY_VALUE(money)               (GET_OBJ_VAL((money), 0))
#define GET_ITEM_MONEY_IS_CREDSTICK(money)        (GET_OBJ_VAL((money), 1))
#define GET_ITEM_MONEY_CREDSTICK_GRADE(money)     (GET_OBJ_VAL((money), 2))
#define GET_ITEM_MONEY_CREDSTICK_ACTIVATED(money) (GET_OBJ_VAL((money), 4))

// ITEM_PHONE convenience defines
#define GET_ITEM_PHONE_NUMBER_PART_ONE(phone)     (GET_OBJ_VAL((phone), 0))
#define GET_ITEM_PHONE_NUMBER_PART_TWO(phone)     (GET_OBJ_VAL((phone), 1))
#define GET_ITEM_PHONE_SWITCHED_ON(phone)         (GET_OBJ_VAL((phone), 2))
#define GET_ITEM_PHONE_RINGER_ON(phone)           (GET_OBJ_VAL((phone), 3))


// ITEM_BIOWARE convenience defines

#define GET_BIOWARE_TYPE(bioware)                 (GET_OBJ_VAL((bioware), 0))
#define GET_BIOWARE_RATING(bioware)               (GET_OBJ_VAL((bioware), 1))
#define GET_BIOWARE_IS_CULTURED(bioware)          (GET_OBJ_VAL((bioware), 2) || GET_BIOWARE_TYPE((bioware)) >= BIO_CEREBRALBOOSTER)
#define GET_SETTABLE_BIOWARE_IS_CULTURED(bioware) (GET_OBJ_VAL((bioware), 2))
#define GET_BIOWARE_IS_ACTIVATED(bioware)         (GET_OBJ_VAL((bioware), 3))
#define GET_BIOWARE_ESSENCE_COST(bioware)         (GET_OBJ_VAL((bioware), 4))

// ITEM_FOUNTAIN convenience defines

// ITEM_CYBERWARE convenience defines
#define GET_CYBERWARE_TYPE(cyberware)             (GET_OBJ_VAL((cyberware), 0))
#define GET_CYBERWARE_RATING(cyberware)           (GET_OBJ_VAL((cyberware), 1))
#define GET_CYBERWARE_GRADE(cyberware)            (GET_OBJ_VAL((cyberware), 2))
#define GET_CYBERWARE_FLAGS(cyberware)            (GET_OBJ_VAL((cyberware), 3)) // CYBERWEAPON_RETRACTABLE, CYBERWEAPON_IMPROVED
#define GET_CYBERWARE_LACING_TYPE(cyberware)      (GET_OBJ_VAL((cyberware), 3)) // Yes, this is also value 3. Great design here.
#define GET_CYBERWARE_ESSENCE_COST(cyberware)     (GET_OBJ_VAL((cyberware), 4))
#define GET_CYBERWARE_IS_DISABLED(cyberware)      (GET_OBJ_VAL((cyberware), 9))

// ITEM_CYBERDECK convenience defines
#define GET_CYBERDECK_MPCP(deck)                  (GET_OBJ_VAL((deck), 0))
#define GET_CYBERDECK_HARDENING(deck)             (GET_OBJ_VAL((deck), 1))
#define GET_CYBERDECK_ACTIVE_MEMORY(deck)         (GET_OBJ_VAL((deck), 2))
#define GET_CYBERDECK_TOTAL_STORAGE(deck)         (GET_OBJ_VAL((deck), 3))
#define GET_CYBERDECK_IO_RATING(deck)             (GET_OBJ_VAL((deck), 4))
#define GET_CYBERDECK_USED_STORAGE(deck)          (GET_OBJ_VAL((deck), 5))
#define GET_CYBERDECK_RESPONSE_INCREASE(deck)     (GET_OBJ_VAL((deck), 6))
#define GET_CYBERDECK_IS_INCOMPLETE(deck)         (GET_OBJ_VAL((deck), 9))
#define GET_CYBERDECK_FREE_STORAGE(deck)          (GET_CYBERDECK_TOTAL_STORAGE((deck)) -GET_CYBERDECK_USED_STORAGE((deck)))

// ITEM_PROGRAM convenience defines
#define GET_PROGRAM_TYPE(prog)                    (GET_OBJ_VAL((prog), 0))
#define GET_PROGRAM_RATING(prog)                  (GET_OBJ_VAL((prog), 1))
#define GET_PROGRAM_SIZE(prog)                    (GET_OBJ_VAL((prog), 2))
#define GET_PROGRAM_ATTACK_DAMAGE(prog)           (GET_OBJ_VAL((prog), 3))
#define GET_PROGRAM_IS_DEFAULTED(prog)            (GET_OBJ_VAL((prog), 4))

// ITEM_GUN_MAGAZINE convenience defines
#define GET_MAGAZINE_BONDED_MAXAMMO(magazine)     (GET_OBJ_VAL((magazine), 0))
#define GET_MAGAZINE_BONDED_ATTACKTYPE(magazine)  (GET_OBJ_VAL((magazine), 1))
#define GET_MAGAZINE_AMMO_TYPE(magazine)          (GET_OBJ_VAL((magazine), 2))
// #define GET_MAGAZINE_AMMO_MAX(magazine)        (GET_OBJ_VAL((magazine), 5))
#define GET_MAGAZINE_AMMO_COUNT(magazine)         (GET_OBJ_VAL((magazine), 9))

// ITEM_GUN_ACCESSORY convenience defines
#define GET_ACCESSORY_ATTACH_LOCATION(accessory)  (GET_OBJ_VAL((accessory), 0))
#define GET_ACCESSORY_TYPE(accessory)             (GET_OBJ_VAL((accessory), 1))
#define GET_ACCESSORY_RATING(accessory)           (GET_OBJ_VAL((accessory), 2))

// ITEM_SPELL_FORMULA convenience defines
#define GET_SPELLFORMULA_FORCE(formula)           (GET_OBJ_VAL((formula), 0))
#define GET_SPELLFORMULA_SPELL(formula)           (GET_OBJ_VAL((formula), 1))
#define GET_SPELLFORMULA_TRADITION(formula)       (GET_OBJ_VAL((formula), 2))
#define GET_SPELLFORMULA_SUBTYPE(formula)         (GET_OBJ_VAL((formula), 3))
#define GET_SPELLFORMULA_TIME_LEFT(formula)       (GET_OBJ_VAL((formula), 6))
#define GET_SPELLFORMULA_INITIAL_TIME(formula)    (GET_OBJ_VAL((formula), 7))
#define GET_SPELLFORMULA_CREATOR_IDNUM(formula)   (GET_OBJ_VAL((formula), 8))
#define GET_SPELLFORMULA_CATEGORY(formula)        (GET_OBJ_VAL((formula), 9))

#define SPELL_HAS_SUBTYPE(spell_number)           (spell_number == SPELL_INCATTR || spell_number == SPELL_INCCYATTR || spell_number == SPELL_DECATTR || spell_number == SPELL_DECCYATTR)

// ITEM_FOCUS convenience defines, search term GET_FOCI
#define GET_FOCUS_TYPE(focus)                     (GET_OBJ_VAL((focus), 0))
#define GET_FOCUS_FORCE(focus)                    (GET_OBJ_VAL((focus), 1))
#define GET_FOCUS_BONDED_TO(focus)                (GET_OBJ_VAL((focus), 2))
#define GET_FOCUS_BONDED_SPIRIT_OR_SPELL(focus)   (GET_OBJ_VAL((focus), 3))
#define GET_FOCUS_ACTIVATED(focus)                (GET_OBJ_VAL((focus), 4))
#define GET_FOCUS_TRADITION(focus)                (GET_OBJ_VAL((focus), 5))
#define GET_FOCUS_BOND_TIME_REMAINING(focus)      (GET_OBJ_VAL((focus), 9))

// ITEM_PATCH convenience defines

// ITEM_CLIMBING convenience defines

// ITEM_QUIVER convenience defines

// ITEM_DECK_ACCESSORY convenience defines
#define GET_DECK_ACCESSORY_TYPE(accessory)                  (GET_OBJ_VAL((accessory), 0))

// ITEM_DECK_ACCESSORY TYPE_PARTS convenience defines
#define GET_DECK_ACCESSORY_IS_CHIPS(accessory)              (GET_OBJ_VAL((accessory), 1))

// ITEM_DECK_ACCESSORY TYPE_FILE convenience defines
#define GET_DECK_ACCESSORY_FILE_CREATION_TIME(accessory)    (GET_OBJ_VAL((accessory), 1))
#define GET_DECK_ACCESSORY_FILE_SIZE(accessory)             (GET_OBJ_VAL((accessory), 2))
#define GET_DECK_ACCESSORY_FILE_HOST_VNUM(accessory)        (GET_OBJ_VAL((accessory), 3))
#define GET_DECK_ACCESSORY_FILE_HOST_COLOR(accessory)       (GET_OBJ_VAL((accessory), 4))
#define GET_DECK_ACCESSORY_FILE_PROTECTION(accessory)       (GET_OBJ_VAL((accessory), 5))
#define GET_DECK_ACCESSORY_FILE_RATING(accessory)           (GET_OBJ_VAL((accessory), 6))
#define GET_DECK_ACCESSORY_FILE_FOUND_BY(accessory)         (GET_OBJ_VAL((accessory), 7))
#define GET_DECK_ACCESSORY_FILE_WORKER_IDNUM(accessory)     (GET_OBJ_VAL((accessory), 8))
#define GET_DECK_ACCESSORY_FILE_REMAINING(accessory)        (GET_OBJ_VAL((accessory), 9))

// ITEM_DECK_ACCESSORY TYPE_COOKER convenience defines
#define GET_DECK_ACCESSORY_COOKER_RATING(accessory)         (GET_OBJ_VAL((accessory), 1))
#define GET_DECK_ACCESSORY_COOKER_ORIGINAL_TIME(accessory)  (GET_OBJ_VAL((accessory), 8))
#define GET_DECK_ACCESSORY_COOKER_TIME_REMAINING(accessory) (GET_OBJ_VAL((accessory), 9))

// ITEM_RCDECK convenience defines

// ITEM_CHIP convenience defines
#define GET_CHIP_SKILL(chip)                                (GET_OBJ_VAL((chip), 0))
#define GET_CHIP_RATING(chip)                               (GET_OBJ_VAL((chip), 1))
#define GET_CHIP_SIZE(chip)                                 (GET_OBJ_VAL((chip), 2))
#define GET_CHIP_COMPRESSION_FACTOR(chip)                   (GET_OBJ_VAL((chip), 8))
#define GET_CHIP_LINKED(chip)                               (GET_OBJ_VAL((chip), 9))

// ITEM_MOD convenience defines
#define GET_VEHICLE_MOD_TYPE(mod)                           (GET_OBJ_VAL((mod), 0))
#define GET_VEHICLE_MOD_MOUNT_TYPE(mod)                     (GET_OBJ_VAL((mod), 1))
#define GET_VEHICLE_MOD_LOAD_SPACE_REQUIRED(mod)            (GET_OBJ_VAL((mod), 1)) // Yes, this is also value 1.
#define GET_VEHICLE_MOD_RATING(mod)                         (GET_OBJ_VAL((mod), 2))
// what's 3?
#define GET_VEHICLE_MOD_DESIGNED_FOR_DRONE(mod)             (GET_OBJ_VAL((mod), 4)) // 0=veh, 1=drone, 2=both
#define GET_VEHICLE_MOD_ENGINE_BITS(mod)                    (GET_OBJ_VAL((mod), 5))
#define GET_VEHICLE_MOD_LOCATION(mod)                       (GET_OBJ_VAL((mod), 6))

// ITEM_HOLSTER convenience defines
#define GET_HOLSTER_TYPE(holster)                           (GET_OBJ_VAL((holster), 0))
#define GET_HOLSTER_READY_STATUS(holster)                   (GET_OBJ_VAL((holster), 3))

// ITEM_DESIGN convenience defines

// ITEM_GUN_AMMO convenience defines
#define GET_AMMOBOX_QUANTITY(box)                           (GET_OBJ_VAL((box), 0))
#define GET_AMMOBOX_WEAPON(box)                             (GET_OBJ_VAL((box), 1))
#define GET_AMMOBOX_TYPE(box)                               (GET_OBJ_VAL((box), 2))
#define GET_AMMOBOX_INTENDED_QUANTITY(box)                  (GET_OBJ_VAL((box), 3))
#define GET_AMMOBOX_TIME_TO_COMPLETION(box)                 (GET_OBJ_VAL((box), 4))
#define GET_AMMOBOX_CREATOR(box)                            (GET_OBJ_VAL((box), 9))

// ITEM_KEYRING convenience defines


/* Misc utils ************************************************************/
#define IS_DAMTYPE_PHYSICAL(type) \
  !((type) == TYPE_HIT || (type) == TYPE_BLUDGEON || (type) == TYPE_PUNCH || (type) == TYPE_TASER || (type) == TYPE_CRUSH || (type) == TYPE_POUND)

// If d->edit_convert_color_codes is true, doubles up ^ marks to not print color codes to the user in the specified string. Uses a static buf, so only one invocation per snprintf()!
#define DOUBLE_UP_COLOR_CODES_IF_NEEDED(str) (d ? (d->edit_convert_color_codes ? double_up_color_codes((str)) : (str)) : (str))

#define GET_CHAR_COLOR_HIGHLIGHT(ch) ((ch)->player.highlight_color_code ? (ch)->player.highlight_color_code : "^n")
#define SETTABLE_CHAR_COLOR_HIGHLIGHT(ch) ((ch)->player.highlight_color_code)

/*
 * Some systems such as Sun's don't have prototyping in their header files.
 * Thus, we try to compensate for them.
 *
 * Much of this is from Merc 2.2, used with permission.
 */

#if defined(_AIX)
char    *crypt(const char *key, const char *salt);
#endif

#if defined(apollo)
int     atoi (const char *string);
void    *calloc( unsigned nelem, size_t size);
char    *crypt( const char *key, const char *salt);
#endif

#if defined(hpux)
char    *crypt(char *key, const char *salt);
#endif

#if defined(linux)
extern "C" char *crypt(const char *key, const char *setting) throw ();
#endif

#if defined(freebsd)
extern "C" char *crypt(const char *key, const char *setting);
#endif

#if defined(MIPS_OS)
char    *crypt(const char *key, const char *salt);
#endif

#if defined(NeXT)
char    *crypt(const char *key, const char *salt);
int     unlink(const char *path);
int     getpid(void);
#endif

#if defined(WIN32)
extern "C" char    *crypt(const char *key, const char *salt);
#endif

#if defined(__CYGWIN__)
extern "C" char    *crypt(const char *key, const char *salt);
#endif



/*
 * The proto for [NeXT's] getpid() is defined in the man pages are returning
 * pid_t but the compiler pukes on it (cc). Since pid_t is just
 * normally a typedef for int, I just use int instead.
 * So far I have had no other problems but if I find more I will pass
 * them along...
 * -reni
 */

#if defined(sequent)
char    *crypt(const char *key, const char *salt);
int     fclose(FILE *stream);
int     fprintf(FILE *stream, const char *format, ... );
int     fread(void *ptr, int size, int n, FILE *stream);
int     fseek(FILE *stream, long offset, int ptrname);
void    perror(const char *s);
int     ungetc(int c, FILE *stream);
#endif

#if defined(sun)
#include <memory.h>
void    bzero(char *b, int length);
char    *crypt(const char *key, const char *salt);
int     fclose(FILE *stream);
int     fflush(FILE *stream);
void    rewind(FILE *stream);
int     sscanf(const char *s, const char *format, ... );
int     fprintf(FILE *stream, const char *format, ... );
int     fscanf(FILE *stream, const char *format, ... );
int     fseek(FILE *stream, long offset, int ptrname);
size_t  fread(void *ptr, size_t size, size_t n, FILE *stream);
size_t  fwrite(const void *ptr, size_t size, size_t n, FILE *stream);
void    perror(const char *s);
int     ungetc(int c, FILE *stream);
time_t  time(time_t *tloc);
int     system(const char *string);
#endif

#if defined(ultrix)
char    *crypt(const char *key, const char *salt);
#endif

#if defined(DGUX_TARGET) || (defined(WIN32) && !defined(__CYGWIN__))
#ifndef NOCRYPT
#include <crypt.h>
#endif
#define bzero(a, b) memset((a), 0, (b))
#endif

#if defined(sgi)
#include <bstring.h>
#ifndef NOCRYPT
#include <crypt.h>
#endif
#endif

/*
 * The crypt(3) function is not available on some operating systems.
 * In particular, the U.S. Government prohibits its export from the
 *   United States to foreign countries.
 * Turn on NOCRYPT to keep passwords in plain text.
 */
#ifdef NOCRYPT
#define CRYPT(a,b) (a)
#else
#define CRYPT(a,b) ((char *) crypt((a),(b)))
#endif

// A simple error handling define to cut out some of the boilerplate.
#define FAILURE_CASE(condition, message) { \
  if ((condition)) {                       \
    send_to_char(ch, "%s\r\n", (message)); \
    return;                                \
  }                                        \
}                                          \

#define FOR_ITEMS_AROUND_CH(ch, item_ptr) for ((item_ptr) = (ch)->in_room ? (ch)->in_room->contents : (ch)->in_veh->contents; (item_ptr); (item_ptr) = (item_ptr)->next_content)

// Nuyen tracking functions.
void gain_nuyen(struct char_data *ch, long amount, int category);
void lose_nuyen(struct char_data *ch, long amount, int category);
void gain_bank(struct char_data *ch, long amount, int category);
void lose_bank(struct char_data *ch, long amount, int category);
void gain_nuyen_on_credstick(struct char_data *ch, struct obj_data *credstick, long amount, int category);
void lose_nuyen_from_credstick(struct char_data *ch, struct obj_data *credstick, long amount, int category);

#define GET_WOUND_NAME(damage_level) (wound_name[MIN(DEADLY, MAX(0, damage_level))])

#endif
