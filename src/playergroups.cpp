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
  bool valid_while_group_not_founded;
} pgroup_commands[] = {
  { "create" , PRIV_NONE      , TRUE  },
  { "disband", PRIV_LEADER    , TRUE  },
  { "found"  , PRIV_LEADER    , TRUE  },
  { "invite" , PRIV_RECRUITER , TRUE  },
  { "edit"   , PRIV_LEADER    , TRUE  },
  { "roster" , PRIV_NONE      , TRUE  },
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
      ch->desc->edit_pgroup->set_idnum(0);
      ch->desc->edit_pgroup->raw_set_tag("^R[^WEMPTY^R]^n");
      ch->desc->edit_pgroup->raw_set_name("An Unimaginative Player Group");
      ch->desc->edit_pgroup->raw_set_alias("newgroup");
      
      // Put character into the playergroup edit menu.
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
    case 4: /* pgroup edit */
      if (!GET_PGROUP_DATA(ch)) {
        send_to_char("You're not currently part of a group. Did you mean to PGROUP CREATE?", ch);
        return;
      }
      // Create a clone from the player's current group.
      ch->desc->edit_pgroup = new Playergroup(GET_PGROUP(ch));
      
      // Put character into the playergroup edit menu.
      PLR_FLAGS(ch).SetBit(PLR_EDITING);
      STATE(ch->desc) = CON_PGEDIT;
      ch->desc->edit_mode = PGEDIT_MAIN_MENU;
      pgedit_disp_menu(ch->desc);
      break;
    case 5: /* pgroup roster */
      // TODO: If group has secret membership, require PRIV_COCONSPIRATOR.
      // TODO: Display list of current members and their last login times.
      send_to_char("roster", ch);
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
  send_to_char(CH, "^WPlayer Group: ^c%d^n\r\n", d->edit_number);
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

/* Playergroup Class implementation */

Playergroup::Playergroup() :
idnum(0), tag(NULL), name(NULL), alias(NULL)
{}

Playergroup::Playergroup(long id_to_load) :
tag(NULL), name(NULL), alias(NULL)
{
  load_pgroup_from_db(id_to_load);
}

Playergroup::Playergroup(Playergroup *clone_strings_from) :
tag(NULL), name(NULL), alias(NULL)
{
  raw_set_tag(clone_strings_from->tag);
  raw_set_name(clone_strings_from->name);
  raw_set_alias(clone_strings_from->alias);
  settings.SetBit(PGROUP_CLONE);
}

Playergroup::~Playergroup() {
  if (tag)
    delete tag;
  if (name)
    delete name;
  if (alias)
    delete alias;
}

// Control whether or not the group is officially founded.
void Playergroup::set_founded(bool founded) {
  if (founded)
    settings.SetBit(PGROUP_FOUNDED);
  else
    settings.RemoveBit(PGROUP_FOUNDED);
}

// Control whether or not the group is disabled.
void Playergroup::set_disabled(bool disabled) {
  if (disabled)
    settings.SetBit(PGROUP_DISABLED);
  else
    settings.RemoveBit(PGROUP_DISABLED);
}

// Control whether or not the group's membership list is hidden.
void Playergroup::set_membership_secret(bool secret) {
  if (secret)
    settings.SetBit(PGROUP_SECRETSQUIRREL);
  else
    settings.RemoveBit(PGROUP_SECRETSQUIRREL);
}

// Performs validity checks before setting. Returns TRUE if set succeeded, FALSE otherwise.
bool Playergroup::set_tag(const char *newtag, struct char_data *ch) {
  // TODO: Validity checks.
  if (strlen(newtag) > (MAX_PGROUP_TAG_LENGTH - 1)) {
    sprintf(buf, "Sorry, tags can't be longer than %d characters.\r\n", MAX_PGROUP_TAG_LENGTH - 1);
    send_to_char(buf, ch);
    return FALSE;
  }
  
  raw_set_tag(newtag);
  return TRUE;
}

// Performs validity checks before setting. Returns TRUE if set succeeded, FALSE otherwise.
bool Playergroup::set_name(const char *newname, struct char_data *ch) {
  // Check for length.
  if (strlen(newname) > (MAX_PGROUP_NAME_LENGTH - 1)) {
    sprintf(buf, "Sorry, playergroup names can't be longer than %d characters.\r\n", MAX_PGROUP_NAME_LENGTH - 1);
    send_to_char(buf, ch);
    return FALSE;
  }
  
  raw_set_name(newname);
  return TRUE;
}

// Performs validity checks before setting. Returns TRUE if set succeeded, FALSE otherwise.
bool Playergroup::set_alias(const char *newalias, struct char_data *ch) {
  // Check for length.
  if (strlen(newalias) > (MAX_PGROUP_ALIAS_LENGTH - 1)) {
    sprintf(buf, "Sorry, aliases can't be longer than %d characters.\r\n", MAX_PGROUP_ALIAS_LENGTH - 1);
    send_to_char(buf, ch);
    return FALSE;
  }
  
  // Check for contents: must be lower-case letters.
  const char *ptr = newalias;
  while (*ptr) {
    if (!isalpha(*ptr) || *ptr != tolower(*ptr)) {
      send_to_char("Sorry, aliases can only contain lower-case letters (no color codes, punctuation, etc).\r\n", ch);
      return FALSE;
    } else {
      ptr++;
    }
  }
  
  // Check for duplicates.
  char querybuf[MAX_STRING_LENGTH];
  sprintf(querybuf, "SELECT idnum FROM playergroups WHERE alias = '%s'", prepare_quotes(buf, newalias));
  mysql_wrapper(mysql, querybuf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row && mysql_field_count(mysql)) {
    raw_set_alias(newalias);
    mysql_free_result(res);
    return TRUE;
  } else {
    send_to_char("Sorry, that alias has already been taken by another group.\r\n", ch);
    mysql_free_result(res);
    return FALSE;
  }
  
  // TODO: Race condition. Two people in the edit buf at the same time can use the same alias.
}

// Does not perform validity checks before setting.
void Playergroup::raw_set_tag(const char *newtag) {
  if (tag)
    delete tag;
  
  // If you used raw_set and didn't follow the rules, enjoy your halt.
  assert(strlen(newtag) <= MAX_PGROUP_TAG_LENGTH - 1);
  
  tag = str_dup(newtag);
}

// Does not perform validity checks before setting.
void Playergroup::raw_set_name(const char *newname) {
  if (name)
    delete name;
  
  // If you used raw_set and didn't follow the rules, enjoy your halt.
  assert(strlen(newname) <= MAX_PGROUP_NAME_LENGTH - 1);
  
  name = str_dup(newname);
}

// Does not perform validity checks before setting.
void Playergroup::raw_set_alias(const char *newalias) {
  if (alias)
    delete alias;
  
  // If you used raw_set and didn't follow the rules, enjoy your halt.
  assert(strlen(newalias) <= MAX_PGROUP_ALIAS_LENGTH - 1);
  
  alias = str_dup(newalias);
}

// Saves the playergroup to the database.
bool Playergroup::save_pgroup_to_db() {
  char querybuf[MAX_STRING_LENGTH];
  char quotedname[MAX_PGROUP_NAME_LENGTH * 2];
  char quotedalias[MAX_PGROUP_ALIAS_LENGTH * 2];
  char quotedtag[MAX_PGROUP_TAG_LENGTH * 2];
  char quotedsettings[settings.TotalWidth()];
  
  const char * pgroup_save_query_format =
  "INSERT INTO playergroups (idnum, Name, Alias, Tag, Settings) VALUES ('%ld', '%s', '%s', '%s', '%s')"
  " ON DUPLICATE KEY UPDATE"
  "   Name = VALUES(Name),"
  "   Alias = VALUES(Alias),"
  "   Tag = VALUES(Tag),"
  "   Settings = VALUES(Settings)";
  
  if (!idnum) {
    // We've never saved this group before. Give it a new idnum.
    set_idnum(get_new_pgroup_idnum());
  }
  
  sprintf(querybuf, pgroup_save_query_format,
          idnum,
          prepare_quotes(quotedname, name),
          prepare_quotes(quotedalias, alias),
          prepare_quotes(quotedtag, tag),
          prepare_quotes(quotedsettings, settings.ToString()));
  mysql_wrapper(mysql, querybuf);
  
  return mysql_errno(mysql) != 0;
}

// Loads the playergroup from the database. Returns TRUE on successful load, FALSE otherwise.
bool Playergroup::load_pgroup_from_db(long load_idnum) {
  char querybuf[MAX_STRING_LENGTH];
  const char * pgroup_load_query_format = "SELECT * FROM playergroups WHERE idnum = %ld";
  
  sprintf(querybuf, pgroup_load_query_format, load_idnum);
  mysql_wrapper(mysql, querybuf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (row) {
    set_idnum(atol(row[0]));
    raw_set_name(row[1]);
    raw_set_alias(row[2]);
    raw_set_tag(row[3]);
    settings.FromString(row[4]);
    mysql_free_result(res);
    
    // Add the group to the linked list.
    next_pgroup = loaded_playergroups;
    loaded_playergroups = this;
    
    return TRUE;
  } else {
    sprintf(buf, "Error loading playergroup from DB-- group %ld does not seem to exist.", load_idnum);
    log(buf);
    mysql_free_result(res);
    return FALSE;
  }
}
