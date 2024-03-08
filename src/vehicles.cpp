#include "structs.hpp"
#include "handler.hpp"
#include "vehicles.hpp"

bool _can_veh_lift_obj(struct veh_data *veh, struct obj_data *obj, struct char_data *ch);
bool _veh_get_obj(struct veh_data *veh, struct obj_data *obj, struct char_data *ch, bool from_obj);

struct obj_data *get_veh_grabber(struct veh_data *veh) {
  if (!veh) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Invalid arguments to veh_has_grabber(%s)", GET_VEH_NAME(veh));
    return NULL;
  }

  return veh->mod[MOD_GRABBER];
}

bool container_is_vehicle_accessible(struct obj_data *cont) {
  if (!cont) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Invalid arguments to container_is_vehicle_accessible(%s)", GET_OBJ_NAME(cont));
    return FALSE;
  }

  // Vehicles can only reach inside containers (including corpses).
  return GET_OBJ_TYPE(cont) == ITEM_CONTAINER;
}

bool veh_can_get_obj(struct veh_data *veh, struct obj_data *obj, struct char_data *ch) {
  if (!veh || !obj || !ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid arguments to can_veh_get_obj(%s, %s, ch)",
                    GET_VEH_NAME(veh), GET_OBJ_NAME(obj));
    return FALSE;
  }

  // Error messages sent in function.
  if (!_can_veh_lift_obj(veh, obj, ch))
    return FALSE;

  // Failure cases for vehicle (too full, etc)
  FALSE_CASE_PRINTF(veh->usedload + GET_OBJ_WEIGHT(obj) > veh->load, "Your vehicle is too full to hold %s.", decapitalize_a_an(obj));

  return TRUE;
}

bool veh_get_from_container(struct veh_data *veh, struct obj_data *cont, const char *obj_name, int dotmode, struct char_data *ch) {
  if (!veh || !cont || !obj_name || !*obj_name || !ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid arguments to veh_get_from_container(%s, %s, %s, %d, ch)",
                    GET_VEH_NAME(veh), GET_OBJ_NAME(cont), obj_name, dotmode);
    return FALSE;
  }

  // Container failure cases.
  FALSE_CASE_PRINTF(GET_OBJ_TYPE(cont) != ITEM_CONTAINER, "Your manipulator isn't dextrous enough to reach inside %s.", decapitalize_a_an(cont));
  FALSE_CASE_PRINTF(IS_OBJ_STAT(cont, ITEM_EXTRA_CORPSE) && GET_CORPSE_IS_PC(cont) && GET_CORPSE_IDNUM(cont) != GET_IDNUM(ch) && !IS_SENATOR(ch),
                    "You can't loot %s.", decapitalize_a_an(cont));
  FALSE_CASE_PRINTF(IS_SET(GET_CONTAINER_FLAGS(cont), CONT_CLOSED), "%s is closed.", CAP(GET_OBJ_NAME(cont)));
  FALSE_CASE_PRINTF(!cont->contains, "%s is empty.", CAP(GET_OBJ_NAME(cont)));

  // Obj failure cases (not found, etc)
  struct obj_data *obj = get_obj_in_list_vis(ch, obj_name, cont->contains);
  FALSE_CASE_PRINTF(!obj, "You don't see anything named '%s' in %s.", obj_name, decapitalize_a_an(cont));

  // Obj / vehicle combined failure cases. Error messages are sent in-function.
  if (!veh_can_get_obj(veh, obj, ch)) {
    return FALSE;
  }

  // Success.  
  return _veh_get_obj(veh, obj, ch, TRUE);
}

bool veh_get_from_room(struct veh_data *veh, struct obj_data *obj, struct char_data *ch) {
  if (!veh || !obj || !ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid arguments to veh_get_from_room(%s, %s, ch)",
                    GET_VEH_NAME(veh), GET_OBJ_NAME(obj));
    return FALSE;
  }

  // Obj / vehicle combined failure cases. Error messages are sent in-function.
  if (!veh_can_get_obj(veh, obj, ch)) {
    return FALSE;
  }

  // Success.
  return _veh_get_obj(veh, obj, ch, FALSE);
}

/////////// Helper functions and utils.

bool _can_veh_lift_obj(struct veh_data *veh, struct obj_data *obj, struct char_data *ch) {
  if (!veh || !obj || !ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid arguments to can_veh_lift_obj(%s, %s, ch)",
                    GET_VEH_NAME(veh), GET_OBJ_NAME(obj));
    return FALSE;
  }

  // Failure cases for obj (!TAKE, etc)
  FALSE_CASE_PRINTF(!CAN_WEAR(obj, ITEM_WEAR_TAKE), "You can't take %s.", decapitalize_a_an(obj));

  // Too big for the grabber
  struct obj_data *grabber = get_veh_grabber(veh);
  FAILURE_CASE_PRINTF(GET_VEHICLE_MOD_GRABBER_MAX_LOAD(grabber) < GET_OBJ_WEIGHT(obj), "%s is too heavy for %s.",
                      CAP(GET_OBJ_NAME(obj)), decapitalize_a_an(grabber));

  return TRUE;
}

// We assume all precondition checking has been done here.
bool _veh_get_obj(struct veh_data *veh, struct obj_data *obj, struct char_data *ch, bool from_obj) {
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

  if (veh->people) {
    send_to_veh("A manipulator arm reaches in and deposits %s.", veh, ch, FALSE, decapitalize_a_an(obj));
  }

  return TRUE;
}