/**************************************************************************
*  File: trigedit.cpp                                                     *
*  Usage: The trigger editor, and the attached-trigger sub-menu that       *
*         redit, medit and iedit hand off to.                              *
*                                                                         *
*  DG Scripts is part of the core tbaMUD source code distribution, which  *
*  is a derivative of, and continuation of, CircleMUD.                    *
*                                                                         *
*  $Author: Mark A. Heilpern/egreen/Welcor $                              *
*  $Date: 2004/10/11 12:07:00$                                            *
*  $Revision: 1.0.14 $                                                    *
*                                                                         *
*  Ported to AwakeMUD CE by Fizban. The menus are the ones a tbaMUD        *
*  builder knows; the plumbing underneath is AwakeMUD's own OLC.           *
**************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "dblist.hpp"
#include "screen.hpp"
#include "constants.hpp"
#include "olc.hpp"
#include "dg_scripts.hpp"
#include "dg_event.hpp"

extern void write_index_file(const char *suffix);
extern int get_zone_index_number_from_vnum(vnum_t vnum);
extern bool is_olc_available(struct char_data *ch);
extern class objList ObjList;

static void refresh_script_types(struct script_data *sc)
{
  if (!sc)
    return;
  SCRIPT_TYPES(sc) = 0;
  for (struct trig_data *t = TRIGGERS(sc); t; t = t->next)
    SCRIPT_TYPES(sc) |= GET_TRIG_TYPE(t);
}

/* Which trigger-type table goes with which attach type. The three are not the
 * same length, so the count comes from the table rather than from a constant. */
static const char **trigedit_type_table(int attach_type)
{
  switch (attach_type) {
    case WLD_TRIGGER: return wtrig_types;
    case OBJ_TRIGGER: return otrig_types;
    case MOB_TRIGGER:
    default:          return trig_types;
  }
}

static int trigedit_type_count(int attach_type)
{
  const char **types = trigedit_type_table(attach_type);
  int i;

  for (i = 0; *types[i] != '\n'; i++)
    ;

  return i;
}

static const char *trigedit_attach_name(int attach_type)
{
  switch (attach_type) {
    case WLD_TRIGGER: return "Rooms";
    case OBJ_TRIGGER: return "Objects";
    default:          return "Mobiles";
  }
}

/* ************************************************************************
*  Setting up the scratch trigger.                                         *
************************************************************************ */

void trigedit_setup_new(struct descriptor_data *d)
{
  struct trig_data *trig = new trig_data;

  trig->nr = NOTHING;
  trig->name = str_dup("new trigger");
  trig->trigger_type = MTRIG_GREET;
  trig->narg = 100;

  trig->cmdlist = new cmdlist_element;
  trig->cmdlist->cmd = str_dup("* This trigger has no commands yet.");

  d->edit_trig = trig;
  d->edit_number2 = 0; /* has-changed flag */
}

void trigedit_setup_existing(struct descriptor_data *d, rnum_t rtrg_num)
{
  struct trig_data *trig = new trig_data;
  struct cmdlist_element *src, *dst = NULL;

  trig_data_copy(trig, trig_index[rtrg_num]->proto);

  /* trig_data_copy leaves the prototype's command list shared, which is right
   * for a live trigger and wrong for the editor: take a copy the builder can
   * chop about without touching what is running in the world. */
  trig->cmdlist = NULL;
  for (src = trig_index[rtrg_num]->proto->cmdlist; src; src = src->next) {
    struct cmdlist_element *cle = new cmdlist_element;
    cle->cmd = str_dup(src->cmd ? src->cmd : "");

    if (dst)
      dst->next = cle;
    else
      trig->cmdlist = cle;
    dst = cle;
  }

  if (!trig->cmdlist) {
    trig->cmdlist = new cmdlist_element;
    trig->cmdlist->cmd = str_dup("* This trigger has no commands yet.");
  }

  d->edit_trig = trig;
  d->edit_number2 = 0; /* has-changed flag */
}

/* ************************************************************************
*  Menus.                                                                  *
************************************************************************ */

/* Flatten the command list into one string, for display and for editing. */
static void trigedit_render_commands(struct trig_data *trig, char *dest, size_t dest_size)
{
  size_t len = 0;

  *dest = '\0';

  for (struct cmdlist_element *cle = trig->cmdlist; cle; cle = cle->next) {
    if (len >= dest_size - 64) {
      snprintf(dest + len, dest_size - len, "*** script too long to display ***\r\n");
      return;
    }
    snprintf(dest + len, dest_size - len, "%s\r\n", cle->cmd ? cle->cmd : "");
    len = strlen(dest);
  }
}

void trigedit_disp_menu(struct descriptor_data *d)
{
  struct trig_data *trig = d->edit_trig;
  char trgtypes[MAX_STRING_LENGTH];
  char commands[MAX_STRING_LENGTH];

  sprintbit(GET_TRIG_TYPE(trig), trigedit_type_table(trig->attach_type), trgtypes, sizeof(trgtypes));
  trigedit_render_commands(trig, commands, sizeof(commands));

  CLS(CH);
  send_to_char(CH, "Trigger Editor [^c%ld^n]\r\n\r\n", d->edit_number);
  send_to_char(CH, "1) Name         : ^c%s^n\r\n", GET_TRIG_NAME(trig));
  send_to_char(CH, "2) Intended for : ^c%s^n\r\n", trigedit_attach_name(trig->attach_type));
  send_to_char(CH, "3) Trigger types: ^c%s^n\r\n", trgtypes);
  send_to_char(CH, "4) Numeric arg  : ^c%d^n\r\n", trig->narg);
  send_to_char(CH, "5) Arguments    : ^c%s^n\r\n", trig->arglist ? trig->arglist : "");
  send_to_char(CH, "6) Commands:\r\n^c%s^n\r\n", commands);
  send_to_char("w) Copy another trigger over this one\r\n", CH);
  send_to_char("q) Quit and save\r\n", CH);
  send_to_char("x) Exit and abort\r\n", CH);
  send_to_char("Enter your choice:\r\n", CH);

  d->edit_mode = TRIGEDIT_MAIN_MENU;
}

static void trigedit_disp_types(struct descriptor_data *d)
{
  int i, columns = 0;
  const char **types = trigedit_type_table(d->edit_trig->attach_type);
  char bitbuf[MAX_STRING_LENGTH];

  CLS(CH);

  for (i = 0; *types[i] != '\n'; i++)
    send_to_char(CH, "^c%2d^n) %-20.20s  %s", i + 1, types[i], !(++columns % 2) ? "\r\n" : "");

  sprintbit(GET_TRIG_TYPE(d->edit_trig), types, bitbuf, sizeof(bitbuf));
  send_to_char(CH, "\r\nCurrent types : ^c%s^n\r\nEnter type (0 to quit): ", bitbuf);

  d->edit_mode = TRIGEDIT_TYPES;
}

/* ************************************************************************
*  Saving.                                                                 *
************************************************************************ */

/* Grow the trigger index by one, keeping it sorted by vnum, and return the
 * rnum the new entry landed on. */
static rnum_t trigedit_insert_index(vnum_t vnum)
{
  struct trig_index_data **new_index = new struct trig_index_data *[top_of_trigt + 1]();
  rnum_t rnum = top_of_trigt;
  rnum_t i;
  bool found = FALSE;

  for (i = 0; i < top_of_trigt; i++) {
    if (!found && trig_index[i]->vnum > vnum) {
      found = TRUE;
      rnum = i;
      new_index[i + 1] = trig_index[i];
    } else if (found) {
      new_index[i + 1] = trig_index[i];
    } else {
      new_index[i] = trig_index[i];
    }
  }

  new_index[rnum] = new trig_index_data;
  new_index[rnum]->vnum = vnum;
  new_index[rnum]->number = 0;
  new_index[rnum]->proto = new trig_data;

  delete [] trig_index;
  trig_index = new_index;
  top_of_trigt++;

  /* Everything at or past the insertion point shifted up by one. */
  for (i = rnum; i < top_of_trigt; i++)
    if (trig_index[i]->proto)
      trig_index[i]->proto->nr = i;

  return rnum;
}

/* Point every live copy of this trigger at the freshly saved prototype. */
static void trigedit_refresh_live(rnum_t rnum, struct trig_data *proto)
{
  for (struct trig_data *live = trigger_list; live; live = live->next_in_world) {
    if (GET_TRIG_RNUM(live) != rnum)
      continue;

    DELETE_ARRAY_IF_EXTANT(live->arglist);
    DELETE_ARRAY_IF_EXTANT(live->name);

    if (proto->arglist)
      live->arglist = str_dup(proto->arglist);
    if (proto->name)
      live->name = str_dup(proto->name);

    /* The script it was part-way through no longer exists in the form it
     * was running, so stop it cleanly rather than resuming into new code. */
    if (GET_TRIG_WAIT(live)) {
      event_cancel(GET_TRIG_WAIT(live));
      GET_TRIG_WAIT(live) = NULL;
    }
    if (live->var_list) {
      free_varlist(live->var_list);
      live->var_list = NULL;
    }

    live->cmdlist = proto->cmdlist;
    live->curr_state = live->cmdlist;
    live->trigger_type = proto->trigger_type;
    live->attach_type = proto->attach_type;
    live->narg = proto->narg;
    live->data_type = proto->data_type;
    live->depth = 0;
  }
}

void trigedit_save(struct descriptor_data *d)
{
  struct trig_data *trig = d->edit_trig;
  struct trig_data *proto;
  rnum_t rnum;
  bool is_new = FALSE;

  if ((rnum = real_trigger(d->edit_number)) < 0) {
    rnum = trigedit_insert_index(d->edit_number);
    is_new = TRUE;

    /* Live triggers hold rnums, and everything at or past the insertion point
     * just moved up. Zone reset commands are safe: this port keeps the T
     * command's trigger argument as a vnum. */
    for (struct trig_data *live = trigger_list; live; live = live->next_in_world)
      if (GET_TRIG_RNUM(live) >= rnum)
        GET_TRIG_RNUM(live)++;

    /* Other builders' open editors hold one too. */
    for (struct descriptor_data *dsc = descriptor_list; dsc; dsc = dsc->next)
      if (dsc != d && STATE(dsc) == CON_TRIGEDIT && dsc->edit_trig)
        if (GET_TRIG_RNUM(dsc->edit_trig) >= rnum)
          GET_TRIG_RNUM(dsc->edit_trig)++;
  }

  proto = trig_index[rnum]->proto;

  if (!is_new) {
    /* Free what the old prototype owned. Live triggers share its command list,
     * so they are repointed below, in the same breath. */
    struct cmdlist_element *cmd, *next_cmd;
    for (cmd = proto->cmdlist; cmd; cmd = next_cmd) {
      next_cmd = cmd->next;
      DELETE_ARRAY_IF_EXTANT(cmd->cmd);
      delete cmd;
    }
    proto->cmdlist = NULL;
    DELETE_ARRAY_IF_EXTANT(proto->arglist);
    DELETE_ARRAY_IF_EXTANT(proto->name);
  }

  /* Hand the editor's command list to the prototype outright. */
  trig_data_copy(proto, trig);
  proto->cmdlist = trig->cmdlist;
  trig->cmdlist = NULL;
  proto->nr = rnum;

  trigedit_refresh_live(rnum, proto);

  /* SCRIPT_CHECK tests the owner's cached mask before looking at triggers. */
  for (struct char_data *ch = character_list; ch; ch = ch->next_in_character_list)
    refresh_script_types(SCRIPT(ch));
  for (nodeStruct<struct obj_data *> *node = ObjList.Head(); node; node = node->next)
    refresh_script_types(SCRIPT(node->data));
  for (rnum_t room = 0; room <= top_of_world; room++)
    refresh_script_types(SCRIPT(&world[room]));

  /* Write it out now rather than leaving it to the builder: losing a trigger
   * that things have already been attached to produces errors at reboot that
   * are miserable to track down. */
  write_trigs_to_disk(zone_table[d->edit_zone].number);
  send_to_char("Trigger saved to disk.\r\n", CH);
}

/* ************************************************************************
*  The command editor.                                                     *
************************************************************************ */

/* The builder is done with the string editor: turn what they typed back into
 * a command list. */
void trigedit_string_cleanup(struct descriptor_data *d, bool aborted)
{
  struct cmdlist_element *cmd, *next_cmd;
  char *line, *text;

  if (d->edit_mode != TRIGEDIT_COMMANDS)
    return;

  if (aborted) {
    DELETE_D_STR_IF_EXTANT(d);
    trigedit_disp_menu(d);
    return;
  }

  text = (d->str && *(d->str)) ? *(d->str) : NULL;

  if (!text || !*text) {
    DELETE_D_STR_IF_EXTANT(d);
    send_to_char("Nothing written, so the script stands as it was.\r\n", CH);
    trigedit_disp_menu(d);
    return;
  }

  /* Lay the script out before it is broken up, so what comes back from the
   * editor reads the way a script should. */
  if (format_script(d))
    text = *(d->str);

  /* Out with the old list. */
  for (cmd = d->edit_trig->cmdlist; cmd; cmd = next_cmd) {
    next_cmd = cmd->next;
    DELETE_ARRAY_IF_EXTANT(cmd->cmd);
    delete cmd;
  }
  d->edit_trig->cmdlist = NULL;

  cmd = NULL;

  for (line = text ? strtok(text, "\n\r") : NULL; line; line = strtok(NULL, "\n\r")) {
    struct cmdlist_element *cle = new cmdlist_element;
    cle->cmd = str_dup(line);

    if (cmd)
      cmd->next = cle;
    else
      d->edit_trig->cmdlist = cle;
    cmd = cle;
  }

  if (!d->edit_trig->cmdlist) {
    d->edit_trig->cmdlist = new cmdlist_element;
    d->edit_trig->cmdlist->cmd = str_dup("* This trigger has no commands yet.");
  }

  DELETE_D_STR_IF_EXTANT(d);
  d->edit_number2 = 1; /* changed */
  trigedit_disp_menu(d);
}

/* Re-indent a script so that the body of every if/while/switch sits one level
 * in from the line that opened it. */
int format_script(struct descriptor_data *d)
{
  char nsc[MAX_STRING_LENGTH], *t, line[MAX_INPUT_LENGTH];
  size_t len = 0, nlen = 0;
  int indent = 0, indent_next = FALSE, found_end = FALSE, line_num = 0;

  if (!d->str || !*(d->str))
    return FALSE;

  char *work = str_dup(*(d->str));

  t = strtok(work, "\r\n");
  if (!t) {
    DELETE_ARRAY_IF_EXTANT(work);
    return FALSE;
  }

  *nsc = '\0';

  while (t) {
    line_num++;
    skip_spaces(&t);

    if (!strncasecmp(t, "if ", 3) || !strncasecmp(t, "switch ", 7)) {
      indent_next = TRUE;
    } else if (!strncasecmp(t, "while ", 6)) {
      found_end = FALSE;
      indent_next = TRUE;
    } else if (!strncasecmp(t, "end", 3) || !strncasecmp(t, "done", 4)) {
      if (!indent) {
        send_to_char(CH, "Unmatched 'end' or 'done' on line %d.\r\n", line_num);
        DELETE_ARRAY_IF_EXTANT(work);
        return FALSE;
      }
      indent--;
      indent_next = FALSE;
    } else if (!strncasecmp(t, "else", 4)) {
      if (!indent) {
        send_to_char(CH, "Unmatched 'else' on line %d.\r\n", line_num);
        DELETE_ARRAY_IF_EXTANT(work);
        return FALSE;
      }
      indent--;
      indent_next = TRUE;
    } else if (!strncasecmp(t, "case", 4) || !strncasecmp(t, "default", 7)) {
      if (!indent) {
        send_to_char(CH, "Case or default outside a switch on line %d.\r\n", line_num);
        DELETE_ARRAY_IF_EXTANT(work);
        return FALSE;
      }
      if (found_end)
        indent--;
      found_end = TRUE;
      indent_next = TRUE;
    }

    nlen = snprintf(line, sizeof(line), "%*s%s\r\n", indent * 2, "", t);
    if ((len + nlen) > MAX_STRING_LENGTH - 100) {
      send_to_char("String too long, formatting aborted.\r\n", CH);
      DELETE_ARRAY_IF_EXTANT(work);
      return FALSE;
    }
    len += nlen;
    strlcat(nsc, line, sizeof(nsc));

    if (indent_next) {
      indent++;
      indent_next = FALSE;
    }

    t = strtok(NULL, "\r\n");
  }

  if (indent)
    send_to_char("Unmatched if, while or switch ignored.\r\n", CH);

  DELETE_ARRAY_IF_EXTANT(work);
  DELETE_ARRAY_IF_EXTANT(*(d->str));
  *(d->str) = str_dup(nsc);

  return TRUE;
}

/* ************************************************************************
*  The editor itself.                                                      *
************************************************************************ */

ACMD(do_trigedit)
{
  char arg1[MAX_INPUT_LENGTH];
  vnum_t number;
  rnum_t real_num, zone_idx;
  struct descriptor_data *d;

  if (!is_olc_available(ch))
    return;

  one_argument(argument, arg1);

  if (!*arg1 || !isdigit(*arg1)) {
    send_to_char("Usage: trigedit <vnum>\r\n", ch);
    return;
  }

  number = atol(arg1);

  if ((zone_idx = get_zone_index_number_from_vnum(number)) < 0) {
    send_to_char("Sorry, that number is not part of any zone.\r\n", ch);
    return;
  }

  REQUIRE_ZONE_EDIT_ACCESS(zone_idx);

  /* Nobody else may be in this trigger at the same time. */
  for (d = descriptor_list; d; d = d->next) {
    if (STATE(d) == CON_TRIGEDIT && d->edit_number == number) {
      send_to_char(ch, "That trigger is currently being edited by %s.\r\n",
                   d->character ? GET_CHAR_NAME(d->character) : "someone");
      return;
    }
  }

  d = ch->desc;
  d->edit_zone = zone_idx;
  d->edit_number = number;
  ch->player_specials->saved.zonenum = zone_table[zone_idx].number;

  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  STATE(d) = CON_TRIGEDIT;
  GET_WAS_IN(ch) = ch->in_room;
  char_from_room(ch);

  if ((real_num = real_trigger(number)) >= 0)
    trigedit_setup_existing(d, real_num);
  else
    trigedit_setup_new(d);

  trigedit_disp_menu(d);
}

/* Put the builder back in the world and throw the scratch trigger away. */
static void trigedit_finish(struct descriptor_data *d)
{
  struct cmdlist_element *cmd, *next_cmd;

  if (d->edit_trig) {
    for (cmd = d->edit_trig->cmdlist; cmd; cmd = next_cmd) {
      next_cmd = cmd->next;
      DELETE_ARRAY_IF_EXTANT(cmd->cmd);
      delete cmd;
    }
    d->edit_trig->cmdlist = NULL;
    free_trigger(d->edit_trig);
    d->edit_trig = NULL;
  }

  DELETE_D_STR_IF_EXTANT(d);

  char_to_room(CH, GET_WAS_IN(CH));
  GET_WAS_IN(CH) = NULL;
  PLR_FLAGS(CH).RemoveBit(PLR_EDITING);
  STATE(d) = CON_PLAYING;
  clear_editing_data(d);
}

void trigedit_parse(struct descriptor_data *d, const char *arg)
{
  int i;

  switch (d->edit_mode) {
    case TRIGEDIT_MAIN_MENU:
      switch (LOWER(*arg)) {
        case 'q':
          if (!GET_TRIG_TYPE(d->edit_trig)) {
            send_to_char("This trigger has no types set, so it would never fire. Set some first, "
                         "or use x to abort.\r\n", CH);
            trigedit_disp_menu(d);
            return;
          }
          trigedit_save(d);
          mudlog_vfprintf(CH, LOG_WIZLOG, "OLC: %s edits trigger %ld", GET_CHAR_NAME(CH), d->edit_number);
          trigedit_finish(d);
          send_to_char("Done.\r\n", CH);
          return;

        case 'x':
          if (d->edit_number2) {
            send_to_char("Trigger not saved, aborting.\r\n", CH);
          }
          trigedit_finish(d);
          return;

        case '1':
          send_to_char("Enter trigger name: ", CH);
          d->edit_mode = TRIGEDIT_NAME;
          return;

        case '2':
          send_to_char("0) Mobiles, 1) Objects, 2) Rooms\r\nEnter choice: ", CH);
          d->edit_mode = TRIGEDIT_INTENDED;
          return;

        case '3':
          trigedit_disp_types(d);
          return;

        case '4':
          send_to_char("Enter numeric argument (non-negative; percentages use 0-100): ", CH);
          d->edit_mode = TRIGEDIT_NARG;
          return;

        case '5':
          send_to_char("Enter argument: ", CH);
          d->edit_mode = TRIGEDIT_ARGUMENT;
          return;

        case '6':
          send_to_char("What you write replaces the script above, so bring anything\r\n"
                       "you want to keep with you. Finish without writing a line and\r\n"
                       "the script is left as it is.\r\n"
                       "\r\nEnter trigger commands:\r\n", CH);
          d->edit_mode = TRIGEDIT_COMMANDS;
          DELETE_D_STR_IF_EXTANT(d);
          INITIALIZE_NEW_D_STR(d);
          d->max_str = MAX_STRING_LENGTH - 100;
          d->mail_to = 0;
          return;

        case 'w':
          send_to_char("Copy which trigger (by vnum)? ", CH);
          d->edit_mode = TRIGEDIT_COPY;
          return;

        default:
          trigedit_disp_menu(d);
          return;
      }

    case TRIGEDIT_NAME:
      DELETE_ARRAY_IF_EXTANT(d->edit_trig->name);
      d->edit_trig->name = str_dup((arg && *arg) ? arg : "undefined");
      d->edit_number2 = 1;
      break;

    case TRIGEDIT_INTENDED:
      i = atoi(arg);
      if (i >= MOB_TRIGGER && i <= WLD_TRIGGER) {
        if (i != d->edit_trig->attach_type) {
          /* The type bits mean different things per attach type, so carrying
           * them across would silently turn a greet trigger into something
           * else. */
          d->edit_trig->attach_type = i;
          GET_TRIG_TYPE(d->edit_trig) = 0;
          send_to_char("Trigger types cleared, since they differ per attach type.\r\n", CH);
        }
        d->edit_number2 = 1;
      } else {
        send_to_char("Must be 0, 1 or 2.\r\n", CH);
      }
      break;

    case TRIGEDIT_NARG:
      d->edit_trig->narg = MAX(0, atoi(arg));
      d->edit_number2 = 1;
      break;

    case TRIGEDIT_ARGUMENT:
      DELETE_ARRAY_IF_EXTANT(d->edit_trig->arglist);
      d->edit_trig->arglist = (arg && *arg) ? str_dup(arg) : NULL;
      d->edit_number2 = 1;
      break;

    case TRIGEDIT_TYPES:
      if ((i = atoi(arg)) == 0)
        break;
      if (i > 0 && i <= trigedit_type_count(d->edit_trig->attach_type)) {
        GET_TRIG_TYPE(d->edit_trig) ^= (1 << (i - 1));
        d->edit_number2 = 1;
      }
      trigedit_disp_types(d);
      return;

    case TRIGEDIT_COPY: {
      rnum_t rnum;

      if (!arg || !*arg || !isdigit(*arg)) {
        send_to_char("Cancelled.\r\n", CH);
        break;
      }

      if ((rnum = real_trigger(atol(arg))) < 0) {
        send_to_char("That trigger does not exist.\r\n", CH);
        break;
      }

      /* Throw the current scratch trigger away and start from that one. */
      {
        struct cmdlist_element *cmd, *next_cmd;
        for (cmd = d->edit_trig->cmdlist; cmd; cmd = next_cmd) {
          next_cmd = cmd->next;
          DELETE_ARRAY_IF_EXTANT(cmd->cmd);
          delete cmd;
        }
        d->edit_trig->cmdlist = NULL;
        free_trigger(d->edit_trig);
        d->edit_trig = NULL;
      }

      trigedit_setup_existing(d, rnum);
      d->edit_number2 = 1;
      break;
    }

    case TRIGEDIT_COMMANDS:
      /* The string editor owns this state; trigedit_string_cleanup() brings
       * us back. */
      return;

    default:
      break;
  }

  trigedit_disp_menu(d);
}

/* ************************************************************************
*  The attached-trigger sub-menu, shared by redit, medit and iedit.        *
*                                                                          *
*  Awake's editors work on a scratch copy of the thing being edited, so     *
*  the list edited here rides along with that copy and reaches the          *
*  prototype when the editor saves.                                         *
************************************************************************ */

/* The list this editor is working on, whichever editor that is. */
static struct trig_proto_list **dg_script_list_for(struct descriptor_data *d, int *type)
{
  switch (STATE(d)) {
    case CON_REDIT:
      *type = WLD_TRIGGER;
      return d->edit_room ? &d->edit_room->proto_script : NULL;
    case CON_MEDIT:
      *type = MOB_TRIGGER;
      return d->edit_mob ? &d->edit_mob->proto_script : NULL;
    case CON_IEDIT:
      *type = OBJ_TRIGGER;
      return d->edit_obj ? &d->edit_obj->proto_script : NULL;
  }

  *type = MOB_TRIGGER;
  return NULL;
}

void dg_script_menu(struct descriptor_data *d)
{
  struct trig_proto_list **list;
  struct trig_proto_list *proto;
  int type, i = 0;

  if (!(list = dg_script_list_for(d, &type))) {
    send_to_char("Nothing to attach triggers to.\r\n", CH);
    d->edit_mode = DG_SCRIPT_DONE;
    return;
  }

  CLS(CH);
  send_to_char("Attached triggers:\r\n", CH);

  for (proto = *list; proto; proto = proto->next) {
    rnum_t rnum = real_trigger(proto->vnum);

    send_to_char(CH, "%2d) [^c%ld^n] %s\r\n", ++i, (long) proto->vnum,
                 rnum >= 0 ? GET_TRIG_NAME(trig_index[rnum]->proto) : "^r(does not exist)^n");
  }

  if (!i)
    send_to_char("  None.\r\n", CH);

  send_to_char("\r\na) Attach a trigger\r\n"
               "d) Detach a trigger\r\n"
               "q) Back to the previous menu\r\n"
               "Enter your choice: ", CH);

  d->edit_mode = DG_SCRIPT_MAIN_MENU;
}

/* Returns TRUE while the sub-menu still owns the input, FALSE once the
 * calling editor should take over and redisplay its own menu. */
bool dg_script_edit_parse(struct descriptor_data *d, const char *arg)
{
  struct trig_proto_list **list, *proto, *prev;
  int type, i;

  if (!(list = dg_script_list_for(d, &type)))
    return FALSE;

  switch (d->edit_mode) {
    case DG_SCRIPT_MAIN_MENU:
      switch (LOWER(*arg)) {
        case 'a':
          send_to_char("Attach which trigger (by vnum)? ", CH);
          d->edit_mode = DG_SCRIPT_ATTACH;
          return TRUE;
        case 'd':
          send_to_char("Detach which one (by the number beside it)? ", CH);
          d->edit_mode = DG_SCRIPT_DETACH;
          return TRUE;
        case 'q':
          return FALSE;
        default:
          dg_script_menu(d);
          return TRUE;
      }

    case DG_SCRIPT_ATTACH: {
      vnum_t vnum = atol(arg);
      rnum_t rnum = real_trigger(vnum);

      if (rnum < 0) {
        send_to_char("That trigger does not exist.\r\n", CH);
        dg_script_menu(d);
        return TRUE;
      }

      if (trig_index[rnum]->proto->attach_type != type) {
        send_to_char(CH, "That trigger is meant for %s, not for this.\r\n",
                     trigedit_attach_name(trig_index[rnum]->proto->attach_type));
        dg_script_menu(d);
        return TRUE;
      }

      {
        struct trig_proto_list *new_trg = new trig_proto_list;
        new_trg->vnum = vnum;

        if (!(proto = *list)) {
          *list = new_trg;
        } else {
          while (proto->next)
            proto = proto->next;
          proto->next = new_trg;
        }
      }

      dg_script_menu(d);
      return TRUE;
    }

    case DG_SCRIPT_DETACH:
      i = atoi(arg);

      if (i < 1) {
        dg_script_menu(d);
        return TRUE;
      }

      for (prev = NULL, proto = *list; proto && --i > 0; prev = proto, proto = proto->next)
        ;

      if (!proto) {
        send_to_char("There is no trigger with that number attached.\r\n", CH);
        dg_script_menu(d);
        return TRUE;
      }

      if (prev)
        prev->next = proto->next;
      else
        *list = proto->next;

      delete proto;

      dg_script_menu(d);
      return TRUE;
  }

  return FALSE;
}
