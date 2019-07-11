#include <mysql/mysql.h>

#include "awake.h"
#include "structs.h"
#include "db.h"
#include "comm.h"
#include "olc.h"
#include "newdb.h"

#define HELPFILE d->edit_helpfile

#define MAX_HELPFILE_TITLE_LENGTH 127

extern void string_add(struct descriptor_data *d, char *str);

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
        // Delete the original help article.
        sprintf(query_buf, "DELETE FROM help_topic WHERE `name`='%s'", prepare_quotes(buf3, HELPFILE->original_title, sizeof(buf)));
        mysql_wrapper(mysql, query_buf);
      }
      sprintf(query_buf, "INSERT INTO help_topic (name, body) VALUES ('%s', '%s')"
                         " ON DUPLICATE KEY UPDATE"
                         "  `name` = VALUES(`name`),"
                         "  `body` = VALUES(`body`)",
              prepare_quotes(buf, HELPFILE->title, sizeof(buf)),
              prepare_quotes(buf2, HELPFILE->body, sizeof(buf2)));
      if (mysql_wrapper(mysql, query_buf)) {
        send_to_char("Sorry, something went wrong with saving your edit.\r\n", CH);
      } else {
        send_to_char("OK.\r\n", CH);
      }
      // fallthrough
    case 'x': // Discard the edit.
      DELETE_ARRAY_IF_EXTANT(HELPFILE->original_title);
      DELETE_ARRAY_IF_EXTANT(HELPFILE->title);
      DELETE_ARRAY_IF_EXTANT(HELPFILE->body);
      DELETE_ARRAY_IF_EXTANT(HELPFILE->links);
      delete HELPFILE;
      HELPFILE = NULL;
      STATE(d) = CON_PLAYING;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
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
        send_to_char("Titles cannot contain the '%' character.", CH);
        return;
      }
      
      // Check for clobber.
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
