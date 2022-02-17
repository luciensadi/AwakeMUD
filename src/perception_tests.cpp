/* what we actually need to do:
  - add unordered map to all characters
  - add invis-changing function that is called whenever invis level shifts
  - link it in to releasing invis spell and toggling invis on thermoptic/ruthenium
  - add perception function
  - link in perception function on activities that could induce it (movement after breaking sneak, attacks, etc)
  - add function for stripping an idnum from maps, link it in extract_char
  - add houseruled limiter for invis / impinvis: no more successes than force of spell
*/

// unordered_map<idnum_t, bool> pc_perception_test_results;
// unordered_map<idnum_t, bool> mob_perception_test_results;

#include "structs.h"
#include "comm.h"
#include "newmagic.h"
#include "handler.h"
#include "db.h"

// Helper function for remove_ch_from_pc_perception_records().
void _remove_ch_from_pc_perception_records(struct char_data *ch, struct char_data *vict) {
  std::unordered_map<idnum_t, bool> *map_to_operate_on = NULL;
  std::unordered_map<idnum_t, bool>::const_iterator found;

  // Find the map they belong to, bailing out if the map doesn't exist.
  if (IS_NPC(ch)) {
    if (!vict->mob_perception_test_results)
      return;
    map_to_operate_on = vict->mob_perception_test_results;
  } else {
    if (!vict->pc_perception_test_results)
      return;
    map_to_operate_on = vict->pc_perception_test_results;
  }

  // If they're not already in the map, bail out.
  if (map_to_operate_on->find(GET_IDNUM(ch)) == map_to_operate_on->end())
    return;

  // They were there-- erase them from it.
  vict->pc_perception_test_results->erase(GET_IDNUM(ch));
}

// Remove the given character from all PC's perception test results. Used when ch is destroyed (death, quit, etc).
void remove_ch_from_pc_perception_records(struct char_data *ch) {
  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    if (d->character) {
      _remove_ch_from_pc_perception_records(ch, d->character);
    }
    if (d->original) {
      _remove_ch_from_pc_perception_records(ch, d->original);
    }
  }
}

// Blow away the invis perception records. Used when ch is about to be deleted.
void purge_invis_perception_records(struct char_data *ch) {
  if (ch->mob_perception_test_results) {
    ch->mob_perception_test_results->clear();
    delete ch->mob_perception_test_results;
  }

  if (ch->pc_perception_test_results) {
    ch->pc_perception_test_results->clear();
    delete ch->pc_perception_test_results;
  }
}

// Checks if you've seen through their invis, and does the perception check if you haven't done it yet.
bool can_see_through_invis(struct char_data *ch, struct char_data *vict) {
  std::unordered_map<idnum_t, bool> *map_to_operate_on = NULL;
  std::unordered_map<idnum_t, bool>::const_iterator found;
  char perception_test_rbuf[5000];

  // NPCs and PCs have their own separate maps.
  if (IS_NPC(ch)) {
    if (!vict->mob_perception_test_results)
      vict->mob_perception_test_results = new std::unordered_map<idnum_t, bool>;
    map_to_operate_on = vict->mob_perception_test_results;
  } else {
    if (!vict->pc_perception_test_results)
      vict->pc_perception_test_results = new std::unordered_map<idnum_t, bool>;
    map_to_operate_on = vict->pc_perception_test_results;
  }

  // If they're already in the map, return their prior result-- they already tested.
  if ((found = map_to_operate_on->find(GET_IDNUM(ch))) != map_to_operate_on->end()) {
    snprintf(perception_test_rbuf, sizeof(perception_test_rbuf), "Using cached result: %s.", found->second ? "true" : "false");
    act(perception_test_rbuf, FALSE, ch, NULL, NULL, TO_ROLLS);
    return found->second;
  }

  // They weren't in the map yet-- add them to it with their perception test results.
  // First, figure out the TN- iterate over target's spells to find the invis, and if there is none, give them the standard TN 4.
  snprintf(perception_test_rbuf, sizeof(perception_test_rbuf), "Perception test %s vs %s: ", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict));

  struct sustain_data *invis_spell_sust = NULL;

  // Scan through their spells and find the invis spell on them, if any.
  for (invis_spell_sust = GET_SUSTAINED(vict); invis_spell_sust; invis_spell_sust = invis_spell_sust->next) {
    // Note the use of the negated ->caster here-- we only want spells affecting them, not the caster records.
    if (!invis_spell_sust->caster && (invis_spell_sust->spell == SPELL_IMP_INVIS || invis_spell_sust->spell == SPELL_INVIS))
      break;
  }

  // You don't get to use a perception test to see through ruthenium etc-- just bail out.
  if (!invis_spell_sust) {
    act("Skipping perception test: No spell.", FALSE, ch, NULL, NULL, TO_ROLLS);
    return FALSE;
  }

  // Calculate the TN.
  int tn = invis_spell_sust->force;
  buf_mod(perception_test_rbuf, sizeof(perception_test_rbuf), "Spell TN", tn);
  tn += modify_target_rbuf(ch, perception_test_rbuf, sizeof(perception_test_rbuf));

  // If the victim is affected by the Conceal power, add the force of that to the victim's TN.
  int conceal_rating = affected_by_power(vict, CONCEAL);
  if (conceal_rating) {
    tn += conceal_rating;
    buf_mod(perception_test_rbuf, sizeof(perception_test_rbuf), "Conceal", conceal_rating);
  }

  // Next, figure out how many dice they're rolling.
  int dice = GET_INT(ch) + GET_TASK_POOL(ch, INT);
  snprintf(ENDOF(perception_test_rbuf), sizeof(perception_test_rbuf) - strlen(perception_test_rbuf), "\r\nDice: %d (int) + %d (task pool)", GET_INT(ch), GET_TASK_POOL(ch, INT));

  // Add tactical computer's rating as perception dice.
  for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content) {
    if (GET_CYBERWARE_TYPE(cyber) == CYB_TACTICALCOMPUTER) {
      dice += GET_CYBERWARE_RATING(cyber);
      buf_mod(perception_test_rbuf, sizeof(perception_test_rbuf), ", TacComp", GET_CYBERWARE_RATING(cyber));
      break;
    }
  }

  // Figure out how many successes they need to have to beat this spell.
  // House rule: You can't have more successes than the force of the spell.
  int effective_spell_successes = MIN(invis_spell_sust->success, invis_spell_sust->force);

  // Finally, roll the dice.
  int successes = success_test(dice, tn);
  bool test_result = (successes >= effective_spell_successes);
  snprintf(ENDOF(perception_test_rbuf), sizeof(perception_test_rbuf) - strlen(perception_test_rbuf), "\r\nResult: %d hits vs %d: %s",
           successes,
           effective_spell_successes,
           test_result ? "success" : "failure"
         );

  // Store the result.
  map_to_operate_on->emplace(GET_IDNUM(ch), test_result);

  // Write the rolls for debugging.
  act(perception_test_rbuf, FALSE, ch, NULL, NULL, TO_ROLLS);

  // Return the result.
  return test_result;
}
