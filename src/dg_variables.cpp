/**************************************************************************
*  File: dg_variables.cpp                                                 *
*  Usage: Variable substitution -- turning %actor.name% and friends into  *
*         text before a trigger line is run.                              *
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
*  THE SHADOWRUN MAPPING                                                  *
*                                                                         *
*  The trigger language is tbaMUD's, but the numbers behind it are not.   *
*  Where a stock field has an obvious Awake counterpart it keeps its name  *
*  and gains an Awake-native alias, so old scripts read and new ones say   *
*  what they mean:                                                         *
*                                                                         *
*    hitp / maxhitp    physical condition monitor, in boxes                *
*    mana / maxmana    mental condition monitor, in boxes                  *
*    gold  = nuyen     nuyen                                               *
*    exp   = karma     karma                                               *
*    str   = str       Strength      con = bod   Body                      *
*    int   = int       Intelligence  dex = qui   Quickness                 *
*    wis   = wil       Willpower     cha = cha   Charisma                  *
*                      plus mag, rea and ess, which have no stock name     *
*    armor = ballistic ballistic rating; impact is its own field           *
*                                                                         *
*  Fields with nothing behind them in Awake are gone rather than faked:    *
*  align, class, damroll, hitroll, prac, the saving_* family, stradd,      *
*  is_killer, is_thief, drunk, hunger, thirst, move/maxmove, affect,       *
*  questpoints, and cost_per_day. A script asking for one gets the same    *
*  "unknown field" log any other typo would produce.                       *
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
#include "quest.hpp"
#include "config.hpp"
#include "dg_scripts.hpp"
#include "dg_event.hpp"

extern struct time_info_data time_info;
extern int wear_bitvectors[];

void add_var(struct trig_var_data **var_list, const char *name, const char *value, long id)
{
  struct trig_var_data *vd;

  if (strchr(name, '.')) {
    log_vfprintf("add_var(): attempt to add illegal var: %s", name);
    return;
  }

  for (vd = *var_list; vd && str_cmp(vd->name, name); vd = vd->next)
    ;

  if (vd && (!vd->context || vd->context == id)) {
    DELETE_ARRAY_IF_EXTANT(vd->value);
  } else {
    vd = new trig_var_data;
    vd->name = str_dup(name);
    vd->next = *var_list;
    vd->context = id;
    *var_list = vd;
  }

  vd->value = str_dup(value);
}

/* Does this object answer to this name? */
static bool dg_obj_is_named(const char *name, struct obj_data *obj)
{
  return obj && name && isname(name, GET_OBJ_KEYWORDS(obj)) != 0;
}

/* Search a list of objects, containers included, and count the matches by
 * uid, vnum or name. */
int item_in_list(const char *item, struct obj_data *list)
{
  struct obj_data *i;
  int count = 0;

  if (!item || !*item)
    return 0;

  if (*item == UID_CHAR) {
    long id = atol(item + 1);

    for (i = list; i; i = i->next_content) {
      if (id == i->script_id)
        count++;
      if (GET_OBJ_TYPE(i) == ITEM_CONTAINER)
        count += item_in_list(item, i->contains);
    }
  } else if (is_number((char *) item)) { /* check for vnum */
    vnum_t ovnum = atol(item);

    for (i = list; i; i = i->next_content) {
      if (GET_OBJ_VNUM(i) == ovnum)
        count++;
      if (GET_OBJ_TYPE(i) == ITEM_CONTAINER)
        count += item_in_list(item, i->contains);
    }
  } else {
    for (i = list; i; i = i->next_content) {
      if (dg_obj_is_named(item, i))
        count++;
      if (GET_OBJ_TYPE(i) == ITEM_CONTAINER)
        count += item_in_list(item, i->contains);
    }
  }

  return count;
}

/* Whether a character has this item at all: worn, carried, or in a container. */
int char_has_item(const char *item, struct char_data *ch)
{
  if (get_object_in_equip(ch, item) != NULL)
    return 1;

  return item_in_list(item, ch->carrying) > 0 ? 1 : 0;
}

/* Is this named flag set in the bitfield? */
static bool dg_flag_is_set(const Bitfield &field, const char *name,
                           const char *table[], int numflags)
{
  if (!name || !*name)
    return FALSE;

  for (int i = 0; i < numflags; i++) {
    if (!table[i] || !str_cmp(table[i], MAX_FLAG_MARKER))
      break;
    if (!str_cmp(table[i], name))
      return field.IsSet(i);
  }

  return FALSE;
}

/* The rating a character has in a named skill. */
static const char *dg_skill_rating(struct char_data *ch, const char *skill)
{
  static char retval[16];
  int skillnum;

  if (!skill || !*skill)
    return "unknown skill";

  skillnum = find_skill_num((char *) skill);
  if (skillnum <= 0)
    return "unknown skill";

  snprintf(retval, sizeof(retval), "%d", GET_SKILL(ch, skillnum));
  return retval;
}

/* The string-manipulation fields, which work on any variable's text. */
int text_processed(const char *field, const char *subfield, struct trig_var_data *vd,
                   char *str, size_t slen)
{
  char *p, *p2;
  char tmpvar[MAX_STRING_LENGTH];

  if (!str_cmp(field, "strlen")) {                     /* strlen  */
    snprintf(str, slen, "%d", (int) strlen(vd->value));
    return TRUE;
  }

  if (!str_cmp(field, "toupper")) {                    /* toupper */
    char *upper = vd->value;
    if (*upper)
      snprintf(str, slen, "%c%s", UPPER(*upper), upper + 1);
    return TRUE;
  }

  if (!str_cmp(field, "trim")) {                       /* trim    */
    strlcpy(tmpvar, vd->value, sizeof(tmpvar) - 1);
    p = tmpvar;
    p2 = tmpvar + strlen(tmpvar) - 1;
    while (*p && isspace(*p))
      p++;
    while ((p <= p2) && isspace(*p2))
      p2--;
    if (p > p2) { /* nothing left */
      *str = '\0';
      return TRUE;
    }
    *(++p2) = '\0';
    snprintf(str, slen, "%s", p);
    return TRUE;
  }

  if (!str_cmp(field, "contains")) {                   /* contains */
    strlcpy(tmpvar, vd->value, sizeof(tmpvar));
    strcpy(str, dg_str_str(tmpvar, subfield) ? "1" : "0");
    return TRUE;
  }

  if (!str_cmp(field, "car")) {                        /* car */
    size_t n = 0;
    while (vd->value[n] && !isspace(vd->value[n]) && n + 1 < slen) {
      str[n] = vd->value[n];
      n++;
    }
    if (slen)
      str[n] = '\0';
    return TRUE;
  }

  if (!str_cmp(field, "cdr")) {                        /* cdr */
    char *cdr = vd->value;
    while (*cdr && !isspace(*cdr))
      cdr++; /* skip the first field */
    while (*cdr && isspace(*cdr))
      cdr++; /* skip to the next */

    snprintf(str, slen, "%s", cdr);
    return TRUE;
  }

  if (!str_cmp(field, "charat")) {                     /* charat */
    size_t len = strlen(vd->value), cindex = atoi(subfield ? subfield : "0");
    if (cindex > len || cindex < 1)
      strcpy(str, "");
    else
      snprintf(str, slen, "%c", vd->value[cindex - 1]);
    return TRUE;
  }

  if (!str_cmp(field, "mudcommand")) {
    /* the game command this text abbreviates, if any */
    size_t length = strlen(vd->value);
    int cmd;

    for (cmd = 0; *cmd_info[cmd].command != '\n'; cmd++)
      if (!strncmp(cmd_info[cmd].command, vd->value, length))
        break;

    if (*cmd_info[cmd].command == '\n')
      *str = '\0';
    else
      snprintf(str, slen, "%s", cmd_info[cmd].command);
    return TRUE;
  }

  return FALSE;
}

/* One exit's worth of %room.<dir>%. */
static void dg_exit_field(struct room_data *r, int dir, const char *subfield,
                          char *str, size_t slen)
{
  struct room_direction_data *ex = r->dir_option[dir];

  if (!ex) {
    *str = '\0';
    return;
  }

  if (subfield && *subfield) {
    if (!str_cmp(subfield, "vnum")) {
      snprintf(str, slen, "%ld", ex->to_room ? (long) GET_ROOM_VNUM(ex->to_room) : -1L);
      return;
    }
    if (!str_cmp(subfield, "key")) {
      snprintf(str, slen, "%ld", (long) ex->key);
      return;
    }
    if (!str_cmp(subfield, "room")) {
      if (ex->to_room)
        snprintf(str, slen, "%c%ld", UID_CHAR, room_script_id(ex->to_room));
      else
        *str = '\0';
      return;
    }
    if (!str_cmp(subfield, "barrier")) {
      snprintf(str, slen, "%d", (int) ex->barrier);
      return;
    }
  }

  /* no subfield, or an unrecognized one: the exit's flags */
  sprintbit(ex->exit_info, exit_bits, str, slen);
}

/* sets str to the value of var.field */
void find_replacement(void *go, struct script_data *sc, struct trig_data *trig,
                      int type, char *var, char *field, char *subfield, char *str, size_t slen)
{
  struct trig_var_data *vd = NULL;
  struct char_data *ch, *c = NULL, *rndm;
  struct obj_data *obj, *o = NULL;
  struct room_data *room, *r = NULL;
  char *name;
  int num, count, i, j, doors;

  static const char *log_cmd[]        = {"mlog ",        "olog ",        "wlog "       };
  static const char *send_cmd[]       = {"msend ",       "osend ",       "wsend "      };
  static const char *echo_cmd[]       = {"mecho ",       "oecho ",       "wecho "      };
  static const char *echoaround_cmd[] = {"mechoaround ", "oechoaround ", "wechoaround "};
  static const char *door[]           = {"mdoor ",       "odoor ",       "wdoor "      };
  static const char *force[]          = {"mforce ",      "oforce ",      "wforce "     };
  static const char *load[]           = {"mload ",       "oload ",       "wload "      };
  static const char *purge[]          = {"mpurge ",      "opurge ",      "wpurge "     };
  static const char *teleport[]       = {"mteleport ",   "oteleport ",   "wteleport "  };
  static const char *xdamage[]        = {"mdamage ",     "odamage ",     "wdamage "    };
  static const char *zoneecho[]       = {"mzoneecho ",   "ozoneecho ",   "wzoneecho "  };
  static const char *asound[]         = {"masound ",     "oasound ",     "wasound "    };
  static const char *at[]             = {"mat ",         "oat ",         "wat "        };
  /* there is no wtransform, hence the wecho */
  static const char *transform[]      = {"mtransform ",  "otransform ",  "wecho "      };
  static const char *recho[]          = {"mrecho ",      "orecho ",      "wrecho "     };
  /* there is no mmove, hence the mecho */
  static const char *omove[]          = {"mecho ",       "omove ",       "wmove "      };

  *str = '\0';

  /* X.global() arrives with a NULL trig */
  if (trig)
    for (vd = GET_TRIG_VARS(trig); vd; vd = vd->next)
      if (!str_cmp(vd->name, var))
        break;

  /* an ill-timed wait could send us here with sc == NULL */
  if (!vd && sc)
    for (vd = sc->global_vars; vd; vd = vd->next)
      if (!str_cmp(vd->name, var) && (vd->context == 0 || vd->context == sc->context))
        break;

  if (!field || !*field) {
    if (vd) {
      snprintf(str, slen, "%s", vd->value);
    } else if (!str_cmp(var, "self")) {
      switch (type) {
        case MOB_TRIGGER:
          snprintf(str, slen, "%c%ld", UID_CHAR, char_script_id((struct char_data *) go));
          break;
        case OBJ_TRIGGER:
          snprintf(str, slen, "%c%ld", UID_CHAR, obj_script_id((struct obj_data *) go));
          break;
        case WLD_TRIGGER:
          snprintf(str, slen, "%c%ld", UID_CHAR, room_script_id((struct room_data *) go));
          break;
      }
    }
    else if (!str_cmp(var, "global")) {
      /* so "remote varname %global%" works */
      snprintf(str, slen, "%d", ROOM_ID_BASE);
    }
    else if (!str_cmp(var, "door"))       snprintf(str, slen, "%s", door[type]);
    else if (!str_cmp(var, "force"))      snprintf(str, slen, "%s", force[type]);
    else if (!str_cmp(var, "load"))       snprintf(str, slen, "%s", load[type]);
    else if (!str_cmp(var, "purge"))      snprintf(str, slen, "%s", purge[type]);
    else if (!str_cmp(var, "teleport"))   snprintf(str, slen, "%s", teleport[type]);
    else if (!str_cmp(var, "damage"))     snprintf(str, slen, "%s", xdamage[type]);
    else if (!str_cmp(var, "send"))       snprintf(str, slen, "%s", send_cmd[type]);
    else if (!str_cmp(var, "echo"))       snprintf(str, slen, "%s", echo_cmd[type]);
    else if (!str_cmp(var, "echoaround")) snprintf(str, slen, "%s", echoaround_cmd[type]);
    else if (!str_cmp(var, "zoneecho"))   snprintf(str, slen, "%s", zoneecho[type]);
    else if (!str_cmp(var, "asound"))     snprintf(str, slen, "%s", asound[type]);
    else if (!str_cmp(var, "at"))         snprintf(str, slen, "%s", at[type]);
    else if (!str_cmp(var, "transform"))  snprintf(str, slen, "%s", transform[type]);
    else if (!str_cmp(var, "recho"))      snprintf(str, slen, "%s", recho[type]);
    else if (!str_cmp(var, "move"))       snprintf(str, slen, "%s", omove[type]);
    else if (!str_cmp(var, "log"))        snprintf(str, slen, "%s", log_cmd[type]);
    else                                  *str = '\0';

    return;
  }

  if (vd && text_processed(field, subfield, vd, str, slen))
    return;

  if (vd) {
    name = vd->value;

    switch (type) {
      case MOB_TRIGGER:
        ch = (struct char_data *) go;

        if ((o = get_object_in_equip(ch, name)))
          ;
        else if ((o = get_obj_in_list(name, ch->carrying)))
          ;
        else if (ch->in_room && (c = get_char_in_room(ch->in_room, name)))
          ;
        else if (ch->in_room && (o = get_obj_in_list(name, ch->in_room->contents)))
          ;
        else if ((c = get_char(name)))
          ;
        else if ((o = get_obj(name)))
          ;
        else
          r = get_room(name);
        break;

      case OBJ_TRIGGER:
        obj = (struct obj_data *) go;

        if ((c = get_char_by_obj(obj, name)))
          ;
        else if ((o = get_obj_by_obj(obj, name)))
          ;
        else
          r = get_room(name);
        break;

      case WLD_TRIGGER:
        room = (struct room_data *) go;

        if ((c = get_char_by_room(room, name)))
          ;
        else if ((o = get_obj_by_room(room, name)))
          ;
        else
          r = get_room(name);
        break;
    }
  }

  else {
    if (!str_cmp(var, "self")) {
      switch (type) {
        case MOB_TRIGGER: c = (struct char_data *) go; break;
        case OBJ_TRIGGER: o = (struct obj_data *) go;  break;
        case WLD_TRIGGER: r = (struct room_data *) go; break;
      }
    }

    else if (!str_cmp(var, "global")) {
      /* The room with vnum 0 holds the mud-wide globals, and %global% hands
       * out its uid, so look it up the same way rather than by rnum. */
      rnum_t global_rnum = real_room(0);
      struct script_data *thescript = global_rnum >= 0 ? SCRIPT(&world[global_rnum]) : NULL;
      *str = '\0';
      if (!thescript) {
        script_log("Attempt to read a global var, but room 0 has no script.");
        return;
      }
      for (vd = thescript->global_vars; vd; vd = vd->next)
        if (!str_cmp(vd->name, field))
          break;

      if (vd)
        snprintf(str, slen, "%s", vd->value);

      return;
    }

    else if (!str_cmp(var, "people")) {
      snprintf(str, slen, "%d", ((num = atoi(field)) > 0) ? trgvar_in_room(num) : 0);
      return;
    }

    else if (!str_cmp(var, "time")) {
      if (!str_cmp(field, "hour"))
        snprintf(str, slen, "%d", time_info.hours);
      else if (!str_cmp(field, "day"))
        snprintf(str, slen, "%d", time_info.day + 1);
      else if (!str_cmp(field, "month"))
        snprintf(str, slen, "%d", time_info.month + 1);
      else if (!str_cmp(field, "year"))
        snprintf(str, slen, "%d", time_info.year);
      else
        *str = '\0';
      return;
    }

    /* %findmob.<room vnum>(<mob vnum>)% counts mobs of that vnum in that room.
     * %findobj.<room vnum>(<vnum/id/name>)% does the same for objects. */
    else if (!str_cmp(var, "findmob")) {
      if (!*field || !subfield || !*subfield) {
        script_log("findmob.vnum(mvnum) - illegal syntax");
        strcpy(str, "0");
      } else {
        rnum_t rrnum = real_room(atol(field));
        vnum_t mvnum = atol(subfield);

        if (rrnum < 0) {
          script_log("findmob.vnum(mvnum): no room with vnum %s", field);
          strcpy(str, "0");
        } else {
          for (i = 0, ch = world[rrnum].people; ch; ch = ch->next_in_room)
            if (GET_MOB_VNUM(ch) == mvnum)
              i++;

          snprintf(str, slen, "%d", i);
        }
      }
      return;
    }

    else if (!str_cmp(var, "findobj")) {
      if (!*field || !subfield || !*subfield) {
        script_log("findobj.vnum(ovnum) - illegal syntax");
        strcpy(str, "0");
      } else {
        rnum_t rrnum = real_room(atol(field));

        if (rrnum < 0) {
          script_log("findobj.vnum(ovnum): no room with vnum %s", field);
          strcpy(str, "0");
        } else {
          snprintf(str, slen, "%d", item_in_list(subfield, world[rrnum].contents));
        }
      }
      return;
    }

    else if (!str_cmp(var, "random")) {
      if (!str_cmp(field, "char")) {
        struct room_data *in_room = NULL;

        rndm = NULL;
        count = 0;

        switch (type) {
          case MOB_TRIGGER: in_room = ((struct char_data *) go)->in_room; break;
          case OBJ_TRIGGER: in_room = obj_room((struct obj_data *) go);   break;
          case WLD_TRIGGER: in_room = (struct room_data *) go;            break;
        }

        if (in_room) {
          for (c = in_room->people; c; c = c->next_in_room) {
            if (type == MOB_TRIGGER && (c == (struct char_data *) go || !CAN_SEE((struct char_data *) go, c)))
              continue;
            if (!valid_dg_target(c, DG_ALLOW_GODS))
              continue;
            if (!number(0, count))
              rndm = c;
            count++;
          }
        }

        if (rndm)
          snprintf(str, slen, "%c%ld", UID_CHAR, char_script_id(rndm));
        else
          *str = '\0';
      }

      else if (!str_cmp(field, "dir")) {
        struct room_data *in_room = NULL;

        switch (type) {
          case WLD_TRIGGER: in_room = (struct room_data *) go;            break;
          case OBJ_TRIGGER: in_room = obj_room((struct obj_data *) go);   break;
          case MOB_TRIGGER: in_room = ((struct char_data *) go)->in_room; break;
        }

        if (!in_room) {
          *str = '\0';
        } else {
          doors = 0;
          for (i = 0; i < NUM_OF_DIRS; i++)
            if (in_room->dir_option[i])
              doors++;

          if (!doors) {
            *str = '\0';
          } else {
            do {
              doors = number(0, NUM_OF_DIRS - 1);
            } while (!in_room->dir_option[doors]);

            snprintf(str, slen, "%s", dirs[doors]);
          }
        }
      }

      else {
        snprintf(str, slen, "%d", ((num = atoi(field)) > 0) ? number(1, num) : 0);
      }

      return;
    }
  }

  /* ********************************************************************
  *  Character fields.                                                   *
  ******************************************************************** */
  if (c) {
    if (!str_cmp(field, "global")) { /* another entity's globals */
      if (IS_NPC(c) && c->script)
        find_replacement(go, c->script, NULL, MOB_TRIGGER, subfield, NULL, NULL, str, slen);
      return;
    }

    /* mark it as 'no match yet' with a byte no field would produce */
    *str = '\x1';

    /* Route aliases to the branch implementing the stock field. */
    char field_initial = LOWER(*field);
    if (!str_cmp(field, "ballistic")) field_initial = 'a';
    else if (!str_cmp(field, "karma")) field_initial = 'e';
    else if (!str_cmp(field, "nuyen")) field_initial = 'g';
    else if (!str_cmp(field, "physical")) field_initial = 'h';
    else if (!str_cmp(field, "pronouns")) field_initial = 's';

    switch (field_initial) {
      case 'a':
        if (!str_cmp(field, "alias")) {
          snprintf(str, slen, "%s", IS_NPC(c) ? GET_KEYWORDS(c) : GET_CHAR_NAME(c));
        }
        else if (!str_cmp(field, "armor") || !str_cmp(field, "ballistic")) {
          snprintf(str, slen, "%d", GET_BALLISTIC(c));
        }
        break;

      case 'b':
        if (!str_cmp(field, "bod")) {
          snprintf(str, slen, "%d", GET_BOD(c));
        }
        break;

      case 'c':
        if (!str_cmp(field, "canbeseen")) {
          if ((type == MOB_TRIGGER) && !CAN_SEE(((struct char_data *) go), c))
            strcpy(str, "0");
          else
            strcpy(str, "1");
        }
        else if (!str_cmp(field, "cha")) {
          snprintf(str, slen, "%d", GET_CHA(c));
        }
        else if (!str_cmp(field, "con")) {  /* stock name for Body */
          snprintf(str, slen, "%d", GET_BOD(c));
        }
        break;

      case 'd':
        if (!str_cmp(field, "dex")) {       /* stock name for Quickness */
          snprintf(str, slen, "%d", GET_QUI(c));
        }
        break;

      case 'e':
        if (!str_cmp(field, "eq")) {
          int pos;
          if (!subfield || !*subfield) {
            *str = '\0';
          } else if (*subfield == '*') {
            for (i = 0, j = 0; i < NUM_WEARS; i++)
              if (GET_EQ(c, i)) {
                j++;
                break;
              }
            if (j > 0)
              strcpy(str, "1");
            else
              *str = '\0';
          } else if ((pos = find_eq_pos_script(subfield)) < 0 || !GET_EQ(c, pos)) {
            *str = '\0';
          } else {
            snprintf(str, slen, "%c%ld", UID_CHAR, obj_script_id(GET_EQ(c, pos)));
          }
        }
        else if (!str_cmp(field, "ess")) {
          snprintf(str, slen, "%d", GET_ESS(c));
        }
        else if (!str_cmp(field, "exp") || !str_cmp(field, "karma")) {
          if (subfield && *subfield)
            GET_KARMA(c) = MAX(0, GET_KARMA(c) + atoi(subfield));
          snprintf(str, slen, "%ld", (long) GET_KARMA(c));
        }
        break;

      case 'f':
        if (!str_cmp(field, "fighting")) {
          if (FIGHTING(c))
            snprintf(str, slen, "%c%ld", UID_CHAR, char_script_id(FIGHTING(c)));
          else
            *str = '\0';
        }
        else if (!str_cmp(field, "follower")) {
          if (!c->followers || !c->followers->follower)
            *str = '\0';
          else
            snprintf(str, slen, "%c%ld", UID_CHAR, char_script_id(c->followers->follower));
        }
        break;

      case 'g':
        if (!str_cmp(field, "gold") || !str_cmp(field, "nuyen")) {
          if (subfield && *subfield)
            GET_NUYEN_RAW(c) = MAX(0, GET_NUYEN_RAW(c) + atoi(subfield));
          snprintf(str, slen, "%ld", (long) GET_NUYEN(c));
        }
        break;

      case 'h':
        if (!str_cmp(field, "has_item")) {
          if (!(subfield && *subfield))
            *str = '\0';
          else
            snprintf(str, slen, "%d", char_has_item(subfield, c));
        }
        else if (!str_cmp(field, "hasattached")) {
          if (!(subfield && *subfield) || !IS_NPC(c))
            *str = '\0';
          else
            snprintf(str, slen, "%d", trig_is_attached(SCRIPT(c), atol(subfield)));
        }
        else if (!str_cmp(field, "heshe"))
          snprintf(str, slen, "%s", HSSH(c));
        else if (!str_cmp(field, "himher"))
          snprintf(str, slen, "%s", HMHR(c));
        else if (!str_cmp(field, "hisher"))
          snprintf(str, slen, "%s", HSHR(c));
        else if (!str_cmp(field, "height"))
          snprintf(str, slen, "%d", GET_HEIGHT(c));
        else if (!str_cmp(field, "hitp") || !str_cmp(field, "physical")) {
          /* Condition monitors are stored in hundredths of a box. */
          if (subfield && *subfield)
            GET_PHYSICAL(c) = MIN(GET_MAX_PHYSICAL(c), GET_PHYSICAL(c) + atoi(subfield) * 100);
          snprintf(str, slen, "%d", (int) (GET_PHYSICAL(c) / 100));
        }
        break;

      case 'i':
        if (!str_cmp(field, "id")) {
          snprintf(str, slen, "%ld", char_script_id(c));
        }
        else if (!str_cmp(field, "impact")) {
          snprintf(str, slen, "%d", GET_IMPACT(c));
        }
        else if (!str_cmp(field, "is_pc")) {
          strcpy(str, IS_NPC(c) ? "0" : "1");
        }
        else if (!str_cmp(field, "int")) {
          snprintf(str, slen, "%d", GET_INT(c));
        }
        else if (!str_cmp(field, "inventory")) {
          if (subfield && *subfield) {
            for (obj = c->carrying; obj; obj = obj->next_content) {
              if (GET_OBJ_VNUM(obj) == atol(subfield)) {
                snprintf(str, slen, "%c%ld", UID_CHAR, obj_script_id(obj));
                return;
              }
            }
            *str = '\0';
          } else if (c->carrying) {
            snprintf(str, slen, "%c%ld", UID_CHAR, obj_script_id(c->carrying));
          } else {
            *str = '\0';
          }
        }
        break;

      case 'l':
        if (!str_cmp(field, "level")) {
          /* Awake has no mortal levels; this is the staff level. */
          snprintf(str, slen, "%d", GET_LEVEL(c));
        }
        break;

      case 'm':
        if (!str_cmp(field, "mag")) {
          snprintf(str, slen, "%d", GET_MAG(c) / 100);
        }
        else if (!str_cmp(field, "mana") || !str_cmp(field, "mental")) {
          if (subfield && *subfield)
            GET_MENTAL(c) = MIN(GET_MAX_MENTAL(c), GET_MENTAL(c) + atoi(subfield) * 100);
          snprintf(str, slen, "%d", (int) (GET_MENTAL(c) / 100));
        }
        else if (!str_cmp(field, "master")) {
          if (!c->master)
            *str = '\0';
          else
            snprintf(str, slen, "%c%ld", UID_CHAR, char_script_id(c->master));
        }
        else if (!str_cmp(field, "maxhitp") || !str_cmp(field, "maxphysical")) {
          snprintf(str, slen, "%d", (int) (GET_MAX_PHYSICAL(c) / 100));
        }
        else if (!str_cmp(field, "maxmana") || !str_cmp(field, "maxmental")) {
          snprintf(str, slen, "%d", (int) (GET_MAX_MENTAL(c) / 100));
        }
        break;

      case 'n':
        if (!str_cmp(field, "name")) {
          snprintf(str, slen, "%s", GET_CHAR_NAME(c));
        }
        else if (!str_cmp(field, "next_in_room")) {
          if (c->next_in_room)
            snprintf(str, slen, "%c%ld", UID_CHAR, char_script_id(c->next_in_room));
          else
            *str = '\0';
        }
        else if (!str_cmp(field, "npcflag")) {
          if (subfield && *subfield && IS_NPC(c))
            snprintf(str, slen, "%d", dg_flag_is_set(MOB_FLAGS(c), subfield, action_bits, MOB_MAX) ? 1 : 0);
          else
            strcpy(str, "0");
        }
        break;

      case 'p':
        if (!str_cmp(field, "pos")) {
          if (subfield && *subfield) {
            for (i = POS_MORTALLYW; i <= POS_STANDING; i++) {
              if (!strncmp(subfield, position_types[i], strlen(subfield))) {
                GET_POS(c) = i;
                break;
              }
            }
          }
          snprintf(str, slen, "%s", position_types[(int) GET_POS(c)]);
        }
        else if (!str_cmp(field, "pref")) {
          if (!IS_NPC(c) && subfield && *subfield)
            snprintf(str, slen, "%d", dg_flag_is_set(PRF_FLAGS(c), subfield, preference_bits, PRF_MAX) ? 1 : 0);
          else
            strcpy(str, "0");
        }
        break;

      case 'q':
        if (!str_cmp(field, "qui")) {
          snprintf(str, slen, "%d", GET_QUI(c));
        }
        else if (!str_cmp(field, "quest")) {
          if (!IS_NPC(c) && GET_QUEST(c) > 0)
            snprintf(str, slen, "%ld", (long) GET_QUEST(c));
          else
            strcpy(str, "0");
        }
        break;

      case 'r':
        if (!str_cmp(field, "rea")) {
          snprintf(str, slen, "%d", GET_REA(c));
        }
        else if (!str_cmp(field, "room")) {
          /* Somebody riding in a vehicle is not on any room's people list,
           * so no room trigger will ever see them, but a script that already
           * has hold of them should still be told where they are rather than
           * be handed the void. Anyone who is in neither a room nor a vehicle
           * is between states and yields nothing at all. */
          struct room_data *where = c->in_room;

          if (!where && c->in_veh)
            where = get_veh_in_room(c->in_veh);

          if (where)
            snprintf(str, slen, "%c%ld", UID_CHAR, room_script_id(where));
          else
            *str = '\0';
        }
        break;

      case 's':
        if (!str_cmp(field, "sex") || !str_cmp(field, "pronouns")) {
          snprintf(str, slen, "%s", HSSH(c));
        }
        else if (!str_cmp(field, "skill")) {
          snprintf(str, slen, "%s", dg_skill_rating(c, subfield));
        }
        else if (!str_cmp(field, "skillset")) {
          if (!IS_NPC(c) && subfield && *subfield) {
            char skillname[MAX_INPUT_LENGTH], *amount;
            amount = one_argument(subfield, skillname);
            skip_spaces(&amount);
            if (amount && *amount && is_number(amount)) {
              int skillnum = find_skill_num(skillname);
              if (skillnum > 0)
                SET_SKILL(c, skillnum, MAX(0, MIN(MAX_SKILL_LEVEL_FOR_MORTS, atoi(amount))));
            }
          }
          *str = '\0'; /* so the parser knows 'skillset' was recognized */
        }
        else if (!str_cmp(field, "str")) {
          snprintf(str, slen, "%d", GET_STR(c));
        }
        break;

      case 't':
        if (!str_cmp(field, "title")) {
          snprintf(str, slen, "%s", (!IS_NPC(c) && GET_TITLE(c)) ? GET_TITLE(c) : "");
        }
        break;

      case 'v':
        if (!str_cmp(field, "varexists")) {
          struct trig_var_data *remote_vd;
          strcpy(str, "0");
          if (SCRIPT(c)) {
            for (remote_vd = SCRIPT(c)->global_vars; remote_vd; remote_vd = remote_vd->next)
              if (!str_cmp(remote_vd->name, subfield))
                break;
            if (remote_vd)
              strcpy(str, "1");
          }
        }
        else if (!str_cmp(field, "vnum")) {
          if (subfield && *subfield)
            snprintf(str, slen, "%d", IS_NPC(c) ? (int) (GET_MOB_VNUM(c) == atol(subfield)) : 0);
          else if (IS_NPC(c))
            snprintf(str, slen, "%ld", (long) GET_MOB_VNUM(c));
          else
            strcpy(str, "-1"); /* deprecated; use is_pc */
        }
        break;

      case 'w':
        if (!str_cmp(field, "weight")) {
          snprintf(str, slen, "%d", GET_WEIGHT(c));
        }
        else if (!str_cmp(field, "wil") || !str_cmp(field, "wis")) {
          snprintf(str, slen, "%d", GET_WIL(c));
        }
        break;
    } /* switch *field */

    if (*str == '\x1') { /* nothing matched */
      if (SCRIPT(c)) {
        for (vd = (SCRIPT(c))->global_vars; vd; vd = vd->next)
          if (!str_cmp(vd->name, field))
            break;
        if (vd) {
          snprintf(str, slen, "%s", vd->value);
          return;
        }
      }

      *str = '\0';
      script_log("Trigger: %s, VNum %ld. unknown char field: '%s'",
                 GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), field);
    }

    return;
  }

  /* ********************************************************************
  *  Object fields.                                                      *
  ******************************************************************** */
  if (o) {
    *str = '\x1';

    switch (LOWER(*field)) {
      case 'a':
        if (!str_cmp(field, "affects")) {
          if (subfield && *subfield)
            snprintf(str, slen, "%d", dg_flag_is_set(GET_OBJ_AFFECT(o), subfield, affected_bits, AFF_MAX) ? 1 : 0);
          else
            strcpy(str, "0");
        }
        break;

      case 'c':
        if (!str_cmp(field, "cost")) {
          if (subfield && *subfield)
            GET_OBJ_COST(o) = MAX(1, atoi(subfield) + GET_OBJ_COST(o));
          snprintf(str, slen, "%d", GET_OBJ_COST(o));
        }
        else if (!str_cmp(field, "carried_by")) {
          if (o->carried_by)
            snprintf(str, slen, "%c%ld", UID_CHAR, char_script_id(o->carried_by));
          else
            *str = '\0';
        }
        else if (!str_cmp(field, "contents")) {
          if (o->contains)
            snprintf(str, slen, "%c%ld", UID_CHAR, obj_script_id(o->contains));
          else
            *str = '\0';
        }
        else if (!str_cmp(field, "count")) {
          if (GET_OBJ_TYPE(o) == ITEM_CONTAINER)
            snprintf(str, slen, "%d", item_in_list(subfield, o->contains));
          else
            strcpy(str, "0");
        }
        break;

      case 'e':
        if (!str_cmp(field, "extra")) {
          if (subfield && *subfield)
            snprintf(str, slen, "%d", dg_flag_is_set(GET_OBJ_EXTRA(o), subfield, extra_bits, MAX_ITEM_EXTRA) ? 1 : 0);
          else
            GET_OBJ_EXTRA(o).PrintBits(str, slen, extra_bits, MAX_ITEM_EXTRA);
        }
        break;

      case 'h':
        if (!str_cmp(field, "has_in")) {
          if (GET_OBJ_TYPE(o) == ITEM_CONTAINER)
            strcpy(str, item_in_list(subfield, o->contains) ? "1" : "0");
          else
            strcpy(str, "0");
        }
        else if (!str_cmp(field, "hasattached")) {
          if (!(subfield && *subfield))
            *str = '\0';
          else
            snprintf(str, slen, "%d", trig_is_attached(SCRIPT(o), atol(subfield)));
        }
        break;

      case 'i':
        if (!str_cmp(field, "id")) {
          snprintf(str, slen, "%ld", obj_script_id(o));
        }
        else if (!str_cmp(field, "is_inroom")) {
          if (o->in_room)
            snprintf(str, slen, "%c%ld", UID_CHAR, room_script_id(o->in_room));
          else
            *str = '\0';
        }
        else if (!str_cmp(field, "is_pc")) {
          strcpy(str, "-1");
        }
        break;

      case 'n':
        if (!str_cmp(field, "name")) {
          snprintf(str, slen, "%s", GET_OBJ_KEYWORDS(o) ? GET_OBJ_KEYWORDS(o) : "");
        }
        else if (!str_cmp(field, "next_in_list")) {
          if (o->next_content)
            snprintf(str, slen, "%c%ld", UID_CHAR, obj_script_id(o->next_content));
          else
            *str = '\0';
        }
        break;

      case 'r':
        if (!str_cmp(field, "room")) {
          struct room_data *orm = obj_room(o);
          if (orm)
            snprintf(str, slen, "%c%ld", UID_CHAR, room_script_id(orm));
          else
            *str = '\0';
        }
        break;

      case 's':
        if (!str_cmp(field, "shortdesc")) {
          snprintf(str, slen, "%s", GET_OBJ_NAME(o));
        }
        break;

      case 't':
        if (!str_cmp(field, "type")) {
          sprinttype(GET_OBJ_TYPE(o), item_types, str, slen);
        }
        else if (!str_cmp(field, "timer")) {
          snprintf(str, slen, "%d", GET_OBJ_TIMER(o));
        }
        break;

      case 'v':
        if (!str_cmp(field, "vnum")) {
          if (subfield && *subfield)
            snprintf(str, slen, "%d", (int) (GET_OBJ_VNUM(o) == atol(subfield)));
          else
            snprintf(str, slen, "%ld", (long) GET_OBJ_VNUM(o));
        }
        else if (!strncmp(field, "val", 3) && is_number(field + 3)) {
          /* val0 through val17: Awake objects carry more values than the
           * four tbaMUD's version exposed. */
          int which = atoi(field + 3);
          if (which >= 0 && which < NUM_OBJ_VALUES)
            snprintf(str, slen, "%d", GET_OBJ_VAL(o, which));
          else
            *str = '\0';
        }
        break;

      case 'w':
        if (!str_cmp(field, "wearflag")) {
          if (subfield && *subfield) {
            int pos = find_eq_pos_script(subfield);
            strcpy(str, (pos >= 0 && CAN_WEAR(o, wear_bitvectors[pos])) ? "1" : "0");
          } else {
            strcpy(str, "0");
          }
        }
        else if (!str_cmp(field, "weight")) {
          if (subfield && *subfield)
            GET_OBJ_WEIGHT(o) = MAX(1, atoi(subfield) + GET_OBJ_WEIGHT(o));
          snprintf(str, slen, "%d", (int) GET_OBJ_WEIGHT(o));
        }
        else if (!str_cmp(field, "worn_by")) {
          if (o->worn_by)
            snprintf(str, slen, "%c%ld", UID_CHAR, char_script_id(o->worn_by));
          else
            *str = '\0';
        }
        break;
    } /* switch *field */

    if (*str == '\x1') { /* nothing matched */
      if (SCRIPT(o)) {
        for (vd = (SCRIPT(o))->global_vars; vd; vd = vd->next)
          if (!str_cmp(vd->name, field))
            break;
        if (vd) {
          snprintf(str, slen, "%s", vd->value);
          return;
        }
      }

      *str = '\0';
      script_log("Trigger: %s, VNum %ld, type: %d. unknown object field: '%s'",
                 GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), type, field);
    }

    return;
  }

  /* ********************************************************************
  *  Room fields.                                                        *
  ******************************************************************** */
  if (r) {
    /* Room 0 is where the mud-wide globals live. */
    if (r->number == 0) {
      if (!SCRIPT(r)) {
        *str = '\0';
        script_log("Trigger: %s, VNum %ld, type: %d. Tried to read the global var list of room 0, which has no script.",
                   GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), type);
      } else {
        for (vd = (SCRIPT(r))->global_vars; vd; vd = vd->next)
          if (!str_cmp(vd->name, field))
            break;
        if (vd)
          snprintf(str, slen, "%s", vd->value);
        else
          *str = '\0';
      }
      return;
    }

    if (!str_cmp(field, "name")) {
      snprintf(str, slen, "%s", GET_ROOM_NAME(r));
      return;
    }

    if (!str_cmp(field, "sector")) {
      sprinttype(r->sector_type, spirit_name, str, slen);
      return;
    }

    if (!str_cmp(field, "vnum")) {
      if (subfield && *subfield)
        snprintf(str, slen, "%d", (int) (r->number == atol(subfield)));
      else
        snprintf(str, slen, "%ld", (long) r->number);
      return;
    }

    if (!str_cmp(field, "contents")) {
      if (subfield && *subfield) {
        for (obj = r->contents; obj; obj = obj->next_content) {
          if (GET_OBJ_VNUM(obj) == atol(subfield)) {
            snprintf(str, slen, "%c%ld", UID_CHAR, obj_script_id(obj));
            return;
          }
        }
        *str = '\0';
      } else if (r->contents) {
        snprintf(str, slen, "%c%ld", UID_CHAR, obj_script_id(r->contents));
      } else {
        *str = '\0';
      }
      return;
    }

    if (!str_cmp(field, "people")) {
      if (r->people)
        snprintf(str, slen, "%c%ld", UID_CHAR, char_script_id(r->people));
      else
        *str = '\0';
      return;
    }

    if (!str_cmp(field, "id")) {
      snprintf(str, slen, "%ld", room_script_id(r));
      return;
    }

    if (!str_cmp(field, "weather")) {
      static const char *sky_look[] = { "sunny", "cloudy", "rainy", "lightning" };

      if (!ROOM_FLAGGED(r, ROOM_INDOORS) && weather_info.sky >= 0 && weather_info.sky <= 3)
        snprintf(str, slen, "%s", sky_look[weather_info.sky]);
      else
        *str = '\0';
      return;
    }

    if (!str_cmp(field, "hasattached")) {
      if (!(subfield && *subfield))
        *str = '\0';
      else
        snprintf(str, slen, "%d", trig_is_attached(SCRIPT(r), atol(subfield)));
      return;
    }

    if (!str_cmp(field, "zonenumber")) {
      snprintf(str, slen, "%d", zone_table[r->zone].number);
      return;
    }

    if (!str_cmp(field, "zonename")) {
      snprintf(str, slen, "%s", zone_table[r->zone].name ? zone_table[r->zone].name : "");
      return;
    }

    if (!str_cmp(field, "roomflag")) {
      if (subfield && *subfield)
        snprintf(str, slen, "%d", dg_flag_is_set(ROOM_FLAGS(r), subfield, room_bits, ROOM_MAX) ? 1 : 0);
      else
        strcpy(str, "0");
      return;
    }

    /* Exits, one field per direction. */
    {
      int dir = search_block(field, dirs, TRUE);
      if (dir >= 0 && dir < NUM_OF_DIRS) {
        dg_exit_field(r, dir, subfield, str, slen);
        return;
      }
    }

    /* nothing matched: fall back to the room's own globals */
    if (SCRIPT(r)) {
      for (vd = (SCRIPT(r))->global_vars; vd; vd = vd->next)
        if (!str_cmp(vd->name, field))
          break;
      if (vd) {
        snprintf(str, slen, "%s", vd->value);
        return;
      }
    }

    *str = '\0';
    script_log("Trigger: %s, VNum %ld, type: %d. unknown room field: '%s'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), type, field);
  }
}

/* Substitutes any variables in line, and returns the result in buf. A field
 * that yields a name or a uid can be chained, so %actor.room.name% works, and
 * subfields are themselves substituted first. */
void var_subst(void *go, struct script_data *sc, struct trig_data *trig,
               int type, char *line, char *buf)
{
  char tmp[MAX_INPUT_LENGTH], repl_str[MAX_INPUT_LENGTH - 20]; /* -20 for "eval tmpvr " */
  char *var = NULL, *field = NULL, *p = NULL;
  char tmp2[MAX_INPUT_LENGTH];
  char *subfield_p, subfield[MAX_INPUT_LENGTH];
  int left, len;
  int paren_count = 0;
  int dots = 0;

  /* A trigger command line has no length limit of its own. A .trg file cannot
   * deliver one over the reader's line length, but the string editor can, and
   * trigedit_save() rebuilds the command list and arms every live instance
   * before the builder leaves the editor. So a single line can be far longer
   * than the MAX_INPUT_LENGTH buffers below -- both tmp[] here and the cmd[]
   * the caller passes as buf. Refuse the line rather than copy it, and hand
   * back an empty result so the caller runs nothing instead of running a
   * truncated command. */
  if (strlen(line) >= MAX_INPUT_LENGTH) {
    script_log("Trigger: %s, VNum %ld, type: %d. Line is %d characters, over the %d limit: '%.60s...'",
               GET_TRIG_NAME(trig), (long) GET_TRIG_VNUM(trig), type,
               (int) strlen(line), MAX_INPUT_LENGTH - 1, line);
    *buf = '\0';
    return;
  }

  /* nothing to do if there are no %'s */
  if (!strchr(line, '%')) {
    strcpy(buf, line);
    return;
  }

  *repl_str = *tmp = *tmp2 = '\0';

  p = strcpy(tmp, line);
  subfield_p = subfield;

  left = MAX_INPUT_LENGTH - 1;

  while (*p && (left > 0)) {
    /* copy until we find the first % */
    while (*p && (*p != '%') && (left > 0)) {
      *(buf++) = *(p++);
      left--;
    }

    *buf = '\0';

    /* a doubled %% is a literal % */
    if (*p && (*(++p) == '%') && (left > 0)) {
      *(buf++) = *(p++);
      *buf = '\0';
      left--;
      continue;
    }

    else if (*p && (left > 0)) {
      subfield_p = subfield;
      *subfield = '\0';
      paren_count = 0;

      /* search until the end of the var, or the start of the field */
      for (var = p; *p && (*p != '%') && (*p != '.'); p++)
        ;

      field = p;
      if (*p == '.') {
        *(p++) = '\0';
        dots = 0;
        for (field = p; *p && ((*p != '%') || (paren_count > 0) || (dots)); p++) {
          if (dots > 0) {
            *subfield_p = '\0';
            find_replacement(go, sc, trig, type, var, field, subfield, repl_str, sizeof(repl_str));
            if (*repl_str) {
              snprintf(tmp2, sizeof(tmp2), "eval tmpvr %s", repl_str); /* temp var */
              process_eval(go, sc, trig, type, tmp2);
              strcpy(var, "tmpvr");
              field = p;
              dots = 0;
              continue;
            }
            dots = 0;
          } else if (*p == '(') {
            *p = '\0';
            paren_count++;
          } else if (*p == ')') {
            *p = '\0';
            paren_count--;
          } else if (paren_count > 0) {
            *subfield_p++ = *p;
          } else if (*p == '.') {
            *p = '\0';
            dots++;
          }
        }
      }

      if (!*p) {
        script_log("Unterminated variable in trigger %ld.", (long) GET_TRIG_VNUM(trig));
        break;
      }
      *(p++) = '\0';
      *subfield_p = '\0';

      if (*subfield) {
        var_subst(go, sc, trig, type, subfield, tmp2);
        strcpy(subfield, tmp2);
      }

      find_replacement(go, sc, trig, type, var, field, subfield, repl_str, sizeof(repl_str));

      /* strncat() stops after left characters, so advance by what it really
       * wrote. Advancing by the whole length of repl_str instead walks buf
       * past the end of the caller's buffer whenever a replacement does not
       * fit, and the terminator written after the loop then lands there. */
      strncat(buf, repl_str, left);
      len = strlen(repl_str);
      if (len > left)
        len = left;
      buf += len;
      left -= len;
    }
  }

  *buf = '\0';
}
