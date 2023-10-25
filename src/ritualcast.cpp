#include "awake.hpp"
#include "structs.hpp"
#include "constants.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "newmagic.hpp"
#include "newhouse.hpp"

extern void raw_cast_detection_spell(struct char_data *ch, struct char_data *vict, int spell, int force, struct char_data *mob, int successes);
extern void raw_cast_health_spell(struct char_data *ch, struct char_data *vict, int spell, int sub, int force, struct char_data *mob, int ritual_successes);
extern void raw_cast_illusion_spell(struct char_data *ch, struct char_data *vict, int spell, int force, struct char_data *mob, int ritual_successes);
extern void raw_cast_manipulation_spell(struct char_data *ch, struct char_data *vict, int spell, int force, struct char_data *mob, int ritual_successes, int basedamage);

bool spell_is_valid_ritual_spell(int spell) {
  switch (spell) {
    case SPELL_COMBATSENSE:
    case SPELL_NIGHTVISION:
    case SPELL_INFRAVISION:
    case SPELL_DETOX:
    case SPELL_STABILIZE:
    case SPELL_RESISTPAIN:
    case SPELL_HEALTHYGLOW:
    case SPELL_TREAT:
    case SPELL_HEAL:
    case SPELL_INCREF1:
    case SPELL_INCREF2:
    case SPELL_INCREF3:
    case SPELL_INCREA:
    case SPELL_DECATTR:
    case SPELL_DECCYATTR:
    case SPELL_INCATTR:
    case SPELL_INCCYATTR:
    case SPELL_INVIS:
    case SPELL_IMP_INVIS:
    case SPELL_STEALTH:
    case SPELL_ARMOR:
    case SPELL_FLAME_AURA:
    case SPELL_LEVITATE:
      // You may ritual-cast these spells.
      return TRUE;
  }
  return FALSE;
}

// Helper function for ritual spells. Returns TRUE if you can have this spell cast on you, FALSE otherwise.
bool vict_meets_spell_preconditions(struct char_data *vict, int spell, int subtype) {
  // Check for spell duplication.
  for (struct sustain_data *sus = GET_SUSTAINED(vict); sus; sus = sus->next) {
    if (!sus->caster && sus->spell == spell && sus->subtype == subtype) {
      // Already affected.
      return FALSE;
    }
  }
  return TRUE;
}

void set_up_ritualcast(struct char_data *ch, struct room_data *in_room, struct spell_data *spell, int force) {
  FAILURE_CASE(!in_room, "You can't quite figure out where to put things...\r\n");
  FAILURE_CASE(IS_WORKING(ch), "You're too busy to cast a ritual spell.\r\n");
  FAILURE_CASE(CH_IN_COMBAT(ch), "Ritual cast while fighting?? You ARE mad!\r\n");
  FAILURE_CASE(IS_PROJECT(ch), "You can't manipulate physical objects in this form, so setting up a ritual space will be hard.\r\n");
  FAILURE_CASE(!GET_APARTMENT(in_room), "Ritual casting requires an undisturbed place with room to move around-- you'll need to be in an apartment.\r\n");
  FAILURE_CASE(GET_ROOM_VNUM(in_room) >= 6900 && GET_ROOM_VNUM(in_room) <= 6999, "You can't do that in NERPcorpolis.");
  FAILURE_CASE(ch->in_veh, "Ritual casting requires more space to move around-- you'll need to leave your vehicle.\r\n");
  FAILURE_CASE(!spell_is_valid_ritual_spell(spell->type), "That spell isn't eligible for ritual casting. You can only ritual-cast buffs.\r\n");

  // Only one ritual at a time.
  for (struct obj_data *obj = in_room->contents; obj; obj = obj->next_content) {
    FAILURE_CASE(GET_OBJ_VNUM(obj) == OBJ_RITUAL_SPELL_COMPONENTS,
                  "There's already a ritual in progress here. Either wait for it to finish, or ##^WDESTROY^n the components so you can begin your own.\r\n");
  }

  // Find the target.
  struct char_data *vict = get_char_room_vis(ch, buf1);

  if (!check_spell_victim(ch, vict, spell->type, buf1)) {
    return;
  }

  FAILURE_CASE(IS_NPC(vict), "You can only cast ritual spells on player characters.");
  FAILURE_CASE(ch != vict, "You can only cast ritual spells on yourself.");
  FAILURE_CASE(!vict_meets_spell_preconditions(vict, spell->type, spell->subtype), "You can't cast that spell on them right now.");

  // Charge them.
  int cost = RITUAL_SPELL_COMPONENT_COST * force * MAX(1, spells[spell->type].drainpower);
  int time_in_ticks = (RITUAL_SPELL_BASE_TIME * force * MAX(1, spells[spell->type].drainpower)) / (GET_SKILL(ch, SKILL_SORCERY) + MIN(GET_SKILL(ch, SKILL_SORCERY), GET_CASTING(ch)));

  if (GET_NUYEN(ch) < cost) {
    send_to_char(ch, "You need at least %d nuyen on hand to pay for the ritual components.\r\n", cost);
    return;
  }
  lose_nuyen(ch, cost, NUYEN_OUTFLOW_RITUAL_CASTING);

  // Load the ritual casting tracking object.
  AFF_FLAGS(ch).SetBit(AFF_RITUALCAST);
  struct obj_data *components = read_object(OBJ_RITUAL_SPELL_COMPONENTS, VIRTUAL);

  GET_RITUAL_COMPONENT_CASTER(components) = GET_IDNUM(ch);
  GET_RITUAL_COMPONENT_SPELL(components) = spell->type;
  GET_RITUAL_COMPONENT_SUBTYPE(components) = spell->subtype;
  GET_RITUAL_COMPONENT_FORCE(components) = force;
  GET_RITUAL_COMPONENT_TARGET(components) = GET_IDNUM(vict);
  GET_RITUAL_TICKS_AT_START(components) = GET_RITUAL_TICKS_LEFT(components) = time_in_ticks;

  char restring_buf[500];
  snprintf(restring_buf, sizeof(restring_buf), "a ritual invocation of %s", get_spell_name(spell->type, spell->subtype));
  components->restring = str_dup(restring_buf);

  GET_BUILDING(ch) = components;

  obj_to_room(components, ch->in_room);

  send_to_char(ch, "You set up candles, incense, and other ritual components, then settle in to cast %s.\r\n", get_spell_name(spell->type, spell->subtype));
  act("$n sets up candles, incense, and other ritual components, then settles in to cast a spell.", FALSE, ch, 0, 0, TO_ROOM);
}

bool handle_ritualcast_tick(struct char_data *ch, struct obj_data *components) {
  if (--GET_RITUAL_TICKS_LEFT(components) <= 0) {
    struct char_data *vict = NULL;

    // Require that the target is present.
    for (vict = ch->in_room->people; vict; vict = vict->next_in_room) {
      if (GET_IDNUM(vict) == GET_RITUAL_COMPONENT_TARGET(components))
        break;
    }

    if (!vict) {
      send_to_char(ch, "Unable to find their target, the spiraling energies of your ritual scatter away.\r\n");
      act("$n's ritual spell flares, then dissipates into motes of ineffectual light.", FALSE, ch, 0, 0, TO_ROOM);
      // Don't return-- we have cleanup work to do below.
    } else if (!vict_meets_spell_preconditions(vict, GET_RITUAL_COMPONENT_SPELL(components), GET_RITUAL_COMPONENT_SUBTYPE(components))) {
      send_to_char(ch, "There's a vibrant clash of energies as your ritual fails to take hold.\r\n");
      act("$n's ritual spell rebounds off of $N in a vibrant clash of energies.", FALSE, ch, 0, vict, TO_ROOM);
      // Don't return-- we have cleanup work to do below.
    } else {
      // Message completion.
      send_to_char(ch, "A rush of magic runs through you with the completion of your ritual.\r\n");

      // cast_x_spell() w/ predetermined successes
      int spell = GET_RITUAL_COMPONENT_SPELL(components);
      int force = GET_RITUAL_COMPONENT_FORCE(components);
      int subtype = GET_RITUAL_COMPONENT_SUBTYPE(components);
      int successes = MAX(1, get_max_usable_spell_successes(GET_RITUAL_COMPONENT_SPELL(components), GET_RITUAL_COMPONENT_FORCE(components)) * RITUAL_SPELL_MAX_SUCCESS_MULTIPLIER);

      switch (spells[spell].category) {
        case COMBAT:
          mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Got COMBAT spell %d while ritualcasting!", spell);
          break;
        case DETECTION:
          raw_cast_detection_spell(ch, vict, spell, force, NULL, successes);
          break;
        case HEALTH:
          raw_cast_health_spell(ch, vict, spell, subtype, force, NULL, successes);
          break;
        case ILLUSION:
          raw_cast_illusion_spell(ch, vict, spell, force, NULL, successes);
          break;
        case MANIPULATION:
          raw_cast_manipulation_spell(ch, vict, spell, force, NULL, successes, 0);
          break;
      }
    }

    // Destroy the components.
    extract_obj(components);
    components = NULL;
    ch->char_specials.timer = 0;
    STOP_WORKING(ch);
    return TRUE;
  }

  return FALSE;
}