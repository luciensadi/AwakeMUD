/**************************************************************************
*  File: dg_db_scripts.cpp                                                *
*  Usage: Reading triggers off disk, attaching them to prototypes, and    *
*         writing a zone's triggers back out.                             *
*                                                                         *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
*                                                                         *
*  $Author: Mark A. Heilpern/egreen/Welcor $                              *
*  $Date: 2004/10/11 12:07:00$                                            *
*  $Revision: 1.0.14 $                                                    *
*                                                                         *
*  Ported to AwakeMUD CE by Fizban. tbaMUD's .trg files are Diku-format;  *
*  these are VTable-format, like everything else under lib/world.         *
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "db.hpp"
#include "file.hpp"
#include "vtable.hpp"
#include "handler.hpp"
#include "interpreter.hpp"
#include "constants.hpp"
#include "olc.hpp"
#include "dg_scripts.hpp"
#include "dg_event.hpp"

extern void write_index_file(const char *suffix);

/* local functions */
static void trig_data_init(struct trig_data *this_data);

/* Turn one saved command list into a cmdlist_element chain.
 *
 * File::GetLine() drops any line whose first character is '*', which is
 * exactly what a DG comment looks like, so the writer indents comments by one
 * space on the way out; script_driver() skips leading whitespace before it
 * looks for the '*', so an indented comment is still a comment. */
static struct cmdlist_element *build_cmdlist(char *commands, vnum_t vnum)
{
  struct cmdlist_element *head = NULL, *cle = NULL;
  char *s;

  /* Split by hand rather than with strtok(), which runs delimiters together
   * and so would quietly drop every blank line a builder had used to space
   * their script out. */
  for (s = (commands && *commands) ? commands : NULL; s; ) {
    char *eol = s;

    while (*eol && *eol != '\n' && *eol != '\r')
      eol++;

    bool last = (*eol == '\0');
    char terminator = *eol;
    *eol = '\0';

    if (!head) {
      head = cle = new cmdlist_element;
    } else {
      cle->next = new cmdlist_element;
      cle = cle->next;
    }
    cle->cmd = str_dup(s);

    if (last)
      break;

    /* Treat CRLF as one line ending, not two. */
    s = eol + 1;
    if (terminator == '\r' && *s == '\n')
      s++;

    /* A trailing newline ends the script; it does not add a blank line. */
    if (!*s)
      break;
  }

  if (!head) {
    /* A trigger with no commands would leave script_driver with nothing to
     * walk, so give it a single no-op line. */
    log_vfprintf("Warning: trigger %ld has an empty command list.", (long) vnum);
    head = new cmdlist_element;
    head->cmd = str_dup("* This trigger has no commands.");
  }

  return head;
}

void parse_trigger(File &fl, long nr)
{
  struct trig_index_data *t_index;
  struct trig_data *trig;
  VTable data;
  char *commands;

  data.Parse(&fl);

  trig = new trig_data;
  t_index = new trig_index_data;

  t_index->vnum = nr;
  t_index->number = 0;
  t_index->proto = trig;

  trig->nr = top_of_trigt;
  trig->name = str_dup(data.GetString("Name", "unnamed trigger"));
  trig->attach_type = (byte) data.GetInt("AttachType", MOB_TRIGGER);
  trig->trigger_type = data.GetLong("Types", 0);
  trig->narg = data.GetInt("NArg", 0);
  trig->arglist = str_dup(data.GetString("Arg", NULL));

  commands = str_dup(data.GetString("Commands", "* This trigger has no commands."));
  trig->cmdlist = build_cmdlist(commands, nr);
  DELETE_ARRAY_IF_EXTANT(commands);

  trig_index[top_of_trigt++] = t_index;
}

/* Create a live trigger from a prototype. nr is the trigger's real number. */
struct trig_data *read_trigger(rnum_t nr)
{
  struct trig_index_data *t_index;
  struct trig_data *trig;

  if (nr < 0 || nr >= top_of_trigt)
    return NULL;

  if ((t_index = trig_index[nr]) == NULL)
    return NULL;

  trig = new trig_data;
  trig_data_copy(trig, t_index->proto);

  t_index->number++;

  return trig;
}

static void trig_data_init(struct trig_data *this_data)
{
  this_data->nr = NOTHING;
  this_data->data_type = 0;
  this_data->name = NULL;
  this_data->trigger_type = 0;
  this_data->cmdlist = NULL;
  this_data->curr_state = NULL;
  this_data->narg = 0;
  this_data->arglist = NULL;
  this_data->depth = 0;
  this_data->loops = 0;
  this_data->wait_event = NULL;
  this_data->purged = FALSE;
  this_data->var_list = NULL;
  this_data->next = NULL;
  this_data->next_in_world = NULL;
}

/* A live trigger shares its prototype's command list; only the mutable state
 * is copied. */
void trig_data_copy(struct trig_data *this_data, const struct trig_data *trg)
{
  trig_data_init(this_data);

  this_data->nr = trg->nr;
  this_data->attach_type = trg->attach_type;
  this_data->data_type = trg->data_type;

  if (trg->name) {
    this_data->name = str_dup(trg->name);
  } else {
    this_data->name = str_dup("unnamed trigger");
    log_vfprintf("Trigger with no name! (%ld)", (long) trg->nr);
  }

  this_data->trigger_type = trg->trigger_type;
  this_data->cmdlist = trg->cmdlist;
  this_data->narg = trg->narg;

  if (trg->arglist)
    this_data->arglist = str_dup(trg->arglist);
}

/* Read a mob/obj/room prototype's list of attached trigger vnums. The saved
 * form is a single space-separated list, e.g. "Scripts:\t1000 1002". */
void dg_read_trigger_list(const char *line, void *proto, int type)
{
  struct trig_proto_list **head = NULL, *trg_proto, *new_trg;
  char buf[MAX_STRING_LENGTH];
  char *token;

  if (!line || !*line)
    return;

  switch (type) {
    case MOB_TRIGGER: head = &((struct char_data *) proto)->proto_script; break;
    case OBJ_TRIGGER: head = &((struct obj_data *) proto)->proto_script;  break;
    case WLD_TRIGGER: head = &((struct room_data *) proto)->proto_script; break;
    default:
      mudlog("SYSERR: dg_read_trigger_list() called with an unknown type.", NULL, LOG_SYSLOG, TRUE);
      return;
  }

  strlcpy(buf, line, sizeof(buf));

  for (token = strtok(buf, " \t,"); token; token = strtok(NULL, " \t,")) {
    vnum_t vnum = atol(token);

    if (vnum <= 0)
      continue;

    new_trg = new trig_proto_list;
    new_trg->vnum = vnum;

    if (!(trg_proto = *head)) {
      *head = new_trg;
    } else {
      while (trg_proto->next)
        trg_proto = trg_proto->next;
      trg_proto->next = new_trg;
    }
  }
}

/* Render a prototype's attached trigger list back into the saved form, or
 * NULL if it has none. The buffer is static; use it before calling again. */
const char *dg_render_proto_list(void *proto, int type)
{
  static char buf[MAX_STRING_LENGTH];
  struct trig_proto_list *trg_proto = NULL;
  size_t len = 0;

  switch (type) {
    case MOB_TRIGGER: trg_proto = ((struct char_data *) proto)->proto_script; break;
    case OBJ_TRIGGER: trg_proto = ((struct obj_data *) proto)->proto_script;  break;
    case WLD_TRIGGER: trg_proto = ((struct room_data *) proto)->proto_script; break;
  }

  if (!trg_proto)
    return NULL;

  *buf = '\0';
  for (; trg_proto; trg_proto = trg_proto->next)
    len += snprintf(buf + len, sizeof(buf) - len, "%s%ld", len ? " " : "", (long) trg_proto->vnum);

  return buf;
}

/* Give a freshly-loaded mob/obj/room the triggers its prototype lists. */
void assign_triggers(void *i, int type)
{
  struct trig_proto_list *trg_proto = NULL;
  struct script_data **script = NULL;
  rnum_t rnum;
  const char *what = "?";
  long what_vnum = -1;

  switch (type) {
    case MOB_TRIGGER: {
      struct char_data *mob = (struct char_data *) i;
      trg_proto = mob->proto_script;
      script = &SCRIPT(mob);
      what = "mob";
      what_vnum = GET_MOB_VNUM(mob);
      break;
    }
    case OBJ_TRIGGER: {
      struct obj_data *obj = (struct obj_data *) i;
      trg_proto = obj->proto_script;
      script = &SCRIPT(obj);
      what = "object";
      what_vnum = GET_OBJ_VNUM(obj);
      break;
    }
    case WLD_TRIGGER: {
      struct room_data *room = (struct room_data *) i;
      trg_proto = room->proto_script;
      script = &SCRIPT(room);
      what = "room";
      what_vnum = room->number;
      break;
    }
    default:
      mudlog("SYSERR: unknown type for assign_triggers()", NULL, LOG_SYSLOG, TRUE);
      return;
  }

  for (; trg_proto; trg_proto = trg_proto->next) {
    rnum = real_trigger(trg_proto->vnum);

    if (rnum < 0) {
      mudlog_vfprintf(NULL, LOG_ZONELOG, "SYSERR: trigger #%ld does not exist, but is assigned to %s #%ld",
                      (long) trg_proto->vnum, what, what_vnum);
      continue;
    }

    if (!*script)
      *script = new script_data;

    add_trigger(*script, read_trigger(rnum), -1);
  }
}

/* ************************************************************************
*  Saving.                                                                 *
************************************************************************ */

/* Write one command line, keeping it readable back. A leading '*' would be
 * eaten by File::GetLine() as a file comment, and a wholly blank line would be
 * skipped, so both get a leading space. A '~' would end the string early, so
 * it is written as the bell that File::ReadString() turns back into a tilde,
 * the same trick the rest of the world files use. */
static void write_trig_command_line(FILE *fp, const char *cmd)
{
  if (!cmd || !*cmd) {
    fprintf(fp, " \n");
    return;
  }

  if (*cmd == '*')
    fputc(' ', fp);

  for (const char *p = cmd; *p; p++)
    fputc(*p == '~' ? '\7' : *p, fp);

  fputc('\n', fp);
}

void write_trigs_to_disk(vnum_t zone_vnum)
{
  FILE *fp;
  rnum_t znum = real_zone(zone_vnum);
  bool wrote_something = FALSE;

  if (znum < 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: write_trigs_to_disk() called for nonexistent zone %ld.",
                    (long) zone_vnum);
    return;
  }

  char final_file_name[1000];
  snprintf(final_file_name, sizeof(final_file_name), "%s/%d.trg", TRG_PREFIX, zone_table[znum].number);

  /* Write to a temporary file so a failed write can't clobber the originals. */
  char tmp_file_name[1000];
  snprintf(tmp_file_name, sizeof(tmp_file_name), "%s.tmp", final_file_name);

  if (!(fp = fopen(tmp_file_name, "w+"))) {
    perror("Error opening file in write_trigs_to_disk()");
    return;
  }

  for (rnum_t rnum = 0; rnum < top_of_trigt; rnum++) {
    struct trig_index_data *t_index = trig_index[rnum];
    struct trig_data *trig;

    if (!t_index || !(trig = t_index->proto))
      continue;

    if (t_index->vnum < zone_table[znum].number * 100 || t_index->vnum > zone_table[znum].top)
      continue;

    wrote_something = TRUE;

    fprintf(fp, "#%ld\n", (long) t_index->vnum);
    fprintf(fp, "Name:\t%s\n", trig->name ? trig->name : "unnamed trigger");
    fprintf(fp, "AttachType:\t%d\n", (int) trig->attach_type);
    fprintf(fp, "Types:\t%ld\n", trig->trigger_type);
    fprintf(fp, "NArg:\t%d\n", trig->narg);
    fprintf(fp, "Arg:\t%s\n", trig->arglist ? trig->arglist : "");

    fprintf(fp, "Commands:$\n");
    for (struct cmdlist_element *cle = trig->cmdlist; cle; cle = cle->next)
      write_trig_command_line(fp, cle->cmd);
    fprintf(fp, "~\n");

    fprintf(fp, "BREAK\n");
  }

  fprintf(fp, "END\n");
  fclose(fp);

  if (wrote_something) {
    if (rename(tmp_file_name, final_file_name) != 0)
      perror("Error renaming temporary file in write_trigs_to_disk()");
  } else {
    /* The zone has no triggers left: drop both the file and the temporary. */
    remove(tmp_file_name);
    remove(final_file_name);
  }

  write_index_file("trg");
}
