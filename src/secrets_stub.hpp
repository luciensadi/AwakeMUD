#ifndef USE_PRIVATE_CE_WORLD

#include "structs.hpp"

SPECIAL(marksmanship_trainer);
SPECIAL(marksmanship_master);
SPECIAL(marksmanship_fourth);
SPECIAL(marksmanship_first);
SPECIAL(marksmanship_second);
SPECIAL(marksmanship_third);

void display_secret_info_about_object(struct char_data *ch, struct obj_data *obj);
void do_secret_ticks(int pulse);
void rewrite_hidden_site(struct descriptor_data *d);
void perform_secret_room_assignments();
void perform_secret_mob_assignments();
void perform_secret_obj_assignments();

#endif
