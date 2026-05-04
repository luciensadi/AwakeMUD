#include "classes.hpp"
#include "../handler.hpp"
#include "../db.hpp"
#include "../nlohmann/json.hpp"

/*
Check: A boolean test that has a pass/fail state. Data includes:
- What is being tested (e.g. "has_skill", "skill_test", "has_item", "on_quest", etc)
- a settings dict that contains further information (e.g. "on_quest" might be {"vnum": 3})
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

// Maps slugs to check functions. Remember, check functions can NEVER result in character death!
#define MAP_CHECK_FUNCTION(slug, func_name) {slug, (void*)&_check_function_##func_name}
std::map<std::string, void *> _check_type_to_function = {
  MAP_CHECK_FUNCTION("_test_func", test_func),
  MAP_CHECK_FUNCTION("always_false", always_false),

  MAP_CHECK_FUNCTION("has_skill", has_skill),
  MAP_CHECK_FUNCTION("roll_skill", roll_skill),

  MAP_CHECK_FUNCTION("has_spell_active", has_spell_active),
  MAP_CHECK_FUNCTION("could_cast_spell", could_cast_spell),
};

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
    e.resolve_ptr(_check_type_to_function);
}
// And the wrapper to get a string out of that.
std::string Check::serialize(const int indent, const char indent_char) {
  json basic_info;
  to_json(basic_info, *this);
  return basic_info.dump(indent, indent_char);
}

Check::Check(const std::string& supplied_type, const std::map<std::string, std::string>& supplied_settings)
  : ActivityFunction(supplied_type, supplied_settings, _check_type_to_function) {}

// Deserialize a new Check from a JSON string.
Check::Check(const std::string serialized)
  : ActivityFunction(serialized, _check_type_to_function, (void*)&_check_function_always_false) {}

// Runs the associated func_ptr and returns its value.
bool Check::test(struct char_data *ch) {
  return invoke(ch);
}

///////////// Check function definitions below. Remember, check functions can NEVER result in character death!
#define CHECK_FAILURE_CASE(condition, fmt, ...) {                               \
  if ((condition)) {                                                            \
    mudlog_vfprintf(ch, LOG_SYSLOG, "Check %s: " fmt, __func__, ##__VA_ARGS__); \
    return false;                                                               \
  }                                                                             \
}

// Fetch a setting, failing if it's not found.
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
  mudlog_vfprintf(ch, LOG_SYSLOG, "Check() called with _check_test_func(ch, {}x%d), returning true.", settings.size());
  return true;
};

/////////////// Skills section

CHECK_FUNCTION(has_skill) {
  GET_SETTING(skill_name);
  int skill_idx = skill_name_to_idx(skill_name);
  CHECK_FAILURE_CASE(skill_idx < 0, "Bad skill name: %s", skill_name);
  return GET_SKILL(ch, skill_idx) > 0;
};

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

#undef CHECK_FUNCTION



//// shitty little debug test function, tucked out of the way down here
void run_check_debug_tests(struct char_data *ch) {
  Check *check = new Check("_test_func", {{"a", "b"}});
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