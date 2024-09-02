/* General design: All characters have a vision array that takes NONE, IS_NATURAL, IS_ARTIFICIAL.
   Vision-using code calls functions from here to see what level of vision they have.
   We also rip out the vision mods from the current penalty function and put them here for centralization. */

// Eventual todo: MitS pg 82, "the light level on the physical plane has no effect on astral visibility",
//  but also "semi-opaque things like water, fog, smoke, or fire can limit visibility and add modifiers"
//  (see astral perception modifiers table on that page).

#include <map>

#include "structs.hpp"
#include "comm.hpp"
#include "constants.hpp"

bool _vision_prereqs_are_valid(struct char_data *ch, int type, const char *function_name);
bool _vision_bit_is_valid(int bit, const char *function_name);

// Only returns false for characters who are lacking all standard vision types. This would primarily affect ghouls.
bool has_any_vision(struct char_data *ch) {
  // Returns TRUE if you have any one vision type.
  for (int idx = 0; idx < NUM_VISION_TYPES; idx++) {
    if (ch->points.vision[idx].HasAnythingSetAtAll()) {
      return TRUE;
    }
  }

  // RIP, no vision bits set at all.
  return FALSE;
}

// Do we have a given type of vision at all?
bool has_vision(struct char_data *ch, int type, bool staff_override) {
  if (!_vision_prereqs_are_valid(ch, type, __func__))
    return FALSE;

  if (staff_override)
    return TRUE;

  // Handle riggers.
  struct veh_data *veh = ch->char_specials.rigging;
  if (veh && (AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE))) {
    switch (type) {
      case VISION_NORMAL:
      case VISION_LOWLIGHT:
      case VISION_THERMOGRAPHIC:
        // Vehicles have these sensors by default.
        return TRUE;
      case VISION_ULTRASONIC:
        // Vehicles only have this if they have an ultrasonic module installed.
        return veh->flags.IsSet(VFLAG_ULTRASOUND);
      default:
        mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Received unrecognized vision type to has_vision(%s, %d, %s).", GET_CHAR_NAME(ch), type, staff_override ? "T" : "F");
        return FALSE;
    }
  }

  return ch->points.vision[type].HasAnythingSetAtAll();
}

// Do we have a given type of vision in a natural form? (Used for vision penalties)
bool has_natural_vision(struct char_data *ch, int type) {
  if (!_vision_prereqs_are_valid(ch, type, __func__))
    return FALSE;

  // Spell vision specifically functions as if cybernetic, so it doesn't qualify as natural.
  //  MitS p141, 'Nightvision': '[...] like that provided by the low-light cybernetic enhancement'
  //  SR3 p111, 'Visibility Impaired': '[...] the first modifier applies to cybernetic or electronic vision and the second to natural vision.'
  // The Adept improved senses power is natural according to https://www.shadowrunrpg.com/resources/sr3faq.html.
  return ch->points.vision[type].AreAnySet(VISION_BIT_IS_NATURAL, VISION_BIT_IS_ADEPT_POWER, ENDBIT);
}

// Do we have a given type of vision in a form we can cast with?
bool has_castable_vision(struct char_data *ch, int type) {
  if (!_vision_prereqs_are_valid(ch, type, __func__))
    return FALSE;

#ifndef DIES_IRAE
  // Short circuit: If we're not using castable-vision restrictions, bail out.
  return TRUE;
#endif

  // You can never cast through ultrasound (M&M p18).
  if (type == VISION_ULTRASONIC)
    return FALSE;

  // You can always cast through natural vision.
  if (has_natural_vision(ch, type))
    return TRUE;

  // Implanted cybernetics allow spellcasting per SR3 p181.
  if (ch->points.vision[type].IsSet(VISION_BIT_FROM_IMPLANTS))
    return TRUE;

  // All other vision types block spellcasting. This specifically includes:
  // - Electronic means of vision (SR3 p181)
  // - Spell vision (same page)
  // - Adept powers (adepts can't cast)
  // - Equipment-based vision that you don't already have (LL/IR sensors are still electronic imaging)
  return FALSE;
}

void remove_racial_vision_due_to_eye_replacement(struct char_data *ch) {
  // Make sure you set VISION_BIT_FROM_IMPLANTS on normal vision after you call this.
  remove_vision_bit(ch, VISION_NORMAL, VISION_BIT_IS_NATURAL);

  switch (GET_RACE(ch)) {
    case RACE_DWARF:
    case RACE_GNOME:
    case RACE_MENEHUNE:
    case RACE_KOBOROKURU:
    case RACE_TROLL:
    case RACE_CYCLOPS:
    case RACE_FOMORI:
    case RACE_GIANT:
    case RACE_MINOTAUR:
    case RACE_DRAKE_DWARF:
    case RACE_DRAKE_TROLL:
    case RACE_WESTERN_DRAGON:
    case RACE_EASTERN_DRAGON:
    case RACE_FEATHERED_SERPENT:
      remove_vision_bit(ch, VISION_THERMOGRAPHIC, VISION_BIT_IS_NATURAL);
      break;
    case RACE_ORK:
    case RACE_HOBGOBLIN:
    case RACE_SATYR:
    case RACE_ONI:
    case RACE_ELF:
    case RACE_WAKYAMBI:
    case RACE_NIGHTONE:
    case RACE_DRYAD:
    case RACE_DRAKE_ELF:
    case RACE_DRAKE_ORK:
      // We refuse to remove LL vision from someone who has the Cat's Eyes bioware.
      for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content) {
        if (GET_BIOWARE_TYPE(bio) == BIO_CATSEYES)
          return;
      }

      // No bioware to protect you? Goodbye, natural vision.
      remove_vision_bit(ch, VISION_LOWLIGHT, VISION_BIT_IS_NATURAL);
      break;
  }
}


/* Setters. */
void set_vision_bit(struct char_data *ch, int type, int bit) {
  if (!_vision_prereqs_are_valid(ch, type, __func__))
    return;

  if (!_vision_bit_is_valid(bit, __func__))
    return;

  ch->points.vision[type].SetBit(bit);
}

void remove_vision_bit(struct char_data *ch, int type, int bit) {
  if (!_vision_prereqs_are_valid(ch, type, __func__))
    return;

  if (!_vision_bit_is_valid(bit, __func__))
    return;

  ch->points.vision[type].RemoveBit(bit);
}


/////////////////// Utilities and helpers for other files //////////////////////

// Local defines to make the get_vision_penalty code easier to handle.
#define TH_PENALTY(name_str, pen) { thermo_penalties.emplace(std::string(name_str), pen); tn_with_thermo += pen; }
#define LL_PENALTY(name_str, pen) { lowlight_penalties.emplace(std::string(name_str), pen); tn_with_ll += pen; }
#define NM_PENALTY(name_str, pen) { normal_penalties.emplace(std::string(name_str), pen); tn_with_none += pen; }
int get_vision_penalty(struct char_data *ch, struct room_data *temp_room, char *rbuf, size_t rbuf_size) {
  if (!ch || !temp_room || !rbuf) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: get_vision_penalty(%s, %s, %s, %ld) called with invalid parameters! Returning insane vision penalty.",
                    ch ? GET_CHAR_NAME(ch) : "NULL",
                    temp_room ? GET_ROOM_NAME(temp_room) : "NULL",
                    rbuf ? "rbuf" : "NULL", 
                    rbuf_size);
    return 20;
  }

  int tn_with_thermo = 0;
  int tn_with_ll = 0;
  int tn_with_none = 0;

  bool is_rigging = (AFF_FLAGGED(ch, AFF_RIG) || PLR_FLAGGED(ch, PLR_REMOTE));

  bool has_natural_thermographic_vision = !is_rigging && has_natural_vision(ch, VISION_THERMOGRAPHIC);
  bool has_natural_lowlight_vision = !is_rigging && has_natural_vision(ch, VISION_LOWLIGHT);

  // each vision mode has a vector that we tack things onto along with a running total
  // at the end, we pick the one with the fewest penalties, and output the vector to rolls
  std::map<std::string, int> thermo_penalties = {};
  std::map<std::string, int> lowlight_penalties = {};
  std::map<std::string, int> normal_penalties = {};

  switch (light_level(temp_room)) {
    case LIGHT_FULLDARK:
      if (has_natural_thermographic_vision) {
        TH_PENALTY("FullDark[TH-N]", 2);
      } else {
        TH_PENALTY("FullDark[TH-A]", 4);
      }

      LL_PENALTY("FullDark[LL]", 8);

      NM_PENALTY("FullDark[NM]", 8);
      break;
    case LIGHT_MINLIGHT:
      if (has_natural_thermographic_vision) {
        TH_PENALTY("MinLight[TH-N]", 2);
      } else {
        TH_PENALTY("MinLight[TH-A]", 4);
      }

      if (has_natural_lowlight_vision) {
        LL_PENALTY("MinLight[LL-N]", 2);
      } else {
        LL_PENALTY("MinLight[LL-A]", 4);
      }

      NM_PENALTY("MinLight[NM]", 6);
      break;
    case LIGHT_PARTLIGHT:
      if (has_natural_thermographic_vision) {
        TH_PENALTY("PartLight[TH-N]", 1);
      } else {
        TH_PENALTY("PartLight[TH-A]", 2);
      }

      if (!has_natural_lowlight_vision) {
        LL_PENALTY("PartLight[LL-A]", 1);
      }

      NM_PENALTY("PartLight[NM]", 2);
      break;
    case LIGHT_GLARE:
      // If you have a flare compensator, this doesn't affect you.
      if (!has_flare_compensation(ch)) {
        // You don't have a flare compensator. What a shame.
        if (has_natural_thermographic_vision) {
          TH_PENALTY("Glare[TH-N]", 2);
        } else {
          TH_PENALTY("Glare[TH-A]", 4);
        }

        if (has_natural_lowlight_vision) {
          LL_PENALTY("Glare[LL-N]", 2);
        } else {
          LL_PENALTY("Glare[LL-A]", 4);
        }

        NM_PENALTY("Glare[NM]", 2);
        break;
      }
      break;
  }

  if (temp_room->shadow[0]) {
    TH_PENALTY("ShadowSpell[TH]", MIN(8, temp_room->shadow[1]));
    LL_PENALTY("ShadowSpell[LL]", MIN(8, temp_room->shadow[1]));
    NM_PENALTY("ShadowSpell[NM]", MIN(8, temp_room->shadow[1]));
  }

  if (light_level(temp_room) != LIGHT_GLARE && temp_room->light[ROOM_HIGHEST_SPELL_FORCE]) {
    if (tn_with_thermo > 0) {
      int new_thermo = MAX(0, tn_with_thermo - temp_room->light[ROOM_HIGHEST_SPELL_FORCE]);
      if (new_thermo != tn_with_thermo) {
        TH_PENALTY("LightSpell[TH]", new_thermo - tn_with_thermo);
      }
    }

    if (tn_with_ll > 0) {
      int new_ll = MAX(0, tn_with_ll - temp_room->light[ROOM_HIGHEST_SPELL_FORCE]);
      if (new_ll != tn_with_ll) {
        LL_PENALTY("LightSpell[LL]", new_ll - tn_with_ll);
      }
    }

    if (tn_with_none > 0) {
      int new_normal = MAX(0, tn_with_none - temp_room->light[ROOM_HIGHEST_SPELL_FORCE]);
      if (new_normal != tn_with_none) {
        NM_PENALTY("LightSpell[NM]", new_normal - tn_with_none);
      }
    }
  }

  if (temp_room->vision[1] == LIGHT_MIST) {
    if (!has_natural_lowlight_vision) {
      LL_PENALTY("Mist[LL-A]", 2);
    }

    NM_PENALTY("Mist[NM]", 2);
  }

  if (temp_room->vision[1] == LIGHT_LIGHTSMOKE
      || (weather_info.sky == SKY_RAINING && !ROOM_FLAGGED(temp_room, ROOM_INDOORS)))
  {
    if (has_natural_lowlight_vision) {
      LL_PENALTY("LSmoke/LRain[LL-N]", 2);
    } else {
      LL_PENALTY("LSmoke/LRain[LL-A]", 4);
    }

    NM_PENALTY("LSmoke/LRain[NM]", 4);
  }

  if (temp_room->vision[1] == LIGHT_HEAVYSMOKE
      || (weather_info.sky == SKY_LIGHTNING && !ROOM_FLAGGED(temp_room, ROOM_INDOORS)))
  {
    if (!has_natural_thermographic_vision) {
      TH_PENALTY("HSmoke/HRain[TH-A]", 1);
    }

    if (has_natural_lowlight_vision) {
      LL_PENALTY("HSmoke/HRain[LL-N]", 4);
    } else {
      LL_PENALTY("HSmoke/HRain[LL-A]", 6);
    }

    NM_PENALTY("HSmoke/HRain[NM]", 6);
  }

  if (temp_room->vision[1] == LIGHT_THERMALSMOKE) {
    if (has_natural_thermographic_vision) {
      TH_PENALTY("TSmoke[TH-N]", 6);
    } else {
      TH_PENALTY("TSmoke[TH-A]", 8);
    }

    LL_PENALTY("TSmoke[LL]", 4);

    NM_PENALTY("TSmoke[NM]", 4);
  }

  // We've reached the end of all the penalties. We'll hand back the lowest.
  std::map<std::string, int> *penalty_vector;
  int penalty_chosen;

  bool has_thermographic = is_rigging || has_vision(ch, VISION_THERMOGRAPHIC);
  bool has_lowlight = is_rigging || has_vision(ch, VISION_LOWLIGHT);

  // Ultrasound halves normal vision penalties (round up- M&M p18).
  if (has_vision(ch, VISION_ULTRASONIC)) {
    int delta = ((tn_with_none + 1) / 2) - tn_with_none;

    if (delta) {
      NM_PENALTY("Ultrasound (half, round up)", delta);
    }
  }

  if (has_thermographic && (!has_lowlight || (tn_with_thermo <= tn_with_ll)) && tn_with_thermo <= tn_with_none) {
    penalty_vector = &thermo_penalties;
    penalty_chosen = tn_with_thermo;
  }
  else if (has_lowlight && (!has_thermographic || (tn_with_ll <= tn_with_thermo)) && tn_with_ll <= tn_with_none) {
    penalty_vector = &lowlight_penalties;
    penalty_chosen = tn_with_ll;
  }
  else {
    penalty_vector = &normal_penalties;
    penalty_chosen = tn_with_none;
  }

  for (auto it : *penalty_vector) {
    buf_mod(rbuf, rbuf_size, it.first.c_str(), it.second);
  }

  if (penalty_chosen > 8) {
    buf_mod(rbuf, rbuf_size, "[vision penalty max: 8]", 8 - penalty_chosen);
  }

  return MIN(8, penalty_chosen);
}

// Sets a character's natural vision based on its race.
void set_natural_vision_for_race(struct char_data *ch) {
  // Everyone gets natural vision.
  set_vision_bit(ch, VISION_NORMAL, VISION_BIT_IS_NATURAL);

  switch (GET_RACE(ch)) {
    case RACE_HUMAN:
    case RACE_OGRE:
      // Do nothing: Their vision is already normal.
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
    case RACE_DRAKE_DWARF:
    case RACE_DRAKE_TROLL:
      set_vision_bit(ch, VISION_THERMOGRAPHIC, VISION_BIT_IS_NATURAL);
      break;
    case RACE_ORK:
    case RACE_HOBGOBLIN:
    case RACE_SATYR:
    case RACE_ONI:
    case RACE_ELF:
    case RACE_WAKYAMBI:
    case RACE_NIGHTONE:
    case RACE_DRYAD:
    case RACE_DRAKE_ELF:
    case RACE_DRAKE_ORK:
      set_vision_bit(ch, VISION_LOWLIGHT, VISION_BIT_IS_NATURAL);
      break;
    case RACE_WESTERN_DRAGON:
    case RACE_EASTERN_DRAGON:
    case RACE_FEATHERED_SERPENT:
      set_vision_bit(ch, VISION_THERMOGRAPHIC, VISION_BIT_IS_NATURAL);
      set_vision_bit(ch, VISION_LOWLIGHT, VISION_BIT_IS_NATURAL);
      break;
    case RACE_FLESHFORM:
      set_vision_bit(ch, VISION_THERMOGRAPHIC, VISION_BIT_IS_NATURAL);
      set_vision_bit(ch, VISION_LOWLIGHT, VISION_BIT_IS_NATURAL);
      set_vision_bit(ch, VISION_ULTRASONIC, VISION_BIT_IS_NATURAL);
      break;
  }
}

// Clone over all vision bits from the original to the clone.
void copy_vision_from_original_to_clone(struct char_data *original, struct char_data *clone) {
  for (int vision_idx = 0; vision_idx < NUM_VISION_TYPES; vision_idx++) {
    clone->points.vision[vision_idx].SetAll(original->points.vision[vision_idx]);
  }
}

const char *write_vision_string_for_display(struct char_data *ch, int mode) {
  static char vision_string_buf[1000], bit_string[1000];
  bool printed_something = FALSE;

  if (mode == VISION_STRING_MODE_STATUS)
    strlcpy(vision_string_buf, "", sizeof(vision_string_buf));
  else
    strlcpy(vision_string_buf, "Vision: ", sizeof(vision_string_buf));

  bit_string[0] = '\0';

  for (int vision_idx = 0; vision_idx < NUM_VISION_TYPES; vision_idx++) {
    if (has_vision(ch, vision_idx)) {
      snprintf(ENDOF(vision_string_buf), sizeof(vision_string_buf) - strlen(vision_string_buf), "%s%s%s^n (%s%s%s^n)",
               // In status mode, we put in newlines-- otherwise, colorized commas.
               printed_something ? (mode == VISION_STRING_MODE_STATUS ? "\r\n  " : "^n, ^C") : "",
               // Skip the bright blue for status mode.
               mode != VISION_STRING_MODE_STATUS ? "^C" : "",
               // Print the vision type itself (ex: thermographic).
               vision_types[vision_idx],
               // Mark it as natural or artificial.
               has_natural_vision(ch, vision_idx) ? "natural" : "artificial",
               // Skip the color for status mode again.
               mode != VISION_STRING_MODE_STATUS ? "^c" : "",
#ifdef DIES_IRAE
               // If we're in DI mode, add the castable flag if it's there.
               has_castable_vision(ch, vision_idx) ? ", castable" : ""
#else
               ""
#endif
      );

      // We don't print the bits for score mode (what's seen in the status command etc.)
      if (mode != VISION_STRING_MODE_STATUS) {
        ch->points.vision[vision_idx].PrintBits(bit_string, sizeof(bit_string), vision_bits, NUM_VISION_BITS);
        snprintf(ENDOF(vision_string_buf), sizeof(vision_string_buf) - strlen(vision_string_buf), " [%s]", bit_string);
      }

      printed_something = TRUE;
    }
  }

  // Add in magnification.
  int vision_mag = get_vision_mag(ch);


  if (vision_mag) {
    if (mode == VISION_STRING_MODE_STATUS) {
      snprintf(ENDOF(vision_string_buf), sizeof(vision_string_buf) - strlen(vision_string_buf), "\r\n\r\nYou%s can see %d room%s away with your magnification.",
               has_flare_compensation(ch) ? " have flare compensation and" : "",
               vision_mag,
               vision_mag == 1 ? "" : "s"
             );
    } else {
      snprintf(ENDOF(vision_string_buf), sizeof(vision_string_buf) - strlen(vision_string_buf), "^n, ^CMagnification-%d^n%s",
               vision_mag,
               has_flare_compensation(ch) ? ", ^CFlare Comp^n" : ""
             );
    }
  } else if (has_flare_compensation(ch)) {
    if (mode == VISION_STRING_MODE_STATUS) {
      strlcat(vision_string_buf, "\r\n\r\nYou have flare compensation.", sizeof(vision_string_buf));
    } else {
      strlcat(vision_string_buf, "^n, ^CFlare Comp^n", sizeof(vision_string_buf));
    }
  }



  strlcat(vision_string_buf, "^n\r\n", sizeof(vision_string_buf));

  return vision_string_buf;
}

void clear_ch_vision_bits(struct char_data *ch) {
  for (int vision_idx = 0; vision_idx < NUM_VISION_TYPES; vision_idx++) {
    ch->points.vision[vision_idx].Clear();
  }
  set_natural_vision_for_race(ch);
}

void apply_vision_bits_from_implant(struct char_data *ch, struct obj_data *implant) {
  char oopsbuf[500];

  if (!implant || !ch) {
    mudlog("SYSERR: Received NULL implant or ch to apply_vision_bits_from_implant()!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  switch (GET_OBJ_TYPE(implant)) {
    // Handle cyberware.
    case ITEM_CYBERWARE:
      if (GET_CYBERWARE_TYPE(implant) != CYB_EYES) {
        snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Received non-CYB_EYES cyberware type %d to %s!", GET_CYBERWARE_TYPE(implant), __func__);
        mudlog(oopsbuf, ch, LOG_SYSLOG, TRUE);
        return;
      }

      // Replacements strip standard vision.
      if (IS_SET(GET_CYBERWARE_FLAGS(implant), EYE_CYBEREYES)) {
        // Eye replacements strip racial benefits, so we call this function before applying the implant.
        // See M&M p64 (Cat's Eyes) for an example of this rule.
        remove_racial_vision_due_to_eye_replacement(ch);
      }

      if (IS_SET(GET_CYBERWARE_FLAGS(implant), EYE_THERMO)) {
        set_vision_bit(ch, VISION_THERMOGRAPHIC, VISION_BIT_FROM_IMPLANTS);
      }

      if (IS_SET(GET_CYBERWARE_FLAGS(implant), EYE_LOWLIGHT)) {
        set_vision_bit(ch, VISION_LOWLIGHT, VISION_BIT_FROM_IMPLANTS);
      }

      if (IS_SET(GET_CYBERWARE_FLAGS(implant), EYE_ULTRASOUND)) {
        set_vision_bit(ch, VISION_ULTRASONIC, VISION_BIT_FROM_IMPLANTS);
      }

      // All implants grant normal vision as a matter of course-- but only if they're not zero-essence cosmetics.
      if (GET_CYBERWARE_ESSENCE_COST(implant) > 0) {
        set_vision_bit(ch, VISION_NORMAL, VISION_BIT_FROM_IMPLANTS);
      }

      break;

    // Handle bioware.
    case ITEM_BIOWARE:
      if (GET_BIOWARE_TYPE(implant) != BIO_CATSEYES) {
        snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Received non-BIO_CATSEYES bioware type %d to %s!", GET_BIOWARE_TYPE(implant), __func__);
        mudlog(oopsbuf, ch, LOG_SYSLOG, TRUE);
        return;
      }

      // Eye replacements strip racial benefits, so we call this function before applying the implant.
      // See M&M p64 (Cat's Eyes) for an example of this rule.
      remove_racial_vision_due_to_eye_replacement(ch);
      remove_vision_bit(ch, VISION_NORMAL, VISION_BIT_FROM_IMPLANTS);

      set_vision_bit(ch, VISION_NORMAL, VISION_BIT_IS_NATURAL);
      set_vision_bit(ch, VISION_LOWLIGHT, VISION_BIT_IS_NATURAL);
      break;

    // Handle bugs.
    default:
      snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Received unhandled item type %d to %s!", GET_OBJ_TYPE(implant), __func__);
      mudlog(oopsbuf, ch, LOG_SYSLOG, TRUE);
      return;
  }
}

bool has_flare_compensation(struct char_data *ch) {
  // Adept flare comp.
  if (GET_POWER(ch, ADEPT_FLARE)) {
    return TRUE;
  }
    
  // Find cybernetic flare comp.
  for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content) {
    if (GET_CYBERWARE_TYPE(cyber) == CYB_EYES && IS_SET(GET_CYBERWARE_FLAGS(cyber), EYE_FLARECOMP)) {
      return TRUE;
    }
  }

  // Find bioware flare comp.
  for (struct obj_data *bio = ch->bioware; bio; bio = bio->next_content) {
    if (GET_BIOWARE_TYPE(bio) == BIO_NICTATINGGLAND) {
      return TRUE;
    }
  }

  return FALSE;
}

int get_vision_mag(struct char_data *ch) {
  int vision_mag = 0;

  if (AFF_FLAGGED(ch, AFF_VISION_MAG_3))
    vision_mag = 3;
  else if (AFF_FLAGGED(ch, AFF_VISION_MAG_2))
    vision_mag = 2;
  else if (AFF_FLAGGED(ch, AFF_VISION_MAG_1))
    vision_mag = 1;

  return vision_mag;
}

int get_character_light_sources(struct char_data *ch) {
  int sources = 0;

  if (!ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Got NULL ch to get_character_light_sources()!");
    return 0;
  }

  // log_vfprintf("Entered get_character_light_sources(%s) (%sPC)", GET_CHAR_NAME(ch), IS_NPC(ch) ? "N" : "");

  if (IS_SENATOR(ch) && PRF_FLAGGED(ch, PRF_HOLYLIGHT)) {
    sources = 10;
  }

  for (int pos = 0; pos < NUM_WEARS; pos++) {
    struct obj_data *eq = GET_EQ(ch, pos);

    if (!eq) {
      continue;
    }

    if (GET_OBJ_TYPE(eq) == ITEM_LIGHT) {
      sources++;
      continue;
    }

#ifdef GLOWING_ITEMS_ARE_LIGHT_SOURCES
    if (IS_OBJ_STAT(eq, ITEM_EXTRA_GLOW)) {
      sources++;
      continue;
    }
#endif
  }

  // log_vfprintf("Got %d sources from get_character_light_sources(%s)", sources, GET_CHAR_NAME(ch));
  return sources;
}

void recalculate_room_light(struct room_data *room) {
  if (!room) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Got NULL room to recalculate_room_light()!");
    return;
  }

  room->light[ROOM_LIGHT_HEADLIGHTS_AND_FLASHLIGHTS] = 0;

  // Characters with lights add to the light level.
  for (struct char_data *ch = room->people; ch; ch = ch->next_in_room) {
    room->light[ROOM_LIGHT_HEADLIGHTS_AND_FLASHLIGHTS] += get_character_light_sources(ch);

    // If they're staff, max out the light level and stop processing.
    // This isn't enough to make a room more than partially lit, but it's partial AF.
    if (IS_SENATOR(ch) && PRF_FLAGGED(ch, PRF_HOLYLIGHT)) {
      room->light[ROOM_LIGHT_HEADLIGHTS_AND_FLASHLIGHTS] = 16;
      return;
    }
  }

  // Vehicles always provide light.
  for (struct veh_data *veh = room->vehicles; veh; veh = veh->next_veh) {
    room->light[ROOM_LIGHT_HEADLIGHTS_AND_FLASHLIGHTS]++;
  }
}


////////////////////////// Internal helper methods. ////////////////////////////
bool _vision_prereqs_are_valid(struct char_data *ch, int type, const char *function_name) {
  char oopsbuf[500];

  if (!ch) {
    snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Received NULL character to %s()!", function_name);
    mudlog(oopsbuf, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (type < 0 || type >= NUM_VISION_TYPES) {
    snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Received invalid vision type %d to %s()!", type, function_name);
    mudlog(oopsbuf, ch, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  return TRUE;
}

bool _vision_bit_is_valid(int bit, const char *function_name) {
  if (bit < 0 || bit >= NUM_VISION_BITS) {
    char oopsbuf[500];
    snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Received invalid vision bit %d to %s()!", bit, function_name);
    mudlog(oopsbuf, NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  return TRUE;
}
