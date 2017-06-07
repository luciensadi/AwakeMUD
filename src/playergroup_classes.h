/******************************************************
* Code for the handling of player groups. Written for *
* AwakeMUD Community Edition by Lucien Sadi. 06/03/17 *
******************************************************/

#ifndef _playergroup_class_h
#define _playergroup_class_h
#include "playergroups.h"

/* Playergroup Class */
class Playergroup {
  long idnum;
  const char *tag;
  const char *name;
  const char *alias;
  
  Bitfield settings;
public:
  Playergroup *next_pgroup;

  // Constructors and destructors.
  Playergroup();
  Playergroup(long id_to_load);
  Playergroup(Playergroup *clone_from);
  ~Playergroup();
  
  // Getters
  const char *get_tag() { return tag; }
  const char *get_name() { return name; }
  const char *get_alias() { return alias; }
  long get_idnum() { return idnum; }
  
  // Setters
  void set_founded(bool founded);
  void set_disabled(bool disabled);
  void set_secret(bool secret);
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
  bool is_secret() { return settings.IsSet(PGROUP_SECRETSQUIRREL); }
  
  // Misc Methods
  bool save_pgroup_to_db();
  bool load_pgroup_from_db(long load_idnum);
  void invite(struct char_data *ch, char *argument);
  static Playergroup *find_pgroup(long idnum);
};

class Pgroup_invitation {
  time_t expires_on;
public:
  long pg_idnum;
  struct char_data *ch;
  
  Pgroup_invitation *prev;
  Pgroup_invitation *next;
  
  // Constructors
  Pgroup_invitation();
  Pgroup_invitation(long idnum);
  Pgroup_invitation(long idnum, time_t expiration);
  
  // Getters
  time_t get_expiration() { return expires_on; }
  
  // Setters
  void set_expiration(time_t expiration);
  
  // Checkers
  bool is_expired();
  static bool is_expired(time_t tm);
  
  // Misc Methods
  time_t calculate_expiration();
  bool save_invitation_to_db();
  static void prune_expired(struct char_data *ch);
};

#endif
