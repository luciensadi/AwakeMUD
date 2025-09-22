#ifndef _invis_resistance_tests_h_
#define _invis_resistance_tests_h_

extern bool can_see_through_invis(struct char_data *ch, struct char_data *vict);

extern void purge_invis_invis_resistance_records(struct char_data *ch);

extern void remove_ch_from_pc_invis_resistance_records(struct char_data *ch);

extern bool process_spotted_invis(struct char_data *ch, struct char_data *vict);

#endif
