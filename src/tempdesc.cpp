#include "awake.hpp"
#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "db.hpp"

void set_room_tempdesc(struct room_data *room, const char *desc) {
  DELETE_AND_NULL_ARRAY(room->temp_desc);
  room->temp_desc = str_dup(desc);
  room->temp_desc_timeout = MAX(1, room->temp_desc_timeout);

  if (room->people) {
    send_to_room("You blink and your surroundings look a little different.\r\n", room);
  }
}

void tick_down_room_tempdesc_expiries() {
  for (int idx = 0; idx < top_of_world; idx++) {
    if (world[idx].temp_desc && --world[idx].temp_desc_timeout <= 0) {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "Cleaning up temp room desc @ %s (%ld).", GET_ROOM_NAME(&world[idx]), GET_ROOM_VNUM(&world[idx]));
      DELETE_AND_NULL_ARRAY(world[idx].temp_desc);
      world[idx].temp_desc_timeout = 0;

      if (world[idx].people) {
        send_to_room("You blink and your surroundings look a little different.\r\n", &world[idx]);
      }
    }
  }
}

ACMD(do_tempdesc) {
  int minutes_to_expiry = 480;

  FAILURE_CASE(!ch->in_room, "You must be standing in a room to do that.");

  // tempdesc <time in minutes>: Set a temporary room description that expires in X IRL minutes.
  skip_spaces(&argument);
  if (*argument) {
    if (!str_cmp(argument, "clear")) {
      send_to_char("OK, clearing temp desc.\r\n", ch);
      DELETE_AND_NULL_ARRAY(ch->in_room->temp_desc);
      ch->in_room->temp_desc_timeout = 0;
      return;
    }

    minutes_to_expiry = atoi(argument);
  }

  ch->in_room->temp_desc_timeout = minutes_to_expiry;

  PLR_FLAGS(ch).SetBit(PLR_WRITING);
  send_to_char(ch, "Welcome to the temporary description editor! Enter your new room description. It will expire after %ld minutes.\r\n"
               "Terminate with @ on an empty line. You can also just enter @ right now to remove any existing decoration.\r\n"
               "Note that no formatting will be applied here, so indent as/if desired! (MUD-standard indent is 3 spaces.)\r\n", minutes_to_expiry);
  act("$n starts to move things around the room.", TRUE, ch, 0, 0, TO_ROOM);
  STATE(ch->desc) = CON_TEMPDESC_EDIT;
  DELETE_D_STR_IF_EXTANT(ch->desc);
  INITIALIZE_NEW_D_STR(ch->desc);
  ch->desc->max_str = MAX_MESSAGE_LENGTH;
  ch->desc->mail_to = 0;
}