#include "../interpreter.hpp"
#include "classes.hpp"
#include "activity_olc.hpp"

void do_activities_list(struct char_data *ch, char *arguments);
void do_activities_show(struct char_data *ch, char *arguments);
void do_activities_create(struct char_data *ch, char *arguments);
void do_activities_edit(struct char_data *ch, char *arguments);
void do_activities_delete(struct char_data *ch, char *arguments);
void do_activities_run(struct char_data *ch, char *arguments);
void do_activities_debug(struct char_data *ch, char *arguments);

void display_activities_help(struct char_data *ch);

// The list of commands they can run under the ACTIVITY verb. Protip: Keep this alphabetized.
struct activity_cmd_struct {
  const char *cmd;
  int level_required;
  void (*command_pointer) (struct char_data *ch, char *argument);
  const char *help_string;
} activity_commands[] = {
  { "create"  , LVL_BUILDER   , do_activities_create , "Create an activity with the given shortname, which may not contain spaces." },
  { "debug"   , LVL_PRESIDENT , do_activities_debug  , "Donate money to the group. It is wired from your account to the group's." },
  { "delete"  , LVL_PRESIDENT , do_activities_delete , "Delete an activity. Do this with caution!" },
  { "edit"    , LVL_BUILDER   , do_activities_edit   , "Edit an existing activity." },
  { "list"    , LVL_BUILDER   , do_activities_list   , "Lists the activities that you know about." },
  { "run"     , LVL_BUILDER   , do_activities_run    , "Demote someone who is below your level." },
  { "show"    , LVL_BUILDER   , do_activities_show   , "Shows the details on a given activity." },
  { "\n"      , 0             , 0                    , "" } // This must be last.
};

ACMD(do_activity) {
  FAILURE_CASE(!ch->desc, "nope");
  FAILURE_CASE(IS_ASTRAL(ch), "Astral characters would time out during activities. Return to your body first.");

  char command[MAX_STRING_LENGTH];
  char parameters[MAX_STRING_LENGTH];

  half_chop(argument, command, parameters, sizeof(parameters));

  // In the absence of a command, show help and exit.
  if (!*command) {
    display_activities_help(ch);
    return;
  }

  // Find the index of the command the player wants.
  int cmd_index = 0;
  for (cmd_index = 0; *(activity_commands[cmd_index].cmd) != '\n'; cmd_index++)
    if (!strncmp(command, activity_commands[cmd_index].cmd, strlen(command)))
      break;

  // Precondition: If the command was invalid or not accessible to them, show help and exit.
  if (*(activity_commands[cmd_index].cmd) == '\n' || !access_level(ch, activity_commands[cmd_index].level_required)) {
    display_activities_help(ch);
    return;
  }

  // Execute the subcommand.
  ((*activity_commands[cmd_index].command_pointer) (ch, parameters));
}

void display_activities_help(struct char_data *ch) {
  send_to_char("Usage: activity <command> <parameters>\r\n", ch);
  send_to_char("Commands:\r\n", ch);

  for (int cmd_index = 0; *(activity_commands[cmd_index].cmd) != '\n'; cmd_index++) {
    if (access_level(ch, activity_commands[cmd_index].level_required)) {
      send_to_char(ch, "  %-10s - %s\r\n", activity_commands[cmd_index].cmd, activity_commands[cmd_index].help_string);
    }
  }
}

void do_activities_create(struct char_data *ch, char *arguments) {
  // Message sent in function.
  if (!is_olc_available(ch)) { return; }

  ch->desc->edit_activity = new Activity();

  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  STATE(ch->desc) = CON_ACTIVITY_EDIT;
  activity_activity_main_menu(ch->desc);
}

void do_activities_debug(struct char_data *ch, char *arguments) {

}

void do_activities_delete(struct char_data *ch, char *arguments) {
  // Message sent in function.
  if (!is_olc_available(ch)) { return; }

  // without 'confirm', do nothing

  // with 'confirm', process deletion and log
}

void do_activities_edit(struct char_data *ch, char *arguments) {
  // Message sent in function.
  if (!is_olc_available(ch)) { return; }

  // find the activity identified by slug; error if not found
  auto itr = global_activities.find(arguments);
  FAILURE_CASE_PRINTF(itr == global_activities.end(), "'%s' is not a valid activity slug. Use ACTIVITIES LIST to see the ones available.", arguments);

  // Ensure you have edit abilities.
  FAILURE_CASE(itr->second.can_edit(ch), "You don't have permission to edit that activity.");

  ch->desc->edit_activity = new Activity(itr->second);

  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  STATE(ch->desc) = CON_ACTIVITY_EDIT;
  activity_activity_main_menu(ch->desc);
}

void do_activities_list(struct char_data *ch, char *arguments) {
  FAILURE_CASE(global_activities.empty(), "There are no activities to list.");
  for (auto activity : global_activities) {
    if (access_level(ch, LVL_BUILDER) || activity.second.meets_preconditions(ch)) {
      send_to_char(ch, "%30s -- %s\r\n", activity.second.slug.c_str(), activity.second.display_name.c_str());
    }
  }
}

void do_activities_run(struct char_data *ch, char *arguments) {

}

void do_activities_show(struct char_data *ch, char *arguments) {

}
