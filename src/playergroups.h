/******************************************************
* Code for the handling of player groups. Written for *
* AwakeMUD Community Edition by Lucien Sadi. 06/03/17 *
******************************************************/

#ifndef playergroups_h
#define playergroups_h

// Privileges.
#define PRIV_ADMINISTRATOR     0      // Can assign any priv they possess except Admin.
#define PRIV_AUDITOR           1      // Can view the logs.
#define PRIV_ARCHITECT         2      // Can edit the PGHQ blueprints.
#define PRIV_COCONSPIRATOR     3      // In groups where membership is secret, allows viewing of roster.
#define PRIV_DRIVER            4      // Can lock/unlock and drive any PG vehicle.
#define PRIV_LANDLORD          5      // Can lease apts, relinquish apts, and add guests.
#define PRIV_LEADER            6      // Can do anything.
#define PRIV_MANAGER           7      // Can promote, demote, and outcast.
#define PRIV_MECHANIC          8      // Can perform modifications to PG vehicles.
#define PRIV_PROCURER          9      // Can purchase/sell PG vehicles.
#define PRIV_RECRUITER         10     // Can invite members.
#define PRIV_TENANT            11     // Can enter PG apartments.
#define PRIV_TREASURER         12     // Can withdraw from the PG bank account.
#define PRIV_NONE              10000  // No privilege required.

// Playergroup settings.
#define PGROUP_FOUNDED         0 // Set by PGROUP FOUND, indicates that group is fully founded/active.
#define PGROUP_DISABLED        1 // Set by PGROUP DISBAND.
#define PGROUP_SECRETSQUIRREL  2 // TODO: Membership is secret and requires PRIV_COCONSPIRATOR to view.
#define PGROUP_CLONE           3 // This group is a clone used for pgedit.

// Edit modes.
#define PGEDIT_CONFIRM_EDIT    0
#define PGEDIT_MAIN_MENU       1
#define PGEDIT_CHANGE_NAME     2
#define PGEDIT_CHANGE_ALIAS    3
#define PGEDIT_CHANGE_TAG      4
#define PGEDIT_CONFIRM_SAVE    5

// Helper functions.
#define GET_PGROUP_DATA(ch)  (ch)->pgroup
#define GET_PGROUP(ch)       (ch)->pgroup->pgroup

// Maximums.
#define MAX_PGROUP_RANK          10
#define MAX_PGROUP_NAME_LENGTH   80 // If you change this, update your SQL tables too.
#define MAX_PGROUP_ALIAS_LENGTH  20 // If you change this, update your SQL tables too.
#define MAX_PGROUP_TAG_LENGTH    30 // If you change this, update your SQL tables too.

// Function prototypes.
long get_new_pgroup_idnum();
void display_pgroup_help(struct char_data *ch);
void pgedit_parse(struct descriptor_data *d, const char *arg);
void pgedit_disp_menu(struct descriptor_data *d);

/* Playergroup Class */
class Playergroup {
  long idnum;
  const char *tag;
  const char *name;
  const char *alias;
  
  Bitfield settings;
public:
  // Constructors and destructors.
  Playergroup();
  Playergroup(long id_to_load);
  Playergroup(Playergroup *clone_from);
  ~Playergroup();
  
  // Getters
  const char *get_tag() { return tag; }
  const char *get_name() { return name; }
  const char *get_alias() { return alias; }
  
  // Setters
  void set_founded(bool founded);
  void set_disabled(bool disabled);
  void set_membership_secret(bool secret);
  void set_idnum(long newnum) { idnum = newnum; }
  
  // Versions of setters that perform validity checks
  bool set_tag(const char *newtitle, struct char_data *ch);
  bool set_name(const char *newname, struct char_data *ch);
  bool set_alias(const char *newalias, struct char_data *ch);
  
  // Versions of setters that do not perform validity checks (avoid using unless you're completely sure)
  void raw_set_tag(const char *newtitle);
  void raw_set_name(const char *newname);
  void raw_set_alias(const char *newalias);
  
  // Checkers
  bool is_founded() { return settings.IsSet(PGROUP_FOUNDED); }
  bool is_clone() { return settings.IsSet(PGROUP_CLONE); }
  
  // Methods
  bool save_pgroup_to_db();
  bool load_pgroup_from_db(long load_idnum);
};

#endif
