/**************************************************************************
*  File: dg_handler.cpp                                                   *
*  Usage: Memory handling for scripts.                                    *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "db.hpp"
#include "handler.hpp"
#include "constants.hpp"
#include "dg_scripts.hpp"
#include "dg_event.hpp"

/* frees memory associated with var */
void free_var_el(struct trig_var_data *var)
{
  DELETE_ARRAY_IF_EXTANT(var->name);
  DELETE_ARRAY_IF_EXTANT(var->value);
  delete var;
}

/* release memory allocated for a variable list */
void free_varlist(struct trig_var_data *vd)
{
  struct trig_var_data *i, *j;

  for (i = vd; i;) {
    j = i;
    i = i->next;
    free_var_el(j);
  }
}

/* Remove var name from var_list. Returns 1 if found, else 0. */
int remove_var(struct trig_var_data **var_list, const char *name)
{
  struct trig_var_data *i, *j;

  for (j = NULL, i = *var_list; i && str_cmp(name, i->name); j = i, i = i->next)
    ;

  if (i) {
    if (j)
      j->next = i->next;
    else
      *var_list = i->next;

    free_var_el(i);
    return 1;
  }

  return 0;
}

/* Return memory used by a trigger. The command list belongs to the prototype,
 * so it is freed when the prototype changes and when shutting down, not here. */
void free_trigger(struct trig_data *trig)
{
  DELETE_ARRAY_IF_EXTANT(trig->name);
  DELETE_ARRAY_IF_EXTANT(trig->arglist);

  if (trig->var_list) {
    free_varlist(trig->var_list);
    trig->var_list = NULL;
  }

  if (GET_TRIG_WAIT(trig)) {
    event_cancel(GET_TRIG_WAIT(trig));
    GET_TRIG_WAIT(trig) = NULL;
  }

  delete trig;
}

/* remove a single trigger from a mob/obj/room */
void extract_trigger(struct trig_data *trig)
{
  struct trig_data *temp;

  if (GET_TRIG_WAIT(trig)) {
    event_cancel(GET_TRIG_WAIT(trig));
    GET_TRIG_WAIT(trig) = NULL;
  }

  if (trig->nr >= 0 && trig->nr < top_of_trigt)
    trig_index[trig->nr]->number--;

  /* walk the trigger list and remove this one */
  REMOVE_FROM_LIST(trig, trigger_list, next_in_world);

  free_trigger(trig);
}

/* remove all triggers from a mob/obj/room */
void extract_script(void *thing, int type)
{
  struct script_data *sc = NULL;
  struct trig_data *trig, *next_trig;

  switch (type) {
    case MOB_TRIGGER: {
      struct char_data *mob = (struct char_data *) thing;
      sc = SCRIPT(mob);
      SCRIPT(mob) = NULL;
      break;
    }
    case OBJ_TRIGGER: {
      struct obj_data *obj = (struct obj_data *) thing;
      sc = SCRIPT(obj);
      SCRIPT(obj) = NULL;
      break;
    }
    case WLD_TRIGGER: {
      struct room_data *room = (struct room_data *) thing;
      sc = SCRIPT(room);
      SCRIPT(room) = NULL;
      break;
    }
  }

  if (!sc)
    return;

  for (trig = TRIGGERS(sc); trig; trig = next_trig) {
    next_trig = trig->next;
    extract_trigger(trig);
  }
  TRIGGERS(sc) = NULL;

  free_varlist(sc->global_vars);
  sc->global_vars = NULL;

  delete sc;
}

/* erase the script memory of a mob */
void extract_script_mem(struct script_memory *sc)
{
  struct script_memory *next;

  while (sc) {
    next = sc->next;
    DELETE_ARRAY_IF_EXTANT(sc->cmd);
    delete sc;
    sc = next;
  }
}

void free_proto_script(void *thing, int type)
{
  struct trig_proto_list *proto = NULL, *fproto;

  switch (type) {
    case MOB_TRIGGER: {
      struct char_data *mob = (struct char_data *) thing;
      proto = mob->proto_script;
      mob->proto_script = NULL;
      break;
    }
    case OBJ_TRIGGER: {
      struct obj_data *obj = (struct obj_data *) thing;
      proto = obj->proto_script;
      obj->proto_script = NULL;
      break;
    }
    case WLD_TRIGGER: {
      struct room_data *room = (struct room_data *) thing;
      proto = room->proto_script;
      room->proto_script = NULL;
      break;
    }
  }

  while (proto) {
    fproto = proto;
    proto = proto->next;
    delete fproto;
  }
}

void copy_proto_script(void *source, void *dest, int type)
{
  struct trig_proto_list *tp_src = NULL, *tp_dst = NULL;

  switch (type) {
    case MOB_TRIGGER:
      tp_src = ((struct char_data *) source)->proto_script;
      break;
    case OBJ_TRIGGER:
      tp_src = ((struct obj_data *) source)->proto_script;
      break;
    case WLD_TRIGGER:
      tp_src = ((struct room_data *) source)->proto_script;
      break;
  }

  if (!tp_src)
    return;

  tp_dst = new trig_proto_list;

  switch (type) {
    case MOB_TRIGGER:
      ((struct char_data *) dest)->proto_script = tp_dst;
      break;
    case OBJ_TRIGGER:
      ((struct obj_data *) dest)->proto_script = tp_dst;
      break;
    case WLD_TRIGGER:
      ((struct room_data *) dest)->proto_script = tp_dst;
      break;
  }

  while (tp_src) {
    tp_dst->vnum = tp_src->vnum;
    tp_src = tp_src->next;
    if (tp_src)
      tp_dst->next = new trig_proto_list;
    tp_dst = tp_dst->next;
  }
}

/* A room's contents moved elsewhere, so any script waiting on this room has to
 * be told where it ended up. */
void update_wait_events(struct room_data *to, struct room_data *from)
{
  struct trig_data *trig;

  if (!SCRIPT(from))
    return;

  for (trig = TRIGGERS(SCRIPT(from)); trig; trig = trig->next) {
    if (!GET_TRIG_WAIT(trig))
      continue;

    ((struct wait_event_data *) GET_TRIG_WAIT(trig)->event_obj)->go = to;
  }
}
