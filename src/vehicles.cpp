#include "structs.hpp"
#include "handler.hpp"
#include "vehicles.hpp"
#include "db.hpp"

extern bool can_take_obj_from_room(struct char_data *ch, struct obj_data *obj);
extern void list_obj_to_char(struct obj_data * list, struct char_data * ch, int mode, bool show, bool corpse);
extern void delete_veh_file(struct veh_data *veh, const char *reason);
extern bool check_quest_delivery(struct char_data *ch, struct obj_data *obj);

int calculate_vehicle_entry_load(struct veh_data *veh);
bool _can_veh_lift_obj(struct veh_data *veh, struct obj_data *obj, struct char_data *ch);
bool _can_veh_lift_veh(struct veh_data *veh, struct veh_data *carried_veh, struct char_data *ch);
bool _veh_get_obj(struct veh_data *veh, struct obj_data *obj, struct char_data *ch, struct obj_data *from_obj);
bool _veh_get_veh(struct veh_data *veh, struct veh_data *target_veh, struct char_data *ch);
bool _veh_can_get_obj(struct veh_data *veh, struct obj_data *obj, struct char_data *ch);
bool _veh_can_get_veh(struct veh_data *veh, struct veh_data *target_veh, struct char_data *ch);

struct obj_data *get_veh_grabber(struct veh_data *veh) {
  if (!veh) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Invalid arguments to veh_has_grabber(%s)", GET_VEH_NAME(veh));
    return NULL;
  }

  return veh->mod[MOD_GRABBER];
}

// Functions for getting from containers.

bool container_is_vehicle_accessible(struct obj_data *cont) {
  if (!cont) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Invalid arguments to container_is_vehicle_accessible(%s)", GET_OBJ_NAME(cont));
    return FALSE;
  }

  // Vehicles can only reach inside containers (including corpses).
  return GET_OBJ_TYPE(cont) == ITEM_CONTAINER;
}

bool veh_get_from_container(struct veh_data *veh, struct obj_data *cont, const char *obj_name, int obj_dotmode, struct char_data *ch) {
  if (!veh || !cont || !obj_name || !*obj_name || !ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid arguments to veh_get_from_container(%s, %s, %s, %d, ch)",
                    GET_VEH_NAME(veh), GET_OBJ_NAME(cont), obj_name, obj_dotmode);
    return FALSE;
  }

  // Container failure cases.
  FALSE_CASE_PRINTF(GET_OBJ_TYPE(cont) != ITEM_CONTAINER, "Your manipulator isn't dextrous enough to reach inside %s.", decapitalize_a_an(cont));
  FALSE_CASE_PRINTF(IS_OBJ_STAT(cont, ITEM_EXTRA_CORPSE) && GET_CORPSE_IS_PC(cont) && GET_CORPSE_IDNUM(cont) != GET_IDNUM(ch) && !IS_SENATOR(ch),
                    "You can't loot %s.", decapitalize_a_an(cont));
  FALSE_CASE_PRINTF(IS_SET(GET_CONTAINER_FLAGS(cont), CONT_CLOSED), "%s is closed.", CAP(GET_OBJ_NAME(cont)));
  FALSE_CASE_PRINTF(!cont->contains, "%s is empty.", CAP(GET_OBJ_NAME(cont)));

  if (obj_dotmode == FIND_INDIV) {
    // Find the object.
    struct obj_data *obj = get_obj_in_list_vis(ch, obj_name, cont->contains);
    FALSE_CASE_PRINTF(!obj, "You don't see anything named '%s' in %s.", obj_name, decapitalize_a_an(cont));

    // Obj / vehicle combined failure cases. Error messages are sent in-function.
    if (!_veh_can_get_obj(veh, obj, ch)) {
      return FALSE;
    }

    // Success.  
    return _veh_get_obj(veh, obj, ch, cont);
  } else {
    bool found_something = FALSE;
    for (struct obj_data *obj = cont->contains, *next_obj; obj; obj = next_obj) {
      next_obj = obj->next_content;
      if (obj_dotmode == FIND_ALL || keyword_appears_in_obj(obj_name, obj)) {
        found_something = TRUE;
        // Error messages are sent in _veh_can_get_obj.
        if (!_veh_can_get_obj(veh, obj, ch))
          continue;
          
        _veh_get_obj(veh, obj, ch, cont);
      }
    }
    FALSE_CASE_PRINTF(!found_something, "You don't see anything named '%s' in %s.", obj_name, decapitalize_a_an(cont));
    return TRUE;
  }
}

// Functions for getting from rooms.

bool veh_get_from_room(struct veh_data *veh, struct obj_data *obj, struct char_data *ch) {
  if (!veh || !obj || !ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid arguments to veh_get_from_room(%s, o-%s, ch)",
                    GET_VEH_NAME(veh), GET_OBJ_NAME(obj));
    return FALSE;
  }

  // Obj / vehicle combined failure cases. Error messages are sent in-function.
  if (!_veh_can_get_obj(veh, obj, ch)) {
    return FALSE;
  }

  // Success.
  return _veh_get_obj(veh, obj, ch, NULL);
}

bool veh_get_from_room(struct veh_data *veh, struct veh_data *target_veh, struct char_data *ch) {
  if (!veh || !target_veh || !ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid arguments to veh_get_from_room(%s, v-%s, ch)",
                    GET_VEH_NAME_NOFORMAT(veh), GET_VEH_NAME(target_veh));
    return FALSE;
  }

  // target_veh / vehicle combined failure cases. Error messages are sent in-function.
  if (!_veh_can_get_veh(veh, target_veh, ch)) {
    return FALSE;
  }

  // Success.
  return _veh_get_veh(veh, target_veh, ch);
}

// Functions for displaying info about vehicle and contents.

void vehicle_inventory(struct char_data *ch) {
  struct veh_data *veh;
  RIG_VEH(ch, veh);

  FAILURE_CASE(!veh, "You're not rigging a vehicle.");

  send_to_char(ch, "%s is carrying:\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
  list_obj_to_char(veh->contents, ch, SHOW_MODE_IN_INVENTORY, TRUE, FALSE);

  if (veh->carriedvehs) {
    send_to_char(ch, "\r\n%s is also carrying:\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
    for (struct veh_data *carried = veh->carriedvehs; carried; carried = carried->next_veh) {
    send_to_char(ch, "^y%s%s^n\r\n", GET_VEH_NAME(carried), carried->owner == GET_IDNUM(ch) ? " ^Y(yours)" : "");
  }
  }
}

// Functions for dropping contents.

bool veh_drop_veh(struct veh_data *veh, struct veh_data *carried_veh, struct char_data *ch) {
  if (!veh || !carried_veh || !ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid arguments to veh_drop_veh(%s, %s, ch)",
                    GET_VEH_NAME_NOFORMAT(veh), GET_VEH_NAME_NOFORMAT(carried_veh));
    return FALSE;
  }

  // Things like locked state, etc.
  if (!can_take_veh(ch, carried_veh)) {
    return FALSE;
  }

  // Weight restrictions etc.
  if (!_can_veh_lift_veh(veh, carried_veh, ch)) {
    return FALSE;
  }

  char msg_buf[1000];

  veh_from_room(carried_veh);

  if (veh->in_room)
    veh_to_room(carried_veh, veh->in_room);
  else
    veh_to_veh(carried_veh, veh->in_veh);

  send_to_char(ch, "You drop %s.\r\n", GET_VEH_NAME(carried_veh));
  
  // Message others.
  snprintf(msg_buf, sizeof(msg_buf), "%s's manipulator arm deposits %s here.\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)), GET_VEH_NAME(carried_veh));
  if (veh->in_room) {
    send_to_room(msg_buf, veh->in_room, veh);
  } else {
    send_to_veh(msg_buf, veh->in_veh, ch, TRUE);
  }

  // Message passengers.
  if (veh->people) {
    snprintf(msg_buf, sizeof(msg_buf), "The manipulator arm reaches in and lifts out %s.\r\n", decapitalize_a_an(carried_veh));
    send_to_veh(msg_buf, veh, ch, FALSE);
  }

  return TRUE;
}

bool veh_drop_obj(struct veh_data *veh, struct obj_data *obj, struct char_data *ch) {
  if (!veh || !obj || !ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid arguments to veh_drop_obj(%s, %s, ch)",
                    GET_VEH_NAME(veh), GET_OBJ_NAME(obj));
    return FALSE;
  }
  
  // Error messages sent in function.
  if (!can_take_obj_from_room(ch, obj))
    return FALSE;

  // Obj / vehicle combined failure cases. Error messages are sent in-function.
  if (!_can_veh_lift_obj(veh, obj, ch)) {
    return FALSE;
  }

  char msg_buf[1000];

  obj_from_room(obj);

  if (veh->in_room)
    obj_to_room(obj, veh->in_room);
  else
    obj_to_veh(obj, veh->in_veh);

  send_to_char(ch, "You drop %s.\r\n", decapitalize_a_an(obj));
  check_quest_delivery(ch, obj);
  
  // Message others.
  snprintf(msg_buf, sizeof(msg_buf), "%s's manipulator arm deposits %s here.\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)), decapitalize_a_an(obj));
  if (veh->in_room) {
    send_to_room(msg_buf, veh->in_room, veh);
  } else {
    send_to_veh(msg_buf, veh->in_veh, ch, TRUE);
  }

  // Message passengers.
  if (veh->people) {
    snprintf(msg_buf, sizeof(msg_buf), "The manipulator arm reaches in and lifts out %s.\r\n", decapitalize_a_an(obj));
    send_to_veh(msg_buf, veh, ch, FALSE);
  }

  return TRUE;
}

/////////// Helper functions and utils.

bool can_take_veh(struct char_data *ch, struct veh_data *veh) {
  // No taking vehicles that are moving.
  FALSE_CASE_PRINTF(veh->cspeed != SPEED_OFF, "%s needs to be completely powered off before you can lift it.", capitalize(GET_VEH_NAME_NOFORMAT(veh)));

  // No taking vehicles that are actively rigged.
  FALSE_CASE_PRINTF(veh->rigger, "%s has someone in control of it, you'd better not.", capitalize(GET_VEH_NAME_NOFORMAT(veh)));

  // No taking vehicles with people inside.
  FALSE_CASE_PRINTF(veh->people, "%s has people inside it, you'd better not.", capitalize(GET_VEH_NAME_NOFORMAT(veh)));

  // No taking vehicles with other vehicles inside.
  FALSE_CASE_PRINTF(veh->carriedvehs, "%s has another vehicle inside it, there's no way you can carry that much!", capitalize(GET_VEH_NAME_NOFORMAT(veh)));

  // No taking vehicles that are actively towing.
  FALSE_CASE_PRINTF(veh->towing, "Not while %s is towing something.", GET_VEH_NAME(veh));

  // Not during vehicle combat.
  FALSE_CASE_PRINTF(veh->fighting, "%s is a little busy right now.", capitalize(GET_VEH_NAME_NOFORMAT(veh)));

  // Nor during flight.
  FALSE_CASE_PRINTF(veh->flight_duration, "...How? %s is in midair.", GET_VEH_NAME(veh));

  // No taking NPC vehicles or locked, non-destroyed vehicles that belong to someone else.
  FALSE_CASE_PRINTF(!veh->owner || (veh->owner != GET_IDNUM(ch) && (veh->locked && veh->damage < VEH_DAM_THRESHOLD_DESTROYED)),
                    "%s's anti-theft measures beep loudly.", capitalize(GET_VEH_NAME_NOFORMAT(veh)));

  return TRUE;
}

bool _can_veh_lift_obj(struct veh_data *veh, struct obj_data *obj, struct char_data *ch) {
  if (!veh || !obj || !ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid arguments to can_veh_lift_obj(%s, %s, ch)",
                    GET_VEH_NAME(veh), GET_OBJ_NAME(obj));
    return FALSE;
  }

  // Failure cases for obj (!TAKE, etc)
  FALSE_CASE_PRINTF(!CAN_WEAR(obj, ITEM_WEAR_TAKE), "You can't take %s.", decapitalize_a_an(obj));

  // No picking up nuyen.
  FALSE_CASE(GET_OBJ_TYPE(obj) == ITEM_MONEY && !GET_ITEM_MONEY_IS_CREDSTICK(obj),
             "Your mechanical clampers fumble the loose change and bills, spilling them everywhere.");

  // Nor keys.
  FALSE_CASE_PRINTF(GET_OBJ_TYPE(obj) == ITEM_KEY, "Your mechanical clampers can't get a good grip on %s.", decapitalize_a_an(obj));

  // Too heavy for your vehicle's body rating
  FALSE_CASE_PRINTF(GET_OBJ_WEIGHT(obj) > veh->body * veh->body * 20, "%s is too heavy for your vehicle's chassis to lift.", CAP(GET_OBJ_NAME(obj)));

  // Too big for the grabber
  struct obj_data *grabber = get_veh_grabber(veh);
  FALSE_CASE_PRINTF(GET_VEHICLE_MOD_GRABBER_MAX_LOAD(grabber) < GET_OBJ_WEIGHT(obj), "%s is too heavy for %s.",
                    CAP(GET_OBJ_NAME(obj)), decapitalize_a_an(grabber));

  return TRUE;
}

bool _can_veh_lift_veh(struct veh_data *veh, struct veh_data *carried_veh, struct char_data *ch) {
  if (!veh || !carried_veh || !ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid arguments to _can_veh_lift_veh(%s, %s, ch)",
                    GET_VEH_NAME_NOFORMAT(veh), GET_VEH_NAME_NOFORMAT(carried_veh));
    return FALSE;
  }

  int veh_weight = calculate_vehicle_weight(carried_veh);

  // Drones can't lift other vehicles.
  FALSE_CASE(veh->type == VEH_DRONE, "Drones don't have the right build to lift other vehicles.");

  // Too heavy for your vehicle's body rating
  FALSE_CASE_PRINTF(veh_weight > veh->body * veh->body * 20, "%s is too heavy for your vehicle's chassis to lift.", CAP(GET_VEH_NAME_NOFORMAT(carried_veh)));

  // Too big for the grabber
  struct obj_data *grabber = get_veh_grabber(veh);
  FALSE_CASE_PRINTF(GET_VEHICLE_MOD_GRABBER_MAX_LOAD(grabber) < veh_weight, "%s is too heavy for %s.", CAP(GET_VEH_NAME_NOFORMAT(carried_veh)), decapitalize_a_an(grabber));

  return TRUE;
}

// We assume all precondition checking has been done here.
bool _veh_get_obj(struct veh_data *veh, struct obj_data *obj, struct char_data *ch, struct obj_data *from_obj) {
  char msg_buf[1000];

  if (!veh || !obj || !ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid arguments to _veh_get_obj(%s, %s, ch)",
                    GET_VEH_NAME(veh), GET_OBJ_NAME(obj));
    return FALSE;
  }

  if (from_obj)
    obj_from_obj(obj);
  else
    obj_from_room(obj);

  obj_to_veh(obj, veh);
  send_to_char(ch, "You load %s into your vehicle's storage.\r\n", decapitalize_a_an(obj));
  

  if (from_obj) {
    snprintf(msg_buf, sizeof(msg_buf), "%s's manipulator arm gets %s from %s.\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)), decapitalize_a_an(obj), GET_OBJ_NAME(from_obj));
  } else {
    snprintf(msg_buf, sizeof(msg_buf), "%s loads %s into its internal storage.\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)), decapitalize_a_an(obj));
  }
  
  // Message others.
  if (veh->in_room) {
    send_to_room(msg_buf, veh->in_room, veh);
  } else {
    send_to_veh(msg_buf, veh->in_veh, ch, TRUE);
  }

  // Message passengers.
  if (veh->people) {
    snprintf(msg_buf, sizeof(msg_buf), "The manipulator arm reaches in and deposits %s.\r\n", decapitalize_a_an(obj));
    send_to_veh(msg_buf, veh, ch, FALSE);
  }

  return TRUE;
}

bool _veh_can_get_obj(struct veh_data *veh, struct obj_data *obj, struct char_data *ch) {
  if (!veh || !obj || !ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid arguments to can_veh_get_obj(%s, %s, ch)",
                    GET_VEH_NAME(veh), GET_OBJ_NAME(obj));
    return FALSE;
  }

  // Error messages sent in function.
  if (!can_take_obj_from_room(ch, obj))
    return FALSE;

  // Error messages sent in function.
  if (!_can_veh_lift_obj(veh, obj, ch))
    return FALSE;

  // Failure cases for vehicle (too full, etc)
  FALSE_CASE_PRINTF(veh->usedload + GET_OBJ_WEIGHT(obj) > veh->load, "Your vehicle is too full to hold %s.", decapitalize_a_an(obj));

  return TRUE;
}

// We assume all precondition checking has been done here.
bool _veh_get_veh(struct veh_data *veh, struct veh_data *target_veh, struct char_data *ch) {
  char msg_buf[1000];

  if (!veh || !target_veh || !ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid arguments to _veh_get_veh(%s, %s, ch)",
                    GET_VEH_NAME(veh), GET_VEH_NAME_NOFORMAT(target_veh));
    return FALSE;
  }

  veh_from_room(target_veh);
  veh_to_veh(target_veh, veh);
  send_to_char(ch, "You load %s into your vehicle's storage.\r\n", decapitalize_a_an(target_veh));
  
  snprintf(msg_buf, sizeof(msg_buf), "%s loads %s into its internal storage.\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)), decapitalize_a_an(target_veh));
  
  // Message others.
  if (veh->in_room) {
    send_to_room(msg_buf, veh->in_room, veh);
  } else {
    send_to_veh(msg_buf, veh->in_veh, ch, TRUE);
  }

  // Message passengers.
  if (veh->people) {
    snprintf(msg_buf, sizeof(msg_buf), "The manipulator arm reaches in and deposits %s.\r\n", decapitalize_a_an(target_veh));
    send_to_veh(msg_buf, veh, ch, FALSE);
  }

  return TRUE;
}

bool _veh_can_get_veh(struct veh_data *veh, struct veh_data *target_veh, struct char_data *ch) {
  if (!veh || !target_veh || !ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid arguments to can_veh_get_obj(%s, %s, ch)",
                    GET_VEH_NAME(veh), GET_VEH_NAME_NOFORMAT(target_veh));
    return FALSE;
  }

  // Error messages sent in function.
  if (!can_take_veh(ch, target_veh))
    return FALSE;

  // Error messages sent in function.
  if (!_can_veh_lift_veh(veh, target_veh, ch))
    return FALSE;

  // Failure cases for vehicle (too full, etc)
  FALSE_CASE_PRINTF(veh->usedload + calculate_vehicle_weight(target_veh) > veh->load, "Your vehicle is too full to hold %s.", decapitalize_a_an(target_veh));

  return TRUE;
}

void generate_veh_idnum(struct veh_data *veh) {
  veh->idnum = number(1, 999999);
}

int get_obj_vehicle_load_usage(struct obj_data *obj, bool is_installed_mod) {
  if (is_installed_mod) {
    // Mounts: Their own weight (calculated by type) plus that of their contents.
    if (GET_VEHICLE_MOD_TYPE(obj) == TYPE_MOUNT) {
      int result = 0;

      for (struct obj_data *contained = obj->contains; contained; contained = contained->next_content) {
        result += get_obj_vehicle_load_usage(contained, FALSE);
      }

      switch (GET_VEHICLE_MOD_MOUNT_TYPE(obj)) {
        case MOUNT_TURRET:
          result += 100;
          break;
        case MOUNT_MINITURRET:
          result += 25;
          break;
        default:
          result += 10;
          break;
      }

      return result;
    }

    // Other mods: Their mod load space requirement.
    return GET_VEHICLE_MOD_LOAD_SPACE_REQUIRED(obj);
  }

  // Otherwise it's just an item with weight etc.
  return GET_OBJ_WEIGHT(obj);
}

void recalculate_vehicle_usedload(struct veh_data *veh) {
  int old_load = veh->usedload;

  veh->usedload = 0;

  // Recalculate mod weights.
  for (int mod_idx = 0; mod_idx < NUM_MODS; mod_idx++) {
    if (GET_MOD(veh, mod_idx))
      veh->usedload += get_obj_vehicle_load_usage(GET_MOD(veh, mod_idx), TRUE);
  }

  // Recalculate mount weights.
  for (struct obj_data *mount = veh->mount; mount; mount = mount->next_content) {
    veh->usedload += get_obj_vehicle_load_usage(mount, TRUE);
  }

  // Recalculate contained weights.
  for (struct obj_data *contained = veh->contents; contained; contained = contained->next_content) {
    veh->usedload += get_obj_vehicle_load_usage(contained, FALSE);
  }

  // Recalculate carried vehicle weights.
  for (struct veh_data *carried = veh->carriedvehs; carried; carried = carried->next_veh) {
    veh->usedload += calculate_vehicle_entry_load(carried);
  }
  
  if (veh->usedload != old_load) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Vehicle %s (%ld-%ld) @ %ld had unexpected usedload (%d != %.2f)",
                    GET_VEH_NAME(veh),
                    GET_VEH_VNUM(veh),
                    GET_VEH_IDNUM(veh),
                    GET_ROOM_NAME(get_veh_in_room(veh)),
                    old_load,
                    veh->usedload);
  }
}

int calculate_vehicle_entry_load(struct veh_data *veh) {
  int mult;
  switch (veh->type) {
    case VEH_DRONE:
      mult = 100;
      break;
    case VEH_TRUCK:
      mult = 1500;
      break;
    default:
      mult = 500;
      break;
  }

  return veh->body * mult;
}

int get_mount_signature_penalty(struct obj_data *mount) {
  switch (GET_VEHICLE_MOD_MOUNT_TYPE(mount)) {
    case MOUNT_FIRMPOINT_EXTERNAL:
    case MOUNT_HARDPOINT_EXTERNAL:
    case MOUNT_TURRET:
    case MOUNT_MINITURRET:
      return 1;
  }

  return 0;
}