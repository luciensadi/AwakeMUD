#ifndef __deck_build_hpp__
#define __deck_build_hpp__

extern std::vector<struct obj_data *> global_in_progress_deck_parts;

extern void set_cyberdeck_part_pointer(struct obj_data *part, struct obj_data *deck);
extern void clear_cyberdeck_part_pointer(struct obj_data *part);
extern void clear_all_cyberdeck_part_pointers_pointing_to_deck(struct obj_data *deck);
extern bool part_is_compatible_with_deck(struct obj_data *part, struct obj_data *deck, struct char_data *ch);

#endif