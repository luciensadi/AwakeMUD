#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <mysql/mysql.h>

#if defined(WIN32) && !defined(__CYGWIN__)
#define strcasecmp(x, y) _stricmp(x,y)
#endif

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "newmagic.h"
#include "house.h"
#include "time.h"
#include "constants.h"
#include "newdb.h"
#include "quest.h"
#include "mail.h"
#include "pocketsec.h"
#include "limits.h"

/*   external vars  */
extern struct time_info_data time_info;
extern struct weather_data weather_info;
extern int return_general(int skill_num);
extern train_t trainers[];
extern teach_t teachers[];
extern teach_t metamagict[];
extern adept_t adepts[];
extern spell_t spelltrainers[];
/* extern functions */
void add_follower(struct char_data * ch, struct char_data * leader);
extern void docwagon(struct char_data *ch);
extern void die(struct char_data * ch);
extern void affect_total(struct char_data * ch);
extern struct obj_data *get_first_credstick(struct char_data *ch, const char *arg);
extern struct char_data *give_find_vict(struct char_data * ch, char *arg);
extern int perform_give(struct char_data * ch, struct char_data * vict, struct obj_data * obj);
extern int belongs_to(struct char_data *ch, struct obj_data *obj);
extern void reset_zone(int zone, int reboot);
extern int find_weapon_range(struct char_data *ch, struct obj_data *weapon);
extern int find_sight(struct char_data *ch);
extern void check_quest_kill(struct char_data *ch, struct char_data *victim);
extern void wire_nuyen(struct char_data *ch, struct char_data *target, int amount, bool isfile);
bool memory(struct char_data *ch, struct char_data *vict);
extern MYSQL *mysql;
extern int mysql_wrapper(MYSQL *mysql, const char *query);

ACMD_CONST(do_say);
ACMD(do_echo);
ACMD_CONST(do_time);
ACMD_CONST(do_gen_door);

struct social_type
{
  char *cmd;
  int next_line;
};

// object defines
#define ROOTS_PAGER  13
#define SEATAC_TICKET  500
#define SEATTLE_TICKET  501
#define PHRODOS_PAGER  15005

// mob defines
#define TIM_ENCHANTER  1002
#define TROLL_BOUNCER  2112
#define MAGE_BOUNCER  2113
#define PARK_MUGGER  4003

// room defines
#define PHRODOS_RECEPTION       18
#define ROOTS_RECEPTION        20
#define MANSION_GATE            4059
#define YUKIYA_OFFICE           17157

// misc defines
#define LIBRARY_SKILL		3
#define NEWBIE_SKILL		6
#define NORMAL_MAX_SKILL	8
#define LEARNED_LEVEL		12
#define RENT_FACTOR 1

int fixers_need_save;

/* ********************************************************************
*  Special procedures for mobiles                                     *
******************************************************************** */

char *how_good(int percent)
{
  static char buf[256];

  if (percent < 0)
    strcpy(buf, " (uh oh! you have a negative skill, please report!)");
  else if (percent == 0)
    strcpy(buf, " (not learned)");
  else if (percent == 1)
    strcpy(buf, " (awful)");
  else if (percent == 2)
    strcpy(buf, " (sloppy)");
  else if (percent == 3)
    strcpy(buf, " (average)");
  else if (percent == 4)
    strcpy(buf, " (above average)");
  else if (percent == 5)
    strcpy(buf, " (good)");
  else if (percent == 6)
    strcpy(buf, " (very good)");
  else if (percent == 7)
    strcpy(buf, " (distinguished)");
  else if (percent == 8)
    strcpy(buf, " (excellent)");
  else if (percent == 9)
    strcpy(buf, " (superb)");
  else if (percent == 10)
    strcpy(buf, " (learned)");
  else if (percent <= 20)
    strcpy(buf, " (amazing)");
  else if (percent <= 30)
    strcpy(buf, " (incredible)");
  else if (percent <= 40)
    strcpy(buf, " (unbelievable)");
  else if (percent <= 50)
    strcpy(buf, " (ludicrous)");
  else
    strcpy(buf, " (godly)");

  return (buf);
}

int max_ability(int i)
{
  switch (i) {
  case ADEPT_PERCEPTION:
  case ADEPT_LOW_LIGHT:
  case ADEPT_THERMO:
  case ADEPT_IMAGE_MAG:
  case ADEPT_MISSILE_PARRY:
  case ADEPT_BLIND_FIGHTING:
  case ADEPT_DISTANCE_STRIKE:
  case ADEPT_NERVE_STRIKE:
  case ADEPT_MISSILE_MASTERY:
  case ADEPT_QUICK_STRIKE:
  case ADEPT_SMASHING_BLOW:
  case ADEPT_SUSTENANCE:
  case ADEPT_PAINRELIEF:
  case ADEPT_MOTIONSENSE:
  case ADEPT_LIVINGFOCUS:
  case ADEPT_LINGUISTICS:
  case ADEPT_EMPATHICHEAL:
    return 1;
  case ADEPT_PENETRATINGSTRIKE:
  case ADEPT_COMBAT_SENSE:
  case ADEPT_REFLEXES:
  case ADEPT_KINESICS:
    return 3;
  case ADEPT_KILLING_HANDS:
    return 4;
  case ADEPT_PAIN_RESISTANCE:
    return 10;
  default:
    return 100;
  }
}

int ability_cost(int abil, int level)
{
  if (!level)
    return 0;

  switch(abil) {
  case ADEPT_BOOST_STR:
  case ADEPT_BOOST_QUI:
  case ADEPT_BOOST_BOD:
  case ADEPT_FREEFALL:
  case ADEPT_SPELL_SHROUD:
  case ADEPT_TEMPERATURE_TOLERANCE:
  case ADEPT_TRUE_SIGHT:
  case ADEPT_LOW_LIGHT:
  case ADEPT_THERMO:
  case ADEPT_IMAGE_MAG:
  case ADEPT_LINGUISTICS:
  case ADEPT_SUSTENANCE:
    return 25;
  case ADEPT_PAIN_RESISTANCE:
  case ADEPT_IMPROVED_PERCEPT:
  case ADEPT_HEALING:
  case ADEPT_MYSTIC_ARMOUR:
  case ADEPT_BLIND_FIGHTING:
  case ADEPT_IRONWILL:
  case ADEPT_IMPROVED_BOD:
  case ADEPT_IMPROVED_QUI:
  case ADEPT_IMPROVED_STR:
  case ADEPT_AIDSPELL:
  case ADEPT_MOTIONSENSE:
  case ADEPT_SIDESTEP:
    return 50;
  case ADEPT_COMBAT_SENSE:
  case ADEPT_MAGIC_RESISTANCE:
  case ADEPT_MISSILE_PARRY:
  case ADEPT_MISSILE_MASTERY:
  case ADEPT_NERVE_STRIKE:
  case ADEPT_SMASHING_BLOW:
  case ADEPT_EMPATHICHEAL:
  case ADEPT_KINESICS:
  case ADEPT_LIVINGFOCUS:
  case ADEPT_PAINRELIEF:
    return 100;
  case ADEPT_PERCEPTION:
  case ADEPT_DISTANCE_STRIKE:
    return 200;
  case ADEPT_QUICK_STRIKE:
    return 300;
  case ADEPT_REFLEXES:
    switch(level) {
    case 1:
      return 200;
    case 2:
      return 100;
    case 3:
      return 200;
    }
  case ADEPT_KILLING_HANDS:
    if (level < 3)
      return 50;
    else if (level == 3)
      return 100;
    else
      return 200;
  case ADEPT_PENETRATINGSTRIKE:
    return 75;
  }
  return 0;
}

void attack_random_player(struct char_data *mob, struct char_data *boss)
{
  struct char_data *vict;
  int num = 0;

  for (vict = world[mob->in_room].people; vict; vict = vict->next_in_room)
    if (!IS_NPC(vict) && FIGHTING(vict) == boss)
      num++;

  num = MAX(1, num - 1);

  for (vict = world[mob->in_room].people; vict; vict = vict->next_in_room)
    if (!IS_NPC(vict) && FIGHTING(vict) == boss && !number(0, num))
    {
      set_fighting(mob, vict);
      return;
    }
}

int summon_mob(struct char_data *ch, int vnum, int number)
{
  struct char_data *tch;
  int num = 0, rnum, total = 0;

  if (!ch || !FIGHTING(ch) || (rnum = real_mobile(vnum)) < 0)
    return 0;

  for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room)
    if (GET_MOB_VNUM(tch) == vnum && GET_POS(tch) > POS_SLEEPING &&
        !FIGHTING(tch))
    {
      num++;
      total++;
      if (!MOB_FLAGGED(tch, MOB_AGGRESSIVE))
        set_fighting(tch, FIGHTING(ch));
      else
        attack_random_player(tch, ch);
    }

  number = MIN(number, mob_index[rnum].number - num);

  // since it is necessary, find and summon the mob(s)
  for (tch = character_list; tch && number > 0; tch = tch->next)
    if (GET_MOB_VNUM(tch) == vnum && ch->in_room != tch->in_room &&
        !FIGHTING(tch) && GET_POS(tch) > POS_SLEEPING)
    {
      number--;
      total++;
      act("You have been summoned by $N.", FALSE, tch, 0, ch, TO_CHAR);
      act("$n suddenly vanishes.", TRUE, tch, 0, 0, TO_ROOM);
      char_from_room(tch);
      char_to_room(tch, ch->in_room);
      act("$n has arrived.", TRUE, tch, 0, 0, TO_ROOM);
      if (!MOB_FLAGGED(tch, MOB_AGGRESSIVE))
        set_fighting(tch, FIGHTING(ch));
      else
        attack_random_player(tch, ch);
    }

  return total;
}

int load_mob(struct char_data *ch, int vnum, int number, char *message)
{
  struct char_data *mob;
  int i, rnum, total = 0;

  if (!ch || !FIGHTING(ch) || (rnum = real_mobile(vnum)) < 0)
    return 0;

  for (i = 0; i < number; i++)
    if ((mob = read_mobile(rnum, REAL)))
    {
      total++;
      char_to_room(mob, ch->in_room);
      act(message, TRUE, mob, 0, 0, TO_ROOM);
      if (!MOB_FLAGGED(mob, MOB_AGGRESSIVE))
        set_fighting(mob, FIGHTING(ch));
      else
        attack_random_player(mob, ch);
    }

  return total;
}

int get_skill_price(struct char_data *ch, int i)
{
  if (!GET_SKILL(ch, i))
    return 1;
  if (GET_SKILL(ch, i) + 1 <= GET_REAL_ATT(ch, skills[i].attribute))
  {
    if (skills[i].type)
      return GET_SKILL(ch, i) + 1;
    else
      return (int)((GET_SKILL(ch, i) + 1) * 1.5);
  } else if (GET_SKILL(ch, i) + 1 <= GET_REAL_ATT(ch, skills[i].attribute) * 2)
  {
    if (skills[i].type)
      return (int)((GET_SKILL(ch, i) + 1) * 1.5);
    else
      return (int)((GET_SKILL(ch, i) + 1) * 2);
  } else
  {
    if (skills[i].type)
      return (GET_SKILL(ch, i) + 1) * 2;
    else
      return (int)((GET_SKILL(ch, i) + 1) * 2.5);
  }
}

SPECIAL(metamagic_teacher)
{
  struct char_data *master = (struct char_data *) me;
  int i = 0, x = 0, suc, ind;
  if (IS_NPC(ch) || !CMD_IS("train") || !CAN_SEE(master, ch) || FIGHTING(ch) ||
      GET_POS(ch) < POS_SITTING || GET_TRADITION(ch) == TRAD_MUNDANE)
    return FALSE;

  skip_spaces(&argument);

  for (ind = 0; metamagict[ind].vnum != 0; ind++)
    if (metamagict[ind].vnum == GET_MOB_VNUM(master))
      break;

  if (metamagict[ind].vnum != GET_MOB_VNUM(master))
    return FALSE;

  if (!*argument) {
    send_to_char("Your teacher can teach you the following techniques: \r\n", ch);
    for (; i < NUM_TEACHER_SKILLS; i++)
      if (metamagict[ind].s[i])
        send_to_char(ch, "  %s\r\n", metamagic[metamagict[ind].s[i]]);
    return TRUE;
  }
  for (; i < META_MAX; i++)
    if (is_abbrev(argument, metamagic[i]))
      break;

  if (i == META_MAX) {
    send_to_char("What metamagic technique do you wish to train?\r\n", ch);
    return TRUE;
  }

  if (GET_METAMAGIC(ch, i) == 2 || GET_METAMAGIC(ch, i) == 4) {
    send_to_char("You already know that metamagic technique.\r\n", ch);
    return TRUE;
  }

  if (!GET_METAMAGIC(ch, i)) {
    send_to_char("You aren't close enough to the astral plane to learn that.\r\n", ch);
    return TRUE;
  }
  if (GET_GRADE(ch) >= (GET_MAG(master) / 100) - 6) {
    send_to_char("That teacher is not powerful enough to teach you that technique.\r\n", ch);
    return TRUE;
  }

  for (; x < NUM_TEACHER_SKILLS; x++)
    if (metamagict[ind].s[x] == i)
      break;

  if (x == NUM_TEACHER_SKILLS) {
    send_to_char("You can't train that here.\r\n", ch);
    return TRUE;
  }

  if ((suc = success_test(GET_MAG(ch) / 100, 14 - (GET_MAG(master) / 100))) < 1) {
    send_to_char("Try as you might, you fail to understand how to learn that technique.\r\n", ch);
    return TRUE;
  }
  int cost = (int)((GET_MAG(ch) / 100) * 1000 * (14 / suc));
  if (GET_NUYEN(ch) < cost) {
    send_to_char(ch, "You don't have the %d nuyen required to learn that.\r\n", cost);
    return TRUE;
  }
  GET_NUYEN(ch) -= cost;
  send_to_char(ch, "%s runs through how to use %s with you, you walk away knowing exactly how to use it.\r\n", GET_NAME(master), metamagic[i]);
  GET_METAMAGIC(ch, i)++;
  return TRUE;
}

SPECIAL(teacher)
{
  struct char_data *master = (struct char_data *) me;
  int i, ind, max, skill_num;

  if (IS_NPC(ch) || !CMD_IS("practice") || !CAN_SEE(master, ch) || FIGHTING(ch) ||
      GET_POS(ch) < POS_STANDING)
    return FALSE;

  skip_spaces(&argument);

  for (ind = 0; teachers[ind].vnum != 0; ind++)
    if (teachers[ind].vnum == GET_MOB_VNUM(master))
      break;

  if (teachers[ind].vnum != GET_MOB_VNUM(master))
    return FALSE;

  if (teachers[ind].type != NEWBIE && PLR_FLAGGED(ch, PLR_NEWBIE)) {
    do_say(master, "You're not quite ready for that yet!", 0, 0);
    return TRUE;
  }

  if (!PLR_FLAGGED(ch, PLR_NEWBIE) && GET_SKILL_POINTS(ch) != 0)
    GET_SKILL_POINTS(ch) = 0;

  if (!AWAKE(ch)) {
    send_to_char("You must be conscious to practice.\r\n", ch);
    return TRUE;
  }
  if (teachers[ind].type == NEWBIE)
    max = NEWBIE_SKILL;
  else if (teachers[ind].type == AMATEUR)
    max = NORMAL_MAX_SKILL;
  else if (teachers[ind].type == ADVANCED)
    max = LEARNED_LEVEL;
  else if (teachers[ind].type == GODLY)
    max = 100;
  else if (teachers[ind].type == LIBRARY)
    max = LIBRARY_SKILL;
  else
    return FALSE;

  if (!*argument) {
    sprintf(buf, "Your teacher can teach you the following:\r\n");
    for (int i = 0; i < NUM_TEACHER_SKILLS; i++) {
      if (teachers[ind].s[i] > 0) {
        if (GET_SKILL_POINTS(ch) > 0)
          sprintf(buf, "%s  %s\r\n", buf, skills[teachers[ind].s[i]].name);
        else if (GET_SKILL(ch, teachers[ind].s[i]) < max && !ch->char_specials.saved.skills[teachers[ind].s[i]][1])
          sprintf(buf, "%s  %-24s (%d karma %d nuyen)\r\n", buf, skills[teachers[ind].s[i]].name, get_skill_price(ch, teachers[ind].s[i]),
                  MAX(1000, (GET_SKILL(ch, teachers[ind].s[i]) * 5000)));
      }
    }
    if (GET_SKILL_POINTS(ch) > 0)
      sprintf(buf, "%s\r\nYou have %d points to use for skills.\r\n", buf,
              GET_SKILL_POINTS(ch));
    else
      sprintf(buf, "%s\r\nYou have %0.2f karma to use for skills.\r\n", buf,
              ((float)GET_KARMA(ch) / 100));
    send_to_char(buf, ch);
    return TRUE;
  }

  if ((teachers[ind].type == NEWBIE && GET_SKILL_POINTS(ch) <= 0 &&
       GET_KARMA(ch) <= 0) || (teachers[ind].type != NEWBIE && GET_KARMA(ch) <= 0)) {
    send_to_char("You do not seem to be able to practice now.\r\n", ch);
    return TRUE;
  }

  skill_num = find_skill_num(argument);

  if (skill_num < 0) {
    send_to_char("Your teacher doesn't seem to know anything about that subject.\r\n", ch);
    return TRUE;
  }
  if (GET_SKILL(ch, skill_num) && !REAL_SKILL(ch, skill_num)) {
    send_to_char("You can't train a skill you currently have a skillsoft for.\r\n", ch);
    return TRUE;
  }
  if ((skill_num == SKILL_CENTERING || skill_num == SKILL_ENCHANTING) &&
      GET_TRADITION(ch) == TRAD_MUNDANE) {
    send_to_char("Without access to the astral plane you can't even begin to fathom the basics of that skill.\r\n", ch);
    return TRUE;
  }

  i = 0;
  for (i = 0; i < NUM_TEACHER_SKILLS; i++)
    if (skill_num == teachers[ind].s[i])
      break;
  if (i >= NUM_TEACHER_SKILLS) {
    send_to_char("Your teacher doesn't seem to know about that subject.\r\n", ch);
    return TRUE;
  }

  if (GET_SKILL(ch, skill_num) >= max) {
    if (max == LIBRARY_SKILL)
      send_to_char("You can't find any books that tell you things you don't already know.\r\n", ch);
    else {
      sprintf(arg, "%s You already know more than I can teach you in that area.", GET_CHAR_NAME(ch));
      do_say(master, arg, 0, SCMD_SAYTO);
    }
    return TRUE;
  }

  if (GET_KARMA(ch) < get_skill_price(ch, skill_num) * 100 &&
      GET_SKILL_POINTS(ch) <= 0) {
    send_to_char("You don't have enough karma to improve that skill.\r\n", ch);
    return TRUE;
  }
  if (GET_NUYEN(ch) < MAX(1000, (GET_SKILL(ch, skill_num) * 5000)) &&
      teachers[ind].type != NEWBIE) {
    send_to_char(ch, "You can't afford the %d nuyen practice fee!\r\n",
                 MAX(1000, (GET_SKILL(ch, skill_num) * 5000)));
    return TRUE;
  }
  if (teachers[ind].type != NEWBIE)
    GET_NUYEN(ch) -= MAX(1000, (GET_SKILL(ch, skill_num) * 5000));
  if (GET_SKILL_POINTS(ch) > 0)
    GET_SKILL_POINTS(ch)--;
  else
    GET_KARMA(ch) -= get_skill_price(ch, skill_num) * 100;

  send_to_char(teachers[ind].msg, ch);
  SET_SKILL(ch, skill_num, REAL_SKILL(ch, skill_num) + 1);
  if (GET_SKILL(ch, skill_num) >= max)
    send_to_char("You have learnt all you can here.\r\n", ch);

  return TRUE;
}

bool trainable_attribute_is_maximized(struct char_data *ch, int attribute) {
  switch (attribute) {
    case ATT_BOD:
      return (GET_REAL_BOD(ch) + GET_PERM_BOD_LOSS(ch)) >= racial_limits[(int)GET_RACE(ch)][0][0];
    case ATT_QUI:
      return GET_REAL_QUI(ch) >= racial_limits[(int)GET_RACE(ch)][0][1];
    case ATT_STR:
      return GET_REAL_STR(ch) >= racial_limits[(int)GET_RACE(ch)][0][2];
    case ATT_CHA:
      return GET_REAL_CHA(ch) >= racial_limits[(int)GET_RACE(ch)][0][3];
    case ATT_INT:
      return GET_REAL_INT(ch) >= racial_limits[(int)GET_RACE(ch)][0][4];
    case ATT_WIL:
      return GET_REAL_WIL(ch) >= racial_limits[(int)GET_RACE(ch)][0][5];
  }
  return true;
}

SPECIAL(trainer)
{
  struct char_data *trainer = (struct char_data *) me;
  int i, ind, first = 1;

  if (!CMD_IS("train") || IS_NPC(ch) || !CAN_SEE(trainer, ch) || FIGHTING(ch) ||
      GET_POS(ch) < POS_STANDING)
    return FALSE;

  skip_spaces(&argument);

  for (ind = 0; trainers[ind].vnum != 0; ind++)
    if (trainers[ind].vnum == GET_MOB_VNUM(trainer))
      break;

  if (trainers[ind].vnum != GET_MOB_VNUM(trainer))
    return FALSE;

  if (!trainers[ind].is_newbie && PLR_FLAGGED(ch, PLR_NEWBIE)) {
    sprintf(arg, "%s You are not quite ready yet!", GET_CHAR_NAME(ch));
    do_say(trainer, arg, 0, SCMD_SAYTO);
    return TRUE;
  }

  if (!PLR_FLAGGED(ch, PLR_NEWBIE) && GET_ATT_POINTS(ch) != 0)
    GET_ATT_POINTS(ch) = 0;

  if (!*argument) {
    if (GET_ATT_POINTS(ch) > 0) {
      send_to_char(ch, "You have %d points to distribute.  You can train",
                   GET_ATT_POINTS(ch));
      bool found_something_to_train = FALSE;
      for (i = 0; (1 << i) <= TWIL; i++)
        if (IS_SET(trainers[ind].attribs, (1 << i))) {
          switch (1 << i) {
            case TBOD:
              if (!trainable_attribute_is_maximized(ch, ATT_BOD)) {
                send_to_char(ch, "%s bod", first ? "" : ",");
                found_something_to_train = TRUE;
                first = 0;
              }
              break;
            case TQUI:
              if (!trainable_attribute_is_maximized(ch, ATT_QUI)) {
                send_to_char(ch, "%s qui", first ? "" : ",");
                found_something_to_train = TRUE;
                first = 0;
              }
              break;
            case TSTR:
              if (!trainable_attribute_is_maximized(ch, ATT_STR)) {
                send_to_char(ch, "%s str", first ? "" : ",");
                found_something_to_train = TRUE;
                first = 0;
              }
              break;
            case TCHA:
              if (!trainable_attribute_is_maximized(ch, ATT_CHA)) {
                send_to_char(ch, "%s cha", first ? "" : ",");
                found_something_to_train = TRUE;
                first = 0;
              }
              break;
            case TINT:
              if (!trainable_attribute_is_maximized(ch, ATT_INT)) {
                send_to_char(ch, "%s int", first ? "" : ",");
                found_something_to_train = TRUE;
                first = 0;
              }
              break;
            case TWIL:
              if (!trainable_attribute_is_maximized(ch, ATT_WIL)) {
                send_to_char(ch, "%s wil", first ? "" : ",");
                found_something_to_train = TRUE;
                first = 0;
              }
              break;
          }
        }
      if (found_something_to_train) {
        send_to_char(".\r\n", ch);
      } else {
        send_to_char(" nothing-- you're already at your maximums!\r\n", ch);
      }
    } else {
      send_to_char(ch, "You have %0.2f karma points.  You can train",
                   (float)GET_KARMA(ch) / 100);
      for (i = 0; (1 << i) <= TWIL; i++)
        if (IS_SET(trainers[ind].attribs, (1 << i))) {
          switch (1 << i) {
          case TBOD:
            if (GET_REAL_BOD(ch) + GET_PERM_BOD_LOSS(ch) < racial_limits[(int)GET_RACE(ch)][0][0])
              send_to_char(ch, "%s bod (%d karma %d nuyen)", first ? "" : ",", 2*(GET_REAL_BOD(ch)+1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_BOD)),
                           (GET_REAL_BOD(ch) + 1+GET_POWER(ch, ADEPT_IMPROVED_BOD)) * 1000);
            break;
          case TQUI:
            if (GET_REAL_QUI(ch) < racial_limits[(int)GET_RACE(ch)][0][1])
              send_to_char(ch, "%s qui (%d karma %d nuyen)", first ? "" : ",", 2*(GET_REAL_QUI(ch)+1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_QUI)),
                           (GET_REAL_QUI(ch) + 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_QUI)) * 1000);
            break;
          case TSTR:
            if (GET_REAL_STR(ch) < racial_limits[(int)GET_RACE(ch)][0][2])
              send_to_char(ch, "%s str (%d karma %d nuyen)", first ? "" : ",", 2*(GET_REAL_STR(ch)+1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_STR)),
                           (GET_REAL_STR(ch) + 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_STR)) * 1000);
            break;
          case TCHA:
            if (GET_REAL_CHA(ch) < racial_limits[(int)GET_RACE(ch)][0][3])
              send_to_char(ch, "%s cha (%d karma %d nuyen)", first ? "" : ",", 2*(GET_REAL_CHA(ch)+1),
                           (GET_REAL_CHA(ch) + 1) * 1000);
            break;
          case TINT:
            if (GET_REAL_INT(ch) < racial_limits[(int)GET_RACE(ch)][0][4])
              send_to_char(ch, "%s int (%d karma %d nuyen)", first ? "" : ",", 2*(GET_REAL_INT(ch)+1),
                           (GET_REAL_INT(ch) + 1) * 1000);
            break;
          case TWIL:
            if (GET_REAL_WIL(ch) < racial_limits[(int)GET_RACE(ch)][0][5])
              send_to_char(ch, "%s wil (%d karma %d nuyen)", first ? "" : ",", 2*(GET_REAL_WIL(ch)+1),
                           (GET_REAL_WIL(ch) + 1) * 1000);
            break;
          }
          first = 0;
        }
      send_to_char(".\r\n", ch);
    }
    return 1;
  }

  if (!str_cmp(argument, "bod") && IS_SET(trainers[ind].attribs, TBOD)) {
    if (GET_REAL_BOD(ch) + GET_PERM_BOD_LOSS(ch) >= racial_limits[(int)GET_RACE(ch)][0][0]) {
      send_to_char("Your body attribute is at its maximum.\r\n", ch);
      return 1;
    }
    if (((2*(GET_REAL_BOD(ch) + 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_BOD))) > (int)(GET_KARMA(ch) / 100)) &&
        GET_ATT_POINTS(ch) <= 0) {
      send_to_char("You don't have enough karma to raise your body attribute.\r\n", ch);
      return 1;
    }
    if (!trainers[ind].is_newbie) {
      if (GET_NUYEN(ch) < ((GET_REAL_BOD(ch) + 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_BOD)) * 1000)) {
        sprintf(arg, "%s The charge for that is %d nuyen, which I see you don't have.",
                GET_CHAR_NAME(ch), ((GET_REAL_BOD(ch) + 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_BOD)) * 1000));
        do_say(trainer, arg, 0, SCMD_SAYTO);
        return TRUE;
      }
      GET_NUYEN(ch) -= (GET_REAL_BOD(ch) + 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_BOD)) * 1000;
    }
    if (GET_ATT_POINTS(ch) > 0)
      GET_ATT_POINTS(ch)--;
    else
      GET_KARMA(ch) -= (GET_REAL_BOD(ch) + 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_BOD)) * 200;
    GET_REAL_BOD(ch)++;
    affect_total(ch);
    send_to_char(ch, "Your previous weeks worth of hard work increase your Bod "
                 "to %d.\r\n", GET_REAL_BOD(ch));
  } else if (!str_cmp(argument, "qui") && IS_SET(trainers[ind].attribs, TQUI)) {
    if (GET_REAL_QUI(ch) >= racial_limits[(int)GET_RACE(ch)][0][1]) {
      send_to_char("Your quickness attribute is at its maximum.\r\n", ch);
      return 1;
    }
    if (((2*(GET_REAL_QUI(ch) + 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_QUI))) > (int)(GET_KARMA(ch) / 100)) &&
        GET_ATT_POINTS(ch) < 1) {
      send_to_char("You don't have enough karma to raise your quickness attribute.\r\n", ch);
      return 1;
    }
    if (!trainers[ind].is_newbie) {
      if (GET_NUYEN(ch) < ((GET_REAL_QUI(ch) + 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_QUI)) * 1000)) {
        sprintf(arg, "%s The charge for that is %d nuyen, which I see you don't have.",
                GET_CHAR_NAME(ch), ((GET_REAL_QUI(ch) + 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_QUI)) * 1000));
        do_say(trainer, arg, 0, SCMD_SAYTO);
        return TRUE;
      }
      GET_NUYEN(ch) -= (GET_REAL_QUI(ch) + 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_QUI)) * 1000;
    }
    if (GET_ATT_POINTS(ch) > 0)
      GET_ATT_POINTS(ch)--;
    else
      GET_KARMA(ch) -= (GET_REAL_QUI(ch) + 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_QUI)) * 200;
    GET_REAL_QUI(ch)++;
    affect_total(ch);
    send_to_char(ch, "After weeks of chasing chickens, your hard work pays off and your "
                 "quickness\r\nraises to %d.\r\n", GET_REAL_QUI(ch));
  } else if (!str_cmp(argument, "str") && IS_SET(trainers[ind].attribs, TSTR)) {
    if (GET_REAL_STR(ch) >= racial_limits[(int)GET_RACE(ch)][0][2]) {
      send_to_char("Your Strength attribute is at its maximum.\r\n", ch);
      return 1;
    }
    if (((2*(GET_REAL_STR(ch)+1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_STR))) > (int)(GET_KARMA(ch) / 100)) &&
        GET_ATT_POINTS(ch) < 1) {
      send_to_char("You don't have enough karma to raise your strength attribute.\r\n", ch);
      return 1;
    }
    if (!trainers[ind].is_newbie) {
      if (GET_NUYEN(ch) < ((GET_REAL_STR(ch)+ 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_STR)) * 1000)) {
        sprintf(arg, "%s The charge for that is %d nuyen, which I see you don't have.",
                GET_CHAR_NAME(ch), ((GET_REAL_STR(ch) + 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_STR)) * 1000));
        do_say(trainer, arg, 0, SCMD_SAYTO);
        return TRUE;
      }
      GET_NUYEN(ch) -= (GET_REAL_STR(ch) + 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_STR)) * 1000;
    }
    if (GET_ATT_POINTS(ch) > 0)
      GET_ATT_POINTS(ch)--;
    else
      GET_KARMA(ch) -= (GET_REAL_STR(ch) + 1+GET_POWER_TOTAL(ch, ADEPT_IMPROVED_STR)) * 200;
    GET_REAL_STR(ch)++;
    affect_total(ch);
    send_to_char(ch, "With months of weight lifting, your Strength increases to %d.\r\n",
                 GET_REAL_STR(ch));
  } else if (!str_cmp(argument, "cha") && IS_SET(trainers[ind].attribs, TCHA)) {
    if (GET_REAL_CHA(ch) >= racial_limits[(int)GET_RACE(ch)][0][3]) {
      send_to_char("Your charisma attribute is at its maximum.\r\n", ch);
      return 1;
    }
    if (((2*(GET_REAL_CHA(ch) + 1)) > (int)(GET_KARMA(ch) / 100)) &&
        GET_ATT_POINTS(ch) < 1) {
      send_to_char("You don't have enough karma to raise your charisma attribute.\r\n", ch);
      return 1;
    }
    if (!trainers[ind].is_newbie) {
      if (GET_NUYEN(ch) < ((GET_REAL_CHA(ch) + 1) * 1000)) {
        sprintf(arg, "%s The charge for that is %d nuyen, which I see you can't afford.",
                GET_CHAR_NAME(ch), ((GET_REAL_CHA(ch) + 1) * 1000));
        do_say(trainer, arg, 0, SCMD_SAYTO);
        return TRUE;
      }
      GET_NUYEN(ch) -= (GET_REAL_CHA(ch) + 1) * 1000;
    }
    if (GET_ATT_POINTS(ch) > 0)
      GET_ATT_POINTS(ch)--;
    else
      GET_KARMA(ch) -= (GET_REAL_CHA(ch) + 1) * 200;
    GET_REAL_CHA(ch)++;
    affect_total(ch);
    send_to_char(ch, "After weeks of reading self-help books and raising your "
                 "confidence, your\r\nCharisma raises to %d.\r\n", GET_REAL_CHA(ch));
  } else if (!str_cmp(argument, "int") && IS_SET(trainers[ind].attribs, TINT)) {
    if (GET_REAL_INT(ch) >= racial_limits[(int)GET_RACE(ch)][0][4]) {
      send_to_char("Your intelligence attribute is at its maximum.\r\n", ch);
      return 1;
    }
    if (((2*(GET_REAL_INT(ch) + 1)) > (int)(GET_KARMA(ch) / 100)) && GET_ATT_POINTS(ch) < 1) {
      send_to_char("You don't have enough karma to raise your intelligence "
                   "attribute.\r\n", ch);
      return 1;
    }
    if (!trainers[ind].is_newbie) {
      if (GET_NUYEN(ch) < ((GET_REAL_INT(ch) + 1) * 1000)) {
        sprintf(arg, "%s The charge for that is %d nuyen, which I see you can't afford.",
                GET_CHAR_NAME(ch), ((GET_REAL_INT(ch) + 1) * 1000));
        do_say(trainer, arg, 0, SCMD_SAYTO);
        return TRUE;
      }
      GET_NUYEN(ch) -= (GET_REAL_INT(ch) + 1) * 1000;
    }
    if (GET_ATT_POINTS(ch) > 0)
      GET_ATT_POINTS(ch)--;
    else
      GET_KARMA(ch) -= (GET_REAL_INT(ch) + 1) * 200;
    GET_REAL_INT(ch)++;
    affect_total(ch);
    send_to_char(ch, "Through many long hours using educational simsense, your "
                 "Intelligence raises\r\nto %d.\r\n", GET_REAL_INT(ch));
  } else if (!str_cmp(argument, "wil") && IS_SET(trainers[ind].attribs, TWIL)) {
    if (GET_REAL_WIL(ch) >= racial_limits[(int)GET_RACE(ch)][0][5]) {
      send_to_char("Your willpower attribute is at its maximum.\r\n", ch);
      return 1;
    }
    if (((2*(GET_REAL_WIL(ch) + 1)) > (int)(GET_KARMA(ch) / 100)) && GET_ATT_POINTS(ch) < 1) {
      send_to_char("You don't have enough karma to raise your willpower attribute.\r\n", ch);
      return 1;
    }
    if (!trainers[ind].is_newbie) {
      if (GET_NUYEN(ch) < ((GET_REAL_WIL(ch) + 1) * 1000)) {
        sprintf(arg, "%s The charge for that is %d nuyen, which I see you can't afford.",
                GET_CHAR_NAME(ch), ((GET_REAL_WIL(ch) + 1) * 1000));
        do_say(trainer, arg, 0, SCMD_SAYTO);
        return TRUE;
      }
      GET_NUYEN(ch) -= (GET_REAL_WIL(ch) + 1) * 1000;
    }
    if (GET_ATT_POINTS(ch) > 0)
      GET_ATT_POINTS(ch)--;
    else
      GET_KARMA(ch) -= (GET_REAL_WIL(ch) + 1) * 200;
    GET_REAL_WIL(ch)++;
    affect_total(ch);
    send_to_char(ch, "Through a strict regimen of work and study, your Willpower raises "
                 "to %d.\r\n", GET_REAL_WIL(ch));
  } else {
    if (GET_ATT_POINTS(ch) > 0) {
      send_to_char(ch, "You have %d points to distribute.  You can train",
                   GET_ATT_POINTS(ch));
      for (i = 0; (1 << i) <= TWIL; i++)
        if (IS_SET(trainers[ind].attribs, (1 << i))) {
          switch (1 << i) {
          case TBOD:
            send_to_char(ch, "%s bod", first ? "" : ",");
            break;
          case TQUI:
            send_to_char(ch, "%s qui", first ? "" : ",");
            break;
          case TSTR:
            send_to_char(ch, "%s str", first ? "" : ",");
            break;
          case TCHA:
            send_to_char(ch, "%s cha", first ? "" : ",");
            break;
          case TINT:
            send_to_char(ch, "%s int", first ? "" : ",");
            break;
          case TWIL:
            send_to_char(ch, "%s wil", first ? "" : ",");
            break;
          }
          first = 0;
        }
      send_to_char(".\r\n", ch);
    }
  }
  return 1;
}

SPECIAL(spell_trainer)
{
  struct char_data *trainer = (struct char_data *) me;
  int i, force;
  if (!CMD_IS("learn") || IS_NPC(ch) || !CAN_SEE(trainer, ch) || FIGHTING(ch) ||
      GET_POS(ch) < POS_STANDING)
    return FALSE;
  if (GET_TRADITION(ch) != TRAD_SHAMANIC && GET_TRADITION(ch) != TRAD_HERMETIC) {
    sprintf(arg, "%s You don't have the talent.", GET_CHAR_NAME(ch));
    do_say(trainer, arg, 0, SCMD_SAYTO);
    return TRUE;
  }
  if (!*argument) {
    send_to_char("The following spells are available for you to learn: \r\n", ch);
    if (GET_ASPECT(ch) != ASPECT_CONJURER)
      for (i = 0; spelltrainers[i].teacher; i++)
        if (GET_MOB_VNUM(trainer) == spelltrainers[i].teacher) {
          if ((GET_ASPECT(ch) == ASPECT_ELEMFIRE && spells[spelltrainers[i].type].category != COMBAT) ||
              (GET_ASPECT(ch) == ASPECT_ELEMEARTH && spells[spelltrainers[i].type].category != MANIPULATION) ||
              (GET_ASPECT(ch) == ASPECT_ELEMWATER && spells[spelltrainers[i].type].category != ILLUSION) ||
              (GET_ASPECT(ch) == ASPECT_ELEMAIR && spells[spelltrainers[i].type].category != DETECTION))
            continue;
          if (GET_ASPECT(ch) == ASPECT_SHAMANIST) {
            int skill = 0, target = 0;
            totem_bonus(ch, 0, spelltrainers[i].type, target, skill);
            if (skill < 1)
              continue;
          }

          send_to_char(ch, "%-30s Force Max: %d\r\n", spelltrainers[i].name, spelltrainers[i].force);
        }
    if (PLR_FLAGGED(ch, PLR_AUTH)) {
      if (GET_TRADITION(ch) == TRAD_HERMETIC && GET_ASPECT(ch) != ASPECT_SORCERER)
        send_to_char("Conjuring  Materials          1 Force Point/Level\r\n", ch);
      send_to_char("Extra Force Point             25000 \xC2\xA5\r\n", ch);
      send_to_char(ch, "%d Force Points Remaining.\r\n", GET_FORCE_POINTS(ch));
    } else
      send_to_char(ch, "%.2f Karma Available.\r\n", GET_KARMA(ch) / 100);
  } else {
    char *s, buf[MAX_STRING_LENGTH], buf1[MAX_STRING_LENGTH];
    if (strtok(argument, "\"") && (s = strtok(NULL, "\""))) {
      strcpy(buf, s);
      if ((s = strtok(NULL, "\0"))) {
        skip_spaces(&s);
        strcpy(buf1, s);
      } else
        *buf1 = '\0';
    } else
      two_arguments(argument, buf, buf1);
    if (PLR_FLAGGED(ch, PLR_AUTH)) {
      if (!str_cmp(buf, "force")) {
        if (GET_NUYEN(ch) < 25000)
          send_to_char("You don't have enough nuyen to buy an extra force point.\r\n", ch);
        else {
          send_to_char("You spend 25000\xC2\xA5 on an extra force point.\r\n", ch);
          GET_NUYEN(ch) -= 25000;
          GET_FORCE_POINTS(ch)++;
        }
        return TRUE;
      } else if (GET_TRADITION(ch) == TRAD_HERMETIC && GET_ASPECT(ch) != ASPECT_SORCERER &&
                 !str_cmp(buf, "conjuring")) {
        if (!(i = atoi(buf1)))
          send_to_char("What force of conjuring materials do you wish to buy?\r\n", ch);
        else if (GET_FORCE_POINTS(ch) < i)
          send_to_char("You don't have enough force points to purchase that much material.\r\n", ch);
        else {
          struct obj_data *obj = ch->carrying;
          for (; obj; obj = obj->next_content)
            if (GET_OBJ_TYPE(obj) == ITEM_MAGIC_TOOL && GET_OBJ_VAL(obj, 0) == TYPE_SUMMONING)
              break;
          if (!obj) {
            obj = read_object(116, VIRTUAL);
            obj_to_char(obj, ch);
          }
          GET_OBJ_COST(obj) += i * 1000;
          GET_FORCE_POINTS(ch) -= i;
          send_to_char(ch, "You buy %d force worth of conjuring materials.\r\n", i);
        }
        return TRUE;
      }
    }
    for (i = 0; spelltrainers[i].teacher; i++) {
      if ((GET_MOB_VNUM(trainer) != spelltrainers[i].teacher) ||
          (GET_ASPECT(ch) == ASPECT_ELEMFIRE && spells[spelltrainers[i].type].category != COMBAT) ||
          (GET_ASPECT(ch) == ASPECT_ELEMEARTH && spells[spelltrainers[i].type].category != MANIPULATION) ||
          (GET_ASPECT(ch) == ASPECT_ELEMWATER && spells[spelltrainers[i].type].category != ILLUSION) ||
          (GET_ASPECT(ch) == ASPECT_ELEMAIR && spells[spelltrainers[i].type].category != DETECTION))
        continue;
      if (GET_ASPECT(ch) == ASPECT_SHAMANIST) {
        int skill = 0, target = 0;
        totem_bonus(ch, 0, spelltrainers[i].type, target, skill);
        if (skill < 1)
          continue;
      }
      if (is_abbrev(buf, spelltrainers[i].name))
        break;
    }
    if (!spelltrainers[i].teacher) {
      send_to_char("That spell is not being offered here.\r\n", ch);
      return TRUE;
    }
    if (!(force = atoi(buf1)))
      send_to_char("What force do you wish to learn this spell at?\r\n", ch);
    else if (force > spelltrainers[i].force)
      send_to_char(ch, "%s doesn't know the spell at that high a force to teach you.\r\n", GET_NAME(trainer));
    else {
      if (PLR_FLAGGED(ch, PLR_AUTH)) {
        if (force > GET_FORCE_POINTS(ch)) {
          send_to_char("You don't have enough force points to learn the spell at that high a force.\r\n", ch);
          return TRUE;
        }
        GET_FORCE_POINTS(ch) -= force;
      } else {
        if (force > GET_KARMA(ch) / 100) {
          send_to_char("You don't have enough karma to learn the spell at that high a force.\r\n", ch);
          return TRUE;
        }
        GET_KARMA(ch) -= force * 100;
      }
      for (struct spell_data *spell = GET_SPELLS(ch); spell; spell = spell->next)
        if (spell->type == spelltrainers[i].type && spell->subtype == spelltrainers[i].subtype) {
          if (spell->force >= force) {
            send_to_char("You already know this spell at an equal or higher force.\r\n", ch);
            if (PLR_FLAGGED(ch, PLR_AUTH))
              GET_FORCE_POINTS(ch) += force;
            else
              GET_KARMA(ch) += force * 100;
            return TRUE;
          } else {
            struct spell_data *temp;
            REMOVE_FROM_LIST(spell, GET_SPELLS(ch), next);
            delete [] spell;
            break;
          }
        }
      send_to_char(ch, "%s sits you down and teaches you the ins and outs of casting %s at force %d.\r\n",
                   GET_NAME(trainer), spelltrainers[i].name, force);
      struct spell_data *spell = new spell_data;
      spell->name = str_dup(spelltrainers[i].name);
      spell->type = spelltrainers[i].type;
      spell->subtype = spelltrainers[i].subtype;
      spell->force = force;
      spell->next = GET_SPELLS(ch);
      GET_SPELLS(ch) = spell;
    }
  }
  return TRUE;
}

SPECIAL(adept_trainer)
{
  struct char_data *trainer = (struct char_data *) me;
  int ind, power, i;

  if (!CMD_IS("train") || IS_NPC(ch) || !CAN_SEE(trainer, ch) || FIGHTING(ch) ||
      GET_POS(ch) < POS_STANDING)
    return FALSE;

  if (GET_TRADITION(ch) != TRAD_ADEPT) {
    sprintf(arg, "%s You do not have the talent.", GET_CHAR_NAME(ch));
    do_say(trainer, arg, 0, SCMD_SAYTO);
    return TRUE;
  }

  skip_spaces(&argument);

  for (ind = 0; adepts[ind].vnum != 0; ind++)
    if (adepts[ind].vnum == GET_MOB_VNUM(trainer))
      break;

  if (adepts[ind].vnum != GET_MOB_VNUM(trainer))
    return FALSE;

  skip_spaces(&argument);

  if (adepts[ind].is_newbie && !PLR_FLAGGED(ch, PLR_NEWBIE)) {
    sprintf(arg, "%s You do not belong here.", GET_CHAR_NAME(ch));
    do_say(trainer, arg, 0, SCMD_SAYTO);
    return TRUE;
  } else if (!adepts[ind].is_newbie && PLR_FLAGGED(ch, PLR_NEWBIE)) {
    sprintf(arg, "%s You're not quite ready yet!", GET_CHAR_NAME(ch));
    do_say(trainer, arg, 0, SCMD_SAYTO);
    return TRUE;
  }
  if (GET_ATT_POINTS(ch) > 0) {
    sprintf(arg, "%s You must go train your attributes fully before you see me.", GET_CHAR_NAME(ch));
    do_say(trainer, arg, 0, SCMD_SAYTO);
    return TRUE;
  }
  if (!*argument) {
    int num = 0;
    for (i = 1; i < ADEPT_NUMPOWER; i++)
      if (adepts[ind].skills[i])
        num++;
    sprintf(buf, "You can learn the following abilit%s:\r\n", num == 1 ? "y" : "ies");
    for (i = 1; i < ADEPT_NUMPOWER; i++)
      if (adepts[ind].skills[i] && GET_POWER_TOTAL(ch, i) < max_ability(i))
        sprintf(buf + strlen(buf), "%30s (%0.2f points)\r\n", adept_powers[i],
                ((float) ability_cost(i, GET_POWER_TOTAL(ch, i) + 1)/ 100));
    sprintf(buf + strlen(buf), "\r\nYou have %0.2f power point%s to "
            "distribute to your abilities.\r\n", ((float)GET_PP(ch) / 100),
            ((GET_PP(ch) != 100) ? "s" : ""));
    send_to_char(buf, ch);
    return TRUE;
  }
  for (power = 1; power < ADEPT_NUMPOWER; power++)
    if (is_abbrev(argument, adept_powers[power]) && adepts[ind].skills[power])
      break;
  if (power == ADEPT_NUMPOWER) {
    send_to_char(ch, "Which power do you wish to train?\r\n");
    return TRUE;
  }
  if (GET_POWER_TOTAL(ch, power) >= adepts[ind].skills[power] ||
      GET_POWER_TOTAL(ch, power) >= max_ability(power)) {
    send_to_char("You have advanced beyond the teachings of your trainer.\r\n", ch);
    return 1;
  }
  if (GET_POWER_TOTAL(ch, power) >= GET_MAG(ch) / 100) {
    send_to_char(ch, "You have advanced to the limit of your abilities.\r\n");
    return TRUE;
  }
  int cost = ability_cost(power, GET_POWER_TOTAL(ch, power) + 1);
  switch (power) {
  case ADEPT_IMPROVED_BOD:
    if (GET_REAL_BOD(ch) + GET_POWER_TOTAL(ch, ADEPT_IMPROVED_BOD) >= racial_limits[(int)GET_RACE(ch)][0][0] - GET_PERM_BOD_LOSS(ch))
      cost *= 2;
    break;
  case ADEPT_IMPROVED_QUI:
    if (GET_REAL_QUI(ch) + GET_POWER_TOTAL(ch, ADEPT_IMPROVED_QUI) >= racial_limits[(int)GET_RACE(ch)][0][1])
      cost *= 2;
    break;
  case ADEPT_IMPROVED_STR:
    if (GET_REAL_STR(ch) + GET_POWER_TOTAL(ch, ADEPT_IMPROVED_STR) >= racial_limits[(int)GET_RACE(ch)][0][2])
      cost *= 2;
    break;
  }
  if (GET_PP(ch) < cost) {
    send_to_char("You do not have enough magic to raise that ability.\r\n", ch);
    return TRUE;
  }

  GET_PP(ch) -= cost;
  send_to_char("After hours of focus and practice, you feel your "
               "ability sharpen.\r\n", ch);

  GET_POWER_TOTAL(ch, power)++;

  if (GET_POWER_TOTAL(ch, power) >= max_ability(power) ||
      GET_POWER_TOTAL(ch, power) >= adepts[ind].skills[power])
    send_to_char("You have learned all your teacher knows in that area.\r\n", ch);

  affect_total(ch);
  return TRUE;
}

/* ********************************************************************
*  General special procedures for mobiles                             *
******************************************************************** */

/*SPECIAL(magic_user)
{
  struct char_data *vict;

  if (cmd || GET_POS(ch) != POS_FIGHTING)
    return FALSE;

  for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room)
    if (FIGHTING(vict) == ch && !number(0, 4))
      break;

  if (vict == NULL)
    vict = FIGHTING(ch);

  if ((GET_TKE(ch) > 20) && (number(0, 10) == 0))
    mob_cast(ch, vict, NULL, SPELL_STUN_BOLT, 0);

  if ((GET_TKE(ch) > 12) && (number(0, 8) == 0))
    mob_cast(ch, vict, NULL, SPELL_POWER_DART, 0);

  if ((GET_TKE(ch) > 18) && (number(0, 12) == 0)) {
    switch(number(0 ,1)) {
    case 0:
      mob_cast(ch, vict, NULL, SPELL_MANA_BOLT, 0);
      break;
    case 1:
      mob_cast(ch, vict, NULL, SPELL_POWER_BOLT, 0);
      break;
    }
  }
  if (number(0, 4))
    return TRUE;

  switch ((int)(GET_TKE(ch) / 5)) {
  case 4:
  case 5:
    mob_cast(ch, vict, NULL, SPELL_POWER_DART, 0);
    break;
  case 6:
  case 7:
    mob_cast(ch, vict, NULL, SPELL_POWER_MISSILE, 0);
    break;
  default:
    mob_cast(ch, vict, NULL, SPELL_ELEMENTBALL, SPELL_ELEMENT_FIRE);
    break;
  }
  return TRUE;
}
*/

/* ********************************************************************
*  Special procedures for mobiles                                      *
******************************************************************** */

SPECIAL(janitor)
{
  struct char_data *jan = (struct char_data *) me;
  struct obj_data *i;
  bool extract = FALSE;
  ACMD(do_gen_comm);

  if (cmd || FIGHTING(ch) || !AWAKE(ch))
    return 0;

  for (i = world[jan->in_room].contents; i; i = i->next_content) {
    if (!CAN_WEAR(i, ITEM_WEAR_TAKE) || IS_OBJ_STAT(i, ITEM_CORPSE))
      continue;
    switch (GET_MOB_VNUM(jan)) {
      case 2022:
        act("$n sweeps some garbage into the gutter.",
            FALSE, jan, 0, 0, TO_ROOM);
        extract = TRUE;
        break;
      case TIM_ENCHANTER:
        act("$n rolls $s eyes in disgust as $e eats up some trash.",
            FALSE, jan, 0, 0, TO_ROOM);
        extract = TRUE;
        break;
      case 4008:
        act("$n mumbles as $e picks up some trash.",
            FALSE, jan, 0, 0, TO_ROOM);
        break;
      case 5101:
        act("$n dumps some trash into $s stylized trash compactor.",
            FALSE, jan, 0, 0, TO_ROOM);
        extract = TRUE;
        break;
      default:
        act("$n picks up some trash.", FALSE, jan, 0, 0, TO_ROOM);
        break;
    }
    obj_from_room(i);
    if (extract)
      extract_obj(i);
    else
      obj_to_char(i, jan);
    return FALSE;
  }

  return FALSE;
}

SPECIAL(generic_guard)
{
  struct char_data *tch;

  if (cmd || FIGHTING(ch) || !AWAKE(ch))
    return 0;

  for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room)
    if (IS_NPC(tch) && CAN_SEE(ch, tch) && PLR_FLAGGED(tch, PLR_KILLER)) {
      act("$n screams, 'Hey, it's one of those fraggin unreg'ed pkers!!!'",
          FALSE, ch, 0, 0, TO_ROOM);
      set_fighting(ch, tch);
      return (TRUE);
    }

  switch (GET_MOB_VNUM(ch)) {
    case 2013:
    case 5100:
      for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room)
        if (CAN_SEE(ch, tch) && GET_POS(tch) == POS_SLEEPING) {
          GET_POS(tch) = POS_SITTING;
          act("$n slaps $N on $S shoulder, forcing $M awake.",
              FALSE, ch, 0, tch, TO_NOTVICT);
          act("You slap $N on $S shoulder, forcing $M awake.",
              FALSE, ch, 0, tch, TO_CHAR);
          act("$n slaps you on the shoulder, forcing you awake.",
              FALSE, ch, 0, tch, TO_VICT);
          do_say(ch, "I'd advise that you move, citizen.", 0, 0);
          return (TRUE);
        }
      break;
    case 1916:
      for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room)
        if (CAN_SEE(ch, tch) && GET_POS(tch) == POS_SLEEPING) {
          GET_POS(tch) = POS_SITTING;
          act("$n pokes $N on the shoulder, forcing $M awake.",
              FALSE, ch, 0, tch, TO_NOTVICT);
          act("You poke $N on the shoulder, forcing $M awake.",
              FALSE, ch, 0, tch, TO_CHAR);
          act("$n pokes you on the shoulder, forcing you awake.",
              FALSE, ch, 0, tch, TO_VICT);
          do_say(ch, "This ain't the place for that.", 0, 0);
          return (TRUE);
        }
      break;
  }

  return (FALSE);
}

SPECIAL(car_dealer)
{
  struct veh_data *veh, *newveh;

  if (!cmd)
    return FALSE;

  int car_room = ch->in_room - 1;

  if (CMD_IS("list")) {
    send_to_char("Available vehicles are:\r\n", ch);
    for (veh = world[car_room].vehicles; veh; veh = veh->next_veh) {
      sprintf(buf, "%8d - %s\r\n", veh->cost, veh->short_description);
      send_to_char(buf, ch);
    }
    return TRUE;
  } else if (CMD_IS("buy")) {
    argument = one_argument(argument, buf);
    if (!(veh = get_veh_list(buf, world[car_room].vehicles, ch))) {
      send_to_char("There is no such vehicle for sale.\r\n", ch);
      return TRUE;
    }
    if (GET_NUYEN(ch) < veh->cost) {
      send_to_char("You can't afford that.\r\n", ch);
      return TRUE;
    }
    GET_NUYEN(ch) -= veh->cost;
    newveh = read_vehicle(veh->veh_number, REAL);
    veh_to_room(newveh, ch->in_room);
    newveh->owner = GET_IDNUM(ch);
    newveh->idnum = number(0, 1000000);
    if (veh->type == VEH_DRONE)
      sprintf(buf, "You buy a %s. It is brought out into the room.\r\n", newveh->short_description);
    else
      sprintf(buf, "You buy a %s. It is wheeled out into the yard.\r\n", newveh->short_description);
    send_to_char(buf, ch);
    save_vehicles();
    return TRUE;
  }
  return FALSE;
}

SPECIAL(pike)
{
  struct char_data *pike = (struct char_data *) me;

  if (CMD_IS("give")) {
    char arg1[50], arg2[50], arg3[50], *temp;

    temp = any_one_arg(argument, arg1);
    temp = any_one_arg(temp, arg2);
    temp = any_one_arg(temp, arg3);

    if (*arg2 && !strcasecmp("pike", arg2)) {
      act("$n says, \"Hey chummer, nuyen only.\"", FALSE, pike, 0, 0, TO_ROOM);
      send_to_char(pike, "You say, \"Hey chummer, nuyen only.\"\r\n");
      return TRUE;
    }

    if (!*arg3 || strcasecmp("pike", arg3))
      return FALSE;

    int amount = atoi(arg1);
    if (amount < 50) {
      act("$n says, \"Hey chummer, it's 50 nuyen or nothing.\"",
          FALSE, pike, 0, 0, TO_ROOM);
      send_to_char(pike, "You say, \"Hey chummer, it's 50 nuyen or nothing.\"\r\n");
      return TRUE;
    } else if (GET_NUYEN(ch) < 50) {
      act("$n says, \"Better check your account, you don't have the 50 creds.\"",
          FALSE, pike, 0, ch, TO_ROOM);
      send_to_char(pike, "You say, \"Better check your account, you don't have the 50 creds.\"");
    } else {
      act("$n says, \"Thanks chummer.\"", FALSE, pike, 0, 0, TO_ROOM);
      send_to_char(pike, "You say, \"Thanks chummer.\"\r\n");
      if (world[pike->in_room].number != 2337) {
        act("$n says, \"Where the frag am I?\"", FALSE, pike, 0, 0, TO_ROOM);
        send_to_char(pike, "You say, \"Where the frag am I?\"\r\n");
      } else {
        if (IS_SET(EXIT(pike, WEST)->exit_info, EX_CLOSED)) {
          if (IS_SET(EXIT(pike, WEST)->exit_info, EX_LOCKED)) {
            act("With a loud click, $n unlocks the gate to the junkyard.",
                FALSE, pike, 0, 0, TO_ROOM);
            send_to_char(pike, "With a loud click, you unlock the gate to the junkyard.\r\n");
            TOGGLE_BIT(EXIT(pike, WEST)->exit_info, EX_LOCKED);
            do_gen_door(pike, "gate", 0, SCMD_OPEN);
            act("$n says, \"There ya go.\"", FALSE, pike, 0, 0, TO_ROOM);
            send_to_char(pike, "You say, \"There ya go.\"\r\n");
            GET_NUYEN(pike) += amount;
            GET_NUYEN(ch) -= amount;
          } else {
            act("$n says, \"The gate isn't locked, so go on in.\"",
                FALSE, pike, 0, 0, TO_ROOM);
            send_to_char(pike, "You say, \"The gate isn't locked, so go on in.\"\r\n");
          }
        } else {
          act("$n says, \"The gate isn't even closed, so go on in.\"",
              FALSE, pike, 0, 0, TO_ROOM);
          send_to_char(pike, "You say, \"The gate isn't even closed, so go on in.\"\r\n");
        }
      }
      return TRUE;
    }
  } else if (CMD_IS("west")) {
    if (perform_move(ch, WEST, LEADER, NULL) && world[pike->in_room].number == 2337) {
      if (!IS_SET(EXIT(pike, WEST)->exit_info, EX_CLOSED))
        do_gen_door(pike, "gate", 0, SCMD_CLOSE);

      if (IS_SET(EXIT(pike, WEST)->exit_info, EX_CLOSED) &&
          !IS_SET(EXIT(pike, WEST)->exit_info, EX_LOCKED)) {
        TOGGLE_BIT(EXIT(pike, WEST)->exit_info, EX_LOCKED);
        act("$n locks the gate.", FALSE, pike, 0, 0, TO_ROOM);
      }
    }
    // we return TRUE no matter what cause we move with perform_move
    return TRUE;
  } else if (number(0, 40) == 9)
    do_say(pike, "For 50 nuyen, I'll unlock the gate to the junkyard for you.", 0, 0);

  return FALSE;
}

SPECIAL(jeff)
{
  struct char_data *jeff = (struct char_data *) me;
  if (!AWAKE(jeff))
    return FALSE;

  if (CMD_IS("give")) {
    char arg1[50], arg2[50], arg3[50], *temp;

    temp = any_one_arg(argument, arg1);
    temp = any_one_arg(temp, arg2);
    temp = any_one_arg(temp, arg3);

    if (*arg2 && !strcasecmp("jeff", arg2)) {
      act("$n says, \"Hey fraghead, nuyen only.\"", FALSE, jeff, 0, 0, TO_ROOM);
      send_to_char(jeff, "You say, \"Hey fraghead, nuyen only.\"\r\n");
      return TRUE;
    }

    if (!*arg3 || strcasecmp("jeff", arg3))
      return FALSE;

    int amount = atoi(arg1);
    if (amount < 10) {
      do_say(jeff, "Hey fraghead, it's 10 creds!", 0, 0);
      return TRUE;
    } else if (GET_NUYEN(ch) < amount) {
      do_say(jeff, "Hey slothead, you're not even worth that much.", 0, 0);
      return TRUE;
    } else {
      act("$n looks at $N suspiciously.", FALSE, jeff, 0, ch, TO_NOTVICT);
      act("You look at $N suspiciously.", FALSE, jeff, 0, ch, TO_CHAR);
      act("$n looks at you suspiciously.", FALSE, jeff, 0, ch, TO_VICT);
      if (world[jeff->in_room].number != 2326) {
        do_say(jeff, "Where the frag am I?", 0, 0);
      } else {
        if (IS_SET(EXIT(jeff, EAST)->exit_info, EX_CLOSED)) {
          do_gen_door(jeff, "roadblock", 0, SCMD_OPEN);
          GET_NUYEN(jeff) += amount;
          GET_NUYEN(ch) -= amount;
          do_say(jeff, "Go ahead.", 0, 0);
        } else {
          do_say(jeff, "The roadblock is open, so go ahead.", 0, 0);
        }
      }
      return TRUE;
    }
  } else if (CMD_IS("east")) {
    if (perform_move(ch, EAST, LEADER, NULL) && world[jeff->in_room].number == 2326) {
      if (!IS_SET(EXIT(jeff, EAST)->exit_info, EX_CLOSED))
        do_gen_door(jeff, "roadblock", 0, SCMD_CLOSE);
    }
    return TRUE;
  } else if (CMD_IS("open")) {
    char *args = argument;
    skip_spaces(&args);
    if (!strcasecmp("roadblock", args))
      do_say(jeff, "Slot off, it's 10 creds to pass chummer.", 0, 0);
    return TRUE;
  } else if (number(0, 50) == 11)
    do_say(jeff, "10 creds to pass through the roadblock chummer.", 0, 0);

  return FALSE;
}

SPECIAL(delaney)
{
  if (cmd || FIGHTING(ch) || !AWAKE(ch))
    return FALSE;

  switch (number(0, 20)) {
    case 0:
      act("$n cranks some ancient classical music and settles down "
          "with a cigar.", FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
    case 1:
      act("After a long night on the booze, $n contemplates the basket by his "
          "desk, convulses, and throws up in it.", FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
  }
  return FALSE;
}

SPECIAL(bio_secretary)
{
  if (cmd || FIGHTING(ch) || !AWAKE(ch))
    return FALSE;

  switch (number(0, 20)) {
    case 0:
      act("$n applies some more mascara and files her nails.",
          FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
    case 1:
      do_say(ch, "Hello, can I help you at all?", 0, 0);
      return TRUE;
  }
  return FALSE;
}

SPECIAL (captain)
{
  if (cmd || FIGHTING(ch) || !AWAKE(ch))
    return FALSE;

  switch (number(0, 80)) {
    case 0:
      do_say(ch, "Have you seen the state of the streets out there?", 0, 0);
      return TRUE;
    case 1:
      do_say(ch, "Hey you! Who allowed you in here?", 0, 0);
      return TRUE;
    case 2:
      act("$n kicks back and enjoys the football game on the Trid.",
          FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
    case 3:
      do_say(ch, "Life in the sprawl has gotten bad, and it's our job to "
             "clean it up.", 0, 0);
      return TRUE;
    case 4:
      act("$n removes the magazine from his pistol and checks the bullets.",
          FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
  }
  return FALSE;
}

SPECIAL(rodgers)
{
  if (cmd || FIGHTING(ch) || !AWAKE(ch))
    return 0;

  switch (number(0, 80)) {
    case 0:
      do_say(ch, "Another day in the office, another day in hell.", 0, 0);
      return TRUE;
    case 1:
      do_say(ch, "All this paperwork! Why can't I be on the beat?", 0, 0);
      return TRUE;
    case 2:
      act("$n pushes some keys on his terminal and views the general payroll "
          "in envy.", FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
    case 3:
      act("$n fumes with rage as he hears of another failed drug-bust.",
          FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
    case 4:
      act("$n polishes the ornaments in his display cabinet while humming an "
          "old '90s tune.", FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
  }
  return FALSE;
}

SPECIAL(lone_star_park)
{
  struct char_data *tch;

  if (cmd || !AWAKE(ch) || FIGHTING(ch))
    return FALSE;

  for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room) {
    if (CAN_SEE(ch, tch) && PLR_FLAGGED(tch, PLR_KILLER)) {
      act("$n pulls out his trusty standard-issue weapon upon seeing you!",
          FALSE, ch, 0, tch, TO_VICT);
      act("$n sneers at $N, recognizing him as a felon!",
          FALSE, ch, 0, tch, TO_NOTVICT);
      act("You recognize $N from the Wanted picture on a milk carton and attack!",
          FALSE, ch, 0, tch, TO_CHAR);
      set_fighting(ch, tch);
      return TRUE;
    }
  }

  for (tch = world[ch->in_room].people; tch; tch = tch->next_in_room) {
    if (GET_MOB_VNUM(tch) == PARK_MUGGER) {
      set_fighting(ch, tch);
      return TRUE;
    }
  }

  return FALSE;
}

SPECIAL(mugger_park)
{
  struct char_data *vict;
  struct obj_data *obj;
  int gold;

  if (cmd || !AWAKE(ch))
    return FALSE;

  if (FIGHTING(ch) && !number(0,9)) {
    vict = FIGHTING(ch);
    gold = (int)(number(5,8) * number(2,3) * 2.5);
    if (GET_NUYEN(vict) > gold) {
      act("$n deftly lifts some nuyen from $N!", FALSE, ch, 0, vict, TO_NOTVICT);
      act("$n deftly lifts some nuyen from your pocket!", FALSE, ch, 0, vict, TO_VICT);
      act("You deftly grab some nuyen from $N!", FALSE, ch, 0, vict, TO_CHAR);
      GET_NUYEN(ch) += gold;
      GET_NUYEN(vict) -= gold;
      return TRUE;
    }
    return FALSE;
  }

  if (!FIGHTING(ch)) {
    for (obj = world[ch->in_room].contents; obj; obj = obj->next_content)
      if (GET_OBJ_TYPE(obj) == ITEM_MONEY) {
        act("$n grins as he picks up $p from the ground.", FALSE, ch, obj, 0, TO_ROOM);
        act("You grin slightly as you pick up $p.", FALSE, ch, obj, 0, TO_CHAR);
        GET_NUYEN(ch) += GET_OBJ_VAL(obj, 0);
        extract_obj(obj);
        return TRUE;
      }
    for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room) {
      if (CAN_SEE(ch, vict)
          && (((IS_NPC(vict) && vict != ch)
              || (GET_TKE(vict) > 8 && !IS_SENATOR(vict))) && !number(0,4))) {
            act("$n says, 'Gimme your money, chummer!'",
                FALSE, ch, 0, vict, TO_VICT);
            act("$n pulls out $s gun and asks $N for $s credstick.",
                FALSE, ch, 0, vict, TO_NOTVICT);
            act("You say to $N, 'Gimme your money, chummer!' in typical New York "
                "fashion.", FALSE, ch, 0, vict, TO_CHAR);
            set_fighting(ch, vict);
            return TRUE;
          }
    }
  }
  return FALSE;
}

SPECIAL(gate_guard_park)
{
  struct char_data *guard = (char_data *) me;

  if (!AWAKE(guard) || FIGHTING(guard))
    return FALSE;

  if (!cmd) {
    switch (number(1,160)) {
      case 12:
        do_say(guard, "Hey, bub, this is private property.", 0, 0);
        return TRUE;
      case 92:
        do_say(guard, "I thought I told you to leave.", 0, 0);
        return TRUE;
      case 147:
        do_say(guard, "This is the property of Takehero Tsuyama.. "
               "Trespassers are shot on sight.", 0, 0);
        return TRUE;
    }
    return FALSE;
  }

  if (CMD_IS("north")) {
    if (perform_move(ch, NORTH, LEADER, NULL) &&
        world[guard->in_room].number == MANSION_GATE) {
      if (!IS_SET(EXIT(guard, NORTH)->exit_info, EX_CLOSED))
        do_gen_door(guard, "gate", 0, SCMD_CLOSE);
      if (IS_SET(EXIT(guard, NORTH)->exit_info, EX_CLOSED)) {
        TOGGLE_BIT(EXIT(guard, NORTH)->exit_info, EX_CLOSED | EX_LOCKED);
        act("$n locks the gate.", FALSE, guard, 0, 0, TO_ROOM);
      }
    }
    return TRUE;
  } else if (CMD_IS("open") || CMD_IS("unlock") || CMD_IS("bypass")) {
    skip_spaces(&argument);
    if (!strcasecmp("gate", argument)) {
      do_say(guard, "Piddle off, I'm tryin' to do my job here.", 0, 0);
      return TRUE;
    }
  }
  return(FALSE);
}

SPECIAL(squirrel_park)
{
  if (cmd || !AWAKE(ch))
    return FALSE;

  switch (number(1,150)) {
    case 74:
      act("$n chatters quietly.", FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
    case 148:
      act("$n munches happily on an acorn.", FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
  }
  return FALSE;
}

SPECIAL(sick_ork)
{
  struct char_data *vict;

  if (cmd || number(0,40))
    return FALSE;

  for(vict = world[ch->in_room].people; vict; vict = vict->next_in_room) {
    if (vict && CAN_SEE(ch, vict)) {
      act("$n turns to you curiously.  You can see vomit running down his "
          "chin.", FALSE, ch, 0, vict, TO_VICT);
      act("$n looks at $N in bewilderment.", FALSE, ch, 0, vict, TO_NOTVICT);
      act("You look at $N.  The sight of $M almost makes you puke.",
          FALSE, ch, 0, vict, TO_CHAR);
      if(!number(0,2)) {
        act("$n convulses quickly and pukes into the toilet, resulting in an "
            "odd-colored mess.", FALSE, ch, 0, 0, TO_ROOM);
        act("You suddenly feel very sick, your stomach twists, and you spew "
            "a strange arrangement of vomit into the toilet.",
            FALSE, ch, 0, 0, TO_CHAR);
      }
      return TRUE;
    }
  }
  return FALSE;
}

SPECIAL(adept_guard)
{
  struct char_data *vict;

  if (cmd)
    return(FALSE);

  if (!FIGHTING(ch)) {
    for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room) {
      if (vict != ch && CAN_SEE(ch, vict) && (IS_NPC(vict) ||
                                              (GET_TKE(vict) > 10 && !IS_SENATOR(vict))) &&
          world[ch->in_room].number == 4098) {
        act("$n steps out from the shadows and touches $N!",
            FALSE, ch, 0, vict, TO_NOTVICT);
        act("A streak of pain courses through your body!",
            FALSE, ch, 0, vict, TO_VICT);
        act("Sensing an oppertune moment, you come out of hiding and draw upon "
            "your mystic abilities to kill $N.", FALSE, ch, 0, vict, TO_CHAR);
        damage(ch, vict, 1, 0, PHYSICAL);
        return(TRUE);
      }
    }
    if (!IS_AFFECTED(ch, AFF_HIDE)) {
      act("$n fades out of sight.", FALSE, ch, 0, ch, TO_NOTVICT);
      act("You fade out of the sight of others.", FALSE, ch, 0, 0, TO_CHAR);
      AFF_FLAGS(ch).SetBit(AFF_HIDE);
      return(FALSE);
    }
  } else {
    vict = FIGHTING(ch);
    switch(number(1,20)) {
      case 8:
        act("As $n reaches touches you, you begin to feel numb.",
            FALSE, ch, 0, vict, TO_VICT);
        act("$n reaches out his hand and grabs onto $N!",
            FALSE, ch, 0, vict, TO_NOTVICT);
        act("You grab $N and let energy flow through him.",
            FALSE, ch, 0, vict, TO_CHAR);
        damage(ch, vict, number(0, 2), 0, PHYSICAL);
        return FALSE;
    }
  }
  return(FALSE);
}

SPECIAL(takehero_tsuyama)
{
  struct char_data *tsuyama = (struct char_data *) me;
  struct char_data *vict;

  if (cmd || !AWAKE(tsuyama))
    return(FALSE);

  if (!FIGHTING(tsuyama)) {
    for(vict = world[tsuyama->in_room].people; vict; vict = vict->next_in_room) {
      if (vict != ch && CAN_SEE(tsuyama, vict)
          && ((IS_NPC(vict) || (GET_TKE(vict) > 19 && !IS_SENATOR(vict)))
              && number(0,3) && world[tsuyama->in_room].number == 4101)) {
            act("$n unsheathes his deadly katana, swiftly attacking $N!",
                FALSE, tsuyama, 0, vict, TO_NOTVICT);
            act("$n unsheathes his deadly katana, swiftly attacking you!",
                FALSE, tsuyama, 0, vict, TO_VICT);
            act("You unsheathe your katana and swiftly attack $N!",
                FALSE, tsuyama, 0, vict, TO_CHAR);
            damage(tsuyama, vict, 2, TYPE_SLASH, PHYSICAL);
            return(TRUE);
          }
    }
    return FALSE;
  }

  if (GET_PHYSICAL(tsuyama) < GET_MAX_PHYSICAL(tsuyama) && !number(0, 4)) {
    //    mob_cast(tsuyama, tsuyama, NULL, SPELL_HEAL, MODERATE);
    return(TRUE);
  }

  for (vict = world[tsuyama->in_room].people; vict; vict = vict->next_in_room)
    if (FIGHTING(vict) == tsuyama && vict != FIGHTING(tsuyama) &&
        CAN_SEE(tsuyama, vict) && !number(0, 4)) {
      stop_fighting(tsuyama);
      set_fighting(tsuyama, vict);
      return FALSE;
    }
  return FALSE;
}

SPECIAL(aegnor)
{
  struct char_data *vict;
  int dist, dir, range = 0, room, nextroom;

  if (cmd || !AWAKE(ch) || FIGHTING(ch))
    return FALSE;

  range = find_sight(ch);

  for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room)
    if (!IS_NPC(vict) && CAN_SEE(ch, vict) && FIGHTING(vict) &&
        IS_NPC(FIGHTING(vict))) {
      act("$n sneers at you and attacks!", FALSE, ch, 0, vict, TO_VICT);
      act("$n sneers at $N and attacks $M!", TRUE, ch, 0, vict, TO_NOTVICT);
      act("You sneer at $N and attack $M!", FALSE, ch, 0, vict, TO_CHAR);
      set_fighting(ch, vict);
      return TRUE;
    }

  for (dir = 0; dir < NUM_OF_DIRS; dir++) {
    room = ch->in_room;
    if (CAN_GO2(room, dir))
      nextroom = EXIT2(room, dir)->to_room;
    else
      nextroom = NOWHERE;

    for (dist = 1; nextroom != NOWHERE && dist <= range; dist++) {
      for (vict = world[nextroom].people; vict; vict = vict->next_in_room)
        if (!IS_NPC(vict) && CAN_SEE(ch, vict) && FIGHTING(vict) &&
            IS_NPC(FIGHTING(vict))) {
          act("You see $n sneer at you from the bar and attack!",
              FALSE, ch, 0, vict, TO_VICT);
          act("$n sneers at someone in the distance and attacks!",
              TRUE, ch, 0, 0, TO_ROOM);
          act("You sneer at $N and attack $M!", FALSE, ch, 0, vict, TO_CHAR);
          set_fighting(ch, vict);
          return TRUE;
        }

      room = nextroom;
      if (CAN_GO2(room, dir))
        nextroom = EXIT2(room, dir)->to_room;
      else
        nextroom = NOWHERE;
    }
  }
  return FALSE;
}

SPECIAL(branson)
{
  if (cmd || FIGHTING(ch) || !AWAKE(ch))
    return 0;

  switch (number(0, 60)) {
    case 0:
      do_say(ch, "As Chief Executive, it is my job to keep this company in line.", 0, 0);
      return TRUE;
    case 1:
      do_say(ch, "Do you understand what I'm trying to do? Do you?", 0, 0);
      return TRUE;
    case 2:
      do_say(ch, "No-one else has a character to rival mine.", 0, 0);
      return TRUE;
    case 3:
      act("$n switches the trid to CNN and checks the latest stock updates.",
          FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
  }
  return FALSE;
}

SPECIAL(harlten)
{
  if (cmd || FIGHTING(ch) || !AWAKE(ch))
    return 0;

  switch (number(0, 60)) {
    case 0:
      act("$n straightens his tie and smiles meekly in the mirror.",
          FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
    case 1:
      do_say(ch, "We here at BioHyde aim to make you a better person, "
             "physically and spiritually.", 0, 0);
      return TRUE;
    case 2:
      act("$n screams at his secretary for more Macadamia coffee.",
          FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
  }
  return FALSE;
}

SPECIAL(bio_guard)
{
  if (cmd || FIGHTING(ch) || !AWAKE(ch))
    return 0;

  switch (number(0, 100)) {
    case 0:
      do_say(ch, "These premises are closed. Access is only permitted to "
             "those with security passes.", 0, 0);
      return TRUE;
    case 1:
      do_say(ch, "Hey! What you looking for? Trouble?", 0, 0);
      return TRUE;
    case 2:
      act("Seeking another nicotine hit, the guard sparks a cigarette.",
          FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
    case 3:
      do_say(ch, "If you would like a tour of the building, call our "
             "Customer Service Desk.", 0, 0);
      return TRUE;
    case 4:
      act("$n polishes the huge machine gun at his side.",
          FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
  }
  return FALSE;
}

SPECIAL(worker)
{
  if (cmd || FIGHTING(ch) || !AWAKE(ch))
    return 0;

  switch (number(0, 60)) {
    case 0:
      act("$n rushes around the room, hurriedly grabbing papers.",
          FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
    case 1:
      do_say(ch, "The stress! THE STRESS!", 0, 0);
      return TRUE;
    case 2:
      act("Finding a sudden free moment, $n lights a cigarette to calm his "
          "nerves.", FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
  }
  return FALSE;
}

SPECIAL(saeder_guard)
{
  struct char_data *guard = (char_data *) me;
  struct obj_data *obj;
  bool found = FALSE;

  if (!AWAKE(guard) || (GET_POS(guard) == POS_FIGHTING))
    return(FALSE);

  if (CMD_IS("east") && CAN_SEE(guard, ch) && world[guard->in_room].number == 4930) {
    for (obj = ch->carrying; obj; obj = obj->next_content)
      if (GET_OBJ_VNUM(obj) == 4914)
        found = TRUE;

    if (found)
      perform_move(ch, EAST, LEADER, NULL);
    else
      do_say(guard, "No pass, no entry.", 0, 0);
    return(TRUE);
  }

  return(FALSE);
}

SPECIAL(crime_mall_guard)
{
  if (!cmd)
    return FALSE;

  struct char_data *guard = (struct char_data *) me;

  if ((world[guard->in_room].number == 10075 && CMD_IS("east")) ||
      (world[guard->in_room].number == 10077 && CMD_IS("west"))) {
    if (GET_NUYEN(ch) < 2000000) {
      act("$n shakes $s head as $e stops you.  \"Not this time, chummer.\"",
          FALSE, guard, 0, ch, TO_VICT);
      act("You shake your head as $N tries to sneak past you.",
          FALSE, guard, 0, ch, TO_CHAR);
      return TRUE;
    } else {
      act("$n gives you one final look, and lets you pass.",
          FALSE, guard, 0, ch, TO_VICT);
      act("As $N finally got 2M nuyen, you let $M pass.",
          FALSE, guard, 0, ch, TO_CHAR);
      return FALSE;
    }
  }
  return FALSE;
}

SPECIAL(hacker)
{
  struct char_data *hacker = (struct char_data *) me, *vict;
  struct obj_data *obj;
  int amount, nuyen = 0;

  if (CMD_IS("value")) {
    if (!*argument) {
      send_to_char("Value what?\r\n", ch);
      return TRUE;
    } else if (!AWAKE(hacker))
      return(FALSE);
    else if (!CAN_SEE(hacker, ch)) {
      do_say(hacker, "I don't deal with people I can't see!", 0, 0);
      return(TRUE);
    }

    skip_spaces(&argument);
    if (!(obj = get_obj_in_list_vis(ch, argument, ch->carrying))) {
      sprintf(buf, "You don't seem to have %s %s.\r\n", AN(argument), argument);
      send_to_char(buf, ch);
      return(TRUE);
    }
    if (GET_OBJ_TYPE(obj) != ITEM_MONEY || !GET_OBJ_VAL(obj, 1) || GET_OBJ_VAL(obj, 0) <= 0 ||
        !GET_OBJ_VAL(obj, 4) || belongs_to(ch, obj)) {
      sprintf(arg, "%s Why are you bringing this to me?", GET_CHAR_NAME(ch));
      do_say(hacker, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if (GET_OBJ_VAL(obj, 2) == 1)
      amount = (int)(GET_OBJ_VAL(obj, 0) / 8);
    else if (GET_OBJ_VAL(obj, 2) == 2)
      amount = (int)(GET_OBJ_VAL(obj, 0) / 5);
    else
      amount = (int)(GET_OBJ_VAL(obj, 0) / 3);
    sprintf(arg, "%s I'd charge about %d nuyen for that.", GET_CHAR_NAME(ch), amount);
    do_say(hacker, arg, 0, SCMD_SAYTO);
    return TRUE;
  } else if (CMD_IS("give")) {
    if (!*argument) {
      send_to_char("Give what to whom?\r\n", ch);
      return TRUE;
    }
    any_one_arg(any_one_arg(argument, buf), buf1);

    if (!(obj = get_obj_in_list_vis(ch, buf, ch->carrying))) {
      sprintf(arg, "You don't seem to have %s %s.\r\n", AN(buf), buf);
      send_to_char(arg, ch);
      return(TRUE);
    } else if (!(vict = give_find_vict(ch, buf1)))
      return TRUE;

    if (vict != hacker || !AWAKE(hacker))
      return FALSE;
    else if (!CAN_SEE(hacker, ch)) {
      do_say(hacker, "I don't deal with people I can't see!", 0, 0);
      return(TRUE);
    }

    if (GET_OBJ_TYPE(obj) != ITEM_MONEY
        || !GET_OBJ_VAL(obj, 1)
        || GET_OBJ_VAL(obj, 0) <= 0
        || !GET_OBJ_VAL(obj, 4)
        || belongs_to(ch, obj)) {
      sprintf(arg, "%s Why are you bringing this to me?", GET_CHAR_NAME(ch));
      do_say(hacker, arg, 0, SCMD_SAYTO);
      return TRUE;
    }

    if (!perform_give(ch, hacker, obj))
      return TRUE;

    if (GET_OBJ_VAL(obj, 2) == 1)
      amount = (int)(GET_OBJ_VAL(obj, 0) / 8);
    else if (GET_OBJ_VAL(obj, 2) == 2)
      amount = (int)(GET_OBJ_VAL(obj, 0) / 5);
    else
      amount = (int)(GET_OBJ_VAL(obj, 0) / 3);
    nuyen = negotiate(ch, hacker, 0, nuyen, 2, FALSE);
    nuyen = GET_OBJ_VAL(obj, 0) - amount;
    GET_BANK(hacker) += amount;
    GET_BANK(ch) += nuyen;
    sprintf(buf1, "%s Updated.  %d nuyen transferred to your bank account.",
            GET_CHAR_NAME(ch), nuyen);
    do_say(hacker, buf1, 0, SCMD_SAYTO);
    extract_obj(obj);
    return TRUE;
  }
  return FALSE;
}

SPECIAL(fence)
{
  struct char_data *fence = (struct char_data *) me;
  struct obj_data *obj;
  int value = 0;

  if (CMD_IS("sell")) {
    if (!*argument) {
      send_to_char("Sell what?\r\n", ch);
      return(TRUE);
    }
    if (!AWAKE(fence))
      return(FALSE);
    if (!CAN_SEE(fence, ch)) {
      do_say(fence, "I don't buy from someone I can't see!", 0, 0);
      return(TRUE);
    }

    skip_spaces(&argument);
    if (!(obj = get_obj_in_list_vis(ch, argument, ch->carrying))) {
      sprintf(buf, "You don't seem to have %s %s.\r\n", AN(argument), argument);
      send_to_char(buf, ch);
      return(TRUE);
    }
    if (!(GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(obj, 0) == TYPE_FILE &&
          GET_OBJ_VAL(obj, 3))) {
      act("You say, \"I only buy datafiles, chummer.\"\n", FALSE, fence, 0, 0, TO_CHAR);
      act("$n says, \"I only buy datafiles, chummer.\"\n", FALSE, fence, 0, ch, TO_VICT);
      return(TRUE);
    }
    value = negotiate(ch, fence, SKILL_DATA_BROKERAGE, market[GET_OBJ_VAL(obj, 4)], 2, FALSE);
    value /= MAX(1, (time(0) - GET_OBJ_VAL(obj, 1)) / SECS_PER_MUD_DAY);
    GET_NUYEN(ch) += value;
    market[GET_OBJ_VAL(obj, 4)] -= (int)(market[GET_OBJ_VAL(obj, 4)] * ((float)(5 - GET_OBJ_VAL(obj, 4))/ 50));
    if (market[GET_OBJ_VAL(obj, 4)] < 100)
      market[GET_OBJ_VAL(obj, 4)] = 100;
    obj_from_char(obj);
    extract_obj(obj);
    sprintf(buf, "%s says, \"Here's your %d creds.\"\r\n",
            GET_NAME(fence), value);
    send_to_char(buf, ch);
    act("You grab the chip and pay $M for it.", TRUE, fence, 0, ch, TO_CHAR);
    return TRUE;
  }
  return FALSE;
}

SPECIAL(fixer)
{
  struct char_data *fixer = (struct char_data *) me;
  struct obj_data *obj, *credstick = NULL;
  int cost;
  sh_int cash = 0, extra, hour, day = 0, pm = 0;

  if (cmd && !CMD_IS("repair") && !CMD_IS("list") && !CMD_IS("receive"))
    return FALSE;

  if (cmd && (!AWAKE(fixer) || IS_NPC(ch)))
    return FALSE;
  if (cmd && !CAN_SEE(fixer, ch)) {
    do_say(fixer, "I don't deal with someone I can't see!", 0, 0);
    return TRUE;
  }

  if (CMD_IS("repair")) {
    any_one_arg(argument, buf);
    skip_spaces(&argument);

    if (!str_cmp(buf, "cash")) {
      argument = any_one_arg(argument, buf);
      skip_spaces(&argument);
      cash = 1;
    } else if (!(credstick = get_first_credstick(ch, "credstick"))) {
      sprintf(arg, "%s You need a credstick to do that!", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if (!(obj = get_obj_in_list_vis(ch, argument, ch->carrying))) {
      sprintf(buf, "You don't seem to have %s %s.\r\n", AN(argument), argument);
      send_to_char(buf, ch);
      return TRUE;
    }
    if (IS_OBJ_STAT(obj, ITEM_CORPSE) || IS_OBJ_STAT(obj, ITEM_IMMLOAD)) {
      sprintf(arg, "%s I can't repair that.", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if (GET_OBJ_CONDITION(obj) >= GET_OBJ_BARRIER(obj)) {
      sprintf(arg, "%s %s doesn't need to be repaired!",
              GET_CHAR_NAME(ch), CAP(obj->text.name));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if ((IS_CARRYING_N(fixer) >= CAN_CARRY_N(fixer)) ||
        ((GET_OBJ_WEIGHT(obj) + IS_CARRYING_W(fixer)) > CAN_CARRY_W(fixer))) {
      sprintf(arg, "%s I've got my hands full...come back later.", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    cost = (int)((GET_OBJ_COST(obj) / (2 * GET_OBJ_BARRIER(obj))) *
                 (GET_OBJ_BARRIER(obj) - GET_OBJ_CONDITION(obj)));
    if ((cash ? GET_NUYEN(ch) : GET_OBJ_VAL(credstick, 0)) < cost) {
      sprintf(arg, "%s You can't afford to repair that!", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if (!perform_give(ch, fixer, obj))
      return TRUE;
    if (cash)
      GET_NUYEN(ch) -= cost;
    else
      GET_OBJ_VAL(credstick, 0) -= cost;
    extra = (int)((GET_OBJ_BARRIER(obj) - GET_OBJ_CONDITION(obj)) / 2);
    if (((GET_OBJ_BARRIER(obj) - GET_OBJ_CONDITION(obj)) % 2) > 0)
      extra++;
    if ((time_info.hours + extra) > 23)
      day = 1;
    else
      pm = ((time_info.hours + extra) >= 12);
    hour = ((time_info.hours + extra) % 12 == 0 ? 12 :
            (time_info.hours + extra) % 12);
    sprintf(arg, "%s That'll be %d nuyen.  Should be ready by about %d %s%s.",
            GET_CHAR_NAME(ch), cost, hour, pm ? "PM" : "AM", day ? " tomorrow" : "");
    do_say(fixer, arg, 0, SCMD_SAYTO);
    GET_OBJ_TIMER(obj) = GET_IDNUM(ch);
    fixers_need_save = 1;
    return TRUE;
  } else if (CMD_IS("list")) {
    bool found = FALSE;
    for (obj = fixer->carrying; obj; obj = obj->next_content)
      if (GET_OBJ_TIMER(obj) == GET_IDNUM(ch)) {
        if (!found) {
          sprintf(arg, "%s I currently am in possession of the following:", GET_CHAR_NAME(ch));
          do_say(fixer, arg, 0, SCMD_SAYTO);
          found = TRUE;
        }
        if (GET_OBJ_CONDITION(obj) < GET_OBJ_BARRIER(obj)) {
          hour = (int)((GET_OBJ_BARRIER(obj) - GET_OBJ_CONDITION(obj)) / 2);
          if (((GET_OBJ_BARRIER(obj) - GET_OBJ_CONDITION(obj)) % 2) > 0)
            hour++;
          send_to_char(ch, "%-59s Status: %d hour%s\r\n",
                       obj->text.name, hour, hour == 1 ? "" : "s");
        } else
          send_to_char(ch, "%-59s Stats: Ready\r\n",
                       obj->text.name);
      }
    if (!found) {
      sprintf(arg, "%s I don't have anything of yours.", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
    }
    return TRUE;
  } else if (CMD_IS("receive")) {
    int j = 0;
    char tmpvar[LINE_LENGTH], *temp = tmpvar;
    any_one_arg(argument, temp);

    if (!*temp) {
      sprintf(arg, "%s What do you want to retrieve?", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if (!(extra = get_number(&temp))) {
      sprintf(arg, "%s I don't have anything like that.", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    for (obj = fixer->carrying; obj && j <= extra; obj = obj->next_content)
      if (GET_OBJ_TIMER(obj) == GET_IDNUM(ch) &&
          isname(temp, obj->text.keywords))
        if (++j == extra)
          break;
    if (!obj) {
      sprintf(arg, "%s I don't have anything like that.", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if (GET_OBJ_CONDITION(obj) < GET_OBJ_BARRIER(obj)) {
      sprintf(arg, "%s %s isn't ready yet.", GET_CHAR_NAME(ch), CAP(obj->text.name));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if ((IS_CARRYING_N(ch) >= CAN_CARRY_N(ch)) ||
        ((GET_OBJ_WEIGHT(obj) + IS_CARRYING_W(ch)) > CAN_CARRY_W(ch))) {
      sprintf(arg, "%s You can't carry it right now.", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if (!perform_give(fixer, ch, obj)) {
      sprintf(arg, "%s That's odd...I can't let go of it.", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    fixers_need_save = 1;
    return TRUE;
  }

  // update his objects every 60 rl seconds (30 mud minutes)
  if (GET_SPARE1(fixer) >= 6) {
    for (obj = fixer->carrying; obj; obj = obj->next_content)
      if (GET_OBJ_CONDITION(obj) < GET_OBJ_BARRIER(obj))
        GET_OBJ_CONDITION(obj)++;
    GET_SPARE1(fixer) = 1;
    fixers_need_save = 1;
  } else
    GET_SPARE1(fixer)++;
  return FALSE;
}

SPECIAL(doctor_scriptshaw)
{
  if (cmd || FIGHTING(ch) || GET_POS(ch) <= POS_SLEEPING)
    return FALSE;

  if (GET_ACTIVE(ch) < 0 || GET_ACTIVE(ch) > 12)
    GET_ACTIVE(ch) = 0;

  switch (GET_ACTIVE(ch)) {
    case 0:
      do_say(ch, "Back in those days no one knew what was up there...", 0, 0);
      break;
    case 1:
      do_say(ch, "I know what is up there...", 0, 0);
      break;
    case 2:
      do_say(ch, "I saw it...", 0, 0);
      break;
    case 3:
      do_say(ch, "They say it made me go mad...", 0, 0);
      break;
    case 4:
      do_say(ch, "But I'll show them who's mad...", 0, 0);
      break;
    case 5:
      act("You cackle gleefully.", FALSE, ch, 0, 0, TO_CHAR);
      act("$n throws back his head and cackles with insane glee!",
          TRUE, ch, 0, 0, TO_ROOM);
      break;
  }
  GET_ACTIVE(ch)++;
  return FALSE;
}

SPECIAL(huge_troll)
{
  struct char_data *troll = (struct char_data *) me;
  struct obj_data *obj;

  if (CMD_IS("west") && world[troll->in_room].number == 9437 && CAN_SEE(troll, ch)) {
    for (obj = ch->carrying; obj; obj = obj->next_content)
      if (GET_OBJ_VNUM(obj) == 9412)
        break;

    if (!obj) {
      act("As you try to exit, you notice you can't get by $N.",
          FALSE, ch, 0, troll, TO_CHAR);
      act("Try as $e might, $n can't get by $N.", TRUE, ch, 0, troll, TO_NOTVICT);
      act("$n takes one look at you, then decides to find another route.",
          FALSE, ch, 0, troll, TO_VICT);
      return TRUE;
    } else {
      act("$N moves slightly, creating just enough room for you to slip by.",
          FALSE, ch, 0, troll, TO_CHAR);
      act("$N moves slightly, creating just enough room for $n to slip by.",
          FALSE, ch, 0, troll, TO_NOTVICT);
      act("You move slightly, creating just enough room for $n to slip by.",
          FALSE, ch, 0, troll, TO_VICT);
    }
  }
  return FALSE;
}

SPECIAL(purple_haze_bartender)
{
  if (cmd)
    return FALSE;

  switch (number(0, 18)) {
    case NORTH:
      if (world[ch->in_room].number == 1844 ||
          world[ch->in_room].number == 1846) {
        perform_move(ch, NORTH, CHECK_SPECIAL | LEADER, NULL);
        return TRUE;
      }
      break;
    case SOUTH:
      if (world[ch->in_room].number == 1844 ||
          world[ch->in_room].number == 1845) {
        perform_move(ch, SOUTH, CHECK_SPECIAL | LEADER, NULL);
        return TRUE;
      }
      break;
  }
  return FALSE;
}

SPECIAL(yukiya_dahoto)
{
  char_data *yukiya = (char_data *) me;

  if (yukiya != NULL && CMD_IS("open") && CAN_SEE(yukiya, ch) &&
      world[yukiya->in_room].number == YUKIYA_OFFICE) {
    skip_spaces(&argument);

    if (!str_cmp(argument, "vent")) {
      act("$n attacks, saying, \"You will not pass!  YOU WILL DIE!\"",
          FALSE, yukiya, 0, ch, TO_VICT);
      act("$n attacks $N, saying, \"You will not pass!  YOU WILL DIE!\"",
          FALSE, yukiya, 0, ch, TO_NOTVICT);
      act("You notice $N trying to sneak into the vent, and attack!",
          FALSE, yukiya, 0, ch, TO_CHAR);

      set_fighting(yukiya, ch);

      return TRUE;
    }
  }

  return FALSE;
}

SPECIAL(smiths_bouncer)
{
  struct char_data *wendigo = (char_data *) me;
  struct obj_data *obj;
  bool found = FALSE;

  if (!AWAKE(ch) || (GET_POS(ch) == POS_FIGHTING))
    return(FALSE);

  if (!cmd)
    switch(number(1,160)) {
      case 12:
        do_say(ch, "Look, no invitation, no entry. It's that simple, ya see.", 0, 0);
        return TRUE;
      case 92:
        do_say(ch, "You're a friend of who? Nice fraggin' try.", 0, 0);
        return TRUE;
      case 147:
        act("As someone tries to sneak past, $n grabs them by the collar and "
            "chucks them back through the bar.", FALSE, ch, 0, 0, TO_ROOM);
        return TRUE;
      default:
        return FALSE;
    }

  if (CMD_IS("east")) {
    for (obj = ch->carrying; obj; obj = obj->next_content)
      if (GET_OBJ_VNUM(obj) == 38094)
        found = TRUE;

    if (found)
      perform_move(ch, EAST, LEADER, NULL);
    else
      do_say(wendigo, "Hey chummer, invitation needed.", 0, 0);
    return(TRUE);
  }

  return(FALSE);
}


/* Special procedures for weapons                                    */

WSPEC(monowhip)
{
  int skill, dam_total, target = 6;

  if (dam < 1 && !number(0, 1)) {
    skill = get_skill(ch, SKILL_WHIPS_FLAILS, target);
    if (!success_test(skill, target)) {
      act("Your whip flails out of control, striking you instead of $N!", FALSE, ch, 0, vict, TO_CHAR);
      act("$n's whip completely misses and recoils to hit $m!", TRUE, ch, 0, 0, TO_ROOM);
      dam_total = convert_damage(stage(-(success_test(GET_BOD(ch) + GET_DEFENSE(ch),
                                                      GET_OBJ_VAL(weapon, 0))), GET_OBJ_VAL(weapon, 1)));


      damage(ch, ch, dam_total, TYPE_RECOIL, PHYSICAL);
      return TRUE;
    }
  }
  return FALSE;
}

/* ********************************************************************
 *  Special procedures for objects                                     *
 ******************************************************************** */
SPECIAL(vending_machine)
{
  if (!CMD_IS("buy") && !CMD_IS("list"))
    return FALSE;

  struct obj_data *obj = (struct obj_data *) me, *temp;
  int found = 0;

  if (CMD_IS("list")) {
    act("$p is able to dispense:", FALSE, ch, obj, 0, TO_CHAR);
    for (temp = obj->contains; temp; temp = temp->next_content)
      if (GET_OBJ_TYPE(temp) == ITEM_FOOD || GET_OBJ_TYPE(temp) == ITEM_DRINKCON) {
        send_to_char(ch, "    %-30s     %3d\r\n",
                     temp->text.name, GET_OBJ_COST(temp));
        found = 1;
      }
    if (!found)
      send_to_char(ch, "    Nothing!\r\n");
    return TRUE;
  } else {
    any_one_arg(argument, arg);
    for (temp = obj->contains; temp; temp = temp->next_content)
      if ((GET_OBJ_TYPE(temp) == ITEM_FOOD || GET_OBJ_TYPE(temp) == ITEM_DRINKCON) &&
          isname(arg, temp->text.keywords)) {
        if (GET_NUYEN(ch) < GET_OBJ_COST(temp)) {
          act("You can't afford $p!", FALSE, ch, temp, 0, TO_CHAR);
          return TRUE;
        }
        GET_NUYEN(ch) -= GET_OBJ_COST(temp);
        temp = read_object(GET_OBJ_RNUM(temp), REAL);
        obj_to_char(temp, ch);
        act("$n buys $p from $P.", FALSE, ch, temp, obj, TO_ROOM);
        act("You now have $p.", FALSE, ch, temp, 0, TO_CHAR);
        return TRUE;
      }
    sprintf(buf, "%s doesn't sell '%s'.\r\n", obj->text.name, arg);
    send_to_char(buf, ch);
    return TRUE;
  }
}

SPECIAL(hand_held_scanner)
{
  struct char_data *temp;
  struct obj_data *scanner = (struct obj_data *) me;
  int i, dir;

  if (!cmd || !scanner->worn_by || number(1, 10) > 4)
    return FALSE;

  if (CMD_IS("north"))
    dir = NORTH;
  else if (CMD_IS("northeast") || CMD_IS("ne"))
    dir = NORTHEAST;
  else if (CMD_IS("east"))
    dir = EAST;
  else if (CMD_IS("southeast") || CMD_IS("se"))
    dir = SOUTHEAST;
  else if (CMD_IS("south"))
    dir = SOUTH;
  else if (CMD_IS("southwest") || CMD_IS("sw"))
    dir = SOUTHWEST;
  else if (CMD_IS("west"))
    dir = WEST;
  else if (CMD_IS("northwest") || CMD_IS("nw"))
    dir = NORTHWEST;
  else if (CMD_IS("up"))
    dir = UP;
  else if (CMD_IS("down"))
    dir = DOWN;
  else
    return FALSE;

  if (world[ch->in_room].dir_option[dir] &&
      world[ch->in_room].dir_option[dir]->to_room != NOWHERE) {
    for (i = NORTH; i < NUM_OF_DIRS; i++)
      if (world[ch->in_room].dir_option[i] &&
          world[ch->in_room].dir_option[i]->to_room != NOWHERE)
        for (temp = world[world[ch->in_room].dir_option[i]->to_room].people;
             temp; temp = temp->next_in_room)
          if (temp != ch) {
            act("You feel $p vibrate momentarily.", FALSE, ch, scanner, 0, TO_CHAR);
            return FALSE;
          }
  }

  return FALSE;
}

SPECIAL(clock)
{
  struct obj_data *clock = (struct obj_data *) me;

  if (!cmd || !CAN_SEE_OBJ(ch, clock) || !AWAKE(ch))
    return FALSE;

  if (CMD_IS("time")) {
    do_time(ch, "", 0, SCMD_PRECISE);
    return TRUE;
  }
  return FALSE;
}

SPECIAL(anticoagulant)
{
  // this is handled in do_quaff (do_use), do_drink, and do_eat
  return FALSE;
}

SPECIAL(vendtix)
{
  extern struct obj_data *obj_proto;
  struct obj_data *vendtix = (struct obj_data *) me;
  int ticket;

  if (!cmd)
    return FALSE;

  if (zone_table[world[ch->in_room].zone].number == 30)
    ticket = SEATAC_TICKET;
  else
    ticket = SEATTLE_TICKET;

  if (CMD_IS("list")) {
    send_to_char(ch, "Ticket price is %d nuyen.\r\n",
                 obj_proto[real_object(ticket)].obj_flags.cost);
    act("$n presses some buttons on $p.", TRUE, ch, vendtix, 0, TO_ROOM);
    return TRUE;
  }

  if (CMD_IS("buy")) {
    if (!is_abbrev(arg, "ticket")) {
      send_to_char("This machine only sells tickets.\r\n", ch);
      return TRUE;
    }

    if ((GET_NUYEN(ch) - obj_proto[real_object(ticket)].obj_flags.cost) < 0) {
      send_to_char("You don't have enough nuyen!\r\n", ch);
      return TRUE;
    }

    struct obj_data *tobj = read_object(ticket, VIRTUAL);
    if (!tobj) {
      mudlog("No ticket for the Vend-Tix machine!", ch, LOG_SYSLOG, TRUE);
      return TRUE;
    }

    obj_to_char(tobj, ch);
    GET_NUYEN(ch) -= tobj->obj_flags.cost;
    act("You receive $p.", FALSE, ch, tobj, 0, TO_CHAR);
    act("$n buys $p from the Vend-Tix machine.",
        TRUE, ch, tobj, 0, TO_ROOM);
    return TRUE;
  }
  return FALSE;
}

SPECIAL(bank)
{
  struct obj_data *credstick;
  int amount;

  if ((CMD_IS("balance") || CMD_IS("transfer") || CMD_IS("deposit")
       || CMD_IS("withdraw") || CMD_IS("wire")) && IS_NPC(ch)) {
    send_to_char(ch, "What use do you have for a bank account?\r\n", ch);
    return FALSE;
  }

  if (CMD_IS("balance")) {
    if (GET_BANK(ch) > 0)
      sprintf(buf, "Your current balance is %ld nuyen.\r\n", GET_BANK(ch));
    else
      sprintf(buf, "Your account is empty!\r\n");
    send_to_char(buf, ch);
    return 1;
  } else if (CMD_IS("deposit")) {
    if ((amount = atoi(argument)) <= 0) {
      send_to_char("How much do you want to deposit?\r\n", ch);
      return 1;
    }
    if (!str_cmp(buf, "all"))
      amount = GET_NUYEN(ch);
    if (GET_NUYEN(ch) < amount) {
      send_to_char("You aren't carrying that much!\r\n", ch);
      return 1;
    }
    GET_NUYEN(ch) -= amount;
    GET_BANK(ch) += amount;
    sprintf(buf, "You deposit %d nuyen.\r\n", amount);
    send_to_char(buf, ch);
    act("$n accesses the ATM.", TRUE, ch, 0, FALSE, TO_ROOM);
    return 1;
  } else if (CMD_IS("withdraw")) {
    if ((amount = atoi(argument)) <= 0) {
      send_to_char("How much do you want to withdraw?\r\n", ch);
      return 1;
    }
    if (!str_cmp(buf, "all"))
      amount = GET_BANK(ch);
    if (GET_BANK(ch) < amount) {
      send_to_char("You don't have that much deposited!\r\n", ch);
      return 1;
    }
    GET_NUYEN(ch) += amount;
    GET_BANK(ch) -= amount;
    sprintf(buf, "The ATM ejects %d nuyen and updates your bank account.\r\n", amount);
    send_to_char(buf, ch);
    act("$n accesses the ATM.", TRUE, ch, 0, FALSE, TO_ROOM);
    return 1;
  } else if (CMD_IS("transfer")) {
    any_one_arg(any_one_arg(argument, buf), buf1);
    if ( ((amount = atoi(buf)) <= 0) && (str_cmp(buf,"all")) ) {
      send_to_char("How much do you want to transfer?\r\n", ch);
      return TRUE;
    }
    if (!(credstick = get_first_credstick(ch, "credstick"))) {
      send_to_char("You need a personalized credstick to do that!\r\n", ch);
      return TRUE;
    }
    if (!str_cmp(buf1, "account")) {
      if (!str_cmp(buf,"all")) {
        amount = GET_OBJ_VAL(credstick, 0);
      }
      if (GET_OBJ_VAL(credstick, 0) < amount) {
        act("$p doesn't even have that much!", FALSE, ch, credstick, 0, TO_CHAR);
        return TRUE;
      }
      GET_OBJ_VAL(credstick, 0) -= amount;
      GET_BANK(ch) += amount;
      sprintf(buf, "%d nuyen transferred from $p to your account.", amount);
    } else if (!str_cmp(buf1, "credstick")) {
      if (!str_cmp(buf,"all")) {
        amount = GET_BANK(ch);
      }
      if (GET_BANK(ch) < amount) {
        send_to_char("You don't have that much deposited!\r\n", ch);
        return TRUE;
      }
      GET_OBJ_VAL(credstick, 0) += amount;
      GET_BANK(ch) -= amount;
      sprintf(buf, "%d nuyen transferred from your account to $p.", amount);
    } else {
      send_to_char("Transfer to what? (Type out \"credstick\" or \"account\", please.)\r\n", ch);
      return TRUE;
    }
    act(buf, FALSE, ch, credstick, 0, TO_CHAR);
    act("$n accesses the ATM.", TRUE, ch, 0, 0, TO_ROOM);
    return TRUE;
  } else if (CMD_IS("wire")) {
    any_one_arg(any_one_arg(argument, buf), buf1);
    if ((amount = atoi(buf)) <= 0)
      send_to_char("How much do you wish to wire?\r\n", ch);
    else if (amount > GET_BANK(ch))
      send_to_char("You don't have that much nuyen.\r\n", ch);
    else if (!*buf1)
      send_to_char("Who do you want to wire funds to?\r\n", ch);
    else {
      struct char_data *vict = NULL;
      long isfile = FALSE;
      if (!(vict = get_player_vis(ch, buf1, FALSE)))
        if ((isfile = get_player_id(buf1)) == -1) {
          send_to_char("It won't let you transfer to that account\r\n", ch);
          return TRUE;
        }
      wire_nuyen(ch, vict, amount, isfile);
      send_to_char(ch, "You wire %d nuyen to %s's account.\r\n", amount, vict ? GET_CHAR_NAME(vict) : get_player_name(isfile));
    }
    return TRUE;
  }
  return 0;
}

SPECIAL(toggled_invis)
{
  struct obj_data *obj = (struct obj_data *) me;

  if(!CMD_IS("activate") || !obj->worn_by)
    return FALSE;
  else {
    if(str_cmp(argument, "invis")) {
      if (AFF_FLAGGED(obj->worn_by, AFF_IMP_INVIS)) {
        AFF_FLAGS(obj->worn_by).RemoveBit(AFF_IMP_INVIS);
        send_to_char(ch, "Your suit goes silent as the cloaking device shuts off.\r\n");
        act("The air shimmers briefly as $n fades into view.\r\n", FALSE, ch, 0, 0, TO_ROOM);
        return TRUE;
      } else {
        AFF_FLAGS(obj->worn_by).SetBit(AFF_IMP_INVIS);
        send_to_char(ch, "Your suit whirs quietly as the cloaking device kicks in.\r\n");
        act("The world bends around $n as they vanish from sight.\r\n", FALSE, ch, 0, 0, TO_ROOM);
        return TRUE;
      }
    }
  }
  return FALSE;
}

SPECIAL(traffic)
{
  struct room_data *room = (struct room_data *) me;

  if (!cmd && room->people)
    switch (number(1, 50)) {
      case 1:
        send_to_room("A man on a Yamaha Rapier zips by.\r\n",
                     real_room(room->number));
        break;
      case 2:
        send_to_room("A Mitsuhama Nightsky limousine slowly drives by.\r\n",
                     real_room(room->number));
        break;
      case 3:
        send_to_room("A Ford Bison drives through here, splashing mud on you.\r\n",
                     real_room(room->number));
        break;
      case 4:
        if (zone_table[room->zone].jurisdiction == 0)
          send_to_room("A Lone Star squad car drives by, sirens blaring loudly.\r\n",
                       real_room(room->number));
        break;
      case 5:
        send_to_room("An orkish woman drives through here on her Harley Scorpion.\r\n",
                     real_room(room->number));
        break;
      case 6:
        send_to_room("An elf drives through here on his decked-out Yamaha Rapier.\r\n",
                     real_room(room->number));
        break;
      case 7:
        send_to_room("A ^rred^n Chrysler-Nissan Jackrabbit cruises by.\r\n",
                     real_room(room->number));
        break;
      case 8:
        send_to_room("A ^yyellow^n Chrysler-Nissan Jackrabbit cruises by.\r\n",
                     real_room(room->number));
        break;
      case 9:
        send_to_room("A ^Wwhite^n Chrysler-Nissan Jackrabbit cruises by.\r\n",
                     real_room(room->number));
        break;
      case 10:
        send_to_room("A ^rred^n Ford Americar cruises by.\r\n",
                     real_room(room->number));
        break;
      case 11:
        send_to_room("A ^yyellow^n Ford Americar cruises by.\r\n",
                     real_room(room->number));
        break;
      case 12:
        send_to_room("A ^Wwhite^n Ford Americar cruises by.\r\n",
                     real_room(room->number));
        break;
      case 13:
        send_to_room("A ^Bblue^n Ford Americar cruises by.\r\n",
                     real_room(room->number));
        break;
      case 14:
        send_to_room("A ^Bblue^n Chrysler-Nissan Jackrabbit cruises by.\r\n",
                     real_room(room->number));
        break;
      case 15:
        send_to_room("A ^Rcherry red^n Eurocar Westwind 2000 flies past you.\r\n",
                     real_room(room->number));
        break;
      case 16:
        send_to_room("A ^Wwhite^n Mitsubishi Runabout drives by slowly.\r\n",
                     real_room(room->number));
        break;
      case 17:
        send_to_room("A ^bblue^n Mitsuhama Runabout drives by slowly.\r\n",
                     real_room(room->number));
        break;
      case 18:
        send_to_room("An elven woman on a Dodge Scoot passes through here.\r\n",
                     real_room(room->number));
        break;
      case 19:
        send_to_room("A ^Ybright yellow^n Volkswagen Electra passes by silently.\r\n",
                     real_room(room->number));
        break;
      case 20:
        send_to_room("A huge troll rides by on a modified BMW Blitzen 2050.\r\n",
                     real_room(room->number));
        break;
      case 21:
        send_to_room("A large, ^Wwhite^n GMC Bulldog van drives through here.\r\n",
                     real_room(room->number));
        break;
    }
  return FALSE;
}

SPECIAL(oceansounds)
{
  struct room_data *room = (struct room_data *) me;

  if (!cmd && room->people)
    switch (number(1, 100)) {
      case 1:
        send_to_room("A cool breeze blows over the ocean sending ripples across the water.\r\n",
                     real_room(room->number));
        break;
      case 2:
        send_to_room("The cries of seagulls fill the air.\r\n",
                     real_room(room->number));
        break;
      case 3:
        send_to_room("A lone bird skims across the surface of the water.\r\n",
                     real_room(room->number));
        break;
      case 4:
        send_to_room("Water splashes as a fish disturbs the surface of a wave.\r\n",
                     real_room(room->number));
        break;
      case 5:
        send_to_room("The waves continue their endless rhythm towards the shore.\r\n",
                     real_room(room->number));
    }

  return FALSE;
}

SPECIAL(neophyte_entrance)
{
  if (!cmd)
    return FALSE;

  if (CMD_IS("drag")) {
    send_to_char("You can't drag people here.\r\n", ch);
    return TRUE;
  }
  if ((CMD_IS("south") || CMD_IS("enter")) && !PLR_FLAGGED(ch, PLR_NEWBIE)
      && !(IS_SENATOR(ch))) {
    send_to_char("The barrier prevents you from entering the guild.", ch);
    send_to_char("NOTE: You may only visit the training grounds until you have received 26 karma.", ch);
    act("$n stumbles into the barrier covering the entrance.", FALSE, ch, 0, 0, TO_ROOM);
    return TRUE;
  }
  return FALSE;
}

SPECIAL(simulate_bar_fight)
{
  struct room_data *room = (struct room_data *) me;
  struct char_data *vict;
  int dam;

  if (cmd || number(0, 1))
    return FALSE;

  for (vict = room->people; vict; vict = vict->next_in_room)
    if (!IS_NPC(vict) && GET_POS(vict) > POS_RESTING && !number(0, 4))
      break;

  if (!vict)
    return FALSE;
  act("A chair flies across the room, hitting $n square in the head!",
      TRUE, vict, 0, 0, TO_ROOM);
  act("A chair flies across the room, hitting you square in the head!",
      TRUE, vict, 0, 0, TO_CHAR);
  dam = convert_damage(stage(-success_test(GET_WIL(vict), 4), MODERATE));
  damage(vict, vict, dam, 0, FALSE);
  return TRUE;
}

SPECIAL(waterfall)
{
  if (CMD_IS("northeast") || CMD_IS("northwest") || CMD_IS("ne") || CMD_IS("nw")) {
    if (success_test(GET_STR(ch), 10)) {
      act("You push your way through the rushing water and tumble into a vast cavern.",
          FALSE, ch, 0, 0, TO_CHAR);
      act("$n pushes $s way through the waterfall and disappears.", TRUE, ch, 0, 0, TO_ROOM);
      return FALSE;
    } else {
      act("You succumb to the heavy waves and crack your skull on the floor!", FALSE, ch, 0, 0, TO_CHAR);
      act("$n gets slammed down by the waves and hits $s head on the floor!", TRUE, ch, 0, 0, TO_ROOM);
      damage(ch, ch, number(1, 2), 0, TRUE);
      return TRUE;
    }
  }
  return FALSE;
}

SPECIAL(crime_mall_blockade)
{
  if (!cmd)
    return FALSE;
  int found = 0;
  struct char_data *temp;

  for (temp = world[ch->in_room].people; temp && !found; temp = temp->next_in_room)
    if (IS_NPC(temp) && GET_MOB_VNUM(temp) == 10022)
      found = 1;
  if (!found)
    if ((world[ch->in_room].number == 10075 && CMD_IS("east")) || (world[ch->in_room].number == 10077 &&
                                                                   CMD_IS("west"))) {
      act("There seems to be an invisible barrier of some kind...", FALSE, ch, 0, 0, TO_CHAR);
      return TRUE;
    }
  return FALSE;
}

SPECIAL(circulation_fan)
{
  static bool running = true;
  room_data *room = (struct room_data *) me;

  if (cmd)
    return false;

  if (running) {
    if (CMD_IS("north") && ch != NULL && !IS_ASTRAL(ch) && !IS_NPC(ch)) {
      act("\"Sharp, whirling metal fan blades can't hurt!\", "
          "you used to think...", FALSE, ch, 0, 0, TO_CHAR);
      act("A mist of $n's warm blood falls on you as $e walks into the fan.",
          FALSE, ch, 0, 0, TO_ROOM);

      // Deathlog Addendum
      sprintf(buf,"%s got chopped up into tiny bits. {%s (%ld)}",
              GET_CHAR_NAME(ch),
              world[ch->in_room].name, world[ch->in_room].number );
      mudlog(buf, ch, LOG_DEATHLOG, TRUE);

      die(ch);

      return true;
    } else if (!cmd) {
      if (room->people != NULL) {
        act("The fan shuts off to save energy, leaving the duct "
            "to the north open.", FALSE, room->people, 0, 0, TO_CHAR);
        act("The fan shuts off to save energy, leaving the duct "
            "to the north open.", FALSE, room->people, 0, 0, TO_ROOM);
      }

      running = false;
    }
  } else {
    if (room->people != NULL) {
      act("A loud hum signals the power-up of the fan.",
          FALSE, room->people, 0, 0, TO_CHAR);
      act("A loud hum signals the power-up of the fan.",
          FALSE, room->people, 0, 0, TO_ROOM);
    }

    running = true;
  }

  return false;
}

SPECIAL(newbie_car)
{
  struct veh_data *veh;
  struct obj_data *obj;
  int num = 0;

  if (!cmd)
    return FALSE;

  if (CMD_IS("collect")) {
    any_one_arg(argument, arg);
    if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
      send_to_char(ch, "You don't have a deed for that.\r\n");
      return TRUE;
    }
    if (GET_OBJ_VNUM(obj) < 891 || GET_OBJ_VNUM(obj) > 898 || GET_OBJ_VNUM(obj) == 896) {
      send_to_char(ch, "You can't collect anything with that.\r\n");
      return TRUE;
    }
    if (ch->in_veh) {
      send_to_char("You cannot collect a vehicle while in another vehicle.\r\n", ch);
      return TRUE;
    }
    switch (GET_OBJ_VNUM(obj)) {
      case 891:
        num = 1305;
        break;
      case 892:
        num = 1307;
        break;
      case 893:
        num = 1302;
        break;
      case 894:
        num = 1320;
        break;
      case 895:
        num = 1308;
        break;
      case 897:
        num = 1309;
        break;
      case 898:
        num = 1303;
        break;
    }
    veh = read_vehicle(num, VIRTUAL);
    veh->locked = TRUE;
    veh->owner = GET_IDNUM(ch);
    veh_to_room(veh, ch->in_room);
    veh->idnum = number(0, 1000000);  // TODO: why is this not unique
    veh->flags.SetBit(VFLAG_NEWBIE);
    sprintf(buf, "%s is wheeled out into the garage.\r\n", veh->short_description);
    send_to_room(buf, ch->in_room);
    obj_from_char(obj);
    extract_obj(obj);
    save_vehicles();
    return TRUE;
  }
  return FALSE;
}

void Crash_rent_deadline(struct char_data * ch, struct char_data * recep,
                         long cost)
{
  long rent_deadline;

  if (!cost)
    return;

  rent_deadline = ((GET_NUYEN(ch) + GET_BANK(ch)) / cost);
  if (rent_deadline >= 50)
    sprintf(buf, "$n tells you, \"You can rent forever with the nuyen you have.\"");
  else
    sprintf(buf, "$n tells you, \"You can rent for %ld night%s with the nuyen you have.\r\n",
            rent_deadline, (rent_deadline > 1) ? "s" : "");
  act(buf, FALSE, recep, 0, ch, TO_VICT);
}

int find_hotel_cost(struct char_data *ch)
{
  float cost = 0.0;
  int val;

  switch (GET_LOADROOM(ch))
  {
    case 1939:
    case 1940:
      cost = 11.0;
      break;
    case 2126:
    case 2127:
      cost = 6.25;
      break;
    case 60565:
    case 60599:
    case 70760:
    case 70769:
      cost = 1.4;
      break;
    case 9990:
    case 9991:
      cost = 5.5;
      break;
    case 18947:
    case 18949:
      cost = 12.0;
      break;
    case 14625:
    case 14626:
      cost = 6.0;
      break;
    default:
      sprintf(buf, "SYSERR: Invalid loadroom %ld specified to find_hotel_cost. Defaulting to 6.0.", GET_LOADROOM(ch));
      mudlog(buf, NULL, LOG_SYSLOG, FALSE);
      cost = 6.0;
      break;
  }
  val = (int)(cost * MAX(1, (GET_REP(ch) + number(-1, 1)) / 10));
  return val;
}

int find_hotel_room(int room)
{
  switch (room) {
    case 1939:
      return 1940;
    case 2126:
      return 2127;
    case 60565:
      return 60599;
    case 9990:
      return 9991;
    case 70760:
      return 70759;
  }
  return room;
}

int Crash_offer_rent(struct char_data * ch, struct char_data * receptionist,
                     int display, int factor)
{
  char buf[MAX_INPUT_LENGTH];
  long totalcost = 0, i;

  i = GET_LOADROOM(ch);
  GET_LOADROOM(ch) = world[ch->in_room].number;

  totalcost = find_hotel_cost(ch);

  GET_LOADROOM(ch) = i;

  if (display)
  {
    sprintf(buf, "$n tells you, \"Plus, my %d credit fee..\"", 50 * factor);
    act(buf, FALSE, receptionist, 0, ch, TO_VICT);
    sprintf(buf, "$n tells you, \"For a total of %ld nuyen%s.\"",
            totalcost, (factor == RENT_FACTOR ? " per day" : ""));
    act(buf, FALSE, receptionist, 0, ch, TO_VICT);
    if (totalcost > GET_NUYEN(ch)) {
      act("$n tells you, '...which I see you can't afford.'",
          FALSE, receptionist, 0, ch, TO_VICT);
      return (0);
    } else if (factor == RENT_FACTOR)
      Crash_rent_deadline(ch, receptionist, totalcost);
  }
  return (totalcost);
}

int gen_receptionist(struct char_data * ch, struct char_data * recep,
                     int cmd, char *arg, int mode)
{
  int cost = 0;
  vnum_t save_room;
  const char *action_table[] = {"smile", "dance", "sigh", "blush", "burp", "cough",
    "fart", "twiddle", "yawn"};
  ACMD(do_action);

  if (!ch->desc || IS_NPC(ch))
    return FALSE;

  if (!cmd && !number(0, 5))
  {
    char empty_argument = '\0';
    do_action(recep, &empty_argument, find_command(action_table[number(0, 8)]), 0);
    return FALSE;
  }
  if (!CMD_IS("offer") && !CMD_IS("rent"))
    return FALSE;
  if (!AWAKE(recep))
  {
    send_to_char("Sleeping receptionists aren't much help, I'm afraid...\r\n", ch);
    return TRUE;
  }
  if (!CAN_SEE(recep, ch))
  {
    do_say(recep, "I don't deal with people I can't see!", 0, 0);
    return TRUE;
  }
  if (!(cost = Crash_offer_rent(ch, recep, FALSE, mode)))
    cost = 5;
  if (CMD_IS("offer"))
  {
    sprintf(buf, "$n tells you, \"Rooms here are %d nuyen per night.\"", cost);
    act(buf, FALSE, recep, 0, ch, TO_VICT);
    return TRUE;
  }
  if (mode == RENT_FACTOR)
    sprintf(buf, "$n tells you, \"Rooms are %d nuyen per night.\"", cost);
  act(buf, FALSE, recep, 0, ch, TO_VICT);
  if (cost > GET_NUYEN(ch))
  {
    act("$n tells you, \"...which I see you can't afford.\"", FALSE, recep, 0, ch, TO_VICT);
    return TRUE;
  }
  if (cost && (mode == RENT_FACTOR))
    Crash_rent_deadline(ch, recep, cost);

  save_room = find_hotel_room(world[ch->in_room].number);
  GET_LOADROOM(ch) = save_room;

  if (mode == RENT_FACTOR)
  {
    act("$n gives you a key and shows you to your room.", FALSE, recep, 0, ch, TO_VICT);
    sprintf(buf, "%s has rented at %ld", GET_CHAR_NAME(ch), world[ch->in_room].number);
    mudlog(buf, ch, LOG_CONNLOG, TRUE);
    act("$n helps $N into $S room.", FALSE, recep, 0, ch, TO_NOTVICT);
  }

  if (ch->desc && !IS_SENATOR(ch))
    STATE(ch->desc) = CON_QMENU;
  extract_char(ch);

  return TRUE;
}

SPECIAL(receptionist)
{
  return (gen_receptionist(ch, (struct char_data *)me, cmd, argument, RENT_FACTOR));
}

SPECIAL(smelly)
{
  struct char_data *smelly = (struct char_data *) me;

  if (cmd)
    return FALSE;

  if (!FIGHTING(smelly)) {
    switch (number(0, 60)) {
      case 0:
        do_say(smelly, "Spare any change for beetles?", 0, 0);
        return TRUE;
      case 1:
        do_say(smelly, "Spare a few nuyen for a beer?", 0, 0);
        return TRUE;
      case 2:
        do_say(smelly, "Spare a few nuyen for Sisters of the Road?", 0, 0);
        return TRUE;
      case 3:
        do_say(smelly, "Spare any change for a motel coffin?", 0, 0);
        return TRUE;
    }
    return FALSE;
  }

  return FALSE;
}

void make_newbie(struct obj_data *obj)
{
  for (;obj;obj = obj->next_content)
  {
    if (obj->contains)
      make_newbie(obj->contains);
    if (GET_OBJ_TYPE(obj) != ITEM_MAGIC_TOOL) {
      GET_OBJ_EXTRA(obj).SetBits(ITEM_NODONATE, ITEM_NOSELL, ENDBIT);
      GET_OBJ_COST(obj) = 0;
    }
  }
}
SPECIAL(auth_room)
{
  if ((CMD_IS("say") || CMD_IS("'")) && !IS_ASTRAL(ch)) {
    skip_spaces(&argument);
    if (!strcmp("I have read the rules and policies, understand them, and agree to abide by them during my stay here.", argument)) {
      PLR_FLAGS(ch).RemoveBit(PLR_AUTH);
      GET_NUYEN(ch) = 0;
      make_newbie(ch->carrying);
      for (int i = 0; i < NUM_WEARS; i++)
        if (GET_EQ(ch, i))
          make_newbie(GET_EQ(ch, i));
      for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content)
        GET_OBJ_COST(obj) = 1;
      for (struct obj_data *obj = ch->bioware; obj; obj = obj->next_content)
        GET_OBJ_COST(obj) = 1;
      char_from_room(ch);
      char_to_room(ch, real_room(RM_NEWBIE_LOBBY));
      send_to_char(ch, "^YYou are now Authorized. Welcome to Awakened Worlds.^n\r\n");
      if (real_object(OBJ_NEWBIE_RADIO)>-1)
      {
        struct obj_data *radio = read_object(OBJ_NEWBIE_RADIO, VIRTUAL);
        GET_OBJ_VAL(radio, 0) = 8;
        obj_to_char(radio, ch);
        send_to_char(ch, "You have been given a radio.^n\r\n");
      }
      sprintf(buf, "DELETE FROM pfiles_chargendata WHERE idnum=%ld;", GET_IDNUM(ch));
      mysql_wrapper(mysql, buf);
    }
  }
  return FALSE;
}

SPECIAL(room_damage_radiation)
{
  struct room_data *room = (struct room_data *) me;
  struct char_data *next = NULL;
  int rad_dam = 0;
  if (!ch)
    for (struct char_data *vict = room->people; next; vict = next) {
      next = vict->next_in_room;
      if (!success_test(GET_BOD(vict), 8) && number(1, 3) == 3) {
        rad_dam = number(1,3);
        if (rad_dam == 3) {
          act("Your entire body is wracked with an intense inner heat as your nose begins to bleed and blood pours freely from your mouth.", FALSE, vict, 0, 0, TO_CHAR);
          act("$n begins dripping a horrible amount of blood from their nose and mouth.", TRUE, vict, 0, 0, TO_ROOM);
        } else if (rad_dam == 2) {
          act("You cough violently, feeling your lungs burn and your skin crawl with heat as you spit up a mouthful of blood.", FALSE, vict, 0, 0, TO_CHAR);
          act("$n coughs violently, spitting a bit of blood onto the floor.", TRUE, vict, 0, 0, TO_ROOM);
        } else {
          act("You feel itchy all over and your eyes become very irritated.  You get the metallic taste of pennies in the back of your throat.",FALSE, vict, 0, 0, TO_CHAR);
          act("$n suddenly doesn't look well at all, seems they're developing a rash.", TRUE, vict, 0, 0, TO_ROOM);
        }
        damage(vict,vict,rad_dam,0, TRUE);
      }
    }
  return FALSE;
}

SPECIAL(terell_davis)
{
  extern SPECIAL(shop_keeper);
  if (!AWAKE(ch))
    return FALSE;
  else if (cmd) {
    if (CMD_IS("buy") || CMD_IS("sell") || CMD_IS("value") || CMD_IS("list") || CMD_IS("check")
        || CMD_IS("cancel") || CMD_IS("receive") || CMD_IS("info")) {
      shop_keeper(ch, me, cmd, argument);
      return TRUE;
    }
    return FALSE;
  } else if (time_info.hours == 7) {
    int toroom = NOWHERE;
    switch (number(0, 5)) {
      case 0:
        /* toroom = real_room(5008);
        break; */
      case 1:
        /* toroom = real_room(3104);
        break; */
      case 2:
        /* toroom = real_room(39854);
         break; */
      case 3:
        /* toroom = real_room(16227);
        break; */
      case 4:
        /* toroom = real_room(2113);
        break; */
      case 5:
        toroom = real_room(RM_DANTES_DESCENT);
        break;
    }
    act("$n finishes up his business and walks out of the club.", FALSE, ch, 0, 0, TO_ROOM);
    char_from_room(ch);
    char_to_room(ch, toroom);
    act("$n walks into the club, ready to sell his wares.", FALSE, ch, 0, 0, TO_ROOM);
  }
  return FALSE;
}

SPECIAL(desktop)
{
  struct obj_data *obj = (struct obj_data *) me;
  if (!CMD_IS("list") || (!obj->in_veh && obj->in_room == NOWHERE))
    return FALSE;
  send_to_char(ch, "%s (%d/%d)", obj->text.name, GET_OBJ_VAL(obj, 2) - GET_OBJ_VAL(obj, 3), GET_OBJ_VAL(obj, 2));
  if (obj->contains) {
    send_to_char(ch, " contains:\r\n");
    for (struct obj_data *soft = obj->contains; soft; soft = soft->next_content) {
      if (GET_OBJ_TYPE(soft) == ITEM_DESIGN)
        send_to_char(ch, "%-40s %dMp (%dMp taken) %2.2f%% Complete\r\n", soft->restring, GET_OBJ_VAL(soft, 6),
                     GET_OBJ_VAL(soft, 6) + (GET_OBJ_VAL(soft, 6) / 10),
                     GET_OBJ_TIMER(soft) ? (GET_OBJ_VAL(soft, 5) ?
                                            ((float)(GET_OBJ_TIMER(soft) - GET_OBJ_VAL(soft, 5)) / GET_OBJ_TIMER(soft)) * 100 :
                                            ((float)(GET_OBJ_TIMER(soft) - GET_OBJ_VAL(soft, 4)) / GET_OBJ_TIMER(soft)) * 100) : 0);
      else
        send_to_char(ch, "%-40s %dMp (%dMp taken) Rating %d\r\n", soft->restring ? soft->restring :
                     soft->text.name, GET_OBJ_VAL(soft, 2), GET_OBJ_VAL(soft, 2), GET_OBJ_VAL(soft, 1));
    }
  } else
    send_to_char(ch, " is empty.\r\n");
  return TRUE;
}

SPECIAL(bouncy_castle)
{
  struct room_data *room = (struct room_data *) me;

  if (!cmd && room->people)
    switch (number(1, 20)) {
      case 5:
        send_to_room("You bounce lightly along the floor.\r\n",real_room(room->number));
        break;
      case 10:
        send_to_room("You bounce good and high, getting a lot of air.\r\n",real_room(room->number));
        break;
      case 15:
        send_to_room("You bounce into one of the walls and then fall on your ass, only to bounce back onto your feet.\r\n",real_room(room->number));
        break;
      case 20:
        send_to_room("You bounce really hard and smack your head off the ceiling.\r\n",real_room(room->number));
        break;
    }
  return FALSE;
}

SPECIAL(rpe_room)
{
  if ((CMD_IS("southwest") || CMD_IS("sw")) && !(PLR_FLAGGED(ch, PLR_RPE) || GET_LEVEL(ch) > 1)) {
    send_to_char(ch, "You don't need to go to the bathroom.\r\n");
    return TRUE;
  }
  return FALSE;
}

SPECIAL(matchsticks)
{
  struct char_data *carl = (struct char_data *) me;
  if (!AWAKE(carl))
    return FALSE;
  if (CMD_IS("give")) {
    half_chop(argument, arg, buf);
    two_arguments(buf, buf1, buf2);

    if (!*buf || !*buf1 || !*buf2)
      return FALSE;

    if (str_cmp("carl", buf2))
      return FALSE;

    if (str_cmp("nuyen", buf1)) {
      act("$n gives $N a strange look and nods once in $S direction.", TRUE, carl, 0, ch, TO_ROOM);
      return TRUE;
    }
    int amount = atoi(arg);
    if (amount < 15000 || GET_NUYEN(ch) < amount) {
      act("$n gives $N a strange look and nods once in $S direction.", TRUE, carl, 0, ch, TO_ROOM);
      return TRUE;
    } else {
      if (amount == 15000)
        act("$n nods once then digs briefly behind the bar, bringing out a small card and handing it to $N.", TRUE, carl, 0, ch, TO_ROOM);
      else
        act("$n nods once then digs briefly behind the bar, bringing out a small card and handing it to $N, smirking.", TRUE, carl, 0, ch, TO_ROOM);
      struct obj_data *obj = read_object(31714, VIRTUAL);
      obj_to_char(obj, ch);
      GET_NUYEN(ch) -= amount;
      return TRUE;
    }
  }
  return FALSE;
}

SPECIAL(portable_gridguide)
{
  struct obj_data *obj = (struct obj_data *) me;
  if (CMD_IS("gridguide") && !ch->in_veh) {
    if (!(ROOM_FLAGGED(ch->in_room, ROOM_ROAD) || ROOM_FLAGGED(ch->in_room, ROOM_GARAGE)) ||
        ROOM_FLAGGED(ch->in_room, ROOM_NOGRID))
      send_to_char(ch, "The %s flashes up with ^RINVALID LOCATION^n.\r\n", GET_OBJ_NAME(obj));
    else {
      int x = world[ch->in_room].number - (world[ch->in_room].number * 3), y = world[ch->in_room].number + 100;
      send_to_char(ch, "The %s flashes up with ^R%d, %d^n.\r\n", GET_OBJ_NAME(obj), x, y);
    }
    return TRUE;
  }
  return FALSE;
}

SPECIAL(painter)
{
  struct veh_data *veh = NULL;
  struct char_data *painter = (struct char_data *) me;
  extern void vehcust_menu(struct descriptor_data *d);
  if ((veh = world[painter->in_room+1].vehicles))
    if (veh->spare <= time(0)) {
      veh_from_room(veh);
      veh_to_room(veh, real_room(RM_PAINTER_LOT));
      veh->spare = 0;
      if (world[real_room(RM_PAINTER_LOT)].people) {
        sprintf(buf, "%s is wheeled out into the parking lot.", GET_VEH_NAME(veh));
        act(buf, FALSE, world[real_room(RM_PAINTER_LOT)].people, 0, 0, TO_ROOM);
        act(buf, FALSE, world[real_room(RM_PAINTER_LOT)].people, 0, 0, TO_CHAR);
        save_vehicles();
      }
    }
  if (!CMD_IS("paint"))
    return FALSE;
  skip_spaces(&argument);
  if (!*argument) {
    do_say(painter, "I'll paint any vehicle for 20000 nuyen upfront.", 0, 0);
    return TRUE;
  }
  if (world[painter->in_room+1].vehicles) {
    do_say(painter, "We're already working on someones ride, bring it back later.", 0, 0);
    return TRUE;
  }
  if (!(veh = get_veh_list(argument, world[ch->in_room].vehicles, ch)))
    do_say(painter, "What do you want me to paint?", 0, 0);
  else if (veh->owner != GET_IDNUM(ch))
    do_say(painter, "Bring me your own ride and I'll paint it.", 0, 0);
  else if (GET_NUYEN(ch) < 20000)
    do_say(painter, "You don't have the nuyen for that.", 0, 0);
  else {
    veh->spare = time(0) + (SECS_PER_MUD_DAY * 3);
    ch->desc->edit_veh = veh;
    veh_from_room(veh);
    veh_to_room(veh, painter->in_room+1);
    GET_NUYEN(ch) -= 20000;
    sprintf(buf, "%s is wheeled into the painting booth.", GET_VEH_NAME(veh));
    act(buf, FALSE, painter, 0, 0, TO_ROOM);
    if (!veh->restring)
      veh->restring = str_dup(veh->short_description);
    if (!veh->restring_long)
      veh->restring_long = str_dup(veh->long_description);
    STATE(ch->desc) = CON_VEHCUST;
    PLR_FLAGS(ch).SetBit(PLR_WRITING);
    vehcust_menu(ch->desc);
  }
  return TRUE;
}

SPECIAL(multnomah_gate)
{
  if (!cmd)
    return FALSE;
  long in_room = ch->in_veh ? ch->in_veh->in_room : ch->in_room, to_room = 0;
  if (world[in_room].number == RM_MULTNOMAH_GATE_NORTH)
    to_room = real_room(RM_MULTNOMAH_GATE_SOUTH);
  else to_room = real_room(RM_MULTNOMAH_GATE_NORTH);

  if ((world[in_room].number == RM_MULTNOMAH_GATE_NORTH && CMD_IS("south")) || (world[in_room].number == RM_MULTNOMAH_GATE_SOUTH && CMD_IS("north"))) {
    if (!PLR_FLAGGED(ch, PLR_VISA)) {
      send_to_char("The gate refuses to open for you.\r\n", ch);
      return TRUE;
    } else if (ch->in_veh) {
      for (struct char_data *vict = ch->in_veh->people; vict; vict = vict->next_in_veh)
        if (vict != ch && !PLR_FLAGGED(vict, PLR_VISA)) {
          send_to_char("The guards won't open the gate until everyone has shown their visas.\r\n", ch);
          return TRUE;
        }
    }
    send_to_char("The gate slides open just enough for you to pass through.\r\n", ch);
    if (ch->in_veh) {
      sprintf(buf, "The gate slides open allowing %s to pass through.\r\n", GET_VEH_NAME(ch->in_veh));
      send_to_room(buf, ch->in_veh->in_room);
      for (struct char_data *vict = ch->in_veh->people; vict; vict = vict->next_in_veh)
        PLR_FLAGS(vict).RemoveBit(PLR_VISA);
      veh_from_room(ch->in_veh);
      veh_to_room(ch->in_veh, to_room);
    }
    else {
      act("The gate opens allowing $n to pass through.\r\n", FALSE, ch, 0, 0, TO_ROOM);
      PLR_FLAGS(ch).RemoveBit(PLR_VISA);
      char_from_room(ch);
      char_to_room(ch, to_room);
    }
    return TRUE;
  }

  return FALSE;
}

SPECIAL(multnomah_guard)
{
  if (cmd && CMD_IS("show")) {
    skip_spaces(&argument);
    struct char_data *guard = (struct char_data *) me;
    struct obj_data *visa = get_obj_in_list_vis(ch, argument, ch->carrying);
    if (visa && GET_OBJ_VNUM(visa) == OBJ_MULTNOMAH_VISA) {
      if (GET_OBJ_VAL(visa, 0) == GET_IDNUM(ch)) {
        PLR_FLAGS(ch).SetBit(PLR_VISA);
        sprintf(arg, "%s Everything seems to be in order, proceed.", GET_CHAR_NAME(ch));
        do_say(guard, arg, 0, SCMD_SAYTO);
      } else {
        sprintf(arg, "%s Get out of here, before we arrest you.", GET_CHAR_NAME(ch));
        extract_obj(visa);
        do_say(guard, arg, 0, SCMD_SAYTO);
      }
      return TRUE;
    } else return FALSE;
  }
  return FALSE;
}

SPECIAL(pocket_sec)
{
  struct obj_data *sec = (struct obj_data *) me;
  extern void pocketsec_menu(struct descriptor_data *ch);
  if (*argument && cmd && CMD_IS("use")) {
    skip_spaces(&argument);
    if ((isname(argument, sec->text.keywords) || isname(argument, sec->text.name) ||
         (sec->restring && isname(argument, sec->restring))) && (sec->carried_by == ch || sec->worn_by == ch)) {
      if (FIGHTING(ch))
        send_to_char("You're too busy to do that.\r\n", ch);
      else if (AFF_FLAGGED(ch, AFF_PILOT))
        send_to_char("You are too scared of The Star adding demerit points to your license.\r\n", ch);
      else if (GET_OBJ_VAL(sec, 1) > 0 && GET_OBJ_VAL(sec, 1) != GET_IDNUM(ch))
        send_to_char("You don't know the password.\r\n", ch);
      else {
        act("$n starts looking at $p.", TRUE, ch, sec, 0, TO_ROOM);
        STATE(ch->desc) = CON_POCKETSEC;
        ch->desc->edit_obj = sec;
        pocketsec_menu(ch->desc);
      }
      return TRUE;
    }
  }
  return FALSE;
}

SPECIAL(bouncer_troll)
{
  if (!cmd)
    return FALSE;

  struct char_data *troll = (struct char_data *) me;
  if (!(PLR_FLAGGED(ch, PLR_KILLER) || PLR_FLAGGED(ch, PLR_BLACKLIST)))
    return FALSE;
  int toroom = -1;
  char *dir = NULL, *dir2 = str_dup("nothing");
  switch (world[troll->in_room].number) {
    case 32300:
      toroom = 32653;
      dir = str_dup("east");
      break;
    case 2100:
      toroom = 32587;
      dir = str_dup("west");
      break;
    case 31700:
      toroom = 30579;
      dir = str_dup("ne");
      dir2 = str_dup("northeast");
      break;
    case 3100:
      toroom = 30612;
      dir = str_dup("north");
      break;
    case 5001:
      toroom = 5000;
      dir = str_dup("east");
      break;
    case 35692:
      toroom = 35693;
      dir = str_dup("south");
      break;
    case 35500:
      toroom = 32661;
      dir = str_dup("east");
      break;
  }
  if (toroom == -1)
    return FALSE;
  if (CMD_IS(dir) || CMD_IS(dir2)) {
    act("Without warning, $n picks $N up and tosses $M into the street.", FALSE, troll, 0, ch, TO_NOTVICT);
    act("Without warning, $n picks you up and tosses you into the street.", FALSE, troll, 0, ch, TO_VICT);
    char_from_room(ch);
    char_to_room(ch, real_room(toroom));
    act("$n comes flying out of the club, literally.", TRUE, ch, 0, 0, TO_ROOM);
    DELETE_ARRAY_IF_EXTANT(dir);
    DELETE_ARRAY_IF_EXTANT(dir2);
    return TRUE;
  }
  DELETE_ARRAY_IF_EXTANT(dir);
  DELETE_ARRAY_IF_EXTANT(dir2);
  return FALSE;
}

SPECIAL(locker)
{
  if (!ch || !cmd)
    return FALSE;
  struct obj_data *locker = world[ch->in_room].contents, *next = NULL;
  int num = 0, free = 0;
  for (; locker; locker = locker->next_content)
    if (GET_OBJ_VNUM(locker) == 9825) {
      num++;
      if (!GET_OBJ_VAL(locker, 9))
        free++;
      else if (((time(0) - GET_OBJ_VAL(locker, 8)) / SECS_PER_REAL_DAY) > 7) {
        GET_OBJ_VAL(locker, 8) = GET_OBJ_VAL(locker, 9) = 0;
        for (struct obj_data *obj = locker->contains; next; obj = next) {
          next = obj->next_content;
          extract_obj(obj);
        }
      }
    }
  if (!num)
    return FALSE;
  if (CMD_IS("list")) {
    send_to_char(ch, "%d/%d lockers free. RENT to rent. TYPE <password> to open.\r\n", free, num);
  } else if (CMD_IS("rent")) {
    if (!free)
      send_to_char("No lockers are currently free. Please try again later.\r\n", ch);
    else {
      num = 0;
      for (locker = world[ch->in_room].contents; locker; locker = locker->next_content)
        if (GET_OBJ_VNUM(locker) == 9825) {
          num++;
          if (!GET_OBJ_VAL(locker, 9))
            break;
        }
      send_to_char(ch, "%d.locker opens and the screen reads 'LOCK %d TO CONTINUE'.\r\n", num, num);
      REMOVE_BIT(GET_OBJ_VAL(locker, 1), CONT_CLOSED);
      REMOVE_BIT(GET_OBJ_VAL(locker, 1), CONT_LOCKED);
    }
  } else if (CMD_IS("type")) {
    free = atoi(argument);
    if (!free)
      send_to_char("The system beeps loudly.\r\n", ch);
    else {
      num = 0;
      for (locker = world[ch->in_room].contents; locker; locker = locker->next_content)
        if (GET_OBJ_VNUM(locker) == 9825 && ++num && GET_OBJ_VAL(locker, 9) == free) {
          int cost = (int)((((time(0) - GET_OBJ_VAL(locker, 8)) / SECS_PER_REAL_DAY) + 1) * 50);
          if (GET_NUYEN(ch) < cost)
            send_to_char(ch, "The system beeps loudly and the screen reads 'PLEASE INSERT %d NUYEN'.\r\n", cost);
          else {
            GET_NUYEN(ch) -= cost;
            REMOVE_BIT(GET_OBJ_VAL(locker, 1), CONT_CLOSED);
            REMOVE_BIT(GET_OBJ_VAL(locker, 1), CONT_LOCKED);
            GET_OBJ_VAL(locker, 9) = GET_OBJ_VAL(locker, 8) = 0;
            send_to_char(ch, "%d.locker pops open.\r\n", num);
          }
          return TRUE;
        }
      send_to_char("The system beeps loudly.\r\n", ch);
    }
  } else if (CMD_IS("lock")) {
    num = atoi(argument);
    for (locker = world[ch->in_room].contents; locker; locker = locker->next_content)
      if (GET_OBJ_VNUM(locker) == 9825 && !--num && !IS_SET(GET_OBJ_VAL(locker, 1), CONT_CLOSED)) {
        sprintf(buf, "%d%d%d%d%d%d%d", number(1, 9), number(1, 9), number(1, 9), number(1, 9), number(1, 9), number(1, 9), number(1, 9));
        GET_OBJ_VAL(locker, 8) = time(0);
        GET_OBJ_VAL(locker, 9) = atoi(buf);
        send_to_char(ch, "The locker clicks closed and the screen reads 'TYPE %d TO OPEN'.\r\n", GET_OBJ_VAL(locker, 9));
        SET_BIT(GET_OBJ_VAL(locker, 1), CONT_CLOSED);
        SET_BIT(GET_OBJ_VAL(locker, 1), CONT_LOCKED);
        return TRUE;
      }
    send_to_char("The system beeps loudly.\r\n", ch);
  } else return FALSE;
  return TRUE;
}

SPECIAL(newbie_housing)
{
  if (!ch || !cmd || !CMD_IS("recharge"))
    return FALSE;
  if (GET_NUYEN(ch) < 10000)
    send_to_char("You don't have enough nuyen to recharge a housing card.\r\n", ch);
  else {
    struct obj_data *card = NULL;
    for (card = ch->carrying; card; card = card->next_content)
      if (GET_OBJ_VNUM(card) == 119)
        break;
    if (!card) {
      card = read_object(119, VIRTUAL);
      GET_OBJ_VAL(card, 0) = GET_IDNUM(ch);
      obj_to_char(card, ch);
    }
    GET_OBJ_VAL(card, 1) += 10000;
    GET_NUYEN(ch) -= 10000;
    send_to_char(ch, "You place your card into the machine and add an extra 10,000 nuyen. The value is currently at %d nuyen.\r\n", GET_OBJ_VAL(card, 1));
  }
  return TRUE;
}

SPECIAL(trideo)
{
  struct obj_data *trid = (struct obj_data *) me;
  if (*argument && cmd && CMD_IS("use")) {
    skip_spaces(&argument);
    if ((isname(argument, trid->text.keywords) || isname(argument, trid->text.name) ||
       (trid->restring && isname(argument, trid->restring)))) {
      if (IS_ASTRAL(ch))
        send_to_char("You just can't seem to activate it.\r\n", ch);
      if (trid->in_room == NOWHERE)
        send_to_char("You have to plug it in to turn it on.\r\n", ch);
      else if (!(CAN_WEAR(trid, ITEM_WEAR_TAKE)))
        send_to_char("You don't have control over that unit.\r\n", ch);
      else if (GET_OBJ_VAL(trid, 0)) {
        GET_OBJ_VAL(trid, 0) = 0;
        act("$n turns $p off.", FALSE, ch, trid, 0, TO_ROOM);
        send_to_char("You turn it off.\r\n", ch);
      } else {
        GET_OBJ_VAL(trid, 0) = 1;
        act("$n turns $p on.", FALSE, ch, trid, 0, TO_ROOM);
        send_to_char("You turn it on.\r\n", ch);
      }
      return TRUE;
    }
 }
 return FALSE;
}
