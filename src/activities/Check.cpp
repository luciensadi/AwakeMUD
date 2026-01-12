#include "activities/classes.hpp"

/*
Check: A boolean test that has a pass/fail state. Data includes:
- What is being tested (e.g. "has_skill", "skill_test", "has_item", "on_quest", etc)
- a parameters dict that contains further information (e.g. "on_quest" might be {"vnum": 3})
*/
class Check {
public:
  std::string type;
  std::map<std::string, std::string> parameters;

  Check(const std::string& type, const std::map<std::string, std::string>& parameters)
    : type(type), parameters(parameters) {}
  Check() = default;
  
  // True if this specific test passed for the character, false otherwise.
  bool test(struct char_data *ch);
};