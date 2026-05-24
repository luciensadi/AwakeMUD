#pragma once

#include <functional>

#include "structs.hpp"

#define MF_TRYAGAIN_CASE(condition, message) {       \
  if (d->character && (condition)) {                 \
    send_to_char(d->character, "%s\r\n", (message)); \
    return { MenuFrameAction::JustDisplay };         \
  }                                                  \
}                                                    \

#define MF_TRYAGAIN_CASE_PRINTF(condition, ...) {           \
  if (d->character && (condition)) {                        \
    send_to_char(d->character, __VA_ARGS__);                \
    send_to_char(d->character, "\r\n"); /*force a newline*/ \
    return { MenuFrameAction::JustDisplay };                \
  }                                                         \
}                                                           \

#define MF_PROMPT_INT_AUTOSET_D_AND_RETURN(prompt, var_to_set, min, max) \
    nextFrame = std::make_unique<IntPromptFrame>(prompt, 0, [d](int r){d->var_to_set = r;}, min, max); \
    return { MenuFrameAction::Push };

#define MF_PROMPT_INT_AND_RETURN(...) \
    nextFrame = std::make_unique<IntPromptFrame>(__VA_ARGS__); \
    return { MenuFrameAction::Push };

#define MF_PROMPT_FLOAT_AUTOSET_D_AND_RETURN(prompt, var_to_set) \
    nextFrame = std::make_unique<FloatPromptFrame>(prompt, 0, [d](float r){d->var_to_set = r;}); \
    return { MenuFrameAction::Push };

#define MF_PROMPT_FLOAT_AND_RETURN(...) \
    nextFrame = std::make_unique<FloatPromptFrame>(__VA_ARGS__); \
    return { MenuFrameAction::Push };

#define MF_PROMPT_STRING_AUTOSET_D_AND_RETURN(prompt, var_to_set) \
    nextFrame = std::make_unique<StringPromptFrame>(prompt, 0, [d](std::string r) { \
      delete [] d->var_to_set;                                                      \
      d->var_to_set = str_dup(r.c_str());                                           \
    });                                                                             \
    return { MenuFrameAction::Push };

#define MF_PROMPT_STRING_AND_RETURN(...) \
    nextFrame = std::make_unique<StringPromptFrame>(__VA_ARGS__); \
    return { MenuFrameAction::Push };

#define MF_PROMPT_YESNO_AND_RETURN(...) \
    nextFrame = std::make_unique<YesNoPromptFrame>(__VA_ARGS__); \
    return { MenuFrameAction::Push };

// Generic for defining a frame; forces you to write the parse(), display(), and handle_child_response() funcs.
#define MF_BOILERPLATE_CLASS(class_name) \
class class_name : public MenuFrame { \
public: \
  class_name(int child_identifier) : MenuFrame(child_identifier) {} \
  MenuFrameResult parse(struct descriptor_data *, char *) override; \
  void display(struct descriptor_data *) const override; \
  const MenuFrameResult handle_child_response(struct descriptor_data *d, const MenuFrameResult &) override; \
};

// Generic for pushing a fully qualified frame (no prompt or lambda needed)
#define MF_PUSH_FRAME(frame, child_identifier) \
    nextFrame = std::make_unique<frame>(child_identifier); \
    return { MenuFrameAction::Push };

/*
  The Generic Input family of menu frames requests a typed input and returns it.
  Their default display is to just send the prompt again.
  If you somehow give them a child result, they'll return that up the stack instead, popping themselves in the process.
*/
class GenericInputMenuFrame : public MenuFrame {
protected:
  std::string prompt_;
public:
  explicit GenericInputMenuFrame(std::string prompt, int child_identifier)
      : MenuFrame(child_identifier), prompt_(std::move(prompt)) {}
  void display(struct descriptor_data *d) const override { send_to_char(prompt_.c_str(), d->character); }

  const MenuFrameResult handle_child_response(struct descriptor_data *, const MenuFrameResult & result) override { return { MenuFrameAction::Pop, child_identifier, result.data }; }
};

class YesNoPromptFrame : public GenericInputMenuFrame {
protected:
  std::function<void(bool)> on_choice_ = nullptr;
public:
  YesNoPromptFrame(std::string prompt, int child_identifier, std::function<void(bool)> on_choice=nullptr)
      : GenericInputMenuFrame(std::move(prompt), child_identifier), on_choice_(std::move(on_choice)) {}
  MenuFrameResult parse(struct descriptor_data *d, char *arg) override;
};

class IntPromptFrame : public GenericInputMenuFrame {
protected:
  std::function<void(int)> on_choice_ = nullptr;
  int min_val, max_val;
public:
  IntPromptFrame(std::string prompt, int child_identifier, std::function<void(int)> on_choice=nullptr, int min=INT_MIN, int max=INT_MAX)
      : GenericInputMenuFrame(std::move(prompt), child_identifier), on_choice_(std::move(on_choice)), min_val(min), max_val(max) {}
  MenuFrameResult parse(struct descriptor_data *d, char *arg) override;
};

class VnumPromptFrame : public GenericInputMenuFrame {
protected:
  std::function<void(vnum_t)> on_choice_ = nullptr;
  int min_val, max_val;
public:
  VnumPromptFrame(std::string prompt, int child_identifier, std::function<void(vnum_t)> on_choice=nullptr, vnum_t min=INT_MIN, vnum_t max=INT_MAX)
      : GenericInputMenuFrame(std::move(prompt), child_identifier), on_choice_(std::move(on_choice)), min_val(min), max_val(max) {}
  MenuFrameResult parse(struct descriptor_data *d, char *arg) override;
};

class FloatPromptFrame : public GenericInputMenuFrame {
protected:
  std::function<void(float)> on_choice_ = nullptr;
  int min_val, max_val;
public:
  FloatPromptFrame(std::string prompt, int child_identifier, std::function<void(float)> on_choice=nullptr, float min=-100000.0f, int max=100000.0f)
      : GenericInputMenuFrame(std::move(prompt), child_identifier), on_choice_(std::move(on_choice)), min_val(min), max_val(max) {}
  MenuFrameResult parse(struct descriptor_data *d, char *arg) override;
};

class StringPromptFrame : public GenericInputMenuFrame {
protected:
  std::function<void(std::string)> on_choice_ = nullptr;
  size_t min_length, max_length_with_color;
  int max_length_after_color_removal;
public:
  StringPromptFrame(std::string prompt,
                    int child_identifier,
                    std::function<void(std::string)> on_choice=nullptr,
                    size_t min_length=0,
                    size_t max_length_with_color=MAX_INPUT_LENGTH,
                    int max_length_after_color_removal=MAX_INPUT_LENGTH)
      : GenericInputMenuFrame(std::move(prompt), child_identifier),
        on_choice_(std::move(on_choice)),
        min_length(min_length),
        max_length_with_color(max_length_with_color),
        max_length_after_color_removal(max_length_after_color_removal)
      {}
  MenuFrameResult parse(struct descriptor_data *d, char *arg) override;
};

//////////////////////////////////////////////////////////////////////
// Less-generic generics below here (skill, spell, power, etc inputs)
//////////////////////////////////////////////////////////////////////

// Prompts for a skill name, returns the skill index (int). Also accepts 'abort' to cancel.
class SkillNamePromptFrame : public IntPromptFrame {
public:
  SkillNamePromptFrame(std::string prompt, int child_identifier, std::function<void(int)> on_choice=nullptr)
      : IntPromptFrame(std::move(prompt), child_identifier, std::move(on_choice)) {}
  MenuFrameResult parse(struct descriptor_data *d, char *arg) override;
};

// Prompts for a spell name, returns the spell index (int). Does not have handling for spells with subtypes (e.g. Increase/Decrease). Also accepts 'abort' to cancel.
class SpellNamePromptFrame : public IntPromptFrame {
public:
  SpellNamePromptFrame(std::string prompt, int child_identifier, std::function<void(int)> on_choice=nullptr)
      : IntPromptFrame(std::move(prompt), child_identifier, std::move(on_choice)) {}
  MenuFrameResult parse(struct descriptor_data *d, char *arg) override;
};

// Prompts for a power name, returns the power index (int). Also accepts 'abort' to cancel.
class PowerNamePromptFrame : public IntPromptFrame {
public:
  PowerNamePromptFrame(std::string prompt, int child_identifier, std::function<void(int)> on_choice=nullptr)
      : IntPromptFrame(std::move(prompt), child_identifier, std::move(on_choice)) {}
  MenuFrameResult parse(struct descriptor_data *d, char *arg) override;
};

// Prompts for an object vnum, validates that it exists, then returns the vnum. Also accepts -1 to cancel.
class ObjVnumPromptFrame : public VnumPromptFrame {
public:
  ObjVnumPromptFrame(std::string prompt, int child_identifier, std::function<void(int)> on_choice=nullptr)
      : VnumPromptFrame(std::move(prompt), child_identifier, std::move(on_choice)) {}
  MenuFrameResult parse(struct descriptor_data *d, char *arg) override;
};
/*

        case ActivityParamType::OBJ_VNUM:
          nextFrame = std::make_unique<ObjVnumPromptFrame>("Enter the object vnum: ", 0, [param_name=param_itr.name, d](vnum_t vnum){ (*PARAMS)[param_name] = std::to_string(vnum); });
          return { MenuFrameAction::Push };
        case ActivityParamType::MOB_VNUM:
          nextFrame = std::make_unique<MobVnumPromptFrame>("Enter the mob's vnum: ", 0, [param_name=param_itr.name, d](vnum_t vnum){ (*PARAMS)[param_name] = std::to_string(vnum); });
          return { MenuFrameAction::Push };
        case ActivityParamType::ROOM_VNUM:
          nextFrame = std::make_unique<ObjVnumPromptFrame>("Enter the room's vnum: ", 0, [param_name=param_itr.name, d](vnum_t vnum){ (*PARAMS)[param_name] = std::to_string(vnum); });
          return { MenuFrameAction::Push };
        case ActivityParamType::QUEST_VNUM:
          nextFrame = std::make_unique<ObjVnumPromptFrame>("Enter the quest's vnum: ", 0, [param_name=param_itr.name, d](vnum_t vnum){ (*PARAMS)[param_name] = std::to_string(vnum); });
          return { MenuFrameAction::Push };
        case ActivityParamType::NUYEN_AMOUNT:
          nextFrame = std::make_unique<NuyenQtyPromptFrame>("Enter the nuyen quantity: ", 0, [param_name=param_itr.name, d](int amount){ (*PARAMS)[param_name] = std::to_string(amount); });
          return { MenuFrameAction::Push };
        case ActivityParamType::BOOLEAN:
          MF_PROMPT_YESNO_AND_RETURN("Enter Y or N: ", 0, [param_name=param_itr.name, d](bool result){ (*PARAMS)[param_name] = result; });
        case ActivityParamType::KARMA_AMOUNT:
          nextFrame = std::make_unique<KarmaQtyPromptFrame
*/