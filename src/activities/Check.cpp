#include "classes.hpp"
#include "../handler.hpp"
#include "../db.hpp"
#include "nlohmann/json.hpp"

#include "activity_olc.hpp"

/*
Check: A boolean test that has a pass/fail state. Data includes:
- What is being tested (e.g. "has_skill", "skill_test", "has_item", "on_quest", etc)
- a settings dict that contains further information (e.g. "on_quest" might be {"vnum": 3})

The registry below is the single source of truth for everything about a check:
function pointer, human description, parameter schema (with types), and the
deterministic-vs-non-deterministic flag (used by Activity preconditions, which
must never be non-deterministic).

The registry is in an anonymous namespace so it has internal linkage -- no other
translation unit can extern it. All cross-file consumers go through
Check::lookup_spec() / Check::list_slugs(), which are family-locked: you cannot
accidentally pass an Effect spec where a Check spec is expected.
*/

#define CHECK_FUNCTION(func_name) bool _check_function_##func_name(struct char_data *ch, const std::map<std::string, std::string>& settings)

// Check function prototypes. Remember, check functions can NEVER result in character death!
CHECK_FUNCTION(test_func);
CHECK_FUNCTION(always_false);

CHECK_FUNCTION(has_skill);
CHECK_FUNCTION(roll_skill);
CHECK_FUNCTION(has_spell_active);
CHECK_FUNCTION(could_cast_spell);
CHECK_FUNCTION(has_power_active);
CHECK_FUNCTION(has_item);
CHECK_FUNCTION(random);

namespace {
  // The Check registry. File-local (internal linkage) -- access from other
  // translation units MUST go through Check::lookup_spec / Check::list_slugs.
  const std::map<std::string, ActivityFuncSpec> _check_registry = {
    {"_test_func", {
      &_check_function_test_func,
      "Debug-only check that always returns true (and logs the call).",
      {
        {"message",  "A string to echo in the log output.",       ActivityParamType::STRING,  PARAM_REQUIRED},
        {"count",    "How many times to log it (for testing).",   ActivityParamType::INTEGER, PARAM_OPTIONAL, "1"},
      },
      DETERMINISTIC,
    }},
    {"always_false", {
      &_check_function_always_false,
      "Always fails. Used as a fallback when an unknown or invalid check is loaded.",
      {},
      DETERMINISTIC,
    }},

    {"has_skill", {
      &_check_function_has_skill,
      "True if the character has any rank in the named skill.",
      {
        {"skill_name", "Skill to check (e.g. \"firearms\").", ActivityParamType::SKILL_NAME, PARAM_REQUIRED},
      },
      DETERMINISTIC,
    }},
    {"roll_skill", {
      &_check_function_roll_skill,
      "Rolls the named skill against a target number; passes on net successes > 0.",
      {
        {"skill_name", "Skill to roll (e.g. \"firearms\").", ActivityParamType::SKILL_NAME, PARAM_REQUIRED},
        {"tn",         "Target number for the roll.",        ActivityParamType::INTEGER,    PARAM_REQUIRED},
      },
      NON_DETERMINISTIC,
    }},

    {"has_spell_active", {
      &_check_function_has_spell_active,
      "True if the character is currently affected by the named spell.",
      {
        {"spell_name", "Spell to look for.", ActivityParamType::SPELL_NAME, PARAM_REQUIRED},
      },
      DETERMINISTIC,
    }},
    {"could_cast_spell", {
      &_check_function_could_cast_spell,
      "True if the character knows the spell at >= the requested force and is capable of casting.",
      {
        {"spell_name", "Spell to look for.",                                ActivityParamType::SPELL_NAME, PARAM_REQUIRED},
        {"force",      "Minimum force the character must know it at.",     ActivityParamType::INTEGER,    PARAM_OPTIONAL, "1"},
      },
      DETERMINISTIC,
    }},

    {"has_power_active", {
      &_check_function_has_power_active,
      "True if the character has the named adept power active at >= the requested rank.",
      {
        {"power_name", "Adept power to check.",         ActivityParamType::POWER_NAME, PARAM_REQUIRED},
        {"rank",       "Minimum rank required (>= 1).", ActivityParamType::INTEGER,    PARAM_OPTIONAL, "1"},
      },
      DETERMINISTIC,
    }},

    {"has_item", {
      &_check_function_has_item,
      "True if the character is carrying or wearing an object with the given vnum.",
      {
        {"item_vnum", "Vnum of the object to look for.", ActivityParamType::OBJ_VNUM, PARAM_REQUIRED},
      },
      DETERMINISTIC,
    }},

    {"random", {
      &_check_function_random,
      "Rolls 0..max_value-1 and passes if the result is <= this_or_lower (i.e. probabilistic gate).",
      {
        {"max_value",     "Upper bound (exclusive) of the random roll. Must be >= 1.", ActivityParamType::INTEGER, PARAM_REQUIRED},
        {"this_or_lower", "Pass if the roll is < this value.",                         ActivityParamType::INTEGER, PARAM_REQUIRED},
      },
      NON_DETERMINISTIC,
    }},
  };
} // anonymous namespace

// Family-locked accessors. These are the ONLY way to reach _check_registry
// from outside Check.cpp.
const ActivityFuncSpec* Check::lookup_spec(const std::string& slug) {
  auto it = _check_registry.find(slug);
  if (it == _check_registry.end()) return nullptr;
  return &(it->second);
}

std::vector<std::string> Check::list_slugs() {
  std::vector<std::string> out;
  out.reserve(_check_registry.size());
  for (const auto& kv : _check_registry) out.push_back(kv.first);
  return out;
}

// Serialization function.
void to_json(json& j, const Check& e) {
  j = json{
    {"func", e.func_name}, 
    {"settings", e.settings}
  };
}
// Deserialization function.
void from_json(const json& j, Check& e) {
    // .at() is safer than [] because it throws an error if the key is missing
    j.at("func").get_to(e.func_name);
    j.at("settings").get_to(e.settings);
    e.resolve_ptr(_check_registry);
}
// And the wrapper to get a string out of that.
std::string Check::serialize(const int indent, const char indent_char) {
  json basic_info;
  to_json(basic_info, *this);
  return basic_info.dump(indent, indent_char);
}

Check::Check(const std::string& supplied_type, const std::map<std::string, std::string>& supplied_settings)
  : ActivityFunction(supplied_type, supplied_settings, _check_registry, (ActivityFuncPtr)&_check_function_always_false) {}

// Deserialize a new Check from a JSON string.
Check::Check(const std::string serialized)
  : ActivityFunction(serialized, _check_registry, (ActivityFuncPtr)&_check_function_always_false) {}

// Runs the associated func_ptr and returns its value.
bool Check::test(struct char_data *ch) {
  return invoke(ch);
}

const char *Check::stringify() const {
  // todo
  return nullptr;
}

///////////// Check function definitions below. Remember, check functions can NEVER result in character death!
#define CHECK_FAILURE_CASE(condition, fmt, ...) {                               \
  if ((condition)) {                                                            \
    mudlog_vfprintf(ch, LOG_SYSLOG, "Check %s: " fmt, __func__, ##__VA_ARGS__); \
    return false;                                                               \
  }                                                                             \
}

// Fetch a setting, failing if it's not found. Note: with the registry's
// validation in ActivityFunction's ctors, missing required params should never
// reach this point. The macro stays as defense-in-depth.
#define GET_SETTING(setting_name)                                               \
    auto it_##setting_name = settings.find(#setting_name);                      \
    if (it_##setting_name == settings.end()) {                                  \
        mudlog_vfprintf(ch, LOG_SYSLOG,                                         \
            "SYSERR: Check %s: Called with missing required setting %s.",       \
            __func__, #setting_name);                                           \
        return false;                                                           \
    }                                                                           \
    const char *setting_name = it_##setting_name->second.c_str();

// Fetch a setting, returning the default if not found.
#define GET_SETTING_DEFAULT(setting_name, default_val)                                                                \
    auto it_##setting_name = settings.find(#setting_name);                                                            \
    const char *setting_name = it_##setting_name != settings.end() ? it_##setting_name->second.c_str() : default_val;

CHECK_FUNCTION(always_false) { return false; }

CHECK_FUNCTION(test_func) {
  GET_SETTING(message);
  GET_SETTING_DEFAULT(count, "1");
  int count_val = atoi(count);
  for (int i = 0; i < count_val; i++)
    mudlog_vfprintf(ch, LOG_SYSLOG, "Check _test_func: message='%s' (%d/%d), returning true.", message, i + 1, count_val);
  return true;
}

/////////////// Skills section

CHECK_FUNCTION(has_skill) {
  GET_SETTING(skill_name);
  int skill_idx = skill_name_to_idx(skill_name);
  CHECK_FAILURE_CASE(skill_idx < 0, "Bad skill name: %s", skill_name);
  return GET_SKILL(ch, skill_idx) > 0;
}

CHECK_FUNCTION(roll_skill) {
  GET_SETTING(skill_name);
  int skill_idx = skill_name_to_idx(skill_name);
  CHECK_FAILURE_CASE(skill_idx < 0, "Bad skill name: %s", skill_name);

  GET_SETTING(tn);
  int tn_val = atoi(tn);

  char writeout_buffer[10000] = {0};
  int skill_dice = get_skill(ch, skill_idx, tn_val, writeout_buffer, sizeof(writeout_buffer));
  int result = success_test(skill_dice, tn_val);
  return result > 0;
}

/////////////// Spells section

CHECK_FUNCTION(has_spell_active) {
  GET_SETTING(spell_name);
  int spell_idx = spell_name_to_idx(spell_name);
  CHECK_FAILURE_CASE(spell_idx < 0, "Bad spell name: %s", spell_name);
  return affected_by_spell(ch, spell_idx) > 0;
}

CHECK_FUNCTION(could_cast_spell) {
  // Skip people who can't cast at all.
  if (GET_ASPECT(ch) == ASPECT_CONJURER || GET_TRADITION(ch) == TRAD_ADEPT || GET_TRADITION(ch) == TRAD_MUNDANE || !GET_SKILL(ch, SKILL_SORCERY)) {
    return false;
  }

  GET_SETTING(spell_name);
  int spell_idx = spell_name_to_idx(spell_name);
  GET_SETTING_DEFAULT(force, "1");
  int spell_force = atoi(force);
  
  for (struct spell_data *spell = GET_SPELLS(ch); spell; spell = spell->next)
    if (spell->type == spell_idx)
      return spell->force >= spell_force;
  
  return false;
}

////////////// Powers section

CHECK_FUNCTION(has_power_active) {
  GET_SETTING(power_name);
  int power_idx = power_name_to_idx(power_name);
  CHECK_FAILURE_CASE(power_idx < 0, "Bad power name: %s", power_name);

  GET_SETTING_DEFAULT(rank, "1");
  int power_rank = atoi(rank);

  return affected_by_power(ch, power_idx) >= power_rank;
}

///////////// Items, equipment, etc

// TODO: You know they're gonna want to check for multiple items ('all of these uniform items'), any item of type (ITEM_WEAPON), etc... expand this
CHECK_FUNCTION(has_item) {
  GET_SETTING(item_vnum);
  vnum_t vnum = atol(item_vnum);

  for (struct obj_data *obj = ch->carrying; obj; obj = obj->next_content) if (GET_OBJ_VNUM(obj) == vnum) return true;

  for (int wear_idx = 0; wear_idx < NUM_WEARS; wear_idx++) {
    if (GET_EQ(ch, wear_idx) && GET_OBJ_VNUM(GET_EQ(ch, wear_idx)) == vnum) return true;
  }

  return false;
}

///////////// Others / misc
CHECK_FUNCTION(random) {
  GET_SETTING(max_value);
  int max_val = atoi(max_value);

  GET_SETTING(this_or_lower);
  int threshold = atoi(this_or_lower);

  return (random() % max_val) < threshold;
}

#undef CHECK_FUNCTION


/*
class Check : public ActivityFunction {
public:  
  std::string serialize(const int indent = -1, const char indent_char = ' ') override;

  std::string func_name = "not set"; // The string that identifies the function to run.
  std::map<std::string, std::string> settings = {}; // A map of settings/parameters.
  
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
*/


//// OLC
void CheckMenuFrame::display(struct descriptor_data *d) const {
  send_to_char(CH, "1) Function: %s\r\n", CHK->func_name.empty() ? "(not set)" : CHK->func_name);

  send_to_char(CH, "2) Settings:\r\n");

  // TODO: look up the settings config from the registry
  // TODO: list required and optional settings, and what they're currently set to

  send_to_char(CH, "\r\n"
                   "q) Keep changes\r\n",
                   "x) Discard changes\r\n"
                   "\r\n"
                   "Enter your choice: ");
}

MenuFrameResult CheckMenuFrame::parse(struct descriptor_data *d, char *arg) {
  switch (*arg) {
    case '1':
      MF_PROMPT_INT_AUTOSET_D_AND_RETURN("Type an int between 0-10: ", edit_number2, 0, 10);
    case '2':
      // todo: push a settings frame
      break;
    case 'x':
      // Discard changes: drop our current val and replace it with a clone of original
      delete d->edit_check;
      d->edit_check = d->edit_check_original;
      // fall through
    case 'q': // 'save' changes and quit-- this is a no-op, we want to maintain what we've done to the d->edit_check.
      return { MenuFrameAction::Pop };
  }
  send_to_char(d->character, "'%s' is not a valid selection, try again:", arg);
  return { MenuFrameAction::JustDisplay };
}



//// shitty little debug test function, tucked out of the way down here
void run_check_debug_tests(struct char_data *ch) {
  // Valid params + one unknown ("not_a_param") to verify the typo-guard fires exactly once.
  Check *check = new Check("_test_func", {{"message", "hello from check test"}, {"count", "2"}, {"not_a_param", "oops"}});
  send_to_char(ch, "You %s!\r\n", check->test(ch) ? "passed" : "failed");
  
  auto serialized = check->serialize(2);
  send_to_char(ch, "Serialized, the check is '%s'\r\n", serialized.c_str());
  Check *deserialized = new Check(serialized);
  auto reserialized = deserialized->serialize(2);
  send_to_char(ch, "Reserialization: %s\r\n", !strcmp(serialized.c_str(), reserialized.c_str()) ? "match!" : "mismatch");
  send_to_char(ch, "You %s the reserialized check.\r\n", deserialized->test(ch) ? "passed" : "failed");

  delete deserialized;
  delete check;
}
