#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "house.h"
#include "transport.h"
#include "constants.h"
#include "limits.h"

void die_follower(struct char_data *ch);
void roll_individual_initiative(struct char_data *ch);
void order_list(struct char_data *start);
int find_first_step(vnum_t src, vnum_t target);
int move_vehicle(struct char_data *ch, int dir);
ACMD_CONST(do_return);

#define VEH ch->in_veh

int get_maneuver(struct veh_data *veh)
{
  int score = -2;
  struct char_data *ch = NULL;
  int i = 0, x = 0, skill;

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
      score +=5;
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
    for (skill = veh_skill(ch, veh);skill > 0; skill--) {
      i = srdice();
      if (i > x)
        x = i;
    }
    score += x;
  }
  return score;
}

int modify_veh(struct veh_data *veh)
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
    for (cyber = ch->cyberware; cyber; cyber = cyber->next_content)
      if (GET_OBJ_VAL(cyber, 0) == CYB_VCR) {
        mod -= GET_OBJ_VAL(cyber, 1);
        break;
      }
    if (mod == 0 && AFF_FLAGGED(ch, AFF_RIG))
      mod--;
    if (get_speed(veh) > (int)(GET_REA(ch) * 20)) {
      if (get_speed(veh) > (int)(GET_REA(ch) * 40))
        mod += 3;
      else if (get_speed(veh) < (int)(GET_REA(ch) * 40) && get_speed(veh) > (int)(GET_REA(ch) * 30))
        mod += 2;
      else
        mod++;
    }
  }
  switch (veh->damage)
  {
  case 1:
  case 2:
    mod += 1;
    break;
  case 3:
  case 4:
  case 5:
    mod += 2;
    break;
  case 6:
  case 7:
  case 8:
  case 9:
    mod += 3;
    break;
  }
  if (weather_info.sky >= SKY_RAINING)
    mod++;
  return mod;
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

void crash_test(struct char_data *ch)
{
  int target = 0, skill = 0;
  int power, attack_resist = 0, damage_total = SERIOUS;
  struct veh_data *veh;
  struct char_data *tch, *next;

  RIG_VEH(ch, veh);
  target = veh->handling + modify_veh(veh) + modify_target(ch);
  power = (int)(ceilf(get_speed(veh) / 10));

  sprintf(buf, "%s begins to lose control!\r\n", GET_VEH_NAME(veh));
  send_to_room(buf, veh->in_room);

  send_to_char("You begin to lose control!\r\n", ch);
  skill = veh_skill(ch, veh) + veh->autonav;

  if (success_test(skill, target))
  {
    send_to_char("You get your ride under control.\r\n", ch);
    return;
  }
  send_to_veh("You careen off the road!\r\n", veh, NULL, TRUE);
  sprintf(buf, "A %s careens off the road!\r\n", GET_VEH_NAME(veh));
  send_to_room(buf, veh->in_room);

  attack_resist = success_test(veh->body, power) * -1;

  int staged_damage = stage(attack_resist, damage_total);
  damage_total = convert_damage(staged_damage);

  veh->damage += damage_total;
  stop_chase(veh);
  if (veh->type == VEH_BIKE && veh->people)
  {
    power = (int)(get_speed(veh) / 10);
    for (tch = veh->people; tch; tch = next) {
      next = tch->next_in_veh;
      char_from_room(tch);
      char_to_room(tch, veh->in_room);
      damage_total = convert_damage(stage(0 - success_test(GET_BOD(tch), power), MODERATE));
      damage(tch, tch, damage_total, TYPE_CRASH, PHYSICAL);
      AFF_FLAGS(tch).RemoveBits(AFF_PILOT, AFF_RIG, ENDBIT);
      send_to_char("You are thrown from the bike!\r\n", tch);
    }
    veh->cspeed = SPEED_OFF;
  }
}

ACMD(do_drive)
{
  struct char_data *temp;

  if (IS_ASTRAL(ch)) {
    send_to_char("You cannot seem to touch physical objects.\r\n", ch);
    return;
  }
  if (!VEH) {
    send_to_char("You have to be in a vehicle to do that.\r\n", ch);
    return;
  }
  if (!ch->vfront) {
    send_to_char("Nobody likes a backseat driver.\r\n", ch);
    return;
  }
  if (AFF_FLAGGED(ch, AFF_RIG)) {
    send_to_char("You are already rigging the vehicle.\r\n", ch);
    return;
  }
  if (IS_WORKING(ch) && !AFF_FLAGGED(ch, AFF_PILOT)) {
    send_to_char(TOOBUSY, ch);
    return;
  }
  if(GET_SKILL(ch, SKILL_PILOT_CAR) == 0 && GET_SKILL(ch, SKILL_PILOT_BIKE) == 0 &&
      GET_SKILL(ch, SKILL_PILOT_TRUCK) == 0) {
    send_to_char("You have no idea how to do that.\r\n", ch);
    return;
  }
  if (VEH->damage >= 10) {
    send_to_char("This vehicle is too much of a wreck to move!\r\n", ch);
    return;
  }
  if (VEH->rigger || VEH->dest) {
    send_to_char("You can't seem to take control!\r\n", ch);
    return;
  }
  for(temp = VEH->people; temp; temp= temp->next_in_veh)
    if(ch != temp && AFF_FLAGGED(temp, AFF_PILOT)) {
      send_to_char("Someone is already in charge!\r\n", ch);
      return;
    }
  if (VEH->type == VEH_BIKE && VEH->locked && GET_IDNUM(ch) != VEH->owner) {
    send_to_char("You can't seem to start it.\r\n", ch);
    return;
  }
  if (VEH->cspeed == SPEED_OFF || VEH->dest) {
    AFF_FLAGS(ch).ToggleBit(AFF_PILOT);
    send_to_char("The wheel is in your hands.\r\n", ch);
    sprintf(buf1, "%s takes the wheel.\r\n", GET_NAME(ch));
    VEH->cspeed = SPEED_IDLE;
    VEH->lastin[0] = VEH->in_room;
    send_to_veh(buf1, VEH, ch, FALSE);
    if (AFF_FLAGGED(ch, AFF_MANNING)) {
      struct obj_data *mount;
      for (mount = ch->in_veh->mount; mount; mount = mount->next_content)
        if (mount->worn_by == ch)
          break;
      mount->worn_by = NULL;
      AFF_FLAGS(ch).RemoveBit(AFF_MANNING);
    }
  } else {
    AFF_FLAGS(ch).ToggleBit(AFF_PILOT);
    send_to_char("You relinquish the driver's seat.\r\n", ch);
    sprintf(buf1, "%s relinquishes the driver's seat.\r\n", GET_NAME(ch));
    stop_chase(VEH);
    if (!VEH->dest)
      VEH->cspeed = SPEED_OFF;
    send_to_veh(buf1, VEH, ch, FALSE);
  }
  return;
}

ACMD(do_rig)
{
  struct char_data *temp;
  struct obj_data *cyber;
  int has_datajack = 0;

  if (IS_ASTRAL(ch)) {
    send_to_char("You cannot seem to touch physical objects.\r\n", ch);
    return;
  }
  if (!VEH) {
    send_to_char("You have to be in a vehicle to do that.\r\n", ch);
    return;
  }
  if (!ch->vfront) {
    send_to_char("Nobody likes a backseat driver.\r\n", ch);
    return;
  }
  if (IS_WORKING(ch) && !AFF_FLAGGED(ch, AFF_RIG)) {
    send_to_char(TOOBUSY, ch);
    return;
  }
  for (cyber = ch->cyberware; cyber; cyber = cyber->next_content)
    if (GET_OBJ_VAL(cyber, 0) == CYB_DATAJACK || (GET_OBJ_VAL(cyber, 0) == CYB_EYES && IS_SET(GET_OBJ_VAL(cyber, 3), EYE_DATAJACK))) {
      has_datajack = TRUE;
      break;
    }
  if (!has_datajack) {
    send_to_char("You need atleast a datajack to do that.\r\n", ch);
    return;
  }

  if (GET_OBJ_VAL(cyber, 0) == CYB_DATAJACK && GET_OBJ_VAL(cyber, 3) == DATA_INDUCTION) {
    if (GET_EQ(ch, WEAR_HANDS)) {
      send_to_char(ch, "Try removing your gloves first.\r\n");
      return;
    }
  } else if (GET_EQ(ch, WEAR_HEAD) && GET_OBJ_VAL(GET_EQ(ch, WEAR_HEAD), 7) > 0) {
    send_to_char(ch, "Try removing your helmet first.\r\n");
    return;
  }

  if (GET_SKILL(ch, SKILL_PILOT_CAR) == 0 && GET_SKILL(ch, SKILL_PILOT_BIKE) == 0 &&
      GET_SKILL(ch, SKILL_PILOT_TRUCK) == 0) {
    send_to_char("You have no idea how to do that.\r\n", ch);
    return;
  }
  if (VEH->damage >= 10) {
    send_to_char("This vehicle is too much of a wreck to move!\r\n", ch);
    return;
  }
  if (VEH->rigger || VEH->dest) {
    send_to_char("The system is unresponsive!\r\n", ch);
    return;
  }
  for(temp = VEH->people; temp; temp= temp->next_in_veh)
    if(ch != temp && AFF_FLAGGED(temp, AFF_PILOT)) {
      send_to_char("Someone is already in charge!\r\n", ch);
      return;
    }
  if(VEH->cspeed == SPEED_OFF || VEH->dest) {
    if (VEH->type == VEH_BIKE && VEH->locked && GET_IDNUM(ch) != VEH->owner) {
      send_to_char("You jack in and nothing happens.\r\n", ch);
      return;
    } 
    AFF_FLAGS(ch).ToggleBits(AFF_PILOT, AFF_RIG, ENDBIT);
    send_to_char("As you jack in, your perception shifts.\r\n", ch);
    sprintf(buf1, "%s jacks into the vehicle control system.\r\n", GET_NAME(ch));
    VEH->cspeed = SPEED_IDLE;
    VEH->lastin[0] = VEH->in_room;
    send_to_veh(buf1, VEH, ch, TRUE);
    if (AFF_FLAGGED(ch, AFF_MANNING)) {
      struct obj_data *mount;
      for (mount = ch->in_veh->mount; mount; mount = mount->next_content)
        if (mount->worn_by == ch)
          break;
      mount->worn_by = NULL;
      AFF_FLAGS(ch).RemoveBit(AFF_MANNING);
    }
  } else {
    if (!AFF_FLAGGED(ch, AFF_RIG)) {
      send_to_char("But you're not rigging.\r\n", ch);
      return;
    }
    AFF_FLAGS(ch).ToggleBits(AFF_PILOT, AFF_RIG, ENDBIT);
    if (!VEH->dest)
      VEH->cspeed = SPEED_OFF;
    stop_chase(VEH);
    send_to_char("You return to your senses.\r\n", ch);
    sprintf(buf1, "%s returns to their senses.\r\n", GET_NAME(ch));
    send_to_veh(buf1, VEH, ch, FALSE);
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
  sprintf(buf, "%s%s.\r\n", GET_VEH_NAME(veh), argument);
  send_to_room(buf, veh->in_room);
  sprintf(buf, "Your vehicle%s.\r\n", argument);
  send_to_veh(buf, veh, NULL, TRUE);
  return;
}

ACMD(do_ram)
{
  struct veh_data *veh, *tveh = NULL;
  struct char_data *vict = NULL;
  int skill = 0, target = 0, tvehm = 0, vehm = 0;

  if (!(AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE))) {
    send_to_char("You need to be driving a vehicle to do that...\r\n", ch);
    return;
  }
  if (FIGHTING(ch) || FIGHTING_VEH(ch)) {
    send_to_char(ch, "You're in the middle of combat!\r\n");
    return;
  }
  RIG_VEH(ch, veh);
  if (get_speed(veh) <= SPEED_IDLE) {
    send_to_char("You're moving far too slowly.\r\n", ch);
    return;
  }
  if (world[veh->in_room].peaceful) {
    send_to_char("This room just has a peaceful, easy feeling...\r\n", ch);
    return;
  }
  two_arguments(argument, arg, buf2);

  if (!*arg) {
    send_to_char("Ram what?", ch);
    return;
  }
  if (!(vict = get_char_room(arg, veh->in_room)) &&
      !(tveh = get_veh_list(arg, world[veh->in_room].vehicles, ch))) {
    send_to_char("You can't seem to find the target you're looking for.\r\n", ch);
    return;
  }
  if (vict && !INVIS_OK(ch, vict)) {
    send_to_char("You can't seem to find the target you're looking for.\r\n", ch);
    return;
  }
  if (vict && !IS_NPC(vict) && !(PRF_FLAGGED(ch, PRF_PKER) && PRF_FLAGGED(vict, PRF_PKER))) {
    send_to_char(ch, "You have to be flagged PK to attack another player.\r\n");
    return;
  }
  if (tveh == veh) {
    send_to_char("You can't ram yourself!\r\n", ch);
    return;
  }

  skill = veh_skill(ch, veh);
  if (tveh) {
    target = modify_veh(veh) + veh->handling + modify_target(ch);
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
    sprintf(buf, "%s heads straight towards your ride.\r\n", GET_VEH_NAME(veh));
    sprintf(buf1, "%s heads straight towards %s.\r\n", GET_VEH_NAME(veh), GET_VEH_NAME(tveh));
    sprintf(buf2, "You attempt to ram %s.\r\n", GET_VEH_NAME(tveh));
    send_to_veh(buf, tveh, 0, TRUE);
    send_to_room(buf1, veh->in_room);
    send_to_char(buf2, ch);
  } else {
    target = modify_veh(veh) + veh->handling + modify_target(ch);
    sprintf(buf, "%s heads straight towards you.", GET_VEH_NAME(veh));
    sprintf(buf1, "%s heads straight towards $n.", GET_VEH_NAME(veh));
    act(buf, FALSE, vict, 0, 0, TO_CHAR);
    act(buf1, FALSE, vict, 0, 0, TO_ROOM);
    act("You head straight towards $N.", FALSE, ch, 0, vict, TO_CHAR);
    if (success_test(GET_REA(vict), 4)) {
      send_to_char(ch, "You quickly let out an attack at it!\r\n");
      act("$n quickly lets an attack fly.", FALSE, vict, 0, 0, TO_ROOM);
      act("$N suddenly attacks!", FALSE, ch, 0, vict, TO_CHAR);
      AFF_FLAGS(vict).SetBit(AFF_COUNTER_ATT);
      vcombat(vict, veh);
      AFF_FLAGS(vict).RemoveBit(AFF_COUNTER_ATT);
      if (veh->damage > 9)
        return;
    }
  }
  int success = success_test(skill, target);
  if (vict) {
    target = 4 + damage_modifier(vict, buf);
    success -= success_test(GET_DEFENSE(vict), target);
  }
  if (success > 0)
    if (vict)
      vram(veh, vict, NULL);
    else
      vram(veh, NULL, tveh);
  else {
    crash_test(ch);
    chkdmg(veh);
  }
}

ACMD(do_upgrade)
{
  struct veh_data *veh;
  struct obj_data *mod, *obj, *shop = NULL;
  int j = 0, skill = 0, target = 0, kit = 0;

  half_chop(argument, buf1, buf2);

  if (!*buf1) {
    send_to_char("What do you want to upgrade?\r\n", ch);
    return;
  }
  if (ch->in_veh && (ch->vfront || !ch->in_veh->flags.IsSet(VFLAG_WORKSHOP))) {
    send_to_char("You don't have any more music to play on the radio.\r\n", ch);
    return;
  }
  if (!(veh = get_veh_list(buf1, ch->in_veh ? ch->in_veh->carriedvehs : world[ch->in_room].vehicles, ch))) {
    send_to_char(NOOBJECT, ch);
    return;
  }

  if (!*buf2) {
    send_to_char("You have to upgrade it with something!\r\n", ch);
    return;
  } else if (!(mod = get_obj_in_list_vis(ch, buf2, ch->carrying))) {
    send_to_char("You don't seem to have that item.\r\n", ch);
    return;
  }
  if (GET_OBJ_TYPE(mod) != ITEM_MOD) {
    send_to_char(ch, "That is not a vehicle modification.\r\n");
    return;
  }
  if (!IS_NPC(ch)) {
    switch(veh->type) {
    case VEH_DRONE:
      skill = SKILL_BR_DRONE;
      break;
    case VEH_BIKE:
      skill = SKILL_BR_BIKE;
      break;
    case VEH_CAR:
      skill = SKILL_BR_CAR;
      break;
    case VEH_TRUCK:
      skill = SKILL_BR_TRUCK;
      break;
    }
    switch (GET_OBJ_VAL(mod, 0)) {
      case TYPE_ENGINECUST:
        target = 6;
        break;
      case TYPE_TURBOCHARGER:
        target = 2 + GET_OBJ_VAL(mod, 2);
        break;
      case TYPE_AUTONAV:
        target = 8 - veh->handling;
        break;
      case TYPE_CMC:
      case TYPE_DRIVEBYWIRE:
        target = 10 - veh->handling;
        break;
      case TYPE_ARMOUR:
      case TYPE_CONCEALEDARMOUR:
        target = (int)((GET_OBJ_VAL(mod, 2) + (GET_MOD(veh, GET_OBJ_VAL(mod, 6)) ? GET_OBJ_VAL(GET_MOD(veh, GET_OBJ_VAL(mod, 6)), 2) : 0))/ 3);
        break;
      case TYPE_ROLLBARS:
      case TYPE_TIRES:
      case TYPE_MISC:
        target = 3;
        break;
      default:
        target = 4;
        break;
    }
    skill = get_skill(ch, skill, target);
    target += modify_target(ch);
    kit = has_kit(ch, TYPE_VEHICLE);
    if ((shop = find_workshop(ch, TYPE_VEHICLE)))
      kit = GET_OBJ_VAL(shop, 0);
    if (!kit && !shop) {
      send_to_char("You don't have any tools here for working on vehicles.\r\n", ch);
      return;
    }
    if (kit < mod_types[GET_OBJ_VAL(mod, 0)].tools) {
      send_to_char(ch, "You don't have the right tools for that job.\r\n");
      return;
    } else if (mod_types[GET_OBJ_VAL(mod, 0)].tools == TYPE_KIT) {
      if (kit == TYPE_SHOP)
        target--;
      else if (kit == TYPE_FACILITY)
        target -= 3;
    } else if (mod_types[GET_OBJ_VAL(mod, 0)].tools == TYPE_SHOP && kit == TYPE_FACILITY)
      target--;

    if ((skill = success_test(skill, target)) == -1) {
      sprintf(buf, "You botch up the upgrade completely, destroying %s.\r\n", GET_OBJ_NAME(mod));
      send_to_char(buf, ch);
      extract_obj(mod);
      return;
    } else if (skill < 1) {
      send_to_char(ch, "You can't figure out how to install it. \r\n");
      return;
    }
    if (GET_OBJ_VAL(mod, 0) == TYPE_DRIVEBYWIRE) {
      target = 4;
      skill = get_skill(ch, SKILL_BR_COMPUTER, target);
      if (success_test(skill, target) < 0) {
        send_to_char("You can't seem to get your head around the computers on this part.\r\n", ch);
        return;
      }
      target = 4;
      skill = get_skill(ch, SKILL_BR_ELECTRONICS, target);
      if (success_test(skill, target) < 0) {
        send_to_char("You can't seem to get your head around the wiring on this part.\r\n", ch);
        return;
      }
    }
  }

  if ((veh->type == VEH_DRONE && GET_OBJ_VAL(mod, 4) < 1) ||
      (veh->type != VEH_DRONE && GET_OBJ_VAL(mod, 4) == 1)) {
    send_to_char(ch, "That part won't fit on.\r\n");
    return;
  }
  if (GET_OBJ_VAL(mod, 0) == TYPE_MOUNT) {
    skill = 0;
    for (obj = veh->mount; obj; obj = obj->next_content)
      switch (GET_OBJ_VAL(obj, 3)) {
      case 0:
      case 1:
        j++;
        break;
      case 2:
      case 3:
      case 5:
        j += 2;
        break;
      case 4:
        j += 4;
        break;
      }
    switch (GET_OBJ_VAL(mod, 1)) {
    case 1:
      skill = 1;
    case 0:
      j++;
      target = 10;
      break;
    case 3:
      skill = 1;
    case 2:
      j += 2;
      target = 10;
      break;
    case 4:
      skill = 1;
      j += 4;
      target = 100;
      break;
    case 5:
      skill = 1;
      j += 2;
      target = 25;
      break;
    }
    if (j > veh->body || (veh->usedload + target) > veh->load) {
      send_to_char("Try as you might, you just can't fit it on.\r\n", ch);
      return;
    }
    veh->usedload += target;
    veh->sig -= skill;
    obj_from_char(mod);
    if (veh->mount)
      mod->next_content = veh->mount;
    veh->mount = mod;
  } else {
    if (GET_OBJ_VAL(mod, 5) && !IS_SET(GET_OBJ_VAL(mod, 5), 1 << veh->engine)) {
      send_to_char("You can't use that part on this type of engine.\r\n", ch);
      return;
    }
    if ((GET_OBJ_VAL(mod, 0) != TYPE_SEATS && GET_OBJ_VAL(mod, 0) == TYPE_ARMOUR && GET_OBJ_VAL(mod, 0) == TYPE_CONCEALEDARMOUR) && GET_MOD(veh, GET_OBJ_VAL(mod, 6))) {
      send_to_char(ch, "There is already a mod of that type installed.\r\n");
      return;
    }
    if (GET_OBJ_VAL(mod, 0) == TYPE_ARMOUR || GET_OBJ_VAL(mod, 0) == TYPE_CONCEALEDARMOUR) {
      if (GET_MOD(veh, GET_OBJ_VAL(mod, 6)) && GET_OBJ_VAL(GET_MOD(veh, GET_OBJ_VAL(mod, 6)), 0) != GET_OBJ_VAL(mod, 0)) {
        send_to_char("You cannot mix normal and concealed armour.\r\n", ch);
        return;
      }
      int totalarmour = GET_OBJ_VAL(mod, 2);
      if (GET_MOD(veh, GET_OBJ_VAL(mod, 6)))
        totalarmour += GET_OBJ_VAL(GET_MOD(veh, GET_OBJ_VAL(mod, 6)), 2);
      if (totalarmour > veh->body / 2) {
        send_to_char("You can't put this much armour on that vehicle.\r\n", ch);
        return;
      }      
      if (!GET_MOD(veh, GET_OBJ_VAL(mod, 6)))
        GET_MOD(veh, GET_OBJ_VAL(mod, 6)) = mod;      
      else  affect_veh(veh, mod->affected[0].location, -mod->affected[0].modifier);
      veh->usedload += GET_OBJ_VAL(GET_MOD(veh, GET_OBJ_VAL(mod, 6)), 1);
      GET_OBJ_VAL(GET_MOD(veh, GET_OBJ_VAL(mod, 6)), 1) = (veh->body * veh->body) * totalarmour * 5;
      mod->affected[0].modifier = totalarmour;
      affect_veh(veh, mod->affected[0].location, mod->affected[0].modifier);
      if (GET_MOD(veh, GET_OBJ_VAL(mod, 6)) != mod)
        extract_obj(mod);
      else obj_from_char(mod);
    } else {
      if (veh->load - veh->usedload < GET_OBJ_VAL(mod, 1)) {
        send_to_char(ch, "Try as you might, you just can't fit it in.\r\n");
        return;
      }
      veh->usedload += GET_OBJ_VAL(mod, 1);
      for (j = 0; j < MAX_OBJ_AFFECT; j++)
        affect_veh(veh, mod->affected[j].location, mod->affected[j].modifier);
      if (GET_OBJ_VAL(mod, 0) == TYPE_SEATS && GET_MOD(veh, GET_OBJ_VAL(mod, 6))) {
        GET_MOD(veh, GET_OBJ_VAL(mod, 6))->affected[0].modifier++;
        extract_obj(mod);
      } else {
        GET_MOD(veh, GET_OBJ_VAL(mod, 6)) = mod;
       obj_from_char(mod);
      } 
    }
  }

  sprintf(buf, "$n goes to work on %s.\r\n", GET_VEH_NAME(veh));
  act(buf, TRUE, ch, 0, 0, TO_ROOM);
  sprintf(buf, "You go to work on %s.\r\n", GET_VEH_NAME(veh));
  send_to_char(buf, ch);
}

void disp_mod(struct veh_data *veh, struct char_data *ch, int i)
{
  send_to_char(ch, "\r\nMounts:\r\n");
  for (struct obj_data *mount = veh->mount; mount; mount = mount->next_content)
    if (GET_OBJ_VAL(mount, 1) != 0 && GET_OBJ_VAL(mount, 1) != 2 && mount->contains)
      send_to_char(ch, "%s mounted on %s.\r\n", CAP(GET_OBJ_NAME(mount->contains)), GET_OBJ_NAME(mount));
  send_to_char(ch, "Modifications:\r\n");
  for (int x = 0; x < NUM_MODS; x++) {
    if (!GET_MOD(veh, x))
      continue;
    if ((x >= MOD_WHEELS && x <= MOD_ARMOUR) || x == MOD_EXHAUST) 
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

    }
  }
}

ACMD(do_control)
{
  struct obj_data *cyber, *jack = NULL;;
  struct veh_data *veh;
  struct char_data *temp;
  bool has_rig = FALSE;
  int i;

  for (cyber = ch->cyberware; cyber; cyber = cyber->next_content)
    if (GET_OBJ_VAL(cyber, 0) == CYB_VCR)
      has_rig = TRUE;
    else if (GET_OBJ_VAL(cyber, 0) == CYB_DATAJACK || (GET_OBJ_VAL(cyber, 0) == CYB_EYES && IS_SET(GET_OBJ_VAL(cyber, 3), EYE_DATAJACK)))
      jack = cyber;

  if (IS_WORKING(ch) || AFF_FLAGGED(ch, AFF_PILOT)) {
    send_to_char("Now that would be a neat trick.\r\n", ch);
    return;
  }
  if (!jack) {
    send_to_char("You need a datajack to do that.\r\n", ch);
    return;
  }
  if (!has_rig) {
    send_to_char("You need a vehicle control rig to do that.\r\n", ch);
    return;
  }

  if (GET_OBJ_VAL(jack, 0) == CYB_DATAJACK && GET_OBJ_VAL(jack, 3) == DATA_INDUCTION) {
    if (GET_EQ(ch, WEAR_HANDS)) {
      send_to_char(ch, "Try removing your gloves first.\r\n");
      return;
    }
  } else if (GET_EQ(ch, WEAR_HEAD) && GET_OBJ_VAL(GET_EQ(ch, WEAR_HEAD), 7) > 0) {
    send_to_char(ch, "Try removing your helmet first.\r\n");
    return;
  }

  if (ch->in_veh) {
    send_to_char(ch, "You can't control a vehicle from inside one.\r\n");
    return;
  }
  has_rig = FALSE;
  for (cyber = ch->carrying; cyber; cyber = cyber->next_content)
    if (GET_OBJ_TYPE(cyber) == ITEM_RCDECK)
      has_rig = TRUE;

  if (!has_rig)
    for (int x = 0; x < NUM_WEARS; x++)
      if (GET_EQ(ch, x))
        if (GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_RCDECK)
          has_rig = TRUE;

  if (!has_rig) {
    send_to_char("You need a Remote Control Deck to do that.\r\n", ch);
    return;
  }

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
  if (!veh->in_room) {
    send_to_char("You can't seem to make contact with it.\r\n", ch);
    return;
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
  if (GET_OBJ_VAL(jack, 0) == CYB_DATAJACK && GET_OBJ_VAL(jack, 3) == DATA_INDUCTION)
    sprintf(buf, "$n places $s hand over $s induction pad as $e connects to $s deck.");
  else if (GET_OBJ_VAL(jack, 0) == CYB_DATAJACK)
    sprintf(buf, "$n slides ones end of the cable into $s datajack and the other into $s remote control deck.");
  else sprintf(buf, "$n's eye opens up as $e slides $s remote control deck cable into $s eye datajack.");
  act(buf, TRUE, ch, NULL, NULL, TO_ROOM);
  PLR_FLAGS(ch).ToggleBit(PLR_REMOTE);
  sprintf(buf, "You take control of %s.\r\n", GET_VEH_NAME(veh));
  send_to_char(buf, ch);
}


ACMD(do_subscribe)
{
  struct veh_data *veh, *temp;
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
    send_to_char("You need a Remote Control Deck to do that.\r\n", ch);
    return;
  }
  if (subcmd == SCMD_UNSUB) {
    if (!*argument) {
      send_to_char("You need to supply a number.\r\n", ch);
      return;
    }
    num = atoi(argument);
    for (veh = ch->char_specials.subscribe; veh; veh = veh->next_sub)
      if (--num < 0)
        break;
    if (!veh) {
      send_to_char("Your subscriber list isn't that big.\r\n", ch);
      return;
    }
    REMOVE_FROM_LIST(veh, ch->char_specials.subscribe, next_sub);
    veh->sub = FALSE;
    veh->next_sub = NULL;
    sprintf(buf, "You remove %s from your subscriber list.\r\n", GET_VEH_NAME(veh));
    send_to_char(buf, ch);
    return;
  }

  if (!*argument) {
    if (ch->char_specials.subscribe) {
      send_to_char("Your subscriber list contains:\r\n", ch);
      for (veh = ch->char_specials.subscribe; veh; veh = veh->next_sub) {
        sprintf(buf, "%2d) %-30s (At %s) [%2d/10] Damage\r\n", i, GET_VEH_NAME(veh),
                veh->in_veh ? world[veh->in_veh->in_room].name : world[veh->in_room].name, veh->damage);
        send_to_char(buf, ch);
        i++;
      }
    } else
      send_to_char("Your subscriber list is empty.\r\n", ch);
    return;
  }


  one_argument(argument, buf);
  if (ch->in_veh) {
    if (ch->in_veh->in_veh)
      veh = get_veh_list(buf, ch->in_veh->in_veh->carriedvehs, ch);
    else veh = get_veh_list(buf, world[ch->in_veh->in_room].vehicles, ch);
  } else
    veh = get_veh_list(buf, world[ch->in_room].vehicles, ch);
  if (!veh) {
    send_to_char(NOOBJECT, ch);
    return;
  }
  if (veh->owner != GET_IDNUM(ch)) {
    send_to_char("That's not yours.\r\n", ch);
    return;
  }
  if (veh->sub) {
    send_to_char("That is already part of your subscriber list.\r\n", ch);
    return;
  }
  veh->sub = TRUE;
  veh->next_sub = ch->char_specials.subscribe;
  ch->char_specials.subscribe = veh;
  sprintf(buf, "You add %s to your subscriber list.\r\n", GET_VEH_NAME(veh));
  send_to_char(buf, ch);
}

ACMD(do_repair)
{
  struct obj_data *obj, *shop = NULL;
  struct veh_data *veh;
  int target = 4, skill = 0, success, mod = 0;

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

  if (!(veh = get_veh_list(argument, ch->in_veh ? ch->in_veh->carriedvehs : world[ch->in_room].vehicles, ch))) {
    send_to_char(NOOBJECT, ch);
    return;
  }

  if (veh->damage == 0) {
    send_to_char(ch, "But it's not damaged!\r\n");
    return;
  }

  switch(veh->type) {
    case VEH_DRONE:
      skill = SKILL_BR_DRONE;
      break;
    case VEH_BIKE:
      skill = SKILL_BR_BIKE;
      break;
    case VEH_CAR:
      skill = SKILL_BR_CAR;
      break;
    case VEH_TRUCK:
      skill = SKILL_BR_TRUCK;
      break;
  }
  skill = get_skill(ch, skill, target);
  target += (veh->damage - 2) / 2;
  target += modify_target(ch);

  for (obj = ch->carrying; obj && !mod; obj = obj->next_content)
    if (GET_OBJ_TYPE(obj) == ITEM_WORKSHOP && GET_OBJ_VAL(obj, 1) == 9 && GET_OBJ_VAL(obj, 0) == 4)
      mod = GET_OBJ_VAL(obj, 1);
  for (int i = 0; !mod && i < (NUM_WEARS - 1); i++)
    if ((obj = GET_EQ(ch, i)) && GET_OBJ_TYPE(obj) == ITEM_WORKSHOP && GET_OBJ_VAL(obj, 1) == 0 && GET_OBJ_VAL(obj, 0) == 4)
      mod = GET_OBJ_VAL(obj, 1);
  shop = find_workshop(ch, TYPE_VEHICLE);
  if (!shop) {
    if (veh->damage >= 7) {
      send_to_char("You better get it to a garage before you try and fix this much damage.\r\n", ch);
      return;
    }
    target += 2;
  }

  switch (mod) {
  case 0:
    target += 4;
    break;
  case 1:
    target += 2;
    break;
  case 3:
    target -= 2;
    break;
  }

  if ((success = (GET_LEVEL(ch) > LVL_MORTAL ? 20 : success_test(skill, target))) < 1) {
    send_to_char(ch, "It seems to be beyond your skill.\r\n");
    return;
  }
  veh->damage -= (success + 1) / 2;
  if (veh->damage <= 0) {
    send_to_char(ch, "You go to work on the vehicle and it looks as good as new.\r\n");
    veh->damage = 0;
  } else
    send_to_char(ch, "You go to work and repair part of the damage.\r\n");
  WAIT_STATE(ch, 2 * PULSE_VIOLENCE);
}

ACMD(do_driveby)
{
  struct obj_data *wielded;
  struct char_data *vict, *pass, *list = NULL;
  int dir;

  if (!ch->in_veh) {
    send_to_char(ch, "You must be in a vehicle to do that.\r\n");
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
  if (!(vict = get_char_in_list_vis(ch, arg, world[ch->in_veh->in_room].people))) {
    send_to_char(NOPERSON, ch);
    return;
  }
  if ((dir = search_block(buf2, lookdirs, FALSE)) == -1) {
    send_to_char("What direction?\r\n", ch);
    return;
  }
  dir = convert_look[dir];
  if (!EXIT(ch->in_veh, dir)) {
    send_to_char("That would just be crazy...\r\n", ch);
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
    if (!IS_NPC(vict) && !PRF_FLAGGED(ch, PRF_PKER) && !PRF_FLAGGED(vict, PRF_PKER)) {
      send_to_char(ch, "You have to be flagged PK to attack another player.\r\n");
      return;
    }
    sprintf(buf, "You point %s towards %s and open fire!\r\n", GET_OBJ_NAME(wielded), GET_NAME(vict));
    send_to_char(buf, ch);
    sprintf(buf, "%s aims %s towards %s and opens fire!\r\n", GET_NAME(ch), GET_OBJ_NAME(wielded), GET_NAME(vict));
    send_to_veh(buf, ch->in_veh, ch, FALSE);
    list = ch;
    roll_individual_initiative(ch);
    GET_INIT_ROLL(ch) += 20;
  } else {
    sprintf(buf, "You point towards %s and shout, \"Light 'em Up!\".\r\n", GET_NAME(vict));
    send_to_char(buf, ch);
    sprintf(buf, "%s points towards %s and shouts, \"Light 'em Up!\".!\r\n", GET_NAME(ch), GET_NAME(vict));
    send_to_veh(buf, ch->in_veh, ch, FALSE);
  }

  for (pass = ch->in_veh->people; pass; pass = pass->next_in_veh) {
    if (pass != ch && PLR_FLAGGED(pass, PLR_DRIVEBY) && AWAKE(pass) && GET_EQ(pass, WEAR_WIELD) &&
        GET_OBJ_VAL(GET_EQ(pass, WEAR_WIELD),4) >= SKILL_FIREARMS) {
      if (!IS_NPC(vict) && !PRF_FLAGGED(pass, PRF_PKER) && !PRF_FLAGGED(vict, PRF_PKER))
        continue;
      sprintf(buf, "You follow %s lead and take aim.\r\n", HSHR(ch));
      send_to_char(pass, buf);
      PLR_FLAGS(pass).RemoveBit(PLR_DRIVEBY);
      pass->next_fighting = list;
      list = pass;
      roll_individual_initiative(pass);
      GET_INIT_ROLL(pass) += 20;
    }
  }
  if (!list) {
    send_to_char(ch, "But no one is in the car to hear you.\r\n");
    return;
  }
  order_list(list);
  roll_individual_initiative(vict);
  if (GET_INIT_ROLL(vict) < 12)
    sprintf(buf, "You suddenly feel a cap busted in your ass as %s zooms past!\r\n", GET_VEH_NAME(ch->in_veh));
  else
    sprintf(buf, "A hail of bullets flies from %s as it zooms past!\r\n", GET_VEH_NAME(ch->in_veh));
  send_to_char(vict, buf);

  for (int i = 2; i > 0; i--) {
    for (pass = list; pass && vict->in_room == ch->in_veh->in_room; pass = pass->next_fighting) {
      if (GET_INIT_ROLL(pass) >= 0) {
        hit(pass, vict, GET_EQ(pass, WEAR_WIELD), NULL);
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
      sprintf(buf, "You swing around and take aim at %s.\r\n", GET_VEH_NAME(ch->in_veh));
      send_to_char(vict, buf);
      sprintf(buf, "%s swings around and takes aim at your ride.\r\n", GET_NAME(vict));
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

  RIG_VEH(ch, veh);
  if (veh->hood) {
    send_to_char("You can't move with the hood up.\r\n", ch);
    return;
  } else if (i < veh->cspeed) {
    if (i == 1) {
      send_to_char("You bring the vehicle to a halt.\r\n", ch);
      send_to_veh("The vehicle slows to a stop.\r\n", veh, ch, FALSE);
    } else {
      if (!PLR_FLAGGED(ch, PLR_REMOTE) && !AFF_FLAGGED(ch, AFF_RIG))
        send_to_char("You put your foot on the brake.\r\n", ch);
      send_to_veh("You slow down.", veh, ch, TRUE);
    }
  } else if (i > veh->cspeed) {
    if (!PLR_FLAGGED(ch, PLR_REMOTE) && !AFF_FLAGGED(ch, AFF_RIG))
      send_to_char("You put your foot on the accelerator.\r\n", ch);
    send_to_veh("You speed up.", veh, ch, TRUE);
  } else {
    send_to_char("But you're already travelling that fast!\r\n", ch);
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
    send_to_char("You have to be controlling a vehicle to do that.\r\n", ch);
    return;
  }
  RIG_VEH(ch, veh);

  if (veh->following) {
    sprintf(buf, "You stop following a %s.\r\n", GET_VEH_NAME(veh->following));
    stop_chase(veh);
    send_to_char(buf, ch);
    return;
  } else if (veh->followch) {
    sprintf(buf, "You stop following %s.\r\n", GET_NAME(veh->followch));
    veh->followch = NULL;
    send_to_char(buf, ch);
    return;
  }
  two_arguments(argument, arg, buf2);

  if (!*arg) {
    send_to_char("Chase what?\r\n", ch);
    return;
  }

  if (!(tveh = get_veh_list(arg, world[veh->in_room].vehicles, ch)) &&
      !(vict = get_char_room(arg, veh->in_room))) {
    send_to_char(NOOBJECT, ch);
    return;
  }

  if (tveh == veh) {
    send_to_char(ch, "You cannot chase yourself.\r\n");
    return;
  }
  if (tveh) {
    sprintf(buf, "You start chasing %s.\r\n", GET_VEH_NAME(tveh));
    send_to_char(buf, ch);
    veh->following = tveh;
    veh->dest = NOWHERE;
    k = new veh_follow;
    k->follower = veh;
    if (tveh->followers)
      k->next = tveh->followers;
    tveh->followers = k;
    if (veh->cspeed == SPEED_IDLE) {
      send_to_char(ch, "You put your foot on the accelarator.\r\n");
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
  if (!veh) {
    send_to_char("You must be in a vehicle to do that.\r\n", ch);
    return;
  }
  if (!veh->mount) {
    send_to_char("There is nothing to target.\r\n", ch);
    return;
  }
  two_arguments(argument, arg, buf2);
  if (!*arg) {
    send_to_char("Target what at who?\r\n", ch);
    return;
  }
  if (*arg && *buf2) {
    if (!(AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE))) {
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
      send_to_char("There aren't that many mounts.\r\n", ch);
      return;
    }
    if (!obj->contains) {
      send_to_char("It has no weapon mounted.\r\n", ch);
      return;
    }
    strcpy(arg, buf2);
  } else {
    if (!(AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE) || AFF_FLAGGED(ch, AFF_MANNING))) {
      send_to_char("You don't have control over any mounts.\r\n", ch);
      return;
    }
    if (AFF_FLAGGED(ch, AFF_MANNING)) {
      for (obj = veh->mount; obj; obj = obj->next_content)
        if (obj->worn_by == ch)
          break;
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
  if (!(vict = get_char_room(arg, veh->in_room)) &&
      !(tveh = get_veh_list(arg, world[veh->in_room].vehicles, ch)) && (vict && !CAN_SEE(ch, vict))) {
    send_to_char(ch, "You don't see %s there.\r\n", arg);
    return;
  }
  if (tveh == veh) {
    send_to_char("Why would you want to target yourself?", ch);
    return;
  }
  if (FIGHTING(ch) || FIGHTING_VEH(ch))
    stop_fighting(ch);
  if (modeall) {
    for (obj = veh->mount; obj; obj = obj->next_content)
      if (obj->contains && !obj->worn_by) {
        if (vict) {
          set_fighting(ch, vict);
          if (obj->tveh)
            obj->tveh = NULL;
          obj->targ = vict;
          set_fighting(vict, veh);
          sprintf(buf, "You target %s towards %s.\r\n", GET_OBJ_NAME(obj->contains), PERS(vict, ch));
          send_to_char(buf, ch);
        } else {
          set_fighting(ch, tveh);
          if (obj->targ)
            obj->targ = NULL;
          obj->tveh = tveh;
          sprintf(buf, "You target %s towards %s.\r\n", GET_OBJ_NAME(obj->contains), GET_VEH_NAME(tveh));
          send_to_char(buf, ch);
        }
      }
    return;
  }
  if (vict) {
    for (struct obj_data *obj2 = veh->mount; obj2; obj2 = obj2->next_content)
      if (obj2->targ == vict) {
        send_to_char("Someone is already targetting that.\r\n", ch);
        return;
      }
    set_fighting(ch, vict);
    if (obj->tveh)
      obj->tveh = NULL;
    obj->targ = vict;
    set_fighting(vict, veh);
    act("You target $o towards $N.", FALSE, ch, obj->contains, vict, TO_CHAR);
    if (AFF_FLAGGED(ch, AFF_MANNING)) {
      sprintf(buf, "%s's %s swivels towards you.\r\n", GET_VEH_NAME(ch->in_veh), GET_OBJ_NAME(obj));
      send_to_char(buf, vict);
    }
  } else {
    set_fighting(ch, tveh);
    if (obj->targ)
      obj->targ = NULL;
    obj->tveh = tveh;
    sprintf(buf, "You target %s towards %s.\r\n", GET_OBJ_NAME(obj->contains), GET_VEH_NAME(tveh));
    send_to_char(buf, ch);
    if (AFF_FLAGGED(ch, AFF_MANNING)) {
      sprintf(buf, "%s's %s swivels towards your ride.\r\n", GET_VEH_NAME(ch->in_veh), GET_OBJ_NAME(obj));
      send_to_veh(buf, tveh, 0, TRUE);
    }
  }
}

ACMD(do_mount)
{
  int i = 0;
  struct veh_data *veh;
  struct obj_data *obj, *gun = NULL, *ammo = NULL; /* Appears unused:  *bin = NULL; */
  RIG_VEH(ch, veh);
  if (!veh) {
    send_to_char("You must be in a vehicle to use that.\r\n", ch);
    return;
  }
  if (IS_ASTRAL(ch)) {
    send_to_char("You cannot seem to touch physical objects.\r\n", ch);
    return;
  }

  sprintf(buf, "%s is mounting the following:\r\n", CAP(GET_VEH_NAME(veh)));
  send_to_char(buf, ch);
  for (obj = veh->mount; obj; obj = obj->next_content) {
    for (struct obj_data *x = obj->contains; x; x = x->next_content)
      if (GET_OBJ_TYPE(x) == ITEM_GUN_AMMO)
        ammo = x;
      else if (GET_OBJ_TYPE(x) == ITEM_WEAPON)
        gun = x;
      /* Code appears to be unused:
      else if (GET_OBJ_TYPE(x) == ITEM_MOD)
        bin = x; */
    if (gun)
      sprintf(buf, "%2d) %-20s (Mounting %s) (Ammo %d/%d)", i, GET_OBJ_NAME(obj),
              GET_OBJ_NAME(gun), ammo ? GET_OBJ_VAL(ammo, 0) : 0, GET_OBJ_VAL(gun, 5) * 2);
    else
      sprintf(buf, "%2d) %-20s (Mounting Nothing)", i, GET_OBJ_NAME(obj));
    if (obj->worn_by)
      sprintf(buf + strlen(buf), " (Manned by %s)", (found_mem(GET_MEMORY(ch), obj->worn_by) ?
              (found_mem(GET_MEMORY(ch), obj->worn_by))->mem
              : GET_NAME(obj->worn_by)));
    strcat(buf, "\r\n");
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
    for (mount = ch->in_veh->mount; mount; mount = mount->next_content)
      if (mount->worn_by == ch)
        break;
    mount->worn_by = NULL;
    AFF_FLAGS(ch).ToggleBit(AFF_MANNING);
    act("$n stops manning $o.", FALSE, ch, mount, 0, TO_ROOM);
    act("You stop manning $o.", FALSE, ch, mount, 0, TO_CHAR);
    if (FIGHTING(ch))
      stop_fighting(ch);
    if (!*argument)
      return;
  }

  if ((j = atoi(argument)) < 0) {
    send_to_char("Which mount?", ch);
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
  for (struct obj_data *obj = mount->contains; obj; obj = obj->next_content)
    if (GET_OBJ_TYPE(obj) == ITEM_WEAPON) {
      mount->worn_by = ch;
      act("$n mans $o.", FALSE, ch, mount, 0, TO_ROOM);
      act("You man $o.", FALSE, ch, mount, 0, TO_CHAR);
      AFF_FLAGS(ch).ToggleBit(AFF_MANNING);
      return;
    }
  send_to_char("But there is no weapon mounted there.\r\n", ch);
}

ACMD(do_gridguide)
{
  struct veh_data *veh;
  struct grid_data *grid = NULL;
  vnum_t x = 0, y = 0;
  RIG_VEH(ch, veh);

  if (!veh) {
    send_to_char("You have to be in a vehicle to do that.\r\n", ch);
    return;
  }
  if (IS_ASTRAL(ch)) {
    send_to_char("You cannot seem to touch physical objects.\r\n", ch);
    return;
  }

  if (!veh->autonav) {
    send_to_char("You need to have an autonav system installed.\r\n", ch);
    return;
  }
  if (veh->hood) {
    send_to_char("Gridguide refuses to engage with the hood up.\r\n", ch);
    return;
  }
  if ((veh->locked && GET_IDNUM(ch) != veh->owner) && !(PLR_FLAGGED(ch, PLR_REMOTE) || AFF_FLAGGED(ch, AFF_PILOT))) {
    send_to_char("You don't have control over the vehicle.\r\n", ch);
    return;
  }
  argument = two_arguments(argument, arg, buf2);
  if (!*arg) {
    int i = 0;
    send_to_char("The following destinations are available:\r\n", ch);
    for (grid = veh->grid; grid; grid = grid->next) {
      i++;
      if (find_first_step(veh->in_room, real_room(grid->room)) < 0)
        sprintf(buf, "^r%-20s [%-6ld, %-6ld](Unavailable)\r\n", CAP(grid->name),
                grid->room - (grid->room * 3), grid->room + 100);
      else
        sprintf(buf, "^B%-20s [%-6ld, %-6ld](Available)\r\n", CAP(grid->name),
                grid->room - (grid->room * 3), grid->room + 100);
      send_to_char(buf, ch);
    }
    send_to_char(ch, "%d Entries remaining.\r\n", (veh->autonav * 5) - i);
  } else if (!str_cmp(arg, "stop")) {
    veh->dest = 0;
    send_to_veh("The autonav disengages.\r\n", veh, 0, TRUE);
    if (!AFF_FLAGGED(ch, AFF_PILOT))
      veh->cspeed = SPEED_OFF;
  } else if (!*buf2) {
    if (veh->cspeed > SPEED_IDLE && !(AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE))) {
      send_to_char("You don't have control over the vehicle.\r\n", ch);
      return;
    }
    for (grid = veh->grid; grid; grid = grid->next)
      if (is_abbrev(arg, grid->name))
        break;
    if (!grid)
      send_to_char("That destination doesn't seem to be in the system.\r\n", ch);
    else if (find_first_step(veh->in_room, real_room(grid->room)) < 0)
      send_to_char("That destination is currently unavailable.\r\n", ch);
    else {
      veh->dest = real_room(grid->room);
      veh->cspeed = SPEED_CRUISING;
      if (AFF_FLAGGED(ch, AFF_PILOT))
        AFF_FLAGS(ch).RemoveBits(AFF_PILOT, AFF_RIG, ENDBIT);
      if (PLR_FLAGGED(ch, PLR_REMOTE)) {
        send_to_veh("The autonav beeps and the vehicle starts to move towards its destination.\r\n", veh, 0, FALSE);
        send_to_char("Gridguide activates and kicks you out of the system.\r\n", ch);
        PLR_FLAGS(ch).RemoveBit(PLR_REMOTE);
        ch->char_specials.rigging = FALSE;
        veh->rigger = NULL;
      } else send_to_veh("The autonav beeps and the vehicle starts to move towards its destination.\r\n", veh, 0, TRUE);

    }
  } else if (!str_cmp(arg, "del")) {
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
      act("$n punches something into the autonav.", FALSE, ch, 0 , 0, TO_ROOM);
    }
  } else if (!str_cmp(arg, "add")) {
    int i = 0;
    for (grid = veh->grid; grid; grid = grid->next) {
      if (!str_cmp(buf2, grid->name)) {
        send_to_char("That entry already exists.\r\n", ch);
        return;
      }
      i++;
    }
    if (i >= (veh->autonav * 5)) {
      send_to_char("The system seems to be full.\r\n", ch);
      return;
    }
    if (!*argument) {
      if (!(ROOM_FLAGGED(veh->in_room, ROOM_ROAD) || ROOM_FLAGGED(veh->in_room, ROOM_GARAGE)) ||
          ROOM_FLAGGED(veh->in_room, ROOM_NOGRID)) {
        send_to_char("You currently aren't on the grid.\r\n", ch);
        return;
      }
      x = veh->in_room;
    } else {
      two_arguments(argument, arg, buf);
      if (!*buf) {
        send_to_char("You need a second co-ordinate.\r\n", ch);
        return;
      }
      if (!((x = atoi(arg)) && (y = atoi(buf)))) {
        send_to_char("Those co-ordinates seem invalid.\r\n", ch);
        return;
      }
      x = (x + ((y - 100) * 3));
      if (!(x = real_room(x)) || !(ROOM_FLAGGED(x, ROOM_ROAD) || ROOM_FLAGGED(x, ROOM_GARAGE)) ||
          ROOM_FLAGGED(x, ROOM_NOGRID)) {
        send_to_char("Those co-ordinates seem invalid.\r\n", ch);
        return;
      }
    }
    grid = new grid_data;
    if (strlen(buf2) >= LINE_LENGTH)
      buf2[LINE_LENGTH] = '\0';
    grid->name = str_dup(buf2);
    grid->room = world[x].number;
    grid->next = veh->grid;
    veh->grid = grid;
    send_to_char("You add the destination into the system.\r\n", ch);
    act("$n punches something into the autonav.", FALSE, ch, 0 , 0, TO_ROOM);
  }
}

void process_autonav(void)
{
  for (struct veh_data *veh = veh_list; veh; veh = veh->next)
    if (veh->dest && veh->cspeed > SPEED_IDLE && veh->damage < 10) {
      struct char_data *ch = NULL;
      if (!(ch = veh->rigger))
        ch = veh->people; 
      // Stop empty vehicles
      if (!ch) {
        veh->dest = 0;
        veh->cspeed = SPEED_OFF;
      }
      int dir = 0;
      for (int x = (int)get_speed(veh) / 10; x && dir >= 0 && veh->dest; x--) {
        dir = find_first_step(veh->in_room, veh->dest);
        if (dir >= 0)
          move_vehicle(ch, dir);
      }
      if (veh->in_room == veh->dest) {
        send_to_veh("Having reached its destination, the autonav shuts off.\r\n", veh, 0, TRUE);
        veh->cspeed = SPEED_OFF;
        veh->dest = 0;
      }
    }
}

ACMD(do_switch)
{
  if (!ch->in_veh)
    send_to_char("You can't switch positions when not in a vehicle.\r\n", ch);
  else if (IS_WORKING(ch) || AFF_FLAGGED(ch, AFF_PILOT))
    send_to_char(TOOBUSY, ch);
  else if (!ch->in_veh->seating[!ch->vfront])
    send_to_char("There's no room to move to.\r\n", ch);
  else {
    ch->in_veh->seating[ch->vfront]++;
    ch->in_veh->seating[!ch->vfront]--;
    ch->vfront = !ch->vfront;
    sprintf(buf, "$n climbs into the %s.", ch->vfront ? "front" : "back");
    act(buf, TRUE, ch, 0, 0, TO_ROOM);
    send_to_char(ch, "You climb into the %s.\r\n", ch->vfront ? "front" : "back");
  }
}

ACMD(do_pop)
{
  skip_spaces(&argument);
  struct veh_data *veh = ch->in_veh;
  if (veh && veh->hood) {
    send_to_char("You cannot close the hood from in here.\r\n", ch);
    return;
  }
  if (!veh && (!(veh = get_veh_list(argument, world[ch->in_room].vehicles, ch)))) {
    send_to_char(NOOBJECT, ch);
    return;
  }
  if (!ch->in_veh && !veh->hood && veh->owner != GET_IDNUM(ch)) 
    send_to_char("That's not your vehicle.\r\n", ch);
  else if (veh->cspeed > SPEED_OFF)
    send_to_char("It's moving too fast for you to do that.\r\n", ch);
  else if (veh->hood) {
    send_to_char(ch, "You close the hood of %s.\r\n", GET_VEH_NAME(veh));
    sprintf(buf, "$n closes the hood of %s.", GET_VEH_NAME(veh));
    act(buf, 0, ch, 0, 0, TO_ROOM);
    veh->hood = FALSE;
  } else {
    sprintf(buf, "$n pops the hood of %s.", GET_VEH_NAME(veh));
    act(buf, FALSE, ch, 0, 0, TO_ROOM);
    if (ch->in_veh && world[ch->in_veh->in_room].people) {
      sprintf(buf, "Someone pops the hood of %s.", GET_VEH_NAME(veh));
      act(buf, FALSE, world[ch->in_veh->in_room].people, 0, 0, TO_ROOM);
      act(buf, FALSE, world[ch->in_veh->in_room].people, 0, 0, TO_CHAR);
    }
    send_to_char(ch, "You pop the hood of %s.\r\n", GET_VEH_NAME(veh));
    veh->hood = TRUE;
  }
}

ACMD(do_tow)
{
  struct veh_data *veh, *tveh;
  if (!(AFF_FLAGGED(ch, AFF_PILOT) || PLR_FLAGGED(ch, PLR_REMOTE))) {
    send_to_char("You have to be controlling a vehicle to do that.\r\n", ch);
    return;
  }
  RIG_VEH(ch, veh);
  if (veh->type != VEH_TRUCK && veh->type != VEH_DRONE) {
    send_to_char("You don't have enough power to tow anything.\r\n", ch);
    return;
  }
  if (veh->cspeed != SPEED_IDLE) {
    send_to_char("You are moving too fast!\r\n", ch);
    return;
  }
  if (veh->towing) {
    send_to_char(ch, "You release %s from your towing equipment.\r\n", GET_VEH_NAME(veh->towing));
    if (world[veh->in_room].people) {
      sprintf(buf, "%s releases %s from its towing equipment.\r\n", GET_VEH_NAME(veh), GET_VEH_NAME(veh->towing));
      act(buf, FALSE, world[veh->in_room].people, 0, 0, TO_ROOM);
      act(buf, FALSE, world[veh->in_room].people, 0, 0, TO_CHAR);
    }
    veh_to_room(veh->towing, veh->in_room);
    veh->towing = NULL;
    return;
  }
  skip_spaces(&argument);
  if (!(tveh = get_veh_list(argument, world[veh->in_room].vehicles, ch))) {
    send_to_char(NOOBJECT, ch);
    return;
  }
  if (tveh->type == VEH_TRUCK)
    send_to_char("That is too heavy for you to tow.\r\n", ch);
  else if (tveh->type == VEH_BIKE)
    send_to_char("Try as you might, you can't seem to balance it.\r\n", ch);
  else if (tveh->locked && tveh->type != VEH_DRONE)
    send_to_char("That vehicle won't budge until it's unlocked.\r\n", ch);
  else if (tveh->people)
    send_to_char("Towing a car full of people isn't very safe.\r\n", ch);
  else if (tveh->cspeed > SPEED_OFF)
    send_to_char("The vehicle has to be off for you to tow it.\r\n", ch);
  else if (veh->type == VEH_DRONE && tveh->type != VEH_DRONE)
    send_to_char("Drones can only tow other drones.\r\n", ch);
  else if (veh->type == VEH_DRONE && veh->load < tveh->load)
    send_to_char("Drones can only tow drones that are lighter than them.\r\n", ch);
  else {
    send_to_char(ch, "You pick up %s with your towing equipment.\r\n", GET_VEH_NAME(tveh));
    if (world[veh->in_room].people) {
      sprintf(buf, "%s picks up %s with its towing equipment.\r\n", GET_VEH_NAME(veh), GET_VEH_NAME(tveh));
      act(buf, FALSE, world[veh->in_room].people, 0, 0, TO_ROOM);
      act(buf, FALSE, world[veh->in_room].people, 0, 0, TO_CHAR);
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
          CLEANUP_AND_INITIALIZE_D_STR(d);
          d->max_str = MAX_MESSAGE_LENGTH;
          d->mail_to = 0;
          break;
        case 'q':
          STATE(d) = CON_PLAYING;
          PLR_FLAGS(CH).RemoveBit(PLR_WRITING);
          d->edit_veh = NULL;
          send_to_char("Your ride will be ready in 3 days.\r\n", CH);
          break; 
      }
      break;
    case VEHCUST_NAME:   
      if (strlen(arg) >= LINE_LENGTH) {
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
  if (!*argument)
    send_to_char("Push what?\r\n", ch);   
  else if (ch->in_veh) {
    if (ch->vfront)
      send_to_char("You can't push a vehicle out from here.\r\n", ch);
    else if (ch->in_veh->cspeed > SPEED_IDLE)
      send_to_char("You are moving too fast to do that.\r\n", ch);
    else if (!(veh = get_veh_list(argument, ch->in_veh->carriedvehs, ch)))
      send_to_char("That vehicle isn't in here.\r\n", ch);
    else {
      send_to_char(ch, "You push %s out of the back.\r\n", GET_VEH_NAME(veh));
      sprintf(buf, "$n pushes %s out of the back.", GET_VEH_NAME(veh));
      sprintf(buf2, "$N pushes %s out of the back of %s.", GET_VEH_NAME(veh), GET_VEH_NAME(ch->in_veh));
      act(buf, 0, ch, 0, 0, TO_ROOM);
      if (world[ch->in_veh->in_room].people)
        act(buf2, 0, world[ch->in_veh->in_room].people, 0, ch, TO_NOTVICT);
      veh_from_room(veh);
      veh_to_room(veh, ch->in_veh->in_room);
    }
  } else {  
    two_arguments(argument, buf, buf1);
    if (!*buf || !*buf1) {
      send_to_char("Push what?\r\n", ch);
      return;
    }
    if (!(veh = get_veh_list(buf, world[ch->in_room].vehicles, ch)) || !(found_veh = get_veh_list(buf1, world[ch->in_room].vehicles, ch))) {
      send_to_char("You don't see that vehicle here.\r\n", ch);
      return;
    }
    int mult;
    switch (veh->type) {
      case VEH_DRONE:
        mult = 100;
        break;
      case VEH_TRUCK:
        mult = 1500;
        break;
      default:
        mult = 500;
        break;
    }
    if (found_veh == veh)
      send_to_char("You can't push it into itself.\r\n", ch);
    else if (found_veh->load - found_veh->usedload < veh->body * mult)
      send_to_char("There is not enough room in there for that.\r\n", ch);
    else if (found_veh->locked)
      send_to_char("You can't push it into a locked vehicle.\r\n", ch);
    else if (veh->locked && veh->damage < 10 && veh->type != VEH_DRONE)
      send_to_char("The wheels seem to be locked.\r\n", ch);
    else {
      sprintf(buf, "$n pushes %s into the back of %s.", GET_VEH_NAME(veh), GET_VEH_NAME(found_veh));
      send_to_char(ch, "You push %s into the back of %s.\r\n", GET_VEH_NAME(veh), GET_VEH_NAME(found_veh));
      act(buf, 0, ch, 0, 0, TO_ROOM);
      sprintf(buf2, "You are pushed into %s.\r\n", GET_VEH_NAME(found_veh));
      send_to_veh(buf2, veh, NULL, TRUE);
      veh_to_veh(veh, found_veh);
    }
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
  else if (!(veh = get_veh_list(buf, world[ch->in_room].vehicles, ch)))
    send_to_char("You don't see that vehicle here.\r\n", ch);
  else if (veh->owner != GET_IDNUM(ch))
    send_to_char("You can't transfer ownership of a vehicle you don't own.\r\n", ch);
  else if (!(targ = get_char_room_vis(ch, buf2)))
    send_to_char("You don't see that person here.\r\n", ch);
  else {
    sprintf(buf, "You transfer ownership of %s to $N.", GET_VEH_NAME(veh));
    sprintf(buf2, "$n transfers ownership of %s to you.", GET_VEH_NAME(veh));
    act(buf, 0, ch, 0, targ, TO_CHAR);
    act(buf2, 0, ch, 0, targ, TO_VICT);
    veh->owner = GET_IDNUM(targ);
    veh->sub = FALSE;
    
    save_vehicles();
  }
}
 
