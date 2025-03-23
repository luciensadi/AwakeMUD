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
#include "newmail.hpp"
#include "metrics.hpp"
#include "pets.hpp"

// extern vars
extern class helpList Help;
extern class helpList WizHelp;

// extern funcs
extern void print_object_location(int, struct obj_data *, struct char_data *, int);
#define OBJ temp->data

SPECIAL(pocket_sec);

int objList::PrintList(struct char_data *ch, const char *arg, bool override_vis_check)
{
  nodeStruct<struct obj_data *> *temp = head;
  int num = 0;

  for (;temp; temp = temp->next)
    if (temp->data && (override_vis_check || CAN_SEE_OBJ(ch, temp->data))
        && keyword_appears_in_obj(arg, temp->data))
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

  std::unordered_map<obj_data *, bool> obj_ptrs = {};

  for (nodeStruct<struct obj_data *> *temp = head; temp; temp = temp->next) {
    verify_obj_validity(temp->data, TRUE);

    // Assert we have no duplicates in the list, then mark this as being in the list.
    assert(obj_ptrs.find(temp->data) == obj_ptrs.end());
    obj_ptrs[temp->data] = TRUE;
  }
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

      temp->data->item_number = rnum;

      temp->data->in_room = old.in_room;
      temp->data->in_veh = old.in_veh;

      GET_OBJ_QUEST_CHAR_ID(temp->data) = old.obj_flags.quest_id;
      temp->data->obj_flags.condition = old.obj_flags.condition;

      temp->data->restring = old.restring;
      temp->data->matrix_restring = old.matrix_restring;
      temp->data->photo = old.photo;
      temp->data->graffiti = old.graffiti;

      temp->data->carried_by = old.carried_by;
      temp->data->worn_by = old.worn_by;
      temp->data->worn_on = old.worn_on;
      
      temp->data->in_obj = old.in_obj;
      temp->data->contains = old.contains;
      temp->data->files = old.files;
      temp->data->next_content = old.next_content;
      temp->data->in_host = old.in_host;

      temp->data->cyberdeck_part_pointer = old.cyberdeck_part_pointer;

      temp->data->targ = old.targ;
      temp->data->tveh = old.tveh;

      temp->data->dropped_by_char = old.dropped_by_char;
      temp->data->dropped_by_host = old.dropped_by_host;

      temp->data->idnum = old.idnum;

      if (temp->data->carried_by)
        affect_total(temp->data->carried_by);
      else if (temp->data->worn_by)
        affect_total(temp->data->worn_by);
    }
  }
}

// TODO: Why is this a different function? Can it be collapsed into the above?
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
      temp->data->files = old.files;
      temp->data->next_content = old.next_content;
      temp->data->obj_flags.condition = old.obj_flags.condition;
      temp->data->restring = old.restring;
      temp->data->matrix_restring = old.matrix_restring;
      temp->data->photo = old.photo;
      temp->data->graffiti = old.graffiti;
      temp->data->idnum = old.idnum;
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
unsigned long global_pet_act_tick = 0;
void objList::UpdateCounters(void)
{
  PERF_PROF_SCOPE(updatecounters_, __func__);
  SPECIAL(trideo);
  MYSQL_RES *res;
  MYSQL_ROW row;
  char *trid = NULL;
  static nodeStruct<struct obj_data *> *temp, *next;
  time_t timestamp_now = time(0);

  bool trideo_plays = (trideo_ticks++ % TRIDEO_TICK_DELAY == 0);
  int pet_act_tick = (global_pet_act_tick++) % 10;

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

    // This is the only thing a pet object can do, so get it out of the way.
    if (GET_OBJ_TYPE(OBJ) == ITEM_PET) {
      pet_acts(OBJ, pet_act_tick);
      continue;
    }

    // We use the trideo broadcast tick for a variety of timed things.
    if (trideo_plays) {
      // Pocket secretary beep tick.
      if (GET_OBJ_SPEC(OBJ) == pocket_sec && !GET_POCKET_SECRETARY_SILENCED(OBJ)) {
        // Skip secs that couldn't be being carried. Saves two function invocations.
        if (OBJ->in_veh || OBJ->in_room)
          continue;

        struct char_data *carried_by = get_obj_carried_by_recursive(OBJ);
        struct char_data *worn_by = get_obj_worn_by_recursive(OBJ);
        struct char_data *recipient = carried_by ? carried_by : worn_by;

        if (recipient && amount_of_mail_waiting(recipient) > 0) {
          send_to_char(recipient, "%s buzzes quietly, reminding you that you have mail waiting.\r\n", CAP(GET_OBJ_NAME(OBJ)));
        }
        continue;
      }
      // Trideo message tick. 
      else if (GET_OBJ_SPEC(OBJ) == trideo && GET_OBJ_VAL(OBJ, 0) && (OBJ->in_room || OBJ->in_veh)) {
        snprintf(buf, sizeof(buf), "$p broadcasts, \"%s\"", trid);
        act(buf, TRUE, 0, OBJ, 0, TO_ROOM);
        continue;
      }
    }
    
    // Decay evaluate programs. This only fires when they're completed, as non-finished software is ITEM_DESIGN instead.
    if (GET_OBJ_TYPE(OBJ) == ITEM_PROGRAM && GET_PROGRAM_TYPE(OBJ) == SOFT_EVALUATE) {
      if (!GET_PROGRAM_EVALUATE_LAST_DECAY_TIME(OBJ)) {
        GET_PROGRAM_EVALUATE_LAST_DECAY_TIME(OBJ) = current_time;
        GET_PROGRAM_EVALUATE_CREATION_TIME(OBJ) = GET_PROGRAM_EVALUATE_LAST_DECAY_TIME(OBJ);
      }
      // Decay Evaluate program ratings by one every two IRL days.
      else if (GET_PROGRAM_EVALUATE_LAST_DECAY_TIME(OBJ) < current_time - (SECS_PER_REAL_DAY * 2) && !(OBJ->carried_by && IS_NPC(OBJ->carried_by))) {
        GET_PROGRAM_RATING(OBJ)--;
        GET_PROGRAM_EVALUATE_LAST_DECAY_TIME(OBJ) = current_time;
        if (GET_PROGRAM_RATING(OBJ) < 0)
          GET_PROGRAM_RATING(OBJ) = 0;
      }
      continue;
    }

    // Decrement the attempt counter for credsticks.
    if (GET_OBJ_TYPE(OBJ) == ITEM_MONEY && GET_OBJ_ATTEMPT(OBJ) > 0)
      GET_OBJ_ATTEMPT(OBJ)--;

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
        if (AFF_FLAGGED(ch, AFF_PACKING))
      {
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
        && GET_DECK_ACCESSORY_TYPE(OBJ) == TYPE_COOKER
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

    // Time out things that have been abandoned outside of storage etc rooms (qualifier checked when setting timeout)
    // TODO: Maybe put it in a vector of expiring objects rather than making this check be part of every item?
    if (OBJ->in_room
        && !OBJ->in_room->people
        && (GET_OBJ_EXPIRATION_TIMESTAMP(OBJ) > 0 && GET_OBJ_EXPIRATION_TIMESTAMP(OBJ) <= timestamp_now)
        && !OBJ->contains // it's just a headache trying to figure out what to skip over with this
        && !GET_OBJ_QUEST_CHAR_ID(OBJ)
        && !(zone_table[OBJ->in_room->zone].is_pghq
             || ((OBJ->restring || OBJ->photo) && GET_OBJ_TYPE(OBJ) != ITEM_GUN_AMMO) // No customized objects.
             || GET_OBJ_TYPE(OBJ) == ITEM_CREATIVE_EFFORT // No art.
             || GET_OBJ_TYPE(OBJ) == ITEM_DECK_ACCESSORY // No cookers, computers, etc.
             // || GET_OBJ_TYPE(OBJ) == ITEM_CUSTOM_DECK // No custom decks (only matters if !OBJ->contains condition is removed)
             || GET_OBJ_TYPE(OBJ) == ITEM_MAGIC_TOOL
             || GET_OBJ_TYPE(OBJ) == ITEM_PET
             || (GET_OBJ_TYPE(OBJ) == ITEM_GUN_AMMO && GET_AMMOBOX_QUANTITY(OBJ) > 0)
             || (GET_OBJ_TYPE(OBJ) == ITEM_WORKSHOP && GET_WORKSHOP_GRADE(OBJ) > TYPE_KIT) // No workshops or facilities.
             // || GET_OBJ_TYPE(OBJ) == ITEM_SHOPCONTAINER // No shopcontainers (only matters if !OBJ->contains condition is removed)
             || (GET_OBJ_TYPE(OBJ) == ITEM_FOCUS && GET_FOCUS_BONDED_TO(OBJ) > 0) // No bonded normal foci.
             || (GET_OBJ_TYPE(OBJ) == ITEM_WEAPON && !WEAPON_IS_GUN(OBJ) && GET_WEAPON_FOCUS_BONDED_BY(OBJ) > 0) // No bonded weapon foci.
            )
        )
    {
      // Don't steal food and drinks from bar scenes
      bool do_delete = TRUE;
      if ((GET_OBJ_TYPE(OBJ) == ITEM_FOOD || GET_OBJ_TYPE(OBJ) == ITEM_DRINKCON) && OBJ->in_room->people) {
        for (struct char_data *tmp_ch = OBJ->in_room->people; tmp_ch; tmp_ch = tmp_ch->next_in_room) {
          if (!IS_NPC(tmp_ch)) {
            do_delete = FALSE;
            break;
          }
        }
      }
      if (!do_delete) {
        continue;
      }

      const char *representation = generate_new_loggable_representation(OBJ);
#ifndef EXPIRE_STRAY_ITEMS
      mudlog_vfprintf(NULL, LOG_MISCLOG, "Item %s @ %s (%ld) WOULD HAVE been cleaned up by expiration logic.", representation, GET_ROOM_NAME(OBJ->in_room), GET_ROOM_VNUM(OBJ->in_room));
      GET_OBJ_EXPIRATION_TIMESTAMP(OBJ) = 0;
#else
      mudlog_vfprintf(NULL, LOG_MISCLOG, "Item %s @ %s (%ld) HAS been cleaned up by expiration logic.", representation, GET_ROOM_NAME(OBJ->in_room), GET_ROOM_VNUM(OBJ->in_room));
      if (OBJ->in_room->people) {
        act("$p is lost on the ground.", TRUE, OBJ->in_room->people, OBJ, 0, TO_CHAR);
      }
      next = temp->next;
      extract_obj(OBJ);
#endif
      delete [] representation;
      continue;
    }

    // Corpses either have no vnum or are 43 (belongings).
    if (GET_OBJ_VNUM(OBJ) < 0 || GET_OBJ_VNUM(OBJ) == 43) {
      // Don't touch PC belongings that have contents.
      if (GET_CORPSE_IS_PC(OBJ) && OBJ->contains != NULL)
        continue;

      // Corpse decay.
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
        struct room_data *in_room = get_obj_in_room(temp->data);
        for (struct obj_data *contents = temp->data->contains, *next_thing; contents; contents = next_thing) {
          next_thing = contents->next_content;     /*Next in inventory */

          if (GET_OBJ_QUEST_CHAR_ID(contents) && in_room) {
            // If it's a quest item, and we have somewhere to drop it, do so.
            obj_from_obj(contents);
            obj_to_room(contents, in_room);
          } else {
            // Otherwise, extract it. We know it's an NPC corpse because PC corpses never decay.
            if (GET_OBJ_TYPE(contents) == ITEM_WEAPON && WEAPON_IS_GUN(contents) && contents->contains && GET_OBJ_TYPE(contents->contains) == ITEM_GUN_MAGAZINE) {
              AMMOTRACK_OK(GET_MAGAZINE_BONDED_ATTACKTYPE(contents->contains), GET_MAGAZINE_AMMO_TYPE(contents->contains), AMMOTRACK_NPC_SPAWNED, -GET_MAGAZINE_AMMO_COUNT(contents->contains));
            }
            extract_obj(contents);
          }
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

void _remove_obj_from_world(struct obj_data *obj) {
  if (obj->carried_by)
    act("$p disintegrates.", FALSE, obj->carried_by, obj, 0, TO_CHAR);
  else if (obj->worn_by)
    act("$p disintegrates.", FALSE, obj->carried_by, obj, 0, TO_CHAR);
  else if (obj->in_room && obj->in_room->people) {
    act("$p disintegrates.", TRUE, obj->in_room->people, obj, 0, TO_ROOM);
    act("$p disintegrates.", TRUE, obj->in_room->people, obj, 0, TO_CHAR);
  }
  else if (obj->in_host) {
    snprintf(buf3, sizeof(buf3), "%s depixelates and vanishes from the host.\r\n", capitalize(GET_OBJ_NAME(obj)));
    send_to_host(obj->in_host->rnum, buf3, NULL, TRUE);
  }
  extract_obj(obj);
}

void objList::RemoveObjNum(rnum_t rnum)
{
  nodeStruct<struct obj_data *> *temp, *next;

  if (rnum < 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Got invalid rnum to RemoveObjNum(%ld)!", rnum);
    return;
  }

  for (temp = head; temp; temp = next) {
    next = temp->next;

    if (GET_OBJ_RNUM(temp->data) == rnum) {
      _remove_obj_from_world(temp->data);
      // We've potentially invalidated our next pointer, so start iterating again.
      next = head;
    }
  }
}

void objList::RemoveQuestObjs(idnum_t questor_idnum)
{
  nodeStruct<struct obj_data *> *temp, *next;

  if (questor_idnum <= 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Got invalid questor_idnum to RemoveQuestObjs(%ld)!", questor_idnum);
    return;
  }

  for (temp = head; temp; temp = next) {
    next = temp->next;

    if (GET_OBJ_QUEST_CHAR_ID(temp->data) == questor_idnum) {
      _remove_obj_from_world(temp->data);
      // We've potentially invalidated our next pointer, so start iterating again.
      next = head;
    }
  }
}

void objList::CheckForDeletedCharacterFuckery(struct char_data *ch, const char *their_name, idnum_t idnum)
{
  static nodeStruct<struct obj_data *> *temp, *next;
  bool found_something = FALSE;

  if (!ch) {
    mudlog("SYSERR: CheckForDeletedCharacterFuckery got a NULL char.", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // Iterate through the list.
  for (temp = head; temp; temp = next) {
    next = temp->next;

    // Precondition: The object being examined must exist.
    if (!OBJ) {
      mudlog("SYSERR: CheckForDeletedCharacterFuckery encountered a non-existent object.", NULL, LOG_SYSLOG, TRUE);
      continue;
    }

    struct char_data *owner = OBJ->carried_by ? OBJ->carried_by : OBJ->worn_by;

    if (owner) {
      if (owner == ch) {
        mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: CheckForDeletedCharacterFuckery FOUND object %s (%ld) in objList after character deletion (pointer match)!", GET_OBJ_NAME(OBJ), GET_OBJ_VNUM(OBJ));
        found_something = TRUE;
        continue;
      }

      if (GET_CHAR_NAME(owner) && !str_cmp(GET_CHAR_NAME(owner), their_name)) {
        mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: CheckForDeletedCharacterFuckery FOUND object %s (%ld) in objList after character deletion (name match)!", GET_OBJ_NAME(OBJ), GET_OBJ_VNUM(OBJ));
        found_something = TRUE;
        continue;
      }

      if (GET_IDNUM(owner) == idnum) {
        mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: CheckForDeletedCharacterFuckery FOUND object %s (%ld) in objList after character deletion (idnum match)!", GET_OBJ_NAME(OBJ), GET_OBJ_VNUM(OBJ));
        found_something = TRUE;
        continue;
      }
    }
  }

  if (!found_something) {
    mudlog("CheckForDeletedCharacterFuckery cleared successfully.", NULL, LOG_SYSLOG, TRUE);
  }
}