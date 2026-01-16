#include "classes.hpp"

/*
Check: A boolean test that has a pass/fail state. Data includes:
- What is being tested (e.g. "has_skill", "skill_test", "has_item", "on_quest", etc)
- a settings dict that contains further information (e.g. "on_quest" might be {"vnum": 3})
*/

#define CHECK_FUNCTION(func_name) bool _check_function_func_name(struct char_data *ch, const std::map<std::string, std::string>& settings)

// Check function prototypes. Remember, check functions can NEVER result in character death!
CHECK_FUNCTION(test_func);

// Maps slugs to check functions. Remember, check functions can NEVER result in character death!
#define MAP_CHECK_FUNCTION(slug, func_name) {slug, (void*)&_check_function_func_name}
std::map<std::string, void *> _check_type_to_function = {
  MAP_CHECK_FUNCTION("_test_func", test_func)
};

Check::Check(const std::string& supplied_type, const std::map<std::string, std::string>& supplied_settings) 
  : type(supplied_type), settings(supplied_settings) {

  // Using our Type, select the function to run.
  auto it = _check_type_to_function.find(type);
  if (it == _check_type_to_function.end()) {
    mudlog_vfprintf(nullptr, LOG_SYSLOG, "SYSERR: Got unknown Check type '%s'.", type.c_str());
    func_ptr = nullptr;
  } else {
    func_ptr = it->second;
  }
}

// Runs the associated func_ptr and returns its value.
bool Check::test(struct char_data *ch) {
  if (!func_ptr) {
    mudlog_vfprintf(nullptr, LOG_SYSLOG, "SYSERR: Refusing to invoke Check() with null function pointer.");
    return false;
  }

  return ((bool (*)(struct char_data *, const std::map<std::string, std::string>&))func_ptr)(ch, settings);
}

///////////// Check function definitions below. Remember, check functions can NEVER result in character death!
CHECK_FUNCTION(test_func) {
  mudlog_vfprintf(ch, LOG_SYSLOG, "Check() called with _check_test_func(ch, {}x%d), returning true.", settings.size());
  return true;
};

#undef CHECK_FUNCTION