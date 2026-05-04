#ifndef _ACTIVITIES_CLASSES_HPP_
#define _ACTIVITIES_CLASSES_HPP_

#include <exception>
#include <string>
#include <map>
#include <vector>

#include <time.h>

#include "../structs.hpp"

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


/*
BEFORE YOU GO ANY FURTHER:

Figure out the flow of messaging - where does it come from, especially the final result message set?

"Assisted by X and Y, Lucien kicks the door down."
Only the evaluation of the RunningActivity really knows who did what, so we need to pass the info all the way up the chain about:
- what a character did, and if they passed
- what the message to that character would be

Then it needs to select the outcome it's actually using from the pass list, and ONLY THEN do we apply the effects of the outcome.

Probably need to rearchitect so that an Option.evaluate(ch) _returns the Outcome_ rather than trying to evaluate anything.
This allows RunningActivity (or whatever is coordinating it) to loop through each character to select the Outcomes they've earned, then select from those, and then print the message and evaluate it.
*/



















// Base class shared by Check and Effect.
// Holds the function pointer, func_name, settings, and provides serialization.
// Derived classes supply their own function registry via the protected constructors.
class ActivityFunction {
protected:
  void *func_ptr = nullptr;

  // Regular construction: looks up type in func_map and stores the pointer.
  ActivityFunction(const std::string& type,
                   const std::map<std::string, std::string>& settings,
                   const std::map<std::string, void*>& func_map);

  // Deserializing construction: parses JSON, looks up func_name in func_map.
  // Falls back to fallback_func_ptr (and logs a SYSERR) if the name is not found.
  ActivityFunction(const std::string& serialized,
                   const std::map<std::string, void*>& func_map,
                   void* fallback_func_ptr);

  virtual ~ActivityFunction() = default;

  // Dispatches to func_ptr. Returns false (with a SYSERR log) if func_ptr is null.
  bool invoke(struct char_data *ch);

public:
  std::string func_name = "not set"; // The string that identifies the function to run.
  std::map<std::string, std::string> settings = {}; // A map of settings/parameters.

  ActivityFunction() = default;

  virtual std::string serialize() = 0;
};

// A boolean test that has a pass/fail state.
// Assertion: Check functions can NEVER result in character death (they must not mutate the character).
class Check : public ActivityFunction {
public:
  Check(const std::string& type, const std::map<std::string, std::string>& settings);
  Check(const std::string serialized);
  Check() = default;
  
  std::string serialize() override;

  // True if this specific test passed for the character, false otherwise.
  bool test(struct char_data *ch);
};

// A side effect that, when encountered, modifies the character and/or their RunningActivity in some way. (todo add ptr from char struct to RunningActivity)
class Effect : public ActivityFunction {
public:
  Effect(const std::string& type, const std::map<std::string, std::string>& settings);
  Effect(const std::string serialized);
  Effect() = default;
  
  std::string serialize() override;

  // Applying an effect can result in death. Returns true when this happens.
  bool apply(struct char_data *ch);
};

// An edge in the digraph that routes from a situation to either another situation or the end of the activity.
class Outcome {
public:
  std::string message; // A fragment that would fit after both 'Lucien ' and 'Assisted by X and Y, Lucien '.
  // e.g. "breaks down the door with his enormous schlong!"
  bool is_pass_outcome;

  std::vector<Effect> effects = {};
  // An optional list of effects that apply to all characters in the party when this outcome is selected.

  std::vector<std::string> situations = {};
  // A list of situations this outcome can branch to, or empty if the end of the activity.

  Outcome(const std::string message, const std::vector<Effect> effects, const std::vector<std::string> situations, bool is_pass_outcome) :
          message(message), is_pass_outcome(is_pass_outcome), effects(effects), situations(situations) {}
  Outcome(const std::string serialized_json, bool is_pass_outcome);
  Outcome() = default;

  // Apply all effects to the selected character. Return true if they die (i.e. they have been extracted.)
  bool apply(struct char_data *ch);

  // Fetch a random situation slug from the available set, or returns "no situation slug available" if not yet set.
  std::string get_next_situation_slug();

  std::string serialize();
};

// A pair of Outcomes that are selected between based on the result of zero or more tests (default success).
class Option {
public:
  std::string slug; // identifies the option (for editing etc)
  std::string menu_text; // the text that shows in the selection menu to describe the option to the player

  std::vector<Check> preconditions; // checks that must pass for the player to be able to select this option
  std::vector<Check> tests; // checks that must pass for the player to receive the Pass outcome, otherwise get the Fail outcome

  Outcome pass; // an outcome describing the 'good' version of this (passed check). Printed string comes from here.
  Outcome fail; // an outcome describing the 'bad' version of this (failed check). Printed string comes from here.

  // Testing constructor.
  Option(const std::string slug, const std::string menu_text, std::vector<Check> preconditions, std::vector<Check> tests, Outcome pass, Outcome fail) : 
         slug(slug), menu_text(menu_text), preconditions(preconditions), tests(tests), pass(pass), fail(fail) {};

  Option(const std::string serialized_json);

  Option() = default;

  // Returns TRUE if you successfully pass all the preconditions to even see this option, FALSE otherwise.
  bool is_available_to_ch(struct char_data *ch) { for (auto check : preconditions) { if (!check.test(ch)) return false; } return true; }

  // Returns the Fail outcome if you fail any tests, the Pass outcome otherwise.
  Outcome *test(struct char_data *ch) { for (auto check : tests) { if (!check.test(ch)) return &fail; } return &pass; }

  std::string serialize();
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

  bool meets_preconditions(struct char_data *ch) { return false; /* todo */};
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
  Activity(const Activity& other) {};

  // Deserialize from JSON string.
  Activity(std::string serialized_json) {};

  ~Activity() {};

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