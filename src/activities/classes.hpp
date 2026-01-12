#include <exception>
#include <string>
#include <map>
#include <vector>

#include <time.h>

#include "structs.hpp"

extern std::map<std::string, Activity> activities;


/*
Check: A boolean test that has a pass/fail state. Data includes:
- What is being tested (e.g. "has_skill", "skill_test", "has_item", "on_quest", etc)
- a parameters dict that contains further information (e.g. "on_quest" might be {"vnum": 3})
*/
class Check {
public:
  std::string type;
  std::map<std::string, std::string> parameters;

  Check(const std::string& type, const std::map<std::string, std::string>& parameters)
    : type(type), parameters(parameters) {}
  Check() = default;
  
  // True if this specific test passed for the character, false otherwise.
  bool test(struct char_data *ch);
};

/*
Effect: A side effect that, when encountered, modifies the party in some way. Data includes:
- the type of effect (from an enum, e.g. "lose_nuyen", "phys_damage", "quest_progress")
- a parameters dict that contains further information (e.g. "lose_nuyen"'s parameters might be {"amount": 50})
*/
class Effect {
public:
  std::string type;
  std::map<std::string, std::string> parameters;

  Effect(const std::string& type, const std::map<std::string, std::string>& parameters)
    : type(type), parameters(parameters) {}
  Effect() = default;

  // Applying an effect can result in death, at which point we raise an exception instead of using the old boolean return code system.
  void apply(struct char_data *ch);
};

/*
Outcome: An edge in the digraph routing from a situation to either another situation or the end of the activity. Data includes:
- the message displayed when this outcome is achieved, in the form of a sentence fragment that can follow "Assisted by X and Y, Lucien "
- an optional list of effects
- a list of Situation slugs that this outcome directs to (one will be randomly selected; empty if end of activity)
*/
class Outcome {
public:
  std::string message;
  std::vector<Effect> effects;
  std::vector<std::string> situations;

  Outcome(const std::string& message, const std::vector<Effect>& effects, const std::vector<std::string>& situations)
    : message(message), effects(effects), situations(situations) {}
  Outcome() = default;

  // Applying an outcome can result in death, so for now we return TRUE if that happens, FALSE otherwise. Later we'll just throw an exception.
  std::string *apply(struct char_data *ch) {
    // Apply all effects.
    for (auto& effect : effects) { effect.apply(ch); }

    // todo this isn't quite right, it's creaky if this is a group of PCs. Design more before further implementation

    // Return a random situation slug from the vector, or NULL if vector is empty.
    return situations.size() > 0 ? &situations[rand() % situations.size()] : nullptr;
  }
};

/*
Option: A pair of Outcomes that are selected between based on the result of zero or more tests (default success). Data includes:
- a list of Checks representing preconditions / requirements for this option to be available to a given PC to select
- text to display to players in the options menu
- a list of checks or tests to run when the option is selected
- a "pass" dict describing the 'good' outcome
- an optional "fail" dict describing the 'bad' outcome
*/
class Option {
public:
  std::string menu_text;
  std::vector<Check> preconditions;
  std::vector<Check> tests;
  Outcome *pass;
  Outcome *fail;

  Option(const std::string& menu_text, const std::vector<Check>& preconditions, const std::vector<Check>& tests, Outcome *pass, Outcome *fail)
    : menu_text(menu_text), preconditions(preconditions), tests(tests), pass(pass), fail(fail) {}
  Option() = default;

  // Applying an outcome can result in death, so for now we return TRUE if that happens, FALSE otherwise. Later we'll just throw an exception.
  bool evaluate(struct char_data *ch);
};

/*
Situation: A node in the activity digraph containing one or more Options as well as metadata about the situation itself. Data includes:
- a list of Checks representing preconditions / requirements for it to be encountered (only applies when being evaluated as a starting situation)
- slug
- text to display to players when they enter the situation
- a list of Options (if list is empty, this is a terminal node; display the text and exit activity)
- an optional list of Effects
*/
class Situation {
public:
  std::string slug;
  std::string text;
  std::vector<Check> preconditions;
  std::vector<Option> options;
  std::vector<Effect> effects;

  Situation(const std::string& slug, const std::string& text, const std::vector<Check>& preconditions, const std::vector<Option>& options, const std::vector<Effect>& effects)
    : slug(slug), text(text), preconditions(preconditions), options(options), effects(effects) {}
  Situation() = default;

  bool meets_preconditions(struct char_data *ch);
};

/*
Activity: A digraph containing one or more Situations as well as metadata about the activity itself. Data includes:
- authorship information (who, when, etc)
- identifying information (slug, display name)
- preconditions / requirements for it to be encountered
- dict of all possible situations in the activity, keyed by situation slug
- list of potential starting situation slugs to put the players in
*/
class Activity {

public:
    // Metadata
    std::string slug;
    std::string displayName;
    std::string author;
    time_t createdAt = time(0);
    time_t updatedAt = time(0);

    // Preconditions for the activity to be available
    // (e.g., character level, faction standing, items possessed)
    // This could be a more complex object or a list of conditions
    std::vector<Check> preconditions;

    // All possible situations in this activity, keyed by situation slug
    std::map<std::string, Situation> situations;

    // List of potential starting situation slugs
    std::vector<std::string> startingSituations;

    // Default constructor
    Activity() = default;

    // Copy constructor
    Activity(const Activity& other);

    // Load from JSON
    Activity(nlohmann::json json);

    // Method to get a situation by its slug
    Situation* getSituation(const std::string& situationSlug) {
        auto it = situations.find(situationSlug);
        if (it != situations.end()) {
            return &(it->second);
        }
        return nullptr; // Situation not found
    }
};