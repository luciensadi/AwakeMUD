#ifndef __newmatrix_h__
#define __newmatrix_h__

#include "bitfield.h"

#define ACCESS	0
#define CONTROL	1
#define INDEX	2
#define FILES	3
#define SLAVE	4

#define MAX_CUSTOM_MPCP_RATING 15

#define FILE_PROTECTION_SCRAMBLED 1

#define IC_CRIPPLER         0  // Destroys programs.
#define IC_KILLER           1  // NERP
#define IC_PROBE            2  // NERP
#define IC_SCRAMBLE         3  // NERP
#define IC_TARBABY          4  // Removes programs from active memory.
#define IC_SCOUT            5  // Increases tally.
#define IC_TRACE            6  // Locates your meatspace body. Increases tally.
#define IC_BLASTER          7  // Damages your deck on attack, also harms you
#define IC_RIPPER           8  // Destroys persona chips. Damages deck if it reduces the value to 0.
#define IC_SPARKY           9  // Damages your deck on attack
#define IC_TARPIT           10 // Deletes programs from your active and possibly storage memory.
#define IC_LETHAL_BLACK     11 // Deals lethal damage to decker. Damages deck.
#define IC_NON_LETHAL_BLACK 12 // Deals nonlethal damage to decker. Damages deck?
#define NUM_OF_IC_TYPES     13

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
    storage(0), response(0), io(0), res_det(0), res_test(0), ras(0), reality(0), iccm(0),
    tally(0), last_trigger(0), scout(0), located(FALSE),
    redirect(0), redirectedon(NULL), mxp(0), hitcher(NULL), software(NULL), deck(NULL),
    seen(NULL), ch(NULL), phone(NULL)
   {
     asist[0] = 0;
     asist[1] = 0;
   } 
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

/*
struct per_character_located_data {
  vnum_t jackpoint_room_vnum;
  per_character_located_data *prev, *next;
};
*/

struct per_character_host_data {
  vnum_t host_num;
  int tally;
  int last_trigger;
  int alert;
  
  // For the linked list of many such hosts.
  struct per_character_host_data *prev, *next;
  
  per_character_host_data():
    host_num(-1), tally(0), last_trigger(0), prev(NULL), next(NULL)
  {}
};

extern bool has_spotted(struct matrix_icon *icons, struct matrix_icon *targ);

bool display_cyberdeck_issues(struct char_data *ch, struct obj_data *cyberdeck);

#endif
