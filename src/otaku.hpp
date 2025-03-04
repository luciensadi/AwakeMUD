#ifndef __otaku_h__
#define __otaku_h__

struct otaku_echo {
  const char *name;
  bool incremental;
};

int     get_otaku_int(struct char_data *ch);
int     get_otaku_qui(struct char_data *ch);
int     get_otaku_rea(struct char_data *ch);

#define SUBMERSION_MAIN	             0
#define SUBMERSION_ECHO	             1
#define SUBMERSION_CONFIRM           2

#define GET_OTAKU_MPCP(ch)           \
  (({ \
    int mpcp = (get_otaku_int(ch) + GET_REAL_WIL(ch) + GET_REAL_CHA(ch) + 2) / 3; \
    if (GET_ECHO(ch, ECHO_IMPROVED_MPCP)) { \
      mpcp = MIN(get_otaku_int(ch) * 2, mpcp + GET_ECHO(ch, ECHO_IMPROVED_MPCP)); \
    } \
    mpcp; \
  }))

#endif
