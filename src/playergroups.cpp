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
#include "comm.h"
#include "db.h"
#include "utils.h"
#include "interpreter.h"
#include "constants.h"
#include "playergroups.h"
#include "olc.h"

struct pgroup_cmd_struct {
  const char *cmd;
  dword privilege_required;
  bool valid_while_group_not_founded;
} pgroup_commands[] = {
  { "create" , PRIV_NONE      , TRUE  },
  { "disband", PRIV_LEADER    , TRUE  },
  { "found"  , PRIV_LEADER    , TRUE  },
  { "invite" , PRIV_RECRUITER , TRUE  },
  { "edit"   , PRIV_LEADER    , TRUE  },
  { "\n"     , PRIV_NONE      , FALSE } // This must be last.
};

/* Main Playergroup Command */
ACMD(do_pgroup) {
  char command[MAX_STRING_LENGTH];
  int cmd_index = 0;
  
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
  
  // If the command was invalid, show help and exit.
  if (*(pgroup_commands[cmd_index].cmd) == '\n') {
    display_pgroup_help(ch);
    return;
  }
  
  // Precondition: If the command is not 'create', you must be part of a group.
  if (cmd_index != 0 && !GET_PGROUP_DATA(ch)) {
    send_to_char("You need to be a member of a playergroup to do that.\r\n", ch);
    return;
  }
  
  // Precondition: Your data structures must be correctly initialized.
  if (GET_PGROUP_DATA(ch) && !GET_PGROUP(ch)) {
    sprintf(buf, "SYSERR: Somehow, %s (%ld) is part of an invalid group.",
            GET_CHAR_NAME(ch), GET_IDNUM(ch));
    mudlog(buf, ch, LOG_MISCLOG, TRUE);
    log(buf);
    send_to_char("We're sorry, but your character is currently bugged. Please log out and back in.\r\nIf that doesn't fix the problem, please reach out to the staff for help.\r\n", ch);
    return;
  }
  
  // Precondition: If the command requires a kosherized group, you must be part of one.
  if (!pgroup_commands[cmd_index].valid_while_group_not_founded
      && (!GET_PGROUP_DATA(ch) || !GET_PGROUP(ch)->is_founded())) {
    send_to_char("You need to be a member of a fully-fledged playergroup to do that.\r\n", ch);
    return;
  }
  
  // TODO: Privilege check.
  if (pgroup_commands[cmd_index].privilege_required != PRIV_NONE) {
    if (!(GET_PGROUP_DATA(ch)->privileges.AreAnySet(
          pgroup_commands[cmd_index].privilege_required, PRIV_LEADER, ENDBIT))) {
      sprintf(buf, "You must be a %s within your group to do that.\r\n",
              pgroup_privileges[pgroup_commands[cmd_index].privilege_required]);
      return;
    }
  }
  
  switch (cmd_index) {
    case 0: /* pgroup create */
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
      ch->desc->edit_pgroup->raw_set_whotitle("[EMPTY]");
      ch->desc->edit_pgroup->raw_set_name("An Unimaginative Player Group");
      ch->desc->edit_pgroup->raw_set_alias("unimaginative");
      
      // put character into edit menu like OLC menus
      ch->desc->edit_mode = PGEDIT_MAIN_MENU;
      pgedit_disp_menu(ch->desc);
      break;
    case 1: /* pgroup disband */
      // TODO
      send_to_char("disband", ch);
      break;
    case 2: /* pgroup found */
      // TODO
      send_to_char("found", ch);
      break;
    case 3: /* pgroup invite */
      // TODO
      send_to_char("invite", ch);
      break;
    case 4: /*pgroup edit */
      if (!GET_PGROUP_DATA(ch)) {
        send_to_char("You're not currently part of a group. Did you mean to PGROUP CREATE?", ch);
        return;
      }
      break;
    default:
      sprintf(buf, "SYSERR: Unknown command %d referenced from do_pgroup.", cmd_index);
      mudlog(buf, ch, LOG_MISCLOG, TRUE);
      log(buf);
      break;
  }
}

/* Help messaging for main pgroup command. */
void display_pgroup_help(struct char_data *ch) {
  int cmds_written = 0;
  sprintf(buf, "You must specify a command. Valid commands for you are: \r\n");
  
  // If they're not part of a group, the only command they can do is 'create'.
  if (!GET_PGROUP_DATA(ch)) {
    sprintf(ENDOF(buf), "   create\r\n");
    send_to_char(buf, ch);
    return;
  }
  
  // Iterate over all commands other than create and show them the ones they have privileges for.
  for (int cmd_index = 1; *(pgroup_commands[cmd_index].cmd) != '\n'; cmd_index++) {
    if (pgroup_commands[cmd_index].privilege_required == PRIV_NONE
        || (GET_PGROUP_DATA(ch)->privileges.AreAnySet(
            pgroup_commands[cmd_index].privilege_required, PRIV_LEADER, ENDBIT))) {
      sprintf(ENDOF(buf), "   %s%s", pgroup_commands[cmd_index].cmd, cmds_written++ % 5 == 4 ? "\r\n" : "");
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
          send_to_char("Enter new whotitle: ", CH);
          d->edit_mode = PGEDIT_CHANGE_WHOTITLE;
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
    case PGEDIT_CHANGE_WHOTITLE:
      d->edit_pgroup->set_whotitle(arg, CH);
      pgedit_disp_menu(d);
      break;
    case PGEDIT_CONFIRM_SAVE:
      // Save the group.
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
  send_to_char(CH, "^WPlayer Group: ^c%d^n\r\n", d->edit_number);
  send_to_char(CH, "^G1^Y) ^WName: ^c%s^n\r\n", d->edit_pgroup->get_name());
  send_to_char(CH, "^G2^Y) ^WAlias: ^c%s^n\r\n", d->edit_pgroup->get_alias());
  send_to_char(CH, "^G3^Y) ^WWho Tag: ^c%s^n\r\n", d->edit_pgroup->get_whotitle());
  send_to_char("^Gq^Y) ^WQuit\r\n^Gx^Y) ^WQuit without saving\r\n^wEnter selection: ", CH);
  d->edit_mode = PGEDIT_MAIN_MENU;
}

/* Playergroup Class implementation */

Playergroup::Playergroup() :
  whotitle(NULL), name(NULL), alias(NULL)
{}

Playergroup::~Playergroup() {
  if (whotitle)
    delete whotitle;
  if (name)
    delete name;
  if (alias)
    delete alias;
}

// Performs validity checks before setting. Returns TRUE if set succeeded, FALSE otherwise.
bool Playergroup::set_whotitle(const char *newtitle, struct char_data *ch) {
  // TODO: Validity checks.
  
  raw_set_whotitle(newtitle);
  return TRUE;
}

// Performs validity checks before setting. Returns TRUE if set succeeded, FALSE otherwise.
bool Playergroup::set_name(const char *newname, struct char_data *ch) {
  // TODO: Validity checks.
  
  raw_set_name(newname);
  return TRUE;
}

// Performs validity checks before setting. Returns TRUE if set succeeded, FALSE otherwise.
bool Playergroup::set_alias(const char *newalias, struct char_data *ch) {
  // TODO: Validity checks.
  
  raw_set_alias(newalias);
  return TRUE;
}

// Does not perform validity checks before setting.
void Playergroup::raw_set_whotitle(const char *newtitle) {
  if (whotitle)
    delete whotitle;
  
  whotitle = str_dup(newtitle);
}

// Does not perform validity checks before setting.
void Playergroup::raw_set_name(const char *newname) {
  if (name)
    delete name;
  
  name = str_dup(newname);
}

// Does not perform validity checks before setting.
void Playergroup::raw_set_alias(const char *newalias) {
  if (alias)
    delete alias;
  
  alias = str_dup(newalias);
}

// Saves the playergroup to the database.
void Playergroup::save_pgroup_to_db() {
  // TODO.
}
