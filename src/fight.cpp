#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <queue>

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "db.h"
#include "screen.h"
#include "newmagic.h"
#include "constants.h"
#include "config.h"
#include "memory.h"
#include "newmatrix.h"
#include "newmagic.h"
#include "bullet_pants.h"

/* Structures */
struct char_data *combat_list = NULL;   /* head of l-list of fighting chars */
struct char_data *next_combat_list = NULL;

/* External structures */
extern struct message_list fight_messages[MAX_MESSAGES];

extern const char *KILLER_FLAG_MESSAGE;


int find_sight(struct char_data *ch);
void damage_door(struct char_data *ch, struct room_data *room, int dir, int power, int type);
void damage_obj(struct char_data *ch, struct obj_data *obj, int power, int type);
void mount_fire(struct char_data *ch);
/* External procedures */
char *fread_action(FILE * fl, int nr);
char *fread_string(FILE * fl, char *error);
void stop_follower(struct char_data * ch);
ACMD_DECLARE(do_assist);
ACMD_CONST(do_flee);
ACMD_DECLARE(do_action);
void docwagon(struct char_data *ch);
void roll_individual_initiative(struct char_data *ch);
bool ranged_response(struct char_data *ch, struct char_data *vict);
int find_weapon_range(struct char_data *ch, struct obj_data *weapon);
void weapon_scatter(struct char_data *ch, struct char_data *victim,
                    struct obj_data *weapon);
void explode(struct char_data *ch, struct obj_data *weapon, struct room_data *room);
void target_explode(struct char_data *ch, struct obj_data *weapon,
                    struct room_data *room, int mode);
void forget(struct char_data * ch, struct char_data * victim);
void remember(struct char_data * ch, struct char_data * victim);
void order_list(bool first,...);
bool can_hurt(struct char_data *ch, struct char_data *victim, bool include_func_protections);

SPECIAL(johnson);
SPECIAL(weapon_dominator);
SPECIAL(landlord_spec);
SPECIAL(pocket_sec);
SPECIAL(receptionist);

extern int success_test(int number, int target);
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
extern void draw_weapon(struct char_data *);
extern void crash_test(struct char_data *ch);
extern int get_vehicle_modifier(struct veh_data *veh);
extern SPECIAL(shop_keeper);
extern void mob_magic(struct char_data *ch);
extern void cast_spell(struct char_data *ch, int spell, int sub, int force, char *arg);
extern char *get_player_name(vnum_t id);

// Corpse saving externs.
extern void House_save(struct house_control_rec *house, const char *file_name, long rnum);
extern void write_world_to_disk(int vnum);
extern bool Storage_get_filename(vnum_t vnum, char *filename, int filename_size);
extern bool House_get_filename(vnum_t vnum, char *filename, int filename_size);

extern void dominator_mode_switch(struct char_data *ch, struct obj_data *obj, int mode);
extern struct obj_data *generate_ammobox_from_pockets(struct char_data *ch, int weapontype, int ammotype, int quantity);

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
  {"thrash", "thrashes", "thrash"},     /* 10 */
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

void update_pos(struct char_data * victim)
{
  if ((GET_MENTAL(victim) < 100) && (GET_PHYSICAL(victim) >= 100))
  {
    for (struct obj_data *bio = victim->bioware; bio; bio = bio->next_content)
      if (GET_OBJ_VAL(bio, 0) == BIO_PAINEDITOR && GET_OBJ_VAL(bio, 3))
        return;
    GET_POS(victim) = POS_STUNNED;
    GET_INIT_ROLL(victim) = 0;
    return;
  }
  
  if ((GET_PHYSICAL(victim) >= 100) && (GET_POS(victim) > POS_STUNNED))
    return;
  else if (GET_PHYSICAL(victim) >= 100)
  {
    GET_POS(victim) = POS_STANDING;
    return;
  } else if ((int)(GET_PHYSICAL(victim) / 100) <= -GET_REAL_BOD(victim) + (GET_BIOOVER(victim) > 0 ? GET_BIOOVER(victim) : 0))
    GET_POS(victim) = POS_DEAD;
  else
    GET_POS(victim) = POS_MORTALLYW;
  
  GET_INIT_ROLL(victim) = 0;
}

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
            GET_CHAR_NAME(vict), vict->in_room->name);
    mudlog(buf, ch, LOG_MISCLOG, TRUE);
    
    send_to_char(KILLER_FLAG_MESSAGE, ch);
  }
}

/* start one char fighting another (yes, it is horrible, I know... )  */
void set_fighting(struct char_data * ch, struct char_data * vict, ...)
{
  struct follow_type *k;
  if (ch == vict)
    return;
  
  if (CH_IN_COMBAT(ch))
    return;
  
  if (IS_NPC(ch)) {
    // Prevents you from surprising someone who's attacking you already.
    GET_MOBALERTTIME(ch) = 20;
    GET_MOBALERT(ch) = MALERT_ALERT;
  }
  
  ch->next_fighting = combat_list;
  combat_list = ch;
  
  GET_INIT_ROLL(ch) = 0;
  FIGHTING(ch) = vict;
  GET_POS(ch) = POS_FIGHTING;
  if (!(AFF_FLAGGED(ch, AFF_MANNING) || PLR_FLAGGED(ch, PLR_REMOTE) || AFF_FLAGGED(ch, AFF_RIG)))
  {
    if (!(GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD)))
      draw_weapon(ch);
    
    if (GET_EQ(ch, WEAR_WIELD))
      if (!IS_GUN(GET_WEAPON_ATTACK_TYPE(GET_EQ(ch, WEAR_WIELD))) &&
          GET_WEAPON_ATTACK_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_ARROW)
        AFF_FLAGS(ch).SetBit(AFF_APPROACH);
    if (GET_EQ(ch, WEAR_HOLD))
      if (!IS_GUN(GET_WEAPON_ATTACK_TYPE(GET_EQ(ch, WEAR_HOLD))) &&
          GET_WEAPON_ATTACK_TYPE(GET_EQ(ch, WEAR_HOLD)) != TYPE_ARROW)
        AFF_FLAGS(ch).SetBit(AFF_APPROACH);
    if (!GET_EQ(ch, WEAR_WIELD) && !GET_EQ(ch, WEAR_HOLD))
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
  
  if (CH_IN_COMBAT(ch))
    return;
  
  ch->next_fighting = combat_list;
  combat_list = ch;
  
  roll_individual_initiative(ch);
  FIGHTING_VEH(ch) = vict;
  GET_POS(ch) = POS_FIGHTING;
  
  if (!(GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD)))
    draw_weapon(ch);
  
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
  update_pos(ch);
  GET_INIT_ROLL(ch) = 0;
  SHOOTING_DIR(ch) = NOWHERE;
}

void make_corpse(struct char_data * ch)
{
  struct obj_data *create_nuyen(int amount);
  struct obj_data *create_credstick(struct char_data *ch, int amount);
  
  struct obj_data *corpse;
  struct obj_data *money;
  int i, nuyen, credits, corpse_value = 0;
  if (PLR_FLAGGED(ch, PLR_NEWBIE))
    return;
  corpse = create_obj();
  
  corpse->item_number = NOTHING;
  corpse->in_room = NULL;
  
  if (IS_NPC(ch))
  {
    snprintf(buf, sizeof(buf), "corpse %s", ch->player.physical_text.keywords);
    snprintf(buf1, sizeof(buf1), "^rThe corpse of %s is lying here.^n", decapitalize_a_an(GET_NAME(ch)));
    snprintf(buf2, sizeof(buf2), "^rthe corpse of %s^n", decapitalize_a_an(GET_NAME(ch)));
    strlcpy(buf3, "What once was living is no longer. Poor sap.\r\n", sizeof(buf3));
  } else
  {
    snprintf(buf, sizeof(buf), "belongings %s", ch->player.physical_text.keywords);
    snprintf(buf1, sizeof(buf1), "^rThe belongings of %s are lying here.^n", decapitalize_a_an(GET_NAME(ch)));
    snprintf(buf2, sizeof(buf2), "^rthe belongings of %s^n", decapitalize_a_an(GET_NAME(ch)));
    strlcpy(buf3, "Looks like the DocWagon trauma team wasn't able to bring this stuff along.\r\n", sizeof(buf3));
    corpse->item_number = real_object(OBJ_SPECIAL_PC_CORPSE);
  }
  corpse->text.keywords = str_dup(buf);
  corpse->text.room_desc = str_dup(buf1);
  corpse->text.name = str_dup(buf2);
  corpse->text.look_desc = str_dup(buf3);
  GET_OBJ_TYPE(corpse) = ITEM_CONTAINER;
  // (corpse)->obj_flags.wear_flags.SetBits(ITEM_WEAR_TAKE, ENDBIT);
  (corpse)->obj_flags.extra_flags.SetBits(ITEM_NODONATE, // ITEM_NORENT,
                                          ITEM_CORPSE, ENDBIT);
  
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
      if (GET_OBJ_TYPE(o) == ITEM_GUN_MAGAZINE && !GET_OBJ_VAL(o, 1))
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
      corpse_value += GET_OBJ_COST( ch->equipment[i] );
      obj_to_obj(unequip_char(ch, i, TRUE), corpse);
    }
  }
  
  /* transfer nuyen & credstick */
  if (IS_NPC(ch))
  {
    nuyen = (int)(GET_NUYEN(ch) / 10);
    nuyen = number(GET_NUYEN(ch) - nuyen, GET_NUYEN(ch) + nuyen) * NUYEN_GAIN_MULTIPLIER;
    credits = (int)(GET_BANK(ch) / 10);
    credits = number(GET_BANK(ch) - credits, GET_BANK(ch) + credits) * NUYEN_GAIN_MULTIPLIER;
  } else
  {
    nuyen = GET_NUYEN(ch);
    credits = 0;
  }
  
  if (IS_NPC(ch) && (nuyen > 0 || credits > 0))
    if (vnum_from_non_connected_zone(GET_MOB_VNUM(ch)))
    {
      nuyen = 0;
      credits = 0;
    }
  
  if (nuyen > 0)
  {
    money = create_nuyen(nuyen);
    obj_to_obj(money, corpse);
    GET_NUYEN(ch) = 0;
  }
  
  if (credits > 0)
  {
    money = create_credstick(ch, credits);
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
  int door;
  struct room_data *was_in = NULL;
  
  act("$n cries out $s last breath as $e dies!", FALSE, ch, 0, 0, TO_ROOM);
  was_in = ch->in_room;
  
  for (door = 0; door < NUM_OF_DIRS; door++)
  {
    if (CAN_GO(ch, door)) {
      ch->in_room = was_in->dir_option[door]->to_room;
      act("Somewhere close, you hear someone's death cry!", FALSE, ch, 0, 0, TO_ROOM);
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
  
  if (IS_ELEMENTAL(ch) && GET_ACTIVE(ch))
  {
    for (struct descriptor_data *d = descriptor_list; d; d = d->next)
      if (d->character && GET_IDNUM(d->character) == GET_ACTIVE(ch)) {
        for (struct spirit_data *spirit = GET_SPIRIT(d->character); spirit; spirit = spirit->next)
          if (spirit->id == GET_GRADE(ch)) {
            spirit->called = FALSE;
          }
        send_to_char(d->character, "%s is disrupted and returns to the metaplanes to heal.\r\n", CAP(GET_NAME(ch)));
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
    
    if (!(IS_SPIRIT(ch) || IS_ELEMENTAL(ch)))
      make_corpse(ch);
    
    if (!IS_NPC(ch)) {
      for (bio = ch->bioware; bio; bio = bio->next_content)
        switch (GET_OBJ_VAL(bio, 0)) {
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
            GET_OBJ_VAL(bio, 3) = 0;
            break;
        }
      GET_DRUG_AFFECT(ch) = GET_DRUG_DURATION(ch) = GET_DRUG_STAGE(ch) = 0;
      if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED))
        i = real_room(RM_CHARGEN_START_ROOM);
      else switch (GET_JURISDICTION(in_room)) {
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
          snprintf(buf, sizeof(buf), "SYSERR: Bad jurisdiction type %d in room %ld encountered in raw_kill() while transferring %s (%ld).",
                  GET_JURISDICTION(in_room),
                  in_room->number,
                  GET_CHAR_NAME(ch), GET_IDNUM(ch));
          mudlog(buf, ch, LOG_SYSLOG, TRUE);
          i = real_room(RM_ENTRANCE_TO_DANTES);
          break;
      }
      char_from_room(ch);
      char_to_room(ch, &world[i]);
      PLR_FLAGS(ch).SetBit(PLR_JUST_DIED);
    }
  }
  
  if (IS_SPIRIT(ch) && GET_ACTIVE(ch))
    end_spirit_existance(ch, TRUE);
  else extract_char(ch);
}

void death_penalty(struct char_data *ch)
{
  int attribute = 0;
  int old_tke = GET_TKE( ch );
  
  if(!IS_NPC(ch)
     && !PLR_FLAGGED(ch, PLR_NEWBIE)
     && GET_REAL_BOD(ch) > 1
     && !(success_test(GET_REAL_BOD(ch),4) >= 1 ))
  {
    do {
      attribute = number(0,5);
    } while (attribute == CHA);
    GET_TKE(ch) -= 2*GET_REAL_ATT(ch, attribute);
    GET_REAL_ATT(ch, attribute)--;
    snprintf(buf, sizeof(buf),"%s lost a point of %s.  Total Karma Earned from %d to %d.",
            GET_CHAR_NAME(ch), short_attributes[attribute], old_tke, GET_TKE( ch ) );
    mudlog(buf, ch, LOG_DEATHLOG, TRUE);
    
  }
}

void die(struct char_data * ch)
{
  struct room_data *temp_room = get_ch_in_room(ch);
  
  if (!((IS_NPC(ch) && MOB_FLAGGED(ch, MOB_INANIMATE)) || IS_PROJECT(ch) || IS_SPIRIT(ch) || IS_ELEMENTAL(ch))) {
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
    send_to_char("Your mother would be so sad.. :(\n\r",ch);
    return;
  }
  
  send_to_char("You give up the will to live..\n\r",ch);
  
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
    
  if ((GET_RACE(vict) == RACE_SPIRIT || GET_RACE(vict) == RACE_ELEMENTAL) && GET_IDNUM(ch) == GET_ACTIVE(vict))
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
          int ammo_value = AMMO_NORMAL - index; // APDS = 3, EX = 2, Explosive = 1, Normal = 0, Gel -1, Flechette -2... see bullet_pants.cpp
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

void perform_group_gain(struct char_data * ch, int base, struct char_data * victim)
{
  int share;
  
  // Short-circuit: Not authed yet? No karma.
  if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED))
    return;
  
  share = MIN(max_exp_gain, MAX(1, base));
  if (!IS_NPC(ch))
    share = MIN(base, (int) (PLR_FLAGGED(ch, PLR_NEWBIE) ? 20 : GET_NOT(ch) * 2));
  
  /* psuedo-fix of the group with a newbie to get more exp exploit */
  if ( !PLR_FLAGGED(ch, PLR_NEWBIE) )
    share /= 2;
  
  share = gain_karma(ch, share, FALSE, TRUE, TRUE);
  
  if ( share >= (200) || access_level(ch, LVL_BUILDER) )
  {
    snprintf(buf, sizeof(buf),"%s gains %.2f karma from killing %s.", GET_CHAR_NAME(ch),
            (double)share/100.0, GET_CHAR_NAME(victim));
    mudlog(buf, ch, LOG_DEATHLOG, TRUE);
  }
  if ( IS_NPC( victim ) )
  {
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
        && f->follower->in_room == ch->in_room) {
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
      "$n grazes you as $e #W you."
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
  for (witness = victim->in_room->people; witness; witness = witness->next_in_room)
    if (witness != ch && witness != victim && !PRF_FLAGGED(witness, PRF_FIGHTGAG) && SENDOK(witness))
      perform_act(buf, ch, NULL, victim, witness);
  if (ch->in_room != victim->in_room && !PLR_FLAGGED(ch, PLR_REMOTE))
    for (witness = ch->in_room->people; witness; witness = witness->next_in_room)
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
    room = victim->in_room;
    
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
  
  for (vict = victim->in_room->people; vict; vict = vict->next_in_room)
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
    for (vict = victim->in_room->people; vict; vict = vict->next_in_room)
      if (vict != victim && !IS_ASTRAL(vict) && GET_POS(vict) >= POS_MORTALLYW &&
          !number(0, total - 1))
        break;
    
    if (vict && (IS_NPC(vict) || (!IS_NPC(vict) && vict->desc))) {
      snprintf(buf, sizeof(buf), "A %s flies in from nowhere, hitting you!", ammo_type);
      act(buf, FALSE, vict, 0, 0, TO_CHAR);
      snprintf(buf, sizeof(buf), "A %s hums into the room and hits $n!", ammo_type);
      act(buf, FALSE, vict, 0, 0, TO_ROOM);
      power = MAX(GET_OBJ_VAL(weapon, 0) - GET_BALLISTIC(vict) - 3, 2);
      damage_total = MAX(1, GET_OBJ_VAL(weapon, 1));
      damage_total = convert_damage(stage((2 - success_test(GET_BOD(vict) + GET_BODY(vict), power)), damage_total));
      damage(ch, vict, damage_total, TYPE_SCATTERING, PHYSICAL);
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
      damage_door(NULL, victim->in_room, dir[i], GET_OBJ_VAL(weapon, 0), DAMOBJ_PROJECTILE);
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
  
  int i = number(0, attacktype == TYPE_FIRE || attacktype == TYPE_ACID ? NUM_WEARS : 60);
  
  if (i >= WEAR_PATCH || !GET_EQ(victim, i))
    return;
  
  if (attacktype == TYPE_FIRE)
    damage_obj(ch, GET_EQ(victim, i), power, DAMOBJ_FIRE);
  if (attacktype == TYPE_ACID)
  {
    int x = 10;
    while (!GET_EQ(victim, i) && x > 0) {
      i = number(0, NUM_WEARS - 1);
      x--;
    }
    damage_obj(ch, GET_EQ(victim, i), power, DAMOBJ_ACID);
  }
  if (attacktype >= TYPE_PISTOL && attacktype <= TYPE_BIFURCATE)
  {
    damage_obj(ch, GET_EQ(victim, i), power, DAMOBJ_PROJECTILE);
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
    case TYPE_THRASH:
    case TYPE_ARROW:
    case TYPE_THROWING_KNIFE:
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
          return 5.0;
        default:
          return 0.7;
      }
      break;
    case DAMOBJ_WATER:
      switch (material) {
        case 10:
        case 11:
          return 3.0;
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
          return 3.0;
        default:
          return 1.1;
      }
      break;
    case DAMOBJ_PROJECTILE:
      switch (material) {
        case 2:
          return 3.0;
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
          return 4.0;
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
  
  if (IS_SET(type, DAMOBJ_MANIPULATION))
  {
    rating = room->dir_option[dir]->barrier;
    REMOVE_BIT(type, DAMOBJ_MANIPULATION);
  } else
    rating = room->dir_option[dir]->barrier * 2;
  
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
  
  int success = 0, dam, target, rating, half;
  struct char_data *vict = (obj->worn_by ? obj->worn_by : obj->carried_by);
  struct obj_data *temp, *next;
  
  // PC corpses are indestructable by normal means
  if ( IS_OBJ_STAT(obj, ITEM_CORPSE) && GET_OBJ_VAL(obj, 4) == 1 )
  {
    if ( ch != NULL )
      send_to_char("You are not allowed to damage a player's corpse.\n\r",ch);
    return;
  }
  if (IS_SET(type, DAMOBJ_MANIPULATION) || type == DAMOBJ_FIRE || type == DAMOBJ_ACID || type == DAMOBJ_LIGHTNING)
  {
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
  
  for (struct obj_data *cont = obj->contains; cont; cont = cont->next_content)
    damage_obj(ch, cont, power, type);
  
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
  if (power < rating) {
    if (ch)
      send_to_char(ch, "%s has been slightly damaged.\r\n",
                   CAP(GET_OBJ_NAME(obj)));
    if (vict && vict != ch)
      send_to_char(vict, "%s has been slightly damaged.\r\n",
                   CAP(GET_OBJ_NAME(obj)));
    GET_OBJ_CONDITION(obj)--;
  } else {
    if (ch)
      send_to_char(ch, "%s has been damaged!\r\n",
                   CAP(GET_OBJ_NAME(obj)));
    if (vict && vict != ch)
      send_to_char(vict, "%s has been damaged!\r\n",
                   CAP(GET_OBJ_NAME(obj)));
    GET_OBJ_CONDITION(obj) -= 1 + (power - rating) / half;
  }
  
  // if the end result is that the object condition rating is 0 or less
  // it is destroyed -- a good reason to keep objects in good repair
  if (GET_OBJ_CONDITION(obj) <= 0)
  {
    if (ch)
      send_to_char(ch, "%s has been destroyed!\r\n",
                   CAP(GET_OBJ_NAME(obj)));
    if (vict && vict != ch)
      send_to_char(vict, "%s has been destroyed!\r\n",
                   CAP(GET_OBJ_NAME(obj)));
    if (ch && !IS_NPC(ch) && GET_QUEST(ch))
      check_quest_destroy(ch, obj);
    else if (ch && AFF_FLAGGED(ch, AFF_GROUP) && ch->master &&
             !IS_NPC(ch->master) && GET_QUEST(ch->master))
      check_quest_destroy(ch->master, obj);
    
    // Disgorge contents, except for pocsecs.
    if (GET_OBJ_SPEC(obj) != pocket_sec) {
      for (temp = obj->contains; temp; temp = next) {
        next = temp->next_content;
        obj_from_obj(temp);
        if ((IS_OBJ_STAT(obj, ITEM_CORPSE) && !GET_OBJ_VAL(obj, 4) &&
             GET_OBJ_TYPE(temp) != ITEM_MONEY) || GET_OBJ_VNUM(obj) == 118)
          extract_obj(temp);
        else if (vict)
          obj_to_char(temp, vict);
        else if (obj->in_room)
          obj_to_room(temp, obj->in_room);
        else
          extract_obj(temp);
      }
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

void docwagon(struct char_data *ch)
{
  int i, creds;
  struct obj_data *docwagon = NULL;
  
  if (IS_NPC(ch) || PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED) || GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD)
    return;
  
  // Find the best docwagon contract they're wearing.
  for (i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_DOCWAGON && GET_DOCWAGON_BONDED_IDNUM(GET_EQ(ch, i)) == GET_IDNUM(ch))
      if (docwagon == NULL || GET_DOCWAGON_CONTRACT_GRADE(GET_EQ(ch, i)) > GET_DOCWAGON_CONTRACT_GRADE(docwagon))
      docwagon = GET_EQ(ch, i);
  
  if (!docwagon)
    return;
  
  // In an area with 4 or less security level: Basic has a 75% chance of rescue, Gold has 87.5% rescue, Plat has 93.8% chance.
  if (success_test(GET_DOCWAGON_CONTRACT_GRADE(docwagon) + 1,
                   MAX(GET_SECURITY_LEVEL(ch->in_room), 4)) > 0)
  {
    if (FIGHTING(ch) && FIGHTING(FIGHTING(ch)) == ch)
      stop_fighting(FIGHTING(ch));
    if (CH_IN_COMBAT(ch))
      stop_fighting(ch);
    if (GET_SUSTAINED(ch)) {
      struct sustain_data *next;
      for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = next) {
        next = sust->next;
        if (next && sust->idnum == next->idnum)
          next = next->next;
        end_sustained_spell(ch, sust);
      }
    }
    if (ch->persona) {
      snprintf(buf, sizeof(buf), "%s depixelizes and vanishes from the host.\r\n", ch->persona->name);
      send_to_host(ch->persona->in_host, buf, ch->persona, TRUE);
      extract_icon(ch->persona);
      ch->persona = NULL;
      PLR_FLAGS(ch).RemoveBit(PLR_MATRIX);
    } else if (PLR_FLAGGED(ch, PLR_MATRIX))
      for (struct char_data *temp = ch->in_room->people; temp; temp = temp->next_in_room)
        if (PLR_FLAGGED(temp, PLR_MATRIX))
          temp->persona->decker->hitcher = NULL;
    docwagon_message(ch);
    death_penalty(ch);  /* Penalty for deadly wounds */
    GET_PHYSICAL(ch) = 400;
    GET_MENTAL(ch) = 0;
    GET_POS(ch) = POS_STUNNED;
    for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content)
      switch (GET_OBJ_VAL(bio, 0)) {
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
          GET_OBJ_VAL(bio, 3) = 0;
          break;
      }
    ch->points.fire[0] = 0;
    send_to_char("\r\n\r\nYour last conscious memory is the arrival of a DocWagon.\r\n", ch);
    switch (GET_JURISDICTION(ch->in_room)) {
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
    }
    char_from_room(ch);
    char_to_room(ch, &world[i]);
    creds = MAX((number(8, 12) * 500 / GET_DOCWAGON_CONTRACT_GRADE(docwagon)), (int)(GET_NUYEN(ch) / 10));
    send_to_char(ch, "DocWagon demands %d nuyen for your rescue.\r\n", creds);
    if ((GET_NUYEN(ch) + GET_BANK(ch)) < creds) {
      send_to_char("Not finding sufficient payment, your DocWagon contract was retracted.\r\n", ch);
      extract_obj(docwagon);
    } else if (GET_BANK(ch) < creds) {
      GET_NUYEN(ch) -= (creds - GET_BANK(ch));
      GET_BANK(ch) = 0;
    } else
      GET_BANK(ch) -= creds;
  }
  return;
}


void check_adrenaline(struct char_data *ch, int mode)
{
  int i, dam;
  struct obj_data *pump = NULL;
  
  for (pump = ch->bioware; pump && GET_OBJ_VAL(pump, 0) != BIO_ADRENALPUMP; pump = pump->next_content)
    ;
  if (!pump)
    return;
  if (GET_OBJ_VAL(pump, 5) == 0 && mode == 1)
  {
    GET_OBJ_VAL(pump, 5) = dice(GET_OBJ_VAL(pump, 1), 6);
    GET_OBJ_VAL(pump, 6) = GET_OBJ_VAL(pump, 5);
    send_to_char("Your body is wracked with renewed vitality as adrenaline pumps into your\r\n"
                 "bloodstream.\r\n", ch);
    for (i = 0; i < MAX_OBJ_AFFECT; i++)
      affect_modify(ch,
                    pump->affected[i].location,
                    pump->affected[i].modifier,
                    pump->obj_flags.bitvector, TRUE);
  } else if (GET_OBJ_VAL(pump, 5) > 0 && !mode)
  {
    GET_OBJ_VAL(pump, 5)--;
    if (GET_OBJ_VAL(pump, 5) == 0) {
      for (i = 0; i < MAX_OBJ_AFFECT; i++)
        affect_modify(ch,
                      pump->affected[i].location,
                      pump->affected[i].modifier,
                      pump->obj_flags.bitvector, FALSE);
      GET_OBJ_VAL(pump, 5) = -number(80, 100);
      send_to_char("Your body softens and relaxes as the adrenaline wears off.\r\n", ch);
      dam = convert_damage(stage(-success_test(GET_BOD(ch) + GET_BODY(ch),
                                               (int)(GET_OBJ_VAL(pump, 6) / 2)), DEADLY));
      GET_OBJ_VAL(pump, 6) = 0;
      damage(ch, ch, dam, TYPE_BIOWARE, FALSE);
    }
  } else if (GET_OBJ_VAL(pump, 5) < 0 && !mode)
    GET_OBJ_VAL(pump, 5)++;
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

#define IS_SINGLE(eq)  (GET_OBJ_TYPE(eq) == ITEM_WEAPON && \
GET_OBJ_VAL(eq, 11) == MODE_SS)

#define IS_SEMI(eq)    (GET_OBJ_TYPE(eq) == ITEM_WEAPON && \
GET_OBJ_VAL(eq, 11) == MODE_SA)

#define IS_BURST(eq)   (GET_OBJ_TYPE(eq) == ITEM_WEAPON && \
GET_OBJ_VAL(eq, 11) == MODE_BF)

#define IS_AUTO(eq)    (GET_OBJ_TYPE(eq) == ITEM_WEAPON && \
GET_OBJ_VAL(eq, 11) == MODE_FA)

bool would_become_killer(struct char_data * ch, struct char_data * vict)
{
  char_data *attacker;
  
  if (IS_NPC(ch) && (ch->desc == NULL || ch->desc->original == NULL))
    return false;
  
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
    return true;
  }
  return false;
}

// Basically ripped the logic from damage(). Used to adjust combat messages for edge cases.
bool can_hurt(struct char_data *ch, struct char_data *victim, int attacktype, bool include_func_protections) {
  // Corpse protection.
  if (GET_POS(victim) <= POS_DEAD)
    return false;
    
  if (IS_NPC(victim)) {
    // Nokill protection.
    if (MOB_FLAGGED(victim,MOB_NOKILL))
      return false;
      
    // Quest target protection.
    if (victim->mob_specials.quest_id && victim->mob_specials.quest_id != GET_IDNUM(ch)) {
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
      
    // Special NPC protection.
    if (include_func_protections
        && (mob_index[GET_MOB_RNUM(victim)].func == shop_keeper 
            || mob_index[GET_MOB_RNUM(victim)].sfunc == shop_keeper
            || mob_index[GET_MOB_RNUM(victim)].func == johnson 
            || mob_index[GET_MOB_RNUM(victim)].sfunc == johnson
            || mob_index[GET_MOB_RNUM(victim)].func == landlord_spec
            || mob_index[GET_MOB_RNUM(victim)].sfunc == landlord_spec
            || mob_index[GET_MOB_RNUM(victim)].func == receptionist
            || mob_index[GET_MOB_RNUM(victim)].sfunc == receptionist))
    {
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
    // Known ignored edge case: if the player is not a killer but would become a killer because of this action.
    if (ch != victim && would_become_killer(ch, victim))
      return false;
    
    if (PLR_FLAGGED(ch, PLR_KILLER) && !IS_NPC(victim))
      return false;
  }
  
  // Looks like ch can hurt vict after all.
  return true;
}

// return 1 if victim died, 0 otherwise
bool damage(struct char_data *ch, struct char_data *victim, int dam, int attacktype,
            bool is_physical)
{
  char rbuf[MAX_STRING_LENGTH];
  int exp;
  bool total_miss = FALSE, awake = TRUE;
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
  
  if (!can_hurt(ch, victim, attacktype, TRUE)) {
    dam = -1;
    buf_mod(rbuf, sizeof(rbuf), "Can'tHurt",dam);
    if (ch)
      send_to_char(ch, "^m(OOC Notice: You are unable to damage %s.)^n\r\n", GET_CHAR_NAME(victim));
  }
  
  if (attacktype <= TYPE_BLACKIC)
    snprintf(rbuf, sizeof(rbuf),"Damage (%s vs %s, %s %s): ", GET_CHAR_NAME(ch), GET_CHAR_NAME(victim),
            wound_name[MIN(DEADLY, MAX(0, dam))], damage_type_names_must_subtract_300_first_and_must_not_be_greater_than_blackic[attacktype - 300]);
  else
    snprintf(rbuf, sizeof(rbuf),"Damage (%s vs %s, %s %d): ", GET_CHAR_NAME(ch), GET_CHAR_NAME(victim),
            wound_name[MIN(DEADLY, MAX(0, dam))], attacktype);
  
  if (victim != ch)
  {
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
  
  if (IS_AFFECTED(ch, AFF_HIDE))
    appear(ch);
  
  /* stop sneaking if it's the case */
  if (IS_AFFECTED(ch, AFF_SNEAK))
    AFF_FLAGS(ch).RemoveBit(AFF_SNEAK);
  
  if (PLR_FLAGGED(victim, PLR_PROJECT) && ch != victim)
  {
    do_return(victim, "", 0, 0);
    WAIT_STATE(victim, PULSE_VIOLENCE);
  }
  /* Figure out how to do WANTED flag*/
  
  if (ch != victim) {
    if (attacktype == TYPE_SCATTERING && !IS_NPC(ch) && !IS_NPC(victim)) {
      // Unless both chars are PK, or victim is KILLER, deal no damage and abort without flagging anyone as KILLER.
      if (!(PLR_FLAGGED(victim, PLR_KILLER) || (PRF_FLAGGED(ch, PRF_PKER) && PRF_FLAGGED(victim, PRF_PKER)))) {
        dam = -1;
        buf_mod(rbuf, sizeof(rbuf), "accidental", dam);
      } else {
        check_killer(ch, victim);
      }
    } else {
      check_killer(ch, victim);
    }
  }
  if (PLR_FLAGGED(ch, PLR_KILLER) && ch != victim && !IS_NPC(victim))
  {
    dam = -1;
    buf_mod(rbuf, sizeof(rbuf), "No-PK",dam);
  }
  if (attacktype == TYPE_EXPLOSION && (IS_ASTRAL(victim) || MOB_FLAGGED(victim, MOB_IMMEXPLODE)))
  {
    dam = -1;
    buf_mod(rbuf, sizeof(rbuf), "ImmExplode",dam);
  }
  if (dam == 0)
    strcat(rbuf, " 0");
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
  if (attacktype != TYPE_BIOWARE) {
    for (bio = victim->bioware; bio; bio = bio->next_content) {
      if (GET_OBJ_VAL(bio, 0) == BIO_PLATELETFACTORY && dam >= 3 && is_physical)
        dam--;
      else if (GET_OBJ_VAL(bio, 0) == BIO_DAMAGECOMPENSATOR)
        comp = GET_OBJ_VAL(bio, 1) * 100;
      else if (GET_OBJ_VAL(bio, 0) == BIO_TRAUMADAMPNER && GET_MENTAL(victim) >= 100)
        trauma = TRUE;
      else if (GET_OBJ_VAL(bio, 0) == BIO_PAINEDITOR && GET_OBJ_VAL(bio, 3))
        pain = TRUE;
    }
  }
  
  if (GET_PHYSICAL(victim) > 0)
    awake = FALSE;
  if (dam > 0) {
    if (is_physical) {
      GET_PHYSICAL(victim) -= MAX(dam * 100, 0);
      if (!pain && trauma && (!comp || (comp && GET_MENTAL(victim) > 0 && 1000 - GET_PHYSICAL(victim) > comp))) {
        GET_PHYSICAL(victim) += 100;
        GET_MENTAL(victim) -= 100;
      }
      AFF_FLAGS(victim).SetBit(AFF_DAMAGED);
      if (IS_PROJECT(victim)) {
        GET_PHYSICAL(victim->desc->original) -= MAX(dam * 100, 0);
        AFF_FLAGS(victim->desc->original).SetBit(AFF_DAMAGED);
      }
    } else if (((int)(GET_MENTAL(victim) / 100) - dam) < 0 ) {
      int physdam = dam - (int)(GET_MENTAL(victim) / 100);
      GET_MENTAL(victim) = 0;
      GET_PHYSICAL(victim) -= MAX(physdam * 100, 0);
      AFF_FLAGS(victim).SetBit(AFF_DAMAGED);
      if (IS_PROJECT(victim)) {
        GET_MENTAL(victim->desc->original) = 0;
        GET_PHYSICAL(victim->desc->original) -= MAX(physdam * 100, 0);
        AFF_FLAGS(victim->desc->original).SetBit(AFF_DAMAGED);
      }
    } else {
      GET_MENTAL(victim) -= MAX(dam * 100, 0);
      if (!pain && trauma && (!comp || (comp && GET_MENTAL(victim) > 0 && 1000 - GET_MENTAL(victim) > comp)))
        GET_MENTAL(victim) += 100;
      if (IS_PROJECT(victim))
        GET_MENTAL(victim->desc->original) -= MAX(dam * 100, 0);
    }
  }
  if (!awake && GET_PHYSICAL(victim) <= 0)
    victim->points.lastdamage = time(0);
  update_pos(victim);
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
    } else if (dam > 0) {
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
  } else if (dam > 0 && attacktype == TYPE_FALL) {
    if (dam > 5) {
      send_to_char(victim, "^RYou slam into the %s at speed, the impact reshaping your body in ways you do not appreciate.^n\r\n",
                   ROOM_FLAGGED(get_ch_in_room(victim), ROOM_INDOORS) ? "floor" : "ground");
      snprintf(buf3, sizeof(buf3), "^R$n slams into the %s at speed, the impact reshaping $s body in horrific ways.^n\r\n", ROOM_FLAGGED(get_ch_in_room(victim), ROOM_INDOORS) ? "floor" : "ground");
      act(buf3, FALSE, victim, 0, 0, TO_ROOM);
    } else {
      send_to_char(victim, "^rYou hit the %s with brusing force.^n\r\n", ROOM_FLAGGED(get_ch_in_room(victim), ROOM_INDOORS) ? "floor" : "ground");
      snprintf(buf3, sizeof(buf3), "^r$n hits the %s with bruising force.^n\r\n", ROOM_FLAGGED(get_ch_in_room(victim), ROOM_INDOORS) ? "floor" : "ground");
      act(buf3, FALSE, victim, 0, 0, TO_ROOM);
    }
  }
  
  if ((ch->in_room != victim->in_room) && total_miss && attacktype < TYPE_SUFFERING &&
      attacktype >= TYPE_PISTOL)
    weapon_scatter(ch, victim, GET_EQ(ch, WEAR_WIELD));
  
  if (victim->bioware && GET_POS(victim) > POS_STUNNED && dam > 0 && ch != victim)
    check_adrenaline(victim, 1);
  
  if (ch != victim && dam > 0 && attacktype >= TYPE_HIT)
    damage_equip(ch, victim, dam, attacktype);
  
  /* Use send_to_char -- act() doesn't send message if you are DEAD. */
  bool unbonded_docwagon = FALSE;
  struct obj_data *docwagon_biomonitor = NULL;
  switch (GET_POS(victim))
  {
    case POS_MORTALLYW:
      act("$n is mortally wounded, and will die soon, if not aided.",
          TRUE, victim, 0, 0, TO_ROOM);
      if (!PLR_FLAGGED(victim, PLR_MATRIX)) {
        send_to_char("You are mortally wounded, and will die soon, if not "
                     "aided.\r\n", victim);
      }
      if (!IS_NPC(victim))
        docwagon(victim);
      break;
    case POS_STUNNED:
      act("$n is stunned, but will probably regain consciousness again.",
          TRUE, victim, 0, 0, TO_ROOM);
      if (!PLR_FLAGGED(victim, PLR_MATRIX)) {
        send_to_char("You're stunned, but will probably regain consciousness "
                     "again.\r\n", victim);
      }
      break;
    case POS_DEAD:
      if (IS_NPC(victim))
        act("$n is dead!  R.I.P.", FALSE, victim, 0, 0, TO_ROOM);
      else
        act("$n slumps in a pile. You hear sirens as a DocWagon rushes in and grabs $m.", FALSE, victim, 0, 0, TO_ROOM);
        
      for (int i = 0; i < NUM_WEARS && !docwagon_biomonitor; i++) {
        if (GET_EQ(victim, i) && GET_OBJ_TYPE(GET_EQ(victim, i)) == ITEM_DOCWAGON) {
          if (GET_DOCWAGON_BONDED_IDNUM(GET_EQ(victim, i)) != GET_IDNUM(victim)) {
            unbonded_docwagon = TRUE;
          } else {
            docwagon_biomonitor = GET_EQ(victim, i);
          }
        }
      }
      
      if (docwagon_biomonitor) {
        send_to_char("You feel the vibration of your DocWagon biomonitor sending out an alert, but as the world slips into darkness you realize it's too little, too late.\r\n", victim);
      } else if (unbonded_docwagon) {
        send_to_char("Your last conscious thought involves wondering why your DocWagon biomonitor didn't work-- maybe you forgot to bond it? The world slips into darkness, and you hope a wandering DocWagon finds you.\r\n", victim);
      } else {
        send_to_char("You feel the world slip in to darkness, you'd better hope a wandering DocWagon finds you.\r\n", victim);
      }
      
      break;
    default:                      /* >= POSITION SLEEPING */
      if (dam > ((int)(GET_MAX_PHYSICAL(victim) / 100) >> 2))
        act("^RThat really did HURT!^n", FALSE, victim, 0, 0, TO_CHAR);
      
      if (GET_PHYSICAL(victim) < (GET_MAX_PHYSICAL(victim) >> 2))
        send_to_char(victim, "%sYou wish that your wounds would stop "
                     "^RBLEEDING^r so much!%s\r\n", CCRED(victim, C_SPR),
                     CCNRM(victim, C_SPR));
      
      if (MOB_FLAGGED(victim, MOB_WIMPY) && !AFF_FLAGGED(victim, AFF_PRONE) && ch != victim &&
          GET_PHYSICAL(victim) < (GET_MAX_PHYSICAL(victim) >> 2))
        do_flee(victim, "", 0, 0);
      
      if (!IS_NPC(victim) && GET_WIMP_LEV(victim) && victim != ch &&
          (int)(GET_PHYSICAL(victim) / 100) < GET_WIMP_LEV(victim)) {
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
        if (IS_AFFECTED(ch, AFF_GROUP)) {
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
    return 1;
  }
  return 0;
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
          update_ammobox_ammo_quantity(ammobox, -1);
        return TRUE;
      }
      
      // No ammo boxes found, or the ones that were found were empty. Click it.
      send_to_char("*Click*\r\n", ch);
      return FALSE;
    } // End manned checks.
    
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
      send_to_char("*Click*\r\n", ch);
      
      act("$n can't fire- no ammo.", TRUE, ch, 0, 0, TO_ROLLS);

      // NPCs try to reload, and if they fail they switch weapons.
      if (IS_NPC(ch) && !attempt_reload(ch, wielded->worn_on))
        switch_weapons(ch, wielded->worn_on);
      
      // Regardless-- no ammo in the current weapon means we lose our turn.
      return FALSE;
    } // End magazine checks.
    
    // Check for a non-ammo-requiring gun. No deduction needed here.
    if (GET_WEAPON_MAX_AMMO(wielded) < 0)
      return TRUE;
    
    // The weapon requires a magazine and doesn't have one.
    send_to_char("*Click*\r\n", ch);
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
  
  int mod = 0, real_obj;
  for (int i = ACCESS_LOCATION_TOP; !mod && i <= ACCESS_LOCATION_UNDER; i++) {
    // If they have a smartlink attached:
    if (GET_OBJ_VAL(weapon, i) > 0
        && (real_obj = real_object(GET_OBJ_VAL(weapon, i))) > 0
        && (access = &obj_proto[real_obj])
        && GET_ACCESSORY_TYPE(access) == ACCESS_SMARTLINK) {
      
      // Iterate through their cyberware and look for a matching smartlink.
      for (obj = ch->cyberware; !mod && obj; obj = obj->next_content) {
        if (GET_OBJ_VAL(obj, 0) == CYB_SMARTLINK) {
          if (GET_ACCESSORY_TYPE(obj) == 2 && GET_ACCESSORY_RATING(access) == 2) {
            // Smartlink II with compatible cyberware.
            return 3;
          }
          // Smartlink I.
          return 2;
        }
      }
      if (!mod 
          && GET_EQ(ch, WEAR_EYES) 
          && GET_OBJ_TYPE(GET_EQ(ch, WEAR_EYES)) == ITEM_GUN_ACCESSORY 
          && GET_ACCESSORY_TYPE(GET_EQ(ch, WEAR_EYES)) == ACCESS_SMARTGOGGLE) {
        // Smartlink plus goggle found-- half value.
        return 1;
      }
    }
  }
  
  if (mod < 1 && AFF_FLAGGED(ch, AFF_LASER_SIGHT)) {
    // Lasers apply when no smartlink was found.
    mod = 1;
  }
  
  return mod;
}

int check_recoil(struct char_data *ch, struct obj_data *gun)
{
  struct obj_data *obj;
  int i, rnum, comp = 0;
  
  if (!gun || GET_OBJ_TYPE(gun) != ITEM_WEAPON)
    return 0;
  
  for (i = 7; i < 10; i++)
  {
    obj = NULL;
    
    if (GET_OBJ_VAL(gun, i) > 0 &&
        (rnum = real_object(GET_OBJ_VAL(gun, i))) > -1 &&
        (obj = &obj_proto[rnum]) && GET_OBJ_TYPE(obj) == ITEM_GUN_ACCESSORY) {
      if (GET_OBJ_VAL(obj, 1) == ACCESS_GASVENT)
        comp += 0 - GET_OBJ_VAL(obj, 2);
      else if (GET_OBJ_VAL(obj, 1) == ACCESS_SHOCKPAD)
        comp++;
      else if (AFF_FLAGGED(ch, AFF_PRONE)) {
        if (GET_OBJ_VAL(obj, 1) == ACCESS_BIPOD)
          comp += RECOIL_COMP_VALUE_BIPOD;
        else if (GET_OBJ_VAL(obj, 1) == ACCESS_TRIPOD)
          comp += RECOIL_COMP_VALUE_TRIPOD;
      }
      
      // Add in integral recoil compensation.
      if (GET_WEAPON_INTEGRAL_RECOIL_COMP(obj))
        comp += GET_WEAPON_INTEGRAL_RECOIL_COMP(obj);
    }
  }
  for (obj = ch->cyberware; obj; obj = obj->next_content)
    if (GET_OBJ_VAL(obj, 0) == CYB_FOOTANCHOR && !GET_OBJ_VAL(obj, 9))
      comp++;
  
  return comp;
}

void astral_fight(struct char_data *ch, struct char_data *vict)
{
  int w_type, dam, power, attack_success, newskill, skill_total, base_target;
  bool focus = FALSE, is_physical;
  
  struct obj_data *wielded = ch->equipment[WEAR_WIELD];
  
  if (ch->in_room != vict->in_room) {
    stop_fighting(ch);
    if (FIGHTING(vict) == ch)
      stop_fighting(vict);
    return;
  }
  
  if ((IS_PROJECT(ch) || PLR_FLAGGED(ch, PLR_PERCEIVE))
      && wielded
      && GET_OBJ_TYPE(wielded) == ITEM_WEAPON
      && GET_OBJ_VAL(wielded, 3) < TYPE_TASER
      && GET_OBJ_VAL(wielded, 7)
      && GET_OBJ_VAL(wielded, 8)
      && GET_OBJ_VAL(wielded, 9) == GET_IDNUM(ch))
    focus = TRUE;
  else if (wielded)
  {
    stop_fighting(ch);
    return;
  }
  
  if (IS_PROJECT(ch) && (ch != vict) && PLR_FLAGGED(vict, PLR_PERCEIVE) &&
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
  
  if (GET_POS(vict) <= POS_DEAD)
  {
    log("SYSERR: Attempt to damage a corpse.");
    return;                     /* -je, 7/7/92 */
  }
  
  if (wielded) {
    switch (GET_OBJ_VAL(wielded, 3)) {
      case WEAP_EDGED:
      case WEAP_POLEARM:
        w_type = TYPE_SLASH;
        break;
      case WEAP_WHIP:
        w_type = TYPE_POUND;
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
    is_physical = FALSE;
  else
    is_physical = TRUE;
  
  base_target = 4 + modify_target(ch);
  
  if (!AWAKE(vict))
    base_target -= 2;
  
  if ((w_type != TYPE_HIT) && wielded)
  {
    power = (GET_OBJ_VAL(wielded, 0) ? GET_OBJ_VAL(wielded, 0) : GET_STR(ch)) +
    GET_OBJ_VAL(wielded, 2);
    if (focus)
      power += GET_OBJ_VAL(wielded, 8);
    power -= GET_IMPACT(vict);
    dam = MODERATE;
    if (IS_SPIRIT(vict) || IS_ELEMENTAL(vict))
      skill_total = GET_WIL(ch);
    else if (GET_SKILL(ch, GET_OBJ_VAL(wielded, 4)) < 1) {
      newskill = return_general(GET_OBJ_VAL(wielded, 4));
      skill_total = get_skill(ch, newskill, base_target);
    } else
      skill_total = GET_SKILL(ch, GET_OBJ_VAL(wielded, 4));
  } else
  {
    power = GET_STR(ch) - GET_IMPACT(vict);
    if (IS_PROJECT(ch) || PLR_FLAGGED(ch, PLR_PERCEIVE))
      dam = LIGHT;
    else
      dam = MODERATE;
    if (IS_SPIRIT(vict) || IS_ELEMENTAL(vict))
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
    if (!AFF_FLAGGED(ch, AFF_COUNTER_ATT)) {
      if ((GET_ASTRAL(vict) > 0) 
          && (attack_success < 0)
          && FIGHTING(vict) == ch 
          && GET_POS(vict) > POS_SLEEPING) {
        send_to_char(ch, "%s counters your attack!\r\n", GET_NAME(vict));
        send_to_char(vict, "You counter %s's attack!\r\n", GET_NAME(ch));
        AFF_FLAGS(vict).SetBit(AFF_COUNTER_ATT);
        astral_fight(vict, ch);
      }
      return;
    } else {
      AFF_FLAGS(ch).RemoveBit(AFF_COUNTER_ATT);
      return;
    }
  } else if (AFF_FLAGGED(ch, AFF_COUNTER_ATT))
    AFF_FLAGS(ch).RemoveBit(AFF_COUNTER_ATT);
  
  attack_success -= success_test(GET_BOD(vict) + GET_BODY(vict), power);
  
  dam = convert_damage(stage(attack_success, dam));
  
  if (IS_PROJECT(ch) && PLR_FLAGGED(ch->desc->original, PLR_KILLER))
    dam = 0;
  
  damage(ch, vict, dam, w_type, is_physical);
  if (IS_PROJECT(vict) && dam > 0)
    damage(vict->desc->original, vict->desc->original, dam, 0, is_physical);
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

void combat_message_process_ranged_response(struct char_data *ch, rnum_t rnum) {
  for (struct char_data *tch = world[rnum].people; tch; tch = tch->next_in_room) {
    if (IS_NPC(tch)) {
      // Everyone ends up on edge when they hear gunfire nearby.
      GET_MOBALERTTIME(tch) = 20;
      GET_MOBALERT(tch) = MALERT_ALERT;
      
      // Guards and helpers will actively try to fire on a player using a gun.
      if (!IS_NPC(ch) && CH_IN_COMBAT(ch)
          && (MOB_FLAGGED(tch, MOB_GUARD) || MOB_FLAGGED(tch, MOB_HELPER))
          && !CH_IN_COMBAT(tch)
          && !(FIGHTING(ch) ? (IS_NPC(FIGHTING(ch)) && MOB_FLAGGED(FIGHTING(ch), MOB_INANIMATE)) : TRUE)) {
        if (number(0, 6) >= 2) {
          GET_MOBALERT(tch) = MALERT_ALARM;
          struct room_data *was_in = tch->in_room;
          if (ranged_response(ch, tch) && tch->in_room == was_in) {
            act("$n aims $s weapon at a distant threat!",
                FALSE, tch, 0, ch, TO_ROOM);
          }
        }
      }
    }
  }
}

// Pick your favorite.
#define COMBAT_MESSAGE_DEBUG_LOG(str) log(str);
//#define COMBAT_MESSAGE_DEBUG_LOG(str) ;
void combat_message(struct char_data *ch, struct char_data *victim, struct obj_data *weapon, int damage, int burst)
{
  char buf[MAX_MESSAGE_LENGTH], buf1[MAX_MESSAGE_LENGTH], buf2[MAX_MESSAGE_LENGTH], buf3[MAX_MESSAGE_LENGTH], buf4[MAX_MESSAGE_LENGTH],
  been_heard[MAX_STRING_LENGTH], temp[20];
  struct obj_data *obj = NULL;
  struct room_data *ch_room = NULL, *vict_room = NULL;
  rnum_t room1 = 0, room2 = 0, rnum = 0;
  
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
  ch_room = get_ch_in_room(ch);
  vict_room = get_ch_in_room(victim);
  if (ch->in_room == victim->in_room) {
    // Same-room messaging.
    static char vehicle_message[1000];
    
    if (ch->in_veh)
      snprintf(vehicle_message, sizeof(vehicle_message), "From inside %s, ", decapitalize_a_an(GET_VEH_NAME(ch->in_veh)));
    else
      strcpy(vehicle_message, "");
    
    if (damage < 0) {
      switch (number(1, 3)) {
        case 1:
          snprintf(buf1, sizeof(buf1), "^r%s$n fires a %s^r at you but you manage to dodge.^n", vehicle_message, buf);
          snprintf(buf2, sizeof(buf2), "^yYou fire a %s^y at $N but $E manage%s to dodge.^n", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "%s$n fires a %s^n at $N but $E manage%s to dodge.", vehicle_message, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
        case 2:
          snprintf(buf1, sizeof(buf1), "^r%s$n fires a %s^r at you but you easily dodge.^n", vehicle_message, buf);
          snprintf(buf2, sizeof(buf2), "^yYou fire a %s^y at $N but $E easily dodge%s.^n", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "%s$n fires a %s^n at $N but $E easily dodge%s.", vehicle_message, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
        case 3:
          snprintf(buf1, sizeof(buf1), "^r%s$n fires a %s^r at you but you move out of the way.^n", vehicle_message, buf);
          snprintf(buf2, sizeof(buf2), "^yYou fire a %s^y at $N but $E move%s out of the way.^n", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "%s$n fires a %s^n at $N but $E move%s out of the way.", vehicle_message, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
      }
    } else if (damage == 0) {
      switch (number(1, 2)) {
        case 1:
          snprintf(buf1, sizeof(buf1), "^r%s$n fires a %s^r at you but your armor holds.^n", vehicle_message, buf);
          snprintf(buf2, sizeof(buf2), "^yYou fire a %s^y at $N but it doesn't seem to hurt $M.^n", buf);
          snprintf(buf3, sizeof(buf3), "%s$n fires a %s^n at $N but it doesn't seem to hurt $M.", vehicle_message, buf);
          break;
        case 2:
          snprintf(buf1, sizeof(buf1), "^r%s$n fires a %s^r at you but you roll with the impact.^n", vehicle_message, buf);
          snprintf(buf2, sizeof(buf2), "^yYou fire a %s^y at $N but $E roll%s with the impact.^n", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "%s$n fires a %s^n at $N but $E roll%s with the impact.", vehicle_message, buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
      }
    } else if (damage == LIGHT) {
      snprintf(buf1, sizeof(buf1), "^r%s$n grazes you with a %s^r.^n", vehicle_message, buf);
      snprintf(buf2, sizeof(buf2), "^yYou graze $N with a %s^y.^n", buf);
      snprintf(buf3, sizeof(buf3), "%s$n grazes $N with a %s^n.", vehicle_message, buf);
    } else if (damage == MODERATE) {
      snprintf(buf1, sizeof(buf1), "^r%s$n hits you with a %s^r.^n", vehicle_message, buf);
      snprintf(buf2, sizeof(buf2), "^yYou hit $N with a %s^y.^n", buf);
      snprintf(buf3, sizeof(buf3), "%s$n hits $N with a %s^n.", vehicle_message, buf);
    } else if (damage == SERIOUS) {
      snprintf(buf1, sizeof(buf1), "^r%s$n massacres you with a %s^r.^n", vehicle_message, buf);
      snprintf(buf2, sizeof(buf2), "^yYou massacre $N with a %s^y.^n", buf);
      snprintf(buf3, sizeof(buf3), "%s$n massacres $N with a %s^n.", vehicle_message, buf);
    } else if (damage >= DEADLY) {
      switch (number(1, 2)) {
        case 1:
          snprintf(buf1, sizeof(buf1), "^r%s$n puts you down with a deadly %s^r.^n", vehicle_message, buf);
          snprintf(buf2, sizeof(buf2), "^yYou put $N down with a deadly %s^y.^n", buf);
          snprintf(buf3, sizeof(buf3), "%s$n puts $N down with a deadly %s^n.", vehicle_message, buf);
          break;
        case 2:
          snprintf(buf1, sizeof(buf1), "^r%s$n sublimates you with a deadly %s^r.^n", vehicle_message, buf);
          snprintf(buf2, sizeof(buf2), "^yYou sublimate $N with a deadly %s^y.^n", buf);
          snprintf(buf3, sizeof(buf3), "%s$n sublimates $N with a deadly %s^n.", vehicle_message, buf);
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
          snprintf(buf1, sizeof(buf1), "^r$n fires a %s^r at you but you manage to dodge.^n", buf);
          snprintf(buf2, sizeof(buf2), "^yYou fire a %s^y at $N but $E manage%s to dodge.^n", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "$n fires a %s^n at $N but $E manage%s to dodge.", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf4, sizeof(buf4), "$N fires a %s^n at $n but $e manage%s to dodge.", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
        case 2:
          snprintf(buf1, sizeof(buf1), "^r$n fires a %s^r at you but you easily dodge.^n", buf);
          snprintf(buf2, sizeof(buf2), "^yYou fire a %s^y at $N but $E easily dodge%s.^n", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "$n fires a %s^n at $N but $E easily dodge%s.", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf4, sizeof(buf4), "$N fires a %s^n at $n but $e easily dodge%s.", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
        case 3:
          snprintf(buf1, sizeof(buf1), "^r$n fires a %s^r at you but you move out of the way.^n", buf);
          snprintf(buf2, sizeof(buf2), "^yYou fire a %s^y at $N but $E move%s out of the way.^n", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "$n fires a %s^n at $N but $E move%s out of the way.", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf4, sizeof(buf4), "$N fires a %s^n at $n but $e move%s out of the way.", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
      }
    } else if (damage == 0) {
      switch (number(1, 2)) {
        case 1:
          snprintf(buf1, sizeof(buf1), "^r$n fires a %s^r at you but your armor holds.^n", buf);
          snprintf(buf2, sizeof(buf2), "^yYou fire a %s^y at $N but it doesn't seem to hurt $M.^n", buf);
          snprintf(buf3, sizeof(buf3), "$n fires a %s^n at $N but it doesn't seem to hurt $M.", buf);
          snprintf(buf4, sizeof(buf4), "$N fires a %s^n at $n but it doesn't seem to hurt $m.", buf);
          break;
        case 2:
          snprintf(buf1, sizeof(buf1), "^r$n fires a %s^r at you but you roll with the impact.^n", buf);
          snprintf(buf2, sizeof(buf2), "^yYou fire a %s^y at $N but $E roll%s with the impact.^n", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf3, sizeof(buf3), "$n fires a %s^n at $N but $E roll%s with the impact.", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          snprintf(buf4, sizeof(buf4), "$N fires a %s^n at $n but $e roll%s with the impact.", buf, HSSH_SHOULD_PLURAL(victim) ? "s" : "");
          break;
      }
    } else if (damage == LIGHT) {
      snprintf(buf1, sizeof(buf1), "^r$n grazes you with a %s^r.^n", buf);
      snprintf(buf2, sizeof(buf2), "^yYou graze $N with a %s^y.^n", buf);
      snprintf(buf3, sizeof(buf3), "$n grazes $N with a %s^n.", buf);
      snprintf(buf4, sizeof(buf4), "$N grazes $n with a %s^n.", buf);
    } else if (damage == MODERATE) {
      snprintf(buf1, sizeof(buf1), "^r$n hits you with a %s^r.^n", buf);
      snprintf(buf2, sizeof(buf2), "^yYou hit $N with a %s^y.^n", buf);
      snprintf(buf3, sizeof(buf3), "$n hits $N with a %s^n.", buf);
      snprintf(buf4, sizeof(buf4), "$N hits $n with a %s^n.", buf);
    } else if (damage == SERIOUS) {
      snprintf(buf1, sizeof(buf1), "^r$n massacres you with a %s^r.^n", buf);
      snprintf(buf2, sizeof(buf2), "^yYou massacre $N with a %s^y.^n", buf);
      snprintf(buf3, sizeof(buf3), "$n massacres $N with a %s^n.", buf);
      snprintf(buf4, sizeof(buf4), "$N massacres $n with a %s^n.", buf);
    } else if (damage >= DEADLY) {
      switch (number(1, 2)) {
        case 1:
          snprintf(buf1, sizeof(buf1), "^r$n puts you down with a deadly %s^r.^n", buf);
          snprintf(buf2, sizeof(buf2), "^yYou put $N down with a deadly %s^y.^n", buf);
          snprintf(buf3, sizeof(buf3), "$n puts $N down with a deadly %s^n.", buf);
          snprintf(buf4, sizeof(buf4), "$N puts $n down with a deadly %s^n.", buf);
          break;
        case 2:
          snprintf(buf1, sizeof(buf1), "^r$n sublimates you with a deadly %s^r.^n", buf);
          snprintf(buf2, sizeof(buf2), "^yYou sublimate $N with a deadly %s^y.^n", buf);
          snprintf(buf3, sizeof(buf3), "$n sublimates $N with a deadly %s^n.", buf);
          snprintf(buf4, sizeof(buf4), "$N sublimates $n with a deadly %s^n.", buf);
          break;
      }
    }
    act(buf1, FALSE, ch, weapon, victim, TO_VICT);
    act(buf2, FALSE, ch, weapon, victim, TO_CHAR | TO_REMOTE);
    act(buf3, FALSE, ch, weapon, victim, TO_NOTVICT);
    act(buf4, FALSE, victim, weapon, ch, TO_ROOM);
    // End ranged messaging.
  }
  
  // If the player's in a silent room, don't propagate the gunshot.
  if (ch_room->silence[0])
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
    if (ch_room->dir_option[door1] && (room1 = real_room(ch_room->dir_option[door1]->to_room->number)) != NOWHERE && !(world[room1].silence[0])) {
      // If the room is in the heard-it-already list, skip to the next one.
      snprintf(temp, sizeof(temp), ".%ld.", room1);
      if (strstr(been_heard, temp) != 0)
        continue;
      
      if (has_suppressor) {
        // Special case: If they're using a suppressed weapon, print a muffled-gunfire message and terminate.
        send_to_room("You hear muffled gunshots nearby.\r\n", &world[room1]);
        combat_message_process_ranged_response(ch, room1);
        strcat(been_heard, temp);
      } else {
        // Send gunshot notifications to the selected room. Process guard/helper responses.
        send_to_room("You hear gunshots nearby!\r\n", &world[room1]);
        combat_message_process_ranged_response(ch, room1);
        strcat(been_heard, temp);
        
        // Add the room's exits to the list.
        for (int door2 = 0; door2 < NUM_OF_DIRS; door2++)
          if (world[room1].dir_option[door2] && (room2 = real_room(world[room1].dir_option[door2]->to_room->number)) != NOWHERE && !(world[room2].silence[0]))
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
    
    send_to_room("You hear gunshots not far off.\r\n", &world[room1]);
    combat_message_process_ranged_response(ch, room1);
    strcat(been_heard, temp);
    
    // Add the room's exits to the list.
    for (int door = 0; door < NUM_OF_DIRS; door++)
      if (world[room1].dir_option[door] && (room2 = real_room(world[room1].dir_option[door]->to_room->number)) != NOWHERE && !(world[room2].silence[0]))
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
    
    send_to_room("You hear gunshots in the distance.\r\n", &world[room1]);
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

int calculate_vision_penalty(struct char_data *ch, struct char_data *victim) {
  // TODO: Why would someone handling a vehicle or manning a turret negate visibility penalties?
  // !(ch->in_veh || PLR_FLAGGED(ch, PLR_REMOTE) || AFF_FLAGGED(ch, AFF_MANNING))
  int modifier = 0;
  if (!CAN_SEE(ch, victim)) {
    modifier = 8;
    if (AFF_FLAGGED(ch, AFF_DETECT_INVIS) || GET_POWER(ch, ADEPT_BLIND_FIGHTING))
      modifier = 4;
  }
  return modifier;
}

//todo: single shot weaps can only be fired once per combat phase-- what does this mean for us?

struct combat_data *populate_cyberware(struct char_data *ch, struct combat_data *cd) {
  // Pull info on all the character's active cyberware.
  for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content) {
    if (!GET_CYBERWARE_IS_DISABLED(obj)) {
      switch (GET_CYBERWARE_TYPE(obj)) {
        case CYB_CLIMBINGCLAWS:
          cd->climbingclaws++;
          cd->num_cyberweapons++;
          break;
        case CYB_FIN:
          cd->fins++;
          cd->num_cyberweapons++;
          break;
        case CYB_HANDBLADE:
          cd->handblades++;
          cd->num_cyberweapons++;
          break;
        case CYB_HANDRAZOR:
          if (IS_SET(GET_CYBERWARE_FLAGS(obj), CYBERWEAPON_IMPROVED))
            cd->improved_handrazors++;
          else
            cd->handrazors++;
          cd->num_cyberweapons++;
          break;
        case CYB_HANDSPUR:
          cd->handspurs++;
          cd->num_cyberweapons++;
          break;
        case CYB_FOOTANCHOR:
          cd->footanchors++;
          cd->num_cyberweapons++;
          break;
          
      }
    } else if (GET_CYBERWARE_TYPE(obj) == CYB_BONELACING) {
      switch (GET_CYBERWARE_LACING_TYPE(obj)) {
        case BONE_PLASTIC:
          cd->bone_lacing_power = MAX(cd->bone_lacing_power, 2);
          break;
        case BONE_ALUMINIUM:
        case BONE_CERAMIC:
          cd->bone_lacing_power = MAX(cd->bone_lacing_power, 3);
          break;
        case BONE_TITANIUM:
          cd->bone_lacing_power = MAX(cd->bone_lacing_power, 4);
          break;
      }
    }
  }
  return cd;
}

bool weapon_has_bayonet(struct obj_data *weapon) {
  // Precondition: Weapon must exist, must be a weapon-class item, and must be a gun.
  if (!(weapon && GET_OBJ_TYPE(weapon) == ITEM_WEAPON && IS_GUN(GET_WEAPON_ATTACK_TYPE(weapon))))
    return FALSE;

  long attach_obj;
  struct obj_data *attach_proto = NULL;
  
  return (GET_WEAPON_ATTACH_UNDER_VNUM(weapon)
          && (attach_obj = real_object(GET_WEAPON_ATTACH_UNDER_VNUM(weapon))) > 0
          && (attach_proto = &obj_proto[attach_obj])
          && GET_OBJ_VAL(attach_proto, 1) == ACCESS_BAYONET);
}

int get_melee_skill(struct combat_data *cd) {
  assert(cd != NULL);
  
  int skill = 0;
  if (cd->weapon && GET_OBJ_TYPE(cd->weapon) == ITEM_WEAPON) {
    // If you're wielding a gun, you get to beat your opponent with it (or stab them, if it has a bayonet).
    if (cd->weapon_is_gun) {
      if (cd->weapon_has_bayonet)
        skill = SKILL_POLE_ARMS;
      else
        skill = SKILL_CLUBS;
    }
    // Non-gun weapons.
    else {
      skill = GET_OBJ_VAL(cd->weapon, 4);
    }
  } else {
    // If the character has active cyber weapons, use the cyber implants skill.
    if (cd->num_cyberweapons > 0) {
      skill = SKILL_CYBER_IMPLANTS;
    }
    // Otherwise, they use their standard unarmed skill.
    else {
      skill = SKILL_UNARMED_COMBAT;
    }
  }
  
  return skill;
}

void hit(struct char_data *attacker, struct char_data *victim, struct obj_data *weap, struct obj_data *vict_weap)
{  
  // Initialize our data structures for holding this round's fight-related data.
  struct combat_data attacker_data(attacker, weap);
  struct combat_data defender_data(victim, vict_weap);
  
  // Allows for switching roles, which can happen during melee counterattacks.
  struct combat_data *att = &attacker_data;
  struct combat_data *def = &defender_data;
  
  // Set the base TNs to accommodate get_skill() changing the TNs like the special snowflake it is.
  att->tn = 4;
  def->tn = 4;
  
  char rbuf[MAX_STRING_LENGTH];
  bool melee = FALSE;
  
  // Precondition: If you're wielding a non-weapon, back out.
  if (att->weapon && (GET_OBJ_TYPE(att->weapon) != ITEM_WEAPON && GET_OBJ_TYPE(att->weapon) != ITEM_FIREWEAPON)) {
    send_to_char(att->ch, "You struggle to figure out how to attack while using %s as a weapon!\r\n", decapitalize_a_an(GET_OBJ_NAME(att->weapon)));
    return;
  }
  
  // Precondition: If you're asleep or paralyzed, you don't get to fight, and also your opponent closes immediately.
  if (!AWAKE(att->ch) || GET_QUI(att->ch) <= 0) {
    AFF_FLAGS(attacker).RemoveBit(AFF_APPROACH);
    AFF_FLAGS(victim).RemoveBit(AFF_APPROACH);
    return;
  }
  
  // Precondition: If you're out of ammo, you don't get to fight. Note the use of the deducting has_ammo here.
  if (att->weapon && !has_ammo(att->ch, att->weapon))
    return;
  
  // Short-circuit: If you're wielding an activated Dominator, you don't care about all these pesky rules.
  if (att->weapon && GET_OBJ_SPEC(att->weapon) == weapon_dominator) {
    if (GET_LEVEL(def->ch) > GET_LEVEL(att->ch)) {
      send_to_char(att->ch, "As you aim your weapon at %s, a dispassionate feminine voice states: \"^cThe target's Crime Coefficient is below 100. %s is not a target for enforcement. The trigger has been locked.^n\"\r\n", GET_CHAR_NAME(def->ch), HSSH(def->ch));
      dominator_mode_switch(att->ch, att->weapon, DOMINATOR_MODE_DEACTIVATED);
      return;
    }
    switch (GET_WEAPON_ATTACK_TYPE(att->weapon)) {
      case WEAP_TASER:
        // Paralyzer? Stun your target.
        act("Your crackling shot of energy slams into $N, disabling $M.", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("A crackling shot of energy erupts from $n's Dominator and slams into $N, disabling $M!", FALSE, att->ch, 0, def->ch, TO_NOTVICT);
        act("A crackling shot of energy erupts from $n's Dominator and slams into you! Your vision fades as your muscles lock up.", FALSE, att->ch, 0, def->ch, TO_VICT);
        damage(att->ch, def->ch, 15, TYPE_SUFFERING, FALSE);
        break;
      case WEAP_HEAVY_PISTOL:
        // Lethal? Kill your target.
        act("A ball of coherent light leaps from your Dominator, tearing into $N. With a scream, $E crumples, bubbles, and explodes in a shower of gore!", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("A ball of coherent light leaps from $n's Dominator, tearing into $N. With a scream, $E crumples, bubbles, and explodes in a shower of gore!", FALSE, att->ch, 0, def->ch, TO_NOTVICT);
        act("A ball of coherent light leaps from $n's Dominator, tearing into you! A horrible rending sensation tears through you as your vision fades.", FALSE, att->ch, 0, def->ch, TO_VICT);
        die(def->ch);
        break;
      case WEAP_CANNON:
        // Decomposer? Don't just kill your target-- if they're a player, disconnect them.
        act("A roaring column of force explodes from your Dominator, erasing $N from existence!", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("A roaring column of force explodes from $n's Dominator, erasing $N from existence!", FALSE, att->ch, 0, def->ch, TO_NOTVICT);
        act("A roaring column of force explodes from $n's Dominator, erasing you from existence!", FALSE, att->ch, 0, def->ch, TO_VICT);
        die(def->ch);
        if (def->ch->desc) {
          STATE(def->ch->desc) = CON_CLOSE;
          close_socket(def->ch->desc);
        }
        break;
      default:
        send_to_char(att->ch, "Dominator code fault.\r\n");
        break;
    }
    return;
  }
  
  // Precondition: If your foe is astral, you don't belong here.
  if (IS_ASTRAL(def->ch)) {
    if (IS_DUAL(att->ch) || IS_ASTRAL(att->ch))
      astral_fight(att->ch, def->ch);
    return;
  }
  
  // Precondition: If you're in melee combat and your foe isn't present, stop fighting.
  if ((melee = !(att->weapon_is_gun))) {
    if (att->ch->in_room != def->ch->in_room) {
      send_to_char(att->ch, "You relax with the knowledge that your opponent is no longer present.\r\n");
      stop_fighting(att->ch);
      return;
    }
  }
  
  // Remove closing flags if both are melee.
  if (melee && !def->weapon_is_gun) {
    AFF_FLAGS(att->ch).RemoveBit(AFF_APPROACH);
    AFF_FLAGS(def->ch).RemoveBit(AFF_APPROACH);
  }
  
  // Setup: Height checks. Removed 'surprised ||' from the defender's check.
  att->too_tall = is_char_too_tall(att->ch);
  def->too_tall = is_char_too_tall(def->ch);
  
  // Setup: Calculate sight penalties.
  att->modifiers[COMBAT_MOD_VISIBILITY] += calculate_vision_penalty(att->ch, def->ch);
  def->modifiers[COMBAT_MOD_VISIBILITY] += calculate_vision_penalty(def->ch, att->ch);
  
  // Early execution: Nerve strike doesn't require as much setup, so perform it here to save on resources.
  if ((melee && IS_NERVE(att->ch)) 
      && !(att->weapon 
           || IS_SPIRIT(def->ch) 
           || IS_ELEMENTAL(def->ch) 
           || (IS_NPC(def->ch) && MOB_FLAGGED(def->ch, MOB_INANIMATE)))) {
    // Calculate and display pre-success-test information.
    snprintf(rbuf, sizeof(rbuf), "%s VS %s: Nerve Strike target is 4 + impact (%d) + modifiers: ",
             GET_CHAR_NAME(att->ch), 
             GET_CHAR_NAME(def->ch),
             GET_IMPACT(def->ch));
    
    att->tn += GET_IMPACT(def->ch) + modify_target_rbuf_raw(att->ch, rbuf, sizeof(rbuf), att->modifiers[COMBAT_MOD_VISIBILITY]);
    
    for (int mod_index = 0; mod_index < NUM_COMBAT_MODIFIERS; mod_index++) {
      buf_mod(rbuf, sizeof(rbuf), combat_modifiers[mod_index], att->modifiers[mod_index]);
      att->tn += att->modifiers[mod_index];
    }
    
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), ". Total TN is %d.", att->tn);
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    
    // Calculate the attacker's total skill and execute a success test.
    att->dice = get_skill(att->ch, SKILL_UNARMED_COMBAT, att->tn);
    if (!att->too_tall) {
      int bonus = MIN(GET_SKILL(att->ch, SKILL_UNARMED_COMBAT), GET_OFFENSE(att->ch));
      snprintf(rbuf, sizeof(rbuf), "Attacker is rolling %d + %d dice", att->dice, bonus);
      att->dice += bonus;
    } else {
      snprintf(rbuf, sizeof(rbuf), "Attacker is rolling %d dice (no bonus: too tall)", att->dice);
    }
    
    if (GET_QUI(def->ch) <= 0) {
      strncat(rbuf, "-- but we're zeroing out successes since the defender is paralyzed.", sizeof(rbuf) - strlen(rbuf) - 1);
      att->successes = 0;
    }
    else {
      att->successes = success_test(att->dice, att->tn);
      snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), ", and got %d successes, which translates to %d qui loss.", 
               att->successes,
               (int) (att->successes / 2));
    }
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
      
    if (att->successes > 1) {
      GET_TEMP_QUI_LOSS(def->ch) += (int) (att->successes / 2); // This used to be * 2!
      affect_total(def->ch);
      if (GET_QUI(def->ch) <= 0) {
        act("You hit $N's pressure points successfully, $e is paralyzed!", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("As $n hits you, you feel your body freeze up!", FALSE, att->ch, 0, def->ch, TO_VICT);
        act("$N freezes as $n's attack lands successfully!", FALSE, att->ch, 0, def->ch, TO_NOTVICT);
      } else {
        act("You hit $N's pressure points successfully, $e seems to slow down!", FALSE, att->ch, 0, def->ch, TO_CHAR);
        act("$n's blows seem to bring great pain and you find yourself moving slower!", FALSE, att->ch, 0, def->ch, TO_VICT);
        act("$n's attack hits $N, who seems to move slower afterwards.", FALSE, att->ch, 0, def->ch, TO_NOTVICT);
      }
    } else {
      act("You fail to hit any of the needed pressure points on $N.", FALSE, att->ch, 0, def->ch, TO_CHAR);
      act("$n fails to land any blows on you.", FALSE, att->ch, 0, def->ch, TO_VICT);
      act("$n's unarmed attack misses $N completely.", TRUE, att->ch, 0, def->ch, TO_NOTVICT);
    }
    return;
  }
  
  // Setup: Populate character cyberware.
  att = populate_cyberware(att->ch, att);
  def = populate_cyberware(def->ch, def);
  
  // Setup: Find their bayonet (if any).
  att->weapon_has_bayonet = weapon_has_bayonet(att->weapon);
  def->weapon_has_bayonet = weapon_has_bayonet(def->weapon);
  
  // Setup: Determine damage types.
  att->dam_type = get_weapon_damage_type(att->weapon);
  def->dam_type = get_weapon_damage_type(def->weapon);
  
  // Setup: Determine if we're doing physical damage.
  att->is_physical = IS_DAMTYPE_PHYSICAL(att->dam_type);
  def->is_physical = IS_DAMTYPE_PHYSICAL(def->dam_type);
  
  // Setup: If the character is rigging a vehicle or is in a vehicle, set veh to that vehicle.
  RIG_VEH(att->ch, att->veh);
  
  snprintf(rbuf, sizeof(rbuf), "%s: %s too tall, %sbayonet, damtype is %s (%s).\r\n%s: %s too tall, %sbayonet, damtype is %s (%s).\r\n",
           GET_CHAR_NAME(attacker),
           att->too_tall ? "is" : "not",
           att->weapon_has_bayonet ? "" : "!", 
           att->dam_type > TYPE_BLACKIC ? "weird" : damage_type_names_must_subtract_300_first_and_must_not_be_greater_than_blackic[att->dam_type - 300],
           att->is_physical ? "physical" : "mental",
           
           GET_CHAR_NAME(victim),
           def->too_tall ? "is" : "not",
           def->weapon_has_bayonet ? "" : "!", 
           def->dam_type > TYPE_BLACKIC ? "weird" : damage_type_names_must_subtract_300_first_and_must_not_be_greater_than_blackic[def->dam_type - 300],
           def->is_physical ? "physical" : "mental");
           
  act(rbuf, TRUE, attacker, NULL, NULL, TO_ROLLS);
  
  // Setup for ranged combat.
  if (!melee) {
    // Setup: Find the character's gyroscopic stabilizer (if any).
    if (!PLR_FLAGGED(att->ch, PLR_REMOTE) && !AFF_FLAGGED(att->ch, AFF_RIG) && !AFF_FLAGGED(att->ch, AFF_MANNING)) {
      for (int q = 0; q < NUM_WEARS; q++) {
        if (GET_EQ(att->ch, q) && GET_OBJ_TYPE(GET_EQ(att->ch, q)) == ITEM_GYRO) {
          att->gyro = GET_EQ(att->ch, q);
          break;
        }
      }
      
      // Precondition: If you're using a heavy weapon, you must be strong enough to wield it, or else be using a gyro. CC p99
      if (!att->gyro && !IS_NPC(att->ch)
          && (att->weapon_skill >= SKILL_MACHINE_GUNS && att->weapon_skill <= SKILL_ASSAULT_CANNON)
          && (GET_STR(att->ch) < 8 || GET_BOD(att->ch) < 8)
          && !(AFF_FLAGGED(att->ch, AFF_PRONE)))
      {
        send_to_char("You can't lift the barrel high enough to fire.\r\n", att->ch);
        return;
      }
    } else {
      // Gunnery modifiers.
    }
    
    // Setup: Cyclops suffer a +2 ranged-combat modifier.
    if (GET_RACE(att->ch) == RACE_CYCLOPS)
      att->modifiers[COMBAT_MOD_DISTANCE] += 2;
    
    // Setup: Determine the burst value of the weapon.
    if (IS_BURST(att->weapon))
      att->burst_count = 3;
    else if (IS_AUTO(att->weapon))
      att->burst_count = GET_OBJ_TIMER(att->weapon);
    
    // Setup: Limit the burst of the weapon to the available ammo, and decrement ammo appropriately.
    if (att->burst_count) {
      if (att->magazine) {
        // When we called has_ammo() earlier, we decremented their ammo by one. Give it back to true up the equation.
        GET_MAGAZINE_AMMO_COUNT(att->magazine)++;
        
        // Cap their burst to their magazine's ammo.
        att->burst_count = MIN(att->burst_count, GET_MAGAZINE_AMMO_COUNT(att->magazine));
        GET_MAGAZINE_AMMO_COUNT(att->magazine) -= att->burst_count;
      }
      
      // Setup: Compute recoil.
      att->recoil_comp = check_recoil(att->ch, att->weapon);
      
      // SR3 p151
      int att_burst_after_veh_mount_reduction = att->burst_count;
      if (PLR_FLAGGED(att->ch, PLR_REMOTE) || AFF_FLAGGED(att->ch, AFF_RIG) || AFF_FLAGGED(att->ch, AFF_MANNING))
        att_burst_after_veh_mount_reduction /= 2;
        
      att->modifiers[COMBAT_MOD_RECOIL] += MAX(0, att_burst_after_veh_mount_reduction - att->recoil_comp);
      switch (att->weapon_skill) {
        case SKILL_SHOTGUNS:
        case SKILL_MACHINE_GUNS:
        case SKILL_ASSAULT_CANNON:
          att->modifiers[COMBAT_MOD_RECOIL] *= 2;
      }
    }
    
    // Setup: Modify recoil based on vehicular stats.
    if (att->veh) {
      if (!(AFF_FLAGGED(att->ch, AFF_MANNING) || AFF_FLAGGED(att->ch, AFF_RIG) || PLR_FLAGGED(att->ch, PLR_REMOTE))) {
        // Core p152: Unmounted weapons get +2 TN.
        att->modifiers[COMBAT_MOD_MOVEMENT] += 2;
      } else {
        // TODO: Sensor stuff. It's a bitch to write due to all the various things that go into it, so it'll be done later.
        
        // Sensor tests per Core p152 and p135.
        
        // TN modifiers for sensor test per Core p154.
        
        // Success? You get half the sensor rating, rounded down, added to your dice.
      }
      
      // We assume all targets are standing still.
      // Per Core p153, movement gunnery modifier is +1 per 30m/CT.
      att->modifiers[COMBAT_MOD_MOVEMENT] += (int) (get_speed(att->veh) / 30);
      
      // Penalty for damaged veh.
      if ((att->veh)->damage > 0) {
        if ((att->veh)->damage <= 2)
          att->modifiers[COMBAT_MOD_VEHICLE_DAMAGE] += 1;
        else if ((att->veh)->damage <= 5)
          att->modifiers[COMBAT_MOD_VEHICLE_DAMAGE] += 2;
        else
          att->modifiers[COMBAT_MOD_VEHICLE_DAMAGE] += 3;
      }
    }
    
    // Setup: If you're dual-wielding, take that penalty, otherwise you get your smartlink bonus.
    if (GET_EQ(att->ch, WEAR_WIELD) && GET_EQ(att->ch, WEAR_HOLD))
      att->modifiers[COMBAT_MOD_DUAL_WIELDING] += 2;
    else
      att->modifiers[COMBAT_MOD_SMARTLINK] -= check_smartlink(att->ch, att->weapon);
    
    // Setup: Trying to fire a sniper rifle at close range is tricky. This is non-canon to reduce twinkery.
    if (IS_OBJ_STAT(att->weapon, ITEM_SNIPER) && att->ch->in_room == def->ch->in_room)
      att->modifiers[COMBAT_MOD_DISTANCE] += 6;
    
    // Setup: Compute modifiers to the TN based on the def->ch's current state.
    if (!AWAKE(def->ch))
      att->modifiers[COMBAT_MOD_POSITION] -= 6;
    else if (AFF_FLAGGED(def->ch, AFF_PRONE))
      att->modifiers[COMBAT_MOD_POSITION]--;
    
    // Setup: Determine distance penalties.
    if (!att->veh && att->ch->in_room != def->ch->in_room) {
      struct char_data *vict;
      bool vict_found = FALSE;
      struct room_data *room = NULL, *nextroom = NULL;
      
      int weapon_range;
      if (att->weapon && IS_RANGED(att->weapon)) {
        weapon_range = MIN(find_sight(att->ch), find_weapon_range(att->ch, att->weapon));
        for (int dir = 0; dir < NUM_OF_DIRS && !vict_found; dir++) {
          room = att->ch->in_room;
          if (CAN_GO2(room, dir))
            nextroom = EXIT2(room, dir)->to_room;
          else
            nextroom = NULL;
          for (int distance = 1; nextroom && (distance <= weapon_range) && !vict_found; distance++) {
            for (vict = nextroom->people; vict; vict = vict->next_in_room) {
              if (vict == def->ch) {
                att->modifiers[COMBAT_MOD_DISTANCE] += 2 * distance;
                vict_found = TRUE;
                break;
              }
            }
            room = nextroom;
            if (CAN_GO2(room, dir))
              nextroom = EXIT2(room, dir)->to_room;
            else
              nextroom = NULL;
          }
        }
      }
      if (!vict_found) {
        send_to_char(att->ch, "You squint around, but you can't find your opponent anywhere.\r\n");
        stop_fighting(att->ch);
        return;
      }
    }
    
    // Setup: If your attacker is closing the distance (running), take a penalty per Core p112.
    if (AFF_FLAGGED(def->ch, AFF_APPROACH))
      att->modifiers[COMBAT_MOD_DEFENDER_MOVING] += 2;
    
    // Setup: If you have a gyro mount, it negates recoil and movement penalties up to its rating.
    if (att->gyro && !(AFF_FLAGGED(att->ch, AFF_MANNING) || AFF_FLAGGED(att->ch, AFF_RIG) || AFF_FLAGGED(att->ch, AFF_PILOT)))
      att->modifiers[COMBAT_MOD_GYRO] -= MIN(att->modifiers[COMBAT_MOD_MOVEMENT] + att->modifiers[COMBAT_MOD_RECOIL], GET_OBJ_VAL(att->gyro, 0));
    else if (IS_NPC(att->ch)) {
      /*    We _used to_ assume NPCs had gyros, but this made them unreasonably powerful. -- LS
      // We assume that NPCs have a gyro equipped, and that the builder just didn't want to be handing out free gyros everywhere for balance reasons.
      att->modifiers[COMBAT_MOD_GYRO] -= MIN(att->modifiers[COMBAT_MOD_MOVEMENT] + att->modifiers[COMBAT_MOD_RECOIL], 4);
      */
    }
    
    // Calculate and display pre-success-test information.
    snprintf(rbuf, sizeof(rbuf), "%s's burst/compensation info is %d/%d. Additional modifiers: ", 
             GET_CHAR_NAME( att->ch ),
             att->burst_count, 
             att->recoil_comp);
             
    att->tn += modify_target_rbuf_raw(att->ch, rbuf, sizeof(rbuf), att->modifiers[COMBAT_MOD_VISIBILITY]);
    for (int mod_index = 0; mod_index < NUM_COMBAT_MODIFIERS; mod_index++) {
      buf_mod(rbuf, sizeof(rbuf), combat_modifiers[mod_index], att->modifiers[mod_index]);
      att->tn += att->modifiers[mod_index];
    }
    
    // Calculate the attacker's total skill (this modifies TN)
    att->dice = get_skill(att->ch, att->weapon_skill, att->tn);
    
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "\r\nThus, attacker's TN is: %d.", att->tn);
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    
    // Execute skill test
    if (!att->too_tall) {
      int bonus = MIN(GET_SKILL(att->ch, att->weapon_skill), GET_OFFENSE(att->ch));
      snprintf(rbuf, sizeof(rbuf), "Not too tall, so will roll %d + %d dice... ", att->dice, bonus);
      att->dice += bonus;
    } else {
      snprintf(rbuf, sizeof(rbuf), "Too tall, so will roll just %d dice... ", att->dice);
    }
    
    att->successes = success_test(att->dice, att->tn);
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "%d successes.", att->successes);
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    
    // Dodge test.
    if (AWAKE(def->ch) && !AFF_FLAGGED(def->ch, AFF_SURPRISE) && !def->too_tall && !AFF_FLAGGED(def->ch, AFF_PRONE)) {
      // Previous code only allowed you to sidestep if you had also allocated at least one normal dodge die. Why?
      def->dice = GET_DEFENSE(def->ch) + GET_POWER(def->ch, ADEPT_SIDESTEP);
      def->tn += damage_modifier(def->ch, buf, sizeof(buf)) + (int)(att->burst_count / 3) + def->footanchors;
      def->successes = MAX(success_test(def->dice, def->tn), 0);
      att->successes -= def->successes;
      
      snprintf(rbuf, sizeof(rbuf), "Dodge: Dice %d, TN %d, Successes %d.  This means attacker's net successes = %d.", def->dice, def->tn, def->successes, att->successes);
    } else {
      att->successes = MAX(att->successes, 1);
      snprintf(rbuf, sizeof(rbuf), "Opponent unable to dodge, successes confirmed as %d.", att->successes);
    }
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    
    // If the attack failed, print the relevant message and terminate.
    if (att->successes < 1) {
      snprintf(rbuf, sizeof(rbuf), "%s failed to roll any successes, so we're bailing out.", GET_CHAR_NAME(attacker));
      act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
      
      combat_message(att->ch, def->ch, att->weapon, -1, att->burst_count);
      damage(att->ch, def->ch, -1, att->dam_type, 0);
      return;
    }
    // But wait-- there's more! Next we jump down to dealing damage.
  }
  // Melee combat.
  else {
    // Decide what skills we'll be using for our tests.
    att->skill = get_melee_skill(att);
    def->skill = get_melee_skill(def);
    
    // Setup: Calculate position modifiers.
    if (AFF_FLAGGED(att->ch, AFF_PRONE))
      def->modifiers[COMBAT_MOD_POSITION] -= 2;
    if (AFF_FLAGGED(def->ch, AFF_PRONE))
      att->modifiers[COMBAT_MOD_POSITION] -= 2;
    
    // Spirits use different dice than the rest of us plebs.
    if (IS_SPIRIT(def->ch) || IS_ELEMENTAL(def->ch)) {
      att->dice = GET_WIL(att->ch);
      def->dice = GET_REA(def->ch);
    } else if (IS_SPIRIT(att->ch) || IS_ELEMENTAL(att->ch)) {
      att->dice = GET_REA(att->ch);
      def->dice = GET_WIL(def->ch);
    } else {
      act("Computing dice for attacker...", 1, att->ch, NULL, NULL, TO_ROLLS );
      att->dice = get_skill(att->ch, att->skill, att->tn) + (att->too_tall ? 0 : MIN(GET_SKILL(att->ch, att->skill), GET_OFFENSE(att->ch)));
      act("Computing dice for defender...", 1, att->ch, NULL, NULL, TO_ROLLS );
      def->dice = get_skill(def->ch, def->skill, def->tn) + (def->too_tall ? 0 : MIN(GET_SKILL(def->ch, def->skill), GET_OFFENSE(def->ch)));
    }
    
    // Adepts get bonus dice when counterattacking. Ip Man approves.
    if (GET_POWER(def->ch, ADEPT_COUNTERSTRIKE) > 0) {
      def->dice += GET_POWER(def->ch, ADEPT_COUNTERSTRIKE);
      snprintf(rbuf, sizeof(rbuf), "Defender counterstrike dice bonus is %d.", GET_POWER(def->ch, ADEPT_COUNTERSTRIKE));
      act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    }
    
    // Bugfix: If you're unconscious or mortally wounded, you don't get to counterattack.
    if (GET_PHYSICAL(def->ch) <= 0 || GET_MENTAL(def->ch) <= 0) {
      def->dice = 0;
      act("^yDefender incapped, dice capped to zero.^n", 1, att->ch, NULL, NULL, TO_ROLLS );
    }
      
      
    snprintf(rbuf, sizeof(rbuf), "^g%s's dice: ^W%d^g, %s's dice: ^W%d^g.^n", GET_CHAR_NAME(attacker), att->dice, GET_CHAR_NAME(victim), def->dice);
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    
    // Calculate the net reach.
    int net_reach = GET_REACH(att->ch) - GET_REACH(def->ch);
    
    if (!GET_POWER(att->ch, ADEPT_PENETRATINGSTRIKE) && GET_POWER(att->ch, ADEPT_DISTANCE_STRIKE)) {
      // MitS 149: Ignore reach modifiers.
      net_reach = 0;
    }
    
    // Reach is always used offensively. TODO: Add option to use it defensively instead.
    if (net_reach > 0)
      att->modifiers[COMBAT_MOD_REACH] -= net_reach;
    else
      def->modifiers[COMBAT_MOD_REACH] -= -net_reach;
      
    // -------------------------------------------------------------------------------------------------------
    // Calculate and display pre-success-test information.
    snprintf(rbuf, sizeof(rbuf), "^cMelee combat mode. %s's TN modifiers: ", GET_CHAR_NAME(att->ch) );
    att->tn += modify_target_rbuf_raw(att->ch, rbuf, sizeof(rbuf), att->modifiers[COMBAT_MOD_VISIBILITY]);
    for (int mod_index = 0; mod_index < NUM_COMBAT_MODIFIERS; mod_index++) {
      buf_mod(rbuf, sizeof(rbuf), combat_modifiers[mod_index], att->modifiers[mod_index]);
      att->tn += att->modifiers[mod_index];
    }
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    
    snprintf(rbuf, sizeof(rbuf), "^c%s%s's TN modifiers: ", GET_CHAR_NAME( def->ch ),
            (GET_PHYSICAL(def->ch) <= 0 || GET_MENTAL(def->ch) <= 0) ? " (incap)" : "" );
    def->tn += modify_target_rbuf_raw(def->ch, rbuf, sizeof(rbuf), def->modifiers[COMBAT_MOD_VISIBILITY]);
    for (int mod_index = 0; mod_index < NUM_COMBAT_MODIFIERS; mod_index++) {
      buf_mod(rbuf, sizeof(rbuf), combat_modifiers[mod_index], def->modifiers[mod_index]);
      def->tn += def->modifiers[mod_index];
    }
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    def->tn = MAX(def->tn, 2);

    // ----------------------
    
    int net_successes;
    if (AWAKE(def->ch) && !AFF_FLAGGED(def->ch, AFF_SURPRISE)) {
      att->successes = success_test(att->dice, att->tn);
      def->successes = success_test(def->dice, def->tn);
      net_successes = att->successes - def->successes;
    } else {
      att->successes = MAX(1, success_test(att->dice, att->tn));
      net_successes = att->successes;
    }
    
    if (def->weapon ? (GET_OBJ_TYPE(def->weapon) != ITEM_WEAPON && GET_OBJ_TYPE(def->weapon) != ITEM_FIREWEAPON) : FALSE) {
      // Defender's wielding a non-weapon? Whoops, net successes will never be less than 0.
      net_successes = MAX(0, net_successes);
    }
    
    snprintf(rbuf, sizeof(rbuf), "^g%s got ^W%d^g success%s from ^W%d^g dice at TN ^W%d^g.^n\r\n",
             GET_CHAR_NAME(att->ch),
             att->successes,
             att->successes != 1 ? "s" : "",
             att->dice,
             att->tn);
    snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "^g%s got ^W%d^g success%s from ^W%d^g dice at TN ^W%d^g.^n\r\n",
             GET_CHAR_NAME(def->ch),
             def->successes,
             def->successes != 1 ? "s" : "",
             def->dice,
             def->tn);
    if (net_successes < 0)
      snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "^yNet successes is ^W%d^y, which will cause a counterattack.^n\r\n", net_successes);
    else
      snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), "Net successes is ^W%d^n.\r\n", net_successes);
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    
    act("$n clashes with $N in melee combat.", FALSE, att->ch, 0, def->ch, TO_ROOM);
    act("You clash with $N in melee combat.", FALSE, att->ch, 0, def->ch, TO_CHAR);
    
    // If your enemy got more successes than you, guess what? You're the one who gets their face caved in.
    if (net_successes < 0) {
      if (!GET_POWER(att->ch, ADEPT_PENETRATINGSTRIKE) && GET_POWER(att->ch, ADEPT_DISTANCE_STRIKE)) {
        // MitS 149: You cannot be counterstriked while using distance strike.
        net_successes = 0;
      }
      // This messaging gets a little annoying.
      act("You successfully counter $N's attack!", FALSE, def->ch, 0, att->ch, TO_CHAR);
      act("$n deflects your attack and counterstrikes!", FALSE, def->ch, 0, att->ch, TO_VICT);
      act("$n deflects $N's attack and counterstrikes!", FALSE, def->ch, 0, att->ch, TO_NOTVICT);
      struct combat_data *temp_att = att;
      att = def;
      def = temp_att;
      
      att->successes = -1 * net_successes;
    } else
      att->successes = net_successes;
  }
  
  // Calculate the power of the attack (applies to both melee and ranged).
  if (att->weapon) {
    // Ranged weapon?
    if (att->weapon_is_gun) {
      att->power = GET_WEAPON_POWER(att->weapon) + att->burst_count;
      att->damage_level = GET_WEAPON_DAMAGE_CODE(att->weapon) + (int)(att->burst_count / 3);
      
      // Calculate effects of armor on the power of the attack.
      if (att->magazine) {
        switch (GET_MAGAZINE_AMMO_TYPE(att->magazine)) {
          case AMMO_APDS:
            att->power -= (int)(GET_BALLISTIC(def->ch) / 2);
            break;
          case AMMO_EX:
            att->power++;
            // fall through
          case AMMO_EXPLOSIVE:
            att->power++;
            att->power -= GET_BALLISTIC(def->ch);
            break;
          case AMMO_FLECHETTE:
            if (!GET_IMPACT(def->ch) && !GET_BALLISTIC(def->ch))
              att->damage_level++;
            else {
              att->power -= MAX(GET_BALLISTIC(def->ch), GET_IMPACT(def->ch) * 2);
            }
            break;
          case AMMO_GEL:
            att->power -= GET_BALLISTIC(def->ch) + 2;
            break;
          default:
            att->power -= GET_BALLISTIC(def->ch);
        }
      }
      // Weapon fired without a magazine (probably by an NPC)-- we assume its ammo type is normal.
      else {
        att->power -= GET_BALLISTIC(def->ch);
      }
      
      // Increment character's shots_fired.
      if (GET_SKILL(att->ch, att->weapon_skill) >= 8 && !SHOTS_TRIGGERED(att->ch))
        SHOTS_FIRED(att->ch)++;
    }
    // Melee weapon.
    else {
      att->power = GET_WEAPON_STR_BONUS(att->weapon) + GET_STR(att->ch);
      att->power -= GET_IMPACT(def->ch);
    }
    
    // Core p113.
    if (att->power < 2)
      att->power = 2;
  }
  // Cyber and unarmed combat.
  else {
    // Base power for melee attacks is character's strength.
    att->power = GET_STR(att->ch);
    att->damage_level = MODERATE;
    att->dam_type = TYPE_HIT;
    att->is_physical = FALSE;
    
    // Check for active cyberweapons.
    if (att->num_cyberweapons > 0) {
      if (att->num_cyberweapons >= 2) {
        // Dual cyberweapons gives a power bonus per Core p121.
        att->power += (int) (GET_STR(att->ch) / 2);
      }
      
      // Select the best cyberweapon and use its stats.
      if (att->handspurs) {
        att->damage_level = MODERATE;
        att->dam_type = TYPE_SLASH;
        att->is_physical = TRUE;
      }
      else if (att->handblades) {
        att->power += 3;
        att->damage_level = LIGHT;
        att->dam_type = TYPE_STAB;
        att->is_physical = TRUE;
      }
      else if (att->improved_handrazors) {
        att->power += 2;
        att->damage_level = LIGHT;
        att->dam_type = TYPE_STAB;
        att->is_physical = TRUE;
      }
      else if (att->handrazors) {
        att->damage_level = LIGHT;
        att->dam_type = TYPE_STAB;
        att->is_physical = TRUE;
      }
      else if (att->fins || att->climbingclaws) {
        att->power -= 1;
        att->damage_level = LIGHT;
        att->dam_type = TYPE_SLASH;
        att->is_physical = TRUE;
      }
      else if (att->footanchors) {
        att->power -= 1;
        att->damage_level = LIGHT;
        att->dam_type = TYPE_STAB;
        att->is_physical = TRUE;
      }
      else {
        snprintf(buf, sizeof(buf), "SYSERR in hit(): num_cyberweapons %d but no weapons found.", att->num_cyberweapons);
        log(buf);
      }
    }
    // No cyberweapons active-- check for bone lacing, then proceed with adept/standard slapping.
    else {
      // TODO: Implement option to use bone lacing to cause physical damage at half power.
      if (att->bone_lacing_power) {
        att->power += att->bone_lacing_power;
        att->damage_level = MODERATE;
        att->is_physical = FALSE;
      }
      
      // Check for Adept powers.
      // TODO: Elemental Strike (SotA64 p65)
      
      // Apply Killing Hands.
      if (GET_POWER(att->ch, ADEPT_KILLING_HANDS)) {
        att->damage_level = GET_POWER(att->ch, ADEPT_KILLING_HANDS);
        att->is_physical = TRUE;
      }
      
      // Apply impact armor reduction to attack power.
      if (GET_POWER(att->ch, ADEPT_PENETRATINGSTRIKE) && !GET_POWER(att->ch, ADEPT_DISTANCE_STRIKE))
        att->power -= MAX(0, GET_IMPACT(def->ch) - GET_POWER(att->ch, ADEPT_PENETRATINGSTRIKE));
      else
        att->power -= GET_IMPACT(def->ch);
    }
  }
  
  // Handle spirits and elementals being little divas with their special combat rules.
  if (IS_SPIRIT(def->ch) || IS_ELEMENTAL(def->ch)) {
    if (att->power <= GET_LEVEL(def->ch) * 2) {
      if (!melee)
        combat_message(att->ch, def->ch, att->weapon, 0, att->burst_count);
      else
        damage(att->ch, def->ch, 0, att->dam_type, att->is_physical);
      return;
    }
    else
      att->power -= GET_LEVEL(def->ch) * 2;
    att->power = MAX(2, att->power);
  }
  
  // Perform body test for damage resistance.
  int bod_success=0, bod = GET_BOD(def->ch) + (def->too_tall ? 0 : GET_BODY(def->ch));
  if (IS_SPIRIT(att->ch) && GET_MAG(def->ch) > 0 && GET_SKILL(def->ch, SKILL_CONJURING))
    bod_success = success_test(MAX(bod, GET_SKILL(def->ch, SKILL_CONJURING)), att->power);
  else if (!AWAKE(def->ch))
    bod_success = 0;
  else
    bod_success = success_test(bod, att->power);
  
  att->successes -= bod_success;
  
  int staged_damage = 0;
  
  // Adjust messaging for unkillable entities.
  if (can_hurt(att->ch, def->ch, att->dam_type, TRUE))
    staged_damage = stage(att->successes, att->damage_level);
  else {
    staged_damage = -1;
    act("Damage reduced to -1 due to failure of CAN_HURT().", TRUE, att->ch, NULL, NULL, TO_ROLLS);
  }
  
  int damage_total = convert_damage(staged_damage);
  
  snprintf(rbuf, sizeof(rbuf), "^CBod dice %d, attack power %d, BodSuc %d, ResSuc %d: Dam %s->%s. %d%c.^n",
          bod, att->power, bod_success, att->successes,
          wound_name[MIN(DEADLY, MAX(0, att->damage_level))],
          wound_name[MIN(DEADLY, MAX(0, damage_total))],
          damage_total, att->is_physical ? 'P' : 'M');
  act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
  
  if (!melee)
    combat_message(att->ch, def->ch, att->weapon, MAX(0, damage_total), att->burst_count);
  
  damage(att->ch, def->ch, damage_total, att->dam_type, att->is_physical);
  
  if (!IS_NPC(att->ch) && IS_NPC(def->ch)) {
    GET_LASTHIT(def->ch) = GET_IDNUM(att->ch);
  }
  
  // If you're firing a heavy weapon without a gyro, you need to test against the damage of the recoil.
  if (att->weapon 
      && !IS_NPC(att->ch) 
      && !att->gyro 
      && att->weapon_skill >= SKILL_MACHINE_GUNS 
      && att->weapon_skill <= SKILL_ASSAULT_CANNON
      && !(PLR_FLAGGED(att->ch, PLR_REMOTE) 
           || AFF_FLAGGED(att->ch, AFF_RIG) 
           || AFF_FLAGGED(att->ch, AFF_MANNING)))
  {
    int recoil_successes = success_test(GET_BOD(att->ch) + GET_BODY(att->ch), GET_WEAPON_POWER(att->weapon) / 2 + modify_target(att->ch));
    int staged_dam = stage(-recoil_successes, LIGHT);
    snprintf(rbuf, sizeof(rbuf), "Heavy Recoil: %d successes, L->%s wound.", recoil_successes, staged_dam == LIGHT ? "L" : "no");
    act( rbuf, 1, att->ch, NULL, NULL, TO_ROLLS );
    damage(att->ch, att->ch, convert_damage(staged_dam), TYPE_HIT, FALSE);
  }
  
  // Set the violence background count.
  struct room_data *room = get_ch_in_room(att->ch);
  if (room && !GET_BACKGROUND_COUNT(room)) {
    GET_SETTABLE_BACKGROUND_COUNT(room) = 1;
    GET_SETTABLE_BACKGROUND_AURA(room) = AURA_PLAYERCOMBAT;
  }
  
}

int find_sight(struct char_data *ch)
{
  int sight;
  
  if ((!IS_NPC(ch) && access_level(ch, LVL_VICEPRES)) || AFF_FLAGGED(ch, AFF_VISION_MAG_3))
    sight = 4;
  else if (AFF_FLAGGED(ch, AFF_VISION_MAG_2))
    sight = 3;
  else if (AFF_FLAGGED(ch, AFF_VISION_MAG_1))
    sight = 2;
  else
    sight = 1;
  
  /* add more weather conditions here to affect scan */
  if (SECT(get_ch_in_room(ch)) != SPIRIT_HEARTH && (IS_NPC(ch) || !access_level(ch, LVL_VICEPRES)))
    switch (weather_info.sky)
  {
    case SKY_RAINING:
      sight -= 1;
      break;
    case SKY_LIGHTNING:
      sight -= 2;
      break;
  }
  
  sight = MIN(4, MAX(1, sight));
  
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
      || !vict->in_room
      || (IS_NPC(vict) && (MOB_FLAGGED(vict, MOB_INANIMATE)))
      || CH_IN_COMBAT(vict)) {
    return FALSE;
  }
  
  if (GET_POS(vict) < POS_FIGHTING)
    GET_POS(vict) = POS_STANDING;
  if (!AFF_FLAGGED(vict, AFF_PRONE) && (IS_NPC(vict) && MOB_FLAGGED(vict, MOB_WIMPY) && !MOB_FLAGGED(vict, MOB_SENTINEL))) {
    do_flee(vict, "", 0, 0);
  } else if (RANGE_OK(vict)) {
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
        for (temp = nextroom->people; !is_responding && temp; temp = temp->next_in_room) {
          if (temp == ch && (distance > range || distance > sight) && IS_NPC(vict) && !MOB_FLAGGED(vict, MOB_SENTINEL)) {
            is_responding = TRUE;
            act("$n charges towards $s distant foe.", TRUE, vict, 0, 0, TO_ROOM);
            act("You charge after $N.", FALSE, vict, 0, ch, TO_CHAR);
            char_from_room(vict);
            char_to_room(vict, EXIT2(room, dir)->to_room);
            if (vict->in_room == ch->in_room) {
              act("$n arrives in a rush of fury, immediately attacking $N!", TRUE, vict, 0, ch, TO_NOTVICT);
              act("$n arrives in a rush of fury, rushing straight towards you!", TRUE, vict, 0, ch, TO_VICT);
            }
          }
        }
        room = nextroom;
        if (CAN_GO2(room, dir))
          nextroom = EXIT2(room, dir)->to_room;
        else
          nextroom = NULL;
      }
    }
    is_responding = TRUE;
    set_fighting(vict, ch);
  } else if (IS_NPC(vict) && !MOB_FLAGGED(vict, MOB_SENTINEL)) {
    for (dir = 0; dir < NUM_OF_DIRS && !is_responding; dir++) {
      if (CAN_GO(vict, dir) && EXIT2(vict->in_room, dir)->to_room == ch->in_room) {
        is_responding = TRUE;
        act("$n charges towards $s distant foe.", TRUE, vict, 0, 0, TO_ROOM);
        act("You charge after $N.", FALSE, vict, 0, ch, TO_CHAR);
        char_from_room(vict);
        char_to_room(vict, ch->in_room);
        set_fighting(vict, ch);
        act("$n arrives in a rush of fury, immediately attacking $N!", TRUE, vict, 0, ch, TO_NOTVICT);
        act("$n arrives in a rush of fury, rushing straight towards you!", TRUE, vict, 0, ch, TO_VICT);
      }
    }
  }
  return is_responding;
}

void explode(struct char_data *ch, struct obj_data *weapon, struct room_data *room)
{
  int damage_total, i, power, level;
  struct char_data *victim, *next_vict;
  struct obj_data *obj, *next;
  
  power = GET_WEAPON_POWER(weapon);
  level = GET_WEAPON_DAMAGE_CODE(weapon);
  
  extract_obj(weapon);
  
  if (room->people)
  {
    act("The room is lit by an explosion!", FALSE, room->people, 0, 0, TO_ROOM);
    act("The room is lit by an explosion!", FALSE, room->people, 0, 0, TO_CHAR);
  }
  
  for (obj = room->contents; obj; obj = next)
  {
    next = obj->next_content;
    damage_obj(NULL, obj, level * 2 + (int)(power / 6), DAMOBJ_EXPLODE);
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
  
  if ((ch->char_specials.rigging ? ch->char_specials.rigging->in_room : ch->in_room)->peaceful)
  {
    send_to_char("This room just has a peaceful, easy feeling...\r\n", ch);
    return;
  }
  
  sight = find_sight(ch);
  
  if (CAN_GO2(ch->in_room, dir))
    nextroom = EXIT2(ch->in_room, dir)->to_room;
  else
    nextroom = NULL;
  
  if (GET_OBJ_TYPE(weapon) == ITEM_WEAPON && GET_WEAPON_ATTACK_TYPE(weapon) == TYPE_HAND_GRENADE)
  {
    if (!nextroom) {
      send_to_char("There seems to be something in the way...\r\n", ch);
      return;
    }
    if (nextroom->peaceful) {
      send_to_char("Nah - leave them in peace.\r\n", ch);
      return;
    }
    if (CH_IN_COMBAT(ch))
      stop_fighting(ch);
    act("You pull the pin and throw $p!", FALSE, ch, weapon, 0, TO_CHAR);
    act("$n pulls the pin and throws $p!", FALSE, ch, weapon, 0, TO_ROOM);
    
    WAIT_STATE(ch, PULSE_VIOLENCE);
    
    temp = MAX(1, GET_SKILL(ch, SKILL_THROWING_WEAPONS));
    
    if (!number(0, 2)) {
      snprintf(buf, sizeof(buf), "A defective grenade lands on the floor.\r\n");
      send_to_room(buf, nextroom);
      return;
    } else if (!(success_test(temp+GET_OFFENSE(ch), 5) < 2)) {
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
          explode(ch, weapon, scatter[temp2]);
          return;
        }
      }
    }
    explode(ch, weapon, nextroom);
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
    } else if (vict->in_room->peaceful) {
      send_to_char("Nah - leave them in peace.\r\n", ch);
      return;
    } else if (distance > range) {
      act("$N seems to be out of $p's range.", FALSE, ch, weapon, vict, TO_CHAR);
      return;
    } else if (mob_index[GET_MOB_RNUM(vict)].func == shop_keeper 
               || mob_index[GET_MOB_RNUM(vict)].sfunc == shop_keeper
               || mob_index[GET_MOB_RNUM(vict)].func == johnson 
               || mob_index[GET_MOB_RNUM(vict)].sfunc == johnson) {
      send_to_char("Maybe that's not such a good idea.\r\n", ch);
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
          (GET_EQ(vict, WEAR_WIELD) ? GET_EQ(vict, WEAR_WIELD) : GET_EQ(vict, WEAR_HOLD))
          );
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
            (GET_EQ(vict, WEAR_WIELD) ? GET_EQ(vict, WEAR_WIELD) : GET_EQ(vict, WEAR_HOLD))
            );
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
      } else if (!success_test(temp + GET_OFFENSE(ch), 6)) {
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
        (PRF_FLAGGED(ch, PRF_HOLYLIGHT) || CURRENT_VISION(ch) == THERMOGRAPHIC ||
         light_level(nextroom) <= LIGHT_NORMALNOLIT ||
         ((light_level(nextroom) == LIGHT_MINLIGHT || light_level(nextroom) == LIGHT_PARTLIGHT) &&
          CURRENT_VISION(ch) == LOWLIGHT))) {
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
  if (AFF_FLAGGED(ch, AFF_SURPRISE))
    AFF_FLAGS(ch).RemoveBit(AFF_SURPRISE);
  if (AWAKE(ch))
  {
    if (AFF_FLAGGED(ch, AFF_PILOT))
      GET_INIT_ROLL(ch) = dice(1, 6) + GET_REA(ch);
    else
      GET_INIT_ROLL(ch) = dice(1 + GET_INIT_DICE(ch), 6) + GET_REA(ch);
    GET_INIT_ROLL(ch) -= damage_modifier(ch, buf, sizeof(buf));
    if (AFF_FLAGGED(ch, AFF_ACTION)) {
      GET_INIT_ROLL(ch) -= 10;
      AFF_FLAGS(ch).RemoveBit(AFF_ACTION);
    }
    if (IS_SPIRIT(ch) || IS_ELEMENTAL(ch)) {
      if (IS_DUAL(ch))
        GET_INIT_ROLL(ch) += 10;
      else
        GET_INIT_ROLL(ch) += 20;
    }
    if (IS_NPC(ch)) {
      if (GET_MOBALERT(ch) == MALERT_CALM && FIGHTING(ch) && success_test(GET_REA(ch), 4) < success_test(GET_REA(FIGHTING(ch)), 4)) {
        GET_INIT_ROLL(ch) = 0;
        act("You surprise $n!", TRUE, ch, 0, FIGHTING(ch), TO_VICT);
        AFF_FLAGS(ch).SetBit(AFF_SURPRISE);
      }
      GET_MOBALERT(ch) = MALERT_ALARM;
      GET_MOBALERTTIME(ch) = 30;
    }
  }
  char rbuf[MAX_STRING_LENGTH];
  snprintf(rbuf, sizeof(rbuf),"Init: %2d %s",
          GET_INIT_ROLL(ch), GET_NAME(ch));
  act(rbuf,TRUE,ch,NULL,NULL,TO_ROLLS);
  if (AFF_FLAGGED(ch, AFF_ACID))
    AFF_FLAGS(ch).RemoveBit(AFF_ACID);
}

void decide_combat_pool(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct char_data *ch;
  
  for (ch = combat_list; ch; ch = ch->next_fighting) {
    if (ch->bioware)
      check_adrenaline(ch, 0);
    
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

/* control the fights going on.  Called every 2 seconds from comm.c. */
void perform_violence(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct char_data *ch = NULL;
  extern struct index_data *mob_index;
  if (combat_list) {
    if (GET_INIT_ROLL(combat_list) <= 0) {
      roll_initiative();
      order_list(TRUE);
    }
  }
  for (ch = combat_list; ch; ch = next_combat_list) {
    bool engulfed = FALSE;
    next_combat_list = ch->next_fighting;
    
    // You're not in combat or not awake.
    if (!CH_IN_COMBAT(ch) || !AWAKE(ch)) {
      stop_fighting(ch);
      continue;
    }
  
    // You get no action if you're out of init.
    if (GET_INIT_ROLL(ch) <= 0)
      continue;
    
    // Knock down their init.
    GET_INIT_ROLL(ch) -= 10;
    
    // Process spirit attacks.
    for (struct spirit_sustained *ssust = SPIRIT_SUST(ch); ssust; ssust = ssust->next) {
      if (ssust->type == ENGULF) {
        if (ssust->caster) {
          int dam;
          if (IS_SPIRIT(ch) || (IS_ELEMENTAL(ch) && GET_SPARE1(ch) == ELEM_WATER)) {
            dam = convert_damage(stage(-success_test(GET_BOD(ssust->target), GET_SPARE2(ch) + GET_EXTRA(ssust->target)), MODERATE));
            act("$n can contorts in pain as the water engulfs $m!", TRUE, ssust->target, 0, 0, TO_ROOM);
            send_to_char("The water crushes you and leaves you unable to breath!\r\n", ssust->target);
            if (dam > 0)
              damage(ch, ssust->target, dam, TYPE_FUMES, MENTAL);
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
    
    // On fire and panicking, or engulfed? Lose your action.
    if ((ch->points.fire[0] > 0 && success_test(GET_WIL(ch), 6) < 0) 
        || engulfed)
      continue;
      
    // Process banishment actions.
    if (IS_AFFECTED(ch, AFF_BANISH) 
        && FIGHTING(ch) 
        && (IS_ELEMENTAL(FIGHTING(ch)) 
            || IS_SPIRIT(FIGHTING(ch)))) {
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
        send_to_char(mage, "You lock minds with %s but are beaten back by its force!\r\n", GET_NAME(spirit));
      } else if (success == 0) {
        send_to_char(mage, "You lock minds with %s but fail to gain any ground.\r\n",
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
        damage(mage, spirit, TYPE_DRAIN, DEADLY, MENTAL);
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
          && success_test(1, 8 - GET_SKILL(ch, SKILL_SORCERY))) {
        mob_magic(ch);
        continue;
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
      // Many failure conditions for charging at someone.
      if (IS_AFFECTED(FIGHTING(ch), AFF_APPROACH) 
          || GET_POS(FIGHTING(ch)) < POS_FIGHTING 
          || (GET_EQ(ch, WEAR_WIELD) 
              && (IS_GUN(GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3)) 
                  || GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) == TYPE_ARROW)) 
          || (GET_EQ(ch, WEAR_HOLD) 
              && (IS_GUN(GET_OBJ_VAL(GET_EQ(ch, WEAR_HOLD), 3)) 
                  || GET_OBJ_VAL(GET_EQ(ch, WEAR_HOLD), 3) == TYPE_ARROW))
          || (GET_EQ(FIGHTING(ch), WEAR_WIELD) 
              && !(IS_GUN(GET_WEAPON_ATTACK_TYPE(GET_EQ(FIGHTING(ch), WEAR_WIELD))))) 
          || (GET_EQ(FIGHTING(ch), WEAR_HOLD) 
              && !(IS_GUN(GET_WEAPON_ATTACK_TYPE(GET_EQ(FIGHTING(ch), WEAR_HOLD))))) 
          || ch->in_room != FIGHTING(ch)->in_room) {
        AFF_FLAGS(ch).RemoveBit(AFF_APPROACH);
        AFF_FLAGS(FIGHTING(ch)).RemoveBit(AFF_APPROACH);
      } 
      
      // Otherwise, process the charge.
      else {
        int target = 8, quickness = GET_QUI(ch);
        bool footanchor = FALSE;
        
        // Armor penalties.
        if (GET_TOTALBAL(ch) > GET_QUI(ch))
          quickness -= GET_TOTALBAL(ch) - GET_QUI(ch);
        if (GET_TOTALIMP(ch) > GET_QUI(ch))
          quickness -= GET_TOTALIMP(ch) - GET_QUI(ch);
          
        // Distance Strike (only works unarmed)
        if (GET_TRADITION(ch) == TRAD_ADEPT 
            && GET_POWER(ch, ADEPT_DISTANCE_STRIKE) 
            && !(GET_EQ(ch, WEAR_WIELD) 
                 || GET_EQ(ch, WEAR_HOLD)))
          target -= (int)GET_REAL_MAG(ch) / 150;
        
        // Hydraulic jack and foot anchor.
        for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content) {
          if (GET_OBJ_VAL(cyber, 0) == CYB_HYDRAULICJACK)
            quickness += GET_OBJ_VAL(cyber, 1);
          else if (GET_OBJ_VAL(cyber, 0) == CYB_FOOTANCHOR && !GET_OBJ_VAL(cyber, 9))
            footanchor = TRUE;
        }
            
        // Movement modifications via spells.
        for (struct spirit_sustained *ssust = SPIRIT_SUST(ch); ssust; ssust = ssust->next)
          if (ssust->type == MOVEMENTUP && ssust->caster == FALSE && GET_LEVEL(ssust->target))
            quickness *= GET_LEVEL(ssust->target);
          else if (ssust->type == MOVEMENTDOWN && ssust->caster == FALSE && GET_LEVEL(ssust->target))
            quickness /= GET_LEVEL(ssust->target);
            
        // Movement reset: Can't move if binding.
        if (AFF_FLAGGED(ch, AFF_BINDING))
          quickness = 0;
          
        // Penalty from footanchor.
        if (footanchor)
          quickness /= 2;
          
        // Penalty from too-tall.
#ifdef USE_SLOUCH_RULES
        if (ROOM_FLAGGED(ch->in_room, ROOM_INDOORS)) {
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

        // Strike.
        if (quickness > 0 && success_test(quickness, target) > 1) {
          send_to_char(ch, "You close the distance and strike!\r\n");
          act("$n closes the distance and strikes.", FALSE, ch, 0, 0, TO_ROOM);
          AFF_FLAGS(ch).RemoveBit(AFF_APPROACH);
          AFF_FLAGS(FIGHTING(ch)).RemoveBit(AFF_APPROACH);
        } else {
          send_to_char(ch, "You attempt to close the distance!\r\n");
          act("$n charges towards $N.", FALSE, ch, 0, FIGHTING(ch), TO_ROOM);
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
        send_to_char("Your target has left your line of sight.\r\n", ch);
        stop_fighting(ch);
        continue;
      }
      mount_fire(ch);
    }
    
    // Attacking a vehicle. Stopped here.
    else if (FIGHTING_VEH(ch)) {
      if (ch->in_room != FIGHTING_VEH(ch)->in_room)
        stop_fighting(ch);
      else
        vcombat(ch, FIGHTING_VEH(ch));
    } else {
      hit(ch,
          FIGHTING(ch),
          GET_EQ(ch, WEAR_WIELD) ? GET_EQ(ch, WEAR_WIELD) : GET_EQ(ch, WEAR_HOLD),
          GET_EQ(FIGHTING(ch), WEAR_WIELD) ? GET_EQ(FIGHTING(ch), WEAR_WIELD) : GET_EQ(FIGHTING(ch), WEAR_HOLD)
          );
      if (GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD) && FIGHTING(ch))
        hit(ch,
            FIGHTING(ch),
            GET_EQ(ch, WEAR_HOLD),
            GET_EQ(FIGHTING(ch), WEAR_WIELD) ? GET_EQ(FIGHTING(ch), WEAR_WIELD) : GET_EQ(FIGHTING(ch), WEAR_HOLD)
            );
    }
    
    if (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_SPEC) &&
        mob_index[GET_MOB_RNUM(ch)].func != NULL) {
      char empty_argument = '\0';
      (mob_index[GET_MOB_RNUM(ch)].func) (ch, ch, 0, &empty_argument);
    }
  }
}

void order_list(bool first, ...)
{
  struct char_data *one, *two = combat_list, *next, *previous = NULL, *temp;
  
  if (combat_list == NULL)
    return;
  
  for (one = two; one; previous = NULL, one = next) {
    next = one->next_fighting;
    for (two = combat_list; two && two->next_fighting; previous = two,
         two = two->next_fighting) {
      if (GET_INIT_ROLL(two->next_fighting) > GET_INIT_ROLL(two)) {
        if (previous)
          previous->next_fighting = two->next_fighting;
        else
          combat_list = two->next_fighting;
        temp = two->next_fighting->next_fighting;
        two->next_fighting->next_fighting = two;
        two->next_fighting = temp;
      }
    }
  }
  
  if (first)
    for (one = combat_list; one; one = next) {
      next = one->next_fighting;
      two = one;
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
    send_to_veh("The wind screen shatters and the bumper falls off.\r\n", veh, NULL, TRUE);
  } else if (veh->damage < VEH_DAM_THRESHOLD_DESTROYED) {
    send_to_veh("The engine starts spewing smoke and flames.\r\n", veh, NULL, TRUE);
  } else {
    struct char_data *i, *next;
    struct obj_data *obj, *nextobj;
    int damage_rating, damage_tn;
    
    
    if (veh->cspeed >= SPEED_IDLE) {
      send_to_veh("You are hurled into the street as your ride is wrecked!\r\n", veh, NULL, FALSE);
      snprintf(buf, sizeof(buf), "%s careens off the road, its occupants hurled to the street!\r\n", GET_VEH_NAME(veh));
      send_to_room(buf, veh->in_room);
      
      damage_rating = SERIOUS;
      damage_tn = 8;
    } else {
      send_to_veh("You scramble into the street as your ride is wrecked!\r\n", veh, NULL, FALSE);
      
      if (veh->people) {
        snprintf(buf, sizeof(buf), "%s's occupants scramble to safety as it is wrecked!\r\n", GET_VEH_NAME(veh));
        send_to_room(buf, veh->in_room);
      }
      
      damage_rating = MODERATE;
      damage_tn = 4;
    }
    
    if (veh->rigger) {
      send_to_char("Your mind is blasted with pain as your vehicle is wrecked.\r\n", veh->rigger);
      damage(veh->rigger, veh->rigger, convert_damage(stage(-success_test(GET_WIL(veh->rigger), 6), SERIOUS)), TYPE_CRASH, MENTAL);
      veh->rigger->char_specials.rigging = NULL;
      PLR_FLAGS(veh->rigger).RemoveBit(PLR_REMOTE);
      veh->rigger = NULL;
    }
    
    // Turn down various settings on the vehicle.
    stop_chase(veh);
    veh->cspeed = SPEED_OFF;
    veh->dest = 0;
    
    if (veh->towing) {
      veh_to_room(veh->towing, veh->in_room);
      veh->towing = NULL;
    }
    
    // Dump out the people in the vehicle and set their relevant values.
    for (i = veh->people; i; i = next) {
      next = i->next_in_veh;
      stop_manning_weapon_mounts(i, FALSE);
      char_from_room(i);
      char_to_room(i, veh->in_room);
      // TODO: What about the other flags for people who are sitting in the back working on something?
      AFF_FLAGS(i).RemoveBits(AFF_PILOT, AFF_RIG, ENDBIT);
      
      // Deal damage.
      damage_rating = convert_damage(stage(-success_test(GET_BOD(i), damage_tn), damage_rating));
      damage(i, i, damage_rating, TYPE_CRASH, PHYSICAL);
    }
    
    // Dump out and destroy objects.
    for (obj = veh->contents; obj; obj = nextobj) {
      nextobj = obj->next_content;
      switch(number(0, 2)) {
          /* case 0: the item stays in the vehicle */
        case 1:
          obj_from_room(obj);
          obj_to_room(obj, veh->in_room);
          break;
        case 2:
          extract_obj(obj);
          break;
      }
    }
  }
  return;
}

void vram(struct veh_data * veh, struct char_data * ch, struct veh_data * tveh)
{
  int power, damage_total = 0, veh_dam = 0;
  int veh_resist = 0, ch_resist = 0, modbod = 0;
  
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
    
    veh_resist = success_test(veh->body, power);
    veh_dam -= veh_resist;
    
    bool will_damage_vehicle = FALSE;
    
    if (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_NORAM)) {
      damage_total = -1;
      snprintf(buf, sizeof(buf), "You can't seem to get close enough to run %s down!\r\n", thrdgenders[(int)GET_SEX(ch)]);
      snprintf(buf1, sizeof(buf1), "%s can't seem to get close enough to $n to run $m down!", GET_VEH_NAME(veh));
      snprintf(buf2, sizeof(buf2), "%s can't even get close to you!", GET_VEH_NAME(veh));
      send_to_driver(buf, veh);
    } else if (damage_total < LIGHT) {
      snprintf(buf, sizeof(buf), "You ram into %s, but %s armor holds!", thrdgenders[(int)GET_SEX(ch)], HSHR(ch));
      snprintf(buf1, sizeof(buf1), "%s rams into $n, but $s armor holds!", GET_VEH_NAME(veh));
      snprintf(buf2, sizeof(buf2), "%s rams into you, but your armor holds!", GET_VEH_NAME(veh));
      send_to_driver(buf, veh);
      will_damage_vehicle = TRUE;
    } else if (veh_dam > 0) {
      send_to_veh("THUMP!\r\n", veh, NULL, TRUE);
      snprintf(buf, sizeof(buf), "You run %s down!\r\n", thrdgenders[(int)GET_SEX(ch)]);
      snprintf(buf1, sizeof(buf1), "%s runs $n down!", GET_VEH_NAME(veh));
      snprintf(buf2, sizeof(buf2), "%s runs you down!", GET_VEH_NAME(veh));
      send_to_driver(buf, veh);
      will_damage_vehicle = TRUE;
    } else {
      send_to_veh("THUTHUMP!\r\n", veh, NULL, TRUE);
      snprintf(buf, sizeof(buf), "You roll right over %s!\r\n", thrdgenders[(int)GET_SEX(ch)]);
      snprintf(buf1, sizeof(buf1), "%s rolls right over $n!", GET_VEH_NAME(veh));
      snprintf(buf2, sizeof(buf2), "%s runs right over you!", GET_VEH_NAME(veh));
      send_to_driver(buf, veh);
    }
    act(buf1, FALSE, ch, 0, 0, TO_ROOM);
    act(buf2, FALSE, ch, 0, 0, TO_CHAR);
    damage(get_driver(veh), ch, damage_total, TYPE_RAM, PHYSICAL);
    
    // Deal damage to the rammer's vehicle if they incurred any.
    if (will_damage_vehicle && veh_dam > 0) {
      veh->damage += veh_dam;
      chkdmg(veh);
    }
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
    
    snprintf(buf, sizeof(buf), "A %s rams into you!\r\n", GET_VEH_NAME(veh));
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
    
    veh_resist = 0 - success_test(veh->body, power);
    staged_damage = stage(veh_resist, veh_dam);
    damage_total = convert_damage(staged_damage);
    veh->damage += damage_total;
    
    snprintf(buf, sizeof(buf), "You ram a %s!\r\n", GET_VEH_NAME(tveh));
    snprintf(buf1, sizeof(buf1), "%s rams straight into your ride!\r\n", GET_VEH_NAME(veh));
    strcpy(buf3, GET_VEH_NAME(veh));
    snprintf(buf2, sizeof(buf2), "%s rams straight into %s!\r\n", buf3, GET_VEH_NAME(tveh));
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
}

void vcombat(struct char_data * ch, struct veh_data * veh)
{
  char ammo_type[20];
  static struct obj_data *wielded = NULL;
  static int base_target, power, damage_total;
  
  int attack_success = 0, attack_resist=0, skill_total = 1;
  int recoil=0, burst=0, recoil_comp=0, newskill, modtarget = 0;
  
  if (IS_AFFECTED(ch, AFF_PETRIFY)) {
    act("$n: is petrified, aborting attack.", 0, ch, 0, 0, TO_ROLLS);
    stop_fighting(ch);
    return;
  }
    
  if (veh->damage >= VEH_DAM_THRESHOLD_DESTROYED) {
    act("$n: target vehicle has been destroyed, aborting attack.", 0, ch, 0, 0, TO_ROLLS);
    stop_fighting(ch);
    return;
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
    vram(veh, ch, NULL);
    return;
  }
  
  if (wielded) {
    // Ensure it has ammo.
    if (!has_ammo(ch, wielded)) {
      act("$n has no ammo, aborting attack.", 0, ch, 0, 0, TO_ROLLS);
      return;
    }
      
    // Set the attack description.
    switch(GET_OBJ_VAL(wielded, 3)) {
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
    if (IS_BURST(wielded) && wielded->contains && GET_OBJ_TYPE(wielded->contains) == ITEM_GUN_MAGAZINE) {
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
    send_to_veh(buf, veh, 0, TRUE);
    return;
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
  if (wielded && IS_OBJ_STAT(wielded, ITEM_SNIPER) && ch->in_room == veh->in_room)
    modtarget += 6;
  
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
  
  base_target = 4 + modtarget + recoil;
  
  attack_success = success_test(skill_total, base_target);
  if (attack_success < 1)
  {
    if (wielded) {
      snprintf(buf, sizeof(buf), "$n fires his $o at %s, but misses.", GET_VEH_NAME(veh));
      snprintf(buf1, sizeof(buf1), "You fire your $o at %s, but miss.", GET_VEH_NAME(veh));
      snprintf(buf2, sizeof(buf2), "%s's %s misses you completely.\r\n", GET_NAME(ch), ammo_type);
    } else {
      snprintf(buf, sizeof(buf), "$n swings at %s, but misses.", GET_VEH_NAME(veh));
      snprintf(buf1, sizeof(buf1), "You swing at %s, but miss.", GET_VEH_NAME(veh));
      snprintf(buf2, sizeof(buf2), "%s's %s misses you completely.\r\n", GET_NAME(ch), ammo_type);
    }
    act(buf, FALSE, ch, wielded, 0, TO_NOTVICT);
    act(buf1, FALSE, ch, wielded, 0, TO_CHAR);
    send_to_veh(buf2, veh, 0, TRUE);
    weapon_scatter(ch, ch, wielded);
    return;
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
    snprintf(buf, sizeof(buf), "%s attacked vehicle (%s) owned by player %s (%ld).", GET_CHAR_NAME(ch), GET_VEH_NAME(veh), cname, veh->owner);
    delete [] cname;
    mudlog(buf, ch, LOG_WRECKLOG, TRUE);
  }
  chkdmg(veh);
}

void mount_fire(struct char_data *ch)
{
  struct veh_data *veh = NULL;
  struct obj_data *mount = NULL, *gun = NULL;
  RIG_VEH(ch, veh);
  
  // Hands-on firing.
  if (AFF_FLAGGED(ch, AFF_MANNING)) {
    // No mount or mount is empty? Failure.
    if (!(mount = get_mount_manned_by_ch(ch)) || !(gun = get_mount_weapon(mount)))
      return;
    
    // No target? Failure.
    if (!mount->targ && !mount->tveh)
      return;
    
    // Got a target and a gun? Fire.
    if (mount->targ && (gun = get_mount_weapon(mount)))
      hit(ch, mount->targ, gun, NULL);
    else
      vcombat(ch, mount->tveh);
    
    return;
  }
  
  if (ch->char_specials.rigging || AFF_FLAGGED(ch, AFF_RIG)) {
    for (mount = veh->mount; mount; mount = mount->next_content) {          
      // If nobody's manning it and it has a gun...
      if (!mount->worn_by && (gun = get_mount_weapon(mount))) {
        // Fire at the enemy, assuming we're fighting it.
        if (mount->targ && FIGHTING(ch) == mount->targ)
          hit(ch, mount->targ, gun, NULL);
        else if (mount->tveh && FIGHTING_VEH(ch) == mount->tveh)
          vcombat(ch, mount->tveh);
      }
    }
  }
}
