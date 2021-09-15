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
#include "newmail.h"
#include "newdb.h"
#include "memory.h"
#include "config.h"

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
#define SEC_KEEPMAIL    21

#define POCSEC_FOLDER_MAIL      "Mail"
#define POCSEC_FOLDER_NOTES     "Notes"
#define POCSEC_FOLDER_PHONEBOOK "Phonebook"
#define POCSEC_FOLDER_FILES     "Files"

ACMD_DECLARE(do_phone);

struct obj_data *generate_pocket_secretary_folder(struct obj_data *sec, const char *string) {
  struct obj_data *folder = read_object(OBJ_POCKET_SECRETARY_FOLDER, VIRTUAL);
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

void wire_nuyen(struct char_data *ch, int amount, vnum_t character_id)
{  
  // First, scan the game to see if the target character is online.
  struct char_data *targ = NULL;
  char query_buf[1000], name_buf[500];
  
  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    targ = d->original ? d->original : d->character;
    
    if (targ && GET_IDNUM(targ) == character_id)
      break;
    
    targ = NULL;
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
    snprintf(query_buf, sizeof(query_buf), "%s has wired %d nuyen to your account.\r\n", ch ? GET_CHAR_NAME(ch) : "Someone", amount);
    snprintf(name_buf, sizeof(name_buf), "%s (wire transfer)", ch ? GET_CHAR_NAME(ch) : "Someone");
    raw_store_mail(character_id, GET_IDNUM(ch), name_buf, query_buf);
  }
  
  // Log it.
  char *player_name = targ ? NULL : get_player_name(character_id);
  snprintf(query_buf, sizeof(query_buf), "%s wired %d nuyen to %s.", ch ? GET_CHAR_NAME(ch) : "An NPC", amount, targ ? GET_CHAR_NAME(targ) : player_name);
  DELETE_ARRAY_IF_EXTANT(player_name);
  mudlog(query_buf, ch, LOG_GRIDLOG, TRUE);
}

void pocketsec_phonemenu(struct descriptor_data *d)
{
  struct obj_data *data = NULL, *folder = SEC->contains;
  int i = 0;
  for (; folder; folder = folder->next_content)
    if (!strcmp(folder->restring, POCSEC_FOLDER_PHONEBOOK))
      break;
  CLS(CH);
  if (!folder) {
    send_to_char("Your phonebook is empty.", CH);
    mudlog("Prevented missing-phonebook crash. This player has lost their contacts list.", d->character, LOG_SYSLOG, TRUE);
    generate_pocket_secretary_folder(SEC, POCSEC_FOLDER_PHONEBOOK);
  } else {
    send_to_char(CH, "^LYour Phonebook^n\r\n");
    for (data = folder->contains; data; data = data->next_content) {
      i++;
      send_to_char(CH, " %2d > %-20s - %s\r\n", i, GET_OBJ_NAME(data), GET_OBJ_DESC(data));
    }
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
    send_to_char(CH, "^cMain Menu^n\r\n" 
                 "   [^c1^n]^c%sMail%s^n\r\n"
                 "   [^c2^n] ^cNotes^n\r\n"
                 "   [^c3^n] ^cPhonebook^n\r\n"
                 "   [^c4^n] ^cFiles^n\r\n"
                 "   [^c5^n] ^cBanking^n\r\n"
                 "   [^c6^n] ^c%sock^n\r\n"
                 "   [^c0^n] ^cQuit^n\r\n", 
                 amount_of_mail_waiting(CH) > 0 ? " ^R" : " ", 
                 amount_of_mail_waiting(CH) > 0 ? " (unread messages)" : "",
                 GET_OBJ_VAL(SEC, 1) ? "Unl" : "L");
    d->edit_mode = SEC_MENU;
  }
}

void pocketsec_mailmenu(struct descriptor_data *d)
{
  struct obj_data *mail = NULL, *folder = SEC->contains;
  char sender[150];
  int i = 0;

  for (; folder; folder = folder->next_content)
    if (!strcmp(folder->restring, POCSEC_FOLDER_MAIL))
      break;
      
  if (!folder) {
    mudlog("Prevented missing-mail crash. This player has lost their mail.", d->character, LOG_SYSLOG, TRUE);
    folder = generate_pocket_secretary_folder(SEC, POCSEC_FOLDER_MAIL);
  }
  
  while (amount_of_mail_waiting(CH) > 0) {
    mail = read_object(OBJ_PIECE_OF_MAIL, VIRTUAL);
    mail->photo = str_dup(get_and_delete_one_message(CH, sender));
    
    if (*sender)
      mail->restring = str_dup(CAP(sender));
    else mail->restring = str_dup("Alert");

    if (mail->photo == NULL)
      mail->photo = str_dup("Mail system error - please report.  Error #11.\r\n");

    obj_to_obj(mail, folder);
  }
  CLS(CH);
  send_to_char(CH, "^LShadowland Mail Network^n\r\n");
  int expiry_ticks, days, hours, minutes;
  for (mail = folder->contains; mail; mail = mail->next_content) {
    i++;
    
    // Compose our status string.
    snprintf(buf, sizeof(buf), " %2d > %s%s^n",
                  i,
                  !GET_OBJ_VAL(mail, 0) ? "(unread) ^R" : "",
                  GET_OBJ_NAME(mail)
                );
    
    if (GET_OBJ_TIMER(mail) >= 0) {
      expiry_ticks = MAIL_EXPIRATION_TICKS - GET_OBJ_TIMER(mail);
      minutes = expiry_ticks % 60;
      hours = expiry_ticks % (60 * 24);
      days = expiry_ticks / (60 * 24);
      
      if (days > 0)
        snprintf(buf2, sizeof(buf2), "%s\r\n", buf);
      else if (hours > 0 || minutes > 0)
        snprintf(buf2, sizeof(buf2), "%-30s [expires in %dh %dm]\r\n", buf, hours, minutes);
      else
        snprintf(buf2, sizeof(buf2), "%-30s [expiring at any moment!]\r\n", buf);
    } else {
      snprintf(buf2, sizeof(buf2), "%s (kept)\r\n", buf);
    }
    send_to_char(buf2, CH);
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
          send_to_char("Delete which entry? ('*' for all)\r\n", CH);
          d->edit_mode = SEC_PHONEDEL;
          break;
        case 'b':
          pocketsec_menu(d);
          break;
      }
      break;
    case SEC_PHONECALL:
      for (folder = SEC->contains; folder; folder = folder->next_content)
        if (!strcmp(folder->restring, POCSEC_FOLDER_PHONEBOOK))
          break;
          
      if (!folder) {
        mudlog("Prevented missing-phonebook crash. This player has lost their contacts list.", d->character, LOG_SYSLOG, TRUE);
        folder = generate_pocket_secretary_folder(SEC, POCSEC_FOLDER_PHONEBOOK);
      }
      
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
        if (!strcmp(folder->restring, POCSEC_FOLDER_PHONEBOOK))
          break;
          
      if (!folder) {
        mudlog("Prevented missing-phonebook crash. This player has lost their contacts list.", d->character, LOG_SYSLOG, TRUE);
        folder = generate_pocket_secretary_folder(SEC, POCSEC_FOLDER_PHONEBOOK);
      }
        
      file = read_object(OBJ_POCKET_SECRETARY_FOLDER, VIRTUAL);       
      obj_to_obj(file, folder);
      file->restring = str_dup(arg);
      send_to_char("Enter Number: ", CH);
      d->edit_mode = SEC_PHONEADD2;
      break;
    case SEC_PHONEADD2:
      for (folder = SEC->contains; folder; folder = folder->next_content)
        if (!strcmp(folder->restring, POCSEC_FOLDER_PHONEBOOK))
          break;
      if (!folder) {
        mudlog("Prevented missing-phonebook crash. This player has lost their contacts list.", d->character, LOG_SYSLOG, TRUE);
        folder = generate_pocket_secretary_folder(SEC, POCSEC_FOLDER_PHONEBOOK);
      }
      folder->contains->photo = str_dup(arg);
      pocketsec_phonemenu(d);
      break;
    case SEC_PHONEDEL:
      for (folder = SEC->contains; folder; folder = folder->next_content)
        if (!strcmp(folder->restring, POCSEC_FOLDER_PHONEBOOK))
          break;
      
      if (!folder) {
        mudlog("Prevented missing-phonebook crash. This player has lost their contacts list.", d->character, LOG_SYSLOG, TRUE);
        folder = generate_pocket_secretary_folder(SEC, POCSEC_FOLDER_PHONEBOOK);
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
      
      file = read_object(OBJ_POCKET_SECRETARY_FOLDER, VIRTUAL);       
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
          name = get_player_name(GET_EXTRA(CH));
          send_to_char(CH, "You wire %d nuyen to %s's account.\r\n", x, capitalize(name));
          delete [] name;
          wire_nuyen(CH, x, GET_EXTRA(CH));
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
      for (folder = SEC->contains; folder; folder = folder->next_content)
        if (!strcmp(folder->restring, POCSEC_FOLDER_MAIL))
          break;
          
      if (!folder) {
        mudlog("Prevented missing-mail crash. This player has lost their mail.", d->character, LOG_SYSLOG, TRUE);
        folder = generate_pocket_secretary_folder(SEC, POCSEC_FOLDER_MAIL);
      }
      
      if (arg) {
        if (d->edit_mode != SEC_KEEPMAIL && *arg == '*') {
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
          if (file) {
            if (d->edit_mode == SEC_KEEPMAIL) {
              if (GET_OBJ_TIMER(file) == -1)
                GET_OBJ_TIMER(file) = 0;
              else
                GET_OBJ_TIMER(file) = -1;
            } else {
              extract_obj(file);
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
      for (folder = SEC->contains; folder; folder = folder->next_content)
        if (!strcmp(folder->restring, POCSEC_FOLDER_MAIL))
          break;
          
      if (!folder) {
        mudlog("Prevented missing-mail crash. This player has lost their mail.", d->character, LOG_SYSLOG, TRUE);
        folder = generate_pocket_secretary_folder(SEC, POCSEC_FOLDER_MAIL);
      }
          
      i = atoi(arg);
      for (file = folder->contains; file && i > 1; file = file->next_content)
        i--;
      if (file) {
        send_to_char(file->photo, CH);
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
