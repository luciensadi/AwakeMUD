#ifndef __CREATIVE_WORKS_H
#define __CREATIVE_WORKS_H

#define ART_EDIT_MAIN     0
#define ART_EDIT_NAME     1
#define ART_EDIT_DESC     2
#define ART_EDIT_ROOMDESC 3

void art_edit_parse(struct descriptor_data *d, const char *arg);
void create_art_main_menu(struct descriptor_data *d);

#endif
