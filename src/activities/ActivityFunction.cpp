#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "classes.hpp"
#include "../db.hpp"
#include "../utils.hpp"
#include "nlohmann/json.hpp"

/*
ActivityFunction: Base class for Check and Effect.
Handles the shared plumbing: function pointer lookup, settings storage,
serialization/deserialization, and dispatch.
*/

using json = nlohmann::json;

/////////////////// Param-type validation //////////////////////////////////////

const char *activity_param_type_name(ActivityParamType type) {
  switch (type) {
    case ActivityParamType::STRING:         return "string";
    case ActivityParamType::INTEGER:        return "integer";
    case ActivityParamType::INTEGER_MAX_12: return "integer (max 12)";
    case ActivityParamType::BOOLEAN:        return "boolean";
    case ActivityParamType::SKILL_IDX:      return "skill idx";
    case ActivityParamType::SPELL_IDX:      return "spell idx";
    case ActivityParamType::POWER_IDX:      return "power idx";
    case ActivityParamType::OBJ_VNUM:       return "object vnum";
    case ActivityParamType::MOB_VNUM:       return "mob vnum";
    case ActivityParamType::ROOM_VNUM:      return "room vnum";
    case ActivityParamType::QUEST_VNUM:     return "quest vnum";
    case ActivityParamType::NUYEN_AMOUNT:   return "nuyen amount";
    case ActivityParamType::KARMA_AMOUNT:   return "karma amount";
  }
  return "unknown";
}

// Returns true if `value` parses cleanly as a base-10 signed integer.
// Trailing/leading whitespace and a single leading +/- are allowed.
static bool parse_clean_long(const std::string& value, long& out) {
  if (value.empty()) return false;
  const char *c_str = value.c_str();
  char *end = nullptr;
  errno = 0;
  long parsed = strtol(c_str, &end, 10);
  if (errno != 0 || end == c_str) return false;
  // Allow trailing whitespace only.
  while (*end) {
    if (!isspace(static_cast<unsigned char>(*end))) return false;
    end++;
  }
  out = parsed;
  return true;
}

static bool parse_clean_double(const std::string& value, double& out) {
  if (value.empty()) return false;
  const char *c_str = value.c_str();
  char *end = nullptr;
  errno = 0;
  double parsed = strtod(c_str, &end);
  if (errno != 0 || end == c_str) return false;
  while (*end) {
    if (!isspace(static_cast<unsigned char>(*end))) return false;
    end++;
  }
  out = parsed;
  return true;
}

bool validate_activity_param_value(ActivityParamType type,
                                   const std::string& value,
                                   std::string& err_out) {
  err_out.clear();

  switch (type) {
    case ActivityParamType::STRING:
      if (value.empty()) {
        err_out = "value must not be empty";
        return false;
      }
      return true;

    case ActivityParamType::INTEGER:
    case ActivityParamType::INTEGER_MAX_12:
    {
      long parsed = 0;
      if (!parse_clean_long(value, parsed)) {
        err_out = "must be a whole number (e.g. 5, -3)";
        return false;
      }
      if (type == ActivityParamType::INTEGER_MAX_12 && (parsed < 0 || parsed > 12)) {
        err_out = "must be in range [0,12]";
        return false;
      }
      return true;
    }

    case ActivityParamType::BOOLEAN: {
      std::string lower = value;
      for (auto& c : lower) c = tolower(static_cast<unsigned char>(c));
      if (lower == "true" || lower == "false" || lower == "1" || lower == "0" ||
          lower == "yes"  || lower == "no") return true;
      err_out = "must be true/false, yes/no, or 1/0";
      return false;
    }

    case ActivityParamType::SKILL_IDX:
      if (atoi(value.c_str()) < 0 || atoi(value.c_str()) >= MAX_SKILLS) {
        err_out = "not a recognized skill idx";
        return false;
      }
      return true;

    case ActivityParamType::SPELL_IDX:
      if (atoi(value.c_str()) < 0 || atoi(value.c_str()) >= MAX_SPELLS) {
        err_out = "not a recognized spell idx";
        return false;
      }
      return true;

    case ActivityParamType::POWER_IDX:
      if (atoi(value.c_str()) < 0 || atoi(value.c_str()) >= ADEPT_NUMPOWER) {
        err_out = "not a recognized adept power idx";
        return false;
      }
      return true;

    case ActivityParamType::OBJ_VNUM: {
      long parsed = 0;
      if (!parse_clean_long(value, parsed)) { err_out = "must be a whole-number vnum"; return false; }
      if (real_object(parsed) < 0) { err_out = "no object exists with that vnum"; return false; }
      return true;
    }

    case ActivityParamType::MOB_VNUM: {
      long parsed = 0;
      if (!parse_clean_long(value, parsed)) { err_out = "must be a whole-number vnum"; return false; }
      if (real_mobile(parsed) < 0) { err_out = "no mobile exists with that vnum"; return false; }
      return true;
    }

    case ActivityParamType::ROOM_VNUM: {
      long parsed = 0;
      if (!parse_clean_long(value, parsed)) { err_out = "must be a whole-number vnum"; return false; }
      if (real_room(parsed) < 0) { err_out = "no room exists with that vnum"; return false; }
      return true;
    }

    case ActivityParamType::QUEST_VNUM: {
      long parsed = 0;
      if (!parse_clean_long(value, parsed)) { err_out = "must be a whole-number vnum"; return false; }
      if (real_quest(parsed) < 0) { err_out = "no quest exists with that vnum"; return false; }
      return true;
    }

    case ActivityParamType::NUYEN_AMOUNT: {
      long parsed = 0;
      if (!parse_clean_long(value, parsed)) { err_out = "must be a whole number of nuyen"; return false; }
      if (parsed < 0) { err_out = "nuyen amount must be zero or positive"; return false; }
      return true;
    }

    case ActivityParamType::KARMA_AMOUNT: {
      double parsed = 0.0;
      if (!parse_clean_double(value, parsed)) { err_out = "must be a number (e.g. 1.5)"; return false; }
      if (parsed < 0.0) { err_out = "karma amount must be zero or positive"; return false; }
      return true;
    }
  }

  err_out = "unknown parameter type";
  return false;
}

/////////////////// Settings validation /////////////////////////////////////////

// Validates a settings map against a spec. Logs SYSERRs for each issue found.
// Returns false if any required-param-missing problem occurred (the caller
// should then fall back to a safe no-op pointer); unknown-key and bad-type
// problems are logged but do not by themselves trigger fallback, since the
// activity is still mostly recoverable.
//
// `context_name` is used in log messages so builders can locate the offender.
static bool validate_settings_against_spec(const std::string& context_name,
                                           const ActivityFuncSpec& spec,
                                           const std::map<std::string, std::string>& settings) {
  bool required_ok = true;

  // Check 1: every required param is present, and present values are well-typed.
  for (const auto& param : spec.params) {
    auto it = settings.find(param.name);
    if (it == settings.end()) {
      if (param.required) {
        mudlog_vfprintf(nullptr, LOG_SYSLOG,
            "SYSERR: ActivityFunction '%s' is missing required param '%s' (%s).",
            context_name.c_str(), param.name.c_str(),
            activity_param_type_name(param.type));
        required_ok = false;
      }
      continue;
    }
    std::string err;
    if (!validate_activity_param_value(param.type, it->second, err)) {
      mudlog_vfprintf(nullptr, LOG_SYSLOG,
          "SYSERR: ActivityFunction '%s' param '%s' has invalid value '%s': %s.",
          context_name.c_str(), param.name.c_str(), it->second.c_str(), err.c_str());
      // Bad value is a soft error; we keep going so the activity can still
      // partially load. The runtime func will hit its own GET_SETTING checks.
    }
  }

  // Check 2: warn about settings keys not declared in the spec (typo guard).
  for (const auto& kv : settings) {
    bool declared = false;
    for (const auto& param : spec.params) {
      if (param.name == kv.first) { declared = true; break; }
    }
    if (!declared) {
      mudlog_vfprintf(nullptr, LOG_SYSLOG,
          "SYSERR: ActivityFunction '%s' has unknown setting '%s' = '%s' (typo? deprecated?). Ignored.",
          context_name.c_str(), kv.first.c_str(), kv.second.c_str());
    }
  }

  return required_ok;
}

/////////////////// Constructors / dispatch /////////////////////////////////////

void ActivityFunction::resolve_ptr(const std::map<std::string, ActivityFuncSpec>& registry) {
  auto it = registry.find(func_name);
  if (it != registry.end()) {
    func_ptr = it->second.func_ptr;
  } else {
    mudlog_vfprintf(nullptr, LOG_SYSLOG,
        "SYSERR: Got unknown ActivityFunction type '%s'.", func_name.c_str());
    func_ptr = nullptr;
  }
}

// Regular constructor: store func_name and settings, look up spec, validate.
ActivityFunction::ActivityFunction(const std::string& type,
                                   const std::map<std::string, std::string>& supplied_settings,
                                   const std::map<std::string, ActivityFuncSpec>& registry,
                                   ActivityFuncPtr fallback_func_ptr)
  : func_name(type), settings(supplied_settings) {
  auto it = registry.find(func_name);
  if (it == registry.end()) {
    mudlog_vfprintf(nullptr, LOG_SYSLOG,
        "SYSERR: ActivityFunction constructed with unknown type '%s'. Using fallback.",
        func_name.c_str());
    func_ptr = fallback_func_ptr;
    return;
  }

  if (!validate_settings_against_spec(func_name, it->second, settings)) {
    func_ptr = fallback_func_ptr;
    return;
  }

  func_ptr = it->second.func_ptr;
}

// Deserializing constructor: parse JSON, set func_name and settings, validate.
ActivityFunction::ActivityFunction(const std::string& serialized,
                                   const std::map<std::string, ActivityFuncSpec>& registry,
                                   ActivityFuncPtr fallback_func_ptr) {
  auto parsed_json = json::parse(serialized);

  if (parsed_json.contains("func")) {
    func_name = parsed_json["func"].get<std::string>();
  }
  if (parsed_json.contains("settings")) {
    settings = parsed_json["settings"].get<std::map<std::string, std::string>>();
  }

  auto it = registry.find(func_name);
  if (it == registry.end()) {
    mudlog_vfprintf(nullptr, LOG_SYSLOG,
        "SYSERR: Invalid func name '%s' loaded from serialization. Using fallback.",
        func_name.c_str());
    func_ptr = fallback_func_ptr;
    return;
  }

  if (!validate_settings_against_spec(func_name, it->second, settings)) {
    // Required param missing; fall back to safe no-op so the world still loads.
    func_ptr = fallback_func_ptr;
    return;
  }

  func_ptr = it->second.func_ptr;
}

// Dispatch to func_ptr. Returns false (with a SYSERR log) if func_ptr is null.
bool ActivityFunction::invoke(struct char_data *ch) {
  if (!func_ptr) {
    mudlog_vfprintf(nullptr, LOG_SYSLOG, "SYSERR: Refusing to invoke ActivityFunction '%s' with null function pointer.", func_name.c_str());
    return false;
  }
  return func_ptr(ch, settings);
}
