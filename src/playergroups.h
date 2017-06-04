/******************************************************
* Code for the handling of player groups. Written for *
* AwakeMUD Community Edition by Lucien Sadi. 06/03/17 *
******************************************************/

#ifndef playergroups_h
#define playergroups_h

// Renumbering to be done when the full privilege list is implemented.
#define PRIV_ADMINISTRATOR  0      // Can assign any priv they possess except Admin.
#define PRIV_AUDITOR        1      // Can view the logs.
#define PRIV_ARCHITECT      2      // Can edit the PGHQ blueprints.
#define PRIV_DRIVER         3      // Can lock/unlock and drive any PG vehicle.
#define PRIV_LANDLORD       4      // Can lease apts, relinquish apts, and add guests.
#define PRIV_LEADER         5      // Can do anything.
#define PRIV_MANAGER        6      // Can promote, demote, and outcast.
#define PRIV_MECHANIC       7      // Can perform modifications to PG vehicles.
#define PRIV_PROCURER       8      // Can purchase/sell PG vehicles.
#define PRIV_RECRUITER      9      // Can invite members.
#define PRIV_TENANT         10     // Can enter PG apartments.
#define PRIV_TREASURER      11     // Can withdraw from the PG bank account.
#define PRIV_NONE           10000  // No privilege required.

// Edit modes.
#define PGEDIT_CONFIRM_EDIT    0
#define PGEDIT_MAIN_MENU       1
#define PGEDIT_CHANGE_NAME     2
#define PGEDIT_CHANGE_ALIAS    3
#define PGEDIT_CHANGE_WHOTITLE 4
#define PGEDIT_CONFIRM_SAVE    5

// Helper functions.
#define GET_PGROUP_DATA(ch)  (ch)->pgroup
#define GET_PGROUP(ch)       (ch)->pgroup->pgroup

// This defines the highest rank in the pgroup. Could be all the way up to 255.
#define MAX_PGROUP_RANK      10

// Function prototypes.
void display_pgroup_help(struct char_data *ch);
void pgedit_parse(struct descriptor_data *d, const char *arg);
void pgedit_disp_menu(struct descriptor_data *d);

/* Playergroup Class */
class Playergroup {
  const char *whotitle;
  const char *name;
  const char *alias;
  
  Bitfield attributes;
  
  
  
public:
  // Constructors and destructors.
  Playergroup();
  ~Playergroup();
  
  // Getters
  const char *get_whotitle() { return whotitle; }
  const char *get_name() { return name; }
  const char *get_alias() { return alias; }
  
  // Setters that perform validity checks (use these unless you have a very good reason not to)
  bool set_whotitle(const char *newtitle, struct char_data *ch);
  bool set_name(const char *newname, struct char_data *ch);
  bool set_alias(const char *newalias, struct char_data *ch);
  
  // Setters that do not perform validity checks (avoid using unless you're completely sure)
  void raw_set_whotitle(const char *newtitle);
  void raw_set_name(const char *newname);
  void raw_set_alias(const char *newalias);
  
  // Checkers
  bool is_founded() { return TRUE; } // TODO
  
  // Methods
  void save_pgroup_to_db();
};

#endif
