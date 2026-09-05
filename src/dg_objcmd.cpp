/**************************************************************************
*  File: dg_objcmd.cpp                                                    *
*  Usage: The %commands% an object trigger can run.                       *
*                                                                         *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
*                                                                         *
*  $Author: Mark A. Heilpern/egreen/Welcor $                              *
*  $Date: 2004/10/11 12:07:00$                                            *
*  $Revision: 1.0.14 $                                                    *
*                                                                         *
*  Ported to AwakeMUD CE by Fizban.                                       *
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
#include "memory.hpp"
#include "constants.hpp"
#include "dg_scripts.hpp"
#include "dg_event.hpp"

#define OCMD(name)  \
   static void (name)(struct obj_data *obj, char *argument, int cmd, int subcmd)

OCMD(do_oecho);
OCMD(do_oforce);
OCMD(do_ozoneecho);
OCMD(do_osend);
OCMD(do_orecho);
OCMD(do_otimer);
OCMD(do_otransform);
OCMD(do_opurge);
OCMD(do_oteleport);
OCMD(do_dgoload);
OCMD(do_odamage);
OCMD(do_oasound);
OCMD(do_odoor);
OCMD(do_osetval);
OCMD(do_oat);
OCMD(do_omove);
OCMD(do_olog);

struct obj_command_info {
  const char *command;
  void (*command_pointer)(struct obj_data *obj, char *argument, int cmd, int subcmd);
  int subcmd;
};

/* do_osend */
#define SCMD_OSEND         0
#define SCMD_OECHOAROUND   1

/* attaches the object's name and vnum to msg and sends it to script_log */
static void obj_log(struct obj_data *obj, const char *format, ...)
{
  va_list args;
  char body[MAX_STRING_LENGTH];

  va_start(args, format);
  vsnprintf(body, sizeof(body), format, args);
  va_end(args);

  script_log("Obj (%s, VNum %ld):: %s", GET_OBJ_NAME(obj), (long) GET_OBJ_VNUM(obj), body);
}

/* the room the object, or whoever is carrying it, is in */
struct room_data *obj_room(struct obj_data *obj)
{
  if (!obj)
    return NULL;

  if (obj->in_room)
    return obj->in_room;
  if (obj->carried_by)
    return obj->carried_by->in_room;
  if (obj->worn_by)
    return obj->worn_by->in_room;
  if (obj->in_obj)
    return obj_room(obj->in_obj);

  return NULL;
}

/* Resolve a script argument to a room, relative to this object. */
static struct room_data *find_obj_target_room(struct obj_data *obj, char *rawroomstr)
{
  rnum_t location;
  struct char_data *target_mob;
  struct obj_data *target_obj;
  char roomstr[MAX_INPUT_LENGTH];

  one_argument(rawroomstr, roomstr);

  if (!*roomstr)
    return NULL;

  if (*roomstr == UID_CHAR)
    return find_room(atol(roomstr + 1));

  if (isdigit(*roomstr) && !strchr(roomstr, '.')) {
    if ((location = real_room(atol(roomstr))) < 0)
      return NULL;
    return &world[location];
  }

  if ((target_mob = get_char_by_obj(obj, roomstr)))
    return target_mob->in_room;

  if ((target_obj = get_obj_by_obj(obj, roomstr)))
    return obj_room(target_obj);

  return NULL;
}

/* Send a message to every awake player in rooms numbered start..finish. */
static void dg_send_to_range(vnum_t start, vnum_t finish, const char *messg)
{
  struct descriptor_data *i;

  if (!messg || !*messg)
    return;

  for (i = descriptor_list; i; i = i->next)
    if (!i->connected && i->character && AWAKE(i->character) && i->character->in_room &&
        GET_ROOM_VNUM(i->character->in_room) >= start &&
        GET_ROOM_VNUM(i->character->in_room) <= finish)
      send_to_char(messg, i->character);
}

OCMD(do_oecho)
{
  struct room_data *room;

  skip_spaces(&argument);

  if (!*argument) {
    obj_log(obj, "oecho called with no args");
    return;
  }

  if (!(room = obj_room(obj))) {
    obj_log(obj, "oecho called by an object that is nowhere");
    return;
  }

  if (room->people) {
    sub_write(argument, room->people, TRUE, TO_ROOM);
    sub_write(argument, room->people, TRUE, TO_CHAR);
  }
}

OCMD(do_olog)
{
  skip_spaces(&argument);

  if (*argument)
    obj_log(obj, "%s", argument);
}

OCMD(do_oforce)
{
  struct char_data *ch, *next_ch;
  struct room_data *room;
  char arg1[MAX_INPUT_LENGTH], *line;

  line = one_argument(argument, arg1);

  if (!*arg1 || !*line) {
    obj_log(obj, "oforce called with too few args");
    return;
  }

  if (!str_cmp(arg1, "all")) {
    if (!(room = obj_room(obj))) {
      obj_log(obj, "oforce called by an object that is nowhere");
      return;
    }

    for (ch = room->people; ch; ch = next_ch) {
      next_ch = ch->next_in_room;
      if (valid_dg_target(ch, 0))
        command_interpreter(ch, line, GET_OBJ_NAME(obj));
    }
    return;
  }

  if ((ch = get_char_by_obj(obj, arg1))) {
    if (valid_dg_target(ch, 0))
      command_interpreter(ch, line, GET_OBJ_NAME(obj));
    return;
  }

  obj_log(obj, "oforce: no target found");
}

OCMD(do_ozoneecho)
{
  struct zone_data *zone;
  char room_number[MAX_INPUT_LENGTH], msgbuf[MAX_INPUT_LENGTH], *msg;

  msg = any_one_arg(argument, room_number);
  skip_spaces(&msg);

  if (!*room_number || !*msg) {
    obj_log(obj, "ozoneecho called with too few args");
    return;
  }

  if (!(zone = get_zone_from_vnum(atol(room_number)))) {
    obj_log(obj, "ozoneecho called for a nonexistent zone");
    return;
  }

  snprintf(msgbuf, sizeof(msgbuf), "%s\r\n", msg);
  dg_send_to_zone(msgbuf, real_zone(zone->number));
}

OCMD(do_osend)
{
  char buf[MAX_INPUT_LENGTH], *msg;
  struct char_data *ch;

  msg = any_one_arg(argument, buf);

  if (!*buf) {
    obj_log(obj, "osend called with no args");
    return;
  }

  skip_spaces(&msg);

  if (!*msg) {
    obj_log(obj, "osend called without a message");
    return;
  }

  if ((ch = get_char_by_obj(obj, buf))) {
    if (subcmd == SCMD_OSEND)
      sub_write(msg, ch, TRUE, TO_CHAR);
    else if (subcmd == SCMD_OECHOAROUND)
      sub_write(msg, ch, TRUE, TO_ROOM);
    return;
  }

  obj_log(obj, "no target found for osend");
}

/* Prints the message to everyone in a range of room vnums. */
OCMD(do_orecho)
{
  char start[MAX_INPUT_LENGTH], finish[MAX_INPUT_LENGTH], msgbuf[MAX_INPUT_LENGTH], *msg;

  msg = two_arguments(argument, start, finish);
  skip_spaces(&msg);

  if (!*msg || !*start || !*finish || !is_number(start) || !is_number(finish)) {
    obj_log(obj, "orecho: too few args");
    return;
  }

  snprintf(msgbuf, sizeof(msgbuf), "%s\r\n", msg);
  dg_send_to_range(atol(start), atol(finish), msgbuf);
}

/* set the object's timer value */
OCMD(do_otimer)
{
  char arg[MAX_INPUT_LENGTH];

  one_argument(argument, arg);

  if (!*arg)
    obj_log(obj, "otimer: missing argument");
  else if (!isdigit(*arg) && *arg != '-')
    obj_log(obj, "otimer: bad argument");
  else
    GET_OBJ_TIMER(obj) = atoi(arg);
}

/* Transform into a different object. Don't use this on containers unless
 * both objects are containers. */
OCMD(do_otransform)
{
  char arg[MAX_INPUT_LENGTH];
  struct obj_data *o;
  struct char_data *wearer = NULL;
  rnum_t new_rnum;
  int pos = 0;

  one_argument(argument, arg);

  if (!*arg) {
    obj_log(obj, "otransform: missing argument");
    return;
  }

  if (!isdigit(*arg)) {
    obj_log(obj, "otransform: bad argument");
    return;
  }

  if ((new_rnum = real_object(atol(arg))) < 0) {
    obj_log(obj, "otransform: bad object vnum");
    return;
  }

  o = read_object(new_rnum, REAL, OBJ_LOAD_REASON_SCRIPT);
  if (o == NULL) {
    obj_log(obj, "otransform: could not read object");
    return;
  }

  if (obj->worn_by) {
    pos = obj->worn_on;
    wearer = obj->worn_by;
    unequip_char(wearer, pos, TRUE);
  }

  /* Adopt the new object's identity, keeping everything that ties this
   * object to the world. */
  {
    struct room_data *in_room = obj->in_room;
    struct veh_data *in_veh = obj->in_veh;
    struct char_data *carried_by = obj->carried_by;
    struct char_data *worn_by = obj->worn_by;
    sh_int worn_on = obj->worn_on;
    struct obj_data *in_obj = obj->in_obj;
    struct obj_data *contains = obj->contains;
    struct obj_data *next_content = obj->next_content;
    struct host_data *in_host = obj->in_host;
    long script_id = obj->script_id;
    struct trig_proto_list *proto_script = obj->proto_script;
    struct script_data *script = obj->script;
    unsigned long idnum = obj->idnum;

    *obj = *o;

    obj->in_room = in_room;
    obj->in_veh = in_veh;
    obj->carried_by = carried_by;
    obj->worn_by = worn_by;
    obj->worn_on = worn_on;
    obj->in_obj = in_obj;
    obj->contains = contains;
    obj->next_content = next_content;
    obj->in_host = in_host;
    obj->script_id = script_id;
    obj->proto_script = proto_script;
    obj->script = script;
    obj->idnum = idnum;
    obj->item_number = new_rnum;
  }

  if (wearer)
    equip_char(wearer, obj, pos);

  /* o's pointers now live on obj, so blank them before freeing the shell. */
  o->contains = NULL;
  o->in_obj = NULL;
  o->next_content = NULL;
  o->carried_by = NULL;
  o->worn_by = NULL;
  o->in_room = NULL;
  o->in_veh = NULL;
  o->in_host = NULL;
  o->proto_script = NULL;
  o->script = NULL;
  extract_obj(o);
}

/* purge all objects and NPCs in the room, or a named object or mob */
OCMD(do_opurge)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *ch, *next_ch;
  struct obj_data *o, *next_obj;
  struct room_data *rm;

  one_argument(argument, arg);

  if (!*arg) {
    /* purge all */
    if ((rm = obj_room(obj))) {
      for (ch = rm->people; ch; ch = next_ch) {
        next_ch = ch->next_in_room;
        if (IS_NPC(ch))
          extract_char(ch);
      }

      for (o = rm->contents; o; o = next_obj) {
        next_obj = o->next_content;
        if (o != obj)
          extract_obj(o);
      }
    }

    return;
  }

  ch = get_char_by_obj(obj, arg);
  if (!ch) {
    o = get_obj_by_obj(obj, arg);
    if (o) {
      if (o == obj) {
        /* Purging the object whose script is running would free it out from
         * under script_driver, so defer it. */
        dg_owner_purged = 1;
        dg_note_obj_extraction(o);
      } else {
        extract_obj(o);
      }
    } else {
      obj_log(obj, "opurge: bad argument");
    }

    return;
  }

  if (!IS_NPC(ch)) {
    obj_log(obj, "opurge: tried to purge a PC");
    return;
  }

  extract_char(ch);
}

OCMD(do_oteleport)
{
  struct char_data *ch, *next_ch;
  struct room_data *target, *rm;
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];

  two_arguments(argument, arg1, arg2);

  if (!*arg1 || !*arg2) {
    obj_log(obj, "oteleport called with too few args");
    return;
  }

  target = find_obj_target_room(obj, arg2);

  if (!target) {
    obj_log(obj, "oteleport target is an invalid room");
    return;
  }

  if (!str_cmp(arg1, "all")) {
    rm = obj_room(obj);

    if (!rm)
      return;

    if (target == rm) {
      obj_log(obj, "oteleport target is itself");
      return;
    }

    for (ch = rm->people; ch; ch = next_ch) {
      next_ch = ch->next_in_room;
      if (!valid_dg_target(ch, DG_ALLOW_GODS))
        continue;
      char_from_room(ch);
      char_to_room(ch, target);
      enter_wtrigger(ch->in_room, ch, -1);
    }
    return;
  }

  if ((ch = get_char_by_obj(obj, arg1))) {
    if (valid_dg_target(ch, DG_ALLOW_GODS)) {
      char_from_room(ch);
      char_to_room(ch, target);
      enter_wtrigger(ch->in_room, ch, -1);
    }
    return;
  }

  obj_log(obj, "oteleport: no target found");
}

OCMD(do_dgoload)
{
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  vnum_t number = 0;
  struct room_data *room;
  struct char_data *mob;
  struct obj_data *object;
  char *target;
  struct char_data *tch;
  struct obj_data *cnt;
  int pos;

  target = two_arguments(argument, arg1, arg2);

  if (!*arg1 || !*arg2 || !is_number(arg2) || ((number = atol(arg2)) < 0)) {
    obj_log(obj, "oload: bad syntax");
    return;
  }

  if (!(room = obj_room(obj))) {
    obj_log(obj, "oload: object is nowhere, and so has nowhere to load into");
    return;
  }

  if (is_abbrev(arg1, "mob")) {
    struct room_data *dest = room;

    if (target && *target) {
      rnum_t rnum;
      if (!isdigit(*target) || (rnum = real_room(atol(target))) < 0) {
        obj_log(obj, "oload: room target vnum doesn't exist (loading mob vnum %ld to room %s)",
                (long) number, target);
        return;
      }
      dest = &world[rnum];
    }

    if ((mob = read_mobile(number, VIRTUAL)) == NULL) {
      obj_log(obj, "oload: bad mob vnum");
      return;
    }

    char_to_room(mob, dest);

    if (SCRIPT(obj)) {
      char buf[MAX_INPUT_LENGTH];
      snprintf(buf, sizeof(buf), "%c%ld", UID_CHAR, char_script_id(mob));
      add_var(&(SCRIPT(obj)->global_vars), "lastloaded", buf, 0);
    }

    load_mtrigger(mob);
    return;
  }

  if (is_abbrev(arg1, "obj")) {
    if ((object = read_object(number, VIRTUAL, OBJ_LOAD_REASON_SCRIPT)) == NULL) {
      obj_log(obj, "oload: bad object vnum");
      return;
    }

    if (SCRIPT(obj)) {
      char buf[MAX_INPUT_LENGTH];
      snprintf(buf, sizeof(buf), "%c%ld", UID_CHAR, obj_script_id(object));
      add_var(&(SCRIPT(obj)->global_vars), "lastloaded", buf, 0);
    }

    if (!target || !*target) {
      obj_to_room(object, room);
      load_otrigger(object);
      return;
    }

    two_arguments(target, arg1, arg2); /* recycling */

    tch = get_char_near_obj(obj, arg1);
    if (tch) {
      if (*arg2 && (pos = find_eq_pos_script(arg2)) >= 0 && !GET_EQ(tch, pos)) {
        equip_char(tch, object, pos);
        load_otrigger(object);
        return;
      }
      obj_to_char(object, tch);
      load_otrigger(object);
      return;
    }

    cnt = get_obj_near_obj(obj, arg1);
    if (cnt && GET_OBJ_TYPE(cnt) == ITEM_CONTAINER) {
      obj_to_obj(object, cnt);
      load_otrigger(object);
      return;
    }

    obj_to_room(object, room);
    load_otrigger(object);
    return;
  }

  obj_log(obj, "oload: bad type");
}

OCMD(do_odamage)
{
  char name[MAX_INPUT_LENGTH], amount[MAX_INPUT_LENGTH];
  int dam = 0;
  struct char_data *ch;

  two_arguments(argument, name, amount);

  if (!*name || !*amount) {
    obj_log(obj, "odamage: bad syntax");
    return;
  }

  dam = atoi(amount);
  ch = get_char_by_obj(obj, name);

  if (!ch) {
    obj_log(obj, "odamage: target not found");
    return;
  }

  script_damage(ch, dam);
}

OCMD(do_oasound)
{
  struct room_data *room;
  int door;

  skip_spaces(&argument);

  if (!*argument) {
    obj_log(obj, "oasound called with no args");
    return;
  }

  if (!(room = obj_room(obj))) {
    obj_log(obj, "oasound called by an object that is nowhere");
    return;
  }

  for (door = 0; door < NUM_OF_DIRS; door++) {
    struct room_direction_data *newexit = room->dir_option[door];

    if (newexit && newexit->to_room && newexit->to_room != room && newexit->to_room->people) {
      sub_write(argument, newexit->to_room->people, TRUE, TO_ROOM);
      sub_write(argument, newexit->to_room->people, TRUE, TO_CHAR);
    }
  }
}

OCMD(do_odoor)
{
  char errbuf[MAX_INPUT_LENGTH];
  const char *err;

  if ((err = dg_edit_door(argument, errbuf, sizeof(errbuf))))
    obj_log(obj, "odoor: %s", err);
}

OCMD(do_osetval)
{
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  int position, new_value, worn_on;
  struct char_data *worn_by = NULL;

  two_arguments(argument, arg1, arg2);

  if (!*arg1 || !*arg2 || !is_number(arg1) || !is_number(arg2)) {
    obj_log(obj, "osetval: bad syntax");
    return;
  }

  position = atoi(arg1);
  new_value = atoi(arg2);

  if (position < 0 || position >= NUM_OBJ_VALUES) {
    obj_log(obj, "osetval: position out of bounds");
    return;
  }

  /* Take it off first: several values feed the wearer's totals, and those
   * are recomputed on equip. */
  worn_by = obj->worn_by;
  worn_on = obj->worn_on;

  if (worn_by != NULL)
    unequip_char(worn_by, worn_on, TRUE);

  GET_OBJ_VAL(obj, position) = new_value;

  if (worn_by != NULL)
    equip_char(worn_by, obj, worn_on);
}

OCMD(do_oat)
{
  struct room_data *loc = NULL;
  struct char_data *ch;
  struct obj_data *object;
  char arg[MAX_INPUT_LENGTH], *command;

  command = any_one_arg(argument, arg);

  if (!*arg) {
    obj_log(obj, "oat called with no args");
    return;
  }

  skip_spaces(&command);

  if (!*command) {
    obj_log(obj, "oat called without a command");
    return;
  }

  if (isdigit(*arg)) {
    rnum_t rnum = real_room(atol(arg));
    if (rnum >= 0)
      loc = &world[rnum];
  } else if ((ch = get_char_by_obj(obj, arg))) {
    loc = ch->in_room;
  }

  if (!loc) {
    obj_log(obj, "oat: location not found (%s)", arg);
    return;
  }

  if (!(object = read_object(GET_OBJ_VNUM(obj), VIRTUAL, OBJ_LOAD_REASON_SCRIPT)))
    return;

  obj_to_room(object, loc);
  obj_command_interpreter(object, command);

  if (object->in_room == loc)
    extract_obj(object);
}

OCMD(do_omove)
{
  struct room_data *target;
  char arg1[MAX_INPUT_LENGTH];

  one_argument(argument, arg1);

  if (!*arg1) {
    obj_log(obj, "omove called with too few args");
    return;
  }

  target = find_obj_target_room(obj, arg1);

  if (!target) {
    obj_log(obj, "omove target is an invalid room");
    return;
  }

  /* Take the object out of wherever it currently is. */
  if (obj->carried_by != NULL)
    obj_from_char(obj);
  else if (obj->in_room != NULL)
    obj_from_room(obj);
  else if (obj->in_obj != NULL)
    obj_from_obj(obj);
  else {
    obj_log(obj, "omove: target object is not in a room, held, or in a container");
    return;
  }

  obj_to_room(obj, target);
}

static const struct obj_command_info obj_cmd_info[] = {
  { "oasound",     do_oasound,    0 },
  { "oat",         do_oat,        0 },
  { "odamage",     do_odamage,    0 },
  { "odoor",       do_odoor,      0 },
  { "oecho",       do_oecho,      0 },
  { "oechoaround", do_osend,      SCMD_OECHOAROUND },
  { "oforce",      do_oforce,     0 },
  { "oload",       do_dgoload,    0 },
  { "olog",        do_olog,       0 },
  { "omove",       do_omove,      0 },
  { "opurge",      do_opurge,     0 },
  { "orecho",      do_orecho,     0 },
  { "osend",       do_osend,      SCMD_OSEND },
  { "osetval",     do_osetval,    0 },
  { "oteleport",   do_oteleport,  0 },
  { "otimer",      do_otimer,     0 },
  { "otransform",  do_otransform, 0 },
  { "ozoneecho",   do_ozoneecho,  0 },
  { NULL,          NULL,          0 }
};

/* The command interpreter used by objects, called by script_driver. */
void obj_command_interpreter(struct obj_data *obj, char *argument)
{
  char *line, arg[MAX_INPUT_LENGTH];

  skip_spaces(&argument);

  if (!*argument)
    return;

  line = any_one_arg(argument, arg);
  skip_spaces(&line);

  for (int cmd = 0; obj_cmd_info[cmd].command; cmd++) {
    if (!str_cmp(arg, obj_cmd_info[cmd].command)) {
      (*obj_cmd_info[cmd].command_pointer)(obj, line, cmd, obj_cmd_info[cmd].subcmd);
      return;
    }
  }

  obj_log(obj, "Unknown object cmd: '%s'", argument);
}
