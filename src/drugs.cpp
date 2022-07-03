#include "awake.hpp"
#include "structs.hpp"
#include "comm.hpp"
#include "db.hpp"
#include "utils.hpp"
#include "handler.hpp"
#include "constants.hpp"
#include "interpreter.hpp"

/* Houserules:
  - We don't have drug families / immunities, so we model tolerance as increasing the number of doses you must take to get high.
  - We do not reduce any maximums, although you can lose stats down to racial minimum from withdrawal.
  - Hyper deals up-front damage because we don't model the increased damage otherwise.
  - Kamikaze damages you when you come down.
*/

/* When taking drugs:
   Trigger do_drug_take to consume the object and start this process, otherwise start it manually with apply_doses_of_drug_to_char.

   - wait for limit tick, where we call process_drug_point_update_tick(), which transitions between onset/comedown/idle
   - on X tick, process_withdrawal inflicts withdrawal symptoms, and also induces withdrawal for those who have gone without a fix
   - on X tick, apply_drug_modifiers_to_ch modifiers char stats
*/

// TODO: Add guided withdrawal sequence.
// TODO: Add a conversion pass in parse_obj that sets all 0-dose drugs to 1 dose.

extern int raw_stat_loss(struct char_data *);
extern bool check_adrenaline(struct char_data *, int);

ACMD_DECLARE(do_use);

// ----------------- Helper Prototypes
void _apply_doses_of_drug_to_char(int doses, int drug_id, struct char_data *ch);
bool _drug_dose_exceeds_tolerance(struct char_data *ch, int drug_id);
bool _specific_addiction_test(struct char_data *ch, int drug_id, bool is_mental, const char *test_identifier);
bool _combined_addiction_test(struct char_data *ch, int drug_id, const char *test_identifier);



// Given a character and a drug object, dose the character with that drug object, then extract it if needed. Effects apply at next limit tick.
void do_drug_take(struct char_data *ch, struct obj_data *obj) {
  char oopsbuf[500];

  if (!ch) {
    mudlog("SYSERR: Received NULL character to do_drug_take()!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  if (!obj || GET_OBJ_TYPE(obj) != ITEM_DRUG) {
    snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Received non-drug object %s to do_drug_take!", obj ? GET_OBJ_NAME(obj) : "NULL");
    mudlog(oopsbuf, ch, LOG_SYSLOG, TRUE);
    return;
  }

  int drug_id = GET_OBJ_DRUG_TYPE(obj);

  // You can't take more of a drug you're already high on.
  if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_ONSET) {
    send_to_char(ch, "You're already high on %s, it would be a bad idea to take more right now.\r\n", drug_types[drug_id].name);
    return;
  }

  // Right now, we apply maximum doses from the object. Later, we'll be smarter about this.
  // Many drugs exist without doses set, so we assume 0 doses means the drug is an old-style drug.
  int drug_doses = GET_OBJ_DRUG_DOSES(obj) > 0 ? GET_OBJ_DRUG_DOSES(obj) : 1;

  // Add doses to their current on-board as well as their lifetime total.
  _apply_doses_of_drug_to_char(drug_doses, drug_id, ch);

  // Message the room.
  act("$n takes $p.", TRUE, ch, obj, 0, TO_ROOM);

  // The message we give the character depends on if they've passed their tolerance dose yet.
  if (!_drug_dose_exceeds_tolerance(ch, drug_id)) {
    // They didn't take enough. Give the appropriate message.
    send_to_char(ch, "You take %s, but it doesn't kick in-- you'll need to take more to surpass your tolerance.\r\n", GET_OBJ_NAME(obj));
  } else {
    send_to_char(ch, "You take %s.\r\n", GET_OBJ_NAME(obj));
  }

  // Remove the doses from the drug.
  GET_OBJ_DRUG_DOSES(obj) -= drug_doses;
  if (GET_OBJ_DRUG_DOSES(obj) <= 0) {
    // The drug has been finished off.
    extract_obj(obj);
  }

  // That's it! All other checks are done when the drug kicks in.
}

// Tick down all drugs for a given character. Induces onset/comedown etc. Called once per IG minute.
bool process_drug_point_update_tick(struct char_data *ch) {
  char roll_buf[500];
  bool is_on_anything = FALSE;

  for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
    int damage_boxes = 0;

    if (GET_DRUG_STAGE(ch, drug_id) != DRUG_STAGE_UNAFFECTED)
      is_on_anything = TRUE;

    // All drugs are ticked down as long as you're on them.
    if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_ONSET || GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_COMEDOWN)
      GET_DRUG_DURATION(ch, drug_id) -= 1;

    // Transition from no drug to having it, assuming you've passed your tolerance threshold.
    if (GET_DRUG_STAGE(ch, drug_id) != DRUG_STAGE_ONSET && _drug_dose_exceeds_tolerance(ch, drug_id)) {
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
        int resist_test_successes = success_test(GET_REAL_BOD(ch) - (GET_BIOOVER(ch) / 2), power);

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
      GET_DRUG_LAST_FIX(ch, drug_id) = time(0);

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
          send_to_char(ch, "The world seems to swirl around you as your mind is bombarded with feedback.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = damage_boxes * 100;
          break;
        case DRUG_JAZZ:
          send_to_char(ch,  "The world slows down around you.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = 100 * srdice();
          break;
        case DRUG_KAMIKAZE:
          send_to_char(ch,  "Your body feels alive with energy and the desire to fight.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = 100 * srdice();
          break;
        case DRUG_PSYCHE:
          send_to_char(ch,  "Your feel your mind racing.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = MAX(1, 12 - GET_REAL_BOD(ch)) * 600;
          break;
        case DRUG_BLISS:
          send_to_char(ch,  "The world fades into bliss as your body becomes sluggish.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = MAX(1, 6 - GET_REAL_BOD(ch)) * 600;
          break;
        case DRUG_BURN:
          send_to_char(ch,  "You suddenly feel very intoxicated.\r\n");
          // Burn's long-term effects are done through the drunk code.
          reset_drug_for_char(ch, drug_id);
          GET_COND(ch, COND_DRUNK) = FOOD_DRINK_MAX;
          break;
        case DRUG_CRAM:
          send_to_char(ch,  "Your body feels alive with energy.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = MAX(1, 12 - GET_REAL_BOD(ch)) * 600;
          break;
        case DRUG_NITRO:
          send_to_char(ch,  "You lose sense of yourself as your entire body comes alive with energy.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = 100 * srdice();
          break;
        case DRUG_NOVACOKE:
          send_to_char(ch,  "You feel euphoric and alert.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = MAX(1, 10 - GET_REAL_BOD(ch)) * 600;
          break;
        case DRUG_ZEN:
          send_to_char(ch,  "You start to lose your sense of reality as your sight fills with hallucinations.\r\n");
          GET_DRUG_DURATION(ch, drug_id) = 100 * srdice();
          break;
        default:
          snprintf(buf, sizeof(buf), "SYSERR: Unknown drug type %d when printing drug-start message!", drug_id);
          mudlog(buf, ch, LOG_SYSLOG, TRUE);
          break;
      }

      // Staff get short uptimes for testing.
      if (GET_LEVEL(ch) > LVL_MORTAL && GET_DRUG_DURATION(ch, drug_id) > 0) {
        send_to_char(ch, "Staff: Accelerating drug onset duration from %d to 4s for easier testing.", GET_DRUG_DURATION(ch, drug_id));
        GET_DRUG_DURATION(ch, drug_id) = 2;
      }
    }

    // They're already on it and lost their last tick of uptime. Switch to comedown.
    else if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_ONSET && GET_DRUG_DURATION(ch, drug_id) <= 0) {
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
          GET_DRUG_DURATION(ch, drug_id) = 100 * srdice();
          if (damage(ch, ch, convert_damage(stage(-success_test(bod_for_success_test, 8 - toxin_extractor_rating), LIGHT)), TYPE_BIOWARE, 0)) {
            return TRUE;
          }
          break;
        case DRUG_KAMIKAZE:
          GET_DRUG_DURATION(ch, drug_id) = 100 * srdice();
          {
            // Canonically, this should not hurt this much-- but we don't have a concept of cyberware/bioware system damage here,
            // so instead of applying permanent subsystem damage, we just deal CHONKY damage that increases based on the number of doses you took.
            int damage_level = LIGHT + GET_DRUG_DOSE(ch, DRUG_KAMIKAZE);
            int power = 12 - toxin_extractor_rating;
            int resist_test_successes = success_test(bod_for_success_test, power);
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
            int resist_test_successes = success_test(bod_for_success_test, power);
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
        send_to_char(ch, "Staff; Accelerating drug comedown duration from %d to 4s for easier testing.", GET_DRUG_DURATION(ch, drug_id));
        GET_DRUG_DURATION(ch, drug_id) = 2;
      }
    }

    // Your comedown has worn off.
    else if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_COMEDOWN && GET_DRUG_DURATION(ch, drug_id) <= 0) {
      send_to_char(ch, "The aftereffects of the %s wear off.\r\n", drug_types[drug_id].name);
      reset_drug_for_char(ch, drug_id);
      if (AFF_FLAGGED(ch, AFF_DETOX))
        AFF_FLAGS(ch).RemoveBit(AFF_DETOX);
    }
  }

  // Once you're clean of all drugs, you lose the detox aff.
  if (!is_on_anything)
    AFF_FLAGS(ch).RemoveBit(AFF_DETOX);

  return FALSE;
}

// Apply the onset / comedown modifiers to the selected character.
int noop_var_for_perception_test = 0;
#define GET_PERCEPTION_TEST_DICE_MOD(ch) noop_var_for_perception_test /* does nothing-- I added this to track the work we need to eventually do around drug perception tests.*/
void apply_drug_modifiers_to_ch(struct char_data *ch) {
  GET_PERCEPTION_TEST_DICE_MOD(ch) = 0;

  if (!AFF_FLAGGED(ch, AFF_DETOX)) {
    for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
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
  }

  if (AFF_FLAGGED(ch, AFF_WITHDRAWAL_FORCE)) {
    GET_MAX_MENTAL(ch) -= 300;
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
  // Iterate through all drugs.
  for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
    // Tick up addiction / withdrawal stat losses. Houseruled from Addiction Effects (M&M p109).
    if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_GUIDED_WITHDRAWAL || GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_FORCED_WITHDRAWAL) {
      // Calculate time since last fix. (What is this used for?)
      int time_since_last_fix = (time(0) - GET_DRUG_LAST_FIX(ch, drug_id)) / SECS_PER_MUD_DAY;

      // Increment our addiction tick counter.
      GET_DRUG_ADDICTION_TICK_COUNTER(ch, drug_id)++;

      // Process stat loss as appropriate.
      if (GET_DRUG_STAGE(ch, drug_id == DRUG_STAGE_FORCED_WITHDRAWAL)) {
        bool body_is_above_racial_minimums = GET_REAL_BOD(ch) > 1 + racial_attribute_modifiers[(int) GET_RACE(ch)][BOD];
        bool should_test_for_stat_loss = GET_DRUG_ADDICTION_TICK_COUNTER(ch, drug_id) % (24 * 30 /* aka once a month, tested IG hourly */) == 0;

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
        // Decrement their edge, allowing their addiction rating to decrease
        if (time_since_last_fix > GET_DRUG_LAST_WITHDRAWAL_TICK(ch, drug_id) + 1) {
          GET_DRUG_LAST_WITHDRAWAL_TICK(ch, drug_id) += (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_GUIDED_WITHDRAWAL ? 2 : 1);
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
        if (time_since_last_fix >= drug_types[drug_id].fix_factor) {
          if (!_combined_addiction_test(ch, drug_id, "auto-take resistance")) {
            // Compose message, which is replaced by craving messages if they're carrying drugs.
            if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_FORCED_WITHDRAWAL) {
              snprintf(buf, sizeof(buf), "Your lack of %s is causing you great pain and discomfort.\r\n", drug_types[drug_id].name);
            } else {
              snprintf(buf, sizeof(buf), "You crave some %s.\r\n", drug_types[drug_id].name);
            }

            // Auto-use drugs if available in inventory.
            for (struct obj_data *obj = ch->carrying; obj; obj = obj->next_content) {
              if (GET_OBJ_TYPE(obj) == ITEM_DRUG && GET_OBJ_DRUG_TYPE(obj) == drug_id) {
                do_use(ch, obj->text.keywords, 0, 0);
                if (GET_DRUG_DOSE(ch, drug_id) > GET_DRUG_TOLERANCE_LEVEL(ch, drug_id))
                  snprintf(buf, sizeof(buf), "You satisfy your craving for %s.\r\n", drug_types[drug_id].name);
                else
                  snprintf(buf, sizeof(buf), "You attempt to satisfy your craving for %s.\r\n", drug_types[drug_id].name);
                break;
              }
            }
            send_to_char(buf, ch);
          }
        }
      }

      // They weren't in withdrawal: Put them into that state if they've passed their fix limit.
      else if (time_since_last_fix > drug_types[drug_id].fix_factor) {
        send_to_char(ch, "You begin to go into forced %s withdrawal.\r\n", drug_types[drug_id].name);
        GET_DRUG_STAGE(ch, drug_id) = DRUG_STAGE_FORCED_WITHDRAWAL;
        affect_total(ch);
      }
    }
  }

  // Re-apply withdrawal flags as appropriate.
  AFF_FLAGS(ch).RemoveBits(AFF_WITHDRAWAL_FORCE, AFF_WITHDRAWAL, ENDBIT);
  for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
    if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_GUIDED_WITHDRAWAL)
      AFF_FLAGS(ch).SetBit(AFF_WITHDRAWAL);
    else if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_FORCED_WITHDRAWAL)
      AFF_FLAGS(ch).SetBit(AFF_WITHDRAWAL_FORCE);
  }
}

void render_drug_info_for_targ(struct char_data *ch, struct char_data *targ) {
  // Send drug info
  send_to_char("\r\nDrug info: \r\n", ch);
  for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
    send_to_char(ch, " - ^c%s^n (edg %d, add %d, doses %d, lstfx %d, addt %d, toler %d, lstwith %d, dur %d, current dose %d, stage %d)\r\n",
                 drug_types[drug_id].name,
                 GET_DRUG_ADDICTION_EDGE(targ, drug_id),
                 GET_DRUG_ADDICT(targ, drug_id),
                 GET_DRUG_LIFETIME_DOSES(targ, drug_id),
                 GET_DRUG_LAST_FIX(targ, drug_id),
                 GET_DRUG_ADDICTION_TICK_COUNTER(targ, drug_id),
                 GET_DRUG_TOLERANCE_LEVEL(targ, drug_id),
                 GET_DRUG_LAST_WITHDRAWAL_TICK(targ, drug_id),
                 GET_DRUG_DURATION(targ, drug_id),
                 GET_DRUG_DOSE(targ, drug_id),
                 GET_DRUG_STAGE(targ, drug_id));
  }
}

// ----------------- Helpers
// Take a character off of a drug without changing their addiction values etc.
void reset_drug_for_char(struct char_data *ch, int drugval) {
  GET_DRUG_DOSE(ch, drugval) = GET_DRUG_DURATION(ch, drugval) = 0;
  GET_DRUG_STAGE(ch, drugval) = DRUG_STAGE_UNAFFECTED;
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
    for (int x = 0; x <= 10; x++) {
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

void _apply_doses_of_drug_to_char(int doses, int drug_id, struct char_data *ch) {
  /* Every time you take a drug:
     - Check if total number of doses taken exceeds the appropriate Edge (pre- or post-addiction, as applicable).
       - If it does, -= it down until it's below the Edge rating, and add the number of times you decreased it to both Addiction and Tolerance.
     - Add one to the number of active doses and the total number of doses taken.
  */

  // Increase our tolerance and addiction levels if we've passed the edge value.
  int edge_value = (GET_DRUG_ADDICT(ch, drug_id) ? drug_types[drug_id].edge_posadd : drug_types[drug_id].edge_preadd);
  int edge_delta = ((GET_DRUG_DOSE(ch, drug_id) % edge_value) + doses) / edge_value;
  if (edge_delta > 0) {
    GET_DRUG_ADDICTION_EDGE(ch, drug_id)++;
    GET_DRUG_TOLERANCE_LEVEL(ch, drug_id)++;
  }

  // Check to see if they become addicted.
  if (GET_DRUG_ADDICT(ch, drug_id) == NOT_ADDICTED && (edge_delta > 0 || GET_DRUG_LIFETIME_DOSES(ch, drug_id) <= 0)) {
    // It's their first dose, or they've taken more than Edge doses.
    if (!_combined_addiction_test(ch, drug_id, "application-time")) {
      // Character failed their addiction check and has become addicted.
      GET_DRUG_ADDICT(ch, drug_id) = IS_ADDICTED;
      GET_DRUG_ADDICTION_EDGE(ch, drug_id) = 1;
    }
  }

  // Check to see if their tolerance increases. KNOWN ISSUE: This can cause your first dose to not have an effect.
  if (drug_types[drug_id].tolerance > 0 && (GET_DRUG_LIFETIME_DOSES(ch, drug_id) == 1 || edge_delta > 0)) {
    if (!_combined_addiction_test(ch, drug_id, "tolerance"))
      GET_DRUG_TOLERANCE_LEVEL(ch, drug_id)++;
  }

  // Add the number of doses to both the current and lifetime doses.
  GET_DRUG_LIFETIME_DOSES(ch, drug_id) += doses;
  GET_DRUG_DOSE(ch, drug_id) += doses;
}

bool _drug_dose_exceeds_tolerance(struct char_data *ch, int drug_id) {
  return GET_DRUG_DOSE(ch, drug_id) > 0 && GET_DRUG_DOSE(ch, drug_id) > GET_DRUG_TOLERANCE_LEVEL(ch, drug_id);
}

// Perform a test against mental or physical ratings.
bool _specific_addiction_test(struct char_data *ch, int drug_id, bool is_mental, const char *test_identifier) {
  int dice, base_addiction_rating;

  // M&M p108: Use unaugmented wil or body for these tests.
  if (is_mental) {
    dice = GET_REAL_WIL(ch);
    base_addiction_rating = drug_types[drug_id].mental_addiction_rating;
  } else {
    dice = GET_REAL_BOD(ch) + (GET_RACE(ch) == RACE_DWARF ? 2 : 0);
    base_addiction_rating = drug_types[drug_id].physical_addiction_rating;
  }

  int num_successes = success_test(dice, base_addiction_rating + GET_DRUG_ADDICTION_EDGE(ch, drug_id));
  bool addiction_passed = (num_successes > 0);

  snprintf(buf, sizeof(buf), "%s's %s %s addiction %s test: wil(%d) vs addiction factor(%d) + edge(%d) = %d hits (%s).",
           GET_CHAR_NAME(ch),
           drug_types[drug_id].name,
           is_mental ? "mental" : "physical",
           test_identifier,
           dice,
           base_addiction_rating,
           GET_DRUG_ADDICTION_EDGE(ch, drug_id),
           num_successes,
           addiction_passed ? "passed" : "failed, RIP");
  act(buf, FALSE, ch, 0, 0, TO_ROLLS);

  return addiction_passed;
}

bool _combined_addiction_test(struct char_data *ch, int drug_id, const char *test_identifier) {
  return _specific_addiction_test(ch, drug_id, FALSE, test_identifier) && _specific_addiction_test(ch, drug_id, TRUE, test_identifier);
}

// ----------------- Definitions for Drugs
struct drug_data drug_types[] =
  {
    // name,   power,   level, m_add, p_add, toler, edge_preadd, edge_posadd,  fix
    { "Nothing",   0,       0,     0,     0,     0,           0,           0,    0
    },
    { "ACTH",      0,   LIGHT,     0,     0,     3,          10,           0,    0 },
    { "Hyper",     6, SERIOUS,     0,     0,     0,           0,           0,    0 },
    { "Jazz",      0,       0,     4,     5,     2,           2,           8,    3 },
    { "Kamikaze",  0,       0,     0,     5,     2,           2,          10,    2 },
    { "Psyche",    0,       4,     0,     2,    10,          20,           7,    0 },
    { "Bliss",     0,       0,     5,     5,     2,           2,          30,    2 },
    { "Burn",      3,  DEADLY,     2,     0,     2,          20,         100,    1 },
    { "Cram",      0,       0,     4,     0,     2,           5,          50,    2 },
    { "Nitro",     4,  DEADLY,     5,     8,     3,           2,           5,    3 },
    { "Novacoke",  0,       0,     6,     5,     2,           3,          50,    2 },
    { "Zen",       0,       0,     3,     0,     2,           5,          50,    2 }
  };
