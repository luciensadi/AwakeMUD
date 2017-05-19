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

#include <assert.h>
#include <stdio.h>

#include "memory.h"
#include "structs.h"
#include "awake.h"
#include "db.h"
#include "utils.h"
#include "newmatrix.h"

memoryClass::memoryClass()
{
  Room = new stackClass<struct room_data>;
  Obj = new stackClass<struct obj_data>;
  Ch = new stackClass<struct char_data>;
  Veh = new stackClass<struct veh_data>;
  Host = new stackClass<struct host_data>;
  Icon = new stackClass<struct matrix_icon>;
}

memoryClass::memoryClass(const memoryClass & mClass)
{
  Obj = new stackClass<obj_data>;
  Obj = mClass.Obj;
  Ch = new stackClass<char_data>;
  Ch = mClass.Ch;
  Veh = new stackClass<veh_data>;
  Veh = mClass.Veh;
  Host = new stackClass<host_data>;
  Host = mClass.Host;
  Icon = new stackClass<matrix_icon>;
  Icon = mClass.Icon;
}

memoryClass::~memoryClass()
{}

struct obj_data *memoryClass::GetObject()
{
  if (Obj->StackIsEmpty())
  {
    struct obj_data *temp = new obj_data;
    clear_object(temp);
    return temp;
  } else
    return Obj->Pop();
}

struct veh_data *memoryClass::GetVehicle()
{
  if (Veh->StackIsEmpty())
  {
    struct veh_data *temp = new veh_data;
    clear_vehicle(temp);
    return temp;
  } else
    return Veh->Pop();
}

struct host_data *memoryClass::GetHost()
{
  if (Host->StackIsEmpty())
  {
    struct host_data *temp = new host_data;
    clear_host(temp);
    return temp;
  } else
    return Host->Pop();
}

struct matrix_icon *memoryClass::GetIcon()
{
  if (Icon->StackIsEmpty())
  {
    struct matrix_icon *temp = new matrix_icon;
    clear_icon(temp);
    return temp;
  } else
    return Icon->Pop();
}

struct char_data *memoryClass::GetCh()
{
  if (Ch->StackIsEmpty())
  {
    struct char_data *temp = new char_data;
    clear_char(temp);
    return temp;
  } else
    return Ch->Pop();
}

struct room_data *memoryClass::GetRoom()
{
  if (Room->StackIsEmpty())
  {
    struct room_data *temp = new room_data;
    // clear room just zeros the room structure, which is all we need to
    // do since it is fresh
    clear_room(temp);
    return temp;
  } else
    return Room->Pop();
}

void memoryClass::DeleteObject(struct obj_data *obj)
{
  // we want to do this so that when we pop em off, they are usable
  free_obj(obj);
  Obj->Push(obj);
}

void memoryClass::DeleteCh(struct char_data *ch)
{
  free_char(ch);
  Ch->Push(ch);
}

void memoryClass::DeleteRoom(struct room_data *room)
{
  free_room(room);
  Room->Push(room);
}

void memoryClass::DeleteHost(struct host_data *host)
{
  free_host(host);
  Host->Push(host);
}

void memoryClass::DeleteIcon(struct matrix_icon *icon)
{
  free_icon(icon);
  Icon->Push(icon);
}

void memoryClass::DeleteVehicle(struct veh_data *veh)
{
  free_veh(veh);
  Veh->Push(veh);
}

void memoryClass::ClearObject(struct obj_data *obj)
{
  clear_object(obj);
  Obj->Push(obj);
}

void memoryClass::ClearVehicle(struct veh_data *veh)
{
  clear_vehicle(veh);
  Veh->Push(veh);
}

void memoryClass::ClearHost(struct host_data *host)
{
  clear_host(host);
  Host->Push(host);
}

void memoryClass::ClearIcon(struct matrix_icon *icon)
{
  clear_icon(icon);
  Icon->Push(icon);
}

void memoryClass::ClearCh(struct char_data *ch)
{
  clear_char(ch);
  Ch->Push(ch);
}

void memoryClass::ClearRoom(struct room_data *room)
{
  clear_room(room);
  Room->Push(room);
}

bool memoryClass::ClearStacks()
{
  // if all the stacks are empty, return FALSE
  if (Obj->StackIsEmpty() && Ch->StackIsEmpty() && Room->StackIsEmpty())
    return FALSE;

  // and here we just clean up the stacks, hoping to free some memory
  while (!Obj->StackIsEmpty())
    Obj->PopDelete();

  while (!Ch->StackIsEmpty())
    Ch->PopDelete();

  while (!Room->StackIsEmpty())
    Room->PopDelete();

  return TRUE;
}
