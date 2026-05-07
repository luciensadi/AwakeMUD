#include "classes.hpp"
#include "activity_olc.hpp"

#define CH (d->character)
#define ACT (d->edit_activity)
#define CHK (d->edit_check)
#define EFT (d->edit_effect)
#define OPT (d->edit_option)
#define OUT (d->edit_outcome)
#define SIT (d->edit_situation)

#define DECLARE_FUNCS(name_component) \
  void activity_##name_component##_parse(struct descriptor_data *d, const char *arg); \
  void activity_##name_component##_menu(struct descriptor_data *d); \
  void activity_##name_component##_inner(struct descriptor_data *d);

#define PARSEFUNC(name_component) void activity_##name_component##_parse(struct descriptor_data *d, const char *arg)

#define MENUFUNC(name_component)                                     \
  void activity_##name_component##_menu(struct descriptor_data *d) { \
    d->edit_mode = ACTIVITY_EDIT_##name_component;                   \
    activity_##name_component##_inner(d);                            \
  }                                                                  \
  void activity_##name_component##_inner(struct descriptor_data *d)

#define MENU_CASE_TO_PARSER(name_component) case ACTIVITY_EDIT_##name_component: activity_##name_component##_parse(d, arg); break
#define CASE_TO_PARSER(switchcase, name_component) case switchcase: activity_##name_component##_parse(d, arg); break
#define CASE_TO_MENU(switchcase, name_component) case switchcase: activity_##name_component##_menu(d); break
#define CASE_TO_EDITMODE(switchcase, message, the_mode) case switchcase: send_to_char(message, CH); d->edit_mode = the_mode; break

#define REPLACE_STRING_IF_NOT_ABORT(switchcase, var_name, menu_component)  \
  case ACTIVITY_EDIT_##switchcase: {                                       \
    if (!str_cmp(arg, "abort") || !*arg) {                                 \
      send_to_char("Aborted.\r\n", CH);                                    \
    } else {                                                               \
      d->edit_activity->var_name = std::string(arg);                       \
    }                                                                      \
    activity_##menu_component##_menu(d);                                   \
  } break;

// todo: move to OLC header
#define ACTIVITY_EDIT_activity_main_menu 0
#define ACTIVITY_EDIT_activity_edit_slug 1
#define ACTIVITY_EDIT_activity_edit_display_name 2
#define ACTIVITY_EDIT_activity_edit_summary 3
#define ACTIVITY_EDIT_activity_edit_preconditions 4
#define ACTIVITY_EDIT_activity_edit_situations 5
#define ACTIVITY_EDIT_activity_edit_starting_situations 6
#define ACTIVITY_EDIT_check_main_menu 7

// Put a DECLARE_FUNCS() here for each line item where you want a menu and parser function built. Best to name them after the variable involved.
DECLARE_FUNCS(activity_main_menu);
DECLARE_FUNCS(activity_preconditions);
DECLARE_FUNCS(activity_situations);
DECLARE_FUNCS(activity_starting_situations);

DECLARE_FUNCS(activity_check_main_menu);

// This is where all commands are routed for folks in OLC mode.
PARSEFUNC(activity_editing_entrypoint) {
  switch (d->edit_mode) {
    MENU_CASE_TO_PARSER(activity_main_menu);
    MENU_CASE_TO_PARSER(activity_edit_preconditions);
    MENU_CASE_TO_PARSER(activity_edit_situations);
    MENU_CASE_TO_PARSER(activity_edit_starting_situations);
    REPLACE_STRING_IF_NOT_ABORT(activity_edit_slug, slug, activity_main_menu);
    REPLACE_STRING_IF_NOT_ABORT(activity_edit_display_name, display_name, activity_main_menu);
  }
}

MENUFUNC(activity_main_menu) {
  send_to_char(CH, "1)  Shortname (slug): ^c%s^n\r\n", ACT->get_slug());
  send_to_char(CH, "2)  Display Name:     ^c%s^n\r\n", ACT->get_display_name());
  send_to_char(CH, "3)  Summary:\r\n\r\n%s\r\n", ACT->get_summary());

  send_to_char(CH, "\r\n4)  Preconditions:\r\n%s", ACT->preconditions.empty() ? "    - None defined (always available)\r\n" : "");
  for (const auto &precondition : ACT->preconditions) {
    send_to_char(CH, "    - %s\r\n", precondition.stringify());
  }

  send_to_char(CH, "\r\n5)  Situations Available:\r\n%s", ACT->situations.empty() ? "    - ^yNone defined^n\r\n" : "");
  for (const auto &pair : ACT->situations) {
    send_to_char(CH, "    - %s\r\n", pair.second.stringify());
  }

  send_to_char(CH, "\r\n6)  Starting Situations: %s", ACT->starting_situations.empty() ? "^ynot set^n\r\n" : "");
  int items_printed = 0;
  for (const auto &slug : ACT->starting_situations) {
    send_to_char(CH, "%s%s", items_printed++ == 0 ? "" : ", ", slug.c_str());
  }
  send_to_char("\r\n", CH);

  send_to_char("\r\nq)  Quit and save\r\n", CH);
  send_to_char("x)  Exit without saving\r\n", CH);
  send_to_char("\r\nMake a selection: ", CH);
}

PARSEFUNC(activity_main_menu) {
  switch (tolower(*arg)) {
    case 'q':
      {
        // Warn on failed validations.
        if (ACT->starting_situations.empty()) {
          send_to_char("^YWARNING:^n This Activity does not have any starting situations defined, so will not function.", CH);
        }

        // If an existing activity with this slug exists in the map, drop it.
        auto activities_iterator = global_activities.find(ACT->slug);
        if (activities_iterator != global_activities.end()) {
          global_activities.erase(activities_iterator);
        }

        // If we have an old slug, drop the matching activity from the global map.
        if (!ACT->old_slug.empty()) {
          activities_iterator = global_activities.find(ACT->old_slug);
          if (activities_iterator != global_activities.end()) {
            global_activities.erase(activities_iterator);
          }
          ACT->old_slug = "";
        }

        ACT->updatedAt = time(0);

        ACT->save_to_disk();
        
        // Write this activity into the map, keyed by slug, then invalidate our pointer to it so it's not cleaned up.
        global_activities.emplace(ACT->slug, std::move(*ACT));
        ACT = nullptr;
      }
      // fall through
    case 'x':
      // Clean up and return them to the playing state.
      STATE(d) = CON_PLAYING;
      free_editing_structs(d, STATE(d));
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    case '3':
      send_to_char("Enter a tantalizing non-spoiler summary for the activity.\r\n", CH);
      d->edit_mode = ACTIVITY_EDIT_summary;
      DELETE_D_STR_IF_EXTANT(d);
      INITIALIZE_NEW_D_STR(d);
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      break;
    CASE_TO_EDITMODE('1', "Enter the new slug, or enter 'abort' to cancel.\r\n", ACTIVITY_EDIT_slug);
    CASE_TO_EDITMODE('2', "Enter the new display name, or enter 'abort' to cancel.\r\n", ACTIVITY_EDIT_display_name);
    CASE_TO_MENU('4', preconditions);
    CASE_TO_MENU('5', situations);
    CASE_TO_MENU('6', starting_situations);
    default:
      send_to_char(CH, "'%s' is not a valid selection. Enter a number from the menu, or Q to quit and save, or X to abort without saving.\r\n");
      break;
  }
}

// activity preconditions
MENUFUNC(activity_edit_preconditions) {
  // req: no non-deterministic checks (random, etc); aka not in extern std::vector<std::string> non_deterministic_check_functions
  // todo: display current list of preconditions or none, then give options for add, edit parameters on existing, delete
  if (ACT->preconditions.empty()) {
    send_to_char(CH, "No preconditions are defined, so this activity is available for everyone.\r\n");
  } else {
    int idx = 1;
    for (const auto &precondition : ACT->preconditions) {
      send_to_char(CH, "%2d) %s\r\n", idx++, precondition.stringify());
    }
  }
  send_to_char(CH, "\r\n c) Create a condition that must be true for this activity to show up for a character\r\n");
  if (ACT->preconditions.empty()) {
    send_to_char(CH, " #) Edit or delete a condition\r\n");
  }
  send_to_char(CH, " q) Return to menu\r\n");
}

PARSEFUNC(activity_edit_preconditions) {
  // req: no non-deterministic checks (random, etc); aka not in extern std::vector<std::string> non_deterministic_check_functions
  switch (tolower(*arg)) {
    case 'c':
    CASE_TO_MENU('q', activity_main_menu);
    default:
      if (!ACT->preconditions.empty() && isnumber(*arg)) {
        int selection = atoi(arg);
        if (selection > ACT->preconditions.size() || selection <= 0) {
          send_to_char(CH, "There aren't that many preconditions. Pick a number from 1 - %d.", ACT->preconditions.size());
          return;
        }
        d->edit_check = &(ACT->preconditions[selection - 1]);
        activity_check_main_menu(d);
        break;
      }
  }
}

// activity situations
MENUFUNC(activity_edit_situations) {
  // todo: display current list of situations, then give options for add, edit, toggle starting status, delete
}

PARSEFUNC(activity_edit_situations) {
  // todo: if a situation is removed or renamed, ensure it's removed/updated in starting_situations as well
  
}

// activity starting situations
MENUFUNC(activity_edit_starting_situations) {
  // todo: display current list of situations, then ask for a slug to toggle starting status on, or q/0 to go back
}

PARSEFUNC(activity_edit_starting_situations) {
  // todo
}

/////////////////////// Check editing
// Precondition: d->edit_check must be valid.
MENUFUNC(check_main_menu) {

}

PARSEFUNC(check_main_menu) {

}


/////////////////////// Effect editing



/////////////////////// Option editing



/////////////////////// Outcome editing



/////////////////////// Situation editing


// Clean up our various magic defines, just in case.
#undef REPLACE_STRING_IF_NOT_ABORT
#undef PARSEFUNC
#undef MENUFUNC_DECLARE
#undef MENUFUNC
#undef MENU_CASE_TO_PARSER
#undef CASE_TO_PARSER
#undef CASE_TO_MENU
#undef CASE_TO_EDITMODE
#undef ACT
#undef CH