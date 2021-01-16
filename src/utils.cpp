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
#include "newdb.h"
#include "config.h"
#include "bullet_pants.h"

extern class memoryClass *Mem;
extern struct time_info_data time_info;

extern void die(struct char_data * ch);
extern const char *log_types[];
extern long beginning_of_time;
extern int ability_cost(int abil, int level);
extern void weight_change_object(struct obj_data * obj, float weight);
extern void calc_weight(struct char_data *ch);
extern const char *get_ammobox_default_restring(struct obj_data *ammobox);

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
    snprintf(errbuf, sizeof(errbuf), "WARNING: Attempting to generate random number between %d and %d, but RAND_MAX is %d!", from, to, RAND_MAX);
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

int damage_modifier(struct char_data *ch, char *rbuf, int rbuf_size)
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
  if (!IS_NPC(ch)) {
    if (GET_TRADITION(ch) == TRAD_ADEPT
        && GET_POWER(ch, ADEPT_PAIN_RESISTANCE) > 0)
    {
      physical += GET_POWER(ch, ADEPT_PAIN_RESISTANCE) * 100;
      mental += GET_POWER(ch, ADEPT_PAIN_RESISTANCE) * 100;
    }
    
    if (ch->player_specials && GET_DRUG_STAGE(ch) == 1)
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
  }
  
  // first apply physical damage modifiers
  if (physical <= 400)
  {
    base_target += 3;
    buf_mod(rbuf, rbuf_size, "Physical damage (S)", 3 );
  } else if (physical <= 700)
  {
    base_target += 2;
    buf_mod(rbuf, rbuf_size, "Physical damage (M)", 2 );
  } else if (GET_PHYSICAL(ch) <= 900)
  {
    base_target += 1;
    buf_mod(rbuf, rbuf_size, "Physical damage (L)", 1 );
  }
  if (mental <= 400)
  {
    base_target += 3;
    buf_mod(rbuf, rbuf_size, "Mental damage (S)", 3 );
  } else if (mental <= 700)
  {
    base_target += 2;
    buf_mod(rbuf, rbuf_size, "Mental damage (M)", 2 );
  } else if (mental <= 900)
  {
    base_target += 1;
    buf_mod(rbuf, rbuf_size, "Mental damage (L)", 1 );
  }
  return base_target;
}

// Adds the combat_mode toggle
int modify_target_rbuf_raw(struct char_data *ch, char *rbuf, int rbuf_len, int current_visibility_penalty) {
  extern time_info_data time_info;
  int base_target = 0, light_target = 0;
  base_target += damage_modifier(ch, rbuf, rbuf_len);
  // then apply modifiers for sustained spells
  if (GET_SUSTAINED_NUM(ch) > 0)
  {
    base_target += ((GET_SUSTAINED_NUM(ch) - GET_SUSTAINED_FOCI(ch)) * 2);
    buf_mod(rbuf, rbuf_len, "Sustain", (GET_SUSTAINED_NUM(ch) - GET_SUSTAINED_FOCI(ch)) * 2);
  }
  
  struct room_data *temp_room = get_ch_in_room(ch);
  
  if (!temp_room) {
    mudlog("SYSERR: modify_target_rbuf_raw received char with NO room!", ch, LOG_SYSLOG, TRUE);
    return 100;
  }
  
  if (PLR_FLAGGED(ch, PLR_PERCEIVE))
  {
    base_target += 2;
    buf_mod(rbuf, rbuf_len, "AstralPercep", 2);
  } else if (current_visibility_penalty < 8) {
    switch (light_level(temp_room)) {
      case LIGHT_FULLDARK:
        if (CURRENT_VISION(ch) == THERMOGRAPHIC) {
          if (NATURAL_VISION(ch) == THERMOGRAPHIC) {
            light_target += 2;
            buf_mod(rbuf, rbuf_len, "FullDark", 2);
          } else {
            light_target += 4;
            buf_mod(rbuf, rbuf_len, "FullDark", 4);
          }
        } else {
          light_target += 8;
          buf_mod(rbuf, rbuf_len, "FullDark", 8);
        }
        break;
      case LIGHT_MINLIGHT:
        if (CURRENT_VISION(ch) == NORMAL) {
          light_target += 6;
          buf_mod(rbuf, rbuf_len, "MinLight", 6);
        } else {
          if (NATURAL_VISION(ch) == NORMAL) {
            light_target += 4;
            buf_mod(rbuf, rbuf_len, "MinLight", 4);
          } else {
            base_target += 2;
            buf_mod(rbuf, rbuf_len, "MinLight", 2);
          }
        }
        break;
      case LIGHT_PARTLIGHT:
        if (CURRENT_VISION(ch) == NORMAL) {
          light_target += 2;
          buf_mod(rbuf, rbuf_len, "PartLight", 2);
        } else if (CURRENT_VISION(ch) == LOWLIGHT) {
          if (NATURAL_VISION(ch) != LOWLIGHT) {
            light_target++;
            buf_mod(rbuf, rbuf_len, "PartLight", 1);
          }
        } else {
          if (NATURAL_VISION(ch) != THERMOGRAPHIC) {
            light_target += 2;
            buf_mod(rbuf, rbuf_len, "PartLight", 2);
          } else {
            light_target++;
            buf_mod(rbuf, rbuf_len, "PartLight", 1);
          }
        }
        break;
      case LIGHT_GLARE:
        if (CURRENT_VISION(ch) == NORMAL) {
          light_target += 2;
          buf_mod(rbuf, rbuf_len, "Glare", 2);
        } else {
          if (NATURAL_VISION(ch) == NORMAL) {
            light_target += 4;
            buf_mod(rbuf, rbuf_len, "Glare", 2);
          } else {
            light_target += 2;
            buf_mod(rbuf, rbuf_len, "Glare", 2);
          }
        }
        break;
    }
    if (light_target > 0 && temp_room->light[1]) {
      if (temp_room->light[2]) {
        light_target = MAX(0, light_target - temp_room->light[2]);
        buf_mod(rbuf, rbuf_len, "LightSpell", - temp_room->light[2]);
      } else
        light_target /= 2;
    }
    if (temp_room->shadow[0]) {
      light_target += temp_room->shadow[1];
      buf_mod(rbuf, rbuf_len, "ShadowSpell", temp_room->shadow[1]);
    }
    int smoke_target = 0;
    
    if (temp_room->vision[1] == LIGHT_MIST)
      if (CURRENT_VISION(ch) == NORMAL || (CURRENT_VISION(ch) == LOWLIGHT && NATURAL_VISION(ch) == LOWLIGHT)) {
        smoke_target += 2;
        buf_mod(rbuf, rbuf_len, "Mist", 2);
      }
    if (temp_room->vision[1] == LIGHT_LIGHTSMOKE || (weather_info.sky == SKY_RAINING &&
                                                             temp_room->sector_type != SPIRIT_HEARTH && !ROOM_FLAGGED(temp_room, ROOM_INDOORS))) {
      if (CURRENT_VISION(ch) == NORMAL || (CURRENT_VISION(ch) == LOWLIGHT && NATURAL_VISION(ch) != LOWLIGHT)) {
        smoke_target += 4;
        buf_mod(rbuf, rbuf_len, "LSmoke/LRain", 4);
      } else if (CURRENT_VISION(ch) == LOWLIGHT) {
        smoke_target += 2;
        buf_mod(rbuf, rbuf_len, "LSmoke/LRain", 2);
      }
    }
    if (temp_room->vision[1] == LIGHT_HEAVYSMOKE || (weather_info.sky == SKY_LIGHTNING &&
                                                             temp_room->sector_type != SPIRIT_HEARTH && !ROOM_FLAGGED(temp_room, ROOM_INDOORS))) {
      if (CURRENT_VISION(ch) == NORMAL || (CURRENT_VISION(ch) == LOWLIGHT && NATURAL_VISION(ch) == NORMAL)) {
        smoke_target += 6;
        buf_mod(rbuf, rbuf_len, "HSmoke/HRain", 6);
      } else if (CURRENT_VISION(ch) == LOWLIGHT) {
        smoke_target += 4;
        buf_mod(rbuf, rbuf_len, "HSmoke/HRain", 4);
      } else if (CURRENT_VISION(ch) == THERMOGRAPHIC && NATURAL_VISION(ch) != THERMOGRAPHIC) {
        smoke_target++;
        buf_mod(rbuf, rbuf_len, "HSmoke/HRain", 1);
      }
    }
    if (temp_room->vision[1] == LIGHT_THERMALSMOKE) {
      if (CURRENT_VISION(ch) == NORMAL || CURRENT_VISION(ch) == LOWLIGHT) {
        smoke_target += 4;
        buf_mod(rbuf, rbuf_len, "TSmoke", 4);
      } else {
        if (NATURAL_VISION(ch) == THERMOGRAPHIC) {
          smoke_target += 6;
          buf_mod(rbuf, rbuf_len, "TSmoke", 6);
        } else {
          smoke_target += 8;
          buf_mod(rbuf, rbuf_len, "TSmoke", 8);
        }
      }
    }
    // The maximum visibility penalty we apply is +8 TN to avoid things like an invisible person in a smoky pitch-black room getting +24 to hit TN.
    if (light_target + smoke_target + current_visibility_penalty > 8) {
      buf_mod(rbuf, rbuf_len, "ButVisPenaltyMaxIs8", (8 - current_visibility_penalty) - light_target + smoke_target);
      base_target += 8 - current_visibility_penalty;
    } else
      base_target += light_target + smoke_target;
  }
  base_target += GET_TARGET_MOD(ch);
  buf_mod(rbuf, rbuf_len, "GET_TARGET_MOD", GET_TARGET_MOD(ch) );
  if (GET_RACE(ch) == RACE_NIGHTONE && ((time_info.hours > 6) && (time_info.hours < 19)) && OUTSIDE(ch))
  {
    base_target += 1;
    buf_mod(rbuf, rbuf_len, "Sunlight", 1);
  }
  if (temp_room->poltergeist[0] && !IS_ASTRAL(ch) && !IS_DUAL(ch))
  {
    base_target += 2;
    buf_mod(rbuf, rbuf_len, "Polter", 2);
  }
  if (AFF_FLAGGED(ch, AFF_ACID))
  {
    base_target += 4;
    buf_mod(rbuf, rbuf_len, "Acid", 4);
  }
  if (ch->points.fire[0] > 0)
  {
    base_target += 4;
    buf_mod(rbuf, rbuf_len, "OnFire", 4);
  }
  for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = sust->next)
  {
    if (sust->caster == FALSE && (sust->spell == SPELL_CONFUSION || sust->spell == SPELL_CHAOS)) {
      base_target += MIN(sust->force, sust->success);
      buf_mod(rbuf, rbuf_len, "Confused", MIN(sust->force, sust->success));
    }
  }
  if (!(IS_ELEMENTAL(ch) || IS_SPIRIT(ch)))
    for (struct spirit_sustained *sust = SPIRIT_SUST(ch); sust; sust = sust->next)
      if (sust == CONFUSION)
      {
        base_target += GET_LEVEL(sust->target);
        buf_mod(rbuf, rbuf_len, "SConfused", GET_LEVEL(sust->target));
      }
#ifdef USE_SLOUCH_RULES
  if (temp_room && ROOM_FLAGGED(temp_room, ROOM_INDOORS)) {
    float heightdif = GET_HEIGHT(ch) / ((temp_room->z != 0 ? temp_room->z : 1)*100);
    if (heightdif > 1) {
      base_target += 2;
      buf_mod(rbuf, rbuf_len, "TooTallRatio", (int)(heightdif*100));
    }
    if (heightdif > 1.2)
      base_target += 2;
    if (heightdif > 1.5)
      base_target += 2;
    if (heightdif > 2)
      base_target += 2;
  }
#endif
  return base_target;
}

int modify_target_rbuf(struct char_data *ch, char *rbuf, int rbuf_len)
{
  return modify_target_rbuf_raw(ch, rbuf, rbuf_len, 0);
}

int modify_target(struct char_data *ch)
{
  char fake_rbuf[5000];
  memset(fake_rbuf, 0, sizeof(fake_rbuf));
  return modify_target_rbuf_raw(ch, fake_rbuf, sizeof(fake_rbuf), 0);
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
  
  char *New = new char[strlen(source) + 2];
  
  // This shouldn't be needed, but just in case.
  // memset(New, 0, sizeof(char) * (strlen(source) + 1));
  
  strncpy(New, source, sizeof(New) - 1);
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
  
  snprintf(buf, sizeof(buf), "%s hit DeathTrap #%ld (%s)", GET_CHAR_NAME(ch),
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
  
  // Fallback-- it's blank if we find no useful conditions.
  strcpy(buf2, "");
  
  if (ch && (ch->in_room || ch->in_veh)) {
    if (ch->desc)
      snprintf(buf2, sizeof(buf2), "[%5ld] (%s) ",
              get_ch_in_room(ch)->number,
              GET_CHAR_NAME(ch));
    else
      snprintf(buf2, sizeof(buf2), "[%5ld] ", get_ch_in_room(ch)->number);
  }
  
  if (file)
    fprintf(stderr, "%-19.19s :: %s: %s%s\n", tmp, log_types[log], buf2, str);
  
  snprintf(buf, sizeof(buf), "^g[%s: %s%s^g]^n\r\n", log_types[log], buf2, str);
  
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
      
      // We don't show log messages from imms who are invis at a level higher than you, unless you're a high enough level that that doesn't matter.
      if (ch
          && ((IS_NPC(ch) || !ch->player_specials) ? FALSE : !access_level(tch, GET_INVIS_LEV(ch)))
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
        case LOG_PGROUPLOG:
          check_log = PRF_PGROUPLOG;
          break;
        case LOG_HELPLOG:
          check_log = PRF_HELPLOG;
          break;
        case LOG_PURGELOG:
          check_log = PRF_PURGELOG;
          break;
        case LOG_FUCKUPLOG:
          check_log = PRF_FUCKUPLOG;
          break;
        case LOG_ECONLOG:
          check_log = PRF_ECONLOG;
          break;
        default:
          char errbuf[500];
          snprintf(errbuf, sizeof(errbuf), "SYSERR: Attempting to display a message to log type %d, but that log type is not handled in utils.cpp's mudlog() function! Dumping to SYSLOG.", log);
          mudlog(errbuf, NULL, LOG_SYSLOG, TRUE);
          check_log = PRF_SYSLOG;
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
  
  bool have_printed = FALSE;
  for (nr = 0; vektor; vektor >>= 1) {
    if (IS_SET(1, vektor)) {
      if (*names[nr] != '\n') {
        if (have_printed) {
          // Better formatting. Better coding. Papa Lucien's.
          strcat(result, ", ");
        }
        strcat(result, names[nr]);
      } else {
        strcat(result, "UNDEFINED ");
      }
      have_printed = TRUE;
    }
    if (*names[nr] != '\n')
      nr++;
  }
  
  if (!*result)
    strcat(result, "None ");
}

void sprinttype(int type, const char *names[], char *result, int result_size)
{
  snprintf(result, result_size, "%s", names[type]);
  
  if (str_cmp(result, "(null)") == 0) {
    strcpy(result, "UNDEFINED");
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
    snprintf(result, sizeof(result),"%s %s", result, xbuf);
  }
  
  for (int i = 0; i < MAX_OBJ_AFFECT; i++)
    if (obj->affected[i].modifier != 0)
    {
      char xbuf[MAX_STRING_LENGTH];
      sprinttype(obj->affected[i].location, apply_types, xbuf, sizeof(xbuf));
      snprintf(result, sizeof(result),"%s (%+d %s)",
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

char * buf_mod(char *rbuf, int rbuf_len, const char *name, int bonus)
{
  if ( !rbuf )
    return rbuf;
  if ( bonus == 0 )
    return rbuf;
    
  rbuf_len -= strlen(rbuf);
  rbuf += strlen(rbuf);
  
  if ( bonus > 0 )
    snprintf(rbuf, rbuf_len, "%s +%d, ", name, bonus);
  else
    snprintf(rbuf, rbuf_len, "%s %d, ", name, bonus);
  rbuf += strlen(rbuf);
  return rbuf;
}

char * buf_roll(char *rbuf, const char *name, int bonus)
{
  if ( !rbuf )
    return rbuf;
  rbuf += strlen(rbuf);
  snprintf(rbuf, sizeof(rbuf), " [%s %d]", name, bonus);
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
  int cmod = -GET_POWER(ch, ADEPT_KINESICS);
  int tmod = -GET_POWER(tch, ADEPT_KINESICS);
  snprintf(buf3, sizeof(buf3), "ALRIGHT BOIS HERE WE GO. Base global mod is %d. After application of Kinesics bonuses (if any), base modifiers for PC and NPC are %d and %d.", mod, cmod, tmod);
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
        snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), " Metavariant TN penalty +4 for PC.");
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
        snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), " Metavariant TN penalty +4 for NPC.");
        break;
    }
  }
  
  int chtn = GET_INT(tch)+mod+cmod;
  int tchtn = GET_INT(ch)+mod+tmod;
  
  act("Getting skill for PC...", FALSE, ch, NULL, NULL, TO_ROLLS);
  int cskill = get_skill(ch, SKILL_NEGOTIATION, chtn);
  act("Getting skill for NPC...", FALSE, tch, NULL, NULL, TO_ROLLS);
  int tskill = get_skill(tch, SKILL_NEGOTIATION, tchtn);
  
  for (bio = ch->bioware; bio; bio = bio->next_content)
    if (GET_OBJ_VAL(bio, 0) == BIO_TAILOREDPHEREMONES)
    {
      int delta = GET_OBJ_VAL(bio, 2) ? GET_OBJ_VAL(bio, 1) * 2: GET_OBJ_VAL(bio, 1);
      cskill += delta;
      snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), " Pheremone skill buff of %d for PC.", delta);
      break;
    }
  
  for (bio = tch->bioware; bio; bio = bio->next_content)
    if (GET_OBJ_VAL(bio, 0) == BIO_TAILOREDPHEREMONES)
    {
      int delta = GET_OBJ_VAL(bio, 2) ? GET_OBJ_VAL(bio, 1) * 2: GET_OBJ_VAL(bio, 1);
      tskill += delta;
      snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), " Pheremone skill buff of %d for NPC.", delta);
      break;
    }
  
  int tchnego = success_test(tskill, tchtn);
  int chnego = success_test(cskill, chtn);
  
  snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "\r\nPC negotiation test gave %d successes on %d dice with TN %d (calculated from opponent int (%d) + global mod (%d) + our mod (%d)).",
           chnego, cskill, chtn, GET_INT(tch), mod, cmod);
  snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "\r\nNPC negotiation test gave %d successes on %d dice with TN %d (calculated from opponent int (%d) + global mod (%d) + our mod (%d)).", 
           tchnego, tskill, tchtn, GET_INT(ch), mod, tmod);
  if (comp)
  {
    int ch_delta = success_test(GET_SKILL(ch, comp), GET_INT(tch)+mod+cmod) / 2;
    int tch_delta = success_test(GET_SKILL(tch, comp), GET_INT(ch)+mod+tmod) / 2;
    chnego += ch_delta;
    tchnego += tch_delta;
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "\r\nidk what this 'comp' field is, but it was true, so we got an additional %d PC and %d successes.", ch_delta, tch_delta);
  }
  int num = chnego - tchnego;
  if (num > 0)
  {
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "\r\nPC got more successes, so basevalue goes from %d", basevalue);
    if (buy)
      basevalue = MAX((int)(basevalue * 3/4), basevalue - (num * (basevalue / 20)));
    else
      basevalue = MIN((int)(basevalue * 5/4), basevalue + (num * (basevalue / 15)));
  } else
  { 
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "\r\nNPC got more successes, so basevalue goes from %d", basevalue);
    if (buy)
      basevalue = MIN((int)(basevalue * 5/4), basevalue + (num * (basevalue / 15)));
    else
      basevalue = MAX((int)(basevalue * 3/4), basevalue - (num * (basevalue / 20)));
  }
  snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), " to %d.", basevalue);
  act(buf3, FALSE, ch, NULL, NULL, TO_ROLLS);
  return basevalue;
  
}

// I hate this name. This isn't just a getter, it's a setter as well. -LS
int get_skill(struct char_data *ch, int skill, int &target)
{
  char gskbuf[MAX_STRING_LENGTH];
  gskbuf[0] = '\0';
  int increase = 0;
  
  // Wearing too much armor? That'll hurt.
  if (skills[skill].attribute == QUI) {
    if (GET_TOTALIMP(ch) > GET_QUI(ch)) {
      increase = GET_TOTALIMP(ch) - GET_QUI(ch);
      buf_mod(ENDOF(gskbuf), sizeof(gskbuf) - strlen(gskbuf), "OverImp", increase);
      target += increase;
    }
    if (GET_TOTALBAL(ch) > GET_QUI(ch)) {
      increase = GET_TOTALBAL(ch) - GET_QUI(ch);
      buf_mod(ENDOF(gskbuf), sizeof(gskbuf) - strlen(gskbuf), "OverBal", increase);
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
        else if (GET_OBJ_VAL(obj, 0) == CYB_CHIPJACK) {
          int real_obj;
          for (int i = 5; i < 10; i++) {
            if ((real_obj = real_object(GET_OBJ_VAL(obj, i))) > 0 && obj_proto[real_obj].obj_flags.value[0] == skill)
              chip = TRUE;
          }
        }
      
      // If they have both a chipjack with the correct chip loaded and a Chipjack Expert, add the rating to their skill as task pool dice (up to skill max).
      if (chip && expert) {
        increase = MIN(REAL_SKILL(ch, skill), expert);
        totalskill += increase;
        snprintf(gskbuf, sizeof(gskbuf), "Chip & Expert Skill Increase: %d", increase);
        act(gskbuf, 1, ch, NULL, NULL, TO_ROLLS);
      }
    }
    
    // Iterate through their bioware, looking for anything important.
    if (ch->bioware) {
      for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content) {
        if (GET_OBJ_VAL(bio, 0) == BIO_REFLEXRECORDER && GET_OBJ_VAL(bio, 3) == skill) {
          act("Reflex Recorder skill increase: 1", 1, ch, NULL, NULL, TO_ROLLS);
          totalskill++;
        } else if (GET_OBJ_VAL(bio, 0) == BIO_ENHANCEDARTIC)
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
        case SKILL_BR_ARMOR:
          act("Enhanced Articulation skill increase: +1", 1, ch, NULL, NULL, TO_ROLLS);
          totalskill++;
          break;
          // Vehicle skills
        case SKILL_PILOT_ROTORCRAFT:
        case SKILL_PILOT_FIXEDWING:
        case SKILL_PILOT_VECTORTHRUST:
        case SKILL_PILOT_BIKE:
        case SKILL_PILOT_CAR:
        case SKILL_PILOT_TRUCK:
          // You only get the bonus for vehicle skills if you're physically driving the vehicle.
          if (!AFF_FLAGGED(ch, AFF_RIG)) {
            act("Enhanced Articulation skill increase: +1", 1, ch, NULL, NULL, TO_ROLLS);
            totalskill++;
          }
          break;
          break;
        default:
          break;
      }
    }
    
    // Move-by-wire.
    if ((skill == SKILL_STEALTH || skill == SKILL_ATHLETICS) && mbw) {
      snprintf(gskbuf, sizeof(gskbuf), "Move-By-Wire Skill Increase: %d", mbw);
      act(gskbuf, 1, ch, NULL, NULL, TO_ROLLS);
      totalskill += mbw;
    }
    
    // Synthacardium.
    if (skill == SKILL_ATHLETICS && synth) {
      snprintf(gskbuf, sizeof(gskbuf), "Synthacardium Skill Increase: %d", synth);
      act(gskbuf, 1, ch, NULL, NULL, TO_ROLLS);
      totalskill += synth;
    }
    
    return totalskill;
  }
  else {
    if (target >= 8)
      return 0;
    target += 4;
    snprintf(gskbuf, sizeof(gskbuf), "$n (%s) defaulting on %s = %d(%d): +4 TN, will have %d dice.", GET_CHAR_NAME(ch), skills[skill].name, GET_SKILL(ch, skill), REAL_SKILL(ch, skill), GET_ATT(ch, skills[skill].attribute));
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
  
  for (i = number(1, ADEPT_NUMPOWER - 1); GET_PP(vict) < 0;
       i = number(1, ADEPT_NUMPOWER - 1))
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
    snprintf(buf, sizeof(buf), "DELETE FROM pfiles_magic WHERE idnum=%ld;", GET_IDNUM(ch));
    mysql_wrapper(mysql, buf);
    snprintf(buf, sizeof(buf), "UPDATE pfiles SET Tradition=%d WHERE idnum=%ld;", TRAD_MUNDANE, GET_IDNUM(ch));
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
      } else if (GET_WORKSHOP_GRADE(o) == TYPE_WORKSHOP && GET_WORKSHOP_IS_SETUP(o))
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
    snprintf(buf, sizeof(buf), "SYSERR: Non-workshop item '%s' (%ld) passed to workshop functions.",
            GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj));
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  // Don't allow it to crash the MUD via out-of-bounds world table access.
  if (!obj->in_room) {
    snprintf(buf, sizeof(buf), "SYSERR: Workshop '%s' (%ld) has NULL room.",
            GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj));
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  // Precondition: The item must be a deployed workshop or a facility.
  if (!(GET_WORKSHOP_GRADE(obj) == TYPE_WORKSHOP && GET_WORKSHOP_IS_SETUP(obj)) && GET_WORKSHOP_GRADE(obj) != TYPE_FACILITY) {
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
        case TYPE_WORKSHOP:
          // The value of best_workshop is either a workshop or null, so no harm in setting it to another workshop.
          obj->in_room->best_workshop[GET_WORKSHOP_TYPE(obj)] = o;
          break;
        case TYPE_KIT:
          break;
        default:
          snprintf(buf, sizeof(buf), "SYSERR: Invalid workshop type %d found for object '%s' (%ld).", GET_WORKSHOP_GRADE(o), GET_OBJ_NAME(o), GET_OBJ_VNUM(o));
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

// Retrieve the ammobox from a given mount.
struct obj_data *get_mount_ammo(struct obj_data *mount) {
  if (mount == NULL) {
    mudlog("SYSERR: Attempting to retrieve weapon for nonexistent mount.", NULL, LOG_SYSLOG, TRUE);
    return NULL;
  }
  
  for (struct obj_data *contains = mount->contains; contains; contains = contains->next_content) {
    if (GET_OBJ_TYPE(contains) == ITEM_GUN_AMMO)
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
    snprintf(buf, sizeof(buf), "SYSERR: Attempting to get mount manned by %s, but %s is not in any vehicle.", GET_CHAR_NAME(ch), HSHR(ch));
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
  snprintf(buf, sizeof(buf), "SYSERR: Attempting to get mount manned by %s, but the mount does not exist in %s vehicle.", GET_CHAR_NAME(ch), HSHR(ch));
  mudlog(buf, ch, LOG_SYSLOG, TRUE);
  
  // Clean up their mount info.
  AFF_FLAGS(ch).RemoveBit(AFF_MANNING);
  if (CH_IN_COMBAT(ch))
    stop_fighting(ch);
  
  // No mount returned.
  return NULL;
}

void store_message_to_history(struct descriptor_data *d, int channel, const char *newd_message) {
  // We use our very own message buffer to ensure we'll never overwrite whatever buffer the caller is using.
  static char log_message[256];
  
  // Precondition: No screwy pointers. Removed warning since we can be passed NPC descriptors (which we ignore).
  if (d == NULL || newd_message == NULL) {
    // mudlog("SYSERR: Null descriptor or message passed to store_message_to_history.", NULL, LOG_SYSLOG, TRUE);
    DELETE_ARRAY_IF_EXTANT(newd_message);
    return;
  }
  
  // Precondition: Channel must be a valid index (0 ≤ channel < number of channels defined in awake.h).
  if (channel < 0 || channel >= NUM_COMMUNICATION_CHANNELS) {
    snprintf(log_message, sizeof(log_message), "SYSERR: Channel %d is not within bounds 0 <= channel < %d.", channel, NUM_COMMUNICATION_CHANNELS);
    mudlog(log_message, NULL, LOG_SYSLOG, TRUE);
    DELETE_ARRAY_IF_EXTANT(newd_message);
    return;
  }
  
  // Add the message to the descriptor's channel history.
  d->message_history[channel].AddItem(NULL, newd_message);
  
  // Constrain message history to the specified amount.
  if (d->message_history[channel].NumItems() > NUM_MESSAGES_TO_RETAIN) {
    // We're over the amount. Remove the tail, making sure we delete the contents.
    if (d->message_history[channel].Tail()->data)
      delete [] d->message_history[channel].Tail()->data;

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
  snprintf(buf, sizeof(buf), "FATAL ERROR: The MUD has encountered a terminal error (code %d) and will now halt. The message given was as follows: %s",
          error_code, message);
  mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  log(buf);
  exit(error_code);
}

struct room_data *get_veh_in_room(struct veh_data *veh) {
  if (!veh) {
    mudlog("SYSERR: get_veh_in_room was passed a NULL vehicle!", NULL, LOG_SYSLOG, TRUE);
    return NULL;
  }
  
  while (veh->in_veh)
    veh = veh->in_veh;
  
  /*  This is not precisely an error case-- it just means the vehicle is being towed. -- LS. 
  if (!veh->in_room) {
    char errbuf[500];
    sprintf(errbuf, "SYSERR: get_veh_in_room called on veh %s, but it's not in a room or vehicle!", GET_VEH_NAME(veh));
    mudlog(errbuf, NULL, LOG_SYSLOG, TRUE); 
    
  }
  */
  
  return veh->in_room;
}

struct room_data *get_ch_in_room(struct char_data *ch) {
  char errbuf[500];
  if (!ch) {
    snprintf(errbuf, sizeof(errbuf), "SYSERR: get_ch_in_room was passed a NULL character!");
    mudlog(errbuf, NULL, LOG_SYSLOG, TRUE);
    return NULL;
  }
  
  if (ch->in_room)
    return ch->in_room;
  
  if (ch->in_veh) {
    return get_veh_in_room(ch->in_veh);
  }
  
  snprintf(errbuf, sizeof(errbuf), "SYSERR: get_ch_in_room called on char %s, but they're not in a room or vehicle!", GET_CHAR_NAME(ch));
  mudlog(errbuf, ch, LOG_SYSLOG, TRUE);
  
  return NULL;
}

struct room_data *get_obj_in_room(struct obj_data *obj) {
  char errbuf[500];
  if (!obj) {
    snprintf(errbuf, sizeof(errbuf), "SYSERR: get_obj_in_room was passed a NULL object!");
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
  snprintf(errbuf, sizeof(errbuf), "SYSERR: get_obj_in_room called on obj %s, but it's not in a room or vehicle!", GET_OBJ_NAME(obj));
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
  if (veh->rigger)
    return veh->rigger;
    
  for (struct char_data *i = veh->people; i; i = i->next_in_veh)
    if (AFF_FLAGGED(i, AFF_PILOT) || AFF_FLAGGED(i, AFF_RIG))
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

bool attach_attachment_to_weapon(struct obj_data *attachment, struct obj_data *weapon, struct char_data *ch, int location) {
  if (!attachment || !weapon) {
    if (ch)
      send_to_char(ch, "Sorry, something went wrong. Staff have been notified.\r\n");
    mudlog("SYSERR: NULL weapon or attachment passed to attach_attachment_to_weapon().", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  if (location < ACCESS_ACCESSORY_LOCATION_TOP
      || location > ACCESS_ACCESSORY_LOCATION_UNDER) {
    if (ch)
      send_to_char(ch, "Sorry, something went wrong. Staff have been notified.\r\n");
    snprintf(buf, sizeof(buf), "SYSERR: Accessory attachment location %d out of range for '%s' (%ld).",
            location, GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  if (GET_OBJ_TYPE(attachment) != ITEM_GUN_ACCESSORY) {
    if (ch)
      send_to_char(ch, "%s is not a gun accessory.\r\n", CAP(GET_OBJ_NAME(attachment)));
    else {
      snprintf(buf, sizeof(buf), "SYSERR: Attempting to attach non-attachment '%s' (%ld) to '%s' (%ld).",
              GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment), GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    }
    return FALSE;
  }
  
  if (GET_OBJ_TYPE(weapon) != ITEM_WEAPON || !IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))) {
    if (ch)
      send_to_char(ch, "%s is not a gun.\r\n", CAP(GET_OBJ_NAME(weapon)));
    else {
      snprintf(buf, sizeof(buf), "SYSERR: Attempting to attach '%s' (%ld) to non-gun '%s' (%ld).",
              GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment), GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    }
    return FALSE;
  }
  
  if (GET_ACCESSORY_TYPE(attachment) == ACCESS_SMARTGOGGLE) {
    if (ch)
      send_to_char(ch, "%s are for your eyes, not your gun.\r\n", CAP(GET_OBJ_NAME(attachment)));
    else {
      snprintf(buf, sizeof(buf), "SYSERR: Attempting to attach smartgoggle '%s' (%ld) to '%s' (%ld).",
              GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment), GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    }
    return FALSE;
  }
  
  if (   ((location == ACCESS_ACCESSORY_LOCATION_TOP) && (GET_WEAPON_ATTACH_TOP_VNUM(weapon)    > 0))
      || ((location == ACCESS_ACCESSORY_LOCATION_BARREL) && (GET_WEAPON_ATTACH_BARREL_VNUM(weapon) > 0))
      || ((location == ACCESS_ACCESSORY_LOCATION_UNDER) && (GET_WEAPON_ATTACH_UNDER_VNUM(weapon)  > 0))) {
    if (ch) {
      send_to_char(ch, "You cannot mount more than one attachment to the %s of %s.\r\n",
                   gun_accessory_locations[location],
                   GET_OBJ_NAME(weapon));
    } else {
      snprintf(buf, sizeof(buf), "SYSERR: Attempting to attach '%s' (%ld) to already-filled %s location on '%s' (%ld).",
              GET_OBJ_NAME(attachment),
              GET_OBJ_VNUM(attachment),
              gun_accessory_locations[location],
              GET_OBJ_NAME(weapon),
              GET_OBJ_VNUM(weapon));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    }
    return FALSE;
  }
  
  if (   ((location == ACCESS_ACCESSORY_LOCATION_TOP) && (GET_WEAPON_ATTACH_TOP_VNUM(weapon)    < 0))
      || ((location == ACCESS_ACCESSORY_LOCATION_BARREL) && (GET_WEAPON_ATTACH_BARREL_VNUM(weapon) < 0))
      || ((location == ACCESS_ACCESSORY_LOCATION_UNDER) && (GET_WEAPON_ATTACH_UNDER_VNUM(weapon)  < 0))) {
    if (ch) {
      send_to_char(ch, "%s isn't compatible with %s-mounted attachments.\r\n",
                   CAP(GET_OBJ_NAME(weapon)),
                   gun_accessory_locations[location]);
    } else {
      snprintf(buf, sizeof(buf), "SYSERR: Attempting to attach '%s' (%ld) to unacceptable %s location on '%s' (%ld).",
              GET_OBJ_NAME(attachment),
              GET_OBJ_VNUM(attachment),
              gun_accessory_locations[location],
              GET_OBJ_NAME(weapon),
              GET_OBJ_VNUM(weapon));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    }
    return FALSE;
  }
  
  // Handling for silencer and suppressor restrictions.
  if (GET_ACCESSORY_TYPE(attachment) == ACCESS_SILENCER || GET_ACCESSORY_TYPE(attachment) == ACCESS_SOUNDSUPP) {
    // Weapon cannot be a revolver or shotgun.
    if (GET_WEAPON_ATTACK_TYPE(weapon) == WEAP_REVOLVER || GET_WEAPON_ATTACK_TYPE(weapon) == WEAP_SHOTGUN) {
      if (ch) {
        send_to_char(ch, "%ss can't use silencers or suppressors.\r\n", CAP(weapon_type[GET_WEAPON_ATTACK_TYPE(weapon)]));
      } else {
        snprintf(buf, sizeof(buf), "SYSERR: Attempting to attach silencer/suppressor '%s' (%ld) to %s '%s' (%ld).",
                GET_OBJ_NAME(attachment),
                GET_OBJ_VNUM(attachment),
                weapon_type[GET_WEAPON_ATTACK_TYPE(weapon)],
                GET_OBJ_NAME(weapon),
                GET_OBJ_VNUM(weapon));
        mudlog(buf, ch, LOG_SYSLOG, TRUE);
      }
      return FALSE;
    }
    
    // Silencers.
    if (GET_ACCESSORY_TYPE(attachment) == ACCESS_SILENCER) {
      // Weapon cannot have BF or FA modes.
      if (WEAPON_CAN_USE_FIREMODE(weapon, MODE_BF) || WEAPON_CAN_USE_FIREMODE(weapon, MODE_FA)) {
        if (ch) {
          send_to_char(ch, "%s would tear your silencer apart-- it needs a sound suppressor.\r\n", CAP(GET_OBJ_NAME(weapon)));
        } else {
          snprintf(buf, sizeof(buf), "SYSERR: Attempting to attach pistol silencer '%s' (%ld) to BF/FA weapon '%s' (%ld).",
                  GET_OBJ_NAME(attachment),
                  GET_OBJ_VNUM(attachment),
                  GET_OBJ_NAME(weapon),
                  GET_OBJ_VNUM(weapon));
          mudlog(buf, ch, LOG_SYSLOG, TRUE);
        }
        return FALSE;
      }
    }
    
    // Sound suppressors.
    if (GET_ACCESSORY_TYPE(attachment) == ACCESS_SOUNDSUPP) {
      // Weapon must have BF or FA mode available.
      if (!(WEAPON_CAN_USE_FIREMODE(weapon, MODE_BF) || WEAPON_CAN_USE_FIREMODE(weapon, MODE_FA))) {
        if (ch) {
          send_to_char(ch, "Sound suppressors are too heavy-duty for %s-- it needs a silencer.\r\n", CAP(GET_OBJ_NAME(weapon)));
        } else {
          snprintf(buf, sizeof(buf), "SYSERR: Attempting to attach rifle suppressor '%s' (%ld) to non-BF/FA weapon '%s' (%ld).",
                  GET_OBJ_NAME(attachment),
                  GET_OBJ_VNUM(attachment),
                  GET_OBJ_NAME(weapon),
                  GET_OBJ_VNUM(weapon));
          mudlog(buf, ch, LOG_SYSLOG, TRUE);
        }
        return FALSE;
      }
    }
  }
  
  // Transfer the first (and only the first) applies-affect from the attachment to the weapon.
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
        snprintf(buf, sizeof(buf), "You seem unable to attach %s to %s.\r\n",
                GET_OBJ_NAME(attachment), GET_OBJ_NAME(weapon));
        send_to_char(buf, ch);
      }
      
      snprintf(buf, sizeof(buf), "WARNING: '%s' (%ld) attempted to attach '%s' (%ld) to '%s' (%ld), but the gun was full up on affects. Something needs revising."
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
  
  // Add the attachment's weight to the weapon's weight, but only if a player attached it.
  if (ch)
    weight_change_object(weapon, GET_OBJ_WEIGHT(attachment));
  
  // Add the attachment's cost to the weapon's cost, but only if a player attached it.
  if (ch)
    GET_OBJ_COST(weapon) = MAX(0, GET_OBJ_COST(weapon) + GET_OBJ_COST(attachment));
  
  // Update the weapon's aff flags.
  for (int waff_index = 0; acceptable_weapon_attachment_affects[waff_index] != -1; waff_index++) {
    if (attachment->obj_flags.bitvector.IsSet(acceptable_weapon_attachment_affects[waff_index])) {
      weapon->obj_flags.bitvector.SetBit(acceptable_weapon_attachment_affects[waff_index]);
    }
  }
  
  // Update the weapon's attach location to reflect this item.
  GET_OBJ_VAL(weapon, location + ACCESS_ACCESSORY_LOCATION_DELTA) = GET_OBJ_VNUM(attachment);
  
  // Send the success message, assuming there's a character.
  if (ch) {
    int where = location;
    
    snprintf(buf, sizeof(buf), "You attach $p to the %s of $P.",
            (where == 0 ? "top" : (where == 1 ? "barrel" : "underside")));
    act(buf, TRUE, ch, attachment, weapon, TO_CHAR);
    
    snprintf(buf, sizeof(buf), "$n attaches $p to the %s of $P.",
            (where == 0 ? "top" : (where == 1 ? "barrel" : "underside")));
    act(buf, TRUE, ch, attachment, weapon, TO_ROOM);
  } else {
#ifdef DEBUG_ATTACHMENTS
    snprintf(buf, sizeof(buf), "Successfully attached '%s' (%ld) to the %s of '%s' (%ld).",
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
    snprintf(buf, sizeof(buf), "SYSERR: Attempt to unattach item from invalid location %d on '%s' (%ld).",
            location, GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
  }
  
  if (GET_OBJ_TYPE(weapon) != ITEM_WEAPON) {
    if (ch)
      send_to_char("You can only unattach accessories from weapons.\r\n", ch);
    else {
      snprintf(buf, sizeof(buf), "SYSERR: Attempting to unattach something from non-weapon '%s' (%ld).",
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
    snprintf(buf, sizeof(buf), "SYSERR: Attempting to unattach invalid vnum %d from %s of weapon '%s' (%ld).",
            GET_WEAPON_ATTACH_LOC(weapon, location), gun_accessory_locations[location - ACCESS_ACCESSORY_LOCATION_DELTA], GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    
    // We use a raw get_obj_val here so we can set it.
    GET_OBJ_VAL(weapon, location) = 0;
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
      snprintf(buf, sizeof(buf), "WARNING: '%s' (%ld) unattached '%s' (%ld) from '%s' (%ld), but the gun was missing the attachment's affect. Something needs revising.",
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
  
  // Cycle through all the viable attachment affect flags.
  for (int waff_index = 0; acceptable_weapon_attachment_affects[waff_index] != -1; waff_index++) {
    // If we would remove one, first look at the other attachments (if any) and make sure they don't have it too.
    if (attachment->obj_flags.bitvector.IsSet(acceptable_weapon_attachment_affects[waff_index])) {
      bool found_duplicate = FALSE;
      for (int attach_loc = ACCESS_LOCATION_TOP; attach_loc <= ACCESS_LOCATION_UNDER; attach_loc++) {
        // Don't compare against ourselves.
        if (attach_loc == location)
          continue;
        
        // Skip if nothing's attached there.
        if (GET_WEAPON_ATTACH_LOC(weapon, attach_loc) <= 0)
          continue;
        
        // Resolve the thing into a vnum and skip if it's not kosher.
        long rnum = real_object(GET_WEAPON_ATTACH_LOC(weapon, attach_loc));
        if (rnum < 0)
          continue;
        
        // Grab the thing's prototype and check it for this flag.
        if ((&obj_proto[rnum])->obj_flags.bitvector.IsSet(acceptable_weapon_attachment_affects[waff_index])) {
          found_duplicate = TRUE;
          break;
        }
      }
      
      // Didn't find another item with this flag, so un-set it.
      if (!found_duplicate)
        weapon->obj_flags.bitvector.RemoveBit(acceptable_weapon_attachment_affects[waff_index]);
    }
  }
  
  // Update the weapon's attach location to reflect this item.
  GET_OBJ_VAL(weapon, location) = 0;
  
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

// Uses static, so don't use it more than once per call (to snprintf, etc)
char *double_up_color_codes(const char *string) {
  static char doubledbuf[MAX_STRING_LENGTH];
  
  // This will happen for night descs that haven't been set, etc.
  if (!string)
    return NULL;
  
  if (strlen(string) * 2 + 1 > sizeof(doubledbuf)) {
    mudlog("SYSERR: Size of string passed to double_up_color_codes exceeds max size; aborting process.", NULL, LOG_SYSLOG, TRUE);
    return NULL;
  }
  
  
  const char *read_ptr = string;
  char *write_ptr = doubledbuf;
  
  while (*read_ptr) {
    if (*read_ptr == '^')
      *(write_ptr++) = '^';
    *(write_ptr++) = *(read_ptr++);
  }
  *(write_ptr++) = '\0';
  return doubledbuf;
}

// Wipes out all the various fiddly bits so we don't have to remember to do it every time.
void clear_editing_data(struct descriptor_data *d) {
  // This is distinct from free_editing_structs()! That one purges out memory, this just unsets flags.
  d->edit_number = 0;
  PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
  d->edit_convert_color_codes = FALSE;
  
  // We're setting things to NULL here. If you don't want to leak memory, clean it up beforehand.
  d->edit_room = NULL;
}

// Sets a character's skill, with bounds. Assumes that you've already deducted the appropriate cost.
void set_character_skill(struct char_data *ch, int skill_num, int new_value, bool send_message) {
  char msgbuf[500];
  
  if (!ch) {
    mudlog("SYSERR: NULL character passed to set_character_skill.", ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  if (IS_NPC(ch)) {
    mudlog("SYSERR: NPC passed to set_character_skill.", ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  if (skill_num < MIN_SKILLS || skill_num >= MAX_SKILLS) {
    snprintf(msgbuf, sizeof(msgbuf), "SYSERR: Invalid skill number %d passed to set_character_skill.", skill_num);
    mudlog(msgbuf, ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  if (new_value < 0 || (!access_level(ch, LVL_BUILDER) && new_value > MAX_SKILL_LEVEL_FOR_MORTS)) {
    snprintf(msgbuf, sizeof(msgbuf), "SYSERR: Attempting to assign skill level %d to %s, which exceeds range 0 <= x <= %d.",
            new_value, GET_CHAR_NAME(ch), access_level(ch, LVL_BUILDER) ? MAX_SKILL_LEVEL_FOR_IMMS : MAX_SKILL_LEVEL_FOR_MORTS);
    mudlog(msgbuf, ch, LOG_SYSLOG, TRUE);
    return;
  }
  
  if (new_value == REAL_SKILL(ch, skill_num)) {
    // This is not an error condition (think restoring an imm and skillsetting everything); just silently fail.
    return;
  }
  
  if (send_message) {    
    // Active skill messaging.
    if (skills[skill_num].type == SKILL_TYPE_ACTIVE) {
      if (new_value == 0) {
        send_to_char(ch, "You completely forget your skills in %s.\r\n", skills[skill_num].name);
      } else if (new_value == 1) {
        send_to_char(ch, "^cYou have been introduced to the basics.^n\r\n");
      } else if (new_value == 2) {
        send_to_char(ch, "^cYou have gotten in some practice.^n\r\n");
      } else if (new_value == 3) {
        send_to_char(ch, "^cYou have attained average proficiency.^n\r\n");
      } else if (new_value == 4) {
        send_to_char(ch, "^CYour skills are now above average.^n\r\n");
      } else if (new_value == 5) {
        send_to_char(ch, "^CYou are considered a professional at %s.^n\r\n", skills[skill_num].name);
      } else if (new_value == 6) {
        send_to_char(ch, "^gYou've practiced so much that you can act without thinking about it.^n\r\n");
      } else if (new_value == 7) {
        send_to_char(ch, "^gYou are considered an expert in your field.^n\r\n");
      } else if (new_value == 8) {
        send_to_char(ch, "^GYour talents at %s are considered world-class.^n\r\n", skills[skill_num].name);
      } else {
        send_to_char(ch, "^GYou further hone your talents towards perfection.^n\r\n");
      }
    }
    // Knowledge skill messaging.
    else {
      if (new_value == 0) {
        send_to_char(ch, "You completely forget your knowledge of %s.\r\n", skills[skill_num].name);
      } else if (new_value == 1) {
        send_to_char(ch, "^cYou've picked up a few things.^n\r\n");
      } else if (new_value == 2) {
        send_to_char(ch, "^cYou've developed an interest in %s.^n\r\n", skills[skill_num].name);
      } else if (new_value == 3) {
        send_to_char(ch, "^cYou have a dedicated knowledge of that area.^n\r\n");
      } else if (new_value == 4) {
        send_to_char(ch, "^CYou are well-rounded in the field of %s.^n\r\n", skills[skill_num].name);
      } else if (new_value == 5) {
        send_to_char(ch, "^CYou could earn a degree in %s.^n\r\n", skills[skill_num].name);
      } else if (new_value == 6) {
        send_to_char(ch, "^gYou have mastered the field of %s.^n\r\n", skills[skill_num].name);
      } else if (new_value == 7) {
        send_to_char(ch, "^gYou are considered an expert in your field.^n\r\n");
      } else if (new_value == 8) {
        send_to_char(ch, "^GYour knowledge of %s is genius-level.^n\r\n", skills[skill_num].name);
      } else {
        send_to_char(ch, "^GYou further hone your knowledge towards perfection.^n\r\n");
      }
     }
  }
  
  // Update their skill.
  (ch)->char_specials.saved.skills[skill_num][0] = new_value;
  
  // Set the dirty bit so we know we need to save their skills.
  GET_SKILL_DIRTY_BIT((ch)) = TRUE;
}

// Per SR3 core p98-99.
const char *skill_rank_name(int rank, bool knowledge) {
#define RANK_MESSAGE(value, active_name, knowledge_name) { \
    if (rank == value) { \
      if (knowledge) return knowledge_name; \
      else return active_name; \
   } \
}
  
  if (rank < 0)
    return "uh oh! you have a negative skill, please report!";
  
  RANK_MESSAGE(0, "not learned", "not learned");
  RANK_MESSAGE(1, "introduced", "scream-sheet level");
  RANK_MESSAGE(2, "practiced", "interested");
  RANK_MESSAGE(3, "proficient", "dedicated");
  RANK_MESSAGE(4, "skilled", "well-rounded");
  RANK_MESSAGE(5, "professional", "educated");
  RANK_MESSAGE(6, "innate", "mastered");
  RANK_MESSAGE(7, "expert", "expert");
  
  if (rank < MAX_SKILL_LEVEL_FOR_IMMS) {
    if (knowledge) return "genius";
    else return "world-class";
  }
  
  return "godly";
#undef RANK_MESSAGE
}

char *how_good(int skill, int rank)
{
  static char buf[256];
  snprintf(buf, sizeof(buf), " (%s / rank %d)", skill_rank_name(rank, skills[skill].type == SKILL_TYPE_KNOWLEDGE), rank);
  return buf;
}

/*
Returns total length of the string that would have been created.
*/
size_t strlcpy(char *buf, const char *src, size_t bufsz)
{
    size_t src_len = strlen(src);
    size_t rtn = src_len;

    if (bufsz > 0)
    {
        if (src_len >= bufsz) 
        {
            src_len = bufsz - 1;
        }
        memcpy(buf, src, src_len);
        buf[src_len] = '\0';
    }

    return rtn;
}

/*
Returns total length of the string that would have been created.
*/
size_t strlcat(char *buf, const char *src, size_t bufsz)
{
    size_t buf_len = strlen(buf);
    size_t src_len = strlen(src);
    size_t rtn = buf_len + src_len;

    if (buf_len < (bufsz - 1))
    {
        if (src_len >= (bufsz - buf_len))
        {
            src_len = bufsz - buf_len - 1;
        }
        memcpy(buf + buf_len, src, src_len);
        buf[buf_len + src_len] = '\0';
    }

    return rtn;
}

// Un-nests contained objects until it figures out who's actually carrying the object (if anyone).
struct char_data *get_obj_carried_by_recursive(struct obj_data *obj) {
  if (!obj)
    return NULL;
  
  if (obj->carried_by)
    return obj->carried_by;
  
  if (obj->in_obj)
    return get_obj_carried_by_recursive(obj->in_obj);
  
  return NULL;
}

// Un-nests contained objects until it figures out who's actually wearing the object (if anyone).
struct char_data *get_obj_worn_by_recursive(struct obj_data *obj) {
  if (!obj)
    return NULL;
  
  if (obj->worn_by)
    return obj->worn_by;
  
  if (obj->in_obj)
    return get_obj_worn_by_recursive(obj->in_obj);
  
  return NULL;
}

// Finds the object's holder (either carrying or wearing it or its parent object recursively).
struct char_data *get_obj_possessor(struct obj_data *obj) {
  struct char_data *owner;
  
  if ((owner = get_obj_carried_by_recursive(obj)))
    return owner;
  
  return get_obj_worn_by_recursive(obj);
}

// Creates a NEW loggable string from an object. YOU MUST DELETE [] THE OUTPUT OF THIS.
char *generate_new_loggable_representation(struct obj_data *obj) {
  char log_string[MAX_STRING_LENGTH];
  memset(log_string, 0, sizeof(char) * MAX_STRING_LENGTH);
  
  if (!obj) {
    strcpy(log_string, "SYSERR: Null object passed to generate_loggable_representation().");
    mudlog(log_string, NULL, LOG_SYSLOG, TRUE);
    return str_dup(log_string);
  }
  
  snprintf(log_string, sizeof(log_string), "(obj %ld) %s%s",
          GET_OBJ_VNUM(obj),
          obj->text.name,
          IS_OBJ_STAT(obj, ITEM_WIZLOAD) ? " [wizloaded]" : "");
    
  if (obj->restring)
    snprintf(ENDOF(log_string), sizeof(log_string) - strlen(log_string), " [restring: %s]", obj->restring);
  
  if (obj->contains) {
    snprintf(ENDOF(log_string), sizeof(log_string) - strlen(log_string), ", containing: [");
    for (struct obj_data *temp = obj->contains; temp; temp = temp->next_content) {
      char *representation = generate_new_loggable_representation(temp);
      snprintf(buf3, sizeof(buf3), " %s%s%s",
              (!temp->next_content && temp != obj->contains) ? "and " : "",
              representation,
              temp->next_content ? ";" : "");
      strlcat(log_string, buf3, MAX_STRING_LENGTH);
      delete [] representation;
    }
    strlcat(log_string, " ]", MAX_STRING_LENGTH);
  }
  
  switch(GET_OBJ_TYPE(obj)) {
    case ITEM_MONEY:
      // The only time we'll ever hit perform_give with money is if it's a credstick.
      snprintf(ENDOF(log_string), sizeof(log_string) - strlen(log_string), ", containing %d nuyen", GET_OBJ_VAL(obj, 0));
      break;
    case ITEM_DECK_ACCESSORY:
      // Computer parts and optical chips.
      if (GET_OBJ_VAL(obj, 0) == TYPE_PARTS) {
        snprintf(ENDOF(log_string), sizeof(log_string) - strlen(log_string), ", containing %d nuyen worth of %s", GET_OBJ_COST(obj), GET_OBJ_VAL(obj, 1) ? "optical chips" : "parts");
      }
      break;
    case ITEM_MAGIC_TOOL:
      // Summoning materials.
      if (GET_OBJ_VAL(obj, 0) == TYPE_SUMMONING) {
        snprintf(ENDOF(log_string), sizeof(log_string) - strlen(log_string), ", containing %d nuyen worth of summoning materials", GET_OBJ_COST(obj));
      }
      break;
    case ITEM_GUN_AMMO:
      // A box of ammunition.
      snprintf(ENDOF(log_string), sizeof(log_string) - strlen(log_string), ", containing %d %s", 
               GET_AMMOBOX_QUANTITY(obj), 
               get_ammo_representation(GET_AMMOBOX_WEAPON(obj), GET_AMMOBOX_TYPE(obj), GET_AMMOBOX_QUANTITY(obj)));
      break;
    case ITEM_GUN_MAGAZINE:
      // A magazine.
      snprintf(ENDOF(log_string), sizeof(log_string) - strlen(log_string), ", containing %d %s", 
               GET_MAGAZINE_AMMO_COUNT(obj), 
               get_ammo_representation(GET_MAGAZINE_BONDED_ATTACKTYPE(obj), GET_MAGAZINE_AMMO_TYPE(obj), GET_MAGAZINE_AMMO_COUNT(obj)));
    default:
      break;
  }
  
  if (GET_OBJ_VNUM(obj) == OBJ_NEOPHYTE_SUBSIDY_CARD) {
    snprintf(ENDOF(log_string), sizeof(log_string) - strlen(log_string), ", bonded to character id %d with %d nuyen on it", 
             GET_OBJ_VAL(obj, 0),
             GET_OBJ_VAL(obj, 1));
  }
  
  return str_dup(log_string);
}

// Purging a vehicle? Call this function to log it.
void purgelog(struct veh_data *veh) {
  char internal_buffer[MAX_STRING_LENGTH];
  const char *representation = NULL;
  
  // Begin the purgelog entry.
  snprintf(internal_buffer, sizeof(internal_buffer), "- Writing purgelog for vehicle %s (vnum %ld, idnum %ld, owned by %ld).", GET_VEH_NAME(veh), GET_VEH_VNUM(veh), veh->idnum, veh->owner);
  mudlog(internal_buffer, NULL, LOG_PURGELOG, TRUE);
  
  // Log its mounts.
  for (struct obj_data *mount = veh->mount; mount; mount = mount->next_content) {
    representation = generate_new_loggable_representation(mount);
    snprintf(internal_buffer, sizeof(internal_buffer), "- Mount: %s", representation);
    mudlog(internal_buffer, NULL, LOG_PURGELOG, TRUE);
    delete [] representation;
  }
  
  // Log its mods.
  for (int x = 0; x < NUM_MODS; x++) {
    if (!GET_MOD(veh, x))
      continue;
    
    representation = generate_new_loggable_representation(GET_MOD(veh, x));
    snprintf(internal_buffer, sizeof(internal_buffer), "- Mod attached to the %s: %s", mod_name[x], representation);
    mudlog(internal_buffer, NULL, LOG_PURGELOG, TRUE);
    delete [] representation;
  }
  
  // Log its contained objects.
  for (struct obj_data *obj = veh->contents; obj; obj = obj->next_content) {
    representation = generate_new_loggable_representation(obj);
    snprintf(internal_buffer, sizeof(internal_buffer), "- Contained object: %s", representation);
    mudlog(internal_buffer, NULL, LOG_PURGELOG, TRUE);
    delete [] representation;
  }
  
  // End the purgelog entry.
  snprintf(internal_buffer, sizeof(internal_buffer), "- End purgelog for %s.", GET_VEH_NAME(veh));
  mudlog(internal_buffer, NULL, LOG_PURGELOG, TRUE);
}

// Copy Source into Dest, replacing the target substring in Source with the specified replacement.
// Requirement: dest's max size must be greater than source's current size plus all the replacements being done in it.
char *replace_substring(char *source, char *dest, const char *replace_target, const char *replacement) {
  const char *replace_target_ptr = replace_target;
  char *dest_ptr = dest;
  
  *dest = '\0';
  
  for (unsigned long i = 0; i < strlen(source); i++) {
    // Compare the source to our replacement target pointer and increment RTP.
    if (source[i] == *(replace_target_ptr)) {
      // If they are equal, check to see if we've reached the end of RTP.
      if (!*(++replace_target_ptr)) {
        // If we have, we've matched the full thing. Plop the replacement into dest and reset.
        for (unsigned long j = 0; j < strlen(replacement); j++)
          *(dest_ptr++) = *(replacement + j);
        replace_target_ptr = replace_target;
      } else {
        // If we haven't, do nothing-- just continue.
      }
    } else {
      // They're not equal. If we've incremented RTP, we know we've skipped characters, so it's time to unwind that.
      for (unsigned long diff = replace_target_ptr - replace_target; diff > 0; diff--) {
        *(dest_ptr++) = source[i-diff];
      }
        
      // Now, reset RTP and copy the most recent character directly.
      replace_target_ptr = replace_target;
      *(dest_ptr++) = source[i];
    }
  }
  
  // Finally, null-termanate dest.
  *dest_ptr = '\0';
  
  // Return the dest they gave us.
  return dest;
}

// Adds the amount to the ammobox, then processes its weight etc. 
void update_ammobox_ammo_quantity(struct obj_data *ammobox, int amount) {
  if (!ammobox) {
    mudlog("SYSERR: Null ammobox passed to update_ammobox_ammo_quantity.", ammobox->carried_by, LOG_SYSLOG, TRUE);
    return;
  }
  
  if (amount == 0) {
    mudlog("SYSERR: Zero-quantity ammobox passed to update_ammobox_ammo_quantity.", ammobox->carried_by, LOG_SYSLOG, TRUE);
    return;
  }
  
  if (GET_OBJ_TYPE(ammobox) != ITEM_GUN_AMMO) {
    mudlog("SYSERR: Non-ammobox passed to update_ammobox_ammo_quantity.", ammobox->carried_by, LOG_SYSLOG, TRUE);
  }
  
  // Calculate what the new amount of ammo will be.
  GET_AMMOBOX_QUANTITY(ammobox) = GET_AMMOBOX_QUANTITY(ammobox) + amount;
  
  if (GET_AMMOBOX_QUANTITY(ammobox) < 0) {
    mudlog("SYSERR: Updated ammobox to have negative ammo count! Restoring...", NULL, LOG_SYSLOG, TRUE);
    GET_AMMOBOX_QUANTITY(ammobox) = 0;
  }
  
  // Update the box's weight.
  weight_change_object(ammobox, ammo_type[GET_AMMOBOX_TYPE(ammobox)].weight * amount);
  
  // Calculate cost as count * multiplier (multiplier is per round)
  GET_OBJ_COST(ammobox) = GET_AMMOBOX_QUANTITY(ammobox) * get_ammo_cost(GET_AMMOBOX_WEAPON(ammobox), GET_AMMOBOX_TYPE(ammobox));
  
  // Update the carrier's carry weight.
  if (ammobox->carried_by) {
    calc_weight(ammobox->carried_by);
  }  
}


bool combine_ammo_boxes(struct char_data *ch, struct obj_data *from, struct obj_data *into, bool print_messages) {
  if (!ch || !from || !into) {
    mudlog("SYSERR: combine_ammo_boxes received a null value.", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  if (GET_AMMOBOX_CREATOR(from) || GET_AMMOBOX_CREATOR(into)) {
    mudlog("SYSERR: combine_ammo_boxes received a box that was not yet completed.", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  // If the weapons don't match, no good.
  if (GET_AMMOBOX_WEAPON(from) != GET_AMMOBOX_WEAPON(into)) {
    mudlog("SYSERR: combine_ammo_boxes received boxes with non-matching weapon types.", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  // If the ammo types don't match, no good.
  if (GET_AMMOBOX_TYPE(from) != GET_AMMOBOX_TYPE(into)) {
    mudlog("SYSERR: combine_ammo_boxes received boxes with non-matching ammo types.", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  
  if (GET_AMMOBOX_QUANTITY(from) == 0) {
    if (print_messages)
      send_to_char(ch, "But %s is empty...\r\n", GET_OBJ_NAME(from));
    return FALSE;
  }
  
  update_ammobox_ammo_quantity(into, GET_AMMOBOX_QUANTITY(from));
  update_ammobox_ammo_quantity(from, -GET_AMMOBOX_QUANTITY(from));
  
  // Notify the owner, then destroy the empty.
  if (!from->restring || strcmp(from->restring, get_ammobox_default_restring(from)) == 0) {
    if (print_messages) {
      send_to_char(ch, "You dump the ammo into %s and throw away the empty box.\r\n", GET_OBJ_NAME(into) );
    }
    
    extract_obj(from);
  } else {
    if (print_messages) {
      send_to_char(ch, "You dump the ammo from %s into %s, but hang on to the customized empty container.\r\n",
        GET_OBJ_NAME(from),
        GET_OBJ_NAME(into)
      );
    }
  }
  
  // Restring it, as long as it's not already restrung. Use n_s_b so we don't accidentally donk someone's buf.
  if (!into->restring) {
    char notification_string_buf[500];
    
    // Compose the new name.
    const char *restring = get_ammobox_default_restring(into);
    
    // Compose the notification string.
    snprintf(notification_string_buf, sizeof(notification_string_buf), "The name '%s' probably doesn't fit anymore, so we'll call it '%s'.\r\n",
      GET_OBJ_NAME(into),
      restring
    );
    
    // Commit the change and notify the player.
    into->restring = str_dup(restring);
    
    if (print_messages)
      send_to_char(notification_string_buf, ch);
  }
  
  // Everything succeeded.
  return TRUE;
}

void destroy_door(struct room_data *room, int dir) {
  if (!room || dir < NORTH || dir > DOWN)
    return;
    
  room->dir_option[dir]->condition = 0;
  REMOVE_BIT(room->dir_option[dir]->exit_info, EX_CLOSED);
  REMOVE_BIT(room->dir_option[dir]->exit_info, EX_LOCKED);
  REMOVE_BIT(room->dir_option[dir]->exit_info, EX_HIDDEN);
  SET_BIT(room->dir_option[dir]->exit_info, EX_DESTROYED);
}

bool spell_is_nerp(int spell_num) {
  switch (spell_num) {
    case SPELL_MANABALL:
    case SPELL_POWERBALL:
    case SPELL_STUNBALL:
    case SPELL_ANALYZEDEVICE:
    case SPELL_CLAIRAUDIENCE:
    case SPELL_CLAIRVOYANCE:
    case SPELL_DETECTENEMIES:
    case SPELL_DETECTINDIV:
    case SPELL_DETECTLIFE:
    case SPELL_DETECTMAGIC:
    case SPELL_DETECTOBJECT:
    case SPELL_PROPHYLAXIS:
    case SPELL_MASSCONFUSION:
    case SPELL_CHAOTICWORLD:
    case SPELL_MASK:
    case SPELL_PHYSMASK:
    case SPELL_FIREBALL:
    case SPELL_BALLLIGHTNING:
    case SPELL_PHYSICALBARRIER:
    case SPELL_ASTRALBARRIER:
      return TRUE;
  }
  
  return FALSE;
}

// Gets the last non-zero, non-whitespace character from a given string. Returns 0 on error.
char get_final_character_from_string(const char *str) {
  if (!str || !*str)
    return 0;
    
  for (int i = strlen(str) - 1; i >= 0; i--)
    if (str[i] != '\r' && str[i] != '\n' && str[i] != ' ')
      return str[i];
  
  return 0;
}

// Pass in an object's vnum during world loading and this will tell you what the authoritative vnum is for it.
// Great for swapping out old Classic weapons, cyberware, etc for the new guaranteed-canon versions.
#define PAIR(classic, current) case (classic): return (current);
vnum_t get_authoritative_vnum_for_item(vnum_t vnum) {
  switch (vnum) {
    // Cyberdecks.
    PAIR(1820,   80001);
    PAIR(1116,   80002);
    PAIR(1916,   80003);
    PAIR(29005,  80004);
    PAIR(7418,   80005);
    
    // Melee weapons.
    PAIR(632, 80100);
    PAIR(652, 80101);
    // More to do here, it's just difficult to sort through the wall of items.
    
    // Ranged weapons.
    PAIR(728,   80200);
    PAIR(731,   80201);
    PAIR(618,   80202);
    PAIR(734,   80203);
    PAIR(60501, 80203);
    PAIR(2035,  80204);
    PAIR(619,   80205);
    PAIR(739,   80206);
    PAIR(668,   80207);
    PAIR(19892, 80208);
    PAIR(758,   80209);
    PAIR(623,   80210);
    PAIR(39202, 80210);
    PAIR(747,   80211);
    PAIR(776,   80212);
    PAIR(837,   80217);
    PAIR(779,   80213);
    PAIR(39203, 80213);
    PAIR(60273, 80213);
    PAIR(781,   80215);
    PAIR(785,   80215);
    PAIR(841,   80218);
    PAIR(20004, 80218);
    PAIR(826,   80219);
    PAIR(827,   80219);
    PAIR(832,   80221);
    PAIR(631,   80222);
    PAIR(676,   80223);
    PAIR(878,   80224);
    PAIR(881,   80225);
    PAIR(880,   80226);
    PAIR(883,   80227);
    
    // TODO: Pair things before this with mismatched names, have pairing only overwrite stats.
    
    // Weapon attachable accessories.
    PAIR(654,   80400);
    PAIR(650,   80401);
    PAIR(7218,  80401);
    PAIR(14844, 80401);
    PAIR(60551, 80401);
    PAIR(653,   80402);
    PAIR(30153, 80402);
    PAIR(691,   80403);
    PAIR(2319,  80403);
    PAIR(18981, 80404);
    PAIR(35038, 80404);
    PAIR(38030, 80404);
    PAIR(697,   80405);
    PAIR(698,   80406);
    PAIR(28704, 80406);
    PAIR(10007, 80407);
    PAIR(28640, 80407);
    PAIR(28703, 80407);
    PAIR(30152, 80407);
    PAIR(651,   80408);
    PAIR(32229, 80408);
    PAIR(9154,  80411); // Note that the 409-411 range is electronic scopes, we don't have optical scopes implemented.
    PAIR(27812, 80411);
    PAIR(30136, 80411);
    PAIR(60553, 80411);
    PAIR(2325,  80413);
    PAIR(28702, 80415);
    
    // Weapon other accessories (holsters, gyros, etc)
    PAIR(64963, 80502);
    PAIR(64964, 80501);
    
    // Armor (80700-80799) TODO, I don't have the patience to deal with this right now.
    
    // Cyberware. All 125 items of it, hand-compared with other cyberware. I hope y'all realize just how much dedication this takes.
    PAIR(325,   85000);
    PAIR(487,   85001);
    PAIR(488,   85002);
    PAIR(489,   85003);
    PAIR(503,   85004);
    PAIR(531,   85200);
    PAIR(530,   85201);
    PAIR(529,   85202);
    PAIR(528,   85203);
    PAIR(514,   85007);
    PAIR(515,   85008);
    PAIR(516,   85009);
    PAIR(517,   85010);
    PAIR(521,   85209);
    PAIR(522,   85208);
    PAIR(523,   85207);
    PAIR(33220, 85210);
    PAIR(33221, 85410);
    PAIR(304,   80512);
    PAIR(444,   85212);
    PAIR(486,   85015);
    PAIR(511,   85045);
    PAIR(32234, 85612);
    PAIR(39370, 85245);
    PAIR(39337, 85215);
    PAIR(1106,  85013);
    PAIR(1107,  85014);
    PAIR(23371, 85613);
    PAIR(23382, 85614);
    PAIR(29027, 85013);
    PAIR(39380, 85213);
    PAIR(39381, 85214);
    PAIR(491,   85016);
    PAIR(526,   85216);
    PAIR(33202, 85416);
    PAIR(533,   85017);
    PAIR(1108,  85017);
    PAIR(1109,  85018);
    PAIR(1110,  85019);
    PAIR(23369, 85617);
    PAIR(22370, 85618);
    PAIR(23379, 85619);
    PAIR(25498, 85419);
    PAIR(39378, 85218);
    PAIR(39379, 85219);
    PAIR(505,   85022);
    PAIR(1918,  85021);
    PAIR(371,   85023);
    PAIR(25496, 85623);
    PAIR(33249, 85223);
    PAIR(50306, 85623);
    PAIR(373,   85024);
    PAIR(33248, 85224);
    PAIR(502,   85025);
    PAIR(427,   85034); // Vehicle control rigs.
    PAIR(428,   85035);
    PAIR(429,   85036);
    PAIR(464,   85235);
    PAIR(465,   85435);
    PAIR(466,   85236);
    PAIR(467,   85436);
    PAIR(532,   85234);
    PAIR(493,   85037); // oral 'ware
    PAIR(494,   80538);
    PAIR(524,   85238);
    PAIR(525,   85237);
    PAIR(492,   85039);
    PAIR(33264, 85040);
    PAIR(39721, 85041);
    PAIR(39722, 85241);
    PAIR(507,   85043); // eyeware
    PAIR(557,   85243);
    PAIR(506,   85042);
    PAIR(509,   85051);
    PAIR(512,   85047);
    PAIR(513,   85046);
    PAIR(554,   85246);
    PAIR(555,   85247);
    PAIR(558,   85242);
    PAIR(33223, 85442);
    PAIR(305,   85048);
    PAIR(559,   85248);
    PAIR(508,   85049);
    PAIR(556,   85249);
    PAIR(306,   85050);
    PAIR(565,   85250);
    PAIR(510,   85052);
    PAIR(39371, 85252);
    
    // Bioware.
    PAIR(1461, 85802); // cats eyes
    PAIR(60344, 85802);
    PAIR(33254, 85902);
    // WIP
  }
  
  // No match.
  return vnum;
}
#undef PAIR
