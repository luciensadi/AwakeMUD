#ifndef __MENUFRAME_HPP__
#define __MENUFRAME_HPP__

/*
  For OLC and anything else using menus.

  Descriptor has a vector of MenuFrames, which we push into and pop from as needed.

  In the usual interpreter.cpp parser, if a desc has any frames in its vector, call the parse(arg) func on that frame and bail.

  When a frame decides that it is done, it somehow pops itself and passes data back to its parent.
  - maybe parser pops it before calling it, so it doesn't need to pop itself? If parse results in it needing to stick around, it instead pushes itself back onto the stack
  - but that only shifts the issue-- if you push a result to parent and parent decides it needs to pop, what then? wait for more user input? have a time_to_pop_me() call?
*/

#include <vector>
#include <memory>
#include <string>
#include <variant>

class MenuFrame;
struct descriptor_data;
struct MenuFrameResult;

// Frames return one of these values to interpreter.cpp's invocation:
// - JustDisplay means do nothing to the stack
// - pop means pull me off the stack and send my result up the chain
// - push means put my identified frame on the stack
// - replace means "pop me, then put my nextFrame on the stack"
// - DoNothing: "I just pushed a promptframe onto your stack which will gave you a prompt, so do nothing else (already displayed)"
// regardless of which of these is returned, the next action after processing it will be to invoke display() on the top of the stack
enum class MenuFrameAction { JustDisplay, Pop, Push, Replace, DoNothing };

struct MenuFrameResult {
    MenuFrameAction action;
    int child_identifier = 0;

    std::variant<int, float, bool, std::string, vnum_t> data{std::in_place_type<int>, 0};
};


// The primary MenuFrame parent. Implementations will subclass this. Subclasses can be generic (YesNoMenuFrame etc) with on_success lambdas, or they can
// be bespoke implementations-- up to you.
class MenuFrame {
protected:
  int child_identifier;
public:
  std::unique_ptr<MenuFrame> nextFrame = nullptr; // For pushing new frames (null by default)

  MenuFrame() = default;
  MenuFrame(int child_identifier) : child_identifier(child_identifier) {}

  virtual ~MenuFrame() = default;
  virtual void display(struct descriptor_data *) const = 0;
  virtual MenuFrameResult parse(struct descriptor_data *, char *) = 0;

  virtual const MenuFrameResult handle_child_response(struct descriptor_data *, const MenuFrameResult &) { return { MenuFrameAction::JustDisplay, child_identifier }; }
};

void push_menu_frame(struct descriptor_data *d, std::unique_ptr<MenuFrame> new_frame);

#endif // __MENUFRAME_HPP__