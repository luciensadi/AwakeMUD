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
extern void auto_repair_obj(struct obj_data *obj);
extern void handle_weapon_attachments(struct obj_data *obj);
extern void raw_store_mail(long to, long from_id, const char *from_name, const char *message_pointer);
extern bool player_is_dead_hardcore(long id);

struct landlord *landlords = NULL;
ACMD_CONST(do_say);

extern void remove_vehicles_from_apartment(struct room_data *room);
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
bool House_load_storage(struct room_data *world_room, const char *filename)
{
  File fl;
  struct obj_data *obj = NULL, *last_obj = NULL;
  int inside = 0, last_inside = 0;
  int house_version = 0;
  std::vector<nested_obj> contained_obj;
  struct nested_obj contained_obj_entry;

  if (!(fl.Open(filename, "r+b"))) { /* no file found */
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Not loading house %ld: File not found.", GET_ROOM_VNUM(world_room));
    return FALSE;
  }

  log_vfprintf("Loading house file %s.", filename);
  VTable data;
  data.Parse(&fl);
  fl.Close();
  snprintf(buf3, sizeof(buf3), "house-load %s", filename);
  house_version = data.GetInt("METADATA/Version", 0);

  for (int i = 0; i < MAX_GUESTS; i++) {
    snprintf(buf, sizeof(buf), "GUESTS/Guest%d", i);
    data.GetLong(buf, 0);
    // We discard this data - guests are handled in newhouse.
  }

  int num_objs = data.NumSubsections("HOUSE");
  int x;
  for (int i = 0; i < num_objs; i++)
  {
    const char *sect_name = data.GetIndexSection("HOUSE", i);
    snprintf(buf, sizeof(buf), "%s/Vnum", sect_name);
    vnum_t vnum = data.GetLong(buf, 0);
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
        auto_repair_obj(obj);

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
              obj_to_room(obj, world_room);

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
          obj_to_room(obj, world_room);

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
            obj_to_room(obj, world_room);
        }
        else
          obj_to_room(obj, world_room);

        last_inside = inside;
        last_obj = obj;
      }
      else {
        snprintf(buf2, sizeof(buf2), "Load ERROR: Unknown file format for house %ld. Dumping valid objects to room.", GET_ROOM_VNUM(world_room));
        mudlog(buf2, NULL, LOG_SYSLOG, TRUE);
        obj_to_room(obj, world_room);
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
        obj_to_room(it.obj, world_room);

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

void Storage_save(const char *file_name, struct room_data *room) {
  int level = 0;
  struct obj_data *obj = NULL;

  FILE *fl;

  // Can't open the house file? Oof.
  if (!file_name || !*file_name || !(fl = fopen(file_name, "wb"))) {
    perror("SYSERR: Error saving house file");
    return;
  } else {
    // log_vfprintf("Saving storage room %s.", file_name);
  }

  char print_buffer[FILEBUF_SIZE];
  memset(print_buffer, 0, sizeof(print_buffer));
  setvbuf(fl, print_buffer, _IOFBF, FILEBUF_SIZE);

  fprintf(fl, "[METADATA]\n");
  fprintf(fl, "\tVersion:\t%d\n", VERSION_HOUSE_FILE);

  obj = room->contents;

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


/* Misc. administrative functions */


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

void warn_about_apartment_deletion() {
  //log("Beginning apartment deletion warning cycle.");
  for (struct landlord *llord = landlords; llord; llord = llord->next) {
    for (struct house_control_rec *house = llord->rooms; house; house = house->next) {
      if (!house->owner) {
        //log_vfprintf("No owner for %s.", house->name);
        continue;
      }

      int days_until_deletion = (house->date - time(0)) / (60 * 60 * 24);

      if (days_until_deletion <= 5) {
        mudlog_vfprintf(NULL, LOG_GRIDLOG, "Sending %d-day rent warning message for apartment %s to %ld.", days_until_deletion, house->name, house->owner);
        if (days_until_deletion > 0) {
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
