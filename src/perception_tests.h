#ifndef _perception_tests_h_
#define _perception_tests_h_

bool can_see_through_invis(struct char_data *ch, struct char_data *vict);

void purge_invis_perception_records(struct char_data *ch);

void remove_ch_from_pc_perception_records(struct char_data *ch);

#endif
