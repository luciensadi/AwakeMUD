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

int adjust_device_memory(struct obj_data *device, int change) {
  switch (GET_OBJ_TYPE(device)) {
    case ITEM_DECK_ACCESSORY:
      return GET_OBJ_VAL(device, 2) - change;
      break;
    case ITEM_CUSTOM_DECK:
    case ITEM_CYBERDECK:
      return GET_OBJ_VAL(device, 3) - change;
      break;
  }

  return 0;
}

int get_device_total_memory(struct obj_data *device) {
  switch (GET_OBJ_TYPE(device)) {
    case ITEM_DECK_ACCESSORY:
      return GET_OBJ_VAL(device, 2);
      break;
    case ITEM_CUSTOM_DECK:
    case ITEM_CYBERDECK:
      return GET_OBJ_VAL(device, 3);
      break;
  }
  return 0;
}

int get_device_used_memory(struct obj_data *device) {
  switch (GET_OBJ_TYPE(device)) {
    case ITEM_DECK_ACCESSORY:
      return GET_OBJ_VAL(device, 3);
      break;
    case ITEM_CUSTOM_DECK:
    case ITEM_CYBERDECK:
      return GET_OBJ_VAL(device, 5);
      break;
  }
  return 0;
}

int get_device_free_memory(struct obj_data *device) {
  int total_memory = get_device_total_memory(device);
  int used_memory = get_device_used_memory(device);

  return total_memory - used_memory;
}

bool can_file_fit(struct matrix_file *file, struct obj_data *device) {
  if (!file->size) return TRUE;
  
  int total_memory = get_device_total_memory(device);
  int used_memory = get_device_used_memory(device);

  if (file->size + used_memory > total_memory) return FALSE;
  return TRUE;
}

std::vector<struct obj_data*> get_internal_storage_devices(struct char_data *ch) {
  struct obj_data* cyber;
  std::vector<struct obj_data*> found_list = {};

  cyber = find_cyberware(ch, CYB_MEMORY);
  if (cyber) {
    found_list.push_back(cyber);
  }

  return found_list;
}

std::vector<struct obj_data*> get_storage_devices(struct char_data *ch, bool only_relevant) {
  struct obj_data* cyber;
  std::vector<struct obj_data*> found_list = get_internal_storage_devices(ch);

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

obj_data* find_internal_storage(struct char_data *ch, int free_space) {
  std::vector<obj_data *> list = get_internal_storage_devices(ch);

  if (list.size() <= 0) return NULL;

  for(obj_data* i : list)  {
    if (!free_space) return i;

    int total_memory = get_device_total_memory(i);
    int used_memory = get_device_used_memory(i);
    int free_memory = total_memory - used_memory;
    if (free_memory >= free_space) return i;
  }

  return NULL;
}

obj_data* find_obj_in_vector_vis(struct char_data * ch, const char *name, std::vector<obj_data *> &list) {
  int j = 0, number;
  char tmpname[MAX_INPUT_LENGTH];
  char *tmp = tmpname;

  // No list, no worries.
  if (list.size() <= 0)
    return NULL;

  strlcpy(tmp, name, sizeof(tmpname));
  if (!(number = get_number(&tmp, sizeof(tmpname))))
    return NULL;

  for(obj_data* i : list)  {
    if (keyword_appears_in_obj(tmp, i)) {
      if (++j == number)
        return i;
    }
  }

  return NULL;
}

matrix_file* create_matrix_file(obj_data *storage, int load_origin) {
  struct matrix_file *new_file = new matrix_file();
  new_file->idnum = _next_matrix_id();
  new_file->load_origin = load_origin;
  new_file->creation_time = time(0);

  if (storage) {
    new_file->in_obj = storage;
    new_file->next_file = storage->files;
    storage->files = new_file;
  }

  return new_file;
}

matrix_file* clone_matrix_file(struct matrix_file *source) {
  struct matrix_file *copy = create_matrix_file(NULL, source->load_origin);

  copy->name = strdup(source->name);
  copy->file_type = source->file_type;
  copy->program_type = source->program_type;
  copy->rating = source->rating;
  copy->size = source->size;
  copy->wound_category = source->wound_category;
  copy->is_default = source->is_default;
  copy->creator_idnum = source->creator_idnum;
  if (source->content) copy->content = strdup(source->content);

  copy->work_phase = source->work_phase;
  copy->work_ticks_left = source->work_ticks_left;
  copy->work_original_ticks_left = source->work_original_ticks_left;
  copy->work_successes = source->work_successes;
  
  copy->last_decay_time = source->last_decay_time;

  return copy;
}

/* remove a matrix file from an object */
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

/* remove a matrix file from an object */
void file_from_host(struct matrix_file * obj)
{
  struct matrix_file *temp;
  struct host_data *host_from;

  if (obj->in_host == NULL)
  {
    log("error (handler.c): trying to illegally extract obj from obj");
    return;
  }
  host_from = obj->in_host;
  REMOVE_FROM_LIST(obj, host_from->files, next_file);

  obj->in_host = NULL;
  obj->next_file = NULL;
}

matrix_file* obj_to_matrix_file(obj_data *prog, obj_data *device) {
  if (prog->files) {
    // Sometimes I hide files on objects as a convenient way of saving memory
    struct matrix_file *file = prog->files;
    file_from_obj(prog->files);

    file->next_file = device->files;
    device->files = file;
    return file;
  }
  struct matrix_file *new_file = create_matrix_file(device, prog->load_origin);

  new_file->wound_category = GET_PROGRAM_ATTACK_DAMAGE(prog);
  new_file->name = strdup(prog->restring);
  new_file->rating = GET_PROGRAM_RATING(prog);
  new_file->work_ticks_left = GET_OBJ_VAL(prog, 4);
  new_file->is_default = GET_PROGRAM_IS_DEFAULTED(prog);
  new_file->work_original_ticks_left = GET_OBJ_TIMER(prog);
  new_file->work_successes = GET_DESIGN_SUCCESSES(prog);

  if (GET_OBJ_TYPE(prog) == ITEM_PROGRAM) {
    new_file->file_type = MATRIX_FILE_PROGRAM;
    new_file->program_type = GET_PROGRAM_TYPE(prog);
  } else if (GET_OBJ_TYPE(prog) == ITEM_DESIGN) {
    new_file->file_type = MATRIX_FILE_DESIGN;
    new_file->program_type = GET_PROGRAM_TYPE(prog);
  }
  
  return new_file;
}

matrix_file* obj_to_matrix_file(obj_data *prog) {
  return obj_to_matrix_file(prog, prog->in_obj);
}

obj_data* matrix_file_to_obj(matrix_file *file) {
  struct obj_data *chip = read_object(OBJ_BLANK_OPTICAL_CHIP, VIRTUAL, file->load_origin);
  GET_DECK_ACCESSORY_TYPE(chip) = TYPE_FILE;
  
  if (file->file_type == MATRIX_FILE_PROGRAM) {
    snprintf(buf, sizeof(buf), "a R%d %s '%s' program chip", file->rating, capitalize(programs[file->program_type].name), file->name);
  } else if (file->file_type == MATRIX_FILE_SOURCE_CODE) {
    snprintf(buf, sizeof(buf), "a R%d %s '%s' source chip", file->rating, capitalize(programs[file->program_type].name), file->name);
  } else if (file->file_type == MATRIX_FILE_DESIGN) {
    snprintf(buf, sizeof(buf), "a R%d %s '%s' design chip", file->rating, capitalize(programs[file->program_type].name), file->name);
  } else {
    snprintf(buf, sizeof(buf), "a '%s' optical chip", file->name);
  }
  
  chip->restring = str_dup(buf);
  if (file->in_obj)
    file_from_obj(file); // derefs from list

  // We hide the file on the chip because .. it makes things easier.
  file->in_obj = chip;
  file->next_file = chip->files;
  chip->files = file; 

  return chip;
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

bool perform_get_matrix_file_from_device(struct char_data *ch, struct matrix_file *file, struct obj_data *device, int mode) {
  if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return FALSE;
  } 

  snprintf(buf, sizeof(buf), "$n saves %s from $p %s and ejects the file on an optical chip.", file->name, GET_OBJ_NAME(device));
  act(buf, TRUE, ch, device, 0, TO_ROOM);
  snprintf(buf, sizeof(buf), "You save %s from %s and eject the file onto an optical chip.", file->name, GET_OBJ_NAME(device));
  act(buf, FALSE, ch, device, 0, TO_CHAR);

  struct obj_data *chip = matrix_file_to_obj(file);
  obj_to_char(chip, ch);

  return TRUE;
}

/* Extract a matrix file from the world */
void extract_matrix_file(struct matrix_file *file)
{
  bool set = FALSE;
  if (file->in_host) {
    file_from_host(file);
    if (set)
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: More than one list pointer set when extracting '%s' (%ld)! (in_host)", file->name, file->idnum);
    set = TRUE;
  }

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

  switch (prog->program_type) {
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
  for(obj_data* device : devices)  {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  ^c%s^n\r\n",
             capitalize(GET_OBJ_NAME(device)));
    if (!device->files) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "    There are no files on this device.\r\n");
    } else {
      for (struct matrix_file *file = device->files; file; file = file->next_file) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "    [^g%4d^nmp] %-30s", file->size, file->name);
        if (file->file_type == MATRIX_FILE_PROGRAM || file->file_type == MATRIX_FILE_DESIGN || file->file_type == MATRIX_FILE_SOURCE_CODE) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " R%-2d %-18s", file->rating, programs[file->program_type].name);

          if (file->file_type == MATRIX_FILE_DESIGN && file->work_phase <= WORK_PHASE_READY)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^y(concept)^n");
          else if (file->file_type == MATRIX_FILE_DESIGN)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^y(design)^n");
          else if (file->file_type == MATRIX_FILE_SOURCE_CODE)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^y(source)^n");
          else if (file->file_type == MATRIX_FILE_PROGRAM)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^y(program)^n");
        }
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n");
      }
    }
    int used_memory = get_device_used_memory(device);
    int total_memory = get_device_total_memory(device);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  Storage Memory: %d free of %d total\r\n\r\n",
          total_memory - used_memory, total_memory);
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

matrix_file* copy_matrix_file_to(struct matrix_file *file, obj_data* to_device) {
  return NULL;
}

void move_matrix_file_to(struct matrix_file *file, host_data* to_host) {
    if (file->in_host)
    file_from_host(file);
  if (file->in_obj)
    file_from_obj(file);

    file->in_host = to_host;
    file->next_file = to_host->files;
    to_host->files = file;
}

void move_matrix_file_to(struct matrix_file *file, obj_data* to_device) {
  if (file->in_host)
    file_from_host(file);
  if (file->in_obj)
    file_from_obj(file);
  
  file->in_obj = to_device;
  file->next_file = to_device->files;
  to_device->files = file; 
}

ACMD(do_delete) {
  // TODO
}

bool handle_matrix_file_transfer(struct char_data *ch, char *argument) {
  // possible syntaxes:
  //   transfer somefile myawesomedeck
  //   transfer desktop somefile myawesomedeck
  const char* remainder = two_arguments(argument, buf1, buf2);
  obj_data *target_deck = NULL, *source_deck = NULL;
  matrix_file *file = NULL;
  send_to_char(ch, "buf1: %s, buf2: %s, remainder: %s\r\n", buf1, buf2, remainder);

  std::vector<struct obj_data*> all_devices = get_storage_devices(ch, FALSE);
  std::vector<struct obj_data*> search_devices = {};

  if (*remainder) {
    // If we have *remainder, it means the user did the 3 arg version of transfer, so we can be sure
    // they were doing the decker version, and thus can report errors.
    if (!(target_deck = find_obj_in_vector_vis(ch, remainder, all_devices))) {
      // Unable to find the deck we want to transfer to. :(
      send_to_char(ch, "I don't see any matrix file storage device named %s near you.", remainder);
      return TRUE; // True because we abort early. 
    }

    if (!(source_deck = find_obj_in_vector_vis(ch, buf1, all_devices))) {
      send_to_char(ch, "I don't see any matrix file storage device named %s near you.", buf1);
      return TRUE; // True because we abort early. 
    }

    search_devices = {source_deck};
  } else {
    if (!(target_deck = find_obj_in_vector_vis(ch, buf2, all_devices))) {
      return FALSE;
    }

    search_devices = all_devices;
  }

  for(obj_data* device : search_devices)  {
    if ((file = get_matrix_file_in_list_vis(ch, *remainder ? buf2 : buf1, device->files)))
      break;
  }

  send_to_char(ch, "file: %s\r\n", file->name);

  if (!file)
    return FALSE;

  source_deck = file->in_obj;
  send_to_char(ch, "source deck: %s\r\n", GET_OBJ_NAME(source_deck));

  if (AFF_FLAGGED(ch, PLR_MATRIX) && ch->persona) {
    send_to_icon(ch->persona, "You transfer %s from %s to %s.",
      file->name, GET_OBJ_NAME(source_deck), GET_OBJ_NAME(target_deck));
  } else {
    snprintf(buf, sizeof(buf), "You connect a cable between %s and %s, making quick work of transferring %s to %s.",
      GET_OBJ_NAME(source_deck), GET_OBJ_NAME(target_deck), file->name, GET_OBJ_NAME(target_deck));
    act(buf, FALSE, ch, source_deck, target_deck, TO_CHAR);
  }

  file_from_obj(file); 
  file->in_obj = target_deck;
  file->next_file = target_deck->files;
  target_deck->files = file; 
  return TRUE;
} 