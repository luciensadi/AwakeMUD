#ifndef __ritualcast_hpp__
#define __ritualcast_hpp__

void set_up_ritualcast(struct char_data *ch, struct room_data *in_room, struct spell_data *spell, int force);
bool handle_ritualcast_tick(struct char_data *ch, struct obj_data *components);

#endif // __ritualcast_hpp__