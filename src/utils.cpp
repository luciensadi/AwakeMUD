/* ************************************************************************
*   File: utils.c                                       Part of CircleMUD *
*  Usage: various internal functions of a utility nature                  *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <sys/types.h>
#include <stdarg.h>
#include <iostream>
#include <mysql/mysql.h>

using namespace std;

#if defined(WIN32) && !defined(__CYGWIN__)
#include <winsock.h>
#define random() rand()
#define srandom(x) srand(x)
#else
#include <netinet/in.h>
#include <unistd.h>
#endif

#include "telnet.h"
#include "structs.h"
#include "utils.h"
#include "awake.h"
#include "comm.h"
#include "handler.h"
#include "memory.h"
#include "house.h"
#include "db.h"
#include "constants.h"
#include "newmagic.h"

extern class memoryClass *Mem;
extern struct time_info_data time_info;

extern void die(struct char_data * ch);
extern const char *log_types[];
extern long beginning_of_time;
extern int ability_cost(int abil, int level);
extern MYSQL *mysql;
extern int mysql_wrapper(MYSQL *mysql, char *buf);

/* creates a random number in interval [from;to] */
int number(int from, int to)
{
  if (from == to)
    return from;
  else if (from > to) {
    // it should not happen, but if it does...
    int temp = to;
    to = from;
    from = temp;
  }
  return ((random() % (to - from + 1)) + from);
}

/* simulates dice roll */
int dice(int number, int size)
{
  int sum = 0;

  if (size <= 0 || number <= 0)
    return 0;

  while (number-- > 0)
    sum += ((random() % size) + 1);

  return sum;
}

#ifndef __GNUG__   
int MIN(long a, long b)
{
  return a < b ? a : b;
}

int MAX(long a, long b)
{
  return a > b ? a : b;
}
#endif

/* rolls a 6-sided dice by rule of 6 and rule of 1 */
int srdice(void)
{
  static int roll;
  int sum = 0, num = 1;
  register int i;

  for (i = 1; i <= num; i++) {
    roll = ((random() % 6) + 1);
    if (roll == 6)
      num++;
    sum += roll;
  }
  return sum;
}

int success_test(int number, int target)
{
  if (number < 1)
    return -1;

  int total = 0, roll, one = 0;
  register int i;

  target = MAX(target, 2);

  for (i = 1; i <= number; i++) {
    if ((roll = srdice()) == 1)
      one++;
    else if (roll >= target)
      total++;
  }

  if (one == number)
    return -1;
  return total;
}

int resisted_test(int num4ch, int tar4ch, int num4vict, int tar4vict)
{
  return (success_test(num4ch, tar4ch) - success_test(num4vict, tar4vict));
}

int stage(int successes, int wound)
{
  if (successes >= 0)
    while (successes >= 2) {
      wound++;
      successes -= 2;
    }
  else
    while (successes <= -2) {
      wound--;
      successes += 2;
    }
  return wound;
}

int convert_damage(int damage)
{
  if (damage < 0)
    damage = 0;
  else
    damage = damage_array[MIN(4, damage)];

  return damage;
}

int light_level(rnum_t room)
{
  if (world[room].sector_type == SPIRIT_HEARTH)
    return world[room].vision[0];
  if (world[room].sector_type == SPIRIT_CITY) {
    if ((time_info.hours > 6 && time_info.hours < 19))
      return world[room].vision[0];
    else if (world[room].vision[0] == LIGHT_NORMALNOLIT)
      return LIGHT_MINLIGHT;
    else
      return LIGHT_PARTLIGHT;
  }
  if ((time_info.hours < 6 && time_info.hours > 19) && (world[room].vision[0] > LIGHT_MINLIGHT || world[room].vision[0] <= LIGHT_NORMALNOLIT))
    return LIGHT_MINLIGHT;
  else
    return world[room].vision[0];
}

int damage_modifier(struct char_data *ch, char *rbuf)
{
  int physical = GET_PHYSICAL(ch), mental = GET_MENTAL(ch), base_target = 0;
  for (struct obj_data *obj = ch->bioware; obj; obj = obj->next_content) {
    if (GET_OBJ_VAL(obj, 0) == BIO_DAMAGECOMPENSATOR) {
      physical += GET_OBJ_VAL(obj, 1) * 100;
      mental += GET_OBJ_VAL(obj, 1) * 100;
    } else if (GET_OBJ_VAL(obj, 0) == BIO_PAINEDITOR && GET_OBJ_VAL(obj, 3))
      mental = 1000;
  }
  if (AFF_FLAGGED(ch, AFF_RESISTPAIN))
  {
    physical += ch->points.resistpain * 100;
    mental += ch->points.resistpain * 100;
  }
  if (GET_TRADITION(ch) == TRAD_ADEPT
      && GET_POWER(ch, ADEPT_PAIN_RESISTANCE) > 0)
  {
    physical += GET_POWER(ch, ADEPT_PAIN_RESISTANCE) * 100;
    mental += GET_POWER(ch, ADEPT_PAIN_RESISTANCE) * 100;
  }
  if (GET_DRUG_STAGE(ch) == 1)
    switch (GET_DRUG_AFFECT(ch)) {
      case DRUG_NITRO:
        physical += 600;
        mental += 600;
        break;
      case DRUG_NOVACOKE:
        physical += 100;
        mental += 100;
        break;
      case DRUG_BLISS:
        physical += 300;
        mental += 300;
        break;
      case DRUG_KAMIKAZE:
        physical += 400;
        mental += 400;
        break;
    }
  // first apply physical damage modifiers
  if (physical <= 400)
  {
    base_target += 3;
    buf_mod( rbuf, "PhyS", 3 );
  } else if (physical <= 700)
  {
    base_target += 2;
    buf_mod( rbuf, "PhyM", 2 );
  } else if (GET_PHYSICAL(ch) <= 900)
  {
    base_target += 1;
    buf_mod( rbuf, "PhyL", 1 );
  }
  if (mental <= 400)
  {
    base_target += 3;
    buf_mod( rbuf, "MenS", 3 );
  } else if (mental <= 700)
  {
    base_target += 2;
    buf_mod( rbuf, "MenM", 2 );
  } else if (mental <= 900)
  {
    base_target += 1;
    buf_mod( rbuf, "MenL", 1 );
  }
  return base_target;
}
int modify_target_rbuf(struct char_data *ch, char *rbuf)
{
  extern time_info_data time_info;
  int base_target = 0, light_target = 0;
  base_target += damage_modifier(ch, rbuf);
  // then apply modifiers for sustained spells
  if (GET_SUSTAINED_NUM(ch) > 0)
  {
    base_target += ((GET_SUSTAINED_NUM(ch) - GET_SUSTAINED_FOCI(ch)) * 2);
    buf_mod( rbuf, "Sustain", (GET_SUSTAINED_NUM(ch) - GET_SUSTAINED_FOCI(ch)) * 2);
  }

  if (PLR_FLAGGED(ch, PLR_PERCEIVE))
  {
    base_target += 2;
    buf_mod(rbuf, "AstralPercep", 2);
  } else
  {
    switch (light_level(ch->in_room)) {
    case LIGHT_FULLDARK:
      if (CURRENT_VISION(ch) == THERMOGRAPHIC) {
        if (NATURAL_VISION(ch) == THERMOGRAPHIC) {
          light_target += 2;
          buf_mod(rbuf, "FullDark", 2);
        } else {
          light_target += 4;
          buf_mod(rbuf, "FullDark", 4);
        }
      } else {
        light_target += 8;
        buf_mod(rbuf, "FullDark", 8);
      }
      break;
    case LIGHT_MINLIGHT:
      if (CURRENT_VISION(ch) == NORMAL) {
        light_target += 6;
        buf_mod(rbuf, "MinLight", 6);
      } else {
        if (NATURAL_VISION(ch) == NORMAL) {
          light_target += 4;
          buf_mod(rbuf, "MinLight", 4);
        } else {
          base_target += 2;
          buf_mod(rbuf, "MinLight", 2);
        }
      }
      break;
    case LIGHT_PARTLIGHT:
      if (CURRENT_VISION(ch) == NORMAL) {
        light_target += 2;
        buf_mod(rbuf, "PartLight", 2);
      } else if (CURRENT_VISION(ch) == LOWLIGHT) {
        if (NATURAL_VISION(ch) != LOWLIGHT) {
          light_target++;
          buf_mod(rbuf, "PartLight", 1);
        }
      } else {
        if (NATURAL_VISION(ch) != THERMOGRAPHIC) {
          light_target += 2;
          buf_mod(rbuf, "PartLight", 2);
        } else {
          light_target++;
          buf_mod(rbuf, "PartLight", 1);
        }
      }
      break;
    case LIGHT_GLARE:
      if (CURRENT_VISION(ch) == NORMAL) {
        light_target += 2;
        buf_mod(rbuf, "Glare", 2);
      } else {
        if (NATURAL_VISION(ch) == NORMAL) {
          light_target += 4;
          buf_mod(rbuf, "Glare", 2);
        } else {
          light_target += 2;
          buf_mod(rbuf, "Glare", 2);
        }
      }
      break;
    }
    if (light_target > 0 && world[ch->in_room].light[1]) {
      if (world[ch->in_room].light[2]) {
        light_target = MAX(0, light_target - world[ch->in_room].light[2]);
        buf_mod(rbuf, "LightSpell", - world[ch->in_room].light[2]);
      } else
        light_target /= 2;
    }
    if (world[ch->in_room].shadow[0]) {
      light_target += world[ch->in_room].shadow[1];
      buf_mod(rbuf, "ShadowSpell", world[ch->in_room].shadow[1]);
    }
    base_target += light_target;

    if (world[ch->in_room].vision[1] == LIGHT_MIST)
      if (CURRENT_VISION(ch) == NORMAL || (CURRENT_VISION(ch) == LOWLIGHT && NATURAL_VISION(ch) == LOWLIGHT)) {
        base_target += 2;
        buf_mod(rbuf, "Mist", 2);
      }
    if (world[ch->in_room].vision[1] == LIGHT_LIGHTSMOKE || (weather_info.sky == SKY_RAINING &&
        world[ch->in_room].sector_type != SPIRIT_HEARTH && !ROOM_FLAGGED(ch->in_room, ROOM_INDOORS))) {
      if (CURRENT_VISION(ch) == NORMAL || (CURRENT_VISION(ch) == LOWLIGHT && NATURAL_VISION(ch) != LOWLIGHT)) {
        base_target += 4;
        buf_mod(rbuf, "LSmoke", 4);
      } else if (CURRENT_VISION(ch) == LOWLIGHT) {
        base_target += 2;
        buf_mod(rbuf, "LSmoke", 2);
      }
    }
    if (world[ch->in_room].vision[1] == LIGHT_HEAVYSMOKE || (weather_info.sky == SKY_LIGHTNING &&
        world[ch->in_room].sector_type != SPIRIT_HEARTH && !ROOM_FLAGGED(ch->in_room, ROOM_INDOORS))) {
      if (CURRENT_VISION(ch) == NORMAL || (CURRENT_VISION(ch) == LOWLIGHT && NATURAL_VISION(ch) == NORMAL)) {
        base_target += 6;
        buf_mod(rbuf, "HSmoke", 6);
      } else if (CURRENT_VISION(ch) == LOWLIGHT) {
        base_target += 4;
        buf_mod(rbuf, "HSmoke", 4);
      } else if (CURRENT_VISION(ch) == THERMOGRAPHIC && NATURAL_VISION(ch) != THERMOGRAPHIC) {
        base_target++;
        buf_mod(rbuf, "HSmoke", 1);
      }
    }
    if (world[ch->in_room].vision[1] == LIGHT_THERMALSMOKE) {
      if (CURRENT_VISION(ch) == NORMAL || CURRENT_VISION(ch) == LOWLIGHT) {
        base_target += 4;
        buf_mod(rbuf, "TSmoke", 4);
      } else {
        if (NATURAL_VISION(ch) == THERMOGRAPHIC) {
          base_target += 6;
          buf_mod(rbuf, "TSmoke", 6);
        } else {
          base_target += 8;
          buf_mod(rbuf, "TSmoke", 8);
        }
      }
    }
  }
  base_target += GET_TARGET_MOD(ch);
  buf_mod( rbuf, "GET_TARGET_MOD", GET_TARGET_MOD(ch) );
  if (GET_RACE(ch) == RACE_NIGHTONE && ((time_info.hours > 6) && (time_info.hours < 19)) && OUTSIDE(ch))
  {
    base_target += 1;
    buf_mod( rbuf, "Sunlight", 1);
  }
  if (world[ch->in_room].poltergeist[0] && !IS_ASTRAL(ch) && !IS_DUAL(ch))
  {
    base_target += 2;
    buf_mod(rbuf, "Polter", 2);
  }
  if (AFF_FLAGGED(ch, AFF_ACID))
  {
    base_target += 4;
    buf_mod(rbuf, "Acid", 4);
  }
  if (ch->points.fire[0] > 0)
  {
    base_target += 4;
    buf_mod(rbuf, "OnFire", 4);
  }
  for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = sust->next)
  {
    if (sust->caster == FALSE && (sust->spell == SPELL_CONFUSION || sust->spell == SPELL_CHAOS)) {
      base_target += MIN(sust->force, sust->success);
      buf_mod(rbuf, "Confused", MIN(sust->force, sust->success));
    }
  }
  if (!(IS_ELEMENTAL(ch) || IS_SPIRIT(ch)))
    for (struct spirit_sustained *sust = SPIRIT_SUST(ch); sust; sust = sust->next)
      if (sust == CONFUSION)
      {
        base_target += GET_LEVEL(sust->target);
        buf_mod(rbuf, "SConfused", GET_LEVEL(sust->target));
      }
  if (ch->in_room != NOWHERE && ROOM_FLAGGED(ch->in_room, ROOM_INDOORS)) {
    float heightdif = GET_HEIGHT(ch) / (world[ch->in_room].z*100);
    if (heightdif > 1) {
      base_target += 2;
      buf_mod(rbuf, "TooTallRatio", (int)(heightdif*100));
    }
    if (heightdif > 1.2)
      base_target += 2;
    if (heightdif > 1.5)
      base_target += 2;
    if (heightdif > 2)
      base_target += 2;
  }
  return base_target;
}

int modify_target(struct char_data *ch)
{
  return modify_target_rbuf(ch, NULL);
}

// this returns the general skill
int return_general(int skill_num)
          {
            switch (skill_num) {
            case SKILL_PISTOLS:
            case SKILL_RIFLES:
            case SKILL_SHOTGUNS:
            case SKILL_ASSAULT_RIFLES:
            case SKILL_SMG:
            case SKILL_GRENADE_LAUNCHERS:
            case SKILL_TASERS:
            case SKILL_MACHINE_GUNS:
            case SKILL_MISSILE_LAUNCHERS:
            case SKILL_ASSAULT_CANNON:
            case SKILL_ARTILLERY:
              return (SKILL_FIREARMS);
            case SKILL_EDGED_WEAPONS:
            case SKILL_POLE_ARMS:
            case SKILL_WHIPS_FLAILS:
            case SKILL_CLUBS:
              return (SKILL_ARMED_COMBAT);
            default:
              return (skill_num);
            }
          }

// capitalize a string, now allows for color strings at the beginning
char *capitalize(const char *source)
{
  static char dest[MAX_STRING_LENGTH];
  strcpy(dest, source);
  
  char* first_actual_char = dest;
  while (*first_actual_char == '^')
    first_actual_char += 2;
  *first_actual_char = UPPER(*first_actual_char);
  
  return dest;
}

// duplicate a string -- uses new!
char *str_dup(const char *source)
{
  if (!source)
    return NULL;

  char *New = new char[strlen(source) + 1];
  sprintf(New, "%s", source);
  return New;
}

// this function runs through 'str' and copies the first token to 'token'.
// it assumes that token is already allocated--it returns a pointer to the
// next char after the token in str
char *get_token(char *str, char *token)
{
  if (!str)
    return NULL;

  register char *temp = str;
  register char *temp1 = token;

  // first eat up any white space
  while (isspace(*temp))
    temp++;

  // now loop through the string and copy each char till we find a space
  while (*temp && !isspace(*temp))
    *temp1++ = *temp++;

  // terminate the string properly
  *temp1 = '\0';

  return temp;
}

/* strips \r's from line -- Chris*/
char *cleanup(char *dest, const char *src)
{
  if (!src) // this is because sometimes a null gets sent to src
    return NULL;

  register char *temp = &dest[0];

  for (; *src; src++)
    if (*src != '\r')
      *temp++ = *src;

  *temp = '\0';
  return dest;
}

/* str_cmp: a case-insensitive version of strcmp */
/* returns: 0 if equal, pos if arg1 > arg2, neg if arg1 < arg2  */
/* scan 'till found different or end of both                 */
int str_cmp(const char *one, const char *two)
{
  if (!*one || !*two)
    return 1;
  for (; *one; one++, two++) {
    int diff = LOWER(*one) - LOWER(*two);

    if (diff!= 0)
      return diff;
  }

  return (LOWER(*one) - LOWER(*two));
}


/* str_str: A case-insensitive version of strstr */
/* returns: A pointer to the first occurance of str2 in str1 */
/* or a null pointer if it isn't found.                      */
char *str_str( const char *str1, const char *str2 )
{
  int i;
  char temp1[MAX_INPUT_LENGTH], temp2[MAX_INPUT_LENGTH];

  for ( i = 0; *(str1 + i); i++ ) {
    temp1[i] = LOWER(*(str1 + i));
  }

  temp1[i] = '\0';

  for ( i = 0; *(str2 + i); i++ ) {
    temp2[i] = LOWER(*(str2 + i));
  }

  temp2[i] = '\0';

  return (strstr(temp1, temp2));
}


/* strn_cmp: a case-insensitive version of strncmp */
/* returns: 0 if equal, 1 if arg1 > arg2, -1 if arg1 < arg2  */
/* scan 'till found different, end of both, or n reached     */
int strn_cmp(const char *arg1, const char *arg2, int n)
{
  int chk, i;

  if (arg1 == NULL || arg2 == NULL) {
    log_vfprintf("SYSERR: strn_cmp() passed a NULL pointer, %p or %p.", arg1, arg2);
    return (0);
  }

  for (i = 0; (arg1[i] || arg2[i]) && (n > 0); i++, n--)
    if ((chk = LOWER(arg1[i]) - LOWER(arg2[i])) != 0)
      return (chk); /* not equal */

  return (0);
}

/* returns 1 if the character has a cyberweapon; 0 otherwise */
bool has_cyberweapon(struct char_data * ch)
{
  struct obj_data *obj = NULL;
  for (obj = ch->cyberware;
       obj ;
       obj = obj->next_content)
    if ((GET_OBJ_VAL(obj, 0) == CYB_HANDSPUR || GET_OBJ_VAL(obj, 0) == CYB_HANDRAZOR || GET_OBJ_VAL(obj, 0) == CYB_HANDBLADE || GET_OBJ_VAL(obj, 0) == CYB_FIN || GET_OBJ_VAL(obj, 0) == CYB_CLIMBINGCLAWS)
        && !GET_OBJ_VAL(obj, 9))
      return TRUE;
  return 0;
}


/* log a death trap hit */
void log_death_trap(struct char_data * ch)
{
  char buf[150];
  extern struct room_data *world;

  sprintf(buf, "%s hit DeathTrap #%ld (%s)", GET_CHAR_NAME(ch),
          world[ch->in_room].number, world[ch->in_room].name);
  mudlog(buf, ch, LOG_DEATHLOG, TRUE);
}

void log(const char *str) {
  va_list args;
  time_t ct = time(0);
  char *tmstr;
  
  tmstr = asctime(localtime(&ct));
  *(tmstr + strlen(tmstr) - 1) = '\0';
  fprintf(stderr, "%-15.15s :: %s\n", tmstr + 4, str);
}

void log_vfprintf(const char *format, ...)
{
  va_list args;
  time_t ct = time(0);
  char *tmstr;

  tmstr = asctime(localtime(&ct));
  *(tmstr + strlen(tmstr) - 1) = '\0';
  fprintf(stderr, "%-15.15s :: ", tmstr + 4);

  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  fprintf(stderr, "\n");
}

/*
 * mudlog -- log mud messages to a file & to online imm's syslogs
 * based on syslog by Fen Jul 3, 1992
 */
void mudlog(const char *str, struct char_data *ch, int log, bool file)
{
  char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];
  extern struct descriptor_data *descriptor_list;
  struct descriptor_data *i;
  struct char_data *tch;
  char *tmp;
  time_t ct;
  int check_log = 0;

  ct = time(0);
  tmp = asctime(localtime(&ct));

  if ( ch && ch->desc && ch->desc->original )
    sprintf(buf2, "[%5ld] (%s) ",
            world[ch->in_room].number,
            GET_CHAR_NAME(ch));
  else if (ch && ch->in_room != NOWHERE)
    sprintf(buf2, "[%5ld] ", world[ch->in_room].number);
  else
    strcpy(buf2, "");

  if (file)
    fprintf(stderr, "%-19.19s :: %s: %s%s\n", tmp, log_types[log], buf2, str);

  ct = ct;
  sprintf(buf, "^g[%s: %s%s]^n\r\n", log_types[log], buf2, str);

  for (i = descriptor_list; i; i = i->next)
    if (!i->connected)
    {
      if (i->original)
        tch = i->original;
      else
        tch = i->character;
      if (!tch ||
          PLR_FLAGS(tch).AreAnySet(PLR_WRITING, PLR_MAILING,
                                   PLR_EDITING, ENDBIT))
        continue;

      if (ch
          && !access_level(tch, GET_INVIS_LEV(ch))
          && !access_level(tch, LVL_VICEPRES))
        continue;
      switch (log) {
      case LOG_CONNLOG:
        check_log = PRF_CONNLOG;
        break;
      case LOG_DEATHLOG:
        check_log = PRF_DEATHLOG;
        break;
      case LOG_MISCLOG:
        check_log = PRF_MISCLOG;
        break;
      case LOG_WIZLOG:
        check_log = PRF_WIZLOG;
        break;
      case LOG_SYSLOG:
        check_log = PRF_SYSLOG;
        break;
      case LOG_ZONELOG:
        check_log = PRF_ZONELOG;
        break;
      case LOG_CHEATLOG:
        check_log = PRF_CHEATLOG;
        break;
      case LOG_WIZITEMLOG:
        check_log = PRF_CHEATLOG;
        break;
      case LOG_BANLOG:
        check_log = PRF_BANLOG;
        break;
      case LOG_GRIDLOG:
        check_log = PRF_GRIDLOG;
        break;
      case LOG_WRECKLOG:
        check_log = PRF_WRECKLOG;
        break;
      }
      if (PRF_FLAGGED(tch, check_log))
        SEND_TO_Q(buf, i);
    }
}

void sprintbit(long vektor, const char *names[], char *result)
{
  long nr;

  *result = '\0';

  if (vektor < 0) {
    strcpy(result, "SPRINTBIT ERROR!");
    return;
  }
  for (nr = 0; vektor; vektor >>= 1) {
    if (IS_SET(1, vektor)) {
      if (*names[nr] != '\n') {
        strcat(result, names[nr]);
        strcat(result, " ");
      } else
        strcat(result, "UNDEFINED ");
    }
    if (*names[nr] != '\n')
      nr++;
  }

  if (!*result)
    strcat(result, "None ");
}

void sprinttype(int type, const char *names[], char *result)
{
  sprintf(result, "%s", names[type]);
  
  if (str_cmp(result, "(null)") == 0) {
    sprintf(result, "UNDEFINED");
  }
}

void sprint_obj_mods(struct obj_data *obj, char *result)
{
  *result = 0;
  if (obj->obj_flags.bitvector.GetNumSet() > 0)
  {
    char xbuf[MAX_STRING_LENGTH];
    obj->obj_flags.bitvector.PrintBits(xbuf, MAX_STRING_LENGTH,
                                       affected_bits, AFF_MAX);
    sprintf(result,"%s %s", result, xbuf);
  }

  for (register int i = 0; i < MAX_OBJ_AFFECT; i++)
    if (obj->affected[i].modifier != 0)
    {
      char xbuf[MAX_STRING_LENGTH];
      sprinttype(obj->affected[i].location, apply_types, xbuf);
      sprintf(result,"%s (%+d %s)",
              result, obj->affected[i].modifier, xbuf);
    }
  return;
}

/* Calculate the REAL time passed over the last t2-t1 centuries (secs) */
struct time_info_data real_time_passed(time_t t2, time_t t1)
{
  long secs;
  struct time_info_data now;

  secs = (long) (t2 - t1);

  now.hours = (secs / SECS_PER_REAL_HOUR) % 24; /* 0..23 hours */
  secs -= SECS_PER_REAL_HOUR * now.hours;

  now.day = (secs / SECS_PER_REAL_DAY); /* 0..34 days  */
  secs -= SECS_PER_REAL_DAY * now.day;

  now.month = 0;
  now.year = 0;

  return now;
}

/* Calculate the MUD time passed over the last t2-t1 centuries (secs) */
struct time_info_data mud_time_passed(time_t t2, time_t t1)
{
  long secs;
  struct time_info_data now;

  secs = (long) (t2 - t1);

  now.hours = (secs / SECS_PER_MUD_HOUR) % 24;  /* 0..23 hours */
  secs -= SECS_PER_MUD_HOUR * now.hours;

  now.day = (secs / SECS_PER_MUD_DAY) % 30;     /* 0..34 days  */
  secs -= SECS_PER_MUD_DAY * now.day;

  now.month = (secs / SECS_PER_MUD_MONTH) % 12; /* 0..16 months */
  secs -= SECS_PER_MUD_MONTH * now.month;

  now.year = (secs / SECS_PER_MUD_YEAR);        /* 0..XX? years */

  return now;
}

bool access_level(struct char_data *ch, int level)
{
  ch = ch->desc && ch->desc->original ? ch->desc->original : ch;

  return (!IS_NPC(ch)
          && (GET_LEVEL(ch) >= level ));
}

/* Check if making CH follow VICTIM will create an illegal */
/* Follow "Loop/circle"                                    */
bool circle_follow(struct char_data * ch, struct char_data * victim)
{
  struct char_data *k;

  for (k = victim; k; k = k->master)
  {
    if (k == ch)
      return TRUE;
  }

  return FALSE;
}

/* Called when stop following persons, or stopping charm */
/* This will NOT do if a character quits/dies!!          */
void stop_follower(struct char_data * ch)
{
  struct follow_type *j, *k;

  if (ch->master->followers->follower == ch)
  {  /* Head of follower-list? */
    k = ch->master->followers;
    ch->master->followers = k->next;
    delete k;
  } else
  {                      /* locate follower who is not head of list */
    for (k = ch->master->followers; k->next->follower != ch; k = k->next)
      ;

    j = k->next;
    k->next = j->next;
    delete j;
  }

  AFF_FLAGS(ch).RemoveBits(AFF_CHARM, AFF_GROUP, ENDBIT);
  act("You stop following $N.", FALSE, ch, 0, ch->master, TO_CHAR);
  act("$n stops following $N.", TRUE, ch, 0, ch->master, TO_NOTVICT);
  act("$n stops following you.", TRUE, ch, 0, ch->master, TO_VICT);
  ch->master = NULL;
}

/* Called when a character that follows/is followed dies */
void die_follower(struct char_data * ch)
{
  struct follow_type *j, *k;

  if (ch->master)
    stop_follower(ch);

  for (k = ch->followers; k; k = j)
  {
    j = k->next;
    stop_follower(k->follower);
  }
}

/* Do NOT call this before having checked if a circle of followers */
/* will arise. CH will follow leader                               */
void add_follower(struct char_data * ch, struct char_data * leader)
{
  struct follow_type *k;

  ch->master = leader;

  k = new follow_type;

  k->follower = ch;
  k->next = leader->followers;
  leader->followers = k;
  if (!IS_SPIRIT(ch) && !IS_ELEMENTAL(ch))
  {
    act("You now follow $N.", FALSE, ch, 0, leader, TO_CHAR);
    if (CAN_SEE(leader, ch))
      act("$n starts following you.", TRUE, ch, 0, leader, TO_VICT);
    act("$n starts to follow $N.", TRUE, ch, 0, leader, TO_NOTVICT);
  }
}

/*
 * get_line reads the next non-blank line off of the input stream.
 * The newline character is removed from the input.  Lines which begin
 * with '*' are considered to be comments.
 *
 * Returns the number of lines advanced in the file.
 */
int get_line(FILE * fl, char *buf)
{
  char temp[256];
  int lines = 0;

  do {
    lines++;
    fgets(temp, 256, fl);
    if (*temp)
      temp[strlen(temp) - 1] = '\0';
  } while (!feof(fl) && (*temp == '*' || !*temp));

  if (feof(fl))
    return 0;
  else {
    strcpy(buf, temp);
    return lines;
  }
}

bool PRF_TOG_CHK(char_data *ch, dword offset)
{
  PRF_FLAGS(ch).ToggleBit(offset);

  return PRF_FLAGS(ch).IsSet(offset);
}

bool PLR_TOG_CHK(char_data *ch, dword offset)
{
  PLR_FLAGS(ch).ToggleBit(offset);

  return PLR_FLAGS(ch).IsSet(offset);
}

char * buf_mod(char *rbuf, const char *name, int bonus)
{
  if ( !rbuf )
    return rbuf;
  if ( bonus == 0 )
    return rbuf;
  rbuf += strlen(rbuf);
  if ( bonus > 0 )
    sprintf(rbuf, "%s +%d, ", name, bonus);
  else
    sprintf(rbuf, "%s %d, ", name, bonus);
  rbuf += strlen(rbuf);
  return rbuf;
}

char * buf_roll(char *rbuf, const char *name, int bonus)
{
  if ( !rbuf )
    return rbuf;
  rbuf += strlen(rbuf);
  sprintf(rbuf, " [%s %d]", name, bonus);
  return rbuf;
}

int get_speed(struct veh_data *veh)
{
  int speed = 0, maxspeed = (int)(veh->speed * ((veh->load - veh->usedload) / veh->load));

  switch (veh->cspeed)
  {
  case SPEED_OFF:
  case SPEED_IDLE:
    speed = 0;
    break;
  case SPEED_CRUISING:
    if (ROOM_FLAGGED(veh->in_room, ROOM_INDOORS))
      speed = MIN(maxspeed, 3);
    else
      speed = MIN(maxspeed, 55);
    break;
  case SPEED_SPEEDING:
    if (ROOM_FLAGGED(veh->in_room, ROOM_INDOORS))
      speed = MIN(maxspeed, MAX(5, (int)(maxspeed * .7)));
    else
      speed = MIN(maxspeed, MAX(55, (int)(maxspeed * .7)));
    break;
  case SPEED_MAX:
    if (ROOM_FLAGGED(veh->in_room, ROOM_INDOORS))
      speed = MIN(maxspeed, 8);
    else
      speed = maxspeed;
    break;
  }
  return (speed);
}

int negotiate(struct char_data *ch, struct char_data *tch, int comp, int basevalue, int mod, bool buy)
{
  struct obj_data *bio;
  int cmod = 0, tmod = 0;
  int cskill = get_skill(ch, SKILL_NEGOTIATION, cmod);
  cmod -= GET_POWER(ch, ADEPT_KINESICS);
  tmod -= GET_POWER(tch, ADEPT_KINESICS);
  if (GET_RACE(ch) != GET_RACE(tch)) {
    switch (GET_RACE(ch)) {
      case RACE_HUMAN:
      case RACE_ELF:
      case RACE_ORK:
      case RACE_TROLL:
      case RACE_DWARF:
        break;
      default:
        cmod += 4;
        break;
    }
    switch (GET_RACE(tch)) {
      case RACE_HUMAN:
      case RACE_ELF:
      case RACE_ORK:
      case RACE_TROLL:
      case RACE_DWARF:
        break;
      default:
        tmod += 4;
        break;
    }
  } 

  for (bio = ch->bioware; bio; bio = bio->next_content)
    if (GET_OBJ_VAL(bio, 0) == BIO_TAILOREDPHEREMONES)
    {
      cskill += GET_OBJ_VAL(bio, 2) ? GET_OBJ_VAL(bio, 1) * 2: GET_OBJ_VAL(bio, 1);
      break;
    }
  int tskill = get_skill(tch, SKILL_NEGOTIATION, tmod);

  for (bio = tch->bioware; bio; bio = bio->next_content)
    if (GET_OBJ_VAL(bio, 0) == BIO_TAILOREDPHEREMONES)
    {
      tskill += GET_OBJ_VAL(bio, 2) ? GET_OBJ_VAL(bio, 1) * 2: GET_OBJ_VAL(bio, 1);
      break;
    }
  int chnego = success_test(cskill, GET_INT(tch)+mod+cmod);
  int tchnego = success_test(tskill, GET_INT(ch)+mod+tmod);
  if (comp)
  {
    chnego += success_test(GET_SKILL(ch, comp), GET_INT(tch)+mod+cmod) / 2;
    tchnego += success_test(GET_SKILL(tch, comp), GET_INT(ch)+mod+tmod) / 2;
  }
  int num = chnego - tchnego;
  if (num > 0)
  {
    if (buy)
      basevalue = MAX((int)(basevalue * 3/4), basevalue - (num * (basevalue / 20)));
    else
      basevalue = MIN((int)(basevalue * 5/4), basevalue + (num * (basevalue / 15)));
  } else
  {
    if (buy)
      basevalue = MIN((int)(basevalue * 5/4), basevalue + (num * (basevalue / 15)));
    else
      basevalue = MAX((int)(basevalue * 3/4), basevalue - (num * (basevalue / 20)));
  }
  return basevalue;

}

int get_skill(struct char_data *ch, int skill, int &target)
{
  // Wearing too much armor? That'll hurt.
  if (skills[skill].attribute == QUI) {
    if (GET_TOTALIMP(ch) > GET_QUI(ch))
      target += GET_TOTALIMP(ch) - GET_QUI(ch);
    if (GET_TOTALBAL(ch) > GET_QUI(ch))
      target += GET_TOTALBAL(ch) - GET_QUI(ch);
  }
  
  if (GET_SKILL(ch, skill))
  {
    int totalskill, mbw = 0, enhan = 0, synth = 0;
    totalskill = GET_SKILL(ch, skill);
    if (REAL_SKILL(ch, skill) == GET_SKILL(ch, skill))
      totalskill += MIN(REAL_SKILL(ch, skill), GET_TASK_POOL(ch, skills[skill].attribute));
    else if (ch->cyberware) {
      int expert = 0;
      bool chip = FALSE;;
      for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content)
        if (GET_OBJ_VAL(obj, 0) == CYB_MOVEBYWIRE)
          mbw = GET_OBJ_VAL(obj, 1);
        else if (GET_OBJ_VAL(obj, 0) == CYB_CHIPJACKEXPERT)
          expert = GET_OBJ_VAL(obj, 1);
        else if (GET_OBJ_VAL(obj, 0) == CYB_CHIPJACK)
          for (int i = 5; i < 10; i++)
            if (real_object(GET_OBJ_VAL(obj, i)) && obj_proto[real_object(GET_OBJ_VAL(obj, i))].obj_flags.value[0] == skill)
              chip = TRUE;
      if (chip && expert)
        totalskill += MIN(REAL_SKILL(ch, skill), expert);
    } else if (ch->bioware) {
      for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content) {
        if (GET_OBJ_VAL(bio, 0) == BIO_REFLEXRECORDER && GET_OBJ_VAL(bio, 3) == skill)
          totalskill++;
        else if (GET_OBJ_VAL(bio, 0) == BIO_ENHANCEDARTIC)
          enhan = TRUE;
        else if (GET_OBJ_VAL(bio, 0) == BIO_SYNTHACARDIUM)
          synth = GET_OBJ_VAL(bio, 1);
      }
    }
    if (skill == SKILL_STEALTH)
      totalskill += mbw;
    if (skill == SKILL_ATHLETICS)
      totalskill += synth + mbw;
    if (enhan && (skills[skill].attribute == QUI || skill == SKILL_BR_CAR || skill == SKILL_BR_BIKE ||
        skill == SKILL_BR_COMPUTER || skill == SKILL_BR_ELECTRONICS || skill == SKILL_BR_TRUCK ||
        skill == SKILL_BR_DRONE))
      totalskill++;
    return totalskill;
  } else
  {
    if (target >= 8)
      return 0;
    target += 4;
    return GET_ATT(ch, skills[skill].attribute);
  }
}

bool biocyber_compatibility(struct obj_data *obj1, struct obj_data *obj2, struct char_data *ch)
{
  struct obj_data *cyber1 = NULL, *cyber2 = NULL, *bio1 = NULL;
  if (GET_OBJ_TYPE(obj1) == ITEM_CYBERWARE)
    cyber1 = obj1;
  else
    bio1 = obj1;
  if (GET_OBJ_TYPE(obj2) == ITEM_BIOWARE)
      bio1 = obj2;
  else {
    if (cyber1)
      cyber2 = obj2;
    else
      cyber1 = obj2;
  }
  if (cyber1 && cyber2) {
    if (GET_OBJ_VAL(cyber1, 0) != CYB_EYES)
      switch (GET_OBJ_VAL(cyber1, 0)) {
      case CYB_FILTRATION:
        if (GET_OBJ_VAL(cyber1, 3) == GET_OBJ_VAL(cyber2, 3)) {
          send_to_char("You already have this type of filtration installed.\r\n", ch);
          return FALSE;
        }
        break;
      case CYB_DATAJACK:
        if (GET_OBJ_VAL(cyber2, 0) == CYB_EYES && IS_SET(GET_OBJ_VAL(cyber2, 3), EYE_DATAJACK)) {
          send_to_char("You already have an eye datajack installed.\r\n", ch);
          return FALSE;
        }
        break;
      case CYB_VCR:
        if (GET_OBJ_VAL(cyber2, 0) == CYB_BOOSTEDREFLEXES) {
          send_to_char("Vehicle Control Rigs are not compatible with Boosted reflexes.\r\n", ch);
          return FALSE;
        }
        break;
      case CYB_BOOSTEDREFLEXES:
        if (GET_OBJ_VAL(cyber2, 0) == CYB_VCR) {
          send_to_char("Boosted reflexes is not compatible with Vehicle Control Rigs.\r\n", ch);
          return FALSE;
        }
      case CYB_MOVEBYWIRE:
      case CYB_WIREDREFLEXES:
        if (GET_OBJ_VAL(cyber2, 0) == CYB_WIREDREFLEXES || GET_OBJ_VAL(cyber2, 0) == CYB_MOVEBYWIRE ||
            GET_OBJ_VAL(cyber2, 0) == CYB_BOOSTEDREFLEXES) {
          send_to_char("You already have reaction enhancing cyberware installed.\r\n", ch);
          return FALSE;
        }
        if (GET_OBJ_VAL(cyber1, 0) == CYB_MOVEBYWIRE && GET_OBJ_VAL(cyber2, 0) == CYB_REACTIONENHANCE) {
          send_to_char("Move-by-wire is not compatible with reaction enhancers.\r\n", ch);
          return FALSE;
        }
        break;
      case CYB_ORALSLASHER:
      case CYB_ORALDART:
      case CYB_ORALGUN:
        if (GET_OBJ_VAL(cyber2, 0) == CYB_ORALSLASHER || GET_OBJ_VAL(cyber2, 0) == CYB_ORALDART || GET_OBJ_VAL(cyber2, 0)
            == CYB_ORALGUN) {
          send_to_char("You already have a weapon in your mouth.\r\n", ch);
          return FALSE;
        }
        break;
      case CYB_DERMALPLATING:
      case CYB_DERMALSHEATHING:
        if (GET_OBJ_VAL(cyber2, 0) == CYB_DERMALPLATING || GET_OBJ_VAL(cyber2, 0) == CYB_DERMALSHEATHING) {
          send_to_char("You already have an skin modifiaction.\r\n", ch);
          return FALSE;
        }
        break;
      case CYB_REACTIONENHANCE:
        if (GET_OBJ_VAL(cyber2, 0) == CYB_MOVEBYWIRE) {
          send_to_char("Reaction enhancers are not compatible with Move-by-wire.\r\n", ch);
          return FALSE;
        }
        break;
      }
    if (IS_SET(GET_OBJ_VAL(cyber1, 3), EYE_DATAJACK) && GET_OBJ_VAL(cyber2, 0) == CYB_DATAJACK) {
      send_to_char("You already have a datajack installed.\r\n", ch);
      return FALSE;
    }
    if (GET_OBJ_VAL(cyber2, 0) == CYB_EYES && GET_OBJ_VAL(cyber1, 0) == CYB_EYES)
      for (int bit = EYE_CAMERA; bit <= EYE_ULTRASOUND; bit *= 2) {
        if (IS_SET(GET_OBJ_VAL(cyber2, 3), bit) && IS_SET(GET_OBJ_VAL(cyber1, 3), bit)) {
          send_to_char("You already have eye modifications with this option installed.\r\n", ch);
          return FALSE;
        }
        if (bit >= EYE_OPTMAG1 && bit <= EYE_OPTMAG3 && IS_SET(GET_OBJ_VAL(cyber1, 3), bit))
          if (IS_SET(GET_OBJ_VAL(cyber2, 3), EYE_ELECMAG1) || IS_SET(GET_OBJ_VAL(cyber2, 3), EYE_ELECMAG2) || IS_SET(GET_OBJ_VAL(cyber2, 3), EYE_ELECMAG3)) {
            send_to_char("Optical magnification is not compatible with electronic magnification.\r\n", ch);
            return FALSE;
          }
        if (bit >= EYE_ELECMAG1 && bit <= EYE_ELECMAG3 && IS_SET(GET_OBJ_VAL(cyber1, 3), bit))
          if (IS_SET(GET_OBJ_VAL(cyber2, 3), EYE_OPTMAG1) || IS_SET(GET_OBJ_VAL(cyber2, 3), EYE_OPTMAG2) || IS_SET(GET_OBJ_VAL(cyber2, 3), EYE_OPTMAG3)) {
            send_to_char("Optical magnification is not compatible with electronic magnification.\r\n", ch);
            return FALSE;
          }
      }
  } else if (bio1 && cyber1) {
    switch (GET_OBJ_VAL(cyber1, 0)) {
    case CYB_FILTRATION:
      if (GET_OBJ_VAL(bio1, 0) == BIO_TRACHEALFILTER && GET_OBJ_VAL(cyber1, 3) == FILTER_AIR) {
        send_to_char("Air filtration cyberware is not compatible with a Tracheal Filter.\r\n", ch);
        return FALSE;
      }
      if (GET_OBJ_VAL(bio1, 0) == BIO_DIGESTIVEEXPANSION && GET_OBJ_VAL(cyber1, 3) == FILTER_INGESTED) {
        send_to_char("Tracheal Filtration cyberware is not compatible with Digestive Expansion.\r\n", ch);
        return FALSE;
      }
      break;
    case CYB_MOVEBYWIRE:
      if (GET_OBJ_VAL(bio1, 0) == BIO_ADRENALPUMP) {
        send_to_char("Adrenal Pumps are not compatible with Move-By-Wire.\r\n", ch);
        return FALSE;
      }
      if (GET_OBJ_VAL(bio1, 0) == BIO_SYNAPTICACCELERATOR) {
        send_to_char("Synaptic Accelerators are not compatible with Move-By-Wire.\r\n", ch);
        return FALSE;
      }
      if (GET_OBJ_VAL(bio1, 0) == BIO_SUPRATHYROIDGLAND) {
        send_to_char("Suprathyroid Glands are not compatible with Move-By-Wire.\r\n", ch);
        return FALSE;
      }
      break;
    case CYB_WIREDREFLEXES:
      if (GET_OBJ_VAL(bio1, 0) == BIO_SYNAPTICACCELERATOR) {
        send_to_char("Your Synaptic Accelerator is not compatible with Wired Reflexes.\r\n", ch);
        return FALSE;
      }
      break;
    case CYB_EYES:
      if (GET_OBJ_VAL(bio1, 0) == BIO_CATSEYES || GET_OBJ_VAL(bio1, 0) == BIO_NICTATINGGLAND) {
        send_to_char("Bioware and cyberware eye modifications aren't compatible.\r\n", ch);
        return FALSE;
      }
      break;
    case CYB_MUSCLEREP:
      if (GET_OBJ_VAL(bio1, 0) == BIO_MUSCLEAUG || GET_OBJ_VAL(bio1, 0) == BIO_MUSCLETONER) {
        send_to_char("Muscle replacement isn't compatible with Muscle Augmentation or Toners.\r\n", ch);
        return FALSE;
      }
      break;
    case CYB_DERMALPLATING:
    case CYB_DERMALSHEATHING:
      if (GET_OBJ_VAL(bio1, 0) == BIO_ORTHOSKIN) {
        send_to_char("Orthoskin is not compatible with Dermal Plating or Sheathing.\r\n", ch);
        return FALSE;
      }
      break;
    }
  }
  return TRUE;
}

void reduce_abilities(struct char_data *vict)
{
  int i;

  for (i = 0; i < ADEPT_NUMPOWER; i++)
    if (GET_POWER_TOTAL(vict, i) > GET_MAG(vict) / 100) {
      GET_PP(vict) += ability_cost(i, GET_POWER_TOTAL(vict, i));
      GET_POWER_TOTAL(vict, i)--;
      send_to_char(vict, "Your loss in magic makes you feel less "
                   "skilled in %s.\r\n", adept_powers[i]);
    }

  if (GET_PP(vict) >= 0)
    return;

  for (i = number(1, ADEPT_NUMPOWER); GET_PP(vict) < 0;
       i = number(1, ADEPT_NUMPOWER))
  {
    if (GET_POWER_TOTAL(vict, i) > 0) {
      GET_PP(vict) += ability_cost(i, GET_POWER_TOTAL(vict, i));
      GET_POWER_TOTAL(vict, i)--;
      send_to_char(vict, "Your loss in magic makes you feel less "
                   "skilled in %s.\r\n", adept_powers[i]);
    }
    int y = 0;
    for (int x = 0; x < ADEPT_NUMPOWER; x++)
      if (GET_POWER_TOTAL(vict, x)) 
        y += ability_cost(x, GET_POWER_TOTAL(vict, x));
    if (y < 1)
      return;
  }  
}
  
void magic_loss(struct char_data *ch, int magic, bool msg)
{
  if (GET_TRADITION(ch) == TRAD_MUNDANE)
    return;
  GET_REAL_MAG(ch) = MAX(0, GET_REAL_MAG(ch) - magic);
  if (GET_REAL_MAG(ch) < 100) {
    send_to_char(ch, "You feel the last of your magic leave your body.\r\n", ch);
    PLR_FLAGS(ch).RemoveBit(PLR_PERCEIVE);
    GET_TRADITION(ch) = TRAD_MUNDANE;
    sprintf(buf, "DELETE FROM pfiles_magic WHERE idnum=%ld;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    sprintf(buf, "UPDATE pfiles SET Tradition=%d WHERE idnum=%ld;", TRAD_MUNDANE, GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    for (int i = 0; i < NUM_WEARS; i++)
      if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_FOCUS && GET_OBJ_VAL(GET_EQ(ch, i), 2) == GET_IDNUM(ch))
        GET_OBJ_VAL(GET_EQ(ch, i), 2) = GET_OBJ_VAL(GET_EQ(ch, i), 4) =  GET_OBJ_VAL(GET_EQ(ch, i), 9) = 0;
    struct sustain_data *nextsust;
    for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = nextsust) {
      nextsust = sust->next;
      if (sust->caster)
        end_sustained_spell(ch, sust);
    }
    return;
  }
  if (msg)
    send_to_char(ch, "You feel some of your magic leave your body.\r\n", ch);
  if (GET_TRADITION(ch) == TRAD_ADEPT) {
    GET_PP(ch) -= magic;
    reduce_abilities(ch);
  }
}

