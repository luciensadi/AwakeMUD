#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
namespace bf = boost::filesystem;

#include "classes.hpp"

/*
Activity: A digraph containing one or more Situations as well as metadata about the activity itself. Data includes:
- authorship information (who, when, etc)
- identifying information (slug, display name)
- preconditions / requirements for it to be encountered
- dict of all possible situations in the activity, keyed by situation slug
- list of potential starting situation slugs to put the players in
*/

/*
  std::string slug = "";
  std::string displayName = "";
  idnum_t author = 0;
  time_t createdAt = time(0);
  time_t updatedAt = time(0);

  std::vector<Check> preconditions = {};

  std::map<std::string, Situation> situations = {};

  std::vector<std::string> starting_situations = {};
  */

extern void to_json(json& j, const Check& e);
extern void to_json(json& j, const Situation& e);
extern void from_json(const json& j, Check& e);
extern void from_json(const json& j, Situation& e);

// Serialization function.
void to_json(json& j, const Activity& e) {
  j = json{
    {"slug", e.slug}, 
    {"display_name", e.display_name},
    {"summary", e.summary},
    {"author", e.author},
    {"createdAt", e.createdAt},
    {"updatedAt", e.updatedAt},
    {"preconditions", e.preconditions},
    {"situations", e.situations},
    {"starting_situations", e.starting_situations},
    {"flags", e.flags}
  };
}
// Deserialization function.
void from_json(const json& j, Activity& e) {
  // .at() is safer than [] because it throws an error if the key is missing
  j.at("slug").get_to(e.slug);
  j.at("display_name").get_to(e.display_name);
  j.at("summary").get_to(e.summary);
  j.at("author").get_to(e.author);
  j.at("createdAt").get_to(e.createdAt);
  j.at("updatedAt").get_to(e.updatedAt);
  j.at("preconditions").get_to(e.preconditions);
  j.at("situations").get_to(e.situations);
  j.at("starting_situations").get_to(e.starting_situations);
  j.at("flags").get_to(e.flags);
}
// And the wrapper to get a string out of the serialization function. Not sure I actually need this.
std::string Activity::serialize(const int indent, const char indent_char) const {
  json basic_info;
  to_json(basic_info, *this);
  return basic_info.dump(indent, indent_char);
}
Activity::Activity(const std::string serialized_json) {
  from_json(json::parse(serialized_json), *this);
}

Activity::Activity(bf::path path_to_file) {
  // We've got this real handy function from newhouse.cpp to crib:
  extern void _json_parse_from_file(bf::path path, json &target);

  json basic_info;
  _json_parse_from_file(path_to_file, basic_info);
  from_json(basic_info, *this);
}

void Activity::save_to_disk() {
  // Saves to `lib/activities/slug`
  bf::path base_path = BASE_ACTIVITY_PATH;
  bf::create_directories(base_path);

  json basic_info;
  to_json(basic_info, *this);

  bf::ofstream ofs(base_path / slug);
  ofs << std::setw(4) << basic_info << std::endl;
  ofs.close();
}

Situation* Activity::_get_situation_from_list_of_slugs(const std::vector<std::string> &slugs, struct char_data *ch, bool allow_fallback_to_invalid) {
  if (slugs.size() <= 0) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: %s got empty list of slugs to select from.", __func__);
    return nullptr;
  }

  std::vector <Situation *> qualifying_situations = {};

  for (auto sitslug : slugs) {
    Situation* situation = lookup_situation(sitslug);
    if (situation && situation->meets_preconditions(ch))
      qualifying_situations.push_back(situation);
  }

  if (qualifying_situations.size() > 0) {
    return qualifying_situations[rand() % qualifying_situations.size()];
  }

  if (allow_fallback_to_invalid) {
    return lookup_situation(slugs[rand() % slugs.size()]);
  }

  return nullptr;
}

// Gets a starting situation that ch passes preconditions for, or nullptr if no such situation exists.
Situation* Activity::get_starting_situation(struct char_data *ch) {
  return _get_situation_from_list_of_slugs(starting_situations, ch, false);
}

// Gets the next valid situation from an outcome's list for the given character. Difference between this and the above is this is guaranteed to return a situation as long as one exists.
Situation* Activity::get_next_situation_from_outcome(const Outcome *outcome, struct char_data *ch) {
  return _get_situation_from_list_of_slugs(outcome->situations, ch, true);
}


//// shitty little debug test function, tucked out of the way down here
void run_activity_debug_tests(struct char_data *ch) {
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

  /*
  std::string slug, std::string display_name, idnum_t author, time_t createdAt, time_t updatedAt, std::vector<Check> preconditions, std::map<std::string, Situation> situations,
           std::vector<std::string> starting_situations, std::map<std::string, std::string> flags
           */
  Activity *activity = new Activity(
    "activity_slug",
    "A Romp in the Woods",
    1,
    time(0),
    time(0),
    {Check("_test_func", {{"a", "b"}})}, // always-true precondition
    {{
      "sitslug",
      Situation(
        "sitslug",
        "This is the text of the situation! It's got special characters.",
        checkvec,
        optionvec,
        effectvec
      )
    }},
    {"sitslug"},
    {}
  );

  if (!activity->meets_preconditions(ch)) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Activity preconditions should have been true :(");
  }
  
  auto serialized = activity->serialize(2);
  send_to_char(ch, "Serialized, the activity is: ```\r\n%s\r\n```\r\n", serialized.c_str());
  log_vfprintf("Serialized outcome: ```%s```", serialized.c_str());
  Activity *deserialized = new Activity(serialized);
  auto reserialized = deserialized->serialize(2);
  send_to_char(ch, "Reserialization: %s\r\n", !strcmp(serialized.c_str(), reserialized.c_str()) ? "match!" : "mismatch");

  if (!deserialized->meets_preconditions(ch)) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Reserialized activity preconditions should have been true :(");
  }

  delete deserialized;
  delete situation;
}