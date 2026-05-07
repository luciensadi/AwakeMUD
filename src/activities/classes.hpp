#ifndef _ACTIVITIES_CLASSES_HPP_
#define _ACTIVITIES_CLASSES_HPP_

#include <exception>
#include <string>
#include <map>
#include <vector>

#include <time.h>

#include "../structs.hpp"

#define BASE_ACTIVITY_PATH bf::system_complete("lib") / "activities"

#define STRING_GETTER(varname)  const char *get_##varname() { return varname.c_str(); }

/*
  Design:
  The code tracks a vector of RunningActivities, which are the owning instances for the groups of players in the travel system.

  When starting an activity:
  - Create a new RunningActivity and stick it in the vector
  - For each character involved:
    - Update character struct's activity pointer to the new running activity
    - Display the text and options available
  - Set a timeout for option wait state; non-selection leads to random selection

  On each activity tick:
  - Iterate over each RunningActivity:
    - If in a forced wait state (we just showed text and want people to RP about it), and the wait state has not expired, continue
    - If in options selection:
      - If not all options selected and wait state not expired, continue
      - If not all selected: randomly select remaining options
      - For each character, process whether or not they succeeded at their option, compiling the passes and fails into separate maps (characters -> outcomes)
      - If passes has anything in it, randomly select a successful outcome, otherwise randomly select a failed outcome:
        - For each character, display the outcome text
        - For each character, evaluate the outcome on them (CHARACTERS MAY DIE HERE)
      - Set a wait state for RP
    - Otherwise:
      - If no further situations available, activity has ended; halt and delete the activity (this means the running code should be a separate func that calls the activity, not an invocation of something on the activity)
      - Select and display the next situation (for each character display text/options, set timeout etc-- same as when starting activity, so this should be a func call)


*/

typedef bool (*ActivityFuncPtr)(struct char_data*, const std::map<std::string, std::string>&);

// Type constraints for ActivityParamSpec values. Used both for documentation and
// runtime/OLC validation. Add new entries as needed; remember to extend the
// validator in ActivityFunction.cpp.
enum class ActivityParamType {
  STRING,         // any non-empty string
  INTEGER,        // base-10 signed int (atoi-clean, no trailing junk)
  BOOLEAN,        // "true"/"false"/"1"/"0"/"yes"/"no" (case-insensitive)
  SKILL_NAME,     // resolves via skill_name_to_idx()
  SPELL_NAME,     // resolves via spell_name_to_idx()
  POWER_NAME,     // resolves via power_name_to_idx()
  OBJ_VNUM,       // resolves via real_object()
  MOB_VNUM,       // resolves via real_mobile()
  ROOM_VNUM,      // resolves via real_room()
  QUEST_VNUM,     // resolves via real_quest()
  NUYEN_AMOUNT,   // non-negative int (cap-able by callers)
  KARMA_AMOUNT,   // non-negative float
};

// Convenience macros for ActivityParamSpec.required and ActivityFuncSpec.is_deterministic
// so registry entries read as data, not magic booleans.
#define PARAM_REQUIRED        true
#define PARAM_OPTIONAL        false
#define DETERMINISTIC         true
#define NON_DETERMINISTIC     false

// Describes one parameter of an ActivityFunction (Check or Effect).
struct ActivityParamSpec {
  std::string name;                                       // key in the settings map, e.g. "tn"
  std::string description;                                // human-facing hint, e.g. "the target number to roll against"
  ActivityParamType type = ActivityParamType::STRING;     // value-type constraint
  bool required = PARAM_REQUIRED;                         // whether the param must be present
  std::string default_value = "";                         // only meaningful when !required
};

// Describes one entry in a Check or Effect registry.
struct ActivityFuncSpec {
  ActivityFuncPtr func_ptr = nullptr;
  std::string description;                                // human-facing summary of what the function does
  std::vector<ActivityParamSpec> params = {};             // ordered: matches OLC display order
  bool is_deterministic = DETERMINISTIC;                  // false => uses RNG / cannot be used in Activity preconditions
};

// Validates a single parameter value against a type. Returns true on success.
// On failure, fills err_out with a builder-friendly message (no trailing newline).
// Exposed at namespace scope so OLC code can validate before constructing a
// Check or Effect.
bool validate_activity_param_value(ActivityParamType type,
                                   const std::string& value,
                                   std::string& err_out);

// Convenience: stringify an ActivityParamType for OLC menus / error messages.
const char *activity_param_type_name(ActivityParamType type);

// Base class shared by Check and Effect.
// Holds the function pointer, func_name, settings, and provides serialization.
// Derived classes supply their own function registry via the protected constructors.
class ActivityFunction {
protected:
  ActivityFuncPtr func_ptr = nullptr;

  // Regular construction: looks up type in registry, validates settings, stores
  // the pointer (or fallback_func_ptr on validation failure -- with SYSERR log).
  ActivityFunction(const std::string& type,
                   const std::map<std::string, std::string>& settings,
                   const std::map<std::string, ActivityFuncSpec>& registry,
                   ActivityFuncPtr fallback_func_ptr);

  // Deserializing construction: parses JSON, looks up func_name in registry.
  // Falls back to fallback_func_ptr (and logs a SYSERR) if the name is not
  // found or if validation of required params fails.
  ActivityFunction(const std::string& serialized,
                   const std::map<std::string, ActivityFuncSpec>& registry,
                   ActivityFuncPtr fallback_func_ptr);

  // Virtual destructor for inheritance, default copy/move operations for everything else
  virtual ~ActivityFunction() = default;
  ActivityFunction(const ActivityFunction&) = default;
  ActivityFunction& operator=(const ActivityFunction&) = default;
  ActivityFunction(ActivityFunction&&) = default;
  ActivityFunction& operator=(ActivityFunction&&) = default;

  // Dispatches to func_ptr. Returns false (with a SYSERR log) if func_ptr is null.
  bool invoke(struct char_data *ch);

public:
  std::string func_name = "not set"; // The string that identifies the function to run.
  std::map<std::string, std::string> settings = {}; // A map of settings/parameters.

  ActivityFunction() = default;

  // Resolves func_ptr by looking up func_name in the supplied registry. Logs a
  // SYSERR and leaves func_ptr null if not found. Used by from_json() paths.
  void resolve_ptr(const std::map<std::string, ActivityFuncSpec>& registry);

  virtual std::string serialize(const int indent = -1, const char indent_char = ' ') = 0;
};

// A boolean test that has a pass/fail state.
// Assertion: Check functions can NEVER result in character death (they must not mutate the character).
class Check : public ActivityFunction {
public:
  Check(const std::string& type, const std::map<std::string, std::string>& settings);
  Check(const std::string serialized);
  Check() = default;
  
  std::string serialize(const int indent = -1, const char indent_char = ' ') override;
  
  // Display the Check for easy reading. Only used in edit/debug contexts.
  const char *stringify() const;

  // True if this specific test passed for the character, false otherwise.
  bool test(struct char_data *ch);

  // Registry introspection. Both are family-locked: the Check.cpp registry is
  // only reachable through these accessors -- you cannot extern it across
  // translation units, so you cannot accidentally pass an Effect spec where a
  // Check spec is expected.
  static const ActivityFuncSpec* lookup_spec(const std::string& slug);
  static std::vector<std::string> list_slugs();
};

// A side effect that, when encountered, modifies the character and/or their RunningActivity in some way. (todo add ptr from char struct to RunningActivity)
class Effect : public ActivityFunction {
public:
  Effect(const std::string& type, const std::map<std::string, std::string>& settings);
  Effect(const std::string serialized);
  Effect() = default;
  
  std::string serialize(const int indent = -1, const char indent_char = ' ') override;

  // Display the Effect for easy reading. Only used in edit/debug contexts.
  const char *stringify() const;

  // Applying an effect can result in death. Returns true when this happens.
  bool apply(struct char_data *ch);

  // Registry introspection -- see Check::lookup_spec for the family-locking rationale.
  static const ActivityFuncSpec* lookup_spec(const std::string& slug);
  static std::vector<std::string> list_slugs();
};

// An edge in the digraph that routes from a situation to either another situation or the end of the activity.
class Outcome {
public:
  std::string message; // A fragment that would fit after both 'Lucien ' and 'Assisted by X and Y, Lucien '.
  // e.g. "breaks down the door with his enormous schlong!"
  bool is_pass_outcome;

  std::vector<Effect> effects = {};
  // An optional list of effects that apply to all characters in the party when this outcome is selected.

  std::vector<std::string> situations = {};  // (these slugs may become invalidated during OLC, so handle that gracefully)
  // A list of situations this outcome can branch to, or empty if the end of the activity.

  Outcome(const std::string message, const std::vector<Effect> effects, const std::vector<std::string> situations, bool is_pass_outcome) :
          message(message), is_pass_outcome(is_pass_outcome), effects(effects), situations(situations) {}
  explicit Outcome(const std::string serialized_json, bool is_pass_outcome);
  Outcome() = default;

  // Getters, which kindly return C strings to prevent having to go .c_str() all the time.
  STRING_GETTER(message)

  // Display the Outcome for easy reading. Only used in edit/debug contexts.
  const char *stringify() const;

  // Apply all effects to the selected character. Return true if they die (i.e. they have been extracted.)
  bool apply(struct char_data *ch) { for (auto& effect : effects) { if (effect.apply(ch)) { return true; } }    return false; }

  std::string serialize(const int indent = -1, const char indent_char = ' ') const;
};

// A pair of Outcomes that are selected between based on the result of zero or more tests (default success).
class Option {
public:
  std::string slug; // identifies the option (for editing etc); not protected because it's not used in a map like other slugs are.
  std::string menu_text; // the text that shows in the selection menu to describe the option to the player

  std::vector<Check> preconditions = {}; // checks that must pass for the player to be able to select this option
  std::vector<Check> tests = {}; // checks that must pass for the player to receive the Pass outcome, otherwise get the Fail outcome

  Outcome pass; // an outcome describing the 'good' version of this (passed check). Printed string comes from here.
  Outcome fail; // an outcome describing the 'bad' version of this (failed check). Printed string comes from here.

  // Testing constructor.
  Option(const std::string slug, const std::string menu_text, std::vector<Check> preconditions, std::vector<Check> tests, Outcome pass, Outcome fail) : 
         slug(slug), menu_text(menu_text), preconditions(preconditions), tests(tests), pass(pass), fail(fail) {
          pass.is_pass_outcome = true;
          fail.is_pass_outcome = false;
         };

  explicit Option(const std::string serialized_json);

  Option() = default;

  // Getters, which kindly return C strings to prevent having to go .c_str() all the time.
  STRING_GETTER(slug)
  STRING_GETTER(menu_text)

  // Display the Option for easy reading. Only used in edit/debug contexts.
  const char *stringify() const;

  // Returns TRUE if you successfully pass all the preconditions to even see this option, FALSE otherwise.
  bool is_available_to_ch(struct char_data *ch) { for (auto check : preconditions) { if (!check.test(ch)) return false; } return true; }

  // Returns the Fail outcome if you fail any tests, the Pass outcome otherwise.
  Outcome *test(struct char_data *ch) { for (auto check : tests) { if (!check.test(ch)) return &fail; } return &pass; }

  std::string serialize(const int indent = -1, const char indent_char = ' ') const;
};

// A node in the activity digraph containing one or more Options as well as metadata about the situation itself.
class Situation {
public:
  // identifies the situation; used as a key in Activities etc, so if you want to change the slug, create a clone with the new slug.
  std::string slug;

  std::string text; // displayed when the players enter the situation

  std::vector<Check> preconditions = {}; // checks that must pass for the players to be able to enter the situation

  std::vector<Option> options = {}; // the list of options they can choose from, provided they pass the option's preconditions
  // only one option is rendered as the result, everyone else's options are discarded

  std::vector<Effect> effects = {}; // the list of effects that apply immediately to the PCs on situation start

  Situation(const std::string& slug, const std::string& text, const std::vector<Check>& preconditions, const std::vector<Option>& options, const std::vector<Effect>& effects)
    : slug(slug), text(text), preconditions(preconditions), options(options), effects(effects) {}
  explicit Situation(const std::string serialized_json);
  Situation() = default;

  // Getters, which kindly return C strings to prevent having to go .c_str() all the time.
  STRING_GETTER(slug)
  STRING_GETTER(text)

  // Display the Situation for easy reading. Only used in edit/debug contexts.
  const char *stringify() const;

  bool meets_preconditions(struct char_data *ch) { for (auto check : preconditions) { if (!check.test(ch)) return false; } return true; };

  bool apply(struct char_data *ch) { for (auto& effect : effects) { if (effect.apply(ch)) { return true; } }    return false; }

  // TODO: This potentially has random-value tests, so the output is non-deterministic. You need to cache it when called for a character.
  std::vector<Option *> get_options_for_ch(struct char_data *ch);

  std::string serialize(const int indent = -1, const char indent_char = ' ') const;
};

// A digraph containing one or more Situations as well as metadata about the activity itself.
class Activity {
  Situation* _get_situation_from_list_of_slugs(const std::vector<std::string> &slugs, struct char_data *ch, bool allow_fallback_to_invalid);
public:
  // Metadata
  std::string slug = "";
  std::string display_name = "";
  std::string summary = "";
  idnum_t author = 0;
  time_t createdAt = time(0);
  time_t updatedAt = time(0);

  // Editing metadata (non-persisted)
  std::string old_slug = "";

  // Preconditions for the activity to be available (e.g., character level, faction standing, items possessed)
  // TODO: Forbid setting non-deterministic preconditions (e.g. random)
  std::vector<Check> preconditions = {};

  // All possible situations in this activity, keyed by situation slug
  std::map<std::string, Situation> situations = {};

  // List of potential starting situation slugs
  std::vector<std::string> starting_situations = {};

  // Prefs/flags set on the activity, like viewership restriction etc.
  std::map<std::string, std::string> flags = {};

  // Default constructor
  Activity() = default;

  // yeah I know, don't @ me about this, it's only ever used for manual testing
  Activity(std::string slug, std::string display_name, idnum_t author, time_t createdAt, time_t updatedAt, std::vector<Check> preconditions, std::map<std::string, Situation> situations,
           std::vector<std::string> starting_situations, std::map<std::string, std::string> flags)
           :  slug(slug), display_name(display_name), author(author), createdAt(createdAt), updatedAt(updatedAt), preconditions(preconditions), situations(situations),
              starting_situations(starting_situations), flags(flags) {}

  // Deserialize from JSON string.
  explicit Activity(std::string serialized_json);

  // Load from a file.
  Activity(bf::path path_to_file);

  // Getters, which kindly return C strings to prevent having to go .c_str() all the time.
  STRING_GETTER(slug)
  STRING_GETTER(display_name)
  STRING_GETTER(summary)

  std::string serialize(const int indent = -1, const char indent_char = ' ') const;

  bool meets_preconditions(struct char_data *ch) { for (auto check : preconditions) { if (!check.test(ch)) return false; } return true; };
  bool can_edit(struct char_data *ch) const { return access_level(ch, LVL_BUILDER) && (GET_IDNUM(ch) == author || access_level(ch, LVL_ADMIN)); }

  // Gets a starting situation that ch passes preconditions for, or nullptr if no such situation exists.
  Situation* get_starting_situation(struct char_data *ch);

  // Gets the next valid situation from an outcome's list for the given character.
  Situation* get_next_situation_from_outcome(const Outcome *outcome, struct char_data *ch);

  // Method to get a situation by its slug. NEVER store this result past immediate usage- if the map is moved (e.g. OLC), your pointer breaks!
  Situation* lookup_situation(const std::string& situationSlug) {
    auto it = situations.find(situationSlug);
    if (it != situations.end()) {
      return &(it->second);
    }
    return nullptr; // Situation not found
  }

  void save_to_disk();
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

  // Which activity is running? (this slug may become invalidated during OLC, so handle that gracefully)
  std::string running_activity_slug = "";

  // What Situation are you on? (this slug may become invalidated during OLC, so handle that gracefully)
  std::string current_situation_slug = "";

  // todo: any additional state tracking etc, like long-term effects, progress along a journey, etc
};

extern std::map<std::string, Activity> global_activities;

#undef STRING_GETTER

#endif // _ACTIVITIES_CLASSES_HPP_