#include "classes.hpp"
#include "../handler.hpp"
#include "../db.hpp"
#include "nlohmann/json.hpp"

/*
Effect: A side effect that, when encountered, modifies the party in some way. Data includes:
- the type of effect (e.g. "lose_nuyen", "phys_damage", "quest_progress")
- a settings dict that contains further information (e.g. "lose_nuyen" might be {"amount": "50"})

The registry below is the single source of truth for everything about an
effect: function pointer, human description, parameter schema (with types),
and the deterministic-vs-non-deterministic flag.

The registry is in an anonymous namespace so it has internal linkage -- no
other translation unit can extern it. All cross-file consumers go through
Effect::lookup_spec() / Effect::list_slugs(), which are family-locked: you
cannot accidentally pass a Check spec where an Effect spec is expected.
*/

#define EFFECT_FUNCTION(func_name) bool _effect_function_##func_name(struct char_data *ch, const std::map<std::string, std::string>& settings)

// Effect function prototypes. Remember, effect functions return TRUE on death, FALSE otherwise!
EFFECT_FUNCTION(always_false);
EFFECT_FUNCTION(test_func);

namespace {
  // The Effect registry. File-local (internal linkage) -- access from other
  // translation units MUST go through Effect::lookup_spec / Effect::list_slugs.
  const std::map<std::string, ActivityFuncSpec> _effect_registry = {
    {"_test_func", {
      &_effect_function_test_func,
      "Debug-only effect that does nothing (and logs the call). Never kills.",
      {
        {"message",  "A string to echo in the log output.",       ActivityParamType::STRING,  PARAM_REQUIRED},
        {"count",    "How many times to log it (for testing).",   ActivityParamType::INTEGER, PARAM_OPTIONAL},
      },
      DETERMINISTIC,
    }},
    {"always_false", {
      &_effect_function_always_false,
      "Does nothing. Used as a fallback when an unknown or invalid effect is loaded.",
      {},
      DETERMINISTIC,
    }},
  };
} // anonymous namespace

// Family-locked accessors. These are the ONLY way to reach _effect_registry
// from outside Effect.cpp.
const ActivityFuncSpec* Effect::lookup_spec(const std::string& slug) {
  auto it = _effect_registry.find(slug);
  if (it == _effect_registry.end()) return nullptr;
  return &(it->second);
}

std::vector<std::string> Effect::list_slugs() {
  std::vector<std::string> out;
  out.reserve(_effect_registry.size());
  for (const auto& kv : _effect_registry) out.push_back(kv.first);
  return out;
}

Effect::Effect(const std::string& supplied_type, const std::map<std::string, std::string>& supplied_settings)
  : ActivityFunction(supplied_type, supplied_settings, _effect_registry, (ActivityFuncPtr)&_effect_function_always_false) {}

// Deserialize a new Effect from a JSON string.
Effect::Effect(const std::string serialized)
  : ActivityFunction(serialized, _effect_registry, (ActivityFuncPtr)&_effect_function_always_false) {}

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
    e.resolve_ptr(_effect_registry);
}
// And the wrapper to get a string out of the serialization function. Not sure I actually need this.
std::string Effect::serialize(const int indent, const char indent_char) {
  json basic_info;
  to_json(basic_info, *this);
  return basic_info.dump(indent, indent_char);
}

const std::string Effect::stringify() const {
  return std::string("(effect stringify not implemented)");
}

///////////// Effect function definitions below. Remember, effect functions return TRUE on death, FALSE otherwise!
#define EFFECT_FAILURE_CASE(condition, fmt, ...) {                                \
  if ((condition)) {                                                              \
    mudlog_vfprintf(ch, LOG_SYSLOG, "Effect %s: " fmt, __func__, ##__VA_ARGS__); \
    return false;                                                                 \
  }                                                                               \
}

// Fetch a setting, failing (returning false, i.e. no death) if it's not found.
// Note: with the registry's validation in ActivityFunction's ctors, missing
// required params should never reach this point. The macro stays as
// defense-in-depth.
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
  GET_SETTING(message);
  GET_SETTING_DEFAULT(count, "1");
  int count_val = atoi(count);
  for (int i = 0; i < count_val; i++)
    mudlog_vfprintf(ch, LOG_SYSLOG, "Effect _test_func: message='%s' (%d/%d), returning false.", message, i + 1, count_val);
  return false;
}

#undef EFFECT_FUNCTION




//// shitty little debug test function, tucked out of the way down here
void run_effect_debug_tests(struct char_data *ch) {
  // Valid params + one unknown ("not_a_param") to verify the typo-guard fires exactly once.
  Effect *effect = new Effect("_test_func", {{"message", "hello from effect test"}, {"count", "2"}, {"not_a_param", "oops"}});
  if (effect->apply(ch)) { mudlog_vfprintf(NULL, LOG_SYSLOG, "Applying debug test caused character death, RIP"); return; }
  send_to_char(ch, "Effect applied.\r\n");
  
  auto serialized = effect->serialize(2);
  send_to_char(ch, "Serialized, the effect is '%s'\r\n", serialized.c_str());
  Effect *deserialized = new Effect(serialized);
  auto reserialized = deserialized->serialize(2);
  send_to_char(ch, "Reserialization: %s\r\n", !strcmp(serialized.c_str(), reserialized.c_str()) ? "match!" : "mismatch");

  if (effect->apply(ch)) { mudlog_vfprintf(NULL, LOG_SYSLOG, "Applying reserialized debug test caused character death, RIP"); return; }
  send_to_char(ch, "Reserialized effect applied.\r\n");

  delete deserialized;
  delete effect;
}
