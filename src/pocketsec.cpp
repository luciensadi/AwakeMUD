#include "structs.h"
#include "awake.h"
#include "db.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "utils.h"
#include "screen.h"
#include "constants.h"
#include "olc.h"
#include "mail.h"
#include "newdb.h"
#include "memory.h"
#include <mysql/mysql.h>

extern MYSQL *mysql;
extern int mysql_wrapper(MYSQL *mysql, const char *buf);

#define CH d->character
#define SEC d->edit_obj
#define SEC_MENU		0
#define SEC_INIT		1
#define SEC_MAILMENU		2
#define SEC_READMAIL		3
#define SEC_DELMAIL		4
#define SEC_SENDMAIL		5
#define SEC_READMAIL2		6
#define SEC_BANKMENU		7
#define SEC_WIRE1		8
#define SEC_WIRE2		9
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
#define SEC_NOTEDEL		20

ACMD(do_phone);

void wire_nuyen(struct char_data *ch, struct char_data *targ, int amount, long isfile)
{
  GET_BANK(ch) -= amount;
  if (isfile) {
    sprintf(buf, "UPDATE pfiles SET Bank=Bank+%d WHERE idnum=%ld;", amount, isfile);
    mysql_wrapper(mysql, buf);
    GET_EXTRA(ch) = 0;
  } else
    GET_BANK(targ) += amount;
  sprintf(buf, "%s has wired %d nuyen to your account.\r\n", GET_CHAR_NAME(ch), amount);
  store_mail(targ ? GET_IDNUM(targ) : isfile, GET_IDNUM(ch), buf);
  char *player_name = NULL;
  sprintf(buf, "%s wired %d nuyen to %s.", GET_CHAR_NAME(ch), amount, targ ? GET_CHAR_NAME(targ) : (player_name = get_player_name(isfile)));
  DELETE_ARRAY_IF_EXTANT(player_name);
  mudlog(buf, ch, LOG_GRIDLOG, TRUE);
}

void pocketsec_phonemenu(struct descriptor_data *d)
{
  struct obj_data *data = NULL, *folder = SEC->contains;
  int i = 0;
  for (; folder; folder = folder->next_content)
    if (!strcmp(folder->restring, "Phonebook"))
      break;
  CLS(CH);
  send_to_char(CH, "^LYour Phonebook^n\r\n");
  for (data = folder->contains; data; data = data->next_content) {
    i++;
    send_to_char(CH, " %2d > %-20s - %s\r\n", i, GET_OBJ_NAME(data), GET_OBJ_DESC(data));
  }
  send_to_char("\r\n[^cC^n]^call^n     [^cA^n]^cdd Name^n     [^cD^n]^celete Name^n     [^cB^n]^cack^n\r\n", CH);
  d->edit_mode = SEC_PHONEMENU;
}

void pocketsec_notemenu(struct descriptor_data *d)
{
  struct obj_data *data = NULL, *folder = SEC->contains;
  int i = 0;
  for (; folder; folder = folder->next_content)
    if (!strcmp(folder->restring, "Notes"))
      break;
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
    send_to_char("This pocket secretary has not yet been initialised. Initialise? [Y/N]\r\n", CH);
    d->edit_mode = SEC_INIT;
  } else {
    send_to_char(CH, "^cMain Menu^n\r\n" 
                 "   [^c1^n]^c%sMail^n\r\n"
                 "   [^c2^n] ^cNotes^n\r\n"
                 "   [^c3^n] ^cPhonebook^n\r\n"
                 "   [^c4^n] ^cFiles^n\r\n"
                 "   [^c5^n] ^cBanking^n\r\n"
                 "   [^c6^n] ^c%sock^n\r\n"
                 "   [^c0^n] ^cQuit^n\r\n", has_mail(GET_IDNUM(CH)) ? " ^R" : " ", GET_OBJ_VAL(SEC, 1) ? "Unl" : "L");
    d->edit_mode = SEC_MENU;
  }
}

void pocketsec_mailmenu(struct descriptor_data *d)
{
  struct obj_data *mail = NULL, *folder = SEC->contains;
  char sender[30];
  int i = 0;

  for (; folder; folder = folder->next_content)
    if (!strcmp(folder->restring, "Mail"))
      break;
  while (has_mail(GET_IDNUM(CH))) {
    mail = read_object(111, VIRTUAL);
    mail->photo = read_delete(GET_IDNUM(CH), sender);
    
    if (*sender)
      mail->restring = str_dup(CAP(sender));
    else mail->restring = str_dup("Alert");

    if (mail->photo == NULL)
      mail->photo =
        str_dup("Mail system error - please report.  Error #11.\r\n");

    obj_to_obj(mail, folder);
  }
  CLS(CH);
  send_to_char(CH, "^LShadowland Mail Network^n\r\n");
  for (mail = folder->contains; mail; mail = mail->next_content) {
    i++;
    send_to_char(CH, " %2d >%s%s\r\n", i, !GET_OBJ_VAL(mail, 0) ? " ^R" : " ", GET_OBJ_NAME(mail));
  }
  send_to_char("\r\n[^cR^n]^cead Mail^n     [^cD^n]^celete Mail^n     [^cS^n]^cend mail^n     [^cB^n]^cack^n\r\n", CH);
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
  switch (d->edit_mode) {
    case SEC_MENU:
      switch (atoi(arg)) {
        case 0:
          send_to_char("Good bye.\r\n", CH);
          STATE(d) = CON_PLAYING;
          act("$n looks up from $s $p.", TRUE, CH, SEC, 0, TO_ROOM);
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
        case 5:
          pocketsec_bankmenu(d);
          break;
        case 6:
          if (!GET_OBJ_VAL(SEC, 1))
            GET_OBJ_VAL(SEC, 1) = GET_IDNUM(CH);
          else GET_OBJ_VAL(SEC, 1) = 0;
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
          send_to_char("Delete which entry?\r\n", CH);
          d->edit_mode = SEC_PHONEDEL;
          break;
        case 'b':
          pocketsec_menu(d);
          break;
      }
      break;
    case SEC_PHONECALL:
      for (folder = SEC->contains; folder; folder = folder->next_content)
        if (!strcmp(folder->restring, "Phonebook"))
          break;
      i = atoi(arg);
      for (file = folder->contains; file && i > 1; file = file->next_content)
        i--;
      if (file) {
        STATE(d) = CON_PLAYING;
        act("$n looks up from $s $p.", TRUE, CH, SEC, 0, TO_ROOM);
        d->edit_obj = NULL;
        do_phone(CH, GET_OBJ_DESC(file), 0, SCMD_RING); 
      } else {
        pocketsec_phonemenu(d);
        send_to_char("That entry does not exist.\r\n", CH);
      }
      break;
    case SEC_PHONEADD1:
      for (folder = SEC->contains; folder; folder = folder->next_content)
        if (!strcmp(folder->restring, "Phonebook"))
          break;
      file = read_object(118, VIRTUAL);       
      obj_to_obj(file, folder);
      file->restring = str_dup(arg);
      send_to_char("Enter Number: ", CH);
      d->edit_mode = SEC_PHONEADD2;
      break;
    case SEC_PHONEADD2:
      for (folder = SEC->contains; folder; folder = folder->next_content)
        if (!strcmp(folder->restring, "Phonebook"))
          break;
      folder->contains->photo = str_dup(arg);
      pocketsec_phonemenu(d);
      break;
    case SEC_PHONEDEL:
      for (folder = SEC->contains; folder; folder = folder->next_content)
        if (!strcmp(folder->restring, "Phonebook"))
          break;
      i = atoi(arg);  
      for (file = folder->contains; file && i > 1; file = file->next_content)
        i--;
      if (file)
        extract_obj(file);
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
          send_to_char("Delete which note?\r\n", CH);
          d->edit_mode = SEC_NOTEDEL;
          break;
        case 'b':
          pocketsec_menu(d);
          break;
      }
      break;
    case SEC_NOTEREAD:
      for (folder = SEC->contains; folder; folder = folder->next_content)
        if (!strcmp(folder->restring, "Notes"))
          break;
      i = atoi(arg);
      for (file = folder->contains; file && i > 1; file = file->next_content)
        i--;
      if (file) {
        send_to_char(CH, file->photo);
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
        if (!strcmp(folder->restring, "Notes"))
          break;
      file = read_object(118, VIRTUAL);       
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
        if (!strcmp(folder->restring, "Notes"))
          break;
      i = atoi(arg);  
      for (file = folder->contains; file && i > 1; file = file->next_content)
        i--;
      if (file)
        extract_obj(file);
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
      if (!(d->edit_mob = get_player_vis(CH, arg, FALSE)))
        if ((GET_EXTRA(CH) = get_player_id(arg)) == -1) {
          pocketsec_bankmenu(d);
          send_to_char("The bank refuses to let you wire to that account.\r\n", CH);
          break;
        }
      send_to_char("How much do you wish to transfer? (@ to cancel)\r\n", CH);
      d->edit_mode = SEC_WIRE2;
      break;
    case SEC_WIRE2:
      if (*arg != '@') {
        x = atoi(arg);
        if (x < 0)
          send_to_char("How much do you wish to transfer? (@ to cancel)\r\n", CH);
        else if (x > GET_BANK(CH))
          send_to_char("You do not have that much to transfer. (@ to cancel)\r\n", CH);
        else {
          wire_nuyen(CH, d->edit_mob, x, GET_EXTRA(CH));
          d->edit_mob = NULL;
          pocketsec_bankmenu(d);
        }
        return;
      }
      pocketsec_bankmenu(d);
      break;

    case SEC_MAILMENU:
      switch (LOWER(*arg)) {
        case 'r':
          send_to_char("Read which message?\r\n", CH);
          d->edit_mode = SEC_READMAIL;
          break;
        case 'd':
          send_to_char("Delete which message?\r\n", CH);
          d->edit_mode = SEC_DELMAIL;
          break;
        case 's':
          send_to_char("Send message to whom?\r\n", CH);
          d->edit_mode = SEC_SENDMAIL;
          break;
        case 'b':
          pocketsec_menu(d);
          break;
      }
      break;
    case SEC_DELMAIL:
      for (folder = SEC->contains; folder; folder = folder->next_content)
        if (!strcmp(folder->restring, "Mail"))
          break;
      i = atoi(arg);
      for (file = folder->contains; file && i > 1; file = file->next_content)
        i--;
      if (file)
        extract_obj(file);
      pocketsec_mailmenu(d);
      break;
    case SEC_SENDMAIL:
      one_argument(arg, buf); 
      if ((x = get_player_id(buf)) < 0)
        pocketsec_mailmenu(d);
      else {
        send_to_char("Write your message. Use @ on a new line to finish.\r\n", CH);
        PLR_FLAGS(CH).SetBits(PLR_MAILING, PLR_WRITING, ENDBIT);
        d->mail_to = x;
        DELETE_D_STR_IF_EXTANT(d);
        INITIALIZE_NEW_D_STR(d);
        d->max_str = MAX_MAIL_SIZE;
      }
      break;
    case SEC_READMAIL:
      for (folder = SEC->contains; folder; folder = folder->next_content)
        if (!strcmp(folder->restring, "Mail"))
          break;
      i = atoi(arg);
      for (file = folder->contains; file && i > 1; file = file->next_content)
        i--;
      if (file) {
        send_to_char(CH, file->photo);
        GET_OBJ_VAL(file, 0) = 1;
        d->edit_mode = SEC_READMAIL2;
        send_to_char("Press [^cENTER^n] to continue.\r\n", CH);
      }
      else {
        pocketsec_mailmenu(d);
        send_to_char("That mail does not exist.\r\n", CH);
      }
      break;
    case SEC_READMAIL2:
      pocketsec_mailmenu(d);
      break;
    case SEC_INIT:
      if (LOWER(*arg) == 'y') {
        folder = read_object(118, VIRTUAL);
        folder->restring = str_dup("Mail");
        obj_to_obj(folder, SEC);
        folder = read_object(118, VIRTUAL);
        folder->restring = str_dup("Notes");
        obj_to_obj(folder, SEC);
        folder = read_object(118, VIRTUAL);
        folder->restring = str_dup("Phonebook");
        obj_to_obj(folder, SEC);
        folder = read_object(118, VIRTUAL);
        folder->restring = str_dup("Files");
        obj_to_obj(folder, SEC);
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
