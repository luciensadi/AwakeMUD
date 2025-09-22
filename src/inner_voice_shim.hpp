#ifndef INNER_VOICE_SHIM_HPP
#define INNER_VOICE_SHIM_HPP
// No-op shim so files can call InnerVoice::* even if the feature is disabled.
struct char_data;
struct veh_data;
struct room_data;
#ifndef INNER_VOICE_REAL_IMPL
namespace InnerVoice {
  static inline void notify_vehicle_travel_tick(char_data*, veh_data*) {}
  static inline void notify_room_move(char_data*, room_data*) {}
  static inline void notify_door_interaction(char_data*, int, int) {}
}
#endif // !INNER_VOICE_REAL_IMPL
#endif
