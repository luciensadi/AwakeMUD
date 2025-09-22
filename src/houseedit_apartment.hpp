#ifndef __houseedit_apartment_h__
#define __houseedit_apartment_h__

#import "structs.hpp"

void houseedit_list_apartments(struct char_data *ch, const char *func_remainder);
void houseedit_create_apartment(struct char_data *ch, const char *func_remainder);
void houseedit_delete_apartment(struct char_data *ch, const char *func_remainder);
void houseedit_edit_apartment(struct char_data *ch, const char *func_remainder);
void houseedit_set_apartment_lease_length(struct char_data *ch, time_t seconds_left, const char *func_remainder);
void houseedit_show_apartment(struct char_data *ch, char *arg);

#endif //__houseedit_apartment_h__
