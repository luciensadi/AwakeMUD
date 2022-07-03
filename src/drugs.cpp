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
*/

/* When taking drugs:
   Trigger do_drug_take to consume the object and start this process, otherwise start it manually with apply_doses_of_drug_to_char.

   - wait for limit tick, where we call process_drug_point_update_tick(), which transitions between onset/comedown/idle
   - on X tick, process_withdrawal inflicts withdrawal symptoms, and also induces withdrawal for those who have gone without a fix
   - on X tick, apply_drug_modifiers_to_ch modifiers char stats
*/

// TODO: Make sure that taking a drug removes the withdrawal status.

extern int raw_stat_loss(struct char_data *);
extern bool check_adrenaline(struct char_data *, int);

ACMD_DECLARE(do_use);
ACMD_DECLARE(do_look);

// ----------------- Helper Prototypes
void _apply_doses_of_drug_to_char(int doses, int drug_id, struct char_data *ch);
bool _drug_dose_exceeds_tolerance(struct char_data *ch, int drug_id);
bool _specific_addiction_test(struct char_data *ch, int drug_id, bool is_mental, const char *test_identifier);
bool _combined_addiction_test(struct char_data *ch, int drug_id, const char *test_identifier, bool is_guided_withdrawal_check=FALSE);
void seek_drugs(struct char_data *ch, int drug_id);
void update_withdrawal_flags(struct char_data *ch);


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

      // Clear the dose.
      GET_DRUG_DOSE(ch, drug_id) = 0;
    }

    // Your comedown has worn off.
    else if (GET_DRUG_STAGE(ch, drug_id) == DRUG_STAGE_COMEDOWN && GET_DRUG_DURATION(ch, drug_id) <= 0) {
      send_to_char(ch, "The aftereffects of the %s wear off.\r\n", drug_types[drug_id].name);
      reset_drug_for_char(ch, drug_id);
      if (AFF_FLAGGED(ch, AFF_DETOX))
        AFF_FLAGS(ch).RemoveBit(AFF_DETOX);
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
  }

  // Once you're clean of all drugs, you lose the detox aff.
  if (!is_on_anything)
    AFF_FLAGS(ch).RemoveBit(AFF_DETOX);
  else {
    update_withdrawal_flags(ch);
  }

  return FALSE;
}

// Apply the onset / comedown modifiers to the selected character.
int noop_var_for_perception_test = 0;
#define GET_PERCEPTION_TEST_DICE_MOD(ch) noop_var_for_perception_test /* does nothing-- I added this to track the work we need to eventually do around drug perception tests.*/
void apply_drug_modifiers_to_ch(struct char_data *ch) {
  // Note: You CANNOT use update_withdrawal_flags() in this function. This is called from affect_total, and update_withdrawal_flags calls affect_total.
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
            bool found_drugs = FALSE;
            for (struct obj_data *obj = ch->carrying; obj; obj = obj->next_content) {
              if (GET_OBJ_TYPE(obj) == ITEM_DRUG && GET_OBJ_DRUG_TYPE(obj) == drug_id) {
                found_drugs = TRUE;
                do_use(ch, obj->text.keywords, 0, 0);
                if (GET_DRUG_DOSE(ch, drug_id) > GET_DRUG_TOLERANCE_LEVEL(ch, drug_id))
                  snprintf(buf, sizeof(buf), "You satisfy your craving for %s.\r\n", drug_types[drug_id].name);
                else
                  snprintf(buf, sizeof(buf), "You attempt to satisfy your craving for %s.\r\n", drug_types[drug_id].name);
                break;
              }
            }
            send_to_char(buf, ch);

            if (!found_drugs) {
              seek_drugs(ch, drug_id);
            }
          }
        }
      }

      // They weren't in withdrawal: Put them into that state if they've passed their fix limit.
      else if (time_since_last_fix > drug_types[drug_id].fix_factor) {
        send_to_char(ch, "You begin to go into forced %s withdrawal.\r\n", drug_types[drug_id].name);
        GET_DRUG_STAGE(ch, drug_id) = DRUG_STAGE_FORCED_WITHDRAWAL;
        update_withdrawal_flags(ch);
      }
    }
  }

  // Re-apply withdrawal flags as appropriate.
  update_withdrawal_flags(ch);
}

void render_drug_info_for_targ(struct char_data *ch, struct char_data *targ) {
  bool printed_something = FALSE;
  // Send drug info
  send_to_char("\r\nDrug info: \r\n", ch);

  for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
    bool should_show_this_drug = FALSE;
    for (int chk = 0; chk < NUM_DRUG_PLAYER_SPECIAL_FIELDS; chk++) {
      if (ch->player_specials->drugs[drug_id][chk]) {
        should_show_this_drug = TRUE;
        break;
      }
    }

    if (!should_show_this_drug)
      continue;

    printed_something = TRUE;
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

  if (!printed_something)
    send_to_char(" - Nothing.\r\n", ch);
}

void attempt_safe_withdrawal(struct char_data *ch, const char *target_arg) {
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

  // Charge them for it.
  int total_cost_to_begin = GUIDED_WITHDRAWAL_ATTEMPT_NUYEN_COST + (INVOLUNTARY_DRUG_PURCHASE_COST_PER_DOSE * (GET_DRUG_TOLERANCE_LEVEL(ch, drug_id) + 1));
  if (GET_NUYEN(ch) < total_cost_to_begin) {
    send_to_char(ch, "You can't afford the chems to start the process. You need %d cash nuyen-- no credsticks allowed.\r\n", total_cost_to_begin);
    return;
  } else {
    lose_nuyen(ch, GUIDED_WITHDRAWAL_ATTEMPT_NUYEN_COST, NUYEN_OUTFLOW_GENERIC_SPEC_PROC);
  }

  // Test for success. If they fail, they immediately seek out drugs.
  if (!_combined_addiction_test(ch, drug_id, "guided withdrawal", TRUE)) {
    send_to_char(ch, "Even with the chems, you can't kick the urge for %s.\r\n", drug_types[drug_id].name);
    seek_drugs(ch, drug_id);
    return;
  }

  // Put them in safe withdrawal.
  send_to_char(ch, "With the aid of a few chemical inducements, you begin the process of withdrawal from %s.\r\n", drug_types[drug_id].name);
  GET_DRUG_STAGE(ch, drug_id) = DRUG_STAGE_GUIDED_WITHDRAWAL;
  update_withdrawal_flags(ch);

  return;
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
  char rbuf[1000];

  // Increase our tolerance and addiction levels if we've passed the edge value.
  int edge_value = (GET_DRUG_ADDICT(ch, drug_id) ? drug_types[drug_id].edge_posadd : drug_types[drug_id].edge_preadd);
  int edge_delta = ((GET_DRUG_LIFETIME_DOSES(ch, drug_id) % edge_value) + doses) / edge_value;
  if (edge_delta > 0) {
    GET_DRUG_ADDICTION_EDGE(ch, drug_id)++;
    GET_DRUG_TOLERANCE_LEVEL(ch, drug_id)++;
  }
  snprintf(rbuf, sizeof(rbuf), "Edge rating: %d, edge delta: %d.", edge_value, edge_delta);
  act(rbuf, FALSE, ch, 0, 0, TO_ROLLS);

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
bool _specific_addiction_test(struct char_data *ch, int drug_id, bool is_mental, const char *test_identifier, bool guided_withdrawal_modifier) {
  int dice, base_addiction_rating;

  // M&M p108: Use unaugmented wil or body for these tests.
  if (is_mental) {
    dice = GET_REAL_WIL(ch);
    base_addiction_rating = drug_types[drug_id].mental_addiction_rating;
    if (guided_withdrawal_modifier)
      base_addiction_rating += 1;
  } else {
    dice = GET_REAL_BOD(ch) + (GET_RACE(ch) == RACE_DWARF ? 2 : 0);
    base_addiction_rating = drug_types[drug_id].physical_addiction_rating;
    if (guided_withdrawal_modifier)
      base_addiction_rating += 3;
  }

  int num_successes = success_test(dice, base_addiction_rating + GET_DRUG_ADDICTION_EDGE(ch, drug_id));
  bool addiction_passed = (num_successes > 0);

  snprintf(buf, sizeof(buf), "%s's %s %s addiction %s test: %s(%d) vs addiction factor(%d) + edge(%d) = %d hits (%s).",
           GET_CHAR_NAME(ch),
           drug_types[drug_id].name,
           is_mental ? "mental" : "physical",
           test_identifier,
           is_mental ? "wil" : "bod",
           dice,
           base_addiction_rating,
           GET_DRUG_ADDICTION_EDGE(ch, drug_id),
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
    RM_ALONG_THIRD_STREET,
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
    rnum = real_room(RM_ENTRANCE_TO_DANTES);
  }

  return &world[rnum];
}

void seek_drugs(struct char_data *ch, int drug_id) {
  send_to_char("You enter a fugue state! By the time you recover, you're somewhere else entirely, with a familiar feeling running through your veins...\r\n", ch);

  int sought_dosage = GET_DRUG_TOLERANCE_LEVEL(ch, drug_id) + 2;

  lose_nuyen(ch, INVOLUNTARY_DRUG_PURCHASE_COST_PER_DOSE * sought_dosage, NUYEN_OUTFLOW_GENERIC_SPEC_PROC);
  _apply_doses_of_drug_to_char(sought_dosage, drug_id, ch);

  char_from_room(ch);
  char_to_room(ch, _get_random_drug_seller_room());
  WAIT_STATE(ch, 2 RL_SEC);

  char blank_buf[10];
  strlcpy(blank_buf, "", sizeof(blank_buf));
  do_look(ch, blank_buf, 0, 0);
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
