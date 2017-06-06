/******************************************************
 * Code for the handling of player groups. Written for *
 * AwakeMUD Community Edition by Lucien Sadi. 06/03/17 *
 ******************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#include "awake.h"
#include "structs.h"
#include "newdb.h"
#include "db.h"
#include "utils.h"
#include "interpreter.h"
#include "constants.h"
#include "playergroups.h"
#include "playergroup_class.h"
#include "olc.h"
#include "handler.h"
#include "comm.h"

// Externs from other files.
extern MYSQL *mysql;

// Prototypes from other files.
char *prepare_quotes(char *dest, const char *str);
int mysql_wrapper(MYSQL *mysql, const char *query);

// The linked list of loaded playergroups.
Playergroup *loaded_playergroups = NULL;

struct pgroup_cmd_struct {
  const char *cmd;
  dword privilege_required;
  void (*command_pointer) (struct char_data *ch, char *argument);
  bool valid_while_group_not_founded;
} pgroup_commands[] = {
  { "abdicate"   , PRIV_LEADER        , do_pgroup_abdicate    , FALSE },
  { "balance"    , PRIV_TREASURER     , do_pgroup_balance     , FALSE },
  { "buy"        , PRIV_PROCURER      , do_pgroup_buy         , FALSE },
  { "contest"    , PRIV_NONE          , do_pgroup_contest     , FALSE },
  { "create"     , PRIV_NONE          , do_pgroup_create      , TRUE  },
  { "demote"     , PRIV_MANAGER       , do_pgroup_promote     , FALSE },
  { "deposit"    , PRIV_NONE          , do_pgroup_deposit     , FALSE },
  { "design"     , PRIV_ARCHITECT     , do_pgroup_design      , FALSE },
  { "disband"    , PRIV_LEADER        , do_pgroup_disband     , TRUE  },
  { "edit"       , PRIV_LEADER        , do_pgroup_edit        , TRUE  },
  { "found"      , PRIV_LEADER        , do_pgroup_found       , TRUE  },
  { "grant"      , PRIV_ADMINISTRATOR , do_pgroup_grant       , FALSE },
  { "help"       , PRIV_NONE          , do_pgroup_help        , TRUE  },
  { "invitations", PRIV_RECRUITER     , do_pgroup_invitations , TRUE  },
  { "invite"     , PRIV_RECRUITER     , do_pgroup_invite      , TRUE  },
  { "lease"      , PRIV_LANDLORD      , do_pgroup_lease       , FALSE },
  { "logs"       , PRIV_AUDITOR       , do_pgroup_logs        , TRUE  },
  { "note"       , PRIV_NONE          , do_pgroup_note        , TRUE  },
  { "outcast"    , PRIV_MANAGER       , do_pgroup_outcast     , TRUE  },
  { "promote"    , PRIV_MANAGER       , do_pgroup_promote     , FALSE },
  { "quit"       , PRIV_NONE          , do_pgroup_resign      , TRUE  },
  { "resign"     , PRIV_NONE          , do_pgroup_resign      , TRUE  },
  { "revoke"     , PRIV_ADMINISTRATOR , do_pgroup_revoke      , FALSE },
  { "roster"     , PRIV_NONE          , do_pgroup_roster      , TRUE  },
  { "status"     , PRIV_NONE          , do_pgroup_status      , TRUE  },
  { "transfer"   , PRIV_PROCURER      , do_pgroup_transfer    , FALSE },
  { "vote"       , PRIV_NONE          , do_pgroup_vote        , FALSE },
  { "withdraw"   , PRIV_TREASURER     , do_pgroup_withdraw    , FALSE },
  { "\n"         , 0                  , 0                     , FALSE } // This must be last.
};

/* Main Playergroup Command */
ACMD(do_pgroup) {
  char command[MAX_STRING_LENGTH];
  int cmd_index = 0;
  
  if (IS_NPC(ch)) {
    send_to_char("NPCs can't participate in PC groups.\r\n", ch);
    return;
  }
  
  half_chop(argument, command, buf);
  
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
  
  // Precondition: If the command is not 'create', you must be part of a group.
  if (pgroup_commands[cmd_index].command_pointer != do_pgroup_create && !GET_PGROUP_DATA(ch)) {
    send_to_char("You need to be a member of a playergroup to do that.\r\n", ch);
    return;
  }
  
  // Precondition: Your data structures must be correctly initialized.
  if (GET_PGROUP_DATA(ch) && !GET_PGROUP(ch)) {
    sprintf(buf, "SYSERR: Somehow, %s (%ld) is part of an invalid group.",
            GET_CHAR_NAME(ch), GET_IDNUM(ch));
    mudlog(buf, ch, LOG_MISCLOG, TRUE);
    log(buf);
    send_to_char("We're sorry, but your character is currently bugged. Please log out and back in.\r\n"
                 "If that doesn't fix the problem, please reach out to the staff for help.\r\n", ch);
    return;
  }
  
  // Precondition: If the command requires a kosherized group, you must be part of one.
  if (!pgroup_commands[cmd_index].valid_while_group_not_founded
      && (!GET_PGROUP_DATA(ch) || !GET_PGROUP(ch)->is_founded())) {
    send_to_char("You need to be a member of a fully-fledged playergroup to do that.\r\n", ch);
    return;
  }
  
  // Precondition: You must have the appropriate privilege to perform this command.
  if (pgroup_commands[cmd_index].privilege_required != PRIV_NONE) {
    if (!(GET_PGROUP_DATA(ch)->privileges.AreAnySet(pgroup_commands[cmd_index].privilege_required, PRIV_LEADER, ENDBIT))) {
      sprintf(buf, "You must be a %s within your group to do that.\r\n",
              pgroup_privileges[pgroup_commands[cmd_index].privilege_required]);
      return;
    }
  }
  
  // Execute the subcommand.
  ((*pgroup_commands[cmd_index].command_pointer) (ch, buf));
}

void do_pgroup_abdicate(struct char_data *ch, char *argument) {
  send_to_char("", ch);
}

void do_pgroup_balance(struct char_data *ch, char *argument) {
  send_to_char("", ch);
}

void do_pgroup_buy(struct char_data *ch, char *argument) {
  send_to_char("", ch);
}

void do_pgroup_contest(struct char_data *ch, char *argument) {
  send_to_char("", ch);
}

void do_pgroup_create(struct char_data *ch, char *argument) {
  // If the player is already in a group, they can't create a new one.
  if (GET_PGROUP_DATA(ch)) {
    send_to_char("You are already part of a playergroup. You'll need to leave it first.\r\n", ch);
    return;
  }
  
  // Set char editing. This does not remove the character from the game world.
  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  STATE(ch->desc) = CON_PGEDIT;
  
  // Create the new pgroup and add it to the player's descriptor's edit pointer.
  ch->desc->edit_pgroup = new Playergroup();
  ch->desc->edit_pgroup->set_idnum(0);
  ch->desc->edit_pgroup->raw_set_tag("^R[^WEMPTY^R]^n");
  ch->desc->edit_pgroup->raw_set_name("An Unimaginative Player Group");
  ch->desc->edit_pgroup->raw_set_alias("newgroup");
  
  // Put character into the playergroup edit menu.
  ch->desc->edit_mode = PGEDIT_MAIN_MENU;
  pgedit_disp_menu(ch->desc);
}

void do_pgroup_deposit(struct char_data *ch, char *argument) {
  send_to_char("deposit", ch);
}

void do_pgroup_design(struct char_data *ch, char *argument) {
  send_to_char("design", ch);
}

void do_pgroup_disband(struct char_data *ch, char *argument) {
  send_to_char("disband", ch);
}

void do_pgroup_edit(struct char_data *ch, char *argument) {
  // Create a clone from the player's current group.
  ch->desc->edit_pgroup = new Playergroup(GET_PGROUP(ch));
  
  // Put character into the playergroup edit menu.
  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  STATE(ch->desc) = CON_PGEDIT;
  ch->desc->edit_mode = PGEDIT_MAIN_MENU;
  pgedit_disp_menu(ch->desc);
}

void do_pgroup_found(struct char_data *ch, char *argument) {
  send_to_char("found", ch);
}

void do_pgroup_grant(struct char_data *ch, char *argument) {
  send_to_char("grant", ch);
}

void do_pgroup_help(struct char_data *ch, char *argument) {
  send_to_char("help", ch);
}

void do_pgroup_invitations(struct char_data *ch, char *argument) {
  send_to_char("invitations", ch);
}

void do_pgroup_invite(struct char_data *ch, char *argument) {
  GET_PGROUP(ch)->invite(ch, argument);
}

void do_pgroup_lease(struct char_data *ch, char *argument) {
  send_to_char("lease", ch);
}

void do_pgroup_logs(struct char_data *ch, char *argument) {
  send_to_char("logs", ch);
}

void do_pgroup_note(struct char_data *ch, char *argument) {
  send_to_char("notes", ch);
}

void do_pgroup_outcast(struct char_data *ch, char *argument) {
  send_to_char("outcast", ch);
}

void do_pgroup_promote(struct char_data *ch, char *argument) {
  send_to_char("promote", ch);
}

void do_pgroup_resign(struct char_data *ch, char *argument) {
  send_to_char("resign", ch);
}

void do_pgroup_revoke(struct char_data *ch, char *argument) {
  send_to_char("revoke", ch);
}

void do_pgroup_roster(struct char_data *ch, char *argument) {
  send_to_char("roster", ch);
}

void do_pgroup_status(struct char_data *ch, char *argument) {
  send_to_char("status", ch);
}

void do_pgroup_transfer(struct char_data *ch, char *argument) {
  send_to_char("transfer", ch);
}

void do_pgroup_vote(struct char_data *ch, char *argument) {
  send_to_char("vote", ch);
}

void do_pgroup_withdraw(struct char_data *ch, char *argument) {
  send_to_char("withdraw", ch);
}


/* Help messaging for main pgroup command. */
void display_pgroup_help(struct char_data *ch) {
  int cmds_written = 0;
  sprintf(buf, "Valid PGROUP commands for you are: \r\n");
  
  // If they're not part of a group, the only command they can do is 'create'.
  if (!GET_PGROUP_DATA(ch)) {
    sprintf(ENDOF(buf), " %-11s\r\n", "create");
    send_to_char(buf, ch);
    return;
  }
  
  // Iterate over all commands other than create and show them the ones they have privileges for.
  for (int cmd_index = 1; *(pgroup_commands[cmd_index].cmd) != '\n'; cmd_index++) {
    if (pgroup_commands[cmd_index].privilege_required == PRIV_NONE
        || (GET_PGROUP_DATA(ch)->privileges.AreAnySet(
                                                      pgroup_commands[cmd_index].privilege_required, PRIV_LEADER, ENDBIT))) {
      sprintf(ENDOF(buf), " %-11s%s", pgroup_commands[cmd_index].cmd, cmds_written++ % 5 == 4 ? "\r\n" : "");
    }
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
        case 'q':
          send_to_char("Would you like to save your changes?\r\n", CH);
          d->edit_mode = PGEDIT_CONFIRM_SAVE;
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
      d->edit_pgroup->set_name(arg, CH);
      pgedit_disp_menu(d);
      break;
    case PGEDIT_CHANGE_ALIAS:
      d->edit_pgroup->set_alias(arg, CH);
      pgedit_disp_menu(d);
      break;
    case PGEDIT_CHANGE_TAG:
      d->edit_pgroup->set_tag(arg, CH);
      pgedit_disp_menu(d);
      break;
    case PGEDIT_CONFIRM_SAVE:
      if (d->edit_pgroup->is_clone()) {
        // Group is a clone of an existing group. Update the cloned group.
        GET_PGROUP(CH)->raw_set_name(d->edit_pgroup->get_name());
        GET_PGROUP(CH)->raw_set_alias(d->edit_pgroup->get_alias());
        GET_PGROUP(CH)->raw_set_tag(d->edit_pgroup->get_tag());
        GET_PGROUP(CH)->save_pgroup_to_db();
        
        delete d->edit_pgroup;
      } else {
        // Group did not exist previously. Save the group.
        d->edit_pgroup->save_pgroup_to_db();
        
        // Make the character a member of this temporary group.
        if (GET_PGROUP_DATA(CH))
          delete GET_PGROUP_DATA(CH);
        GET_PGROUP_DATA(CH) = new pgroup_data;
        GET_PGROUP_DATA(CH)->pgroup = d->edit_pgroup;
        GET_PGROUP_DATA(CH)->rank = MAX_PGROUP_RANK;
        GET_PGROUP_DATA(CH)->privileges.SetBit(PRIV_LEADER);
        if (GET_PGROUP_DATA(CH)->title)
          delete GET_PGROUP_DATA(CH)->title;
        GET_PGROUP_DATA(CH)->title = str_dup("Leader");
      }
      send_to_char("OK.\r\n", CH);
      
      // Return character to game.
      STATE(d) = CON_PLAYING;
      d->edit_pgroup = NULL;
      PLR_FLAGS(CH).RemoveBit(PLR_EDITING);
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
  send_to_char("^Gq^Y) ^WQuit\r\n^Gx^Y) ^WQuit without saving\r\n^wEnter selection: ", CH);
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
