#include "awake.hpp"
#include "structs.hpp"
#include "comm.hpp"
#include "db.hpp"
#include "utils.hpp"
#include "handler.hpp"
#include "constants.hpp"
#include "interpreter.hpp"
#include "config.hpp"

/* Houserules:
  - We don't have drug families / immunities, so we model tolerance as increasing the number of doses you must take to get high.
  - We do not reduce any maximums, although you can lose stats down to racial minimum from withdrawal.
  - Hyper deals up-front damage because we don't model the increased damage otherwise.
  - Kamikaze damages you when you come down.
  - Decreased edge values (they were initially set based on tolerance being crippling)
  A bunch more that I've forgotten to write down.
*/

/* When taking drugs:
   Trigger do_drug_take to consume the object and start this process, otherwise start it manually with apply_doses_of_drug_to_char.

   - wait for limit tick, where we call process_drug_point_update_tick(), which transitions between onset/comedown/idle
   - on X tick, process_withdrawal inflicts withdrawal symptoms, and also induces withdrawal for those who have gone without a fix
   - on X tick, apply_drug_modifiers_to_ch modifiers char stats
*/

// TODO: Balance pass on drugs / penalties / withdrawal costs

extern int raw_stat_loss(struct char_data *);
extern bool check_adrenaline(struct char_data *, int);
extern void weight_change_object(struct obj_data *, float);

ACMD_DECLARE(do_look);

// ----------------- Helper Prototypes
bool _process_edge_and_tolerance_changes_for_applied_dose(struct char_data *ch, int drug_id);
bool _apply_doses_of_drug_to_char(int doses, int drug_id, struct char_data *ch);
bool _drug_dose_exceeds_tolerance(struct char_data *ch, int drug_id);
bool _specific_addiction_test(struct char_data *ch, int drug_id, bool is_mental, const char *test_identifier);
bool _combined_addiction_test(struct char_data *ch, int drug_id, const char *test_identifier, bool is_guided_withdrawal_check=FALSE);
int _seek_drugs_purchase_cost(struct char_data *ch, int drug_id);
bool seek_drugs(struct char_data *ch, int drug_id);
void update_withdrawal_flags(struct char_data *ch);
void _put_char_in_withdrawal(struct char_data *ch, int drug_id, bool is_guided);
bool _take_anti_drug_chems(struct char_data *ch, int drug_id);

ACMD(do_drugs) {
  FAILURE_CASE(IS_NPC(ch), "NPCs don't have drug information.\r\n");

  send_to_char(ch, "The following OOC information is available on your drug states:\r\n");

  for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
    if (GET_DRUG_LIFETIME_DOSES(ch, drug_id)) {
      send_to_char(ch, "^W%s^n: ^c%d^n tolerance, ^c%d^n lifetime doses.",
                   drug_types[drug_id].name,
                   GET_DRUG_TOLERANCE_LEVEL(ch, drug_id),
                   GET_DRUG_LIFETIME_DOSES(ch, drug_id));
      if (GET_DRUG_ADDICT(ch, drug_id) || GET_DRUG_ADDICTION_EDGE(ch, drug_id)) {
        send_to_char(ch, " %s, addiction edge ^c%d^n.",
                     GET_DRUG_ADDICT(ch, drug_id) ? "^yAddicted^n" : "^cNot addicted^n",
                     GET_DRUG_ADDICTION_EDGE(ch, drug_id));
      }
      send_to_char("\r\n", ch);
    }
  }
}


// Given a character and a drug object, dose the character with that drug object, then extract it if needed. Effects apply at next limit tick.
bool do_drug_take(struct char_data *ch, struct obj_data *obj) {
  char oopsbuf[500];

  if (!ch) {
    mudlog("SYSERR: Received NULL character to do_drug_take()!", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (!obj || GET_OBJ_TYPE(obj) != ITEM_DRUG) {
    snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Received non-drug object %s to do_drug_take!", obj ? GET_OBJ_NAME(obj) : "NULL");
    mudlog(oopsbuf, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (!PLR_FLAGGED(ch, PLR_ENABLED_DRUGS)) {
    send_to_char("^YWARNING:^n Drugs in Shadowrun are highly addictive and, while granting powerful bonuses, can also have negative effects on your character! You should only proceed if you are FULLY comfortable with the ramifications of taking drugs.\r\n", ch);
    send_to_char("\r\nIn order to enable drug usage for your character, you must ##^WTOGGLE DRUGUSE^n. Note that this is a permanent choice.\r\n", ch);
    return FALSE;
  }

  int drug_id = GET_OBJ_DRUG_TYPE(obj);

  // You can't take more of a drug you're already high on.
  if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_ONSET) {
    send_to_char(ch, "You're already high on %s, it would be a bad idea to take more right now.\r\n", drug_types[drug_id].name);
    return FALSE;
  }

  // Right now, we apply maximum doses from the object. Later, we'll be smarter about this.
  // Many drugs exist without doses set, so we assume 0 doses means the drug is an old-style drug.
  int available_drug_doses = GET_OBJ_DRUG_DOSES(obj) > 0 ? GET_OBJ_DRUG_DOSES(obj) : 1;

  // Onboard doses until the object runs out or they end up high (whichever comes first)
  int doses_to_take = MIN(available_drug_doses, GET_DRUG_TOLERANCE_LEVEL(ch, drug_id) + 1);
  GET_OBJ_DRUG_DOSES(obj) -= doses_to_take;
  weight_change_object(obj, -1 * (doses_to_take * 0.01));

  if (_apply_doses_of_drug_to_char(doses_to_take, drug_id, ch)) {
    return TRUE;
  }

  // Message the room.
  act("$n takes a hit from $p.", TRUE, ch, obj, 0, TO_ROOM);

  // The message we give the character depends on if they've passed their tolerance dose yet.
  if (!_drug_dose_exceeds_tolerance(ch, drug_id)) {
    // They didn't take enough. Give the appropriate message.
    send_to_char(ch, "You take %d dose%s from %s, but it doesn't kick in-- you'll need to take more to surpass your tolerance of %d.\r\n",
                 doses_to_take,
                 doses_to_take == 1 ? "" : "s",
                 GET_OBJ_NAME(obj),
                 GET_DRUG_TOLERANCE_LEVEL(ch, drug_id));
  } else {
    send_to_char(ch, "You take %d dose%s from %s.\r\n",
                 doses_to_take,
                 doses_to_take == 1 ? "" : "s",
                GET_OBJ_NAME(obj));
  }

  // Remove the doses from the drug.
  if (GET_OBJ_DRUG_DOSES(obj) <= 0) {
    // The drug has been finished off.
    extract_obj(obj);
  }

  // That's it! All other checks are done when the drug kicks in.
  return FALSE;
}

// Tick down all drugs for a given character. Induces onset/comedown etc. Called once per IG minute.
bool process_drug_point_update_tick(struct char_data *ch) {
  char roll_buf[500];
  time_t current_time = time(0);

  // We skip anyone who has not enabled drugs. Saves on processing time.
  if (!PLR_FLAGGED(ch, PLR_ENABLED_DRUGS))
    return FALSE;

  for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
    int damage_boxes = 0;

    // All drugs are ticked down as long as you're on them.
    if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_ONSET || GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_COMEDOWN)
      if (GET_DRUG_DURATION(ch, drug_id) > 0)
        GET_DRUG_DURATION(ch, drug_id)--;

    // They're already on it and lost their last tick of uptime. Switch to comedown.
    if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_ONSET && GET_DRUG_DURATION(ch, drug_id) <= 0) {
      int toxin_extractor_rating = 0;
      for (struct obj_data *obj = ch->bioware; obj; obj = obj->next_content)
        if (GET_BIOWARE_TYPE(obj) == BIO_TOXINEXTRACTOR)
          toxin_extractor_rating = MAX(toxin_extractor_rating, GET_BIOWARE_RATING(obj));

      if (drug_id == DRUG_KAMIKAZE) {
        send_to_char("You shudder and twitch as the combat drugs start to leave your system.\r\n", ch);
      } else {
        send_to_char(ch, "You begin to feel drained as the %s wears off.\r\n", drug_types[drug_id].name);
      }
      GET_DRUG_STAGE(ch, drug_id) = DRUG_STAGE_COMEDOWN;

      // Apply comedown durations and instant effects (damage etc).
      int bod_for_success_test = GET_REAL_BOD(ch) - (GET_BIOOVER(ch) / 2);
      switch (drug_id) {
        case DRUG_JAZZ:
          GET_DRUG_DURATION(ch, drug_id) = 100 * dice(1,6);
          if (damage(ch, ch, convert_damage(stage(-success_test(bod_for_success_test, 8 - toxin_extractor_rating, ch, "jazz damage resist"), LIGHT)), TYPE_BIOWARE, 0)) {
            return TRUE;
          }
          break;
        case DRUG_KAMIKAZE:
          GET_DRUG_DURATION(ch, drug_id) = 100 * dice(1,6);
          {
            // Canonically, this should not hurt this much-- but we don't have a concept of cyberware/bioware system damage here,
            // so instead of applying permanent subsystem damage, we just deal CHONKY damage that increases based on the number of doses you took.
            int damage_level = LIGHT + GET_DRUG_DOSE(ch, DRUG_KAMIKAZE);
            int power = 12 - toxin_extractor_rating;
            int resist_test_successes = success_test(bod_for_success_test, power, ch, "kamikaze damage resist");
            int damage_boxes = convert_damage(stage(-resist_test_successes, damage_level));

            snprintf(roll_buf, sizeof(roll_buf), "%s's %s comedown damage: Power %d, DL %d, resist success %d, staged and converted to %d boxes of mental damage.",
                     GET_CHAR_NAME(ch),
                     drug_types[drug_id].name,
                     power,
                     damage_level,
                     resist_test_successes,
                     damage_boxes);
            act(roll_buf, FALSE, ch, 0, 0, TO_ROLLS);

            if (damage(ch, ch, damage_boxes, TYPE_DRUGS, 0)) {
              return TRUE;
            }
          }
          break;
        case DRUG_CRAM:
          GET_DRUG_DURATION(ch, drug_id) = MAX(1, 12 - bod_for_success_test) * 600;
          break;
        case DRUG_NITRO:
          {
            int damage_level = DEADLY;
            int power = 8 - toxin_extractor_rating;
            int resist_test_successes = success_test(bod_for_success_test, power, ch, "nitro damage resist");
            int damage_boxes = convert_damage(stage(-resist_test_successes, damage_level));

            snprintf(roll_buf, sizeof(roll_buf), "%s's %s comedown damage: Power %d, DL %d, resist success %d, staged and converted to %d boxes of mental damage.",
                     GET_CHAR_NAME(ch),
                     drug_types[drug_id].name,
                     power,
                     damage_level,
                     resist_test_successes,
                     damage_boxes);
            act(roll_buf, FALSE, ch, 0, 0, TO_ROLLS);

            if (damage(ch, ch, damage_boxes, TYPE_DRUGS, 0)) {
              return TRUE;
            }
          }
          reset_drug_for_char(ch, drug_id);
          break;
        case DRUG_NOVACOKE:
          GET_DRUG_DURATION(ch, drug_id) = MAX(1, 10 - bod_for_success_test) * 600;
          break;
        default:
          reset_drug_for_char(ch, drug_id);
      }

      // Staff get accelerated comedowns.
      if (GET_LEVEL(ch) > LVL_MORTAL && GET_DRUG_DURATION(ch, drug_id) > 0) {
        send_to_char(ch, "Staff; Accelerating drug comedown duration from %d to 4s for easier testing.\r\n", GET_DRUG_DURATION(ch, drug_id));
        GET_DRUG_DURATION(ch, drug_id) = 2;
      }

      // Clear the dose.
      GET_DRUG_DOSE(ch, drug_id) = 0;
    }

    // Your comedown has worn off.
    else if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_COMEDOWN && GET_DRUG_DURATION(ch, drug_id) <= 0) {
      send_to_char(ch, "The aftereffects of the %s wear off.\r\n", drug_types[drug_id].name);
      reset_drug_for_char(ch, drug_id);
    }

    // Transition from no drug to having it, assuming you've passed your tolerance threshold.
    else if (GET_DRUG_STAGE(ch, drug_id) != DRUG_STAGE_ONSET && _drug_dose_exceeds_tolerance(ch, drug_id)) {
      bool damage_is_physical = (drug_id != DRUG_HYPER && drug_id != DRUG_BURN);

      // Some drugs hurt when they kick in.
      if (drug_types[drug_id].damage_level > 0) {
        // Your damage level is dictated by the drug's base damage level, leveled up once if you've taken multiple doses.
        int damage_level = drug_types[drug_id].damage_level;
        if (GET_DRUG_DOSE(ch, drug_id) > 1) {
          damage_level++;
        }

        // The power of the damage is specified by the drug, and influenced by the doses taken.
        int power = drug_types[drug_id].power;
        if (GET_DRUG_DOSE(ch, drug_id) > 2) {
          power += GET_DRUG_DOSE(ch, drug_id) - 2;
        }

        // How many successes did we roll to stage down the damage?
        int resist_test_successes = success_test(GET_REAL_BOD(ch) - (GET_BIOOVER(ch) / 2), power, ch, "onset damage resist");

        // Calculate the boxes of damage to apply.
        damage_boxes = convert_damage(stage(-resist_test_successes, damage_level));

        snprintf(roll_buf, sizeof(roll_buf), "%s's %s onset damage: Power %d, DL %d, resist success %d, staged and converted to %d boxes of %s damage.",
                 GET_CHAR_NAME(ch),
                 drug_types[drug_id].name,
                 power,
                 damage_level,
                 resist_test_successes,
                 damage_boxes,
                 damage_is_physical ? "physical" : "mental");
        act(roll_buf, FALSE, ch, 0, 0, TO_ROLLS);

        // Apply damage boxes. If they die from drug damage, terminate evaluation.
        if (damage(ch, ch, damage_boxes, TYPE_DRUGS, damage_is_physical)) {
          return TRUE;
        }
      }

      // Onset them and set when their last fix was (used for withdrawal calculations).
      GET_DRUG_STAGE(ch, drug_id) = DRUG_STAGE_ONSET;
      GET_DRUG_LAST_FIX(ch, drug_id) = current_time;

      // Process increases to addiction and tolerance, and also deal bod damage.
      if (_process_edge_and_tolerance_changes_for_applied_dose(ch, drug_id)) {
        return TRUE;
      }

      // Send message, set duration, and apply instant effects.
      switch (drug_id) {
        case DRUG_ACTH:
          snprintf(buf, sizeof(buf), "You feel a brief moment of vertigo.\r\n");
          if (check_adrenaline(ch, 1)) {
            // They died from the kick, RIP.
            return TRUE;
          }
          // ACTH has no long-term effects, so we take them off it immediately.
          reset_drug_for_char(ch, drug_id);
          break;
        case DRUG_HYPER:
          snprintf(buf, sizeof(buf), "The world seems to swirl around you as your mind is bombarded with feedback.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = damage_boxes * 100;
          break;
        case DRUG_JAZZ:
          snprintf(buf, sizeof(buf), "The world slows down around you.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = 100 * dice(1,6);
          break;
        case DRUG_KAMIKAZE:
          snprintf(buf, sizeof(buf), "Your body feels alive with energy and the desire to fight.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = 100 * dice(1,6);
          break;
        case DRUG_PSYCHE:
          snprintf(buf, sizeof(buf), "Your feel your mind racing.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = MAX(1, 12 - GET_REAL_BOD(ch)) * 600;
          break;
        case DRUG_BLISS:
          snprintf(buf, sizeof(buf), "The world fades into bliss as your body becomes sluggish.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = MAX(1, 6 - GET_REAL_BOD(ch)) * 600;
          break;
        case DRUG_BURN:
          snprintf(buf, sizeof(buf), "You suddenly feel very intoxicated.\r\n");
          // Burn's long-term effects are done through the drunk code.
          reset_drug_for_char(ch, drug_id);
          GET_COND(ch, COND_DRUNK) = FOOD_DRINK_MAX;
          break;
        case DRUG_CRAM:
          snprintf(buf, sizeof(buf), "Your body feels alive with energy.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = MAX(1, 12 - GET_REAL_BOD(ch)) * 600;
          break;
        case DRUG_NITRO:
          snprintf(buf, sizeof(buf), "You lose sense of yourself as your entire body comes alive with energy.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = 100 * dice(1,6);
          break;
        case DRUG_NOVACOKE:
          snprintf(buf, sizeof(buf), "You feel euphoric and alert.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = MAX(1, 10 - GET_REAL_BOD(ch)) * 600;
          break;
        case DRUG_ZEN:
          snprintf(buf, sizeof(buf), "You start to lose your sense of reality as your sight fills with hallucinations.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = 100 * dice(1,6);
          break;
        default:
          snprintf(buf, sizeof(buf), "SYSERR: Unknown drug type %d when printing drug-start message!", drug_id);
          mudlog(buf, ch, LOG_SYSLOG, TRUE);
          GET_DRUG_DURATION(ch, drug_id) = 10;
          return FALSE;
      }

      int detox_force = affected_by_spell(ch, SPELL_DETOX);
      if (!detox_force || detox_force < drug_types[drug_id].power) {
        send_to_char(buf, ch);
      } else {
        send_to_char("The sensation of the drugs moving through your system is muted by a detox effect.\r\n", ch);
      }

      // Staff get short uptimes for testing.
      if (GET_LEVEL(ch) > LVL_MORTAL && GET_DRUG_DURATION(ch, drug_id) > 0) {
        send_to_char(ch, "Staff: Accelerating drug onset duration from %d to 4s for easier testing.\r\n", GET_DRUG_DURATION(ch, drug_id));
        GET_DRUG_DURATION(ch, drug_id) = 2;
      }
    }
  }

  update_withdrawal_flags(ch);
  return FALSE;
}

// Apply the onset / comedown modifiers to the selected character.
int noop_var_for_perception_test = 0;
#define GET_PERCEPTION_TEST_DICE_MOD(ch) noop_var_for_perception_test /* does nothing-- I added this to track the work we need to eventually do around drug perception tests.*/
void apply_drug_modifiers_to_ch(struct char_data *ch) {
  // Note: You CANNOT use update_withdrawal_flags() in this function. This is called from affect_total, and update_withdrawal_flags calls affect_total.
  GET_PERCEPTION_TEST_DICE_MOD(ch) = 0;

  // Skip anyone who has not enabled drugs.
  if (!PLR_FLAGGED(ch, PLR_ENABLED_DRUGS)) {
    return;
  }

  int detox_force = affected_by_spell(ch, SPELL_DETOX);

  for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
    if (detox_force && detox_force >= drug_types[drug_id].power) {
      continue;
    }

    if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_ONSET) {
      switch (drug_id) {
        case DRUG_HYPER:
          GET_TARGET_MOD(ch)++;
          GET_CONCENTRATION_TARGET_MOD(ch) += 3; // should be 4, but it already includes the general target mod above
          break;
        case DRUG_JAZZ:
          GET_QUI(ch) += 2;
          GET_INIT_DICE(ch)++;
          break;
        case DRUG_KAMIKAZE:
          GET_BOD(ch)++;
          GET_QUI(ch)++;
          GET_STR(ch) += 2;
          GET_WIL(ch)++;
          GET_INIT_DICE(ch)++;
          break;
        case DRUG_PSYCHE:
          GET_INT(ch)++;
          break;
        case DRUG_BLISS:
          GET_TARGET_MOD(ch)++;
          GET_REA(ch)--;
          // Pain resistance is handled directly in the relevant function.
          break;
        case DRUG_CRAM:
          GET_REA(ch)++;
          GET_INIT_DICE(ch)++;
          break;
        case DRUG_NITRO:
          GET_STR(ch) += 2;
          GET_WIL(ch) += 2;
          // Pain resistance is handled directly in the relevant function.
          GET_PERCEPTION_TEST_DICE_MOD(ch) += 2;
          break;
        case DRUG_NOVACOKE:
          GET_REA(ch)++;
          GET_CHA(ch)++;
          // Pain resistance is handled directly in the relevant function.
          GET_PERCEPTION_TEST_DICE_MOD(ch)++;
          break;
        case DRUG_ZEN:
          GET_REA(ch) -= 2;
          GET_TARGET_MOD(ch)++;
          GET_WIL(ch)++;
          break;
      }
    } else if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_COMEDOWN) {
      switch (drug_id) {
        case DRUG_JAZZ:
          GET_CONCENTRATION_TARGET_MOD(ch)++;
          GET_QUI(ch)--;
          break;
        case DRUG_KAMIKAZE:
          GET_QUI(ch)--;
          GET_WIL(ch)--;
          break;
        case DRUG_NOVACOKE:
          GET_CHA(ch) = 1;
          GET_WIL(ch) /= 2;
          break;
      }
    }
  }

  if (AFF_FLAGGED(ch, AFF_WITHDRAWAL_FORCE)) {
    // M&M p110
    GET_MAX_MENTAL(ch) = MIN(600, GET_MAX_MENTAL(ch));
    GET_MENTAL(ch) = MIN(GET_MENTAL(ch), GET_MAX_MENTAL(ch));
    GET_TARGET_MOD(ch) += 3;
    GET_CONCENTRATION_TARGET_MOD(ch) += 6;
  } else if (AFF_FLAGGED(ch, AFF_WITHDRAWAL)) {
    GET_TARGET_MOD(ch) += 2;
    GET_CONCENTRATION_TARGET_MOD(ch) += 4;
  }
}
#undef GET_PERCEPTION_TEST_DICE_MOD

// Apply withdrawal state and penalties to character. Entering withdrawal here puts you in Forced which is faster but sucks harder.
// Called once per in-game hour.
void process_withdrawal(struct char_data *ch) {
  time_t current_time = time(0);
  char rbuf[1000];

  // Skip anyone who has not enabled drugs.
  if (!PLR_FLAGGED(ch, PLR_ENABLED_DRUGS)) {
    return;
  }

  // Iterate through all drugs.
  for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
    // Calculate time since last fix.
    time_t days_since_last_fix = (current_time - GET_DRUG_LAST_FIX(ch, drug_id)) / SECS_PER_MUD_DAY;

    // If they're not currently high / coming down, tick down their dose (it metabolizes away).
    if (GET_DRUG_DOSE(ch, drug_id) > 0 && (GET_DRUG_STAGE(ch, drug_id) != DRUG_STAGE_ONSET && GET_DRUG_STAGE(ch, drug_id) != DRUG_STAGE_COMEDOWN))
      GET_DRUG_DOSE(ch, drug_id)--;

    // If they're currently unaffected but have a tolerance, tick down tolerance-- speed configurable in config.hpp.
    if (!GET_DRUG_ADDICT(ch, drug_id) && GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_UNAFFECTED) {
      if (GET_DRUG_TOLERANCE_LEVEL(ch, drug_id) > 0 && number(0, AVG_HOURS_PER_TOLERANCE_TICKDOWN) == 0) {
        GET_DRUG_TOLERANCE_LEVEL(ch, drug_id)--;
      }
      continue;
    }

    // If we've gotten here, they're addicted, high, or in withdrawal.
    if (GET_LEVEL(ch) > LVL_MORTAL) {
      send_to_char(ch, "Accelerating %s withdrawal due to staff status.\r\n", drug_types[drug_id].name);
      days_since_last_fix = drug_types[drug_id].fix_factor;
    }

    // Tick up addiction / withdrawal stat losses. Houseruled from Addiction Effects (M&M p109).
    if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_GUIDED_WITHDRAWAL || GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_FORCED_WITHDRAWAL) {
      // Increment our addiction tick counter.
      GET_DRUG_ADDICTION_TICK_COUNTER(ch, drug_id)++;

      // Process stat loss as appropriate.
      if (GET_DRUG_STAGE(ch, drug_id == DRUG_STAGE_FORCED_WITHDRAWAL)) {
        bool body_is_above_racial_minimums = GET_REAL_BOD(ch) > 1 + racial_attribute_modifiers[(int) GET_RACE(ch)][BOD];
        bool should_test_for_stat_loss = GET_DRUG_ADDICTION_TICK_COUNTER(ch, drug_id) % (24 /* aka per in-game day, tested IG hourly */) == 0;

        // If the timer is expired, we check for loss.
        if (should_test_for_stat_loss) {
          if (!_combined_addiction_test(ch, drug_id, "damage")) {
            send_to_char(ch, "Your health suffers at the hand of your %s addiction.\r\n", drug_types[drug_id].name);

            int lost_attribute;

            // We always decrease body first.
            if (body_is_above_racial_minimums) {
              lost_attribute = BOD;
              GET_REAL_BOD(ch)--;
            }
            // No more body? Decrease other stats.
            else {
              lost_attribute = raw_stat_loss(ch);
            }

            send_to_char(ch, "You've lost a point of %s, but you can re-train it at a trainer. You should raise your Body quickly to avoid losing further stats!\r\n", attributes[lost_attribute]);

            snprintf(buf, sizeof(buf), "%s lost a point of %s (%d->%d) to %s addiction.",
                     GET_CHAR_NAME(ch),
                     attributes[lost_attribute],
                     GET_REAL_ATT(ch, lost_attribute) + 1,
                     GET_REAL_ATT(ch, lost_attribute),
                     drug_types[drug_id].name);
            mudlog(buf, ch, LOG_SYSLOG, TRUE);
          }
        }
      }

      // Tick down their addiction rating as they withdraw. Speed varies based on whether this is forced or not.
      if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_GUIDED_WITHDRAWAL || GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_FORCED_WITHDRAWAL) {
        // Decrement their edge, allowing their addiction rating to decrease.
        if (days_since_last_fix > GET_DRUG_LAST_WITHDRAWAL_TICK(ch, drug_id)) {
          snprintf(rbuf, sizeof(rbuf), "$n: %s withdrawal: d_s_l_f %ld > l_w_t %d, ticking down edge.\r\n",
                   GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_FORCED_WITHDRAWAL ? "Forced" : "Guided",
                   days_since_last_fix,
                   GET_DRUG_LAST_WITHDRAWAL_TICK(ch, drug_id));
          act(rbuf, FALSE, ch, 0, 0, TO_ROLLS);

          GET_DRUG_LAST_WITHDRAWAL_TICK(ch, drug_id) = days_since_last_fix + (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_GUIDED_WITHDRAWAL ? 2 : 1);

          snprintf(rbuf, sizeof(rbuf), " - New l_w_t = %d. Edge goes from %d to %d.\r\n",
                   GET_DRUG_LAST_WITHDRAWAL_TICK(ch, drug_id),
                   GET_DRUG_ADDICTION_EDGE(ch, drug_id),
                   GET_DRUG_ADDICTION_EDGE(ch, drug_id) - 1);
          act(rbuf, FALSE, ch, 0, 0, TO_ROLLS);

          if (--GET_DRUG_ADDICTION_EDGE(ch, drug_id) <= 0) {
            GET_DRUG_STAGE(ch, drug_id) = DRUG_STAGE_UNAFFECTED;
            GET_DRUG_ADDICT(ch, drug_id) = 0;
            GET_DRUG_ADDICTION_EDGE(ch, drug_id) = 0;
            send_to_char(ch, "The last of the trembles from your %s%s withdrawal wear off.\r\n",
                         GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_FORCED_WITHDRAWAL ? "forced " : "",
                         drug_types[drug_id].name);
            continue;
          }
          send_to_char(ch, "Your body cries out for some %s.\r\n", drug_types[drug_id].name);
        }

        // If they got here, they're still addicted. Check to see if they're weak-willed enough to auto-take it.
        if (days_since_last_fix >= drug_types[drug_id].fix_factor) {
          // If you're undergoing guided withdrawal AND have the right chems on you, you skip the test (consumes chems though)
          if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_GUIDED_WITHDRAWAL && _take_anti_drug_chems(ch, drug_id)) {
            continue;
          }

          if (!_combined_addiction_test(ch, drug_id, "auto-take resistance")) {
            // Compose message, which is replaced by craving messages if they're carrying drugs.
            if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_FORCED_WITHDRAWAL) {
              snprintf(buf, sizeof(buf), "Your lack of %s is causing you great pain and discomfort.\r\n", drug_types[drug_id].name);
            } else {
              snprintf(buf, sizeof(buf), "You crave some %s.\r\n", drug_types[drug_id].name);
            }

            // Auto-use drugs if available in inventory.
            send_to_char(ch, "You attempt to satisfy your craving for %s.\r\n", drug_types[drug_id].name);
            for (struct obj_data *obj = ch->carrying, *next_content; obj; obj = next_content) {
              next_content = obj->next_content;
              if (GET_OBJ_TYPE(obj) == ITEM_DRUG && GET_OBJ_DRUG_TYPE(obj) == drug_id) {
                do_drug_take(ch, obj); // obj is potentially extracted at this point
                if (GET_DRUG_DOSE(ch, drug_id) > GET_DRUG_TOLERANCE_LEVEL(ch, drug_id)) {
                  return;
                }
              }
            }

            if (GET_DRUG_DOSE(ch, drug_id) <= GET_DRUG_TOLERANCE_LEVEL(ch, drug_id)) {
              seek_drugs(ch, drug_id);
            }
          }
        }
      }
    } else if (GET_DRUG_ADDICT(ch, drug_id)) {
      // They weren't in withdrawal: Put them into that state if they've passed their fix limit.
      if (days_since_last_fix > drug_types[drug_id].fix_factor) {
        send_to_char(ch, "You begin to go into forced %s withdrawal.\r\n", drug_types[drug_id].name);
        _put_char_in_withdrawal(ch, drug_id, FALSE);
      } else if (number(0, 2) == 0) {
        send_to_char(ch, "You crave %s.\r\n", drug_types[drug_id].name);
      }
    }
  }

  // Re-apply withdrawal flags as appropriate.
  update_withdrawal_flags(ch);
}

void render_drug_info_for_targ(struct char_data *viewer, struct char_data *target) {
  if (!PLR_FLAGGED(target, PLR_ENABLED_DRUGS)) {
    send_to_char("\r\nDrug usage not enabled.\r\n", viewer);
    return;
  }

  bool printed_something = FALSE;
  // Send drug info
  send_to_char("\r\nDrug info: \r\n", viewer);

  for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
    bool should_show_this_drug = FALSE;
    for (int chk = 0; chk < NUM_DRUG_PLAYER_SPECIAL_FIELDS; chk++) {
      if (target->player_specials->drugs[drug_id][chk]) {
        should_show_this_drug = TRUE;
        break;
      }
    }

    if (!should_show_this_drug)
      continue;

    printed_something = TRUE;
    send_to_char(viewer, " - ^c%s^n (edg %d, add %s, lstfx %lds ago, addt %d, lstwith %d, dur %d, current dose %d/%d (%d lifetime), stage %d)\r\n",
                 drug_types[drug_id].name,
                 GET_DRUG_ADDICTION_EDGE(target, drug_id),
                 GET_DRUG_ADDICT(target, drug_id) ? "Y" : "N",
                 time(0) - GET_DRUG_LAST_FIX(target, drug_id),
                 GET_DRUG_ADDICTION_TICK_COUNTER(target, drug_id),
                 GET_DRUG_LAST_WITHDRAWAL_TICK(target, drug_id),
                 GET_DRUG_DURATION(target, drug_id),
                 GET_DRUG_DOSE(target, drug_id),
                 GET_DRUG_TOLERANCE_LEVEL(target, drug_id),
                 GET_DRUG_LIFETIME_DOSES(target, drug_id),
                 GET_DRUG_STAGE(target, drug_id));
  }

  if (!printed_something)
    send_to_char(" - Nothing.\r\n", viewer);
}

void attempt_safe_withdrawal(struct char_data *ch, const char *target_arg) {
  if (!PLR_FLAGGED(ch, PLR_ENABLED_DRUGS)) {
    send_to_char("You haven't enabled the drug system.\r\n", ch);
    return;
  }

  // Identify which drug they're withdrawing from.
  int drug_id;
  for (drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
    if (!str_cmp(drug_types[drug_id].name, target_arg)) {
      break;
    }
  }

  // Require that they entered a valid drug name.
  if (drug_id == NUM_DRUGS) {
    send_to_char(ch, "You don't know of any drug called '%s'.\r\n", target_arg);
    return;
  }

  // Check that they're addicted to it-- bail if not.
  if (!GET_DRUG_ADDICT(ch, drug_id)) {
    send_to_char(ch, "You're not addicted to %s.\r\n", drug_types[drug_id].name);
    return;
  }

  // You can only begin treatment when you're not high or coming down.
  if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_ONSET || GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_COMEDOWN) {
    send_to_char(ch, "You'll have to get the %s out of your system first.\r\n", drug_types[drug_id].name);
    return;
  }

  // If you're already withdrawing, no need to be here.
  if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_GUIDED_WITHDRAWAL) {
    send_to_char(ch, "You're already undergoing safe withdrawal from %s.\r\n", drug_types[drug_id].name);
    return;
  }

  // You can't begin guided while undergoing forced.
  if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_FORCED_WITHDRAWAL) {
    send_to_char(ch, "Your craving for %s is too great for you to stomach this process. You have to find a fix before you can begin guided withdrawal.\r\n", drug_types[drug_id].name);
    return;
  }

  // Charge them for it.
  int treatment_cost = GUIDED_WITHDRAWAL_ATTEMPT_NUYEN_COST_PER_EDGE * GET_DRUG_ADDICTION_EDGE(ch, drug_id);
  int total_cost_to_begin = treatment_cost + _seek_drugs_purchase_cost(ch, drug_id);
  if (GET_NUYEN(ch) < total_cost_to_begin) {
    send_to_char(ch, "You can't afford the chems to start the process. You need %d cash nuyen-- no credsticks allowed.\r\n", total_cost_to_begin);
    return;
  } else {
    lose_nuyen(ch, treatment_cost, NUYEN_OUTFLOW_DRUGS);
  }

  // Test for success. If they fail, they immediately seek out drugs.
  if (!_combined_addiction_test(ch, drug_id, "guided withdrawal", TRUE)) {
    send_to_char(ch, "Even with the chems, you can't kick the urge for %s.\r\n", drug_types[drug_id].name);
    seek_drugs(ch, drug_id);
    return;
  }

  // Put them in safe withdrawal.
  send_to_char(ch, "With the aid of a few chemical inducements, you begin the process of withdrawal from %s.\r\n", drug_types[drug_id].name);
  _put_char_in_withdrawal(ch, drug_id, TRUE);

  return;
}

// The more drugs you're on, the slower your healing.
float get_drug_heal_multiplier(struct char_data *ch) {
  char oopsbuf[500];
  int divisor = 1;

  if (!PLR_FLAGGED(ch, PLR_ENABLED_DRUGS)) {
    return 1;
  }

  for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
    switch (GET_DRUG_STAGE(ch, drug_id)) {
      case DRUG_STAGE_UNAFFECTED:
        break;
      case DRUG_STAGE_ONSET:
        divisor += 1;
        break;
      case DRUG_STAGE_COMEDOWN:
        divisor += 2;
        break;
      case DRUG_STAGE_GUIDED_WITHDRAWAL:
        divisor += 3;
        break;
      case DRUG_STAGE_FORCED_WITHDRAWAL:
        divisor += 5;
        break;
      default:
        snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Unknown drug state %d to get_drug_heal_multiplier()!", GET_DRUG_STAGE(ch, drug_id));
        mudlog(oopsbuf, ch, LOG_SYSLOG, TRUE);
        return 1.0;
    }
  }

  // Returns a value in range 0.1 ≤ X ≤ 1.0.
  return MIN(1.0, MAX(0.1, 2.0 / divisor));
}

// ----------------- Helpers
// Take a character off of a drug without changing their addiction values etc.
void reset_drug_for_char(struct char_data *ch, int drugval) {
  GET_DRUG_DOSE(ch, drugval) = GET_DRUG_DURATION(ch, drugval) = 0;
  GET_DRUG_STAGE(ch, drugval) = DRUG_STAGE_UNAFFECTED;
  update_withdrawal_flags(ch);
}

// Take a character off of all drugs without changing addiction values etc.
void reset_all_drugs_for_char(struct char_data *ch) {
  for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
    reset_drug_for_char(ch, drug_id);
  }
}

// Wipe out ALL drug data including tolerance, addiction etc-- used for staff only.
void clear_all_drug_data_for_char(struct char_data *ch) {
  for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
    for (int x = 0; x < NUM_DRUG_PLAYER_SPECIAL_FIELDS; x++) {
      ch->player_specials->drugs[drug_id][x] = 0;
    }
  }
}

int get_drug_pain_resist_level(struct char_data *ch) {
  if (ch->player_specials) {
    if (GET_DRUG_STAGE(ch, DRUG_NITRO) == DRUG_STAGE_ONSET) {
      return 6;
    }
    else if (GET_DRUG_STAGE(ch, DRUG_KAMIKAZE) == DRUG_STAGE_ONSET) {
      return 4;
    }
    else if (GET_DRUG_STAGE(ch, DRUG_BLISS) == DRUG_STAGE_ONSET) {
      return 3;
    }
    else if (GET_DRUG_STAGE(ch, DRUG_NOVACOKE) == DRUG_STAGE_ONSET) {
      return 1;
    }
  }

  return 0;
}

bool _process_edge_and_tolerance_changes_for_applied_dose(struct char_data *ch, int drug_id) {
  char rbuf[1000];

  bool is_first_time_taking = (GET_DRUG_LIFETIME_DOSES(ch, drug_id) - GET_DRUG_DOSE(ch, drug_id) <= 0);

  // Increase our tolerance and addiction levels if we've passed the edge value.
  int edge_value = (GET_DRUG_ADDICT(ch, drug_id) ? drug_types[drug_id].edge_posadd : drug_types[drug_id].edge_preadd);
  int edge_delta = 0;
  if (edge_value > 0) {
    edge_delta = ((GET_DRUG_LIFETIME_DOSES(ch, drug_id) % edge_value) + GET_DRUG_DOSE(ch, drug_id)) / edge_value;
    if (edge_delta > 0) {
      GET_DRUG_ADDICTION_EDGE(ch, drug_id) += edge_delta;
      GET_DRUG_TOLERANCE_LEVEL(ch, drug_id) += edge_delta;
    }
    snprintf(rbuf, sizeof(rbuf), "Edge rating: %d, edge delta: %d.", edge_value, edge_delta);
    act(rbuf, FALSE, ch, 0, 0, TO_ROLLS);
  }

  // Check to see if they become addicted.
  if (GET_DRUG_ADDICT(ch, drug_id) == NOT_ADDICTED && (edge_delta > 0 || is_first_time_taking)) {
    // It's their first dose, or they've taken more than Edge doses.
    if (!_combined_addiction_test(ch, drug_id, "application-time")) {
      // Character failed their addiction check and has become addicted.
      GET_DRUG_ADDICT(ch, drug_id) = IS_ADDICTED;
      GET_DRUG_ADDICTION_EDGE(ch, drug_id) = 1;
    }
  }

  // Check to see if their tolerance increases.
  if (drug_types[drug_id].tolerance > 0 && (edge_delta > 0 || is_first_time_taking)) {
    if (!_combined_addiction_test(ch, drug_id, "tolerance"))
      GET_DRUG_TOLERANCE_LEVEL(ch, drug_id)++;
  }

  // Deal a box of damage every time you onboard at least 2*Body rating in doses, and one additional per Bod doses past that.
  int bod_vs_doses = (GET_DRUG_DOSE(ch, drug_id) / GET_REAL_BOD(ch)) - 1;
  if (bod_vs_doses > 0) {
    send_to_char(ch, "You cough from the heavy dose of %s.\r\n", drug_types[drug_id].name);
    if (damage(ch, ch, bod_vs_doses, TYPE_DRUGS, TRUE)) {
      return TRUE;
    }
  }

  return FALSE;
}

bool _apply_doses_of_drug_to_char(int doses, int drug_id, struct char_data *ch) {
  if (!PLR_FLAGGED(ch, PLR_ENABLED_DRUGS)) {
    mudlog("SYSERR: Got to _apply_doses_of_drug_to_char() for character who did not enable the drug system!", ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  // Add the number of doses to both the current and lifetime doses.
  GET_DRUG_LIFETIME_DOSES(ch, drug_id) += doses;
  GET_DRUG_DOSE(ch, drug_id) += doses;

  return FALSE;
}

bool _drug_dose_exceeds_tolerance(struct char_data *ch, int drug_id) {
  return GET_DRUG_DOSE(ch, drug_id) > 0 && GET_DRUG_DOSE(ch, drug_id) > GET_DRUG_TOLERANCE_LEVEL(ch, drug_id);
}

// Perform a test against mental or physical ratings.
bool _specific_addiction_test(struct char_data *ch, int drug_id, bool is_mental, const char *test_identifier, bool guided_withdrawal_modifier) {
  int dice, base_addiction_rating;

  // M&M p108: Use unaugmented wil or body for these tests.
  if (is_mental) {
    dice = GET_REAL_WIL(ch);
    base_addiction_rating = drug_types[drug_id].mental_addiction_rating;
    if (guided_withdrawal_modifier)
      base_addiction_rating -= 1;
  } else {
    dice = GET_REAL_BOD(ch) + (GET_RACE(ch) == RACE_DWARF ? 2 : 0);
    base_addiction_rating = drug_types[drug_id].physical_addiction_rating;
    if (guided_withdrawal_modifier)
      base_addiction_rating -= 3;
  }

  // Being trapped in a drug addiction loop is not fun, so we cap the TN here.
  int tn = MIN(MAX_ADDICTION_TEST_DIFFICULTY, base_addiction_rating + GET_DRUG_ADDICTION_EDGE(ch, drug_id));

  int num_successes = success_test(dice, tn, ch, "specific addiction test");
  bool addiction_passed = (num_successes > 0);

  snprintf(buf, sizeof(buf), "%s's %s %s addiction %s test: %s(%d) vs addiction factor(%d) + edge(%d) (capped: %s: final TN %d) = %d hits (%s).",
           GET_CHAR_NAME(ch),
           drug_types[drug_id].name,
           is_mental ? "mental" : "physical",
           test_identifier,
           is_mental ? "wil" : "bod",
           dice,
           base_addiction_rating,
           GET_DRUG_ADDICTION_EDGE(ch, drug_id),
           tn != base_addiction_rating + GET_DRUG_ADDICTION_EDGE(ch, drug_id) ? "yes" : "no",
           tn,
           num_successes,
           addiction_passed ? "passed" : "failed, RIP");
  act(buf, FALSE, ch, 0, 0, TO_ROLLS);

  return addiction_passed;
}

bool _combined_addiction_test(struct char_data *ch, int drug_id, const char *test_identifier, bool is_guided_withdrawal_check) {
  return    _specific_addiction_test(ch, drug_id, FALSE, test_identifier, is_guided_withdrawal_check)
         && _specific_addiction_test(ch, drug_id, TRUE, test_identifier, is_guided_withdrawal_check);
}

struct room_data *_get_random_drug_seller_room() {
  vnum_t vnums[] = {
    RM_AGGIES_DRUG_STORE,
    RM_DETENTION_CELL_A,
    RM_HEARL_STREET,
    // RM_ALONG_THIRD_STREET,  /* not unless visa-free return is available */
    RM_ENTRANCE_TO_DANTES,
    RM_MASSAGE_PARLOR,
    RM_ABANDONED_CLOTHING_STORE
  };
  size_t num_vnums = 6;

  vnum_t vnum = vnums[number(0, num_vnums - 1)];

  rnum_t rnum = real_room(vnum);
  if (rnum < 0) {
    char oopsbuf[500];
    snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Invalid vnum %ld specified in _get_random_drug_seller_room()!", vnum);
    mudlog(oopsbuf, NULL, LOG_SYSLOG, TRUE);
    rnum = real_room(RM_ENTRANCE_TO_DANTES);
  }

  return &world[rnum];
}

bool seek_drugs(struct char_data *ch, int drug_id) {
  if (GET_PHYSICAL(ch) <= 0 || GET_MENTAL(ch) <= 0) {
    // Skip-- stunned or morted.
    return FALSE;
  }

  if (GET_POS(ch) <= POS_SITTING) {
    GET_POS(ch) = POS_STANDING;
  }

  int sought_dosage = 1 + GET_DRUG_TOLERANCE_LEVEL(ch, drug_id) * 1.5;
  int dosage_cost = _seek_drugs_purchase_cost(ch, drug_id);

  send_to_char(ch, "You enter a fugue state, wandering the streets in search of more %s!\r\n", drug_types[drug_id].name);
  act("$n enters a fugue state and wanders off!", TRUE, ch, 0, 0, TO_ROOM);

  if (GET_NUYEN(ch) < dosage_cost && GET_BANK(ch) + GET_NUYEN(ch) < dosage_cost) {
    send_to_char("Without enough cash nuyen in your pockets to cover your drug habit, things take a turn for the worse...\r\n", ch);
    lose_nuyen(ch, GET_NUYEN(ch), NUYEN_OUTFLOW_DRUGS);

    char_from_room(ch);
    char_to_room(ch, _get_random_drug_seller_room());
    WAIT_STATE(ch, 10 RL_SEC);

    GET_PHYSICAL(ch) = MIN(GET_PHYSICAL(ch), 100);
    GET_MENTAL(ch) = MIN(GET_MENTAL(ch), 0);
    update_pos(ch);
    return FALSE;
  } else {
    if (GET_NUYEN(ch) >= dosage_cost) {
      lose_nuyen(ch, dosage_cost, NUYEN_OUTFLOW_DRUGS);
    } else {
      lose_bank(ch, dosage_cost - GET_NUYEN(ch), NUYEN_OUTFLOW_DRUGS);
      lose_nuyen(ch, GET_NUYEN(ch), NUYEN_OUTFLOW_DRUGS);
    }

    if (_apply_doses_of_drug_to_char(sought_dosage, drug_id, ch)) {
      return TRUE;
    }

    send_to_char("By the time you recover, you're somewhere else entirely, with a familiar feeling running through your veins...\r\n", ch);
    update_withdrawal_flags(ch);

    char_from_room(ch);
    char_to_room(ch, _get_random_drug_seller_room());
    WAIT_STATE(ch, 2 RL_SEC);

    char blank_buf[10];
    strlcpy(blank_buf, "", sizeof(blank_buf));
    do_look(ch, blank_buf, 0, 0);
  }
  return FALSE;
}

void update_withdrawal_flags(struct char_data *ch) {
  AFF_FLAGS(ch).RemoveBits(AFF_WITHDRAWAL_FORCE, AFF_WITHDRAWAL, ENDBIT);
  for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
    if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_GUIDED_WITHDRAWAL)
      AFF_FLAGS(ch).SetBit(AFF_WITHDRAWAL);
    else if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_FORCED_WITHDRAWAL)
      AFF_FLAGS(ch).SetBit(AFF_WITHDRAWAL_FORCE);
  }

  affect_total(ch);
}

const char *get_time_until_withdrawal_ends(struct char_data *ch, int drug_id) {
  static char time_buf[20];

  // How many days must elapse in total before we're off the drug?
  int ig_days = GET_DRUG_ADDICTION_EDGE(ch, drug_id);
  int irl_secs = ig_days * SECS_PER_MUD_DAY;
  int irl_mins = (irl_secs / 60);

  if (irl_secs >= 60)
    snprintf(time_buf, sizeof(time_buf), "%d minute%s", irl_mins, irl_mins != 1 ? "s" : "");
  else
    snprintf(time_buf, sizeof(time_buf), "%d second%s", irl_secs, irl_secs != 1 ? "s" : "");

  return (const char *) time_buf;
}

int _seek_drugs_purchase_cost(struct char_data *ch, int drug_id) {
  int involuntary_dose_qty = 1 + GET_DRUG_TOLERANCE_LEVEL(ch, drug_id) * 1.5;
  int involuntary_purchase_cost_per_dose = drug_types[drug_id].cost * drug_types[drug_id].street_idx;
  int involuntary_purchase_total = INVOLUNTARY_DRUG_PURCHASE_COST_MULTIPLIER * involuntary_purchase_cost_per_dose * involuntary_dose_qty;

  return involuntary_purchase_total;
}

void _put_char_in_withdrawal(struct char_data *ch, int drug_id, bool is_guided) {
  if (is_guided) {
    GET_DRUG_STAGE(ch, drug_id) = DRUG_STAGE_GUIDED_WITHDRAWAL;
  } else {
    GET_DRUG_STAGE(ch, drug_id) = DRUG_STAGE_FORCED_WITHDRAWAL;
  }

  GET_DRUG_LAST_WITHDRAWAL_TICK(ch, drug_id) = 1; // Set to 1 so we don't tick on the first day.
  GET_DRUG_ADDICTION_TICK_COUNTER(ch, drug_id) = 0;
  update_withdrawal_flags(ch);
}

bool _take_anti_drug_chems(struct char_data *ch, int drug_id) {
  int doses_required = GET_DRUG_ADDICTION_EDGE(ch, drug_id);
  struct obj_data *next_obj;

  for (struct obj_data *chems = ch->carrying; chems; chems = next_obj) {
    next_obj = chems->next_content;

    if (GET_OBJ_VNUM(chems) == OBJ_ANTI_DRUG_CHEMS) {
      int chems_avail = GET_CHEMS_QTY(chems);

      if (chems_avail >= doses_required) {
        GET_CHEMS_QTY(chems) -= doses_required;
        weight_change_object(chems, doses_required * 0.01);

        if (GET_CHEMS_QTY(chems) <= 0) {
          send_to_char(ch, "You take the edge off your %s addiction with the last dose from %s.\r\n", drug_types[drug_id].name, GET_OBJ_NAME(chems));
          extract_obj(chems);
        } else {
          send_to_char(ch, "You take the edge off your %s addiction with a dose of %s.\r\n", drug_types[drug_id].name, GET_OBJ_NAME(chems));
        }
        return TRUE;
      } else {
        send_to_char(ch, "You knock back the last dose%s from %s, but it's%snot quite enough.\r\n",
                     GET_CHEMS_QTY(chems) == 1 ? "" : "s",
                     GET_OBJ_NAME(chems),
                     doses_required != GET_DRUG_ADDICTION_EDGE(ch, drug_id) ? " still" : " ");
        doses_required -= GET_CHEMS_QTY(chems);
        GET_CHEMS_QTY(chems) = 0;
        extract_obj(chems);
      }
    }
  }

  if (doses_required != GET_DRUG_ADDICTION_EDGE(ch, drug_id)) {
    send_to_char("You weren't able to find enough chems to stave off the itching in your veins...\r\n", ch);
  } else {
    send_to_char("You could really use some anti-craving chems right about now.\r\n", ch);
  }
  return FALSE;
}

// ----------------- Definitions for Drugs
struct drug_data drug_types[] =
  {
    // name,   power,   level, m_add, p_add, toler, edge_preadd, edge_posadd,  fix, cost, street_idx, d_method
    { "Nothing",   0,       0,     0,     0,     0,           0,           0,    0,    0,          0,       ""
    },
    { "ACTH",      0,   LIGHT,     0,     0,     3,          10,           0,    0,  100,          1,  "tablet" },
    { "Hyper",     6, SERIOUS,     0,     0,     0,           0,           0,    0,  180,        0.9,    "pill" },
    { "Jazz",      0,       0,     4,     5,     2,           2,           5,    3,   40,          3, "inhaler" },
    { "Kamikaze",  0,       0,     0,     5,     2,           2,           6,    2,   50,          5, "inhaler" },
    { "Psyche",    0,       4,     0,     2,    10,           8,           4,    0,  500,          2,    "pill" },
    { "Bliss",     0,       0,     5,     5,     2,           2,          10,    2,   15,          2, "syringe" },
    { "Burn",      3,  DEADLY,     2,     0,     2,          10,         100,    1,    5,          1,  "bottle" },
    { "Cram",      0,       0,     4,     0,     2,           5,          25,    2,   20,          1, "capsule" },
    { "Nitro",     4,  DEADLY,     5,     8,     3,           2,           3,    3,  100,          1,    "dose" },
    { "Novacoke",  0,       0,     6,     5,     2,           3,          25,    2,   20,          1,  "baggie" },
    { "Zen",       0,       0,     3,     0,     2,           5,          25,    2,    5,          1,   "blunt" }
  };
