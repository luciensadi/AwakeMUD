#include "classes.hpp"
#include "../handler.hpp"
#include "../db.hpp"
#include "../constants.hpp"
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

extern int is_abbrev(const char *arg1, const char *arg2);

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
        {"count",    "How many times to log it (for testing).",   ActivityParamType::INTEGER, PARAM_OPTIONAL},
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
      "True if the character has the specified rank in the named skill.",
      {
        {"skill_idx", "Internal index of skill to check.",            ActivityParamType::SKILL_IDX,      PARAM_REQUIRED},
        {"rank",      "Rank between 1 and 12 inclusive (default 1)",  ActivityParamType::INTEGER_MAX_12, PARAM_OPTIONAL},
      },
      DETERMINISTIC,
    }},
    {"roll_skill", {
      &_check_function_roll_skill,
      "Rolls the named skill against a target number; passes on net successes > 0.",
      {
        {"skill_idx",  "Skill to roll (e.g. \"firearms\").", ActivityParamType::SKILL_IDX,  PARAM_REQUIRED},
        {"tn",         "Target number for the roll.",        ActivityParamType::INTEGER,    PARAM_REQUIRED},
      },
      NON_DETERMINISTIC,
    }},

    {"has_spell_active", {
      &_check_function_has_spell_active,
      "True if the character is currently affected by the specified spell.",
      {
        {"spell_idx", "Spell to look for.",                            ActivityParamType::SPELL_IDX, PARAM_REQUIRED},
        {"force",     "Force between 1 and 12 inclusive (default 1)",  ActivityParamType::INTEGER_MAX_12, PARAM_OPTIONAL},
      },
      DETERMINISTIC,
    }},
    {"could_cast_spell", {
      &_check_function_could_cast_spell,
      "True if the character knows the spell at >= the requested force and is capable of casting.",
      {
        {"spell_idx",  "Spell to look for.",                                               ActivityParamType::SPELL_IDX,      PARAM_REQUIRED},
        {"force",      "Force the character must know it at (1-12 inclusive, default 1)",  ActivityParamType::INTEGER_MAX_12, PARAM_OPTIONAL},
      },
      DETERMINISTIC,
    }},

    {"has_power_active", {
      &_check_function_has_power_active,
      "True if the character has the named adept power active at >= the requested rank.",
      {
        {"power_idx",  "Adept power to check.",                    ActivityParamType::POWER_IDX,      PARAM_REQUIRED},
        {"rank",       "Minimum rank required (>= 1, default 1).", ActivityParamType::INTEGER_MAX_12, PARAM_OPTIONAL},
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

// Family-locked accessors. These are the ONLY way to reach _check_registry from outside Check.cpp.
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

const std::string Check::stringify() const {
  return "stringify not implemented on check";
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
  GET_SETTING(setting_skill_idx);
  int skill_idx = atoi(setting_skill_idx);
  CHECK_FAILURE_CASE(skill_idx < 0, "Bad skill index: %s", setting_skill_idx);

  GET_SETTING_DEFAULT(setting_required_rank, "1");
  int required_rank = MAX(0, atoi(setting_required_rank));

  return GET_SKILL(ch, skill_idx) > 0 && GET_SKILL(ch, skill_idx) >= required_rank;
}

CHECK_FUNCTION(roll_skill) {
  GET_SETTING(setting_skill_idx);
  int skill_idx = atoi(setting_skill_idx);
  CHECK_FAILURE_CASE(skill_idx < 0, "Bad skill index: %s", setting_skill_idx);

  GET_SETTING(tn);
  int tn_val = atoi(tn);

  char writeout_buffer[10000] = {0};
  int skill_dice = get_skill(ch, skill_idx, tn_val, writeout_buffer, sizeof(writeout_buffer));
  int result = success_test(skill_dice, tn_val);
  return result > 0;
}

/////////////// Spells section

CHECK_FUNCTION(has_spell_active) {
  GET_SETTING(spell_idx);
  int parsed_spell_idx = atoi(spell_idx);
  CHECK_FAILURE_CASE(parsed_spell_idx < 0 || parsed_spell_idx >= MAX_SPELLS, "Bad spell idx: %s", spell_idx);

  GET_SETTING_DEFAULT(setting_force, "1");
  int force = MAX(0, atoi(setting_force));

  int affected_force = affected_by_spell(ch, parsed_spell_idx);

  return affected_force > 0 && affected_force >= force;
}

CHECK_FUNCTION(could_cast_spell) {
  // Skip people who can't cast at all.
  if (GET_ASPECT(ch) == ASPECT_CONJURER || GET_TRADITION(ch) == TRAD_ADEPT || GET_TRADITION(ch) == TRAD_MUNDANE || !GET_SKILL(ch, SKILL_SORCERY)) {
    return false;
  }

  GET_SETTING(spell_idx);
  int parsed_spell_idx = atoi(spell_idx);
  GET_SETTING_DEFAULT(force, "1");
  int spell_force = atoi(force);
  
  for (struct spell_data *spell = GET_SPELLS(ch); spell; spell = spell->next)
    if (spell->type == parsed_spell_idx)
      return spell->force >= spell_force;
  
  return false;
}

////////////// Powers section

CHECK_FUNCTION(has_power_active) {
  GET_SETTING(setting_power_idx);
  int power_idx = atoi(setting_power_idx);
  CHECK_FAILURE_CASE(power_idx < 0, "Bad power name: %s", setting_power_idx);

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
void _display_check_parameters_for_olc(struct descriptor_data *d, bool with_indentation) {
  auto settings_to_check = CHK->settings;
  for (auto param_itr : CHK->lookup_spec(CHK->get_func_name())->params) {
    auto check_param = settings_to_check.find(param_itr.name);

    send_to_char(CH, "%s%20s ", with_indentation ? "  " : "", std::string(param_itr.name + ":").c_str());

    if (check_param == settings_to_check.end()) {
      send_to_char(CH, "%s\r\n", (param_itr.required ? "^y(not set)^n [required]" : "^c(not set)^n"));
      continue;
    }

    // Special parsing: Spell, skill, and power idx are converted back to names.
    if (param_itr.type == ActivityParamType::SKILL_IDX || param_itr.type == ActivityParamType::SPELL_IDX || param_itr.type == ActivityParamType::POWER_IDX) {
      int parsed_value = atoi(check_param->second.c_str());
      switch (param_itr.type) {
        case ActivityParamType::SKILL_IDX:
          if (parsed_value >= 0 && parsed_value < MAX_SKILLS) {
            send_to_char(CH, "%s\r\n", skills[parsed_value].name);
          } else {
            send_to_char(CH, "<invalid! set me again>\r\n");
          }
          break;
        case ActivityParamType::SPELL_IDX:
          if (parsed_value >= 0 && parsed_value < MAX_SPELLS) {
            send_to_char(CH, "%s\r\n", spells[parsed_value].name);
          } else {
            send_to_char(CH, "<invalid! set me again>\r\n");
          }
          break;
        case ActivityParamType::POWER_IDX:
          if (parsed_value >= 0 && parsed_value < ADEPT_NUMPOWER) {
            send_to_char(CH, "%s\r\n", adept_powers[parsed_value]);
          } else {
            send_to_char(CH, "<invalid! set me again>\r\n");
          }
          break;
        default:
          break;
      }
    } else {
      send_to_char(CH, "%s\r\n", check_param->second.c_str());
    }
  }
}

////////////////////////////////////////////////////////
// CheckMenuFrame defines
////////////////////////////////////////////////////////

void CheckMenuFrame::display(struct descriptor_data *d) const {
  send_to_char(CH, "1) Function: ^c%s^n\r\n", CHK->get_func_name());
  
  if (CHK->func_ptr_is_set()) {
    send_to_char(CH, "\r\nParameters (enter a name to edit it):\r\n\r\n");
    _display_check_parameters_for_olc(d, true);
  }
  
  send_to_char(CH, "\r\n"
                   "q) Keep changes\r\n"
                   "x) Discard changes\r\n"
                   "\r\n"
                   "Enter your choice: ");
}

MenuFrameResult CheckMenuFrame::parse(struct descriptor_data *d, char *arg) {  
  // Handle quit-and-save and quit-and-discard modes.
  if (!str_cmp(arg, "x") || !str_cmp(arg, "q")) {
    if (!str_cmp(arg, "x")) {
      // Discard changes: drop our current val and replace it with a clone of original
      delete d->edit_check;
      d->edit_check = d->edit_check_original;
    }
    return { MenuFrameAction::Pop, child_identifier };
  }

  if (!str_cmp(arg, "1")) {
    MF_PUSH_FRAME(CheckFunctionMenuFrame, 1);
  }

  if (CHK->func_ptr_is_set()) {
    // They want to set a parameter, the name of which is in arg.
    for (auto param_itr : CHK->lookup_spec(CHK->get_func_name())->params) {
      // If they selected this parameter:
      if (is_abbrev(arg, param_itr.name.c_str())) {
        // Send them into the appropriate menuframe primitive to extract the value.
        switch (param_itr.type) {
          case ActivityParamType::STRING:
            MF_PROMPT_STRING_AND_RETURN("Enter a string: ", 0, [param_name=param_itr.name, d](std::string result){ CHK->settings[param_name] = result; });
          case ActivityParamType::SKILL_IDX:
            nextFrame = std::make_unique<SkillNamePromptFrame>("Enter the name of the skill: ", 0, [param_name=param_itr.name, d](int skill_idx){ CHK->settings[param_name] = std::to_string(skill_idx); });
            return { MenuFrameAction::Push };
          case ActivityParamType::SPELL_IDX:
            nextFrame = std::make_unique<SpellNamePromptFrame>("Enter the name of the spell: ", 0, [param_name=param_itr.name, d](int spell_idx){ CHK->settings[param_name] = std::to_string(spell_idx); });
            return { MenuFrameAction::Push };
          case ActivityParamType::POWER_IDX:
            nextFrame = std::make_unique<PowerNamePromptFrame>("Enter the name of the power: ", 0, [param_name=param_itr.name, d](int power_idx){ CHK->settings[param_name] = std::to_string(power_idx); });
            return { MenuFrameAction::Push };
          case ActivityParamType::INTEGER:
            MF_PROMPT_INT_AND_RETURN("Enter an integer: ", 0, [param_name=param_itr.name, d](int result){ CHK->settings[param_name] = std::to_string(result); });
          case ActivityParamType::OBJ_VNUM:
            nextFrame = std::make_unique<ObjVnumPromptFrame>("Enter the object vnum: ", 0, [param_name=param_itr.name, d](vnum_t vnum){ CHK->settings[param_name] = std::to_string(vnum); });
            return { MenuFrameAction::Push };
          case ActivityParamType::MOB_VNUM:
            nextFrame = std::make_unique<MobVnumPromptFrame>("Enter the mob's vnum: ", 0, [param_name=param_itr.name, d](vnum_t vnum){ CHK->settings[param_name] = std::to_string(vnum); });
            return { MenuFrameAction::Push };
          case ActivityParamType::ROOM_VNUM:
            nextFrame = std::make_unique<RoomVnumPromptFrame>("Enter the room's vnum: ", 0, [param_name=param_itr.name, d](vnum_t vnum){ CHK->settings[param_name] = std::to_string(vnum); });
            return { MenuFrameAction::Push };
          case ActivityParamType::QUEST_VNUM:
            nextFrame = std::make_unique<QuestVnumPromptFrame>("Enter the quest's vnum: ", 0, [param_name=param_itr.name, d](vnum_t vnum){ CHK->settings[param_name] = std::to_string(vnum); });
            return { MenuFrameAction::Push };
          case ActivityParamType::NUYEN_AMOUNT:
            nextFrame = std::make_unique<NuyenAmountPromptFrame>("Enter the nuyen quantity: ", 0, [param_name=param_itr.name, d](int amount){ CHK->settings[param_name] = std::to_string(amount); });
            return { MenuFrameAction::Push };
          case ActivityParamType::BOOLEAN:
            MF_PROMPT_YESNO_AND_RETURN("Enter Y or N: ", 0, [param_name=param_itr.name, d](bool result){ CHK->settings[param_name] = std::to_string(result); });
          case ActivityParamType::KARMA_AMOUNT:
            nextFrame = std::make_unique<KarmaAmountPromptFrame>("Enter the karma amount as a decimal number (ex: 2.3): ", 0, [param_name=param_itr.name, d](float amount){ CHK->settings[param_name] = std::to_string(amount); });
            return { MenuFrameAction::Push };
          case ActivityParamType::INTEGER_MAX_12:
            nextFrame = std::make_unique<IntPromptFrame>("Enter a value between 0 and 12 (inclusive): ", 0, [param_name=param_itr.name, d](vnum_t vnum){ CHK->settings[param_name] = std::to_string(vnum); }, 0, 12);
            return { MenuFrameAction::Push };
        };
      }
    }
  }

  // If they got here, their specified parameter name was not part of the check's valid parameter list; show an error and bail
  MF_PARSE_FAILED("That's not a valid selection. Enter a choice number%s, 'q' to save, or 'x' to abort without saving.\r\n",
                         !CHK->func_ptr_is_set() ? ", the name of the parameter you want to edit" : "");
}

const MenuFrameResult CheckMenuFrame::handle_child_response(struct descriptor_data *d, const MenuFrameResult & result) { return { MenuFrameAction::JustDisplay }; }

////////////////////////////////////////////////////////
// CheckFunctionMenuFrame defines
////////////////////////////////////////////////////////

void CheckFunctionMenuFrame::display(struct descriptor_data *d) const {
  if (CHK->func_ptr_is_set()) {
    send_to_char(CH, "This check is currently set to the function '^c%s^n':\r\n", CHK->get_func_name());
    send_to_char(CH, "  ^c%s^n\r\n", CHK->lookup_spec(CHK->get_func_name())->description.c_str());
    send_to_char(CH, "^yChanging it will clear the existing parameters.^n\r\n");
  }
  send_to_char(CH, "Available functions:\r\n");
  for (auto func_itr : _check_registry) {
    send_to_char(CH, "  ^W%20s^n: ^c%s^n\r\n", func_itr.first.c_str(), func_itr.second.description.c_str());
  }
  send_to_char(CH, "\r\nSelect a new function name, or 'abort' to cancel:");
}

MenuFrameResult CheckFunctionMenuFrame::parse(struct descriptor_data *d, char *arg) {
  if (!str_cmp(arg, "abort") || !str_cmp(arg, CHK->get_func_name())) {
    send_to_char(CH, "OK, aborting without changes.\r\n");
    return { MenuFrameAction::Pop };
  }

  // Valid function name-- blow away current check and replace it with a new one of that function.
  if (_check_registry.find(arg) != _check_registry.end()) {
    delete CHK;
    CHK = new Check(std::string(arg), {});
    return { MenuFrameAction::Pop, child_identifier };
  }

  MF_PARSE_FAILED("That's not a valid function name. Enter a slug from above, or 'abort' to cancel.");
}

const MenuFrameResult CheckFunctionMenuFrame::handle_child_response(struct descriptor_data *d, const MenuFrameResult & result) { return { MenuFrameAction::JustDisplay }; }

void debug_check_menu(struct char_data *ch) {
  send_to_char(ch, "entering check menu debug:\r\n");
  ch->desc->edit_check = new Check(); 
  ch->desc->edit_check_original = new Check();
  push_menu_frame(ch->desc, std::make_unique<CheckMenuFrame>(0));
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
