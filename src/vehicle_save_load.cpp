#include <fstream>
#include <iostream>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <vector>
#include <sstream>
#include <algorithm>
#include <unordered_map>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
namespace bf = boost::filesystem;

#include "structs.hpp"
#include "perfmon.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "handler.hpp"
#include "dblist.hpp"
#include "file.hpp"
#include "vtable.hpp"
#include "utils.hpp"
#include "bullet_pants.hpp"
#include "vehicles.hpp"
#include "awake.hpp"

extern char *cleanup(char *dest, const char *src);
extern void auto_repair_obj(struct obj_data *obj, idnum_t owner);
extern void add_phone_to_list(struct obj_data *);
extern void handle_weapon_attachments(struct obj_data *obj);

bool veh_is_in_junkyard(struct veh_data *veh);
bool veh_is_in_crusher_queue(struct veh_data *veh);
bool should_save_this_vehicle(struct veh_data *veh, char *reason);
int get_obj_vehicle_load_usage(struct obj_data *obj, bool is_installed_mod);

#ifdef IS_BUILDPORT
  bf::path global_vehicles_dir = bf::system_complete("lib") / "buildport-vehicles";
#else
  bf::path global_vehicles_dir = bf::system_complete("lib") / "vehicles";
#endif

// TODO: extraction
// TODO: vedit reassignation
// TODO: anywhere else a vehicle is moved around in memory
std::unordered_map<std::string, struct veh_data *> veh_map = {};

const char *get_veh_save_key(idnum_t owner, vnum_t vnum, idnum_t idnum) {
  static char veh_save_key[100];
  snprintf(veh_save_key, sizeof(veh_save_key), "%ld/%ld_%ld", owner, vnum, idnum);
  return veh_save_key;
}

// Create what's hopefully a unique value to reference this vehicle with.
const char *get_veh_save_key(struct veh_data *veh) {
  return get_veh_save_key(veh->owner, GET_VEH_VNUM(veh), GET_VEH_IDNUM(veh));
}

void add_veh_to_map(struct veh_data *veh) {
  // Stick it in our map for future lookup.
  veh_map[std::string(get_veh_save_key(veh))] = veh;
}

void delete_veh_from_map(struct veh_data *veh) {
  // Remove vehicle from our map.
  veh_map.erase(std::string(get_veh_save_key(veh)));
}

bool key_is_in_map(const char *save_key) {
  try {
    veh_map.at(std::string(save_key));
    return TRUE;
  } catch (std::out_of_range) {
    return FALSE;
  }
}

bool veh_is_in_map(struct veh_data *veh) {
  return key_is_in_map(get_veh_save_key(veh));
}

vnum_t junkyard_room_numbers[] = {
  RM_JUNKYARD_GATES, // Just Inside the Gates
  RM_JUNKYARD_PARTS, // Rounding a Tottering Pile of Drone Parts
  RM_JUNKYARD_GLASS, // Beside a Mound of Glass
  RM_JUNKYARD_APPLI, // Amidst Aging Appliances
  RM_JUNKYARD_ELECT  // The Electronics Scrapheap
};

vnum_t boneyard_room_numbers[] = {
  RM_BONEYARD_WRECK_ROOM
};

void delete_veh_file(struct veh_data *veh, const char *reason) {
  if (veh->owner == 0)
    return;

  bf::path owner_dir = global_vehicles_dir / vnum_to_string(veh->owner);

  if (!bf::exists(owner_dir))
    return;

  bf::path veh_file = global_vehicles_dir / get_veh_save_key(veh);

  if (!bf::exists(veh_file))
    return;

  log_vfprintf("Deleting veh file %s: %s.", veh_file.c_str(), reason);
  bf::remove(veh_file);
}

bool save_single_vehicle(struct veh_data *veh, bool fromCopyover) {
  FILE *fl;
  struct room_data *temp_room = NULL;
  struct char_data *i;
  struct obj_data *obj;

  // Skip disqualified vehicles.
  char deletion_reason[1000];
  if (!should_save_this_vehicle(veh, deletion_reason)) {
    // mudlog_vfprintf(NULL, LOG_GRIDLOG, "Refusing to save vehicle %s: Not a candidate for saving.", GET_VEH_NAME(veh));
    delete_veh_file(veh, deletion_reason);
    return FALSE;
  }

  // Previously, ownerless vehicles were saved, allowing them to disgorge contents on next load. This is bad for the economy. Instead, we just drop them.
  if (veh->owner > 0 && !does_player_exist(veh->owner)) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Vehicle '%s' no longer has a valid owner (was %ld). Locking it and refusing to save it.", GET_VEH_NAME(veh), veh->owner);
    // Auto-lock the doors to prevent cheese.
    veh->locked = TRUE;
    // Clear the owner. This vehicle will be DQ'd in should_save_this_vehicle() henceforth.
    set_veh_owner(veh, 0);
    return FALSE;
  }

  // Ensure our owner directory exists.
  bf::path owner_dir = global_vehicles_dir / vnum_to_string(veh->owner);
  if (!bf::exists(owner_dir)) {
    bf::create_directory(owner_dir);
  }
  bf::path veh_file_path = global_vehicles_dir / get_veh_save_key(veh);

  if (!(fl = fopen(veh_file_path.c_str(), "w"))) {
    mudlog("SYSERR: Can't Open Vehicle File For Write.", NULL, LOG_SYSLOG, FALSE);
    return FALSE;
  }

  if (veh->sub) {
    for (i = character_list; i; i = i->next_in_character_list) {
      if (GET_IDNUM(i) == veh->owner) {
        struct veh_data *f = NULL;
        for (f = i->char_specials.subscribe; f; f = f->next_sub)
          if (f == veh)
            break;
        if (!f) {
          veh->next_sub = i->char_specials.subscribe;

          // Doubly link it into the list.
          if (i->char_specials.subscribe)
            i->char_specials.subscribe->prev_sub = veh;

          i->char_specials.subscribe = veh;
        }
        break;
      }
    }
  }

  // Any vehicle that's destroyed and not being actively tended to (in a vehicle, in a garage) gets sent to either the junkyard or the crusher.
  if (veh->damage >= VEH_DAM_THRESHOLD_DESTROYED && !(veh->in_veh || (veh->in_room && ROOM_FLAGGED(veh->in_room, ROOM_GARAGE)))) {
    int junkyard_number;

    // If it's already in the junkyard, move it to the crusher, but only on crashes.
    // Vehicles in the crusher will not be saved, effectively destroying them.
    if (!fromCopyover && veh_is_in_junkyard(veh)) {
      junkyard_number = RM_VEHICLE_CRUSHER;
    } else {
      // Pick a spot and put the vehicle there. Sort roughly based on type.
      if (veh_is_aircraft(veh)) {
        junkyard_number = RM_BONEYARD_WRECK_ROOM;
      } else {
        switch (veh->type) {
          case VEH_DRONE:
            // Drones in the drone spot.
            junkyard_number = RM_JUNKYARD_PARTS;
            break;
          case VEH_BIKE:
            // Bikes in the bike spot.
            junkyard_number = RM_JUNKYARD_BIKES;
            break;
          case VEH_CAR:
          case VEH_TRUCK:
            // Standard vehicles just inside the gates.
            junkyard_number = RM_JUNKYARD_GATES;
            break;
          default:
            // Pick a random one to scatter them about.
            junkyard_number = junkyard_room_numbers[number(0, NUM_JUNKYARD_ROOMS - 1)];
            break;
        }
      }
    }
    
    temp_room = &world[real_room(junkyard_number)];
  } else {
    // If veh is not in a garage (or the owner is not allowed to enter the house anymore), send it to a garage.
    temp_room = get_veh_in_room(veh);

    // No temp room means it's towed. Yolo it into the Seattle garage.
    if (!temp_room) {
      // snprintf(buf, sizeof(buf), "Falling back to Seattle garage for non-veh, non-room veh %s.", GET_VEH_NAME(veh));
      // log(buf);
      if (veh_is_aircraft(veh)) {
        temp_room = &world[real_room(RM_BONEYARD_INTACT_ROOM_1)];
      } else {
        temp_room = &world[real_room(RM_SEATTLE_PARKING_GARAGE)];
      }
    } else {
      // Otherwise, derive the garage from its location.
      bool room_is_valid_garage = IDNUM_CAN_ENTER_APARTMENT(temp_room, veh->owner);
      if (veh_is_aircraft(veh)) {
        room_is_valid_garage &= ROOM_FLAGGED(temp_room, ROOM_RUNWAY) || ROOM_FLAGGED(temp_room, ROOM_HELIPAD) || ROOM_FLAGGED(temp_room, ROOM_GARAGE);
      } else {
        room_is_valid_garage &= ROOM_FLAGGED(temp_room, ROOM_GARAGE);
      }
      
      if (!fromCopyover && !room_is_valid_garage) {
        /* snprintf(buf, sizeof(buf), "Falling back to a garage for non-garage-room veh %s (in '%s' %ld).",
                    GET_VEH_NAME(veh), GET_ROOM_NAME(temp_room), GET_ROOM_VNUM(temp_room));
        log(buf); */
        if (veh_is_aircraft(veh)) {
          if (dice(1, 2) == 1) {
            temp_room = &world[real_room(RM_BONEYARD_INTACT_ROOM_1)];
          } else {
            temp_room = &world[real_room(RM_BONEYARD_INTACT_ROOM_2)];
          }
        } else {
          switch (GET_JURISDICTION(temp_room)) {
            case ZONE_SEATTLE:
              temp_room = &world[real_room(RM_SEATTLE_PARKING_GARAGE)];
              break;
            case ZONE_CARIB:
              temp_room = &world[real_room(RM_CARIB_PARKING_GARAGE)];
              break;
            case ZONE_OCEAN:
              temp_room = &world[real_room(RM_OCEAN_PARKING_GARAGE)];
              break;
            case ZONE_PORTLAND:
#ifdef USE_PRIVATE_CE_WORLD
              switch (number(0, 2)) {
                case 0:
                  temp_room = &world[real_room(RM_PORTLAND_PARKING_GARAGE1)];
                  break;
                case 1:
                  temp_room = &world[real_room(RM_PORTLAND_PARKING_GARAGE2)];
                  break;
                case 2:
                  temp_room = &world[real_room(RM_PORTLAND_PARKING_GARAGE3)];
                  break;
              }
#else
              temp_room = &world[real_room(RM_PORTLAND_PARKING_GARAGE)];
#endif
              break;
          }
        }
      }
    }
  }
  fprintf(fl, "[METADATA]\n");
  fprintf(fl, "\tVersion:\t%d\n", VERSION_VEH_FILE);
  fprintf(fl, "[VEHICLE]\n");
  fprintf(fl, "\tVnum:\t%ld\n", veh_index[veh->veh_number].vnum);
  fprintf(fl, "\tOwner:\t%ld\n", veh->owner);
  fprintf(fl, "\tInRoom:\t%ld\n", temp_room->number);
  fprintf(fl, "\tSubscribed:\t%d\n", veh->sub);
  fprintf(fl, "\tDamage:\t%d\n", veh->damage);
  fprintf(fl, "\tSpare:\t%ld\n", veh->spare);
  fprintf(fl, "\tIdnum:\t%ld\n", veh->idnum);

  rnum_t rnum = real_vehicle(GET_VEH_VNUM(veh));
  if (rnum < 0 || veh->flags != veh_proto[rnum].flags)
    fprintf(fl, "\tFlags:\t%s\n", veh->flags.ToString());
  if (veh->in_veh)
    fprintf(fl, "\tInVeh:\t%ld\n", veh->in_veh->idnum);
  if (veh->restring)
    fprintf(fl, "\tVRestring:\t%s\n", veh->restring);
  if (veh->restring_long)
    fprintf(fl, "\tVRestringLong:$\n%s~\n", cleanup(buf2, veh->restring_long));
  if (veh->decorate_front)
    fprintf(fl, "\tVDecorateFront:$\n%s~\n", cleanup(buf2, veh->decorate_front));
  if (veh->decorate_rear)
    fprintf(fl, "\tVDecorateRear:$\n%s~\n", cleanup(buf2, veh->decorate_rear));
  fprintf(fl, "[CONTENTS]\n");
  int o = 0, level = 0;
  std::vector<std::string> obj_strings;
  std::stringstream obj_string_buf;
  for (obj = veh->contents;obj;) {
    if (!IS_OBJ_STAT(obj, ITEM_EXTRA_NORENT)) {
      obj_string_buf << "\t\tVnum:\t" << GET_OBJ_VNUM(obj) << "\n";
      obj_string_buf << "\t\tInside:\t" << level << "\n";

      switch (GET_OBJ_TYPE(obj)) {
        case ITEM_PHONE:
          for (int x = 0; x < 4; x++) {
            obj_string_buf << "\t\tValue " << x << ":\t" << GET_OBJ_VAL(obj, x) <<"\n";
          }
          break;
        case ITEM_WORN:
          // The only thing we save is the hardened armor bond status.
          if (IS_OBJ_STAT(obj, ITEM_EXTRA_HARDENED_ARMOR)) {
            obj_string_buf << "\t\tValue " << WORN_OBJ_HARDENED_ARMOR_SLOT << ":\t" << GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(obj) << "\n";
          } else if (GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(obj)) {
            mudlog_vfprintf(obj->worn_by, LOG_SYSLOG, "SYSERR: Worn item %s (%ld) has the customized armor value set to %d, but it's not hardened armor! Saving anyways.",
                            GET_OBJ_NAME(obj),
                            GET_OBJ_VNUM(obj),
                            GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(obj));
            obj_string_buf << "\t\tValue " << WORN_OBJ_HARDENED_ARMOR_SLOT << ":\t" << GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(obj) << "\n";
          }
          break;
        default:
          for (int x = 0; x < NUM_OBJ_VALUES; x++) {
            obj_string_buf << "\t\tValue " << x << ":\t" << GET_OBJ_VAL(obj, x) << "\n";
          }
          break;
      }
      
      obj_string_buf << "\t\tCondition:\t" << (int) GET_OBJ_CONDITION(obj) << "\n";
      obj_string_buf << "\t\tCost:\t" << GET_OBJ_COST(obj) << "\n";
      obj_string_buf << "\t\tTimer:\t" << GET_OBJ_TIMER(obj) << "\n";
      obj_string_buf << "\t\tAttempt:\t" << GET_OBJ_ATTEMPT(obj) << "\n";
      obj_string_buf << "\t\tExtraFlags:\t" << GET_OBJ_EXTRA(obj).ToString() << "\n";
      obj_string_buf <<  "\t\tFront:\t" << obj->vfront << "\n";
      if (obj->restring)
        obj_string_buf << "\t\tName:\t" << obj->restring << "\n";
      if (obj->photo)
        obj_string_buf << "\t\tPhoto:$\n" << cleanup(buf2, obj->photo) << "~\n";
      if (obj->graffiti)
        obj_string_buf << "\t\tGraffiti:$\n" << cleanup(buf2, obj->graffiti) << "~\n";

      obj_string_buf << "\t\t" << FILESTRING_OBJ_IDNUM << ":\t" << GET_OBJ_IDNUM(obj) << "\n";

      obj_strings.push_back(obj_string_buf.str());
      obj_string_buf.str(std::string());
    }

    if (obj->contains && !IS_OBJ_STAT(obj, ITEM_EXTRA_NORENT) && GET_OBJ_TYPE(obj) != ITEM_PART) {
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

  if (!obj_strings.empty()) {
    int i = 0;
    for(std::vector<std::string>::reverse_iterator rit = obj_strings.rbegin(); rit != obj_strings.rend(); rit++ ) {
      fprintf(fl, "\t[Object %d]\n", i);
      fprintf(fl, "%s", rit->c_str());
      i++;
    }
    obj_strings.clear();
  }

  fprintf(fl, "[MODIS]\n");
  for (int x = 0, v = 0; x < NUM_MODS; x++) {
    if (x == MOD_MOUNT)
      continue;
      
    if (GET_MOD(veh, x)) {
      fprintf(fl, "\tMod%d:\t%ld\n", v, GET_OBJ_VNUM(GET_MOD(veh, x)));
      v++;
    }
  }
  fprintf(fl, "[MOUNTS]\n");
  int m = 0;
  for (obj = veh->mount; obj; obj = obj->next_content, m++) {
    struct obj_data *ammo = NULL;
    struct obj_data *gun = NULL;

    fprintf(fl, "\t[Mount %d]\n", m);
    fprintf(fl, "\t\tMountNum:\t%ld\n", GET_OBJ_VNUM(obj));
    if ((ammo = get_mount_ammo(obj)) && GET_AMMOBOX_QUANTITY(ammo) > 0) {
      fprintf(fl, "\t\tAmmo:\t%d\n", GET_AMMOBOX_QUANTITY(ammo));
      fprintf(fl, "\t\tAmmoType:\t%d\n", GET_AMMOBOX_TYPE(ammo));
      fprintf(fl, "\t\tAmmoWeap:\t%d\n", GET_AMMOBOX_WEAPON(ammo));
      fprintf(fl, "\t\t%s:\t%lu\n", FILESTRING_OBJ_IDNUM, GET_OBJ_IDNUM(ammo));
    }
    if ((gun = get_mount_weapon(obj))) {
      fprintf(fl, "\t\tVnum:\t%ld\n", GET_OBJ_VNUM(gun));
      fprintf(fl, "\t\tCondition:\t%d\n", GET_OBJ_CONDITION(gun));
      if (gun->restring)
        fprintf(fl, "\t\tName:\t%s\n", gun->restring);
      for (int x = 0; x < NUM_OBJ_VALUES; x++)
        fprintf(fl, "\t\tValue %d:\t%d\n", x, GET_OBJ_VAL(gun, x));
      fprintf(fl, "\t\t%s:\t%lu\n", FILESTRING_OBJ_IDNUM, GET_OBJ_IDNUM(gun));
    }
  }
  fprintf(fl, "[GRIDGUIDE]\n");
  o = 0;
  for (struct grid_data *grid = veh->grid; grid; grid = grid->next) {
    fprintf(fl, "\t[GRID %d]\n", o++);
    fprintf(fl, "\t\tName:\t%s\n", grid->name);
    fprintf(fl, "\t\tRoom:\t%ld\n", grid->room);
  }
  fclose(fl);

  return TRUE;
}

void save_vehicles(bool fromCopyover)
{
  PERF_PROF_SCOPE(pr_, __func__);
  FILE *fl;
  int num_veh = 0;

  for (struct veh_data *veh = veh_list; veh; veh = veh->next) {
    if (save_single_vehicle(veh, fromCopyover))
      num_veh++;
  }

  // Write the count of vehicles to the file.
  if (!(fl = fopen("veh/vfile", "w"))) {
    mudlog("SYSERR: Can't Open Vehicle File For Write.", NULL, LOG_SYSLOG, FALSE);
    return;
  }
  fprintf(fl, "%d\n", num_veh);
  fclose(fl);
}

void load_single_veh(const char *filename) {
  File file;
  int veh_version = 0;
  idnum_t owner;
  vnum_t vnum;
  struct veh_data *veh = NULL;
  struct obj_data *obj, *last_obj = NULL;
  std::vector<nested_obj> contained_obj;
  struct nested_obj contained_obj_entry;

  log_vfprintf("Loading vehicle file %s.", filename);
  if (!(file.Open(filename, "r"))) {
    log_vfprintf("Warning: Unable to open vehfile %s for reading. Skipping.", filename);
  }


  VTable data;
  data.Parse(&file);
  file.Close();

  veh_version = data.GetInt("METADATA/Version", 0);
  owner = data.GetLong("VEHICLE/Owner", 0);
  vnum = data.GetLong("VEHICLE/Vnum", 0);
  int damage = data.GetInt("VEHICLE/Damage", 0);
  idnum_t idnum = data.GetLong("VEHICLE/Idnum", 0);

  if (key_is_in_map(get_veh_save_key(owner, vnum, idnum))) {
    log_vfprintf("Refusing to load vehicle %ld (%ld) owned by %ld: Already loaded.", vnum, idnum, owner);
    return;
  }

  if (!vnum)
    return;

  veh = read_vehicle(vnum, VIRTUAL);

  if (!veh) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Attempted to load vehicle at file %s, but its vnum %ld does not exist in game!", filename, vnum);
    return;
  }

#ifdef USE_DEBUG_CANARIES
  veh->canary = CANARY_VALUE;
  assert(veh->canary == CANARY_VALUE);
#endif

  veh->damage = damage;
  veh->owner = owner;
  veh->idnum = idnum;
  veh->spare = data.GetLong("VEHICLE/Spare", 0);
  veh->spare2 = data.GetLong("VEHICLE/InVeh", 0);
  veh->locked = TRUE;
  veh->sub = data.GetLong("VEHICLE/Subscribed", 0);
  veh->desired_in_room_on_load = data.GetLong("VEHICLE/InRoom", 0);

  // Can't get there? Pull your veh out.
  rnum_t veh_room_rnum = real_room(veh->desired_in_room_on_load);
  if (veh_room_rnum < 0
      || ( veh->owner > 0 
            && world[veh_room_rnum].apartment 
            && !world[veh_room_rnum].apartment->can_enter_by_idnum(veh->owner))) 
  {
    log_vfprintf("Vehicle %ld owned by %ld would have loaded in %s room %ld, sending it to a parking spot.",
                  veh->idnum,
                  veh->owner,
                  veh_room_rnum < 0 ? "non-existent" : "inaccessible",
                  veh->desired_in_room_on_load);

    if (veh_is_aircraft(veh)) {
      veh->desired_in_room_on_load = RM_BONEYARD_INTACT_ROOM_1;
      veh_room_rnum = real_room(veh->desired_in_room_on_load);

      if (veh_room_rnum < 0) {
        log_vfprintf("FATAL ERROR: Boneyard room 1 does not exist, cannot send vehicle there. Add a room at %ld or remove all vehicles.", veh->desired_in_room_on_load);
        shutdown();
        return;
      }
    } else {
      veh->desired_in_room_on_load = RM_SEATTLE_PARKING_GARAGE;
      veh_room_rnum = real_room(veh->desired_in_room_on_load);

      if (veh_room_rnum < 0) {
        log_vfprintf("FATAL ERROR: Seattle parking garage does not exist, cannot send vehicle there. Add a room at %ld or remove all vehicles.", veh->desired_in_room_on_load);
        shutdown();
        return;
      }
    }
  }

  const char *veh_flag_string = data.GetString("VEHICLE/Flags", NULL);
  if (veh_flag_string)
    veh->flags.FromString(veh_flag_string);

  if (!veh->spare2)
    veh_to_room(veh, &world[veh_room_rnum]);
  veh->restring = str_dup(data.GetString("VEHICLE/VRestring", NULL));
  veh->restring_long = str_dup(data.GetString("VEHICLE/VRestringLong", NULL));
  veh->decorate_front = str_dup(data.GetString("VEHICLE/VDecorateFront", NULL));
  veh->decorate_rear = str_dup(data.GetString("VEHICLE/VDecorateRear", NULL));
  int inside = 0, last_inside = 0;
  int num_objs = data.NumSubsections("CONTENTS");

  snprintf(buf3, sizeof(buf3), "veh-load %ld owned by %ld", veh->idnum, veh->owner);
  for (int i = 0; i < num_objs; i++) {
    const char *sect_name = data.GetIndexSection("CONTENTS", i);
    snprintf(buf, sizeof(buf), "%s/Vnum", sect_name);
    vnum = data.GetLong(buf, 0);
    if (vnum > 0 && (obj = read_object(vnum, VIRTUAL))) {
      snprintf(buf, sizeof(buf), "%s/Inside", sect_name);
      inside = data.GetInt(buf, 0);
      for (int x = 0; x < NUM_OBJ_VALUES; x++) {
        snprintf(buf, sizeof(buf), "%s/Value %d", sect_name, x);
        GET_OBJ_VAL(obj, x) = data.GetInt(buf, GET_OBJ_VAL(obj, x));
      }
      if (GET_OBJ_TYPE(obj) == ITEM_PHONE && GET_ITEM_PHONE_SWITCHED_ON(obj))
        add_phone_to_list(obj);
      if (GET_OBJ_TYPE(obj) == ITEM_WEAPON) {
        handle_weapon_attachments(obj);
      }
      snprintf(buf, sizeof(buf), "%s/Condition", sect_name);
      GET_OBJ_CONDITION(obj) = data.GetInt(buf, GET_OBJ_CONDITION(obj));
      snprintf(buf, sizeof(buf), "%s/Cost", sect_name);
      GET_OBJ_COST(obj) = data.GetInt(buf, 0);
      snprintf(buf, sizeof(buf), "%s/Timer", sect_name);
      GET_OBJ_TIMER(obj) = data.GetInt(buf, TRUE);
      snprintf(buf, sizeof(buf), "%s/Attempt", sect_name);
      GET_OBJ_ATTEMPT(obj) = data.GetInt(buf, 0);
      snprintf(buf, sizeof(buf), "%s/ExtraFlags", sect_name);
      GET_OBJ_EXTRA(obj).FromString(data.GetString(buf, "0"));
      snprintf(buf, sizeof(buf), "%s/Front", sect_name);
      obj->vfront = data.GetInt(buf, TRUE);
      snprintf(buf, sizeof(buf), "%s/Name", sect_name);
      obj->restring = str_dup(data.GetString(buf, NULL));
      snprintf(buf, sizeof(buf), "%s/Graffiti", sect_name);
      obj->graffiti = str_dup(data.GetString(buf, NULL));
      snprintf(buf, sizeof(buf), "%s/Photo", sect_name);
      obj->photo = str_dup(data.GetString(buf, NULL));
      snprintf(buf, sizeof(buf), "%s/%s", sect_name, FILESTRING_OBJ_IDNUM);
      GET_OBJ_IDNUM(obj) = data.GetUnsignedLong(buf, 0);

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
        auto_repair_obj(obj, veh->owner);

      if (veh_version == VERSION_VEH_FILE) {
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
              obj_to_veh(obj, veh);

            auto it = std::find_if(contained_obj.begin(), contained_obj.end(), find_level(inside+1));
            while (it != contained_obj.end()) {
              obj_to_obj(it->obj, obj);
              it = contained_obj.erase(it);
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
        } else
          obj_to_veh(obj, veh);

        last_inside = inside;
      }
      // This handles loading old house file format prior to introduction of version number in the file.
      // Version number will always be 0 for this format.
      else if (!veh_version) {
        if (inside > 0) {
          if (inside == last_inside)
            last_obj = last_obj->in_obj;
          else if (inside < last_inside) {
            while (inside <= last_inside && last_obj) {
              if (!last_obj) {
                snprintf(buf2, sizeof(buf2), "Load error: Nested-item load failed for %s. Disgorging to vehicle.", GET_OBJ_NAME(obj));
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
            obj_to_veh(obj, veh);
        } else
          obj_to_veh(obj, veh);
        last_inside = inside;
        last_obj = obj;
      }
      else {
        snprintf(buf2, sizeof(buf2), "Load ERROR: Unknown file format for vehicle ID: %ld. Dumping valid objects to vehicle.", veh->idnum);
        mudlog(buf2, NULL, LOG_SYSLOG, TRUE);
        obj_to_veh(obj, veh);
      }
    }
  }

  if (veh_version == VERSION_VEH_FILE) {
    // Failsafe. If something went wrong and we still have objects stored in the vector, dump them in the room.
    if (!contained_obj.empty()) {
      for (auto it : contained_obj)
        obj_to_veh(it.obj, veh);

      contained_obj.clear();
    }
  }

  int num_mods = data.NumFields("MODIS");
  for (int i = 0; i < num_mods; i++) {
    snprintf(buf, sizeof(buf), "MODIS/Mod%d", i);
    vnum_t vnum = data.GetLong(buf, 0);

    if (!(obj = read_object(vnum, VIRTUAL))) {
      log_vfprintf("ERROR: Unknown vnum %ld in veh file! Skipping.", vnum);
      continue;
    }

    GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(obj)) = obj;
    if (GET_VEHICLE_MOD_TYPE(obj) == TYPE_ENGINECUST)
      veh->engine = GET_VEHICLE_MOD_RATING(obj);
    if (GET_VEHICLE_MOD_TYPE(obj) == TYPE_AUTONAV)
      veh->autonav += GET_VEHICLE_MOD_RATING(obj);
    veh->usedload += get_obj_vehicle_load_usage(obj, TRUE);
    for (int l = 0; l < MAX_OBJ_AFFECT; l++)
      affect_veh(veh, obj->affected[l].location, obj->affected[l].modifier);
  }
  num_mods = data.NumSubsections("GRIDGUIDE");
  for (int i = 0; i < num_mods; i++) {
    struct grid_data *grid = new grid_data;
    const char *sect_name = data.GetIndexSection("GRIDGUIDE", i);
    snprintf(buf, sizeof(buf), "%s/Name", sect_name);
    grid->name = str_dup(data.GetString(buf, NULL));
    snprintf(buf, sizeof(buf), "%s/Room", sect_name);
    grid->room = data.GetLong(buf, 0);
    grid->next = veh->grid;
    veh->grid = grid;
  }
  num_mods = data.NumSubsections("MOUNTS");
  for (int i = 0; i < num_mods; i++) {
    const char *sect_name = data.GetIndexSection("MOUNTS", i);
    snprintf(buf, sizeof(buf), "%s/MountNum", sect_name);
    obj = read_object(data.GetLong(buf, 0), VIRTUAL);
    snprintf(buf, sizeof(buf), "%s/Ammo", sect_name);
    int ammo_qty = data.GetInt(buf, 0);
    if (ammo_qty > 0) {
      struct obj_data *ammo = read_object(OBJ_BLANK_AMMOBOX, VIRTUAL);
      GET_AMMOBOX_QUANTITY(ammo) = ammo_qty;
      snprintf(buf, sizeof(buf), "%s/AmmoType", sect_name);
      GET_AMMOBOX_TYPE(ammo) = data.GetInt(buf, 0);
      snprintf(buf, sizeof(buf), "%s/AmmoWeap", sect_name);
      GET_AMMOBOX_WEAPON(ammo) = data.GetInt(buf, 0);
      
      snprintf(buf, sizeof(buf), "%s/%s", sect_name, FILESTRING_OBJ_IDNUM);
      GET_OBJ_IDNUM(ammo) = data.GetUnsignedLong(buf, 0);

      ammo->restring = str_dup(get_ammobox_default_restring(ammo));
      auto_repair_obj(ammo, veh->owner);
      obj_to_obj(ammo, obj);
    }
    snprintf(buf, sizeof(buf), "%s/Vnum", sect_name);
    int gun = data.GetLong(buf, 0);
    struct obj_data *weapon;
    if (gun && (weapon = read_object(gun, VIRTUAL))) {
      snprintf(buf, sizeof(buf), "%s/Condition", sect_name);
      GET_OBJ_CONDITION(weapon) = data.GetInt(buf, GET_OBJ_CONDITION(weapon));
      snprintf(buf, sizeof(buf), "%s/Name", sect_name);
      weapon->restring = str_dup(data.GetString(buf, NULL));
      for (int x = 0; x < NUM_OBJ_VALUES; x++) {
        snprintf(buf, sizeof(buf), "%s/Value %d", sect_name, x);
        GET_OBJ_VAL(weapon, x) = data.GetInt(buf, GET_OBJ_VAL(weapon, x));
      }

      snprintf(buf, sizeof(buf), "%s/%s", sect_name, FILESTRING_OBJ_IDNUM);
      GET_OBJ_IDNUM(weapon) = data.GetUnsignedLong(buf, 0);

      auto_repair_obj(weapon, veh->owner);
      obj_to_obj(weapon, obj);
      veh->usedload += get_obj_vehicle_load_usage(weapon, FALSE);
    }
    
    veh->sig -= get_mount_signature_penalty(obj);
    veh->usedload += get_obj_vehicle_load_usage(obj, TRUE);
    if (veh->mount)
      obj->next_content = veh->mount;
    veh->mount = obj;
  }

  if (veh->idnum == 0) {
    delete_veh_file(veh, "regenerating idnum, was zero");
    generate_veh_idnum(veh);
  }

  // Stick it in our map for future lookup.
  add_veh_to_map(veh);

  char msg_buf[1000];
  snprintf(msg_buf, sizeof(msg_buf), "You blink and realize %s must have been here the whole time.\r\n", GET_VEH_NAME(veh));
  if (veh->in_room) {
    send_to_room(msg_buf, veh->in_room, veh);
  } else if (veh->in_veh) {
    send_to_veh(msg_buf, veh->in_veh, NULL, FALSE);
  }
}

void load_vehicles_for_idnum(idnum_t owner_id) {
  bf::path owner_dir = global_vehicles_dir / vnum_to_string(owner_id);

  // Skip trying to load vehicles for an owner who has none saved.
  if (!bf::exists(owner_dir))
    return;

  log_vfprintf(" - Loading vehicles from path %s.", owner_dir.c_str());

  bf::directory_iterator dir_end_itr; // default construction yields past-the-end
  for (bf::directory_iterator dir_itr(owner_dir); dir_itr != dir_end_itr; ++dir_itr) {
    // This directory contains owner ID directories.
    if (!is_directory(dir_itr->status())) {
      bf::path filename = dir_itr->path();
      // log_vfprintf(" -- Loading %s.", filename.c_str());
      load_single_veh(filename.c_str());
    }
  }
}

void restore_carried_vehicle_pointers() {
  for (struct veh_data *veh = veh_list; veh; veh = veh->next) {
    if (veh->spare2) {
      for (struct veh_data *veh2 = veh_list; veh2 && !veh->in_veh; veh2 = veh2->next) {
        if (veh->spare2 == veh2->idnum) {
          veh_to_veh(veh, veh2);
          veh->spare2 = 0;
          veh->locked = FALSE;

          #ifdef USE_DEBUG_CANARIES
            assert(veh->canary == CANARY_VALUE);
            assert(veh2->canary == CANARY_VALUE);
          #endif
        }
      }
      if (!veh->in_veh) {
        rnum_t veh_room_rnum = real_room(veh->desired_in_room_on_load);
        if (veh_room_rnum < 0 || (veh->owner > 0 && world[veh_room_rnum].apartment && !world[veh_room_rnum].apartment->can_enter_by_idnum(veh->owner))) {
          veh->desired_in_room_on_load = RM_SEATTLE_PARKING_GARAGE;
          snprintf(buf, sizeof(buf), "SYSERR: Attempted to restore vehicle %s (%ld) inside nonexistent carrier, and no valid room was found! Dumping to Seattle Garage (%ld).\r\n",
                   veh->name, veh->veh_number, veh->desired_in_room_on_load);
        } else {
          snprintf(buf, sizeof(buf), "SYSERR: Attempted to restore vehicle %s (%ld) inside nonexistent carrier! Dumping to room %ld.\r\n",
                  veh->name, veh->veh_number, veh->desired_in_room_on_load);
        }
        mudlog(buf, NULL, LOG_SYSLOG, TRUE);
        veh->spare2 = 0;
        veh_to_room(veh, &world[veh_room_rnum]);

        #ifdef USE_DEBUG_CANARIES
          assert(veh->canary == CANARY_VALUE);
        #endif
      }
    }
  }
}

// aka load_vehs, veh_load, and everything else I keep searching for it by --LS
void load_saved_veh(bool purge_existing)
{
  FILE *fl;
  std::vector<nested_obj> contained_obj;
  struct nested_obj contained_obj_entry;

  if (purge_existing) {
    while (struct veh_data *veh = veh_list) {
      while (struct obj_data *contents = veh->contents) {
        extract_obj(contents);
      }
      extract_veh(veh_list);
    }
  }

  if (!bf::exists(global_vehicles_dir)) {
    // We've never run our migration. Load the old files.
    bf::create_directory(global_vehicles_dir);

    if (!(fl = fopen("veh/vfile", "r"))) {
      log("SYSERR: Could not open vfile for reading.");
      return;
    }

    if (!get_line(fl, buf)) {
      log("SYSERR: Invalid Entry In Vfile.");
      return;
    }
    fclose(fl);
    int num_veh = atoi(buf);

    for (int i = 0; i < num_veh; i++) {
      snprintf(buf, sizeof(buf), "veh/%07d", i);
      load_single_veh(buf);
    }
  } else {
    // Iterate over directories in /vehicles.
    bf::directory_iterator global_dir_end_itr; // default construction yields past-the-end
    for (bf::directory_iterator global_itr(global_vehicles_dir); global_itr != global_dir_end_itr; ++global_itr) {
      // This directory contains owner ID directories.
      if (is_directory(global_itr->status())) {
        // Calculate the owner ID.
        idnum_t owner_id = atol(global_itr->path().filename().c_str());

        // Skip invalid owners.
        if (owner_id == 0)
          continue;
        
        // Check to make sure this PC is active in the last 30 days. If not, don't load their vehicles until they log in.
        if (!purge_existing && !pc_active_in_last_30_days(owner_id)) {
          log_vfprintf(" - Refusing to load vehicles for idnum %ld: Owner is idle.", owner_id);
          continue;
        }

        load_vehicles_for_idnum(owner_id);
      }
    }
  }

  restore_carried_vehicle_pointers();
}


//////////// Helpers.

// Returns TRUE if vehicle is in one of the Junkyard vehicle-depositing rooms, FALSE otherwise.
bool veh_is_in_junkyard(struct veh_data *veh) {
  if (!veh || !veh->in_room)
    return FALSE;

  // Aircraft end up in the Boneyard instead of the Junkyard.
  if (veh_is_aircraft(veh)) {
    for (int index = 0; index < NUM_BONEYARD_ROOMS; index++)
      if (veh->in_room->number == boneyard_room_numbers[index])
        return TRUE;
  } else {
    for (int index = 0; index < NUM_JUNKYARD_ROOMS; index++)
      if (veh->in_room->number == junkyard_room_numbers[index])
        return TRUE;
  }

  return FALSE;
}

bool veh_is_in_crusher_queue(struct veh_data *veh) {
  if (!veh || !veh->in_room)
    return FALSE;

  return GET_ROOM_VNUM(veh->in_room) == RM_VEHICLE_CRUSHER;
}

bool should_save_this_vehicle(struct veh_data *veh, char *reason) {
  // Must be a PC vehicle. We specifically don't check for PC exist-- that's handled in LOADING, not saving.
  if (veh->owner <= 0) {
    strlcpy(reason, "NPC vehicle", sizeof(reason));
    return FALSE;
  }

  /* log_vfprintf("Evaluating save info for PC veh '%s' at '%ld' (damage %d/10)",
               GET_VEH_NAME(veh), get_veh_in_room(veh) ? get_veh_in_room(veh)->number : -1, veh->damage); */

  // If it's thrashed, it must be in the proper location.
  if (veh->damage >= VEH_DAM_THRESHOLD_DESTROYED) {
    // It was in the crusher queue and nobody came to fix it.
    if (veh_is_in_crusher_queue(veh)) {
      strlcpy(reason, "In the crusher queue", sizeof(reason));
      return FALSE;
    }

    // It's being taken care of.
    if (veh->in_veh || (veh->in_room && ROOM_FLAGGED(veh->in_room, ROOM_GARAGE)))
      return TRUE;

    // It's towed.
    if (!veh->in_veh && !veh->in_room)
      return TRUE;

    // We don't preserve damaged watercraft, mostly because we don't have a water version of the junkyard yet.
    bool will_preserve;
    switch (veh->type) {
      case VEH_CAR:
      case VEH_BIKE:
      case VEH_DRONE:
      case VEH_TRUCK:
        return TRUE;
      default:
        will_preserve = veh_is_aircraft(veh);
        if (!will_preserve)
          strlcpy(reason, "Destroyed watercraft, nowhere to put it", sizeof(reason));
        return will_preserve;
    }
  }

  return TRUE;
}

void set_veh_owner(struct veh_data *veh, idnum_t new_owner) {
  if (veh->owner != 0)
    delete_veh_file(veh, "changing owner");

  delete_veh_from_map(veh);
  veh->owner = new_owner;
  add_veh_to_map(veh);
}