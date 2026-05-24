#include "MenuFrameGenerics.hpp"
#include "interpreter.hpp"
#include "constants.hpp"

#define AS_BOOL(thing)   std::variant<int, float, bool, std::string>{std::in_place_type<bool>, thing}
#define AS_INT(thing)    std::variant<int, float, bool, std::string>{std::in_place_type<int>, thing}
#define AS_FLOAT(thing)  std::variant<int, float, bool, std::string>{std::in_place_type<float>, thing}
#define AS_STRING(thing) std::variant<int, float, bool, std::string>{std::in_place_type<std::string>, thing}

MenuFrameResult YesNoPromptFrame::parse(struct descriptor_data *d, char *arg) {
  skip_spaces(&arg);

  MF_TRYAGAIN_CASE(!*arg || (*arg != 'y' && *arg != 'n'), "Invalid input. Please enter 'y' or 'n'.");
  
  bool value = (*arg == 'y');

  if (on_choice_) {
    on_choice_(value);
  }
  return { MenuFrameAction::Pop, child_identifier, AS_BOOL(value) };
}

MenuFrameResult IntPromptFrame::parse(struct descriptor_data *d, char *arg) {
  skip_spaces(&arg);
  int value = atoi(arg);

  MF_TRYAGAIN_CASE_PRINTF(!*arg || value < min_val || value > max_val, "Invalid input: must be a number between %d and %d (inclusive).", min_val, max_val);
  
  if (on_choice_) {
    on_choice_(value);
  }
  return { MenuFrameAction::Pop, child_identifier, AS_BOOL(value) };
}

MenuFrameResult FloatPromptFrame::parse(struct descriptor_data *d, char *arg) {
  skip_spaces(&arg);
  float value = atof(arg);

  MF_TRYAGAIN_CASE_PRINTF(!*arg 
                          || (value < min_val && !FLOATS_ARE_EQUAL_ISH(value, min_val))
                          || (value > max_val && !FLOATS_ARE_EQUAL_ISH(value, max_val)),
                          "Invalid input: must be a number between %.2f and %.2f (inclusive).", min_val, max_val);
  
  if (on_choice_) {
    on_choice_(value);
  }
  return { MenuFrameAction::Pop, child_identifier, AS_BOOL(value) };
}

MenuFrameResult StringPromptFrame::parse(struct descriptor_data *d, char *arg) {
  skip_spaces(&arg);

  MF_TRYAGAIN_CASE_PRINTF(!*arg || strlen(arg) < min_length, "Invalid input: Must be a string at least %lu characters long.", min_length);
  MF_TRYAGAIN_CASE_PRINTF(strlen(arg) > max_length_with_color, "Invalid input: Cannot exceed %lu characters in length (including color codes).", max_length_with_color);

  int size_after_removal = get_string_length_after_color_code_removal(arg, nullptr);
  MF_TRYAGAIN_CASE_PRINTF(size_after_removal < 0 || size_after_removal > max_length_after_color_removal,
                          "Invalid input: Cannot exceed %lu characters in length (excluding color codes).", max_length_after_color_removal);
  
  if (on_choice_) {
    on_choice_(std::string(arg));
  }
  return { MenuFrameAction::Pop, child_identifier, AS_STRING(std::string(arg)) };
}

//////////////////////////////////////////////////////////////////////
// Less-generic generics below here (skill, spell, power, etc inputs)
//////////////////////////////////////////////////////////////////////

#define SYNTAX "Invalid input: Must be a name of a skill (e.g. Pistols), or 'abort' to cancel.\r\n"
MenuFrameResult SkillNamePromptFrame::parse(struct descriptor_data *d, char *arg) {
  skip_spaces(&arg);

  MF_TRYAGAIN_CASE(!*arg, SYNTAX);

  if (!str_cmp(arg, "abort")) {
    // Don't call on_choice_, just bail.
    return { MenuFrameAction::Pop, child_identifier, AS_INT(-1) };
  }

  int result = skill_name_to_idx(arg, true);

  if (result == -1) {
    if (d->character) {
      send_to_char(d->character, SYNTAX);
    }
    return { MenuFrameAction::JustDisplay };
  }

  if (on_choice_) {
    on_choice_(result);
  }
  return { MenuFrameAction::Pop, child_identifier, AS_INT(result) };  
}
#undef SYNTAX

#define SYNTAX "Invalid input: Must be a name of a spell (e.g. Manabolt), or 'abort' to cancel.\r\n"
MenuFrameResult SpellNamePromptFrame::parse(struct descriptor_data *d, char *arg) {
  skip_spaces(&arg);

  MF_TRYAGAIN_CASE(!*arg, SYNTAX);

  if (!str_cmp(arg, "abort")) {
    // Don't call on_choice_, just bail.
    return { MenuFrameAction::Pop, child_identifier, AS_INT(-1) };
  }

  int result = spell_name_to_idx(arg, true);

  if (result == -1) {
    if (d->character) {
      send_to_char(d->character, SYNTAX);
    }
    return { MenuFrameAction::JustDisplay };
  }

  if (on_choice_) {
    on_choice_(result);
  }
  return { MenuFrameAction::Pop, child_identifier, AS_INT(result) };  
}
#undef SYNTAX

#define SYNTAX "Invalid input: Must be a name of an adept power (e.g. Kinesics), or 'abort' to cancel.\r\n"
MenuFrameResult PowerNamePromptFrame::parse(struct descriptor_data *d, char *arg) {
  skip_spaces(&arg);

  MF_TRYAGAIN_CASE(!*arg, SYNTAX);

  if (!str_cmp(arg, "abort")) {
    // Don't call on_choice_, just bail.
    return { MenuFrameAction::Pop, child_identifier, AS_INT(-1) };
  }

  int result = power_name_to_idx(arg, true);

  if (result == -1) {
    if (d->character) {
      send_to_char(d->character, SYNTAX);
    }
    return { MenuFrameAction::JustDisplay };
  }

  if (on_choice_) {
    on_choice_(result);
  }
  return { MenuFrameAction::Pop, child_identifier, AS_INT(result) };  
}
#undef SYNTAX