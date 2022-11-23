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

#if defined(WIN32) && !defined(__CYGWIN__)
#include <winsock.h>
#define random() rand()
#define srandom(x) srand(x)
#else
#include <netinet/in.h>
#include <unistd.h>
#endif

#include "telnet.hpp"
#include "structs.hpp"
#include "utils.hpp"
#include "awake.hpp"
#include "comm.hpp"
#include "handler.hpp"
#include "memory.hpp"
#include "house.hpp"
#include "db.hpp"
#include "constants.hpp"
#include "newmagic.hpp"
#include "list.hpp"
#include "newdb.hpp"
#include "config.hpp"
#include "bullet_pants.hpp"
#include "invis_resistance_tests.hpp"
#include "vision_overhaul.hpp"

extern class memoryClass *Mem;
extern struct time_info_data time_info;

extern const char *log_types[];
extern long beginning_of_time;
extern int ability_cost(int abil, int level);
extern void weight_change_object(struct obj_data * obj, float weight);
extern void calc_weight(struct char_data *ch);
extern const char *get_ammobox_default_restring(struct obj_data *ammobox);
extern bool can_edit_zone(struct char_data *ch, rnum_t real_zone);
extern int find_first_step(vnum_t src, vnum_t target, bool ignore_roads);

extern SPECIAL(johnson);
extern SPECIAL(landlord_spec);
extern SPECIAL(receptionist);
extern SPECIAL(shop_keeper);
extern SPECIAL(postmaster);
extern SPECIAL(generic_guard);
extern SPECIAL(receptionist);
extern SPECIAL(cryogenicist);
extern SPECIAL(teacher);
extern SPECIAL(metamagic_teacher);
extern SPECIAL(trainer);
extern SPECIAL(adept_trainer);
extern SPECIAL(spell_trainer);
extern SPECIAL(fixer);
extern SPECIAL(hacker);
extern SPECIAL(fence);
extern SPECIAL(taxi);
extern SPECIAL(painter);
extern SPECIAL(nerp_skills_teacher);

bool npc_can_see_in_any_situation(struct char_data *npc);

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

// If supplying
int success_test(int number, int target, struct char_data *ch, const char *note_for_rolls, struct char_data *other)
{
  if (number < 1)
    return BOTCHED_ROLL_RESULT;

  int total = 0, ones = 0;

  target = MAX(target, 2);

  for (int i = 1; i <= number; i++) {
    int roll = srdice();

    // A 1 is always a failure, as no TN can be less than 2.
    if (roll == 1) {
      ones++;
    }
    else if (roll >= target) {
      total++;
    }
  }

  if (ch) {
    char msgbuf[1000];
    snprintf(msgbuf, sizeof(msgbuf), "^L%s^L rolled %d %s VS TN %d: %d %s, %d %s.^n",
             GET_CHAR_NAME(ch),
             number,
             number == 1 ? "die" : "dice",
             target,
             total,
             total == 1 ? "success" : "successes",
             ones,
             ones == 1 ? "one" : "ones");
    if (note_for_rolls && *note_for_rolls) {
      snprintf(ENDOF(msgbuf), sizeof(msgbuf) - strlen(msgbuf), "^L (%s)^n", note_for_rolls);
    }
    act(msgbuf, TRUE, ch, 0, 0, TO_ROLLS);
    if (other && ch->in_room != other->in_room && ch->in_veh != other->in_veh) {
      act(msgbuf, TRUE, other, 0, 0, TO_ROLLS);
    }
  }

  if (ones == number)
    return BOTCHED_ROLL_RESULT;
  return total;
}

int resisted_test(int num4ch, int tar4ch, int num4vict, int tar4vict)
{
  return (success_test(num4ch, tar4ch) - success_test(num4vict, tar4vict));
}

int open_test(int num_dice) {
  int maximum_rolled = 0, sr_result;
  while (num_dice-- > 0) {
    sr_result = srdice();
    maximum_rolled = MAX(maximum_rolled, sr_result);
  }

  return maximum_rolled;
}

int stage(int successes, int wound)
{
  // This is just a little bit faster than what was below.
  return wound + (successes / 2);

  /*
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
  */
}

int convert_damage(int damage)
{
  if (damage < 0)
    damage = 0;
  else
    damage = damage_array[MAX(0, MIN(DEADLY, damage))];

  return damage;
}

int light_level(struct room_data *room)
{
  if (!room) {
    mudlog("SYSERR: light_level() called on null room.", NULL, LOG_SYSLOG, TRUE);
    return LIGHT_NORMAL;
  }

  int artificial_light_level = -1;

  // If we have streetlights, we don't care about flashlights etc-- we're guaranteeing partlight.
  if (ROOM_FLAGGED(room, ROOM_STREETLIGHTS) && (time_info.hours <= 6 || time_info.hours >= 19)) {
    artificial_light_level = LIGHT_PARTLIGHT;
  } else {
    int num_light_sources = room->light[ROOM_LIGHT_HEADLIGHTS_AND_FLASHLIGHTS] + room->light[ROOM_HIGHEST_SPELL_FORCE];

    // Light sources. More sources, more light.
    if (num_light_sources >= 2) {
      artificial_light_level = LIGHT_PARTLIGHT;
    } else if (num_light_sources) {
      artificial_light_level = LIGHT_MINLIGHT;
    }
  }

  int candidate_light_level = room->vision[0];

  switch (artificial_light_level) {
    // Minlight overrides full dark, otherwise we use the existing value.
    case LIGHT_MINLIGHT:
      if (room->vision[0] == LIGHT_FULLDARK)
        candidate_light_level = LIGHT_MINLIGHT;
      else
        candidate_light_level = room->vision[0];
      break;

    // Partlight overrides minlight and full dark, otherwise we use the existing value.
    case LIGHT_PARTLIGHT:
      if (room->vision[0] == LIGHT_FULLDARK || room->vision[0] == LIGHT_MINLIGHT)
        candidate_light_level = LIGHT_PARTLIGHT;
      else
        candidate_light_level = room->vision[0];
      break;

    // Normal light overrides normal-no-lit, minlight, and fulldark, otherwise we use the existing value.
    case LIGHT_NORMAL:
      if (room->vision[0] == LIGHT_FULLDARK || room->vision[0] == LIGHT_MINLIGHT || room->vision[0] == LIGHT_NORMALNOLIT)
        candidate_light_level = LIGHT_NORMAL;
      else
        candidate_light_level = room->vision[0];
      break;
  }

  // Indoor rooms have no weather or time-based light influences.
  if (ROOM_FLAGGED(room, ROOM_INDOORS))
    return candidate_light_level;

  // Outdoor city rooms (roads, etc) are impacted by ambient light.
  if (room->sector_type == SPIRIT_CITY) {
    // It's daytime. No changes.
    if ((time_info.hours > 6 && time_info.hours < 19)) {
      return candidate_light_level;
    }
    // It's nighttime, and this area is flagged as having no light.
    else if (candidate_light_level == LIGHT_NORMALNOLIT) {
      return MAX(LIGHT_MINLIGHT, artificial_light_level);
    }
    else
      return MAX(LIGHT_PARTLIGHT, artificial_light_level);
  }
  if ((time_info.hours <= 6 || time_info.hours >= 19) && (candidate_light_level > LIGHT_MINLIGHT || candidate_light_level <= LIGHT_NORMALNOLIT)) {
    return MAX(LIGHT_MINLIGHT, artificial_light_level);
  } else {
    return candidate_light_level;
  }
}

int damage_modifier(struct char_data *ch, char *rbuf, int rbuf_size)
{
  int physical = GET_PHYSICAL(ch) / 100;
  int mental = GET_MENTAL(ch) / 100;
  int base_target = 0;

  for (struct obj_data *obj = ch->bioware; obj; obj = obj->next_content) {
    if (GET_BIOWARE_TYPE(obj) == BIO_DAMAGECOMPENSATOR) {
      physical += GET_BIOWARE_RATING(obj);
      mental += GET_BIOWARE_RATING(obj);
    } else if (GET_BIOWARE_TYPE(obj) == BIO_PAINEDITOR && GET_BIOWARE_IS_ACTIVATED(obj))
      mental = 10;
  }
  if (AFF_FLAGGED(ch, AFF_RESISTPAIN))
  {
    physical += ch->points.resistpain;
    mental += ch->points.resistpain;
  }
  if (!IS_NPC(ch)) {
    if (GET_TRADITION(ch) == TRAD_ADEPT && GET_POWER(ch, ADEPT_PAIN_RESISTANCE) > 0)
    {
      physical += GET_POWER(ch, ADEPT_PAIN_RESISTANCE);
      mental += GET_POWER(ch, ADEPT_PAIN_RESISTANCE);
    }

    int drug_pain_resist = get_drug_pain_resist_level(ch);
    physical += drug_pain_resist;
    mental += drug_pain_resist;
  }

  // first apply physical damage modifiers
  if (physical <= 4)
  {
    base_target += 3;
    buf_mod(rbuf, rbuf_size, "Physical damage (S)", 3 );
  } else if (physical <= 7)
  {
    base_target += 2;
    buf_mod(rbuf, rbuf_size, "Physical damage (M)", 2 );
  } else if (physical <= 9)
  {
    base_target += 1;
    buf_mod(rbuf, rbuf_size, "Physical damage (L)", 1 );
  }
  if (mental <= 4)
  {
    base_target += 3;
    buf_mod(rbuf, rbuf_size, "Mental damage (S)", 3 );
  } else if (mental <= 7)
  {
    base_target += 2;
    buf_mod(rbuf, rbuf_size, "Mental damage (M)", 2 );
  } else if (mental <= 9)
  {
    base_target += 1;
    buf_mod(rbuf, rbuf_size, "Mental damage (L)", 1 );
  }
  return base_target;
}

int sustain_modifier(struct char_data *ch, char *rbuf, size_t rbuf_len, bool minus_one_sustained) {
  int base_target = 0;

  // Since NPCs don't have sustain foci available to them at the moment, we don't throw these penalties on them.
  if (IS_NPC(ch) && !IS_PROJECT(ch))
    return 0;

  if (GET_SUSTAINED_NUM(ch) > 0) {
    // We often create a sustained spell and THEN roll drain. minus_one_sustained accounts for this.
    int sustained_num = GET_SUSTAINED_NUM(ch) - GET_SUSTAINED_FOCI(ch);
    if (minus_one_sustained && sustained_num > 0)
      sustained_num--;

    int delta = sustained_num * 2;
    base_target += delta;
    buf_mod(rbuf, rbuf_len, "Sustain", delta);
  }

  if (IS_PROJECT(ch) && ch->desc && ch->desc->original) {
    if (GET_SUSTAINED_NUM(ch->desc->original) > 0) {
      int delta = (GET_SUSTAINED_NUM(ch->desc->original) - GET_SUSTAINED_FOCI(ch->desc->original)) * 2;
      base_target += delta;
      buf_mod(rbuf, rbuf_len, "Sustain", delta);
    }
  }

  return base_target;
}

// Adds the combat_mode toggle
int modify_target_rbuf_raw(struct char_data *ch, char *rbuf, int rbuf_len, int current_visibility_penalty, bool skill_is_magic) {
  extern time_info_data time_info;
  int base_target = 0;
  // get damage modifier
  base_target += damage_modifier(ch, rbuf, rbuf_len);
  // then apply modifiers for sustained spells
  base_target += sustain_modifier(ch, rbuf, rbuf_len);

  struct room_data *temp_room = get_ch_in_room(ch);

  if (!temp_room) {
    mudlog("SYSERR: modify_target_rbuf_raw received char with NO room!", ch, LOG_SYSLOG, TRUE);
    return 100;
  }

  bool is_rigging = (AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE));
  if (is_rigging) {
    struct veh_data *veh;
    RIG_VEH(ch, veh);
    temp_room = get_veh_in_room(veh);
  }

  // Magic skill with certain drugs? Take a penalty.
  if (skill_is_magic && GET_CONCENTRATION_TARGET_MOD(ch)) {
    base_target += GET_CONCENTRATION_TARGET_MOD(ch);
    buf_mod(rbuf, rbuf_len, "DrugConcTN", GET_CONCENTRATION_TARGET_MOD(ch));
  }

  // If you're astrally perceiving, you don't take additional vision penalties, and shouldn't have any coming in here.
  if (!is_rigging && SEES_ASTRAL(ch))
  {
    if (!skill_is_magic && IS_PERCEIVING(ch)) {
      base_target += 2;
      buf_mod(rbuf, rbuf_len, "AstralPercep", 2);
    }
  }
  // Otherwise, check to see if you've exceeded the vision pen max coming in here. This only happens for totalinvis and staff opponents.
  else if (current_visibility_penalty >= MAX_VISIBILITY_PENALTY) {
    int new_visibility_penalty = MAX_VISIBILITY_PENALTY - current_visibility_penalty;
    buf_mod(rbuf, rbuf_len, "PreconditionVisPenaltyMax8", new_visibility_penalty);
    base_target += new_visibility_penalty;
  }
  // If we're within the allowed amount, calculate the remaining vision penalty, capping at 8.
  else {
    int visibility_penalty = get_vision_penalty(ch, temp_room, rbuf, rbuf_len);

    if (visibility_penalty > (MAX_VISIBILITY_PENALTY - current_visibility_penalty)) {
      // We already printed all their modifiers, so we need to apply a negative modifier to clamp them back to 8.
      int new_visibility_penalty = MAX_VISIBILITY_PENALTY - current_visibility_penalty;
      buf_mod(rbuf, rbuf_len, "VisPenaltyMax8", new_visibility_penalty);
      visibility_penalty = new_visibility_penalty;
    }

    base_target += visibility_penalty;
  }

  base_target += GET_TARGET_MOD(ch);
  buf_mod(rbuf, rbuf_len, "GET_TARGET_MOD", GET_TARGET_MOD(ch) );
  if (GET_RACE(ch) == RACE_NIGHTONE && ((time_info.hours > 6) && (time_info.hours < 19)) && OUTSIDE(ch) && weather_info.sky < SKY_RAINING)
  {
    base_target += 1;
    buf_mod(rbuf, rbuf_len, "Sunlight", 1);
  }
  if (temp_room->poltergeist[0] && !IS_ASTRAL(ch) && !MOB_FLAGGED(ch, MOB_DUAL_NATURE))
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
  if (!(IS_PC_CONJURED_ELEMENTAL(ch) || IS_SPIRIT(ch)))
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
  return modify_target_rbuf_raw(ch, rbuf, rbuf_len, 0, FALSE);
}

int modify_target_rbuf_magical(struct char_data *ch, char *rbuf, int rbuf_len)
{
  return modify_target_rbuf_raw(ch, rbuf, rbuf_len, 0, TRUE);
}

int modify_target(struct char_data *ch)
{
  char fake_rbuf[5000];
  memset(fake_rbuf, 0, sizeof(fake_rbuf));
  return modify_target_rbuf_raw(ch, fake_rbuf, sizeof(fake_rbuf), 0, FALSE);
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
    case SKILL_GUNNERY:
    case SKILL_DEMOLITIONS:
      return (SKILL_FIREARMS);
    case SKILL_EDGED_WEAPONS:
    case SKILL_POLE_ARMS:
    case SKILL_WHIPS_FLAILS:
    case SKILL_CLUBS:
    case SKILL_CYBER_IMPLANTS:
    case SKILL_PROJECTILES:
    case SKILL_THROWING_WEAPONS:
      return (SKILL_ARMED_COMBAT);
    default:
      return (skill_num);
  }
}

// capitalize a string, now allows for color strings at the beginning
char *capitalize(const char *source)
{
  static char dest[MAX_STRING_LENGTH];
  if (source)
    strlcpy(dest, source, sizeof(dest));
  else {
    strlcpy(dest, "(Error)", sizeof(dest));
    mudlog("SYSERR: Received NULL string to capitalize().", NULL, LOG_SYSLOG, TRUE);
    return dest;
  }

  int len = strlen(dest);
  int index = 0;

  while (index < len-2 && *(dest + index) == '^')
    index += 2;
  *(dest + index) = UPPER(*(dest + index));

  return dest;
}

char *string_to_uppercase(const char *source) {
  static char dest[MAX_STRING_LENGTH];

  int i, x = strlen(source);
  for (i = 0; i < x; i++){
    if (isalpha(source[i])){
      dest[i] = toupper(source[i]);
    } else {
      dest[i] = source[i];
    }
  }
  dest[i] = '\0';

  return dest;
}

char *string_to_lowercase(const char *source) {
  static char dest[MAX_STRING_LENGTH];

  int i = 0, x = strlen(source);
  for (i = 0; i < x; i++){
    if (isalpha(source[i])){
      dest[i] = tolower(source[i]);
    } else {
      dest[i] = source[i];
    }
  }
  dest[i] = '\0';

  return dest;
}

// decapitalize a string that starts with A or An, now allows for color strings at the beginning
// Also takes care of 'the'.
char *decapitalize_a_an(const char *source)
{
  static char dest[MAX_STRING_LENGTH];
  strlcpy(dest, source, sizeof(dest));

  int len = strlen(source);
  int index = 0;

  while (*(source + index) == '^')
    index += 2;
  if (*(source + index) == 'A') {
    // If it starts with 'A ' or 'An ' then decapitalize the A.
    if (index < len-1 && (*(source + index+1) == ' ' || (*(source + index+1) == 'n' && index < len-2 && *(source + index+2) == ' '))) {
      *(dest + index) = 'a';
    }
  } else if (*(source + index) == 'T') {
    if (index < len-3 && *(source + index + 1) == 'h' && *(source + index + 2) == 'e' && *(source + index + 3) == ' ')
      *(dest + index) = 't';
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

  strncpy(New, source, strlen(source) + 1);
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
    if ((GET_OBJ_VAL(obj, 0) == CYB_HANDSPUR || GET_OBJ_VAL(obj, 0) == CYB_HANDRAZOR || GET_OBJ_VAL(obj, 0) == CYB_HANDBLADE || GET_OBJ_VAL(obj, 0) == CYB_FIN || GET_OBJ_VAL(obj, 0) == CYB_CLIMBINGCLAWS || GET_OBJ_VAL(obj, 0) == CYB_FANGS || GET_OBJ_VAL(obj, 0) == CYB_HORNS)
        && !GET_OBJ_VAL(obj, 9))
      return TRUE;
  return 0;
}


/* log a death trap hit */
void log_death_trap(struct char_data * ch)
{
  char buf[150];

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
  char buf[MAX_STRING_LENGTH * 2], buf2[MAX_STRING_LENGTH * 2];
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
        case LOG_RADLOG:
          check_log = PRF_RADLOG;
          break;
        case LOG_IGNORELOG:
          check_log = PRF_IGNORELOG;
          break;
        case LOG_MAILLOG:
          check_log = PRF_MAILLOG;
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

void mudlog_vfprintf(struct char_data *ch, int log, const char *format, ...)
{
  va_list args;

  char composed_string[10000];

  va_start(args, format);
  vsnprintf(composed_string, sizeof(composed_string), format, args);
  va_end(args);

  mudlog(composed_string, ch, log, TRUE);
}

// This is a function to reverse the order of an obj_data linked list.
// I ended up not using it but I'm leaving it in because it may be handy at some point.
void reverse_obj_list(struct obj_data **obj)
{
  struct obj_data *temp = NULL;
  struct obj_data *prev = NULL;
  struct obj_data *current = (*obj);

  while (current != NULL) {
    temp = current->next_content;
    current->next_content = prev;
    prev = current;
    current = temp;
  }
  (*obj) = prev;
}
void sprintbit(long vektor, const char *names[], char *result, size_t result_size)
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
          strlcat(result, ", ", result_size);
        }
        strlcat(result, names[nr], result_size);
      } else {
        strlcat(result, "UNDEFINED ", result_size);
      }
      have_printed = TRUE;
    }
    if (*names[nr] != '\n')
      nr++;
  }

  if (!*result)
    strlcat(result, "None ", result_size);
}

void sprinttype(int type, const char *names[], char *result, size_t result_size)
{
  snprintf(result, result_size, "%s", names[type]);

  if (str_cmp(result, "(null)") == 0) {
    strcpy(result, "UNDEFINED");
  }
}

void sprint_obj_mods(struct obj_data *obj, char *result, size_t result_size)
{
  *result = 0;
  if (obj->obj_flags.bitvector.GetNumSet() > 0)
  {
    char xbuf[MAX_STRING_LENGTH];
    obj->obj_flags.bitvector.PrintBits(xbuf, MAX_STRING_LENGTH, affected_bits, AFF_MAX);
    strlcpy(result, xbuf, result_size);
  }

  for (int i = 0; i < MAX_OBJ_AFFECT; i++)
    if (obj->affected[i].modifier != 0)
    {
      char xbuf[MAX_STRING_LENGTH];
      sprinttype(obj->affected[i].location, apply_types, xbuf, sizeof(xbuf));
      snprintf(ENDOF(result), result_size - strlen(result), " (%+d %s)",
               obj->affected[i].modifier, xbuf);
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
  if (get_ch_in_room(ch->master) == get_ch_in_room(ch) && ch->master->in_veh == ch->in_veh) {
    act("$n stops following $N.", TRUE, ch, 0, ch->master, TO_NOTVICT);
    act("$n stops following you.", TRUE, ch, 0, ch->master, TO_VICT);
  }
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
  if (!IS_SPIRIT(ch) && !IS_PC_CONJURED_ELEMENTAL(ch))
  {
    act("You now follow $N.", FALSE, ch, 0, leader, TO_CHAR);
    if (get_ch_in_room(leader) == get_ch_in_room(ch) && leader->in_veh == ch->in_veh && CAN_SEE(leader, ch)) {
      act("$n starts following you.", TRUE, ch, 0, leader, TO_VICT);
      act("$n starts to follow $N.", TRUE, ch, 0, leader, TO_NOTVICT);
    }
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

char * buf_mod(char *rbuf, size_t rbuf_len, const char *name, int bonus)
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

char * buf_roll(char *rbuf, size_t rbuf_len, const char *name, int bonus)
{
  if ( !rbuf )
    return rbuf;
  rbuf += strlen(rbuf);
  snprintf(rbuf, rbuf_len, " [%s %d]", name, bonus);
  return rbuf;
}

int get_speed(struct veh_data *veh)
{
  int speed = 0, maxspeed = (int)(veh->speed * ((veh->load - veh->usedload) / (veh->load != 0 ? veh->load : 1)));
  struct room_data *in_room = get_veh_in_room(veh);
  switch (veh->cspeed)
  {
    case SPEED_OFF:
      speed = 0;
      break;
    case SPEED_IDLE:
      speed = MIN(maxspeed, 1);
      break;
    case SPEED_CRUISING:
      if (ROOM_FLAGGED(in_room, ROOM_INDOORS))
        speed = MIN(maxspeed, 3);
      else
        speed = MIN(maxspeed * .5, 55);
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

int negotiate(struct char_data *ch, struct char_data *tch, int comp, int basevalue, int mod, bool buy, bool include_metavariant_penalty)
{
  struct obj_data *bio;
  int cmod = -GET_POWER(ch, ADEPT_KINESICS);
  int tmod = -GET_POWER(tch, ADEPT_KINESICS);
  snprintf(buf3, sizeof(buf3), "ALRIGHT BOIS HERE WE GO. Base global mod is %d. After application of Kinesics bonuses (if any), base modifiers for PC and NPC are %d and %d.", mod, cmod, tmod);

  if (include_metavariant_penalty) {
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
  } else {
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), " Skipping any potential metavariant penalties.");
  }
  // House rule: Data fence negotiations use raw int in order to make them a little easier to balance.
  bool negotiation_is_with_data_fence = (IS_NPC(ch) && MOB_HAS_SPEC(ch, fence)) || (IS_NPC(tch) && MOB_HAS_SPEC(ch, fence));

  int ch_int = (negotiation_is_with_data_fence ? GET_REAL_INT(ch) : GET_INT(ch));
  int tch_int = (negotiation_is_with_data_fence ? GET_REAL_INT(tch) : GET_INT(tch));

  // Plenty of NPCs have no int, so we set these to an average value if found.
  if (IS_NPC(tch) && tch_int <= 0)
    tch_int = MAX(tch_int, 3);
  if (IS_NPC(ch) && ch_int <= 0)
    ch_int = MAX(ch_int, 3);

  int chtn = mod + cmod + tch_int;
  int tchtn = mod + tmod + ch_int;

  act("Getting skill for PC...", FALSE, ch, NULL, NULL, TO_ROLLS);
  int cskill = get_skill(ch, SKILL_NEGOTIATION, chtn);
  act("Getting skill for NPC...", FALSE, tch, NULL, NULL, TO_ROLLS);

  // Plenty of NPCs also have no negotiation skill set when they should, so we make this an average value as well.
  int tskill;
  if (IS_NPC(tch) && GET_SKILL(tch, SKILL_NEGOTIATION) == 0) {
    int tch_cha = (negotiation_is_with_data_fence ? GET_REAL_CHA(tch) : GET_CHA(tch));
    tskill = MAX(tch_cha, 3);
  } else {
    tskill = get_skill(tch, SKILL_NEGOTIATION, tchtn);
  }


  for (bio = ch->bioware; bio; bio = bio->next_content)
    if (GET_BIOWARE_TYPE(bio) == BIO_TAILOREDPHEROMONES)
    {
      int delta = GET_BIOWARE_IS_CULTURED(bio) ? GET_BIOWARE_RATING(bio) * 2 : GET_BIOWARE_RATING(bio);
      cskill += delta;
      snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), " Pheromone skill buff of %d for PC.", delta);
      break;
    }

  for (bio = tch->bioware; bio; bio = bio->next_content)
    if (GET_BIOWARE_TYPE(bio) == BIO_TAILOREDPHEROMONES)
    {
      int delta = GET_BIOWARE_IS_CULTURED(bio) ? GET_BIOWARE_RATING(bio) * 2: GET_BIOWARE_RATING(bio);
      tskill += delta;
      snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), " Pheromone skill buff of %d for NPC.", delta);
      break;
    }

  int tchnego = success_test(tskill, tchtn);
  int chnego = success_test(cskill, chtn);

  snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "\r\nPC negotiation test gave %d successes on %d dice with TN %d (calculated from opponent int (%d) + global mod (%d) + our mod (%d)).",
           chnego, cskill, chtn, tch_int, mod, cmod);
  snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "\r\nNPC negotiation test gave %d successes on %d dice with TN %d (calculated from opponent int (%d) + global mod (%d) + our mod (%d)).",
           tchnego, tskill, tchtn, ch_int, mod, tmod);
  if (comp)
  {
    chtn = tch_int+mod+cmod;
    tchtn = ch_int+mod+tmod;

    act("Getting additional skill for PC...", FALSE, ch, NULL, NULL, TO_ROLLS);
    cskill = get_skill(ch, comp, chtn);
    act("Getting additional skill for NPC...", FALSE, tch, NULL, NULL, TO_ROLLS);
    tskill = get_skill(tch, comp, tchtn);

    int ch_delta = success_test(cskill, chtn) / 2;
    int tch_delta = success_test(tskill, tchtn) / 2;

    chnego += ch_delta;
    tchnego += tch_delta;
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "\r\nAdded additional rolls for %s (%d dice at TN %d vs %d dice at TN %d), so we got an additional %d PC and %d NPC successes.",
             skills[comp].name,
             cskill,
             chtn,
             tskill,
             tchtn,
             ch_delta,
             tch_delta);
  }
  int num = chnego - tchnego;
  if (num > 0)
  {
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "\r\nPC got %d net successes, so basevalue goes from %d", num, basevalue);
    if (buy)
      basevalue = MAX((int)(basevalue * 3/4), basevalue - (num * (basevalue / 20)));
    else
      basevalue = MIN((int)(basevalue * 5/4), basevalue + (num * (basevalue / 15)));
  } else
  {
    num *= -1;
    snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "\r\nNPC got %d net successes, so basevalue goes from %d", num, basevalue);
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
  int increase = 0;
  int defaulting_tn = 0;

  snprintf(gskbuf, sizeof(gskbuf), "get_skill(%s, %s): ", GET_CHAR_NAME(ch), skills[skill].name);

  // Convert NPCs so that they use the correct base skill for fighting (Armed Combat etc)
  if (IS_NPC(ch))
    skill = GET_SKILL(ch, skill) >= GET_SKILL(ch, return_general(skill)) ? skill : return_general(skill);

  if (GET_SKILL(ch, skill) <= 0) {
    // If the base TN is 8 or more and you'd default, you fail instead.
    if (target >= 8) {
      strlcat(gskbuf, "failed to default (TN 8+), returning 0 dice. ", sizeof(gskbuf));
      act(gskbuf, 1, ch, NULL, NULL, TO_ROLLS);
      return 0;
    }

    // Some skills cannot be defaulted on.
    if (skill == SKILL_AURA_READING || skill == SKILL_SORCERY || skill == SKILL_CONJURING) {
      strlcat(gskbuf, "failed to default (skill prohibits default), returning 0 dice. ", sizeof(gskbuf));
      act(gskbuf, 1, ch, NULL, NULL, TO_ROLLS);
      return 0;
    }

    // If we've gotten here, we're able to default. First, check for in-group defaults.
    if (skills[skill].group != 99) {
      int max_defaultable_skill = skill;
      for (int skill_idx = MIN_SKILLS; skill_idx < MAX_SKILLS; skill_idx++) {
        if (skills[skill_idx].group == skills[skill].group && GET_SKILL(ch, skill_idx) > GET_SKILL(ch, max_defaultable_skill)) {
          max_defaultable_skill = skill_idx;
        }
      }

      if (max_defaultable_skill != skill) {
        skill = max_defaultable_skill;
        defaulting_tn = 2;
        snprintf(ENDOF(gskbuf), sizeof(gskbuf) - strlen(gskbuf), "defaulting to %s (+2 TN). ", skills[skill].name);
      }
    }

    // If we haven't successfully defaulted to a skill, default to an attribute. This is represented by TN 4.
    if (defaulting_tn == 0) {
      defaulting_tn = 4;
      snprintf(ENDOF(gskbuf), sizeof(gskbuf) - strlen(gskbuf), "defaulting to %s (+4 TN). ", short_attributes[skills[skill].attribute]);
    }
  }

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

  // Core p38
  if (target < 2) {
    target = 2;
  }

  // Add the defaulting TN.
  target += defaulting_tn;

  // If you ever implement the Adept power Improved Ability, it would go here. See Core p169 for details.

  bool should_gain_physical_boosts = !PLR_FLAGGED(ch, PLR_MATRIX) && !PLR_FLAGGED(ch, PLR_REMOTE);
  int mbw = 0, enhan = 0, synth = 0;

  // Calculate our starting skill dice.
  int skill_dice = defaulting_tn == 4 ? GET_ATT(ch, skills[skill].attribute) : GET_SKILL(ch, skill);

  // If their skill in this area has not been boosted, they get to add their task pool up to the skill's learned level.
  int pool_dice = MIN(REAL_SKILL(ch, skill), GET_TASK_POOL(ch, skills[skill].attribute));
  if (REAL_SKILL(ch, skill) == GET_SKILL(ch, skill)) {
    // This is capped by defaulting, if any.
    if (defaulting_tn == 4) {
      // You get no dice.
      pool_dice = 0;
    }
    else if (defaulting_tn == 2) {
      // You get pool dice up to half your skill, round down.
      pool_dice = (int) (pool_dice / 2);
    }
    else {
      // You get all the pool dice.
    }

    if (pool_dice > 0) {
      skill_dice += pool_dice;
      snprintf(ENDOF(gskbuf), sizeof(gskbuf) - strlen(gskbuf), "+%d dice (task pool). ", pool_dice);
    } else {
      strlcat(gskbuf, "No task pool avail. ", sizeof(gskbuf));
    }
  } else {
    if (pool_dice > 0) {
      strlcat(gskbuf, "Task pool blocked (skill modified). ", sizeof(gskbuf));
    }
  }

  // Iterate through their cyberware, looking for anything important.
  if (ch->cyberware) {
    int expert = 0;
    int chip = 0;
    for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content) {
      switch (GET_CYBERWARE_TYPE(obj)) {
        case CYB_MOVEBYWIRE:
          mbw = should_gain_physical_boosts ? GET_CYBERWARE_RATING(obj) : 0;
          break;
        case CYB_CHIPJACKEXPERT:
          expert = GET_CYBERWARE_RATING(obj);
          break;
        case CYB_CHIPJACK:
          // Since the chipjack expert driver influences the _chipjack_, we don't account for memory skills here.
          for (struct obj_data *chip_obj = obj->contains; chip_obj; chip_obj = chip_obj->next_content) {
            if (GET_CHIP_SKILL(chip_obj) == skill)
              chip = GET_CHIP_RATING(chip_obj);
          }
          break;
      }
    }

    // If they have both a chipjack with the correct chip loaded and a Chipjack Expert, add the rating to their skill as task pool dice (up to skill max).
    if (chip && expert) {
      if (chip != GET_SKILL(ch, skill)) {
        strlcat(gskbuf, "Ignored expert driver (ch skill not equal to chip rating). ", sizeof(gskbuf));
      } else if (defaulting_tn == 4) {
        strlcat(gskbuf, "Ignored expert driver (S2A default). ", sizeof(gskbuf));
      } else if (defaulting_tn == 2) {
        increase = (int) (MIN(GET_SKILL(ch, skill), expert) / 2);
        skill_dice += increase;
        snprintf(ENDOF(gskbuf), sizeof(gskbuf) - strlen(gskbuf), "Chip & Expert Skill Increase (S2S default): %d. ", increase);
      } else {
        increase = MIN(GET_SKILL(ch, skill), expert);
        skill_dice += increase;
        snprintf(ENDOF(gskbuf), sizeof(gskbuf) - strlen(gskbuf), "Chip & Expert Skill Increase: %d. ", increase);
      }
    } else if (expert) {
      strlcat(gskbuf, "Ignored expert driver (no chip). ", sizeof(gskbuf));
    }
  }

  // Iterate through their bioware, looking for anything important.
  if (should_gain_physical_boosts && ch->bioware) {
    for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content) {
      if (GET_BIOWARE_TYPE(bio) == BIO_REFLEXRECORDER && GET_OBJ_VAL(bio, 3) == skill) {
        strlcat(gskbuf, "Reflex Recorder skill increase: 1. ", sizeof(gskbuf));
        skill_dice++;
      } else if (GET_BIOWARE_TYPE(bio) == BIO_ENHANCEDARTIC)
        enhan = TRUE;
      else if (GET_BIOWARE_TYPE(bio) == BIO_SYNTHACARDIUM)
        synth = GET_BIOWARE_RATING(bio);
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
      case SKILL_BR_FIXEDWING:
      case SKILL_BR_ROTORCRAFT:
      case SKILL_BR_VECTORTHRUST:
      case SKILL_BR_HOVERCRAFT:
      case SKILL_BR_MOTORBOAT:
      case SKILL_BR_SHIP:
      case SKILL_BR_LTA:
      case SKILL_PILOT_HOVERCRAFT:
      case SKILL_PILOT_MOTORBOAT:
      case SKILL_PILOT_SHIP:
      case SKILL_PILOT_LTA:
      case SKILL_PILOT_ROTORCRAFT:
      case SKILL_PILOT_FIXEDWING:
      case SKILL_PILOT_VECTORTHRUST:
      case SKILL_PILOT_BIKE:
      case SKILL_PILOT_CAR:
      case SKILL_PILOT_TRUCK:
        // Enhanced articulation only matters if you're actually using your body for the thing.
        if (!AFF_FLAGGED(ch, AFF_RIG) && !PLR_FLAGGED(ch, PLR_REMOTE)) {
          strlcat(gskbuf, "Enhanced Articulation skill increase: +1 ", sizeof(gskbuf));
          skill_dice++;
        }
        break;
      default:
        break;
    }
  }

  // Move-by-wire.
  if ((skill == SKILL_STEALTH || skill == SKILL_ATHLETICS) && mbw) {
    snprintf(ENDOF(gskbuf), sizeof(gskbuf) - strlen(gskbuf), "Move-By-Wire Skill Increase: %d. ", mbw);
    skill_dice += mbw;
  }

  // Synthacardium.
  if (skill == SKILL_ATHLETICS && synth) {
    snprintf(ENDOF(gskbuf), sizeof(gskbuf) - strlen(gskbuf), "Synthacardium Skill Increase: %d. ", synth);
    skill_dice += synth;
  }

  snprintf(ENDOF(gskbuf), sizeof(gskbuf) - strlen(gskbuf), "Final total skill: %d dice.", skill_dice);

  act(gskbuf, 1, ch, NULL, NULL, TO_ROLLS);

  return skill_dice;
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
    if (GET_CYBERWARE_TYPE(cyber1) != CYB_EYES)
      switch (GET_CYBERWARE_TYPE(cyber1)) {
        case CYB_FILTRATION:
          if (GET_CYBERWARE_FLAGS(cyber1) == GET_CYBERWARE_FLAGS(cyber2)) {
            send_to_char("You already have this type of filtration installed.\r\n", ch);
            return FALSE;
          }
          break;
        case CYB_DATAJACK:
          if (GET_CYBERWARE_TYPE(cyber2) == CYB_EYES && IS_SET(GET_CYBERWARE_FLAGS(cyber2), EYE_DATAJACK)) {
            send_to_char("You already have an eye datajack installed.\r\n", ch);
            return FALSE;
          }
          break;
        case CYB_VCR:
          if (GET_CYBERWARE_TYPE(cyber2) == CYB_BOOSTEDREFLEXES) {
            send_to_char("Vehicle Control Rigs are not compatible with Boosted reflexes.\r\n", ch);
            return FALSE;
          }
          break;
        case CYB_BOOSTEDREFLEXES:
          if (GET_CYBERWARE_TYPE(cyber2) == CYB_VCR) {
            send_to_char("Boosted reflexes is not compatible with Vehicle Control Rigs.\r\n", ch);
            return FALSE;
          }
          // fall through
        case CYB_MOVEBYWIRE:
        case CYB_WIREDREFLEXES:
          if (GET_CYBERWARE_TYPE(cyber2) == CYB_WIREDREFLEXES || GET_CYBERWARE_TYPE(cyber2) == CYB_MOVEBYWIRE ||
              GET_CYBERWARE_TYPE(cyber2) == CYB_BOOSTEDREFLEXES) {
            send_to_char("You already have reaction enhancing cyberware installed.\r\n", ch);
            return FALSE;
          }
          if (GET_CYBERWARE_TYPE(cyber1) == CYB_MOVEBYWIRE && GET_CYBERWARE_TYPE(cyber2) == CYB_REACTIONENHANCE) {
            send_to_char("Move-by-wire is not compatible with reaction enhancers.\r\n", ch);
            return FALSE;
          }
          break;
        case CYB_ORALSLASHER:
        case CYB_ORALDART:
        case CYB_ORALGUN:
          if (GET_CYBERWARE_TYPE(cyber2) == CYB_ORALSLASHER || GET_CYBERWARE_TYPE(cyber2) == CYB_ORALDART || GET_CYBERWARE_TYPE(cyber2)
              == CYB_ORALGUN) {
            send_to_char("You already have a weapon in your mouth.\r\n", ch);
            return FALSE;
          }
          break;
        case CYB_DERMALPLATING:
        case CYB_DERMALSHEATHING:
          switch (GET_CYBERWARE_TYPE(cyber2)) {
            case CYB_DERMALPLATING:
            case CYB_DERMALSHEATHING:
              send_to_char("You already have an skin modification.\r\n", ch);
              return FALSE;
            case CYB_ARMS:
            case CYB_LEGS:
            case CYB_SKULL:
            case CYB_TORSO:
              send_to_char("Skin modifications are incompatible with cybernetic replacements (limbs, skull, torso).\r\n", ch);
              return FALSE;
          }
          break;
        case CYB_REACTIONENHANCE:
          if (GET_CYBERWARE_TYPE(cyber2) == CYB_MOVEBYWIRE) {
            send_to_char("Reaction enhancers are not compatible with Move-by-wire.\r\n", ch);
            return FALSE;
          }
          break;
        case CYB_ARMS:
        case CYB_LEGS:
        case CYB_SKULL:
        case CYB_TORSO:
          switch (GET_CYBERWARE_TYPE(cyber2)) {
            case CYB_DERMALPLATING:
            case CYB_DERMALSHEATHING:
              send_to_char("Cybernetic replacements (limbs, skull, torso) are incompatible with skin modifications.\r\n", ch);
              return FALSE;
            case CYB_BONELACING:
              send_to_char("Cybernetic replacements (limbs, skull, torso) are incompatible with bone lacings.\r\n", ch);
              return FALSE;
            case CYB_MUSCLEREP:
              send_to_char("Cybernetic replacements (limbs, skull, torso) are incompatible with muscle replacements.\r\n", ch);
              return FALSE;
          }

          if (GET_CYBERWARE_TYPE(cyber1) == GET_CYBERWARE_TYPE(cyber2)) {
            send_to_char("You already have a cybernetic replacement of that type installed.\r\n", ch);
            return FALSE;
          }

          break;
        case CYB_MUSCLEREP:
          switch (GET_CYBERWARE_TYPE(cyber2)) {
            case CYB_ARMS:
            case CYB_LEGS:
            case CYB_SKULL:
            case CYB_TORSO:
              send_to_char("Muscle replacements are incompatible with cybernetic replacements (limbs, skull, torso).\r\n", ch);
              return FALSE;
          }
          break;
        case CYB_BONELACING:
          switch (GET_CYBERWARE_TYPE(cyber2)) {
            case CYB_ARMS:
            case CYB_LEGS:
            case CYB_SKULL:
            case CYB_TORSO:
              send_to_char("Bone lacings are incompatible with cybernetic replacements (limbs, skull, torso).\r\n", ch);
              return FALSE;
          }
          break;
      }
    if (IS_SET(GET_CYBERWARE_FLAGS(cyber1), EYE_DATAJACK) && GET_CYBERWARE_TYPE(cyber2) == CYB_DATAJACK) {
      send_to_char("You already have a datajack installed.\r\n", ch);
      return FALSE;
    }
    if (GET_CYBERWARE_TYPE(cyber2) == CYB_EYES && GET_CYBERWARE_TYPE(cyber1) == CYB_EYES)
      for (int bit = EYE_CAMERA; bit <= EYE_ULTRASOUND; bit *= 2) {
        if (IS_SET(GET_CYBERWARE_FLAGS(cyber2), bit) && IS_SET(GET_CYBERWARE_FLAGS(cyber1), bit)) {
          send_to_char("You already have eye modifications with this option installed.\r\n", ch);
          return FALSE;
        }
        if (bit >= EYE_OPTMAG1 && bit <= EYE_OPTMAG3 && IS_SET(GET_CYBERWARE_FLAGS(cyber1), bit))
          if (IS_SET(GET_CYBERWARE_FLAGS(cyber2), EYE_ELECMAG1) || IS_SET(GET_CYBERWARE_FLAGS(cyber2), EYE_ELECMAG2) || IS_SET(GET_CYBERWARE_FLAGS(cyber2), EYE_ELECMAG3)) {
            send_to_char("Optical magnification is not compatible with electronic magnification.\r\n", ch);
            return FALSE;
          }
        if (bit >= EYE_ELECMAG1 && bit <= EYE_ELECMAG3 && IS_SET(GET_CYBERWARE_FLAGS(cyber1), bit))
          if (IS_SET(GET_CYBERWARE_FLAGS(cyber2), EYE_OPTMAG1) || IS_SET(GET_CYBERWARE_FLAGS(cyber2), EYE_OPTMAG2) || IS_SET(GET_CYBERWARE_FLAGS(cyber2), EYE_OPTMAG3)) {
            send_to_char("Optical magnification is not compatible with electronic magnification.\r\n", ch);
            return FALSE;
          }
      }
  } else if (bio1 && cyber1) {
    switch (GET_CYBERWARE_TYPE(cyber1)) {
      case CYB_FILTRATION:
        if (GET_BIOWARE_TYPE(bio1) == BIO_TRACHEALFILTER && GET_CYBERWARE_FLAGS(cyber1) == FILTER_AIR) {
          send_to_char("Air filtration cyberware is not compatible with a Tracheal Filter.\r\n", ch);
          return FALSE;
        }
        if (GET_BIOWARE_TYPE(bio1) == BIO_DIGESTIVEEXPANSION && GET_CYBERWARE_FLAGS(cyber1) == FILTER_INGESTED) {
          send_to_char("Tracheal Filtration cyberware is not compatible with Digestive Expansion.\r\n", ch);
          return FALSE;
        }
        break;
      case CYB_MOVEBYWIRE:
        if (GET_BIOWARE_TYPE(bio1) == BIO_ADRENALPUMP) {
          send_to_char("Adrenal Pumps are not compatible with Move-By-Wire.\r\n", ch);
          return FALSE;
        }
        if (GET_BIOWARE_TYPE(bio1) == BIO_SYNAPTICACCELERATOR) {
          send_to_char("Synaptic Accelerators are not compatible with Move-By-Wire.\r\n", ch);
          return FALSE;
        }
        if (GET_BIOWARE_TYPE(bio1) == BIO_SUPRATHYROIDGLAND) {
          send_to_char("Suprathyroid Glands are not compatible with Move-By-Wire.\r\n", ch);
          return FALSE;
        }
        break;
      case CYB_WIREDREFLEXES:
        if (GET_BIOWARE_TYPE(bio1) == BIO_SYNAPTICACCELERATOR) {
          send_to_char("Your Synaptic Accelerator is not compatible with Wired Reflexes.\r\n", ch);
          return FALSE;
        }
        break;
      case CYB_EYES:
        if (GET_BIOWARE_TYPE(bio1) == BIO_CATSEYES || GET_BIOWARE_TYPE(bio1) == BIO_NICTATINGGLAND) {
          send_to_char("Bioware and cyberware eye modifications aren't compatible.\r\n", ch);
          return FALSE;
        }
        break;
      case CYB_MUSCLEREP:
        if (GET_BIOWARE_TYPE(bio1) == BIO_MUSCLEAUG || GET_BIOWARE_TYPE(bio1) == BIO_MUSCLETONER) {
          send_to_char("Muscle replacement isn't compatible with Muscle Augmentation or Toners.\r\n", ch);
          return FALSE;
        }
        break;
      case CYB_DERMALPLATING:
      case CYB_DERMALSHEATHING:
        if (GET_BIOWARE_TYPE(bio1) == BIO_ORTHOSKIN) {
          send_to_char("Orthoskin is not compatible with Dermal Plating or Sheathing.\r\n", ch);
          return FALSE;
        }
        break;
      case CYB_ARMS:
      case CYB_LEGS:
      case CYB_SKULL:
      case CYB_TORSO:
        if (GET_BIOWARE_TYPE(bio1) == BIO_MUSCLEAUG) {
          send_to_char("Cybernetic replacements (limbs, skull, torso) are incompatible with Muscle Augmentations.\r\n", ch);
          return FALSE;
        }
        if (GET_BIOWARE_TYPE(bio1) == BIO_MUSCLETONER) {
          send_to_char("Cybernetic replacements (limbs, skull, torso) are incompatible with Muscle Toners.\r\n", ch);
          return FALSE;
        }
        if (GET_BIOWARE_TYPE(bio1) == BIO_ORTHOSKIN) {
          send_to_char("Cybernetic replacements (limbs, skull, torso) are incompatible with Orthoskin.\r\n", ch);
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
      SET_POWER_TOTAL(vict, i, GET_POWER_TOTAL(vict, i) - 1);
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
      SET_POWER_TOTAL(vict, i, GET_POWER_TOTAL(vict, i) - 1);
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
  log_vfprintf("Subtracting %d magic from %s.", magic, GET_CHAR_NAME(ch));
  if (GET_TRADITION(ch) == TRAD_MUNDANE)
    return;

  GET_SETTABLE_REAL_MAG(ch) = MAX(0, GET_REAL_MAG(ch) - magic);

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

    // TODO: Deactivate adept powers.

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
// Note: some kits may still have workshop grade of 0 instead of TYPE_KIT.
#define IS_KIT(obj, type) ( GET_OBJ_TYPE((obj)) == ITEM_WORKSHOP && GET_WORKSHOP_TYPE((obj)) == type && (GET_WORKSHOP_GRADE((obj)) == TYPE_KIT || GET_WORKSHOP_GRADE((obj)) == 0) )
bool has_kit(struct char_data * ch, int type)
{
  for (struct obj_data *obj = ch->carrying; obj; obj = obj->next_content) {
    if (IS_KIT(obj, type)) {
      return TRUE;
    }
  }

  for (int i = 0; i < (NUM_WEARS - 1); i++) {
    if (GET_EQ(ch, i) && IS_KIT(GET_EQ(ch, i), type)) {
      return TRUE;
    }
  }

  return FALSE;
}

// Return true if the character has a key of the given number, false otherwise.
int has_key(struct char_data *ch, int key_vnum)
{
  struct obj_data *o, *key;

  // Check carried items.
  for (o = ch->carrying; o; o = o->next_content) {
    if (GET_OBJ_VNUM(o) == key_vnum)
      return 1;

    if (GET_OBJ_TYPE(o) == ITEM_KEYRING) {
      for (key = o->contains; key; key = key->next_content) {
        if (GET_OBJ_VNUM(key) == key_vnum)
          return 1;
      }
    }
  }

  // Check worn items.
  for (int x = 0; x < NUM_WEARS; x++) {
    // Must exist.
    if (!GET_EQ(ch, x))
      continue;

    // Direct match?
    if (GET_OBJ_VNUM(GET_EQ(ch, x)) == key_vnum)
      return 1;

    // Keyring match?
    if (GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_KEYRING) {
      for (key = GET_EQ(ch, x)->contains; key; key = key->next_content) {
        if (GET_OBJ_VNUM(key) == key_vnum)
          return 1;
      }
    }
  }

  return 0;
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
      // If we got here, it's either a kit, or a workshop that's not set up.
    }
  }

  // Return workshop (which is NULL if no workshop was found).
  return workshop;
}
#undef IS_KIT

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

  return TRUE;
}

void add_workshop_to_room(struct obj_data *obj) {
  // If it's not a workshop, skip.
  if (!_is_workshop_valid(obj))
    return;

  // If it's not in a room, skip.
  if (!obj->in_room)
    return;

  // If it's not a deployed workshop or a facility, skip.
  if (!(GET_WORKSHOP_GRADE(obj) == TYPE_WORKSHOP && GET_WORKSHOP_IS_SETUP(obj)) && GET_WORKSHOP_GRADE(obj) != TYPE_FACILITY)
    return;

  struct obj_data *current = obj->in_room->best_workshop[GET_WORKSHOP_TYPE(obj)];

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
    mudlog("SYSERR: Attempting to retrieve ammo for nonexistent mount.", NULL, LOG_SYSLOG, TRUE);
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
    // Find the mount in use, if any.
    struct obj_data *mount = get_mount_manned_by_ch(ch);

    // If we found one, clean it up and print a message.
    if (mount) {
      // Clean up the mount's data (stop it from pointing to character; remove targets)
      mount->worn_by = NULL;
      mount->targ = NULL;
      mount->tveh = NULL;

      // Let them / the room know that they've stopped manning.
      if (send_message) {
        act("$n stops manning $p.", FALSE, ch, mount, 0, TO_ROOM);
        act("You stop manning $p.", FALSE, ch, mount, 0, TO_CHAR);
      }
    }

    // Clean up their character flags.
    AFF_FLAGS(ch).RemoveBit(AFF_MANNING);

    // Take them out of fight mode if they're in it.
    if (CH_IN_COMBAT(ch))
      stop_fighting(ch);

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

void store_message_to_history(struct descriptor_data *d, int channel, const char *message) {
  // We use our very own message buffer to ensure we'll never overwrite whatever buffer the caller is using.
  static char log_message[MAX_INPUT];

  // Precondition: No screwy pointers. Removed warning since we can be passed NPC descriptors (which we ignore).
  if (d == NULL || !message || !*message) {
    // mudlog("SYSERR: Null descriptor or message passed to store_message_to_history.", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // Precondition: Channel must be a valid index (0 ≤ channel < number of channels defined in awake.h).
  if (channel < 0 || channel >= NUM_COMMUNICATION_CHANNELS) {
    snprintf(log_message, sizeof(log_message), "SYSERR: Channel %d is not within bounds 0 <= channel < %d.", channel, NUM_COMMUNICATION_CHANNELS);
    mudlog(log_message, NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // Add a clone of the message to the descriptor's channel history.
  d->message_history[channel].AddItem(NULL, str_dup(message));

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
    snprintf(errbuf, sizeof(errbuf), "SYSERR: get_veh_in_room called on veh %s, but it's not in a room or vehicle!", GET_VEH_NAME(veh));
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
    return &world[0];
  }

  if (ch->in_room)
    return ch->in_room;

  if (ch->in_veh) {
    return get_veh_in_room(ch->in_veh);
  }

  snprintf(errbuf, sizeof(errbuf), "SYSERR: get_ch_in_room called on char %s, but they're not in a room or vehicle!", GET_CHAR_NAME(ch));
  mudlog(errbuf, ch, LOG_SYSLOG, TRUE);

  return &world[0];
}

struct room_data *get_obj_in_room(struct obj_data *obj) {
  char errbuf[500];
  if (!obj) {
    snprintf(errbuf, sizeof(errbuf), "SYSERR: get_obj_in_room was passed a NULL object!");
    mudlog(errbuf, NULL, LOG_SYSLOG, TRUE);
    return &world[0];
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
    return get_ch_in_room(obj->worn_by);

  // All is lost. The object floats in an endless void.
  snprintf(errbuf, sizeof(errbuf), "SYSERR: get_obj_in_room called on obj %s, but it's not in a room or vehicle!", GET_OBJ_NAME(obj));
  mudlog(errbuf, NULL, LOG_SYSLOG, TRUE);

  return &world[0];
}

bool invis_ok(struct char_data *ch, struct char_data *vict) {
  struct room_data *ch_room = NULL, *vict_room = NULL;
  // No room at all? Nope.
  if (!ch || !(ch_room = get_ch_in_room(ch))) {
    mudlog("invis_ok() received char with NO room!", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (!vict || !(vict_room = get_ch_in_room(vict))) {
    mudlog("invis_ok() received vict with NO room!", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  // If they're in an invis staffer above your level, no.
  if (!IS_NPC(vict) && GET_INVIS_LEV(vict) > 0 && (IS_NPC(ch) || !access_level(ch, GET_INVIS_LEV(vict))))
    return FALSE;

  // Staff members see almost everything.
  if (IS_SENATOR(ch))
    return TRUE;

  // Totalinvis blocks mort sight.
  if (IS_NPC(vict) && MOB_FLAGGED(vict, MOB_TOTALINVIS))
    return FALSE;

  // Figure out if we're using vehicle sensors. If we are, we need to be judicious about the sight we give.
  bool has_thermographic, has_ultrasound, has_astral, is_vehicle;
  if (AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE)) {
    // We are. Set based on vehicle sensors.
    struct veh_data *veh;
    RIG_VEH(ch, veh);

    if (!veh) {
      mudlog("SYSERR: Something went seriously wrong: rig_veh in invis_ok failed!", ch, LOG_SYSLOG, TRUE);
      return FALSE;
    }

    is_vehicle = TRUE;
    has_astral = FALSE;
    has_ultrasound = veh->flags.IsSet(VFLAG_ULTRASOUND);
    has_thermographic = TRUE;

#ifdef ULTRASOUND_REQUIRES_SAME_ROOM
    has_ultrasound &= vict_room == veh->in_room;
#endif
  } else {
    is_vehicle = FALSE;
    has_astral = SEES_ASTRAL(ch);
    has_ultrasound = has_vision(ch, VISION_ULTRASONIC) && !affected_by_spell(ch, SPELL_STEALTH);
    has_thermographic = has_vision(ch, VISION_THERMOGRAPHIC);

#ifdef ULTRASOUND_REQUIRES_SAME_ROOM
    has_ultrasound &= vict_room == ch_room;
#endif
  }

  // Astral perception sees most things-- unless said thing is an inanimate mob with no spells on it.
  if (has_astral && !(MOB_FLAGGED(vict, MOB_INANIMATE) && !GET_SUSTAINED(vict)))
    return TRUE;

  // Ultrasound pierces all invis as long as it's in the same room and not blocked by silence or stealth.
  if (has_ultrasound && !affected_by_spell(vict, SPELL_STEALTH) && vict_room->silence[ROOM_NUM_SPELLS_OF_TYPE] <= 0)
    return TRUE;

  // Allow resist test VS improved invis-- but only if you're not seeing the world through sensors.
  if (IS_AFFECTED(vict, AFF_SPELLIMPINVIS)) {
    return !is_vehicle && can_see_through_invis(ch, vict);
  }

  // Thermoptic camouflage, a houseruled thing that doesn't actually show up in the game yet. This can only be broken by ultrasound.
  if (IS_AFFECTED(vict, AFF_IMP_INVIS)) {
    return FALSE;
  }

  // Ruthenium is pierced by thermographic vision, which is default on vehicles.
  if (IS_AFFECTED(vict, AFF_INVISIBLE)) {
    return has_thermographic;
  }

  // Allow resistance test VS invis spell.
  if (IS_AFFECTED(vict, AFF_SPELLINVIS)) {
    return (has_thermographic || can_see_through_invis(ch, vict));
  }

  // If we've gotten here, they're not invisible.
  return TRUE;
}

// Returns TRUE if the character is able to make noise, FALSE otherwise.
bool char_can_make_noise(struct char_data *ch, const char *message) {
  bool is_stealth = affected_by_spell(ch, SPELL_STEALTH);
  bool is_silence = get_ch_in_room(ch)->silence[ROOM_NUM_SPELLS_OF_TYPE] > 0;
  if (is_stealth || is_silence) {
    // Can't make noise.
    if (message) {
      send_to_char(message, ch);
      send_to_char(ch, "(OOC: You're %s.)\r\n", is_stealth ? "under a stealth spell" : "in a silent room");
    }

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
  if (container == NULL || GET_OBJ_TYPE(container) == ITEM_PART)
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
      send_to_char(ch, "Sorry, something went wrong. Staff have been notified.\r\n", ch);
    mudlog("SYSERR: NULL weapon or attachment passed to attach_attachment_to_weapon().", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  // The chosen location must be valid.
  if (location < ACCESS_ACCESSORY_LOCATION_TOP || location > ACCESS_ACCESSORY_LOCATION_UNDER) {
    if (ch)
      send_to_char("Sorry, something went wrong. Staff have been notified.\r\n", ch);
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Accessory attachment location %d out of range for '%s' (%ld).", location, GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment));
    return FALSE;
  }

  // You can only attach weapon accessories in this function.
  if (GET_OBJ_TYPE(attachment) != ITEM_GUN_ACCESSORY) {
    if (ch) {
      send_to_char(ch, "%s is not a gun accessory.\r\n", CAP(GET_OBJ_NAME(attachment)));
    } else {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Attempting to attach non-attachment '%s' (%ld) to '%s' (%ld).",
                      GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment), GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
    }
    return FALSE;
  }

  // You can only attach things to guns.
  if (GET_OBJ_TYPE(weapon) != ITEM_WEAPON || !IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))) {
    if (ch) {
      send_to_char(ch, "%s is not a gun.\r\n", CAP(GET_OBJ_NAME(weapon)));
    } else {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Attempting to attach '%s' (%ld) to non-gun '%s' (%ld).",
                      GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment), GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
    }
    return FALSE;
  }

  // You can't attach smartgoggles to weapons.
  if (GET_ACCESSORY_TYPE(attachment) == ACCESS_SMARTGOGGLE) {
    if (ch) {
      send_to_char(ch, "%s are for your eyes, not your gun.\r\n", CAP(GET_OBJ_NAME(attachment)));
    } else {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Attempting to attach smartgoggle '%s' (%ld) to '%s' (%ld).",
                      GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment), GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
    }
    return FALSE;
  }

  // If something is already attached there, bail out.
  if (GET_WEAPON_ATTACH_LOC(weapon, location + ACCESS_ACCESSORY_LOCATION_DELTA) > 0) {
    if (ch) {
      send_to_char(ch, "You cannot mount more than one attachment to the %s of %s.\r\n",
                   gun_accessory_locations[location],
                   GET_OBJ_NAME(weapon));
    } else {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Attempting to attach '%s' (%ld) to already-filled %s location on '%s' (%ld).",
                      GET_OBJ_NAME(attachment),
                      GET_OBJ_VNUM(attachment),
                      gun_accessory_locations[location],
                      GET_OBJ_NAME(weapon),
                      GET_OBJ_VNUM(weapon));
    }
    return FALSE;
  }

  // A negative number in an attachment slot blocks that attachment.
  if (GET_WEAPON_ATTACH_LOC(weapon, location + ACCESS_ACCESSORY_LOCATION_DELTA) < 0) {
    if (ch) {
      send_to_char(ch, "%s isn't compatible with %s-mounted attachments.\r\n",
                   CAP(GET_OBJ_NAME(weapon)),
                   gun_accessory_locations[location]);
    } else {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Attempting to attach '%s' (%ld) to unacceptable %s location on '%s' (%ld).",
                      GET_OBJ_NAME(attachment),
                      GET_OBJ_VNUM(attachment),
                      gun_accessory_locations[location],
                      GET_OBJ_NAME(weapon),
                      GET_OBJ_VNUM(weapon));
    }
    return FALSE;
  }

  // Handling for silencer and suppressor restrictions.
  if (GET_ACCESSORY_TYPE(attachment) == ACCESS_SILENCER || GET_ACCESSORY_TYPE(attachment) == ACCESS_SOUNDSUPP) {
    // Certain weapon types cannot take silencers / suppressors.
    switch (GET_WEAPON_ATTACK_TYPE(weapon)) {
      case WEAP_REVOLVER:
      case WEAP_SHOTGUN:
      case WEAP_GREN_LAUNCHER:
      case WEAP_CANNON:
      case WEAP_MINIGUN:
      case WEAP_MISS_LAUNCHER:
        if (ch) {
          send_to_char(ch, "%ss can't use silencers or suppressors.\r\n", CAP(weapon_types[GET_WEAPON_ATTACK_TYPE(weapon)]));
        } else {
          mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Attempting to attach silencer/suppressor '%s' (%ld) to %s '%s' (%ld).",
                          GET_OBJ_NAME(attachment),
                          GET_OBJ_VNUM(attachment),
                          weapon_types[GET_WEAPON_ATTACK_TYPE(weapon)],
                          GET_OBJ_NAME(weapon),
                          GET_OBJ_VNUM(weapon));
        }
        return FALSE;
    }

    // Restrictions for silencers:
    if (GET_ACCESSORY_TYPE(attachment) == ACCESS_SILENCER) {
      // Weapon cannot have BF or FA modes.
      if (WEAPON_CAN_USE_FIREMODE(weapon, MODE_BF) || WEAPON_CAN_USE_FIREMODE(weapon, MODE_FA)) {
        if (ch) {
          send_to_char(ch, "%s would tear your silencer apart-- it needs a sound suppressor.\r\n", CAP(GET_OBJ_NAME(weapon)));
        } else {
          mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Attempting to attach pistol silencer '%s' (%ld) to BF/FA weapon '%s' (%ld).",
                          GET_OBJ_NAME(attachment),
                          GET_OBJ_VNUM(attachment),
                          GET_OBJ_NAME(weapon),
                          GET_OBJ_VNUM(weapon));
        }
        return FALSE;
      }
    }

    // Restrictions for sound suppressors:
    if (GET_ACCESSORY_TYPE(attachment) == ACCESS_SOUNDSUPP) {
      // Weapon must have BF or FA mode available.
      if (!(WEAPON_CAN_USE_FIREMODE(weapon, MODE_BF) || WEAPON_CAN_USE_FIREMODE(weapon, MODE_FA))) {
        if (ch) {
          send_to_char(ch, "Sound suppressors are too heavy-duty for %s-- it needs a silencer.\r\n", CAP(GET_OBJ_NAME(weapon)));
        } else {
          mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Attempting to attach rifle suppressor '%s' (%ld) to non-BF/FA weapon '%s' (%ld).",
                          GET_OBJ_NAME(attachment),
                          GET_OBJ_VNUM(attachment),
                          GET_OBJ_NAME(weapon),
                          GET_OBJ_VNUM(weapon));
        }
        return FALSE;
      }
    }
  }

  // We've cleared preconditions at this point. Proceed with attaching.

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

    // Complain loudly if this operation failed.
    if (!successfully_modified) {
      if (ch)
        send_to_char(ch, "You seem unable to attach %s to %s.\r\n", GET_OBJ_NAME(attachment), GET_OBJ_NAME(weapon));

      mudlog_vfprintf(ch, LOG_SYSLOG, "WARNING: '%s' (%ld) attempted to attach '%s' (%ld) to '%s' (%ld),"
                      " but the gun was full up on affects. Something needs revising."
                      " Gun's current top/barrel/bottom attachment vnums are %d / %d / %d.",
                      ch ? GET_CHAR_NAME(ch) : "An automated process", ch ? GET_IDNUM(ch) : -1,
                      GET_OBJ_NAME(attachment), GET_OBJ_VNUM(attachment),
                      GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon),
                      GET_WEAPON_ATTACH_TOP_VNUM(weapon),
                      GET_WEAPON_ATTACH_BARREL_VNUM(weapon),
                      GET_WEAPON_ATTACH_UNDER_VNUM(weapon));
      return FALSE;
    }
  }

  // Several changes are needed if a player is directly attaching. These are skipped for auto-attach cases.
  if (ch) {
    // Add the attachment's weight to the weapon's weight.
    weight_change_object(weapon, GET_OBJ_WEIGHT(attachment));

    // Add the attachment's cost to the weapon's cost, but only if a player attached it.
    GET_OBJ_COST(weapon) = MAX(0, GET_OBJ_COST(weapon) + GET_OBJ_COST(attachment));
  }

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
  // The weapon must exist.
  if (!weapon) {
    if (ch)
      send_to_char("Sorry, something went wrong. Staff have been notified.\r\n", ch);
    mudlog("SYSERR: NULL weapon passed to unattach_attachment_from_weapon().", ch, LOG_SYSLOG, TRUE);
    return NULL;
  }

  // The location must be valid.
  if (location < ACCESS_LOCATION_TOP || location > ACCESS_LOCATION_UNDER) {
    if (ch)
      send_to_char("Sorry, something went wrong. Staff have been notified.\r\n", ch);
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Attempt to unattach item from invalid location %d on '%s' (%ld).",
                    location, GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
    return NULL;
  }

  if (GET_OBJ_TYPE(weapon) != ITEM_WEAPON || !IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))) {
    if (ch) {
      send_to_char("You can only unattach accessories from weapons.\r\n", ch);
    } else {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Attempting to unattach something from non-gun '%s' (%ld).",
                      GET_OBJ_NAME(weapon), GET_OBJ_VNUM(weapon));
    }
    return NULL;
  }

  // Get a pointer to the attachment so that we can reference it.
  struct obj_data *attachment = read_object(GET_WEAPON_ATTACH_LOC(weapon, location), VIRTUAL);

  // If the attachment was un-loadable, bail out.
  if (!attachment) {
    if (ch)
      send_to_char("You accidentally break it as you remove it!\r\n", ch);
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Attempting to unattach invalid vnum %d from %s of weapon '%s' (%ld).",
                    GET_WEAPON_ATTACH_LOC(weapon, location),
                    gun_accessory_locations[location - ACCESS_ACCESSORY_LOCATION_DELTA],
                    GET_OBJ_NAME(weapon),
                    GET_OBJ_VNUM(weapon));

    // We use a raw get_obj_val here so we can set it.
    GET_OBJ_VAL(weapon, location) = 0;
    return NULL;
  }

  if (GET_ACCESSORY_TYPE(attachment) == ACCESS_GASVENT) {
    // Non-chargen chars have to pay and have a workshop to unattach gas vents.
    if (ch && !PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
      int removal_cost = MAX(GET_OBJ_COST(weapon) / 10, MINIMUM_GAS_VENT_REMOVAL_COST);

      if (!find_workshop(ch, TYPE_GUNSMITHING)) {
        send_to_char(ch, "That's a complex task! You'll need a gunsmithing workshop and %d nuyen on hand to do that.\r\n", removal_cost);
        return NULL;
      }

      if (GET_NUYEN(ch) < removal_cost) {
        send_to_char(ch, "You'll need at least %d nuyen on hand to cover the cost of the new barrel.\r\n", removal_cost);
        return NULL;
      }

      lose_nuyen(ch, removal_cost, NUYEN_OUTFLOW_REPAIRS);
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
      mudlog_vfprintf(ch, LOG_SYSLOG, "WARNING: '%s' (%ld) unattached '%s' (%ld) from '%s' (%ld), but"
                      " the gun was missing the attachment's affect. Something needs revising.",
                      ch ? GET_CHAR_NAME(ch) : "An automated process",
                      ch ? GET_IDNUM(ch) : -1,
                      GET_OBJ_NAME(attachment),
                      GET_OBJ_VNUM(attachment),
                      GET_OBJ_NAME(weapon),
                      GET_OBJ_VNUM(weapon));
    }
  }

  // Subtract the attachment's weight from the weapon's weight.
  weight_change_object(weapon, -GET_OBJ_WEIGHT(attachment));

  // Subtract the attachment's cost from the weapon's cost.
  if (GET_OBJ_COST(attachment) > GET_OBJ_COST(weapon)) {
    GET_OBJ_COST(attachment) = GET_OBJ_COST(weapon);
    GET_OBJ_COST(weapon) = 0;
  } else {
    GET_OBJ_COST(weapon) -= GET_OBJ_COST(attachment);
  }

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
        rnum_t rnum = real_object(GET_WEAPON_ATTACH_LOC(weapon, attach_loc));
        if (rnum < 0)
          continue;

        // Grab the thing's prototype and check it for this flag.
        if ((&obj_proto[rnum])->obj_flags.bitvector.IsSet(acceptable_weapon_attachment_affects[waff_index])) {
          found_duplicate = TRUE;
          break;
        }
      }

      // Check the weapon's own proto flags (we don't want to remove built-in flags).
      if (!found_duplicate) {
        rnum_t weapon_rnum = real_object(GET_OBJ_VNUM(weapon));
        if ((&obj_proto[weapon_rnum])->obj_flags.bitvector.IsSet(acceptable_weapon_attachment_affects[waff_index])) {
          found_duplicate = TRUE;
        }
      }

      // Didn't find another item with this flag, so un-set it.
      if (!found_duplicate)
        weapon->obj_flags.bitvector.RemoveBit(acceptable_weapon_attachment_affects[waff_index]);
    }
  }

  // Update the weapon's attach location to reflect the removal of this item.
  GET_OBJ_VAL(weapon, location) = 0;

  // Send the success message, assuming there's a character.
  if (ch) {
    if (GET_ACCESSORY_TYPE(attachment) == ACCESS_GASVENT) {
      act("You remove the ported barrel from $p and discard it, installing a non-ported one in its place.", TRUE, ch, weapon, NULL, TO_CHAR);
      act("$n removes the ported barrel from $p and discards it, installing a non-ported one in its place.", TRUE, ch, weapon, NULL, TO_ROOM);
      extract_obj(attachment);
      attachment = NULL;
    } else {
      act("You unattach $p from $P.", TRUE, ch, attachment, weapon, TO_CHAR);
      act("$n unattaches $p from $P.", TRUE, ch, attachment, weapon, TO_ROOM);
    }
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

  // Descriptor data (null for non-possessed NPCs)
  REPLICATE(desc);

  // Spell info.
  REPLICATE(squeue);
  REPLICATE(sustained);
  for (struct sustain_data *sust = clone->sustained; sust; sust = sust->next) {
    if (sust->other == original)
      sust->other = clone;
  }

  REPLICATE(ssust);
  for (struct spirit_sustained *sust = clone->ssust; sust; sust = sust->next) {
    if (sust->target == original)
      sust->target = clone;
  }

  REPLICATE(spells);

  // Equipment info.
  for (int pos = 0; pos < NUM_WEARS; pos++) {
    REPLICATE(equipment[pos]);
    if (clone->equipment[pos])
      clone->equipment[pos]->worn_by = clone;
  }

  REPLICATE(carrying);
  for (struct obj_data *obj = clone->carrying; obj; obj = obj->next_content) {
    obj->carried_by = clone;
  }

  REPLICATE(cyberware);
  for (struct obj_data *obj = clone->cyberware; obj; obj = obj->next_content) {
    obj->carried_by = clone;
  }


  REPLICATE(bioware);
  for (struct obj_data *obj = clone->bioware; obj; obj = obj->next_content) {
    obj->carried_by = clone;
  }

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

  // Ignore data (null for NPCs)
  REPLICATE(ignore_data);

  // Pgroup data (null for NPCs)
  REPLICATE(pgroup);
  REPLICATE(pgroup_invitations);

  // Invis resistance test data
  REPLICATE(pc_invis_resistance_test_results);
  REPLICATE(mob_invis_resistance_test_results);

  // Nested data (pointers from included structs)
  REPLICATE(char_specials.fighting);
  REPLICATE(char_specials.fight_veh);
  REPLICATE(char_specials.hunting);
  REPLICATE(char_specials.programming);
  REPLICATE(char_specials.defined_position);
  REPLICATE(char_specials.position);
  REPLICATE(char_specials.subscribe);
  REPLICATE(char_specials.rigging);
  REPLICATE(char_specials.mindlink);
  REPLICATE(char_specials.spirits);
  REPLICATE(char_specials.highlight_color_code);
  REPLICATE(mob_specials.last_direction);
  REPLICATE(mob_specials.memory);
  REPLICATE(mob_specials.wait_state);
  REPLICATE(mob_specials.quest_id);
  REPLICATE(mob_specials.alert);
  REPLICATE(mob_specials.alerttime);
  REPLICATE(mob_specials.spare1);
  REPLICATE(mob_specials.spare2);
  REPLICATE(points.mental);
  REPLICATE(points.max_mental);
  REPLICATE(points.physical);
  REPLICATE(points.max_physical);
  REPLICATE(points.init_roll);
  REPLICATE(points.sustained[0]);
  REPLICATE(points.sustained[1]);
  REPLICATE(points.track[0]);
  REPLICATE(points.track[1]);
  REPLICATE(points.fire[0]);
  REPLICATE(points.fire[1]);
  REPLICATE(points.fire[2]);
  REPLICATE(points.reach[0]);
  REPLICATE(points.reach[1]);
  REPLICATE(points.extras[0]);
  REPLICATE(points.extras[1]);

  REPLICATE(vfront);

  copy_vision_from_original_to_clone(original, clone);

#undef REPLICATE

  // Also clone important flags.
  if (AFF_FLAGGED(original, AFF_PILOT))
    AFF_FLAGS(clone).SetBit(AFF_PILOT);
  if (AFF_FLAGGED(original, AFF_MANNING))
    AFF_FLAGS(clone).SetBit(AFF_MANNING);
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
    else if (*read_ptr == '#') {
      *(write_ptr++) = '^';
      *(write_ptr++) = '^';
    }
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
    snprintf(msgbuf, sizeof(msgbuf), "SYSERR: NULL character passed to set_character_skill(NULL, %d, %d, %s).",
             skill_num,
             new_value,
             send_message ? "TRUE" : "FALSE");
    mudlog(msgbuf, ch, LOG_SYSLOG, TRUE);
    return;
  }

  if (IS_NPC(ch)) {
    snprintf(msgbuf, sizeof(msgbuf), "SYSERR: NPC '%s' (%ld) passed to set_character_skill(ch, %d, %d, %s).",
             GET_CHAR_NAME(ch),
             GET_MOB_VNUM(ch),
             skill_num,
             new_value,
             send_message ? "TRUE" : "FALSE"
           );
    mudlog(msgbuf, ch, LOG_SYSLOG, TRUE);
    return;
  }

  if (skill_num < MIN_SKILLS || skill_num >= MAX_SKILLS) {
    snprintf(msgbuf, sizeof(msgbuf), "SYSERR: Invalid skill number %d passed to set_character_skill(%s, %d, %d, %s).",
             skill_num,
             GET_CHAR_NAME(ch),
             skill_num,
             new_value,
             send_message ? "TRUE" : "FALSE");
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
      } else if (new_value == 3) {
        send_to_char(ch, "^cYou have gotten in some practice.^n\r\n");
      } else if (new_value == 5) {
        send_to_char(ch, "^cYou have attained average proficiency.^n\r\n");
      } else if (new_value == 7) {
        send_to_char(ch, "^CYour skills are now above average.^n\r\n");
      } else if (new_value == 9) {
        send_to_char(ch, "^CYou are considered a professional at %s.^n\r\n", skills[skill_num].name);
      } else if (new_value == 10) {
        send_to_char(ch, "^gYou've practiced so much that you can act without thinking about it.^n\r\n");
      } else if (new_value == 11) {
        send_to_char(ch, "^gYou are considered an expert in your field.^n\r\n");
      } else if (new_value == 12) {
        send_to_char(ch, "^GYour talents at %s are considered world-class.^n\r\n", skills[skill_num].name);
      } else {
        send_to_char(ch, "^GYou further hone your talents towards perfection.^n\r\n");
      }
    }
    // Language messaging.
    else if (SKILL_IS_LANGUAGE(skill_num)) {
      if (new_value == 0) {
        send_to_char(ch, "You completely forget your skills in %s.\r\n", skills[skill_num].name);
      } else if (new_value == 1) {
        send_to_char(ch, "^cYou have been introduced to the basics.^n\r\n");
      } else if (new_value <= 3) {
        send_to_char(ch, "^cYou have gotten in some practice.^n\r\n");
      } else if (new_value <= 5) {
        send_to_char(ch, "^cYou have attained average proficiency.^n\r\n");
      } else if (new_value == 7) {
        send_to_char(ch, "^CYou are now considered fluent at a high-school level.^n\r\n");
      } else if (new_value == 9) {
        send_to_char(ch, "^CYou have achieved bachelor's-degree-level fluency.^n\r\n");
      } else if (new_value == 10) {
        send_to_char(ch, "^gYou have achieved a native-level fluency.^n\r\n");
      } else if (new_value == 12) {
        send_to_char(ch, "^GYou have achieved doctorate-degree-level fluency.^n\r\n");
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
      } else if (new_value == 3) {
        send_to_char(ch, "^cYou've developed an interest in %s.^n\r\n", skills[skill_num].name);
      } else if (new_value == 5) {
        send_to_char(ch, "^cYou have a dedicated knowledge of that area.^n\r\n");
      } else if (new_value == 7) {
        send_to_char(ch, "^CYou are well-rounded in the field of %s.^n\r\n", skills[skill_num].name);
      } else if (new_value == 9) {
        send_to_char(ch, "^CYou could earn a degree in %s.^n\r\n", skills[skill_num].name);
      } else if (new_value == 10) {
        send_to_char(ch, "^gYou have mastered the field of %s.^n\r\n", skills[skill_num].name);
      } else if (new_value == 11) {
        send_to_char(ch, "^gYou are considered an expert in your field.^n\r\n");
      } else if (new_value == 12) {
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

  RANK_MESSAGE(0,  "Not Learned" , "Not Learned");
  RANK_MESSAGE(1,  "Introduced"  , "Scream-Sheet Level");
  RANK_MESSAGE(2,  "Practiced"   , "Interested");
  RANK_MESSAGE(3,  "Novice"      , "Interested");
  RANK_MESSAGE(4,  "Competent"   , "Dedicated");
  RANK_MESSAGE(5,  "Proficient"  , "Well-Rounded");
  RANK_MESSAGE(6,  "Proficient"  , "Educated");
  RANK_MESSAGE(7,  "Skilled"     , "Educated");
  RANK_MESSAGE(8,  "Professional", "Mastered");
  RANK_MESSAGE(9,  "Professional", "Intuitive");
  RANK_MESSAGE(10, "Specialist"  , "Specialist");
  RANK_MESSAGE(11, "Expert"      , "Expert");

  if (rank < MAX_SKILL_LEVEL_FOR_IMMS) {
    if (knowledge) return "Genius";
    else return "World-Class";
  }

  return "Godly";
#undef RANK_MESSAGE
}

char *how_good(int skill, int rank)
{
  static char buf[256];
  snprintf(buf, sizeof(buf), " (rank ^c%2d^n: %s)", rank, skill_rank_name(rank, skills[skill].type == SKILL_TYPE_KNOWLEDGE));
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

struct obj_data *obj_is_or_contains_obj_with_vnum(struct obj_data *obj, vnum_t vnum) {
  if (!obj) {
    return NULL;
  }

  if (GET_OBJ_VNUM(obj) == vnum) {
    return obj;
  }

  if (obj->contains) {
    for (struct obj_data *child = obj->contains; child; child = child->next_content) {
      if (GET_OBJ_VNUM(child) == vnum) {
        return child;
      }
    }
  }

  return NULL;
}

// Given a character and a vnum, returns true if it's in their inventory, worn, or in the top level of a container in either slot.
// Does not unequip the object, so be careful about extracting etc!
struct obj_data *ch_has_obj_with_vnum(struct char_data *ch, vnum_t vnum) {
  struct obj_data *result = NULL;

  // Check their carried objects.
  for (struct obj_data *carried = ch->carrying; carried; carried = carried->next_content) {
    if ((result = obj_is_or_contains_obj_with_vnum(carried, vnum))) {
      return result;
    }
  }

  // Check their equipped objects.
  for (int i = 0; i < NUM_WEARS; i++) {
    if ((result = obj_is_or_contains_obj_with_vnum(GET_EQ(ch, i), vnum))) {
      return result;
    }
  }

  return NULL;
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

  snprintf(log_string, sizeof(log_string), "(obj %ld) %s^g%s",
          GET_OBJ_VNUM(obj),
          obj->text.name,
          IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) ? " [wizloaded]" : "");

  if (obj->restring)
    snprintf(ENDOF(log_string), sizeof(log_string) - strlen(log_string), " [restring: %s^g]", obj->restring);

  // We explicitly have to exclude ITEM_PART here because these things 'contain' the deck while in progress.
  if (obj->contains && GET_OBJ_TYPE(obj) != ITEM_PART) {
    // I don't need to see "Containing a blank magazine (restring X) containing..."
    if (GET_OBJ_TYPE(obj->contains) == ITEM_GUN_MAGAZINE && !(obj->contains->next_content)) {
      snprintf(ENDOF(log_string), sizeof(log_string) - strlen(log_string), " (loaded with %d %s)",
               GET_MAGAZINE_AMMO_COUNT(obj->contains),
               get_ammo_representation(GET_MAGAZINE_BONDED_ATTACKTYPE(obj->contains),
                                       GET_MAGAZINE_AMMO_TYPE(obj->contains),
                                       GET_MAGAZINE_AMMO_COUNT(obj->contains)));
    } else {
      snprintf(ENDOF(log_string), sizeof(log_string) - strlen(log_string), ", containing: [");
      for (struct obj_data *temp = obj->contains; temp; temp = temp->next_content) {
        char *representation = generate_new_loggable_representation(temp);
        snprintf(buf3, sizeof(buf3), " %s%s^g%s",
                (!temp->next_content && temp != obj->contains) ? "and " : "",
                representation,
                temp->next_content ? ";" : "");
        strlcat(log_string, buf3, MAX_STRING_LENGTH);
        delete [] representation;
      }
      strlcat(log_string, " ]", MAX_STRING_LENGTH);
    }
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
    const char *name = get_player_name(GET_OBJ_VAL(obj, 0));
    snprintf(ENDOF(log_string), sizeof(log_string) - strlen(log_string), ", bonded to character %s (%d) with %d nuyen on it",
             name,
             GET_OBJ_VAL(obj, 0),
             GET_OBJ_VAL(obj, 1));
    DELETE_ARRAY_IF_EXTANT(name);
  }

  return str_dup(log_string);
}

// Purging a vehicle? Call this function to log it.
void purgelog(struct veh_data *veh) {
  char internal_buffer[MAX_STRING_LENGTH];
  const char *representation = NULL;

  // Begin the purgelog entry.
  snprintf(internal_buffer, sizeof(internal_buffer), "- Writing purgelog for vehicle %s (vnum %ld, idnum %ld, owned by %ld).",
           GET_VEH_NAME(veh),
           veh_index[GET_VEH_RNUM(veh)].vnum,
           veh->idnum, veh->owner);
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
void update_ammobox_ammo_quantity(struct obj_data *ammobox, int amount, const char *caller) {
  if (!ammobox) {
    mudlog_vfprintf(ammobox->carried_by, LOG_SYSLOG, "SYSERR: Null ammobox passed to update_ammobox_ammo_quantity by %s.", caller);
    return;
  }

  if (amount == 0) {
    mudlog_vfprintf(ammobox->carried_by, LOG_SYSLOG, "SYSERR: Zero amount passed to update_ammobox_ammo_quantity by %s.", caller);
    return;
  }

  if (GET_OBJ_TYPE(ammobox) != ITEM_GUN_AMMO) {
    mudlog_vfprintf(ammobox->carried_by, LOG_SYSLOG, "SYSERR: Non-ammobox passed to update_ammobox_ammo_quantity by %s.", caller);
  }

  // Calculate what the new amount of ammo will be.
  GET_AMMOBOX_QUANTITY(ammobox) = GET_AMMOBOX_QUANTITY(ammobox) + amount;

  if (GET_AMMOBOX_QUANTITY(ammobox) < 0) {
    mudlog("SYSERR: Updated ammobox to have negative ammo count! Restoring...", NULL, LOG_SYSLOG, TRUE);
    GET_AMMOBOX_QUANTITY(ammobox) = 0;
  }

  // Update the box's weight. We must fully subtract and re-add it due to miniscule ammo weight amounts.
  weight_change_object(ammobox, -GET_OBJ_WEIGHT(ammobox));
  weight_change_object(ammobox, GET_AMMOBOX_QUANTITY(ammobox) * get_ammo_weight(GET_AMMOBOX_WEAPON(ammobox), GET_AMMOBOX_TYPE(ammobox)));

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

  if (GET_OBJ_TYPE(from) != ITEM_GUN_AMMO || GET_OBJ_TYPE(into) != ITEM_GUN_AMMO) {
    mudlog("SYSERR: combine_ammo_boxes received something that was not an ammo box.", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (GET_AMMOBOX_INTENDED_QUANTITY(from) > 0 || GET_AMMOBOX_INTENDED_QUANTITY(into) > 0) {
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

  update_ammobox_ammo_quantity(into, GET_AMMOBOX_QUANTITY(from), "combine into");
  update_ammobox_ammo_quantity(from, -GET_AMMOBOX_QUANTITY(from), "combine from");

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

  // Restring it, as long as it's not already restrung.
  if (!into->restring) {
    // Compose the new name.
    const char *restring = get_ammobox_default_restring(into);

    // Compose the notification string.
    if (print_messages) {
      send_to_char(ch, "The name '%s' probably doesn't fit anymore, so we'll call it '%s'.\r\n",
        GET_OBJ_NAME(into),
        restring
      );
    }

    // Commit the change.
    into->restring = str_dup(restring);
  }

  // Everything succeeded.
  return TRUE;
}

bool combine_drugs(struct char_data *ch, struct obj_data *from, struct obj_data *into, bool print_messages) {
  if (!ch || !from || !into) {
    mudlog("SYSERR: combine_drugs received a null value.", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (GET_OBJ_TYPE(from) != ITEM_DRUG || GET_OBJ_TYPE(into) != ITEM_DRUG) {
    mudlog("SYSERR: combine_drugs received something that was not a drug.", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  int drug_id = GET_OBJ_DRUG_TYPE(from);

  if (drug_id != GET_OBJ_DRUG_TYPE(into)) {
    mudlog("SYSERR: combine_drug received non-matching drugs.", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (print_messages) {
    send_to_char(ch, "You mix the dose%s from %s into %s.\r\n",
                 GET_OBJ_DRUG_DOSES(from) == 1 ? "" : "s",
                 GET_OBJ_NAME(from),
                 GET_OBJ_NAME(into));
  }

  GET_OBJ_DRUG_DOSES(into) += GET_OBJ_DRUG_DOSES(from);
  GET_OBJ_DRUG_DOSES(from) = 0;

  extract_obj(from);

  // Restring it, as long as it's not already restrung.
  if (!into->restring) {
    // Compose the new name.
    char restring[500];

    snprintf(restring, sizeof(restring), "a box of %s %ss",
      drug_types[drug_id].name,
      drug_types[drug_id].delivery_method
    );

    if (print_messages) {
      send_to_char(ch, "The name '%s' probably doesn't fit anymore, so we'll call it '%s'.\r\n",
        GET_OBJ_NAME(into),
        restring
      );
    }

    // Commit the change.
    into->restring = str_dup(restring);
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
    case SPELL_DEATHTOUCH:
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
    case SPELL_HEALTHYGLOW:
    case SPELL_TOXICWAVE:
    case SPELL_NOVA:
    case SPELL_SMOKECLOUD:
    case SPELL_THUNDERCLAP:
    case SPELL_SPLASH:
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

bool CAN_SEE(struct char_data *subj, struct char_data *obj) {
  struct room_data *subj_in_room = get_ch_in_room(subj);
  struct room_data *obj_in_room = get_ch_in_room(obj);

  if (!subj_in_room || !obj_in_room)
    return FALSE;

  return CAN_SEE_ROOM_SPECIFIED(subj, obj, subj_in_room) && (subj_in_room == obj_in_room || CAN_SEE_ROOM_SPECIFIED(subj, obj, obj_in_room));
}

bool CAN_SEE_ROOM_SPECIFIED(struct char_data *subj, struct char_data *obj, struct room_data *room_specified) {
  if (!subj || !obj)
    return FALSE;

  if (subj == obj)
    return TRUE;

  // Does the viewee have an astral state that makes them invisible to subj?
  if (IS_ASTRAL(obj) && !AFF_FLAGGED(obj, AFF_MANIFEST) && !SEES_ASTRAL(subj))
    return FALSE;

  // Johnsons, trainers, and cab drivers can always see. Them going blind doesn't increase the fun.
  if (npc_can_see_in_any_situation(subj))
    return TRUE;

  // If your vision can't see in the ambient light, fail.
  if (!LIGHT_OK_ROOM_SPECIFIED(subj, room_specified))
    return FALSE;

  // If they're in an invis state (spell or setting) you can't handle, fail.
  if (!invis_ok(subj, obj))
    return FALSE;

  return TRUE;
}

bool LIGHT_OK_ROOM_SPECIFIED(struct char_data *sub, struct room_data *provided_room) {
  // #define DEBUG_LIGHT_OK(msg) {act(msg, 0, sub, 0, 0, TO_ROLLS);}
  #define DEBUG_LIGHT_OK(msg)

  // Fix the ruh-rohs.
  if (!sub || !provided_room)
    return FALSE;

  // If you can see on the astral plane, light means nothing to you.
  if (SEES_ASTRAL(sub)) {
    DEBUG_LIGHT_OK("- L_O_R_S: $n is astral or dual.");
    return TRUE;
  }

  // If you have ultrasonic or thermographic vision or holy light, you're good.
  if (has_vision(sub, VISION_THERMOGRAPHIC)) {
    DEBUG_LIGHT_OK("- L_O_R_S: $n has thermographic vision, can see.");
    return TRUE;
  }
  if (has_vision(sub, VISION_ULTRASONIC)) {
    DEBUG_LIGHT_OK("- L_O_R_S: $n has ultrasonic vision, can see.");
    return TRUE;
  }
  if (HOLYLIGHT_OK(sub)) {
    DEBUG_LIGHT_OK("- L_O_R_S: $n has holylight, can see.");
    return TRUE;
  }

  // If you're in a vehicle, we assume you have headlights and interior lights.
  if (sub->in_veh) {
    DEBUG_LIGHT_OK("- L_O_R_S: $n is in a vehicle and can see a minimal amount.");
    return TRUE;
  }

  /*  removed-- this is theoretically covered by the headlight code in light_level(). - LS
  // We also give allowances for people in rooms with player-occupied cars.
  for (struct veh_data *veh = provided_room->vehicles; veh; veh = veh->next_veh) {
    for (struct char_data *tch = veh->people; tch; tch = tch->next_in_veh)
      if (!IS_NPC(tch))
        return TRUE;
  }
  */

  // Fetch the room's light level. Note that light_level already handles flashlights etc.
  int room_light_level = light_level(provided_room);

  // We already checked for thermo and ultra above, which means at that this point you have LL or Normal vision. You can't see in full darkness.
  if (room_light_level == LIGHT_FULLDARK) {
    DEBUG_LIGHT_OK("- L_O_R_S: $n is in fulldark w/o thermo/ultra: no vision.");
    return FALSE;
  }

  // Between LL and Normal, only LL can see in minlight.
  if (room_light_level == LIGHT_MINLIGHT) {
    if (has_vision(sub, VISION_LOWLIGHT)) {
      DEBUG_LIGHT_OK("- L_O_R_S: $n in minlight with LL vision, can see.");
      return TRUE;
    }
    if (MOB_FLAGGED(sub, MOB_GUARD)) {
      DEBUG_LIGHT_OK("- L_O_R_S: $n in minlight and is a guard, can see.");
      return TRUE;
    }
    DEBUG_LIGHT_OK("- L_O_R_S: $n in minlight w/o LL/flag: no vision.");
    return FALSE;
  }

  // Both LL and Normal can see in all other lighting conditions (partlight, etc).
  DEBUG_LIGHT_OK("- L_O_R_S: $n can see (fallthrough case).");
  return TRUE;
}

float get_proto_weight(struct obj_data *obj) {
  int rnum = real_object(GET_OBJ_VNUM(obj));
  if (rnum <= -1)
    return 0.00;
  else
    return GET_OBJ_WEIGHT(&obj_proto[rnum]);
}

int get_armor_penalty_grade(struct char_data *ch) {
  int total = 0;

  if (GET_TOTALBAL(ch) > GET_QUI(ch))
    total += GET_TOTALBAL(ch) - GET_QUI(ch);
  if (GET_TOTALIMP(ch) > GET_QUI(ch))
    total += GET_TOTALIMP(ch) - GET_QUI(ch);
  if (total > 0) {
    if (total >= GET_QUI(ch))
      return ARMOR_PENALTY_TOTAL;
    else if (total >= GET_QUI(ch) * 3/4)
      return ARMOR_PENALTY_HEAVY;
    else if (total >= GET_QUI(ch) / 2)
      return ARMOR_PENALTY_MEDIUM;
    else
      return ARMOR_PENALTY_LIGHT;
  }

  return ARMOR_PENALTY_NONE;
}

void handle_weapon_attachments(struct obj_data *obj) {
  if (GET_OBJ_TYPE(obj) != ITEM_WEAPON)
    return;

  if (!IS_GUN(GET_WEAPON_ATTACK_TYPE(obj)))
    return;

  int real_obj;

  for (int i = ACCESS_LOCATION_TOP; i <= ACCESS_LOCATION_UNDER; i++)
    if (GET_OBJ_VAL(obj, i) > 0 && (real_obj = real_object(GET_OBJ_VAL(obj, i))) > 0) {
      struct obj_data *mod = &obj_proto[real_obj];
      // We know the attachment code will throw a fit if we attach over the top of an 'existing' object, so wipe it out without removing it.
      GET_OBJ_VAL(obj, i) = 0;
      attach_attachment_to_weapon(mod, obj, NULL, i - ACCESS_ACCESSORY_LOCATION_DELTA);
    }

  if (!GET_WEAPON_FIREMODE(obj)) {
    if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(obj), 1 << MODE_SS))
      GET_WEAPON_FIREMODE(obj) = MODE_SS;
    else if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(obj), 1 << MODE_SA))
      GET_WEAPON_FIREMODE(obj) = MODE_SA;
    else if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(obj), 1 << MODE_BF))
      GET_WEAPON_FIREMODE(obj) = MODE_BF;
    else if (IS_SET(GET_WEAPON_POSSIBLE_FIREMODES(obj), 1 << MODE_FA)) {
      GET_WEAPON_FIREMODE(obj) = MODE_FA;
      GET_OBJ_TIMER(obj) = 10;
    }
  }
}

// Use this to restrict builder movement. Handy for preventing unproven builders abusing their privileges to learn game secrets.
bool builder_cant_go_there(struct char_data *ch, struct room_data *room) {
  if (GET_LEVEL(ch) == LVL_BUILDER) {
    // We assume all builders can go to staff-locked rooms, which are staff-only.
    if (room->staff_level_lock > 1)
      return FALSE;

    // Otherwise, iterate and find the zone for the room. We allow them to go there if they have edit access.
    for (int counter = 0; counter <= top_of_zone_table; counter++) {
      if (GET_ROOM_VNUM(room) >= (zone_table[counter].number * 100)
          && GET_ROOM_VNUM(room) <= zone_table[counter].top)
      {
        if (can_edit_zone(ch, counter))
          return FALSE;

        if (zone_table[counter].number == 100)
          return FALSE;

        return TRUE;
      }
    }
  }
  return FALSE;
}

// Allows morts to have accelerated deckbuilding actions. Useful for when their deck breaks.
bool get_and_deduct_one_deckbuilding_token_from_char(struct char_data *ch) {
  for (struct obj_data *ptr = ch->carrying; ptr; ptr = ptr->next_content) {
    if (GET_OBJ_VNUM(ptr) == OBJ_STAFF_REBATE_FOR_DECKBUILDING) {
      mudlog("Consuming a deckbuilding token to accelerate a deckbuilding task.", ch, LOG_CHEATLOG, TRUE);
      extract_obj(ptr);
      return TRUE;
    }
  }

  return FALSE;
}

// States whether a program can be copied or not.
bool program_can_be_copied(struct obj_data *prog) {
  if (!prog || GET_OBJ_TYPE(prog) != ITEM_PROGRAM)
    return FALSE;

  switch (GET_PROGRAM_TYPE(prog)) {
    case SOFT_ASIST_COLD:
    case SOFT_ASIST_HOT:
    case SOFT_HARDENING:
    case SOFT_ICCM:
    case SOFT_ICON:
    case SOFT_MPCP:
    case SOFT_REALITY:
    case SOFT_RESPONSE:
    case SOFT_BOD:
    case SOFT_SENSOR:
    case SOFT_MASKING:
    case SOFT_EVASION:
    case SOFT_SUITE:
      return FALSE;
  }

  return TRUE;
}

struct obj_data *get_obj_proto_for_vnum(vnum_t vnum) {
  if (vnum <= 0)
    return NULL;

  int rnum = real_object(vnum);
  if (rnum < 0)
    return NULL;

  return &obj_proto[rnum];
}

bool WEAPON_FOCUS_USABLE_BY(struct obj_data *focus, struct char_data *ch) {
  // Nonexistant and non-weapons can't be used as foci.
  if (!focus || !WEAPON_IS_FOCUS(focus))
    return FALSE;

  // Weapons in the middle of bonding can't be used.
  if (GET_WEAPON_FOCUS_BOND_STATUS(focus) != 0)
    return FALSE;

  // Astral projection? We want to check your original character.
  if (IS_PROJECT(ch)) {
    if (ch->desc && ch->desc->original)
      return GET_WEAPON_FOCUS_BONDED_BY(focus) == GET_IDNUM(ch->desc->original);
  }

  // Non-projection NPC? You get to use it.
  if (IS_NPC(ch))
    return TRUE;

  // Otherwise, yes but only if you bonded it.
  return GET_WEAPON_FOCUS_BONDED_BY(focus) == GET_IDNUM(ch);
}

// Cribbed from taxi code. Eventually, we should replace the taxi distance calculation with this.
// Returns -1 for not found or error.
int calculate_distance_between_rooms(vnum_t start_room_vnum, vnum_t target_room_vnum, bool ignore_roads) {
  struct room_data *temp_room = NULL;
  int dist = 0, x = 0;
  rnum_t start_room_rnum, target_room_rnum;

  // Ensure the vnums are valid.
  start_room_rnum = real_room(start_room_vnum);
  if (start_room_rnum < 0) {
    mudlog("SYSERR: Invalid start room vnum passed to calculate_distance_between_rooms().", NULL, LOG_SYSLOG, TRUE);
    return -1;
  }

  target_room_rnum = real_room(target_room_vnum);
  if (target_room_rnum < 0) {
    mudlog("SYSERR: Invalid target room vnum passed to calculate_distance_between_rooms().", NULL, LOG_SYSLOG, TRUE);
    return -1;
  }

  temp_room = &world[start_room_rnum];

  // Remember that temp room starts as null, so if no exit was found then this is skipped.
  while (temp_room) {
    x = find_first_step(real_room(temp_room->number), target_room_rnum, ignore_roads);

    // Arrived at target.
    if (x == BFS_ALREADY_THERE)
      break;

    // Could not find target, and ran out of options.
    if (x == BFS_ERROR || x == BFS_NO_PATH) {
      temp_room = NULL;
      break;
    }

    // Continue.
    temp_room = temp_room->dir_option[x]->to_room;
    dist++;
  }

  // Not found or error: -1.
  if (!temp_room)
    return -1;
  else
    return dist;
}

bool item_should_be_treated_as_melee_weapon(struct obj_data *obj) {
  // Doesn't exist-- fists are melee weapons.
  if (!obj)
    return TRUE;

  // Not a weapon.
  if (GET_OBJ_TYPE(obj) != ITEM_WEAPON)
    return FALSE;

  // It's a gun that has a magazine in it.
  if (IS_GUN(GET_WEAPON_ATTACK_TYPE(obj)) && obj->contains)
    return FALSE;

  // It's a gun that has no magazine (it was EJECTed), or it's not a gun.
  return TRUE;
}

bool item_should_be_treated_as_ranged_weapon(struct obj_data *obj) {
  // Doesn't exist.
  if (!obj)
    return FALSE;

  // Not a weapon.
  if (GET_OBJ_TYPE(obj) != ITEM_WEAPON)
    return FALSE;

  // It's not a gun, or it doesn't have a magazine.
  if (!IS_GUN(GET_WEAPON_ATTACK_TYPE(obj)) || !obj->contains)
    return FALSE;

  // It's a gun that has a magazine in it.
  return TRUE;
}

void turn_hardcore_on_for_character(struct char_data *ch) {
  if (!ch || IS_NPC(ch))
    return;

  PRF_FLAGS(ch).SetBit(PRF_HARDCORE);
  PLR_FLAGS(ch).SetBit(PLR_NODELETE);
  snprintf(buf, sizeof(buf), "UPDATE pfiles SET Hardcore=1, NoDelete=1 WHERE idnum=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
}

void turn_hardcore_off_for_character(struct char_data *ch) {
  if (!ch || IS_NPC(ch))
    return;

  PRF_FLAGS(ch).RemoveBit(PRF_HARDCORE);
  PLR_FLAGS(ch).RemoveBit(PLR_NODELETE);
  snprintf(buf, sizeof(buf), "UPDATE pfiles SET Hardcore=0, NoDelete=0 WHERE idnum=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
}

// Returns -1 on error, make sure you catch that!
int get_string_length_after_color_code_removal(const char *str, struct char_data *ch_to_notify_of_failure_reason) {
  if (!str) {
    mudlog("SYSERR: Null string received to get_string_length_after_color_code_removal().", ch_to_notify_of_failure_reason, LOG_SYSLOG, TRUE);
    return 0;
  }

  const char *ptr = str;
  long ptr_max = strlen(str) - 1;
  int len = 0;

  while (*ptr && (ptr - str) <= ptr_max) {
    if (*ptr == '^') {
      if (*(ptr+1) == '\0') {
        if (ch_to_notify_of_failure_reason)
          send_to_char("Sorry, tag strings can't end with the ^ character.\r\n", ch_to_notify_of_failure_reason);
        return -1;
      }
      // Print a single ^ character.
      else if (*(ptr+1) == '^') {
        ptr += 2;
        len += 1;
      }
      // Print a color character.
      else {
        // There are two types of color: Two-character tags (^g) and xterm tags (^[F123]). We must account for both.
        if (*(ptr+1) == '[') {
          // We know that ptr+1 was a valid character (see first check in this while), so at worst, +2 can be \0.
          if (!*(ptr + 2) || !(*(ptr + 2) == 'F')) {
            if (ch_to_notify_of_failure_reason)
              send_to_char("Sorry, xterm256 colors can only be specified in foreground mode (^^[F...]).\r\n", ch_to_notify_of_failure_reason);
            return -1;
          }
#define IS_CODE_DIGIT_VALID(chr) (*(chr) && (*(chr) == '0' || *(chr) == '1' || *(chr) == '2' || *(chr) == '3' || *(chr) == '4' || *(chr) == '5'))
          if (!IS_CODE_DIGIT_VALID(ptr + 3) || !IS_CODE_DIGIT_VALID(ptr + 4) || !IS_CODE_DIGIT_VALID(ptr + 5)) {
            if (ch_to_notify_of_failure_reason)
              send_to_char("Sorry, you've entered an invalid xterm256 color code.\r\n", ch_to_notify_of_failure_reason);
            return -1;
          }
#undef IS_CODE_DIGIT_VALID
          if (!*(ptr + 6) || *(ptr + 6) != ']') {
            if (ch_to_notify_of_failure_reason)
              send_to_char("You've forgotten to terminate an xterm256 color code.\r\n", ch_to_notify_of_failure_reason);
            return -1;
          }

          ptr += strlen("^[F123]");
        } else {
          ptr += 2;
        }
      }
    } else {
      len += 1;
      ptr += 1;
    }
  }

  return len;
}

// Returns a string stripped of color for keyword matching or possibly other uses as well.
// We don't need to check for color code validity because we call get_string_legth_after_color_code_removal() when the strings are initially written.
char* get_string_after_color_code_removal(const char *str, struct char_data *ch) {
  if (!str) {
    mudlog("SYSERR: Null string received to get_string_after_color_code_removal().", ch, LOG_SYSLOG, TRUE);
    return NULL;
  }

  const char *ptr = str;
  long ptr_max = strlen(str) - 1;

  static char clearstr [MAX_STRING_LENGTH];
  memset(clearstr, 0, sizeof(clearstr));
  int pos = 0;

  while (*ptr && (ptr - str) <= ptr_max) {
    // Buffer overflow failsafe.
    if (pos == MAX_STRING_LENGTH - 1) {
      clearstr[pos] = '\0';
      return clearstr;
    }

    if (*ptr == '^') {
      // Parse a single ^ character.
      if (*(ptr+1) == '^') {
        clearstr[pos] = *(ptr+1);
        pos++;
        ptr += 2;
        continue;
      }
      // Skip color codes.
      // There are two types of color: Two-character tags (^g) and xterm tags (^[F123]). We must account for both.
      // 7 for xterm tags 2 for regular tags
      else if (*(ptr+1) == '[') {
          ptr  += 7;
      }
      else {
        ptr += 2;
      }
    }
    //Clear character, save it.
    else {
      clearstr[pos] = *ptr;
      pos++;
      ptr += 1;
    }
  }
  return  clearstr;
}

// Returns the amount of color codes in a string.
int count_color_codes_in_string(const char *str) {
  const char *ptr = str;
  long ptr_max = strlen(str) - 1;

  int sum = 0;

  while (*ptr && (ptr - str) <= ptr_max) {
    if (*ptr == '^') {
      // Parse a single ^ character.
      if (*(ptr+1) == '^') {
        sum++;
        ptr += 2;
        continue;
      }
      // Count color codes.
      // There are two types of color: Two-character tags (^g) and xterm tags (^[F123]). We must account for both.
      // 7 for xterm tags 2 for regular tags
      else if (*(ptr+1) == '[') {
          ptr  += 7;
          sum += 7;
      }
      else {
        ptr += 2;
        sum += 2;
      }
    }
    //Clear character, save it.
    else
      ptr += 1;
  }
  return  sum;
}

char *get_obj_name_with_padding(struct obj_data *obj, int padding) {
  static char namestr[MAX_INPUT_LENGTH * 2];
  char paddingnumberstr[50], formatstr[50];

  snprintf(paddingnumberstr, sizeof(paddingnumberstr), "%d", padding + count_color_codes_in_string(GET_OBJ_NAME(obj)));
  snprintf(formatstr, sizeof(formatstr), "%s%s%s", "%-", paddingnumberstr, "s^n");
  snprintf(namestr, sizeof(namestr), formatstr, GET_OBJ_NAME(obj));

  return namestr;
}

// Returns TRUE if the NPC has a spec that should protect it from damage, FALSE otherwise.
bool npc_is_protected_by_spec(struct char_data *npc) {
  return (CHECK_FUNC_AND_SFUNC_FOR(npc, shop_keeper)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, johnson)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, landlord_spec)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, postmaster)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, teacher)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, metamagic_teacher)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, trainer)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, adept_trainer)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, spell_trainer)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, receptionist)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, fixer)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, fence)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, taxi)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, painter)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, nerp_skills_teacher)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, hacker));
}
// Returns TRUE if the NPC should be able to see in any situation.
bool npc_can_see_in_any_situation(struct char_data *npc) {
  if (!IS_NPC(npc))
    return FALSE;

  return (CHECK_FUNC_AND_SFUNC_FOR(npc, johnson)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, teacher)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, metamagic_teacher)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, trainer)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, adept_trainer)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, spell_trainer)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, taxi)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, nerp_skills_teacher)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, shop_keeper)
          || CHECK_FUNC_AND_SFUNC_FOR(npc, landlord_spec)
        );
}

bool can_damage_vehicle(struct char_data *ch, struct veh_data *veh) {
  if (veh->owner && GET_IDNUM(ch) != veh->owner) {
    bool has_valid_vict = FALSE;
    for (struct char_data *killer_check = veh->people; killer_check; killer_check = killer_check->next_in_veh) {
      if ((PRF_FLAGGED(ch, PRF_PKER) && PRF_FLAGGED(killer_check, PRF_PKER)) || PLR_FLAGGED(killer_check, PLR_KILLER)) {
        has_valid_vict = TRUE;
        break;
      }
    }

    if (!has_valid_vict) {
      if (!PRF_FLAGGED(ch, PRF_PKER) && !get_plr_flag_is_set_by_idnum(PLR_KILLER, veh->owner)) {
        send_to_char("That's a player-owned vehicle. Better leave it alone.\r\n", ch);
        return FALSE;
      }

      if (!get_prf_flag_is_set_by_idnum(PRF_PKER, veh->owner)) {
        send_to_char("The owner of that vehicle is not flagged PK. Better leave it alone.\r\n", ch);
        return FALSE;
      }
    }
    // PLR_FLAGS(ch).SetBit(PLR_KILLER);
    // send_to_char(KILLER_FLAG_MESSAGE, ch);
  }

  // No failure conditions found.
  return TRUE;
}

// Precondition: You must pass in a valid type.
char *compose_spell_name(int type, int subtype) {
  static char name_buf[500];

  strlcpy(name_buf, spells[type].name, sizeof(name_buf));

  if (SPELL_HAS_SUBTYPE(type)) {
    strlcat(name_buf, attributes[subtype], sizeof(name_buf));
  }

  return name_buf;
}

bool obj_contains_kept_items(struct obj_data *obj) {
  if (!obj) {
    mudlog("SYSERR: Received null object to obj_contains_kept_items().", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (!obj->contains || GET_OBJ_TYPE(obj) == ITEM_PART) {
    return FALSE;
  }

  // Iterate over each item in the content list.
  for (struct obj_data *tmp = obj->contains; tmp; tmp = tmp->next_content) {
    // If this item is kept, return true.
    if (IS_OBJ_STAT(tmp, ITEM_EXTRA_KEPT))
      return TRUE;

    // If this item contains kept items, return true.
    if (tmp->contains && obj_contains_kept_items(tmp))
      return TRUE;
  }
  // We found no kept items- return false.
  return FALSE;
}

// Just a little helper function to message everyone.
void send_gamewide_annoucement(const char *msg, bool prepend_announcement_string) {
  if (!msg || !*msg) {
    mudlog("SYSERR: Received null or empty message to send_gamewide_annoucement().", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  char announcement[MAX_STRING_LENGTH];
  snprintf(announcement, sizeof(announcement), "%s%s\r\n", prepend_announcement_string ? "^WGamewide Announcement:^n " : "", msg);

  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    if (!d->connected && d->character) {
      send_to_char(d->character, announcement);
    }
  }
}

// Given an NPC, reads out the unique-on-this-boot-only ID number associated with this character.
char *get_printable_mob_unique_id(struct char_data *ch) {
  static char result_buf[1000];
  // If you're using UUIDs here, you need something like this:
  // uuid_unparse(GET_MOB_UNIQUE_ID(ch), result_buf);
  // Buuut since we're just using unsigned longs...
  snprintf(result_buf, sizeof(result_buf), "%ld", GET_MOB_UNIQUE_ID(ch));
  return result_buf;
}

// This sort of function is dead stupid for comparing two unsigned longs... but if you change to UUIDs, you'll appreciate being able to change this.
bool mob_unique_id_matches(mob_unique_id_t id1, mob_unique_id_t id2) {
  // return uuid_compare(id1, id2) == 0;
  return id1 == id2;
}

// Don't @ me about having a global declared here, it's only used in this one function. -LS
unsigned long global_mob_unique_id_number = 0;
void set_new_mobile_unique_id(struct char_data *ch) {
  // Eventually, you'll swap this out for UUIDs, assuming we find a use case for that.
  // uuid_generate(GET_MOB_UNIQUE_ID(ch));
  GET_MOB_UNIQUE_ID(ch) = global_mob_unique_id_number++;
}

void knockdown_character(struct char_data *ch) {
  // Error guard.
  if (!ch) {
    mudlog("SYSERR: Received NULL character to knockdown_character()!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  // No knockdown if you're already down.
  if (AFF_FLAGGED(ch, AFF_PRONE))
    return;

  // Getting knocked down staggers your commands.
  WAIT_STATE(ch, 1 RL_SEC);

  bool eligible_for_kipup = PLR_FLAGGED(ch, PLR_PAID_FOR_KIPUP) || (IS_NPC(ch) && (GET_QUI(ch) >= 10 && GET_SKILL(ch, SKILL_UNARMED_COMBAT) >= 6));

  if (eligible_for_kipup && PRF_FLAGGED(ch, PRF_AUTOKIPUP) && !IS_JACKED_IN(ch)) {
    // Successfully kipping up gets you back on your feet with no penalty.
    if (success_test(GET_QUI(ch), 6) > 0) {
      send_to_char("^yYou're sent sprawling, but recover with a quick kip-up!\r\n", ch);
      act("$n is knocked down, but pops right back up again!", TRUE, ch, 0, 0, TO_ROOM);
      return;
    }

    // Char failed to kip up.
    GET_INIT_ROLL(ch) -= 5;

    // If they want to autostand no matter what, let them.
    if (PRF_FLAGGED(ch, PRF_AUTOSTAND)) {
      send_to_char("^yYou're sent sprawling, and your failed attempt to kip-up adds insult to injury!\r\n^yIt takes you a long moment to struggle back to your feet.^n\r\n", ch);
      act("$n is knocked down, only regaining $s feet after an embarrassing struggle!", TRUE, ch, 0, 0, TO_ROOM);
      GET_INIT_ROLL(ch) -= 5;
      return;
    }

    // Otherwise, notify them of the failed kip-up and just send them prone.
    send_to_char("^yYou're sent sprawling, and your failed attempt to kip-up adds insult to injury!^n\r\n", ch);
    act("After being knocked down, $n wriggles around in a vain attempt to kip back up!", TRUE, ch, 0, 0, TO_ROOM);
    AFF_FLAGS(ch).SetBit(AFF_PRONE);

    if (PRF_FLAGGED(ch, PRF_SEE_TIPS)) {
      send_to_char("^c(You'll probably want to ^WKIPUP^c or ^WSTAND^c.)^n\r\n", ch);
    }
    return;
  }

  // Otherwise, if they're set up to auto-stand...
  if (PRF_FLAGGED(ch, PRF_AUTOSTAND) && !IS_JACKED_IN(ch)) {
    GET_INIT_ROLL(ch) -= 5;
    send_to_char("^yYou're sent sprawling, and it takes you a moment to clamber back to your feet.\r\n", ch);
    act("$n is knocked down, but clambers back to $s feet!", TRUE, ch, 0, 0, TO_ROOM);
    return;
  }

  // Neither autokip nor autostand.
  if (!IS_JACKED_IN(ch))
    send_to_char("^YYou are knocked prone!^n\r\n", ch);
  act("$n is knocked prone!", TRUE, ch, 0, 0, TO_ROOM);
  AFF_FLAGS(ch).SetBit(AFF_PRONE);

  if (PRF_FLAGGED(ch, PRF_SEE_TIPS)) {
    send_to_char("^c(You'll probably want to ^WKIPUP^c or ^WSTAND^c.)^n\r\n", ch);
  }
}

// Performing a knockdown test (SR3 p124)
bool perform_knockdown_test(struct char_data *ch, int initial_tn, int successes_to_avoid_knockback) {
  // Some things can't be knocked down.
  if (AFF_FLAGGED(ch, AFF_PRONE) || MOB_FLAGGED(ch, MOB_EMPLACED))
    return FALSE;

  if (GET_PHYSICAL(ch) <= (10 - damage_array[DEADLY]) || GET_MENTAL(ch) <= (10 - damage_array[DEADLY])) {
    // If you're already mortally wounded (or knocked out), going prone won't matter.
    return TRUE;
  }

  char rbuf[5000];
  snprintf(rbuf, sizeof(rbuf), "^mKD test: %s. Mods: ", GET_CHAR_NAME(ch));
  int tn_modifiers = modify_target_rbuf_raw(ch, rbuf, sizeof(rbuf), 8, FALSE);

  bool has_tail = FALSE, has_aug = FALSE;
  for (struct obj_data *cyb = ch->cyberware; cyb; cyb = cyb->next_content) {
    switch (GET_CYBERWARE_TYPE(cyb)) {
      case CYB_BALANCETAIL:
        has_tail = TRUE;
        break;
      case CYB_BALANCEAUG:
        has_aug = TRUE;
        break;
      case CYB_FOOTANCHOR:
        if (!GET_CYBERWARE_IS_DISABLED(cyb))
          tn_modifiers += -1;
        buf_mod(rbuf, sizeof(rbuf), "foot anchors", GET_CYBERWARE_RATING(cyb));
        break;
    }
  }

  if (has_tail && has_aug) {
    tn_modifiers += -3;
    buf_mod(rbuf, sizeof(rbuf), "balance tail & aug", -3);
  } else if (has_tail || has_aug) {
    tn_modifiers += -2;
    buf_mod(rbuf, sizeof(rbuf), "balance tail or aug", -2);
  }

  if (GET_PHYSICAL(ch) <= (10 - damage_array[SERIOUS]) || GET_MENTAL(ch) <= (10 - damage_array[SERIOUS])) {
    successes_to_avoid_knockback = MAX(successes_to_avoid_knockback, 4);
  }

  else if (GET_PHYSICAL(ch) <= (10 - damage_array[MODERATE]) || GET_MENTAL(ch) <= (10 - damage_array[MODERATE])) {
    successes_to_avoid_knockback = MAX(successes_to_avoid_knockback, 3);
  }

  else if (GET_PHYSICAL(ch) <= (10 - damage_array[LIGHT]) || GET_MENTAL(ch) <= (10 - damage_array[LIGHT])) {
    successes_to_avoid_knockback = MAX(successes_to_avoid_knockback, 2);
  }

  // Roll the test.
  int dice = GET_BOD(ch) + GET_BODY(ch);
  int tn = MAX(2, initial_tn + tn_modifiers);
  int successes = success_test(dice, tn);
  snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "%s. %d dice (%d + %d) vs TN %d (%d + %d): %d hit%s (goal: %d).",
           tn_modifiers ? "" : "None",
           dice,
           GET_BOD(ch),
           GET_BODY(ch),
           tn,
           initial_tn,
           tn_modifiers,
           successes,
           successes == 1 ? "" : "s",
           successes_to_avoid_knockback
         );

  if (successes <= 0) {
    strlcat(rbuf, " Knockdown!", sizeof(rbuf));
    act(rbuf, FALSE, ch, 0, 0, TO_ROLLS);
    knockdown_character(ch);
    return TRUE;
  } else if (successes < successes_to_avoid_knockback) {
    strlcat(rbuf, " Knocked back.", sizeof(rbuf));
  } else {
    strlcat(rbuf, " No effect.", sizeof(rbuf));
  }

  act(rbuf, FALSE, ch, 0, 0, TO_ROLLS);
  return FALSE;
}

const char *get_level_wholist_color(int level) {
  switch (level) {
    case LVL_BUILDER:
      return "^g";
    case LVL_ARCHITECT:
      return "^G";
    case LVL_FIXER:
      return "^m";
    case LVL_CONSPIRATOR:
      return "^M";
    case LVL_EXECUTIVE:
      return "^c";
    case LVL_DEVELOPER:
      return "^C";
    case LVL_VICEPRES:
      return "^y";
    case LVL_ADMIN:
      return "^b";
    case LVL_PRESIDENT:
      return "^B";
    default:
      return "^n";
  }
}

long get_room_gridguide_x(vnum_t room_vnum) {
  return room_vnum - (room_vnum * 3);
}

long get_room_gridguide_y(vnum_t room_vnum) {
  return room_vnum + 100;
}

vnum_t vnum_from_gridguide_coordinates(long x, long y, struct char_data *ch, struct veh_data *veh) {
  // We could just use the y-coordinate, but then the X could be anything.
  int vnum_from_y = y - 100;
  int vnum_from_x = x - ((x / 2) * 3);

  // Probably a typo.
  if (vnum_from_x != vnum_from_y)
    return -1;

  vnum_t candidate_vnum = vnum_from_y;
  rnum_t candidate_rnum = real_room(candidate_vnum);

  // -2: Room didn't even exist.
  if (candidate_rnum <= 0)
    return -2;

  // -3: Room was not drivable, or not on the gridguide system.
  if (!ROOM_FLAGS(&world[candidate_rnum]).AreAnySet(ROOM_ROAD, ROOM_GARAGE, ENDBIT)
      || ROOM_FLAGS(&world[candidate_rnum]).AreAnySet(ROOM_NOGRID, ROOM_STAFF_ONLY, ROOM_NOBIKE, ROOM_FALL, ENDBIT))
  {
    if (ch) {
      char susbuf[1000];
      snprintf(susbuf, sizeof(susbuf), "%s attempted to grid to room #%ld (%s), but it's not a legal gridguide destination.",
               GET_CHAR_NAME(ch),
               candidate_vnum,
               GET_ROOM_NAME(&world[candidate_rnum])
              );
      mudlog(susbuf, ch, LOG_CHEATLOG, TRUE);

      // send_to_char("^Y(FYI, if that had actually worked, it would have been an exploit!)^n\r\n", ch);
    }

    return -3;
  }

  // -4: Vehicle not compatible with room.
  if (veh && !room_accessible_to_vehicle_piloted_by_ch(&world[candidate_rnum], veh, ch)) {
    return -4;
  }

  return candidate_vnum;
}

const char *get_gridguide_coordinates_for_room(struct room_data *room) {
  static char coords[100];

  if (!room) {
    mudlog("SYSERR: Received NULL room to get_gridguide_coordinates_for_room()!", NULL, LOG_SYSLOG, TRUE);
    return NULL;
  }

  long x_coord = get_room_gridguide_x(GET_ROOM_VNUM(room));
  long y_coord = get_room_gridguide_y(GET_ROOM_VNUM(room));

  if (vnum_from_gridguide_coordinates(x_coord, y_coord, NULL, NULL) > 0) {
    snprintf(coords, sizeof(coords), "%ld, %ld", x_coord, y_coord);
    return coords;
  }

  return NULL;
}

int get_zone_index_number_from_vnum(vnum_t vnum) {
  for (int counter = 0; counter <= top_of_zone_table; counter++) {
    if ((vnum >= (zone_table[counter].number * 100)) &&
        (vnum <= (zone_table[counter].top))) {
      return counter;
    }
  }
  return -1;
}

bool room_accessible_to_vehicle_piloted_by_ch(struct room_data *room, struct veh_data *veh, struct char_data *ch) {
  if (veh->type == VEH_BIKE && ROOM_FLAGGED(room, ROOM_NOBIKE))
    return FALSE;

  if (!ROOM_FLAGGED(room, ROOM_ROAD) && !ROOM_FLAGGED(room, ROOM_GARAGE)) {
    if (veh->type != VEH_BIKE && veh->type != VEH_DRONE)
      return FALSE;
  }

  if (ROOM_FLAGGED(room, ROOM_HOUSE) && !House_can_enter(ch, GET_ROOM_VNUM(room))) {
    return FALSE;
  }

  #ifdef DEATH_FLAGS
    if (ROOM_FLAGGED(room, ROOM_DEATH)) {
      return FALSE;
    }
  #endif

  // Flying vehicles can traverse any terrain.
  if (!ROOM_FLAGGED(room, ROOM_ALL_VEHICLE_ACCESS) && !veh_can_traverse_air(veh)) {
    // Non-flying vehicles can't pass fall rooms.
    if (ROOM_FLAGGED(room, ROOM_FALL)) {
      return FALSE;
    }

    // Check to see if your vehicle can handle the terrain type you're giving it.
    if (IS_WATER(room)) {
      if (!veh_can_traverse_water(veh)) {
        return FALSE;
      }
    } else {
      if (!veh_can_traverse_land(veh)) {
        return FALSE;
      }
    }
  }

  if (ROOM_FLAGGED(room, ROOM_TOO_CRAMPED_FOR_CHARACTERS) && (veh->body > 1 || veh->type != VEH_DRONE)) {
    return FALSE;
  }

  for (struct char_data *tch = veh->people; tch; tch = tch->next_in_veh) {
    if (ROOM_FLAGGED(room, ROOM_STAFF_ONLY) && !IS_NPC(tch) && !access_level(tch, LVL_BUILDER)) {
      return FALSE;
    }

    if (ROOM_FLAGGED(room, ROOM_HOUSE) && !House_can_enter(tch, GET_ROOM_VNUM(room))) {
      return FALSE;
    }
  }

  return TRUE;
}

bool veh_can_traverse_land(struct veh_data *veh) {
  if (veh_can_traverse_air(veh) || veh->flags.IsSet(VFLAG_AMPHIB)) {
    return TRUE;
  }

  switch (veh->type) {
    case VEH_DRONE:
    case VEH_BIKE:
    case VEH_CAR:
    case VEH_TRUCK:
    case VEH_FIXEDWING:
    case VEH_ROTORCRAFT:
    case VEH_VECTORTHRUST:
    case VEH_HOVERCRAFT:
    case VEH_LTA:
      return TRUE;
  }

  return FALSE;
}

bool veh_can_traverse_water(struct veh_data *veh) {
  if (veh_can_traverse_air(veh) || veh->flags.IsSet(VFLAG_AMPHIB)) {
    return TRUE;
  }

  switch (veh->type) {
    case VEH_FIXEDWING:
    case VEH_ROTORCRAFT:
    case VEH_VECTORTHRUST:
    case VEH_HOVERCRAFT:
    case VEH_MOTORBOAT:
    case VEH_SHIP:
    case VEH_LTA:
      return TRUE;
  }

  return FALSE;
}

bool veh_can_traverse_air(struct veh_data *veh) {
  if (veh->flags.IsSet(VFLAG_CAN_FLY)) {
    return TRUE;
  }

  switch (veh->type) {
    case VEH_FIXEDWING:
    case VEH_ROTORCRAFT:
    case VEH_VECTORTHRUST:
    case VEH_LTA:
      return TRUE;
  }

  return FALSE;
}

int get_pilot_skill_for_veh(struct veh_data *veh) {
  if (!veh) {
    mudlog("SYSERR: NULL vehicle to get_pilot_skill_for_veh()!", NULL, LOG_SYSLOG, TRUE);
    return 0;
  }

  switch(veh->type) {
    case VEH_DRONE:
      mudlog("SYSERR: Called get_pilot_skill_for_veh() on a drone!", NULL, LOG_SYSLOG, TRUE);
      return 0;
    case VEH_BIKE:
      return SKILL_PILOT_BIKE;
    case VEH_CAR:
      return SKILL_PILOT_CAR;
    case VEH_TRUCK:
      return SKILL_PILOT_TRUCK;
    case VEH_FIXEDWING:
      return SKILL_PILOT_FIXEDWING;
    case VEH_ROTORCRAFT:
      return SKILL_PILOT_ROTORCRAFT;
    case VEH_VECTORTHRUST:
      return SKILL_PILOT_VECTORTHRUST;
    case VEH_HOVERCRAFT:
      return SKILL_PILOT_HOVERCRAFT;
    case VEH_MOTORBOAT:
      return SKILL_PILOT_MOTORBOAT;
    case VEH_SHIP:
      return SKILL_PILOT_SHIP;
    case VEH_LTA:
      return SKILL_PILOT_LTA;
    case VEH_SEMIBALLISTIC:
      return SKILL_PILOT_SEMIBALLISTIC;
    case VEH_SUBORBITAL:
      return SKILL_PILOT_SUBORBITAL;
    case VEH_TRACKED:
      return SKILL_PILOT_TRACKED;
    case VEH_WALKER:
      return SKILL_PILOT_WALKER;
    default:
      {
        char oopsbuf[500];
        snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Unknown veh type %d to get_br_skill_for_veh()!", veh->type);
        mudlog(oopsbuf, NULL, LOG_SYSLOG, TRUE);
      }
      return 0;
  }
}

int get_br_skill_for_veh(struct veh_data *veh) {
  if (!veh) {
    mudlog("SYSERR: NULL vehicle to get_br_skill_for_veh()!", NULL, LOG_SYSLOG, TRUE);
    return 0;
  }

  switch(veh->type) {
    case VEH_DRONE:
      return SKILL_BR_DRONE;
    case VEH_BIKE:
      return SKILL_BR_BIKE;
    case VEH_CAR:
      return SKILL_BR_CAR;
    case VEH_TRUCK:
      return SKILL_BR_TRUCK;
    case VEH_FIXEDWING:
      return SKILL_BR_FIXEDWING;
    case VEH_ROTORCRAFT:
      return SKILL_BR_ROTORCRAFT;
    case VEH_VECTORTHRUST:
      return SKILL_BR_VECTORTHRUST;
    case VEH_HOVERCRAFT:
      return SKILL_BR_HOVERCRAFT;
    case VEH_MOTORBOAT:
      return SKILL_BR_MOTORBOAT;
    case VEH_SHIP:
      return SKILL_BR_SHIP;
    case VEH_LTA:
      return SKILL_BR_LTA;
    default:
      {
        char oopsbuf[500];
        snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Unknown veh type %d to get_br_skill_for_veh()!", veh->type);
        mudlog(oopsbuf, NULL, LOG_SYSLOG, TRUE);
      }
      return 0;
  }
}

// Rigger 3 p.61-62
int calculate_vehicle_weight(struct veh_data *veh) {
  int body = veh->body;
  int load;

  // Try to parse out the prototype's body.
  {
    rnum_t real_veh = real_vehicle(GET_VEH_VNUM(veh));
    if (real_veh >= 0) {
      // log_vfprintf("Overwriting veh body %d with proto's %d in calculate_vehicle_weight()", body, veh_proto[real_veh].body);
      body = veh_proto[real_veh].body;
    } else {
      // log_vfprintf("Unable to find proto for veh %ld in calculate_vehicle_weight()", GET_VEH_VNUM(veh));
    }
  }

  switch (body) {
    case 0:
      load = 2;
      break;
    case 1:
      load = 20;
      break;
    case 2:
      load = 150;
      break;
    case 3:
      load = 500;
      break;
    case 4:
      load = 1500;
      break;
    case 5:
      load = 4000;
      break;
    case 6:
      load = 12000;
      break;
    case 7:
      load = 25000;
      break;
    case 8:
      load = 35000;
      break;
    case 9:
      load = 50000;
      break;
    case 10:
      load = 75000;
      break;
    default:
      load = 500;
      break;
  }
  load += veh->usedload;

  return load;
}

int roll_default_initiative(struct char_data *ch) {
  return dice(1 + GET_INIT_DICE(ch), 6) + GET_REA(ch);
}

int pilot_skills[] {
  SKILL_PILOT_BIKE,
  SKILL_PILOT_CAR,
  SKILL_PILOT_TRUCK,
  SKILL_PILOT_ROTORCRAFT,
  SKILL_PILOT_FIXEDWING,
  SKILL_PILOT_VECTORTHRUST,
  SKILL_PILOT_HOVERCRAFT,
  SKILL_PILOT_MOTORBOAT,
  SKILL_PILOT_SHIP,
  SKILL_PILOT_LTA
};
#define NUM_PILOT_SKILLS 10

void load_vehicle_brain(struct veh_data *veh) {
  if (!veh->people) {
    struct char_data *brain = read_mobile(MOB_BRAIN_IN_A_JAR, VIRTUAL);

    // Vehicle's skills are autonav rating.
    for (int idx = 0; idx < NUM_PILOT_SKILLS; idx++)
      GET_SKILL(brain, pilot_skills[idx]) = veh->autonav;

    char_to_veh(veh, brain);
  } else {
    mudlog("SYSERR: Called load_vehicle_brain on a vehicle that had occupants!", NULL, LOG_SYSLOG, TRUE);
  }
}

void remove_vehicle_brain(struct veh_data *veh) {
  for (struct char_data *tmp_next, *tmp = veh->people; tmp; tmp = tmp_next) {
    tmp_next = tmp->next_in_veh;
    if (GET_MOB_VNUM(tmp) == MOB_BRAIN_IN_A_JAR) {
      extract_char(tmp);
    }
  }

  repair_vehicle_seating(veh);
}

struct obj_data *make_new_finished_part(int part_type, int mpcp, int rating=0) {
  struct obj_data *part = read_object(OBJ_BLANK_PART_DESIGN, VIRTUAL);
  GET_PART_TYPE(part) = part_type;
  GET_PART_DESIGN_COMPLETION(part) = 0;
  GET_PART_TARGET_MPCP(part) = mpcp;
  GET_PART_PART_COST(part) = 0;
  GET_PART_CHIP_COST(part) = 0;

  if (!rating) {
    extern int get_part_maximum_rating(struct obj_data *part);
    rating = get_part_maximum_rating(part);
  }
  GET_PART_RATING(part) = rating;

  part->obj_flags.extra_flags.SetBit(ITEM_EXTRA_WIZLOAD);
  part->obj_flags.extra_flags.SetBit(ITEM_EXTRA_NORENT);
  part->obj_flags.extra_flags.SetBit(ITEM_EXTRA_NOSELL);

  char restring[500];
  snprintf(restring, sizeof(restring), "a rating-%d %s part (MPCP %d)", rating, parts[part_type].name, mpcp);
  part->restring = str_dup(restring);

  return part;
}

struct obj_data *make_new_finished_program(int part_type, int mpcp, int rating=0) {
  struct obj_data *prog = read_object(OBJ_BLANK_PROGRAM, VIRTUAL);

  GET_PROGRAM_TYPE(prog) = part_type;
  GET_PROGRAM_SIZE(prog) = 1;
  GET_PROGRAM_ATTACK_DAMAGE(prog) = DEADLY;
  GET_PROGRAM_IS_DEFAULTED(prog) = TRUE;
  GET_OBJ_TIMER(prog) = 1;

  if (!rating) {
    rating = mpcp;
  }
  GET_PROGRAM_RATING(prog) = rating;

  prog->obj_flags.extra_flags.SetBit(ITEM_EXTRA_WIZLOAD);
  prog->obj_flags.extra_flags.SetBit(ITEM_EXTRA_NORENT);
  prog->obj_flags.extra_flags.SetBit(ITEM_EXTRA_NOSELL);

  char restring[500];
  snprintf(restring, sizeof(restring), "a rating-%d %s program (MPCP %d)", rating, programs[part_type].name, mpcp);
  prog->restring = str_dup(restring);

  return prog;
}

struct obj_data *make_staff_deck_target_mpcp(int mpcp) {
  struct obj_data *new_deck = read_object(OBJ_CUSTOM_CYBERDECK_SHELL, VIRTUAL);

  // Add parts.
  obj_to_obj(make_new_finished_part(PART_MPCP, mpcp, mpcp), new_deck);
  obj_to_obj(make_new_finished_part(PART_BOD, mpcp, (int) (mpcp / 3)), new_deck);
  obj_to_obj(make_new_finished_part(PART_SENSOR, mpcp, (int) (mpcp / 3)), new_deck);
  obj_to_obj(make_new_finished_part(PART_MASKING, mpcp, (int) (mpcp / 3)), new_deck);
  obj_to_obj(make_new_finished_part(PART_ASIST_HOT, mpcp), new_deck);
  obj_to_obj(make_new_finished_part(PART_RAS_OVERRIDE, mpcp), new_deck);

  // Add software.
  for (int soft_type = SOFT_ATTACK; soft_type <= SOFT_LOCKON; soft_type++) {
    obj_to_obj(make_new_finished_program(soft_type, mpcp), new_deck);
  }

  GET_CYBERDECK_MPCP(new_deck) = mpcp;
  GET_CYBERDECK_HARDENING(new_deck) = mpcp;
  GET_CYBERDECK_ACTIVE_MEMORY(new_deck) = mpcp * 250;
  GET_CYBERDECK_TOTAL_STORAGE(new_deck) = mpcp * 600;
  GET_CYBERDECK_RESPONSE_INCREASE(new_deck) = MIN(3, (int)(mpcp / 4));
  GET_CYBERDECK_IO_RATING(new_deck) = mpcp * 100;
  GET_CYBERDECK_IS_INCOMPLETE(new_deck) = FALSE;

  new_deck->obj_flags.extra_flags.SetBit(ITEM_EXTRA_WIZLOAD);
  new_deck->obj_flags.extra_flags.SetBit(ITEM_EXTRA_NORENT);
  new_deck->obj_flags.extra_flags.SetBit(ITEM_EXTRA_NOSELL);

  char restring[500];
  snprintf(restring, sizeof(restring), "a staff-only MPCP-%d cyberdeck", mpcp);
  new_deck->restring = str_dup(restring);

  return new_deck;
}

// Replaces ^n with ^y for example. Only works with two-character codes.
char *replace_neutral_color_codes(const char *input, const char *replacement_code) {
  static char internal_buf[MAX_STRING_LENGTH];
  char *internal_buf_ptr = internal_buf;

  for (const char *ptr = input; *ptr; ptr++) {
    if (*ptr == '^' && (*(ptr + 1) == 'n' || *(ptr + 1) == 'N')) {
      *(internal_buf_ptr++) = *(ptr++);
      *(internal_buf_ptr++) = *(replacement_code + 1);
    } else {
      *(internal_buf_ptr++) = *ptr;
    }
  }
  *internal_buf_ptr = '\0';

  return internal_buf;
}

// Fix a vehicle's seating amounts. Returns TRUE on change, FALSE otherwise.
bool repair_vehicle_seating(struct veh_data *veh) {
  struct obj_data *mod;

  // First, calculate the expected seating for front and back.
  int front = veh_proto[veh->veh_number].seating[SEATING_FRONT];
  int rear = veh_proto[veh->veh_number].seating[SEATING_REAR];

  // Next, subtract seating due to people in the vehicle. Skip staff.
  for (struct char_data *ch = veh->people; ch; ch = ch->next_in_veh) {
    if (GET_LEVEL(ch) <= LVL_MORTAL) {
      if (ch->vfront)
        front--;
      else
        rear--;
    }
  }

  // Then add in any seating mods.
  for (int slot = 0; slot < NUM_MODS; slot++) {
    if (!(mod = GET_MOD(veh, slot)))
      continue;

    // Read the affects of the mod.
    for (int aff_slot = 0; aff_slot < MAX_OBJ_AFFECT; aff_slot++) {
      switch (mod->affected[aff_slot].location) {
        case VAFF_SEAF:
          front += mod->affected[aff_slot].modifier;
          break;
        case VAFF_SEAB:
          rear += mod->affected[aff_slot].modifier;
          break;
      }
    }
  }

  // Finally, apply this to the vehicle if needed.
  if (veh->seating[SEATING_FRONT] != front || veh->seating[SEATING_REAR] != rear) {
    veh->seating[SEATING_FRONT] = front;
    veh->seating[SEATING_REAR] = rear;
    return TRUE;
  }

  // No changes made.
  return FALSE;
}

bool is_voice_masked(struct char_data *ch) {
  for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content) {
    if (GET_CYBERWARE_TYPE(obj) == CYB_VOICEMOD && GET_CYBERWARE_FLAGS(obj)) {
      return TRUE;
    }
  }
  if (AFF_FLAGGED(ch, AFF_VOICE_MODULATOR)) {
    return TRUE;
  }

  return FALSE;
}

// Forces a character to perceive if they can.
bool force_perception(struct char_data *ch) {
  // No need to do this if they're already perceiving.
  if (SEES_ASTRAL(ch))
    return TRUE;

  switch (GET_TRADITION(ch)) {
    case TRAD_SHAMANIC:
    case TRAD_HERMETIC:
      break;
    case TRAD_ADEPT:
      if (GET_POWER(ch, ADEPT_PERCEPTION) <= 0) {
        send_to_char("You have no sense of the astral plane.\r\n", ch);
        return FALSE;
      }
      break;
    default:
      if (!access_level(ch, LVL_ADMIN)) {
        send_to_char("You have no sense of the astral plane.\r\n", ch);
        return FALSE;
      } else {
        send_to_char("You abuse your staff powers to open your third eye.\r\n", ch);
      }
      break;
  }

  PLR_FLAGS(ch).SetBit(PLR_PERCEIVE);
  send_to_char("Your physical body seems distant, as the astral plane slides into view.\r\n", ch);
  return TRUE;
}

int get_focus_bond_cost(struct obj_data *obj) {
  if (GET_OBJ_TYPE(obj) == ITEM_WEAPON) {
    return (3 + GET_WEAPON_REACH(obj)) * GET_WEAPON_FOCUS_RATING(obj);
  }

  else if (GET_OBJ_TYPE(obj) == ITEM_FOCUS) {
    switch (GET_FOCUS_TYPE(obj)) {
      case FOCI_EXPENDABLE:
        return 0;
      case FOCI_SPEC_SPELL:
      case FOCI_SUSTAINED:
        return GET_FOCUS_FORCE(obj);
      case FOCI_SPIRIT:
        return GET_FOCUS_FORCE(obj) * 2;
      case FOCI_SPELL_CAT:
      case FOCI_SPELL_DEFENSE:
        return GET_FOCUS_FORCE(obj) * 3;
      case FOCI_POWER:
        return GET_FOCUS_FORCE(obj) * 5;
      default:
        return 10000000;
    }
  }

  else {
    mudlog("SYSERR: Received non-focus to get_focus_bond_cost!", NULL, LOG_SYSLOG, TRUE);
    return 1000000;
  }
}

struct veh_data *get_veh_controlled_by_char(struct char_data *ch) {
  if (!ch) {
    mudlog("SYSERR: NULL char received to get_veh_controlled_by_char!", ch, LOG_SYSLOG, TRUE);
    return NULL;
  }

  // You must be flagged as controlling a vehicle to continue.
  if (!PLR_FLAGGED(ch, PLR_REMOTE) && !AFF_FLAGGED(ch, AFF_RIG) && !AFF_FLAGGED(ch, AFF_PILOT))
    return NULL;

  if (ch->char_specials.rigging)
    return ch->char_specials.rigging;

  return ch->in_veh;
}

struct obj_data *find_best_active_docwagon_modulator(struct char_data *ch) {
  struct obj_data *docwagon = NULL;

  for (int i = 0; i < NUM_WEARS; i++) {
    struct obj_data *eq = GET_EQ(ch, i);

    if (eq && GET_OBJ_TYPE(eq) == ITEM_DOCWAGON && GET_DOCWAGON_BONDED_IDNUM(eq) == GET_IDNUM(ch)) {
      if (docwagon == NULL || GET_DOCWAGON_CONTRACT_GRADE(eq) > GET_DOCWAGON_CONTRACT_GRADE(docwagon)) {
        docwagon = eq;
      }
    }
  }

  return docwagon;
}

bool char_is_in_social_room(struct char_data *ch) {
  if (!ch) {
    mudlog("SYSERR: NULL character passed to char_is_in_social_room()!", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  return ch->in_room && ROOM_FLAGGED(ch->in_room, ROOM_ENCOURAGE_CONGREGATION);
}

bool is_custom_ware(struct obj_data *ware) {
  if (!ware) {
    mudlog("SYSERR: Received NULL ware to is_custom_ware()!", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  switch (GET_OBJ_TYPE(ware)) {
    case ITEM_CYBERWARE:
      return GET_CYBERWARE_TYPE(ware) == CYB_CUSTOM_NERPS;
    case ITEM_BIOWARE:
      return GET_BIOWARE_TYPE(ware) == BIO_CUSTOM_NERPS;
    default:
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Got non-'ware obj %s (%ld) to is_custom_ware!", GET_OBJ_NAME(ware), GET_OBJ_VNUM(ware));
      return FALSE;
  }
}

extern int max_ability(int power_idx);
void render_targets_abilities_to_viewer(struct char_data *viewer, struct char_data *vict) {
  if (IS_NPC(vict)) {
    send_to_char("NPCs don't have abilities.\r\n", viewer);
    return;
  }

  if (GET_TRADITION(vict) != TRAD_ADEPT) {
    send_to_char(viewer, "%s isn't an adept.\r\n", GET_CHAR_NAME(vict));
    return;
  }

  bool screenreader = PRF_FLAGGED(viewer, PRF_SCREENREADER);
  bool already_printed = FALSE;

  for (int power_idx = 1; power_idx < ADEPT_NUMPOWER; power_idx++) {
    if (GET_POWER_TOTAL(vict, power_idx) > 0) {
      // Send the intro string.
      if (screenreader) {
        if (!already_printed) {
          send_to_char(viewer, "%s %s the following abilities:\r\n",
                       vict == viewer ? "You" : GET_CHAR_NAME(vict),
                       vict == viewer ? "have" : "has");
        }
        send_to_char(viewer, "%s (%0.2f PP): %s",
                     adept_powers[power_idx],
                     ((float) ability_cost(power_idx, 1)) / 100,
                     GET_POWER_ACT(vict, power_idx) > 0 ? "active" : "");
      } else {
        if (!already_printed) {
          send_to_char("^WPP      Ability              Level (Active)^n\r\n", viewer);
        }
        send_to_char(viewer, "^c%0.2f^n    %-20s", ((float)ability_cost(power_idx, 1))/100, adept_powers[power_idx]);
      }

      already_printed = TRUE;

      if (max_ability(power_idx) > 1) {
        if (power_idx == ADEPT_KILLING_HANDS) {
          if (screenreader) {
            send_to_char(viewer, " (current level %s, max level %s)",
                         GET_WOUND_NAME(GET_POWER_ACT(vict, power_idx)),
                         GET_WOUND_NAME(GET_POWER_TOTAL(vict, power_idx)));
          } else {
            send_to_char(viewer, " %s", GET_WOUND_NAME(GET_POWER_TOTAL(vict, power_idx)));
            if (GET_POWER_ACT(vict, power_idx))
              send_to_char(viewer, " ^Y(%s)^n", GET_WOUND_NAME(GET_POWER_ACT(vict, power_idx)));
          }
        } else {
          if (screenreader) {
            send_to_char(viewer, " (current level %d, max level %d)",
                         GET_POWER_ACT(vict, power_idx),
                         GET_POWER_TOTAL(vict, power_idx));
          } else {
            send_to_char(viewer, " +%d", GET_POWER_TOTAL(vict, power_idx));
            if (GET_POWER_ACT(vict, power_idx))
              send_to_char(viewer, " ^Y(%d)^n", GET_POWER_ACT(vict, power_idx));
          }
        }
      } else if (!screenreader && GET_POWER_ACT(vict, power_idx)) {
        send_to_char(viewer, " ^Y(active)^n");
      }
      send_to_char("\r\n", viewer);
    }
  }

  if (!already_printed) {
    send_to_char(viewer, "%s %s no abilities.\r\n",
                 vict == viewer ? "You" : GET_CHAR_NAME(vict),
                 vict == viewer ? "have" : "has");
  }

  send_to_char(viewer, "\r\n%s %s ^c%.2f^n powerpoints remaining and ^c%.2f^n points of powers activated.\r\n",
               vict == viewer ? "You" : GET_CHAR_NAME(vict),
               vict == viewer ? "have" : "has",
               (float) GET_PP(vict) / 100,
               (float) GET_POWER_POINTS(vict) / 100);

#ifndef DIES_IRAE
  // In Dies Irae, the addpoint command is not available.
  int unpurchased_points = (int)(GET_TKE(vict) / 50) + 1 - vict->points.extrapp;
  send_to_char(viewer, "%s %s ^c%d^n point%s of ^WADDPOINT^n available.\r\n",
               vict == viewer ? "You" : GET_CHAR_NAME(vict),
               vict == viewer ? "have" : "has",
               unpurchased_points,
               unpurchased_points == 1 ? "" : "s");
#endif
}

bool keyword_appears_in_obj(const char *keyword, struct obj_data *obj, bool search_keywords, bool search_name, bool search_desc) {
  if (!keyword || !*keyword) {
    return FALSE;
  }

  if (!obj) {
    mudlog("SYSERR: Received NULL obj to keyword_appears_in_obj()!", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (search_keywords && isname(keyword, obj->text.keywords))
    return TRUE;

  if (search_name) {
    if (isname(keyword, get_string_after_color_code_removal(obj->text.name, NULL)))
      return TRUE;
    if (obj->restring && isname(keyword, get_string_after_color_code_removal(obj->restring, NULL)))
      return TRUE;
  }

  if (search_desc) {
    if (isname(keyword, get_string_after_color_code_removal(obj->text.room_desc, NULL)))
      return TRUE;
    if (isname(keyword, get_string_after_color_code_removal(obj->text.look_desc, NULL)))
      return TRUE;
  }

  return FALSE;
}

bool keyword_appears_in_char(const char *keyword, struct char_data *ch, bool search_keywords, bool search_name, bool search_desc) {
  if (!keyword || !*keyword) {
    return FALSE;
  }

  if (!ch) {
    mudlog("SYSERR: Received NULL ch to keyword_appears_in_char()!", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (search_keywords && isname(keyword, ch->player.physical_text.keywords))
    return TRUE;

  if (search_name) {
    if (isname(keyword, get_string_after_color_code_removal(ch->player.physical_text.name, NULL)))
      return TRUE;
    if (ch->player.char_name && isname(keyword, get_string_after_color_code_removal(ch->player.char_name, NULL)))
      return TRUE;
  }

  if (search_desc) {
    if (isname(keyword, get_string_after_color_code_removal(ch->player.physical_text.room_desc, NULL)))
      return TRUE;
    if (isname(keyword, get_string_after_color_code_removal(ch->player.physical_text.look_desc, NULL)))
      return TRUE;

    // Since this is not a common use case, we use full keyword matching here to prevent mixups like 'hu' from 'hunter' matching 'human'
    if (!str_cmp(keyword, pc_race_types[(int) GET_RACE(ch)]))
      return TRUE;
    if (!str_cmp(keyword, genders[(int) GET_SEX(ch)]))
      return TRUE;
  }

  return FALSE;
}

bool keyword_appears_in_veh(const char *keyword, struct veh_data *veh, bool search_name, bool search_desc) {
  if (!keyword || !*keyword) {
    return FALSE;
  }

  if (!veh) {
    mudlog("SYSERR: Received NULL veh to keyword_appears_in_veh()!", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (search_name) {
    if (isname(keyword, get_string_after_color_code_removal(veh->name, NULL)))
      return TRUE;
    if (veh->restring && isname(keyword, get_string_after_color_code_removal(veh->restring, NULL)))
      return TRUE;
    if (isname(keyword, get_string_after_color_code_removal(veh->description, NULL)))
      return TRUE;
  }

  if (search_desc) {
    if (isname(keyword, get_string_after_color_code_removal(veh->long_description, NULL)))
      return TRUE;
    if (veh->restring_long && isname(keyword, get_string_after_color_code_removal(veh->restring_long, NULL)))
      return TRUE;
  }

  return FALSE;
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
