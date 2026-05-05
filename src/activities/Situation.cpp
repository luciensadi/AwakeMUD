#include "classes.hpp"

/*
  std::string slug;
public:
  std::string text; // displayed when the players enter the situation

  std::vector<Check> preconditions; // checks that must pass for the players to be able to enter the situation

  std::vector<Option> options; // the list of options they can choose from, provided they pass the option's preconditions
  // only one option is rendered as the result, everyone else's options are discarded

  std::vector<Effect> effects; // the list of effects that apply immediately to the PCs on situation start
*/

extern void to_json(json& j, const Check& e);
extern void to_json(json& j, const Effect& e);
extern void to_json(json& j, const Option& e);
extern void from_json(const json& j, Check& e);
extern void from_json(const json& j, Effect& e);
extern void from_json(const json& j, Option& e);

// Serialization function.
void to_json(json& j, const Situation& e) {
  j = json{
    {"slug", e.slug},
    {"text", e.text},
    {"preconditions", e.preconditions},
    {"options", e.options},
    {"effects", e.effects}
  };
}
// Deserialization function.
void from_json(const json& j, Situation& e) {
    // .at() is safer than [] because it throws an error if the key is missing
    j.at("slug").get_to(e.slug);
    j.at("text").get_to(e.text);
    j.at("preconditions").get_to(e.preconditions);
    j.at("options").get_to(e.options);
    j.at("effects").get_to(e.effects);
}
// And the wrapper to get a string out of the serialization function. Not sure I actually need this.
std::string Situation::serialize(const int indent, const char indent_char) const {
  json basic_info;
  to_json(basic_info, *this);
  return basic_info.dump(indent, indent_char);
}
Situation::Situation(const std::string serialized_json) {
  from_json(json::parse(serialized_json), *this);
}

std::vector<Option *> Situation::get_options_for_ch(struct char_data *ch) {
  std::vector<Option *> available_options = {};
  for (auto &option : options) {
    if (option.is_available_to_ch(ch))
      available_options.push_back(&option);
  }
  return available_options;
}


//// shitty little debug test function, tucked out of the way down here
void run_situation_debug_tests(struct char_data *ch) {
  std::vector<Check> opt_preconditions = {Check("_test_func", {{"a", "b"}})};
  std::vector<Check> opt_tests = {Check("_test_func", {{"c", "d"}})};
  std::vector<Check> opt_false_tests = {Check("always_false", {})};

  std::vector<Check> checkvec = {Check("_test_func", {{"g", "h"}})};
  std::vector<Effect> effectvec = {Effect("_test_func", {{"i", "j"}})};
  std::vector<Option> optionvec = {
    Option("sluggy", "This is the menu text.", opt_preconditions, opt_tests, Outcome("pass_msg", {}, {}, true), Outcome("fail_msg", {}, {}, false)),
    Option("alwaysfalse", "This is the false menu text.", opt_preconditions, opt_false_tests, Outcome("pass_msg", {}, {}, true), Outcome("fail_msg", {}, {}, false))
  };

  Situation *situation = new Situation(
    "sitslug",
    "This is the text of the situation! It's got special characters.",
    checkvec,
    optionvec,
    effectvec
  );

  auto opts = situation->get_options_for_ch(ch);
  if (opts.size() != 1) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Size of get_options_for_ch() from expected-1 is actually %ld.", opts.size());
  }
  if (!situation->meets_preconditions(ch)) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Situation preconditions should have been true :(");
  }
  if (situation->apply(ch)) { mudlog_vfprintf(NULL, LOG_SYSLOG, "Applying debug situation caused character death, RIP"); return; }
  send_to_char(ch, "Situation:\r\n%s\r\n\r\nOptions:\r\n", situation->text.c_str());
  for (auto option : opts) {
    send_to_char(ch, " - %s\r\n", option->menu_text.c_str());
  }

  auto serialized = situation->serialize(2);
  send_to_char(ch, "Serialized, the situation is: ```\r\n%s\r\n```\r\n", serialized.c_str());
  log_vfprintf("Serialized outcome: ```%s```", serialized.c_str());
  Situation *deserialized = new Situation(serialized);
  auto reserialized = deserialized->serialize(2);
  send_to_char(ch, "Reserialization: %s\r\n", !strcmp(serialized.c_str(), reserialized.c_str()) ? "match!" : "mismatch");

  opts = deserialized->get_options_for_ch(ch);
  if (opts.size() != 1) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Size of get_options_for_ch() from expected-1 is actually %ld.", opts.size());
  }
  if (!deserialized->meets_preconditions(ch)) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Situation preconditions should have been true :(");
  }
  if (deserialized->apply(ch)) { mudlog_vfprintf(NULL, LOG_SYSLOG, "Applying debug situation caused character death, RIP"); return; }
  send_to_char(ch, "Situation:\r\n%s\r\n\r\nOptions:\r\n", deserialized->text.c_str());
  for (auto option : opts) {
    send_to_char(ch, " - %s\r\n", option->menu_text.c_str());
  }

  delete deserialized;
  delete situation;
}