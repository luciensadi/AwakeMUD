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
#include "list.h"

extern class memoryClass *Mem;
extern struct time_info_data time_info;

extern void die(struct char_data * ch);
extern const char *log_types[];
extern long beginning_of_time;
extern int ability_cost(int abil, int level);
extern MYSQL *mysql;
extern int mysql_wrapper(MYSQL *mysql, const char *buf);
extern void weight_change_object(struct obj_data * obj, float weight);

extern char *colorize(struct descriptor_data *d, const char *str, bool skip_check = FALSE);

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
  
  if (to > RAND_MAX) {
    char errbuf[150];
    sprintf(errbuf, "WARNING: Attempting to generate random number between %d and %d, but RAND_MAX is %d!", from, to, RAND_MAX);
    mudlog(errbuf, NULL, LOG_SYSLOG, TRUE);
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
  int i;
  
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
  int i;
  
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
    damage = damage_array[MIN(DEADLY, damage)];
  
  return damage;
}

int light_level(struct room_data *room)
{
  if (!room) {
    mudlog("SYSERR: light_level() called on null room.", NULL, LOG_SYSLOG, TRUE);
    return LIGHT_NORMAL;
  }
  if (room->sector_type == SPIRIT_HEARTH)
    return room->vision[0];
  if (room->sector_type == SPIRIT_CITY) {
    if ((time_info.hours > 6 && time_info.hours < 19))
      return room->vision[0];
    else if (room->vision[0] == LIGHT_NORMALNOLIT)
      return LIGHT_MINLIGHT;
    else
      return LIGHT_PARTLIGHT;
  }
  if ((time_info.hours < 6 && time_info.hours > 19) && (room->vision[0] > LIGHT_MINLIGHT || room->vision[0] <= LIGHT_NORMALNOLIT))
    return LIGHT_MINLIGHT;
  else
    return room->vision[0];
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

// Adds the combat_mode toggle
int modify_target_rbuf_raw(struct char_data *ch, char *rbuf, int current_visibility_penalty) {
  extern time_info_data time_info;
  int base_target = 0, light_target = 0;
  base_target += damage_modifier(ch, rbuf);
  // then apply modifiers for sustained spells
  if (GET_SUSTAINED_NUM(ch) > 0)
  {
    base_target += ((GET_SUSTAINED_NUM(ch) - GET_SUSTAINED_FOCI(ch)) * 2);
    buf_mod( rbuf, "Sustain", (GET_SUSTAINED_NUM(ch) - GET_SUSTAINED_FOCI(ch)) * 2);
  }
  
  struct room_data *temp_room = get_ch_in_room(ch);
  
  if (PLR_FLAGGED(ch, PLR_PERCEIVE))
  {
    base_target += 2;
    buf_mod(rbuf, "AstralPercep", 2);
  } else if (current_visibility_penalty < 8) {
    switch (light_level(temp_room)) {
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
    if (light_target > 0 && temp_room->light[1]) {
      if (temp_room->light[2]) {
        light_target = MAX(0, light_target - temp_room->light[2]);
        buf_mod(rbuf, "LightSpell", - temp_room->light[2]);
      } else
        light_target /= 2;
    }
    if (temp_room->shadow[0]) {
      light_target += temp_room->shadow[1];
      buf_mod(rbuf, "ShadowSpell", temp_room->shadow[1]);
    }
    int smoke_target = 0;
    
    if (temp_room->vision[1] == LIGHT_MIST)
      if (CURRENT_VISION(ch) == NORMAL || (CURRENT_VISION(ch) == LOWLIGHT && NATURAL_VISION(ch) == LOWLIGHT)) {
        smoke_target += 2;
        buf_mod(rbuf, "Mist", 2);
      }
    if (temp_room->vision[1] == LIGHT_LIGHTSMOKE || (weather_info.sky == SKY_RAINING &&
                                                             temp_room->sector_type != SPIRIT_HEARTH && !ROOM_FLAGGED(temp_room, ROOM_INDOORS))) {
      if (CURRENT_VISION(ch) == NORMAL || (CURRENT_VISION(ch) == LOWLIGHT && NATURAL_VISION(ch) != LOWLIGHT)) {
        smoke_target += 4;
        buf_mod(rbuf, "LSmoke", 4);
      } else if (CURRENT_VISION(ch) == LOWLIGHT) {
        smoke_target += 2;
        buf_mod(rbuf, "LSmoke", 2);
      }
    }
    if (temp_room->vision[1] == LIGHT_HEAVYSMOKE || (weather_info.sky == SKY_LIGHTNING &&
                                                             temp_room->sector_type != SPIRIT_HEARTH && !ROOM_FLAGGED(temp_room, ROOM_INDOORS))) {
      if (CURRENT_VISION(ch) == NORMAL || (CURRENT_VISION(ch) == LOWLIGHT && NATURAL_VISION(ch) == NORMAL)) {
        smoke_target += 6;
        buf_mod(rbuf, "HSmoke", 6);
      } else if (CURRENT_VISION(ch) == LOWLIGHT) {
        smoke_target += 4;
        buf_mod(rbuf, "HSmoke", 4);
      } else if (CURRENT_VISION(ch) == THERMOGRAPHIC && NATURAL_VISION(ch) != THERMOGRAPHIC) {
        smoke_target++;
        buf_mod(rbuf, "HSmoke", 1);
      }
    }
    if (temp_room->vision[1] == LIGHT_THERMALSMOKE) {
      if (CURRENT_VISION(ch) == NORMAL || CURRENT_VISION(ch) == LOWLIGHT) {
        smoke_target += 4;
        buf_mod(rbuf, "TSmoke", 4);
      } else {
        if (NATURAL_VISION(ch) == THERMOGRAPHIC) {
          smoke_target += 6;
          buf_mod(rbuf, "TSmoke", 6);
        } else {
          smoke_target += 8;
          buf_mod(rbuf, "TSmoke", 8);
        }
      }
    }
    // The maximum visibility penalty we apply is +8 TN to avoid things like an invisible person in a smoky pitch-black room getting +24 to hit TN.
    if (light_target + smoke_target + current_visibility_penalty > 8) {
      buf_mod(rbuf, "ButVisPenaltyMaxIs8", (8 - current_visibility_penalty) - light_target + smoke_target);
      base_target += 8 - current_visibility_penalty;
    } else
      base_target += light_target + smoke_target;
  }
  base_target += GET_TARGET_MOD(ch);
  buf_mod( rbuf, "GET_TARGET_MOD", GET_TARGET_MOD(ch) );
  if (GET_RACE(ch) == RACE_NIGHTONE && ((time_info.hours > 6) && (time_info.hours < 19)) && OUTSIDE(ch))
  {
    base_target += 1;
    buf_mod( rbuf, "Sunlight", 1);
  }
  if (temp_room->poltergeist[0] && !IS_ASTRAL(ch) && !IS_DUAL(ch))
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
  if (temp_room && ROOM_FLAGGED(temp_room, ROOM_INDOORS)) {
    float heightdif = GET_HEIGHT(ch) / ((temp_room->z != 0 ? temp_room->z : 1)*100);
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

int modify_target_rbuf(struct char_data *ch, char *rbuf)
{
  return modify_target_rbuf_raw(ch, rbuf, 0);
}

int modify_target(struct char_data *ch)
{
  return modify_target_rbuf_raw(ch, NULL, 0);
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
  
  int len = strlen(source);
  int index = 0;
  
  while (index < len-2 && *(source + index) == '^')
    index += 2;
  *(dest + index) = UPPER(*(source + index));
  
  return dest;
}

char *string_to_uppercase(const char *source) {
  static char dest[MAX_STRING_LENGTH];
  
  int x = strlen(source);
  for (int i = 0; i < x; i++){
    if (isalpha(source[i])){
      dest[i] = toupper(source[i]);
    }
  }
  
  return dest;
}

// decapitalize a string that starts with A or An, now allows for color strings at the beginning
char *decapitalize_a_an(const char *source)
{
  static char dest[MAX_STRING_LENGTH];
  strcpy(dest, source);
  
  int len = strlen(source);
  int index = 0;
  
  while (*(source + index) == '^')
    index += 2;
  if (*(source + index) == 'A') {
    // If it starts with 'A ' or 'An ' then decapitalize the A.
    if (index < len-1 && (*(source + index+1) == ' ' || (*(source + index+1) == 'n' && index < len-2 && *(source + index+2) == ' '))) {
      *(dest + index) = 'a';
    }
  }
  
  return dest;
}

// duplicate a string -- uses new!
char *str_dup(const char *source)
{
  if (!source)
    return NULL;
  
  char *New = new char[strlen(source) + 1];
  
  // This shouldn't be needed, but just in case.
  memset(New, 0, sizeof(char) * (strlen(source) + 1));
  
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
  
  char *temp = str;
  char *temp1 = token;
  
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
  
  char *temp = &dest[0];
  
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
          ch->in_room->number, ch->in_room->name);
  mudlog(buf, ch, LOG_DEATHLOG, TRUE);
}

void log(const char *str) {
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
            get_ch_in_room(ch)->number,
            GET_CHAR_NAME(ch));
  else if (ch && ch->in_room)
    sprintf(buf2, "[%5ld] ", ch->in_room->number);
  else
    strcpy(buf2, "");
  
  if (file)
    fprintf(stderr, "%-19.19s :: %s: %s%s\n", tmp, log_types[log], buf2, str);
  
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
          && !access_level(tch, LVL_BUILDER)) // Decreased from VICEPRES-- we already have priv checks to toggle logs in the first place.
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
        case LOG_PGROUPLOG:
          check_log = PRF_PGROUPLOG;
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
  
  for (int i = 0; i < MAX_OBJ_AFFECT; i++)
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
  int speed = 0, maxspeed = (int)(veh->speed * ((veh->load - veh->usedload) / (veh->load != 0 ? veh->load : 1)));
  struct room_data *in_room = get_veh_in_room(veh);
  switch (veh->cspeed)
  {
    case SPEED_OFF:
    case SPEED_IDLE:
      speed = 0;
      break;
    case SPEED_CRUISING:
      if (ROOM_FLAGGED(in_room, ROOM_INDOORS))
        speed = MIN(maxspeed, 3);
      else
        speed = MIN(maxspeed, 55);
      break;
    case SPEED_SPEEDING:
      if (ROOM_FLAGGED(in_room, ROOM_INDOORS))
        speed = MIN(maxspeed, MAX(5, (int)(maxspeed * .7)));
      else
        speed = MIN(maxspeed, MAX(55, (int)(maxspeed * .7)));
      break;
    case SPEED_MAX:
      if (ROOM_FLAGGED(in_room, ROOM_INDOORS))
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

// I hate this name. This isn't just a getter, it's a setter as well. -LS
int get_skill(struct char_data *ch, int skill, int &target)
{
  char gskbuf[MAX_STRING_LENGTH];
  gskbuf[0] = '\0';
  
  // Wearing too much armor? That'll hurt.
  if (skills[skill].attribute == QUI) {
    int increase = 0;
    if (GET_TOTALIMP(ch) > GET_QUI(ch)) {
      increase = GET_TOTALIMP(ch) - GET_QUI(ch);
      buf_mod(ENDOF(gskbuf), "OverImp", increase);
      target += increase;
    }
    if (GET_TOTALBAL(ch) > GET_QUI(ch)) {
      increase = GET_TOTALBAL(ch) - GET_QUI(ch);
      buf_mod(ENDOF(gskbuf), "OverBal", increase);
      target += increase;
    }
  }
  act(gskbuf, 1, ch, NULL, NULL, TO_ROLLS);
  
  // Core p38
  if (target < 2)
    target = 2;
  
  // TODO: Adept power Improved Ability. This ability is not currently in the game, but would be factored in here. See Core p169 for details.
  
  // Convert NPCs so that they can use Armed Combat in place of any weapon skill except unarmed combat.
  if (IS_NPC(ch) && ((skill >= SKILL_ARMED_COMBAT && skill <= SKILL_CLUBS)
                     || (skill >= SKILL_CYBER_IMPLANTS && skill <= SKILL_ORALSTRIKE)
                     || (skill == SKILL_THROWING_WEAPONS))) {
    if (GET_SKILL(ch, skill) < GET_SKILL(ch, SKILL_ARMED_COMBAT))
      skill = SKILL_ARMED_COMBAT;
  }
  
  if (GET_SKILL(ch, skill))
  {
    int mbw = 0, enhan = 0, synth = 0;
    int totalskill = GET_SKILL(ch, skill);
    
    // If their skill in this area has not been boosted, they get to add their task pool up to the skill's learned level.
    if (REAL_SKILL(ch, skill) == GET_SKILL(ch, skill))
      totalskill += MIN(REAL_SKILL(ch, skill), GET_TASK_POOL(ch, skills[skill].attribute));
    
    // Iterate through their cyberware, looking for anything important.
    if (ch->cyberware) {
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
      
      // If they have both a chipjack with the correct chip loaded and a Chipjack Expert, add the rating to their skill as task pool dice (up to skill max).
      if (chip && expert) {
        totalskill += MIN(REAL_SKILL(ch, skill), expert);
      }
    }
    
    // Iterate through their bioware, looking for anything important.
    if (ch->bioware) {
      for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content) {
        if (GET_OBJ_VAL(bio, 0) == BIO_REFLEXRECORDER && GET_OBJ_VAL(bio, 3) == skill)
          totalskill++;
        else if (GET_OBJ_VAL(bio, 0) == BIO_ENHANCEDARTIC)
          enhan = TRUE;
        else if (GET_OBJ_VAL(bio, 0) == BIO_SYNTHACARDIUM)
          synth = GET_OBJ_VAL(bio, 1);
      }
    }
    
    /* Enhanced Articulation: Possessors roll an additional die when making Success Tests involving any Combat, Physical, Technical and B/R Skills.
     Bonus also applies to physical use of Vehicle Skills. */
    if (enhan) {
      switch (skill) {
          // Combat skills
        case SKILL_ARMED_COMBAT:
        case SKILL_EDGED_WEAPONS:
        case SKILL_POLE_ARMS:
        case SKILL_WHIPS_FLAILS:
        case SKILL_CLUBS:
        case SKILL_UNARMED_COMBAT:
        case SKILL_CYBER_IMPLANTS:
        case SKILL_FIREARMS:
        case SKILL_PISTOLS:
        case SKILL_RIFLES:
        case SKILL_SHOTGUNS:
        case SKILL_ASSAULT_RIFLES:
        case SKILL_SMG:
        case SKILL_GRENADE_LAUNCHERS:
        case SKILL_TASERS:
        case SKILL_GUNNERY:
        case SKILL_MACHINE_GUNS:
        case SKILL_MISSILE_LAUNCHERS:
        case SKILL_ASSAULT_CANNON:
        case SKILL_ARTILLERY:
        case SKILL_PROJECTILES:
        case SKILL_ORALSTRIKE:
        case SKILL_THROWING_WEAPONS:
        case SKILL_OFFHAND_EDGED:
        case SKILL_OFFHAND_CLUB:
        case SKILL_OFFHAND_CYBERIMPLANTS:
        case SKILL_OFFHAND_WHIP:
        case SKILL_UNDERWATER_COMBAT:
        case SKILL_SPRAY_WEAPONS:
        case SKILL_GUNCANE:
        case SKILL_BRACERGUN:
        case SKILL_BLOWGUN:
          // Physical skills
        case SKILL_ATHLETICS:
        case SKILL_DIVING:
        case SKILL_PARACHUTING:
        case SKILL_STEALTH:
        case SKILL_CLIMBING:
        case SKILL_STEAL:
        case SKILL_DANCING:
        case SKILL_INSTRUMENT:
        case SKILL_LOCK_PICKING:
        case SKILL_RIDING:
          // Technical skills
        case SKILL_COMPUTER:
        case SKILL_ELECTRONICS:
        case SKILL_DEMOLITIONS:
        case SKILL_BIOTECH:
        case SKILL_CHEMISTRY:
        case SKILL_DISGUISE:
          // B/R skills
        case SKILL_CYBERTERM_DESIGN:
        case SKILL_BR_BIKE:
        case SKILL_BR_CAR:
        case SKILL_BR_DRONE:
        case SKILL_BR_TRUCK:
        case SKILL_BR_ELECTRONICS:
        case SKILL_BR_COMPUTER:
        case SKILL_BR_EDGED:
        case SKILL_BR_POLEARM:
        case SKILL_BR_CLUB:
        case SKILL_BR_THROWINGWEAPONS:
        case SKILL_BR_WHIPS:
        case SKILL_BR_PROJECTILES:
        case SKILL_BR_PISTOL:
        case SKILL_BR_SHOTGUN:
        case SKILL_BR_RIFLE:
        case SKILL_BR_HEAVYWEAPON:
        case SKILL_BR_SMG:
        case SKILL_BR_ARMOUR:
          totalskill++;
          break;
          // Vehicle skills
        case SKILL_PILOT_ROTORCRAFT:
        case SKILL_PILOT_FIXEDWING:
        case SKILL_PILOT_VECTORTHRUST:
        case SKILL_PILOT_BIKE:
        case SKILL_PILOT_FIXED_WING:
        case SKILL_PILOT_CAR:
        case SKILL_PILOT_TRUCK:
          // You only get the bonus for vehicle skills if you're physically driving the vehicle.
          if (!AFF_FLAGGED(ch, AFF_RIG))
            totalskill++;
          break;
          break;
        default:
          break;
      }
    }
    
    // Move-by-wire.
    if (skill == SKILL_STEALTH || skill == SKILL_ATHLETICS)
      totalskill += mbw;
    
    // Synthacardium.
    if (skill == SKILL_ATHLETICS)
      totalskill += synth;
    
    return totalskill;
  }
  else {
    if (target >= 8)
      return 0;
    target += 4;
    sprintf(gskbuf, "$n (%s) %s(%d) = %d(%d): +4 TN", GET_CHAR_NAME(ch), skills[skill].name, skill, GET_SKILL(ch, skill), REAL_SKILL(ch, skill));
    act(gskbuf, 1, ch, NULL, NULL, TO_ROLLS);
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
          // fall through
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
            send_to_char("You already have an skin modification.\r\n", ch);
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

// Return true if the character has a kit of the given type, false otherwise.
bool has_kit(struct char_data * ch, int type)
{
  for (struct obj_data *o = ch->carrying; o; o = o->next_content)
    if (GET_OBJ_TYPE(o) == ITEM_WORKSHOP && GET_OBJ_VAL(o, 0) == type && GET_OBJ_VAL(o, 1) == TYPE_KIT)
      return TRUE;
  
  if (GET_EQ(ch, WEAR_HOLD))
    if (GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) == ITEM_WORKSHOP &&
        GET_OBJ_VAL(GET_EQ(ch, WEAR_HOLD), 0) == type && GET_OBJ_VAL(GET_EQ(ch, WEAR_HOLD), 1) == TYPE_KIT)
      return TRUE;
  
  return FALSE;
}

// Returns a pointer to the best workshop/facility of the requested type.
struct obj_data *find_workshop(struct char_data * ch, int type)
{
  struct obj_data *workshop = NULL;
  
  if (!ch->in_veh && !ch->in_room)
    return NULL;
  
  // If they're in a valid room, return the room's workshop field for MAXIMUM EFFICIENCY.
  if (ch->in_room)
    return ch->in_room->best_workshop[type];
  
  // If we've gotten here, they must be in a vehicle. Iterate through and find the best candidate.
  for (struct obj_data *o = ch->in_veh->contents; o; o = o->next_content) {
    if (GET_OBJ_TYPE(o) == ITEM_WORKSHOP && GET_WORKSHOP_TYPE(o) == type) {
      if (GET_WORKSHOP_GRADE(o) == TYPE_FACILITY) {
        // Jackpot! Facilities are the best option, so we can terminate early and return this item.
        return o;
      } else if (GET_WORKSHOP_GRADE(o) == TYPE_SHOP && GET_WORKSHOP_IS_SETUP(o))
        workshop = o;
    }
  }
  
  // Return workshop (which is NULL if no workshop was found).
  return workshop;
}

// Preconditions checking for add_ and remove_ workshop functions.
bool _is_workshop_valid(struct obj_data *obj) {
  // We only allow workshop-type items in this function.
  if (GET_OBJ_TYPE(obj) != ITEM_WORKSHOP) {
    sprintf(buf, "SYSERR: Non-workshop item '%s' (%ld) passed to workshop functions.",
            GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj));
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  // Don't allow it to crash the MUD via out-of-bounds world table access.
  if (!obj->in_room) {
    sprintf(buf, "SYSERR: Workshop '%s' (%ld) has NULL room.",
            GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj));
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  // Precondition: The item must be a deployed workshop or a facility.
  if (!(GET_WORKSHOP_GRADE(obj) == TYPE_SHOP && GET_WORKSHOP_IS_SETUP(obj)) && GET_WORKSHOP_GRADE(obj) != TYPE_FACILITY) {
    // For this to be true, the item is a kit. We're not going to throw an error over that, but it's not a valid workshop.
    return FALSE;
  }
  
  return TRUE;
}

void add_workshop_to_room(struct obj_data *obj) {
  if (!_is_workshop_valid(obj))
    return;
  
  if (!obj->in_room)
    return;
  
  struct obj_data *current = obj->in_room->best_workshop[GET_OBJ_VAL(obj, 0)];
  
  if (current && GET_WORKSHOP_GRADE(current) > GET_WORKSHOP_GRADE(obj))
    return;
  
  obj->in_room->best_workshop[GET_WORKSHOP_TYPE(obj)] = obj;
}

void remove_workshop_from_room(struct obj_data *obj) {
  if (!_is_workshop_valid(obj))
    return;
  
  if (!obj->in_room)
    return;
  
  // If this wasn't the best workshop in the first place, who cares?
  if (obj->in_room->best_workshop[GET_WORKSHOP_TYPE(obj)] != obj)
    return;
  
  // Clear out our current best_workshop pointer (previously pointed to obj).
  obj->in_room->best_workshop[GET_WORKSHOP_TYPE(obj)] = NULL;
  
  // Iterate through all items in the room, looking for other valid workshops/facilities of this type.
  for (struct obj_data *o = obj->in_room->contents; o; o = o->next_content) {
    if (o != obj && GET_OBJ_TYPE(o) == ITEM_WORKSHOP && GET_WORKSHOP_TYPE(o) == GET_WORKSHOP_TYPE(obj)) {
      switch (GET_WORKSHOP_GRADE(o)) {
        case TYPE_FACILITY:
          // This is the best possible outcome; set room val to this and return.
          obj->in_room->best_workshop[GET_WORKSHOP_TYPE(obj)] = o;
          return;
        case TYPE_SHOP:
          // The value of best_workshop is either a workshop or null, so no harm in setting it to another workshop.
          obj->in_room->best_workshop[GET_WORKSHOP_TYPE(obj)] = o;
          break;
        case TYPE_KIT:
          break;
        default:
          sprintf(buf, "SYSERR: Invalid workshop type %d found for object '%s' (%ld).", GET_WORKSHOP_GRADE(o), GET_OBJ_NAME(o), GET_OBJ_VNUM(o));
          mudlog(buf, NULL, LOG_SYSLOG, TRUE);
          break;
      }
    }
  }
}

// Checks if a given mount has a weapon on it.
bool mount_has_weapon(struct obj_data *mount) {
  return get_mount_weapon(mount) != NULL;
}

// Retrieve the weapon from a given mount.
struct obj_data *get_mount_weapon(struct obj_data *mount) {
  if (mount == NULL) {
    mudlog("SYSERR: Attempting to retrieve weapon for nonexistent mount.", NULL, LOG_SYSLOG, TRUE);
    return NULL;
  }
  
  for (struct obj_data *contains = mount->contains; contains; contains = contains->next_content) {
    if (GET_OBJ_TYPE(contains) == ITEM_WEAPON)
      return contains;
  }
  
  return NULL;
}

// Cleans up after a character who was manning a mount.
struct obj_data *stop_manning_weapon_mounts(struct char_data *ch, bool send_message) {
  if (AFF_FLAGGED(ch, AFF_MANNING)) {
    struct obj_data *mount;
    
    // Find the mount in-use, if any.
    mount = get_mount_manned_by_ch(ch);
    
    // Clean up the mount's data (stop it from pointing to character; remove targets)
    mount->worn_by = NULL;
    mount->targ = NULL;
    mount->tveh = NULL;
    
    // Clean up their character flags.
    AFF_FLAGS(ch).RemoveBit(AFF_MANNING);
    
    // Take them out of fight mode if they're in it.
    if (CH_IN_COMBAT(ch))
      stop_fighting(ch);
    
    // Let them / the room know that they've stopped manning.
    if (send_message) {
      act("$n stops manning $p.", FALSE, ch, mount, 0, TO_ROOM);
      act("You stop manning $p.", FALSE, ch, mount, 0, TO_CHAR);
    }
    
    return mount;
  }
  return NULL;
}

// Safely retrieves the mount a character is using. Returns NULL for no mount.
struct obj_data *get_mount_manned_by_ch(struct char_data *ch) {
  // Catch-all to notify on bad coding or memory issues.
  if (!ch) {
    mudlog("SYSERR: Attempting to get mount manned by NULL character.", NULL, LOG_SYSLOG, TRUE);
    // No mount returned.
    return NULL;
  }
  
  // Not an error, silently return null.
  if (!AFF_FLAGGED(ch, AFF_MANNING)) {
    return NULL;
  }
  
  // Require that they be in a vehicle. Error and clear their flag if they're not.
  if (!ch->in_veh) {
    sprintf(buf, "SYSERR: Attempting to get mount manned by %s, but %s is not in any vehicle.", GET_CHAR_NAME(ch), HSHR(ch));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    
    // Clean up their mount info. We don't use stop_manning..() because it calls this function itself.
    AFF_FLAGS(ch).RemoveBit(AFF_MANNING);
    if (CH_IN_COMBAT(ch))
      stop_fighting(ch);
    
    // No mount returned.
    return NULL;
  }
  
  // Find and return their mount.
  for (struct obj_data *mount = ch->in_veh->mount; mount; mount = mount->next_content) {
    if (mount->worn_by == ch) {
      // Mount available, return it.
      return mount;
    }
  }
  
  // No mount? Error and clear their flag.
  sprintf(buf, "SYSERR: Attempting to get mount manned by %s, but the mount does not exist in %s vehicle.", GET_CHAR_NAME(ch), HSHR(ch));
  mudlog(buf, ch, LOG_SYSLOG, TRUE);
  
  // Clean up their mount info.
  AFF_FLAGS(ch).RemoveBit(AFF_MANNING);
  if (CH_IN_COMBAT(ch))
    stop_fighting(ch);
  
  // No mount returned.
  return NULL;
}

void store_message_to_history(struct descriptor_data *d, int channel, const char *mallocd_message) {
  // We use our very own message buffer to ensure we'll never overwrite whatever buffer the caller is using.
  static char log_message[256];
  
  // Precondition: No screwy pointers. Removed warning since we can be passed NPC descriptors (which we ignore).
  if (d == NULL || mallocd_message == NULL) {
    // mudlog("SYSERR: Null descriptor or message passed to store_message_to_history.", NULL, LOG_SYSLOG, TRUE);
    return;
  }
  
  // Precondition: Channel must be a valid index (0 ≤ channel < number of channels defined in awake.h).
  if (channel < 0 || channel >= NUM_COMMUNICATION_CHANNELS) {
    sprintf(log_message, "SYSERR: Channel %d is not within bounds 0 <= channel < %d.", channel, NUM_COMMUNICATION_CHANNELS);
    mudlog(log_message, NULL, LOG_SYSLOG, TRUE);
    return;
  }
  
  // Add the message to the descriptor's channel history.
  d->message_history[channel].AddItem(NULL, mallocd_message);
  
  // Constrain message history to the specified amount.
  if (d->message_history[channel].NumItems() > NUM_MESSAGES_TO_RETAIN) {
    // We're over the amount. Remove the tail, making sure we delete the contents.
    if (d->message_history[channel].Tail()->data)
      delete d->message_history[channel].Tail()->data;
    
    d->message_history[channel].RemoveItem(d->message_history[channel].Tail());
  }
}

void delete_message_history(struct descriptor_data *d) {
  // NPCs not allowed, if you pass me a null you fucked up.
  if (d == NULL) {
    mudlog("SYSERR: Null descriptor passed to delete_message_history.", NULL, LOG_SYSLOG, TRUE);
    return;
  }
  
  nodeStruct<const char*> *temp = NULL;
  // For each channel in history, delete all messages (just keep nuking head until it's NULL).
  for (int channel = 0; channel < NUM_COMMUNICATION_CHANNELS; channel++) {
    while ((temp = d->message_history[channel].Head())) {
      DELETE_ARRAY_IF_EXTANT(temp->data);
      
      d->message_history[channel].RemoveItem(temp);
    }
  }
}

// Call this to kill the game while notifying staff etc of what happened.
void terminate_mud_process_with_message(const char *message, int error_code) {
  sprintf(buf, "FATAL ERROR: The MUD has encountered a terminal error (code %d) and will now halt. The message given was as follows: %s",
          error_code, message);
  mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  log(buf);
  exit(error_code);
}

struct room_data *get_veh_in_room(struct veh_data *veh) {
  char errbuf[500];
  if (!veh) {
    sprintf(errbuf, "SYSERR: get_veh_in_room was passed a NULL vehicle!");
    mudlog(errbuf, NULL, LOG_SYSLOG, TRUE);
    return NULL;
  }
  
  while (veh->in_veh)
    veh = veh->in_veh;
  
  // Error messaging.
  if (!veh->in_room) {
    sprintf(errbuf, "SYSERR: get_veh_in_room called on veh %s, but it's not in a room or vehicle!", GET_VEH_NAME(veh));
    mudlog(errbuf, NULL, LOG_SYSLOG, TRUE);
  }
  
  return veh->in_room;
}

struct room_data *get_ch_in_room(struct char_data *ch) {
  char errbuf[500];
  if (!ch) {
    sprintf(errbuf, "SYSERR: get_ch_in_room was passed a NULL character!");
    mudlog(errbuf, ch, LOG_SYSLOG, TRUE);
    return NULL;
  }
  
  if (ch->in_room)
    return ch->in_room;
  
  if (ch->in_veh) {
    return get_veh_in_room(ch->in_veh);
  }
  
  sprintf(errbuf, "SYSERR: get_ch_in_room called on char %s, but they're not in a room or vehicle!", GET_NAME(ch));
  mudlog(errbuf, ch, LOG_SYSLOG, TRUE);
  
  return NULL;
}

struct room_data *get_obj_in_room(struct obj_data *obj) {
  char errbuf[500];
  if (!obj) {
    sprintf(errbuf, "SYSERR: get_obj_in_room was passed a NULL object!");
    mudlog(errbuf, NULL, LOG_SYSLOG, TRUE);
    return NULL;
  }
  
  if (obj->in_room)
    return obj->in_room;
  
  if (obj->in_veh)
    return get_veh_in_room(obj->in_veh);
  
  // Below this line is frantic flailing to avoid passing back NULL (which tends to crash the game).
  if (obj->in_obj)
    return get_obj_in_room(obj->in_obj);
  
  if (obj->carried_by)
    return get_ch_in_room(obj->carried_by);
  
  if (obj->worn_by)
    return get_ch_in_room(obj->carried_by);
  
  // All is lost. The object floats in an endless void.
  sprintf(errbuf, "SYSERR: get_obj_in_room called on obj %s, but it's not in a room or vehicle!", GET_OBJ_NAME(obj));
  mudlog(errbuf, NULL, LOG_SYSLOG, TRUE);
  
  return NULL;
}

bool invis_ok(struct char_data *ch, struct char_data *vict) {
  // Staff member or astrally aware? You can see everything.
  if (IS_SENATOR(ch) || IS_ASTRAL(ch) || IS_DUAL(ch))
    return TRUE;
  
  // Ultrasound pierces all invis as long as it's not blocked by silence or stealth.
  if (AFF_FLAGGED(ch, AFF_DETECT_INVIS) && (get_ch_in_room(ch)->silence[0] <= 0 && !affected_by_spell(vict, SPELL_STEALTH)))
    return TRUE;
  
  // Improved invis defeats all other detection measures.
  if (IS_AFFECTED(vict, AFF_IMP_INVIS) || IS_AFFECTED(vict, AFF_SPELLIMPINVIS))
    return FALSE;
  
  // Standard invis is pierced by thermographic vision, which is default on vehicles.
  if (IS_AFFECTED(vict, AFF_INVISIBLE)) {
    return CURRENT_VISION(ch) == THERMOGRAPHIC || AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE);
  }
  
  // If we've gotten here, they're not invisible.
  return TRUE;
}

// Returns TRUE if the character is able to make noise, FALSE otherwise.
bool char_can_make_noise(struct char_data *ch, const char *message) {
  if (affected_by_spell(ch, SPELL_STEALTH) || get_ch_in_room(ch)->silence[0]) {
    // Can't make noise.
    if (message)
      send_to_char(message, ch);
    
    return FALSE;
  }
  
  return TRUE;
}

struct char_data *get_driver(struct veh_data *veh) {
  struct char_data *i;
    
  for (i = veh->people; i; i = i->next_in_veh)
    if (AFF_FLAGGED(i, AFF_PILOT))
      return i;
  
  return NULL;
}

// Given a vnum, searches all objects and nested containers in the given container for the first match and returns it.
struct obj_data *find_matching_obj_in_container(struct obj_data *container, vnum_t vnum) {
  struct obj_data *result = NULL;
  
  // Nothing given to us? Nothing to find.
  if (container == NULL)
    return NULL;
  
  // Check each item in this container. If it's a match, return it; otherwise, check its contents.
  for (struct obj_data *contents = container->contains; contents; contents = contents->next_content) {
    if (GET_OBJ_VNUM(contents) == vnum)
      return contents;
    
    if ((result = find_matching_obj_in_container(contents, vnum)))
      return result;
  }
  
  // If we got here, the item wasn't found anywhere.
  return NULL;
}

bool attach_attachment_to_weapon(struct obj_data *attachment, struct obj_data *weapon, struct char_data *ch) {
  if (!attachment || !weapon) {
    if (ch)
      send_to_char(ch, "Sorry, something went wrong. Staff have been notified.\r\n");
    mudlog("SYSERR: NULL weapon or attachment passed to attach_attachment_to_weapon().", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  if (GET_OBJ_TYPE(attachment) != ITEM_GUN_ACCESSORY) {
    if (ch)
      send_to_char(ch, "%s is not a gun accessory.\r\n", CAP(GET_OBJ_NAME(attachment)));
    else {
      sprintf(buf, "SYSERR: Attempting to attach non-attachment '%s' (%ld) to '%s' (%ld).",
              GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment), GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    }
    return FALSE;
  }
  
  if (GET_OBJ_TYPE(weapon) != ITEM_WEAPON || !IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))) {
    if (ch)
      send_to_char(ch, "%s is not a gun.\r\n", CAP(GET_OBJ_NAME(weapon)));
    else {
      sprintf(buf, "SYSERR: Attempting to attach '%s' (%ld) to non-gun '%s' (%ld).",
              GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment), GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    }
    return FALSE;
  }
  
  if (GET_OBJ_VAL(attachment, 1) == ACCESS_SMARTGOGGLE) {
    if (ch)
      send_to_char("These are for your eyes, not your gun.\r\n", ch);
    else {
      sprintf(buf, "SYSERR: Attempting to attach smartgoggle '%s' (%ld) to '%s' (%ld).",
              GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment), GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    }
    return FALSE;
  }
  
  if (   ((GET_OBJ_VAL(attachment, 0) == 0) && (GET_WEAPON_ATTACH_TOP_VNUM(weapon)    > 0))
      || ((GET_OBJ_VAL(attachment, 0) == 1) && (GET_WEAPON_ATTACH_BARREL_VNUM(weapon) > 0))
      || ((GET_OBJ_VAL(attachment, 0) == 2) && (GET_WEAPON_ATTACH_UNDER_VNUM(weapon)  > 0))) {
    if (ch) {
      send_to_char(ch, "You cannot mount more than one attachment to the %s of that.\r\n", gun_accessory_locations[GET_OBJ_VAL(attachment, 0)]);
      return FALSE;
    }
    // We assume the coder knows what they're doing (maybe it's a zload where the builder wants to replace a premade attachment with a better one?)
  }
  
  if (   ((GET_OBJ_VAL(attachment, 0) == 0) && (GET_WEAPON_ATTACH_TOP_VNUM(weapon)    == -1))
      || ((GET_OBJ_VAL(attachment, 0) == 1) && (GET_WEAPON_ATTACH_BARREL_VNUM(weapon) == -1))
      || ((GET_OBJ_VAL(attachment, 0) == 2) && (GET_WEAPON_ATTACH_UNDER_VNUM(weapon)  == -1))) {
    sprintf(buf, "%s isn't compatible with %s-mounted attachments.\r\n",
            CAP(GET_OBJ_NAME(weapon)), gun_accessory_locations[GET_OBJ_VAL(attachment, 0)]);
    if (ch) {
      send_to_char(buf, ch);
      return FALSE;
    }
    // We assume the coder knows what they're doing (maybe it's a zload where the builder wants to replace a premade attachment with a better one?)
  }
  
  if (GET_ACCESSORY_ATTACH_LOCATION(attachment) < ACCESS_ACCESSORY_LOCATION_TOP
      || GET_ACCESSORY_ATTACH_LOCATION(attachment) > ACCESS_ACCESSORY_LOCATION_UNDER) {
    if (ch)
      send_to_char(ch, "Sorry, something went wrong. Staff have been notified.\r\n");
    sprintf(buf, "SYSERR: Accessory attachment location %d out of range for '%s' (%ld).",
            GET_ACCESSORY_ATTACH_LOCATION(attachment), GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  if (GET_ACCESSORY_TYPE(attachment) == ACCESS_SILENCER && GET_WEAPON_SKILL(weapon) != SKILL_PISTOLS) {
    if (ch) {
      send_to_char(ch, "%s looks to be threaded for a pistol barrel only.\r\n", capitalize(GET_OBJ_NAME(attachment)));
      return FALSE;
    } else {
      sprintf(buf, "WARNING: Attaching pistol silencer '%s' (%ld) to non-pistol '%s' (%ld).",
              GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment), GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    }
  }
  
  if (GET_ACCESSORY_TYPE(attachment) == ACCESS_SILENCER && !(GET_WEAPON_SKILL(weapon) == SKILL_PISTOLS
                                                       || GET_WEAPON_SKILL(weapon) == SKILL_RIFLES
                                                       || GET_WEAPON_SKILL(weapon) == SKILL_ASSAULT_RIFLES)) {
    if (ch) {
      send_to_char("Sound suppressors can only be attached to rifles, assault rifles, and SMGs.\r\n", ch);
      return FALSE;
    } else {
      sprintf(buf, "WARNING; Attaching rifle/AR/SMG silencer '%s' (%ld) to non-qualifying weapon '%s' (%ld).",
              GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment), GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    }
  }
  
  // Transfer the first (and only the first) affect from the attachment to the weapon.
  if (attachment->affected[0].modifier != 0) {
    bool successfully_modified = FALSE;
    for (int index = 0; index < MAX_OBJ_AFFECT; index++) {
      if (!(weapon->affected[index].modifier)) {
        weapon->affected[index].location = attachment->affected[0].location;
        weapon->affected[index].modifier = attachment->affected[0].modifier;
        successfully_modified = TRUE;
        break;
      }
    }
    
    if (!successfully_modified) {
      if (ch) {
        sprintf(buf, "You seem unable to attach %s to %s.\r\n",
                GET_OBJ_NAME(attachment), GET_OBJ_NAME(weapon));
        send_to_char(buf, ch);
      }
      
      sprintf(buf, "WARNING: '%s' (%ld) attempted to attach '%s' (%ld) to '%s' (%ld), but the gun was full up on affects. Something needs revising."
              " Gun's current top/barrel/bottom attachment vnums are %d / %d / %d.",
              ch ? GET_CHAR_NAME(ch) : "An automated process", ch ? GET_IDNUM(ch) : -1,
              GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment),
              GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon),
              GET_WEAPON_ATTACH_TOP_VNUM(weapon),
              GET_WEAPON_ATTACH_BARREL_VNUM(weapon),
              GET_WEAPON_ATTACH_UNDER_VNUM(weapon));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      return FALSE;
    }
  }
  
  // Add the attachment's weight to the weapon's weight.
  weight_change_object(weapon, GET_OBJ_WEIGHT(attachment));
  
  // Add the attachment's cost to the weapon's cost.
  GET_OBJ_COST(weapon) = MAX(0, GET_OBJ_COST(weapon) + GET_OBJ_COST(attachment));
  
  // Update the weapon's aff flags.
  if (attachment->obj_flags.bitvector.IsSet(AFF_LASER_SIGHT))
    weapon->obj_flags.bitvector.SetBit(AFF_LASER_SIGHT);
  if (attachment->obj_flags.bitvector.IsSet(AFF_VISION_MAG_1))
    weapon->obj_flags.bitvector.SetBit(AFF_VISION_MAG_1);
  if (attachment->obj_flags.bitvector.IsSet(AFF_VISION_MAG_2))
    weapon->obj_flags.bitvector.SetBit(AFF_VISION_MAG_2);
  if (attachment->obj_flags.bitvector.IsSet(AFF_VISION_MAG_3))
    weapon->obj_flags.bitvector.SetBit(AFF_VISION_MAG_3);
  
  // Update the weapon's attach location to reflect this item.
  GET_OBJ_VAL(weapon, GET_ACCESSORY_ATTACH_LOCATION(attachment) + 7) = GET_OBJ_VNUM(attachment);
  
  // Send the success message, assuming there's a character.
  if (ch) {
    int where = GET_ACCESSORY_ATTACH_LOCATION(attachment);
    
    sprintf(buf, "You attach $p to the %s of $P.",
            (where == 0 ? "top" : (where == 1 ? "barrel" : "underside")));
    act(buf, TRUE, ch, attachment, weapon, TO_CHAR);
    
    sprintf(buf, "$n attaches $p to the %s of $P.",
            (where == 0 ? "top" : (where == 1 ? "barrel" : "underside")));
    act(buf, TRUE, ch, attachment, weapon, TO_ROOM);
  } else {
#ifdef DEBUG_ATTACHMENTS
    sprintf(buf, "Successfully attached '%s' (%ld) to the %s of '%s' (%ld).",
            GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment),
            gun_accessory_locations[GET_OBJ_VAL(attachment, 0)],
            GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
#endif
  }
  
  // Let the caller handle trashing the object (assuming they didn't just pass us a proto reference).

  return TRUE;
}

struct obj_data *unattach_attachment_from_weapon(int location, struct obj_data *weapon, struct char_data *ch) {
  if (!weapon) {
    if (ch)
      send_to_char(ch, "Sorry, something went wrong. Staff have been notified.\r\n");
    mudlog("SYSERR: NULL weapon passed to unattach_attachment_from_weapon().", NULL, LOG_SYSLOG, TRUE);
    return NULL;
  }
  
  if (location < ACCESS_LOCATION_TOP || location > ACCESS_LOCATION_UNDER) {
    if (ch)
      send_to_char(ch, "Sorry, something went wrong. Staff have been notified.\r\n");
    sprintf(buf, "SYSERR: Attempt to unattach item from invalid location %d on '%s' (%ld).",
            location, GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
  }
  
  if (GET_OBJ_TYPE(weapon) != ITEM_WEAPON) {
    if (ch)
      send_to_char("You can only unattach accessories from weapons.\r\n", ch);
    else {
      sprintf(buf, "SYSERR: Attempting to unattach something from non-weapon '%s' (%ld).",
              GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    }
    return NULL;
  }
  
  // Get a pointer to the attachment so that we can reference it.
  struct obj_data *attachment = read_object(GET_WEAPON_ATTACH_LOC(weapon, location), VIRTUAL);
  
  // If the attachment was un-loadable, bail out.
  if (!attachment) {
    if (ch)
      send_to_char("You accidentally break it as you remove it!\r\n", ch);
    sprintf(buf, "SYSERR: Attempting to unattach invalid vnum %d from weapon '%s' (%ld).",
            GET_WEAPON_ATTACH_LOC(weapon, location), GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    
    // We use a raw get_obj_val here so we can set it.
    GET_OBJ_VAL(weapon, location) = 0;
    return NULL;
  }
  
  if (GET_ACCESSORY_ATTACH_LOCATION(attachment) < ACCESS_ACCESSORY_LOCATION_TOP
      || GET_ACCESSORY_ATTACH_LOCATION(attachment) > ACCESS_ACCESSORY_LOCATION_UNDER) {
    if (ch)
      send_to_char(ch, "Sorry, something went wrong. Staff have been notified.\r\n");
    sprintf(buf, "SYSERR: Accessory attachment location %d out of range for '%s' (%ld).",
            GET_ACCESSORY_ATTACH_LOCATION(attachment), GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return NULL;
  }
  
  if (GET_ACCESSORY_TYPE(attachment) == ACCESS_GASVENT) {
    if (ch) {
      send_to_char(ch, "%s is permanently attached to %s and can't be removed.\r\n",
                   GET_OBJ_NAME(attachment), GET_OBJ_NAME(weapon));
      return NULL;
    }
   // We assume the coder knows what they're doing when unattaching a gasvent. They may proceed.
  }
  
  // Remove the first (and only the first) affect of the attachment from the weapon.
  if (attachment->affected[0].modifier != 0) {
    bool successfully_modified = FALSE;
    for (int index = 0; index < MAX_OBJ_AFFECT; index++) {
      if (weapon->affected[index].location == attachment->affected[0].location
          && weapon->affected[index].modifier == attachment->affected[0].modifier) {
        weapon->affected[index].location = 0;
        weapon->affected[index].modifier = 0;
        successfully_modified = TRUE;
        break;
      }
    }
    
    if (!successfully_modified) {
      sprintf(buf, "WARNING: '%s' (%ld) unattached '%s' (%ld) from '%s' (%ld), but the gun was missing the attachment's affect. Something needs revising.",
              ch ? GET_CHAR_NAME(ch) : "An automated process", ch ? GET_IDNUM(ch) : -1,
              GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment),
              GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    }
  }
  
  // Subtract the attachment's weight from the weapon's weight.
  weight_change_object(weapon, -GET_OBJ_WEIGHT(attachment));
  
  // Subtract the attachment's cost from the weapon's cost.
  GET_OBJ_COST(weapon) = MAX(0, GET_OBJ_COST(weapon) - GET_OBJ_COST(attachment));
  
  // Update the weapon's aff flags.
  weapon->obj_flags.bitvector.RemoveBit(AFF_LASER_SIGHT);
  weapon->obj_flags.bitvector.RemoveBit(AFF_VISION_MAG_1);
  weapon->obj_flags.bitvector.RemoveBit(AFF_VISION_MAG_2);
  weapon->obj_flags.bitvector.RemoveBit(AFF_VISION_MAG_3);
  
  // Update the weapon's attach location to reflect this item.
  GET_OBJ_VAL(weapon, GET_ACCESSORY_ATTACH_LOCATION(attachment) + 7) = 0;
  
  // Send the success message, assuming there's a character.
  if (ch) {
    act("You unattach $p from $P.", TRUE, ch, attachment, weapon, TO_CHAR);
    act("$n unattaches $p from $P.", TRUE, ch, attachment, weapon, TO_ROOM);
  }
  
  // Hand back our attachment object.
  return attachment;
}

void copy_over_necessary_info(struct char_data *original, struct char_data *clone) {
#define REPLICATE(field) ((clone)->field = (original)->field)
  // Location data.
  REPLICATE(in_room);
  REPLICATE(was_in_room);
  REPLICATE(in_veh);
  
  // PC specials (null for NPCs)
  REPLICATE(player_specials);
  
  // Matrix info (null for NPCs)
  REPLICATE(persona);
  
  // Spell info.
  REPLICATE(squeue);
  REPLICATE(sustained);
  REPLICATE(ssust);
  REPLICATE(spells);
  
  // Equipment info.
  for (int pos = 0; pos < NUM_WEARS; pos++)
    REPLICATE(equipment[pos]);
  REPLICATE(carrying);
  REPLICATE(cyberware);
  REPLICATE(bioware);
  
  // Linked lists.
  REPLICATE(next_in_room);
  REPLICATE(next);
  REPLICATE(next_fighting);
  REPLICATE(next_in_zone);
  REPLICATE(next_in_veh);
  REPLICATE(next_watching);
  
  // Follower / master data.
  REPLICATE(followers);
  REPLICATE(master);
  
  // Pgroup data (null for NPCs)
  REPLICATE(pgroup);
  REPLICATE(pgroup_invitations);
  
  // Nested data (pointers from included structs)
  REPLICATE(char_specials.fighting);
  REPLICATE(char_specials.fight_veh);
  REPLICATE(char_specials.hunting);
  REPLICATE(mob_specials.last_direction);
  REPLICATE(mob_specials.memory);
  REPLICATE(mob_specials.wait_state);
  REPLICATE(points.mental);
  REPLICATE(points.max_mental);
  REPLICATE(points.physical);
  REPLICATE(points.max_physical);
  REPLICATE(points.init_dice);
  REPLICATE(points.init_roll);
  REPLICATE(points.sustained[0]);
  REPLICATE(points.sustained[1]);
  REPLICATE(points.vision[0]);
  REPLICATE(points.vision[1]);
  REPLICATE(points.fire[0]);
  REPLICATE(points.fire[1]);
  REPLICATE(points.reach[0]);
  REPLICATE(points.reach[1]);
#undef REPLICATE
}
