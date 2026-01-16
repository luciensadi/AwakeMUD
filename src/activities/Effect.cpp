#include "classes.hpp"

/*
Effect: A side effect that, when encountered, modifies the party in some way. Data includes:
- the type of effect (from an enum, e.g. "lose_nuyen", "phys_damage", "quest_progress")
- a parameters dict that contains further information (e.g. "lose_nuyen"'s parameters might be {"amount": 50})
*/

#define EFFECT_FUNCTION(func_name) bool func_name(struct char_data *ch, const std::map<std::string, std::string>& settings)

// Effect function prototypes. Remember, effect functions return TRUE on death, FALSE otherwise!
EFFECT_FUNCTION(_effect_test_func);

// Maps slugs to effect functions. Remember, effect functions return TRUE on death, FALSE otherwise!
std::map<std::string, void *> _effect_type_to_function = {
  {"_test_func", (void*)&_effect_test_func}
};

Effect::Effect(const std::string& supplied_type, const std::map<std::string, std::string>& supplied_settings) 
  : type(supplied_type), settings(supplied_settings) {

  // Using our Type, select the function to run.
  auto it = _effect_type_to_function.find(type);
  if (it == _effect_type_to_function.end()) {
    mudlog_vfprintf(nullptr, LOG_SYSLOG, "SYSERR: Got unknown Effect type '%s'.", type.c_str());
    func_ptr = nullptr;
  } else {
    func_ptr = it->second;
  }
}

// Runs the associated func_ptr and returns its value.
bool Effect::apply(struct char_data *ch) {
  if (!func_ptr) {
    mudlog_vfprintf(nullptr, LOG_SYSLOG, "SYSERR: Refusing to invoke Effect() with null function pointer.");
    return false;
  }

  return ((bool (*)(struct char_data *, const std::map<std::string, std::string>&))func_ptr)(ch, settings);
}

///////////// Effect function definitions below. Remember, effect functions return TRUE on death, FALSE otherwise!
EFFECT_FUNCTION(_effect_test_func) {
  mudlog_vfprintf(ch, LOG_SYSLOG, "Effect() called with _effect_test_func(ch, {}x%d), returning false.", settings.size());
  return false;
};

#undef EFFECT_FUNCTION