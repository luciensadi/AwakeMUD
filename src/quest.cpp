/* *************************************************************************
*    file: quest.cc                                                        *
*    author: Andrew Hynek                                                  *
*    purpose: contains all autoquest functions and the online quest editor *
*    Copyright (c) 1997, 1998 by Andrew Hynek                              *
*    Copyright (c) 2001 The AwakeMUD Consortium                            *
************************************************************************* */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "quest.h"
#include "dblist.h"
#include "olc.h"
#include "screen.h"
#include "boards.h"
#include "constants.h"
#include "newmatrix.h"

extern bool memory(struct char_data *ch, struct char_data *vict);
extern class objList ObjList;
extern struct time_info_data time_info;
extern int olc_state;
extern bool perform_give(struct char_data *ch, struct char_data *vict,
                           struct obj_data *obj);
extern void add_follower(struct char_data *ch, struct char_data *leader);
extern void free_quest(struct quest_data *quest);
extern bool resize_qst_array(void);
extern char *cleanup(char *, const char *);

ACMD(do_say);
ACMD(do_action);
SPECIAL(johnson);

#define QUEST          d->edit_quest

#define NUM_OBJ_LOADS            7
#define NUM_MOB_LOADS            3
#define NUM_OBJ_OBJECTIVES       8
#define NUM_MOB_OBJECTIVES       5

const char *obj_loads[] =
  {
    "Do not load",
    "Give item to Johnson (who gives it to quester at start)",
    "Give item to a target",
    "Equip item on a target",
    "Install item in a target",
    "Load at a location",
    "Load at host",
    "\n"
  };

const char *mob_loads[] =
  {
    "Do not load",
    "Load at a location",
    "Load at/follow quester",
    "\n"
  };

const char *obj_objectives[] =
  {
    "No objective",
    "Return item to Johnson",
    "Deliver item to a target",
    "Deliver item to location",
    "Destroy an item",
    "Destroy as many items as possible",
    "Return Paydata to johnson",
    "Upload to a host",
    "\n"
  };

const char *mob_objectives[] =
  {
    "No objective",
    "Escort target to a location",
    "Kill target",
    "Kill as many targets as possible",
    "Target hunts a different quest target",
    "\n"
  };


const char *sol[] =
  {
    "DNL",
    "Johnson",
    "Give target",
    "Equip target",
    "Install target",
    "Location",
    "\n"
  };

const char *sml[] =
  {
    "DNL",
    "Location",
    "Follow quester",
    "\n"
  };

const char *soo[] =
  {
    "none",
    "return",
    "to target",
    "to location",
    "destroy one",
    "destroy many",
    "\n"
  };

const char *smo[] =
  {
    "none",
    "escort",
    "kill one",
    "kill many",
    "hunt",
    "\n"
  };

void end_quest(struct char_data *ch);

void load_quest_targets(struct char_data *johnson, struct char_data *ch)
{
  struct obj_data *obj;
  struct char_data *mob;
  int i, j, room = -1, pos, num = GET_QUEST(ch), rnum = -1;

  for (i = 0; i < quest_table[num].num_mobs; i++)
  {
    if (quest_table[num].mob[i].load == QML_LOCATION &&
        (rnum = real_mobile(quest_table[num].mob[i].vnum)) > -1 &&
        (room = real_room(quest_table[num].mob[i].l_data)) > -1) {
      mob = read_mobile(rnum, REAL);
      mob->mob_specials.quest_id = GET_IDNUM(ch);
      char_to_room(mob, room);
      if(quest_table[num].mob[i].objective == QMO_LOCATION)
        add_follower(mob, ch);
      for (j = 0; j < quest_table[num].num_objs; j++)
        if (quest_table[num].obj[j].l_data == i &&
            (rnum = real_object(quest_table[num].obj[j].vnum)) > -1) {
          switch (quest_table[num].obj[j].load) {
          case QOL_TARMOB_I:
            obj = read_object(rnum, REAL);
            obj->obj_flags.quest_id = GET_IDNUM(ch);
            obj_to_char(obj, mob);
            break;
          case QOL_TARMOB_E:
            pos = quest_table[num].obj[j].l_data2;
            if (pos >= 0 && pos < NUM_WEARS && (!GET_EQ(mob, pos) ||
                                                (pos == WEAR_WIELD && !GET_EQ(mob, WEAR_HOLD)))) {
              obj = read_object(rnum, REAL);
              obj->obj_flags.quest_id = GET_IDNUM(ch);
              equip_char(mob, obj, pos);
            }
            break;
          case QOL_TARMOB_C:
            obj = read_object(rnum, REAL);
            obj->obj_flags.quest_id = GET_IDNUM(ch);
            if (GET_OBJ_TYPE(obj) == ITEM_CYBERWARE &&
                GET_ESS(mob) > GET_OBJ_VAL(obj, 1)) {
              obj_to_cyberware(obj, mob);
            } else if (GET_OBJ_TYPE(obj) == ITEM_BIOWARE &&
                       GET_INDEX(mob) > GET_OBJ_VAL(obj, 1)) {
              obj_to_bioware(obj, mob);
            } else
              extract_obj(obj);
            break;
          }
          obj = NULL;
        }
      mob = NULL;
    } else if (quest_table[num].mob[i].load == QML_FOLQUESTER &&
               (rnum = real_mobile(quest_table[num].mob[i].vnum)) > -1) {
      mob = read_mobile(rnum, REAL);
      mob->mob_specials.quest_id = GET_IDNUM(ch);
      char_to_room(mob, ch->in_room);
      for (j = 0; j < quest_table[num].num_objs; j++)
        if (quest_table[num].obj[j].l_data == i &&
            (rnum = real_object(quest_table[num].obj[j].vnum)) > -1) {
          switch (quest_table[num].obj[j].load) {
          case QOL_TARMOB_I:
            obj = read_object(rnum, REAL);
            obj->obj_flags.quest_id = GET_IDNUM(ch);
            obj_to_char(obj, mob);
            break;
          case QOL_TARMOB_E:
            pos = quest_table[num].obj[j].l_data2;
            if (pos >= 0 && pos < NUM_WEARS && (!GET_EQ(mob, pos) ||
                                                (pos == WEAR_WIELD && !GET_EQ(mob, WEAR_HOLD)))) {
              obj = read_object(rnum, REAL);
              obj->obj_flags.quest_id = GET_IDNUM(ch);
              equip_char(mob, obj, pos);
            }
            break;
          case QOL_TARMOB_C:
            obj = read_object(rnum, REAL);
            obj->obj_flags.quest_id = GET_IDNUM(ch);
            if (GET_OBJ_TYPE(obj) == ITEM_CYBERWARE &&
                GET_ESS(mob) > GET_OBJ_VAL(obj, 1)) {
              obj_to_cyberware(obj, mob);
            } else if (GET_OBJ_TYPE(obj) == ITEM_BIOWARE &&
                       GET_INDEX(mob) > GET_OBJ_VAL(obj, 1)) {
              obj_to_bioware(obj, mob);
            } else
              extract_obj(obj);
            break;
          }
          obj = NULL;
        }
      add_follower(mob, ch);
      mob = NULL;
    }
  }

  for (i = 0; i < quest_table[num].num_objs; i++)
    if ((rnum = real_object(quest_table[num].obj[i].vnum)) > -1)
      switch (quest_table[num].obj[i].load)
      {
      case QOL_LOCATION:
        if ((room = real_room(quest_table[num].obj[i].l_data)) > -1) {
          obj = read_object(rnum, REAL);
          obj->obj_flags.quest_id = GET_IDNUM(ch);
          obj_to_room(obj, room);
        }
        obj = NULL;
        break;
      case QOL_HOST:
        if ((room = real_host(quest_table[num].obj[i].l_data)) > -1) {
          obj = read_object(rnum, REAL);
          obj->obj_flags.quest_id = GET_IDNUM(ch);
          GET_OBJ_VAL(obj, 7) = GET_IDNUM(ch);
          GET_OBJ_VAL(obj, 9) = 1;
          obj->next_content = matrix[room].file;
          matrix[room].file = obj;
        }
        obj = NULL;
        break;
      case QOL_JOHNSON:
        obj = read_object(rnum, REAL);
        obj->obj_flags.quest_id = GET_IDNUM(ch);
        obj_to_char(obj, johnson);
        if (!perform_give(johnson, ch, obj)) {
          char buf[256];
          sprintf(buf, "You cannot hold %s.", obj->text.name);
          do_say(johnson, buf, 0, 0);
          extract_obj(obj);
        }
        obj = NULL;
        break;
      }
}

void extract_quest_targets(int num)
{
  struct char_data *mob, *next;
  struct obj_data *obj, *next_obj;
  int i;

  for (mob = character_list; mob; mob = next) {
    next = mob->next;
    if (mob->mob_specials.quest_id == num) {
      for (obj = mob->carrying; obj; obj = next_obj) {
        next_obj = obj->next_content;
        extract_obj(obj);
      }
      for (i = 0; i < NUM_WEARS; i++)
        if (GET_EQ(mob, i))
          extract_obj(GET_EQ(mob, i));
      extract_char(mob);
    }
  }

  ObjList.RemoveQuestObjs(num);
}

bool is_escortee(struct char_data *mob)
{
  int i;

  if (!IS_NPC(mob) || !mob->master || IS_NPC(mob->master) ||
      !GET_QUEST(mob->master))
    return FALSE;

  for (i = 0; i < quest_table[GET_QUEST(mob->master)].num_mobs; i++)
    if (quest_table[GET_QUEST(mob->master)].mob[i].vnum == GET_MOB_VNUM(mob))
      if (quest_table[GET_QUEST(mob->master)].mob[i].objective == QMO_LOCATION)
        return TRUE;

  return FALSE;
}

bool hunting_escortee(struct char_data *ch, struct char_data *vict)
{
  int i, num;

  if (!IS_NPC(ch) || !is_escortee(vict))
    return FALSE;

  num = GET_QUEST(vict->master);

  for (i = 0; i < quest_table[num].num_mobs; i++)
    if (quest_table[num].mob[i].vnum == GET_MOB_VNUM(ch) &&
        quest_table[num].mob[i].objective == QMO_KILL_ESCORTEE &&
        quest_table[num].mob[quest_table[num].mob[i].o_data].vnum == GET_MOB_VNUM(vict))
      return TRUE;

  return FALSE;
}

bool check_quest_delivery(struct char_data *ch, struct char_data *mob, struct obj_data *obj)
{
  if (!ch || IS_NPC(ch) || !IS_NPC(mob) || !GET_QUEST(ch))
    return FALSE;
  int vnum = GET_OBJ_VNUM(obj);
  int i;

  for (i = 0; i < quest_table[GET_QUEST(ch)].num_objs; i++)
    if (quest_table[GET_QUEST(ch)].obj[i].vnum == vnum)
      switch (quest_table[GET_QUEST(ch)].obj[i].objective)
      {
      case QOO_JOHNSON:
        if (GET_MOB_SPEC(mob) && GET_MOB_SPEC(mob) == johnson && memory(mob, ch)) {
          ch->player_specials->obj_complete[i] = 1;
          return TRUE;
        }
        break;
      case QOO_TAR_MOB:
        if (quest_table[GET_QUEST(ch)].obj[i].o_data == GET_MOB_VNUM(mob)) {
          ch->player_specials->obj_complete[i] = 1;
          return TRUE;
        }
        break;
      case QOO_RETURN_PAY:
        if (GET_MOB_SPEC(mob) && GET_MOB_SPEC(mob) == johnson && memory(mob, ch)) {
          if (GET_OBJ_VAL(obj, 3) == quest_table[GET_QUEST(ch)].obj[i].o_data)
            ch->player_specials->obj_complete[i] = 1;
          return TRUE;
        }
      }
  return FALSE;
}

void check_quest_delivery(struct char_data *ch, struct obj_data *obj)
{
  if (!ch || IS_NPC(ch) || !GET_QUEST(ch))
    return;

  int i;

  for (i = 0; i < quest_table[GET_QUEST(ch)].num_objs; i++)
    if (quest_table[GET_QUEST(ch)].obj[i].objective == QOO_LOCATION &&
        GET_OBJ_VNUM(obj) == quest_table[GET_QUEST(ch)].obj[i].vnum &&
        world[ch->in_room].number == quest_table[GET_QUEST(ch)].obj[i].o_data)
    {
      ch->player_specials->obj_complete[i] = 1;
      return;
    }
  if (ch->persona && quest_table[GET_QUEST(ch)].obj[i].objective == QOO_UPLOAD &&
      GET_OBJ_VNUM(obj) == quest_table[GET_QUEST(ch)].obj[i].vnum &&
      matrix[ch->persona->in_host].vnum == quest_table[GET_QUEST(ch)].obj[i].o_data)
  {
    ch->player_specials->obj_complete[i] = 1;
    return;
  }
}

void check_quest_destination(struct char_data *ch, struct char_data *mob)
{
  if (!ch || IS_NPC(ch) || !IS_NPC(mob) || !GET_QUEST(ch))
    return;

  int i;

  for (i = 0; i < quest_table[GET_QUEST(ch)].num_mobs; i++)
    if (quest_table[GET_QUEST(ch)].mob[i].objective == QMO_LOCATION &&
        world[mob->in_room].number == quest_table[GET_QUEST(ch)].mob[i].o_data)
    {
      ch->player_specials->mob_complete[i] = 1;
      stop_follower(mob);
      do_say(mob, "Thanks for the escort.", 0, 0);
      return;
    }
}

void check_quest_destroy(struct char_data *ch, struct obj_data *obj)
{
  if (!ch || IS_NPC(ch) || !GET_QUEST(ch))
    return;

  int i;

  for (i = 0; i < quest_table[GET_QUEST(ch)].num_objs; i++)
    if (GET_OBJ_VNUM(obj) == quest_table[GET_QUEST(ch)].obj[i].vnum)
      switch (quest_table[GET_QUEST(ch)].obj[i].objective)
      {
      case QOO_DSTRY_ONE:
      case QOO_DSTRY_MANY:
        ch->player_specials->obj_complete[i]++;
        return;
      }
}

void check_quest_kill(struct char_data *ch, struct char_data *victim)
{
  if (!ch || IS_NPC(ch) || !GET_QUEST(ch) || !IS_NPC(victim))
    return;

  int i;

  for (i = 0; i < quest_table[GET_QUEST(ch)].num_mobs; i++)
    if (GET_MOB_VNUM(victim) == quest_table[GET_QUEST(ch)].mob[i].vnum)
      switch (quest_table[GET_QUEST(ch)].mob[i].objective)
      {
      case QMO_KILL_ONE:
      case QMO_KILL_MANY:
        ch->player_specials->mob_complete[i]++;
        return;
      }
}

void end_quest(struct char_data *ch)
{
  if (IS_NPC(ch) || !GET_QUEST(ch))
    return;

  extract_quest_targets(GET_IDNUM(ch));

  GET_QUEST(ch) = 0;

  delete [] ch->player_specials->mob_complete;
  delete [] ch->player_specials->obj_complete;
  ch->player_specials->mob_complete = NULL;
  ch->player_specials->obj_complete = NULL;
}

bool rep_too_high(struct char_data *ch, int num)
{
  if (num < 0 || num >= top_of_questt)
    return TRUE;

  if (GET_REP(ch) > quest_table[num].max_rep)
    return TRUE;

  return FALSE;
}

bool rep_too_low(struct char_data *ch, int num)
{
  if (num < 0 || num >= top_of_questt)
    return TRUE;

  if (GET_REP(ch) < quest_table[num].min_rep)
    return TRUE;

  return FALSE;
}

void reward(struct char_data *ch, struct char_data *johnson)
{
  if (from_ip_zone(quest_table[GET_QUEST(ch)].vnum))
    return;

  struct follow_type *f;
  struct obj_data *obj;
  int i, nuyen = 0, karma = 0, num, all = 1, old;

  for (i = 0; i < quest_table[GET_QUEST(ch)].num_objs; i++)
    if (ch->player_specials->obj_complete[i])
    {
      if (quest_table[GET_QUEST(ch)].obj[i].objective == QOO_DSTRY_MANY) {
        nuyen += quest_table[GET_QUEST(ch)].obj[i].nuyen *
                 ch->player_specials->obj_complete[i];
        karma += quest_table[GET_QUEST(ch)].obj[i].karma *
                 ch->player_specials->obj_complete[i];
      } else {
        nuyen += quest_table[GET_QUEST(ch)].obj[i].nuyen;
        karma += quest_table[GET_QUEST(ch)].obj[i].karma;
      }
    } else
      all = 0;
  for (i = 0; i < quest_table[GET_QUEST(ch)].num_mobs; i++)
    if (ch->player_specials->mob_complete[i])
    {
      if (quest_table[GET_QUEST(ch)].mob[i].objective == QMO_KILL_MANY) {
        nuyen += quest_table[GET_QUEST(ch)].mob[i].nuyen *
                 ch->player_specials->mob_complete[i];
        karma += quest_table[GET_QUEST(ch)].mob[i].karma *
                 ch->player_specials->mob_complete[i];
      } else {
        nuyen += quest_table[GET_QUEST(ch)].mob[i].nuyen;
        karma += quest_table[GET_QUEST(ch)].mob[i].karma;
      }
    } else
      all = 0;
  if (all)
  {
    nuyen += quest_table[GET_QUEST(ch)].nuyen;
    karma += quest_table[GET_QUEST(ch)].karma;
    if ((num = real_object(quest_table[GET_QUEST(ch)].reward)) > 0) {
      obj = read_object(num, REAL);
      obj_to_char(obj, ch);
      act("You give $p to $N.", FALSE, johnson, obj, ch, TO_CHAR);
      act("$n gives you $p.", FALSE, johnson, obj, ch, TO_VICT);
      act("$n gives $p to $N.", TRUE, johnson, obj, ch, TO_NOTVICT);
    }
  }
  nuyen = negotiate(ch, johnson, 0, nuyen, 0, FALSE);

  if (AFF_FLAGGED(ch, AFF_GROUP))
  {
    num = 1;
    for (f = ch->followers; f; f = f->next)
      if (AFF_FLAGGED(f->follower, AFF_GROUP) && !(rep_too_low(f->follower, GET_QUEST(ch)) || rep_too_high(f->follower, GET_QUEST(ch))))
        num++;

    nuyen = (int)(nuyen / num);
    karma = (int)(karma / num);

    for (f = ch->followers; f; f = f->next)
      if (AFF_FLAGGED(f->follower, AFF_GROUP) && !(rep_too_low(f->follower, GET_QUEST(ch)) || rep_too_high(f->follower, GET_QUEST(ch)))) {
        old = (int)(GET_KARMA(f->follower) / 100);
        GET_NUYEN(f->follower) += nuyen;
        GET_KARMA(f->follower) += karma;
        GET_REP(f->follower) += (int)(GET_KARMA(f->follower) / 100) - old;
        GET_TKE(f->follower) += (int)(GET_KARMA(f->follower) / 100) - old;
      }
  }
  old = (int)(GET_KARMA(ch) / 100);
  GET_NUYEN(ch) += nuyen;
  GET_KARMA(ch) += karma;
  GET_REP(ch) += (int)(GET_KARMA(ch) / 100) - old;
  GET_TKE(ch) += (int)(GET_KARMA(ch) / 100) - old;
  act("$n gives some nuyen to $N.", TRUE, johnson, 0, ch, TO_NOTVICT);
  act("You give some nuyen to $N.", TRUE, johnson, 0, ch, TO_CHAR);
  sprintf(buf, "$n gives you %d nuyen.", nuyen);
  act(buf, FALSE, johnson, 0, ch, TO_VICT);
  sprintf(buf, "You gain %.2f karma.\r\n", ((float) karma / 100));
  send_to_char(buf, ch);
  end_quest(ch);
}

void new_quest(struct char_data *mob)
{
  int i, num = 0;

  for (i = 0; i < top_of_questt; i++)
    if (quest_table[i].johnson == GET_MOB_VNUM(mob))
      num++;

  if (num < 1)
  {
    if (mob_index[mob->nr].func == johnson)
      mob_index[mob->nr].func = mob_index[mob->nr].sfunc;
    mob_index[mob->nr].sfunc = NULL;
    return;
  }

  for (i = 0;;)
  {
    if (quest_table[i].johnson == GET_MOB_VNUM(mob) &&
        !number(0, num - 1) && !(num > 1 && GET_SPARE2(mob) == i)) {
      GET_SPARE2(mob) = i;
      return;
    }
    if ((i + 1) < top_of_questt)
      i++;
    else
      i = 0;
  }
}

void handle_info(struct char_data *johnson)
{
  int allowed, pos, num, i, j;
  char speech[210];

  allowed = 210 - strlen(GET_NAME(johnson));
  pos = GET_SPARE1(johnson);
  num = GET_SPARE2(johnson);
  i = strlen(quest_table[num].info);

  if (allowed < 10)
    allowed += 79;

  if ((pos + allowed) < i)
  {
    for (i = pos + allowed; i > pos; i--)
      if ((isspace(*(quest_table[num].info + i)) || *(quest_table[num].info + i) == '\r') &&
          isprint(*(quest_table[num].info + i - 1)) &&
          !isspace(*(quest_table[num].info + i - 1)))
        break;
    GET_SPARE1(johnson) = i + 1;
  } else
  {
    if (!number(0, 9))
      new_quest(johnson);
    GET_SPARE1(johnson) = -1;
  }

  for (j = 0; pos < i; pos++)
  {
    if (*(quest_table[num].info + pos) == '\n')
      continue;
    if (*(quest_table[num].info + pos) == '\r')
      speech[j] = ' ';
    else
      speech[j] = *(quest_table[num].info + pos);
    j++;
  }

  speech[j] = '\0';

  do_say(johnson, speech, 0, 0);
}

SPECIAL(johnson)
{
  struct char_data *johnson = (struct char_data *) me, *temp = NULL;
  int i, obj_complete = 0, mob_complete = 0, num, comm = CMD_NONE;

  if (!IS_NPC(johnson))
    return FALSE;
  if (!cmd) {
    if (!GET_SPARE2(johnson)) {
      new_quest(johnson);
      GET_SPARE1(johnson) = -1;
    }
    if (GET_SPARE1(johnson) >= 0) {
      for (temp = world[johnson->in_room].people; temp; temp = temp->next_in_room)
        if (memory(johnson, temp))
          break;
      if (!temp) {
        new_quest(johnson);
        GET_SPARE1(johnson) = -1;
      } else if (GET_QUEST(temp))
        handle_info(johnson);
    } else if (time_info.minute > 0 && time_info.minute <= 5)
      new_quest(johnson);
    return FALSE;
  }

  if (!CAN_SEE(johnson, ch) || IS_NPC(ch) || IS_SENATOR(ch) || PRF_FLAGGED(ch, PRF_QUEST) ||
      (AFF_FLAGGED(ch, AFF_GROUP) && ch->master))
    return FALSE;

  skip_spaces(&argument);

  if (CMD_IS("say") || CMD_IS("'")) {
    if (str_str(argument, "quit"))
      comm = CMD_QUIT;
    else if (str_str(argument, "collect") || str_str(argument, "complete") || str_str(argument, "done"))
      comm = CMD_DONE;
    else if (str_str(argument, "work") || str_str(argument, "business") ||
             str_str(argument, "run") || str_str(argument, "shadowrun") ||
             str_str(argument, "job"))
      comm = CMD_START;
    else if (str_str(argument, "yes") || str_str(argument, "accept") || str_str(argument, "yeah")
            || str_str(argument, "sure") || str_str(argument, "okay"))
      comm = CMD_YES;
    else if (strstr(argument, "no"))
      comm = CMD_NO;
    do_say(ch, argument, 0, 0);
  } else if (CMD_IS("nod")) {
    comm = CMD_YES;
    do_action(ch, argument, cmd, 0);
  } else if (CMD_IS("shake") && !*argument) {
    comm = CMD_NO;
    do_action(ch, argument, cmd, 0);
  } else
    return FALSE;

  if (comm == CMD_QUIT && GET_SPARE1(johnson) == -1 && GET_QUEST(ch) &&
      memory(johnson, ch)) {
    do_say(johnson, quest_table[GET_QUEST(ch)].quit, 0, 0);
    end_quest(ch);
    forget(johnson, ch);
  } else if (comm == CMD_DONE && GET_SPARE1(johnson) == -1 && GET_QUEST(ch) &&
             memory(johnson, ch)) {
    for (i = 0; i < quest_table[GET_QUEST(ch)].num_objs; i++)
      if (ch->player_specials->obj_complete[i]) {
        obj_complete = 1;
        break;
      }
    for (i = 0; i < quest_table[GET_QUEST(ch)].num_mobs; i++)
      if (ch->player_specials->mob_complete[i]) {
        mob_complete = 1;
        break;
      }
    if (obj_complete || mob_complete) {
      for (int i = QUEST_TIMER - 1; i > 0; i--)
        GET_LQUEST(ch, i) = GET_LQUEST(ch, i - 1);
      GET_LQUEST(ch, 0) = quest_table[GET_QUEST(ch)].vnum;
      reward(ch, johnson);
      do_say(johnson, quest_table[GET_QUEST(ch)].finish, 0, 0);
      forget(johnson, ch);
    } else
      do_say(johnson, "But you have not completed any objectives yet.", 0, 0);
  } else if (comm == CMD_START && GET_SPARE1(johnson) == -1 &&
             GET_SPARE2(johnson) >= 0) {
    if (GET_QUEST(ch)) {
      do_say(johnson, "Maybe when you've finished what you're doing.", 0, 0);
      return TRUE;
    }
    for (int i = QUEST_TIMER - 1; i >= 0; i--)
      if (GET_LQUEST(ch, i) == quest_table[GET_SPARE2(johnson)].vnum) {
        do_say(johnson, quest_table[GET_SPARE2(johnson)].done, 0, 0);
        if (memory(johnson, ch))
          forget(johnson, ch);
        return TRUE;
      }
    if (PLR_FLAGGED(ch, PLR_KILLER) || PLR_FLAGGED(ch, PLR_BLACKLIST)) {
      do_say(johnson, "Word on the street is you can't be trusted.", 0, 0);
      GET_SPARE1(johnson) = -1;
      if (memory(johnson, ch))
        forget(johnson, ch);      
    } else if (rep_too_high(ch, GET_SPARE2(johnson))) {
      do_say(johnson, "I've got nothing you'd be interested in right now.", 0, 0);
      GET_SPARE1(johnson) = -1;
      if (memory(johnson, ch))
        forget(johnson, ch);
    } else if (rep_too_low(ch, GET_SPARE2(johnson))) {
      do_say(johnson, "And just who are you?", 0, 0);
      GET_SPARE1(johnson) = -1;
      if (memory(johnson, ch))
        forget(johnson, ch);
    } else {
      GET_SPARE1(johnson) = 0;
      do_say(johnson, quest_table[GET_SPARE2(johnson)].intro, 0, 0);
      do_say(johnson, "Are you interested?", 0, 0);
      if (!memory(johnson, ch))
        remember(johnson, ch);
    }
  } else if (comm == CMD_YES && !GET_SPARE1(johnson) && !GET_QUEST(ch) &&
             memory(johnson, ch)) {
    GET_QUEST(ch) = GET_SPARE2(johnson);
    ch->player_specials->obj_complete =
      new sh_int[quest_table[GET_QUEST(ch)].num_objs];
    ch->player_specials->mob_complete =
      new sh_int[quest_table[GET_QUEST(ch)].num_mobs];
    for (num = 0; num < quest_table[GET_QUEST(ch)].num_objs; num++)
      ch->player_specials->obj_complete[num] = 0;
    for (num = 0; num < quest_table[GET_QUEST(ch)].num_mobs; num++)
      ch->player_specials->mob_complete[num] = 0;
    load_quest_targets(johnson, ch);
    handle_info(johnson);
  } else if (comm == CMD_NO && !GET_SPARE1(johnson) && !GET_QUEST(ch) &&
             memory(johnson, ch)) {
    GET_SPARE1(johnson) = -1;
    GET_QUEST(ch) = 0;
    forget(johnson, ch);
    do_say(johnson, quest_table[GET_SPARE2(johnson)].decline, 0, 0);
  }

  return TRUE;
}


/*
 * Almost a copy of the 'is_open()' for shops but modified
 * a good deal to work for Johnsons
 */
void johnson_update(void)
{
  struct char_data *johnson = NULL, *tmp = NULL;
  char buf[200];
  int i, rstart = 0, rend = 0;

  *buf = 0;

  for ( i = 0; i < top_of_questt;  i++ ) {
    /* Random times */
    if ( quest_table[i].s_time == -1 ) {
      if ( dice(1, 6) )
        rstart++;

      if ( dice(1, 6) ) {
        rstart--;
        rend++;
      }
    }

    /* Needs to come to 'work' */
    if ( rstart || quest_table[i].s_time > time_info.hours ) {
      if ( quest_table[i].s_string != NULL )
        strcpy( buf, quest_table[i].s_string );
      johnson = read_mobile( quest_table[i].johnson, REAL );
      MOB_FLAGS(johnson).SetBit(MOB_ISNPC);
      char_to_room( johnson, quest_table[i].s_room );
    }
    /* Needs to head off */
    else if ( rend || quest_table[i].e_time < time_info.hours ) {
      if ( quest_table[i].e_string != NULL )
        strcpy( buf, quest_table[i].e_string );
      for ( johnson = character_list; johnson != NULL; johnson = johnson->next ) {
        if ( johnson->nr == (tmp = read_mobile( quest_table[i].johnson, REAL))->nr )
          break;
      }
      if ( johnson != NULL && johnson->in_room != NOWHERE ) {
        MOB_FLAGS(johnson).SetBit(MOB_ISNPC);
        char_from_room( johnson );
        char_to_room( johnson, 0 );
        extract_char(johnson);
      }
      if ( tmp != NULL && tmp->in_room != NOWHERE ) {
        MOB_FLAGS(tmp).SetBit(MOB_ISNPC);
        extract_char( tmp );
      }
    }
  }

  if ( *buf != '\0' ) {
    act( buf, TRUE, johnson, 0, 0, TO_ROOM );
  }

  return;
}


void assign_johnsons(void)
{
  int i, rnum;

  for (i = 0; i < top_of_questt; i++) {
    if ((rnum = real_mobile(quest_table[i].johnson)) < 0)
      log("Johnson #%d does not exist (quest #%d)",
          quest_table[i].johnson, quest_table[i].vnum);
    else if (mob_index[rnum].func != johnson) {
      mob_index[rnum].sfunc = mob_index[rnum].func;
      mob_index[rnum].func = johnson;
    }
  }
}

void list_detailed_quest(struct char_data *ch, long rnum)
{
  int i;

  {
    int johnson = real_mobile(quest_table[rnum].johnson);

    sprintf(buf, "Vnum: [%5ld], Rnum: [%ld], Johnson: [%s (%ld)]\r\n",
            quest_table[rnum].vnum, rnum,
            johnson < 0 ? "none" : GET_NAME(mob_proto+johnson),
            quest_table[rnum].johnson);
  }

  if (!access_level(ch, LVL_ADMIN))
  {
    send_to_char(buf, ch);
    return;
  }

  sprintf(buf + strlen(buf), "Time allowed: [%d], Minimum reputation: [%d], "
          "Maximum reputation: [%d]\r\n", quest_table[rnum].time,
          quest_table[rnum].min_rep, quest_table[rnum].max_rep);

  sprintf(buf + strlen(buf), "Bonus nuyen: [%d], Bonus Karma: [%0.2f], "
          "Reward: [%d]\r\n", quest_table[rnum].nuyen,
          ((float)quest_table[rnum].karma / 100), quest_table[rnum].reward);

  for (i = 0; i < quest_table[rnum].num_mobs; i++)
    sprintf(buf + strlen(buf), "M%2d) %d\xC2\xA5/%0.2f: V%ld; %s (%d); %s (%d)\r\n",
            i, quest_table[rnum].mob[i].nuyen,
            ((float)quest_table[rnum].mob[i].karma / 100),
            quest_table[rnum].mob[i].vnum, sml[(int)quest_table[rnum].mob[i].load],
            quest_table[rnum].mob[i].l_data,
            smo[(int)quest_table[rnum].mob[i].objective],
            quest_table[rnum].mob[i].o_data);


  for (i = 0; i < quest_table[rnum].num_objs; i++)
    sprintf(buf + strlen(buf), "O%2d) %d\xC2\xA5/%0.2f: V%ld; %s (%d/%d); %s (%d)\r\n",
            i, quest_table[rnum].obj[i].nuyen,
            ((float)quest_table[rnum].obj[i].karma / 100),
            quest_table[rnum].obj[i].vnum, sol[(int)quest_table[rnum].obj[i].load],
            quest_table[rnum].obj[i].l_data, quest_table[rnum].obj[i].l_data2,
            soo[(int)quest_table[rnum].obj[i].objective],
            quest_table[rnum].obj[i].o_data);

  page_string(ch->desc, buf, 1);
}

void boot_one_quest(struct quest_data *quest)
{
  int count, quest_nr = -1, i;

  if ((top_of_questt + 1) >= top_of_quest_array)
    // if it cannot resize, return...the edit_quest is freed later
    if (!resize_qst_array())
    {
      olc_state = 0;
      return;
    }

  for (count = 0; count < top_of_questt; count++)
    if (quest_table[count].vnum > quest->vnum)
    {
      quest_nr = count;
      break;
    }

  if (quest_nr == -1)
    quest_nr = top_of_questt;
  else
    for (count = top_of_questt; count > quest_nr; count--)
    {
      // copy quest_table[count-1] to quest_table[count]
      quest_table[count] = quest_table[count-1];
    }

  top_of_questt++;

  // and now we copy quest to quest_table[quest_nr]
  quest_table[quest_nr].vnum = quest->vnum;
  quest_table[quest_nr].johnson = quest->johnson;
  quest_table[quest_nr].time = quest->time;
  quest_table[quest_nr].min_rep = quest->min_rep;
  quest_table[quest_nr].max_rep = quest->max_rep;
  quest_table[quest_nr].nuyen = quest->nuyen;
  quest_table[quest_nr].karma = quest->karma;
  quest_table[quest_nr].reward = quest->reward;

  quest_table[quest_nr].num_objs = quest->num_objs;
  if (quest_table[quest_nr].num_objs > 0)
  {
    quest_table[quest_nr].obj = new struct quest_om_data[quest_table[quest_nr].num_objs];
    for (i = 0; i < quest_table[quest_nr].num_objs; i++)
      quest_table[quest_nr].obj[i] = quest->obj[i];
  } else
    quest_table[quest_nr].obj = NULL;

  quest_table[quest_nr].num_mobs = quest->num_mobs;
  if (quest_table[quest_nr].num_mobs > 0)
  {
    quest_table[quest_nr].mob = new struct quest_om_data[quest_table[quest_nr].num_mobs];
    for (i = 0; i < quest_table[quest_nr].num_mobs; i++)
      quest_table[quest_nr].mob[i] = quest->mob[i];
  } else
    quest_table[quest_nr].mob = NULL;

  quest_table[quest_nr].intro = str_dup(quest->intro);
  quest_table[quest_nr].decline = str_dup(quest->decline);
  quest_table[quest_nr].quit = str_dup(quest->quit);
  quest_table[quest_nr].finish = str_dup(quest->finish);
  quest_table[quest_nr].info = str_dup(quest->info);
  quest_table[quest_nr].done = str_dup(quest->done);

  if ((i = real_mobile(quest_table[quest_nr].johnson)) > 0 &&
      mob_index[i].func != johnson)
  {
    mob_index[i].sfunc = mob_index[i].func;
    mob_index[i].func = johnson;
  }
}

void reboot_quest(int rnum, struct quest_data *quest)
{
  int i, ojn, njn;

  if (quest_table[rnum].johnson != quest->johnson)
  {
    ojn = real_mobile(quest_table[rnum].johnson);
    njn = real_mobile(quest->johnson);
    if (mob_index[ojn].func == johnson) {
      mob_index[ojn].func = mob_index[ojn].sfunc;
      mob_index[ojn].sfunc = NULL;
    } else if (mob_index[ojn].sfunc == johnson)
      mob_index[ojn].sfunc = NULL;
    mob_index[njn].sfunc = mob_index[njn].func;
    mob_index[njn].func = johnson;
    quest_table[rnum].johnson = quest->johnson;
  }

  quest_table[rnum].time = quest->time;
  quest_table[rnum].min_rep = quest->min_rep;
  quest_table[rnum].max_rep = quest->max_rep;
  quest_table[rnum].num_objs = quest->num_objs;
  quest_table[rnum].num_mobs = quest->num_mobs;
  quest_table[rnum].nuyen = quest->nuyen;
  quest_table[rnum].karma = quest->karma;
  quest_table[rnum].reward = quest->reward;

  if (quest_table[rnum].obj)
    delete [] quest_table[rnum].obj;

  if (quest_table[rnum].num_objs > 0)
  {
    quest_table[rnum].obj = new quest_om_data[quest_table[rnum].num_objs];

    for (i = 0; i < quest_table[rnum].num_objs; i++)
      quest_table[rnum].obj[i] = quest->obj[i];
  } else
    quest_table[rnum].obj = NULL;

  if (quest_table[rnum].mob)
    delete [] quest_table[rnum].mob;

  if (quest_table[rnum].num_mobs > 0)
  {
    quest_table[rnum].mob = new quest_om_data[quest_table[rnum].num_mobs];

    for (i = 0; i < quest_table[rnum].num_mobs; i++)
      quest_table[rnum].mob[i] = quest->mob[i];
  } else
    quest_table[rnum].mob = NULL;

  if (quest_table[rnum].intro)
    delete [] quest_table[rnum].intro;
  quest_table[rnum].intro = str_dup(quest->intro);

  if (quest_table[rnum].decline)
    delete [] quest_table[rnum].decline;
  quest_table[rnum].decline = str_dup(quest->decline);

  if (quest_table[rnum].quit)
    delete [] quest_table[rnum].quit;
  quest_table[rnum].quit = str_dup(quest->quit);

  if (quest_table[rnum].finish)
    delete [] quest_table[rnum].finish;
  quest_table[rnum].finish = str_dup(quest->finish);

  if (quest_table[rnum].info)
    delete [] quest_table[rnum].info;
  quest_table[rnum].info = str_dup(quest->info);

  if (quest_table[rnum].done)
    delete [] quest_table[rnum].done;
  quest_table[rnum].done = str_dup(quest->done);
}

int write_quests_to_disk(int zone)
{
  long i, j, found = 0, counter;
  FILE *fp;
  zone = real_zone(zone);

  sprintf(buf, "world/qst/%d.qst", zone_table[zone].number);

  if (!(fp = fopen(buf, "w+"))) {
    log("SYSERR: could not open file %d.qst", zone_table[zone].number);

    fclose(fp);
    return 0;
  }

  for (counter = zone_table[zone].number * 100;
       counter <= zone_table[zone].top; counter++) {
    if ((i = real_quest(counter)) > -1) {
      fprintf(fp, "#%ld\n", quest_table[i].vnum);
      fprintf(fp, "%ld %d %d %d %d %d %d %d %d %d %d %d\n", quest_table[i].johnson,
              quest_table[i].time, quest_table[i].min_rep,
              quest_table[i].max_rep, quest_table[i].nuyen,
              quest_table[i].karma, quest_table[i].reward,
              quest_table[i].num_objs, quest_table[i].num_mobs,
              quest_table[i].s_time, quest_table[i].e_time,
              quest_table[i].s_room);

      for (j = 0; j < quest_table[i].num_objs; j++)
        fprintf(fp, "%ld %d %d %d %d %d %d %d\n", quest_table[i].obj[j].vnum,
                quest_table[i].obj[j].nuyen, quest_table[i].obj[j].karma,
                quest_table[i].obj[j].load, quest_table[i].obj[j].objective,
                quest_table[i].obj[j].l_data, quest_table[i].obj[j].l_data2,
                quest_table[i].obj[j].o_data);

      for (j = 0; j < quest_table[i].num_mobs; j++)
        fprintf(fp, "%ld %d %d %d %d %d %d %d\n", quest_table[i].mob[j].vnum,
                quest_table[i].mob[j].nuyen, quest_table[i].mob[j].karma,
                quest_table[i].mob[j].load, quest_table[i].mob[j].objective,
                quest_table[i].mob[j].l_data, quest_table[i].mob[j].l_data2,
                quest_table[i].mob[j].o_data);

      fprintf(fp, "%s~\n", cleanup(buf2, quest_table[i].intro));
      fprintf(fp, "%s~\n", cleanup(buf2, quest_table[i].decline));
      fprintf(fp, "%s~\n", cleanup(buf2, quest_table[i].quit));
      fprintf(fp, "%s~\n", cleanup(buf2, quest_table[i].finish));
      fprintf(fp, "%s~\n", cleanup(buf2, quest_table[i].info));
      fprintf(fp, "%s~\n", cleanup(buf2, quest_table[i].s_string));
      fprintf(fp, "%s~\n", cleanup(buf2, quest_table[i].e_string));
      fprintf(fp, "%s~\n", cleanup(buf2, quest_table[i].done));
    }
  }
  fprintf(fp, "$~\n");
  fclose(fp);

  fp = fopen("world/qst/index", "w+");

  for (i = 0; i <= top_of_zone_table; ++i) {
    found = 0;
    for (j = 0; !found && j < top_of_questt; j++)
      if (quest_table[j].vnum >= (zone_table[i].number * 100) &&
          quest_table[j].vnum <= zone_table[i].top) {
        found = 1;
        fprintf(fp, "%d.qst\n", zone_table[i].number);
      }
  }

  fprintf(fp, "$~\n");
  fclose(fp);
  return 1;
}

void qedit_list_obj_objectives(struct descriptor_data *d)
{
  int i;
  long rnum;

  CLS(CH);

  *buf = '\0';

  for (i = 0; i < QUEST->num_objs; i++)
  {
    sprintf(buf + strlen(buf), "%2d) ", i);
    switch (QUEST->obj[i].load) {
    case QUEST_NONE:
      strcat(buf, "Not set");
      break;
    case QOL_JOHNSON:
      sprintf(buf + strlen(buf), "Give %ld to Johnson", QUEST->obj[i].vnum);
      break;
    case QOL_TARMOB_I:
      sprintf(buf + strlen(buf), "Give %ld to M%d ", QUEST->obj[i].vnum,
              QUEST->obj[i].l_data);
      if (QUEST->obj[i].l_data >= 0 &&
          QUEST->obj[i].l_data < QUEST->num_mobs &&
          (rnum = real_mobile(QUEST->mob[QUEST->obj[i].l_data].vnum)) > -1)
        sprintf(buf + strlen(buf), "(%s)",
                GET_NAME(mob_proto+rnum));
      else
        strcat(buf, "(null)");
      break;

    case QOL_TARMOB_E:
      sprintf(buf + strlen(buf), "Equip M%d ", QUEST->obj[i].l_data);

      if (QUEST->obj[i].l_data >= 0 &&
          QUEST->obj[i].l_data < QUEST->num_mobs &&
          (rnum = real_mobile(QUEST->mob[QUEST->obj[i].l_data].vnum)) > -1)
        sprintf(buf + strlen(buf), "(%s) ",
                GET_NAME(mob_proto+rnum));

      strcat(buf, "(null) ");
      sprintf(buf + strlen(buf), "with %ld at %s", QUEST->obj[i].vnum,
              wear_bits[QUEST->obj[i].l_data2]);
      break;

    case QOL_TARMOB_C:
      sprintf(buf + strlen(buf), "Install %ld in M%d ", QUEST->obj[i].vnum,
              QUEST->obj[i].l_data);

      if (QUEST->obj[i].l_data >= 0 &&
          QUEST->obj[i].l_data < QUEST->num_mobs &&
          (rnum = real_mobile(QUEST->mob[QUEST->obj[i].l_data].vnum)) > -1)
        sprintf(buf + strlen(buf), "(%s)", GET_NAME(mob_proto+rnum));
      else
        strcat(buf, "(null)");

      break;
    case QOL_HOST:
      sprintf(buf + strlen(buf), "Load %ld in host %d", QUEST->obj[i].vnum, QUEST->obj[i].l_data);
      break;
    case QOL_LOCATION:
      sprintf(buf + strlen(buf), "Load %ld in room %d", QUEST->obj[i].vnum,
              QUEST->obj[i].l_data);
      break;
    }
    sprintf(buf + strlen(buf), "\r\n    Award %d \xC2\xA5 & %0.2f karma for ",
            QUEST->obj[i].nuyen, ((float)QUEST->obj[i].karma / 100));
    switch (QUEST->obj[i].objective) {
    case QUEST_NONE:
      strcat(buf, "nothing\r\n");
      break;
    case QOO_JOHNSON:
      strcat(buf, "returning item to Johnson\r\n");
      break;
    case QOO_TAR_MOB:
      sprintf(buf + strlen(buf), "delivering item to M%d ",
              QUEST->obj[i].o_data);
      if (QUEST->obj[i].o_data >= 0 &&
          QUEST->obj[i].o_data < QUEST->num_mobs &&
          (rnum = real_mobile(QUEST->mob[QUEST->obj[i].o_data].vnum)) > -1)
        sprintf(buf + strlen(buf), "(%s)\r\n", GET_NAME(mob_proto+rnum));
      else
        strcat(buf, "(null)\r\n");
      break;
    case QOO_LOCATION:
      sprintf(buf + strlen(buf), "delivering item to room %d\r\n",
              QUEST->obj[i].o_data);
      break;
    case QOO_DSTRY_ONE:
      strcat(buf, "destroying item\r\n");
      break;
    case QOO_DSTRY_MANY:
      strcat(buf, "each item destroyed\r\n");
      break;
    case QOO_UPLOAD:
      sprintf(buf + strlen(buf), "uploading to host %d\n\n", QUEST->obj[i].o_data);
      break;
    case QOO_RETURN_PAY:
      sprintf(buf + strlen(buf), "returning paydata from host %d\r\n",
              QUEST->obj[i].o_data);
    }
  }
  send_to_char(buf, CH);
}

void qedit_list_mob_objectives(struct descriptor_data *d)
{
  int i, rnum = 0;

  CLS(CH);

  *buf = '\0';

  for (i = 0; i < QUEST->num_mobs; i++)
  {
    sprintf(buf + strlen(buf), "%2d) ", i);
    switch (QUEST->mob[i].load) {
    case QUEST_NONE:
      strcat(buf, "Not set");
      break;
    case QML_LOCATION:
      sprintf(buf + strlen(buf), "Load %ld (%s) at room %d",
              QUEST->mob[i].vnum,
              (rnum = real_mobile(QUEST->mob[i].vnum)) > -1 ?
              GET_NAME(mob_proto+rnum) : "null",
              QUEST->mob[i].l_data);
      break;
    case QML_FOLQUESTER:
      sprintf(buf + strlen(buf), "Load %ld (%s) at and follow quester",
              QUEST->mob[i].vnum,
              (rnum = real_mobile(QUEST->mob[i].vnum)) > -1 ?
              GET_NAME(mob_proto+rnum) : "null");
      break;
    }
    switch (QUEST->mob[i].objective) {
    case QUEST_NONE:
      sprintf(buf + strlen(buf), "\r\n    Award %d \xC2\xA5 & %0.2f karma for "
              "nothing\r\n", QUEST->mob[i].nuyen,
              ((float)QUEST->mob[i].karma / 100));
      break;
    case QMO_LOCATION:
      sprintf(buf + strlen(buf), "\r\n    Award %d \xC2\xA5 & %0.2f karma for "
              "escorting target to room %d\r\n", QUEST->mob[i].nuyen,
              ((float)QUEST->mob[i].karma / 100), QUEST->mob[i].o_data);
      break;
    case QMO_KILL_ONE:
      sprintf(buf + strlen(buf), "\r\n    Award %d \xC2\xA5 & %0.2f karma for "
              "killing target\r\n", QUEST->mob[i].nuyen,
              ((float)QUEST->mob[i].karma / 100));
      break;
    case QMO_KILL_MANY:
      sprintf(buf + strlen(buf), "\r\n    Award %d \xC2\xA5 & %0.2f karma for "
              "each target killed\r\n", QUEST->mob[i].nuyen,
              ((float)QUEST->mob[i].karma / 100));
      break;
    case QMO_KILL_ESCORTEE:
      sprintf(buf + strlen(buf), "Target hunts M%d \r\n",
              QUEST->mob[i].o_data);
      if (QUEST->mob[i].o_data >= 0 &&
          QUEST->mob[i].o_data < QUEST->num_mobs &&
          (rnum = real_mobile(QUEST->mob[QUEST->mob[i].o_data].vnum)) > -1)
        sprintf(buf + strlen(buf), "(%s)\r\n", GET_NAME(mob_proto+rnum));
      else
        strcat(buf, "(null)\r\n");
      break;
    }
  }
  send_to_char(buf, CH);
}

void qedit_disp_obj_menu(struct descriptor_data *d)
{
  send_to_char(CH, "Item objective menu:\r\n"
               " 1) List current objectives\r\n"
               " 2) Edit an existing objective\r\n"
               " 3) Add a new objective (%d slots remaining)\r\n"
               " q) Return to main menu\r\n"
               "Enter your choice: ", QMAX_OBJS - QUEST->num_objs);

  d->edit_number2 = 0;
  d->edit_mode = QEDIT_O_MENU;
}

void qedit_disp_mob_menu(struct descriptor_data *d)
{
  send_to_char(CH, "Mobile objective menu:\r\n"
               " 1) List current objectives\r\n"
               " 2) Edit an existing objective\r\n"
               " 3) Add a new objective (%d slots remaining)\r\n"
               " q) Return to main menu\r\n"
               "Enter your choice: ", QMAX_MOBS - QUEST->num_mobs);

  d->edit_number2 = 0;
  d->edit_mode = QEDIT_M_MENU;
}

void qedit_disp_obj_loads(struct descriptor_data *d)
{
  int i;

  CLS(CH);

  for (i = 0; i < NUM_OBJ_LOADS; i++)
    send_to_char(CH, "%2d) %s\r\n", i, obj_loads[i]);

  send_to_char(CH, "Enter item load location: ");

  d->edit_mode = QEDIT_O_LOAD;
}

void qedit_disp_mob_loads(struct descriptor_data *d)
{
  int i;

  CLS(CH);

  for (i = 0; i < NUM_MOB_LOADS; i++)
    send_to_char(CH, "%2d) %s\r\n", i, mob_loads[i]);

  send_to_char(CH, "Enter mobile load location: ");

  d->edit_mode = QEDIT_M_LOAD;
}

void qedit_disp_obj_objectives(struct descriptor_data *d)
{
  int i;

  CLS(CH);

  for (i = 0; i < NUM_OBJ_OBJECTIVES; i++)
    send_to_char(CH, "%2d) %s\r\n", i, obj_objectives[i]);

  send_to_char(CH, "Enter item objective: ");

  d->edit_mode = QEDIT_O_OBJECTIVE;
}

void qedit_disp_mob_objectives(struct descriptor_data *d)
{
  int i;

  CLS(CH);

  for (i = 0; i < NUM_MOB_OBJECTIVES; i++)
    send_to_char(CH, "%2d) %s\r\n", i, mob_objectives[i]);

  send_to_char(CH, "Enter mob objective/task: ");

  d->edit_mode = QEDIT_M_OBJECTIVE;
}

void qedit_disp_locations(struct descriptor_data *d)
{
  int i;

  CLS(CH);

  for (i = 0; i < (NUM_WEARS - 1); i += 2)
    send_to_char(CH, "%2d) %-20s    %2d) %-20s\r\n", i + 1, short_where[i],
                 i + 2, i + 1 < (NUM_WEARS - 1) ? short_where[i + 1] : "");

  send_to_char(CH, "Enter wear location: ");
  d->edit_mode = QEDIT_O_LDATA2;
}

void qedit_disp_menu(struct descriptor_data *d)
{
  char s_time[10], e_time[10];

  CLS(CH);
  send_to_char(CH, "Quest number: %s%d%s\r\n", CCCYN(CH, C_CMP),
               d->edit_number, CCNRM(CH, C_CMP));
  send_to_char(CH, "1) Johnson: %s%d%s (%s%s%s)\r\n", CCCYN(CH, C_CMP),
               QUEST->johnson, CCNRM(CH, C_CMP), CCCYN(CH, C_CMP),
               real_mobile(QUEST->johnson) < 0 ? "null" :
               GET_NAME(mob_proto+real_mobile(QUEST->johnson)),
               CCNRM(CH, C_CMP));
  send_to_char(CH, "2) Time allowed (minutes): %s%d%s\r\n", CCCYN(CH, C_CMP),
               QUEST->time, CCNRM(CH, C_CMP));
  send_to_char(CH, "3) Reputation range: %s%d%s-%s%d%s\r\n", CCCYN(CH, C_CMP),
               QUEST->min_rep, CCNRM(CH, C_CMP), CCCYN(CH, C_CMP),
               QUEST->max_rep, CCNRM(CH, C_CMP));
  send_to_char(CH, "4) Bonus nuyen: %s%d%s\r\n", CCCYN(CH, C_CMP),
               QUEST->nuyen, CCNRM(CH, C_CMP));
  send_to_char(CH, "5) Bonus karma: %s%0.2f%s\r\n", CCCYN(CH, C_CMP),
               ((float)QUEST->karma / 100), CCNRM(CH, C_CMP));
  send_to_char(CH, "6) Item objective menu\r\n");
  send_to_char(CH, "7) Mobile objective menu\r\n");
  send_to_char(CH, "8) Introductory text:  %s%s%s\r\n", CCCYN(CH, C_CMP),
               QUEST->intro, CCNRM(CH, C_CMP));
  send_to_char(CH, "9) Decline text:       %s%s%s\r\n", CCCYN(CH, C_CMP),
               QUEST->decline, CCNRM(CH, C_CMP));
  send_to_char(CH, "a) Quit text:          %s%s%s\r\n", CCCYN(CH, C_CMP),
               QUEST->quit, CCNRM(CH, C_CMP));
  send_to_char(CH, "b) Completed text:     %s%s%s\r\n", CCCYN(CH, C_CMP),
               QUEST->finish, CCNRM(CH, C_CMP));
  send_to_char(CH, "c) Informational text:\r\n%s%s%s\r\n\r\n", CCCYN(CH, C_CMP),
               QUEST->info, CCNRM(CH, C_CMP));

  /*
   * Determine what to print for the times the Johnson is out
   */
  if ( QUEST->s_time == -1 )
  {
    strcpy(s_time,"Random");
    e_time[0] = '\0';
  } else if ( QUEST->s_time == QUEST->e_time )
  {
    strcpy(s_time,"Always");
    e_time[0] = '\0';
  } else
  {
    sprintf(s_time,"%d - ", QUEST->s_time);
    sprintf(e_time,"%d", QUEST->e_time);
  }
  send_to_char(CH, "d) Johnson hours: %s%s%s%s\r\n", CCCYN(CH, C_CMP),
               s_time, e_time, CCNRM(CH, C_CMP));

  send_to_char(CH, "e) Start work message: %s%s%s\r\n", CCCYN(CH, C_CMP),
               QUEST->s_string, CCNRM(CH, C_CMP));
  send_to_char(CH, "f) End work message: %s%s%s\r\n", CCCYN(CH, C_CMP),
               QUEST->e_string, CCNRM(CH, C_CMP));
  send_to_char(CH, "g) Quest already completed message: %s%s%s\r\n", CCCYN(CH, C_CMP),
               QUEST->done, CCNRM(CH, C_CMP));

  if (access_level(CH, LVL_VICEPRES))
    send_to_char(CH, "h) Reward: %s%d%s (%s%s%s)\r\n", CCCYN(CH, C_CMP),
                 QUEST->reward, CCNRM(CH, C_CMP), CCCYN(CH, C_CMP),
                 real_object(QUEST->reward) < 0 ? "null" :
                 obj_proto[real_object(QUEST->reward)].text.name,
                 CCNRM(CH, C_CMP));
  send_to_char("q) Quit and save\r\n", CH);
  send_to_char("x) Exit and abort\r\n", CH);
  send_to_char("Enter your choice:\r\n", CH);
  d->edit_mode = QEDIT_MAIN_MENU;
}

void qedit_parse(struct descriptor_data *d, char *arg)
{
  int number;
  float karma;

  switch (d->edit_mode)
  {
  case QEDIT_CONFIRM_EDIT:
    switch (*arg) {
    case 'y':
    case 'Y':
      qedit_disp_menu(d);
      break;
    case 'n':
    case 'N':
      STATE(d) = CON_PLAYING;
      free_quest(d->edit_quest);
      delete d->edit_quest;
      d->edit_quest = NULL;
      d->edit_number = 0;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("That's not a valid choice!\r\n", CH);
      send_to_char("Do you wish to edit it?\r\n", CH);
      break;
    }
    break;
  case QEDIT_CONFIRM_SAVESTRING:
    switch(*arg) {
    case 'y':
    case 'Y':
      if (real_quest(d->edit_number) == -1)
        boot_one_quest(QUEST);
      else
        reboot_quest(real_quest(d->edit_number), QUEST);
      if (!write_quests_to_disk(d->character->player_specials->saved.zonenum))
        send_to_char("There was an error in writing the zone's quests.\r\n", d->character);
      free_quest(d->edit_quest);
      delete d->edit_quest;
      d->edit_quest = NULL;
      d->edit_number = 0;
      d->edit_number2 = 0;
      STATE(d) = CON_PLAYING;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      send_to_char("Done.\r\n", d->character);
      break;
    case 'n':
    case 'N':
      send_to_char("Quest not saved, aborting.\r\n", d->character);
      STATE(d) = CON_PLAYING;
      free_quest(d->edit_quest);
      delete d->edit_quest;
      d->edit_quest = NULL;
      d->edit_number = 0;
      d->edit_number2 = 0;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char("Do you wish to save this quest internally?\r\n", d->character);
      break;
    }
    break;
  case QEDIT_MAIN_MENU:
    switch (*arg) {
    case 'q':
    case 'Q':
      d->edit_mode = QEDIT_CONFIRM_SAVESTRING;
      qedit_parse(d, "y");
      break;
    case 'x':
    case 'X':
      d->edit_mode = QEDIT_CONFIRM_SAVESTRING;
      qedit_parse(d, "n");
      break;
    case '1':
      send_to_char("Enter johnson's vnum: ", CH);
      d->edit_mode = QEDIT_JOHNSON;
      break;
    case '2':
      send_to_char("Enter allowed time (in mud minutes): ", CH);
      d->edit_mode = QEDIT_TIME;
      break;
    case '3':
      send_to_char("Enter minimum reputation: ", CH);
      d->edit_mode = QEDIT_MIN_REP;
      break;
    case '4':
      send_to_char("Enter bonus nuyen: ", CH);
      d->edit_mode = QEDIT_NUYEN;
      break;
    case '5':
      send_to_char("Enter bonus karma: ", CH);
      d->edit_mode = QEDIT_KARMA;
      break;
    case '6':
      CLS(CH);
      qedit_disp_obj_menu(d);
      break;
    case '7':
      CLS(CH);
      qedit_disp_mob_menu(d);
      break;
    case '8':
      send_to_char("Enter introductory text: ", d->character);
      d->edit_mode = QEDIT_INTRO;
      break;
    case '9':
      send_to_char("Enter decline text: ", d->character);
      d->edit_mode = QEDIT_DECLINE;
      break;
    case 'a':
    case 'A':
      send_to_char("Enter quit text: ", d->character);
      d->edit_mode = QEDIT_QUIT;
      break;
    case 'b':
    case 'B':
      send_to_char("Enter completed text: ", d->character);
      d->edit_mode = QEDIT_FINISH;
      break;
    case 'c':
    case 'C':
      send_to_char("Enter informational text:\r\n", d->character);
      d->edit_mode = QEDIT_INFO;
      d->str = new (char *);
      if (!d->str) {
        mudlog("Malloc failed!", NULL, LOG_SYSLOG, TRUE);
        shutdown();
      }

      *(d->str) = NULL;
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      break;
    case 'd':
    case 'D':
      send_to_char("Enter hour for johnson to start giving jobs: ", CH);
      d->edit_mode = QEDIT_SHOUR;
      break;
    case 'e':
    case 'E':
      send_to_char("Enter the string that will be given when the johnson comes to work:\n\r", CH);
      d->edit_mode = QEDIT_SSTRING;
      break;
    case 'f':
    case 'F':
      send_to_char("Enter the string that will be given when the johnson leaves work:\n\r", CH);
      d->edit_mode = QEDIT_ESTRING;
      break;
    case 'g':
    case 'G':
      send_to_char("Enter the string that will be given if quest is already complete:\n\r", CH);
      d->edit_mode = QEDIT_DONE;
      break;
    case 'h':
    case 'H':
      if (!access_level(CH, LVL_VICEPRES))
        qedit_disp_menu(d);
      else {
        send_to_char("Enter vnum of reward: ", CH);
        d->edit_mode = QEDIT_REWARD;
      }
      break;
    default:
      qedit_disp_menu(d);
      break;
    }
    break;
  case QEDIT_JOHNSON:
    number = atoi(arg);
    if (real_mobile(number) < 0) {
      send_to_char("No such mob!  Enter johnson's vnum: ", CH);
      return;
    } else {
      QUEST->johnson = number;
      qedit_disp_menu(d);
    }
    break;
  case QEDIT_TIME:
    number = atoi(arg);
    if (number < 30 || number > 1440)
      send_to_char("Time must range from 30 to 1440 mud minutes.\r\n"
                   "Enter allowed time: ", CH);
    else {
      QUEST->time = number;
      qedit_disp_menu(d);
    }
    break;
  case QEDIT_MIN_REP:
    number = atoi(arg);
    if (number < 0 || number > 500)
      send_to_char("Invalid value.  Enter minimum reputation: ", CH);
    else {
      QUEST->min_rep = number;
      send_to_char("Enter maximum reputation: ", CH);
      d->edit_mode = QEDIT_MAX_REP;
    }
    break;
  case QEDIT_MAX_REP:
    number = atoi(arg);
    if ((unsigned)number < QUEST->min_rep)
      send_to_char("Maximum reputation must be greater than the minimum.\r\n"
                   "Enter maximum reputation: ", CH);
    else if (number > 10000)
      send_to_char("Invalid value.  Enter maximum reputation: ", CH);
    else {
      QUEST->max_rep = number;
      qedit_disp_menu(d);
    }
    break;
  case QEDIT_NUYEN:
    number = atoi(arg);
    if (number < 0 || number > 500000)
      send_to_char("Invalid amount.  Enter bonus nuyen: ", CH);
    else {
      QUEST->nuyen = number;
      qedit_disp_menu(d);
    }
    break;
  case QEDIT_KARMA:
    karma = atof(arg);
    if (karma < 0.0 || karma > 25.0)
      send_to_char("Invalid amount.  Enter bonus karma: ", CH);
    else {
      QUEST->karma = (int)(karma * 100);
      qedit_disp_menu(d);
    }
    break;
  case QEDIT_M_MENU:
    switch (*arg) {
    case '1':
      qedit_list_mob_objectives(d);
      qedit_disp_mob_menu(d);
      break;
    case '2':
      send_to_char("Enter number of mobile objective to edit: ", CH);
      d->edit_mode = QEDIT_M_AWAIT_NUMBER;
      break;
    case '3':
      if (QUEST->num_mobs < QMAX_MOBS) {
        d->edit_number2 = QUEST->num_mobs;
        QUEST->num_mobs++;
        send_to_char("Enter vnum of mob: ", CH);
        d->edit_mode = QEDIT_M_VNUM;
      } else {
        CLS(CH);
        qedit_disp_mob_menu(d);
      }
      break;
    case 'q':
    case 'Q':
      qedit_disp_menu(d);
      break;
    default:
      CLS(CH);
      qedit_disp_mob_menu(d);
      break;
    }
    break;
  case QEDIT_M_AWAIT_NUMBER:
    number = atoi(arg);
    if (number < 0 || number >= QUEST->num_mobs) {
      CLS(CH);
      qedit_disp_mob_menu(d);
    } else {
      d->edit_number2 = number;
      d->edit_mode = QEDIT_M_VNUM;
      send_to_char("Enter vnum of mob: ", CH);
    }
    break;
  case QEDIT_M_VNUM:
    number = atoi(arg);
    if (real_mobile(number) < 0)
      send_to_char("No such mob.  Enter vnum of mob: ", CH);
    else {
      QUEST->mob[d->edit_number2].vnum = number;
      d->edit_mode = QEDIT_M_NUYEN;
      send_to_char("Enter nuyen reward: ", CH);
    }
    break;
  case QEDIT_M_NUYEN:
    number = atoi(arg);
    if (number < 0 || number > 25000)
      send_to_char("Invalid amount.  Enter nuyen reward: ", CH);
    else {
      QUEST->mob[d->edit_number2].nuyen = number;
      d->edit_mode = QEDIT_M_KARMA;
      send_to_char("Enter karma reward: ", CH);
    }
    break;
  case QEDIT_M_KARMA:
    karma = atof(arg);
    if (karma < 0.0 || karma > 5.0)
      send_to_char("Invalid amount.  Enter karma reward: ", CH);
    else {
      QUEST->mob[d->edit_number2].karma = (int)(karma * 100);
      qedit_disp_mob_loads(d);
    }
    break;
  case QEDIT_M_LOAD:
    number = atoi(arg);
    if (number < 0 || number >= NUM_MOB_LOADS)
      qedit_disp_mob_loads(d);
    else {
      QUEST->mob[d->edit_number2].load = (byte)(number);
      switch (QUEST->mob[d->edit_number2].load) {
      case QML_LOCATION:
        d->edit_mode = QEDIT_M_LDATA;
        send_to_char("Enter vnum of room to load mob into: ", CH);
        break;
      default:
        qedit_disp_mob_objectives(d);
        break;
      }
    }
    break;
  case QEDIT_M_OBJECTIVE:
    number = atoi(arg);
    if (number < 0 || number >= NUM_MOB_OBJECTIVES)
      qedit_disp_mob_objectives(d);
    else {
      QUEST->mob[d->edit_number2].objective = (byte)(number);
      switch (QUEST->mob[d->edit_number2].objective) {
      case QMO_LOCATION:
        d->edit_mode = QEDIT_M_ODATA;
        send_to_char(CH, "Enter vnum of room mob must be led to: ");
        break;
      case QMO_KILL_ESCORTEE:
        d->edit_mode = QEDIT_M_ODATA;
        send_to_char(CH, "Enter M# of mob to hunt ('l' to list, 'q' to quit): ");
        break;
      default:
        CLS(CH);
        qedit_disp_mob_menu(d);
        break;
      }
    }
    break;
  case QEDIT_M_LDATA:
    // only if the mob_load type is QML_LOCATION will we ever reach this
    number = atoi(arg);
    if (real_room(number) < 0)
      send_to_char(CH, "Invalid room.  Enter vnum of room to load mob into: ");
    else {
      QUEST->mob[d->edit_number2].l_data = number;
      qedit_disp_mob_objectives(d);
    }
    break;
  case QEDIT_M_LDATA2:
    break;             // unused (for now)
  case QEDIT_M_ODATA:
    number = atoi(arg);
    switch (QUEST->mob[d->edit_number2].objective) {
    case QMO_LOCATION:
      if (real_room(number) < 0)
        send_to_char(CH, "Invalid room.  Enter vnum of room mob "
                     "must be led to: ");
      else {
        QUEST->mob[d->edit_number2].o_data = number;
        qedit_disp_mob_menu(d);
      }
      break;
    case QMO_KILL_ESCORTEE:
      if (*arg == 'q' || *arg == 'Q') {
        QUEST->mob[d->edit_number2].vnum = 0;
        QUEST->mob[d->edit_number2].nuyen = 0;
        QUEST->mob[d->edit_number2].karma = 0;
        QUEST->mob[d->edit_number2].load = 0;
        QUEST->mob[d->edit_number2].objective = 0;
        QUEST->mob[d->edit_number2].l_data = 0;
        QUEST->mob[d->edit_number2].l_data2 = 0;
        QUEST->mob[d->edit_number2].o_data = 0;
        QUEST->num_mobs--;
        CLS(CH);
        qedit_disp_mob_menu(d);
      } else if (*arg == 'l' || *arg == 'L') {
        qedit_list_mob_objectives(d);
        send_to_char(CH, "Enter M# of mob to hunt ('l' to list, 'q' to quit): ");
      } else if (number < 0 || number >= QUEST->num_mobs)
        send_to_char(CH, "Invalid response.  "
                     "Enter M# of mob to hunt ('l' to list, 'q' to quit): ");
      else {
        QUEST->mob[d->edit_number2].o_data = number;
        CLS(CH);
        qedit_disp_mob_menu(d);
      }
      break;
    }
    break;
  case QEDIT_O_MENU:
    switch (*arg) {
    case '1':
      qedit_list_obj_objectives(d);
      qedit_disp_obj_menu(d);
      break;
    case '2':
      send_to_char("Enter number of item objective to edit: ", CH);
      d->edit_mode = QEDIT_O_AWAIT_NUMBER;
      break;
    case '3':
      if (QUEST->num_objs < QMAX_OBJS) {
        d->edit_number2 = QUEST->num_objs;
        QUEST->num_objs++;
        send_to_char("Enter vnum of item: ", CH);
        d->edit_mode = QEDIT_O_VNUM;
      } else {
        CLS(CH);
        qedit_disp_obj_menu(d);
      }
      break;
    case 'q':
    case 'Q':
      qedit_disp_menu(d);
      break;
    default:
      CLS(CH);
      qedit_disp_obj_menu(d);
      break;
    }
    break;
  case QEDIT_O_AWAIT_NUMBER:
    number = atoi(arg);
    if (number < 0 || number >= QUEST->num_objs) {
      CLS(CH);
      qedit_disp_obj_menu(d);
    } else {
      d->edit_number2 = number;
      d->edit_mode = QEDIT_O_VNUM;
      send_to_char("Enter vnum of item: ", CH);
    }
    break;
  case QEDIT_O_VNUM:
    number = atoi(arg);
    if (real_object(number) < 0)
      send_to_char("No such item.  Enter vnum of item: ", CH);
    else {
      QUEST->obj[d->edit_number2].vnum = number;
      d->edit_mode = QEDIT_O_NUYEN;
      send_to_char("Enter nuyen reward: ", CH);
    }
    break;
  case QEDIT_O_NUYEN:
    number = atoi(arg);
    if (number < 0 || number > 25000)
      send_to_char("Invalid amount.  Enter nuyen reward: ", CH);
    else {
      QUEST->obj[d->edit_number2].nuyen = number;
      d->edit_mode = QEDIT_O_KARMA;
      send_to_char("Enter karma reward: ", CH);
    }
    break;
  case QEDIT_O_KARMA:
    karma = atof(arg);
    if (karma < 0.0 || karma > 5.0)
      send_to_char("Invalid amount.  Enter karma reward: ", CH);
    else {
      QUEST->obj[d->edit_number2].karma = (int)(karma * 100);
      qedit_disp_obj_loads(d);
    }
    break;
  case QEDIT_O_LOAD:
    number = atoi(arg);
    if (number < 0 || number >= NUM_OBJ_LOADS)
      qedit_disp_obj_loads(d);
    else {
      QUEST->obj[d->edit_number2].load = (byte)(number);
      switch (QUEST->obj[d->edit_number2].load) {
      case QOL_TARMOB_I:
        d->edit_mode = QEDIT_O_LDATA;
        send_to_char(CH, "Enter M# of mob to give item to: "
                     "('l' to list, 'q' to quit): ");
        break;
      case QOL_TARMOB_E:
        d->edit_mode = QEDIT_O_LDATA;
        send_to_char(CH, "Enter M# of mob to equip item on: "
                     "('l' to list, 'q' to quit): ");
        break;
      case QOL_TARMOB_C:
        d->edit_mode = QEDIT_O_LDATA;
        send_to_char(CH, "Enter M# of mob to install item in: "
                     "('l' to list, 'q' to quit): ");
        break;
      case QOL_HOST:
        d->edit_mode = QEDIT_O_LDATA;
        send_to_char(CH, "Enter vnum of host to load item into: ");
        break;
      case QOL_LOCATION:
        d->edit_mode = QEDIT_O_LDATA;
        send_to_char(CH, "Enter vnum of room to load item into: ");
        break;
      default:
        qedit_disp_obj_objectives(d);
        break;
      }
    }
    break;
  case QEDIT_O_OBJECTIVE:
    number = atoi(arg);
    if (number < 0 || number >= NUM_OBJ_OBJECTIVES)
      qedit_disp_obj_objectives(d);
    else {
      QUEST->obj[d->edit_number2].objective = (byte)(number);
      switch (QUEST->obj[d->edit_number2].objective) {
      case QOO_TAR_MOB:
        d->edit_mode = QEDIT_O_ODATA;
        send_to_char(CH, "Enter M# of mob item must be delivered to "
                     "('l' to list, 'q' to quit): ");
        break;
      case QOO_LOCATION:
        d->edit_mode = QEDIT_O_ODATA;
        send_to_char(CH, "Enter vnum of room item must be delivered to: ");
        break;
      case QOO_RETURN_PAY:
      case QOO_UPLOAD:
        d->edit_mode = QEDIT_O_ODATA;
        send_to_char(CH, "Enter vnum of host paydata must be retreived/uploaded from: ");
        break;
      default:
        CLS(CH);
        qedit_disp_obj_menu(d);
        break;
      }
    }
    break;
  case QEDIT_O_LDATA:
    number = atoi(arg);
    switch (QUEST->obj[d->edit_number2].load) {
    case QOL_TARMOB_I:
      if (*arg == 'l' || *arg == 'L') {
        qedit_list_mob_objectives(d);
        send_to_char(CH, "Enter M# of mob to give item to: "
                     "('l' to list, 'q' to quit): ");
      } else if (number < 0 || number >= top_of_mobt)
        send_to_char(CH, "Enter M# of mob to give item to: "
                     "('l' to list, 'q' to quit): ");
      else {
        QUEST->obj[d->edit_number2].l_data = number;
        qedit_disp_obj_objectives(d);
      }
      break;
    case QOL_TARMOB_E:
      if (*arg == 'l' || *arg == 'L') {
        qedit_list_mob_objectives(d);
        send_to_char(CH, "Enter M# of mob to equip item on: "
                     "('l' to list, 'q' to quit): ");
      } else if (number < 0 || number >= top_of_mobt)
        send_to_char(CH, "Enter M# of mob to equip item on: "
                     "('l' to list, 'q' to quit): ");
      else {
        QUEST->obj[d->edit_number2].l_data = number;
        qedit_disp_locations(d);
      }
      break;
    case QOL_TARMOB_C:
      if (*arg == 'l' || *arg == 'L') {
        qedit_list_mob_objectives(d);
        send_to_char(CH, "Enter M# of mob to install item in: "
                     "('l' to list, 'q' to quit): ");
      } else  if (number < 0 || number >= top_of_mobt)
        send_to_char(CH, "Enter M# of mob to install item in: "
                     "('l' to list, 'q' to quit): ");
      else {
        QUEST->obj[d->edit_number2].l_data = number;
        qedit_disp_obj_objectives(d);
      }
      break;
    case QOL_LOCATION:
      if (real_room(number) < 0)
        send_to_char(CH, "Enter vnum of room to load item into: ");
      else {
        QUEST->obj[d->edit_number2].l_data = number;
        qedit_disp_obj_objectives(d);
      }
      break;
    case QOL_HOST:
      if (real_host(number) < 0)
        send_to_char(CH, "Enter vnum of host to load item into: ");
      else {
        QUEST->obj[d->edit_number2].l_data = number;
        qedit_disp_obj_objectives(d);
      }
      break;
    }
    break;
  case QEDIT_O_LDATA2:
    // only QOL_TARMOB_E should ever make it here
    number = atoi(arg);
    if (number < 1 || number > (NUM_WEARS - 1))
      qedit_disp_locations(d);
    else {
      QUEST->obj[d->edit_number2].l_data2 = number - 1;
      qedit_disp_obj_objectives(d);
    }
    break;
  case QEDIT_O_ODATA:
    number = atoi(arg);
    switch (QUEST->obj[d->edit_number2].objective) {
    case QOO_TAR_MOB:
      if (*arg == 'q' || *arg == 'Q') {
        CLS(CH);
        qedit_disp_obj_menu(d);
      } else if (*arg == 'l' || *arg == 'L') {
        qedit_list_mob_objectives(d);
        send_to_char(CH, "Enter M# of mob item must be delivered to: "
                     "('l' to list, 'q' to quit): ");
      } else if (real_mobile(number) < 0)
        send_to_char(CH, "Enter M# of mob item must be delivered to: "
                     "('l' to list, 'q' to quit): ");
      else {
        QUEST->obj[d->edit_number2].o_data = number;
        CLS(CH);
        qedit_disp_obj_menu(d);
      }
      break;
    case QOO_LOCATION:
      if (real_room(number) < 0)
        send_to_char(CH, "Enter vnum of room item must be delivered to: ");
      else {
        QUEST->obj[d->edit_number2].o_data = number;
        CLS(CH);
        qedit_disp_obj_menu(d);
      }
      break;
    case QOO_UPLOAD:
    case QOO_RETURN_PAY:
      if (real_host(number) < 0)
        send_to_char(CH, "Enter vnum of host paydata must be retreived/uploaded from: ");
      else {
        QUEST->obj[d->edit_number2].o_data = number;
        CLS(CH);
        qedit_disp_obj_menu(d);
      }
      break;
    }
    break;
  case QEDIT_INTRO:
    if (QUEST->intro)
      delete [] QUEST->intro;
    QUEST->intro = str_dup(arg);
    qedit_disp_menu(d);
    break;
  case QEDIT_DECLINE:
    if (QUEST->decline)
      delete [] QUEST->decline;
    QUEST->decline = str_dup(arg);
    qedit_disp_menu(d);
    break;
  case QEDIT_QUIT:
    if (QUEST->quit)
      delete [] QUEST->quit;
    QUEST->quit = str_dup(arg);
    qedit_disp_menu(d);
    break;
  case QEDIT_FINISH:
    if (QUEST->finish)
      delete [] QUEST->finish;
    QUEST->finish = str_dup(arg);
    qedit_disp_menu(d);
    break;
  case QEDIT_INFO:
    break;               // we should never get here
  case QEDIT_REWARD:
    number = atoi(arg);
    if (real_object(number) < 0)
      send_to_char(CH, "Invalid vnum.  Enter vnum of reward: ");
    else {
      QUEST->reward = number;
      qedit_disp_menu(d);
    }
    break;
  case QEDIT_SHOUR:
    number = atoi(arg);
    if ( number > 23 || number < -1 ) {
      send_to_char("Needs to be between -1 and 23.\n\rWhat time does he start work? ", CH);
    } else {
      QUEST->s_time = number;
      d->edit_mode = QEDIT_EHOUR;
      send_to_char("Enter hour for johnson to stop giving jobs: ", CH);
    }
    break;
  case QEDIT_EHOUR:
    number = atoi(arg);
    if ( number > 23 || number < 0 ) {
      send_to_char("Needs to be between 0 and 23.\n\rWhat time does he stop work? ", CH);
      return;
    } else {
      QUEST->e_time = number;
      qedit_disp_menu(d);
    }
    break;
  case QEDIT_SSTRING:
    if (QUEST->s_string)
      delete [] QUEST->s_string;
    QUEST->s_string = str_dup(arg);
    qedit_disp_menu(d);
    break;
  case QEDIT_ESTRING:
    if (QUEST->e_string)
      delete [] QUEST->e_string;
    QUEST->e_string = str_dup(arg);
    qedit_disp_menu(d);
    break;
  case QEDIT_DONE:
    if (QUEST->done)
      delete [] QUEST->done;
    QUEST->done = str_dup(arg);
    qedit_disp_menu(d);
    break;
  }
}
