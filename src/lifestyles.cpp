#include <fstream>
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
namespace bf = boost::filesystem;

#include "nlohmann/json.hpp"
using nlohmann::json;

#include "structs.hpp"
#include "utils.hpp"
#include "lifestyles.hpp"
#include "house.hpp"
#include "db.hpp"
#include "interpreter.hpp"
#include "comm.hpp"
#include "newhouse.hpp"
#include "newdb.hpp"
#include "olc.hpp"

extern void cedit_disp_menu(struct descriptor_data *d, int mode);

#define DEFAULT_NEOPHYTE_GUILD_LIFESTYLE_STRING "The metallic scent of the Neophyte Guild clings to $m."
#define DEFAULT_COFFIN_MOTEL_GENDERED_LIFESTYLE_STRING "They've got a smell about them-- eau de Coffin Motel?"
#define DEFAULT_COFFIN_MOTEL_AGENDER_LIFESTYLE_STRING "$e's got a smell about him-- eau de Coffin Motel?"

/* System design:

- √ Lifestyles exist: Streets, Squatter, Low, Middle, High, Luxury. See SR3 p62.
- √ Lifestyle strings exist: These describe the 'air' of a character living that lifestyle.
- √ Lifestyle rent bands exist: These state the minimum for a rent in that lifestyle. Max is implicitly determined by next lifestyle's min - 1.
- √ Garage strings exist: Similar to lifestyle strings, but describe a character living in a garage regardless of lifestyle.

- √ Lifestyle information is saved in a JSON file and loaded into memory on boot. This allows for editing without recompiling the codebase, and also makes them mutable.

- √ Apartment complexes have associated lifestyles, which may be overridden by individual apartments (only higher, never lower- no Squatter rooms in a Luxury building)
- √ Individual apartments are Garages if the number of garage rooms in the apartment exceeds the number of non-garage rooms.
  - 1 garage, 0 non: garage.
  - 1 garage, 1 non: not garage.

- √ PCs always display a lifestyle string when looked at.
- √ Via CUSTOMIZE PHYSICAL, PCs can select a lifestyle string from the set composed of:
  - All default lifestyle strings from all lifestyles at or below their current maximum ('maximum' being the highest-class apartment they are currently renting)
  - IF THEY OWN AT LEAST ONE GARAGE: All default garage strings
  - IF THEY OWN AT LEAST ONE GARAGE: All complex/apartment garage strings for all garages they currently rent at
- √ Selected lifestyle string is saved to the DB in full.

STRETCH:
- Apartment rents are constrained to their lifestyle bands.
- Lifestyle strings can be edited in-game by staff, tridlog style.
- PCs can submit lifestyle strings for approval / inclusion in the lists.
- Builders can set custom lifestyles on individual complexes / apartments.
- To avoid inconsistencies with editing / changes to the lifestyle string list, when a character loads, compare their lifestyle string to their available ones: If it's not in the list, give them a random default from their highest lifestyle and notify them.
- Complexes / apartments can have custom lifestyle strings available to them, separated between garage / non-garage
*/

// TODO: Confirm lifestyle is recalculated on hcontrol destroy.

/////// LOGISTICAL TODOS
// TODO: Test the conversion from old-style to new-style apartments REPEATEDLY. Check for loss of items, vehicles, ownership, guests, etc.

/////// CONTENT TODOS
// TODO: Fill out garage strings for Low, High, Luxury lifestyles in json file.
// TODO: Poll playerbase to collect more lifestyle strings.

/////// STRETCH TODOS
// TODO: Add audit command to check for complexes / apartments with rents out of lifestyle bounds
// TODO: If they haven't selected a lifestyle string: Iterate over all apartments, finding the best one that belongs to them, and return a string from that. "Best" is first by highest lifestyle, then by not-garage as a tiebreaker.

extern void _json_parse_from_file(bf::path path, json &target);

const bf::path global_lifestyles_file = bf::system_complete("lib") / "etc" / "lifestyles.json";

// Set name and default rent values that are overridden on load.
struct lifestyle_data lifestyles[] = {
  {"Streets", {}, {}, {}, {}, 0},
  {"Squatter", {}, {}, {}, {}, 500},
  {"Low", {}, {}, {}, {}, 5000},
  {"Middle", {}, {}, {}, {}, 30000},
  {"High", {}, {}, {}, {}, 120000},
  {"Luxury", {}, {}, {}, {}, 750000}
};

// Returns your best lifestyle. Changes to negative if it's a garage lifestyle.
int calculate_best_lifestyle(struct char_data *ch) {
  int best_lifestyle_found = LIFESTYLE_SQUATTER;
  bool best_lifestyle_is_garage = FALSE;
  Apartment *best_apartment = NULL;

  // Check for NPC status. If they don't have one, they don't get this.
  if (IS_NPC(ch) && !ch->desc) {
    GET_BEST_LIFESTYLE(ch) = LIFESTYLE_SQUATTER;
    return LIFESTYLE_SQUATTER;
  }

  // Snap back to their original so we're not finding the apartment of a projection or whatnot.
  if (ch->desc && ch->desc->original) {
    ch = ch->desc->original;
  }

  for (auto &complex : global_apartment_complexes) {
    for (auto &apartment : complex->get_apartments()) {
      if (apartment->has_owner_privs(ch)) {
        int found_lifestyle = apartment->get_lifestyle();

        // An apartment is a lifestyle garage if more than half of the rooms are garages.
        bool found_lifestyle_is_garage = apartment->is_garage_lifestyle();

        if (found_lifestyle > best_lifestyle_found || (found_lifestyle == best_lifestyle_found && !best_lifestyle_is_garage)) {
          best_lifestyle_found = found_lifestyle;
          best_lifestyle_is_garage = found_lifestyle_is_garage;
          best_apartment = apartment;
        }
      }
    }
  }

  if (best_apartment) {
    /*
    log_vfprintf("Using %s (%s / %s) as best apartment for %s.",
                 best_apartment->get_full_name(),
                 lifestyles[best_apartment->get_lifestyle()].name,
                 best_apartment->is_garage_lifestyle() ? "garage" : "standard",
                 GET_CHAR_NAME(ch));
    */

    if (best_lifestyle_is_garage)
      best_lifestyle_found *= -1;

    GET_BEST_LIFESTYLE(ch) = best_lifestyle_found;
  } else {
    //log_vfprintf("No apartment found for %s. Using Squatter lifestyle.", GET_CHAR_NAME(ch));
    GET_BEST_LIFESTYLE(ch) = LIFESTYLE_SQUATTER;
  }

  // Ensure their lifestyle string matches the expected value.
  set_lifestyle_string(ch, get_lifestyle_string(ch));

  return GET_BEST_LIFESTYLE(ch);
}

#define APPEND_VECTOR_TO_RESULTS(vecname) { results.insert(results.end(), vecname.begin(), vecname.end() ); }
std::vector<const char *> *_get_lifestyle_vector(struct char_data *ch) {
  static std::vector<const char *> results = {};
  results.clear();

  int best_lifestyle = GET_BEST_LIFESTYLE(ch);
  bool is_garage = FALSE;

  if (best_lifestyle < 0) {
    best_lifestyle *= -1;
    is_garage = TRUE;
  }

  mudlog_vfprintf(ch, LOG_SYSLOG, "Best lifestyle for %s is %s, which %s a garage.", GET_CHAR_NAME(ch), lifestyles[best_lifestyle].name, is_garage ? "is" : "is NOT");

  for (int lifestyle_idx = best_lifestyle; lifestyle_idx >= LIFESTYLE_STREETS; lifestyle_idx--) {
    if (GET_PRONOUNS(ch) == PRONOUNS_NEUTRAL) {
      if (is_garage) {
        APPEND_VECTOR_TO_RESULTS(lifestyles[lifestyle_idx].garage_strings_neutral);
      } else {
        APPEND_VECTOR_TO_RESULTS(lifestyles[lifestyle_idx].default_strings_neutral);
      }
    } else {
      if (is_garage) {
        APPEND_VECTOR_TO_RESULTS(lifestyles[lifestyle_idx].garage_strings_gendered);
      } else {
        APPEND_VECTOR_TO_RESULTS(lifestyles[lifestyle_idx].default_strings_gendered);
      }
    }
  }

  /*
  if (!IS_NPC(ch)) {
    log_vfprintf("LV for %s is:", GET_CHAR_NAME(ch));
    for (auto &it: results) {
      log(it);
    }
  }
  */

  return &results;
}
#undef APPEND_VECTOR_TO_RESULTS

void cedit_lifestyle_menu(struct descriptor_data *d) {
  struct char_data *ch = d->original ? d->original : d->character;
  int idx = 0;

  for (auto it : *_get_lifestyle_vector(ch)) {
    send_to_char(CH, "%2d) %s\r\n", idx++, it);
  }
  send_to_char("\r\nSelect the lifestyle string you'd like to display: ", CH);

  d->edit_mode = CEDIT_LIFESTYLE;
}

void cedit_lifestyle_parse(struct descriptor_data *d, char *arg) {
  struct char_data *ch = d->original ? d->original : d->character;

  if (!ch) {
    SEND_TO_Q("Something went wrong- you have no character!", d);
    mudlog("SYSERR: Descriptor had NO character in cedit_lifestyle_parse()!", NULL, LOG_SYSLOG, TRUE);
    cedit_disp_menu(d, FALSE);
    return;
  }

  std::vector<const char *> *lifestyle_vector = _get_lifestyle_vector(ch);

  size_t parsed = atol(arg);

  // Failure-- invalid input.
  if (lifestyle_vector->size() <= parsed) {
    send_to_char("Invalid choice.\r\n", ch);
    cedit_lifestyle_menu(d);
    return;
  }

  // Success-- set the string.
  set_lifestyle_string(d->edit_mob, lifestyle_vector->at(parsed));
  send_to_char("OK.\r\n", ch);
  cedit_disp_menu(d, FALSE);
}

///// Setting / Getting /////

void set_lifestyle_string(struct char_data *ch, const char *str) {
  if (!ch) {
    mudlog("SYSERR: Received NULL ch to set_lifestyle_string!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  if (!ch->player_specials) {
    mudlog("SYSERR: Received to set_lifestyle_string a character that doesn't have player_specials!!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  // Iterate through the strings they're allowed to have. If this new one isn't in it, select the first one on the list.
  // Of course, we only do this if the string being set isn't one of our defaults.
  if (strcmp(DEFAULT_NEOPHYTE_GUILD_LIFESTYLE_STRING, str)
      && strcmp(DEFAULT_COFFIN_MOTEL_GENDERED_LIFESTYLE_STRING, str)
      && strcmp(DEFAULT_COFFIN_MOTEL_AGENDER_LIFESTYLE_STRING, str)) 
  {
    std::vector<const char *> *lifestyle_vector = _get_lifestyle_vector(ch);

    if (lifestyle_vector->empty()) {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Got EMPTY lifestyle vector for %s in set_lifestyle_string(\"%s\")!", GET_CHAR_NAME(ch), str);
      str = (GET_PRONOUNS(ch) == PRONOUNS_NEUTRAL ? DEFAULT_COFFIN_MOTEL_AGENDER_LIFESTYLE_STRING : DEFAULT_COFFIN_MOTEL_GENDERED_LIFESTYLE_STRING);
    } else {
      bool found_lifestyle = FALSE;
      for (auto it : *lifestyle_vector) {
        if (!strcmp(str, it)) {
          // log_vfprintf("LV for %s: Comparison string %s matches LV string %s.", GET_CHAR_NAME(ch), str, it);
          found_lifestyle = TRUE;
          break;
        } else {
          // log_vfprintf("LV for %s: Comparison string %s does not match LV string %s.", GET_CHAR_NAME(ch), str, it);
        }
      }
      if (!found_lifestyle) {
        mudlog_vfprintf(ch, LOG_SYSLOG, "%s's lifestyle string isn't listed in %s currently-available set, so changing to the first in that set.", 
                        GET_CHAR_NAME(ch),
                        HSHR(ch));
        str = lifestyle_vector->at(0);
      }
    }
  }

  // Because there's a decent chance that str points to their saved lifestyle string, we dup it here first.
  const char *duped = str_dup(str);

  if (ch->player_specials->saved.lifestyle_string)
    delete [] ch->player_specials->saved.lifestyle_string;
  
  ch->player_specials->saved.lifestyle_string = duped;
}

const char *get_lifestyle_string(struct char_data *ch) {
  if (!ch) {
    mudlog("SYSERR: Received NULL ch to get_lifestyle_string!", ch, LOG_SYSLOG, TRUE);
    return "<error gls-1>";
  }

  if (!ch->player_specials) {
    mudlog("SYSERR: Received to get_lifestyle_string a character that doesn't have player_specials!!", ch, LOG_SYSLOG, TRUE);
    return "<error gls-2>";
  }

  // If they don't have a string set at all, attempt to set it from the best apartment they have.
  if (!ch->player_specials->saved.lifestyle_string) {
    // If they haven't picked a string, and they're new, give them the newbie / default one.
    if (GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD) {
      return DEFAULT_NEOPHYTE_GUILD_LIFESTYLE_STRING;
    }

    // Assume they're renting at a coffin motel.
    if (GET_PRONOUNS(ch) == PRONOUNS_NEUTRAL) {
      return DEFAULT_COFFIN_MOTEL_GENDERED_LIFESTYLE_STRING;
    } else {
      return DEFAULT_COFFIN_MOTEL_AGENDER_LIFESTYLE_STRING;
    }
  }

  // Return their lifestyle string.
  return ch->player_specials->saved.lifestyle_string;
}

///// Saving / Loading - Lifestyles /////

// Read lifestyles from JSON and store them globally.
#define JSON_TO_STRING_VECTOR(field_name) { \
  lifestyles[lifestyle_idx].field_name.clear(); \
  for (auto it : specific_lifestyle_info[#field_name].get<std::vector<std::string>>()) { \
    lifestyles[lifestyle_idx].field_name.push_back(str_dup(it.c_str())); \
  } \
}

void load_lifestyles() {
  json lifestyle_info;
  _json_parse_from_file(global_lifestyles_file, lifestyle_info);

  // Iterate through our known lifestyles, loading the values and strings for each.
  for (int lifestyle_idx = LIFESTYLE_STREETS; lifestyle_idx < NUM_LIFESTYLES; lifestyle_idx++) {
    json specific_lifestyle_info = lifestyle_info[lifestyles[lifestyle_idx].name];

    lifestyles[lifestyle_idx].monthly_cost_min = specific_lifestyle_info["monthly_cost_min"].get<long>();

    JSON_TO_STRING_VECTOR(default_strings_neutral);
    JSON_TO_STRING_VECTOR(default_strings_gendered);
    JSON_TO_STRING_VECTOR(garage_strings_neutral);
    JSON_TO_STRING_VECTOR(garage_strings_gendered);
  }

  // Note that the min costs here are houseruled higher than the book values due to MUD inflation.
  // For reference: UBI is currently 1000¥/hr * 24 hr/day * 30 day/mo = 720,000¥/mo
  long min_luxury_cost = IDLE_NUYEN_REWARD_AMOUNT * (MINS_PER_REAL_MONTH / IDLE_NUYEN_MINUTES_BETWEEN_AWARDS) + 30000;
  if (lifestyles[LIFESTYLE_LUXURY].monthly_cost_min < min_luxury_cost) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Luxury lifestyle cost is below idle nuyen amount + 30k: Raising to %ld.", min_luxury_cost);
    lifestyles[LIFESTYLE_LUXURY].monthly_cost_min = min_luxury_cost;
  }
}
#undef JSON_TO_STRING_VECTOR

// Save lifestyles in memory to file.
void save_lifestyles_file() {
  json lifestyle_info = {};

  // Iterate through our known lifestyles, composing the dicts for each.
  for (int lifestyle_idx = LIFESTYLE_STREETS; lifestyle_idx < NUM_LIFESTYLES; lifestyle_idx++) {
    lifestyle_info[lifestyles[lifestyle_idx].name] = {
      {"monthly_cost_min", lifestyles[lifestyle_idx].monthly_cost_min},
      {"default_strings_neutral", lifestyles[lifestyle_idx].default_strings_neutral},
      {"default_strings_gendered", lifestyles[lifestyle_idx].default_strings_gendered},
      {"garage_strings_neutral", lifestyles[lifestyle_idx].garage_strings_neutral},
      {"garage_strings_gendered", lifestyles[lifestyle_idx].garage_strings_gendered}
    };
  }

  bf::ofstream o(global_lifestyles_file);
  o << std::setw(4) << lifestyle_info << std::endl;
  o.close();
}

///// Util /////
