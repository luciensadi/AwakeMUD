//  file: dblist.cc
//  authors: Chris Dickey, Andrew Hynek
//  purpose: contains the ObjList functions
//  Copyright (c) 1996 by Chris Dickey,
//  some parts Copyright (c) 1998 by Andrew Hynek

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/time.h>

#include "structs.hpp"
#include "awake.hpp"
#include "comm.hpp"
#include "db.hpp"
#include "utils.hpp"
#include "dblist.hpp"
#include "handler.hpp"
#include "file.hpp"
#include "newdb.hpp"
#include "perfmon.hpp"
#include "config.hpp"
#include "newmatrix.hpp"
#include "deck_build.hpp"
#include "redit.hpp"

// extern vars
extern class helpList Help;
extern class helpList WizHelp;

// extern funcs
extern void print_object_location(int, struct obj_data *, struct char_data *, int);
#define OBJ temp->data

int objList::PrintList(struct char_data *ch, const char *arg, bool override_vis_check)
{
  nodeStruct<struct obj_data *> *temp = head;
  int num = 0;

  for (;temp; temp = temp->next)
    if (temp->data && (override_vis_check || CAN_SEE_OBJ(ch, temp->data))
        && isname(arg, temp->data->text.keywords))
      print_object_location(++num, temp->data, ch, TRUE);

  return num;
}

int objList::PrintBelongings(struct char_data *ch)
{
  nodeStruct<struct obj_data *> *temp = head;
  int num = 0;

  for (;temp; temp = temp->next) {
    // Skip anything that doesn't exist or isn't a container.
    if (!temp->data || GET_OBJ_TYPE(temp->data) != ITEM_CONTAINER)
      continue;

    // Note the out-of-order calls here. We check the idnums (cheap calls) before checking the obj flags (slightly more expensive).
    if (GET_CORPSE_IS_PC(temp->data) && GET_CORPSE_IDNUM(temp->data) == GET_IDNUM(ch) && IS_OBJ_STAT(temp->data, ITEM_EXTRA_CORPSE))
      print_object_location(++num, temp->data, ch, TRUE);
  }

  return num;
}

void objList::Traverse(void (*func)(struct obj_data *))
{
  for (nodeStruct<struct obj_data *> *temp = head; temp; temp = temp->next)
    func(temp->data);
}

#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
void objList::CheckPointers()
{
  extern void verify_obj_validity(struct obj_data *obj, bool go_deep=FALSE);

  for (nodeStruct<struct obj_data *> *temp = head; temp; temp = temp->next)
    verify_obj_validity(temp->data, TRUE);
}
#endif

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
        && IS_OBJ_STAT(temp->data, ITEM_EXTRA_CORPSE)
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
  PERF_PROF_SCOPE(pr_, __func__);
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
  PERF_PROF_SCOPE(pr_, __func__);
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

  time_t current_time = time(0);

  // Iterate through the list.
  for (temp = head; temp; temp = next) {
    next = temp->next;

    // Precondition: The object being examined must exist.
    if (!OBJ) {
      mudlog("SYSERR: UpdateCounters encountered a non-existent object.", NULL, LOG_SYSLOG, TRUE);
      continue;
    }

    // Decay evaluate programs. This only fires when they're completed, as non-finished software is ITEM_DESIGN instead.
    if (GET_OBJ_TYPE(OBJ) == ITEM_PROGRAM && GET_PROGRAM_TYPE(OBJ) == SOFT_EVALUATE) {
      if (!GET_OBJ_VAL(OBJ, 5)) {
        GET_OBJ_VAL(OBJ, 5) = current_time;
        GET_OBJ_VAL(OBJ, 6) = GET_OBJ_VAL(OBJ, 5);
      }
      // Decay Evaluate program ratings by one every two IRL days.
      else if (GET_OBJ_VAL(OBJ, 5) < current_time - (SECS_PER_REAL_DAY * 2) && !(OBJ->carried_by && IS_NPC(OBJ->carried_by))) {
        GET_PROGRAM_RATING(OBJ)--;
        GET_OBJ_VAL(OBJ, 5) = current_time;
        if (GET_PROGRAM_RATING(OBJ) < 0)
          GET_PROGRAM_RATING(OBJ) = 0;
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
              GET_SETTABLE_WORKSHOP_IS_SETUP(OBJ) = 0;

              // Handle the room's workshop[] array.
              if (OBJ->in_room)
                remove_workshop_from_room(OBJ);
            } else {
              send_to_char(ch, "You finish setting up %s.\r\n", GET_OBJ_NAME(OBJ));
              act("$n finishes setting up $P.", FALSE, ch, 0, OBJ, TO_ROOM);
              GET_SETTABLE_WORKSHOP_IS_SETUP(OBJ) = 1;

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
        if (OBJ->in_room) {
          snprintf(buf, sizeof(buf), "A passerby rolls %s eyes and quickly re-%spacks the half-packed %s.",
                  number(0, 1) == 0 ? "his" : "her",
                  GET_WORKSHOP_IS_SETUP(OBJ) ? "un" : "",
                  GET_OBJ_NAME(OBJ)
                );
          send_to_room(buf, OBJ->in_room);
        } else if (OBJ->in_veh) {
          snprintf(buf, sizeof(buf), "Huh, %s must have actually been %spacked this whole time.",
                  GET_OBJ_NAME(OBJ),
                  GET_WORKSHOP_IS_SETUP(OBJ) ? "un" : ""
                );
          send_to_veh(buf, OBJ->in_veh, NULL, FALSE);
        }
        GET_WORKSHOP_UNPACK_TICKS(OBJ) = 0;
      }
    }

    // Cook chips.
    if (GET_OBJ_TYPE(OBJ) == ITEM_DECK_ACCESSORY
        && GET_OBJ_VAL(OBJ, 0) == TYPE_COOKER
        && OBJ->contains
        && GET_DECK_ACCESSORY_COOKER_TIME_REMAINING(OBJ) > 0)
    {
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

    // Decay mail.
    if (GET_OBJ_VNUM(OBJ) == OBJ_PIECE_OF_MAIL) {
      if (GET_OBJ_TIMER(OBJ) != -1 && GET_OBJ_TIMER(OBJ) > MAIL_EXPIRATION_TICKS) {
        snprintf(buf, sizeof(buf), "Extracting expired mail '%s'.", GET_OBJ_NAME(OBJ));
        mudlog(buf, NULL, LOG_SYSLOG, TRUE);
        extract_obj(OBJ);
        continue;
      }
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
    if ( IS_OBJ_STAT(OBJ, ITEM_EXTRA_CORPSE) && GET_OBJ_VAL(OBJ, 4) && OBJ->contains != NULL )
      continue;

    // Corpse decay.
    if (IS_OBJ_STAT(OBJ, ITEM_EXTRA_CORPSE)) {
      if (GET_OBJ_TIMER(OBJ) > 1) {
        GET_OBJ_TIMER(OBJ)--;
      } else {
        if (OBJ->carried_by)
          act("$p decays in your hands.", FALSE, temp->data->carried_by, temp->data, 0, TO_CHAR);
        else if (temp->data->worn_by)
          act("$p decays in your hands.", FALSE, temp->data->worn_by, temp->data, 0, TO_CHAR);
        else if (temp->data->in_room && temp->data->in_room->people) {
          if (str_str("remains of", temp->data->text.room_desc)) {
            act("$p are taken away by the coroner.", TRUE, temp->data->in_room->people, temp->data, 0, TO_ROOM);
            act("$p are taken away by the coroner.", TRUE, temp->data->in_room->people, temp->data, 0, TO_CHAR);
          } else {
            act("$p is taken away by the coroner.", TRUE, temp->data->in_room->people, temp->data, 0, TO_ROOM);
            act("$p is taken away by the coroner.", TRUE, temp->data->in_room->people, temp->data, 0, TO_CHAR);
          }

          if (ROOM_FLAGGED(temp->data->in_room, ROOM_CORPSE_SAVE_HACK)) {
            bool should_clear_flag = TRUE;

            // Iterate through items in room, making sure there are no other corpses.
            for (struct obj_data *tmp_obj = temp->data->in_room->contents; tmp_obj; tmp_obj = tmp_obj->next_content) {
              if (tmp_obj != temp->data && IS_OBJ_STAT(tmp_obj, ITEM_EXTRA_CORPSE) && GET_OBJ_BARRIER(tmp_obj) == PC_CORPSE_BARRIER) {
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
        send_to_host(temp->data->in_host->rnum, buf3, NULL, TRUE);
        obj_from_host(temp->data);
      }
      extract_obj(temp->data);
    }
  }
}
