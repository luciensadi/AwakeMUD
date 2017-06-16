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
bool     has_cyberweapon(struct char_data *ch);
char    *str_dup(const char *source);
char *str_str( const char *str1, const char *str2 );
int     str_cmp(const char *arg1, const char *arg2);
int strn_cmp(const char *arg1, const char *arg2, int n);
void    log(const char *str);
void    log_vfprintf(const char *format, ...);
void    mudlog(const char *str, struct char_data *ch, int log, bool file);
void    log_death_trap(struct char_data *ch);
int     number(int from, int to);
int     dice(int number, int size);
void    sprintbit(long vektor, const char *names[], char *result);
void    sprinttype(int type, const char *names[], char *result);
void sprint_obj_mods(struct obj_data *obj, char *result);
int     get_line(FILE *fl, char *buf);
struct time_info_data age(struct char_data *ch);
int     convert_damage(int damage);
int     srdice(void);
int     success_test(int number, int target);
int     resisted_test(int num4ch, int tar4ch, int num4vict, int tar4vict);
int     stage(int successes, int wound);
bool    access_level(struct char_data *ch, int level);
char * buf_mod(char *buf, const char *name, int bonus);
char * buf_roll(char *buf, const char *name, int bonus);
int modify_target_rbuf_raw(struct char_data *ch, char *rbuf, int current_visibility_penalty);
int modify_target_rbuf(struct char_data *ch, char *rbuf);
int modify_target(struct char_data *ch);
int damage_modifier(struct char_data *ch, char *rbuf);
char * capitalize(const char *source);
int get_speed(struct veh_data *veh);
int negotiate(struct char_data *ch, struct char_data *tch, int comp, int basevalue, int mod, bool buy);
float gen_size(int race, bool height, int size, int sex);
int get_skill(struct char_data *ch, int skill, int &target);
void add_follower(struct char_data *ch, struct char_data *leader);
int light_level(rnum_t room);
bool biocyber_compatibility(struct obj_data *obj1, struct obj_data *obj2, struct char_data *ch);
void magic_loss(struct char_data *ch, int magic, bool msg);

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
int MAX(int a, int b);
int MIN(int a, int b);
#endif

#define RM_BLOOD(rm) (world[rm].blood)
/* Ok im done here -- root */

/* in magic.c */
bool    circle_follow(struct char_data *ch, struct char_data * victim);

/* in act.informative.c */
void    look_at_room(struct char_data *ch, int mode);

/* in act.movmement.c */
int     do_simple_move(struct char_data *ch, int dir, int extra, struct
                       char_data *vict);
int perform_move(struct char_data *ch, int dir, int extra, struct char_data
                 *vict);

void    mental_gain(struct char_data *ch);
void    physical_gain(struct char_data *ch);
void    advance_level(struct char_data *ch);
void    set_title(struct char_data *ch, const char *title);
void    set_pretitle(struct char_data *ch, const char *title);
void    set_whotitle(struct char_data *ch, const char *title);
int     gain_exp(struct char_data *ch, int gain, bool rep);
void    gain_exp_regardless(struct char_data *ch, int gain);
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
  { perror("SYSERR: malloc failure"); exit(1); } } while(0)

/*
 * the source previously used the same code in many places to remove an item
 * from a list: if it's the list head, change the head, else traverse the
 * list looking for the item before the one to be removed.  Now, we have a
 * macro to do this.  To use, just make sure that there is a variable 'temp'
 * declared as the same type as the list to be manipulated.  BTW, this is
 * a great application for C++ templates but, alas, this is not C++.  Maybe
 * CircleMUD 4.0 will be...
 */
#define REMOVE_FROM_LIST(item, head, next)      \
   if ((item) == (head))                \
      head = (item)->next;              \
   else {                               \
      temp = head;                      \
      while (temp && (temp->next != (item))) \
         temp = temp->next;             \
      if (temp)                         \
         temp->next = (item)->next;     \
   }                                    \


/* basic bitvector utils *************************************************/

#define EXIT2(roomnum, door) (world[(roomnum)].dir_option[(door)])
#define CAN_GO2(roomnum, door) (EXIT2(roomnum, door) && \
        (EXIT2(roomnum, door)->to_room != NOWHERE) && \
        !IS_SET(EXIT2(roomnum,door)->exit_info, EX_CLOSED))

#define VEH_FLAGS(ch) ((ch).flags)
#define MOB_FLAGS(ch) ((ch)->char_specials.saved.act)
#define PLR_FLAGS(ch) ((ch)->char_specials.saved.act)
#define PRF_FLAGS(ch) ((ch)->player_specials->saved.pref)
#define AFF_FLAGS(ch) ((ch)->char_specials.saved.affected_by)
#define ROOM_FLAGS(loc) (world[(loc)].room_flags)

#define IS_NPC(ch)  (MOB_FLAGS(ch).IsSet(MOB_ISNPC))
#define IS_MOB(ch)  (IS_NPC(ch) && ((ch)->nr >-1))
#define IS_PROJECT(ch) (IS_NPC(ch) && ch->desc && ch->desc->original && \
  PLR_FLAGS(ch->desc->original).IsSet(PLR_PROJECT) && \
  GET_MOB_VNUM(ch) == 22)
#define IS_SPIRIT(ch) (IS_NPC(ch) && GET_RACE(ch) == RACE_SPIRIT)
#define IS_ELEMENTAL(ch) (IS_NPC(ch) && GET_RACE(ch) == RACE_ELEMENTAL)
#define IS_ASTRAL(ch) (IS_NPC(ch) && MOB_FLAGS(ch).IsSet(MOB_ASTRAL))
#define IS_DUAL(ch)   ((IS_NPC(ch) && MOB_FLAGS(ch).IsSet(MOB_DUAL_NATURE)) || \
               (!IS_NPC(ch) && (PLR_FLAGS(ch).IsSet(PLR_PERCEIVE) || access_level(ch, LVL_ADMIN))))
#define IS_SENATOR(ch) (GET_LEVEL(ch) >= LVL_BUILDER)

// ONLY for use on non-Bitfield bitvectors:
#define IS_SET(set, flag)     ((set) & (flag))
#define SET_BIT(set, flag)    ((set) |= (flag))
#define REMOVE_BIT(set, flag) ((set) &= ~(flag))
#define TOGGLE_BIT(set, flag) ((set) ^= (flag))

#define MOB_FLAGGED(ch, flag) (IS_NPC(ch) && MOB_FLAGS(ch).IsSet((flag)))
#define PLR_FLAGGED(ch, flag) (!IS_NPC(ch) && PLR_FLAGS(ch).IsSet((flag)))
#define AFF_FLAGGED(ch, flag) (AFF_FLAGS(ch).IsSet((flag)))
#define VEH_FLAGGED(ch, flag) (VEH_FLAGS(ch).IsSet((flag))
#define PRF_FLAGGED(ch, flag) \
  ((ch->desc && ch->desc->original) \
   ? PRF_FLAGS(ch->desc->original).IsSet((flag)) \
   : PRF_FLAGS(ch).IsSet((flag)))
#define ROOM_FLAGGED(loc, flag) (ROOM_FLAGS(loc).IsSet((flag)))

/* IS_AFFECTED for backwards compatibility */
#define IS_AFFECTED(ch, skill) (AFF_FLAGGED((ch), (skill)))

extern bool PRF_TOG_CHK(char_data *ch, dword offset);
extern bool PLR_TOG_CHK(char_data *ch, dword offset);

/* room utils ************************************************************/


#define SECT(room) (VALID_ROOM_RNUM(room) ? \
    world[(room)].sector_type : SPIRIT_HEARTH)
#define IS_WATER(room) (SECT((room)) == SPIRIT_LAKE || SECT((room)) == SPIRIT_RIVER || SECT((room)) == SPIRIT_SEA)

#define IS_DARK(room)  (light_level((room)) == ITEM_FULLDARK)
#define IS_LIGHT(room)  (light_level((room)) <= LIGHT_NORMALNOLIT || light_level((room)) == LIGHT_PARTLIGHT)
#define IS_LOW(room)	(light_level((room)) == LIGHT_MINLIGHT || light_level((room)) == LIGHT_PARTLIGHT)


#define VALID_ROOM_RNUM(rnum) ((rnum) != NOWHERE && (rnum) <= top_of_world)

#define GET_ROOM_SPEC(room) \
 (VALID_ROOM_RNUM(room) ? world[(room)].func : NULL)

#define GET_ROOM_VNUM(rnum) \
 ((vnum_t)(VALID_ROOM_RNUM(rnum) ? world[(rnum)].number : NOWHERE))

/* char utils ************************************************************/


#define IN_ROOM(ch)     ((ch)->in_room)
#define GET_WAS_IN(ch)  ((ch)->was_in_room)

#define GET_VEH_NAME(veh) ((veh)->restring ? (veh)->restring : (veh)->short_description)
#define GET_VEH_DESC(veh) ((veh)->restring_long ? (veh)->restring_long : (veh)->long_description)
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

/*
 * I wonder if this definition of GET_REAL_LEVEL should be the definition
 * of GET_LEVEL?  JE
 */
#define GET_REAL_LEVEL(ch) \
   (ch->desc && ch->desc->original ? GET_LEVEL(ch->desc->original) : \
    GET_LEVEL(ch))

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
#define GET_ESSHOLE(ch)		((ch)->aff_abils.esshole)
#define GET_INDEX(ch)         ((ch)->real_abils.bod_index)
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
#define GET_TOTALBAL(ch)      ((ch)->points.ballistic[1])
#define GET_BALLISTIC(ch)     ((ch)->points.ballistic[0])
#define GET_TOTALIMP(ch)      ((ch)->points.impact[1])
#define GET_IMPACT(ch)          ((ch)->points.impact[0])
#define GET_PHYSICAL(ch)        ((ch)->points.physical)
#define GET_MAX_PHYSICAL(ch)  ((ch)->points.max_physical)
#define GET_GRADE(ch)   	((ch)->points.grade)
#define GET_MENTAL(ch)          ((ch)->points.mental)
#define GET_MAX_MENTAL(ch)      ((ch)->points.max_mental)
#define GET_NUYEN(ch)           ((ch)->points.nuyen)
#define GET_BANK(ch)            ((ch)->points.bank)
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
#define IS_CARRYING_W(ch)       ((ch)->char_specials.carry_weight)
#define IS_CARRYING_N(ch)       ((ch)->char_specials.carry_items)
#define FIGHTING(ch)            ((ch)->char_specials.fighting)
#define FIGHTING_VEH(ch)        ((ch)->char_specials.fight_veh)
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
#define GET_LAST_IN(ch)  ((ch)->player_specials->saved.last_in)
#define GET_PRACTICES(ch)       ((ch)->player_specials->saved.spells_to_learn)
#define GET_INVIS_LEV(ch)       ((ch)->player_specials->saved.invis_level)
#define GET_INCOG_LEV(ch)       ((ch)->player_specials->saved.incog_level)
#define GET_WIMP_LEV(ch)        ((ch)->player_specials->saved.wimp_level)
#define GET_FREEZE_LEV(ch)      ((ch)->player_specials->saved.freeze_level)
#define GET_BAD_PWS(ch)         ((ch)->player_specials->saved.bad_pws)
#define GET_WATCH(ch)		((ch)->player_specials->watching)
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
#define GET_CASTING(ch)		((ch)->aff_abils.casting_pool)
#define GET_DRAIN(ch)		((ch)->aff_abils.drain_pool)
#define GET_SDEFENSE(ch)	((ch)->aff_abils.spell_defense_pool)
#define GET_REFLECT(ch)		((ch)->aff_abils.reflection_pool)
#define GET_SPIRIT(ch)          ((ch)->char_specials.spirits)
#define GET_NUM_SPIRITS(ch)     ((ch)->char_specials.num_spirits)
#define GET_DOMAIN(ch)		((ch)->points.domain)
#define GET_ATT_POINTS(ch)	((ch)->player_specials->saved.att_points)
#define GET_SKILL_POINTS(ch)	((ch)->player_specials->saved.skill_points)
#define GET_FORCE_POINTS(ch)	((ch)->player_specials->saved.force_points)
#define GET_RESTRING_POINTS(ch)	((ch)->player_specials->saved.restring_points)
#define GET_TARGET_MOD(ch)	((ch)->char_specials.target_mod)
#define LAST_HEAL(ch)		((ch)->char_specials.last_healed)
#define GET_FOCI(ch)		((ch)->char_specials.foci)
#define GET_QUEST(ch)		((ch)->desc && (ch)->desc->original ? (ch)->desc->original->player_specials->questnum : \
                                                                      (ch)->player_specials->questnum)
#define GET_LQUEST(ch, i)	((ch)->player_specials->last_quest[i])
#define POOFIN(ch)              ((ch)->player.poofin)
#define POOFOUT(ch)             ((ch)->player.poofout)
#define GET_PROMPT(ch)		((PLR_FLAGGED((ch), PLR_MATRIX) ? (ch)->player.matrixprompt : (ch)->player.prompt))
#define GET_ALIASES(ch)		((ch)->desc && (ch)->desc->original ? (ch)->desc->original->player_specials->aliases : (ch)->player_specials->aliases)
#define GET_MEMORY(ch)		((ch)->player_specials->remem)
#define GET_IGNORE(ch)		((ch)->player_specials->ignored)
#define GET_LAST_TELL(ch)	((ch)->player_specials->last_tell)
#define GET_LAST_DAMAGETIME(ch)	((ch)->points.lastdamage)
#define HOURS_LEFT_TRACK(ch)	((ch)->points.track[0])
#define HOURS_SINCE_TRACK(ch)	((ch)->points.track[1])
#define NATURAL_VISION(ch)	((ch)->points.vision[0])
#define CURRENT_VISION(ch)	((ch)->points.vision[1])
#define SHOTS_FIRED(ch)		((ch)->points.extras[0])
#define SHOTS_TRIGGERED(ch)	((ch)->points.extras[1])
#define RIG_VEH(ch, veh)	(veh) = (ch)->char_specials.rigging ? (ch)->char_specials.rigging : (ch)->in_veh;


/* the skills structure was moved to char_specials so that mobs could
 * have access to them also, ie load them up from mob files and use
 * the skills.  Sure love corrupting the p-file! -Flynn */
#define GET_SKILL(ch, i)        ((ch)->char_specials.saved.skills[i][1] > 0 ? (ch)->char_specials.saved.skills[i][1] : (ch)->char_specials.saved.skills[i][0])
#define REAL_SKILL(ch, i)       ((ch)->char_specials.saved.skills[i][1] > 0 ? 0 : (ch)->char_specials.saved.skills[i][0])
#define SET_SKILL(ch, i, pct)   {(ch)->char_specials.saved.skills[i][0] = pct;}
#define GET_POWER(ch, i)	((ch)->char_specials.saved.powers[i][1] ? \
                                 MIN((ch)->char_specials.saved.powers[i][1], (ch)->char_specials.saved.powers[i][0]) : 0)
#define GET_POWER_TOTAL(ch, i)	((ch)->char_specials.saved.powers[i][0])
#define GET_POWER_ACT(ch, i)	((ch)->char_specials.saved.powers[i][1])
#define GET_POWER_POINTS(ch)    ((ch)->char_specials.saved.points)
#define GET_METAMAGIC(ch, i)    ((ch)->char_specials.saved.metamagic[i])
#define GET_MASKING(ch)		((ch)->char_specials.saved.masking)
#define GET_CENTERINGSKILL(ch)	((ch)->char_specials.saved.centeringskill)
#define GET_PP(ch)		((ch)->char_specials.saved.powerpoints)
#define BOOST(ch)               ((ch)->char_specials.saved.boosted)
#define GET_EQ(ch, i)         ((ch)->equipment[i])

#define GET_MOB_SPEC(ch)      (IS_MOB(ch) ? (mob_index[(ch->nr)].func) : NULL)
#define GET_MOB_RNUM(mob)       ((mob)->nr)
#define GET_MOB_VNUM(mob)       (IS_MOB(mob) ? mob_index[GET_MOB_RNUM(mob)].vnum : -1)
#define MOB_VNUM_RNUM(rnum) ((mob_index[rnum]).vnum)

#define GET_MOB_WAIT(ch)      ((ch)->mob_specials.wait_state)
#define GET_DEFAULT_POS(ch)   ((ch)->mob_specials.default_pos)
#define MEMORY(ch)            ((ch)->mob_specials.memory)
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

#define GET_TOTEM(ch)           (ch->player_specials->saved.totem)
#define GET_TOTEMSPIRIT(ch)	(ch->player_specials->saved.totemspirit)

#define GET_DRUG_AFFECT(ch)     (ch->player_specials->drug_affect[0])
#define GET_DRUG_DURATION(ch)   (ch->player_specials->drug_affect[1])
#define GET_DRUG_DOSE(ch)       (ch->player_specials->drug_affect[2])
#define GET_DRUG_STAGE(ch)      (ch->player_specials->drug_affect[3])
#define GET_DRUG_EDGE(ch, i)    (ch->player_specials->drugs[i][0])
#define GET_DRUG_ADDICT(ch, i)  (ch->player_specials->drugs[i][1])
#define GET_DRUG_DOSES(ch, i)   (ch->player_specials->drugs[i][2])
#define GET_DRUG_LASTFIX(ch, i) (ch->player_specials->drugs[i][3])
#define GET_DRUG_ADDTIME(ch, i) (ch->player_specials->drugs[i][4])
#define GET_DRUG_TOLERANT(ch, i) (ch->player_specials->drugs[i][5])
#define GET_DRUG_LASTWITH(ch, i) (ch->player_specials->drugs[i][6])
#define GET_MENTAL_LOSS(ch)	(ch->player_specials->mental_loss)
#define GET_PHYSICAL_LOSS(ch)	(ch->player_specials->physical_loss)
#define GET_PERM_BOD_LOSS(ch)	(ch->player_specials->perm_bod)
/* descriptor-based utils ************************************************/

#define WAIT_STATE(ch, cycle) { \
        if ((ch)->desc) (ch)->desc->wait = (cycle); \
        else if (IS_NPC(ch)) GET_MOB_WAIT(ch) = (cycle); }

#define CHECK_WAIT(ch)        (((ch)->desc) ? ((ch)->desc->wait > 1) : 0)
#define STATE(d)                    ((d)->connected)

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
#define GET_OBJ_TIMER(obj)      ((obj)->obj_flags.timer)
#define GET_OBJ_ATTEMPT(obj)    ((obj)->obj_flags.attempt)
#define GET_OBJ_RNUM(obj)       ((obj)->item_number)
#define GET_OBJ_MATERIAL(obj)   ((obj)->obj_flags.material)
#define GET_OBJ_CONDITION(obj) ((obj)->obj_flags.condition)
#define GET_OBJ_BARRIER(obj)    ((obj)->obj_flags.barrier)
#define GET_OBJ_VNUM(obj) (VALID_OBJ_RNUM(obj) ? \
    obj_index[GET_OBJ_RNUM(obj)].vnum : NOTHING)
#define IS_OBJ_STAT(obj, stat)  ((obj)->obj_flags.extra_flags.IsSet(stat))
#define OBJ_VNUM_RNUM(rnum) ((obj_index[rnum]).vnum)
#define VEH_VNUM_RNUM(rnum) ((veh_index[rnum]).vnum)
#define GET_MOD(veh, i) ((veh)->mod[i])
#define GET_OBJ_SPEC(obj) (VALID_OBJ_RNUM(obj) ? \
    obj_index[GET_OBJ_RNUM(obj)].func : NULL)
#define GET_WSPEC(obj) ((obj)->item_number >= 0 ? \
        (obj_index[GET_OBJ_RNUM(obj)].wfunc) : NULL)

#define CAN_WEAR(obj, part) ((obj)->obj_flags.wear_flags.IsSet((part)))

#define IS_WEAPON(type) ((((type) >= TYPE_HIT) && ((type) < TYPE_SUFFERING)) || (type) == TYPE_TASER)
#define IS_GUN(type) (((type) >= WEAP_HOLDOUT) && ((type) < MAX_WEAP) && (type) != WEAP_TASER)


/* compound utilities and other macros **********************************/

#define HSHR(ch) (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "his":"her") :"its")
#define HSSH(ch) (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "he" :"she") : "it")
#define HMHR(ch) (GET_SEX(ch) ? (GET_SEX(ch)==SEX_MALE ? "him":"her") : "it")

#define ANA(obj) (strchr((const char *)"aeiouyAEIOUY", *(obj)->text.keywords) ? "An" : "A")
#define SANA(obj) (strchr((const char *)"aeiouyAEIOUY", *(obj)->text.keywords) ? "an" : "a")

/* Various macros building up to CAN_SEE */

#define HOLYLIGHT_OK(sub)      (GET_REAL_LEVEL(sub) >= LVL_BUILDER && \
   PRF_FLAGGED((sub), PRF_HOLYLIGHT))

#define LIGHT_OK(sub)          ((IS_ASTRAL(sub) || IS_DUAL(sub) || \
    CURRENT_VISION((sub)) == THERMOGRAPHIC || HOLYLIGHT_OK(sub) || \
    IS_LIGHT((sub)->in_room) || !((light_level((sub)->in_room) == LIGHT_MINLIGHT || light_level((sub)->in_room) == LIGHT_FULLDARK) && CURRENT_VISION((sub)) == NORMAL)))


#define INVIS_OK(sub, obj) (IS_SENATOR(sub) || IS_ASTRAL(sub) || IS_DUAL(sub) || \
  (!(world[sub->in_room].silence[0] > 0 || affected_by_spell(obj, SPELL_STEALTH)) && AFF_FLAGGED(sub, AFF_DETECT_INVIS)) || \
(!(IS_AFFECTED(obj, AFF_INVISIBLE) && CURRENT_VISION(sub) != THERMOGRAPHIC) && \
 !(IS_AFFECTED(obj, AFF_SPELLINVIS) && !(AFF_FLAGGED(sub, AFF_RIG) || PLR_FLAGGED(sub, PLR_REMOTE))) && \
 !(IS_AFFECTED(obj, AFF_IMP_INVIS) || IS_AFFECTED(obj, AFF_SPELLIMPINVIS))))


#define SELF(sub, obj)         ((sub) == (obj))

#define SEE_ASTRAL(sub, obj)   (!IS_ASTRAL(obj) || IS_ASTRAL(sub) || \
                                IS_DUAL(sub) || AFF_FLAGGED(obj, AFF_MANIFEST))

/* Can subject see character "obj"? */
#define CAN_SEE(sub, obj)      (!(MOB_FLAGGED(obj, MOB_TOTALINVIS) && GET_LEVEL(sub) < LVL_BUILDER) && (SELF((sub), (obj)) || \
   (SEE_ASTRAL((sub), (obj)) && LIGHT_OK(sub) && INVIS_OK((sub), (obj)) && \
    (GET_INVIS_LEV(obj) <= 0 || access_level(sub, GET_INVIS_LEV(obj)) \
     || access_level(sub, LVL_VICEPRES)))))

/* End of CAN_SEE */

#define INVIS_OK_OBJ(sub, obj) (!IS_OBJ_STAT((obj), ITEM_INVISIBLE) || \
   IS_AFFECTED((sub), AFF_DETECT_INVIS) || IS_ASTRAL(sub) || \
   IS_DUAL(sub) || HOLYLIGHT_OK(sub))

#define CAN_SEE_CARRIER(sub, obj) \
   ((!(obj)->carried_by || CAN_SEE((sub), (obj)->carried_by)) && \
    (!(obj)->worn_by || CAN_SEE((sub), (obj)->worn_by)))

#define CAN_SEE_OBJ(sub, obj) (LIGHT_OK(sub) && \
   INVIS_OK_OBJ((sub), (obj)) && CAN_SEE_CARRIER((sub), (obj)))

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

#define EXIT(ch, door)  (world[(ch)->in_room].dir_option[door])

#define LOCK_LEVEL(ch, obj, door) ((obj) ? GET_OBJ_VAL(obj, 4) : \
           world[(ch)->in_room].dir_option[door]->key_level)

#define CAN_GO(ch, door)      (EXIT(ch,door) && \
                              (EXIT(ch,door)->to_room != NOWHERE) && \
                              !IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED))

#define OUTSIDE(ch)           (!ROOM_FLAGGED(((ch)->in_veh ? (ch)->in_veh->in_room : (ch)->in_room), ROOM_INDOORS))

/* Memory management *****************************************************/

// This guarantees that the pointer will be null after deletion (something that's plagued this codebase for years).
#define DELETE_AND_NULL_ARRAY(target) {delete [] (target); (target) = NULL;}
#define DELETE_AND_NULL(target) {delete (target); (target) = NULL;}

// Checks for existence and deletes the pointed value if extant.
#define DELETE_ARRAY_IF_EXTANT(target) {if ((target)) DELETE_AND_NULL_ARRAY((target))}
#define DELETE_IF_EXTANT(target) {if ((target)) DELETE_AND_NULL((target))}

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

#endif




