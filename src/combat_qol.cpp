#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "handler.hpp"
#include "interpreter.hpp"
#include "db.hpp"
#include "bullet_pants.hpp"
extern int perform_move(struct char_data *ch, int dir, int extra, struct char_data *vict, struct veh_data *vict_veh);

#include "combat_qol.hpp"

#include <unordered_map>
static std::unordered_map<long, int> __auto_heal_threshold;

int qol_get_auto_heal_threshold(struct char_data *ch) {
  if (!ch) return 25;
  auto it = __auto_heal_threshold.find(GET_IDNUM(ch));
  return it == __auto_heal_threshold.end() ? 25 : it->second;
}
void qol_set_auto_heal_threshold_backend(struct char_data *ch, int pct) {
  if (!ch) return;
  if (pct < 1) pct = 1;
  if (pct > 99) pct = 99;
  __auto_heal_threshold[GET_IDNUM(ch)] = pct;
}

void do_patch(struct char_data *ch, char *argument, int cmd, int subcmd);

// externs
extern int find_weapon_range(struct char_data *ch, struct obj_data *weapon);
extern void range_combat(struct char_data *ch, char target[MAX_INPUT_LENGTH], struct obj_data *weapon, int range, int dir);
extern bool has_ammo_no_deduct(struct char_data *ch, struct obj_data *wielded);

static bool qol_is_ranged_firearm(struct obj_data *weapon) {
  if (!weapon) return FALSE;
  if (GET_OBJ_TYPE(weapon) != ITEM_WEAPON) return FALSE;
  return GET_WEAPON_MAX_AMMO(weapon) > 0;
}

bool qol_try_auto_reload(struct char_data *ch, struct obj_data *weapon) {
  if (!ch || !weapon) return FALSE;
  if (!PRF_FLAGGED(ch, PRF_AUTO_RELOAD)) return FALSE;
  if (!qol_is_ranged_firearm(weapon)) return FALSE;
  if (CHECK_WAIT(ch)) return FALSE;

  int ammotype = -1;
  if (reload_weapon_from_bulletpants(ch, weapon, ammotype)) {
    act("^GAuto:^n You reload $p.", FALSE, ch, weapon, 0, TO_CHAR);
    act("$n reloads $p.", TRUE, ch, weapon, 0, TO_ROOM);
    WAIT_STATE(ch, PULSE_VIOLENCE);
    return TRUE;
  }
  return FALSE;
}

bool qol_try_auto_swap_to_sidearm(struct char_data *ch) {
  if (!ch) return FALSE;
  if (!PRF_FLAGGED(ch, PRF_AUTO_SWAP)) return FALSE;
  if (CHECK_WAIT(ch)) return FALSE;

  struct obj_data *w = NULL;
  if (GET_EQ(ch, WEAR_WIELD) && qol_is_ranged_firearm(GET_EQ(ch, WEAR_WIELD)))
    w = GET_EQ(ch, WEAR_WIELD);
  else if (GET_EQ(ch, WEAR_HOLD) && qol_is_ranged_firearm(GET_EQ(ch, WEAR_HOLD)))
    w = GET_EQ(ch, WEAR_HOLD);

  if (w && has_ammo_no_deduct(ch, w))
    return FALSE;

  for (struct obj_data *obj = ch->carrying; obj; obj = obj->next_content) {
    if (!qol_is_ranged_firearm(obj)) continue;
    if (has_ammo_no_deduct(ch, obj)) {
      char namebuf[256]; namebuf[0] = '\0';
      snprintf(namebuf, sizeof(namebuf), "%s", GET_OBJ_NAME(obj));
      ACMD_DECLARE(do_wield);
      do_wield(ch, namebuf, 0, 0);
      act("^GAuto:^n You draw $p.", FALSE, ch, obj, 0, TO_CHAR);
      act("$n draws $p.", TRUE, ch, obj, 0, TO_ROOM);
      WAIT_STATE(ch, PULSE_VIOLENCE / 2);
      return TRUE;
    }
  }
  return FALSE;
}

void qol_tick_repeat_shoot(struct char_data *ch) {
  if (!ch) return;
  if (!PRF_FLAGGED(ch, PRF_REPEAT_SHOOT)) return;
  if (CHECK_WAIT(ch)) return;
  if (GET_POS(ch) <= POS_STUNNED) return;
  if (!ch->in_room) return;

  struct obj_data *weapon = NULL;
  if (GET_EQ(ch, WEAR_WIELD) && qol_is_ranged_firearm(GET_EQ(ch, WEAR_WIELD)))
    weapon = GET_EQ(ch, WEAR_WIELD);
  else if (GET_EQ(ch, WEAR_HOLD) && qol_is_ranged_firearm(GET_EQ(ch, WEAR_HOLD)))
    weapon = GET_EQ(ch, WEAR_HOLD);

  if (!weapon) return;
  if (!ch->char_specials.shooting_dir) return;

  if (!has_ammo_no_deduct(ch, weapon)) {
    if (!qol_try_auto_reload(ch, weapon)) {
      if (!qol_try_auto_swap_to_sidearm(ch)) {
        ch->char_specials.shooting_dir = 0;
        return;
      } else {
        if (GET_EQ(ch, WEAR_WIELD) && qol_is_ranged_firearm(GET_EQ(ch, WEAR_WIELD)))
          weapon = GET_EQ(ch, WEAR_WIELD);
        else if (GET_EQ(ch, WEAR_HOLD) && qol_is_ranged_firearm(GET_EQ(ch, WEAR_HOLD)))
          weapon = GET_EQ(ch, WEAR_HOLD);
        else
          return;
      }
    }
  }

  int range = find_weapon_range(ch, weapon);
  range_combat(ch, (char *)"", weapon, range, ch->char_specials.shooting_dir);
  WAIT_STATE(ch, PULSE_VIOLENCE);
}

// ===== Auto-heal =====
void qol_tick_auto_heal(struct char_data *ch) {
  if (!ch) return;
  if (!PRF_FLAGGED(ch, PRF_AUTO_HEAL)) return;
  if (CHECK_WAIT(ch)) return;
  if (GET_POS(ch) <= POS_STUNNED) return;

  int thresh = qol_get_auto_heal_threshold(ch);

  int maxp = GET_MAX_PHYSICAL(ch);
  int curp = GET_PHYSICAL(ch);
  if (maxp <= 0) return;
  int pct = (curp * 100) / maxp;

  if (pct <= thresh) {
    struct obj_data *best = NULL;
    for (struct obj_data *o = ch->carrying; o; o = o->next_content) {
      if (GET_OBJ_TYPE(o) != ITEM_PATCH) continue;
      int ptype = GET_PATCH_TYPE(o);
      if (ptype == PATCH_TRAUMA || ptype == PATCH_STIM) { best = o; break; }
    }
    if (!best) return;

    char argbuf[MAX_INPUT_LENGTH];
    snprintf(argbuf, sizeof(argbuf), "%s me", fname(const_cast<char*>(GET_OBJ_NAME(best))));

    do_patch(ch, argbuf, 0, 0);
    WAIT_STATE(ch, PULSE_VIOLENCE);
  }
}

// Auto-flee at 15%% HP
bool qol_tick_auto_flee(struct char_data *ch) {
  if (!ch) return FALSE;
  if (!PRF_FLAGGED(ch, PRF_AUTO_FLEE)) return FALSE;
  if (CHECK_WAIT(ch)) return FALSE;
  if (GET_POS(ch) <= POS_STUNNED) return FALSE;
  if (!FIGHTING(ch)) return FALSE;

  int maxp = GET_MAX_PHYSICAL(ch);
  int curp = GET_PHYSICAL(ch);
  if (maxp <= 0) return FALSE;
  int pct = (curp * 100) / maxp;
  if (pct > 15) return FALSE;

  ACMD_DECLARE(do_flee);
  do_flee(ch, (char *)"", 0, 0);
  WAIT_STATE(ch, PULSE_VIOLENCE);
  return TRUE;
}

void qol_tick_auto_advance(struct char_data *ch) {
  if (!ch) return;
  if (!PRF_FLAGGED(ch, PRF_AUTO_ADVANCE)) return;
  if (CHECK_WAIT(ch)) return;
  if (GET_POS(ch) <= POS_STUNNED) return;
  if (!ch->in_room) return;
  if (FIGHTING(ch)) return;
  int dir = ch->char_specials.shooting_dir;
  if (dir <= 0 || dir >= NUM_OF_DIRS) return;
  perform_move(ch, dir, 0, NULL, NULL);
}
