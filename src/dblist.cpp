//  file: dblist.cc
//  authors: Chris Dickey, Andrew Hynek
//  purpose: contains the ObjList functions
//  Copyright (c) 1996 by Chris Dickey,
//  some parts Copyright (c) 1998 by Andrew Hynek

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/time.h>

#include "structs.h"
#include "awake.h"
#include "comm.h"
#include "db.h"
#include "utils.h"
#include "dblist.h"
#include "handler.h"
#include "file.h"
#include "newdb.h"
#include "perfmon.h"
#include "config.h"
#include "newmatrix.h"

// extern vars
extern class helpList Help;
extern class helpList WizHelp;
extern void write_world_to_disk(int vnum);

// extern funcs
extern void print_object_location(int, struct obj_data *, struct char_data *, int);
#define OBJ temp->data

int objList::PrintList(struct char_data *ch, const char *arg)
{
  nodeStruct<struct obj_data *> *temp = head;
  int num = 0;

  for (;temp; temp = temp->next)
    if (temp->data && CAN_SEE_OBJ(ch, temp->data)
        && isname(arg, temp->data->text.keywords))
      print_object_location(++num, temp->data, ch, TRUE);

  return num;
}

void objList::Traverse(void (*func)(struct obj_data *))
{
  for (nodeStruct<struct obj_data *> *temp = head; temp; temp = temp->
       next)
    func(temp->data);
}

// this function searches through the list and returns a count of objects
// with the specified virtual number.
int objList::CountObj(int num)
{
  int counter = 0;
  nodeStruct<struct obj_data *> *temp;
  for (temp = head; temp; temp = temp->next)
    if (num == GET_OBJ_RNUM(temp->data))
      counter++;

  return counter;
}

// Adaptation of CountObj to find PC corpses with things in them.
int objList::CountPlayerCorpses()
{
  int counter = 0;
  nodeStruct<struct obj_data *> *temp;
  for (temp = head; temp; temp = temp->next)
    if (GET_OBJ_TYPE(temp->data) == ITEM_CONTAINER 
        && temp->data->contains
        && GET_OBJ_VAL(temp->data, 4) 
        && IS_OBJ_STAT(temp->data, ITEM_CORPSE) 
    )
      counter++;

  return counter;
}

// this function searches through the list and returns a pointer to the
// object whose object rnum matches num
struct obj_data *objList::FindObj(int num)
{
  nodeStruct<struct obj_data *> *temp;
  for (temp = head; temp; temp = temp->next)
    if (num == GET_OBJ_RNUM(temp->data))
      return temp->data;

  return NULL;
}

// this function searches through the list and returns a pointer to the
// object whose name matches 'name' and who is the 'num'th object in the
// list
struct obj_data *objList::FindObj(struct char_data *ch, char *name, int num)
{
  nodeStruct<struct obj_data *> *temp = head;
  int i = 0;

  while (temp && (i <= num))
  {
    if (isname(name, temp->data->text.keywords) &&
        CAN_SEE_OBJ(ch, temp->data) &&
        (++i == num))
      return temp->data;
    temp = temp->next;
  }

  return NULL;
}

// this function updates pointers to this particular prototype--necessary
// for OLC so objects on the mud get updated with the correct values
void objList::UpdateObjs(const struct obj_data *proto, int rnum)
{
  static nodeStruct<struct obj_data *> *temp;
  static struct obj_data old;

  for (temp = head; temp; temp = temp->next)
  {
    if (temp->data->item_number == rnum) {
      old = *temp->data;
      *temp->data = *proto;
      temp->data->in_room = old.in_room;
      temp->data->item_number = rnum;
      temp->data->carried_by = old.carried_by;
      temp->data->worn_by = old.worn_by;
      temp->data->worn_on = old.worn_on;
      temp->data->in_obj = old.in_obj;
      temp->data->contains = old.contains;
      temp->data->next_content = old.next_content;
      temp->data->obj_flags.condition = old.obj_flags.condition;
      temp->data->restring = old.restring;
      temp->data->photo = old.photo;
      if (temp->data->carried_by)
        affect_total(temp->data->carried_by);
      else if (temp->data->worn_by)
        affect_total(temp->data->worn_by);
    }
  }
}

void objList::UpdateObjsIDelete(const struct obj_data *proto, int rnum, int new_rnum)
{
  static nodeStruct<struct obj_data *> *temp;
  static struct obj_data old;
  
  for (temp = head; temp; temp = temp->next)
  {
    if (temp->data->item_number == rnum) {
      old = *temp->data;
      *temp->data = *proto;
      temp->data->in_room = old.in_room;
      temp->data->item_number = rnum;
      temp->data->carried_by = old.carried_by;
      temp->data->worn_by = old.worn_by;
      temp->data->worn_on = old.worn_on;
      temp->data->in_obj = old.in_obj;
      temp->data->contains = old.contains;
      temp->data->next_content = old.next_content;
      temp->data->obj_flags.condition = old.obj_flags.condition;
      temp->data->restring = old.restring;
      temp->data->photo = old.photo;
      if (temp->data->carried_by)
        affect_total(temp->data->carried_by);
      else if (temp->data->worn_by)
        affect_total(temp->data->worn_by);
      
      temp->data->item_number = new_rnum;
    }
  }
}

// this function runs through the list and checks the timers of each
// object, extracting them if their timers hit 0
void objList::UpdateCounters(void)
{
  PERF_PROF_SCOPE(updatecounters_, __func__);
  SPECIAL(trideo);
  MYSQL_RES *res;
  MYSQL_ROW row;
  char *trid = NULL;
  static nodeStruct<struct obj_data *> *temp, *next;
  
  bool trideo_plays = (trideo_ticks++ % TRIDEO_TICK_DELAY == 0);

  if (trideo_plays) {
    // Select the trideo broadcast message we'll be using this tick.
    mysql_wrapper(mysql, "SELECT message FROM trideo_broadcast ORDER BY RAND() LIMIT 1");
    res = mysql_use_result(mysql);
    row = mysql_fetch_row(res);
    trid = str_dup(row[0]);
    mysql_free_result(res);
  }
  
  // Iterate through the list.
  for (temp = head; temp; temp = next) {
    next = temp->next;
    
    // Precondition: The object being examined must exist.
    if (!OBJ) {
      mudlog("SYSERR: UpdateCounters encountered a non-existent object.", NULL, LOG_SYSLOG, TRUE);
      continue;
    }
    
    // Decay evaluate programs.
    if (GET_OBJ_TYPE(OBJ) == ITEM_PROGRAM && GET_OBJ_VAL(OBJ, 0) == SOFT_EVALUATE) {
      if (!GET_OBJ_VAL(OBJ, 5)) {
        GET_OBJ_VAL(OBJ, 5) = time(0);
        GET_OBJ_VAL(OBJ, 6) = GET_OBJ_VAL(OBJ, 5);
      } else if (GET_OBJ_VAL(OBJ, 5) < time(0) - SECS_PER_REAL_DAY && !(OBJ->carried_by && IS_NPC(OBJ->carried_by))) {
        GET_OBJ_VAL(OBJ, 1) -= number(0, 3);
        GET_OBJ_VAL(OBJ, 5) = time(0);
        if (GET_OBJ_VAL(OBJ, 1) < 0)
          GET_OBJ_VAL(OBJ, 1) = 0;
      }
      continue;
    }
    
    // Decrement the attempt counter for credsticks.
    if (GET_OBJ_TYPE(OBJ) == ITEM_MONEY && GET_OBJ_ATTEMPT(OBJ) > 0)
      GET_OBJ_ATTEMPT(OBJ)--;

    // Send out the trideo messages. We assume anything that is a trideo box is not anything else.
    if (trideo_plays && GET_OBJ_SPEC(OBJ) == trideo && GET_OBJ_VAL(OBJ, 0)) {
      snprintf(buf, sizeof(buf), "$p broadcasts, \"%s\"", trid);
      act(buf, TRUE, 0, OBJ, 0, TO_ROOM);
      continue;
    }
    
    // Packing / unpacking of workshops.
    if (GET_OBJ_TYPE(OBJ) == ITEM_WORKSHOP && GET_WORKSHOP_UNPACK_TICKS(OBJ)) {
      struct char_data *ch;
      if (!OBJ->in_veh && !OBJ->in_room) {
        // It's being carried by a character (or is in a container, etc).
        continue;
      }
      
      for (ch = OBJ->in_veh ? OBJ->in_veh->people : OBJ->in_room->people;
            ch;
           ch = OBJ->in_veh ? ch->next_in_veh : ch->next_in_room) {
        if (AFF_FLAGGED(ch, AFF_PACKING)) {
          if (!--GET_WORKSHOP_UNPACK_TICKS(OBJ)) {
            if (GET_WORKSHOP_IS_SETUP(OBJ)) {
              send_to_char(ch, "You finish packing up %s.\r\n", GET_OBJ_NAME(OBJ));
              act("$n finishes packing up $P.", FALSE, ch, 0, OBJ, TO_ROOM);
              GET_WORKSHOP_IS_SETUP(OBJ) = 0;
              
              // Handle the room's workshop[] array.
              if (OBJ->in_room)
                remove_workshop_from_room(OBJ);
            } else {
              send_to_char(ch, "You finish setting up %s.\r\n", GET_OBJ_NAME(OBJ));
              act("$n finishes setting up $P.", FALSE, ch, 0, OBJ, TO_ROOM);
              GET_WORKSHOP_IS_SETUP(OBJ) = 1;
              
              // Handle the room's workshop[] array.
              if (OBJ->in_room)
                add_workshop_to_room(OBJ);
            }
            AFF_FLAGS(ch).RemoveBit(AFF_PACKING);
          }
          break;
        }
      }
      
      // If there were no characters in the room working on it, clear its pack/unpack counter.
      if (!ch) {
        // Only send a message if someone is there.
        if (OBJ->in_room && OBJ->in_room->people) {
          snprintf(buf, sizeof(buf), "A passerby rolls %s eyes and quickly re-%spacks the half-packed $p.",
                  number(0, 1) == 0 ? "his" : "her",
                  GET_WORKSHOP_IS_SETUP(OBJ) ? "un" : "");
          act(buf, FALSE, OBJ->in_room->people, NULL, OBJ, TO_ROOM);
        }
        GET_WORKSHOP_UNPACK_TICKS(OBJ) = 0;
      }
    }
    
    // Cook chips.
    if (GET_OBJ_TYPE(OBJ) == ITEM_DECK_ACCESSORY 
        && GET_OBJ_VAL(OBJ, 0) == TYPE_COOKER 
        && OBJ->contains 
        && GET_DECK_ACCESSORY_COOKER_TIME_REMAINING(OBJ) > 0) {
      if (--GET_DECK_ACCESSORY_COOKER_TIME_REMAINING(OBJ) < 1) {
        struct obj_data *chip = OBJ->contains;
        act("$p beeps loudly, signaling completion.", FALSE, 0, OBJ, 0, TO_ROOM);
        if (GET_OBJ_TIMER(chip) == -1) {
          DELETE_ARRAY_IF_EXTANT(chip->restring);
          chip->restring = str_dup("a ruined optical chip");
        } else
          GET_OBJ_TIMER(chip) = 1;
      }
      continue;
    }

    // In-game this is a UCAS dress shirt. I suspect the files haven't been updated.
    /*
    if (GET_OBJ_VNUM(OBJ) == 120 && ++GET_OBJ_TIMER(OBJ) >= 72) {
      next = temp->next;
      extract_obj(OBJ);
      continue;
    }
    */

    // Time out objects that end up on the floor a lot (magazines, cash, etc).
    if (OBJ->in_room && !OBJ->in_obj && !OBJ->carried_by && !OBJ->obj_flags.quest_id &&
       ((GET_OBJ_TYPE(OBJ) == ITEM_GUN_MAGAZINE && !GET_MAGAZINE_AMMO_COUNT(OBJ)) || (GET_OBJ_TYPE(OBJ) == ITEM_MONEY && !GET_OBJ_VAL(OBJ, 0))) 
        && ++GET_OBJ_TIMER(OBJ) == 3) {
        act("$p is lost on the ground.", TRUE, temp->data->in_room->people,
                OBJ, 0, TO_CHAR);
      next = temp->next;
      extract_obj(OBJ);
      continue;
    }
    
    /* anti-twink measure...no decay until there's no eq in it */
    if ( IS_OBJ_STAT(OBJ, ITEM_CORPSE) && GET_OBJ_VAL(OBJ, 4) && OBJ->contains != NULL )
      continue;

    // Corpse decay.
    if (IS_OBJ_STAT(OBJ, ITEM_CORPSE)) {
      if (GET_OBJ_TIMER(OBJ) > 1) {
        GET_OBJ_TIMER(OBJ)--;
      } else {
        if (OBJ->carried_by)
          act("$p decays in your hands.", FALSE, temp->data->carried_by, temp->data, 0, TO_CHAR);
        else if (temp->data->worn_by)
          act("$p decays in your hands.", FALSE, temp->data->worn_by, temp->data, 0, TO_CHAR);
        else if (temp->data->in_room && temp->data->in_room->people) {
          act("$p is taken away by the coroner.", TRUE, temp->data->in_room->people, temp->data, 0, TO_ROOM);
          act("$p is taken away by the coroner.", TRUE, temp->data->in_room->people, temp->data, 0, TO_CHAR);
          
          if (ROOM_FLAGGED(temp->data->in_room, ROOM_CORPSE_SAVE_HACK)) {
            bool should_clear_flag = TRUE;
            
            // Iterate through items in room, making sure there are no other corpses.
            for (struct obj_data *tmp_obj = temp->data->in_room->contents; tmp_obj; tmp_obj = tmp_obj->next_content) {
              if (tmp_obj != temp->data && IS_OBJ_STAT(tmp_obj, ITEM_CORPSE) && GET_OBJ_BARRIER(tmp_obj) == PC_CORPSE_BARRIER) {
                should_clear_flag = FALSE;
                break;
              }
            }
            
            if (should_clear_flag) {
              snprintf(buf, sizeof(buf), "Cleanup: Auto-removing storage flag from %s (%ld) due to no more player corpses being in it.",
                       GET_ROOM_NAME(temp->data->in_room),
                       GET_ROOM_VNUM(temp->data->in_room));
              mudlog(buf, NULL, LOG_SYSLOG, TRUE);
              
              // No more? Remove storage flag and save.
              temp->data->in_room->room_flags.RemoveBit(ROOM_CORPSE_SAVE_HACK);
              temp->data->in_room->room_flags.RemoveBit(ROOM_STORAGE);
              
              // Save the change.
              for (int counter = 0; counter <= top_of_zone_table; counter++) {
                if ((GET_ROOM_VNUM(temp->data->in_room) >= (zone_table[counter].number * 100)) 
                    && (GET_ROOM_VNUM(temp->data->in_room) <= (zone_table[counter].top))) 
                {
                  write_world_to_disk(zone_table[counter].number);
                  return;
                }
              }
            }
          }
        }
        // here we make sure to remove all items from the object
        struct obj_data *next_thing, *temp2;
        for (temp2 = temp->data->contains; temp2; temp2 = next_thing) {
          next_thing = temp2->next_content;     /*Next in inventory */
          extract_obj(temp2);
        }
        next = temp->next;
        extract_obj(temp->data);
      }
    }
  }
  DELETE_ARRAY_IF_EXTANT(trid);
}

// this function updates the objects in the list whose real numbers are
// greater than num--necessary for olc--but maybe obsolete once the new
// structures come into effect
void objList::UpdateNums(int num)
{
  nodeStruct<struct obj_data *> *temp;
  // just loop through the list and update
  for (temp = head; temp; temp = temp->next)
    if (GET_OBJ_RNUM(temp->data) >= num)
      GET_OBJ_RNUM(temp->data)++;
}

/* this function goes through each object, and if it has a spec, calls it */
void objList::CallSpec()
{
  nodeStruct<struct obj_data *> *temp;

  char empty_argument = '\0';
  for (temp = head; temp; temp = temp->next) {
    if (!temp->data) {
      log("Warning-- found an object in the CallSpec list with null data.");
      continue;
    }
    if (GET_OBJ_SPEC(temp->data) != NULL)
      GET_OBJ_SPEC(temp->data) (NULL, temp->data, 0, &empty_argument);
  }
}

void objList::RemoveObjNum(int num)
{
  nodeStruct<struct obj_data *> *temp, *next;

  for (temp = head; temp; temp = next) {
    next = temp->next;

    if (GET_OBJ_RNUM(temp->data) == num) {
      if (temp->data->carried_by)
        act("$p disintegrates.", FALSE, temp->data->carried_by, temp->data, 0, TO_CHAR);
      else if (temp->data->worn_by)
        act("$p disintegrates.", FALSE, temp->data->carried_by, temp->data, 0, TO_CHAR);
      else if (temp->data->in_room && temp->data->in_room->people) {
        act("$p disintegrates.", TRUE, temp->data->in_room->people, temp->data, 0, TO_ROOM);
        act("$p disintegrates.", TRUE, temp->data->in_room->people, temp->data, 0, TO_CHAR);
      }
      extract_obj(temp->data);
    }
  }
}

void objList::RemoveQuestObjs(int id)
{
  nodeStruct<struct obj_data *> *temp, *next;

  for (temp = head; temp; temp = next) {
    next = temp->next;

    if (temp->data->obj_flags.quest_id == id) {
      if (temp->data->carried_by)
        act("$p disintegrates.", FALSE, temp->data->carried_by, temp->data, 0, TO_CHAR);
      else if (temp->data->worn_by)
        act("$p disintegrates.", FALSE, temp->data->carried_by, temp->data, 0, TO_CHAR);
      else if (temp->data->in_room && temp->data->in_room->people) {
        act("$p disintegrates.", TRUE, temp->data->in_room->people, temp->data, 0, TO_ROOM);
        act("$p disintegrates.", TRUE, temp->data->in_room->people, temp->data, 0, TO_CHAR);
      }
      else if (temp->data->in_host) {
        snprintf(buf3, sizeof(buf3), "%s depixelates and vanishes from the host.\r\n", capitalize(GET_OBJ_NAME(temp->data)));
        send_to_host(temp->data->in_host->vnum, buf3, NULL, TRUE);
        obj_from_host(temp->data);
      }
      extract_obj(temp->data);
    }
  }
}
