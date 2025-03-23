#include <algorithm>
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
#include "newdb.hpp"
#include "handler.hpp"
#include "constants.hpp"
#include "chipjacks.hpp"

// Function to get a unique ID
std::atomic<long> matrix_file_id_counter{1};
int _next_matrix_id() {
    return matrix_file_id_counter.fetch_add(1, std::memory_order_relaxed);
}

int adjust_device_memory(struct obj_data *device, int change) {
  switch (GET_OBJ_TYPE(device)) {
    case ITEM_CYBERWARE:
      return GET_OBJ_VAL(device, 5) -= change;
    case ITEM_DECK_ACCESSORY:
      return GET_OBJ_VAL(device, 3) -= change;
    case ITEM_CUSTOM_DECK:
    case ITEM_CYBERDECK:
      return GET_OBJ_VAL(device, 5) -= change;
  }

  return 0;
}

int get_device_total_memory(struct obj_data *device) {
  switch (GET_OBJ_TYPE(device)) {
    case ITEM_CYBERWARE:
      return GET_OBJ_VAL(device, 3);
    case ITEM_DECK_ACCESSORY:
      return GET_OBJ_VAL(device, 2);
    case ITEM_CUSTOM_DECK:
    case ITEM_CYBERDECK:
      return GET_OBJ_VAL(device, 3);
  }
  return 0;
}

int get_device_used_memory(struct obj_data *device) {
  switch (GET_OBJ_TYPE(device)) {
    case ITEM_CYBERWARE:
      return GET_OBJ_VAL(device, 5);
    case ITEM_DECK_ACCESSORY:
      return GET_OBJ_VAL(device, 3);
    case ITEM_CUSTOM_DECK:
    case ITEM_CYBERDECK:
      return GET_OBJ_VAL(device, 5);
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
  std::vector<struct obj_data*> found_list = get_internal_storage_devices(ch);
  int max_cyberdecks = 0;

  if (only_relevant) {
    for (struct obj_data *check = ch->cyberware; check != NULL; check = check->next_content) {
      if (GET_CYBERWARE_TYPE(check) == CYB_DATAJACK || (GET_CYBERWARE_TYPE(check) == CYB_EYES && IS_SET(GET_CYBERWARE_FLAGS(check), EYE_DATAJACK))) {
        max_cyberdecks++;
      }
    }
  } else {
    max_cyberdecks = 99; // Arbitrary maximum, sorry for magic number
  }

  if (only_relevant && AFF_FLAGGED(ch, PLR_MATRIX)) {
    if (ch->persona && ch->persona->decker) {
      if (ch->persona->decker->deck && !ch->persona->decker->deck->obj_flags.extra_flags.IsSet(ITEM_EXTRA_OTAKU_RESONANCE))
        found_list.push_back(ch->persona->decker->deck);
    }
    max_cyberdecks--; // Always make sure the cyberdeck of the decker is included.
  }

  for (struct obj_data *cyber = ch->carrying; cyber; cyber = cyber->next_content) {
    if (max_cyberdecks <= 0) break;
    if (std::find(found_list.begin(), found_list.end(), cyber) != found_list.end()) continue; // If we contain this set, continue
    if (IS_OBJ_STAT(cyber, ITEM_EXTRA_STAFF_ONLY) && !IS_SENATOR(ch)) continue;
    if ((GET_OBJ_TYPE(cyber) == ITEM_CYBERDECK || GET_OBJ_TYPE(cyber) == ITEM_CUSTOM_DECK)) {
      found_list.push_back(cyber);
      max_cyberdecks--;
    }
  }

  for (int i = 0; i < NUM_WEARS; i++) {
    if (max_cyberdecks <= 0) break;
    if (std::find(found_list.begin(), found_list.end(), GET_EQ(ch, i)) != found_list.end()) continue; // If we contain this set, continue
    if (IS_OBJ_STAT(GET_EQ(ch, i), ITEM_EXTRA_STAFF_ONLY) && !IS_SENATOR(ch)) continue;
    if (GET_EQ(ch, i) && (GET_OBJ_TYPE(GET_EQ(ch,i )) == ITEM_CYBERDECK || GET_OBJ_TYPE(GET_EQ(ch,i )) == ITEM_CUSTOM_DECK)) {
      found_list.push_back(GET_EQ(ch, i));
      max_cyberdecks--;
    }
  }

  if (only_relevant && !AFF_FLAGGED(ch, PLR_MATRIX)) {
    // Decking accessories / computers
    // These cannot be accessed while in the matrix
    for (struct obj_data *cyber = (ch)->in_room ? (ch)->in_room->contents : (ch)->in_veh->contents; cyber; cyber = cyber->next_content) {
      if (IS_OBJ_STAT(cyber, ITEM_EXTRA_STAFF_ONLY) && !IS_SENATOR(ch)) continue;
      if (GET_OBJ_TYPE(cyber) == ITEM_DECK_ACCESSORY && GET_DECK_ACCESSORY_TYPE(cyber) == TYPE_COMPUTER)
        found_list.push_back(cyber);  
    }
  }

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
  bool staff_bit = IS_SENATOR(ch);

  // No list, no worries.
  if (list.size() <= 0)
    return NULL;

  strlcpy(tmp, name, sizeof(tmpname));
  if (!(number = get_number(&tmp, sizeof(tmpname))))
    return NULL;

  for(obj_data* i : list)  {
    // Invisible to you: Blocked by quest protections.
    if (!staff_bit && ch_is_blocked_by_quest_protections(ch, i, FALSE, FALSE))
      continue;

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

  new_file->dirty_bit = TRUE;
  return new_file;
}

matrix_file* clone_matrix_file(struct matrix_file *source) {
  struct matrix_file *copy = create_matrix_file(NULL, source->load_origin);

  if (source->name) copy->name = strdup(source->name);
  copy->file_type = source->file_type;
  copy->program_type = source->program_type;
  copy->rating = source->rating;
  copy->size = source->size;
  copy->wound_category = source->wound_category;
  copy->is_default = source->is_default;
  copy->creator_idnum = source->creator_idnum;
  if (source->content) copy->content = strdup(source->content);
  copy->skill = source->skill;
  copy->linked = source->linked;
  copy->compression_factor = source->compression_factor;

  copy->work_phase = source->work_phase;
  copy->work_ticks_left = source->work_ticks_left;
  copy->work_original_ticks_left = source->work_original_ticks_left;
  copy->work_successes = source->work_successes;

  copy->from_host_color = source->from_host_color;
  copy->from_host_vnum = source->from_host_vnum;
  
  copy->last_decay_time = source->last_decay_time;
  copy->dirty_bit = TRUE;

  return copy;
}

/* remove a matrix file from an object */
/* NOTE, THIS WILL ALSO DELETE THE SQL ENTRY */
void file_from_obj(struct matrix_file * obj, bool do_sql_delete = TRUE)
{
  struct matrix_file *temp;
  struct obj_data *obj_from;

  if (obj->in_obj == NULL)
  {
    log("error (matrix_storage.cpp): trying to illegally extract file from obj");
    return;
  }
  obj_from = obj->in_obj;
  
  REMOVE_FROM_LIST(obj, obj_from->files, next_file);
  if (do_sql_delete) {
    snprintf(buf, sizeof(buf), "DELETE FROM matrix_files WHERE idnum=%ld; ", 
      obj->idnum);
    mysql_wrapper(mysql, buf);
    obj->dirty_bit = TRUE;
  }

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
    log_vfprintf("error (matrix_storage.cpp): trying to illegally extract file (%ld: %s) from host", obj->idnum, obj->name);
    return;
  }
  host_from = obj->in_host;
  REMOVE_FROM_LIST(obj, host_from->files, next_file);

  obj->in_host = NULL;
  obj->next_file = NULL;
}

matrix_file* obj_to_matrix_file(obj_data *prog, obj_data *device) {
  struct matrix_file *new_file;

  if (GET_OBJ_TYPE(prog) == ITEM_DECK_ACCESSORY && GET_DECK_ACCESSORY_TYPE(prog) == TYPE_MATRIX_FILE) {
    // Special handling for matrix files. These already contain our matrix file.
    new_file = prog->files;
    if (device) {
      adjust_device_memory(device, prog->files->size * -1);
    } else {
      prog->files = new_file->next_file;
      new_file->in_obj = NULL;
      new_file->next_file = NULL;
    }

    return new_file;
  }

  new_file = create_matrix_file(device, prog->load_origin);

  new_file->wound_category = GET_PROGRAM_ATTACK_DAMAGE(prog);
  new_file->dirty_bit = TRUE;
  if (prog->matrix_restring) new_file->name = strdup(prog->matrix_restring);
  else if (prog->restring) new_file->name = strdup(prog->restring);
  else new_file->name = strdup(prog->text.name);
  new_file->work_ticks_left = GET_OBJ_VAL(prog, 4);
  
  switch(GET_OBJ_TYPE(prog)) {
    case ITEM_DECK_ACCESSORY:
      if (GET_DECK_ACCESSORY_TYPE(prog) == TYPE_PHOTO) {
        new_file->content = str_dup(prog->photo);
        new_file->file_type = MATRIX_FILE_PHOTO;
        new_file->size = 1;
        break;
      }
      new_file->file_type = MATRIX_FILE_PROGRAM;
      new_file->program_type = GET_DECK_ACCESSORY_FILE_TYPE(prog);
      new_file->size = GET_DECK_ACCESSORY_FILE_SIZE(prog);
      new_file->rating = GET_DECK_ACCESSORY_FILE_RATING(prog);
      new_file->from_host_vnum = GET_DECK_ACCESSORY_FILE_HOST_VNUM(prog);
      new_file->from_host_color = GET_DECK_ACCESSORY_FILE_HOST_COLOR(prog);
      break;
    case ITEM_PROGRAM:
      // Programs are a bit different, if they don't have restrings,
      // we just make one to compensate for store-bought programs
      if (!prog->matrix_restring) {
        snprintf(buf, sizeof(buf), "%s:r%d", programs[GET_PROGRAM_TYPE(prog)].name, GET_PROGRAM_RATING(prog));
        new_file->name = str_dup(buf);
      }
      // We help store software shuffle into the right type
      switch (GET_PROGRAM_TYPE(prog)) {
        case SOFT_BOD:
        case SOFT_SENSOR:
        case SOFT_MASKING:
        case SOFT_EVASION:
        case SOFT_ASIST_COLD:
        case SOFT_ASIST_HOT:
        case SOFT_HARDENING:
        case SOFT_ICCM:
        case SOFT_ICON:
        case SOFT_MPCP:
        case SOFT_REALITY:
        case SOFT_RESPONSE:
          new_file->file_type = MATRIX_FILE_FIRMWARE;
          break;
        default:
          new_file->file_type = MATRIX_FILE_PROGRAM;
          break;
      }
      new_file->program_type = GET_PROGRAM_TYPE(prog);
      new_file->size = GET_PROGRAM_SIZE(prog);
      new_file->rating = GET_PROGRAM_RATING(prog);
      new_file->is_default = GET_PROGRAM_IS_DEFAULTED(prog);
      break;
    case ITEM_SOURCE_CODE:
      new_file->file_type = MATRIX_FILE_PROGRAM;
      new_file->program_type = GET_PROGRAM_TYPE(prog);
      new_file->size = GET_DESIGN_SIZE(prog);
      new_file->rating = GET_PROGRAM_RATING(prog);
      new_file->is_default = FALSE;
      break;
    case ITEM_DESIGN:
      new_file->file_type = MATRIX_FILE_DESIGN;
      new_file->program_type = GET_PROGRAM_TYPE(prog);
      new_file->size = GET_DESIGN_SIZE(prog);
      new_file->original_size = GET_PROGRAM_SIZE(prog);
      new_file->rating = GET_DESIGN_RATING(prog);
      new_file->work_successes = GET_DESIGN_SUCCESSES(prog);
      new_file->work_original_ticks_left = GET_OBJ_TIMER(prog);
      if (new_file->work_successes)
        new_file->work_phase = WORK_PHASE_COMPLETE;
      break;
    case ITEM_CHIP:
      // Skillsoft/knowsoft chip
      new_file->file_type = MATRIX_FILE_SKILLSOFT;
      new_file->skill = GET_CHIP_SKILL(prog);
      new_file->size = GET_CHIP_SIZE(prog);
      new_file->rating = GET_CHIP_RATING(prog);
      break;
  }

  if (device && new_file->file_type != MATRIX_FILE_FIRMWARE) {
    adjust_device_memory(device, new_file->size * -1);
  }
  
  return new_file;
}

matrix_file* obj_to_matrix_file(obj_data *prog) {
  return obj_to_matrix_file(prog, prog->in_obj);
}

obj_data* matrix_file_to_obj(matrix_file *file) {
  struct obj_data *chip = NULL;
  
  switch(file->file_type) {
    case MATRIX_FILE_PROGRAM:
      chip = read_object(OBJ_BLANK_OPTICAL_CHIP, VIRTUAL, file->load_origin);
      GET_PROGRAM_SIZE(chip) = file->size;
      GET_PROGRAM_ATTACK_DAMAGE(chip) = file->wound_category;
      GET_PROGRAM_EVALUATE_CREATION_TIME(chip) = file->creation_time;
      GET_PROGRAM_EVALUATE_LAST_DECAY_TIME(chip) = file->last_decay_time;
      GET_PROGRAM_RATING(chip) = file->rating;
      GET_PROGRAM_TYPE(chip) = file->program_type;
      
      snprintf(buf, sizeof(buf), "a R%d %s '%s' program chip", file->rating, capitalize(programs[file->program_type].name), file->name);
      break;
    case MATRIX_FILE_SOURCE_CODE:
      chip = read_object(OBJ_BLANK_PROGRAM, VIRTUAL, file->load_origin);
      GET_OBJ_TYPE(chip) = ITEM_SOURCE_CODE;
      GET_PROGRAM_SIZE(chip) = file->original_size;
      GET_DESIGN_SIZE(chip) = file->size;
      GET_DESIGN_PROGRAM_WOUND_LEVEL(chip) = file->wound_category;
      GET_DESIGN_RATING(chip) = file->rating;

      snprintf(buf, sizeof(buf), "a R%d %s '%s' source chip", file->rating, capitalize(programs[file->program_type].name), file->name);
      break;
    case MATRIX_FILE_DESIGN:
      chip = read_object(OBJ_BLANK_PROGRAM_DESIGN, VIRTUAL, file->load_origin);
      GET_PROGRAM_SIZE(chip) = file->original_size;
      GET_DESIGN_SIZE(chip) = file->size;
      GET_DESIGN_PROGRAM_WOUND_LEVEL(chip) = file->wound_category;
      GET_DESIGN_RATING(chip) = file->rating;
      GET_DESIGN_SUCCESSES(chip) = file->work_successes;

      snprintf(buf, sizeof(buf), "a R%d %s '%s' design chip", file->rating, capitalize(programs[file->program_type].name), file->name);
      break;
    case MATRIX_FILE_SKILLSOFT:
      chip = read_object(OBJ_BLANK_OPTICAL_CHIP, VIRTUAL, file->load_origin);
      GET_CHIP_SKILL(chip) = file->skill;
      GET_CHIP_SIZE(chip) = file->size;
      GET_CHIP_COMPRESSION_FACTOR(chip) = file->compression_factor;
      GET_CHIP_RATING(chip) = file->rating;

      snprintf(buf, sizeof(buf), "a R%d %s chip", file->rating, skills[file->skill].name);    
      break;
    case MATRIX_FILE_FIRMWARE:
      chip = read_object(OBJ_BLANK_OPTICAL_CHIP, VIRTUAL, file->load_origin);
      GET_DECK_ACCESSORY_TYPE(chip) = TYPE_FIRMWARE;
      GET_DECK_ACCESSORY_FILE_RATING(chip) = file->rating;
      GET_DECK_ACCESSORY_FILE_SIZE(chip) = file->size;
      GET_DECK_ACCESSORY_FILE_TYPE(chip) = file->program_type;
      GET_DECK_ACCESSORY_FILE_CREATION_TIME(chip) = file->creation_time;
      
      snprintf(buf, sizeof(buf), "a R%d %s firmware", file->rating, programs[file->program_type].name);   
      break;   
    case MATRIX_FILE_PHOTO:
      chip = read_object(OBJ_BLANK_PHOTO, VIRTUAL, file->load_origin);
      chip->photo = str_dup(file->content);
      chip->restring = str_dup(file->name);

      snprintf(buf, sizeof(buf), "%s", file->name);
      break;
    case MATRIX_FILE_PAYDATA:
      chip = read_object(OBJ_BLANK_OPTICAL_CHIP, VIRTUAL, file->load_origin);

      GET_DECK_ACCESSORY_FILE_HOST_VNUM(chip) = file->from_host_vnum;
      GET_DECK_ACCESSORY_TYPE(chip) = TYPE_PAYDATA;
      GET_DECK_ACCESSORY_FILE_CREATION_TIME(chip) = file->creation_time;
      GET_DECK_ACCESSORY_FILE_SIZE(chip) = file->size;
      GET_DECK_ACCESSORY_FILE_HOST_COLOR(chip) = file->from_host_color;
      break;
    default:
      chip = read_object(OBJ_BLANK_OPTICAL_CHIP, VIRTUAL, file->load_origin);
      GET_DECK_ACCESSORY_TYPE(chip) = TYPE_MATRIX_FILE;
      GET_CHIP_SIZE(chip) = file->size;

      move_matrix_file_to(file, chip);
      snprintf(buf, sizeof(buf), "a '%s' optical chip", file->name);
      break;
  }
  
  chip->matrix_restring = str_dup(file->name); // Preserve the matrix filename if we reinstall
  chip->restring = str_dup(buf); 
  if (file->in_obj && file->in_obj != chip)
    file_from_obj(file); // derefs from list

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

  if (search_desc) {
    CHECK_KEYWORD(file->content, "content");
  }

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
    // Invisible to you: Blocked by quest protections.
    if (!staff_bit && ch_is_blocked_by_quest_protections(ch, i, FALSE, FALSE))
      continue;

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
    adjust_device_memory(file->in_obj, file->size);
    file_from_obj(file, FALSE);
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

  if (prog->quest_id) 
    return FALSE;
  
  // Programs are compiled for one system; cannot be copied
  if (prog->file_type == MATRIX_FILE_PROGRAM)
    return FALSE;

  // Loses its value if copied
  if (prog->file_type == MATRIX_FILE_PAYDATA)
    return FALSE;

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
        if (file->program_type) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " R%-2d %-18s", file->rating, programs[file->program_type].name);

          if (file->file_type == MATRIX_FILE_DESIGN && file->work_phase <= WORK_PHASE_READY)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^y(concept)^n");
          else if (file->file_type == MATRIX_FILE_DESIGN)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^y(design)^n");
          else if (file->file_type == MATRIX_FILE_SOURCE_CODE)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^y(source)^n");
          else if (file->file_type == MATRIX_FILE_PROGRAM)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^y(program)^n");
          else if (file->file_type == MATRIX_FILE_FIRMWARE)
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^y(firmware)^n");
        }
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n");
      }
    }
    int used_memory = get_device_used_memory(device);
    int total_memory = get_device_total_memory(device);
    float percentage_remaining = ((float)used_memory / (float)total_memory) * 100; 
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  Storage Memory: ^%s%d^nMp free of ^c%d^nMp total (%.02f%%)\r\n\r\n",
          percentage_remaining < 25 ? "r" : "g",
          total_memory - used_memory, total_memory, percentage_remaining);
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
  struct matrix_file *copy = clone_matrix_file(file);

  if (to_device) {
    copy->in_obj = to_device;
    copy->next_file = to_device->files;
    to_device->files = file;
  }

  return copy;
}

void delete_matrix_file(struct matrix_file *file) {
  if(file->linked) {
    for (char_data *i = character_list; i; i = i->next_in_character_list) {
      if (i->unique_id != file->linked) continue;
      deactivate_single_skillsoft(file, i, TRUE);
    }
  }

  if (file->in_host) {
    file_from_host(file);
  }

  if (file->in_obj) {
    snprintf(buf, sizeof(buf), "DELETE FROM matrix_files WHERE idnum=%ld; ", 
      file->idnum);
    mysql_wrapper(mysql, buf);

    adjust_device_memory(file->in_obj, file->size);
    file_from_obj(file);
  }

  extract_matrix_file(file);
}

void move_matrix_file_to(struct matrix_file *file, host_data* to_host) {
  if (file->in_host) {
    file_from_host(file);
  }

  if (file->in_obj) {
    adjust_device_memory(file->in_obj, file->size);
    file_from_obj(file);    
  }

  file->dirty_bit = TRUE;
  file->in_host = to_host;
  file->next_file = to_host->files;
  to_host->files = file;
}

void move_matrix_file_to(struct matrix_file *file, obj_data* to_device) {
  if (file->in_host) {
    file_from_host(file);
  }

  if (file->in_obj) {
    adjust_device_memory(file->in_obj, file->size);
    file_from_obj(file);
  }
  
  file->dirty_bit = TRUE;
  file->in_obj = to_device;
  file->next_file = to_device->files;
  to_device->files = file; 
  adjust_device_memory(file->in_obj, file->size * -1);
}

void print_file_help(struct char_data *ch) {
  send_to_char(ch, "Do what with your files?\r\n"
    "   file copy <file> <to device>\r\n"
    "   file copy <from device> <file> <to device>\r\n"
    "   file move <file> <to device>\r\n"
    "   file move <from device> <file> <to device>\r\n"
    "   file delete <file>\r\n"
    "   file delete <from device> <file>\r\n"
    "   file rename <file> <new name>\r\n"
    "   file rename <from device> <file> <new name>\r\n");
}

ACMD(do_file) {
  skip_spaces(&argument);
  if (!*argument) {
    print_file_help(ch);
    return;
  }

  std::vector<struct obj_data*> all_devices = get_storage_devices(ch, TRUE);

  char first_arg[256];
  char *file_switch, *remainder, *second_arg = NULL, *third_arg = NULL;
  struct obj_data *from_device = NULL, *to_device = NULL;
  struct matrix_file *file = NULL;
  remainder = one_argument(argument, file_switch);
  if (*remainder) second_arg = one_argument(remainder, first_arg);
  if (*second_arg) third_arg = one_argument(second_arg, second_arg);

  if (is_abbrev(file_switch, "copy") || !strcmp(file_switch, "cp")) {
    if (!*first_arg) {
      send_to_char(ch, "Invalid syntax. You can ^WFILE COPY <file> <to device>^n or you can ^WFILE COPY <from device> <file> <to device>^n.\r\n");
      return;
    }

    if (!*third_arg && !*second_arg) {
      send_to_char(ch, "But where would you like to copy '%s' to?\r\n", first_arg);
      return;
    }

    
    if (*third_arg) { // FILE COPY <from device> <file> <to device> syntax
      if (!(to_device = find_obj_in_vector_vis(ch, third_arg, all_devices))) {
        send_to_char(ch, "You don't see any device named '%s' that you could copy files to.\r\n", third_arg);
        return;
      } else if (!(from_device = find_obj_in_vector_vis(ch, first_arg, all_devices))) {
        send_to_char(ch, "You don't see any device named '%s' that you could copy files from.\r\n", first_arg);
        return;
      }

      if (!(file = get_matrix_file_in_list_vis(ch, second_arg, from_device->files))) {
        send_to_char(ch, "You search but don't see any file named '%s' on %s.\r\n", second_arg, GET_OBJ_NAME(from_device));
        return;
      }
    } else if (!*third_arg) { // FILE COPY <file> <to device> syntax
      if (!(to_device = find_obj_in_vector_vis(ch, second_arg, all_devices))) {
        send_to_char(ch, "You don't see any device named '%s' that you could copy files to.\r\n", second_arg);
        return;
      }

      for (obj_data *device : all_devices) {
        if ((file = get_matrix_file_in_list_vis(ch, first_arg, device->files))) {
          from_device = device;
          break;
        }
      }

      if (!file || !from_device) {
         send_to_char(ch, "You search but don't see any file named '%s' on any of your devices.\r\n", first_arg);
         return;
      }
    }

    // If we made it this far then we have all our variables properly set.
    if (!program_can_be_copied(file)) {
      send_to_char(ch, "You try to copy %s from %s, but the copy-protection stops you.\r\n", file->name, GET_OBJ_NAME(from_device));
      return;
    }

    if (can_file_fit(file, to_device)) {
      send_to_char(ch, "There isn't enough space on %s, you need ^c%d^nmp but only ^c%d^n is available.\r\n",
        GET_OBJ_NAME(to_device), file->size, get_device_free_memory(to_device));
      return;
    }

    if (to_device == from_device) {
      send_to_char(ch, "You can't copy a file to and from the same device!\r\n");
      return;
    }

    if (AFF_FLAGGED(ch, PLR_MATRIX) && ch->persona) {
      send_to_icon(ch->persona, "You copy %s from %s to %s.\r\n",
        file->name, GET_OBJ_NAME(from_device), GET_OBJ_NAME(to_device));
    } else {
      snprintf(buf, sizeof(buf), "You connect a cable between %s and %s, making quick work of copying %s to %s.\r\n",
        GET_OBJ_NAME(from_device), GET_OBJ_NAME(to_device), file->name, GET_OBJ_NAME(to_device));
      act(buf, FALSE, ch, from_device, to_device, TO_CHAR);
    }

    copy_matrix_file_to(file, to_device);
  } else if (is_abbrev(file_switch, "move") || !strcmp(file_switch, "mv")) {
    if (!*first_arg) {
      send_to_char(ch, "Invalid syntax. You can ^WFILE MOVE <file> <to device>^n or you can ^WFILE MOVE <from device> <file> <to device>^n.\r\n");
      return;
    }

    if (!*third_arg && !*second_arg) {
      send_to_char(ch, "But where would you like to move '%s' to?\r\n", first_arg);
      return;
    }

    
    if (*third_arg) { // FILE MOVE <from device> <file> <to device> syntax
      if (!(to_device = find_obj_in_vector_vis(ch, third_arg, all_devices))) {
        send_to_char(ch, "You don't see any device named '%s' that you could move files to.\r\n", third_arg);
        return;
      } else if (!(from_device = find_obj_in_vector_vis(ch, first_arg, all_devices))) {
        send_to_char(ch, "You don't see any device named '%s' that you could move files from.\r\n", first_arg);
        return;
      }

      if (!(file = get_matrix_file_in_list_vis(ch, second_arg, from_device->files))) {
        send_to_char(ch, "You search but don't see any file named '%s' on %s.\r\n", second_arg, GET_OBJ_NAME(from_device));
        return;
      }
    } else if (!*third_arg) { // FILE MOVE <file> <to device> syntax
      if (!(to_device = find_obj_in_vector_vis(ch, second_arg, all_devices))) {
        send_to_char(ch, "You don't see any device named '%s' that you could move files to.\r\n", second_arg);
        return;
      }

      for (obj_data *device : all_devices) {
        if ((file = get_matrix_file_in_list_vis(ch, first_arg, device->files))) {
          from_device = device;
          break;
        }
      }

      if (!file || !from_device) {
         send_to_char(ch, "You search but don't see any file named '%s' on any of your devices.\r\n", first_arg);
         return;
      }
    }

    if (can_file_fit(file, to_device)) {
      send_to_char(ch, "There isn't enough space on %s, you need ^c%d^nmp but only ^c%d^n is available.\r\n",
        GET_OBJ_NAME(to_device), file->size, get_device_free_memory(to_device));
      return;
    }

    if (to_device == from_device) {
      send_to_char(ch, "You can't move a file to and from the same device!\r\n");
      return;
    }

    if (file->file_type == MATRIX_FILE_FIRMWARE) {
      send_to_char(ch, "The operating system prevents you from moving device firmware.\r\n");
      return;
    }

    if (AFF_FLAGGED(ch, PLR_MATRIX) && ch->persona) {
      send_to_icon(ch->persona, "You transfer %s from %s to %s.\r\n",
        file->name, GET_OBJ_NAME(from_device), GET_OBJ_NAME(to_device));
    } else {
      snprintf(buf, sizeof(buf), "You connect a cable between %s and %s, making quick work of transferring %s to %s.\r\n",
        GET_OBJ_NAME(from_device), GET_OBJ_NAME(to_device), file->name, GET_OBJ_NAME(to_device));
      act(buf, FALSE, ch, from_device, to_device, TO_CHAR);
    }

    move_matrix_file_to(file, to_device);
  } else if (is_abbrev(file_switch, "delete")) {
    if (*second_arg) {
      // If we have a second arg we have a target device to delete from.
      if (!(from_device = find_obj_in_vector_vis(ch, first_arg, all_devices))) {
        send_to_char(ch, "I don't see any matrix file storage device named %s near you.\r\n", first_arg);
        return;
      }

      if (!(file = get_matrix_file_in_list_vis(ch, second_arg, from_device->files))) {
        send_to_char(ch, "You search but don't see any file named '%s' on %s.\r\n", second_arg, GET_OBJ_NAME(from_device));
        return;
      }
    } else {
      for (obj_data *device : all_devices) {
        if ((file = get_matrix_file_in_list_vis(ch, first_arg, device->files))) {
          from_device = device;
          break;
        }
      }

      if (!file || !from_device) {
        send_to_char(ch, "You search but don't see any file named '%s' on any of your devices.\r\n", first_arg);
        return;
      }
    }

    if (file->quest_id) {
      send_to_char(ch, "That file seems too important to delete.\r\n");
      return;
    }

    if (file->transferring_to || file->transferring_to_host) {
      send_to_char(ch, "You cannot delete a file that is actively being transferred.\r\n");
      return;
    }

    if (file->file_type == MATRIX_FILE_FIRMWARE) {
      send_to_char(ch, "The operating system prevents you from deleting device firmware.\r\n");
      return;
    }

    send_to_char(ch, "You successfully delete file ^W%s^n from %s.\r\n", file->name, GET_OBJ_NAME(from_device));
    delete_matrix_file(file);
  } else if (is_abbrev(file_switch, "rename")) {
    if (*third_arg) {
      // If we have a third arg we have a target device to rename from.
      if (!(from_device = find_obj_in_vector_vis(ch, first_arg, all_devices))) {
        send_to_char(ch, "I don't see any matrix file storage device named %s near you.\r\n", first_arg);
        return;
      }

      if (!(file = get_matrix_file_in_list_vis(ch, second_arg, from_device->files))) {
        send_to_char(ch, "You search but don't see any file named '%s' on %s.\r\n", second_arg, GET_OBJ_NAME(from_device));
        return;
      }
    } else {
      for (obj_data *device : all_devices) {
        if ((file = get_matrix_file_in_list_vis(ch, first_arg, device->files))) {
          from_device = device;
          break;
        }
      }

      if (!file || !from_device) {
        send_to_char(ch, "You search but don't see any file named '%s' on any of your devices.\r\n", first_arg);
        return;
      }
    }

    // We're clear to rename
    send_to_char(ch, "You successfully rename %s to ^W%s^n.\r\n", file->name, *third_arg ? third_arg : second_arg);
    file->name = strdup(*third_arg ? third_arg : second_arg);
    file->dirty_bit = TRUE;
  } else {
    print_file_help(ch);
  }
}