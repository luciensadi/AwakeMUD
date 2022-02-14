/* ************************************************************************
*   File: act.social.c                                  Part of CircleMUD *
*  Usage: Functions to handle socials                                     *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "awake.h"
#include "ignore_system.h"

/* extern variables */
extern struct room_data *world;
extern struct index_data *mob_index;
extern struct descriptor_data *descriptor_list;

/* local globals */
static int list_top = -1;

struct social_messg
{
  int act_nr;
  int hide;
  int min_victim_position;      /* Position of victim */

  /* No argument was supplied */
  char *char_no_arg;
  char *others_no_arg;

  /* An argument was there, and a victim was found */
  char *char_found;             /* if NULL, read no further, ignore args */
  char *others_found;
  char *vict_found;

  /* An argument was there, but no victim was found */
  char *not_found;

  /* The victim turned out to be the character */
  char *char_auto;
  char *others_auto;
} *soc_mess_list = NULL;


int find_action(int cmd)
{
  for (int x = 0; x < list_top; x++)
    if (soc_mess_list[x].act_nr == cmd)
      return x;
  return -1;
}

ACMD(do_action)
{
  int act_nr;
  struct social_messg *action;
  struct char_data *vict;

  if ((act_nr = find_action(cmd)) < 0) {
    send_to_char("That action is not supported.\r\n", ch);
    return;
  }
  action = &soc_mess_list[act_nr];

  if (action->char_found)
    one_argument(argument, buf);
  else
    *buf = '\0';

  if (!*buf) {
    send_to_char(action->char_no_arg, ch);
    send_to_char("\r\n", ch);
    act(action->others_no_arg, action->hide, ch, 0, 0, TO_ROOM);
    return;
  }
  if (!(vict = get_char_room_vis(ch, buf))) {
    send_to_char(action->not_found, ch);
    send_to_char("\r\n", ch);
  } else if (vict == ch) {
    send_to_char(action->char_auto, ch);
    send_to_char("\r\n", ch);
    act(action->others_auto, action->hide, ch, 0, 0, TO_ROOM);
  } else {
    if (GET_POS(vict) < action->min_victim_position)
      act("$N is not in a proper position for that.",
          FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
    else {
      act(action->char_found, 0, ch, 0, vict, TO_CHAR | TO_SLEEP | TO_REMOTE);
      act(action->others_found, action->hide, ch, 0, vict, TO_NOTVICT | TO_REMOTE);

      if (!IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
        act(action->vict_found, action->hide, ch, 0, vict, TO_VICT);
      }
    }
  }
}

void perform_wizsocial(char *orig, struct char_data * ch, struct char_data *vict,
                       struct char_data * to)
{
  const char *i = NULL;
  char *buf;
  static char lbuf[MAX_STRING_LENGTH];

  buf = lbuf;

  for (;;)
  {
    if (*orig == '$') {
      switch (*(++orig)) {
      case 'n':
        i = WPERS(ch, to);
        break;
      case 'N':
        if (vict)
          i = WPERS(vict, to);
        else
          i = "NULL";
        break;
      case 'm':
        i = HMHR(ch);
        break;
      case 'M':
        if (vict)
          i = HMHR(vict);
        else
          i = "NULL";
        break;
      case 's':
        i = HSHR(ch);
        break;
      case 'S':
        if (vict)
          i = HSHR(vict);
        else
          i = "NULL";
        break;
      case 'e':
        i = HSSH(ch);
        break;
      case 'E':
        if (vict)
          i = HSSH(vict);
        else
          i = "NULL";
        break;
      case '$':
        i = "$";
        break;
      default:
        log_vfprintf("SYSERR: Illegal $-code to wf():\nSYSERR: %s", orig);
        break;
      }
      while ((*buf = *(i++)))
        buf++;
      orig++;
    } else if (!(*(buf++) = *(orig++)))
      break;
  }

  *(--buf) = '\r';
  *(++buf) = '\n';
  *(++buf) = '\0';

  SEND_TO_Q(CAP(lbuf), to->desc);
}

ACMD(do_wizfeel)
{
  int act_nr, length, cmdnum;
  struct social_messg *action;
  struct char_data *vict = NULL;
  char text1[256], text2[256], text3[256];
  struct descriptor_data *d;
  bool emote = FALSE;

  if (!*argument) {
    send_to_char("Don't bother the gods like that!\r\n", ch);
    return;
  }

  half_chop(argument, buf, buf1);

  if (!isalpha(*buf))
    *buf = '\0';

  /* otherwise, find the command */
  for (length = strlen(buf), cmdnum = 0; *cmd_info[cmdnum].command != '\n'; cmdnum++)
    if (!strncmp(cmd_info[cmdnum].command, buf, length))
      if (access_level(ch, cmd_info[cmdnum].minimum_level))
        break;

  if ((act_nr = find_action(cmdnum)) < 0) {
    if (is_abbrev(buf, "emote"))
      emote = TRUE;
    else {
      send_to_char("^c[]: That action is not supported.^n\r\n", ch);
      return;
    }
  }

  if (emote) {
    if (!*buf1) {
      send_to_char(ch, "^c[]: Yes, but what?!^n\r\n");
      return;
    }

    if ( buf1[0] == '\'' && buf1[1] == 's' ) {
      snprintf(text1, sizeof(text1), "^c[]: $n%s^n", buf1);
      snprintf(text2, sizeof(text2), "^c[]: $n%s^n", buf1);
    } else {
      snprintf(text1, sizeof(text1), "^c[]: $n %s^n", buf1);
      snprintf(text2, sizeof(text2), "^c[]: $n %s^n", buf1);
    }

  } else {
    action = &soc_mess_list[act_nr];

    if (action->char_found)
      one_argument(buf1, buf2);
    else
      *buf2 = '\0';
    if (!*buf2) {
      snprintf(text1, sizeof(text1), "^c[]: %s^n", (action->char_no_arg ? action->char_no_arg : ""));
      snprintf(text2, sizeof(text2), "^c[]: %s^n", (action->others_no_arg ? action->others_no_arg : ""));
    } else {
      for (d = descriptor_list; d && !vict; d = d->next)
        if (d->character &&
            isname(buf2, GET_KEYWORDS(d->character)) &&
            !d->connected &&
            IS_SENATOR(d->character) &&
            CAN_SEE(ch, d->character))
          vict = d->character;
      if (!vict) {
        send_to_char(ch, "^c[]: %s^n\r\n", (action->not_found ? action->not_found : ""));
        return;
      } else if (vict == ch) {
        snprintf(text1, sizeof(text1), "^c[]: %s^n", (action->char_auto ? action->char_auto : ""));
        snprintf(text2, sizeof(text2), "^c[]: %s^n", (action->others_auto ? action->others_auto : ""));
      } else {
        snprintf(text1, sizeof(text1), "^c[]: %s^n", (action->char_found ? action->char_found : ""));
        snprintf(text2, sizeof(text2), "^c[]: %s^n", (action->others_found ? action->others_found : ""));
        snprintf(text3, sizeof(text3), "^c[]: %s^n", (action->vict_found ? action->vict_found : ""));
      }
    }
  }
  if (strlen(text1) > 9)
    perform_wizsocial(text1, ch, vict, ch);

  for (d = descriptor_list; d; d = d->next)
    if ((!d->connected)
        && d->character
        && access_level(d->character, LVL_BUILDER)
        && (!PLR_FLAGS(d->character).AreAnySet(PLR_WRITING, PLR_MAILING, PLR_EDITING, ENDBIT))
        && (d != ch->desc)) {
      if (vict && vict == d->character && strlen(text3) > 9)
        perform_wizsocial(text3, ch, vict, vict);
      else if (strlen(text2) > 9)
        perform_wizsocial(text2, ch, vict, d->character);
    }
}

ACMD(do_insult)
{
  struct char_data *victim;

  one_argument(argument, arg);

  if (*arg) {
    if (!(victim = get_char_room_vis(ch, arg)))
      send_to_char("Can't hear you!\r\n", ch);
    else {
      if (victim != ch) {
        send_to_char(ch, "You insult %s.\r\n", GET_NAME(victim));

        switch (number(0, 1)) {
        case 0:
          act("$n calls your mother a bitch!", FALSE, ch, 0, victim, TO_VICT);
          break;
        default:
          act("$n tells you to get lost!", FALSE, ch, 0, victim, TO_VICT);
          break;
        }                       /* end switch */

        act("$n insults $N.", TRUE, ch, 0, victim, TO_NOTVICT);
      } else {                  /* ch == victim */
        send_to_char("You feel insulted.\r\n", ch);
      }
    }
  } else
    send_to_char("I'm sure you don't want to insult *everybody*...\r\n", ch);
}

char *fread_action(FILE * fl, int nr)
{
  char *rslt;

  fgets(buf, MAX_STRING_LENGTH, fl);
  if (feof(fl)) {
    fprintf(stderr, "fread_action - unexpected EOF near action #%d", nr);
    shutdown();
  }
  if (*buf == '#')
    return (NULL);
  else {
    *(buf + strlen(buf) - 1) = '\0';
    rslt = new char[strlen(buf) + 1];
    strcpy(rslt, buf);
    return (rslt);
  }
}


void boot_social_messages(void)
{
  FILE *fl;
  int nr, hide, min_pos, curr_soc = -1;
  char next_soc[250];

  /* open social file */
  if (!(fl = fopen(SOCMESS_FILE, "r"))) {
    snprintf(buf, sizeof(buf), "Can't open socials file '%s'", SOCMESS_FILE);
    perror(buf);
    shutdown();
  }
  /* count socials & allocate space */
  for (nr = 0; *cmd_info[nr].command != '\n'; nr++)
    if (cmd_info[nr].command_pointer == do_action)
      list_top++;

  soc_mess_list = new struct social_messg[list_top+6];

  /* now read 'em */
  for (;;) {
    fscanf(fl, " %s ", next_soc);
    if (*next_soc == '$')
      break;
    if ((nr = find_command(next_soc)) < 0)
      log_vfprintf("Unknown social '%s' in social file", next_soc);

    if (fscanf(fl, " %d %d \n", &hide, &min_pos) != 2) {
      fprintf(stderr, "Format error in social file near social '%s'\n",
              next_soc);
      shutdown();
    }
    /* read the stuff */
    curr_soc++;
    soc_mess_list[curr_soc].act_nr = nr;
    soc_mess_list[curr_soc].hide = hide;
    soc_mess_list[curr_soc].min_victim_position = min_pos;

    soc_mess_list[curr_soc].char_no_arg = fread_action(fl, nr);
    soc_mess_list[curr_soc].others_no_arg = fread_action(fl, nr);
    soc_mess_list[curr_soc].char_found = fread_action(fl, nr);

    /* if no char_found, the rest is to be ignored */
    if (!soc_mess_list[curr_soc].char_found)
      continue;

    soc_mess_list[curr_soc].others_found = fread_action(fl, nr);
    soc_mess_list[curr_soc].vict_found = fread_action(fl, nr);
    soc_mess_list[curr_soc].not_found = fread_action(fl, nr);
    soc_mess_list[curr_soc].char_auto = fread_action(fl, nr);
    soc_mess_list[curr_soc].others_auto = fread_action(fl, nr);
  }

  /* close file & set top */
  fclose(fl);
  list_top = curr_soc;

  // Why tho? --LS
  // soc_mess_list[0] = soc_mess_list[1];
}
