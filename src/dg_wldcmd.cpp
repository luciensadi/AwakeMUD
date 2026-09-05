/**************************************************************************
*  File: dg_wldcmd.cpp                                                    *
*  Usage: The %commands% a room trigger can run.                          *
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

#define WCMD(name)  \
   static void (name)(struct room_data *room, char *argument, int cmd, int subcmd)

WCMD(do_wasound);
WCMD(do_wecho);
WCMD(do_wsend);
WCMD(do_wzoneecho);
WCMD(do_wrecho);
WCMD(do_wdoor);
WCMD(do_wteleport);
WCMD(do_wforce);
WCMD(do_wpurge);
WCMD(do_wload);
WCMD(do_wdamage);
WCMD(do_wat);
WCMD(do_wmove);
WCMD(do_wlog);

struct wld_command_info {
  const char *command;
  void (*command_pointer)(struct room_data *room, char *argument, int cmd, int subcmd);
  int subcmd;
};

/* do_wsend */
#define SCMD_WSEND         0
#define SCMD_WECHOAROUND   1

/* attaches the room's vnum to msg and sends it to script_log */
static void wld_log(struct room_data *room, const char *format, ...)
{
  va_list args;
  char body[MAX_STRING_LENGTH];

  va_start(args, format);
  vsnprintf(body, sizeof(body), format, args);
  va_end(args);

  script_log("Room (%s, VNum %ld):: %s", GET_ROOM_NAME(room), (long) GET_ROOM_VNUM(room), body);
}

/* Resolve a script argument to a room, relative to this room. */
static struct room_data *find_room_target_room(struct room_data *room, char *rawroomstr)
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

  if ((target_mob = get_char_in_room(room, roomstr)))
    return target_mob->in_room;

  if ((target_obj = get_obj_in_room(room, roomstr)))
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

/* prints the message to everyone in the rooms around this one */
WCMD(do_wasound)
{
  int door;

  skip_spaces(&argument);

  if (!*argument) {
    wld_log(room, "wasound called with no args");
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

WCMD(do_wecho)
{
  skip_spaces(&argument);

  if (!*argument) {
    wld_log(room, "wecho called with no args");
    return;
  }

  if (room->people) {
    sub_write(argument, room->people, TRUE, TO_ROOM);
    sub_write(argument, room->people, TRUE, TO_CHAR);
  }
}

WCMD(do_wlog)
{
  skip_spaces(&argument);

  if (*argument)
    wld_log(room, "%s", argument);
}

WCMD(do_wsend)
{
  char buf[MAX_INPUT_LENGTH], *msg;
  struct char_data *ch;

  msg = any_one_arg(argument, buf);

  if (!*buf) {
    wld_log(room, "wsend called with no args");
    return;
  }

  skip_spaces(&msg);

  if (!*msg) {
    wld_log(room, "wsend called without a message");
    return;
  }

  if ((ch = get_char_by_room(room, buf))) {
    if (subcmd == SCMD_WSEND)
      sub_write(msg, ch, TRUE, TO_CHAR);
    else if (subcmd == SCMD_WECHOAROUND)
      sub_write(msg, ch, TRUE, TO_ROOM);
    return;
  }

  wld_log(room, "no target found for wsend");
}

WCMD(do_wzoneecho)
{
  struct zone_data *zone;
  char room_number[MAX_INPUT_LENGTH], msgbuf[MAX_INPUT_LENGTH], *msg;

  msg = any_one_arg(argument, room_number);
  skip_spaces(&msg);

  if (!*room_number || !*msg) {
    wld_log(room, "wzoneecho called with too few args");
    return;
  }

  if (!(zone = get_zone_from_vnum(atol(room_number)))) {
    wld_log(room, "wzoneecho called for a nonexistent zone");
    return;
  }

  snprintf(msgbuf, sizeof(msgbuf), "%s\r\n", msg);
  dg_send_to_zone(msgbuf, real_zone(zone->number));
}

/* Prints the message to everyone in a range of room vnums. */
WCMD(do_wrecho)
{
  char start[MAX_INPUT_LENGTH], finish[MAX_INPUT_LENGTH], msgbuf[MAX_INPUT_LENGTH], *msg;

  msg = two_arguments(argument, start, finish);
  skip_spaces(&msg);

  if (!*msg || !*start || !*finish || !is_number(start) || !is_number(finish)) {
    wld_log(room, "wrecho called with too few args");
    return;
  }

  snprintf(msgbuf, sizeof(msgbuf), "%s\r\n", msg);
  dg_send_to_range(atol(start), atol(finish), msgbuf);
}

WCMD(do_wdoor)
{
  char errbuf[MAX_INPUT_LENGTH];
  const char *err;

  if ((err = dg_edit_door(argument, errbuf, sizeof(errbuf))))
    wld_log(room, "wdoor: %s", err);
}

WCMD(do_wteleport)
{
  struct char_data *ch, *next_ch;
  struct room_data *target;
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];

  two_arguments(argument, arg1, arg2);

  if (!*arg1 || !*arg2) {
    wld_log(room, "wteleport called with too few args");
    return;
  }

  target = find_room_target_room(room, arg2);

  if (!target) {
    wld_log(room, "wteleport target is an invalid room");
    return;
  }

  if (!str_cmp(arg1, "all")) {
    if (target == room) {
      wld_log(room, "wteleport target is itself");
      return;
    }

    for (ch = room->people; ch; ch = next_ch) {
      next_ch = ch->next_in_room;
      if (!valid_dg_target(ch, DG_ALLOW_GODS))
        continue;
      char_from_room(ch);
      char_to_room(ch, target);
      enter_wtrigger(ch->in_room, ch, -1);
    }
    return;
  }

  if ((ch = get_char_by_room(room, arg1))) {
    if (valid_dg_target(ch, DG_ALLOW_GODS)) {
      char_from_room(ch);
      char_to_room(ch, target);
      enter_wtrigger(ch->in_room, ch, -1);
    }
    return;
  }

  wld_log(room, "wteleport: no target found");
}

WCMD(do_wforce)
{
  struct char_data *ch, *next_ch;
  char arg1[MAX_INPUT_LENGTH], *line;

  line = one_argument(argument, arg1);

  if (!*arg1 || !*line) {
    wld_log(room, "wforce called with too few args");
    return;
  }

  if (!str_cmp(arg1, "all")) {
    for (ch = room->people; ch; ch = next_ch) {
      next_ch = ch->next_in_room;
      if (valid_dg_target(ch, 0))
        command_interpreter(ch, line, GET_ROOM_NAME(room));
    }
    return;
  }

  if ((ch = get_char_by_room(room, arg1))) {
    if (valid_dg_target(ch, 0))
      command_interpreter(ch, line, GET_ROOM_NAME(room));
    return;
  }

  wld_log(room, "wforce: no target found");
}

/* purge all objects and NPCs in the room, or a named object or mob */
WCMD(do_wpurge)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *ch, *next_ch;
  struct obj_data *obj, *next_obj;

  one_argument(argument, arg);

  if (!*arg) {
    /* purge all */
    for (ch = room->people; ch; ch = next_ch) {
      next_ch = ch->next_in_room;
      if (IS_NPC(ch))
        extract_char(ch);
    }

    for (obj = room->contents; obj; obj = next_obj) {
      next_obj = obj->next_content;
      extract_obj(obj);
    }

    return;
  }

  ch = get_char_in_room(room, arg);

  if (!ch) {
    obj = get_obj_in_room(room, arg);

    if (obj)
      extract_obj(obj);
    else
      wld_log(room, "wpurge: bad argument");

    return;
  }

  if (!IS_NPC(ch)) {
    wld_log(room, "wpurge: tried to purge a PC");
    return;
  }

  extract_char(ch);
}

WCMD(do_wload)
{
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  vnum_t number = 0;
  struct char_data *mob;
  struct obj_data *object;
  char *target;
  struct char_data *tch;
  struct obj_data *cnt;
  int pos;

  target = two_arguments(argument, arg1, arg2);

  if (!*arg1 || !*arg2 || !is_number(arg2) || ((number = atol(arg2)) < 0)) {
    wld_log(room, "wload: bad syntax");
    return;
  }

  if (is_abbrev(arg1, "mob")) {
    if ((mob = read_mobile(number, VIRTUAL)) == NULL) {
      wld_log(room, "wload: bad mob vnum");
      return;
    }

    char_to_room(mob, room);

    if (SCRIPT(room)) {
      char buf[MAX_INPUT_LENGTH];
      snprintf(buf, sizeof(buf), "%c%ld", UID_CHAR, char_script_id(mob));
      add_var(&(SCRIPT(room)->global_vars), "lastloaded", buf, 0);
    }

    load_mtrigger(mob);
    return;
  }

  if (is_abbrev(arg1, "obj")) {
    if ((object = read_object(number, VIRTUAL, OBJ_LOAD_REASON_SCRIPT)) == NULL) {
      wld_log(room, "wload: bad object vnum");
      return;
    }

    if (SCRIPT(room)) {
      char buf[MAX_INPUT_LENGTH];
      snprintf(buf, sizeof(buf), "%c%ld", UID_CHAR, obj_script_id(object));
      add_var(&(SCRIPT(room)->global_vars), "lastloaded", buf, 0);
    }

    if (!target || !*target) {
      obj_to_room(object, room);
      load_otrigger(object);
      return;
    }

    two_arguments(target, arg1, arg2); /* recycling */

    tch = get_char_in_room(room, arg1);
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

    cnt = get_obj_in_room(room, arg1);
    if (cnt && GET_OBJ_TYPE(cnt) == ITEM_CONTAINER) {
      obj_to_obj(object, cnt);
      load_otrigger(object);
      return;
    }

    obj_to_room(object, room);
    load_otrigger(object);
    return;
  }

  wld_log(room, "wload: bad type");
}

WCMD(do_wdamage)
{
  char name[MAX_INPUT_LENGTH], amount[MAX_INPUT_LENGTH];
  int dam = 0;
  struct char_data *ch;

  two_arguments(argument, name, amount);

  if (!*name || !*amount) {
    wld_log(room, "wdamage: bad syntax");
    return;
  }

  dam = atoi(amount);
  ch = get_char_by_room(room, name);

  if (!ch) {
    wld_log(room, "wdamage: target not found");
    return;
  }

  script_damage(ch, dam);
}

WCMD(do_wat)
{
  struct room_data *loc = NULL;
  char arg[MAX_INPUT_LENGTH], *command;

  command = any_one_arg(argument, arg);

  if (!*arg) {
    wld_log(room, "wat called with no args");
    return;
  }

  skip_spaces(&command);

  if (!*command) {
    wld_log(room, "wat called without a command");
    return;
  }

  if (!(loc = find_room_target_room(room, arg))) {
    wld_log(room, "wat: location not found (%s)", arg);
    return;
  }

  wld_command_interpreter(loc, command);
}

WCMD(do_wmove)
{
  struct room_data *target;
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  struct obj_data *obj;

  two_arguments(argument, arg1, arg2);

  if (!*arg1 || !*arg2) {
    wld_log(room, "wmove called with too few args");
    return;
  }

  if (!(obj = get_obj_in_room(room, arg1))) {
    wld_log(room, "wmove: no such object in this room");
    return;
  }

  if (!(target = find_room_target_room(room, arg2))) {
    wld_log(room, "wmove target is an invalid room");
    return;
  }

  obj_from_room(obj);
  obj_to_room(obj, target);
}

static const struct wld_command_info wld_cmd_info[] = {
  { "wasound",     do_wasound,    0 },
  { "wat",         do_wat,        0 },
  { "wdamage",     do_wdamage,    0 },
  { "wdoor",       do_wdoor,      0 },
  { "wecho",       do_wecho,      0 },
  { "wechoaround", do_wsend,      SCMD_WECHOAROUND },
  { "wforce",      do_wforce,     0 },
  { "wload",       do_wload,      0 },
  { "wlog",        do_wlog,       0 },
  { "wmove",       do_wmove,      0 },
  { "wpurge",      do_wpurge,     0 },
  { "wrecho",      do_wrecho,     0 },
  { "wsend",       do_wsend,      SCMD_WSEND },
  { "wteleport",   do_wteleport,  0 },
  { "wzoneecho",   do_wzoneecho,  0 },
  { NULL,          NULL,          0 }
};

/* The command interpreter used by rooms, called by script_driver. */
void wld_command_interpreter(struct room_data *room, char *argument)
{
  char *line, arg[MAX_INPUT_LENGTH];

  skip_spaces(&argument);

  if (!*argument)
    return;

  line = any_one_arg(argument, arg);
  skip_spaces(&line);

  for (int cmd = 0; wld_cmd_info[cmd].command; cmd++) {
    if (!str_cmp(arg, wld_cmd_info[cmd].command)) {
      (*wld_cmd_info[cmd].command_pointer)(room, line, cmd, wld_cmd_info[cmd].subcmd);
      return;
    }
  }

  wld_log(room, "Unknown world cmd: '%s'", argument);
}
