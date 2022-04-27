#ifndef _perception_tests_h_
#define _perception_tests_h_

extern bool can_see_through_invis(struct char_data *ch, struct char_data *vict);

extern void purge_invis_perception_records(struct char_data *ch);

extern void remove_ch_from_pc_perception_records(struct char_data *ch);

extern void process_spotted_invis(struct char_data *ch, struct char_data *vict, bool just_spotted=FALSE);

#endif
