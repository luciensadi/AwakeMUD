#ifndef __otaku_h__
#define __otaku_h__

struct otaku_echo {
  const char *name;
  bool incremental;
  bool nerps;
};

int     get_otaku_wil(struct char_data *ch);
int     get_otaku_int(struct char_data *ch);
int     get_otaku_qui(struct char_data *ch);
int     get_otaku_rea(struct char_data *ch);

void    update_otaku_deck(struct char_data *ch, struct obj_data *cyberdeck);

#define SUBMERSION_MAIN	             0
#define SUBMERSION_ECHO	             1
#define SUBMERSION_CONFIRM           2

#define COMPLEX_FORM_TYPES 10

extern int complex_form_programs[COMPLEX_FORM_TYPES];

#define GET_OTAKU_MPCP(ch)           \
  (({ \
    int mpcp = (get_otaku_int(ch) + GET_REAL_WIL(ch) + GET_REAL_CHA(ch) + 2) / 3; \
    if (GET_ECHO(ch, ECHO_IMPROVED_MPCP)) { \
      mpcp = MIN(get_otaku_int(ch) * 2, mpcp + GET_ECHO(ch, ECHO_IMPROVED_MPCP)); \
    } \
    mpcp; \
  }))

#endif
