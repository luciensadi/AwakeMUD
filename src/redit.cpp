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

extern class memoryClass *Mem;

#define ROOM d->edit_room

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

  for (register int counter = 0; counter < NUM_BARRIERS; ++counter)
    send_to_char(CH, "%2d) %s\r\n", counter + 1, barrier_names[counter]);
  send_to_char("Enter construction category, 0 to return: ", CH);
}

void redit_disp_material_menu(struct descriptor_data *d)
{
  CLS(CH);

  for (register int counter = 0; counter < NUM_MATERIALS; ++counter)
    send_to_char(CH, "%2d) %s\r\n", counter + 1, material_names[counter]);
  send_to_char("Enter material type, 0 to return: ", CH);
}


/* For extra descriptions */
void redit_disp_extradesc_menu(struct descriptor_data * d)
{
  struct extra_descr_data *extra_desc =
          (struct extra_descr_data *) * d->misc_data;

  send_to_char(CH, "Extra descript menu\r\n"
               "0) Quit\r\n"
               "1) Keyword: %s%s%s\r\n"
               "2) Description:\r\n%s\r\n", CCCYN(CH, C_CMP),
               extra_desc->keyword, CCNRM(CH, C_CMP),
               (extra_desc->description ? extra_desc->description : "(none)"));
  send_to_char(CH, "3) %s\r\n"
               "Enter Choice:\r\n",
               (extra_desc->next ? "Another description set. (not viewed)" : "Another description"));
  d->edit_mode = REDIT_EXTRADESC_MENU;
}

/* For exits */
void redit_disp_exit_menu(struct descriptor_data * d)
{
  CLS(CH);
#define DOOR d->edit_room->dir_option[d->edit_number2]
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
               (DOOR->general_description ? DOOR->general_description : "(None)"));
  send_to_char(CH,      "3) Door names: %s%s%s\r\n"
               "4) Key vnum: %s%d%s\r\n"
               "5) Door flag: %s%s%s\r\n",
               CCCYN(CH, C_CMP), (DOOR->keyword ? DOOR->keyword : "(none)"),
               CCNRM(CH, C_CMP), CCCYN(CH, C_CMP), DOOR->key, CCNRM(CH, C_CMP),
               CCCYN(CH, C_CMP), (IS_SET(DOOR->exit_info, EX_ISDOOR) ?
                                  (IS_SET(DOOR->exit_info, EX_PICKPROOF) ? "Pickproof" : "Regular door") :
                                      "No door"), CCNRM(CH, C_CMP));

  send_to_char(CH,        "6) Lock level: %s%d%s\r\n"
               "7) Material Type: %s%s%s\r\n"
               "8) Barrier Rating: %s%d%s\r\n",
               CCCYN(CH, C_CMP), DOOR->key_level,
               CCNRM(CH, C_CMP), CCCYN(CH, C_CMP), material_names[(int)DOOR->material],
               CCNRM(CH, C_CMP), CCCYN(CH, C_CMP), DOOR->barrier, CCNRM(CH, C_CMP));


  send_to_char(CH,       "9) Hidden Rating: %s%d%s\r\n"
               "a) Purge exit.\r\n"
               "Enter choice, 0 to quit:",
               CCCYN(CH, C_CMP), DOOR->hidden, CCNRM(CH, C_CMP));
  d->edit_mode = REDIT_EXIT_MENU;
}

/* For exit flags */
void redit_disp_exit_flag_menu(struct descriptor_data * d)
{
  send_to_char( "0) No door\r\n"
                "1) Closeable door\r\n"
                "2) Pickproof\r\n"
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
    sprintf(buf, "%2d) %-10s     %2d) %-10s\r\n",
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
               CCCYN(CH, C_CMP),
               d->edit_number, CCNRM(CH, C_CMP),
               CCCYN(CH, C_CMP), d->edit_room->name, CCNRM(CH, C_CMP));
  send_to_char(CH, "2) Room Desc:\r\n%s\r\n", d->edit_room->description);
  send_to_char(CH, "3) Night Desc: \r\n%s\r\n", d->edit_room->night_desc);
  send_to_char(CH, "Room zone: %s%d%s\r\n",
               CCCYN(CH, C_CMP),
               zone_table[d->edit_room->zone].number,
               CCNRM(CH, C_CMP));
  ROOM->room_flags.PrintBits(buf2, MAX_STRING_LENGTH, room_bits, ROOM_MAX);
  send_to_char(CH, "4) Room flags: %s%s%s\r\n",
               CCCYN(CH, C_CMP),
               buf2, CCNRM(CH,
                           C_CMP));
  sprinttype(d->edit_room->sector_type, spirit_name, buf2);
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
  send_to_char(CH, "k) Background Count: ^c%d (%s)^n\r\n", d->edit_room->background[0], background_types[d->edit_room->background[1]]);
  send_to_char(CH, "l) Extra descriptions\r\n", d->character);
  if (d->edit_room->sector_type == SPIRIT_LAKE || d->edit_room->sector_type == SPIRIT_SEA ||
      d->edit_room->sector_type == SPIRIT_RIVER)
    send_to_char(CH, "m) Current rating: %s%d%s\r\n", CCCYN(CH, C_CMP),
                 ROOM->rating, CCNRM(CH, C_CMP));
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
      GET_WAS_IN(CH) = NOWHERE;
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
        if (!from_ip_zone(d->edit_number)) {
          sprintf(buf,"%s wrote new room #%ld",
                  GET_CHAR_NAME(d->character), d->edit_number);
          mudlog(buf, d->character, LOG_WIZLOG, TRUE);
        }
        room_num = real_room(d->edit_number);
        if (room_num > 0) {
          /* copy people/object pointers over to the temp room
             as a temporary measure */
          d->edit_room->contents = world[room_num].contents;
          d->edit_room->people = world[room_num].people;
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
              GET_WAS_IN(CH) = NOWHERE;
              return;
            }


          /* count thru world tables */
          for (counter = 0; counter < top_of_world + 1; counter++) {
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
                if (temp_ch->in_room != NOWHERE)
                  temp_ch->in_room++;
              }

              for (temp_veh = world[counter].vehicles; temp_veh;
                   temp_veh = temp_veh->next_veh)  {
                if (temp_veh->in_room != NOWHERE)
                  temp_veh->in_room++;
              }

              /* move objects */
              for (temp_obj = world[counter].contents; temp_obj;
                   temp_obj = temp_obj->next_content)
                if (temp_obj->in_room != -1)
                  temp_obj->in_room++;
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
            if (GET_WAS_IN(temp_ch) >= room_num)
              GET_WAS_IN(temp_ch)++;
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
          for (counter = 0; counter < top_of_world + 1; counter++) {
            for (counter2 = 0; counter2 < NUM_OF_DIRS; counter2++) {
              /* if exit exists */
              if (world[counter].dir_option[counter2]) {
                /* increment r_nums for rooms bigger than or equal to new one
                 * because we inserted room */
                if (world[counter].dir_option[counter2]->to_room >= room_num)
                  world[counter].dir_option[counter2]->to_room += 1;
                /* if an exit to the new room is indicated, change to_room */
                if (world[counter].dir_option[counter2]->to_room_vnum == d->edit_number)
                  world[counter].dir_option[counter2]->to_room = room_num;
              }
            }
          }
        } // end 'insert' else
        /* resolve all vnum doors to rnum doors in the newly edited room */
        int opposite;
        for (counter2 = 0; counter2 < NUM_OF_DIRS; counter2++) {
          if (world[room_num].dir_option[counter2]) {
            world[room_num].dir_option[counter2]->to_room =
              real_room(world[room_num].dir_option[counter2]->to_room_vnum);
            if (counter2 < NUM_OF_DIRS) {
              opposite = world[room_num].dir_option[counter2]->to_room;
              if (opposite != NOWHERE && world[opposite].dir_option[rev_dir[counter2]] &&
                  world[opposite].dir_option[rev_dir[counter2]]->to_room == room_num) {
                world[opposite].dir_option[rev_dir[counter2]]->material =
                  world[room_num].dir_option[counter2]->material;
                world[opposite].dir_option[rev_dir[counter2]]->barrier =
                  world[room_num].dir_option[counter2]->barrier;
                world[opposite].dir_option[rev_dir[counter2]]->condition =
                  world[room_num].dir_option[counter2]->condition;
              }
            }
          }
        }
        send_to_char("Writing room to disk.\r\n", d->character);
        write_world_to_disk(d->character->player_specials->saved.zonenum);
        send_to_char("Saved.\r\n", CH);
        /* do NOT free strings! just the room structure */
        Mem->ClearRoom(d->edit_room);
        d->edit_room = NULL;
        PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
        STATE(d) = CON_PLAYING;
        char_to_room(CH, GET_WAS_IN(CH));
        GET_WAS_IN(CH) = NOWHERE;
        send_to_char("Done.\r\n", d->character);
        break;
      }
    case 'n':
    case 'N':
      send_to_char("Room not saved, aborting.\r\n", d->character);
      STATE(d) = CON_PLAYING;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      /* free everything up, including strings etc */
      if (d->edit_room)
        Mem->DeleteRoom(d->edit_room);
      d->edit_room = NULL;
      d->edit_number = 0;
      char_to_room(CH, GET_WAS_IN(CH));
      GET_WAS_IN(CH) = NOWHERE;
      break;
    default:
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char("Do you wish to save this room internally?", d->character);
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
    case '1':
      send_to_char("Enter room name:", d->character);
      d->edit_mode = REDIT_NAME;
      break;
    case '2':
      send_to_char("Enter room description:\r\n", d->character);
      d->edit_mode = REDIT_DESC;
      DELETE_ARRAY_IF_EXTANT(d->str);
      d->str = new (char *);
      if (!d->str) {
        mudlog("Malloc failed!", NULL, LOG_SYSLOG, TRUE);
        shutdown();
      }

      *(d->str) = NULL;
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      break;
    case '3':
      send_to_char("Enter room nighttime desc description:\r\n", d->character);
      d->edit_mode = REDIT_NDESC;
      DELETE_ARRAY_IF_EXTANT(d->str);
      d->str = new (char *);
      if (!d->str) {
        mudlog("Malloc failed!", NULL, LOG_SYSLOG, TRUE);
        shutdown();
      }

      *(d->str) = NULL;
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
      if (ROOM->sector_type == SPIRIT_LAKE || ROOM->sector_type == SPIRIT_SEA || ROOM->sector_type == SPIRIT_RIVER) {
        send_to_char("Enter current rating (1 to 20): ", CH);
        d->edit_mode = REDIT_LIBRARY_RATING;
      } else {
        redit_disp_menu(d);
        return;
      }
      break;
    default:
      send_to_char("Invalid choice!", d->character);
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
      send_to_char(CH, "Enter Access Modifier: ");
      d->edit_mode = REDIT_ACCESS;
      break;
    case '2':
      send_to_char(CH, "Enter Trace Modifier: ");
      d->edit_mode = REDIT_TRACE;
      break;
    case '3':
      send_to_char(CH, "Enter I/O Speed (-1 For Tap, 0 For Unlimited): ");
      d->edit_mode = REDIT_IO;
      break;
    case '4':
      send_to_char(CH, "Enter Base Bandwidth (-1 For Tap, 0 For Unlimited): ");
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
  case REDIT_BACKGROUND2:
    number = atoi(arg);
    if (number < 0 || number > AURA_PLAYERCOMBAT - 1) {
      send_to_char(CH, "Number must be between 0 and %d. Enter Type: ", AURA_PLAYERCOMBAT - 1);
      return;
    }
    d->edit_room->background[1] = number;
    redit_disp_menu(d);
    break;
  case REDIT_BACKGROUND:
    d->edit_room->background[0] = atoi(arg);
    for (number = 0; number < AURA_PLAYERCOMBAT; number++)
      send_to_char(CH, "%d) %s\r\n", number, background_types[number]);
    send_to_char("Enter background count type: ", CH);
    d->edit_mode = REDIT_BACKGROUND2;
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
    } else {
      if (number == 0)
        /* back out */
        redit_disp_menu(d);
      else {
        /* toggle bits */

        if (ROOM->room_flags.IsSet(number-1)) {
          ROOM->room_flags.RemoveBit(number-1);
        } else
          ROOM->room_flags.SetBit(number-1);
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
      send_to_char("Invalid choice!", d->character);
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
      DELETE_ARRAY_IF_EXTANT(d->str);
      d->str = new (char *);
      if (!d->str) {
        mudlog("Malloc failed!", NULL, LOG_SYSLOG, TRUE);
        shutdown();
      }

      *(d->str) = NULL;
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
      /* delete exit */
      if (d->edit_room->dir_option[d->edit_number2]->keyword)
        delete [] d->edit_room->dir_option[d->edit_number2]->keyword;
      if (d->edit_room->dir_option[d->edit_number2]->general_description)
        delete [] d->edit_room->dir_option[d->edit_number2]->general_description;
      delete d->edit_room->dir_option[d->edit_number2];
      d->edit_room->dir_option[d->edit_number2] = NULL;
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
      d->edit_room->dir_option[d->edit_number2]->to_room_vnum = number;
      redit_disp_exit_menu(d);
    }
    break;
  case REDIT_EXIT_DESCRIPTION:
    /* we should NEVER get here */
    break;
  case REDIT_EXIT_KEYWORD:
    if (d->edit_room->dir_option[d->edit_number2]->keyword)
      delete [] d->edit_room->dir_option[d->edit_number2]->keyword;
    d->edit_room->dir_option[d->edit_number2]->keyword = str_dup(arg);
    redit_disp_exit_menu(d);
    break;
  case REDIT_EXIT_KEY:
    number = atoi(arg);
    d->edit_room->dir_option[d->edit_number2]->key = number;
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

  case REDIT_LIBRARY_RATING:
    number = atoi(arg);
    if ((number < 0) || (number > 20)) {
      send_to_char("Value must be between 1 and 20.\r\n", CH);
      send_to_char("Enter current rating: ", CH);
    } else {
      ROOM->rating = number;
      redit_disp_menu(d);
    }
    break;
  case REDIT_EXIT_DOORFLAGS:
    number = atoi(arg);
    if ((number < 0) || (number > 2)) {
      send_to_char("That's not a valid choice!\r\n", d->character);
      redit_disp_exit_flag_menu(d);
    } else {
      /* doors are a bit idiotic, don't you think? :) */
      if (number == 0)
        d->edit_room->dir_option[d->edit_number2]->exit_info = 0;
      else if (number == 1)
        d->edit_room->dir_option[d->edit_number2]->exit_info = EX_ISDOOR;
      else if (number == 2)
        d->edit_room->dir_option[d->edit_number2]->exit_info =
          EX_ISDOOR | EX_PICKPROOF;
      /* jump out to menu */
      redit_disp_exit_menu(d);
    }
    break;
  case REDIT_EXTRADESC_KEY:
    if (((struct extra_descr_data *) *d->misc_data)->keyword)
      delete [] (((struct extra_descr_data *) *d->misc_data)->keyword);
    ((struct extra_descr_data *) * d->misc_data)->keyword = str_dup(arg);
    redit_disp_extradesc_menu(d);
    break;
  case REDIT_EXTRADESC_MENU:
    number = atoi(arg);
    switch (number) {
    case 0: {
        /* if something got left out, delete the extra desc
         when backing out to menu */
        if (!((struct extra_descr_data *) * d->misc_data)->keyword ||
            !((struct extra_descr_data *) * d->misc_data)->description) {
          if (((struct extra_descr_data *) * d->misc_data)->keyword)
            delete [] ((struct extra_descr_data *) * d->misc_data)->keyword;
          if (((struct extra_descr_data *) * d->misc_data)->description)
            delete [] ((struct extra_descr_data *) * d->misc_data)->description;

          delete (struct extra_descr_data *) *d->misc_data;
          *d->misc_data = NULL;
        }
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
      DELETE_ARRAY_IF_EXTANT(d->str);
      d->str = new (char *);
      if (!d->str) {
        mudlog("Malloc failed!", NULL, LOG_SYSLOG, TRUE);
        shutdown();
      }

      *(d->str) = NULL;
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
          d->misc_data =
            (void **) &((struct extra_descr_data *) * d->misc_data)->next;
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
#define RM world[realcounter]
void write_world_to_disk(int vnum)
{
  long             counter, counter2, realcounter;
  int             znum = real_zone(vnum);
  FILE           *fp;
  struct extra_descr_data *ex_desc;

  // ideally, this would just fill a VTable with vals...maybe one day

  sprintf(buf, "%s/%d.wld", WLD_PREFIX,
          zone_table[znum].number);
  fp = fopen(buf, "w+");
  for (counter = zone_table[znum].number * 100;
       counter <= zone_table[znum].top; counter++) {
    realcounter = real_room(counter);

    if (realcounter >= 0) {
      if (!strcmp("An unfinished room", RM.name))
        continue;
      fprintf(fp, "#%ld\n", counter);

      fprintf(fp, "Name:\t%s\n",
              RM.name ? RM.name : "An unnamed room");
      fprintf(fp, "Desc:$\n%s~\n",
              cleanup(buf2, RM.description ? RM.description :
                      "You see an empty room"));
      if (RM.night_desc)
        fprintf(fp, "NightDesc:$\n%s~\n", cleanup(buf2, RM.night_desc));

      fprintf(fp,
              "Flags:\t%s\n"
              "SecType:\t%s\n"
              "MatrixExit:\t%ld\n",
              RM.room_flags.ToString(),
              spirit_name[RM.sector_type],
              RM.matrix);

      if (real_host(RM.matrix) > 0)
        fprintf(fp, "IO:\t%d\n"
                "Bandwidth:\t%d\n"
                "Access:\t%d\n"
                "Trace:\t%d\n"
                "RTG:\t%ld\n"
                "JackID:\t%d\n"
                "Address:\t%s\n",
                RM.io, RM.bandwidth, RM.access, RM.trace, RM.rtg, RM.jacknumber, RM.address);

      fprintf(fp, "Crowd:\t%d\n"
                  "Cover:\t%d\n"
                  "X:\t%d\n"
                  "Y:\t%d\n"
                  "Z:\t%.2f\n"
                  "RoomType:\t%d\n", RM.crowd, RM.cover, RM.x, RM.y, RM.z, RM.type);

      fprintf(fp  ,
              "[POINTS]\n"
              "\tSpecIdx:\t%d\n"
              "\tRating:\t%d\n"
              "\tLight:\t%d\n"
              "\tSmoke:\t%d\n"
              "\tBackground:\t%d\n"
              "\tBackgroundType:\t%d\n",
              RM.spec, RM.rating, RM.vision[0], RM.vision[1], RM.background[0], RM.background[1]);

      for (counter2 = 0; counter2 < NUM_OF_DIRS; counter2++) {
        room_direction_data *ptr = RM.dir_option[counter2];

        if (ptr) {
          int             temp_door_flag;

          fprintf(fp, "[EXIT %s]\n", fulldirs[counter2]);

          if (ptr->keyword)
            fprintf(fp, "\tKeywords:\t%s\n", ptr->keyword);
          if (ptr->general_description)
            fprintf(fp, "\tDesc:$\n%s~\n",
                    cleanup(buf2, ptr->general_description));

          /* door flags need special handling, unfortunately. argh! */
          if (IS_SET(ptr->exit_info, EX_ISDOOR)) {
            if (IS_SET(ptr->exit_info, EX_PICKPROOF))
              temp_door_flag = 2;
            else
              temp_door_flag = 1;
          } else
            temp_door_flag = 0;

          fprintf(fp,
                  "\tToVnum:\t%ld\n"
                  "\tFlags:\t%d\n"
                  "\tMaterial:\t%s\n"
                  "\tBarrier:\t%d\n",
                  ptr->to_room_vnum,
                  temp_door_flag,
                  material_names[(int)ptr->material],
                  ptr->barrier);

          if (DBIndex::IsValidV(ptr->key))
            fprintf(fp, "\tKeyVnum:\t%ld\n", ptr->key);

          if (ptr->key_level > 0)
            fprintf(fp, "\tLockRating:\t%d\n", ptr->key_level);

          if (ptr->hidden > 0)
            fprintf(fp, "\tHiddenRating:\t%d\n", ptr->hidden);
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

  write_index_file("wld");
  /* do NOT free strings! just the room structure */
}
#undef RM

