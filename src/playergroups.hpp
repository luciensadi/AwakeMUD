/******************************************************
* Code for the handling of player groups. Written for *
* AwakeMUD Community Edition by Lucien Sadi. 06/03/17 *
******************************************************/

#ifndef _playergroups_h
#define _playergroups_h

// Privileges.
#define PRIV_ADMINISTRATOR                  0      // Can assign any priv they possess except Admin; can promote, demote, and outcast.
#define PRIV_AUDITOR                        1      // Can view the logs.
#define PRIV_ARCHITECT                      2      // Can edit the PGHQ blueprints.
#define PRIV_COCONSPIRATOR                  3      // In groups where membership is secret, allows viewing of roster.
#define PRIV_DRIVER                         4      // Can lock/unlock and drive any PG vehicle.
#define PRIV_LANDLORD                       5      // Can lease apts, relinquish apts, and add guests.
#define PRIV_LEADER                         6      // Can do anything.
#define PRIV_MECHANIC                       7      // Can perform modifications to PG vehicles.
#define PRIV_PROCURER                       8      // Can purchase/sell PG vehicles.
#define PRIV_RECRUITER                      9      // Can invite members.
#define PRIV_TENANT                         10     // Can enter PG apartments.
#define PRIV_TREASURER                      11     // Can wire money from the PG bank account.
#define PRIV_MAX                            12
/* Not an actual priv-- used for iteration over pgroup_privileges[].
   Must always be equal to total number of privileges, not including priv_none of course. */

#define PRIV_NONE                           PRIV_MAX + 1  // No privilege required. A metaprivilege, so it's fine if it shifts.

// Playergroup settings.
#define PGROUP_FOUNDED                      0 // Set by PGROUP FOUND, indicates that group is fully founded/active.
#define PGROUP_DISABLED                     1 // Set by PGROUP DISBAND.
#define PGROUP_SECRETSQUIRREL               2 // TODO: Membership is secret and requires PRIV_COCONSPIRATOR to view.
#define PGROUP_CLONE                        3 // This group is a clone used for pgedit.
#define NUM_PGROUP_SETTINGS                 4

// Edit modes.
#define PGEDIT_CONFIRM_EDIT                 0
#define PGEDIT_MAIN_MENU                    1
#define PGEDIT_CHANGE_NAME                  2
#define PGEDIT_CHANGE_ALIAS                 3
#define PGEDIT_CHANGE_TAG                   4
#define PGEDIT_CONFIRM_SAVE                 5

// Configurables.
#define NUM_MEMBERS_NEEDED_TO_FOUND         3 // TODO: Should be 3, but decreased for testing purposes.
#define COST_TO_FOUND_GROUP                 100000 // Nuyen that must be paid by the founding player in order to make a group official.
#define PGROUP_INVITATION_LIFETIME_IN_DAYS  7 // Number of IRL days an invitation will be valid for.
#define DEFAULT_PGROUP_LOG_LOOKUP_LENGTH    7 // Number of IRL days to look back when PGROUP LOGS is passed no argument.

// Helper functions.
//  GET_PGROUP_MEMBER_DATA pulls up the character's membership struct, which has rank, privileges, etc.
#define GET_PGROUP_MEMBER_DATA(ch)          (ch)->pgroup
//  GET_PGROUP pulls the actual instantiated class of the pgroup that the character is a member of.
//  Unless you're using this to set values, make sure you check for the presence of GET_PGROUP_MEMBER_DATA(ch) first to ensure the pgroup exists.
#define GET_PGROUP(ch)                      GET_PGROUP_MEMBER_DATA(ch)->pgroup

// Maximums.
#define MAX_PGROUP_RANK                     10
#define MAX_PGROUP_NAME_LENGTH              80  // If you change this, update your SQL tables too. SQL field length should be 2x+1 this (or greater).
#define MAX_PGROUP_ALIAS_LENGTH             20  // If you change this, update your SQL tables too. SQL field length should be 2x+1 this (or greater).
#define MAX_PGROUP_LOG_LENGTH               256 // If you change this, update your SQL tables too. SQL field length should be 2x+1 this (or greater).
#define MAX_PGROUP_LOG_READBACK             30 // The maximum number of days into the past players can view group logs.

// Tag maximums: Only update tag-without-color, and update your SQL tables too. SQL field length should be MAX_PGROUP_TAG_LENGTH + 1 (or greater).
#define MAX_PGROUP_TAG_LENGTH_WITHOUT_COLOR 24
#define MAX_PGROUP_TAG_LENGTH               (MAX_PGROUP_TAG_LENGTH_WITHOUT_COLOR * 8 + 2) // Accounts for color codes before each letter as well as a ^n at the end.

struct pgroup_roster_data {
  vnum_t idnum;
  Bitfield privileges;
  byte rank;
};

// Function prototypes.
long get_new_pgroup_idnum();
void display_pgroup_help(struct char_data *ch);
void pgedit_parse(struct descriptor_data *d, const char *arg);
void pgedit_disp_menu(struct descriptor_data *d);
bool has_valid_pocket_secretary(struct char_data *ch);

// Command prototypes.
void do_pgroup_abdicate(struct char_data *ch, char *argument);
void do_pgroup_balance(struct char_data *ch, char *argument);
void do_pgroup_buy(struct char_data *ch, char *argument);
void do_pgroup_contest(struct char_data *ch, char *argument);
void do_pgroup_create(struct char_data *ch, char *argument);
void do_pgroup_donate(struct char_data *ch, char *argument);
void do_pgroup_demote(struct char_data *ch, char *argument);
void do_pgroup_design(struct char_data *ch, char *argument);
void do_pgroup_disband(struct char_data *ch, char *argument);
void do_pgroup_edit(struct char_data *ch, char *argument);
void do_pgroup_found(struct char_data *ch, char *argument);
void do_pgroup_grant(struct char_data *ch, char *argument);
void do_pgroup_help(struct char_data *ch, char *argument);
void do_pgroup_invitations(struct char_data *ch, char *argument);
void do_pgroup_invite(struct char_data *ch, char *argument);
void do_pgroup_lease(struct char_data *ch, char *argument);
void do_pgroup_logs(struct char_data *ch, char *argument);
void do_pgroup_note(struct char_data *ch, char *argument);
void do_pgroup_outcast(struct char_data *ch, char *argument);
void do_pgroup_promote(struct char_data *ch, char *argument);
void do_pgroup_privileges(struct char_data *ch, char *argument);
void do_pgroup_resign(struct char_data *ch, char *argument);
void do_pgroup_revoke(struct char_data *ch, char *argument);
void do_pgroup_roster(struct char_data *ch, char *argument);
void do_pgroup_status(struct char_data *ch, char *argument);
void do_pgroup_transfer(struct char_data *ch, char *argument);
void do_pgroup_vote(struct char_data *ch, char *argument);
void do_pgroup_wire(struct char_data *ch, char *argument);

bool pgroup_char_has_any_priv(idnum_t char_idnum, idnum_t pgroup_idnum, Bitfield privileges);

#endif
