/******************************************************
 * Code for the handling of player groups. Written for *
 * AwakeMUD Community Edition by Lucien Sadi. 06/03/17 *
 ******************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "awake.h"
#include "structs.h"
#include "newdb.h"
#include "db.h"
#include "utils.h"
#include "interpreter.h"
#include "constants.h"
#include "playergroups.h"
#include "playergroup_classes.h"
#include "olc.h"
#include "handler.h"
#include "comm.h"
#include "newdb.h"

// Externs from other files.
extern void store_mail(long to, struct char_data *from, const char *message_pointer);

// Prototypes from this file.
void perform_pgroup_grant_revoke(struct char_data *ch, char *argument, bool revoke);
const char *pgroup_print_privileges(Bitfield privileges);
void do_pgroup_promote_demote(struct char_data *ch, char *argument, bool promote);

// The linked list of loaded playergroups.
Playergroup *loaded_playergroups = NULL;

// The invitations command, for viewing your pgroup invitations.
ACMD(do_invitations) {
  FAILURE_CASE(IS_NPC(ch), "NPCs don't get playergroup invitations.");
  FAILURE_CASE(ch->pgroup_invitations == NULL, "You have no playergroup invitations at this time.");
  
  // Remove any expired invitations.
  Pgroup_invitation::prune_expired(ch);
  
  char expiration_string[20];
  Pgroup_invitation *pgi = ch->pgroup_invitations;
  Playergroup *pgr = NULL;
  send_to_char("You have invitations from: \r\n", ch);
  while (pgi) {
    pgr = pgi->get_pg();
    
    // Skip over anything from a disabled group.
    if (pgr->is_disabled()) {
      pgi = pgi->next;
      continue;
    }
    time_t expiration = pgi->get_expiration();
    strftime(expiration_string, 20, "%Y-%m-%d %H:%M:%S", localtime(&expiration));
    sprintf(buf, " '%s' (%s), which expires on %s\r\n", pgr->get_name(), pgr->get_alias(), expiration_string);
    pgi = pgi->next;
  }
  send_to_char(buf, ch);
}

// The accept command.
ACMD(do_accept) {
  FAILURE_CASE(IS_NPC(ch), "NPCs don't get playergroup invitations.");
  FAILURE_CASE(ch->pgroup_invitations == NULL, "You have no playergroup invitations at this time.");
  
  if (!*argument) {
    send_to_char(ch, "Syntax: ACCEPT <alias of playergroup>.\r\n", ch);
    do_invitations(ch, NULL, 0, 0);
    return;
  }
  
  skip_spaces(&argument);
  
  // Remove any expired invitations.
  Pgroup_invitation::prune_expired(ch);
  
  Pgroup_invitation *pgi = ch->pgroup_invitations;
  Playergroup *pgr;
  while (pgi) {
    pgr = pgi->get_pg();
    
    // Skip over anything from a disabled group.
    if (pgr->is_disabled()) {
      pgi = pgi->next;
      continue;
    }
    
    // if argument matches case-insensitively with invitation's group's alias
    if (strn_cmp(argument, pgr->get_name(), strlen(argument)) == 0 || strn_cmp(argument, pgr->get_alias(), strlen(argument)) == 0) {
      send_to_char(ch, "You accept the invitation and declare yourself a member of '%s'.\r\n", pgr->get_name());
      
      // Notify all online members. If someone is offline, they need to check the roster or logs to see what changed.
      for (struct char_data* i = character_list; i; i = i->next) {
        if (!IS_NPC(i) && GET_PGROUP_MEMBER_DATA(i) && GET_PGROUP(i)->get_idnum() == pgr->get_idnum()) {
          // Notify the character.
          if (!pgr->is_secret() || !GET_PGROUP_MEMBER_DATA(i)->privileges.AreAnySet(PRIV_COCONSPIRATOR, PRIV_LEADER, ENDBIT)) {
            send_to_char(i, "%s has joined '%s'.\r\n", GET_CHAR_NAME(ch), pgr->get_name());
          }
        }
      }
      
      // Add the player to the group. This function also removes the invitation and logs the character's joining (if applicable).
      pgr->add_member(ch);
      
      return;
    }
    pgi = pgi->next;
  }
  
  send_to_char(ch, "You don't seem to have any invitations from '%s'.\r\n", argument);
}

// The decline command.
ACMD(do_decline) {
  FAILURE_CASE(IS_NPC(ch), "NPCs don't get playergroup invitations.");
  FAILURE_CASE(ch->pgroup_invitations == NULL, "You have no playergroup invitations at this time.");
  
  if (!*argument) {
    send_to_char(ch, "Syntax: DECLINE <alias of playergroup>.\r\n", ch);
    do_invitations(ch, NULL, 0, 0);
    return;
  }
  
  skip_spaces(&argument);
  
  // Remove any expired invitations.
  Pgroup_invitation::prune_expired(ch);
  
  Pgroup_invitation *pgi = ch->pgroup_invitations;
  Playergroup *pgr;
  while (pgi) {
    pgr = pgi->get_pg();
    
    // Skip over anything from a disabled group, but don't delete-- they'll auto-expire on their own.
    if (pgr->is_disabled()) {
      pgi = pgi->next;
      continue;
    }
    
    // If argument matches case-insensitively with invitation's group's alias:
    if (strn_cmp(argument, pgr->get_name(), strlen(argument)) == 0 || strn_cmp(argument, pgr->get_alias(), strlen(argument)) == 0) {
      send_to_char(ch, "You decline the invitation from '%s'.\r\n", pgr->get_name());
      
      // Drop the invitation.
      if (pgi->prev)
        pgi->prev->next = pgi->next;
      else
        ch->pgroup_invitations = pgi->next;
      if (pgi->next)
        pgi->next->prev = pgi->prev;
      delete pgi;
      return;
    }
    pgi = pgi->next;
  }
  
  send_to_char(ch, "You don't seem to have any invitations from '%s'.\r\n", argument);
}

// Find or load the specified group.
Playergroup *Playergroup::find_pgroup(long idnum) {
  Playergroup *pgr = loaded_playergroups;
  
  if (!idnum) {
    log_vfprintf("SYSERR: Illegal pgroup num %ld passed to find_pgroup.", idnum);
    return NULL;
  }
  
  while (pgr) {
    if (pgr->get_idnum() == idnum)
      return pgr;
    
    pgr = pgr->next_pgroup;
  }
  
  return new Playergroup(idnum);
}

struct pgroup_cmd_struct {
  const char *cmd;
  dword privilege_required;
  void (*command_pointer) (struct char_data *ch, char *argument);
  bool valid_while_group_not_founded;
  bool valid_while_group_is_founded;
  bool requires_pocket_secretary;
  bool valid_while_group_disabled;
  bool requires_coconspirator_if_secret;
} pgroup_commands[] = {                                     /* !founded founded pocsec disabld secret */
  { "abdicate"   , PRIV_LEADER        , do_pgroup_abdicate    , FALSE , TRUE  , FALSE , FALSE , FALSE },
  { "balance"    , PRIV_TREASURER     , do_pgroup_balance     , FALSE , TRUE  , TRUE  , FALSE , FALSE },
  { "buy"        , PRIV_PROCURER      , do_pgroup_buy         , FALSE , TRUE  , TRUE  , FALSE , FALSE },
  { "contest"    , PRIV_NONE          , do_pgroup_contest     , FALSE , TRUE  , TRUE  , FALSE , FALSE },
  { "create"     , PRIV_NONE          , do_pgroup_create      , TRUE  , TRUE  , TRUE  , FALSE , FALSE },
  { "demote"     , PRIV_MANAGER       , do_pgroup_demote      , FALSE , TRUE  , TRUE  , FALSE , TRUE  },
  { "donate"     , PRIV_NONE          , do_pgroup_donate      , FALSE , TRUE  , FALSE , FALSE , FALSE },
  { "design"     , PRIV_ARCHITECT     , do_pgroup_design      , FALSE , TRUE  , TRUE  , FALSE , FALSE },
  { "disband"    , PRIV_LEADER        , do_pgroup_disband     , TRUE  , TRUE  , TRUE  , FALSE , FALSE },
  { "edit"       , PRIV_LEADER        , do_pgroup_edit        , TRUE  , TRUE  , TRUE  , FALSE , FALSE },
  { "found"      , PRIV_LEADER        , do_pgroup_found       , TRUE  , FALSE , TRUE  , FALSE , FALSE },
  { "grant"      , PRIV_ADMINISTRATOR , do_pgroup_grant       , FALSE , TRUE  , TRUE  , FALSE , TRUE  },
  { "help"       , PRIV_NONE          , do_pgroup_help        , TRUE  , TRUE  , FALSE , FALSE , FALSE },
  { "invite"     , PRIV_RECRUITER     , do_pgroup_invite      , TRUE  , TRUE  , TRUE  , FALSE , FALSE },
  { "invitations", PRIV_RECRUITER     , do_pgroup_invitations , TRUE  , TRUE  , TRUE  , FALSE , TRUE  },
  { "lease"      , PRIV_LANDLORD      , do_pgroup_lease       , FALSE , TRUE  , TRUE  , FALSE , FALSE },
  { "logs"       , PRIV_AUDITOR       , do_pgroup_logs        , TRUE  , TRUE  , TRUE  , FALSE , FALSE },
  { "note"       , PRIV_NONE          , do_pgroup_note        , TRUE  , TRUE  , TRUE  , FALSE , FALSE },
  { "outcast"    , PRIV_MANAGER       , do_pgroup_outcast     , TRUE  , TRUE  , TRUE  , FALSE , TRUE  },
  { "privileges" , PRIV_NONE          , do_pgroup_privileges  , TRUE  , TRUE  , TRUE  , FALSE , FALSE },
  { "promote"    , PRIV_MANAGER       , do_pgroup_promote     , FALSE , TRUE  , TRUE  , FALSE , TRUE  },
  { "quit"       , PRIV_NONE          , do_pgroup_resign      , TRUE  , TRUE  , FALSE , FALSE , FALSE },
  { "resign"     , PRIV_NONE          , do_pgroup_resign      , TRUE  , TRUE  , FALSE , TRUE  , FALSE },
  { "revoke"     , PRIV_ADMINISTRATOR , do_pgroup_revoke      , FALSE , TRUE  , TRUE  , FALSE , TRUE  },
  { "roster"     , PRIV_NONE          , do_pgroup_roster      , TRUE  , TRUE  , TRUE  , FALSE , TRUE  },
  { "status"     , PRIV_NONE          , do_pgroup_status      , TRUE  , TRUE  , TRUE  , TRUE  , FALSE },
  { "transfer"   , PRIV_PROCURER      , do_pgroup_transfer    , FALSE , TRUE  , TRUE  , FALSE , FALSE },
  { "vote"       , PRIV_NONE          , do_pgroup_vote        , FALSE , TRUE  , TRUE  , FALSE , FALSE },
  { "wire"       , PRIV_TREASURER     , do_pgroup_wire        , FALSE , TRUE  , FALSE , FALSE , FALSE },
  { "\n"         , 0                  , 0                     , FALSE , TRUE  , FALSE , FALSE , FALSE } // This must be last.
};                                                          /* !founded founded pocsec disabld secret */


/* Main Playergroup Command */
ACMD(do_pgroup) {
  FAILURE_CASE(IS_NPC(ch), "NPCs can't participate in PC groups.");
  
  char command[MAX_STRING_LENGTH];
  char parameters[MAX_STRING_LENGTH];
  int cmd_index = 0;
  
  skip_spaces(&argument);
  half_chop(argument, command, parameters);
  
  // In the absence of a command, show help and exit.
  if (!*command) {
    display_pgroup_help(ch);
    return;
  }
  
  // Find the index of the command the player wants.
  for (cmd_index = 0; *(pgroup_commands[cmd_index].cmd) != '\n'; cmd_index++)
    if (!strncmp(command, pgroup_commands[cmd_index].cmd, strlen(command)))
      break;
  
  // Precondition: If the command was invalid, show help and exit.
  if (*(pgroup_commands[cmd_index].cmd) == '\n') {
    display_pgroup_help(ch);
    return;
  }
  
  // Is a member of a group.
  if (GET_PGROUP_MEMBER_DATA(ch)) {
    // Precondition: Your data structures must be correctly initialized.
    if (!GET_PGROUP(ch)) {
      sprintf(buf, "SYSERR: Somehow, %s (%ld) is part of an invalid group.",
              GET_CHAR_NAME(ch), GET_IDNUM(ch));
      mudlog(buf, ch, LOG_MISCLOG, TRUE);
      log(buf);
      send_to_char("We're sorry, but your character is currently bugged. Please log out and back in.\r\n"
                   "If that doesn't fix the problem, please reach out to the staff for help.\r\n", ch);
      return;
    }
    
    // Precondition: If your group is disabled, the only commands you can use are LOGS, STATUS, and RESIGN.
    if (GET_PGROUP(ch)->is_disabled() && !pgroup_commands[cmd_index].valid_while_group_disabled) {
      send_to_char("You can't perform that action after your group has been disbanded.\r\n", ch);
      display_pgroup_help(ch);
      return;
    }
    
    // Precondition: If the command requires a kosherized group, you must be part of one.
    if (!GET_PGROUP(ch)->is_founded() && !pgroup_commands[cmd_index].valid_while_group_not_founded) {
      send_to_char("You need to be a member of a fully-fledged playergroup to do that.\r\n", ch);
      return;
    }
    
    if (GET_PGROUP(ch)->is_founded() && !pgroup_commands[cmd_index].valid_while_group_is_founded) {
      // Deliberately letting this fall through-- only candidate command right now is FOUND and it has its own error message.
    }
    
    // Precondition: You must have the appropriate privilege (or be leader) to perform this command.
    if (pgroup_commands[cmd_index].privilege_required != PRIV_NONE && !GET_PGROUP_MEMBER_DATA(ch)->privileges.IsSet(PRIV_LEADER)) {
      // If the group is secret, and you're not the leader, you must have the COCONSPIRATOR priv to perform sensitive actions.
      if (!(GET_PGROUP_MEMBER_DATA(ch)->privileges.IsSet(pgroup_commands[cmd_index].privilege_required))) {
        send_to_char(ch, "You must be %s %s within your group to do that.\r\n",
                     strchr((const char *)"aeiouyAEIOUY", *pgroup_privileges[pgroup_commands[cmd_index].privilege_required]) ? "an" : "a",
                     pgroup_privileges[pgroup_commands[cmd_index].privilege_required]);
        return;
      }
    }
    
    if (GET_PGROUP(ch)->is_secret() && pgroup_commands[cmd_index].requires_coconspirator_if_secret) {
      if (!(GET_PGROUP_MEMBER_DATA(ch)->privileges.AreAnySet(PRIV_COCONSPIRATOR, PRIV_LEADER, ENDBIT))) {
        send_to_char("This group is clandestine, so you need to be a Co-Conspirator to do that.\r\n", ch);
        return;
      }
    }
  }
  
  // Not a member of a group.
  else {
    // Precondition: If you are not part of a group, the only command you can use is CREATE.
    if (pgroup_commands[cmd_index].command_pointer != do_pgroup_create) {
      send_to_char("You need to be a member of a playergroup to do that.\r\n", ch);
      return;
    }
  }
  
  // Precondition: You must have an accessible pocket secretary if one is required.
  if (pgroup_commands[cmd_index].requires_pocket_secretary && !has_valid_pocket_secretary(ch)) {
    send_to_char("You must have an unlocked pocket secretary to do that.\r\n", ch);
    return;
  }
  
  // Execute the subcommand.
  ((*pgroup_commands[cmd_index].command_pointer) (ch, parameters));
}

void do_pgroup_abdicate(struct char_data *ch, char *argument) {
  // TODO: Successor mode, where you can specify who becomes the next leader.
  // TODO: Line of succession, where the next-highest-ranking member becomes the next leader.
  
  // Require confirmation.
  if (!*argument || str_cmp(argument, "confirm") != 0) {
    send_to_char(ch, "If you're sure you want to abdicate from leadership of '%s', type PGROUP ABDICATE CONFIRM.\r\n",
                 GET_PGROUP(ch)->get_name());
    return;
  }
  
  // Demote them one rank to 9 and remove their leader bit.
  GET_PGROUP_MEMBER_DATA(ch)->rank = 9;
  GET_PGROUP_MEMBER_DATA(ch)->privileges.RemoveBit(PRIV_LEADER);
  
  Playergroup *pgr = GET_PGROUP(ch);
  
  // Notify the character and log it.
  send_to_char(ch, "You abdicate your leadership position in '%s'.\r\n", GET_PGROUP(ch)->get_name());
  pgr->audit_log_vfprintf("%s has abdicated from leadership of the group.", GET_CHAR_NAME(ch));
  pgr->secret_log("A shadowy figure has abdicated from leadership of the group.");
  
  // Find all group members and add them to a list.
  sprintf(buf2, "SELECT idnum FROM pfiles_playergroups WHERE `group` = %ld", pgr->get_idnum());
  mysql_wrapper(mysql, buf2);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row;
  int idnum;
  listClass<struct pgroup_roster_data *> results;
  nodeStruct<struct pgroup_roster_data *> *ns = NULL;
  pgroup_roster_data *roster_data = NULL;
  while ((row = mysql_fetch_row(res))) {
    idnum = atoi(row[0]);
    if (idnum == GET_IDNUM(ch))
      continue;
    
    roster_data = new struct pgroup_roster_data;
    roster_data->idnum = idnum;
    results.AddItem(NULL, roster_data);
  }
  mysql_free_result(res);
  
  // Compose a mail message.
  if (GET_PGROUP(ch)->is_secret()) {
    sprintf(buf, "A shadowy figure has abdicated from the leadership of '%s'.", pgr->get_name());
  } else {
    sprintf(buf, "%s has abdicated from the leadership of '%s'.", GET_CHAR_NAME(ch), pgr->get_name());
  }
  
  // Mail everyone in the list.
  while ((ns = results.Head())) {
    store_mail(ns->data->idnum, ch, buf);
    delete ns->data;
    results.RemoveItem(ns);
  }
}

void do_pgroup_balance(struct char_data *ch, char *argument) {
  send_to_char(ch, "%s currently has %lu nuyen in its accounts.", GET_PGROUP(ch)->get_name(), GET_PGROUP(ch)->get_bank());
}

void do_pgroup_buy(struct char_data *ch, char *argument) {
  // TODO: Log.
  // pgroup  buy  <vehicle>  Purchase a vehicle, drawing funds from bank and assigning ownership to pgroup.
  send_to_char("buy", ch);
}

void do_pgroup_contest(struct char_data *ch, char *argument) {
  // TODO: Log.
  // pgroup  contest  [confirm]      On confirm, kicks off election cycle. If cycle open, joins cycle as nominee.
  send_to_char("contest", ch);
}

void do_pgroup_create(struct char_data *ch, char *argument) {
  // If the player is already in a group, they can't create a new one.
  FAILURE_CASE(GET_PGROUP_MEMBER_DATA(ch), "You are already part of a playergroup.");
  
  // Set char editing. This does not remove the character from the game world.
  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  STATE(ch->desc) = CON_PGEDIT;
  
  // Create the new pgroup and add it to the player's descriptor's edit pointer.
  ch->desc->edit_pgroup = new Playergroup();
  ch->desc->edit_pgroup->set_idnum(0);
  ch->desc->edit_pgroup->raw_set_tag("^R[^WEMPTY^R]^n");
  ch->desc->edit_pgroup->raw_set_name("An Unimaginative Player Group");
  ch->desc->edit_pgroup->raw_set_alias("newgroup");
  
  // Put character into the playergroup edit menu. Logging is handled there.
  ch->desc->edit_mode = PGEDIT_MAIN_MENU;
  pgedit_disp_menu(ch->desc);
}

void do_pgroup_demote(struct char_data *ch, char *argument) {
  do_pgroup_promote_demote(ch, argument, FALSE);
}

void do_pgroup_design(struct char_data *ch, char *argument) {
  // TODO: Log
  send_to_char("Sorry, the design feature is still under construction.", ch);
  
  /* Requirements for design:
   - You must obtain an architect's drafting board (to be built)
   - You must deploy this drafting board (like a workshop)
   - You must sit at the drafting board (like a computer)
   - PGROUP DESIGN puts you into a mode where you're essentially projecting (body vulnerable to attack, etc)
   
   Creates a new zone (or uses one that already exists if you've done this before) and puts you in a new empty room.
   Allows use of BLUEPRINT commands. Changes save automatically (there's no undo, just like normal building).
   Allows RETURN to return to your body.
   
   You can also PGROUP DESIGN TALLY|TOTAL|COST to see how much your drafted HQ will cost to actualize.
   You can then PGROUP DESIGN FINALIZE to lock it, pay the deposit, and send it to staff for approval.
   
   Staff can approve it (clones into a real zone for connection to world) or deny it (refunds nuyen to group, unlocks blueprint).
   */
}

void do_pgroup_disband(struct char_data *ch, char *argument) {
  char query_buf[512];
  
  if (!*argument || str_cmp(argument, "confirm") != 0) {
    send_to_char(ch, "If you're sure you want to disband '%s', type PGROUP DISBAND CONFIRM.\r\n",
                 GET_PGROUP(ch)->get_name());
    return;
  }
  
  Playergroup *pgr = GET_PGROUP(ch);
  
  // Read out the people who are getting kicked (in case we want to manually restore later).
  sprintf(query_buf, "SELECT idnum, Rank, Privileges FROM pfiles_playergroups WHERE `group` = %ld ORDER BY Rank ASC", pgr->get_idnum());
  mysql_wrapper(mysql, query_buf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row;
  listClass<struct pgroup_roster_data *> results;
  nodeStruct<struct pgroup_roster_data *> *ns = NULL;
  pgroup_roster_data *roster_data = NULL;
  while ((row = mysql_fetch_row(res))) {
    roster_data = new struct pgroup_roster_data;
    roster_data->idnum = atoi(row[0]);
    roster_data->rank = atoi(row[1]);
    roster_data->privileges.FromString(row[2]);
    results.AddItem(NULL, roster_data);
  }
  mysql_free_result(res);
  
  // Log the results of the above.
  sprintf(buf, "The following characters are being kicked from '%s' (%s, %ld) due to disbanding by %s: \r\n",
          pgr->get_name(),
          pgr->get_alias(),
          pgr->get_idnum(),
          GET_CHAR_NAME(ch));
  
  while ((ns = results.Head())) {
    if (!strcmp(ns->data->privileges.ToString(), "0"))
      sprintf(ENDOF(buf), "%s, rank %d (no privileges).\r\n", get_player_name(ns->data->idnum), ns->data->rank);
    else {
      sprintf(ENDOF(buf), "%s, rank %d, privs: %s.\r\n",
              get_player_name(ns->data->idnum), ns->data->rank, pgroup_print_privileges(ns->data->privileges));
    }
    delete ns->data;
    results.RemoveItem(ns);
  }
  log(buf);
  
  // Delete pfile pgroup associations.
  sprintf(query_buf, "DELETE FROM pfiles_playergroups WHERE `group` = %ld", pgr->get_idnum());
  mysql_wrapper(mysql, query_buf);
  
  // Delete pgroup's sent invitations.
  sprintf(query_buf, "DELETE FROM playergroup_invitations WHERE `Group` = %ld", pgr->get_idnum());
  mysql_wrapper(mysql, query_buf);
  
  /* We deliberately do not remove old logfiles. They can be cleaned up by hand if they clutter things up.
   * This prevents any issues where players might abuse pgroup commands and then delete them to hide their tracks. */
  
  // Remove this group from all active players' char_data structs. Delete members' pgroup_data pointers.
  for (struct char_data* i = character_list; i; i = i->next) {
    if (!IS_NPC(i) && GET_PGROUP_MEMBER_DATA(i) && GET_PGROUP(i)->get_idnum() == pgr->get_idnum()) {
      // Notify the character, unless they're the person doing the disbanding.
      if (i != ch) {
        send_to_char(i, "The playergroup '%s' has been disbanded.", pgr->get_name());
      }
      
      // Wipe out the data and null the pointer.
      delete GET_PGROUP_MEMBER_DATA(i);
      GET_PGROUP_MEMBER_DATA(i) = NULL;
    }
  }
  
  // Notify the ex-leader that the job is (nearly) complete.
  send_to_char(ch, "You disband the group '%s'.\r\n", pgr->get_name());
  pgr->audit_log_vfprintf("%s disbanded the group.", GET_CHAR_NAME(ch));
  pgr->secret_log("A shadowy figure disbanded the group.");
  
  // Disable the group.
  pgr->set_disabled(TRUE);
  
  // Save the changes to the DB.
  pgr->save_pgroup_to_db();
}

void do_pgroup_donate(struct char_data *ch, char *argument) {
  // Wires the specified amount from your bank account to the pgroup's shell accounts.
  // pgroup  donate  <amount>  <reason>   Deposits <amount> of nuyen. Reason is listed in logs.
  long amount;
  
  char *remainder = any_one_arg(argument, buf); // Amount, remainder = reason
  skip_spaces(&remainder);
  
  if ((amount = atol(buf)) <= 0) {
    send_to_char(ch, "How much nuyen do you wish to donate to '%s'? (Syntax: DONATE <amount> <reason>)\r\n", GET_PGROUP(ch)->get_name());
    return;
  }
  
  if (amount > GET_BANK(ch)) {
    send_to_char(ch, "Your bank account only has %ld nuyen in it.\r\n", GET_BANK(ch));
    return;
  }
  
  if (!*remainder) {
    send_to_char("You must specify a reason for transferring these funds. (Syntax: DONATE <amount> <reason>)\r\n", ch);
    return;
  }
  
  if (strlen(remainder) > MAX_PGROUP_LOG_LENGTH - 2) {
    send_to_char(ch, "That reason is too long; you're limited to %d characters.", MAX_PGROUP_LOG_LENGTH - 2);
    return;
  }
  
  // Execute the change.
  GET_BANK(ch) -= amount;
  GET_PGROUP(ch)->set_bank(GET_PGROUP(ch)->get_bank() + amount);
  GET_PGROUP(ch)->save_pgroup_to_db();
  
  // Log it.
  GET_PGROUP(ch)->audit_log_vfprintf("%s donated %ld nuyen. Reason: %s",
                                     GET_CHAR_NAME(ch),
                                     amount,
                                     remainder);
  GET_PGROUP(ch)->secret_log_vfprintf("A shadowy figure donated %ld nuyen. Reason: %s",
                                      amount,
                                      remainder);
  sprintf(buf, "%s donated %ld nuyen to '%s'. Reason: %s",
          GET_CHAR_NAME(ch),
          amount,
          GET_PGROUP(ch)->get_name(),
          remainder);
  mudlog(buf, ch, LOG_GRIDLOG, TRUE);
  
  send_to_char(ch, "You donate %ld nuyen to '%s'.\r\n",
               amount,
               GET_PGROUP(ch)->get_name());
}

void do_pgroup_edit(struct char_data *ch, char *argument) {
  // Create a clone from the player's current group.
  ch->desc->edit_pgroup = new Playergroup(GET_PGROUP(ch));
  
  // Put character into the playergroup edit menu. Logging is handled there.
  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  STATE(ch->desc) = CON_PGEDIT;
  ch->desc->edit_mode = PGEDIT_MAIN_MENU;
  pgedit_disp_menu(ch->desc);
}

void do_pgroup_found(struct char_data *ch, char *argument) {
  // Precondition: The group may not have already been founded.
  FAILURE_CASE(GET_PGROUP(ch)->is_founded(), "Your group has already been founded.");
  
  // Precondition: The group must meet the minimum membership requirements.
  int member_count = GET_PGROUP(ch)->sql_count_members();
  if (member_count < NUM_MEMBERS_NEEDED_TO_FOUND) {
    send_to_char(ch, "You need %d members to found your group, but you only have %d.\r\n",
                 NUM_MEMBERS_NEEDED_TO_FOUND, member_count);
    return;
  }
  
  // Precondition: The player must have enough nuyen to found the group.
  if (GET_NUYEN(ch) < COST_TO_FOUND_GROUP) {
    send_to_char(ch, "You need to have %d nuyen on hand to found your group.\r\n", COST_TO_FOUND_GROUP);
    return;
  }
  
  // TODO: Should this be done in a specific place or in the presence of a specific NPC for RP reasons?
  send_to_char(ch, "You pay %d nuyen to found '%s'.\r\n", COST_TO_FOUND_GROUP, GET_PGROUP(ch)->get_name());
  GET_NUYEN(ch) -= COST_TO_FOUND_GROUP;
  playerDB.SaveChar(ch);
  
  GET_PGROUP(ch)->set_founded(TRUE);
  GET_PGROUP(ch)->save_pgroup_to_db();
  
  // Make a note in the group's audit log.
  GET_PGROUP(ch)->audit_log_vfprintf("%s has officially founded the group.", GET_CHAR_NAME(ch));
  GET_PGROUP(ch)->secret_log_vfprintf("A shadowy figure has officially founded the group.");
}

void do_pgroup_grant(struct char_data *ch, char *argument) {
  perform_pgroup_grant_revoke(ch, argument, FALSE);
}

void do_pgroup_help(struct char_data *ch, char *argument) {
  send_to_char("Sorry, the help feature is still under construction.", ch);
  
  // pgroup  help  [command]     Displays info on pgroup or its subcommands.
}

void do_pgroup_invitations(struct char_data *ch, char *argument) {
  send_to_char("Sorry, the invitations feature is still under construction.", ch);
  // pgroup  invitations  [revoke]  [name]  No args: Displays group's open invitations. With revoke: Deletes the specified invitation.
}

void do_pgroup_invite(struct char_data *ch, char *argument) {
  // Wrapper for pgroup's invitation method. Logging is handled there.
  GET_PGROUP(ch)->invite(ch, argument);
}

void do_pgroup_lease(struct char_data *ch, char *argument) {
  // TODO: Log.
  // pgroup  lease  <apartment>      Lease an apartment in your pgroup's name using pgroup bank funds.
  send_to_char("Sorry, the lease feature is still under construction.", ch);
}

void do_pgroup_logs(struct char_data *ch, char *argument) {
  // Retrieve the logs for the last X days, where X = 7 unless specified.
  // TODO: Potentially allow PGROUP LOGS (x) (y) to select logs between days X and Y.
  int days;
  
  if (!*argument) {
    days = DEFAULT_PGROUP_LOG_LOOKUP_LENGTH;
  } else {
    days = atoi(argument);
    if (days < 1) {
      send_to_char("Syntax: PGROUP LOGS [number of days of history to include]\r\n", ch);
      return;
    }
    if (days > MAX_PGROUP_LOG_READBACK) {
      send_to_char(ch, "You can only view the logs created in the last %d days.\r\n", MAX_PGROUP_LOG_READBACK);
      return;
    }
  }
  
  bool redacted_mode = GET_PGROUP(ch)->is_secret() && !GET_PGROUP_MEMBER_DATA(ch)->privileges.AreAnySet(PRIV_COCONSPIRATOR, PRIV_LEADER, ENDBIT);
  
  // Select the logs between this moment and X days in the past, then display in descending order.
  char querybuf[MAX_STRING_LENGTH];
  const char *query_fmt = "SELECT message FROM pgroup_logs"
  " WHERE "
  "  idnum = %ld"
  "  AND DATE_SUB(CURDATE(), INTERVAL %d DAY) <= DATE(date)"
  "  AND `redacted` = %s"
  " ORDER BY date ASC";
  
  sprintf(querybuf, query_fmt, GET_PGROUP(ch)->get_idnum(), days, redacted_mode ? "TRUE" : "FALSE");
  mysql_wrapper(mysql, querybuf);
  
  // Process and display the results.
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row;
  send_to_char(ch, "%s entries in the last %d days:\r\n", redacted_mode ? "Redacted log" : "Log", days);
  while ((row = mysql_fetch_row(res))) {
    send_to_char(ch, " %s\r\n", row[0]);
  }
  mysql_free_result(res);
}

void do_pgroup_note(struct char_data *ch, char *argument) {
  FAILURE_CASE(!*argument, "You must specify something to notate in the logs.");
  
  if (strlen(argument) > MAX_PGROUP_LOG_LENGTH) {
    send_to_char(ch, "Sorry, log entries must be %d characters or fewer.", MAX_PGROUP_LOG_LENGTH);
    return;
  }
  
  GET_PGROUP(ch)->audit_log_vfprintf("Note from %s: %s", GET_CHAR_NAME(ch), argument);
  GET_PGROUP(ch)->secret_log_vfprintf("Note from someone: %s", argument);
  send_to_char("You take a moment and type your note into your pocket secretary.\r\n", ch);
  WAIT_STATE(ch, 1 RL_SEC); // Lessens spam.
}

void do_pgroup_outcast(struct char_data *ch, char *argument) {
  char name[strlen(argument)];
  struct char_data *vict = NULL;
  bool vict_is_logged_in = TRUE;
  
  // Parse argument to obtain a name and a reason.
  char *reason = any_one_arg(argument, name);
  
  // Require name and reason to both be filled out.
  FAILURE_CASE(!*name || !*reason, "Syntax: PGROUP OUTCAST <character name> <reason>");
  
  // Search the online characters for someone matching the specified name.
  for (vict = character_list; vict; vict = vict->next) {
    if (!IS_NPC(vict) && (isname(name, GET_KEYWORDS(vict)) || isname(name, GET_CHAR_NAME(vict)) || recog(ch, vict, name)))
      break;
  }
  
  // If they weren't online, attempt to load them from the DB.
  if (!vict) {
    vict_is_logged_in = FALSE;
    if (!(vict = playerDB.LoadChar(name, false))) {
      // We were unable to find them online or load them from DB-- fail out.
      send_to_char("There is no such player.\r\n", ch);
      return;
    }
  }
  
  // Ensure targeted character is part of the same group as the invoking character.
  // Note that this is not info leakage-- you must be Co-Conspirator to do this in a secret group.
  if (!(GET_PGROUP_MEMBER_DATA(vict) && GET_PGROUP(vict) && GET_PGROUP(vict) == GET_PGROUP(ch))) {
    send_to_char(ch, "%s's not part of your group.\r\n", capitalize(HSSH(vict)));
    return;
  }
  
  // Prevent modification of someone higher than you.
  if (GET_PGROUP_MEMBER_DATA(vict)->rank >= GET_PGROUP_MEMBER_DATA(ch)->rank) {
    send_to_char(ch, "You can't do that to your %s!\r\n",
                 GET_PGROUP_MEMBER_DATA(vict)->rank > GET_PGROUP_MEMBER_DATA(ch)->rank ? "superiors" : "peers");
    return;
  }
  
  // Remove their data.
  delete GET_PGROUP_MEMBER_DATA(vict);
  GET_PGROUP_MEMBER_DATA(vict) = NULL;
  
  // Delete pfile pgroup associations.
  sprintf(buf2, "DELETE FROM pfiles_playergroups WHERE `idnum` = %ld", GET_IDNUM(vict));
  mysql_wrapper(mysql, buf2);
  
  // Delete any old invitations that could let them come right back.
  sprintf(buf2, "DELETE FROM playergroup_invitations WHERE `idnum` = %ld", GET_IDNUM(vict));
  mysql_wrapper(mysql, buf2);
  
  // Log the action.
  GET_PGROUP(ch)->audit_log_vfprintf("%s has outcasted %s (reason: %s).", 
                                      GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), reason);
  // No reason given in secret logs.
  GET_PGROUP(ch)->secret_log_vfprintf("A shadowy figure has outcasted %s.", GET_CHAR_NAME(vict));
  
  // Notify the character.
  send_to_char(ch, "You outcast %s from '%s'.\r\n", GET_CHAR_NAME(vict), GET_PGROUP(ch)->get_name());
  sprintf(buf, "You have been outcasted from '%s' (reason: %s).\r\n", GET_PGROUP(ch)->get_name(), reason);
  store_mail(GET_IDNUM(vict), ch, buf);
  
  // Save the character.
  if (vict_is_logged_in) {
    // Online characters are saved to the DB without unloading.
    playerDB.SaveChar(vict, GET_LOADROOM(vict));
  } else {
    // Loaded characters are unloaded (saving happens during extract_char).
    extract_char(vict);
  }
}

void do_pgroup_promote(struct char_data *ch, char *argument) {
  do_pgroup_promote_demote(ch, argument, TRUE);
}

void do_pgroup_privileges(struct char_data *ch, char *argument) {
  send_to_char(ch, "You have the following privileges in '%s': %s\r\n",
               GET_PGROUP(ch)->get_name(), pgroup_print_privileges(GET_PGROUP_MEMBER_DATA(ch)->privileges));
}

void do_pgroup_resign(struct char_data *ch, char *argument) {
  skip_spaces(&argument);
  
  // Leaders can't resign without first abdicating.
  FAILURE_CASE(GET_PGROUP_MEMBER_DATA(ch)->privileges.IsSet(PRIV_LEADER), 
               "You can't resign without abdicating your leadership first.");
  
  // Require confirmation.
  if (!*argument || str_cmp(argument, "confirm") != 0) {
    send_to_char(ch, "If you're sure you want to resign from '%s', type PGROUP RESIGN CONFIRM.\r\n",
                 GET_PGROUP(ch)->get_name());
    return;
  }
  
  // Log the action. No secret log here.
  GET_PGROUP(ch)->audit_log_vfprintf("%s has resigned from the group.", GET_CHAR_NAME(ch));
  
  // Notify the player, then strip their membership.
  send_to_char(ch, "You resign your position in '%s'.\r\n", GET_PGROUP(ch)->get_name());
  GET_PGROUP(ch)->remove_member(ch);
}

void do_pgroup_revoke(struct char_data *ch, char *argument) {
  // Revoke priv from player.
  perform_pgroup_grant_revoke(ch, argument, TRUE);
}

void do_pgroup_roster(struct char_data *ch, char *argument) {
  Playergroup *pgr = GET_PGROUP(ch);
  
  char query_buf[512];
  sprintf(query_buf, "SELECT idnum, Rank, Privileges FROM pfiles_playergroups WHERE `group` = %ld ORDER BY Rank ASC", pgr->get_idnum());
  mysql_wrapper(mysql, query_buf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row;
  
  listClass<struct pgroup_roster_data *> results;
  nodeStruct<struct pgroup_roster_data *> *ns = NULL;
  pgroup_roster_data *roster_data = NULL;
  while ((row = mysql_fetch_row(res))) {
    roster_data = new struct pgroup_roster_data;
    roster_data->idnum = atoi(row[0]);
    roster_data->rank = atoi(row[1]);
    roster_data->privileges.FromString(row[2]);
    results.AddItem(NULL, roster_data);
  }
  mysql_free_result(res);
  
  sprintf(buf, "%s has the following members:\r\n", pgr->get_name());
  while ((ns = results.Head())) {
    if (!strcmp(ns->data->privileges.ToString(), "0"))
      sprintf(ENDOF(buf), "%s, rank %d (no privileges).\r\n", get_player_name(ns->data->idnum), ns->data->rank);
    else {
      sprintf(ENDOF(buf), "%s, rank %d, with the following privileges: %s.\r\n",
              get_player_name(ns->data->idnum), ns->data->rank, pgroup_print_privileges(ns->data->privileges));
    }
    delete ns->data;
    results.RemoveItem(ns);
  }
  
  send_to_char(buf, ch);
}

void do_pgroup_status(struct char_data *ch, char *argument) {
  char query_buf[512];
  Playergroup *pgr = GET_PGROUP(ch);
  
  send_to_char(ch, "Playergroup information for %s (tag %s, alias %s):\r\n\r\n", pgr->get_name(), pgr->get_tag(), pgr->get_alias());
  
  // If group is disabled, display this and end status readout.
  if (pgr->is_disabled()) {
    send_to_char("^RThis group has been disbanded by its leader and is no longer operational.^n\r\n", ch);
    return;
  }
  
  // Display founding information.
  if (!pgr->is_founded()) {
    send_to_char("^YThis group is provisional, and needs to be founded with PGROUP FOUND before it is fully operational.^n\r\n", ch);
  } else {
    send_to_char("This group is fully-founded.\r\n", ch);
  }
  
  send_to_char("\r\n", ch);
  
  // If group is secret and member is not privileged, they stop here.
  if (pgr->is_secret() && !GET_PGROUP_MEMBER_DATA(ch)->privileges.AreAnySet(PRIV_COCONSPIRATOR, PRIV_LEADER, ENDBIT)) {
    send_to_char("This group is clandestine, limiting your ability to delve deeper into its information.\r\n", ch);
    return;
  }
  
  // Group is not secret, or member is privileged. Display sensitive information.
  
  // Get membership info.
  sprintf(query_buf, "SELECT COUNT(*) FROM pfiles_playergroups WHERE `group` = %ld", pgr->get_idnum());
  mysql_wrapper(mysql, query_buf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  send_to_char(ch, "This group has %s members.\r\n", row[0]);
  mysql_free_result(res);
  
  // TODO: Information about property (vehicles, apartments, etc)
}

void do_pgroup_transfer(struct char_data *ch, char *argument) {
  // TODO: Log.
  // pgroup  transfer  <vehicle>  <target>   Transfer a vehicle like do_transfer.
  send_to_char("Sorry, the transfer feature is still under construction.", ch);
}

void do_pgroup_vote(struct char_data *ch, char *argument) {
  // PGROUP VOTE to list candidates (assuming a contest is open). PGROUP VOTE FOR <name> to cast/change vote.
  send_to_char("Sorry, the vote feature is still under construction.", ch);
}

void do_pgroup_wire(struct char_data *ch, char *argument) {
  // wire <amount> <target> <reason>  Sends a given amount of nuyen to the target, identifed by PC unique name. Reason must be given.
  
  // Parse out the amount of nuyen the character wishes to wire.
  char *remainder = any_one_arg(argument, buf); // Amount
  unsigned long amount = atol(buf);
  
  // Parse out the target into buf1, and store the reason in remainder.
  remainder = any_one_arg(remainder, buf1);
  skip_spaces(&remainder);
  
  // Must specify an amount greater than zero.
  FAILURE_CASE(amount <= 0, "How much do you wish to wire?");
  
  // Must have enough cash in group bank to cover the transaction.
  FAILURE_CASE(amount > GET_PGROUP(ch)->get_bank(), "The group doesn't have that much nuyen.");
  
  // Must specify a target.
  FAILURE_CASE(!*buf1, "Whom do you want to wire funds to? (Syntax: PGROUP WIRE <amount> <recipient> <reason>)");
  
  // Must specify a reason.
  FAILURE_CASE(!*remainder, "Why are you sending this wire? (Syntax: PGROUP WIRE <amount> <recipient> <reason>)");
  
  if (strlen(remainder) > MAX_PGROUP_LOG_LENGTH - 2) {
    send_to_char(ch, "That reason is too long; you're limited to %d characters.", MAX_PGROUP_LOG_LENGTH - 2);
    return;
  }
  
  struct char_data *vict = NULL;
  long isfile = FALSE;
  if (!(vict = get_player_vis(ch, buf1, FALSE))) {
    if ((isfile = get_player_id(buf1)) == -1) {
      send_to_char("There is no such player.\r\n", ch);
      return;
    }
  }
  
  // Execute the change.
  GET_PGROUP(ch)->set_bank(GET_PGROUP(ch)->get_bank() - amount);
  GET_PGROUP(ch)->save_pgroup_to_db();
  if (isfile) {
    sprintf(buf, "UPDATE pfiles SET Bank=Bank+%lu WHERE idnum=%ld;", amount, isfile);
    mysql_wrapper(mysql, buf);
  } else
    GET_BANK(vict) += amount;
  
  // Mail the recipient.
  sprintf(buf, "'%s' has wired %lu nuyen to your account.\r\n", GET_PGROUP(ch)->get_name(), amount);
  store_mail(vict ? GET_IDNUM(vict) : isfile, ch, buf);
  
  // Log it.
  char *player_name = NULL;
  GET_PGROUP(ch)->audit_log_vfprintf("%s wired %lu nuyen to %s. Reason: %s",
                                     GET_CHAR_NAME(ch),
                                     amount,
                                     vict ? GET_CHAR_NAME(vict) : (player_name = get_player_name(isfile)),
                                     remainder);
  GET_PGROUP(ch)->secret_log_vfprintf("A shadowy figure wired %lu nuyen to someone's account. Reason: %s",
                                      amount,
                                      remainder);
  sprintf(buf, "%s wired %lu nuyen to %s on behalf of '%s'. Reason: %s",
          GET_CHAR_NAME(ch),
          amount,
          vict ? GET_CHAR_NAME(vict) : player_name,
          GET_PGROUP(ch)->get_name(),
          remainder);
  mudlog(buf, ch, LOG_GRIDLOG, TRUE);
  
  // Clean up.
  DELETE_ARRAY_IF_EXTANT(player_name);
  
  send_to_char(ch, "You wire %lu nuyen to %s's account on behalf of '%s'.\r\n",
               amount,
               vict ? GET_CHAR_NAME(vict) : get_player_name(isfile),
               GET_PGROUP(ch)->get_name());
}


/* Help messaging for main pgroup command. Specific help on these commands should be written into helpfiles (eg HELP PGROUP FOUND). */
void display_pgroup_help(struct char_data *ch) {
  int cmds_written = 0;
  sprintf(buf, "You have access to the following PGROUP commands: \r\n");
  
  // If they're not part of a group, the only command they can do is 'create'.
  if (!GET_PGROUP_MEMBER_DATA(ch)) {
    sprintf(ENDOF(buf), " %-11s\r\n", "create");
    send_to_char(buf, ch);
    return;
  }
  
  for (int cmd_index = 0; *(pgroup_commands[cmd_index].cmd) != '\n'; cmd_index++) {
    // If the command is "quit", skip it (it's a hidden alias).
    if (!strcmp(pgroup_commands[cmd_index].cmd, "quit"))
      continue;
    
    // If they don't have the privileges to use the current command, skip it.
    if (!(pgroup_commands[cmd_index].privilege_required == PRIV_NONE
          || (GET_PGROUP_MEMBER_DATA(ch)->privileges.AreAnySet(pgroup_commands[cmd_index].privilege_required, PRIV_LEADER, ENDBIT)))) {
      continue;
    }
    
    // If the group is not yet founded and the command is not valid while not founded, skip it.
    if (!(GET_PGROUP(ch)->is_founded() || pgroup_commands[cmd_index].valid_while_group_not_founded)) {
      continue;
    }
    
    // If the group has already been founded and the command is not valid while founded, skip it.
    if (GET_PGROUP(ch)->is_founded() && !pgroup_commands[cmd_index].valid_while_group_is_founded) {
      continue;
    }
    
    // If the group is disabled and the command is not valid while the group is disabled, skip it.
    if (GET_PGROUP(ch)->is_disabled() && !pgroup_commands[cmd_index].valid_while_group_disabled) {
      continue;
    }
    
    // If the command is 'create', don't show it (that's only valid for people not in groups).
    if (pgroup_commands[cmd_index].command_pointer == do_pgroup_create) {
      continue;
    }
    
    // If we've gotten here, the command is assumed kosher for this character and group combo-- print it in our list.
    sprintf(ENDOF(buf), " %-11s%s", pgroup_commands[cmd_index].cmd, cmds_written++ % 5 == 4 ? "\r\n" : "");
  }
  
  sprintf(ENDOF(buf), "\r\n");
  send_to_char(buf, ch);
}



/* Parser for playergroup editing. */
void pgedit_parse(struct descriptor_data * d, const char *arg) {
  switch (d->edit_mode) {
    case PGEDIT_CONFIRM_EDIT:
      switch (*arg) {
        case 'y':
        case 'Y':
          pgedit_disp_menu(d);
          break;
        case 'n':
        case 'N':
          STATE(d) = CON_PLAYING;
          /* free up the editing object */
          if (d->edit_pgroup)
            delete d->edit_pgroup;
          d->edit_pgroup = NULL;
          PLR_FLAGS(CH).RemoveBit(PLR_EDITING);
          break;
        default:
          send_to_char("That's not a valid choice!\r\n", CH);
          send_to_char("Do you wish to edit it?\r\n", CH);
          break;
      }
      break;
    case PGEDIT_MAIN_MENU:
      switch (*arg) {
        case '1':
          send_to_char("Enter new name: ", CH);
          d->edit_mode = PGEDIT_CHANGE_NAME;
          break;
        case '2':
          send_to_char("Enter new alias: ", CH);
          d->edit_mode = PGEDIT_CHANGE_ALIAS;
          break;
        case '3':
          send_to_char("Enter new tag: ", CH);
          d->edit_mode = PGEDIT_CHANGE_TAG;
          break;
        case '4':
          d->edit_pgroup->set_secret(!d->edit_pgroup->is_secret());
          send_to_char(CH, "OK, the group is %s clandestine.\r\n", d->edit_pgroup->is_secret() ? "now" : "no longer");
          pgedit_disp_menu(d);
          break;
        case 'q':
          if (d->edit_pgroup->is_clone()) {
            // Log the information about who edited it and what the values are.
            GET_PGROUP(CH)->audit_log_vfprintf("%s modified the group's information (name %s, alias %s, tag %s, %s).",
                                               GET_CHAR_NAME(CH),
                                               d->edit_pgroup->get_name(),
                                               d->edit_pgroup->get_alias(),
                                               d->edit_pgroup->get_tag(),
                                               d->edit_pgroup->is_secret() ? "clandestine" : "not clandestine");
            
            GET_PGROUP(CH)->secret_log_vfprintf("A shadowy figure modified the group's information (name %s, alias %s, tag %s, %s).",
                                                d->edit_pgroup->get_name(),
                                                d->edit_pgroup->get_alias(),
                                                d->edit_pgroup->get_tag(),
                                                d->edit_pgroup->is_secret() ? "clandestine" : "not clandestine");
            
            // Group is a clone of an existing group. Update the cloned group.
            GET_PGROUP(CH)->raw_set_name(d->edit_pgroup->get_name());
            GET_PGROUP(CH)->raw_set_alias(d->edit_pgroup->get_alias());
            GET_PGROUP(CH)->raw_set_tag(d->edit_pgroup->get_tag());
            GET_PGROUP(CH)->set_secret(d->edit_pgroup->is_secret());
            GET_PGROUP(CH)->save_pgroup_to_db();
            
            delete d->edit_pgroup;
            d->edit_pgroup = NULL;
            
            send_to_char(CH, "Successfully updated '%s'.\r\n", GET_PGROUP(CH)->get_name());
          } else {
            // Group did not exist previously. Save the group.
            d->edit_pgroup->save_pgroup_to_db();
            
            // Make the character a member of this temporary group.
            if (GET_PGROUP_MEMBER_DATA(CH))
              delete GET_PGROUP_MEMBER_DATA(CH);
            GET_PGROUP_MEMBER_DATA(CH) = new Pgroup_data();
            GET_PGROUP(CH) = d->edit_pgroup;
            GET_PGROUP_MEMBER_DATA(CH)->rank = MAX_PGROUP_RANK;
            GET_PGROUP_MEMBER_DATA(CH)->privileges.SetBit(PRIV_LEADER);
            if (GET_PGROUP_MEMBER_DATA(CH)->title)
              delete GET_PGROUP_MEMBER_DATA(CH)->title;
            GET_PGROUP_MEMBER_DATA(CH)->title = str_dup("Leader");
            
            // Log the information about who created it and what the values are.
            GET_PGROUP(CH)->audit_log_vfprintf("%s created the group (name %s, alias %s, tag %s).",
                                               GET_CHAR_NAME(CH),
                                               d->edit_pgroup->get_name(),
                                               d->edit_pgroup->get_alias(),
                                               d->edit_pgroup->get_tag());
            
            GET_PGROUP(CH)->secret_log_vfprintf("A shadowy figure created the group (name %s, alias %s, tag %s).",
                                                d->edit_pgroup->get_name(),
                                                d->edit_pgroup->get_alias(),
                                                d->edit_pgroup->get_tag());
            
            send_to_char(CH, "Created new group '%s'.\r\n", GET_PGROUP(CH)->get_name());
            
            // Save the character to ensure the DB reflects their new membership.
            playerDB.SaveChar(CH, GET_LOADROOM(CH));
          }
          
          // Return character to game.
          STATE(d) = CON_PLAYING;
          d->edit_pgroup = NULL;
          PLR_FLAGS(CH).RemoveBit(PLR_EDITING);
          break;
        case 'x':
          send_to_char("OK.\r\n", CH);
          STATE(d) = CON_PLAYING;
          /* free up the editing object */
          if (d->edit_pgroup)
            delete d->edit_pgroup;
          d->edit_pgroup = NULL;
          PLR_FLAGS(CH).RemoveBit(PLR_EDITING);
          break;
        default:
          send_to_char("That's not a valid choice.\r\n", CH);
          pgedit_disp_menu(d);
          break;
      }
      break;
    case PGEDIT_CHANGE_NAME:
      // TODO: Don't allow duplicate names.
      
      d->edit_pgroup->set_name(arg, CH);
      pgedit_disp_menu(d);
      break;
    case PGEDIT_CHANGE_ALIAS:
      // TODO: Don't allow duplicate aliases.
      
      d->edit_pgroup->set_alias(arg, CH);
      pgedit_disp_menu(d);
      break;
    case PGEDIT_CHANGE_TAG:
      // TODO: Don't allow duplicate tags.
      
      d->edit_pgroup->set_tag(arg, CH);
      pgedit_disp_menu(d);
      break;
    default:
      sprintf(buf, "SYSERR: Unknown edit mode %d referenced from pgedit_parse.", d->edit_mode);
      mudlog(buf, CH, LOG_MISCLOG, TRUE);
      log(buf);
      break;
  }
}

/* Main menu for playergroup editing. */
void pgedit_disp_menu(struct descriptor_data *d) {
  CLS(CH);
  send_to_char(CH, "^WPlayer Group:\r\n");
  send_to_char(CH, "^G1^Y) ^WName: ^c%s^n\r\n", d->edit_pgroup->get_name());
  send_to_char(CH, "^G2^Y) ^WAlias: ^c%s^n\r\n", d->edit_pgroup->get_alias());
  send_to_char(CH, "^G3^Y) ^WWho Tag: ^c%s^n\r\n", d->edit_pgroup->get_tag());
  send_to_char(CH, "^G4^Y) ^WClandestine: ^c%s^n\r\n", d->edit_pgroup->is_secret() ? "Yes" : "No");
  send_to_char("^Gq^Y) ^WQuit\r\n^Gx^Y) ^WQuit without saving\r\n\r\n^wEnter selection: ", CH);
  d->edit_mode = PGEDIT_MAIN_MENU;
}

long get_new_pgroup_idnum() {
  char querybuf[MAX_STRING_LENGTH];
  strcpy(querybuf, "SELECT idnum FROM playergroups ORDER BY idnum DESC;");
  mysql_wrapper(mysql, querybuf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row && mysql_field_count(mysql)) {
    // No current groups exist. This will be our first.
    mysql_free_result(res);
    return 1;
  } else {
    // Hand out the highest idnum.
    long new_num = atol(row[0]) + 1;
    mysql_free_result(res);
    return new_num;
  }
}

// Search character's inventory and worn items for an accessible pocket secretary. If found, return true.
bool has_valid_pocket_secretary(struct char_data *ch) {
  SPECIAL(pocket_sec);
  
  // Check worn items.
  for (int i = 0; i < NUM_WEARS; i++) {
    if (GET_EQ(ch, i) && GET_OBJ_SPEC(ch->equipment[i]) == pocket_sec)
      if (GET_OBJ_VAL(ch->equipment[i], 1) == 0 || GET_OBJ_VAL(ch->equipment[i], 1) == GET_IDNUM(ch))
        return TRUE;
  }
  
  // Check carried items.
  for (struct obj_data *i = ch->carrying; i; i = i->next_content) {
    if (GET_OBJ_SPEC(i) == pocket_sec)
      if (GET_OBJ_VAL(i, 1) == 0 || GET_OBJ_VAL(i, 1) == GET_IDNUM(ch))
        return TRUE;
  }
  
  // Nothing we found was kosher-- return false.
  return FALSE;
}

const char *list_privs_char_can_affect(struct char_data *ch) {
  bool is_leader = GET_PGROUP_MEMBER_DATA(ch)->privileges.IsSet(PRIV_LEADER);
  bool is_first = TRUE;
  
  static char privstring_buf[500];
  strcpy(privstring_buf, "");
  
  for (int priv = 0; priv < PRIV_MAX; priv++) {
    // Nobody can hand out the leadership privilege.
    if (priv == PRIV_LEADER)
      continue;
    
    // Leaders can hand out anything; otherwise, only return things the char has (except admin, which is leader-assigned-only).
    if (is_leader || (GET_PGROUP_MEMBER_DATA(ch)->privileges.IsSet(priv) && priv != PRIV_ADMINISTRATOR)) {
      sprintf(ENDOF(privstring_buf), "%s%s", is_first ? "" : ", ", pgroup_privileges[priv]);
      is_first = FALSE;
    }
  }
  
  return privstring_buf;
}

void perform_pgroup_grant_revoke(struct char_data *ch, char *argument, bool revoke) {
  // Since a lot of logic is the same, combined grant and revoke methods.
  
  // Can't overflow the substring buffer if the substring buffer is always the length of your string.
  char name[strlen(argument)+1];
  char privilege[strlen(argument)+1];
  int priv;
  struct char_data *vict = NULL;
  bool vict_is_logged_in = TRUE;
  
  // Parse argument into name and privilege.
  half_chop(argument, name, privilege);
  
  if (!*name || !*privilege) {
    send_to_char(ch, "Syntax: PGROUP %s <character> <privilege>\r\n", revoke ? "REVOKE" : "GRANT");
    send_to_char(ch, "You may %s any of the following privileges: %s\r\n",
                 revoke ? "revoke" : "grant", list_privs_char_can_affect(ch));
    return;
  }
  
  // Find the index number of the requested privilege by comparing the typed name with the privs table.
  for (priv = 0; priv < PRIV_MAX; priv++)
    if (is_abbrev(privilege, pgroup_privileges[priv]))
      break;
  
  // If the privilege requested doesn't match a privilege, fail.
  switch (priv) {
    case PRIV_MAX: // No privilege was found matching the string. Fail.
      send_to_char(ch, "'%s' is not a valid privilege. Privileges you can %s are: %s\r\n",
                   privilege, revoke ? "revoke" : "grant", list_privs_char_can_affect(ch));
      return;
    case PRIV_LEADER: // LEADER cannot be handed out. Fail.
      send_to_char(ch, "Sorry, leadership cannot be %s in this way.\r\n", revoke ? "revoked" : "assigned");
      return;
    case PRIV_ADMINISTRATOR: // ADMINISTRATOR cannot be handed out. Fail.
      if (!(GET_PGROUP_MEMBER_DATA(ch)->privileges.IsSet(PRIV_LEADER))) {
        send_to_char(ch, "Only the leader of the group may %s that privilege.\r\n", revoke ? "revoke" : "grant");
        return;
      }
      break;
  }
  
  // If the invoker does not have the privilege requested, fail.
  if (!(GET_PGROUP_MEMBER_DATA(ch)->privileges.AreAnySet(priv, PRIV_LEADER, ENDBIT))) {
    send_to_char(ch, "You must first be assigned that privilege before you can %s others.\r\n", revoke ? "revoke it from" : "grant it to");
    return;
  }
  
  // Now that we know the privilege is kosher, we can do the more expensive character validity check.
  
  // Search the online characters for someone matching the specified name.
  for (vict = character_list; vict; vict = vict->next) {
    if (!IS_NPC(vict) && (isname(name, GET_KEYWORDS(vict)) || isname(name, GET_CHAR_NAME(vict)) || recog(ch, vict, name)))
      break;
  }
  
  // If they weren't online, attempt to load them from the DB.
  if (!vict) {
    vict_is_logged_in = FALSE;
    if (!(vict = playerDB.LoadChar(name, false))) {
      // We were unable to find them online or load them from DB-- fail out.
      send_to_char("There is no such player.\r\n", ch);
      return;
    }
  }
  
  // Ensure targeted character is part of the same group as the invoking character.
  if (!(GET_PGROUP_MEMBER_DATA(vict) && GET_PGROUP(vict) && GET_PGROUP(vict) == GET_PGROUP(ch))) {
    send_to_char(ch, "%s's not part of your group.\r\n", capitalize(HSSH(vict)));
    return;
  }
  
  // Ensure targeted character is below the invoker's rank.
  if (GET_PGROUP_MEMBER_DATA(vict)->rank >= GET_PGROUP_MEMBER_DATA(ch)->rank) {
    send_to_char(ch, "You can only %s people who are lower-ranked than you.\r\n", revoke ? "revoke privileges from" : "grant privileges to");
    return;
  }
  
  // Revoke mode.
  if (revoke) {
    // Ensure targeted character has this priv.
    if (!(GET_PGROUP_MEMBER_DATA(vict)->privileges.IsSet(priv))) {
      send_to_char(ch, "%s doesn't have that privilege.\r\n", HSSH(vict));
      return;
    }
    
    // Update the character with the removal of privilege requested.
    GET_PGROUP_MEMBER_DATA(vict)->privileges.RemoveBit(priv);
    
    // Write to the log.
    GET_PGROUP(ch)->audit_log_vfprintf("%s revoked the %s privilege from %s.", GET_CHAR_NAME(ch), pgroup_privileges[priv], GET_CHAR_NAME(vict));
    // No secret log for this one ("Someone has revoked X from someone else." is pointless)
    
    // Write to the relevant characters' screens.
    send_to_char(ch, "You revoke from %s the %s privilege in '%s'.\r\n", GET_CHAR_NAME(vict), pgroup_privileges[priv], GET_PGROUP(ch)->get_name());
    sprintf(buf, "Your %s privilege in '%s' has been revoked.\r\n", pgroup_privileges[priv], GET_PGROUP(ch)->get_name());
  }
  
  // Grant mode.
  else {
    // Ensure targeted character does not already have this priv.
    if (GET_PGROUP_MEMBER_DATA(vict)->privileges.IsSet(priv)) {
      send_to_char(ch, "%s already has that privilege.\r\n", HSSH(vict));
      return;
    }
    
    // Update the character with the privilege requested.
    GET_PGROUP_MEMBER_DATA(vict)->privileges.SetBit(priv);
    
    // Write to the log.
    GET_PGROUP(ch)->audit_log_vfprintf("%s granted %s the %s privilege.", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), pgroup_privileges[priv]);
    // No secret log for this one.
    
    // Write to the relevant characters' screens.
    send_to_char(ch, "You grant %s the %s privilege in '%s'.\r\n", GET_CHAR_NAME(vict), pgroup_privileges[priv], GET_PGROUP(ch)->get_name());
    sprintf(buf, "You have been granted the %s privilege in '%s'.\r\n", pgroup_privileges[priv], GET_PGROUP(ch)->get_name());
  }
  
  // Save the character.
  if (vict_is_logged_in) {
    // Online characters are saved to the DB without unloading.
    send_to_char(vict, buf);
    playerDB.SaveChar(vict, GET_LOADROOM(vict));
  } else {
    // Loaded characters are unloaded (saving happens during extract_char).
    store_mail(GET_IDNUM(vict), ch, buf);
    extract_char(vict);
  }
}

void do_pgroup_promote_demote(struct char_data *ch, char *argument, bool promote) {
  char name[strlen(argument)];
  char rank_string[strlen(argument)];
  int rank;
  struct char_data *vict = NULL;
  bool vict_is_logged_in = TRUE;
  
  // Parse argument into name and rank string.
  half_chop(argument, name, rank_string);
  
  // Require name.
  if (!*name) {
    send_to_char(ch, "Syntax: PGROUP %s <character> <rank>\r\n", promote ? "PROMOTE" : "DEMOTE");
    return;
  }
  
  // Bounds check rank.
  if (!(rank = atoi(rank_string)) || rank < (promote ? 2 : 1) || rank > MAX_PGROUP_RANK) {
    send_to_char(ch, "You must specify a rank between %d and %d.\r\n", promote ? 2 : 1, MAX_PGROUP_RANK);
    return;
  }
  
  // Better messaging for promotion failure if you're rank 1.
  if (GET_PGROUP_MEMBER_DATA(ch)->rank == 1) {
    send_to_char(ch, "You're unable to %s anyone due to your own low rank.\r\n", promote ? "promote" : "demote");
    return;
  }
  
  // Precondition: Promotion can't equal or exceed your own rank.
  if (rank >= GET_PGROUP_MEMBER_DATA(ch)->rank) {
    send_to_char(ch, "The highest rank you can %s someone to is %d.\r\n",
                 promote ? "promote" : "demote", GET_PGROUP_MEMBER_DATA(ch)->rank - 1);
    return;
  }
  
  // Search the online characters for someone matching the specified name.
  for (vict = character_list; vict; vict = vict->next) {
    if (!IS_NPC(vict) && (isname(name, GET_KEYWORDS(vict)) || isname(name, GET_CHAR_NAME(vict)) || recog(ch, vict, name)))
      break;
  }
  
  // If they weren't online, attempt to load them from the DB.
  if (!vict) {
    vict_is_logged_in = FALSE;
    if (!(vict = playerDB.LoadChar(name, false))) {
      // We were unable to find them online or load them from DB-- fail out.
      send_to_char("There is no such player.\r\n", ch);
      return;
    }
  }
  
  // Ensure targeted character is part of the same group as the invoking character.
  if (!(GET_PGROUP_MEMBER_DATA(vict) && GET_PGROUP(vict) && GET_PGROUP(vict) == GET_PGROUP(ch))) {
    send_to_char(ch, "%s's not part of your group.\r\n", capitalize(HSSH(vict)));
    return;
  }
  
  // Prevent modification of someone higher than you.
  if (GET_PGROUP_MEMBER_DATA(vict)->rank >= GET_PGROUP_MEMBER_DATA(ch)->rank) {
    send_to_char(ch, "You can't do that to your %s!\r\n",
                 GET_PGROUP_MEMBER_DATA(vict)->rank > GET_PGROUP_MEMBER_DATA(ch)->rank ? "superiors" : "peers");
    return;
  }
  
  // Prevent modification of someone higher than you.
  if (GET_PGROUP_MEMBER_DATA(vict)->rank == rank) {
    send_to_char(ch, "But %s's already that rank.\r\n", HSSH(vict));
    return;
  }
  
  if (promote && GET_PGROUP_MEMBER_DATA(vict)->rank > rank) {
    send_to_char(ch, "That would be a demotion for %s.\r\n", HMHR(vict));
    return;
  }
  
  if (!promote && GET_PGROUP_MEMBER_DATA(vict)->rank < rank) {
    send_to_char(ch, "That would be a promotion for %s.\r\n", HMHR(vict));
    return;
  }
  
  // Set their rank.
  GET_PGROUP_MEMBER_DATA(vict)->rank = rank;
  
  // Log the action.
  GET_PGROUP(ch)->audit_log_vfprintf("%s %s %s to rank %d.", GET_CHAR_NAME(ch), promote ? "promoted" : "demoted",
                                     GET_CHAR_NAME(vict), rank);
  
  GET_PGROUP(ch)->secret_log_vfprintf("Someone has been %s to rank %d.", promote ? "promoted" : "demoted", rank);
  
  // Notify the character.
  send_to_char(ch, "You %s %s to rank %d.\r\n", promote ? "promote" : "demote", GET_CHAR_NAME(vict), rank);
  sprintf(buf, "You have been %s to rank %d in '%s'.\r\n", promote ? "promoted" : "demoted", rank, GET_PGROUP(ch)->get_name());
  
  // Save the character.
  if (vict_is_logged_in) {
    // Online characters are saved to the DB without unloading.
    send_to_char(buf, vict);
    playerDB.SaveChar(vict, GET_LOADROOM(vict));
  } else {
    // Loaded characters are unloaded (saving happens during extract_char).
    store_mail(GET_IDNUM(vict), ch, buf);
    extract_char(vict);
  }
}

const char *pgroup_print_privileges(Bitfield privileges) {
  static char output[500];
  strcpy(output, "");
  
  bool is_first = TRUE;
  for (int priv = PRIV_ADMINISTRATOR; priv < PRIV_MAX; priv++) {
    if (privileges.IsSet(priv)) {
      sprintf(ENDOF(output), "%s%s", is_first ? "" : ", ", pgroup_privileges[priv]);
      if (priv == PRIV_LEADER)
        strcat(output, " (grants all other privileges)");
      is_first = FALSE;
    }
  }
  if (is_first)
    strcpy(output, "(none)");
  return output;
}
