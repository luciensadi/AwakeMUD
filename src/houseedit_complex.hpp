#ifndef __houseedit_complex_h__
#define __houseedit_complex_h__

#import "structs.hpp"

void houseedit_create_complex(struct char_data *ch);
void houseedit_delete_complex(struct char_data *ch, char *arg);
void houseedit_reload(struct room_data *room);
void houseedit_list_complexes(struct char_data *ch, char *arg);
void houseedit_edit_existing_complex(struct char_data *ch, char *arg);

void houseedit_display_complex_edit_menu(struct descriptor_data *d);

#endif //__houseedit_complex_h__
