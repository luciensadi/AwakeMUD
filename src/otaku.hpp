#ifndef __otaku_h__
#define __otaku_h__

struct otaku_echo {
  const char *name;
  bool incremental;
  bool nerps;
};

int     get_otaku_mpcp(struct char_data *ch);

void    update_otaku_deck(struct char_data *ch, struct obj_data *cyberdeck);

#define SUBMERSION_MAIN	             0
#define SUBMERSION_ECHO	             1
#define SUBMERSION_CONFIRM           2

#define COMPLEX_FORM_TYPES 11

extern int complex_form_programs[COMPLEX_FORM_TYPES];

#endif
