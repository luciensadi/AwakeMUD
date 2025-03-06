#include <string.h>
#include <vector>
#include <unordered_set>

#include "structs.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "matrix_storage.hpp"
#include "interpreter.hpp"
#include "newmatrix.hpp"
#include "db.hpp"
#include "handler.hpp"
#include "constants.hpp"

// Function to get a unique ID
std::atomic<long> matrix_file_id_counter{1};
int _next_matrix_id() {
    return matrix_file_id_counter.fetch_add(1, std::memory_order_relaxed);
}

std::vector<struct obj_data*> get_storage_devices(struct char_data *ch, bool only_relevant = FALSE) {
  struct obj_data* cyber;
  std::vector<struct obj_data*> found_list = {};

  cyber = find_cyberware(ch, CYB_MEMORY);
  if (cyber) {
    found_list.push_back(cyber);
  }

  if (only_relevant && AFF_FLAGGED(ch, PLR_MATRIX)) {
    /* When someone is decking, the only thing that matters is their headware memory and deck. */
    if (ch->persona && ch->persona->decker) {
      if (ch->persona->decker->deck && !ch->persona->decker->deck->obj_flags.extra_flags.IsSet(ITEM_EXTRA_OTAKU_RESONANCE))
        found_list.push_back(ch->persona->decker->deck);
      if (ch->persona->decker->proxy_deck)
        found_list.push_back(ch->persona->decker->proxy_deck);
    }
    return found_list;
  }

  cyber = find_cyberware(ch, CYB_CRANIALCYBER);
  if (cyber) {
    found_list.push_back(cyber);
  }

  for (cyber = ch->carrying; cyber; cyber = cyber->next_content)
    if ((GET_OBJ_TYPE(cyber) == ITEM_CYBERDECK || GET_OBJ_TYPE(cyber) == ITEM_CUSTOM_DECK) && (IS_SENATOR(ch) || !IS_OBJ_STAT(cyber, ITEM_EXTRA_STAFF_ONLY)))
      found_list.push_back(cyber);
  for (int i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i) && (GET_OBJ_TYPE(GET_EQ(ch,i )) == ITEM_CYBERDECK || GET_OBJ_TYPE(GET_EQ(ch,i )) == ITEM_CUSTOM_DECK))
      found_list.push_back(GET_EQ(ch, i));
  // Decking accessories / computers
  for (cyber = (ch)->in_room ? (ch)->in_room->contents : (ch)->in_veh->contents; cyber; cyber = cyber->next_content)
    if (GET_OBJ_TYPE(cyber) == ITEM_DECK_ACCESSORY && GET_DECK_ACCESSORY_TYPE(cyber) == TYPE_COMPUTER)
      found_list.push_back(cyber);  

  // Deduplicating any repeat pointers.
  std::unordered_set<obj_data*> unique_set(found_list.begin(), found_list.end());
  found_list.assign(unique_set.begin(), unique_set.end());
  return found_list;
}

matrix_file* create_matrix_file(obj_data *storage, int load_origin) {
  struct matrix_file *new_file = new matrix_file();
  new_file->idnum = _next_matrix_id();
  new_file->load_origin = load_origin;
  new_file->in_obj = storage;

  new_file->next_file = storage->files;
  storage->files = new_file;

  return new_file;
}

matrix_file* obj_to_matrix_file(obj_data *prog) {
  struct matrix_file *new_file = create_matrix_file(prog->in_obj, prog->load_origin);

  new_file->attack_damage = GET_PROGRAM_ATTACK_DAMAGE(prog);
  new_file->name = strdup(prog->restring);
  new_file->rating = GET_PROGRAM_RATING(prog);
  new_file->file_type = GET_PROGRAM_TYPE(prog);
  new_file->work_ticks_left = GET_OBJ_VAL(prog, 4);
  new_file->is_default = GET_PROGRAM_IS_DEFAULTED(prog);
  new_file->work_original_ticks_left = GET_OBJ_TIMER(prog);
  new_file->work_phase = GET_DESIGN_COMPLETED(prog) ? WORK_PHASE_DESIGN : WORK_PHASE_NONE;
  new_file->work_successes = GET_DESIGN_SUCCESSES(prog);
  
  return new_file;
}

#define CHECK_KEYWORD(target_string, context) {if ((target_string) && isname(keyword, get_string_after_color_code_removal((target_string), NULL))) { return (context); }}
const char * keyword_appears_in_file(const char *keyword, struct matrix_file *file, bool search_name, bool search_desc) {
  if (!keyword || !*keyword) {
    return NULL;
  }

  if (!file) {
    mudlog("SYSERR: Received NULL file to keyword_appears_in_file()!", NULL, LOG_SYSLOG, TRUE);
    return NULL;
  }

  if (search_name) {
    CHECK_KEYWORD(file->name, "name");
  }

  // if (search_desc) {
  //   CHECK_KEYWORD(file->text.look_desc, "look desc");
  // }

  return NULL;
}

struct matrix_file *get_matrix_file_in_list_vis(struct char_data * ch, const char *name, struct matrix_file * list)
{
  struct matrix_file *i;
  int j = 0, number;
  char tmpname[MAX_INPUT_LENGTH];
  char *tmp = tmpname;
  bool staff_bit = IS_SENATOR(ch);

  if (!list)
    return NULL;
  strlcpy(tmp, name, sizeof(tmpname));

  if (!(number = get_number(&tmp, sizeof(tmpname))))
    return NULL;

  for (i = list; i && (j <= number); i = i->next_file) {    
    if (keyword_appears_in_file(tmp, i)) {
      if (++j == number)
        return i;
    }
  }
  return NULL;
}

/* Remove a file from a Matrix host. */
// void file_from_host(struct matrix_file *file) {
//   if (!file) {
//     mudlog("SYSERR: Null obj given to obj_from_host!", NULL, LOG_SYSLOG, TRUE);
//     return;
//   }

//   if (!file->in_host) {
//     mudlog("SYSERR: Non-hosted obj given to obj_from_host!", NULL, LOG_SYSLOG, TRUE);
//     return;
//   }

//   struct matrix_file *temp;
//   REMOVE_FROM_LIST(file, file->in_host->file, next_file);
//   file->in_host = NULL;
// }

/* remove an object from an object */
void file_from_obj(struct matrix_file * obj)
{
  struct matrix_file *temp;
  struct obj_data *obj_from;

  if (obj->in_obj == NULL)
  {
    log("error (handler.c): trying to illegally extract obj from obj");
    return;
  }
  obj_from = obj->in_obj;
  REMOVE_FROM_LIST(obj, obj_from->files, next_file);

  obj->in_obj = NULL;
  obj->next_file = NULL;
}

/* Extract an object from the world */
void extract_matrix_file(struct matrix_file *file)
{
  bool set = FALSE;
  // if (file->in_host) {
  //   file_from_host(file);
  //   if (set)
  //     mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: More than one list pointer set when extracting '%s' (%ld)! (in_host)", file->name, file->idnum);
  //   set = TRUE;
  // }

  if (file->in_obj) {
    file_from_obj(file);
    if (set)
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: More than one list pointer set when extracting '%s' (%ld)! (in_obj)", file->name, file->idnum);
    set = TRUE;
  }

  delete file;

#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
  verify_every_pointer_we_can_think_of();
#endif
}

// States whether a program can be copied or not.
bool program_can_be_copied(struct matrix_file *prog) {
  if (!prog)
    return FALSE;

  switch (prog->file_type) {
    case SOFT_ASIST_COLD:
    case SOFT_ASIST_HOT:
    case SOFT_HARDENING:
    case SOFT_ICCM:
    case SOFT_ICON:
    case SOFT_MPCP:
    case SOFT_REALITY:
    case SOFT_RESPONSE:
    case SOFT_BOD:
    case SOFT_SENSOR:
    case SOFT_MASKING:
    case SOFT_EVASION:
    case SOFT_SUITE:
      return FALSE;
  }

  return TRUE;
}

void print_memory(struct char_data *ch, std::vector<struct obj_data*> devices) {
  for(const obj_data* device : devices)  {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  ^c%s^n\r\n",
             capitalize(GET_OBJ_NAME(device)));
    if (!device->files) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "    There are no files on this device.\r\n");
    } else {
      for (struct matrix_file *file = device->files; file; file = file->next_file) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "    [^g%4d^nmp] %-30s", file->size, file->name);
        if (file->file_type) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " R%-2d %-16s", file->rating, programs[file->file_type].name);

          if (file->work_failed == WORK_PHASE_NONE)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^y(concept)^n");
          else if (file->work_phase < WORK_PHASE_PROGRAM)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^y(design)^n");
          else
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^y(program)^n");
        }
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n");
      }
    }
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  Storage Memory: %d free of %d total\r\n\r\n",
          GET_CYBERDECK_FREE_STORAGE(device), GET_CYBERDECK_TOTAL_STORAGE(device));
  }
  send_to_char(buf, ch);
}

ACMD(do_memory) {
  struct obj_data* cyber;
  skip_spaces(&argument);
  snprintf(buf, sizeof(buf), ""); // clear buffer

  if (is_abbrev(argument, "headware memory")) {
    cyber = find_cyberware(ch, CYB_MEMORY);
    if (!cyber) {
      send_to_char(ch, "You don't appear to have any headware memory installed.\r\n");
      return;
    }

    return print_memory(ch, {cyber});
  }

  std::vector<struct obj_data*> devices = get_storage_devices(ch, TRUE);
  if (!devices.size()) {
    send_to_char(ch, "You don't have any devices on you capable of file storage.\r\n");
    return;
  }

  if (strlen(argument)) {
    for(obj_data* device : devices)  {
      if (keyword_appears_in_obj(argument, device)) {
        return print_memory(ch, {device});
      }
    }
    send_to_char(ch, "You don't have %s '%s' in your inventory.\r\n", AN(argument), argument);
    return;
  }

  snprintf(buf, sizeof(buf), "You start checking the files across your various devices:\r\n");
  return print_memory(ch, devices);
}