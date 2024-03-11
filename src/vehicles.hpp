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

#endif // __vehicles_hpp__