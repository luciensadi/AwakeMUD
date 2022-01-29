#ifndef _drive_h_
#define _drive_h_

void do_raw_ram(struct char_data *ch, struct veh_data *veh, struct veh_data *tveh, struct char_data *vict);
void do_raw_target(struct char_data *ch, struct veh_data *veh, struct veh_data *tveh, struct char_data *vict, bool modeall, struct obj_data *obj);
int calculate_vehicle_weight(struct veh_data *veh);

#endif
