/**************************************************************************/
/*  file: memory.h  -  1/21/96 -- Christopher J. Dickey                   */
/*    This is a set of classes to handle memory for the mud.  There's     */
/*    not a whole lot of fancy stuff behind them, but they do help in     */
/*    efficiency.  Basically, we never really delete any memory unless    */
/*    absolutely necessary, and we keep stacks of the most used           */
/*    objects.                                                            */
/**************************************************************************/

#ifndef _memory_h_
#define _memory_h_

#include "structs.hpp"
#include "utils.hpp"

extern struct obj_data *GetObject();
extern struct char_data *GetCh();
extern struct room_data *GetRoom();
extern struct veh_data  *GetVehicle();
extern struct host_data *GetHost();
extern struct matrix_icon *GetIcon();

extern void DeleteObject(struct obj_data *obj, const char *source);
extern void DeleteCh(struct char_data *ch);
extern void DeleteRoom(struct room_data *room);
extern void DeleteVehicle(struct veh_data *veh);
extern void DeleteHost(struct host_data *host);
extern void DeleteIcon(struct matrix_icon * icon);

extern class objList ObjList;

#endif
