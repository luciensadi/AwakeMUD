/**************************************************************************
*  File: dg_scripts.cpp                                                   *
*  Usage: The script driver, the entity lookup table, and the staff-side  *
*         attach/detach/tstat/vdelete commands.                           *
*                                                                         *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
*                                                                         *
*  $Author: Mark A. Heilpern/egreen/Welcor $                              *
*  $Date: 2004/10/11 12:07:00$                                            *
*  $Revision: 1.0.14 $                                                    *
*                                                                         *
*  Ported to AwakeMUD CE by Fizban.                                       *
*                                                                         *
*  Triggers do not attach to players. Everywhere stock DG consults its    *
*  script_players config option, this port simply refuses.                *
**************************************************************************/

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "dblist.hpp"
#include "screen.hpp"
#include "constants.hpp"
#include "olc.hpp"
#include "newdb.hpp"
#include "dg_scripts.hpp"
#include "dg_event.hpp"

extern class objList ObjList;
extern bool zone_is_empty(int zone_nr);
extern struct time_info_data time_info;

/* The trigger index and the world-wide list of live triggers. */
struct trig_index_data **trig_index = NULL;
struct trig_data *trigger_list = NULL;
rnum_t top_of_trigt = 0;

/* Set when a %purge% destroys the very thing whose script is running. */
int dg_owner_purged = 0;

/* Script ids are handed out lazily, from these counters. */
static long max_mob_id = MOB_ID_BASE;
static long max_obj_id = OBJ_ID_BASE;

/* local functions */
static void eval_op(const char *op, char *lhs, char *rhs, char *result, void *go,
                    struct script_data *sc, struct trig_data *trig);
static void eval_expr(char *line, char *result, void *go, struct script_data *sc,
                      struct trig_data *trig, int type);
static int eval_lhs_op_rhs(char *expr, char *result, void *go, struct script_data *sc,
                           struct trig_data *trig, int type);
static int process_if(char *cond, void *go, struct script_data *sc,
                      struct trig_data *trig, int type);
static struct cmdlist_element *find_else_end(struct trig_data *trig,
    struct cmdlist_element *cl, void *go, struct script_data *sc, int type);
static void process_wait(void *go, struct trig_data *trig, int type, char *cmd,
                         struct cmdlist_element *cl);
static void process_attach(void *go, struct script_data *sc, struct trig_data *trig,
                           int type, char *cmd);
static void process_detach(void *go, struct script_data *sc, struct trig_data *trig,
                           int type, char *cmd);
static void makeuid_var(void *go, struct script_data *sc, struct trig_data *trig,
                        int type, char *cmd);
static struct cmdlist_element *find_case(struct trig_data *trig, struct cmdlist_element *cl,
    void *go, struct script_data *sc, int type, char *cond);
static struct cmdlist_element *find_done(struct cmdlist_element *cl);
static struct char_data *find_char_by_uid_in_lookup_table(long uid);
static struct obj_data *find_obj_by_uid_in_lookup_table(long uid);

/* Does this name match the entity? PCs answer to their character name, mobs
 * to their keyword list. */
static bool dg_char_is_named(const char *name, struct char_data *ch)
{
  if (!name || !ch)
    return FALSE;

  if (IS_NPC(ch))
    return isname(name, GET_KEYWORDS(ch)) != 0;

  return isname(name, GET_CHAR_NAME(ch)) != 0;
}

static bool dg_obj_is_named(const char *name, struct obj_data *obj)
{
  if (!name || !obj)
    return FALSE;

  return isname(name, GET_OBJ_KEYWORDS(obj)) != 0;
}

/* A case-insensitive strstr. Named dg_str_str because Awake already has a
 * str_str of its own with different semantics. */
char *dg_str_str(char *cs, const char *ct)
{
  char *s;
  const char *t;

  if (!cs || !ct || !*ct)
    return NULL;

  while (*cs) {
    t = ct;

    while (*cs && (LOWER(*cs) != LOWER(*t)))
      cs++;

    s = cs;

    while (*t && *cs && (LOWER(*cs) == LOWER(*t))) {
      t++;
      cs++;
    }

    /* Reaching the end of ct means the whole string matched. */
    if (!*t)
      return s;
  }

  return NULL;
}

/* How many people are in the room with this vnum, or -1 if there isn't one. */
int trgvar_in_room(vnum_t vnum)
{
  rnum_t rnum = real_room(vnum);
  int i = 0;
  struct char_data *ch;

  if (rnum < 0) {
    script_log("people.vnum: room %ld does not exist", vnum);
    return -1;
  }

  for (ch = world[rnum].people; ch != NULL; ch = ch->next_in_room)
    i++;

  return i;
}

/* Find an object in a list, by uid or by name. Declared in handler.hpp. */
struct obj_data *get_obj_in_list(char *name, struct obj_data *list)
{
  struct obj_data *i;
  long id;

  if (!name)
    return NULL;

  if (*name == UID_CHAR) {
    id = atol(name + 1);

    for (i = list; i; i = i->next_content)
      if (id == i->script_id)
        return i;
  } else {
    for (i = list; i; i = i->next_content)
      if (dg_obj_is_named(name, i))
        return i;
  }

  return NULL;
}

/* Find a worn object on ch, by uid, vnum or name. */
struct obj_data *get_object_in_equip(struct char_data *ch, const char *name)
{
  int j;
  struct obj_data *obj;
  long id;

  if (!ch || !name)
    return NULL;

  if (*name == UID_CHAR) {
    id = atol(name + 1);

    for (j = 0; j < NUM_WEARS; j++)
      if ((obj = GET_EQ(ch, j)) && id == obj->script_id)
        return obj;
  } else if (is_number((char *) name)) {
    vnum_t ovnum = atol(name);

    for (j = 0; j < NUM_WEARS; j++)
      if ((obj = GET_EQ(ch, j)) && GET_OBJ_VNUM(obj) == ovnum)
        return obj;
  } else {
    for (j = 0; j < NUM_WEARS; j++)
      if ((obj = GET_EQ(ch, j)) && dg_obj_is_named(name, obj))
        return obj;
  }

  return NULL;
}

/* Turn the argument of %wear% into a wear location, by name or by number. */
int find_eq_pos_script(char *arg)
{
  int i;

  if (is_number(arg) && (i = atoi(arg)) >= 0 && i < NUM_WEARS)
    return i;

  for (i = 0; i < NUM_WEARS; i++)
    if (!str_cmp(short_where[i], arg))
      return i;

  return -1;
}

/* Find a character by script id. */
struct char_data *find_char(long n)
{
  if (n >= ROOM_ID_BASE) /* see the id map in dg_scripts.hpp */
    return NULL;

  return find_char_by_uid_in_lookup_table(n);
}

/* Find an object by script id. */
struct obj_data *find_obj(long n)
{
  if (n < OBJ_ID_BASE) /* see the id map in dg_scripts.hpp */
    return NULL;

  return find_obj_by_uid_in_lookup_table(n);
}

/* Find a room by script id. */
struct room_data *find_room(long n)
{
  rnum_t rnum;

  n -= ROOM_ID_BASE;
  if (n < 0)
    return NULL;

  rnum = real_room((vnum_t) n);

  if (rnum >= 0)
    return &world[rnum];

  return NULL;
}

/* Generic searches based only on name. */

/* Search the whole world for a character, by uid or name. */
struct char_data *get_char(const char *name)
{
  struct char_data *i;

  if (*name == UID_CHAR) {
    i = find_char(atol(name + 1));

    if (i && valid_dg_target(i, DG_ALLOW_GODS))
      return i;
  } else {
    for (i = character_list; i; i = i->next_in_character_list)
      if (dg_char_is_named(name, i) && valid_dg_target(i, DG_ALLOW_GODS))
        return i;
  }

  return NULL;
}

/* Find a character in the same room as a known object. */
struct char_data *get_char_near_obj(struct obj_data *obj, const char *name)
{
  struct char_data *ch;

  if (*name == UID_CHAR) {
    ch = find_char(atol(name + 1));

    if (ch && valid_dg_target(ch, DG_ALLOW_GODS))
      return ch;
  } else {
    struct room_data *room = obj_room(obj);

    if (room)
      for (ch = room->people; ch; ch = ch->next_in_room)
        if (dg_char_is_named(name, ch) && valid_dg_target(ch, DG_ALLOW_GODS))
          return ch;
  }

  return NULL;
}

/* Find a character in a specific room. */
struct char_data *get_char_in_room(struct room_data *room, const char *name)
{
  struct char_data *ch;

  if (*name == UID_CHAR) {
    ch = find_char(atol(name + 1));

    if (ch && valid_dg_target(ch, DG_ALLOW_GODS))
      return ch;
  } else if (room) {
    for (ch = room->people; ch; ch = ch->next_in_room)
      if (dg_char_is_named(name, ch) && valid_dg_target(ch, DG_ALLOW_GODS))
        return ch;
  }

  return NULL;
}

/* Find an object near another object: inside it, around it, or on the floor. */
struct obj_data *get_obj_near_obj(struct obj_data *obj, const char *name)
{
  struct obj_data *i = NULL;
  struct char_data *ch;
  struct room_data *room;
  long id;

  if (!str_cmp(name, "self") || !str_cmp(name, "me"))
    return obj;

  /* is it inside? */
  if (obj->contains && (i = get_obj_in_list((char *) name, obj->contains)))
    return i;

  /* or outside? */
  if (obj->in_obj) {
    if (*name == UID_CHAR) {
      id = atol(name + 1);

      if (id == obj->in_obj->script_id)
        return obj->in_obj;
    } else if (dg_obj_is_named(name, obj->in_obj)) {
      return obj->in_obj;
    }
  }
  /* or worn? */
  else if (obj->worn_by && (i = get_object_in_equip(obj->worn_by, name))) {
    return i;
  }
  /* or carried? */
  else if (obj->carried_by && (i = get_obj_in_list((char *) name, obj->carried_by->carrying))) {
    return i;
  }
  else if ((room = obj_room(obj))) {
    /* check the floor */
    if ((i = get_obj_in_list((char *) name, room->contents)))
      return i;

    /* check people's equipment */
    for (ch = room->people; ch; ch = ch->next_in_room)
      if ((i = get_object_in_equip(ch, name)))
        return i;
  }

  return NULL;
}

/* The object in the world with this name or uid, or NULL. Declared in
 * handler.hpp. */
struct obj_data *get_obj(char *name)
{
  if (*name == UID_CHAR)
    return find_obj(atol(name + 1));

  for (nodeStruct<struct obj_data *> *node = ObjList.Head(); node; node = node->next)
    if (dg_obj_is_named(name, node->data))
      return node->data;

  return NULL;
}

/* Find a room by uid or by vnum. */
struct room_data *get_room(const char *name)
{
  rnum_t nr;

  if (*name == UID_CHAR)
    return find_room(atol(name + 1));

  if ((nr = real_room(atol(name))) < 0)
    return NULL;

  return &world[nr];
}

/* First character in the world by this name, starting with the object's owner. */
struct char_data *get_char_by_obj(struct obj_data *obj, const char *name)
{
  struct char_data *ch;

  if (*name == UID_CHAR) {
    ch = find_char(atol(name + 1));

    if (ch && valid_dg_target(ch, DG_ALLOW_GODS))
      return ch;
  } else {
    if (obj->carried_by && dg_char_is_named(name, obj->carried_by) &&
        valid_dg_target(obj->carried_by, DG_ALLOW_GODS))
      return obj->carried_by;

    if (obj->worn_by && dg_char_is_named(name, obj->worn_by) &&
        valid_dg_target(obj->worn_by, DG_ALLOW_GODS))
      return obj->worn_by;

    for (ch = character_list; ch; ch = ch->next_in_character_list)
      if (dg_char_is_named(name, ch) && valid_dg_target(ch, DG_ALLOW_GODS))
        return ch;
  }

  return NULL;
}

/* First character in the world by this name, starting in the given room. */
struct char_data *get_char_by_room(struct room_data *room, const char *name)
{
  struct char_data *ch;

  if (*name == UID_CHAR) {
    ch = find_char(atol(name + 1));

    if (ch && valid_dg_target(ch, DG_ALLOW_GODS))
      return ch;
  } else {
    if (room)
      for (ch = room->people; ch; ch = ch->next_in_room)
        if (dg_char_is_named(name, ch) && valid_dg_target(ch, DG_ALLOW_GODS))
          return ch;

    for (ch = character_list; ch; ch = ch->next_in_character_list)
      if (dg_char_is_named(name, ch) && valid_dg_target(ch, DG_ALLOW_GODS))
        return ch;
  }

  return NULL;
}

/* The object in the world with this name, searched relative to obj. */
struct obj_data *get_obj_by_obj(struct obj_data *obj, const char *name)
{
  struct obj_data *i = NULL;
  struct room_data *room;

  if (*name == UID_CHAR)
    return find_obj(atol(name + 1));

  if (!str_cmp(name, "self") || !str_cmp(name, "me"))
    return obj;

  if (obj->contains && (i = get_obj_in_list((char *) name, obj->contains)))
    return i;

  if (obj->in_obj && dg_obj_is_named(name, obj->in_obj))
    return obj->in_obj;

  if (obj->worn_by && (i = get_object_in_equip(obj->worn_by, name)))
    return i;

  if (obj->carried_by && (i = get_obj_in_list((char *) name, obj->carried_by->carrying)))
    return i;

  if ((room = obj_room(obj)) && (i = get_obj_in_list((char *) name, room->contents)))
    return i;

  return get_obj((char *) name);
}

/* only searches the room */
struct obj_data *get_obj_in_room(struct room_data *room, const char *name)
{
  struct obj_data *obj;
  long id;

  if (!room)
    return NULL;

  if (*name == UID_CHAR) {
    id = atol(name + 1);
    for (obj = room->contents; obj; obj = obj->next_content)
      if (id == obj->script_id)
        return obj;
  } else {
    for (obj = room->contents; obj; obj = obj->next_content)
      if (dg_obj_is_named(name, obj))
        return obj;
  }

  return NULL;
}

/* searches the room, then the world */
struct obj_data *get_obj_by_room(struct room_data *room, const char *name)
{
  struct obj_data *obj;

  if (*name == UID_CHAR)
    return find_obj(atol(name + 1));

  if (room)
    for (obj = room->contents; obj; obj = obj->next_content)
      if (dg_obj_is_named(name, obj))
        return obj;

  return get_obj((char *) name);
}

/* checks every PULSE_DG_SCRIPT for random triggers */
void script_trigger_check(void)
{
  struct char_data *ch, *next_ch;
  struct room_data *room = NULL;
  rnum_t nr;
  struct script_data *sc;

  for (ch = character_list; ch; ch = next_ch) {
    next_ch = ch->next_in_character_list;

    if ((sc = SCRIPT(ch))) {
      if (IS_SET(SCRIPT_TYPES(sc), MTRIG_RANDOM) &&
          (!ch->in_room || !zone_is_empty(ch->in_room->zone) ||
           IS_SET(SCRIPT_TYPES(sc), MTRIG_GLOBAL)))
        random_mtrigger(ch);
    }
  }

  for (nodeStruct<struct obj_data *> *node = ObjList.Head(), *next_node; node; node = next_node) {
    next_node = node->next;

    if ((sc = SCRIPT(node->data)))
      if (IS_SET(SCRIPT_TYPES(sc), OTRIG_RANDOM))
        random_otrigger(node->data);
  }

  for (nr = 0; nr <= top_of_world; nr++) {
    room = &world[nr];

    if ((sc = SCRIPT(room))) {
      if (IS_SET(SCRIPT_TYPES(sc), WTRIG_RANDOM) &&
          (!zone_is_empty(room->zone) || IS_SET(SCRIPT_TYPES(sc), WTRIG_GLOBAL)))
        random_wtrigger(room);
    }
  }

}

void check_time_triggers(void)
{
  struct char_data *ch, *next_ch;
  struct room_data *room = NULL;
  rnum_t nr;
  struct script_data *sc;

  for (ch = character_list; ch; ch = next_ch) {
    next_ch = ch->next_in_character_list;

    if ((sc = SCRIPT(ch))) {
      if (IS_SET(SCRIPT_TYPES(sc), MTRIG_TIME) &&
          (!ch->in_room || !zone_is_empty(ch->in_room->zone) ||
           IS_SET(SCRIPT_TYPES(sc), MTRIG_GLOBAL)))
        time_mtrigger(ch);
    }
  }

  for (nodeStruct<struct obj_data *> *node = ObjList.Head(), *next_node; node; node = next_node) {
    next_node = node->next;

    if ((sc = SCRIPT(node->data)))
      if (IS_SET(SCRIPT_TYPES(sc), OTRIG_TIME))
        time_otrigger(node->data);
  }

  for (nr = 0; nr <= top_of_world; nr++) {
    room = &world[nr];

    if ((sc = SCRIPT(room))) {
      if (IS_SET(SCRIPT_TYPES(sc), WTRIG_TIME) &&
          (!zone_is_empty(room->zone) || IS_SET(SCRIPT_TYPES(sc), WTRIG_GLOBAL)))
        time_wtrigger(room);
    }
  }

}

/* Name the triggers attached to something, for the stat commands. Says
 * nothing at all when there are none, so stat output for the overwhelming
 * majority of the world is unchanged. */
void dg_stat_triggers(struct char_data *ch, struct script_data *sc)
{
  bool printed_any = FALSE;

  if (!sc)
    return;

  for (struct trig_data *t = TRIGGERS(sc); t; t = t->next) {
    if (!printed_any)
      send_to_char("Triggers: ", ch);

    send_to_char(ch, "%s^c%s [%ld]^n", printed_any ? ", " : "",
                 GET_TRIG_NAME(t) ? GET_TRIG_NAME(t) : "unnamed",
                 (long) GET_TRIG_VNUM(t));
    printed_any = TRUE;
  }

  if (printed_any)
    send_to_char("\r\n", ch);
}

/* ************************************************************************
*  Deferred extraction.                                                    *
*                                                                          *
*  Awake's extract_char() and extract_obj() free immediately, so a script   *
*  that purges the thing running it would be reading freed memory the       *
*  moment script_driver() looked at its next line. %purge% on the script's  *
*  own owner therefore records the intent here and lets the driver unwind   *
*  first; the pulse hook does the actual extraction.                        *
************************************************************************ */

static struct char_data *pending_char_extractions[64];
static int pending_char_count = 0;
static struct obj_data *pending_obj_extractions[64];
static int pending_obj_count = 0;

void dg_note_char_extraction(struct char_data *ch)
{
  for (int i = 0; i < pending_char_count; i++)
    if (pending_char_extractions[i] == ch)
      return;

  if (pending_char_count >= (int) (sizeof(pending_char_extractions) / sizeof(pending_char_extractions[0]))) {
    mudlog("SYSERR: DG Scripts deferred-extraction table for characters is full.", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  pending_char_extractions[pending_char_count++] = ch;
}

void dg_note_obj_extraction(struct obj_data *obj)
{
  for (int i = 0; i < pending_obj_count; i++)
    if (pending_obj_extractions[i] == obj)
      return;

  if (pending_obj_count >= (int) (sizeof(pending_obj_extractions) / sizeof(pending_obj_extractions[0]))) {
    mudlog("SYSERR: DG Scripts deferred-extraction table for objects is full.", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  pending_obj_extractions[pending_obj_count++] = obj;
}

void dg_flush_pending_extractions(void)
{
  while (pending_char_count > 0) {
    struct char_data *ch = pending_char_extractions[--pending_char_count];

    /* Something else may have got to it first; only extract what is still
     * on the character list. */
    for (struct char_data *i = character_list; i; i = i->next_in_character_list) {
      if (i == ch) {
        extract_char(ch);
        break;
      }
    }
  }

  while (pending_obj_count > 0) {
    struct obj_data *obj = pending_obj_extractions[--pending_obj_count];

    for (nodeStruct<struct obj_data *> *node = ObjList.Head(); node; node = node->next) {
      if (node->data == obj) {
        extract_obj(obj);
        break;
      }
    }
  }
}

/* ************************************************************************ */

static EVENTFUNC(trig_wait_event)
{
  struct wait_event_data *wait_event_obj = (struct wait_event_data *) event_obj;
  struct trig_data *trig;
  void *go;
  int type;
  bool found = FALSE;

  trig = wait_event_obj->trigger;
  go = wait_event_obj->go;
  type = wait_event_obj->type;

  delete wait_event_obj;
  GET_TRIG_WAIT(trig) = NULL;

  /* The entity may have been extracted while the script slept. */
  if (type == MOB_TRIGGER) {
    for (struct char_data *tch = character_list; tch && !found; tch = tch->next_in_character_list)
      if (tch == (struct char_data *) go)
        found = TRUE;
  } else if (type == OBJ_TRIGGER) {
    for (nodeStruct<struct obj_data *> *node = ObjList.Head(); node && !found; node = node->next)
      if (node->data == (struct obj_data *) go)
        found = TRUE;
  } else {
    for (rnum_t i = 0; i <= top_of_world && !found; i++)
      if (&world[i] == (struct room_data *) go)
        found = TRUE;
  }

  if (!found) {
    script_log("Trigger %ld tried to restart on an entity that no longer exists.",
               (long) GET_TRIG_VNUM(trig));
    return 0;
  }

  script_driver(&go, trig, type, TRIG_RESTART);

  /* Do not re-enqueue. */
  return 0;
}

static void do_stat_trigger(struct char_data *ch, struct trig_data *trig)
{
  struct cmdlist_element *cmd_list;
  char sb[MAX_STRING_LENGTH], flagbuf[MAX_STRING_LENGTH];
  size_t len = 0;

  if (!trig) {
    log("SYSERR: NULL trigger passed to do_stat_trigger.");
    return;
  }

  len += snprintf(sb, sizeof(sb), "Name: '^y%s^n',  VNum: [^g%5ld^n], RNum: [%5ld]\r\n",
                  GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), (long) GET_TRIG_RNUM(trig));

  if (trig->attach_type == OBJ_TRIGGER) {
    len += snprintf(sb + len, sizeof(sb) - len, "Trigger Intended Assignment: Objects\r\n");
    sprintbit(GET_TRIG_TYPE(trig), otrig_types, flagbuf, sizeof(flagbuf));
  } else if (trig->attach_type == WLD_TRIGGER) {
    len += snprintf(sb + len, sizeof(sb) - len, "Trigger Intended Assignment: Rooms\r\n");
    sprintbit(GET_TRIG_TYPE(trig), wtrig_types, flagbuf, sizeof(flagbuf));
  } else {
    len += snprintf(sb + len, sizeof(sb) - len, "Trigger Intended Assignment: Mobiles\r\n");
    sprintbit(GET_TRIG_TYPE(trig), trig_types, flagbuf, sizeof(flagbuf));
  }

  len += snprintf(sb + len, sizeof(sb) - len, "Trigger Type: %s, Numeric Arg: %d, Arg list: %s\r\n",
                  flagbuf, GET_TRIG_NARG(trig),
                  ((GET_TRIG_ARG(trig) && *GET_TRIG_ARG(trig)) ? GET_TRIG_ARG(trig) : "None"));

  len += snprintf(sb + len, sizeof(sb) - len, "Commands:\r\n");

  for (cmd_list = trig->cmdlist; cmd_list; cmd_list = cmd_list->next) {
    if (len > sizeof(sb) - 128) {
      snprintf(sb + len, sizeof(sb) - len, "*** Overflow - script too long! ***\r\n");
      break;
    }

    if (cmd_list->cmd)
      len += snprintf(sb + len, sizeof(sb) - len, "%s\r\n", cmd_list->cmd);
  }

  page_string(ch->desc, sb, 1);
}

/* find the name of whatever the uid points at */
void find_uid_name(const char *uid, char *name, size_t nlen)
{
  struct char_data *ch;
  struct obj_data *obj;
  struct room_data *room;

  if ((ch = get_char(uid)))
    snprintf(name, nlen, "%s", GET_CHAR_NAME(ch));
  else if ((obj = get_obj((char *) uid)))
    snprintf(name, nlen, "%s", GET_OBJ_NAME(obj));
  else if ((room = find_room(atol(uid + 1))))
    snprintf(name, nlen, "%s", GET_ROOM_NAME(room));
  else
    snprintf(name, nlen, "uid = %s, (not found)", uid + 1);
}

/* general function to display stats on script sc */
static void script_stat(struct char_data *ch, struct script_data *sc)
{
  struct trig_var_data *tv;
  struct trig_data *t;
  char name[MAX_INPUT_LENGTH];
  char namebuf[512];
  char flagbuf[MAX_STRING_LENGTH];

  send_to_char(ch, "Global Variables: %s\r\n", sc->global_vars ? "" : "None");
  send_to_char(ch, "Global context: %ld\r\n", sc->context);

  for (tv = sc->global_vars; tv; tv = tv->next) {
    snprintf(namebuf, sizeof(namebuf), "%s:%ld", tv->name, tv->context);
    if (*(tv->value) == UID_CHAR) {
      find_uid_name(tv->value, name, sizeof(name));
      send_to_char(ch, "    %15s:  %s\r\n", tv->context ? namebuf : tv->name, name);
    } else {
      send_to_char(ch, "    %15s:  %s\r\n", tv->context ? namebuf : tv->name, tv->value);
    }
  }

  for (t = TRIGGERS(sc); t; t = t->next) {
    send_to_char(ch, "\r\n  Trigger: ^y%s^n, VNum: [^g%5ld^n], RNum: [%5ld]\r\n",
                 GET_TRIG_NAME(t), (long) GET_TRIG_VNUM(t), (long) GET_TRIG_RNUM(t));

    if (t->attach_type == OBJ_TRIGGER) {
      send_to_char("  Trigger Intended Assignment: Objects\r\n", ch);
      sprintbit(GET_TRIG_TYPE(t), otrig_types, flagbuf, sizeof(flagbuf));
    } else if (t->attach_type == WLD_TRIGGER) {
      send_to_char("  Trigger Intended Assignment: Rooms\r\n", ch);
      sprintbit(GET_TRIG_TYPE(t), wtrig_types, flagbuf, sizeof(flagbuf));
    } else {
      send_to_char("  Trigger Intended Assignment: Mobiles\r\n", ch);
      sprintbit(GET_TRIG_TYPE(t), trig_types, flagbuf, sizeof(flagbuf));
    }

    send_to_char(ch, "  Trigger Type: %s, Numeric Arg: %d, Arg list: %s\r\n",
                 flagbuf, GET_TRIG_NARG(t),
                 ((GET_TRIG_ARG(t) && *GET_TRIG_ARG(t)) ? GET_TRIG_ARG(t) : "None"));

    if (GET_TRIG_WAIT(t)) {
      send_to_char(ch, "    Wait: %ld, Current line: %s\r\n",
                   event_time(GET_TRIG_WAIT(t)),
                   t->curr_state ? t->curr_state->cmd : "End of Script");
      send_to_char(ch, "  Variables: %s\r\n", GET_TRIG_VARS(t) ? "" : "None");

      for (tv = GET_TRIG_VARS(t); tv; tv = tv->next) {
        if (*(tv->value) == UID_CHAR) {
          find_uid_name(tv->value, name, sizeof(name));
          send_to_char(ch, "    %15s:  %s\r\n", tv->name, name);
        } else {
          send_to_char(ch, "    %15s:  %s\r\n", tv->name, tv->value);
        }
      }
    }
  }
}

void do_sstat_room(struct char_data *ch, struct room_data *rm)
{
  send_to_char("Triggers:\r\n", ch);
  if (!SCRIPT(rm)) {
    send_to_char("  None.\r\n", ch);
    return;
  }

  script_stat(ch, SCRIPT(rm));
}

void do_sstat_object(struct char_data *ch, struct obj_data *j)
{
  send_to_char("Triggers:\r\n", ch);
  if (!SCRIPT(j)) {
    send_to_char("  None.\r\n", ch);
    return;
  }

  script_stat(ch, SCRIPT(j));
}

void do_sstat_character(struct char_data *ch, struct char_data *k)
{
  send_to_char("Triggers:\r\n", ch);
  if (!SCRIPT(k)) {
    send_to_char("  None.\r\n", ch);
    return;
  }

  script_stat(ch, SCRIPT(k));
}

/* Add trigger t to script sc at position loc. loc == -1 appends, loc == 0
 * puts it in front of everything else. */
void add_trigger(struct script_data *sc, struct trig_data *t, int loc)
{
  struct trig_data *i;
  int n;

  for (n = loc, i = TRIGGERS(sc); i && i->next && (n != 0); n--, i = i->next)
    ;

  if (!loc) {
    t->next = TRIGGERS(sc);
    TRIGGERS(sc) = t;
  } else if (!i) {
    TRIGGERS(sc) = t;
  } else {
    t->next = i->next;
    i->next = t;
  }

  SCRIPT_TYPES(sc) |= GET_TRIG_TYPE(t);

  t->next_in_world = trigger_list;
  trigger_list = t;
}

ACMD(do_tattach)
{
  struct char_data *victim;
  struct obj_data *object;
  struct room_data *room;
  struct trig_data *trig;
  char targ_name[MAX_INPUT_LENGTH], trig_name[MAX_INPUT_LENGTH];
  char loc_name[MAX_INPUT_LENGTH], arg[MAX_INPUT_LENGTH];
  int loc;
  rnum_t rn;
  vnum_t tn, num_arg;

  argument = two_arguments(argument, arg, trig_name);
  two_arguments(argument, targ_name, loc_name);

  if (!*arg || !*targ_name || !*trig_name) {
    send_to_char("Usage: tattach { mob | obj | room } { trigger } { name } [ location ]\r\n", ch);
    return;
  }

  num_arg = atol(targ_name);
  tn = atol(trig_name);
  loc = (*loc_name) ? atoi(loc_name) : -1;

  if (is_abbrev(arg, "mobile") || is_abbrev(arg, "mtr")) {
    victim = get_char_vis(ch, targ_name);
    if (!victim) { /* search the room for one with this vnum */
      for (victim = ch->in_room ? ch->in_room->people : NULL; victim; victim = victim->next_in_room)
        if (GET_MOB_VNUM(victim) == num_arg)
          break;

      if (!victim) {
        send_to_char("That mob does not exist.\r\n", ch);
        return;
      }
    }

    if (!IS_NPC(victim)) {
      send_to_char("Players can't have scripts.\r\n", ch);
      return;
    }

    if (!can_edit_zone(ch, &zone_table[victim->in_room ? victim->in_room->zone : 0])) {
      send_to_char("You can only attach triggers in your own zone.\r\n", ch);
      return;
    }

    rn = real_trigger(tn);
    if (rn < 0 || !(trig = read_trigger(rn))) {
      send_to_char("That trigger does not exist.\r\n", ch);
      return;
    }

    if (!SCRIPT(victim))
      SCRIPT(victim) = new script_data;
    add_trigger(SCRIPT(victim), trig, loc);

    send_to_char(ch, "Trigger %ld (%s) attached to %s [%ld].\r\n",
                 (long) tn, GET_TRIG_NAME(trig), GET_CHAR_NAME(victim), (long) GET_MOB_VNUM(victim));
  }

  else if (is_abbrev(arg, "object") || is_abbrev(arg, "otr")) {
    object = get_obj_vis(ch, targ_name);
    if (!object) { /* search the room, then inventory, for this vnum */
      for (object = ch->in_room ? ch->in_room->contents : NULL; object; object = object->next_content)
        if (GET_OBJ_VNUM(object) == num_arg)
          break;

      if (!object) {
        for (object = ch->carrying; object; object = object->next_content)
          if (GET_OBJ_VNUM(object) == num_arg)
            break;

        if (!object) {
          send_to_char("That object does not exist.\r\n", ch);
          return;
        }
      }
    }

    if (!can_edit_zone(ch, &zone_table[ch->in_room ? ch->in_room->zone : 0])) {
      send_to_char("You can only attach triggers in your own zone.\r\n", ch);
      return;
    }

    rn = real_trigger(tn);
    if (rn < 0 || !(trig = read_trigger(rn))) {
      send_to_char("That trigger does not exist.\r\n", ch);
      return;
    }

    if (!SCRIPT(object))
      SCRIPT(object) = new script_data;
    add_trigger(SCRIPT(object), trig, loc);

    send_to_char(ch, "Trigger %ld (%s) attached to %s [%ld].\r\n",
                 (long) tn, GET_TRIG_NAME(trig), GET_OBJ_NAME(object), (long) GET_OBJ_VNUM(object));
  }

  else if (is_abbrev(arg, "room") || is_abbrev(arg, "wtr")) {
    rnum_t rnum;

    if (strchr(targ_name, '.'))
      rnum = ch->in_room ? real_room(GET_ROOM_VNUM(ch->in_room)) : -1;
    else if (isdigit(*targ_name))
      rnum = real_room(atol(targ_name));
    else
      rnum = -1;

    if (rnum < 0) {
      send_to_char("You need to supply a room number, or . for the current room.\r\n", ch);
      return;
    }

    if (!can_edit_zone(ch, &zone_table[world[rnum].zone])) {
      send_to_char("You can only attach triggers in your own zone.\r\n", ch);
      return;
    }

    rn = real_trigger(tn);
    if (rn < 0 || !(trig = read_trigger(rn))) {
      send_to_char("That trigger does not exist.\r\n", ch);
      return;
    }

    room = &world[rnum];

    if (!SCRIPT(room))
      SCRIPT(room) = new script_data;
    add_trigger(SCRIPT(room), trig, loc);

    send_to_char(ch, "Trigger %ld (%s) attached to room %ld.\r\n",
                 (long) tn, GET_TRIG_NAME(trig), (long) world[rnum].number);
  }

  else {
    send_to_char("Please specify 'mob', 'obj', or 'room'.\r\n", ch);
  }
}

/* Remove the trigger named by name, which can be a number or something like
 * 2.beggar-death. Returns 0 if the trigger was not found, else 1. The caller
 * has to decide whether to drop the whole script once the last one is gone. */
static int remove_trigger(struct script_data *sc, char *name)
{
  struct trig_data *i, *j;
  int num = 0, string = FALSE, n;
  char *cname;

  if (!sc || !name)
    return 0;

  if ((cname = strstr(name, ".")) || (!isdigit(*name))) {
    string = TRUE;
    if (cname) {
      *cname = '\0';
      num = atoi(name);
      name = ++cname;
    }
  } else {
    num = atoi(name);
  }

  for (n = 0, j = NULL, i = TRIGGERS(sc); i; j = i, i = i->next) {
    if (string) {
      if (isname(name, GET_TRIG_NAME(i)))
        if (++n >= num)
          break;
    }
    /* A number matches either a position or a vnum; it used to be position
     * only. */
    else if (++n >= num)
      break;
    else if (GET_TRIG_VNUM(i) == num)
      break;
  }

  if (!i)
    return 0;

  if (j)
    j->next = i->next;
  else
    TRIGGERS(sc) = i->next;

  extract_trigger(i);

  /* rebuild the script type bitvector */
  SCRIPT_TYPES(sc) = 0;
  for (i = TRIGGERS(sc); i; i = i->next)
    SCRIPT_TYPES(sc) |= GET_TRIG_TYPE(i);

  return 1;
}

ACMD(do_tdetach)
{
  struct char_data *victim = NULL;
  struct obj_data *object = NULL;
  struct room_data *room;
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH], *snum;
  char *trigger = NULL;
  vnum_t num_arg;

  argument = two_arguments(argument, arg1, arg2);
  one_argument(argument, arg3);

  if (!*arg1 || !*arg2) {
    send_to_char("Usage: tdetach [ mob | object | room ] { target } { trigger | 'all' }\r\n", ch);
    return;
  }

  /* vnum of mob/obj, if given */
  num_arg = atol(arg2);

  if (!str_cmp(arg1, "room") || !str_cmp(arg1, "wtr")) {
    rnum_t rnum;

    if (!*arg3 || strchr(arg2, '.'))
      rnum = ch->in_room ? real_room(GET_ROOM_VNUM(ch->in_room)) : -1;
    else if (isdigit(*arg2))
      rnum = real_room(atol(arg2));
    else
      rnum = -1;

    if (rnum < 0) {
      send_to_char("That's not a valid room.\r\n", ch);
      return;
    }

    room = &world[rnum];
    if (!can_edit_zone(ch, &zone_table[room->zone])) {
      send_to_char("You can only detach triggers in your own zone.\r\n", ch);
      return;
    }

    if (!SCRIPT(room)) {
      send_to_char("This room does not have any triggers.\r\n", ch);
    } else if (!str_cmp(arg2, "all") || !str_cmp(arg3, "all")) {
      extract_script(room, WLD_TRIGGER);
      send_to_char(ch, "All triggers removed from room %ld.\r\n", (long) room->number);
    } else {
      snum = *arg3 ? arg3 : arg2;

      if (remove_trigger(SCRIPT(room), snum)) {
        send_to_char(ch, "Trigger removed from room %ld.\r\n", (long) room->number);

        if (!TRIGGERS(SCRIPT(room)))
          extract_script(room, WLD_TRIGGER);
      } else {
        send_to_char("That trigger was not found.\r\n", ch);
      }
    }

    return;
  }

  if (is_abbrev(arg1, "mobile") || !str_cmp(arg1, "mtr")) {
    victim = get_char_vis(ch, arg2);
    if (!victim) {
      for (victim = ch->in_room ? ch->in_room->people : NULL; victim; victim = victim->next_in_room)
        if (GET_MOB_VNUM(victim) == num_arg)
          break;

      if (!victim) {
        send_to_char("No such mobile around.\r\n", ch);
        return;
      }
    }

    if (!*arg3) {
      send_to_char("You must specify a trigger to remove.\r\n", ch);
      return;
    }

    trigger = arg3;
  }

  else if (is_abbrev(arg1, "object") || !str_cmp(arg1, "otr")) {
    object = get_obj_vis(ch, arg2);
    if (!object) {
      for (object = ch->in_room ? ch->in_room->contents : NULL; object; object = object->next_content)
        if (GET_OBJ_VNUM(object) == num_arg)
          break;

      if (!object) {
        for (object = ch->carrying; object; object = object->next_content)
          if (GET_OBJ_VNUM(object) == num_arg)
            break;

        if (!object) {
          send_to_char("No such object around.\r\n", ch);
          return;
        }
      }
    }

    if (!*arg3) {
      send_to_char("You must specify a trigger to remove.\r\n", ch);
      return;
    }

    trigger = arg3;
  }

  else {
    int eq_pos;

    if ((object = get_object_in_equip_vis(ch, arg1, ch->equipment, &eq_pos)))
      ;
    else if ((object = get_obj_in_list_vis(ch, arg1, ch->carrying)))
      ;
    else if ((victim = get_char_room_vis(ch, arg1)))
      ;
    else if ((object = get_obj_in_list_vis(ch, arg1, ch->in_room ? ch->in_room->contents : NULL)))
      ;
    else if ((victim = get_char_vis(ch, arg1)))
      ;
    else if ((object = get_obj_vis(ch, arg1)))
      ;
    else {
      send_to_char("Nothing around by that name.\r\n", ch);
      return;
    }

    trigger = arg2;
  }

  if (victim) {
    if (!SCRIPT(victim)) {
      send_to_char(ch, "That %s doesn't have any triggers.\r\n", IS_NPC(victim) ? "mob" : "player");
    } else if (IS_NPC(victim) && victim->in_room &&
               !can_edit_zone(ch, &zone_table[victim->in_room->zone])) {
      send_to_char("You can only detach triggers in your own zone.\r\n", ch);
    } else if (trigger && !str_cmp(trigger, "all")) {
      extract_script(victim, MOB_TRIGGER);
      send_to_char(ch, "All triggers removed from %s.\r\n", GET_CHAR_NAME(victim));
    } else if (trigger && remove_trigger(SCRIPT(victim), trigger)) {
      send_to_char(ch, "Trigger removed from %s.\r\n", GET_CHAR_NAME(victim));

      if (!TRIGGERS(SCRIPT(victim)))
        extract_script(victim, MOB_TRIGGER);
    } else {
      send_to_char("That trigger was not found.\r\n", ch);
    }
  }

  else if (object) {
    if (!SCRIPT(object)) {
      send_to_char("That object doesn't have any triggers.\r\n", ch);
    } else if (trigger && !str_cmp(trigger, "all")) {
      extract_script(object, OBJ_TRIGGER);
      send_to_char(ch, "All triggers removed from %s.\r\n", GET_OBJ_NAME(object));
    } else if (trigger && remove_trigger(SCRIPT(object), trigger)) {
      send_to_char(ch, "Trigger removed from %s.\r\n", GET_OBJ_NAME(object));

      if (!TRIGGERS(SCRIPT(object)))
        extract_script(object, OBJ_TRIGGER);
    } else {
      send_to_char("That trigger was not found.\r\n", ch);
    }
  }
}

/* Script errors go to the zone log, which is where the builders who can fix
 * them are already looking. */
void script_log(const char *format, ...)
{
  char output[MAX_STRING_LENGTH];
  va_list args;

  va_start(args, format);
  vsnprintf(output, sizeof(output) - 2, format, args);
  va_end(args);

  mudlog_vfprintf(NULL, LOG_ZONELOG, "SCRIPT ERROR: %s", output);
}

/* Returns 1 if the string is a number, else 0. */
static int is_num(char *arg)
{
  if (*arg == '\0')
    return FALSE;

  if (*arg == '+' || *arg == '-')
    arg++;

  if (*arg == '\0')
    return FALSE;

  for (; *arg != '\0'; arg++)
    if (!isdigit(*arg))
      return FALSE;

  return TRUE;
}

/* evaluates 'lhs op rhs', and copies the answer to result */
static void eval_op(const char *op, char *lhs, char *rhs, char *result, void *go,
                    struct script_data *sc, struct trig_data *trig)
{
  unsigned char *p;
  int n;

  /* strip off extra spaces at the beginning and the end */
  while (*lhs && isspace(*lhs))
    lhs++;
  while (*rhs && isspace(*rhs))
    rhs++;

  /* Test the bound before dereferencing. On an empty operand the scan below
   * stops on the terminator, --p then steps in front of the buffer, and
   * reading *p there is out of bounds. */
  for (p = (unsigned char *) lhs; *p; p++)
    ;
  for (--p; ((char *) p > lhs) && isspace(*p); *p-- = '\0')
    ;
  for (p = (unsigned char *) rhs; *p; p++)
    ;
  for (--p; ((char *) p > rhs) && isspace(*p); *p-- = '\0')
    ;

  /* find the op, and work out the value */
  if (!strcmp("||", op)) {
    strcpy(result, ((!*lhs || (*lhs == '0')) && (!*rhs || (*rhs == '0'))) ? "0" : "1");
  }

  else if (!strcmp("&&", op)) {
    strcpy(result, (!*lhs || (*lhs == '0') || !*rhs || (*rhs == '0')) ? "0" : "1");
  }

  else if (!strcmp("==", op)) {
    if (is_num(lhs) && is_num(rhs))
      sprintf(result, "%d", atoi(lhs) == atoi(rhs));
    else
      sprintf(result, "%d", !str_cmp(lhs, rhs));
  }

  else if (!strcmp("!=", op)) {
    if (is_num(lhs) && is_num(rhs))
      sprintf(result, "%d", atoi(lhs) != atoi(rhs));
    else
      sprintf(result, "%d", str_cmp(lhs, rhs) != 0);
  }

  else if (!strcmp("<=", op)) {
    if (is_num(lhs) && is_num(rhs))
      sprintf(result, "%d", atoi(lhs) <= atoi(rhs));
    else
      sprintf(result, "%d", str_cmp(lhs, rhs) <= 0);
  }

  else if (!strcmp(">=", op)) {
    if (is_num(lhs) && is_num(rhs))
      sprintf(result, "%d", atoi(lhs) >= atoi(rhs));
    else
      sprintf(result, "%d", str_cmp(lhs, rhs) >= 0);
  }

  else if (!strcmp("<", op)) {
    if (is_num(lhs) && is_num(rhs))
      sprintf(result, "%d", atoi(lhs) < atoi(rhs));
    else
      sprintf(result, "%d", str_cmp(lhs, rhs) < 0);
  }

  else if (!strcmp(">", op)) {
    if (is_num(lhs) && is_num(rhs))
      sprintf(result, "%d", atoi(lhs) > atoi(rhs));
    else
      sprintf(result, "%d", str_cmp(lhs, rhs) > 0);
  }

  else if (!strcmp("/=", op))
    sprintf(result, "%c", dg_str_str(lhs, rhs) ? '1' : '0');

  else if (!strcmp("*", op))
    sprintf(result, "%d", atoi(lhs) * atoi(rhs));

  else if (!strcmp("/", op))
    sprintf(result, "%d", (n = atoi(rhs)) ? (atoi(lhs) / n) : 0);

  else if (!strcmp("+", op))
    sprintf(result, "%d", atoi(lhs) + atoi(rhs));

  else if (!strcmp("-", op))
    sprintf(result, "%d", atoi(lhs) - atoi(rhs));

  else if (!strcmp("!", op)) {
    if (is_num(rhs))
      sprintf(result, "%d", !atoi(rhs));
    else
      sprintf(result, "%d", !*rhs);
  }
}

/* p points at the first quote; returns the matching end quote, or the last
 * non-null char in p. */
char *matching_quote(char *p)
{
  for (p++; *p && (*p != '"'); p++) {
    if (*p == '\\')
      p++;
  }

  if (!*p)
    p--;

  return p;
}

/* p points at the first paren; returns the matching closing paren, or the
 * last non-null char in p. */
static char *matching_paren(char *p)
{
  int i;

  for (p++, i = 1; *p && i; p++) {
    if (*p == '(')
      i++;
    else if (*p == ')')
      i--;
    else if (*p == '"')
      p = matching_quote(p);
  }

  return --p;
}

/* evaluates line, and returns the answer in result */
static void eval_expr(char *line, char *result, void *go, struct script_data *sc,
                      struct trig_data *trig, int type)
{
  char expr[MAX_INPUT_LENGTH], *p;

  while (*line && isspace(*line))
    line++;

  /* The condition on an 'if', 'while' or 'switch' is a slice of a trigger
   * line, which carries no length limit of its own, while expr[] here,
   * line[] in eval_lhs_op_rhs() and the tokens[] array beside it are all
   * sized MAX_INPUT_LENGTH. An over-long condition evaluates to nothing,
   * which reads as false. */
  if (strlen(line) >= MAX_INPUT_LENGTH) {
    script_log("Trigger: %s, VNum %ld, type: %d. Expression is %d characters, over the %d limit: '%.60s...'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), type,
               (int) strlen(line), MAX_INPUT_LENGTH - 1, line);
    *result = '\0';
    return;
  }

  if (eval_lhs_op_rhs(line, result, go, sc, trig, type))
    ;

  else if (*line == '(') {
    strcpy(expr, line);
    p = matching_paren(expr);
    *p = '\0';
    eval_expr(expr + 1, result, go, sc, trig, type);
  }

  else
    var_subst(go, sc, trig, type, line, result);
}

/* Evaluates expr if it is of the form lhs op rhs, copying the answer into
 * result. Returns 1 if expr was evaluated, else 0. */
static int eval_lhs_op_rhs(char *expr, char *result, void *go, struct script_data *sc,
                           struct trig_data *trig, int type)
{
  char *p, *tokens[MAX_INPUT_LENGTH];
  char line[MAX_INPUT_LENGTH], lhr[MAX_INPUT_LENGTH], rhr[MAX_INPUT_LENGTH];
  int i, j;

  /* valid operands, in order of priority; each must also be handled by
   * eval_op() */
  static const char *ops[] = {
    "||", "&&", "==", "!=", "<=", ">=", "<", ">", "/=", "-", "+", "/", "*", "!", "\n"
  };

  p = strcpy(line, expr);

  /* Initialize tokens, an array of pointers into line where the ops could
   * possibly occur. */
  for (j = 0; *p; j++) {
    tokens[j] = p;
    if (*p == '(')
      p = matching_paren(p) + 1;
    else if (*p == '"')
      p = matching_quote(p) + 1;
    else if (isalnum(*p))
      for (p++; *p && (isalnum(*p) || isspace(*p)); p++)
        ;
    else
      p++;
  }
  tokens[j] = NULL;

  for (i = 0; *ops[i] != '\n'; i++)
    for (j = 0; tokens[j]; j++)
      if (!strncmp(ops[i], tokens[j], strlen(ops[i]))) {
        *tokens[j] = '\0';
        p = tokens[j] + strlen(ops[i]);

        eval_expr(line, lhr, go, sc, trig, type);
        eval_expr(p, rhr, go, sc, trig, type);
        eval_op(ops[i], lhr, rhr, result, go, sc, trig);

        return 1;
      }

  return 0;
}

/* returns 1 if cond is true, else 0 */
static int process_if(char *cond, void *go, struct script_data *sc,
                      struct trig_data *trig, int type)
{
  char result[MAX_INPUT_LENGTH], *p;

  eval_expr(cond, result, go, sc, trig, type);

  p = result;
  skip_spaces(&p);

  return (!*p || *p == '0') ? 0 : 1;
}

/* Scans for the end of an if-block. Returns the line holding 'end', or the
 * last line of the trigger if there isn't one. */
static struct cmdlist_element *find_end(struct trig_data *trig, struct cmdlist_element *cl)
{
  struct cmdlist_element *c;
  char *p;

  if (!(cl->next)) { /* if this is the last line, there is no end */
    script_log("Trigger VNum %ld has 'if' without 'end'. (error 1)", (long) GET_TRIG_VNUM(trig));
    return cl;
  }

  for (c = cl->next; c; c = c->next) {
    for (p = c->cmd; *p && isspace(*p); p++)
      ;

    if (!strncmp("if ", p, 3))
      c = find_end(trig, c);
    else if (!strncmp("end", p, 3))
      return c;

    if (!c->next) { /* last line, and we didn't find an end */
      script_log("Trigger VNum %ld has 'if' without 'end'. (error 2)", (long) GET_TRIG_VNUM(trig));
      return c;
    }
  }

  script_log("Trigger VNum %ld has 'if' without 'end'. (error 3)", (long) GET_TRIG_VNUM(trig));
  return c;
}

/* Searches for a valid elseif, else, or end to continue execution at. */
static struct cmdlist_element *find_else_end(struct trig_data *trig,
    struct cmdlist_element *cl, void *go, struct script_data *sc, int type)
{
  struct cmdlist_element *c;
  char *p;

  if (!(cl->next))
    return cl;

  for (c = cl->next; c->next; c = c->next) {
    for (p = c->cmd; *p && isspace(*p); p++) /* skip spaces */
      ;

    if (!strncmp("if ", p, 3)) {
      c = find_end(trig, c);
    }

    else if (!strncmp("elseif ", p, 7)) {
      if (process_if(p + 7, go, sc, trig, type)) {
        GET_TRIG_DEPTH(trig)++;
        return c;
      }
    }

    else if (!strncmp("else", p, 4)) {
      GET_TRIG_DEPTH(trig)++;
      return c;
    }

    else if (!strncmp("end", p, 3)) {
      return c;
    }

    if (!c->next) /* last line, return */
      return c;
  }

  for (p = c->cmd; *p && isspace(*p); p++) /* skip spaces */
    ;
  if (strncmp("end", p, 3))
    script_log("Trigger VNum %ld has 'if' without 'end'. (error 5)", (long) GET_TRIG_VNUM(trig));
  return c;
}

/* processes any 'wait' commands in a trigger */
static void process_wait(void *go, struct trig_data *trig, int type, char *cmd,
                         struct cmdlist_element *cl)
{
  char buf[MAX_INPUT_LENGTH], *arg;
  struct wait_event_data *wait_event_obj;
  long when = 0, hr = 0, min = 0, ntime;
  char c;

  arg = any_one_arg(cmd, buf);
  skip_spaces(&arg);

  if (!*arg) {
    script_log("Trigger: %s, VNum %ld. wait w/o an arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cl->cmd);
    return;
  }

  if (!strncmp(arg, "until ", 6)) {
    /* valid forms of time are 14:30 and 1430 */
    if (sscanf(arg, "until %ld:%ld", &hr, &min) == 2)
      min += (hr * 60);
    else
      min = (hr % 100) + ((hr / 100) * 60);

    /* the pulse of the day of the "until" time */
    ntime = (min * SECS_PER_MUD_HOUR * PASSES_PER_SEC) / 60;

    /* the pulse of the day of the current time */
    when = (dg_pulse % (SECS_PER_MUD_HOUR * PASSES_PER_SEC)) +
           (time_info.hours * SECS_PER_MUD_HOUR * PASSES_PER_SEC);

    if (when >= ntime) /* adjust for the next day */
      when = (SECS_PER_MUD_DAY * PASSES_PER_SEC) - when + ntime;
    else
      when = ntime - when;
  }

  else {
    if (sscanf(arg, "%ld %c", &when, &c) == 2) {
      if (c == 't')
        when *= (SECS_PER_MUD_HOUR * PASSES_PER_SEC);
      else if (c == 's')
        when *= PASSES_PER_SEC;
    }
  }

  wait_event_obj = new wait_event_data;
  wait_event_obj->trigger = trig;
  wait_event_obj->go = go;
  wait_event_obj->type = type;

  GET_TRIG_WAIT(trig) = event_create(trig_wait_event, wait_event_obj, when);
  trig->curr_state = cl->next;
}

/* processes a script set command */
static void process_set(struct script_data *sc, struct trig_data *trig, char *cmd)
{
  char arg[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH], *value;

  value = two_arguments(cmd, arg, name);

  skip_spaces(&value);

  if (!*name) {
    script_log("Trigger: %s, VNum %ld. set w/o an arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  add_var(&GET_TRIG_VARS(trig), name, value, sc ? sc->context : 0);
}

/* processes a script eval command */
void process_eval(void *go, struct script_data *sc, struct trig_data *trig,
                  int type, char *cmd)
{
  char arg[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH];
  char result[MAX_INPUT_LENGTH], *expr;

  expr = one_argument(cmd, arg);   /* cut off 'eval' */
  expr = one_argument(expr, name); /* cut off the name */

  skip_spaces(&expr);

  if (!*name) {
    script_log("Trigger: %s, VNum %ld. eval w/o an arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  eval_expr(expr, result, go, sc, trig, type);
  add_var(&GET_TRIG_VARS(trig), name, result, sc ? sc->context : 0);
}

/* a script attaching a trigger to something */
static void process_attach(void *go, struct script_data *sc, struct trig_data *trig,
                           int type, char *cmd)
{
  char arg[MAX_INPUT_LENGTH], trignum_s[MAX_INPUT_LENGTH];
  char result[MAX_INPUT_LENGTH], *id_p;
  struct trig_data *newtrig;
  struct char_data *c = NULL;
  struct obj_data *o = NULL;
  struct room_data *r = NULL;
  rnum_t trignum;
  long id;

  id_p = two_arguments(cmd, arg, trignum_s);
  skip_spaces(&id_p);

  if (!*trignum_s) {
    script_log("Trigger: %s, VNum %ld. attach w/o an arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  if (!id_p || !*id_p || atoi(id_p) == 0) {
    script_log("Trigger: %s, VNum %ld. attach invalid id arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  /* parse and locate the id specified */
  eval_expr(id_p, result, go, sc, trig, type);
  if (!(id = atol(result))) {
    script_log("Trigger: %s, VNum %ld. attach invalid id arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  if (!(c = find_char(id)) && !(o = find_obj(id)) && !(r = find_room(id))) {
    script_log("Trigger: %s, VNum %ld. attach invalid id arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  /* locate and load the trigger specified */
  trignum = real_trigger(atol(trignum_s));
  if (trignum < 0 || !(newtrig = read_trigger(trignum))) {
    script_log("Trigger: %s, VNum %ld. attach invalid trigger: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), trignum_s);
    return;
  }

  if (c) {
    if (!IS_NPC(c)) {
      script_log("Trigger: %s, VNum %ld. attach cannot target the player '%s'",
                 GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), GET_CHAR_NAME(c));
      extract_trigger(newtrig);
      return;
    }
    if (!SCRIPT(c))
      SCRIPT(c) = new script_data;
    add_trigger(SCRIPT(c), newtrig, -1);
    return;
  }

  if (o) {
    if (!SCRIPT(o))
      SCRIPT(o) = new script_data;
    add_trigger(SCRIPT(o), newtrig, -1);
    return;
  }

  if (r) {
    if (!SCRIPT(r))
      SCRIPT(r) = new script_data;
    add_trigger(SCRIPT(r), newtrig, -1);
    return;
  }
}

/* a script detaching a trigger from something */
static void process_detach(void *go, struct script_data *sc, struct trig_data *trig,
                           int type, char *cmd)
{
  char arg[MAX_INPUT_LENGTH], trignum_s[MAX_INPUT_LENGTH];
  char result[MAX_INPUT_LENGTH], *id_p;
  struct char_data *c = NULL;
  struct obj_data *o = NULL;
  struct room_data *r = NULL;
  long id;

  id_p = two_arguments(cmd, arg, trignum_s);
  skip_spaces(&id_p);

  if (!*trignum_s) {
    script_log("Trigger: %s, VNum %ld. detach w/o an arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  if (!id_p || !*id_p || atoi(id_p) == 0) {
    script_log("Trigger: %s, VNum %ld. detach invalid id arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  /* parse and locate the id specified */
  eval_expr(id_p, result, go, sc, trig, type);
  if (!(id = atol(result))) {
    script_log("Trigger: %s, VNum %ld. detach invalid id arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  if (!(c = find_char(id)) && !(o = find_obj(id)) && !(r = find_room(id))) {
    script_log("Trigger: %s, VNum %ld. detach invalid id arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  if (c && SCRIPT(c)) {
    if (!strcmp(trignum_s, "all")) {
      extract_script(c, MOB_TRIGGER);
      return;
    }
    if (remove_trigger(SCRIPT(c), trignum_s) && !TRIGGERS(SCRIPT(c)))
      extract_script(c, MOB_TRIGGER);
    return;
  }

  if (o && SCRIPT(o)) {
    if (!strcmp(trignum_s, "all")) {
      extract_script(o, OBJ_TRIGGER);
      return;
    }
    if (remove_trigger(SCRIPT(o), trignum_s) && !TRIGGERS(SCRIPT(o)))
      extract_script(o, OBJ_TRIGGER);
    return;
  }

  if (r && SCRIPT(r)) {
    if (!strcmp(trignum_s, "all")) {
      extract_script(r, WLD_TRIGGER);
      return;
    }
    if (remove_trigger(SCRIPT(r), trignum_s) && !TRIGGERS(SCRIPT(r)))
      extract_script(r, WLD_TRIGGER);
    return;
  }
}

struct room_data *dg_room_of_obj(struct obj_data *obj)
{
  return obj_room(obj);
}

/* create a UID variable from the id number */
static void makeuid_var(void *go, struct script_data *sc, struct trig_data *trig,
                        int type, char *cmd)
{
  char junk[MAX_INPUT_LENGTH], varname[MAX_INPUT_LENGTH];
  char arg[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH];
  char uid[MAX_INPUT_LENGTH + 1]; /* room for UID_CHAR */

  *uid = '\0';
  half_chop(cmd, junk, cmd, MAX_INPUT_LENGTH);    /* makeuid */
  half_chop(cmd, varname, cmd, MAX_INPUT_LENGTH); /* variable name */
  half_chop(cmd, arg, cmd, MAX_INPUT_LENGTH);     /* id, or 'obj'/'mob'/'room' */
  half_chop(cmd, name, cmd, MAX_INPUT_LENGTH);    /* the name, if the above was a kind */

  if (!*varname) {
    script_log("Trigger: %s, VNum %ld. makeuid w/o an arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  if (!*arg) {
    script_log("Trigger: %s, VNum %ld. makeuid invalid id arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  if (atoi(arg) != 0) { /* easy, if you pass an id number */
    char result[MAX_INPUT_LENGTH];

    eval_expr(arg, result, go, sc, trig, type);
    snprintf(uid, sizeof(uid), "%c%s", UID_CHAR, result);
  } else { /* a lot more work without it */
    if (!*name) {
      script_log("Trigger: %s, VNum %ld. makeuid needs a name: '%s'",
                 GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
      return;
    }

    if (is_abbrev(arg, "mob")) {
      struct char_data *c = NULL;
      switch (type) {
        case WLD_TRIGGER:
          c = get_char_in_room((struct room_data *) go, name);
          break;
        case OBJ_TRIGGER:
          c = get_char_near_obj((struct obj_data *) go, name);
          break;
        case MOB_TRIGGER:
          c = get_char_room_vis((struct char_data *) go, name);
          break;
      }
      if (c)
        snprintf(uid, sizeof(uid), "%c%ld", UID_CHAR, char_script_id(c));
    } else if (is_abbrev(arg, "obj")) {
      struct obj_data *o = NULL;
      switch (type) {
        case WLD_TRIGGER:
          o = get_obj_in_room((struct room_data *) go, name);
          break;
        case OBJ_TRIGGER:
          o = get_obj_near_obj((struct obj_data *) go, name);
          break;
        case MOB_TRIGGER: {
          struct char_data *goch = (struct char_data *) go;
          if (!(o = get_obj_in_list_vis(goch, name, goch->carrying)))
            o = get_obj_in_list_vis(goch, name, goch->in_room ? goch->in_room->contents : NULL);
          break;
        }
      }
      if (o)
        snprintf(uid, sizeof(uid), "%c%ld", UID_CHAR, obj_script_id(o));
    } else if (is_abbrev(arg, "room")) {
      struct room_data *r = NULL;
      switch (type) {
        case WLD_TRIGGER:
          r = (struct room_data *) go;
          break;
        case OBJ_TRIGGER:
          r = obj_room((struct obj_data *) go);
          break;
        case MOB_TRIGGER:
          r = ((struct char_data *) go)->in_room;
          break;
      }
      if (r)
        snprintf(uid, sizeof(uid), "%c%ld", UID_CHAR, room_script_id(r));
    } else {
      script_log("Trigger: %s, VNum %ld. makeuid syntax error: '%s'",
                 GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
      return;
    }
  }

  if (*uid)
    add_var(&GET_TRIG_VARS(trig), varname, uid, sc ? sc->context : 0);
}

/* Processes a script return command, yielding the script's new return value. */
static int process_return(struct trig_data *trig, char *cmd)
{
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];

  two_arguments(cmd, arg1, arg2);

  if (!*arg2) {
    script_log("Trigger: %s, VNum %ld. return w/o an arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return 1;
  }

  return atoi(arg2);
}

/* Removes a variable from sc's globals, or from trig's locals if it isn't
 * found in the global list. */
static void process_unset(struct script_data *sc, struct trig_data *trig, char *cmd)
{
  char arg[MAX_INPUT_LENGTH], *var;

  var = any_one_arg(cmd, arg);

  skip_spaces(&var);

  if (!*var) {
    script_log("Trigger: %s, VNum %ld. unset w/o an arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  if (!remove_var(&(sc->global_vars), var))
    remove_var(&GET_TRIG_VARS(trig), var);
}

/* Copy a locally owned variable to the globals of another script.
 * 'remote <variable_name> <uid>' */
static void process_remote(struct script_data *sc, struct trig_data *trig, char *cmd)
{
  struct trig_var_data *vd;
  struct script_data *sc_remote = NULL;
  char *line, *var, *uid_p;
  char arg[MAX_INPUT_LENGTH], buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];
  long uid, context;
  struct room_data *room;
  struct char_data *mob;
  struct obj_data *obj;

  line = any_one_arg(cmd, arg);
  two_arguments(line, buf, buf2);
  var = buf;
  uid_p = buf2;
  skip_spaces(&var);
  skip_spaces(&uid_p);

  if (!*buf || !*buf2) {
    script_log("Trigger: %s, VNum %ld. remote: invalid arguments '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  /* find the locally owned variable */
  for (vd = GET_TRIG_VARS(trig); vd; vd = vd->next)
    if (!str_cmp(vd->name, buf))
      break;

  if (!vd)
    for (vd = sc->global_vars; vd; vd = vd->next)
      if (!str_cmp(vd->name, var) && (vd->context == 0 || vd->context == sc->context))
        break;

  if (!vd) {
    script_log("Trigger: %s, VNum %ld. local var '%s' not found in remote call",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), buf);
    return;
  }

  uid = atol(buf2);
  if (uid <= 0) {
    script_log("Trigger: %s, VNum %ld. remote: illegal uid '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), buf2);
    return;
  }

  /* For everything but PCs, context comes from the existing context. For a
   * PC, context is 0 (global). */
  context = vd->context;

  if ((room = find_room(uid))) {
    sc_remote = SCRIPT(room);
  } else if ((mob = find_char(uid))) {
    /* A PC never has triggers attached, but it can still carry script
     * variables, which is how quest state follows a player around. These
     * live in memory only: nothing writes them to the database, so they
     * are gone the moment the player quits. A script that has to remember
     * something across a login wants a real pfile field for now. */
    if (!SCRIPT(mob))
      SCRIPT(mob) = new script_data;
    sc_remote = SCRIPT(mob);
    if (!IS_NPC(mob))
      context = 0;
  } else if ((obj = find_obj(uid))) {
    sc_remote = SCRIPT(obj);
  } else {
    script_log("Trigger: %s, VNum %ld. remote: uid '%ld' invalid",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), uid);
    return;
  }

  if (sc_remote == NULL)
    return; /* no script to assign to */

  add_var(&(sc_remote->global_vars), vd->name, vd->value, context);
}

/* Command-line interface to rdelete. Called vdelete so nobody reads it as a
 * way to delete rooms. */
ACMD(do_vdelete)
{
  struct trig_var_data *vd, *vd_prev = NULL;
  struct script_data *sc_remote = NULL;
  char *var, *uid_p;
  char buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];
  long uid;
  struct room_data *room;
  struct char_data *mob;
  struct obj_data *obj;

  two_arguments(argument, buf, buf2);
  var = buf;
  uid_p = buf2;
  skip_spaces(&var);
  skip_spaces(&uid_p);

  if (!*buf || !*buf2) {
    send_to_char("Usage: vdelete { <variablename> | * | all } <id>\r\n", ch);
    return;
  }

  uid = atol(buf2);
  if (uid <= 0) {
    send_to_char("vdelete: illegal id specified.\r\n", ch);
    return;
  }

  if ((room = find_room(uid)))
    sc_remote = SCRIPT(room);
  else if ((mob = find_char(uid)))
    sc_remote = SCRIPT(mob);
  else if ((obj = find_obj(uid)))
    sc_remote = SCRIPT(obj);
  else {
    send_to_char("vdelete: cannot resolve specified id.\r\n", ch);
    return;
  }

  if (sc_remote == NULL || sc_remote->global_vars == NULL) {
    send_to_char("That id represents no global variables.\r\n", ch);
    return;
  }

  if (*var == '*' || is_abbrev(var, "all")) {
    free_varlist(sc_remote->global_vars);
    sc_remote->global_vars = NULL;
    send_to_char("All variables deleted from that id.\r\n", ch);
    return;
  }

  for (vd = sc_remote->global_vars; vd; vd_prev = vd, vd = vd->next)
    if (!str_cmp(vd->name, var))
      break;

  if (!vd) {
    send_to_char("That variable cannot be located.\r\n", ch);
    return;
  }

  if (vd_prev)
    vd_prev->next = vd->next;
  else
    sc_remote->global_vars = vd->next;

  free_var_el(vd);

  send_to_char("Deleted.\r\n", ch);
}

/* Delete a variable from the globals of another script.
 * 'rdelete <variable_name> <uid>' */
static void process_rdelete(struct script_data *sc, struct trig_data *trig, char *cmd)
{
  struct trig_var_data *vd, *vd_prev = NULL;
  struct script_data *sc_remote = NULL;
  char *line, *var, *uid_p;
  char arg[MAX_INPUT_LENGTH], buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];
  long uid;
  struct room_data *room;
  struct char_data *mob;
  struct obj_data *obj;

  line = any_one_arg(cmd, arg);
  two_arguments(line, buf, buf2);
  var = buf;
  uid_p = buf2;
  skip_spaces(&var);
  skip_spaces(&uid_p);

  if (!*buf || !*buf2) {
    script_log("Trigger: %s, VNum %ld. rdelete: invalid arguments '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  uid = atol(buf2);
  if (uid <= 0) {
    script_log("Trigger: %s, VNum %ld. rdelete: illegal uid '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), buf2);
    return;
  }

  if ((room = find_room(uid)))
    sc_remote = SCRIPT(room);
  else if ((mob = find_char(uid)))
    sc_remote = SCRIPT(mob);
  else if ((obj = find_obj(uid)))
    sc_remote = SCRIPT(obj);
  else {
    script_log("Trigger: %s, VNum %ld. rdelete: uid '%ld' invalid",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), uid);
    return;
  }

  if (sc_remote == NULL || sc_remote->global_vars == NULL)
    return;

  for (vd = sc_remote->global_vars; vd; vd_prev = vd, vd = vd->next)
    if (!str_cmp(vd->name, var) && (vd->context == 0 || vd->context == sc->context))
      break;

  if (!vd)
    return; /* the variable doesn't exist, or is in the wrong context */

  if (vd_prev)
    vd_prev->next = vd->next;
  else
    sc_remote->global_vars = vd->next;

  free_var_el(vd);
}

/* Makes a local variable into a global variable. */
static void process_global(struct script_data *sc, struct trig_data *trig, char *cmd, long id)
{
  struct trig_var_data *vd;
  char arg[MAX_INPUT_LENGTH], *var;

  var = any_one_arg(cmd, arg);

  skip_spaces(&var);

  if (!*var) {
    script_log("Trigger: %s, VNum %ld. global w/o an arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  for (vd = GET_TRIG_VARS(trig); vd; vd = vd->next)
    if (!str_cmp(vd->name, var))
      break;

  if (!vd) {
    script_log("Trigger: %s, VNum %ld. local var '%s' not found in global call",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), var);
    return;
  }

  add_var(&(sc->global_vars), vd->name, vd->value, id);
  remove_var(&GET_TRIG_VARS(trig), vd->name);
}

/* set the current context for a script */
static void process_context(struct script_data *sc, struct trig_data *trig, char *cmd)
{
  char arg[MAX_INPUT_LENGTH], *var;

  var = any_one_arg(cmd, arg);

  skip_spaces(&var);

  if (!*var) {
    script_log("Trigger: %s, VNum %ld. context w/o an arg: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), cmd);
    return;
  }

  sc->context = atol(var);
}

static void extract_value(struct script_data *sc, struct trig_data *trig, char *cmd)
{
  char buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];
  char *buf3;
  /* Filled from buf2, which holds a token off the trigger line and is
   * MAX_INPUT_LENGTH itself, so this has to match it. */
  char to[MAX_INPUT_LENGTH];
  int num;

  buf3 = any_one_arg(cmd, buf);
  half_chop(buf3, buf2, buf, sizeof(buf));
  strlcpy(to, buf2, sizeof(to));

  num = atoi(buf);
  if (num < 1) {
    script_log("Trigger: %s, VNum %ld. extract number < 1",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig));
    return;
  }

  half_chop(buf, buf3, buf2, sizeof(buf2));

  while (num > 0) {
    half_chop(buf2, buf, buf2, sizeof(buf2));
    num--;
  }

  add_var(&GET_TRIG_VARS(trig), to, buf, sc ? sc->context : 0);
}

/* This is the core driver for scripts.
 *
 * go_adress is the address of the caller's pointer to the entity running the
 * script, so that the caller can see whether the entity was freed.
 *
 * mode is TRIG_NEW for a fresh start, or TRIG_RESTART after a 'wait'. */
int script_driver(void *go_adress, struct trig_data *trig, int type, int mode)
{
  return script_driver_default(go_adress, trig, type, mode, 1);
}

int script_driver_default(void *go_adress, struct trig_data *trig, int type, int mode,
                          int default_ret)
{
  static int depth = 0;
  int ret_val = default_ret;
  struct cmdlist_element *cl;
  char cmd[MAX_INPUT_LENGTH], *p;
  struct script_data *sc = NULL;
  struct cmdlist_element *temp;
  void *go = NULL;

  switch (type) {
    case MOB_TRIGGER:
      go = *(struct char_data **) go_adress;
      sc = SCRIPT((struct char_data *) go);
      break;
    case OBJ_TRIGGER:
      go = *(struct obj_data **) go_adress;
      sc = SCRIPT((struct obj_data *) go);
      break;
    case WLD_TRIGGER:
      go = *(struct room_data **) go_adress;
      sc = SCRIPT((struct room_data *) go);
      break;
  }

  if (!go || !sc)
    return ret_val;

  if (depth > MAX_SCRIPT_DEPTH) {
    script_log("Trigger %ld recursed beyond the maximum allowed depth.", (long) GET_TRIG_VNUM(trig));
    switch (type) {
      case MOB_TRIGGER:
        script_log("It was attached to %s [%ld]",
                   GET_CHAR_NAME((struct char_data *) go), (long) GET_MOB_VNUM((struct char_data *) go));
        break;
      case OBJ_TRIGGER:
        script_log("It was attached to %s [%ld]",
                   GET_OBJ_NAME((struct obj_data *) go), (long) GET_OBJ_VNUM((struct obj_data *) go));
        break;
      case WLD_TRIGGER:
        script_log("It was attached to %s [%ld]",
                   GET_ROOM_NAME((struct room_data *) go), (long) ((struct room_data *) go)->number);
        break;
    }

    extract_script(go, type);

    /* extract_script() is enough for a room, but on a mob or an object the
     * caller may be load_mtrigger or load_otrigger, which would just reload
     * the script onto the next one. Let the caller decide. */
    return SCRIPT_ERROR_CODE;
  }

  depth++;

  if (mode == TRIG_NEW) {
    GET_TRIG_DEPTH(trig) = 1;
    GET_TRIG_LOOPS(trig) = 0;
    sc->context = 0;
  }

  dg_owner_purged = 0;

  for (cl = (mode == TRIG_NEW) ? trig->cmdlist : trig->curr_state;
       cl && GET_TRIG_DEPTH(trig); cl = cl->next) {
    for (p = cl->cmd; *p && isspace(*p); p++)
      ;

    if (*p == '*') /* comment */
      continue;

    else if (!strncmp(p, "if ", 3)) {
      if (process_if(p + 3, go, sc, trig, type))
        GET_TRIG_DEPTH(trig)++;
      else
        cl = find_else_end(trig, cl, go, sc, type);
    }

    else if (!strncmp("elseif ", p, 7) || !strncmp("else", p, 4)) {
      /* If not in an if-block, ignore the extra 'else[if]' and warn. */
      if (GET_TRIG_DEPTH(trig) == 1) {
        script_log("Trigger VNum %ld has 'else' without 'if'.", (long) GET_TRIG_VNUM(trig));
        continue;
      }
      cl = find_end(trig, cl);
      GET_TRIG_DEPTH(trig)--;
    }

    else if (!strncmp("while ", p, 6)) {
      temp = find_done(cl);
      if (!temp) {
        script_log("Trigger VNum %ld has 'while' without 'done'.", (long) GET_TRIG_VNUM(trig));
        break;
      }
      if (process_if(p + 6, go, sc, trig, type)) {
        temp->original = cl;
      } else {
        cl->loops = 0;
        cl = temp;
      }
    }

    else if (!strncmp("switch ", p, 7)) {
      cl = find_case(trig, cl, go, sc, type, p + 7);
    }

    else if (!strncmp("end", p, 3)) {
      /* If not in an if-block, ignore the extra 'end' and warn. */
      if (GET_TRIG_DEPTH(trig) == 1) {
        script_log("Trigger VNum %ld has 'end' without 'if'.", (long) GET_TRIG_VNUM(trig));
        continue;
      }
      GET_TRIG_DEPTH(trig)--;
    }

    else if (!strncmp("done", p, 4)) {
      /* in a while loop, cl->original is non-NULL */
      if (cl->original) {
        char *orig_cmd = cl->original->cmd;
        while (*orig_cmd && isspace(*orig_cmd))
          orig_cmd++;
        if (process_if(orig_cmd + 6, go, sc, trig, type)) {
          cl = cl->original;
          cl->loops++;
          GET_TRIG_LOOPS(trig)++;
          if (cl->loops == 30) {
            cl->loops = 0;
            process_wait(go, trig, type, (char *) "wait 1", cl);
            depth--;
            return ret_val;
          }
          if (GET_TRIG_LOOPS(trig) >= 100) {
            script_log("Trigger VNum %ld has looped 100 times!", (long) GET_TRIG_VNUM(trig));
            break;
          }
        }
        /* falling out of a switch statement ends it */
      }
    }

    else if (!strncmp("break", p, 5)) {
      cl = find_done(cl);
    }

    else if (!strncmp("case", p, 4)) {
      /* Do nothing; this allows multiple cases for a single instance. */
    }

    else {
      var_subst(go, sc, trig, type, p, cmd);

      if (!strncmp(cmd, "eval ", 5))
        process_eval(go, sc, trig, type, cmd);

      else if (!strncmp(cmd, "nop ", 4))
        ; /* nop: do nothing */

      else if (!strncmp(cmd, "extract ", 8))
        extract_value(sc, trig, cmd);

      else if (!strncmp(cmd, "makeuid ", 8))
        makeuid_var(go, sc, trig, type, cmd);

      else if (!strncmp(cmd, "halt", 4))
        break;

      else if (!strncmp(cmd, "global ", 7))
        process_global(sc, trig, cmd, sc->context);

      else if (!strncmp(cmd, "context ", 8))
        process_context(sc, trig, cmd);

      else if (!strncmp(cmd, "remote ", 7))
        process_remote(sc, trig, cmd);

      else if (!strncmp(cmd, "rdelete ", 8))
        process_rdelete(sc, trig, cmd);

      else if (!strncmp(cmd, "return ", 7))
        ret_val = process_return(trig, cmd);

      else if (!strncmp(cmd, "set ", 4))
        process_set(sc, trig, cmd);

      else if (!strncmp(cmd, "unset ", 6))
        process_unset(sc, trig, cmd);

      else if (!strncmp(cmd, "wait ", 5)) {
        process_wait(go, trig, type, cmd, cl);
        depth--;
        return ret_val;
      }

      else if (!strncmp(cmd, "attach ", 7))
        process_attach(go, sc, trig, type, cmd);

      else if (!strncmp(cmd, "detach ", 7))
        process_detach(go, sc, trig, type, cmd);

      else {
        switch (type) {
          case MOB_TRIGGER:
            mob_command_interpreter((struct char_data *) go, cmd);
            break;
          case OBJ_TRIGGER:
            obj_command_interpreter((struct obj_data *) go, cmd);
            break;
          case WLD_TRIGGER:
            wld_command_interpreter((struct room_data *) go, cmd);
            break;
        }

        if (dg_owner_purged) {
          depth--;
          if (type == OBJ_TRIGGER)
            *(struct obj_data **) go_adress = NULL;
          else if (type == MOB_TRIGGER)
            *(struct char_data **) go_adress = NULL;
          return ret_val;
        }
      }
    }
  }

  /* the script may have been detached while it ran */
  switch (type) {
    case MOB_TRIGGER: sc = SCRIPT((struct char_data *) go); break;
    case OBJ_TRIGGER: sc = SCRIPT((struct obj_data *) go);  break;
    case WLD_TRIGGER: sc = SCRIPT((struct room_data *) go); break;
  }

  if (sc)
    free_varlist(GET_TRIG_VARS(trig));
  GET_TRIG_VARS(trig) = NULL;
  GET_TRIG_DEPTH(trig) = 0;

  depth--;
  return ret_val;
}

/* the real number of the trigger with the given virtual number */
rnum_t real_trigger(vnum_t vnum)
{
  rnum_t bot, top, mid;

  bot = 0;
  top = top_of_trigt - 1;

  if (!top_of_trigt || !trig_index || trig_index[bot]->vnum > vnum || trig_index[top]->vnum < vnum)
    return NOTHING;

  /* binary search on the trigger table */
  while (bot <= top) {
    mid = (bot + top) / 2;

    if (trig_index[mid]->vnum == vnum)
      return mid;
    if (trig_index[mid]->vnum > vnum)
      top = mid - 1;
    else
      bot = mid + 1;
  }

  return NOTHING;
}

ACMD(do_tstat)
{
  rnum_t rnum;
  char str[MAX_INPUT_LENGTH];

  half_chop(argument, str, argument, MAX_INPUT_LENGTH);
  if (*str) {
    rnum = real_trigger(atol(str));
    if (rnum < 0) {
      send_to_char("That vnum does not exist.\r\n", ch);
      return;
    }

    do_stat_trigger(ch, trig_index[rnum]->proto);
  } else {
    send_to_char("Usage: tstat <vnum>\r\n", ch);
  }
}

/* Scans for a case/default instance. Returns the line holding the matching
 * case, or the last line of the trigger if there isn't one. */
static struct cmdlist_element *find_case(struct trig_data *trig, struct cmdlist_element *cl,
                                         void *go, struct script_data *sc, int type, char *cond)
{
  char result[MAX_INPUT_LENGTH];
  char buf[MAX_INPUT_LENGTH];
  struct cmdlist_element *c;
  char *p;

  eval_expr(cond, result, go, sc, trig, type);

  if (!(cl->next))
    return cl;

  for (c = cl->next; c->next; c = c->next) {
    for (p = c->cmd; *p && isspace(*p); p++)
      ;

    if (!strncmp("while ", p, 6) || !strncmp("switch", p, 6)) {
      c = find_done(c);
    } else if (!strncmp("case ", p, 5)) {
      eval_op("==", result, p + 5, buf, go, sc, trig);
      if (*buf && *buf != '0')
        return c;
    } else if (!strncmp("default", p, 7)) {
      return c;
    } else if (!strncmp("done", p, 3)) {
      return c;
    }
  }

  return c;
}

/* Scans for the end of a while/switch block. Returns the line holding 'done',
 * or the last line of the trigger if there isn't one. */
static struct cmdlist_element *find_done(struct cmdlist_element *cl)
{
  struct cmdlist_element *c;
  char *p;

  if (!cl || !(cl->next))
    return cl;

  for (c = cl->next; c && c->next; c = c->next) {
    for (p = c->cmd; *p && isspace(*p); p++)
      ;

    if (!strncmp("while ", p, 6) || !strncmp("switch ", p, 7))
      c = find_done(c);
    else if (!strncmp("done", p, 3))
      return c;
  }

  return c;
}

/* ************************************************************************
*  The uid lookup table.                                                   *
************************************************************************ */

#define BUCKET_COUNT 64
/* a UID that will never be used */
#define UID_OUT_OF_RANGE 0

struct lookup_table_t {
  long uid;
  void *c;
  struct lookup_table_t *next;
};

static struct lookup_table_t lookup_table[BUCKET_COUNT];

void init_lookup_table(void)
{
  for (int i = 0; i < BUCKET_COUNT; i++) {
    lookup_table[i].uid = UID_OUT_OF_RANGE;
    lookup_table[i].c = NULL;
    lookup_table[i].next = NULL;
  }
}

static inline struct lookup_table_t *get_bucket_head(long uid)
{
  int bucket = (int) (uid & (BUCKET_COUNT - 1));
  return &lookup_table[bucket];
}

static inline struct lookup_table_t *find_element_by_uid_in_lookup_table(long uid)
{
  struct lookup_table_t *lt = get_bucket_head(uid);

  for (; lt && lt->uid != uid; lt = lt->next)
    ;

  return lt;
}

static struct char_data *find_char_by_uid_in_lookup_table(long uid)
{
  struct lookup_table_t *lt = find_element_by_uid_in_lookup_table(uid);

  if (lt)
    return (struct char_data *) (lt->c);

  return NULL;
}

static struct obj_data *find_obj_by_uid_in_lookup_table(long uid)
{
  struct lookup_table_t *lt = find_element_by_uid_in_lookup_table(uid);

  if (lt)
    return (struct obj_data *) (lt->c);

  return NULL;
}

/* Whether anything is still registered under this uid. Lets a trigger notice
 * that an object it was holding on to has been purged. */
int has_obj_by_uid_in_lookup_table(long uid)
{
  return find_element_by_uid_in_lookup_table(uid) != NULL;
}

void add_to_lookup_table(long uid, void *c)
{
  struct lookup_table_t *lt = get_bucket_head(uid);

  if (lt && lt->uid == uid) {
    lt->c = c;
    return;
  }

  for (; lt && lt->next; lt = lt->next)
    if (lt->next->uid == uid) {
      lt->next->c = c;
      return;
    }

  lt->next = new lookup_table_t;
  lt->next->uid = uid;
  lt->next->c = c;
  lt->next->next = NULL;
}

void remove_from_lookup_table(long uid)
{
  int bucket = (int) (uid & (BUCKET_COUNT - 1));
  struct lookup_table_t *lt, *flt = NULL;

  /* UID 0 is never handed out. */
  if (uid == 0)
    return;

  flt = find_element_by_uid_in_lookup_table(uid);

  if (!flt || flt == &lookup_table[bucket])
    return;

  for (lt = &lookup_table[bucket]; lt->next && lt->next != flt; lt = lt->next)
    ;

  if (lt->next == flt) {
    lt->next = flt->next;
    delete flt;
  }
}

int trig_is_attached(struct script_data *sc, vnum_t trig_num)
{
  struct trig_data *t;

  if (!sc || !TRIGGERS(sc))
    return 0;

  for (t = TRIGGERS(sc); t; t = t->next)
    if (GET_TRIG_VNUM(t) == trig_num)
      return 1;

  return 0;
}

/* Fetch this character's script id, handing one out if it doesn't have one
 * yet. Lazy assignment keeps the id space from running out quickly; the idea
 * comes from EmpireMUD. */
long char_script_id(struct char_data *ch)
{
  if (ch->script_id == 0) {
    ch->script_id = max_mob_id++;
    add_to_lookup_table(ch->script_id, (void *) ch);

    if (max_mob_id >= ROOM_ID_BASE)
      mudlog("SYSERR: Script IDs for mobiles have exceeded the limit -- reboot to fix this.",
             NULL, LOG_SYSLOG, TRUE);
  }

  return ch->script_id;
}

/* Fetch this object's script id, handing one out if it doesn't have one yet. */
long obj_script_id(struct obj_data *obj)
{
  if (obj->script_id == 0) {
    obj->script_id = max_obj_id++;
    add_to_lookup_table(obj->script_id, (void *) obj);
  }

  return obj->script_id;
}

/* ************************************************************************
*  Shared by %door% in all three command sets.                             *
*                                                                          *
*  Returns NULL on success, or a description of what went wrong for the     *
*  caller to put into its own log line -- the three callers each prefix      *
*  their log with the mob, object or room the script is running on.          *
************************************************************************ */
const char *dg_edit_door(char *argument, char *errbuf, size_t errbuf_size)
{
  char target[MAX_INPUT_LENGTH], direction[MAX_INPUT_LENGTH];
  char field[MAX_INPUT_LENGTH], *value;
  struct room_data *rm;
  struct room_direction_data *newexit;
  int dir, fd;
  rnum_t to_room;

  static const char *door_field[] = {
    "purge",
    "description",
    "flags",
    "key",
    "name",
    "room",
    "\n"
  };

  argument = two_arguments(argument, target, direction);
  value = one_argument(argument, field);
  skip_spaces(&value);

  if (!*target || !*direction || !*field)
    return "called with too few args";

  if ((rm = get_room(target)) == NULL) {
    snprintf(errbuf, errbuf_size, "invalid target (arg == %s)", target);
    return errbuf;
  }

  if ((dir = search_block(direction, dirs, FALSE)) == -1) {
    snprintf(errbuf, errbuf_size, "invalid direction (arg == %s)", direction);
    return errbuf;
  }

  if ((fd = search_block(field, door_field, FALSE)) == -1) {
    snprintf(errbuf, errbuf_size, "invalid field (arg == %s)", field);
    return errbuf;
  }

  newexit = rm->dir_option[dir];

  /* purge the exit */
  if (fd == 0) {
    if (newexit) {
      DELETE_ARRAY_IF_EXTANT(newexit->general_description);
      DELETE_ARRAY_IF_EXTANT(newexit->keyword);
      delete newexit;
      rm->dir_option[dir] = NULL;
    }
    return NULL;
  }

  if (!newexit) {
    newexit = new room_direction_data;
    rm->dir_option[dir] = newexit;
  }

  switch (fd) {
    case 1: /* description */
      DELETE_ARRAY_IF_EXTANT(newexit->general_description);
      newexit->general_description = new char[strlen(value) + 3];
      strcpy(newexit->general_description, value);
      strcat(newexit->general_description, "\r\n");
      break;
    case 2: /* flags */
      newexit->exit_info = (sh_int) atoi(value);
      break;
    case 3: /* key */
      newexit->key = atol(value);
      break;
    case 4: /* name */
      DELETE_ARRAY_IF_EXTANT(newexit->keyword);
      newexit->keyword = str_dup(value);
      break;
    case 5: /* room */
      if ((to_room = real_room(atol(value))) >= 0) {
        newexit->to_room = &world[to_room];
        newexit->to_room_vnum = atol(value);
      } else {
        newexit->to_room = NULL;
        newexit->to_room_vnum = NOWHERE;
        snprintf(errbuf, errbuf_size, "invalid door target (arg == %s)", value);
        return errbuf;
      }
      break;
  }

  return NULL;
}
