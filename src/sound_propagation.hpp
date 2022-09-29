#ifndef _sound_propagation_hpp_
#define _sound_propagation_hpp_

extern void propagate_sound_from_room(struct room_data *original_room, std::vector<const char *> sound_vect, bool is_combat_noise, struct char_data *originator);

#endif // _sound_propagation_hpp_
