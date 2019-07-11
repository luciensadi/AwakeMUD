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

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "comm.h"
#include "newmail.h"
#include "boards.h"
#include "olc.h"
#include "quest.h"
#include "newmagic.h"
#include "newmatrix.h"
#include "constants.h"
#include "newdb.h"
#include "helpedit.h"

#define DO_FORMAT_INDENT   1
#define DONT_FORMAT_INDENT 0

void show_string(struct descriptor_data *d, char *input);
void qedit_disp_menu(struct descriptor_data *d);

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

  /* Why was this a thing? -LS
  if (strlen(*d->str) >= 1023)
    return;
   */
  
  // if the editor is an implementor and begins with the sequence
  // /**/, then we know not to format this string
  if (*(*d->str) == '/' && *(*d->str + 1) == '*' && *(*d->str + 2) == '*' && *(*d->str + 3) == '/')
  {
    for (i = 0; i < (int)(strlen(*d->str) - 4); i++)
      (*d->str)[i] = (*d->str)[i+4];
    (*d->str)[strlen(*d->str) - 4] = '\0';
    return;
  }

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
  
  char *format = new char[d->max_str];
  sprintf(format, "%s%s\r\n", indent ? "   " : "", *d->str);
  int q = 0;
  for (k = 0; strlen(format) > (u_int) k && q < 1023; q++)
  {
    line = 78;
    for (i = k, j = 0; format[i] && j < 79; i++)
      if (format[i] == '^' && (i < 1 || format[i-1] != '^') && format[i+1]) {
        // Look for color code initializers; if they exist, bump the line count longer.
        if (!strchr((const char *)"nrgybmcwajeloptv", LOWER(format[i+1]))) {
          line += 2;
        } else if (format[i+1] == '^') {
          line++;
          j++;
        } else {
          j += 2;
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
    DELETE_D_STR_IF_EXTANT(d);
    if (!IS_NPC(d->character))
      PLR_FLAGS(d->character).RemoveBits(PLR_MAILING, PLR_WRITING, ENDBIT);
    return;
  }

  if (!(*d->str))
  {
    if (strlen(str) > (u_int)d->max_str) {
      send_to_char("String too long - Truncated.\r\n", d->character);
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
      strcpy(temp, *d->str);
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
    
// REPLACE_STRING swaps target with d->str if d->str exists (otherwise leaves target alone).
#define REPLACE_STRING_FORMAT_SPECIFIED(target, format_bit) { \
  if ((d->str)) {                                             \
    DELETE_ARRAY_IF_EXTANT((target));                         \
    format_string(d, (format_bit));                           \
    (target) = str_dup(*d->str);                              \
    DELETE_D_STR_IF_EXTANT(d);                                \
  }                                                           \
}
    
#define REPLACE_STRING(target) (REPLACE_STRING_FORMAT_SPECIFIED(target, DONT_FORMAT_INDENT))
#define REPLACE_STRING_WITH_INDENTED_FORMATTING(target) (REPLACE_STRING_FORMAT_SPECIFIED(target, DO_FORMAT_INDENT))

    if (STATE(d) == CON_DECK_CREATE && d->edit_mode == 1) {
      REPLACE_STRING(d->edit_obj->photo);
      deckbuild_main_menu(d);
    } else if (STATE(d) == CON_VEHCUST) {
      REPLACE_STRING(d->edit_veh->restring_long);
      vehcust_menu(d); 
    } else if (STATE(d) == CON_TRIDEO) {
      (*d->str)[strlen(*d->str)-2] = '\0';
      sprintf(buf, "INSERT INTO trideo_broadcast (author, message) VALUES (%ld, '%s')", GET_IDNUM(d->character), prepare_quotes(buf2, *d->str, sizeof(buf2) / sizeof(buf2[0])));
      mysql_wrapper(mysql, buf);
      DELETE_D_STR_IF_EXTANT(d);
      STATE(d) = CON_PLAYING;
    } else if (STATE(d) == CON_DECORATE) {
      REPLACE_STRING(d->character->in_room->description);
      write_world_to_disk(zone_table[d->character->in_room->zone].number);
      STATE(d) = CON_PLAYING;
    } else if (STATE(d) == CON_SPELL_CREATE && d->edit_mode == 3) {
      REPLACE_STRING(d->edit_obj->photo);
      spedit_disp_menu(d);
    } else if (STATE(d) == CON_HELPEDIT) {
      REPLACE_STRING(d->edit_helpfile->body);
      helpedit_disp_menu(d);
    } else if (STATE(d) == CON_IEDIT) {
      switch(d->edit_mode) {
        case IEDIT_EXTRADESC_DESCRIPTION:
          REPLACE_STRING(((struct extra_descr_data *)*d->misc_data)->description);
          iedit_disp_extradesc_menu(d);
          break;
        case IEDIT_LONGDESC:
          REPLACE_STRING(d->edit_obj->text.look_desc);
          iedit_disp_menu(d);
          break;
      }
    } else if (STATE(d) == CON_MEDIT) {
      switch(d->edit_mode) {
      case MEDIT_LONG_DESCR:
        REPLACE_STRING(d->edit_mob->player.physical_text.look_desc);
        medit_disp_menu(d);
        break;
      case MEDIT_REG_DESCR:
        REPLACE_STRING(d->edit_mob->player.physical_text.room_desc);
        medit_disp_menu(d);
        break;
      }
    } else if (STATE(d) == CON_VEDIT) {
      switch(d->edit_mode) {
      case VEDIT_LONGDESC:
        REPLACE_STRING(d->edit_veh->long_description);
        vedit_disp_menu(d);
        break;
      case VEDIT_INSDESC:
        REPLACE_STRING(d->edit_veh->inside_description);
        vedit_disp_menu(d);
        break;
      case VEDIT_REARDESC:
        REPLACE_STRING(d->edit_veh->rear_description);
        vedit_disp_menu(d);
        break;
      }
    } else if (STATE(d) == CON_HEDIT) {
      switch(d->edit_mode) {
      case HEDIT_DESC:
        REPLACE_STRING(d->edit_host->desc);
        hedit_disp_data_menu(d);
        break;
      }
    } else if (STATE(d) == CON_ICEDIT) {
      switch(d->edit_mode) {
      case ICEDIT_DESC:
        REPLACE_STRING(d->edit_icon->long_desc);
        icedit_disp_menu(d);
        break;
      }
    } else if (STATE(d) == CON_REDIT) {
      switch(d->edit_mode) {
      case REDIT_DESC:
        REPLACE_STRING_WITH_INDENTED_FORMATTING(d->edit_room->description);
        redit_disp_menu(d);
        break;
      case REDIT_NDESC:
          if (d->str && *d->str) {
            if (strlen(*d->str) > 5)
              REPLACE_STRING_WITH_INDENTED_FORMATTING(d->edit_room->night_desc);
            else
              d->edit_room->night_desc = NULL;
          }
        DELETE_ARRAY_IF_EXTANT(d->edit_room->night_desc);
        if (strlen(*d->str) > 5) {
          d->edit_room->night_desc = str_dup(*d->str);
        } else
          d->edit_room->night_desc = NULL;
        DELETE_D_STR_IF_EXTANT(d);
        redit_disp_menu(d);
        break;
      case REDIT_EXTRADESC_DESCRIPTION:
        REPLACE_STRING(((struct extra_descr_data *)*d->misc_data)->description);
        redit_disp_extradesc_menu(d);
        break;
      case REDIT_EXIT_DESCRIPTION:
        REPLACE_STRING(d->edit_room->dir_option[d->edit_number2]->general_description);
        redit_disp_exit_menu(d);
        break;
      }
    } else if (STATE(d) == CON_QEDIT && d->edit_mode == QEDIT_INFO) {
      REPLACE_STRING(d->edit_quest->info);
      qedit_disp_menu(d);
    } else if (STATE(d) == CON_BCUSTOMIZE && d->edit_mode == CEDIT_DESC) {
      REPLACE_STRING(d->edit_mob->player.background);
      cedit_disp_menu(d, 0);
    } else if (STATE(d) == CON_PCUSTOMIZE || STATE(d) == CON_ACUSTOMIZE || STATE(d) == CON_FCUSTOMIZE) {
      switch(d->edit_mode) {
      case CEDIT_LONG_DESC:
        REPLACE_STRING(d->edit_mob->player.physical_text.look_desc);
        cedit_disp_menu(d, 0);
        break;
      case CEDIT_DESC:
        REPLACE_STRING(d->edit_mob->player.physical_text.room_desc);
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
      REPLACE_STRING(file->photo);
      pocketsec_notemenu(d);
    } else if (PLR_FLAGGED(d->character, PLR_MAILING)) {
      store_mail(d->mail_to, d->character, *d->str);
      d->mail_to = 0;
      DELETE_D_STR_IF_EXTANT(d);
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
*  Modification of character spells                                     *
********************************************************************** */

ACMD(do_spellset)
{
  struct char_data *vict;
  char name[100], help[MAX_STRING_LENGTH];
  int spelltoset, force, i, qend;

  argument = one_argument(argument, name);

  if (!*name) {                 /* no arguments. print an informative text */
    send_to_char("Syntax: spellset <name> '<spell>' <force>\r\n", ch);
    strcpy(help, "Spell being one of the following:\r\n");
    for ( i = 0; i < MAX_SPELLS; i++) {
      if (*spells[i].name == '!')
        continue;
      sprintf(help + strlen(help), "%18s", spells[i].name);
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
  
  // Moved this to the top for quicker precondition failure (we don't care about args if they can't set the person's values in the first place).
  if (IS_NPC(vict)) {
    send_to_char("You can't set NPC spells.\r\n", ch);
    return;
  }
  
  // Added staff-level checking.
  if (GET_LEVEL(vict) > GET_LEVEL(ch)) {
    send_to_char("You can't modify the spells of someone more powerful than you.\r\n", ch);
    return;
  }
  
  skip_spaces(&argument);

  /* If there is no chars in argument */
  if (!*argument) {
    send_to_char("Spell name expected.\r\n", ch);
    return;
  }
  if (*argument != '\'') {
    send_to_char("Spell must be enclosed in: ''\r\n", ch);
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
  if ((spelltoset = find_spell_num(help)) <= 0) {
    send_to_char("Unrecognized spell.\r\n", ch);
    return;
  }
  argument += qend + 1;         /* skip to next parameter */
  
  char force_arg[MAX_STRING_LENGTH];
  argument = one_argument(argument, force_arg);

  if (!*force_arg) {
    send_to_char("Force value expected.\r\n", ch);
    return;
  }
  force = atoi(force_arg);
  if (force < 1) {
    send_to_char("Minimum force is 1.\r\n", ch);
    return;
  }
  // TODO Does the magic attribute or sorcery skill limit the force? 
  if (force > 100) {
    send_to_char("Max value for force is 100.\r\n", ch);
    return;
  }
  
  int subtype = 0;
  strcpy(buf, spells[spelltoset].name);
  // Require that the attribute spells have an attribute specified (see spell->subtype comment).
  if (spelltoset == SPELL_INCATTR || spelltoset == SPELL_INCCYATTR ||
      spelltoset == SPELL_DECATTR || spelltoset == SPELL_DECCYATTR) {
    // Check for the argument.
    if (!*argument) {
      send_to_char("You must supply one of 'bod', 'qui', 'str', 'int', 'wil', 'cha', 'rea'.\r\n", ch);
      return;
    }
    
    // Compare against the new short_attributes consts in constants.cpp and find the applicable one.
    for (subtype = 0; subtype < 6; subtype++) {
      if (!strncmp(argument, short_attributes[subtype], strlen(argument))) {
        // The goal here is to find the index number the argument represents.
        //  Once we find it, update the spell's name, then break.
        sprintf(ENDOF(buf), " (%s)", attributes[subtype]);
        break;
      }
    }
    
    // If we hit 6, the argument did not match the list.
    if (subtype >= 6) {
      send_to_char("You must supply one of 'bod', 'qui', 'str', 'int', 'wil', 'cha', 'rea'.\r\n", ch);
      return;
    }
  }
  
  // TODO: Checks for validity (if the character is unable to use the spell, don't set it).
  // TODO: What if you want to remove a spell from someone?
  
  struct spell_data *spell = new spell_data;

  // Three lines away from working spellset command. How do I get the damn values?

  /* spell->name is a pointer to a char, and other code (spell release etc) deletes what it points to
      on cleanup. Thus, you need to clone the spell_type's name into the spell_data to assign your
      new spell a name. */
  spell->name = str_dup(buf);
  
  /* The index of the spells[] table is a 1:1 mapping of the SPELL_ type, so you can just set the
      spell->type field to your spelltoset value. */
  spell->type = spelltoset;
  
  /* This one's the tricky one. Via newmagic.cpp's do_learn, we know that subtype is used for the attribute
      spells to decide which attribute is being increased/decreased. To find this, I've included an optional
      parameter in the command after force that specifies 'str', 'bod' etc. */
  spell->subtype = subtype;
  
  spell->force = force;
  spell->next = GET_SPELLS(vict);
  GET_SPELLS(vict) = spell;
  
  send_to_char("OK.\r\n", ch);
  sprintf(buf, "$n has set your '%s' spell to Force %d.", spells[spelltoset].name, force);
  act(buf, TRUE, ch, NULL, vict, TO_VICT);
}


/* **********************************************************************
*  Modification of character skills                                     *
********************************************************************** */

ACMD(do_skillset)
{
  struct char_data *vict;
  char name[100], buf2[100], buf[100], help[MAX_STRING_LENGTH];
  int skill, value, i, qend;

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

  GET_SKILL_DIRTY_BIT(vict) = TRUE;
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
    DELETE_ARRAY_IF_EXTANT(d->showstr_head);
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
  char *scan, *chk;
  int lines = 0, toggle = 1;

  one_argument(input, buf);

  if (*buf)
  {
    DELETE_ARRAY_IF_EXTANT(d->showstr_head);
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
        DELETE_ARRAY_IF_EXTANT(d->showstr_head);
        d->showstr_point = 0;
      }
      return;
    }
  }
}

