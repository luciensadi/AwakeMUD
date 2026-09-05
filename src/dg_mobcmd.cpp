/**************************************************************************
*  File: dg_mobcmd.cpp                                                    *
*  Usage: The %commands% a mob trigger can run.                           *
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
*  These commands live in their own table rather than in cmd_info, so a   *
*  player who types 'mpurge' gets the usual "Huh?" and nothing else.      *
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

extern class objList ObjList;

/* attaches the mob's name and vnum to msg and sends it to script_log */
static void mob_log(struct char_data *mob, const char *format, ...)
{
  va_list args;
  char output[MAX_STRING_LENGTH];
  char body[MAX_STRING_LENGTH];

  va_start(args, format);
  vsnprintf(body, sizeof(body), format, args);
  va_end(args);

  snprintf(output, sizeof(output), "Mob (%s, VNum %ld):: %s",
           GET_CHAR_NAME(mob), (long) GET_MOB_VNUM(mob), body);

  script_log("%s", output);
}

/* Is this mob allowed to run script commands? A mob with a live script is,
 * and so is one a president-level staffer has switched into. */
#define MOB_OR_IMPL(ch) \
 ((IS_NPC(ch) && (!(ch)->desc || ((ch)->desc->original && access_level((ch)->desc->original, LVL_PRESIDENT)))) || \
  (SCRIPT(ch) && TRIGGERS(SCRIPT(ch))))

/* Stock DG refuses these commands to a charmed mob, so a player cannot drive
 * one through its own script commands. Awake has no charm affect; the mobs a
 * player directs are conjured spirits and elementals. */
#define MOB_IS_PLAYER_DIRECTED(ch) (IS_SPIRIT(ch) || IS_PC_CONJURED_ELEMENTAL(ch))

/* The guard every one of these commands opens with. */
#define MOBCMD_PREAMBLE(ch)                     \
  if (!MOB_OR_IMPL(ch)) {                       \
    send_to_char("Huh?!?\r\n", (ch));           \
    return;                                     \
  }                                             \
  if (MOB_IS_PLAYER_DIRECTED(ch))               \
    return;

/* Resolve a script argument to a room: a uid, or a room vnum. */
static struct room_data *dg_find_target_room(struct char_data *ch, char *rawroomstr)
{
  rnum_t location;

  skip_spaces(&rawroomstr);

  if (!*rawroomstr)
    return NULL;

  if (*rawroomstr == UID_CHAR)
    return find_room(atol(rawroomstr + 1));

  if (!isdigit(*rawroomstr))
    return NULL;

  if ((location = real_room(atol(rawroomstr))) < 0)
    return NULL;

  return &world[location];
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

/* prints the argument to all the rooms around the mobile */
ACMD(do_masound)
{
  struct room_data *was_in_room;
  int door;

  MOBCMD_PREAMBLE(ch);

  if (!*argument) {
    mob_log(ch, "masound called with no argument");
    return;
  }

  skip_spaces(&argument);

  if (!(was_in_room = ch->in_room))
    return;

  for (door = 0; door < NUM_OF_DIRS; door++) {
    struct room_direction_data *newexit;

    if (((newexit = was_in_room->dir_option[door]) != NULL) &&
        newexit->to_room && newexit->to_room != was_in_room) {
      ch->in_room = newexit->to_room;
      sub_write(argument, ch, TRUE, TO_ROOM);
    }
  }

  ch->in_room = was_in_room;
}

/* lets the mobile attack any player or mobile */
ACMD(do_mkill)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *victim;

  MOBCMD_PREAMBLE(ch);

  one_argument(argument, arg);

  if (!*arg) {
    mob_log(ch, "mkill called with no argument");
    return;
  }

  if (*arg == UID_CHAR) {
    if (!(victim = get_char(arg))) {
      mob_log(ch, "mkill: victim (%s) not found", arg);
      return;
    }
  } else if (!(victim = get_char_room_vis(ch, arg))) {
    mob_log(ch, "mkill: victim (%s) not found", arg);
    return;
  }

  if (victim == ch) {
    mob_log(ch, "mkill: victim is self");
    return;
  }

  if (!IS_NPC(victim) && PRF_FLAGGED(victim, PRF_NOHASSLE)) {
    mob_log(ch, "mkill: target has nohassle on");
    return;
  }

  if (FIGHTING(ch)) {
    mob_log(ch, "mkill: already fighting");
    return;
  }

  set_fighting(ch, victim);
}

/* Lets the mobile destroy an object in its inventory or worn, including
 * all.xxxx and plain all. */
ACMD(do_mjunk)
{
  char arg[MAX_INPUT_LENGTH];
  int pos;
  bool junk_all = FALSE;
  struct obj_data *obj, *obj_next;

  MOBCMD_PREAMBLE(ch);

  one_argument(argument, arg);

  if (!*arg) {
    mob_log(ch, "mjunk called with no argument");
    return;
  }

  if (!str_cmp(arg, "all"))
    junk_all = TRUE;

  if ((find_all_dots(arg, sizeof(arg)) != FIND_INDIV) && !junk_all) {
    if (get_object_in_equip_vis(ch, arg, ch->equipment, &pos)) {
      extract_obj(unequip_char(ch, pos, TRUE));
      return;
    }
    if ((obj = get_obj_in_list_vis(ch, arg, ch->carrying)) != NULL)
      extract_obj(obj);
    return;
  }

  for (obj = ch->carrying; obj != NULL; obj = obj_next) {
    obj_next = obj->next_content;
    if (junk_all || isname(arg + 4, GET_OBJ_KEYWORDS(obj)))
      extract_obj(obj);
  }

  for (pos = 0; pos < NUM_WEARS; pos++) {
    if (!GET_EQ(ch, pos))
      continue;
    if (junk_all || isname(arg + 4, GET_OBJ_KEYWORDS(GET_EQ(ch, pos))))
      extract_obj(unequip_char(ch, pos, TRUE));
  }
}

/* prints the message to everyone in the room other than the mob and victim */
ACMD(do_mechoaround)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *victim;
  char *p;

  MOBCMD_PREAMBLE(ch);

  p = one_argument(argument, arg);
  skip_spaces(&p);

  if (!*arg) {
    mob_log(ch, "mechoaround called with no argument");
    return;
  }

  if (*arg == UID_CHAR) {
    if (!(victim = get_char(arg))) {
      mob_log(ch, "mechoaround: victim (%s) does not exist", arg);
      return;
    }
  } else if (!(victim = get_char_room_vis(ch, arg))) {
    mob_log(ch, "mechoaround: victim (%s) does not exist", arg);
    return;
  }

  sub_write(p, victim, TRUE, TO_ROOM);
}

/* sends the message to only the victim */
ACMD(do_msend)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *victim;
  char *p;

  MOBCMD_PREAMBLE(ch);

  p = one_argument(argument, arg);
  skip_spaces(&p);

  if (!*arg) {
    mob_log(ch, "msend called with no argument");
    return;
  }

  if (*arg == UID_CHAR) {
    if (!(victim = get_char(arg))) {
      mob_log(ch, "msend: victim (%s) does not exist", arg);
      return;
    }
  } else if (!(victim = get_char_room_vis(ch, arg))) {
    mob_log(ch, "msend: victim (%s) does not exist", arg);
    return;
  }

  sub_write(p, victim, TRUE, TO_CHAR);
}

/* prints the message to the room at large */
ACMD(do_mecho)
{
  char *p;

  MOBCMD_PREAMBLE(ch);

  if (!*argument) {
    mob_log(ch, "mecho called with no arguments");
    return;
  }

  p = argument;
  skip_spaces(&p);

  sub_write(p, ch, TRUE, TO_ROOM);
  sub_write(p, ch, TRUE, TO_CHAR);
}

/* writes a line to the zone log, for builders debugging their own scripts */
ACMD(do_mlog)
{
  char *p;

  MOBCMD_PREAMBLE(ch);

  if (!*argument)
    return;

  p = argument;
  skip_spaces(&p);

  mob_log(ch, "%s", p);
}

ACMD(do_mzoneecho)
{
  struct zone_data *zone;
  char room_number[MAX_INPUT_LENGTH], msgbuf[MAX_INPUT_LENGTH], *msg;

  MOBCMD_PREAMBLE(ch);

  msg = any_one_arg(argument, room_number);
  skip_spaces(&msg);

  if (!*room_number || !*msg) {
    mob_log(ch, "mzoneecho called with too few args");
    return;
  }

  if (!(zone = get_zone_from_vnum(atol(room_number)))) {
    mob_log(ch, "mzoneecho called for a nonexistent zone");
    return;
  }

  snprintf(msgbuf, sizeof(msgbuf), "%s\r\n", msg);
  dg_send_to_zone(msgbuf, real_zone(zone->number));
}

/* Prints the message to everyone in a range of room vnums. */
ACMD(do_mrecho)
{
  char start[MAX_INPUT_LENGTH], finish[MAX_INPUT_LENGTH], msgbuf[MAX_INPUT_LENGTH], *msg;

  MOBCMD_PREAMBLE(ch);

  msg = two_arguments(argument, start, finish);
  skip_spaces(&msg);

  if (!*msg || !*start || !*finish || !is_number(start) || !is_number(finish)) {
    mob_log(ch, "mrecho called with too few args");
    return;
  }

  snprintf(msgbuf, sizeof(msgbuf), "%s\r\n", msg);
  dg_send_to_range(atol(start), atol(finish), msgbuf);
}

/* Lets the mobile load an item or a mobile. Items land in inventory unless
 * they can't be taken, or unless a target is named. */
ACMD(do_mload)
{
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  vnum_t number = 0;
  struct char_data *mob;
  struct obj_data *object;
  char *target;
  struct char_data *tch;
  struct obj_data *cnt;
  int pos;

  MOBCMD_PREAMBLE(ch);

  if (ch->desc)
    return;

  target = two_arguments(argument, arg1, arg2);

  if (!*arg1 || !*arg2 || !is_number(arg2) || ((number = atol(arg2)) < 0)) {
    mob_log(ch, "mload: bad syntax");
    return;
  }

  if (is_abbrev(arg1, "mob")) {
    struct room_data *room;

    if (!target || !*target) {
      room = ch->in_room;
    } else if (!(room = dg_find_target_room(ch, target))) {
      mob_log(ch, "mload: room target vnum doesn't exist (loading mob vnum %ld to room %s)",
              (long) number, target);
      return;
    }

    if (!room) {
      mob_log(ch, "mload: no room to load the mob into");
      return;
    }

    if ((mob = read_mobile(number, VIRTUAL)) == NULL) {
      mob_log(ch, "mload: bad mob vnum");
      return;
    }

    char_to_room(mob, room);

    if (SCRIPT(ch)) { /* it should have one, but it might have been detached */
      char buf[MAX_INPUT_LENGTH];
      snprintf(buf, sizeof(buf), "%c%ld", UID_CHAR, char_script_id(mob));
      add_var(&(SCRIPT(ch)->global_vars), "lastloaded", buf, 0);
    }

    load_mtrigger(mob);
    return;
  }

  if (is_abbrev(arg1, "obj")) {
    if ((object = read_object(number, VIRTUAL, OBJ_LOAD_REASON_SCRIPT)) == NULL) {
      mob_log(ch, "mload: bad object vnum");
      return;
    }

    if (SCRIPT(ch)) {
      char buf[MAX_INPUT_LENGTH];
      snprintf(buf, sizeof(buf), "%c%ld", UID_CHAR, obj_script_id(object));
      add_var(&(SCRIPT(ch)->global_vars), "lastloaded", buf, 0);
    }

    if (!target || !*target) {
      if (CAN_WEAR(object, ITEM_WEAR_TAKE))
        obj_to_char(object, ch);
      else if (ch->in_room)
        obj_to_room(object, ch->in_room);
      else
        obj_to_char(object, ch);
      load_otrigger(object);
      return;
    }

    two_arguments(target, arg1, arg2); /* recycling */

    tch = (*arg1 == UID_CHAR) ? get_char(arg1) : get_char_room_vis(ch, arg1);
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

    cnt = (*arg1 == UID_CHAR) ? get_obj(arg1) : get_obj_vis(ch, arg1);
    if (cnt && GET_OBJ_TYPE(cnt) == ITEM_CONTAINER) {
      obj_to_obj(object, cnt);
      load_otrigger(object);
      return;
    }

    /* neither char nor container found: just dump it in the room */
    if (ch->in_room)
      obj_to_room(object, ch->in_room);
    else
      obj_to_char(object, ch);
    load_otrigger(object);
    return;
  }

  mob_log(ch, "mload: bad type");
}

/* Lets the mobile purge all objects and NPCs in the room, or one named
 * object or mob. It can purge itself, but that has to be its last act. */
ACMD(do_mpurge)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *victim;
  struct obj_data *obj;

  MOBCMD_PREAMBLE(ch);

  if (ch->desc)
    return;

  one_argument(argument, arg);

  if (!*arg) {
    /* bare 'purge' */
    struct char_data *vnext;
    struct obj_data *obj_next;

    if (!ch->in_room)
      return;

    for (victim = ch->in_room->people; victim; victim = vnext) {
      vnext = victim->next_in_room;
      if (IS_NPC(victim) && victim != ch)
        extract_char(victim);
    }

    for (obj = ch->in_room->contents; obj; obj = obj_next) {
      obj_next = obj->next_content;
      extract_obj(obj);
    }

    return;
  }

  victim = (*arg == UID_CHAR) ? get_char(arg) : get_char_room_vis(ch, arg);

  if (victim == NULL) {
    obj = (*arg == UID_CHAR) ? get_obj(arg) : get_obj_vis(ch, arg);

    if (obj)
      extract_obj(obj);
    else
      mob_log(ch, "mpurge: bad argument");

    return;
  }

  if (!IS_NPC(victim)) {
    mob_log(ch, "mpurge: tried to purge a PC");
    return;
  }

  if (victim == ch) {
    /* Purging the mob whose script is running would free it out from under
     * script_driver, so record the intent and let the driver unwind first. */
    dg_owner_purged = 1;
    dg_note_char_extraction(victim);
    return;
  }

  extract_char(victim);
}

/* lets the mobile go to any location it likes */
ACMD(do_mgoto)
{
  char arg[MAX_INPUT_LENGTH];
  struct room_data *location;

  MOBCMD_PREAMBLE(ch);

  one_argument(argument, arg);

  if (!*arg) {
    mob_log(ch, "mgoto called with no argument");
    return;
  }

  if ((location = dg_find_target_room(ch, arg)) == NULL) {
    mob_log(ch, "mgoto: invalid location");
    return;
  }

  if (FIGHTING(ch))
    stop_fighting(ch);

  char_from_room(ch);
  char_to_room(ch, location);
  enter_wtrigger(ch->in_room, ch, -1);
}

/* lets the mobile do a command at another location */
ACMD(do_mat)
{
  char arg[MAX_INPUT_LENGTH];
  struct room_data *location, *original;

  MOBCMD_PREAMBLE(ch);

  argument = one_argument(argument, arg);

  if (!*arg || !*argument) {
    mob_log(ch, "mat: bad argument");
    return;
  }

  if ((location = dg_find_target_room(ch, arg)) == NULL) {
    mob_log(ch, "mat: invalid location");
    return;
  }

  original = ch->in_room;
  char_from_room(ch);
  char_to_room(ch, location);
  command_interpreter(ch, argument, GET_CHAR_NAME(ch));

  /* The command may have moved or removed ch; only put it back if it is
   * still standing where we left it. */
  if (ch->in_room == location && original) {
    char_from_room(ch);
    char_to_room(ch, original);
  }
}

/* Lets the mobile transfer people. 'all' moves everyone in the room. */
ACMD(do_mteleport)
{
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  struct room_data *target;
  struct char_data *vict, *next_ch;

  MOBCMD_PREAMBLE(ch);

  two_arguments(argument, arg1, arg2);

  if (!*arg1 || !*arg2) {
    mob_log(ch, "mteleport: bad syntax");
    return;
  }

  target = dg_find_target_room(ch, arg2);

  if (target == NULL) {
    mob_log(ch, "mteleport target is an invalid room");
    return;
  }

  if (!str_cmp(arg1, "all")) {
    if (target == ch->in_room) {
      mob_log(ch, "mteleport all target is itself");
      return;
    }

    for (vict = ch->in_room ? ch->in_room->people : NULL; vict; vict = next_ch) {
      next_ch = vict->next_in_room;

      if (valid_dg_target(vict, DG_ALLOW_GODS)) {
        char_from_room(vict);
        char_to_room(vict, target);
        enter_wtrigger(vict->in_room, vict, -1);
      }
    }
    return;
  }

  if (*arg1 == UID_CHAR) {
    if (!(vict = get_char(arg1))) {
      mob_log(ch, "mteleport: victim (%s) does not exist", arg1);
      return;
    }
  } else if (!(vict = get_char_vis(ch, arg1))) {
    mob_log(ch, "mteleport: victim (%s) does not exist", arg1);
    return;
  }

  if (valid_dg_target(vict, DG_ALLOW_GODS)) {
    char_from_room(vict);
    char_to_room(vict, target);
    enter_wtrigger(vict->in_room, vict, -1);
  }
}

ACMD(do_mdamage)
{
  char name[MAX_INPUT_LENGTH], amount[MAX_INPUT_LENGTH];
  int dam = 0;
  struct char_data *vict;

  MOBCMD_PREAMBLE(ch);

  two_arguments(argument, name, amount);

  if (!*name || !*amount) {
    mob_log(ch, "mdamage: bad syntax");
    return;
  }

  dam = atoi(amount);

  if (*name == UID_CHAR) {
    if (!(vict = get_char(name))) {
      mob_log(ch, "mdamage: victim (%s) does not exist", name);
      return;
    }
  } else if (!(vict = get_char_room_vis(ch, name))) {
    mob_log(ch, "mdamage: victim (%s) does not exist", name);
    return;
  }

  script_damage(vict, dam);
}

/* Lets the mobile force someone to do something. 'all' affects everyone in
 * the room with the mobile. */
ACMD(do_mforce)
{
  char arg[MAX_INPUT_LENGTH];

  MOBCMD_PREAMBLE(ch);

  if (ch->desc)
    return;

  argument = one_argument(argument, arg);

  if (!*arg || !*argument) {
    mob_log(ch, "mforce: bad syntax");
    return;
  }

  if (!str_cmp(arg, "all")) {
    struct descriptor_data *i;
    struct char_data *vch;

    for (i = descriptor_list; i; i = i->next) {
      if (i->character && (i->character != ch) && !i->connected &&
          (i->character->in_room == ch->in_room)) {
        vch = i->character;
        if (CAN_SEE(ch, vch) && valid_dg_target(vch, 0))
          command_interpreter(vch, argument, GET_CHAR_NAME(ch));
      }
    }
    return;
  }

  struct char_data *victim;

  if (*arg == UID_CHAR) {
    if (!(victim = get_char(arg))) {
      mob_log(ch, "mforce: victim (%s) does not exist", arg);
      return;
    }
  } else if ((victim = get_char_room_vis(ch, arg)) == NULL) {
    mob_log(ch, "mforce: no such victim");
    return;
  }

  if (victim == ch) {
    mob_log(ch, "mforce: forcing self");
    return;
  }

  if (valid_dg_target(victim, 0))
    command_interpreter(victim, argument, GET_CHAR_NAME(ch));
}

/* hunt for someone */
ACMD(do_mhunt)
{
  struct char_data *victim;
  char arg[MAX_INPUT_LENGTH];

  MOBCMD_PREAMBLE(ch);

  if (ch->desc)
    return;

  one_argument(argument, arg);

  if (!*arg) {
    mob_log(ch, "mhunt called with no argument");
    return;
  }

  if (FIGHTING(ch))
    return;

  if (*arg == UID_CHAR) {
    if (!(victim = get_char(arg))) {
      mob_log(ch, "mhunt: victim (%s) does not exist", arg);
      return;
    }
  } else if (!(victim = get_char_vis(ch, arg))) {
    mob_log(ch, "mhunt: victim (%s) does not exist", arg);
    return;
  }

  HUNTING(ch) = victim;
}

/* place someone into the mob's memory list */
ACMD(do_mremember)
{
  struct char_data *victim;
  struct script_memory *mem;
  char arg[MAX_INPUT_LENGTH];

  MOBCMD_PREAMBLE(ch);

  if (ch->desc)
    return;

  argument = one_argument(argument, arg);

  if (!*arg) {
    mob_log(ch, "mremember: bad syntax");
    return;
  }

  if (*arg == UID_CHAR) {
    if (!(victim = get_char(arg))) {
      mob_log(ch, "mremember: victim (%s) does not exist", arg);
      return;
    }
  } else if (!(victim = get_char_vis(ch, arg))) {
    mob_log(ch, "mremember: victim (%s) does not exist", arg);
    return;
  }

  mem = new script_memory;

  if (!SCRIPT_MEM(ch)) {
    SCRIPT_MEM(ch) = mem;
  } else {
    struct script_memory *tmpmem = SCRIPT_MEM(ch);
    while (tmpmem->next)
      tmpmem = tmpmem->next;
    tmpmem->next = mem;
  }

  mem->id = char_script_id(victim);
  if (argument && *argument)
    mem->cmd = str_dup(argument);
}

/* remove someone from the memory list */
ACMD(do_mforget)
{
  struct char_data *victim;
  struct script_memory *mem, *prev;
  char arg[MAX_INPUT_LENGTH];

  MOBCMD_PREAMBLE(ch);

  if (ch->desc)
    return;

  one_argument(argument, arg);

  if (!*arg) {
    mob_log(ch, "mforget: bad syntax");
    return;
  }

  if (*arg == UID_CHAR) {
    if (!(victim = get_char(arg))) {
      mob_log(ch, "mforget: victim (%s) does not exist", arg);
      return;
    }
  } else if (!(victim = get_char_vis(ch, arg))) {
    mob_log(ch, "mforget: victim (%s) does not exist", arg);
    return;
  }

  mem = SCRIPT_MEM(ch);
  prev = NULL;
  while (mem) {
    if (mem->id == char_script_id(victim)) {
      DELETE_ARRAY_IF_EXTANT(mem->cmd);
      if (prev == NULL) {
        SCRIPT_MEM(ch) = mem->next;
        delete mem;
        mem = SCRIPT_MEM(ch);
      } else {
        prev->next = mem->next;
        delete mem;
        mem = prev->next;
      }
    } else {
      prev = mem;
      mem = mem->next;
    }
  }
}

/* Transform into a different mobile.
 *
 * tbaMUD does this with memcpy over char_data. Awake's char_data owns
 * unordered_maps and vectors, so this uses the same struct-assignment plus
 * copy_over_necessary_info() dance medit uses when a prototype changes under
 * a live mob. A leading '-' on the vnum lets the new body's own condition
 * monitors apply instead of carrying the old ones over. */
ACMD(do_mtransform)
{
  char arg[MAX_INPUT_LENGTH];
  struct char_data *temp;
  rnum_t new_rnum, old_rnum = GET_MOB_RNUM(ch);
  bool keep_condition = TRUE;

  MOBCMD_PREAMBLE(ch);

  if (ch->desc) {
    send_to_char("You've got no vnum to return to. Try 'switch'.\r\n", ch);
    return;
  }

  one_argument(argument, arg);

  if (!*arg) {
    mob_log(ch, "mtransform: missing argument");
    return;
  }

  if (!isdigit(*arg) && *arg != '-') {
    mob_log(ch, "mtransform: bad argument");
    return;
  }

  if (*arg == '-') {
    keep_condition = FALSE;
    new_rnum = real_mobile(atol(arg + 1));
  } else {
    new_rnum = real_mobile(atol(arg));
  }

  if (new_rnum < 0) {
    mob_log(ch, "mtransform: bad mobile vnum");
    return;
  }

  if (new_rnum == old_rnum)
    return;

  /* Stash the live state, adopt the new prototype, then put the live state
   * back on top of it. */
  temp = GetCh();
  *temp = *ch;

  *ch = mob_proto[new_rnum];
  ch->nr = new_rnum;
  ch->script_id = temp->script_id;
  ch->proto_script = temp->proto_script;
  ch->script = temp->script;
  ch->script_memory = temp->script_memory;

  copy_over_necessary_info(temp, ch);

  if (!keep_condition) {
    GET_PHYSICAL(ch) = GET_MAX_PHYSICAL(ch);
    GET_MENTAL(ch) = GET_MAX_MENTAL(ch);
  }

  /* temp only ever held ch's pointers, so clear rather than extract it. */
  clear_char(temp);
  delete temp;

  if (old_rnum >= 0)
    mob_index[old_rnum].number--;
  mob_index[new_rnum].number++;
}

ACMD(do_mdoor)
{
  char errbuf[MAX_INPUT_LENGTH];
  const char *err;

  MOBCMD_PREAMBLE(ch);

  if ((err = dg_edit_door(argument, errbuf, sizeof(errbuf))))
    mob_log(ch, "mdoor: %s", err);
}

ACMD(do_mfollow)
{
  char buf[MAX_INPUT_LENGTH];
  struct char_data *leader;
  struct follow_type *j, *k;

  MOBCMD_PREAMBLE(ch);

  one_argument(argument, buf);

  if (!*buf) {
    mob_log(ch, "mfollow: bad syntax");
    return;
  }

  if (*buf == UID_CHAR) {
    if (!(leader = get_char(buf))) {
      mob_log(ch, "mfollow: victim (%s) does not exist", buf);
      return;
    }
  } else if (!(leader = get_char_room_vis(ch, buf))) {
    mob_log(ch, "mfollow: victim (%s) not found", buf);
    return;
  }

  if (ch->master == leader) /* already following */
    return;

  /* stop following someone else first */
  if (ch->master) {
    if (ch->master->followers && ch->master->followers->follower == ch) {
      k = ch->master->followers;
      ch->master->followers = k->next;
      delete k;
    } else {
      for (k = ch->master->followers; k && k->next && k->next->follower != ch; k = k->next)
        ;

      if (k && k->next) {
        j = k->next;
        k->next = j->next;
        delete j;
      }
    }
    ch->master = NULL;
  }

  if (ch == leader)
    return;

  if (circle_follow(ch, leader)) {
    mob_log(ch, "mfollow: following in circles");
    return;
  }

  ch->master = leader;

  k = new follow_type;
  k->follower = ch;
  k->next = leader->followers;
  leader->followers = k;
}

/* ************************************************************************
*  The mob command table.                                                  *
************************************************************************ */

struct mob_command_info {
  const char *command;
  void (*command_pointer)(struct char_data *ch, char *argument, int cmd, int subcmd);
};

static const struct mob_command_info mob_cmd_info[] = {
  { "masound",    do_masound },
  { "mat",        do_mat },
  { "mdamage",    do_mdamage },
  { "mdoor",      do_mdoor },
  { "mecho",      do_mecho },
  { "mechoaround", do_mechoaround },
  { "mfollow",    do_mfollow },
  { "mforce",     do_mforce },
  { "mforget",    do_mforget },
  { "mgoto",      do_mgoto },
  { "mhunt",      do_mhunt },
  { "mjunk",      do_mjunk },
  { "mkill",      do_mkill },
  { "mload",      do_mload },
  { "mlog",       do_mlog },
  { "mpurge",     do_mpurge },
  { "mrecho",     do_mrecho },
  { "mremember",  do_mremember },
  { "msend",      do_msend },
  { "mteleport",  do_mteleport },
  { "mtransform", do_mtransform },
  { "mzoneecho",  do_mzoneecho },
  { NULL,         NULL }
};

/* Run one line of a mob trigger: a mob command if it is one, otherwise an
 * ordinary game command typed by the mob. */
void mob_command_interpreter(struct char_data *ch, char *argument)
{
  char arg[MAX_INPUT_LENGTH];
  char *line;

  skip_spaces(&argument);

  if (!*argument)
    return;

  line = any_one_arg(argument, arg);
  skip_spaces(&line);

  for (int i = 0; mob_cmd_info[i].command; i++) {
    if (!str_cmp(arg, mob_cmd_info[i].command)) {
      (*mob_cmd_info[i].command_pointer)(ch, line, 0, 0);
      return;
    }
  }

  command_interpreter(ch, argument, GET_CHAR_NAME(ch));
}
