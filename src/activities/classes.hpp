#ifndef _ACTIVITIES_CLASSES_HPP_
#define _ACTIVITIES_CLASSES_HPP_

#include <exception>
#include <string>
#include <map>
#include <vector>

#include <time.h>

#include "../structs.hpp"

// A boolean test that has a pass/fail state.
class Check {
  void *func_ptr = nullptr;
public:
  std::string type = "not set"; // The string that identifies the function to run, like "has_item"

  std::map<std::string, std::string> settings = {}; // A map of settings/parameters, like {"vnum": "3"}

  Check(const std::string& type, const std::map<std::string, std::string>& settings);
  Check() = default;
  
  // True if this specific test passed for the character, false otherwise. Assertion: Tests cannot kill character.
  bool test(struct char_data *ch);
};

// A side effect that, when encountered, modifies the party in some way.
class Effect {
  void *func_ptr = nullptr;
public:
  std::string type = "not set"; // The string that identifies the function to run, like "phys_damage"

  std::map<std::string, std::string> settings = {}; // A map of settings/parameters, like {"amount": "50"}

  Effect(const std::string& type, const std::map<std::string, std::string>& settings);
  Effect() = default;

  // Applying an effect can result in death. Return true when this happens.
  bool apply(struct char_data *ch);
};

// An edge in the digraph that routes from a situation to either another situation or the end of the activity.
class Outcome {
public:
  std::string message; // A fragment that would fit after both 'Lucien ' and 'Assisted by X and Y, Lucien '.
  // e.g. "breaks down the door with his enormous schlong!"

  std::vector<Effect> effects;
  // An optional list of effects that apply to all characters in the party when this outcome is selected.

  std::vector<std::string> situations;
  // A list of situations this outcome can branch to, or empty if the end of the activity.

  Outcome(const std::string& message, const std::vector<Effect>& effects, const std::vector<std::string>& situations)
    : message(message), effects(effects), situations(situations) {}
  Outcome() = default;

  // Apply all effects to the selected character. Return true if they die (i.e. they have been extracted.)
  bool apply(struct char_data *ch) {
    for (auto& effect : effects) { if (effect.apply(ch)) { return true; } }
    return false;
  }

  // Fetch a random situation slug from the available set.
  std::string *get_next_situation_slug() {
    return situations.size() > 0 ? &situations[rand() % situations.size()] : nullptr;
  }
};

// A pair of Outcomes that are selected between based on the result of zero or more tests (default success).
class Option {
public:
  std::string slug; // identifies the option (for editing etc)

  std::string menu_text; // the text that shows in the selection menu to describe the option to the player

  std::string abort_message; // the neutral message that displays when someone picked this option but someone else superceded them
  // e.g. "You start prying at the hatch with a crowbar, but don't make it very far."

  std::vector<Check> preconditions; // checks that must pass for the player to be able to select this option

  std::vector<Check> tests; // checks that must pass for the player to receive the Pass outcome, otherwise get the Fail outcome

  Outcome pass; // an outcome describing the 'good' version of this (passed check). Printed string comes from here.

  Outcome fail; // an outcome describing the 'bad' version of this (failed check). Printed string comes from here.

  Option(const std::string& menu_text, const std::vector<Check>& preconditions, const std::vector<Check>& tests, Outcome &pass, Outcome &fail)
    : menu_text(menu_text), preconditions(preconditions), tests(tests), pass(pass), fail(fail) {}
  Option() = default;

  // Applying an outcome can result in death, so for now we return TRUE if that happens, FALSE otherwise. Later we'll just throw an exception.
  bool evaluate(struct char_data *ch);
};

// A node in the activity digraph containing one or more Options as well as metadata about the situation itself.
class Situation {
public:
  std::string slug; // identifies the situation

  std::string text; // displayed when the players enter the situation

  std::vector<Check> preconditions; // checks that must pass for the players to be able to enter the situation

  std::vector<Option> options; // the list of options they can choose from, provided they pass the option's preconditions
  // only one option is rendered as the result, everyone else's options are discarded

  std::vector<Effect> effects; // the list of effects that apply immediately to the PCs on situation start

  Situation(const std::string& slug, const std::string& text, const std::vector<Check>& preconditions, const std::vector<Option>& options, const std::vector<Effect>& effects)
    : slug(slug), text(text), preconditions(preconditions), options(options), effects(effects) {}
  Situation() = default;

  bool meets_preconditions(struct char_data *ch);
};

// A digraph containing one or more Situations as well as metadata about the activity itself.
class Activity {
public:
  // Metadata
  std::string slug = "";
  std::string displayName = "";
  idnum_t author = 0;
  time_t createdAt = time(0);
  time_t updatedAt = time(0);

  // Preconditions for the activity to be available
  // (e.g., character level, faction standing, items possessed)
  // This could be a more complex object or a list of conditions
  std::vector<Check> preconditions = {};

  // All possible situations in this activity, keyed by situation slug
  std::map<std::string, Situation> situations = {};

  // List of potential starting situation slugs
  std::vector<std::string> startingSituations = {};

  // Default constructor
  Activity() = default;

  // Copy constructor
  Activity(const Activity& other);

  // Load from JSON
  Activity(nlohmann::json json);

  ~Activity();

  // Method to get a situation by its slug
  Situation* getSituation(const std::string& situationSlug) {
    auto it = situations.find(situationSlug);
    if (it != situations.end()) {
      return &(it->second);
    }
    return nullptr; // Situation not found
  }
};

/* Finally, we have a RunningActivity, which tracks a player or group of players' progress through an activity.
   Note that everything is slugs and idnums instead of pointers, because honestly, fuck dealing with pointer logic
   while also allowing free edit/delete of scenarios. The buildport would crash 50x a day like that until you wrote
   code to run through each RunningActivity to clear out pointers etc.
*/
class RunningActivity {
public:
  // A map of all the PCs in the event and what their most recent selections were.
  std::unordered_map<idnum_t, std::string> participants = {};

  // Which activity is running?
  std::string running_vector_slug = "";

  // What Situation are you on?
  std::string current_situation_slug = "";

  // todo: any additional state tracking etc, like long-term effects, progress along a journey, etc
};


extern std::map<std::string, Activity> activities;

#endif // _ACTIVITIES_CLASSES_HPP_