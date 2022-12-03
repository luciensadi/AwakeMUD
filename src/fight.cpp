#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <queue>

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "handler.hpp"
#include "interpreter.hpp"
#include "db.hpp"
#include "screen.hpp"
#include "newmagic.hpp"
#include "constants.hpp"
#include "config.hpp"
#include "memory.hpp"
#include "newmatrix.hpp"
#include "newmagic.hpp"
#include "bullet_pants.hpp"
#include "ignore_system.hpp"
#include "invis_resistance_tests.hpp"
#include "playerdoc.hpp"
#include "sound_propagation.hpp"

/* Structures */
struct char_data *combat_list = NULL;   /* head of l-list of fighting chars */
struct char_data *next_combat_list = NULL;

/* External structures */
extern struct message_list fight_messages[MAX_MESSAGES];

extern const char *KILLER_FLAG_MESSAGE;

bool does_weapon_have_bayonet(struct obj_data *weapon);

int find_sight(struct char_data *ch);
void damage_door(struct char_data *ch, struct room_data *room, int dir, int power, int type);
void damage_obj(struct char_data *ch, struct obj_data *obj, int power, int type);
bool mount_fire(struct char_data *ch);
/* External procedures */
char *fread_action(FILE * fl, int nr);
char *fread_string(FILE * fl, char *error);
void stop_follower(struct char_data * ch);
ACMD_DECLARE(do_assist);
ACMD_CONST(do_flee);
ACMD_DECLARE(do_action);
bool docwagon(struct char_data *ch);
void roll_individual_initiative(struct char_data *ch);
bool ranged_response(struct char_data *ch, struct char_data *vict);
int find_weapon_range(struct char_data *ch, struct obj_data *weapon);
void weapon_scatter(struct char_data *ch, struct char_data *victim,
                    struct obj_data *weapon);
void explode_explosive_grenade(struct char_data *ch, struct obj_data *weapon, struct room_data *room);
void explode_flashbang_grenade(struct char_data *ch, struct obj_data *weapon, struct room_data *room);
void target_explode(struct char_data *ch, struct obj_data *weapon,
                    struct room_data *room, int mode);
void forget(struct char_data * ch, struct char_data * victim);
void remember(struct char_data * ch, struct char_data * victim);
void order_list(bool first,...);
bool can_hurt(struct char_data *ch, struct char_data *victim, int attacktype, bool include_func_protections);

bool damage(struct char_data *ch, struct char_data *victim, int dam, int attacktype, bool is_physical);
bool damage_without_message(struct char_data *ch, struct char_data *victim, int dam, int attacktype, bool is_physical);
bool raw_damage(struct char_data *ch, struct char_data *victim, int dam, int attacktype, bool is_physical, bool send_message);
void docwagon_retrieve(struct char_data *ch);

SPECIAL(weapon_dominator);
SPECIAL(pocket_sec);
WSPEC(monowhip);

extern int resisted_test(int num_for_ch, int tar_for_ch, int num_for_vict,
                         int tar_for_vict);
extern int modify_target(struct char_data *ch);
extern int skill_web (struct char_data *ch, int skillnumber);
extern bool attempt_reload(struct char_data *mob, int pos);
extern void switch_weapons(struct char_data *mob, int pos);
extern void hunt_victim(struct char_data * ch);
extern void matrix_fight(struct char_data *ch, struct char_data *victim);
extern void check_quest_kill(struct char_data *ch, struct char_data *victim);
extern void check_quest_destroy(struct char_data *ch, struct obj_data *obj);
extern int return_general(int);
extern struct zone_data *zone_table;
extern void perform_tell(struct char_data *, struct char_data *, char *);
extern int can_wield_both(struct char_data *, struct obj_data *, struct obj_data *);
extern void find_and_draw_weapon(struct char_data *);
extern void crash_test(struct char_data *ch);
extern int get_vehicle_modifier(struct veh_data *veh);
extern bool mob_magic(struct char_data *ch);
extern void cast_spell(struct char_data *ch, int spell, int sub, int force, char *arg);
extern char *get_player_name(vnum_t id);
extern bool mob_is_aggressive(struct char_data *ch, bool include_base_aggression);

// Corpse saving externs.
extern void House_save(struct house_control_rec *house, const char *file_name, long rnum);
extern void write_world_to_disk(int vnum);
extern bool Storage_get_filename(vnum_t vnum, char *filename, int filename_size);
extern bool House_get_filename(vnum_t vnum, char *filename, int filename_size);

extern bool item_should_be_treated_as_melee_weapon(struct obj_data *obj);
extern bool item_should_be_treated_as_ranged_weapon(struct obj_data *obj);

extern struct obj_data *generate_ammobox_from_pockets(struct char_data *ch, int weapontype, int ammotype, int quantity);
extern void send_mob_aggression_warnings(struct char_data *pc, struct char_data *mob);
extern bool hit_with_multiweapon_toggle(struct char_data *attacker, struct char_data *victim, struct obj_data *weap, struct obj_data *vict_weap, struct obj_data *weap_ammo, bool multi_weapon_modifier);

extern void mobact_change_firemode(struct char_data *ch);
extern bool dumpshock(struct matrix_icon *icon);

/* Weapon attack texts */
struct attack_hit_type attack_hit_text[] =
{
  {"hit", "hits", "hit"
  }
  ,               /* 0 */
  {"sting", "stings", "sting"},
  {"whip", "whips", "whip"},
  {"slash", "slashes", "slash"},
  {"bite", "bites", "bite"},
  {"bludgeon", "bludgeons", "bludgeon"},        /* 5 */
  {"crush", "crushes", "crush"},
  {"pound", "pounds", "pound"},
  {"claw", "claws", "claw"},
  {"maul", "mauls", "maul"},
  {"gore", "gores", "gore"},     /* 10 */
  {"pierce", "pierces", "pierce"},
  {"punch", "punches", "punch"},
  {"stab", "stabs", "stab"},
  {"shock", "shocks", "shock"},
  {"shuriken", "shurikens", "shuriken"}, /* 15 */
  {"pierce", "pierces", "pierce"},
  {"pierce", "pierces", "pierce"},
  {"grenade", "grenades", "grenade"},
  {"grenade", "grenades", "grenade"},
  {"rocket", "rockets", "rocket"},  /* 20 */
  {"shoot", "shoots", "shot"},
  {"blast", "blasts", "blast"},
  {"shoot", "shoots", "shot"},
  {"blast", "blasts", "blast"},
  {"blast", "blasts", "burst fire"},    /* 25 */
  {"blast", "blasts", "blast"},
  {"bifurcate", "bifurcates", "BIFURCATION"}
};

/* The Fight related routines */

void appear(struct char_data * ch)
{
  AFF_FLAGS(ch).RemoveBits(AFF_IMP_INVIS,
                           AFF_INVISIBLE,
                           AFF_HIDE, ENDBIT);

  // TODO: Go through all sustained spells in the game and remove those that are causing this character to be invisible.

  if (!IS_SENATOR(ch))
    act("$n slowly fades into existence.", FALSE, ch, 0, 0, TO_ROOM);
  else
    act("You feel a strange presence as $n appears, seemingly from nowhere.",
        FALSE, ch, 0, 0, TO_ROOM);
}

void load_messages(void)
{
  FILE *fl;
  int i, type;
  struct message_type *messages;
  char chk[128];

  if (!(fl = fopen(MESS_FILE, "r"))) {
    snprintf(buf2, sizeof(buf2), "Error reading combat message file %s", MESS_FILE);
    perror(buf2);
    shutdown();
  }
  for (i = 0; i < MAX_MESSAGES; i++) {
    fight_messages[i].a_type = 0;
    fight_messages[i].number_of_attacks = 0;
    fight_messages[i].msg = 0;
  }

  fgets(chk, 128, fl);
  while (!feof(fl) && (*chk == '\n' || *chk == '*'))
    fgets(chk, 128, fl);

  while (*chk == 'M') {
    fgets(chk, 128, fl);
    sscanf(chk, " %d\n", &type);
    for (i = 0; (i < MAX_MESSAGES) && (fight_messages[i].a_type != type) &&
         (fight_messages[i].a_type); i++)
      ;
    if (i >= MAX_MESSAGES) {
      fprintf(stderr, "Too many combat messages.  Increase MAX_MESSAGES and recompile.");
      shutdown();
    }
    messages = new message_type;
    fight_messages[i].number_of_attacks++;
    fight_messages[i].a_type = type;
    messages->next = fight_messages[i].msg;
    fight_messages[i].msg = messages;

    messages->die_msg.attacker_msg = fread_action(fl, i);
    messages->die_msg.victim_msg = fread_action(fl, i);
    messages->die_msg.room_msg = fread_action(fl, i);
    messages->miss_msg.attacker_msg = fread_action(fl, i);
    messages->miss_msg.victim_msg = fread_action(fl, i);
    messages->miss_msg.room_msg = fread_action(fl, i);
    messages->hit_msg.attacker_msg = fread_action(fl, i);
    messages->hit_msg.victim_msg = fread_action(fl, i);
    messages->hit_msg.room_msg = fread_action(fl, i);
    messages->god_msg.attacker_msg = fread_action(fl, i);
    messages->god_msg.victim_msg = fread_action(fl, i);
    messages->god_msg.room_msg = fread_action(fl, i);
    fgets(chk, 128, fl);
    while (!feof(fl) && (*chk == '\n' || *chk == '*'))
      fgets(chk, 128, fl);
  }

  fclose(fl);
}

#define MAKE_MORTALLY_WOUNDED(victim) {GET_POS(victim) = POS_MORTALLYW; AFF_FLAGS(victim).RemoveBit(AFF_PRONE);}
bool update_pos(struct char_data * victim, bool protect_spells_from_purge)
{
  bool was_morted = GET_POS(victim) == POS_MORTALLYW;
  int min_body = -GET_BOD(victim) + (GET_BIOOVER(victim) > 0 ? GET_BIOOVER(victim) : 0);

  // Are they stunned?
  if ((GET_MENTAL(victim) < 100) && (GET_PHYSICAL(victim) >= 100)) {
    if (!IS_NPC(victim)) {
      if (was_morted && !PRF_FLAGGED(victim, PRF_DONT_ALERT_PLAYER_DOCTORS_ON_MORT)) {
        alert_player_doctors_of_contract_withdrawal(victim, FALSE);
      }
      PLR_FLAGS(victim).RemoveBit(PLR_SENT_DOCWAGON_PLAYER_ALERT);
    }

    // Pain editor prevents stunned condition.
    for (struct obj_data *bio = victim->bioware; bio; bio = bio->next_content)
      if (GET_BIOWARE_TYPE(bio) == BIO_PAINEDITOR && GET_BIOWARE_IS_ACTIVATED(bio))
        return FALSE;
    GET_POS(victim) = POS_STUNNED;
    GET_INIT_ROLL(victim) = 0;
  }

  // Are they doing fine?
  else if ((GET_PHYSICAL(victim) >= 100) && (GET_POS(victim) > POS_STUNNED)) {
    return FALSE;
  }

  // They were potentially morted and now are healed.
  else if (GET_PHYSICAL(victim) >= 100) {
    if (!IS_NPC(victim)) {
      if (was_morted && !PRF_FLAGGED(victim, PRF_DONT_ALERT_PLAYER_DOCTORS_ON_MORT)) {
        alert_player_doctors_of_contract_withdrawal(victim, FALSE);
      }
      PLR_FLAGS(victim).RemoveBit(PLR_SENT_DOCWAGON_PLAYER_ALERT);
    }

    GET_POS(victim) = POS_STANDING;
    return FALSE;
  }

  // They died from their injuries.
  else if ((int)(GET_PHYSICAL(victim) / 100) <= min_body) {
    if (!IS_NPC(victim) && (--GET_PC_SALVATION_TICKS(victim) > 0)) {
      send_to_char("^yYou struggle to keep a grip on life!^n\r\n", victim);
      GET_PHYSICAL(victim) = (min_body + 1) * 100;
      MAKE_MORTALLY_WOUNDED(victim);
    } else {
      GET_POS(victim) = POS_DEAD;
    }
  }

  // They're mortally wounded but not dead.
  else {
    // They were just morted. Give them some hits of damage invulnerability to let them last longer.
    if (!was_morted) {
      GET_PC_SALVATION_TICKS(victim) = 5;
    }
    MAKE_MORTALLY_WOUNDED(victim);
  }

  // To get here, you're stunned or morted.

  GET_INIT_ROLL(victim) = 0;

  // SR3 p178
  if (GET_POS(victim) <= POS_SLEEPING) {
    if (!protect_spells_from_purge) {
      struct sustain_data *next;
      for (struct sustain_data *sust = GET_SUSTAINED(victim); sust; sust = next) {
        next = sust->next;
        if (sust->caster && !sust->focus && !sust->spirit)
          end_sustained_spell(victim, sust);
      }
    }

    char cmd_buf[100];
    *cmd_buf = '\0';

    if (PLR_FLAGGED(victim, PLR_REMOTE) || AFF_FLAGGED(victim, AFF_RIG)) {
      ACMD_DECLARE(do_return);
      do_return(victim, cmd_buf, 0, 0);
    }

    if (GET_POS(victim) > POS_DEAD && PLR_FLAGGED(victim, PLR_MATRIX) && victim->persona) {
      return dumpshock(victim->persona);
    }
  }

  return FALSE;
}
#undef MAKE_MORTALLY_WOUNDED

/* blood blood blood, root */
void increase_blood(struct room_data *rm)
{
  rm->blood = MIN(rm->blood + 1, 10);
}

void increase_debris(struct room_data *rm)
{
  rm->debris= MIN(rm->debris + 1, 50);
}


void check_killer(struct char_data * ch, struct char_data * vict)
{
  char_data *attacker;
  char buf[256];

  if (IS_NPC(ch) && (ch->desc == NULL || ch->desc->original == NULL))
    return;

  if (!IS_NPC(ch))
    attacker = ch;
  else
    attacker = ch->desc->original;

  if (!IS_NPC(vict) &&
      !PLR_FLAGS(vict).AreAnySet(PLR_KILLER, ENDBIT) &&
      !(ROOM_FLAGGED(ch->in_room, ROOM_ARENA) && ROOM_FLAGGED(vict->in_room, ROOM_ARENA)) &&
      (!PRF_FLAGGED(attacker, PRF_PKER) || !PRF_FLAGGED(vict, PRF_PKER)) &&
      !PLR_FLAGGED(attacker, PLR_KILLER) && attacker != vict && !IS_SENATOR(attacker))
  {
    PLR_FLAGS(attacker).SetBit(PLR_KILLER);

    snprintf(buf, sizeof(buf), "PC Killer bit set on %s for initiating attack on %s at %s.",
            GET_CHAR_NAME(attacker),
            GET_CHAR_NAME(vict), get_ch_in_room(vict)->name);
    mudlog(buf, ch, LOG_MISCLOG, TRUE);

    send_to_char(KILLER_FLAG_MESSAGE, ch);
  }
}

/* start one char fighting another (yes, it is horrible, I know... )  */
void set_fighting(struct char_data * ch, struct char_data * vict, ...)
{
  struct follow_type *k;
  struct char_data * combat_list_head = NULL;
  
  if (!ch || !vict || ch == vict)
    return;

  if (IS_NPC(ch)) {
    // Prevents you from surprising someone who's attacking you already.
    GET_MOBALERTTIME(ch) = MAX(GET_MOBALERTTIME(ch), 20);
    GET_MOBALERT(ch) = MALERT_ALERT;
  }

  if (CH_IN_COMBAT(ch))
    return;

#ifdef IGNORING_IC_ALSO_IGNORES_COMBAT
  char warnbuf[5000];
  if (IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
    snprintf(warnbuf, sizeof(warnbuf), "WARNING: Entered set_fighting with vict %s ignoring attacker %s!", GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
    mudlog(warnbuf, ch, LOG_SYSLOG, TRUE);
    return;
  }

  if (IS_IGNORING(ch, is_blocking_ic_interaction_from, vict)) {
    snprintf(warnbuf, sizeof(warnbuf), "WARNING: Entered set_fighting with attacker %s ignoring victim %s!", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict));
    mudlog(warnbuf, ch, LOG_SYSLOG, TRUE);
    return;
  }
#endif

  // Check to see if they're already in the combat list.
  bool already_there = FALSE;
  for (struct char_data *tmp = combat_list; tmp; tmp = tmp->next_fighting) {
    if (tmp == ch) {
      mudlog("SYSERR: Attempted to re-add character to combat list!", ch, LOG_SYSLOG, TRUE);
      already_there = TRUE;
    }
  }
  if (!already_there) {
    if (!combat_list) {
      combat_list = ch;
    } else {
      // Global re-roll happens when the head of the list reaches 0 init. In order to prevent new
      // combantants from arbitrarily delaying the next global re-roll, we want to hang on to the
      // original head.
      combat_list_head = combat_list;
      ch->next_fighting = combat_list->next_fighting;
      combat_list = ch;
  }

  // We set fighting before we call roll_individual_initiative() because we need the fighting target there.
  FIGHTING(ch) = vict;
  GET_POS(ch) = POS_FIGHTING;

  roll_individual_initiative(ch);
  order_list(TRUE);
  
  // Put back the original combat list head.
  if (combat_list_head) {
    combat_list_head->next_fighting = combat_list;
    combat_list = combat_list_head;
  }

  if (!(AFF_FLAGGED(ch, AFF_MANNING) || PLR_FLAGGED(ch, PLR_REMOTE) || AFF_FLAGGED(ch, AFF_RIG)))
  {
    if (!(GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD)))
      find_and_draw_weapon(ch);

    if (item_should_be_treated_as_melee_weapon(GET_EQ(ch, WEAR_WIELD)) && item_should_be_treated_as_melee_weapon(GET_EQ(ch, WEAR_HOLD)))
      AFF_FLAGS(ch).SetBit(AFF_APPROACH);

    else if (!GET_EQ(ch, WEAR_WIELD) && !GET_EQ(ch, WEAR_HOLD))
      AFF_FLAGS(ch).SetBit(AFF_APPROACH);
  }

  check_killer(ch, vict);
  AFF_FLAGS(ch).RemoveBit(AFF_BANISH);
  AFF_FLAGS(vict).RemoveBit(AFF_BANISH);

  strcpy(buf, GET_CHAR_NAME(ch));

  if (!ch->in_veh) {
    if (ch->followers)
      for (k = ch->followers; k; k = k->next)
        if (PRF_FLAGGED(k->follower, PRF_ASSIST) && k->follower->in_room == ch->in_room && !k->follower->in_veh &&
            !FIGHTING(k->follower) && AWAKE(k->follower))
          do_assist(k->follower, buf, 0, 0);
    if (ch->master)
    {
      if (PRF_FLAGGED(ch->master, PRF_ASSIST) && ch->master->in_room == ch->in_room && !ch->master->in_veh &&
          !FIGHTING(ch->master) && AWAKE(ch->master))
        do_assist(ch->master, buf, 0, 0);
      for (k = ch->master->followers; k; k = k->next)
        if (PRF_FLAGGED(k->follower, PRF_ASSIST) && k->follower->in_room == ch->in_room && !k->follower->in_veh
            && k->follower != ch && !FIGHTING(k->follower) && AWAKE(k->follower))
          do_assist(k->follower, buf, 0, 0);
    }
  }
}

void set_fighting(struct char_data * ch, struct veh_data * vict)
{
  struct follow_type *k;
  struct char_data * combat_list_head = NULL;

  if (!ch || !vict)
    return;

  if (CH_IN_COMBAT(ch))
    return;

  // Check to see if they're already in the combat list.
  bool already_there = FALSE;
  for (struct char_data *tmp = combat_list; tmp; tmp = tmp->next_fighting) {
    if (tmp == ch) {
      mudlog("SYSERR: Attempted to re-add character to combat list!", ch, LOG_SYSLOG, TRUE);
      already_there = TRUE;
    }
  }
  if (!already_there) {
    if (!combat_list) {
      combat_list = ch;
    } else {
      // Global re-roll happens when the head of the list reaches 0 init. In order to prevent new
      // combantants from arbitrarily delaying the next global re-roll, we want to hang on to the
      // original head.
      combat_list_head = combat_list;
      ch->next_fighting = combat_list->next_fighting;
      combat_list = ch;
  }

  FIGHTING_VEH(ch) = vict;
  GET_POS(ch) = POS_FIGHTING;

  roll_individual_initiative(ch);
  order_list(TRUE);
  
  // Put back the original combat list head.
  if (combat_list_head) {
    combat_list_head->next_fighting = combat_list;
    combat_list = combat_list_head;
  }
  
  if (!(GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD)))
    find_and_draw_weapon(ch);

  if (IS_NPC(ch))
    strcpy(buf, GET_NAME(ch));
  else
    strcpy(buf, GET_CHAR_NAME(ch));
  if (ch->followers)
    for (k = ch->followers; k; k = k->next)
      if (PRF_FLAGGED(k->follower, PRF_ASSIST) && k->follower->in_room == ch->in_room &&
          !FIGHTING(k->follower) && AWAKE(k->follower))
        do_assist(k->follower, buf, 0, 0);
  if (ch->master)
  {
    if (PRF_FLAGGED(ch->master, PRF_ASSIST) && ch->master->in_room == ch->in_room &&
        !FIGHTING(ch->master) && AWAKE(ch->master))
      do_assist(ch->master, buf, 0, 0);
    for (k = ch->master->followers; k; k = k->next)
      if (PRF_FLAGGED(k->follower, PRF_ASSIST) && k->follower->in_room == ch->in_room
          && k->follower != ch && !FIGHTING(k->follower) && AWAKE(k->follower))
        do_assist(k->follower, buf, 0, 0);
  }
}

/* remove a char from the list of fighting chars */
void stop_fighting(struct char_data * ch)
{
  struct char_data *temp;
  if (ch == next_combat_list)
    next_combat_list = ch->next_fighting;

  if (IS_AFFECTED(ch, AFF_APPROACH))
    AFF_FLAGS(ch).RemoveBit(AFF_APPROACH);

  REMOVE_FROM_LIST(ch, combat_list, next_fighting);
  FIGHTING(ch) = NULL;
  FIGHTING_VEH(ch) = NULL;
  ch->next_fighting = NULL;

  if (ch->in_veh) {
    // If they're manning, detarget their mount. TODO: What about rigger mounts?
    if (IS_AFFECTED(ch, AFF_MANNING)) {
      struct obj_data *mount = get_mount_manned_by_ch(ch);
      mount->targ = NULL;
      mount->tveh = NULL;
    }
    GET_POS(ch) = POS_SITTING;
  }
  else
    GET_POS(ch) = POS_STANDING;


  GET_INIT_ROLL(ch) = 0;
  SHOOTING_DIR(ch) = NOWHERE;

  // If they're mob who cast confusion/chaos on someone, drop the spell.
  if (IS_NPC(ch) && GET_SUSTAINED(ch)) {
    struct sustain_data *next;
    for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = next) {
      next = sust->next;
      // Removed checks for focus and spirit sustains. It does say 'all'...
      if (sust->caster && (sust->spell == SPELL_CONFUSION || sust->spell == SPELL_CHAOS))
        end_sustained_spell(ch, sust);
    }
  }

  update_pos(ch);
}

void make_corpse(struct char_data * ch)
{
  struct obj_data *create_nuyen(int amount);
  struct obj_data *create_credstick(struct char_data *ch, int amount);

  struct obj_data *corpse;
  struct obj_data *money;
  int i, nuyen, credits, corpse_value = 0;

  if (PLR_FLAGGED(ch, PLR_NEWBIE)) {
    mudlog("Won't generate a corpse-- still a newbie.", ch, LOG_DEATHLOG, TRUE);
    return;
  }

  corpse = create_obj();

  corpse->item_number = NOTHING;
  corpse->in_room = NULL;

  char color_replaced_name[500];
  {
    strlcpy(color_replaced_name, decapitalize_a_an(GET_NAME(ch)), sizeof(color_replaced_name));
    bool just_saw_carat = FALSE;
    for (char *ptr = color_replaced_name; *ptr; ptr++) {
      if (just_saw_carat && (*ptr == 'n' || *ptr == 'N'))
        *ptr = 'r';
      just_saw_carat = (*ptr == '^');
    }
  }

  if (IS_NPC(ch))
  {
    if (MOB_FLAGGED(ch, MOB_INANIMATE)) {
      snprintf(buf, sizeof(buf), "remains corpse %s", ch->player.physical_text.keywords);
      snprintf(buf1, sizeof(buf1), "^rThe remains of %s are lying here.^n", color_replaced_name);
      snprintf(buf2, sizeof(buf2), "^rthe remains of %s^n", color_replaced_name);
      strlcpy(buf3, "It's been powered down permanently.\r\n", sizeof(buf3));
    } else {
      snprintf(buf, sizeof(buf), "corpse %s", ch->player.physical_text.keywords);
      snprintf(buf1, sizeof(buf1), "^rThe corpse of %s is lying here.^n", color_replaced_name);
      snprintf(buf2, sizeof(buf2), "^rthe corpse of %s^n", color_replaced_name);
      strlcpy(buf3, "What once was living is no longer. Poor sap.\r\n", sizeof(buf3));
    }
  } else
  {
    snprintf(buf, sizeof(buf), "belongings %s", ch->player.physical_text.keywords);
    snprintf(buf1, sizeof(buf1), "^rThe belongings of %s are lying here.^n", color_replaced_name);
    snprintf(buf2, sizeof(buf2), "^rthe belongings of %s^n", color_replaced_name);
    strlcpy(buf3, "Looks like the DocWagon trauma team wasn't able to bring this stuff along.\r\n", sizeof(buf3));
    corpse->item_number = real_object(OBJ_SPECIAL_PC_CORPSE);
  }
  corpse->text.keywords = str_dup(buf);
  corpse->text.room_desc = str_dup(buf1);
  corpse->text.name = str_dup(buf2);
  corpse->text.look_desc = str_dup(buf3);
  GET_OBJ_TYPE(corpse) = ITEM_CONTAINER;
  // (corpse)->obj_flags.wear_flags.SetBits(ITEM_WEAR_TAKE, ENDBIT);
  (corpse)->obj_flags.extra_flags.SetBits(ITEM_EXTRA_NODONATE, // ITEM_NORENT,
                                          ITEM_EXTRA_CORPSE, ENDBIT);

  GET_OBJ_VAL(corpse, 0) = 0;   /* You can't store stuff in a corpse */
  GET_OBJ_VAL(corpse, 4) = 1;   /* corpse identifier */
  GET_OBJ_WEIGHT(corpse) = MAX(100, GET_WEIGHT(ch)) + IS_CARRYING_W(ch);
  if (IS_NPC(ch))
  {
    GET_OBJ_TIMER(corpse) = max_npc_corpse_time;
    GET_OBJ_VAL(corpse, 4) = 0;
    GET_OBJ_VAL(corpse, 5) = ch->nr;
  } else
  {
    GET_OBJ_TIMER(corpse) = max_pc_corpse_time;
    GET_OBJ_VAL(corpse, 4) = 1;
    GET_OBJ_VAL(corpse, 5) = GET_IDNUM(ch);
    /* make 'em bullet proof...(anti-twink measure) */
    GET_OBJ_BARRIER(corpse) = PC_CORPSE_BARRIER;

    // Drain their pockets of ammo and put it on the corpse.
    for (int wp = START_OF_AMMO_USING_WEAPONS; wp <= END_OF_AMMO_USING_WEAPONS; wp++)
      for (int am = AMMO_NORMAL; am < NUM_AMMOTYPES; am++) {
        struct obj_data *o = NULL;
        if (GET_BULLETPANTS_AMMO_AMOUNT(ch, wp, am) > 0
            && (o = generate_ammobox_from_pockets(ch, wp, am, GET_BULLETPANTS_AMMO_AMOUNT(ch, wp, am)))) {
          obj_to_obj(o, corpse);
        }
      }
  }

  /* transfer character's inventory to the corpse */
  {
    struct obj_data *obj = NULL;
    for (struct obj_data *o = ch->carrying; o; o = obj)
    {
      obj = o->next_content;
      obj_from_char(o);
      // Skip empty magazines.
      if (GET_OBJ_TYPE(o) == ITEM_GUN_MAGAZINE && !GET_OBJ_VAL(o, 1))
        extract_obj(o);
      // Skip gowns.
      else if (IS_OBJ_STAT(o, ITEM_EXTRA_PURGE_ON_DEATH))
        extract_obj(o);
      else {
        obj_to_obj(o, corpse);
        corpse_value += GET_OBJ_COST( o );
      }
    }
  }

  /* transfer character's equipment to the corpse */
  for (i = 0; i < NUM_WEARS; i++)
  {
    if (ch->equipment[i]) {
      if (IS_OBJ_STAT(ch->equipment[i], ITEM_EXTRA_PURGE_ON_DEATH)) {
        extract_obj(unequip_char(ch, i, TRUE));
      } else {
        corpse_value += GET_OBJ_COST( ch->equipment[i] );
        obj_to_obj(unequip_char(ch, i, TRUE), corpse);
      }
    }
  }

  /* transfer nuyen & credstick */
  if (IS_NPC(ch))
  {
    if (MOB_FLAGGED(ch, MOB_NO_NUYEN_LOOT_DROPS) || vnum_from_non_connected_zone(GET_MOB_VNUM(ch))) {
      nuyen = 0;
      credits = 0;
    } else {
      nuyen = (int)(GET_NUYEN(ch) / 10);
      nuyen = number(GET_NUYEN(ch) - nuyen, GET_NUYEN(ch) + nuyen) * NUYEN_GAIN_MULTIPLIER;
      credits = (int)(GET_BANK(ch) / 10);
      credits = number(GET_BANK(ch) - credits, GET_BANK(ch) + credits) * NUYEN_GAIN_MULTIPLIER;
    }
  } else
  {
#ifdef DIES_IRAE
    lose_nuyen(ch, MAX(0, (int) (GET_NUYEN(ch) / DEATH_NUYEN_LOSS_DIVISOR)), NUYEN_OUTFLOW_DEATH_PENALTY);
#endif
    nuyen = GET_NUYEN(ch);
    credits = 0;
  }

  if (nuyen > 0)
  {
    money = create_nuyen(nuyen);
    obj_to_obj(money, corpse);
    GET_NUYEN_RAW(ch) = 0;
  }

  if (credits > 0 && !number(0, CREDSTICK_RARITY_FACTOR - 1))
  {
    money = create_credstick(ch, credits * CREDSTICK_RARITY_FACTOR);
    obj_to_obj(money, corpse);
  }

  if ( IS_NPC(ch) )
  {
    extern struct char_data *mob_proto;
    mob_proto[GET_MOB_RNUM(ch)].mob_specials.value_death_nuyen += credits + nuyen;
    mob_proto[GET_MOB_RNUM(ch)].mob_specials.value_death_items += corpse_value;
  } else {
    // Log it so we can help people recover their stuff.
    const char *need_delete = generate_new_loggable_representation(corpse);
    snprintf(buf, sizeof(buf), "Corpse generated for %s: %s", GET_CHAR_NAME(ch), need_delete);
    delete [] need_delete;
    mudlog(buf, ch, LOG_GRIDLOG, TRUE);
  }

  ch->carrying = NULL;
  IS_CARRYING_N(ch) = 0;
  IS_CARRYING_W(ch) = 0;

  // Empty player belongings? Pointless.
  if (GET_OBJ_VAL(corpse, 4) == 1 && !corpse->contains) {
    extract_obj(corpse);
    return;
  }

  if (ch->in_veh) {
    obj_to_veh(corpse, ch->in_veh);
    corpse->vfront = ch->vfront;
  } else {
    obj_to_room(corpse, ch->in_room);

    if (GET_OBJ_VNUM(corpse) == OBJ_SPECIAL_PC_CORPSE) {
      char filename[500];
      filename[0] = 0;

      if (!ROOM_FLAGGED(ch->in_room, ROOM_STORAGE) && !ROOM_FLAGGED(ch->in_room, ROOM_HOUSE)) {
        snprintf(buf, sizeof(buf), "Setting storage flag for %s (%ld) due to player corpse being in it.",
                 GET_ROOM_NAME(ch->in_room),
                 GET_ROOM_VNUM(ch->in_room));
        mudlog(buf, NULL, LOG_SYSLOG, TRUE);

        ch->in_room->room_flags.SetBit(ROOM_CORPSE_SAVE_HACK);
        ch->in_room->room_flags.SetBit(ROOM_STORAGE);

        // Save the new flag.
        for (int counter = 0; counter <= top_of_zone_table; counter++) {
          if ((GET_ROOM_VNUM(ch->in_room) >= (zone_table[counter].number * 100))
              && (GET_ROOM_VNUM(ch->in_room) <= (zone_table[counter].top)))
          {
            write_world_to_disk(zone_table[counter].number);
            break;
          }
        }
      }

      if (ROOM_FLAGGED(ch->in_room, ROOM_STORAGE)) {
        if (!Storage_get_filename(GET_ROOM_VNUM(ch->in_room), filename, sizeof(filename))) {
          mudlog("WARNING: Failed to make room into a save room for corpse - no filename!!", ch, LOG_SYSLOG, TRUE);
          send_to_char("WARNING: Due to an error, your corpse will not be saved on copyover or crash! Prioritize retrieving it!\r\n", ch);
          return;
        }
      } else if (ROOM_FLAGGED(ch->in_room, ROOM_HOUSE)) {
        if (!House_get_filename(GET_ROOM_VNUM(ch->in_room), filename, sizeof(filename))) {
          mudlog("WARNING: Failed to make room into a save room for corpse - no filename!!", ch, LOG_SYSLOG, TRUE);
          send_to_char("WARNING: Due to an error, your corpse will not be saved on copyover or crash! Prioritize retrieving it!\r\n", ch);
          return;
        }
      } else {
        mudlog("WARNING: Failed to make room into a save room for corpse - flag not set!!", ch, LOG_SYSLOG, TRUE);
        send_to_char("WARNING: Due to an error, your corpse will not be saved on copyover or crash! Prioritize retrieving it!\r\n", ch);
        return;
      }

      House_save(NULL, filename, real_room(GET_ROOM_VNUM(ch->in_room)));
    }
  }
}

void death_cry(struct char_data * ch)
{
  bool should_make_mechanical_noise_instead_of_scream = IS_NPC(ch) && MOB_FLAGGED(ch, MOB_INANIMATE);

  if (should_make_mechanical_noise_instead_of_scream) {
    act("A horrible grinding buzz reverberates from $n as it shuts down!", FALSE, ch, 0, 0, TO_ROOM);
  } else {
    snprintf(buf3, sizeof(buf3), "$n cries out $s last breath as $e die%s!", HSSH_SHOULD_PLURAL(ch) ? "s" : "");
    act(buf3, FALSE, ch, 0, 0, TO_ROOM);
  }

  if (ch->in_veh) {
    snprintf(buf3, sizeof(buf3), "A %s comes from within %s!\r\n",
             should_make_mechanical_noise_instead_of_scream ? "horrible grinding buzz" : "cry of agony",
             GET_VEH_NAME(ch->in_veh));
    send_to_room(buf3, get_ch_in_room(ch));
    return;
  }

  for (struct char_data *listener = get_ch_in_room(ch)->people; listener; listener = listener->next_in_room) {
    if (IS_NPC(listener)) {
      GET_MOBALERTTIME(listener) = 30;
      GET_MOBALERT(listener) = MALERT_ALARM;
    }
  }

  struct room_data *was_in = ch->in_room;
  for (int door = 0; door < NUM_OF_DIRS; door++)
  {
    if (CAN_GO(ch, door)) {
      ch->in_room = was_in->dir_option[door]->to_room;
      if (should_make_mechanical_noise_instead_of_scream) {
        act("Somewhere close, you hear a horrible grinding buzz!", FALSE, ch, 0, 0, TO_ROOM);
      } else {
        act("Somewhere close, you hear someone's death cry!", FALSE, ch, 0, 0, TO_ROOM);
      }
      // This specific use of in_room is okay since it's set above and guaranteed to exist.
      for (struct char_data *listener = ch->in_room->people; listener; listener = listener->next_in_room)
        if (IS_NPC(listener)) {
          GET_MOBALERTTIME(listener) = 30;
          GET_MOBALERT(listener) = MALERT_ALERT;
        }
      ch->in_room = was_in;
    }
  }
}

void raw_kill(struct char_data * ch)
{
  struct obj_data *bio, *obj, *o;
  long i;

  if (CH_IN_COMBAT(ch))
    stop_fighting(ch);

  if (IS_PC_CONJURED_ELEMENTAL(ch) && GET_ACTIVE(ch))
  {
    for (struct descriptor_data *d = descriptor_list; d; d = d->next)
      if (d->character && GET_IDNUM(d->character) == GET_ACTIVE(ch)) {
        for (struct spirit_data *spirit = GET_SPIRIT(d->character); spirit; spirit = spirit->next)
          if (spirit->id == GET_GRADE(ch)) {
            spirit->called = FALSE;
          }
        send_to_char(d->character, "%s is disrupted and returns to the metaplanes to heal.\r\n", CAP(GET_NAME(ch)));
        GET_ELEMENTALS_DIRTY_BIT(d->character) = TRUE;
        break;
      }
  }
  if (IS_ASTRAL(ch))
  {
    act("$n vanishes.", FALSE, ch, 0, 0, TO_ROOM);
    for (i = 0; i < NUM_WEARS; i++)
      if (GET_EQ(ch, i))
        extract_obj(GET_EQ(ch, i));
    for (obj = ch->carrying; obj; obj = o) {
      o = obj->next_content;
      extract_obj(obj);
    }
  } else
  {
    struct room_data *in_room = get_ch_in_room(ch);

    death_cry(ch);

    if (!(IS_SPIRIT(ch) || IS_ANY_ELEMENTAL(ch)))
      make_corpse(ch);

    if (!IS_NPC(ch)) {
      for (bio = ch->bioware; bio; bio = bio->next_content) {
        switch (GET_BIOWARE_TYPE(bio)) {
          case BIO_ADRENALPUMP:
            if (GET_OBJ_VAL(bio, 5) > 0) {
              for (i = 0; i < MAX_OBJ_AFFECT; i++)
                affect_modify(ch,
                              bio->affected[i].location,
                              bio->affected[i].modifier,
                              bio->obj_flags.bitvector, FALSE);
              GET_OBJ_VAL(bio, 5) = 0;
            }
            break;
          case BIO_PAINEDITOR:
            GET_BIOWARE_IS_ACTIVATED(bio) = 0;
            break;
        }
      }

      reset_all_drugs_for_char(ch);

      GET_COND(ch, COND_DRUNK) = 0;

      if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
        i = real_room(RM_CHARGEN_START_ROOM);
      } else {
        switch (GET_JURISDICTION(in_room)) {
          case ZONE_SEATTLE:
            i = real_room(RM_SEATTLE_DOCWAGON);
            break;
          case ZONE_PORTLAND:
            i = real_room(RM_PORTLAND_DOCWAGON);
            break;
          case ZONE_CARIB:
            i = real_room(RM_CARIB_DOCWAGON);
            break;
          case ZONE_OCEAN:
            i = real_room(RM_OCEAN_DOCWAGON);
            break;
          default:
            snprintf(buf, sizeof(buf), "SYSERR: Bad jurisdiction type %d in room %ld encountered in raw_kill() while transferring %s (%ld). Sending to Dante's entrance.",
                    GET_JURISDICTION(in_room),
                    in_room->number,
                    GET_CHAR_NAME(ch), GET_IDNUM(ch));
            mudlog(buf, ch, LOG_SYSLOG, TRUE);
            i = real_room(RM_ENTRANCE_TO_DANTES);
            break;
        }
      }

      if ((ch->in_veh && AFF_FLAGGED(ch, AFF_PILOT)) || PLR_FLAGGED(ch, PLR_REMOTE)) {
        struct veh_data *veh;
        RIG_VEH(ch, veh);

        send_to_veh("Now driverless, the vehicle slows to a stop.\r\n", veh, ch, FALSE);
        AFF_FLAGS(ch).RemoveBits(AFF_PILOT, AFF_RIG, ENDBIT);
        stop_chase(veh);
        if (!veh->dest)
          veh->cspeed = SPEED_OFF;
      }

      if (ch->persona) {
        if (access_level(ch, LVL_PRESIDENT))
          send_to_char(ch, "^YExtracted icon from host.\r\n");
        snprintf(buf, sizeof(buf), "%s depixelizes and vanishes from the host.\r\n", CAP(ch->persona->name));
        send_to_host(ch->persona->in_host, buf, ch->persona, TRUE);
        extract_icon(ch->persona);
        ch->persona = NULL;
        PLR_FLAGS(ch).RemoveBit(PLR_MATRIX);
      } else if (PLR_FLAGGED(ch, PLR_MATRIX) && ch->in_room) {
        if (access_level(ch, LVL_PRESIDENT))
          send_to_char(ch, "^YPLR_MATRIX set, but not in mtx.\r\n");
        for (struct char_data *temp = get_ch_in_room(ch)->people; temp; temp = temp->next_in_room)
          if (PLR_FLAGGED(temp, PLR_MATRIX))
            temp->persona->decker->hitcher = NULL;
      }

      char_from_room(ch);
      char_to_room(ch, &world[i]);
      PLR_FLAGS(ch).SetBit(PLR_JUST_DIED);
      PLR_FLAGS(ch).RemoveBit(PLR_DOCWAGON_READY);
    }
  }

  if (IS_SPIRIT(ch) && GET_ACTIVE(ch))
    end_spirit_existance(ch, TRUE);
  else extract_char(ch);
}

int raw_stat_loss(struct char_data *ch) {
  int old_tke = GET_TKE( ch );

  for (int limiter = 100; limiter > 0; limiter--) {
    int attribute = number(BOD, WIL);

    if (GET_REAL_ATT(ch, attribute) > MAX(1, 1 + racial_attribute_modifiers[(int)GET_RACE(ch)][attribute])) {
      // We can safely knock down the attribute since we've guaranteed it's above their racial minimum.
      int karma_to_lose = MIN(GET_TKE(ch), 2 * GET_REAL_ATT(ch, attribute));

      // Take the full amount from TKE.
      GET_TKE(ch) -= karma_to_lose;

      // Take half of the amount from notoriety and reputation each.
      GET_NOT(ch) -= karma_to_lose / 2;
      GET_REP(ch) -= karma_to_lose / 2;

      // Knock down the attribute.
      GET_REAL_ATT(ch, attribute)--;
      snprintf(buf, sizeof(buf),"%s lost a point of %s.  Total Karma Earned from %d to %d.",
              GET_CHAR_NAME(ch), short_attributes[attribute], old_tke, GET_TKE( ch ) );
      mudlog(buf, ch, LOG_DEATHLOG, TRUE);
      return attribute;
    }
  }
  mudlog("Note: Would have lost attribute point, but couldn't find one to lose.", ch, LOG_DEATHLOG, TRUE);
  return -1;
}

void death_penalty(struct char_data *ch)
{
  if(!IS_NPC(ch)
     && !PLR_FLAGGED(ch, PLR_NEWBIE)
     && !number(0, DEATH_PENALTY_CHANCE)) // a 1:25 chance of incurring death penalty.
  {
    int lost_attribute = raw_stat_loss(ch);
    if (lost_attribute >= BOD) {
      send_to_char(ch, "^yThere are some things even the DocWagon can't fix :(^n\r\n"
                       "You've lost a point of %s from that death. You can re-train it at a trainer.\r\n",
                       attributes[lost_attribute]);
    }
  }
}

void die(struct char_data * ch)
{
  // If they're ready for docwagon retrieval, save them.
  if (PLR_FLAGGED(ch, PLR_DOCWAGON_READY)) {
    docwagon_retrieve(ch);
    return;
  }

  struct room_data *temp_room = get_ch_in_room(ch);

  if (!(MOB_FLAGGED(ch, MOB_INANIMATE) || IS_PROJECT(ch) || IS_SPIRIT(ch) || IS_ANY_ELEMENTAL(ch))) {
    increase_blood(temp_room);
    act("^rBlood splatters everywhere!^n", FALSE, ch, 0, 0, TO_ROOM);
    if (!GET_BACKGROUND_COUNT(temp_room) || GET_BACKGROUND_AURA(temp_room) == AURA_PLAYERCOMBAT) {
      if (GET_BACKGROUND_AURA(temp_room) != AURA_PLAYERDEATH) {
        GET_SETTABLE_BACKGROUND_COUNT(temp_room) = 1;
        GET_SETTABLE_BACKGROUND_AURA(temp_room) = AURA_PLAYERDEATH;
      } else {
        GET_SETTABLE_BACKGROUND_COUNT(temp_room)++;
      }
    }
  }
  if (!IS_NPC(ch)) {
    death_penalty(ch);
    PLR_FLAGS(ch).RemoveBit(PLR_WANTED);
  }

  if (!PRF_FLAGGED(ch, PRF_DONT_ALERT_PLAYER_DOCTORS_ON_MORT)) {
    alert_player_doctors_of_contract_withdrawal(ch, TRUE);
  }

  AFF_FLAGS(ch).RemoveBit(AFF_HEALED);

  raw_kill(ch);
}

/*
 * Lets the player give up and die if they're at 0 or less
 * physical points.
 */
ACMD(do_die)
{
  char buf[100];

  /* If they're still okay... */
  if ( GET_PHYSICAL(ch) >= 100 && GET_MENTAL(ch) >= 100 ) {
    send_to_char("Your mother would be so sad... :(\n\r",ch);
    return;
  }

  // If they're ready to be docwagon'd out, save them.
  if (PLR_FLAGGED(ch, PLR_DOCWAGON_READY)) {
    docwagon_retrieve(ch);
    return;
  }

  send_to_char("You give up the will to live...\n\r",ch);

  /* log it */
  snprintf(buf, sizeof(buf),"%s gave up the will to live. {%s%s (%ld)}",
          GET_CHAR_NAME(ch),
          ch->in_veh ? "in veh at " : "",
          GET_ROOM_NAME(get_ch_in_room(ch)),
          GET_ROOM_VNUM(get_ch_in_room(ch)));
  mudlog(buf, ch, LOG_DEATHLOG, TRUE);

  /* Now we just kill them, MuHahAhAhahhaAHhaAHaA!!...or something */
  die(ch);

  return;
}

int calc_karma(struct char_data *ch, struct char_data *vict)
{
  int base = 0, i, bonus_karma;

  if (!vict || !IS_NPC(vict))
    return 0;

  if (ch && IS_PROJECT(ch))
    ch = ch->desc->original;

  if (!ch || IS_NPC(ch))
    return 0;

  if ((GET_RACE(vict) == RACE_SPIRIT || GET_RACE(vict) == RACE_PC_CONJURED_ELEMENTAL) && GET_IDNUM(ch) == GET_ACTIVE(vict))
    return 0;

  base = (int)((1.2 * (GET_BOD(vict) + GET_BALLISTIC(vict) + GET_IMPACT(vict))) +
               (GET_QUI(vict) + (int)(GET_MAG(vict) / 100) +
                GET_STR(vict) + GET_WIL(vict) + GET_REA(vict) + GET_INIT_DICE(vict)));

  for (i = MIN_SKILLS; i < MAX_SKILLS; i++)
    if (GET_SKILL(vict, i) > 0)
    {
      if (i == SKILL_SORCERY && GET_MAG(vict) > 0)
        base += (int)(1.2 * GET_SKILL(vict, i));
      else if (return_general(i) == SKILL_FIREARMS ||
               return_general(i) == SKILL_UNARMED_COMBAT ||
               return_general(i) == SKILL_ARMED_COMBAT)
        base += (int)(1.4 * GET_SKILL(vict, i));
    }

  if (GET_RACE(vict) == RACE_SPIRIT || GET_RACE(vict) == RACE_ELEMENTAL)
    base = (int)(base * 1.15);

  struct obj_data *weapon = NULL;
  if (!((weapon = GET_EQ(vict, WEAR_WIELD)) && GET_OBJ_TYPE(weapon) == ITEM_WEAPON)
      && !((weapon = GET_EQ(vict, WEAR_HOLD)) && GET_OBJ_TYPE(weapon) == ITEM_WEAPON))
  {
    for (weapon = vict->carrying; weapon; weapon = weapon->next_content)
      if (GET_OBJ_TYPE(weapon) == ITEM_WEAPON)
        break;
  }

  if (weapon) {
    base += GET_WEAPON_POWER(weapon) * GET_WEAPON_DAMAGE_CODE(weapon);

    if (IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))) {
      int rnum = real_mobile(GET_MOB_VNUM(vict));
      for (int index = 0; index < NUM_AMMOTYPES; index++) {
        if (GET_BULLETPANTS_AMMO_AMOUNT(&mob_proto[rnum], GET_WEAPON_ATTACK_TYPE(weapon), npc_ammo_usage_preferences[index]) > 0) {
          int ammo_value = NPC_AMMO_USAGE_PREFERENCES_AMMO_NORMAL_INDEX - index; // APDS = 3, EX = 2, Explosive = 1, Normal = 0, Gel -1, Flechette -2... see bullet_pants.cpp
          base += ammo_value * 10;
        }
      }
    }
  }

  bonus_karma = MIN( GET_KARMA(vict), base );
  base += bonus_karma;

  if (PLR_FLAGGED(ch, PLR_NEWBIE))
    base -= (int)(GET_NOT(ch) / 7);
  else
    base -= (int)(GET_NOT(ch) / 5);

  //now to randomize it a bit
  base += (!ch ? 0 : (!number(0,2) ? number(0,5) :
                      0 - number(0,5)));

  base = (ch ? MIN(max_exp_gain, base) : base);
  base = MAX(base, 1);

  if (!IS_SENATOR(ch) && vnum_from_non_connected_zone(GET_MOB_VNUM(vict)))
    base = 0;


  return base;
}

bool eligible_for_karma_gain(struct char_data *ch) {
  if (!ch) {
    mudlog("SYSERR: Received NULL character to eligible_for_karma_gain()!", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (IS_PC_CONJURED_ELEMENTAL(ch) || IS_SPIRIT(ch))
    return FALSE;

  // Chargen characters don't get karma.
  if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED))
    return FALSE;

  return TRUE;
}

void perform_group_gain(struct char_data * ch, int base, struct char_data * victim)
{
  int share;

  // Short-circuit: Not authed yet? No karma.
  if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED))
    return;

  share = MIN(max_exp_gain, MAX(1, base));
  if (!IS_NPC(ch)) {
    // Cap XP gain to 0.20 karma if you're a newbie, otherwise notor * 2
    share = MIN(base, (int) (PLR_FLAGGED(ch, PLR_NEWBIE) ? 20 : GET_NOT(ch) * 2));
  }

  /* psuedo-fix of the group with a newbie to get more exp exploit */
  if ( !PLR_FLAGGED(ch, PLR_NEWBIE) )
    share /= 2;

  share = gain_karma(ch, share, FALSE, TRUE, TRUE);

  if ( share >= (200) || access_level(ch, LVL_BUILDER) )
  {
    snprintf(buf, sizeof(buf),"%s gains %.2f karma from killing %s (group share).", GET_CHAR_NAME(ch),
            (double)share/100.0, GET_CHAR_NAME(victim));
    mudlog(buf, ch, LOG_DEATHLOG, TRUE);
  }
  if ( IS_NPC( victim ) )
  {
    // TODO: If you end up implementing value_death_karma (what even is this?), check to make sure there are no exploits around farming projections/spirits
    extern struct char_data *mob_proto;
    mob_proto[GET_MOB_RNUM(victim)].mob_specials.value_death_karma += share;
  }

  send_to_char(ch, "You receive your share of %0.2f karma.\r\n", ((float)share / 100));
}

void group_gain(struct char_data * ch, struct char_data * victim)
{
  int tot_members, base;
  struct char_data *k, *high;
  struct follow_type *f;

  if (!(k = ch->master))
    k  = ch;
  high = k;

  if (IS_AFFECTED(k, AFF_GROUP) && (k->in_room == ch->in_room))
    tot_members = 1;
  else
    tot_members = 0;

  for (f = k->followers; f; f = f->next)
    if (IS_AFFECTED(f->follower, AFF_GROUP)
        && f->follower->in_room == ch->in_room
        && eligible_for_karma_gain(f->follower))
    {
      if (GET_NOT(high) < GET_NOT(f->follower))
        high = f->follower;
      tot_members++;
    }

  base = calc_karma(high, victim) / tot_members + 1;

  if (tot_members >= 1)
    base = MAX(1, (base / tot_members));
  else
    base = 0;

  if (IS_AFFECTED(k, AFF_GROUP) && k->in_room == ch->in_room)
    perform_group_gain(k, base, victim);

  for (f = k->followers; f; f = f->next)
    if (IS_AFFECTED(f->follower, AFF_GROUP) && f->follower->in_room == ch->in_room)
      perform_group_gain(f->follower, base, victim);
}

char *replace_string(const char *str, const char *weapon_singular, const char *weapon_plural,
                     const char *weapon_different)
{
  static char buf[256];
  char *cp;

  cp = buf;

  for (; *str; str++) {
    if (*str == '#') {
      switch (*(++str)) {
        case 'W':
          for (; *weapon_plural; *(cp++) = *(weapon_plural++))
            ;
          break;
        case 'w':
          for (; *weapon_singular; *(cp++) = *(weapon_singular++))
            ;
          break;
        case 'd':
          for (; *weapon_different; *(cp++) = *(weapon_different++))
            ;
          break;
        default:
          *(cp++) = '#';
          break;
      }
    } else
      *(cp++) = *str;

    *cp = 0;
  }                             /* For */

  return (buf);
}

#define SENDOK(ch) ((ch)->desc && AWAKE(ch) && !(PLR_FLAGGED((ch), PLR_WRITING) || PLR_FLAGGED((ch), PLR_EDITING) || PLR_FLAGGED((ch), PLR_MAILING) || PLR_FLAGGED((ch), PLR_CUSTOMIZE)) && (STATE(ch->desc) != CON_SPELL_CREATE))

/* message for doing damage with a weapon */
void dam_message(int dam, struct char_data * ch, struct char_data * victim, int w_type)
{
  char *buf;
  int msgnum;
  struct char_data *witness;

  static struct dam_weapon_type
  {
    const char *to_room;
    const char *to_char;
    const char *to_victim;
  }
  dam_weapons[] = {
    {
      "$n tries to #w $N, but misses.", /* 0: 0     */
      "You try to #w $N, but miss.",
      "$n tries to #w you, but misses."
    },

    {
      "$n grazes $N as $e #W $M.",      /* 1: 1..2  */
      "You graze $N as you #w $M.",
      "$n grazes you with $s #w."
    },

    {
      "$n barely #W $N.",               /* 2: 3..4  */
      "You barely #w $N.",
      "$n barely #W you."
    },

    {
      "$n #W $N.",                      /* 3: 5..6  */
      "You #w $N.",
      "$n #W you."
    },

    {
      "$n #W $N hard.",                 /* 4: 7..10  */
      "You #w $N hard.",
      "$n #W you hard."
    },

    {
      "$n #W $N very hard.",            /* 5: 11..14  */
      "You #w $N very hard.",
      "$n #W you very hard."
    },

    {
      "$n #W $N extremely hard.",       /* 6: 15..19  */
      "You #w $N extremely hard.",
      "$n #W you extremely hard."
    },

    {
      "$n massacres $N to small fragments with $s #d.", /* 7: 19..23 */
      "You massacre $N to small fragments with your #d.",
      "$n massacres you to small fragments with $s #d."
    },

    {
      "$n demolishes $N with $s deadly #d!",    /* 8: > 23   */
      "You demolish $N with your deadly #d!",
      "$n demolishes you with $s deadly #d!"
    },

    {
      "$n pulverizes $N with $s incredibly powerful #d!!",
      "You pulverize $N with your incredibly powerful #d!!",
      "$n pulverizes you with $s incredibly powerful #d!!",
    },

    {
      "$n sublimates $N with an ultimate #d!!!",
      "You sublimate $N with an ultimate #d!!!",
      "$n sublimates you with an ultimate #d!!!",
    }
  };


  w_type -= TYPE_HIT;           /* Change to base of table with text */

  if (dam < 0)
    msgnum = 0;
  else if (dam == 0)
  {
    switch(number(0,1)) {
      case 0:
        msgnum = 1;
        break;
      case 1:
        msgnum = 2;
        break;
      default:
        msgnum = 0;
        break;
    }
  } else if (dam <= 1)
    msgnum = 3;
  else if (dam <= 3)
    msgnum = 5;
  else if (dam <= 6)
    msgnum = 7;
  else if (dam <= 10)
    msgnum = 8;
  else if (dam <= 13)
    msgnum = 9;
  else
    msgnum = 10;
  /* damage message to onlookers */
  buf = replace_string(dam_weapons[msgnum].to_room,
                       attack_hit_text[w_type].singular, attack_hit_text[w_type].plural,
                       attack_hit_text[w_type].different);
  for (witness = get_ch_in_room(victim)->people; witness; witness = witness->next_in_room)
    if (witness != ch && witness != victim && !PRF_FLAGGED(witness, PRF_FIGHTGAG) && SENDOK(witness))
      perform_act(buf, ch, NULL, victim, witness);
  if (ch->in_room != victim->in_room && !PLR_FLAGGED(ch, PLR_REMOTE))
    for (witness = get_ch_in_room(ch)->people; witness; witness = witness->next_in_room)
      if (witness != ch && witness != victim && !PRF_FLAGGED(witness, PRF_FIGHTGAG) && SENDOK(witness))
        perform_act(buf, ch, NULL, victim, witness);


  /* damage message to damager */
  strcpy(buf1, "^y");
  buf = replace_string(dam_weapons[msgnum].to_char,
                       attack_hit_text[w_type].singular, attack_hit_text[w_type].plural,
                       attack_hit_text[w_type].different);
  strcat(buf1, buf);
  if (SENDOK(ch))
    perform_act(buf1, ch, NULL, victim, ch);

  /* damage message to damagee */
  strcpy(buf1, "^r");
  buf = replace_string(dam_weapons[msgnum].to_victim,
                       attack_hit_text[w_type].singular, attack_hit_text[w_type].plural,
                       attack_hit_text[w_type].different);
  strcat(buf1, buf);
  act(buf1, FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
}
#undef SENDOK

/*
 * message for doing damage with a spell or skill
 *  C3.0: Also used for weapon damage on miss and death blows
 */
bool skill_message(int dam, struct char_data * ch, struct char_data * vict, int attacktype)
{
  int i, j, nr;
  struct message_type *msg;

  struct obj_data *weap = ch->equipment[WEAR_WIELD];

  for (i = 0; i < MAX_MESSAGES; i++)
  {
    if (fight_messages[i].a_type == attacktype) {
      nr = dice(1, fight_messages[i].number_of_attacks);
      for (j = 1, msg = fight_messages[i].msg; (j < nr) && msg; j++)
        msg = msg->next;

      if (!IS_NPC(vict) && (IS_SENATOR(vict)) && dam < 1) {
        act(msg->god_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
        act(msg->god_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT);
        act(msg->god_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
      } else if (dam > 0) {
        if (GET_POS(vict) == POS_DEAD) {
          send_to_char(CCYEL(ch, C_CMP), ch);
          act(msg->die_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
          send_to_char(CCNRM(ch, C_CMP), ch);

          send_to_char(CCRED(vict, C_CMP), vict);
          act(msg->die_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
          send_to_char(CCNRM(vict, C_CMP), vict);

          act(msg->die_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
        } else {
          send_to_char(CCYEL(ch, C_CMP), ch);
          act(msg->hit_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
          send_to_char(CCNRM(ch, C_CMP), ch);

          send_to_char(CCRED(vict, C_CMP), vict);
          act(msg->hit_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
          send_to_char(CCNRM(vict, C_CMP), vict);

          act(msg->hit_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
        }
      } else if ((ch != vict) && (dam <= 0)) {  /* Dam == 0 */
        send_to_char(CCYEL(ch, C_CMP), ch);
        act(msg->miss_msg.attacker_msg, FALSE, ch, weap, vict, TO_CHAR);
        send_to_char(CCNRM(ch, C_CMP), ch);

        send_to_char(CCRED(vict, C_CMP), vict);
        act(msg->miss_msg.victim_msg, FALSE, ch, weap, vict, TO_VICT | TO_SLEEP);
        send_to_char(CCNRM(vict, C_CMP), vict);

        act(msg->miss_msg.room_msg, FALSE, ch, weap, vict, TO_NOTVICT);
      }
      return 1;
    }
  }
  return 0;
}

int find_orig_dir(struct char_data *ch, struct char_data *victim)
{
  struct char_data *vict;
  int dir, dist;
  struct room_data *nextroom = NULL, *room = NULL;

  for (dir = 0; dir < NUM_OF_DIRS; dir++)
  {
    room = get_ch_in_room(victim);

    if (CAN_GO2(room, dir))
      nextroom = EXIT2(room, dir)->to_room;
    else
      nextroom = NULL;

    for (dist = 1; nextroom && dist <= 4; dist++) {
      for (vict = nextroom->people; vict; vict = vict->next_in_room)
        if (vict == ch)
          return (rev_dir[dir]);

      room = nextroom;
      if (CAN_GO2(room, dir))
        nextroom = EXIT2(room, dir)->to_room;
      else
        nextroom = NULL;
    }
  }
  return -1;
}

void weapon_scatter(struct char_data *ch, struct char_data *victim, struct obj_data *weapon)
{
  struct char_data *vict;
  char ammo_type[20];
  int damage_total, power, total = 0, door = 0, dir[3], i;

  if (!ch || !victim || !weapon)
    return;

  dir[1] = find_orig_dir(ch, victim);
  if (dir[1] >= NORTH && dir[1] <= NORTHWEST)
  {
    if (dir[1] == NORTH)
      dir[0] = NORTHWEST;
    else
      dir[0] = dir[1] - 1;
    if (dir[1] == NORTHWEST)
      dir[2] = NORTH;
    else
      dir[2] = dir[1] + 1;
  } else
    dir[0] = dir[2] = -1;

  for (vict = get_ch_in_room(victim)->people; vict; vict = vict->next_in_room)
    if (vict != victim && !IS_ASTRAL(vict) && GET_POS(vict) > POS_SLEEPING)
      total++;

  for (i = 0; i < 3; i++)
    if (dir[i] != -1 && EXIT(victim, dir[i]) &&
        IS_SET(EXIT(victim, dir[i])->exit_info, EX_CLOSED))
      door += 2;

  switch(GET_OBJ_VAL(weapon, 3))
  {
    case WEAP_SHOTGUN:
      snprintf(ammo_type, sizeof(ammo_type), "horde of pellets");
      break;
    case WEAP_LMG:
    case WEAP_MMG:
    case WEAP_HMG:
      snprintf(ammo_type, sizeof(ammo_type), "stream of bullets");
      break;
    case WEAP_CANNON:
      snprintf(ammo_type, sizeof(ammo_type), "shell");
      break;
    case WEAP_MISS_LAUNCHER:
      snprintf(ammo_type, sizeof(ammo_type), "rocket");
      break;
    case WEAP_GREN_LAUNCHER:
      snprintf(ammo_type, sizeof(ammo_type), "grenade");
      break;
    case WEAP_TASER:
      snprintf(ammo_type, sizeof(ammo_type), "taser dart");
      break;
    default:
      snprintf(ammo_type, sizeof(ammo_type), "bullet");
      break;
  }

  i = number(1, MAX(20, total + door + 2));
  if (i <= total)
  { // hits a victim
    for (vict = get_ch_in_room(victim)->people; vict; vict = vict->next_in_room)
      if (vict != victim && !IS_ASTRAL(vict) && GET_POS(vict) >= POS_MORTALLYW &&
          !number(0, total - 1))
        break;

    if (vict && (IS_NPC(vict) || (!IS_NPC(vict) && vict->desc)) && can_hurt(ch, vict, TYPE_SCATTERING, TRUE)) {
      power = MAX(GET_WEAPON_POWER(weapon) - GET_BALLISTIC(vict) - 3, 2);

      // Spirits and elementals get their own diva powers.
      if (IS_SPIRIT(vict) || IS_ANY_ELEMENTAL(vict)) {
        power -= GET_LEVEL(vict) * 2;
      }

      // We do some ghettoized bastardization of the fight code here to make scatterfire a little less OP.
      if (AWAKE(vict) && !AFF_FLAGGED(vict, AFF_SURPRISE) && !AFF_FLAGGED(vict, AFF_PRONE)) {
        int def_dice = GET_DEFENSE(vict) + GET_POWER(vict, ADEPT_SIDESTEP);
        int def_tn = damage_modifier(vict, buf, sizeof(buf));
        int def_successes = MAX(success_test(def_dice, def_tn), 0);
        power -= def_successes;
      }

      // Successful dodge?
      if (power <= 0) {
        snprintf(buf, sizeof(buf), "A stray %s flies in from nowhere, almost hitting you!", ammo_type);
        act(buf, FALSE, vict, 0, 0, TO_CHAR);
        snprintf(buf, sizeof(buf), "A stray %s hums by, barely missing $n!", ammo_type);
        act(buf, FALSE, vict, 0, 0, TO_ROOM);
      }
      // Failed to dodge.
      else {
        snprintf(buf, sizeof(buf), "A stray %s flies in from nowhere, hitting you!", ammo_type);
        act(buf, FALSE, vict, 0, 0, TO_CHAR);
        snprintf(buf, sizeof(buf), "A stray %s hums into the room and hits $n!", ammo_type);
        act(buf, FALSE, vict, 0, 0, TO_ROOM);

        damage_total = MAX(1, GET_WEAPON_DAMAGE_CODE(weapon));
        damage_total = convert_damage(stage((2 - success_test(GET_BOD(vict) + GET_BODY(vict), power)), damage_total));
        if (damage(ch, vict, damage_total, TYPE_SCATTERING, PHYSICAL)) {
          // They died!
          return;
        }
      }

      // If you're not already fighting someone else, that's a great reason to get into combat, don't you think?
      if (!FIGHTING(vict))
        ranged_response(ch, vict);
      return;
    }
  } else if (i > (MAX(20, total + door + 2) - door))
  { // hits a door
    for (i = 0; i < 3; i++)
      if (dir[i] != -1 && EXIT(victim, dir[i]) && !number(0, door - 1) &&
          IS_SET(EXIT(victim, dir[i])->exit_info, EX_CLOSED))
        break;

    if (i < 3) {
      snprintf(buf, sizeof(buf), "A %s hums into the room and hits the %s!", ammo_type,
              fname(EXIT(victim, dir[i])->keyword));
      act(buf, FALSE, victim, 0, 0, TO_ROOM);
      act(buf, FALSE, victim, 0, 0, TO_CHAR);
      damage_door(NULL, get_ch_in_room(victim), dir[i], GET_OBJ_VAL(weapon, 0), DAMOBJ_PROJECTILE);
      return;
    }
  }

  // if it's reached this point, it's harmless
  snprintf(buf, sizeof(buf), "A %s hums harmlessly through the room.", ammo_type);
  act(buf, FALSE, victim, 0, 0, TO_ROOM);
  act(buf, FALSE, victim, 0, 0, TO_CHAR);
}

void damage_equip(struct char_data *ch, struct char_data *victim, int power,
                  int attacktype)
{
  int i;

  // Acid is unusually aggressive.
  if (attacktype == TYPE_ACID) {
    int limiter = 5;

    // Roll until we find an occupied slot OR run out of limiter.
    while ((i = number(WEAR_HEAD, NUM_WEARS - 1)) && (!GET_EQ(victim, i) || i == WEAR_PATCH)) {
      if (--limiter <= 0)
        return;
    }

    // Damage the object.
    damage_obj(ch, GET_EQ(victim, i), power, DAMOBJ_ACID);
  }

  // Roll a random wearslot number.
  i = number(0, attacktype == TYPE_FIRE ? NUM_WEARS : 60);

  // Rolled too high of a slot, or the slot is empty? No damage.
  if (i == WEAR_PATCH || i >= NUM_WEARS || !GET_EQ(victim, i))
    return;

  if (attacktype == TYPE_FIRE) {
    damage_obj(ch, GET_EQ(victim, i), power, DAMOBJ_FIRE);
    return;
  }

  if (attacktype >= TYPE_PISTOL && attacktype <= TYPE_BIFURCATE) {
    damage_obj(ch, GET_EQ(victim, i), power, DAMOBJ_PROJECTILE);
    return;
  }

  switch (attacktype)
  {
    case TYPE_PIERCE:
    case TYPE_STAB:
    case TYPE_SHURIKEN:
      damage_obj(ch, GET_EQ(victim, i), power, DAMOBJ_PIERCE);
      break;
    case TYPE_STING:
    case TYPE_SLASH:
    case TYPE_CLAW:
    case TYPE_GORE:
    case TYPE_ARROW:
    case TYPE_THROWING_KNIFE:
    case TYPE_WHIP:
      damage_obj(ch, GET_EQ(victim, i), power, DAMOBJ_SLASH);
      break;
    case TYPE_HIT:
    case TYPE_BLUDGEON:
    case TYPE_POUND:
    case TYPE_MAUL:
    case TYPE_PUNCH:
      damage_obj(ch, GET_EQ(victim, i), power, DAMOBJ_CRUSH);
      break;
  }
}

float power_multiplier(int type, int material)
{
  switch (type) {
    case DAMOBJ_ACID:
      switch (material) {
        case 6:
        case 7:
        case 10:
        case 11:
          return 1.2;
        case 13:
          return 2.0;
      }
      break;
    case DAMOBJ_AIR:
      switch (material) {
        case 2:
          return 2.0;
        case 5:
        case 8:
        case 14:
        case 15:
        case 16:
          return 0.4;
        default:
          return 0.8;
      }
      break;
    case DAMOBJ_EARTH:
      switch (material) {
        case 2:
          return 2.0;
        default:
          return 0.9;
      }
      break;
    case DAMOBJ_FIRE:
      switch (material) {
        case 0:
        case 1:
        case 3:
          return 1.5;
        case 4:
        case 6:
        case 7:
          return 1.1;
        default:
          return 0.8;
      }
      break;
    case DAMOBJ_ICE:
      switch (material) {
        case 3:
        case 4:
          return 1.2;
        default:
          return 0.9;
      }
      break;
    case DAMOBJ_LIGHTNING:
      switch (material) {
        case 8:
          return 2.0;
        case 10:
        case 11:
          return 2.0;
        default:
          return 0.7;
      }
      break;
    case DAMOBJ_WATER:
      switch (material) {
        case 10:
        case 11:
          return 2.0;
        default:
          return 0.5;
      }
      break;
    case DAMOBJ_EXPLODE:
      switch (material) {
        case 0:
        case 1:
        case 3:
        case 4:
        case 13:
          return 1.3;
        case 2:
          return 2.0;
        default:
          return 1.1;
      }
      break;
    case DAMOBJ_PROJECTILE:
      switch (material) {
        case 2:
          return 2.0;
        case 10:
        case 11:
          return 1.1;
        default:
          return 0.8;
      }
      break;
    case DAMOBJ_CRUSH:
      switch (material) {
        case 1:
        case 6:
        case 7:
          return 1.4;
        case 2:
          return 2.0;
        case 5:
        case 14:
        case 15:
        case 16:
          return 1.0;
        default:
          return 0.7;
      }
      break;
    case DAMOBJ_SLASH:
      switch (material) {
        case 0:
        case 3:
        case 4:
        case 13:
          return 1.4;
        case 5:
        case 14:
        case 15:
        case 16:
          return 0.3;
        default:
          return 0.6;
      }
      break;
    case DAMOBJ_PIERCE:
      switch (material) {
        case 0:
        case 3:
        case 4:
        case 13:
          return 1.0;
        case 5:
        case 8:
        case 14:
        case 15:
        case 16:
          return 0.1;
        default:
          return 0.3;
      }
      break;
  }
  return 1.0;
}

void damage_door(struct char_data *ch, struct room_data *room, int dir, int power, int type)
{
  if (!room || dir < NORTH || dir > DOWN || !room->dir_option[dir] || !room->dir_option[dir]->keyword || !IS_SET(room->dir_option[dir]->exit_info, EX_CLOSED))
    return;

  int rating, half, rev, ok = 0;
  struct room_data *opposite = room->dir_option[dir]->to_room;

  rev = rev_dir[dir];
  if (opposite && opposite->dir_option[rev] && opposite->dir_option[rev]->to_room == room)
    ok = TRUE;

  // Remove the hidden bits-- doors don't stay hidden when attacked.
  REMOVE_BIT(room->dir_option[dir]->exit_info, EX_HIDDEN);
  if (ok)
    REMOVE_BIT(opposite->dir_option[rev]->exit_info, EX_HIDDEN);

  // Dominators break down doors without rolls.
  if (ch && GET_EQ(ch, WEAR_WIELD) && GET_OBJ_SPEC(GET_EQ(ch, WEAR_WIELD)) == weapon_dominator) {
    destroy_door(room, dir);
    snprintf(buf, sizeof(buf), "The %s is destroyed in a scintillating burst of blue light!\r\n", fname(room->dir_option[dir]->keyword));

    send_to_room(buf, room);
    if (ch->in_room != room)
      send_to_char(buf, ch);

    if (ok) {
      destroy_door(opposite, rev);
      snprintf(buf, sizeof(buf2), "The %s is destroyed from the other side in a scintillating burst of blue light!\r\n", fname(opposite->dir_option[rev]->keyword));
      send_to_room(buf, opposite);
    }
    return;
  }

  if (IS_SET(type, DAMOBJ_MANIPULATION)) {
    REMOVE_BIT(type, DAMOBJ_MANIPULATION);
    rating = room->dir_option[dir]->barrier;
  /*
  } else if (type == DAMOBJ_CRUSH) {
    rating = room->dir_option[dir]->barrier;
  */
  } else {
    rating = room->dir_option[dir]->barrier * 2;
  }

  switch (room->dir_option[dir]->material) {
    // Model weak materials.
    case MATERIAL_PAPER       :
      rating /= 4;
      break;
    case MATERIAL_ELECTRONICS :
    case MATERIAL_GLASS       :
    case MATERIAL_FABRIC      :
    case MATERIAL_COMPUTERS   :
    case MATERIAL_TOXIC_WASTES:
      rating /= 2;
      break;
    case MATERIAL_ORGANIC     :
    case MATERIAL_LEATHER     :
      rating--;
      break;
    case MATERIAL_WOOD        :
    case MATERIAL_PLASTIC     :
    case MATERIAL_CERAMIC     :
      // no-op
      break;
    case MATERIAL_ADV_PLASTICS:
    case MATERIAL_ORICHALCUM  :
    case MATERIAL_BRICK       :
    case MATERIAL_STONE       :
    case MATERIAL_METAL       :
    case MATERIAL_CONCRETE    :
      rating++;
      break;
    default:
      mudlog("SYSERR: Unknown material type in door-damage switch-- update it!", ch, LOG_SYSLOG, TRUE);
      break;
  }

  half = MAX(1, rating >> 1);

  if (ch && IS_SET(type, DAMOBJ_CRUSH) && GET_TRADITION(ch) == TRAD_ADEPT && GET_POWER(ch, ADEPT_SMASHING_BLOW))
    power += MAX(0, success_test(GET_SKILL(ch, SKILL_UNARMED_COMBAT), 4));
  if (IS_GUN(type))
    snprintf(buf, sizeof(buf), "You hear gunshots and the sound of bullets impacting the %s.\r\n", fname(room->dir_option[dir]->keyword));
  else
    snprintf(buf, sizeof(buf), "Someone bashes on the %s from the other side.\r\n", fname(room->dir_option[dir]->keyword));
  send_to_room(buf, opposite);

  if (power < half)
  {
    snprintf(buf, sizeof(buf), "The %s remains undamaged.\r\n", fname(room->dir_option[dir]->keyword));
    send_to_room(buf, room);
    if (ch && ch->in_room != room)
      send_to_char(buf, ch);
    return;
  }

  // Compose the damage message, but don't send it-- it can be overwritten by the destruction message if necessary.
  if (power < rating) {
    snprintf(buf, sizeof(buf), "The %s has been slightly damaged.\r\n", fname(room->dir_option[dir]->keyword));
    room->dir_option[dir]->condition--;
  } else {
    snprintf(buf, sizeof(buf), "The %s has been damaged!\r\n", fname(room->dir_option[dir]->keyword));
    room->dir_option[dir]->condition -= 1 + (power - rating) / half;
  }

  if (ok)
    opposite->dir_option[rev]->condition = room->dir_option[dir]->condition;

  if (room->dir_option[dir]->condition <= 0) {
    snprintf(buf, sizeof(buf), "The %s has been destroyed!\r\n", fname(room->dir_option[dir]->keyword));
    destroy_door(room, dir);
    if (ok) {
      snprintf(buf2, sizeof(buf2), "The %s is destroyed from the other side!\r\n", fname(room->dir_option[dir]->keyword));
      send_to_room(buf2, opposite);
      destroy_door(opposite, rev);
    }
  }

  send_to_room(buf, room);
  if (ch && ch->in_room != room)
    send_to_char(buf, ch);
}

// damage_obj does what its name says, it figures out effects of successes
// damage, applies that damage to that object, and sends messages to the player
void damage_obj(struct char_data *ch, struct obj_data *obj, int power, int type)
{
  if (!obj)
    return;
  if (power <= 0)
    return;

  int rating, half;
  struct char_data *vict = get_obj_possessor(obj);
  struct obj_data *temp, *next;

  // PC corpses are indestructable by normal means
  if ( IS_OBJ_STAT(obj, ITEM_EXTRA_CORPSE) && GET_OBJ_VAL(obj, 4) == 1 ) {
    if ( ch != NULL )
      send_to_char("You are not allowed to damage a player's corpse.\n\r",ch);
    return;
  }

  if (IS_SET(type, DAMOBJ_MANIPULATION) || type == DAMOBJ_FIRE || type == DAMOBJ_ACID || type == DAMOBJ_LIGHTNING) {
    rating = GET_OBJ_BARRIER(obj);
    REMOVE_BIT(type, DAMOBJ_MANIPULATION);
  } else
    rating = GET_OBJ_BARRIER(obj) * 2;

  power = (int)(power * power_multiplier(type, GET_OBJ_MATERIAL(obj)));
  half = MAX(1, rating / 2);

  if (power < half) {
    if (ch)
      send_to_char(ch, "%s remains undamaged.\r\n",
                   CAP(GET_OBJ_NAME(obj)));
    return;
  }

  if (vict && GET_TKE(vict) <= NEWBIE_KARMA_THRESHOLD) {
    send_to_char(vict, "^y(%s would have been damaged, but was shielded by your newbie status!)^n\r\n", CAP(GET_OBJ_NAME(obj)));
    return;
  }

  /* Don't cascade down damage, it's unnecessarily punitive. -LS
  // Cascade damage down to everything except custom decks and parts (the latter due to breaking the game)
  if (GET_OBJ_TYPE(obj) != ITEM_CUSTOM_DECK && GET_OBJ_TYPE(obj) != ITEM_PART) {
    for (struct obj_data *cont = obj->contains; cont; cont = cont->next_content)
      damage_obj(ch, cont, power, type);
  }
  */

  /* This whole block is a no-op due to the `success == 2` block. Success is not actually set anywhere... -LS
  switch (type) {
    case DAMOBJ_ACID:
      if ((GET_OBJ_TYPE(obj) == ITEM_GYRO || GET_OBJ_TYPE(obj) == ITEM_WORN) && success == 2)
        switch (number(0, 4)) {
          case 0:
            GET_OBJ_VAL(obj, 5) = MAX(GET_OBJ_VAL(obj, 0) - 1, 0);
            GET_OBJ_VAL(obj, 6) = MAX(GET_OBJ_VAL(obj, 1) - 1, 0);
            break;
          case 1:
          case 2:
            GET_OBJ_VAL(obj, 5) = MAX(GET_OBJ_VAL(obj, 0) - 1, 0);
            break;
          case 3:
          case 4:
            GET_OBJ_VAL(obj, 6) = MAX(GET_OBJ_VAL(obj, 1) - 1, 0);
            break;
        }
      break;
    case DAMOBJ_FIRE:
    case DAMOBJ_EXPLODE:
      if (GET_OBJ_TYPE(obj) == ITEM_GUN_MAGAZINE && success == 2 && vict) {
        act("$p ignites, spraying bullets about!", FALSE, vict, obj, 0, TO_CHAR);
        act("One of $n's magazines ignites, spraying bullets about!",
            FALSE, vict, obj, 0, TO_ROOM);
        target = (int)(GET_OBJ_VAL(obj, 0) / 4);
        switch (GET_OBJ_VAL(obj, 1) + 300) {
          case TYPE_ROCKET:
            dam = DEADLY;
            break;
          case TYPE_GRENADE_LAUNCHER:
          case TYPE_CANNON:
            dam = SERIOUS;
            break;
          case TYPE_SHOTGUN:
          case TYPE_MACHINE_GUN:
          case TYPE_RIFLE:
            dam = MODERATE;
            break;
          default:
            dam = LIGHT;
        }
        dam = convert_damage(stage(-success_test(GET_BOD(vict) + GET_BODY(vict),
                                                 target), dam));
        damage(vict, vict, dam, TYPE_SCATTERING, TRUE);
        extract_obj(obj);
        return;
      }
      if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && success == 2 &&
          GET_OBJ_VAL(obj, 3) == TYPE_HAND_GRENADE) {
        if (vict) {
          act("$p is set off by the fire!", FALSE, vict, obj, 0, TO_CHAR);
          act("A $p carried by $n is set off by the fire!", FALSE, vict, obj, 0, TO_ROOM);
          explode(NULL, obj, obj->in_room);
        } else if (obj->in_room) {
          snprintf(buf, sizeof(buf), "%s is set off by the flames!",
                  CAP(obj->text.name));
          send_to_room(buf, obj->in_room);
          explode(NULL, obj, obj->in_room);
        }
        return;
      }
      break;
  }
  */

  char inside_buf[200];
  if (obj->in_obj)
    snprintf(inside_buf, sizeof(inside_buf), "Inside %s, ", GET_OBJ_NAME(obj->in_obj));
  else
    strlcpy(inside_buf, "", sizeof(inside_buf));

  if (power < rating) {
    if (ch) {
      send_to_char(ch, "%s%s%s%s has been slightly damaged.^n\r\n",
                   ch == vict ? "^y" : "",
                   CAP(inside_buf),
                   GET_OBJ_NAME(obj),
                   ch == vict ? "^y" : "");
    }
    if (vict && vict != ch) {
      send_to_char(vict, "^y%s%s^y has been slightly damaged.^n\r\n",
                   CAP(inside_buf),
                   GET_OBJ_NAME(obj));
    }
    GET_OBJ_CONDITION(obj)--;
  }
  else {
    if (ch) {
      send_to_char(ch, "%s%s%s%s has been damaged!^n%s\r\n",
                   ch == vict ? "^Y" : "",
                   CAP(inside_buf),
                   GET_OBJ_NAME(obj),
                   ch == vict ? "^Y" : "",
                   ch == vict ? (SHOULD_SEE_TIPS(ch) && GET_OBJ_CONDITION(obj) > 0 ? " Better find a repairman..." : "") : ""
                 );
    }
    if (vict && vict != ch) {
      send_to_char(vict, "^Y%s%s^Y has been damaged!^n%s\r\n",
                   CAP(inside_buf),
                   GET_OBJ_NAME(obj),
                   SHOULD_SEE_TIPS(ch) && GET_OBJ_CONDITION(obj) > 0 ? " Better find a repairman..." : ""
                 );
    }
    GET_OBJ_CONDITION(obj) -= 1 + (power - rating) / half;
  }

  // if the end result is that the object condition rating is 0 or less
  // it is destroyed -- a good reason to keep objects in good repair
  if (GET_OBJ_CONDITION(obj) <= 0) {
    if (ch) {
      send_to_char(ch, "%s%s%s%s has been destroyed!\r\n",
                   ch == vict ? "^R" : "",
                   CAP(inside_buf),
                   GET_OBJ_NAME(obj),
                   ch == vict ? "^R" : "^n");
    }
    if (vict && vict != ch) {
      send_to_char(vict, "^R%s%s^R has been destroyed!^n\r\n",
                   CAP(inside_buf),
                   GET_OBJ_NAME(obj));
    }

    if (ch && !IS_NPC(ch) && GET_QUEST(ch)) {
      check_quest_destroy(ch, obj);
    }
    else if (ch && AFF_FLAGGED(ch, AFF_GROUP) && ch->master &&
             !IS_NPC(ch->master) && GET_QUEST(ch->master)) {
      check_quest_destroy(ch->master, obj);
    }

    // Log destruction.
    const char *representation = generate_new_loggable_representation(obj);
    snprintf(buf, sizeof(buf), "Destroying %s's %s due to combat damage. Representation: [%s]",
             vict ? GET_CHAR_NAME(vict) : "nobody",
             GET_OBJ_NAME(obj),
             representation);
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    delete [] representation;

    // Disgorge contents.
    switch (GET_OBJ_TYPE(obj)) {
      case ITEM_WEAPON:
      case ITEM_FIREWEAPON:
      case ITEM_CUSTOM_DECK:
      case ITEM_CYBERDECK:
      case ITEM_VEHCONTAINER:
      case ITEM_SHOPCONTAINER:
      case ITEM_PART:
        // We don't disgorge these. Doing so causes bugs.
        break;
      default:
        if (GET_OBJ_SPEC(obj) != pocket_sec) {
          for (temp = obj->contains; temp; temp = next) {
            next = temp->next_content;
            obj_from_obj(temp);
            if ((IS_OBJ_STAT(obj, ITEM_EXTRA_CORPSE) && !GET_OBJ_VAL(obj, 4) && GET_OBJ_TYPE(temp) != ITEM_MONEY)
                || GET_OBJ_VNUM(temp) == OBJ_POCKET_SECRETARY_FOLDER)
            {
              extract_obj(temp);
            } else if (vict)
              obj_to_char(temp, vict);
            else if (obj->in_room)
              obj_to_room(temp, obj->in_room);
            else
              extract_obj(temp);
          }
        }
        break;
    }

    extract_obj(obj);
  }
}

void docwagon_message(struct char_data *ch)
{
  char buf[MAX_STRING_LENGTH];

  switch (SECT(get_ch_in_room(ch)))
  {
    case SPIRIT_HEARTH:
      snprintf(buf, sizeof(buf),"A DocWagon employee suddenly appears, transports %s's body to\r\nsafety, and rushes away.", GET_NAME(ch));
      act(buf,FALSE, ch, 0, 0, TO_ROOM);
      snprintf(buf, sizeof(buf),"A DocWagon employee suddenly appears, transports %s's body to\r\nsafety, and rushes away.", GET_CHAR_NAME(ch));
      break;
    case SPIRIT_LAKE:
    case SPIRIT_RIVER:
    case SPIRIT_SEA:
      snprintf(buf, sizeof(buf),"A DocWagon armored speedboat arrives, loading %s's body on\r\nboard before leaving.", GET_NAME(ch));
      act(buf, FALSE, ch, 0, 0, TO_ROOM);
      snprintf(buf, sizeof(buf),"A DocWagon armored speedboat arrives, loading %s's body on\r\nboard before leaving.", GET_CHAR_NAME(ch));
      break;
    default:
      snprintf(buf, sizeof(buf),"A DocWagon helicopter flies in, taking %s's body to safety.", GET_NAME(ch));
      act(buf, FALSE, ch, 0, 0, TO_ROOM);
      snprintf(buf, sizeof(buf),"A DocWagon helicopter flies in, taking %s's body to safety.", GET_CHAR_NAME(ch));
      break;
  }

  mudlog(buf, ch, LOG_DEATHLOG, TRUE);
}

void docwagon_retrieve(struct char_data *ch) {
  struct room_data *room = get_ch_in_room(ch);

  if (!PLR_FLAGGED(ch, PLR_DOCWAGON_READY)) {
    mudlog("SYSERR: Got someone in docwagon_retrieve() who was not DOCWAGON_READY.", ch, LOG_SYSLOG, TRUE);
  }
  PLR_FLAGS(ch).RemoveBit(PLR_DOCWAGON_READY);

  if (FIGHTING(ch) && FIGHTING(FIGHTING(ch)) == ch)
    stop_fighting(FIGHTING(ch));
  if (CH_IN_COMBAT(ch))
    stop_fighting(ch);

  // Stop all their sustained spells as if they died.
  if (GET_SUSTAINED(ch)) {
    struct sustain_data *next;
    for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = next) {
      next = sust->next;
      if (next && sust->idnum == next->idnum)
        next = next->next;
      end_sustained_spell(ch, sust);
    }
  }

  // Remove them from the Matrix.
  if (ch->persona) {
    snprintf(buf, sizeof(buf), "%s depixelizes and vanishes from the host.\r\n", CAP(ch->persona->name));
    send_to_host(ch->persona->in_host, buf, ch->persona, TRUE);
    extract_icon(ch->persona);
    ch->persona = NULL;
    PLR_FLAGS(ch).RemoveBit(PLR_MATRIX);
  } else if (PLR_FLAGGED(ch, PLR_MATRIX))
    for (struct char_data *temp = room->people; temp; temp = temp->next_in_room)
      if (PLR_FLAGGED(temp, PLR_MATRIX))
        temp->persona->decker->hitcher = NULL;
  docwagon_message(ch);
  // death_penalty(ch);  /* Penalty for deadly wounds */

  // They just got patched up: heal them slightly, make them stunned.
  GET_PHYSICAL(ch) = 400;
  GET_MENTAL(ch) = 0;
  GET_POS(ch) = POS_STUNNED;

  // Restore their salvation ticks.
  GET_PC_SALVATION_TICKS(ch) = 5;

  // Disable bioware etc that resets on death.
  for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content) {
    switch (GET_BIOWARE_TYPE(bio)) {
      case BIO_ADRENALPUMP:
        if (GET_OBJ_VAL(bio, 5) > 0) {
          for (int i = 0; i < MAX_OBJ_AFFECT; i++)
            affect_modify(ch,
                          bio->affected[i].location,
                          bio->affected[i].modifier,
                          bio->obj_flags.bitvector, FALSE);
          GET_OBJ_VAL(bio, 5) = 0;
        }
        break;
      case BIO_PAINEDITOR:
        GET_BIOWARE_IS_ACTIVATED(bio) = 0;
        break;
    }
  }

  // Extinguish their fire, if any.
  ch->points.fire[0] = 0;

  send_to_char("\r\n\r\nYour last conscious memory is the arrival of a DocWagon.\r\n", ch);
  {
    rnum_t recovery_room = 0;

    switch (GET_JURISDICTION(room)) {
      case ZONE_SEATTLE:
        recovery_room = real_room(RM_SEATTLE_DOCWAGON);
        break;
      case ZONE_PORTLAND:
        recovery_room = real_room(RM_PORTLAND_DOCWAGON);
        break;
      case ZONE_CARIB:
        recovery_room = real_room(RM_CARIB_DOCWAGON);
        break;
      case ZONE_OCEAN:
        recovery_room = real_room(RM_OCEAN_DOCWAGON);
        break;
      default:
        mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Unknown jurisdiction %d encountered in docwagon_retrieve()! %s being sent to Dante's.", GET_JURISDICTION(room), GET_CHAR_NAME(ch));
        recovery_room = real_room(RM_ENTRANCE_TO_DANTES);
        break;
    }

    if (recovery_room < 0) {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid docwagon room specified for jurisdiction %d! %s being sent to A Bright Light.", GET_JURISDICTION(room), GET_CHAR_NAME(ch));
      recovery_room = 0;
    }

    char_from_room(ch);
    char_to_room(ch, &world[recovery_room]);
  }

  if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED) || GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD) {
    send_to_char("Your DocWagon rescue is free due to your newbie status, and you've been restored to full health.\r\n", ch);
    GET_PHYSICAL(ch) = 1000;
    GET_MENTAL(ch) = 1000;
    GET_POS(ch) = POS_STANDING;
  } else {
    struct obj_data *docwagon = find_best_active_docwagon_modulator(ch);

    // Compensate for edge case: Their modulator was destroyed since they were flagged.
    if (docwagon) {
      int dw_random_cost = number(8, 12) * 500 / GET_DOCWAGON_CONTRACT_GRADE(docwagon);
      int creds = MAX(dw_random_cost, (int)(GET_NUYEN(ch) / 10));

      send_to_char(ch, "DocWagon demands %d nuyen for your rescue.\r\n", creds);

      if ((GET_NUYEN(ch) + GET_BANK(ch)) < creds) {
        send_to_char("Not finding sufficient payment, your DocWagon contract was retracted.\r\n", ch);
        extract_obj(docwagon);
      }
      else if (GET_BANK(ch) < creds) {
        lose_nuyen(ch, creds - GET_BANK(ch), NUYEN_OUTFLOW_DOCWAGON);
        lose_bank(ch, GET_BANK(ch), NUYEN_OUTFLOW_DOCWAGON);
      }
      else {
        lose_bank(ch, creds, NUYEN_OUTFLOW_DOCWAGON);
      }
    }
  }

  if (PLR_FLAGGED(ch, PLR_SENT_DOCWAGON_PLAYER_ALERT)) {
    alert_player_doctors_of_contract_withdrawal(ch, FALSE);
    PLR_FLAGS(ch).RemoveBit(PLR_SENT_DOCWAGON_PLAYER_ALERT);
  }
}

bool docwagon(struct char_data *ch)
{
  struct obj_data *docwagon = NULL;
  char rollbuf[500];

  if (IS_NPC(ch))
    return FALSE;

  // Find the best docwagon contract they're wearing.
  if (!(docwagon = find_best_active_docwagon_modulator(ch)))
    return FALSE;

  struct room_data *room = get_ch_in_room(ch);

  if (!room)
    return FALSE;

  int docwagon_tn = MAX(GET_SECURITY_LEVEL(room), 4);
  int docwagon_dice = GET_DOCWAGON_CONTRACT_GRADE(docwagon) + 1;
  int successes = success_test(docwagon_dice, docwagon_tn);

  snprintf(rollbuf, sizeof(rollbuf), "$n: DocWagon rescue roll: %d dice vs TN %d netted %d hit(s).", docwagon_dice, docwagon_tn, successes);
  act(rollbuf, TRUE, ch, 0, 0, TO_ROLLS);

  if (successes <= 0 && access_level(ch, LVL_BUILDER) && PRF_FLAGGED(ch, PRF_PACIFY)) {
    act("$n: Overriding failed DocWagon roll due to pacified staff level.", TRUE, ch, 0, 0, TO_ROLLS);
    successes = 1;
  }

  // In an area with 4 or less security level: Basic has a 75% chance of rescue, Gold has 87.5% rescue, Plat has 93.8% chance.
  if (successes > 0)
  {
    send_to_char(ch, "%s^n chirps cheerily: an automated DocWagon trauma team is on its way!\r\n", CAP(GET_OBJ_NAME(docwagon)));
    send_to_char(ch, "^L[OOC: You can choose to wait for player assistance to arrive, or you can get picked up immediately by entering ^wDIE^L. See ^wHELP DOCWAGON^L for more details.]\r\n");
    PLR_FLAGS(ch).SetBit(PLR_DOCWAGON_READY);
  } else {
    send_to_char(ch, "%s^n vibrates, sending out a trauma call that will hopefully be answered.\r\n", CAP(GET_OBJ_NAME(docwagon)));
  }

  if ((ch->in_room || ch->in_veh) && !PRF_FLAGGED(ch, PRF_DONT_ALERT_PLAYER_DOCTORS_ON_MORT)) {
    int num_responders = alert_player_doctors_of_mort(ch, docwagon);
    if (num_responders > 0) {
      send_to_char(ch, "^L[OOC: There %s ^w%d^L player%s online who may be able to respond to your DocWagon call.]^n\r\n",
                   num_responders == 1 ? "is" : "are",
                   num_responders,
                   num_responders == 1 ? "" : "s");
    }
  }

  return FALSE;
}

// M&M p.63-64
// Mode 1 for activation, mode 0 for reducing duration in turns.
bool check_adrenaline(struct char_data *ch, int mode)
{
  int i, dam;
  struct obj_data *pump = NULL;
  bool early_activation = FALSE;

  for (pump = ch->bioware; pump && GET_BIOWARE_TYPE(pump) != BIO_ADRENALPUMP; pump = pump->next_content)
    ;
  if (!pump)
    return FALSE;
  if (mode && GET_BIOWARE_PUMP_ADRENALINE(pump) <= 0) {
    if (GET_BIOWARE_PUMP_ADRENALINE(pump) < 0)
      early_activation = TRUE;

    // We want to increase the pump duration without increasing the TN test so we're doing the duration roll in the
    // TN value directly and multiply that. Previous possible duration was 2-12 seconds per pump level and there was
    // a bug decreasing it twice too. Multiplying the roll x10 and we'll see how that pans out.
    GET_BIOWARE_PUMP_TEST_TN(pump) = GET_BIOWARE_RATING(pump) == 2 ? srdice() + srdice() : srdice();
    GET_BIOWARE_PUMP_ADRENALINE(pump)  = GET_BIOWARE_PUMP_TEST_TN(pump) * 10;
    // Reactivation before the adrenaline sack is completely full?
    if (early_activation)
      GET_BIOWARE_PUMP_ADRENALINE(pump) /= 2;
    if (!IS_JACKED_IN(ch))
      send_to_char("Your body is wracked with renewed vitality as adrenaline pumps into your bloodstream.\r\n", ch);
    for (i = 0; i < MAX_OBJ_AFFECT; i++)
      affect_modify(ch,
                    pump->affected[i].location,
                    pump->affected[i].modifier,
                    pump->obj_flags.bitvector, TRUE);
  }
  else if (GET_OBJ_VAL(pump, 5) > 0 && !mode) {
    GET_BIOWARE_PUMP_ADRENALINE(pump)--;
    if (GET_BIOWARE_PUMP_ADRENALINE(pump) == 0) {
      for (i = 0; i < MAX_OBJ_AFFECT; i++)
        affect_modify(ch,
                      pump->affected[i].location,
                      pump->affected[i].modifier,
                      pump->obj_flags.bitvector, FALSE);
      GET_BIOWARE_PUMP_ADRENALINE(pump) = -number(60, 90);
      if (!IS_JACKED_IN(ch))
        send_to_char("Your body softens and relaxes as the adrenaline wears off.\r\n", ch);
      dam = convert_damage(stage(-success_test(GET_BOD(ch) + GET_BODY(ch),
                                 (int)(GET_BIOWARE_PUMP_TEST_TN(pump) / 2)), DEADLY));
      GET_BIOWARE_PUMP_TEST_TN(pump) = 0;
      if (damage(ch, ch, dam, TYPE_BIOWARE, FALSE)) {
        // Died-- RIP
        return TRUE;
      }
    }
  } else if (!mode && GET_BIOWARE_PUMP_ADRENALINE(pump) < 0) {
    GET_BIOWARE_PUMP_ADRENALINE(pump)++;
    if (GET_BIOWARE_PUMP_ADRENALINE(pump) == 0)
      send_to_char("Your feel your adrenaline pump is full.\r\n", ch);
  }
  return FALSE;
}

#define WRITE_DEATH_MESSAGE(format_string) \
  snprintf(buf2, sizeof(buf2), format_string, GET_CHAR_NAME(vict), vict->in_room ? GET_ROOM_NAME(vict->in_room) : GET_VEH_NAME(vict->in_veh), GET_ROOM_VNUM(get_ch_in_room(vict)))
#define WRITE_DEATH_MESSAGE_WITH_AGGRESSOR(format_string) \
  snprintf(buf2, sizeof(buf2), format_string, GET_CHAR_NAME(vict), ch == vict ? "Misadventure(?)" : GET_NAME(ch), vict->in_room ? GET_ROOM_NAME(vict->in_room) : GET_VEH_NAME(vict->in_veh), GET_ROOM_VNUM(get_ch_in_room(vict)))
void gen_death_msg(struct char_data *ch, struct char_data *vict, int attacktype)
{
  switch (attacktype)
  {
    case TYPE_SUFFERING:
      switch (number(0, 4)) {
        case 0:
          WRITE_DEATH_MESSAGE("%s died (no blood, it seems). {%s (%ld)}");
          break;
        case 1:
          WRITE_DEATH_MESSAGE("%s's brain ran out of oxygen {%s (%ld)}");
          break;
        case 2:
          WRITE_DEATH_MESSAGE("%s simply ran out of blood. {%s (%ld)}");
          break;
        case 3:
          WRITE_DEATH_MESSAGE("%s unwittingly committed suicide. {%s (%ld)}");
          break;
        case 4:
          WRITE_DEATH_MESSAGE("Bloodlack stole %s's life. {%s (%ld)}");
          break;
      }
      break;
    case TYPE_EXPLOSION:
      switch (number(0, 4)) {
        case 0:
          snprintf(buf2, sizeof(buf2), "%s blew %s to pieces. {%s (%ld)}",
                  ch == vict ? "???" : GET_NAME(ch), GET_CHAR_NAME(vict),
                  vict->in_room ? GET_ROOM_NAME(vict->in_room) : GET_VEH_NAME(vict->in_veh),
                  GET_ROOM_VNUM(get_ch_in_room(vict)));
          break;
        case 1:
          WRITE_DEATH_MESSAGE_WITH_AGGRESSOR("%s was blown to bits. [%s] {%s (%ld)}");
          break;
        case 2:
          WRITE_DEATH_MESSAGE_WITH_AGGRESSOR("%s got vaporized by %s. {%s (%ld)}");
          break;
        case 3:
          WRITE_DEATH_MESSAGE_WITH_AGGRESSOR("%s was incinerated by an explosion. [%s] {%s (%ld)}");
          break;
        case 4:
          WRITE_DEATH_MESSAGE_WITH_AGGRESSOR("%s got blown to hell by %s. {%s (%ld)}");
          break;
      }
      break;
    case TYPE_SCATTERING:
      switch (number(0, 4)) {
        case 0:
          WRITE_DEATH_MESSAGE_WITH_AGGRESSOR("%s accidentally (?) killed by %s. {%s (%ld)}");
          break;
        case 1:
          WRITE_DEATH_MESSAGE_WITH_AGGRESSOR("Shouldn't have been standing there, should you %s? [%s] {%s (%ld)}");
          break;
        case 2:
          snprintf(buf2, sizeof(buf2), "Oops....%s just blew %s's head off. {%s (%ld)}",
                  ch == vict ? "???" : GET_NAME(ch), GET_CHAR_NAME(vict),
                  vict->in_room ? GET_ROOM_NAME(vict->in_room) : GET_VEH_NAME(vict->in_veh),
                  GET_ROOM_VNUM(get_ch_in_room(vict)));
          break;
        case 3:
          snprintf(buf2, sizeof(buf2), "%s's stray bullet caught %s in the heart. What a shame. {%s (%ld)}",
                  ch == vict ? "???" : GET_NAME(ch), GET_CHAR_NAME(vict),
                  vict->in_room ? GET_ROOM_NAME(vict->in_room) : GET_VEH_NAME(vict->in_veh),
                  GET_ROOM_VNUM(get_ch_in_room(vict)));
          break;
        case 4:
          WRITE_DEATH_MESSAGE_WITH_AGGRESSOR("A random bullet killed a random person -- poor %s. [%s] {%s (%ld)}");
          break;
      }
      break;
    case TYPE_FALL:
      switch (number(0, 4)) {
        case 0:
          WRITE_DEATH_MESSAGE("%s died on impact. {%s (%ld)}");
          break;
        case 1:
          WRITE_DEATH_MESSAGE("%s failed to miss the ground. {%s (%ld)}");
          break;
        case 2:
          WRITE_DEATH_MESSAGE("Life's a bitch, %s.  So's concrete. {%s (%ld)}");
          break;
        case 3:
          WRITE_DEATH_MESSAGE("What %s would have given for a safety net... {%s (%ld)}");
          break;
        case 4:
          WRITE_DEATH_MESSAGE("The ground can be such an unforgiving thing. Right, %s? {%s (%ld)}");
          break;
      }
      break;
    case TYPE_DROWN:
      switch (number(0, 4)) {
        case 0:
          WRITE_DEATH_MESSAGE("%s drank way, way, WAY too much. {%s (%ld)}");
          break;
        case 1:
          WRITE_DEATH_MESSAGE("%s sleeps with the fishes. Involuntarily. {%s (%ld)}");
          break;
        case 2:
          WRITE_DEATH_MESSAGE("The water had a fight with %s's lungs. The water won. {%s (%ld)}");
          break;
        case 3:
          WRITE_DEATH_MESSAGE("Water doesn't seem so harmless now, does it %s? {%s (%ld)}");
          break;
        case 4:
          WRITE_DEATH_MESSAGE("%s didn't float. {%s (%ld)}");
          break;
      }
      break;
    case TYPE_BIOWARE:
      switch (number(0, 4)) {
        case 0:
          snprintf(buf2, sizeof(buf2), "%s just hasn't been taking %s medication.  Oops. "
                  "{%s (%ld)}", GET_CHAR_NAME(vict), GET_SEX(vict) == SEX_MALE ?
                  "his" : (GET_SEX(vict) == SEX_FEMALE ? "her" : "its"),
                  vict->in_room ? GET_ROOM_NAME(vict->in_room) : GET_VEH_NAME(vict->in_veh),
                  GET_ROOM_VNUM(get_ch_in_room(vict)));
          break;
        case 1:
          WRITE_DEATH_MESSAGE("%s was killed in a bioware rebellion. {%s (%ld)}");
          break;
        case 2:
          WRITE_DEATH_MESSAGE("%s had a fatal heart attack.  Wuss. {%s (%ld)}");
          break;
        case 3:
          WRITE_DEATH_MESSAGE("Still think the bioware was worth it, %s? {%s (%ld)}");
          break;
        case 4:
          WRITE_DEATH_MESSAGE("Maybe %s got a defective piece of bioware... {%s (%ld)}");
          break;
      }
      break;
    case TYPE_DRUGS:
      switch (number(0, 1)) {
        case 0:
          WRITE_DEATH_MESSAGE("%s failed to graduate from the school of hard knocks. {%s (%ld)}");
          break;
        case 1:
          WRITE_DEATH_MESSAGE("%s didn't survive the drugs. {%s (%ld)}");
          break;
      }
      break;
    case TYPE_RECOIL:
      switch (number(0, 4)) {
        case 0:
          WRITE_DEATH_MESSAGE("You're meant to hit *other* people with that whip, %s. {%s (%ld)}");
          break;
        case 1:
          snprintf(buf2, sizeof(buf2), "%s lopped off %s own head.  Oops. {%s (%ld)}",
                  GET_CHAR_NAME(vict), HSHR(vict),
                  vict->in_room ? GET_ROOM_NAME(vict->in_room) : GET_VEH_NAME(vict->in_veh),
                  GET_ROOM_VNUM(get_ch_in_room(vict)));
          break;
        case 2:
          WRITE_DEATH_MESSAGE("%s, watch out for your whi....nevermind. {%s (%ld)}");
          break;
        case 3:
          WRITE_DEATH_MESSAGE("THWAP!  Wait.....was that *your* whip, %s?!? {%s (%ld)}");
          break;
        case 4:
          snprintf(buf2, sizeof(buf2), "%s's whip didn't agree with %s. {%s (%ld)}",
                  GET_CHAR_NAME(vict), HMHR(vict),
                  vict->in_room ? GET_ROOM_NAME(vict->in_room) : GET_VEH_NAME(vict->in_veh),
                  GET_ROOM_VNUM(get_ch_in_room(vict)));
          break;
      }
      break;
    case TYPE_RAM:
      WRITE_DEATH_MESSAGE_WITH_AGGRESSOR("%s was made into a hood ornament by %s. {%s (%ld)}");
      break;
    case TYPE_DUMPSHOCK:
      WRITE_DEATH_MESSAGE("%s couldn't quite manage a graceful logoff. {%s (%ld)}");
      break;
    case TYPE_BLACKIC:
      WRITE_DEATH_MESSAGE("%s couldn't haxor the gibson. {%s (%ld)}");
      break;
    case TYPE_POLTERGEIST:
      WRITE_DEATH_MESSAGE("%s is a pansy who got killed by the poltergeist spell. {%s (%ld)}");
      break;
    case TYPE_ELEVATOR:
      WRITE_DEATH_MESSAGE("%s got crushed by a moving elevator. Ouch. {%s (%ld)}");
      break;
    case TYPE_MEDICAL_MISHAP:
      WRITE_DEATH_MESSAGE("%s inadvertently starred in a medical-themed snuff film. {%s (%ld)}");
      break;
    case TYPE_SPELL_DRAIN:
      WRITE_DEATH_MESSAGE("%s turned themselves into a Mana bonfire. {%s (%ld)}");
      break;
    case TYPE_CRASH:
      switch(number(0, 1)) {
        case 0:
          snprintf(buf2, sizeof(buf2), "%s forgot to wear %s seatbelt. {%s (%ld)}",
                  GET_CHAR_NAME(vict), HSHR(vict),
                  vict->in_room ? GET_ROOM_NAME(vict->in_room) : GET_VEH_NAME(vict->in_veh),
                  GET_ROOM_VNUM(get_ch_in_room(vict)));
          break;
        case 1:
          WRITE_DEATH_MESSAGE("%s become one with the dashboard. {%s (%ld)}");
          break;
      }
      break;
    case TYPE_POISON:
      WRITE_DEATH_MESSAGE("%s drank themself to death. {%s (%ld)}");
      break;
    default:
      if (ch == vict)
        snprintf(buf2, sizeof(buf2), "%s died (cause uncertain-- damage type %d). {%s (%ld)}",
                GET_CHAR_NAME(vict), attacktype,
                vict->in_room ? GET_ROOM_NAME(vict->in_room) : GET_VEH_NAME(vict->in_veh),
                GET_ROOM_VNUM(get_ch_in_room(vict)));
      else
        switch (number(0, 5)) {
          case 0:
            WRITE_DEATH_MESSAGE_WITH_AGGRESSOR("%s killed by %s. {%s (%ld)}");
            break;
          case 1:
            WRITE_DEATH_MESSAGE_WITH_AGGRESSOR("%s wiped out of existence by %s. {%s (%ld)}");
            break;
          case 2:
            WRITE_DEATH_MESSAGE_WITH_AGGRESSOR("%s's life terminated by %s. {%s (%ld)}");
            break;
          case 3:
            WRITE_DEATH_MESSAGE_WITH_AGGRESSOR("%s flatlined by %s. {%s (%ld)}");
            break;
          case 4:
            WRITE_DEATH_MESSAGE_WITH_AGGRESSOR("%s transformed into a corpse by %s. {%s (%ld)}");
            break;
          case 5:
            WRITE_DEATH_MESSAGE_WITH_AGGRESSOR("%s got geeked by %s. {%s (%ld)}");
            break;
        }
      break;
  }

  mudlog(buf2, vict, LOG_DEATHLOG, TRUE);
}

#define IS_RANGED(eq)   (GET_OBJ_TYPE(eq) == ITEM_FIREWEAPON || \
(GET_OBJ_TYPE(eq) == ITEM_WEAPON && \
(IS_GUN(GET_OBJ_VAL(eq, 3)))))

#define RANGE_OK(ch) ((GET_EQ(ch, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_WEAPON && \
IS_RANGED(GET_EQ(ch, WEAR_WIELD))) || (GET_EQ(ch, WEAR_HOLD) && \
GET_OBJ_TYPE(GET_EQ(ch, WEAR_HOLD)) == ITEM_WEAPON && IS_RANGED(GET_EQ(ch, WEAR_HOLD))))

bool would_become_killer(struct char_data * ch, struct char_data * vict)
{
  char_data *attacker;

  if (IS_NPC(ch) && (ch->desc == NULL || ch->desc->original == NULL))
    return FALSE;

  if (!IS_NPC(ch))
    attacker = ch;
  else
    attacker = ch->desc->original;

  if (!IS_NPC(vict) &&
      !PLR_FLAGS(vict).AreAnySet(PLR_KILLER, ENDBIT) &&
      !(ROOM_FLAGGED(get_ch_in_room(ch), ROOM_ARENA) && ROOM_FLAGGED(get_ch_in_room(vict), ROOM_ARENA)) &&
      (!PRF_FLAGGED(attacker, PRF_PKER) || !PRF_FLAGGED(vict, PRF_PKER)) &&
      !PLR_FLAGGED(attacker, PLR_KILLER) && attacker != vict && !IS_SENATOR(attacker))
  {
    return TRUE;
  }
  return FALSE;
}

// Basically ripped the logic from damage(). Used to adjust combat messages for edge cases.
bool can_hurt(struct char_data *ch, struct char_data *victim, int attacktype, bool include_func_protections) {
  // Corpse protection.
  if (GET_POS(victim) <= POS_DEAD)
    return FALSE;

  // You can always hurt yourself. :dead-eyed stare:
  if (ch == victim)
    return TRUE;

  if (IS_NPC(victim)) {
    // Nokill protection.
    if (MOB_FLAGGED(victim, MOB_NOKILL))
      return false;

    // Quest target protection.
    if (!IS_NPC(ch) && victim->mob_specials.quest_id && victim->mob_specials.quest_id != GET_IDNUM_EVEN_IF_PROJECTING(ch)) {
      // Aggro mobs don't get this protection.
      if (!mob_is_aggressive(victim, TRUE)) {
        // If grouped, check to see if anyone in the group is the mob's owner.
        if (AFF_FLAGGED(ch, AFF_GROUP)) {
          bool found_group_member = FALSE;
          for (struct follow_type *f = ch->followers; f && found_group_member; f = f->next)
            if (!IS_NPC(f->follower)
                && AFF_FLAGGED(f->follower, AFF_GROUP)
                && GET_IDNUM(f->follower) == victim->mob_specials.quest_id)
              found_group_member = TRUE;

          // How about the group master?
          if (!found_group_member && ch->master && !IS_NPC(ch->master)) {
            found_group_member = victim->mob_specials.quest_id == GET_IDNUM(ch->master);
          }

          if (!found_group_member)
            return false;
        } else
          return false;
      }
    }

    // Special NPC protection.
    if (include_func_protections && npc_is_protected_by_spec(victim)) {
      return false;
    }

    // If ch is staff lower than admin, or ch is a charmie of a staff lower than admin, and the target is connected, no kill.
    if (( (!IS_NPC(ch)
          && IS_SENATOR(ch)
          && !access_level(ch, LVL_ADMIN))
        || (IS_NPC(ch)
            && ch->master
            && AFF_FLAGGED(ch, AFF_CHARM)
            && !IS_NPC(ch->master)
            && IS_SENATOR(ch->master)
            && !access_level(ch->master, LVL_ADMIN)))
        && IS_NPC(victim)
        && !vnum_from_non_connected_zone(GET_MOB_VNUM(victim)))
    {
      return false;
    }

    // Some things can't be exploded.
    if (attacktype == TYPE_EXPLOSION && (IS_ASTRAL(victim) || MOB_FLAGGED(victim, MOB_IMMEXPLODE)))
      return false;

  } else {
#ifdef IGNORING_IC_ALSO_IGNORES_COMBAT
  if (IS_IGNORING(victim, is_blocking_ic_interaction_from, ch))
    return FALSE;

  if (IS_IGNORING(ch, is_blocking_ic_interaction_from, victim))
    return FALSE;
#endif

    // Known ignored edge case: if the player is not a killer but would become a killer because of this action.
    if (ch != victim && would_become_killer(ch, victim))
      return FALSE;

    if (PLR_FLAGGED(ch, PLR_KILLER) && !IS_NPC(victim))
      return FALSE;
  }

  // Looks like ch can hurt vict after all.
  return TRUE;
}

bool damage(struct char_data *ch, struct char_data *victim, int dam, int attacktype, bool is_physical) {
  return raw_damage(ch, victim, dam, attacktype, is_physical, TRUE);
}

bool damage_without_message(struct char_data *ch, struct char_data *victim, int dam, int attacktype, bool is_physical) {
  return raw_damage(ch, victim, dam, attacktype, is_physical, FALSE);
}

// return 1 if victim died, 0 otherwise
bool raw_damage(struct char_data *ch, struct char_data *victim, int dam, int attacktype, bool is_physical, bool send_message)
{
  char rbuf[MAX_STRING_LENGTH];
  memset(rbuf, 0, sizeof(rbuf));

  int exp;
  bool total_miss = FALSE, awake = TRUE, did_docwagon = FALSE;
  struct obj_data *bio;
  ACMD_DECLARE(do_disconnect);
  ACMD_CONST(do_return);

  if (!ch) {
    mudlog("SYSERR: NULL ch passed to damage()! Aborting call, nobody takes damage this time.", victim, LOG_SYSLOG, TRUE);
    return FALSE;
  }
  if (!victim) {
    mudlog("SYSERR: NULL victim passed to damage()! Aborting call, nobody takes damage this time.", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (GET_POS(victim) == POS_DEAD) {
    mudlog("SYSERR: Refusing to damage an already-dead victim!", victim, LOG_SYSLOG, TRUE);
    return TRUE;
  }

  if (!can_hurt(ch, victim, attacktype, TRUE)) {
    dam = -1;
    buf_mod(rbuf, sizeof(rbuf), "Can'tHurt",dam);
    if (ch)
      send_to_char(ch, "^m(OOC Notice: You are unable to damage %s.)^n\r\n", GET_CHAR_NAME(victim));
  }

  if (ch == victim) {
    if (attacktype == TYPE_SPELL_DRAIN) {
      snprintf(rbuf, sizeof(rbuf), "Drain damage (%s: ", GET_CHAR_NAME(ch));
    } else if (attacktype == TYPE_DRUGS) {
      snprintf(rbuf, sizeof(rbuf), "Drug damage (%s: ", GET_CHAR_NAME(ch));
    } else if (attacktype == TYPE_BIOWARE) {
      snprintf(rbuf, sizeof(rbuf), "Bioware damage (%s: ", GET_CHAR_NAME(ch));
    } else if (attacktype == TYPE_POISON) {
      snprintf(rbuf, sizeof(rbuf), "Poison damage (%s: ", GET_CHAR_NAME(ch));
    } else {
      snprintf(rbuf, sizeof(rbuf), "Self-damage (%s: ", GET_CHAR_NAME(ch));
    }
  } else {
    snprintf(rbuf, sizeof(rbuf), "Damage (%s to %s: ", GET_CHAR_NAME(ch), GET_CHAR_NAME(victim));
  }

  if (attacktype >= TYPE_HIT && attacktype <= TYPE_BLACKIC)
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "%d %s boxes from %s): ",
            dam, is_physical ? "physical" : "mental", damage_type_names_must_subtract_300_first_and_must_not_be_greater_than_blackic[attacktype - 300]);
  else
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "%d %s boxes from type %d): ",
            dam, is_physical ? "physical" : "mental", attacktype);

  if (victim != ch)
  {
#ifdef IGNORING_IC_ALSO_IGNORES_COMBAT
    if (ch != victim && IS_IGNORING(victim, is_blocking_ic_interaction_from, ch)) {
      char oopsbuf[5000];
      snprintf(oopsbuf, sizeof(oopsbuf), "SUPER SYSERR: Somehow, we made it all the way to damage() with the victim ignoring the attacker! "
                                         "%s ch, %s victim, %d dam, %d attacktype, %d is_physical, %d send_message.",
                                         GET_CHAR_NAME(ch), GET_CHAR_NAME(victim), dam, attacktype, is_physical, send_message
                                       );
      mudlog(oopsbuf, ch, LOG_SYSLOG, TRUE);
      return 0;
    }

    if (ch != victim && IS_IGNORING(ch, is_blocking_ic_interaction_from, victim)) {
      char oopsbuf[5000];
      snprintf(oopsbuf, sizeof(oopsbuf), "SUPER SYSERR: Somehow, we made it all the way to damage() with the attacker ignoring the victim! "
                                         "%s ch, %s victim, %d dam, %d attacktype, %d is_physical, %d send_message.",
                                         GET_CHAR_NAME(ch), GET_CHAR_NAME(victim), dam, attacktype, is_physical, send_message
                                       );
      mudlog(oopsbuf, ch, LOG_SYSLOG, TRUE);
      return 0;
    }
#endif

    if (GET_POS(ch) > POS_STUNNED && attacktype < TYPE_SUFFERING) {
      if (!FIGHTING(ch) && !ch->in_veh)
        set_fighting(ch, victim);

      if (!IS_NPC(ch) && IS_NPC(victim) && victim->master && !number(0, 10) &&
          IS_AFFECTED(victim, AFF_CHARM) && (victim->master->in_room == ch->in_room) &&
          !(ch->master && ch->master == victim->master)) {
        if (CH_IN_COMBAT(ch))
          stop_fighting(ch);
        set_fighting(ch, victim->master);
        if (!FIGHTING(victim->master))
          set_fighting(victim->master, ch);
        return 0;
      }
    }
    if (GET_POS(victim) > POS_STUNNED && !FIGHTING(victim)) {
      if (victim->in_room == ch->in_room)
        set_fighting(victim, ch);
      if (MOB_FLAGGED(victim, MOB_MEMORY) && !IS_NPC(ch) &&
          (!IS_SENATOR(ch)))
        remember(victim, ch);
    }
  }

  if (victim->master && victim->master == ch)
    stop_follower(victim);

  if (attacktype != TYPE_SPELL_DRAIN) {
    if (IS_AFFECTED(ch, AFF_HIDE))
      appear(ch);

    /* stop sneaking if it's the case */
    if (IS_AFFECTED(ch, AFF_SNEAK))
      AFF_FLAGS(ch).RemoveBit(AFF_SNEAK);
  }

  if (ch != victim) {
    if (PLR_FLAGGED(victim, PLR_PROJECT)) {
      do_return(victim, "", 0, 0);
      WAIT_STATE(victim, PULSE_VIOLENCE);
    }

    if (attacktype == TYPE_SCATTERING && !IS_NPC(ch) && !IS_NPC(victim)) {
      // Unless both chars are PK, or victim is KILLER, deal no damage and abort without flagging anyone as KILLER.
      if (!(PLR_FLAGGED(victim, PLR_KILLER) || (PRF_FLAGGED(ch, PRF_PKER) && PRF_FLAGGED(victim, PRF_PKER)))) {
        dam = -1;
        buf_mod(rbuf, sizeof(rbuf), "accidental", dam);
      } else {
        check_killer(ch, victim);
      }
    } else if (attacktype != TYPE_MEDICAL_MISHAP) {
      check_killer(ch, victim);
    }

    if (PLR_FLAGGED(ch, PLR_KILLER) && !IS_NPC(victim))
    {
      dam = -1;
      buf_mod(rbuf, sizeof(rbuf), "No-PK",dam);
    }
  }

  if (attacktype == TYPE_EXPLOSION && (IS_ASTRAL(victim) || MOB_FLAGGED(victim, MOB_IMMEXPLODE)))
  {
    dam = -1;
    buf_mod(rbuf, sizeof(rbuf), "ImmExplode",dam);
  }

  if (dam == 0)
    strlcat(rbuf, " 0", sizeof(rbuf));
  else if (dam > 0)
    buf_mod(rbuf, sizeof(rbuf), "", dam);

  act(rbuf, 0, ch, 0, victim, TO_ROLLS);

  if (dam == -1)
  {
    total_miss = TRUE;
  }

  if (ch != victim && !IS_NPC(victim) && !(victim->desc))
  {
    if (!CH_IN_COMBAT(victim)) {
      act("$n is rescued by divine forces.", FALSE, victim, 0, 0, TO_ROOM);
      GET_WAS_IN(victim) = victim->in_room;
      GET_PHYSICAL(victim) = MAX(100, GET_PHYSICAL(victim) -
                                 (int)(GET_MAX_PHYSICAL(victim) / 2));
      GET_MENTAL(victim) = MAX(100, GET_MENTAL(victim) -
                               (int)(GET_MAX_MENTAL(victim) / 2));
      char_from_room(victim);
      char_to_room(victim, 0);
    }
    return 0;
  }

  int comp = 0;
  bool trauma = FALSE, pain = FALSE;

  // We want to pull bioware from their real body-- if they're projecting, find their original.
  struct char_data *real_body = victim;
  if (IS_PROJECT(victim) && victim->desc && victim->desc->original)
    real_body = victim->desc->original;

  if (attacktype != TYPE_BIOWARE && attacktype != TYPE_DRUGS && attacktype != TYPE_POISON) {
    for (bio = real_body->bioware; bio; bio = bio->next_content) {
      if (GET_BIOWARE_TYPE(bio) == BIO_PLATELETFACTORY && dam >= 3 && is_physical)
        dam--;
      else if (GET_BIOWARE_TYPE(bio) == BIO_DAMAGECOMPENSATOR)
        comp = GET_BIOWARE_RATING(bio) * 100;
      else if (GET_BIOWARE_TYPE(bio) == BIO_TRAUMADAMPER && GET_MENTAL(real_body) >= 100)
        trauma = TRUE;
      else if (GET_BIOWARE_TYPE(bio) == BIO_PAINEDITOR && GET_BIOWARE_IS_ACTIVATED(bio))
        pain = TRUE;
    }
  }

  // Pain editors disable trauma dampers (M&M p75).
  if (pain) {
    act("(damper disabled - pedit)", FALSE, victim, NULL, NULL, TO_ROLLS);
    trauma = FALSE;
  }

  // Damage compensators disable trauma dampers unless the character is being damaged past the DC's ability to compensate.
  int current_health = is_physical ? GET_PHYSICAL(real_body) : GET_MENTAL(real_body);
  int expected_total_damage = (dam * 100) + (1000 - current_health);
  if (comp && expected_total_damage <= comp) {
    act("(damper disabled - dcomp)", FALSE, victim, NULL, NULL, TO_ROLLS);
    trauma = FALSE;
  }

  if (GET_PHYSICAL(victim) <= 0)
    awake = FALSE;

  if (dam > 0) {
    // Physical damage. This one's simple-- no overflow to deal with.
    if (is_physical) {
      GET_PHYSICAL(real_body) -= MAX(dam * 100, 0);
      AFF_FLAGS(real_body).SetBit(AFF_DAMAGED);
      if (trauma) {
        if (GET_MENTAL(real_body) >= 100) {
          GET_PHYSICAL(real_body) += 100;
          GET_MENTAL(real_body) -= 100;
          act("(trauma damper: +1 phys -1 ment)", FALSE, victim, NULL, NULL, TO_ROLLS);
        } else {
          act("(damper disabled: not enough ment to flow into)", FALSE, victim, NULL, NULL, TO_ROLLS);
        }
      }
      AFF_FLAGS(real_body).SetBit(AFF_DAMAGED);

      if (real_body != victim) {
        GET_PHYSICAL(victim) -= MAX(dam * 100, 0);
        AFF_FLAGS(victim).SetBit(AFF_DAMAGED);
      }
    }

    // Mental damage. We need to calculate overflow here.
    else {
      GET_MENTAL(real_body) -= MAX(dam * 100, 0);
      AFF_FLAGS(real_body).SetBit(AFF_DAMAGED);
      if (trauma) {
        GET_MENTAL(real_body) += 100;
        act("(trauma damper: +1 ment)", FALSE, victim, NULL, NULL, TO_ROLLS);
      }
      AFF_FLAGS(real_body).SetBit(AFF_DAMAGED);

      // Handle overflow for the physical body.
      if (GET_MENTAL(real_body) < 0) {
        int overflow = -GET_MENTAL(real_body);
        GET_MENTAL(real_body) = 0;
        GET_PHYSICAL(real_body) -= overflow;
      }

      if (real_body != victim) {
        GET_MENTAL(victim) -= MAX(dam * 100, 0);
        AFF_FLAGS(victim).SetBit(AFF_DAMAGED);

        // Handle overflow for the projection.
        if (GET_MENTAL(victim) < 0) {
          int overflow = -GET_MENTAL(victim);
          GET_MENTAL(victim) = 0;
          GET_PHYSICAL(victim) -= overflow;
        }
      }
    }

    // Under Hyper, you take 50% more damage as stun damage, rounded up.
    if (GET_DRUG_STAGE(victim, DRUG_HYPER) == DRUG_STAGE_ONSET && (attacktype != TYPE_DRUGS && attacktype != TYPE_BIOWARE && attacktype != TYPE_POISON)) {
      if (damage_without_message(victim, victim, dam / 2 + dam % 2, TYPE_DRUGS, FALSE)) {
        return TRUE;
      }
    }

    // Drug damage can't kill you.
    if (attacktype == TYPE_DRUGS) {
      GET_PHYSICAL(real_body) = MAX(GET_PHYSICAL(real_body), 100);
    }
  }
  if (!awake && GET_PHYSICAL(victim) <= 0)
    GET_LAST_DAMAGETIME(victim) = time(0);

  if (update_pos(victim)) {
    // They died from dumpshock.
    return TRUE;
  }

  if (GET_SUSTAINED_NUM(victim))
  {
    struct sustain_data *next;
    if (GET_POS(victim) < POS_LYING) {
      for (struct sustain_data *sust = GET_SUSTAINED(victim); sust; sust = next) {
        next = sust->next;
        if (sust->caster && !sust->focus && !sust->spirit) {
          if (next && sust->idnum == next->idnum)
            next = next->next;
          end_sustained_spell(victim, sust);
        }
      }
    } else if (dam > 0 && attacktype != TYPE_SPELL_DRAIN) {
      for (struct sustain_data *sust = GET_SUSTAINED(victim); sust; sust = next) {
        next = sust->next;
        if (sust->caster && !sust->focus && !sust->spirit)
          if (success_test(GET_SKILL(victim, SKILL_SORCERY), sust->force + damage_modifier(victim, buf, sizeof(buf))) < 1)
            end_sustained_spell(victim, sust);
      }
    }
  }
  /*
   * skill_message sends a message from the messages file in lib/misc.
   * dam_message just sends a generic "You hit $n extremely hard.".
   * skill_message is preferable to dam_message because it is more
   * descriptive.
   *
   * If we are _not_ attacking with a weapon (i.e. a spell), always use
   * skill_message. If we are attacking with a weapon: If this is a miss or a
   * death blow, send a skill_message if one exists; if not, default to a
   * dam_message. Otherwise, always send a dam_message.
   */
   if (send_message) {
     if (attacktype > 0 && ch != victim && attacktype < TYPE_HAND_GRENADE)
     {
       if (!IS_WEAPON(attacktype)) {
         skill_message(dam, ch, victim, attacktype);
       } else {
         if (GET_POS(victim) == POS_DEAD) {
           if (!skill_message(dam, ch, victim, attacktype))
             dam_message(dam, ch, victim, attacktype);
         } else
           dam_message(dam, ch, victim, attacktype);
       }
     }
     else if (dam > 0 && attacktype == TYPE_FALL) {
       if (dam > 5) {
         send_to_char(victim, "^RYou slam into the %s at speed, the impact reshaping your body in ways you do not appreciate.^n\r\n",
                      ROOM_FLAGGED(get_ch_in_room(victim), ROOM_INDOORS) ? "floor" : "ground");
         snprintf(buf3, sizeof(buf3), "^R$n slams into the %s at speed, the impact reshaping $s body in horrific ways.^n\r\n", ROOM_FLAGGED(get_ch_in_room(victim), ROOM_INDOORS) ? "floor" : "ground");
         act(buf3, FALSE, victim, 0, 0, TO_ROOM);
       } else {
         send_to_char(victim, "^rYou hit the %s with bruising force.^n\r\n", ROOM_FLAGGED(get_ch_in_room(victim), ROOM_INDOORS) ? "floor" : "ground");
         snprintf(buf3, sizeof(buf3), "^r$n hits the %s with bruising force.^n\r\n", ROOM_FLAGGED(get_ch_in_room(victim), ROOM_INDOORS) ? "floor" : "ground");
         act(buf3, FALSE, victim, 0, 0, TO_ROOM);
       }
     }
     else if (attacktype == TYPE_SPELL_DRAIN) {
       if ((GET_POS(ch) <= POS_STUNNED) && (GET_POS(ch) > POS_DEAD)) {
         send_to_char("You are unable to resist the strain of channeling mana and fall unconscious!\r\n", ch);
         act("$n collapses unconscious!", FALSE, ch, 0, 0, TO_ROOM);
       } else if (GET_POS(ch) == POS_DEAD) {
         send_to_char("Unchecked mana overloads your body with energy, killing you...\r\n", ch);
         act("$n suddenly collapses, dead!", FALSE, ch, 0, 0, TO_ROOM);
       }
     }
   }

  if ((ch->in_room != victim->in_room) && total_miss && attacktype < TYPE_SUFFERING &&
      attacktype >= TYPE_PISTOL)
    weapon_scatter(ch, victim, GET_EQ(ch, WEAR_WIELD));

  if (victim->bioware && GET_POS(victim) > POS_STUNNED && dam > 0 && ch != victim)
    if (check_adrenaline(victim, 1))
      return TRUE;

  if (ch != victim && dam > 0) {
    switch (attacktype) {
      case TYPE_HIT:
      case TYPE_STING:
      case TYPE_WHIP:
      case TYPE_SLASH:
      case TYPE_BITE:
      case TYPE_BLUDGEON:
      case TYPE_CRUSH:
      case TYPE_POUND:
      case TYPE_CLAW:
      case TYPE_MAUL:
      case TYPE_GORE:
      case TYPE_PIERCE:
      case TYPE_PUNCH:
      case TYPE_STAB:
      case TYPE_TASER:
      case TYPE_SHURIKEN:
      case TYPE_THROWING_KNIFE:
      case TYPE_ARROW:
      case TYPE_HAND_GRENADE:
      case TYPE_GRENADE_LAUNCHER:
      case TYPE_ROCKET:
      case TYPE_PISTOL:
      case TYPE_BLAST:
      case TYPE_RIFLE:
      case TYPE_SHOTGUN:
      case TYPE_MACHINE_GUN:
      case TYPE_CANNON:
      case TYPE_BIFURCATE:
      case TYPE_CRASH:
      case TYPE_EXPLOSION:
      case TYPE_SCATTERING:
      case TYPE_FALL:
      case TYPE_RECOIL:
      case TYPE_RAM:
      case TYPE_POLTERGEIST:
      case TYPE_ELEVATOR:
      case TYPE_MEDICAL_MISHAP:
      case TYPE_FIRE:
      case TYPE_ACID:
      case TYPE_POWERBOLT:
      case TYPE_MANIPULATION_SPELL:
        damage_equip(ch, victim, dam, attacktype);
        break;
      case TYPE_DUMPSHOCK:
      case TYPE_BLACKIC:
      case TYPE_SUFFERING:
      case TYPE_DROWN:
      case TYPE_ALLERGY:
      case TYPE_BIOWARE:
      case TYPE_DRAIN:
      case TYPE_FUMES:
      case TYPE_SPELL_DRAIN:
      case TYPE_DRUGS:
      case TYPE_POISON:
      case TYPE_MANABOLT_OR_STUNBOLT:
        // These types do not risk equipment damage.
        break;
      default:
        {
          char oopsbuf[500];
          snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Unknown damtype %d in check for equipment damage", attacktype);
          mudlog(oopsbuf, ch, LOG_SYSLOG, TRUE);
        }
        break;
    }
  }


  /* Use send_to_char -- act() doesn't send message if you are DEAD. */
  bool unbonded_docwagon = FALSE;
  struct obj_data *docwagon_biomonitor = NULL;
  switch (GET_POS(victim))
  {
    case POS_MORTALLYW:
      if (IS_NPC(victim) && MOB_FLAGGED(victim, MOB_INANIMATE)) {
        act("$n is critically damaged, and will fail soon, if not aided.",
            TRUE, victim, 0, 0, TO_ROOM);
      } else {
        act("$n is mortally wounded, and will die soon, if not aided.",
            TRUE, victim, 0, 0, TO_ROOM);
      }
      if (!PLR_FLAGGED(victim, PLR_MATRIX)) {
        send_to_char("You are mortally wounded, and will die soon, if not "
                     "aided.\r\n", victim);
      }
      if (!IS_NPC(victim))
        did_docwagon = docwagon(victim);
      break;
    case POS_STUNNED:
      if (IS_NPC(victim) && MOB_FLAGGED(victim, MOB_INANIMATE)) {
        act("$n is rebooting from heavy damage.",
            TRUE, victim, 0, 0, TO_ROOM);
      } else {
        act("$n is stunned, but will probably regain consciousness again.",
            TRUE, victim, 0, 0, TO_ROOM);
      }
      if (!IS_JACKED_IN(victim)) {
        send_to_char("You're stunned, but will probably regain consciousness "
                     "again.\r\n", victim);
      }
      break;
    case POS_DEAD:
      if (IS_NPC(victim)) {
        if (MOB_FLAGGED(victim, MOB_INANIMATE)) {
          act("$n terminally fails in a shower of sparks!", FALSE, victim, 0, 0, TO_ROOM);
        } else {
          act("$n is dead!  R.I.P.", FALSE, victim, 0, 0, TO_ROOM);
        }

      } else {
        if (PLR_FLAGGED(victim, PLR_DOCWAGON_READY)) {
          docwagon_retrieve(victim);
          return TRUE;
        } else {
          act("$n slumps in a pile. You hear sirens as a DocWagon rushes in and grabs $m.", FALSE, victim, 0, 0, TO_ROOM);
        }
      }

      for (int i = 0; i < NUM_WEARS; i++) {
        if (GET_EQ(victim, i) && GET_OBJ_TYPE(GET_EQ(victim, i)) == ITEM_DOCWAGON) {
          if (GET_DOCWAGON_BONDED_IDNUM(GET_EQ(victim, i)) != GET_IDNUM(victim)) {
            unbonded_docwagon = TRUE;
          } else {
            docwagon_biomonitor = GET_EQ(victim, i);
            break;
          }
        }
      }

      if (docwagon_biomonitor) {
        send_to_char("You feel the vibration of your DocWagon biomonitor sending out an alert, but as the world slips into darkness you realize it's too little, too late.\r\n", victim);
      } else if (unbonded_docwagon) {
        send_to_char("Your last conscious thought involves wondering why your DocWagon biomonitor didn't work-- maybe you forgot to ##^WBOND^n it?\r\n"
                     "The world slips into darkness, and you hope a wandering DocWagon finds you.\r\n", victim);
      } else {
        send_to_char("You feel the world slip into darkness, you'd better hope a wandering DocWagon finds you.\r\n", victim);
      }

      break;
    default:                      /* >= POSITION SLEEPING */
      if (dam > ((int)(GET_MAX_PHYSICAL(victim) / 100) >> 2) && !IS_JACKED_IN(victim)) {
        act("^RThat really did HURT!^n", FALSE, victim, 0, 0, TO_CHAR);
      }

      // Don't bleed or flee from your own spellcast.
      if (attacktype == TYPE_SPELL_DRAIN)
        break;

      if (GET_PHYSICAL(victim) < (GET_MAX_PHYSICAL(victim) >> 2) && !IS_JACKED_IN(victim)) {
        send_to_char(victim, "%sYou wish that your wounds would stop "
                     "^RBLEEDING^r so much!%s\r\n", CCRED(victim, C_SPR),
                     CCNRM(victim, C_SPR));
      }

      if (MOB_FLAGGED(victim, MOB_WIMPY) && !AFF_FLAGGED(victim, AFF_PRONE) && ch != victim &&
          GET_PHYSICAL(victim) < (GET_MAX_PHYSICAL(victim) >> 2))
        do_flee(victim, "", 0, 0);

      if (!IS_NPC(victim) && GET_WIMP_LEV(victim) && victim != ch &&
          (int)(GET_PHYSICAL(victim) / 100) < GET_WIMP_LEV(victim) && !IS_JACKED_IN(victim)) {
        send_to_char("You ^Ywimp^n out, and attempt to flee!\r\n", victim);
        do_flee(victim, "", 0, 0);
      }
      break;
  }

  if (GET_MENTAL(victim) < 100 || GET_PHYSICAL(victim) < 0)
    if (FIGHTING(ch) == victim)
      if (GET_POS(victim) == POS_DEAD || !IS_NPC(victim) || PRF_FLAGGED(ch, PRF_NOAUTOKILL)) {
        stop_fighting(ch);
        if (GET_POS(victim) != POS_DEAD)
          act("You leave off attacking $N.", FALSE, ch, 0, victim, TO_CHAR);
      }

  if (!AWAKE(victim))
    if (CH_IN_COMBAT(victim))
      stop_fighting(victim);

  if (GET_POS(victim) == POS_DEAD)
  {
    if (ch == victim && GET_LASTHIT(victim) > 0) {
      for (struct char_data *i = character_list; i; i = i->next)
        if (!IS_NPC(i) && GET_IDNUM(i) == GET_LASTHIT(victim)) {
          ch = i;
          break;
        }
    }
    if (ch != victim) {
      if (!IS_NPC(ch) && GET_QUEST(ch) && IS_NPC(victim))
        check_quest_kill(ch, victim);
      else if (AFF_FLAGGED(ch, AFF_GROUP) && ch->master && !IS_NPC(ch->master) && GET_QUEST(ch->master) && IS_NPC(victim))
        check_quest_kill(ch->master, victim);
    }

    if ((IS_NPC(victim) || victim->desc) && ch != victim &&
        attacktype != TYPE_EXPLOSION) {
      if ( IS_NPC( victim ) ) {
        extern struct char_data *mob_proto;
        mob_proto[GET_MOB_RNUM(victim)].mob_specials.count_death++;
      }
      if (!PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
        bool need_group_gain = FALSE;
        if (IS_AFFECTED(ch, AFF_GROUP)) {
          for (struct follow_type *f = ch->followers; f; f = f->next) {
            if (IS_AFFECTED(f->follower, AFF_GROUP)
                && f->follower->in_room == ch->in_room
                && !IS_PC_CONJURED_ELEMENTAL(f->follower)
                && !IS_SPIRIT(f->follower))
            {
              need_group_gain = TRUE;
              break;
            }
          }
        }

        if (need_group_gain) {
          group_gain(ch, victim);
        } else {
          exp = calc_karma(ch, victim);
          exp = gain_karma(ch, exp, FALSE, TRUE, TRUE);
          if ( exp >= 100 || access_level(ch, LVL_BUILDER) ) {
            snprintf(buf, sizeof(buf),"%s gains %.2f karma from killing %s.",
                    IS_NPC(ch) ? GET_NAME(ch) : GET_CHAR_NAME(ch),
                    (double)exp/100.0, GET_NAME(victim));
            mudlog(buf, ch, LOG_DEATHLOG, TRUE);
          }
          if ( IS_NPC( victim ) ) {
            extern struct char_data *mob_proto;
            mob_proto[GET_MOB_RNUM(victim)].mob_specials.value_death_karma += exp;
          }

          send_to_char(ch, "You receive %0.2f karma.\r\n", ((float)exp / 100));
        }
      }
    }
    if (!IS_NPC(victim)) {
      gen_death_msg(ch, victim, attacktype);
      if (MOB_FLAGGED(ch, MOB_MEMORY) && !IS_NPC(victim))
        forget(ch, victim);
    }
    die(victim);
    return TRUE;
  }
  return did_docwagon;
}

bool process_has_ammo(struct char_data *ch, struct obj_data *wielded, bool deduct_one_round) {
  int i;
  bool found = FALSE;
  struct obj_data *obj, *cont;

  if (!ch) {
    mudlog("SYSERR: process_has_ammo received null value for ch. Returning TRUE.", ch, LOG_SYSLOG, TRUE);
    return TRUE;
  }

  if (!wielded) {
    // If you're not wielding anything, then not having ammo won't stop you.
    return TRUE;
  }

  // Fireweapon code.
  if (GET_OBJ_TYPE(wielded) == ITEM_FIREWEAPON) {
    for (i = 0; i < NUM_WEARS; i++) {
      if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_QUIVER)
        for (obj = GET_EQ(ch, i)->contains; obj; obj = obj->next_content)
          if (GET_OBJ_TYPE(obj) == ITEM_MISSILE && GET_OBJ_VAL(obj, 0) == GET_OBJ_VAL(wielded, 5)) {
            if (deduct_one_round) {
              GET_OBJ_VAL(GET_EQ(ch, i), 2)--;
              extract_obj(obj);
            }
            found = TRUE;
          }
      if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_WORN)
        for (cont = GET_EQ(ch,i)->contains; cont; cont = cont->next_content)
          if (GET_OBJ_TYPE(cont) == ITEM_QUIVER)
            for (obj = cont->contains; obj; obj = obj->next_content)
              if (GET_OBJ_TYPE(obj) == ITEM_MISSILE && GET_OBJ_VAL(obj, 0) == GET_OBJ_VAL(wielded, 5)) {
                if (deduct_one_round) {
                  GET_OBJ_VAL(cont, 2)--;
                  extract_obj(obj);
                }
                found = TRUE;
              }
      // Was there really no better way to do this whole block?
    }
    if (found)
      return TRUE;
    else {
      if (IS_NPC(ch)) {
        switch_weapons(ch, wielded->worn_on);
        return TRUE;
      } else {
        send_to_char("You're out of arrows!\r\n", ch);
        stop_fighting(ch);
        return FALSE;
      }
    }
  } // End fireweapon code.

  // Check for guns. We can get here either with wielded or mounted guns.
  if (GET_OBJ_TYPE(wielded) == ITEM_WEAPON && IS_GUN(GET_WEAPON_ATTACK_TYPE(wielded)))
  {
    // First, check if they're manning a turret-- if they are, special handling is required.
    if (AFF_FLAGGED(ch, AFF_MANNING) || AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE))  {
      // NPCs don't care about ammo in their mounts. No deduction needed here.
      if (IS_NPC(ch))
        return TRUE;

      // Is it one of the unlimited-ammo weapons?
      if (GET_WEAPON_MAX_AMMO(wielded) < 0)
        return TRUE;

      // It's a player. Look through their gun and locate any ammo boxes in there.
      struct obj_data *ammobox = get_mount_ammo(wielded->in_obj);
      if (ammobox && GET_AMMOBOX_QUANTITY(ammobox) > 0) {
        if (deduct_one_round)
          update_ammobox_ammo_quantity(ammobox, -1, "combat round deduction");
        return TRUE;
      }

      // No ammo boxes found, or the ones that were found were empty. Click it.
      send_to_char(ch, "%s just clicks-- it's out of ammo!\r\n", capitalize(GET_OBJ_NAME(wielded)));
      return FALSE;
    } // End manned checks.

    // Check for a non-ammo-requiring gun. No deduction needed here.
    if (GET_WEAPON_MAX_AMMO(wielded) < 0)
      return TRUE;

    // Check for a magazine.
    if (wielded->contains) {
      // Loaded? Good to go.
      if (GET_MAGAZINE_AMMO_COUNT(wielded->contains)) {
        // Deduct one round if required.
        if (deduct_one_round)
          GET_MAGAZINE_AMMO_COUNT(wielded->contains)--;
        return TRUE;
      }

      // Empty magazine. Send the empty-gun click.
      send_to_char(ch, "%s just clicks when you pull the trigger-- it's out of ammo!\r\n", capitalize(GET_OBJ_NAME(wielded)));

      act("$n can't fire- no ammo.", TRUE, ch, 0, 0, TO_ROLLS);

      // NPCs try to reload, and if they fail they switch weapons.
      if (IS_NPC(ch) && !attempt_reload(ch, wielded->worn_on))
        switch_weapons(ch, wielded->worn_on);

      // Regardless-- no ammo in the current weapon means we lose our turn.
      return FALSE;
    }

    // No magazine, but has a bayonet? They're in charge mode.
    else if (does_weapon_have_bayonet(wielded)) {
      // send_to_char(ch, "has bayonet\r\n");
      return TRUE;
    } else {
      // send_to_char(ch, "no bayonet\r\n");
    }

    // The weapon requires a magazine and doesn't have one.
    send_to_char(ch, "%s just clicks when you pull the trigger-- it's out of ammo!\r\n", capitalize(GET_OBJ_NAME(wielded)));
    return FALSE;
  }

  // snprintf(buf, sizeof(buf), "Got to end of process_has_ammo (%s, %s (%ld)) with no result. Returning true.",
  //         GET_CHAR_NAME(ch), GET_OBJ_NAME(wielded), GET_OBJ_VNUM(wielded));
  // mudlog(buf, ch, LOG_SYSLOG, TRUE);

  // You're wielding something that's not a fireweapon or a gun. Ammo's a given.
  return TRUE;
}

bool has_ammo(struct char_data *ch, struct obj_data *wielded) {
  // Compatibility wrapper: Any call to has_ammo requires ammo deduction.
  return process_has_ammo(ch, wielded, TRUE);
}

bool has_ammo_no_deduct(struct char_data *ch, struct obj_data *wielded) {
  // Calls process_has_ammo with deduction turned off.
  return process_has_ammo(ch, wielded, FALSE);
}

int check_smartlink(struct char_data *ch, struct obj_data *weapon)
{
  struct obj_data *obj, *access;

  // are they wielding two weapons?
  if (GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD) &&
      CAN_WEAR(GET_EQ(ch, WEAR_HOLD), ITEM_WEAR_WIELD))
    return 0;

  int real_obj;
  for (int i = ACCESS_LOCATION_TOP; i <= ACCESS_LOCATION_UNDER; i++) {
    // If they have a smartlink attached:
    if (GET_OBJ_VAL(weapon, i) > 0
        && (real_obj = real_object(GET_OBJ_VAL(weapon, i))) > 0
        && (access = &obj_proto[real_obj])
        && GET_ACCESSORY_TYPE(access) == ACCESS_SMARTLINK) {

      // Iterate through their cyberware and look for a matching smartlink.
      for (obj = ch->cyberware; obj; obj = obj->next_content) {
        if (GET_CYBERWARE_TYPE(obj) == CYB_SMARTLINK) {
          if (GET_CYBERWARE_RATING(obj) == 2 && GET_ACCESSORY_RATING(access) == 2) {
            // Smartlink II with compatible cyberware.
            return SMARTLINK_II_MODIFIER;
          }
          // Smartlink I.
          return SMARTLINK_I_MODIFIER;
        }
      }
      if (GET_EQ(ch, WEAR_EYES)
          && GET_OBJ_TYPE(GET_EQ(ch, WEAR_EYES)) == ITEM_GUN_ACCESSORY
          && GET_ACCESSORY_TYPE(GET_EQ(ch, WEAR_EYES)) == ACCESS_SMARTGOGGLE) {
        // Smartlink plus goggle found-- half value.
        return 1;
      }
    }
  }

  if (AFF_FLAGGED(ch, AFF_LASER_SIGHT)) {
    // Lasers apply when no smartlink was found.
    return 1;
  }

  return 0;
}

// TODO: Remove the default and populate this properly.
int check_recoil(struct char_data *ch, struct obj_data *gun, bool is_using_gyromount=FALSE)
{
  struct obj_data *obj;
  int rnum, comp = 0;

  // Can't use bipods/tripods if you're controlling a vehicle weapon.
  bool can_use_bipods_and_tripods = !is_using_gyromount && !PLR_FLAGGED(ch, PLR_REMOTE) && !AFF_FLAGGED(ch, AFF_RIG) && !AFF_FLAGGED(ch, AFF_MANNING);

  if (!gun || GET_OBJ_TYPE(gun) != ITEM_WEAPON)
    return 0;

  for (int i = ACCESS_LOCATION_TOP; i <= ACCESS_LOCATION_UNDER; i++)
  {
    obj = NULL;

    if (GET_WEAPON_ATTACH_LOC(gun, i) > 0
        && (rnum = real_object(GET_WEAPON_ATTACH_LOC(gun, i))) > -1
        && (obj = &obj_proto[rnum])
        && GET_OBJ_TYPE(obj) == ITEM_GUN_ACCESSORY)
    {
      if (GET_ACCESSORY_TYPE(obj) == ACCESS_GASVENT) {
        // Gas vent values are negative when built, so we need to flip them.
        comp += 0 - GET_ACCESSORY_RATING(obj);
      }
      else if (GET_ACCESSORY_TYPE(obj) == ACCESS_SHOCKPAD)
        comp++;
      else if (can_use_bipods_and_tripods && AFF_FLAGGED(ch, AFF_PRONE))
      {
        if (GET_ACCESSORY_TYPE(obj) == ACCESS_BIPOD)
          comp += RECOIL_COMP_VALUE_BIPOD;
        else if (GET_ACCESSORY_TYPE(obj) == ACCESS_TRIPOD)
          comp += RECOIL_COMP_VALUE_TRIPOD;
      }
    }
  }

  // Add in integral recoil compensation. Previously, this was blocked by gas vents, but that's not RAW as far as I can tell.
  if (GET_WEAPON_INTEGRAL_RECOIL_COMP(gun))
    comp += GET_WEAPON_INTEGRAL_RECOIL_COMP(gun);

  for (obj = ch->cyberware; obj; obj = obj->next_content)
    if (GET_CYBERWARE_TYPE(obj) == CYB_FOOTANCHOR && !GET_CYBERWARE_IS_DISABLED(obj)) {
      comp++;
      break;
    }

  return comp;
}

bool astral_fight(struct char_data *ch, struct char_data *vict)
{
  char oopsbuf[1000];
  int w_type, dam, power, attack_success, newskill, skill_total, base_target;
  bool focus = FALSE, is_physical;

  if (ch->in_room != vict->in_room) {
    stop_fighting(ch);
    if (FIGHTING(vict) == ch)
      stop_fighting(vict);
    snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Entered astral_fight with %s and %s who are not in the same room.", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict));
    mudlog(oopsbuf, ch, LOG_SYSLOG, TRUE);
    send_to_char("You struggle to figure out how to attack astral targets at range.\r\n", ch);
    return FALSE;
  }

  if (GET_POS(vict) <= POS_DEAD)
  {
    mudlog("SYSERR: Attempt to damage a corpse.", ch, LOG_SYSLOG, TRUE);
    return FALSE;                     /* -je, 7/7/92 */
  }

  snprintf(buf3, sizeof(buf3), "^WWelcome to the ancient-ass astral_fight()! Featuring: %s vs %s.^n", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict));
  act(buf3, 1, ch, NULL, NULL, TO_ROLLS);

  // Find astral foci.
  struct obj_data *wielded = ch->equipment[WEAR_WIELD];
  if ((IS_ASTRAL(ch) || SEES_ASTRAL(ch))
      && wielded
      && GET_OBJ_TYPE(wielded) == ITEM_WEAPON
      && WEAPON_FOCUS_USABLE_BY(wielded, ch))
  {
    focus = TRUE;
  }
  else if (wielded) {
    snprintf(buf3, sizeof(buf3), "%s is not a weapon focus, so we'll use unarmed combat.^n", capitalize(GET_OBJ_NAME(wielded)));
    act(buf3, 1, ch, NULL, NULL, TO_ROLLS);
    send_to_char(ch, "%s can't damage astral forms, so you use your fists instead!\r\n", capitalize(GET_OBJ_NAME(wielded)));
    wielded = NULL;
    // stop_fighting(ch);
    // return;
  }

  if (IS_PROJECT(ch) && (ch != vict) && !IS_NPC(vict) && SEES_ASTRAL(vict) &&
      !PLR_FLAGGED(vict, PLR_KILLER) && !PLR_FLAGGED(vict,PLR_WANTED) &&
      (!PRF_FLAGGED(ch, PRF_PKER) || !PRF_FLAGGED(vict, PRF_PKER)) &&
      !PLR_FLAGGED(ch->desc->original, PLR_KILLER))
  {
    PLR_FLAGS(ch->desc->original).SetBit(PLR_KILLER);
    snprintf(buf, sizeof(buf), "PC Killer bit set on %s (astral) for initiating attack on %s at %s.",
            GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), GET_ROOM_NAME(get_ch_in_room(vict)));
    mudlog(buf, ch, LOG_MISCLOG, TRUE);
    send_to_char("If you want to be a PLAYER KILLER, so be it...\r\n", ch);
  }

  if (wielded) {
    switch (GET_WEAPON_ATTACK_TYPE(wielded)) {
      case WEAP_EDGED:
      case WEAP_POLEARM:
        w_type = TYPE_SLASH;
        break;
      case WEAP_WHIP:
        w_type = TYPE_WHIP;
        break;
      case WEAP_CLUB:
      case WEAP_GLOVE:
        w_type = TYPE_POUND;
        break;
      case WEAP_SMG:
      case WEAP_ASSAULT_RIFLE:
      case WEAP_LMG:
      case WEAP_MMG:
      case WEAP_HMG:
      case WEAP_MINIGUN:
        w_type = TYPE_MACHINE_GUN;
        break;
      case WEAP_SHOTGUN:
        w_type = TYPE_SHOTGUN;
        break;
      case WEAP_TASER:
        w_type = TYPE_TASER;
        break;
      case WEAP_CANNON:
        w_type = TYPE_CANNON;
        break;
      case WEAP_SPORT_RIFLE:
      case WEAP_SNIPER_RIFLE:
        w_type = TYPE_RIFLE;
        break;
      case WEAP_MISS_LAUNCHER:
      case WEAP_GREN_LAUNCHER:
        w_type = TYPE_ROCKET;
        break;

      default:
        w_type = TYPE_PISTOL;
        break;
    }
  } else
  {
    if (ch->mob_specials.attack_type != 0)
      w_type = ch->mob_specials.attack_type;
    else
      w_type = TYPE_HIT;
  }

  if (((w_type == TYPE_HIT) || (w_type == TYPE_BLUDGEON) || (w_type == TYPE_PUNCH) ||
       (w_type == TYPE_TASER) || (w_type == TYPE_CRUSH) || (w_type == TYPE_POUND)) &&
      (GET_MENTAL(vict) >= 100))
  {
    is_physical = FALSE;
  } else {
    is_physical = TRUE;
  }

  base_target = 4 + modify_target(ch);

  if (!AWAKE(vict))
    base_target -= 2;

  if ((w_type != TYPE_HIT) && wielded)
  {
    power = GET_WEAPON_POWER(wielded) ? GET_WEAPON_POWER(wielded) : GET_WEAPON_STR_BONUS(wielded) + GET_STR(ch);
    if (focus)
      power += GET_WEAPON_FOCUS_RATING(wielded);
    power -= GET_IMPACT(vict);
    dam = MODERATE;
    if (IS_SPIRIT(vict) || IS_ANY_ELEMENTAL(vict))
      skill_total = GET_WIL(ch);
    else if (GET_SKILL(ch, GET_WEAPON_SKILL(wielded)) < 1) {
      newskill = return_general(GET_WEAPON_SKILL(wielded));
      skill_total = MAX(get_skill(ch, newskill, base_target), GET_SKILL(ch, SKILL_SORCERY));
    } else {
      // TODO: Isn't this a bug in that the base target is not modified per the shit-tastically named get_skill()?
      skill_total = MAX(GET_SKILL(ch, GET_WEAPON_SKILL(wielded)), GET_SKILL(ch, SKILL_SORCERY));
    }

  } else
  {
    power = GET_STR(ch) - GET_IMPACT(vict);
    if (IS_PROJECT(ch) || SEES_ASTRAL(ch))
      dam = LIGHT;
    else
      dam = MODERATE;
    if (IS_SPIRIT(vict) || IS_ANY_ELEMENTAL(vict))
      skill_total = GET_WIL(ch);
    else if (GET_SKILL(ch, SKILL_UNARMED_COMBAT) < 1) {
      if (GET_SKILL(ch, SKILL_SORCERY) < 1) {
        newskill = SKILL_UNARMED_COMBAT;
        skill_total = get_skill(ch, newskill, base_target);
      } else
        skill_total = GET_SKILL(ch, SKILL_SORCERY);
    } else
      skill_total = MAX(GET_SKILL(ch, SKILL_UNARMED_COMBAT),
                        GET_SKILL(ch, SKILL_SORCERY));
  }

  skill_total += (int)(GET_ASTRAL(ch) / 2);

  if (power < 2)
  {
    skill_total = MAX(0, skill_total - (2 - power));
    power = 2;
  }

  if (AWAKE(vict))
    attack_success = resisted_test(skill_total, base_target,
                                   (int)(GET_ASTRAL(vict) / 2), power);
  else
    attack_success = success_test(skill_total, base_target);

  if (attack_success < 1)
  {
    act("$n whiffs $s attack on $N!", 1, ch, NULL, vict, TO_ROOM);
    act("$n whiffs $s attack on $N!", 1, ch, NULL, vict, TO_CHAR);
    act("$n whiffs $s attack on $N!", 1, ch, NULL, vict, TO_VICT);
    if (!AFF_FLAGGED(ch, AFF_COUNTER_ATT)) {
      bool victim_died = FALSE;
      if ((GET_ASTRAL(vict) > 0)
          && (attack_success < 0)
          && FIGHTING(vict) == ch
          && GET_POS(vict) > POS_SLEEPING) {
        send_to_char(ch, "%s counters your attack!\r\n", CAP(GET_NAME(vict)));
        send_to_char(vict, "You counter %s's attack!\r\n", GET_NAME(ch));
        act("$n counters $N's attack!", 1, ch, NULL, vict, TO_ROOM);
        AFF_FLAGS(vict).SetBit(AFF_COUNTER_ATT);
        victim_died = astral_fight(vict, ch);
      }
      return victim_died;
    } else {
      AFF_FLAGS(ch).RemoveBit(AFF_COUNTER_ATT);
      return FALSE;
    }
  } else if (AFF_FLAGGED(ch, AFF_COUNTER_ATT))
    AFF_FLAGS(ch).RemoveBit(AFF_COUNTER_ATT);

  attack_success -= success_test(GET_BOD(vict) + GET_BODY(vict), power);

  dam = convert_damage(stage(attack_success, dam));

  if (IS_PROJECT(ch) && PLR_FLAGGED(ch->desc->original, PLR_KILLER))
    dam = 0;

  return damage(ch, vict, dam, w_type, is_physical);
  /*  This echo-back-damage code is not necessary, it's handled automatically in damage().
  if (IS_PROJECT(vict) && dam > 0)
    damage(vict->desc->original, vict->desc->original, dam, 0, is_physical);
  */
}

void remove_throwing(struct char_data *ch)
{
  struct obj_data *obj = NULL;
  int i, pos, type;

  for (pos = WEAR_WIELD; pos <= WEAR_HOLD; pos++) {
    if (GET_EQ(ch, pos))
    {
      type = GET_OBJ_VAL(GET_EQ(ch, pos), 3);
      if (type == TYPE_SHURIKEN || type == TYPE_THROWING_KNIFE) {
        extract_obj(unequip_char(ch, pos, TRUE));
        for (i = 0; i < NUM_WEARS; i++)
          if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_QUIVER)
            for (obj = GET_EQ(ch, i)->contains; obj; obj = obj->next_content)
              if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && GET_OBJ_VAL(obj, 3) == type) {
                obj_from_obj(obj);
                equip_char(ch, obj, WEAR_WIELD);
                return;
              }
        return;
      }
    }
  }
}

void combat_message_process_single_ranged_response(struct char_data *ch, struct char_data *tch) {
  if (IS_NPC(tch)) {
    // Everyone ends up on edge when they hear gunfire nearby.
    GET_MOBALERTTIME(tch) = 20;
    GET_MOBALERT(tch) = MALERT_ALERT;

    // Only guards and helpers who are not in combat can participate.
    if (CH_IN_COMBAT(tch) || !(MOB_FLAGGED(tch, MOB_GUARD) || MOB_FLAGGED(tch, MOB_HELPER)))
      return;

    // Guards and helpers will actively try to fire on a player using a gun.
    if (!IS_NPC(ch) && (!FIGHTING(ch) || IS_NPC(FIGHTING(ch)))) {
      if (number(0, 6) >= 2) {
        GET_MOBALERTTIME(tch) = 30;
        GET_MOBALERT(tch) = MALERT_ALARM;
        struct room_data *was_in = tch->in_room;
        if (ranged_response(ch, tch) && tch->in_room == was_in) {
          act("$n aims $s weapon at a distant threat!",
              FALSE, tch, 0, ch, TO_ROOM);
          send_mob_aggression_warnings(ch, tch);
        }
      }
    }

    // They also try to fire on the target of a gun-wielder.
    if (IS_NPC(ch) && CH_IN_COMBAT(ch) && FIGHTING(ch) && CAN_SEE(tch, FIGHTING(ch))) {
      if (number(0, 8) == 1) {
        bool found_target = FALSE;

        // Make sure they have line of sight to the target.
        if (GET_EQ(tch, WEAR_WIELD)) {
          for (int dir = NORTH; !found_target && dir <= NORTHWEST; dir++) {
            struct room_data *curr_room = tch->in_room;

            // If there's no exit in this direction, stop immediately.
            if (!curr_room->dir_option[dir] || IS_SET(curr_room->dir_option[dir]->exit_info, EX_CLOSED))
              continue;

            // Otherwise, scan down that exit up to weapon range.
            for (int range = 1; !found_target && range <= find_weapon_range(tch, GET_EQ(tch, WEAR_WIELD)); range++) {
              curr_room = curr_room->dir_option[dir]->to_room;

              // Check for presence of target.
              for (struct char_data *candidate = curr_room->people; candidate; candidate = candidate->next_in_room) {
                if (candidate == FIGHTING(ch)) {
                  found_target = TRUE;
                  break;
                }
              }

              // Stop further iteration if there are no further exits.
              if (!curr_room->dir_option[dir] || IS_SET(curr_room->dir_option[dir]->exit_info, EX_CLOSED))
                break;
            }
          }
        }

        // No line of sight. Abort.
        if (!found_target)
          return;

        // Line of sight established, fire.
        GET_MOBALERTTIME(tch) = 30;
        GET_MOBALERT(tch) = MALERT_ALARM;
        struct room_data *was_in = tch->in_room;
        if (ranged_response(FIGHTING(ch), tch) && tch->in_room == was_in) {
          act("$n aims $s weapon at a distant threat!",
              FALSE, tch, 0, FIGHTING(ch), TO_ROOM);
          send_mob_aggression_warnings(FIGHTING(ch), tch);
        }
      }
    }
  }
}

void combat_message_process_ranged_response(struct char_data *ch, rnum_t rnum) {
  for (struct char_data *tch = world[rnum].people; tch; tch = tch->next_in_room) {
    combat_message_process_single_ranged_response(ch, tch);
  }
}

// Pick your favorite.
#define COMBAT_MESSAGE_DEBUG_LOG(str) log(str);
//#define COMBAT_MESSAGE_DEBUG_LOG(str) ;
void combat_message(struct char_data *ch, struct char_data *victim, struct obj_data *weapon, int damage, int burst, int vision_penalty_for_messaging)
{
  char buf[MAX_MESSAGE_LENGTH], buf1[MAX_MESSAGE_LENGTH], buf2[MAX_MESSAGE_LENGTH], buf3[MAX_MESSAGE_LENGTH], buf4[MAX_MESSAGE_LENGTH],
  been_heard[MAX_STRING_LENGTH], temp[20];
  struct obj_data *obj = NULL;
  struct room_data *ch_room = NULL, *vict_room = NULL;
  rnum_t room1 = 0, room2 = 0, rnum = 0;
  struct veh_data *veh = NULL;

  if (weapon == NULL) {
    mudlog("SYSERR: Null weapon in combat_message()!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  if (burst <= 1) {
    if (GET_OBJ_VAL(weapon, 4) == SKILL_SHOTGUNS || GET_OBJ_VAL(weapon, 4) == SKILL_ASSAULT_CANNON)
      strcpy(buf, "single shell from $p");
    else
      strcpy(buf, "single round from $p");
  } else if (burst == 2) {
    strcpy(buf, "short burst from $p");
  } else if (burst == 3) {
    strcpy(buf, "burst from $p");
  } else {
    strcpy(buf, "long burst from $p");
  }

  char blindfire_buf[100];

  if (vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY) {
    strlcpy(blindfire_buf, "blindly ", sizeof(blindfire_buf));
  } else {
    strlcpy(blindfire_buf, "", sizeof(blindfire_buf));
  }

  ch_room = get_ch_in_room(ch);
  vict_room = get_ch_in_room(victim);
  if (ch_room == vict_room) {
    // Same-room messaging.
    static char vehicle_message[1000];

    if (ch->in_veh)
      snprintf(vehicle_message, sizeof(vehicle_message), "From inside %s, ", decapitalize_a_an(GET_VEH_NAME_NOFORMAT(ch->in_veh)));
    else
      strcpy(vehicle_message, "");

    if (damage < 0) {
      switch (number(1, 3)) {
        case 1:
          snprintf(buf1, sizeof(buf1), "^r%s$n^r %sfires a %s^r at you, but you manage to dodge.^n", vehicle_message, blindfire_buf, buf);
          snprintf(buf2, sizeof(buf2), "^yYou %sfire a %s^y at $N^y, but $E manage%s to dodge.^n", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "%s$n %sfires a %s^n at $N, but $E manage%s to dodge.", vehicle_message, blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
        case 2:
          snprintf(buf1, sizeof(buf1), "^r%s$n^r %sfires a %s^r at you, but you easily dodge.^n", vehicle_message, blindfire_buf, buf);
          snprintf(buf2, sizeof(buf2), "^yYou %sfire a %s^y at $N^y, but $E easily dodge%s.^n", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "%s$n %sfires a %s^n at $N, but $E easily dodge%s.", vehicle_message, blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
        case 3:
          snprintf(buf1, sizeof(buf1), "^r%s$n^r %sfires a %s^r at you, but you move out of the way.^n", vehicle_message, blindfire_buf, buf);
          snprintf(buf2, sizeof(buf2), "^yYou %sfire a %s^y at $N^y, but $E move%s out of the way.^n", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "%s$n %sfires a %s^n at $N, but $E move%s out of the way.", vehicle_message, blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
      }
    } else if (damage == 0) {
      switch (number(1, 2)) {
        case 1:
          snprintf(buf1, sizeof(buf1), "^r%s$n^r %sfires a %s^r at you, but your armor holds.^n", vehicle_message, blindfire_buf, buf);
          snprintf(buf2, sizeof(buf2), "^yYou %sfire a %s^y at $N^y, but it doesn't seem to hurt $M.^n", blindfire_buf, buf);
          snprintf(buf3, sizeof(buf3), "%s$n %sfires a %s^n at $N, but it doesn't seem to hurt $M.", vehicle_message, blindfire_buf, buf);
          break;
        case 2:
          snprintf(buf1, sizeof(buf1), "^r%s$n^r %sfires a %s^r at you, but you roll with the impact.^n", vehicle_message, blindfire_buf, buf);
          snprintf(buf2, sizeof(buf2), "^yYou %sfire a %s^y at $N^y, but $E roll%s with the impact.^n", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "%s$n %sfires a %s^n at $N, but $E roll%s with the impact.", vehicle_message, blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
      }
    } else if (damage == LIGHT) {
      snprintf(buf1, sizeof(buf1), "^r%s$n^r grazes you with a %s%s^r.^n", vehicle_message, vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
      snprintf(buf2, sizeof(buf2), "^yYou graze $N^y with a %s%s^y.^n", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
      snprintf(buf3, sizeof(buf3), "%s$n grazes $N with a %s%s^n.", vehicle_message, vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
    } else if (damage == MODERATE) {
      snprintf(buf1, sizeof(buf1), "^r%s$n^r hits you with a %s%s^r.^n", vehicle_message, vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
      snprintf(buf2, sizeof(buf2), "^yYou hit $N^y with a %s%s^y.^n", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
      snprintf(buf3, sizeof(buf3), "%s$n hits $N with a %s%s^n.", vehicle_message, vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
    } else if (damage == SERIOUS) {
      snprintf(buf1, sizeof(buf1), "^r%s$n^r massacres you with a %s%s^r.^n", vehicle_message, vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
      snprintf(buf2, sizeof(buf2), "^yYou massacre $N^y with a %s%s^y.^n", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
      snprintf(buf3, sizeof(buf3), "%s$n massacres $N with a %s%s^n.", vehicle_message, vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
    } else if (damage >= DEADLY) {
      switch (number(1, 2)) {
        case 1:
          snprintf(buf1, sizeof(buf1), "^r%s$n^r puts you down with a %sdeadly %s^r.^n", vehicle_message, vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired but still " : "", buf);
          snprintf(buf2, sizeof(buf2), "^yYou put $N^y down with a %sdeadly %s^y.^n", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired but still " : "", buf);
          snprintf(buf3, sizeof(buf3), "%s$n puts $N down with a %sdeadly %s^n.", vehicle_message, vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired but still " : "", buf);
          break;
        case 2:
          snprintf(buf1, sizeof(buf1), "^r%s$n^r sublimates you with a %sdeadly %s^r.^n", vehicle_message, vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired but still " : "", buf);
          snprintf(buf2, sizeof(buf2), "^yYou sublimate $N^y with a %sdeadly %s^y.^n", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired but still " : "", buf);
          snprintf(buf3, sizeof(buf3), "%s$n sublimates $N with a %sdeadly %s^n.", vehicle_message, vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired but still " : "", buf);
          break;
      }
    }
    act(buf1, FALSE, ch, weapon, victim, TO_VICT);
    act(buf2, FALSE, ch, weapon, victim, TO_CHAR | TO_REMOTE);
    act(buf3, FALSE, ch, weapon, victim, ch->in_veh ? TO_VEH_ROOM : TO_NOTVICT);
    // End same-room messaging.
  } else {
    // Ranged messaging. TODO
    if (damage < 0) {
      switch (number(1, 3)) {
        case 1:
          snprintf(buf1, sizeof(buf1), "^r$n^r %sfires a %s^r at you, but you manage to dodge.^n", blindfire_buf, buf);
          snprintf(buf2, sizeof(buf2), "^yYou %sfire a %s^y at $N^y, but $E manage%s to dodge.^n", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "$n %sfires a %s^n at $N, but $E manage%s to dodge.", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf4, sizeof(buf4), "$N %sfires a %s^n at $n, but $e manage%s to dodge.", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
        case 2:
          snprintf(buf1, sizeof(buf1), "^r$n^r %sfires a %s^r at you, but you easily dodge.^n", blindfire_buf, buf);
          snprintf(buf2, sizeof(buf2), "^yYou %sfire a %s^y at $N^y, but $E easily dodge%s.^n", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "$n %sfires a %s^n at $N, but $E easily dodge%s.", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf4, sizeof(buf4), "$N %sfires a %s^n at $n, but $e easily dodge%s.", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
        case 3:
          snprintf(buf1, sizeof(buf1), "^r$n^r %sfires a %s^r at you, but you move out of the way.^n", blindfire_buf, buf);
          snprintf(buf2, sizeof(buf2), "^yYou %sfire a %s^y at $N^y, but $E move%s out of the way.^n", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "$n %sfires a %s^n at $N, but $E move%s out of the way.", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf4, sizeof(buf4), "$N %sfires a %s^n at $n, but $e move%s out of the way.", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
      }
    } else if (damage == 0) {
      switch (number(1, 2)) {
        case 1:
          snprintf(buf1, sizeof(buf1), "^r$n^r %sfires a %s^r at you, but your armor holds.^n", blindfire_buf, buf);
          snprintf(buf2, sizeof(buf2), "^yYou %sfire a %s^y at $N^y, but it doesn't seem to hurt $M.^n", blindfire_buf, buf);
          snprintf(buf3, sizeof(buf3), "$n %sfires a %s^n at $N, but it doesn't seem to hurt $M.", blindfire_buf, buf);
          snprintf(buf4, sizeof(buf4), "$N %sfires a %s^n at $n, but it doesn't seem to hurt $m.", blindfire_buf, buf);
          break;
        case 2:
          snprintf(buf1, sizeof(buf1), "^r$n^r %sfires a %s^r at you, but you roll with the impact.^n", blindfire_buf, buf);
          snprintf(buf2, sizeof(buf2), "^yYou %sfire a %s^y at $N^y, but $E roll%s with the impact.^n", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "$n %sfires a %s^n at $N, but $E roll%s with the impact.", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf4, sizeof(buf4), "$N %sfires a %s^n at $n, but $e roll%s with the impact.", blindfire_buf, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
      }
    } else if (damage == LIGHT) {
      snprintf(buf1, sizeof(buf1), "^r$n^r grazes you with a %s%s^r.^n", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
      snprintf(buf2, sizeof(buf2), "^yYou graze $N^y with a %s%s^y.^n", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
      snprintf(buf3, sizeof(buf3), "$n grazes $N with a %s%s^n.", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
      snprintf(buf4, sizeof(buf4), "$N grazes $n with a %s%s^n.", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
    } else if (damage == MODERATE) {
      snprintf(buf1, sizeof(buf1), "^r$n^r hits you with a %s%s^r.^n", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
      snprintf(buf2, sizeof(buf2), "^yYou hit $N^y with a %s%s^y.^n", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
      snprintf(buf3, sizeof(buf3), "$n hits $N with a %s%s^n.", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
      snprintf(buf4, sizeof(buf4), "$N hits $n with a %s%s^n.", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
    } else if (damage == SERIOUS) {
      snprintf(buf1, sizeof(buf1), "^r$n^r massacres you with a %s%s^r.^n", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
      snprintf(buf2, sizeof(buf2), "^yYou massacre $N^y with a %s%s^y.^n", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
      snprintf(buf3, sizeof(buf3), "$n massacres $N with a %s%s^n.", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
      snprintf(buf4, sizeof(buf4), "$N massacres $n with a %s%s^n.", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
    } else if (damage >= DEADLY) {
      switch (number(1, 2)) {
        case 1:
          snprintf(buf1, sizeof(buf1), "^r$n^r puts you down with a %sdeadly %s^r.^n", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
          snprintf(buf2, sizeof(buf2), "^yYou put $N^y down with a %sdeadly %s^y.^n", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
          snprintf(buf3, sizeof(buf3), "$n puts $N down with a %sdeadly %s^n.", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
          snprintf(buf4, sizeof(buf4), "$N puts $n down with a %sdeadly %s^n.", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
          break;
        case 2:
          snprintf(buf1, sizeof(buf1), "^r$n^r sublimates you with a %sdeadly %s^r.^n", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
          snprintf(buf2, sizeof(buf2), "^yYou sublimate $N^y with a %sdeadly %s^y.^n", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
          snprintf(buf3, sizeof(buf3), "$n sublimates $N with a %sdeadly %s^n.", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
          snprintf(buf4, sizeof(buf4), "$N sublimates $n with a %sdeadly %s^n.", vision_penalty_for_messaging >= MAX_VISIBILITY_PENALTY ? "blind-fired " : "", buf);
          break;
      }
    }
    act(buf1, FALSE, ch, weapon, victim, TO_VICT);
    act(buf2, FALSE, ch, weapon, victim, TO_CHAR | TO_REMOTE);
    if (!PLR_FLAGGED(ch, PLR_REMOTE)) {
      act(buf3, FALSE, ch, weapon, victim, TO_NOTVICT);
    }
    act(buf4, FALSE, victim, weapon, ch, TO_NOTVICT);
    // End ranged messaging.
  }

  // Now that we've sent the messages, change to the vehicle's room (if we're in a vehicle).
  if (AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE)) {
    RIG_VEH(ch, veh);
    ch_room = get_veh_in_room(veh);
  }

  // Alert everyone in the room.
  for (struct char_data *listener = ch_room->people; listener; listener = listener->next_in_room) {
    if (IS_NPC(listener)) {
      if (veh || CAN_SEE(listener, ch)) {
        GET_MOBALERTTIME(listener) = 30;
        GET_MOBALERT(listener) = MALERT_ALARM;
      } else {
        GET_MOBALERTTIME(listener) = 20;
        GET_MOBALERT(listener) = MALERT_ALERT;
      }
    }
  }

  // If the player's in a silent room, don't propagate the gunshot.
  if (ch_room->silence[ROOM_NUM_SPELLS_OF_TYPE] > 0)
    return;

  // If the player has a silencer or suppressor, restrict the propagation of the gunshot.
  bool has_suppressor = FALSE;
  for (int i = 7; i < 10; i++) {
    if (GET_OBJ_VAL(weapon, i) > 0 &&
        (rnum = real_object(GET_OBJ_VAL(weapon, i))) > -1 &&
        (obj = &obj_proto[rnum]) && GET_OBJ_TYPE(obj) == ITEM_GUN_ACCESSORY)
      if (GET_OBJ_VAL(obj, 1) == ACCESS_SILENCER || GET_OBJ_VAL(obj, 1) == ACCESS_SOUNDSUPP) {
        has_suppressor = TRUE;
        break;
      }
  }

  snprintf(been_heard, sizeof(been_heard), ".%ld.", real_room(ch_room->number));

  // Initialize gunshot queue.
  std::queue<rnum_t> room_queue;
  std::queue<rnum_t> secondary_room_queue;

  // Scan the shooter's room's exits and add notifications to any valid adjacent rooms.
  // 'Valid' is defined as 'exists and is not flagged as silent'.
  for (int door1 = 0; door1 < NUM_OF_DIRS; door1++) {
    if (ch_room->dir_option[door1] && (room1 = real_room(ch_room->dir_option[door1]->to_room->number)) != NOWHERE && world[room1].silence[ROOM_NUM_SPELLS_OF_TYPE] <= 0) {
      // If the room is in the heard-it-already list, skip to the next one.
      snprintf(temp, sizeof(temp), ".%ld.", room1);
      if (strstr(been_heard, temp) != 0)
        continue;

      if (has_suppressor) {
        // Special case: If they're using a suppressed weapon, print a muffled-gunfire message and terminate.
        for (struct char_data *listener = world[room1].people; listener; listener = listener->next_in_room)
          if (!PRF_FLAGGED(listener, PRF_FIGHTGAG))
            send_to_char("You hear muffled gunshots nearby.\r\n", listener);

        combat_message_process_ranged_response(ch, room1);
        strcat(been_heard, temp);
      } else {
        // Send gunshot notifications to the selected room. Process guard/helper responses.
        for (struct char_data *listener = world[room1].people; listener; listener = listener->next_in_room)
          if (!PRF_FLAGGED(listener, PRF_FIGHTGAG))
            send_to_char("You hear gunshots nearby!\r\n", listener);
        combat_message_process_ranged_response(ch, room1);
        strcat(been_heard, temp);

        // Add the room's exits to the list.
        for (int door2 = 0; door2 < NUM_OF_DIRS; door2++)
          if (world[room1].dir_option[door2] && (room2 = real_room(world[room1].dir_option[door2]->to_room->number)) != NOWHERE && world[room2].silence[ROOM_NUM_SPELLS_OF_TYPE] <= 0)
            room_queue.push(room2);
      }
    }
  }

  if (has_suppressor) return;

  // Scan the list of near-adjacent rooms and send messages to all non-heard ones. Add their exits to the secondary queue.
  while (!room_queue.empty()) {
    room1 = room_queue.front();
    room_queue.pop();

    // If the room is in the heard-it-already list, skip to the next one.
    snprintf(temp, sizeof(temp), ".%ld.", room1);
    if (strstr(been_heard, temp) != 0)
      continue;

    for (struct char_data *listener = world[room1].people; listener; listener = listener->next_in_room)
      if (!PRF_FLAGGED(listener, PRF_FIGHTGAG))
        send_to_char("You hear gunshots not far off.\r\n", listener);
    combat_message_process_ranged_response(ch, room1);
    strcat(been_heard, temp);

    // Add the room's exits to the list.
    for (int door = 0; door < NUM_OF_DIRS; door++)
      if (world[room1].dir_option[door] && (room2 = real_room(world[room1].dir_option[door]->to_room->number)) != NOWHERE && world[room2].silence[ROOM_NUM_SPELLS_OF_TYPE] <= 0)
        secondary_room_queue.push(room2);
  }

  // Scan the list of distant rooms and message them as appropriate.
  while (!secondary_room_queue.empty()) {
    room1 = secondary_room_queue.front();
    secondary_room_queue.pop();

    // If the room is in the heard-it-already list, skip to the next one.
    snprintf(temp, sizeof(temp), ".%ld.", room1);
    if (strstr(been_heard, temp) != 0)
      continue;

    for (struct char_data *listener = world[room1].people; listener; listener = listener->next_in_room)
      if (!PRF_FLAGGED(listener, PRF_FIGHTGAG))
        send_to_char("You hear gunshots in the distance.\r\n", listener);
    combat_message_process_ranged_response(ch, room1);
    strcat(been_heard, temp);
  }
}

int get_weapon_damage_type(struct obj_data* weapon) {
  if (!weapon)
    return TYPE_HIT;

  int type;
  switch (GET_OBJ_VAL(weapon, 3)) {
    case WEAP_EDGED:
    case WEAP_POLEARM:
      type = TYPE_SLASH;
      break;
    case WEAP_WHIP:
      type = TYPE_WHIP;
      break;
    case WEAP_CLUB:
    case WEAP_GLOVE:
      type = TYPE_POUND;
      break;
    case WEAP_SMG:
    case WEAP_ASSAULT_RIFLE:
    case WEAP_LMG:
    case WEAP_MMG:
    case WEAP_HMG:
    case WEAP_MINIGUN:
      type = TYPE_MACHINE_GUN;
      break;
    case WEAP_SHOTGUN:
      type = TYPE_SHOTGUN;
      break;
    case WEAP_TASER:
      type = TYPE_TASER;
      break;
    case WEAP_CANNON:
      type = TYPE_CANNON;
      break;
    case WEAP_SPORT_RIFLE:
    case WEAP_SNIPER_RIFLE:
      type = TYPE_RIFLE;
      break;
    case WEAP_MISS_LAUNCHER:
    case WEAP_GREN_LAUNCHER:
      type = TYPE_ROCKET;
      break;
    default:
      type = TYPE_PISTOL;
      break;
  }

  return type;
}

/* Combat rules:
 1) All dice pools refresh.
 2) Calculate initiative for all parties (reaction + dice)
 - Highest goes first
 3) Characters take actions on their combat phase.
 4) Return to step 1.
 */

bool is_char_too_tall(struct char_data *ch) {
  assert(ch != NULL);

#ifdef USE_SLOUCH_RULES
  if (!ch->in_room)
    return FALSE;

  return ROOM_FLAGGED(ch->in_room, ROOM_INDOORS) && GET_HEIGHT(ch) >= ch->in_room->z*100;
#else
  return FALSE;
#endif
}

bool vehicle_has_ultrasound_sensors(struct veh_data *veh) {
  if (!veh)
    return FALSE;

  if (VEH_FLAGGED(*veh, VFLAG_ULTRASOUND))
    return TRUE;

  for (int i = 0; i < NUM_MODS; i++)
    if (GET_MOD(veh, i) && GET_MOD(veh, i)->obj_flags.bitvector.IsSet(AFF_ULTRASOUND))
      return TRUE;

  return FALSE;
}

#define INVIS_CODE_STAFF       321
#define INVIS_CODE_TOTALINVIS  123
#define BLIND_FIGHTING_MAX     (MAX_VISIBILITY_PENALTY / 2)
#define BLIND_FIRE_PENALTY     MAX_VISIBILITY_PENALTY
int calculate_vision_penalty(struct char_data *ch, struct char_data *victim) {
  int modifier = 0;
  char rbuf[2048];

  // If we can't see due to light levels, bail out.
  if (!LIGHT_OK_ROOM_SPECIFIED(ch, get_ch_in_room(ch)) || (ch->in_room != victim->in_room && !LIGHT_OK_ROOM_SPECIFIED(ch, get_ch_in_room(victim)))) {
    modifier = MAX_VISIBILITY_PENALTY;

    if (GET_POWER(ch, ADEPT_BLIND_FIGHTING) && modifier > BLIND_FIGHTING_MAX) {
      modifier = BLIND_FIGHTING_MAX;

      snprintf(rbuf, sizeof(rbuf), "%s: Vision penalty capped to %d by Blind Fighting", GET_CHAR_NAME(ch), modifier);

      act(rbuf, 0, ch, 0, 0, TO_ROLLS);
      if (ch->in_room != victim->in_room)
        act(rbuf, 0, victim, 0, 0, TO_ROLLS);
    }

    snprintf(rbuf, sizeof(rbuf), "%s: Cannot see target due to light level, so final char-to-char visibility TN is ^c%d^n.", GET_CHAR_NAME(ch), modifier);
    act(rbuf, 0, victim, 0, ch, TO_ROLLS);
    return modifier;
  }

  // If they're in an invis staffer above your level, you done goofed by fighting them. Return special code so we know what caused this in rolls.
  if (!IS_NPC(victim) && GET_INVIS_LEV(victim) > 0 && (IS_NPC(ch) || !access_level(ch, GET_INVIS_LEV(victim)))) {
    act("$n: Maximum penalty- fighting invis staff.", 0, ch, 0, 0, TO_ROLLS);
    if (ch->in_room != victim->in_room)
      act("$N: Maximum penalty- fighting invis staff.", 0, victim, 0, ch, TO_ROLLS);
    return INVIS_CODE_STAFF;
  }

  // If they're flagged totalinvis (library mobs etc), you shouldn't be fighting them anyways.
  if (IS_NPC(victim) && MOB_FLAGGED(victim, MOB_TOTALINVIS)) {
    act("$n: Maximum penalty- fighting total-invis mob.", 0, ch, 0, 0, TO_ROLLS);
    if (ch->in_room != victim->in_room)
      act("$N: Maximum penalty- fighting total-invis mob.", 0, victim, 0, ch, TO_ROLLS);
    return INVIS_CODE_TOTALINVIS;
  }

  // Pre-calculate the things we care about here. First, character vision info.
  bool ch_has_ultrasound = has_vision(ch, VISION_ULTRASONIC) && !affected_by_spell(ch, SPELL_STEALTH);
  bool ch_has_thermographic = has_vision(ch, VISION_THERMOGRAPHIC);
  bool ch_sees_astral = SEES_ASTRAL(ch);

#ifdef ULTRASOUND_REQUIRES_SAME_ROOM
  has_ultrasound &= ch->in_room == victim->in_room;
#endif

  // EXCEPT: If you're rigging (not manning), things change.
  if (AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE)) {
    struct veh_data *veh = get_veh_controlled_by_char(ch);
    ch_has_ultrasound = vehicle_has_ultrasound_sensors(veh); // Eventually, we'll have ultrasonic sensors on vehicles too.
    ch_has_thermographic = TRUE;
    ch_sees_astral = FALSE;

#ifdef ULTRASOUND_REQUIRES_SAME_ROOM
    has_ultrasound &= veh->in_room == victim->in_room;
#endif
  }

  // Next, vict invis info.
  bool vict_is_imp_invis = IS_AFFECTED(victim, AFF_IMP_INVIS);
  bool vict_is_just_invis = IS_AFFECTED(victim, AFF_INVISIBLE);

  if ((access_level(ch, LVL_PRESIDENT) || access_level(victim, LVL_PRESIDENT)) && (PRF_FLAGGED(ch, PRF_ROLLS) || PRF_FLAGGED(victim, PRF_ROLLS))) {
    snprintf(rbuf, sizeof(rbuf), "^b[c_v_p for $n: UL=%d, TH=%d, AS=%d; for $N: IMP(aff)=%d, STD(aff)=%d]",
             ch_has_ultrasound, ch_has_thermographic, ch_sees_astral, vict_is_imp_invis, vict_is_just_invis);
    act(rbuf, FALSE, ch, 0, victim, TO_ROLLS);
  }

  // This checks to see if we've resisted their spell (if any). If we haven't, add on their spell invis.
  if (!can_see_through_invis(ch, victim)) {
    vict_is_imp_invis  |= IS_AFFECTED(victim, AFF_SPELLIMPINVIS);
    vict_is_just_invis |= IS_AFFECTED(victim, AFF_SPELLINVIS);
    if ((access_level(ch, LVL_PRESIDENT) || access_level(victim, LVL_PRESIDENT))  && (PRF_FLAGGED(ch, PRF_ROLLS) || PRF_FLAGGED(victim, PRF_ROLLS))) {
      snprintf(rbuf, sizeof(rbuf), "^b[c_v_p after adding spells: IMP=%d, STD=%d]", vict_is_imp_invis, vict_is_just_invis);
      act(rbuf, FALSE, ch, 0, victim, TO_ROLLS);
    }
  }

  bool vict_is_inanimate = MOB_FLAGGED(victim, MOB_INANIMATE);

  if (ch_sees_astral) {
    if (!vict_is_inanimate) {
      // If you're astrally perceiving, you see anything living with no vision penalty.
      // (You get penalties from perceiving, that's handled elsewhere.)
      snprintf(rbuf, sizeof(rbuf), "$n: %s vs living target, so final char-to-char visibility TN is ^c%d^n.",
               MOB_FLAGGED(ch, MOB_DUAL_NATURE) ? "Dual-natured" : "Perceiving",
               modifier);
      act(rbuf, 0, ch, 0, victim, TO_ROLLS);
      return 0;
    } else {
      act("$n: Astral sight ignored-- inanimate, non-spelled target.", 0, ch, 0, victim, TO_ROLLS);
    }
    // Otherwise, fall through.
  }
  else if (IS_ASTRAL(victim) && !AFF_FLAGGED(victim, AFF_MANIFEST)) {
    // This shouldn't have happened in the first place.
    mudlog("SYSERR: Received non-perceiving ch and purely-astral vict to calculate_vision_penalty()!", ch, LOG_SYSLOG, TRUE);

    // You're not astrally perceiving, and your victim is a non-manifested astral being. Blind fire.
    if (GET_POWER(ch, ADEPT_BLIND_FIGHTING)) {
      modifier = BLIND_FIGHTING_MAX;
      snprintf(rbuf, sizeof(rbuf), "%s: Non-perceiving character fighting non-manifested astral: %d after Blind Fighting", GET_CHAR_NAME(ch), modifier);
    } else {
      modifier = BLIND_FIRE_PENALTY;
      snprintf(rbuf, sizeof(rbuf), "%s: Non-perceiving character fighting non-manifested astral: %d", GET_CHAR_NAME(ch), modifier);
    }

    act(rbuf, 0, ch, 0, 0, TO_ROLLS);
    if (ch->in_room != victim->in_room)
      act(rbuf, 0, victim, 0, 0, TO_ROLLS);

    snprintf(rbuf, sizeof(rbuf), "$n: In bugged case, final char-to-char visibility TN is ^c%d^n.", modifier);
    act(rbuf, 0, victim, 0, ch, TO_ROLLS);

    return modifier;
  }

  // We are deliberately not handling anything to do with partial light penalties here.
  // That's handled in modify_target_rbuf_raw().

  // Ultrasound reduces all vision penalties by half (some restrictions apply, see store for details; SR3 p282)
  if (ch_has_ultrasound) {
    // Improved invisibility works against tech sensors. Regular invis does not, so we don't account for it here.
    if (vict_is_imp_invis) {
      // Invisibility penalty, we're at the full +8 from not being able to see them. This overwrites weather effects etc.
      // We don't apply the Adept blind fighting max here, as that's calculated later on.
      modifier = BLIND_FIRE_PENALTY;
      snprintf(rbuf, sizeof(rbuf), "%s: Ultrasound-using character fighting improved invis: TN %d", GET_CHAR_NAME(ch), modifier);
    } else {
      snprintf(rbuf, sizeof(rbuf), "%s: Ultrasound-using character", GET_CHAR_NAME(ch));
    }

    // Silence level is the highest of the room's silence or the victim's stealth. Stealth in AwakeMUD is a hybrid of
    //  the silence and stealth spells, so it's a little OP but whatever.
    int silence_level = MAX(get_ch_in_room(ch)->silence[ROOM_NUM_SPELLS_OF_TYPE], get_ch_in_room(victim)->silence[ROOM_NUM_SPELLS_OF_TYPE]);
    silence_level = MAX(silence_level, get_spell_affected_successes(victim, SPELL_STEALTH));

    // SR3 p196: Silence spell increases hearing perception test TN up to the lower of its successes or the spell's rating.
    // We don't have that test, so it seems reasonable to just shoehorn the TN increase in here.
    if (silence_level > 0) {
      modifier += silence_level;
      snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), ", silence/stealth level adds %d", silence_level);
    }

    // Finally, apply ultrasound division. We add one since the system expects us to round up and we're using truncating integer math.
    if (modifier > 0) {
      modifier = (modifier + 1) / 2;
      snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "; /2 (round up) = %d", modifier);

      act(rbuf, 0, ch, 0, 0, TO_ROLLS);
      if (ch->in_room != victim->in_room)
        act(rbuf, 0, victim, 0, 0, TO_ROLLS);
    }
  }

  // No ultrasound. Check for thermographic vision.
  else if (ch_has_thermographic) {
    // Improved invis? You can't see them.
    if (vict_is_imp_invis) {
      modifier = BLIND_FIRE_PENALTY;
      snprintf(rbuf, sizeof(rbuf), "%s: Thermographic-using character fighting improved invis: %d", GET_CHAR_NAME(ch), modifier);
    }

    // House rule: Since everyone and their dog has thermographic, now standard invis is actually a tiny bit useful.
    // This deviates from canon, where thermographic can see through standard invis.
    else if (vict_is_just_invis) {
      modifier = 2;
      snprintf(rbuf, sizeof(rbuf), "%s: Thermographic-using character fighting invis (second stanza): %d", GET_CHAR_NAME(ch), modifier);
    }

    if (modifier > 0) {
      act(rbuf, 0, ch, 0, 0, TO_ROLLS);
      if (ch->in_room != victim->in_room)
        act(rbuf, 0, victim, 0, 0, TO_ROLLS);
    }
  }

  // Low-light and normal vision aren't any help here.
  else {
    if (vict_is_imp_invis || vict_is_just_invis) {
      modifier = BLIND_FIRE_PENALTY;

      snprintf(rbuf, sizeof(rbuf), "%s: Low-light or standard vision fighting invis: %d", GET_CHAR_NAME(ch), modifier);

      act(rbuf, 0, ch, 0, 0, TO_ROLLS);
      if (ch->in_room != victim->in_room)
        act(rbuf, 0, victim, 0, 0, TO_ROLLS);
    }
  }

  // MitS p148: Penalty is capped to +4 with Blind Fighting. We also cap it to +8 without, since that's Blind Fire.
  if (GET_POWER(ch, ADEPT_BLIND_FIGHTING)) {
    if (modifier > BLIND_FIGHTING_MAX) {
      modifier = BLIND_FIGHTING_MAX;

      snprintf(rbuf, sizeof(rbuf), "%s: Vision penalty capped to %d by Blind Fighting", GET_CHAR_NAME(ch), modifier);

      act(rbuf, 0, ch, 0, 0, TO_ROLLS);
      if (ch->in_room != victim->in_room)
        act(rbuf, 0, victim, 0, 0, TO_ROLLS);
    }
  } else {
    if (modifier > BLIND_FIRE_PENALTY) {
      modifier = BLIND_FIRE_PENALTY;

      snprintf(rbuf, sizeof(rbuf), "%s: Capped to %d", GET_CHAR_NAME(ch), modifier);

      act(rbuf, 0, ch, 0, 0, TO_ROLLS);
      if (ch->in_room != victim->in_room)
        act(rbuf, 0, victim, 0, 0, TO_ROLLS);
    }
  }

  // With the recent bugfix that enabled the invis penalties that were supposed to be there, there's been a lot
  // of concern about mundanes getting the short end of the stick-- they do not have the ability to perceive to
  // get around invis penalties like awakened characters do. Thus, this decidedly non-canon hack:
  if (IS_NPC(victim) && GET_TRADITION(ch) == TRAD_MUNDANE) {
    snprintf(rbuf, sizeof(rbuf), "%s: Negating invis penalty due to mundane PC vs NPC.", GET_CHAR_NAME(ch));
    modifier = 0;
  }

  snprintf(rbuf, sizeof(rbuf), "%s: Final char-to-char visibility TN: ^c%d^n", GET_CHAR_NAME(ch), modifier);

  act(rbuf, 0, ch, 0, 0, TO_ROLLS);
  if (ch->in_room != victim->in_room)
    act(rbuf, 0, victim, 0, 0, TO_ROLLS);

  return modifier;
}
#undef INVIS_CODE_STAFF
#undef INVIS_CODE_TOTALINVIS

//todo: single shot weaps can only be fired once per combat phase-- what does this mean for us?

int find_sight(struct char_data *ch)
{
  // You can always see the room you're in.
  int sight = 1;

  // High-level staff see forever.
  if (!access_level(ch, LVL_VICEPRES)) {
    sight = 4;
  }
  // Lower-level staff, morts, and NPCs are limited by sight and weather.
  else {
    sight += get_vision_mag(ch);

    /* add more weather conditions here to affect scan */
    if (SECT(get_ch_in_room(ch)) != SPIRIT_HEARTH) {
      switch (weather_info.sky) {
        case SKY_RAINING:
          sight -= 1;
          break;
        case SKY_LIGHTNING:
          sight -= 2;
          break;
      }
    }

    sight = MIN(4, MAX(1, sight));
  }

  return sight;
}

int find_weapon_range(struct char_data *ch, struct obj_data *weapon)
{
  int temp;

  if ( weapon == NULL )
    return 0;

  if (GET_OBJ_TYPE(weapon) == ITEM_FIREWEAPON)
  {
    temp = MIN(MAX(1, ((int)(GET_STR(ch)/3))), 4);
    return temp;
  }

  switch(GET_WEAPON_ATTACK_TYPE(weapon))
  {
    case WEAP_HOLDOUT:
    case WEAP_LIGHT_PISTOL:
    case WEAP_MACHINE_PISTOL:
    case WEAP_HEAVY_PISTOL:
      return 1;
    case WEAP_GREN_LAUNCHER:
    case WEAP_SMG:
    case WEAP_SHOTGUN:
      return 2;
    case WEAP_MINIGUN:
    case WEAP_ASSAULT_RIFLE:
    case WEAP_SPORT_RIFLE:
      return 3;
    case WEAP_LMG:
    case WEAP_MMG:
    case WEAP_HMG:
    case WEAP_SNIPER_RIFLE:
    case WEAP_CANNON:
    case WEAP_MISS_LAUNCHER:
      return 4;
    default:
      return 0;
  }
}

bool ranged_response(struct char_data *ch, struct char_data *vict)
{
  int range, sight, distance, dir;
  struct room_data *room = NULL, *nextroom = NULL;
  struct char_data *temp;
  bool is_responding = FALSE;

  // Precondition checking.
  if (!vict
      || ch->in_room == vict->in_room
      || GET_POS(vict) <= POS_STUNNED
      || (!ch->in_room || ch->in_room->peaceful)
      || (!vict->in_room || vict->in_room->peaceful)
      || CH_IN_COMBAT(vict))
  {
    return FALSE;
  }

  // Stand up if you're not already standing.
  if (GET_POS(vict) < POS_FIGHTING)
    GET_POS(vict) = POS_STANDING;

  // Wimpy mobs try to flee, unless they're sentinels.
  if (!AFF_FLAGGED(vict, AFF_PRONE) && (IS_NPC(vict) && MOB_FLAGGED(vict, MOB_WIMPY) && !MOB_FLAGGED(vict, MOB_SENTINEL))) {
    do_flee(vict, "", 0, 0);
    return FALSE;
  }

  bool vict_will_not_move = !IS_NPC(vict) || MOB_FLAGGED(vict, MOB_SENTINEL) || MOB_FLAGGED(vict, MOB_EMPLACED) || AFF_FLAGGED(vict, AFF_PRONE);

  // If we're wielding a ranged weapon, try to shoot.
  if (RANGE_OK(vict)) {
    sight = find_sight(vict);
    range = find_weapon_range(vict, GET_EQ(vict, WEAR_WIELD));

    for (dir = 0; dir < NUM_OF_DIRS  && !is_responding; dir++) {
      room = vict->in_room;
      if (!room) {
        snprintf(buf, sizeof(buf), "SYSERR: Invalid room for %s (%ld) in ranged_response.",
                GET_CHAR_NAME(vict), GET_MOB_VNUM(vict));
        mudlog(buf, vict, LOG_SYSLOG, TRUE);
        break;
      }

      if (CAN_GO2(room, dir)) {
        nextroom = EXIT2(room, dir)->to_room;
      } else {
        nextroom = NULL;
      }

      for (distance = 1; !is_responding && (nextroom && (distance <= 4)); distance++) {
        // Players and sentinels never charge, so we stop processing when we're getting attacked from beyond our weapon or sight range.
        if (vict_will_not_move && (distance > range || distance > sight)) {
          if (IS_NPC(vict)) {
            char buf[100];
            snprintf(buf, sizeof(buf), "Sentinel/prone/emplaced $n not ranged-responding: $N is out of range (weap %d, sight %d).", range, sight);
            act(buf, FALSE, vict, 0, ch, TO_ROLLS);
          }
          return FALSE;
        }

        // We're either within weapon AND sight range, or we're willing to charge towards the target.
        for (temp = nextroom->people; !is_responding && temp; temp = temp->next_in_room) {
          // Found our target?
          if (temp == ch) {
            // If we're over our weapon's max range:
            if (distance > range && IS_NPC(vict)) {
              act("$n charges towards $s distant foe.", TRUE, vict, 0, 0, TO_ROOM);
              act("You charge after $N.", FALSE, vict, 0, ch, TO_CHAR);
              char_from_room(vict);

              // Move one room in their direction.
              char_to_room(vict, EXIT2(room, dir)->to_room);

              if (vict->in_room == ch->in_room) {
                act("$n arrives in a rush of fury, immediately attacking $N!", TRUE, vict, 0, ch, TO_NOTVICT);
                act("$n arrives in a rush of fury, rushing straight towards you!", TRUE, vict, 0, ch, TO_VICT);
              } else {
                act("$n arrives in a rush of fury, searching for $s attacker!!", TRUE, vict, 0, 0, TO_ROOM);
              }

              set_fighting(vict, ch);
              return TRUE;
            }

            // Within max range? Open fire.
            else if (distance <= range) {
              set_fighting(vict, ch);
              return TRUE;
            }

            // Otherwise, we're a PC with distance > range. Do nothing.
          }
        }

        // Prep for our next iteration.
        room = nextroom;
        if (CAN_GO2(room, dir))
          nextroom = EXIT2(room, dir)->to_room;
        else
          nextroom = NULL;
      }
    }

    // If we got here, they're beyond our ability to damage.
    act("$n not ranged-responding: Cannot find $N.", FALSE, vict, 0, ch, TO_ROLLS);
  }

  // Mobile NPC with a melee weapon. They only charge one room away for some reason.
  else if (!vict_will_not_move) {
    for (dir = 0; dir < NUM_OF_DIRS && !is_responding; dir++) {
      if (CAN_GO(vict, dir) && EXIT2(vict->in_room, dir)->to_room == ch->in_room) {
        act("$n charges towards $s distant foe.", TRUE, vict, 0, 0, TO_ROOM);
        act("You charge after $N.", FALSE, vict, 0, ch, TO_CHAR);
        char_from_room(vict);
        char_to_room(vict, ch->in_room);
        set_fighting(vict, ch);
        act("$n arrives in a rush of fury, immediately attacking $N!", TRUE, vict, 0, ch, TO_NOTVICT);
        act("$n arrives in a rush of fury, rushing straight towards you!", TRUE, vict, 0, ch, TO_VICT);
        return TRUE;
      }
    }
  }
  return is_responding;
}

void explode_explosive_grenade(struct char_data *ch, struct obj_data *weapon, struct room_data *room)
{
  int damage_total, i, power, level;
  struct char_data *victim, *next_vict;
  struct obj_data *obj, *next;

  power = GET_WEAPON_POWER(weapon);
  level = GET_WEAPON_DAMAGE_CODE(weapon);

  extract_obj(weapon);

  if (ROOM_FLAGGED(room, ROOM_PEACEFUL) || ROOM_FLAGGED(room, ROOM_HOUSE)) {
    mudlog_vfprintf(ch, LOG_CHEATLOG, "Somehow, %s got an explosive grenade into an invalid room!", GET_CHAR_NAME(ch));
    return;
  }

  if (room->people)
  {
    act("The room is lit by an explosion!", FALSE, room->people, 0, 0, TO_ROOM);
    act("The room is lit by an explosion!", FALSE, room->people, 0, 0, TO_CHAR);
  }

  /*
  for (obj = room->contents; obj; obj = next)
  {
    next = obj->next_content;
    damage_obj(NULL, obj, level * 2 + (int)(power / 6), DAMOBJ_EXPLODE);
  }
  */

  for (victim = room->people; victim; victim = next_vict)
  {
    next_vict = victim->next_in_room;
    if (IS_ASTRAL(victim))
      continue;

    if (!IS_NPC(victim) && victim != ch)
      continue;

    // Skip people you can't injure (Grog, etc)
    if (!can_hurt(ch, victim, TYPE_EXPLOSION, TRUE) || MOB_FLAGGED(victim, MOB_IMMEXPLODE))
      continue;

    act("You are hit by the flames!", FALSE, victim, 0, 0, TO_CHAR);

    for (obj = victim->carrying; obj; obj = next) {
      next = obj->next_content;
      if (number(1, 100) < 50)
        damage_obj(NULL, obj, level * 2 + (int)(power / 6), DAMOBJ_EXPLODE);
    }

    for (i = 0; i < (NUM_WEARS - 1); i++)
      if (GET_EQ(victim, i) && number(1, 100) < 100)
        damage_obj(NULL, GET_EQ(victim, i), level * 2 + (int)(power / 6),
                   DAMOBJ_EXPLODE);

    if (IS_NPC(victim) && !CH_IN_COMBAT(victim)) {
      GET_DEFENSE(victim) = GET_COMBAT(victim);
      GET_OFFENSE(victim) = 0;
    }
    damage_total =
    convert_damage(stage((number(1, 3) - success_test(GET_BOD(victim) +
                                                      GET_BODY(victim), MAX(2, (power -
                                                                                (int)(GET_IMPACT(victim) / 2))) + modify_target(victim))),
                         level));
    if (!ch)
      damage(victim, victim, damage_total, TYPE_EXPLOSION, PHYSICAL);
    else
      damage(ch, victim, damage_total, TYPE_EXPLOSION, PHYSICAL);
  }

  for (i = 0; i < NUM_OF_DIRS; i++)
    if (room->dir_option[i] && IS_SET(room->dir_option[i]->exit_info, EX_CLOSED))
      damage_door(NULL, room, i, level * 2 + (int)(power / 6), DAMOBJ_EXPLODE);

  propagate_sound_from_room(room,
                            std::vector<const char *> {
                              "The sharp *BANG* of an explosive grenade sounds from nearby!\r\n",
                              "You hear an explosion from somewhere not far off!\r\n",
                              "You hear a muffled explosion from somewhere in the distance.\r\n"
                            },
                            TRUE,
                            ch);
}

void explode_flashbang_grenade(struct char_data *ch, struct obj_data *weapon, struct room_data *room)
{
  int power = GET_WEAPON_POWER(weapon);

  extract_obj(weapon);

  if (ROOM_FLAGGED(room, ROOM_PEACEFUL) || ROOM_FLAGGED(room, ROOM_HOUSE)) {
    mudlog_vfprintf(ch, LOG_CHEATLOG, "Somehow, %s got a flashbang grenade into an invalid room!", GET_CHAR_NAME(ch));
    return;
  }

  if (room->people)
  {
    send_to_room("^WAn enormous **BANG** leaves your ears ringing!^n\r\n", room);
  }

  for (struct char_data *next_vict, *victim = room->people; victim; victim = next_vict)
  {
    next_vict = victim->next_in_room;

    // Cannot affect astral projections.
    if (IS_ASTRAL(victim)) {
      act("Skipping $n: astral", TRUE, victim, 0, 0, TO_ROLLS);
      continue;
    }

    // Cannot affect players other than the thrower.
    if (!IS_NPC(victim) && victim != ch) {
      act("Skipping $n: player", TRUE, victim, 0, 0, TO_ROLLS);
      continue;
    }

    // Skip people you can't injure (Grog, etc)
    if (!can_hurt(ch, victim, TYPE_EXPLOSION, TRUE) || MOB_FLAGGED(victim, MOB_IMMEXPLODE)) {
      act("Skipping $n: can't be hurt", TRUE, victim, 0, 0, TO_ROLLS);
      continue;
    }

    char rbuf[1000];
    int tn = power;

    snprintf(rbuf, sizeof(rbuf), "^yFlashbang eval for $N^y: %d initial TN; ", tn);

    if (has_flare_compensation(victim)) {
      tn -= 4;
      strlcat(rbuf, "-4 flare comp, ", sizeof(rbuf));
    }

    tn += modify_target_rbuf(victim, rbuf, sizeof(rbuf));

    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), ". Final TN %d.", tn);

    int dice = GET_BOD(victim);

    // Roll to see if they resist. You get a bonus from flare comp.
    int successes = success_test(dice, tn);

    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), " With %d dice, got %d success%s.^n", dice, successes, successes != 1 ? "es" : "");
    act(rbuf, FALSE, victim, 0, victim, TO_ROLLS);
    if (victim->in_room != ch->in_room) {
      act(rbuf, FALSE, ch, 0, victim, TO_ROLLS);
    }

    // They successfully resisted? Bail out.
    if (successes > 0) {
      // We have to do the ranged response here since we won't get a chance later.
      if (IS_NPC(victim))
        ranged_response(ch, victim);
      continue;
    }

    // Failed to resist? Roll for each of their spells to see if they lose it.
    for (struct sustain_data *next, *sust = GET_SUSTAINED(victim); sust; sust = next) {
      next = sust->next;
      // Only affect spells that are victim records, since this means that this is the effect portion of the spell record pair.
      if (!sust->caster) {
        // House rule: Make an opposed WIL test. If net successes is negative, lose spell successes.
        int grenade_tn = sust->other ? GET_WIL(sust->other) : 1;
        int grenade_dice = power;
        int grenade_successes = success_test(grenade_dice, grenade_tn);

        char rbuf[1000];
        int char_tn = MAX(power, sust->force) + damage_modifier(victim, rbuf, sizeof(rbuf));
        int char_dice = GET_WIL(victim);
        int char_successes = success_test(char_dice, char_tn);

        int net_successes = char_successes - grenade_successes;

        if (access_level(ch, LVL_BUILDER)) {
          snprintf(rbuf, sizeof(rbuf), "Spell sustain test for $N's %s: ch's %d dice at TN %d VS gren's %d @ %d: %d net success%s.",
                   get_spell_name(sust->spell, sust->subtype),
                   char_dice,
                   char_tn,
                   grenade_dice,
                   grenade_tn,
                   net_successes,
                   net_successes != 1 ? "es" : "");
          act(rbuf, TRUE, ch, 0, victim, TO_ROLLS);
        }

        if (net_successes < 0) {
          if (-1 * net_successes >= sust->success) {
            if (access_level(ch, LVL_BUILDER)) {
              send_to_char(ch, "^y- Grenade stripped %s from ^y%s.^n\r\n", get_spell_name(sust->spell, sust->subtype), GET_CHAR_NAME(victim));
            }
            end_sustained_spell(victim, sust);
          } else {
            if (access_level(ch, LVL_BUILDER)) {
              send_to_char(ch, "^y- Grenade weakened %s^y's %s from %d to %d.^n\r\n", GET_CHAR_NAME(victim), get_spell_name(sust->spell, sust->subtype), sust->success, sust->success + net_successes);
            }
            send_to_char(victim, "Your control over %s weakens.\r\n", get_spell_name(sust->spell, sust->subtype));
            // Remember that net is negative, so we add it here to lower the sustain successes.
            sust->success += net_successes;
          }
        }
      }
    }

    // Have the NPC fire back.
    if (IS_NPC(victim))
      ranged_response(ch, victim);
  }

  propagate_sound_from_room(room,
                            std::vector<const char *> {
                              "The massive *BOOM* of a concussive blast sounds from nearby!\r\n",
                              "You hear a loud concussive blast from somewhere not far off!\r\n",
                              "You hear a concussive blast from somewhere in the area.\r\n",
                              "You hear a muffled concussive blast from somewhere in the distance.\r\n"
                            },
                            TRUE,
                            ch);
}

void target_explode(struct char_data *ch, struct obj_data *weapon, struct room_data *room, int mode)
{
  int damage_total, i;
  struct char_data *victim, *next_vict;
  struct obj_data *obj, *next;

  snprintf(buf, sizeof(buf), "The room is lit by a%s explosion!",
          (GET_WEAPON_ATTACK_TYPE(weapon) == TYPE_ROCKET ? " massive" : "n"));

  if (room->people)
  {
    act(buf, FALSE, room->people, 0, 0, TO_ROOM);
    act(buf, FALSE, room->people, 0, 0, TO_CHAR);
  }

  for (obj = room->contents; obj; obj = next)
  {
    next = obj->next_content;
    damage_obj(NULL, obj, GET_WEAPON_DAMAGE_CODE(weapon) * 2 +
               (int)(GET_WEAPON_POWER(weapon) / 6), DAMOBJ_EXPLODE);
  }

  for (victim = room->people; victim; victim = next_vict)
  {
    next_vict = victim->next_in_room;
    if (IS_ASTRAL(victim))
      continue;

    act("You are hit by the flames!", FALSE, victim, 0, 0, TO_CHAR);

    for (obj = victim->carrying; obj; obj = next) {
      next = obj->next_content;
      if (number(1, 100) < 50)
        damage_obj(NULL, obj, GET_WEAPON_DAMAGE_CODE(weapon) * 2 +
                   (int)(GET_WEAPON_POWER(weapon) / 6), DAMOBJ_EXPLODE);
    }

    for (i = 0; i < (NUM_WEARS - 1); i++)
      if (GET_EQ(victim, i) && number(1, 100) < 100)
        damage_obj(NULL, GET_EQ(victim, i), GET_WEAPON_DAMAGE_CODE(weapon) * 2 +
                   (int)(GET_WEAPON_POWER(weapon) / 6), DAMOBJ_EXPLODE);

    if (IS_NPC(victim) && !CH_IN_COMBAT(victim)) {
      GET_DEFENSE(victim) = GET_COMBAT(victim);
      GET_OFFENSE(victim) = 0;
    }
    if (!mode) {
      if (GET_WEAPON_ATTACK_TYPE(weapon) == TYPE_ROCKET)
        damage_total = convert_damage(stage((number(3,6) - success_test(GET_BOD(victim) + GET_BODY(victim),
                                                                        MAX(2, (GET_WEAPON_POWER(weapon) - (int)(GET_IMPACT(victim) / 2))) +
                                                                        modify_target(victim))), GET_WEAPON_DAMAGE_CODE(weapon)));
      else
        damage_total = convert_damage(stage((number(2,5) - success_test(GET_BOD(victim) + GET_BODY(victim),
                                                                        MAX(2, (GET_WEAPON_POWER(weapon) - (int)(GET_IMPACT(victim) / 2))) +
                                                                        modify_target(victim))), GET_WEAPON_DAMAGE_CODE(weapon)));
    } else
      damage_total = convert_damage(stage((number(2,5) - success_test(GET_BOD(victim) + GET_BODY(victim),
                                                                      MAX(2, (GET_WEAPON_POWER(weapon) - 4 - (int)(GET_IMPACT(victim) / 2))) +
                                                                      modify_target(victim))), GET_WEAPON_DAMAGE_CODE(weapon) - 1));
    damage(ch, victim, damage_total, TYPE_EXPLOSION, PHYSICAL);
  }

  if (!mode)
    for (i = 0; i < NUM_OF_DIRS; i++)
      if (room->dir_option[i] &&
          IS_SET(room->dir_option[i]->exit_info, EX_CLOSED))
        damage_door(NULL, room, i, GET_WEAPON_DAMAGE_CODE(weapon) * 2 +
                    (int)(GET_WEAPON_POWER(weapon) / 6), DAMOBJ_EXPLODE);
}

void range_combat(struct char_data *ch, char *target, struct obj_data *weapon,
                  int range, int dir)
{
  int distance, sight, temp, temp2, left, right;
  struct room_data *scatter[4];
  struct room_data *room = NULL, *nextroom = NULL;
  struct char_data *vict = NULL;

  for (int i = 0; i < 4; i++)
    scatter[i] = NULL;

  struct room_data *in_room = ch->char_specials.rigging ? ch->char_specials.rigging->in_room : ch->in_room;

  if (in_room->peaceful || ROOM_FLAGGED(in_room, ROOM_HOUSE))
  {
    send_to_char("This room just has a peaceful, easy feeling...\r\n", ch);
    return;
  }

  sight = find_sight(ch);

  if (CAN_GO2(in_room, dir))
    nextroom = EXIT2(in_room, dir)->to_room;
  else
    nextroom = NULL;

  if (GET_OBJ_TYPE(weapon) == ITEM_WEAPON && GET_WEAPON_ATTACK_TYPE(weapon) == WEAP_GRENADE)
  {
    if (!nextroom) {
      send_to_char("There seems to be something in the way...\r\n", ch);
      return;
    }
    if (nextroom->peaceful || ROOM_FLAGGED(nextroom, ROOM_HOUSE)) {
      send_to_char("Nah - leave them in peace.\r\n", ch);
      return;
    }
    if (CH_IN_COMBAT(ch))
      stop_fighting(ch);
    act("You pull the pin and throw $p!", FALSE, ch, weapon, 0, TO_CHAR);
    act("$n pulls the pin and throws $p!", FALSE, ch, weapon, 0, TO_ROOM);

    WAIT_STATE(ch, PULSE_VIOLENCE);

    temp = MAX(1, GET_SKILL(ch, SKILL_THROWING_WEAPONS));

    if (!number(0, GRENADE_DEFECTIVE_CHECK_DIVISOR)) {
      send_to_room("A defective grenade lands on the floor.\r\n", nextroom);
      send_to_char(ch, "You hear a *clink* as your defective grenade bounces away...\r\n");

      for (struct char_data *vict = nextroom->people; vict; vict = vict->next_in_room) {
        if (!IS_NPC(vict))
          continue;

        if (CAN_SEE(vict, ch)) {
          ranged_response(ch, vict);
        } else {
          GET_MOBALERT(vict) = MALERT_ALARM;
          GET_MOBALERTTIME(vict) = MAX(GET_MOBALERTTIME(vict), 30);
        }
      }

      extract_obj(weapon);
      return;
    }

    int tn = 5;

    char rbuf[1000];
    snprintf(rbuf, sizeof(rbuf), "Grenade-throw check for $N: Starting TN %d, ", tn);

    int dice = get_skill(ch, SKILL_THROWING_WEAPONS, tn);
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "after get_skill() %d, ", tn);

    tn += modify_target_rbuf(ch, rbuf, sizeof(rbuf));
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "total %d. Rolling %d dice: ", tn, dice);

    int successes = success_test(dice, tn);
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "%d success%s. Will%s scatter.", successes, successes == 1 ? "" : "es", successes >= GRENADE_SCATTER_THRESHOLD ? " NOT" : "");

    act(rbuf, FALSE, ch, 0, ch, TO_ROLLS);
    if (nextroom->people)
      act(rbuf, FALSE, nextroom->people, 0, ch, TO_ROLLS);

    if (successes < GRENADE_SCATTER_THRESHOLD) {
      left = -1;
      right = -1;
      if (dir < UP) {
        left = (dir - 1) < NORTH ? NORTHWEST : (dir - 1);
        right = (dir + 1) > NORTHWEST ? NORTH : (dir + 1);
      }

      // Initialize scatter array.
      scatter[0] = in_room;
      scatter[1] = left >= 0 && CAN_GO(ch, left) ? EXIT(ch, left)->to_room : NULL;
      scatter[2] = right >= 0 && CAN_GO(ch, right) ? EXIT(ch, right)->to_room : NULL;
      scatter[3] = CAN_GO2(nextroom, dir) ? EXIT2(nextroom, dir)->to_room : NULL;

      for (temp = 0, temp2 = 0; temp2 < 4; temp2++)
        if (scatter[temp2])
          temp++;
      for (temp2 = 0; temp2 < 4; temp2++) {
        if (scatter[temp2] && !number(0, temp-1)) {
          if (temp2 == 0) {
            act("$p deflects due to $n's poor accuracy, landing at $s feet.", FALSE, ch, weapon, 0, TO_ROOM);
            snprintf(buf, sizeof(buf), "Your realize your aim must've been off-target as $p lands at your feet.");
          } else if (temp2 == 3)
            snprintf(buf, sizeof(buf), "Your aim is slightly off, going past its target.");
          else
            snprintf(buf, sizeof(buf), "Your aim is slightly off, and $p veers to the %s.", dirs[temp2 == 1 ? left : right]);
          act(buf, FALSE, ch, weapon, 0, TO_CHAR);
          switch (GET_WEAPON_GRENADE_TYPE(weapon)) {
            case GRENADE_TYPE_EXPLOSIVE:
              explode_explosive_grenade(ch, weapon, scatter[temp2]);
              break;
            case GRENADE_TYPE_FLASHBANG:
              explode_flashbang_grenade(ch, weapon, scatter[temp2]);
              break;
            default:
              mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Unknown grenade type %d in range_combat()!", GET_WEAPON_GRENADE_TYPE(weapon));
              break;
          }
          return;
        }
      }
    }

    switch (GET_WEAPON_GRENADE_TYPE(weapon)) {
      case GRENADE_TYPE_EXPLOSIVE:
        explode_explosive_grenade(ch, weapon, nextroom);
        break;
      case GRENADE_TYPE_FLASHBANG:
        explode_flashbang_grenade(ch, weapon, nextroom);
        break;
      default:
        mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Unknown grenade type %d in range_combat()!", GET_WEAPON_GRENADE_TYPE(weapon));
        break;
    }

    return;
  }

  for (distance = 1; (nextroom && (distance <= sight)); distance++)
  {
    if ((vict = get_char_room(target, nextroom)) && vict != ch &&
        CAN_SEE(ch, vict))
      break;
    vict = NULL;
    room = nextroom;
    if (CAN_GO2(room, dir))
      nextroom = EXIT2(room, dir)->to_room;
    else
      nextroom = NULL;
  }

  if (vict)
  {
    if (vict == FIGHTING(ch)) {
      send_to_char("You're doing the best you can!\r\n", ch);
      return;
    }

    if (get_ch_in_room(vict)->peaceful) {
      send_to_char("Nah - leave them in peace.\r\n", ch);
      return;
    }

    if (distance > range) {
      act("$N seems to be out of $p's range.", FALSE, ch, weapon, vict, TO_CHAR);
      return;
    }

    if (!IS_NPC(vict)) {
      if (!IS_NPC(ch) && !(PRF_FLAGGED(ch, PRF_PKER) && PRF_FLAGGED(vict, PRF_PKER))) {
        send_to_char("You and your opponent must both be toggled PK for that.\r\n", ch);
        return;
      }

#ifdef IGNORING_IC_ALSO_IGNORES_COMBAT
      if (IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
        send_to_char("You and your opponent must both be toggled PK for that.\r\n", ch);
        log_attempt_to_bypass_ic_ignore(ch, vict, "range_combat");
        return;
      }

      if (IS_IGNORING(ch, is_blocking_ic_interaction_from, vict)) {
        send_to_char("You can't attack someone you're ignoring.\r\n", ch);
        return;
      }
#endif
    }
    else if (npc_is_protected_by_spec(vict)) {
      send_to_char("You can't attack protected NPCs like that.\r\n", ch);
      return;
    }

    SHOOTING_DIR(ch) = dir;

    if (GET_OBJ_TYPE(weapon) == ITEM_FIREWEAPON) {
      act("$n draws $p and fires into the distance!", TRUE, ch, weapon, 0, TO_ROOM);
      act("You draw $p, aim it at $N and fire!", FALSE, ch, weapon, vict, TO_CHAR);
      check_killer(ch, vict);
      if (IS_NPC(vict) && !IS_PROJECT(vict) && !CH_IN_COMBAT(vict)) {
        GET_DEFENSE(vict) = GET_COMBAT(vict);
        GET_OFFENSE(vict) = 0;
      }
      if (CH_IN_COMBAT(ch))
        stop_fighting(ch);
      hit(ch,
          vict,
          (GET_EQ(ch, WEAR_WIELD) ? GET_EQ(ch, WEAR_WIELD) : GET_EQ(ch, WEAR_HOLD)),
          (GET_EQ(vict, WEAR_WIELD) ? GET_EQ(vict, WEAR_WIELD) : GET_EQ(vict, WEAR_HOLD)),
          NULL);
      WAIT_STATE(ch, 2 * PULSE_VIOLENCE);
      return;
    }

    act("$n aims $p and fires into the distance!", TRUE, ch, weapon, 0, TO_ROOM);
    act("You aim $p at $N and fire!", FALSE, ch, weapon, vict, TO_CHAR);
    if (IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))) {
      check_killer(ch, vict);
      if (GET_WEAPON_ATTACK_TYPE(weapon) < TYPE_PISTOL || has_ammo_no_deduct(ch, weapon)) {
        if (IS_NPC(vict) && !IS_PROJECT(vict) && !CH_IN_COMBAT(vict)) {
          GET_DEFENSE(vict) = GET_COMBAT(vict);
          GET_OFFENSE(vict) = 0;
        }
        if (CH_IN_COMBAT(ch))
          stop_fighting(ch);
        hit(ch,
            vict,
            (GET_EQ(ch, WEAR_WIELD) ? GET_EQ(ch, WEAR_WIELD) : GET_EQ(ch, WEAR_HOLD)),
            (GET_EQ(vict, WEAR_WIELD) ? GET_EQ(vict, WEAR_WIELD) : GET_EQ(vict, WEAR_HOLD)),
            NULL);
        ranged_response(ch, vict);
      } else
        send_to_char("*Click*\r\n", ch);
      WAIT_STATE(ch, 2 * PULSE_VIOLENCE);
    } else {
      if (!has_ammo(ch, weapon))
        return;
      stop_fighting(ch);
      if (GET_OBJ_VAL(weapon, 3) == WEAP_MISS_LAUNCHER) {
        if (!GET_SKILL(ch, SKILL_MISSILE_LAUNCHERS))
          temp = MAX(1, GET_SKILL(ch, SKILL_GUNNERY));
        else
          temp = GET_SKILL(ch, SKILL_MISSILE_LAUNCHERS);
      } else {
        if (!GET_SKILL(ch, SKILL_GRENADE_LAUNCHERS))
          temp = MAX(1, GET_SKILL(ch, SKILL_FIREARMS));
        else
          temp = GET_SKILL(ch, SKILL_GRENADE_LAUNCHERS);
      }

      WAIT_STATE(ch, 2 * PULSE_VIOLENCE);

      if (!number(0,49)) {
        snprintf(buf, sizeof(buf), "A defective %s lands on the floor.",
                (GET_OBJ_VAL(weapon, 3) == TYPE_ROCKET ? "rocket" : "grenade"));
        act(buf, TRUE, vict, 0, 0, TO_ROOM);
        act(buf, TRUE, vict, 0, 0, TO_CHAR);
        return;
      } else if (success_test(temp + GET_OFFENSE(ch), 6) <= 0) {
        left = -1;
        right = -1;
        if (dir < UP) {
          left = (dir - 1) < NORTH ? NORTHWEST : (dir - 1);
          right = (dir + 1) > NORTHWEST ? NORTH : (dir + 1);
        }

        // Initialize scatter array.
        scatter[0] = ch->in_room;
        scatter[1] = left >= 0 && CAN_GO(ch, left) ? EXIT(ch, left)->to_room : NULL;
        scatter[2] = right >= 0 && CAN_GO(ch, right) ? EXIT(ch, right)->to_room : NULL;
        scatter[3] = CAN_GO2(nextroom, dir) ? EXIT2(nextroom, dir)->to_room : NULL;

        for (temp = 0, temp2 = 0; temp2 < 4; temp2++)
          if (scatter[temp2])
            temp++;
        for (temp2 = 0; temp2 < 4; temp2++)
          if (scatter[temp2] && !number(0, temp-1)) {
            if (temp2 == 0) {
              act("$p's trajectory is slightly off...", FALSE, ch, weapon, 0, TO_ROOM);
              snprintf(buf, sizeof(buf), "Your arm jerks just before you fire...");
            } else if (temp2 == 3)
              snprintf(buf, sizeof(buf), "Your aim is slightly off, going past $N.");
            else
              snprintf(buf, sizeof(buf), "Your aim is slightly off, the %s veering to the %s.",
                      (GET_OBJ_VAL(weapon, 3) == TYPE_ROCKET ? "rocket" : "grenade"),
                      dirs[temp2 == 1 ? left : right]);
            act(buf, FALSE, ch, weapon, vict, TO_CHAR);
            target_explode(ch, weapon, scatter[temp2], 0);
            if (GET_OBJ_VAL(weapon, 3) == TYPE_ROCKET)
              for (temp = 0; temp < NUM_OF_DIRS; temp++)
                if (CAN_GO2(scatter[temp2], temp))
                  target_explode(ch, weapon, EXIT2(scatter[temp2], temp)->to_room, 1);
            return;
          }
      }
      target_explode(ch, weapon, vict->in_room, 0);
      if (GET_OBJ_VAL(weapon, 3) == TYPE_ROCKET)
        for (temp = 0; temp < NUM_OF_DIRS; temp++)
          if (CAN_GO2(vict->in_room, temp))
            target_explode(ch, weapon, EXIT2(vict->in_room, temp)->to_room, 1);
    }
    return;
  }
  bool found = FALSE;

  if (CAN_GO2(ch->in_room, dir))
    nextroom = EXIT2(ch->in_room, dir)->to_room;
  else
    nextroom = NULL;

  // now we search for a door by the given name
  for (distance = 1; nextroom && distance <= sight; distance++)
  {
    if (EXIT2(nextroom, dir) && EXIT2(nextroom, dir)->keyword &&
        isname(target, EXIT2(nextroom, dir)->keyword) &&
        !IS_SET(EXIT2(nextroom, dir)->exit_info, EX_DESTROYED) &&
        !IS_SET(EXIT2(nextroom, dir)->exit_info, EX_HIDDEN) &&
        (PRF_FLAGGED(ch, PRF_HOLYLIGHT) || has_vision(ch, VISION_THERMOGRAPHIC) ||
         light_level(nextroom) <= LIGHT_NORMALNOLIT ||
         ((light_level(nextroom) == LIGHT_MINLIGHT || light_level(nextroom) == LIGHT_PARTLIGHT) &&
          has_vision(ch, VISION_LOWLIGHT)))) {
           found = TRUE;
           break;
         }
    room = nextroom;
    if (CAN_GO2(room, dir))
      nextroom = EXIT2(room, dir)->to_room;
    else
      nextroom = NULL;
  }

  if (found)
  {
    if (CH_IN_COMBAT(ch)) {
      send_to_char("Maybe you'd better wait...\r\n", ch);
      return;
    } else if (!IS_SET(EXIT2(nextroom, dir)->exit_info, EX_CLOSED) && isname(target, EXIT2(nextroom, dir)->keyword) ) {
      send_to_char("You can only damage closed doors!\r\n", ch);
      return;
    } else if (nextroom->peaceful) {
      send_to_char("Nah - leave it in peace.\r\n", ch);
      return;
    } else if (distance > range) {
      send_to_char(ch, "The %s seems to be out of %s's range.\r\n",
                   CAP(fname(EXIT2(nextroom, dir)->keyword)),
                   weapon->text.name);
      return;
    }

    if (GET_OBJ_TYPE(weapon) == ITEM_FIREWEAPON) {
      act("$n draws $p and fires into the distance!", TRUE, ch, weapon, 0, TO_ROOM);
      snprintf(buf, sizeof(buf), "You draw $p, aim it at the %s and fire!",
              fname(EXIT2(nextroom, dir)->keyword));
      act(buf, FALSE, ch, weapon, vict, TO_CHAR);
    } else {
      act("$n aims $p and fires into the distance!", TRUE, ch, weapon, 0, TO_ROOM);
      snprintf(buf, sizeof(buf), "You aim $p at the %s and fire!",
              fname(EXIT2(nextroom, dir)->keyword));
      act(buf, FALSE, ch, weapon, vict, TO_CHAR);
      if (IS_GUN(GET_OBJ_VAL(weapon, 3)))
        if (!has_ammo(ch, weapon))
          return;
    }
    switch (GET_OBJ_VAL(weapon, 3)) {
      case WEAP_GREN_LAUNCHER:
        target_explode(ch, weapon, nextroom, 0);
        break;
      case WEAP_MISS_LAUNCHER:
        target_explode(ch, weapon, nextroom, 0);
        for (temp = 0; temp < NUM_OF_DIRS; temp++)
          if (CAN_GO2(nextroom, temp))
            target_explode(ch, weapon, EXIT2(nextroom, temp)->to_room, 1);
        break;
      default:
        damage_door(ch, nextroom, dir, GET_OBJ_VAL(weapon, 0), DAMOBJ_PROJECTILE);
        break;
    }
    WAIT_STATE(ch, 2 * PULSE_VIOLENCE);
    return;
  }

  send_to_char(ch, "You can't see any %s there.\r\n", target);
  return;
}

void roll_individual_initiative(struct char_data *ch)
{
  if (AWAKE(ch))
  {
    // While rigging, riggers receive only the modifications given them by the vehicle control rig (see Vehicles and Drones, p. 130) they are using.
    if (AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE)) {
      // Note: Dice don't explode in initiative rolls. This is your base value.
      GET_INIT_ROLL(ch) = GET_REAL_REA(ch) + dice(1, 6);

      for (struct obj_data *rig = ch->cyberware; rig; rig = rig->next_content) {
        if (GET_CYBERWARE_TYPE(rig) == CYB_VCR) {
          // Each level adds +2 to the user’s Reaction and +1D6 Initiative dice while rigging. (SR3 p301)
          GET_INIT_ROLL(ch) += (GET_CYBERWARE_RATING(rig) * 2) + dice(GET_CYBERWARE_RATING(rig), 6);
          break;
        }
      }
    }
    else
      GET_INIT_ROLL(ch) = roll_default_initiative(ch);
    GET_INIT_ROLL(ch) -= damage_modifier(ch, buf, sizeof(buf));
    if (AFF_FLAGGED(ch, AFF_ACTION)) {
      GET_INIT_ROLL(ch) -= 10;
      AFF_FLAGS(ch).RemoveBit(AFF_ACTION);
    }
    if (IS_SPIRIT(ch) || IS_ANY_ELEMENTAL(ch)) {
      if (IS_DUAL(ch))
        GET_INIT_ROLL(ch) += 10;
      else
        GET_INIT_ROLL(ch) += 20;
    }
    // Here we set the Surprise flag to our target if conditions are met. Clearing of flag and alert status are handled
    // in hit_with_multi_weapon_toggle() as soon as a suprised NPC gets damaged in combat. Otherwise we can't
    // properly enter the surprise state due to mobs being continously alert. Because we were handling this in
    // init state and either the flag was getting stripped or the mob became alert before we ever tried to hit it.
    // Also the no defense condition happens in the hit_with_multi_weapon_toggle() and we can't remove surprise
    // flag until we process all that.
    if (FIGHTING(ch)
        && IS_NPC(FIGHTING(ch))
        && !MOB_FLAGGED(FIGHTING(ch), MOB_INANIMATE)
        && GET_MOBALERT(FIGHTING(ch)) == MALERT_CALM
        && success_test(GET_REA(ch), 4) > success_test(GET_REA(FIGHTING(ch)), 4)) {
      GET_INIT_ROLL(FIGHTING(ch)) = 0;
      act("You surprise $n!", TRUE, FIGHTING(ch), 0, ch, TO_VICT);
      AFF_FLAGS(FIGHTING(ch)).SetBit(AFF_SURPRISE);
    }
  }
  char rbuf[MAX_STRING_LENGTH];
  snprintf(rbuf, sizeof(rbuf),"Init: %2d %s", GET_INIT_ROLL(ch), GET_NAME(ch));
  act(rbuf,TRUE,ch,NULL,NULL,TO_ROLLS);
  if (AFF_FLAGGED(ch, AFF_ACID))
    AFF_FLAGS(ch).RemoveBit(AFF_ACID);
}

void decide_combat_pool(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct char_data *ch, *next_ch;

  for (ch = combat_list; ch; ch = next_ch) {
    next_ch = ch->next_fighting;
    if (ch->bioware)
      if (check_adrenaline(ch, 0))
        continue;

    if (IS_NPC(ch) && !IS_PROJECT(ch) && FIGHTING(ch)) {
      if (GET_INIT_ROLL(ch) == GET_INIT_ROLL(FIGHTING(ch)))
        GET_OFFENSE(ch) = GET_COMBAT(ch) >> 1;
      else if (GET_INIT_ROLL(ch) > GET_INIT_ROLL(FIGHTING(ch)))
        GET_OFFENSE(ch) = (int)(GET_COMBAT(ch) * .75);
      else
        GET_OFFENSE(ch) = (int)(GET_COMBAT(ch) / 4);
      GET_DEFENSE(ch) = GET_COMBAT(ch) - GET_OFFENSE(ch);
      if (GET_IMPACT(ch) > 6 || GET_BALLISTIC(ch) > 6) {
        GET_BODY(ch) = (int)(GET_DEFENSE(ch) * .75);
        GET_DEFENSE(ch) -= GET_BODY(ch);
      }
    }
  }
}

void roll_initiative(void)
{
  struct char_data *ch, *next_combat_list = NULL;

  for (ch = combat_list; ch; ch = next_combat_list) {
    next_combat_list = ch->next_fighting;
    roll_individual_initiative(ch);
    if (ch->in_room && ch->in_room->poltergeist[0]) {
      int dam = convert_damage(stage(-success_test(GET_QUI(ch), ch->in_room->poltergeist[1] - GET_IMPACT(ch)), LIGHT));
      if (dam > 0) {
        act("You are hit by flying objects!\r\n", FALSE, ch, 0, 0, TO_CHAR);
        act("$n is hit by flying objects!\r\n", TRUE, ch, 0, 0, TO_ROOM);
        damage(ch, ch, dam, TYPE_POLTERGEIST, MENTAL);
      }
    }
  }
  return;
}

/*
// Bugfix for weird edge case where next_combat_list could become invalidated (ex: quest target extraction after ch death)
if (next_combat_list != NULL && next_combat_list != combat_list) {
  bool next_combat_list_is_in_combat_list = FALSE;
  for (struct char_data *tmp_combat_list = combat_list; tmp_combat_list; tmp_combat_list = tmp_combat_list->next_fighting) {
    next_combat_list_is_in_combat_list = TRUE;
  }

  if (!next_combat_list_is_in_combat_list) {
    mudlog("CRASH PREVENTION: next_combat_list was not found in combat_list!", NULL, LOG_SYSLOG, TRUE);
    break;
  }
}

ch = next_combat_list;
*/

bool next_combat_list_is_valid(struct char_data *ncl) {
  // Beginning or end of loop means NCL is fine.
  if (next_combat_list == NULL || next_combat_list == combat_list)
    return TRUE;

  // Otherwise, ensure NCL is in CL. If it is, good-- return true (valid)
  // I don't even want to hear about the O(n^2) here. YOU try iterating over a list where any one of the elements can disappear mid-iteration!
  for (struct char_data *tmp_combat_list = combat_list; tmp_combat_list; tmp_combat_list = tmp_combat_list->next_fighting)
    if (tmp_combat_list == next_combat_list)
      return TRUE;

  // Otherwise, bail out of violence.
  mudlog("CRASH PREVENTION: next_combat_list was not found in combat_list!", NULL, LOG_SYSLOG, TRUE);
  return FALSE;
}

/* control the fights going on.  Called every 2 seconds from comm.c. */
// aka combat_loop
unsigned long violence_loop_counter = 0;
void perform_violence(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct char_data *ch = NULL;
  bool engulfed;
  extern struct index_data *mob_index;

  // No list? No work.
  if (!combat_list)
    return;

  // Increment our violence loop counter.
  violence_loop_counter++;

  if (GET_INIT_ROLL(combat_list) <= 0) {
    roll_initiative();
    order_list(TRUE);
  }

  // This while-loop replaces the combat list for-loop so we can do better edge case checking.
  ch = NULL;
  bool first_iteration = TRUE;
  while (ch || first_iteration) {
    // First iteration: Set ch to combat list. We don't check next_combat_list_is_valid() in this case.
    if (first_iteration)
      ch = combat_list;
    else {
      // Subsequent iterations: Check to see if n_c_l is valid. If it is, use it; if not, abort and warn.
      if (next_combat_list_is_valid(next_combat_list))
        ch = next_combat_list;
      else {
        mudlog("CRASH PREVENTION: Bailing out of combat loop due to invalid next_combat_list. Some people don't get their turns.\r\n", ch, LOG_SYSLOG, TRUE);
        break;
      }
    }

    if (!ch) {
      return;
    }

    // Clear the first-iteration flag.
    first_iteration = FALSE;

    next_combat_list = ch->next_fighting;

    // Clear the engulfed status.
    engulfed = FALSE;

    // Prevent people from being processed multiple times per loop.
    if (ch->last_violence_loop == violence_loop_counter) {
      mudlog("SYSERR: Encountered someone who already went this combat turn. Fix set_fighting().", ch, LOG_SYSLOG, TRUE);
      continue;
    } else {
      ch->last_violence_loop = violence_loop_counter;
    }

    // You're not in combat or not awake.
    if (!CH_IN_COMBAT(ch) || !AWAKE(ch)) {
      act("$n is no longer in combat- removing from combat list.", FALSE, ch, 0, 0, TO_ROLLS);
      stop_fighting(ch);
      continue;
    }

    if (!FIGHTING(ch) && !FIGHTING_VEH(ch)) {
      mudlog("SYSERR: Character is in the combat list, but isn't fighting anything!", ch, LOG_SYSLOG, TRUE);
      send_to_char("Your mind goes fuzzy for a moment, and you shake your head, then stand down.\r\n", ch);
      stop_fighting(ch);
      continue;
    }

    // You get no action if you're out of init.
    if (GET_INIT_ROLL(ch) <= 0 && !IS_JACKED_IN(ch)) {
      if (PRF_FLAGGED(ch, PRF_SEE_TIPS)) {
        send_to_char("^L(OOC: You're out of initiative! Waiting for combat round reset.)^n\r\n", ch);
      }
      continue;
    }

    // Knock down their init.
    GET_INIT_ROLL(ch) -= 10;

    // NPCs stand up if so desired. We just use mobact_change_firemode here since it's baked in.
    if (IS_NPC(ch)) {
      mobact_change_firemode(ch);
    }

    // On fire and panicking, or engulfed? Lose your action.
    if ((ch->points.fire[0] > 0 && success_test(GET_WIL(ch), 6, ch, "process_violence fire test") < 0) || engulfed)
    {
      send_to_char("^RThe flames engulfing you cause you to panic!^n\r\n", ch);
      act("$n panics and flails ineffectually.", TRUE, ch, 0, 0, TO_ROOM);
      continue;
    }

    // Skip people who are surprised, but only if they're in combat with someone who is attacking them.
    if (AFF_FLAGGED(ch, AFF_SURPRISE)) {
      if (FIGHTING(ch) && FIGHTING(FIGHTING(ch)) == ch) {
        act("Skipping $n's action: Surprised.", FALSE, ch, 0, 0, TO_ROLLS);
        continue;
      }
    }

    // Process spirit attacks.
    for (struct spirit_sustained *ssust = SPIRIT_SUST(ch); ssust; ssust = ssust->next) {
      if (ssust->type == ENGULF) {
        if (ssust->caster) {
          int dam;
          if (IS_SPIRIT(ch) || (IS_ANY_ELEMENTAL(ch) && GET_SPARE1(ch) == ELEM_WATER)) {
            dam = convert_damage(stage(-success_test(GET_BOD(ssust->target), GET_SPARE2(ch) + GET_EXTRA(ssust->target)), MODERATE));
            act("$n can contorts in pain as the water engulfs $m!", TRUE, ssust->target, 0, 0, TO_ROOM);
            send_to_char("The water crushes you and leaves you unable to breath!\r\n", ssust->target);
            if (dam > 0 && damage(ch, ssust->target, dam, TYPE_FUMES, MENTAL))
              break;
            GET_EXTRA(ssust->target)++;
          } else
            switch (GET_SPARE1(ch)) {
              case ELEM_FIRE:
                dam = convert_damage(stage(-success_test(GET_BOD(ssust->target), GET_SPARE2(ch) - GET_IMPACT(ssust->target) + (MOB_FLAGGED(ch, MOB_FLAMEAURA) ? 2 : 0)), MODERATE));
                act("$n can be heard screaming from inside $s prison of flames!", TRUE, ssust->target, 0, 0, TO_ROOM);
                send_to_char("The flames continue to burn you, causing horrendous pain!\r\n", ssust->target);
                if (dam > 0)
                  damage(ch, ssust->target, dam, TYPE_FUMES, PHYSICAL);
                break;
              case ELEM_EARTH:
                dam = convert_damage(stage(-success_test(GET_BOD(ssust->target), GET_SPARE2(ch) - GET_IMPACT(ssust->target)), SERIOUS));
                act("$n can be heard screaming from inside $s earthen prison.", TRUE, ssust->target, 0, 0, TO_ROOM);
                send_to_char("The earth around you compresses against you, causing you great pain.\r\n", ssust->target);
                if (dam > 0)
                  damage(ch, ssust->target, dam, TYPE_FUMES, PHYSICAL);
                break;
              case ELEM_AIR:
                dam = convert_damage(stage(-success_test(MAX(GET_WIL(ssust->target), GET_BOD(ssust->target)), GET_SPARE2(ch)), SERIOUS));
                act("$n chokes and coughs.", TRUE, ssust->target, 0, 0, TO_ROOM);
                send_to_char("Noxious fumes fill your throat and lungs, causing your vision to swim.\r\n", ssust->target);
                if (dam > 0)
                  damage(ch, ssust->target, dam, TYPE_FUMES, MENTAL);
                break;
            }
        } else {
          if (success_test(GET_STR(ch), GET_LEVEL(ssust->target)) - success_test(GET_LEVEL(ssust->target), GET_STR(ch)) > 0)
            stop_spirit_power(ssust->target, ENGULF);
          else {
            act("$n struggles against the spirit engulfing $m!", FALSE, ch, 0, 0, TO_ROOM);
            act("You struggle against $n's engulfment!", FALSE, ssust->target, 0, ch, TO_VICT);
          }
        }
        engulfed = TRUE;
        break;
      }
    }

    // Process banishment actions.
    if (IS_AFFECTED(ch, AFF_BANISH)
        && FIGHTING(ch)
        && (IS_ANY_ELEMENTAL(FIGHTING(ch))
            || IS_SPIRIT(FIGHTING(ch))))
    {
      struct char_data *spirit, *mage;
      if (IS_NPC(ch)) {
        spirit = ch;
        mage = FIGHTING(ch);
      } else {
        spirit = FIGHTING(ch);
        mage = ch;
      }
      int skill = GET_SKILL(mage, SKILL_CONJURING);
      for (int i = 0; i < NUM_WEARS; i++)
        if (GET_EQ(mage, i) && GET_OBJ_TYPE(GET_EQ(mage, i)) == ITEM_FOCUS && GET_OBJ_VAL(GET_EQ(mage, i), 0) == FOCI_SPIRIT
            && GET_OBJ_VAL(GET_EQ(mage, i), 2) == GET_IDNUM(mage) && GET_OBJ_VAL(GET_EQ(mage, i), 3) == GET_SPARE1(spirit)
            && GET_OBJ_VAL(GET_EQ(mage, i), 4)) {
          skill += GET_OBJ_VAL(GET_EQ(mage, i), 1);
          break;
        }
      if (GET_ACTIVE(spirit) == GET_IDNUM(mage))
        skill += GET_CHA(mage);
      int success = resisted_test(skill, GET_LEVEL(spirit), GET_LEVEL(spirit), GET_MAG(mage) / 100);
      if (success < 0) {
        GET_EXTRA(mage) = 0;
        GET_EXTRA(spirit) = 1;
        GET_TEMP_MAGIC_LOSS(mage) -= success;
        send_to_char(mage, "You lock minds with %s, but are beaten back by its force!\r\n", GET_NAME(spirit));
      } else if (success == 0) {
        send_to_char(mage, "You lock minds with %s, but fail to gain any ground.\r\n",
                     GET_NAME(spirit));
      } else {
        GET_EXTRA(mage) = 1;
        GET_EXTRA(spirit) = 0;
        send_to_char(mage, "You lock minds with %s and succeed in weakening it!\r\n", GET_NAME(spirit));
        GET_LEVEL(spirit) -= success;
      }
      affect_total(mage);
      affect_total(spirit);
      if (GET_LEVEL(spirit) < 1) {
        send_to_char(mage, "You drain %s off the last of its magical force, banishing it to the metaplanes!\r\n", GET_NAME(spirit));
        act("$N is banished to the metaplanes as $n drains the last of its magical force.", FALSE, mage, 0, spirit, TO_ROOM);
        stop_fighting(spirit);
        stop_fighting(mage);
        end_spirit_existance(spirit, TRUE);
        AFF_FLAGS(mage).RemoveBit(AFF_BANISH);
      } else if (GET_MAG(mage) < 1) {
        send_to_char(mage, "Your magic spent from your battle with %s, you fall unconscious.\r\n", GET_NAME(spirit));
        act("$n falls unconscious, drained by magical combat.", FALSE, mage, 0, 0, TO_ROOM);
        if (damage(spirit, mage, TYPE_DRAIN, DEADLY, MENTAL)) {
          continue;
        }
        stop_fighting(spirit);
        stop_fighting(mage);
        update_pos(mage);
        AFF_FLAGS(mage).RemoveBit(AFF_BANISH);
        AFF_FLAGS(spirit).RemoveBit(AFF_BANISH);
      }
      continue;
    }

    // Process mage actions.
    if (FIGHTING(ch)) {
      if (IS_NPC(ch)
          && !ch->desc
          && GET_SKILL(ch, SKILL_SORCERY) > 0
          && GET_MENTAL(ch) > 400
          && ch->in_room == FIGHTING(ch)->in_room
          && !AFF_FLAGGED(ch, AFF_SURPRISE)
          && success_test(1, 8 - GET_SKILL(ch, SKILL_SORCERY)))
      {
        // Only continue if we successfully cast.
        if (mob_magic(ch)) {
          continue;
        }
      }

      if (ch->squeue) {
        cast_spell(ch, ch->squeue->spell, ch->squeue->sub, ch->squeue->force, ch->squeue->arg);
        DELETE_ARRAY_IF_EXTANT(ch->squeue->arg);
        DELETE_AND_NULL(ch->squeue);
        continue;
      }
    }

    // Process melee charging.
    if (IS_AFFECTED(ch, AFF_APPROACH)) {
      /* Complete failure cases, with continue to prevent evaluation:
         - Not in same room (terminate charging)
      */
      // Ideally, we should have the NPC chase to the room their attacker is in, but given the command delays from flee/retreat this would make combat inescapable.
      if (ch->in_room != FIGHTING(ch)->in_room) {
        AFF_FLAGS(ch).RemoveBit(AFF_APPROACH);
        AFF_FLAGS(FIGHTING(ch)).RemoveBit(AFF_APPROACH);
        // stop_fighting(ch);
        continue;
      }

      /* Automatic success:
        - Opponent charging at you too (both stop charging; clash)
        - Opponent is emplaced (they can't run)
        - You both have melee weapons out (both stop charging; clash)
      */
      if (IS_AFFECTED(FIGHTING(ch), AFF_APPROACH)
          || MOB_FLAGGED(FIGHTING(ch), MOB_EMPLACED)
          || (item_should_be_treated_as_melee_weapon(GET_EQ(ch, WEAR_WIELD)) && item_should_be_treated_as_melee_weapon(GET_EQ(FIGHTING(ch), WEAR_WIELD))))
      {
        AFF_FLAGS(ch).RemoveBit(AFF_APPROACH);
        AFF_FLAGS(FIGHTING(ch)).RemoveBit(AFF_APPROACH);
      }

      // No need to charge if you're wielding a loaded gun. Your opponent still charges, if they're doing so.
      if (item_should_be_treated_as_ranged_weapon(GET_EQ(ch, WEAR_WIELD))) {
        AFF_FLAGS(ch).RemoveBit(AFF_APPROACH);
      }

      // Otherwise, process the charge.
      else {
        // This whole section is houseruled around the fact that we don't want to roll tons of opposed athletics checks.
        //  1) that would mean that every built mob would need ath, which is a pain to add to old mobs
        //  2) players would get shafted due to mobs having high stats
        // The current iteration instead sets a TN based on opponent's stats and doesn't include athletics in either side of the roll.

        // Take the better of the defender's QUI and REA.
        int defender_attribute = MAX(GET_QUI(FIGHTING(ch)), GET_REA(FIGHTING(ch)));
        int target = 0;
        // Set the dice pool to be the character's quickness.
        int quickness = GET_QUI(ch);
        // Extended foot anchors suck for running.

        bool footanchor = FALSE;
        bool defender_footanchor = FALSE;

        // Visibility penalty for defender, it's hard to avoid someone you can't see.
        if (!CAN_SEE(FIGHTING(ch), ch))
          target -= 4;
        // Visibility penalty for attacker, it's hard to gap-close with someone you can't see.
        if (!CAN_SEE(ch, FIGHTING(ch)))
          target += 4;

        // House rule: Satyrs get -1 TN in their favor for closing the distance due to their speed. Dwarves and related metatypes get +1 against them.
        switch (GET_RACE(ch)) {
          case RACE_SATYR:
            target--;
            break;
          case RACE_DWARF:
          case RACE_MENEHUNE:
          case RACE_GNOME:
            target++;
            break;
        }

        // Same house rule as above, but applied on your opponent.
        switch (GET_RACE(FIGHTING(ch))) {
          case RACE_SATYR:
            target++;
            break;
          case RACE_DWARF:
          case RACE_MENEHUNE:
          case RACE_GNOME:
            target--;
            break;
        }

        // Armor penalties.
        if (GET_TOTALBAL(ch) > GET_QUI(ch))
          quickness -= GET_TOTALBAL(ch) - GET_QUI(ch);
        if (GET_TOTALIMP(ch) > GET_QUI(ch))
          quickness -= GET_TOTALIMP(ch) - GET_QUI(ch);

        // Distance Strike (only works unarmed)
        if (GET_TRADITION(ch) == TRAD_ADEPT
            && GET_POWER(ch, ADEPT_DISTANCE_STRIKE)
            && !(GET_EQ(ch, WEAR_WIELD) || GET_EQ(ch, WEAR_HOLD)))
          target -= (int)GET_REAL_MAG(ch) / 150;

        // Hydraulic jack, foot anchor, Kid Stealth - charger
        for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content) {
          if (GET_CYBERWARE_TYPE(cyber) == CYB_HYDRAULICJACK)
            quickness += GET_CYBERWARE_RATING(cyber);
          else if (GET_CYBERWARE_TYPE(cyber) == CYB_FOOTANCHOR && !GET_CYBERWARE_IS_DISABLED(cyber))
            footanchor = TRUE;
          else if (GET_CYBERWARE_TYPE(cyber) == CYB_LEGS && IS_SET(GET_CYBERWARE_FLAGS(cyber), LEGS_MOD_KIDSTEALTH))
            target--;
        }

        // Hydraulic jack, foot anchor, Kid Stealth - defender
        for (struct obj_data *cyber = FIGHTING(ch)->cyberware; cyber; cyber = cyber->next_content) {
          if (GET_CYBERWARE_TYPE(cyber) == CYB_HYDRAULICJACK)
            defender_attribute += GET_CYBERWARE_RATING(cyber);
          else if (GET_CYBERWARE_TYPE(cyber) == CYB_FOOTANCHOR && !GET_CYBERWARE_IS_DISABLED(cyber))
            defender_footanchor = TRUE;
          else if (GET_CYBERWARE_TYPE(cyber) == CYB_LEGS && IS_SET(GET_CYBERWARE_FLAGS(cyber), LEGS_MOD_KIDSTEALTH))
            target++;
        }

        // Movement modifications via spells - charger
        for (struct spirit_sustained *ssust = SPIRIT_SUST(ch); ssust; ssust = ssust->next)
          if (ssust->type == MOVEMENTUP && ssust->caster == FALSE && GET_LEVEL(ssust->target))
            quickness *= GET_LEVEL(ssust->target);
          else if (ssust->type == MOVEMENTDOWN && ssust->caster == FALSE && GET_LEVEL(ssust->target))
            quickness /= GET_LEVEL(ssust->target);

        // Movement modifications via spells - defender
        for (struct spirit_sustained *ssust = SPIRIT_SUST(FIGHTING(ch)); ssust; ssust = ssust->next)
          if (ssust->type == MOVEMENTUP && ssust->caster == FALSE && GET_LEVEL(ssust->target))
            defender_attribute *= GET_LEVEL(ssust->target);
          else if (ssust->type == MOVEMENTDOWN && ssust->caster == FALSE && GET_LEVEL(ssust->target))
            defender_attribute /= GET_LEVEL(ssust->target);

        // Movement reset: Can't move if binding.
        if (AFF_FLAGGED(ch, AFF_BINDING))
          quickness = 0;
        if (AFF_FLAGGED(FIGHTING(ch), AFF_BINDING))
          defender_attribute = 0;

        // Penalty from footanchor.
        if (footanchor)
          quickness /= 2;
        if (defender_footanchor)
          defender_attribute /= 2;

        // Penalty from too-tall.
#ifdef USE_SLOUCH_RULES
        if (ch->in_room && ROOM_FLAGGED(ch->in_room, ROOM_INDOORS)) {
          if (GET_HEIGHT(ch) > ch->in_room->z*100) {
            if (GET_HEIGHT(ch) > ch->in_room->z * 200) {
              send_to_char(ch, "You're bent over double in here, there's no way you'll close the distance like this!\r\n");
              act("$n looks like $e would really like to hit $N, but $e can't get close enough.", FALSE, ch, 0, FIGHTING(ch), TO_ROOM);
              continue;
            }
            else quickness /= 2;
          }
        }
#endif

        // Set the target from the defender's attribute.
        target += defender_attribute;
        // Lock the target to a range. Nobody enjoys rolling TN 14 to close with high-level invis mages.
        target = MIN(MINIMUM_TN_FOR_CLOSING_CHECK, MAX(target, MAXIMUM_TN_FOR_CLOSING_CHECK));

        // Strike.
        if (quickness > 0 && success_test(quickness, target) > 1) {
          send_to_char(ch, "You close the distance and strike!\r\n");
          act("$n closes the distance and strikes.", TRUE, ch, 0, 0, TO_ROOM);
          AFF_FLAGS(ch).RemoveBit(AFF_APPROACH);
          AFF_FLAGS(FIGHTING(ch)).RemoveBit(AFF_APPROACH);
        } else {
          send_to_char(ch, "You attempt to close the distance!\r\n");
          act("$n charges towards $N, but $N manages to keep some distance.", TRUE, ch, 0, FIGHTING(ch), TO_NOTVICT);
          act("$n charges towards you, but you manage to keep some distance.", TRUE, ch, 0, FIGHTING(ch), TO_VICT);
          // TODO: Is it really supposed to cost an extra init pass?
          // GET_INIT_ROLL(ch) -= 10;

          continue;
        }
      }
    }

    // Manning weaponry.
    if (AFF_FLAGGED(ch, AFF_MANNING) || PLR_FLAGGED(ch, PLR_REMOTE) || AFF_FLAGGED(ch, AFF_RIG)) {
      struct veh_data *veh = NULL;
      RIG_VEH(ch, veh);

      // Require that the target is reachable.
      if ((veh && veh->in_room)
          && ((FIGHTING(ch) && veh->in_room != FIGHTING(ch)->in_room)
               || (FIGHTING_VEH(ch) && veh->in_room != FIGHTING_VEH(ch)->in_room))) {
        if (!IS_JACKED_IN(ch))
          send_to_char("Your target has left your line of sight.\r\n", ch);
        stop_fighting(ch);
        continue;
      }

      // Fire, then check for death.
      if (mount_fire(ch))
        continue;
    }

    // Attacking a vehicle. Stopped here.
    else if (FIGHTING_VEH(ch)) {
      if (ch->in_room != FIGHTING_VEH(ch)->in_room) {
        stop_fighting(ch);
      } else
        if (vcombat(ch, FIGHTING_VEH(ch)))
          continue;
    } else if (FIGHTING(ch)) {
      bool target_died = hit(ch,
                             FIGHTING(ch),
                             GET_EQ(ch, WEAR_WIELD) ? GET_EQ(ch, WEAR_WIELD) : GET_EQ(ch, WEAR_HOLD),
                             GET_EQ(FIGHTING(ch), WEAR_WIELD) ? GET_EQ(FIGHTING(ch), WEAR_WIELD) : GET_EQ(FIGHTING(ch), WEAR_HOLD),
                             NULL);

      if (target_died)
        continue;

      // TODO: Two-weapon fighting for melee weapons (see CC p96 for off-hand weapons).
      if (GET_EQ(ch, WEAR_WIELD)
          && GET_EQ(ch, WEAR_HOLD)
          && FIGHTING(ch)
          && IS_GUN(GET_WEAPON_ATTACK_TYPE(GET_EQ(ch, WEAR_HOLD)))
          && GET_WEAPON_ATTACK_TYPE(GET_EQ(ch, WEAR_HOLD)) != WEAP_TASER)
      {
        target_died = hit(ch,
                          FIGHTING(ch),
                          GET_EQ(ch, WEAR_HOLD),
                          GET_EQ(FIGHTING(ch), WEAR_WIELD) ? GET_EQ(FIGHTING(ch), WEAR_WIELD) : GET_EQ(FIGHTING(ch), WEAR_HOLD),
                          NULL);

        if (target_died)
          continue;
      }
    } else {
      mudlog("SYSERR: Character is in the combat list, but isn't fighting anything! (Second block)", ch, LOG_SYSLOG, TRUE);
      send_to_char("Your mind goes fuzzy for a moment, and you shake your head, then stand down.\r\n", ch);
      stop_fighting(ch);
      continue;
    }

    if (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_SPEC) &&
        mob_index[GET_MOB_RNUM(ch)].func != NULL) {
      char empty_argument = '\0';
      (mob_index[GET_MOB_RNUM(ch)].func) (ch, ch, 0, &empty_argument);
    }
  }

  mudlog("Combat list complete.", NULL, LOG_SYSLOG, TRUE);
}

void order_list(bool first, ...)
{
  struct char_data *one, *two = combat_list, *next, *previous = NULL, *temp;

  if (combat_list == NULL)
    return;

  // Some kind of bastardized bubble sort. Doesn't work unless we...
  bool made_changes;
  do {
    made_changes = FALSE;
    previous = NULL;
    for (two = combat_list;
         two && two->next_fighting;
         two = two->next_fighting)
    {
      if (GET_INIT_ROLL(two->next_fighting) > GET_INIT_ROLL(two)) {
        if (previous)
          previous->next_fighting = two->next_fighting;
        else
          combat_list = two->next_fighting;

        temp = two->next_fighting->next_fighting;
        two->next_fighting->next_fighting = two;
        two->next_fighting = temp;
        made_changes = TRUE;
      }

      previous = two;
    }
  } while (made_changes);

  if (first)
    for (one = combat_list; one; one = next) {
      next = one->next_fighting;
      two = one;
      // TODO BUG: Since reroll happens when the first person on the list hits zero, and this reorders the list, dist strike adepts fuck up the init passes.
      if (GET_TRADITION(one) == TRAD_ADEPT && GET_POWER(one, ADEPT_QUICK_STRIKE) && GET_PHYSICAL(one) >= 1000 && GET_MENTAL(one) >= 1000) {
        REMOVE_FROM_LIST(one, combat_list, next_fighting);
        one->next_fighting = combat_list;
        combat_list = one;
      }
    }
}

void order_list(struct char_data *start)
{
  struct char_data *one, *two, *next, *previous = NULL, *temp;

  for (one = start; one; previous = NULL, one = next)
  {
    next = one->next_fighting;
    for (two = start; two && two->next_fighting; previous = two,
         two = two->next_fighting) {
      if (GET_INIT_ROLL(two->next_fighting) > GET_INIT_ROLL(two)) {
        if (previous)
          previous->next_fighting = two->next_fighting;
        else
          start = two->next_fighting;
        temp = two->next_fighting->next_fighting;
        two->next_fighting->next_fighting = two;
        two->next_fighting = temp;
      }
    }
  }
}

void order_list(struct matrix_icon *start)
{
  struct matrix_icon *one, *two, *next, *previous = NULL, *temp;

  for (one = start; one; previous = NULL, one = next)
  {
    next = one->next_fighting;
    for (two = start; two && two->next_fighting; previous = two,
         two = two->next_fighting) {
      if (two->next_fighting->initiative > two->initiative) {
        if (previous)
          previous->next_fighting = two->next_fighting;
        else
          start = two->next_fighting;
        temp = two->next_fighting->next_fighting;
        two->next_fighting->next_fighting = two;
        two->next_fighting = temp;
      }
    }
  }
  matrix[start->in_host].fighting = start;
}

void chkdmg(struct veh_data * veh)
{
  if (veh->damage <= VEH_DAM_THRESHOLD_LIGHT) {
    send_to_veh("A scratch appears on the paintwork.\r\n", veh, NULL, TRUE);
  } else if (veh->damage <= VEH_DAM_THRESHOLD_MODERATE) {
    send_to_veh("You see some dings and scratches appear on the paintwork.\r\n", veh, NULL, TRUE);
  } else if (veh->damage <= VEH_DAM_THRESHOLD_SEVERE) {
    send_to_veh("The windshield shatters and the bumper falls off.\r\n", veh, NULL, TRUE);
  } else if (veh->damage < VEH_DAM_THRESHOLD_DESTROYED) {
    send_to_veh("The engine starts spewing smoke and flames.\r\n", veh, NULL, TRUE);
  } else {
    int damage_rating, damage_tn;

    // Remove any vehicle brains, we don't want them thrown into the street.
    remove_vehicle_brain(veh);

    // This only matters for NPC vehicles, but there's no harm in setting it on player vehicles.
    GET_VEH_DESTRUCTION_TIMER(veh) = 0;

    if (veh->cspeed >= SPEED_IDLE) {
      if (veh->people) {
        if (veh->in_room && IS_WATER(veh->in_room)) {
          send_to_veh("You are hurled into the water as your ride is wrecked!\r\n", veh, NULL, FALSE);
          snprintf(buf, sizeof(buf), "%s bites deep into the water and flips, its occupants hurled into the water!\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
        } else {
          send_to_veh("You are hurled into the street as your ride is wrecked!\r\n", veh, NULL, FALSE);
          snprintf(buf, sizeof(buf), "%s careens off the road, its occupants hurled into the street!\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
        }
        act(buf, FALSE, veh->people, NULL, NULL, TO_VEH_ROOM);
      } else {
        if (veh->in_room && IS_WATER(veh->in_room)) {
          snprintf(buf, sizeof(buf), "%s takes on too much water and is swamped!\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
        } else {
          snprintf(buf, sizeof(buf), "%s careens off the road!\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
        }
        send_to_room(buf, get_veh_in_room(veh));
      }

      damage_rating = SERIOUS;
      damage_tn = 8;
    } else {
      if (veh->in_room && IS_WATER(veh->in_room)) {
        send_to_veh("You dive into the water as your ride is wrecked!\r\n", veh, NULL, FALSE);
      } else {
        send_to_veh("You scramble into the street as your ride is wrecked!\r\n", veh, NULL, FALSE);
      }

      if (veh->people) {
        if (veh->in_room && IS_WATER(veh->in_room)) {
          snprintf(buf, sizeof(buf), "%s's occupants dive for safety as it is wrecked!\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
        } else {
          snprintf(buf, sizeof(buf), "%s's occupants scramble to safety as it is wrecked!\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
        }
      } else {
        snprintf(buf, sizeof(buf), "Smoke belches from %s as it is wrecked!\r\n", GET_VEH_NAME(veh));
      }
      send_to_room(buf, veh->in_room);

      damage_rating = MODERATE;
      damage_tn = 4;
    }

    // Write purgelogs for player vehicle kills.
    if (veh->owner) {
      mudlog("Writing player vehicle contents to purgelog-- destroyed via standard damage.", NULL, LOG_WRECKLOG, TRUE);
      mudlog("Writing player vehicle contents to purgelog-- destroyed via standard damage.", NULL, LOG_PURGELOG, TRUE);
      purgelog(veh);
    }

    if (veh->rigger) {
      send_to_char("Your mind is blasted with pain as your vehicle is wrecked.\r\n", veh->rigger);
      if (!damage(veh->rigger, veh->rigger, convert_damage(stage(-success_test(GET_WIL(veh->rigger), 6), SERIOUS)), TYPE_CRASH, MENTAL)) {
        // If they got knocked out, they've already broken off from rigging.
        if (veh->rigger) {
          veh->rigger->char_specials.rigging = NULL;
          PLR_FLAGS(veh->rigger).RemoveBit(PLR_REMOTE);
        }
      }
      veh->rigger = NULL;
    }

    // Turn down various settings on the vehicle.
    stop_chase(veh);
    veh->cspeed = SPEED_OFF;
    veh->dest = 0;

    if (veh->towing) {
      if (veh->in_room) {
        veh_to_room(veh->towing, veh->in_room);
      } else if (veh->in_veh) {
        veh_to_veh(veh->towing, veh->in_veh);
      } else {
        mudlog("SYSERR: Vehicle with a towed veh was neither in room nor veh! Sending towed vehicle to Dante's.", NULL, LOG_SYSLOG, TRUE);
        veh_to_room(veh->towing, &world[real_room(RM_DANTES_GARAGE)]);
      }
      veh->towing = NULL;
    }

    // Dump out the people in the vehicle and set their relevant values.
    for (struct char_data *i = veh->people, *next; i; i = next) {
      next = i->next_in_veh;

      stop_manning_weapon_mounts(i, FALSE);
      STOP_WORKING(i);

      char_from_room(i);
      if (veh->in_room) {
        char_to_room(i, veh->in_room);
      } else if (veh->in_veh) {
        char_to_veh(veh->in_veh, i);
      } else {
        mudlog("SYSERR: Destroyed vehicle had no in_room or in_veh! Disgorging occupant to Dante's.", i, LOG_SYSLOG, TRUE);
        char_to_room(i, &world[real_room(RM_DANTES_GARAGE)]);
      }

      // TODO: What about the other flags for people who are sitting in the back working on something?
      AFF_FLAGS(i).RemoveBits(AFF_PILOT, AFF_RIG, ENDBIT);

      GET_POS(i) = POS_STANDING;

      // Deal damage.
      damage_rating = convert_damage(stage(-success_test(GET_BOD(i), damage_tn), damage_rating));
      if (damage(i, i, damage_rating, TYPE_CRASH, PHYSICAL))
        continue;
    }

    // Dump out and destroy objects.
    for (struct obj_data *obj = veh->contents, *nextobj; obj; obj = nextobj) {
      nextobj = obj->next_content;
      switch(number(0, 2)) {
          /* case 0: the item stays in the vehicle */
        case 0:
          break;
        case 1:
          obj_from_room(obj);
          if (veh->in_room) {
            obj_to_room(obj, veh->in_room);
          } else if (veh->in_veh) {
            obj_to_veh(obj, veh->in_veh);
          } else {
            mudlog("SYSERR: Destroyed veh had no in_room or in_veh! Disgorging object to Dante's.", NULL, LOG_SYSLOG, TRUE);
            obj_to_room(obj, &world[real_room(RM_DANTES_GARAGE)]);
          }
          break;
        case 2:
        {
          char purgebuf[500];
          snprintf(purgebuf, sizeof(purgebuf), "Destroying vehicle-held %s (%ld): Wreck loss", GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj));
          mudlog(purgebuf, NULL, LOG_WRECKLOG, TRUE);
        }
          extract_obj(obj);
          break;
      }
    }
  }
  return;
}

bool vram(struct veh_data * veh, struct char_data * ch, struct veh_data * tveh)
{
  int power, damage_total = 0, veh_dam = 0;
  int veh_resist = 0, ch_resist = 0, modbod = 0;

  // Alarm all NPCs inside the ramming vehicle.
  for (struct char_data *npc = veh->people; npc; npc = npc->next_in_veh) {
    if (IS_NPC(npc)) {
      GET_MOBALERT(npc) = MALERT_ALARM;
      GET_MOBALERTTIME(npc) = 30;
    }
  }

  if (ch && IS_NPC(ch)) {
    GET_MOBALERT(ch) = MALERT_ALARM;
    GET_MOBALERTTIME(ch) = 30;
  }

  // Alarm all NPCs inside the target vehicle.
  if (tveh) {
    for (struct char_data *npc = tveh->people; npc; npc = npc->next_in_veh) {
      if (IS_NPC(npc)) {
        GET_MOBALERT(npc) = MALERT_ALARM;
        GET_MOBALERTTIME(npc) = 30;
      }
    }
  }

  if (ch)
  {
    power = (int)(get_speed(veh) / 10);

    if (GET_BOD(ch) < 8)
      modbod = 1;
    else
      modbod = 2;

    if (modbod > (veh->body * 2)) {
      damage_total = LIGHT;
      veh_dam = DEADLY;
    } else if (modbod > veh->body) {
      damage_total = MODERATE;
      veh_dam = SERIOUS;
    } else if (modbod == veh->body) {
      damage_total = SERIOUS;
      veh_dam = MODERATE;
    } else if (modbod < veh->body) {
      damage_total = DEADLY;
      veh_dam = LIGHT;
    }

    ch_resist = 0 - success_test(GET_BOD(ch) + GET_BODY(ch), power);
    int staged_damage = stage(ch_resist, damage_total);
    damage_total = convert_damage(staged_damage);

    veh_resist = success_test(veh->body, power - (veh->body * 1 /* TODO: Replace this 1 with the number of successes rolled on the ram test. */));
    veh_dam -= veh_resist;

    bool will_damage_vehicle = FALSE;

    if (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_NORAM)) {
      damage_total = -1;
      snprintf(buf, sizeof(buf), "You can't seem to get close enough to run %s down!\r\n", decapitalize_a_an(GET_NAME(ch)));
      snprintf(buf1, sizeof(buf1), "%s can't seem to get close enough to $n to run $m down!", CAP(GET_VEH_NAME_NOFORMAT(veh)));
      snprintf(buf2, sizeof(buf2), "%s can't even get close to you!", CAP(GET_VEH_NAME_NOFORMAT(veh)));
      send_to_driver(buf, veh);
    } else if (damage_total < LIGHT) {
      snprintf(buf, sizeof(buf), "You ram into %s, but %s armor holds!\r\n", decapitalize_a_an(GET_NAME(ch)), HSHR(ch));
      snprintf(buf1, sizeof(buf1), "%s rams into $n, but $s armor holds!", CAP(GET_VEH_NAME_NOFORMAT(veh)));
      snprintf(buf2, sizeof(buf2), "%s rams into you, but your armor holds!", CAP(GET_VEH_NAME_NOFORMAT(veh)));
      send_to_driver(buf, veh);
      will_damage_vehicle = TRUE;
    } else if (veh_dam > 0) {
      send_to_veh("THUMP!\r\n", veh, NULL, TRUE);
      snprintf(buf, sizeof(buf), "You run %s down!\r\n", decapitalize_a_an(GET_NAME(ch)));
      snprintf(buf1, sizeof(buf1), "%s runs $n down!", CAP(GET_VEH_NAME_NOFORMAT(veh)));
      snprintf(buf2, sizeof(buf2), "%s runs you down!", CAP(GET_VEH_NAME_NOFORMAT(veh)));
      send_to_driver(buf, veh);
      will_damage_vehicle = TRUE;
    } else {
      send_to_veh("THUTHUMP!\r\n", veh, NULL, TRUE);
      snprintf(buf, sizeof(buf), "You roll right over %s!\r\n", decapitalize_a_an(GET_NAME(ch)));
      snprintf(buf1, sizeof(buf1), "%s rolls right over $n!", CAP(GET_VEH_NAME_NOFORMAT(veh)));
      snprintf(buf2, sizeof(buf2), "%s runs right over you!", CAP(GET_VEH_NAME_NOFORMAT(veh)));
      send_to_driver(buf, veh);
    }
    act(buf1, FALSE, ch, 0, 0, TO_ROOM);
    act(buf2, FALSE, ch, 0, 0, TO_CHAR);
    bool victim_died = damage(get_driver(veh), ch, damage_total, TYPE_RAM, PHYSICAL);

    // Deal damage to the rammer's vehicle if they incurred any.
    if (will_damage_vehicle && veh_dam > 0) {
      veh->damage += veh_dam;
      chkdmg(veh);
    }
    return victim_died;
  } else
  {
    power = veh->cspeed - tveh->cspeed;
    if (power < 0)
      power = -power;
    power = (int)(ceil((float)power / 10));

    if (tveh->body > (veh->body * 2)) {
      damage_total = LIGHT;
      veh_dam = DEADLY;
    } else if (tveh->body > veh->body) {
      damage_total = MODERATE;
      veh_dam = SERIOUS;
    } else if (tveh->body == veh->body) {
      damage_total = SERIOUS;
      veh_dam = MODERATE;
    } else if ((tveh->body * 2) < veh->body) {
      damage_total = DEADLY;
      veh_dam = LIGHT;
    } else if (tveh->body < veh->body) {
      damage_total = SERIOUS;
      veh_dam = MODERATE;
    }

    ch_resist = 0 - success_test(tveh->body, power);
    int staged_damage = stage(ch_resist, damage_total);
    damage_total = convert_damage(staged_damage);
    tveh->damage += damage_total;

    snprintf(buf, sizeof(buf), "%s rams into you!\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
    send_to_veh(buf, tveh, NULL, TRUE);

    if (tveh->cspeed > SPEED_IDLE)
      switch (damage_total) {
        case MODERATE:
        case LIGHT:
          tveh->cspeed = SPEED_CRUISING;
          send_to_veh("You lose some speed.\r\n", tveh, NULL, TRUE);
          break;
        case SERIOUS:
          tveh->cspeed = SPEED_CRUISING;
          if (tveh->rigger)
            crash_test(tveh->rigger);
          else
            for (struct char_data *pilot = tveh->people; pilot; pilot = pilot->next_in_veh)
              if (AFF_FLAGGED(pilot, AFF_PILOT))
                crash_test(pilot);
          break;
      }
    chkdmg(tveh);

    veh_resist = 0 - success_test(veh->body, power - (veh->body * 1 /* TODO: Replace this 1 with the number of successes rolled on the ram test. */));
    staged_damage = stage(veh_resist, veh_dam);
    damage_total = convert_damage(staged_damage);
    veh->damage += damage_total;

    snprintf(buf, sizeof(buf), "You ram a %s!\r\n", GET_VEH_NAME(tveh));
    snprintf(buf1, sizeof(buf1), "%s rams straight into your ride!\r\n", CAP(GET_VEH_NAME_NOFORMAT(tveh)));
    strcpy(buf3, GET_VEH_NAME(veh));
    snprintf(buf2, sizeof(buf2), "%s rams straight into %s!\r\n", buf3, CAP(GET_VEH_NAME_NOFORMAT(tveh)));
    send_to_veh(buf, veh, NULL, TRUE);
    send_to_veh(buf1, tveh, NULL, TRUE);
    send_to_room(buf2, veh->in_room);

    if (veh->cspeed > SPEED_IDLE)
      switch (damage_total) {
        case MODERATE:
        case LIGHT:
          veh->cspeed = SPEED_CRUISING;
          send_to_veh("You lose some speed.\r\n", tveh, NULL, TRUE);
          break;
        case SERIOUS:
          veh->cspeed = SPEED_CRUISING;
          if (veh->rigger)
            crash_test(veh->rigger);
          else
            for (struct char_data *pilot = veh->people; pilot; pilot = pilot->next_in_veh)
              if (AFF_FLAGGED(pilot, AFF_PILOT))
                crash_test(pilot);
          break;
      }
    chkdmg(veh);
  }
  return FALSE;
}

bool vcombat(struct char_data * ch, struct veh_data * veh)
{
  char ammo_type[20];
  static struct obj_data *wielded = NULL;
  static int base_target, power, damage_total;

  int attack_success = 0, attack_resist=0, skill_total = 1;
  int recoil=0, burst=0, recoil_comp=0, newskill, modtarget = 0;

  if (veh->damage >= VEH_DAM_THRESHOLD_DESTROYED) {
    stop_fighting(ch);
    return FALSE;
  }

  // Manning a mount, let's use that.
  if (ch->in_veh && AFF_FLAGGED(ch, AFF_MANNING)) {
    struct obj_data *mount = get_mount_manned_by_ch(ch);
    if (!mount) {
      // Flagged mount, with no mount, inside a vehicle, performing vcombat... idk, hitting a car inside a car?
      wielded = GET_EQ(ch, WEAR_WIELD);
    } else {
      wielded = get_mount_weapon(mount);
      if (!wielded) {
        // uhhhhhhhhhhhhhhhhhhhh. manning a valid turret with no weapon and attacking a vehicle... idk how we got here.
        mudlog("SYSERR: Manning a mount with no weapon in vcombat!", ch, LOG_SYSLOG, TRUE);
        wielded = GET_EQ(ch, WEAR_WIELD);
      }
    }
  } else {
    wielded = GET_EQ(ch, WEAR_WIELD);
  }

  if (get_speed(veh) > 10 && !AFF_FLAGGED(ch, AFF_COUNTER_ATT) && ((!wielded || !IS_GUN(GET_OBJ_VAL(wielded, 3)))))
  {
    return vram(veh, ch, NULL);
  }

  if (wielded) {
    // Ensure it has ammo.
    if (!has_ammo(ch, wielded)) {
      return FALSE;
    }

    // Set the attack description.
    switch(GET_WEAPON_ATTACK_TYPE(wielded)) {
      case WEAP_SHOTGUN:
        snprintf(ammo_type, sizeof(ammo_type), "horde of pellets");
        break;
      case WEAP_LMG:
      case WEAP_MMG:
      case WEAP_HMG:
        snprintf(ammo_type, sizeof(ammo_type), "stream of bullets");
        break;
      case WEAP_CANNON:
        snprintf(ammo_type, sizeof(ammo_type), "shell");
        break;
      case WEAP_MISS_LAUNCHER:
        snprintf(ammo_type, sizeof(ammo_type), "rocket");
        break;
      case WEAP_GREN_LAUNCHER:
        snprintf(ammo_type, sizeof(ammo_type), "grenade");
        break;
      default:
        snprintf(ammo_type, sizeof(ammo_type), "bullet");
        break;
    }

    // Deduct burstfire ammo. Note that one has already been deducted in has_ammo.
    if (WEAPON_IS_BF(wielded) && wielded->contains && GET_OBJ_TYPE(wielded->contains) == ITEM_GUN_MAGAZINE) {
      if (GET_MAGAZINE_AMMO_COUNT(wielded->contains) >= 2) {
        burst = 3;
        GET_MAGAZINE_AMMO_COUNT(wielded->contains) -= 2;
      } else if (GET_MAGAZINE_AMMO_COUNT(wielded->contains) == 1) {
        burst = 2;
        GET_MAGAZINE_AMMO_COUNT(wielded->contains)--;
      } else {
        burst = 0;
      }
    }

    if (IS_GUN(GET_WEAPON_ATTACK_TYPE(wielded)))
      power = GET_WEAPON_POWER(wielded) + burst;
    else
      power = GET_STR(ch) + GET_WEAPON_STR_BONUS(wielded);
    damage_total = GET_WEAPON_DAMAGE_CODE(wielded);
  } else
  {
    power = GET_STR(ch);
    strcpy(ammo_type, "blow");
    /*
    for (obj = ch->cyberware;
         obj && !damage_total;
         obj = obj->next_content)
      switch (GET_OBJ_VAL(obj, 2)) {
        // TODO: Figure out what these magic numbers are and comment. Until then, can't fix the implicit fallthrough at case 29.
        // FOLLOW-UP: cyberware value 2 is its grade (alpha/beta/etc). Whatever this code was, it's non-functional.
        case 19:
          damage_total = LIGHT;
          break;
        case 29:
          power += GET_OBJ_VAL(obj, 0);
        case 21:
          damage_total = MODERATE;
          break;
      }
    */
    if (!damage_total)
      damage_total = MODERATE;
  }

  power = (int)(power / 2);
  damage_total--;
  if (power <= veh->armor || !damage_total)
  {
    snprintf(buf, sizeof(buf), "$n's %s ricochets off of %s.", ammo_type, GET_VEH_NAME(veh));
    snprintf(buf2, sizeof(buf2), "Your attack ricochets off of %s.", GET_VEH_NAME(veh));
    act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
    act(buf2, FALSE, ch, NULL, NULL, TO_CHAR);
    snprintf(buf, sizeof(buf), "A %s ricochets off of your ride.\r\n", ammo_type);
    send_to_veh(buf, veh, 0, FALSE);
    return FALSE;
  } else
    power -= veh->armor;

  if (wielded)
    recoil_comp = check_recoil(ch, wielded);
  recoil = MAX(0, burst - recoil_comp);
  if (AFF_FLAGGED(ch, AFF_RIG) || AFF_FLAGGED(ch, AFF_MANNING))
    recoil = MAX(0, recoil - 2);

  if (get_speed(veh) > 10) {
    if (get_speed(veh) < 60)
      modtarget++;
    else if (get_speed(veh) < 120)
      modtarget += 2;
    else if (get_speed(veh) < 200)
      modtarget += 2;
    else if (get_speed(veh) >= 200)
      modtarget += 3;
  }
  if (wielded)
    modtarget -= check_smartlink(ch, wielded);
  if (wielded && IS_OBJ_STAT(wielded, ITEM_EXTRA_SNIPER) && ch->in_room == veh->in_room)
    modtarget += SAME_ROOM_SNIPER_RIFLE_PENALTY;

  if (GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD))
    modtarget++;

  if (AFF_FLAGGED(ch, AFF_RIG) || AFF_FLAGGED(ch, AFF_MANNING) || PLR_FLAGGED(ch, PLR_REMOTE))
  {
    skill_total = get_skill(ch, SKILL_GUNNERY, base_target);
  } else if (wielded && GET_SKILL(ch, GET_OBJ_VAL(wielded, 4)) < 1)
  {
    newskill = return_general(GET_OBJ_VAL(wielded, 4));
    skill_total = get_skill(ch, newskill, base_target);
  } else if (!wielded)
  {
    if (GET_SKILL(ch, SKILL_UNARMED_COMBAT) < 1) {
      newskill = SKILL_UNARMED_COMBAT;
      skill_total = get_skill(ch, newskill, base_target);
    } else
      skill_total = get_skill(ch, SKILL_UNARMED_COMBAT, base_target);
  } else
    skill_total = get_skill(ch, GET_OBJ_VAL(wielded, 4), base_target);

  base_target = 4 + modtarget + recoil + modify_target(ch);

  attack_success = success_test(skill_total, base_target);
  if (attack_success < 1)
  {
    if (wielded) {
      snprintf(buf, sizeof(buf), "$n fires his $o at %s, but misses.", GET_VEH_NAME(veh));
      snprintf(buf1, sizeof(buf1), "You fire your $o at %s, but miss.", GET_VEH_NAME(veh));
      snprintf(buf2, sizeof(buf2), "%s's %s misses you completely.\r\n", CAP(GET_NAME(ch)), ammo_type);
    } else {
      snprintf(buf, sizeof(buf), "$n swings at %s, but misses.", GET_VEH_NAME(veh));
      snprintf(buf1, sizeof(buf1), "You swing at %s, but miss.", GET_VEH_NAME(veh));
      snprintf(buf2, sizeof(buf2), "%s's %s misses you completely.\r\n", CAP(GET_NAME(ch)), ammo_type);
    }
    act(buf, FALSE, ch, wielded, 0, TO_NOTVICT);
    act(buf1, FALSE, ch, wielded, 0, TO_CHAR);
    send_to_veh(buf2, veh, 0, TRUE);
    weapon_scatter(ch, ch, wielded);
    return FALSE;
  }
  attack_resist = success_test(veh->body, power + get_vehicle_modifier(veh));
  attack_success -= attack_resist;

  int staged_damage = stage(attack_success, damage_total);
  damage_total = convert_damage(staged_damage);

  if (damage_total < LIGHT)
  {
    snprintf(buf, sizeof(buf), "$n's %s ricochets off of %s.", ammo_type, GET_VEH_NAME(veh));
    snprintf(buf1, sizeof(buf1), "Your attack ricochets off of %s.", GET_VEH_NAME(veh));
    snprintf(buf2, sizeof(buf2), "A %s ricochets off of your ride.\r\n", ammo_type);
  } else if (damage_total == LIGHT)
  {
    snprintf(buf, sizeof(buf), "$n's %s causes extensive damage to %s paintwork.", ammo_type, GET_VEH_NAME(veh));
    snprintf(buf1, sizeof(buf1), "Your attack causes extensive damage to %s paintwork.", GET_VEH_NAME(veh));
    snprintf(buf2, sizeof(buf2), "A %s scratches your paintjob.\r\n", ammo_type);
  } else if (damage_total == MODERATE)
  {
    snprintf(buf, sizeof(buf), "$n's %s leaves %s riddled with holes.", ammo_type, GET_VEH_NAME(veh));
    snprintf(buf1, sizeof(buf1), "Your attack leave %s riddled with holes.", GET_VEH_NAME(veh));
    snprintf(buf2, sizeof(buf2), "A %s leaves your ride full of holes.\r\n", ammo_type);
  } else if (damage_total == SERIOUS)
  {
    snprintf(buf, sizeof(buf), "$n's %s obliterates %s.", ammo_type, GET_VEH_NAME(veh));
    snprintf(buf1, sizeof(buf1), "You obliterate %s with your attack.", GET_VEH_NAME(veh));
    snprintf(buf2, sizeof(buf2), "A %s obliterates your ride.\r\n", ammo_type);
  } else if (damage_total >= DEADLY)
  {
    snprintf(buf, sizeof(buf), "$n's %s completely destroys %s.", ammo_type, GET_VEH_NAME(veh));
    snprintf(buf1, sizeof(buf1), "Your attack completely destroys %s.", GET_VEH_NAME(veh));
    snprintf(buf2, sizeof(buf2), "A %s completely destroys your ride.\r\n", ammo_type);
  }

  if (ch->in_veh && veh->in_veh != ch->in_veh)
    act(buf, FALSE, ch, NULL, NULL, TO_VEH_ROOM);
  else
    act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
  act(buf1, FALSE, ch, NULL, NULL, TO_CHAR);
  send_to_veh(buf2, veh, 0, TRUE);

  veh->damage += damage_total;
  if (veh->owner && !IS_NPC(ch))
  {
    char *cname = get_player_name(veh->owner);
    snprintf(buf, sizeof(buf), "%s attacked vehicle (%s) owned by player %s (%ld).", CAP(GET_CHAR_NAME(ch)), GET_VEH_NAME(veh), cname, veh->owner);
    delete [] cname;
    mudlog(buf, ch, LOG_WRECKLOG, TRUE);
  }
  chkdmg(veh);
  return FALSE;
}

bool mount_fire(struct char_data *ch)
{
  struct veh_data *veh = NULL;
  struct obj_data *mount = NULL, *gun = NULL;
  RIG_VEH(ch, veh);

  // Hands-on firing.
  if (AFF_FLAGGED(ch, AFF_MANNING)) {
    // No mount or mount is empty? Failure.
    if (!(mount = get_mount_manned_by_ch(ch)) || !(gun = get_mount_weapon(mount)))
      return FALSE;

    // No target? Failure.
    if (!mount->targ && !mount->tveh)
      return FALSE;

    // Got a target and a gun? Fire.
    if (mount->targ && (gun = get_mount_weapon(mount)))
      return hit(ch, mount->targ, gun, NULL, get_mount_ammo(mount));
    else {
      return vcombat(ch, mount->tveh);
    }
  }

  if (ch->char_specials.rigging || AFF_FLAGGED(ch, AFF_RIG)) {
    int num_mounts = 0;

    // Count mounts.
    for (mount = veh->mount; mount; mount = mount->next_content)
      if (!mount->worn_by && (gun = get_mount_weapon(mount)))
        num_mounts++;

    for (mount = veh->mount; mount; mount = mount->next_content) {
      // If nobody's manning it and it has a gun...
      if (!mount->worn_by && (gun = get_mount_weapon(mount))) {
        // Fire at the enemy, assuming we're fighting it.
        if (mount->targ && FIGHTING(ch) == mount->targ) {
          if (hit_with_multiweapon_toggle(ch, mount->targ, gun, NULL, get_mount_ammo(mount), num_mounts > 1))
            return TRUE;
        }
        else if (mount->tveh && FIGHTING_VEH(ch) == mount->tveh) {
          if (vcombat(ch, mount->tveh))
            return TRUE;
        }
      }
    }
  }
  return FALSE;
}
