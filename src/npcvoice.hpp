
#pragma once

#include "structs.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "handler.hpp"
#include <string>
#include <unordered_map>
#include <ctime>

namespace NPCVoice {

  enum Personality {
    PERS_GANG = 0,
    PERS_NEUTRAL = 1,
    PERS_POLITE = 2
  };

  void maybe_greet(struct char_data* ch, struct char_data* mob);
  void maybe_farewell(struct char_data* ch, struct char_data* mob);
  void maybe_listen(struct char_data* ch, struct char_data* mob, const char* said);
  void combat_start(struct char_data* ch, struct char_data* mob);
  void combat_hit(struct char_data* ch, struct char_data* mob);
  void combat_death(struct char_data* ch, struct char_data* mob);
  void shop_event(struct char_data* ch, struct char_data* mob, int type);
  void quest_event(struct char_data* ch, struct char_data* mob, int type);
  void receive_item(struct char_data* ch, struct char_data* mob, struct obj_data* obj);

  void set_personality(struct char_data* mob, Personality p);
  Personality get_personality(struct char_data* mob);

} // namespace NPCVoice
