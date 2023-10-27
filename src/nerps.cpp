#include "structs.hpp"
#include "awake.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "handler.hpp"

/* Consume a NERP-flagged item in your inventory, printing to the room. */
ACMD(do_usenerps) {
  skip_spaces(&argument);

  FAILURE_CASE(!*argument, "To consume (delete) one item and inform GMs in the room about it, do ^WUSENERPS <item name>^n");

  struct obj_data *obj = get_obj_in_list_vis(ch, argument, ch->carrying);

  FAILURE_CASE_PRINTF(!obj, "You don't seem to be carrying anything called '%s'.", argument);
  FAILURE_CASE_PRINTF(!IS_OBJ_STAT(obj, ITEM_EXTRA_NERPS), "%s isn't a NERP item.", CAP(GET_OBJ_NAME(obj)));
  FAILURE_CASE_PRINTF(IS_OBJ_STAT(obj, ITEM_EXTRA_KEPT), "You'll have to UNKEEP %s first.", decapitalize_a_an(GET_OBJ_NAME(obj)));
  FAILURE_CASE_PRINTF(obj->contains, "You'll have to empty %s first.", decapitalize_a_an(GET_OBJ_NAME(obj)));

  // If there were no questors or staff around to see it happen, you can't do it.
  {
    struct char_data *viewer;

    // Find valid viewers.
    for (viewer = viewer = (ch->in_room ? ch->in_room->people : ch->in_veh->people);
         viewer;
         viewer = (ch->in_room ? viewer->next_in_room : viewer->next_in_veh)) 
    {
      if (viewer == ch)
        continue;

      if (PRF_FLAGGED(viewer, PRF_QUESTOR) || (IS_SENATOR(viewer) && CAN_SEE(ch, viewer)))
        break;
    }

    FAILURE_CASE_PRINTF(!viewer, "No GMs are around to see you use %s, so you hold off.", decapitalize_a_an(GET_OBJ_NAME(obj)));
  }

  // Success. Send a message to GMs in the room.
  char msg_buf[1000];
  snprintf(msg_buf, sizeof(msg_buf), "1 '%s' is used by $n.", decapitalize_a_an(GET_OBJ_NAME(obj)));

  for (struct char_data *viewer = viewer = (ch->in_room ? ch->in_room->people : ch->in_veh->people);
        viewer;
        viewer = (ch->in_room ? viewer->next_in_room : viewer->next_in_veh)) 
  {
    if (viewer == ch)
      continue;
      
    if (PRF_FLAGGED(viewer, PRF_QUESTOR) || IS_SENATOR(viewer)) {
      act(msg_buf, FALSE, ch, NULL, viewer, TO_VICT);
    }
  }

  send_to_char(ch, "You discreetly notify the GM that you've spent '%s'.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));

  // Consume it.
  extract_obj(obj);
}