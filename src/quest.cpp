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
#include <vector>
#include <algorithm>

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
#include "config.h"
#include "bullet_pants.h"

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
extern int perform_drop(struct char_data * ch, struct obj_data * obj, byte mode,
                        const char *sname, struct room_data *random_donation_room);

unsigned int get_johnson_overall_max_rep(struct char_data *johnson);
unsigned int get_johnson_overall_min_rep(struct char_data *johnson);

ACMD_CONST(do_say);
ACMD_DECLARE(do_action);
SPECIAL(johnson);
ACMD_DECLARE(do_echo);

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
    "Return Paydata to Johnson",
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
        (room = real_room(quest_table[num].mob[i].l_data)) > -1)
    {
      mob = read_mobile(rnum, REAL);
      mob->mob_specials.quest_id = GET_IDNUM(ch);
      char_to_room(mob, &world[room]);
      act("$n has arrived.", FALSE, mob, 0, 0, TO_ROOM);
      if(quest_table[num].mob[i].objective == QMO_LOCATION)
        add_follower(mob, ch);
      for (j = 0; j < quest_table[num].num_objs; j++)
        if (quest_table[num].obj[j].l_data == i &&
            (rnum = real_object(quest_table[num].obj[j].vnum)) > -1) {
          switch (quest_table[num].obj[j].load) {
          case QOL_TARMOB_I:
            obj = read_object(rnum, REAL);
            obj->obj_flags.quest_id = GET_IDNUM(ch);
            obj->obj_flags.extra_flags.SetBits(ITEM_NODONATE, ITEM_NORENT, ITEM_NOSELL, ENDBIT);
            obj_to_char(obj, mob);
            break;
          case QOL_TARMOB_E:
            pos = quest_table[num].obj[j].l_data2;
            if (pos >= 0 && pos < NUM_WEARS && (!GET_EQ(mob, pos) ||
                                                (pos == WEAR_WIELD && !GET_EQ(mob, WEAR_HOLD)))) {
              obj = read_object(rnum, REAL);
              obj->obj_flags.quest_id = GET_IDNUM(ch);
              obj->obj_flags.extra_flags.SetBits(ITEM_NODONATE, ITEM_NORENT, ITEM_NOSELL, ENDBIT);
              equip_char(mob, obj, pos);

              // Could be a weapon-- make sure it's loaded if it is.
              if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && IS_GUN(GET_WEAPON_ATTACK_TYPE(obj))) {
                // If it's carried by an NPC, make sure it's loaded.
                if (GET_WEAPON_MAX_AMMO(obj) > 0) {
                  // Reload from their ammo.
                  for (int index = 0; index < NUM_AMMOTYPES; index++) {
                    if (GET_BULLETPANTS_AMMO_AMOUNT(mob, GET_WEAPON_ATTACK_TYPE(obj), npc_ammo_usage_preferences[index]) > 0) {
                      reload_weapon_from_bulletpants(mob, obj, npc_ammo_usage_preferences[index]);
                      break;
                    }
                  }

                  // If they failed to reload, they have no ammo. Give them some normal and reload with it.
                  if (!obj->contains || GET_MAGAZINE_AMMO_COUNT(obj->contains) == 0) {
                    GET_BULLETPANTS_AMMO_AMOUNT(mob, GET_WEAPON_ATTACK_TYPE(obj), AMMO_NORMAL) = GET_WEAPON_MAX_AMMO(obj) * NUMBER_OF_MAGAZINES_TO_GIVE_TO_UNEQUIPPED_MOBS;
                    reload_weapon_from_bulletpants(mob, obj, AMMO_NORMAL);

                    // Decrement their debris-- we want this reload to not create clutter.
                    get_ch_in_room(mob)->debris--;
                  }
                }

                // Set the firemode.
                if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(obj), 1 << MODE_BF)) {
                  GET_WEAPON_FIREMODE(obj) = MODE_BF;
                } else if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(obj), 1 << MODE_FA)) {
                  GET_WEAPON_FIREMODE(obj) = MODE_FA;
                  GET_OBJ_TIMER(obj) = 10;
                } else if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(obj), 1 << MODE_SA)) {
                  GET_WEAPON_FIREMODE(obj) = MODE_SA;
                } else if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(obj), 1 << MODE_SS)) {
                  GET_WEAPON_FIREMODE(obj) = MODE_SS;
                }
              }
              // Weapon is loaded, and the mob has its ammo.
            }
            break;
          case QOL_TARMOB_C:
            obj = read_object(rnum, REAL);
            obj->obj_flags.quest_id = GET_IDNUM(ch);
            obj->obj_flags.extra_flags.SetBits(ITEM_NODONATE, ITEM_NORENT, ITEM_NOSELL, ENDBIT);
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
    }
    else if (quest_table[num].mob[i].load == QML_FOLQUESTER &&
               (rnum = real_mobile(quest_table[num].mob[i].vnum)) > -1)
    {
      mob = read_mobile(rnum, REAL);
      mob->mob_specials.quest_id = GET_IDNUM(ch);
      char_to_room(mob, ch->in_room);
      act("$n has arrived.", FALSE, mob, 0, 0, TO_ROOM);
      for (j = 0; j < quest_table[num].num_objs; j++)
        if (quest_table[num].obj[j].l_data == i &&
            (rnum = real_object(quest_table[num].obj[j].vnum)) > -1) {
          switch (quest_table[num].obj[j].load) {
          case QOL_TARMOB_I:
            obj = read_object(rnum, REAL);
            obj->obj_flags.quest_id = GET_IDNUM(ch);
            obj->obj_flags.extra_flags.SetBits(ITEM_NODONATE, ITEM_NORENT, ITEM_NOSELL, ENDBIT);
            obj_to_char(obj, mob);
            break;
          case QOL_TARMOB_E:
            pos = quest_table[num].obj[j].l_data2;
            if (pos >= 0 && pos < NUM_WEARS && (!GET_EQ(mob, pos) ||
                                                (pos == WEAR_WIELD && !GET_EQ(mob, WEAR_HOLD)))) {
              obj = read_object(rnum, REAL);
              obj->obj_flags.quest_id = GET_IDNUM(ch);
              obj->obj_flags.extra_flags.SetBits(ITEM_NODONATE, ITEM_NORENT, ITEM_NOSELL, ENDBIT);
              equip_char(mob, obj, pos);
            }
            break;
          case QOL_TARMOB_C:
            obj = read_object(rnum, REAL);
            obj->obj_flags.quest_id = GET_IDNUM(ch);
            obj->obj_flags.extra_flags.SetBits(ITEM_NODONATE, ITEM_NORENT, ITEM_NOSELL, ENDBIT);
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
          obj->obj_flags.extra_flags.SetBits(ITEM_NODONATE, ITEM_NORENT, ITEM_NOSELL, ENDBIT);
          obj_to_room(obj, &world[room]);
        }
        obj = NULL;
        break;
      case QOL_HOST:
        if ((room = real_host(quest_table[num].obj[i].l_data)) > -1) {
          obj = read_object(rnum, REAL);
          obj->obj_flags.quest_id = GET_IDNUM(ch);
          obj->obj_flags.extra_flags.SetBits(ITEM_NODONATE, ITEM_NORENT, ITEM_NOSELL, ENDBIT);
          GET_OBJ_VAL(obj, 7) = GET_IDNUM(ch);
          GET_OBJ_VAL(obj, 9) = 1;
          obj_to_host(obj, &matrix[room]);
        }
        obj = NULL;
        break;
      case QOL_JOHNSON:
        obj = read_object(rnum, REAL);
        obj->obj_flags.quest_id = GET_IDNUM(ch);
        obj->obj_flags.extra_flags.SetBits(ITEM_NODONATE, ITEM_NORENT, ITEM_NOSELL, ENDBIT);
        obj_to_char(obj, johnson);
        if (!perform_give(johnson, ch, obj)) {
          char buf[512];
          snprintf(buf, sizeof(buf), "Looks like your hands are full. You'll need %s for the run.", decapitalize_a_an(obj->text.name));
          do_say(johnson, buf, 0, 0);
          perform_drop(johnson, obj, SCMD_DROP, "drop", NULL);
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
      act("$n slips away quietly.", FALSE, mob, 0, 0, TO_ROOM);
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
    if (quest_table[GET_QUEST(ch)].obj[i].vnum == vnum) {
      switch (quest_table[GET_QUEST(ch)].obj[i].objective)
      {
      case QOO_JOHNSON:
        if (GET_MOB_SPEC(mob) && (GET_MOB_SPEC(mob) == johnson || GET_MOB_SPEC2(mob) == johnson) && memory(mob, ch)) {
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
        if (GET_MOB_SPEC(mob) && (GET_MOB_SPEC(mob) == johnson || GET_MOB_SPEC2(mob) == johnson) && memory(mob, ch)) {
          if (GET_DECK_ACCESSORY_FILE_HOST_VNUM(obj) == quest_table[GET_QUEST(ch)].obj[i].o_data) {
            ch->player_specials->obj_complete[i] = 1;
            return TRUE;
          }
        }
      }
    }
  return FALSE;
}

void check_quest_delivery(struct char_data *ch, struct obj_data *obj)
{
  if (!ch || IS_NPC(ch) || !GET_QUEST(ch))
    return;

  int i;
  if (ch->in_room) {
    for (i = 0; i < quest_table[GET_QUEST(ch)].num_objs; i++) {
      if (quest_table[GET_QUEST(ch)].obj[i].objective == QOO_LOCATION &&
          GET_OBJ_VNUM(obj) == quest_table[GET_QUEST(ch)].obj[i].vnum &&
          ch->in_room->number == quest_table[GET_QUEST(ch)].obj[i].o_data)
      {
        ch->player_specials->obj_complete[i] = 1;
        return;
      }
    }
  }
  if (ch->persona && ch->persona->in_host && quest_table[GET_QUEST(ch)].obj[i].objective == QOO_UPLOAD &&
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
        mob->in_room->number == quest_table[GET_QUEST(ch)].mob[i].o_data)
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
        // send_to_char("check_quest_kill: +1\r\n", ch);
        ch->player_specials->mob_complete[i]++;
        return;
      }
  // send_to_char("check_quest_kill: didn't count\r\n", ch);
}

void end_quest(struct char_data *ch)
{
  if (IS_NPC(ch) || !GET_QUEST(ch))
    return;

  extract_quest_targets(GET_IDNUM(ch));
  // We mark the quest as completed here because if you fail...
  //well you failed. Better luck next time chummer.
  for (int i = QUEST_TIMER - 1; i > 0; i--)
    GET_LQUEST(ch, i) = GET_LQUEST(ch, i - 1);

  GET_LQUEST(ch, 0) = quest_table[GET_QUEST(ch)].vnum;
  GET_QUEST(ch) = 0;

  delete [] ch->player_specials->mob_complete;
  delete [] ch->player_specials->obj_complete;
  ch->player_specials->mob_complete = NULL;
  ch->player_specials->obj_complete = NULL;
}

bool rep_too_high(struct char_data *ch, int num)
{
  if (num < 0 || num > top_of_questt)
    return TRUE;

  if (GET_REP(ch) > quest_table[num].max_rep)
    return TRUE;

  return FALSE;
}

bool rep_too_low(struct char_data *ch, int num)
{
  if (num < 0 || num > top_of_questt)
    return TRUE;

  if (GET_REP(ch) < quest_table[num].min_rep)
    return TRUE;

  return FALSE;
}

bool would_be_rewarded_for_turnin(struct char_data *ch) {
  int nuyen = 0, karma = 0;

  bool all = TRUE;

  for (int i = 0; i < quest_table[GET_QUEST(ch)].num_objs; i++)
    if (ch->player_specials->obj_complete[i]) {
      if (quest_table[GET_QUEST(ch)].obj[i].objective == QOO_DSTRY_MANY) {
        nuyen += quest_table[GET_QUEST(ch)].obj[i].nuyen * ch->player_specials->obj_complete[i];
        karma += quest_table[GET_QUEST(ch)].obj[i].karma * ch->player_specials->obj_complete[i];
      } else {
        nuyen += quest_table[GET_QUEST(ch)].obj[i].nuyen;
        karma += quest_table[GET_QUEST(ch)].obj[i].karma;
      }
    } else
      all = FALSE;

  for (int i = 0; i < quest_table[GET_QUEST(ch)].num_mobs; i++)
    if (ch->player_specials->mob_complete[i]) {
      if (quest_table[GET_QUEST(ch)].mob[i].objective == QMO_KILL_MANY) {
        nuyen += quest_table[GET_QUEST(ch)].mob[i].nuyen * ch->player_specials->mob_complete[i];
        karma += quest_table[GET_QUEST(ch)].mob[i].karma * ch->player_specials->mob_complete[i];
      } else {
        nuyen += quest_table[GET_QUEST(ch)].mob[i].nuyen;
        karma += quest_table[GET_QUEST(ch)].mob[i].karma;
      }
    } else
      all = FALSE;

  if (all)
    return TRUE;

  return nuyen > 0 || karma > 0;
}

void reward(struct char_data *ch, struct char_data *johnson)
{
  if (vnum_from_non_connected_zone(quest_table[GET_QUEST(ch)].vnum)) {
    send_to_char(ch, "Quest reward suppressed due to this zone not being marked as connected to the game world.\r\n");
    end_quest(ch);
    return;
  }

  struct follow_type *f;
  struct obj_data *obj;
  int i, nuyen = 0, karma = 0, num, all = 1, old;
  UNUSED(old);

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
  nuyen = negotiate(ch, johnson, 0, nuyen, 0, FALSE, FALSE) * NUYEN_GAIN_MULTIPLIER * ((float) GET_CHAR_MULTIPLIER(ch) / 100);

  if (AFF_FLAGGED(ch, AFF_GROUP))
  {
    num = 1;
    for (f = ch->followers; f; f = f->next)
      if (!IS_NPC(f->follower) && AFF_FLAGGED(f->follower, AFF_GROUP) && !(rep_too_low(f->follower, GET_QUEST(ch)) || rep_too_high(f->follower, GET_QUEST(ch))))
        num++;

    nuyen = (int)(nuyen / num);
    karma = (int)(karma / num);

    for (f = ch->followers; f; f = f->next) {
      if (IS_NPC(f->follower))
        continue;

      if (AFF_FLAGGED(f->follower, AFF_GROUP) && !(rep_too_low(f->follower, GET_QUEST(ch)) || rep_too_high(f->follower, GET_QUEST(ch)))) {
        if (f->follower->in_room != ch->in_room) {
          snprintf(buf, sizeof(buf), "^YGroup room mismatch for %s and %s.", GET_CHAR_NAME(ch), GET_CHAR_NAME(f->follower));
          mudlog(buf, ch, LOG_SYSLOG, TRUE);
        } else {
          old = (int)(GET_KARMA(f->follower) / 100);
          gain_nuyen(f->follower, nuyen, NUYEN_INCOME_AUTORUNS);
          int gained = gain_karma(f->follower, karma, TRUE, FALSE, TRUE);
          send_to_char(f->follower, "You gain %0.2f karma and %d nuyen for being in %s's group.\r\n", (float) gained * 0.01, nuyen, GET_CHAR_NAME(ch));
        }
      } else if (!AFF_FLAGGED(f->follower, AFF_GROUP)) {
        send_to_char(ch, "^y(OOC note: %s wasn't grouped, so didn't get a cut of the pay.)^n\r\n",
                     GET_CHAR_NAME(f->follower), HSSH(f->follower));
        send_to_char("^y(OOC note: You didn't meet the qualifications for this run, so you didn't get a cut of the pay.)^n\r\n", f->follower);
      } else {
        send_to_char(ch, "^y(OOC note: %s didn't meet the qualifications for this run, so %s didn't get a cut of the pay.)^n\r\n",
                     GET_CHAR_NAME(f->follower), HSSH(f->follower));
        send_to_char("^y(OOC note: You didn't meet the qualifications for this run, so you didn't get a cut of the pay.)^n\r\n", f->follower);
      }
    }
  }
  gain_nuyen(ch, nuyen, NUYEN_INCOME_AUTORUNS);
  int gained = gain_karma(ch, karma, TRUE, FALSE, TRUE);
  act("$n gives some nuyen to $N.", TRUE, johnson, 0, ch, TO_NOTVICT);
  act("You give some nuyen to $N.", TRUE, johnson, 0, ch, TO_CHAR);
  snprintf(buf, sizeof(buf), "$n gives you %d nuyen.", nuyen);
  act(buf, FALSE, johnson, 0, ch, TO_VICT);
  send_to_char(ch, "You gain %.2f karma.\r\n", ((float) gained / 100));
  end_quest(ch);
}

//Comparator function for sorting quest
bool compareRep(const quest_entry &a, const quest_entry &b)
{
  return (a.rep < b.rep);
}

// New quest function builds a list of quests that exclude already
//done, and outgrown, sorts it by reputation and returns the lowest
//rep one first. It returns 0 if no more quests are available or -1 if
//the johnson is broken.
int new_quest(struct char_data *mob, struct char_data *ch)
{
  int i, num = 0;

  quest_entry temp_entry;
  std::vector<quest_entry> qlist;

  for (i = 0; i <= top_of_questt; i++)
    if (quest_table[i].johnson == GET_MOB_VNUM(mob))
      num++;

  if (num < 1)
  {
    if (mob_index[mob->nr].func == johnson)
      mob_index[mob->nr].func = mob_index[mob->nr].sfunc;
    mob_index[mob->nr].sfunc = NULL;
    snprintf(buf, sizeof(buf), "Stripping Johnson status from %s (%ld) due to mob not having any quests to assign.",
            GET_NAME(mob), GET_MOB_VNUM(mob));
    mudlog(buf, NULL, LOG_SYSLOG, true);
    return -1;
  }

  // Build array of quests for this johnson, excluding quests that are already
  // done or max_rep is below character rep. We include those with min_rep
  // higher than character rep because we want johnsons to hint to available
  // runs at higher character rep.
  for (i = 0;i < top_of_questt;i++) {
    bool allow_disconnected = vnum_from_non_connected_zone(quest_table[i].johnson);

    if (quest_table[i].johnson == GET_MOB_VNUM(mob)
      && (allow_disconnected || !vnum_from_non_connected_zone(quest_table[i].vnum)))
    {
        if (GET_REP(ch) > quest_table[i].max_rep) {
          continue;
        }

        bool found = FALSE;
        for (int q = QUEST_TIMER - 1; q >= 0; q--) {
          if (GET_LQUEST(ch, q) == quest_table[i].vnum) {
            found = TRUE;
            break;
          }
        }
        if (found) {
          continue;
        } else {
          temp_entry.index = i;
          temp_entry.rep = quest_table[i].min_rep;
          qlist.push_back(temp_entry);
        }
    }
  }
  // Sort vector by reputation and return a quest if vector is not empty.
  if (!qlist.empty()) {
    sort(qlist.begin(), qlist.end(), compareRep);
    return qlist[0].index;
  }

  return 0;
}

void handle_info(struct char_data *johnson, int num)
{
  int allowed, pos, i, speech_index = 0;

  // Want to control how much the Johnson says per tick? Change this magic number.
  char speech[strlen(GET_NAME(johnson)) + 200];

  // Calculate how much text we can put into the speech buffer, leaving room for ellipses and \0.
  allowed = sizeof(speech) - 7;

  // Spare1 is the position in the info string we last left off at.
  pos = GET_SPARE1(johnson);

  // If we've continued from earlier, let's go ahead and prepend some '...'.
  if (pos > 0)
    for (int ellipses = 0; ellipses < 3; ellipses++)
      speech[speech_index++] = '.';

  // i is the total length of the info string. We skip any newlines at the end.
  i = strlen(quest_table[num].info);
  while (quest_table[num].info[i] == '\r' || quest_table[num].info[i] == '\n' || quest_table[num].info[i] == '\0')
    i--;

  // We assume that all info strings will have ellipses (aka '...') at the end.
  bool will_add_ellipses = TRUE;

  // If the entire string won't fit, find the space or newline closest to the end. We'll write up to that.
  if ((pos + allowed) < i) {
    for (i = pos + allowed; i > pos; i--)
      if ((isspace(*(quest_table[num].info + i)) || *(quest_table[num].info + i) == '\r') &&
          isprint(*(quest_table[num].info + i - 1)) &&
          !isspace(*(quest_table[num].info + i - 1)))
        break;
    GET_SPARE1(johnson) = i + 1;
  }

  // Otherwise, we'll be done talking after this call-- wipe out their spare1 data
  //  (position in string) .
  else {
    GET_SPARE1(johnson) = -1;
    will_add_ellipses = FALSE;
  }

  // Print the string into the speech buff, replacing newlines with spaces.
  for (; pos < i && speech_index < allowed; pos++)
  {
    if (*(quest_table[num].info + pos) == '\n')
      continue;
    if (*(quest_table[num].info + pos) == '\r')
      speech[speech_index++] = ' ';
    else
      speech[speech_index++] = *(quest_table[num].info + pos);
  }

  // Add the final ellipses and cap it off with a '\0'.
  if (will_add_ellipses)
    for (int ellipses = 0; ellipses < 3; ellipses++)
      speech[speech_index++] = '.';

  speech[speech_index] = '\0';

  // Say it.
  do_say(johnson, speech, 0, 0);
}

SPECIAL(johnson)
{
  struct char_data *johnson = (struct char_data *) me, *temp = NULL;
  int i, obj_complete = 0, mob_complete = 0, num, new_q, cached_new_q = -2, comm = CMD_JOB_NONE;

  if (!IS_NPC(johnson))
    return FALSE;

  if (!cmd) {
    if (GET_SPARE1(johnson) >= 0) {
      for (temp = johnson->in_room->people; temp; temp = temp->next_in_room)
        if (memory(johnson, temp))
          break;
      if (!temp) {
        GET_SPARE1(johnson) = -1;
      } else if (GET_QUEST(temp)) {
        handle_info(johnson, GET_QUEST(temp));
      } else {
        // We're in the gap between someone asking for a job and accepting it. Do nothing.
      }
    }
    return FALSE;
  }

  skip_spaces(&argument);

  bool need_to_speak = FALSE;
  bool need_to_act = FALSE;
  if (CMD_IS("say") || CMD_IS("'") || CMD_IS("sayto") || CMD_IS("\"")) {
    if (str_str(argument, "quit"))
      comm = CMD_JOB_QUIT;
    else if (str_str(argument, "collect") || str_str(argument, "complete") || str_str(argument, "done") || str_str(argument, "finish") || str_str(argument, "pay"))
      comm = CMD_JOB_DONE;
    else if (str_str(argument, "work") || str_str(argument, "business") ||
             str_str(argument, "run") || str_str(argument, "shadowrun") ||
             str_str(argument, "job"))
      comm = CMD_JOB_START;
    else if (str_str(argument, "yes") || str_str(argument, "accept") || str_str(argument, "yeah")
            || str_str(argument, "sure") || str_str(argument, "okay"))
      comm = CMD_JOB_YES;
    else if (strstr(argument, "no"))
      comm = CMD_JOB_NO;
    else if (access_level(ch, LVL_BUILDER) && !str_cmp(argument, "clear")) {
      for (int i = QUEST_TIMER - 1; i >= 0; i--)
        GET_LQUEST(ch, i) = 0;
      send_to_char("OK, your quest history has been cleared.\r\n", ch);
      return FALSE;
    } else {
      //snprintf(buf, sizeof(buf), "INFO: No Johnson keywords found in %s's speech: '%s'.", GET_CHAR_NAME(ch), argument);
      //mudlog(buf, ch, LOG_SYSLOG, TRUE);
      return FALSE;
    }
    need_to_speak = TRUE;
  } else if (CMD_IS("nod") || CMD_IS("agree")) {
    comm = CMD_JOB_YES;
    need_to_act = TRUE;
  } else if (CMD_IS("shake") || CMD_IS("disagree")) {
    comm = CMD_JOB_NO;
    need_to_act = TRUE;
  } else if (CMD_IS("quests") || CMD_IS("jobs")) {
    // Intercept the 'quests' and 'jobs' ease-of-use commands to give them a new one if they don't have one yet.
    if (!GET_QUEST(ch)) {
      do_say(ch, "I'm looking for a job.", 0, 0);
      comm = CMD_JOB_START;
    } else {
      return FALSE;
    }
  } else if (CMD_IS("complete")) {
    if (GET_QUEST(ch)) {
      do_say(ch, "I've finished the job.", 0, 0);
      comm = CMD_JOB_DONE;
    } else {
      return FALSE;
    }
  } else
    return FALSE;

  if (IS_ASTRAL(ch)) {
    send_to_char("Astral projections aren't really employable.\r\n", ch);
    return FALSE;
  }

  if (IS_NPC(ch)) {
    send_to_char("NPCs don't get autoruns, go away.\r\n", ch);
    return FALSE;
  }

  if (PRF_FLAGGED(ch, PRF_NOHASSLE)) {
    send_to_char("You can't take runs with NOHASSLE turned on-- TOGGLE NOHASSLE to turn it off.\r\n", ch);
    return FALSE;
  }

  if (PRF_FLAGGED(ch, PRF_QUEST)) {
    send_to_char("You can't take autoruns while questing-- TOGGLE QUEST to disable your questing flag, then try again.\r\n", ch);
    return FALSE;
  }

  if (AFF_FLAGGED(johnson, AFF_GROUP) && ch->master) {
    send_to_char("I don't know how you ended up leading this Johnson around, but you can't take quests from your charmies.\r\n", ch);
    snprintf(buf, sizeof(buf), "WARNING: %s somehow managed to start leading Johnson %s.", GET_CHAR_NAME(ch), GET_NAME(johnson));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  // Hack to get around the fact that moving the failure check after the interact check would make you double-speak and double-act.
  if (need_to_speak)
    do_say(ch, argument, 0, 0);
  if (need_to_act)
    do_action(ch, argument, cmd, 0);

  switch (comm) {
    case CMD_JOB_QUIT:
      // Precondition: I cannot be talking right now.
      if (GET_SPARE1(johnson) == 0) {
        if (!memory(johnson, ch)) {
          do_say(johnson, "Hold on, I'm talking to someone else right now.", 0, 0);
          return TRUE;
        } else {
          do_say(johnson, "I'm lookin' for a yes-or-no answer, chummer.", 0, 0);
          return TRUE;
        }
      }

      // Precondition: You must be on a quest.
      if (!GET_QUEST(ch)) {
        do_say(johnson, "You're not even on a run right now.", 0, 0);
        return TRUE;
      }

      // Precondition: You must have gotten the quest from me.
      if (!memory(johnson, ch)) {
        do_say(johnson, "Whoever you got your job from, it wasn't me. What, do we all look alike to you?", 0 , 0);
        send_to_char("(OOC note: You can hit RECAP to see who gave you your current job.)\r\n", ch);
        return TRUE;
      }

      // Drop the quest.
      if (quest_table[GET_QUEST(ch)].quit)
        do_say(johnson, quest_table[GET_QUEST(ch)].quit, 0, 0);
      else {
        snprintf(buf, sizeof(buf), "WARNING: Null string in quest %ld!", quest_table[GET_QUEST(ch)].vnum);
        mudlog(buf, ch, LOG_SYSLOG, TRUE);
        do_say(johnson, "Fine.", 0, 0);
      }

      end_quest(ch);
      forget(johnson, ch);
      return TRUE;
    case CMD_JOB_DONE:
      // Precondition: I cannot be talking right now.
      if (GET_SPARE1(johnson) == 0) {
        if (!memory(johnson, ch)) {
          do_say(johnson, "Hold on, I'm talking to someone else right now.", 0, 0);
          return TRUE;
        } else {
          do_say(johnson, "I'm lookin' for a yes-or-no answer, chummer.", 0, 0);
          return TRUE;
        }
      }

      // Precondition: You must be on a quest.
      if (!GET_QUEST(ch)) {
        do_say(johnson, "You're not even on a run right now.", 0, 0);
        return TRUE;
      }

      // Precondition: You must have gotten the quest from me.
      if (!memory(johnson, ch)) {
        do_say(johnson, "Whoever you got your job from, it wasn't me. What, do we all look alike to you?", 0 , 0);
        send_to_char("(OOC note: You can hit RECAP to see who gave you your current job.)\r\n", ch);
        return TRUE;
      }

      // Check for some form of completion-- even if one thing is done, we'll allow them to turn in the quest.
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

      // Process turnin of the quest. The reward() function handles the work here.
      if (obj_complete || mob_complete) {
        if (!would_be_rewarded_for_turnin(ch)) {
          do_say(johnson, "You're not done yet!", 0, 0);
          return TRUE;
        }

        if (quest_table[GET_QUEST(ch)].finish)
          do_say(johnson, quest_table[GET_QUEST(ch)].finish, 0, 0);
        else {
          snprintf(buf, sizeof(buf), "WARNING: Null string in quest %ld!", quest_table[GET_QUEST(ch)].vnum);
          mudlog(buf, ch, LOG_SYSLOG, TRUE);
          do_say(johnson, "Well done.", 0, 0);
        }
        reward(ch, johnson);
        forget(johnson, ch);

        if (GET_QUEST(ch) == QST_MAGE_INTRO && GET_TRADITION(ch) != TRAD_MUNDANE)
          send_to_char(ch, "^M(OOC):^n You've discovered a follow-on quest! Follow the hint %s gave you to continue.\r\n", GET_CHAR_NAME(johnson));
      } else
        do_say(johnson, "You haven't completed any of your objectives yet.", 0, 0);

      return TRUE;
    case CMD_JOB_START: {

      // Reject high-rep characters.
      unsigned int johnson_max_rep = get_johnson_overall_max_rep(johnson);
      if (johnson_max_rep < GET_REP(ch)) {
        do_say(johnson, "My jobs aren't high-profile enough for someone with your rep!", 0, 0);
        send_to_char(ch, "[OOC: This Johnson caps out at %d reputation, so you won't get any further work from them.]\r\n", johnson_max_rep);

        GET_SPARE1(johnson) = -1;
        if (memory(johnson, ch))
            forget(johnson, ch);
        return TRUE;
      }

      new_q = new_quest(johnson, ch);
      //Clever hack to safely save us a call to new_quest() that compiler will be ok with.
      //If we have a cached quest use that and reset the cache integer back to -2 when
      //it is consumed.
      cached_new_q = new_q;

      //Handle out of quests and broken johnsons.
      //Calls to new_quest() return 0 when there's no quest left available and
      //-1 if the johnson is broken and has no quests.
      if (new_q == 0) {
          do_say(johnson, "I got nothing for you now. Come back later.", 0, 0);
          return TRUE;
      }
      if (new_q == -1) {
          return TRUE;
      }

      // Precondition: I cannot be talking right now.
      if (GET_SPARE1(johnson) == 0) {
        if (!memory(johnson, ch)) {
          do_say(johnson, "Hold on, I'm talking to someone else right now.", 0, 0);
          return TRUE;
        } else {
          do_say(johnson, "I'm lookin' for a yes-or-no answer, chummer.", 0, 0);
          return TRUE;
        }
      }

      // Precondition: You may not have an active quest.
      if (GET_QUEST(ch)) {
        do_say(johnson, "Maybe when you've finished what you're doing.", 0, 0);
        send_to_char("(OOC note: You're currently on another run. You can hit RECAP to see the details for it.)\r\n", ch);
        return TRUE;
      }

      // Precondition: You cannot be a flagged killer or a blacklisted character.
      if (PLR_FLAGGED(ch, PLR_KILLER) || PLR_FLAGGED(ch, PLR_BLACKLIST)) {
        do_say(johnson, "Word on the street is you can't be trusted.", 0, 0);
        GET_SPARE1(johnson) = -1;
        if (memory(johnson, ch))
          forget(johnson, ch);
      }

      // Reject low-rep characters.
      if (rep_too_low(ch, new_q)) {
        unsigned int johnson_min_rep = get_johnson_overall_min_rep(johnson);
        if (johnson_min_rep > GET_REP(ch)) {
          do_say(johnson, "You're not even worth my time right now.", 0, 0);
          send_to_char(ch, "[OOC: This Johnson has a minimum reputation requirement of %d. Come back when you have at least that much rep.]\r\n", johnson_min_rep);
        } else {
            int rep_delta = quest_table[new_q].min_rep - GET_REP(ch);
            if (rep_delta >= 1000)
                do_say(johnson, "Who are you?", 0, 0);
            else if  (rep_delta >= 500)
                do_say(johnson, "Don't talk to me.", 0, 0);
            else if  (rep_delta >= 200)
                do_say(johnson, "Go pick up a few new tricks.", 0, 0);
            else if  (rep_delta >= 100)
                do_say(johnson, "Wet behind the ears, huh?", 0, 0);
            else if  (rep_delta >= 20)
                do_say(johnson, "Come back later, omae.", 0, 0);
            else
                do_say(johnson, "I might have something for you soon.", 0, 0);
        }

        GET_SPARE1(johnson) = -1;
        if (memory(johnson, ch))
          forget(johnson, ch);
        return TRUE;
      }

      // Assign the quest.
      GET_SPARE1(johnson) = 0;
      if (quest_table[new_q].intro)
        do_say(johnson, quest_table[new_q].intro, 0, 0);
      else {
        snprintf(buf, sizeof(buf), "WARNING: Null string in quest %ld!", quest_table[new_q].vnum);
        mudlog(buf, ch, LOG_SYSLOG, TRUE);
        do_say(johnson, "I've got a job for you.", 0, 0);
      }
      do_say(johnson, "Are you interested?", 0, 0);
      if (!memory(johnson, ch))
        remember(johnson, ch);

      return TRUE;
    }
    case CMD_JOB_YES:
      // Precondition: If I'm not talking right now, don't react.
      if (GET_SPARE1(johnson) == -1) {
        return TRUE;
      }

      // Precondition: If I have no memory of you, dismiss you.
      if (!memory(johnson, ch)) {
        do_say(johnson, "Hold on, I'm talking to someone else right now.", 0, 0);
        return TRUE;
      }
      //Clever hack to safely save us a call to new_quest() that compiler will be ok with.
      //If we have a cached quest use that and reset the cache integer back to -2 when
      //it is consumed.
      if (cached_new_q != -2) {
        new_q = cached_new_q;
        cached_new_q = -2;
      } else {
        new_q = new_quest(johnson, ch);
      }

      //Handle out of quests and broken johnsons.
      //Calls to new_quest() return 0 when there's no quest left available and
      //-1 if the johnson is broken and has no quests.
      if (new_q == 0) {
        //This is should never really happen but if it does let's log it.
        snprintf(buf, sizeof(buf), "WARNING: Quest vanished between offered and accepted from %s (%ld) due.", GET_NAME(johnson), GET_MOB_VNUM(johnson));
        mudlog(buf, NULL, LOG_SYSLOG, true);
        do_say(johnson, "What are you talking about?", 0, 0);
        return TRUE;
      }
      if (new_q == -1) {
        return TRUE;
      }

      // Precondition: You may not have an active quest.
      if (GET_QUEST(ch)) {
        // If it's the same quest, just bail out without a message.
        if (GET_QUEST(ch) == new_q)
          return TRUE;

        do_say(johnson, "Maybe when you've finished what you're doing.", 0, 0);
        send_to_char("(OOC note: You're currently on another run. You can hit RECAP to see the details for it.)\r\n", ch);
        return TRUE;
      }

      // Assign them the quest since they've accepted it.
      GET_QUEST(ch) = new_q;
      ch->player_specials->obj_complete =
        new sh_int[quest_table[GET_QUEST(ch)].num_objs];
      ch->player_specials->mob_complete =
        new sh_int[quest_table[GET_QUEST(ch)].num_mobs];
      for (num = 0; num < quest_table[GET_QUEST(ch)].num_objs; num++)
        ch->player_specials->obj_complete[num] = 0;
      for (num = 0; num < quest_table[GET_QUEST(ch)].num_mobs; num++)
        ch->player_specials->mob_complete[num] = 0;

      // Load up the quest's targets.
      load_quest_targets(johnson, ch);

      // Go into my spiel.
      handle_info(johnson, new_q);

      return TRUE;

    case CMD_JOB_NO:
      // Precondition: If I'm not talking right now, don't react.
      if (GET_SPARE1(johnson) == -1) {
        return TRUE;
      }

      // Precondition: If I have no memory of you, dismiss you.
      if (!memory(johnson, ch)) {
        do_say(johnson, "Hold on, I'm talking to someone else right now.", 0, 0);
        return TRUE;
      }

      //Clever hack to safely save us a call to new_quest() that compiler will be ok with.
      //If we have a cached quest use that and reset the cache integer back to -2 when
      //it is consumed.
      if (cached_new_q != -2) {
        new_q = cached_new_q;
        cached_new_q = -2;
      } else {
        new_q = new_quest(johnson, ch);
      }

      //Handle out of quests and broken johnsons.
      //Calls to new_quest() return 0 when there's no quest left available and
      //-1 if the johnson is broken and has no quests.
      if (new_q == 0) {
        //This is should never really happen but if it does let's log it.
        snprintf(buf, sizeof(buf), "WARNING: Quest vanished between offered and declined from %s (%ld) due.", GET_NAME(johnson), GET_MOB_VNUM(johnson));
        mudlog(buf, NULL, LOG_SYSLOG, true);
        do_say(johnson, "What are you talking about?", 0, 0);
        return TRUE;
      }
      if (new_q == -1) {
        return TRUE;
      }

      // Decline the quest.
      GET_SPARE1(johnson) = -1;
      GET_QUEST(ch) = 0;
      forget(johnson, ch);
      if (quest_table[new_q].decline)
        do_say(johnson, quest_table[new_q].decline, 0, 0);
      else {
        snprintf(buf, sizeof(buf), "WARNING: Null string in quest %ld!", quest_table[new_q].vnum);
        mudlog(buf, ch, LOG_SYSLOG, TRUE);
        do_say(johnson, "Fine.", 0, 0);
      }
      return TRUE;
    default:
      new_q = new_quest(johnson, ch);
      do_say(johnson, "Ugh, drank too much last night. Talk to me later when I've sobered up.", 0, 0);
      snprintf(buf, sizeof(buf), "WARNING: Failed to evaluate Johnson tree and return successful message for Johnson '%s' (%ld). Values: comm = %d, spare1 = %ld, quest = %d (maps to %ld)",
              GET_NAME(johnson), GET_MOB_VNUM(johnson), comm, GET_SPARE1(johnson), new_q, quest_table[new_q].vnum);
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      break;
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

  for ( i = 0; i <= top_of_questt;  i++ ) {
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
      char_to_room( johnson, &world[quest_table[i].s_room] );
    }
    /* Needs to head off */
    else if ( rend || quest_table[i].e_time < time_info.hours ) {
      if ( quest_table[i].e_string != NULL )
        strcpy( buf, quest_table[i].e_string );
      for ( johnson = character_list; johnson != NULL; johnson = johnson->next ) {
        if ( johnson->nr == (tmp = read_mobile( quest_table[i].johnson, REAL))->nr )
          break;
      }
      if ( johnson != NULL && johnson->in_room) {
        MOB_FLAGS(johnson).SetBit(MOB_ISNPC);
        char_from_room( johnson );
        char_to_room( johnson, 0 );
        extract_char(johnson);
      }
      if ( tmp != NULL && tmp->in_room) {
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

  for (i = 0; i <= top_of_questt; i++) {
    if ((rnum = real_mobile(quest_table[i].johnson)) < 0)
      log_vfprintf("Johnson #%d does not exist (quest #%d)",
          quest_table[i].johnson, quest_table[i].vnum);
    else if (mob_index[rnum].func != johnson && mob_index[rnum].sfunc != johnson) {
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

    snprintf(buf, sizeof(buf), "Vnum: [%5ld], Rnum: [%ld], Johnson: [%s (%ld)]\r\n",
            quest_table[rnum].vnum, rnum,
            johnson < 0 ? "none" : GET_NAME(mob_proto+johnson),
            quest_table[rnum].johnson);
  }

  if (!access_level(ch, LVL_ADMIN))
  {
    send_to_char(buf, ch);
    return;
  }

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Time allowed: [%d], Minimum reputation: [%d], "
          "Maximum reputation: [%d]\r\n", quest_table[rnum].time,
          quest_table[rnum].min_rep, quest_table[rnum].max_rep);

    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Bonus nuyen: [%d], Bonus Karma: [%0.2f], "
          "Reward: [%d]\r\n", quest_table[rnum].nuyen,
          ((float)quest_table[rnum].karma / 100), quest_table[rnum].reward);

  for (i = 0; i < quest_table[rnum].num_mobs; i++) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "M%2d) %d nuyen/%0.2f: V%ld; %s (%d); %s (%d)\r\n",
            i, quest_table[rnum].mob[i].nuyen,
            ((float)quest_table[rnum].mob[i].karma / 100),
            quest_table[rnum].mob[i].vnum, sml[(int)quest_table[rnum].mob[i].load],
            quest_table[rnum].mob[i].l_data,
            smo[(int)quest_table[rnum].mob[i].objective],
            quest_table[rnum].mob[i].o_data);
  }


  for (i = 0; i < quest_table[rnum].num_objs; i++) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "O%2d) %d nuyen/%0.2f: V%ld; %s (%d/%d); %s (%d)\r\n",
            i, quest_table[rnum].obj[i].nuyen,
            ((float)quest_table[rnum].obj[i].karma / 100),
            quest_table[rnum].obj[i].vnum, sol[(int)quest_table[rnum].obj[i].load],
            quest_table[rnum].obj[i].l_data, quest_table[rnum].obj[i].l_data2,
            soo[(int)quest_table[rnum].obj[i].objective],
            quest_table[rnum].obj[i].o_data);
  }

  page_string(ch->desc, buf, 1);
}

void boot_one_quest(struct quest_data *quest)
{
  int count, quest_nr = -1, i;

  if ((top_of_questt + 2) >= top_of_quest_array)
    // if it cannot resize, return...the edit_quest is freed later
    if (!resize_qst_array())
    {
      olc_state = 0;
      return;
    }

  for (count = 0; count <= top_of_questt; count++)
    if (quest_table[count].vnum > quest->vnum)
    {
      quest_nr = count;
      break;
    }

  if (quest_nr == -1)
    quest_nr = top_of_questt + 1;
  else
    for (count = top_of_questt + 1; count > quest_nr; count--)
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
  quest_table[quest_nr].s_string = str_dup(quest->s_string);
  quest_table[quest_nr].e_string = str_dup(quest->e_string);
#ifdef USE_QUEST_LOCATION_CODE
  quest_table[quest_nr].location = str_dup(quest->location);
#endif

  if ((i = real_mobile(quest_table[quest_nr].johnson)) > 0 &&
      mob_index[i].func != johnson)
  {
    mob_index[i].sfunc = mob_index[i].func;
    mob_index[i].func = johnson;
    mob_proto[i].real_abils.attributes[QUI] = MAX(1, mob_proto[i].real_abils.attributes[QUI]);
  }
}

void reboot_quest(int rnum, struct quest_data *quest)
{
  int i, ojn, njn;

  if (quest_table[rnum].johnson != quest->johnson)
  {
    ojn = real_mobile(quest_table[rnum].johnson);
    njn = real_mobile(quest->johnson);
    if (njn < 0) {
      char oopsbuf[5000];
      snprintf(oopsbuf, sizeof(oopsbuf), "BUILD ERROR: Quest %ld has non-existent new Johnson %ld.", quest_table[rnum].vnum, quest->johnson);
      mudlog(oopsbuf, NULL, LOG_SYSLOG, TRUE);
      return;
    }

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

  if (quest_table[rnum].s_string)
    delete [] quest_table[rnum].s_string;
  quest_table[rnum].s_string = str_dup(quest->s_string);

  if (quest_table[rnum].e_string)
    delete [] quest_table[rnum].e_string;
  quest_table[rnum].e_string = str_dup(quest->e_string);

#ifdef USE_QUEST_LOCATION_CODE
  if (quest_table[rnum].location)
    delete [] quest_table[rnum].location;
  quest_table[rnum].location = str_dup(quest->location);
#endif
}

int write_quests_to_disk(int zone)
{
  long i, j, found = 0, counter;
  FILE *fp;
  zone = real_zone(zone);
  bool wrote_something = FALSE;

  snprintf(buf, sizeof(buf), "world/qst/%d.qst", zone_table[zone].number);

  if (!(fp = fopen(buf, "w+"))) {
    log_vfprintf("SYSERR: could not open file %d.qst", zone_table[zone].number);

    fclose(fp);
    return 0;
  }

  for (counter = zone_table[zone].number * 100;
       counter <= zone_table[zone].top; counter++) {
    if ((i = real_quest(counter)) > -1) {
      wrote_something = TRUE;
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
#ifdef USE_QUEST_LOCATION_CODE
      fprintf(fp, "%s~\n", cleanup(buf2, quest_table[i].location));
#endif
    }
  }
  fprintf(fp, "$~\n");
  fclose(fp);

  // If we wrote anything for this zone, update the index file.
  if (wrote_something) {
    fp = fopen("world/qst/index", "w+");

    for (i = 0; i <= top_of_zone_table; ++i) {
      found = 0;
      for (j = 0; !found && j <= top_of_questt; j++)
        if (quest_table[j].vnum >= (zone_table[i].number * 100) &&
            quest_table[j].vnum <= zone_table[i].top) {
          found = 1;
          fprintf(fp, "%d.qst\n", zone_table[i].number);
        }
    }

    fprintf(fp, "$~\n");
    fclose(fp);
  }
  // Otherwise, delete the empty junk file.
  else
    remove(buf);

  return 1;
}

void qedit_list_obj_objectives(struct descriptor_data *d)
{
  int i, real_obj;
  long rnum;

  CLS(CH);

  *buf = '\0';

  for (i = 0; i < QUEST->num_objs; i++)
  {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%2d) ", i);
    switch (QUEST->obj[i].load) {
    case QUEST_NONE:
      strlcat(buf, "Load nothing", sizeof(buf));
      break;
    case QOL_JOHNSON:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Give %ld to Johnson", QUEST->obj[i].vnum);
      break;
    case QOL_TARMOB_I:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Add %ld to inventory of M%d ", QUEST->obj[i].vnum,
              QUEST->obj[i].l_data);
      if (QUEST->obj[i].l_data >= 0 &&
          QUEST->obj[i].l_data < QUEST->num_mobs &&
          (rnum = real_mobile(QUEST->mob[QUEST->obj[i].l_data].vnum)) > -1) {
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "(%s)", GET_NAME(mob_proto+rnum));
      }
      else
        strlcat(buf, "(null)", sizeof(buf));
      strlcat(buf, "(NOT A DELIVERY OBJECTIVE)", sizeof(buf));
      break;

    case QOL_TARMOB_E:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Equip M%d ", QUEST->obj[i].l_data);

      if (QUEST->obj[i].l_data >= 0 &&
          QUEST->obj[i].l_data < QUEST->num_mobs &&
          (rnum = real_mobile(QUEST->mob[QUEST->obj[i].l_data].vnum)) > -1) {
                    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "(%s) ",
                GET_NAME(mob_proto+rnum));
      }

      strlcat(buf, "(null) ", sizeof(buf));
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "with %ld at %s", QUEST->obj[i].vnum,
              wear_bits[QUEST->obj[i].l_data2]);
      break;

    case QOL_TARMOB_C:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Install %ld in M%d ", QUEST->obj[i].vnum,
              QUEST->obj[i].l_data);

      if (QUEST->obj[i].l_data >= 0 &&
          QUEST->obj[i].l_data < QUEST->num_mobs &&
          (rnum = real_mobile(QUEST->mob[QUEST->obj[i].l_data].vnum)) > -1) {
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "(%s)", GET_NAME(mob_proto+rnum));
      }
      else
        strlcat(buf, "(null)", sizeof(buf));

      break;
    case QOL_HOST:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Load %ld in host %d", QUEST->obj[i].vnum, QUEST->obj[i].l_data);
      break;
    case QOL_LOCATION:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Load %ld in room %d", QUEST->obj[i].vnum,
              QUEST->obj[i].l_data);
      break;
    }
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n    Award %d nuyen & %0.2f karma for ",
            QUEST->obj[i].nuyen, ((float)QUEST->obj[i].karma / 100));
    switch (QUEST->obj[i].objective) {
    case QUEST_NONE:
      strlcat(buf, "nothing\r\n", sizeof(buf));
      break;
    case QOO_JOHNSON:
      strlcat(buf, "returning item to Johnson\r\n", sizeof(buf));
      break;
    case QOO_TAR_MOB:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "delivering item %ld (%s) to M%d ",
              QUEST->obj[i].vnum,
              (real_obj = real_object(QUEST->obj[i].vnum)) >= 0 ? GET_OBJ_NAME(&obj_proto[real_obj]) : "N/A",
              QUEST->obj[i].o_data);
      if (QUEST->obj[i].o_data >= 0 &&
          QUEST->obj[i].o_data < QUEST->num_mobs &&
          (rnum = real_mobile(QUEST->mob[QUEST->obj[i].o_data].vnum)) > -1) {
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "(%s)\r\n", GET_NAME(mob_proto+rnum));
      }
      else
        strlcat(buf, "(null)\r\n", sizeof(buf));
      break;
    case QOO_LOCATION:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "delivering item to room %d\r\n",
              QUEST->obj[i].o_data);
      break;
    case QOO_DSTRY_ONE:
      strlcat(buf, "destroying item\r\n", sizeof(buf));
      break;
    case QOO_DSTRY_MANY:
      strlcat(buf, "each item destroyed\r\n", sizeof(buf));
      break;
    case QOO_UPLOAD:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "uploading to host %d\n\n", QUEST->obj[i].o_data);
      break;
    case QOO_RETURN_PAY:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "returning paydata from host %d\r\n",
              QUEST->obj[i].o_data);
    }
  }
  send_to_char(buf, CH);
}

void qedit_list_mob_objectives(struct descriptor_data *d)
{
  int i, rnum = 0, real_mob;

  CLS(CH);

  *buf = '\0';

  for (i = 0; i < QUEST->num_mobs; i++)
  {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%2d) ", i);
    switch (QUEST->mob[i].load) {
    case QUEST_NONE:
      strlcat(buf, "Not set", sizeof(buf));
      break;
    case QML_LOCATION:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Load %ld (%s) at room %d",
              QUEST->mob[i].vnum,
              (rnum = real_mobile(QUEST->mob[i].vnum)) > -1 ?
              GET_NAME(mob_proto+rnum) : "null",
              QUEST->mob[i].l_data);
      break;
    case QML_FOLQUESTER:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Load %ld (%s) and follow quester",
              QUEST->mob[i].vnum,
              (rnum = real_mobile(QUEST->mob[i].vnum)) > -1 ?
              GET_NAME(mob_proto+rnum) : "null");
      break;
    }
    switch (QUEST->mob[i].objective) {
    case QUEST_NONE:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n    Award %d nuyen & %0.2f karma for "
              "nothing\r\n", QUEST->mob[i].nuyen,
              ((float)QUEST->mob[i].karma / 100));
      break;
    case QMO_LOCATION:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n    Award %d nuyen & %0.2f karma for "
              "escorting target to room %d\r\n", QUEST->mob[i].nuyen,
              ((float)QUEST->mob[i].karma / 100), QUEST->mob[i].o_data);
      break;
    case QMO_KILL_ONE:
            if ((real_mob = real_mobile(QUEST->mob[i].vnum)) >= 0)
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n    Award %d nuyen & %0.2f karma for "
                "killing target '%s' (%ld)\r\n", QUEST->mob[i].nuyen,
                ((float)QUEST->mob[i].karma / 100),
                GET_CHAR_NAME(&mob_proto[real_mob]),
                QUEST->mob[i].vnum);
      break;
    case QMO_KILL_MANY:
            if ((real_mob = real_mobile(QUEST->mob[i].vnum)) >= 0)
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n    Award %d nuyen & %0.2f karma for "
                "each target '%s' (%ld) killed\r\n", QUEST->mob[i].nuyen,
                ((float)QUEST->mob[i].karma / 100),
                GET_CHAR_NAME(&mob_proto[real_mob]),
                QUEST->mob[i].vnum);
      break;
    case QMO_KILL_ESCORTEE:
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Target hunts M%d \r\n",
              QUEST->mob[i].o_data);
      if (QUEST->mob[i].o_data >= 0 &&
          QUEST->mob[i].o_data < QUEST->num_mobs &&
          (rnum = real_mobile(QUEST->mob[QUEST->mob[i].o_data].vnum)) > -1) {
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "(%s)\r\n", GET_NAME(mob_proto+rnum));
      } else
        strlcat(buf, "(null)\r\n", sizeof(buf));
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
#ifdef USE_QUEST_LOCATION_CODE
  send_to_char(CH, "0) Location text:      %s%s%s\r\n", CCCYN(CH, C_CMP),
               QUEST->decline, CCNRM(CH, C_CMP));
#endif
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
    snprintf(s_time, sizeof(s_time), "%d - ", QUEST->s_time);
    snprintf(e_time, sizeof(e_time), "%d", QUEST->e_time);
  }
  send_to_char(CH, "d) Johnson hours: %s%s%s%s\r\n", CCCYN(CH, C_CMP),
               s_time, e_time, CCNRM(CH, C_CMP));

  send_to_char(CH, "e) Start work message: %s%s%s\r\n", CCCYN(CH, C_CMP),
               QUEST->s_string, CCNRM(CH, C_CMP));
  send_to_char(CH, "f) End work message: %s%s%s\r\n", CCCYN(CH, C_CMP),
               QUEST->e_string, CCNRM(CH, C_CMP));
  send_to_char(CH, "g) Quest already completed message: %s%s%s\r\n", CCCYN(CH, C_CMP),
               QUEST->done, CCNRM(CH, C_CMP));

  if (access_level(CH, LVL_VICEPRES)) {
    int real_obj;
    send_to_char(CH, "h) Item Reward: %s%d%s (%s%s%s)\r\n", CCCYN(CH, C_CMP),
                 QUEST->reward, CCNRM(CH, C_CMP), CCCYN(CH, C_CMP),
                 (real_obj = real_object(QUEST->reward)) <= 0 ? "no item reward" : obj_proto[real_obj].text.name,
                 CCNRM(CH, C_CMP));
  }
  send_to_char("q) Quit and save\r\n", CH);
  send_to_char("x) Exit and abort\r\n", CH);
  send_to_char("Enter your choice:\r\n", CH);
  d->edit_mode = QEDIT_MAIN_MENU;
}

void qedit_parse(struct descriptor_data *d, const char *arg)
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
#ifdef ONLY_LOG_BUILD_ACTIONS_ON_CONNECTED_ZONES
      if (!vnum_from_non_connected_zone(d->edit_number)) {
#else
      {
#endif
        snprintf(buf, sizeof(buf),"%s wrote new quest #%ld",
                GET_CHAR_NAME(d->character), d->edit_number);
        mudlog(buf, d->character, LOG_WIZLOG, TRUE);
      }

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
      send_to_char("Enter Johnson's vnum: ", CH);
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
#ifdef USE_QUEST_LOCATION_CODE
    case '0':
      send_to_char("Enter a description of the Johnson's location (ex: 'a booth on the second level of Dante's Inferno'): ", d->character);
      d->edit_mode = QEDIT_LOCATION;
      break;
#endif
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
      DELETE_D_STR_IF_EXTANT(d);
      INITIALIZE_NEW_D_STR(d);
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      break;
    case 'd':
    case 'D':
      send_to_char("Enter hour for Johnson to start giving jobs: ", CH);
      d->edit_mode = QEDIT_SHOUR;
      break;
    case 'e':
    case 'E':
      send_to_char("Enter the string that will be given when the Johnson comes to work:\r\n", CH);
      d->edit_mode = QEDIT_SSTRING;
      break;
    case 'f':
    case 'F':
      send_to_char("Enter the string that will be given when the Johnson leaves work:\r\n", CH);
      d->edit_mode = QEDIT_ESTRING;
      break;
    case 'g':
    case 'G':
      send_to_char("Enter the string that will be given if quest is already complete:\r\n", CH);
      d->edit_mode = QEDIT_DONE;
      break;
    case 'h':
    case 'H':
      if (!access_level(CH, LVL_VICEPRES))
        qedit_disp_menu(d);
      else {
        send_to_char("Enter vnum of reward (-1 for nothing): ", CH);
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
      send_to_char("No such mob!  Enter Johnson's vnum: ", CH);
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
      send_to_char("Invalid value.  Enter minimum reputation between 0-500: ", CH);
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
        send_to_char("Enter vnum of item (0 for nothing): ", CH);
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
      send_to_char("Enter vnum of item (0 for nothing): ", CH);
    }
    break;
  case QEDIT_O_VNUM:
    number = atoi(arg);
    if (number != 0 && real_object(number) < 0)
      send_to_char("No such item.  Enter vnum of item (0 for nothing): ", CH);
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
        QUEST->obj[d->edit_number2].vnum = OBJ_BLANK_OPTICAL_CHIP;
        // Fallthrough.
      case QOO_UPLOAD:
        d->edit_mode = QEDIT_O_ODATA;
        send_to_char(CH, "Enter vnum of host paydata must be retrieved/uploaded from: ");
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
        send_to_char(CH, "That's not a valid room. Enter vnum of room item must be delivered to: ");
      else {
        QUEST->obj[d->edit_number2].o_data = number;
        CLS(CH);
        qedit_disp_obj_menu(d);
      }
      break;
    case QOO_UPLOAD:
    case QOO_RETURN_PAY:
      if (real_host(number) < 0)
        send_to_char(CH, "That's not a valid host. Enter vnum of host paydata must be retrieved/uploaded from: ");
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
#ifdef USE_QUEST_LOCATION_CODE
  case QEDIT_LOCATION:
    if (QUEST->location)
      delete [] QUEST->location;
    QUEST->location = str_dup(arg);
    qedit_disp_menu(d);
    break;
#endif
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
    if (real_object(number) < -1)
      send_to_char(CH, "Invalid vnum.  Enter vnum of reward (-1 for nothing): ");
    else {
      QUEST->reward = number;
      qedit_disp_menu(d);
    }
    break;
  case QEDIT_SHOUR:
    number = atoi(arg);
    if ( number > 23 || number < -1 ) {
      send_to_char("Needs to be between -1 and 23.\r\nWhat time does he start work? ", CH);
    } else {
      QUEST->s_time = number;
      d->edit_mode = QEDIT_EHOUR;
      send_to_char("Enter hour for Johnson to stop giving jobs: ", CH);
    }
    break;
  case QEDIT_EHOUR:
    number = atoi(arg);
    if ( number > 23 || number < 0 ) {
      send_to_char("Needs to be between 0 and 23.\r\nWhat time does he stop work? ", CH);
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

// Remotely end a run. Requires a phone.
ACMD(do_endrun) {
  struct obj_data *phone = NULL;

  // Must be on a quest.
  FAILURE_CASE(!GET_QUEST(ch), "But you're not on a run.");

  // Must have a phone.
  for (phone = ch->carrying; phone; phone = phone->next_content)
    if (GET_OBJ_TYPE(phone) == ITEM_PHONE)
      break;
  // Worn phones are OK.
  if (!phone)
    for (int x = 0; !phone && x < NUM_WEARS; x++)
      if (GET_EQ(ch, x) && GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_PHONE)
        phone = GET_EQ(ch, x);
  // Cyberware phones are fine.
  if (!phone)
    for (phone = ch->cyberware; phone; phone = phone->next_content)
      if (GET_OBJ_VAL(phone, 0) == CYB_PHONE)
        break;
  // No phone? Go away.
  FAILURE_CASE(!phone, "How do you expect to contact your Johnson without a phone?");

  // Drop the quest.
  for (struct char_data *johnson = character_list; johnson; johnson = johnson->next) {
    if (IS_NPC(johnson) && (GET_MOB_VNUM(johnson) == quest_table[GET_QUEST(ch)].johnson)) {
      send_to_char(ch, "You call your Johnson, and after a short wait the phone is picked up.\r\n"
                       "^Y%s on the other end of the line says, \"%s\"^n\r\n"
                       "With your run abandoned, you hang up the phone.\r\n",
                       GET_CHAR_NAME(johnson),
                       quest_table[GET_QUEST(ch)].quit);
      if (ch->in_room)
        act("$n makes a brief phone call to $s Johnson to quit $s current run. Scandalous.", FALSE, ch, 0, 0, TO_ROOM);
      snprintf(buf, sizeof(buf), "$z's phone rings. $e answers, listens for a moment, then says into it, \"%s\"", quest_table[GET_QUEST(ch)].quit);
      act(buf, FALSE, johnson, NULL, NULL, TO_ROOM);

      end_quest(ch);
      forget(johnson, ch);
      return;
    }
  }

  // Error case.
  mudlog("SYSERR: Attempted remote job termination, but the Johnson could not be found!", ch, LOG_SYSLOG, TRUE);
  send_to_char("You dial your phone, but something's up with the connection, and you can't get through.\r\n", ch);
}

unsigned int get_johnson_overall_max_rep(struct char_data *johnson) {
  unsigned int max_rep = 0;

  bool johnson_is_from_disconnected_zone = vnum_from_non_connected_zone(GET_MOB_VNUM(johnson));

  for (int i = 0; i <= top_of_questt; i++) {
    if (quest_table[i].johnson == GET_MOB_VNUM(johnson)
        && (johnson_is_from_disconnected_zone
            || !vnum_from_non_connected_zone(quest_table[i].vnum)))
    {
      max_rep = MAX(max_rep, quest_table[i].max_rep);
    }
  }

  return max_rep;
}

unsigned int get_johnson_overall_min_rep(struct char_data *johnson) {
  unsigned int min_rep = UINT_MAX;

  bool johnson_is_from_disconnected_zone = vnum_from_non_connected_zone(GET_MOB_VNUM(johnson));

  for (int i = 0; i <= top_of_questt; i++) {
    if (quest_table[i].johnson == GET_MOB_VNUM(johnson)
        && (johnson_is_from_disconnected_zone
            || !vnum_from_non_connected_zone(quest_table[i].vnum)))
    {
      min_rep = MIN(min_rep, quest_table[i].min_rep);
    }
  }

  return min_rep;
}
