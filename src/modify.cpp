/* ************************************************************************
*   File: modify.c                                      Part of CircleMUD *
*  Usage: Run-time modification of game variables                         *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <mysql/mysql.h>

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "comm.h"
#include "mail.h"
#include "boards.h"
#include "olc.h"
#include "quest.h"
#include "newmagic.h"
#include "newmatrix.h"

void show_string(struct descriptor_data *d, char *input);
void qedit_disp_menu(struct descriptor_data *d);

extern MYSQL *mysql;
extern int mysql_wrapper(MYSQL *mysql, const char *query);
extern char *prepare_quotes(char *dest, const char *str);
/* ************************************************************************
*  modification of malloc'ed strings                                      *
************************************************************************ */

int add_spaces(char *str, int size, int from, int spaces)
{
  int i;

  if (strlen(str) + spaces > (u_int)size)
    return 0;

  for (i = strlen(str) + spaces; i > from + spaces - 1; i--)
    str[i] = str[i-spaces];

  return 1;
}

void format_string(struct descriptor_data *d, int indent)
{
  int i, j, k, line;

  if (strlen(*d->str) >= 1023)
    return;
  // if the editor is an implementor and begins with the sequence
  // /**/, then we know not to format this string
  if (*(*d->str) == '/' &&
      *(*d->str + 1) == '*' && *(*d->str + 2) == '*' && *(*d->str + 3) == '/')
  {
    for (i = 0; i < (int)(strlen(*d->str) - 4); i++)
      (*d->str)[i] = (*d->str)[i+4];
    (*d->str)[strlen(*d->str) - 4] = '\0';
    return;
  }

  char *format = new char[d->max_str];

  for (i = 0; (*d->str)[i]; i++)
    while ((*d->str)[i] == '\r' || (*d->str)[i] == '\n')
    {
      if ((*d->str)[i] == '\n' && (*d->str)[i+1] != '\r')
        (*d->str)[i] = ' ';
      else
        for (j = i; (*d->str)[j]; j++)
          (*d->str)[j] = (*d->str)[j+1];
      if ((*d->str)[i] == '\r') {
        i += 2;
        if (indent && add_spaces(*d->str, d->max_str, i, 3)) {
          (*d->str)[i] = (*d->str)[i+1] = (*d->str)[i+2] = ' ';
          while ((*d->str)[i+3] == ' ')
            for (j = i+3; (*d->str)[j]; j++)
              (*d->str)[j] = (*d->str)[j+1];
          (*d->str)[i+3] = UPPER((*d->str)[i+3]);
        } else if (add_spaces(*d->str, d->max_str, i, 2)) {
          (*d->str)[i] = '\r';
          (*d->str)[i+1] = '\n';
          while ((*d->str)[i+2] == ' ')
            for (j = i+2; (*d->str)[j]; j++)
              (*d->str)[j] = (*d->str)[j+1];
          (*d->str)[i+2] = UPPER((*d->str)[i+2]);
        }
      }
    }

  sprintf(format, "%s%s\r\n", indent ? "   " : "", *d->str);
  int q = 0;
  for (k = 0; strlen(format) > (u_int) k && q < 1023; q++)
  {
    line = 78;
    for (i = k, j = 0; format[i] && j < 79; i++)
      if (format[i] == '^' && (i < 1 || format[i-1] != '^') && format[i+1]) {
        switch (LOWER(format[i+1])) {
        case 'b':
        case 'c':
        case 'g':
        case 'l':
        case 'm':
        case 'n':
        case 'r':
        case 'w':
        case 'y':
          line += 2;
          break;
        case '^':
          line++;
          j++;
          break;
        default:
          j += 2;
          break;
        }
      } else
        j++;
    for (i = MIN((unsigned)strlen(format) - 1, (unsigned)k + line); i > k && i > 0; i--)
      if (format[i] == '\n' && format[i-1] == '\r') {
        k = i + 1;
        break;
      }
    for (i = MIN((unsigned)strlen(format) - 1, (unsigned)k + line); i > k && i > 0; i--)
      if (isspace(format[i]) && isprint(format[i-1]) && !isspace(format[i-1])) {
        format[i] = '\r';
        if (format[i+1] != ' ')
          add_spaces(format, d->max_str, i, 1);
        format[i+1] = '\n';
        i += 2;
        while (format[i] == ' ')
          for (j = i; format[j]; j++)
            format[j] = format[j+1];
        k = i;
        break;
      }
  }

  i = MAX(strlen(format)-1, 3);
  if (format[i] == '\n' && format[i-1] == '\r' && format[i-2] == '\n' && format[i-3] == '\r')
    format[i-1] = '\0';
  delete [] *d->str;
  *d->str = format;
}

/* Add user input to the 'current' string (as defined by d->str) */
void string_add(struct descriptor_data *d, char *str)
{
  int terminator = 0;

  /* determine if this is the terminal string, and truncate if so */
  /* changed to only accept '@' at the beginning of line - J. Elson 1/17/94 */

  delete_doubledollar(str);

  if ((terminator = (*str == '@')))
    *str = '\0';
  else if (*str == '$' && !str_cmp(str, "$abort") && !d->connected)
  {
    SEND_TO_Q("Aborted.\r\n", d);
    d->mail_to = 0;
    delete [] *d->str;
    d->str = NULL;
    if (!IS_NPC(d->character))
      PLR_FLAGS(d->character).RemoveBits(PLR_MAILING, PLR_WRITING, ENDBIT);
    return;
  }

  if (!(*d->str))
  {
    if (strlen(str) > (u_int)d->max_str) {
      send_to_char("String too long - Truncated.\r\n",
                   d->character);
      *(str + d->max_str) = '\0';
      terminator = 1;
    }
    *d->str = new char[strlen(str) + 3];
    strcpy(*d->str, str);
  } else
  {
    if (strlen(str) + strlen(*d->str) > (u_int)d->max_str) {
      send_to_char("String too long.  Last line skipped.\r\n", d->character);
      terminator = 1;
    } else {
      char *temp = new char[strlen(*d->str) + strlen(str) + 3];
      if (!temp) {
        perror("string_add");
        shutdown();
        ;
      }
      strcpy (temp, *d->str);
      strcat(temp, str);
      delete [] *d->str;
      *d->str = temp;
    }
  }

  if (terminator)
  {
    extern void iedit_disp_menu(struct descriptor_data *d);
    extern void iedit_disp_extradesc_menu(struct descriptor_data *d);
    extern void redit_disp_menu(struct descriptor_data *d);
    extern void redit_disp_extradesc_menu(struct descriptor_data *d);
    extern void redit_disp_exit_menu(struct descriptor_data *d);
    extern void medit_disp_menu(struct descriptor_data *d);
    extern void cedit_disp_menu(struct descriptor_data *d, int mode);
    extern void vedit_disp_menu(struct descriptor_data *d);
    extern void hedit_disp_data_menu(struct descriptor_data *d);
    extern void icedit_disp_menu(struct descriptor_data *d);
    extern void deckbuild_main_menu(struct descriptor_data *d);
    extern void spedit_disp_menu(struct descriptor_data *d);
    extern void write_world_to_disk(int zone);
    extern void vehcust_menu(struct descriptor_data *d);
    extern void pocketsec_mailmenu(struct descriptor_data *d);
    extern void pocketsec_notemenu(struct descriptor_data *d);

    if (STATE(d) == CON_DECK_CREATE && d->edit_mode == 1) {
      if (d->edit_obj->photo)
        delete [] d->edit_obj->photo;
      format_string(d, 0);
      d->edit_obj->photo = str_dup(*d->str);
      delete [] *d->str;
      delete d->str;
      deckbuild_main_menu(d);
    } else if (STATE(d) == CON_VEHCUST) {
      if (d->edit_veh->restring_long) 
        delete [] d->edit_veh->restring_long;
      format_string(d, 0);
      d->edit_veh->restring_long = str_dup(*d->str);
      delete [] *d->str;
      delete d->str;
      vehcust_menu(d); 
    } else if (STATE(d) == CON_TRIDEO) {
      (*d->str)[strlen(*d->str)-2] = '\0';
      sprintf(buf, "INSERT INTO trideo_broadcast (author, message) VALUES (%ld, '%s')", GET_IDNUM(d->character), prepare_quotes(buf2, *d->str));
      mysql_wrapper(mysql, buf);
      delete [] *d->str;
      delete d->str;
      STATE(d) = CON_PLAYING;
    } else if (STATE(d) == CON_DECORATE) {
      delete [] world[d->character->in_room].description;
      format_string(d, 0);
      world[d->character->in_room].description = str_dup(*d->str);
      write_world_to_disk(zone_table[world[d->character->in_room].zone].number);
      delete [] *d->str;
      delete d->str;
      STATE(d) = CON_PLAYING;
    } else if (STATE(d) == CON_SPELL_CREATE && d->edit_mode == 3) {
      if (d->edit_obj->photo) 
        delete [] d->edit_obj->photo;
      format_string(d, 0);
      d->edit_obj->photo = str_dup(*d->str);
      delete [] *d->str;
      delete d->str;
      spedit_disp_menu(d);
    } else if (STATE(d) == CON_IEDIT) {
      switch(d->edit_mode) {
      case IEDIT_EXTRADESC_DESCRIPTION:
        if (((struct extra_descr_data *)*d->misc_data)->description)
          delete [] ((struct extra_descr_data *)*d->misc_data)->description;
        format_string(d, 0);
        ((struct extra_descr_data *)*d->misc_data)->description =
          str_dup(*d->str);
        delete [] *d->str;
        delete d->str;
        iedit_disp_extradesc_menu(d);
        break;
      case IEDIT_LONGDESC:
        if (d->edit_obj->text.look_desc)
          delete [] d->edit_obj->text.look_desc;
        format_string(d, 0);
        d->edit_obj->text.look_desc = str_dup(*d->str);
        delete [] *d->str;
        delete d->str;
        iedit_disp_menu(d);
        break;
      }
    } else if (STATE(d) == CON_MEDIT) {
      switch(d->edit_mode) {
      case MEDIT_LONG_DESCR:
        if (d->edit_mob->player.physical_text.look_desc)
          delete [] d->edit_mob->player.physical_text.look_desc;
        format_string(d, 0);
        d->edit_mob->player.physical_text.look_desc = str_dup(*d->str);
        delete [] *d->str;
        delete d->str;
        medit_disp_menu(d);
        break;
      case MEDIT_REG_DESCR:
        if (d->edit_mob->player.physical_text.room_desc)
          delete [] d->edit_mob->player.physical_text.room_desc;
        format_string(d, 0);
        d->edit_mob->player.physical_text.room_desc = str_dup(*d->str);
        delete [] *d->str;
        delete d->str;
        medit_disp_menu(d);
        break;
      }
    } else if (STATE(d) == CON_VEDIT) {
      switch(d->edit_mode) {
      case VEDIT_LONGDESC:
        if (d->edit_veh->long_description)
          delete [] d->edit_veh->long_description;
        format_string(d, 0);
        d->edit_veh->long_description = str_dup(*d->str);
        delete [] *d->str;
        delete d->str;
        vedit_disp_menu(d);
        break;
      case VEDIT_INSDESC:
        if (d->edit_veh->inside_description)
          delete [] d->edit_veh->inside_description;
        format_string(d, 0);
        d->edit_veh->inside_description = str_dup(*d->str);
        delete [] *d->str;
        delete d->str;
        vedit_disp_menu(d);
        break;
      case VEDIT_REARDESC:
        if (d->edit_veh->rear_description)
          delete [] d->edit_veh->rear_description;
        format_string(d, 0);
        d->edit_veh->rear_description = str_dup(*d->str);
        delete [] *d->str;
        delete d->str;
        vedit_disp_menu(d);
        break;
      }
    } else if (STATE(d) == CON_HEDIT) {
      switch(d->edit_mode) {
      case HEDIT_DESC:
        if (d->edit_host->desc)
          delete [] d->edit_host->desc;
        format_string(d, 0);
        d->edit_host->desc = str_dup(*d->str);
        delete [] *d->str;
        delete d->str;
        hedit_disp_data_menu(d);
        break;
      }
    } else if (STATE(d) == CON_ICEDIT) {
      switch(d->edit_mode) {
      case ICEDIT_DESC:
        if (d->edit_icon->long_desc)
          delete [] d->edit_icon->long_desc;
        format_string(d, 0);
        d->edit_icon->long_desc = str_dup(*d->str);
        delete [] *d->str;
        delete d->str;
        icedit_disp_menu(d);
        break;
      }
    } else if (STATE(d) == CON_REDIT) {
      switch(d->edit_mode) {
      case REDIT_DESC:
        if (d->edit_room->description)
          delete [] d->edit_room->description;
        format_string(d, 1);
        d->edit_room->description = str_dup(*d->str);
        delete [] *d->str;
        delete d->str;
        redit_disp_menu(d);
        break;
      case REDIT_NDESC:
        if (d->edit_room->night_desc)
          delete [] d->edit_room->night_desc;
        format_string(d, 1);
        if (strlen(*d->str) > 5) {
          d->edit_room->night_desc = str_dup(*d->str);
        } else
          d->edit_room->night_desc = NULL;
        delete [] *d->str;
        delete d->str;
        redit_disp_menu(d);
        break;
      case REDIT_EXTRADESC_DESCRIPTION:
        if (((struct extra_descr_data *)*d->misc_data)->description)
          delete ((struct extra_descr_data *)*d->misc_data)->description;
        format_string(d, 0);
        ((struct extra_descr_data *)*d->misc_data)->description =
          str_dup(*d->str);
        delete [] *d->str;
        delete d->str;
        redit_disp_extradesc_menu(d);
        break;
      case REDIT_EXIT_DESCRIPTION:
        if (d->edit_room->dir_option[d->edit_number2]->general_description)
          delete [] d->edit_room->dir_option[d->edit_number2]->general_description;
        format_string(d, 0);
        d->edit_room->dir_option[d->edit_number2]->general_description =
          str_dup(*d->str);
        delete [] *d->str;
        delete d->str;
        redit_disp_exit_menu(d);
        break;
      }
    } else if (STATE(d) == CON_QEDIT && d->edit_mode == QEDIT_INFO) {
      if (d->edit_quest->info)
        delete [] d->edit_quest->info;
      format_string(d, 0);
      d->edit_quest->info = str_dup(*d->str);
      delete [] *d->str;
      delete d->str;
      qedit_disp_menu(d);
    } else if (STATE(d) == CON_BCUSTOMIZE && d->edit_mode == CEDIT_DESC) {
      if (d->edit_mob->player.background)
        delete [] d->edit_mob->player.background;
      format_string(d, 0);
      d->edit_mob->player.background = str_dup(*d->str);
      delete [] *d->str;
      delete d->str;
      cedit_disp_menu(d, 0);
    } else if (STATE(d) == CON_PCUSTOMIZE || STATE(d) == CON_ACUSTOMIZE || STATE(d) == CON_FCUSTOMIZE) {
      switch(d->edit_mode) {
      case CEDIT_LONG_DESC:
        if (d->edit_mob->player.physical_text.look_desc)
          delete [] d->edit_mob->player.physical_text.look_desc;
        format_string(d, 0);
        d->edit_mob->player.physical_text.look_desc = str_dup(*d->str);
        delete [] *d->str;
        delete d->str;
        cedit_disp_menu(d, 0);
        break;
      case CEDIT_DESC:
        if (d->edit_mob->player.physical_text.room_desc)
          delete [] d->edit_mob->player.physical_text.room_desc;
        format_string(d, 0);
        d->edit_mob->player.physical_text.room_desc = str_dup(*d->str);
        delete [] *d->str;
        delete d->str;
        cedit_disp_menu(d, 0);
        break;
      }
    } else if (STATE(d) == CON_POCKETSEC && d->edit_mode == 19) {
      struct obj_data *file = NULL;
      for (file = d->edit_obj->contains; file; file = file->next_content)
        if (!strcmp(file->restring, "Notes")) {
          file = file->contains;
          break;
        }
      if (file->photo)
        delete [] file->photo;
      format_string(d, 0);
      file->photo = str_dup(*d->str);
      delete [] *d->str;
      delete d->str;
      pocketsec_notemenu(d);
    } else if (PLR_FLAGGED(d->character, PLR_MAILING)) {
      store_mail(d->mail_to, GET_IDNUM(d->character), *d->str);
      d->mail_to = 0;
      delete [] *d->str;
      delete d->str;
      SEND_TO_Q("Message sent.\r\n", d);
      if (!IS_NPC(d->character))
        PLR_FLAGS(d->character).RemoveBits(PLR_MAILING, PLR_WRITING, ENDBIT);
      if (STATE(d) == CON_POCKETSEC)
        pocketsec_mailmenu(d);
    } 
    if (d->mail_to >= BOARD_MAGIC) {
      time_t now = time(0);
      char *tmstr = (char *) asctime(localtime(&now)), tmp[80], time[9];
      int i, day, month, year;

      for (i = 0; i < 3; i++)
        tmp[i] = tmstr[4+i];
      tmp[3] = '\0';
      switch (tmp[0]) {
      case 'A':
        if (tmp[1] == 'p')
          month = 4;
        else
          month = 8;
        break;
      case 'D':
        month = 12;
        break;
      case 'F':
        month = 2;
        break;
      case 'J':
        if (tmp[1] == 'a')
          month = 1;
        else if (tmp[2] == 'n')
          month = 6;
        else
          month = 7;
        break;
      case 'M':
        if (tmp[2] == 'r')
          month = 3;
        else
          month = 5;
        break;
      case 'N':
        month = 11;
        break;
      case 'O':
        month = 10;
        break;
      case 'S':
        month = 9;
        break;
      default:
        month = 0;
        break;
      }
      for (i = 0; i < 2; i++)
        tmp[i] = tmstr[8+i];
      tmp[2] = '\0';
      day = atoi(tmp);
      for (i = 0; i < 8; i++)
        time[i] = tmstr[11+i];
      time[8] = '\0';
      for (i = 0; i < 2; i++)
        tmp[i] = tmstr[22+i];
      tmp[2] = '\0';
      year = atoi(tmp);

      if (IS_NPC(d->character))
        sprintf(tmp, "\r\n--%s (%s/%s%d-%s%d-%s%d)\r\n",
                GET_NAME(d->character), time, month < 10 ? "0" : "", month,
                day < 10 ? "0" : "", day, year < 10 ? "0" : "", year);
      else
        sprintf(tmp, "\r\n--%s (%s/%s%d-%s%d-%s%d)\r\n",
                GET_CHAR_NAME(d->character), time, month < 10 ? "0" : "", month,
                day < 10 ? "0" : "", day, year < 10 ? "0" : "", year);
      char *ptr = new char[strlen(*d->str) + strlen(tmp) + 1];
      if (!ptr) {
        perror("string_add");
        shutdown();
      }
      strcpy (ptr, *d->str);
      strcat(ptr, tmp);
      delete [] *d->str;
      *d->str = ptr;
      Board_save_board(d->mail_to - BOARD_MAGIC);
      d->mail_to = 0;
    }
    d->str = NULL;

    if (!d->connected && d->character && !IS_NPC(d->character))
      PLR_FLAGS(d->character).RemoveBit(PLR_WRITING);
  } else
    strcat(*d->str, "\r\n");
}



/* **********************************************************************
*  Modification of character skills                                     *
********************************************************************** */

ACMD(do_skillset)
{
  struct char_data *vict;
  char name[100], buf2[100], buf[100], help[MAX_STRING_LENGTH];
  int skill, value, i, qend;
  extern struct skill_data skills[];

  argument = one_argument(argument, name);

  if (!*name) {                 /* no arguments. print an informative text */
    send_to_char("Syntax: skillset <name> '<skill>' <value>\r\n", ch);
    strcpy(help, "Skill being one of the following:\r\n");
    for (i = 0; i < MAX_SKILLS; i++) {
      if (*skills[i].name == '!')
        continue;
      sprintf(help + strlen(help), "%18s", skills[i].name);
      if (i % 4 == 3) {
        strcat(help, "\r\n");
        send_to_char(help, ch);
        *help = '\0';
      }
    }
    if (*help)
      send_to_char(help, ch);
    send_to_char("\r\n", ch);
    return;
  }
  if (!(vict = get_char_vis(ch, name))) {
    send_to_char(NOPERSON, ch);
    return;
  }
  skip_spaces(&argument);

  /* If there is no chars in argument */
  if (!*argument) {
    send_to_char("Skill name expected.\r\n", ch);
    return;
  }
  if (*argument != '\'') {
    send_to_char("Skill must be enclosed in: ''\r\n", ch);
    return;
  }
  /* Locate the last quote && lowercase the magic words (if any) */

  for (qend = 1; *(argument + qend) && (*(argument + qend) != '\''); qend++)
    *(argument + qend) = LOWER(*(argument + qend));

  if (*(argument + qend) != '\'') {
    send_to_char("Skill must be enclosed in: ''\r\n", ch);
    return;
  }
  strcpy(help, (argument + 1));
  help[qend - 1] = '\0';
  if ((skill = find_skill_num(help)) <= 0) {
    send_to_char("Unrecognized skill.\r\n", ch);
    return;
  }
  argument += qend + 1;         /* skip to next parameter */
  argument = one_argument(argument, buf);

  if (!*buf) {
    send_to_char("Learned value expected.\r\n", ch);
    return;
  }
  value = atoi(buf);
  if (value < 0) {
    send_to_char("Minimum value for learned is 0.\r\n", ch);
    return;
  }
  if (value > 100) {
    send_to_char("Max value for learned is 100.\r\n", ch);
    return;
  }
  if (IS_NPC(vict)) {
    send_to_char("You can't set NPC skills.\r\n", ch);
    return;
  }
  sprintf(buf2, "%s changed %s's %s from %d to %d.",
          GET_CHAR_NAME(ch), GET_NAME(vict),
          skills[skill].name,
          GET_SKILL(vict, skill), value);
  mudlog(buf2, ch, LOG_WIZLOG, TRUE);

  SET_SKILL(vict, skill, value);

  sprintf(buf2, "You change %s's %s to %d.\r\n", GET_NAME(vict), skills[skill].name, value);
  send_to_char(buf2, ch);

}


/* db stuff *********************************************** */


/* One_Word is like one_argument, execpt that words in quotes "" are */
/* regarded as ONE word                                              */

char *one_word(char *argument, char *first_arg)
{
  int begin, look_at;

  begin = 0;

  do {
    for (; isspace(*(argument + begin)); begin++)
      ;

    if (*(argument + begin) == '\"') {  /* is it a quote */

      begin++;

      for (look_at = 0; (*(argument + begin + look_at) >= ' ') &&
           (*(argument + begin + look_at) != '\"'); look_at++)
        *(first_arg + look_at) = LOWER(*(argument + begin + look_at));

      if (*(argument + begin + look_at) == '\"')
        begin++;

    } else {

      for (look_at = 0; *(argument + begin + look_at) > ' '; look_at++)
        *(first_arg + look_at) = LOWER(*(argument + begin + look_at));

    }

    *(first_arg + look_at) = '\0';
    begin += look_at;
  } while (fill_word(first_arg));

  return (argument + begin);
}


void page_string(struct descriptor_data *d, char *str, int keep_internal)
{
  if (keep_internal)
  {
    d->showstr_head = new char[strlen(str) + 1];
    strcpy(d->showstr_head, str);
    d->showstr_point = d->showstr_head;
  } else
    d->showstr_point = str;

  char empty_argument = '\0';
  show_string(d, &empty_argument);
}

void show_string(struct descriptor_data *d, char *input)
{
  char buffer[MAX_STRING_LENGTH], buf[MAX_INPUT_LENGTH];
  register char *scan, *chk;
  int lines = 0, toggle = 1;

  one_argument(input, buf);

  if (*buf)
  {
    if (d->showstr_head) {
      delete [] d->showstr_head;
      d->showstr_head = 0;
    }
    d->showstr_point = 0;
    return;
  }
  for (scan = buffer;; scan++, d->showstr_point++)
  {
    if ((((*scan = *d->showstr_point) == '\n') || (*scan == '\r')) &&
        ((toggle = -toggle) < 0))
      lines++;
    else if (!*scan || (lines >= 44)) {
      if (lines >= 44) {
        d->showstr_point++;
        *(++scan) = '\0';
      }
      SEND_TO_Q(buffer, d);
      for (chk = d->showstr_point; isspace(*chk); chk++)
        ;
      if (!*chk) {
        if (d->showstr_head) {
          delete [] d->showstr_head;
          d->showstr_head = 0;
        }
        d->showstr_point = 0;
      }
      return;
    }
  }
}

