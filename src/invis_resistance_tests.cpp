#include "structs.hpp"
#include "comm.hpp"
#include "newmagic.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "config.hpp"

void send_npc_newly_alarmed_message(struct char_data *npc, struct char_data *vict);

// Helper function for remove_ch_from_pc_invis_resistance_records().
void _remove_ch_from_pc_invis_resistance_records(struct char_data *ch, struct char_data *vict) {
  std::unordered_map<idnum_t, bool> *map_to_operate_on = NULL;
  std::unordered_map<idnum_t, bool>::const_iterator found;
  idnum_t idnum;

  // Find the map they belong to, bailing out if the map doesn't exist.
  if (IS_NPC(ch)) {
    if (!vict->mob_invis_resistance_test_results)
      return;
    map_to_operate_on = vict->mob_invis_resistance_test_results;
    idnum = GET_MOB_UNIQUE_ID(ch);
  } else {
    if (!vict->pc_invis_resistance_test_results)
      return;
    map_to_operate_on = vict->pc_invis_resistance_test_results;
    idnum = GET_IDNUM(ch);
  }

  // If they're not already in the map, bail out.
  if (map_to_operate_on->find(idnum) == map_to_operate_on->end())
    return;

  // They were there-- erase them from it.
  vict->pc_invis_resistance_test_results->erase(idnum);
}

// Remove the given character from all PC's invis resistance test results. Used when ch is destroyed (death, quit, etc).
void remove_ch_from_pc_invis_resistance_records(struct char_data *ch) {
  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    if (d->character) {
      _remove_ch_from_pc_invis_resistance_records(ch, d->character);
    }
    if (d->original) {
      _remove_ch_from_pc_invis_resistance_records(ch, d->original);
    }
  }
}

// Blow away the invis resistance records. Used when ch is about to be deleted.
void purge_invis_invis_resistance_records(struct char_data *ch) {
  delete ch->mob_invis_resistance_test_results;
  ch->mob_invis_resistance_test_results = NULL;

  delete ch->pc_invis_resistance_test_results;
  ch->pc_invis_resistance_test_results = NULL;
}

// Returns TRUE if we're alarmed, FALSE otherwise.
bool process_spotted_invis(struct char_data *ch, struct char_data *vict) {
  // PCs don't get alarmed.
  if (!IS_NPC(ch))
    return FALSE;

  // NPCs don't get alarmed by other NPCs.
  if (IS_NPC(vict) && !vict->desc)
    return FALSE;

  // Refresh alert time to 20 since we're seeing a PC being shady.
  GET_MOBALERTTIME(ch) = MAX(GET_MOBALERTTIME(ch), 20);

  if (GET_MOBALERT(ch) != MALERT_ALARM) {
    // The more secure an area is, the twitchier the guards are. This evaluates TRUE if a given guard is twitchy.
    int alert_threshold = GET_SECURITY_LEVEL(get_ch_in_room(ch)) * 5;
    if (MOB_FLAGS(ch).AreAnySet(MOB_GUARD, MOB_AGGRESSIVE, MOB_AGGR_ELF, MOB_AGGR_ORK, MOB_AGGR_DWARF, MOB_AGGR_HUMAN, MOB_AGGR_TROLL, ENDBIT)) {
      alert_threshold += 10;
    } else if (MOB_FLAGGED(ch, MOB_HELPER)) {
      alert_threshold += 5;
    }
    bool alert_state_should_be_alarm = number(0, MAX_ZONE_SECURITY_RATING * 4) * 5 <= alert_threshold;

    // Seeing someone walking around invis alarms you, or alerts you if you're not twitchy.
    if (alert_state_should_be_alarm) {
      GET_MOBALERT(ch) = MALERT_ALARM;
      send_npc_newly_alarmed_message(ch, vict);
      if (access_level(vict, LVL_PRESIDENT) && PRF_FLAGGED(vict, PRF_ROLLS)) {
        send_to_char(vict, "^L[%s#%ld has become ^ralarmed^L due to seeing you while you're invis.]\r\n", GET_CHAR_NAME(ch), GET_MOB_UNIQUE_ID(ch));
      }
      return TRUE;
    } else {
      GET_MOBALERT(ch) = MALERT_ALERT;
      if (access_level(vict, LVL_PRESIDENT) && PRF_FLAGGED(vict, PRF_ROLLS)) {
        send_to_char(vict, "^L[%s#%ld has become ^yalert^L due to seeing you while you're invis.]\r\n", GET_CHAR_NAME(ch), GET_MOB_UNIQUE_ID(ch));
      }
      return FALSE;
    }
  }
  return FALSE;
}

bool can_see_through_invis(struct char_data *ch, struct char_data *vict) {
  std::unordered_map<idnum_t, bool> *map_to_operate_on = NULL;
  std::unordered_map<idnum_t, bool>::const_iterator found;
  idnum_t idnum;

  /*
  act("Processing can_see_through_invis ($N looking at $n).", FALSE, vict, 0, ch, TO_ROLLS);
  if (vict->in_room != ch->in_room)
    act("Processing can_see_through_invis ($n looking at $N).", FALSE, ch, 0, vict, TO_ROLLS);
  */

  // NPCs and PCs have their own separate maps.
  if (IS_NPC(ch)) {
    if (!vict->mob_invis_resistance_test_results)
      vict->mob_invis_resistance_test_results = new std::unordered_map<idnum_t, bool>;
    map_to_operate_on = vict->mob_invis_resistance_test_results;
    idnum = GET_MOB_UNIQUE_ID(ch);
  } else {
    if (!vict->pc_invis_resistance_test_results)
      vict->pc_invis_resistance_test_results = new std::unordered_map<idnum_t, bool>;
    map_to_operate_on = vict->pc_invis_resistance_test_results;
    idnum = GET_IDNUM(ch);
  }

  // If they're already in the map, return their prior result-- they already tested.
  if ((found = map_to_operate_on->find(idnum)) != map_to_operate_on->end()) {
    // Anti-cheese: Prevents someone running in and out again, waiting for them to lose memory of us, and then going in while they're calm.
    if (IS_NPC(ch) && !IS_NPC(vict) && found->second) {
      if (GET_MOBALERT(ch) == MALERT_CALM) {
        // They timed out, so let's run the whole spotted check again.
        if (process_spotted_invis(ch, vict)) {
          send_npc_newly_alarmed_message(ch, vict);
        }
      } else {
        // Just refresh their existing alert time.
        GET_MOBALERTTIME(ch) = MAX(GET_MOBALERTTIME(ch), 20);
      }
    }

    return found->second;
  }

  // They weren't in the map yet-- add them to it with their invis resistance test results.
  struct sustain_data *invis_spell_sust = NULL;

  // Scan through their spells and find the invis spell on them, if any.
  for (invis_spell_sust = GET_SUSTAINED(vict); invis_spell_sust; invis_spell_sust = invis_spell_sust->next) {
    // Note the use of the negated ->caster here-- we only want spells affecting them, not the caster records.
    if (!invis_spell_sust->caster && (invis_spell_sust->spell == SPELL_IMP_INVIS || invis_spell_sust->spell == SPELL_INVIS))
      break;
  }

  // You don't get to use a invis resistance test to see through ruthenium etc-- just bail out.
  if (!invis_spell_sust) {
    return FALSE;
  }

  // Looks like we're going to need to process a check.
  char resistance_test_rbuf[5000];
  snprintf(resistance_test_rbuf, sizeof(resistance_test_rbuf), "Invis Resistance test %s vs %s: ", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict));

  // Calculate the TN.
  int tn = invis_spell_sust->force;
  buf_mod(resistance_test_rbuf, sizeof(resistance_test_rbuf), "Spell TN", tn);

  // SR3 p183: These modifiers don't apply.
  // tn += modify_target_rbuf(ch, resistance_test_rbuf, sizeof(resistance_test_rbuf));

  // If the victim is affected by the Conceal power, add the force of that to the victim's TN.
  int conceal_rating = affected_by_power(vict, CONCEAL);
  if (conceal_rating) {
    tn += conceal_rating;
    buf_mod(resistance_test_rbuf, sizeof(resistance_test_rbuf), "Conceal", conceal_rating);
  }

  // Next, figure out how many dice they're rolling. We don't get task pool because this isn't a skill.
  int dice = GET_INT(ch);
  snprintf(ENDOF(resistance_test_rbuf), sizeof(resistance_test_rbuf) - strlen(resistance_test_rbuf), "\r\nDice: %d (int)", GET_INT(ch));

  // House rule: Add tactical computer's rating as invis resistance dice, but only for mundanes.
  if (GET_MAG(ch) <= 0) {
    for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content) {
      if (GET_CYBERWARE_TYPE(cyber) == CYB_TACTICALCOMPUTER) {
        dice += GET_CYBERWARE_RATING(cyber);
        buf_mod(resistance_test_rbuf, sizeof(resistance_test_rbuf), "TacComp", GET_CYBERWARE_RATING(cyber));
        break;
      }
    }
  }

  if (GET_TRADITION(ch) == TRAD_SHAMANIC && GET_TOTEM(ch) == TOTEM_LEOPARD) {
    dice -= 1;
    buf_mod(resistance_test_rbuf, sizeof(resistance_test_rbuf), "LeopardTotem", -1);
  }

  if (GET_TRADITION(ch) == TRAD_ADEPT) {
    if (GET_POWER(ch, ADEPT_TRUE_SIGHT)) {
      dice += GET_POWER(ch, ADEPT_TRUE_SIGHT);
      buf_mod(resistance_test_rbuf, sizeof(resistance_test_rbuf), "TrueSight", GET_POWER(ch, ADEPT_TRUE_SIGHT));
    }
    if (GET_POWER(ch, ADEPT_MAGIC_RESISTANCE)) {
      dice += GET_POWER(ch, ADEPT_MAGIC_RESISTANCE);
      buf_mod(resistance_test_rbuf, sizeof(resistance_test_rbuf), "MagicResist", GET_POWER(ch, ADEPT_MAGIC_RESISTANCE));
    }
  }

  int magic_pool_dice = GET_SDEFENSE(ch) + GET_REFLECT(ch);
  if (magic_pool_dice > 0) {
    dice += magic_pool_dice;
    buf_mod(resistance_test_rbuf, sizeof(resistance_test_rbuf), "SpellDefense/Reflect", magic_pool_dice);
  }

  // Figure out how many successes they need to have to beat this spell.
  // House rule: You can't have more successes than the force of the spell.
  int effective_spell_successes = MIN(invis_spell_sust->success, invis_spell_sust->force);

  // Finally, roll the dice.
  int successes = success_test(dice, tn);
  bool test_succeeded = (successes >= effective_spell_successes);
  snprintf(ENDOF(resistance_test_rbuf), sizeof(resistance_test_rbuf) - strlen(resistance_test_rbuf), "\r\nResult: %d hits vs %d: %s",
           successes,
           effective_spell_successes,
           test_succeeded ? "success" : "failure"
         );

  // Store the result.
  map_to_operate_on->emplace(idnum, test_succeeded);

  // Write the rolls for debugging.
  act(resistance_test_rbuf, FALSE, ch, NULL, NULL, TO_ROLLS);

  // Message the character if they can suddenly see their opponent.
  bool ch_in_same_place_as_vict = (ch->in_room && ch->in_room == vict->in_room) || (ch->in_veh && ch->in_veh == vict->in_veh);
  if (ch != vict && ch_in_same_place_as_vict && GET_POS(ch) > POS_SLEEPING && AWAKE(ch)) {
    if (test_succeeded) {
      act("You squint at a hazy patch of air, and suddenly $N pops into view!", FALSE, ch, 0, vict, TO_CHAR);

      // Spotting an invisible person can alarm NPCs.
      if (IS_NPC(ch) && process_spotted_invis(ch, vict)) {
        send_npc_newly_alarmed_message(ch, vict);
      }
    }
    else if (GET_EQ(vict, WEAR_LIGHT) && IS_NPC(ch)) {
      // You can't see them, but their flashlight beam is still visible.
      GET_MOBALERT(ch) = MALERT_ALERT;
      GET_MOBALERTTIME(ch) = 20;
    }
  }

  // Return the result.
  return test_succeeded;
}

void send_npc_newly_alarmed_message(struct char_data *npc, struct char_data *vict) {
  bool passed_check = success_test(GET_INT(vict) + GET_POWER(vict, ADEPT_IMPROVED_PERCEPT), GET_INT(npc)) > 0;

  if (passed_check && CAN_SEE(vict, npc) && vict->in_room == npc->in_room) {
    if (MOB_FLAGGED(npc, MOB_INANIMATE)) {
      act("You swivel towards $N with an alarmed chirp as your sensors pierce $S invisibility.^n", TRUE, npc, 0, vict, TO_CHAR);
      act("$n^n swivels towards $N with an alarmed chirp.", TRUE, npc, 0, vict, TO_NOTVICT);
      act("^y$n^y swivels towards you with an alarmed chirp as its sensors pierce your invisibility.^n", TRUE, npc, 0, vict, TO_VICT);
    } else {
      act("You scowl at $N as your eyes focus through $S invisibility.^n", TRUE, npc, 0, vict, TO_CHAR);
      act("$n^n scowls at $N as $s eyes focus through $S invisibility.", TRUE, npc, 0, vict, TO_NOTVICT);
      act("^y$n^y scowls at you as $s eyes focus through your invisibility.^n", TRUE, npc, 0, vict, TO_VICT);
    }
  }
  // Otherwise, you have a chance to get a feeling about it.
  else if (number(0, 5) == 0) {
    send_to_char("You get an uneasy feeling...\r\n", vict);
    if (GET_NOT(vict) < NEWBIE_KARMA_THRESHOLD && number(0, MAX(1, GET_NOT(vict) / 5)) == 0) {
      send_to_char(vict, "(Being invisible sets NPCs on edge! Be careful out there.)\r\n");
    }
  }
}
