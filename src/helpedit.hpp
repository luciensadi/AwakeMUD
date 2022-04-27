// helpedit.h: prototypes for helpedit.cpp -LS

#ifndef _helpedit_h_
#define _helpedit_h_

#define HELPEDIT_MAIN_MENU 1
#define HELPEDIT_TITLE     2
#define HELPEDIT_BODY      3

ACMD_DECLARE(do_helpedit);
ACMD_DECLARE(do_helpexport);

void helpedit_parse(struct descriptor_data *d, const char *arg);
void helpedit_disp_menu(struct descriptor_data *d);

const char *generate_sql_for_helpfile(const char *name, const char *body);
const char *generate_sql_for_helpfile_by_name(const char *name);

#endif
