#ifndef __newmatrix_h__
#define __newmatrix_h__

#include "bitfield.h"

#define ACCESS	0
#define CONTROL	1
#define INDEX	2
#define FILES	3
#define SLAVE	4

#define MAX_CUSTOM_MPCP_RATING 15

struct host_data {
  vnum_t vnum;
  rnum_t rnum;
  vnum_t parent;

  char *name;
  char *keywords;
  char *desc;
  char *shutdown_start;
  char *shutdown_stop;

  sh_int colour;
  sh_int security;
  sh_int intrusion;
  long stats[5][6];

  int type;
  int reset;
  int found;
  int alert;
  int pass;
  int shutdown;
  int shutdown_success;
  int shutdown_mpcp;
  bool payreset;

  struct trigger_step *trigger;
  struct matrix_icon *icons;
  struct matrix_icon *fighting;
  struct exit_data *exit;
  struct obj_data *file;
  host_data():
    name(NULL), keywords(NULL), desc(NULL), shutdown_start(NULL), shutdown_stop(NULL),
    type(0), reset(0), found(0), alert(0), pass(0), shutdown(0), shutdown_success(0), shutdown_mpcp(0), 
    payreset(TRUE), trigger(NULL), icons(NULL), fighting(NULL), exit(NULL), file(NULL)
  {}
};

struct exit_data {
  vnum_t host;
  char *addresses;
  char *roomstring;
  bool hidden;
  struct exit_data *next;
  exit_data():
    host(0), addresses(NULL), roomstring(NULL), hidden(FALSE), next(NULL)
  {}
};

struct trigger_step {
  int step;
  int alert;
  vnum_t ic;
  struct trigger_step *next;
  trigger_step():
    step(0), alert(0), ic(0), next(NULL)
  {}
};

struct seen_data {
  int idnum;
  struct seen_data *next;
  seen_data():
    next(NULL)
  {}
};

struct ic_info {
  int rating;
  int type;
  int subtype;
  long target;
  bool supressed;
  Bitfield options;
  long trap; 
  int expert;
  int cascade;
  int targ_evasion;
  ic_info():
    rating(0), type(0), subtype(0), target(0), supressed(FALSE), trap(0), expert(0), cascade(0), targ_evasion(0)
  {}
};

struct deck_info {
  int mpcp;
  int bod;
  int sensor;
  int evasion;
  int masking;
  int hardening;

  int active;
  int storage;
  int response;
  int io;
  int res_det;
  int res_test;
  bool asist[2];
  bool ras;
  bool reality;
  bool iccm;

  int tally;
  int last_trigger;
  int scout;
  bool located;
  int redirect;
  long *redirectedon;
  long mxp;

  struct char_data *hitcher;
  struct obj_data *software;
  struct obj_data *deck;
  struct seen_data *seen;
  struct char_data *ch;
  struct phone_data *phone;
  deck_info():
    mpcp(0), bod(0), sensor(0), evasion(0), masking(0), hardening(0), active(0), 
    storage(0), response(0), res_det(0), res_test(0),
    tally(0), last_trigger(0), scout(0), located(FALSE),
    redirect(0), redirectedon(NULL), hitcher(NULL), software(NULL), deck(NULL),
    seen(NULL), ch(NULL), phone(NULL)
   {} 
};

struct matrix_icon {
  char *name;
  char *long_desc;
  char *look_desc;

  int idnum;
  vnum_t number;
  int condition;
  int initiative;
  int parry;
  int evasion;
  int position;
  struct ic_info ic;
  struct deck_info *decker;
  rnum_t in_host;
  struct matrix_icon *fighting;
  struct matrix_icon *next;
  struct matrix_icon *next_in_host;
  struct matrix_icon *next_fighting;

  matrix_icon():
    name(NULL), long_desc(NULL), look_desc(NULL), idnum(0), number(0), condition(10),
    initiative(0), parry(0), evasion(0), position(0),
    decker(NULL), fighting(NULL), next(NULL), next_in_host(NULL), next_fighting(NULL)
  {}
};

extern bool has_spotted(struct matrix_icon *icons, struct matrix_icon *targ);

void display_cyberdeck_issues(struct char_data *ch, struct obj_data *cyberdeck);

#endif
