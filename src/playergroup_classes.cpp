/******************************************************
 * Code for the handling of player groups. Written for *
 * AwakeMUD Community Edition by Lucien Sadi. 06/03/17 *
 ******************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "awake.h"
#include "structs.h"
#include "newdb.h"
#include "db.h"
#include "utils.h"
#include "interpreter.h"
#include "constants.h"
#include "playergroups.h"
#include "olc.h"
#include "handler.h"
#include "comm.h"

// The linked list of loaded playergroups.
extern Playergroup *loaded_playergroups;

extern void raw_store_mail(long to, long from_id, const char *from_name, const char *message_pointer);

/************* Constructors *************/
Playergroup::Playergroup() :
idnum(0), bank(0), tag(NULL), name(NULL), alias(NULL)
{}

Playergroup::Playergroup(long id_to_load) :
idnum(0), bank(0), tag(NULL), name(NULL), alias(NULL)
{
  load_pgroup_from_db(id_to_load);
}

Playergroup::Playergroup(Playergroup *clone_strings_from) :
idnum(0), bank(0), tag(NULL), name(NULL), alias(NULL)
{
  raw_set_tag(clone_strings_from->tag);
  raw_set_name(clone_strings_from->name);
  raw_set_alias(clone_strings_from->alias);
  set_secret(clone_strings_from->is_secret());
  settings.SetBit(PGROUP_CLONE);
}

/************* Destructor *************/
Playergroup::~Playergroup() {
  if (tag)
    delete [] tag;
  if (name)
    delete [] name;
  if (alias)
    delete [] alias;
}

/************* Getters *************/
// Almost all of these are declared inline in playergroup_class.h.

/************* Setters *************/
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
void Playergroup::set_secret(bool secret) {
  if (secret)
    settings.SetBit(PGROUP_SECRETSQUIRREL);
  else
    settings.RemoveBit(PGROUP_SECRETSQUIRREL);
}

// Determines if a letter belongs to a color code.
bool iscolor(char c) {
  return (c == 'l' || c == 'r' || c == 'g' || c == 'y' || c == 'b' || c == 'm' || c == 'n' || c == 'c'
          || c == 'w' || c == 'L' || c == 'R' || c == 'G' || c == 'Y' || c == 'B' || c == 'M' || c == 'N'
          || c == 'C' || c == 'W');
}

/************* Setters w/ Validity Checks *************/
// Performs validity checks before setting. Returns TRUE if set succeeded, FALSE otherwise.
bool Playergroup::set_tag(const char *newtag, struct char_data *ch) {
  if (strlen(newtag) > MAX_PGROUP_TAG_LENGTH) {
    send_to_char(ch, "Sorry, tag strings can't be longer than %d characters.\r\n", MAX_PGROUP_TAG_LENGTH);
    return FALSE;
  }
  
  int len = get_string_length_after_color_code_removal(newtag, ch);
  
  // Silent failure: We already sent the error message in get_string_length_after_color_code_removal().
  if (len == -1)
    return FALSE;
  
  if (len < 1) {
    send_to_char("Tags must contain at least one printable character.\r\n", ch);
    return FALSE;
  }
  
  if (len > MAX_PGROUP_TAG_LENGTH_WITHOUT_COLOR) {
    send_to_char(ch, "Sorry, the non-color-code parts of tags can't be longer than %d characters.\r\n",
                 MAX_PGROUP_TAG_LENGTH_WITHOUT_COLOR);
    return FALSE;
  }
  
  raw_set_tag(newtag);
  return TRUE;
}

// Performs validity checks before setting. Returns TRUE if set succeeded, FALSE otherwise.
bool Playergroup::set_name(const char *newname, struct char_data *ch) {
  // Check for length.
  if (strlen(newname) > MAX_PGROUP_NAME_LENGTH) {
    send_to_char(ch, "Sorry, playergroup names can't be longer than %d characters.\r\n", MAX_PGROUP_NAME_LENGTH);
    return FALSE;
  }
  
  raw_set_name(newname);
  return TRUE;
}

// Performs validity checks before setting. Returns TRUE if set succeeded, FALSE otherwise.
bool Playergroup::set_alias(const char *newalias, struct char_data *ch) {
  // Check for length.
  if (strlen(newalias) > MAX_PGROUP_ALIAS_LENGTH) {
    send_to_char(ch, "Sorry, aliases can't be longer than %d characters.\r\n", MAX_PGROUP_ALIAS_LENGTH);
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
  if (!alias_is_in_use(newalias)) {
    raw_set_alias(newalias);
    return TRUE;
  } else {
    send_to_char("Sorry, that alias has already been taken by another group.\r\n", ch);
    return FALSE;
  }
  
  // TODO: Race condition. Two people in the edit buf at the same time can use the same alias.
}

bool Playergroup::alias_is_in_use(const char *alias) {
  // Declare our local vars-- in this case, something to stick alias in, and something to hold our query.
  char local_alias[100];
  char querybuf[1000];
  
  // Clone over alias to the new local version. This way we can state conclusively the size of the buffer that contains it-- crucial for prepare_quotes.
  strcpy(local_alias, alias);
  
  // Compose and execute our query.
  snprintf(querybuf, sizeof(querybuf), "SELECT idnum FROM playergroups WHERE alias = '%s'", prepare_quotes(buf, alias, sizeof(local_alias) / sizeof(local_alias[0])));
  mysql_wrapper(mysql, querybuf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row;
  
  // We default to returning TRUE (it's in use).
  bool retval = TRUE;
  
  // If the database has stated that it's not in use, we revise the return value to FALSE.
  if (!res || (!(row = mysql_fetch_row(res)) && mysql_field_count(mysql)))
    retval = FALSE;
  
  // Clean up our database state and return the value.
  mysql_free_result(res);
  return retval;
}

/************* Setters w/ Bypassed Validity Checks *************/
// Does not perform validity checks before setting.
void Playergroup::raw_set_tag(const char *newtag) {
  if (tag)
    delete [] tag;
  
  // If you used raw_set and didn't follow the rules, enjoy your halt.
  assert(strlen(newtag) <= MAX_PGROUP_TAG_LENGTH);
  
  tag = str_dup(newtag);
}

// Does not perform validity checks before setting.
void Playergroup::raw_set_name(const char *newname) {
  if (name)
    delete [] name;
  
  // If you used raw_set and didn't follow the rules, enjoy your halt.
  assert(strlen(newname) <= MAX_PGROUP_NAME_LENGTH);
  
  name = str_dup(newname);
}

// Does not perform validity checks before setting.
void Playergroup::raw_set_alias(const char *newalias) {
  if (alias)
    delete [] alias;
  
  // If you used raw_set and didn't follow the rules, enjoy your halt.
  assert(strlen(newalias) <= MAX_PGROUP_ALIAS_LENGTH);
  
  alias = str_dup(newalias);
}

// Raw setter for bank values (no validity checking).
void Playergroup::set_bank(unsigned long amount) {
  bank = amount;
}

/************* Misc Methods *************/
void Playergroup::audit_log(const char *original_msg, bool is_redacted) {
  char msg[MAX_STRING_LENGTH];
  char query_buf[MAX_STRING_LENGTH];
  
  // Prepend the timestamp to the message.
  time_t ct = time(0);
  char *tmstr = asctime(localtime(&ct));
  *(tmstr + strlen(tmstr) - 1) = '\0';
  snprintf(msg, sizeof(buf), "%-15.15s :: %s", tmstr + 4, original_msg);
  
  // Quote the message for some pretense of DB safety.
  char quoted_msg[strlen(msg) * 2 + 1];
  prepare_quotes(quoted_msg, msg, sizeof(quoted_msg) / sizeof(quoted_msg[0]));
  
  if (is_redacted) {
    // Format the query and execute it.
    const char *query_fmt = "INSERT INTO pgroup_logs (`idnum`, `message`, `date`, `redacted`) VALUES ('%ld', '%s', NOW(), TRUE)";
    snprintf(query_buf, sizeof(buf), query_fmt, get_idnum(), quoted_msg);
    mysql_wrapper(mysql, query_buf);
    
    // Don't log it-- we only want the unredacted one sent to imms.
  } else {
    // Format the query and execute it.
    const char *query_fmt = "INSERT INTO pgroup_logs (`idnum`, `message`, `date`, `redacted`) VALUES ('%ld', '%s', NOW(), FALSE)";
    snprintf(query_buf, sizeof(buf), query_fmt, get_idnum(), quoted_msg);
    mysql_wrapper(mysql, query_buf);
  
  // Reuse the query buf to format the message for MUD-level logging.
    snprintf(query_buf, sizeof(buf), "[%s (%ld)]: %s", get_alias(), get_idnum(), msg);
    mudlog(query_buf, NULL, LOG_PGROUPLOG, TRUE);
  }
}

void Playergroup::secret_log(const char *original_msg) {
  audit_log(original_msg, TRUE);
}

void Playergroup::audit_log_vfprintf(const char *format, ...)
{
  char playergroup_log_buf[MAX_STRING_LENGTH];
  va_list args;
  
  va_start(args, format);
  vsnprintf(playergroup_log_buf, sizeof(playergroup_log_buf), format, args);
  va_end(args);
  
  audit_log(playergroup_log_buf, FALSE);
}

void Playergroup::secret_log_vfprintf(const char *format, ...)
{
  // The expectation is that anything you feed to this log is redacted.
  char playergroup_log_buf[MAX_STRING_LENGTH];
  va_list args;
  
  va_start(args, format);
  vsnprintf(playergroup_log_buf, sizeof(playergroup_log_buf), format, args);
  va_end(args);
  
  audit_log(playergroup_log_buf, TRUE);
}

const char *Playergroup::render_settings() {
  static char settings_string[100];
  strcpy(settings_string, "");
  bool is_first = TRUE;
  
  for (int index = 0; index < NUM_PGROUP_SETTINGS; index++) {
    if (index != PGROUP_CLONE && settings.IsSet(index)) {
      int settings_len = strlen(settings_string);
      snprintf(settings_string + settings_len, 100 - settings_len, "%s%s", is_first ? "" : ", ", pgroup_settings[index]);
    }
  }
  if (is_first)
    strcpy(settings_string, "(none)");
  
  return settings_string;
}

// Saves the playergroup to the database.
bool Playergroup::save_pgroup_to_db() {
  char querybuf[MAX_STRING_LENGTH];
  char quotedname[MAX_PGROUP_NAME_LENGTH * 2 + 1];
  char quotedalias[MAX_PGROUP_ALIAS_LENGTH * 2 + 1];
  char quotedtag[MAX_PGROUP_TAG_LENGTH * 2 + 1];
  char quotedsettings[settings.TotalWidth()];
  
  const char * pgroup_save_query_format =
  "INSERT INTO playergroups (idnum, Name, Alias, Tag, Settings, bank) VALUES ('%ld', '%s', '%s', '%s', '%s', '%lu')"
  " ON DUPLICATE KEY UPDATE"
  "   Name = VALUES(Name),"
  "   Alias = VALUES(Alias),"
  "   Tag = VALUES(Tag),"
  "   Settings = VALUES(Settings),"
  "   bank = VALUES(bank)";
  
  if (!idnum) {
    // We've never saved this group before. Give it a new idnum.
    set_idnum(get_new_pgroup_idnum());
  }
  
  snprintf(querybuf, sizeof(querybuf), pgroup_save_query_format,
          idnum,
          prepare_quotes(quotedname, name, sizeof(quotedname) / sizeof(quotedname[0])),
          prepare_quotes(quotedalias, alias, sizeof(quotedalias) / sizeof(quotedalias[0])),
          prepare_quotes(quotedtag, tag, sizeof(quotedtag) / sizeof(quotedtag[0])),
          prepare_quotes(quotedsettings, settings.ToString(), sizeof(quotedsettings) / sizeof(quotedsettings[0])),
          bank);
  mysql_wrapper(mysql, querybuf);
  
  return mysql_errno(mysql) != 0;
}

// Loads the playergroup from the database. Returns TRUE on successful load, FALSE otherwise.
bool Playergroup::load_pgroup_from_db(long load_idnum) {
  char querybuf[MAX_STRING_LENGTH];
  const char * pgroup_load_query_format = "SELECT * FROM playergroups WHERE idnum = %ld";
  
  // Defines for the purposes of avoiding magic numbers.
#define PGROUP_DB_ROW_IDNUM    0
#define PGROUP_DB_ROW_NAME     1
#define PGROUP_DB_ROW_ALIAS    2
#define PGROUP_DB_ROW_TAG      3
#define PGROUP_DB_ROW_SETTINGS 4
#define PGROUP_DB_ROW_BANK     5
  
  snprintf(querybuf, sizeof(querybuf), pgroup_load_query_format, load_idnum);
  mysql_wrapper(mysql, querybuf);
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row;
  if (res && (row = mysql_fetch_row(res))) {
    set_idnum(atol(row[PGROUP_DB_ROW_IDNUM]));
    set_bank(strtoul(row[PGROUP_DB_ROW_BANK], NULL, 0));
    raw_set_name(row[PGROUP_DB_ROW_NAME]);
    raw_set_alias(row[PGROUP_DB_ROW_ALIAS]);
    raw_set_tag(row[PGROUP_DB_ROW_TAG]);
    settings.FromString(row[PGROUP_DB_ROW_SETTINGS]);
    mysql_free_result(res);
    
    // Add the group to the linked list.
    next_pgroup = loaded_playergroups;
    loaded_playergroups = this;
    
    return TRUE;
  } else {
    snprintf(buf, MAX_STRING_LENGTH, "Error loading playergroup from DB-- group %ld does not seem to exist.", load_idnum);
    log(buf);
    mysql_free_result(res);
    return FALSE;
  }
}

int Playergroup::sql_count_members() {
  char query_buf[MAX_STRING_LENGTH];
  snprintf(query_buf, MAX_STRING_LENGTH, "SELECT count(idnum) FROM `pfiles_playergroups` WHERE `group` = %ld", get_idnum());
  mysql_wrapper(mysql, query_buf);
  MYSQL_RES *res = mysql_use_result(mysql);
  if (res) {
    MYSQL_ROW row = mysql_fetch_row(res);
    int count = atoi(row[0]);
    mysql_free_result(res);
    return count;
  } else {
    log_vfprintf("SYSERR: Attempting to count members for a group with no members.\r\n");
    return -1;
  }
}

// From comm.cpp
#define SENDOK(ch) ((ch)->desc && AWAKE(ch) && !(PLR_FLAGGED((ch), PLR_WRITING) || PLR_FLAGGED((ch), PLR_EDITING) || PLR_FLAGGED((ch), PLR_MAILING) || PLR_FLAGGED((ch), PLR_CUSTOMIZE)) && (STATE(ch->desc) != CON_SPELL_CREATE))
void Playergroup::invite(struct char_data *ch, char *argument) {
  struct char_data *target = NULL;
  char arg[strlen(argument) + 1];
  one_argument(argument, arg);
  
  if (!*arg) {
    send_to_char("Whom would you like to invite to join your group?\r\n", ch);
  } else if (!(target = get_char_room_vis(ch, arg))) {
    send_to_char("They aren't here.\r\n", ch);
  } else if (ch == target) {
    send_to_char("Why would you want to invite yourself?\r\n", ch);
  } else if (IS_NPC(target)) {
    send_to_char("NPCs aren't interested in player groups.\r\n", ch);
  } else if (!SENDOK(target)) {
    send_to_char("They can't hear you.\r\n", ch);
  } else if (GET_TKE(target) < 100) {
    send_to_char(ch, "%s isn't experienced enough to be a valuable addition to your group yet.\r\n", capitalize(GET_CHAR_NAME(target)));
  } else if (GET_PGROUP_MEMBER_DATA(target) && GET_PGROUP(target) && GET_PGROUP(target) == GET_PGROUP(ch)) {
    if (is_secret())
      send_to_char("They can't hear you.\r\n", ch);
    else
      send_to_char("They're already part of your group!\r\n", ch);
    // TODO: If the group is secret, this is info disclosure.
  } else if (FALSE) {
    // TODO: The ability to auto-decline invitations / block people / not be harassed by invitation spam.
  } else {
    // Check to see if the player has already been invited to your group.
    Pgroup_invitation *temp = target->pgroup_invitations;
    
    while (temp) {
      if (temp->pg_idnum == idnum) {
        if (is_secret())
          send_to_char("They can't hear you.\r\n", ch);
        else
          send_to_char("They've already been invited to your group.\r\n", ch);
        return;
      }
      temp = temp->next;
    }
    
    strcpy(buf, "You invite $N to join your group.");
    act(buf, FALSE, ch, NULL, target, TO_CHAR);
    if (is_secret())
      snprintf(buf, MAX_STRING_LENGTH, "^GSomeone has invited you to join their playergroup, '%s'. You can ACCEPT or DECLINE this at any time in the next %d days.^n", get_name(), PGROUP_INVITATION_LIFETIME_IN_DAYS);
    else
      snprintf(buf, MAX_STRING_LENGTH, "^G$n has invited you to join their playergroup, '%s'. You can ACCEPT or DECLINE this at any time in the next %d days.^n", get_name(), PGROUP_INVITATION_LIFETIME_IN_DAYS);
    act(buf, FALSE, ch, NULL, target, TO_VICT);
    snprintf(buf, sizeof(buf), "You have been invited to join the playergroup %s.\r\n", get_name());
    raw_store_mail(GET_IDNUM(target), GET_IDNUM(ch), is_secret() ? "a shadowy figure" : GET_CHAR_NAME(ch), buf);
    
    if (!is_secret())
      audit_log_vfprintf("%s has invited %s to join the group.", GET_CHAR_NAME(ch), GET_CHAR_NAME(target));
    
    // Create a new invitation.
    temp = new Pgroup_invitation(GET_PGROUP(ch)->get_idnum());
    
    // Add it to the linked list.
    temp->next = target->pgroup_invitations;
    if (target->pgroup_invitations)
      target->pgroup_invitations->prev = temp;
    target->pgroup_invitations = temp;
    
    temp->save_invitation_to_db(GET_IDNUM(target));
  }
}

void Playergroup::add_member(struct char_data *ch) {
  if (IS_NPC(ch))
    return;
  
  if (GET_PGROUP_MEMBER_DATA(ch)) {
    if (GET_PGROUP(ch)) {
      log_vfprintf("WARNING: add_member called on %s for group %s when they were already part of %s.",
          GET_CHAR_NAME(ch), get_name(), GET_PGROUP(ch)->get_name());
    } else {
      log("SYSERR: overriding add_member call, also %s had pgroup_data but no pgroup.");
    }
    delete GET_PGROUP_MEMBER_DATA(ch);
  }
  GET_PGROUP_MEMBER_DATA(ch) = new Pgroup_data();
  GET_PGROUP(ch) = this;
  GET_PGROUP_MEMBER_DATA(ch)->rank = 1;
  if (GET_PGROUP_MEMBER_DATA(ch)->title)
    delete GET_PGROUP_MEMBER_DATA(ch)->title;
  GET_PGROUP_MEMBER_DATA(ch)->title = str_dup("Member");
  
  // Save the character.
  playerDB.SaveChar(ch);
  
  // Log the character's joining.
  // TODO: Should we conceal this info in a secret group?
  audit_log_vfprintf("%s has joined the group.", GET_CHAR_NAME(ch));
  
  // Delete the invitation and remove it from the character's linked list.
  Pgroup_invitation *pgi = ch->pgroup_invitations;
  while (pgi) {
    if (pgi->pg_idnum == idnum) {
      if (pgi->prev)
        pgi->prev->next = pgi->next;
      else {
        // This is the head of the list.
        ch->pgroup_invitations = pgi->next;
      }
      
      if (pgi->next)
        pgi->next->prev = pgi->prev;
      
      pgi->delete_invitation_from_db(GET_IDNUM(ch));
      delete pgi;
      pgi = NULL;
    } else {
      pgi = pgi->next;
    }
  }
}

void Playergroup::remove_member(struct char_data *ch) {
  if (IS_NPC(ch))
    return;
  
  if (!GET_PGROUP_MEMBER_DATA(ch) || !GET_PGROUP(ch)) {
    log_vfprintf("SYSERR: pgroup->remove_member() called on %s, who does not have an associated group.",
                 GET_CHAR_NAME(ch));
    return;
  }
  
  // TODO: DB updates.
  delete GET_PGROUP_MEMBER_DATA(ch);
  GET_PGROUP_MEMBER_DATA(ch) = NULL;
  
  // Save the character.
  playerDB.SaveChar(ch);
}

/*************** Invitation methods. *****************/
Pgroup_invitation::Pgroup_invitation() :
pg(NULL), pg_idnum(0), prev(NULL), next(NULL)
{
  expires_on = calculate_expiration();
  // log_vfprintf("Expiration calculated as %ld seconds.", expires_on);
}

Pgroup_invitation::Pgroup_invitation(long idnum) :
pg(NULL), pg_idnum(idnum), prev(NULL), next(NULL)
{
  expires_on = calculate_expiration();
  // log_vfprintf("Expiration calculated as %ld seconds.", expires_on);
}

Pgroup_invitation::Pgroup_invitation(long idnum, time_t expiration) :
pg(NULL), pg_idnum(idnum), prev(NULL), next(NULL)
{
  set_expiration(expiration);
}

void Pgroup_invitation::set_expiration(time_t expiration) {
  expires_on = expiration;
  if (is_expired()) {
    log("SYSERR: A time in the past was passed to pgroup_invitation.");
    expires_on = calculate_expiration();
  }
}

bool Pgroup_invitation::is_expired() {
  return is_expired(expires_on);
}

bool Pgroup_invitation::is_expired(time_t tm) {
  time_t current_time;
  time(&current_time);
  return difftime(current_time, tm) >= 0;
}

time_t Pgroup_invitation::calculate_expiration() {
  time_t current_time;
  time(&current_time);
  struct tm* tm = localtime(&current_time);
  tm->tm_mday += PGROUP_INVITATION_LIFETIME_IN_DAYS;
  return mktime(tm);
}

bool Pgroup_invitation::save_invitation_to_db(long ch_idnum) {
  char querybuf[MAX_STRING_LENGTH];
  
  const char * pinv_save_query_format =
  "INSERT INTO `playergroup_invitations` (`idnum`, `Group`, `Expiration`) VALUES ('%ld', '%ld', '%ld')";
  const char * pinv_delete_query_format =
  "DELETE FROM `playergroup_invitations` WHERE `idnum` = '%ld' AND `Group` = '%ld'";
  
  // Execute deletion.
  snprintf(querybuf, MAX_STRING_LENGTH, pinv_delete_query_format, ch_idnum, pg_idnum);
  mysql_wrapper(mysql, querybuf);
  
  // Execute save.
  snprintf(querybuf, MAX_STRING_LENGTH, pinv_save_query_format, ch_idnum, pg_idnum, expires_on);
  mysql_wrapper(mysql, querybuf);
  
  return mysql_errno(mysql) != 0;
}

// Delete expired invitations from the character and their DB entry.
void Pgroup_invitation::prune_expired(struct char_data *ch) {
  assert(ch);
  
  Pgroup_invitation *pgi = ch->pgroup_invitations;
  Pgroup_invitation *temp;
  
  while (pgi) {
    if (pgi->is_expired()) {
      // TODO: If the group is disabled, the character may not know they have this invitation.
      // Gag expiration messages on disabled-group-invites?
      send_to_char(ch, "Your invitation from '%s' has expired.\r\n",
                   Playergroup::find_pgroup(pgi->pg_idnum)->get_name());
      
      pgi->delete_invitation_from_db(GET_IDNUM(ch));
      
      if (pgi->prev) {
        pgi->prev->next = pgi->next;
      } else {
        // This is the head of the linked list.
        ch->pgroup_invitations = pgi->next;
      }
      
      if (pgi->next)
        pgi->next->prev = pgi->prev;
      
      temp = pgi->next;
      delete pgi;
      pgi = temp;
    } else {
      pgi = pgi->next;
    }
  }
}

void Pgroup_invitation::delete_invitation_from_db(long ch_idnum) {
  Pgroup_invitation::delete_invitation_from_db(pg_idnum, ch_idnum);
}

void Pgroup_invitation::delete_invitation_from_db(long pgr_idnum, long ch_idnum) {
  snprintf(buf, MAX_STRING_LENGTH, "DELETE FROM `playergroup_invitations` WHERE `idnum`='%ld' AND `Group`='%ld'", ch_idnum, pgr_idnum);
  mysql_wrapper(mysql, buf);
}

Playergroup *Pgroup_invitation::get_pg() {
  if (pg)
    return pg;
  
  pg = Playergroup::find_pgroup(pg_idnum);
  return pg;
}
