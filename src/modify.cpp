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

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "comm.hpp"
#include "newmail.hpp"
#include "boards.hpp"
#include "olc.hpp"
#include "quest.hpp"
#include "newmagic.hpp"
#include "newmatrix.hpp"
#include "constants.hpp"
#include "newdb.hpp"
#include "helpedit.hpp"
#include "config.hpp"

#define DO_FORMAT_INDENT   1
#define DONT_FORMAT_INDENT 0

void show_string(struct descriptor_data *d, char *input);
void qedit_disp_menu(struct descriptor_data *d);
void format_tabs(struct descriptor_data *d);

extern void insert_or_append_emote_at_position(struct descriptor_data *d, char *string);

/* ************************************************************************
*  modification of new'ed strings                                      *
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
      // convert '\n[^\r]' to ' [^\r]'
      if ((*d->str)[i] == '\n' && (*d->str)[i+1] != '\r')
        (*d->str)[i] = ' ';
      // delete the first character of patterns '\n\r' '\r.*'
      else
        for (j = i; (*d->str)[j]; j++)
          (*d->str)[j] = (*d->str)[j+1];
      // if pattern was '\n\r' or '\r\r' then
      if ((*d->str)[i] == '\r') {
        i += 2; // leave it in place-- it's an official newline character
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

  size_t format_len = d->max_str + 1;
  char *format = new char[format_len];
  memset(format, '\0', format_len * sizeof(char));
  snprintf(format, d->max_str, "%s%s\r\n", indent ? "   " : "", *d->str);
  int q = 0;
  for (k = 0; strlen(format) > (u_int) k && q < 1023; q++)
  {
    line = 78;
    for (i = k, j = 0; format[i] && j < 79; i++)
      if (format[i] == '^' && (i < 1 || format[i-1] != '^') && format[i+1]) {
        // Look for color code initializers; if they exist, bump the line count longer.
        if (strchr((const char *)"nrgybmcwajeloptv", LOWER(format[i+1]))) {
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
    for (i = MIN((unsigned)strlen(format) - 2, (unsigned)k + line); i > k && i > 0; i--)
      if (isspace(format[i]) && isprint(format[i-1]) && !isspace(format[i-1])) {
        format[i] = '\r';
        if (format[i+1] != ' ')
          add_spaces(format, d->max_str, i, 1);
        format[i+1] = '\n';
        format[format_len - 1] = '\0';

        if (i + 2 < (int) strlen(format)) {
          i += 2;
          while (format[i] == ' ')
            for (j = i; format[j]; j++)
              format[j] = format[j+1];
        }

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
  /* changed further to only accept it if the character after it is not alphabetical - LS 2022 */

  delete_doubledollar(str);

  if ((terminator = (*str == '@' && !isalpha(*(str + 1)))))
    *str = '\0';
  else if (*str == '$')
  {
    bool will_abort = !str_cmp(str, "$abort");
    send_to_char(d->character, "[detected $, str is: '%s' -- will %sabort (str_cmp %s)]",
                 str,
                 will_abort ? "" : "NOT ",
                 !str_cmp(str, "$abort") ? "ok" : "FAIL"
               );
    if (will_abort) {
      SEND_TO_Q("Aborted.\r\n", d);
      d->mail_to = 0;
      DELETE_D_STR_IF_EXTANT(d);
      if (!IS_NPC(d->character))
        PLR_FLAGS(d->character).RemoveBits(PLR_MAILING, PLR_WRITING, ENDBIT);
      return;
    }
  }

  if (!(*d->str))
  {
    if (strlen(str) >= (u_int)d->max_str) {
      send_to_char("String too long - Truncated.\r\n", d->character);
      *(str + d->max_str - 1) = '\0';
      terminator = 1;
    }
    *d->str = new char[strlen(str) + 3];
    strlcpy(*d->str, str, d->max_str);
  } else
  {
    if (strlen(str) + strlen(*d->str) >= (u_int)d->max_str) {
      send_to_char("String too long.  Last line skipped.\r\n", d->character);
      terminator = 1;
    } else {
      int temp_size = strlen(*d->str) + strlen(str) + 3;
      char *temp = new char[temp_size];
      if (!temp) {
        perror("string_add");
        shutdown();
        ;
      }
      strlcpy(temp, *d->str, temp_size);
      strlcat(temp, str, temp_size);
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
#define REPLACE_STRING_FORMAT_SPECIFIED(target, format_bit) ({ \
  if ((d->str)) {                                             \
    DELETE_ARRAY_IF_EXTANT((target));                         \
    format_string(d, (format_bit));                           \
    (target) = str_dup(*d->str);                              \
    DELETE_D_STR_IF_EXTANT(d);                                \
  }                                                           \
})

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
      snprintf(buf, sizeof(buf), "INSERT INTO trideo_broadcast (author, message) VALUES (%ld, '%s')", GET_IDNUM(d->character), prepare_quotes(buf2, *d->str, sizeof(buf2) / sizeof(buf2[0])));
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
        char candidate = d->edit_mob->player.physical_text.room_desc[strlen(d->edit_mob->player.physical_text.room_desc) - 4];
        if (!ispunct(candidate))
          send_to_char(d->character, "^YWARNING: You're missing punctuation at the end of the room desc. (%c is not punctuation)^n\r\n", candidate);
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
        format_tabs(d);
        REPLACE_STRING_WITH_INDENTED_FORMATTING(d->edit_room->description);
        redit_disp_menu(d);
        break;
      case REDIT_NDESC:
          format_tabs(d);
          DELETE_ARRAY_IF_EXTANT(d->edit_room->night_desc);
          if (d->str && *d->str) {
            if (strlen(*d->str) > 5)
              REPLACE_STRING_WITH_INDENTED_FORMATTING(d->edit_room->night_desc);
            else
              d->edit_room->night_desc = NULL;
          }
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
    } else if (STATE(d) == CON_QEDIT) {
      switch (d->edit_mode) {
        case QEDIT_INFO:
          REPLACE_STRING(d->edit_quest->info);
          qedit_disp_menu(d);
          break;
      }
    } else if (STATE(d) == CON_BCUSTOMIZE && d->edit_mode == CEDIT_DESC) {
      REPLACE_STRING(d->edit_mob->player.background);
      cedit_disp_menu(d, 0);
    } else if (STATE(d) == CON_PCUSTOMIZE || STATE(d) == CON_ACUSTOMIZE || STATE(d) == CON_FCUSTOMIZE) {
      switch(d->edit_mode) {
      case CEDIT_LONG_DESC:
        if (!d->str || !*d->str || strlen(*d->str) < 2)
          SEND_TO_Q("Sorry, the minimum length is 2 characters.\r\n", d);
        else
          REPLACE_STRING(d->edit_mob->player.physical_text.look_desc);
        cedit_disp_menu(d, 0);
        break;
      case CEDIT_DESC:
        if (!d->str || !*d->str || strlen(*d->str) < 2)
          SEND_TO_Q("Sorry, the minimum length is 2 characters.\r\n", d);
        else
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
        snprintf(tmp, sizeof(tmp), "\r\n--%s (%s/%s%d-%s%d-%s%d)\r\n",
                GET_NAME(d->character), time, month < 10 ? "0" : "", month,
                day < 10 ? "0" : "", day, year < 10 ? "0" : "", year);
      else
        snprintf(tmp, sizeof(tmp), "\r\n--%s (%s/%s%d-%s%d-%s%d)\r\n",
                GET_CHAR_NAME(d->character), time, month < 10 ? "0" : "", month,
                day < 10 ? "0" : "", day, year < 10 ? "0" : "", year);

      int ptr_length = strlen(*d->str) + strlen(tmp) + 1;
      char *ptr = new char[ptr_length];
      if (!ptr) {
        perror("string_add");
        shutdown();
      }
      strlcpy (ptr, *d->str, ptr_length);
      strlcat(ptr, tmp, ptr_length);
      delete [] *d->str;
      *d->str = ptr;
      Board_save_board(d->mail_to - BOARD_MAGIC);
      d->mail_to = 0;
    }
    d->str = NULL;

    if (!d->connected && d->character && !IS_NPC(d->character))
      PLR_FLAGS(d->character).RemoveBit(PLR_WRITING);
  } else {
    // Only add a newline if it's not just /**/ with nothing else in it.
    if (strcmp(*d->str, "/**/") != 0)
      strlcat(*d->str, "\r\n", d->max_str);
  }
}


/* **********************************************************************
*  Modification of character spells                                     *
********************************************************************** */

ACMD(do_spellset)
{
  struct char_data *vict;
  char name[MAX_INPUT_LENGTH], help[MAX_STRING_LENGTH];
  int spelltoset, force, i, qend;

  argument = one_argument(argument, name);

  if (!*name) {                 /* no arguments. print an informative text */
    send_to_char("Syntax: spellset <name> '<spell>' <force>\r\n", ch);
    strlcpy(help, "Spell being one of the following:\r\n", sizeof(help));
    for ( i = 0; i < MAX_SPELLS; i++) {
      if (*spells[i].name == '!')
        continue;
      snprintf(ENDOF(help), sizeof(help) - strlen(help), "%18s", spells[i].name);
      if (i % 4 == 3) {
        strlcat(help, "\r\n", sizeof(help));
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
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", name);
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
    send_to_char("Spell must be enclosed in: ''\r\n", ch);
    return;
  }

  strlcpy(help, (argument + 1), sizeof(help));
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

  if (force >= 1 && (GET_TRADITION(vict) == TRAD_ADEPT || GET_TRADITION(vict) == TRAD_MUNDANE || GET_ASPECT(vict) == ASPECT_CONJURER)) {
    send_to_char(ch, "%s can't learn spells. You can only delete them from them with Force = 0.\r\n", GET_CHAR_NAME(vict));
    return;
  }

  if (force > GET_SKILL(vict, SKILL_SORCERY)) {
    send_to_char(ch, "Max value for force for %s is %d.\r\n", GET_CHAR_NAME(vict), GET_SKILL(vict, SKILL_SORCERY));
    return;
  }

  int subtype = 0;
  // Require that the attribute spells have an attribute specified (see spell->subtype comment).
  if (spelltoset == SPELL_INCATTR || spelltoset == SPELL_INCCYATTR ||
      spelltoset == SPELL_DECATTR || spelltoset == SPELL_DECCYATTR)
  {
    // Check for the argument.
    if (!*argument) {
      send_to_char("You must supply one of 'bod', 'qui', 'str', 'int', 'wil', 'cha'. (ex: spellset X 'increase' 6 wil)\r\n", ch);
      return;
    }

    // Compare against the new short_attributes consts in constants.cpp and find the applicable one.
    for (subtype = 0; subtype < 6; subtype++) {
      if (!strncmp(argument, short_attributes[subtype], strlen(argument))) {
        // The goal here is to find the index number the argument represents.
        break;
      }
    }

    // If we hit 6, the argument did not match the list.
    if (subtype >= 6) {
      send_to_char("You must supply one of 'bod', 'qui', 'str', 'int', 'wil', 'cha'.\r\n", ch);
      return;
    }
  }

  // Checks for validity (if the character is unable to use the spell, don't set it).
  if (force > 0) {
    if ((GET_ASPECT(vict) == ASPECT_ELEMFIRE && spells[spelltoset].category != COMBAT) ||
        (GET_ASPECT(vict) == ASPECT_ELEMEARTH && spells[spelltoset].category != MANIPULATION) ||
        (GET_ASPECT(vict) == ASPECT_ELEMWATER && spells[spelltoset].category != ILLUSION) ||
        (GET_ASPECT(vict) == ASPECT_ELEMAIR && spells[spelltoset].category != DETECTION)) {
      send_to_char("Spell is not compatible with character's aspect.\r\n", ch);
      return;
    }

    if (GET_ASPECT(vict) == ASPECT_SHAMANIST) {
      int skill = 0, target = 0;
      totem_bonus(vict, 0, spelltoset, target, skill);
      if (skill < 1) {
        send_to_char("Spell is not compatible with character's totem.\r\n", ch);
        return;
      }
    }
  }

  char spellname[500];
  strlcpy(spellname, spells[spelltoset].name, sizeof(spellname));
  if (spelltoset == SPELL_INCATTR || spelltoset == SPELL_INCCYATTR ||
      spelltoset == SPELL_DECATTR || spelltoset == SPELL_DECCYATTR)
  {
    strlcat(spellname, attributes[subtype], sizeof(spellname));
  }

  struct spell_data *spell = NULL;

  // Find the spell if they have it already.
  for (spell = GET_SPELLS(vict); spell; spell = spell->next) {
    if (spell->type == spelltoset && spell->subtype == subtype) {
      // Found it! Handle accordingly.
      int old_force = spell->force;

      // Spell removal, or just set it?
      if (force <= 0) {
        struct spell_data *temp;
        REMOVE_FROM_LIST(spell, GET_SPELLS(vict), next);
        delete spell;
        spell = NULL;
      } else {
        spell->force = force;
      }

      send_to_char(ch, "%s's %s changed from force %d to %d.\r\n", GET_CHAR_NAME(vict), spellname, old_force, force);
      snprintf(buf, sizeof(buf), "$n has set your '%s' spell to Force %d (from %d).", spellname, force, old_force);
      act(buf, TRUE, ch, NULL, vict, TO_VICT);
      snprintf(buf, sizeof(buf), "%s set %s's '%s' spell to Force %d (was %d).", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), spellname, force, old_force);
      mudlog(buf, ch, LOG_WIZLOG, TRUE);
      GET_SPELLS_DIRTY_BIT(ch) = TRUE;
      return;
    }
  }

  // They didn't have it before, so we can just call it done.
  if (force <= 0) {
    send_to_char(ch, "%s doesn't have the %s spell.\r\n", GET_CHAR_NAME(vict), spellname);
    return;
  }

  spell = new spell_data;

  /* spell->name is a pointer to a char, and other code (spell release etc) deletes what it points to
      on cleanup. Thus, you need to clone the spell_type's name into the spell_data to assign your
      new spell a name. */
  spell->name = str_dup(spellname);

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
  GET_SPELLS_DIRTY_BIT(ch) = TRUE;

  send_to_char("OK.\r\n", ch);
  snprintf(buf, sizeof(buf), "$n has given you the '%s' spell at Force %d.", spellname, force);
  act(buf, TRUE, ch, NULL, vict, TO_VICT);
  snprintf(buf, sizeof(buf), "%s set %s's '%s' spell to Force %d (was 0).", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), spellname, force);
  mudlog(buf, ch, LOG_WIZLOG, TRUE);

  // Save them.
  playerDB.SaveChar(vict);
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
    strlcpy(help, "Skill being one of the following:\r\n", sizeof(help));
    for (i = 0; i < MAX_SKILLS; i++) {
      if (*skills[i].name == '!')
        continue;
      snprintf(ENDOF(help), sizeof(help) - strlen(help), "%18s", skills[i].name);
      if (i % 4 == 3 || PRF_FLAGGED(ch, PRF_SCREENREADER)) {
        strlcat(help, "\r\n", sizeof(help));
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
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", name);
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
  strlcpy(help, (argument + 1), sizeof(help));
  help[qend - 1] = '\0';

  if (!strn_cmp(help, "orzet", strlen("orzet"))) {
    skill = SKILL_ORZET;
  } else if ((skill = find_skill_num(help)) <= 0) {
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
  if (value > MAX_SKILL_LEVEL_FOR_IMMS) {
    send_to_char(ch, "Max value for learned is %d.\r\n", MAX_SKILL_LEVEL_FOR_IMMS);
    return;
  }
  if (IS_NPC(vict)) {
    send_to_char("You can't set NPC skills.\r\n", ch);
    return;
  }
  if (GET_SKILL(vict, skill) == value) {
    send_to_char(ch, "%s's %s is already at %d.\r\n", capitalize(GET_CHAR_NAME(vict)), skills[skill].name, value);
    return;
  }

  snprintf(buf2, sizeof(buf2), "%s changed %s's %s from %d to %d.",
          GET_CHAR_NAME(ch), GET_CHAR_NAME(vict),
          skills[skill].name,
          GET_SKILL(vict, skill), value);
  mudlog(buf2, ch, LOG_WIZLOG, TRUE);

  send_to_char(ch, "You change %s's %s from %d to %d.\r\n", GET_CHAR_NAME(vict), skills[skill].name, GET_SKILL(vict, skill), value);
  send_to_char(vict, "Your skill in %s has been altered by the game's administration.\r\n", skills[skill].name);
  set_character_skill(vict, skill, value, TRUE);
}

ACMD(do_abilityset)
{
  struct char_data *vict;
  char name[100], buf2[100], buf[100], help[MAX_STRING_LENGTH];
  int ability, value, i, qend;

  argument = one_argument(argument, name);

  if (!*name) {                 /* no arguments. print an informative text */
    send_to_char("Syntax: abilityset <name> '<skill>' <value>\r\n", ch);
    strlcpy(help, "Ability being one of the following:\r\n", sizeof(help));
    for (i = 1; i < ADEPT_NUMPOWER; i++) {
      snprintf(ENDOF(help), sizeof(help) - strlen(help), "%32s", adept_powers[i]);
      if (i % 3 == 2 || PRF_FLAGGED(ch, PRF_SCREENREADER)) {
        strlcat(help, "\r\n", sizeof(help));
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
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", name);
    return;
  }

  if (GET_TRADITION(vict) != TRAD_ADEPT) {
    send_to_char(ch, "%s is not an Adept.\r\n", capitalize(GET_CHAR_NAME(vict)));
    return;
  }

  skip_spaces(&argument);

  /* If there is no chars in argument */
  if (!*argument) {
    send_to_char("Ability name expected.\r\n", ch);
    return;
  }
  if (*argument != '\'') {
    send_to_char("Ability must be enclosed in single quotes (').\r\n", ch);
    return;
  }
  /* Locate the last quote && lowercase the magic words (if any) */

  for (qend = 1; *(argument + qend) && (*(argument + qend) != '\''); qend++)
    *(argument + qend) = LOWER(*(argument + qend));

  if (*(argument + qend) != '\'') {
    send_to_char("Ability must be enclosed in single quotes (').\r\n", ch);
    return;
  }
  strlcpy(help, (argument + 1), sizeof(help));
  help[qend - 1] = '\0';
  if ((ability = find_ability_num(help)) <= 0) {
    send_to_char("Unrecognized ability.\r\n", ch);
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
  if (value > 50) {
    send_to_char(ch, "Max value for learned is %d.\r\n", 50);
    return;
  }
  if (IS_NPC(vict)) {
    send_to_char("You can't set NPC abilities.\r\n", ch);
    return;
  }
  if (GET_POWER_TOTAL(vict, ability) == value) {
    send_to_char(ch, "%s's %s is already at %d.\r\n", capitalize(GET_CHAR_NAME(vict)), adept_powers[ability], value);
    return;
  }

  snprintf(buf2, sizeof(buf2), "%s changed %s's %s from %d to %d.",
          GET_CHAR_NAME(ch), GET_CHAR_NAME(vict),
          adept_powers[ability],
          GET_POWER_TOTAL(vict, ability),
          value);
  mudlog(buf2, ch, LOG_WIZLOG, TRUE);

  send_to_char(ch, "You change %s's %s from %d to %d.\r\n", GET_CHAR_NAME(vict), adept_powers[ability], GET_POWER_TOTAL(vict, ability), value);
  send_to_char(vict, "Your abilities in %s has been altered by the game's administration (%d to %d).\r\n",
               adept_powers[ability], GET_POWER_TOTAL(vict, ability), value);
  SET_POWER_TOTAL(vict, ability, value);
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
    int showstr_head_len = strlen(str) + 1;
    d->showstr_head = new char[showstr_head_len];
    strlcpy(d->showstr_head, str, showstr_head_len);
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

void format_tabs(struct descriptor_data *d) {
  if (!d || !d->str || !*d->str || !*(*d->str))
    return;

  int tabcount = 0;

  // Replace all instances of "\t" with '\t' (backslash-t to tab)
  for (unsigned long i = 0; i < strlen(*d->str) - 2; i++)
    if (*(*d->str + i) == '\\' && *(*d->str + i + 1) == 't')
      tabcount++;

  // If there were an even number of tabs, we assume they didn't fuck up and go ahead and do a replacement.
  if (tabcount % 2 == 0) {
    for (unsigned long i = 0; i < strlen(*d->str) - 2; i++) {
      if (*(*d->str + i) == '\\' && *(*d->str + i + 1) == 't') {
        *(*d->str + i) = '\t';
        tabcount++;
        for (unsigned long j = i+1; j < strlen(*d->str) - 2; j++)
          *(*d->str + j) = *(*d->str + j + 1);
      }
    }
  }
}
