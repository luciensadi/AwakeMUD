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
- Apartment complexes and apartments MAY have additional lifestyle strings available to them. These are separated between Garage and Non-Garage.
- Apartment rents are constrained to their lifestyle bands.

- PCs have a maximum available lifestyle, which is derived from the highest-class apartment they are currently renting.
- PCs always display a lifestyle string when looked at. This string is chosen from the set composed of:
  - All default lifestyle strings from all lifestyles at or below their current maximum
  - All custom lifestyle strings for all complexes and apartments they currently rent at
  - IF THEY OWN AT LEAST ONE GARAGE: All default garage strings
  - IF THEY OWN AT LEAST ONE GARAGE: All complex/apartment garage strings for all garages they currently rent at

- Lifestyle strings can be swapped out with available ones at any time with CUSTOMIZE LIFESTYLE.
  - The selected lifestyle string is saved to the DB in full.
  - To avoid inconsistencies with editing / changes to the lifestyle string list, when a character loads, compare their lifestyle string to their available ones: If it's not in the list, give them a random default from their highest lifestyle and notify them.

STRETCH:
- Lifestyle strings can be edited in-game by staff, tridlog style.
- PCs can submit lifestyle strings for approval / inclusion in the lists.

*/

// TODO: Set newbie lifestyle on creation.
// TODO: Default lifestyle on loading for chars that have none (can put this in the SQL statement to create the field)
// TODO: Make sure lifestyle strings are handled in all places arrive strings are set/deleted
// TODO: Add code to apartment / complex editing to allow specification of lifestyles (complex: any, apt: higher than complex, or "parent")
// TODO: Add code to apartment / complex editing to allow specification of garage override status
// TODO: Add code to apartment / complex editing to allow specification of custom lifestyle string(s)
// TODO: Add code to apartment / complex editing to require complex lifestyle is set before rent
// TODO: Add code to apartment / complex editing to clear rent on lifestyle change
// TODO: Add code to apartment / complex editing to require rent is in band permitted by lifestyle
// TODO: Add code to apartment / complex editing to not allow saving of an apartment with invalid rent
// TODO: Add code to apartments to allow retrieving of all custom strings available at that apartment / complex
// TODO: Add cedit parsing code to let players select from all available lifestyles
// TODO: Fill out garage strings for Low, High, Luxury lifestyles in json file.

extern void _json_parse_from_file(bf::path path, json &target);

const bf::path global_lifestyles_file = bf::system_complete("lib") / "etc" / "lifestyles.json";

struct lifestyle_data lifestyles[NUM_LIFESTYLES];

void cedit_lifestyle_parse(struct descriptor_data *d, char *arg) {
  // todo fill out
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

  ch->player_specials->saved.lifestyle_string = str_dup(str);

  playerDB.SaveChar(ch);
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
    // TODO: Iterate over all apartments, finding the best one that belongs to them. "Best" is first by highest lifestyle, then by not-garage as a tiebreaker.
    Apartment *best_apartment = NULL;

    // Set their lifestyle string to a random one from their best apartment's set.
    if (best_apartment) {
      // TODO: Implement.
    } else {
      // If that failed, and they're new, give them the newbie / default one.
      if (GET_TKE(ch) < NEWBIE_KARMA_THRESHOLD) {
        return "The metallic scent of the Neophyte Guild clings to $m.";
      }

      // They have no house and are not a newbie. Assume they're renting at a coffin motel.
      if (GET_PRONOUNS(ch) == PRONOUNS_NEUTRAL) {
        return "They've got a smell about them-- eau de Coffin Motel?";
      } else {
        return "$e's got a smell about him-- eau de Coffin Motel?";
      }
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

void boot_lifestyles() {
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

///////////////////////////////// old file / logic below ////////////////////////////

#ifdef use_old_code_from_lifestyles_file

const char *lifestyle_garage_strings[NUM_GARAGE_STRINGS] = {

};

int house_to_lifestyle_map[NUM_LIFESTYLES] = {
  /* 0 is house's Low    */ LIFESTYLE_LOW,
  /* 1 is house's Middle */ LIFESTYLE_MIDDLE,
  /* 2 is house's High   */ LIFESTYLE_HIGH,
  /* 3 is house's Luxury */ LIFESTYLE_LUXURY
};

// Return a string that describes their lifestyle.
const char *get_lifestyle_string(struct char_data *ch) {
  static char compiled_string[500] = { '\0' };

  if (!ch || IS_NPC(ch)) {
    mudlog("SYSERR: Received invalid char to get_lifestyle_string().", ch, LOG_SYSLOG, TRUE);
    return compiled_string;
  }

  if (GET_LIFESTYLE_SELECTION(ch) < 0 || GET_LIFESTYLE_SELECTION(ch) >= NUM_LIFESTYLES) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Lifestyle %d is out of range in get_lifestyle_string().", GET_LIFESTYLE_SELECTION(ch));
    return compiled_string;
  }

  if (GET_LIFESTYLE_STRING_SELECTION(ch) < 0 || GET_LIFESTYLE_STRING_SELECTION(ch) >= NUM_STRINGS_PER_LIFESTYLE) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Lifestyle string selection %d is out of range in get_lifestyle_string().", GET_LIFESTYLE_STRING_SELECTION(ch));
    return compiled_string;
  }

  strlcpy(compiled_string, lifestyles[GET_LIFESTYLE_SELECTION(ch)].strings[GET_LIFESTYLE_STRING_SELECTION(ch)][GET_PRONOUNS(ch) == PRONOUNS_NEUTRAL ? 1 : 0], sizeof(compiled_string));

  if (GET_LIFESTYLE_IS_GARAGE_SELECTION(ch)) {
    snprintf(ENDOF(compiled_string),
                   sizeof(compiled_string) - strlen(compiled_string),
                   " %s",
                   lifestyle_garage_strings[GET_LIFESTYLE_GARAGE_STRING_SELECTION(ch)]);
  }

  return compiled_string;
}

// Iterate through all houses and assign the best owned one to the character.
void determine_lifestyle(struct char_data *ch) {
  int best_lifestyle_found = -1;
  bool best_lifestyle_is_garage = FALSE;
  Apartment *best_apartment = NULL;

  if (!ch || IS_NPC(ch)) {
    mudlog("SYSERR: Received invalid char to determine_lifestyle()!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  for (auto &complex : global_apartment_complexes) {
    for (auto &apartment : complex->get_apartments()) {
      if (apartment->has_owner_privs(ch)) {
        int found_lifestyle = apartment->get_lifestyle();

        // An apartment is a lifestyle garage if more than half of the rooms are garages.
        bool found_lifestyle_is_garage = apartment->get_garage_count() > (apartment->get_rooms().size() / 2);

        if (found_lifestyle > best_lifestyle_found || (found_lifestyle == best_lifestyle_found && !best_lifestyle_is_garage)) {
          best_lifestyle_found = found_lifestyle;
          best_lifestyle_is_garage = found_lifestyle_is_garage;
          best_apartment = apartment;
        }
      }
    }
  }

  if (best_lifestyle_found == -1) {
    best_lifestyle_found = LIFESTYLE_SQUATTER;
    best_lifestyle_is_garage = FALSE;
    log_vfprintf("Assigned %s the squatter lifestyle.", GET_CHAR_NAME(ch));
    return;
  }

  GET_SETTABLE_LIFESTYLE(ch) = GET_SETTABLE_ORIGINAL_LIFESTYLE(ch) = best_lifestyle_found;
  GET_SETTABLE_LIFESTYLE_IS_GARAGE(ch) = GET_SETTABLE_ORIGINAL_LIFESTYLE_IS_GARAGE(ch) = best_lifestyle_is_garage;

  log_vfprintf("Assigned %s lifestyle: %d (%s), %s is%s garage.",
               GET_CHAR_NAME(ch),
               GET_LIFESTYLE_SELECTION(ch),
               lifestyles[GET_LIFESTYLE_SELECTION(ch)].name,
               best_apartment ? best_apartment->get_full_name() : "(null)",
               GET_LIFESTYLE_IS_GARAGE_SELECTION(ch) ? "" : " NOT");
}

// TODO: Add this to point_update(?) and fill out.
void lifestyle_tick(struct char_data *ch) {}

// TODO: Replace this with CUSTOMIZE LIFESTYLE.
ACMD(do_lifestyle) {
  FAILURE_CASE(IS_NPC(ch), "Only player characters can configure their lifestyle.\r\n");

  // No argument? Display current.
  if (!*argument) {
    if (GET_ORIGINAL_LIFESTYLE(ch) != GET_LIFESTYLE_SELECTION(ch)) {
      send_to_char(ch, "You currently present as leading a %s-class lifestyle, but your standard is %d-class.\r\n", GET_LIFESTYLE_SELECTION(ch), GET_ORIGINAL_LIFESTYLE(ch));
    } else {
      send_to_char(ch, "You lead a %s-class lifestyle.\r\n", GET_LIFESTYLE_SELECTION(ch));
    }
    send_to_char(ch, "Your lifestyle is represented as '%s'. Change this with ^WCUSTOMIZE LIFESTYLE^n.", get_lifestyle_string(ch));
    return;
  }
}
#endif
