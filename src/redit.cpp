/* *************************************************
* file: redit.cc                                   *
* authors: Andrew Hynek, Nick Robertson, Jon Lin,  *
* Phrodo, Demise, Terra, Washu                     *
* purpose: Room Editor for AwakeOLC, a             *
* component of AwakeMUD                            *
* (c) 1997-1999 Andrew Hynek, (c) 2001             *
* The AwakeMUD Consortium                          *
************************************************* */


#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "structs.h"
#include "awake.h"
#include "interpreter.h"
#include "comm.h"
#include "utils.h"
#include "db.h"
#include "newdb.h"
#include "boards.h"
#include "screen.h"
#include "olc.h"
#include "memory.h"
#include "constants.h"
#include "handler.h"
#include "config.h"

extern class memoryClass *Mem;

#define ROOM d->edit_room
#define DOOR d->edit_room->dir_option[d->edit_number2]

extern sh_int r_mortal_start_room;
extern sh_int r_immort_start_room;
extern sh_int r_frozen_start_room;
extern vnum_t r_newbie_start_room;
extern int olc_state;
extern void char_to_room(struct char_data * ch, int room);


// extern funcs
extern char *cleanup(char *dest, const char *src);
extern bool resize_world_array();

/* function protos */
void redit_disp_extradesc_menu(struct descriptor_data * d);
void redit_disp_exit_menu(struct descriptor_data * d);
void redit_disp_exit_flag_menu(struct descriptor_data * d);
void redit_disp_flag_menu(struct descriptor_data * d);
void redit_disp_sector_menu(struct descriptor_data * d);
void redit_disp_menu(struct descriptor_data * d);
void redit_parse(struct descriptor_data * d, const char *arg);
void write_world_to_disk(int vnum);
/**************************************************************************
 Menu functions
 **************************************************************************/

void redit_disp_light_menu(struct descriptor_data *d, bool light)
{
  CLS(CH);
  int i = light ? 1 : 5, max = light ? 5 : 10;
  send_to_char("0) Normal\r\n", CH);
  for (int x = 1; i < max; i++)
    send_to_char(CH, "%d) %s\r\n", x++, light_levels[i]);
  send_to_char("Select Light Option: ", CH);
  if (light)
     d->edit_mode = REDIT_LIGHT;
  else d->edit_mode = REDIT_SMOKE;
}

void redit_disp_combat_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "1) Crowd: ^c%d^n\r\n"
                "2) Cover: ^c%d^n\r\n"
                "3) Room Type: ^c%s^n\r\n"
                "4) X: ^c%dm^n\r\n"
                "5) Y: ^c%dm^n\r\n"
                "6) Z: ^c%.2fm^n\r\n"
                "q) Return to previous menu\r\n"
                "Select Option: ", d->edit_room->crowd, d->edit_room->cover, room_types[d->edit_room->type],
                d->edit_room->x, d->edit_room->y, d->edit_room->z);
  d->edit_mode = REDIT_COMBAT;
}

void redit_disp_barrier_menu(struct descriptor_data *d)
{
  CLS(CH);

  for (int counter = 0; counter < NUM_BARRIERS; ++counter)
    send_to_char(CH, "%2d) %s\r\n", counter + 1, barrier_names[counter]);
  send_to_char("Enter construction category, 0 to return: ", CH);
}

void redit_disp_material_menu(struct descriptor_data *d)
{
  CLS(CH);

  for (int counter = 0; counter < NUM_MATERIALS; ++counter)
    send_to_char(CH, "%2d) %s\r\n", counter + 1, material_names[counter]);
  send_to_char("Enter material type, 0 to return: ", CH);
}


/* For extra descriptions */
void redit_disp_extradesc_menu(struct descriptor_data * d)
{
  struct extra_descr_data *extra_desc = (struct extra_descr_data *) * d->misc_data;

  send_to_char(CH, "Extra descript menu\r\n"
               "0) Quit\r\n"
               "1) Keyword: %s%s%s\r\n"
               "2) Description:\r\n%s\r\n", CCCYN(CH, C_CMP),
               extra_desc->keyword, CCNRM(CH, C_CMP),
               extra_desc->description ? DOUBLE_UP_COLOR_CODES_IF_NEEDED(extra_desc->description) : "(none)");
  send_to_char(CH, "3) %s\r\n"
               "Enter Choice:\r\n",
               (extra_desc->next ? "Another description set. (not viewed)" : "Another description"));
  d->edit_mode = REDIT_EXTRADESC_MENU;
}

const char *render_door_type_string(struct room_direction_data *door) {
  if (!IS_SET(door->exit_info, EX_ISDOOR))
    return "No door";

  if (IS_SET(door->exit_info, EX_PICKPROOF)) {
    if (IS_SET(door->exit_info, EX_ASTRALLY_WARDED))
      return "Pickproof, astrally-warded door";
    else
      return "Pickproof";
  } else {
    if (IS_SET(door->exit_info, EX_ASTRALLY_WARDED))
      return "Astrally-warded regular door";
    else
      return "Regular door";
  }
}

/* For exits */
void redit_disp_exit_menu(struct descriptor_data * d)
{
  CLS(CH);
  /* if exit doesn't exist, alloc/create it and clear it*/
  if(!DOOR)
  {
    DOOR = new room_direction_data;
    memset((char *) DOOR, 0, sizeof (struct room_direction_data));
    DOOR->barrier = 4;
    DOOR->condition = DOOR->barrier;
    DOOR->material = 5;
  }

  send_to_char(CH,      "1) Exit to: %s%d%s\r\n"
               "2) Description: %s\r\n",
               CCCYN(CH, C_CMP), DOOR->to_room_vnum, CCNRM(CH, C_CMP),
               (DOOR->general_description ? DOUBLE_UP_COLOR_CODES_IF_NEEDED(DOOR->general_description) : "(None)"));
  send_to_char(CH,      "3) Door keywords (first one is its name too): %s%s%s\r\n"
               "4) Key vnum: %s%d%s\r\n"
               "5) Door flag: %s%s%s\r\n",
               CCCYN(CH, C_CMP), (DOOR->keyword ? DOOR->keyword : "(none)"),
               CCNRM(CH, C_CMP), CCCYN(CH, C_CMP), DOOR->key, CCNRM(CH, C_CMP),
               CCCYN(CH, C_CMP), render_door_type_string(DOOR), CCNRM(CH, C_CMP));



  send_to_char(CH,        "6) Lock level: %s%d%s\r\n"
               "7) Material Type: %s%s%s\r\n"
               "8) Barrier Rating: %s%d%s\r\n",
               CCCYN(CH, C_CMP), DOOR->key_level,
               CCNRM(CH, C_CMP), CCCYN(CH, C_CMP), material_names[(int)DOOR->material],
               CCNRM(CH, C_CMP), CCCYN(CH, C_CMP), DOOR->barrier, CCNRM(CH, C_CMP));


  send_to_char(CH,       "9) Hidden Rating: %s%d%s\r\n"
               "a) Leaving-through-this-exit second-person custom message: %s%s%s\r\n"
               "b) Leaving-through-this-exit third-person custom message: %s%s%s\r\n"
               "c) Entering-from-this-exit third-person custom message: %s%s%s\r\n"
               "\r\n^rx) Purge exit.^n\r\n"
               "0) Quit\r\n"
               "Enter choice (0 to quit):",
               CCCYN(CH, C_CMP), DOOR->hidden, CCNRM(CH, C_CMP),
               CCCYN(CH, C_CMP), DOOR->go_into_secondperson, CCNRM(CH, C_CMP),
               CCCYN(CH, C_CMP), DOOR->go_into_thirdperson, CCNRM(CH, C_CMP),
               CCCYN(CH, C_CMP), DOOR->come_out_of_thirdperson, CCNRM(CH, C_CMP));
  d->edit_mode = REDIT_EXIT_MENU;
}

/* For exit flags */
void redit_disp_exit_flag_menu(struct descriptor_data * d)
{
  send_to_char( "0) No door\r\n"
                "1) Closeable door\r\n"
                "2) Pickproof\r\n"
                "3) Astrally-warded closeable door\r\n"
                "4) Astrally-warded pickproof door\r\n"
                "Enter choice:", CH);
}

/* For jackpoint */
void redit_disp_mtx_menu(struct descriptor_data * d)
{
  CLS(CH);
  send_to_char(CH, "0) To Host: ^c%ld^n\r\n", d->edit_room->matrix);
  send_to_char(CH, "1) Access Mod: ^c%d^n\r\n", d->edit_room->access);
  send_to_char(CH, "2) Trace Mod:  ^c%d^n\r\n", d->edit_room->trace);
  send_to_char(CH, "3) I/O Speed: ^c%d^n\r\n", d->edit_room->io);
  send_to_char(CH, "4) Base Bandwidth: ^c%d^n\r\n", d->edit_room->bandwidth);
  send_to_char(CH, "5) Parent RTG: ^c%ld^n\r\n", d->edit_room->rtg);
  send_to_char(CH, "6) Commlink Number: ^c%d^n\r\n", d->edit_room->jacknumber);
  send_to_char(CH, "7) Physical Address: ^c%s^n\r\n",  d->edit_room->address);
  send_to_char(CH, "q) Return to Main Menu\r\nEnter Choice: ");
  d->edit_mode = REDIT_MATRIX;
}

/* For room flags */
void redit_disp_flag_menu(struct descriptor_data * d)
{
  int             counter;

  CLS(CH);
  for (counter = 0; counter < ROOM_MAX; counter += 2)
  {
    send_to_char(CH, "%2d) %-12s      %2d) %-12s\r\n",
                 counter + 1, room_bits[counter],
                 counter + 2, counter + 1 < ROOM_MAX ?
                 room_bits[counter + 1] : "");
  }
  ROOM->room_flags.PrintBits(buf1, MAX_STRING_LENGTH, room_bits, ROOM_MAX);
  send_to_char(CH, "Room flags: %s%s%s\r\n"
               "Enter room flags, 0 to quit:", CCCYN(CH, C_CMP),
               buf1, CCNRM(CH, C_CMP));
  d->edit_mode = REDIT_FLAGS;
}

/* for sector type */
void redit_disp_sector_menu(struct descriptor_data * d)
{
  int             counter;

  CLS(CH);
  for (counter = 0; counter < NUM_SPIRITS; counter += 2)
  {
    snprintf(buf, sizeof(buf), "%2d) %-10s     %2d) %-10s\r\n",
            counter, spirits[counter].name,
            counter + 1, counter + 1 < NUM_SPIRITS ?
            spirits[counter + 1].name : "");
    send_to_char(buf, d->character);
  }
  send_to_char("Enter domain:", d->character);
  d->edit_mode = REDIT_SECTOR;
}

/* the main menu */
void redit_disp_menu(struct descriptor_data * d)
{

  CLS(CH);
  d->edit_mode = REDIT_MAIN_MENU;
  send_to_char(CH, "Room number: %s%d%s\r\n"
               "1) Room name: %s%s%s\r\n",
               CCCYN(CH, C_CMP), d->edit_number, CCNRM(CH, C_CMP),
               CCCYN(CH, C_CMP), DOUBLE_UP_COLOR_CODES_IF_NEEDED(d->edit_room->name),
               CCNRM(CH, C_CMP));
  send_to_char(CH, "2) Room Desc:\r\n%s\r\n", DOUBLE_UP_COLOR_CODES_IF_NEEDED(d->edit_room->description));
  send_to_char(CH, "3) Night Desc: \r\n%s\r\n", DOUBLE_UP_COLOR_CODES_IF_NEEDED(d->edit_room->night_desc));
  send_to_char(CH, "Room zone: %s%d%s\r\n",
               CCCYN(CH, C_CMP),
               zone_table[d->edit_room->zone].number,
               CCNRM(CH, C_CMP));
  ROOM->room_flags.PrintBits(buf2, MAX_STRING_LENGTH, room_bits, ROOM_MAX);
  send_to_char(CH, "4) Room flags: %s%s%s\r\n",
               CCCYN(CH, C_CMP),
               buf2, CCNRM(CH,
                           C_CMP));
  sprinttype(d->edit_room->sector_type, spirit_name, buf2, sizeof(buf2));
  send_to_char(CH, "5) Domain type: %s%s%s\r\n",CCCYN(CH, C_CMP), buf2, CCNRM(CH, C_CMP));

  if (d->edit_room->dir_option[NORTH])
    send_to_char(CH, "6) Exit north to:     %s%d%s\r\n", CCCYN(CH, C_CMP),
                 d->edit_room->dir_option[NORTH]->to_room_vnum, CCNRM(CH, C_CMP));
  else
    send_to_char(    "6) Exit north to:     (none)\r\n", CH);
  if (d->edit_room->dir_option[NORTHEAST])
    send_to_char(CH, "7) Exit northeast to: %s%d%s\r\n", CCCYN(CH, C_CMP),
                 d->edit_room->dir_option[NORTHEAST]->to_room_vnum, CCNRM(CH, C_CMP));
  else
    send_to_char(    "7) Exit northeast to: (none)\r\n", CH);
  if (d->edit_room->dir_option[EAST])
    send_to_char(CH, "8) Exit east to:      %s%d%s\r\n", CCCYN(CH, C_CMP),
                 d->edit_room->dir_option[EAST]->to_room_vnum, CCNRM(CH, C_CMP));
  else
    send_to_char(    "8) Exit east to:      (none)\r\n", CH);
  if (d->edit_room->dir_option[SOUTHEAST])
    send_to_char(CH, "9) Exit southeast to: %s%d%s\r\n", CCCYN(CH, C_CMP),
                 d->edit_room->dir_option[SOUTHEAST]->to_room_vnum, CCNRM(CH, C_CMP));
  else
    send_to_char(    "9) Exit southeast to: (none)\r\n", CH);
  if (d->edit_room->dir_option[SOUTH])
    send_to_char(CH, "a) Exit south to:     %s%d%s\r\n", CCCYN(CH, C_CMP),
                 d->edit_room->dir_option[SOUTH]->to_room_vnum, CCNRM(CH, C_CMP));
  else
    send_to_char(    "a) Exit south to:     (none)\r\n", CH);
  if (d->edit_room->dir_option[SOUTHWEST])
    send_to_char(CH, "b) Exit southwest to: %s%d%s\r\n", CCCYN(CH, C_CMP),
                 d->edit_room->dir_option[SOUTHWEST]->to_room_vnum, CCNRM(CH, C_CMP));
  else
    send_to_char(    "b) Exit southwest to: (none)\r\n", CH);
  if (d->edit_room->dir_option[WEST])
    send_to_char(CH, "c) Exit west to:      %s%d%s\r\n", CCCYN(CH, C_CMP),
                 d->edit_room->dir_option[WEST]->to_room_vnum, CCNRM(CH, C_CMP));
  else
    send_to_char(    "c) Exit west to:      (none)\r\n", CH);
  if (d->edit_room->dir_option[NORTHWEST])
    send_to_char(CH, "d) Exit northwest to: %s%d%s\r\n", CCCYN(CH, C_CMP),
                 d->edit_room->dir_option[NORTHWEST]->to_room_vnum, CCNRM(CH, C_CMP));
  else
    send_to_char(    "d) Exit northwest to: (none)\r\n", CH);
  if (d->edit_room->dir_option[UP])
    send_to_char(CH, "e) Exit up to:        %s%d%s\r\n", CCCYN(CH, C_CMP),
                 d->edit_room->dir_option[UP]->to_room_vnum, CCNRM(CH, C_CMP));
  else
    send_to_char(    "e) Exit up to:        (none)\r\n", CH);
  if (d->edit_room->dir_option[DOWN])
    send_to_char(CH, "f) Exit down to:      %s%d%s\r\n", CCCYN(CH, C_CMP),
                 d->edit_room->dir_option[DOWN]->to_room_vnum, CCNRM(CH, C_CMP));
  else
    send_to_char(    "f) Exit down to:      (none)\r\n", CH);
  send_to_char(    "g) Edit Jackpoint\r\n", CH);
  send_to_char(CH, "h) Light Level: ^c%s^n\r\n", light_levels[d->edit_room->vision[0]]);
  send_to_char(CH, "i) Smoke Level: ^c%s^n\r\n", light_levels[d->edit_room->vision[1]]);
  send_to_char(CH, "j) Combat Options: Crowd (^c%d^n) Cover: (^c%d^n) Room Type: (^c%s^n) X: (^c%d^n) Y: (^c%d^n) Z: (^c%.2f^n)\r\n",
               d->edit_room->crowd, d->edit_room->cover, room_types[d->edit_room->type], d->edit_room->x, d->edit_room->y, d->edit_room->z);
  send_to_char(CH, "k) Background Count: ^c%d (%s)^n\r\n",
               d->edit_room->background[PERMANENT_BACKGROUND_COUNT],
               background_types[d->edit_room->background[PERMANENT_BACKGROUND_TYPE]]);
  send_to_char(CH, "l) Extra descriptions\r\n", d->character);
  if (d->edit_room->sector_type == SPIRIT_LAKE || d->edit_room->sector_type == SPIRIT_SEA || d->edit_room->sector_type == SPIRIT_RIVER) {
    send_to_char(CH, "m) Swim test difficulty (TN): %s%d%s\r\n", CCCYN(CH, C_CMP), ROOM->rating, CCNRM(CH, C_CMP));
  }
  else if (d->edit_room->room_flags.IsSet(ROOM_FALL)) {
    send_to_char(CH, "m) Fall test difficulty (TN): %s%d%s\r\n", CCCYN(CH, C_CMP), ROOM->rating, CCNRM(CH, C_CMP));
  }
  else if (d->edit_room->room_flags.IsSet(ROOM_RADIATION)) {
    send_to_char(CH, "m) Radiation power: %s%d%s\r\n", CCCYN(CH, C_CMP), ROOM->rating, CCNRM(CH, C_CMP));
  }
  send_to_char(CH, "n) Staff Level Required to Enter: %s%d%s\r\n", CCCYN(CH, C_CMP), ROOM->staff_level_lock, CCNRM(CH, C_CMP));
  if (d->edit_convert_color_codes)
    send_to_char("t) Restore color codes\r\n", d->character);
  else
    send_to_char("t) Toggle color codes\r\n", d->character);
  send_to_char("q) Quit and save\r\n", d->character);
  send_to_char("x) Exit and abort\r\n", d->character);
  send_to_char("Enter your choice:\r\n", d->character);
}

/**************************************************************************
  The main loop
 **************************************************************************/

void redit_parse(struct descriptor_data * d, const char *arg)
{
  extern struct room_data *world;
  SPECIAL(cpu);
  SPECIAL(datastore);
  SPECIAL(input_output);
  SPECIAL(spu);
  SPECIAL(system_access);
  SPECIAL(slave);

  int             number;
  int             room_num;
  switch (d->edit_mode)
  {
  case REDIT_CONFIRM_EDIT:
    switch (*arg) {
    case 'y':
    case 'Y':
      // put the allocation over here!
      redit_disp_menu(d);
      break;
    case 'n':
    case 'N':
      /* player doesn't want to edit, free entire temp room */
      STATE(d) = CON_PLAYING;
      if (d->edit_room)
        Mem->DeleteRoom(d->edit_room);
      d->edit_room = NULL;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
        char_to_room(CH, GET_WAS_IN(CH));
        GET_WAS_IN(CH) = NULL;
      break;
    default:
      send_to_char("That's not a valid choice!\r\n", d->character);
      send_to_char("Do you wish to edit it?\r\n", d->character);
      break;
    }
    break; /* end of REDIT_CONFIRM_EDIT */
  case REDIT_CONFIRM_SAVESTRING:
    switch (*arg) {
    case 'y':
    case 'Y': {
        int counter2;
#ifdef ONLY_LOG_BUILD_ACTIONS_ON_CONNECTED_ZONES
        if (!vnum_from_non_connected_zone(d->edit_number)) {
#else
        {
#endif
          snprintf(buf, sizeof(buf),"%s wrote new room #%ld",
                  GET_CHAR_NAME(d->character), d->edit_number);
          mudlog(buf, d->character, LOG_WIZLOG, TRUE);
        }
        room_num = real_room(d->edit_number);
        if (room_num > 0) {
          /* copy people/object pointers over to the temp room
             as a temporary measure */
          d->edit_room->contents = world[room_num].contents;
          d->edit_room->people = world[room_num].people;

          // Update the peace values.
          bool edit_room_peaceful = d->edit_room->room_flags.IsSet(ROOM_PEACEFUL);
          bool world_room_peaceful = world[room_num].room_flags.IsSet(ROOM_PEACEFUL);
          if (edit_room_peaceful && !world_room_peaceful) {
            d->edit_room->peaceful += 1;
          }
          // More complex case: Room was peaceful and now is not.
          else if (world_room_peaceful && !edit_room_peaceful) {
            d->edit_room->peaceful -= 1;
            if (d->edit_room->peaceful < 0) {
              snprintf(buf, sizeof(buf), "SYSERR: Changing PEACEFUL flag of room caused world[room_num].peaceful to be %d.",
                      d->edit_room->peaceful);
              mudlog(buf, NULL, LOG_SYSLOG, TRUE);
              d->edit_room->peaceful = 0;
            }
          }

          // we use free_room here because we are not ready to turn it over
          // to the stack just yet as we are gonna use it immediately
          free_room(world + room_num);
          /* now copy everything over! */
          world[room_num] = *d->edit_room;
        } else {
          /* hm, we can't just copy.. gotta insert a new room */
          int             counter;
          int             counter2;
          int             found = 0;
          // check first if you need to resize it
          if ((top_of_world + 1) >= top_of_world_array)
            // if it cannot resize, free the edit_room and return
            if (!resize_world_array()) {
              send_to_char("Unable to save, OLC temporarily unavailable.\r\n"
                           ,CH);
              Mem->DeleteRoom(d->edit_room);
              olc_state = 0;
              d->edit_room = NULL;
              PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
              STATE(d) = CON_PLAYING;
              char_to_room(CH, GET_WAS_IN(CH));
              GET_WAS_IN(CH) = NULL;
              return;
            }


          /* count thru world tables */
          for (counter = 0; counter <= top_of_world; counter++) {
            if (!found) {
              /* check if current virtual is bigger than our virtual */
              if (world[counter].number > d->edit_number) {
                // now, zoom backwards through the list copying over
                for (counter2 = top_of_world + 1; counter2 > counter; counter2--) {
                  world[counter2] = world[counter2 - 1];
                }

                world[counter] = *(d->edit_room);
                world[counter].number = d->edit_number;
                world[counter].func = NULL;
                found = TRUE;
              }
            } else {
              // okay, it has been found and inserted
              // anything in a room after the one inserted needs their
              // real numbers increased
              struct char_data *temp_ch;
              struct obj_data *temp_obj;
              struct veh_data *temp_veh;
              for (temp_ch = world[counter].people; temp_ch;
                   temp_ch = temp_ch->next_in_room)  {
                if (temp_ch->in_room)
                  temp_ch->in_room = &world[real_room(temp_ch->in_room->number) + 1];
              }

              for (temp_veh = world[counter].vehicles; temp_veh;
                   temp_veh = temp_veh->next_veh)  {
                if (temp_veh->in_room)
                  temp_veh->in_room = &world[real_room(temp_veh->in_room->number) + 1];
              }

              /* move objects */
              for (temp_obj = world[counter].contents; temp_obj;
                   temp_obj = temp_obj->next_content)
                if (temp_obj->in_room)
                  temp_obj->in_room = &world[real_room(temp_obj->in_room->number) + 1];
            } // end else
          } // end 'insert' for-loop

          /* if place not found, insert at end */
          if (!found) {
            world[top_of_world + 1] = *d->edit_room;
            world[top_of_world + 1].number = d->edit_number;
            world[top_of_world + 1].func = NULL;
          }
          top_of_world++;
          /* now this is the *real* room_num */
          room_num = real_room(d->edit_number);

          /* now zoom through the character list and update anyone in limbo */
          struct char_data * temp_ch;
          for (temp_ch = character_list; temp_ch; temp_ch = temp_ch->next) {
            if (GET_WAS_IN(temp_ch) && real_room(GET_WAS_IN(temp_ch)->number) >= room_num)
              GET_WAS_IN(temp_ch) = &world[real_room(GET_WAS_IN(temp_ch)->number) + 1];
          }
          /* update zone tables */
          {
            int zone, cmd_no;

            for (zone = 0; zone <= top_of_zone_table; zone++)
              for (cmd_no = 0; cmd_no < zone_table[zone].num_cmds; cmd_no++)
              {
                switch (ZCMD.command) {
                case 'M':
                  ZCMD.arg3 = (ZCMD.arg3 >= room_num ? ZCMD.arg3 + 1 :
                               ZCMD.arg3);
                  break;
                case 'O':
                  if (ZCMD.arg3 != NOWHERE)
                    ZCMD.arg3 =
                      (ZCMD.arg3 >= room_num ?
                       ZCMD.arg3 + 1 : ZCMD.arg3);
                  break;
                case 'D':
                  ZCMD.arg1 =
                    (ZCMD.arg1 >= room_num ?
                     ZCMD.arg1 + 1 : ZCMD.arg1);
                  break;
                case 'R': /* rem obj from room */
                  ZCMD.arg1 =
                    (ZCMD.arg1 >= room_num ?
                     ZCMD.arg1 + 1 : ZCMD.arg1);
                  break;
                }
              }
          }

          /* update load rooms, to fix creeping load room problem */
          if (room_num <= r_mortal_start_room)
            r_mortal_start_room++;
          if (room_num <= r_immort_start_room)
            r_immort_start_room++;
          if (room_num <= r_frozen_start_room)
            r_frozen_start_room++;
          if (room_num <= r_newbie_start_room)
            r_newbie_start_room++;
          /* go through the world. if any of the old rooms indicated an exit
           * to our new room, we have to change it */
          for (counter = 0; counter <= top_of_world; counter++) {
            for (counter2 = 0; counter2 < NUM_OF_DIRS; counter2++) {
              /* if exit exists */
              if (world[counter].dir_option[counter2] && world[counter].dir_option[counter2]->to_room) {
                /* increment r_nums for rooms bigger than or equal to new one
                 * because we inserted room */
                vnum_t rnum = real_room(world[counter].dir_option[counter2]->to_room->number);
                if (rnum >= room_num)
                  world[counter].dir_option[counter2]->to_room = &world[rnum + 1];
                /* if an exit to the new room is indicated, change to_room */
                if (world[counter].dir_option[counter2]->to_room_vnum == d->edit_number)
                  world[counter].dir_option[counter2]->to_room = &world[room_num];
              }
            }
          }
        } // end 'insert' else
        /* resolve all vnum doors to rnum doors in the newly edited room */
        struct room_data *opposite = NULL;
        for (counter2 = 0; counter2 < NUM_OF_DIRS; counter2++) {
          if (world[room_num].dir_option[counter2]) {
            vnum_t rnum = real_room(world[room_num].dir_option[counter2]->to_room_vnum);
            if (rnum != NOWHERE)
              world[room_num].dir_option[counter2]->to_room = &world[rnum];
            else {
              world[room_num].dir_option[counter2]->to_room = &world[0];
            }
            if (counter2 < NUM_OF_DIRS) {
              opposite = world[room_num].dir_option[counter2]->to_room;
              if (opposite && opposite->dir_option[rev_dir[counter2]] && opposite->dir_option[rev_dir[counter2]]->to_room == &world[room_num]) {
                opposite->dir_option[rev_dir[counter2]]->material = world[room_num].dir_option[counter2]->material;
                opposite->dir_option[rev_dir[counter2]]->barrier = world[room_num].dir_option[counter2]->barrier;
                opposite->dir_option[rev_dir[counter2]]->condition = world[room_num].dir_option[counter2]->condition;
              }
            }
          }
        }
        send_to_char("Writing room to disk.\r\n", d->character);
        write_world_to_disk(d->character->player_specials->saved.zonenum);
        send_to_char("Saved.\r\n", CH);
        /* do NOT free strings! just the room structure */
        Mem->ClearRoom(d->edit_room);
        char_to_room(CH, GET_WAS_IN(CH));
        GET_WAS_IN(CH) = NULL;
        STATE(d) = CON_PLAYING;
        clear_editing_data(d);
        send_to_char("Done.\r\n", d->character);
        break;
      }
    case 'n':
    case 'N':
      send_to_char("Room not saved, aborting.\r\n", d->character);
      /* free everything up, including strings etc */
      if (d->edit_room)
        Mem->DeleteRoom(d->edit_room); // this is set to NULL in clear_editing_data().
      char_to_room(CH, GET_WAS_IN(CH));
      GET_WAS_IN(CH) = NULL;
      STATE(d) = CON_PLAYING;
      clear_editing_data(d);
      break;
    default:
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char("Do you wish to save this room internally?\r\n", d->character);
      break;
    }
    break; /* end of REDIT_CONFIRM_SAVESTRING */
  case REDIT_MAIN_MENU:
    switch (LOWER(*arg)) {
    case 'q':
      d->edit_mode = REDIT_CONFIRM_SAVESTRING;
      redit_parse(d, "y");
      break;
    case 'x':
      d->edit_mode = REDIT_CONFIRM_SAVESTRING;
      redit_parse(d, "n");
      break;
      case 't':
        if ((d->edit_convert_color_codes = !d->edit_convert_color_codes))
          send_to_char("OK, color codes for title/desc will be displayed as tags for easier copy-paste.\r\n", CH);
        else
          send_to_char("OK, color codes will be re-enabled.\r\n", CH);
        redit_disp_menu(d);
      break;
    case '1':
      send_to_char("Enter room name:", d->character);
      d->edit_mode = REDIT_NAME;
      break;
    case '2':
      send_to_char("Enter room description:\r\n", d->character);
      d->edit_mode = REDIT_DESC;
      DELETE_D_STR_IF_EXTANT(d);
      INITIALIZE_NEW_D_STR(d);
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      break;
    case '3':
      send_to_char("Enter room nighttime desc description:\r\n", d->character);
      d->edit_mode = REDIT_NDESC;
      DELETE_D_STR_IF_EXTANT(d);
      INITIALIZE_NEW_D_STR(d);
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      break;
    case '4':
      redit_disp_flag_menu(d);
      break;
    case '5':
      redit_disp_sector_menu(d);
      break;
    case '6':
      d->edit_number2 = NORTH;
      redit_disp_exit_menu(d);
      break;
    case '7':
      d->edit_number2 = NORTHEAST;
      redit_disp_exit_menu(d);
      break;
    case '8':
      d->edit_number2 = EAST;
      redit_disp_exit_menu(d);
      break;
    case '9':
      d->edit_number2 = SOUTHEAST;
      redit_disp_exit_menu(d);
      break;
    case 'a':
      d->edit_number2 = SOUTH;
      redit_disp_exit_menu(d);
      break;
    case 'b':
      d->edit_number2 = SOUTHWEST;
      redit_disp_exit_menu(d);
      break;
    case 'c':
      d->edit_number2 = WEST;
      redit_disp_exit_menu(d);
      break;
    case 'd':
      d->edit_number2 = NORTHWEST;
      redit_disp_exit_menu(d);
      break;
    case 'e':
      d->edit_number2 = UP;
      redit_disp_exit_menu(d);
      break;
    case 'f':
      d->edit_number2 = DOWN;
      redit_disp_exit_menu(d);
      break;
    case 'g':
      redit_disp_mtx_menu(d);
      break;
    case 'h':
      redit_disp_light_menu(d, 1);
      break;
    case 'i':
      redit_disp_light_menu(d, 0);
      break;
    case 'j':
      redit_disp_combat_menu(d);
      break;
    case 'k':
      send_to_char("Enter Background Count: ", CH);
      d->edit_mode = REDIT_BACKGROUND;
      break;
    case 'l':
      /* if extra desc doesn't exist . */
      if (!d->edit_room->ex_description) {
        d->edit_room->ex_description = new extra_descr_data;
        memset((char *) d->edit_room->ex_description, 0,
               sizeof(struct extra_descr_data));
      }
      d->misc_data = (void **) &(d->edit_room->ex_description);
      redit_disp_extradesc_menu(d);
      break;
      // new stuff here
    case 'm':
      if (ROOM->sector_type == SPIRIT_LAKE || ROOM->sector_type == SPIRIT_SEA || ROOM->sector_type == SPIRIT_RIVER || d->edit_room->room_flags.AreAnySet(ROOM_FALL, ROOM_RADIATION, ENDBIT)) {
        send_to_char("Enter environmental difficulty rating (1 to 20): ", CH);
        d->edit_mode = REDIT_LIBRARY_RATING;
      } else {
        redit_disp_menu(d);
        return;
      }
      break;
    case 'n':
      if (ROOM->staff_level_lock > GET_REAL_LEVEL(CH)) {
        send_to_char("The lock level for this room is higher than your lock level-- you can't change it.\r\n", CH);
        redit_disp_menu(d);
        return;
      }
      send_to_char("Enter the privilege level a character must have to enter this room (0: NPC; 1: Player; X>1: Staff rank X): ", CH);
      d->edit_mode = REDIT_STAFF_LOCK_LEVEL;
      break;
    default:
      send_to_char("Invalid choice!\r\n", d->character);
      redit_disp_menu(d);
      break;
    }
    break;
  case REDIT_MATRIX:
    switch (LOWER(*arg)) {
    case '0':
      send_to_char(CH, "Enter Destination Host: ");
      d->edit_mode = REDIT_HOST;
      break;
    case '1':
      send_to_char(CH, "Enter Access Modifier (may not be implemented?): ");
      d->edit_mode = REDIT_ACCESS;
      break;
    case '2':
      send_to_char(CH, "Enter Trace Modifier (higher values make it harder to trace the player): ");
      d->edit_mode = REDIT_TRACE;
      break;
    case '3':
      send_to_char(CH, "Enter I/O Speed Limit (-1 For Tap, 0 For Unlimited): ");
      d->edit_mode = REDIT_IO;
      break;
    case '4':
      send_to_char(CH, "Enter Base Bandwidth (-1 For Tap, 0 For Unlimited) (not implemented?): ");
      d->edit_mode = REDIT_BASE;
      break;
    case '5':
      send_to_char(CH, "Enter Parent RTG: ");
      d->edit_mode = REDIT_PARENT;
      break;
    case '6':
      send_to_char(CH, "Enter Commlink Number (8 numbers): ");
      d->edit_mode = REDIT_COMMLINK;
      break;
    case '7':
      send_to_char(CH, "Enter Physical Address Description: ");
      d->edit_mode = REDIT_ADDRESS;
      break;
    case 'q':
      redit_disp_menu(d);
      break;
    default:
      send_to_char("Invalid choice! Enter Choice: ", d->character);
      break;
    }
    break;
  case REDIT_HOST:
    number = atoi(arg);
    if (real_host(number) < 0)
      send_to_char("Invalid host!\r\nEnter destination host: ", CH);
    else {
      d->edit_room->matrix = number;
      redit_disp_mtx_menu(d);
    }
    break;
  case REDIT_PARENT:
    number = atoi(arg);
    if (real_host(number) < 0)
      send_to_char("Invalid host!\r\nEnter Parent RTG: ", CH);
    else {
      d->edit_room->rtg = number;
      redit_disp_mtx_menu(d);
    }
    break;
  case REDIT_COMMLINK:
    d->edit_room->jacknumber = atoi(arg);
    redit_disp_mtx_menu(d);
    break;
  case REDIT_IO:
    if ((number = atoi(arg)) < -1)
      send_to_char("Invalid host!\r\nEnter I/O Speed: ", CH);
    else {
      d->edit_room->io = number;
      redit_disp_mtx_menu(d);
    }
    break;
  case REDIT_BASE:
    if ((number = atoi(arg)) < -1)
      send_to_char("Invalid host!\r\nEnter Base Bandwidth: ", CH);
    else {
      d->edit_room->bandwidth = number;
      redit_disp_mtx_menu(d);
    }
    break;
  case REDIT_ACCESS:
    d->edit_room->access = atoi(arg);
    redit_disp_mtx_menu(d);
    break;
  case REDIT_TRACE:
    d->edit_room->trace = atoi(arg);
    redit_disp_mtx_menu(d);
    break;
  case REDIT_ADDRESS:
    if (d->edit_room->address)
      delete [] d->edit_room->address;
    d->edit_room->address = str_dup(arg);
    redit_disp_mtx_menu(d);
    break;
  case REDIT_BACKGROUND:
    number = MAX(MIN(10, atoi(arg)), 0);
    d->edit_room->background[CURRENT_BACKGROUND_COUNT] = d->edit_room->background[PERMANENT_BACKGROUND_COUNT] = number;
    for (number = 0; number < AURA_PLAYERCOMBAT; number++)
      send_to_char(CH, "%d) %s\r\n", number, background_types[number]);
    send_to_char("Enter background count type: ", CH);
    d->edit_mode = REDIT_BACKGROUND2;
    break;
  case REDIT_BACKGROUND2:
    number = atoi(arg);
    if (number < 0 || number >= AURA_PLAYERCOMBAT) {
      send_to_char(CH, "Number must be between 0 and %d. Enter Type: ", AURA_PLAYERCOMBAT - 1);
      return;
    }
    d->edit_room->background[CURRENT_BACKGROUND_TYPE] = d->edit_room->background[PERMANENT_BACKGROUND_TYPE] = number;
    redit_disp_menu(d);
    break;
  case REDIT_NAME:
    if (d->edit_room->name)
      delete [] d->edit_room->name;
    d->edit_room->name = str_dup(arg);
    redit_disp_menu(d);
    break;
  case REDIT_DESC:
    /* we will NEVER get here */
    break;
  case REDIT_FLAGS:
    number = atoi(arg);
    if ((number < 0) || (number > ROOM_MAX)) {
      send_to_char("That's not a valid choice!\r\n", d->character);
      redit_disp_flag_menu(d);
#ifndef DEATH_FLAGS
    } else if (number == ROOM_DEATH + 1) {
      send_to_char("Sorry, death flags have been disabled in this game.\r\n", d->character);
      ROOM->room_flags.RemoveBit(number-1);
      redit_disp_flag_menu(d);
#endif
    } else {
      if (number == 0)
        /* back out */
        redit_disp_menu(d);
      else {
        /* toggle bits */

        if (number-1 == ROOM_NO_TRAFFIC) {
          send_to_char("!TRAFFIC changes will only take effect on copyover.\r\n", d->character);
        }

        if (ROOM->room_flags.IsSet(number-1)) {
          ROOM->room_flags.RemoveBit(number-1);
          if (number-1 == ROOM_STERILE) {
            send_to_char("Automatically removing sterile aura.\r\n", d->character);
            d->edit_room->background[CURRENT_BACKGROUND_COUNT] = d->edit_room->background[PERMANENT_BACKGROUND_COUNT] = 0;
            d->edit_room->background[CURRENT_BACKGROUND_TYPE] = d->edit_room->background[PERMANENT_BACKGROUND_TYPE] = 0;
          }
        } else {
          ROOM->room_flags.SetBit(number-1);
          if (number-1 == ROOM_STERILE) {
            send_to_char("Automatically setting sterile aura.\r\n", d->character);
            d->edit_room->background[CURRENT_BACKGROUND_COUNT] = d->edit_room->background[PERMANENT_BACKGROUND_COUNT] = 1;
            d->edit_room->background[CURRENT_BACKGROUND_TYPE] = d->edit_room->background[PERMANENT_BACKGROUND_TYPE] = AURA_STERILITY;
          }
        }
        redit_disp_flag_menu(d);
      }
    }
    break;
  case REDIT_COMBAT:
    switch (*arg) {
      case '1':
        send_to_char("Amount of crowd (0-None, 10-Busy City Street): ", CH);
        d->edit_mode = REDIT_CROWD;
        break;
      case '2':
        send_to_char("Amount of cover (0-None, 10-Can Barely Move): ", CH);
        d->edit_mode = REDIT_COVER;
        break;
      case '3':
        CLS(CH);
        for (int i = 0; i < 11; i++)
          send_to_char(CH, "%2d) %s\r\n", i, room_types[i]);
        send_to_char("Room Type: ", CH);
        d->edit_mode = REDIT_TYPE;
        break;
      case '4':
        send_to_char("Room Length: ", CH);
        d->edit_mode = REDIT_X;
        break;
      case '5':
        send_to_char("Room Width: ", CH);
        d->edit_mode = REDIT_Y;
        break;
      case '6':
        send_to_char("Room Height in meters: ", CH);
        d->edit_mode = REDIT_Z;
        break;
      default:
        redit_disp_menu(d);
    }
    break;
  case REDIT_COVER:
    number = atoi(arg);
    if (number >= 0 && number <= 10)
      d->edit_room->cover = number;
    redit_disp_combat_menu(d);
    break;
  case REDIT_CROWD:
    number = atoi(arg);
    if (number >= 0 && number <= 10)
      d->edit_room->crowd = number;
    redit_disp_combat_menu(d);
    break;
  case REDIT_TYPE:
    number = atoi(arg);
    if (number >= 0 && number < 11)
      d->edit_room->type = number;
    redit_disp_combat_menu(d);
    break;
  case REDIT_X:
    d->edit_room->x = atoi(arg);
    redit_disp_combat_menu(d);
    break;
  case REDIT_Y:
    d->edit_room->y = atoi(arg);
    redit_disp_combat_menu(d);
    break;
  case REDIT_Z:
    d->edit_room->z = atof(arg);
    redit_disp_combat_menu(d);
    break;
  case REDIT_LIGHT:
    number = atoi(arg);
    if (number < 0 || number >= 5) {
      send_to_char("Invalid input. Select Light Options:", CH);
      return;
    } else {
      d->edit_room->vision[0] = number;
      redit_disp_menu(d);
    }
    break;
  case REDIT_SMOKE:
    number = atoi(arg);
    if (number == 0) {
      d->edit_room->vision[1] = 0;
      redit_disp_menu(d);
    } else if (number < 1 || number > 5) {
      send_to_char("Invalid input. Select Light Options:", CH);
      return;
    } else {
      d->edit_room->vision[1] = number + 4;
      redit_disp_menu(d);
    }
    break;
  case REDIT_SECTOR:
    number = atoi(arg);
    if (number < 0 || number >= NUM_SPIRITS) {
      send_to_char("Invalid choice!\r\n", d->character);
      redit_disp_sector_menu(d);
    } else {
      d->edit_room->sector_type = number;
      redit_disp_menu(d);
    }
    break;
  case REDIT_EXIT_MENU:
    switch (*arg) {
    case '0':
      redit_disp_menu(d);
      break;
    case '1':
      d->edit_mode = REDIT_EXIT_NUMBER;
      send_to_char("Exit to room number:", d->character);
      break;
    case '2':
      d->edit_mode = REDIT_EXIT_DESCRIPTION;
      DELETE_D_STR_IF_EXTANT(d);
      INITIALIZE_NEW_D_STR(d);
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      send_to_char("Enter exit description:\r\n", d->character);
      break;
    case '3':
      d->edit_mode = REDIT_EXIT_KEYWORD;
      send_to_char("Enter keywords:", d->character);
      break;
    case '4':
      d->edit_mode = REDIT_EXIT_KEY;
      send_to_char("Enter key number:", d->character);
      break;
    case '5':
      redit_disp_exit_flag_menu(d);
      d->edit_mode = REDIT_EXIT_DOORFLAGS;
      break;
    case '6':
      d->edit_mode = REDIT_EXIT_KEY_LEV;
      send_to_char("Enter lock level:", CH);
      break;
    case '7':
      d->edit_mode = REDIT_EXIT_MATERIAL;
      redit_disp_material_menu(d);
      break;
    case '8':
      d->edit_mode = REDIT_EXIT_BARRIER;
      redit_disp_barrier_menu(d);
      break;
    case '9':
      d->edit_mode = REDIT_EXIT_HIDDEN;
      send_to_char("Enter hidden rating of the exit: ", CH);
      break;
    case 'a':
        d->edit_mode = REDIT_EXIT_ENTRY_STRING_SECONDPERSON;
        send_to_char("Enter the custom second-person string that displays when your character leaves through this exit. Make sure to include the direction in the string. Ex: 'You get on your hands and knees and crawl into the eastern shaft.' If you want no custom string, just hit enter now.\r\n", d->character);
        break;
    case 'b':
        d->edit_mode = REDIT_EXIT_ENTRY_STRING_THIRDPERSON;
        send_to_char("Enter the custom third-person string that displays when a character leaves through this exit. Make sure to include the direction in the string. Use '$n' for their name, '$s' for his/her/its, '$m' for him/her/it. Example: '$n gets on $s hands and knees and crawls into the eastern shaft.' If you want no custom string, just hit enter now.\r\n", d->character);
        break;
    case 'c':
        d->edit_mode = REDIT_EXIT_EXIT_STRING_THIRDPERSON;
        send_to_char("Enter the custom third-person string that displays when a character enters through this exit. Make sure to include the direction in the string. Use '$n' for their name, '$s' for his/her/its, '$m' for him/her/it. Example: '$n crawls on $s hands and knees out of the eastern shaft.' If you want no custom string, just hit enter now.\r\n", d->character);
      break;
    case 'x':
      /* delete exit */
      DELETE_ARRAY_IF_EXTANT(DOOR->keyword);
      DELETE_ARRAY_IF_EXTANT(DOOR->general_description);
      DELETE_ARRAY_IF_EXTANT(DOOR->go_into_thirdperson);
      DELETE_ARRAY_IF_EXTANT(DOOR->go_into_secondperson);
      DELETE_ARRAY_IF_EXTANT(DOOR->come_out_of_thirdperson);
      DELETE_AND_NULL(DOOR);
      redit_disp_menu(d);
      break;
    default:
      send_to_char("Invalid choice!\r\n", d->character);
      break;
    }
    break;
  case REDIT_EXIT_NUMBER:
    number = atoi(arg);
    if (number < 0)
      send_to_char("Invalid choice!\r\nExit to room number:", d->character);
    else {
      DOOR->to_room_vnum = number;
      redit_disp_exit_menu(d);
    }
    break;
  case REDIT_EXIT_DESCRIPTION:
    /* we should NEVER get here */
    break;
  case REDIT_EXIT_KEYWORD:
    DELETE_ARRAY_IF_EXTANT(DOOR->keyword);
    DOOR->keyword = str_dup(arg);
    redit_disp_exit_menu(d);
    break;
  case REDIT_EXIT_KEY:
    number = atoi(arg);
    DOOR->key = number;
    redit_disp_exit_menu(d);
    break;
  case REDIT_EXIT_KEY_LEV:
    number = atoi(arg);
    DOOR->key_level = MAX(0, MIN(50, number));
    redit_disp_exit_menu(d);
    break;
  case REDIT_EXIT_HIDDEN:
    number = atoi(arg);
    DOOR->hidden = MAX(0, MIN(50, number));
    redit_disp_exit_menu(d);
    break;
  case REDIT_EXIT_MATERIAL:
    number = atoi(arg);
    if (number == 0)
      redit_disp_exit_menu(d);
    else if (number > 0 && number <= NUM_MATERIALS) {
      DOOR->material = number - 1;
      redit_disp_exit_menu(d);
    } else
      redit_disp_material_menu(d);
    break;
  case REDIT_EXIT_BARRIER:
    number = atoi(arg);
    if (number == 0)
      redit_disp_exit_menu(d);
    else if (number > 0 && number <= NUM_BARRIERS) {
      DOOR->condition = DOOR->barrier = barrier_ratings[number - 1];
      redit_disp_exit_menu(d);
    } else
      redit_disp_barrier_menu(d);
    break;
  case REDIT_EXIT_ENTRY_STRING_SECONDPERSON:
    // What's displayed to you when you walk into the exit.
    DELETE_ARRAY_IF_EXTANT(DOOR->go_into_secondperson);
    if (*arg) {
      strcpy(buf, arg);
      delete_doubledollar(buf);
      DOOR->go_into_secondperson = str_dup(buf);
    }
    redit_disp_exit_menu(d);
    break;
  case REDIT_EXIT_ENTRY_STRING_THIRDPERSON:
    // What's displayed to others when you walk into the exit.
    DELETE_ARRAY_IF_EXTANT(DOOR->go_into_thirdperson);
    if (arg && *arg) {
      strcpy(buf, arg);
      delete_doubledollar(buf);
      DOOR->go_into_thirdperson = str_dup(buf);
    }
    redit_disp_exit_menu(d);
  break;
  case REDIT_EXIT_EXIT_STRING_THIRDPERSON:
    // What's displayed when you walk out of the exit into the room.
    DELETE_ARRAY_IF_EXTANT(DOOR->come_out_of_thirdperson);
    if (*arg) {
      strcpy(buf, arg);
      delete_doubledollar(buf);
      DOOR->come_out_of_thirdperson = str_dup(buf);
    }
    redit_disp_exit_menu(d);
    break;

  case REDIT_LIBRARY_RATING:
    number = atoi(arg);
    if ((number < 0) || (number > 20)) {
      send_to_char("Value must be between 0 and 20.\r\n", CH);
      send_to_char("Enter current rating: ", CH);
    } else {
      ROOM->rating = number;
      redit_disp_menu(d);
    }
    break;

  case REDIT_STAFF_LOCK_LEVEL:
    number = atoi(arg);
    if ((number < 0) || (number > 10)) {
      send_to_char("Value must be between 0 and 10.\r\n", CH);
      send_to_char("Enter staff lock level: ", CH);
    } else if (number > GET_REAL_LEVEL(CH)) {
      send_to_char("You can't set a lock level greater than your own level.\r\n", CH);
    } else {
      ROOM->staff_level_lock = number;
      redit_disp_menu(d);
    }
    break;

  case REDIT_EXIT_DOORFLAGS:
    number = atoi(arg);
    if ((number < 0) || (number > 4)) {
      send_to_char("That's not a valid choice!\r\n", d->character);
      redit_disp_exit_flag_menu(d);
    } else {
      /* doors are a bit idiotic, don't you think? :) */
      /* yep -LS */
      if (number == 0)
        DOOR->exit_info = 0;
      else if (number == 1)
        DOOR->exit_info = EX_ISDOOR;
      else if (number == 2)
        DOOR->exit_info = EX_ISDOOR | EX_PICKPROOF;
      else if (number == 3)
        DOOR->exit_info = EX_ISDOOR | EX_ASTRALLY_WARDED;
      else if (number == 4)
        DOOR->exit_info = EX_ISDOOR | EX_PICKPROOF | EX_ASTRALLY_WARDED;
      /* jump out to menu */
      redit_disp_exit_menu(d);
    }
    break;
  case REDIT_EXTRADESC_KEY:
    DELETE_ARRAY_IF_EXTANT((((struct extra_descr_data *) *d->misc_data)->keyword));
    ((struct extra_descr_data *) * d->misc_data)->keyword = str_dup(arg);
    redit_disp_extradesc_menu(d);
    break;
  case REDIT_EXTRADESC_MENU:
    number = atoi(arg);
    switch (number) {
    case 0: {
#define MISCDATA ((struct extra_descr_data *) * d->misc_data)
      /* if something got left out, delete the extra desc
       when backing out to menu */
        if (!MISCDATA->keyword || !MISCDATA->description) {
          DELETE_ARRAY_IF_EXTANT(MISCDATA->keyword);
          DELETE_ARRAY_IF_EXTANT(MISCDATA->description);

          /* Null out the ex_description linked list pointer to this object. */
          struct extra_descr_data *temp = d->edit_room->ex_description, *next = NULL;
          if (temp == MISCDATA) {
            d->edit_room->ex_description = NULL;
          } else {
            for (; temp; temp = next) {
              next = temp->next;
              if (next == MISCDATA) {
                if (next->next) {
                  temp->next = next->next;
                } else {
                  temp->next = NULL;
                }
                break;
              }
            }
          }

          delete MISCDATA;
          *d->misc_data = NULL;
        }
#undef MISCDATA
        /* else, we don't need to do anything.. jump to menu */
        redit_disp_menu(d);
      }
      break;
    case 1:
      d->edit_mode = REDIT_EXTRADESC_KEY;
      send_to_char("Enter keywords, separated by spaces:", d->character);
      break;
    case 2:
      d->edit_mode = REDIT_EXTRADESC_DESCRIPTION;
      send_to_char("Enter extra description:\r\n", d->character);
      DELETE_D_STR_IF_EXTANT(d);
      INITIALIZE_NEW_D_STR(d);
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      break;
    case 3:
      if (!((struct extra_descr_data *) *d->misc_data)->keyword ||
          !((struct extra_descr_data *) *d->misc_data)->description) {
        send_to_char("You can't edit the next extra desc without completing this one.\r\n", d->character);
        redit_disp_extradesc_menu(d);
      } else {
        struct extra_descr_data *new_extra;

        if (((struct extra_descr_data *) * d->misc_data)->next)
          d->misc_data = (void **) &((struct extra_descr_data *) * d->misc_data)->next;
        else {
          /* make new extra, attach at end */
          new_extra = new extra_descr_data;
          memset((char *) new_extra, 0, sizeof(extra_descr_data));
          ((struct extra_descr_data *) * d->misc_data)->next = new_extra;
          d->misc_data = (void **) &((struct extra_descr_data *) * d->misc_data)->next;
        }
        redit_disp_extradesc_menu(d);
      }
    }
    break;
  default:
    /* we should never get here */
    break;
  }
}

// world saving routine
#define PRINT_TO_FILE_IF_TRUE(section, value) { if (value) { fprintf(fp, (section), (value)); } }
#define RM world[realcounter]
void write_world_to_disk(int vnum)
{
  long             counter, counter2, realcounter;
  int             znum = real_zone(vnum);
  FILE           *fp;
  struct extra_descr_data *ex_desc;
  bool wrote_something = FALSE;

  // ideally, this would just fill a VTable with vals...maybe one day

  snprintf(buf, sizeof(buf), "%s/%d.wld", WLD_PREFIX,
          zone_table[znum].number);
  fp = fopen(buf, "w+");
  for (counter = zone_table[znum].number * 100;
       counter <= zone_table[znum].top; counter++) {
    realcounter = real_room(counter);

    if (realcounter >= 0) {
      if (!strcmp(STRING_ROOM_TITLE_UNFINISHED, RM.name))
        continue;

      wrote_something = TRUE;
      fprintf(fp, "#%ld\n", counter);

      fprintf(fp, "Name:\t%s\n",
              RM.name ? RM.name : STRING_ROOM_TITLE_UNFINISHED);
      fprintf(fp, "Desc:$\n%s~\n",
              cleanup(buf2, RM.description ? RM.description :
                      STRING_ROOM_DESC_UNFINISHED));
      if (RM.night_desc)
        fprintf(fp, "NightDesc:$\n%s~\n", cleanup(buf2, RM.night_desc));

      fprintf(fp, "Flags:\t%s\n", RM.room_flags.ToString());

      if (RM.sector_type != DEFAULT_SECTOR_TYPE)
        fprintf(fp, "SecType:\t%s\n", spirit_name[RM.sector_type]);

      PRINT_TO_FILE_IF_TRUE("MatrixExit:\t%ld\n", RM.matrix);

      if (real_host(RM.matrix) > 0)
        fprintf(fp, "IO:\t%d\n"
                "Bandwidth:\t%d\n"
                "Access:\t%d\n"
                "Trace:\t%d\n"
                "RTG:\t%ld\n"
                "JackID:\t%d\n"
                "Address:\t%s\n",
                RM.io, RM.bandwidth, RM.access, RM.trace, RM.rtg, RM.jacknumber, RM.address);

      PRINT_TO_FILE_IF_TRUE("Crowd:\t%d\n", RM.crowd);
      PRINT_TO_FILE_IF_TRUE("Cover:\t%d\n", RM.cover);

      if (RM.x != DEFAULT_DIMENSIONS_X)
        fprintf(fp, "X:\t%d\n", RM.x);

      if (RM.y != DEFAULT_DIMENSIONS_Y)
        fprintf(fp, "Y:\t%d\n", RM.y);

      if (RM.z != DEFAULT_DIMENSIONS_Z)
        fprintf(fp, "Z:\t%.2f\n", RM.z);

      PRINT_TO_FILE_IF_TRUE("RoomType:\t%d\n",  RM.type);

      fprintf(fp, "[POINTS]\n");

      PRINT_TO_FILE_IF_TRUE("\tRating:\t%d\n", RM.rating);
      PRINT_TO_FILE_IF_TRUE("\tSpecIdx:\t%d\n", RM.spec);
      PRINT_TO_FILE_IF_TRUE("\tLight:\t%d\n", RM.vision[0]);
      PRINT_TO_FILE_IF_TRUE("\tSmoke:\t%d\n", RM.vision[1]);

      if (RM.background[PERMANENT_BACKGROUND_COUNT] > 0) {
        fprintf(fp, "\tBackground:\t%d\n"
                    "\tBackgroundType:\t%d\n",
                    RM.background[PERMANENT_BACKGROUND_COUNT],
                    RM.background[PERMANENT_BACKGROUND_TYPE]);
      }

      PRINT_TO_FILE_IF_TRUE("\tStaffLockLevel:\t%d\n", RM.staff_level_lock);

      for (counter2 = 0; counter2 < NUM_OF_DIRS; counter2++) {
        room_direction_data *ptr = RM.dir_option[counter2];

        if (ptr) {
          int             temp_door_flag;

          fprintf(fp, "[EXIT %s]\n", fulldirs[counter2]);

          PRINT_TO_FILE_IF_TRUE("\tKeywords:\t%s\n", ptr->keyword);

          if (ptr->general_description)
            fprintf(fp, "\tDesc:$\n%s~\n",
                    cleanup(buf2, ptr->general_description));

          /* door flags need special handling, unfortunately. argh! */
          if (IS_SET(ptr->exit_info, EX_ISDOOR)) {
            if (IS_SET(ptr->exit_info, EX_ASTRALLY_WARDED)) {
              if (IS_SET(ptr->exit_info, EX_PICKPROOF))
                temp_door_flag = 4;
              else
                temp_door_flag = 3;
            } else {
              if (IS_SET(ptr->exit_info, EX_PICKPROOF))
                temp_door_flag = 2;
              else
                temp_door_flag = 1;
            }
          } else
            temp_door_flag = 0;

          fprintf(fp, "\tToVnum:\t%ld\n", ptr->to_room_vnum);

          PRINT_TO_FILE_IF_TRUE("\tFlags:\t%d\n", temp_door_flag);

          if (ptr->material != DEFAULT_EXIT_MATERIAL)
            fprintf(fp, "\tMaterial:\t%s\n", material_names[(int)ptr->material]);

          if (ptr->barrier != DEFAULT_EXIT_BARRIER_RATING)
            fprintf(fp, "\tBarrier:\t%d\n", ptr->barrier);

          if (DBIndex::IsValidV(ptr->key))
            fprintf(fp, "\tKeyVnum:\t%ld\n", ptr->key);

          if (ptr->key_level > 0)
            fprintf(fp, "\tLockRating:\t%d\n", ptr->key_level);

          if (ptr->hidden > 0)
            fprintf(fp, "\tHiddenRating:\t%d\n", MIN(ptr->hidden, MAX_EXIT_HIDDEN_RATING));

          // Add the new custom entry / exit strings.
          PRINT_TO_FILE_IF_TRUE("\tGoIntoSecondPerson:$\n%s~\n", ptr->go_into_secondperson);
          PRINT_TO_FILE_IF_TRUE("\tGoIntoThirdPerson:$\n%s~\n", ptr->go_into_thirdperson);
          PRINT_TO_FILE_IF_TRUE("\tComeOutOfThirdPerson:$\n%s~\n", ptr->come_out_of_thirdperson);
        }
      }
      if (RM.ex_description) {
        int y = 0;
        for (ex_desc = RM.ex_description; ex_desc; ex_desc = ex_desc->next) {
          if (ex_desc->keyword && ex_desc->description) {
            fprintf(fp,
                    "[EXTRADESC %d]\n"
                    "\tKeywords:\t%s\n"
                    "\tDesc:$\n%s~\n",
                    y, ex_desc->keyword,
                    cleanup(buf2, ex_desc->description));
          }
          y++;
        }
      }
      fprintf(fp, "BREAK\n");
    }
  }

  fprintf(fp, "END\n");
  fclose(fp);

  if (wrote_something)
    write_index_file("wld");
  else
    remove(buf);
  /* do NOT free strings! just the room structure */
}
#undef RM
#undef PRINT_TO_FILE_IF_TRUE
