#ifndef __zoomies_hpp__
#define __zoomies_hpp__

#define RM_AIRBORNE 10096
#define RM_FLIGHT_CRASH_FALLBACK 2048

bool check_for_valid_launch_location(struct char_data *ch, struct veh_data *veh, bool message);
bool veh_can_launch_from_or_land_at(struct veh_data *veh, struct room_data *room);
float get_flight_distance_to_room(struct veh_data *veh, struct room_data *room);
int calculate_flight_cost(struct veh_data *veh, float distance);
int flight_test(struct char_data *ch, struct veh_data *veh);
int calculate_flight_time(struct veh_data *veh, float distance);
void crash_flying_vehicle(struct veh_data *veh, bool is_controlled_landing=FALSE);
void send_flight_estimate(struct char_data *ch, struct veh_data *veh);
bool veh_is_currently_flying(struct veh_data *veh);
void clear_veh_flight_info(struct veh_data *veh);

#endif