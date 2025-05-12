#include <mysql/mysql.h>

#include "structs.hpp"
#include "awake.hpp"
#include "db.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "utils.hpp"
#include "screen.hpp"
#include "constants.hpp"
#include "olc.hpp"
#include "newmail.hpp"
#include "newdb.hpp"
#include "memory.hpp"
#include "config.hpp"

#define CH d->character
#define SEC d->edit_obj
#define SEC_MENU		    0
#define SEC_INIT		    1
#define SEC_MAILMENU		2
#define SEC_READMAIL		3
#define SEC_DELMAIL		  4
#define SEC_SENDMAIL		5
#define SEC_READMAIL2		6
#define SEC_BANKMENU		7
#define SEC_WIRE1		    8
#define SEC_WIRE2		    9
#define SEC_PHONEMENU		10
#define SEC_PHONEADD1		11
#define SEC_PHONEADD2		12
#define SEC_PHONECALL		13
#define SEC_PHONEDEL		14
#define SEC_NOTEMENU		15
#define SEC_NOTEREAD		16
#define SEC_NOTEREAD2		17
#define SEC_NOTEADD1		18
#define SEC_NOTEADD2		19
#define SEC_NOTEDEL		  20
#define SEC_KEEPMAIL    21
#define SEC_WIRE3       22

#define POCSEC_FOLDER_MAIL      "Mail"
#define POCSEC_FOLDER_NOTES     "Notes"
#define POCSEC_FOLDER_PHONEBOOK "Phonebook"
#define POCSEC_FOLDER_FILES     "Files"

extern bool is_approved_multibox_host(const char *host);

void migrate_pocket_secretary_contents(struct obj_data *obj, idnum_t owner);

ACMD_DECLARE(do_phone);

struct obj_data *generate_pocket_secretary_folder(struct obj_data *sec, const char *string) {
  struct obj_data *folder = read_object(OBJ_POCKET_SECRETARY_FOLDER, VIRTUAL, OBJ_LOAD_REASON_POCSEC_FOLDER_GENERATION);
  folder->restring = str_dup(string);
  obj_to_obj(folder, sec);
  return folder;
}

void initialize_pocket_secretary(struct obj_data *sec) {
  generate_pocket_secretary_folder(sec, POCSEC_FOLDER_MAIL);
  generate_pocket_secretary_folder(sec, POCSEC_FOLDER_NOTES);
  generate_pocket_secretary_folder(sec, POCSEC_FOLDER_PHONEBOOK);
  generate_pocket_secretary_folder(sec, POCSEC_FOLDER_FILES);
}

void wire_nuyen(struct char_data *ch, int amount, idnum_t character_id, const char *reason)
{
  // First, scan the game to see if the target character is online.
  struct char_data *targ = NULL;
  char query_buf[1000], name_buf[500];

  // We used to just iterate over descriptors, but the change would be lost if someone was linkdead during transfer and reconnected after.
  for (targ = character_list; targ; targ = targ->next_in_character_list) {
    if (GET_IDNUM(targ) == character_id)
      break;
  }

  // Deduct from the sender (if any). Not a faucet or sink, unless coming from a shop.
  if (ch) {
    GET_BANK_RAW(ch) -= amount;
    playerDB.SaveChar(ch);
  }

  // Add to the receiver. Not a faucet or sink, unless coming from a shop.
  if (targ) {
    GET_BANK_RAW(targ) += amount;
    playerDB.SaveChar(targ);
  } else {
    snprintf(query_buf, sizeof(query_buf), "UPDATE pfiles SET Bank=Bank+%d WHERE idnum=%ld;", amount, character_id);
    mysql_wrapper(mysql, query_buf);
  }

  // Mail it. We don't send mail for NPC shopkeepers refunding you.
  if (ch) {
    snprintf(query_buf, sizeof(query_buf), "%s has wired %d nuyen to your account with the memo '%s^n'.\r\n", ch ? GET_CHAR_NAME(ch) : "Someone", amount, reason);
    snprintf(name_buf, sizeof(name_buf), "%s (wire transfer)", ch ? GET_CHAR_NAME(ch) : "Someone");
    raw_store_mail(character_id, GET_IDNUM(ch), name_buf, query_buf);
  }

  // Log it.
  char *player_name = targ ? NULL : get_player_name(character_id);
  if ((targ ? is_same_host(ch, targ) : is_same_host(ch, character_id)) && !is_approved_multibox_host(ch->desc->host)) {
    mudlog_vfprintf(ch, LOG_CHEATLOG, "%s wired %d nuyen to %s (memo: %s) [SAME-HOST @ %s]",
                    ch ? GET_CHAR_NAME(ch) : "An NPC",
                    amount,
                    targ ? GET_CHAR_NAME(targ) : player_name,
                    reason,
                    ch->desc->host);
  } else {
    mudlog_vfprintf(ch, LOG_GRIDLOG, "%s wired %d nuyen to %s (memo: %s).",
                    ch ? GET_CHAR_NAME(ch) : "An NPC",
                    amount,
                    targ ? GET_CHAR_NAME(targ) : player_name,
                    reason);
  }
  DELETE_ARRAY_IF_EXTANT(player_name);
}

void pocketsec_phonemenu(struct descriptor_data *d)
{
  bool first_run = TRUE;
  char query_buf[500] = {0};
  MYSQL_RES *res;
  MYSQL_ROW row;

  if (!d) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: null d to pocsec_phonemenu");
    return;
  }
  
  struct char_data *ch = d->original ? d->original : d->character;

  if (!ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: no character to pocsec_phonemenu");
    return;
  }

  snprintf(query_buf, sizeof(query_buf), "SELECT phonenum, note FROM pocsec_phonebook WHERE idnum=%ld ORDER BY note DESC;", GET_IDNUM(ch));
  if (mysql_wrapper(mysql, query_buf)) {
    send_to_char(ch, "Sorry, your phonebook isn't working right now.\r\n");
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: failed to query phonebook for %s", GET_CHAR_NAME(ch));
    return;
  }
  
  
  if (!(res = mysql_use_result(mysql))) {
    send_to_char(ch, "Sorry, your phonebook isn't working right now.\r\n");
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: failed to use_result when querying phonebook for %s", GET_CHAR_NAME(ch));
    return;
  }
  
  int idx = 0;
  while ((row = mysql_fetch_row(res))) {
    if (first_run) {
      send_to_char("^LYour Phonebook^n\r\n", ch);
      first_run = FALSE;
    }

    send_to_char(CH, " %2d > %-20s - %s\r\n", idx++, row[0], row[1]);
  }
  mysql_free_result(res);
  
  if (first_run) {
    send_to_char("Your phonebook is empty.\r\n", CH);
  }

  send_to_char("\r\n[^cC^n]^call^n     [^cA^n]^cdd Name^n     [^cD^n]^celete Name^n     [^cB^n]^cack^n\r\n", CH);
  d->edit_mode = SEC_PHONEMENU;
}

void pocketsec_notemenu(struct descriptor_data *d)
{
  struct obj_data *data = NULL, *folder = SEC->contains;
  int i = 0;
  for (; folder; folder = folder->next_content)
    if (!strcmp(folder->restring, POCSEC_FOLDER_NOTES))
      break;

  if (!folder) {
    mudlog("Prevented missing-notes crash. This player has lost their notes.", d->character, LOG_SYSLOG, TRUE);
    folder = generate_pocket_secretary_folder(SEC, POCSEC_FOLDER_NOTES);
  }
  CLS(CH);
  send_to_char(CH, "^LNotes^n\r\n");
  for (data = folder->contains; data; data = data->next_content) {
    i++;
    send_to_char(CH, " %2d > %s\r\n", i, GET_OBJ_NAME(data));
  }
  send_to_char("\r\n[^cR^n]^cead^n     [^cA^n]^cdd Note^n     [^cD^n]^celete Note^n     [^cB^n]^cack^n\r\n", CH);
  d->edit_mode = SEC_NOTEMENU;
}


void pocketsec_menu(struct descriptor_data *d)
{
  CLS(CH);
  if (!SEC->contains) {
    send_to_char("This pocket secretary has not been initialized yet. Initialize? [Y/N]\r\n", CH);
    d->edit_mode = SEC_INIT;
  } else {
    // Make sure it's been converted to DB format.
    migrate_pocket_secretary_contents(SEC, GET_IDNUM(CH));

    send_to_char(CH, "^cMain Menu^n\r\n"
                 "   [^c1^n]^c%sMail%s^n\r\n"
                 "   [^c2^n] ^cNotes^n\r\n"
                 "   [^c3^n] ^cPhonebook^n\r\n"
                 // "   [^c4^n] ^cFiles^n\r\n"
                 "   [^c5^n] ^cBanking^n\r\n"
                 "   [^c6^n] ^c%sock^n\r\n"
                 "   [^c7^n] ^c%s^n\r\n"
                 "   [^c0^n] ^cQuit^n\r\n",
                 amount_of_mail_waiting(CH) > 0 ? " ^R" : " ",
                 amount_of_mail_waiting(CH) > 0 ? " (new messages)" : "",
                 GET_POCKET_SECRETARY_LOCKED_BY(SEC) ? "Unl" : "L",
                 GET_POCKET_SECRETARY_SILENCED(SEC) ? "Unsilence" : "Silence");
    d->edit_mode = SEC_MENU;
  }
}

void pocketsec_mailmenu(struct descriptor_data *d)
{
  char query_buf[500] = {0};
  bool first_run = TRUE;
  MYSQL_RES *res;
  MYSQL_ROW row;

  snprintf(query_buf, sizeof(query_buf), "SELECT sender_name, is_read, is_protected FROM pfiles_mail WHERE recipient=%ld ORDER BY idnum DESC;", GET_IDNUM(CH));
  if (mysql_wrapper(mysql, query_buf)) {
    send_to_char(CH, "Sorry, your mail isn't working right now.\r\n");
    mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: failed to query mail for %s", GET_CHAR_NAME(CH));
    return;
  }

  if (!(res = mysql_use_result(mysql))) {
    send_to_char(CH, "Sorry, your mail isn't working right now.\r\n");
    mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: failed to use_result when querying mail for %s", GET_CHAR_NAME(CH));
    return;
  }
  
  int idx = 0;
  while ((row = mysql_fetch_row(res))) {
    if (first_run) {
      send_to_char(CH, "^LShadowland Mail Network^n\r\n");
      first_run = FALSE;
    }

    send_to_char(CH, " %2d > %s%s^n%s\r\n", 
                  idx++,
                  *(row[1]) == '0' ? "(unread) ^R" : "",
                  row[0],
                  *(row[2]) == '1' ? " (kept)" : ""
                );
  }
  mysql_free_result(res);
  
  if (first_run) {
    send_to_char("You haven't received any mail.\r\n", CH);
  } else {
    // Mark all existing mail as received.
    char query_buf[500] = {0};
    snprintf(query_buf, sizeof(query_buf), "UPDATE pfiles_mail SET is_received=1 WHERE recipient=%ld;", GET_IDNUM(CH));
    if (mysql_wrapper(mysql, query_buf)) {
      mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: failed to mark mail as read for %s", GET_CHAR_NAME(CH));
    }
  }

  send_to_char("\r\n[^cR^n]^cead Mail^n     [^cD^n]^celete Mail^n     [^cS^n]^cend Mail^n     [^cK^n]^ceep Mail^n     [^cB^n]^cack^n\r\n", CH);
  d->edit_mode = SEC_MAILMENU;
}

void pocketsec_bankmenu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "Current Bank Balance: ^c%d^n\r\n[^cW^n]^cire Money^n     [^cB^n]^cack^n\r\n", GET_BANK(CH));
  d->edit_mode = SEC_BANKMENU;
}

void pocketsec_parse(struct descriptor_data *d, char *arg)
{
  struct obj_data *folder = NULL, *file = NULL;
  int i;
  long x;
  const char *name;

  switch (d->edit_mode) {
    case SEC_MENU:
      switch (atoi(arg)) {
        case 0:
          send_to_char("Good bye.\r\n", CH);
          STATE(d) = CON_PLAYING;
          act("$n looks up from $p.", TRUE, CH, SEC, 0, TO_ROOM);
          d->edit_obj = NULL;
          break;
        case 1:
          pocketsec_mailmenu(d);
          break;
        case 2:
          pocketsec_notemenu(d);
          break;
        case 3:
          pocketsec_phonemenu(d);
          break;
        case 4:
          send_to_char(d->character, "The files functionality has been disabled.\r\n");
          break;
        case 5:
          pocketsec_bankmenu(d);
          break;
        case 6:
          if (!GET_POCKET_SECRETARY_LOCKED_BY(SEC))
            GET_POCKET_SECRETARY_LOCKED_BY(SEC) = GET_IDNUM(CH);
          else GET_POCKET_SECRETARY_LOCKED_BY(SEC) = 0;
          pocketsec_menu(d);
          break;
        case 7:
          GET_POCKET_SECRETARY_SILENCED(SEC) = !GET_POCKET_SECRETARY_SILENCED(SEC);
          pocketsec_menu(d);
          break;
      }
      break;
    case SEC_PHONEMENU:
      switch (LOWER(*arg)) {
        case 'c':
          send_to_char("Call which entry?\r\n", CH);
          d->edit_mode = SEC_PHONECALL;
          break;
        case 'a':
          send_to_char("Enter Name:\r\n", CH);
          d->edit_mode = SEC_PHONEADD1;
          break;
        case 'd':
          send_to_char("Delete which entry? ('*' for all)\r\n", CH);
          d->edit_mode = SEC_PHONEDEL;
          break;
        case 'b':
          pocketsec_menu(d);
          break;
      }
      break;
    case SEC_PHONECALL:
      {
        char query_buf[MAX_INPUT_LENGTH + 500] = {0};
        MYSQL_RES *res;
        MYSQL_ROW row;

        int offset = atoi(arg);
        if (offset < 0) {
          pocketsec_phonemenu(d);
          send_to_char("That entry does not exist.\r\n", CH);
          return;
        }
        
        snprintf(query_buf, sizeof(query_buf), "SELECT phonenum FROM pocsec_phonebook WHERE idnum=%ld ORDER BY note DESC LIMIT 1 OFFSET %d;", GET_IDNUM(CH), offset);
        if (mysql_wrapper(mysql, query_buf)) {
          send_to_char(d->character, "Sorry, your phonebook isn't working right now.\r\n");
          mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: failed to query phonebook for %s", GET_CHAR_NAME(d->character));
          pocketsec_phonemenu(d);
          return;
        }

        if (!(res = mysql_use_result(mysql))) {
          send_to_char(CH, "Sorry, your phonebook isn't working right now.\r\n");
          mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: failed to select phone index entry for %s", GET_CHAR_NAME(d->character));
          pocketsec_phonemenu(d);
          return;
        }

        if (!(row = mysql_fetch_row(res)) && mysql_field_count(mysql)) {
          send_to_char(CH, "That entry does not exist.\r\n");
          pocketsec_phonemenu(d);
          mysql_free_result(res);
          return;
        }

        strlcpy(buf3, row[0], sizeof(buf3));
        mysql_free_result(res);

        STATE(d) = CON_PLAYING;
        act("$n looks up from $s $p.", TRUE, CH, SEC, 0, TO_ROOM);
        send_to_char(CH, "\r\n");
        d->edit_obj = NULL;
        do_phone(CH, buf3, 0, SCMD_RING);
      }
      break;
    case SEC_PHONEADD1:
      {
        d->edit_obj_secondary = read_object(OBJ_POCKET_SECRETARY_FOLDER, VIRTUAL, OBJ_LOAD_REASON_POCSEC_PHONEADD);

        // Ensure they're not trying something fucky.
        if (strlen(arg) >= 199 || str_cmp(arg, prepare_quotes(buf3, arg, sizeof(buf3)))) {
          send_to_char(CH, "Sorry, %s is not a valid phonebook record name (it can't be too long or contain quotes). Try again: ", arg);
          return;
        }

        d->edit_obj_secondary->restring = str_dup(arg);
        send_to_char("Enter Number: ", CH);
        d->edit_mode = SEC_PHONEADD2;
      }
      break;
    case SEC_PHONEADD2:
      {
        // Ensure they're not trying something fucky.
        int phonenum = atoi(arg);
        if (phonenum <= 0 || (phonenum < 10000000 && strlen(arg) < 8) || phonenum > 99999999) {
          send_to_char(CH, "Sorry, %s is not a valid phone number. Try again: ", arg);
          return;
        }

        char query_buf[MAX_INPUT_LENGTH + 500] = {0};        
        snprintf(query_buf, sizeof(query_buf), "INSERT INTO pocsec_phonebook (idnum, phonenum, note) VALUES (%ld, %d, '%s')", GET_IDNUM(CH), phonenum, prepare_quotes(buf3, d->edit_obj_secondary->restring, sizeof(buf3)));
        if (mysql_wrapper(mysql, query_buf)) {
          send_to_char(d->character, "Sorry, your phonebook isn't working right now.\r\n");
          mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: failed to insert into phonebook for %s", GET_CHAR_NAME(d->character));
        }

        extract_obj(d->edit_obj_secondary);
        d->edit_obj_secondary = NULL;
        pocketsec_phonemenu(d);
      }
      break;
    case SEC_PHONEDEL:
      if (arg && *arg) {
        if (*arg == '*') {
          char query_buf[MAX_INPUT_LENGTH + 500] = {0};        
          snprintf(query_buf, sizeof(query_buf), "DELETE FROM pocsec_phonebook WHERE idnum=%ld;", GET_IDNUM(CH));
          if (mysql_wrapper(mysql, query_buf)) {
            send_to_char(d->character, "Sorry, your phonebook isn't working right now.\r\n");
            mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: failed to delete from phonebook for %s", GET_CHAR_NAME(d->character));
          }
        } else {
          char query_buf[500] = {0};
          MYSQL_RES *res;
          MYSQL_ROW row;

          int offset = atoi(arg);
          if (offset < 0) {
            send_to_char(CH, "That entry does not exist.\r\n");
            pocketsec_phonemenu(d);
            return;
          }

          snprintf(query_buf, sizeof(query_buf), "SELECT record_id FROM pocsec_phonebook WHERE idnum=%ld ORDER BY note DESC LIMIT 1 OFFSET %d;", GET_IDNUM(CH), offset);
          if (mysql_wrapper(mysql, query_buf)) {
            send_to_char(CH, "Sorry, your phonebook isn't working right now.\r\n");
            mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: failed to query phonebook for %s", GET_CHAR_NAME(d->character));
            pocketsec_phonemenu(d);
            return;
          }

          if (!(res = mysql_use_result(mysql))) {
            send_to_char(CH, "Sorry, your phonebook isn't working right now.\r\n");
            mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: failed to select phone index entry for %s", GET_CHAR_NAME(d->character));
            pocketsec_phonemenu(d);
            return;
          }

          if (!(row = mysql_fetch_row(res)) && mysql_field_count(mysql)) {
            send_to_char(CH, "That entry does not exist.\r\n");
            pocketsec_phonemenu(d);
            mysql_free_result(res);
            return;
          }

          // Entry exists, delete it.
          snprintf(query_buf, sizeof(query_buf), "DELETE FROM pocsec_phonebook WHERE record_id=%s;", row[0]);
          mysql_free_result(res);

          if (mysql_wrapper(mysql, query_buf)) {
            send_to_char(d->character, "Sorry, your phonebook isn't working right now.\r\n");
            mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: failed to delete phonebook record for %s", GET_CHAR_NAME(d->character));
            pocketsec_phonemenu(d);
            return;
          }
        }
      }
      pocketsec_phonemenu(d);
      break;

    case SEC_NOTEMENU:
      switch (LOWER(*arg)) {
        case 'r':
          send_to_char("Read Which Note?\r\n", CH);
          d->edit_mode = SEC_NOTEREAD;
          break;
        case 'a':
          send_to_char("Enter Note Title:\r\n", CH);
          d->edit_mode = SEC_NOTEADD1;
          break;
        case 'd':
          send_to_char("Delete which note? ('*' for all)\r\n", CH);
          d->edit_mode = SEC_NOTEDEL;
          break;
        case 'b':
          pocketsec_menu(d);
          break;
      }
      break;
    case SEC_NOTEREAD:
      for (folder = SEC->contains; folder; folder = folder->next_content)
        if (!strcmp(folder->restring, POCSEC_FOLDER_NOTES))
          break;

      if (!folder) {
        mudlog("Prevented missing-notes crash. This player has lost their notes.", d->character, LOG_SYSLOG, TRUE);
        folder = generate_pocket_secretary_folder(SEC, POCSEC_FOLDER_NOTES);
      }

      i = atoi(arg);
      for (file = folder->contains; file && i > 1; file = file->next_content)
        i--;
      if (file) {
        send_to_char(file->photo, CH);
        GET_OBJ_VAL(file, 0) = 1;
        d->edit_mode = SEC_NOTEREAD2;
        send_to_char("Press [^cENTER^n] to continue.\r\n", CH);
      }
      else {
        pocketsec_notemenu(d);
        send_to_char("That note does not exist.\r\n", CH);
      }
      break;
    case SEC_NOTEREAD2:
      pocketsec_notemenu(d);
      break;
    case SEC_NOTEADD1:
      for (folder = SEC->contains; folder; folder = folder->next_content)
        if (!strcmp(folder->restring, POCSEC_FOLDER_NOTES))
          break;

      if (!folder) {
        mudlog("Prevented missing-notes crash. This player has lost their notes.", d->character, LOG_SYSLOG, TRUE);
        folder = generate_pocket_secretary_folder(SEC, POCSEC_FOLDER_NOTES);
      }

      file = read_object(OBJ_POCKET_SECRETARY_FOLDER, VIRTUAL, OBJ_LOAD_REASON_POCSEC_NOTEADD);
      obj_to_obj(file, folder);
      file->restring = str_dup(arg);
      send_to_char("Write note body. Use @ on a new line to finish.\r\n", CH);
      d->str = new (char *);
      *(d->str) = NULL;
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      d->edit_mode = SEC_NOTEADD2;
      break;
    case SEC_NOTEDEL:
      for (folder = SEC->contains; folder; folder = folder->next_content)
        if (!strcmp(folder->restring, POCSEC_FOLDER_NOTES))
          break;

      if (!folder) {
        mudlog("Prevented missing-notes crash. This player has lost their notes.", d->character, LOG_SYSLOG, TRUE);
        folder = generate_pocket_secretary_folder(SEC, POCSEC_FOLDER_NOTES);
      }

      if (arg && *arg == '*') {
        struct obj_data *next;
        for (file = folder->contains; file; file = next) {
          next = file->next_content;
          extract_obj(file);
        }
        folder->contains = NULL;
      } else {
        i = atoi(arg);
        for (file = folder->contains; file && i > 1; file = file->next_content)
          i--;
        if (file)
          extract_obj(file);
      }

      pocketsec_notemenu(d);
      break;

    case SEC_BANKMENU:
      switch (LOWER(*arg)) {
        case 'w':
          send_to_char("Wire money to whom?\r\n", CH);
          d->edit_mode = SEC_WIRE1;
          break;
        case 'b':
          pocketsec_menu(d);
          break;
      }
      break;
    case SEC_WIRE1:
      GET_EXTRA(CH) = FALSE;
      if ((GET_EXTRA(CH) = get_player_id(arg)) == -1) {
        send_to_char("The bank refuses to let you wire to that account.\r\n", CH);
          pocketsec_bankmenu(d);
        break;
      }
      name = get_player_name(GET_EXTRA(CH));
      send_to_char(CH, "How much do you wish to transfer to %s? (@ to cancel)\r\n", capitalize(name));
      delete [] name;
      d->edit_mode = SEC_WIRE2;
      break;
    case SEC_WIRE2:
      if (*arg != '@' && (x = atoi(arg)) != 0) {
        if (x < 0)
          send_to_char("How much do you wish to transfer? (@ to cancel)\r\n", CH);
        else if (x > GET_BANK(CH))
          send_to_char("You do not have that much to transfer. (@ to cancel)\r\n", CH);
        else {
          GET_EXTRA2(CH) = x;
          send_to_char("Please write a memo / reason for the transfer (e.g. 'for decking supplies'). (@ to cancel)\r\n", CH);
          d->edit_mode = SEC_WIRE3;
        }
        return;
      }
      pocketsec_bankmenu(d);
      break;
    case SEC_WIRE3:
      if (*arg != '@') {
        if (strlen(arg) < 5) {
          send_to_char("Please write a longer reason for this transfer, or type @ to abort.\r\n", CH);
        } else {
          name = get_player_name(GET_EXTRA(CH));
          send_to_char(CH, "You wire %d nuyen to %s's account.\r\n", GET_EXTRA2(CH), capitalize(name));
          delete [] name;
          wire_nuyen(CH, GET_EXTRA2(CH), GET_EXTRA(CH), arg);
          pocketsec_bankmenu(d);
        }
        return;
      }
    case SEC_MAILMENU:
      switch (LOWER(*arg)) {
        case 'r':
          send_to_char("Read which message?\r\n", CH);
          d->edit_mode = SEC_READMAIL;
          break;
        case 'd':
          send_to_char("Delete which message? ('*' for all)\r\n", CH);
          d->edit_mode = SEC_DELMAIL;
          break;
        case 's':
          send_to_char("Send message to whom?\r\n", CH);
          d->edit_mode = SEC_SENDMAIL;
          break;
        case 'k':
          send_to_char("Keep which message?\r\n", CH);
          d->edit_mode = SEC_KEEPMAIL;
          break;
        case 'b':
          pocketsec_menu(d);
          break;
      }
      break;
    case SEC_DELMAIL:
    case SEC_KEEPMAIL:
      if (arg) {
        if (d->edit_mode == SEC_DELMAIL && *arg == '*') {
          char query_buf[500] = {0};
          snprintf(query_buf, sizeof(query_buf), "DELETE FROM pfiles_mail WHERE recipient=%ld AND is_protected=0;", GET_IDNUM(CH));
          if (mysql_wrapper(mysql, query_buf)) {
            send_to_char(CH, "Sorry, your mail isn't working right now.\r\n");
            mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: failed to delete all mail for %s", GET_CHAR_NAME(CH));
            pocketsec_menu(d);
            return;
          }
        } else {
          char query_buf[500] = {0};
          MYSQL_RES *res;
          MYSQL_ROW row;

          int offset = atoi(arg);
          if (offset < 0) {
            send_to_char(CH, "That mailpiece does not exist.\r\n");
            pocketsec_mailmenu(d);
            return;
          }

          snprintf(query_buf, sizeof(query_buf), "SELECT idnum, is_protected FROM pfiles_mail WHERE recipient=%ld ORDER BY idnum DESC LIMIT 1 OFFSET %d;", GET_IDNUM(CH), offset);
          if (mysql_wrapper(mysql, query_buf)) {
            send_to_char(CH, "Sorry, your mail isn't working right now.\r\n");
            mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: failed to query mail for %s", GET_CHAR_NAME(d->character));
            pocketsec_mailmenu(d);
            return;
          }

          if (!(res = mysql_use_result(mysql))) {
            send_to_char(CH, "Sorry, your mail isn't working right now.\r\n");
            mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: failed to select mail index entry for %s", GET_CHAR_NAME(d->character));
            pocketsec_mailmenu(d);
            return;
          }

          if (!(row = mysql_fetch_row(res)) && mysql_field_count(mysql)) {
            send_to_char(CH, "That mailpiece does not exist.\r\n");
            pocketsec_mailmenu(d);
            mysql_free_result(res);
            return;
          }

          if (d->edit_mode == SEC_DELMAIL) {
            if (*(row[1]) == '1') {
              send_to_char(d->character, "That mailpiece has been marked as 'kept' and can't be deleted.\r\n");
              pocketsec_mailmenu(d);
              mysql_free_result(res);
              return;
            }
  
            // Entry exists, delete it.
            snprintf(query_buf, sizeof(query_buf), "DELETE FROM pfiles_mail WHERE idnum=%s;", row[0]);
            mysql_free_result(res);
  
            if (mysql_wrapper(mysql, query_buf)) {
              send_to_char(d->character, "Sorry, your mail isn't working right now.\r\n");
              mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: failed to delete mail record for %s", GET_CHAR_NAME(d->character));
              pocketsec_mailmenu(d);
              return;
            }
          } else {
            snprintf(query_buf, sizeof(query_buf), "UPDATE pfiles_mail SET is_protected=%d WHERE idnum=%s;", *(row[1]) == '1' ? 0 : 1, row[0]);
            mysql_free_result(res);
  
            if (mysql_wrapper(mysql, query_buf)) {
              send_to_char(d->character, "Sorry, your mail isn't working right now.\r\n");
              mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: failed to keep-mark mail record for %s", GET_CHAR_NAME(d->character));
              pocketsec_mailmenu(d);
              return;
            }
          }
        }
      }

      pocketsec_mailmenu(d);
      break;
    case SEC_SENDMAIL:
      one_argument(arg, buf);
      if ((x = get_player_id(buf)) < 0) {
        send_to_char("There is no such player.\r\n", CH);
        pocketsec_mailmenu(d);
      } else {
        send_to_char("Write your message. Use @ on a new line to finish.\r\n", CH);
        PLR_FLAGS(CH).SetBits(PLR_MAILING, PLR_WRITING, ENDBIT);
        d->mail_to = x;
        DELETE_D_STR_IF_EXTANT(d);
        INITIALIZE_NEW_D_STR(d);
        d->max_str = MAX_MAIL_SIZE;
      }
      break;
    case SEC_READMAIL:
      {
        char query_buf[MAX_INPUT_LENGTH + 500] = {0};
        MYSQL_RES *res;
        MYSQL_ROW row;

        int offset = atoi(arg);
        if (offset < 0) {
          send_to_char("That entry does not exist.\r\n", CH);
          pocketsec_mailmenu(d);
          return;
        }
        
        snprintf(query_buf, sizeof(query_buf), "SELECT sender_name, timestamp, text, idnum FROM pfiles_mail WHERE recipient=%ld ORDER BY idnum DESC LIMIT 1 OFFSET %d;", GET_IDNUM(CH), offset);
        if (mysql_wrapper(mysql, query_buf)) {
          send_to_char(d->character, "Sorry, your mail isn't working right now.\r\n");
          mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: failed to query mail for %s", GET_CHAR_NAME(d->character));
          pocketsec_mailmenu(d);
          return;
        }

        if (!(res = mysql_use_result(mysql))) {
          send_to_char(CH, "Sorry, your mail isn't working right now.\r\n");
          mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: failed to select mail index entry for %s", GET_CHAR_NAME(d->character));
          pocketsec_mailmenu(d);
          return;
        }

        if (!(row = mysql_fetch_row(res)) && mysql_field_count(mysql)) {
          send_to_char(CH, "That mailpiece does not exist.\r\n");
          pocketsec_mailmenu(d);
          mysql_free_result(res);
          return;
        }

        // display mail
        if (str_str(row[2], "^W * * * * Shadowland Mail System * * * *")) {
          // Some of our mail is loaded up from old formatted files. For these, we do no formatting and just print them.
          send_to_char(CH, "%s\r\n", row[2]);
        } else {
          // Everything else is formatted when printed.
          send_to_char(CH, "^W * * * * Shadowland Mail System * * * *\r\n"
                      "^B      To: %s^n\r\n"
                      "^C    From: %s^n\r\n"
                      "^mOOC Date: %s^n\r\n"
                      "\r\n"
                      "%s\r\n",
                      GET_CHAR_NAME(CH),
                      row[0],
                      row[1],
                      row[2]);
        }

        // Mark it as read.
        snprintf(query_buf, sizeof(query_buf), "UPDATE pfiles_mail SET is_read=1 WHERE idnum=%s;", row[3]);

        // Free the result before you execute the query
        mysql_free_result(res);

        if (mysql_wrapper(mysql, query_buf)) {
          mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: failed to mark mail as read for %s", GET_CHAR_NAME(d->character));
        }

        d->edit_mode = SEC_READMAIL2;
        send_to_char("Press [^cENTER^n] to continue.\r\n", CH);
      }
      break;
    case SEC_READMAIL2:
      pocketsec_mailmenu(d);
      break;
    case SEC_INIT:
      if (LOWER(*arg) == 'y') {
        initialize_pocket_secretary(SEC);
        send_to_char("Pocket Secretary Initialised.\r\n", CH);
        pocketsec_menu(d);
      } else {
        STATE(d) = CON_PLAYING;
        act("$n looks up from $s $p.", TRUE, CH, SEC, 0, TO_ROOM);
        send_to_char("Okay, Terminating Session.\r\n", CH);
        d->edit_obj = NULL;
      }
      break;
  }
}

char * write_phonebook_entry_migration_query(struct obj_data *entry, idnum_t owner, char *dest_buf, size_t dest_buf_sz) {
  char pq_note[MAX_STRING_LENGTH] = {0};

  mudlog_vfprintf(NULL, LOG_SYSLOG, "Note: New-migrating phonebook entry '%s'@'%s' to database for %ld.", entry->restring, entry->photo, owner);

  // Remove any dashes.
  char without_dashes[MAX_INPUT_LENGTH + 5] = {0};
  char *write_ptr = without_dashes;
  for (const char *read_ptr = entry->photo; *read_ptr; read_ptr++) {
    if (isdigit(*read_ptr))
      *write_ptr++ = *read_ptr;
  }
  *write_ptr = '\0';

  snprintf(dest_buf, dest_buf_sz, "INSERT INTO pocsec_phonebook (idnum, phonenum, note) VALUES (%ld, %d, '%s')",
           owner,
           atoi(without_dashes),
           prepare_quotes(pq_note, entry->restring, sizeof(pq_note)));

  return dest_buf;
}

void test_phonebook_entry_migration_query_generation() {
  struct obj_data dummy_obj;
  dummy_obj.restring = str_dup("tester");
  dummy_obj.photo = str_dup("0545-2849");

  assert(!str_cmp(
    write_phonebook_entry_migration_query(&dummy_obj, 123, buf3, sizeof(buf3)),
    "INSERT INTO pocsec_phonebook (idnum, phonenum, note) VALUES (123, 5452849, 'tester')"
  ));

  delete [] dummy_obj.restring;
  delete [] dummy_obj.photo;
}

char * write_mail_entry_migration_query(struct obj_data *entry, idnum_t owner, char *dest_buf, size_t dest_buf_sz) {
  char pq_restring[MAX_STRING_LENGTH] = {0};
  char pq_photo[MAX_STRING_LENGTH] = {0};

  mudlog_vfprintf(NULL, LOG_SYSLOG, "Note: Migrating mailpiece from '%s' to database for %ld.", entry->restring, owner);

  snprintf(dest_buf, dest_buf_sz, "INSERT INTO pfiles_mail (sender_name, recipient, text, is_received, is_read, is_protected) VALUES ('%s', %ld, '%s', 1, %d, %d)",
           prepare_quotes(pq_restring, entry->restring, sizeof(pq_restring)),
           owner,
           prepare_quotes(pq_photo, entry->photo, sizeof(pq_photo)),
           GET_OBJ_VAL(entry, 0) ? 1 : 0,
           GET_OBJ_TIMER(entry) < 0 ? 1 : 0
          );
  
  return dest_buf;
}

SPECIAL(pocket_sec);
void migrate_pocket_secretary_contents(struct obj_data *obj, idnum_t owner) {
  if (GET_OBJ_SPEC(obj) == pocket_sec) {
    for (struct obj_data *folder = obj->contains; folder; folder = folder->next_content) {
      // Migrate phonebook.
      if (!str_cmp(folder->restring, POCSEC_FOLDER_PHONEBOOK)) {
        while (folder->contains) {
          mysql_query(mysql, write_phonebook_entry_migration_query(folder->contains, owner, buf3, sizeof(buf3)));
          obj_from_obj(folder->contains);
          extract_obj(folder->contains);
        }
      }

      // Migrate mail.
      if (!str_cmp(folder->restring, POCSEC_FOLDER_MAIL)) {
        while (folder->contains) {
          mysql_query(mysql, write_mail_entry_migration_query(folder->contains, owner, buf3, sizeof(buf3)));
          obj_from_obj(folder->contains);
          extract_obj(folder->contains);
        }
      }

      // Migrate notes.
      if (!str_cmp(folder->restring, POCSEC_FOLDER_NOTES)) {
        while (folder->contains) {
          // struct obj_data *contents = folder->contains;
          // todo
          break;
        }
      }

      // Warn on files.
      if (!str_cmp(folder->restring, POCSEC_FOLDER_FILES)) {
        if (folder->contains) {
          mudlog_vfprintf(NULL, LOG_SYSLOG, "Warning: Got pocket secretary %s (%ld / %ld) with old-style files. Example: %s / %s",
                          GET_OBJ_NAME(obj),
                          GET_OBJ_VNUM(obj),
                          GET_OBJ_IDNUM(obj),
                          GET_OBJ_NAME(folder->contains),
                          GET_OBJ_DESC(folder->contains)
                         );
        }
      }
    }
  }
}
