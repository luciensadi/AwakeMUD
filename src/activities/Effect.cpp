#include "classes.hpp"
#include "../handler.hpp"
#include "../db.hpp"
#include "../nlohmann/json.hpp"

/*
Effect: A side effect that, when encountered, modifies the party in some way. Data includes:
- the type of effect (e.g. "lose_nuyen", "phys_damage", "quest_progress")
- a settings dict that contains further information (e.g. "lose_nuyen" might be {"amount": "50"})
*/

#define EFFECT_FUNCTION(func_name) bool _effect_function_##func_name(struct char_data *ch, const std::map<std::string, std::string>& settings)

// Effect function prototypes. Remember, effect functions return TRUE on death, FALSE otherwise!
EFFECT_FUNCTION(always_false);
EFFECT_FUNCTION(test_func);

// Maps slugs to effect functions. Remember, effect functions return TRUE on death, FALSE otherwise!
#define MAP_EFFECT_FUNCTION(slug, func_name) {slug, (void*)&_effect_function_##func_name}
std::map<std::string, void *> _effect_type_to_function = {
  MAP_EFFECT_FUNCTION("_test_func", test_func)
};

Effect::Effect(const std::string& supplied_type, const std::map<std::string, std::string>& supplied_settings)
  : ActivityFunction(supplied_type, supplied_settings, _effect_type_to_function) {}

// Deserialize a new Effect from a JSON string.
Effect::Effect(const std::string serialized)
  : ActivityFunction(serialized, _effect_type_to_function, (void*)&_effect_function_always_false) {}

// Runs the associated func_ptr and returns its value. Returns true if the character dies.
bool Effect::apply(struct char_data *ch) {
  return invoke(ch);
}

// Serialization function.
void to_json(json& j, const Effect& e) {
  j = json{
    {"func", e.func_name}, 
    {"settings", e.settings}
  };
}
// Deserialization function.
void from_json(const json& j, Effect& e) {
    // .at() is safer than [] because it throws an error if the key is missing
    j.at("func").get_to(e.func_name);
    j.at("settings").get_to(e.settings);
}
// And the wrapper to get a string out of the serialization function. Not sure I actually need this.
std::string Effect::serialize() {
  json basic_info;
  to_json(basic_info, *this);
  return basic_info.dump();
}

///////////// Effect function definitions below. Remember, effect functions return TRUE on death, FALSE otherwise!
#define EFFECT_FAILURE_CASE(condition, fmt, ...) {                                \
  if ((condition)) {                                                              \
    mudlog_vfprintf(ch, LOG_SYSLOG, "Effect %s: " fmt, __func__, ##__VA_ARGS__); \
    return false;                                                                 \
  }                                                                               \
}

// Fetch a setting, failing (returning false, i.e. no death) if it's not found.
#define GET_SETTING(setting_name)                                               \
    auto it_##setting_name = settings.find(#setting_name);                      \
    if (it_##setting_name == settings.end()) {                                  \
        mudlog_vfprintf(ch, LOG_SYSLOG,                                         \
            "SYSERR: Effect %s: Called with missing required setting %s.",      \
            __func__, #setting_name);                                           \
        return false;                                                           \
    }                                                                           \
    const char *setting_name = it_##setting_name->second.c_str();

// Fetch a setting, returning the default if not found.
#define GET_SETTING_DEFAULT(setting_name, default_val)                                                                 \
    auto it_##setting_name = settings.find(#setting_name);                                                             \
    const char *setting_name = it_##setting_name != settings.end() ? it_##setting_name->second.c_str() : default_val;

EFFECT_FUNCTION(always_false) { return false; }

EFFECT_FUNCTION(test_func) {
  mudlog_vfprintf(ch, LOG_SYSLOG, "Effect() called with _effect_test_func(ch, {}x%d), returning false.", settings.size());
  return false;
};

#undef EFFECT_FUNCTION




//// shitty little debug test function, tucked out of the way down here
void run_effect_debug_tests(struct char_data *ch) {
  Effect *effect = new Effect("_test_func", {{"a", "b"}});
  if (effect->apply(ch)) { mudlog_vfprintf(NULL, LOG_SYSLOG, "Applying debug test caused character death, RIP"); return; }
  send_to_char(ch, "Effect applied.\r\n");
  
  auto serialized = effect->serialize();
  send_to_char(ch, "Serialized, the effect is '%s'\r\n", serialized.c_str());
  Effect *deserialized = new Effect(serialized);
  auto reserialized = deserialized->serialize();
  send_to_char(ch, "Reserialization: %s\r\n", !strcmp(serialized.c_str(), reserialized.c_str()) ? "match!" : "mismatch");

  if (effect->apply(ch)) { mudlog_vfprintf(NULL, LOG_SYSLOG, "Applying reserialized debug test caused character death, RIP"); return; }
  send_to_char(ch, "Reserialized effect applied.\r\n");

  delete deserialized;
  delete effect;
}