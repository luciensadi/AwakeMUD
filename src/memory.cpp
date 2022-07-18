/* *********************************************************************
*  file: memory.cc  -  1/21/96 -- Christopher J. Dickey                *
*  This is a set of classes to handle memory for the mud.  There's     *
*  not a whole lot of fancy stuff behind them, but they do help in     *
*  efficiency.  Basically, we never really delete any memory unless    *
*  absolutely necessary, and we keep stacks of the most used           *
*  objects.                                                            *
*  (c) 1996-1997 Christopher J. Dickey, Andrew Hynek,                  *
*  (c) 2001 The AwakeMUD Consortium                                    *
********************************************************************* */

/* Edit 2022: Chris and Andrew, I'm sure you had a good reason for writing
   this stack code the way you did, but it's been the cause of endless
   memory issues stemming from values not being cleared before reuse.
   Modern computers are now fast enough that I don't care about the overhead
   of allocating new memory every time, so I'm going that route instead.
   I've gutted this class to make that happen. -- LS */

#include <assert.h>
#include <stdio.h>

#include "memory.hpp"
#include "structs.hpp"
#include "awake.hpp"
#include "db.hpp"
#include "utils.hpp"
#include "newmatrix.hpp"
#include "ignore_system.hpp"
#include "invis_resistance_tests.hpp"

memoryClass::memoryClass()
{}

memoryClass::memoryClass(const memoryClass & mClass)
{}

memoryClass::~memoryClass()
{}

struct obj_data *memoryClass::GetObject()
{
  struct obj_data *temp = new obj_data;
  clear_object(temp);
  return temp;
}

struct veh_data *memoryClass::GetVehicle()
{
  struct veh_data *temp = new veh_data;
  clear_vehicle(temp);
  return temp;
}

struct host_data *memoryClass::GetHost()
{
  struct host_data *temp = new host_data;
  clear_host(temp);
  return temp;
}

struct matrix_icon *memoryClass::GetIcon()
{
  struct matrix_icon *temp = new matrix_icon;
  clear_icon(temp);
  return temp;
}

struct char_data *memoryClass::GetCh()
{
  struct char_data *temp = new char_data;
  clear_char(temp);
  return temp;
}

struct room_data *memoryClass::GetRoom()
{
  struct room_data *temp = new room_data;
  // clear room just zeros the room structure, which is all we need to
  // do since it is fresh
  clear_room(temp);
  return temp;
}

void memoryClass::DeleteObject(struct obj_data *obj)
{
  // we want to do this so that when we pop em off, they are usable
  free_obj(obj);
  delete obj;
  // Obj->Push(obj);
}

void memoryClass::DeleteCh(struct char_data *ch)
{
  extern struct char_data *combat_list;
  extern void stop_fighting(struct char_data * ch);

  // Verify that we don't have a nulled-out character in the player list. This might add a lot of lag.
  struct char_data *prev = character_list, *next = NULL;
  if (prev && prev == ch) {
    mudlog("^YWARNING: Would have left a nulled-out character in the character list!", ch, LOG_SYSLOG, TRUE);
    character_list = character_list->next;
  } else {
    for (struct char_data *i = character_list; i; i = i->next) {
      if (i == ch) {
        mudlog("^YWARNING: Would have left a nulled-out character in the character list!", ch, LOG_SYSLOG, TRUE);
        prev->next = i->next;
        i->next = NULL;
        break;
      }
      prev = i;
    }
  }

  prev = combat_list;
  if (prev) {
    for (struct char_data *i = combat_list; i; i = next) {
      next = i->next_fighting;
      if (FIGHTING(i) == ch) {
        mudlog("^YWARNING: Would have left a nulled-out character as a combat target!", ch, LOG_SYSLOG, TRUE);
        stop_fighting(i);
        if (FIGHTING(i) == ch) {
          mudlog("^RWE'RE GONNA CRASH, BOIS: Failed to wipe out the fighting pointer pointing to our nulled-out character!", ch, LOG_SYSLOG, TRUE);
          continue;
        }
      }
      prev = i;
    }
  }

  // Delete their ignore data. This deconstructs it as expected.
  if (ch->ignore_data)
    delete ch->ignore_data;

  // Delete their invis resistance data, then remove them from all other PC's invis resistance records.
  purge_invis_invis_resistance_records(ch);
  remove_ch_from_pc_invis_resistance_records(ch);

  free_char(ch);
  delete ch;
  // Ch->Push(ch);
}

void memoryClass::DeleteRoom(struct room_data *room)
{
  free_room(room);
  delete room;
}

void memoryClass::DeleteHost(struct host_data *host)
{
  free_host(host);
  delete host;
}

void memoryClass::DeleteIcon(struct matrix_icon *icon)
{
  free_icon(icon);
  delete icon;
}


void memoryClass::DeleteVehicle(struct veh_data *veh)
{
  free_veh(veh);
  delete veh;
}
