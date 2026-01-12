#include "interpreter.hpp"
#include "activities/classes.hpp"

void activities_list(struct char_data *ch, char *arg1, char *arg2);
void activities_show(struct char_data *ch, char *arg1, char *arg2);
void activities_create(struct char_data *ch, char *arg1, char *arg2);
void activities_edit(struct char_data *ch, char *arg1, char *arg2);
void activities_delete(struct char_data *ch, char *arg1, char *arg2);
void activities_run(struct char_data *ch, char *arg1, char *arg2);

#define ACTIVITY_CMD(char_array, alias, func, privilege) \
if (!is_abbrev(alias, char_array)) { \
  FAILURE_CASE(privilege > 0 && !access_level(ch, privilege), "Sorry, you can't do that at your level."); \
  func(ch, arg1, arg2); \
  return; \
}

/* */
ACMD(do_activity) {
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];

  skip_spaces(&argument);
  half_chop(argument, arg1, arg2, sizeof(arg2));

  ACTIVITY_CMD(arg1, "list", activities_list, LVL_MORTAL);
  ACTIVITY_CMD(arg1, "show", activities_show, LVL_MORTAL);
  ACTIVITY_CMD(arg1, "create", activities_create, LVL_BUILDER);
  ACTIVITY_CMD(arg1, "edit", activities_edit, LVL_BUILDER);
  ACTIVITY_CMD(arg1, "delete", activities_delete, LVL_BUILDER);
  ACTIVITY_CMD(arg1, "run", activities_run, LVL_BUILDER);
}
#undef ACTIVITY_CMD