#include <time.h>

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "dblist.h"
#include "constants.h"
#include "newmatrix.h"
#include "memory.h"
#include "newdb.h"
#include "newmail.h"

// Ensure we have enough space for the query cruft plus a fully-quoted mail.
char mail_query_buf[300 + (MAX_MAIL_SIZE * 2 + 1)];

MYSQL_RES *res;
MYSQL_ROW row;

// For pocket secretary notifications.
SPECIAL(pocket_sec);

void store_mail(long to, struct char_data *from, const char *message_pointer) {
  if (to < 1) {
    log_vfprintf("SYSERR: Invalid 'to' value passed to store_mail: %ld (must be >= 1).", to);
    return;
  }
  
  if (!from) {
    log("SYSERR: Null character passed to store_mail.");
    return;
  }
  
  if (IS_NPC(from)) {
    log_vfprintf("SYSERR: NPC '%s' attempted to use mail system. NPCs can't send mail.", GET_CHAR_NAME(from));
    send_to_char("NPCs can't send mail, so your mail was rejected.\r\n", from);
    return;
  }
  
  if (message_pointer == NULL) {
    log("SYSERR: Null message_pointer passed to store_mail.");
    return;
  }
  
  raw_store_mail(to, GET_IDNUM(from), GET_CHAR_NAME(from), message_pointer);
}

void raw_store_mail(long to, long from_id, const char *from_name, const char *message_pointer) {
  // Note: We trust that whoever is calling this has already validated that the message fits in the query buffer.
  
  char prepared_quote_buf[2 * strlen(message_pointer) + 1];
  prepare_quotes(prepared_quote_buf, message_pointer, sizeof(prepared_quote_buf) / sizeof(prepared_quote_buf[0]));
  
  snprintf(mail_query_buf, sizeof(mail_query_buf), "INSERT INTO pfiles_mail (sender_id, sender_name, recipient, text) VALUES (%ld, '%s', %ld, '%s');",
          from_id,
          from_name,
          to,
          prepared_quote_buf);
  
  mysql_wrapper(mysql, mail_query_buf);
  
  // Notify pocket secretaries of online characters.
  for (struct descriptor_data *desc = descriptor_list; desc; desc = desc->next)
    if (desc->character && GET_IDNUM(desc->character) == to)
      for (struct obj_data *obj = desc->character->carrying; obj; obj = obj->next_content)
        if (GET_OBJ_SPEC(obj) && GET_OBJ_SPEC(obj) == pocket_sec && obj->contains)
          send_to_char(desc->character, "%s beeps.\r\n", GET_OBJ_NAME(obj));
}

int amount_of_mail_waiting(struct char_data *ch) {
  int amount = 0;
  
  if (!ch || IS_NPC(ch)) {
    log("SYSERR: Bad character or NPC attempted to use mail system.");
    return 0;
  }
  
  // Grab the character's mail count from DB.
  snprintf(mail_query_buf, sizeof(mail_query_buf), "SELECT COUNT(*) FROM pfiles_mail WHERE recipient = %ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, mail_query_buf);
  
  res = mysql_use_result(mysql);
  row = mysql_fetch_row(res);
  
  /* On failure, bail out. */
  if (!row && mysql_field_count(mysql)) {
    mysql_free_result(res);
    return 0;
  }
  
  amount = atoi(row[0]);
  
  mysql_free_result(res);
  
  return amount;
}


char *get_and_delete_one_message(struct char_data *ch, char *sender) {
  if (!ch || IS_NPC(ch)) {
    log("SYSERR: Bad character or NPC attempted to use mail system.");
    return 0;
  }
  
  // Grab the character's mail from DB.
  snprintf(mail_query_buf, sizeof(mail_query_buf), "SELECT * FROM pfiles_mail WHERE recipient = %ld LIMIT 1;", GET_IDNUM(ch));
  mysql_wrapper(mysql, mail_query_buf);
  
  res = mysql_use_result(mysql);
  row = mysql_fetch_row(res);
  
  /* On failure, bail out. */
  if (!row && mysql_field_count(mysql)) {
    log("SYSERR: get_and_delete_one_message failed on SQL query.");
    mysql_free_result(res);
    return 0;
  }
  
  // Write the mail header to buf.
  snprintf(buf, sizeof(buf), "^W * * * * Shadowland Mail System * * * *\r\n"
          "^B      To: %s^n\r\n"
          "^C    From: %s^n\r\n"
          "^mOOC Date: %s^n\r\n"
          "\r\n"
          "%s",
          GET_CHAR_NAME(ch),
          row[MAIL_SQL_SENDER_NAME],
          row[MAIL_SQL_TIMESTAMP],
          row[MAIL_SQL_TEXT]);
  
  // Fill out the sender field for the caller.
  strcpy(sender, row[MAIL_SQL_SENDER_NAME]);
  
  // Prepare the query for deleting the mail. Could have passed as a string, but atoi() guarantees no SQL shenanigans.
  snprintf(mail_query_buf, sizeof(mail_query_buf), "DELETE FROM pfiles_mail WHERE idnum = %d;", atoi(row[MAIL_SQL_IDNUM]));
  
  // Free DB resources so we can execute the deletion query.
  mysql_free_result(res);
  
  // Delete the message we just read.
  mysql_wrapper(mysql, mail_query_buf);
  
  return buf;
}

/*******************************************************************
 ** Below is the spec_proc for a postmaster using the above       **
 ** routines.  Written by Jeremy Elson (jelson@server.cs.jhu.edu) **
 *******************************************************************/

SPECIAL(postmaster)
{
  if (!ch->desc || IS_NPC(ch))
    return FALSE;                   /* so mobs don't get caught here */
  
  if (!(CMD_IS("mail") || CMD_IS("check") || CMD_IS("receive")))
    return FALSE;
  
  if (CMD_IS("mail")) {
    postmaster_send_mail(ch, (struct char_data *)me, cmd, argument);
    return TRUE;
  }
  else if (CMD_IS("check")) {
    postmaster_check_mail(ch, (struct char_data *)me, cmd, argument);
    return TRUE;
  }
  else if (CMD_IS("receive")) {
    postmaster_receive_mail(ch, (struct char_data *)me, cmd, argument);
    return TRUE;
  } else {
    return FALSE;
  }
}

void postmaster_send_mail(struct char_data * ch, struct char_data *mailman, int cmd, char *arg) {
  long recipient;
  char buf[256];
  
  /* Require that the user be of the right level to use the mail system. */
  if (!access_level(ch, MIN_MAIL_LEVEL)) {
    snprintf(buf, sizeof(buf), "$n tells you, 'Sorry, you have to be level %d to send mail!'", MIN_MAIL_LEVEL);
    act(buf, FALSE, mailman, 0, ch, TO_VICT);
    return;
  }
  
  /* Keep astral beings from using mail. */
  if (IS_ASTRAL(ch)) {
    send_to_char("If the postal employee could detect the astral plane you still would be ignored!\n\r",ch);
    return;
  }
  one_argument(arg, buf);
  
  /* Check to ensure that an addressee has been specified. */
  if (!*buf) {                  /* you'll get no argument from me! */
    act("$n tells you, 'You need to specify an addressee!'", FALSE, mailman, 0, ch, TO_VICT);
    return;
  }
  
  /* Check to ensure that the addressee exists. */
  if ((recipient = get_player_id(buf)) < 0) {
    act("$n tells you, 'No one by that name is registered here!'", FALSE, mailman, 0, ch, TO_VICT);
    return;
  }
  
  /* Put the character in edit mode and inform the room. */
  act("$n starts to write some mail.", TRUE, ch, 0, 0, TO_ROOM);
  snprintf(buf, sizeof(buf), "$n tells you, 'Write your message, use @ on a new line when done.'");
  act(buf, FALSE, mailman, 0, ch, TO_VICT);
  PLR_FLAGS(ch).SetBits(PLR_MAILING, PLR_WRITING, ENDBIT);
  
  /* Set up their temporary mail data. */
  ch->desc->mail_to = recipient;
  ch->desc->max_str = MAX_MAIL_SIZE;
  ch->desc->str = new (char *);
  
  /* Ensure the setup completed properly. This will only fail if we're out of memory. */
  if (!ch->desc->str) {
    mudlog("New failure!", NULL, LOG_SYSLOG, TRUE);
    exit(1);
  }
  
  /* Set the first character of their mail string to 0. */
  *(ch->desc->str) = NULL;
}

void postmaster_check_mail(struct char_data * ch, struct char_data *mailman, int cmd, char *arg) {
  int amount;
  if ((amount = amount_of_mail_waiting(ch)) > 0)
    snprintf(buf, sizeof(buf), "$n tells you, 'You have %d piece%s of mail waiting.'", amount, amount > 1 ? "s" : "");
  else
    snprintf(buf, sizeof(buf), "$n tells you, 'Sorry, you don't have any mail waiting.'");
  
  act(buf, FALSE, mailman, 0, ch, TO_VICT);
}

void postmaster_receive_mail(struct char_data * ch, struct char_data *mailman, int cmd, char *arg) {
  char sender[150];
  struct obj_data *obj;
  int mail_given = 0;
  
  if (amount_of_mail_waiting(ch) == 0)
  {
    snprintf(buf, sizeof(buf), "$n tells you, 'Sorry, you don't have any mail waiting.'");
    act(buf, FALSE, mailman, 0, ch, TO_VICT);
    return;
  }
  
  while (amount_of_mail_waiting(ch) > 0)
  {
    obj = read_object(OBJ_PIECE_OF_MAIL, VIRTUAL);
    obj->photo = str_dup(get_and_delete_one_message(ch, sender));
    
    if (obj->photo == NULL || *obj->photo == '\0')
      obj->photo = str_dup("Mail system error - please report.  Error #11.\r\n");
    
    obj_to_char(obj, ch);
    mail_given += 1;
  }
  
  // Consolidate messaging so people who haven't been around for a while don't get walls of mail spam.
  if (mail_given == 1) {
    act("$n gives you a piece of mail.", FALSE, mailman, 0, ch, TO_VICT);
    act("$N gives $n a piece of mail.", FALSE, ch, 0, mailman, TO_ROOM);
  } else if (mail_given > 1) {
    act("$n gives you several pieces of mail.", FALSE, mailman, 0, ch, TO_VICT);
    act("$N gives $n several pieces of mail.", FALSE, ch, 0, mailman, TO_ROOM);
  }
}
