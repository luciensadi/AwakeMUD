#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

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
#include "newmail.h"
#include "pocketsec.h"
#include "limits.h"
#include "config.h"
#include "transport.h"
#include "newmatrix.h"

/*   external vars  */
ACMD_DECLARE(do_goto);
extern struct time_info_data time_info;
extern struct weather_data weather_info;
extern int return_general(int skill_num);
extern struct train_data trainers[];
extern struct teach_data teachers[];
extern struct teach_data metamagict[];
extern struct adept_data adepts[];
extern struct spell_trainer spelltrainers[];
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
extern void wire_nuyen(struct char_data *ch, int amount, vnum_t idnum);
extern void restore_character(struct char_data *vict, bool reset_staff_stats);
bool memory(struct char_data *ch, struct char_data *vict);
extern void do_probe_veh(struct char_data *ch, struct veh_data * k);
extern int get_paydata_market_minimum(int host_color);
extern int new_quest(struct char_data *mob, struct char_data *ch);
extern unsigned int get_johnson_overall_max_rep(struct char_data *johnson);
extern unsigned int get_johnson_overall_min_rep(struct char_data *johnson);
extern bool uninstall_ware_from_target_character(struct obj_data *obj, struct char_data *remover, struct char_data *victim, bool damage_on_operation);
extern bool install_ware_in_target_character(struct obj_data *obj, struct char_data *installer, struct char_data *reciever, bool damage_on_operation);
extern struct obj_data *shop_package_up_ware(struct obj_data *obj);
extern const char *get_plaintext_score_essence(struct char_data *ch);
extern void diag_char_to_char(struct char_data * i, struct char_data * ch);


extern struct command_info cmd_info[];

ACMD_CONST(do_say);
ACMD_DECLARE(do_echo);
ACMD_CONST(do_time);
ACMD_CONST(do_gen_door);
ACMD_DECLARE(do_wield);
ACMD_DECLARE(do_draw);
ACMD_DECLARE(do_holster);
ACMD_DECLARE(do_remove);

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

int fixers_need_save;

#define NO_DRAG_BULLSHIT if (cmd && CMD_IS("drag")) { send_to_char("You can't drag people here.\r\n", ch); return TRUE; }

/* ********************************************************************
*  Special procedures for mobiles                                     *
******************************************************************** */

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
  case ADEPT_MYSTIC_ARMOR:
  case ADEPT_BLIND_FIGHTING:
  case ADEPT_IRONWILL:
  case ADEPT_IMPROVED_BOD:
  case ADEPT_IMPROVED_QUI:
  case ADEPT_IMPROVED_STR:
  case ADEPT_AIDSPELL:
  case ADEPT_MOTIONSENSE:
  case ADEPT_SIDESTEP:
  case ADEPT_COUNTERSTRIKE:
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

int train_ability_cost(struct char_data *ch, int abil, int level) {
  int cost = ability_cost(abil, level);

  switch (abil) {
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

  return cost;
}

void attack_random_player(struct char_data *mob, struct char_data *boss)
{
  struct char_data *vict;
  int num = 0;

  for (vict = mob->in_room->people; vict; vict = vict->next_in_room)
    if (!IS_NPC(vict) && FIGHTING(vict) == boss)
      num++;

  num = MAX(1, num - 1);

  for (vict = mob->in_room->people; vict; vict = vict->next_in_room)
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

  for (tch = ch->in_room->people; tch; tch = tch->next_in_room)
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

  if (GET_SKILL(ch, i) + 1 <= GET_REAL_ATT(ch, skills[i].attribute)) {
    if (skills[i].type == SKILL_TYPE_KNOWLEDGE)
      return GET_SKILL(ch, i) + 1;
    else
      return (int)((GET_SKILL(ch, i) + 1) * 1.5);
  }

  if (GET_SKILL(ch, i) + 1 <= GET_REAL_ATT(ch, skills[i].attribute) * 2) {
    if (skills[i].type == SKILL_TYPE_KNOWLEDGE)
      return (int)((GET_SKILL(ch, i) + 1) * 1.5);
    else
      return (int)((GET_SKILL(ch, i) + 1) * 2);
  }

  if (skills[i].type == SKILL_TYPE_KNOWLEDGE)
    return (GET_SKILL(ch, i) + 1) * 2;
  else
    return (int)((GET_SKILL(ch, i) + 1) * 2.5);
}

SPECIAL(metamagic_teacher)
{
  struct char_data *master = (struct char_data *) me;
  int i = 0, x = 0, suc, ind;
  if (!(CMD_IS("train")))
    return FALSE;

  if (GET_TRADITION(ch) == TRAD_MUNDANE) {
    send_to_char("You don't have the talent to train here.\r\n", ch);
    return TRUE;
  }

  if (!CAN_SEE(master, ch)) {
    send_to_char("You'd better become visible first; it's hard to teach someone you can't see.\r\n", ch);
    return TRUE;
  }

  if (FIGHTING(ch)) {
    send_to_char("Learning a new skill while fighting? Now that would be a neat trick.\r\n", ch);
    return TRUE;
  }

  if (GET_POS(ch) < POS_SITTING) {
    send_to_char("You'd better sit up first.\r\n", ch);
    return TRUE;
  }

  if (IS_NPC(ch)) {
    send_to_char("NPCs can't learn metamagic techniques, go away.\r\n", ch);
    return TRUE;
  }

  skip_spaces(&argument);

  for (ind = 0; metamagict[ind].vnum != 0; ind++)
    if (metamagict[ind].vnum == GET_MOB_VNUM(master))
      break;

  if (metamagict[ind].vnum != GET_MOB_VNUM(master))
    return FALSE;

  if (!*argument) {
    send_to_char(ch, "%s can teach you the following techniques: \r\n", GET_NAME(master));
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
    send_to_char(ch, "You already know how to use %s.\r\n", metamagic[i]);
    return TRUE;
  }

  if (!GET_METAMAGIC(ch, i)) {
    send_to_char(ch, "You aren't close enough to the astral plane to learn %s.\r\n", metamagic[i]);
    return TRUE;
  }
  /* "Hey, I have an idea!" "What?" "Let's arbitrarily restrict who can train where so that the builders have to do more work!"
  if (GET_GRADE(ch) >= (GET_MAG(master) / 100) - 6) {
    send_to_char(ch, "%s is not powerful enough to teach you that technique.\r\n", GET_NAME(master));
    return TRUE;
  } */

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
    send_to_char(ch, "You don't have the %d nuyen required to learn %s.\r\n", cost, metamagic[i]);
    return TRUE;
  }
  lose_nuyen(ch, cost, NUYEN_OUTFLOW_METAMAGIC_TRAINING);
  send_to_char(ch, "%s runs through how to use %s with you, you walk away knowing exactly how to use it.\r\n", GET_NAME(master), metamagic[i]);
  SET_METAMAGIC(ch, i, GET_METAMAGIC(ch, i) + 1);
  return TRUE;
}

// Teaches every skill that isn't in the other teachers' train lists. The assumption is that if it exists in code but isn't taught anywhere, it's not implemented.
SPECIAL(nerp_skills_teacher) {
  struct char_data *master = (struct char_data *) me;
  int max = NORMAL_MAX_SKILL, skill_num;

  bool can_teach_skill[MAX_SKILLS];
  for (int skill = 1; skill < MAX_SKILLS; skill++)
    can_teach_skill[skill] = TRUE;
  can_teach_skill[0] = FALSE; // OMGWTFBBQ is not a valid skill
  can_teach_skill[SKILL_ARMED_COMBAT] = FALSE; // NPC-only skill
  can_teach_skill[SKILL_UNUSED_WAS_PILOT_FIXED_WING] = FALSE; // what it says on the tin
  can_teach_skill[SKILL_UNUSED_WAS_CLIMBING] = FALSE;

  if (!CMD_IS("practice"))
    return FALSE;

  if (!CAN_SEE(master, ch)) {
    send_to_char("You'd better become visible first; it's hard to teach someone you can't see.\r\n", ch);
    return TRUE;
  }

  if (FIGHTING(ch)) {
    send_to_char("Learning a new skill while fighting? Now that would be a neat trick.\r\n", ch);
    return TRUE;
  }

  if (GET_POS(ch) < POS_STANDING) {
    send_to_char("You'd better stand up first.\r\n", ch);
    return TRUE;
  }

  if (IS_NPC(ch)) {
    send_to_char("NPCs can't train, go away.\r\n", ch);
    return TRUE;
  }

  // Negate any skill in our array that is taught elsewhere in the game.
  for (int ind = 0; teachers[ind].vnum != 0; ind++) {
    // Skip Sombrero teachers-- they have NERP skills.
    if (teachers[ind].vnum >= 1000 && teachers[ind].vnum <= 1099)
      continue;

    for (int skill_index = 0; skill_index < NUM_TEACHER_SKILLS; skill_index++)
      if (teachers[ind].s[skill_index] > 0)
        can_teach_skill[teachers[ind].s[skill_index]] = FALSE;
  }

  skip_spaces(&argument);

  if (!*argument) {
    // Send them a list of skills they can learn here.
    bool found_a_skill_already = FALSE;
    for (int skill = MIN_SKILLS; skill < MAX_SKILLS; skill++) {
      if (can_teach_skill[skill]) {
        // Mundanes can't learn magic skills.
        if (GET_TRADITION(ch) == TRAD_MUNDANE && skills[skill].requires_magic)
          continue;

        if (GET_SKILL_POINTS(ch) > 0) {
          // Add conditional messaging.
          if (!found_a_skill_already) {
            found_a_skill_already = TRUE;
            snprintf(buf, sizeof(buf), "%s can teach you the following:\r\n", GET_NAME(master));
          }
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  %s\r\n", skills[skill].name);
        }
        else if (GET_SKILL(ch, skill) < max && !ch->char_specials.saved.skills[skill][1]) {
          // Add conditional messaging.
          if (!found_a_skill_already) {
            found_a_skill_already = TRUE;
            snprintf(buf, sizeof(buf), "%s can teach you the following:\r\n", GET_NAME(master));
          }
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  %-24s (%d karma %d nuyen)\r\n", skills[skill].name, get_skill_price(ch, skill),
                  MAX(1000, (GET_SKILL(ch, skill) * 5000)));
        }
      }
    }
    // Failure case.
    if (!found_a_skill_already) {
      send_to_char(ch, "There's nothing %s can teach you that you don't already know.\r\n", GET_NAME(master));
      return TRUE;
    }

    if (GET_SKILL_POINTS(ch) > 0)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nYou have %d point%s to use for skills.\r\n",
              GET_SKILL_POINTS(ch), GET_SKILL_POINTS(ch) > 1 ? "s" : "");
    else
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nYou have %0.2f karma to use for skills.\r\n",
              ((float)GET_KARMA(ch) / 100));
    send_to_char(buf, ch);
    return TRUE;
  }

  skill_num = find_skill_num(argument);

  if (skill_num < 0) {
    send_to_char(ch, "That's not a valid skill name. You can use any keyword from the skill, but don't use quotes or other punctuation.\r\n");
    return TRUE;
  }

  if (!can_teach_skill[skill_num]) {
    send_to_char(ch, "%s doesn't seem to know about %s.\r\n", GET_NAME(master), skills[skill_num].name);
    return TRUE;
  }

  if (GET_SKILL(ch, skill_num) != REAL_SKILL(ch, skill_num)) {
    send_to_char("You can't train a skill you currently have a skillsoft or other boost for.\r\n", ch);
    return TRUE;
  }

  // Deny some magic skills to mundane (different flavor from next block, same effect).
  if ((skill_num == SKILL_CENTERING || skill_num == SKILL_ENCHANTING) && GET_TRADITION(ch) == TRAD_MUNDANE) {
    send_to_char(ch, "Without access to the astral plane you can't even begin to fathom the basics of %s.\r\n", skills[skill_num].name);
    return TRUE;
  }

  // Deny all magic skills to mundane.
  if (skills[skill_num].requires_magic && GET_TRADITION(ch) == TRAD_MUNDANE) {
    send_to_char(ch, "Without the ability to channel magic, %s would be useless to you.\r\n", skills[skill_num].name);
    return TRUE;
  }

  if (GET_SKILL(ch, skill_num) >= max) {
    snprintf(arg, sizeof(arg), "%s You already know more than I can teach you about %s.", GET_CHAR_NAME(ch), skills[skill_num].name);
    do_say(master, arg, 0, SCMD_SAYTO);
    return TRUE;
  }

  if (GET_KARMA(ch) < get_skill_price(ch, skill_num) * 100 &&
      GET_SKILL_POINTS(ch) <= 0) {
    send_to_char(ch, "You don't have enough karma to improve %s.\r\n", skills[skill_num].name);
    return TRUE;
  }
  if (!PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
    int skill_nuyen_cost = MAX(1000, (GET_SKILL(ch, skill_num) * 5000));
    if (GET_NUYEN(ch) < skill_nuyen_cost) {
      send_to_char(ch, "You can't afford the %d nuyen practice fee!\r\n", skill_nuyen_cost);
      return TRUE;
    }
    lose_nuyen(ch, skill_nuyen_cost, NUYEN_OUTFLOW_SKILL_TRAINING);
  }

  if (GET_SKILL_POINTS(ch) > 0)
    GET_SKILL_POINTS(ch)--;
  else
    GET_KARMA(ch) -= get_skill_price(ch, skill_num) * 100;

  send_to_char("You increase your abilities in this UNIMPLEMENTED SKILL.\r\n", ch);
  set_character_skill(ch, skill_num, REAL_SKILL(ch, skill_num) + 1, TRUE);
  if (GET_SKILL(ch, skill_num) >= max)
    send_to_char("You have learnt all you can here.\r\n", ch);

  return TRUE;
}

int get_max_skill_for_char(struct char_data *ch, int skill, int type) {
  int max;

  if (!ch) {
    mudlog("SYSERR: Null character received at get_max_skill_for_char.", NULL, LOG_SYSLOG, TRUE);
    return -1;
  }

  // Scope maximums based on teacher type.
  if (type == NEWBIE)
    max = NEWBIE_SKILL;
  else if (type == AMATEUR)
    max = NORMAL_MAX_SKILL;
  else if (type == ADVANCED)
    max = LEARNED_LEVEL;
  else if (type == GODLY)
    max = 100;
  else if (type == LIBRARY)
    max = LIBRARY_SKILL;
  else {
    snprintf(buf, sizeof(buf), "SYSERR: Unknown teacher type %d received at get_max_skill_for_char().", type);
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return -1;
  }

  // Override: All language skills can be learned to the maximum.
  //  This does remove a tiny bit of flavor (no learning Japanese to level 2 max
  //  from the guy in line at the shop), but it simplifies a lot of stuff.
  if (SKILL_IS_LANGUAGE(skill))
    return 12;

  // Override: Everyone can train negotiation to 12 since it's key in run payouts etc.
  if (skill == SKILL_NEGOTIATION)
    return MIN(max, 12);

  // Clamp maximums.
  switch (GET_TRADITION(ch)) {
    case TRAD_MUNDANE:
      return MIN(max, 12);
    case TRAD_ADEPT:
      return MIN(max, 10);
    default:
      switch (skill) {
        case SKILL_CONJURING:
        case SKILL_SORCERY:
        case SKILL_SPELLDESIGN:
        case SKILL_AURA_READING:
        case SKILL_SINGING:
        case SKILL_DANCING:
        case SKILL_CHANTING:
        case SKILL_INSTRUMENT:
        case SKILL_MEDITATION:
        case SKILL_ARCANELANGUAGE:
        case SKILL_CENTERING:
        case SKILL_ENCHANTING:
          return MIN(max, 12);
        default:
          // Non-aspected mages get their non-magic skills capped at 8.
          if (GET_ASPECT(ch) == ASPECT_FULL)
            return MIN(max, 8);

          // Aspected mages, since they're gimped by their aspect, get them to 10 like adepts.
          return MIN(max, 10);
      }
  }
}

SPECIAL(teacher)
{
  struct char_data *master = (struct char_data *) me;
  int i, ind, max, skill_num;

  if (!(CMD_IS("practice")))
    return FALSE;

  if (!CAN_SEE(master, ch)) {
    send_to_char("You'd better become visible first; it's hard to teach someone you can't see.\r\n", ch);
    return TRUE;
  }

  if (FIGHTING(ch)) {
    send_to_char("Learning a new skill while fighting? Now that would be a neat trick.\r\n", ch);
    return TRUE;
  }

  if (GET_POS(ch) < POS_STANDING) {
    send_to_char("You'd better stand up first.\r\n", ch);
    return TRUE;
  }

  if (IS_NPC(ch)) {
    send_to_char("NPCs can't train, go away.\r\n", ch);
    return TRUE;
  }

  skip_spaces(&argument);

  for (ind = 0; teachers[ind].vnum != 0; ind++)
    if (teachers[ind].vnum == GET_MOB_VNUM(master))
      break;

  if (teachers[ind].vnum != GET_MOB_VNUM(master))
    return FALSE;

  /*
  if (teachers[ind].type != NEWBIE && PLR_FLAGGED(ch, PLR_NEWBIE)) {
    snprintf(arg, sizeof(arg), "%s You're not quite ready yet!", GET_CHAR_NAME(ch));
    do_say(master, arg, 0, SCMD_SAYTO);
    send_to_char(ch, "(^mOOC^n: You can't raise your skills until you've earned at least %d karma.)\r\n", NEWBIE_KARMA_THRESHOLD + 1);
    return TRUE;
  }
  */

  if (!PLR_FLAGGED(ch, PLR_NEWBIE))
    GET_SKILL_POINTS(ch) = 0;

  if (!AWAKE(ch)) {
    send_to_char("You must be conscious to practice.\r\n", ch);
    return TRUE;
  }

  if (!*argument) {
    bool found_a_skill_already = FALSE;
    for (int i = 0; i < NUM_TEACHER_SKILLS; i++) {
      if (teachers[ind].s[i] > 0) {
        // Mundanes can't learn magic skills.
        if (GET_TRADITION(ch) == TRAD_MUNDANE && skills[teachers[ind].s[i]].requires_magic)
          continue;

        // Adepts can't learn externally-focused skills.
        if (GET_TRADITION(ch) == TRAD_ADEPT && (teachers[ind].s[i] == SKILL_CONJURING
                                                || teachers[ind].s[i] == SKILL_SORCERY
                                                || teachers[ind].s[i] == SKILL_SPELLDESIGN))
          continue;

        // Prevent aspected from learning skills they can't use.
        if (GET_ASPECT(ch) == ASPECT_CONJURER && (teachers[ind].s[i] == SKILL_SORCERY || teachers[ind].s[i] == SKILL_SPELLDESIGN))
          continue;

        else if (GET_ASPECT(ch) == ASPECT_SORCERER && teachers[ind].s[i] == SKILL_CONJURING)
          continue;

        // Set our maximum for this skill. Break out on error.
        if ((max = get_max_skill_for_char(ch, teachers[ind].s[i], teachers[ind].type)) < 0)
          return FALSE;

        if (GET_SKILL_POINTS(ch) > 0) {
          // Add conditional messaging.
          if (!found_a_skill_already) {
            found_a_skill_already = TRUE;
            snprintf(buf, sizeof(buf), "%s can teach you the following:\r\n", GET_NAME(master));
          }
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  %s\r\n", skills[teachers[ind].s[i]].name);
        }
        else if (GET_SKILL(ch, teachers[ind].s[i]) < max && !ch->char_specials.saved.skills[teachers[ind].s[i]][1])
        {
          // Add conditional messaging.
          if (!found_a_skill_already) {
            found_a_skill_already = TRUE;
            snprintf(buf, sizeof(buf), "%s can teach you the following:\r\n", GET_NAME(master));
          }
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  %-24s (%d karma, %d nuyen)\r\n", skills[teachers[ind].s[i]].name, get_skill_price(ch, teachers[ind].s[i]),
                  MAX(1000, (GET_SKILL(ch, teachers[ind].s[i]) * 5000)));
        }
      }
    }
    // Failure case.
    if (!found_a_skill_already) {
      send_to_char(ch, "There's nothing %s can teach you that you don't already know.\r\n", GET_NAME(master));
      return TRUE;
    }

    if (GET_SKILL_POINTS(ch) > 0)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nYou have %d point%s to use for skills.\r\n",
              GET_SKILL_POINTS(ch), GET_SKILL_POINTS(ch) > 1 ? "s" : "");
    else
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nYou have %0.2f karma to use for skills.\r\n",
              ((float)GET_KARMA(ch) / 100));
    send_to_char(buf, ch);
    return TRUE;
  }

  if ((teachers[ind].type == NEWBIE && GET_SKILL_POINTS(ch) <= 0 && GET_KARMA(ch) <= 0)
      || (teachers[ind].type != NEWBIE && GET_KARMA(ch) <= 0)) {
    send_to_char(ch, "You don't have the %s to learn anything right now.\r\n", teachers[ind].type == NEWBIE ? "skill points" : "karma");
    return TRUE;
  }

  skill_num = find_skill_num(argument);

  if (skill_num < 0) {
    send_to_char(ch, "That's not a valid skill name. You can use any keyword from the skill, but don't use quotes or other punctuation.\r\n");
    return TRUE;
  }

  for (i = 0; i < NUM_TEACHER_SKILLS; i++)
    if (skill_num == teachers[ind].s[i])
      break;
  if (i >= NUM_TEACHER_SKILLS) {
    send_to_char(ch, "%s doesn't seem to know about %s.\r\n", GET_NAME(master), skills[skill_num].name);
    return TRUE;
  }

  if (GET_SKILL(ch, skill_num) != REAL_SKILL(ch, skill_num)) {
    send_to_char("You can't train a skill you currently have a skillsoft or other boost for.\r\n", ch);
    return TRUE;
  }

  // Deny some magic skills to mundane (different flavor from next block, same effect).
  if ((skill_num == SKILL_CENTERING || skill_num == SKILL_ENCHANTING) && GET_TRADITION(ch) == TRAD_MUNDANE) {
    send_to_char(ch, "Without access to the astral plane you can't even begin to fathom the basics of %s.\r\n", skills[skill_num].name);
    return TRUE;
  }

  // Deny all magic skills to mundane.
  if (skills[skill_num].requires_magic && GET_TRADITION(ch) == TRAD_MUNDANE) {
    send_to_char(ch, "Without the ability to channel magic, the skill of %s would be useless to you.\r\n", skills[skill_num].name);
    return TRUE;
  }

  // Deny some magic skills to adepts.
  if (GET_TRADITION(ch) == TRAD_ADEPT && (skill_num == SKILL_CONJURING
                                          || skill_num == SKILL_SORCERY
                                          || skill_num == SKILL_SPELLDESIGN)) {
    send_to_char("Your magic is focused inwards on improving your physical abilities. You can't learn these external magics.\r\n", ch);
    return TRUE;
  }

  // Prevent aspected shamans from learning skills they can't use.
  if (GET_ASPECT(ch) == ASPECT_CONJURER && (skill_num == SKILL_SORCERY
                                            || skill_num == SKILL_SPELLDESIGN)) {
    send_to_char(ch, "Your magic is focused on the summoning of %s. You cannot learn spellwork.\r\n", GET_TRADITION(ch) == TRAD_SHAMANIC ? "spirits" : "elementals");
    return TRUE;
  }

  if (GET_ASPECT(ch) == ASPECT_SORCERER && skill_num == SKILL_CONJURING) {
    send_to_char(ch, "Your magic is focused on spellwork. You cannot learn to summon %s.\r\n", GET_TRADITION(ch) == TRAD_SHAMANIC ? "spirits" : "elementals");
    return TRUE;
  }

  max = get_max_skill_for_char(ch, skill_num, teachers[ind].type);
  if (GET_SKILL(ch, skill_num) >= max) {
    if (max == LIBRARY_SKILL)
      send_to_char(ch, "You can't find any books that tell you things you don't already know about %s.\r\n", skills[skill_num].name);
    else {
      snprintf(arg, sizeof(arg), "%s You already know more than I can teach you about %s.", GET_CHAR_NAME(ch), skills[skill_num].name);
      do_say(master, arg, 0, SCMD_SAYTO);
    }
    return TRUE;
  }

  if (GET_KARMA(ch) < get_skill_price(ch, skill_num) * 100 &&
      GET_SKILL_POINTS(ch) <= 0) {
    send_to_char(ch, "You don't have enough karma to improve your skill in %s.\r\n", skills[skill_num].name);
    return TRUE;
  }
  if (!PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
    int skill_nuyen_cost = MAX(1000, (GET_SKILL(ch, skill_num) * 5000));
    if (GET_NUYEN(ch) < skill_nuyen_cost) {
      send_to_char(ch, "You can't afford the %d nuyen practice fee!\r\n", skill_nuyen_cost);
      return TRUE;
    }
    lose_nuyen(ch, skill_nuyen_cost, NUYEN_OUTFLOW_SKILL_TRAINING);
  }
  if (GET_SKILL_POINTS(ch) > 0)
    GET_SKILL_POINTS(ch)--;
  else
    GET_KARMA(ch) -= get_skill_price(ch, skill_num) * 100;

  send_to_char(teachers[ind].msg, ch);
  set_character_skill(ch, skill_num, REAL_SKILL(ch, skill_num) + 1, TRUE);
  if (GET_SKILL(ch, skill_num) >= max)
    send_to_char("You have learnt all you can here.\r\n", ch);

  return TRUE;
}

int calculate_training_raw_cost(struct char_data *ch, int attribute) {
  int adept_mod = 0;

  // Calculate increases in cost from adept powers.
  if (attribute == BOD)
    adept_mod = GET_POWER_TOTAL(ch, ADEPT_IMPROVED_BOD);
  else if (attribute == QUI)
    adept_mod = GET_POWER_TOTAL(ch, ADEPT_IMPROVED_QUI);
  else if (attribute == STR)
    adept_mod = GET_POWER_TOTAL(ch, ADEPT_IMPROVED_STR);

  return 1 + GET_REAL_ATT(ch, attribute) + adept_mod;
}

bool attribute_below_maximums(struct char_data *ch, int attribute) {
  // Special case: Bod can have permanent loss.
  if (attribute == BOD)
    return GET_REAL_BOD(ch) + GET_PERM_BOD_LOSS(ch) < racial_limits[(int)GET_RACE(ch)][0][BOD];

  return GET_REAL_ATT(ch, attribute) < racial_limits[(int)GET_RACE(ch)][0][attribute];
}

void send_training_list_to_char(struct char_data *ch, int ind) {
  int first = 1, raw_cost = 0;

  if (GET_ATT_POINTS(ch) > 0) {
    send_to_char(ch, "You have %d attribute point%s to distribute.  You can ^WTRAIN",
                 GET_ATT_POINTS(ch), GET_ATT_POINTS(ch) > 1 ? "s" : "");
  } else {
    send_to_char(ch, "You have %0.2f karma points.  You can train", (float)GET_KARMA(ch) / 100);
  }

  for (int i = 0; i <= WIL; i++) {
    if (IS_SET(trainers[ind].attribs, (1 << i)) && attribute_below_maximums(ch, i)) {
      raw_cost = calculate_training_raw_cost(ch, i);
      if (GET_ATT_POINTS(ch) > 0)
        send_to_char(ch, "%s ^W%.3s^n", first ? "" : ",", string_to_uppercase(attributes[i]));
      else
        send_to_char(ch, "%s   %.3s (%d karma %d nuyen)", first ? ":\r\n" : "\r\n", string_to_uppercase(attributes[i]), 2 * raw_cost, 1000 * raw_cost);
      first = 0;
    }
  }
  if (!first) {
    // Found something to train. Only newbie training is in sentence form, so only append a period for them.
    send_to_char(ch, "%s\r\n", GET_ATT_POINTS(ch) > 0 ? "." : "");
  } else {
    // Found nothing, let them know.
    send_to_char(" nothing-- you're already at your maximums!\r\n", ch);
  }
}

void train_attribute(struct char_data *ch, struct char_data *trainer, int ind, int attr, const char *success_message) {
  int raw_cost = calculate_training_raw_cost(ch, attr);
  int karma_cost = 2 * raw_cost;
  int nuyen_cost = 1000 * raw_cost;

  // Check for racial maximums.
  if (!attribute_below_maximums(ch, attr)) {
    send_to_char(ch, "Your %s attribute is at its maximum.\r\n", attributes[attr]);
    return;
  }

  // Check for affordability.
  if (GET_ATT_POINTS(ch) <= 0 && (int)(GET_KARMA(ch) / 100) < karma_cost) {
    send_to_char(ch, "You don't have enough karma to raise your %s attribute.\r\n", attributes[attr]);
    return;
  }

  // Check for nuyen cost, if applicable.
  if (!trainers[ind].is_newbie) {
    if (GET_NUYEN(ch) < nuyen_cost) {
      snprintf(arg, sizeof(arg), "%s The charge for that is %d nuyen, which you don't seem to be carrying.", GET_CHAR_NAME(ch), nuyen_cost);
      do_say(trainer, arg, 0, SCMD_SAYTO);
      return;
    }
    // Deduct nuyen cost.
    lose_nuyen(ch, nuyen_cost, NUYEN_OUTFLOW_SKILL_TRAINING);
  }

  // Deduct karma/attrpoint cost.
  if (GET_ATT_POINTS(ch) > 0)
    GET_ATT_POINTS(ch)--;
  else
    GET_KARMA(ch) -= karma_cost * 100;

  // Apply the change.
  GET_REAL_ATT(ch, attr) += 1;

  // Update character's calculated values.
  affect_total(ch);

  // Notify the character of success.
  send_to_char(ch, success_message, GET_REAL_ATT(ch, attr));
}

SPECIAL(trainer)
{
  struct char_data *trainer = (struct char_data *) me;
  int ind;

  if (!CMD_IS("train"))
    return FALSE;

  if (GET_POS(ch) < POS_STANDING) {
    send_to_char("You have to be standing to do that.\r\n", ch);
    return TRUE;
  }

  if (!CAN_SEE(trainer, ch)) {
    send_to_char("You try to get their attention for some personalized training, but they can't see you.\r\n", ch);
    return TRUE;
  }

  if (FIGHTING(ch)) {
    send_to_char("While you're fighting? That'd be a neat trick.\r\n", ch);
    return TRUE;
  }

  if (IS_NPC(ch)) {
    send_to_char("NPCs can't train, go away.\r\n", ch);
    return TRUE;
  }

  skip_spaces(&argument);

  for (ind = 0; trainers[ind].vnum != 0; ind++)
    if (trainers[ind].vnum == GET_MOB_VNUM(trainer))
      break;

  if (trainers[ind].vnum != GET_MOB_VNUM(trainer))
    return FALSE;

  /*
  if (!trainers[ind].is_newbie && PLR_FLAGGED(ch, PLR_NEWBIE)) {
    snprintf(arg, sizeof(arg), "%s You're not quite ready yet!", GET_CHAR_NAME(ch));
    do_say(trainer, arg, 0, SCMD_SAYTO);
    send_to_char(ch, "(^mOOC^n: You can't raise your stats until you've earned at least %d karma.)\r\n", NEWBIE_KARMA_THRESHOLD + 1);
    return TRUE;
  }
  */

  if (!access_level(ch, LVL_BUILDER) && !PLR_FLAGGED(ch, PLR_NEWBIE) && GET_ATT_POINTS(ch) != 0) {
    snprintf(buf, sizeof(buf), "SYSERR: %s graduated from newbie status while still having %d attribute point%s left. How?",
            GET_CHAR_NAME(ch), GET_ATT_POINTS(ch), GET_ATT_POINTS(ch) > 1 ? "s" : "");
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    GET_ATT_POINTS(ch) = 0;
  }

  else if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED) && GET_ATT_POINTS(ch) <= 0) {
    send_to_char(ch, "You don't have any more attribute points to spend.\r\n");
    return TRUE;
  }

  if (!*argument) {
    send_training_list_to_char(ch, ind);
    return TRUE;
  }

  if (is_abbrev(argument, "body") && IS_SET(trainers[ind].attribs, TBOD)) {
    train_attribute(ch, trainer, ind, BOD, "Your previous weeks' worth of hard work increase your Body to %d.\r\n");
    return TRUE;
  }

  if (is_abbrev(argument, "quickness") && IS_SET(trainers[ind].attribs, TQUI)) {
    train_attribute(ch, trainer, ind, QUI, "After weeks of chasing chickens, your hard work pays off and your Quickness raises to %d.\r\n");
    return TRUE;
  }

  if (is_abbrev(argument, "strength") && IS_SET(trainers[ind].attribs, TSTR)) {
    train_attribute(ch, trainer, ind, STR, "With months of weight lifting, your Strength increases to %d.\r\n");
    return TRUE;
  }

  if (is_abbrev(argument, "charisma") && IS_SET(trainers[ind].attribs, TCHA)) {
    train_attribute(ch, trainer, ind, CHA, "After weeks of reading self-help books and raising your confidence, your Charisma raises to %d.\r\n");
    return TRUE;
  }

  if (is_abbrev(argument, "intelligence") && IS_SET(trainers[ind].attribs, TINT)) {
    train_attribute(ch, trainer, ind, INT, "Through many long hours using educational simsense, your Intelligence raises to %d.\r\n");
    return TRUE;
  }

  if (is_abbrev(argument, "willpower") && IS_SET(trainers[ind].attribs, TWIL)) {
    train_attribute(ch, trainer, ind, WIL, "Through a strict regimen of work and study, your Willpower raises to %d.\r\n");
    return TRUE;
  }

  send_training_list_to_char(ch, ind);
  return TRUE;
}

SPECIAL(spell_trainer)
{
  struct char_data *trainer = (struct char_data *) me;
  int i, force;
  if (!CMD_IS("learn"))
    return FALSE;

  if (GET_POS(ch) < POS_STANDING) {
    send_to_char("You have to be standing to do that.\r\n", ch);
    return TRUE;
  }

  if (!CAN_SEE(trainer, ch)) {
    send_to_char("You try to get their attention for some personalized training, but they can't see you.\r\n", ch);
    return TRUE;
  }

  if (FIGHTING(ch)) {
    send_to_char("While you're fighting? That'd be a neat trick.\r\n", ch);
    return TRUE;
  }

  if (IS_NPC(ch)) {
    send_to_char("NPCs can't train, go away.\r\n", ch);
    return TRUE;
  }

  if (GET_TRADITION(ch) != TRAD_SHAMANIC && GET_TRADITION(ch) != TRAD_HERMETIC) {
    snprintf(arg, sizeof(arg), "%s You don't have the talent.", GET_CHAR_NAME(ch));
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

          send_to_char(ch, "%-30s Force Max: %d\r\n", compose_spell_name(spelltrainers[i].type, spelltrainers[i].subtype), spelltrainers[i].force);
        }
    if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
      if (GET_TRADITION(ch) == TRAD_HERMETIC && GET_ASPECT(ch) != ASPECT_SORCERER)
        send_to_char("Conjuring  Materials          1 Force Point/Level\r\n", ch);
      send_to_char("Extra Force Point             25000 nuyen\r\n", ch);
      send_to_char(ch, "%d Force Point%s Remaining.\r\n", GET_FORCE_POINTS(ch), GET_FORCE_POINTS(ch) > 1 ? "s" : "");
    } else
      send_to_char(ch, "%.2f Karma Available.\r\n", GET_KARMA(ch) / 100);

    send_to_char("\r\nLearning syntax:\r\n    ^WLEARN \"<spell name>\" <force between 1 and 6>^n\r\n    ^WLEARN FORCE^n\r\n    ^WLEARN CONJURING^n\r\n", ch);
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
    if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
      if (!str_cmp(buf, "force")) {
        if (GET_NUYEN(ch) < 25000)
          send_to_char("You don't have enough nuyen to buy an extra force point.\r\n", ch);
        else {
          send_to_char("You spend 25000 nuyen on an extra force point.\r\n", ch);
          // This is tracked as raw nuyen expenditure because we don't care about chargen spending.
          GET_NUYEN_RAW(ch) -= 25000;
          GET_FORCE_POINTS(ch)++;
        }
        return TRUE;
      }

      if (GET_TRADITION(ch) == TRAD_HERMETIC && GET_ASPECT(ch) != ASPECT_SORCERER && !str_cmp(buf, "conjuring")) {
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
            obj = read_object(OBJ_CONJURING_MATERIALS, VIRTUAL);
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
      if (is_abbrev(buf, compose_spell_name(spelltrainers[i].type, spelltrainers[i].subtype)))
        break;
    }
    if (!spelltrainers[i].teacher) {
      send_to_char(ch, "A spell called '%s' is not being offered here.\r\n", buf);
      return TRUE;
    }
    if (!(force = atoi(buf1)))
      send_to_char(ch, "Syntax: ^Wlearn \"<spell name>\" <force between 1-%d>^n\r\n", spelltrainers[i].force);
    else if (force > spelltrainers[i].force)
      send_to_char(ch, "%s can only teach %s up to force %d.\r\n", GET_NAME(trainer), compose_spell_name(spelltrainers[i].type, spelltrainers[i].subtype), spelltrainers[i].force);
    else {
      // TODO: Decrease force by amount already known.
      int cost = force;
      struct spell_data *spell = NULL;

      for (spell = GET_SPELLS(ch); spell; spell = spell->next)
        if (spell->type == spelltrainers[i].type && spell->subtype == spelltrainers[i].subtype) {
          if (spell->force >= force) {
            send_to_char("You already know this spell at an equal or higher force.\r\n", ch);
            return TRUE;
          } else {
            cost -= spell->force;
            break;
          }
        }

      // They must have the right number of force points or amount of karma to learn this new spell.
      if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
        if (cost > GET_FORCE_POINTS(ch)) {
          send_to_char("You don't have enough force points to learn the spell at that high a force.\r\n", ch);
          return TRUE;
        }
      } else {
        if (cost > GET_KARMA(ch) / 100) {
          send_to_char("You don't have enough karma to learn the spell at that high a force.\r\n", ch);
          return TRUE;
        }
      }

      // Subtract the cost.
      if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED))
        GET_FORCE_POINTS(ch) -= cost;
      else
        GET_KARMA(ch) -= cost * 100;

      send_to_char(ch, "%s sits you down and teaches you the ins and outs of casting %s at force %d.\r\n", GET_NAME(trainer), compose_spell_name(spelltrainers[i].type, spelltrainers[i].subtype), force);

      if (spell) {
        spell->force = force;
      } else {
        spell = new spell_data;
        spell->name = str_dup(compose_spell_name(spelltrainers[i].type, spelltrainers[i].subtype));
        spell->type = spelltrainers[i].type;
        spell->subtype = spelltrainers[i].subtype;
        spell->force = force;
        spell->next = GET_SPELLS(ch);
        GET_SPELLS(ch) = spell;
      }
      GET_SPELLS_DIRTY_BIT(ch) = TRUE;
    }
  }
  return TRUE;
}

SPECIAL(adept_trainer)
{
  struct char_data *trainer = (struct char_data *) me;
  int ind, power, i;

  if (!CMD_IS("train"))
    return FALSE;

  if (GET_POS(ch) < POS_STANDING) {
    send_to_char("You have to be standing to do that.\r\n", ch);
    return TRUE;
  }

  if (!CAN_SEE(trainer, ch)) {
    send_to_char("You try to get their attention for some personalized training, but they can't see you.\r\n", ch);
    return TRUE;
  }

  if (FIGHTING(ch)) {
    send_to_char("While you're fighting? That'd be a neat trick.\r\n", ch);
    return TRUE;
  }

  if (IS_NPC(ch)) {
    send_to_char("NPCs can't train, go away.\r\n", ch);
    return TRUE;
  }

  if (GET_TRADITION(ch) != TRAD_ADEPT) {
    if (PLR_FLAGGED(ch, PLR_PAID_FOR_CLOSECOMBAT)) {
      snprintf(arg, sizeof(arg), "%s You already know all I can teach you about Close Combat.", GET_CHAR_NAME(ch));
    }

    else {
      if (!*argument) {
        snprintf(arg, sizeof(arg), "%s The only thing I can teach you is the art of Close Combat.", GET_CHAR_NAME(ch));
      } else {
        // at this point we just assume they typed 'train art' or 'train close' or anything else.
        if (GET_KARMA(ch) >= KARMA_COST_FOR_CLOSECOMBAT) {
          send_to_char("You drill with your teacher on closing the distance and entering your opponent's range, and you come away feeling like you're better-equipped to fight the hulking giants of the world.\r\n", ch);
          send_to_char("(OOC: You've unlocked the ^WCLOSECOMBAT^n command!)\r\n", ch);
          snprintf(arg, sizeof(arg), "%s Good job. You've now learned everything you can from me.", GET_CHAR_NAME(ch));

          GET_KARMA(ch) -= KARMA_COST_FOR_CLOSECOMBAT;
          PLR_FLAGS(ch).SetBit(PLR_PAID_FOR_CLOSECOMBAT);
        } else {
          send_to_char(ch, "You need %0.2f karma to learn close combat.\r\n", (float) KARMA_COST_FOR_CLOSECOMBAT / 100);
          return TRUE;
        }
      }
    }
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

  // Sanity checks: Newbie trainers only train newbies; newbies cannot train at non-newbie trainers.
  if (adepts[ind].is_newbie && !PLR_FLAGGED(ch, PLR_NEWBIE)) {
    snprintf(arg, sizeof(arg), "%s You do not belong here.", GET_CHAR_NAME(ch));
    do_say(trainer, arg, 0, SCMD_SAYTO);
    snprintf(buf, sizeof(buf), "Error: Character %s is not a newbie but is attempting to train in Chargen.", GET_CHAR_NAME(ch));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return TRUE;
  }

  /*
  if (!adepts[ind].is_newbie && PLR_FLAGGED(ch, PLR_NEWBIE)) {
    snprintf(arg, sizeof(arg), "%s You're not quite ready yet!", GET_CHAR_NAME(ch));
    do_say(trainer, arg, 0, SCMD_SAYTO);
    send_to_char(ch, "(^mOOC^n: You can't train until you've earned at least %d karma.)\r\n", NEWBIE_KARMA_THRESHOLD + 1);
    return TRUE;
  }
  */

  // Exploit prevention: You're not allowed to train before allocating attributes to avoid discounted training of improved-attribute powers.
  if (GET_ATT_POINTS(ch) > 0) {
    snprintf(arg, sizeof(arg), "%s You must go train your attributes fully before you see me.", GET_CHAR_NAME(ch));
    do_say(trainer, arg, 0, SCMD_SAYTO);
    return TRUE;
  }

  // List the powers available to train if they don't supply an argument.
  if (!*argument) {
    int num = 0;
    for (i = 1; i < ADEPT_NUMPOWER; i++)
      if (adepts[ind].skills[i])
        num++;
    snprintf(buf, sizeof(buf), "You can learn the following abilit%s:\r\n", num == 1 ? "y" : "ies");
    for (i = 1; i < ADEPT_NUMPOWER; i++)
      if (adepts[ind].skills[i] && GET_POWER_TOTAL(ch, i) < max_ability(i) && GET_POWER_TOTAL(ch, i) < adepts[ind].skills[i])
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%30s (%0.2f points)\r\n", adept_powers[i],
                ((float) train_ability_cost(ch, i, GET_POWER_TOTAL(ch, i) + 1)/ 100));
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nYou have %0.2f power point%s to "
            "distribute to your abilities.\r\n", ((float)GET_PP(ch) / 100),
            ((GET_PP(ch) != 100) ? "s" : ""));
    send_to_char(buf, ch);

    if (!PLR_FLAGGED(ch, PLR_PAID_FOR_CLOSECOMBAT)) {
      send_to_char(ch, "You can also learn Close Combat for %0.2f karma.", (float) KARMA_COST_FOR_CLOSECOMBAT / 100);
    }
    return TRUE;
  }

  // Search for their selected power.
  for (power = 1; power < ADEPT_NUMPOWER; power++)
    if (is_abbrev(argument, adept_powers[power]) && adepts[ind].skills[power])
      break;

  // If they specified an invalid power, break out.
  if (power == ADEPT_NUMPOWER) {
    if (str_str(argument, "close") || str_str(argument, "combat") || str_str(argument, "closecombat")) {
      if (PLR_FLAGGED(ch, PLR_PAID_FOR_CLOSECOMBAT)) {
        snprintf(arg, sizeof(arg), "%s You already know all I can teach you about close combat.", GET_CHAR_NAME(ch));
      }

      else {
        // at this point we just assume they typed 'train art' or 'train close' or anything else.
        if (GET_KARMA(ch) >= KARMA_COST_FOR_CLOSECOMBAT) {
          send_to_char("You drill with your teacher on closing the distance and entering your opponent's range, and you come away feeling like you're better-equipped to fight the hulking giants of the world.\r\n", ch);
          send_to_char("(OOC: You've unlocked the ^WCLOSECOMBAT^n command!)\r\n", ch);
          snprintf(arg, sizeof(arg), "%s Good job. You've now learned everything you can from me.", GET_CHAR_NAME(ch));

          GET_KARMA(ch) -= KARMA_COST_FOR_CLOSECOMBAT;
          PLR_FLAGS(ch).SetBit(PLR_PAID_FOR_CLOSECOMBAT);
        } else {
          send_to_char(ch, "You need %0.2f karma to learn close combat.\r\n", (float) KARMA_COST_FOR_CLOSECOMBAT / 100);
          return TRUE;
        }
      }
      do_say(trainer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }

    send_to_char(ch, "Which power do you wish to train?\r\n");
    return TRUE;
  }

  // Trainer power limits.
  if (GET_POWER_TOTAL(ch, power) >= adepts[ind].skills[power] ||
      GET_POWER_TOTAL(ch, power) >= max_ability(power)) {
    send_to_char("You have advanced beyond the teachings of your trainer.\r\n", ch);
    return 1;
  }

  // Character power limits.
  if (GET_POWER_TOTAL(ch, power) >= GET_MAG(ch) / 100) {
    send_to_char(ch, "Your magic isn't strong enough to allow you to advance further in that discipline.\r\n");
    return TRUE;
  }

  // Check to see if they can afford the cost of training the selected power.
  int cost = train_ability_cost(ch, power, GET_POWER_TOTAL(ch, power) + 1);
  if (GET_PP(ch) < cost) {
    send_to_char("You don't have enough power points to raise that ability.\r\n", ch);
    return TRUE;
  }

  // Subtract their PP and increase their power level.
  GET_PP(ch) -= cost;
  SET_POWER_TOTAL(ch, power, GET_POWER_TOTAL(ch, power) + 1);
  send_to_char("After hours of focus and practice, you feel your ability sharpen.\r\n", ch);

  // Post-increase messaging to let them know they've maxed out.
  if (GET_POWER_TOTAL(ch, power) >= GET_MAG(ch) / 100)
    send_to_char(ch, "You feel you've reached the limits of your magical ability in %s.\r\n", adept_powers[power]);

  // If they haven't maxed out but their teacher has, let them know that instead.
  else if (GET_POWER_TOTAL(ch, power) >= max_ability(power) || GET_POWER_TOTAL(ch, power) >= adepts[ind].skills[power])
    send_to_char(ch, "You have learned all your teacher knows about %s.\r\n", adept_powers[power]);

  // Update character and end routine.
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

  for (vict = ch->in_room->people; vict; vict = vict->next_in_room)
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
  ACMD_DECLARE(do_gen_comm);

  if (cmd || FIGHTING(ch) || !AWAKE(ch))
    return 0;

  FOR_ITEMS_AROUND_CH(jan, i) {
    if (!CAN_WEAR(i, ITEM_WEAR_TAKE) || IS_OBJ_STAT(i, ITEM_CORPSE) || i->obj_flags.quest_id)
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

  for (tch = ch->in_room->people; tch; tch = tch->next_in_room)
    if (IS_NPC(tch) && CAN_SEE(ch, tch) && PLR_FLAGGED(tch, PLR_KILLER)) {
      act("$n screams, 'Hey, it's one of those fraggin unreg'ed pkers!!!'",
          FALSE, ch, 0, 0, TO_ROOM);
      set_fighting(ch, tch);
      return (TRUE);
    }

  switch (GET_MOB_VNUM(ch)) {
    case 2013:
    case 5100:
      for (tch = ch->in_room->people; tch; tch = tch->next_in_room)
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
      for (tch = ch->in_room->people; tch; tch = tch->next_in_room)
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

  if (!cmd || ch->in_veh || !ch->in_room)
    return FALSE;

  if (!(CMD_IS("list") || CMD_IS("buy") || CMD_IS("info") || CMD_IS("probe")))
    return FALSE;

  if (IS_NPC(ch)) {
    send_to_char("You're having a hard time getting the dealer's attention.\r\n", ch);
    return FALSE;
  }

  int car_room = real_room(ch->in_room->number - 1);

  if (CMD_IS("list")) {
    send_to_char("Available vehicles are:\r\n", ch);
    for (veh = world[car_room].vehicles; veh; veh = veh->next_veh)
      send_to_char(ch, "%8d - %s\r\n", veh->cost, capitalize(GET_VEH_NAME(veh)));
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
    lose_nuyen(ch, veh->cost, NUYEN_OUTFLOW_VEHICLE_PURCHASES);
    newveh = read_vehicle(veh->veh_number, REAL);
    veh_to_room(newveh, ch->in_room);
    newveh->owner = GET_IDNUM(ch);
    newveh->idnum = number(0, 1000000);
    if (veh->type == VEH_DRONE)
      send_to_char(ch, "You buy %s. It is brought out into the room.\r\n", GET_VEH_NAME(newveh));
    else
      send_to_char(ch, "You buy %s. It is wheeled out into the yard.\r\n", GET_VEH_NAME(newveh));
    save_vehicles();
    return TRUE;
  } else if (CMD_IS("probe") || CMD_IS("info")) {
    argument = one_argument(argument, buf);
    if (!(veh = get_veh_list(buf, world[car_room].vehicles, ch))) {
      send_to_char("There is no such vehicle for sale.\r\n", ch);
      return TRUE;
    }
    send_to_char(ch, "^yProbing shopkeeper's ^n%s^y...^n\r\n", GET_VEH_NAME(veh));
    do_probe_veh(ch, veh);
    return TRUE;
  }
  return FALSE;
}

SPECIAL(pike) {
  NO_DRAG_BULLSHIT;

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
      if (pike->in_room->number != 2337) {
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
            GET_NUYEN_RAW(pike) += amount;
            lose_nuyen(ch, amount, NUYEN_OUTFLOW_GENERIC_SPEC_PROC);
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
    if (perform_move(ch, WEST, LEADER, NULL) && pike->in_room->number == 2337) {
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

SPECIAL(jeff) {
  NO_DRAG_BULLSHIT;

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

    if (!*arg3 || (strcasecmp("jeff", arg3) && (strcasecmp("to", arg3) || strcasecmp("jeff", temp))))
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
      if (jeff->in_room->number != 2326) {
        do_say(jeff, "Where the frag am I?", 0, 0);
      } else {
        if (IS_SET(EXIT(jeff, EAST)->exit_info, EX_CLOSED)) {
          do_gen_door(jeff, "roadblock", 0, SCMD_OPEN);
          GET_NUYEN_RAW(jeff) += amount;
          lose_nuyen(ch, amount, NUYEN_OUTFLOW_GENERIC_SPEC_PROC);
          do_say(jeff, "Go ahead.", 0, 0);
        } else {
          do_say(jeff, "The roadblock is open, so go ahead.", 0, 0);
        }
      }
      return TRUE;
    }
  } else if (CMD_IS("east")) {
    if (perform_move(ch, EAST, LEADER, NULL) && jeff->in_room->number == 2326) {
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

  for (tch = ch->in_room->people; tch; tch = tch->next_in_room) {
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

  for (tch = ch->in_room->people; tch; tch = tch->next_in_room) {
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
      GET_NUYEN_RAW(ch) += gold;
      lose_nuyen(vict, gold, NUYEN_OUTFLOW_GENERIC_SPEC_PROC);
      return TRUE;
    }
    return FALSE;
  }

  if (!FIGHTING(ch)) {
    FOR_ITEMS_AROUND_CH(ch, obj)
      if (GET_OBJ_TYPE(obj) == ITEM_MONEY) {
        act("$n grins as he picks up $p from the ground.", FALSE, ch, obj, 0, TO_ROOM);
        act("You grin slightly as you pick up $p.", FALSE, ch, obj, 0, TO_CHAR);
        GET_NUYEN_RAW(ch) += GET_ITEM_MONEY_VALUE(obj);
        extract_obj(obj);
        return TRUE;
      }
    for (vict = ch->in_room->people; vict; vict = vict->next_in_room) {
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

SPECIAL(gate_guard_park) {
  NO_DRAG_BULLSHIT;

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
        guard->in_room->number == MANSION_GATE) {
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

  for(vict = ch->in_room->people; vict; vict = vict->next_in_room) {
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
    for (vict = ch->in_room->people; vict; vict = vict->next_in_room) {
      if (vict != ch && CAN_SEE(ch, vict) && (IS_NPC(vict) ||
                                              (GET_TKE(vict) > 10 && !IS_SENATOR(vict))) &&
          ch->in_room->number == 4098) {
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
    for(vict = tsuyama->in_room->people; vict; vict = vict->next_in_room) {
      if (vict != ch && CAN_SEE(tsuyama, vict) && !IS_NPC(vict)
          && !IS_SENATOR(vict) && number(0, 3)
          && tsuyama->in_room->number == 4101) {
            act("$n unsheathes $s deadly katana, swiftly attacking $N!",
                FALSE, tsuyama, 0, vict, TO_NOTVICT);
            act("$n unsheathes $s deadly katana, swiftly attacking you!",
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

  for (vict = tsuyama->in_room->people; vict; vict = vict->next_in_room)
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
  int dist, dir, range = 0;
  struct room_data *room = NULL, *nextroom = NULL;

  if (cmd || !AWAKE(ch) || FIGHTING(ch))
    return FALSE;

  range = find_sight(ch);

  for (vict = ch->in_room->people; vict; vict = vict->next_in_room)
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
      nextroom = NULL;

    for (dist = 1; nextroom && dist <= range; dist++) {
      for (vict = nextroom->people; vict; vict = vict->next_in_room)
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
        nextroom = NULL;
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

SPECIAL(saeder_guard) {
  NO_DRAG_BULLSHIT;

  struct char_data *guard = (char_data *) me;
  struct obj_data *obj;
  bool found = FALSE;

  if (!AWAKE(guard) || (GET_POS(guard) == POS_FIGHTING))
    return(FALSE);

  if (CMD_IS("east") && CAN_SEE(guard, ch) && guard->in_room->number == 4930) {
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

SPECIAL(crime_mall_guard) {
  NO_DRAG_BULLSHIT;

  if (!cmd)
    return FALSE;

  struct char_data *guard = (struct char_data *) me;

  if ((guard->in_room->number == 100075 && CMD_IS("east")) ||
      (guard->in_room->number == 100077 && CMD_IS("west"))) {
    if (GET_NUYEN(ch) < 2000000) {
      act("$n shakes $s head as $e shoves you back. \"Come back when you're richer, chummer.\"",
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
      send_to_char(ch, "You don't seem to have %s %s.\r\n", AN(argument), argument);
      return(TRUE);
    }
    if (GET_OBJ_TYPE(obj) != ITEM_MONEY || !GET_OBJ_VAL(obj, 1) || GET_ITEM_MONEY_VALUE(obj) <= 0 ||
        !GET_ITEM_MONEY_CREDSTICK_ACTIVATED(obj) || belongs_to(ch, obj)) {
      snprintf(arg, sizeof(arg), "%s Why are you bringing this to me?", GET_CHAR_NAME(ch));
      do_say(hacker, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if (GET_OBJ_VAL(obj, 2) == 1)
      amount = (int)(GET_OBJ_VAL(obj, 0) / 8);
    else if (GET_OBJ_VAL(obj, 2) == 2)
      amount = (int)(GET_OBJ_VAL(obj, 0) / 5);
    else
      amount = (int)(GET_OBJ_VAL(obj, 0) / 3);
    snprintf(arg, sizeof(arg), "%s I'd charge about %d nuyen for that.", GET_CHAR_NAME(ch), amount);
    do_say(hacker, arg, 0, SCMD_SAYTO);
    return TRUE;
  } else if (CMD_IS("give")) {
    if (!*argument) {
      send_to_char("Give what to whom?\r\n", ch);
      return TRUE;
    }
    any_one_arg(any_one_arg(argument, buf), buf1);

    if (!(obj = get_obj_in_list_vis(ch, buf, ch->carrying))) {
      send_to_char(ch, "You don't seem to have %s %s.\r\n", AN(buf), buf);
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
      snprintf(arg, sizeof(arg), "%s Why are you bringing this to me?", GET_CHAR_NAME(ch));
      do_say(hacker, arg, 0, SCMD_SAYTO);
      return TRUE;
    }

    if (!perform_give(ch, hacker, obj))
      return TRUE;

    if (GET_OBJ_VAL(obj, 2) == 1)
      amount = (int)(GET_ITEM_MONEY_VALUE(obj) / 7);
    else if (GET_OBJ_VAL(obj, 2) == 2)
      amount = (int)(GET_ITEM_MONEY_VALUE(obj) / 5);
    else
      amount = (int)(GET_ITEM_MONEY_VALUE(obj) / 3);
    // nuyen = negotiate(ch, hacker, 0, nuyen, 2, FALSE);
    nuyen = GET_ITEM_MONEY_VALUE(obj) - amount;
    GET_BANK_RAW(hacker) += amount;

    // We use the raw value here because this was already counted as a faucet when picked up.
    GET_BANK_RAW(ch) += GET_ITEM_MONEY_VALUE(obj);

    // In fact, since the amount was also part of the faucet, we have to decrease the farm amount by that.
    lose_bank(ch, amount, NUYEN_OUTFLOW_CREDSTICK_CRACKER);

    snprintf(buf1, sizeof(buf1), "%s Updated. %d nuyen transferred to your bank account, I took a cut of %d.",
            GET_CHAR_NAME(ch), nuyen, amount);
    do_say(hacker, buf1, 0, SCMD_SAYTO);
    extract_obj(obj);
    return TRUE;
  } else if (CMD_IS("sell")) {
    send_to_char(ch, "You can't do that here. You can GIVE credsticks to %s for cracking.\r\n", GET_CHAR_NAME(hacker));
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
      send_to_char(ch, "You don't seem to have %s %s.\r\n", AN(argument), argument);
      return(TRUE);
    }
    if (!(GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY
          && GET_DECK_ACCESSORY_TYPE(obj) == TYPE_FILE
          && GET_DECK_ACCESSORY_FILE_HOST_VNUM(obj))) {
      act("You say, \"I only buy datafiles, chummer.\"\n", FALSE, fence, 0, 0, TO_CHAR);
      act("$n says, \"I only buy datafiles, chummer.\"\n", FALSE, fence, 0, ch, TO_VICT);
      return(TRUE);
    }
    int negotiated_value = negotiate(ch, fence, SKILL_DATA_BROKERAGE, market[GET_DECK_ACCESSORY_FILE_HOST_COLOR(obj)], 2, FALSE);
    value = negotiated_value / MAX(1, (time(0) - GET_DECK_ACCESSORY_FILE_CREATION_TIME(obj)) / SECS_PER_MUD_DAY);
    gain_nuyen(ch, value, NUYEN_INCOME_DECKING);

    snprintf(buf, sizeof(buf), "Paying %d nuyen for %s^g paydata (base %d, after roll %d, then time decay). Market going from %d to ",
             value,
             host_sec[GET_DECK_ACCESSORY_FILE_HOST_COLOR(obj)],
             market[GET_OBJ_VAL(obj, 4)],
             negotiated_value,
             market[GET_DECK_ACCESSORY_FILE_HOST_COLOR(obj)]);

    market[GET_DECK_ACCESSORY_FILE_HOST_COLOR(obj)] -= (int)(market[GET_DECK_ACCESSORY_FILE_HOST_COLOR(obj)] * ((float)(5 - GET_DECK_ACCESSORY_FILE_HOST_COLOR(obj))/ 50));
    if (market[GET_DECK_ACCESSORY_FILE_HOST_COLOR(obj)] < get_paydata_market_minimum(GET_DECK_ACCESSORY_FILE_HOST_COLOR(obj)))
      market[GET_DECK_ACCESSORY_FILE_HOST_COLOR(obj)] = get_paydata_market_minimum(GET_DECK_ACCESSORY_FILE_HOST_COLOR(obj));

    snprintf(ENDOF(buf), sizeof(buf), "%d.", market[GET_DECK_ACCESSORY_FILE_HOST_COLOR(obj)]);
    mudlog(buf, ch, LOG_ECONLOG, TRUE);

    obj_from_char(obj);
    extract_obj(obj);
    snprintf(buf, sizeof(buf), "%s says, \"Here's your %d creds.\"\r\n",
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
  sh_int extra, hour, day = 0, pm = 0;

  if (cmd && !CMD_IS("repair") && !CMD_IS("list") && !CMD_IS("receive"))
    return FALSE;

  if (cmd && (!AWAKE(fixer) || IS_NPC(ch)))
    return FALSE;
  if (cmd && !CAN_SEE(fixer, ch)) {
    do_say(fixer, "I don't deal with someone I can't see!", 0, 0);
    return TRUE;
  }

  if (CMD_IS("repair")) {
    skip_spaces(&argument);

    if (!*argument) {
      send_to_char("Syntax: REPAIR [cash|credstick] <item>\r\n", ch);
      return TRUE;
    }

    // Pick the first argument, but don't re-index *argument yet.
    any_one_arg(argument, buf);

    bool force_cash = !str_cmp(buf, "cash");
    bool force_credstick = !str_cmp(buf, "credstick");

    if (force_cash || force_credstick) {
      // Actually re-index *argument now.
      argument = any_one_arg(argument, buf);
      skip_spaces(&argument);

      // Yes, we need this twice, because we stripped out an argument item in cash/credstick checking.
      if (!*argument) {
        send_to_char("Syntax: REPAIR [cash|credstick] <item>\r\n", ch);
        return TRUE;
      }
    }

    // We default to cash-- only look for a credstick if they used the credstick command.
    if (force_credstick && !(credstick = get_first_credstick(ch, "credstick"))) {
      send_to_char("You don't have a credstick.\r\n", ch);
      return TRUE;
    }

    if (!(obj = get_obj_in_list_vis(ch, argument, ch->carrying))) {
      send_to_char(ch, "You don't seem to have %s %s.\r\n", AN(argument), argument);
      return TRUE;
    }
    if (IS_OBJ_STAT(obj, ITEM_CORPSE) || IS_OBJ_STAT(obj, ITEM_IMMLOAD) || IS_OBJ_STAT(obj, ITEM_WIZLOAD)) {
      snprintf(arg, sizeof(arg), "%s I can't repair that.", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if (GET_OBJ_CONDITION(obj) >= GET_OBJ_BARRIER(obj)) {
      snprintf(arg, sizeof(arg), "%s %s doesn't need to be repaired!",
              GET_CHAR_NAME(ch), CAP(obj->text.name));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if ((IS_CARRYING_N(fixer) >= CAN_CARRY_N(fixer)) ||
        ((GET_OBJ_WEIGHT(obj) + IS_CARRYING_W(fixer)) > CAN_CARRY_W(fixer))) {
      snprintf(arg, sizeof(arg), "%s I've got my hands full...come back later.", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    cost = (int)((GET_OBJ_COST(obj) / (2 * (GET_OBJ_BARRIER(obj) > 0 ? GET_OBJ_BARRIER(obj) : 1)) *
                 (GET_OBJ_BARRIER(obj) - GET_OBJ_CONDITION(obj))));
    if ((credstick ? GET_ITEM_MONEY_VALUE(credstick) : GET_NUYEN(ch)) < cost) {
      snprintf(arg, sizeof(arg), "%s You can't afford to repair that! It'll cost %d nuyen.", GET_CHAR_NAME(ch), cost);
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if (!perform_give(ch, fixer, obj))
      return TRUE;
    if (credstick)
      lose_nuyen_from_credstick(ch, credstick, cost, NUYEN_OUTFLOW_REPAIRS);
    else
      lose_nuyen(ch, cost, NUYEN_OUTFLOW_REPAIRS);
    extra = (int)((GET_OBJ_BARRIER(obj) - GET_OBJ_CONDITION(obj)) / 2);
    if (((GET_OBJ_BARRIER(obj) - GET_OBJ_CONDITION(obj)) % 2) > 0)
      extra++;
    if ((time_info.hours + extra) > 23)
      day = 1;
    else
      pm = ((time_info.hours + extra) >= 12);
    hour = ((time_info.hours + extra) % 12 == 0 ? 12 :
            (time_info.hours + extra) % 12);
    snprintf(arg, sizeof(arg), "%s That'll be %d nuyen.  Should be ready by about %d %s%s.",
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
          snprintf(arg, sizeof(arg), "%s I currently am in possession of the following:", GET_CHAR_NAME(ch));
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
      snprintf(arg, sizeof(arg), "%s I don't have anything of yours.", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
    }
    return TRUE;
  } else if (CMD_IS("receive")) {
    int j = 0;
    char tmpvar[LINE_LENGTH], *temp = tmpvar;
    any_one_arg(argument, temp);

    if (!*temp) {
      snprintf(arg, sizeof(arg), "%s What do you want to retrieve?", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if (!(extra = get_number(&temp))) {
      snprintf(arg, sizeof(arg), "%s I don't have anything like that.", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    for (obj = fixer->carrying; obj && j <= extra; obj = obj->next_content)
      if (GET_OBJ_TIMER(obj) == GET_IDNUM(ch) &&
          isname(temp, obj->text.keywords))
        if (++j == extra)
          break;
    if (!obj) {
      snprintf(arg, sizeof(arg), "%s I don't have anything like that.", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if (GET_OBJ_CONDITION(obj) < GET_OBJ_BARRIER(obj)) {
      snprintf(arg, sizeof(arg), "%s %s isn't ready yet.", GET_CHAR_NAME(ch), CAP(obj->text.name));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if ((IS_CARRYING_N(ch) >= CAN_CARRY_N(ch)) ||
        ((GET_OBJ_WEIGHT(obj) + IS_CARRYING_W(ch)) > CAN_CARRY_W(ch))) {
      snprintf(arg, sizeof(arg), "%s You can't carry it right now.", GET_CHAR_NAME(ch));
      do_say(fixer, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
    if (!perform_give(fixer, ch, obj)) {
      snprintf(arg, sizeof(arg), "%s That's odd...I can't let go of it.", GET_CHAR_NAME(ch));
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

SPECIAL(huge_troll) {
  NO_DRAG_BULLSHIT;

  struct char_data *troll = (struct char_data *) me;
  struct obj_data *obj;

  if (CMD_IS("west") && troll->in_room->number == 9437 && CAN_SEE(troll, ch)) {
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
      if (ch->in_room->number == 1844 ||
          ch->in_room->number == 1846) {
        perform_move(ch, NORTH, CHECK_SPECIAL | LEADER, NULL);
        return TRUE;
      }
      break;
    case SOUTH:
      if (ch->in_room->number == 1844 ||
          ch->in_room->number == 1845) {
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

  if (yukiya != NULL && (CMD_IS("open") || CMD_IS("hit") || CMD_IS("shoot") || CMD_IS("up")) && CAN_SEE(yukiya, ch) &&
      yukiya->in_room->number == YUKIYA_OFFICE) {
    skip_spaces(&argument);

    if (str_str("vent", argument)) {
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

SPECIAL(smiths_bouncer) {
  NO_DRAG_BULLSHIT;

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

    if (found || IS_NPC(ch) || access_level(ch, LVL_ADMIN))
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
    if (success_test(skill, target) <= 0) {
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
        lose_nuyen(ch, GET_OBJ_COST(temp), NUYEN_OUTFLOW_GENERIC_SPEC_PROC);
        temp = read_object(GET_OBJ_RNUM(temp), REAL);
        obj_to_char(temp, ch);
        act("$n buys $p from $P.", FALSE, ch, temp, obj, TO_ROOM);
        act("You now have $p.", FALSE, ch, temp, 0, TO_CHAR);
        return TRUE;
      }
    send_to_char(ch, "%s doesn't sell '%s'.\r\n", obj->text.name, arg);
    return TRUE;
  }
}

SPECIAL(hand_held_scanner)
{
  struct char_data *temp;
  struct obj_data *scanner = (struct obj_data *) me;
  int i, dir;

  if (!cmd || !scanner->worn_by || !ch->in_room || number(1, 10) > 4)
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

  if (ch->in_room->dir_option[dir] &&
      ch->in_room->dir_option[dir]->to_room) {
    for (i = NORTH; i < NUM_OF_DIRS; i++)
      if (ch->in_room->dir_option[i] &&
          ch->in_room->dir_option[i]->to_room)
        for (temp = ch->in_room->dir_option[i]->to_room->people;
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
  int ticket, real_obj;

  if (!cmd)
    return FALSE;

  if (zone_table[ch->in_room->zone].number == 30)
    ticket = SEATAC_TICKET;
  else
    ticket = SEATTLE_TICKET;

  if ((real_obj = real_object(ticket)) < 0) {
    send_to_char(ch, "A red light flashes on the VendTix-- it's currently out of order.\r\n");
    mudlog("SYSERR: Invalid vnum for VendTix!", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (CMD_IS("list")) {
    send_to_char(ch, "Ticket price is %d nuyen.\r\n", obj_proto[real_obj].obj_flags.cost);
    act("$n presses some buttons on $p.", TRUE, ch, vendtix, 0, TO_ROOM);
    return TRUE;
  }

  if (CMD_IS("buy")) {
    if (!is_abbrev(arg, "ticket")) {
      send_to_char("This machine only sells tickets.\r\n", ch);
      return TRUE;
    }

    if ((GET_NUYEN(ch) - obj_proto[real_obj].obj_flags.cost) < 0) {
      send_to_char("You don't have enough nuyen!\r\n", ch);
      return TRUE;
    }

    struct obj_data *tobj = read_object(ticket, VIRTUAL);
    if (!tobj) {
      mudlog("No ticket for the Vend-Tix machine!", ch, LOG_SYSLOG, TRUE);
      return TRUE;
    }

    obj_to_char(tobj, ch);
    lose_nuyen(ch, tobj->obj_flags.cost, NUYEN_OUTFLOW_GENERIC_SPEC_PROC);
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
    return TRUE;
  }

  if (CMD_IS("balance")) {
    if (GET_BANK(ch) > 0)
      send_to_char(ch, "Your current balance is %ld nuyen.\r\n", GET_BANK(ch));
    else
      send_to_char("Your account is empty!\r\n", ch);
    return 1;
  }

  else if (CMD_IS("deposit")) {
    if ((amount = atoi(argument)) <= 0 && str_cmp(buf, "all")) {
      send_to_char("How much do you want to deposit?\r\n", ch);
      return 1;
    }
    if (!str_cmp(buf, "all") || GET_NUYEN(ch) < amount)
      amount = GET_NUYEN(ch);

    // Raw amounts-- we don't care about transfering money around like this.
    GET_NUYEN_RAW(ch) -= amount;
    GET_BANK_RAW(ch) += amount;
    send_to_char(ch, "You deposit %d nuyen.\r\n", amount);
    act("$n accesses the ATM.", TRUE, ch, 0, FALSE, TO_ROOM);
    return 1;
  }

  else if (CMD_IS("withdraw")) {
    if ((amount = atoi(argument)) <= 0 && str_cmp(buf, "all")) {
      send_to_char("How much do you want to withdraw?\r\n", ch);
      return 1;
    }
    if (!str_cmp(buf, "all") || GET_BANK(ch) < amount)
      amount = GET_BANK(ch);

    // Raw amounts-- we don't care about transfering money around like this.
    GET_NUYEN_RAW(ch) += amount;
    GET_BANK_RAW(ch) -= amount;
    send_to_char(ch, "The ATM ejects %d nuyen and updates your bank account.\r\n", amount);
    act("$n accesses the ATM.", TRUE, ch, 0, FALSE, TO_ROOM);
    return 1;
  }

  else if (CMD_IS("transfer")) {
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
      if (!str_cmp(buf,"all") || GET_OBJ_VAL(credstick, 0) < amount) {
        amount = GET_OBJ_VAL(credstick, 0);
      }
      if (GET_ITEM_MONEY_VALUE(credstick) == 0) {
        send_to_char(ch, "%s is already empty.\r\n", capitalize(GET_OBJ_NAME(credstick)));
        return TRUE;
      }
      // Just transfers, no faucet/sink.
      GET_ITEM_MONEY_VALUE(credstick) -= amount;
      GET_BANK_RAW(ch) += amount;
      snprintf(buf, sizeof(buf), "%d nuyen transferred from $p to your account.", amount);
    } else if (!str_cmp(buf1, "credstick")) {
      if (!str_cmp(buf,"all") || GET_BANK(ch) < amount) {
        amount = GET_BANK(ch);
      }
      if (GET_BANK(ch) == 0) {
        send_to_char("You don't have any nuyen in the bank.\r\n", ch);
        return TRUE;
      }
      // Just transfers, no faucet / sink.
      GET_ITEM_MONEY_VALUE(credstick) += amount;
      GET_BANK_RAW(ch) -= amount;
      snprintf(buf, sizeof(buf), "%d nuyen transferred from your account to $p.", amount);
    } else {
      send_to_char("Transfer to what? (Type out \"credstick\" or \"account\", please.)\r\n", ch);
      return TRUE;
    }
    act(buf, FALSE, ch, credstick, 0, TO_CHAR);
    act("$n accesses the ATM.", TRUE, ch, 0, 0, TO_ROOM);
    return TRUE;
  }

  else if (CMD_IS("wire")) {
    any_one_arg(any_one_arg(argument, buf), buf1);
    if ((amount = atoi(buf)) <= 0)
      send_to_char("How much do you wish to wire?\r\n", ch);
    else if (amount > GET_BANK(ch))
      send_to_char("You don't have that much nuyen.\r\n", ch);
    else if (!*buf1)
      send_to_char("Who do you want to wire funds to?\r\n", ch);
    else {
      vnum_t isfile = -1;
      if ((isfile = get_player_id(buf1)) == -1) {
        send_to_char("It won't let you transfer to that account.\r\n", ch);
        return TRUE;
      }
      wire_nuyen(ch, amount, isfile);
      char *cname = get_player_name(isfile);
      send_to_char(ch, "You wire %d nuyen to %s's account.\r\n", amount, cname);
      delete [] cname;
    }
    return TRUE;
  }
  return 0;
}

SPECIAL(toggled_invis)
{
  struct obj_data *obj = (struct obj_data *) me;

  if(!obj->worn_by)
    return FALSE;

  if (CMD_IS("deactivate")) {
    skip_spaces(&argument);
    if (!str_cmp(argument, "invis") || isname(argument, GET_OBJ_KEYWORDS(obj))) {
      if (obj->obj_flags.bitvector.IsSet(AFF_INVISIBLE)) {
        AFF_FLAGS(obj->worn_by).RemoveBit(AFF_INVISIBLE);
        obj->obj_flags.bitvector.RemoveBit(AFF_INVISIBLE);
        send_to_char(ch, "You feel the static fade as the ruthenium polymers in %s power down.\r\n", GET_OBJ_NAME(obj));
        act("The air shimmers briefly as $n fades into view.", FALSE, ch, 0, 0, TO_ROOM);
        return TRUE;
      } else {
        send_to_char(ch, "%s is already deactivated.\r\n", capitalize(GET_OBJ_NAME(obj)));
        return TRUE;
      }
    }
    return FALSE;
  }

  if (CMD_IS("activate")) {
    skip_spaces(&argument);
    if (!str_cmp(argument, "invis") || isname(argument, GET_OBJ_KEYWORDS(obj))) {
      if (!obj->obj_flags.bitvector.IsSet(AFF_INVISIBLE)) {
        AFF_FLAGS(obj->worn_by).SetBit(AFF_INVISIBLE);
        obj->obj_flags.bitvector.SetBit(AFF_INVISIBLE);
        send_to_char(ch, "You feel a tiny static charge as the ruthenium polymers in %s power up.\r\n", GET_OBJ_NAME(obj));
        act("The world bends around $n as $e vanishes from sight.", FALSE, ch, 0, 0, TO_ROOM);
        return TRUE;
      } else {
        send_to_char(ch, "%s is already activated.\r\n", capitalize(GET_OBJ_NAME(obj)));
        return TRUE;
      }
    }
    return FALSE;
  }

  return FALSE;
}

const char *traffic_messages[] = {
  "A man on a Yamaha Rapier zips by.\r\n", // 0
  "A Mitsuhama Nightsky limousine slowly drives by.\r\n",
  "A Ford Bison drives through here, splashing mud on you.\r\n",
  "A Lone Star squad car drives by, sirens blaring loudly.\r\n",
  "An orkish woman drives through here on her Harley Scorpion.\r\n",
  "An elf drives through here on his decked-out Yamaha Rapier.\r\n", // 5
  "A ^rred^n Chrysler-Nissan Jackrabbit cruises by.\r\n",
  "A ^yyellow^n Chrysler-Nissan Jackrabbit cruises by.\r\n",
  "A ^Wwhite^n Chrysler-Nissan Jackrabbit cruises by.\r\n",
  "A ^rred^n Ford Americar cruises by.\r\n",
  "A ^yyellow^n Ford Americar cruises by.\r\n", // 10
  "A ^Wwhite^n Ford Americar cruises by.\r\n",
  "A ^Bblue^n Ford Americar cruises by.\r\n",
  "A ^Bblue^n Chrysler-Nissan Jackrabbit cruises by.\r\n",
  "A ^Rcherry red^n Eurocar Westwind 2000 flies past you.\r\n",
  "A ^Wwhite^n Mitsubishi Runabout drives by slowly.\r\n", // 15
  "A ^bblue^n Mitsuhama Runabout drives by slowly.\r\n",
  "An elven woman on a Dodge Scoot passes through here.\r\n",
  "A ^Ybright yellow^n Volkswagen Electra passes by silently.\r\n",
  "A huge troll rides by on a modified BMW Blitzen 2050.\r\n",
  "A large, ^Wwhite^n GMC Bulldog van drives through here.\r\n", // 20
  "A DocWagon ambulance speeds past, its lights flashing brightly.\r\n",
  "The deep thrum of a helicopter passes swiftly overhead.\r\n",
  "A rugged-looking dwarf on a Rhiati Razor howls past.\r\n",
  "A MTC-Nissan roto-drone floats quietly on by.\r\n",
  "A souped-up Saab Dynamit 778 TI purrs past you.\r\n", // 25
  "A bleary-eyed wage slave putters past on an underpowered moped.\r\n",
  "An overloaded GMC Bulldog Security with open gun ports rumbles past.\r\n",
  "The sound of squealing tires echoes from somewhere in the distance.\r\n",
  "A troll on a rusted bicycle pedals squeakily by.\r\n",
  "A badly doppler-shifted track from The Elementals follows a truck speeding by.\r\n", // 30
  "A ^Lmatte-black^n LAV-93 roars through, narrowly missing you.\r\n",
  "A few sporadic pops of gunfire sound from somewhere in the distance.\r\n",
  "The stench of garbage wafts from somewhere nearby.\r\n",
  "A harried-looking salaryman hurries by.\r\n",
  "A backfiring Ford-Canada Bison splutters past.\r\n", // 35
  "A sleek ^rred^n roto-drone zips past.\r\n",
  "A crumpled-up plastic bag skitters past, carried by the wind.\r\n",
  "A poised and confident executive strides past, talking on her phone.\r\n",
  "An annoying-ass teen on a scooter does a drive-by on you with a squirt gun.\r\n",
  "A billboard nearby flickers with an ad for ^RChernobyl Vodka^n.\r\n", // 40
  "A billboard nearby displays an ad for ^rBrimstone ^RRed^n Ale^n.\r\n",
  "The greasy scent of fast food is carried to you on the breeze.\r\n"
};
#define NUM_TRAFFIC_MESSAGES 43

SPECIAL(traffic)
{
  struct room_data *room = (struct room_data *) me;

  if (!cmd && room->people)
    send_to_room(traffic_messages[number(0, NUM_TRAFFIC_MESSAGES - 1)], room);

  return FALSE;
}

SPECIAL(oceansounds)
{
  struct room_data *room = (struct room_data *) me;

  if (!cmd && room->people)
    switch (number(1, 100)) {
      case 1:
        send_to_room("A cool breeze blows over the ocean sending ripples across the water.\r\n", room);
        break;
      case 2:
        send_to_room("The cries of seagulls fill the air.\r\n", room);
        break;
      case 3:
        send_to_room("A lone bird skims across the surface of the water.\r\n", room);
        break;
      case 4:
        send_to_room("Water splashes as a fish disturbs the surface of a wave.\r\n", room);
        break;
      case 5:
        send_to_room("The waves continue their endless rhythm towards the shore.\r\n", room);
    }

  return FALSE;
}

SPECIAL(neophyte_entrance) {
  NO_DRAG_BULLSHIT;

  if (!cmd)
    return FALSE;

  if ((CMD_IS("south") || CMD_IS("enter")) && !PLR_FLAGGED(ch, PLR_NEWBIE)
      && !(IS_SENATOR(ch))) {
    send_to_char("The barrier prevents you from entering the guild.\r\n", ch);
    send_to_char(ch, "(^mOOC^n: You may only visit the training grounds until you have received %d karma.)\r\n", NEWBIE_KARMA_THRESHOLD);
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

SPECIAL(crime_mall_blockade) {
  NO_DRAG_BULLSHIT;

  if (!cmd)
    return FALSE;

  int found = 0;
  struct char_data *temp;

  for (temp = ch->in_room->people; temp && !found; temp = temp->next_in_room)
    if (IS_NPC(temp) && GET_MOB_VNUM(temp) == 100022)
      found = 1;
  if (!found)
    if ((ch->in_room->number == 100075 && CMD_IS("east")) || (ch->in_room->number == 100077 &&
                                                                   CMD_IS("west"))) {
      act("There seems to be an invisible barrier of some kind...", FALSE, ch, 0, 0, TO_CHAR);
      return TRUE;
    }
  return FALSE;
}

SPECIAL(circulation_fan) {
  NO_DRAG_BULLSHIT;

  static bool running = true;
  room_data *room = (struct room_data *) me;

  // Override to stop the fan from killing people.
  if (cmd)
    return false;

  if (running) {
    if (CMD_IS("north") && ch != NULL && !IS_ASTRAL(ch) && !IS_NPC(ch)) {
      act("\"Sharp, whirling metal fan blades can't hurt!\", "
          "you used to think...", FALSE, ch, 0, 0, TO_CHAR);
      act("A mist of $n's warm blood falls on you as $e walks into the fan.",
          FALSE, ch, 0, 0, TO_ROOM);

      // Deathlog Addendum
      snprintf(buf, sizeof(buf),"%s got chopped up into tiny bits. {%s (%ld)}",
              GET_CHAR_NAME(ch),
              ch->in_room->name, ch->in_room->number );
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
    snprintf(buf, sizeof(buf), "%s is wheeled out into the garage.\r\n", veh->short_description);
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
    snprintf(buf, sizeof(buf), "$n tells you, \"You can rent forever with the nuyen you have.\"");
  else
    snprintf(buf, sizeof(buf), "$n tells you, \"You can rent for %ld night%s with the nuyen you have.\r\n",
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
    case 39287:
    case 39288:
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
    case 62820:
      cost = 2.0;
      break;
    default:
      snprintf(buf, sizeof(buf), "SYSERR: Invalid loadroom %ld specified to find_hotel_cost. Defaulting to 6.0.", GET_LOADROOM(ch));
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
    case 62136:
      return 62179;
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
    case 39287:
      return 39288;
    case 62820:
      return 62820;
  }
  return room;
}

int Crash_offer_rent(struct char_data * ch, struct char_data * receptionist,
                     int display, int factor)
{
  char buf[MAX_INPUT_LENGTH];
  long totalcost = 0, i;

  i = GET_LOADROOM(ch);
  GET_LOADROOM(ch) = ch->in_room->number;

  totalcost = find_hotel_cost(ch);

  GET_LOADROOM(ch) = i;

  if (display)
  {
    snprintf(buf, sizeof(buf), "$n tells you, \"Plus, my %d credit fee..\"", 50 * factor);
    act(buf, FALSE, receptionist, 0, ch, TO_VICT);
    snprintf(buf, sizeof(buf), "$n tells you, \"For a total of %ld nuyen%s.\"",
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
  ACMD_DECLARE(do_action);

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
    snprintf(buf, sizeof(buf), "$n tells you, \"Rooms here are %d nuyen per night.\"", cost);
    act(buf, FALSE, recep, 0, ch, TO_VICT);
    return TRUE;
  }
  if (mode == RENT_FACTOR)
    snprintf(buf, sizeof(buf), "$n tells you, \"Rooms are %d nuyen per night.\"", cost);
  act(buf, FALSE, recep, 0, ch, TO_VICT);
  if (cost > GET_NUYEN(ch))
  {
    act("$n tells you, \"...which I see you can't afford.\"", FALSE, recep, 0, ch, TO_VICT);
    return TRUE;
  }
  if (cost && (mode == RENT_FACTOR))
    Crash_rent_deadline(ch, recep, cost);

  save_room = find_hotel_room(ch->in_room->number);
  GET_LOADROOM(ch) = save_room;

  if (mode == RENT_FACTOR)
  {
    act("$n gives you a key and shows you to your room.", FALSE, recep, 0, ch, TO_VICT);
    snprintf(buf, sizeof(buf), "%s has rented at %ld", GET_CHAR_NAME(ch), ch->in_room->number);
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

void process_auth_room(struct char_data *ch) {
  PLR_FLAGS(ch).RemoveBit(PLR_NOT_YET_AUTHED);
  // Raw amount-- chargen money is meaningless.
  GET_NUYEN_RAW(ch) = 0;
  make_newbie(ch->carrying);
  for (int i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i))
      make_newbie(GET_EQ(ch, i));
  for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content)
    GET_OBJ_COST(obj) = 1;
  for (struct obj_data *obj = ch->bioware; obj; obj = obj->next_content)
    GET_OBJ_COST(obj) = 1;
  char_from_room(ch);
  char_to_room(ch, &world[real_room(RM_NEWBIE_LOBBY)]);
  send_to_char(ch, "^YYou are now Authorized. Welcome to Awakened Worlds.^n\r\n");

  for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content)
    if (GET_OBJ_VAL(obj, 0) == CYB_MEMORY) {
      if (obj->contains) {
        while (obj->contains) {
          GET_OBJ_VAL(obj, 5) -= GET_OBJ_VAL(obj->contains, 2) + GET_OBJ_VAL(obj->contains, 8);
          if (GET_OBJ_VAL(obj->contains, 9))
            ch->char_specials.saved.skills[GET_OBJ_VAL(obj->contains, 0)][1] = 0;
          extract_obj(obj->contains);
        }
        send_to_char(ch, "A brief tingle runs through you as %s is wiped clean.\r\n", GET_OBJ_NAME(obj));
      }
    }

  if (real_object(OBJ_NEWBIE_RADIO)>-1)
  {
    struct obj_data *radio = read_object(OBJ_NEWBIE_RADIO, VIRTUAL);
    GET_OBJ_VAL(radio, 0) = 8;
    obj_to_char(radio, ch);
    send_to_char(ch, "You have been given a radio.^n\r\n");
  }
  // Heal them.
  GET_PHYSICAL(ch) = 1000;
  GET_MENTAL(ch) = 1000;
  snprintf(buf, sizeof(buf), "DELETE FROM pfiles_chargendata WHERE idnum=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);

  playerDB.SaveChar(ch);

  // Make them look.
  // if (!PRF_FLAGGED(ch, PRF_SCREENREADER))
  look_at_room(ch, 0, 0);
}

SPECIAL(chargen_points_check) {
  // Final check to make sure you're not wasting resources.
  if (ch) {
    if (GET_FORCE_POINTS(ch) > 0) {
      send_to_char(ch, "^YWarning:^n You still have %d force point%s remaining to spend! They will be lost on graduation. You should go back and spend them on spells (HELP LEARN) and/or bonding foci (HELP BOND).\r\n", GET_FORCE_POINTS(ch), GET_FORCE_POINTS(ch) > 1 ? "s" : "");
    }
    if (GET_RESTRING_POINTS(ch) > 0) {
      send_to_char(ch, "^YWarning:^n You still have %d restring point%s remaining to spend! They will be lost on graduation. See HELP RESTRING for details.\r\n", GET_FORCE_POINTS(ch), GET_RESTRING_POINTS(ch) > 1 ? "s" : "");
    }
  }

  return FALSE;
}

SPECIAL(auth_room)
{
  if (ch && (CMD_IS("say") || CMD_IS("'") || CMD_IS("sayto") || CMD_IS("\"")) && !IS_ASTRAL(ch)) {
    /*
    skip_spaces(&argument);
    if (   !str_cmp("I have read the rules and policies, understand them, and agree to abide by them during my stay here.", argument)
        || !str_cmp("\"I have read the rules and policies, understand them, and agree to abide by them during my stay here.\"", argument) // Complete copy-paste with both quotes
        || !str_cmp("I have read the rules and policies, understand them, and agree to abide by them during my stay here.\"", argument) // Partial copy-paste with trailing quote.
        || !str_cmp("I have read the rules and policies, understand them, and agree to abideby them during my stay here.", argument) // Replaced linewrap carriage return with nothing.
        || !str_cmp("\"I have read the rules and policies, understand them, and agree to abideby them during my stay here.\"", argument) // Complete copy-paste with both quotes and linewrap space.
        || !str_cmp("I have read the rules and policies, understand them, and agree to abideby them during my stay here.\"", argument)) // Partial copy-paste with trailing quote and linewrap space.
    {
      process_auth_room(ch);
    }
    */

    // Honestly, it's not like this is some kind of legally binding agreement. Hand-wave them through to avoid losing players over this thing.
    process_auth_room(ch);
  }
  else if (CMD_IS("inventory")) {
    skip_spaces(&argument);

    // Close enough.
    if (!str_cmp("have read the rules and policies, understand them, and agree to abide by them during my stay here.", argument))
      process_auth_room(ch);
    else {
      send_to_char(ch, "You should use the SAY command here, and be careful about line breaks. Example: ^Wsay I have read the rules and policies, understand them, and agree to abide by them during my stay here.^n\r\n^n");
      return TRUE;
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
      if (success_test(GET_BOD(vict), 8) <= 0 && number(1, 3) == 3) {
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
    char_to_room(ch, &world[toroom]);
    act("$n walks into the club, ready to sell his wares.", FALSE, ch, 0, 0, TO_ROOM);
  }
  return FALSE;
}

SPECIAL(desktop)
{
  struct obj_data *obj = (struct obj_data *) me;
  if (!CMD_IS("list") || (!obj->in_veh && !obj->in_room))
    return FALSE;
  send_to_char(ch, "%s (%d/%d)", obj->text.name, GET_OBJ_VAL(obj, 2) - GET_OBJ_VAL(obj, 3), GET_OBJ_VAL(obj, 2));
  if (obj->contains) {
    send_to_char(ch, " contains:\r\n");
    for (struct obj_data *soft = obj->contains; soft; soft = soft->next_content) {
      if (GET_OBJ_TYPE(soft) == ITEM_DESIGN)
        send_to_char(ch, "%-40s %dMp (%dMp taken) (Design) %2.2f%% Complete\r\n", soft->restring, GET_OBJ_VAL(soft, 6),
                     GET_OBJ_VAL(soft, 6) + (GET_OBJ_VAL(soft, 6) / 10),
                     GET_OBJ_TIMER(soft) ? (GET_OBJ_VAL(soft, 5) ?
                                            ((float)(GET_OBJ_TIMER(soft) - GET_OBJ_VAL(soft, 5)) / (GET_OBJ_TIMER(soft) != 0 ? GET_OBJ_TIMER(soft) : 1)) * 100 :
                                            ((float)(GET_OBJ_TIMER(soft) - GET_OBJ_VAL(soft, 4)) / (GET_OBJ_TIMER(soft) != 0 ? GET_OBJ_TIMER(soft) : 1)) * 100) : 0);
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
        send_to_room("You bounce lightly along the floor.\r\n", room);
        break;
      case 10:
        send_to_room("You bounce good and high, getting a lot of air.\r\n", room);
        break;
      case 15:
        send_to_room("You bounce into one of the walls and then fall on your ass, only to bounce back onto your feet.\r\n", room);
        break;
      case 20:
        send_to_room("You bounce really hard and smack your head off the ceiling.\r\n", room);
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
      lose_nuyen(ch, amount, NUYEN_OUTFLOW_GENERIC_SPEC_PROC);
      return TRUE;
    }
  }
  return FALSE;
}

SPECIAL(johnson);

SPECIAL(quest_debug_scanner)
{
  struct obj_data *obj = (struct obj_data *) me;
  struct char_data *to = NULL;

  // Check to make sure I'm being held by my user.
  if (!ch || obj->worn_by != ch)
    return FALSE;

  // Reject mortals, violently.
  if (GET_LEVEL(ch) <= 1) {
    send_to_char(ch, "%s arcs violently in your hands!\r\n");
    damage(ch, ch, 100, TYPE_TASER, TRUE);
    return TRUE;
  }

  // Diagnostic command.
  if (CMD_IS("diagnose")) {
    skip_spaces(&argument);
    if (!*argument) {
      send_to_char(ch, "Quest-debug who?\r\n");
      return TRUE;
    }

    if (ch->in_veh)
      to = get_char_veh(ch, argument, ch->in_veh);
    else
      to = get_char_room_vis(ch, argument);

    if (!to) {
      send_to_char(ch, "You don't see any '%s' that you can quest-debug here.\r\n", argument);
      return TRUE;
    }

    if (IS_NPC(to)) {
      if (!(mob_index[GET_MOB_RNUM(to)].func == johnson || mob_index[GET_MOB_RNUM(to)].sfunc == johnson)) {
        send_to_char(ch, "That NPC doesn't have any quest-related information available.\r\n");
        return TRUE;
      }

      snprintf(buf, sizeof(buf), "NPC %s's quest-related information:\r\n", GET_CHAR_NAME(to));
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Overall max rep: %d, overall min rep: %d\r\n",
              get_johnson_overall_max_rep(to), get_johnson_overall_min_rep(to));
     //Remember to add here a list of all quests assigned to this johnson -- Nodens
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "SPARE1: %ld\r\n", GET_SPARE1(to));
      strcat(buf, "NPC's mob memory records hold the following character IDs: \r\n");
      for (memory_rec *tmp = GET_MOB_MEMORY(to); tmp; tmp = tmp->next)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %ld\r\n", tmp->id);
      send_to_char(buf, ch);
      return TRUE;
    }

    snprintf(buf, sizeof(buf), "Player %s's quest-related information:\r\n", GET_CHAR_NAME(to));
    int real_mob = real_mobile(quest_table[GET_QUEST(to)].johnson);
    if (GET_QUEST(to)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Current quest: %ld (given by %s [%ld])\r\n",
              quest_table[GET_QUEST(to)].vnum,
              real_mob >= 0 ? mob_proto[real_mob].player.physical_text.name : "N/A",
              quest_table[GET_QUEST(to)].johnson);
    } else {
      strcat(buf, "Not currently on a quest.\r\n");
    }

    send_to_char(ch, "Last %d quests:\r\n", QUEST_TIMER);
    for (int i = 0; i < QUEST_TIMER; i++) {
      if (GET_LQUEST(to, i))
        send_to_char(ch, "%d) %ld\r\n", i, GET_LQUEST(to, i));
    }

    send_to_char(buf, ch);
    return TRUE;
  }

  // WHERE debugger.
  if (CMD_IS("where")) {
    send_to_char("^RUsing extended WHERE due to you holding a diagnostic scanner.^n\r\n", ch);

    struct room_data *room;
    for (int i = 0; i <= top_of_world; i++) {
      room = &world[i];
      for (struct char_data *tch = room->people; tch; tch = tch->next_in_room) {
        if (IS_NPC(tch))
          continue;

        send_to_char(ch, "%-20s - [%6ld] %s^n\r\n", GET_CHAR_NAME(tch), GET_ROOM_VNUM(room), GET_ROOM_NAME(room));
      }
    }

    for (struct veh_data *veh = veh_list; veh; veh = veh->next) {
      room = get_veh_in_room(veh);
      for (struct char_data *tch = veh->people; tch; tch = tch->next_in_veh) {
        if (IS_NPC(tch))
          continue;

        send_to_char(ch, "%-20s - in %s at [%6ld] %s^n\r\n", GET_CHAR_NAME(tch), GET_VEH_NAME(veh), room ? GET_ROOM_VNUM(room) : -1, room ? GET_ROOM_NAME(room) : "nowhere");
      }
    }

    return TRUE;
  }
  // This is not needed currently. But keeping it in so that I remember to repurpose it -- Nodens
  /*
  if (CMD_IS("reload")) {
    skip_spaces(&argument);
    if (!*argument) {
      send_to_char(ch, "Reload quest information on which NPC?\r\n");
      return TRUE;
    }

    if (ch->in_veh)
      to = get_char_veh(ch, argument, ch->in_veh);
    else
      to = get_char_room_vis(ch, argument);

    if (!to) {
      send_to_char(ch, "You don't see any '%s' that you can quest-debug here.\r\n", argument);
      return TRUE;
    }

    if (IS_NPC(to)) {
      if (!(mob_index[GET_MOB_RNUM(to)].func == johnson || mob_index[GET_MOB_RNUM(to)].sfunc == johnson)) {
        send_to_char(ch, "That NPC doesn't have any quest-related information available.\r\n");
        return TRUE;
      }

      act("You roughly slap $N and demand $E pick a new random quest to offer.", FALSE, ch, 0, to, TO_CHAR);
      act("$n roughly slaps $N and demands that $E pick a new random quest to offer.", FALSE, ch, 0, to, TO_ROOM);
      //new_quest(to);
      GET_SPARE1(to) = -1;
      //send_to_char(ch, "Now offering quest %ld.", GET_SPARE2(to) ? quest_table[GET_SPARE2(to)].vnum : -1);

      return TRUE;
    }

    send_to_char(ch, "You can only do that on NPCs, and %s doesn't qualify.\r\n", GET_CHAR_NAME(to));
    return TRUE;
  }
*/
  return FALSE;
}

SPECIAL(portable_gridguide)
{
  struct obj_data *obj = (struct obj_data *) me;

  // Check to make sure I'm being carried or worn by my user.
  if (!ch || (obj->carried_by != ch && obj->worn_by != ch))
    return FALSE;

  if (CMD_IS("gridguide") && !ch->in_veh) {
    if (!(ROOM_FLAGGED(ch->in_room, ROOM_ROAD) || ROOM_FLAGGED(ch->in_room, ROOM_GARAGE)) ||
        ROOM_FLAGGED(ch->in_room, ROOM_NOGRID))
      send_to_char(ch, "%s flashes up with ^RINVALID LOCATION^n.\r\n", CAP(GET_OBJ_NAME(obj)));
    else {
      int x = ch->in_room->number - (ch->in_room->number * 3), y = ch->in_room->number + 100;
      send_to_char(ch, "%s flashes up with ^R%d, %d^n.\r\n", CAP(GET_OBJ_NAME(obj)), x, y);
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
  if ((veh = world[real_room(painter->in_room->number + 1)].vehicles))
    if (veh->spare <= time(0)) {
      veh_from_room(veh);
      veh_to_room(veh, &world[real_room(painter->in_room->number)]);
      veh->spare = 0;
      if (world[real_room(painter->in_room->number)].people) {
        snprintf(buf, sizeof(buf), "%s is wheeled out into the parking lot.", GET_VEH_NAME(veh));
        act(buf, FALSE, world[real_room(painter->in_room->number)].people, 0, 0, TO_ROOM);
        act(buf, FALSE, world[real_room(painter->in_room->number)].people, 0, 0, TO_CHAR);
      }
      save_vehicles();
    }
  if (!CMD_IS("paint"))
    return FALSE;
  if (IS_NPC(ch)) {
    send_to_char("You're having a hard time getting the painter's attention.\r\n", ch);
    return FALSE;
  }
  skip_spaces(&argument);
  if (!*argument) {
    snprintf(buf, sizeof(buf), "I'll paint any vehicle for %d nuyen upfront.", PAINTER_COST);
    do_say(painter, buf, 0, 0);
    return TRUE;
  }
  if (world[real_room(painter->in_room->number + 1)].vehicles) {
    do_say(painter, "We're already working on someone's ride, bring it back later.", 0, 0);
    return TRUE;
  }
  if (!(veh = get_veh_list(argument, ch->in_room->vehicles, ch)))
    do_say(painter, "What do you want me to paint?", 0, 0);
  else if (veh->owner != GET_IDNUM(ch))
    do_say(painter, "Bring me your own ride and I'll paint it.", 0, 0);
  else if (GET_NUYEN(ch) < PAINTER_COST)
    do_say(painter, "You don't have the nuyen for that.", 0, 0);
  else {
    veh->spare = time(0) + (SECS_PER_MUD_DAY * 3);
    ch->desc->edit_veh = veh;
    veh_from_room(veh);
    veh_to_room(veh, &world[real_room(painter->in_room->number + 1)]);
    lose_nuyen(ch, PAINTER_COST, NUYEN_OUTFLOW_GENERIC_SPEC_PROC);
    snprintf(buf, sizeof(buf), "%s is wheeled into the painting booth.", GET_VEH_NAME(veh));
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

SPECIAL(multnomah_gate) {
  NO_DRAG_BULLSHIT;

  if (!cmd)
    return FALSE;
  long in_room = real_room(get_ch_in_room(ch)->number), to_room = 0;
  if (world[in_room].number == RM_MULTNOMAH_GATE_NORTH)
    to_room = real_room(RM_MULTNOMAH_GATE_SOUTH);
  else to_room = real_room(RM_MULTNOMAH_GATE_NORTH);

  if ((world[in_room].number == RM_MULTNOMAH_GATE_NORTH && CMD_IS("south")) || (world[in_room].number == RM_MULTNOMAH_GATE_SOUTH && CMD_IS("north"))) {
    if (!IS_NPC(ch) && !PLR_FLAGGED(ch, PLR_VISA)) {
      if (access_level(ch, LVL_BUILDER)) {
        send_to_char("You don't even bother making eye contact with the guard as you head towards the gate. Rank hath its privileges.\r\n", ch);
        act("$n doesn't even bother making eye contact with the guard as $e heads towards the gate. Rank hath its privileges.\r\n", TRUE, ch, 0, 0, TO_ROOM);
      } else {
        send_to_char("The gate refuses to open for you. Try showing the guard your visa.\r\n", ch);
        return TRUE;
      }
    }

    if (ch->in_veh) {
      for (struct char_data *vict = ch->in_veh->people; vict; vict = vict->next_in_veh)
        if (vict != ch && !IS_NPC(vict) && !PLR_FLAGGED(vict, PLR_VISA) && !access_level(ch, LVL_BUILDER)) {
          send_to_char("The guards won't open the gate until everyone has shown their visas.\r\n", ch);
          return TRUE;
        }
    }
    send_to_char("The gate slides open just enough for you to pass through.\r\n", ch);
    if (ch->in_veh) {
      snprintf(buf, sizeof(buf), "The gate slides open allowing %s to pass through.\r\n", GET_VEH_NAME(ch->in_veh));
      send_to_room(buf, ch->in_veh->in_room);
      for (struct char_data *vict = ch->in_veh->people; vict; vict = vict->next_in_veh)
        if (!IS_NPC(vict))
          PLR_FLAGS(vict).RemoveBit(PLR_VISA);
      veh_from_room(ch->in_veh);
      veh_to_room(ch->in_veh, &world[to_room]);
    }
    else {
      act("The gate opens allowing $n to pass through.\r\n", FALSE, ch, 0, 0, TO_ROOM);
      if (!IS_NPC(ch))
        PLR_FLAGS(ch).RemoveBit(PLR_VISA);
      char_from_room(ch);
      char_to_room(ch, &world[to_room]);
    }
    if (!PRF_FLAGGED(ch, PRF_SCREENREADER))
      look_at_room(ch, 0, 0);
    return TRUE;
  }

  return FALSE;
}

SPECIAL(multnomah_guard)
{
  if (cmd && CMD_IS("show")) {
    strlcpy(buf, argument, sizeof(buf));
    char *no_seriously_fuck_pointers = buf;
    skip_spaces(&no_seriously_fuck_pointers);
    any_one_arg(buf, buf2);
    struct char_data *guard = (struct char_data *) me;
    struct obj_data *visa = get_obj_in_list_vis(ch, buf2, ch->carrying);
    if (visa && GET_OBJ_VNUM(visa) == OBJ_MULTNOMAH_VISA) {
      if (GET_OBJ_VAL(visa, 0) == GET_IDNUM(ch)) {
        PLR_FLAGS(ch).SetBit(PLR_VISA);
        snprintf(arg, sizeof(arg), "%s Everything seems to be in order, proceed.", GET_CHAR_NAME(ch));
        do_say(guard, arg, 0, SCMD_SAYTO);
      } else {
        snprintf(arg, sizeof(arg), "%s That's not your visa. Get out of here before we arrest you.", GET_CHAR_NAME(ch));
        // extract_obj(visa);  -- why do this? Punishing people for having the wrong visa is no bueno.
        do_say(guard, arg, 0, SCMD_SAYTO);
      }
      return TRUE;
    }
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

SPECIAL(floor_has_glass_shards) {
  if (!cmd)
    return FALSE;

  // If they're safe from shards, we don't care what they do.
  if (ch->in_veh || IS_NPC(ch) || IS_ASTRAL(ch) || PRF_FLAGGED(ch, PRF_NOHASSLE) || GET_EQ(ch, WEAR_FEET) || AFF_FLAGGED(ch, AFF_SNEAK))
    return FALSE;

  // If they attempt to leave the room and are not in a vehicle, wearing shoes, or sneaking, they get cut up.
  for (int dir_index = NORTH; dir_index <= DOWN; dir_index++) {
    if (CMD_IS(exitdirs[dir_index]) || CMD_IS(fulldirs[dir_index])) {
      send_to_char("^rAs you walk away, the glass shards tear at your bare feet!^n\r\n\r\n", ch);
      act("The glass shards tear at $n's bare feet as $e leaves!", TRUE, ch, NULL, NULL, TO_ROOM);
      damage(ch, ch, LIGHT, 0, TRUE);
      break;
    }
  }

  // Always return false-- we don't stop them from doing anything, we just hurt them for walking uncautiously.
  return FALSE;
}

SPECIAL(bouncer_gentle)
{
  ACMD_CONST(do_look);

  if (!cmd)
    return FALSE;

  struct char_data *bouncer = (struct char_data *) me;
  if (IS_NPC(ch) || (!PLR_FLAGGED(ch, PLR_KILLER) && !PLR_FLAGGED(ch, PLR_BLACKLIST)))
    return FALSE;
  int toroom = -1, dir_index;

  // You must hardcode in the bouncer's room num and where he'll send you.
  switch (bouncer->in_room->number) {
    case 70301:
      toroom = 70303;
      dir_index = NORTH;
      break;
  }

  // No match means we fail out.
  if (toroom == -1)
    return FALSE;

  if (CMD_IS(exitdirs[dir_index]) || CMD_IS(fulldirs[dir_index])) {
    act("$n shakes $s head at you and escorts you from the premises.", FALSE, bouncer, 0, ch, TO_VICT);
    act("$n shakes $s head at $N and escorts $M from the premises.", FALSE, bouncer, 0, ch, TO_NOTVICT);
    char_from_room(ch);
    char_to_room(ch, &world[real_room(toroom)]);
    act("$n is escorted in by $N, who gives $m a stern look and departs.", FALSE, ch, 0, bouncer, TO_ROOM);
    do_look(ch, "", 0, 0);
    return TRUE;
  }

  return FALSE;
}

SPECIAL(bouncer_troll) {
  NO_DRAG_BULLSHIT;

  if (!cmd)
    return FALSE;

  struct char_data *troll = (struct char_data *) me;
  if (IS_NPC(ch) || !(PLR_FLAGGED(ch, PLR_KILLER) || PLR_FLAGGED(ch, PLR_BLACKLIST)))
    return FALSE;
  int toroom = -1;
  char *dir = NULL, *dir2 = str_dup("nothing");
  switch (troll->in_room->number) {
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
    case 70002:
      toroom = 70000;
      dir = str_dup("north");
      break;
    case 70400:
      toroom = 2062;
      dir = str_dup("north");
      dir2 = str_dup("up");
      break;
    case 70501:
      toroom = 30515;
      dir = str_dup("north");
      break;
    case 15801:
      toroom = 32703;
      dir = str_dup("north");
      dir2 = str_dup("west");
      break;
    case 71001:
      toroom = 71001;
      dir = str_dup("south");
      break;
  }
  if (toroom == -1)
    return FALSE;
  if (CMD_IS(dir) || CMD_IS(dir2)) {
    act("Without warning, $n picks $N up and tosses $M into the street.", FALSE, troll, 0, ch, TO_NOTVICT);
    act("Without warning, $n picks you up and tosses you into the street.", FALSE, troll, 0, ch, TO_VICT);
    char_from_room(ch);
    char_to_room(ch, &world[real_room(toroom)]);
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
  struct obj_data *locker = ch->in_room->contents, *next = NULL;
  int num = 0, free = 0;
  for (; locker; locker = locker->next_content)
    if (GET_OBJ_VNUM(locker) == 9826) {
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
      FOR_ITEMS_AROUND_CH(ch, locker)
        if (GET_OBJ_VNUM(locker) == 9826) {
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
      FOR_ITEMS_AROUND_CH(ch, locker)
        if (GET_OBJ_VNUM(locker) == 9826 && ++num && GET_OBJ_VAL(locker, 9) == free) {
          int cost = (int)((((time(0) - GET_OBJ_VAL(locker, 8)) / SECS_PER_REAL_DAY) + 1) * 50);
          if (GET_NUYEN(ch) < cost)
            send_to_char(ch, "The system beeps loudly and the screen reads 'PLEASE INSERT %d NUYEN'.\r\n", cost);
          else {
            lose_nuyen(ch, cost, NUYEN_OUTFLOW_GENERIC_SPEC_PROC);
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
    FOR_ITEMS_AROUND_CH(ch, locker)
      if (GET_OBJ_VNUM(locker) == 9826 && !--num && !IS_SET(GET_OBJ_VAL(locker, 1), CONT_CLOSED)) {
        snprintf(buf, sizeof(buf), "%d%d%d%d%d%d%d", number(1, 9), number(1, 9), number(1, 9), number(1, 9), number(1, 9), number(1, 9), number(1, 9));
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

struct obj_data *find_neophyte_housing_card(struct obj_data *obj) {
  struct obj_data *temp, *result;
  if (!obj)
    return NULL;

  if (GET_OBJ_VNUM(obj) == OBJ_NEOPHYTE_SUBSIDY_CARD)
    return obj;

  if (obj->contains) {
    for (temp = obj->contains; temp; temp = temp->next_content)
      if ((result = find_neophyte_housing_card(temp)))
        return result;
  }

  return NULL;
}
SPECIAL(newbie_housing)
{
  if (!ch || !cmd || !CMD_IS("recharge"))
    return FALSE;
  if (GET_NUYEN(ch) < 10000)
    send_to_char("You don't have enough nuyen to recharge a housing card.\r\n", ch);
  else {
    struct obj_data *card = NULL, *result = NULL;

    // Search their carried inventory for the housing card.
    for (card = ch->carrying; card; card = card->next_content)
      if ((result = find_neophyte_housing_card(card))) {
        card = result;
        break;
      }

    // Search their worn inventory for the housing card.
    if (!card) {
      for (int i = 0; i < NUM_WEARS; i++)
        if ((result = find_neophyte_housing_card(GET_EQ(ch, i)))) {
          card = result;
          break;
        }
    }

    // Couldn't find it anywhere? Give them a new one.
    if (!card) {
      card = read_object(OBJ_NEOPHYTE_SUBSIDY_CARD, VIRTUAL);
      GET_OBJ_VAL(card, 0) = GET_IDNUM(ch);
      obj_to_char(card, ch);
    }
    GET_OBJ_VAL(card, 1) += 10000;
    // We should probably track this one as going out into the game world, but eh.
    GET_NUYEN_RAW(ch) -= 10000;

    if (card->carried_by == ch)
      send_to_char("You place your card into the machine and add an extra 10,000 nuyen.", ch);
    else
      send_to_char("You dig your card out and put it in the machine, adding 10,000 extra nuyen.", ch);

    send_to_char(ch, " The value is currently at %d nuyen.\r\n", GET_OBJ_VAL(card, 1));
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

void untrain_attribute(struct char_data *ch, int attr, const char *success_message) {
  // Check for racial maximums.
  if (GET_REAL_ATT(ch, attr) <= MAX(1, 1 + racial_attribute_modifiers[(int)GET_RACE(ch)][attr])) {
    send_to_char(ch, "Your %s attribute is at its minimum.\r\n", attributes[attr]);
    return;
  }

  // Success; refund the attribute point and knock down the attribute.
  GET_ATT_POINTS(ch)++;
  GET_REAL_ATT(ch, attr) -= 1;

  // Update character's calculated values.
  affect_total(ch);

  // Notify the character of success.
  send_to_char(ch, success_message, GET_REAL_ATT(ch, attr));
}

SPECIAL(chargen_untrain_attribute)
{
  if (!ch || !cmd || IS_NPC(ch) || !CMD_IS("untrain"))
    return FALSE;

  skip_spaces(&argument);

  if (!*argument) {
    send_to_char("You must specify an attribute to untrain.\r\n", ch);
    return TRUE;
  }

  if (is_abbrev(argument, "body")) {
    untrain_attribute(ch, BOD, "You determinedly chow down on junk food for a week and decrease your Body to %d.\r\n");
    return TRUE;
  }

  if (is_abbrev(argument, "quickness")) {
    untrain_attribute(ch, QUI, "You quit drinking so much caffiene and decrease your Quickness to %d.\r\n");
    return TRUE;
  }

  if (is_abbrev(argument, "strength")) {
    untrain_attribute(ch, STR, "You do your best couch potato impression until your Strength decreases to %d.\r\n");
    return TRUE;
  }

  if (is_abbrev(argument, "charisma")) {
    untrain_attribute(ch, CHA, "You scowl hard enough to put frown lines in your face, decreasing your Charisma to %d.\r\n");
    return TRUE;
  }

  if (is_abbrev(argument, "intelligence")) {
    untrain_attribute(ch, INT, "You slam your head into a wall until your Intelligence decreases to %d.\r\n");
    return TRUE;
  }

  if (is_abbrev(argument, "willpower")) {
    untrain_attribute(ch, WIL, "You declare every day to be cheat day, allowing your Willpower to slip to %d.\r\n");
    return TRUE;
  }

  send_to_char("Valid attributes for untraining are BOD, QUI, STR, CHA, INT, WIL.\r\n", ch);
  return TRUE;
}

// Prevent people from moving south from trainer until they've spent all their attribute points.
SPECIAL(chargen_south_from_trainer) {
  NO_DRAG_BULLSHIT;

  if (!ch || !cmd || IS_NPC(ch))
    return FALSE;

  if ((CMD_IS("s") || CMD_IS("south")) && GET_ATT_POINTS(ch) > 0) {
    send_to_char(ch, "You still have %d attribute point%s to spend! You must finish ^WTRAIN^ning your attributes before you proceed.\r\n",
                 GET_ATT_POINTS(ch), GET_ATT_POINTS(ch) > 1 ? "s" : "");
    return TRUE;
  }

  if (CMD_IS("untrain")) {
    return chargen_untrain_attribute(ch, NULL, cmd, argument);
  }

  return FALSE;
}

SPECIAL(chargen_unpractice_skill)
{
  if (!ch || !cmd || IS_NPC(ch))
    return FALSE;

  if (CMD_IS("unpractice")) {
    skip_spaces(&argument);

    if (!*argument) {
      send_to_char("Syntax: UNPRACTICE [skill name]\r\n", ch);
      return TRUE;
    }

    int skill_num = find_skill_num(argument);

    if (skill_num < 0) {
      send_to_char("Please specify a valid skill.\r\n", ch);
      return TRUE;
    }

    if (GET_SKILL(ch, skill_num) != REAL_SKILL(ch, skill_num)) {
      send_to_char("You can't unpractice a skill you currently have a skillsoft or other boost for.\r\n", ch);
      return TRUE;
    }

    if (GET_SKILL(ch, skill_num) <= 0) {
      send_to_char("You don't know that skill.\r\n", ch);
      return TRUE;
    }

    // Success. Lower the skill by one point.
    GET_SKILL_POINTS(ch)++;
    set_character_skill(ch, skill_num, REAL_SKILL(ch, skill_num) - 1, FALSE);

    if (GET_SKILL(ch, skill_num) == 0) {
      send_to_char(ch, "With the assistance of several blunt impacts, you completely forget %s.\r\n", skills[skill_num].name);
    } else {
      send_to_char(ch, "With the assistance of several blunt impacts, you decrease your skill in %s.\r\n", skills[skill_num].name);
    }

    return TRUE;
  }

  return FALSE;
}

SPECIAL(chargen_language_annex) {
  NO_DRAG_BULLSHIT;

  if (!ch || !cmd || IS_NPC(ch))
    return FALSE;

  if (CMD_IS("ne") || CMD_IS("northeast") || CMD_IS("sw") || CMD_IS("southwest")) {
    int language_skill_total = 0;
    for (int i = SKILL_ENGLISH; i < MAX_SKILLS && language_skill_total < STARTING_LANGUAGE_SKILL_LEVEL; i++)
      if (SKILL_IS_LANGUAGE(i) && GET_SKILL(ch, i) > 0)
        language_skill_total += GET_SKILL(ch, i);

    if (language_skill_total < STARTING_LANGUAGE_SKILL_LEVEL) {
      send_to_char(ch, "You need to have a minimum of %d skill points spent in languages, so you still need to spend %d point%s.\r\n",
                   STARTING_LANGUAGE_SKILL_LEVEL,
                   STARTING_LANGUAGE_SKILL_LEVEL - language_skill_total,
                   STARTING_LANGUAGE_SKILL_LEVEL - language_skill_total != 1 ? "s" : "");
      return TRUE;
    }
  }

  // Tie in with the chargen_unpractice_skill routine above.
  if (CMD_IS("unpractice")) {
    return chargen_unpractice_skill(ch, NULL, cmd, argument);
  }

  return FALSE;
}

void remap_chargen_archetype_paths_and_perform_command(struct room_data *room, struct char_data *ch, char *argument, int cmd) {
  // Store the current exit, then overwrite with our custom one.
  struct room_data *east_temp_to_room = room->dir_option[EAST]->to_room;
  if (GET_TRADITION(ch) == TRAD_HERMETIC)
    room->dir_option[EAST]->to_room = &world[real_room(RM_CHARGEN_PATH_OF_THE_MAGICIAN_HERMETIC)];
  else if (GET_TRADITION(ch) == TRAD_SHAMANIC)
    room->dir_option[EAST]->to_room = &world[real_room(RM_CHARGEN_PATH_OF_THE_MAGICIAN_SHAMANIC)];
  else
    room->dir_option[EAST]->to_room = NULL;

  // Same for the south exit.
  struct room_data *south_temp_to_room = room->dir_option[SOUTH]->to_room;
  if (GET_TRADITION(ch) != TRAD_ADEPT)
    room->dir_option[SOUTH]->to_room = NULL;

  // Execute the actual command as normal. We know it'll always be cmd_info since you can't rig or mtx in chargen.
  ((*cmd_info[cmd].command_pointer) (ch, argument, cmd, cmd_info[cmd].subcmd));

  // Restore the east exit for the room to the normal one.
  room->dir_option[EAST]->to_room = east_temp_to_room;

  // Same for the south exit.
  room->dir_option[SOUTH]->to_room = south_temp_to_room;
}

// Prevent people from moving south from teachers until they've spent all their skill points.
SPECIAL(chargen_skill_annex) {
  NO_DRAG_BULLSHIT;

  if (!ch || !cmd || IS_NPC(ch))
    return FALSE;

  if (CMD_IS("nw") || CMD_IS("northwest")) {
    if (GET_TRADITION(ch) == TRAD_MUNDANE) {
      send_to_char("You don't have the ability to learn magic.\r\n", ch);
      return TRUE;
    }
    return FALSE;
  }

  if ((CMD_IS("s") || CMD_IS("south"))) {
    if (GET_SKILL_POINTS(ch) > 0) {
      send_to_char(ch, "You still have %d skill point%s to spend! You should finish ^WPRACTICE^n-ing your skills before you proceed.\r\n",
                   GET_SKILL_POINTS(ch), GET_SKILL_POINTS(ch) > 1 ? "s" : "");
      return TRUE;
    }
  }

  if (CMD_IS("practice")) {
    send_to_char("You can't do that here. Try visiting one of the teachers in the surrounding areas."
                 " You can always view the skills you've learned so far by typing ^WSKILLS^n.\r\n", ch);
    return TRUE;
  }

  if (CMD_IS("train") || CMD_IS("untrain")) {
    send_to_char("You can't do that here. You can visit Neil to the north to train your abilities.\r\n", ch);
    return TRUE;
  }

  // Tie in with the chargen_unpractice_skill routine above.
  if (CMD_IS("unpractice")) {
    return chargen_unpractice_skill(ch, NULL, cmd, argument);
  }

  return FALSE;
}

SPECIAL(south_of_chargen_skill_annex) {
  NO_DRAG_BULLSHIT;

  if (!ch || !cmd || IS_NPC(ch))
    return FALSE;

  if ((CMD_IS("s") || CMD_IS("south"))) {
    remap_chargen_archetype_paths_and_perform_command(((struct room_data *) me)->dir_option[SOUTH]->to_room, ch, argument, cmd);
    return TRUE;
  }

  return FALSE;
}

SPECIAL(chargen_hopper)
{
  struct obj_data *hopper = (struct obj_data *) me;
  struct obj_data *modulator = hopper->contains;
  static char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  char *arg1_ptr = arg1, *arg2_ptr = arg2;

  if (!ch || !cmd || IS_NPC(ch))
    return FALSE;

  // Regardless of how or why we were called, this is a great chance to ensure the hopper has a mod in it.
  if (!modulator) {
    rnum_t modulator_rnum = real_object(OBJ_DOCWAGON_BASIC_MOD);

    // Cutout: If the modulator doesn't exist, fail.
    if (modulator_rnum == NOWHERE) {
      snprintf(buf, sizeof(buf), "SYSERR: Attempting to reference docwagon modulator at item %d, but that item does not exist.", OBJ_DOCWAGON_BASIC_MOD);
      mudlog(buf, NULL, LOG_SYSLOG, TRUE);
      return FALSE;
    }

    modulator = read_object(modulator_rnum, REAL);
    make_newbie(modulator);
    obj_to_obj(modulator, hopper);
    //mudlog("DEBUG: Loaded hopper with modulator.", NULL, LOG_SYSLOG, TRUE);
  }

  if (CMD_IS("get")) {
    // Clear our static argbufs.
    memset(arg1, 0, sizeof(arg1));
    memset(arg2, 0, sizeof(arg2));

    // Extract arguments from provided argument.
    two_arguments(argument, arg1, arg2);
    if (!*arg1 || !*arg2)
      return FALSE;

    // Strip out the numbers for fewer shenanigans.
    get_number(&arg1_ptr);
    get_number(&arg2_ptr);

    // If the keyword they're using is valid for the hopper:
    if ((isname(arg2, hopper->text.keywords) || isname(arg2, hopper->text.name) || strcmp(arg2, "all") == 0)
        && (isname(arg1, modulator->text.keywords) || isname(arg1, modulator->text.name) || strcmp(arg1, "all") == 0)) {
      struct obj_data *inv = NULL;
      bool ch_already_has_one = FALSE;

      // Check their inventory to see if they have one already.
      for (inv = ch->carrying; inv && !ch_already_has_one; inv = inv->next_content) {
        if (GET_OBJ_VNUM(inv) == OBJ_DOCWAGON_BASIC_MOD)
          ch_already_has_one = TRUE;

        // Recursively search into the object, just in case it's a container.
        if (find_matching_obj_in_container(inv, OBJ_DOCWAGON_BASIC_MOD))
          ch_already_has_one = TRUE;
      }

      // Check their equipment to see if they have one already.
      for (int i = 0; i < NUM_WEARS && !ch_already_has_one; i++) {
        if ((inv = GET_EQ(ch, i)) && GET_OBJ_VNUM(inv) == OBJ_DOCWAGON_BASIC_MOD)
          ch_already_has_one = TRUE;

        // Recursively search into the object, just in case it's a container.
        if (find_matching_obj_in_container(inv, OBJ_DOCWAGON_BASIC_MOD))
          ch_already_has_one = TRUE;
      }

      if (ch_already_has_one) {
        send_to_char("A shutter on the hopper closes as you reach for it, and a sign flashes, \"One per person.\"\r\n", ch);
      } else {
        send_to_char(ch, "You reach into the hopper and retrieve %s.\r\n", GET_OBJ_NAME(modulator));
        obj_from_obj(modulator);
        obj_to_char(modulator, ch);
        GET_OBJ_COST(modulator) = 0;
      }

      return TRUE;
    }
  }

  return FALSE;
}

// Build the exits of the room based on character's traditions.
SPECIAL(chargen_career_archetype_paths) {
  NO_DRAG_BULLSHIT;

  struct room_data *room = (struct room_data *) me;

  if (!ch || !cmd || IS_NPC(ch))
    return FALSE;

  // Block non-mages from going east.
  if ((CMD_IS("east") || CMD_IS("e")) && (GET_TRADITION(ch) != TRAD_HERMETIC && GET_TRADITION(ch) != TRAD_SHAMANIC)) {
    send_to_char("You don't have the aptitude to choose the Path of the Magician.\r\n", ch);
    return TRUE;
  }

  // Block non-adepts from going south.
  if ((CMD_IS("s") || CMD_IS("south")) && GET_TRADITION(ch) != TRAD_ADEPT) {
    send_to_char("You don't have the aptitude to choose the Path of the Adept.\r\n", ch);
    return TRUE;
  }

  remap_chargen_archetype_paths_and_perform_command(room, ch, argument, cmd);
  return TRUE;
}

// Build the exits of the room based on character's traditions.
SPECIAL(chargen_spirit_combat_west)
{
  struct room_data *room = (struct room_data *) me;
  struct room_data *temp_to_room = NULL;

  if (!ch || !cmd || IS_NPC(ch))
    return FALSE;

  // Map the west exit to the correct branch of magic's path, then proceed.

  // Store the current exit, then overwrite with our custom one.
  temp_to_room = room->dir_option[WEST]->to_room;
  if (GET_TRADITION(ch) == TRAD_HERMETIC)
    room->dir_option[WEST]->to_room = &world[real_room(RM_CHARGEN_CONJURING_HERMETIC)];
  else
    room->dir_option[WEST]->to_room = &world[real_room(RM_CHARGEN_CONJURING_SHAMANIC)];

  // Execute the actual command as normal. We know it'll always be cmd_info since you can't rig or mtx in chargen.
  ((*cmd_info[cmd].command_pointer) (ch, argument, cmd, cmd_info[cmd].subcmd));

  // Restore the east exit for the room to the normal one.
  room->dir_option[WEST]->to_room = temp_to_room;

  return TRUE;
}

void dominator_mode_switch(struct char_data *ch, struct obj_data *obj, int mode) {
  // Formatting.
  send_to_char(ch, "\r\n");

  switch (mode) {
    case DOMINATOR_MODE_PARALYZER:
      // This is a bit of a hack, but we know what the previous mode was based on its type and attack type.
      if (GET_OBJ_TYPE(obj) == ITEM_OTHER) {
        // It was deactivated. Light it up.
        snprintf(buf, sizeof(buf), "The red lights on %s begin to glow with cerulean blue power.", GET_OBJ_NAME(obj));
        act(buf, FALSE, ch, 0, ch, TO_CHAR);
        act(buf, FALSE, ch, 0, 0, TO_ROOM);
      } else if (GET_WEAPON_ATTACK_TYPE(obj) == WEAP_HEAVY_PISTOL) {
        // It was in Lethal mode. Collapse it.
        snprintf(buf, sizeof(buf), "%s retracts and folds down into its standard heavy-pistol format, its status lights glowing blue.", GET_OBJ_NAME(obj));
        act(buf, FALSE, ch, 0, ch, TO_CHAR);
        act(buf, FALSE, ch, 0, 0, TO_ROOM);
      } else if (GET_WEAPON_ATTACK_TYPE(obj) == WEAP_CANNON) {
        // It was in Destroyer mode. Collapse it.
        snprintf(buf, sizeof(buf), "%s vents energy with a sharp snap of power, collapsing with uncanny speed into its standard heavy-pistol format.", GET_OBJ_NAME(obj));
        act(buf, FALSE, ch, 0, ch, TO_CHAR);
        act(buf, FALSE, ch, 0, 0, TO_ROOM);
      }

      send_to_char(ch, "\r\nA dispassionate female voice states, \"^cEnforcement mode: Non-Lethal Paralyzer. Please aim calmly and subdue the target.^n\"\r\n");
      GET_OBJ_TYPE(obj) = ITEM_WEAPON;
      GET_WEAPON_ATTACK_TYPE(obj) = WEAP_TASER;
      GET_WEAPON_DAMAGE_CODE(obj) = DEADLY;
      GET_WEAPON_POWER(obj) = 50;
      break;
    case DOMINATOR_MODE_ELIMINATOR:
      if (GET_OBJ_TYPE(obj) == ITEM_OTHER || GET_WEAPON_ATTACK_TYPE(obj) == WEAP_TASER) {
        // It was in a lesser power state. Light it up.
        snprintf(buf, sizeof(buf), "%s splits and morphs, cerulean energy pulsing through its rotating circuitry as it deploys into a jaw-like configuration.", GET_OBJ_NAME(obj));
        act(buf, FALSE, ch, 0, ch, TO_CHAR);
        act(buf, FALSE, ch, 0, 0, TO_ROOM);
      } else if (GET_WEAPON_ATTACK_TYPE(obj) == WEAP_CANNON) {
        // It was in Decomposer mode. Collapse it.
        snprintf(buf, sizeof(buf), "%s collapses in on itself, its raging energies focusing down as it retracts into a jaw-like configuration.", GET_OBJ_NAME(obj));
        act(buf, FALSE, ch, 0, ch, TO_CHAR);
        act(buf, FALSE, ch, 0, 0, TO_ROOM);
      }

      send_to_char(ch, "\r\nA dispassionate female voice states, \"^cEnforcement mode: Lethal Eliminator. Please aim carefully and eliminate the target.^n\"\r\n");
      GET_OBJ_TYPE(obj) = ITEM_WEAPON;
      GET_WEAPON_ATTACK_TYPE(obj) = WEAP_HEAVY_PISTOL;
      GET_WEAPON_DAMAGE_CODE(obj) = DEADLY;
      GET_WEAPON_POWER(obj) = 100;
      break;
    case DOMINATOR_MODE_DECOMPOSER:
      if (GET_OBJ_TYPE(obj) == ITEM_OTHER || GET_WEAPON_ATTACK_TYPE(obj) == WEAP_TASER || GET_WEAPON_ATTACK_TYPE(obj) == WEAP_HEAVY_PISTOL) {
        // It will either print this message or print nothing (this is the end state of the gun-- either it reaches it or was already there.)
        snprintf(buf, sizeof(buf), "%s deploys in a flicker of whirling components, gripping your wrist and hissing with barely-contained destructive energies.", GET_OBJ_NAME(obj));
        act(buf, FALSE, ch, 0, ch, TO_CHAR);
        snprintf(buf, sizeof(buf), "%s deploys in a flicker of whirling components, gripping $n's wrist and hissing with barely-contained destructive energies.", GET_OBJ_NAME(obj));
        act(buf, FALSE, ch, 0, 0, TO_ROOM);
      }

      send_to_char(ch, "\r\nA dispassionate female voice states, \"^cEnforcement mode: Destroy Decomposer. Target will be completely annihilated. Please proceed with maximum caution.^n\"\r\n");
      GET_OBJ_TYPE(obj) = ITEM_WEAPON;
      GET_WEAPON_ATTACK_TYPE(obj) = WEAP_CANNON;
      GET_WEAPON_DAMAGE_CODE(obj) = DEADLY;
      GET_WEAPON_POWER(obj) = 100;
      break;
    case DOMINATOR_MODE_DEACTIVATED:
    default:
      if (GET_OBJ_TYPE(obj) != ITEM_OTHER) {
        // It was deployed.
        if (GET_WEAPON_ATTACK_TYPE(obj) == WEAP_TASER) {
          // Paralyzer mode-- lights go red.
          snprintf(buf, sizeof(buf), "%s's status lights flicker, then turn red.", GET_OBJ_NAME(obj));
          act(buf, FALSE, ch, 0, ch, TO_CHAR);
          act(buf, FALSE, ch, 0, 0, TO_ROOM);
        } else if (GET_WEAPON_ATTACK_TYPE(obj) == WEAP_HEAVY_PISTOL) {
          // It was in Lethal mode. Collapse it.
          snprintf(buf, sizeof(buf), "%s retracts and folds down into its standard heavy-pistol format, its status lights glowing red.", GET_OBJ_NAME(obj));
          act(buf, FALSE, ch, 0, ch, TO_CHAR);
          act(buf, FALSE, ch, 0, 0, TO_ROOM);
        } else if (GET_WEAPON_ATTACK_TYPE(obj) == WEAP_CANNON) {
          // It was in Destroyer mode. Collapse it.
          snprintf(buf, sizeof(buf), "%s vents energy with a sharp snap of power, collapsing with uncanny speed into its standard heavy-pistol format, its status lights glowing red.", GET_OBJ_NAME(obj));
          act(buf, FALSE, ch, 0, ch, TO_CHAR);
          act(buf, FALSE, ch, 0, 0, TO_ROOM);
        }
      }
      send_to_char(ch, "\r\nA dispassionate feminine voice states, \"^cThe trigger has been locked.^n\"\r\n");
      GET_OBJ_TYPE(obj) = ITEM_OTHER;
      break;
  }
}

// A completely non-canon implementation of the Dominator from Psycho-Pass. Intended for novelty, staff-only use.
SPECIAL(weapon_dominator) {

  struct obj_data *obj = (struct obj_data *) me;
  const char *rank = "";
  bool authorized;

  if (!ch || !cmd)
    return FALSE;

  struct obj_data *prev_wield = GET_EQ(ch, WEAR_WIELD);
  struct obj_data *prev_hold = GET_EQ(ch, WEAR_HOLD);

  if (CMD_IS("wield")) {
    // Process the wield command as normal, then process the gun's reaction (if any).
    do_wield(ch, argument, cmd, 0);

    // Nothing changed? Bail out. Return TRUE because we intercepted wield.
    if (GET_EQ(ch, WEAR_WIELD) == prev_wield && GET_EQ(ch, WEAR_HOLD) == prev_hold)
      return TRUE;

    // Are they now wielding this object?
    if ((GET_EQ(ch, WEAR_WIELD) == obj && prev_wield != obj) ||
        (GET_EQ(ch, WEAR_HOLD) == obj && prev_hold != obj)) {
      // Compose their rank string.
      if (GET_LEVEL(ch) == LVL_PRESIDENT) {
        rank = "Inspector";
        authorized = TRUE;
      } else if (GET_LEVEL(ch) == LVL_ADMIN) {
        rank = "Enforcer";
        authorized = TRUE;
      } else {
        authorized = FALSE;
      }

      // Act depending on if they're high-enough level or not.
      if (authorized) {
        // Send the intro message and configure this as a 50D stun weapon.
        send_to_char(ch, "As you grasp the weapon, a feminine voice dispassonately states, \"^cDominator Portable Psychological Diagnosis and Suppression System has been activated. User identification: %s %s. Affiliation: Public Safety Bureau, Criminal Investigation Department. Dominator usage approval confirmed; you are a valid user.^n\"\r\n", rank, GET_CHAR_NAME(ch));
        dominator_mode_switch(ch, obj, DOMINATOR_MODE_PARALYZER);
      } else {
        // Send the rejection message and configure this as a paperweight.
        send_to_char(ch, "As you grasp the weapon, a feminine voice dispassonately states, \"^cDominator Portable Psychological Diagnosis and Suppression System will not activate. User is unauthorized.^n\"\r\n");
        dominator_mode_switch(ch, obj, DOMINATOR_MODE_DEACTIVATED);
      }
    }

    return TRUE;
  }

  // For all other commands, we require that they're wielding the Dominator already.
  if (GET_EQ(ch, WEAR_WIELD) == obj) {
    if (CMD_IS("mode")) {
      const char *mode_string = "You must specify a valid mode from one of the following: Deactivated, Paralyzer, Eliminator, Decomposer.\r\n";
      // Override the current mode.
      if (!*argument) {
        send_to_char(mode_string, ch);
        return TRUE;
      }

      if (GET_LEVEL(ch) < LVL_ADMIN) {
        send_to_char(ch, "The Dominator doesn't respond.\r\n");
        return TRUE;
      }

      int mode = DOMINATOR_MODE_DEACTIVATED;

      skip_spaces(&argument);
      any_one_arg(argument, buf);

      // Switch mode, or tell the player the syntax.
      if (is_abbrev(buf, "non-lethal") || is_abbrev(buf, "paralyzer")) {
        mode = DOMINATOR_MODE_PARALYZER;
      } else if (is_abbrev(buf, "eliminator") || is_abbrev(buf, "lethal")) {
        mode = DOMINATOR_MODE_ELIMINATOR;
      } else if (is_abbrev(buf, "destroy") || is_abbrev(buf, "decomposer")) {
        mode = DOMINATOR_MODE_DECOMPOSER;
      } else if (is_abbrev(buf, "deactivated")) {
        mode = DOMINATOR_MODE_DEACTIVATED;
      } else {
        send_to_char(mode_string, ch);
        return TRUE;
      }

      send_to_char(ch, "You metally request the Sibyl System to override your Dominator's mode.\r\n");
      dominator_mode_switch(ch, obj, mode);
      return TRUE;
    }

    if (CMD_IS("holster") || CMD_IS("remove")) {
      // Holster or remove as normal, then check to see if the Dominator has been removed.
      if (CMD_IS("remove"))
        do_remove(ch, argument, cmd, 0);
      else if (CMD_IS("holster"))
        do_holster(ch, argument, cmd, 0);

      // Nothing changed? Bail out.
      if (GET_EQ(ch, WEAR_WIELD) == prev_wield && GET_EQ(ch, WEAR_HOLD) == prev_hold)
        return TRUE;

      // Did they unwield a Dominator?
      if ((prev_wield == obj && GET_EQ(ch, WEAR_WIELD) != obj) ||
          (prev_hold == obj && GET_EQ(ch, WEAR_HOLD) != obj))
        dominator_mode_switch(ch, obj, DOMINATOR_MODE_DEACTIVATED);

      return TRUE;
    }
  }

  // We did nothing.
  return FALSE;
}

extern void end_quest(struct char_data *ch);

#define REST_STOP_QUEST_VNUM       5000
#define REST_STOP_VNUM             8198
#define REST_STOP_TRUCK_CARGO_AREA 5062
#define REST_STOP_TRUCK_CABIN_AREA 5063

void orkish_truckdriver_drive_away(struct room_data *drivers_room) {
  delete drivers_room->dir_option[WEST];
  drivers_room->dir_option[WEST] = NULL;

  struct char_data *temp, *temp_next;
  for (temp = world[real_room(REST_STOP_TRUCK_CARGO_AREA)].people; temp; temp = temp_next) {
    temp_next = temp->next_in_room;
    send_to_char("You jump off the truck as it takes off!\r\n", temp);
    char_from_room(temp);
    char_to_room(temp, drivers_room);
  }
}

SPECIAL(orkish_truckdriver)
{
  struct char_data *driver = (struct char_data *) me;

  if (!driver || !driver->in_room) {
    mudlog("orkish_truckdriver would have crashed due to no in_room!", driver, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (FIGHTING(driver)) {
    if (quest_table[GET_QUEST(FIGHTING(driver))].vnum == REST_STOP_QUEST_VNUM) {
      send_to_char("The driver has been alerted! The run is a failure!\r\n", FIGHTING(driver));
      if (driver->in_room->dir_option[WEST])
        orkish_truckdriver_drive_away(driver->in_room);
      end_quest(FIGHTING(driver));
    }
    return FALSE;
  }

  if (driver->in_room->number == REST_STOP_VNUM && (time_info.hours < 2 || time_info.hours > 3)) {
    act("$n says goodbye to the girl at the stand and hops into his truck. Its engine rumbles into life and it drives off into the distance.", FALSE, driver, 0, 0, TO_ROOM);
    if (driver->in_room->dir_option[WEST])
      orkish_truckdriver_drive_away(driver->in_room);
    char_from_room(driver);
    char_to_room(driver, &world[real_room(REST_STOP_TRUCK_CABIN_AREA)]);
  } else if (driver->in_room->number == REST_STOP_TRUCK_CABIN_AREA && (time_info.hours == 2 || time_info.hours == 3)) {
    char_from_room(driver);
    char_to_room(driver, &world[real_room(REST_STOP_VNUM)]);
    if (!driver->in_room->dir_option[WEST]) {
      driver->in_room->dir_option[WEST] = new room_direction_data;
      memset((char *) driver->in_room->dir_option[WEST], 0,
             sizeof (struct room_direction_data));
      driver->in_room->dir_option[WEST]->to_room = &world[real_room(REST_STOP_TRUCK_CARGO_AREA)];
    }
    act("A GMC 4201 pulls into the rest stop, the orkish truck driver getting out and going over the coffee stand.", FALSE, driver, 0, 0, TO_ROOM);
  }
  return FALSE;
}
#undef REST_STOP_QUEST_VNUM
#undef REST_STOP_VNUM
#undef REST_STOP_TRUCK_CARGO_AREA
#undef REST_STOP_TRUCK_CABIN_AREA

SPECIAL(Janis_Amer_Girl) {
  NO_DRAG_BULLSHIT;

  struct char_data *mob = (struct char_data *) me, *tch = mob->in_room->people;
  if (!AWAKE(mob))
    return FALSE;
  if (GET_SPARE1(mob) && !cmd) {
    switch (GET_SPARE1(mob)) {
    case 1:
      do_say(mob, "About fraggin' time you got here.", 0, 0);
      break;
    case 3:
      do_say(mob, "Shut the frag up, we'll geek him when we geek him, right now we're going to tell the newcomer what we already know.", 0, 0);
      break;
    case 5:
      act("$n walks forward from the table, coming closer to you.", FALSE, mob, 0, 0, TO_ROOM);
      do_say(mob, "It's taking place at the docks, take that uniform on the table and go collect, he said \"Blue-eyes\" sent him, make sure to remember that you may need it.", 0, 0);
      break;
    case 6:
      for (; tch; tch = tch->next_in_room)
        if (quest_table[GET_QUEST(tch)].vnum == 5001)
          break;
      if (!tch)
        GET_SPARE1(mob) = 0;
      else if (GET_RACE(tch) != RACE_HUMAN || GET_SEX(tch) != SEX_MALE)
        do_say(mob, "And for frags sake, try and get someone who looks something like this guy to make the collection.", 0, 0);
      else
        do_say(mob, "Make sure you don't do anything weird, this guy won't hand it over if you're acting suspicious.", 0, 0);
      break;
    case 8:
      act("$n walks back towards the table and rests on it casually, \"Yeah, Geek the little fraggin' slitch, it'll be the last time he ever tries to frag us over.\"", FALSE, mob, 0, 0, TO_ROOM);
      break;
    case 10:
      do_say(mob, "Newcomer, your job, ^WLight^n the fragger up.", 0, 0);
      break;
    }
    GET_SPARE1(mob)++;
  } else if (!cmd)
    for (; tch; tch = tch->next_in_room)
      if (quest_table[GET_QUEST(tch)].vnum == 5001) {
        GET_SPARE1(mob) = 1;
        break;
      }
  if (CMD_IS("west")) {
    for (; tch; tch = tch->next_in_room)
      if (IS_NPC(tch) && GET_MOB_VNUM(tch) == 5030)
        break;
    if (GET_SPARE1(mob) < 11)
      do_say(mob, "Where the frag do you think you're going? Don't think J won't fraggin' hear about you walking out on us.", 0, 0);
    else if (tch)
      do_say(mob, "Hey fragger, don't you try and fraggin' leave without lighting this slitch up, J will fraggin' hear about this.",  0, 0);
    else
      act("$n claps with glee, \"Hardcore chum, J will hear all about this, she'll fraggin' love it.\"", FALSE, mob, 0, 0, TO_ROOM);
  }
  return FALSE;
}

SPECIAL(Janis_Amer_Jerry)
{
  struct char_data *mob = (struct char_data *) me;
  if (!AWAKE(mob) || cmd)
    return FALSE;
  if (GET_SPARE1(mob)) {
    switch (GET_SPARE1(mob)) {
    case 2:
      do_say(mob, "Sent word to J over an hour ago, how long we have to keep this chump alive?", 0, 0);
      break;
    case 4:
      do_say(mob, "Listen up, I don't want to have to repeat myself, you don't want to end up like this guy do you.", 0, 0);
      act("$n reaches back and slams his fist into the prisoner's face.", FALSE, mob, 0, 0, TO_ROOM);
      break;
    case 7:
      do_say(mob, "Can I fraggin' do him now?", 0, 0);
      break;
    case 9:
      act("$n pours gasoline from the jerry can over captive who starts screaming, he then turns to you and tosses you a lighter, grinning all the while.", FALSE, mob, 0, 0, TO_ROOM);
      break;
    }
    GET_SPARE1(mob)++;
  } else
    for (struct char_data *tch = mob->in_room->people; tch; tch = tch->next_in_room)
      if (quest_table[GET_QUEST(tch)].vnum == 5001) {
        act("The pair turn towards you as you enter.", FALSE, mob, 0, 0, TO_ROOM);
        GET_SPARE1(mob) = 1;
        break;
      }
  return FALSE;
}

SPECIAL(Janis_Amer_Door) {
  NO_DRAG_BULLSHIT;

  struct char_data *mob = (struct char_data *) me;
  if (!AWAKE(mob))
    return FALSE;
  if (CMD_IS("west") && !IS_SET(EXIT(mob, EAST)->exit_info, EX_CLOSED)) {
    do_say(mob, "See you around, omae.", 0, 0);
    do_gen_door(mob, "door", 0, SCMD_CLOSE);
  } else if ((CMD_IS("east") || CMD_IS("open")) && quest_table[GET_QUEST(ch)].vnum != 5001) {
    do_say(mob, "And just who do you think you are, punk?", 0, 0);
    return TRUE;
  } else if (!cmd && IS_SET(EXIT(mob, EAST)->exit_info, EX_CLOSED)) {
    for (struct char_data *tch = mob->in_room->people; tch; tch = tch->next_in_room)
      if (quest_table[GET_QUEST(tch)].vnum == 5001) {
        do_say(mob, "You must be the chum that J sent, the guy's in here.", 0, 0);
        do_gen_door(mob, "door", 0, SCMD_OPEN);
        break;
      }
  }
  return FALSE;
}

SPECIAL(Janis_Meet)
{
  struct char_data *mob = (struct char_data *) me;
  if (!AWAKE(mob))
    return FALSE;
  if ((CMD_IS("say") || CMD_IS("'") || CMD_IS("sayto")) && !IS_ASTRAL(ch) && *argument) {
    skip_spaces(&argument);
    if (!str_cmp(argument, "Blue-eyes sent me")) {
      if (GET_RACE(ch) != RACE_HUMAN || GET_SEX(ch) != SEX_MALE || !(GET_EQ(ch, WEAR_BODY) && GET_OBJ_VNUM(GET_EQ(ch, WEAR_BODY)) == 5032))  {
        do_say(mob, "Who the frag are you!? Oh wait, I fraggin' get it! This is a bust!", 0, 0);
        act("$n turns and starts running towards the road, quickly vanishing into the crowd.", FALSE, mob, 0, 0, TO_ROOM);
      } else {
        do_say(mob, "Ah, so you're the one they sent, here's the parcel. I've already collected payment.", 0, 0);
        struct obj_data *parcel = read_object(5033, VIRTUAL);
        obj_to_char(parcel, ch);
        act("$n hands $N the parcel, then turns and vanishes into the crowd of dockworkers.", FALSE, mob, 0, ch, TO_ROOM);
      }
      extract_char(mob);
      return TRUE;
    }
  }
  return FALSE;
}

SPECIAL(Janis_Captive)
{
  struct char_data *mob = (struct char_data *) me;
  if (CMD_IS("light") || CMD_IS("burn")) {
    act("As $N brings the lighter close to $n the fumes from the petrol catch alight, quickly spreading around $n's body.", FALSE, mob, 0, ch, TO_ROOM);
    act("The girl at the back of the warehouse begins to giggle with glee as the captive's screams are quickly stopped as he succumbs to the flames.", FALSE, mob, 0, 0, TO_ROOM);
    check_quest_kill(ch, mob);
    obj_to_room(read_object(5034, VIRTUAL), mob->in_room);
    extract_char(mob);
    return TRUE;
  }
  return FALSE;
}

SPECIAL(mageskill_hermes)
{
  struct char_data *mage = NULL;
  struct obj_data *recom = NULL;

  if (!ch)
    return FALSE;
  for (mage = ch->in_room->people; mage; mage = mage->next_in_room)
    if (GET_MOB_VNUM(mage) == 24806)
      break;

  if (!mage)
    return FALSE;

  if (CMD_IS("say") || CMD_IS("'") || CMD_IS("ask")) {
    skip_spaces(&argument);
    for (recom = ch->carrying; recom; recom = recom->next_content)
      if (GET_OBJ_VNUM(recom) == OBJ_MAGE_LETTER)
        break;
    if (!*argument)
      return FALSE;
    if (recom && GET_OBJ_VAL(recom, 0) == GET_IDNUM(ch) && (str_str(argument, "recommendation") || str_str(argument, "letter"))) {
      if (GET_OBJ_VAL(recom, 1) && GET_OBJ_VAL(recom, 2) >= 3 && GET_OBJ_VAL(recom, 3) && GET_OBJ_VAL(recom, 4) >= 2) {
        snprintf(arg, sizeof(arg), "%s So you have the recommendation from the other four. I guess that only leaves me. You put in the effort to get the other recommendations so you have mine.", GET_CHAR_NAME(ch));
        do_say(mage, arg, 0, SCMD_SAYTO);
        extract_obj(recom);
        recom = read_object(OBJ_MAGEBLING, VIRTUAL);
        obj_to_char(recom, ch);
        GET_OBJ_VAL(recom, 0) = GET_IDNUM(ch);
        act("$N hands you $p.", TRUE, ch, recom, mage, TO_CHAR);
        snprintf(arg, sizeof(arg), "%s Congratulations, you are now a member of the Order. Seek out the old Masonic Lodge on Swan Street in Tarislar and ask Timothy for training. Make sure you're wearing that chain when you do, otherwise you'll be ignored.", GET_CHAR_NAME(ch));
      } else
        snprintf(arg, sizeof(arg), "%s Why do you want my recommendation?", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
    } else if (str_str(argument, "chain")) {
      if (recom && GET_OBJ_VAL(recom, 0) == GET_IDNUM(ch)) {
        snprintf(arg, sizeof(arg), "%s You already have your letter. Seek out the other four senior members and ask them for their recommendations.", GET_CHAR_NAME(ch));
        do_say(mage, arg, 0, SCMD_SAYTO);
      } else {
        bool dq = FALSE;
        int i = 0;
        for (; i <= QUEST_TIMER; i++)
          if (GET_LQUEST(ch, i) == QST_MAGE_INTRO)
            dq = TRUE;
        if (dq) {
          // Reject people who couldn't pass the quest.
          if (GET_MAG(ch) <= 0 || GET_TRADITION(ch) == TRAD_MUNDANE || GET_TRADITION(ch) == TRAD_ADEPT) {
            snprintf(arg, sizeof(arg), "%s It's nice, isn't it? It's something that only elite mages and shamans can obtain.", GET_CHAR_NAME(ch));
            do_say(mage, arg, 0, SCMD_SAYTO);
            return TRUE;
          }

          snprintf(arg, sizeof(arg), "%s So Harold has sent another one my way has he? Sometimes I don't trust his better judgement, but business is business.", GET_CHAR_NAME(ch));
          do_say(mage, arg, 0, SCMD_SAYTO);
          snprintf(arg, sizeof(arg), "%s We are part of an order dedicated to higher learning in the field of magic. We are widespread around the metroplex and have our hands in more business than you would like to believe.", GET_CHAR_NAME(ch));
          do_say(mage, arg, 0, SCMD_SAYTO);
          snprintf(arg, sizeof(arg), "%s Seek out the other four senior members and ask them for their recommendations. Have them record them in this letter. Once you've gotten them, return to me and let me know.", GET_CHAR_NAME(ch));
          do_say(mage, arg, 0, SCMD_SAYTO);
          recom = read_object(OBJ_MAGE_LETTER, VIRTUAL);
          GET_OBJ_VAL(recom, 0) = GET_IDNUM(ch);
          obj_to_char(recom, ch);
          act("$N hands you $p.", TRUE, ch, recom, mage, TO_CHAR);
        } else {
          snprintf(arg, sizeof(arg), "%s Why would you want to know about that?", GET_CHAR_NAME(ch));
          do_say(mage, arg, 0, SCMD_SAYTO);
        }
      }
      return (TRUE);
    }
  }
  return(FALSE);
}

SPECIAL(mageskill_moore)
{
  struct char_data *mage = NULL;
  struct obj_data *recom = NULL;

  if (!ch)
    return FALSE;
  for (mage = ch->in_room->people; mage; mage = mage->next_in_room)
    if (GET_MOB_VNUM(mage) == 35538)
      break;

  if (!mage)
    return FALSE;

  if (CMD_IS("say") || CMD_IS("'") || CMD_IS("ask")) {
    skip_spaces(&argument);
    if (!*argument || !(str_str(argument, "recommendation") || str_str(argument, "letter")))
      return(FALSE);
    for (recom = ch->carrying; recom; recom = recom->next_content)
      if (GET_OBJ_VNUM(recom) == OBJ_MAGE_LETTER)
        break;
    if (!recom || GET_OBJ_VAL(recom, 0) != GET_IDNUM(ch))
      return FALSE;
    if (GET_OBJ_VAL(recom, 1)) {
      snprintf(arg, sizeof(arg), "%s I have already passed on my recommendation for you.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
    } else if (GET_REP(ch) >= 150) {
      snprintf(arg, sizeof(arg), "%s I feel you will be a welcome asset to our order.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
      GET_OBJ_VAL(recom, 1) = 1;
    } else {
      snprintf(arg, sizeof(arg), "%s I have not even heard of you; why would we want someone so fresh-faced? Come back when you have spent more time in the shadows.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
    }
    return TRUE;
  }
  return(FALSE);
}

SPECIAL(mageskill_herbie)
{
  struct char_data *mage = (struct char_data *) me;
  struct obj_data *recom = NULL, *obj = NULL;

  for (recom = ch->carrying; recom; recom = recom->next_content)
    if (GET_OBJ_VNUM(recom) == OBJ_MAGE_LETTER)
      break;
  if (!recom || GET_OBJ_VAL(recom, 0) != GET_IDNUM(ch))
    return FALSE;
  if (CMD_IS("say") || CMD_IS("'") || CMD_IS("ask")) {
    skip_spaces(&argument);
    if (!*argument || !(str_str(argument, "recommendation") || str_str(argument, "letter")))
      return(FALSE);
    if (GET_OBJ_VAL(recom, 2) >= 3) {
      snprintf(arg, sizeof(arg), "%s You have my recommendation, I can help you no further.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
    } else if (!GET_OBJ_VAL(recom, 2)) {
      snprintf(arg, sizeof(arg), "%s Excellent, you have come. I need you to create a spell for me to sell-- two of them, actually. Any spell is fine, just as long as it is force 4 or above.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
      GET_OBJ_VAL(recom, 2)++;
    } else {
      snprintf(arg, sizeof(arg), "%s Did you not hear me? Bring me two force-four spells you've made for me to sell.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
    }
    return TRUE;
  } else if (CMD_IS("give")) {
    if (!GET_OBJ_VAL(recom, 2)) {
      return FALSE;
    } else if (GET_OBJ_VAL(recom, 2) >= 3) {
      snprintf(arg, sizeof(arg), "%s You have my recommendation, I can help you no further.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
      return TRUE;
    }

    two_arguments(argument, buf, buf1);

    if (!(obj = get_obj_in_list_vis(ch, buf, ch->carrying))) {
      send_to_char(ch, "You don't seem to have %s %s.\r\n", AN(argument), argument);
      return TRUE;
    }

    // Let the give code handle it at this point.
    if (mage != give_find_vict(ch, buf1))
      return FALSE;

    if (GET_OBJ_TYPE(obj) == ITEM_SPELL_FORMULA && GET_OBJ_TIMER(obj) == 0 && GET_OBJ_VAL(obj, 0) >= 4 && GET_OBJ_VAL(obj, 8) == GET_IDNUM(ch)) {
      snprintf(arg, sizeof(arg), "%s Thank you, this will go towards providing for the order.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
      GET_OBJ_VAL(recom, 2)++;
      extract_obj(obj);
      return TRUE;
    } else {
      snprintf(arg, sizeof(arg), "%s I have no need for that.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
      return TRUE;
    }
  }
  return(FALSE);
}

SPECIAL(mageskill_anatoly)
{
  struct char_data *mage = (struct char_data *) me;
  struct obj_data *recom = NULL;
  if (CMD_IS("say") || CMD_IS("'") || CMD_IS("ask")) {
    skip_spaces(&argument);
    if (!*argument || !(str_str(argument, "recommendation") || str_str(argument, "letter")))
      return FALSE;
    for (recom = ch->carrying; recom; recom = recom->next_content)
      if (GET_OBJ_VNUM(recom) == OBJ_MAGE_LETTER)
        break;
    if (!recom || GET_OBJ_VAL(recom, 0) != GET_IDNUM(ch))
      return FALSE;
    if (GET_OBJ_VAL(recom, 3)) {
      snprintf(arg, sizeof(arg), "%s Ih hahf ahlreahdhee gihffehn yhou mhy blehzzehng.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
    } else if (GET_GRADE(ch) >= 3) {
      snprintf(arg, sizeof(arg), "%s Yhou vihl bhe ah vehlkohm ahddihzhuhn tuh ouhr ohrdehr.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
      GET_OBJ_VAL(recom, 3) = 1;
    } else {
      snprintf(arg, sizeof(arg), "%s Vhe ahr luuhkhehng vohr peohpl klohzehr tuh the ahztrahl plahyn.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
    }
    return TRUE;
  }
  return FALSE;
}

SPECIAL(mageskill_nightwing)
{
  struct char_data *mage = (struct char_data *) me;
  struct obj_data *recom = NULL;
  if (time_info.hours == 9 && GET_SPARE1(mage))
    GET_SPARE1(mage) = 0;
  if (!cmd && time_info.hours == 8 && !GET_SPARE1(mage)) {
    int toroom = NOWHERE;
    switch (number(0, 5)) {
      case 0:
        toroom = real_room(2496);
        break;
      case 1:
        toroom = real_room(5405);
        break;
      case 2:
        toroom = real_room(2347);
        break;
      case 3:
        toroom = real_room(14379);
        break;
      case 4:
        toroom = real_room(2590);
        break;
      case 5:
        toroom = real_room(12502);
        break;
    }
    act("$n suddenly dashes towards the nearest entrance.", FALSE, mage, 0, 0, TO_ROOM);
    char_from_room(mage);
    char_to_room(mage, &world[toroom]);
    act("$n dashes into the room, searching for the darkest corner.", FALSE, mage, 0, 0, TO_ROOM);
    GET_SPARE1(mage) = 1;
    return TRUE;
  }
  if (CMD_IS("say") || CMD_IS("'") || CMD_IS("ask")) {
    skip_spaces(&argument);
    if (!*argument || !(str_str(argument, "recommendation") || str_str(argument, "letter")))
      return FALSE;
    for (recom = ch->carrying; recom; recom = recom->next_content)
      if (GET_OBJ_VNUM(recom) == OBJ_MAGE_LETTER)
        break;
    if (!recom || GET_OBJ_VAL(recom, 0) != GET_IDNUM(ch))
      return FALSE;
    if (GET_OBJ_VAL(recom, 4) >= 2) {
      snprintf(arg, sizeof(arg), "%s Bat given you recommendation, now fly.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
    } else if (time_info.hours > 7 && time_info.hours < 19) {
      snprintf(arg, sizeof(arg), "%s Bat must hide now, light blinding her.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
    } else if (GET_OBJ_VAL(recom, 5) == ch->in_room->number) {
      snprintf(arg, sizeof(arg), "%s Bat busy, Bat must leave. You come find Bat somewhere else.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
    } else if (GET_OBJ_VAL(recom, 4) == 1) {
      snprintf(arg, sizeof(arg), "%s Bat thinks you worthy, Bat gives you blessing.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
      GET_OBJ_VAL(recom, 4)++;
    } else {
      snprintf(arg, sizeof(arg), "%s Bat see you, Bat no give blessing now, you find Bat again.", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
      GET_OBJ_VAL(recom, 4)++;
      GET_OBJ_VAL(recom, 5) = ch->in_room->number;
    }
    return TRUE;
  }
  return FALSE;
}

SPECIAL(mageskill_trainer)
{
  struct char_data *mage = (struct char_data *) me;
  struct obj_data *chain = NULL;
  int i;

  if (CMD_IS("say") || CMD_IS("'") || CMD_IS("ask")) {
    skip_spaces(&argument);
    if (!*argument || !str_str(argument, "training"))
      return FALSE;
    for (i = 0; i < NUM_WEARS && !chain; i++)
      if (GET_EQ(ch, i) && GET_OBJ_VNUM(GET_EQ(ch, i)) == OBJ_MAGEBLING)
        chain = GET_EQ(ch, i);
    if (!chain)
      return FALSE;
    if (GET_OBJ_VAL(chain, 0) != GET_IDNUM(ch)) {
      snprintf(arg, sizeof(arg), "%s What are you doing with this!? This is not yours!", GET_CHAR_NAME(ch));
      do_say(mage, arg, 0, SCMD_SAYTO);
      send_to_char(ch, "%s reaches out and snatches the chain from around your neck.\r\n", GET_NAME(mage));
      act("$n reaches out and snatches the chain from around $N's neck.", FALSE, mage, 0, ch, TO_NOTVICT);
      extract_obj(unequip_char(ch, i, TRUE));
    } else {
      snprintf(arg, sizeof(arg), "%s Welcome %s, the Master awaits.", GET_CHAR_NAME(ch), GET_SEX(ch) == SEX_FEMALE ? "Sister" : "Brother");
      do_say(mage, arg, 0, SCMD_SAYTO);
      send_to_char(ch, "%s beckons you to pass through the field to the north, and you do.\r\n", GET_NAME(mage));
      act("$n passes through the field to the north.", TRUE, ch, 0, 0, TO_ROOM);
      char_from_room(ch);
      char_to_room(ch, &world[real_room(RM_MAGE_TRAINER)]);
    }
  }
  return FALSE;
}

SPECIAL(knightcenter_bouncer) {
  NO_DRAG_BULLSHIT;

  struct char_data *wendigo = (char_data *) me;
  struct obj_data *obj;
  bool found = FALSE;

  if (!AWAKE(ch))
    return(FALSE);

  if (CMD_IS("north")) {
    for (obj = ch->carrying; obj; obj = obj->next_content)
      if (GET_OBJ_VNUM(obj) == 19999)
        found = TRUE;

    if (found) {
      act("$n tediously studies the handwriting on the note before waving you through.", FALSE, wendigo, 0, ch, TO_VICT);
      perform_move(ch, NORTH, LEADER, NULL);
    } else
      act("$n steps infront of the doorway and waves you off, \"Not today, chump.\"", FALSE, wendigo, 0, 0, TO_ROOM);
    return(TRUE);
  }

  return(FALSE);
}


SPECIAL(spraypaint)
{
  return FALSE;
}

SPECIAL(sombrero_bridge)
{
  if (!ch || !cmd || !(CMD_IS("land") || CMD_IS("blastoff") || CMD_IS("beam")))
    return FALSE;

  if (GET_LEVEL(ch) <= LVL_BUILDER) {
    send_to_char("All the controls seem to be written in Esperanto.\r\n", ch);
    return TRUE;
  }
  skip_spaces(&argument);
  extern void open_taxi_door(struct room_data *room, int dir, struct room_data *taxi_room, sbyte rating=0);
  extern void close_taxi_door(struct room_data *room, int dir, struct room_data *taxi_room);
  int dir;
  struct room_data *welcome = &world[real_room(1003)];

  if (CMD_IS("land")) {
    if (!*argument) {
      send_to_char("Where do you want to land?\r\n", ch);
      return TRUE;
    }
    vnum_t target = real_room(atol(argument));
    int dir;

    if (ROOM_FLAGGED(&world[target], ROOM_INDOORS)) {
      send_to_char("Try landing outdoors somewhere.\r\n", ch);
      return TRUE;
    }
    for (dir = number(NORTH, UP - 1);; dir = number(NORTH, UP - 1))
      if (!world[target].dir_option[dir]) {
        open_taxi_door(&world[target], dir, welcome);
        if (world[target].people) {
          snprintf(buf, sizeof(buf), "A large sombrero shaped object descends from the sky, a walkway extends and touches ground to the %s.", fulldirs[dir]);
          act(buf, FALSE, world[target].people, 0, 0, TO_ROOM);
          act(buf, FALSE, world[target].people, 0, 0, TO_CHAR);
        }
        if (welcome->people) {
          snprintf(buf, sizeof(buf), "There is a small rumble as the El Sombrero lands, the bay doors opening to the %s.", fulldirs[dir]);
          act(buf, FALSE, welcome->people, 0, 0, TO_ROOM);
          act(buf, FALSE, welcome->people, 0, 0, TO_CHAR);
        }
        return TRUE;
      }
  } else if (CMD_IS("blastoff")) {
    for (dir = NORTH; dir < UP; dir++)
      if (welcome->dir_option[dir]) {
        struct room_data *to = welcome->dir_option[dir]->to_room;
        close_taxi_door(to, rev_dir[dir], welcome);
        if (to->people) {
          snprintf(buf, sizeof(buf), "The walkway retracts back into the sombrero shaped object and it silently shoots off into the sky.");
          act(buf, FALSE, to->people, 0, 0, TO_ROOM);
          act(buf, FALSE, to->people, 0, 0, TO_CHAR);
        }
        if (welcome->people) {
          snprintf(buf, sizeof(buf), "The bay doors close and you feel a slight rumble as the El Sombrero blasts off into space.");
          act(buf, FALSE, welcome->people, 0, 0, TO_ROOM);
          act(buf, FALSE, welcome->people, 0, 0, TO_CHAR);
        }
        return TRUE;
      }
  } else if (CMD_IS("beam")) {
    char_data *victim;
    if (!*argument)
      send_to_char("Who do you want to beam?\r\n", ch);
    else if (!(victim = get_char_vis(ch, argument)))
      send_to_char(ch, "You can't see anyone named '%s'.\r\n", argument);
    else if (GET_ROOM_VNUM(victim->in_room) >= 1000 && GET_ROOM_VNUM(victim->in_room) <= 1099) {
      send_to_char("PZZZAAAAAAATTTT!!!!\r\n", victim);
      act("$n sparkles momentarily then fades from view.", FALSE, victim, 0, 0, TO_ROOM);
      char_from_room(victim);
      char_to_room(victim, &world[real_room(32611)]);
      act("$n fades in to view, surrounded by sparkles and the faint smell of salsa.", FALSE, victim, 0, 0, TO_ROOM);
      send_to_char(ch, "%s has been beamed down to the planet.\r\n", GET_CHAR_NAME(victim));
    } else {
      send_to_char("PZZZAAAAAAATTTT!!!!\r\n", victim);
      act("$n sparkles momentarily then fades from view.", FALSE, victim, 0, 0, TO_ROOM);
      char_from_room(victim);
      char_to_room(victim, &world[real_room(1003)]);
      act("$n fades in to view, surrounded by sparkles and the faint smell of salsa.", FALSE, victim, 0, 0, TO_ROOM);
      send_to_char(ch, "%s has been beamed aboard.\r\n", GET_CHAR_NAME(victim));
    }
    return TRUE;
  }
  return FALSE;
}

SPECIAL(sombrero_infinity) {
  NO_DRAG_BULLSHIT;

  if (!cmd)
    return FALSE;
  if (CMD_IS("west") && GET_LEVEL(ch) == LVL_MORTAL) {
    send_to_char("Try as you might you can't seem to reach the end.\r\n", ch);
    return TRUE;
  }
  return FALSE;
}

SPECIAL(cybered_yakuza)
{
  char_data *yakuza = (char_data *) me;

  if (yakuza != NULL && CMD_IS("open") && CAN_SEE(yakuza, ch) &&
      yakuza->in_room->number == 39355) {
    skip_spaces(&argument);

    if (!str_cmp(argument, "door")) {
      act("$n swears loudly in Japanese and attacks.", FALSE, yakuza, 0, ch, TO_VICT);
      act("$n swears loudly in Japanese and attacks $N.", FALSE, yakuza, 0, ch, TO_NOTVICT);
      set_fighting(yakuza, ch);

      return TRUE;
    }
  }

  return FALSE;
}

SPECIAL(airport_gate) {
  NO_DRAG_BULLSHIT;

  if (!cmd)
    return FALSE;
  struct room_data *in_room = ch->in_veh ? ch->in_veh->in_room : ch->in_room;
  long to_room = 0;
  if (in_room->number == 62166)
    to_room = real_room(62167);
  else to_room = real_room(62197);

  if ((in_room->number == 62166 && CMD_IS("east")) || (in_room->number == 62173 && CMD_IS("west"))) {
    if (!PLR_FLAGGED(ch, PLR_VISA)) {
      send_to_char("The guards won't let you pass until you've shown your visa.\r\n", ch);
      return TRUE;
    }
    send_to_char("You move through the checkpoint to the departure platform.\r\n", ch);
    act("$n moves through the checkpoint to the departure platform.\r\n", FALSE, ch, 0, 0, TO_ROOM);
    PLR_FLAGS(ch).RemoveBit(PLR_VISA);
    char_from_room(ch);
    char_to_room(ch, &world[to_room]);
    return TRUE;
  }

  return FALSE;
}

SPECIAL(airport_guard)
{
  if (cmd && CMD_IS("show")) {
    skip_spaces(&argument);
    struct char_data *guard = (struct char_data *) me;
    one_argument(argument, buf);
    struct obj_data *visa = get_obj_in_list_vis(ch, buf, ch->carrying);
    if (visa && GET_OBJ_VNUM(visa) == OBJ_CARIBBEAN_VISA) {
      if (GET_OBJ_VAL(visa, 0) == GET_IDNUM(ch)) {
        PLR_FLAGS(ch).SetBit(PLR_VISA);
        snprintf(arg, sizeof(arg), "%s Everything seems to be in order, proceed.", GET_CHAR_NAME(ch));
        do_say(guard, arg, 0, SCMD_SAYTO);
      } else {
        snprintf(arg, sizeof(arg), "%s Get out of here, before we arrest you.", GET_CHAR_NAME(ch));
        extract_obj(visa);
        do_say(guard, arg, 0, SCMD_SAYTO);
      }
      return TRUE;
    } else return FALSE;
  }
  return FALSE;
}

// Give to morts for testing combat so staff don't have to restore them all the time.
SPECIAL(restoration_button) {
  if (!cmd)
    return FALSE;

  if (CMD_IS("restore")) {
    restore_character(ch, FALSE);
    send_to_char("You use the cheat button to self-restore, enabling further testing.\r\n", ch);
    mudlog("Self-restore button used.", ch, LOG_CHEATLOG, TRUE);
    return TRUE;
  }

  if (CMD_IS("jump")) {
    for (int dest = 0; *(seattle_taxi_destinations[dest].keyword) != '\n'; dest++) {
      // Skip invalid destinations.
      if (!DEST_IS_VALID(dest, seattle_taxi_destinations))
        continue;

      if ( str_str((const char *)argument, seattle_taxi_destinations[dest].keyword)) {
        snprintf(buf, sizeof(buf), "%ld", seattle_taxi_destinations[dest].vnum);
        do_goto(ch, buf, 0, 0);
        return TRUE;
      }
    }
    send_to_char(ch, "'%s' is not a valid location. You can jump to anywhere that has a taxi stop (ex: jump afterlife).\r\n", argument);
    return TRUE;
  }

  return FALSE;
}

int axehead_last_said = -1;
SPECIAL(axehead) {
  int message_num;
  const char *axehead_messages[] = {
    "Runners these days don't realize how valuable keeping notes on their pocket secretary is. Like where Johnsons hang out.",
    "Stick your radio and phone in a pocket. You can still hear 'em, and it keeps your hands free.",
    "Seems like every day I hear about another wanna-be runner getting gunned down by the Star for walking around with their gun in their hand.",
    "Back in my day, we didn't have anything like the 8 MHz band available. Being able to talk to runners is a blessing.",
    "When in doubt, just take a cab back to somewhere familiar.",
    "If you're on a job and you just can't get it done, call your Johnson and tell them you quit. Easier than hoofing it all the way back.",
    "It's dangerous to go alone. Make friends."
  };
#define NUM_AXEHEAD_MESSAGES 7

  if (cmd || FIGHTING(ch) || !AWAKE(ch) || (message_num = number(0, NUM_AXEHEAD_MESSAGES * 20)) >= NUM_AXEHEAD_MESSAGES || message_num == axehead_last_said)
    return FALSE;

  do_say(ch, axehead_messages[message_num], 0, 0);
  axehead_last_said = message_num;
  return TRUE;
}

SPECIAL(archetype_chargen_magic_split) {
  NO_DRAG_BULLSHIT;

  struct room_data *room = (struct room_data *) me;
  struct room_data *temp_to_room = NULL;

  if (!ch || !cmd || IS_NPC(ch))
    return FALSE;

  // Funnel hermetic and shamanic mages into the correct archetypal path.
  if (CMD_IS("south") || CMD_IS("s")) {
    // Store the current exit, then overwrite with our custom one.
    temp_to_room = room->dir_option[SOUTH]->to_room;
    if (GET_TRADITION(ch) == TRAD_HERMETIC)
      room->dir_option[SOUTH]->to_room = &world[real_room(RM_ARCHETYPAL_CHARGEN_PATH_OF_THE_MAGICIAN_HERMETIC)];
    else
      room->dir_option[SOUTH]->to_room = &world[real_room(RM_ARCHETYPAL_CHARGEN_PATH_OF_THE_MAGICIAN_SHAMANIC)];

    // Execute the actual command as normal. We know it'll always be cmd_info since you can't rig or mtx in chargen.
    ((*cmd_info[cmd].command_pointer) (ch, argument, cmd, cmd_info[cmd].subcmd));

    // Restore the south exit for the room to the normal one.
    room->dir_option[SOUTH]->to_room = temp_to_room;
    return TRUE;
  }

  return FALSE;
}

SPECIAL(archetype_chargen_reverse_magic_split) {
  NO_DRAG_BULLSHIT;

  struct room_data *room = (struct room_data *) me;
  struct room_data *temp_to_room = NULL;

  if (!ch || !cmd || IS_NPC(ch))
    return FALSE;

  // Funnel hermetic and shamanic mages into the correct archetypal path.
  if (CMD_IS("north") || CMD_IS("n")) {
    // Store the current exit, then overwrite with our custom one.
    temp_to_room = room->dir_option[NORTH]->to_room;
    if (GET_TRADITION(ch) == TRAD_HERMETIC)
      room->dir_option[NORTH]->to_room = &world[real_room(RM_ARCHETYPAL_CHARGEN_CONJURING_HERMETIC)];
    else
      room->dir_option[NORTH]->to_room = &world[real_room(RM_ARCHETYPAL_CHARGEN_CONJURING_SHAMANIC)];

    // Execute the actual command as normal. We know it'll always be cmd_info since you can't rig or mtx in chargen.
    ((*cmd_info[cmd].command_pointer) (ch, argument, cmd, cmd_info[cmd].subcmd));

    // Restore the north exit for the room to the normal one.
    room->dir_option[NORTH]->to_room = temp_to_room;
    return TRUE;
  }

  return FALSE;
}

SPECIAL(nerpcorpolis_lobby) {
  // I am requesting a room for private RP sessions, and agree that I will not use it as an apartment, storage, or for other in-game benefit.
  skip_spaces(&argument);
  if (!strcmp("I am requesting a room for private RP sessions, and agree that I will not use it as an apartment, storage, or for other in-game benefit.", argument)) {
    char_from_room(ch);
    char_to_room(ch, &world[real_room(RM_NERPCORPOLIS_RECEPTIONIST)]);

    send_to_char("You are ushered into the leasing office.\r\n", ch);
    act("$n is ushered into the leasing office.", FALSE, ch, 0, 0, TO_ROOM);

    // If not screenreader, look.
    if (!PRF_FLAGGED(ch, PRF_SCREENREADER))
      look_at_room(ch, 0, 0);
  }
  // We still want them to say whatever it is they're saying.
  return FALSE;
}

SPECIAL(troll_barrier) {
  NO_DRAG_BULLSHIT;

  if (!cmd)
    return FALSE;

  if (CMD_IS("west") || CMD_IS("w")) {
    if (!(GET_RACE(ch) == RACE_TROLL || GET_RACE(ch) == RACE_GIANT || GET_RACE(ch) == RACE_FOMORI || GET_RACE(ch) == RACE_CYCLOPS)) {
      send_to_char("A massive troll blocks the way and keeps you from going any further.\r\n", ch);
      act("$n stumbles into the immovable brick wall of a troll guard with an oof.", FALSE, ch, 0, 0, TO_ROOM);
      return TRUE;
    }
  }
  return FALSE;
}

SPECIAL(chargen_docwagon_checker) {
  NO_DRAG_BULLSHIT;

  struct char_data *checker = (char_data *) me;
  struct obj_data *obj;

  if (!AWAKE(ch))
    return(FALSE);

  if (CMD_IS("south")) {
    // Check inventory.
    for (obj = ch->carrying; obj; obj = obj->next_content) {
      if (GET_OBJ_TYPE(obj) == ITEM_DOCWAGON) {
        if (GET_DOCWAGON_BONDED_IDNUM(obj) != GET_IDNUM(ch)) {
          act("$n holds up a hand, stopping $N. \"Hold on, chummer. You need to "
              "^WWEAR^n and ^WBOND^n your modulator before you move on. It'll help keep you alive.\"",
              FALSE, checker, 0, ch, TO_ROOM);
        } else {
          act("$n holds up a hand, stopping $N. \"Hold on, chummer. You still need to "
              "^WWEAR^n your modulator before you move on. It'll help keep you alive.\"",
              FALSE, checker, 0, ch, TO_ROOM);
        }
        return TRUE;
      }
    }

    // Check equipment.
    for (int i = 0; i < NUM_WEARS; i++) {
      if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_DOCWAGON) {
        if (GET_DOCWAGON_BONDED_IDNUM(GET_EQ(ch, i)) != GET_IDNUM(ch)) {
          act("$n holds up a hand, stopping $N. \"Hold on, chummer. You still need to "
              "^WBOND^n your modulator before you move on. It'll help keep you alive.\"",
              FALSE, checker, 0, ch, TO_ROOM);
          return TRUE;
        } else {
          // Good to go-- worn and bonded.
          act ("$n looks $N over, then nods approvingly and lets $M by.", FALSE, checker, 0, ch, TO_ROOM);
          return FALSE;
        }
      }
    }

    act("$n looks $N over on $S way past, then shakes $s head. \"No modulator's a risky way to live. You might want to sell some stuff and pick one up, or just prioritize getting one out in the Sprawl.\"\r\n", FALSE, checker, 0, ch, TO_ROOM);
  }

  return(FALSE);
}

SPECIAL(nerpcorpolis_button) {
  struct obj_data *obj = (struct obj_data *) me;

  struct char_data *tmp_char;
  struct obj_data *tmp_object;

  // No argument given, no character available, no problem. Skip it.
  if (!*argument || !ch || ch->in_veh)
    return FALSE;

  // If it's not a command that would reveal this sign's data, skip it.
  if (!(CMD_IS("push") || CMD_IS("press") || CMD_IS("use") || CMD_IS("activate")))
    return FALSE;

  // Search the room and find something that matches me.
  generic_find(argument, FIND_OBJ_ROOM, ch, &tmp_char, &tmp_object);

  // Senpai didn't notice me? Skip it, he's not worth it.
  if (!tmp_object || tmp_object != obj)
    return FALSE;

  // Select our destination list based on our room's vnum.
  struct room_data *room = get_obj_in_room(obj);
  if (!room)
    return FALSE;

  vnum_t teleport_to = -1;

  switch (GET_ROOM_VNUM(room)) {
    case 1003:
      teleport_to = 6901;
      break;
    case 6901:
      teleport_to = 1003;
      break;
    default:
      snprintf(buf, sizeof(buf), "SYSERR: Invalid room %ld for teleport button.", GET_ROOM_VNUM(room));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      return FALSE;
  }

  int teleport_rnum = real_room(teleport_to);
  if (teleport_rnum <= -1) {
    snprintf(buf, sizeof(buf), "SYSERR: Invalid destination %ld for teleport button in %ld.", teleport_to, GET_ROOM_VNUM(room));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  send_to_char("Your surroundings blur and shift.\r\n", ch);
  act("$n presses the button and disappears.", TRUE, ch, 0, 0, TO_ROOM);
  char_from_room(ch);
  char_to_room(ch, &world[teleport_rnum]);
  act("$n appears from the teleporter.", TRUE, ch, 0, 0, TO_ROOM);

  return TRUE;
}

int fatcop_last_said = -1;
SPECIAL(fatcop) {
  int message_num;
  const char *fatcop_messages[] = {
    "Why are we stuck on guard duty? I'll tell you why, because we're the best of the best, the cream of the crop.",
    "Rumour is the higher-ups are keeping what happened here a secret, gotta be careful about the media snooping around.",
    "These donuts are carbs and sugar, pure energy, energy I'll need if I gotta chase down any of these low-life criminals.",
    "Back in my day, we shot first and didn't ask questions after. This new generation is too soft I tell ya.",
    "You see all those burned bodies they dragged outta here? Something real fragged up happened, I tell ya.",
    "Look, all I'm sayin' is, if I wasn't supposed to eat three dozen donuts a day, it'd say so on the box, right?",
    "So the doctor says... Rectum? Damn near killed em!",
    "Did I tell you about the time I took out a whole team of runners, single-handed, With no back-up?",
    "Look, it ain't a crime if there ain't no witnesses, that's all I'm sayin'.",
    "Did you see those chummers in black suits? Their badges didn't even have names on em, don't even know what agency."
  };
#define NUM_FATCOP_MESSAGES 10

  if (cmd || FIGHTING(ch) || !AWAKE(ch) || (message_num = number(0, NUM_FATCOP_MESSAGES * 20)) >= NUM_FATCOP_MESSAGES || message_num == fatcop_last_said)
    return FALSE;

  do_say(ch, fatcop_messages[message_num], 0, 0);
  fatcop_last_said = message_num;
  return TRUE;
}

// The only thing this does is make the object display a usage string if it's on the floor.
SPECIAL(floor_usable_radio) {
  return FALSE;
}

// Override the 'install' command in the presence of an unpacked medical workshop or facility.
SPECIAL(medical_workshop) {
  bool mode_is_install = FALSE, mode_is_diagnose = FALSE;
  struct obj_data *workshop = (struct obj_data *) me;
  struct obj_data *ware, *found_obj = NULL;
  struct char_data *found_char = NULL;

  char target_arg[MAX_INPUT_LENGTH];

  // No argument given, no character available, no problem. Skip it.
  if (!*argument || !ch || !workshop || GET_OBJ_TYPE(workshop) != ITEM_WORKSHOP)
    return FALSE;

  // Skip anything that's not an expected command.
  if (!((mode_is_install = CMD_IS("install")) || CMD_IS("uninstall") || (mode_is_diagnose = CMD_IS("diagnose"))))
    return FALSE;

  // Require that the medical workshop be unpacked.
  if (!GET_WORKSHOP_IS_SETUP(workshop))
    return FALSE;

  // Preliminary skill check.
  if (GET_SKILL(ch, SKILL_BIOTECH) < 4 || GET_SKILL(ch, SKILL_MEDICINE) < 4) {
    send_to_char("You have no idea where to even start with surgeries. Better leave it to the professionals.\r\n", ch);
    return TRUE;
  }

  if (!PLR_FLAGGED(ch, PLR_CYBERDOC) && !access_level(ch, LVL_ADMIN)) {
    send_to_char("The cyberdoc role is currently application-only. Please contact staff to request the ability to be a cyberdoc.\r\n", ch);
    return TRUE;
  }

  if (!*arg) {
    send_to_char(ch, "Syntax: %sinstall <target character> <'ware>\r\n", mode_is_install ? "" : "un");
    return TRUE;
  }

  // Parse out the victim targeting argument.
  argument = one_argument(argument, target_arg);

  // Reject self-targeting.
  if ((!str_cmp(target_arg, "self") || !str_cmp(target_arg, "me") || !str_cmp(target_arg, "myself"))) {
    send_to_char("You can't operate on yourself!\r\n", ch);
    return TRUE;
  }

  // Identify the victim. Since this is first, this means our syntax is '[un]install <character> <ware>'.
  generic_find(target_arg, FIND_CHAR_ROOM, ch, &found_char, &found_obj);

  if (!found_char) {
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", target_arg);
    return TRUE;
  }

  // Reject self-targeting.
  if (found_char == ch) {
    send_to_char("You can't operate on yourself!\r\n", ch);
    return TRUE;
  }

  if (IS_NPC(found_char) || IS_ASTRAL(found_char) || IS_PROJECT(found_char)) {
    send_to_char("Not on NPCs.\r\n", ch);
    return TRUE;
  }

  if (!found_char->desc) {
    send_to_char("They're disconnected right now, try again later.", ch);
    return TRUE;
  }

  if (GET_LEVEL(found_char) > GET_LEVEL(ch)) {
    send_to_char("Operating on staff is a terrible idea.\r\n", ch);
    return TRUE;
  }

  // Ensure we have the target's permission.
  if (!PRF_FLAGGED(found_char, PRF_TOUCH_ME_DADDY)) {
    send_to_char(ch, "You can't diagnose or operate on %s-- they need to use the ^WTOGGLE CYBERDOC^n command.\r\n", GET_CHAR_NAME(found_char));
    return TRUE;
  }

  // Diagnostics command.
  if (mode_is_diagnose) {
    act("You fire up $p's diagnostic scanner and point it at $N.\r\n", FALSE, ch, workshop, found_char, TO_CHAR);
    act("$n fires up $p's diagnostic scanner and points it at $N.\r\n", FALSE, ch, workshop, found_char, TO_ROOM);

    // Diagnose.
    diag_char_to_char(found_char, ch);

    // List cyberware.
    if (!found_char->cyberware) {
      act("\r\n$N has no cyberware.", FALSE, ch, 0, found_char, TO_CHAR);
    } else {
      act("\r\n$N has the following cyberware:", FALSE, ch, 0, found_char, TO_CHAR);
      for (struct obj_data *obj = found_char->cyberware; obj != NULL; obj = obj->next_content) {
        snprintf(buf, sizeof(buf), "%-40s Essence: %0.2f\r\n",
                GET_OBJ_NAME(obj), ((float)GET_OBJ_VAL(obj, 4) / 100));
        send_to_char(buf, ch);
      }
    }

    // List bioware.
    if (!found_char->bioware) {
      act("\r\n$N has no bioware.", FALSE, ch, 0, found_char, TO_CHAR);
    } else {
      act("\r\n$N has the following bioware:", FALSE, ch, 0, found_char, TO_CHAR);
      for (struct obj_data *obj = found_char->bioware; obj != NULL; obj = obj->next_content) {
        snprintf(buf, sizeof(buf), "%-40s Rating: %-2d     Bioware Index: %0.2f\r\n",
                GET_OBJ_NAME(obj),
                GET_OBJ_VAL(obj, 1), ((float)GET_OBJ_VAL(obj, 4) / 100));
        send_to_char(buf, ch);
      }
    }

    send_to_char("\r\n", ch);

    // Show essence and index.
    send_to_char(get_plaintext_score_essence(found_char), ch);
    send_to_char(ch, "Essence Hole: %.2f\r\n", (float)found_char->real_abils.esshole / 100);
    if (GET_MAG(found_char))
      send_to_char(ch, "Bioware Hole: %.2f\r\n", (float)(found_char->real_abils.highestindex - GET_INDEX(found_char)) / 100);

    return TRUE;
  } /* End diagnose command. */

  // Parse out the dotmode.
  int dotmode = find_all_dots(argument);

  // Can't operate with more than one thing at once.
  if ((dotmode == FIND_ALL) || dotmode == FIND_ALLDOT) {
    send_to_char(ch, "You'll have to %sinstall one thing at a time.\r\n", mode_is_install ? "" : "un");
    return TRUE;
  }

  // Select the object.
  if (mode_is_install) {
    if (!(ware = get_obj_in_list_vis(ch, argument, ch->carrying))) {
      send_to_char(ch, "You aren't carrying anything called '%s'.\r\n", argument);
      return TRUE;
    }

    if (GET_OBJ_TYPE(ware) != ITEM_SHOPCONTAINER) {
      send_to_char(ch, "You're standing in a medical %s, so you can only %sinstall 'ware! %s doesn't count.\r\n",
                   GET_WORKSHOP_GRADE(workshop) == TYPE_WORKSHOP ? "workshop" : "facility",
                   mode_is_install ? "" : "un",
                   GET_OBJ_NAME(ware));
      return TRUE;
    } else {
      if (!ware->contains) {
        send_to_char(ch, "Something went wrong! Please alert staff.\r\n");
        return TRUE;
      }
      ware = ware->contains;
    }
  } else {
    if (!(ware = get_obj_in_list_vis(ch, argument, found_char->cyberware)) && !(ware = get_obj_in_list_vis(ch, argument, found_char->bioware))) {
      send_to_char(ch, "%s doesn't have anything called '%s' installed.\r\n", GET_CHAR_NAME(found_char), argument);
      return TRUE;
    }
  }

  // Reject operations on anything that isn't 'ware.'
  if (GET_OBJ_TYPE(ware) != ITEM_BIOWARE && GET_OBJ_TYPE(ware) != ITEM_CYBERWARE) {
    send_to_char(ch, "You're standing in a medical %s, so you can only %sinstall 'ware! %s doesn't count.\r\n",
                 GET_WORKSHOP_GRADE(workshop) == TYPE_WORKSHOP ? "workshop" : "facility",
                 mode_is_install ? "" : "un",
                 GET_OBJ_NAME(ware));
    return TRUE;
  }

  /* We should be good to go. At this point, we expect that:
     - We're standing in a workshop
     - We've identified the target, and they have consented
     - We've identified the 'ware, and it's valid
  */

  // Base TN is the usual 4.
  int target = 4;
  snprintf(buf, sizeof(buf), "Medical roll for %s on %s: Base TN %d", GET_CHAR_NAME(ch), GET_CHAR_NAME(found_char), target);

  // Calculate TN modifiers.
  switch (GET_OBJ_TYPE(ware)) {
    case ITEM_BIOWARE:
      if (GET_BIOWARE_IS_CULTURED(ware)) {
        target += 2;
        strlcat(buf, ", cultured so +2", sizeof(buf));
      }
      break;
    case ITEM_CYBERWARE:
      switch (GET_CYBERWARE_GRADE(ware)) {
        case GRADE_ALPHA:
          target += 1;
          strlcat(buf, ", alpha so +1", sizeof(buf));
          break;
        case GRADE_BETA:
          target += 3;
          strlcat(buf, ", beta so +3", sizeof(buf));
          break;
        case GRADE_DELTA:
          target += 6;
          strlcat(buf, ", delta so +6", sizeof(buf));
          break;
      }
      break;
  }

  // Patient is cyber-heavy? +1.
  if (GET_ESS(found_char) < 200) {
    target += 1;
    strlcat(buf, ", low essence so +1", sizeof(buf));
  }

  // Shop is unpacked in an actual room instead of a vehicle? -1 TN.
  if (workshop->in_room) {
    target -= 1;
    strlcat(buf, ", in a room so -1", sizeof(buf));

    // Room is sterile (PGHQ feature)? Another -2.
    if (ROOM_FLAGGED(workshop->in_room, ROOM_STERILE)) {
      target -= 2;
      strlcat(buf, ", room is sterile so -2", sizeof(buf));
    }
  }

  // Shop is a facility? -1 TN.
  if (GET_WORKSHOP_GRADE(workshop) == TYPE_FACILITY) {
    target -= 1;
    strlcat(buf, ", using facility so -1", sizeof(buf));
  }

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ". Total: %d", target);
  act(buf, FALSE, ch, 0, 0, TO_ROLLS);

  // We choose from the lesser of the biotech and medical skills.
  int skill = GET_SKILL(ch, SKILL_BIOTECH) < GET_SKILL(ch, SKILL_MEDICINE) ? SKILL_BIOTECH : SKILL_MEDICINE;
  int dice = get_skill(ch, skill, target);
  snprintf(buf, sizeof(buf), "After get_skill (%s): %d", skills[skill].name, target);
  act(buf, FALSE, ch, 0, 0, TO_ROLLS);
  target += modify_target(ch);
  snprintf(buf, sizeof(buf), "After modify_target: %d", target);
  act(buf, FALSE, ch, 0, 0, TO_ROLLS);
  int successes = success_test(dice, target);
  snprintf(buf, sizeof(buf), "Got %d successes.", successes);
  act(buf, FALSE, ch, 0, 0, TO_ROLLS);

  // Botch!
  if (successes < 0) {
    send_to_char(ch, "You fumble your attempt to %sinstall %s!", mode_is_install ? "" : "un");
    snprintf(buf, sizeof(buf), "$n fumbles $s attempt to %sinstall $o, wounding $N!", mode_is_install ? "" : "un");
    act(buf, FALSE, ch, ware, found_char, TO_ROOM);

    // Damage the character. This damage type does not result in a killer check.
    damage(ch, found_char, SERIOUS, TYPE_MEDICAL_MISHAP, PHYSICAL);

    // Victim obviously doesn't trust you anymore, so revoke permission.
    send_to_char("(System notice: Automatically disabling your ^WTOGGLE CYBERDOC^n permission.)\r\n", found_char);
    PRF_FLAGS(found_char).RemoveBit(PRF_TOUCH_ME_DADDY);

    return TRUE;
  }
  // Failure, no botch.
  else if (successes == 0) {
    send_to_char(ch, "You can't quite figure out how to %sinstall %s.\r\n", mode_is_install ? "" : "un", GET_OBJ_NAME(ware));
    snprintf(buf, sizeof(buf), "$n pokes tentatively at $o, trying to figure out how to %sinstall it.", mode_is_install ? "" : "un");
    act(buf, FALSE, ch, ware, found_char, TO_ROOM);
    return TRUE;
  }
  // Standard success gets no conditional block here (it's the default behavior.)
  // Exceptional success.
  else if (successes >= CYBERDOC_NO_INJURY_DIE_REQUIREMENT) {
    send_to_char("Inspiration strikes, and you set to work with near-miraculous precision.\r\n", ch);
    act("Inspiration strikes, and $n sets to work with near-miraculous precision.", FALSE, ch, NULL, NULL, TO_ROOM);
  }

  // Installation of ware.
  if (mode_is_install) {
    // Perform the installation.
    install_ware_in_target_character(ware, ch, found_char, (successes < CYBERDOC_NO_INJURY_DIE_REQUIREMENT));
  }

  // Removal of ware.
  else {
    // Perform the uninstallation.
    if (uninstall_ware_from_target_character(ware, ch, found_char, (successes < CYBERDOC_NO_INJURY_DIE_REQUIREMENT))) {
      // Give them the ware (only if we succeeded)
      obj_to_char(shop_package_up_ware(ware), ch);
    }
  }

  // Automatically revoke permission.
  send_to_char("(System notice: Automatically disabling your ^WTOGGLE CYBERDOC^n permission.)\r\n", found_char);
  PRF_FLAGS(found_char).RemoveBit(PRF_TOUCH_ME_DADDY);

  return TRUE;
}

/*
SPECIAL(business_card_printer) {
  struct obj_data *printer = (struct obj_data *) me;
  struct obj_data *card = printer->contains;

  // No argument given, no character available, no problem. Skip it.
  if (!*argument || !ch || !printer)
    return FALSE;

  // Only accept our expected commands.
  if (!CMD_IS("set") && !CMD_IS("clear") && !CMD_IS("activate"))
    return FALSE;

  // Require an argument. TODO: Does this break anything in subsequent command handling?
  argument = one_argument(argument, arg);
  if (!*arg || !str_str("printer", arg))
    return FALSE;

  // The SET command configures the printer.
  if (CMD_IS("set")) {
    // Extract the next argument.
    argument = one_argument(argument, arg);
    if (!*arg) {
      send_to_char("Syntax: SET PRINTER [material|quality|quantity|title|description]\r\n", ch);
      return TRUE;
    }

    // Set the material used.
    if (!str_str("material", arg)) {
      asdf
    }
  }

  // The CLEAR command wipes the printer's memory.
  else if (CMD_IS("clear")) {

  }

  // The ACTIVATE command prints the cards.
  else if (CMD_IS("activate")) {

  }

  // SET to configure various options.
  // CLEAR to reset it.
  // ACTIVATE to activate the printing function.
  return FALSE;
}
*/
