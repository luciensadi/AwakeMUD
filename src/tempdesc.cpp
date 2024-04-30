#include "awake.hpp"
#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "db.hpp"

extern void write_world_to_disk(vnum_t vnum);

void set_room_tempdesc(struct room_data *room, const char *desc, idnum_t idnum) {
  DELETE_AND_NULL_ARRAY(room->temp_desc);
  room->temp_desc = str_dup(desc);
  room->temp_desc_timeout = MAX(1, room->temp_desc_timeout);
  room->temp_desc_author_idnum = idnum;

  if (room->people) {
    send_to_room("You blink and your surroundings look a little different.\r\n", room);
  }
}

void overwrite_room_desc_with_temp_desc(struct room_data *room, struct char_data *ch) {
  FAILURE_CASE(!room, "You're not in a room.");
  FAILURE_CASE(!room->temp_desc, "There is no temp desc set here.");

  // Overwrite the desc.
  delete [] room->description;
  room->description = str_dup(room->temp_desc);

  mudlog_vfprintf(ch, LOG_WIZLOG, "Overwrote room desc with tempdesc.");
  
  // Clear remaining data.
  DELETE_AND_NULL_ARRAY(room->temp_desc);
  room->temp_desc_timeout = 0;
  room->temp_desc_author_idnum = 0;

  // Save.
  write_world_to_disk(get_zone_from_vnum(GET_ROOM_VNUM(room))->number);
}

void clear_temp_desc(struct room_data *room, struct char_data *clearer) {
  if (!room)
    return;

  if (clearer) {
    mudlog_vfprintf(clearer, LOG_WIZLOG, "%s deleted temp room desc @ %s (%ld)", 
                        GET_CHAR_NAME(clearer),
                        GET_ROOM_NAME(room),
                        GET_ROOM_VNUM(room));
  } else {
    mudlog_vfprintf(NULL, LOG_MISCLOG, "Cleaning up temp room desc @ %s (%ld).", GET_ROOM_NAME(room), GET_ROOM_VNUM(room));
  }

  DELETE_AND_NULL_ARRAY(room->temp_desc);
  room->temp_desc_timeout = 0;
  room->temp_desc_author_idnum = 0;

  if (room->people) {
    send_to_room("You blink and your surroundings look a little different.\r\n", room);
  }
}

void tick_down_room_tempdesc_expiries() {
  for (int idx = 0; idx < top_of_world; idx++) {
    if (world[idx].temp_desc && --world[idx].temp_desc_timeout <= 0) {
      clear_temp_desc(&world[idx], NULL);
    }
  }
}

ACMD(do_tempdesc) {
  int minutes_to_expiry = 480;

  FAILURE_CASE(IS_NPC(ch), "NPCs can't set tempdescs.");
  FAILURE_CASE(!ch->in_room, "You must be standing in a room to do that.");

#ifdef IS_BUILDPORT
  FAILURE_CASE(!access_level(ch, LVL_BUILDER), "Only staff can use this command on the buildport.");
  FAILURE_CASE(!PLR_FLAGGED(ch, PLR_OLC), "You must have OLC enabled to set a tempdesc on the buildport.");
  minutes_to_expiry = 100000;
#else
  FAILURE_CASE(!PLR_FLAGGED(ch, PLR_RPE), "You can't use this command. Speak to RP staff if you want to apply for the ability to do so.");
#endif

  // tempdesc <time in minutes>: Set a temporary room description that expires in X IRL minutes.
  skip_spaces(&argument);
  if (*argument) {
    if (!str_cmp(argument, "clear")) {
      clear_temp_desc(ch->in_room, ch);
      return;
    }

#ifdef IS_BUILDPORT
    if (!str_cmp(argument, "commit")) {
      FAILURE_CASE(!access_level(ch, LVL_ADMIN), "You must be an admin or higher to do that.");
      overwrite_room_desc_with_temp_desc(ch->in_room, ch);
      return;
    }
#endif

    minutes_to_expiry = atoi(argument);

    FAILURE_CASE(minutes_to_expiry <= 0, "Syntax: TEMPDESC; or TEMPDESC CLEAR; or TEMPDESC <number of minutes to set desc>");
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