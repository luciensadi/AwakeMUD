#include "classes.hpp"
#include "../db.hpp"

/*
ActivityFunction: Base class for Check and Effect.
Handles the shared plumbing: function pointer lookup, settings storage,
serialization/deserialization, and dispatch.
*/

using json = nlohmann::json;

void ActivityFunction::resolve_ptr(const std::map<std::string, void*>& func_map) {
  auto it = func_map.find(func_name);
  if (it != func_map.end()) {
    func_ptr = it->second;
  } else {
    // Log error or set to a fallback if necessary
    mudlog_vfprintf(nullptr, LOG_SYSLOG, "SYSERR: Got unknown ActivityFunction type '%s'.", func_name.c_str());
    func_ptr = nullptr; 
  }
}

// Regular constructor: store func_name and settings, look up func_ptr in the supplied map.
ActivityFunction::ActivityFunction(const std::string& type,
                                   const std::map<std::string, std::string>& supplied_settings,
                                   const std::map<std::string, void*>& func_map)
  : func_name(type), settings(supplied_settings) {
  resolve_ptr(func_map);
}

// Deserializing constructor: parse JSON, set func_name and settings, look up func_ptr.
// Falls back to fallback_func_ptr if the func name is not found in the map.
ActivityFunction::ActivityFunction(const std::string& serialized,
                                   const std::map<std::string, void*>& func_map,
                                   void* fallback_func_ptr) {
  auto parsed_json = json::parse(serialized);

  if (parsed_json.contains("func")) {
    func_name = parsed_json["func"].get<std::string>();
    auto it = func_map.find(func_name);
    if (it != func_map.end()) {
      func_ptr = it->second;
    } else {
      mudlog_vfprintf(NULL, LOG_SYSLOG,
          "SYSERR: Invalid func name '%s' loaded from serialization. Using always-false fallback.",
          func_name.c_str());
      func_ptr = fallback_func_ptr;
    }
  }

  if (parsed_json.contains("settings")) {
    settings = parsed_json["settings"].get<std::map<std::string, std::string>>();
  }
}

// Dispatch to func_ptr. Returns false (with a SYSERR log) if func_ptr is null.
bool ActivityFunction::invoke(struct char_data *ch) {
  if (!func_ptr) {
    mudlog_vfprintf(nullptr, LOG_SYSLOG, "SYSERR: Refusing to invoke ActivityFunction '%s' with null function pointer.", func_name.c_str());
    return false;
  }
  return ((bool (*)(struct char_data *, const std::map<std::string, std::string>&))func_ptr)(ch, settings);
}
