#include <time.h>

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "dblist.h"
#include "constants.h"
#include "newmatrix.h"
#include "memory.h"
#include "quest.h"

#define PERSONA ch->persona
#define DECKER PERSONA->decker
#define TEST_ACCESS 0
#define TEST_CONTROL 1
#define TEST_INDEX 2
#define TEST_FILES 3
#define TEST_SLAVE 4
struct ic_info dummy_ic;

extern void order_list(struct matrix_icon *start);
extern void create_program(struct char_data *ch);
extern void create_part(struct char_data *ch);
extern void create_deck(struct char_data *ch);
extern void create_spell(struct char_data *ch);
extern void create_ammo(struct char_data *ch);

#define IS_PROACTIVE(IC) ((IC)->ic.type != 2 && (IC)->ic.type != 3 && (IC)->ic.type != 4 && (IC)->ic.type != 10 && (IC)->ic.type != 5)
#define HOST matrix[host]
void make_seen(struct matrix_icon *icon, int idnum)
{
  struct seen_data *seen;
  for (seen = icon->decker->seen; seen; seen = seen->next)
    if (seen->idnum == idnum)
      return;

  seen = new seen_data;
  seen->idnum = idnum;
  if (icon->decker->seen)
    seen->next = icon->decker->seen;
  icon->decker->seen = seen;
}

void roll_matrix_init(struct matrix_icon *icon)
{
  int x = 1;
  if (icon->decker && icon->decker->ch)
  {
    icon->initiative = GET_REAL_REA(icon->decker->ch) + (icon->decker->response * 2) + (icon->decker->reality ? 2 : 0);
    x += icon->decker->response + (icon->decker->reality ? 1 : 0);
  } else
  {
    icon->initiative = icon->ic.rating;
    if (matrix[icon->in_host].shutdown)
      icon->initiative--;
    x += matrix[icon->in_host].colour;
  }
  icon->initiative += dice(x, 6);
  icon->initiative -= matrix[icon->in_host].pass * 10;
}

void check_trigger(rnum_t host, struct char_data *ch)
{
  if (!PERSONA || !DECKER)
    return;
  for (struct trigger_step *trig= HOST.trigger; trig; trig = trig->next)
    if (trig->step > DECKER->last_trigger && trig->step <= DECKER->tally)
    {
      if (trig->alert && trig->alert > HOST.alert) {
        HOST.alert = trig->alert;
        switch (HOST.alert) {
        case 1:
          send_to_host(host, "Your view begins to flash orange.\r\n", NULL, FALSE);
          break;
        case 2:
          send_to_host(host, "Sirens join the flashing lights.\r\n", NULL, FALSE);
          break;
        }
      }
      if (real_ic(trig->ic)) {
        struct matrix_icon *ic;
        ic = read_ic(trig->ic, VIRTUAL);
        ic->ic.target = PERSONA->idnum;
        for (struct matrix_icon *icon = HOST.icons; icon; icon = icon->next_in_host)
          if (icon->decker) {
            int target = ic->ic.rating;
            if (matrix[host].shutdown)
              target -= 2;
            int success = success_test(icon->decker->sensor, target);
            if (success >= 3) {
              send_to_icon(icon, "%s is triggered.\r\n", CAP(ic->name));
              make_seen(icon, ic->idnum);
            } else if (success == 2)
              send_to_icon(icon, "%s is triggered.\r\n", CAP(ic->name));
            else if (success == 1)
              send_to_icon(icon, "An IC has entered the host.\r\n");
          }
        icon_to_host(ic, host);
      }
    }
  DECKER->last_trigger = DECKER->tally;
}

bool tarbaby(struct obj_data *prog, struct char_data *ch, struct matrix_icon *ic)
{
  int target = ic->ic.rating;
  if (matrix[ic->in_host].shutdown)
    target -= 2;
  int suc = success_test(GET_OBJ_VAL(prog, 1), target);
  suc -= success_test(target, GET_OBJ_VAL(prog, 1));
  if (suc < 0)
  {
    struct obj_data *temp;
    send_to_icon(PERSONA, "%s crashes your %s!\r\n", CAP(ic->name), GET_OBJ_NAME(prog));
    DECKER->active += GET_OBJ_VAL(prog, 2);
    REMOVE_FROM_LIST(prog, DECKER->software, next_content);
    if (ic->ic.type == 10 && success_test(target, DECKER->mpcp + DECKER->hardening) > 0)
      for (struct obj_data *copy = DECKER->deck->contains; copy; copy = copy->next_content) {
        if (!strcmp(GET_OBJ_NAME(copy), GET_OBJ_NAME(prog))) {
          send_to_icon(PERSONA, "It destroys all copies in storage memory aswell!\r\n");
          GET_OBJ_VAL(DECKER->deck, 5) -= GET_OBJ_VAL(copy, 2);
          extract_obj(copy);
          break;
        }
      }
    extract_obj(prog);
    extract_icon(ic);
    return TRUE;
  }
  return FALSE;
}

void dumpshock(struct matrix_icon *icon)
{
  if (icon->decker && icon->decker->ch)
  {
    send_to_char(icon->decker->ch, "You are dumped from the matrix!\r\n");
    sprintf(buf, "%s depixelizes and vanishes from the host.\r\n", icon->name);
    send_to_host(icon->in_host, buf, icon, FALSE);
    int resist = -success_test(GET_WIL(icon->decker->ch), matrix[icon->in_host].security);
    int dam = convert_damage(stage(resist, matrix[icon->in_host].colour));
    for (struct obj_data *cyber = icon->decker->ch->cyberware; cyber; cyber = cyber->next_content)
      if (GET_OBJ_VAL(cyber, 0) == CYB_DATAJACK) {
        if (GET_OBJ_VAL(cyber, 3) == DATA_INDUCTION)
          sprintf(buf, "$n's hand suddenly recoils from $s induction pad, electricity arcing between the two surfaces!");
        else sprintf(buf, "$n suddenly jerks forward and rips the jack out of $s head!");
        break;
      } else if (GET_OBJ_VAL(cyber, 0) == CYB_EYES && IS_SET(GET_OBJ_VAL(cyber, 3), EYE_DATAJACK)) {
        sprintf(buf, "$n suddenly jerks forward and rips the jack out of $s eye!");
        break;
      }
    act(buf, FALSE, icon->decker->ch, NULL, NULL, TO_ROOM);
    PLR_FLAGS(icon->decker->ch).RemoveBit(PLR_MATRIX);
    icon->decker->PERSONA = NULL;
    if (icon->decker->deck && GET_OBJ_VAL(icon->decker->deck, 0) == 0) {
      act("Smoke emerges from $n's $p.", FALSE, icon->decker->ch, icon->decker->deck, NULL, TO_ROOM);
      act("Smoke emerges from $p.", FALSE, icon->decker->ch, icon->decker->deck, NULL, TO_CHAR);
    }
    damage(icon->decker->ch, icon->decker->ch, dam, TYPE_DUMPSHOCK, MENTAL);
  }
  extract_icon(icon);
}

int system_test(rnum_t host, struct char_data *ch, int type, int software, int modifier)
{
  int detect = 0, target = modifier, skill = get_skill(ch, SKILL_COMPUTER, target) + MIN(GET_MAX_HACKING(ch), GET_REM_HACKING(ch));
  GET_REM_HACKING(ch) -= skill - get_skill(ch, SKILL_COMPUTER, detect);
  detect = 0;
  struct obj_data *prog = NULL;
  if (HOST.stats[type][1] && software != SOFT_DECRYPT && software != SOFT_ANALYZE)
    return 0;
  target = HOST.stats[type][0];
  if (HOST.alert)
    target += 2;
  target += modify_target(ch) + DECKER->res_test + (DECKER->ras ? 0 : 4);
  for (struct obj_data *soft = DECKER->software; soft; soft = soft->next_content)
    if (GET_OBJ_VAL(soft, 0) == SOFT_SLEAZE)
      detect = GET_OBJ_VAL(soft, 1);
    else if (GET_OBJ_VAL(soft, 0) == software)
    {
      target -= GET_OBJ_VAL(soft, 1);
      prog = soft;
    }
  detect += DECKER->masking;
  detect = detect / 2;
  detect -= DECKER->res_det;
  int tally = MAX(0, success_test(HOST.security, detect));
  int success = success_test(skill, target);
  success -= tally;
  struct matrix_icon *temp;
  bool tarred = FALSE;
  for (struct matrix_icon *ic = HOST.icons; ic; ic = temp)
  {
    temp = ic->next_in_host;
    target = ic->ic.rating;
    if (HOST.shutdown)
      target -= 2;
    if (ic->number && ic->ic.target == PERSONA->idnum) {
      if (ic->ic.type == 2 || ic->ic.type == 5)
        tally += MAX(0, success_test(target, detect));
      else if (prog && (ic->ic.type == 4 || ic->ic.type == 10) && ic->ic.subtype == 0 && !tarred)
        tarred = tarbaby(prog, ch, ic);
    }
  }
  DECKER->tally += tally;
  if (DECKER->located)
    DECKER->tally++;
  if (DECKER->tally >= 80)
  {
    dumpshock(PERSONA);
    return -1;
  }
  check_trigger(host, ch);
  return success;
}
#undef HOST

bool has_spotted(struct matrix_icon *icon, struct matrix_icon *targ)
{
  if (targ->evasion)
    return FALSE;
  for (struct seen_data *seen = icon->decker->seen; seen; seen = seen->next)
    if (seen->idnum == targ->idnum)
      return TRUE;
  return FALSE;
}

void fry_mpcp(struct matrix_icon *icon, struct matrix_icon *targ, int success)
{
  if (success >= 2 && targ->decker->deck)
  {
    sprintf(buf, "%s uses the opportunity to fry your MPCP!", CAP(icon->name));
    while (success >= 2 && targ->decker->mpcp > 0) {
      success -= 2;
      targ->decker->mpcp--;
      GET_OBJ_VAL(targ->decker->deck, 0)--;
    }
  }
}

void cascade(struct matrix_icon *icon)
{
  switch (matrix[icon->in_host].colour)
  {
  case 0:
    icon->ic.cascade = 1;
    break;
  case 1:
    if (icon->ic.cascade < (int)(icon->ic.rating * .25) || icon->ic.cascade < 2)
      icon->ic.cascade++;
    break;
  case 2:
    if (icon->ic.cascade < (int)(icon->ic.rating * .5) || icon->ic.cascade < 3)
      icon->ic.cascade++;
    break;
  case 3:
  case 4:
    if (icon->ic.cascade < icon->ic.rating || icon->ic.cascade < 4)
      icon->ic.cascade++;
    break;
  }
}

int maneuver_test(struct matrix_icon *icon)
{
  int icon_target = 0, icon_skill, targ_target = 0, targ_skill;
  if (icon->decker)
  {
    for (struct obj_data *soft = icon->decker->software; soft; soft = soft->next_content)
      if (GET_OBJ_VAL(soft, 0) == SOFT_CLOAK)
        icon_target -= GET_OBJ_VAL(soft, 1);
    targ_target += icon_skill = icon->decker->evasion;
  } else
    targ_target += icon_skill = matrix[icon->in_host].security;
  if (icon->fighting->decker)
  {
    for (struct obj_data *soft = icon->fighting->decker->software; soft; soft = soft->next_content)
      if (GET_OBJ_VAL(soft, 0) == SOFT_LOCKON)
        targ_target -= GET_OBJ_VAL(soft, 1);
    icon_target += targ_skill = icon->fighting->decker->sensor;
  } else
    icon_target += targ_skill = matrix[icon->in_host].security;
  return resisted_test(icon_skill, icon_target, targ_skill, targ_target);
}

void evade_detection(struct matrix_icon *icon)
{
  if (icon->fighting)
  {
    int success = maneuver_test(icon);
    if (success > 0) {
      send_to_icon(icon, "You maneuver away from %s.\r\n", icon->fighting->name);
      icon->evasion = success;
      icon->parry = 0;
      icon->fighting->parry = 0;
      if (icon->fighting->decker) {
        send_to_icon(icon->fighting, "%s vanishes from your sight.\r\n", CAP(icon->name));
      } else {
        icon->fighting->ic.targ_evasion = success;
      }
    } else
      send_to_icon(icon, "You fail to evade %s.\r\n", icon->fighting->name);
    return;
  }
  send_to_icon(icon, "But you're not fighting anyone.\r\n");
}

ACMD(do_evade)
{
  evade_detection(PERSONA);
}

void parry_attack(struct matrix_icon *icon)
{
  if (icon->fighting)
  {
    int success = maneuver_test(icon);
    if (success > 0) {
      icon->parry = success;
      send_to_icon(icon, "You manage to enhance your defenses.\r\n");
    } else
      send_to_icon(icon, "You fail to enhance your defenses.\r\n");
  } else
    send_to_icon(icon, "But you're not fighting anyone.\r\n");
}

ACMD(do_parry)
{
  parry_attack(PERSONA);
}

void position_attack(struct matrix_icon *icon)
{
  if (icon->fighting)
  {
    int success = maneuver_test(icon);
    if (success > 0) {
      send_to_icon(icon, "You maneuver yourself to attack!\r\n");
      if (!icon->evasion)
        send_to_icon(icon->fighting, "%s maneuver's themselves into a better position!\r\n", CAP(icon->name));
      icon->position = success;
      icon->fighting->position = 0;
    } else if (success < 0) {
      send_to_icon(icon, "You manage to put yoruself in a worse position than before!\r\n");
      if (!icon->evasion)
        send_to_icon(icon->fighting, "%s tries to maneuver into a better position but ends up right in your sights!\r\n", CAP(icon->name));
      icon->position = 0;
      icon->fighting->position = -success;
    } else
      send_to_icon(icon, "You fail to put yourself in a better position.\r\n");
  } else
    send_to_icon(icon, "But you're not fighting anyone.\r\n");
}

ACMD(do_matrix_position)
{
  position_attack(PERSONA);
}

void matrix_fight(struct matrix_icon *icon, struct matrix_icon *targ)
{
  struct obj_data *soft = NULL;
  int target = 0, skill, bod = 0, dam = 0, power, success;
  int iconrating = icon->ic.rating;
  if (!targ)
    return;
  if (!targ->fighting && !(targ->number && (!IS_PROACTIVE(targ) || targ->ic.type == 5)))
  {
    targ->fighting = icon;
    targ->next_fighting = matrix[targ->in_host].fighting;
    matrix[targ->in_host].fighting = targ;
    roll_matrix_init(targ);
    targ->initiative -= matrix[targ->in_host].pass * 10;
    order_list(matrix[targ->in_host].fighting);
  }
  if (targ->decker && !has_spotted(targ, icon))
    make_seen(targ, icon->idnum);

  switch (matrix[icon->in_host].security)
  {
  case 0:
    if (targ->decker)
      target = 6;
    else
      target = 3;
    break;
  case 1:
    if (targ->decker)
      target = 5;
    else
      target = 4;
    break;
  case 2:
    if (targ->decker)
      target = 4;
    else
      target = 5;
    break;
  case 3:
    if (targ->decker)
      target = 3;
    else
      target = 6;
    break;
  case 4:
    if (targ->decker)
      target = 2;
    else
      target = 7;
    break;
  }
  target += targ->parry;
  target -= icon->position;
  targ->parry = 0;
  icon->position = 0;
  if (icon->decker)
  {
    for (soft = icon->decker->software; soft; soft = soft->next_content)
      if (GET_OBJ_VAL(soft, 0) == SOFT_ATTACK)
        break;
    if (!soft)
      return;
    skill = GET_OBJ_VAL(soft, 1) + MIN(GET_MAX_HACKING(icon->decker->ch), GET_REM_HACKING(icon->decker->ch));
    GET_REM_HACKING(icon->decker->ch) -= skill - GET_OBJ_VAL(soft, 1);
    dam = GET_OBJ_VAL(soft, 3);
    power = GET_OBJ_VAL(soft, 1);
  } else
  {
    skill = matrix[icon->in_host].security;
    if (icon->ic.options.IsSet(IC_EX_OFFENSE))
      skill += icon->ic.expert;
    else if (icon->ic.options.IsSet(IC_EX_DEFENSE))
      skill -= icon->ic.expert;
    skill += icon->ic.cascade;
    if (targ->decker->scout) {
      skill += targ->decker->scout;
      targ->decker->scout = 0;
    }
    power = iconrating;
    if (icon->ic.type >= 11) {
      switch (matrix[icon->in_host].colour) {
      case 0:
      case 1:
        dam = MODERATE;
        break;
      case 4:
        power += 2;
      case 3:
      case 2:
        dam = SERIOUS;
        break;
      }
    } else
      switch (matrix[icon->in_host].colour) {
      case 0:
        dam = LIGHT;
        break;
      case 1:
        dam = MODERATE;
        break;
      case 2:
        dam = SERIOUS;
        break;
      case 4:
        power += 2;
      case 3:
        dam = DEADLY;
        break;
      }
  }
  if (targ->decker)
  {
    if (icon->number && (icon->ic.type == 0 || icon->ic.type == 8)) {
      if (!targ->decker->deck)
        return;
      extern const char *crippler[4];
      send_to_icon(targ, "%s takes a shot at your %s program!\r\n", CAP(icon->name), crippler[icon->ic.subtype]);
      switch (icon->ic.subtype) {
      case 0:
        bod = targ->decker->bod;
        break;
      case 1:
        bod = targ->decker->evasion;
        break;
      case 2:
        bod = targ->decker->sensor;
        break;
      case 3:
        bod = targ->decker->masking;
        break;
      }
      success = success_test(matrix[targ->in_host].security, bod);
      int resist = success_test(bod + MIN(GET_MAX_HACKING(targ->decker->ch), GET_REM_HACKING(targ->decker->ch)), iconrating);
      GET_REM_HACKING(targ->decker->ch) = MAX(0, GET_REM_HACKING(targ->decker->ch) - GET_MAX_HACKING(targ->decker->ch));
      success -= resist;
      if (success >= 2) {
        send_to_icon(targ, "It tears into the program!\r\n");
        while (success >= 2) {
          success -= 2;
          bod--;
        }
      } else {
        send_to_icon(targ, "It fails to cause any damage.\r\n");
        return;
      }
      if (icon->ic.type == 0) {
        bod = MAX(bod, 1);
      } else if (bod <= 0) {
        bod = 0;
        success = success_test(iconrating, targ->decker->mpcp + targ->decker->hardening);
        fry_mpcp(icon, targ, success);
      }
      switch (icon->ic.subtype) {
      case 0:
        targ->decker->bod = bod;
        break;
      case 1:
        targ->decker->evasion = bod;
        break;
      case 2:
        targ->decker->sensor = bod;
        break;
      case 3:
        targ->decker->masking = bod;
        break;
      }
      if (targ->decker->mpcp == 0)
        dumpshock(targ);
      return;
    }
    send_to_icon(targ, "%s runs an attack program against you.\r\n", CAP(icon->name));
    if (icon->ic.type >= 11)
      power -= targ->decker->hardening;
    else
      for (soft = targ->decker->software; soft; soft = soft->next_content)
        if (GET_OBJ_VAL(soft, 0) == SOFT_ARMOUR)
          power -= GET_OBJ_VAL(soft, 1);
    bod = targ->decker->bod + MIN(GET_MAX_HACKING(targ->decker->ch), GET_REM_HACKING(targ->decker->ch));
    GET_REM_HACKING(targ->decker->ch) = bod - targ->decker->bod;
    if (!targ->decker->ras)
      power += 4;
  } else
  {
    bod = matrix[targ->in_host].security;
    if (targ->ic.options.IsSet(IC_EX_DEFENSE))
      bod += targ->ic.expert;
    else if (targ->ic.options.IsSet(IC_EX_OFFENSE))
      bod -= targ->ic.expert;
    if (targ->ic.options.IsSet(IC_SHIELD))
      target += 2;
    else if (targ->ic.options.IsSet(IC_SHIFT))
      target += 2;
    if (targ->ic.options.IsSet(IC_ARMOUR))
      power -= 2;
    if (!icon->decker->ras)
      target += 4;
  }
  if (icon->number && icon->ic.type == 6)
    target += targ->decker->redirect;
  success = success_test(skill, target);
  if (success <= 0)
  {
    send_to_icon(icon, "Your program fails to run.\r\n");
    send_to_icon(targ, "%s's program fails to run.\r\n", CAP(icon->name));
    if (!icon->decker && icon->ic.options.IsSet(IC_CASCADE))
      cascade(icon);
    return;
  }
  success -= success_test(bod, power);
  dam = convert_damage(stage(success, dam));
  targ->condition -= dam;
  if (icon->number) {
    if (icon->ic.type == 5)
    {
      if (success && targ->decker->scout < iconrating) {
        send_to_icon(targ, "%s locates your memory address.\r\n", CAP(icon->name));
        targ->decker->scout += success;
        if (targ->decker->scout > iconrating)
          targ->decker->scout = iconrating;
      }
      return;
    } else if (icon->ic.type == 6)
    {
      success -= success_test(targ->decker->evasion, iconrating);
      if (success > 0) {
        icon->ic.subtype = 10;
        for (struct obj_data *soft = targ->decker->software; soft; soft = soft->next_content)
          if (GET_OBJ_VAL(soft, 1) == SOFT_CAMO) {
            icon->ic.subtype += GET_OBJ_VAL(soft, 1);
            break;
          }
        icon->ic.subtype += world[targ->decker->ch->in_room].trace;
        icon->ic.subtype /= success;
        send_to_icon(targ, "%s locks onto your datatrail and vanishes from sight.\r\n", CAP(icon->name));
        {
          struct matrix_icon *temp;
          icon->fighting = NULL;
          REMOVE_FROM_LIST(icon, matrix[icon->in_host].fighting, next_fighting);
          if (targ->fighting == icon) {
            targ->fighting = NULL;
            REMOVE_FROM_LIST(targ, matrix[icon->in_host].fighting, next_fighting);
          }
        }
        for (struct seen_data *seen = targ->decker->seen; seen; seen = seen->next)
          if (seen->idnum == icon->idnum) {
            struct seen_data *temp;
            REMOVE_FROM_LIST(seen, targ->decker->seen, next);
            delete seen;
            break;
          }
      } else
        send_to_icon(targ, "You manage to avoid %s's attack.\r\n", icon->name);
      return;
    }
  }
  switch(dam)
  {
  case 0:
    send_to_icon(targ, "%s's attack reflects off you harmlessly!\r\n", CAP(icon->name));
    send_to_icon(icon, "%s manages to block your attack.\r\n", CAP(targ->name));
    if (!icon->decker && icon->ic.options.IsSet(IC_CASCADE))
      cascade(icon);
    break;
  case 1:
    send_to_icon(targ, "%s's attack skims off you.\r\n", CAP(icon->name));
    send_to_icon(icon, "Your attack skims off of %s.\r\n", targ->name);
    break;
  case 3:
    send_to_icon(targ, "%s's attack sends parts of you flying.\r\n", CAP(icon->name));
    send_to_icon(icon, "Parts fly off of %s from your attack.\r\n", targ->name);
    break;
  case 6:
    send_to_icon(targ, "%s's attack leaves you reeling.\r\n", CAP(icon->name));
    send_to_icon(icon, "Your attack leaves %s reeling.\r\n", targ->name);
    break;
  default:
    send_to_icon(targ, "%s's attack completly obliterates you!\r\n", CAP(icon->name));
    send_to_icon(icon, "You obliterate %s.\r\n", targ->name);
    break;
  }
  if (dam > 0 && targ->decker)
  {
    if (icon->number && icon->ic.type >= 11) {
      int resist = 0;
      bool lethal = icon->ic.type == 11 ? TRUE : FALSE;
      if (!targ->decker->asist[0] && lethal)
        lethal = FALSE;
      switch (matrix[icon->in_host].colour) {
      case 0:
      case 1:
        dam = MODERATE;
        break;
      case 2:
      case 3:
      case 4:
        dam = SERIOUS;
        break;
      }
      if (lethal)
        resist = GET_BOD(targ->decker->ch) + GET_DEFENSE(targ->decker->ch);
      else
        resist = GET_WIL(targ->decker->ch) + GET_DEFENSE(targ->decker->ch);
      success -= targ->decker->iccm ? MAX(success_test(GET_WIL(targ->decker->ch), power), success_test(GET_BOD(targ->decker->ch), power))
                 : success_test(resist, power);
      dam = convert_damage(stage(success, dam));
      send_to_icon(targ, "You smell something burning.\r\n");
      damage(targ->decker->ch, targ->decker->ch, dam, TYPE_BLACKIC, lethal ? PHYSICAL : MENTAL);
      if (targ && targ->decker->ch && !AWAKE(targ->decker->ch)) {
        success = success_test(iconrating * 2, targ->decker->mpcp + targ->decker->hardening);
        fry_mpcp(icon, targ, success);
        dumpshock(targ);
        return;
      } else if (!targ->decker->ch) {
        success = success_test(iconrating * 2, targ->decker->mpcp + targ->decker->hardening);
        fry_mpcp(icon, targ, success);
        extract_icon(targ);
        return;
      }
    } else {
      switch(dam) {
      case 1:
        power = 2;
        break;
      case 3:
        power = 3;
        break;
      case 6:
        power = 5;
        break;
      }
      if (success_test(GET_WIL(targ->decker->ch), power) < 1) {
        send_to_icon(targ, "Your interface overloads.\r\n");
        damage(targ->decker->ch, targ->decker->ch, 1, TYPE_TASER, MENTAL);
      }
    }
  }
  if (targ->condition < 1)
  {
    if (targ->decker) {
      if (icon->number) {
        switch (icon->ic.type) {
        case 9:
          send_to_icon(targ, "%s sends jolts of electricity into your deck!\r\n", CAP(icon->name));
          success = success_test(iconrating, targ->decker->mpcp + targ->decker->hardening + 2);
          fry_mpcp(icon, targ, success);
          success = success - success_test(GET_BOD(targ->decker->ch), iconrating - targ->decker->hardening);
          dam = convert_damage(stage(success, MODERATE));
          damage(targ->decker->ch, targ->decker->ch, dam, TYPE_BLACKIC, PHYSICAL);
          break;
        case 7:
          success = success_test(iconrating, targ->decker->mpcp + targ->decker->hardening);
          fry_mpcp(icon, targ, success);
          break;
        }
      }
      dumpshock(targ);
    } else {
      icon->decker->tally += iconrating;
      if (icon->decker->located)
        icon->decker->tally++;
      sprintf(buf, "%s shatters into a million pieces and vanishes from the node.\r\n", CAP(targ->name));
      send_to_host(icon->in_host, buf, targ, TRUE);
      if (targ->ic.options.IsSet(IC_TRAP) && real_ic(targ->ic.trap) > 0) {
        struct matrix_icon *trap = read_ic(targ->ic.trap, VIRTUAL);
        trap->ic.target = icon->idnum;
        icon_to_host(trap, icon->in_host);
      }
      extract_icon(targ);
      check_trigger(icon->in_host, icon->decker->ch);
      return;
    }
  }
}

ACMD(do_matrix_score)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  int detect = 0;
  for (struct obj_data *soft = DECKER->software; soft; soft = soft->next_content)
    if (GET_OBJ_VAL(soft, 0) == SOFT_SLEAZE)
      detect = GET_OBJ_VAL(soft, 1);
  detect += DECKER->masking;
  detect = detect / 2;

  sprintf(buf, "You are connected to the matrix.\r\n"
          "  Condition:^B%3d^n       Physical:^R%3d(%2d)^n\r\n"
          "  Detection:^r%3d^n       Hacking Pool:^g%3d/%3d (%2d)^n\r\n"
          "Persona Programs:\r\n"
          "       Bod:^g%3d^n       Evasion:^g%3d^n\r\n"
          "   Masking:^g%3d^n       Sensors:^g%3d^n\r\n"
          "Deck Status:\r\n"
          " Hardening:^g%3d^n       MPCP:^g%3d^n\r\n"
          "  IO Speed:^g%3d^n       Response Increase:^g%3d^n\r\n",
          PERSONA->condition, (int)(GET_PHYSICAL(ch) / 100), (int)(GET_MAX_PHYSICAL(ch) / 100),
          detect, MAX(0, GET_REM_HACKING(ch)), GET_HACKING(ch), GET_MAX_HACKING(ch),
          DECKER->bod, DECKER->evasion, DECKER->masking, DECKER->sensor,
          DECKER->hardening, DECKER->mpcp, DECKER->deck ? GET_OBJ_VAL(DECKER->deck, 4) : 0, DECKER->response);
  send_to_icon(PERSONA, buf);
}

ACMD(do_locate)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  two_arguments(argument, buf, arg);
  int success, i = 0;
  if (is_abbrev(buf, "host")) {
    success = system_test(PERSONA->in_host, ch, TEST_INDEX, SOFT_BROWSE, 0);
    int x = 0;
    char *name = arg;
    while (*name) {
      x++;
      name++;
    }
    if (x < 3) {
      send_to_icon(PERSONA, "You return %d useless matches.\r\n", number(1000, 30000));
      return;
    }
    if (success > 0) {
      for (struct exit_data *exit = matrix[PERSONA->in_host].exit; exit; exit = exit->next)
        if (real_host(exit->host) > 0)
          if (isname(arg, matrix[real_host(exit->host)].keywords) && matrix[real_host(exit->host)].alert <= 2) {
            send_to_icon(PERSONA, "Your search returns the address %s.\r\n", exit->number);
            i++;
          }
    }
    if (!success || !i)
      send_to_icon(PERSONA, "You fail to return any data on that search.\r\n");
    return;
  } else if (is_abbrev(buf, "ic")) {
    success = system_test(PERSONA->in_host, ch, TEST_INDEX, SOFT_ANALYZE, 0);
    if (success > 0) {
      for (struct matrix_icon *icon = matrix[PERSONA->in_host].icons; icon; icon = icon->next_in_host)
        if (icon->number > 0) {
          if (icon->evasion)
            icon->evasion = 0;
          make_seen(PERSONA, icon->idnum);
          i++;
        }
    }
    if (!success || !i)
      send_to_icon(PERSONA, "You don't notice any IC in the host.\r\n");
    else
      send_to_icon(PERSONA, "You notice %d IC in the host.\r\n", i);
    return;
  } else if (is_abbrev(buf, "file")) {
    int x = 0;
    char *name = arg;
    while (*name) {
      x++;
      name++;
    }
    if (x < 3) {
      send_to_icon(PERSONA, "You return %d useless matches.\r\n", number(1000, 30000));
      return;
    }
    success = system_test(PERSONA->in_host, ch, TEST_INDEX, SOFT_BROWSE, 0);
    if (PERSONA) {
      for (struct obj_data *obj = matrix[PERSONA->in_host].file; obj && success > 0; obj = obj->next_content)
        if (GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY || GET_OBJ_TYPE(obj) == ITEM_PROGRAM) {
          if (!GET_OBJ_VAL(obj, 7) && isname(arg, GET_OBJ_NAME(obj))) {
            GET_OBJ_VAL(obj, 7) = GET_IDNUM(ch);
            GET_OBJ_VAL(obj, 9) = 0;
            success--;
            i++;
          } else if (GET_OBJ_VAL(obj, 7) == GET_IDNUM(ch) && GET_OBJ_VAL(obj, 9) && isname(arg, GET_OBJ_NAME(obj))) {
            GET_OBJ_VAL(obj, 9) = 0;
            success--;
            i++;
          }
        }
      if (!i)
        send_to_icon(PERSONA, "You fail to return any data on that search.\r\n");
      else
        send_to_icon(PERSONA, "Your search returns %d match%s.\r\n", i, i > 1 ? "es" : "");
    }
    return;
  } else if (is_abbrev(buf, "decker")) {
    success = system_test(PERSONA->in_host, ch, TEST_INDEX, SOFT_SCANNER, 0);
    if (success > 0) {
      int sensor = 0;
      for (int r = DECKER->sensor;r > 0; r--) {
        i = srdice();
        if (i > sensor)
          sensor = i;
      }
      i = 0;
      for (struct matrix_icon *icon = matrix[PERSONA->in_host].icons; icon; icon = icon->next_in_host)
        if (icon->decker && icon != PERSONA) {
          int targ = icon->decker->masking;
          for (struct obj_data *soft = icon->decker->software; soft; soft = soft->next_content)
            if (GET_OBJ_VAL(soft, 0) == SOFT_SLEAZE)
              targ += GET_OBJ_VAL(soft, 1);
          if (sensor >= targ) {
            make_seen(PERSONA, icon->idnum);
            i++;
          }
        }
    }
    if (!success || !i)
      send_to_icon(PERSONA, "You don't notice any other deckers in the host.\r\n");
    else
      send_to_icon(PERSONA, "You notice %d other decker%c in the host.\r\n", i, (i > 1 ? "s":""));
    return;
  } else if (is_abbrev(buf, "paydata")) {
    success = system_test(PERSONA->in_host, ch, TEST_INDEX, SOFT_EVALUATE, 0);
    if (success > 0 && !matrix[PERSONA->in_host].type && matrix[PERSONA->in_host].found)
      while (success && matrix[PERSONA->in_host].found) {
        matrix[PERSONA->in_host].found--;
        success--;
        i++;
        struct obj_data *obj = read_object(106, VIRTUAL);
        GET_OBJ_VAL(obj, 0) = 0;
        GET_OBJ_VAL(obj, 1) = time(0);
        GET_OBJ_VAL(obj, 2) = (number(1, 6) + number(1, 6)) * MAX(5, (20 - (5 * matrix[PERSONA->in_host].colour)));
        GET_OBJ_VAL(obj, 3) = matrix[PERSONA->in_host].vnum;
        GET_OBJ_VAL(obj, 4) = matrix[PERSONA->in_host].colour;
        GET_OBJ_VAL(obj, 7) = PERSONA->idnum;
        sprintf(buf, "Paydata %s - %dMp", matrix[PERSONA->in_host].name, GET_OBJ_VAL(obj, 2));
        STR_DUP(obj->restring, buf);
        int defense[5][6] = {{ 0, 0, 0, 1, 1, 1 },
                             { 0, 0, 1, 1, 2, 2 },
                             { 0, 1, 1, 2, 2, 3 },
                             { 1, 1, 2, 2, 3, 3 },
                             { 1, 2, 2, 3, 3, 3 }};
        int def = defense[matrix[PERSONA->in_host].colour][number(0, 5)];
        if (def) {
          int rate[4];
          int rating = number(1, 6) + number(1, 6);
          if (rating < 6) {
            rate[0] = 4;
            rate[1] = 5;
            rate[2] = 6;
            rate[3] = 8;
          } else if (rating < 9) {
            rate[0] = 5;
            rate[1] = 7;
            rate[2] = 8;
            rate[3] = 10;
          } else if (rating < 12) {
            rate[0] = 6;
            rate[1] = 9;
            rate[2] = 10;
            rate[3] = 11;
          } else {
            rate[0] = 7;
            rate[1] = 10;
            rate[2] = 12;
            rate[3] = 12;
          }
          if (matrix[PERSONA->in_host].security < 5)
            GET_OBJ_VAL(obj, 6) = rate[0];
          else if (matrix[PERSONA->in_host].security < 8)
            GET_OBJ_VAL(obj, 6) = rate[1];
          else if (matrix[PERSONA->in_host].security < 11)
            GET_OBJ_VAL(obj, 6) = rate[2];
          else
            GET_OBJ_VAL(obj, 6) = rate[3];
          switch (def) {
          case 1:
            GET_OBJ_VAL(obj, 5) = 1;
            break;
          case 2:
          case 3:
            if (number(1, 6) > 4)
              GET_OBJ_VAL(obj, 5) = 4;
            else
              GET_OBJ_VAL(obj, 5) = 2;
            break;
          }
        }
        if (matrix[PERSONA->in_host].file)
          obj->next_content = matrix[PERSONA->in_host].file;
        matrix[PERSONA->in_host].file = obj;
      }
    if (!i)
      send_to_icon(PERSONA, "You fail to find any paydata.\r\n");
    else
      send_to_icon(PERSONA, "You find %d piece%sof paydata.\r\n", i, i > 1 ? "s ": " ");
    return;
  }
  send_to_icon(PERSONA, "Locate what?\r\n");

}

ACMD(do_matrix_look)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  sprintf(buf, "^C%s^n\r\n%s", matrix[PERSONA->in_host].name, matrix[PERSONA->in_host].desc);
  send_to_icon(PERSONA, buf);
  for (struct matrix_icon *icon = matrix[PERSONA->in_host].icons; icon; icon = icon->next_in_host)
    if (has_spotted(PERSONA, icon))
      send_to_icon(PERSONA, "^Y%s^n\r\n", icon->look_desc);
  for (struct obj_data *obj = matrix[PERSONA->in_host].file; obj; obj = obj->next_content)
    if (GET_OBJ_VAL(obj, 7) == PERSONA->idnum && !GET_OBJ_VAL(obj, 9))
      send_to_icon(PERSONA, "^yA file named %s floats here.^n\r\n", GET_OBJ_NAME(obj));
}

ACMD(do_analyze)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  if (!*argument) {
    send_to_icon(PERSONA, "Analyze What?\r\n");
    return;
  }
  int success;
  one_argument(argument, arg);
  if (is_abbrev(arg, "host")) {
    success = system_test(PERSONA->in_host, ch, TEST_CONTROL, SOFT_ANALYZE, 0);
    if (success > 0) {
      send_to_icon(PERSONA, "You analyze the host.\r\n");
      int found[] = { 0, 0, 0, 0, 0, 0, 0};
      if (success < 7) {
        for (int current;success; success--) {
          current = number(0, 6);
          int start = current;
          while (found[current]) {
            current++;
            if (current > 6)
              current = 0;
            if (current == start)
              break;
          }
          found[current] = 1;
        }
        if (found[0])
          sprintf(buf, "%s-", host_sec[matrix[PERSONA->in_host].colour]);
        else
          sprintf(buf, "?-");
        if (found[1])
          sprintf(buf, "%s%d ", buf, matrix[PERSONA->in_host].security);
        else
          strcat(buf, "? ");
        if (found[2])
          sprintf(buf, "%s%ld/", buf, matrix[PERSONA->in_host].stats[ACCESS][0]);
        else
          strcat(buf, "0/");
        if (found[3])
          sprintf(buf, "%s%ld/", buf, matrix[PERSONA->in_host].stats[CONTROL][0]);
        else
          strcat(buf, "0/");
        if (found[4])
          sprintf(buf, "%s%ld/", buf, matrix[PERSONA->in_host].stats[INDEX][0]);
        else
          strcat(buf, "0/");
        if (found[5])
          sprintf(buf, "%s%ld/", buf, matrix[PERSONA->in_host].stats[FILES][0]);
        else
          strcat(buf, "0/");
        if (found[6])
          sprintf(buf, "%s%ld/", buf, matrix[PERSONA->in_host].stats[SLAVE][0]);
        else
          strcat(buf, "0");
        strcat(buf, "\r\n");
      } else
        sprintf(buf, "%s-%d %ld/%ld/%ld/%ld/%ld\r\n", host_sec[matrix[PERSONA->in_host].colour],
                matrix[PERSONA->in_host].security, matrix[PERSONA->in_host].stats[ACCESS][0],
                matrix[PERSONA->in_host].stats[CONTROL][0], matrix[PERSONA->in_host].stats[INDEX][0],
                matrix[PERSONA->in_host].stats[FILES][0], matrix[PERSONA->in_host].stats[SLAVE][0]);
      send_to_char(buf, ch);
    } else
      send_to_icon(PERSONA, "Your program fails to run.\r\n");
    return;
  } else if (is_abbrev(arg, "security")) {
    success = system_test(PERSONA->in_host, ch, TEST_CONTROL, SOFT_ANALYZE, 0);
    if (success > 0) {
      send_to_icon(PERSONA, "You analyze the security of the host.\r\n");
      sprintf(buf, "%s-%d Tally: %d Alert: %s\r\n", host_sec[matrix[PERSONA->in_host].colour],
              matrix[PERSONA->in_host].security, DECKER->tally,
              alerts[matrix[PERSONA->in_host].alert]);
      send_to_icon(PERSONA, buf);
    } else
      send_to_icon(PERSONA, "Your program fails to run.\r\n");
    return;
  } else if (is_abbrev(arg, "access") || is_abbrev(arg, "control") || is_abbrev(arg, "index") || is_abbrev(arg, "files")  || is_abbrev(arg, "slave")) {
    int mode = 0;
    if (is_abbrev(arg, "access"))
      mode = TEST_ACCESS;
    else if (is_abbrev(arg, "control"))
      mode = TEST_CONTROL;
    else if (is_abbrev(arg, "index"))
      mode = TEST_INDEX;
    else if (is_abbrev(arg, "files"))
      mode = TEST_FILES;
    else if (is_abbrev(arg, "slave"))
      mode = TEST_SLAVE;
    int success = system_test(PERSONA->in_host, ch, mode, SOFT_ANALYZE, 0);
    if (success > 0) {
      if (matrix[PERSONA->in_host].stats[mode][1] || matrix[PERSONA->in_host].stats[mode][5]) {
        if (matrix[PERSONA->in_host].stats[mode][1])
          send_to_icon(PERSONA, "The subsystem is protected by Scramble-%ld IC.\r\n", matrix[PERSONA->in_host].stats[mode][2]);
        if (matrix[PERSONA->in_host].stats[mode][5])
          send_to_icon(PERSONA, "There is a trapdoor located in this subsystem.\r\n");
      } else send_to_icon(PERSONA, "There is nothing out of the ordinary about that subsystem.\r\n");
    } else send_to_icon(PERSONA, "Your program fails to run.\r\n");
    return;
  } else {
    struct obj_data *obj = NULL;
    if ((obj = get_obj_in_list_vis(ch, arg, matrix[PERSONA->in_host].file)) && GET_OBJ_VAL(obj, 7) == PERSONA->idnum) {
      int success = system_test(PERSONA->in_host, ch, TEST_CONTROL, SOFT_ANALYZE, 0);
      if (success > 0) {
        send_to_icon(PERSONA, "You analyze the file:\r\n");
        if (GET_OBJ_VAL(obj, 3)) {
          send_to_icon(PERSONA, "Paydata %dMp\r\n", GET_OBJ_VAL(obj, 2));
          if (GET_OBJ_VAL(obj, 5)) {
            send_to_icon(PERSONA, "Protected by: %s-%d\r\n", GET_OBJ_VAL(obj, 5) == 1 ? "Scramble" :
                         GET_OBJ_VAL(obj, 5) == 2 ? "Data Bomb" : "Pavlov", GET_OBJ_VAL(obj, 6));
          }
        } else {
          send_to_icon(PERSONA, "%s - %dMp\r\n", GET_OBJ_NAME(obj), GET_OBJ_VAL(obj, 2));
          send_to_icon(PERSONA, obj->photo ? obj->photo : obj->text.look_desc);
        }
      } else
        send_to_icon(PERSONA, "You fail to get any useful information out of your request.\r\n");
      return;
    }
    for (struct matrix_icon *ic = matrix[PERSONA->in_host].icons; ic; ic = ic->next_in_host)
      if ((isname(arg, ic->look_desc) || isname(arg, ic->name)) && has_spotted(PERSONA, ic)) {
        int success = system_test(PERSONA->in_host, ch, TEST_CONTROL, SOFT_ANALYZE, 0);
        if (success > 0 ) {
          send_to_icon(PERSONA, "%s", ic->long_desc);
          if (ic->number) {
            send_to_icon(PERSONA, "%s is a %s-%d\r\n", CAP(ic->name), ic_type[ic->ic.type],
                         matrix[PERSONA->in_host].shutdown ? ic->ic.rating - 2 : ic->ic.rating);
            if (ic->ic.options.AreAnySet(IC_ARMOUR, IC_CASCADE, IC_EX_OFFENSE, IC_EX_DEFENSE, IC_TRAP, IC_SHIELD, IC_SHIFT, ENDBIT)) {
              ic->ic.options.PrintBits(buf1, MAX_STRING_LENGTH, ic_option_long, IC_TRAP + 1);
              buf1[strlen(buf1) - 2] = '\0';
              send_to_icon(PERSONA, "It has the following options: %s.\r\n", buf1);
            }
          }
        } else
          send_to_icon(PERSONA, "You fail to get any useful information out of your request.\r\n");
        return;
      }
  }
  send_to_icon(PERSONA, "Analyze what?\r\n");
}


ACMD(do_logon)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  skip_spaces(&argument);
  rnum_t target_host = 0;
  if (!str_cmp(argument, "LTG")) {
    if (!(target_host = real_host(matrix[PERSONA->in_host].parent))
        && !(matrix[target_host].type == HOST_LTG || matrix[target_host].type == HOST_PLTG)) {
      send_to_icon(PERSONA, "This host is not connected to a LTG.\r\n");
      return;
    }
  } else if (!str_cmp(argument, "RTG")) {
    if (matrix[PERSONA->in_host].type != HOST_LTG ||
        !(target_host = real_host(matrix[PERSONA->in_host].parent))) {
      send_to_icon(PERSONA, "This host is not connected to a RTG.\r\n");
      return;
    }
  } else if (!str_cmp(argument, "access") && matrix[PERSONA->in_host].stats[ACCESS][5])
    target_host = real_host(matrix[PERSONA->in_host].stats[ACCESS][5]);
  else if (!str_cmp(argument, "control") && matrix[PERSONA->in_host].stats[CONTROL][5])
    target_host = real_host(matrix[PERSONA->in_host].stats[CONTROL][5]);
  else if (!str_cmp(argument, "index") && matrix[PERSONA->in_host].stats[INDEX][5])
    target_host = real_host(matrix[PERSONA->in_host].stats[INDEX][5]);
  else if (!str_cmp(argument, "files") && matrix[PERSONA->in_host].stats[FILES][5])
    target_host = real_host(matrix[PERSONA->in_host].stats[FILES][5]);
  else if (!str_cmp(argument, "slave") && matrix[PERSONA->in_host].stats[SLAVE][5])
    target_host = real_host(matrix[PERSONA->in_host].stats[SLAVE][5]);
  else
    for (struct exit_data *exit = matrix[PERSONA->in_host].exit; exit; exit = exit->next)
      if (!str_cmp(argument, exit->number))
        target_host = real_host(exit->host);

  if (target_host > 0 && matrix[target_host].alert <= 2) {
    int success = system_test(target_host, ch, TEST_ACCESS, SOFT_DECEPTION, 0);
    if (success > 0) {
      if (matrix[target_host].type == HOST_RTG && matrix[PERSONA->in_host].type == HOST_RTG)
        DECKER->tally = 0;
      DECKER->last_trigger = 0;
      DECKER->located = FALSE;
      sprintf(buf, "%s connects to a different host and vanishes from this one.\r\n", PERSONA->name);
      send_to_host(PERSONA->in_host, buf, PERSONA, TRUE);
      send_to_icon(PERSONA, "You connect to %s.\r\n", matrix[target_host].name);
      icon_from_host(PERSONA);
      icon_to_host(PERSONA, target_host);
      do_matrix_look(ch, NULL, 0, 0);
      return;
    } else {
      send_to_icon(PERSONA, "You fail to gain access to the host.\r\n");
      return;
    }
  }
  send_to_icon(PERSONA, "That host doesn't seem to be connected to the network.\r\n");
}

ACMD(do_logoff)
{
  if (!PERSONA) {
    send_to_char(ch, "You yank the plug out and return to the real world.\r\n");
    PLR_FLAGS(ch).RemoveBit(PLR_MATRIX);
    for (struct char_data *temp = world[ch->in_room].people; temp; temp = temp->next_in_room)
      if (PLR_FLAGGED(temp, PLR_MATRIX)) {
        send_to_char("Your hitcher has disconnected.\r\n", ch);
        temp->persona->decker->hitcher = NULL;
      }
    return;
  }
  if (subcmd) {
    send_to_char(ch, "You yank the plug out and return to the real world.\r\n");
    for (struct matrix_icon *icon = matrix[PERSONA->in_host].icons; PERSONA && icon; icon = icon->next_in_host)
      if (icon->fighting == PERSONA && icon->ic.type >= 11) {
        send_to_icon(PERSONA, "The IC takes a final shot.\r\n");
        matrix_fight(icon, PERSONA);
      }
    if (PERSONA)
      dumpshock(PERSONA);
    return;
  } else {
    for (struct matrix_icon *icon = matrix[PERSONA->in_host].icons; icon; icon = icon->next_in_host)
      if (icon->fighting == PERSONA && icon->ic.type >= 11) {
        send_to_icon(PERSONA, "You can't log off gracefully while fighting a black IC!\r\n");
        return;
      }
    int success = system_test(PERSONA->in_host, ch, TEST_ACCESS, SOFT_DECEPTION, 0);
    if (success <= 0) {
      send_to_icon(PERSONA, "You fail to log off successfully.\r\n");
      return;
    }
    send_to_icon(PERSONA, "You gracefully log off from the matrix and return to the real world.\r\n");
  }
  sprintf(buf, "%s depixilates and vanishes from the host.\r\n", PERSONA->name);
  send_to_host(PERSONA->in_host, buf, PERSONA, FALSE);
  extract_icon(PERSONA);
  PERSONA = NULL;
  PLR_FLAGS(ch).RemoveBit(PLR_MATRIX);
}

ACMD(do_connect)
{
  struct char_data *temp;
  struct matrix_icon *icon = NULL;
  struct obj_data *cyber, *cyberdeck = NULL, *jack = NULL;
  rnum_t host;

  if ((!world[ch->in_room].matrix || (host = real_host(world[ch->in_room].matrix)) < 1)) {
    send_to_char("You cannot connect to the matrix from here.\r\n", ch);
    return;
  }
  if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }

  for (jack = ch->cyberware; jack; jack = jack->next_content)
    if (GET_OBJ_VAL(jack, 0) == CYB_DATAJACK || (GET_OBJ_VAL(jack, 0) == CYB_EYES && IS_SET(GET_OBJ_VAL(jack, 3), EYE_DATAJACK)))
      break;

  if (!jack) {
    send_to_char("You need a datajack to connect to the matrix.\r\n", ch);
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

  for (temp = world[ch->in_room].people; temp; temp = temp->next_in_room)
    if (PLR_FLAGGED(temp, PLR_MATRIX)) {
      if (temp->persona && temp->persona->decker->deck)
        for (struct obj_data *hitch = temp->persona->decker->deck->contains; hitch; hitch = hitch->next_content)
          if (GET_OBJ_TYPE(hitch) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(hitch, 0) == 1 &&
              GET_OBJ_VAL(hitch, 1) == 3) {
            for (struct char_data *temp2 = world[ch->in_room].people; temp2; temp2 = temp2->next_in_room)
              if (temp2 != temp && PLR_FLAGGED(temp2, PLR_MATRIX)) {
                send_to_char(ch, "The hitcher jack on that deck is already in use.\r\n");
                return;
              }
            act("You slip your jack into $n's hitcher port.", FALSE, temp, 0, ch, TO_VICT);
            send_to_char("Someone has connected to your hitcher port.\r\n", temp);
            PLR_FLAGS(ch).SetBit(PLR_MATRIX);
            temp->persona->decker->hitcher = ch;
            return;
          }
      send_to_char("The jackpoint is already in use.\r\n", ch);
      return;
    }

  for (cyber = ch->carrying; !cyberdeck && cyber; cyber = cyber->next_content)
    if (GET_OBJ_TYPE(cyber) == ITEM_CYBERDECK || GET_OBJ_TYPE(cyber) == ITEM_CUSTOM_DECK)
      cyberdeck = cyber;
  for (int i = 0; !cyberdeck && i < NUM_WEARS; i++)
    if (GET_EQ(ch, i) && (GET_OBJ_TYPE(GET_EQ(ch,i )) == ITEM_CYBERDECK || GET_OBJ_TYPE(GET_EQ(ch,i )) == ITEM_CUSTOM_DECK))
      cyberdeck = GET_EQ(ch, i);
  if (!cyberdeck) {
    send_to_char(ch, "I don't recommend trying to do that without a cyberdeck.\r\n");
    return;
  }

  if (GET_OBJ_VAL(cyberdeck, 0) == 0) {
    send_to_char("You cannot connect to the matrix with fried MPCP chips!\r\n", ch);
    return;
  }
  if (matrix[host].alert > 2) {
    send_to_char("It seems that host has been shut down.\r\n", ch);
    return;
  }
  GET_POS(ch) = POS_SITTING;
  icon = Mem->GetIcon();
  icon->condition = 10;
  if (ch->player.matrix_text.name)
    STR_DUP(icon->name, ch->player.matrix_text.name)
  else
    STR_DUP(icon->name, "a nondescript persona")
  if (ch->player.matrix_text.room_desc)
    STR_DUP(icon->look_desc, ch->player.matrix_text.room_desc)
  else
    STR_DUP(icon->look_desc, "A nondescript persona stands idly here.\r\n")
  if (ch->player.matrix_text.look_desc)
    STR_DUP(icon->long_desc, ch->player.matrix_text.look_desc)
  else
    STR_DUP(icon->long_desc, "A nondescript persona stands idly here.\r\n")

  if (GET_OBJ_TYPE(cyberdeck) == ITEM_CUSTOM_DECK && GET_OBJ_VAL(cyberdeck, 9)) {
    send_to_char(ch, "That deck is missing some components.\r\n");
    return;
  }
  PERSONA = icon;
  EQUALS_NEW(DECKER, deck_info);
  if (GET_OBJ_VNUM(cyberdeck) == 113) {
    struct obj_data *parts = cyberdeck->contains;
    for (; parts; parts = parts->next_content)
      if (GET_OBJ_TYPE(parts) == ITEM_PART && (GET_OBJ_VAL(parts, 0) == PART_ASIST_HOT || GET_OBJ_VAL(parts, 0) == PART_ASIST_COLD))
        break;
    if (!parts) {
      for (cyber = ch->cyberware; cyber; cyber = cyber->next_content)
        if (GET_OBJ_VAL(cyber, 0) == CYB_ASIST)
          break;
      if (!cyber) {
        send_to_char("You need an ASIST interface to use that.\r\n", ch);
        extract_icon(PERSONA);
        PERSONA = NULL;
        return;
      }
      DECKER->asist[1] = 0;
      DECKER->asist[0] = 0;
      GET_MAX_HACKING(ch) = 0;
      DECKER->response = 0;
    }
  }
  DECKER->ch = ch;
  EQUALS_NEW(DECKER->phone, phone_data);
  DECKER->phone->next = phone_list;
  phone_list = DECKER->phone;
  DECKER->phone->persona = PERSONA;
  DECKER->phone->rtg = world[ch->in_room].rtg;
  DECKER->phone->number = world[ch->in_room].jacknumber;
  DECKER->mxp = ch->in_room * DECKER->phone->number / DECKER->phone->rtg;
  PERSONA->idnum = GET_IDNUM(ch);
  DECKER->deck = cyberdeck;
  DECKER->mpcp = GET_OBJ_VAL(cyberdeck, 0);
  DECKER->hardening = GET_OBJ_VAL(cyberdeck, 1);
  DECKER->active = GET_OBJ_VAL(cyberdeck, 2);
  DECKER->response = GET_OBJ_VAL(cyberdeck, 6);
  affect_total(ch);
  GET_REM_HACKING(ch) = GET_HACKING(ch);
  GET_MAX_HACKING(ch) = (int)(GET_HACKING(ch) / 3);
  if (world[ch->in_room].io == 0)
    DECKER->io = GET_OBJ_VAL(cyberdeck, 4);
  else if (world[ch->in_room].io == -1)
    DECKER->io = MIN(DECKER->mpcp * 50, GET_OBJ_VAL(cyberdeck, 4));
  else
    DECKER->io = MIN(world[ch->in_room].io, GET_OBJ_VAL(cyberdeck, 4));
  DECKER->io = (int)(DECKER->io / 10);
  if (GET_OBJ_VNUM(cyberdeck) != 113) {
    DECKER->asist[1] = 0;
    DECKER->asist[0] = 0;
    GET_MAX_HACKING(ch) = 0;
    DECKER->response = 0;
  }
  for (struct obj_data *soft = cyberdeck->contains; soft; soft = soft->next_content)
    if (GET_OBJ_TYPE(soft) == ITEM_PROGRAM) {
      GET_OBJ_VAL(soft, 8) = GET_OBJ_VAL(soft, 9) = 0;
      if (GET_OBJ_VNUM(soft) != 108) {
        switch (GET_OBJ_VAL(soft, 0)) {
        case SOFT_BOD:
          DECKER->bod = GET_OBJ_VAL(soft, 1);
          break;
        case SOFT_SENSOR:
          DECKER->sensor = GET_OBJ_VAL(soft, 1);
          break;
        case SOFT_MASKING:
          DECKER->masking = GET_OBJ_VAL(soft, 1);
          break;
        case SOFT_EVASION:
          DECKER->evasion = GET_OBJ_VAL(soft, 1);
          break;
        }
      }
      if (GET_OBJ_VAL(soft, 4)) {
        if (GET_OBJ_VAL(soft, 2) > DECKER->active || GET_OBJ_VAL(soft, 1) > DECKER->mpcp)
          continue;
        struct obj_data *active = read_object(GET_OBJ_RNUM(soft), REAL);
        if (soft->restring)
          STR_DUP(active->restring, soft->restring);
        for (int x = 0; x < 10; x++)
          GET_OBJ_VAL(active, x) = GET_OBJ_VAL(soft, x);
        DECKER->active -= GET_OBJ_VAL(active, 2);
        if (DECKER->software)
          active->next_content = DECKER->software;
        DECKER->software = active;
      }

    } else if (GET_OBJ_TYPE(soft) == ITEM_PART) {
      switch (GET_OBJ_VAL(soft, 0)) {
      case PART_BOD:
        DECKER->bod = GET_OBJ_VAL(soft, 1);
        break;
      case PART_SENSOR:
        DECKER->sensor = GET_OBJ_VAL(soft, 1);
        break;
      case PART_MASKING:
        DECKER->masking = GET_OBJ_VAL(soft, 1);
        break;
      case PART_EVASION:
        DECKER->evasion = GET_OBJ_VAL(soft, 1);
        break;
      case PART_ASIST_HOT:
        DECKER->asist[1] = 1;
        DECKER->asist[0] = 1;
        break;
      case PART_ASIST_COLD:
        DECKER->asist[1] = 0;
        DECKER->asist[0] = 0;
        GET_MAX_HACKING(ch) = 0;
        DECKER->response = 0;
        break;
      case PART_RAS_OVERRIDE:
        DECKER->ras = TRUE;
        break;
      case PART_REALITY_FILTER:
        DECKER->reality = TRUE;
        break;
      case PART_ICCM:
        DECKER->iccm = TRUE;
        break;
      }
    }
  if (icon_list)
    PERSONA->next = icon_list;
  icon_list = PERSONA;
  icon_to_host(PERSONA, host);
  if (DECKER->bod + DECKER->sensor + DECKER->evasion + DECKER->masking > DECKER->mpcp * 3) {
    send_to_char("Your deck overloads on persona programs and crashes.\r\n", ch);
    extract_icon(PERSONA);
    PERSONA = NULL;
    return;
  }
  if (GET_OBJ_VAL(jack, 0) == CYB_DATAJACK && GET_OBJ_VAL(jack, 3) == DATA_INDUCTION)
    sprintf(buf, "$n places $s hand over $s induction pad as $e connects to $s cyberdeck.");
  else if (GET_OBJ_VAL(jack, 0) == CYB_DATAJACK)
    sprintf(buf, "$n slides one end of the cable into $s datajack and the other into $s cyberdeck.");
  else sprintf(buf, "$n's eye opens up as $e slides $s cyberdeck cable into $s eye datajack.");
  act(buf, FALSE, ch, 0, 0, TO_ROOM);
  act("You jack into the matrix.", FALSE, ch, 0, 0, TO_CHAR);
  PLR_FLAGS(ch).SetBit(PLR_MATRIX);
  do_matrix_look(ch, NULL, 0, 0);
}

ACMD(do_load)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  if (!DECKER->deck) {
    send_to_char(ch, "You need a deck to %s from!\r\n", subcmd == SCMD_UNLOAD ? "unload" : "upload");
    return;
  }
  if (!*argument) {
    send_to_char(ch, "What do you want to %s?\r\n", subcmd == SCMD_UNLOAD ? "unload" : "upload");
  }
  skip_spaces(&argument);
  if (subcmd == SCMD_UNLOAD) {
    struct obj_data *temp = NULL;
    for (struct obj_data *soft = DECKER->software; soft; soft = soft->next_content) {
      if (isname(argument, soft->text.keywords) || isname(argument, GET_OBJ_NAME(soft))) {
        send_to_icon(PERSONA, "You erase %s from active memory.\r\n", GET_OBJ_NAME(soft));
        if (temp)
          temp->next_content = soft->next_content;
        else
          DECKER->software = soft->next_content;
        soft->next_content = NULL;
        DECKER->active += GET_OBJ_VAL(soft, 2);
        extract_obj(soft);
        return;
      }
      temp = soft;
    }
  } else
    for (struct obj_data *soft = DECKER->deck->contains; soft; soft = soft->next_content)
      if ((isname(argument, soft->text.keywords) || isname(argument, soft->restring))
          && (GET_OBJ_VAL(soft, 0) > SOFT_SENSOR ||
              (GET_OBJ_TYPE(soft) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(soft, 0) == TYPE_FILE))) {
        if (subcmd == SCMD_SWAP && GET_OBJ_VAL(soft, 2) > DECKER->active) {
          send_to_icon(PERSONA, "You don't have enough active memory to load that program.\r\n");
        } else if (subcmd == SCMD_SWAP && GET_OBJ_VAL(soft, 1) > DECKER->mpcp) {
          send_to_icon(PERSONA, "Your deck is not powerful enough to run that program.\r\n");
        } else {
          int success = 1;
          if (subcmd == SCMD_UPLOAD)
            success = system_test(PERSONA->in_host, ch, TEST_FILES, SOFT_READ, 0);
          if (success > 0) {
            GET_OBJ_VAL(soft, 9) = GET_OBJ_VAL(soft, 2);
            if (subcmd == SCMD_UPLOAD)
              GET_OBJ_VAL(soft, 8) = 1;
            else
              DECKER->active -= GET_OBJ_VAL(soft, 2);
            send_to_icon(PERSONA, "You begin to upload %s to %s.\r\n", GET_OBJ_NAME(soft), (subcmd ? "the host" : "your icon"));
          } else
            send_to_icon(PERSONA, "Your commands fail to execute.\r\n");
        }
        return;
      }
  send_to_icon(PERSONA, "You don't have that file.\r\n");
}

ACMD(do_redirect)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  if (matrix[PERSONA->in_host].type != HOST_RTG) {
    send_to_icon(PERSONA, "You can only perform this operation on an RTG.\r\n");
    return;
  }
  int success = system_test(PERSONA->in_host, ch, TEST_CONTROL, SOFT_CAMO, 0);
  if (success > 0) {
    for (int x = 0; x < DECKER->redirect; x++)
      if (DECKER->redirectedon[x] == PERSONA->in_host) {
        send_to_icon(PERSONA, "You have already redirected on this RTG.\r\n");
        return;
      }
    DECKER->redirect++;
    long *temp = new long[DECKER->redirect];
    for (int x = 0; x < DECKER->redirect - 1; x++)
      temp[x] = DECKER->redirectedon[x];
    temp[DECKER->redirect - 1] = PERSONA->in_host;
    DELETE_ARRAY_IF_EXTANT(DECKER->redirectedon);
    DECKER->redirectedon = temp;
    send_to_icon(PERSONA, "You successfully redirect your datatrail.\r\n");
  } else
    send_to_icon(PERSONA, "Your program fails to run.\r\n");
}

ACMD(do_download)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  if (!DECKER->deck) {
    send_to_char(ch, "You don't have anywhere to download it to!\r\n");
    return;
  }
  if (!*argument) {
    send_to_icon(PERSONA, "Download what?\r\n");
    return;
  }
  struct obj_data *soft = NULL;
  skip_spaces(&argument);
  if ((soft = get_obj_in_list_vis(ch, argument, matrix[PERSONA->in_host].file)) && GET_OBJ_VAL(soft, 7) == PERSONA->idnum) {
    if (GET_OBJ_VAL(DECKER->deck, 3) - GET_OBJ_VAL(DECKER->deck, 5) < GET_OBJ_VAL(soft, 2)) {
      send_to_icon(PERSONA, "You don't have enough storage memory to download that file.\r\n");
      return;
    } else {
      int success = system_test(PERSONA->in_host, ch, TEST_FILES, SOFT_READ, 0);
      if (GET_OBJ_VAL(soft, 5) == 4)
        success -= GET_OBJ_VAL(soft, 6);
      if (success > 0) {
        if (GET_OBJ_VAL(soft, 5) == 1 && GET_OBJ_TYPE(soft) != ITEM_PROGRAM) {
          send_to_icon(PERSONA, "A Scramble IC blocks your attempts to download the file.\r\n");
        } else if (GET_OBJ_VAL(soft, 5) > 1 && GET_OBJ_TYPE(soft) != ITEM_PROGRAM) {
          int power = GET_OBJ_VAL(soft, 6);
          for (struct obj_data *prog = DECKER->software; prog; prog = prog->next_content)
            if (GET_OBJ_VAL(prog, 0) == SOFT_ARMOUR) {
              power -= GET_OBJ_VAL(prog, 1);
              break;
            }
          success = -success_test(DECKER->bod + MIN(GET_MAX_HACKING(ch), GET_REM_HACKING(ch)), power);
          GET_REM_HACKING(ch) = MAX(0, GET_REM_HACKING(ch) - GET_MAX_HACKING(ch));
          int dam = convert_damage(stage(success, GET_OBJ_VAL(soft, 5) == 2 ? DEADLY : MODERATE));
          if (!dam)
            send_to_icon(PERSONA, "The %s explodes, but fails to cause damage to you.\r\n", GET_OBJ_VAL(soft, 5) == 2 ? "Data Bomb" : "Pavlov");
          else {
            PERSONA->condition -= dam;
            if (PERSONA->condition < 1) {
              send_to_icon(PERSONA, "The %s explodes, ripping your icon into junk logic\r\n", GET_OBJ_VAL(soft, 5) == 2 ? "Data Bomb" : "Pavlov");
              dumpshock(PERSONA);
            } else
              send_to_icon(PERSONA, "The %s explodes, damaging your icon.\r\n", GET_OBJ_VAL(soft, 5) == 2 ? "Data Bomb" : "Pavlov");
          }
          if (GET_OBJ_VAL(soft, 5) == 2) {
            GET_OBJ_VAL(soft, 5) = 0;
            if (PERSONA) {
              DECKER->tally += GET_OBJ_VAL(soft, 6);
              check_trigger(PERSONA->in_host, ch);
            }
          }
        } else {
          GET_OBJ_VAL(soft, 9) = GET_OBJ_VAL(soft, 2);
          GET_OBJ_VAL(soft, 8) = PERSONA->idnum;
          send_to_icon(PERSONA, "You begin to download %s.\r\n", GET_OBJ_NAME(soft));
        }
      } else
        send_to_icon(PERSONA, "The file fails to download.\r\n");
        return;
    }
  }
  send_to_icon(PERSONA, "You can't seem to locate that file on this host.\r\n");
}

ACMD(do_run)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  struct obj_data *soft;
  struct matrix_icon *temp;
  two_arguments(argument, buf, arg);
  for (soft = DECKER->software; soft; soft = soft->next_content)
    if (isname(buf, soft->text.keywords) || isname(buf, soft->restring))
      break;
  if (soft)
    switch (GET_OBJ_VAL(soft, 0)) {
    case SOFT_ATTACK:
      struct matrix_icon *icon;
      for (icon = matrix[PERSONA->in_host].icons; icon; icon = icon->next_in_host)
        if ((isname(arg, icon->name) || isname(arg, icon->look_desc)) && has_spotted(PERSONA, icon))
          break;
      if (!icon)
        send_to_icon(PERSONA, "You can't see that icon in this host.\r\n");
      else {
        send_to_icon(PERSONA, "You start running %s against %s.\r\n", GET_OBJ_NAME(soft), icon->name);
        if (!PERSONA->fighting) {
          PERSONA->next_fighting = matrix[icon->in_host].fighting;
          matrix[icon->in_host].fighting = PERSONA;
          roll_matrix_init(PERSONA);
        }
        PERSONA->fighting = icon;
        if (!icon->fighting && IS_PROACTIVE(icon)) {
          icon->fighting = PERSONA;
          icon->next_fighting = matrix[icon->in_host].fighting;
          matrix[icon->in_host].fighting = icon;
          roll_matrix_init(icon);
        }
        order_list(matrix[icon->in_host].fighting);
        send_to_icon(icon, "%s begins to run an attack program aimed at you.\r\n", PERSONA->name);
        bool tarred = FALSE;
        for (struct matrix_icon *ic = matrix[PERSONA->in_host].icons; ic && !tarred; ic = temp) {
          temp = ic->next_in_host;
          if ((ic->ic.type == 4 || ic->ic.type == 10) && ic->ic.subtype == 1)
            tarred = tarbaby(soft, ch, ic);
        }
      }
      break;
    case SOFT_MEDIC:
      if (PERSONA->condition == 10) {
        send_to_icon(PERSONA, "You're already at optimal condition.\r\n");
      } else if (GET_OBJ_VAL(soft, 1) <= 0) {
        send_to_icon(PERSONA, "That program is no longer usable!\r\n");
      } else {
        bool tarred = FALSE;
        for (struct matrix_icon *ic = matrix[PERSONA->in_host].icons; ic && !tarred; ic = temp) {
          temp = ic->next_in_host;
          if ((ic->ic.type == 4 || ic->ic.type == 10) && ic->ic.subtype == 1)
            tarred = tarbaby(soft, ch, ic);
        }
        if (tarred)
          return;
        send_to_icon(PERSONA, "You run a medic program.\r\n");
        int targ = 0;
        if (PERSONA->condition < 2)
          targ = 6;
        else if (PERSONA->condition < 7)
          targ = 5;
        else if (PERSONA->condition < 9)
          targ = 4;
        int success = success_test(GET_OBJ_VAL(soft, 1), targ);
        if (success < 1)
          send_to_icon(PERSONA, "It fails to execute.\r\n");
        else {
          PERSONA->condition = MIN(10, success + PERSONA->condition);
          send_to_icon(PERSONA, "It repairs your icon.\r\n");
        }
        PERSONA->initiative -= 10;
        GET_OBJ_VAL(soft, 1)--;
      }
      return;
    default:
      send_to_icon(PERSONA, "You don't need to manually run that program.\r\n");
      break;
    }
  else
    send_to_icon(PERSONA, "You don't seem to have that program loaded.\r\n");
}

ACMD(do_decrypt)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  skip_spaces(&argument);
  if (!*argument) {
    send_to_char(ch, "%s what?\r\n", subcmd ? "Disarm" : "Decrypt");
    return;
  }
  struct obj_data *obj = NULL;
  if ((obj = get_obj_in_list_vis(ch, argument, matrix[PERSONA->in_host].file)) && GET_OBJ_VAL(obj, 7) == PERSONA->idnum) {
    if (!GET_OBJ_VAL(obj, 5) || (GET_OBJ_VAL(obj, 5) == 1 && subcmd) || (GET_OBJ_VAL(obj, 5) > 1 && !subcmd) ||
        GET_OBJ_TYPE(obj) == ITEM_PROGRAM) {
      send_to_icon(PERSONA, "There is no need to %s that file.\r\n", subcmd ? "disarm" : "decrypt");
      return;
    }
    int success = system_test(PERSONA->in_host, ch, TEST_FILES, subcmd ? SOFT_DEFUSE : SOFT_DECRYPT, 0);
    if (success > 0) {
      send_to_icon(PERSONA, "You successfully %s the file.\r\n", subcmd ? "disarm" : "decrypt");
      GET_OBJ_VAL(obj, 5) = 0;
    } else if (PERSONA) {
      send_to_icon(PERSONA, "You fail to %s the IC protecting that file.\r\n", subcmd ? "disarm" : "decrypt");
      if (GET_OBJ_VAL(obj, 5) == 1)
        if (success_test(GET_OBJ_VAL(obj, 6), GET_SKILL(ch, SKILL_COMPUTER)) > 0) {
          struct obj_data *temp;
          send_to_icon(PERSONA, "The Scramble IC destroys the file!\r\n");
          REMOVE_FROM_LIST(obj, matrix[PERSONA->in_host].file, next_content);
          extract_obj(obj);
        }
    }
    return;
  }
  if (is_abbrev(argument, "slave") || is_abbrev(argument, "files") || is_abbrev(argument, "access")) {
    int mode = 0;
    if (is_abbrev(argument, "access"))
      mode = ACCESS;
    else if (is_abbrev(argument, "files"))
      mode = FILES;
    else if (is_abbrev(argument, "slave"))
      mode = SLAVE;
    int success = system_test(PERSONA->in_host, ch, mode, SOFT_DECRYPT, 0 );
    if (success > 0 && matrix[PERSONA->in_host].stats[mode][1]) {
      matrix[PERSONA->in_host].stats[mode][1] = 0;
      send_to_icon(PERSONA, "You decrypt the subsystem.\r\n");
    } else {
      send_to_icon(PERSONA, "You fail to decrypt that subsystem.\r\n");
    }
    return;
  }
  for (struct exit_data *exit = matrix[PERSONA->in_host].exit; exit; exit = exit->next)
    if (!str_cmp(argument, exit->number)) {
      int inhost = PERSONA->in_host;
      icon_from_host(PERSONA);
      icon_to_host(PERSONA, real_host(exit->host));
      int success = system_test(PERSONA->in_host, ch, TEST_ACCESS, SOFT_DECRYPT, 0);
      if (success > 0 && matrix[real_host(exit->host)].stats[ACCESS][1]) {
        send_to_icon(PERSONA, "You successfully decrypt that SAN.\r\n");
        matrix[real_host(exit->host)].stats[ACCESS][1] = 0;
      } else
        send_to_icon(PERSONA, "You fail to decrypt that SAN.\r\n");
      if (PERSONA) {
        icon_from_host(PERSONA);
        icon_to_host(PERSONA, inhost);
      }
      return;
    }
  send_to_icon(PERSONA, "You can't seem to locate that file.\r\n");
}

ACMD(do_software)
{
  struct obj_data *soft;
  if (PLR_FLAGGED(ch, PLR_MATRIX)) {
    if (!PERSONA) {
      send_to_char(ch, "You can't do that while hitching.\r\n");
      return;
    }
    if (!DECKER->deck) {
      send_to_char(ch, "You need a cyberdeck to store your software on!\r\n");
    } else {
      send_to_icon(PERSONA, "Active Memory Total:(^G%d^n) Free:(^R%d^n):\r\n", GET_OBJ_VAL(DECKER->deck, 2), DECKER->active);
      for (soft = DECKER->software; soft; soft = soft->next_content)
        send_to_icon(PERSONA, "%25s Rating: %2d\r\n", GET_OBJ_NAME(soft), GET_OBJ_VAL(soft, 1));
      send_to_icon(PERSONA, "\r\nStorage Memory Total:(^G%d^n) Free:(^R%d^n):\r\n", GET_OBJ_VAL(DECKER->deck, 3),
                   GET_OBJ_VAL(DECKER->deck, 3) - GET_OBJ_VAL(DECKER->deck, 5));
      for (soft = DECKER->deck->contains; soft; soft = soft->next_content)
        if (GET_OBJ_TYPE(soft) == ITEM_PROGRAM)
          send_to_icon(PERSONA, "%-30s^n Rating: %2d\r\n", GET_OBJ_NAME(soft), GET_OBJ_VAL(soft, 1));
        else if (GET_OBJ_TYPE(soft) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(soft, 0) == TYPE_FILE)
          send_to_icon(PERSONA, "%s^n\r\n", GET_OBJ_NAME(soft));
    }
  } else {
    struct obj_data *cyberdeck = NULL, *cyber;
    for (cyber = ch->cyberware; cyber; cyber = cyber->next_content)
      if (GET_OBJ_VAL(cyber, 0) == CYB_DATAJACK || (GET_OBJ_VAL(cyber, 0) == CYB_EYES && IS_SET(GET_OBJ_VAL(cyber, 3), EYE_DATAJACK)))
        break;
    if (!cyber) {
      send_to_char(ch, "You need a datajack to check out the contents of a deck.\r\n");
      return;
    }
    for (cyber = ch->carrying; !cyberdeck && cyber; cyber = cyber->next_content)
      if (GET_OBJ_TYPE(cyber) == ITEM_CYBERDECK || GET_OBJ_TYPE(cyber) == ITEM_CUSTOM_DECK)
        cyberdeck = cyber;
    for (int i = 0; !cyberdeck && i < NUM_WEARS; i++)
      if (GET_EQ(ch, i) && (GET_OBJ_TYPE(GET_EQ(ch,i )) == ITEM_CYBERDECK || GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_CUSTOM_DECK))
        cyberdeck = GET_EQ(ch, i);
    if (!cyberdeck) {
      send_to_char(ch, "You have no cyberdeck to check the software on!\r\n");
      return;
    } else if (GET_OBJ_VAL(cyberdeck, 0) == 0 || GET_OBJ_VAL(cyberdeck, 9)) {
      send_to_char(ch, "The deck doesn't respond.\r\n");
      return;
    }
    send_to_char(ch, "You jack into the deck and retrieve the following data:\r\n"
                 "MPCP ^g%d^n - Active Memory ^g%d^n - Storage Memory ^R%d^n/^g%d^n\r\n"
                 "Hardening ^g%d^n - IO ^g%d^n - Response Increase ^g%d^n\r\n",
                 GET_OBJ_VAL(cyberdeck, 0), GET_OBJ_VAL(cyberdeck, 2),
                 GET_OBJ_VAL(cyberdeck, 3) - GET_OBJ_VAL(cyberdeck, 5), GET_OBJ_VAL(cyberdeck, 3),
                 GET_OBJ_VAL(cyberdeck, 1), GET_OBJ_VAL(cyberdeck, 4), GET_OBJ_VAL(cyberdeck, 6));
    int bod = 0, sensor = 0, masking = 0, evasion = 0;
    for (struct obj_data *soft = cyberdeck->contains; soft; soft = soft->next_content)
      if (GET_OBJ_TYPE(soft) == ITEM_PROGRAM && GET_OBJ_VNUM(soft) != 108) {
        switch (GET_OBJ_VAL(soft, 0)) {
        case SOFT_BOD:
          bod = GET_OBJ_VAL(soft, 1);
          break;
        case SOFT_SENSOR:
          sensor = GET_OBJ_VAL(soft, 1);
          break;
        case SOFT_MASKING:
          masking = GET_OBJ_VAL(soft, 1);
          break;
        case SOFT_EVASION:
          evasion = GET_OBJ_VAL(soft, 1);
          break;
        }
      } else if (GET_OBJ_TYPE(soft) == ITEM_PART)
        switch (GET_OBJ_VAL(soft, 0)) {
        case PART_BOD:
          bod = GET_OBJ_VAL(soft, 1);
          break;
        case PART_SENSOR:
          sensor = GET_OBJ_VAL(soft, 1);
          break;
        case PART_MASKING:
          masking = GET_OBJ_VAL(soft, 1);
          break;
        case PART_EVASION:
          evasion = GET_OBJ_VAL(soft, 1);
          break;
        }
    send_to_char(ch, "Persona Programs:\r\n"
                 "Bod:     ^g%2d^n   Masking: ^g%2d^n\r\n"
                 "Sensors: ^g%2d^n   Evasion: ^g%2d^n\r\n", bod, masking, sensor, evasion);
    strcpy(buf, "Other Software:\r\n");
    if (GET_OBJ_TYPE(cyberdeck) == ITEM_CUSTOM_DECK)
      strcpy(buf2, "Custom Components:\r\n");
    for (struct obj_data *soft = cyberdeck->contains; soft; soft = soft->next_content)
      if (GET_OBJ_TYPE(soft) == ITEM_PROGRAM) {
        sprintf(buf + strlen(buf), "%-30s^n Rating: %2d %c\r\n", GET_OBJ_NAME(soft),
                GET_OBJ_VAL(soft, 1), GET_OBJ_VAL(soft, 4) ? '*' : ' ');
      } else if (GET_OBJ_TYPE(soft) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(soft, 0) == TYPE_FILE)
        sprintf(buf + strlen(buf), "%s^n\r\n", GET_OBJ_NAME(soft));
      else if (GET_OBJ_TYPE(soft) == ITEM_PART)
        sprintf(buf2 + strlen(buf2), "%-30s^n Type: %-15s\r\n", GET_OBJ_NAME(soft), parts[GET_OBJ_VAL(soft, 0)].name);
    send_to_char(buf, ch);
    if (GET_OBJ_TYPE(cyberdeck) == ITEM_CUSTOM_DECK)
      send_to_char(buf2, ch);
  }
}

void process_upload(struct matrix_icon *persona)
{
  if (persona->decker->deck)
    for (struct obj_data *soft = persona->decker->deck->contains; soft; soft = soft->next_content)
      if (GET_OBJ_TYPE(soft) != ITEM_PART && GET_OBJ_VAL(soft, 9) > 0)
      {
        GET_OBJ_VAL(soft, 9) -= persona->decker->io;
        if (GET_OBJ_VAL(soft, 9) <= 0) {
          send_to_icon(persona, "%s has finished uploading to %s.\r\n", CAP(GET_OBJ_NAME(soft)),
                       GET_OBJ_VAL(soft, 8) ? "the host" : "active memory");
          if (GET_OBJ_VAL(soft, 8) == 1) {
            obj_from_obj(soft);
            if (matrix[persona->in_host].file)
              soft->next_content = matrix[persona->in_host].file;
            matrix[persona->in_host].file = soft;
            GET_OBJ_VAL(persona->decker->deck, 5) -= GET_OBJ_VAL(soft, 2);
            GET_OBJ_VAL(soft, 8) = 0;
            if (GET_QUEST(persona->decker->ch)) {
              for (int i = 0; i < quest_table[GET_QUEST(persona->decker->ch)].num_objs; i++)
                if (quest_table[GET_QUEST(persona->decker->ch)].obj[i].objective == QOO_UPLOAD &&
                  GET_OBJ_VNUM(soft) == quest_table[GET_QUEST(persona->decker->ch)].obj[i].vnum &&
                  matrix[persona->in_host].vnum == quest_table[GET_QUEST(persona->decker->ch)].obj[i].o_data)
                  {
                    persona->decker->ch->player_specials->obj_complete[i] = 1;
                    break;
                  }
            }
          } else {
            struct obj_data *active = read_object(GET_OBJ_RNUM(soft), REAL);
            if (soft->restring)
              STR_DUP(active->restring, soft->restring);
            for (int x = 0; x < 10; x++)
              GET_OBJ_VAL(active, x) = GET_OBJ_VAL(soft, x);
            active->next_content = persona->decker->software;
            persona->decker->software = active;
          }
          GET_OBJ_VAL(soft, 9) = 0;
        }
        break;
      }
}

struct matrix_icon *find_icon_by_id(vnum_t idnum)
{
  struct matrix_icon *list = icon_list;
  for (;list; list = list->next)
    if (list->idnum == idnum)
      return list;
  return NULL;
}
#define host matrix[rnum]
void matrix_update()
{
  rnum_t rnum = 1;
  struct matrix_icon *icon;
  extern struct time_info_data time_info;
  for (;rnum <= top_of_matrix; rnum++) {
    bool decker = FALSE;
    struct matrix_icon *nexticon;
    if (host.reset) {
      if (!--host.reset)
        host.alert = 0;
      else
        continue;
    }
    if (time_info.hours == 2 && host.payreset) 
      host.payreset = FALSE;
    if (time_info.hours == 0) {
      if (!host.type && !host.found && !host.payreset) {
        switch (host.colour) {
        case 0:
          host.found = number(1, 6) - 1;
          break;
        case 1:
          host.found = number(1, 6) + number(1, 6) - 2;
          break;
        case 2:
          host.found = number(1, 6) + number(1, 6);
          break;
        case 3:
        case 4:
          host.found = number(1, 6) + number(1, 6) + 2;
          break;
        }
        host.payreset = TRUE;
      } else
        for (int x = 0; x < 5; x++)
          if (host.stats[x][2] && !host.stats[x][1])
            host.stats[x][1]++;
    }
    if (host.shutdown) {
      if (success_test(host.security, host.shutdown_mpcp) > 0) {
        send_to_host(host.rnum, host.shutdown_stop, NULL, FALSE);
        host.shutdown = 0;
        host.shutdown_mpcp = 0;
        host.shutdown_success = 0;
      } else if (!--host.shutdown) {
        struct obj_data *temp, *nextfile = NULL;
        host.shutdown_mpcp = 0;
        host.shutdown_success = 0;
        host.alert = 3;
        while (host.icons)
          dumpshock(host.icons);
        if (host.file)
          for (struct obj_data *obj = host.file; nextfile; obj = nextfile) {
            nextfile = obj->next_content;
            REMOVE_FROM_LIST(obj, host.file, next_content);
            extract_obj(obj);
          }
        host.reset = srdice() + srdice();
        continue;
      }
    }
    for (icon = host.icons; icon; icon = nexticon) {
      nexticon = icon->next_in_host;
      if (!icon->number) {
        process_upload(icon);
        decker = TRUE;
        if (!host.pass)
          GET_REM_HACKING(icon->decker->ch) = GET_HACKING(icon->decker->ch);
      } else {
        struct matrix_icon *icon2;
        for (icon2 = host.icons; icon2; icon2 = icon2->next_in_host)
          if (icon->ic.target == icon2->idnum)
            break;
        if (!icon2)
          extract_icon(icon);

      }
    }
    if (decker) {
      for (icon = host.icons; icon; icon = icon->next_in_host)
        if (icon->number && IS_PROACTIVE(icon) && !icon->fighting)
          for (struct matrix_icon *icon2 = host.icons; icon2; icon2 = icon2->next_in_host)
            if (icon->ic.target == icon2->idnum && icon2->decker) {
              if (icon->ic.type == 6 && icon->ic.subtype > 0) {
                if (!--icon->ic.subtype) {
                  icon2->decker->located = TRUE;
                  send_to_icon(icon2, "Alarms start to ring in your head as %s finds your location.\r\n", icon->name);
                  sprintf(buf, "%s located by Trace IC in host %ld (%s).", GET_CHAR_NAME(icon2->decker->ch), matrix[icon->in_host].vnum, matrix[icon->in_host].name);
                  mudlog(buf, icon2->decker->ch, LOG_GRIDLOG, TRUE);
                }
              } else if (icon->ic.type != 6 || (icon->ic.type == 6 && !icon2->decker->located)) {
                icon->fighting = icon2;
                icon->next_fighting = host.fighting;
                host.fighting = icon;
                roll_matrix_init(icon);
              }
              break;
            }
      struct obj_data *next;
      for (struct obj_data *file = host.file; file; file = next) {
        next = file->next_content;
        if (GET_OBJ_VAL(file, 9)) {
          struct matrix_icon *persona = find_icon_by_id(GET_OBJ_VAL(file, 8));
          if (!persona || persona->in_host != rnum)
            GET_OBJ_VAL(file, 8) = 0;
          else {
            GET_OBJ_VAL(file, 9) -= persona->decker->io;
            if (GET_OBJ_VAL(file, 9) <= 0 && GET_OBJ_VAL(persona->decker->deck, 3) - GET_OBJ_VAL(persona->decker->deck, 5) > GET_OBJ_VAL(file, 2)) {
              struct obj_data *temp;
              REMOVE_FROM_LIST(file, host.file, next_content);
              obj_to_obj(file, persona->decker->deck);
              send_to_icon(persona, "%s has finished downloading to your deck.\r\n", CAP(GET_OBJ_NAME(file)));
              GET_OBJ_VAL(persona->decker->deck, 5) += GET_OBJ_VAL(file, 2);
              GET_OBJ_VAL(file, 7) = 0;
            }
          }
        }
      }
    }
  }
}

void matrix_violence()
{
  struct matrix_icon *temp, *icon;
  rnum_t rnum = 1;
  for (;rnum <= top_of_matrix; rnum++)
    if (host.fighting) {
      host.pass++;
      order_list(host.fighting);
      if (host.fighting->initiative <= 0) {
        host.pass = 0;
        for (icon = host.fighting; icon; icon = icon->next_fighting)
          roll_matrix_init(icon);
        order_list(host.fighting);
      }
      for (icon = host.fighting; icon; icon = icon->next_fighting) {
        if (icon->initiative > 0) {
          icon->initiative -= 10;
          if (icon->fighting) {
            if (icon->decker) {
              if (icon->evasion && !host.pass)
                icon->evasion--;
              if (!icon->fighting->evasion)
                matrix_fight(icon, icon->fighting);
            } else {
              switch(icon->evasion ? number(0, 5) : number(0, 10)) {
              case 1:
                parry_attack(icon);
                break;
              case 2:
                position_attack(icon);
                break;
              case 3:
              case 4:
                if (!icon->evasion)
                  evade_detection(icon);
                break;
              }
              if (icon->ic.targ_evasion) {
                if (!host.pass)
                  icon->ic.targ_evasion--;
              } else if (!icon->evasion)
                matrix_fight(icon, icon->fighting);
              else if (!host.pass)
                icon->evasion--;
            }
          } else
            REMOVE_FROM_LIST(icon, matrix[icon->in_host].fighting, next_fighting);
        }
      }
    }
}

ACMD(do_crash)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  if (matrix[PERSONA->in_host].shutdown) {
    send_to_icon(PERSONA, "Someone has already initiated a shutdown.\r\n");
    return;
  }
  int success = system_test(PERSONA->in_host, ch, TEST_CONTROL, SOFT_CRASH, 0);
  if (success > 0) {
    matrix[PERSONA->in_host].shutdown_success = success;
    matrix[PERSONA->in_host].shutdown_mpcp = DECKER->mpcp;
    int x = matrix[PERSONA->in_host].security / 2;
    while (x > 0) {
      x--;
      matrix[PERSONA->in_host].shutdown += number(1, 6);
    }
    send_to_host(PERSONA->in_host, matrix[PERSONA->in_host].shutdown_start, NULL, FALSE);
    send_to_host(PERSONA->in_host, "\r\n", NULL, FALSE);
  } else
    send_to_icon(PERSONA, "Your program fails to run.\r\n");
}
ACMD(do_matrix_scan)
{
  skip_spaces(&argument);
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  if (!*argument) {
    send_to_icon(PERSONA, "What do you wish to scan?\r\n");
    return;
  }
  for (struct matrix_icon *ic = matrix[PERSONA->in_host].icons; ic; ic = ic->next_in_host)
    if ((isname(argument, ic->look_desc) || isname(argument, ic->name)) && has_spotted(PERSONA, ic) &&
        ic->decker) {
      struct obj_data *obj;
      int target = ic->decker->masking;
      for (obj = ic->decker->software; obj; obj = obj->next_content)
        if (GET_OBJ_VAL(obj, 0) == SOFT_SLEAZE) {
          target += GET_OBJ_VAL(obj, 1);
          break;
        }
      for (obj = DECKER->software; obj; obj = obj->next_content)
        if (GET_OBJ_VAL(obj, 0) == SOFT_SCANNER) {
          target -= GET_OBJ_VAL(obj, 1);
          break;
        }
      int skill = get_skill(ch, SKILL_COMPUTER, target) + MIN(GET_MAX_HACKING(ch), GET_REM_HACKING(ch));
      int success = success_test(skill, target);
      GET_REM_HACKING(ch) = MAX(0, GET_REM_HACKING(ch) - GET_MAX_HACKING(ch));
      if (success > 7) {
        send_to_icon(PERSONA, "MPCP-%d %d/%d/%d/%d Response: %d Condition: %d "
                     "Privileges: None MXP: %ld\r\n", ic->decker->mpcp,
                     ic->decker->bod, ic->decker->evasion, ic->decker->masking,
                     ic->decker->sensor, ic->decker->response, ic->condition, ic->decker->mxp);
      } else if (success > 0) {
        int found[] = { 0, 0, 0, 0, 0, 0, 0, 0};
        for (int current;success; success--) {
          current = number(0, 7);
          int start = current;
          while (found[current]) {
            current++;
            if (current > 7)
              current = 0;
            if (current == start)
              break;
          }
          found[current] = 1;
        }
        sprintf(buf, "MPCP-");
        if (found[0])
          sprintf(buf + strlen(buf), "%d", ic->decker->mpcp);
        else
          sprintf(buf, "0");
        if (found[1])
          sprintf(buf + strlen(buf), " %d/", ic->decker->bod);
        else
          strcat(buf, " 0/");
        if (found[2])
          sprintf(buf + strlen(buf), "%d/", ic->decker->evasion);
        else
          strcat(buf, "0/");
        if (found[3])
          sprintf(buf + strlen(buf), "%d/", ic->decker->masking);
        else
          strcat(buf, "0/");
        if (found[4])
          sprintf(buf + strlen(buf), "%d", ic->decker->sensor);
        else
          strcat(buf, "0");
        strcat(buf, " Response: ");
        if (found[5])
          sprintf(buf + strlen(buf), "%d", ic->decker->response);
        else
          strcat(buf, "0");
        strcat(buf, " Condition: ");
        if (found[6])
          sprintf(buf + strlen(buf), "%d", ic->condition);
        else
          strcat(buf, "0");
        strcat(buf, " Privileges: None MXP: \r\n");
        if (found[7])
          sprintf(buf + strlen(buf), "%ld", ic->decker->mxp);
        else
          strcat(buf, "Not Found");
        strcat(buf, "\r\n");
        send_to_icon(PERSONA, buf);

      } else
        send_to_icon(PERSONA, "You don't find out anything new.\r\n");
      return;
    }
  send_to_icon(PERSONA, NOPERSON);
}
ACMD(do_abort)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  if (!matrix[PERSONA->in_host].shutdown) {
    send_to_icon(PERSONA, "There is no shutdown to abort.\r\n");
    return;
  }
  int success = system_test(PERSONA->in_host, ch, TEST_CONTROL, SOFT_SWERVE, 0);
  success /= 2;
  if (success > matrix[PERSONA->in_host].shutdown_success) {
    send_to_host(PERSONA->in_host, matrix[PERSONA->in_host].shutdown_stop, NULL, FALSE);
    matrix[PERSONA->in_host].shutdown = 0;
    matrix[PERSONA->in_host].shutdown_success = 0;
    matrix[PERSONA->in_host].shutdown_mpcp = 0;
  } else if (success > 0) {
    send_to_icon(PERSONA, "You manage to prolong the shutdown.\r\n");
    matrix[PERSONA->in_host].shutdown += success;
  } else
    send_to_icon(PERSONA, "You fail to prolong the shutdown.\r\n");
}

ACMD(do_talk)
{
  skip_spaces(&argument);
  if (!PERSONA)
    send_to_char(ch, "You can't do that while hitching.\r\n");
  else if (!DECKER->phone->dest)
    send_to_icon(PERSONA, "You don't have a call connected.\r\n");
  else if (!DECKER->phone->dest->connected)
    send_to_icon(PERSONA, "They haven't answered yet.\r\n");
  else if (!*argument)
    send_to_icon(PERSONA, "Say what?\r\n");
  else {
    struct char_data *tch = NULL;
    sprintf(buf, "^Y$v on the other end of the line says, \"%s\"", argument);
    sprintf(buf2, "^YYou say, \"%s\"\r\n", argument);
    send_to_char(buf2, ch);
    if (DECKER->phone->dest->persona && DECKER->phone->dest->persona->decker) {
      act(buf, FALSE, ch, 0, tch, TO_DECK);
    } else {
      tch = DECKER->phone->dest->phone->carried_by;
      if (!tch)
        tch = DECKER->phone->dest->phone->worn_by;
      if (!tch && DECKER->phone->dest->phone->in_obj)
        tch = DECKER->phone->dest->phone->in_obj->carried_by;
      if (!tch && DECKER->phone->dest->phone->in_obj)
        tch = DECKER->phone->dest->phone->in_obj->worn_by;
    }
    if (tch)
      act(buf, FALSE, ch, 0, tch, TO_VICT);
  }
}

ACMD(do_comcall)
{
  struct char_data *tch;
  skip_spaces(&argument);
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  if (subcmd == SCMD_ANSWER) {
    if (DECKER->phone->connected) {
      send_to_icon(PERSONA, "But you already have a call connected!\r\n");
      return;
    }
    if (!DECKER->phone->dest) {
      send_to_icon(PERSONA, "You have no incoming connections.\r\n");
      return;
    }
    if (DECKER->phone->dest->persona)
      send_to_icon(DECKER->phone->dest->persona, "The call is answered.\r\n");
    else {
      tch = DECKER->phone->dest->phone->carried_by;
      if (!tch)
        tch = DECKER->phone->dest->phone->worn_by;
      if (!tch && DECKER->phone->dest->phone->in_obj)
        tch = DECKER->phone->dest->phone->in_obj->carried_by;
      if (!tch && DECKER->phone->dest->phone->in_obj)
        tch = DECKER->phone->dest->phone->in_obj->worn_by;
      if (tch)
        send_to_char("The phone is answered.\r\n", tch);
    }
    send_to_icon(PERSONA, "You establish a connection.\r\n");
    DECKER->phone->connected = TRUE;
    DECKER->phone->dest->connected = TRUE;

  } else if (subcmd == SCMD_HANGUP) {
    if (!DECKER->phone->dest) {
      send_to_icon(PERSONA, "But you don't have a direct connection established.\r\n");
      return;
    }
    if (DECKER->phone->dest->persona)
      send_to_icon(DECKER->phone->dest->persona, "The flashing phone icon fades from view.\r\n");
    else {
      tch = DECKER->phone->dest->phone->carried_by;
      if (!tch)
        tch = DECKER->phone->dest->phone->worn_by;
      if (!tch && DECKER->phone->dest->phone->in_obj)
        tch = DECKER->phone->dest->phone->in_obj->carried_by;
      if (!tch && DECKER->phone->phone->in_obj)
        tch = DECKER->phone->dest->phone->in_obj->worn_by;
      if (tch) {
        if (DECKER->phone->dest->connected)
          send_to_char("The phone is hung up from the other side.\r\n", tch);
        else
          send_to_char("Your phone stops ringing.\r\n", tch);
      }
    }
    DECKER->phone->connected = FALSE;
    DECKER->phone->dest->dest = NULL;
    DECKER->phone->dest->connected = FALSE;
    DECKER->phone->dest = NULL;
    send_to_icon(PERSONA, "You cut the connection.\r\n");
  } else {
    int ring;
    any_one_arg(argument, arg);
    if (!*arg)
      send_to_icon(PERSONA, "Ring what number?");
    else if (!(ring = atoi(arg)))
      send_to_icon(PERSONA, "That's not a valid number.\r\n");
    else if (matrix[PERSONA->in_host].type != HOST_RTG)
      send_to_icon(PERSONA, "You can only perform that action on an RTG.\r\n");
    else {
      struct phone_data *k;
      for (k = phone_list; k; k = k->next)
        if (k->number == ring && k != DECKER->phone && k->rtg == matrix[PERSONA->in_host].vnum)
          break;
      if (!k) {
        send_to_icon(PERSONA, "You can't seem to find that commcode.\r\n");
        return;
      }
      int success = system_test(PERSONA->in_host, ch, TEST_FILES, SOFT_COMMLINK, 0);
      if (success > 0) {
        if (k->dest) {
          send_to_icon(PERSONA, "That line is already busy.\r\n");
          return;
        }
        DECKER->phone->dest = k;
        DECKER->phone->connected = TRUE;
        k->dest = DECKER->phone;

        if (k->persona)
          send_to_icon(k->persona, "A small telephone symbol blinks in the top left of your view.\r\n");
        else {
          tch = k->phone->carried_by;
          if (!tch)
            tch = k->phone->worn_by;
          if (!tch && k->phone->in_obj)
            tch = k->phone->in_obj->carried_by;
          if (!tch && k->phone->in_obj)
            tch = k->phone->in_obj->worn_by;
          if (tch) {
            if (GET_POS(tch) == POS_SLEEPING) {
              if (success_test(GET_WIL(tch), 4)) {
                GET_POS(tch) = POS_RESTING;
                send_to_char("You are woken by your phone ringing.\r\n", tch);
              } else if (!GET_OBJ_VAL(k->phone, 3)) {
                act("$n's phone rings.", FALSE, tch, 0, 0, TO_ROOM);
                return;
              } else
                return;
            }
            if (!GET_OBJ_VAL(k->phone, 3)) {
              send_to_char("Your phone rings.\r\n", tch);
              act("$n's phone rings.", FALSE, tch, NULL, NULL, TO_ROOM);
            } else {
              if (GET_OBJ_TYPE(k->phone) == ITEM_CYBERWARE || success_test(GET_INT(tch), 4))
                send_to_char("You feel your phone ring.\r\n", tch);
            }
          } else {
            sprintf(buf, "%s rings.", GET_OBJ_NAME(k->phone));
            send_to_room(buf, k->phone->in_room);
          }
        }
        send_to_icon(PERSONA, "It begins to ring.\r\n");
      } else
        send_to_icon(PERSONA, "You can't seem to connect.\r\n");
    }
  }
}
ACMD(do_tap)
{}


ACMD(do_restrict)
{
  struct matrix_icon *targ;
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  two_arguments(argument, buf, arg);
  if (!*buf || !*arg) {
    send_to_icon(PERSONA, "You need to specify target and type of restriction.\r\n");
    return;
  }
  int success, detect = 0;
  for (targ = matrix[PERSONA->in_host].icons; targ; targ = targ->next_in_host)
    if (targ->decker && isname(arg, targ->name) && has_spotted(PERSONA, targ))
      break;
  if (!targ) {
    send_to_icon(PERSONA, "You can't find that icon here.\r\n");
    return;
  }
  for (struct obj_data *soft = targ->decker->software; soft; soft = soft->next_content)
    if (GET_OBJ_VAL(soft, 0) == SOFT_SLEAZE)
      detect = GET_OBJ_VAL(soft, 1);
  detect += targ->decker->masking;
  detect = detect / 2;

  if (is_abbrev(buf, "detection")) {
    success = system_test(PERSONA->in_host, ch, TEST_CONTROL, SOFT_VALIDATE, detect);
    targ->decker->res_det += success;
    send_to_icon(PERSONA, "You successfully restrict their detection factor.\r\n");
  } else if (is_abbrev(buf, "tests")) {
    success = system_test(PERSONA->in_host, ch, TEST_CONTROL, SOFT_VALIDATE, detect);
    targ->decker->res_test += success;
    send_to_icon(PERSONA, "You successfully restrict their system tests.\r\n");
  } else
    send_to_icon(PERSONA, "You must specify wether you want to restrict their detection factor or systems test.");
}

ACMD(do_trace)
{
  long addr;
  if (!PERSONA)
    send_to_char(ch, "You can't do that while hitching.\r\n");
  else if (!*argument)
    send_to_icon(PERSONA, "What MXP address do you wish to trace?\r\n");
  else if (!(addr = atoi(argument)))
    send_to_icon(PERSONA, "Invalid MXP address.\r\n");
  else if (matrix[PERSONA->in_host].type != HOST_LTG)
    send_to_icon(PERSONA, "You can only perform this action on an LTG.\r\n");
  else {
    int success = system_test(PERSONA->in_host, ch, TEST_INDEX, SOFT_BROWSE, 0);
    if (success > 0)
      for (struct matrix_icon *icon = icon_list; icon; icon = icon->next)
        if (icon->decker && icon->decker->mxp == addr) {
          if (matrix[PERSONA->in_host].vnum == icon->decker->phone->rtg)
            sprintf(buf, "Your search returns:\r\nOriginating Grid: %s\r\nSerial number: %ld\r\n",
                    matrix[real_host(icon->decker->phone->rtg)].name, icon->decker->phone->rtg * icon->decker->phone->number);
          else
            sprintf(buf, "Your search returns:\r\nJackpoint Location: %s\r\n", world[icon->decker->ch->in_room].address);
          send_to_icon(PERSONA, buf);
          return;
        }
    send_to_icon(PERSONA, "You fail to get any information on that request.\r\n");
  }
}

ACMD(do_default)
{
  struct obj_data *cyberdeck = NULL, *soft, *cyber;
  if (!*argument) {
    send_to_char(ch, "Load what program by default?\r\n");
    return;
  }
  skip_spaces(&argument);
  for (cyber = ch->cyberware; cyber; cyber = cyber->next_content)
    if (GET_OBJ_VAL(cyber, 0) == CYB_DATAJACK || (GET_OBJ_VAL(cyber, 0) == CYB_EYES && IS_SET(GET_OBJ_VAL(cyber, 3), EYE_DATAJACK)))
      break;
  if (!cyber) {
    send_to_char(ch, "You need a datajack to check out the contents of a deck.\r\n");
    return;
  }
  for (cyber = ch->carrying; !cyberdeck && cyber; cyber = cyber->next_content)
    if (GET_OBJ_TYPE(cyber) == ITEM_CYBERDECK || GET_OBJ_TYPE(cyber) == ITEM_CUSTOM_DECK)
      cyberdeck = cyber;
  for (int i = 0; !cyberdeck && i < NUM_WEARS; i++)
    if (GET_EQ(ch, i) && (GET_OBJ_TYPE(GET_EQ(ch,i )) == ITEM_CYBERDECK || GET_OBJ_TYPE(GET_EQ(ch,i )) == ITEM_CUSTOM_DECK))
      cyberdeck = GET_EQ(ch, i);
  if (!cyberdeck)
    send_to_char(ch, "You have no cyberdeck to check the software on!\r\n");
  else if (GET_OBJ_VAL(cyberdeck, 0) == 0)
    send_to_char(ch, "The deck doesn't respond.\r\n");
  else {
    for (soft = cyberdeck->contains; soft; soft = soft->next_content)
      if (GET_OBJ_TYPE(soft) == ITEM_PROGRAM && GET_OBJ_VAL(soft, 0) > SOFT_SENSOR && (isname(argument, soft->text.keywords)
          || isname(argument, GET_OBJ_NAME(soft))))
        break;
    if (!soft) {
      send_to_char(ch, "You don't have that program installed.\r\n");
      return;
    }
    if (GET_OBJ_VAL(soft, 4)) {
      GET_OBJ_VAL(soft, 4)--;
      send_to_char(ch, "%s will no longer load upon connection.\r\n", CAP(GET_OBJ_NAME(soft)));
    } else {
      GET_OBJ_VAL(soft, 4)++;
      send_to_char(ch, "%s will now load upon connection.\r\n", CAP(GET_OBJ_NAME(soft)));
    }

  }
}

ACMD(do_reveal)
{
  if (!PERSONA)
    send_to_char(ch, "You can't do that while hitching.\r\n");
  else {
    send_to_icon(PERSONA, "You lower your masking for a brief moment, broadcast your location to the host.\r\n");
    for (struct matrix_icon *icon = matrix[PERSONA->in_host].icons; icon; icon = icon->next_in_host)
      if (icon != PERSONA && icon->decker && !has_spotted(icon, PERSONA)) {
        make_seen(icon, PERSONA->idnum);
        send_to_icon(icon, "%s fades into the host.\r\n", PERSONA->name);
      }
  }
}

ACMD(do_create)
{
  if (FIGHTING(ch)) {
    send_to_char("You can't create things while fighting!\r\n", ch);
    return;
  }
  if (IS_NPC(ch)) {
    send_to_char("Sure...you do that.\r\n", ch);
    return;
  }
  if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch)) {
    send_to_char("Your arms are already full!\r\n", ch);
    return;
  }
  if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }
  argument = any_one_arg(argument, buf1);

  if (is_abbrev(buf1, "program")) {
    if (!GET_SKILL(ch, SKILL_COMPUTER))
      send_to_char("You must learn computer skills to create programs.\r\n", ch);
    else create_program(ch);
  } else if (is_abbrev(buf1, "part")) {
    if (!GET_SKILL(ch, SKILL_BR_COMPUTER))
      send_to_char("You must learn computer B/R skills to create parts.\r\n", ch);
    else create_part(ch);
  } else if (is_abbrev(buf1, "deck"))
    create_deck(ch);
  else if (is_abbrev(buf1, "ammo") || is_abbrev(buf1, "ammunition"))
    create_ammo(ch);
  else if (is_abbrev(buf1, "spell")) {
    struct obj_data *library = world[ch->in_room].contents;
    for (;library; library = library->next_content)
      if (GET_OBJ_TYPE(library) == ITEM_MAGIC_TOOL &&
          ((GET_TRADITION(ch) == TRAD_SHAMANIC
            && GET_OBJ_VAL(library, 0) == TYPE_LODGE && GET_OBJ_VAL(library, 3) == GET_IDNUM(ch)) ||
           (GET_TRADITION(ch) != TRAD_SHAMANIC && GET_OBJ_VAL(library, 0) == TYPE_LIBRARY_SPELL)))
        break;
    if (!library)
      send_to_char("You don't have the right tools here to create a spell.\r\n", ch);
    else if (!(GET_SKILL(ch, SKILL_SPELLDESIGN) || GET_SKILL(ch, SKILL_SORCERY)))
      send_to_char("You must learn Spell Design or Sorcery to create a spell.\r\n", ch);
    else {
      ch->desc->edit_number2 = GET_OBJ_VAL(library, 1);
      create_spell(ch);
    }
  } else {
    send_to_char("You can't create that.\r\n", ch);
    return;
  }
}

ACMD(do_matrix_max)
{
  if (!PERSONA)
    send_to_char(ch, "You can't do that while hitching.\r\n");
  else if (!*argument)
    send_to_char(ch, "Your maximum hacking pool used per action is %d.\r\n", GET_MAX_HACKING(ch));
  else if (!DECKER->asist[0])
    send_to_char("You can't use hacking pool while running a cold ASIST.\r\n", ch);
  else {
    int i = atoi(argument);
    if (i > GET_HACKING(ch) || i < 0)
      send_to_char(ch, "You must set your max hacking pool to between 0 and %d.\r\n", GET_HACKING(ch));
    else { 
      GET_MAX_HACKING(ch) = i;
      send_to_char(ch, "You will now use a maximum of %d hacking pool per action.\r\n", GET_MAX_HACKING(ch));
    }
  }

}

ACMD(do_asist)
{
  if (!PERSONA)
    send_to_char("You can't do that while hitching.\r\n", ch);
  else if (!DECKER->asist[1])
    send_to_char("You can't switch ASIST modes with a cold ASIST interface.\r\n", ch);
  else if (DECKER->asist[0]) {
    DECKER->asist[0] = 0;
    send_to_char("You switch to cold ASIST mode.\r\n", ch);
    GET_MAX_HACKING(ch) = 0;
    DECKER->response = 0;
  } else {
    send_to_char("You switch to hot ASIST mode.\r\n", ch);
    DECKER->asist[0] = 1;
    DECKER->response = GET_OBJ_VAL(DECKER->deck, 6);
    GET_MAX_HACKING(ch) = (int)(GET_HACKING(ch) / 3);
  }
}

ACMD(do_compact)
{
  skip_spaces(&argument);
  if (!*argument) {
    send_to_char(ch, "What do you wish to %scompress.\r\n", subcmd ? "de" : "");
    return;
  }
  struct obj_data *mem = NULL, *compact = NULL, *obj = NULL;
  for (struct obj_data *tmp = ch->cyberware; tmp; tmp = tmp->next_content)
    if (GET_OBJ_VAL(tmp, 0) == CYB_MEMORY)
      mem = tmp;
    else if (GET_OBJ_VAL(tmp, 0) == CYB_DATACOMPACT)
      compact = tmp;
  if (!mem || !compact) {
    send_to_char(ch, "You need headware memory and a data compactor to %scompress data.\r\n", subcmd ? "de" : "");
    return;
  }
  int i = atoi(argument);
  for (obj = mem->contains; obj; obj = obj->next_content)
    if (!--i)
      break;
  if (!obj) {
    send_to_char("You don't have that file.\r\n", ch);
    return;
  }
  if (GET_OBJ_VAL(obj, 9))
    send_to_char("You cannot compress a file that is in use.\r\n", ch);
  else if (subcmd && !GET_OBJ_VAL(obj, 8))
    send_to_char("That file isn't compressed.\r\n", ch);
  else if (!subcmd && GET_OBJ_VAL(obj, 8))
    send_to_char("That file is already compressed.\r\n", ch);
  else if (subcmd) {
    if (GET_OBJ_VAL(mem, 3) - GET_OBJ_VAL(mem, 5) - GET_OBJ_VAL(obj, 8) < 0) {
      send_to_char("You don't have enough free memory to decompress this.\r\n", ch);
      return;
    }
    GET_OBJ_VAL(mem, 5) += GET_OBJ_VAL(obj, 8);
    send_to_char(ch, "You decompress %s.\r\n", GET_OBJ_NAME(obj));
    GET_OBJ_VAL(obj, 8) = 0;
  } else {
    int size = (int)(((float)GET_OBJ_VAL(obj, 2) / 100) * (GET_OBJ_VAL(compact, 1) * 20));
    GET_OBJ_VAL(mem, 5) -= size;
    send_to_char(ch, "You compress %s, saving %d MP of space.\r\n", GET_OBJ_NAME(obj), size);
    GET_OBJ_VAL(obj, 8) = size;
  }
}

