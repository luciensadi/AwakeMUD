#include <stdlib.h>
#include <time.h>

#include "structs.h"
#include "awake.h"
#include "db.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "newmagic.h"
#include "utils.h"
#include "screen.h"
#include "constants.h"
#include "olc.h"
#include "config.h"

#define POWER(name) void (name)(struct char_data *ch, struct char_data *spirit, struct spirit_data *spiritdata, char *arg)
#define SPELLCASTING 0
#define CONJURING 1
#define FAILED_CAST "You fail to bind the mana to your will.\r\n"

//  set_fighting(ch, vict); set_fighting(vict, ch);
#define SET_WAIT_STATE_AND_COMBAT_STATUS_AFTER_OFFENSIVE_SPELLCAST {WAIT_STATE(ch, (int) (OFFENSIVE_SPELL_WAIT_STATE_TIME));}

extern void die(struct char_data *ch);
extern void damage_equip(struct char_data *ch, struct char_data *vict, int power, int type);
extern void damage_obj(struct char_data *ch, struct obj_data *obj, int power, int type);
extern void check_killer(struct char_data * ch, struct char_data * vict);
extern void nonsensical_reply(struct char_data *ch, const char *arg, const char *mode);
extern void send_mob_aggression_warnings(struct char_data *pc, struct char_data *mob);
extern bool mob_is_aggressive(struct char_data *ch, bool include_base_aggression);

bool focus_is_usable_by_ch(struct obj_data *focus, struct char_data *ch);

struct char_data *find_spirit_by_id(int spiritid, long playerid)
{
  for (struct char_data *ch = character_list; ch; ch = ch->next)
    if (IS_NPC(ch) && GET_ACTIVE(ch) == playerid && GET_GRADE(ch) == spiritid)
      return ch;
  return NULL;
}

void adept_release_spell(struct char_data *ch, bool end_spell)
{
  struct sustain_data *spell = GET_SUSTAINED(ch), *ospell = NULL;
  for (; spell; spell = spell->next)
    if (spell->spirit == ch)
      break;

  if (!spell) {
    mudlog("SYSERR: Could not find spell in adept_release_spell!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  GET_SUSTAINED_NUM(ch)--;
  GET_SUSTAINED_FOCI(spell->other)--;
  spell->spirit = NULL;
  for (ospell = GET_SUSTAINED(spell->other); ospell; ospell = ospell->next)
    if (ospell->idnum == spell->idnum && ospell->other == ch) {
      ospell->spirit = NULL;
      break;
    }

  if (!ospell) {
    mudlog("SYSERR: Could not match spell with ospell in adept_release_spell!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  strcpy(buf, spells[spell->spell].name);
  if (spell->spell == SPELL_INCATTR || spell->spell == SPELL_INCCYATTR ||
      spell->spell == SPELL_DECATTR || spell->spell == SPELL_DECCYATTR)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " (%s)", attributes[spell->subtype]);


  if (end_spell) {
    send_to_char(ch, "You deliberately misalign your powers and disrupt %s.\r\n", buf);
    send_to_char(spell->other, "You briefly feel the burden of your tattered %s spell fall back on to you before it fizzles away.\r\n", buf);
    end_sustained_spell(spell->other, ospell);
  } else {
    send_to_char(ch, "You stop sustaining %s.\r\n", buf);
    send_to_char(spell->other, "You suddenly feel the burden of your %s spell fall back on to you.\r\n", buf);
  }
}

void end_sustained_spell(struct char_data *ch, struct sustain_data *sust)
{
  struct sustain_data *temp, *vsust;
  if (sust->other) {
    for (vsust = GET_SUSTAINED(sust->other); vsust; vsust = vsust->next)
      if (sust->caster != vsust->caster && vsust->other == ch && vsust->idnum == sust->idnum)
      {
        if (vsust->spirit) {
          if (GET_TRADITION(vsust->spirit) == TRAD_ADEPT)
            adept_release_spell(vsust->spirit, FALSE);
          else {
            GET_SUSTAINED_FOCI(sust->other)--;
            GET_SUSTAINED_NUM(vsust->spirit)--;
            GET_SUSTAINED(vsust->spirit) = NULL;
          }
        }
        REMOVE_FROM_LIST(vsust, GET_SUSTAINED(sust->other), next);
        delete vsust;
        break;
      }
    if (sust->spell == SPELL_INVIS || sust->spell == SPELL_IMP_INVIS)
      act("You blink and suddenly $n appears!", TRUE, sust->caster ? sust->other : ch, 0, 0, TO_ROOM);
  }
  spell_modify(sust->caster ? sust->other : ch, sust, FALSE);
  REMOVE_FROM_LIST(sust, GET_SUSTAINED(ch), next);
  if (sust->focus)
  {
    GET_SUSTAINED_FOCI(sust->caster ? ch : sust->other)--;
    GET_FOCI(sust->caster ? ch : sust->other)--;
    GET_OBJ_VAL(sust->focus, 4)--;
  }
  if (sust->spirit)
  {
    GET_SUSTAINED_FOCI(ch)--;
    GET_SUSTAINED_NUM(sust->spirit)--;
    GET_SUSTAINED(sust->spirit) = NULL;
  }
  GET_SUSTAINED_NUM(sust->caster ? ch : sust->other)--;
  send_to_char(sust->caster ? ch : sust->other, "You stop sustaining %s.\r\n", spells[sust->spell].name);
  delete sust;
}

void totem_bonus(struct char_data *ch, int action, int type, int &target, int &skill)
{
  extern struct time_info_data time_info;
  if (GET_TOTEM(ch) == TOTEM_OWL) {
    if (time_info.hours > 6 && time_info.hours < 19)
      target += 2;
    else
      skill += 2;
  } else if (GET_TOTEM(ch) == TOTEM_RAVEN) {
    if (ROOM_FLAGGED(get_ch_in_room(ch), ROOM_INDOORS) || SECT(get_ch_in_room(ch)) == SPIRIT_HEARTH)
      target += 1;
  } else if (GET_TOTEM(ch) == TOTEM_SNAKE && CH_IN_COMBAT(ch)) {
    skill--;
  } else if (GET_TOTEM(ch) == TOTEM_BAT || GET_TOTEM(ch) == TOTEM_PUMA) {
    if (time_info.hours > 6 && time_info.hours < 19 && OUTSIDE(ch))
      target += 2;
  } else if (GET_TOTEM(ch) == TOTEM_WIND) {
    if (ROOM_FLAGGED(get_ch_in_room(ch), ROOM_INDOORS) || SECT(get_ch_in_room(ch)) == SPIRIT_HEARTH)
      target += 2;
  } else if (GET_TOTEM(ch) == TOTEM_PUMA && get_ch_in_room(ch)->crowd > 4) {
    target += 2;
  } else if (GET_TOTEM(ch) == TOTEM_SCORPION && (time_info.hours > 6 && time_info.hours < 19)) {
    target += 2;
  } else if (GET_TOTEM(ch) == TOTEM_SPIDER && OUTSIDE(ch)) {
    target++;
  }

  if (action == SPELLCASTING)
  {
    type = spells[type].category;
    switch (GET_TOTEM(ch)) {
      case TOTEM_PRAIRIEDOG:
        if (type == DETECTION)
          skill += 2;
        else if (type == ILLUSION)
          skill++;
        else if (type == COMBAT)
          skill -= 2;
        break;
      case TOTEM_SPIDER:
        if (type == ILLUSION)
          skill += 2;
        break;
      case TOTEM_LEOPARD:
        if (type == COMBAT || type == HEALTH)
          skill += 2;
        break;
      case TOTEM_BEAR:
      case TOTEM_HORSE:
      case TOTEM_LIZARD:
      case TOTEM_PYTHON:
       if (type == HEALTH)
          skill += 2;
        break;
      case TOTEM_ELK:
        if (type == HEALTH)
          skill++;
        else if (type == COMBAT)
          skill -= 2;
        break;
      case TOTEM_TURTLE:
        if (type == ILLUSION)
          skill += 2;
        else if (type == COMBAT)
          skill -= 2;
        break;
      case TOTEM_BUFFALO:
        if (type == HEALTH)
          skill += 2;
        else if (type == ILLUSION)
          skill--;
        break;
      case TOTEM_PUMA:
      case TOTEM_CAT:
      case TOTEM_PARROT:
        if (type == ILLUSION)
          skill += 2;
        break;
      case TOTEM_DOG:
      case TOTEM_EAGLE:
        if (type == DETECTION)
          skill += 2;
        break;
      case TOTEM_DOLPHIN:
      case TOTEM_FISH:
        if (type == DETECTION)
          skill += 2;
        else if (type == COMBAT)
          skill--;
        break;
      case TOTEM_GATOR:
        if (type == COMBAT || type == DETECTION)
          skill += 2;
        else if (type == ILLUSION)
          skill--;
        break;
      case TOTEM_LION:
      case TOTEM_HYENA:
        if (type == COMBAT)
          skill += 2;
        else if (type == HEALTH)
          skill--;
        break;
      case TOTEM_MOUSE:
        if (type == DETECTION || type == HEALTH)
          skill += 2;
        else if (type == COMBAT)
          skill -= 2;
        break;
      case TOTEM_RACCOON:
      case TOTEM_MONKEY:
        if (type == MANIPULATION)
          skill += 2;
        else if (type == COMBAT)
          skill--;
        break;
      case TOTEM_RAT:
      case TOTEM_JACKAL:
        if (type == DETECTION || type == ILLUSION)
          skill += 2;
        else if (type == COMBAT)
          skill--;
        break;
      case TOTEM_JAGUAR:
        if (type == DETECTION)
          skill += 2;
        else if (type == HEALTH)
          skill--;
        break;
      case TOTEM_RAVEN:
        if (type == MANIPULATION)
          skill += 2;
        break;
      case TOTEM_SHARK:
      case TOTEM_WOLF:
        if (type == COMBAT || type == DETECTION)
          skill += 2;
        break;
      case TOTEM_SNAKE:
        if (type == DETECTION || type == HEALTH || type == ILLUSION)
          skill += 2;
        break;
      case TOTEM_CRAB:
        if (type == ILLUSION)
          skill--;
        break;
      case TOTEM_CROCODILE:
        if (type == COMBAT)
          skill += 2;
        else if (type == ILLUSION)
          skill++;
        break;
      case TOTEM_BOAR:
      case TOTEM_WHALE:
        if (type == ILLUSION)
          skill--;
        break;
      case TOTEM_BADGER:
        if (type == COMBAT)
          skill += 2;
        break;
      case TOTEM_CHEETAH:
        if (type == COMBAT)
          skill += 2;
        else if (type == HEALTH)
          skill--;
        break;
      case TOTEM_BAT:
        if (type == DETECTION || type == MANIPULATION)
          skill += 2;
        break;
      case TOTEM_BULL:
        if (type == HEALTH)
          skill +=2;
        else if (type == COMBAT || type == DETECTION)
          skill++;
        break;
      case TOTEM_SCORPION:
      case TOTEM_COBRA:
        if (type == COMBAT || type == ILLUSION)
          skill += 2;
        break;
      case TOTEM_DOVE:
        if (type == HEALTH)
          skill += 2;
        else if (type == DETECTION)
          skill++;
        break;
      case TOTEM_GOOSE:
        if (type == DETECTION)
          skill += 2;
        else if (type == COMBAT)
          skill++;
        break;
      case TOTEM_FOX:
      case TOTEM_OTTER:
        if (type == ILLUSION)
          skill += 2;
        else if (type == COMBAT)
          skill--;
        break;
      case TOTEM_GECKO:
        if (type == GET_TOTEMSPIRIT(ch))
          skill += 2;
        else if (type == COMBAT)
          skill--;
        break;
      case TOTEM_POLECAT:
        if (type == COMBAT) {
          skill++;
          if (time_info.hours < 6 || time_info.hours > 19)
            skill++;
        } else if (type == HEALTH)
          skill--;
        break;
      case TOTEM_STAG:
        if (type == HEALTH || type == ILLUSION)
          skill += 2;
        else if (type == MANIPULATION)
          skill--;
        break;
      case TOTEM_MOON:
        if (type == ILLUSION || type == MANIPULATION || type == DETECTION)
          skill += 2;
        else if (type == COMBAT)
          skill -= 1;
        break;
      case TOTEM_MOUNTAIN:
        if (type == MANIPULATION)
          skill += 2;
        else if (type == ILLUSION)
          skill -= 1;
        break;
      case TOTEM_OAK:
        if (type == HEALTH)
          skill += 2;
        break;
      case TOTEM_SEA:
        if (type == HEALTH || type == MANIPULATION)
          skill += 2;
        break;
      case TOTEM_STREAM:
        if (type == HEALTH)
          skill += 2;
        else if (type == COMBAT)
          skill -= 1;
        break;
      case TOTEM_SUN:
        if (type == COMBAT || type == DETECTION || type == HEALTH)
          skill += 2;
        break;
      case TOTEM_WIND:
        if (type == DETECTION)
          skill += 2;
        break;
      case TOTEM_ADVERSARY:
        if (type == COMBAT || type == MANIPULATION)
          skill += 2;
        break;
      case TOTEM_BACCHUS:
        if (type == ILLUSION)
          skill += 2;
        break;
      case TOTEM_CREATOR:
        if (type == COMBAT)
          skill -= 1;
        break;
      case TOTEM_DARKKING:
        if (type == HEALTH)
          skill += 2;
        break;
      case TOTEM_DRAGONSLAYER:
        if (type == COMBAT)
          skill += 3;
        else if (type == ILLUSION || type == DETECTION)
          skill -= 1;
        break;
      case TOTEM_FIREBRINGER:
        if (type == MANIPULATION || type == DETECTION)
          skill += 2;
        else if (type == ILLUSION)
          skill -= 1;
        break;
      case TOTEM_HORNEDMAN:
        if (type == COMBAT)
          skill += 2;
        break;
      case TOTEM_LOVER:
        if (type == ILLUSION || type == MANIPULATION)
          skill += 2;
        break;
      case TOTEM_SEAKING:
        if (type == MANIPULATION)
          skill += 2;
        else if (type == COMBAT)
          skill -= 1;
        break;
      case TOTEM_SEDUCTRESS:
        if (type == ILLUSION || type == MANIPULATION)
          skill += 2;
        break;
      case TOTEM_SIREN:
        if (type == ILLUSION || type == MANIPULATION)
          skill += 2;
        break;
      case TOTEM_SKYFATHER:
        if (type == DETECTION || type == MANIPULATION)
          skill += 2;
        break;
      case TOTEM_WILDHUNTSMAN:
        if (type == ILLUSION || type == DETECTION)
          skill += 2;
        break;
      case TOTEM_WISEWARRIOR:
        if (type == COMBAT || type == DETECTION)
          skill += 2;
        else if (type == ILLUSION)
          skill -= 1;
        break;
    }
  } else if (action == CONJURING)
  {
    switch (GET_TOTEM(ch)) {
    case TOTEM_SCORPION:
      skill--;
      break;
    case TOTEM_SPIDER:
      skill++;
      break;
    case TOTEM_PRAIRIEDOG:
    case TOTEM_ELK:
    case TOTEM_POLECAT:
      if (type == SPIRIT_FOREST || type == SPIRIT_DESERT || type == SPIRIT_MOUNTAIN || type == SPIRIT_PRAIRIE)
        skill += 2;
      break;
    case TOTEM_STAG:
    case TOTEM_PYTHON:
    case TOTEM_BEAR:
    case TOTEM_BADGER:
    case TOTEM_JAGUAR:
    case TOTEM_PARROT:
      if (type == SPIRIT_FOREST)
        skill += 2;
      break;
    case TOTEM_COBRA:
      if (type == SPIRIT_FOREST)
        skill++;
      break;
    case TOTEM_JACKAL:
    case TOTEM_HORSE:
    case TOTEM_BUFFALO:
    case TOTEM_LION:
    case TOTEM_CHEETAH:
      if (type == SPIRIT_PRAIRIE)
        skill += 2;
      break;
    case TOTEM_CAT:
    case TOTEM_RAT:
    case TOTEM_RACCOON:
      if (type == SPIRIT_CITY)
        skill += 2;
      break;
    case TOTEM_DOG:
    case TOTEM_MOUSE:
      if (type == SPIRIT_FIELD || type == SPIRIT_HEARTH)
        skill += 2;
      break;
    case TOTEM_MONKEY:
      if (type == SPIRIT_FIELD || type == SPIRIT_HEARTH || type == SPIRIT_CITY)
        skill += 2;
      break;
    case TOTEM_WHALE:
    case TOTEM_DOLPHIN:
    case TOTEM_SHARK:
    case TOTEM_CRAB:
    case TOTEM_CROCODILE:
      if (type == SPIRIT_SEA)
        skill += 2;
      break;
    case TOTEM_EAGLE:
    case TOTEM_RAVEN:
    case TOTEM_BAT:
    case TOTEM_DOVE:
      if (type == SPIRIT_MIST || type == SPIRIT_STORM || type == SPIRIT_WIND)
        skill += 2;
      break;
    case TOTEM_PUMA:
      if (type == SPIRIT_MOUNTAIN)
        skill += 2;
      break;
    case TOTEM_LEOPARD:
      if (time_info.hours < 6 || time_info.hours > 19)
        skill += 2;
      break;
    case TOTEM_LIZARD:
    case TOTEM_GATOR:
    case TOTEM_SNAKE:
    case TOTEM_WOLF:
    case TOTEM_FISH:
    case TOTEM_GOOSE:
    case TOTEM_OTTER:
    case TOTEM_TURTLE:
      if (GET_TOTEMSPIRIT(ch) == type)
        skill += 2;
      break;
    case TOTEM_MOON:
      if (type == SPIRIT_RIVER || type == SPIRIT_SEA || type == SPIRIT_LAKE)
        skill += 1;
      break;
    case TOTEM_MOUNTAIN:
      if (type == SPIRIT_MOUNTAIN)
        skill += 2;
      break;
    case TOTEM_OAK:
      if (type == SPIRIT_FOREST || type == SPIRIT_HEARTH)
        skill += 2;
      break;
    case TOTEM_SEA:
      if (type == SPIRIT_SEA || type == SPIRIT_HEARTH)
        skill += 2;
      break;
    case TOTEM_STREAM:
      if (type == SPIRIT_RIVER)
        skill += 2;
      break;
    case TOTEM_SUN:
      if (type == SPIRIT_FOREST || type == SPIRIT_DESERT || type == SPIRIT_MOUNTAIN || type == SPIRIT_PRAIRIE ||
	      type == SPIRIT_MIST || type == SPIRIT_STORM || type == SPIRIT_RIVER || type == SPIRIT_SEA ||
          type == SPIRIT_WIND || type == SPIRIT_LAKE || type == SPIRIT_SWAMP || type == SPIRIT_CITY ||
          type == SPIRIT_FIELD)
        skill += 2;
      if (time_info.hours < 6 || time_info.hours > 19)
        skill -= 2;
      break;
    case TOTEM_WIND:
      if (type == SPIRIT_MIST || type == SPIRIT_STORM || type == SPIRIT_WIND)
        skill += 2;
      break;
    case TOTEM_BACCHUS:
      if (type == SPIRIT_CITY || type == SPIRIT_FIELD || type == SPIRIT_HEARTH)
        skill += 2;
      break;
    case TOTEM_CREATOR:
      if (type == SPIRIT_CITY)
        skill += 2;
      break;
    case TOTEM_DARKKING:
      if (type == SPIRIT_CITY || type == SPIRIT_FIELD || type == SPIRIT_HEARTH)
        skill += 2;
      break;
    case TOTEM_DRAGONSLAYER:
      if (type == SPIRIT_CITY || type == SPIRIT_FIELD || type == SPIRIT_HEARTH)
        skill += 1;
      break;
    case TOTEM_GREATMOTHER:
      if (type == SPIRIT_FIELD || type == SPIRIT_FOREST || type == SPIRIT_SEA || type == SPIRIT_LAKE ||
          type == SPIRIT_RIVER || type == SPIRIT_SWAMP)
        skill += 2;
      break;
    case TOTEM_HORNEDMAN:
      if (type == SPIRIT_DESERT || type == SPIRIT_FOREST || type == SPIRIT_MOUNTAIN || type == SPIRIT_PRAIRIE)
      break;
    case TOTEM_LOVER:
      if (type == SPIRIT_SEA || type == SPIRIT_LAKE || type == SPIRIT_RIVER || type == SPIRIT_SWAMP)
        skill += 2;
      break;
    case TOTEM_SEAKING:
      if (type == SPIRIT_SEA || type == SPIRIT_LAKE || type == SPIRIT_RIVER || type == SPIRIT_SWAMP)
        skill += 2;
      break;
    case TOTEM_SEDUCTRESS:
      if (type == SPIRIT_CITY || type == SPIRIT_FIELD || type == SPIRIT_HEARTH)
        skill += 2;
      break;
    case TOTEM_SIREN:
      if (type == SPIRIT_SEA || type == SPIRIT_LAKE || type == SPIRIT_RIVER || type == SPIRIT_SWAMP)
        skill += 2;
      break;
    case TOTEM_SKYFATHER:
      if (type == SPIRIT_STORM)
        skill += 2;
      break;
    case TOTEM_WILDHUNTSMAN:
      if (type == SPIRIT_STORM)
        skill += 2;
      break;
    }
  }
}

void end_spirit_existance(struct char_data *ch, bool message)
{
  struct char_data *tempc;
  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    tempc = d->original ? d->original : d->character;
    if (tempc && GET_IDNUM(tempc) == GET_ACTIVE(ch))
    {
      for (struct spirit_data *spirit = GET_SPIRIT(tempc); spirit; spirit = spirit->next)
        if (spirit->id == GET_GRADE(ch)) {
          struct spirit_data *temp;
          REMOVE_FROM_LIST(spirit, GET_SPIRIT(tempc), next);
          delete spirit;
          break;
        }
      GET_NUM_SPIRITS(tempc)--;
      if (message && d->character)
        send_to_char(d->character, "%s returns to the metaplanes, its existence in this world finished.\r\n", CAP(GET_NAME(ch)));

      GET_ELEMENTALS_DIRTY_BIT(tempc) = TRUE;
      break;
    }
  }
  extract_char(ch);
}

void elemental_fulfilled_services(struct char_data *ch, struct char_data *mob, struct spirit_data *spirit)
{
  if (spirit->services < 1 && !(MOB_FLAGGED(mob, MOB_SPIRITGUARD) || MOB_FLAGGED(mob, MOB_STUDY) || GET_SUSTAINED(mob) || GET_SUSTAINED_NUM(mob) || CH_IN_COMBAT(mob)))
  {
    send_to_char(ch, "Its services fulfilled, %s departs to the metaplanes.\r\n", CAP(GET_NAME(mob)));
    end_spirit_existance(mob, FALSE);
    GET_ELEMENTALS_DIRTY_BIT(ch) = TRUE;
  }
}

bool conjuring_drain(struct char_data *ch, int force)
{
  if (GET_BACKGROUND_AURA(get_ch_in_room(ch)) != AURA_POWERSITE)
    force += GET_BACKGROUND_COUNT(get_ch_in_room(ch)) / 2;
  int drain = 0;
  if (force <= GET_CHA(ch) / 2)
    drain = LIGHT;
  else if (force <= GET_CHA(ch))
    drain = MODERATE;
  else if (force > GET_CHA(ch) * 1.5)
    drain = DEADLY;
  else
    drain = SERIOUS;
  drain = convert_damage(stage(-success_test(GET_CHA(ch), force), drain));
  
  // Physical drain.
  if (force > GET_MAG(ch) / 100) {
    // Iterate through bioware to find relevant bioware.
    bool bPlat = FALSE;
    bool bTraum = FALSE;
    for (struct obj_data *bio = ch->bioware; bio && drain > 0; bio = bio->next_content) {
      if (GET_OBJ_VAL(bio, 0) == BIO_PLATELETFACTORY && drain >= 3)
        bPlat = TRUE;
      
      if (GET_OBJ_VAL(bio, 0) == BIO_TRAUMADAMPNER)
        bTraum = TRUE;
    }
    if (bPlat)
      drain--;
    if (bTraum) {
      drain--;
      GET_MENTAL(ch) -= 100;
    }
      
    if (drain <= 0)
    return FALSE;
      
    GET_PHYSICAL(ch) -= drain *100;
  }
  // Mental drain.
  else {
    // Iterate through bioware to find a trauma damper.
    for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content) {
      if (GET_OBJ_VAL(bio, 0) == BIO_TRAUMADAMPNER) {
        drain--;
        break;
      }
    }
    if (drain <= 0)
      return FALSE;
      
    GET_MENTAL(ch) -= drain * 100;
    // Mental overflow to physical
    if (GET_MENTAL(ch) < 0) {
      GET_PHYSICAL(ch) += GET_MENTAL(ch);
      GET_MENTAL(ch) = 0;
    }
  }
  update_pos(ch);
  if ((GET_POS(ch) <= POS_STUNNED) && (GET_POS(ch) > POS_DEAD))
  {
    if (CH_IN_COMBAT(ch))
      stop_fighting(ch);
    send_to_char("You are unable to resist the drain from conjuring and fall unconscious!\r\n", ch);
    act("$n collapses unconscious!", FALSE, ch, 0, 0, TO_ROOM);
    struct sustain_data *next;
    for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = next) {
      next = sust->next;
      if (sust->caster && !sust->focus && !sust->spirit)
        end_sustained_spell(ch, sust);
    }
  } else if (GET_POS(ch) == POS_DEAD)
  {
    if (CH_IN_COMBAT(ch))
      stop_fighting(ch);
    send_to_char("The energy from the conjuring ritual overloads your body with energy, killing you...\r\n", ch);
    act("$n suddenly collapses, dead!", FALSE, ch, 0, 0, TO_ROOM);
    die(ch);
    return TRUE;
  }
  return FALSE;
}

void magic_perception(struct char_data *ch, int force, int spell)
{
  int base = 4 + (int)(GET_MAG(ch) / 100) - force, target = 0, skill = 0, starg = 0;
  totem_bonus(ch, SPELLCASTING, spell, starg, skill);
  if (skill > 0)
    base--;
  for (struct char_data *vict = ch->in_veh ? ch->in_veh->people : ch->in_room->people; vict; vict = ch->in_veh ? vict->next_in_veh : vict->next_in_room) {
    if (ch == vict)
      continue;
    target = base;
    if (GET_MAG(vict) > 0)
      target -= 2;
    if (IS_DUAL(vict) || IS_ASTRAL(vict))
      target -= 2;
    if (success_test(GET_INT(ch) + (GET_TRADITION(ch) == TRAD_ADEPT ? GET_POWER(ch, ADEPT_IMPROVED_PERCEPT) : 0), target)) {
      if (IS_DUAL(vict) || IS_ASTRAL(vict))
        act("You notice $n manipulating the astral plane.", FALSE, ch, 0, vict, TO_VICT);
      else act("You notice $n performing magic.", TRUE, ch, 0, vict, TO_VICT);

#ifdef GUARDS_ATTACK_MAGES
      // Guards and fighters don't like people casting magic around them.
      if (IS_NPC(vict) && (MOB_FLAGGED(vict, MOB_GUARD)
                           || mob_is_aggressive(vict, TRUE)
                           || GET_MOBALERT(vict) == MALERT_ALARM))
      {
        send_mob_aggression_warnings(ch, vict);
        set_fighting(vict, ch);
      }
#endif
    }
  }
}

bool spell_drain(struct char_data *ch, int type, int force, int damage)
{
  char buf[MAX_STRING_LENGTH];
  int target = (int)(force / 2), success = 0;
  target += spells[type].drainpower;
  if (GET_BACKGROUND_AURA(get_ch_in_room(ch)) != AURA_POWERSITE)
    target += (GET_BACKGROUND_COUNT(get_ch_in_room(ch)) / 2);
  if (!damage)
    damage = spells[type].draindamage;
  else if (type)
    damage += spells[type].draindamage + 3;
  magic_perception(ch, force, type);
  snprintf(buf, sizeof(buf), "Drain Test: F:%d%s TN:%d Dice: %d", force, wound_name[damage], target, GET_WIL(ch) + GET_DRAIN(ch));
  success = success_test(GET_WIL(ch) + GET_DRAIN(ch), target);
  damage = convert_damage(stage(-success, damage));
  act(buf, FALSE, ch, NULL, NULL, TO_ROLLS);

  if (damage <= 0)
    return FALSE;

  // Apply physical drain damage.
  if (force > GET_MAG(ch) / 100 || IS_PROJECT(ch)) {
    // Iterate through bioware to find a platelet factory.
    bool bPlat = FALSE;
    bool bTraum = FALSE;
    struct char_data *chptr = (IS_PROJECT(ch) ? ch->desc->original : ch);
    for (struct obj_data *bio = chptr->bioware; bio && damage > 0; bio = bio->next_content) {
      if (GET_OBJ_VAL(bio, 0) == BIO_PLATELETFACTORY && damage >= 3)
        bPlat = TRUE;
      if (GET_OBJ_VAL(bio, 0) == BIO_TRAUMADAMPNER)
        bTraum = TRUE;
    }
    if (bPlat)
      damage--;
    if (bTraum) {
      damage--;
      GET_MENTAL(chptr) -= 100;
    }

    if (damage <= 0)
      return FALSE;

    GET_PHYSICAL(ch) -= damage * 100;
    if (IS_PROJECT(ch)) {
      GET_PHYSICAL(ch->desc->original) -= damage * 100;
      AFF_FLAGS(ch->desc->original).SetBit(AFF_DAMAGED);
    }
  }

  // Apply mental drain damage.
  else {
    // Iterate through cyberware to find a trauma damper.
    for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content) {
      if (GET_OBJ_VAL(bio, 0) == BIO_TRAUMADAMPNER) {
        damage--;
        break;
      }
    }

    if (damage <= 0)
      return FALSE;

    GET_MENTAL(ch) -= damage * 100;
  }

  // Roll over mental damage to physical.
  if (GET_MENTAL(ch) < 0) {
    GET_PHYSICAL(ch) += GET_MENTAL(ch);
    GET_MENTAL(ch) = 0;
  }

  snprintf(buf, sizeof(buf), "Successes: %d  Damage: %d", success, damage);
  act(buf, FALSE, ch, NULL, NULL, TO_ROLLS);

  update_pos(ch);
  if ((GET_POS(ch) <= POS_STUNNED) && (GET_POS(ch) > POS_DEAD))
  {
    if (CH_IN_COMBAT(ch))
      stop_fighting(ch);
    send_to_char("You are unable to resist the drain from spell casting and fall unconscious!\r\n", ch);
    act("$n collapses unconscious!", FALSE, ch, 0, 0, TO_ROOM);
    struct sustain_data *next = NULL;
    for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = next) {
      next = sust->next;
      if (sust->caster && !sust->focus && !sust->spirit)
        end_sustained_spell(ch, sust);
    }
  } else if (GET_POS(ch) == POS_DEAD)
  {
    if (CH_IN_COMBAT(ch))
      stop_fighting(ch);
    send_to_char("The feedback from spell casting floods your body, killing you...\r\n", ch);
    act("$n suddenly collapses, dead!", FALSE, ch, 0, 0, TO_ROOM);
    die(ch);
    return TRUE;
  }
  return FALSE;


}

struct char_data *find_target_at_range(struct char_data *ch, char *name, char *direction)
{
  return NULL;
}
void create_sustained(struct char_data *ch, struct char_data *vict, int spell, int force, int sub, int success, int drain)
{
  struct obj_data *focus = NULL;
  for (int i = 0; !focus && i < NUM_WEARS; i++)
    if (GET_EQ(ch, i)
        && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_FOCUS
        && GET_FOCUS_TYPE(GET_EQ(ch, i)) == FOCI_SUSTAINED
        && !GET_FOCUS_ACTIVATED(GET_EQ(ch, i))
        && GET_FOCUS_BONDED_SPIRIT_OR_SPELL(GET_EQ(ch, i)) == spell
        && GET_FOCUS_FORCE(GET_EQ(ch, i)) >= force
        && focus_is_usable_by_ch(GET_EQ(ch, i), ch))
      focus = GET_EQ(ch, i);
  GET_SUSTAINED_NUM(ch)++;
  if (focus)
  {
    GET_SUSTAINED_FOCI(ch)++;
    GET_FOCUS_ACTIVATED(focus)++;
    GET_FOCI(ch)++;
  }
  struct sustain_data *sust = new sustain_data;
  sust->spell = spell;
  sust->subtype = sub;
  sust->force = force;
  sust->success = success;
  sust->other = vict;
  sust->caster = TRUE;
  sust->drain = drain;
  sust->idnum = number(0, 100000);
  sust->next = GET_SUSTAINED(ch);
  sust->focus = focus;
  GET_SUSTAINED(ch) = sust;
  struct sustain_data *vsust = new sustain_data;
  *vsust = *sust;
  vsust->caster = FALSE;
  vsust->other = ch;
  vsust->next = GET_SUSTAINED(vict);
  vsust->focus = focus;
  GET_SUSTAINED(vict) = vsust;
  spell_modify(vict, vsust, TRUE);
}

void spell_bonus(struct char_data *ch, int spell, int &skill, int &target)
{
  if (GET_BACKGROUND_AURA(get_ch_in_room(ch)) == AURA_POWERSITE)
    skill += GET_BACKGROUND_COUNT(get_ch_in_room(ch));
  else
    target += GET_BACKGROUND_COUNT(get_ch_in_room(ch));
  if (GET_TRADITION(ch) == TRAD_SHAMANIC)
    totem_bonus(ch, SPELLCASTING, spell, target, skill);
  else if (GET_TRADITION(ch) == TRAD_HERMETIC && GET_SPIRIT(ch) && spells[spell].category != HEALTH)
  {
    for (struct spirit_data *spirit = GET_SPIRIT(ch); spirit; spirit = spirit->next)
      if (((spells[spell].category == MANIPULATION && spirit->type == ELEM_EARTH) ||
           (spells[spell].category == COMBAT && spirit->type == ELEM_FIRE) ||
           (spells[spell].category == ILLUSION && spirit->type == ELEM_WATER) ||
           (spells[spell].category == DETECTION && spirit->type == ELEM_AIR)) &&
          spirit->called == TRUE) {
        struct char_data *mob = find_spirit_by_id(spirit->id, GET_IDNUM(ch));
        if (mob && MOB_FLAGGED(mob, MOB_AIDSORCERY)) {
          send_to_char(ch, "%s aids your spell casting.\r\n", CAP(GET_NAME(mob)));
          MOB_FLAGS(mob).RemoveBit(MOB_AIDSORCERY);
          skill += GET_LEVEL(mob);
        }
      }
  }
  int max_specific_spell = 0;
  int max_spell_category = 0;
  bool used_expendable = FALSE;
  if (GET_FOCI(ch) > 0)
  {
    for (int i = 0; i < NUM_WEARS; i++)
      if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_FOCUS && focus_is_usable_by_ch(GET_EQ(ch, i), ch))
        switch (GET_FOCUS_TYPE(GET_EQ(ch, i))) {
          case FOCI_EXPENDABLE:
            if (!used_expendable && spells[spell].category == GET_FOCUS_BONDED_SPIRIT_OR_SPELL(GET_EQ(ch, i))) {
              skill += GET_FOCUS_FORCE(GET_EQ(ch, i));
              send_to_char(ch, "Its energies expended, %s crumbles away.", decapitalize_a_an(GET_OBJ_NAME(GET_EQ(ch, i))));
              extract_obj(GET_EQ(ch, i));
              used_expendable = TRUE;
            }
          case FOCI_SPEC_SPELL:
            if (spell == GET_FOCUS_BONDED_SPIRIT_OR_SPELL(GET_EQ(ch, i)))
              max_specific_spell = MAX(max_specific_spell, GET_FOCUS_FORCE(GET_EQ(ch, i)));
          case FOCI_SPELL_CAT:
            if (spells[spell].category == GET_FOCUS_BONDED_SPIRIT_OR_SPELL(GET_EQ(ch, i)))
              max_spell_category = MAX(max_spell_category, GET_FOCUS_FORCE(GET_EQ(ch, i)));
        }
  }
  skill += max_spell_category + max_specific_spell;
}

bool find_duplicate_spell(struct char_data *ch, struct char_data *vict, int spell, int sub)
{
  if (spells[spell].duration == INSTANT)
    return FALSE;
  struct sustain_data *sus;
  if (!vict || vict == ch)
    sus = GET_SUSTAINED(ch);
  else
    sus = GET_SUSTAINED(vict);
  for (; sus; sus = sus->next)
    if (!sus->caster && sus->spell == spell && sus->subtype == sub)
    {
      send_to_char(ch, "%s are already affected by that spell.\r\n", (!vict || vict == ch) ? "You" : "They");
      return TRUE;
    }
  return FALSE;
}

bool check_spell_victim(struct char_data *ch, struct char_data *vict, int spell, char *buf)
{
  if (!vict)
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf);
  else if (((IS_PROJECT(ch) || IS_ASTRAL(ch)) && !(IS_DUAL(vict) || IS_ASTRAL(vict) || IS_PROJECT(vict))) ||
           ((IS_PROJECT(vict) || IS_ASTRAL(vict)) && !(IS_DUAL(ch) || IS_ASTRAL(ch) || IS_PROJECT(ch))))
    send_to_char("They aren't accessible from this plane.\r\n", ch);
  else if (spells[spell].physical && IS_ASTRAL(vict))
    send_to_char("That spell can't affect beings with no physical form.\r\n", ch);
  else
    return TRUE;
  return FALSE;
}

int reflect_spell(struct char_data *ch, struct char_data *vict, int spell, int force, int sub, int target, int &success)
{
  success -= success_test(GET_REFLECT(vict), force);
  if (success < 0) {
    success *= -1;
    success = success_test(success, target + modify_target(vict));
    send_to_char("You reflect the spell back!\r\n", vict);
    send_to_char("Your spell is reflected back at you!\r\n", ch);
    return TRUE;
  }
  return FALSE;
}

int resist_spell(struct char_data *ch, int spell, int force, int sub)
{
  int skill = 0;
  if (sub)
    skill = GET_ATT(ch, sub);
  else if (spell == SPELL_CHAOS)
    skill = GET_INT(ch);
  else if (spells[spell].physical)
    skill = GET_BOD(ch);
  else
    skill = GET_WIL(ch);
  if (GET_TRADITION(ch) == TRAD_SHAMANIC) {
    if (GET_TOTEM(ch) == TOTEM_HORSE && spells[spell].category == GET_TOTEMSPIRIT(ch))
      skill--;
    else if (GET_TOTEM(ch) == TOTEM_LEOPARD && spells[spell].category == ILLUSION)
      skill--;
  }
  if (GET_TRADITION(ch) == TRAD_ADEPT)
  {
    if (GET_POWER(ch, ADEPT_MAGIC_RESISTANCE))
      skill += GET_POWER(ch, ADEPT_MAGIC_RESISTANCE);
    if (GET_POWER(ch, ADEPT_SPELL_SHROUD) && spells[spell].category == DETECTION)
      skill += GET_POWER(ch, ADEPT_SPELL_SHROUD);
    if (GET_POWER(ch, ADEPT_TRUE_SIGHT) && spells[spell].category == ILLUSION)
      skill += GET_POWER(ch, ADEPT_TRUE_SIGHT);

    if (GET_FOCI(ch) > 0) {
      for (int i = 0; i < NUM_WEARS; i++) {
        if (GET_EQ(ch, i)
            && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_FOCUS
            && GET_FOCUS_TYPE(GET_EQ(ch, i)) == FOCI_SPELL_DEFENSE
            && focus_is_usable_by_ch(GET_EQ(ch, i), ch))
        {
          skill += GET_FOCUS_FORCE(GET_EQ(ch, i));
        }
      }
    }
  }
  skill += GET_SDEFENSE(ch);
  return success_test(skill, force);
}

void cast_combat_spell(struct char_data *ch, int spell, int force, char *arg)
{
  struct char_data *vict = NULL;
  bool reflected = FALSE;
  int basedamage = 0;

  two_arguments(arg, buf, buf1);

  if (!*buf)
  {
    send_to_char("What damage level do you wish to cast that spell at?\r\n", ch);
    return;
  }

  for (basedamage = 0; *wound_name[basedamage] != '\n'; basedamage++)
    if (is_abbrev(buf, wound_name[basedamage]))
      break;
  if (basedamage > 4 || basedamage == 0) {
    send_to_char(ch, "'%s' is not a valid damage level, please choose between Light, Moderate, Serious and Deadly.\r\n", capitalize(buf));
    return;
  }

  if (*buf1)
    vict = get_char_room_vis(ch, buf1);

  if (!check_spell_victim(ch, vict, spell, buf1))
    return;

  if (ch == vict) {
    send_to_char("You can't target yourself with a combat spell!\r\n", ch);
    return;
  }

  if (get_ch_in_room(ch)->peaceful) {
    send_to_char("This room just has a peaceful, easy feeling...\r\n", ch);
    return;
  }

  // Pre-calculate the modifiers to the target (standard modify_target(), altered by spell_bonus()).
  int target_modifiers = modify_target(ch);
  int skill = GET_SKILL(ch, SKILL_SORCERY) + MIN(GET_SKILL(ch, SKILL_SORCERY), GET_CASTING(ch));
  int success = 0;
  spell_bonus(ch, spell, skill, target_modifiers);

  if (skill == -1)
    return;

  check_killer(ch, vict);
  struct char_data *temp = vict;

  switch (spell)
  {
  case SPELL_MANABOLT:
    if ((!IS_NPC(ch) && (IS_NPC(vict) && MOB_FLAGGED(vict, MOB_INANIMATE))) || (PLR_FLAGGED(ch, PLR_KILLER) && !IS_NPC(vict))) {
      act("You can't affect $N with manabolt.", TRUE, ch, 0, vict, TO_CHAR);
      return;
    }
    SET_WAIT_STATE_AND_COMBAT_STATUS_AFTER_OFFENSIVE_SPELLCAST;

    success = success_test(skill, GET_WIL(vict) + target_modifiers);
    if (success > 0 && GET_REFLECT(vict) && (reflected = reflect_spell(ch, vict, spell, force, 0, GET_WIL(ch), success))) {
      vict = ch;
      ch = temp;
    }
    success -= resist_spell(vict, spell, force, 0);
    if (success > 0) {
      int dam = convert_damage(stage(success, basedamage));
      if (GET_MENTAL(vict) - (dam * 100) <= 0) {
        act("$n falls to the floor as ^Rblood^n erupts from every pore on $s head.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("Your world turns to red as you feel blood flow from every pore on your head.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 300) {
        act("$n grabs $s head in pain as blood flows from $s ears.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("Your head is filled with immense pain as your ears begin to bleed.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 700) {
        act("$n grabs ahold of $s head in pain.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("A sudden flash of pain in your head causes you to reflexively grab it.\r\n", vict);
      } else {
        act("$n seems to flinch slightly as blood trickles from $s nose.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("Slight pain fills your mind from an unknown source.\r\n", vict);
      }
      if (damage(ch, vict, dam, TYPE_COMBAT_SPELL, PHYSICAL) && reflected) {
        // Vict was killed by their own reflected spell. Stop processing-- we don't want to calculate drain on a nulled struct.
        return;
      }

      if (IS_NPC(vict) && !IS_NPC(ch))
        GET_LASTHIT(vict) = GET_IDNUM(ch);
    } else
      send_to_char(FAILED_CAST, reflected ? vict : ch);
    spell_drain(reflected ? vict : ch, spell, force, basedamage);
    break;
  case SPELL_STUNBOLT:
    if ((!IS_NPC(ch) && IS_NPC(vict) && MOB_FLAGGED(vict, MOB_INANIMATE)) || (PLR_FLAGGED(ch, PLR_KILLER) && !IS_NPC(vict))) {
      act("You can't affect $N with stunbolt.", TRUE, ch, 0, vict, TO_CHAR);
      return;
    }
    SET_WAIT_STATE_AND_COMBAT_STATUS_AFTER_OFFENSIVE_SPELLCAST;

    success = success_test(skill, GET_WIL(vict) + target_modifiers);
    if (success > 0 && GET_REFLECT(vict) && (reflected = reflect_spell(ch, vict, spell, force, 0, GET_WIL(ch), success))) {
      vict = ch;
      ch = temp;
    }
    success -= resist_spell(vict, spell, force, 0);
    if (success > 0) {
      int dam = convert_damage(stage(success, basedamage));
      if (GET_MENTAL(vict) - (dam * 100) <= 0) {
        act("$n falls to the floor unconscious.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("The world turns black around you as you suddenly fall unconscious.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 300) {
        act("$n grabs $s head and cringes.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("Your head is filled with an unbearable pressure as your vision begins to fade.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 700) {
        act("$n shakes $s head forcibly as though trying to clear it.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("A wave of tiredness comes over you, but seems to clear slightly as you shake your head.\r\n", vict);
      } else  {
        act("$n recoils slightly as though hit by an invisible force.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You mind goes slightly hazy as though you had just been punched.\r\n", vict);
      }
      if (damage(ch, vict, dam, TYPE_COMBAT_SPELL, MENTAL) && reflected) {
        // Vict was killed by their own reflected spell. Stop processing-- we don't want to calculate drain on a nulled struct.
        return;
      }
      if (IS_NPC(vict) && !IS_NPC(ch))
        GET_LASTHIT(vict) = GET_IDNUM(ch);
    } else
      send_to_char(FAILED_CAST, reflected ? vict : ch);
    spell_drain(reflected ? vict : ch, spell, force, basedamage);
    break;
  case SPELL_POWERBOLT:
    if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_KILLER) && !IS_NPC(vict)) {
      act("You have the KILLER flag, so you can't affect $N with powerbolt.", TRUE, ch, 0, vict, TO_CHAR);
      return;
    }
    SET_WAIT_STATE_AND_COMBAT_STATUS_AFTER_OFFENSIVE_SPELLCAST;

    success = success_test(skill, GET_BOD(vict) + target_modifiers);
    if (success > 0 && GET_REFLECT(vict) && (reflected = reflect_spell(ch, vict, spell, force, 0, GET_BOD(ch), success))) {
      vict = ch;
      ch = temp;
    }
    success -= resist_spell(vict, spell, force, 0);
    if (success > 0) {
      int dam = convert_damage(stage(success, basedamage));
      if (GET_MENTAL(vict) - (dam * 100) <= 0) {
        act("$n screams loudly as $s body is torn asunder by magic.", FALSE, vict, 0, 0, TO_ROOM);
        send_to_char("You begin to feel yourself tearing apart.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 300) {
        act("$n cries out in pain as $s begins to bleed from nearly every pore on $s body.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You uncontrollably scream as an immense force tears at your body.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 700) {
        act("$n grabs $s stomach as a trickle of blood comes from $s mouth.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("Your torso is filled with a sharp stabbing pain as you cough up some blood.\r\n", vict);
      } else {
        act("$n grimaces, obviously afflicted with mild pain.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You feel a dull throb of pain flow through your body.\r\n", vict);
      }
      if (damage(ch, vict, dam, TYPE_COMBAT_SPELL, MENTAL) && reflected) {
        // Vict was killed by their own reflected spell. Stop processing-- we don't want to calculate drain on a nulled struct.
        return;
      }
      if (IS_NPC(vict) && !IS_NPC(ch))
        GET_LASTHIT(vict) = GET_IDNUM(ch);
    } else
      send_to_char(FAILED_CAST, reflected ? vict : ch);
    spell_drain(reflected ? vict : ch, spell, force, basedamage);
    break;
  }
}

void cast_detection_spell(struct char_data *ch, int spell, int force, char *arg, struct char_data *mob)
{
  struct char_data *vict = NULL;
  if (mob)
    vict = mob;
  else if (*arg)
    vict = get_char_room_vis(ch, arg);
  if (!check_spell_victim(ch, vict, spell, arg))
    return;
  if (find_duplicate_spell(ch, vict, spell, 0))
    return;

  // Pre-calculate the modifiers to the target (standard modify_target(), altered by spell_bonus()).
  int target_modifiers = modify_target(ch);
  int skill = GET_SKILL(ch, SKILL_SORCERY) + MIN(GET_SKILL(ch, SKILL_SORCERY), GET_CASTING(ch));
  int success = 0;
  spell_bonus(ch, spell, skill, target_modifiers);
  if (skill == -1)
    return;
  switch (spell)
  {
  case SPELL_MINDLINK:
    WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));
    success = success_test(skill, 4 + target_modifiers);
    for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = sust->next)
      if (sust->spell == SPELL_MINDLINK) {
        send_to_char("You are already under the influence of a mindlink.\r\n", ch);
        return;
      }
    for (struct sustain_data *sust = GET_SUSTAINED(vict); sust; sust = sust->next)
      if (sust->spell == SPELL_MINDLINK) {
        send_to_char("They are already under the influence of a mindlink.\r\n", ch);
        return;
      }
    if (success > 0) {
      create_sustained(ch, vict, spell, force, 0, success, spells[spell].draindamage);
      act("You successfully sustain that spell on $N.", FALSE, ch, 0, vict, TO_CHAR);
      vict->char_specials.mindlink = ch;
      ch->char_specials.mindlink = vict;
    } else send_to_char(FAILED_CAST, ch);
    spell_drain(ch, spell, force, 0);
    break;
  case SPELL_COMBATSENSE:
    WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));
    success = success_test(skill, 4 + target_modifiers);
    if (success > 0) {
      create_sustained(ch, vict, spell, force, 0, success, spells[spell].draindamage);
      send_to_char("The world seems to slow down around you as your sense of your surroundings becomes clearer.\r\n", vict);
      act("You successfully sustain that spell on $N.", FALSE, ch, 0, vict, TO_CHAR);
    } else
      send_to_char(FAILED_CAST, ch);
    spell_drain(ch, spell, force, 0);
    break;
  }

}

void cast_health_spell(struct char_data *ch, int spell, int sub, int force, char *arg, struct char_data *mob)
{
  struct char_data *vict = NULL;
  if (mob)
    vict = mob;
  else if (*arg)
    vict = get_char_room_vis(ch, arg);
  if (!check_spell_victim(ch, vict, spell, arg))
    return;
  if (find_duplicate_spell(ch, vict, spell, sub))
    return;

  // Pre-calculate the modifiers to the target (standard modify_target(), altered by spell_bonus()).
  int target_modifiers = modify_target(ch);
  int skill = GET_SKILL(ch, SKILL_SORCERY) + MIN(GET_SKILL(ch, SKILL_SORCERY), GET_CASTING(ch));
  int success = 0;
  int drain = LIGHT;
  spell_bonus(ch, spell, skill, target_modifiers);
  int base_target;

  if (skill == -1)
    return;
  bool cyber = TRUE;
  switch (spell) {
    case SPELL_DETOX:
      base_target = 0;
      if ((GET_DRUG_STAGE(vict) == 1 || GET_DRUG_STAGE(vict) == 2) && GET_DRUG_AFFECT(vict))
        base_target = drug_types[GET_DRUG_AFFECT(vict)].power;
      if (!base_target) {
        send_to_char("They aren't affected by any drugs.\r\n", ch);
        return;
      }
      WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));
      success = success_test(skill, base_target + target_modifiers);
      if (success > 0 && !AFF_FLAGGED(vict, AFF_DETOX)) {
        create_sustained(ch, vict, spell, force, 0, success, spells[SPELL_STABILIZE].draindamage);
        send_to_char("You notice the effects of the drugs suddenly wear off.\r\n", vict);
        act("You successfully sustain that spell on $N.", FALSE, ch, 0, vict, TO_CHAR);
      } else
        send_to_char(FAILED_CAST, ch);
      spell_drain(ch, spell, force, 0);
      break;
    case SPELL_STABILIZE:
      WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));
      success = success_test(skill, 4 + ((GET_LAST_DAMAGETIME(vict) - time(0)) / SECS_PER_MUD_HOUR) + target_modifiers);
      if (success > 0 && force >= (GET_PHYSICAL(vict) <= 0 ? -(GET_PHYSICAL(vict) / 100) : 50)) {
        create_sustained(ch, vict, spell, force, 0, success, spells[SPELL_STABILIZE].draindamage);
        send_to_char("Your condition stabilizes, you manage to grab a thin hold on life.\r\n", vict);
        act("You successfully sustain that spell on $N.", FALSE, ch, 0, vict, TO_CHAR);
      } else
        send_to_char(FAILED_CAST, ch);
      spell_drain(ch, spell, force, 0);
      break;
    case SPELL_RESISTPAIN:
      WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));
      success = success_test(skill, 4 + target_modifiers);
      if (GET_PHYSICAL(vict) <= 0)
        drain = DEADLY;
      else if (GET_PHYSICAL(vict) <= 300)
        drain = SERIOUS;
      else if (GET_PHYSICAL(vict) <= 700)
        drain = MODERATE;
      if (success > 0 && !AFF_FLAGGED(ch, AFF_RESISTPAIN)) {
        create_sustained(ch, vict, spell, force, 0, success, spells[SPELL_RESISTPAIN].draindamage);
        send_to_char("Your pain begins to fade.\r\n", vict);
        act("You successfully sustain that spell on $N.", FALSE, ch, 0, vict, TO_CHAR);
        vict->points.resistpain = MIN(force, success) * 100;
      } else
        send_to_char(FAILED_CAST, ch);
      spell_drain(ch, spell, force, 0);
      break;
    case SPELL_HEALTHYGLOW:
      WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));
      success = success_test(skill, 4 + target_modifiers);
      if (success > 0) {
        create_sustained(ch, vict, spell, force, 0, success, spells[SPELL_HEALTHYGLOW].draindamage);
        send_to_char("You begin to feel healthier and more attractive.\r\n", vict);
        act("You successfully sustain that spell on $N.", FALSE, ch, 0, vict, TO_CHAR);
      } else
        send_to_char(FAILED_CAST, ch);
      spell_drain(ch, spell, force, 0);
      break;
    case SPELL_TREAT:
      if (!AFF_FLAGGED(vict, AFF_DAMAGED)) {
        send_to_char("They are beyond the help of this spell.\r\n", ch);
        return;
      }
      // fall through
    case SPELL_HEAL:
      WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));
      success = MIN(force, success_test(skill, 10 - (int)(GET_ESS(vict) / 100) + target_modifiers + (int)(GET_INDEX(ch) / 200)));
      if (GET_PHYSICAL(vict) <= 0)
        drain = DEADLY;
      else if (GET_PHYSICAL(vict) <= 300)
        drain = SERIOUS;
      else if (GET_PHYSICAL(vict) <= 700)
        drain = MODERATE;
      if (success < 1 || AFF_FLAGGED(vict, AFF_HEALED) || GET_PHYSICAL(vict) == GET_MAX_PHYSICAL(vict)) {
        send_to_char(FAILED_CAST, ch);
      } else {
        AFF_FLAGS(vict).SetBit(AFF_HEALED);
        send_to_char("A warm feeling floods your body.\r\n", vict);
        act("You successfully sustain that spell on $N.", FALSE, ch, 0, vict, TO_CHAR);
        create_sustained(ch, vict, spell, force, 0, success, drain);
        update_pos(vict);
      }
      spell_drain(ch, spell, force, drain);
      break;
    case SPELL_INCREF1:
    case SPELL_INCREF2:
    case SPELL_INCREF3:
      {
        int int_with_just_bioware = GET_REAL_INT(vict);
        int qui_with_just_bioware = GET_REAL_QUI(vict);
        int rea_from_bioware = 0;
        int initiative_dice_with_just_bioware = 0;

        for (struct obj_data *bioware = vict->bioware; bioware; bioware = bioware->next_content)
        {
          // Skip deactivated adrenal pumps.
          if (GET_BIOWARE_TYPE(bioware) == BIO_ADRENALPUMP && GET_OBJ_VAL(bioware, 5) <= 0)
            continue;

          // Apply the int penalty from an activated pain editor.
          if (GET_BIOWARE_TYPE(bioware) == BIO_PAINEDITOR && GET_OBJ_VAL(bioware, 3))
            int_with_just_bioware -= 1;

          // Add any miscellaneous affects to your int / qui / dice.
          for (int j = 0; j < MAX_OBJ_AFFECT; j++) {
            if (bioware->affected[j].location == APPLY_INT) {
              int_with_just_bioware += bioware->affected[j].modifier;
            }
            else if (bioware->affected[j].location == APPLY_QUI) {
              qui_with_just_bioware += bioware->affected[j].modifier;
            }
            else if (bioware->affected[j].location == APPLY_REA) {
              rea_from_bioware += bioware->affected[j].modifier;
            }
            else if (bioware->affected[j].location == APPLY_INITIATIVE_DICE) {
              initiative_dice_with_just_bioware += bioware->affected[j].modifier;
            }
          }
        }

        /*
        send_to_char(ch, "iwjb %d vs %d, qwjb %d vs %d, dwjb %d vs %d\r\n",
                     int_with_just_bioware, GET_INT(vict),
                     qui_with_just_bioware, GET_QUI(vict),
                     initiative_dice_with_just_bioware, GET_INIT_DICE(vict)
                   );
        */

        // If the numbers don't match exactly, refuse to cast.
        if (((((int_with_just_bioware + qui_with_just_bioware) / 2) + rea_from_bioware) != GET_REA(vict))
            || (initiative_dice_with_just_bioware != GET_INIT_DICE(vict)))
        {
          if (ch == vict) {
            send_to_char(ch, "Your reflexes have already been modified, so the increased reflexes spell won't work for you.\r\n");
          } else {
            act("$N's reflexes have already been modified, so you can't cast that spell on $m.", FALSE, ch, 0, vict, TO_CHAR);
          }
          return;
        }
      }
      WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));

      if ((success = success_test(skill, GET_REA(vict) + target_modifiers)) > 0) {
        create_sustained(ch, vict, spell, force, 0, success, spells[spell].draindamage);
        send_to_char("The world slows down around you.\r\n", vict);
        act("You successfully sustain that spell on $N.", FALSE, ch, 0, vict, TO_CHAR);
      } else
        send_to_char(FAILED_CAST, ch);
      spell_drain(ch, spell, force, 0);
      break;
    case SPELL_INCREA:
      sub = REA;
      // fall through
    case SPELL_DECATTR:
    case SPELL_DECCYATTR:
    case SPELL_INCATTR:
    case SPELL_INCCYATTR:
      if (GET_ATT(vict, sub) != GET_REAL_ATT(vict, sub)) {
        if (GET_TRADITION(vict) == TRAD_ADEPT && sub < CHA) {
          switch (sub) {
          case BOD:
            if (BOOST(vict)[BOD][0] || GET_POWER(vict, ADEPT_IMPROVED_BOD))
              cyber = false;
            break;
          case QUI:
            if (BOOST(vict)[QUI][0] || GET_POWER(vict, ADEPT_IMPROVED_QUI))
              cyber = false;
            break;
          case STR:
            if (BOOST(vict)[STR][0] || GET_POWER(vict, ADEPT_IMPROVED_STR))
              cyber = false;
            break;
          }
        } else if (GET_SUSTAINED(vict)) {
          for (struct sustain_data *sus = GET_SUSTAINED(vict); sus; sus = sus->next) {
            if (sus->caster == FALSE && (sus->spell == SPELL_INCATTR || sus->spell == SPELL_DECATTR) && sus->subtype == sub) {
              cyber = false;
              break;
            }
          }
        }
        if (cyber && (spell == SPELL_DECATTR || spell == SPELL_INCATTR || spell == SPELL_INCREA)) {
          snprintf(buf, sizeof(buf), "$N's %s has been modified by technological means and is immune to this spell.\r\n", attributes[sub]);
          act(buf, TRUE, ch, 0, vict, TO_CHAR);
          return;
        }
      }
      if ((spell == SPELL_DECCYATTR || spell == SPELL_INCCYATTR) && (!cyber || GET_ATT(vict, sub) == GET_REAL_ATT(vict, sub))) {
        snprintf(buf, sizeof(buf), "$N's %s has not been modified by technological means and is immune to this spell.\r\n", attributes[sub]);
        act(buf, TRUE, ch, 0, vict, TO_CHAR);
        return;
      }
      WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));
      int target = target_modifiers;
      if (spell == SPELL_INCREA)
        target += GET_REA(vict);
      else if (spell == SPELL_INCATTR || spell == SPELL_INCCYATTR)
        target += GET_ATT(vict, sub);
      else
        target += 10 - (GET_ESS(vict) / 100);
      success = (int)(success_test(skill, target) -
                      ((spell == SPELL_DECATTR || spell == SPELL_DECCYATTR) ? resist_spell(vict, spell, force, sub) : 0));
      if (success > 1) {
        create_sustained(ch, vict, spell, force, sub, success, spells[spell].draindamage);
        act("You successfully sustain that spell on $N.", FALSE, ch, 0, vict, TO_CHAR);
        send_to_char("You feel your body tingle.\r\n", vict);
      } else
        send_to_char(FAILED_CAST, ch);
      spell_drain(ch, spell, force, 0);
      break;
    }
  }

  void cast_illusion_spell(struct char_data *ch, int spell, int force, char *arg, struct char_data *mob)
  {
    struct char_data *vict = NULL;
    bool reflected = FALSE;
    if (mob)
      vict = mob;
    else if (*arg)
      vict = get_char_room_vis(ch, arg);
    if (find_duplicate_spell(ch, vict, spell, 0))
      return;

    // Pre-calculate the modifiers to the target (standard modify_target(), altered by spell_bonus()).
    int target_modifiers = modify_target(ch);
    int skill = GET_SKILL(ch, SKILL_SORCERY) + MIN(GET_SKILL(ch, SKILL_SORCERY), GET_CASTING(ch));
    int success = 0;
    spell_bonus(ch, spell, skill, target_modifiers);
    if (skill == -1)
      return;
    struct char_data *temp = vict;
    switch (spell)
    {
    case SPELL_CONFUSION:
    case SPELL_CHAOS:
      if (!check_spell_victim(ch, vict, spell, arg))
        return;

      check_killer(ch, vict);

      if (!IS_NPC(ch) && !IS_NPC(vict) && PLR_FLAGGED(ch, PLR_KILLER)) {
        act("You have the KILLER flag, so you can't affect $N with a mind-altering spell.", TRUE, ch, 0, vict, TO_CHAR);
        return;
      }
      WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));

      success = success_test(skill, (spell == SPELL_CONFUSION ? GET_WIL(vict) : GET_INT(vict)) + target_modifiers);
      if (success > 0 && GET_REFLECT(vict) && (reflected = reflect_spell(ch, vict, spell, force, 0, (spell == SPELL_CONFUSION ? GET_WIL(ch) : GET_INT(ch)), success))) {
        vict = ch;
        ch = temp;
      }
      success -= resist_spell(vict, spell, force, 0);
      if (success > 0) {
        send_to_char("Coherent thought is suddenly a foreign concept.\r\n", vict);
        act("You successfully sustain that spell on $N.", FALSE, ch, 0, vict, TO_CHAR);
        create_sustained(ch, vict, spell, force, 0, success, spells[spell].draindamage);
      } else
        send_to_char(FAILED_CAST, reflected ? vict : ch);
      spell_drain(reflected ? vict : ch, spell, force, 0);
      break;
    case SPELL_INVIS:
    case SPELL_IMP_INVIS:
      if (!check_spell_victim(ch, vict, spell, arg))
        return;
      WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));

      success = success_test(skill, 4 + target_modifiers);
      if (success > 0) {
        act("You blink and suddenly $n is gone!", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You feel your body tingle.\r\n", vict);
        create_sustained(ch, vict, spell, force, 0, success, spells[spell].draindamage);
      } else
        send_to_char(FAILED_CAST, ch);
      spell_drain(ch, spell, force, 0);
      break;
    case SPELL_STEALTH:
      if (!check_spell_victim(ch, vict, spell, arg))
        return;

      WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));

      success = success_test(skill, 4 + target_modifiers);
      if (success > 0) {
        act("You successfully sustain that spell on $n.", FALSE, vict, 0, ch, TO_VICT);
        send_to_char("Your every move becomes silent.\r\n", vict);
        create_sustained(ch, vict, spell, force, 0, success, spells[spell].draindamage);
      } else
        send_to_char(FAILED_CAST, ch);
      spell_drain(ch, spell, force, 0);
      break;
    case SPELL_SILENCE:
      WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));
      success = success_test(skill, 4 + target_modifiers);
      if (success > 0) {
        act("The room falls silent.", FALSE, ch, 0, 0, TO_ROOM);
        act("The room falls silent.", FALSE, ch, 0, 0, TO_CHAR);
        create_sustained(ch, ch, spell, force, 0, success, spells[spell].draindamage);
      } else
        send_to_char(FAILED_CAST, ch);
      spell_drain(ch, spell, force, 0);
      break;
  }
}

void cast_manipulation_spell(struct char_data *ch, int spell, int force, char *arg, struct char_data *mob)
{
  struct char_data *vict = NULL;
  bool reflected = FALSE;
  int basedamage = 0;
  switch (spell)
  {
  case SPELL_LASER:
  case SPELL_NOVA:
  case SPELL_STEAM:
  case SPELL_SMOKECLOUD:
  case SPELL_THUNDERBOLT:
  case SPELL_THUNDERCLAP:
  case SPELL_WATERBOLT:
  case SPELL_SPLASH:
  case SPELL_ACIDSTREAM:
  case SPELL_TOXICWAVE:
  case SPELL_FLAMETHROWER:
  case SPELL_FIREBALL:
  case SPELL_LIGHTNINGBOLT:
  case SPELL_BALLLIGHTNING:
  case SPELL_CLOUT:
    two_arguments(arg, buf, buf1);
    if (get_ch_in_room(ch)->peaceful) {
      send_to_char("This room just has a peaceful, easy feeling...\r\n", ch);
      return;
    }
    if (!*buf) {
      send_to_char("What damage level do you wish to cast that spell at?\r\n", ch);
      return;
    } else {
      for (basedamage = 0; *wound_name[basedamage] != '\n'; basedamage++)
        if (is_abbrev(buf, wound_name[basedamage]))
          break;
      if (basedamage > 4 || basedamage == 0) {
        send_to_char(ch, "'%s' is not a valid damage level, please choose between Light, Moderate, Serious and Deadly.\r\n", capitalize(buf));
        return;
      }
    }
    if (*buf1)
      vict = get_char_room_vis(ch, buf1);
    if (ch == vict) {
      send_to_char("You can't target yourself with a combat spell!\r\n", ch);
      return;
    }
    break;
  default:
    if (mob)
      vict = mob;
    else if (*arg)
      vict = get_char_room_vis(ch, arg);
  }
  if (find_duplicate_spell(ch, vict, spell, 0))
    return;

  // Pre-calculate the modifiers to the target (standard modify_target(), altered by spell_bonus()).
  int target_modifiers = modify_target(ch);
  int success = 0;
  int skill;
  if (IS_ELEMENTAL(ch) || IS_SPIRIT(ch)) {
    skill = GET_LEVEL(ch);
  } else {
    skill = GET_SKILL(ch, SKILL_SORCERY) + MIN(GET_SKILL(ch, SKILL_SORCERY), GET_CASTING(ch));
    spell_bonus(ch, spell, skill, target_modifiers);
  }

  if (skill == -1)
    return;
  struct char_data *temp = vict;
  switch (spell)
  {
  case SPELL_ARMOR:
    if (!check_spell_victim(ch, vict, spell, arg))
      return;

    WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));
    success = success_test(skill, 6 + target_modifiers);
    if (success > 0) {
      create_sustained(ch, vict, spell, force, 0, success, spells[spell].draindamage);
      send_to_char("You feel your body tingle.\r\n", vict);
      act("You successfully sustain that spell on $N.", FALSE, ch, 0, vict, TO_CHAR);
    } else
      send_to_char(FAILED_CAST, ch);
    spell_drain(ch, spell, force, 0);
    break;
  case SPELL_POLTERGEIST:
    WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));
    success = success_test(skill, 4 + target_modifiers);
    if (success > 0) {
      create_sustained(ch, ch, spell, force, 0, success, spells[spell].draindamage);
      act("An invisible wind begins to spin small objects around the area!", FALSE, ch, 0, 0, TO_ROOM);
      act("An invisible wind begins to spin small objects around the area!", FALSE, ch, 0, 0, TO_CHAR);
    } else
      send_to_char(FAILED_CAST, ch);
    spell_drain(ch, spell, force, 0);
    break;
  case SPELL_LIGHT:
    WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));
    success = success_test(skill, 4 + target_modifiers);
    if (success > 0) {
      create_sustained(ch, ch, spell, force, 0, success, spells[spell].draindamage);
      act("Light radiates from $n, illuminating the area!", FALSE, ch, 0, 0, TO_ROOM);
      act("The area brightens as your light spell takes affect!", FALSE, ch, 0, 0, TO_CHAR);
    } else
      send_to_char(FAILED_CAST, ch);
    spell_drain(ch, spell, force, 0);
    break;
  case SPELL_ICESHEET:
    if (!ch->in_room) {
      send_to_char("You can't create ice in here!\r\n", ch);
      return;
    }
    WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));
    success = success_test(skill, 4 + target_modifiers);
    if (success > 0) {
      ch->in_room->icesheet[0] = (int)(3.14 * ((GET_MAG(ch) * GET_MAG(ch)) / 10000));
      ch->in_room->icesheet[1] = force + MIN(force, success / 2);
      act("The floor is suddenly covered in ice!", FALSE, ch, 0, 0, TO_ROOM);
      act("The floor is suddenly covered in ice!", FALSE, ch, 0, 0, TO_CHAR);
    } else
      send_to_char(FAILED_CAST, ch);
    spell_drain(ch, spell, force, 0);
    break;
  case SPELL_IGNITE:
    if (!check_spell_victim(ch, vict, spell, arg))
      return;
    check_killer(ch, vict);
    if (ch == vict) {
      send_to_char("You can't target yourself with a combat spell!\r\n", ch);
      return;
    }
    if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_KILLER) && !IS_NPC(vict)) {
      act("You have the KILLER flag, so you can't affect $N with ignite.", TRUE, ch, 0, vict, TO_CHAR);
      return;
    }
    SET_WAIT_STATE_AND_COMBAT_STATUS_AFTER_OFFENSIVE_SPELLCAST;

    success = success_test(skill, 4 + target_modifiers);
    if (success > 0 && GET_REFLECT(vict) && (reflected = reflect_spell(ch, vict, spell, force, 0, 4, success))) {
      vict = ch;
      ch = temp;
    }
    success -= resist_spell(vict, spell, force, 0);
    success -= GET_BOD(vict) / 2;

    if (success > 0) {
      send_to_char("You feel the room temperature sharply rise.\r\n", vict);
      act("You succeed in raising the body temperature of $N to dangerous levels.", FALSE, ch, 0, vict, TO_CHAR);
      create_sustained(ch, vict, spell, force, 0, success, MAX(1, 10 / MIN(force, success)));
      if (!FIGHTING(vict) && vict != ch && AWAKE(vict))
        set_fighting(vict, ch);
      if (IS_NPC(vict) && !IS_NPC(ch))
        GET_LASTHIT(vict) = GET_IDNUM(ch);
    } else {
      send_to_char("You feel your body heat slightly then return to normal.\r\n", vict);
      send_to_char("You fail to generate enough heat in your target.\r\n", reflected ? vict : ch);
    }
    spell_drain(reflected ? vict : ch, spell, force, 0);
    break;
  case SPELL_SHADOW:
    WAIT_STATE(ch, (int) (SPELL_WAIT_STATE_TIME));
    success = success_test(skill, 4 + target_modifiers);
    if (success > 0) {
      create_sustained(ch, ch, spell, force, 0, success, spells[spell].draindamage);
      act("Dark shadows fall over the area.", FALSE, ch, 0, 0, TO_ROOM);
      act("Dark shadows fall over the area as your spell takes affect.", FALSE, ch, 0, 0, TO_CHAR);
    } else
      send_to_char(FAILED_CAST, ch);
    spell_drain(ch, spell, force, 0);
    break;
  case SPELL_CLOUT:
    if (!check_spell_victim(ch, vict, spell, arg))
      return;
    check_killer(ch, vict);
    if (!AWAKE(vict))
      target_modifiers -= 2;
    else {
      // Dodge test: You must be awake.
      success -= success_test(GET_DEFENSE(vict) + GET_DEFENSE(vict) ? GET_POWER(vict, ADEPT_SIDESTEP) : 0, 4 + damage_modifier(vict, buf, sizeof(buf)));
    }
    success += success_test(skill, 4 + target_modifiers);

    if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_KILLER) && !IS_NPC(vict)) {
      act("You have the KILLER flag, so you can't affect $N with clout.", TRUE, ch, 0, vict, TO_CHAR);
      return;
    }
    SET_WAIT_STATE_AND_COMBAT_STATUS_AFTER_OFFENSIVE_SPELLCAST;

    send_to_char("You feel a rush of air head towards you!\r\n", vict);

    if (success <= 0) {
      act("Your clout spell harmlessly disperses as $n dodges it.", FALSE, vict, 0, ch, TO_VICT);
      send_to_char("You easily dodge it!\r\n", vict);
      act("$n dodges out of the way of unseen force.", TRUE, vict, 0, ch, TO_NOTVICT);
    } else {
      success -= success_test(GET_BOD(vict) + GET_BODY(vict), force - GET_IMPACT(vict));
      int dam = convert_damage(stage(success, basedamage));
      if (!AWAKE(vict)) {
        act("$n's body recoils as though hit.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You feel a dull thud in the back of your mind.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 0) {
        act("$n is thrown to the ground, unconscious, from an immense invisible force.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("It slams into you with the force of a freight train, knocking you to the ground, unconscious.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 300) {
        act("$n stumbles backwards, almost losing $s footing, as $e is hit by an invisible force.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You stumble backwards, feeling groggy as the air slams into you at full force.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 700) {
        act("$n steps back, and shakes $s head to clear it.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You step back as the force hits you, feeling a little worse for wear.\r\n", vict);
      } else if (dam > 0) {
        act("$n recoils, as if from a light punch.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You feel as though a fist strikes you across the face.\r\n", vict);
      } else {
        send_to_char("It rushes past you, not causing any damage.\r\n", vict);
        act("$n doesn't seem hurt by your clout spell.", FALSE, vict, 0, ch, TO_VICT);
      }
      if (!damage(ch, vict, dam, TYPE_MANIPULATION_SPELL, MENTAL)) {
        // We only care about updating last_hit for non-dead characters.
        if (IS_NPC(vict) && !IS_NPC(ch))
          GET_LASTHIT(vict) = GET_IDNUM(ch);
      }
    }
    spell_drain(ch, spell, force, basedamage);
    break;
  case SPELL_FLAMETHROWER:
    if (!check_spell_victim(ch, vict, spell, arg))
      return;
    check_killer(ch, vict);
    if (!AWAKE(vict))
      target_modifiers -= 2;
    else
      success -= success_test(GET_DEFENSE(vict) + GET_DEFENSE(vict) ? GET_POWER(vict, ADEPT_SIDESTEP) : 0, 4 + damage_modifier(vict, buf, sizeof(buf)));
    if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_KILLER) && !IS_NPC(vict)) {
      act("You have the KILLER flag, so you can't affect $N with flamethrower.", TRUE, ch, 0, vict, TO_CHAR);
      return;
    }
    SET_WAIT_STATE_AND_COMBAT_STATUS_AFTER_OFFENSIVE_SPELLCAST;

    act("$n's hands seem to spontaneously combust as $e directs a stream of flame at $N!", TRUE, ch, 0, vict, TO_ROOM);

    success += success_test(skill, 4 + target_modifiers);
    if (success > 0 && GET_REFLECT(vict) && (reflected = reflect_spell(ch, vict, spell, force, 0, 4, success))) {
      vict = ch;
      ch = temp;
    }
    if (success <= 0) {
      act("$n dodges the flames, which disperse as they past $m.", FALSE, vict, 0, 0, TO_ROOM);
      send_to_char("You easily dodge the flames!\r\n", vict);
    } else {
      success -= success_test(GET_BOD(vict) + GET_BODY(vict) + GET_POWER(ch, ADEPT_TEMPERATURE_TOLERANCE), force - (GET_IMPACT(vict) / 2));
      int dam = convert_damage(stage(success, basedamage));
      if (!AWAKE(vict)) {
        act("$n spasms as the flames hit $m.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You feel a slight burning sensation in the back of your mind.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 0) {
        act("$n is hit full force by the intense flames causing $m to fall to the ground, gurgling.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("The flames burn intensely around you, your last memory before falling unconscious is the hideous pain.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 300) {
        act("Screams as the flames impact $s body, horribly burning $m.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("The flames crash into you, causing you great pain as they horribly burn you.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 700) {
        act("$n cringes as the flames hit, patting at the spots where the flame continues to burn.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("As the flames hit you quickly pat at the spots that continue to burn, causing searing pain.\r\n", vict);
      } else if (dam > 0) {
        act("The flames burst around $m, causing seemingly little damage.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("The flames burst around you causing you slight pain as it burns some of your hair.\r\n", vict);
      } else {
        act("The flames impact $n, but disperse on impact.", FALSE, vict, 0, ch, TO_ROOM);
        send_to_char("The flames rapidly disperse around you, causing only mild discomfort.\r\n", vict);
      }

      if (IS_NPC(ch) || IS_NPC(vict)) {
        damage_equip(ch, vict, force, TYPE_FIRE);
      }
      if (!damage(ch, vict, dam, TYPE_MANIPULATION_SPELL, PHYSICAL)) {
        if (number(0, 6) <= basedamage + 1) {
          act("^RThe flames continue to burn around $m!^N", TRUE, vict, 0, 0, TO_ROOM);
          send_to_char("^RYou continue to burn!\r\n", vict);
          vict->points.fire[0] = srdice();
          vict->points.fire[1] = 0;
        }

        if (IS_NPC(vict) && !IS_NPC(ch))
          GET_LASTHIT(vict) = GET_IDNUM(ch);
      }
    }
    spell_drain(reflected ? vict : ch, spell, force, basedamage);
    break;
  case SPELL_ACIDSTREAM:
    if (!check_spell_victim(ch, vict, spell, arg))
      return;
    check_killer(ch, vict);
    if (!AWAKE(vict))
      target_modifiers -= 2;
    else
      success -= success_test(GET_DEFENSE(vict) + GET_DEFENSE(vict) ? GET_POWER(vict, ADEPT_SIDESTEP) : 0, 4 + damage_modifier(vict, buf, sizeof(buf)));

    if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_KILLER) && !IS_NPC(vict)) {
      act("You have the KILLER flag, so you can't affect $N with acid stream.", TRUE, ch, 0, vict, TO_CHAR);
      return;
    }
    SET_WAIT_STATE_AND_COMBAT_STATUS_AFTER_OFFENSIVE_SPELLCAST;
    act("Dark clouds form around $n moments before it condenses into a dark sludge and flies towards $N!", TRUE, ch, 0, vict, TO_ROOM);

    success += success_test(skill, 4 + target_modifiers);
    // Known bug: Adept sidestep and the defense test don't reply to the reflected spell.
    if (success > 0 && GET_REFLECT(vict) && (reflected = reflect_spell(ch, vict, spell, force, 0, 4, success))) {
      vict = ch;
      ch = temp;
    }
    if (success <= 0) {
      act("$n dodges the acid, which evaporates as it passes $m.", FALSE, vict, 0, 0, TO_ROOM);
      send_to_char("You easily dodge the acid!\r\n", vict);
    } else {
      success -= success_test(GET_BOD(vict) + GET_BODY(vict) + GET_POWER(ch, ADEPT_TEMPERATURE_TOLERANCE), force - (GET_IMPACT(vict) / 2));
      int dam = convert_damage(stage(success, basedamage));
      if (!AWAKE(vict)) {
        act("$n spasms as the acid hits $m.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You feel a slight burning sensation in the back of your mind.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 0) {
        act("As the acid hits $n $e falls to the ground twitching and screaming as $s body smokes.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("The fumes from the acid burning through your body fill your lungs, burning you from the inside out as you fade into unconsciousness.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 300) {
        act("The acid hits $n, $e cries out in pain as trails of smoke come from $s body.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("The acid impacts you with a great force, causing you to step back as it burns through your skin.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 700) {
        act("$n cringes as the acid hits.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("The acid begins to burn your skin as it hits you, causing a bit of pain.\r\n", vict);
      } else if (dam > 0) {
        act("$n is splashed by the acid, causing nothing but mild irritation.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("The acid splashes against you causing a mild burning sensation.\r\n", vict);
      } else {
        act("The acid splashes on $n, but $e doesn't seem to flinch.", FALSE, vict, 0, ch, TO_ROOM);
        send_to_char("You are splashed by the acid, but it causes nothing more than a moment's irritation.\r\n", vict);
      }

      if (IS_NPC(ch) || IS_NPC(vict)) {
        damage_equip(ch, vict, force, TYPE_ACID);
      }
      AFF_FLAGS(vict).SetBit(AFF_ACID);
      if (!damage(ch, vict, dam, TYPE_MANIPULATION_SPELL, PHYSICAL)) {
        if (IS_NPC(vict) && !IS_NPC(ch))
          GET_LASTHIT(vict) = GET_IDNUM(ch);
      }
    }
    spell_drain(reflected ? vict : ch, spell, force, basedamage);
    break;
  case SPELL_LIGHTNINGBOLT:
    if (!check_spell_victim(ch, vict, spell, arg))
      return;
    check_killer(ch, vict);
    if (!AWAKE(vict))
      target_modifiers -= 2;
    else {
      // NOTE: Added sidestep here. Not sure if you should be able to sidestep lightning, but if you can dodge it in the first place...
      success -= success_test(GET_DEFENSE(vict) + GET_DEFENSE(vict) ? GET_POWER(vict, ADEPT_SIDESTEP) : 0, 4 + damage_modifier(vict, buf, sizeof(buf)));
    }
    if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_KILLER) && !IS_NPC(vict)) {
      act("You have the KILLER flag, so you can't affect $N with lightning bolt.", TRUE, ch, 0, vict, TO_CHAR);
      return;
    }
    SET_WAIT_STATE_AND_COMBAT_STATUS_AFTER_OFFENSIVE_SPELLCAST;
    act("Lightning bursts forth from $n and heads directly towards $N!", TRUE, ch, 0, vict, TO_ROOM);

    success += success_test(skill, 4 + target_modifiers);
    // Known bug: Defense test does not apply to reflected spell.
    if (success > 0 && GET_REFLECT(vict) && (reflected = reflect_spell(ch, vict, spell, force, 0, 4, success))) {
      vict = ch;
      ch = temp;
    }
    if (success <= 0) {
      act("$n easily dodges it, and it vanishes to nothing.", FALSE, vict, 0, 0, TO_ROOM);
      send_to_char("You easily dodge it!\r\n", vict);
    } else {
      success -= success_test(GET_BOD(vict) + GET_BODY(vict) + GET_POWER(ch, ADEPT_TEMPERATURE_TOLERANCE), force - (GET_IMPACT(vict) / 2));
      int dam = convert_damage(stage(success, basedamage));
      if (!AWAKE(vict)) {
        act("$n's body goes into convulsions as the lightning flows through it.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You feel a slight burning sensation in the back of your mind.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 0) {
        act("$n is propelled backwards by the force of the lightning bolt, $s body smoking as it lands, not a sign of life from it.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You feel your body begin to spasm as the huge charge of electricity fries your nervous system.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 300) {
        act("$n is almost lifted in the air by the lightning, spasms filling $s body, as a thin trail of smoke rise from $m.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("Your body is filled with pain as the lightning hits you, your limbs going into an uncontrollable seizure.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 700) {
        act("$n spasms as the lightning hits $m, $s body wracked with spasms as the lightning dissipates.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("Pain flashes through your body as the lightning hits, your body wracked with several serious spasms.\r\n", vict);
      } else if (dam > 0) {
        act("$n visibly recoils as the lightning hits, but otherwise seems fine.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You recoil as the lightning hits you, your mind going fuzzy for a moment.\r\n", vict);
      } else {
        act("The lightning hits $n, but seems to be easily absorbed.", FALSE, vict, 0, ch, TO_ROOM);
        send_to_char("Your body absorbs the lightning without harm.\r\n", vict);
      }
      for (int i = 0; i < NUM_WEARS; i++) {
        if (number(0, 1)
            && GET_EQ(vict, i)
            && (GET_OBJ_MATERIAL(GET_EQ(vict, i)) == 10
                || GET_OBJ_MATERIAL(GET_EQ(vict, i)) == 11))
        {
          damage_obj(vict, GET_EQ(vict, i), force, DAMOBJ_LIGHTNING);
        }
      }
      if (!damage(ch, vict, dam, TYPE_MANIPULATION_SPELL, PHYSICAL)) {
        if (IS_NPC(vict) && !IS_NPC(ch))
          GET_LASTHIT(vict) = GET_IDNUM(ch);
      }
    }
    spell_drain(reflected ? vict : ch, spell, force, basedamage);
    break;
  case SPELL_LASER:
    if (!check_spell_victim(ch, vict, spell, arg))
      return;
    check_killer(ch, vict);
    if (!AWAKE(vict))
      target_modifiers -= 2;
    else {
      // NOTE: Added sidestep here. Not sure if you should be able to sidestep lightning, but if you can dodge it in the first place...
      success -= success_test(GET_DEFENSE(vict) + GET_DEFENSE(vict) ? GET_POWER(vict, ADEPT_SIDESTEP) : 0, 4 + damage_modifier(vict, buf, sizeof(buf)));
    }
    if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_KILLER) && !IS_NPC(vict)) {
      act("You have the KILLER flag, so you can't affect $N with your laser.", TRUE, ch, 0, vict, TO_CHAR);
      return;
    }
    SET_WAIT_STATE_AND_COMBAT_STATUS_AFTER_OFFENSIVE_SPELLCAST;
    act("A thin laser beam is emitted from $n and heads directly towards $N!", TRUE, ch, 0, vict, TO_ROOM);

    success += success_test(skill, 4 + target_modifiers);
    // Known bug: Defense test does not apply when reflected.
    if (success > 0 && GET_REFLECT(vict) && (reflected = reflect_spell(ch, vict, spell, force, 0, 4, success))) {
      vict = ch;
      ch = temp;
    }
    if (success <= 0) {
      act("$n easily dodges it, and the beam flickers and vanishes.", FALSE, vict, 0, 0, TO_ROOM);
      send_to_char("You easily dodge it!\r\n", vict);
    } else {
      success -= success_test(GET_BOD(vict) + GET_BODY(vict) + GET_POWER(ch, ADEPT_TEMPERATURE_TOLERANCE), force - (GET_IMPACT(vict) / 2));
      int dam = convert_damage(stage(success, basedamage));
      if (!AWAKE(vict)) {
        act("$n's body recoils in pain as the laser beam finds its mark.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You feel an extremely uncomfortable and painful burning sensation.\r\n", vict);
      } else if (GET_PHYSICAL(vict) - (dam * 100) <= 0) {
        act("$n is cleanly pierced by the laser beam, the hole cut in $s body smoking as they drop like a rock.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You feel your body cleanly pierced by the laser beam, which leaves a smoking hole in your body.\r\n", vict);
      } else if (GET_PHYSICAL(vict) - (dam * 100) <= 300) {
        act("$n is almost pierced by the laser beam, pain filling $s body as a thin wisp of smoke rises from $m.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("Your body is filled with intense pain as the laser beam strikes you, inflicting a cauterized wound.\r\n", vict);
      } else if (GET_PHYSICAL(vict) - (dam * 100) <= 700) {
        act("$n jolts back as the laser strikes $m, $s body shuddering in pain as the laser beam dissipates.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("Pain flashes through your body as the laser strikes, your body shuddering at the intense heat.\r\n", vict);
      } else if (dam > 0) {
        act("$n visibly flinches as the laser connects, but otherwise seems fine.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You flinch as the laser strikes you, feeling an intense heat for a moment.\r\n", vict);
      } else {
        act("The laser beam strikes $n, but seems to be easily absorbed.", FALSE, vict, 0, ch, TO_ROOM);
        send_to_char("Your body absorbs the laser beam without injury.\r\n", vict);
      }
      for (int i = 0; i < NUM_WEARS; i++)
        if (!number(0, 50))
          damage_obj(vict, GET_EQ(vict, i), force, DAMOBJ_PIERCE);
      if (!damage(ch, vict, dam, TYPE_MANIPULATION_SPELL, PHYSICAL)) {
        if (IS_NPC(vict) && !IS_NPC(ch))
          GET_LASTHIT(vict) = GET_IDNUM(ch);
      }
    }
    spell_drain(reflected ? vict : ch, spell, force, basedamage);
    break;
  case SPELL_STEAM:
    if (!check_spell_victim(ch, vict, spell, arg))
      return;
    check_killer(ch, vict);
    if (!AWAKE(vict))
      target_modifiers -= 2;
    else {
      // Dodge test: You must be awake.
      success -= success_test(GET_DEFENSE(vict) + GET_DEFENSE(vict) ? GET_POWER(vict, ADEPT_SIDESTEP) : 0, 4 + damage_modifier(vict, buf, sizeof(buf)));
    }
    success += success_test(skill, 4 + target_modifiers);

    if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_KILLER) && !IS_NPC(vict)) {
      act("You have the KILLER flag, so you can't affect $N with your steam cloud.", TRUE, ch, 0, vict, TO_CHAR);
      return;
    }
    SET_WAIT_STATE_AND_COMBAT_STATUS_AFTER_OFFENSIVE_SPELLCAST;

    send_to_char("You see a cloud of steam vapours rush towards you!\r\n", vict);

    if (success <= 0) {
      act("Your steam spell harmlessly evaporates as $n dodges it.", FALSE, vict, 0, ch, TO_VICT);
      send_to_char("You easily dodge it!\r\n", vict);
      act("$n dodges out of the way of a cloud of steam.", TRUE, vict, 0, ch, TO_NOTVICT);
    } else {
      success -= success_test(GET_BOD(vict) + GET_BODY(vict), force - (GET_IMPACT(vict) / 2));
      int dam = convert_damage(stage(success, basedamage));
      if (!AWAKE(vict)) {
        act("$n's body reels from the blast of steam.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You feel lightheaded from the blast of steam.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 0) {
        act("$n chokes and collapses to the ground, unconscious, from the steam vapours.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("It slams into you and robs the breath from your lungs, choking you into unconsciousness.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 300) {
        act("$n flounders backwards, almost losing $s footing, as $e is choked by the steam cloud.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You flounder backwards, feeling lightheaded as the steam threatens to choke you.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 700) {
        act("$n steps back and coughs a few times to catch their breath.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You step back as the steam hits you, feeling a little short on oxygen.\r\n", vict);
      } else if (dam > 0) {
        act("$n blanches as they're exposed briefly to the steam.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You blanch as the feeling of hot steam washes over your face.\r\n", vict);
      } else {
        send_to_char("It rushes past you, not causing any damage.\r\n", vict);
        act("$n doesn't seem hurt by your steam spell.", FALSE, vict, 0, ch, TO_VICT);
      }
      if (!damage(ch, vict, dam, TYPE_MANIPULATION_SPELL, MENTAL)) {
        if (IS_NPC(vict) && !IS_NPC(ch))
          GET_LASTHIT(vict) = GET_IDNUM(ch);
      }
    }
    spell_drain(ch, spell, force, basedamage);
    break;
  case SPELL_THUNDERBOLT:
    if (!check_spell_victim(ch, vict, spell, arg))
      return;
    check_killer(ch, vict);
    if (!AWAKE(vict))
      target_modifiers -= 2;
    else {
      // Dodge test: You must be awake.
      success -= success_test(GET_DEFENSE(vict) + GET_DEFENSE(vict) ? GET_POWER(vict, ADEPT_SIDESTEP) : 0, 4 + damage_modifier(vict, buf, sizeof(buf)));
    }
    success += success_test(skill, 4 + target_modifiers);

    if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_KILLER) && !IS_NPC(vict)) {
      act("You have the KILLER flag, so you can't affect $N with thunderbolt.", TRUE, ch, 0, vict, TO_CHAR);
      return;
    }
    SET_WAIT_STATE_AND_COMBAT_STATUS_AFTER_OFFENSIVE_SPELLCAST;

    send_to_char("The air around you explodes into a deafening roar!\r\n", vict);

    if (success <= 0) {
      act("Your thunderbolt spell harmlessly echoes as $n dodges it.", FALSE, vict, 0, ch, TO_VICT);
      send_to_char("You easily dodge it!\r\n", vict);
      act("$n dodges out of the way of the concussive blast.", TRUE, vict, 0, ch, TO_NOTVICT);
    } else {
      success -= success_test(GET_BOD(vict) + GET_BODY(vict), force - (GET_IMPACT(vict) / 2));
      int dam = convert_damage(stage(success, basedamage));
      if (!AWAKE(vict)) {
        act("$n's shudders from the concussive blast.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You feel dizzy from the concussive blast.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 0) {
        act("$n is violently thrown to the ground, rendered unconscious from the blast.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("It slams into you and rattles your brain and organs, rendering you unconsciousness.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 300) {
        act("$n is shoved backwards, almost losing $s footing as the blast washes over them.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You're shoved backwards from the intensity of the concussive blast.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 700) {
        act("$n is shoved back and has to shake their head to get their bearings.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You're shoved back as the concussive blast hits you, feeling a bit shaken up.\r\n", vict);
      } else if (dam > 0) {
        act("$n flinches as the blast passes by them.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You flinch as the blast passes by you.\r\n", vict);
      } else {
        send_to_char("It rolls past you, not causing any damage.\r\n", vict);
        act("$n doesn't seem hurt by your thunderbolt spell.", FALSE, vict, 0, ch, TO_VICT);
      }
      if (!damage(ch, vict, dam, TYPE_MANIPULATION_SPELL, MENTAL)) {
        if (IS_NPC(vict) && !IS_NPC(ch))
          GET_LASTHIT(vict) = GET_IDNUM(ch);
      }
    }
    spell_drain(ch, spell, force, basedamage);
    break;
  case SPELL_WATERBOLT:
    if (!check_spell_victim(ch, vict, spell, arg))
      return;
    check_killer(ch, vict);
    if (!AWAKE(vict))
      target_modifiers -= 2;
    else {
      // Dodge test: You must be awake.
      success -= success_test(GET_DEFENSE(vict) + GET_DEFENSE(vict) ? GET_POWER(vict, ADEPT_SIDESTEP) : 0, 4 + damage_modifier(vict, buf, sizeof(buf)));
    }
    success += success_test(skill, 4 + target_modifiers);

    if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_KILLER) && !IS_NPC(vict)) {
      act("You have the KILLER flag, so you can't affect $N with waterbolt.", TRUE, ch, 0, vict, TO_CHAR);
      return;
    }
    SET_WAIT_STATE_AND_COMBAT_STATUS_AFTER_OFFENSIVE_SPELLCAST;

    send_to_char("You see a bolt of water head towards you!\r\n", vict);

    if (success <= 0) {
      act("Your waterbolt spell harmlessly evaporates as $n dodges it.", FALSE, vict, 0, ch, TO_VICT);
      send_to_char("You easily dodge it!\r\n", vict);
      act("$n dodges out of the way of the bolt of water.", TRUE, vict, 0, ch, TO_NOTVICT);
    } else {
      success -= success_test(GET_BOD(vict) + GET_BODY(vict), force - (GET_IMPACT(vict) / 2));
      int dam = convert_damage(stage(success, basedamage));
      if (!AWAKE(vict)) {
        act("$n's stumbles as they're splashed with water.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You stumble back as the bolt of water connects.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 0) {
        act("$n is knocked to the ground, unconscious, from the force of the rushing water.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("It slams into you like a sack of bricks, knocking you to the ground, unconscious.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 300) {
        act("$n stumbles backwards, almost losing $s footing, as $e is hit by the bolt of water.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You stumble backwards, absolutely drenched as the water slams into you at full force.\r\n", vict);
      } else if (GET_MENTAL(vict) - (dam * 100) <= 700) {
        act("$n steps back, and shakes $s limbs to dry off.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You step back as the water hits you, feeling like you swam a mile.\r\n", vict);
      } else if (dam > 0) {
        act("$n stumbles, knocked off balance by the water bolt.", TRUE, vict, 0, 0, TO_ROOM);
        send_to_char("You feel the water bolt strike you and splash across your face.\r\n", vict);
      } else {
        send_to_char("It rushes past you, not causing any damage.\r\n", vict);
        act("$n doesn't seem hurt by your waterbolt spell.", FALSE, vict, 0, ch, TO_VICT);
      }
      if (!damage(ch, vict, dam, TYPE_MANIPULATION_SPELL, MENTAL)) {
        if (IS_NPC(vict) && !IS_NPC(ch))
          GET_LASTHIT(vict) = GET_IDNUM(ch);
      }
    }
    spell_drain(ch, spell, force, basedamage);
    break;
  }
}

void cast_spell(struct char_data *ch, int spell, int sub, int force, char *arg)
{
  if (spells[spell].duration == SUSTAINED)
  {
    if (GET_SUSTAINED_NUM(ch) >= GET_SKILL(ch, SKILL_SORCERY)) {
      send_to_char("You cannot sustain any more spells.\r\n", ch);
      return;
    }
  }
  switch (spells[spell].category)
  {
  case COMBAT:
    cast_combat_spell(ch, spell, force, arg);
    break;
  case DETECTION:
    cast_detection_spell(ch, spell, force, arg, NULL);
    break;
  case HEALTH:
    cast_health_spell(ch, spell, sub, force, arg, NULL);
    break;
  case ILLUSION:
    cast_illusion_spell(ch, spell, force, arg, NULL);
    break;
  case MANIPULATION:
    cast_manipulation_spell(ch, spell, force, arg, NULL);
    break;
  }
}

void mob_magic(struct char_data *ch)
{
  if (!FIGHTING(ch))
    return;
  char buf[MAX_STRING_LENGTH];
  int spell = 0, sub = 0, force, magic = GET_MAG(ch) / 100;
  if (GET_WIL(ch) <= 2)
    force = magic;
  else force = MIN(magic, number(1, 8));
  while (!spell) {
    switch (number (0, 12)) {
      case 0:
        spell = SPELL_POWERBOLT;
        break;
      case 1:
        spell = SPELL_MANABOLT;
        break;
      case 2:
        spell = SPELL_STUNBOLT;
        break;
      case 3:
        spell = SPELL_DECCYATTR;
        sub = number(0, 2);
        break;
      case 4:
        spell = SPELL_DECATTR;
        sub = number(0, 2);
        break;
      case 5:
        if (!affected_by_spell(FIGHTING(ch), SPELL_CONFUSION))
          spell = SPELL_CONFUSION;
        break;
      case 6:
        spell = SPELL_FLAMETHROWER;
        break;
      case 7:
        spell = SPELL_ACIDSTREAM;
        break;
      case 8:
        spell = SPELL_LIGHTNINGBOLT;
        break;
      case 9:
        spell = SPELL_CLOUT;
        break;
      case 10:
/*        if (!affected_by_spell(ch, SPELL_POLTERGEIST))
        spell = SPELL_POLTERGEIST;
        break;
*/      case 11:
        if (!get_ch_in_room(ch)->icesheet[1])
          spell = SPELL_ICESHEET;
        break;
      case 12:
        if (!affected_by_spell(FIGHTING(ch), SPELL_IGNITE) && !ch->points.fire[0])
          spell = SPELL_IGNITE;
        break;
      case 13:
        spell = SPELL_LASER;
        break;
      case 14:
        spell = SPELL_STEAM;
        break;
      case 15:
        spell = SPELL_THUNDERBOLT;
        break;
      case 16:
        spell = SPELL_WATERBOLT;
        break;
    }
  }
  switch (spell) {
    case SPELL_FLAMETHROWER:
    case SPELL_MANABOLT:
    case SPELL_STUNBOLT:
    case SPELL_POWERBOLT:
    case SPELL_ACIDSTREAM:
    case SPELL_LIGHTNINGBOLT:
    case SPELL_CLOUT:
    case SPELL_LASER:
    case SPELL_STEAM:
    case SPELL_THUNDERBOLT:
    case SPELL_WATERBOLT:
      snprintf(buf, sizeof(buf), "%s %s", wound_name[number(1, 4)], GET_CHAR_NAME(FIGHTING(ch)));
      break;
    default:
      strcpy(buf, GET_CHAR_NAME(FIGHTING(ch)));
  }
  cast_spell(ch, spell, sub, force, buf);
}


bool check_spirit_sector(struct room_data *room, int spirit)
{
  if ((spirit == SPIRIT_WIND || spirit == SPIRIT_MIST || spirit == SPIRIT_STORM)) {
    if (SECT(room) == SPIRIT_HEARTH || SECT(room) == SPIRIT_FOREST || ROOM_FLAGGED(room, ROOM_INDOORS))
      return FALSE;
  } else if (SECT(room) != spirit)
    return FALSE;
  return TRUE;
}

void circle_build(struct char_data *ch, char *type, int force)
{
  long cost = force * force;
  if (IS_WORKING(ch))
  {
    send_to_char(TOOBUSY, ch);
    return;
  }
  if (GET_TRADITION(ch) != TRAD_HERMETIC || GET_ASPECT(ch) == ASPECT_SORCERER)
  {
    send_to_char("Only hermetic mages need to construct a hermetic circle.\r\n", ch);
    return;
  }
  if (ch->in_veh) {
    send_to_char("You can't build a circle in a vehicle.\r\n", ch);
     return;
  }
  if (GET_NUYEN(ch) < cost)
  {
    send_to_char(ch, "You need %d nuyen for the materials needed to construct that circle.\r\n", force * force);
    return;
  }
  int element = 0;
  for (;element < NUM_ELEMENTS; element++)
    if (is_abbrev(type, elements[element].name))
      break;
  if (element == NUM_ELEMENTS)
  {
    send_to_char("What element do you wish to dedicate this circle to?\r\n", ch);
    return;
  }
  lose_nuyen(ch, cost, NUYEN_OUTFLOW_LODGE_AND_CIRCLE);
  struct obj_data *obj = read_object(OBJ_HERMETIC_CIRCLE, VIRTUAL);
  GET_OBJ_VAL(obj, 1) = force;
  GET_OBJ_VAL(obj, 2) = element;
  GET_OBJ_VAL(obj, 3) = GET_IDNUM(ch);
  if (GET_LEVEL(ch) > 1) {
    send_to_char("You use your staff powers to greatly accelerate the artistic process.\r\n", ch);
    GET_OBJ_VAL(obj, 9) = 1;
  } else
    GET_OBJ_VAL(obj, 9) = force * 60;
  AFF_FLAGS(ch).SetBit(AFF_CIRCLE);
  GET_BUILDING(ch) = obj;
  obj_to_room(obj, ch->in_room);
  send_to_char("You begin to draw a hermetic circle.\r\n", ch);
  act("$n begins to draw a hermetic circle.\r\n", FALSE, ch, 0, 0, TO_ROOM);
}

void lodge_build(struct char_data *ch, int force)
{
  int cost = force * 500;
  if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }
  if (GET_TRADITION(ch) != TRAD_SHAMANIC) {
    send_to_char("Only shamans need to build a lodge.\r\n", ch);
    return;
  }
  if (ch->in_veh) {
    send_to_char("You can't build a lodge in a vehicle.\r\n", ch);
     return;
  }
  if (GET_NUYEN(ch) < cost) {
    send_to_char(ch, "You need %d nuyen worth of materials to construct that lodge.\r\n", force * 500);
    return;
  }
  if (force > GET_REAL_MAG(ch) / 100) {
    send_to_char("You can't create a lodge higher than your magic rating.\r\n", ch);
    return;
  }
  lose_nuyen(ch, cost, NUYEN_OUTFLOW_LODGE_AND_CIRCLE);
  struct obj_data *obj = read_object(OBJ_SHAMANIC_LODGE, VIRTUAL);
  GET_OBJ_VAL(obj, 1) = force;
  GET_OBJ_VAL(obj, 2) = GET_TOTEM(ch);
  GET_OBJ_VAL(obj, 3) = GET_IDNUM(ch);
  if (GET_LEVEL(ch) > 1) {
    send_to_char("You use your staff powers to greatly accelerate the construction process.\r\n", ch);
    GET_OBJ_VAL(obj, 9) = 1;
  } else
    GET_OBJ_VAL(obj, 9) = force * 60 * 5;
  AFF_FLAGS(ch).SetBit(AFF_LODGE);
  GET_BUILDING(ch) = obj;
  obj_to_room(obj, ch->in_room);
  send_to_char(ch, "You begin to build a lodge to %s.\r\n", totem_types[GET_TOTEM(ch)]);
  act("$n begins to build a lodge.\r\n", FALSE, ch, 0, 0, TO_ROOM);
}

struct char_data *create_elemental(struct char_data *ch, int type, int force, int idnum, int trad)
{
  struct char_data *mob;
  if (trad == TRAD_HERMETIC)
    mob = read_mobile(elements[type].vnum, VIRTUAL);
  else
    mob = read_mobile(spirits[type].vnum, VIRTUAL);
  GET_REAL_BOD(mob) = force;
  GET_REAL_QUI(mob) = force;
  GET_REAL_STR(mob) = force;
  GET_REAL_CHA(mob) = force;
  GET_REAL_INT(mob) = force;
  GET_REAL_WIL(mob) = force;
  GET_ESS(mob) = force * 100;
  GET_LEVEL(mob) = force;
  GET_ASTRAL(mob) = (sh_int) force * 1.5;
  GET_ACTIVE(mob) = GET_IDNUM(ch);
  GET_SPARE1(mob) = type;
  GET_SPARE2(mob) = force;
  GET_GRADE(mob) = idnum;
  if (trad == TRAD_HERMETIC)
    switch (type)
    {
    case ELEM_EARTH:
      GET_REAL_BOD(mob) += 4;
      GET_REAL_QUI(mob) -= 2;
      GET_REAL_STR(mob) += 4;
      break;
    case ELEM_FIRE:
      GET_REAL_BOD(mob)++;
      GET_REAL_QUI(mob) += 2;
      GET_REAL_STR(mob) -= 2;
      break;
    case ELEM_AIR:
      GET_REAL_BOD(mob) -= 2;
      GET_REAL_QUI(mob) += 3;
      GET_REAL_STR(mob) -= 3;
      break;
    case ELEM_WATER:
      GET_REAL_BOD(mob) += 2;
      break;
    }
  else
    switch (type)
    {
    case SPIRIT_CITY:
    case SPIRIT_FIELD:
    case SPIRIT_HEARTH:
      GET_REAL_BOD(mob)++;
      GET_REAL_QUI(mob) += 2;
      GET_REAL_STR(mob) -= 2;
      break;
    case SPIRIT_DESERT:
    case SPIRIT_FOREST:
    case SPIRIT_MOUNTAIN:
    case SPIRIT_PRAIRIE:
      GET_REAL_BOD(mob) += 4;
      GET_REAL_QUI(mob) -= 2;
      GET_REAL_STR(mob) += 4;
      break;
    case SPIRIT_MIST:
    case SPIRIT_STORM:
    case SPIRIT_WIND:
      GET_REAL_BOD(mob) -= 2;
      GET_REAL_QUI(mob) += 3;
      GET_REAL_STR(mob) -= 3;
      break;
    case SPIRIT_LAKE:
    case SPIRIT_RIVER:
    case SPIRIT_SEA:
    case SPIRIT_SWAMP:
      GET_REAL_BOD(mob) += 2;
      break;
    }
  char_to_room(mob, get_ch_in_room(ch));
  add_follower(mob, ch);
  affect_total(mob);
  return mob;
}

ACMD(do_contest)
{
  if (GET_ASPECT(ch) == ASPECT_SORCERER || GET_TRADITION(ch) == TRAD_ADEPT || GET_TRADITION(ch) == TRAD_MUNDANE || !GET_SKILL(ch, SKILL_CONJURING)) {
    send_to_char("You don't have the ability to do that.\r\n", ch);
    return;
  }
  struct char_data *mob, *caster = NULL;
  skip_spaces(&argument);
  if (!(mob = get_char_room_vis(ch, argument))) {
    send_to_char(ch, "Contest the binding of which %s?\r\n", GET_TRADITION(ch) ? "nature spirit" : "elemental");
    return;
  }
  if (GET_CHA(ch) <= GET_NUM_SPIRITS(ch)) {
    send_to_char(ch, "You cannot have any more %ss bound to you.\r\n", GET_TRADITION(ch) ? "nature spirit" : "elemental");
    return;
  }
  if (GET_TRADITION(ch) == TRAD_SHAMANIC && (GET_MOB_VNUM(mob) < 29 || GET_MOB_VNUM(mob) > 42)) {
    send_to_char("You can only contest the binding of a nature spirit.\r\n", ch);
    return;
  } else if (GET_TRADITION(ch) == TRAD_HERMETIC && (GET_MOB_VNUM(mob) < 25 || GET_MOB_VNUM(mob) > 28)) {
    send_to_char("You can only contest the binding of an elemental.\r\n", ch);
    return;
  }
  if (GET_ACTIVE(mob) == GET_IDNUM(ch)) {
    send_to_char(ch, "You already have that %s bound to you!\r\n", GET_TRADITION(ch) ? "nature spirit" : "elemental");
    return;
  }
  if (!GET_ACTIVE(mob)) {
    send_to_char("You can't bind a free spirit!\r\n", ch);
    return;
  }
  for (struct descriptor_data *d = descriptor_list; d; d = d->next)
    if (d->character && GET_IDNUM(d->character) == GET_ACTIVE(mob)) {
      caster = d->character;
      break;
    }
  if (!caster) {
    send_to_char("You fain to gain control!\r\n", ch);
    return;
  }
  int chskill = GET_SKILL(ch, SKILL_CONJURING), caskill = GET_SKILL(ch, SKILL_CONJURING) + GET_CHA(ch);
  for (int i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(ch, i)
        && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_FOCUS
        && GET_FOCUS_TYPE(GET_EQ(ch, i)) == FOCI_SPIRIT
        && focus_is_usable_by_ch(GET_EQ(ch, i), ch)
        && GET_OBJ_VAL(GET_EQ(ch, i), 3) == GET_SPARE1(mob)
        && GET_OBJ_VAL(GET_EQ(ch, i), 4)) {
      chskill += GET_OBJ_VAL(GET_EQ(ch, i), 1);
      break;
    }
  for (int i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(caster, i)
        && GET_OBJ_TYPE(GET_EQ(caster, i)) == ITEM_FOCUS
        && GET_FOCUS_TYPE(GET_EQ(caster, i)) == FOCI_SPIRIT
        && focus_is_usable_by_ch(GET_EQ(caster, i), caster)
        && GET_OBJ_VAL(GET_EQ(caster, i), 3) == GET_SPARE1(mob)
        && GET_OBJ_VAL(GET_EQ(caster, i), 4)) {
      caskill += GET_OBJ_VAL(GET_EQ(caster, i), 1);
      break;
    }

  int chsuc = success_test(chskill, GET_LEVEL(mob));
  int casuc = success_test(caskill, GET_LEVEL(mob));
  struct spirit_data *temp;
  if (chsuc < 1 && casuc < 1) {
    for (struct spirit_data *sdata = GET_SPIRIT(caster); sdata; sdata = sdata->next)
      if (sdata->id == GET_GRADE(mob)) {
        REMOVE_FROM_LIST(sdata, GET_SPIRIT(caster), next);
        delete sdata;
        break;
      }
    GET_ELEMENTALS_DIRTY_BIT(ch) = TRUE;
    if (GET_MOB_VNUM(mob) < 25 || GET_MOB_VNUM(mob) > 28) {
      act("$n senses an opportunity and vanishes!", TRUE, mob, 0, 0, TO_ROOM);
      extract_char(mob);
    } else {
      MOB_FLAGS(mob).SetBit(MOB_AGGRESSIVE);
      act("$n becomes uncontrolled!", TRUE, mob, 0, 0, TO_ROOM);
      GET_ACTIVE(mob) = 0;
    }
    conjuring_drain(caster, GET_LEVEL(mob));
    conjuring_drain(ch, GET_LEVEL(mob));
  } else if (chsuc > casuc) {
    send_to_char(ch, "You steal control of %s!\r\n", GET_NAME(mob));
    snprintf(buf, sizeof(buf), "$n steals control of %s!", GET_NAME(mob));
    act(buf, FALSE, ch, 0, caster, TO_VICT);
    for (struct spirit_data *sdata = GET_SPIRIT(caster); sdata; sdata = sdata->next)
      if (sdata->id == GET_GRADE(mob)) {
        REMOVE_FROM_LIST(sdata, GET_SPIRIT(caster), next);
        sdata->services = chsuc - casuc;
        sdata->next = GET_SPIRIT(ch);
        GET_SPIRIT(ch) = sdata->next;
        GET_NUM_SPIRITS(ch)++;
        GET_NUM_SPIRITS(caster)--;
        GET_ELEMENTALS_DIRTY_BIT(ch) = TRUE;
        GET_ELEMENTALS_DIRTY_BIT(caster) = TRUE;
        break;
      }
    GET_ACTIVE(mob) = GET_IDNUM(ch);
    conjuring_drain(caster, GET_LEVEL(mob));
    conjuring_drain(ch, GET_LEVEL(mob));
  } else {
    send_to_char("You fail to gain control!\r\n", ch);
    snprintf(buf, sizeof(buf), "$n tries to steal control of %s!", GET_NAME(mob));
    act(buf, FALSE, ch, 0, caster, TO_VICT);
    conjuring_drain(ch, GET_LEVEL(mob));
  }
}
ACMD(do_unbond)
{
  if (GET_TRADITION(ch) == TRAD_MUNDANE) {
    send_to_char("You can't unbond something with no sense of the astral plane.\r\n", ch);
    return;
  }
  struct obj_data *obj = NULL;
  struct char_data *vict;
  if (!argument || !*argument) {
    send_to_char("You need to specify something to unbond.\r\n", ch);
    return;
  }
  if (!generic_find(argument,  FIND_OBJ_INV | FIND_OBJ_EQUIP, ch, &vict, &obj)) {
    send_to_char(ch, "You don't have a '%s'.\r\n", argument);
    return;
  }
  if (GET_OBJ_TYPE(obj) == ITEM_FOCUS && GET_FOCUS_BONDED_TO(obj)) {
    send_to_char("You sever the focus's bond with the astral plane.\r\n", ch);
    GET_FOCUS_BONDED_TO(obj) = GET_FOCUS_BONDED_SPIRIT_OR_SPELL(obj) = GET_FOCUS_TRADITION(obj) = 0;
  } else if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && WEAPON_IS_FOCUS(obj) && WEAPON_FOCUS_USABLE_BY(obj, ch)) {
    send_to_char("You sever the weapon focus's bond with the astral plane.\r\n", ch);
    GET_WEAPON_FOCUS_BONDED_BY(obj) = GET_WEAPON_FOCUS_BOND_STATUS(obj) = 0;
  } else
    send_to_char(ch, "You can't unbond %s.\r\n", GET_OBJ_NAME(obj));
}

ACMD(do_bond)
{
  if (!*argument) {
    send_to_char("What do you want to bond?\r\n", ch);
    return;
  }
  half_chop(argument, buf1, buf2);
  struct obj_data *obj;
  int karma = 0, spirit = 0;
  struct spell_data *spell = GET_SPELLS(ch);

  if (!argument || !*argument) {
    send_to_char("You need to specify something to bond.\r\n", ch);
    return;
  }

  // Find the object in their inventory or equipment.
  struct char_data *found_char = NULL;
  generic_find(buf1, FIND_OBJ_INV | FIND_OBJ_EQUIP, ch, &found_char, &obj);

  // No object-- failure case.
  if (!obj) {
    send_to_char(ch, "You don't have a '%s'.\r\n", buf1);
    return;
  }

  if (GET_OBJ_TYPE(obj) == ITEM_DOCWAGON) {
    if (GET_DOCWAGON_BONDED_IDNUM(obj)) {
      if (GET_DOCWAGON_BONDED_IDNUM(obj) == GET_IDNUM(ch))
        act("You have already activated $p.", FALSE, ch, obj, 0, TO_CHAR);
      else
        act("$p has already been activated by someone else.", FALSE, ch, obj, 0, TO_CHAR);
      return;
    }
    GET_DOCWAGON_BONDED_IDNUM(obj) = GET_IDNUM(ch);
    act("$p's lights begin to subtly flash in a rhythmic sequence.", FALSE,
        ch, obj, 0, TO_CHAR);
    return;
  }

  else if (GET_OBJ_TYPE(obj) == ITEM_WEAPON
           && !IS_GUN(GET_WEAPON_ATTACK_TYPE(obj))
           && GET_WEAPON_FOCUS_RATING(obj) > 0)
  {
    if (GET_TRADITION(ch) == TRAD_MUNDANE) {
      send_to_char("Mundanes can't bond weapon foci.\r\n", ch);
      return;
    }
    if (GET_WEAPON_FOCUS_BONDED_BY(obj) > 0) {
      send_to_char(ch, "%s is already bonded to %s.",
                   capitalize(GET_OBJ_NAME(obj)),
                   GET_WEAPON_FOCUS_BONDED_BY(obj) == GET_IDNUM(ch) ? "you" : "someone else");
      return;
    }
    if (((GET_MAG(ch) * 2) / 100) < GET_WEAPON_FOCUS_RATING(obj)) {
      send_to_char(ch, "%s is too powerful for you to bond! You need at least %d magic to bond it, and you have %d.\r\n", capitalize(GET_OBJ_NAME(obj)), (int) ((GET_WEAPON_FOCUS_RATING(obj) + 1) / 2));
      return;
    }

    if (IS_WORKING(ch)) {
      send_to_char(TOOBUSY, ch);
      return;
    }
    if (GET_POS(ch) > POS_SITTING) {
      send_to_char("You must be sitting to perform a bonding ritual.\r\n", ch);
      return;
    }

    if (GET_WEAPON_FOCUS_BOND_STATUS(obj) > 0) {
      GET_WEAPON_FOCUS_BOND_STATUS(obj) = GET_WEAPON_FOCUS_RATING(obj) * 60;
      send_to_char(ch, "You restart the ritual to bond %s.\r\n", GET_OBJ_NAME(obj));
      act("$n begins a ritual to bond $p.", TRUE, ch, obj, 0, TO_ROOM);
      AFF_FLAGS(ch).SetBit(AFF_BONDING);
      ch->char_specials.programming = obj;
      return;
    }

    karma = (3 + GET_WEAPON_REACH(obj)) * GET_WEAPON_FOCUS_RATING(obj);

    if (GET_KARMA(ch) < karma * 100) {
      send_to_char(ch, "You don't have enough karma to bond that (Need %d).\r\n", karma);
      return;
    }
    GET_KARMA(ch) -= karma * 100;
    GET_WEAPON_FOCUS_BOND_STATUS(obj) = GET_WEAPON_FOCUS_RATING(obj) * 60;
    GET_WEAPON_FOCUS_BONDED_BY(obj) = GET_IDNUM(ch);

    if (access_level(ch, LVL_BUILDER)) {
      send_to_char(ch, "You use your staff powers to greatly accelerate the bonding ritual.\r\n");
      GET_WEAPON_FOCUS_BOND_STATUS(obj) = 1;
    }

    send_to_char(ch, "You begin the ritual to bond %s.\r\n", GET_OBJ_NAME(obj));
    act("$n begins a ritual to bond $p.", TRUE, ch, obj, 0, TO_ROOM);
    AFF_FLAGS(ch).SetBit(AFF_BONDING);
    ch->char_specials.programming = obj;
    return;
  }

  else if (GET_OBJ_TYPE(obj) == ITEM_FOCUS) {
    if (GET_TRADITION(ch) == TRAD_MUNDANE)
      send_to_char(ch, "You can't bond foci.\r\n");
    else if (GET_TRADITION(ch) == TRAD_ADEPT)
      send_to_char("Adepts can only bond weapon foci.\r\n", ch);
    else if (GET_FOCUS_BONDED_TO(obj) && GET_FOCUS_BONDED_TO(obj) != GET_IDNUM(ch))
      send_to_char(ch, "%s is already bonded to someone else.\r\n", capitalize(GET_OBJ_NAME(obj)));
    else if (IS_WORKING(ch))
      send_to_char(TOOBUSY, ch);
    else if (GET_POS(ch) > POS_SITTING)
      send_to_char("You must be sitting to perform a bonding ritual.\r\n", ch);
    else if (GET_FOCUS_BONDED_TO(obj) == GET_IDNUM(ch)) {
      if (GET_FOCUS_BOND_TIME_REMAINING(obj)) {
        GET_FOCUS_BOND_TIME_REMAINING(obj) = GET_FOCUS_FORCE(obj) * 60;
        send_to_char(ch, "You restart the ritual to bond %s.\r\n", GET_OBJ_NAME(obj));
        act("$n begins a ritual to bond $p.", TRUE, ch, obj, 0, TO_ROOM);
        AFF_FLAGS(ch).SetBit(AFF_BONDING);
        ch->char_specials.programming = obj;
      } else
        send_to_char("You have already bonded this focus.\r\n", ch);
    } else {
      switch (GET_FOCUS_TYPE(obj)) {
        case FOCI_SPEC_SPELL:
          karma = GET_FOCUS_FORCE(obj);
          break;
        case FOCI_EXPENDABLE:
        case FOCI_SPELL_CAT:
          if (!*buf2) {
            send_to_char("Bond which spell category?\r\n", ch);
            return;
          }
          for (; spirit <= MANIPULATION; spirit++)
            if (is_abbrev(buf2, spell_category[spirit]))
              break;
          if (spirit > MANIPULATION) {
            send_to_char("That is not a valid category.\r\n", ch);
            return;
          }
          if (GET_FOCUS_TYPE(obj) == FOCI_SPELL_CAT)
            karma = GET_FOCUS_FORCE(obj) * 3;
          break;
        case FOCI_SPIRIT:
          if (!*buf2) {
            send_to_char(ch, "Bond which %s type?\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
            return;
          }
          if (GET_TRADITION(ch) == TRAD_HERMETIC) {
            for (; spirit < NUM_ELEMENTS; spirit++)
              if (is_abbrev(buf2, elements[spirit].name))
                break;
          } else
            for (; spirit < NUM_SPIRITS; spirit++)
              if (is_abbrev(buf2, spirits[spirit].name))
                break;
          if (GET_TRADITION(ch) == TRAD_HERMETIC ? spirit == NUM_ELEMENTS : spirit == NUM_SPIRITS) {
            send_to_char(ch, "That is not a valid %s.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
            return;
          }
          karma = GET_FOCUS_FORCE(obj) * 2;
          break;
        case FOCI_POWER:
          karma = GET_FOCUS_FORCE(obj) * 5;
          break;
        case FOCI_SUSTAINED:
          karma = GET_FOCUS_FORCE(obj);
          break;
        case FOCI_SPELL_DEFENSE:
          karma = GET_FOCUS_FORCE(obj) * 3;
          break;
        default:
          snprintf(buf, sizeof(buf), "SYSERR: Unknown focus type %d in bonding switch.", GET_FOCUS_TYPE(obj));
          mudlog(buf, ch, LOG_SYSLOG, TRUE);
          karma = 10000000;
          break;
      }
      if (GET_OBJ_VAL(obj, 0) == FOCI_SUSTAINED || GET_OBJ_VAL(obj, 0) == FOCI_SPEC_SPELL) {
        for (;spell; spell = spell->next)
          if (is_abbrev(buf2, spell->name)) {
            spirit = spell->type;
            break;
          }
        if (!spell) {
          if (*buf2)
            send_to_char(ch, "You don't know a spell called '%s' to bond.\r\n", buf2);
          else
            send_to_char(ch, "You need to specify the spell you want to bond %s for.\r\n", GET_OBJ_NAME(obj));
          return;
        }
        if (GET_FOCUS_TYPE(obj) == FOCI_SUSTAINED && spells[spirit].duration != SUSTAINED) {
          send_to_char("You don't need to bond a sustaining focus for this spell.\r\n", ch);
          return;
        }
      }
      if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
        if (GET_FORCE_POINTS(ch) < karma) {
          send_to_char(ch, "You don't have enough force points to bond that (Need %d). You can get more force points by returning to the talismonger or the spell trainers and typing ^WLEARN FORCE^n.\r\n", karma);
          return;
        }
        GET_FORCE_POINTS(ch) -= karma;
        GET_FOCUS_BOND_TIME_REMAINING(obj) = 1;
      } else {
        if (GET_KARMA(ch) < karma * 100) {
          send_to_char(ch, "You don't have enough karma to bond that (Need %d).\r\n", karma);
          return;
        }
        GET_KARMA(ch) -= karma * 100;
        GET_FOCUS_BOND_TIME_REMAINING(obj) = GET_FOCUS_FORCE(obj) * 60;
      }
      GET_FOCUS_BONDED_TO(obj) = GET_IDNUM(ch);
      GET_FOCUS_BONDED_SPIRIT_OR_SPELL(obj) = spirit;
      GET_FOCUS_TRADITION(obj) = GET_TRADITION(ch) == TRAD_HERMETIC ? 1 : 0;

      if (access_level(ch, LVL_BUILDER)) {
        send_to_char(ch, "You use your staff powers to greatly accelerate the bonding ritual.\r\n");
        GET_FOCUS_BOND_TIME_REMAINING(obj) = 1;
      }

      send_to_char(ch, "You begin the ritual to bond %s.\r\n", GET_OBJ_NAME(obj));
      act("$n begins a ritual to bond $p.", TRUE, ch, obj, 0, TO_ROOM);
      AFF_FLAGS(ch).SetBit(AFF_BONDING);
      ch->char_specials.programming = obj;
      return;
    }
  } else
    send_to_char(ch, "You cannot bond %s.\r\n", GET_OBJ_NAME(obj));
}

ACMD(do_release)
{
  if (GET_TRADITION(ch) == TRAD_ADEPT || GET_TRADITION(ch) == TRAD_MUNDANE) {
    send_to_char("You don't have the ability to do that.\r\n", ch);
    return;
  }
  struct sustain_data *sust = NULL, *next = NULL;
  two_arguments(argument, buf, buf1);
  int i = 0;
  if (is_abbrev(buf, "spirit") || is_abbrev(buf, "elemental")) {
    if (GET_ASPECT(ch) == ASPECT_SORCERER || !GET_SKILL(ch, SKILL_CONJURING)) {
      send_to_char("You don't have the ability to do that.\r\n", ch);
    }
    if (!GET_SPIRIT(ch)) {
      send_to_char(ch, "You don't have any %s bound to you.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elementals" : "spirits");
      return;
    }
    int i;
    if (!(i = atoi(buf1)) || i > GET_NUM_SPIRITS(ch)) {
      send_to_char(ch, "Which %s do you wish to release from your services?\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
      return;
    }
    int real_mob;
    GET_ELEMENTALS_DIRTY_BIT(ch) = TRUE;
    for (struct spirit_data *spirit = GET_SPIRIT(ch); spirit; spirit = spirit->next) {
      if (--i == 0) {
        struct spirit_data *temp;
        if (GET_TRADITION(ch) == TRAD_HERMETIC) {
          send_to_char(ch, "You release %s from its obligations and it departs to the metaplanes.\r\n",
                       (real_mob = real_mobile(elements[spirit->type].vnum)) >= 0 ? GET_NAME(&mob_proto[real_mob]) : "an elemental");
        }
        else
          send_to_char(ch, "You release %s from its obligations and it departs to the metaplanes.\r\n",
                       (real_mob = real_mobile(spirits[spirit->type].vnum)) >= 0 ? GET_NAME(&mob_proto[real_mob]) : "a spirit");
        if (spirit->called)
          for (struct char_data *mob = character_list; mob; mob = mob->next)
            if (IS_NPC(mob) && GET_ACTIVE(mob) == GET_IDNUM(ch) && GET_GRADE(mob) == spirit->id) {
              act("Freed from its services, $n returns to the metaplanes", TRUE, mob, 0, ch, TO_NOTVICT);
              extract_char(mob);
              break;
            }
        REMOVE_FROM_LIST(spirit, GET_SPIRIT(ch), next);
        delete spirit;
        GET_NUM_SPIRITS(ch)--;
        return;
      }
    }
  } else if (is_abbrev(buf, "all")) {
    for (sust = GET_SUSTAINED(ch); sust; sust = next) {
      next = sust->next;
      // Removed checks for focus and spirit sustains. It does say 'all'...
      if (sust->caster)
        end_sustained_spell(ch, sust);
    }
    send_to_char("OK.\r\n", ch);
    return;
  } else if ((i = atoi(buf)) > 0) {
    if (i > GET_SUSTAINED_NUM(ch))
      send_to_char("You don't have that many spells sustained.\r\n", ch);
    else {
      for (sust = GET_SUSTAINED(ch); sust; sust = sust->next)
        if (sust->caster && --i == 0)
          break;
      end_sustained_spell(ch, sust);
    }
    return;
  }
  send_to_char("You'll want to specify an spell number, an elemental/spirit number, or 'all'. HELP RELEASE for more.\r\n", ch);
}

ACMD(do_cast)
{
  if (GET_ASPECT(ch) == ASPECT_CONJURER || GET_TRADITION(ch) == TRAD_ADEPT || GET_TRADITION(ch) == TRAD_MUNDANE || !GET_SKILL(ch, SKILL_SORCERY)) {
    send_to_char("You don't have the ability to do that.\r\n", ch);
    return;
  }

  int force = 0;
  char spell_name[120], tokens[MAX_STRING_LENGTH], *s;
  struct spell_data *spell = GET_SPELLS(ch);
  if (!*argument) {
    send_to_char("Cast what spell?\r\n", ch);
    return;
  }
  strcpy(tokens, argument);
  if ((strtok(tokens, "\"") && (s = strtok(NULL, "\"")))
      || (strtok(tokens, "'") && (s = strtok(NULL, "'")))) {
    strcpy(spell_name, s);
    if ((s = strtok(NULL, "\0"))) {
      skip_spaces(&s);
      strcpy(buf1, s);
    } else
      *buf1 = '\0';
    one_argument(argument, buf);
    force = atoi(buf);
  } else {
    half_chop(argument, buf, buf1);
    if (!(force = atoi(buf))) {
      strcpy(spell_name, buf);
    } else {
      half_chop(buf1, buf2, buf3);
      strlcpy(buf1, buf3, sizeof(buf1));
      strlcpy(spell_name, buf2, sizeof(spell_name));
    }
  }
  for (;spell; spell = spell->next)
    if (is_abbrev(spell_name, spell->name) && !(!str_cmp(spell_name, "heal") && spell->type == SPELL_HEALTHYGLOW))
      break;
  if (!spell) {
    send_to_char(ch, "You don't know a spell called '%s'.\r\n", spell_name);
    return;
  }
  if (!force)
    force = spell->force;
  else if (force > spell->force) {
    send_to_char(ch, "You don't know '%s' at that high a force.\r\n", spell_name);
    return;
  }
  if (spells[spell->type].physical && IS_PROJECT(ch)) {
    send_to_char("You can only cast mana spells on the astral plane.\r\n", ch);
    return;
  }
  if (CH_IN_COMBAT(ch)) {
    if (ch->squeue) {
      if (ch->squeue->arg)
        delete [] ch->squeue->arg;
      delete ch->squeue;
      ch->squeue = NULL;
    }
    ch->squeue = new spell_queue;
    ch->squeue->spell = spell->type;
    ch->squeue->sub = spell->subtype;
    ch->squeue->force = force;
    ch->squeue->arg = str_dup(buf1);
  } else cast_spell(ch, spell->type, spell->subtype, force, buf1);

}

ACMD(do_conjure)
{
  if (GET_ASPECT(ch) == ASPECT_SORCERER || GET_TRADITION(ch) == TRAD_ADEPT || GET_TRADITION(ch) == TRAD_MUNDANE || !GET_SKILL(ch, SKILL_CONJURING)) {
    send_to_char("You don't have the ability to do that.\r\n", ch);
    return;
  }
  if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }
  if (ch->in_veh) {
    send_to_char("There is not enough room to conjure in here.\r\n", ch);
    return;
  }
  if (IS_PROJECT(ch)) {
    send_to_char("You cannot conjure while projecting.\r\n", ch);
    return;
  }
  if (ROOM_FLAGGED(ch->in_room, ROOM_PEACEFUL) || ROOM_FLAGGED(ch->in_room, ROOM_NOMAGIC)) {
    send_to_char("You can't conjure here.\r\n", ch);
    return;
  }
  int force, spirit = 0;
  two_arguments(argument, buf, buf1);
  if (!(force = atoi(buf))) {
    send_to_char("What force do you wish to conjure at?\r\n", ch);
    return;
  }
  if (force > (GET_MAG(ch) / 100) * 2) {
    send_to_char(ch, "You can't conjure %s of force more than twice your magic rating.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "an elemental" : "a spirit");
    return;
  }
  if (GET_TRADITION(ch) == TRAD_HERMETIC) {
    if (GET_NUM_SPIRITS(ch) >= GET_CHA(ch)) {
      send_to_char(ch, "You have too many elementals summoned.\r\n");
      return;
    }
    for (spirit = 0; spirit < NUM_ELEMENTS; spirit++)
      if (is_abbrev(buf1, elements[spirit].name))
        break;
    if (spirit == NUM_ELEMENTS) {
      send_to_char("You must specify one of Earth, Air, Fire, or Water.\r\n", ch);
      return;
    }
    if ((GET_ASPECT(ch) == ASPECT_ELEMFIRE && spirit != ELEM_FIRE) ||
        (GET_ASPECT(ch) == ASPECT_ELEMWATER && spirit != ELEM_WATER) ||
        (GET_ASPECT(ch) == ASPECT_ELEMAIR && spirit != ELEM_AIR) ||
        (GET_ASPECT(ch) == ASPECT_ELEMEARTH && spirit != ELEM_EARTH)) {
      send_to_char("You cannot summon elementals of that type.\r\n", ch);
      return;
    }
    bool library = FALSE, circle = FALSE;
    struct obj_data *obj;
    FOR_ITEMS_AROUND_CH(ch, obj)
      if (GET_OBJ_TYPE(obj) == ITEM_MAGIC_TOOL) {
        if (GET_OBJ_VAL(obj, 0) == TYPE_LIBRARY_CONJURE) {
          if (GET_OBJ_VAL(obj, 1) < force) {
            send_to_char(ch, "Your library isn't of a high enough rating to conjure that elemental. The maximum you can conjure with %s is %d.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)), GET_OBJ_VAL(obj, 1));
            return;
          }
          library = TRUE;
        } else if (GET_OBJ_VAL(obj, 0) == TYPE_CIRCLE && !GET_OBJ_VAL(obj, 9)) {
          if (GET_OBJ_VAL(obj, 1) < force) {
            send_to_char("Your hermetic circle isn't of a high enough rating to conjure that elemental.\r\n", ch);
            return;
          } else if (GET_OBJ_VAL(obj, 2) != spirit) {
            send_to_char("That circle is for a different type of elemental.\r\n", ch);
            return;
          }
          circle = TRUE;
        }
      }
    if (!circle || !library) {
      send_to_char("You need a conjuring library and a hermetic circle to conjure spirits.\r\n", ch);
      return;
    }
    for (obj = ch->carrying; obj; obj = obj->next_content)
      if (GET_OBJ_TYPE(obj) == ITEM_MAGIC_TOOL && GET_OBJ_VAL(obj, 0) == TYPE_SUMMONING) {
        if (GET_OBJ_COST(obj) < force * 1000) {
          send_to_char("You don't have enough materials to summon at that high a force.\r\n", ch);
          return;
        } else
          break;
      }
    if (!obj) {
      send_to_char("You need to be carrying conjuring materials to conjure an elemental.\r\n", ch);
      return;
    }
    AFF_FLAGS(ch).SetBit(AFF_CONJURE);
    ch->char_specials.conjure[0] = spirit;
    ch->char_specials.conjure[1] = force;
    if (GET_LEVEL(ch) > 1) {
      send_to_char(ch, "You use your staff powers to greatly accelerate the conjuring process.\r\n");
      ch->char_specials.conjure[2] = 1;
    } else
      ch->char_specials.conjure[2] = force * 30;
    // Max tracking for PROGRESS command.
    ch->char_specials.conjure[3] = ch->char_specials.conjure[2];
    ch->char_specials.programming = obj;
    send_to_char(ch, "You begin to conjure a %s elemental.\r\n", elements[spirit].name);
  } else {
    if (GET_NUM_SPIRITS(ch)) {
      send_to_char("You already have summoned a nature spirit.\r\n", ch);
      return;
    }
    for (spirit = 0; spirit < NUM_SPIRITS; spirit++)
      if (is_abbrev(buf1, spirits[spirit].name))
        break;
    if (spirit == NUM_SPIRITS) {
      strcpy(buf, "Which spirit do you wish to conjure? In your current domain, you can conjure");
      bool have_sent_text = FALSE;
      for (spirit = 0; spirit < NUM_SPIRITS; spirit++)
        if (GET_DOMAIN(ch) == ((spirit == SPIRIT_MIST || spirit == SPIRIT_STORM || spirit == SPIRIT_WIND) ? SPIRIT_SKY : spirit)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s %s", have_sent_text ? "," : "", spirits[spirit].name);
          have_sent_text = TRUE;
        }
      send_to_char(buf, ch);
      return;
    }
    if (GET_DOMAIN(ch) != ((spirit == SPIRIT_MIST || spirit == SPIRIT_STORM || spirit == SPIRIT_WIND) ? SPIRIT_SKY : spirit)) {
      send_to_char("You aren't in the correct domain to conjure that spirit.\r\n", ch);
      return;
    }

    // Calculate the skill and TN used.
    int skill = GET_SKILL(ch, SKILL_CONJURING), target = force;
    if (ch->in_room->background[CURRENT_BACKGROUND_TYPE] == AURA_POWERSITE)
      skill += GET_BACKGROUND_COUNT(ch->in_room);
    else
      target += GET_BACKGROUND_COUNT(ch->in_room);
    totem_bonus(ch, CONJURING, spirit, target, skill);
    if (GET_ASPECT(ch) == ASPECT_SHAMANIST && skill == GET_SKILL(ch, SKILL_CONJURING)) {
      send_to_char("Your totem will not let you conjure that spirit!\r\n", ch);
      return;
    }

    // Modify the skill rating by foci.
    for (int i = 0; i < NUM_WEARS; i++)
      if (GET_EQ(ch, i)
          && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_FOCUS
          && GET_OBJ_VAL(GET_EQ(ch, i), 0) == FOCI_SPIRIT
          && focus_is_usable_by_ch(GET_EQ(ch, i), ch)
          && GET_OBJ_VAL(GET_EQ(ch, i), 3) == ch->char_specials.conjure[0]
          && GET_OBJ_VAL(GET_EQ(ch, i), 4)) {
        skill += GET_OBJ_VAL(GET_EQ(ch, i), 1);
        break;
      }

    // Modify the TN for wound penalties and sustains.
    strlcpy(buf3, "", sizeof(buf3)); strlcpy(buf2, "", sizeof(buf2));
    target += damage_modifier(ch, buf3, sizeof(buf3)) + sustain_modifier(ch, buf2, sizeof(buf2));
    snprintf(buf, sizeof(buf), "Sustain: %s\r\nWound modifiers: %s", buf3, buf2);
    act(buf, FALSE, ch, 0, 0, TO_ROLLS);

    int success = success_test(skill, target);
    if (!conjuring_drain(ch, force) && AWAKE(ch)) {
      if (success < 1)
        send_to_char("You fail to conjure forth that spirit.\r\n", ch);
      else {
        struct spirit_data *spirdata = new spirit_data;
        spirdata->type = spirit;
        spirdata->force = force;
        spirdata->id = number(0, 1000);
        spirdata->services = success;
        spirdata->called = TRUE;
        spirdata->next = GET_SPIRIT(ch);
        GET_SPIRIT(ch) = spirdata;
        GET_NUM_SPIRITS(ch)++;
        GET_ELEMENTALS_DIRTY_BIT(ch) = TRUE;
        struct char_data *mob = create_elemental(ch, spirit, force, spirdata->id, TRAD_SHAMANIC);
        send_to_char("The spirit hears your call and comes forth from the metaplanes.\r\n", ch);
        act("$n fades into existance on the astral plane.", TRUE, mob, 0, 0, TO_ROOM);

        AFF_FLAGS(ch).SetBit(AFF_GROUP);
        AFF_FLAGS(mob).SetBit(AFF_GROUP);
        GET_SPARE2(mob) = NUMBER_OF_IG_DAYS_FOR_SPIRIT_TO_LAST;
      }
    }
  }
}

ACMD(do_spells)
{
  // Conjurers cannot cast spells.
  if (GET_ASPECT(ch) == ASPECT_CONJURER) {
    send_to_char(ch, "%ss don't have the aptitude for spells.\r\n", aspect_names[GET_ASPECT(ch)]);
    return;
  }

  // Adepts and Mundanes cannot cast spells.
  if (GET_TRADITION(ch) == TRAD_ADEPT || GET_TRADITION(ch) == TRAD_MUNDANE) {
    send_to_char(ch, "%ss don't have the aptitude for spells.\r\n", tradition_names[(int) GET_TRADITION(ch)]);
    return;
  }

  // Character is of the right type to cast spells, but doesn't have the skill trained yet.
  if (!GET_SKILL(ch, SKILL_SORCERY)) {
    send_to_char(ch, "You need to learn the %s skill first.\r\n", skills[SKILL_SORCERY].name);
    return;
  }

  // Character has no spells known.
  if (!GET_SPELLS(ch)) {
    send_to_char("You don't know any spells.\r\n", ch);
    return;
  }

  // Character knows spells. List them.
  send_to_char("You know the following spells:\r\n", ch);
  for (struct spell_data *spell = GET_SPELLS(ch); spell; spell = spell->next)
    send_to_char(ch, "%-50s Category: %12s Force: %d\r\n", spell->name, spell_category[spells[spell->type].category], spell->force);
}

ACMD(do_forget)
{
  if (!PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED) || !GET_SPELLS(ch)) {
    nonsensical_reply(ch, NULL, "standard");
    return;
  }
  skip_spaces(&argument);

  for (struct spell_data *spell = GET_SPELLS(ch); spell; spell = spell->next)
   if (is_abbrev(argument, spell->name)) {
     struct spell_data *temp;

     send_to_char(ch, "You forget %s.\r\n", spell->name);
     GET_FORCE_POINTS(ch) += spell->force;

     REMOVE_FROM_LIST(spell, GET_SPELLS(ch), next);
     delete spell;

     GET_SPELLS_DIRTY_BIT(ch) = TRUE;
     return;
   }
}

ACMD(do_learn)
{
  if (GET_ASPECT(ch) == ASPECT_CONJURER || GET_TRADITION(ch) == TRAD_ADEPT || GET_TRADITION(ch) == TRAD_MUNDANE || !GET_SKILL(ch, SKILL_SORCERY)) {
    send_to_char("You don't have the ability to do that.\r\n", ch);
    return;
  }
  two_arguments(argument, buf, buf1);
  struct obj_data *obj = ch->carrying;
  struct spell_data *spell = NULL;
  int force, oldforce = 0;
  if (!*buf) {
    send_to_char("What spell formula would you like to learn from?\r\n", ch);
    return;
  }

  if (!(obj = get_obj_in_list_vis(ch, buf, ch->carrying))) {
    send_to_char(ch, "You're not carrying any '%s' to learn from.\r\n", buf);
    return;
  }
  if (GET_OBJ_TYPE(obj) != ITEM_SPELL_FORMULA) {
    send_to_char(ch, "%s doesn't contain a spell formula to learn.\r\n", capitalize(GET_OBJ_NAME(obj)));
    return;
  }
  if (GET_OBJ_TIMER(obj) <= -2) {
    send_to_char(ch, "The spell design on %s isn't complete.\r\n", GET_OBJ_NAME(obj));
    return;
  }
  if ((GET_TRADITION(ch) == TRAD_HERMETIC && GET_OBJ_VAL(obj, 2)) || (GET_TRADITION(ch) == TRAD_SHAMANIC && !GET_OBJ_VAL(obj, 2))) {
    send_to_char(ch, "You don't understand the formula written on %s-- seems like it's for another tradition of magic.\r\n", GET_OBJ_NAME(obj));
    return;
  }
  if (!*buf2 || atoi(buf1) == 0)
    force = GET_OBJ_VAL(obj, 0);
  else
    force = MIN(GET_OBJ_VAL(obj, 0), atoi(buf1));
  for (spell = GET_SPELLS(ch); spell; spell = spell->next)
    if (spell->type == GET_SPELLFORMULA_SPELL(obj) && spell->subtype == GET_SPELLFORMULA_SUBTYPE(obj)) {
      if (spell->force >= force) {
        send_to_char(ch, "You already know %s at an equal or higher force.\r\n", spells[GET_SPELLFORMULA_SPELL(obj)].name);
        return;
      } else {
        oldforce = spell->force;
        break;
      }
    }
  if (GET_KARMA(ch) < (force  - oldforce) * 100) {
    send_to_char(ch, "You don't have enough karma to learn this spell at that force! You need %d.\r\n", force);
    return;
  }
  if ((GET_ASPECT(ch) == ASPECT_ELEMFIRE && spells[GET_OBJ_VAL(obj, 1)].category != COMBAT) ||
      (GET_ASPECT(ch) == ASPECT_ELEMEARTH && spells[GET_OBJ_VAL(obj, 1)].category != MANIPULATION) ||
      (GET_ASPECT(ch) == ASPECT_ELEMWATER && spells[GET_OBJ_VAL(obj, 1)].category != ILLUSION) ||
      (GET_ASPECT(ch) == ASPECT_ELEMAIR && spells[GET_OBJ_VAL(obj, 1)].category != DETECTION)) {
    send_to_char("Glancing over the formula, you realize you can't bind mana in that fashion.\r\n", ch);
    return;
  }
  if (GET_ASPECT(ch) == ASPECT_SHAMANIST) {
    int skill = 0, target = 0;
    totem_bonus(ch, 0, GET_OBJ_VAL(obj, 1), target, skill);
    if (skill < 1) {
      send_to_char(ch, "%s forbids you from learning this spell.\r\n", totem_types[GET_TOTEM(ch)]);
      return;
    }
  }
  struct obj_data *library = ch->in_veh ? ch->in_veh->contents : ch->in_room->contents;
  int library_level = 0;
  for (;library; library = library->next_content) {
    if (GET_OBJ_TYPE(library) == ITEM_MAGIC_TOOL
        && ((GET_TRADITION(ch) == TRAD_SHAMANIC
            && GET_OBJ_VAL(library, 0) == TYPE_LODGE && GET_OBJ_VAL(library, 3) == GET_IDNUM(ch))
        || (GET_TRADITION(ch) == TRAD_HERMETIC && GET_OBJ_VAL(library, 0) == TYPE_LIBRARY_SPELL))) {
      if (GET_OBJ_VAL(library, 1) >= force) {
        break;
      } else {
        library_level = MAX(GET_OBJ_VAL(library, 1), library_level);
      }
    }
  }
  if (!library) {
    if (library_level)
      send_to_char(ch, "Your tools aren't a high enough rating to learn from %s. It's rating %d, but you only have a rating %d.",
                   GET_OBJ_NAME(obj), force, library_level);
    else
      send_to_char(ch, "You don't have the right tools here to learn that spell. You need a rating-%d (or higher) %s.\r\n",
                   force, GET_TRADITION(ch) == TRAD_HERMETIC ? "library" : "lodge");
    return;
  }
  if (GET_TRADITION(ch) == TRAD_SHAMANIC && GET_OBJ_VAL(library, 9)) {
    send_to_char("You need to finish building that lodge before you can use it.\r\n", ch);
    return;
  }
  int skill = GET_SKILL(ch, SKILL_SORCERY);
  if (GET_TRADITION(ch) == TRAD_SHAMANIC) {
    int target = 0;
    totem_bonus(ch, 0, GET_OBJ_VAL(obj, 1), target, skill);
  } else if (GET_TRADITION(ch) == TRAD_HERMETIC && GET_SPIRIT(ch)) {
    for (struct spirit_data *spir = GET_SPIRIT(ch); spir && skill == GET_SKILL(ch, SKILL_SORCERY); spir = spir->next)
      if (spir->called) {
        struct char_data *spirit = find_spirit_by_id(spir->id, GET_IDNUM(ch));
        if (MOB_FLAGS(spirit).IsSet(MOB_STUDY)) {
          switch(spir->type) {
          case ELEM_FIRE:
            if (spells[GET_OBJ_VAL(obj, 1)].category == COMBAT) {
              skill += spir->force;
              MOB_FLAGS(spirit).RemoveBit(MOB_STUDY);
            }
            break;
          case ELEM_WATER:
            if (spells[GET_OBJ_VAL(obj, 1)].category == ILLUSION) {
              skill += spir->force;
              MOB_FLAGS(spirit).RemoveBit(MOB_STUDY);
            }
            break;
          case ELEM_AIR:
            if (spells[GET_OBJ_VAL(obj, 1)].category == DETECTION) {
              skill += spir->force;
              MOB_FLAGS(spirit).RemoveBit(MOB_STUDY);
            }
            break;
          case ELEM_EARTH:
            if (spells[GET_OBJ_VAL(obj, 1)].category == MANIPULATION) {
              skill += spir->force;
              MOB_FLAGS(spirit).RemoveBit(MOB_STUDY);
            }
            break;
          }
          elemental_fulfilled_services(ch, spirit, spir);
          break;
        }
      }
  }
  if (success_test(skill, force * 2) < 1) {
    send_to_char("You can't get your head around how to cast that spell.\r\n", ch);
    return;
  }
  if (spell) {
    struct spell_data *temp;
    REMOVE_FROM_LIST(spell, GET_SPELLS(ch), next);
    delete spell;
    spell = NULL;
  }
  GET_KARMA(ch) -= (force - oldforce) * 100;
  spell = new spell_data;
  spell->name = str_dup(compose_spell_name(GET_SPELLFORMULA_SPELL(obj), GET_SPELLFORMULA_SUBTYPE(obj)));
  spell->type = GET_SPELLFORMULA_SPELL(obj);
  spell->subtype = GET_SPELLFORMULA_SUBTYPE(obj);
  spell->force = force;
  spell->next = GET_SPELLS(ch);
  GET_SPELLS(ch) = spell;
  send_to_char(ch, "You spend %d karma and learn %s.\r\n", force - oldforce, spell->name);
  extract_obj(obj);
  GET_SPELLS_DIRTY_BIT(ch) = TRUE;
}

ACMD(do_elemental)
{
  if (GET_ASPECT(ch) == ASPECT_SORCERER || GET_TRADITION(ch) == TRAD_MUNDANE || GET_TRADITION(ch) == TRAD_ADEPT || !GET_SKILL(ch, SKILL_CONJURING)) {
    send_to_char("You don't have the ability to do that.\r\n", ch);
    return;
  }
  if (!GET_NUM_SPIRITS(ch)) {
    send_to_char(ch, "You don't have any %s bound to you.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elementals" : "spirits");
    return;
  }
  int i = 1, real_mob;
  snprintf(buf, sizeof(buf), "You currently have the following %s bound:\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elementals" : "spirits");
  for (struct spirit_data *elem = GET_SPIRIT(ch); elem; elem = elem->next, i++) {
    if (GET_TRADITION(ch) == TRAD_SHAMANIC)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%d) %-30s (Force %d) Services %d\r\n",
               i,
               (real_mob = real_mobile(spirits[elem->type].vnum)) >= 0 ? GET_NAME(&mob_proto[real_mob]) : "a spirit",
               elem->force, elem->services);
    else
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%d) %-30s (Force %d) Services: %d%10s\r\n",
               i,
               (real_mob = real_mobile(elements[elem->type].vnum)) >= 0 ? GET_NAME(&mob_proto[real_mob]) : "an elemental",
               elem->force, elem->services, elem->called ? " Present" : " ");
  }
  send_to_char(buf, ch);
}

ACMD(do_banish)
{
  if (GET_ASPECT(ch) == ASPECT_SORCERER || GET_TRADITION(ch) == TRAD_ADEPT || GET_TRADITION(ch) == TRAD_MUNDANE || !GET_SKILL(ch, SKILL_CONJURING)) {
    send_to_char("You don't have the ability to do that.\r\n", ch);
    return;
  }
  if (IS_PROJECT(ch)) {
    send_to_char("You cannot banish while projecting.\r\n", ch);
    return;
  }
  if (AFF_FLAGGED(ch, AFF_BANISH)) {
    if (GET_EXTRA(ch)) {
      if (FIGHTING(ch)) {
        act("You give up trying to banish $n.", FALSE, FIGHTING(ch), 0, ch, TO_VICT);
        AFF_FLAGS(FIGHTING(ch)).RemoveBit(AFF_BANISH);
      }
      stop_fighting(ch);
      AFF_FLAGS(ch).RemoveBit(AFF_BANISH);
    } else
      send_to_char(ch, "Try as you might, you can't wrest your mind away!\r\n");
    return;
  }
  struct char_data *mob;
  skip_spaces(&argument);
  if (!(mob = get_char_room_vis(ch, argument))) {
    send_to_char(ch, "Attempt to banish which %s?\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
    return;
  }
  if (GET_RACE(mob) != RACE_SPIRIT && GET_RACE(mob) != RACE_ELEMENTAL) {
    send_to_char("You can only banish a nature spirit or elemental.\r\n", ch);
    return;
  }
  stop_fighting(ch);
  stop_fighting(mob);
  set_fighting(ch, mob);
  set_fighting(mob, ch);
  AFF_FLAGS(ch).SetBit(AFF_BANISH);
  AFF_FLAGS(mob).SetBit(AFF_BANISH);
  send_to_char(ch, "You begin attempting to banish %s.\r\n", GET_NAME(mob));
  act("$n begins attempting to banish $N.", FALSE, ch, 0, mob, TO_ROOM);
  GET_EXTRA(ch) = 1;
  GET_EXTRA(mob) = 0;
}

bool spirit_can_perform(int type, int order, int tradition)
{
  if (order == SERV_MATERIALIZE || order == SERV_DEMATERIAL || order == SERV_ATTACK)
    return TRUE;
  if (tradition == TRAD_HERMETIC) {
    if (order == SERV_ENGULF || order == SERV_APPEAR || order == SERV_SORCERY || order == SERV_STUDY || order == SERV_SUSTAIN || order == SERV_LEAVE)
      return TRUE;
    switch (type) {
    case ELEM_FIRE:
      if (order == SERV_FLAMEAURA || order == SERV_GUARD || order == SERV_FLAMETHROWER)
        return TRUE;
      break;
    case ELEM_AIR:
      if (order == SERV_MOVEMENT || order == SERV_BREATH || order == SERV_PSYCHOKINESIS)
        return TRUE;
      break;
    case ELEM_WATER:
    case ELEM_EARTH:
      if (order == SERV_MOVEMENT)
        return TRUE;
      break;
    }
  } else {
    if ((type != SPIRIT_STORM && type != SPIRIT_DESERT) && order == SERV_ACCIDENT)
      return TRUE;
    if ((type != SPIRIT_LAKE && type != SPIRIT_WIND) && order == SERV_CONCEAL)
      return TRUE;
    if (type != SPIRIT_STORM && order == SERV_GUARD)
      return TRUE;
    if ((type != SPIRIT_FOREST && type != SPIRIT_STORM && type != SPIRIT_MIST) && order == SERV_SEARCH)
      return TRUE;
    switch(type) {
    case SPIRIT_CITY:
      if (order == SERV_CONFUSION || order == SERV_FEAR)
        return TRUE;
      break;
    case SPIRIT_FIELD:
      break;
    case SPIRIT_HEARTH:
      if (order == SERV_CONFUSION)
        return TRUE;
      break;
    case SPIRIT_DESERT:
      if (order == SERV_MOVEMENT)
        return TRUE;
      break;
    case SPIRIT_FOREST:
      if (order == SERV_CONFUSION || order == SERV_FEAR)
        return TRUE;
      break;
    case SPIRIT_MOUNTAIN:
    case SPIRIT_PRAIRIE:
      if (order == SERV_MOVEMENT)
        return TRUE;
      break;
    case SPIRIT_MIST:
      if (order == SERV_CONFUSION || order == SERV_MOVEMENT)
        return TRUE;
      break;
    case SPIRIT_STORM:
      if (order == SERV_CONFUSION || order == SERV_FEAR)
        return TRUE;
      break;
    case SPIRIT_WIND:
      if (order == SERV_CONFUSION || order == SERV_MOVEMENT)
        return TRUE;
      break;
    case SPIRIT_LAKE:
      if (order == SERV_ENGULF || order == SERV_FEAR || order == SERV_MOVEMENT)
        return TRUE;
      break;
    case SPIRIT_RIVER:
      if (order == SERV_ENGULF || order == SERV_FEAR || order == SERV_MOVEMENT)
        return TRUE;
      break;
    case SPIRIT_SEA:
      if (order == SERV_CONFUSION || order == SERV_ENGULF || order == SERV_MOVEMENT)
        return TRUE;
      break;
    case SPIRIT_SWAMP:
      if (order == SERV_BINDING || order == SERV_CONFUSION || order == SERV_ENGULF || order == SERV_FEAR ||
          order == SERV_MOVEMENT)
        return TRUE;
      break;
    }
  }
  return FALSE;
}

void make_spirit_power(struct char_data *spirit, struct char_data *tch, int type)
{
  struct spirit_sustained *ssust = new spirit_sustained;
  ssust->type = type;
  ssust->caster = TRUE;
  ssust->target = tch;
  ssust->next = SPIRIT_SUST(spirit);
  SPIRIT_SUST(spirit) = ssust;
  ssust = new spirit_sustained;
  ssust->type = type;
  ssust->caster = FALSE;
  ssust->target = spirit;
  ssust->next = SPIRIT_SUST(tch);
  SPIRIT_SUST(tch) = ssust;
}

void stop_spirit_power(struct char_data *spirit, int type)
{
  struct spirit_sustained *temp;
  for (struct spirit_sustained *ssust = SPIRIT_SUST(spirit); ssust; ssust = ssust->next)
    if (ssust->type == type && ssust->caster == TRUE)
    {
      for (struct spirit_sustained *tsust = SPIRIT_SUST(ssust->target); tsust; tsust = tsust->next)
        if (tsust->type == type && tsust->target == spirit) {
          if (type == ENGULF) {
            if (IS_SPIRIT(spirit) || (IS_ELEMENTAL(spirit) && GET_SPARE1(spirit) == ELEM_WATER)) {
              act("The water surrounding $n falls away and soaks into the ground almost instantly.", TRUE, ssust->target, 0, 0, TO_ROOM);
              send_to_char("The water surrounding you suddenly vanishes allowing you to gasp for breath!\r\n", ssust->target);
            } else {
              switch (GET_SPARE1(spirit)) {
              case ELEM_FIRE:
                act("The fire engulfing $n suddenly goes out.", TRUE, ssust->target, 0, 0, TO_ROOM);
                send_to_char("The water fire surrounding you suddenly vanishes!\r\n", ssust->target);
                break;
              case ELEM_EARTH:
                act("The dirt and rock engulfing $n suddenly bursts apart, covering everyone close by.", TRUE, ssust->target, 0, 0, TO_ROOM);
                send_to_char("The earth surrounding you suddenly bursts open, allowing you to gasp for air!\r\n", ssust->target);
                break;
              case ELEM_AIR:
                act("$n begins to gasp for breath as though $s was just choked.", TRUE, ssust->target, 0, 0, TO_ROOM);
                send_to_char("The mysterious force oppressing your lungs is suddenly gone!\r\n", ssust->target);
                break;
              }
            }
          } else if (type == CONFUSION)
            send_to_char("Your mind clears.\r\n", ssust->target);
          else if (type == CONCEAL)
            send_to_char("You suddenly feel very exposed.\r\n", ssust->target);
          else if (type == MOVEMENTUP || type == MOVEMENTDOWN)
            send_to_char("The force pushing you forward seems to lessen.\r\n", ssust->target);
          REMOVE_FROM_LIST(tsust, SPIRIT_SUST(ssust->target), next);
          delete tsust;
          break;
        }
      REMOVE_FROM_LIST(ssust, SPIRIT_SUST(spirit), next);
      delete ssust;
      break;
    }
}

POWER(spirit_appear)
{
  spiritdata->called = TRUE;
  spirit = create_elemental(ch, spiritdata->type, spiritdata->force, spiritdata->id, GET_TRADITION(ch));
  act("$n appears in the astral plane.", TRUE, spirit, 0, ch, TO_NOTVICT);
  send_to_char(ch, "%s heeds your call and arrives from the metaplanes ready to do your bidding.\r\n", CAP(GET_NAME(spirit)));
}

POWER(spirit_sorcery)
{
  if (!MOB_FLAGS(spirit).IsSet(MOB_AIDSORCERY)) {
    spiritdata->services--;
    MOB_FLAGS(spirit).SetBit(MOB_AIDSORCERY);
    send_to_char(ch, "%s will now assist you in casting a spell next time it is able.\r\n", CAP(GET_NAME(spirit)));
  } else
    send_to_char(ch, "%s will already assist in spell casting next time it is able.\r\n", CAP(GET_NAME(spirit)));

}

POWER(spirit_study)
{
  if (!MOB_FLAGS(spirit).IsSet(MOB_STUDY)) {
    spiritdata->services--;
    MOB_FLAGS(spirit).SetBit(MOB_STUDY);
    send_to_char(ch, "%s will now assist you in studying a spell next time it is able.\r\n", CAP(GET_NAME(spirit)));
  } else
    send_to_char(ch, "%s will already assist you the next time it is able.\r\n", CAP(GET_NAME(spirit)));
}

POWER(spirit_sustain)
{
  struct sustain_data *sust;
  if (GET_SUSTAINED_NUM(spirit))
    send_to_char(ch, "That %s is already sustaining a spell.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
  else {
    int i = atoi(arg);
    if (i <= 0 || i > GET_SUSTAINED_NUM(ch)) {
      send_to_char("You aren't sustaining that many spells.\r\n", ch);
      return;
    }
    for (sust = GET_SUSTAINED(ch); sust; sust = sust->next)
      if (sust->caster && --i == 0)
        break;

    // Anti-crash.
    if (!sust) {
      mudlog("SYSERR: We would have crashed from a bad sustain!", ch, LOG_SYSLOG, TRUE);
      send_to_char("Your elemental can't sustain that spell.\r\n", ch);
      return;
    }

    if (sust->focus || sust->spirit) {
      send_to_char("You aren't sustaining that spell yourself.\r\n", ch);
      return;
    }
    if ((spells[sust->spell].category == MANIPULATION && spiritdata->type == ELEM_EARTH) ||
        (spells[sust->spell].category == COMBAT && spiritdata->type == ELEM_FIRE) ||
        (spells[sust->spell].category == ILLUSION && spiritdata->type == ELEM_WATER) ||
        (spells[sust->spell].category == DETECTION && spiritdata->type == ELEM_AIR)) {
      sust->spirit = spirit;
      GET_SUSTAINED_FOCI(ch)++;
      GET_SUSTAINED_NUM(spirit)++;
      spiritdata->services--;
      GET_SUSTAINED(spirit) = sust;
      send_to_char(ch, "%s sustains %s for you.\r\n", CAP(GET_NAME(spirit)), spells[sust->spell].name);
    } else
      send_to_char(ch, "That %s can't sustain that type of spell.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
  }
}

POWER(spirit_accident)
{
  struct char_data *tch = get_char_room_vis(spirit, arg);
  if (!tch)
    send_to_char("Use accident against which target?\r\n", ch);
  else if (tch == ch)
    send_to_char("You cannot target yourself with that power.\r\n", ch);
  else if (tch == spirit)
    send_to_char(ch, "The %s refuses to perform that service.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
  else {
    int success = success_test(MAX(GET_INT(tch), GET_QUI(tch)), GET_SPARE2(spirit));
    for (struct char_data *mob = spirit->in_room->people; mob; mob = mob->next)
      if (IS_NPC(mob) && (GET_RACE(mob) == RACE_SPIRIT || GET_RACE(mob) == RACE_ELEMENTAL) &&
          MOB_FLAGGED(mob, MOB_SPIRITGUARD)) {
        success = -1;
        break;
      }
    if (success < 1) {
      act("$n trips and stumbles.", TRUE, tch, 0, 0, TO_ROOM);
      send_to_char(tch, "You trip and stumble!\r\n");
      GET_INIT_ROLL(tch) -= 10;
    } else {
      snprintf(buf, sizeof(buf), "%s fails to cause $N to have an accident.", CAP(GET_NAME(spirit)));
      act(buf, TRUE, ch, 0, tch, TO_CHAR);
    }
    spiritdata->services--;
  }
}

POWER(spirit_binding)
{
  struct char_data *tch = get_char_room_vis(spirit, arg);
  if (!tch)
    send_to_char("Use binding against which target?\r\n", ch);
  else if (tch == spirit || tch == ch)
    send_to_char(ch, "The %s refuses to perform that service.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
  else if (ROOM_FLAGGED(get_ch_in_room(spirit), ROOM_PEACEFUL))
    send_to_char("It's too peaceful here...\r\n", ch);
  else {
    act("$N suddenly becomes incapable of movement!", FALSE, spirit, 0, ch, TO_VICT);
    send_to_char("You suddenly notice you are stuck fast to the ground!\r\n", tch);
    AFF_FLAGS(tch).SetBit(AFF_BINDING);
    tch->points.binding = GET_LEVEL(spirit) * 2;
    spiritdata->services--;
  }
}

POWER(spirit_conceal)
{
  struct char_data *tch = get_char_room_vis(spirit, arg);
  if (affected_by_power(spirit, CONCEAL)) {
    act("$N stops providing that services.", FALSE, ch, 0, spirit, TO_CHAR);
    stop_spirit_power(spirit, CONCEAL);
    return;
  }
  if (!tch)
    send_to_char("Use conceal against which target?\r\n", ch);
  else if (tch == spirit || affected_by_power(tch, CONCEAL))
    send_to_char(ch, "The %s refuses to perform that service.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
  else {
    act("$n vanishes from sight.", FALSE, spirit, 0, ch, TO_VICT);
    send_to_char("The terrain seems to cover your tracks.\r\n", tch);
    make_spirit_power(spirit, tch, CONCEAL);
    spiritdata->services--;
  }
}

POWER(spirit_confusion)
{
  struct char_data *tch = get_char_room_vis(spirit, arg);
  if (affected_by_power(spirit, CONFUSION)) {
    act("$N stops providing that services.", FALSE, ch, 0, spirit, TO_CHAR);
    stop_spirit_power(spirit, CONFUSION);
    return;
  }
  if (!tch)
    send_to_char("Use confusion against which target?\r\n", ch);
  else if (tch == spirit || tch == ch || affected_by_power(tch, CONFUSION))
    send_to_char(ch, "The %s refuses to harm itself.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
  else if (ROOM_FLAGGED(get_ch_in_room(spirit), ROOM_PEACEFUL))
    send_to_char("It's too peaceful here...\r\n", ch);
  else {
    act("$n winces and clutches at $s head.", FALSE, spirit, 0, ch, TO_VICT);
    send_to_char("The world shifts and warps unnaturally around you.\r\n", tch);
    make_spirit_power(spirit, tch, CONFUSION);
    spiritdata->services--;
  }
}

POWER(spirit_engulf)
{
  struct char_data *tch = get_char_room_vis(spirit, arg);
  if (affected_by_power(spirit, ENGULF)) {
    act("$N stops providing that services.", FALSE, ch, 0, spirit, TO_CHAR);
    stop_spirit_power(spirit, ENGULF);
    return;
  }
  if (!tch)
    send_to_char("Use movement against which target?\r\n", ch);
  else if (tch == spirit || tch == ch || affected_by_power(tch, ENGULF))
    send_to_char(ch, "The %s refuses to harm itself.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
  else if (ROOM_FLAGGED(get_ch_in_room(spirit), ROOM_PEACEFUL))
    send_to_char("It's too peaceful here...\r\n", ch);
  else {
    act("$n rushes towards $N and attempts to engulf them!", FALSE, spirit, 0, tch, TO_ROOM);
    int target = GET_QUI(spirit), targskill = get_skill(tch, SKILL_UNARMED_COMBAT, target);
    int success = success_test(GET_QUI(spirit), targskill) - success_test(targskill, GET_QUI(spirit));
    if (success < 1) {
      act("$n successfully dodges the attack!", TRUE, ch, 0, 0, TO_ROOM);
      send_to_char("You successfully dodge!\r\n", tch);
      return;
    }
    if (IS_SPIRIT(spirit) || (IS_ELEMENTAL(spirit) && GET_SPARE1(spirit) == ELEM_WATER)) {
      act("The water in the air surrounding $n seems to quickly condense and engulf $s!", TRUE, tch, 0, 0, TO_ROOM);
      send_to_char("The water in the air around you seems to condense and swallow you up!\r\n", tch);
    } else {
      switch (GET_SPARE1(spirit)) {
      case ELEM_FIRE:
        act("A strange, unnatural fire suddenly engulfs $n!", TRUE, tch, 0, 0, TO_ROOM);
        send_to_char("Fire suddenly engulfs your entire body!\r\n", tch);
        break;
      case ELEM_EARTH:
        act("The ground seems to rise up from below and encase $n!", TRUE, tch, 0, 0, TO_ROOM);
        send_to_char("Clods of mud and earth slam into you, forming a suffocating coccoon!\r\n", tch);
        break;
      case ELEM_AIR:
        act("$n suddenly falls to $s knees and begins to retch!", TRUE, tch, 0, 0, TO_ROOM);
        send_to_char("A huge pressure falls on your lungs and you find it impossible to breathe!\r\n", tch);
        break;
      }
    }
    make_spirit_power(spirit, tch, ENGULF);
    set_fighting(spirit, tch);
    set_fighting(tch, spirit);
    GET_EXTRA(tch) = 0;
    spiritdata->services--;
  }
}

POWER(spirit_fear)
{
  struct char_data *tch = get_char_room_vis(spirit, arg);
  if (!tch)
    send_to_char("Use fear against which target?\r\n", ch);
  else if (tch == ch)
    send_to_char("You cannot target yourself with that power.\r\n", ch);
  else if (tch == spirit)
    send_to_char(ch, "The %s refuses to harm itself.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
  else if (ROOM_FLAGGED(get_ch_in_room(spirit), ROOM_PEACEFUL))
    send_to_char("It's too peaceful here...\r\n", ch);
  else {
    int success = success_test(GET_SPARE2(spirit), GET_WIL(tch)) - success_test(GET_WIL(tch), GET_SPARE2(spirit));
    if (success < 1) {
      send_to_char("A dark shadow passes over your mind, but is quickly gone.\r\n", tch);
      snprintf(buf, sizeof(buf), "%s fails to instill fear in $N.", CAP(GET_NAME(spirit)));
      act(buf, FALSE, ch, 0, tch, TO_CHAR);
    } else {
      AFF_FLAGS(tch).SetBit(AFF_FEAR);
      send_to_char("An all consuming terror overcomes you!\r\n", tch);
      snprintf(buf, sizeof(buf), "%s succeeds in instilling fear in $N.", CAP(GET_NAME(spirit)));
      act(buf, FALSE, ch, 0, tch, TO_CHAR);
      extern ACMD_CONST(do_flee);
      do_flee(tch, "", 0, 0);
    }
    spiritdata->services--;
  }
}

POWER(spirit_flameaura)
{
  if (!MOB_FLAGGED(spirit, MOB_FLAMEAURA))
    act("$n's body erupts in flames.", TRUE, spirit, 0, 0, TO_ROOM);
  else
    act("The flames surrounding $n subsides.", TRUE, spirit, 0, 0, TO_ROOM);
  MOB_FLAGS(spirit).ToggleBit(MOB_FLAMEAURA);
  spiritdata->services--;
}

POWER(spirit_flamethrower)
{
  struct char_data *tch = get_char_room_vis(spirit, arg);
  if (!tch)
    send_to_char("Use flamethrower against which target?\r\n", ch);
  else if (tch == ch)
    send_to_char("You cannot target yourself with that power.\r\n", ch);
  else if (tch == spirit)
    send_to_char(ch, "The %s refuses to harm itself.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
  else if (ROOM_FLAGGED(get_ch_in_room(spirit), ROOM_PEACEFUL))
    send_to_char("It's too peaceful here...\r\n", ch);
  else {
    snprintf(buf, sizeof(buf), "moderate %s", arg);
    cast_spell(spirit, SPELL_FLAMETHROWER, 0, GET_LEVEL(spirit), buf);
  }
}

POWER(spirit_guard)
{
  if (!MOB_FLAGGED(spirit, MOB_SPIRITGUARD))
    act("$n begins to guard the area from accidents.", FALSE, spirit, 0, ch, TO_VICT);
  else
    act("$n stops guarding the area from accidents.", FALSE, spirit, 0, ch, TO_VICT);
  MOB_FLAGS(spirit).ToggleBit(MOB_SPIRITGUARD);
  spiritdata->services--;
}

POWER(spirit_leave)
{
  act("$n vanishes back into the metaplanes.", TRUE, spirit, 0, ch, TO_NOTVICT);
  send_to_char(ch, "%s returns to the metaplanes to await further orders.\r\n", CAP(GET_NAME(spirit)));
  spiritdata->called = FALSE;
  extract_char(spirit);
}

POWER(spirit_dematerialize)
{
  act("$n fades from the physical realm.\r\n", TRUE, spirit, 0, ch, TO_ROOM);
  MOB_FLAGS(spirit).RemoveBits(MOB_DUAL_NATURE, MOB_FLAMEAURA, ENDBIT);
  MOB_FLAGS(spirit).SetBit(MOB_ASTRAL);
  // spiritdata->services--;
}

POWER(spirit_materialize)
{
  if (IS_DUAL(spirit)) {
    send_to_char(ch, "%s is already materialized.\r\n", CAP(GET_NAME(spirit)));
    return;
  }
  MOB_FLAGS(spirit).SetBit(MOB_DUAL_NATURE);
  MOB_FLAGS(spirit).RemoveBit(MOB_ASTRAL);
  // spiritdata->services--;
  act("$n takes on a physical form.", TRUE, spirit, 0, ch, TO_ROOM);
}

POWER(spirit_movement)
{
  int increase = 0;
  if (affected_by_power(spirit, MOVEMENTUP)) {
    act("$N stops providing that service.", FALSE, ch, 0, spirit, TO_CHAR);
    stop_spirit_power(spirit, MOVEMENTUP);
    return;
  }
  if (affected_by_power(spirit, MOVEMENTDOWN)) {
    act("$N stops providing that service.", FALSE, ch, 0, spirit, TO_CHAR);
    stop_spirit_power(spirit, MOVEMENTDOWN);
    return;
  }
  if (ROOM_FLAGGED(get_ch_in_room(spirit), ROOM_PEACEFUL)) {
    send_to_char("It's too peaceful here...\r\n", ch);
    return;
  }
  two_arguments(arg, buf, buf1);
  if (!(*buf1 || *buf)) {
    send_to_char("Do you want to increase or decrease the movement of the target?\r\n", ch);
    return;
  }
  if (is_abbrev(buf1, "increase"))
    increase = 1;
  else if (is_abbrev(buf1, "decrease"))
    increase = -1;
  if (!increase) {
    send_to_char("You must specify increase or decrease.\r\n", ch);
    return;
  }
  struct char_data *tch = get_char_room_vis(spirit, buf);
  if (!tch)
    send_to_char("Use movement against which target?\r\n", ch);
  else if (tch == spirit || affected_by_power(tch, increase > 0 ? MOVEMENTUP : MOVEMENTDOWN))
    send_to_char(ch, "The %s refuses to perform that service.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
  else {
    act("$n performs that service for you.", FALSE, spirit, 0, ch, TO_VICT);
    if (increase > 0)
      send_to_char("You feel something pushing you forward faster.\r\n", tch);
    else
      send_to_char("You face heavy resistance as you try and move forward.\r\n", tch);
    make_spirit_power(spirit, tch, increase > 0 ? MOVEMENTUP : MOVEMENTDOWN);
    spiritdata->services--;
  }
}

POWER(spirit_breath)
{
  struct char_data *tch = get_char_room_vis(spirit, arg);
  if (!tch)
    send_to_char("Use noxious breath against which target?\r\n", ch);
  else if (tch == ch)
    send_to_char("You cannot target yourself with that power.\r\n", ch);
  else if (tch == spirit)
    send_to_char(ch, "The %s refuses to harm itself.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
  else if (ROOM_FLAGGED(get_ch_in_room(spirit), ROOM_PEACEFUL))
    send_to_char("It's too peaceful here...\r\n", ch);
  else {
    act("$n turns towards $N as a cloud of noxious fumes forms around $S.", TRUE, spirit, 0, tch, TO_NOTVICT);
    act("$n lets forth a stream of noxious fumes in your direction.", FALSE, spirit, 0, tch, TO_VICT);
    int dam = convert_damage(stage(-success_test(MAX(GET_WIL(tch), GET_BOD(tch)), GET_SPARE2(spirit)), SERIOUS));
    if (dam > 0) {
      act("$n chokes and coughs.", TRUE, tch, 0, 0, TO_ROOM);
      send_to_char("The noxious fumes fill your throat and lungs, causing your vision to swim.\r\n", tch);
      damage(spirit, tch, dam, TYPE_FUMES, MENTAL);
    } else {
      act("$n waves $s hand in front of $s face, dispersing the fumes.", TRUE, tch, 0, 0, TO_ROOM);
      send_to_char("You easily resist the noxious fumes.\r\n", tch);
    }
    spiritdata->services--;
  }
}

POWER(spirit_attack)
{
  struct char_data *tch = get_char_room_vis(spirit, arg);
  if (!tch)
    send_to_char("Use attack which target?\r\n", ch);
  else if (tch == ch)
    send_to_char(ch, "Ordering your own %s to attack you is not a good idea.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
  else if (tch == spirit)
    send_to_char(ch, "The %s refuses to attack itself.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
  else if (ROOM_FLAGGED(get_ch_in_room(spirit), ROOM_PEACEFUL))
    send_to_char("It's too peaceful here...\r\n", ch);
  else {
    check_killer(ch, tch);
    set_fighting(spirit, tch);
    if (!FIGHTING(tch) && CAN_SEE(tch, spirit))
      set_fighting(tch, spirit);
    spiritdata->services--;
  }
}

POWER(spirit_psychokinesis)
{}

POWER(spirit_search)
{}

struct order_data services[] =
  {
    {"Appear", spirit_appear, 0
    },
    {"Sorcery", spirit_sorcery, 0},
    {"Study", spirit_study, 0},
    {"Sustain", spirit_sustain, 0},
    {"Accident", spirit_accident, 1}, // hostile
    {"Binding", spirit_binding, 1},  // hostile?
    {"Concealment", spirit_conceal, 1},
    {"Confusion", spirit_confusion, 0}, // hostile?
    {"Dematerialize", spirit_dematerialize, 1},
    {"Engulf", spirit_engulf, 1}, // hostile
    {"Fear", spirit_fear, 0}, // hostile
    {"Flame Aura", spirit_flameaura, 1}, // hostile
    {"Flamethrower", spirit_flamethrower, 1}, // hostile
    {"Guard", spirit_guard, 1},
    {"Leave", spirit_leave, 0},
    {"Materialize", spirit_materialize, 0},
    {"Movement", spirit_movement, 1},
    {"Breath", spirit_breath, 1}, // hostile
    {"Psychokinesis", spirit_psychokinesis, 1}, // hostile?
    {"Search", spirit_search, 1},
    {"Attack", spirit_attack, 1} // hostile
  };

ACMD(do_order)
{
  if (GET_ASPECT(ch) == ASPECT_SORCERER || GET_TRADITION(ch) == TRAD_ADEPT || GET_TRADITION(ch) == TRAD_MUNDANE || !GET_SKILL(ch, SKILL_CONJURING)) {
    send_to_char("You don't have the ability to do that.\r\n", ch);
    return;
  }
  half_chop(argument, buf, buf1);
  struct spirit_data *spirit;
  int i, order = 0;
  if (!(i = atoi(buf)) || i > GET_NUM_SPIRITS(ch)) {
    send_to_char(ch, "Which %s do you wish to give an order to?\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
    return;
  }

  // It's not worth parsing this whole thing out for changes. If they use the command, just consider their spirits dirty.
  GET_ELEMENTALS_DIRTY_BIT(ch) = TRUE;

  for (spirit = GET_SPIRIT(ch); spirit; spirit = spirit->next)
    if (--i == 0)
      break;
  if (!*buf1) {
    send_to_char("Available Services: \r\n", ch);
    if (GET_TRADITION(ch) == TRAD_HERMETIC) {
      if (!spirit->called)
        send_to_char("  ^WAppear^n\r\n", ch);
      else {
        send_to_char("  Aid ^WSorcery^n\r\n"
                     "  Aid ^WStudy^n\r\n"
                     "  ^WSustain^n Spell\r\n"
                     "  ^WMaterialize^n\r\n"
                     "  ^WDematerialize^n\r\n"
                     "  ^WLeave^n\r\n"
                     "  ^WEngulf^n\r\n", ch);
        switch(spirit->type) {
        case ELEM_FIRE:
          send_to_char("  ^WFlame Aura^n\r\n"
                       "  ^WFlamethrower^n\r\n", ch);
          break;
        case ELEM_WATER:
        case ELEM_EARTH:
          send_to_char("  ^WMovement^n\r\n", ch);
          break;
        case ELEM_AIR:
          send_to_char("  ^WMovement^n\r\n"
                       "  Noxious ^WBreath^n\r\n"
                       "  ^WPsychokinesis^n\r\n", ch);
          break;
        }
      }
    } else {
      send_to_char("  ^WMaterialize^n\r\n"
                   "  ^WDematerialize^n\r\n", ch);
      if (spirit_can_perform(spirit->type, SERV_ACCIDENT, TRAD_SHAMANIC))
        send_to_char("  ^WAccident^n\r\n", ch);
      if (spirit_can_perform(spirit->type, SERV_BINDING, TRAD_SHAMANIC))
        send_to_char("  ^WBinding^n\r\n", ch);
      if (spirit_can_perform(spirit->type, SERV_CONCEAL, TRAD_SHAMANIC))
        send_to_char("  ^WConcealment^n\r\n", ch);
      if (spirit_can_perform(spirit->type, SERV_CONFUSION, TRAD_SHAMANIC))
        send_to_char("  ^WConfusion^n\r\n", ch);
      if (spirit_can_perform(spirit->type, SERV_ENGULF, TRAD_SHAMANIC))
        send_to_char("  ^WEngulf^n\r\n", ch);
      if (spirit_can_perform(spirit->type, SERV_FEAR, TRAD_SHAMANIC))
        send_to_char("  ^WFear^n\r\n", ch);
      if (spirit_can_perform(spirit->type, SERV_GUARD, TRAD_SHAMANIC))
        send_to_char("  ^WGuard^n\r\n", ch);
      if (spirit_can_perform(spirit->type, SERV_MOVEMENT, TRAD_SHAMANIC))
        send_to_char("  ^WMovement^n\r\n", ch);
      if (spirit_can_perform(spirit->type, SERV_BREATH, TRAD_SHAMANIC))
        send_to_char("  Noxious ^WBreath^n\r\n", ch);
      if (spirit_can_perform(spirit->type, SERV_PSYCHOKINESIS, TRAD_SHAMANIC))
        send_to_char("  ^WPsychokinesis^n\r\n", ch);
      if (spirit_can_perform(spirit->type, SERV_SEARCH, TRAD_SHAMANIC))
        send_to_char("  ^WSearch^n\r\n", ch);
    }
    send_to_char("  Attack\r\n", ch);
  } else {
    half_chop(buf1, buf, buf2);
    for (;order < NUM_SERVICES; order++)
      if (is_abbrev(buf, services[order].name) && spirit_can_perform(spirit->type, order, GET_TRADITION(ch)))
        break;
    if (order == NUM_SERVICES) {
      send_to_char(ch, "Which service do you wish to order the %s to perform?\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
      return;
    }
    if (GET_TRADITION(ch) == TRAD_HERMETIC) {
      if (order == SERV_APPEAR) {
        if (spirit->called) {
          send_to_char(ch, "That %s is already here!\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
          return;
        } else {
          spirit_appear(ch, NULL, spirit, buf2);
          return;
        }
      } else if (!spirit->called) {
        send_to_char(ch, "That %s is waiting on the metaplanes.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
        return;
      }
    }
    struct char_data *mob = find_spirit_by_id(spirit->id, GET_IDNUM(ch));

    if (!mob) {
      if (order == SERV_LEAVE) {
        ((*services[order].func) (ch, mob, spirit, buf2));
        return;
      }

      send_to_char(ch, "That %s has been ensnared by forces you cannot control. Your only option is to release it.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
      snprintf(buf, sizeof(buf), "SYSERR: %s belonging to %s (%ld) has disappeared unexpectedly-- did someone purge it?", GET_TRADITION(ch) == TRAD_HERMETIC ? "Elemental" : "Spirit",
              GET_CHAR_NAME(ch), GET_IDNUM(ch));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      return;
    }

    if (services[order].type == 1 && MOB_FLAGGED(mob, MOB_ASTRAL)) {
      send_to_char(ch, "That %s must materialize before it can use that power.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
      return;
    }
    if (spirit->services < 1 && order != SERV_LEAVE) {
      send_to_char(ch, "The %s no longer listens to you.\r\n", GET_TRADITION(ch) == TRAD_HERMETIC ? "elemental" : "spirit");
      return;
    }
    ((*services[order].func) (ch, mob, spirit, buf2));
    if (order != SERV_LEAVE)
      elemental_fulfilled_services(ch, mob, spirit);
  }
}

ACMD(do_domain)
{
  if (GET_TRADITION(ch) != TRAD_SHAMANIC) {
    send_to_char("You have no sense of domain.\r\n", ch);
    return;
  }
  struct room_data *temp_room = get_ch_in_room(ch);
  if (!*argument) {
    send_to_char(ch, "You are currently in the %s domain.\r\n", spirits[GET_DOMAIN(ch)].name);
    if (SECT(temp_room) != SPIRIT_FOREST && SECT(temp_room) != SPIRIT_HEARTH && !ROOM_FLAGGED(temp_room, ROOM_INDOORS)) {
      send_to_char(ch, "You can switch to the following domains:\r\n  %s\r\n  Sky Spirit\r\n", spirits[SECT(temp_room)].name);
    }
  } else {
    struct spirit_data *next;
    skip_spaces(&argument);
    int newdomain = -1;
    if (SECT(temp_room) != SPIRIT_FOREST && SECT(temp_room) != SPIRIT_HEARTH && !ROOM_FLAGGED(get_ch_in_room(ch), ROOM_INDOORS)) {
      if (is_abbrev(argument, spirits[SECT(temp_room)].name))
        newdomain = SECT(temp_room);
      else if (is_abbrev(argument, "sky spirit"))
        newdomain = SPIRIT_SKY;
    }
    if (newdomain == GET_DOMAIN(ch))
      send_to_char("You are already focusing on that domain.\r\n", ch);
    else if (newdomain == -1)
      send_to_char("Which domain do you wish to focus on?\r\n", ch);
    else {
      GET_DOMAIN(ch) = newdomain;
      send_to_char(ch, "You switch your focus to the %s domain.\r\n", GET_DOMAIN(ch) == SPIRIT_SKY ? "Sky spirit" : spirits[GET_DOMAIN(ch)].name);
      for (struct spirit_data *spirit = GET_SPIRIT(ch); spirit; spirit = next) {
        next = spirit->next;
        if (GET_DOMAIN(ch) != spirit->type) {
          struct spirit_data *temp;
          struct char_data *tch = find_spirit_by_id(spirit->id, GET_IDNUM(ch));
          if (tch) {
            send_to_char(ch, "You release %s from the rest of its services.\r\n", GET_NAME(tch));
            act("$n returns to the metaplanes.", TRUE, tch, 0, ch, TO_NOTVICT);
            extract_char(tch);
          }
          REMOVE_FROM_LIST(spirit, GET_SPIRIT(ch), next);
          GET_NUM_SPIRITS(ch)--;
          delete spirit;
          GET_ELEMENTALS_DIRTY_BIT(ch) = TRUE;
        }
      }
    }
  }
}

void deactivate_power(struct char_data *ch, int power)
{
  if (GET_POWER_ACT(ch, power) == 0)
    return;
  GET_POWER_ACT(ch, power) = 0;
  switch (power) {
    case ADEPT_PERCEPTION:
      PLR_FLAGS(ch).RemoveBit(PLR_PERCEIVE);
      send_to_char("You return to your physical senses.\r\n", ch);
      break;
    case ADEPT_LOW_LIGHT:
    case ADEPT_THERMO:
      switch (GET_RACE(ch)) {
        case RACE_HUMAN:
        case RACE_OGRE:
          NATURAL_VISION(ch) = NORMAL;
          break;
        case RACE_DWARF:
        case RACE_GNOME:
        case RACE_MENEHUNE:
        case RACE_KOBOROKURU:
        case RACE_TROLL:
        case RACE_CYCLOPS:
        case RACE_FOMORI:
        case RACE_GIANT:
        case RACE_MINOTAUR:
          NATURAL_VISION(ch) = THERMOGRAPHIC;
          break;
        case RACE_ORK:
        case RACE_HOBGOBLIN:
        case RACE_SATYR:
        case RACE_ONI:
        case RACE_ELF:
        case RACE_WAKYAMBI:
        case RACE_NIGHTONE:
        case RACE_DRYAD:
          NATURAL_VISION(ch) = LOWLIGHT;
          break;
        }
      break;
    case ADEPT_LIVINGFOCUS:
      if (GET_SUSTAINED_NUM(ch))
        adept_release_spell(ch, FALSE);
      break;
  }
}

ACMD(do_powerdown)
{
  if (GET_TRADITION(ch) != TRAD_ADEPT) {
    nonsensical_reply(ch, NULL, "standard");
    return;
  }
  for (int i = 0; i < ADEPT_NUMPOWER; i++)
    deactivate_power(ch, i);
  GET_POWER_POINTS(ch) = 0;
  affect_total(ch);
  send_to_char("You totally deactivate all your powers.\r\n", ch);
}

ACMD(do_deactivate)
{
  struct obj_data *obj;
  int i;

  if (GET_TRADITION(ch) == TRAD_ADEPT && !str_str(argument, "pain editor") && !str_str(argument, "voice modulator")) {
    char name[120], tokens[MAX_STRING_LENGTH], *s;
    extern int ability_cost(int abil, int level);
    strncpy(tokens, argument, sizeof(tokens) - 1);
    if ((strtok(tokens, "\"") && (s = strtok(NULL, "\"")))
        || (strtok(tokens, "'") && (s = strtok(NULL, "'")))) {
      strncpy(name, s, sizeof(name) - 1);
      if ((s = strtok(NULL, "\0"))) {
        skip_spaces(&s);
        strncpy(buf1, s, sizeof(buf1) - 1);
      } else
        *buf1 = '\0';
      one_argument(argument, buf);
    } else {
      skip_spaces(&argument);
      strncpy(name, argument, sizeof(name) - 1);
    }

    // Find powers they have skill in first.
    for (i = 0; i < ADEPT_NUMPOWER; i++)
      if (GET_POWER_TOTAL(ch, i) && is_abbrev(name, adept_powers[i]))
        break;
    // Find powers they don't have skill in.
    if (i >= ADEPT_NUMPOWER)
      for (i = 0; i < ADEPT_NUMPOWER; i++)
        if (is_abbrev(name, adept_powers[i]))
          if (!GET_POWER_TOTAL(ch, i)) {
            send_to_char(ch, "You haven't learned the %s power yet.\r\n", adept_powers[i]);
            return;
          }

    if (i < ADEPT_NUMPOWER) {
      if (!GET_POWER_ACT(ch, i))
        send_to_char(ch, "You don't have %s activated.\r\n", adept_powers[i]);
      else {
        int total = 0;
        for (int q = GET_POWER_ACT(ch, i); q > 0; q--)
          total += ability_cost(i, q);
        GET_POWER_POINTS(ch) -= total;
        deactivate_power(ch, i);
        affect_total(ch);
        send_to_char(ch, "You completely deactivate your %s power.\r\n", adept_powers[i]);
      }
      return;
    }
  }
  skip_spaces(&argument);

  if (is_abbrev(argument, "pain editor")) {
    for (obj = ch->bioware; obj; obj = obj->next_content)
      if (GET_OBJ_VAL(obj, 0) == BIO_PAINEDITOR) {
        if (!GET_OBJ_VAL(obj, 3))
          send_to_char("Your Pain Editor isn't activated.\r\n", ch);
        else {
          GET_OBJ_VAL(obj, 3) = 0;
          send_to_char("Your body starts to tingle as your perception of touch returns.\r\n", ch);
          update_pos(ch);
        }
        return;
      }
  }

  if (is_abbrev(argument, "voice modulator")) {
    for (obj = ch->cyberware; obj; obj = obj->next_content)
      if (GET_OBJ_VAL(obj, 0) == CYB_VOICEMOD) {
        if (!GET_OBJ_VAL(obj, 3))
          send_to_char("Your Voice Modulator isn't activated.\r\n", ch);
        else {
          GET_OBJ_VAL(obj, 3) = 0;
          send_to_char("Your voice is no longer being masked.\r\n", ch);
        }
        return;
      }
  }

  if (!(obj = get_object_in_equip_vis(ch, argument, ch->equipment, &i)) &&
      !(obj = get_obj_in_list_vis(ch, argument, ch->carrying))) {
    send_to_char("Deactivate which focus or power?\r\n", ch);
    return;
  }
  if (GET_OBJ_TYPE(obj) == ITEM_FOCUS) {
    if (GET_FOCUS_ACTIVATED(obj) < 1)
      send_to_char(ch, "%s isn't activated.\r\n", GET_OBJ_NAME(obj));
    else {
      if (GET_FOCUS_TYPE(obj) == FOCI_SUSTAINED) {
        send_to_char(ch, "The only way to deactivate %s is to release the spell it's sustaining.\r\n", GET_OBJ_NAME(obj));
        return;
      }
      GET_OBJ_VAL(obj, 4) = 0;
      GET_FOCI(ch)--;
      affect_total(ch);
      send_to_char(ch, "You deactivate %s.\r\n", GET_OBJ_NAME(obj));
    }
  } else if (GET_OBJ_TYPE(obj) == ITEM_MONEY && GET_OBJ_VAL(obj, 1) && GET_OBJ_VAL(obj, 4) == GET_IDNUM(ch)) {
    GET_OBJ_VAL(obj, 3) = GET_OBJ_VAL(obj, 4) = GET_OBJ_VAL(obj, 5) = 0;
    send_to_char(ch, "You deactivate %s.\r\n", GET_OBJ_NAME(obj));
  } else send_to_char(ch, "You can't deactivate %s.\r\n", GET_OBJ_NAME(obj));
}

ACMD(do_destroy)
{
  if (!*argument) {
    send_to_char("Destroy which magic construct?\r\n", ch);
    return;
  }
  skip_spaces(&argument);
  struct obj_data *obj;
  if (ch->in_veh || !(obj = get_obj_in_list_vis(ch, argument, ch->in_room->contents))) {
    send_to_char(ch, "'%s' isn't here.\r\n", argument);
    return;
  }
  if (GET_OBJ_TYPE(obj) == ITEM_MAGIC_TOOL && (GET_OBJ_VAL(obj, 0) == TYPE_LODGE || GET_OBJ_VAL(obj, 0) == TYPE_CIRCLE)) {
    if (GET_OBJ_VAL(obj, 0) == TYPE_LODGE) {
      send_to_char(ch, "You kick at the supports and smash the talismans.\r\n");
      act("$n roughly pulls down $p.", TRUE, ch, obj, 0, TO_ROOM);
    } else {
      send_to_char(ch, "You rub your feet along the lines making up the hermetic circle, erasing them.\r\n");
      act("$n uses $s feet to rub out $p.", TRUE, ch, obj, 0, TO_ROOM);
    }
    extract_obj(obj);
  } else
    send_to_char(ch, "You can't destroy %s. Maybe pick it up and JUNK it?\r\n", GET_OBJ_NAME(obj));
}

ACMD(do_track)
{
  if (!IS_PROJECT(ch)) {
    send_to_char("You have to be projecting to astrally track.\r\n", ch);
    return;
  }
  if (AFF_FLAGGED(ch->desc->original, AFF_TRACKING)) {
    AFF_FLAGS(ch->desc->original).RemoveBit(AFF_TRACKING);
    send_to_char("You stop searching the astral plane.\r\n", ch);
    return;
  }
  int success;
  skip_spaces(&argument);
  if (!*argument) {
    if (HUNTING(ch->desc->original)) {
      success = success_test(MAX(GET_INT(ch), GET_MAG(ch)/100), HOURS_SINCE_TRACK(ch->desc->original));
      if (success > 0) {
        AFF_FLAGS(ch->desc->original).SetBit(AFF_TRACKING);
        send_to_char("You pick up the trail and continue tracking the astral signature.\r\n", ch);
      } else {
        HUNTING(ch->desc->original) = NULL;
        send_to_char("You seem to have lost the trail.\r\n", ch);
      }
    } else
      send_to_char("What do you wish to track the owner of?\r\n", ch);
    return;
  }
  struct char_data *vict;
  struct obj_data *obj;
  two_arguments(argument, buf, buf1);
  if (!generic_find(buf,  FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP |
                    FIND_CHAR_ROOM, ch, &vict, &obj)) {
    send_to_char(ch, "You don't see anything named '%s' here.\r\n", buf);
    return;
  }
  if (vict) {
    if (*buf1) {
      obj = NULL;
      two_arguments(buf1, buf2, argument);
      int i = 0;
      if (!str_cmp(buf1, "spell")) {
        int spell = 0;
        if (*buf2)
          spell = atoi(buf2);
        for (struct sustain_data *sust = GET_SUSTAINED(vict); sust; sust = sust->next)
          if (!sust->caster && !spell--) {
            vict = sust->other;
            break;
          }
      } else if (!(obj = get_object_in_equip_vis(vict, buf1, vict->equipment, &i))) {
        send_to_char("They aren't wearing that.\r\n", ch);
        return;
      }
    } else if (IS_NPC(vict) && (GET_RACE(vict) == RACE_SPIRIT || GET_RACE(vict) == RACE_ELEMENTAL) && GET_ACTIVE(vict)) {
      for (struct descriptor_data *desc = descriptor_list; desc; desc = desc->next)
        if (desc->original && GET_IDNUM(desc->original) == GET_ACTIVE(vict)) {
          vict = desc->original;
          break;
        } else if (desc->character && GET_IDNUM(desc->character) == GET_ACTIVE(vict)) {
          vict = desc->character;
          break;
        }
    } else {
      send_to_char("They are right here!\r\n", ch);
      return;
    }
  }
  if (obj) {
    vict = NULL;
    if (GET_OBJ_TYPE(obj) != ITEM_FOCUS
        && !(GET_OBJ_TYPE(obj) == ITEM_MAGIC_TOOL
        && (GET_OBJ_VAL(obj, 0) == TYPE_CIRCLE || GET_OBJ_VAL(obj, 0) == TYPE_LODGE))) {
      send_to_char(ch, "There is no astral signature present on %s.\r\n", GET_OBJ_NAME(obj));
      return;
    }
    int num = 3;
    if (GET_OBJ_TYPE(obj) == ITEM_FOCUS)
      num = 2;
    if (!GET_OBJ_VAL(obj, num)) {
      send_to_char(ch, "There is no astral signature present on %s.\r\n", GET_OBJ_NAME(obj));
      return;
    }
    for (struct descriptor_data *desc = descriptor_list; desc; desc = desc->next)
      if (desc->original && GET_IDNUM(desc->original) == GET_OBJ_VAL(obj, num)) {
        vict = desc->original;
        break;
      } else if (desc->character && GET_IDNUM(desc->character) == GET_OBJ_VAL(obj, num)) {
        vict = desc->character;
        break;
      }
  }
  success = success_test(GET_INT(ch), 4);
  if (vict && success > 0) {
    if (get_ch_in_room(vict) == get_ch_in_room(ch)) {
      send_to_char("They're right here!\r\n", ch);
      return;
    }
    HUNTING(ch->desc->original) = vict;
    AFF_FLAGS(ch->desc->original).SetBit(AFF_TRACKING);
    HOURS_SINCE_TRACK(ch->desc->original) = 0;
    HOURS_LEFT_TRACK(ch->desc->original) = MAX(1, 4 / success);
    AFF_FLAGS(vict).SetBit(AFF_TRACKED);
    send_to_char("You begin to trace that astral signature back to its owner.\r\n", ch);
  } else {
    send_to_char("You can't seem to track them down.\r\n", ch);
    return;
  }
}

ACMD(do_manifest)
{
  if (!IS_PROJECT(ch)) {
    send_to_char("You can't manifest when you're not astrally projecting.\r\n", ch);
    return;
  }
  AFF_FLAGS(ch).ToggleBit(AFF_MANIFEST);
  if (AFF_FLAGGED(ch, AFF_MANIFEST)) {
    act("$n fades slowly into view.", TRUE, ch, 0, 0, TO_ROOM);
    send_to_char("You manifest your presence on the physical plane.\r\n", ch);
  } else {
    act("$n fades from the physical plane.", FALSE, ch, 0, 0, TO_ROOM);
    send_to_char("You vanish back into the astral plane.\r\n", ch);
  }
}

ACMD(do_dispell)
{
  if (GET_ASPECT(ch) == ASPECT_CONJURER || GET_TRADITION(ch) == TRAD_ADEPT || GET_TRADITION(ch) == TRAD_MUNDANE || !GET_SKILL(ch, SKILL_SORCERY)) {
    send_to_char("You don't have the ability to do that.\r\n", ch);
    return;
  }
  if (!*argument) {
    send_to_char("What spell do you wish to dispell?\r\n", ch);
    return;
  }
  skip_spaces(&argument);
  struct char_data *vict;
  two_arguments(argument, buf, buf2);
  if (!(vict = get_char_room_vis(ch, buf))) {
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf);
    return;
  }
  int x = atoi(buf2);
  if (x <= 0) {
    send_to_char("Dispell which spell?\r\n", ch);
    return;
  }
  struct sustain_data *sust = GET_SUSTAINED(vict);
  for (;sust; sust = sust->next)
    if (!sust->caster && !--x)
      break;
  if (!sust) {
    send_to_char("They don't have that many spells cast on them.\r\n", ch);
    return;
  }
  int success = success_test(GET_SKILL(ch, SKILL_SORCERY) + MIN(GET_SKILL(ch, SKILL_SORCERY), GET_CASTING(ch)), sust->force);
  int type = sust->spell, force = sust->force;
  snprintf(buf, sizeof(buf), "Dispell $N's %s (force %d) using skill %d vs TN %d: %d successes.",
          spells[sust->spell].name,
          sust->force,
          GET_SKILL(ch, SKILL_SORCERY) + MIN(GET_SKILL(ch, SKILL_SORCERY), GET_CASTING(ch)),
          sust->force,
          success);
  act(buf, 0, ch, 0, vict, TO_ROLLS);
  if (success > 0) {
    spell_modify(vict, sust, FALSE);
    sust->success = MAX(sust->success - success, 0); // Since it's unsigned, we have to do shit like this.
    snprintf(buf, sizeof(buf), "Remaining successes on spell: %d.", sust->success);
    act(buf, 0, ch, 0, vict, TO_ROLLS);
    if (sust->success < 1) {
      send_to_char("You succeed in completely dispelling that spell.\r\n", ch);
      sust->success += success;
      spell_modify(vict, sust, TRUE);
      end_sustained_spell(vict, sust);
    } else {
      spell_modify(vict, sust, TRUE);
      send_to_char("You succeed in weakening that spell.\r\n", ch);
    }
  } else
    send_to_char("You fail to weaken that spell.\r\n", ch);
  spell_drain(ch, type, force, 0);
}

ACMD(do_heal)
{
  if (GET_TRADITION(ch) != TRAD_ADEPT || !GET_POWER(ch, ADEPT_EMPATHICHEAL)) {
    send_to_char("You don't have the ability to do that.\r\n", ch);
    return;
  }
  struct char_data *vict;
  skip_spaces(&argument);
  if (!*argument) {
    send_to_char("Who do you wish to heal?\r\n", ch);
    return;
  }
  two_arguments(argument, arg, buf);
  if (!*buf)
    send_to_char("What damage level do you wish to heal them at?\r\n", ch);
  else if (GET_POS(ch) == POS_FIGHTING)
    send_to_char(TOOBUSY, ch);
  else if (!(vict = get_char_room_vis(ch, arg)))
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
  else if (GET_PHYSICAL(ch) <= 100)
    send_to_char("Succeeding in that task would surely kill you.\r\n", ch);
  else if (GET_PHYSICAL(vict) == GET_MAX_PHYSICAL(vict))
    send_to_char("They don't need your help.\r\n", ch);
  else if (GET_POS(vict) > POS_LYING)
    send_to_char("They have to be lying down to receive your help.\r\n", ch);
  else {
    int basedamage = 0;
    for (; *wound_name[basedamage] != '\n'; basedamage++)
      if (is_abbrev(buf, wound_name[basedamage]))
        break;
    if (basedamage > 4 || basedamage == 0) {
      send_to_char(ch, "'%s' is not a valid damage level, please choose between Light, Moderate, Serious and Deadly.\r\n", capitalize(buf));
      return;
    }
    int success = success_test(GET_MAG(ch) / 100, 10 - (GET_ESS(vict) / 100));
    if (success < 1) {
      send_to_char("You fail to channel your energy into that pursuit.\r\n", ch);
      return;
    }
    success = MIN(damage_array[basedamage], MIN((GET_PHYSICAL(ch) / 100) - 1, success)) * 100;
    success = MIN(GET_MAX_PHYSICAL(vict) - GET_PHYSICAL(vict), success);
    GET_PHYSICAL(vict) += success;
    GET_PHYSICAL(ch) -= success;
    WAIT_STATE(ch, 3 RL_SEC);
    act("You feel $n place $s hands on you, $s minstrations seem to cause your wounds to fade!", TRUE, ch, 0, vict, TO_VICT);
    act("You place your hands on $N, you feel $S pain and suffering transferred to your body!", TRUE, ch, 0, vict, TO_CHAR);
    act("$n places $s hands on $N seemingly transferring the wound to $mself!", TRUE, ch, 0, vict, TO_NOTVICT);
    update_pos(vict);
    update_pos(ch);
  }
}

ACMD(do_relieve)
{
  if (GET_TRADITION(ch) != TRAD_ADEPT || !GET_POWER(ch, ADEPT_PAINRELIEF)) {
    send_to_char("You don't have the ability to do that.\r\n", ch);
    return;
  }
  struct char_data *vict;
  skip_spaces(&argument);
  if (!*argument) {
    send_to_char("Who do you wish to heal?\r\n", ch);
    return;
  }
  if (GET_POS(ch) == POS_FIGHTING)
    send_to_char(TOOBUSY, ch);
  else if (!(vict = get_char_room_vis(ch, argument)))
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", argument);
  else if (GET_MENTAL(vict) == GET_MAX_MENTAL(vict))
    send_to_char("They don't need your help.\r\n", ch);
  else if (GET_POS(vict) > POS_LYING)
    send_to_char("They have to be lying down to receive your help.\r\n", ch);
  else {
    int success = success_test(GET_MAG(ch) / 100, 10 - (GET_ESS(vict) / 100));
    if (success < 1) {
      send_to_char("You fail to channel your energy into that pursuit.\r\n", ch);
      return;
    }
    if (GET_MENTAL(vict) <= 0)
      GET_MENTAL(vict) = (int)(GET_MAX_MENTAL(vict) * 0.4);
    else if (GET_MENTAL(vict) <= GET_MAX_MENTAL(vict) * 0.4)
      GET_MENTAL(vict) = (int)(GET_MAX_MENTAL(vict) * 0.7);
    else if (GET_MENTAL(vict) <= GET_MAX_MENTAL(vict) * 0.7)
      GET_MENTAL(vict) = (int)(GET_MAX_MENTAL(vict) * 0.9);
    else
      GET_MENTAL(vict) = GET_MAX_MENTAL(vict);
    act("You feel $n place $s hands on you, a tingling feeling fills your body as your pain begins to subside", TRUE, ch, 0, vict, TO_VICT);
    act("Your hands warm slightly at the energy being channeled through them into $N's chakra points.", TRUE, ch, 0, vict, TO_CHAR);
    act("$n places $s hands on $N. $N appears to feel better.", TRUE, ch, 0, vict, TO_NOTVICT);
    WAIT_STATE(ch, 5 RL_SEC);
  }
}


ACMD(do_nervestrike)
{
  if (GET_TRADITION(ch) != TRAD_ADEPT) {
    send_to_char("You don't have the ability to do that.\r\n", ch);
    return;
  }
  if (!GET_POWER(ch, ADEPT_NERVE_STRIKE)) {
    send_to_char("You'll have to activate the Nerve Strike power first.\r\n", ch);
    return;
  }
  if (IS_NERVE(ch)) {
    IS_NERVE(ch) = FALSE;
    send_to_char("You will no longer use nerve strike.\r\n", ch);
  } else {
    IS_NERVE(ch) = TRUE;
    send_to_char("You will now use nerve strike.\r\n", ch);
  }
}
#define CH d->character

void disp_init_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char("1) Increase magic and learn metamagic technique\r\n"
               "2) Increase magic and change astral signature\r\n"
               "3) Shed a geas\r\n"
               "4) Return to game\r\n"
               "Enter initiation option: ", CH);
  d->edit_mode = INIT_MAIN;
}

bool can_metamagic(struct char_data *ch, int i)
{
  if (GET_TRADITION(ch) == TRAD_ADEPT) {
    if (i != META_CENTERING && i != META_MASKING)
      return FALSE;
    if (i == META_CENTERING && GET_METAMAGIC(ch, i) == 2)
      return TRUE;
  }
  if (GET_METAMAGIC(ch, i))
    return FALSE;
  if (GET_ASPECT(ch) == ASPECT_CONJURER && (i == META_QUICKENING || i == META_REFLECTING || i == META_ANCHORING || i == META_SHIELDING))
    return FALSE;
  if (GET_ASPECT(ch) == ASPECT_SORCERER && i == META_INVOKING)
    return FALSE;
  return TRUE;
}

void disp_geas_menu(struct descriptor_data *d)
{
  CLS(CH);
  int x = 0;
  for (int i = 0; i < NUM_WEARS; i++)
    if (GET_EQ(CH, i)
        && GET_OBJ_TYPE(GET_EQ(CH, i)) == ITEM_FOCUS
        && GET_OBJ_VAL(GET_EQ(CH, i), 9) == GET_IDNUM(CH))
      send_to_char(CH, "%d) %s\r\n", x++, GET_OBJ_NAME(GET_EQ(CH, i)));
  send_to_char("q) Quit Initiation\r\nSelect geas to shed: ", CH);
  d->edit_mode = INIT_GEAS;
}

void disp_meta_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int i = 0; i < META_MAX; i++)
    send_to_char(CH, "%d) %s%s^n\r\n", i + 1, can_metamagic(CH, i) ? "" : "^r", metamagic[i]);
  send_to_char("q) Quit Initiation\r\nSelect ability to learn: ", CH);
  d->edit_mode = INIT_META;
}

bool init_cost(struct char_data *ch, bool spend)
{
  int desired_grade = GET_GRADE(ch) + 1;
  long karmacost = (desired_grade + 5) * 300;
  long nuyencost;

  if (desired_grade >= 6)
    nuyencost = 825000;
  else
    nuyencost = 25000 + (25000 * (1 << GET_GRADE(ch)));

  if (karmacost < 0) {
    send_to_char("You broke it! You can't initiate until the code is fixed to allow someone at your rank to do so.\r\n", ch);
    mudlog("lol init capped, karma overflowed... this game was not designed for that init level", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  long tke = 0;
  if (karmacost > GET_KARMA(ch) || (GET_KARMA(ch) - karmacost) > GET_KARMA(ch)) {
    send_to_char(ch, "You do not have enough karma to initiate. It will cost you %d karma.\r\n", (int) (karmacost / 100));
    return FALSE;
  }
  if (nuyencost > GET_NUYEN(ch) || (GET_NUYEN(ch) - nuyencost) > GET_NUYEN(ch)) {
    send_to_char(ch, "You do not have enough nuyen to initiate. It will cost you %d nuyen.\r\n", nuyencost);
    return FALSE;
  }

  switch (desired_grade) {
    case 1:
      tke = 0;
      break;
    case 2:
      tke = 100;
      break;
    case 3:
      tke = 200;
      break;
    default:
      tke = 200 + ((GET_GRADE(ch)-2)*200);
      break;
  }
  if (tke > GET_TKE(ch)) {
    send_to_char(ch, "You do not have high enough TKE to initiate. You need %d TKE.\r\n", tke);
    return FALSE;
  }
  if (spend) {
    lose_nuyen(ch, nuyencost, NUYEN_OUTFLOW_INITIATION);
    GET_KARMA(ch) -= karmacost;
  }
  return TRUE;
}

ACMD(do_subpoint)
{
  struct char_data *vict;
  skip_spaces(&argument);
  if (!(vict = get_char_vis(ch, argument)))
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", argument);
  else if (GET_TRADITION(vict) != TRAD_ADEPT)
    send_to_char("You can only use this command on Adepts.\r\n", ch);
  else if (GET_PP(vict) <= 0)
    send_to_char("They do not have any power points remaining.\r\n", ch);
  else {
    send_to_char(ch, "You remove a power point from %s.\r\n", GET_CHAR_NAME(vict));
    GET_PP(vict) -= 100;
  }
}

ACMD(do_initiate)
{
  if (GET_TRADITION(ch) == TRAD_MUNDANE)
    nonsensical_reply(ch, NULL, "standard");
  else if (subcmd == SCMD_INITIATE && init_cost(ch, FALSE)) {
    STATE(ch->desc) = CON_INITIATE;
    PLR_FLAGS(ch).SetBit(PLR_INITIATE);
    disp_init_menu(ch->desc);
  } else if (subcmd == SCMD_POWERPOINT) {
    if (GET_TRADITION(ch) != TRAD_ADEPT) {
      nonsensical_reply(ch, NULL, "standard");
      return;
    }

    if (GET_KARMA(ch) < 2000) {
      send_to_char("You do not have enough karma to purchase a powerpoint. It costs 20 karma.\r\n", ch);
      return;
    }

    if (ch->points.extrapp > (int)(GET_TKE(ch) / 50)) {
      send_to_char(ch, "You haven't earned enough TKE to purchase %s powerpoint yet. You need at least %d.\r\n", ch->points.extrapp ? "another" : "a", 50 * ch->points.extrapp);
      return;
    }

    GET_KARMA(ch) -= 2000;
    GET_PP(ch) += 100;
    ch->points.extrapp++;
    send_to_char(ch, "You have purchased an additional powerpoint.\r\n");
  }
}
void init_parse(struct descriptor_data *d, char *arg)
{
  struct obj_data *obj = NULL;
  int number, i;
  switch (d->edit_mode)
  {
    case INIT_MAIN:
      switch (*arg)
      {
        case '1':
          disp_meta_menu(d);
          break;
        case '2':
          STATE(d) = CON_PLAYING;
          init_cost(CH, TRUE);
          GET_SIG(CH)++;
          GET_GRADE(CH)++;
          GET_REAL_MAG(CH) += 100;
          if (GET_TRADITION(CH) == TRAD_ADEPT)
            GET_PP(CH) += 100;
          send_to_char("You feel your astral reflection shift and mold itself closer to the astral plane.\r\n", CH);
          STATE(d) = CON_PLAYING;
          PLR_FLAGS(CH).RemoveBit(PLR_INITIATE);
          break;
        case '3':
          disp_geas_menu(d);
          break;
        case '4':
          STATE(d) = CON_PLAYING;
          PLR_FLAGS(CH).RemoveBit(PLR_INITIATE);
          send_to_char("Initiation cancelled.\r\n", CH);
          break;
        default:
          disp_init_menu(d);
          break;
      }
      break;
    case INIT_GEAS:
      number = atoi(arg);
      if (*arg == 'q') {
        STATE(d) = CON_PLAYING;
        send_to_char("Initiation cancelled.\r\n", CH);
      } else if (number < 0) {
        send_to_char("Invalid Response. Select geas to shed: ", CH);
      } else {
        for (i = 0; i < NUM_WEARS; i++)
          if (GET_EQ(CH, i)
              && GET_OBJ_TYPE(GET_EQ(CH, i)) == ITEM_FOCUS
              && GET_OBJ_VAL(GET_EQ(CH, i), 9) == GET_IDNUM(CH)
              && --number < 0) {
            obj = GET_EQ(CH, i);
            break;
          }
        if (!obj) {
          send_to_char("You don't have that many geasa. Select geas to shed: ", CH);
          return;
        }
        init_cost(CH, TRUE);
        GET_GRADE(CH)++;
        GET_REAL_MAG(CH) += 100;
        GET_OBJ_VAL(obj, 9) = 0;
        if (GET_TRADITION(CH) == TRAD_ADEPT)
          GET_PP(CH) += 100;
        send_to_char(CH, "You feel your magic return from that object, once again binding to your spirit.\r\n", CH);
        STATE(d) = CON_PLAYING;
        PLR_FLAGS(CH).RemoveBit(PLR_INITIATE);
      }
      break;
    case INIT_META:
      number = atoi(arg);
      if (!number) {
        STATE(d) = CON_PLAYING;
        send_to_char("Initiation cancelled.\r\n", CH);
      } else if (--number > META_MAX) {
        send_to_char("Invalid Response. Select ability to learn: ", CH);
      } else if (!can_metamagic(CH, number)) {
        send_to_char("Your tradition/apect/initiation combination isn't able to learn that metamagic technique. Select ability to learn: ", CH);
      } else {
        init_cost(CH, TRUE);
        SET_METAMAGIC(CH, number, GET_METAMAGIC(CH, number) + 1);
        GET_GRADE(CH)++;
        GET_REAL_MAG(CH) += 100;
        if (GET_TRADITION(CH) == TRAD_ADEPT)
          GET_PP(CH) += 100;
        send_to_char(CH, "You feel yourself grow closer to the astral plane as you become ready to learn %s.\r\n", metamagic[number]);
        STATE(d) = CON_PLAYING;
        PLR_FLAGS(CH).RemoveBit(PLR_INITIATE);
      }
      break;

  }
}

ACMD(do_masking)
{
  if (GET_METAMAGIC(ch, META_MASKING) < 2) {
    nonsensical_reply(ch, NULL, "standard");
    return;
  }
  skip_spaces(&argument);
  if (!*argument) {
    if (!GET_MASKING(ch))
      send_to_char("You aren't masking anything.\r\n", ch);
    else {
      send_to_char("You are masking: \r\n", ch);
      if (IS_SET(GET_MASKING(ch), MASK_COMPLETE))
        send_to_char("  Awakened State\r\n", ch);
      if (IS_SET(GET_MASKING(ch), MASK_INIT))
        send_to_char("  Initiation Grades\r\n", ch);
      if (IS_SET(GET_MASKING(ch), MASK_DUAL))
        send_to_char("  Perceiving State\r\n", ch);
    }
  } else if (is_abbrev(argument, "initiation")) {
    TOGGLE_BIT(GET_MASKING(ch), MASK_INIT);
    send_to_char(ch, "You will %s mask your initiation grades.\r\n", IS_SET(GET_MASKING(ch), MASK_INIT) ? "now" : "no longer");
  } else if (is_abbrev(argument, "complete")) {
    TOGGLE_BIT(GET_MASKING(ch), MASK_COMPLETE);
    send_to_char(ch, "You will %s mask your awakened state.\r\n", IS_SET(GET_MASKING(ch), MASK_COMPLETE) ? "now" : "no longer");
  } else if (is_abbrev(argument, "dual")) {
    TOGGLE_BIT(GET_MASKING(ch), MASK_DUAL);
    send_to_char(ch, "You will %s mask your perceiving state.\r\n", IS_SET(GET_MASKING(ch), MASK_DUAL) ? "now" : "no longer");
  } else send_to_char("What do you wish to mask?\r\n", ch);
}

ACMD(do_focus)
{
  if (GET_TRADITION(ch) != TRAD_ADEPT || !GET_POWER(ch, ADEPT_LIVINGFOCUS)) {
    nonsensical_reply(ch, NULL, "standard");
    return;
  }
  skip_spaces(&argument);
  if (!*argument) {
    if (GET_SUSTAINED_NUM(ch)) {
      send_to_char("You are already sustaining a spell.\r\n", ch);
      return;
    }
    struct sustain_data *spell = GET_SUSTAINED(ch);
    for (; spell; spell = spell->next)
      if (!spell->caster && !spell->spirit && !spell->focus && spells[spell->spell].duration == SUSTAINED)
        break;
    if (!spell) {
      send_to_char("You are not affected by any spells that need sustaining.\r\n", ch);
      return;
    }
    spell->spirit = ch;
    GET_SUSTAINED_NUM(ch)++;
    GET_SUSTAINED_FOCI(spell->other)++;
    for (struct sustain_data *ospell = GET_SUSTAINED(spell->other); ospell; ospell = ospell->next)
      if (ospell->idnum == spell->idnum && ospell->other == ch) {
        ospell->spirit = ch;
        break;
      }
    strcpy(buf, spells[spell->spell].name);
    if (spell->spell == SPELL_INCATTR || spell->spell == SPELL_INCCYATTR ||
        spell->spell == SPELL_DECATTR || spell->spell == SPELL_DECCYATTR)
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " (%s)", attributes[spell->subtype]);
    send_to_char(ch, "You begin to concentrate on sustaining %s.\r\n", buf);
    send_to_char(spell->other, "You feel a weight lifted from you as someone takes over sustaining your %s spell.\r\n", buf);
  } else if (!str_cmp(argument, "release")) {
    if (!GET_SUSTAINED_NUM(ch)) {
      send_to_char("You aren't sustaining any spells.\r\n", ch);
      return;
    }
    adept_release_spell(ch, FALSE);
  } else if (!str_cmp(argument, "disrupt")) {
    if (!GET_SUSTAINED_NUM(ch)) {
      send_to_char("You aren't sustaining any spells.\r\n", ch);
      return;
    }
    adept_release_spell(ch, TRUE);
  } else send_to_char("Syntax: FOCUS RELEASE (to return spell to caster) or FOCUS DISRUPT (to end spell).\r\n", ch);
}

ACMD(do_metamagic)
{
  if (GET_GRADE(ch) == 0) {
    send_to_char("You are not close enough to the astral plane to have any metamagic powers.\r\n", ch);
    return;
  }
  int i = 0;
  *buf = '\0';
  for (int x = 0; x < META_MAX; x++)
    if (GET_METAMAGIC(ch, x)) {
      i++;
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  %s%s^n\r\n", GET_METAMAGIC(ch, x) == 2 ? "" : "^r", metamagic[x]);
    }
  if (i > 0)
    send_to_char(ch, "You know the following metamagic techniques:\r\n%s", buf);
  else send_to_char("You don't know any metamagic techniques.\r\n", ch);
}

ACMD(do_cleanse)
{
  if (GET_METAMAGIC(ch, META_CLEANSING) < 2) {
    nonsensical_reply(ch, NULL, "standard");
    return;
  }
  if (!(IS_ASTRAL(ch) || IS_DUAL(ch)))
    send_to_char("You have no sense of the astral plane.\r\n", ch);
  else if (!GET_SKILL(ch, SKILL_SORCERY))
    send_to_char("You need practical knowledge of sorcery to cleanse the astral plane.\r\n", ch);
  else if (ch->in_veh)
    send_to_char("You'll need to leave your vehicle to attempt to manipulate this area's magical background.\r\n", ch);
  else if (!GET_BACKGROUND_COUNT(ch->in_room))
    send_to_char("The astral plane in this area is calm.\r\n", ch);
  else if (GET_BACKGROUND_COUNT(ch->in_room) > GET_GRADE(ch) || GET_BACKGROUND_COUNT(ch->in_room) > 5)
    send_to_char("Cleansing a disturbance of this magnitude is beyond your abilities.\r\n", ch);
  else if (GET_BACKGROUND_AURA(ch->in_room) == AURA_POWERSITE)
    send_to_char("You cannot cleanse a power site.\r\n", ch);
  else {
    int success = success_test(GET_SKILL(ch, SKILL_SORCERY), GET_BACKGROUND_COUNT(ch->in_room) * 2), background = GET_BACKGROUND_COUNT(ch->in_room);
    success /= 2;
    if (success <= 0)
      send_to_char("You fail to reduce the astral disturbances in this area.\r\n", ch);
    else {
      GET_SETTABLE_BACKGROUND_COUNT(ch->in_room) = MAX(0, GET_BACKGROUND_COUNT(ch->in_room) - success);
      if (!GET_BACKGROUND_COUNT(ch->in_room)) {
        send_to_char("You successfully remove all astral disturbances from the area.\r\n", ch);
        snprintf(buf, sizeof(buf), "The astral disturbances in this area completely vanish.\r\n");
      } else {
        send_to_char("You succeed in lowering the astral disturbances in the area.\r\n", ch);
        snprintf(buf, sizeof(buf), "The astral disturbances in the are seem to diminish slightly.\r\n");
      }
      for (struct char_data *targ = ch->in_room->people; targ; targ = targ->next_in_room)
        if (ch != targ && (IS_ASTRAL(targ) || IS_DUAL(targ)))
          send_to_char(buf, targ);
    }
    spell_drain(ch, 0, background, DEADLY);
  }
}

ACMD(do_think)
{
  struct sustain_data *spell = NULL;
  for (spell = GET_SUSTAINED(ch); spell; spell = spell->next)
    if (spell->spell == SPELL_MINDLINK)
      break;
  if (!spell || !*argument) {
    send_to_char("You think about life, the universe and everything.\r\n", ch);
    return;
  }
  skip_spaces(&argument);
  snprintf(buf, sizeof(buf), "^rYou hear $v in your mind say, \"%s\"", argument);
  act(buf, FALSE, ch, 0, ch->char_specials.mindlink, TO_VICT);
  send_to_char(ch, "You think, \"%s\"\r\n", argument);
}

int get_spell_affected_successes(struct char_data * ch, int type)
{
  if (!GET_SUSTAINED(ch))
    return 0;

  for (struct sustain_data *hjp = GET_SUSTAINED(ch); hjp; hjp = hjp->next)
    if ((hjp->spell == type) && (hjp->caster == FALSE))
      return MIN(hjp->success, hjp->force);

  return FALSE;
}

bool focus_is_usable_by_ch(struct obj_data *focus, struct char_data *ch) {
  // Focus or character does not exist.
  if (!focus || !ch)
    return FALSE;

  // Focus is not a focus.
  if (GET_OBJ_TYPE(focus) != ITEM_FOCUS)
    return FALSE;

  // NPCs get the advantage of the foci they're wearing without having to bond them.
  if (IS_NPC(ch))
    return TRUE;

  // Focus not bonded to character.
  if (GET_FOCUS_BONDED_TO(focus) != GET_IDNUM(ch))
    return FALSE;

  // Focus still being bonded.
  if (GET_FOCUS_BOND_TIME_REMAINING(focus))
    return FALSE;

  return TRUE;
}
