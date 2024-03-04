#ifndef __chipjacks_hpp__
#define __chipjacks_hpp__

void deactivate_skillsofts_in_headware_memory(struct obj_data *mem, struct char_data *victim, bool send_message);
void set_skill_from_chip(struct char_data *ch, struct obj_data *chip, bool send_message, int skillwire_rating);
void initialize_chipjack_and_memory_for_character (struct char_data *ch);
void deactivate_single_skillsoft(struct obj_data *chip, struct char_data *victim, bool send_message);

#endif // __chipjacks_hpp__