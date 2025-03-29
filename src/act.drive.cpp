#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "house.hpp"
#include "transport.hpp"
#include "constants.hpp"
#include "limits.hpp"
#include "act.drive.hpp"
#include "config.hpp"
#include "ignore_system.hpp"
#include "zoomies.hpp"
#include "vehicles.hpp"
#include "matrix_storage.hpp"

void die_follower(struct char_data *ch);
void roll_individual_initiative(struct char_data *ch);
void order_list(struct char_data *start);
extern int find_first_step(vnum_t src, vnum_t target, bool ignore_roads, const char *call_origin, struct char_data *caller);
int move_vehicle(struct char_data *ch, int dir);
ACMD_CONST(do_return);
int get_vehicle_modifier(struct veh_data *veh, bool include_weather=TRUE);
void stop_vehicle(struct veh_data *veh);
void stop_rigging(struct char_data *ch);
void stop_driving(struct char_data *ch, bool is_involuntary);
int calculate_vehicle_entry_load(struct veh_data *veh);

extern int max_npc_vehicle_lootwreck_time;

extern void rectify_desc_host(struct descriptor_data *desc);
extern int get_obj_vehicle_load_usage(struct obj_data *obj, bool is_installed_mod);
extern void recalculate_vehicle_usedload(struct veh_data *veh);

#define VEH ch->in_veh

int get_maneuver(struct veh_data *veh)
{
  int score = -2;
  struct char_data *ch = NULL;
  int x = 0, skill;

  switch (veh->type)
  {
  case VEH_BIKE:
    score += 5;
    break;
  case VEH_TRUCK:
    score -= 5;
    break;
  case VEH_CAR:
    if (veh->speed >= 200)
      score += 5;
    break;
  }
  if (veh->cspeed > SPEED_IDLE)
    score += (int)(get_speed(veh) / 10);

  if (veh->rigger)
    ch = veh->rigger;
  else
    for (ch = veh->people; ch; ch = ch->next_in_veh)
      if (AFF_FLAGGED(ch, AFF_PILOT))
        break;
  if (ch)
  {
    // TODO: What is this passage for? -LS
    int dummy_target = 0;
    for (skill = veh_skill(ch, veh, &dummy_target); skill > 0; skill--) {
      int sr_dice = srdice();
      x = MAX(x, sr_dice);
    }
    score += x;
  }
  return score;
}

void stop_chase(struct veh_data *veh)
{
  struct veh_follow *k, *j;

  if (!veh->following)
    return;

  if (veh == veh->following->followers->follower)
  {
    k = veh->following->followers;
    veh->following->followers = k->next;
    DELETE_AND_NULL(k);
  } else
  {
    for (k = veh->following->followers; k->next->follower != veh; k = k->next)
      ;
    j = k->next;
    k->next = j->next;
    DELETE_AND_NULL(j);
  }
  veh->following = NULL;
}

void crash_test(struct char_data *ch, bool no_driver)
{
  int target = 0, skill = 0;
  int power, attack_resist = 0, damage_total = SERIOUS;
  struct veh_data *veh;
  struct char_data *tch, *next;
  char crash_buf[500];

  RIG_VEH(ch, veh);
  target = veh->handling + get_vehicle_modifier(veh) + modify_target(ch);
  power = (int)(ceilf(get_speed(veh) / 10));

  snprintf(buf, sizeof(buf), "%s begins to lose control!\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
  send_to_room(buf, get_veh_in_room(veh), veh);

  skill = (no_driver ? 0 : veh_skill(ch, veh, &target)) + veh->autonav;

  if (success_test(skill, target, ch, "crash_test vehicle skill test") > 0)
  {
    snprintf(crash_buf, sizeof(crash_buf), "^y%s shimmies sickeningly under you, but remains under control.^n\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
    send_to_veh(crash_buf, veh, NULL, TRUE);

    // we didn't crash, so we *do* have the skills to be driving like this? - Khai
    // if (!number(0, 10))
    //   send_to_char("^YYou don't have the skills to be driving like this!^n\r\n", ch);
    return;
  }
  
  if (veh->in_room && IS_WATER(veh->in_room)) {
    snprintf(crash_buf, sizeof(crash_buf), "^r%s shimmies sickeningly under you, then bounces hard before flipping over!^n\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
    snprintf(buf, sizeof(buf), "%s plows into a wave and flips!\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
  } else {
    snprintf(crash_buf, sizeof(crash_buf), "^r%s shimmies sickeningly under you, then bounces hard before careening off the road!^n\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
    snprintf(buf, sizeof(buf), "%s careens off the road!\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
  }
  send_to_veh(crash_buf, veh, NULL, TRUE);
  send_to_room(buf, get_veh_in_room(veh), veh);

  attack_resist = success_test(veh->body, power, ch, "crash_test attack_resist") * -1;

  int staged_damage = stage(attack_resist, damage_total);
  damage_total = convert_damage(staged_damage);

  veh->damage += damage_total;
  stop_chase(veh);
  if ((veh->type == VEH_BIKE || veh->type == VEH_MOTORBOAT) && veh->people)
  {
    power = (int)(get_speed(veh) / 10);
    for (tch = veh->people; tch; tch = next) {
      next = tch->next_in_veh;
      char_from_room(tch);
      char_to_room(tch, get_veh_in_room(veh));
      damage_total = convert_damage(stage(0 - success_test(GET_BOD(tch), power, tch, "crash_test damage resist"), MODERATE));
      send_to_char(tch, "You are thrown from the %s!\r\n", veh->type == VEH_BIKE ? "bike" : "boat");
      damage(tch, tch, damage_total, TYPE_CRASH, PHYSICAL);
    }
    AFF_FLAGS(ch).RemoveBits(AFF_PILOT, AFF_RIG, ENDBIT);
    veh->cspeed = SPEED_OFF;
    return;
  }

  // Can't drive wrecked vehicles
  if (veh->damage >= VEH_DAM_THRESHOLD_DESTROYED) {
    send_to_veh("You slam against your straps as you jerk to a sudden stop.\r\n", veh, 0, TRUE);
    AFF_FLAGS(ch).RemoveBits(AFF_PILOT, AFF_RIG, ENDBIT);
    veh->cspeed = SPEED_OFF;
  }
}

bool passed_drive_and_rig_precondition_checks(struct char_data *ch) {
  struct char_data *temp = NULL;

  FALSE_CASE(IS_ASTRAL(ch), "You cannot seem to touch physical objects.");
  FALSE_CASE(!VEH, "You have to be in a vehicle to do that.");
  FALSE_CASE(!ch->vfront, "Nobody likes a backseat driver. ##^WSWITCH^n to the front first.");
  FALSE_CASE(IS_WORKING(ch) && !AFF_FLAGGED(ch, AFF_PILOT), "You're too busy.");
  FALSE_CASE(!IS_NPC(ch) && GET_SKILL(ch, get_pilot_skill_for_veh(VEH)) == 0 && GET_SKILL(ch, SKILL_PILOT_CAR) == 0 && GET_SKILL(ch, SKILL_PILOT_BIKE) == 0 && GET_SKILL(ch, SKILL_PILOT_TRUCK) == 0,
             "You have no idea how to drive this thing.");
  FALSE_CASE(VEH->damage >= VEH_DAM_THRESHOLD_DESTROYED, "This vehicle is too much of a wreck to move!");
  FALSE_CASE(VEH->rigger || VEH->dest, "You can't seem to take control!");
  FALSE_CASE((temp = get_driver(VEH)) && temp != ch, "Someone is already in charge!");
  FALSE_CASE((VEH->type == VEH_BIKE || VEH->type == VEH_MOTORBOAT) && VEH->locked && GET_IDNUM(ch) != VEH->owner,
             "You can't seem to start it.");

  FALSE_CASE((VEH->cspeed == SPEED_OFF || VEH->dest) && VEH->hood, "You can't drive with the hood up.");
  
  return TRUE;
}

ACMD(do_drive)
{
  // Error messages sent in function.
  if (!passed_drive_and_rig_precondition_checks(ch))
    return;
  
  FAILURE_CASE(AFF_FLAGGED(ch, AFF_RIG), "You are already rigging the vehicle.");

  // Start driving.
  if (VEH->cspeed == SPEED_OFF || VEH->dest) {
    if (!VEH->locked && VEH->owner == GET_IDNUM(ch)) {
      send_to_veh("The doors click locked.\r\n", VEH, NULL, FALSE);
      VEH->locked = TRUE;
    }

    AFF_FLAGS(ch).SetBit(AFF_PILOT);
    VEH->lastin[0] = VEH->in_room;
    stop_manning_weapon_mounts(ch, TRUE);

    if (veh_is_aircraft(VEH)) {
      send_to_char("You take the stick.\r\n", ch);
      snprintf(buf1, sizeof(buf1), "%s takes the stick.\r\n", capitalize(GET_NAME(ch)));
      VEH->cspeed = SPEED_IDLE;
    } else {
      send_to_char("You take the wheel and accelerate to a cruise.\r\n", ch);
      snprintf(buf1, sizeof(buf1), "%s takes the wheel and accelerates to a cruise.\r\n", capitalize(GET_NAME(ch)));
      VEH->cspeed = SPEED_CRUISING;
    }
    send_to_veh(buf1, VEH, ch, FALSE);
    return;
  }
  
  // Stop driving.
  FAILURE_CASE(veh_is_currently_flying(VEH), "Relinquishing the controls in midair is a great way to crash and die.");

  AFF_FLAGS(ch).RemoveBit(AFF_PILOT);
  send_to_char("You relinquish the driver's seat.\r\n", ch);
  snprintf(buf1, sizeof(buf1), "%s relinquishes the driver's seat.\r\n", capitalize(GET_NAME(ch)));
  send_to_veh(buf1, VEH, ch, FALSE);
  stop_chase(VEH);
  if (!VEH->dest)
    VEH->cspeed = SPEED_OFF;
}

ACMD(do_rig)
{
  // Error messages sent in function.
  if (!passed_drive_and_rig_precondition_checks(ch))
    return;

  // Error message sent in function.
  if (!get_datajack(ch, TRUE))
    return;

  if(VEH->cspeed == SPEED_OFF || VEH->dest) {
    if (!VEH->locked && VEH->owner == GET_IDNUM(ch)) {
      send_to_veh("The doors click locked.\r\n", VEH, NULL, FALSE);
      VEH->locked = TRUE;
    }

    // No perception while rigging. Specifically only checks player perception.
    if (PLR_FLAGGED(ch, PLR_PERCEIVE)) {
      ACMD_DECLARE(do_astral);
      do_astral(ch, buf, 0, SCMD_PERCEIVE);
    }

    AFF_FLAGS(ch).SetBits(AFF_PILOT, AFF_RIG, ENDBIT);
    VEH->cspeed = SPEED_CRUISING;
    VEH->lastin[0] = VEH->in_room;

    stop_manning_weapon_mounts(ch, TRUE);
    send_to_char("You jack in and accelerate to a cruise.\r\n", ch);
    snprintf(buf1, sizeof(buf1), "%s jacks into the vehicle control system and accelerates to a cruise.\r\n", capitalize(GET_NAME(ch)));
    send_to_veh(buf1, VEH, ch, FALSE);
  } else {
    if (!AFF_FLAGGED(ch, AFF_RIG)) {
      send_to_char("But you're not rigging.\r\n", ch);
      return;
    }
    stop_rigging(ch);
    stop_chase(VEH);
    stop_fighting(ch);
  }
  return;
}

ACMD(do_vemote)
{
  struct veh_data *veh;
  if (!(AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE))) {
    send_to_char("You must be driving a vehicle to use that command.\r\n", ch);
    return;
  }
  if (!*argument) {
    send_to_char("Yes... but what?\r\n", ch);
    return;
  }
  RIG_VEH(ch, veh)
  snprintf(buf, sizeof(buf), "%s%s%s^n\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)), argument, ispunct(get_final_character_from_string(argument)) ? "" : ".");
  if (veh->in_room)
    send_to_room(buf, veh->in_room, veh);
  else
    send_to_veh(buf, veh->in_veh, ch, TRUE);
  snprintf(buf, sizeof(buf), "Your vehicle%s%s^n\r\n", argument, ispunct(get_final_character_from_string(argument)) ? "" : ".");
  send_to_veh(buf, veh, NULL, TRUE);
  return;
}

ACMD(do_ram)
{
  struct veh_data *veh, *tveh = NULL;
  struct char_data *vict = NULL;

  FAILURE_CASE(PRF_FLAGGED(ch, PRF_PASSIVE_IN_COMBAT), "You can't do that while in passive combat mode. ^WTOGGLE PASSIVE^n to disable it.");

  if (!(AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE))) {
    send_to_char("You need to be driving a vehicle to do that...\r\n", ch);
    return;
  }
  if (CH_IN_COMBAT(ch)) {
    send_to_char(ch, "You're in the middle of combat!\r\n");
    return;
  }
  RIG_VEH(ch, veh);
  if (veh_is_currently_flying(veh)) {
    send_to_char("While airborne? You'd die.\r\n", ch);
    return;
  }
  if (veh->cspeed <= SPEED_IDLE) {
    send_to_char("You're moving far too slowly.\r\n", ch);
    return;
  }
  if (get_veh_in_room(veh)->peaceful) {
    send_to_char("This room just has a peaceful, easy feeling...\r\n", ch);
    return;
  }
  if (veh->in_veh) {
    send_to_char("There's not enough room in here to even think about trying something like that.\r\n", ch);
    return;
  }
  two_arguments(argument, arg, buf2);

  if (!*arg) {
    send_to_char("Ram what?\r\n", ch);
    return;
  }

  if (!(vict = get_char_room(arg, veh->in_room)) &&
      !(tveh = get_veh_list(arg, veh->in_room->vehicles, ch)))
  {
    send_to_char("You can't seem to find the target you're looking for.\r\n", ch);
    return;
  }

  if (vict) {
    if (!invis_ok(ch, vict)) {
      send_to_char("You can't seem to find the target you're looking for.\r\n", ch);
      return;
    }

#ifdef ENABLE_PK
    if (!IS_NPC(vict) && !(IS_NPC(ch) || (PRF_FLAGGED(ch, PRF_PKER) && PRF_FLAGGED(vict, PRF_PKER)))) {
      send_to_char("You have to be flagged PK to attack another player.\r\n", ch);
      return;
    }
#else
    if (!IS_NPC(vict) && !IS_NPC(ch) && !IS_SENATOR(ch)) {
      send_to_char("You can't ram player characters.\r\n", ch);
      return;
    }
#endif

    if (IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
      send_to_char("You can't seem to find the target you're looking for.\r\n", ch);
      log_attempt_to_bypass_ic_ignore(ch, vict, "do_ram");
      return;
    }

    if (IS_IGNORING(ch, is_blocking_ic_interaction_from, vict)) {
      send_to_char("You can't attack someone you've blocked IC interaction with.\r\n", ch);
      return;
    }

    if (IS_ASTRAL(vict)) {
      send_to_char("Your car's not nearly cool enough to be able to ram astral beings.\r\n", ch);
      return;
    }
  }

  if (tveh) {
    if (tveh == veh) {
      send_to_char("You can't ram yourself!\r\n", ch);
      return;
    }

    // Prevent PvP damage against vehicles.
    if (!can_damage_vehicle(ch, tveh)) {
      return;
    }
  }

  do_raw_ram(ch, veh, tveh, vict);
  WAIT_STATE(ch, (int) RAM_WAIT_STATE_TIME);
}

void do_raw_ram(struct char_data *ch, struct veh_data *veh, struct veh_data *tveh, struct char_data *vict) {
  int target = 0, vehm = 0, tvehm = 0;

  if (tveh && tveh == veh) {
    strncpy(buf, "SYSERR: do_raw_ram got veh = tveh!", sizeof(buf) - 1);
    mudlog(buf, ch, LOG_SYSLOG, TRUE);

    if (vict)
      tveh = NULL;
    else
      return;
  }

  // Alarm all NPCs inside the ramming vehicle. They don't get mad at the PC in particular, just mad in general.
  for (struct char_data *npc = veh->people; npc; npc = npc->next_in_veh) {
    set_mob_alarm(npc, NULL, 30);
  }

  if (tveh) {
    // Alarm all NPCs inside the targeted vehicle.
    for (struct char_data *npc = tveh->people; npc; npc = npc->next_in_veh) {
      set_mob_alarm(npc, ch, 30);
    }

    target = get_vehicle_modifier(veh) + veh->handling + modify_target(ch);
    vehm = get_maneuver(veh);
    tvehm = get_maneuver(tveh);
    if (vehm > (tvehm + 10))
      target -= 4;
    else if (vehm > tvehm)
      target -= 2;
    else if (vehm < (tvehm - 10))
      target += 4;
    else if (vehm < tvehm)
      target += 2;
    strlcpy(buf3, capitalize(GET_VEH_NAME_NOFORMAT(veh)), sizeof(buf3));
    snprintf(buf, sizeof(buf), "%s heads straight towards your ride.\r\n", buf3);
    snprintf(buf1, sizeof(buf1), "%s heads straight towards %s.\r\n", buf3, GET_VEH_NAME(tveh));
    snprintf(buf2, sizeof(buf2), "You attempt to ram %s.\r\n", GET_VEH_NAME(tveh));
    send_to_veh(buf, tveh, 0, TRUE);
    send_to_room(buf1, veh->in_room, tveh);
    send_to_char(buf2, ch);
  } else {
    target = get_vehicle_modifier(veh) + veh->handling + modify_target(ch);
    snprintf(buf, sizeof(buf), "%s heads straight towards you.", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
    snprintf(buf1, sizeof(buf1), "%s heads straight towards $n.", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
    act(buf, FALSE, vict, 0, 0, TO_CHAR);
    act(buf1, FALSE, vict, 0, 0, TO_ROOM);
    act("You head straight towards $N.", FALSE, ch, 0, vict, TO_CHAR);
    if (GET_POS(vict) > POS_RESTING && success_test(GET_REA(vict), 4, vict, "do_raw_ram victim reaction", ch)) {
      send_to_char(vict, "You quickly let out an attack at it!\r\n");
      act("$n quickly lets an attack fly.", FALSE, vict, 0, 0, TO_ROOM);
      act("$N suddenly attacks!", FALSE, ch, 0, vict, TO_CHAR);
      AFF_FLAGS(vict).SetBit(AFF_COUNTER_ATT);
      vcombat(vict, veh);
      AFF_FLAGS(vict).RemoveBit(AFF_COUNTER_ATT);
      if (veh->damage >= VEH_DAM_THRESHOLD_DESTROYED)
        return;
    }
  }
  int skill = veh_skill(ch, veh, &target);
  int success = success_test(skill, target, ch, "do_raw_ram driving test", vict);
  if (vict) {
    if (IS_NPC(vict)) {
      // Swap all dice into dodge for this round.
      GET_DODGE(vict) += GET_BODY_POOL(vict) + GET_OFFENSE(vict);
      GET_BODY_POOL(vict) = GET_OFFENSE(vict) = 0;
    }
    int vict_dodge_dice = GET_DODGE(vict) + (GET_TRADITION(vict) == TRAD_ADEPT ? GET_POWER(vict, ADEPT_SIDESTEP) : 0);
    if (vict_dodge_dice > 0) {
      target = 4 + damage_modifier(vict, buf, sizeof(buf));
      int vict_successes = success_test(vict_dodge_dice, target, vict, "do_raw_ram dodge test", ch);
      success -= vict_successes;
    } else {
      act("$n has no dodge dice allocated- cannot dodge.", TRUE, vict, 0, 0, TO_ROLLS);
    }
  }
  if (success > 0) {
    if (vict)
      vram(veh, vict, NULL);
    else
      vram(veh, NULL, tveh);
  }
  else {
    crash_test(ch, FALSE);
    chkdmg(veh);
  }
}

ACMD(do_upgrade)
{
  struct veh_data *veh;
  struct obj_data *mod, *obj, *shop = NULL;
  int j = 0, skill = 0, target = 0, kit = 0, bod_already_used = 0;
  bool need_extract = FALSE;

  half_chop(argument, buf1, buf2, sizeof(buf2));

  if (!*buf1) {
    send_to_char("What do you want to upgrade?\r\n", ch);
    return;
  }
  if (ch->in_veh && (ch->vfront || !ch->in_veh->flags.IsSet(VFLAG_WORKSHOP))) {
    send_to_char("You need to be in the back of the vehicle with an unpacked workshop to do that.\r\n", ch);
    return;
  }
  if (!(veh = get_veh_list(buf1, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch))) {
    send_to_char(ch, "You don't see a vehicle called '%s' here.\r\n", buf1);
    return;
  }

  if (!veh->owner || (veh->locked && veh->owner != GET_IDNUM(ch))) {
    snprintf(buf, sizeof(buf), "%s's anti-theft measures beep loudly.\r\n", GET_VEH_NAME(veh));
    act(buf, FALSE, ch, 0, 0, TO_ROOM);
    send_to_char(buf, ch);
    return;
  }

  if (!*buf2) {
    send_to_char("You have to upgrade it with something!\r\n", ch);
    return;
  } else if (!(mod = get_obj_in_list_vis(ch, buf2, ch->carrying))) {
    send_to_char(ch, "You don't seem to have an item called '%s'.\r\n", buf2);
    return;
  }
  if (GET_OBJ_TYPE(mod) != ITEM_MOD) {
    send_to_char(ch, "%s is not a vehicle modification.\r\n", capitalize(GET_OBJ_NAME(mod)));
    return;
  }
  if (!IS_NPC(ch)) {
    skill = get_br_skill_for_veh(veh);

    switch (GET_VEHICLE_MOD_TYPE(mod)) {
      case TYPE_ENGINECUST:
        target = 6;
        break;
      case TYPE_TURBOCHARGER:
        target = 2 + GET_VEHICLE_MOD_RATING(mod);
        break;
      case TYPE_AUTONAV:
        target = 8 - veh->handling;
        break;
      case TYPE_CMC:
      case TYPE_DRIVEBYWIRE:
        target = 10 - veh->handling;
        break;
      case TYPE_ARMOR:
      case TYPE_CONCEALEDARMOR:
        target = (int)((GET_VEHICLE_MOD_RATING(mod) + (GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod)) ? GET_VEHICLE_MOD_RATING(GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod))) : 0))/ 3);
        break;
      case TYPE_ROLLBARS:
      case TYPE_TIRES:
      case TYPE_MISC:
      case TYPE_POKEYSTICK:
        target = 3;
        break;
      case TYPE_AUTOPILOT:
        target = 8 - veh->handling;
        break;
      default:
        target = 4;
        break;
    }
    skill = get_skill(ch, skill, target);
    target += modify_target(ch);
    // This is 'cute'. Because has_kit returns a boolean, and TRUE coerces to 1, and 1 is equal to TYPE_KIT,
    //  this var TECHNICALLY contains TYPE_KIT when the player is carrying a vehicle kit.
    kit = has_kit(ch, TYPE_VEHICLE);
    if ((shop = find_workshop(ch, TYPE_VEHICLE)))
      kit = GET_WORKSHOP_GRADE(shop);
    if (!kit && !shop) {
      if (IS_SENATOR(ch)) {
        send_to_char("You handwave away the tool requirement for working on vehicles.\r\n", ch);
        kit = TYPE_FACILITY;
      } else {
        send_to_char("You don't have any tools here for working on vehicles.\r\n", ch);
        return;
      }
    }

    // Artificially capping tool type to WORKSHOP, as many upgrades need facilities but they're not in game.
    if (kit < MIN(TYPE_WORKSHOP, mod_types[GET_VEHICLE_MOD_TYPE(mod)].tools)) {
      send_to_char(ch, "You don't have the right tools for that job.\r\n");
      return;
    }

    if (mod_types[GET_VEHICLE_MOD_TYPE(mod)].tools == TYPE_KIT) {
      if (kit == TYPE_WORKSHOP)
        target--;
      else if (kit == TYPE_FACILITY)
        target -= 3;
    } else if (mod_types[GET_VEHICLE_MOD_TYPE(mod)].tools == TYPE_WORKSHOP && kit == TYPE_FACILITY)
      target--;

    if ((skill = success_test(skill, target, ch, "do_upgrade success test")) == BOTCHED_ROLL_RESULT) {
      send_to_char(ch, "You botch up the upgrade completely, destroying %s.\r\n", GET_OBJ_NAME(mod));
      extract_obj(mod);
      return;
    }

    if (skill < 1) {
      send_to_char(ch, "You can't figure out how to install it. \r\n");
      return;
    }

    if (GET_VEHICLE_MOD_TYPE(mod) == TYPE_DRIVEBYWIRE) {
      target = 4;
      skill = get_skill(ch, SKILL_BR_COMPUTER, target);
      if (success_test(skill, target, ch, "do_upgrade computers b/r test") < 0) {
        send_to_char("You can't seem to get your head around the computers on this part.\r\n", ch);
        return;
      }
      target = 4;
      skill = get_skill(ch, SKILL_BR_ELECTRONICS, target);
      if (success_test(skill, target, ch, "do_upgrade electronics b/r test") < 0) {
        send_to_char("You can't seem to get your head around the wiring on this part.\r\n", ch);
        return;
      }
    }
  }

  if (!IS_SET(GET_VEHICLE_MOD_DESIGNED_FOR_FLAGS(mod), 1 << veh->type)) {
    send_to_char(ch, "That part's not designed for %ss.\r\n", veh_types[veh->type]);
    return;
  }

  if (GET_VEHICLE_MOD_TYPE(mod) == TYPE_MOUNT) {
    // Total up the existing mounts.
    for (obj = veh->mount; obj; obj = obj->next_content) {
      if (GET_VEHICLE_MOD_TYPE(obj) == TYPE_MOUNT) {
        switch (GET_VEHICLE_MOD_MOUNT_TYPE(obj)) {
          case MOUNT_FIRMPOINT_INTERNAL:
          case MOUNT_FIRMPOINT_EXTERNAL:
            bod_already_used++;
            break;
          case MOUNT_HARDPOINT_INTERNAL:
          case MOUNT_HARDPOINT_EXTERNAL:
          case MOUNT_MINITURRET:
            bod_already_used += 2;
            break;
          case MOUNT_TURRET:
            bod_already_used += 4;
            break;
        }
      }
    }

    // Add the new mount's data.
    int bod_required = 0;
    switch (GET_VEHICLE_MOD_MOUNT_TYPE(mod)) {
      case MOUNT_FIRMPOINT_EXTERNAL:
      case MOUNT_FIRMPOINT_INTERNAL:
        bod_required = 1;
        break;
      case MOUNT_HARDPOINT_EXTERNAL:
      case MOUNT_HARDPOINT_INTERNAL:
      case MOUNT_MINITURRET:
        bod_required = 2;
        break;
      case MOUNT_TURRET:
        bod_required = 4;
        break;
    }

    if ((bod_already_used + bod_required) > veh->body) {
      send_to_char(ch, "%s requires %d free bod, and %s only has %d.\r\n",
                   GET_OBJ_NAME(mod),
                   bod_required,
                   GET_VEH_NAME(veh),
                   veh->body - bod_already_used);
      return;
    }

    int mod_load_required = get_obj_vehicle_load_usage(mod, TRUE);
    if ((veh->usedload + mod_load_required) > veh->load) {
        send_to_char(ch, "%s requires %d free load space, and %s only has %.2f.\r\n",
                     GET_OBJ_NAME(mod),
                     mod_load_required,
                     GET_VEH_NAME(veh),
                     veh->load - veh->usedload);
      return;
    }

    veh->usedload += mod_load_required;
    veh->sig -= get_mount_signature_penalty(mod);
    obj_from_char(mod);
    if (veh->mount)
      mod->next_content = veh->mount;
    veh->mount = mod;
  } else {
    if (GET_VEHICLE_MOD_ENGINE_BITS(mod) && !IS_SET(GET_VEHICLE_MOD_ENGINE_BITS(mod), 1 << veh->engine)) {
      send_to_char("You can't use that part on this type of engine.\r\n", ch);
      return;
    }
    if (GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod))) {
      /* Prior logic included this: (GET_OBJ_VAL(mod, 0) != TYPE_SEATS
           && GET_OBJ_VAL(mod, 0) != TYPE_ARMOR
           && GET_OBJ_VAL(mod, 0) != TYPE_CONCEALEDARMOR)

        but it wasn't actually done in a way that would make it save across reboot, so it's stripped out. */
      send_to_char(ch, "There's already something installed in the %s position.\r\n", mod_name[GET_VEHICLE_MOD_LOCATION(mod)]);
      return;
    }
    if (GET_VEHICLE_MOD_TYPE(mod) == TYPE_ARMOR || GET_VEHICLE_MOD_TYPE(mod) == TYPE_CONCEALEDARMOR) {
      if (GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod)) && GET_VEHICLE_MOD_TYPE(GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod))) != GET_VEHICLE_MOD_TYPE(mod)) {
        send_to_char("You cannot mix normal and concealed armor.\r\n", ch);
        return;
      }
      int totalarmor = GET_VEHICLE_MOD_RATING(mod);
      if (GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod)))
        totalarmor += GET_VEHICLE_MOD_RATING(GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod)));
      if (totalarmor > veh->body / 2) {
        send_to_char("You can't put this much armor on that vehicle.\r\n", ch);
        return;
      }
      if (!GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod)))
        GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod)) = mod;
      else
        affect_veh(veh, mod->affected[0].location, -GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod))->affected[0].modifier);

      veh->usedload += get_obj_vehicle_load_usage(mod, TRUE);
      GET_VEHICLE_MOD_LOAD_SPACE_REQUIRED(GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod))) = (veh->body * veh->body) * totalarmor * 5;
      mod->affected[0].modifier = totalarmor;
      affect_veh(veh, mod->affected[0].location, mod->affected[0].modifier);
      if (GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod)) != mod)
        need_extract = TRUE;
      else
        obj_from_char(mod);
    } else {
      if (veh->load - veh->usedload < GET_VEHICLE_MOD_LOAD_SPACE_REQUIRED(mod)) {
        send_to_char(ch, "Try as you might, you just can't fit it in.\r\n");
        return;
      }
      veh->usedload += get_obj_vehicle_load_usage(mod, TRUE);
      for (j = 0; j < MAX_OBJ_AFFECT; j++)
        affect_veh(veh, mod->affected[j].location, mod->affected[j].modifier);

      if (GET_VEHICLE_MOD_TYPE(mod) == TYPE_SEATS && GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod))) {
        GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod))->affected[0].modifier++;
        need_extract = TRUE;
      } else {
        if (GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod)))
          extract_obj(GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod)));
        GET_MOD(veh, GET_VEHICLE_MOD_LOCATION(mod)) = mod;
        obj_from_char(mod);

        if (GET_VEHICLE_MOD_TYPE(mod) == TYPE_AUTONAV) {
          veh->autonav += GET_VEHICLE_MOD_RATING(mod);
        }
      }
    }
  }

  snprintf(buf, sizeof(buf), "$n goes to work on %s and installs %s.\r\n", GET_VEH_NAME(veh), GET_OBJ_NAME(mod));
  act(buf, TRUE, ch, 0, 0, TO_ROOM);
  send_to_char(ch, "You go to work on %s and install %s.\r\n", GET_VEH_NAME(veh), GET_OBJ_NAME(mod));

  if (need_extract)
    extract_obj(mod);
}

void disp_mod(struct veh_data *veh, struct char_data *ch, int i)
{
  send_to_char("\r\nMounts:\r\n", ch);
  struct obj_data *mounted_weapon = NULL;
  for (struct obj_data *mount = veh->mount; mount; mount = mount->next_content) {
    if ((mounted_weapon = get_mount_weapon(mount))) {
      send_to_char(ch, "%s mounted on %s.\r\n", CAP(GET_OBJ_NAME(mounted_weapon)), GET_OBJ_NAME(mount));
    } else if (GET_OBJ_VAL(mount, 1) != 0 && GET_OBJ_VAL(mount, 1) != 2 && mount->contains) {
      send_to_char(ch, "%s attached to %s.\r\n", CAP(GET_OBJ_NAME(mount->contains)), GET_OBJ_NAME(mount));
    } else {
      send_to_char(ch, "%s (empty).\r\n", CAP(GET_OBJ_NAME(mount)));
    }
  }
  send_to_char("Modifications:\r\n", ch);
  for (int x = 0; x < NUM_MODS; x++) {
    if (!GET_MOD(veh, x))
      continue;
    if ((x >= MOD_WHEELS && x <= MOD_ARMOR) || x == MOD_EXHAUST)
      send_to_char(ch, "  %s\r\n", GET_OBJ_NAME(GET_MOD(veh, x)));
    else switch (x) {
      case MOD_INTAKE_FRONT:
      case MOD_INTAKE_MID:
      case MOD_INTAKE_REAR:
      case MOD_ENGINE:
      case MOD_COOLING:
        if (i >= 12)
          send_to_char(ch, "  %s\r\n", GET_OBJ_NAME(GET_MOD(veh, x)));
        break;
      case MOD_SUSPENSION:
      case MOD_BRAKES:
        if (i >= 3)
          send_to_char(ch, "  %s\r\n", GET_OBJ_NAME(GET_MOD(veh, x)));
        break;
      case MOD_COMPUTER1:
      case MOD_COMPUTER2:
      case MOD_COMPUTER3:
      case MOD_PHONE:
      case MOD_RADIO:
      case MOD_SEAT:
        if (i >= 5)
          send_to_char(ch, "  %s\r\n", GET_OBJ_NAME(GET_MOD(veh, x)));
        break;
      default:
        send_to_char(ch, "  %s\r\n", GET_OBJ_NAME(GET_MOD(veh, x)));
        break;
    }
  }

  if (veh->restring) {
    send_to_char(ch, "\r\n^L(restrung from: %s^L)^n\r\n", get_string_after_color_code_removal(veh->short_description, NULL));
  }
}

ACMD(do_control)
{
  struct obj_data *cyber, *jack;
  struct veh_data *veh;
  struct char_data *temp;
  bool has_rig = FALSE;
  int i;
      
  for (cyber = ch->cyberware; cyber; cyber = cyber->next_content)
    if (GET_CYBERWARE_TYPE(cyber) == CYB_VCR)
      has_rig = TRUE;

  FAILURE_CASE(AFF_FLAGGED(ch, AFF_PILOT), "While driving? Now that would be a neat trick.");
  FAILURE_CASE(IS_WORKING(ch), "You can't pilot something while working on another project.");
  FAILURE_CASE(!has_rig, "You need a vehicle control rig to do that.");
  FAILURE_CASE(get_ch_in_room(ch)->peaceful, "You can't do that in peaceful rooms.");

  // Error message sent in function.
  if (!(jack = get_datajack(ch, TRUE)))
    return;

  FAILURE_CASE(ch->in_veh, "You can't control a vehicle from inside one.");

  has_rig = FALSE;
  for (cyber = ch->carrying; cyber; cyber = cyber->next_content)
    if (GET_OBJ_TYPE(cyber) == ITEM_RCDECK)
      has_rig = TRUE;

  if (!has_rig)
    for (int x = 0; x < NUM_WEARS; x++)
      if (GET_EQ(ch, x))
        if (GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_RCDECK)
          has_rig = TRUE;

  if (!has_rig)
    for (cyber = ch->cyberware; cyber; cyber = cyber->next_content)
      if (GET_CYBERWARE_TYPE(cyber) == CYB_CRD)
        has_rig = TRUE;

  FAILURE_CASE(!has_rig, "You need a Remote Control Deck to do that.");

  if ((i = atoi(argument)) < 0) {
    send_to_char("Which position on your subscriber list?\r\n", ch);
    return;
  }

  for (veh = ch->char_specials.subscribe; veh; veh = veh->next_sub)
    if (--i < 0)
      break;

  if (!veh) {
    send_to_char("Your subscriber list isn't that big.\r\n", ch);
    return;
  }

  if (veh->damage >= VEH_DAM_THRESHOLD_DESTROYED) {
    send_to_char("It doesn't respond.\r\n", ch);
    return;
  }

  // Prevent controlling a towed or lost vehicle.
  if (!veh->in_room && !veh->in_veh) {
    send_to_char("The tow hookup has overridden your controls.\r\n", ch);
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Vehicle %s (%ld, %ld) is not located in a valid room or vehicle in do_control()! (Probably being towed?)", GET_VEH_NAME(veh), GET_VEH_VNUM(veh), veh->idnum);
    return;
  }

  // Prevent controlling something inside a towed vehicle.
  if (!get_veh_in_room(veh)) {
    send_to_char("The tow hookup on the containing vehicle has overridden your controls.\r\n", ch);
    return;
  }

  // Prevent controlling a carried vehicle.
  if (veh->in_room && GET_ROOM_VNUM(veh->in_room) == RM_PORTABLE_VEHICLE_STORAGE) {
    send_to_char("The automated safety systems won't let you control a vehicle that's being carried around.\r\n", ch);
    return;
  }

  // No perception while controlling. Specifically only checks player perception.
  if (PLR_FLAGGED(ch, PLR_PERCEIVE)) {
    ACMD_DECLARE(do_astral);
    do_astral(ch, buf, 0, SCMD_PERCEIVE);
  }

  if (PLR_FLAGGED(ch, PLR_REMOTE))
    do_return(ch, NULL, 0, 0);

  ch->char_specials.rigging = veh;
  veh->rigger = ch;
  for(temp = veh->people; temp; temp = temp->next_in_veh)
    if (AFF_FLAGGED(temp, AFF_RIG)) {
      send_to_char("You are forced out of the system!\r\n", temp);
      AFF_FLAGS(temp).ToggleBits(AFF_RIG, AFF_PILOT, ENDBIT);
    } else if (AFF_FLAGGED(temp, AFF_PILOT)) {
      send_to_char("The controls stop responding!\r\n", temp);
      AFF_FLAGS(temp).ToggleBit(AFF_PILOT);
    } else
      send_to_char("The vehicle begins to move.\r\n", temp);

  GET_POS(ch) = POS_SITTING;
  if (GET_OBJ_TYPE(jack) == ITEM_CYBERWARE) {
    if (GET_CYBERWARE_TYPE(jack) == CYB_DATAJACK) {
      if (GET_CYBERWARE_FLAGS(jack) == DATA_INDUCTION)
        snprintf(buf, sizeof(buf), "$n places $s hand over $s induction pad as $e connects to $s deck.");
      else
        snprintf(buf, sizeof(buf), "$n slides ones end of the cable into $s datajack and the other into $s remote control deck.");
    } else {
      snprintf(buf, sizeof(buf), "$n's eye opens up as $e slides $s remote control deck cable into $s eye datajack.");
    }
  } else {
    // We should never see this.
    snprintf(buf, sizeof(buf), "$n plugs $s 'trode net into $s remote control deck.");
  }
  act(buf, TRUE, ch, NULL, NULL, TO_ROOM);
  PLR_FLAGS(ch).SetBit(PLR_REMOTE);
  send_to_char(ch, "You take control of %s.\r\n", GET_VEH_NAME(veh));

  remove_vehicle_brain(veh);
  veh->cspeed = SPEED_CRUISING;

  // Reallocate pools.
  int max_offense = MIN(GET_SKILL(ch, SKILL_GUNNERY), GET_COMBAT_POOL(ch));
  int remainder = MAX(0, GET_COMBAT_POOL(ch) - max_offense);
  GET_OFFENSE(ch) = max_offense;
  GET_BODY_POOL(ch) = 0;
  GET_DODGE(ch) = remainder;
}


ACMD(do_subscribe)
{
  struct veh_data *veh = NULL;
  struct obj_data *obj;
  bool has_deck = FALSE;
  int i = 0, num;

  for (obj = ch->carrying; obj; obj = obj->next_content)
    if (GET_OBJ_TYPE(obj) == ITEM_RCDECK)
      has_deck = TRUE;

  if (!has_deck)
    for (int x = 0; x < NUM_WEARS; x++)
      if (GET_EQ(ch, x))
        if (GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_RCDECK)
          has_deck = TRUE;

  if (!has_deck) {
    for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content)
      if (GET_CYBERWARE_TYPE(cyber) == CYB_CRD)
        has_deck = TRUE;
  }

  FAILURE_CASE(!has_deck, "You need a Remote Control Deck to do that.");

  if (subcmd == SCMD_UNSUB) {
    skip_spaces(&argument);
    
    FAILURE_CASE(!*argument || !isdigit(*argument), "You need to supply a number from your subscriber list (e.g. ^WUNSUB 3^n).");

    num = atoi(argument);
    for (veh = ch->char_specials.subscribe; veh; veh = veh->next_sub)
      if (--num < 0)
        break;
    
    FAILURE_CASE(!veh, "Your subscriber list isn't that big.");

    if (remove_veh_from_chs_subscriber_list(veh, ch, "do_subscribe")) {
      send_to_char(ch, "You remove %s from your subscriber list.\r\n", GET_VEH_NAME(veh));
      veh->sub_rank = 0;
    } else {
      send_to_char(ch, "You attempt to remove %s from your subscriber list, but something glitches out.\r\n", GET_VEH_NAME(veh));
    }
    return;
  }

  if (!*argument) {
    if (ch->char_specials.subscribe) {
      struct room_data *room;
      char room_name_with_coords[1000];

      send_to_char("Your subscriber list contains:\r\n", ch);
      for (veh = ch->char_specials.subscribe; veh; veh = veh->next_sub) {
        if ((room = get_veh_in_room(veh))) {
          snprintf(room_name_with_coords, sizeof(room_name_with_coords), "at %s^n", GET_ROOM_NAME(room));
          if (!ROOM_FLAGGED(room, ROOM_NOGRID) && (ROOM_FLAGGED(room, ROOM_GARAGE) || ROOM_FLAGGED(room, ROOM_ROAD))) {
            snprintf(ENDOF(room_name_with_coords), sizeof(room_name_with_coords) - strlen(room_name_with_coords), " (%ld, %ld)",
                     get_room_gridguide_x(GET_ROOM_VNUM(room)),
                     get_room_gridguide_y(GET_ROOM_VNUM(room))
                    );
          }
        } else {
          strlcpy(room_name_with_coords, "in someone's tow rig", sizeof(room_name_with_coords));
        }

        if (veh->in_veh) {
          send_to_char(ch, "%2d) [%2d/10 dam]: %-35s (in %s^n, %s) \r\n",
                       i++,
                       veh->damage,
                       GET_VEH_NAME_NOFORMAT(veh),
                       GET_VEH_NAME(veh->in_veh),
                       room_name_with_coords
          );
        } else {
          send_to_char(ch, "%2d) [%2d/10 dam]: %-35s (%s)\r\n",
                       i++,
                       veh->damage,
                       GET_VEH_NAME(veh),
                       room_name_with_coords
          );
        }

        if (access_level(ch, LVL_PRESIDENT)) {
          send_to_char(ch, "^^-- rank %d%s, prev_sub %s, next_sub %s\r\n",
                       veh->sub_rank,
                       veh->sub ? "" : ", ^RNOT SUBBED^n",
                       GET_VEH_NAME(veh->prev_sub),
                       GET_VEH_NAME(veh->next_sub));
        }
      }
    } else
      send_to_char("Your subscriber list is empty.\r\n", ch);
    return;
  }


  one_argument(argument, buf);
  if (ch->in_veh) {
    struct veh_data *un_nest = ch->in_veh;
    while (un_nest && !veh) {
      // Starting from the vehicle you're in, search carried vehicles of this veh, then expand out to carrier's carried vehs, etc until a match is found.
      veh = get_veh_list(buf, un_nest->carriedvehs, ch);
      un_nest = un_nest->in_veh;
    }
  } else
    veh = get_veh_list(buf, get_ch_in_room(ch)->vehicles, ch);

  FAILURE_CASE_PRINTF(!veh, "You don't see any vehicles named '%s' here.", buf);
  FAILURE_CASE_PRINTF(veh->owner != GET_IDNUM(ch), "%s isn't yours.", CAP(GET_VEH_NAME_NOFORMAT(veh)));
  FAILURE_CASE_PRINTF(veh->sub, "%s is already part of your subscriber list.", CAP(GET_VEH_NAME_NOFORMAT(veh)));

  if (add_veh_to_chs_subscriber_list(veh, ch, "do_subscribe", FALSE)) {
    send_to_char(ch, "You add %s to your subscriber list.\r\n", GET_VEH_NAME(veh));
  } else {
    send_to_char(ch, "You attempt to add %s to your subscriber list, but something glitches out.\r\n", GET_VEH_NAME(veh));
  }
}

ACMD(do_repair)
{
  struct obj_data *shop = NULL;
  struct veh_data *veh;
  int target = 4, skill = 0, success = 0;
  bool kit = FALSE;

  skip_spaces(&argument);
  if (IS_ASTRAL(ch)) {
    send_to_char("You cannot seem to touch physical objects.\r\n", ch);
    return;
  }
  if (!*argument) {
    send_to_char("What do you want to repair?\r\n", ch);
    return;
  }
  if (ch->in_veh && (ch->vfront || !ch->in_veh->flags.IsSet(VFLAG_WORKSHOP))) {
    send_to_char("Well, you could probably polish the dash from in here.\r\n", ch);
    return;
  }

  if (!(veh = get_veh_list(argument, ch->in_veh ? ch->in_veh->carriedvehs : get_ch_in_room(ch)->vehicles, ch))) {
    send_to_char(ch, "You don't see any vehicles named '%s' here.\r\n", argument);
    return;
  }

  if (veh->damage == 0) {
    send_to_char(ch, "But it's not damaged!\r\n");
    return;
  }

  target += (veh->damage - 2) / 2;
  target += modify_target(ch);
  skill = get_skill(ch, get_br_skill_for_veh(veh), target);

  if (!access_level(ch, LVL_ADMIN)) {
    kit = has_kit(ch, TYPE_VEHICLE);
    shop = find_workshop(ch, TYPE_VEHICLE);
    if (!shop) {
      if (veh->damage >= VEH_DAMAGE_NEEDS_WORKSHOP) {
        send_to_char("You'll need a garage with a vehicle workshop unpacked in it to fix this much damage.\r\n", ch);
        return;
      }

      if (kit) {
        target += 2;
      } else {
        if (veh->damage >= 2) {
          send_to_char("You'll need at least a vehicle repair kit to do more than polish up scratches.\r\n", ch);
          return;
        }
        target += 4;
      }
    } else {
      if (GET_WORKSHOP_GRADE(shop) == TYPE_FACILITY) {
        target -= 2;
      } // if TYPE_WORKSHOP, no target modifier
    }
  } else {
    target = 1;
  }

  if (!IS_SENATOR(ch))
    WAIT_STATE(ch, 2 * PULSE_VIOLENCE);

  if ((success = (GET_LEVEL(ch) > LVL_MORTAL ? 50 : success_test(skill, target, ch, "do_repair"))) < 1) {
    if (success < 0) {
      send_to_char(ch, "You fuck it up and make things worse.\r\n");
      mudlog_vfprintf(ch, LOG_GRIDLOG, "Damaged %s (%ld-%ld) with botched repair.", GET_VEH_NAME(veh), GET_VEH_VNUM(veh), GET_VEH_IDNUM(veh));
      veh->damage++;
      chkdmg(veh);
    } else {
      send_to_char(ch, "You tinker with it a bit, but don't make much headway.\r\n");
    }
    return;
  }
  veh->damage -= (success + 1) / 2;
  if (veh->damage <= 0) {
    send_to_char(ch, "You go to work on the vehicle and it looks as good as new.\r\n");
    veh->damage = 0;
  } else
    send_to_char(ch, "You go to work and repair part of the damage.\r\n");
}

ACMD(do_driveby)
{
  struct obj_data *wielded;
  struct char_data *vict, *pass, *list = NULL;
  int dir;

  FAILURE_CASE(PRF_FLAGGED(ch, PRF_PASSIVE_IN_COMBAT), "You can't do that while in passive combat mode. ^WTOGGLE PASSIVE^n to disable it.");

  if (!ch->in_veh) {
    send_to_char(ch, "You must be in a vehicle to perform a driveby.\r\n");
    return;
  }
  if (veh_is_currently_flying(ch->in_veh)) {
    send_to_char("While airborne? You'd fall out.\r\n", ch);
    return;
  }

  if (IS_RIGGING(ch)) {
    send_to_char("You're jacked into the vehicle itself, so you can't perform a driveby.\r\n", ch);
    return;
  }

  if (!AFF_FLAGGED(ch, AFF_PILOT)) {
    if (PLR_FLAGGED(ch, PLR_DRIVEBY))
      send_to_char(ch, "You will no longer perform a drive-by.\r\n");
    else
      send_to_char(ch, "You will now perform a drive-by.\r\n");
    PLR_FLAGS(ch).ToggleBit(PLR_DRIVEBY);
    return;
  }

  two_arguments(argument, arg, buf2);
  if (!*arg || !*buf2) {
    send_to_char(ch, "Syntax is driveby <character> <direction to leave>.\r\n");
    return;
  }
  if (!ch->in_veh->in_room) {
    // We're inside a vehicle or otherwise don't have a containing room.
    send_to_char("You're going to have a hard time maneuvering in here.\r\n", ch);
    return;
  }
  if (!(vict = get_char_in_list_vis(ch, arg, ch->in_veh->in_room->people))) {
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
    return;
  }
  if (IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
    log_attempt_to_bypass_ic_ignore(ch, vict, "do_driveby");
    return;
  }
  if ((dir = search_block(buf2, lookdirs, FALSE)) == -1) {
    send_to_char("What direction?\r\n", ch);
    return;
  }
  dir = convert_look[dir];
  if (!EXIT(ch->in_veh, dir)) {
    send_to_char("Your vehicle can't go that way.\r\n", ch);
    return;
  }

  if (ch->in_veh->cspeed <= SPEED_IDLE) {
    send_to_char("How do you expect to driveby from a stationary vehicle?\r\n", ch);
    return;
  }

  if ((wielded = GET_EQ(ch, WEAR_WIELD))) {
    if (!(GET_OBJ_VAL(wielded, 4) == SKILL_PISTOLS || GET_OBJ_VAL(wielded, 4) == SKILL_SMG)) {
      send_to_char(ch, "You can only do a driveby with a pistol or SMG.\r\n");
      return;
    }

#ifdef ENABLE_PK
    if (!IS_NPC(vict) && !PRF_FLAGGED(ch, PRF_PKER) && !PRF_FLAGGED(vict, PRF_PKER)) {
      send_to_char(ch, "You have to be flagged PK to attack another player.\r\n");
      return;
    }
#else
    if (!IS_NPC(vict) && !IS_SENATOR(ch)) {
      send_to_char(ch, "You can't perform drivebys on player characters.\r\n");
      return;
    }
#endif

    send_to_char(ch, "You point %s towards %s and open fire!\r\n", GET_OBJ_NAME(wielded), GET_NAME(vict));
    snprintf(buf, sizeof(buf), "%s aims %s towards %s and opens fire!\r\n", GET_NAME(ch), GET_OBJ_NAME(wielded), GET_NAME(vict));
    send_to_veh(buf, ch->in_veh, ch, FALSE);
    list = ch;
    roll_individual_initiative(ch);
    GET_INIT_ROLL(ch) += 20;
  } else {
    send_to_char(ch, "You point towards %s and shout, \"Light 'em Up!\".\r\n", GET_NAME(vict));
    snprintf(buf, sizeof(buf), "%s points towards %s and shouts, \"Light 'em Up!\".!\r\n", GET_NAME(ch), GET_NAME(vict));
    send_to_veh(buf, ch->in_veh, ch, FALSE);
  }

  for (pass = ch->in_veh->people; pass; pass = pass->next_in_veh) {
    if (pass != ch) {
      if (PLR_FLAGGED(pass, PLR_DRIVEBY) && AWAKE(pass) && GET_EQ(pass, WEAR_WIELD) &&
          GET_OBJ_VAL(GET_EQ(pass, WEAR_WIELD),4) >= SKILL_FIREARMS) {
#ifdef ENABLE_PK
        if (!IS_NPC(vict) && !PRF_FLAGGED(pass, PRF_PKER) && !PRF_FLAGGED(vict, PRF_PKER))
          continue;
#else
        if (!IS_NPC(vict))
          continue;
#endif
        send_to_char(pass, "You follow %s lead and take aim.\r\n", HSHR(ch));
        PLR_FLAGS(pass).RemoveBit(PLR_DRIVEBY);
        pass->next_fighting = list;
        list = pass;
        roll_individual_initiative(pass);
        GET_INIT_ROLL(pass) += 20;
      }
    }
  }
  if (!list) {
    if (pass) {
      send_to_char(ch, "But your passengers aren't ready to do drivebys...\r\n");
    } else {
      send_to_char(ch, "But no one is in the car to hear you.\r\n");
    }
    return;
  }
  order_list(list);
  roll_individual_initiative(vict);
  if (GET_INIT_ROLL(vict) < 12)
    send_to_char(vict, "You suddenly feel a cap busted in your ass as %s zooms past!\r\n", GET_VEH_NAME(ch->in_veh));
  else
    send_to_char(vict, "A hail of bullets flies from %s as it zooms past!\r\n", GET_VEH_NAME(ch->in_veh));

  for (int i = 2; i > 0; i--) {
    for (pass = list; pass && vict->in_room == ch->in_veh->in_room; pass = pass->next_fighting) {
      if (GET_INIT_ROLL(pass) >= 0) {
        if (AFF_FLAGGED(pass, AFF_MANNING)) {
          struct obj_data *mount = get_mount_manned_by_ch(ch);
          struct obj_data *ammo = get_mount_ammo(mount);
          if (hit(pass, vict, GET_EQ(pass, WEAR_WIELD), NULL, ammo))
            return;
        } else
          if (hit(pass, vict, GET_EQ(pass, WEAR_WIELD), NULL, NULL))
            return;

        GET_INIT_ROLL(pass) -= 10;
      }
    }
  }

  for (;list ; list = pass) {
    pass = list->next_fighting;
    list->next_fighting = NULL;
  }

  if (GET_INIT_ROLL(vict) >= 12) {
    if (GET_EQ(vict, WEAR_WIELD) && GET_OBJ_VAL(GET_EQ(vict, WEAR_WIELD),4) >= SKILL_FIREARMS) {
      send_to_char(vict, "You swing around and take aim at %s.\r\n", GET_VEH_NAME(ch->in_veh));
      snprintf(buf, sizeof(buf), "%s swings around and takes aim at your ride.\r\n", GET_NAME(vict));
      send_to_veh(buf, ch->in_veh, NULL, TRUE);
      vcombat(vict, ch->in_veh);
    }
  }
  if (ch->in_veh)
    move_vehicle(ch, dir);
}

ACMD(do_speed)
{
  struct veh_data *veh;
  int i = 0;

  RIG_VEH(ch, veh);

  if (!(AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE))) {
    send_to_char("You must be driving a vehicle to do that.\r\n", ch);
    return;
  }
  one_argument(argument, buf);
  if (!*buf) {
    send_to_char("What speed?\r\n", ch);
    return;
  }

  switch (LOWER(*buf)) {
  case 'i':
    i = 1;
    break;
  case 'c':
    i = 2;
    break;
  case 's':
    i = 3;
    break;
  case 'm':
    i = 4;
    break;
  default:
    send_to_char("That is not a valid speed.\r\n", ch);
    return;
  }

  // Clear vehicle position.
  if (i > SPEED_IDLE) {
    DELETE_AND_NULL_ARRAY(GET_VEH_DEFPOS(veh));
  }

  FAILURE_CASE(veh_can_traverse_air(veh) && i > 2, "Sorry, you can't accelerate aircraft faster than cruising speed.");

  if (veh->hood) {
    send_to_char("You can't move with the hood up.\r\n", ch);
    return;
  } else if (i < veh->cspeed) {
    if (i == 1) {
      send_to_char("You bring the vehicle to a halt.\r\n", ch);
      send_to_veh("The vehicle slows to a stop.\r\n", veh, ch, FALSE);
    } else {
      if (!IS_RIGGING(ch)) {
        switch (veh->type) {
          case VEH_CAR:
          case VEH_TRUCK:
          case VEH_TRACKED:
            send_to_char("You put your foot on the brake.\r\n", ch);
            break;
          default:
            send_to_char("You reduce the throttle.\r\n", ch);
            break;
        }
        send_to_veh("You slow down.\r\n", veh, ch, TRUE);
      } else {
        send_to_veh("You slow down.\r\n", veh, NULL, TRUE);
      }
    }
  } else if (i > veh->cspeed) {
    if (!IS_RIGGING(ch)) {
      switch (veh->type) {
        case VEH_CAR:
        case VEH_TRUCK:
        case VEH_TRACKED:
          send_to_char("You put your foot on the accelerator.\r\n", ch);
          break;
        default:
          send_to_char("You open up the throttle.\r\n", ch);
          break;
      }
      send_to_veh("You speed up.\r\n", veh, ch, TRUE);
    } else {
      send_to_veh("You speed up.\r\n", veh, NULL, TRUE);
    }
  } else {
    send_to_char("But you're already traveling that fast!\r\n", ch);
    return;
  }
  veh->cspeed = i;
}

ACMD(do_chase)
{
  struct veh_data *veh, *tveh;
  struct veh_follow *k;
  struct char_data *vict;

  if (!(AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE))) {
    send_to_char(ch, "You have to be %s to do that.\r\n", ch->in_veh ? "driving" : "controlling a vehicle");
    return;
  }
  RIG_VEH(ch, veh);

  if (veh->following) {
    snprintf(buf, sizeof(buf), "You stop following a %s.\r\n", GET_VEH_NAME(veh->following));
    stop_chase(veh);
    send_to_char(buf, ch);
    return;
  } else if (veh->followch) {
    snprintf(buf, sizeof(buf), "You stop following %s.\r\n", GET_NAME(veh->followch));
    veh->followch = NULL;
    send_to_char(buf, ch);
    return;
  }

  if (veh->in_veh) {
    send_to_char("You're going to have a hard time chasing anything from in here.\r\n", ch);
    return;
  }

  two_arguments(argument, arg, buf2);

  if (!*arg) {
    send_to_char("Chase what?\r\n", ch);
    return;
  }

  if (!(tveh = get_veh_list(arg, veh->in_room->vehicles, ch)) &&
      !(vict = get_char_room(arg, veh->in_room))) {
    send_to_char(ch, "You don't see anything or anyone named '%s' here.\r\n", arg);
    return;
  }

  if (tveh == veh) {
    send_to_char(ch, "You cannot chase yourself.\r\n");
    return;
  }
  if (tveh) {
    send_to_char(ch, "You start chasing %s.\r\n", GET_VEH_NAME(tveh));
    veh->following = tveh;
    veh->dest = NULL;
    k = new veh_follow;
    k->follower = veh;
    if (tveh->followers)
      k->next = tveh->followers;
    tveh->followers = k;
    if (veh->cspeed == SPEED_IDLE) {
      send_to_char(ch, "You put your foot on the accelerator.\r\n");
      send_to_veh("You speed up.\r\n", veh, ch, FALSE);
      veh->cspeed = SPEED_CRUISING;
    }
  } else if (vict) {}
}

ACMD(do_target)
{
  struct char_data *vict = NULL;
  struct veh_data *veh, *tveh = NULL;
  struct obj_data *obj;
  bool modeall = FALSE;
  int j;
  RIG_VEH(ch, veh);

  FAILURE_CASE(PRF_FLAGGED(ch, PRF_PASSIVE_IN_COMBAT), "You can't do that while in passive combat mode. ^WTOGGLE PASSIVE^n to disable it.");

  if (!veh) {
    send_to_char("You must be in a vehicle to target someone with mounted weapons.\r\n", ch);
    return;
  }
  if (!veh->mount) {
    send_to_char("There is nothing to target.\r\n", ch);
    return;
  }
  if (veh->in_veh) {
    send_to_char("It'd be a bad idea to start firing in such an enclosed space.\r\n", ch);
    return;
  }
  if (veh_is_currently_flying(veh)) {
    send_to_char("While airborne? What is this, World War II?\r\n", ch);
    return;
  }

  two_arguments(argument, arg, buf2);
  if (!*arg) {
    send_to_char("Target what at who?\r\n", ch);
    return;
  }

  if (*arg && *buf2) {
    if (!IS_RIGGING(ch)) {
      send_to_char("But you aren't piloting this vehicle.\r\n", ch);
      return;
    }
    if ((j = atoi(arg)) < 0) {
      send_to_char("Target mount number what?\r\n", ch);
      return;
    }
    for (obj = veh->mount; obj; obj = obj->next_content)
      if (--j < 0)
        break;
    if (!obj) {
      send_to_char("Your vehicle doesn't have that many mounts.\r\n", ch);
      return;
    }
    if (!obj->contains) {
      send_to_char(ch, "There is no weapon mounted on %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
      return;
    }
    strlcpy(arg, buf2, sizeof(arg));
  } else {
    if (!IS_RIGGING(ch) && !AFF_FLAGGED(ch, AFF_MANNING)) {
      send_to_char("You don't have control over any mounts.\r\n", ch);
      return;
    }
    if (AFF_FLAGGED(ch, AFF_MANNING)) {
      if (!(obj = get_mount_manned_by_ch(ch))) {
        // Safeguard-- if they don't have a mount, drop 'em.
        send_to_char("You don't have control over any mounts.\r\n", ch);
        return;
      }
    } else {
      for (j = 0, obj = veh->mount; obj; obj = obj->next_content)
        if (obj->contains && !obj->worn_by)
          j++;
      if (!j) {
        send_to_char("You have nothing to target.\r\n", ch);
        return;
      }
      modeall = TRUE;
    }
  }

  // We know that the vehicle must be in a room to have gotten here.
  // Find vict and tveh (requiring that vict is visible if found).
  vict = get_char_room(arg, veh->in_room);
  if (vict && !CAN_SEE(ch, vict))
    vict = NULL;

  tveh = get_veh_list(arg, veh->in_room->vehicles, ch);

  // No targets? Bail out.
  if (!vict) {
    if (!tveh) {
      send_to_char(ch, "You don't see anything named '%s' here.\r\n", arg);
      return;
    }

    if (tveh == veh) {
      send_to_char("Why would you want to target yourself?\r\n", ch);
      return;
    }

    if (tveh->damage >= 10) {
      send_to_char("It's already wrecked... no point in shooting it more.\r\n", ch);
      return;
    }
  }
  else {
    if (IS_IGNORING(vict, is_blocking_ic_interaction_from, ch)) {
      send_to_char(ch, "You don't see anything named '%s' here.\r\n", arg);
      log_attempt_to_bypass_ic_ignore(ch, vict, "do_target");
      return;
    }

    if (IS_IGNORING(ch, is_blocking_ic_interaction_from, vict)) {
      send_to_char("You can't attack someone you've blocked IC interaction with.\r\n", ch);
      return;
    }
  }

  do_raw_target(ch, veh, tveh, vict, modeall, obj);
}

void do_raw_target(struct char_data *ch, struct veh_data *veh, struct veh_data *tveh, struct char_data *vict, bool modeall, struct obj_data *obj) {
  if (CH_IN_COMBAT(ch))
    stop_fighting(ch);

  // Reject PvP.
  if (vict) {
#ifdef ENABLE_PK
    if (!IS_NPC(vict) && !IS_NPC(ch) && !(PRF_FLAGGED(ch, PRF_PKER) && PRF_FLAGGED(vict, PRF_PKER))) {
      send_to_char("You and your opponent both need to be flagged PK for that.\r\n", ch);
      return;
    }
#else
    if (!IS_NPC(vict) && !IS_NPC(ch) && !IS_SENATOR(ch)) {
      send_to_char("You can't target PCs.\r\n", ch);
      return;
    }
#endif
  }

  if (tveh) {
    // TODO: Write a check to allow PKers to attack PKer vehicles.
    if (tveh->owner) {
      send_to_char("That's a player-owned vehicle-- better leave it alone.\r\n", ch);
      return;
    }
  }

  // Modeall: fire everything at them.
  if (modeall) {
    for (obj = veh->mount; obj; obj = obj->next_content) {
      struct obj_data *weapon = get_mount_weapon(obj);
      if (weapon && !obj->worn_by) {
        if (vict) {
          set_fighting(ch, vict);
          if (obj->tveh)
            obj->tveh = NULL;
          obj->targ = vict;
          set_fighting(vict, veh);
          send_to_char(ch, "You target %s towards %s.\r\n", GET_OBJ_NAME(weapon), PERS(vict, ch));
        } else if (tveh) {
          set_fighting(ch, tveh);
          if (obj->targ)
            obj->targ = NULL;
          obj->tveh = tveh;
          send_to_char(ch, "You target %s towards %s.\r\n", GET_OBJ_NAME(weapon), GET_VEH_NAME(tveh));
        } else {
          mudlog("SYSERR: Reached end of do_raw_target (mode all) with no valid target.", ch, LOG_SYSLOG, TRUE);
        }
      }
    }
    return;
  }
  if (vict) {
    for (struct obj_data *obj2 = veh->mount; obj2; obj2 = obj2->next_content)
      if (obj2 != obj && obj2->targ == vict) {
        send_to_char("Someone is already targeting that.\r\n", ch);
        return;
      }
    set_fighting(ch, vict);
    if (obj->tveh)
      obj->tveh = NULL;
    obj->targ = vict;
    set_fighting(vict, veh);
    struct obj_data *weapon = get_mount_weapon(obj);
    if (!weapon)
      return;
    act("You target $p towards $N.", FALSE, ch, weapon, vict, TO_CHAR);
    if (AFF_FLAGGED(ch, AFF_MANNING)) {
      snprintf(buf, sizeof(buf), "%s's $p swivels towards you.\r\n", GET_VEH_NAME(ch->in_veh));
      act(buf, FALSE, ch, obj, vict, TO_VICT);
      snprintf(buf, sizeof(buf), "%s's $p swivels towards $N.\r\n", GET_VEH_NAME(ch->in_veh));
      act(buf, FALSE, ch, obj, vict, TO_VEH_ROOM);
    }
  } else if (tveh) {
    set_fighting(ch, tveh);
    if (obj->targ)
      obj->targ = NULL;
    obj->tveh = tveh;
    struct obj_data *weapon = get_mount_weapon(obj);
    if (!weapon)
      return;
    send_to_char(ch, "You target %s towards %s.\r\n", GET_OBJ_NAME(weapon), GET_VEH_NAME(tveh));
    if (AFF_FLAGGED(ch, AFF_MANNING)) {
      snprintf(buf, sizeof(buf), "%s's %s swivels towards your ride.\r\n", GET_VEH_NAME(ch->in_veh), GET_OBJ_NAME(obj));
      send_to_veh(buf, tveh, 0, TRUE);
      strlcpy(buf3, GET_VEH_NAME(ch->in_veh), sizeof(buf3));
      snprintf(buf, sizeof(buf), "%s's $p swivels towards %s.\r\n", buf3, GET_VEH_NAME(tveh));
      act(buf, FALSE, ch, obj, NULL, TO_VEH_ROOM);
    }
  } else {
    mudlog("SYSERR: Reached end of do_raw_target with no valid target.", ch, LOG_SYSLOG, TRUE);
  }
}

ACMD(do_mount)
{
  int i = 0;
  struct veh_data *veh;
  struct obj_data *obj, *gun = NULL, *ammo = NULL; /* Appears unused:  *bin = NULL; */
  RIG_VEH(ch, veh);
  if (!veh) {
    send_to_char("You must be in a vehicle to control mounted weapons. (Are you looking for the ^WATTACH^n command?)\r\n", ch);
    return;
  }
  if (IS_ASTRAL(ch)) {
    send_to_char("You cannot seem to touch physical objects.\r\n", ch);
    return;
  }

  send_to_char(ch, "%s is mounting the following:\r\n", CAP(GET_VEH_NAME(veh)));
  for (obj = veh->mount; obj; obj = obj->next_content) {
    gun = NULL; ammo = NULL;
    for (struct obj_data *x = obj->contains; x; x = x->next_content)
      if (GET_OBJ_TYPE(x) == ITEM_GUN_AMMO)
        ammo = x;
      else if (GET_OBJ_TYPE(x) == ITEM_WEAPON)
        gun = x;
      /* Code appears to be unused:
      else if (GET_OBJ_TYPE(x) == ITEM_MOD)
        bin = x; */
    if (gun)
      snprintf(buf, sizeof(buf), "%2d) %-20s (Mounting %s) (Ammo %d/%d)", i, GET_OBJ_NAME(obj),
              GET_OBJ_NAME(gun), ammo ? GET_OBJ_VAL(ammo, 0) : 0, GET_OBJ_VAL(gun, 5) * MOUNTED_GUN_AMMO_CAPACITY_MULTIPLIER);
    else
      snprintf(buf, sizeof(buf), "%2d) %-20s (Mounting Nothing)", i, GET_OBJ_NAME(obj));
    if (obj->worn_by)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " (Manned by %s)", decapitalize_a_an((safe_found_mem(ch, obj->worn_by) ?
              (safe_found_mem(ch, obj->worn_by))->mem
              : GET_NAME(obj->worn_by))));
    strlcat(buf, "\r\n", sizeof(buf));
    send_to_char(buf, ch);
    i++;
  }
}

ACMD(do_man)
{
  struct obj_data *mount;
  int j;
  if (!ch->in_veh) {
    send_to_char("But you're not in a vehicle.\r\n", ch);
    return;
  }
  if (IS_ASTRAL(ch)) {
    send_to_char("You cannot seem to touch physical objects.\r\n", ch);
    return;
  }

  if (AFF_FLAGGED(ch, AFF_PILOT) || IS_WORKING(ch)) {
    send_to_char(ch, "You can't do that now!\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_MANNING)) {
    stop_manning_weapon_mounts(ch, TRUE);
    if (!*argument)
      return;
  }

  if ((j = atoi(argument)) < 0) {
    send_to_char("Which mount?\r\n", ch);
    return;
  }
  for (mount = ch->in_veh->mount; mount; mount = mount->next_content)
    if (--j < 0)
      break;

  if (!mount) {
    send_to_char("There are not that many mounts.\r\n", ch);
    return;
  }
  if (mount->worn_by) {
    send_to_char("Someone is already manning it.\r\n", ch);
    return;
  }
  if (mount_has_weapon(mount)) {
    mount->worn_by = ch;
    act("$n mans $p.", FALSE, ch, mount, 0, TO_ROOM);
    act("You man $p.", FALSE, ch, mount, 0, TO_CHAR);
    AFF_FLAGS(ch).SetBit(AFF_MANNING);
    return;
  }

  send_to_char("But there is no weapon mounted there.\r\n", ch);
}

ACMD(do_gridguide)
{
  struct veh_data *veh;
  struct grid_data *grid = NULL;
  long x = 0, y = 0;
  vnum_t grid_vnum, target_room = -1;

  RIG_VEH(ch, veh);

  FAILURE_CASE(!veh, "You have to be in a vehicle to do that.");
  FAILURE_CASE(IS_ASTRAL(ch), "You cannot seem to touch physical objects");
  FAILURE_CASE(veh_can_traverse_air(veh) && veh->type != VEH_DRONE, "Unfortunately, aircraft can't interface with the gridguide system.");
  FAILURE_CASE(!veh->autonav, "You need to have an autonav system installed.");
  FAILURE_CASE(veh->hood, "Gridguide refuses to engage with the hood up.");
  FAILURE_CASE((veh->locked && GET_IDNUM(ch) != veh->owner) && !(PLR_FLAGGED(ch, PLR_REMOTE) || AFF_FLAGGED(ch, AFF_PILOT)), "You don't have control over the vehicle.");
  FAILURE_CASE(!veh->in_room, "You'll have to drive out first.");

  argument = two_arguments(argument, arg, buf2);
  if (!*arg) {
    int i = 0;
    send_to_char("The following destinations are available:\r\n", ch);
    for (grid = veh->grid; grid; grid = grid->next) {
      i++;
      if (!veh->in_room || find_first_step(real_room(veh->in_room->number), real_room(grid->room), FALSE, "do_gridguide", ch) < 0)
        snprintf(buf, sizeof(buf), "^r%-20s [%-6ld, %-6ld](Unavailable)\r\n", CAP(grid->name),
                 get_room_gridguide_x(grid->room), get_room_gridguide_y(grid->room));
      else
        snprintf(buf, sizeof(buf), "^B%-20s [%-6ld, %-6ld](Available)\r\n", CAP(grid->name),
                 get_room_gridguide_x(grid->room), get_room_gridguide_y(grid->room));
      send_to_char(buf, ch);
    }
    send_to_char(ch, "%d Entries remaining.\r\n", GET_VEH_MAX_AUTONAV_SLOTS(veh) - i);
    if (veh->in_room) {
      if (ROOM_FLAGS(veh->in_room).AreAnySet(ROOM_INDOORS, ROOM_NOGRID, ROOM_FALL, ROOM_NOBIKE, ENDBIT)) {
        send_to_char("You are not in a valid GridGuide location.\r\n", ch);
      } else {
        send_to_char(ch, "You are currently located at %ld, %ld.\r\n",
                   get_room_gridguide_x(GET_ROOM_VNUM(veh->in_room)),
                   get_room_gridguide_y(GET_ROOM_VNUM(veh->in_room)));
      }
    }
    return;
  }

  if (!str_cmp(arg, "stop")) {
    FAILURE_CASE(!veh->dest, "The autonav is already disengaged.");

    veh->dest = 0;
    send_to_veh("The autonav disengages.\r\n", veh, 0, TRUE);
    if (!AFF_FLAGGED(ch, AFF_PILOT))
      veh->cspeed = SPEED_OFF;
    return;
  }

  if (!*buf2) {
    if (veh->cspeed > SPEED_IDLE && !(AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE))) {
      send_to_char("You don't have control over the vehicle.\r\n", ch);
      return;
    }

    for (grid = veh->grid; grid; grid = grid->next)
      if (is_abbrev(arg, grid->name))
        break;

    if (grid) {
      target_room = grid->room;
    } else {
      // Attempt to find the destination in the taxi registry for your room.
      struct dest_data *destination_list = NULL;
      const char *location_string = "";

      switch (GET_JURISDICTION(veh->in_room)) {
        case JURISDICTION_SEATTLE:
          destination_list = seattle_taxi_destinations;
          location_string = " or the Seattle gridguide system";
          break;
        case JURISDICTION_CARIBBEAN:
          destination_list = caribbean_taxi_destinations;
          location_string = " or the Caribbean gridguide system";
          break;
        case JURISDICTION_PORTLAND:
          destination_list = portland_taxi_destinations;
          location_string = " or the Portland gridguide system";
          break;
        case JURISDICTION_CAS:
          destination_list = cas_taxi_destinations;
          location_string = " or the CAS gridguide system";
        default:
          destination_list = NULL;
          break;
      }

      if (destination_list) {
        for (int dest_idx = 0; *(destination_list[dest_idx].keyword) != '\n'; dest_idx++) {
          // Skip invalid destinations.
          if (!DEST_IS_VALID(dest_idx, destination_list))
            continue;

          if (str_str(arg, destination_list[dest_idx].keyword) || (*(destination_list[dest_idx].subkeyword) && str_str(arg, destination_list[dest_idx].subkeyword))) {
            target_room = destination_list[dest_idx].vnum;
            break;
          }
        }
      }

      if (target_room <= 0) {
        send_to_char(ch, "'%s' doesn't seem to be in your favorites%s.\r\n", arg, location_string);
        return;
      }
    }

    if (!veh->in_room || find_first_step(real_room(veh->in_room->number), real_room(target_room), FALSE, "do_gridguide pt 2", ch) < 0) {
      send_to_char("That destination is currently unavailable.\r\n", ch);
      return;
    }

    if (!veh->people) {
      load_vehicle_brain(veh);
    }

    veh->dest = &world[real_room(target_room)];
    veh->cspeed = SPEED_CRUISING;

    if (AFF_FLAGGED(ch, AFF_PILOT))
      AFF_FLAGS(ch).RemoveBits(AFF_PILOT, AFF_RIG, ENDBIT);

    if (PLR_FLAGGED(ch, PLR_REMOTE)) {
      send_to_veh("The autonav beeps and the vehicle starts to move towards its destination.\r\n", veh, 0, FALSE);
      send_to_char("Gridguide activates and kicks you out of the system.\r\n", ch);
      PLR_FLAGS(ch).RemoveBit(PLR_REMOTE);
      ch->char_specials.rigging = FALSE;
      veh->rigger = NULL;
    } else
      send_to_veh("The autonav beeps and the vehicle starts to move towards its destination.\r\n", veh, 0, TRUE);

    return;
  }

  if (is_abbrev(arg, "delete")) {
    struct grid_data *temp;
    for (grid = veh->grid; grid; grid = grid->next)
      if (!str_cmp(buf2, grid->name))
        break;
    if (!grid)
      send_to_char("That destination doesn't seem to be in the system.\r\n", ch);
    else {
      REMOVE_FROM_LIST(grid, veh->grid, next);
      DELETE_ARRAY_IF_EXTANT(grid->name);
      DELETE_AND_NULL(grid);
      send_to_char("You remove the destination from the system.\r\n", ch);
      if (IS_RIGGING(ch)) {
        send_to_veh("The autonav flashes as a destination is deleted.\r\n", veh, NULL, FALSE);
      } else {
        act("$n punches something into the autonav.", FALSE, ch, 0 , 0, TO_ROOM);
      }
    }
  } else if (is_abbrev(arg, "add")) {
    int i = 0;
    for (grid = veh->grid; grid; grid = grid->next) {
      if (!str_cmp(buf2, grid->name)) {
        send_to_char("That entry already exists.\r\n", ch);
        return;
      }
      i++;
    }
    if (i >= GET_VEH_MAX_AUTONAV_SLOTS(veh)) {
      send_to_char("The system seems to be full.\r\n", ch);
      return;
    }
    if (!*argument) {
      if (!veh->in_room || !(ROOM_FLAGGED(veh->in_room, ROOM_ROAD) || ROOM_FLAGGED(veh->in_room, ROOM_GARAGE)) ||
          ROOM_FLAGGED(veh->in_room, ROOM_NOGRID)) {
        send_to_char("You aren't currently on the grid.\r\n", ch);
        return;
      }
      grid_vnum = veh->in_room->number;
    } else {
      two_arguments(argument, arg, buf);
      if (!*buf) {
        send_to_char("You need a second co-ordinate.\r\n", ch);
        return;
      }
      if (!((x = atol(arg)) && (y = atol(buf))) || (grid_vnum = vnum_from_gridguide_coordinates(x, y, ch)) <= 0) {
        send_to_char("Those co-ordinates seem invalid.\r\n", ch);
        return;
      }
    }
    grid = new grid_data;
    if (strlen(buf2) >= LINE_LENGTH)
      buf2[LINE_LENGTH] = '\0';
    grid->name = str_dup(buf2);
    grid->room = grid_vnum;
    grid->next = veh->grid;
    veh->grid = grid;
    send_to_char("You add the destination into the system.\r\n", ch);
    if (IS_RIGGING(ch)) {
      send_to_veh("The autonav flashes as a destination is added.\r\n", veh, NULL, FALSE);
    } else {
      act("$n punches something into the autonav.", FALSE, ch, 0 , 0, TO_ROOM);
    }
  }
  else {
    send_to_char("Sorry, that command wasn't recognized. You can 'gridguide add' or 'gridguide del'.\r\n", ch);
  }
}

void process_autonav(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  for (struct veh_data *veh = veh_list; veh; veh = veh->next) {
    bool veh_moved = FALSE;

    if (veh->in_room && veh->dest && veh->cspeed > SPEED_IDLE && veh->damage < VEH_DAM_THRESHOLD_DESTROYED) {
      struct char_data *ch = NULL;

      if (!(ch = veh->rigger))
        ch = veh->people;

      if (!ch) {
        veh->dest = NULL;
        veh->cspeed = SPEED_OFF;
        return;
      }

      int dir = 0;
      for (int x = MIN(MAX((int)get_speed(veh) / 10, 1), MAX_GRIDGUIDE_ROOMS_PER_PULSE); x && dir >= 0 && veh->dest; x--) {
        dir = find_first_step(real_room(veh->in_room->number), real_room(veh->dest->number), FALSE, "process_autonav", veh->people);
        if (dir >= 0) {
          veh_moved = TRUE;
          move_vehicle(ch, dir);
        }
      }
      if (!veh_moved) {
        send_to_veh("The autonav beeps and flashes a red 'ERROR' message before shutting off.\r\n", veh, 0, TRUE);
        veh->cspeed = SPEED_OFF;
        veh->dest = NULL;
        remove_vehicle_brain(veh);
        return;
      }
      if (veh->in_room == veh->dest) {
        send_to_veh("Having reached its destination, the autonav shuts off.\r\n", veh, 0, TRUE);
        veh->cspeed = SPEED_OFF;
        veh->dest = NULL;
        remove_vehicle_brain(veh);

        // QoL - show destination room to vehicle occupants on arrival
        for (struct char_data *ch = veh->people; ch; ch = ch->next_in_veh) {
          if (!IS_NPC(ch) && !PRF_FLAGGED(ch, PRF_SCREENREADER)) {
            look_at_room(ch, 0, 0);
          }
        }
      }
    }
  }
}

ACMD(do_switch)
{
  if (!ch->in_veh) {
    send_to_char("You can't switch positions when not in a vehicle.\r\n", ch);
    return;
  }

  else if (IS_WORKING(ch) || AFF_FLAGGED(ch, AFF_PILOT)) {
    send_to_char(TOOBUSY, ch);
    return;
  }

  else if (!ch->in_veh->seating[!ch->vfront] && !(repair_vehicle_seating(ch->in_veh) && ch->in_veh->seating[!ch->vfront])) {
    if (access_level(ch, LVL_ADMIN)) {
      send_to_char("You use your staff powers to bypass the lack of seating.\r\n", ch);
    } else {
      send_to_char("There's no room to move to.\r\n", ch);
      return;
    }
  }

  ch->in_veh->seating[ch->vfront]++;
  ch->in_veh->seating[!ch->vfront]--;
  ch->vfront = !ch->vfront;
  snprintf(buf, sizeof(buf), "$n climbs into the %s.", ch->vfront ? "front" : "back");
  act(buf, TRUE, ch, 0, 0, TO_ROOM);
  if (!PRF_FLAGGED(ch, PRF_SCREENREADER)) {
    look_at_room(ch, 0, 0);
  }
  send_to_char(ch, "You've climbed into the %s.\r\n", ch->vfront ? "front" : "back");
}

ACMD(do_pop)
{
  skip_spaces(&argument);
  struct veh_data *veh = ch->in_veh;
  if (veh && veh->hood) {
    send_to_char("You cannot close the hood from in here.\r\n", ch);
    return;
  }
  if (!veh) {
    FAILURE_CASE(!*argument, "Syntax: POP <target vehicle>");
    FAILURE_CASE_PRINTF(!(veh = get_veh_list(argument, ch->in_room->vehicles, ch)), "You don't see any vehicles named '%s' here.", argument);
  }
  if (!ch->in_veh && !veh->hood && veh->owner != GET_IDNUM(ch)) {
    if (access_level(ch, LVL_ADMIN)) {
      send_to_char("You use your staff powers to bypass the fact that this isn't your vehicle.\r\n", ch);
    } else {
      send_to_char("That's not your vehicle.\r\n", ch);
      return;
    }
  }

  if (veh->cspeed > SPEED_OFF) {
    send_to_char("It's moving too fast for you to do that.\r\n", ch);
    return;
  }

  if (veh->hood) {
    send_to_char(ch, "You close the hood of %s.\r\n", GET_VEH_NAME(veh));
    snprintf(buf, sizeof(buf), "$n closes the hood of %s.", GET_VEH_NAME(veh));
    act(buf, 0, ch, 0, 0, TO_ROOM);
    veh->hood = FALSE;
  } else {
    snprintf(buf, sizeof(buf), "$n pops the hood of %s.", GET_VEH_NAME(veh));
    act(buf, FALSE, ch, 0, 0, TO_ROOM);
    if (ch->in_veh) {
      snprintf(buf, sizeof(buf), "Someone pops the hood of %s.", GET_VEH_NAME(veh));
      if (ch->in_veh->in_room) {
        // No, these doubled if checks are not a bug to be condensed into one line, they're necessary. Note the error case.
        if (ch->in_veh->in_room->people) {
          act(buf, FALSE, ch->in_veh->in_room->people, 0, 0, TO_ROOM);
          act(buf, FALSE, ch->in_veh->in_room->people, 0, 0, TO_CHAR);
        }
      } else if (ch->in_veh->in_veh){
        if (ch->in_veh->in_veh->people) {
          send_to_veh(buf, ch->in_veh->in_veh, ch, TRUE);
        }
      } else {
        snprintf(buf, sizeof(buf), "SYSERR: Veh %s (%ld) has neither in_room nor in_veh!", GET_VEH_NAME(ch->in_veh), ch->in_veh->idnum);
        mudlog(buf, ch, LOG_SYSLOG, TRUE);
      }
    }
    send_to_char(ch, "You pop the hood of %s.\r\n", GET_VEH_NAME(veh));
    veh->hood = TRUE;

    // Clear the used load and recalculate it.
    {
      int old_load = veh->usedload;
      recalculate_vehicle_usedload(veh);

      if (veh->usedload != old_load) {
        if (veh->usedload < old_load) {
          send_to_char("Huh, someone must have stuffed some lead weights in here as a prank. You scoop them out and toss them aside.\r\n", ch);
        } else {
          send_to_char("A few helium balloons escape from under the hood...\r\n", ch);
        }
        mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Vehicle had unexpected usedload on pop (%d != %.2f)", old_load, veh->usedload);
      }
    }
  }
}

ACMD(do_tow)
{
  struct veh_data *veh = NULL, *tveh = NULL;
  if (!(AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE))) {
    send_to_char(ch, "You have to be %s to do that.\r\n", ch->in_veh ? "driving" : "controlling a vehicle");
    return;
  }
  RIG_VEH(ch, veh);
  if (veh->type != VEH_TRUCK && veh->type != VEH_DRONE) {
    send_to_char("You don't have enough power to tow anything.\r\n", ch);
    return;
  }
  if (veh->cspeed != SPEED_IDLE && veh->cspeed != SPEED_OFF) {
    send_to_char("You are moving too fast!\r\n", ch);
    return;
  }
  if (veh->towing) {
    // Compose our release message, which is emitted if the release processes successfully.
    strlcpy(buf3, GET_VEH_NAME(veh), sizeof(buf3));
    snprintf(buf, sizeof(buf), "%s releases %s from its towing equipment.\r\n", buf3, GET_VEH_NAME(veh->towing));
    send_to_char(ch, "You release %s from your towing equipment.\r\n", GET_VEH_NAME(veh->towing));

    if (veh->in_room) {
      if (veh->in_room->people) {
        act(buf, FALSE, veh->in_room->people, 0, 0, TO_ROOM);
        act(buf, FALSE, veh->in_room->people, 0, 0, TO_CHAR);
      }
      veh_to_room(veh->towing, veh->in_room);
      veh->towing = NULL;
    } else if (veh->in_veh){
      send_to_veh(buf, veh->in_veh, ch, TRUE);
      veh_to_veh(veh->towing, veh->in_veh);
    } else {
      snprintf(buf, sizeof(buf), "SYSERR: Veh %s (%ld) has neither in_room nor in_veh!", GET_VEH_NAME(veh), veh->idnum);
      send_to_char("The game system encountered an error. Tow not released.\r\n", ch);
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      return;
    }

    veh->towing = NULL;
    return;
  }
  skip_spaces(&argument);
  if (!(tveh = get_veh_list(argument, veh->in_room ? veh->in_room->vehicles : veh->in_veh->carriedvehs, ch))) {
    send_to_char(ch, "You don't see any vehicles named '%s' here.\r\n", argument);
    return;
  }

  // Purge the vehicle brain if one exists (otherwise it can't be towed)
  remove_vehicle_brain(tveh);

  if (tveh->type == VEH_BIKE)
    send_to_char("Try as you might, you can't seem to balance it. You'll have to ^WPUSH^n that into the back instead.\r\n", ch);
  else if (tveh->locked && tveh->type != VEH_DRONE)
    send_to_char("That vehicle won't budge until it's unlocked.\r\n", ch);
  else if (tveh->people)
    send_to_char("Towing a car full of people isn't very safe.\r\n", ch);
  else if (tveh->cspeed > SPEED_OFF)
    send_to_char("The vehicle has to be off for you to tow it.\r\n", ch);
  else if (veh->type == VEH_DRONE && tveh->type != VEH_DRONE)
    send_to_char("Drones can only tow other drones.\r\n", ch);
  else if (veh->type == VEH_DRONE && ((veh->load - veh->usedload) * 2.5) <= calculate_vehicle_weight(tveh))
    send_to_char("That drone is too heavy for your drone to tow.\r\n", ch);
  else if (veh->towing)
    send_to_char("Towing a vehicle that's towing another vehicle isn't very safe!\r\n", ch);
  else {
    // If anyone is attacking the vehicle, you can't tow it.
    for (struct char_data *check = tveh->in_room ? tveh->in_room->people : tveh->in_veh->people;
         check;
         check = tveh->in_room ? check->next_in_room : check->next_in_veh)
    {
      if (FIGHTING_VEH(check) == veh) {
        send_to_char(ch, "You can't tow something while under fire!\r\n");
        return;
      }
      if (FIGHTING_VEH(check) == tveh) {
        send_to_char(ch, "You can't tow a vehicle that's under fire!\r\n");
        return;
      }
    }
    send_to_char(ch, "You pick up %s with your towing equipment.\r\n", GET_VEH_NAME(tveh));
    strlcpy(buf3, GET_VEH_NAME(veh), sizeof(buf3));
    snprintf(buf, sizeof(buf), "%s picks up %s with its towing equipment.\r\n", buf3, GET_VEH_NAME(tveh));
    if (veh->in_room) {
      if (veh->in_room->people) {
        act(buf, FALSE, veh->in_room->people, 0, 0, TO_ROOM);
        act(buf, FALSE, veh->in_room->people, 0, 0, TO_CHAR);
      }
    } else if (veh->in_veh){
      send_to_veh(buf, veh->in_veh, ch, TRUE);
    } else {
      snprintf(buf, sizeof(buf), "SYSERR: Veh %s (%ld) has neither in_room nor in_veh!", GET_VEH_NAME(veh), veh->idnum);
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
    }
    veh->towing = tveh;
    veh_from_room(tveh);
  }
}

#define CH d->character
#define VEHCUST_NAME	0
#define VEHCUST_MAIN	1
#define VEHCUST_DESC	2

void vehcust_menu(struct descriptor_data *d)
{
  send_to_char(CH, "\033[H\033[J1) Vehicle Name: ^c%s^n\r\n"
                   "2) Vehicle Description: \r\n^c%s^n\r\n"
                   "q) Finalize\r\n"
                   "Enter Choice: ", d->edit_veh->restring, d->edit_veh->restring_long);
  d->edit_mode = VEHCUST_MAIN;
}

void vehcust_parse(struct descriptor_data *d, char *arg)
{
  switch (d->edit_mode)
  {
    case VEHCUST_MAIN:
      switch (LOWER(*arg)) {
        case '1':
          send_to_char("Enter new vehicle name: ", CH);
          d->edit_mode = VEHCUST_NAME;
          break;
        case '2':
          send_to_char(CH, "Enter new vehicle description:\r\n");
          d->edit_mode = VEHCUST_DESC;
          DELETE_D_STR_IF_EXTANT(d);
          d->str = new (char *);
          *(d->str) = NULL;
          d->max_str = MAX_MESSAGE_LENGTH;
          d->mail_to = 0;
          break;
        case 'q':
        case 'Q':
          STATE(d) = CON_PLAYING;
          PLR_FLAGS(CH).RemoveBit(PLR_WRITING);
          d->edit_veh = NULL;
          send_to_char("Your ride will be ready within the next 15 minutes.\r\n", CH);
          break;
      }
      break;
    case VEHCUST_NAME:
      if (get_string_length_after_color_code_removal(arg, CH) >= LINE_LENGTH) {
        send_to_char(CH, "That name is too long. Please limit it to %d characters (ignoring color codes).\r\n", LINE_LENGTH - 1);
        vehcust_menu(d);
        return;
      }
      DELETE_ARRAY_IF_EXTANT(d->edit_veh->restring);
      d->edit_veh->restring = str_dup(arg);
      vehcust_menu(d);
      break;
  }

}

ACMD(do_push)
{
  skip_spaces(&argument);
  struct veh_data *veh = NULL, *found_veh = NULL;

  FAILURE_CASE(!*argument, "You need to specify something to push.");
  FAILURE_CASE(IS_ASTRAL(ch), "You can't affect the material plane.");

  if (ch->in_veh) {
    if (ch->vfront)
      send_to_char("You can't push a vehicle out from here.\r\n", ch);
    else if (ch->in_veh->cspeed > SPEED_IDLE)
      send_to_char("You are moving too fast to do that.\r\n", ch);
    else if (!(veh = get_veh_list(argument, ch->in_veh->carriedvehs, ch)))
      send_to_char("That vehicle isn't in here.\r\n", ch);
    else {
      strlcpy(buf3, GET_VEH_NAME(veh), sizeof(buf3));
      send_to_char(ch, "You push %s out of the back.\r\n", buf3);
      snprintf(buf, sizeof(buf), "$n pushes %s out of the back.", buf3);
      snprintf(buf2, sizeof(buf2), "$n pushes %s out of the back of %s.", buf3, GET_VEH_NAME(ch->in_veh));
      act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
      if (ch->in_veh->in_room) {
        act(buf2, FALSE, ch, 0, 0, TO_VEH_ROOM);
      } else if (ch->in_veh->in_veh){
        // TODO: test this, doesn't it send '$n pushes...'?
        send_to_veh(buf, ch->in_veh->in_veh, ch, TRUE);
      } else {
        snprintf(buf, sizeof(buf), "SYSERR: Veh %s (%ld) has neither in_room nor in_veh!", GET_VEH_NAME(ch->in_veh), ch->in_veh->idnum);
        mudlog(buf, ch, LOG_SYSLOG, TRUE);
      }
      if (ch->in_veh->in_room)
        veh_to_room(veh, ch->in_veh->in_room);
      else
        veh_to_veh(veh, ch->in_veh->in_veh);
    }
  } else {
    two_arguments(argument, buf, buf1);
    if (!*buf || !*buf1) {
      send_to_char("Push what?\r\n", ch);
      return;
    }
    if (!(veh = get_veh_list(buf, ch->in_room->vehicles, ch)) || !(found_veh = get_veh_list(buf1, ch->in_room->vehicles, ch))) {
      send_to_char("You don't see that vehicle here.\r\n", ch);
      return;
    }

    FAILURE_CASE(found_veh == veh, "You can't push it into itself.");
    FAILURE_CASE(!found_veh->seating[0] && !(repair_vehicle_seating(found_veh) && found_veh->seating[0]), "There's nowhere to push it into.");
    FAILURE_CASE(found_veh->load - found_veh->usedload < calculate_vehicle_entry_load(veh), "There is not enough room in there for that.");
    FAILURE_CASE(found_veh->locked, "You can't push it into a locked vehicle.");
    FAILURE_CASE(veh->locked && veh->damage < VEH_DAM_THRESHOLD_DESTROYED, "The wheels seem to be locked.");
    FAILURE_CASE(found_veh->damage >= VEH_DAM_THRESHOLD_DESTROYED, "You can't push anything into a destroyed vehicle.");

    strlcpy(buf2, GET_VEH_NAME(veh), sizeof(buf2));
    snprintf(buf, sizeof(buf), "$n pushes %s into the back of %s.", buf2, GET_VEH_NAME(found_veh));
    send_to_char(ch, "You push %s into the back of %s.\r\n", buf2, GET_VEH_NAME(found_veh));
    act(buf, 0, ch, 0, 0, TO_ROOM);
    snprintf(buf2, sizeof(buf2), "You are pushed into %s.\r\n", GET_VEH_NAME(found_veh));
    send_to_veh(buf2, veh, NULL, TRUE);
    veh_to_veh(veh, found_veh);
  }
}

ACMD(do_transfer)
{
  two_arguments(argument, buf, buf2);
  struct veh_data *veh = NULL;
  struct char_data *targ = NULL;

  if (!*buf || !*buf2)
    send_to_char("Transfer what to who?\r\n", ch);
  else if (ch->in_veh)
    send_to_char("You can't transfer ownership while you're inside a vehicle.\r\n", ch);
  else if (!(veh = get_veh_list(buf, ch->in_room->vehicles, ch))) 
    send_to_char("You don't see that vehicle here.\r\n", ch);
  else if (veh->owner != GET_IDNUM(ch))
    send_to_char("You can't transfer ownership of a vehicle you don't own.\r\n", ch);
  else if (!(targ = get_char_room_vis(ch, buf2)))
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf2);
  else if (!targ->desc || IS_NPC(targ))
    send_to_char(ch, "There's nothing going on behind %s's eyes.\r\n", GET_CHAR_NAME(targ));
  else {
    // Unsub it.
    if (veh->sub) {
      remove_veh_from_chs_subscriber_list(veh, ch, "do_transfer");
      // r_v_f_c_s_l() tries to avoid clobbering things so may not guarantee veh->sub is unset. Let's ensure it.
      veh->sub = FALSE;
    }

    snprintf(buf, sizeof(buf), "You transfer ownership of %s to $N.", GET_VEH_NAME(veh));
    snprintf(buf2, sizeof(buf2), "$n transfers ownership of %s to you.", GET_VEH_NAME(veh));
    act(buf, 0, ch, 0, targ, TO_CHAR);
    act(buf2, 0, ch, 0, targ, TO_VICT);
    set_veh_owner(veh, GET_IDNUM(targ));
    veh->sub = FALSE;
    veh->sub_rank = 0;

    if (ch->desc && targ->desc) {
      rectify_desc_host(ch->desc);
      rectify_desc_host(targ->desc);

      if (!strcmp(ch->desc->host, targ->desc->host)) {
        mudlog_vfprintf(ch, LOG_CHEATLOG, "Transfered ownership of vehicle '%s' (%ld) to SAME HOST character %s (%ld) [host %s].", 
                      GET_VEH_NAME(veh), veh->idnum, GET_CHAR_NAME(targ), GET_IDNUM(targ), ch->desc->host);
      } else {
        mudlog_vfprintf(ch, LOG_CHEATLOG, "Transfered ownership of vehicle '%s' (%ld) to %s (%ld).", 
                      GET_VEH_NAME(veh), veh->idnum, GET_CHAR_NAME(targ), GET_IDNUM(targ));
      }
    } else {
      mudlog_vfprintf(ch, LOG_CHEATLOG, "Transfered ownership of vehicle '%s' (%ld) to %s (%ld).", 
                      GET_VEH_NAME(veh), veh->idnum, GET_CHAR_NAME(targ), GET_IDNUM(targ));
    }

    save_single_vehicle(veh);
  }
}

ACMD(stop_rigging_first) {
  send_to_char(ch, "You'll need to stop rigging by using the %s command before you can do that.\r\n", AFF_FLAGGED(ch, AFF_RIG) ? "^WRIG^n" : "^WRETURN^n");
}

// Iterates over all vehicles, removing NPC ones that have been destroyed and sitting around for a while.
void process_vehicle_decay(void) {
  // Add instrumentation for easier tracking.
  PERF_PROF_SCOPE(pr_, __func__);

  // Declare our tracking variable to help us maintain loop cohesion while removing elements we're iterating over.
  struct veh_data *next_veh;

  // Loop over all the vehicles in the game.
  for (struct veh_data *veh = veh_list; veh; veh = next_veh) {
    // We know we're extracting vehicles in this loop, so track veh->next in a lasting manner.
    next_veh = veh->next;

    // If the vehicle we're looking at doesn't meet our preconditions, reset its timer and go to the next vehicle.
    if (veh->damage < VEH_DAM_THRESHOLD_DESTROYED || veh->owner || veh->in_veh || !veh->in_room || ROOM_FLAGGED(veh->in_room, ROOM_GARAGE)) {
      GET_VEH_DESTRUCTION_TIMER(veh) = 0;
      continue;
    }

    // Check to see if the vehicle is still decaying. If so, increment but do nothing else.
    if (GET_VEH_DESTRUCTION_TIMER(veh) < max_npc_vehicle_lootwreck_time) {
      GET_VEH_DESTRUCTION_TIMER(veh)++;
    }
    // Vehicle has finished decaying. Extract it.
    else {
      snprintf(buf, sizeof(buf), "A hulking utility forklift drives up, lifts the remains of %s up, and drives off into the sunset.\r\n", GET_VEH_NAME(veh));
      send_to_room(buf, get_veh_in_room(veh));
      extract_veh(veh);
    }
  }
}

ACMD(do_vehicle_osay) {
  struct veh_data *veh;
  RIG_VEH(ch, veh);

  if (!veh) {
    return;
  }

  skip_spaces(&argument);

  snprintf(buf, sizeof(buf), "%s^n says ^mOOCly^n, \"%s^n\"\r\n", GET_VEH_NAME(veh), capitalize(argument));

  if (veh->in_room)
    send_to_room(buf, veh->in_room, veh);
  else
    send_to_veh(buf, veh->in_veh, ch, TRUE);

  send_to_char(ch, "You say ^mOOCly^n, \"%s^n\".\r\n", capitalize(argument));
}

int get_vehicle_modifier(struct veh_data *veh, bool include_weather)
{
  struct char_data *ch;
  struct obj_data *cyber;
  int mod = 0;

  if (!(ch = veh->rigger))
    for (ch = veh->people; ch; ch = ch->next_in_veh)
      if (AFF_FLAGGED(ch, AFF_PILOT))
        break;

  if (veh->cspeed > SPEED_IDLE && ch)
  {
    // Subtract VCR rating from handling.
    for (cyber = ch->cyberware; cyber; cyber = cyber->next_content)
      if (GET_OBJ_VAL(cyber, 0) == CYB_VCR) {
        mod -= GET_OBJ_VAL(cyber, 1);
        break;
      }
    // If no VCR, but datajack, sub that.
    if (mod == 0 && AFF_FLAGGED(ch, AFF_RIG))
      mod--;
    // If it's faster than you can react to, the TN goes up.
    if (get_speed(veh) > (int)(GET_REA(ch) * 20)) {
      if (get_speed(veh) > (int)(GET_REA(ch) * 40))
        mod += 3;
      else if (get_speed(veh) < (int)(GET_REA(ch) * 40) && get_speed(veh) > (int)(GET_REA(ch) * 30))
        mod += 2;
      else
        mod++;
    }
  }
  // Damaged? TN goes up.
  switch (veh->damage)
  {
  case VEH_DAM_THRESHOLD_LIGHT:
  case 2:
    mod += 1;
    break;
  case VEH_DAM_THRESHOLD_MODERATE:
  case 4:
  case 5:
    mod += 2;
    break;
  case VEH_DAM_THRESHOLD_SEVERE:
  case 7:
  case 8:
  case 9:
    mod += 3;
    break;
  }
  // Weather makes it harder to drive too.
  if (include_weather && weather_info.sky >= SKY_RAINING)
    mod++;
  return mod;
}

// Call when the rigger or driver is no longer available.
void stop_vehicle(struct veh_data *veh) {
  // If it's not under autopilot, shut it down.
  if (!veh->dest && veh->cspeed > SPEED_OFF) {
    // Message vehicle about it stopping if it's actually moving.
    if (veh->cspeed > SPEED_IDLE)
      send_to_veh("You slow to a halt.\r\n", veh, NULL, 0);
    // Park it.
    veh->cspeed = SPEED_OFF;
  }

  // Clear rigger info.
  veh->rigger = NULL;

  // Clear chasing info.
  stop_chase(veh);
}

void stop_rigging(struct char_data *ch) {
  if (!ch)
    return;

  struct veh_data *veh = ch->char_specials.rigging ? ch->char_specials.rigging : ch->in_veh;

  if (!veh)
    return;

  // Stop their vehicle.
  stop_vehicle(veh);

  // Clear their pointers and flags.
  ch->char_specials.rigging = NULL;
  PLR_FLAGS(ch).RemoveBit(PLR_REMOTE);
  AFF_FLAGS(ch).RemoveBit(AFF_RIG);
  AFF_FLAGS(ch).RemoveBit(AFF_PILOT);

  // Take them out of combat (does not depend on above pointers/flags)
  stop_fighting(ch);

  send_to_char("You return to your senses.\r\n", ch);

  struct obj_data *jack = get_datajack(ch, TRUE);
  if (GET_OBJ_TYPE(jack) == ITEM_CYBERWARE) {
    if (GET_CYBERWARE_TYPE(jack) == CYB_DATAJACK) {
      if (GET_CYBERWARE_FLAGS(jack) == DATA_INDUCTION) {
        snprintf(buf, sizeof(buf), "$n slowly removes $s hand from the induction pad%s.", veh == ch->in_veh ? ", relinquishing control" : "");
      } else {
        snprintf(buf, sizeof(buf), "$n carefully removes the jack from $s head%s.", veh == ch->in_veh ? ", relinquishing control" : "");
      }
    } else {
      snprintf(buf, sizeof(buf), "$n carefully removes the jack from $s eye%s.", veh == ch->in_veh ? ", relinquishing control" : "");
    }
  } else {
    // We should never see this.
    snprintf(buf, sizeof(buf), "$n undoes the leads of $s 'trode net%s.", veh == ch->in_veh ? ", relinquishing control" : "");
  }
  act(buf, TRUE, ch, 0, 0, TO_ROOM);
}

// Only call this if you're SURE you want them to stop driving. Check for flight status etc.
void stop_driving(struct char_data *ch, bool is_involuntary) {
  if (!ch || !ch->in_veh)
    return;

  if (is_involuntary) {
    send_to_char("The controls slip from your unresponsive fingers.\r\n", ch);
    snprintf(buf1, sizeof(buf1), "%s slumps, the controls slipping from %s fingers.\r\n", capitalize(GET_NAME(ch)), HSHR(ch));
    send_to_veh(buf1, VEH, ch, FALSE);
    
    if (ch->in_veh->cspeed > SPEED_IDLE) {
      // Crash test with no driver -- they've gone unconscious or something.
      crash_test(ch, TRUE);
    }
  } else {
    send_to_char("You relinquish the driver's seat.\r\n", ch);
    snprintf(buf1, sizeof(buf1), "%s relinquishes the driver's seat.\r\n", capitalize(GET_NAME(ch)));
    send_to_veh(buf1, VEH, ch, FALSE);
  }

  if (AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE)) {
    stop_rigging(ch);
  } else if (AFF_FLAGGED(ch, AFF_PILOT)) {
    stop_vehicle(ch->in_veh);
    AFF_FLAGS(ch).RemoveBit(AFF_PILOT);
  }
}