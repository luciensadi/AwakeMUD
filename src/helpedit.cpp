#include <mysql/mysql.h>

#include "awake.h"
#include "structs.h"
#include "db.h"
#include "comm.h"
#include "olc.h"
#include "newdb.h"
#include "interpreter.h"
#include "helpedit.h"

#define HELPFILE d->edit_helpfile

#define MAX_HELPFILE_TITLE_LENGTH 127

extern void string_add(struct descriptor_data *d, char *str);

ACMD(do_helpedit) {
  if (!ch->desc) {
    send_to_char("You don't even have a descriptor, how are you going to edit a helpfile?\r\n", ch);
    mudlog("Character without descriptor tried to edit a helpfile, wut?", ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  if (IS_NPC(ch)) {
    send_to_char("NPCs can't edit helpfiles.\r\n", ch);
    return;
  }
  
  if (!*argument) {
    send_to_char("Syntax: HELPEDIT NEW or HELPEDIT <exact and full title of the help file to edit>\r\n", ch);
    return;
  }
  
  if (strchr(argument, '%')) {
    send_to_char("Helpfile names may not contain the '%' character.\r\n", ch);
    return;
  }
  
  skip_spaces(&argument);
  
  if (str_cmp(argument, "new") == 0) {
    // Create a new helpfile and display menu (which puts them into edit mode), then bail out of this function.
    ch->desc->edit_helpfile = new struct help_data(NULL);
    helpedit_disp_menu(ch->desc);
    return;
  }
  
  // Looks like we gotta do a SQL lookup to see if their target file exists.
  sprintf(buf, "SELECT name, body FROM help_topic WHERE name='%s'", prepare_quotes(buf2, argument, sizeof(buf2)));
  if (mysql_wrapper(mysql, buf)) {
    send_to_char("Sorry, an error occurred. Try again later.\r\n", ch);
    mudlog("SYSERR: A MySQL error occurred while attempting to edit a helpfile. See log for details.", ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row) {
    mysql_free_result(res);
    send_to_char("No helpfile by that name currently exists. Please use the *exact and full* name, or HELPEDIT NEW to make a new file.\r\n", ch);
    return;
  }
  
  // Prepare their edit file.
  ch->desc->edit_helpfile = new struct help_data(str_dup(row[0]));
  ch->desc->edit_helpfile->body = str_dup(row[1]);
  mysql_free_result(res);
  helpedit_disp_menu(ch->desc);
}

ACMD(do_helpexport) {
  if (!ch->desc) {
    send_to_char("You don't even have a descriptor, how are you going to edit a helpfile?\r\n", ch);
    mudlog("Character without descriptor tried to edit a helpfile, wut?", ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  if (IS_NPC(ch)) {
    send_to_char("NPCs can't edit helpfiles.\r\n", ch);
    return;
  }
  
  if (!*argument) {
    send_to_char("Syntax: HELPEXPORT <exact and full title of the help file to export>\r\n", ch);
    return;
  }
  
  if (strchr(argument, '%')) {
    send_to_char("Helpfile names may not contain the '%' character.\r\n", ch);
    return;
  }
  
  skip_spaces(&argument);
  
  // Looks like we gotta do a SQL lookup to see if their target file exists.
  sprintf(buf, "SELECT name FROM help_topic WHERE name='%s'", prepare_quotes(buf2, argument, sizeof(buf2)));
  if (mysql_wrapper(mysql, buf)) {
    send_to_char("Sorry, an error occurred. Try again later.\r\n", ch);
    mudlog("SYSERR: A MySQL error occurred while attempting to export a helpfile. See log for details.", ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (!row) {
    mysql_free_result(res);
    send_to_char("No helpfile by that name currently exists. Please use the *exact and full* name of the helpfile.\r\n", ch);
    return;
  }
  mysql_free_result(res);
  
  // Generate the initial pass of the SQL command.
  const char *output_ptr = generate_sql_for_helpfile_by_name(argument);
  if (!output_ptr) {
    send_to_char(ch, "Something went wrong.\r\n");
    return;
  }
  
  send_to_char(ch, "Output:\r\n\r\n^c%s^n\r\n\r\n^RWARNING:^n Make sure your client's line wrap is turned off!", double_up_color_codes(output_ptr));
}


// Display the main menu. Also sets our various states and bits for the editor char.
void helpedit_disp_menu(struct descriptor_data *d) {
  if (!HELPFILE->original_title) // New file-- always display new title.
    send_to_char(CH, "1) Helpfile title: ^c%s^n\r\n", HELPFILE->title);
  else if (!HELPFILE->title || strcmp(HELPFILE->title, HELPFILE->original_title) == 0) // Old file, no new title yet.
    send_to_char(CH, "1) Helpfile title: ^c%s^n\r\n", HELPFILE->original_title);
  else // Old file, new title.
    send_to_char(CH, "1) Helpfile title: ^c%s^n  (was: ^c%s^n)\r\n", HELPFILE->title, HELPFILE->original_title);
  
  send_to_char(CH, "2) Helpfile body:\r\n%s\r\n", HELPFILE->body);
  send_to_char("\r\n", CH);
  send_to_char("q) Save and quit\r\n", CH);
  send_to_char("x) Quit without saving\r\n", CH);
  send_to_char("Enter selection: ", CH);
  
  d->edit_mode = HELPEDIT_MAIN_MENU;
  STATE(d) = CON_HELPEDIT;
  PLR_FLAGS(d->character).SetBit(PLR_EDITING);
}

void helpedit_parse_main_menu(struct descriptor_data *d, const char *arg) {
  switch (*arg) {
    case '1':
      send_to_char("Enter new helpfile title: \r\n", CH);
      d->edit_mode = HELPEDIT_TITLE;
      break;
    case '2':
      send_to_char("Enter new helpfile body: \r\n", CH);
      d->edit_mode = HELPEDIT_BODY;
      
      // Set up for d->str modification.
      DELETE_D_STR_IF_EXTANT(d);
      INITIALIZE_NEW_D_STR(d);
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      
      // Force no formatting mode by making the first entry into the string buf the special code /**/.
      char str_buf[50];
      sprintf(str_buf, "/**/");
      string_add(d, str_buf);
      break;
    case 'q': // Save the edit.
      char query_buf[MAX_STRING_LENGTH];
      if (HELPFILE->title && HELPFILE->original_title && strcmp(HELPFILE->title, HELPFILE->original_title) != 0) {
        // Updated existing and changed name.
        sprintf(buf, "%s wrote new helpfile '%s' (renamed from '%s').", GET_CHAR_NAME(CH), HELPFILE->title, HELPFILE->original_title);
        mudlog(buf, CH, LOG_SYSLOG, TRUE);
      } else if (HELPFILE->original_title && !HELPFILE->title) {
        // Updated existing.
        sprintf(buf, "%s wrote new helpfile '%s'.", GET_CHAR_NAME(CH), HELPFILE->original_title);
        mudlog(buf, CH, LOG_SYSLOG, TRUE);
      } else {
        // Created new.
        sprintf(buf, "%s wrote new helpfile '%s'.", GET_CHAR_NAME(CH), HELPFILE->title ? HELPFILE->title : HELPFILE->original_title);
        mudlog(buf, CH, LOG_SYSLOG, TRUE);
      }
      
      if (HELPFILE->original_title) {
        sprintf(buf, "Query to restore original helpfile: \r\n%s\r\n", generate_sql_for_helpfile_by_name(HELPFILE->original_title));
        log(buf);
      }
      
      // Delete the original help article (if any).
      sprintf(query_buf, "DELETE FROM help_topic WHERE `name`='%s'", prepare_quotes(buf3, HELPFILE->original_title, sizeof(buf)));
      mysql_wrapper(mysql, query_buf);
      
      // Compose and execute the insert-on-duplicate-key-update query.
      if (mysql_wrapper(mysql, generate_sql_for_helpfile(HELPFILE->title ? HELPFILE->title : HELPFILE->original_title, HELPFILE->body)))
        send_to_char("Sorry, something went wrong with saving your edit!\r\n", CH);
      // fallthrough
    case 'x': // Discard the player's editing struct.
      DELETE_ARRAY_IF_EXTANT(HELPFILE->original_title);
      DELETE_ARRAY_IF_EXTANT(HELPFILE->title);
      DELETE_ARRAY_IF_EXTANT(HELPFILE->body);
      DELETE_ARRAY_IF_EXTANT(HELPFILE->links);
      delete HELPFILE;
      HELPFILE = NULL;
      STATE(d) = CON_PLAYING;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      send_to_char("OK.\r\n", CH);
      break;
    default:
      send_to_char("That's not a valid option. Enter selection: \r\n", CH);
      break;
  }
}

void helpedit_parse(struct descriptor_data *d, const char *arg) {
  MYSQL_RES *res = NULL;
  MYSQL_ROW row;
  
  switch (d->edit_mode) {
    case HELPEDIT_MAIN_MENU:
      helpedit_parse_main_menu(d, arg);
      break;
    case HELPEDIT_TITLE:
      // Helpfile title is VARCHAR(128).
      if (strlen(arg) > MAX_HELPFILE_TITLE_LENGTH) {
        send_to_char(CH, "The maximum helpfile title length is %d. Enter title: \r\n", MAX_HELPFILE_TITLE_LENGTH);
        return;
      }
      
      // Check for %s.
      if (strchr(arg, '%')) {
        send_to_char("Titles cannot contain the '%' character.\r\n", CH);
        return;
      }
      
      // Check for clobber. Ignore clobber if it's this file's own title.
      if (HELPFILE->original_title && str_cmp(arg, HELPFILE->original_title) != 0) {
        sprintf(buf, "SELECT name FROM help_topic WHERE `name`='%s'",
                prepare_quotes(buf2, arg, sizeof(buf2)));
        mysql_wrapper(mysql, buf);
        res = mysql_use_result(mysql);
        row = mysql_fetch_row(res);
        if (row) {
          mysql_free_result(res);
          send_to_char("A helpfile by that name already exists. Select something else: \r\n", CH);
          return;
        }
        mysql_free_result(res);
      }
      
      // Accept the new string and store it in our temporary edit struct.
      DELETE_ARRAY_IF_EXTANT(HELPFILE->title);
      HELPFILE->title = str_dup(arg);
      helpedit_disp_menu(d);
      break;
    case HELPEDIT_BODY:
      // We should never get here.
      break;
    default:
      sprintf(buf, "SYSERR: Unknown helpedit edit mode %d encountered in helpedit_parse.", STATE(d));
      mudlog(buf, d->original ? d->original : d->character, LOG_SYSLOG, TRUE);
      send_to_char("Sorry, your edit has been corrupted.\r\n", CH);
      helpedit_disp_menu(d);
      break;
  }
}

const char *generate_sql_for_helpfile(const char *name, const char *body) {
  if (name == NULL) {
    mudlog("SYSERR: NULL name or body pointer passed to generate_sql_for_helpfile!", NULL, LOG_SYSLOG, TRUE);
    return "";
  }
  
  char name_buf[strlen(name) * 2 + 1];
  char body_buf[strlen(body) * 2 + 1];
  
  static char output_buf[MAX_STRING_LENGTH];
  
  sprintf(output_buf, "INSERT INTO help_topic (name, body) VALUES ('%s', '%s')"
          " ON DUPLICATE KEY UPDATE"
          " `name` = VALUES(`name`),"
          " `body` = VALUES(`body`);",
          prepare_quotes(name_buf, name, sizeof(name_buf)),
          prepare_quotes(body_buf, body, sizeof(body_buf)));
  
  return output_buf;
}

// Given a helpfile's name, generate the SQL to restore it.
const char *generate_sql_for_helpfile_by_name(const char *name) {
  if (!name || !*name) {
    sprintf(buf3, "SYSERR: generate_sql_for_helpfile_by_name received name '%s'.", name);
    mudlog(buf3, NULL, LOG_SYSLOG, TRUE);
    return NULL;
  }
  
  if (strchr(name, '%')) {
    sprintf(buf3, "SYSERR: generate_sql_for_helpfile_by_name received name that contained %% '%s'.", name);
    mudlog(buf3, NULL, LOG_SYSLOG, TRUE);
    return NULL;
  }
  
  char quotebuf[500];
  sprintf(buf, "SELECT * FROM help_topic WHERE name='%s'", prepare_quotes(quotebuf, name, sizeof(quotebuf)));
  if (mysql_wrapper(mysql, buf))
    return NULL;
  
  MYSQL_RES *res = mysql_use_result(mysql);
  MYSQL_ROW row = mysql_fetch_row(res);
  if (row) {
    const char *final_query = generate_sql_for_helpfile(row[0], row[1]);
    mysql_free_result(res);
    return final_query;
  }
  mysql_free_result(res);
  
  sprintf(buf3, "SYSERR: Received invalid name '%s' to generate_sql_for_helpfile_by_name, returning no query.", name);
  mudlog(buf3, NULL, LOG_SYSLOG, TRUE);
  return NULL;
}
