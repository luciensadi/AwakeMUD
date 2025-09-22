#ifndef __COMBAT_QOL_HPP__
#define __COMBAT_QOL_HPP__

#include "structs.hpp"

bool qol_try_auto_reload(struct char_data *ch, struct obj_data *weapon);
bool qol_try_auto_swap_to_sidearm(struct char_data *ch);
void qol_tick_repeat_shoot(struct char_data *ch);

// Auto-heal
void qol_tick_auto_heal(struct char_data *ch);

// Auto-flee
bool qol_tick_auto_flee(struct char_data *ch);


// Auto-advance
void qol_tick_auto_advance(struct char_data *ch);

// Auto-heal threshold (backend)
void qol_set_auto_heal_threshold_backend(struct char_data *ch, int pct);
int  qol_get_auto_heal_threshold(struct char_data *ch);
#endif

