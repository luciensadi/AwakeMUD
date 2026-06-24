#include <ranges>
#include <algorithm>

#include "classes.hpp"
#include "activity_olc.hpp"
#include "nlohmann/json.hpp"

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
Outcome::Outcome(const std::string serialized_json) {
  from_json(json::parse(serialized_json), *this);
}

const std::string Outcome::stringify() const {
  // todo
  return std::string("stringify not implemented on outcome");
}

////////////////////////////////////////
//// OLC
////////////////////////////////////////

/*
  Need to have:
  - main outcome editing (display the whole thing and allow you to set all class fields)
  - sub-menus for each thing
*/

// Main menu

void OutcomeMenuFrame::display(struct descriptor_data *d) const {
  send_to_char(CH, "1) Message:  Assisted by X and Y, Lucien ^c%s^n\r\n", OUT->get_message());

  send_to_char(CH, "2) Side Effects to Apply:  ");
  if (OUT->effects.empty()) {
    send_to_char("(not set, so no effects beyond situation change)\r\n", CH);
  } else {
    for (auto [index, effect] : std::views::zip(std::views::iota(0), OUT->effects)) {  // should just use enumerate() but not supported on my compiler yet
      send_to_char(CH, "%s%s", index > 0 ? ", " : "", effect.func_name.c_str());
    }
    send_to_char("\r\n", CH);
  }

  send_to_char(CH, "3) Next Situation(s):  ");
  if (OUT->situations.empty()) {
    send_to_char("(not set, so this outcome ends the activity)\r\n", CH);
  } else {
    for (auto [index, situation] : std::views::zip(std::views::iota(0), OUT->situations)) {
      send_to_char(CH, "%s%s", index > 0 ? ", " : "", situation.c_str());
    }
    send_to_char("\r\n", CH);
  }
  
  send_to_char(CH, "\r\n"
                   "q) Keep changes\r\n"
                   "x) Discard changes\r\n"
                   "\r\n"
                   "Enter your choice: ");
}

MenuFrameResult OutcomeMenuFrame::parse(struct descriptor_data *d, char *arg) {
  switch (LOWER(*arg)) {
    case '1':
      MF_PROMPT_STRING_AND_RETURN(
        "Messages are displayed in the format 'Assisted by X and Y, Lucien <YOUR MESSAGE HERE>'.\r\nWrite your message fragment including terminal punctuation: ",
        0,
        [d](std::string r){OUT->message = r;}
      );
    case '2':
      // Cache the current effects list (serialized to json) to allow 'x' behavior, then kick down into effects menu.
      d->edit_effects_original.clear();
      for (auto effect : OUT->effects) {
        d->edit_effects_original.push_back(effect.serialize());
      }
      MF_PUSH_FRAME(OutcomeEffectsFrame, 2);
    case '3':
      // Cache to allow 'x'
      d->edit_slugs_original = OUT->situations;
      MF_PUSH_FRAME(OutcomeSituationsFrame, 3);
    case 'x':
      delete OUT;
      OUT = d->edit_outcome_original;
      // fall through
    case 'q':
      return { MenuFrameAction::Pop, child_identifier };
  }

  MF_PARSE_FAILED("That's not an option. Select 1-3, or 'q' to save and go back, or 'x' to go back without saving.");
}

const MenuFrameResult OutcomeMenuFrame::handle_child_response(struct descriptor_data *d, const MenuFrameResult & result) { return { MenuFrameAction::JustDisplay }; }

// Effect editing

void OutcomeEffectsFrame::display(struct descriptor_data *d) const {
  send_to_char("Side effects are applied to characters who experience an Outcome, "
               "e.g. solving a Situation by punching your way through a barbed-wire fence might deal a box of physical damage.\r\n\r\n", CH);

  if (OUT->effects.empty()) {
    send_to_char("- No side effects are currently set.\r\n", CH);
  } else {
    for (auto [index, effect] : std::views::zip(std::views::iota(0), OUT->effects)) {
      send_to_char(CH, "%2d) %s\r\n", index + 1, effect.stringify().c_str());
    }
  }

  send_to_char("\r\n"
               "c ) Create a new side effect\r\n", CH);
  
  if (!OUT->effects.empty()) {
    send_to_char("d ) Delete an effect from the list\r\n", CH);
  }

  send_to_char("\r\n"
               "q ) Save and return to menu\r\n"
               "x ) Discard changes and return\r\n", CH);
}

#define _OUTCOME_EFFECTS_FRAME__ADD_EFFECT 0
#define _OUTCOME_EFFECTS_FRAME__EDIT_EXISTING_EFFECT 1
MenuFrameResult OutcomeEffectsFrame::parse(struct descriptor_data *d, char *arg) {
  switch(LOWER(*arg)) {
    case 'a':
    case 'c':
      delete d->edit_effect;
      d->edit_effect = new Effect();
      delete d->edit_effect_original;
      d->edit_effect_original = nullptr;
      send_to_char(CH, "^RThis is where you'd dip down into the effect edit OLC system if it existed (new effect). Try a different option.^n\r\n");
      // TODO: MF_PUSH_FRAME(EffectMenuFrame, _OUTCOME_EFFECTS_FRAME__ADD_EFFECT);
      return { MenuFrameAction::DoNothing };
    case 'd':
      MF_TRYAGAIN_CASE(OUT->effects.empty(), "There's nothing to delete.");
      MF_PROMPT_INT_AND_RETURN("Which effect index do you want to delete? (0 to abort)", 86, [d](int r){
        if ((--r) < 0) return; // resolve their 1-indexed entry to 0-indexed, if they put 0 then bail
        OUT->effects.erase(OUT->effects.begin() + r);
      }, 0, OUT->effects.size() + 1); // accept +1 since they have a 1-indexed list
    case 'x':
      // Wipe the effects and rehydrate the list from the jsonified/serialized saved copy
      OUT->effects.clear();
      for (auto json_effect : d->edit_effects_original) {
        OUT->effects.emplace_back(json_effect);
      }
      // fall through
    case 'q':
      return { MenuFrameAction::Pop, child_identifier };
  }

  size_t idx = atoi(arg) - 1; // -1 since we give them a 1-indexed list
  if (idx >= 0 && idx < OUT->effects.size()) {
    d->edit_effect = &(OUT->effects.at(idx));
    d->edit_effect_original = new Effect(d->edit_effect->serialize());
    send_to_char(CH, "^RThis is where you'd dip down into the effect edit OLC system if it existed (edit existing). Try a different option.^n\r\n");
    // TODO: MF_PUSH_FRAME(EffectMenuFrame, _OUTCOME_EFFECTS_FRAME__EDIT_EXISTING_EFFECT);
    return { MenuFrameAction::DoNothing };
  }

  MF_PARSE_FAILED("That's not an option. Select an effect index to edit, or 'C' to create, 'D' to delete, 'Q' to save, 'X' to abort.");
}

const MenuFrameResult OutcomeEffectsFrame::handle_child_response(struct descriptor_data *d, const MenuFrameResult & result) { return { MenuFrameAction::JustDisplay }; }


// Sitation editing. Displays a list of situations IN the outcome, then lets them Add to or Delete from that list.
void OutcomeSituationsFrame::display(struct descriptor_data *d) const {
  // List situations available (index them), then allow adding / deleting.
  send_to_char("Situation slugs \r\n\r\n", CH);

  if (OUT->situations.empty()) {
    send_to_char("- No situations are currently set, so this is an activity-ending outcome.\r\n", CH);
  } else {
    for (auto [index, slug] : std::views::zip(std::views::iota(0), OUT->situations)) {
      // Look up the situation slug in the activity's index, then display the stringified version.
      if (d->edit_activity) {
        if (auto situation = d->edit_activity->lookup_situation(slug)) {
          send_to_char(CH, "%2d %s\r\n", index + 1, situation->stringify().c_str());
          continue;
        } else if (std::find(ACT->slugs_that_need_writing.begin(), ACT->slugs_that_need_writing.end(), slug) != ACT->slugs_that_need_writing.end()) {
          mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: Slug '%s' is neither in OUT->situations nor ACT->s_t_n_w! Forcibly adding it.", slug.c_str());
          ACT->slugs_that_need_writing.push_back(slug);
          std::sort(ACT->slugs_that_need_writing.begin(), ACT->slugs_that_need_writing.end());
        }
        send_to_char(CH, "%2d - %s ^y(situation not found, you must create it later)^n\r\n", index + 1, slug.c_str());
      } else {
        send_to_char(CH, "%2d - %s ^r(unable to lookup - no d->edit_activity)^n\r\n", index + 1, slug.c_str());
      }
    }
  }

  send_to_char("\r\n"
               "a ) Add a situation\r\n", CH);
  
  if (!OUT->situations.empty()) {
    send_to_char("d ) Delete a situation from the list\r\n", CH);
  }

  send_to_char("\r\n"
               "q ) Save and return to menu\r\n"
               "x ) Discard changes and return\r\n", CH);
}

MenuFrameResult OutcomeSituationsFrame::parse(struct descriptor_data *d, char *arg) {
  switch(LOWER(*arg)) {
    case 'a':
    case 'c':
      MF_PUSH_FRAME(OutcomeSituationSlugSelectFrame, 0);
    case 'd':
      MF_TRYAGAIN_CASE(OUT->situations.empty(), "There's nothing to delete.");
      MF_PROMPT_STRING_AND_RETURN("Which situation slug do you want to delete? (0 to abort)", 86, [d](std::string r){
        auto itr = std::find(OUT->situations.begin(), OUT->situations.end(), r);
        if (itr != OUT->situations.end()) {
          OUT->situations.erase(itr);
          send_to_char(CH, "Deleted '%s'.\r\n", r.c_str());
        } else {
          send_to_char(CH, "Not found.\r\n");
        }
      });
    case 'x':
      OUT->situations = d->edit_slugs_original;
      // fall through
    case 'q':
      return { MenuFrameAction::Pop, child_identifier };
  }

  MF_PARSE_FAILED("That's not an option. Select an effect index to edit, or 'C' to create, 'D' to delete, 'Q' to save, 'X' to abort.");
}

const MenuFrameResult OutcomeSituationsFrame::handle_child_response(struct descriptor_data *d, const MenuFrameResult & result) { return { MenuFrameAction::JustDisplay }; }

// Situation slug selection

void OutcomeSituationSlugSelectFrame::display(struct descriptor_data *d) const {
  // List situation slugs available (not already part of the list), then have them type the slug to add it (or 'c' to create a slug)
  for (auto sitpair : ACT->situations) {
    auto slug = sitpair.first;
    if (std::ranges::contains(OUT->situations, slug)) {
      continue;
    }
    if (auto situation = ACT->lookup_situation(slug)) {
      // Situation exists, so display full lookup info.
      send_to_char(CH, "\r\n", situation->stringify().c_str());
    } else {
      // wtf
      mudlog_vfprintf(CH, LOG_SYSLOG, "Somehow got a slug from act->situations that wasn't able to be looked up??");
      send_to_char(CH, "%s: Something broke\r\n", slug.c_str());
    }
  }

  send_to_char(CH, "Enter the slug to add, or 'q' to return without adding.");
}

MenuFrameResult OutcomeSituationSlugSelectFrame::parse(struct descriptor_data *d, char *arg) {
  if (strlen(arg) == 1) {
    switch(LOWER(*arg)) {
      case 'a':
      case 'c':
        MF_PUSH_FRAME(SituationSlugCreateFrame, 1);
      case 'q':
        return { MenuFrameAction::Pop, child_identifier };
    }
    MF_TRYAGAIN_CASE(LOWER(*arg) == 'x', "Sorry, the 'x' option is not implemented for this interface.");
  }

  // Check if it exists. Stretch goal would be to prompt if they want to add it, but that's thorny to write.
  if (d->edit_activity->lookup_situation(arg)) {
    // Refuse to add if it's already in the list
    MF_TRYAGAIN_CASE_PRINTF(
      std::find(OUT->situations.begin(), OUT->situations.end(), arg) != OUT->situations.end(),
      "Slug %s already exists in the list.",
      arg
    );
    
    // Not already in the list and is valid, so add it and sort it into place.
    OUT->situations.push_back(arg);
    std::sort(OUT->situations.begin(), OUT->situations.end());
    send_to_char(CH, "Done. Add another, or 'q' to save.\r\n");
    return { MenuFrameAction::JustDisplay };
  }

  MF_PARSE_FAILED("That's not an option. Enter an existing situation slug to add, 'C' to create a new one, or 'Q' to abort.");
}

const MenuFrameResult OutcomeSituationSlugSelectFrame::handle_child_response(struct descriptor_data *d, const MenuFrameResult & result) {
  if (result.child_identifier == 1) {
    // Got a valid, brand-new slug back. Add it to the list.
    OUT->situations.push_back(std::get<std::string>(result.data));
    std::sort(OUT->situations.begin(), OUT->situations.end());
  }

  return { MenuFrameAction::JustDisplay };
}




/*

structure of outcomes from header:


// An edge in the digraph that routes from a situation to either another situation or the end of the activity.
class Outcome {
public:
  std::string message; // A fragment that would fit after both 'Lucien ' and 'Assisted by X and Y, Lucien '.
  // e.g. "breaks down the door with his enormous schlong!"

  // An optional list of effects that apply to all characters in the party when this outcome is selected.
  std::vector<Effect> effects = {};

  // A list of situations this outcome can branch to, or empty if the end of the activity.
  std::vector<std::string> situations = {};  // (these slugs may become invalidated during OLC, so handle that gracefully)

  Outcome(const std::string message, const std::vector<Effect> effects, const std::vector<std::string> situations) :
          message(message), effects(effects), situations(situations) {}
  explicit Outcome(const std::string serialized_json);
  Outcome() = default;

  // Getters, which kindly return C strings to prevent having to go .c_str() all the time.
  STRING_GETTER(message)

  // Display the Outcome for easy reading. Only used in edit/debug contexts.
  const char *stringify() const;

  // Apply all effects to the selected character. Return true if they die (i.e. they have been extracted.)
  bool apply(struct char_data *ch) { for (auto& effect : effects) { if (effect.apply(ch)) { return true; } }    return false; }

  std::string serialize(const int indent = -1, const char indent_char = ' ') const;
};

*/



//// shitty little debug test function, tucked out of the way down here
void run_outcome_debug_tests(struct char_data *ch) {
  std::vector<Effect> effectvec = {Effect("_test_func", {{"a", "b"}})};
  Outcome *outcome = new Outcome("my message", effectvec, {"slug here"});

  if (outcome->apply(ch)) { mudlog_vfprintf(NULL, LOG_SYSLOG, "Applying debug outcome caused character death, RIP"); return; }
  send_to_char(ch, "Outcome applied. Here's your message: %s\r\n", outcome->message.c_str());
  
  auto serialized = outcome->serialize(2);
  send_to_char(ch, "Serialized, the outcome is '%s'\r\n", serialized.c_str());
  log_vfprintf("Serialized outcome: '''%s'''", serialized.c_str());
  Outcome *deserialized = new Outcome(serialized);
  auto reserialized = deserialized->serialize(2);
  send_to_char(ch, "Reserialization: %s\r\n", !strcmp(serialized.c_str(), reserialized.c_str()) ? "match!" : "mismatch");

  if (deserialized->apply(ch)) { mudlog_vfprintf(NULL, LOG_SYSLOG, "Applying reserialized debug outcome caused character death, RIP"); return; }
  send_to_char(ch, "Reserialized outcome applied.\r\n");

  delete deserialized;
  delete outcome;
}