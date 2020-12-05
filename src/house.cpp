#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <errno.h>


#if defined(WIN32) && !defined(__CYGWIN__)
#else
#include <unistd.h>
#endif

#include "structs.h"
#include "awake.h"
#include "comm.h"
#include "handler.h"
#include "db.h"
#include "newdb.h"
#include "interpreter.h"
#include "utils.h"
#include "house.h"
#include "memory.h"
#include "constants.h"
#include "file.h"
#include "vtable.h"

extern char *cleanup(char *dest, const char *src);
extern void ASSIGNMOB(long mob, SPECIAL(fname));
extern void add_phone_to_list(struct obj_data *obj);
extern void weight_change_object(struct obj_data * obj, float weight);
struct landlord *landlords = NULL;
ACMD_CONST(do_say);

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
bool House_get_filename(vnum_t vnum, char *filename)
{
  if (vnum == NOWHERE)
    return FALSE;

  sprintf(filename, "house/%ld.house", vnum);
  return TRUE;
}

// Same, but for storage.
bool Storage_get_filename(vnum_t vnum, char *filename)
{
  if (vnum == NOWHERE) 
    return FALSE;

  sprintf(filename, "storage/%ld", vnum);
  return TRUE;
}

/* Load all objects for a house */
bool House_load(struct house_control_rec *house)
{
  vnum_t room;
  File fl;
  char fname[MAX_STRING_LENGTH];
  rnum_t rnum;
  struct obj_data *obj = NULL, *last_obj = NULL, *attach = NULL;
  long vnum;
  int inside = 0, last_in = 0;

  room = house->vnum;
  if ((rnum = real_room(room)) == NOWHERE)
    return FALSE;
  if (!House_get_filename(room, fname))
    return FALSE;
  if (!(fl.Open(fname, "r+b"))) /* no file found */
    return FALSE;
  VTable data;
  data.Parse(&fl);
  fl.Close();
  for (int i = 0; i < MAX_GUESTS; i++)
  {
    sprintf(buf, "GUESTS/Guest%d", i);
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
    sprintf(buf, "%s/Vnum", sect_name);
    vnum = data.GetLong(buf, 0);
    if ((obj = read_object(vnum, VIRTUAL))) {
      // Wipe its cost-- we're restoring from the saved value.
      GET_OBJ_COST(obj) = 0;
      
      sprintf(buf, "%s/Name", sect_name);
      obj->restring = str_dup(data.GetString(buf, NULL));
      sprintf(buf, "%s/Photo", sect_name);
      obj->photo = str_dup(data.GetString(buf, NULL));
      for (x = 0; x < NUM_VALUES; x++) {
        sprintf(buf, "%s/Value %d", sect_name, x);
        GET_OBJ_VAL(obj, x) = data.GetInt(buf, GET_OBJ_VAL(obj, x));
      }
      if (GET_OBJ_TYPE(obj) == ITEM_PHONE && GET_OBJ_VAL(obj, 2))
        add_phone_to_list(obj);
      if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && IS_GUN(GET_OBJ_VAL(obj, 3)))
        for (int q = ACCESS_LOCATION_TOP; q <= ACCESS_LOCATION_UNDER; q++)
          if (GET_OBJ_VAL(obj, q) > 0 && real_object(GET_OBJ_VAL(obj, q)) > 0 && 
             (attach = &obj_proto[real_object(GET_OBJ_VAL(obj, q))])) {
            // We know the attachment code will throw a fit if we attach over the top of an 'existing' object, so wipe it out without removing it.
            GET_OBJ_VAL(obj, i) = 0;
            attach_attachment_to_weapon(attach, obj, NULL, i - ACCESS_ACCESSORY_LOCATION_DELTA);
          }
      sprintf(buf, "%s/Condition", sect_name);
      GET_OBJ_CONDITION(obj) = data.GetInt(buf, GET_OBJ_CONDITION(obj));
      sprintf(buf, "%s/Timer", sect_name);
      GET_OBJ_TIMER(obj) = data.GetInt(buf, GET_OBJ_TIMER(obj));
      sprintf(buf, "%s/Attempt", sect_name);
      GET_OBJ_ATTEMPT(obj) = data.GetInt(buf, 0);
      sprintf(buf, "%s/Cost", sect_name);
      // At this point, the cost is restored to a positive value. MAX() guards against edge case of attachment being edited after it was attached.
      GET_OBJ_COST(obj) += MAX(0, data.GetInt(buf, GET_OBJ_COST(obj)));
      sprintf(buf, "%s/Inside", sect_name);
      inside = data.GetInt(buf, 0);
      if (inside > 0) {
        if (inside == last_in)
          last_obj = last_obj->in_obj;
        else if (inside < last_in)
          while (inside <= last_in) {
            last_obj = last_obj->in_obj;
            last_in--;
          }
        if (last_obj)
          obj_to_obj(obj, last_obj);
      } else
        obj_to_room(obj, &world[rnum]);

      last_in = inside;
      last_obj = obj;
    }
  }
  return TRUE;
}

void House_save(struct house_control_rec *house, FILE *fl, long rnum)
{
  int level = 0, o = 0;
  struct obj_data *obj = NULL;
  
#define FILEBUF_SIZE 8192
  char print_buffer[FILEBUF_SIZE];
  setvbuf(fl, print_buffer, _IOFBF, FILEBUF_SIZE);
#undef FILEBUF_SIZE
  
  // Are we saving an apartment? If so, store guest list.
  if (house) {    
    fprintf(fl, "[GUESTS]\n");
    for (o = 0; o < MAX_GUESTS; o++) {
      // Don't write out empty guest slots.
      if (house->guests[o])
        fprintf(fl, "\tGuest%d:\t%ld\n", o, house->guests[o]);
    }
  } 

  obj = world[rnum].contents;
  
  fprintf(fl, "[HOUSE]\n");
  o = 0;
  
#define PRINT_TO_FILE_IF_UNCHANGED(sectname, obj_val, proto_val) { \
  if (obj_val != proto_val)                                        \
    fprintf(fl, (sectname), (obj_val));                            \
}
  
  struct obj_data *prototype = NULL;
  for (;obj;)
  {
    prototype = &obj_proto[real_object(GET_OBJ_VNUM(obj))];
    if (!IS_OBJ_STAT(obj, ITEM_NORENT) && GET_OBJ_TYPE(obj) != ITEM_KEY) {
      fprintf(fl, "\t[Object %d]\n", o);
      o++;
      fprintf(fl, "\t\tVnum:\t%ld\n", GET_OBJ_VNUM(obj));
      fprintf(fl, "\t\tInside:\t%d\n", level);
      if (GET_OBJ_TYPE(obj) == ITEM_PHONE) {
        for (int x = 0; x < 4; x++)
          if (GET_OBJ_VAL(obj, x) != GET_OBJ_VAL(prototype, x))
            fprintf(fl, "\t\tValue %d:\t%d\n", x, GET_OBJ_VAL(obj, x));
      }
      else if (GET_OBJ_TYPE(obj) != ITEM_WORN)
        for (int x = 0; x < NUM_VALUES; x++)
          if (GET_OBJ_VAL(obj, x) != GET_OBJ_VAL(prototype, x))
            fprintf(fl, "\t\tValue %d:\t%d\n", x, GET_OBJ_VAL(obj, x));
      PRINT_TO_FILE_IF_UNCHANGED("\t\tCondition:\t%d\n", GET_OBJ_CONDITION(obj), GET_OBJ_CONDITION(prototype));
      PRINT_TO_FILE_IF_UNCHANGED("\t\tTimer:\t%d\n", GET_OBJ_TIMER(obj), GET_OBJ_TIMER(prototype));
      PRINT_TO_FILE_IF_UNCHANGED("\t\tAttempt:\t%d\n", GET_OBJ_ATTEMPT(obj), 0);
      PRINT_TO_FILE_IF_UNCHANGED("\t\tCost:\t%d\n", GET_OBJ_COST(obj), GET_OBJ_COST(prototype));
      PRINT_TO_FILE_IF_UNCHANGED("\t\tExtraFlags:\t%s\n", GET_OBJ_EXTRA(obj).ToString(), GET_OBJ_EXTRA(prototype).ToString());
      if (obj->restring)
        fprintf(fl, "\t\tName:\t%s\n", obj->restring);
      if (obj->photo)
        fprintf(fl, "\t\tPhoto:$\n%s~\n", cleanup(buf2, obj->photo));
    }

    if (obj->contains && !IS_OBJ_STAT(obj, ITEM_NORENT) && GET_OBJ_TYPE(obj) != ITEM_PART) {
      obj = obj->contains;
      level++;
      continue;
    } else if (!obj->next_content && obj->in_obj)
      while (obj && !obj->next_content && level >= 0) {
        obj = obj->in_obj;
        level--;
      }

    if (obj)
      obj = obj->next_content;
  }
}

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
  FILE *fp;
  struct house_control_rec *house = NULL;
  struct obj_data *obj = NULL;

  // House does not exist? Skip.
  if ((rnum = real_room(vnum)) == NOWHERE)
    return;
    
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
    if (!House_get_filename(vnum, buf))
      return;
  } else
    if (!Storage_get_filename(vnum, buf))
      return;
  
  // If it has no guests and no objects, why save it?
  if (!should_we_keep_this_file) {
    House_delete_file(vnum, buf);
    return;
  }
    
  // Can't open the house file? Oof.
  if (!(fp = fopen(buf, "wb"))) {
    perror("SYSERR: Error saving house file");
    return;
  }
  
  // Save it.
  House_save(find_house(vnum), fp, rnum);
  fclose(fp);
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
      sprintf(buf, "SYSERR: Error deleting house file #%ld. (1)", vnum);
      perror(buf);
    }
    return;
  }
  fclose(fl);
  if (unlink(name) < 0) {
    sprintf(buf, "SYSERR: Error deleting house file #%ld. (2)", vnum);
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
  send_to_char(ch, "The following rooms are free: \r\n");
  send_to_char(ch, "Name     Class     Price      Name     Class     Price\r\n");
  send_to_char(ch, "-----    ------    ------     -----    ------    -----\r\n");
  
  bool on_first_entry_in_column = TRUE;
  for (struct house_control_rec *room_record = lord->rooms; room_record; room_record = room_record->next) {
    if (!room_record->owner) {
      if (on_first_entry_in_column) {
        sprintf(buf, "%-5s    %-6s    %-8d",
                room_record->name,
                lifestyle[room_record->mode].name,
                lord->basecost * lifestyle[room_record->mode].cost);
        on_first_entry_in_column = FALSE;
      } else {
        sprintf(buf2, "   %-5s    %-6s    %-8d\r\n",
                room_record->name,
                lifestyle[room_record->mode].name,
                lord->basecost * lifestyle[room_record->mode].cost);
        strcat(buf, buf2);
        send_to_char(buf, ch);
        on_first_entry_in_column = TRUE;
      }
    }
  }
  if (!on_first_entry_in_column)
    strcat(buf, "\r\n\n");
  else
    sprintf(buf, "\r\n");
  send_to_char(buf, ch);
}

SPECIAL(landlord_spec)
{
  struct char_data *recep = (struct char_data *) me;
  struct obj_data *obj = NULL;
  struct landlord *lord;
  struct house_control_rec *room_record;
  char buf3[MAX_STRING_LENGTH];

  if (!(CMD_IS("list") || CMD_IS("retrieve") || CMD_IS("lease")
        || CMD_IS("leave") || CMD_IS("pay") || CMD_IS("status")))
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
      do_say(recep, "Which room is that?", 0, 0);
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
      do_say(recep, "Which room is that?", 0, 0);
      return TRUE;
    } else if (room_record->owner) {
      do_say(recep, "Sorry, I'm afraid that room is already taken.", 0, 0);
      return TRUE;
    } else if (ch_already_rents_here(lord->rooms, ch)) {
      do_say(recep, "Sorry, we only allow people to lease one room at a time here.", 0, 0);
      return TRUE;
    }
    int cost = lord->basecost * lifestyle[room_record->mode].cost, origcost = cost;

    sprintf(buf3, "That will be %d nuyen.", cost);
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
        GET_BANK(ch) -= cost;
      } else {
        do_say(recep, "Sorry, you don't have the required funds.", 0, 0);
        return TRUE;
      }
    } else {
      GET_NUYEN(ch) -= cost;
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
      sprintf(buf, "SYSERR: Room record for %ld specifies key vnum %ld, but it does not exist! '%s' (%ld) can't access %s apartment.",
              room_record->vnum, room_record->key, GET_CHAR_NAME(ch), GET_IDNUM(ch), HSHR(ch));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      do_say(recep, "I seem to have misplaced your key. I've refunded you the nuyen in cash.", 0, 0);
      GET_NUYEN(ch) += cost;
    } else {
      do_say(recep, "Thank you, here is your key.", 0, 0);
      obj_to_char(key, ch);
      room_record->owner = GET_IDNUM(ch);
      room_record->date = time(0) + (SECS_PER_REAL_DAY*30);
      ROOM_FLAGS(&world[real_room(room_record->vnum)]).SetBit(ROOM_HOUSE);
      ROOM_FLAGS(&world[room_record->atrium]).SetBit(ROOM_ATRIUM);
      House_crashsave(room_record->vnum);
      House_save_control();
    }
    return TRUE;
  } else if (CMD_IS("leave")) {
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
      ROOM_FLAGS(&world[real_room(room_record->vnum)]).RemoveBit(ROOM_HOUSE);
      ROOM_FLAGS(&world[room_record->atrium]).RemoveBit(ROOM_ATRIUM);
      House_save_control();
      House_get_filename(room_record->vnum, buf2);
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
      sprintf(buf3, "That will be %d nuyen.", cost);
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
          GET_BANK(ch) -= cost;
        } else {
          do_say(recep, "Sorry, you don't have the required funds.", 0, 0);
          return TRUE;
        }
      } else {
        GET_NUYEN(ch) -= cost;
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
      if (room_record->date - time(0) < 0)
        strcpy(buf2, "Your rent has expired on that apartment.");
      else sprintf(buf2, "You are paid for another %d days.", (int)((room_record->date - time(0)) / 86400));
      do_say(recep, buf2, 0, 0);
    }
    return TRUE;
  }
  return FALSE;
}
/* call from boot_db - will load control recs, load objs, set atrium bits */
/* should do sanity checks on vnums & remove invalid records */
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
    for (int x = 0; x < templ->num_room; x++) {
      get_line(fl, line);
      if (sscanf(line, "%ld %ld %d %d %s %ld %d %ld", &house_vnum, &key_vnum, t, t + 1, name,
                 &owner, t + 2, &paid) != 8) {
        fprintf(stderr, "Format error in landlord #%d room #%d.\r\n", i, x);
        return;
      }
      if (real_room(house_vnum) < 0) {
        log_vfprintf("SYSERR: House vnum %ld does not match up with a real room. Terminating.\r\n", landlord_vnum);
        exit(ERROR_CANNOT_RESOLVE_VNUM);
      }
      
      temp = new struct house_control_rec;
      temp->vnum = house_vnum;
      temp->key = key_vnum;
      temp->atrium = TOROOM(real_room(house_vnum), t[0]);
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
        if (does_player_exist(temp->owner) && temp->date > time(0))
          House_load(temp);
        else {
          temp->owner = 0;
          House_get_filename(temp->vnum, buf2);
          House_delete_file(temp->vnum, buf2);
          
          // Move all vehicles from this apartment to a public garage.
          struct veh_data *veh = NULL;
          while (world[real_room(temp->vnum)].vehicles) {
            veh = world[real_room(temp->vnum)].vehicles;
#ifdef DEBUG
            sprintf(buf, "debug: Shifting vehicle '%s' (%ld) from '%s' (%ld) to '%s' (%ld).",
                    veh->description, veh->idnum,
                    world[real_room(temp->vnum)].name,
                    world[real_room(temp->vnum)].number,
                    world[real_room(RM_SEATTLE_PARKING_GARAGE)].name,
                    world[real_room(RM_SEATTLE_PARKING_GARAGE)].number);
            mudlog(buf, NULL, LOG_SYSLOG, TRUE);
#endif
            veh_from_room(veh);
            veh_to_room(veh, &world[real_room(RM_SEATTLE_PARKING_GARAGE)]);
          }
        }
      }
      if (last)
        last->next = temp;
      if (!first)
        first = temp;
      if (temp->owner) {
        ROOM_FLAGS(&world[real_room(temp->vnum)]).SetBit(ROOM_HOUSE);
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
}

/* "House Control" functions */

const char *HCONTROL_FORMAT =
  "Usage:  hcontrol destroy <house vnum>\r\n"
  "       hcontrol show\r\n";
void hcontrol_list_houses(struct char_data *ch)
{
  char *own_name;

  strcpy(buf, "Address  Atrium  Guests  Owner\r\n");
  strcat(buf, "-------  ------  ------  ------------\r\n");
  send_to_char(buf, ch);

  for (struct landlord *llord = landlords; llord; llord = llord->next)
    for (struct house_control_rec *house = llord->rooms; house; house = house->next)
      if (house->owner)
      {
        own_name = get_player_name(house->owner);
        if (!own_name)
          own_name = str_dup("<UNDEF>");
        sprintf(buf, "%7ld %7ld    0     %-12s\r\n",
                house->vnum, world[house->atrium].number, CAP(own_name));
        send_to_char(buf, ch);
        DELETE_ARRAY_IF_EXTANT(own_name);
      }
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

    House_get_filename(i->vnum, buf2);
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


/* The hcontrol command itself, used by imms to create/destroy houses */
ACMD(do_hcontrol)
{
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];

  half_chop(argument, arg1, arg2);

  if (is_abbrev(arg1, "destroy"))
    hcontrol_destroy_house(ch, arg2);
  else if (is_abbrev(arg1, "show"))
    hcontrol_list_houses(ch);
  else
    send_to_char(HCONTROL_FORMAT, ch);
}


ACMD(do_decorate)
{
  extern void write_world_to_disk(int vnum);
  struct house_control_rec *i;
  if (!ROOM_FLAGGED(ch->in_room, ROOM_HOUSE))
    send_to_char("You must be in your house to set the description.\r\n", ch);
  else if ((i = find_house(GET_ROOM_VNUM(ch->in_room))) == NULL)
    send_to_char("Um.. this house seems to be screwed up.\r\n", ch);
  else if (GET_IDNUM(ch) != i->owner)
    send_to_char("Only the primary owner can set the room description.\r\n", ch);
  else {
    PLR_FLAGS(ch).SetBit(PLR_WRITING);
    send_to_char("Enter your new room description.  Terminate with a @ on a new line.\r\n", ch);
    act("$n starts to move things around the room.", TRUE, ch, 0, 0, TO_ROOM);
    STATE(ch->desc) = CON_DECORATE;
    DELETE_D_STR_IF_EXTANT(ch->desc);
    INITIALIZE_NEW_D_STR(ch->desc);
    ch->desc->max_str = MAX_MESSAGE_LENGTH;
    ch->desc->mail_to = 0;
  }
}

/* The house command, used by mortal house owners to assign guests */
ACMD(do_house)
{
  struct house_control_rec *i = NULL;
  int j, id;

  one_argument(argument, arg);

  if (!ROOM_FLAGGED(ch->in_room, ROOM_HOUSE))
    send_to_char("You must be in your house to set guests.\r\n", ch);
  else if ((i = find_house(GET_ROOM_VNUM(ch->in_room))) == NULL)
    send_to_char("Um.. this house seems to be screwed up.\r\n", ch);
  else if (GET_IDNUM(ch) != i->owner)
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

  if (IS_NPC(ch) || !room)
    return FALSE;
  if (GET_LEVEL(ch) >= LVL_ADMIN || GET_IDNUM(ch) == room->owner || IS_ASTRAL(ch))
    return TRUE;
  for (j = 0; j < MAX_GUESTS; j++)
    if (GET_IDNUM(ch) == room->guests[j])
      return TRUE;

  return FALSE;
}

void House_list_guests(struct char_data *ch, struct house_control_rec *i, int quiet)
{
  int j;
  char buf[MAX_STRING_LENGTH], buf2[MAX_NAME_LENGTH + 2];

  strcpy(buf, "  Guests: ");
  int x = 0;
  for (j = 0; j < MAX_GUESTS; j++)
  {
    if (i->guests[j] == 0)
      continue;
    
    const char *temp = get_player_name(i->guests[j]);

    if (!temp)
      continue;

    sprintf(buf2, "%s, ", temp);
    strcat(buf, CAP(buf2));
    x++;
    DELETE_ARRAY_IF_EXTANT(temp);
  }
  if (!x)
    strcat(buf, "None");
  strcat(buf, "\r\n");
  send_to_char(buf, ch);
}
