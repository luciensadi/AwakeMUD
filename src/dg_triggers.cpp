/**************************************************************************
*  File: dg_triggers.cpp                                                  *
*  Usage: The checks that decide whether a trigger fires, and what        *
*         variables it starts with.                                       *
*                                                                         *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
*                                                                         *
*  $Author: Mark A. Heilpern/egreen/Welcor $                              *
*  $Date: 2004/10/11 12:07:00$                                            *
*  $Revision: 1.0.14 $                                                    *
*                                                                         *
*  Ported to AwakeMUD CE by Fizban.                                       *
*                                                                         *
*  Where stock DG checks its script_players config option before letting  *
*  an actor trip its own trigger, this port simply never does: a player   *
*  has no triggers to trip.                                               *
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
#include "constants.hpp"
#include "dg_scripts.hpp"
#include "dg_event.hpp"

extern struct time_info_data time_info;
extern const char *cmd_door[];

/* Has this character been killed out from under us? */
#define DG_DEAD(ch)  (!(ch) || GET_POS(ch) == POS_DEAD)

/* The name of a spell, for the cast triggers. */
static const char *dg_spell_name(int spellnum)
{
  if (spellnum > 0 && spellnum < MAX_SPELLS)
    return spells[spellnum].name;

  return "unknown";
}

/* The name of an attack type, for the damage trigger. */
static const char *dg_attack_name(int attacktype)
{
  if (attacktype >= TYPE_HIT && attacktype <= TYPE_BLACKIC)
    return damage_type_names_must_subtract_300_first_and_must_not_be_greater_than_blackic[attacktype - 300];

  return "unknown";
}

/* ************************************************************************
*  General helpers used by several triggers.                               *
************************************************************************ */

/* Copy the first phrase into first_arg, and return the rest of the string. */
char *one_phrase(char *arg, char *first_arg)
{
  skip_spaces(&arg);

  if (!*arg) {
    *first_arg = '\0';
  }

  else if (*arg == '"') {
    char *p, c;

    p = matching_quote(arg);
    c = *p;
    *p = '\0';
    strcpy(first_arg, arg + 1);
    if (c == '\0')
      return p;
    return p + 1;
  }

  else {
    char *s, *p;

    s = first_arg;
    p = arg;

    while (*p && !isspace(*p) && *p != '"')
      *s++ = *p++;

    *s = '\0';
    return p;
  }

  return arg;
}

/* Is sub a whole word or phrase within string? */
int is_substring(const char *sub, const char *string)
{
  char haystack[MAX_STRING_LENGTH];
  char *s;

  if (!sub || !string)
    return 0;

  strlcpy(haystack, string, sizeof(haystack));

  if ((s = dg_str_str(haystack, sub))) {
    int len = strlen(haystack);
    int sublen = strlen(sub);

    /* check the front */
    if ((s == haystack || isspace(*(s - 1)) || ispunct(*(s - 1))) &&
        /* check the end */
        ((s + sublen == haystack + len) || isspace(s[sublen]) || ispunct(s[sublen])))
      return 1;
  }

  return 0;
}

/* Return 1 if str contains a word or phrase from wordlist. Phrases go in
 * double quotes. A wordlist of "*" matches anything. */
int word_check(const char *str, const char *wordlist)
{
  char words[MAX_INPUT_LENGTH], phrase[MAX_INPUT_LENGTH], *s;

  if (!wordlist || !str)
    return 0;

  if (*wordlist == '*')
    return 1;

  strlcpy(words, wordlist, sizeof(words));

  for (s = one_phrase(words, phrase); *phrase; s = one_phrase(s, phrase))
    if (is_substring(phrase, str))
      return 1;

  return 0;
}

/* ************************************************************************
*  Mob triggers.                                                           *
************************************************************************ */

void random_mtrigger(struct char_data *ch)
{
  struct trig_data *t;

  /* Only called when someone is in the zone. */
  if (!SCRIPT_CHECK(ch, MTRIG_RANDOM) || DG_MOB_IS_PLAYER_DIRECTED(ch))
    return;

  for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
    if (TRIGGER_CHECK(t, MTRIG_RANDOM) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);
      break;
    }
  }
}

void bribe_mtrigger(struct char_data *ch, struct char_data *actor, int amount)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (!SCRIPT_CHECK(ch, MTRIG_BRIBE) || DG_MOB_IS_PLAYER_DIRECTED(ch))
    return;

  for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
    if (TRIGGER_CHECK(t, MTRIG_BRIBE) && (amount >= GET_TRIG_NARG(t))) {
      snprintf(buf, sizeof(buf), "%d", amount);
      add_var(&GET_TRIG_VARS(t), "amount", buf, 0);
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);
      break;
    }
  }
}

/* Drop one memory entry from a mob's list and free it. */
static void forget_one_memory(struct char_data *ch, struct script_memory *mem)
{
  if (SCRIPT_MEM(ch) == mem) {
    SCRIPT_MEM(ch) = mem->next;
  } else {
    struct script_memory *prev = SCRIPT_MEM(ch);
    while (prev && prev->next != mem)
      prev = prev->next;
    if (!prev)
      return;
    prev->next = mem->next;
  }

  DELETE_ARRAY_IF_EXTANT(mem->cmd);
  delete mem;
}

void greet_memory_mtrigger(struct char_data *actor)
{
  struct trig_data *t;
  struct char_data *ch, *next_ch;
  struct script_memory *mem, *next_mem;
  char buf[MAX_INPUT_LENGTH];

  if (!valid_dg_target(actor, DG_ALLOW_GODS) || !actor->in_room)
    return;

  for (ch = actor->in_room->people; ch; ch = next_ch) {
    next_ch = ch->next_in_room;

    if (!SCRIPT_MEM(ch) || !AWAKE(ch) || FIGHTING(ch) || (ch == actor) ||
        DG_MOB_IS_PLAYER_DIRECTED(ch))
      continue;

    for (mem = SCRIPT_MEM(ch); mem && SCRIPT_MEM(ch); mem = next_mem) {
      next_mem = mem->next;

      if (char_script_id(actor) != mem->id)
        continue;

      if (mem->cmd) {
        command_interpreter(ch, mem->cmd, GET_CHAR_NAME(ch)); /* no script */
      } else if (SCRIPT(ch)) {
        for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
          if (IS_SET(GET_TRIG_TYPE(t), MTRIG_MEMORY) && CAN_SEE(ch, actor) &&
              !GET_TRIG_DEPTH(t) && number(1, 100) <= GET_TRIG_NARG(t)) {
            ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
            script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);
            break;
          }
        }
      }

      forget_one_memory(ch, mem);
    }
  }
}

int greet_mtrigger(struct char_data *actor, int dir)
{
  struct trig_data *t, *next_trig;
  struct char_data *ch, *next_ch;
  char buf[MAX_INPUT_LENGTH];
  int intermediate, final = TRUE;

  if (!valid_dg_target(actor, DG_ALLOW_GODS) || !actor->in_room)
    return TRUE;

  for (ch = actor->in_room->people; ch; ch = next_ch) {
    next_ch = ch->next_in_room;

    if (!SCRIPT_CHECK(ch, MTRIG_GREET | MTRIG_GREET_ALL) ||
        !AWAKE(ch) || FIGHTING(ch) || (ch == actor) ||
        DG_MOB_IS_PLAYER_DIRECTED(ch))
      continue;

    for (t = TRIGGERS(SCRIPT(ch)); t; t = next_trig) {
      next_trig = t->next;

      if (((IS_SET(GET_TRIG_TYPE(t), MTRIG_GREET) && CAN_SEE(ch, actor)) ||
           IS_SET(GET_TRIG_TYPE(t), MTRIG_GREET_ALL)) &&
          !GET_TRIG_DEPTH(t) && (number(1, 100) <= GET_TRIG_NARG(t))) {
        if (dir >= 0 && dir < NUM_OF_DIRS)
          add_var(&GET_TRIG_VARS(t), "direction", dirs[rev_dir[dir]], 0);
        else
          add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
        ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
        intermediate = script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);
        if (!intermediate)
          final = FALSE;
      }
    }
  }

  return final;
}

void entry_memory_mtrigger(struct char_data *ch)
{
  struct trig_data *t;
  struct char_data *actor, *next_actor;
  struct script_memory *mem, *next_mem;
  char buf[MAX_INPUT_LENGTH];

  if (!SCRIPT_MEM(ch) || DG_MOB_IS_PLAYER_DIRECTED(ch) || !ch->in_room)
    return;

  for (actor = ch->in_room->people; actor && SCRIPT_MEM(ch); actor = next_actor) {
    next_actor = actor->next_in_room;

    if (actor == ch)
      continue;

    for (mem = SCRIPT_MEM(ch); mem && SCRIPT_MEM(ch); mem = next_mem) {
      next_mem = mem->next;

      if (char_script_id(actor) != mem->id)
        continue;

      if (mem->cmd) {
        command_interpreter(ch, mem->cmd, GET_CHAR_NAME(ch));
      } else if (SCRIPT(ch)) {
        for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
          if (TRIGGER_CHECK(t, MTRIG_MEMORY) && (number(1, 100) <= GET_TRIG_NARG(t))) {
            ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
            script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);
            break;
          }
        }
      }

      forget_one_memory(ch, mem);
    }
  }
}

int entry_mtrigger(struct char_data *ch)
{
  struct trig_data *t;

  if (!SCRIPT_CHECK(ch, MTRIG_ENTRY) || DG_MOB_IS_PLAYER_DIRECTED(ch))
    return 1;

  for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next)
    if (TRIGGER_CHECK(t, MTRIG_ENTRY) && (number(1, 100) <= GET_TRIG_NARG(t)))
      return script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);

  return 1;
}

int command_mtrigger(struct char_data *actor, char *cmd, char *argument)
{
  struct char_data *ch, *ch_next;
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  /* don't let the people we like get trapped */
  if (!valid_dg_target(actor, 0) || !actor->in_room)
    return 0;

  for (ch = actor->in_room->people; ch; ch = ch_next) {
    ch_next = ch->next_in_room;

    if (!SCRIPT_CHECK(ch, MTRIG_COMMAND) || DG_MOB_IS_PLAYER_DIRECTED(ch) || actor == ch)
      continue;

    for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
      if (!TRIGGER_CHECK(t, MTRIG_COMMAND))
        continue;

      if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
        script_log("Command trigger #%ld has no text argument!", (long) GET_TRIG_VNUM(t));
        continue;
      }

      if (*GET_TRIG_ARG(t) == '*' ||
          !strncmp(GET_TRIG_ARG(t), cmd, strlen(GET_TRIG_ARG(t)))) {
        ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
        skip_spaces(&argument);
        add_var(&GET_TRIG_VARS(t), "arg", argument, 0);
        skip_spaces(&cmd);
        add_var(&GET_TRIG_VARS(t), "cmd", cmd, 0);

        if (script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW))
          return 1;
      }
    }
  }

  return 0;
}

void speech_mtrigger(struct char_data *actor, const char *str)
{
  struct char_data *ch, *ch_next;
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (!actor->in_room)
    return;

  for (ch = actor->in_room->people; ch; ch = ch_next) {
    ch_next = ch->next_in_room;

    if (!SCRIPT_CHECK(ch, MTRIG_SPEECH) || !AWAKE(ch) ||
        DG_MOB_IS_PLAYER_DIRECTED(ch) || actor == ch)
      continue;

    for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
      if (!TRIGGER_CHECK(t, MTRIG_SPEECH))
        continue;

      if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
        script_log("Speech trigger #%ld has no text argument!", (long) GET_TRIG_VNUM(t));
        continue;
      }

      if ((GET_TRIG_NARG(t) && word_check(str, GET_TRIG_ARG(t))) ||
          (!GET_TRIG_NARG(t) && is_substring(GET_TRIG_ARG(t), str))) {
        ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
        add_var(&GET_TRIG_VARS(t), "speech", str, 0);
        script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);
        break;
      }
    }
  }
}

void act_mtrigger(const struct char_data *ch, const char *str, struct char_data *actor,
                  struct char_data *victim, struct obj_data *object,
                  struct obj_data *target, const char *arg)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];
  struct char_data *mob = (struct char_data *) ch;

  if (!SCRIPT_CHECK(mob, MTRIG_ACT) || DG_MOB_IS_PLAYER_DIRECTED(mob) || actor == mob)
    return;

  for (t = TRIGGERS(SCRIPT(mob)); t; t = t->next) {
    if (!TRIGGER_CHECK(t, MTRIG_ACT))
      continue;

    if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
      script_log("Act trigger #%ld has no text argument!", (long) GET_TRIG_VNUM(t));
      continue;
    }

    if ((GET_TRIG_NARG(t) && word_check(str, GET_TRIG_ARG(t))) ||
        (!GET_TRIG_NARG(t) && is_substring(GET_TRIG_ARG(t), str))) {
      if (actor)
        ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      if (victim)
        ADD_UID_VAR(buf, t, char_script_id(victim), "victim", 0);
      if (object)
        ADD_UID_VAR(buf, t, obj_script_id(object), "object", 0);
      if (target)
        ADD_UID_VAR(buf, t, obj_script_id(target), "target", 0);
      if (str) {
        char nbuf[MAX_STRING_LENGTH], *nstr = nbuf, *p;
        strlcpy(nbuf, str, sizeof(nbuf));
        if ((p = strchr(nbuf, '\r')))
          *p = '\0';
        skip_spaces(&nstr);
        add_var(&GET_TRIG_VARS(t), "arg", nstr, 0);
      }
      script_driver(&mob, t, MOB_TRIGGER, TRIG_NEW);
      break;
    }
  }
}

void fight_mtrigger(struct char_data *ch)
{
  struct char_data *actor;
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (!SCRIPT_CHECK(ch, MTRIG_FIGHT) || !FIGHTING(ch) || DG_MOB_IS_PLAYER_DIRECTED(ch))
    return;

  for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
    if (TRIGGER_CHECK(t, MTRIG_FIGHT) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      actor = FIGHTING(ch);
      if (actor)
        ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      else
        add_var(&GET_TRIG_VARS(t), "actor", "nobody", 0);

      script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);
      break;
    }
  }
}

/* Named for the stock trigger; the percentage is of the physical condition
 * monitor rather than of hit points. */
void hitprcnt_mtrigger(struct char_data *ch)
{
  struct char_data *actor;
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (!SCRIPT_CHECK(ch, MTRIG_HITPRCNT) || !FIGHTING(ch) || DG_MOB_IS_PLAYER_DIRECTED(ch))
    return;

  for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
    if (TRIGGER_CHECK(t, MTRIG_HITPRCNT) && GET_MAX_PHYSICAL(ch) > 0 &&
        (((GET_PHYSICAL(ch) * 100) / GET_MAX_PHYSICAL(ch)) <= GET_TRIG_NARG(t))) {
      actor = FIGHTING(ch);
      if (actor)
        ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);
      break;
    }
  }
}

int receive_mtrigger(struct char_data *ch, struct char_data *actor, struct obj_data *obj)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];
  int ret_val;
  long object_id;

  if (!SCRIPT_CHECK(ch, MTRIG_RECEIVE) || DG_MOB_IS_PLAYER_DIRECTED(ch))
    return 1;

  object_id = obj_script_id(obj);

  for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
    if (TRIGGER_CHECK(t, MTRIG_RECEIVE) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      ADD_UID_VAR(buf, t, object_id, "object", 0);
      ret_val = script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);

      /* The script may have killed someone or purged the item. */
      if (DG_DEAD(actor) || DG_DEAD(ch) || !has_obj_by_uid_in_lookup_table(object_id) ||
          obj->carried_by != actor)
        return 0;

      return ret_val;
    }
  }

  return 1;
}

int death_mtrigger(struct char_data *ch, struct char_data *actor)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (!SCRIPT_CHECK(ch, MTRIG_DEATH) || DG_MOB_IS_PLAYER_DIRECTED(ch))
    return 1;

  for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
    if (TRIGGER_CHECK(t, MTRIG_DEATH) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      if (actor)
        ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      return script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);
    }
  }

  return 1;
}

void load_mtrigger(struct char_data *ch)
{
  struct trig_data *t;
  int result = 0;

  if (!SCRIPT_CHECK(ch, MTRIG_LOAD))
    return;

  for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
    if (TRIGGER_CHECK(t, MTRIG_LOAD) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      result = script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);
      break;
    }
  }

  if (result == SCRIPT_ERROR_CODE) {
    /* recursed beyond a reasonable depth: make sure this mob is the last one
     * in the load chain */
    if (GET_MOB_RNUM(ch) >= 0)
      free_proto_script(&mob_proto[GET_MOB_RNUM(ch)], MOB_TRIGGER);
  }
}

int cast_mtrigger(struct char_data *actor, struct char_data *ch, int spellnum)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (ch == NULL)
    return 1;

  if (!SCRIPT_CHECK(ch, MTRIG_CAST) || DG_MOB_IS_PLAYER_DIRECTED(ch))
    return 1;

  for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
    if (TRIGGER_CHECK(t, MTRIG_CAST) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      snprintf(buf, sizeof(buf), "%d", spellnum);
      add_var(&GET_TRIG_VARS(t), "spell", buf, 0);
      add_var(&GET_TRIG_VARS(t), "spellname", dg_spell_name(spellnum), 0);
      return script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);
    }
  }

  return 1;
}

/* Returns the damage the hit should do, so a trigger that never returns
 * leaves the incoming damage alone. */
int damage_mtrigger(struct char_data *actor, struct char_data *victim, int dam, int attacktype)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (victim == NULL)
    return dam;

  if (!SCRIPT_CHECK(victim, MTRIG_DAMAGE) || DG_MOB_IS_PLAYER_DIRECTED(victim))
    return dam;

  for (t = TRIGGERS(SCRIPT(victim)); t; t = t->next) {
    if (TRIGGER_CHECK(t, MTRIG_DAMAGE) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      if (actor)
        ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      ADD_UID_VAR(buf, t, char_script_id(victim), "victim", 0);
      snprintf(buf, sizeof(buf), "%d", dam);
      add_var(&GET_TRIG_VARS(t), "damdealt", buf, 0);
      add_var(&GET_TRIG_VARS(t), "attacktype", dg_attack_name(attacktype), 0);
      return script_driver_default(&victim, t, MOB_TRIGGER, TRIG_NEW, dam);
    }
  }

  return dam;
}

int leave_mtrigger(struct char_data *actor, int dir)
{
  struct trig_data *t;
  struct char_data *ch, *next_ch;
  char buf[MAX_INPUT_LENGTH];

  if (!valid_dg_target(actor, DG_ALLOW_GODS) || !actor->in_room)
    return 1;

  for (ch = actor->in_room->people; ch; ch = next_ch) {
    next_ch = ch->next_in_room;

    if (!SCRIPT_CHECK(ch, MTRIG_LEAVE) || !AWAKE(ch) || FIGHTING(ch) ||
        (ch == actor) || DG_MOB_IS_PLAYER_DIRECTED(ch))
      continue;

    for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
      if (IS_SET(GET_TRIG_TYPE(t), MTRIG_LEAVE) && CAN_SEE(ch, actor) &&
          !GET_TRIG_DEPTH(t) && (number(1, 100) <= GET_TRIG_NARG(t))) {
        if (dir >= 0 && dir < NUM_OF_DIRS)
          add_var(&GET_TRIG_VARS(t), "direction", dirs[dir], 0);
        else
          add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
        ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
        return script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);
      }
    }
  }

  return 1;
}

int door_mtrigger(struct char_data *actor, int subcmd, int dir)
{
  struct trig_data *t;
  struct char_data *ch, *next_ch;
  char buf[MAX_INPUT_LENGTH];

  if (!actor->in_room)
    return 1;

  for (ch = actor->in_room->people; ch; ch = next_ch) {
    next_ch = ch->next_in_room;

    if (!SCRIPT_CHECK(ch, MTRIG_DOOR) || !AWAKE(ch) || FIGHTING(ch) ||
        (ch == actor) || DG_MOB_IS_PLAYER_DIRECTED(ch))
      continue;

    for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
      if (IS_SET(GET_TRIG_TYPE(t), MTRIG_DOOR) && CAN_SEE(ch, actor) &&
          !GET_TRIG_DEPTH(t) && (number(1, 100) <= GET_TRIG_NARG(t))) {
        add_var(&GET_TRIG_VARS(t), "cmd", cmd_door[subcmd], 0);
        if (dir >= 0 && dir < NUM_OF_DIRS)
          add_var(&GET_TRIG_VARS(t), "direction", dirs[dir], 0);
        else
          add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
        ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
        return script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);
      }
    }
  }

  return 1;
}

void time_mtrigger(struct char_data *ch)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  /* fires when the hour matches the trigger's numeric argument */
  if (!SCRIPT_CHECK(ch, MTRIG_TIME) || DG_MOB_IS_PLAYER_DIRECTED(ch))
    return;

  for (t = TRIGGERS(SCRIPT(ch)); t; t = t->next) {
    if (TRIGGER_CHECK(t, MTRIG_TIME) && (time_info.hours == GET_TRIG_NARG(t))) {
      snprintf(buf, sizeof(buf), "%d", time_info.hours);
      add_var(&GET_TRIG_VARS(t), "time", buf, 0);
      script_driver(&ch, t, MOB_TRIGGER, TRIG_NEW);
      break;
    }
  }
}

/* ************************************************************************
*  Object triggers.                                                        *
************************************************************************ */

void random_otrigger(struct obj_data *obj)
{
  struct trig_data *t;

  if (!SCRIPT_CHECK(obj, OTRIG_RANDOM))
    return;

  for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
    if (TRIGGER_CHECK(t, OTRIG_RANDOM) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      script_driver(&obj, t, OBJ_TRIGGER, TRIG_NEW);
      break;
    }
  }
}

void timer_otrigger(struct obj_data *obj)
{
  struct trig_data *t;

  if (!SCRIPT_CHECK(obj, OTRIG_TIMER))
    return;

  for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next)
    if (TRIGGER_CHECK(t, OTRIG_TIMER))
      script_driver(&obj, t, OBJ_TRIGGER, TRIG_NEW);
}

int get_otrigger(struct obj_data *obj, struct char_data *actor)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];
  int ret_val;
  long object_id;

  if (!SCRIPT_CHECK(obj, OTRIG_GET))
    return 1;

  object_id = obj_script_id(obj);

  for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
    if (TRIGGER_CHECK(t, OTRIG_GET) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      ret_val = script_driver(&obj, t, OBJ_TRIGGER, TRIG_NEW);

      /* Refuse the get if the actor died or the object was purged: obj_to_char
       * would choke on either. */
      if (DG_DEAD(actor) || !obj || !has_obj_by_uid_in_lookup_table(object_id))
        return 0;

      return ret_val;
    }
  }

  return 1;
}

/* checks the command trigger on one object */
int cmd_otrig(struct obj_data *obj, struct char_data *actor, char *cmd,
              char *argument, int type)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (!obj || !SCRIPT_CHECK(obj, OTRIG_COMMAND))
    return 0;

  for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
    if (!TRIGGER_CHECK(t, OTRIG_COMMAND))
      continue;

    if (!IS_SET(GET_TRIG_NARG(t), type))
      continue;

    if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
      script_log("O-Command trigger #%ld has no text argument!", (long) GET_TRIG_VNUM(t));
      continue;
    }

    if (*GET_TRIG_ARG(t) == '*' ||
        !strncmp(GET_TRIG_ARG(t), cmd, strlen(GET_TRIG_ARG(t)))) {
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      skip_spaces(&argument);
      add_var(&GET_TRIG_VARS(t), "arg", argument, 0);
      skip_spaces(&cmd);
      add_var(&GET_TRIG_VARS(t), "cmd", cmd, 0);

      if (script_driver(&obj, t, OBJ_TRIGGER, TRIG_NEW))
        return 1;
    }
  }

  return 0;
}

int command_otrigger(struct char_data *actor, char *cmd, char *argument)
{
  struct obj_data *obj;
  int i;

  /* don't let the people we like get trapped */
  if (!valid_dg_target(actor, 0))
    return 0;

  for (i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(actor, i))
      if (cmd_otrig(GET_EQ(actor, i), actor, cmd, argument, OCMD_EQUIP))
        return 1;

  for (obj = actor->carrying; obj; obj = obj->next_content)
    if (cmd_otrig(obj, actor, cmd, argument, OCMD_INVEN))
      return 1;

  if (actor->in_room)
    for (obj = actor->in_room->contents; obj; obj = obj->next_content)
      if (cmd_otrig(obj, actor, cmd, argument, OCMD_ROOM))
        return 1;

  return 0;
}

int wear_otrigger(struct obj_data *obj, struct char_data *actor, int where)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];
  int ret_val;
  long object_id;

  if (!SCRIPT_CHECK(obj, OTRIG_WEAR))
    return 1;

  object_id = obj_script_id(obj);

  for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
    if (TRIGGER_CHECK(t, OTRIG_WEAR)) {
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      ret_val = script_driver(&obj, t, OBJ_TRIGGER, TRIG_NEW);

      if (!obj || !has_obj_by_uid_in_lookup_table(object_id))
        return 0;

      return ret_val;
    }
  }

  return 1;
}

int remove_otrigger(struct obj_data *obj, struct char_data *actor)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];
  int ret_val;
  long object_id;

  if (!SCRIPT_CHECK(obj, OTRIG_REMOVE))
    return 1;

  if (!valid_dg_target(actor, 0))
    return 1;

  object_id = obj_script_id(obj);

  for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
    if (TRIGGER_CHECK(t, OTRIG_REMOVE)) {
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      ret_val = script_driver(&obj, t, OBJ_TRIGGER, TRIG_NEW);

      if (!obj || !has_obj_by_uid_in_lookup_table(object_id))
        return 0;

      return ret_val;
    }
  }

  return 1;
}

int drop_otrigger(struct obj_data *obj, struct char_data *actor)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];
  int ret_val;
  long object_id;

  if (!SCRIPT_CHECK(obj, OTRIG_DROP))
    return 1;

  object_id = obj_script_id(obj);

  for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
    if (TRIGGER_CHECK(t, OTRIG_DROP) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      ret_val = script_driver(&obj, t, OBJ_TRIGGER, TRIG_NEW);

      if (!obj || !has_obj_by_uid_in_lookup_table(object_id))
        return 0;

      return ret_val;
    }
  }

  return 1;
}

int give_otrigger(struct obj_data *obj, struct char_data *actor, struct char_data *victim)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];
  int ret_val;
  long object_id;

  if (!SCRIPT_CHECK(obj, OTRIG_GIVE))
    return 1;

  object_id = obj_script_id(obj);

  for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
    if (TRIGGER_CHECK(t, OTRIG_GIVE) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      ADD_UID_VAR(buf, t, char_script_id(victim), "victim", 0);
      ret_val = script_driver(&obj, t, OBJ_TRIGGER, TRIG_NEW);

      /* Refuse the give if the object was purged, or is no longer the
       * giver's to give. */
      if (!obj || !has_obj_by_uid_in_lookup_table(object_id) || obj->carried_by != actor)
        return 0;

      return ret_val;
    }
  }

  return 1;
}

void load_otrigger(struct obj_data *obj)
{
  struct trig_data *t;
  int result = 0;

  if (!SCRIPT_CHECK(obj, OTRIG_LOAD))
    return;

  for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
    if (TRIGGER_CHECK(t, OTRIG_LOAD) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      result = script_driver(&obj, t, OBJ_TRIGGER, TRIG_NEW);
      break;
    }
  }

  if (result == SCRIPT_ERROR_CODE) {
    /* recursed beyond a reasonable depth: make sure this object is the last
     * one in the load chain */
    if (obj && GET_OBJ_RNUM(obj) >= 0)
      free_proto_script(&obj_proto[GET_OBJ_RNUM(obj)], OBJ_TRIGGER);
  }
}

int cast_otrigger(struct char_data *actor, struct obj_data *obj, int spellnum)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (obj == NULL)
    return 1;

  if (!SCRIPT_CHECK(obj, OTRIG_CAST))
    return 1;

  for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
    if (TRIGGER_CHECK(t, OTRIG_CAST) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      snprintf(buf, sizeof(buf), "%d", spellnum);
      add_var(&GET_TRIG_VARS(t), "spell", buf, 0);
      add_var(&GET_TRIG_VARS(t), "spellname", dg_spell_name(spellnum), 0);
      return script_driver(&obj, t, OBJ_TRIGGER, TRIG_NEW);
    }
  }

  return 1;
}

int leave_otrigger(struct room_data *room, struct char_data *actor, int dir)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];
  int temp, final = 1;
  struct obj_data *obj, *obj_next;

  if (!valid_dg_target(actor, DG_ALLOW_GODS) || !room)
    return 1;

  for (obj = room->contents; obj; obj = obj_next) {
    obj_next = obj->next_content;

    if (!SCRIPT_CHECK(obj, OTRIG_LEAVE))
      continue;

    for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
      if (TRIGGER_CHECK(t, OTRIG_LEAVE) && (number(1, 100) <= GET_TRIG_NARG(t))) {
        if (dir >= 0 && dir < NUM_OF_DIRS)
          add_var(&GET_TRIG_VARS(t), "direction", dirs[dir], 0);
        else
          add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
        ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
        temp = script_driver(&obj, t, OBJ_TRIGGER, TRIG_NEW);
        if (temp == 0)
          final = 0;
      }
    }
  }

  return final;
}

int consume_otrigger(struct obj_data *obj, struct char_data *actor, int cmd)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];
  int ret_val;
  long object_id;

  if (!SCRIPT_CHECK(obj, OTRIG_CONSUME))
    return 1;

  object_id = obj_script_id(obj);

  for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
    if (TRIGGER_CHECK(t, OTRIG_CONSUME)) {
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      switch (cmd) {
        case OCMD_EAT:   add_var(&GET_TRIG_VARS(t), "command", "eat", 0);   break;
        case OCMD_DRINK: add_var(&GET_TRIG_VARS(t), "command", "drink", 0); break;
        case OCMD_QUAFF: add_var(&GET_TRIG_VARS(t), "command", "quaff", 0); break;
      }
      ret_val = script_driver(&obj, t, OBJ_TRIGGER, TRIG_NEW);

      if (!obj || !has_obj_by_uid_in_lookup_table(object_id))
        return 0;

      return ret_val;
    }
  }

  return 1;
}

void time_otrigger(struct obj_data *obj)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (!SCRIPT_CHECK(obj, OTRIG_TIME))
    return;

  for (t = TRIGGERS(SCRIPT(obj)); t; t = t->next) {
    if (TRIGGER_CHECK(t, OTRIG_TIME) && (time_info.hours == GET_TRIG_NARG(t))) {
      snprintf(buf, sizeof(buf), "%d", time_info.hours);
      add_var(&GET_TRIG_VARS(t), "time", buf, 0);
      script_driver(&obj, t, OBJ_TRIGGER, TRIG_NEW);
      break;
    }
  }
}

/* ************************************************************************
*  World triggers.                                                         *
************************************************************************ */

void reset_wtrigger(struct room_data *room)
{
  struct trig_data *t;

  if (!SCRIPT_CHECK(room, WTRIG_RESET))
    return;

  for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
    if (TRIGGER_CHECK(t, WTRIG_RESET) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      script_driver(&room, t, WLD_TRIGGER, TRIG_NEW);
      break;
    }
  }
}

void random_wtrigger(struct room_data *room)
{
  struct trig_data *t;

  if (!SCRIPT_CHECK(room, WTRIG_RANDOM))
    return;

  for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
    if (TRIGGER_CHECK(t, WTRIG_RANDOM) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      script_driver(&room, t, WLD_TRIGGER, TRIG_NEW);
      break;
    }
  }
}

int enter_wtrigger(struct room_data *room, struct char_data *actor, int dir)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (!room || !SCRIPT_CHECK(room, WTRIG_ENTER))
    return 1;

  for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
    if (TRIGGER_CHECK(t, WTRIG_ENTER) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      if (dir >= 0 && dir < NUM_OF_DIRS)
        add_var(&GET_TRIG_VARS(t), "direction", dirs[rev_dir[dir]], 0);
      else
        add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      return script_driver(&room, t, WLD_TRIGGER, TRIG_NEW);
    }
  }

  return 1;
}

int command_wtrigger(struct char_data *actor, char *cmd, char *argument)
{
  struct room_data *room;
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (!actor || !actor->in_room || !SCRIPT_CHECK(actor->in_room, WTRIG_COMMAND))
    return 0;

  /* don't let the people we like get trapped */
  if (!valid_dg_target(actor, 0))
    return 0;

  room = actor->in_room;

  for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
    if (!TRIGGER_CHECK(t, WTRIG_COMMAND))
      continue;

    if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
      script_log("W-Command trigger #%ld has no text argument!", (long) GET_TRIG_VNUM(t));
      continue;
    }

    if (*GET_TRIG_ARG(t) == '*' ||
        !strncmp(GET_TRIG_ARG(t), cmd, strlen(GET_TRIG_ARG(t)))) {
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      skip_spaces(&argument);
      add_var(&GET_TRIG_VARS(t), "arg", argument, 0);
      skip_spaces(&cmd);
      add_var(&GET_TRIG_VARS(t), "cmd", cmd, 0);

      return script_driver(&room, t, WLD_TRIGGER, TRIG_NEW);
    }
  }

  return 0;
}

void speech_wtrigger(struct char_data *actor, const char *str)
{
  struct room_data *room;
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (!actor || !actor->in_room || !SCRIPT_CHECK(actor->in_room, WTRIG_SPEECH))
    return;

  room = actor->in_room;

  for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
    if (!TRIGGER_CHECK(t, WTRIG_SPEECH))
      continue;

    if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)) {
      script_log("W-Speech trigger #%ld has no text argument!", (long) GET_TRIG_VNUM(t));
      continue;
    }

    if (*GET_TRIG_ARG(t) == '*' ||
        (GET_TRIG_NARG(t) && word_check(str, GET_TRIG_ARG(t))) ||
        (!GET_TRIG_NARG(t) && is_substring(GET_TRIG_ARG(t), str))) {
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      add_var(&GET_TRIG_VARS(t), "speech", str, 0);
      script_driver(&room, t, WLD_TRIGGER, TRIG_NEW);
      break;
    }
  }
}

int drop_wtrigger(struct obj_data *obj, struct char_data *actor)
{
  struct room_data *room;
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];
  int ret_val;
  long object_id;

  if (!actor || !actor->in_room || !SCRIPT_CHECK(actor->in_room, WTRIG_DROP))
    return 1;

  object_id = obj_script_id(obj);
  room = actor->in_room;

  for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
    if (TRIGGER_CHECK(t, WTRIG_DROP) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      ADD_UID_VAR(buf, t, object_id, "object", 0);
      ret_val = script_driver(&room, t, WLD_TRIGGER, TRIG_NEW);

      if (!has_obj_by_uid_in_lookup_table(object_id) || obj->carried_by != actor)
        return 0;

      return ret_val;
    }
  }

  return 1;
}

int cast_wtrigger(struct char_data *actor, struct char_data *vict, struct obj_data *obj, int spellnum)
{
  struct room_data *room;
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (!actor || !actor->in_room || !SCRIPT_CHECK(actor->in_room, WTRIG_CAST))
    return 1;

  room = actor->in_room;

  for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
    if (TRIGGER_CHECK(t, WTRIG_CAST) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      if (vict)
        ADD_UID_VAR(buf, t, char_script_id(vict), "victim", 0);
      if (obj)
        ADD_UID_VAR(buf, t, obj_script_id(obj), "object", 0);
      snprintf(buf, sizeof(buf), "%d", spellnum);
      add_var(&GET_TRIG_VARS(t), "spell", buf, 0);
      add_var(&GET_TRIG_VARS(t), "spellname", dg_spell_name(spellnum), 0);
      return script_driver(&room, t, WLD_TRIGGER, TRIG_NEW);
    }
  }

  return 1;
}

int leave_wtrigger(struct room_data *room, struct char_data *actor, int dir)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (!valid_dg_target(actor, DG_ALLOW_GODS))
    return 1;

  if (!room || !SCRIPT_CHECK(room, WTRIG_LEAVE))
    return 1;

  for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
    if (TRIGGER_CHECK(t, WTRIG_LEAVE) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      if (dir >= 0 && dir < NUM_OF_DIRS)
        add_var(&GET_TRIG_VARS(t), "direction", dirs[dir], 0);
      else
        add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      return script_driver(&room, t, WLD_TRIGGER, TRIG_NEW);
    }
  }

  return 1;
}

int door_wtrigger(struct char_data *actor, int subcmd, int dir)
{
  struct room_data *room;
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (!actor || !actor->in_room || !SCRIPT_CHECK(actor->in_room, WTRIG_DOOR))
    return 1;

  room = actor->in_room;

  for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
    if (TRIGGER_CHECK(t, WTRIG_DOOR) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      add_var(&GET_TRIG_VARS(t), "cmd", cmd_door[subcmd], 0);
      if (dir >= 0 && dir < NUM_OF_DIRS)
        add_var(&GET_TRIG_VARS(t), "direction", dirs[dir], 0);
      else
        add_var(&GET_TRIG_VARS(t), "direction", "none", 0);
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      return script_driver(&room, t, WLD_TRIGGER, TRIG_NEW);
    }
  }

  return 1;
}

void time_wtrigger(struct room_data *room)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (!SCRIPT_CHECK(room, WTRIG_TIME))
    return;

  for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
    if (TRIGGER_CHECK(t, WTRIG_TIME) && (time_info.hours == GET_TRIG_NARG(t))) {
      snprintf(buf, sizeof(buf), "%d", time_info.hours);
      add_var(&GET_TRIG_VARS(t), "time", buf, 0);
      script_driver(&room, t, WLD_TRIGGER, TRIG_NEW);
      break;
    }
  }
}

int login_wtrigger(struct room_data *room, struct char_data *actor)
{
  struct trig_data *t;
  char buf[MAX_INPUT_LENGTH];

  if (!room || !SCRIPT_CHECK(room, WTRIG_LOGIN))
    return 1;

  for (t = TRIGGERS(SCRIPT(room)); t; t = t->next) {
    if (TRIGGER_CHECK(t, WTRIG_LOGIN) && (number(1, 100) <= GET_TRIG_NARG(t))) {
      ADD_UID_VAR(buf, t, char_script_id(actor), "actor", 0);
      return script_driver(&room, t, WLD_TRIGGER, TRIG_NEW);
    }
  }

  return 1;
}
