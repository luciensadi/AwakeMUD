#include "classes.hpp"
#include "../nlohmann/json.hpp"

extern void to_json(json& j, const Effect& e);
extern void from_json(const json& j, Effect& e);

// Serialization function.
void to_json(json& j, const Outcome& e) {
  j = json{
    {"message", e.message}, 
    {"effects", e.effects},
    {"situations", e.situations}
  };
}
// Deserialization function.
void from_json(const json& j, Outcome& e) {
    // .at() is safer than [] because it throws an error if the key is missing
    j.at("message").get_to(e.message);
    j.at("effects").get_to(e.effects);
    j.at("situations").get_to(e.situations);
}
// And the wrapper to get a string out of the serialization function. Not sure I actually need this.
std::string Outcome::serialize(const int indent, const char indent_char) const {
  json basic_info;
  to_json(basic_info, *this);
  return basic_info.dump(indent, indent_char);
}
Outcome::Outcome(const std::string serialized_json, bool is_pass_outcome) : is_pass_outcome(is_pass_outcome) {
  from_json(json::parse(serialized_json), *this);
}

const char *Outcome::stringify() const {
  // todo
  return nullptr;
}



//// shitty little debug test function, tucked out of the way down here
void run_outcome_debug_tests(struct char_data *ch) {
  std::vector<Effect> effectvec = {Effect("_test_func", {{"a", "b"}})};
  Outcome *outcome = new Outcome("my message", effectvec, {"slug here"}, true);

  if (outcome->apply(ch)) { mudlog_vfprintf(NULL, LOG_SYSLOG, "Applying debug outcome caused character death, RIP"); return; }
  send_to_char(ch, "Outcome applied. Here's your message: %s\r\n", outcome->message.c_str());
  
  auto serialized = outcome->serialize(2);
  send_to_char(ch, "Serialized, the outcome is '%s'\r\n", serialized.c_str());
  log_vfprintf("Serialized outcome: '''%s'''", serialized.c_str());
  Outcome *deserialized = new Outcome(serialized, true);
  auto reserialized = deserialized->serialize(2);
  send_to_char(ch, "Reserialization: %s\r\n", !strcmp(serialized.c_str(), reserialized.c_str()) ? "match!" : "mismatch");

  if (deserialized->apply(ch)) { mudlog_vfprintf(NULL, LOG_SYSLOG, "Applying reserialized debug outcome caused character death, RIP"); return; }
  send_to_char(ch, "Reserialized outcome applied.\r\n");

  delete deserialized;
  delete outcome;
}