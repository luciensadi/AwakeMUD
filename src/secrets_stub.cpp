/* A stub file to allow compilation of the game in the absence of the actual secrets.cpp file.
   The info in that file is kept out of the repo to avoid spoilers for Awake CE. */

#include "structs.hpp"

SPECIAL(marksmanship_trainer) { return false; }
SPECIAL(marksmanship_master) { return false; }
SPECIAL(marksmanship_fourth) { return false; }
SPECIAL(marksmanship_first) { return false; }
SPECIAL(marksmanship_second) { return false; }
SPECIAL(marksmanship_third) { return false; }

void display_secret_info_about_object(struct char_data *ch, struct obj_data *obj) {}
void do_secret_ticks(int pulse) {}
void rewrite_hidden_site(struct descriptor_data *d) {}
void perform_secret_room_assignments() {}
void perform_secret_mob_assignments() {}
void perform_secret_obj_assignments() {}
