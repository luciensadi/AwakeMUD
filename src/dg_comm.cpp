/**************************************************************************
*  File: dg_comm.cpp                                                      *
*  Usage: Message rendering for the %send%/%echo% family of script         *
*         commands, including the ~ | ^ & * ` substitution tokens.         *
*                                                                         *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
*                                                                         *
*  $Author: Mark A. Heilpern/egreen/Welcor $                              *
*  $Date: 2004/10/11 12:07:00$                                            *
*  $Revision: 1.0.14 $                                                    *
*                                                                         *
*  Ported to AwakeMUD CE by Fizban.                                       *
**************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "constants.hpp"
#include "dg_scripts.hpp"

/* Whether this character is in any state to receive script output. A mob with
 * an act trigger counts even with no descriptor: that is how one mob's script
 * hears another's emote. */
#define DG_SENDOK(ch)  (((ch)->desc || SCRIPT_CHECK((ch), MTRIG_ACT)) && \
                        AWAKE(ch) && !PLR_FLAGGED((ch), PLR_WRITING))

/* local functions */
static void sub_write_to_char(struct char_data *ch, char *tokens[], void *otokens[], char type[]);

/* same as any_one_arg except that it stops at punctuation */
char *any_one_name(char *argument, char *first_arg)
{
  char *arg;

  /* Find first non blank */
  while (isspace(*argument))
    argument++;

  /* Find length of first word */
  for (arg = first_arg;
       *argument && !isspace(*argument) && (!ispunct(*argument) || *argument == UID_CHAR || *argument == '#' || *argument == '-');
       arg++, argument++)
    *arg = LOWER(*argument);
  *arg = '\0';

  return argument;
}

static void sub_write_to_char(struct char_data *ch, char *tokens[], void *otokens[], char type[])
{
  char sb[MAX_STRING_LENGTH];
  int i;

  *sb = '\0';

  for (i = 0; tokens[i + 1]; i++) {
    strlcat(sb, tokens[i], sizeof(sb));

    switch (type[i]) {
      case '~':
        if (!otokens[i])
          strlcat(sb, "someone", sizeof(sb));
        else if ((struct char_data *) otokens[i] == ch)
          strlcat(sb, "you", sizeof(sb));
        else
          strlcat(sb, PERS((struct char_data *) otokens[i], ch), sizeof(sb));
        break;

      case '|':
        if (!otokens[i]) {
          strlcat(sb, "someone's", sizeof(sb));
        } else if ((struct char_data *) otokens[i] == ch) {
          strlcat(sb, "your", sizeof(sb));
        } else {
          strlcat(sb, PERS((struct char_data *) otokens[i], ch), sizeof(sb));
          strlcat(sb, "'s", sizeof(sb));
        }
        break;

      case '^':
        if (!otokens[i] || !CAN_SEE(ch, (struct char_data *) otokens[i]))
          strlcat(sb, "its", sizeof(sb));
        else if (otokens[i] == ch)
          strlcat(sb, "your", sizeof(sb));
        else
          strlcat(sb, HSHR((struct char_data *) otokens[i]), sizeof(sb));
        break;

      case '&':
        if (!otokens[i] || !CAN_SEE(ch, (struct char_data *) otokens[i]))
          strlcat(sb, "it", sizeof(sb));
        else if (otokens[i] == ch)
          strlcat(sb, "you", sizeof(sb));
        else
          strlcat(sb, HSSH((struct char_data *) otokens[i]), sizeof(sb));
        break;

      case '*':
        if (!otokens[i] || !CAN_SEE(ch, (struct char_data *) otokens[i]))
          strlcat(sb, "it", sizeof(sb));
        else if (otokens[i] == ch)
          strlcat(sb, "you", sizeof(sb));
        else
          strlcat(sb, HMHR((struct char_data *) otokens[i]), sizeof(sb));
        break;

      case '`':
        if (!otokens[i])
          strlcat(sb, "something", sizeof(sb));
        else
          strlcat(sb, OBJS(((struct obj_data *) otokens[i]), ch), sizeof(sb));
        break;
    }
  }

  strlcat(sb, tokens[i], sizeof(sb));
  strlcat(sb, "\r\n", sizeof(sb));
  send_to_char(ch, "%s", sb);
}

void sub_write(char *arg, struct char_data *ch, byte find_invis, int targets)
{
  char str[MAX_INPUT_LENGTH * 2];
  char type[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH];
  char *tokens[MAX_INPUT_LENGTH], *s, *p;
  void *otokens[MAX_INPUT_LENGTH];
  struct char_data *to;
  struct obj_data *obj;
  int i, tmp;

  if (!arg || !ch || !ch->in_room)
    return;

  tokens[0] = str;

  for (i = 0, p = arg, s = str; *p;) {
    switch (*p) {
      case '~':
      case '|':
      case '^':
      case '&':
      case '*':
        /* get char_data, move to next token */
        type[i] = *p;
        *s = '\0';
        p = any_one_name(++p, name);
        otokens[i] = find_invis ? (void *) get_char_in_room(ch->in_room, name)
                                : (void *) get_char_room_vis(ch, name);
        tokens[++i] = ++s;
        break;

      case '`':
        /* get obj_data, move to next token */
        type[i] = *p;
        *s = '\0';
        p = any_one_name(++p, name);

        if (find_invis)
          obj = get_obj_in_room(ch->in_room, name);
        else if ((obj = get_obj_in_list_vis(ch, name, ch->in_room->contents)))
          ;
        else if ((obj = get_object_in_equip_vis(ch, name, ch->equipment, &tmp)))
          ;
        else
          obj = get_obj_in_list_vis(ch, name, ch->carrying);

        otokens[i] = (void *) obj;
        tokens[++i] = ++s;
        break;

      case '\\':
        p++;
        if (*p)
          *s++ = *p++;
        else
          *s++ = '\\';
        break;

      default:
        *s++ = *p++;
    }
  }

  *s = '\0';
  tokens[++i] = NULL;

  if (IS_SET(targets, TO_CHAR) && DG_SENDOK(ch))
    sub_write_to_char(ch, tokens, otokens, type);

  if (IS_SET(targets, TO_ROOM)) {
    for (to = ch->in_room->people; to; to = to->next_in_room)
      if (to != ch && DG_SENDOK(to))
        sub_write_to_char(to, tokens, otokens, type);
  }
}

/* Send a message to every awake player in a zone. Named with a dg_ prefix so
 * as not to look like one of comm.cpp's own send_to_* family. */
void dg_send_to_zone(const char *messg, int zone)
{
  struct descriptor_data *i;

  if (!messg || !*messg)
    return;

  for (i = descriptor_list; i; i = i->next)
    if (!i->connected && i->character && AWAKE(i->character) &&
        i->character->in_room && i->character->in_room->zone == zone)
      send_to_char(messg, i->character);
}
