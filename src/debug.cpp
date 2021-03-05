/************************************************************************
 * Code for the DEBUG command, which invokes various testing protocols. *
 * Written for AwakeMUD Community Edition by Lucien Sadi.     07/01/19  *
 ************************************************************************/

#include <time.h>
#include <string.h>

#include "types.h"
#include "awake.h"
#include "interpreter.h"
#include "bitfield.h"
#include "comm.h"
#include "utils.h"
#include "playergroup_classes.h"
#include "playergroups.h"
#include "structs.h"
#include "handler.h"

// The linked list of loaded playergroups.
extern Playergroup *loaded_playergroups;

// We're looking to verify that everything is kosher. Validate canaries, etc.
void verify_data(struct char_data *ch, const char *line, int cmd, int subcmd, const char *section) {
  // Called by a character doing something.
  if (ch) {
    // Check character's canaries.
    
    
    // Check their gear's canaries.
    
    // Check their descriptor's canaries.
  }
  
  // Called on a tick. This is a more thorough validation. If it fails, we know to look at the command logs in the last while.
  else {
    
  }
}

void do_pgroup_debug(struct char_data *ch, char *argument) {
  static char arg1[MAX_INPUT_LENGTH];
  static char arg2[MAX_INPUT_LENGTH];
  char *rest_of_argument = NULL;
  
  if (!*argument) {
    send_to_char(ch, "Syntax: DEBUG PGROUP <modes>\r\n");
    return;
  }
  
  // Extract the mode switch argument.
  rest_of_argument = any_one_arg(argument, arg1);
  rest_of_argument = any_one_arg(rest_of_argument, arg2);
  send_to_char(ch, "arg1: '%s', arg2: '%s'\r\n", arg1, arg2);
  
  if (*arg1 && strn_cmp(arg1, "list", strlen(arg1)) == 0) {
    if (!*arg2 && strn_cmp(arg2, "all", strlen(arg2) == 0)) {
      // List all pgroups, including those in the DB.
      send_to_char(ch, "not implemented yet.\r\n");
      return;
    } else if (strn_cmp(arg2, "loaded", strlen(arg2)) == 0) {
      // List only the pgroups that are currently loaded.
      send_to_char(ch, "Currently loaded playergroups:\r\n");
      for (Playergroup *pgr = loaded_playergroups; pgr; pgr = pgr->next_pgroup) {
        send_to_char(ch, "%ld) %s: %s (%s), %s, %s, %s, %s, bank %ld\r\n",
                     pgr->get_idnum(),
                     pgr->get_tag(),
                     pgr->get_name(),
                     pgr->get_alias(),
                     pgr->is_clone() ? "[CLONE]" : "[non-clone]",
                     pgr->is_disabled() ? "[DISABLED]" : "[enabled]",
                     pgr->is_founded() ? "[founded]" : "[provisional]",
                     pgr->is_secret() ? "[secret]" : "[non-secret]",
                     pgr->get_bank());
      }
    } else {
      send_to_char(ch, "unrecognized input '%s'\r\n", arg2);
      return;
    }
  } else {
    send_to_char(ch, "unrecognized input '%s'\r\n", arg2);
    return;
  }
}

ACMD(do_debug) {
  static char arg1[MAX_INPUT_LENGTH];
  static char arg2[MAX_INPUT_LENGTH];
  char *rest_of_argument = NULL;
  
  // It's a debug function-- let's normalize everything as much as we can.
  memset(arg1, 0, sizeof(arg1));
  memset(arg2, 0, sizeof(arg2));
  
  if (!*argument) {
    send_to_char("Syntax: DEBUG <mode> <arguments>\r\n", ch);
    send_to_char("Refer to src/debug.cpp for list of modes.\r\n", ch);
    return;
  }
  
  // Extract the mode switch argument.
  rest_of_argument = any_one_arg(argument, arg1);
  
  if (strn_cmp(arg1, "pgroups", strlen(arg1)) == 0) {
    do_pgroup_debug(ch, rest_of_argument);
  }
  
  if (access_level(ch, LVL_PRESIDENT) && strn_cmp(arg1, "swimcheck", strlen(arg1)) == 0) {
    if (!ch->in_room) {
      send_to_char(ch, "You must be in a room to do that.\r\n");
      return;
    }
    
    send_to_char(ch, "Calculating swim success rates. This will take a bit.\r\n");
    
    void analyze_swim_successes(struct char_data *temp_char);
    analyze_swim_successes(ch);
    
    send_to_char(ch, "Done.\r\n");
  }
  
  if (access_level(ch, LVL_PRESIDENT) && strn_cmp(arg1, "void", strlen(arg1)) == 0) {
    skip_spaces(&rest_of_argument);
    struct obj_data *obj = get_obj_in_list_vis(ch, rest_of_argument, ch->carrying);
    if (obj) {
      send_to_char(ch, "OK, now sending %s beyond time and space...\r\n", GET_OBJ_NAME(obj));
      obj->in_room = NULL;
      obj->in_veh = NULL;
      obj->carried_by = NULL;
      obj->worn_by = NULL;
      obj->in_obj = NULL;
      send_to_char(ch, "Done. %s has had all of its location pointers wiped. The game will now crash when your character is extracted. Enjoy!\r\n", GET_OBJ_NAME(obj));
    } else {
      send_to_char(ch, "You don't seem to have anything called '%s' in your inventory.\r\n", rest_of_argument);
    }
  }
}
