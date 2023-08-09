#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <vector>
#include <sstream>
#include <algorithm>


#if defined(WIN32) && !defined(__CYGWIN__)
#else
#include <unistd.h>
#endif

#include "structs.hpp"
#include "awake.hpp"
#include "comm.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "house.hpp"
#include "memory.hpp"
#include "constants.hpp"
#include "file.hpp"
#include "vtable.hpp"
#include "limits.hpp"

extern char *cleanup(char *dest, const char *src);
extern void ASSIGNMOB(long mob, SPECIAL(fname));
extern void add_phone_to_list(struct obj_data *obj);
extern void auto_repair_obj(struct obj_data *obj, idnum_t owner);
extern void handle_weapon_attachments(struct obj_data *obj);
extern void raw_store_mail(long to, long from_id, const char *from_name, const char *message_pointer);
extern bool player_is_dead_hardcore(long id);

struct landlord *landlords = NULL;
ACMD_CONST(do_say);

void remove_vehicles_from_apartment(struct room_data *room);
void warn_about_apartment_deletion();

void House_delete_file(vnum_t vnum, char *name);

struct life_data
{
  const char *name;
  sh_int cost;
};

struct life_data lifestyle[] =
  {
    { "Low", 1
    },
    { "Middle", 3 },
    { "High", 10 },
    { "Luxury", 25 }
  };

/* First, the basics: finding the filename; loading/saving objects */

/* Return a filename given a house vnum */
bool House_get_filename(vnum_t vnum, char *filename, int filename_size)
{
  if (vnum == NOWHERE)
    return FALSE;

  snprintf(filename, filename_size, "house/%ld.house", vnum);
  return TRUE;
}

// Same, but for storage.
bool Storage_get_filename(vnum_t vnum, char *filename, int filename_size)
{
  if (vnum == NOWHERE)
    return FALSE;

  snprintf(filename, filename_size, "storage/%ld", vnum);
  return TRUE;
}

/* Load all objects for a house */
bool House_load(struct house_control_rec *house)
{
  vnum_t room;
  File fl;
  char fname[MAX_STRING_LENGTH];
  rnum_t rnum;
  struct obj_data *obj = NULL, *last_obj = NULL;
  long vnum;
  int inside = 0, last_inside = 0;
  int house_version = 0;
  std::vector<nested_obj> contained_obj;
  struct nested_obj contained_obj_entry;

  room = house->vnum;
  if ((rnum = real_room(room)) == NOWHERE) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Not loading house %ld: Real room does not exist.", room);
    return FALSE;
  }

  if (!House_get_filename(room, fname, sizeof(fname))) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Not loading house %ld: Cannot generate filename.", room);
    return FALSE;
  }

  if (!(fl.Open(fname, "r+b"))) { /* no file found */
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Not loading house %ld: File not found.", room);
    return FALSE;
  }

  log_vfprintf("Loading house file %s.", fname);
  VTable data;
  data.Parse(&fl);
  fl.Close();
  snprintf(buf3, sizeof(buf3), "house-load %s", fname);
  house_version = data.GetInt("METADATA/Version", 0);

  for (int i = 0; i < MAX_GUESTS; i++)
  {
    snprintf(buf, sizeof(buf), "GUESTS/Guest%d", i);
    long p = data.GetLong(buf, 0);
    if (!p || !(does_player_exist(p)))
      p = 0;
    house->guests[i] = p;
  }

  int num_objs = data.NumSubsections("HOUSE");
  int x;
  for (int i = 0; i < num_objs; i++)
  {
    const char *sect_name = data.GetIndexSection("HOUSE", i);
    snprintf(buf, sizeof(buf), "%s/Vnum", sect_name);
    vnum = data.GetLong(buf, 0);
    if ((obj = read_object(vnum, VIRTUAL))) {
      // Wipe its cost-- we're restoring from the saved value.
      GET_OBJ_COST(obj) = 0;

      snprintf(buf, sizeof(buf), "%s/Name", sect_name);
      obj->restring = str_dup(data.GetString(buf, NULL));
      snprintf(buf, sizeof(buf), "%s/Photo", sect_name);
      obj->photo = str_dup(data.GetString(buf, NULL));
      for (x = 0; x < NUM_VALUES; x++) {
        snprintf(buf, sizeof(buf), "%s/Value %d", sect_name, x);
        GET_OBJ_VAL(obj, x) = data.GetInt(buf, GET_OBJ_VAL(obj, x));
      }
      if (GET_OBJ_TYPE(obj) == ITEM_PHONE && GET_ITEM_PHONE_SWITCHED_ON(obj))
        add_phone_to_list(obj);
      if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
        handle_weapon_attachments(obj);
      snprintf(buf, sizeof(buf), "%s/Condition", sect_name);
      GET_OBJ_CONDITION(obj) = data.GetInt(buf, GET_OBJ_CONDITION(obj));
      snprintf(buf, sizeof(buf), "%s/Timer", sect_name);
      GET_OBJ_TIMER(obj) = data.GetInt(buf, GET_OBJ_TIMER(obj));
      snprintf(buf, sizeof(buf), "%s/Attempt", sect_name);
      GET_OBJ_ATTEMPT(obj) = data.GetInt(buf, 0);
      snprintf(buf, sizeof(buf), "%s/Cost", sect_name);
      // At this point, the cost is restored to a positive value. MAX() guards against edge case of attachment being edited after it was attached.
      GET_OBJ_COST(obj) += MAX(0, data.GetInt(buf, GET_OBJ_COST(obj)));

      if (GET_OBJ_VNUM(obj) == OBJ_SPECIAL_PC_CORPSE) {
        // Invalid belongings.
        if (GET_OBJ_VAL(obj, 5) <= 0) {
          extract_obj(obj);
          continue;
        }

        const char *player_name = get_player_name(GET_OBJ_VAL(obj, 5));
        if (!player_name || !str_cmp(player_name, "deleted")) {
          // Whoops, it belongs to a deleted character. RIP.
          extract_obj(obj);
          continue;
        }

        // Set up special corpse values. This will probably cause a memory leak. We use name instead of desc.
        snprintf(buf, sizeof(buf), "belongings %s", player_name);
        snprintf(buf1, sizeof(buf1), "^rThe belongings of %s are lying here.^n", decapitalize_a_an(player_name));
        snprintf(buf2, sizeof(buf2), "^rthe belongings of %s^n", player_name);
        strlcpy(buf3, "Looks like the DocWagon trauma team wasn't able to bring this stuff along.\r\n", sizeof(buf3));
        obj->text.keywords = str_dup(buf);
        obj->text.room_desc = str_dup(buf1);
        obj->text.name = str_dup(buf2);
        obj->text.look_desc = str_dup(buf3);

        GET_OBJ_VAL(obj, 4) = 1;
        GET_OBJ_BARRIER(obj) = PC_CORPSE_BARRIER;
        GET_OBJ_CONDITION(obj) = 100;

        delete [] player_name;
      }

      // Don't auto-repair cyberdecks until they're fully loaded.
      if (GET_OBJ_TYPE(obj) != ITEM_CYBERDECK)
        auto_repair_obj(obj, house->owner);

      snprintf(buf, sizeof(buf), "%s/Inside", sect_name);
      inside = data.GetInt(buf, 0);
      if (house_version == VERSION_HOUSE_FILE) {
        // Since we're now saved the obj linked lists  in reverse order, in order to fix the stupid reordering on
        // every binary execution, the previous algorithm did not work, as it relied on getting the container obj
        // first and place subsequent objects in it. Since we're now getting it last, and more importantly we get
        // objects at deeper nesting level in between our list, we save all pointers to objects along with their
        // nesting level in a vector container and once we found the next container (inside < last_inside) we
        // move the objects with higher nesting level to container and proceed. This works for any nesting depth.
        if (inside > 0 || (inside == 0 && inside < last_inside)) {
          //Found our container?
          if (inside < last_inside) {
            if (inside == 0)
              obj_to_room(obj, &world[rnum]);

            auto it = contained_obj.end();
            while ((it = std::find_if(contained_obj.begin(), contained_obj.end(), find_level(inside+1))) != contained_obj.end()) {
              obj_to_obj(it->obj, obj);
              contained_obj.erase(it);
            }

            if (inside > 0) {
              contained_obj_entry.level = inside;
              contained_obj_entry.obj = obj;
              contained_obj.push_back(contained_obj_entry);
            }
          }
          else {
            contained_obj_entry.level = inside;
            contained_obj_entry.obj = obj;
            contained_obj.push_back(contained_obj_entry);
          }
          last_inside = inside;
        }
        else
          obj_to_room(obj, &world[rnum]);

        last_inside = inside;
      }
      // This handles loading old house file format prior to introduction of version number in the file.
      // Version number will always be 0 for this format.
      else if (!house_version) {
        if (inside > 0) {
          if (inside == last_inside)
            last_obj = last_obj->in_obj;
          else if (inside < last_inside) {
            while (inside <= last_inside) {
              if (!last_obj) {
                snprintf(buf2, sizeof(buf2), "Load error: Nested-item load failed for %s. Disgorging to room.", GET_OBJ_NAME(obj));
                mudlog(buf2, NULL, LOG_SYSLOG, TRUE);
                break;
              }
              last_obj = last_obj->in_obj;
              last_inside--;
            }
          }

          if (last_obj)
            obj_to_obj(obj, last_obj);
          else
            obj_to_room(obj, &world[rnum]);
        }
        else
          obj_to_room(obj, &world[rnum]);

        last_inside = inside;
        last_obj = obj;
      }
      else {
        snprintf(buf2, sizeof(buf2), "Load ERROR: Unknown file format for house Rnum %ld. Dumping valid objects to room.", rnum);
        mudlog(buf2, NULL, LOG_SYSLOG, TRUE);
        obj_to_room(obj, &world[rnum]);
      }
    } else {
      snprintf(buf2, sizeof(buf2), "Losing object %ld (%s / %s; )- it's not a valid object.",
               vnum,
               data.GetString("HOUSE/Name", "no restring"),
               data.GetString("HOUSE/Photo", "no photo"));
      mudlog(buf2, NULL, LOG_SYSLOG, TRUE);
    }
  }
  if (house_version == VERSION_HOUSE_FILE) {
    // Failsafe. If something went wrong and we still have objects stored in the vector, dump them in the room.
    if (!contained_obj.empty()) {
      for (auto it : contained_obj)
        obj_to_room(it.obj, &world[rnum]);

      contained_obj.clear();
    }
  }
  return TRUE;
}

// Require that all objects link back to each other.
void validate_in_obj_pointers(struct obj_data *obj, struct obj_data *in_obj) {
  if (!obj)
    return;

  if (obj == in_obj) {
    const char *representation = generate_new_loggable_representation(obj);
    snprintf(buf3, sizeof(buf3), "SYSERR: Received duplicate items to validate_in_obj_pointers! Offending obj: '%s' in room %ld [OBJ_NESTING_ERROR_GREP_STRING]",
             representation,
             get_obj_in_room(obj) ? GET_ROOM_VNUM(get_obj_in_room(obj)) : -1
           );
    mudlog(buf3, NULL, LOG_SYSLOG, TRUE);
    delete [] representation;
    return;
  }

  // Cyberdeck parts do some WEIRD shit with in_obj. Best to leave it alone.
  if ((in_obj && GET_OBJ_TYPE(in_obj) == ITEM_PART) || (obj->in_obj && GET_OBJ_TYPE(obj->in_obj) == ITEM_PART))
    return;

  if (in_obj && obj->in_obj != in_obj) {
    const char *representation = generate_new_loggable_representation(obj);
    snprintf(buf3, sizeof(buf3), "^YSYSERR: in_obj mismatch for '%s' in room %ld! Sticking it back in '%s'... [OBJ_NESTING_ERROR_GREP_STRING]",
             representation,
             get_obj_in_room(obj) ? GET_ROOM_VNUM(get_obj_in_room(obj)) : -1,
             GET_OBJ_NAME(in_obj)
           );
    mudlog(buf3, NULL, LOG_SYSLOG, TRUE);
    delete [] representation;
    obj->in_obj = in_obj;
  }

  struct obj_data *previous = NULL;
  for (struct obj_data *tmp_obj = obj->contains; tmp_obj; tmp_obj = tmp_obj->next_content) {
    if (tmp_obj == obj) {
      // We don't use a loggable representation here since I can't guarantee how it would behave when given a bugged object like this.
      snprintf(buf3, sizeof(buf3), "^RSYSERR: Detected self-containing item '%s' (vnum %ld, room %ld) in validate_in_obj_pointers! There's no way to un-fuck this programmatically, so we're losing items. [OBJ_NESTING_ERROR_GREP_STRING]^n",
               GET_OBJ_NAME(obj),
               GET_OBJ_VNUM(obj),
               get_obj_in_room(obj) ? GET_ROOM_VNUM(get_obj_in_room(obj)) : -1
             );
      mudlog(buf3, NULL, LOG_SYSLOG, TRUE);
      if (previous)
        previous->next_content = NULL;
      obj->contains = NULL;
      return;
    }
    validate_in_obj_pointers(tmp_obj, obj);
  }
}

#define APPEND_IF_CHANGED(sectname, obj_val, proto_val) { \
  if (obj_val != proto_val)                                        \
    obj_string_buf << (sectname) << ((int) obj_val) << "\n";                            \
}
#define FILEBUF_SIZE 8192

void House_save(struct house_control_rec *house, const char *file_name, long rnum)
{
  int level = 0;
  struct obj_data *obj = NULL;

  FILE *fl;

  // Can't open the house file? Oof.
  if (!file_name || !*file_name || !(fl = fopen(file_name, "wb"))) {
    perror("SYSERR: Error saving house file");
    return;
  } else {
    // log_vfprintf("Saving house %s.", file_name);
  }

  char print_buffer[FILEBUF_SIZE];
  memset(print_buffer, 0, sizeof(print_buffer));
  setvbuf(fl, print_buffer, _IOFBF, FILEBUF_SIZE);

  fprintf(fl, "[METADATA]\n");
  fprintf(fl, "\tVersion:\t%d\n", VERSION_HOUSE_FILE);

  // Are we saving an apartment? If so, store guest list.
  if (house) {
    fprintf(fl, "[GUESTS]\n");
    for (int o = 0; o < MAX_GUESTS; o++) {
      // Don't write out empty guest slots.
      if (house->guests[o])
        fprintf(fl, "\tGuest%d:\t%ld\n", o, house->guests[o]);
    }
  }

  obj = world[rnum].contents;

  for (struct obj_data *tmp_obj = obj; tmp_obj; tmp_obj = tmp_obj->next_content) {
    validate_in_obj_pointers(tmp_obj, NULL);
  }

  fprintf(fl, "[HOUSE]\n");

  struct obj_data *prototype = NULL;
  int real_obj;
  std::vector<std::string> obj_strings;
  std::stringstream obj_string_buf;

  for (;obj;)
  {
    if ((real_obj = real_object(GET_OBJ_VNUM(obj))) == -1) {
      log_vfprintf("Warning: Will lose house item %s from %s (%ld) due to nonexistent rnum. [house_error_grep_string]",
               GET_OBJ_NAME(obj),
               GET_ROOM_NAME(obj->in_room),
               GET_ROOM_VNUM(obj->in_room));
      obj = obj->next_content;
      continue;
    }

    prototype = &obj_proto[real_obj];
    if (!IS_OBJ_STAT(obj, ITEM_EXTRA_NORENT)) {
      obj_string_buf << "\t\tVnum:\t" << GET_OBJ_VNUM(obj) << "\n";
      obj_string_buf << "\t\tInside:\t" << level << "\n";
      if (GET_OBJ_TYPE(obj) == ITEM_PHONE) {
        for (int x = 0; x < 4; x++)
          if (GET_OBJ_VAL(obj, x) != GET_OBJ_VAL(prototype, x))
            obj_string_buf << "\t\tValue " << x << ":\t" << GET_OBJ_VAL(obj, x) << "\n";
      }
      else if (GET_OBJ_TYPE(obj) != ITEM_WORN)
        for (int x = 0; x < NUM_VALUES; x++)
          if (GET_OBJ_VAL(obj, x) != GET_OBJ_VAL(prototype, x))
            obj_string_buf << "\t\tValue "<< x <<":\t" << GET_OBJ_VAL(obj, x) <<"\n";

      APPEND_IF_CHANGED("\t\tCondition:\t", GET_OBJ_CONDITION(obj), GET_OBJ_CONDITION(prototype));
      APPEND_IF_CHANGED("\t\tTimer:\t", GET_OBJ_TIMER(obj), GET_OBJ_TIMER(prototype));
      APPEND_IF_CHANGED("\t\tAttempt:\t", GET_OBJ_ATTEMPT(obj), 0);
      obj_string_buf << "\t\tCost:\t"<< GET_OBJ_COST(obj) << "\n";

      if (GET_OBJ_EXTRA(obj).ToString() != GET_OBJ_EXTRA(prototype).ToString())
        obj_string_buf << "\t\tExtraFlags:\t"<< GET_OBJ_EXTRA(obj).ToString() << "\n";
      if (obj->restring)
        obj_string_buf << "\t\tName:\t" << obj->restring << "\n";
      if (obj->photo)
        obj_string_buf << "\t\tPhoto:$\n" << cleanup(buf2, obj->photo) << "~\n";

      obj_strings.push_back(obj_string_buf.str());
      obj_string_buf.str(std::string());

      if (obj->contains && !IS_OBJ_STAT(obj, ITEM_EXTRA_NORENT) && GET_OBJ_TYPE(obj) != ITEM_PART) {
        obj = obj->contains;
        level++;

        continue;
      }
    } else {
      log_vfprintf("Discarding house item %s (%ld) from %s because it is !RENT. [house_error_grep_string]",
                   GET_OBJ_NAME(obj),
                   GET_OBJ_VNUM(obj),
                   file_name);
    }

    if (obj->next_content) {
      obj = obj->next_content;
      continue;
    }

    if (obj->in_obj)
      while (obj && !obj->next_content && level > 0) {
        obj = obj->in_obj;
        level--;
      }

    if (obj)
      obj = obj->next_content;
  }

  if (level != 0) {
    log_vfprintf("Warning: Non-zero level at finish when saving %s. [house_error_grep_string]", file_name);
  }

  if (!obj_strings.empty()) {
    int i = 0;
    for(std::vector<std::string>::reverse_iterator rit = obj_strings.rbegin(); rit != obj_strings.rend(); rit++ ) {
      fprintf(fl, "\t[Object %d]\n", i);
      fprintf(fl, "%s", rit->c_str());
      i++;
    }
    obj_strings.clear();
  }

  fclose(fl);
}
#undef FILEBUF_SIZE
#undef APPEND_IF_CHANGED

struct house_control_rec *find_house(vnum_t vnum)
{
  struct landlord *llord;
  struct house_control_rec *room;

  for (llord = landlords; llord; llord = llord->next)
    for (room = llord->rooms; room; room = room->next)
      if (room->vnum == vnum)
        return room;

  return NULL;
}

/* Save all objects in a house */
void House_crashsave(vnum_t vnum)
{
  int rnum;
  char buf[MAX_STRING_LENGTH];
  struct house_control_rec *house = NULL;
  struct obj_data *obj = NULL;

  // House does not exist? Skip.
  if ((rnum = real_room(vnum)) == NOWHERE)
    return;

  // If the dirty bit is not set, we don't save the room-- it means nobody was here.
  // YES, this does induce bugs, like evaluate programs not decaying if nobody is around to see it happen--
  // but fuck it, if someone exploits it we'll just ban them. Easy enough.
  if (!world[rnum].dirty_bit && !world[rnum].people) {
    // log_vfprintf("Skipping save for room %ld: Dirty bit is false and room has no occupants.", vnum);
    return;
  } else {
    // Clear the dirty bit now that we've processed it.
    world[rnum].dirty_bit = FALSE;
  }

  // Calculate whether or not we should keep this house.
  bool should_we_keep_this_file = FALSE;
  house = find_house(vnum);

  // Figure out if the house has any guests/items.
  if (house) {
    obj = world[real_room(house->vnum)].contents;

    for (int o = 0; o < MAX_GUESTS; o++) {
      // Don't write out empty guest slots.
      if (house->guests[o]) {
        should_we_keep_this_file = TRUE;
        break;
      }
    }
  } else {
    // Storage room- just check for items.
    obj = world[rnum].contents;
  }

  // Finalize decision for keeping.
  should_we_keep_this_file |= (obj != NULL);

  // House filename is not kosher? Skip. Otherwise, populate buf with filename.
  if (house) {
    if (!House_get_filename(vnum, buf, sizeof(buf))) {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Bailing on saving house %ld: Cannot generate filename!", vnum);
      return;
    }
  } else {
    if (!Storage_get_filename(vnum, buf, sizeof(buf))) {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Bailing on saving storage room %ld: Cannot generate filename!", vnum);
      return;
    }
  }

  // If it has no guests and no objects, why save it?
  if (!should_we_keep_this_file) {
    // log_vfprintf("Deleting file for house %ld: Has no guests and no contents.", vnum);
    House_delete_file(vnum, buf);
    return;
  }

  // Save it.
  House_save(find_house(vnum), buf, rnum);
}

/* Delete a house save file */
void House_delete_file(vnum_t vnum, char *name)
{
  char buf[MAX_INPUT_LENGTH];
  FILE *fl;

  if (!name || !*name)
    return;
  if (!(fl = fopen(name, "rb"))) {
    if (errno != ENOENT) {
      snprintf(buf, sizeof(buf), "SYSERR: Error deleting house file #%ld. (1)", vnum);
      perror(buf);
    }
    return;
  }
  fclose(fl);
  if (unlink(name) < 0) {
    snprintf(buf, sizeof(buf), "SYSERR: Error deleting house file #%ld. (2)", vnum);
    perror(buf);
  }
}

/******************************************************************
 *  Functions for house administration (creation, deletion, etc.  *
 *****************************************************************/


/* Save the house control information */
void House_save_control(void)
{
  struct house_control_rec *room;
  struct landlord *llord = landlords;
  int i = 0;
  FILE *fl;
  if (!(fl = fopen(HCONTROL_FILE, "w+b"))) {
    perror("SYSERR: Unable to open house control file");
    return;
  }
  for (;llord; llord = llord->next)
    i++;
  fprintf(fl, "%d\n", i);
  for (llord = landlords; llord; llord = llord->next) {
    fprintf(fl, "%ld %s %d %d\n", llord->vnum, llord->race.ToString(), llord->basecost, llord->num_room);
    for (room = llord->rooms; room; room = room->next)
      fprintf(fl, "%ld %ld %d %d %s %ld 0 %ld\n", room->vnum, room->key, room->exit_num, room->mode,
              room->name, room->owner, room->owner ? room->date : 0);
  }

  fclose(fl);
}

struct house_control_rec *find_room(char *arg, struct house_control_rec *room, struct char_data *recep)
{
  for (; room; room = room->next)
    if (isname(arg, room->name))
      return room;
  do_say(recep, "Which room is that?", 0, 0);
  return NULL;
}

bool ch_already_rents_here(struct house_control_rec *room, struct char_data *ch) {
  for (; room; room = room->next)
    if (room->owner == GET_IDNUM(ch))
      return TRUE;

  return FALSE;
}

void display_room_list_to_character(struct char_data *ch, struct landlord *lord) {
  bool printed_message_yet = FALSE;

  if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
    for (struct house_control_rec *room_record = lord->rooms; room_record; room_record = room_record->next) {
      if (!room_record->owner) {
        if (!printed_message_yet)
          send_to_char(ch, "The following rooms are free: \r\n");

        printed_message_yet = TRUE;
        send_to_char(ch, "Room %s (lifestyle %s): %d nuyen.\r\n",
                     room_record->name,
                     lifestyle[room_record->mode].name,
                     lord->basecost * lifestyle[room_record->mode].cost);
      }
    }

    if (!printed_message_yet) {
      send_to_char(ch, "It looks like all the rooms here have been claimed.\r\n");
    }
    return;
  }

  // Non-screenreader display.
  bool on_first_entry_in_column = TRUE;
  strlcpy(buf, "", sizeof(buf));
  for (struct house_control_rec *room_record = lord->rooms; room_record; room_record = room_record->next) {
    if (!room_record->owner) {
      if (!printed_message_yet) {
        send_to_char(ch, "The following rooms are free: \r\n");
        send_to_char(ch, "Name     Class     Price      Name     Class     Price\r\n");
        send_to_char(ch, "-----    ------    ------     -----    ------    -----\r\n");
      }

      printed_message_yet = TRUE;

      send_to_char(ch, "%s%-5s    %-6s    %-8d%s",
              on_first_entry_in_column ? "" : "   ",
              room_record->name,
              lifestyle[room_record->mode].name,
              lord->basecost * lifestyle[room_record->mode].cost,
              on_first_entry_in_column ? "" : "\r\n");
      on_first_entry_in_column = !on_first_entry_in_column;
    }
  }

  if (!printed_message_yet) {
    send_to_char("It looks like all the rooms here have been claimed.\r\n", ch);
  }
}

SPECIAL(landlord_spec)
{
  struct char_data *recep = (struct char_data *) me;
  struct obj_data *obj = NULL;
  struct landlord *lord;
  struct house_control_rec *room_record;
  char buf3[MAX_STRING_LENGTH];

  if (!(CMD_IS("list") || CMD_IS("retrieve") || CMD_IS("lease")
        || CMD_IS("leave") || CMD_IS("break") || CMD_IS("pay") || CMD_IS("status")))
    return FALSE;

  if (!CAN_SEE(recep, ch)) {
    do_say(recep, "I don't deal with people I can't see!", 0, 0);
    return TRUE;
  }
  for (lord = landlords; lord; lord = lord->next)
    if (GET_MOB_VNUM(recep) == lord->vnum)
      break;
  if (!lord)
    return FALSE;
  one_argument(argument, arg);

  if (CMD_IS("list")) {
    display_room_list_to_character(ch, lord);
    return TRUE;
  } else if (CMD_IS("retrieve")) {
    if (!*arg) {
      do_say(recep, "Which room would you like the key to?", 0, 0);
      return TRUE;
    }
    room_record = find_room(arg, lord->rooms, recep);
    if (!room_record)
      return TRUE;
    else if (room_record->owner != GET_IDNUM(ch))
      do_say(recep, "I would get fired if I did that.", 0, 0);
    else {
      do_say(recep, "Here you go...", 0, 0);
      struct obj_data *key = read_object(room_record->key, VIRTUAL);
      obj_to_char(key, ch);
    }
    return TRUE;
  } else if (CMD_IS("lease")) {
    if (!*arg) {
      do_say(recep, "Which room would you like to lease?", 0, 0);

      display_room_list_to_character(ch, lord);

      return TRUE;
    }
    room_record = find_room(arg, lord->rooms, recep);
    if (!room_record) {
      // this is already handled in find_room
      // do_say(recep, "Which room is that?", 0, 0);
      return TRUE;
    } else if (room_record->owner) {
      do_say(recep, "Sorry, I'm afraid that room is already taken.", 0, 0);
      return TRUE;
    } else if (ch_already_rents_here(lord->rooms, ch)) {
      do_say(recep, "Sorry, we only allow people to lease one room at a time here.", 0, 0);
      return TRUE;
    }
    int cost = lord->basecost * lifestyle[room_record->mode].cost, origcost = cost;

    snprintf(buf3, sizeof(buf3), "That will be %d nuyen.", cost);
    do_say(recep, buf3, 0, 0);
    for (obj = ch->carrying; obj; obj = obj->next_content)
      if (GET_OBJ_VNUM(obj) == 119 && GET_OBJ_VAL(obj, 0) == GET_IDNUM(ch)) {
        if (cost >= GET_OBJ_VAL(obj, 1))
          cost -= GET_OBJ_VAL(obj, 1);
        else
          cost = 0;
        break;
      }
    if (GET_NUYEN(ch) < cost) {
      if (GET_BANK(ch) >= cost) {
        do_say(recep, "You don't have the money on you, so I'll transfer it from your bank account.", 0, 0);
        lose_bank(ch, cost, NUYEN_OUTFLOW_HOUSING);
      } else {
        do_say(recep, "Sorry, you don't have the required funds.", 0, 0);
        return TRUE;
      }
    } else {
      lose_nuyen(ch, cost, NUYEN_OUTFLOW_HOUSING);
      send_to_char(ch, "You hand over the cash.\r\n");
    }
    if (obj) {
      if (origcost >= GET_OBJ_VAL(obj, 1)) {
        send_to_char("Your housing card is empty.\r\n", ch);
        extract_obj(obj);
      } else {
        GET_OBJ_VAL(obj, 1) -= origcost;
        send_to_char(ch, "Your housing card has %d nuyen left on it.\r\n", GET_OBJ_VAL(obj, 1));
      }
    }
    struct obj_data *key = read_object(room_record->key, VIRTUAL);
    if (!key) {
      snprintf(buf, sizeof(buf), "SYSERR: Room record for %ld specifies key vnum %ld, but it does not exist! '%s' (%ld) can't access %s apartment.",
              room_record->vnum, room_record->key, GET_CHAR_NAME(ch), GET_IDNUM(ch), HSHR(ch));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      do_say(recep, "I seem to have misplaced your key. I've refunded you the nuyen in cash.", 0, 0);
      GET_NUYEN_RAW(ch) += cost;
      GET_NUYEN_INCOME_THIS_PLAY_SESSION(ch, NUYEN_OUTFLOW_HOUSING) -= cost;
    } else {
      do_say(recep, "Thank you, here is your key.", 0, 0);
      obj_to_char(key, ch);
      room_record->owner = GET_IDNUM(ch);
      room_record->date = time(0) + (SECS_PER_REAL_DAY*30);

      int rnum = real_room(room_record->vnum);
      ROOM_FLAGS(&world[rnum]).SetBit(ROOM_HOUSE);
      ROOM_FLAGS(&world[room_record->atrium]).SetBit(ROOM_ATRIUM);

      // Shift all the vehicles out of it.
      remove_vehicles_from_apartment(&world[rnum]);

      House_crashsave(room_record->vnum);
      House_save_control();
    }
    return TRUE;
  } else if (CMD_IS("leave") || CMD_IS("break")) {
    if (!*arg) {
      do_say(recep, "Which room would you like to leave?", 0, 0);
      return TRUE;
    }
    room_record = find_room(arg, lord->rooms, recep);
    if (!room_record)
      do_say(recep, "Which room is that?", 0, 0);
    else if (room_record->owner != GET_IDNUM(ch))
      do_say(recep, "I would get fired if I did that.", 0, 0);
    else {
      room_record->owner = 0;
      int rnum = real_room(room_record->vnum);
      ROOM_FLAGS(&world[rnum]).RemoveBit(ROOM_HOUSE);
      remove_vehicles_from_apartment(&world[rnum]);

      // Check to see if there are any other houses associated with this atrium. If not, disassociate it.
      {
        struct room_data *atrium = &world[room_record->atrium];
        bool have_other_houses = FALSE;

        for (int i = NORTH; i <= DOWN && !have_other_houses; i++) {
          if (atrium->dir_option[i] && atrium->dir_option[i]->to_room && ROOM_FLAGGED(atrium->dir_option[i]->to_room, ROOM_HOUSE))
            have_other_houses = TRUE;
        }

        if (!have_other_houses)
          ROOM_FLAGS(atrium).RemoveBit(ROOM_ATRIUM);
      }

      House_save_control();
      House_get_filename(room_record->vnum, buf2, MAX_STRING_LENGTH);
      House_delete_file(room_record->vnum, buf2);
      do_say(recep, "I hope you enjoyed your time here.", 0, 0);
    }
    return TRUE;
  } else if (CMD_IS("pay")) {
    if (!*arg) {
      do_say(recep, "Which room would you like to pay the rent to?", 0, 0);
      return TRUE;
    }
    room_record = find_room(arg, lord->rooms, recep);
    if (!room_record)
      do_say(recep, "Which room is that?", 0, 0);
    else if (room_record->owner != GET_IDNUM(ch))
      do_say(recep, "But that isn't your room.", 0, 0);
    else {
      int cost = lord->basecost * lifestyle[room_record->mode].cost, origcost = cost;
      snprintf(buf3, sizeof(buf3), "That will be %d nuyen.", cost);
      do_say(recep, buf3, 0, 0);
      for (obj = ch->carrying; obj; obj = obj->next_content)
        if (GET_OBJ_VNUM(obj) == 119 && GET_OBJ_VAL(obj, 0) == GET_IDNUM(ch)) {
          if (cost >= GET_OBJ_VAL(obj, 1))
            cost -= GET_OBJ_VAL(obj, 1);
          else
            cost = 0;
          break;
        }
      if (GET_NUYEN(ch) < cost) {
        if (GET_BANK(ch) >= cost) {
          do_say(recep, "You don't have the money on you, so I'll transfer it from your bank account.", 0, 0);
          lose_bank(ch, cost, NUYEN_OUTFLOW_HOUSING);
        } else {
          do_say(recep, "Sorry, you don't have the required funds.", 0, 0);
          return TRUE;
        }
      } else {
        lose_nuyen(ch, cost, NUYEN_OUTFLOW_HOUSING);
        send_to_char(ch, "You hand over the cash.\r\n");
      }
      if (obj) {
        if (origcost >= GET_OBJ_VAL(obj, 1)) {
          send_to_char("Your housing card is empty.\r\n", ch);
          extract_obj(obj);
        } else {
          GET_OBJ_VAL(obj, 1) -= origcost;
          GET_NUYEN_INCOME_THIS_PLAY_SESSION(ch, NUYEN_OUTFLOW_HOUSING) += cost;
          send_to_char(ch, "Your housing card has %d nuyen left on it.\r\n", GET_OBJ_VAL(obj, 1));
        }
      }
      do_say(recep, "You are paid up for the next period.", 0, 0);
      room_record->date += SECS_PER_REAL_DAY*30;
      House_save_control();
    }
    return TRUE;
  } else if (CMD_IS("status")) {
    if (!*arg) {
      do_say(recep, "Which room would you like to check the rent on?", 0, 0);
      return TRUE;
    }
    room_record = find_room(arg, lord->rooms, recep);
    if (!room_record)
      do_say(recep, "Which room is that?", 0, 0);
    else if (room_record->owner && room_record->owner != GET_IDNUM(ch))
      do_say(recep, "I'm afraid I can't release that information.", 0, 0);
    else if (!room_record->owner)
      do_say(recep, "That room is currently available for lease.", 0, 0);
    else {
      if (room_record->date - time(0) < 0) {
        strlcpy(buf2, "Your rent has expired on that apartment.", sizeof(buf2));
        do_say(recep, buf2, 0, 0);
      } else {
        snprintf(buf2, sizeof(buf2), "You are paid for another %d days.", (int)((room_record->date - time(0)) / 86400));
        do_say(recep, buf2, 0, 0);
        strlcpy(buf2, "Note: Those are real-world days.", sizeof(buf2));
        do_say(recep, buf2, 0, SCMD_OSAY);
      }
    }
    return TRUE;
  }
  return FALSE;
}
/* call from boot_db - will load control recs, load objs, set atrium bits */
/* should do sanity checks on vnums & remove invalid records */
// aka boot_houses
void House_boot(void)
{
  vnum_t house_vnum, owner, landlord_vnum, paid, key_vnum;
  FILE *fl;
  int num_land;
  char line[256], name[20];
  int t[4];
  struct house_control_rec *temp = NULL;
  struct landlord *templ = NULL, *lastl = NULL;
  if (!(fl = fopen(HCONTROL_FILE, "r+b"))) {
    log("House control file does not exist.");
    return;
  }
  if (!get_line(fl, line) || sscanf(line, "%d", &num_land) != 1) {
    log("Error at beginning of house control file.");
    return;
  }

  // Go through each apartment complex entry.
  for (int i = 0; i < num_land; i++) {
    get_line(fl, line);
    if (sscanf(line, "%ld %s %d %d", &landlord_vnum, name, t, t + 1) != 4) {
      fprintf(stderr, "Format error in landlord #%d.\r\n", i);
      return;
    }
    if (real_mobile(landlord_vnum) < 0) {
      log_vfprintf("SYSERR: Landlord vnum %ld does not match up with a real NPC. Terminating.\r\n", landlord_vnum);
      exit(ERROR_CANNOT_RESOLVE_VNUM);
    }
    ASSIGNMOB(landlord_vnum, landlord_spec);
    templ = new struct landlord;
    templ->vnum = landlord_vnum;
    templ->race.FromString(name);
    templ->basecost = t[0];
    templ->num_room = t[1];
    struct house_control_rec *last = NULL, *first = NULL;

    // Go through each room in the apartment complex entry.
    for (int x = 0; x < templ->num_room; x++) {
      get_line(fl, line);
      if (sscanf(line, "%ld %ld %d %d %s %ld %d %ld", &house_vnum, &key_vnum, t, t + 1, name,
                 &owner, t + 2, &paid) != 8) {
        fprintf(stderr, "Format error in landlord #%d room #%d.\r\n", i, x);
        return;
      }

      rnum_t house_rnum = real_room(house_vnum);

      if (house_rnum < 0) {
        log_vfprintf("SYSERR: House vnum %ld does not match up with a real room. Terminating.\r\n", landlord_vnum);
        exit(ERROR_CANNOT_RESOLVE_VNUM);
      }

      struct room_data *apartment = &world[house_rnum];

      temp = new struct house_control_rec;
      temp->vnum = house_vnum;
      temp->key = key_vnum;
      temp->atrium = TOROOM(house_rnum, t[0]);
      if (temp->atrium == NOWHERE) {
        log_vfprintf("You have an error in your house control file-- there is no valid %s (%d) exit for room %ld.",
                     dirs[t[0]], t[0], temp->vnum);
      }
      temp->exit_num = t[0];
      temp->owner = owner;
      temp->date = paid;
      temp->mode = t[1];
      temp->name = str_dup(name);
      for (int q = 0; q < MAX_GUESTS; q++)
        temp->guests[q] = 0;

      if (temp->owner) {
        // Regardless of whether or not the owner still exists, we load the house's contents.
        House_load(temp);

        // Now, if the owner is not valid, we purge the contents.
        if (temp->date <= time(0) || !does_player_exist(temp->owner) || player_is_dead_hardcore(temp->owner)) {
          temp->owner = 0;
          House_get_filename(temp->vnum, buf2, MAX_STRING_LENGTH);
          House_delete_file(temp->vnum, buf2);

          // Move all vehicles from this apartment to a public garage.
          remove_vehicles_from_apartment(apartment);



          // Purge the apartment, logging it.
          for (struct obj_data *obj = apartment->contents, *next_o; obj; obj = next_o) {
            next_o = obj->next_content;
            const char *representation = generate_new_loggable_representation(obj);
            mudlog_vfprintf(NULL, LOG_PURGELOG, "Apartment cleanup for %ld has purged %s.", temp->vnum, representation);
            delete [] representation;
            extract_obj(obj);
          }

          // Purge attached storage rooms.
          for (int dir = 0; dir < NUM_OF_DIRS; dir++) {
            if (EXIT2(apartment, dir)) {
              struct room_data *the_room = EXIT2(apartment, dir)->to_room;

              if (the_room && ROOM_FLAGGED(the_room, ROOM_STORAGE)) {
                for (struct obj_data *obj = the_room->contents, *next_o; obj; obj = next_o) {
                  next_o = obj->next_content;
                  const char *representation = generate_new_loggable_representation(obj);
                  mudlog_vfprintf(NULL, LOG_PURGELOG, "Apartment cleanup for %ld (attached storage room: %ld) has purged %s.", temp->vnum, GET_ROOM_VNUM(the_room), representation);
                  delete [] representation;
                  extract_obj(obj);
                }
              }
            }
          }
        }
      }
      if (last)
        last->next = temp;
      if (!first)
        first = temp;
      if (temp->owner) {
        ROOM_FLAGS(apartment).SetBit(ROOM_HOUSE);
        ROOM_FLAGS(&world[temp->atrium]).SetBit(ROOM_ATRIUM);
      }
      last = temp;
    }
    templ->rooms = first;
    templ->next = lastl;
    lastl = templ;
  }
  landlords = templ;

  fclose(fl);
  House_save_control();

  warn_about_apartment_deletion();
}

/* "House Control" functions */

int count_objects(struct obj_data *obj) {
  if (!obj)
    return 0;

  int total = 1; // self

  for (struct obj_data *contents = obj->contains; contents; contents = contents->next_content)
    total += count_objects(contents);

  return total;
}

const char *HCONTROL_FORMAT =
  "Usage:  hcontrol destroy <house vnum>\r\n"
  "       hcontrol show\r\n";
void hcontrol_list_houses(struct char_data *ch)
{
  char *own_name;

  strlcpy(buf, "Address  Atrium  Guests  Owner           Crap Count\r\n", sizeof(buf));
  strlcat(buf, "-------  ------  ------  --------------  -----------\r\n", sizeof(buf));
  send_to_char(buf, ch);

  int global_total_apartment_crap = 0;

  for (struct landlord *llord = landlords; llord; llord = llord->next)
    for (struct house_control_rec *house = llord->rooms; house; house = house->next)
      if (house->owner)
      {
        int house_rnum = real_room(house->vnum);
        int total = 0;

        // Count the crap in it, if it exists.
        if (house_rnum > -1) {
          for (struct obj_data *obj = world[house_rnum].contents; obj; obj = obj->next_content)
            total += count_objects(obj);
          for (struct veh_data *veh = world[house_rnum].vehicles; veh; veh = veh->next_veh) {
            total += 1;
            for (struct obj_data *obj = veh->contents; obj; obj = obj->next_content)
              total += count_objects(obj);
          }
          global_total_apartment_crap += total;
        } else
          total = -1;

        // Get the number of objects in it.
        own_name = get_player_name(house->owner);
        if (!own_name)
          own_name = str_dup("<UNDEF>");

        // Get the number of guests in it.
        int guest_num = 0;
        for (int j = 0; j < MAX_GUESTS; j++) {
          if (house->guests[j] != 0)
            guest_num++;
        }

        // Output the line.
        snprintf(buf, sizeof(buf), "%7ld %7ld    %2d    %-14s  %d\r\n",
                house->vnum, world[house->atrium].number, guest_num, CAP(own_name), total);
        send_to_char(buf, ch);
        DELETE_ARRAY_IF_EXTANT(own_name);
      }

  send_to_char(ch, "Global total crap: %d items.\r\n", global_total_apartment_crap);
}

void hcontrol_destroy_house(struct char_data * ch, char *arg)
{
  struct house_control_rec *i = NULL;
  vnum_t real_atrium, real_house;

  if (!*arg)
  {
    send_to_char(HCONTROL_FORMAT, ch);
    return;
  }
  if ((i = find_house(atoi(arg))) == NULL)
  {
    send_to_char("Unknown house.\r\n", ch);
    return;
  }
  if ((real_atrium = real_room(i->atrium)) < 0)
    log("SYSERR: House had invalid atrium!");
  else
    ROOM_FLAGS(&world[real_atrium]).RemoveBit(ROOM_ATRIUM);

  if ((real_house = real_room(i->vnum)) < 0)
    log("SYSERR: House had invalid vnum!");
  else
  {
    ROOM_FLAGS(&world[real_house]).RemoveBit(ROOM_HOUSE);
    remove_vehicles_from_apartment(&world[real_house]);
    House_get_filename(i->vnum, buf2, MAX_STRING_LENGTH);
    House_delete_file(i->vnum, buf2);
    i->owner = 0;
    send_to_char("House deleted.\r\n", ch);
    House_save_control();

    for (struct landlord *llord = landlords; llord; llord = llord->next)
      for (i = llord->rooms; i; i = i->next)
        if (i->atrium >= 0) {
          ROOM_FLAGS(&world[real_atrium]).SetBit(ROOM_ATRIUM);
        }
  }
}

void hcontrol_display_house_by_number(struct char_data * ch, vnum_t house_number) {
  struct house_control_rec *i = NULL;
  vnum_t real_house;

  if (!(i = find_house(house_number))) {
    send_to_char(ch, "There is no house numbered %ld.\r\n", house_number);
    return;
  }

  if ((real_house = real_room(i->vnum)) < 0) {
    send_to_char(ch, "House %ld has an invalid room! Bailing out.\r\n", house_number);
    return;
  }

  const char *player_name = get_player_name(i->owner);
  send_to_char(ch, "House ^c%ld^n (^c%s^n) is owned by ^c%s^n (^c%ld^n).\r\n",
               i->vnum,
               GET_ROOM_NAME(&world[real_house]),
               player_name,
               i->owner
             );
  if (player_name)
    delete [] player_name;

  send_to_char(ch, "It is paid up until epoch ^c%ld^n.\r\n", i->date);

  send_to_char(ch, "Guests:\r\n");
  bool printed_guest_yet = FALSE;
  for (int guest_idx = 0; guest_idx < MAX_GUESTS; guest_idx++) {
    if (i->guests[guest_idx]) {
      printed_guest_yet = TRUE;
      const char *player_name = get_player_name(i->guests[guest_idx]);
      send_to_char(ch, "  ^c%s^n (^c%ld^n)\r\n", player_name, i->guests[guest_idx]);
      if (player_name)
        delete [] player_name;
    }
  }
  if (!printed_guest_yet)
    send_to_char("  ^cNone.^n\r\n", ch);

  int obj_count = 0, veh_count = 0;
  for (struct obj_data *obj = world[real_house].contents; obj; obj = obj->next_content)
    obj_count += count_objects(obj);
  for (struct veh_data *veh = world[real_house].vehicles; veh; veh = veh->next_veh) {
    veh_count ++;
    for (struct obj_data *obj = veh->contents; obj; obj = obj->next_content)
      obj_count += count_objects(obj);
  }
  send_to_char(ch, "It contains ^c%d^n objects and ^c%d^n vehicles.\r\n", obj_count, veh_count);
}

void hcontrol_display_house_with_owner_or_guest(struct char_data * ch, const char *name, vnum_t idnum) {
  send_to_char(ch, "Fetching data for owner/guest ^c%s^n (^c%ld^n)...\r\n", name, idnum);

  bool printed_something = FALSE;
  for (struct landlord *llord = landlords; llord; llord = llord->next) {
    for (struct house_control_rec *house = llord->rooms; house; house = house->next) {
      if (house->owner == idnum) {
        send_to_char(ch, "- Owner of house ^c%ld^n.\r\n", house->vnum);
        printed_something = TRUE;
      }

      else {
        for (int guest_idx = 0; guest_idx < MAX_GUESTS; guest_idx++) {
          if (house->guests[guest_idx] == idnum) {
            const char *player_name = get_player_name(house->owner);
            send_to_char(ch, "- Guest at house ^c%ld^n, owned by ^c%s^n (^c%ld^n).\r\n", house->vnum, player_name, house->owner);
            printed_something = TRUE;
            if (player_name)
              delete [] player_name;
            break;
          }
        }
      }
    }
  }

  if (!printed_something) {
    send_to_char("- No records found.\r\n", ch);
  }
}

/* The hcontrol command itself, used by imms to create/destroy houses */
ACMD(do_hcontrol)
{
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  int house_number;
  vnum_t idnum;

  skip_spaces(&argument);
  half_chop(argument, arg1, arg2);

  if (is_abbrev(arg1, "destroy")) {
    if (GET_LEVEL(ch) >= LVL_EXECUTIVE) {
      hcontrol_destroy_house(ch, arg2);
    } else {
      send_to_char("Sorry, you can't do that at your level.\r\n", ch);
    }
    return;
  }

  if (is_abbrev(arg1, "show")) {
    // With no argument, we default to the standard behavior.
    if (!*arg2) {
      hcontrol_list_houses(ch);
      return;
    }

    // If the argument is an int, we assume you want to know about a specific house.
    if ((house_number = atoi(arg2)) > 0) {
      hcontrol_display_house_by_number(ch, house_number);
      return;
    }

    // Otherwise, it's assumed to be a character name. Look up their houses and also houses where they're a guest.
    if ((idnum = get_player_id(arg2)) > 0) {
      hcontrol_display_house_with_owner_or_guest(ch, capitalize(arg2), idnum);
    } else {
      send_to_char(ch, "There is no player named '%s'.\r\n", arg2);
    }

    return;
  }

  // No valid command found.
  send_to_char("Usage: hcontrol destroy <apartment number>; or hcontrol show; or hcontrol show (<apartment number> | <character name>).\r\n", ch);
}


ACMD(do_decorate)
{
  extern void write_world_to_disk(int vnum);
  struct house_control_rec *house_record = NULL;

  FAILURE_CASE(!ch->in_room || ch->in_veh, "You can't decorate the interior of vehicles, but you can still customize the outside by visiting a painting booth.\r\n");

  // You can perform this action from the house proper, or from a storage room attached to it.
  if (!ROOM_FLAGGED(ch->in_room, ROOM_HOUSE)) {
    FAILURE_CASE(!ROOM_FLAGGED(ch->in_room, ROOM_STORAGE), "You can only decorate apartments and garages.\r\n");

    for (int dir = 0; dir < NUM_OF_DIRS; dir++) {
      struct room_data *the_room = NULL;
      if (EXIT2(ch->in_room, dir) && (the_room = EXIT2(ch->in_room, dir)->to_room) && ROOM_FLAGGED(the_room, ROOM_HOUSE)) {
        house_record = find_house(GET_ROOM_VNUM(the_room));
        break;
      }
    }
  } else {
    house_record = find_house(GET_ROOM_VNUM(ch->in_room));
  }

  FAILURE_CASE(!house_record, "You must be in an apartment or garage to set the description.\r\n");

  FAILURE_CASE(GET_IDNUM(ch) != house_record->owner, "Only the primary owner can set the room description.\r\n");

  PLR_FLAGS(ch).SetBit(PLR_WRITING);
  send_to_char("Enter your new room description.  Terminate with a @ on a new line.\r\n", ch);
  act("$n starts to move things around the room.", TRUE, ch, 0, 0, TO_ROOM);
  STATE(ch->desc) = CON_DECORATE;
  DELETE_D_STR_IF_EXTANT(ch->desc);
  INITIALIZE_NEW_D_STR(ch->desc);
  ch->desc->max_str = MAX_MESSAGE_LENGTH;
  ch->desc->mail_to = 0;
}

/* The house command, used by mortal house owners to assign guests */
ACMD(do_house)
{
  struct house_control_rec *i = NULL;
  int j, id;

  one_argument(argument, arg);

  if (!ROOM_FLAGGED(ch->in_room, ROOM_HOUSE))
    send_to_char("You must be in your house to set guests.\r\n", ch);
  else if ((i = find_house(GET_ROOM_VNUM(ch->in_room))) == NULL) {
    send_to_char("Um.. this house seems to be screwed up.\r\n", ch);
    snprintf(buf, sizeof(buf), "SYSERR: Failed to find_house() on %s (%ld)!", GET_ROOM_NAME(ch->in_room), GET_ROOM_VNUM(ch->in_room));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
  } else if (GET_IDNUM(ch) != i->owner && !access_level(ch, LVL_PRESIDENT))
    send_to_char("Only the primary owner can set guests.\r\n", ch);
  else if (!*arg)
    House_list_guests(ch, i, FALSE);
  else if ((id = get_player_id(arg)) < 0)
    send_to_char("No such player.\r\n", ch);
  else if (id == GET_IDNUM(ch))
    send_to_char("It's your house!\r\n", ch);
  else {
    for (j = 0; j < MAX_GUESTS; j++)
      if (i->guests[j] == id) {
        i->guests[j] = 0;
        House_crashsave(i->vnum);
        send_to_char("Guest deleted.\r\n", ch);
        return;
      }
    for (j = 0; j < MAX_GUESTS; j++)
      if (i->guests[j] == 0)
        break;
    if (j == MAX_GUESTS) {
      send_to_char("You have too many guests already.\r\n", ch);
      return;
    }
    i->guests[j] = id;
    House_crashsave(i->vnum);
    send_to_char("Guest added.\r\n", ch);
  }
}



/* Misc. administrative functions */


/* crash-save all the houses */
void House_save_all(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct landlord *llord;
  struct house_control_rec *room;
  rnum_t real_house;

  for (llord = landlords; llord; llord = llord->next)
    for (room = llord->rooms; room; room = room->next)
      if ((real_house = real_room(room->vnum)) != NOWHERE)
        if (room->owner)
          House_crashsave(room->vnum);

  for (int x = 0; x <= top_of_world; x++)
    if (ROOM_FLAGGED(&world[x], ROOM_STORAGE)) {
      // TODO: This is kinda inefficient (a bunch of binary searches instead of passing through)
      House_crashsave(world[x].number);
    }
}

bool House_can_enter_by_idnum(long idnum, vnum_t house) {
  struct house_control_rec *room = find_house(house);

  if (idnum <= 0 || !room)
    return FALSE;

  if (idnum == room->owner)
    return TRUE;

  for (int guest = 0; guest < MAX_GUESTS; guest++)
    if (idnum == room->guests[guest])
      return TRUE;

  return FALSE;
}


/* note: arg passed must be house vnum, so there. */
bool House_can_enter(struct char_data *ch, vnum_t house)
{
  int j;
  struct house_control_rec *room = find_house(house);

  // No room? No entry.
  if (!room)
    return FALSE;

  // NPC, but not a spirit or elemental? No entry.
  if (IS_NPC(ch) && !(IS_SPIRIT(ch) || IS_PC_CONJURED_ELEMENTAL(ch)))
    return FALSE;

  // Admins, astral projections, and owners can enter any room.
  if (GET_LEVEL(ch) >= LVL_ADMIN || GET_IDNUM(ch) == room->owner || IS_ASTRAL(ch))
    return TRUE;

  // Guests can enter any room.
  for (j = 0; j < MAX_GUESTS; j++)
    if (GET_IDNUM(ch) == room->guests[j])
      return TRUE;

#ifdef IS_BUILDPORT
  if (GET_LEVEL(ch) >= LVL_BUILDER) {
    send_to_char("(OOC: Entry allowed via staff override.)\r\n", ch);
    return TRUE;
  }
#endif

  return FALSE;
}

void House_list_guests(struct char_data *ch, struct house_control_rec *i, int quiet)
{
  int j;
  char buf[MAX_STRING_LENGTH], buf2[MAX_NAME_LENGTH + 2];

  strlcpy(buf, "  Guests: ", sizeof(buf));
  int x = 0;
  for (j = 0; j < MAX_GUESTS; j++)
  {
    if (i->guests[j] == 0)
      continue;

    const char *temp = get_player_name(i->guests[j]);

    if (!temp)
      continue;

    snprintf(buf2, sizeof(buf2), "%s, ", temp);
    strlcat(buf, CAP(buf2), sizeof(buf));
    x++;
    DELETE_ARRAY_IF_EXTANT(temp);
  }
  if (!x)
    strlcat(buf, "None", sizeof(buf));
  strlcat(buf, "\r\n", sizeof(buf));
  send_to_char(buf, ch);
}

void remove_vehicles_from_apartment(struct room_data *room) {
  struct veh_data *veh;
  while ((veh = room->vehicles)) {
    veh_from_room(veh);
    veh_to_room(veh, &world[real_room(RM_SEATTLE_PARKING_GARAGE)]);
  }
  save_vehicles(FALSE);
}

void warn_about_apartment_deletion() {
  //log("Beginning apartment deletion warning cycle.");
  for (struct landlord *llord = landlords; llord; llord = llord->next) {
    for (struct house_control_rec *house = llord->rooms; house; house = house->next) {
      if (!house->owner) {
        //log_vfprintf("No owner for %s.", house->name);
        continue;
      }

      int days_until_deletion = (house->date - time(0)) / (60 * 60 * 24);

      if (days_until_deletion <= 5 && days_until_deletion >= 0) {
        mudlog_vfprintf(NULL, LOG_GRIDLOG, "Sending %d-day rent warning message for apartment %s to %ld.", days_until_deletion, house->name, house->owner);
        if (days_until_deletion == 0) {
          snprintf(buf, sizeof(buf), "Remember to pay your rent for apartment %s! It will be deemed abandoned and its contents reclaimed at any time.\r\n", house->name);
        } else {
          snprintf(buf, sizeof(buf), "Remember to pay your rent for apartment %s. It will be deemed abandoned and its contents reclaimed in %d days.\r\n", house->name, days_until_deletion);
        }
        raw_store_mail(house->owner, 0, "Your landlord", buf);
      } else {
        //log_vfprintf("House %s is OK-- %d days left (%d - %d)", house->name, days_until_deletion, house->date, time(0));
      }
    }
  }
  //log("Apartment deletion warning cycle complete.");
}
