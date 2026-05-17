#include <functional>
#include <string>
#include <cctype>

#include "interpreter.hpp"
#include "structs.hpp"

// Call this whenever you're pushing on a menu frame, just to ensure the display is always triggered and their state is properly set.
void push_menu_frame(struct descriptor_data *d, std::unique_ptr<MenuFrame> new_frame) {
  if (!new_frame) return;

  new_frame->display(d);

  d->menu_frame_stack.push_back(std::move(new_frame));

  if (STATE(d) == CON_PLAYING) {
    STATE(d) = CON_MENUFRAME;
  }
}

void handle_menu_frames(struct descriptor_data *d, char *arg) {
  skip_spaces(&arg);

  if (d->menu_frame_stack.empty()) {
    mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: Got descriptor to handle_menu_frames() with an empty frame stack! They're boned, sending advice to reconnect.");
    send_to_char(d->character, "^RSorry, you seem to be stuck in a bad state.^n Disconnecting and reconnecting will probably fix it.\r\n");
    return;
  }

  // Send their input to the top of their MenuFrame stack for parsing.
  MenuFrameResult result = d->menu_frame_stack.back().get()->parse(d, arg);

  if (result.action == MenuFrameAction::DoNothing) {
    return;
  }

  // Pop in a loop until we have a non-pop result OR until the stack is empty.
  while (result.action == MenuFrameAction::Pop) {
    d->menu_frame_stack.pop_back();

    if (d->menu_frame_stack.empty()) {
      // Nothing left in the stack? Restore to playing state and bail.
      STATE(d) = CON_PLAYING;
      return;
    }

    result = d->menu_frame_stack.back().get()->handle_child_response(d, result);
    if (result.action == MenuFrameAction::Pop) {
      continue;  // keep looping until the parent says to stop popping
    }

    // Finished popping? Process the result normally.
    // fall through
  }

  // If the frame asked us to push something else on (a prompt, etc), do so now.
  if (result.action == MenuFrameAction::Push || result.action == MenuFrameAction::Replace) {
    if (!d->menu_frame_stack.back().get()->nextFrame) {
      mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: Got Push result from MenuFrame without a nextFrame attached to node! Parsed input was '%s'.", arg);
      send_to_char(d->character, "^RThat selection is bugged!^n Please report it to staff. If you're stuck in this menu, disconnect and reconnect to abort.\r\n");
      return;
    }

    auto next_frame = std::move(d->menu_frame_stack.back().get()->nextFrame);
    d->menu_frame_stack.back().get()->nextFrame = nullptr;

    // Replace means that we need to pop before pushing.
    if (result.action == MenuFrameAction::Replace)
      d->menu_frame_stack.pop_back();

    // Push it to the stack, triggering its display function in the process.
    push_menu_frame(d, std::move(next_frame));
    return;
  }

  // At this point, we know that there is at least one frame on their stack. Trigger its display() method.
  d->menu_frame_stack.back().get()->display(d);
}


//// debug / manual testing code here
#include "MenuFrameGenerics.hpp"

struct DebugMenuFrameBuddy {
  int stored_int;
  float stored_float;
  std::string stored_string;
};

class DebugMenuFrame : public MenuFrame {
public:
  struct DebugMenuFrameBuddy data{};

  MenuFrameResult parse(struct descriptor_data *, char *) override;
  void display(struct descriptor_data *) const override;
  const MenuFrameResult handle_child_response(struct descriptor_data *d, const MenuFrameResult &) override;
};

void DebugMenuFrame::display(struct descriptor_data *d) const {
  send_to_char(d->character, "1) Stored int: %d\r\n", d->edit_number2);
  send_to_char(d->character, "2) Stored float: %.02f\r\n", data.stored_float);
  send_to_char(d->character, "3) Stored string: %s\r\n", data.stored_string.c_str());
  send_to_char(d->character, "\r\n"
                             "q) Quit\r\n"
                             "\r\n"
                             "Enter your choice: ");
}

MenuFrameResult DebugMenuFrame::parse(struct descriptor_data *d, char *arg) {
  switch (*arg) {
    case '1':
      MF_PROMPT_INT_AUTOSET_D_AND_RETURN("Type an int between 0-10: ", edit_number2, 0, 10);
    case '2':
      MF_PROMPT_FLOAT_AND_RETURN("type a float:", 0, [data=&data](float r){data->stored_float = r;});
    case '3':
      MF_PROMPT_STRING_AND_RETURN("type a string:", 0, [data=&data](std::string r){data->stored_string = r;});
    case 'q':
      MF_PROMPT_YESNO_AND_RETURN("do you want to save?", 1);
  }
  send_to_char(d->character, "'%s' is not a valid selection, try again:", arg);
  return { MenuFrameAction::JustDisplay };
}

const MenuFrameResult DebugMenuFrame::handle_child_response(struct descriptor_data *d, const MenuFrameResult & result) {
  if (result.child_identifier == 1) {
    send_to_char(d->character, "Your selection was '%s'.\r\n", std::get<bool>(result.data) ? "yes, save" : "nope, trash it");
    return { MenuFrameAction::Pop };
  }
  return { MenuFrameAction::JustDisplay };
}

void debug_menu_frames(struct char_data *ch, char *rest_of_arg) {
  send_to_char(ch, "OK, putting you into the debug menu frame.\r\n");
  push_menu_frame(ch->desc, std::make_unique<DebugMenuFrame>());
}