#ifndef __vehicles_hpp__
#define __vehicles_hpp__

struct obj_data *get_veh_grabber(struct veh_data *veh);
bool container_is_vehicle_accessible(struct obj_data *cont);
bool veh_get_from_container(struct veh_data *veh, struct obj_data *cont, const char *name, int dotmode, struct char_data *ch);
bool veh_get_from_room(struct veh_data *veh, struct obj_data *obj, struct char_data *ch);
bool veh_get_from_room(struct veh_data *veh, struct veh_data *carried_veh, struct char_data *ch);
bool veh_drop_veh(struct veh_data *veh, struct veh_data *carried_veh, struct char_data *ch);
bool veh_drop_obj(struct veh_data *veh, struct obj_data *obj, struct char_data *ch);
void vehicle_inventory(struct char_data *ch);
bool can_take_veh(struct char_data *ch, struct veh_data *veh);
void generate_veh_idnum(struct veh_data *veh);
bool save_single_vehicle(struct veh_data *veh, bool fromCopyover=FALSE);

// Protos from vehicle_save_load.cpp
void set_veh_owner(struct veh_data *veh, idnum_t new_owner);
void add_veh_to_map(struct veh_data *veh);
void delete_veh_from_map(struct veh_data *veh);
void delete_veh_file(struct veh_data *veh, const char *reason);
void load_vehicles_for_idnum(idnum_t owner_id);

#endif // __vehicles_hpp__