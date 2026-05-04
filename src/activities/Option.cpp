#include "classes.hpp"

/*

  std::string slug; // identifies the option (for editing etc)
  std::string menu_text; // the text that shows in the selection menu to describe the option to the player

  std::vector<Check> preconditions; // checks that must pass for the player to be able to select this option
  std::vector<Check> tests; // checks that must pass for the player to receive the Pass outcome, otherwise get the Fail outcome

  Outcome pass; // an outcome describing the 'good' version of this (passed check). Printed string comes from here.
  Outcome fail; // an outcome describing the 'bad' version of this (failed check). Printed string comes from here.

*/

extern void from_json(const json& j, Outcome& e);
extern void from_json(const json& j, Check& e);
extern void to_json(json& j, const Outcome& e);
extern void to_json(json& j, const Check& e);


// Serialization function.
void to_json(json& j, const Option& e) {
  j = json{
    {"slug", e.slug}, 
    {"menu_text", e.menu_text},
    {"preconditions", e.preconditions},
    {"tests", e.tests},
    {"pass", e.pass},
    {"fail", e.fail}
  };
}
// Deserialization function.
void from_json(const json& j, Option& e) {
  // .at() is safer than [] because it throws an error if the key is missing
  j.at("slug").get_to(e.slug);
  j.at("menu_text").get_to(e.menu_text);
  j.at("preconditions").get_to(e.preconditions);
  j.at("tests").get_to(e.tests);
  j.at("pass").get_to(e.pass);
  j.at("fail").get_to(e.fail);
}
// And the wrapper to get a string out of the serialization function. Not sure I actually need this.
std::string Option::serialize() {
  json basic_info;
  to_json(basic_info, *this);
  return basic_info.dump();
}

Option::Option(const std::string serialized_json) {
  from_json(json::parse(serialized_json), *this);
}

//// shitty little debug test function, tucked out of the way down here
void run_option_debug_tests(struct char_data *ch) {
  std::vector<Check> preconditions = {Check("_test_func", {{"a", "b"}})};
  std::vector<Check> tests = {Check("_test_func", {{"a", "b"}})};
  Option *option = new Option("sluggy", "This is the menu text.", preconditions, tests, Outcome("pass_msg", {}, {}, true), Outcome("fail_msg", {}, {}, false));

  send_to_char(ch, "Option '%s' %s available to you, and you %s the tests.\r\n", option->slug.c_str(), option->is_available_to_ch(ch) ? "is" : "is not", option->test(ch) ? "passed" : "failed");

  auto serialized = option->serialize();
  send_to_char(ch, "Serialized, the option is '%s'\r\n", serialized.c_str());
  log_vfprintf("Serialized option: '''%s'''", serialized.c_str());
  Option *deserialized = new Option(serialized);
  auto reserialized = deserialized->serialize();
  send_to_char(ch, "Reserialization: %s\r\n", !strcmp(serialized.c_str(), reserialized.c_str()) ? "match!" : "mismatch");

  send_to_char(ch, "Reserialized option '%s' %s available to you, and you %s the tests.\r\n", deserialized->slug.c_str(), deserialized->is_available_to_ch(ch) ? "is" : "is not", deserialized->test(ch) ? "passed" : "failed");

  delete deserialized;
  delete option;
}