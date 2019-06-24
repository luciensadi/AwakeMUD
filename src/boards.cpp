/* ************************************************************************
*   File: boards.c                                      Part of CircleMUD *
*  Usage: handling of multiple bulletin boards                            *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/* FEATURES & INSTALLATION INSTRUCTIONS ***********************************
 
Written by Jeremy "Ras" Elson (jelson@cs.jhu.edu)
 
TO ADD A NEW BOARD, simply follow our easy 4-step program:
 
1 - Create a new board object in the object files
2 - Increase the NUM_OF_BOARDS constant in awake.h
3 - Add a new line to the board_info array below.  The fields, in order, are:
        Board's virtual number.
        Min level one must be to look at this board or read messages on it.
        Min level one must be to post a message to the board.
        Min level one must be to remove other people's messages from this
                board (but you can always remove your own message).
        Filename of this board, in quotes.
4 - In spec_procs.c, find the section which assigns the special procedure
    gen_board to the other bulletin boards, and add your new one in a
    similar fashion.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#if defined(WIN32) && !defined(__CYGWIN__)
#else
#include <unistd.h>
#include <sys/time.h>
#endif
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "boards.h"
#include "interpreter.h"
#include "handler.h"
#include "awake.h"
#include "newmatrix.h"

/* format:      vnum, read lvl, write lvl, remove lvl, filename
   Be sure to also change NUM_OF_BOARDS in awake.h */

struct board_info_type board_info[NUM_OF_BOARDS] =
  {
    {3   , LVL_BUILDER, LVL_BUILDER, LVL_VICEPRES, "etc/board.rift"},
    {4   , LVL_BUILDER, LVL_BUILDER, LVL_PRESIDENT, "etc/board.pook"},
    {12  , LVL_FIXER, LVL_FIXER, LVL_VICEPRES, "etc/board.dunkelzahn"},
    {22  , LVL_ADMIN, LVL_ADMIN, LVL_ADMIN, "etc/board.oldchange"},
    {26  , 0, 0, LVL_ADMIN, "etc/board.rp"},
    {28  , 0, 0, LVL_ADMIN, "etc/board.quest"},
    {31  , LVL_BUILDER, LVL_BUILDER, LVL_VICEPRES, "etc/board.senate"},
    {50  , LVL_BUILDER, LVL_BUILDER, LVL_VICEPRES, "etc/board.harlequin"},
    {1006, LVL_BUILDER, LVL_BUILDER, LVL_ADMIN, "etc/board.builder"},
    {1007, LVL_BUILDER, LVL_BUILDER, LVL_ADMIN, "etc/board.coder"},
    {66  , LVL_BUILDER, LVL_BUILDER, LVL_ADMIN, "etc/board.lofwyr"},
    {10018, LVL_BUILDER, LVL_BUILDER, LVL_ADMIN, "etc/board.immhq"}
//  {2104, 0, 0, LVL_ADMIN, "etc/board.mort"},
//  {2106, LVL_BUILDER, LVL_BUILDER, LVL_ADMIN, "etc/board.immort"},
//  {64900, 0, 0, LVL_BUILDER, "etc/board.rpe"},
//  {65207, 0, 0, LVL_ADMIN, "etc/board.teamBackstage"},
//  {4603, 0, 0, LVL_ADMIN, "etc/board.teamdds"},
//  {65126, 0, 0, LVL_ADMIN, "etc/board.seattlevoice"},
//  {26150, LVL_BUILDER, LVL_BUILDER, LVL_ADMIN, "etc/board.initiation"},
//  {11482, 0, 0, LVL_BUILDER, "etc/board.hooligan"},
//  {70405, 0, 0, LVL_BUILDER, "etc/board.marund"},
//  {71025, 0, 0, LVL_BUILDER, "etc/board.shadowcourier"}
  };


char *msg_storage[INDEX_SIZE];
int msg_storage_taken[INDEX_SIZE];
int num_of_msgs[NUM_OF_BOARDS];
int CMD_READ, CMD_LOOK, CMD_EXAMINE, CMD_WRITE, CMD_REMOVE, CMD_REPLY, CMD_LIST;
int CMD_MTX_READ, CMD_MTX_REPLY, CMD_MTX_WRITE, CMD_MTX_REMOVE, CMD_MTX_LIST;
struct board_msginfo msg_index[NUM_OF_BOARDS][MAX_BOARD_MESSAGES];

static int find_slot(void)
{
  int i;

  for (i = 0; i < INDEX_SIZE; i++)
    if (!msg_storage_taken[i]) {
      msg_storage_taken[i] = 1;
      return i;
    }
  return -1;
}

/* search the room ch is standing in to find which board they're looking at */
static int find_board(struct char_data * ch, struct obj_data **terminal)
{
  struct obj_data *obj;
  int i;

  if (ch->persona) {
    obj = matrix[ch->persona->in_host].file;
  } else {
    obj = ch->in_room->contents;
  }
    
  for (; obj; obj = obj->next_content) {
    for (i = 0; i < NUM_OF_BOARDS; i++) {
      if (BOARD_VNUM(i) == GET_OBJ_VNUM(obj)) {
        *terminal = obj;
        return i;
      }
    }
  }

  return -1;
}

void BoardInit(void)
{
  int i, j;

  for (i = 0; i < INDEX_SIZE; i++) {
    msg_storage[i] = 0;
    msg_storage_taken[i] = 0;
  }

  bool will_die = FALSE;
  for (i = 0; i < NUM_OF_BOARDS; i++) {
    if (real_object(BOARD_VNUM(i)) == -1) {
      // Changed this logic to print all broken board vnums instead of just one.
      log_vfprintf("--Board #%d does not exist.", BOARD_VNUM(i));
      will_die = TRUE;
    }
  }
  if (will_die){
    // Is this true, though? I don't see anything in the logic that would cause this. -Lucien
    log("WARNING: Missing boards were found (vnums above). Entire board system will be disabled.");
  }

  log("INFO: Loading boards. Any boards that log errors here will be functional but empty.");
  for (i = 0; i < NUM_OF_BOARDS; i++) {
    num_of_msgs[i] = 0;
    for (j = 0; j < MAX_BOARD_MESSAGES; j++) {
      memset((char *) &(msg_index[i][j]), 0, sizeof(struct board_msginfo));
      msg_index[i][j].slot_num = -1;
    }
    Board_load_board(i);
  }
  log("INFO: Board load complete.");

  CMD_READ = find_command("read");
  CMD_WRITE = find_command("write");
  CMD_REMOVE = find_command("remove");
  CMD_LOOK = find_command("look");
  CMD_EXAMINE = find_command("examine");
  CMD_REPLY = find_command("reply");
  CMD_LIST = find_command("list");
  CMD_MTX_READ = find_mcommand("read");
  CMD_MTX_WRITE = find_mcommand("write");
  CMD_MTX_REPLY = find_mcommand("reply");
  CMD_MTX_LIST = find_mcommand("list");
  CMD_MTX_REMOVE = find_mcommand("remove");
}

SPECIAL(gen_board)
{
  int board_type;
  struct obj_data *terminal = NULL;

  if (cmd != CMD_WRITE && cmd != CMD_LOOK && cmd != CMD_EXAMINE &&
      cmd != CMD_READ && cmd != CMD_REMOVE && cmd != CMD_REPLY &&
      cmd != CMD_LIST && cmd != CMD_MTX_READ && cmd != CMD_MTX_WRITE &&
      cmd != CMD_MTX_REPLY && cmd != CMD_MTX_LIST && cmd != CMD_MTX_REMOVE)
    return FALSE;

  if (!ch || !ch->desc || IS_ASTRAL(ch))
    return FALSE;

  if ((board_type = find_board(ch, &terminal)) == -1) {
    log("SYSERR:  degenerate board!  (what the hell...)");
    return 0;
  }
  if (cmd == (PLR_FLAGGED(ch, PLR_MATRIX) ? CMD_MTX_WRITE : CMD_WRITE)) {
    Board_write_message(board_type, terminal, ch, argument);
    return 1;
  } else if (cmd == (PLR_FLAGGED(ch, PLR_MATRIX) ? CMD_MTX_REPLY : CMD_REPLY))
    return (Board_reply_message(board_type, terminal, ch, argument));
  else if (cmd == (PLR_FLAGGED(ch, PLR_MATRIX) ? CMD_MTX_LIST : CMD_LIST))
    return (Board_list_board(board_type, terminal, ch, argument));
  else if (cmd == CMD_LOOK || cmd == CMD_EXAMINE)
    return (Board_show_board(board_type, terminal, ch, argument));
  else if (cmd == (PLR_FLAGGED(ch, PLR_MATRIX) ? CMD_MTX_READ : CMD_READ))
    return (Board_display_msg(board_type, terminal, ch, argument));
  else if (cmd == (PLR_FLAGGED(ch, PLR_MATRIX) ? CMD_MTX_REMOVE : CMD_REMOVE))
    return (Board_remove_msg(board_type, terminal, ch, argument));
  else
    return 0;
}

void Board_write_message(int board_type, struct obj_data *terminal,
                         struct char_data * ch, char *arg)
{
  char *tmstr;
  int len, i;
  time_t ct;
  char buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];

  if (!access_level(ch, WRITE_LVL(board_type)))
  {
    send_to_char(ch, "You do not have the qualifications to post on the %s.\r\n",
                 fname(terminal->text.keywords));
    return;
  }
  /* Prevent Astral beings from writing to boards */
  if( IS_ASTRAL(ch))
  {
    send_to_char("An astral being cannot use mundane items.\n\r",ch);
    return;
  }
  if (num_of_msgs[board_type] >= MAX_BOARD_MESSAGES)
    for (i = 0; i < num_of_msgs[board_type]; i++)
      if (MSG_LEVEL(board_type, i) < LVL_BUILDER)
      { /* <-- fixes annoyance. --Py */
        Board_delete_msg(board_type, i);
        break;
      }

  if ((NEW_MSG_INDEX(board_type).slot_num = find_slot()) == -1)
  {
    send_to_char(ch, "The %s is malfunctioning - sorry.\r\n",
                 fname(terminal->text.keywords));
    log("SYSERR: Board: failed to find empty slot on write.");
    return;
  }

  if (num_of_msgs[board_type] >= MAX_BOARD_MESSAGES)
  {
    send_to_char(ch, "The %s is full right now.\r\n",
                 fname(terminal->text.keywords));
    msg_storage_taken[NEW_MSG_INDEX(board_type).slot_num] = 0;
    return;
  }
  /* skip blanks */
  skip_spaces(&arg);
  delete_doubledollar(arg);

  if (!*arg)
  {
    send_to_char("We must have a headline!\r\n", ch);
    return;
  }
  ct = time(0);
  tmstr = (char *) asctime(localtime(&ct));
  *(tmstr + strlen(tmstr) - 1) = '\0';

  if (IS_NPC(ch))
    sprintf(buf2, "(%s)", GET_NAME(ch));
  else
    sprintf(buf2, "(%s)", GET_CHAR_NAME(ch));
  sprintf(buf, "%6.10s %-12s :: %s", tmstr, buf2, arg);
  len = strlen(buf) + 1;
  if (!(NEW_MSG_INDEX(board_type).heading = new char[len]))
  {
    send_to_char(ch, "The %s is malfunctioning - sorry.\r\n",
                 fname(terminal->text.keywords));
    msg_storage_taken[NEW_MSG_INDEX(board_type).slot_num] = 0;
    return;
  }
  strcpy(NEW_MSG_INDEX(board_type).heading, buf);
  NEW_MSG_INDEX(board_type).heading[len - 1] = '\0';
  NEW_MSG_INDEX(board_type).level = GET_LEVEL(ch);

  send_to_char("Type your message.  Terminate with a @ on a new line.\r\n\r\n", ch);
  sprintf(buf2, "$n starts to type a file into the %s.",
          fname(terminal->text.keywords));
  act(buf2, TRUE, ch, 0, 0, TO_ROOM);

  if (!IS_NPC(ch))
    PLR_FLAGS(ch).SetBit(PLR_WRITING);

  ch->desc->str = &(msg_storage[NEW_MSG_INDEX(board_type).slot_num]);
  ch->desc->max_str = MAX_MESSAGE_LENGTH;
  ch->desc->mail_to = board_type + BOARD_MAGIC;

  num_of_msgs[board_type]++;
}

int Board_reply_message(int board_type, struct obj_data *terminal,
                        struct char_data * ch, char *arg)
{
  char *tmstr, *heading;
  int len, i, msg;
  time_t ct;
  char buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];

  skip_spaces(&arg);

  if (!(msg = atoi(arg)))
    return 0;

  for (i = 0; *(arg+i); i++)
    if (!isdigit(*(arg+i)))
      return 0;

  if (!access_level(ch, WRITE_LVL(board_type))
      || !access_level(ch, READ_LVL(board_type)))
  {
    send_to_char(ch, "You do not have the qualifications to reply on the %s.\r\n",
                 fname(terminal->text.keywords));
    return 1;
  }
  /* Prevent Astral beings from writing to boards */
  if( IS_ASTRAL(ch))
  {
    send_to_char("An astral being cannot use mundane items.\n\r",ch);
    return 1;
  }
  if (!num_of_msgs[board_type])
  {
    send_to_char(ch, "The %s has no files.\r\n",
                 fname(terminal->text.keywords));
    return 1;
  }

  if (msg < 1 || msg > num_of_msgs[board_type])
  {
    send_to_char("That file exists only in your imagination.\r\n", ch);
    return 1;
  }
  msg--;
  if (MSG_SLOTNUM(board_type, msg) < 0 || MSG_SLOTNUM(board_type, msg) >= INDEX_SIZE)
  {
    send_to_char(ch, "Sorry, the %s is not working.\r\n",
                 fname(terminal->text.keywords));
    log("SYSERR: Terminal is screwed up.");
    return 1;
  }
  if (!(MSG_HEADING(board_type, msg)))
  {
    send_to_char("That file appears to be screwed up.\r\n", ch);
    return 1;
  }
  if (!(msg_storage[MSG_SLOTNUM(board_type, msg)]))
  {
    send_to_char("That file seems to be empty.\r\n", ch);
    return 1;
  }

  if (num_of_msgs[board_type] >= MAX_BOARD_MESSAGES)
    for (i = 0; i < num_of_msgs[board_type]; i++)
      if (MSG_LEVEL(board_type, i) < LVL_VICEPRES)
      {
        Board_delete_msg(board_type, i);
        break;
      }

  if ((NEW_MSG_INDEX(board_type).slot_num = find_slot()) == -1)
  {
    send_to_char(ch, "The %s is malfunctioning - sorry.\r\n",
                 fname(terminal->text.keywords));
    log("SYSERR: Board: failed to find empty slot on write.");
    return 1;
  }

  if (num_of_msgs[board_type] >= MAX_BOARD_MESSAGES)
  {
    send_to_char(ch, "The %s is full right now.\r\n",
                 fname(terminal->text.keywords));
    msg_storage_taken[NEW_MSG_INDEX(board_type).slot_num] = 0;
    return 1;
  }

  ct = time(0);
  tmstr = (char *) asctime(localtime(&ct));
  *(tmstr + strlen(tmstr) - 1) = '\0';

  if (IS_NPC(ch))
    sprintf(buf2, "(%s)", GET_NAME(ch));
  else
    sprintf(buf2, "(%s)", GET_CHAR_NAME(ch));

  for (i = 0; *(MSG_HEADING(board_type, msg)+i) &&
       *(MSG_HEADING(board_type, msg)+i) != ':'; i++)
    ;
  i += 3;
  heading = (MSG_HEADING(board_type, msg) + i);
  if (strstr(heading, "Re: "))
    sprintf(buf, "%6.10s %-12s :: %s", tmstr, buf2, heading);
  else
    sprintf(buf, "%6.10s %-12s :: Re: %s", tmstr, buf2, heading);
  len = strlen(buf) + 1;
  if (!(NEW_MSG_INDEX(board_type).heading = new char[len]))
  {
    send_to_char(ch, "The %s is malfunctioning - sorry.\r\n",
                 fname(terminal->text.keywords));
    msg_storage_taken[NEW_MSG_INDEX(board_type).slot_num] = 0;
    return 1;
  }
  strcpy(NEW_MSG_INDEX(board_type).heading, buf);
  NEW_MSG_INDEX(board_type).heading[len - 1] = '\0';
  NEW_MSG_INDEX(board_type).level = GET_LEVEL(ch);

  send_to_char("Type your message.  Terminate with a @ on a new line.\r\n\r\n", ch);
  sprintf(buf2, "$n starts to type a file into the %s.",
          fname(terminal->text.keywords));
  act(buf2, TRUE, ch, 0, 0, TO_ROOM);

  if (!IS_NPC(ch))
    PLR_FLAGS(ch).SetBit(PLR_WRITING);

  ch->desc->str = &(msg_storage[NEW_MSG_INDEX(board_type).slot_num]);
  ch->desc->max_str = MAX_MESSAGE_LENGTH;
  ch->desc->mail_to = board_type + BOARD_MAGIC;

  num_of_msgs[board_type]++;
  return 1;
}

int Board_list_board(int board_type, struct obj_data *terminal,
                     struct char_data *ch, char *arg)
{
  int first = 0, second = 0, i;
  char one[LINE_LENGTH], two[LINE_LENGTH], buf[MAX_STRING_LENGTH*2];
  char *ptr1 = one, *ptr2 = two;
  bool all = FALSE;

  if (!ch->desc)
    return 0;

  two_arguments(arg, ptr1, ptr2);

  if ((!*ptr1 && !*ptr2) || !str_cmp(ptr1, "all"))
    all = TRUE;
  else if (!*ptr1 || (!*ptr2 && *ptr1 != '+'))
  {
    send_to_char(ch, "Usage: list <all | # #>\r\n");
    return 1;
  }

  if (!all)
  {
    if (isdigit(*ptr1)) {
      first = atoi(ptr1) - 1;
      second = atoi(ptr2) - 1;
    } else if (*ptr1 == '+' && isdigit(*(ptr1 + 1))) {
      ptr1++;
      first = atoi(ptr1);
      second = num_of_msgs[board_type] - 1;
    }
    if (first < 0 || second < 0 || first >= MAX_BOARD_MESSAGES ||
        second >= MAX_BOARD_MESSAGES) {
      send_to_char(ch, "Values must range from 1 to %d.\r\n", MAX_BOARD_MESSAGES);
      return 1;
    }
    second = MIN(second, num_of_msgs[board_type] - 1);
  }

  if (!access_level(ch, READ_LVL(board_type)))
  {
    send_to_char("You try but fail to understand the encrypted words.\r\n", ch);
    return 1;
  }

  sprintf(buf, "$n accesses the %s.", fname(terminal->text.keywords));
  act(buf, TRUE, ch, 0, 0, TO_ROOM);

  if (!num_of_msgs[board_type])
  {
    send_to_char(ch, "The %s has no files.\r\n",
                 fname(terminal->text.keywords));
    return 1;
  }
  if (all)
  {
    if (num_of_msgs[board_type] == 1) {
      sprintf(buf, "There is 1 file on the %s.\r\n",
              fname(terminal->text.keywords));
    } else {
      sprintf(buf, "There are %d files on the %s.\r\n",
              num_of_msgs[board_type],
              fname(terminal->text.keywords));
    }
    for (i = 0; i < num_of_msgs[board_type]; i++) {
      if (MSG_HEADING(board_type, i))
        sprintf(buf + strlen(buf), "%-3d: %s^n\r\n", i + 1, MSG_HEADING(board_type, i));
      else {
        log("SYSERR: The terminal is fubar'd.");
        send_to_char(ch, "Sorry, the %s isn't working.\r\n",
                     fname(terminal->text.keywords));
        return 1;
      }
    }
  } else
  {
    if (MAX(second - first + 1, 0) == 1) {
      sprintf(buf, "There is 1 file on the %s in the specified range.\r\n",
              fname(terminal->text.keywords));
    } else {
      sprintf(buf, "There are %d files on the %s in the specified range.\r\n",
              MAX(second - first + 1, 0),
              fname(terminal->text.keywords));
    }
    for (i = first; i <= second; i++) {
      if (MSG_HEADING(board_type, i))
        sprintf(buf + strlen(buf), "%-3d: %s\r\n", i + 1, MSG_HEADING(board_type, i));
      else {
        log("SYSERR: The terminal is fubar'd.");
        send_to_char(ch, "Sorry, the %s isn't working.\r\n",
                     fname(terminal->text.keywords));
        return 1;
      }
    }
  }

  page_string(ch->desc, buf, 1);
  return 1;
}

int Board_show_board(int board_type, struct obj_data *terminal,
                     struct char_data *ch, char *arg)
{
  char tmp[MAX_STRING_LENGTH], buf[MAX_STRING_LENGTH];

  if (!ch->desc)
    return 0;

  one_argument(arg, tmp);

  if (!*tmp || !isname(tmp, terminal->text.keywords))
    return 0;

  if (!access_level(ch, READ_LVL(board_type)))
  {
    send_to_char("You try but fail to understand the encrypted words.\r\n", ch);
    return 1;
  }
  sprintf(buf, "$n accesses the %s.", fname(terminal->text.keywords));
  act(buf, TRUE, ch, 0, 0, TO_ROOM);

  sprintf(buf, "This is a computer bulletin board system.\r\n"
          "Usage: LIST, LIST <+# | # #>, READ/REMOVE/REPLY #, WRITE <header>.\r\n"
          "You will need to look at the %s to save your file.\r\n",
          fname(terminal->text.keywords));

  if (!num_of_msgs[board_type]) {
    sprintf(ENDOF(buf), "The %s has no files.\r\n", fname(terminal->text.keywords));
  } else {
    if (num_of_msgs[board_type] == 1) {
      sprintf(ENDOF(buf), "There is 1 file on the %s.\r\n",
              fname(terminal->text.keywords));
    } else {
      sprintf(ENDOF(buf), "There are %d files on the %s.\r\n",
              num_of_msgs[board_type], fname(terminal->text.keywords));
    }
  }

  send_to_char(buf, ch);

  return 1;
}

int Board_display_msg(int board_type, struct obj_data *terminal,
                      struct char_data * ch, char *arg)
{
  char number[MAX_STRING_LENGTH], buffer[MAX_STRING_LENGTH];
  int msg = 0, ind;

  one_argument(arg, number);
  if (!*number)
    return 0;

  if (isname(number, terminal->text.keywords))
    return (Board_show_board(board_type, terminal, ch, arg));

  if (!isdigit(*number) || !(msg = atoi(number)))
    return 0;

  for (ind = 0; *(number+ind); ind++)
    if (!isdigit(*(number+ind)))
      return 0;

  if (!access_level(ch, READ_LVL(board_type)))
  {
    send_to_char("You try but fail to understand the encrypted words.\r\n", ch);
    return 1;
  }
  if (!num_of_msgs[board_type])
  {
    send_to_char(ch, "The %s has no files.\r\n",
                 fname(terminal->text.keywords));
    return (1);
  }
  if (msg < 1 || msg > num_of_msgs[board_type])
  {
    send_to_char("That file exists only in your imagination.\r\n", ch);
    return (1);
  }
  ind = msg - 1;
  if (MSG_SLOTNUM(board_type, ind) < 0 ||
      MSG_SLOTNUM(board_type, ind) >= INDEX_SIZE)
  {
    send_to_char(ch, "Sorry, the %s is not working.\r\n",
                 fname(terminal->text.keywords));
    log("SYSERR: Terminal is screwed up.");
    return 1;
  }
  if (!(MSG_HEADING(board_type, ind)))
  {
    send_to_char("That file appears to be screwed up.\r\n", ch);
    return 1;
  }
  if (!(msg_storage[MSG_SLOTNUM(board_type, ind)]))
  {
    send_to_char("That file seems to be empty.\r\n", ch);
    return 1;
  }
  sprintf(buffer, "File %d : %s\r\n\r\n%s^n\r\n", msg,
          MSG_HEADING(board_type, ind), msg_storage[MSG_SLOTNUM(board_type, ind)]);

  page_string(ch->desc, buffer, 1);

  return 1;
}

void Board_delete_msg(int board_type, int ind)
{
  struct descriptor_data *d;
  int slot_num;

  slot_num = MSG_SLOTNUM(board_type, ind);

  if (!MSG_HEADING(board_type, ind) || slot_num < 0 || slot_num >= INDEX_SIZE)
    return;

  for (d = descriptor_list; d; d = d->next)
    if (!d->connected && d->str == &(msg_storage[slot_num]))
      return;

  DELETE_ARRAY_IF_EXTANT(msg_storage[slot_num]);
  msg_storage_taken[slot_num] = 0;
  DELETE_ARRAY_IF_EXTANT(MSG_HEADING(board_type, ind));

  for (; ind < num_of_msgs[board_type] - 1; ind++) {
    MSG_HEADING(board_type, ind) = MSG_HEADING(board_type, ind + 1);
    MSG_SLOTNUM(board_type, ind) = MSG_SLOTNUM(board_type, ind + 1);
    MSG_LEVEL(board_type, ind) = MSG_LEVEL(board_type, ind + 1);
  }
  num_of_msgs[board_type]--;
  Board_save_board(board_type);
}

int Board_remove_msg(int board_type, struct obj_data *terminal,
                     struct char_data * ch, char *arg)
{
  int ind, msg;
  char number[MAX_INPUT_LENGTH], buf[MAX_INPUT_LENGTH];
  struct descriptor_data *d;

  one_argument(arg, number);

  if (!*number || !isdigit(*number))
    return 0;
  if (!(msg = atoi(number)))
    return (0);

  for (ind = 0; *(number+ind); ind++)
    if (!isdigit(*(number+ind)))
      return 0;

  if (!num_of_msgs[board_type])
  {
    send_to_char(ch, "The %s is empty!\r\n",
                 fname(terminal->text.keywords));
    return 1;
  }
  if (msg < 1 || msg > num_of_msgs[board_type])
  {
    send_to_char("That file exists only in your imagination.\r\n", ch);
    return 1;
  }
  ind = msg - 1;
  if (!MSG_HEADING(board_type, ind))
  {
    send_to_char("That file appears to be screwed up.\r\n", ch);
    return 1;
  }
  sprintf(buf, "(%s)", GET_CHAR_NAME(ch));
  if (!access_level(ch, REMOVE_LVL(board_type))
      && !(strstr((const char *)MSG_HEADING(board_type, ind), buf)))
  {
    send_to_char("You do not have the correct qualifications to remove other people's files.\r\n", ch);
    return 1;
  }
  if (!access_level(ch, MSG_LEVEL(board_type, ind))
      && !access_level(ch, LVL_VICEPRES))
  {
    send_to_char("You cannot remove this file without proper authorization.\r\n", ch);
    return 1;
  }

  if (MSG_SLOTNUM(board_type, ind) < 0 || MSG_SLOTNUM(board_type, ind) >= INDEX_SIZE)
  {
    log("SYSERR: The terminal is seriously screwed up.");
    send_to_char("That file is majorly screwed up.\r\n", ch);
    return 1;
  }
  for (d = descriptor_list; d; d = d->next)
    if (!d->connected && d->str == &(msg_storage[MSG_SLOTNUM(board_type, ind)]))
    {
      send_to_char("At least wait until the author is finished before removing it!\r\n", ch);
      return 1;
    }

  Board_delete_msg(board_type, ind);

  send_to_char("File deleted.\r\n", ch);
  sprintf(buf, "$n just deleted file %d.", msg);
  act(buf, FALSE, ch, 0, 0, TO_ROOM);

  return 1;
}

void Board_save_board(int board_type)
{
  FILE *fl;
  int i;
  char *tmp1 = 0, *tmp2 = 0;

  if (!num_of_msgs[board_type]) {
    unlink(FILENAME(board_type));
    return;
  }
  if (!(fl = fopen(FILENAME(board_type), "wb"))) {
    perror("Error writing board");
    return;
  }
  fwrite(&(num_of_msgs[board_type]), sizeof(int), 1, fl);

  for (i = 0; i < num_of_msgs[board_type]; i++) {
    if ((tmp1 = MSG_HEADING(board_type, i)))
      msg_index[board_type][i].heading_len = strlen(tmp1) + 1;
    else
      msg_index[board_type][i].heading_len = 0;

    if (MSG_SLOTNUM(board_type, i) < 0 ||
        MSG_SLOTNUM(board_type, i) >= INDEX_SIZE ||
        (!(tmp2 = msg_storage[MSG_SLOTNUM(board_type, i)])))
      msg_index[board_type][i].message_len = 0;
    else
      msg_index[board_type][i].message_len = strlen(tmp2) + 1;

    fwrite(&(msg_index[board_type][i]), sizeof(struct board_msginfo), 1, fl);
    if (tmp1)
      fwrite(tmp1, sizeof(char), msg_index[board_type][i].heading_len, fl);
    if (tmp2)
      fwrite(tmp2, sizeof(char), msg_index[board_type][i].message_len, fl);
  }

  fclose(fl);
}

void Board_load_board(int board_type)
{
  FILE *fl;
  int i, len1 = 0, len2 = 0;
  char *tmp1 = NULL, *tmp2 = NULL;

  if (!(fl = fopen(FILENAME(board_type), "rb"))) {
    sprintf(buf, "Error reading board file %s", FILENAME(board_type));
    perror(buf);
    return;
  }
  fread(&(num_of_msgs[board_type]), sizeof(int), 1, fl);
  if (num_of_msgs[board_type] < 1 || num_of_msgs[board_type] > MAX_BOARD_MESSAGES) {
    log("SYSERR: Board file corrupt.  Resetting.");
    Board_reset_board(board_type);
    return;
  }
  for (i = 0; i < num_of_msgs[board_type]; i++) {
    fread(&(msg_index[board_type][i]), sizeof(struct board_msginfo), 1, fl);
    if (!(len1 = msg_index[board_type][i].heading_len)) {
      log("SYSERR: Board file corrupt!  Resetting.");
      Board_reset_board(board_type);
      return;
    }
    if (!(tmp1 = new char[len1])) {
      log("SYSERR: Error - new failed for board header");
      shutdown();
    }
    fread(tmp1, sizeof(char), len1, fl);
    MSG_HEADING(board_type, i) = tmp1;

    if ((len2 = msg_index[board_type][i].message_len)) {
      if ((MSG_SLOTNUM(board_type, i) = find_slot()) == -1) {
        log("SYSERR: Out of slots booting board!  Resetting...");
        Board_reset_board(board_type);
        return;
      }
      if (!(tmp2 = new char[len2])) {
        log("SYSERR: new failed for board text");
        shutdown();
      }
      fread(tmp2, sizeof(char), len2, fl);
      msg_storage[MSG_SLOTNUM(board_type, i)] = tmp2;
    }
  }

  fclose(fl);
}

void Board_reset_board(int board_type)
{
  int i;

  for (i = 0; i < MAX_BOARD_MESSAGES; i++) {
    DELETE_ARRAY_IF_EXTANT(MSG_HEADING(board_type, i));
    DELETE_IF_EXTANT(msg_storage[MSG_SLOTNUM(board_type, i)]);
    msg_storage_taken[MSG_SLOTNUM(board_type, i)] = 0;
    memset((char *)&(msg_index[board_type][i]),0,sizeof(struct board_msginfo));
    msg_index[board_type][i].slot_num = -1;
  }
  num_of_msgs[board_type] = 0;
  unlink(FILENAME(board_type));
}
