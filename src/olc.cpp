/* *************************************************************
*    file: olc.cc                                              *
*    function: Main routines for AwakeOLC, a component of      *
*    AwakeMUD                                                  *
*    (c)1996-2000 Christopher J. Dickey, Andrew Hynek, and     *
*    Nick Robertson, (c)2001 The AwakeMUD Consortium           *
************************************************************* */


#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "structs.h"
#include "awake.h"
#include "interpreter.h"
#include "comm.h"
#include "utils.h"
#include "db.h"
#include "dblist.h"
#include "olc.h"
#include "memory.h"
#include "newshop.h"
#include "quest.h"
#include "handler.h"
#include "constants.h"
#include "newmatrix.h"
#include "config.h"

extern class objList ObjList;
extern sh_int mortal_start_room;
extern sh_int immort_start_room;
extern sh_int frozen_start_room;
extern vnum_t newbie_start_room;
extern void char_from_room(struct char_data * ch);
extern void write_mobs_to_disk(int zone);
extern void write_objs_to_disk(int zone);
extern void write_shops_to_disk(int zone);
extern void write_world_to_disk(int);
extern void write_zone_to_disk(int vnum);
extern void iedit_disp_menu(struct descriptor_data *d);
extern void redit_disp_menu(struct descriptor_data *d);
extern void medit_disp_menu(struct descriptor_data *d);
extern void shedit_disp_menu(struct descriptor_data *d);
extern void qedit_disp_menu(struct descriptor_data *d);
extern void zedit_disp_command_menu(struct descriptor_data *d);
extern void zedit_disp_data_menu(struct descriptor_data *d);
extern void vedit_disp_menu(struct descriptor_data * d);
extern void hedit_disp_data_menu(struct descriptor_data *d);
extern void icedit_disp_menu(struct descriptor_data *d);

// mem class
extern class memoryClass *Mem;

void write_index_file(const char *suffix)
{
  FILE *fp;
  int i, found, j;

  if (*suffix == 'h')
    sprintf(buf, "world/mtx/index");
  else if ( *suffix == 'i')
    sprintf(buf, "world/mtx/index.ic");
  else
    sprintf(buf, "world/%s/index", suffix);
  fp = fopen(buf, "w+");

  for (i = 0; i <= top_of_zone_table; i++) {
    found = 0;
    switch (*suffix) {
    case 'm':
      for (j = 0; !found && j <= top_of_mobt; j++)
        if (MOB_VNUM_RNUM(j) >= (zone_table[i].number * 100) && MOB_VNUM_RNUM(j) <= zone_table[i].top) {
          found = 1;
          fprintf(fp, "%d.%s\n", zone_table[i].number, suffix);
        }
      break;
    case 'v':
      for (j = 0; !found && j <= top_of_veht; j++)
        if (veh_index[j].vnum >= (zone_table[i].number * 100) && veh_index[j].vnum <= zone_table[i].top) {
          found = 1;
          fprintf(fp, "%d.%s\n", zone_table[i].number, suffix);
        }
      break;
    case 'o':
      for (j = 0; !found && j <= top_of_objt; j++)
        if (OBJ_VNUM_RNUM(j) >= (zone_table[i].number * 100) && OBJ_VNUM_RNUM(j) <= zone_table[i].top) {
          found = 1;
          fprintf(fp, "%d.%s\n", zone_table[i].number, suffix);
        }
      break;
    case 'w':
      for (j = 0; !found && j <= top_of_world; j++)
        if (world[j].number >= (zone_table[i].number * 100) && world[j].number <= zone_table[i].top) {
          found = 1;
          fprintf(fp, "%d.%s\n", zone_table[i].number, suffix);
        }
      break;
    case 'h':
      for (j = 0; !found && j <= top_of_matrix; j++)
        if (matrix[j].vnum >= (zone_table[i].number * 100) && matrix[j].vnum <= zone_table[i].top) {
          found = 1;
          fprintf(fp, "%d.%s\n", zone_table[i].number, suffix);
        }
      break;
    case 'i':
      for (j = 0; !found && j <= top_of_ic; j++)
        if (ic_index[j].vnum >= (zone_table[i].number * 100) && ic_index[j].vnum <= zone_table[i].top) {
          found = 1;
          fprintf(fp, "%d.%s\n", zone_table[i].number, suffix);
        }
      break;
    case 's':
      for (j = 0; !found && j <= top_of_shopt; j++)
        if (shop_table[j].vnum >= (zone_table[i].number * 100) && shop_table[j].vnum <= zone_table[i].top) {
          found = 1;
          fprintf(fp, "%d.%s\n", zone_table[i].number, suffix);
        }
      break;
    case 'z':
      fprintf(fp, "%d.%s\n", zone_table[i].number, suffix);
      break;
    default:
      mudlog("Incorrect suffix sent to write_index_file.", NULL, LOG_SYSLOG, TRUE);
      fprintf(fp, "$\n");
      fclose(fp);
      return;
    }
  }

  fprintf(fp, "$\n");

  fclose(fp);
}

/*
 * the redit ACMD function
 */
ACMD (do_redit)
{
  int number;
  int room_num;
  struct descriptor_data *d;
  char arg1[MAX_INPUT_LENGTH];
  struct room_data *room;
  int counter, found = 0;

  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  if (!olc_state) {
    send_to_char("OLC temporarily unavailable.\r\n", ch);
    return;
  }

  one_argument (argument, arg1);
  if (!*argument) {
    number = world[ch->in_room].number;
    room_num = ch->in_room;
  } else {
    if (!isdigit (*arg1)) {
      send_to_char ("Please supply a valid number.\r\n", ch);
      return;
    }
    number = atoi(arg1);
    room_num = real_room (number);
  }
  for (counter = 0; counter <= top_of_zone_table; counter++) {
    if ((number >= (zone_table[counter].number * 100)) && (number <= (zone_table[counter].top))) {
      ch->desc->edit_zone = counter;
      found = 1;
      break;
    }
  }
  if (!found) {
    send_to_char ("Sorry, that number is not part of any zone!\r\n", ch);
    return;
  }

  // this is part of the new routine that checks for the id list from the
  // actual zone itself to determine if they can edit it or not
  {
    int i = 0;
    for (; i < 5; i++)
      if (zone_table[counter].editor_ids[i] == GET_IDNUM(ch))
        break;
    if ((i >= 5) && (!access_level(ch, LVL_ADMIN)))// it was not found
    {
      send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
      return;
    }
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't edit rooms from a connected zone.\r\n", ch);
    return;
  }

  ch->player_specials->saved.zonenum = zone_table[counter].number;

  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  d = ch->desc;
  STATE(d) = CON_REDIT;
  GET_WAS_IN(ch) = ch->in_room;
  char_from_room(ch);
  d->edit_number = number;
  if (room_num >= 0) {
    room = Mem->GetRoom();
    *room = world[room_num];
    /* allocate space for all strings  */
    if (world[room_num].name)
      room->name = str_dup (world[room_num].name);
    if (world[room_num].description)
      room->description = str_dup (world[room_num].description);
    if (world[room_num].night_desc)
      room->night_desc = str_dup (world[room_num].night_desc);
    if (world[room_num].address)
      room->address = str_dup (world[room_num].address);
    room->zone = world[room_num].zone;
    /* exits - alloc only if necessary */
    for (counter = 0; counter < NUM_OF_DIRS; counter++) {
      if (world[room_num].dir_option[counter]) {
        room->dir_option[counter] = new room_direction_data;
        /* copy numbers over */
        *room->dir_option[counter] = *world[room_num].dir_option[counter];
        /* malloc strings */
        if (world[room_num].dir_option[counter]->general_description)
          room->dir_option[counter]->general_description =
            str_dup(world[room_num].dir_option[counter]->general_description);
        if (world[room_num].dir_option[counter]->keyword)
          room->dir_option[counter]->keyword =
            str_dup(world[room_num].dir_option[counter]->keyword);
      }
    }
    if (world[room_num].ex_description) {
      struct extra_descr_data *This, *temp, *temp2;

      temp = new extra_descr_data;
      room->ex_description = temp;
      for (This = world[room_num].ex_description;
           This; This = This->next) {
        if (This->keyword)
          temp->keyword = str_dup (This->keyword);
        if (This->description)
          temp->description = str_dup (This->description);
        if (This->next) {
          temp2 = new extra_descr_data;

          temp->next = temp2;
          temp = temp2;
        } else
          temp->next = NULL;
      }
    }
    d->edit_room = room;
#ifdef CONFIRM_EXISTING

    send_to_char ("A room already exists at that number. Do you wish to edit it?\r\n", ch);
    d->edit_mode = REDIT_CONFIRM_EDIT;
#else

    redit_disp_menu(d);
#endif

    return;
  } else {
    send_to_char ("That room does not exist, create it?\r\n", ch);
    /*
     * create dummy room
     */
    d->edit_room = Mem->GetRoom();
    //memset((char *) d->edit_room, 0, sizeof(struct room_data));
    d->edit_room->name = str_dup("An unfinished room");
    d->edit_room->description = str_dup("You are in an unfinished room.\r\n");
    d->edit_room->address = str_dup("An undisclosed location");
    int i = 0;
    while ((d->edit_number > zone_table[i].top) && (i < top_of_zone_table))
      ++i;
    if (i <= top_of_zone_table)
      d->edit_room->zone = i;
    d->edit_mode = REDIT_CONFIRM_EDIT;
    return;
  }
}

ACMD(do_rclone)
{
  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  int arg1, arg2, num1, num2, counter, i, zone1 = -1, zone2 = -1;

  two_arguments(argument, buf, buf1);

  if (!*buf || !*buf1) {
    send_to_char("Usage: rclone <old room vnum> <new room vnum>\r\n", ch);
    return;
  }

  arg1 = atoi(buf);
  arg2 = atoi(buf1);

  for (counter = 0; counter <= top_of_zone_table; counter++) {
    if (arg1 >= (zone_table[counter].number * 100) && arg1 <= zone_table[counter].top)
      zone1 = counter;
    if (arg2 >= (zone_table[counter].number * 100) && arg2 <= zone_table[counter].top)
      zone2 = counter;
  }

  if (zone1 < 0 || zone2 < 0) {
    send_to_char("That number is not part of any zone.\r\n", ch);
    return;
  }

  for (i = 0; i < 5; i++)
    if (zone_table[zone1].editor_ids[i] == GET_IDNUM(ch))
      break;
  if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) {
    send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
    return;
  }

  for (i = 0; i < 5; i++)
    if (zone_table[zone2].editor_ids[i] == GET_IDNUM(ch))
      break;
  if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) {
    send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
    return;
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't clone rooms to a connected zone.\r\n", ch);
    return;
  }


  num1 = real_room(arg1);
  num2 = real_room(arg2);

  if (num1 < 0) {
    send_to_char("Invalid room number.\r\n", ch);
    return;
  }

  if (num2 > -1) {
    send_to_char("You cannot clone over an existing room.\r\n", ch);
    return;
  }

  // now for the fun part
  // first duplicate the room
  struct room_data *room;
  room = Mem->GetRoom();

  *room = world[num1];
  /* allocate space for all strings  */
  if (world[num1].name)
    room->name = str_dup (world[num1].name);
  if (world[num1].description)
    room->description = str_dup (world[num1].description);
  if (world[num1].address)
    room->address = str_dup (world[num1].address);
  room->zone = world[num1].zone;
  /* exits - alloc only if necessary */
  for (counter = 0; counter < NUM_OF_DIRS; counter++) {
    if (world[num1].dir_option[counter]) {
      room->dir_option[counter] = new room_direction_data;
      /* copy numbers over */
      *room->dir_option[counter] = *world[num1].dir_option[counter];
      /* malloc strings */
      if (world[num1].dir_option[counter]->general_description)
        room->dir_option[counter]->general_description =
          str_dup(world[num1].dir_option[counter]->general_description);
      if (world[num1].dir_option[counter]->keyword)
        room->dir_option[counter]->keyword =
          str_dup(world[num1].dir_option[counter]->keyword);
    }
  }
  // now do any extra descriptions
  if (world[num1].ex_description) {
    struct extra_descr_data *This, *temp, *temp2;

    temp = new extra_descr_data;
    room->ex_description = temp;
    for (This = world[num1].ex_description; This; This = This->next) {
      if (This->keyword)
        temp->keyword = str_dup (This->keyword);
      if (This->description)
        temp->description = str_dup (This->description);
      if (This->next) {
        temp2 = new extra_descr_data;

        temp->next = temp2;
        temp = temp2;
      } else
        temp->next = NULL;
    }
  }

  // null out some vars to keep probs away
  room->contents = NULL;
  room->people = NULL;
  // put them in the editing state
  PLR_FLAGS (ch).SetBit(PLR_EDITING);
  GET_WAS_IN(ch) = ch->in_room;
  char_from_room(ch);
  STATE (ch->desc) = CON_REDIT;
  ch->desc->edit_room = room;
  send_to_char("Are you sure you want to clone this room?\r\n", ch);
  ch->desc->edit_mode = REDIT_CONFIRM_SAVESTRING;
  ch->desc->edit_number = atoi(buf1);
}

ACMD(do_dig)
{
  int counter, i = 0, dir, room, zone1 = 0, zone2 = 0;

  any_one_arg(any_one_arg(argument, arg), buf);

  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  if (!*arg || !*buf) {
    send_to_char("Usage: dig <direction> <vnum>\r\n", ch);
    return;
  }

  if (is_abbrev(arg, "south"))
    dir = SOUTH;
  else if ((dir = search_block(arg, dirs, FALSE)) < 0) {
    send_to_char("What direction?\r\n", ch);
    return;
  }

  if (dir == NUM_OF_DIRS) {
    send_to_char("What direction?\r\n", ch);
    return;
  }

  if (!atoi(buf)) {
    send_to_char("Please enter a valid room.\r\n", ch);
    return;
  }

  room = real_room(atoi(buf));

  if (room == -1) {
    send_to_char("No such room.\r\n", ch);
    return;
  }

  for (counter = 0; counter <= top_of_zone_table; counter++) {
    if (world[ch->in_room].number >= (zone_table[counter].number * 100) &&
        world[ch->in_room].number <= zone_table[counter].top)
      zone1 = counter;
    if (world[room].number >= (zone_table[counter].number * 100) &&
        world[room].number <= zone_table[counter].top)
      zone2 = counter;
  }

  for (i = 0; i < 5; i++)
    if (zone_table[zone1].editor_ids[i] == GET_IDNUM(ch))
      break;
  if (i >= 5 && !access_level(ch, LVL_ADMIN)) {
    send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
    return;
  }

  for (i = 0; i < 5; i++)
    if (zone_table[zone2].editor_ids[i] == GET_IDNUM(ch))
      break;
  if (i >= 5 && !access_level(ch, LVL_ADMIN)) {
    send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
    return;
  }

  if (world[ch->in_room].dir_option[dir] || world[room].dir_option[rev_dir[dir]]) {
    send_to_char("You can't dig over an existing exit.\r\n", ch);
    return;
  }

  world[ch->in_room].dir_option[dir] = new room_direction_data;
  memset((char *) world[ch->in_room].dir_option[dir], 0, sizeof (struct room_direction_data));
  world[ch->in_room].dir_option[dir]->to_room = room;
  world[ch->in_room].dir_option[dir]->barrier = 4;
  world[ch->in_room].dir_option[dir]->material = 5;
  world[ch->in_room].dir_option[dir]->exit_info = 0;
  world[ch->in_room].dir_option[dir]->to_room_vnum = world[room].number;
  dir = rev_dir[dir];
  world[room].dir_option[dir] = new room_direction_data;
  memset((char *) world[room].dir_option[dir], 0, sizeof (struct room_direction_data));
  world[room].dir_option[dir]->to_room = ch->in_room;
  world[room].dir_option[dir]->barrier = 4;
  world[room].dir_option[dir]->material = 5;
  world[room].dir_option[dir]->exit_info = 0;
  world[room].dir_option[dir]->to_room_vnum = world[ch->in_room].number;
  if (zone1 == zone2)
    write_world_to_disk(zone_table[zone1].number);
  else {
    write_world_to_disk(zone_table[zone1].number);
    write_world_to_disk(zone_table[zone2].number);
  }
  send_to_char("Done.\r\n", ch);
}

ACMD(do_rdelete)
{
  int num, counter, i, found = 0;

  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  one_argument(argument, buf);

  if (!*buf) {
    send_to_char("Usage: rdelete <vnum>\r\n", ch);
    return;
  }

  num = atoi(buf);

  if ((num == mortal_start_room) || (num == immort_start_room) || (num == newbie_start_room) ||
      (num == frozen_start_room)) {
    send_to_char("Deleting the start rooms would not be a good idea.\r\n", ch);
    return;
  }

  if (real_room(num) == -1) {
    send_to_char("No such room.\r\n", ch);
    return;
  }

  for (counter = 0; counter <= top_of_zone_table; counter++) {
    if ((num >= (zone_table[counter].number * 100)) && (num <= (zone_table[counter].top))) {
      found = 1;
      break;
    }
  }
  if (!found) {
    send_to_char ("Sorry, that number is not part of any zone.\r\n", ch);
    return;
  }

  for (i = 0; i < 5; i++)
    if (zone_table[counter].editor_ids[i] == GET_IDNUM(ch))
      break;
  if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) {
    send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
    return;
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't delete rooms from a connected zone.\r\n", ch);
    return;
  }

  ch->player_specials->saved.zonenum = zone_table[counter].number;

  num = real_room(num);

  // make sure it is empty first
  if (world[num].people || world[num].contents) {
    send_to_char("The room must be empty before you delete it.\r\n", ch);
    return;
  }

  // now Free the room
  if (world[num].name)
    delete [] world[num].name;
  if (world[num].description)
    delete [] world[num].description;
  for (counter = 0; counter < NUM_OF_DIRS; counter++) {
    if (world[num].dir_option[counter]) {
      if (world[num].dir_option[counter]->general_description)
        delete [] world[num].dir_option[counter]->general_description;
      if (world[num].dir_option[counter]->keyword)
        delete [] world[num].dir_option[counter]->keyword;
    }
  }

  struct extra_descr_data *This, *next_one;
  if (world[num].ex_description)
    for (This = world[num].ex_description; This; This = next_one) {
      next_one = This->next;
      if (This->keyword)
        delete [] This->keyword;
      if (This->description)
        delete [] This->description;
      delete This;
    }

  // now copy everything down one and decrement the rnum of any
  // objects or people in it
  struct char_data *temp_ch;
  struct obj_data *temp_obj;
  for (counter = num; counter <= top_of_world; counter++) {
    world[counter] = world[counter + 1];
    for (temp_ch = world[counter].people; temp_ch; temp_ch = temp_ch->next_in_room)
      if (temp_ch->in_room != -1)
        temp_ch->in_room = counter;
    /* move objects */
    for (temp_obj = world[counter].contents; temp_obj; temp_obj = temp_obj->next_content)
      if (temp_obj->in_room != -1)
        temp_obj->in_room = counter;
  }

  int counter2;
  // go through the world and fix the exits
  for (counter = 0; counter < top_of_world + 1; counter++)
    for (counter2 = 0; counter2 < NUM_OF_DIRS; counter2++) {
      /* if exit exists */
      if (world[counter].dir_option[counter2]) {
        /* increment r_nums for rooms bigger than or equal to new one
         * because we inserted room */
        if (world[counter].dir_option[counter2]->to_room >= num)
          world[counter].dir_option[counter2]->to_room -= 1;
      }
    }

  // update the zones by decrementing numbers if >= number deleted
  int zone, cmd_no;
  for (zone = 0; zone <= top_of_zone_table; zone++) {
    for (cmd_no = 0; cmd_no < zone_table[zone].num_cmds; cmd_no++)
      switch (ZCMD.command) {
      case 'M':
        if (ZCMD.arg3 == num) {
          ZCMD.command = '*';
          ZCMD.if_flag = 0;
          ZCMD.arg1 = 0;
          ZCMD.arg2 = 0;
          ZCMD.arg3 = 0;
        } else
          ZCMD.arg3 = (ZCMD.arg3 > num ? ZCMD.arg3 - 1 : ZCMD.arg3);
        break;
      case 'O':
        if (ZCMD.arg3 == num) {
          ZCMD.command = '*';
          ZCMD.if_flag = 0;
          ZCMD.arg1 = 0;
          ZCMD.arg2 = 0;
          ZCMD.arg3 = 0;
        } else if (ZCMD.arg3 != NOWHERE)
          ZCMD.arg3 = (ZCMD.arg3 > num ? ZCMD.arg3 - 1 : ZCMD.arg3);
        break;
      case 'D':
      case 'R':
        if (ZCMD.arg1 == num) {
          ZCMD.command = '*';
          ZCMD.if_flag = 0;
          ZCMD.arg1 = 0;
          ZCMD.arg2 = 0;
          ZCMD.arg3 = 0;
        } else
          ZCMD.arg1 = (ZCMD.arg1 > num ? ZCMD.arg1 - 1 : ZCMD.arg1);
        break;
      }
  }

  top_of_world--;
  write_zone_to_disk(ch->player_specials->saved.zonenum);
  write_world_to_disk(ch->player_specials->saved.zonenum);
  send_to_char("Done.\r\n", ch);
}

/*
 * the vedit ACMD function
 */
ACMD (do_vedit)
{
  int number;
  int veh_num;
  struct descriptor_data *d;
  char arg1[MAX_INPUT_LENGTH];
  struct veh_data *veh;
  int counter, found = 0;
  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  if (!olc_state) {
    send_to_char("OLC temporarily unavailable.\r\n", ch);
    return;
  }

  one_argument (argument, arg1);

  if (!*argument) {
    send_to_char ("Specify an vehicle number to edit.\r\n", ch);
    return;
  }
  if (!isdigit (*arg1)) {
    send_to_char ("Please supply a valid number.\r\n", ch);
    return;
  }

  number = atoi (arg1);
  veh_num = real_vehicle (number);

  for (counter = 0; counter <= top_of_zone_table; counter++) {
    if ((number >= (zone_table[counter].number * 100)) &&
        (number <= (zone_table[counter].top))) {
      ch->desc->edit_zone = counter;
      found = 1;
      break;
    }
  }
  if (!found) {
    send_to_char ("Sorry, that number is not part of any zone.\r\n", ch);
    return;
  }

  // this is part of the new routine that checks for the id list from the
  // actual zone itself to determine if they can edit it or not
  {
    int i = 0;
    for (; i < 5; i++)
      if (zone_table[counter].editor_ids[i] == GET_IDNUM(ch))
        break;
    if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) // it wasn't found
    {
      send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
      return;
    }
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't edit objects from a connected zone.\r\n", ch);
    return;
  }

  ch->player_specials->saved.zonenum = zone_table[counter].number;

  /*
   * put person into editing mode
   */
  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  /*
   * now start playing with the descriptor
   */
  d = ch->desc;
  STATE (d) = CON_VEDIT;
  d->edit_number = number;      /*
                                                           * the VNUM not the REAL NUMBER
                                                           */

  if (veh_num >= 0) {
    /*
     * allocate object
     */
    veh = Mem->GetVehicle();
    *veh = veh_proto[veh_num];        /*
                                                                                             * the RNUM
                                                                                             */
    /*
     * copy all strings over
     */
    if (veh_proto[veh_num].name)
      veh->name = str_dup(veh_proto[veh_num].name);
    if (veh_proto[veh_num].description)
      veh->description = str_dup(veh_proto[veh_num].description);
    if (veh_proto[veh_num].short_description)
      veh->short_description = str_dup(veh_proto[veh_num].short_description);
    if (veh_proto[veh_num].long_description)
      veh->long_description = str_dup(veh_proto[veh_num].long_description);
    if (veh_proto[veh_num].inside_description)
      veh->inside_description = str_dup(veh_proto[veh_num].inside_description);
    if (veh_proto[veh_num].rear_description)
      veh->rear_description = str_dup(veh_proto[veh_num].rear_description);
    if (veh_proto[veh_num].arrive)
      veh->arrive = str_dup(veh_proto[veh_num].arrive);
    if (veh_proto[veh_num].leave)
      veh->leave = str_dup(veh_proto[veh_num].leave);
    
    d->edit_veh = veh;
#ifdef CONFIRM_EXISTING

    send_to_char ("An vehicle already exists at that number. Do you wish to edit it?\r\n", ch);
    d->edit_mode = VEDIT_CONFIRM_EDIT;
#else

    vedit_disp_menu(d);
#endif

    return;
  } else {
    send_to_char ("That vehicle does not exist, create it?\r\n", ch);
    /*
     * create dummy object!
     */
    d->edit_veh = Mem->GetVehicle();
    d->edit_veh->name = str_dup ("unfinished object");
    d->edit_veh->description = str_dup ("An unfinished object is lying here.");
    d->edit_veh->short_description = str_dup ("an unfinished object");
    d->edit_veh->long_description = str_dup ("It looks pretty much like an unfinished object");
    d->edit_mode = VEDIT_CONFIRM_EDIT;
    return;
  }
}

/*
 * the iedit ACMD function
 */
ACMD (do_iedit)
{
  vnum_t number;
  vnum_t obj_num;
  struct descriptor_data *d;
  char arg1[MAX_INPUT_LENGTH];
  struct obj_data *obj;
  int counter, found = 0;

  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  if (!olc_state) {
    send_to_char("OLC temporarily unavailable.\r\n", ch);
    return;
  }

  one_argument (argument, arg1);

  if (!*argument) {
    send_to_char ("Specify an object number to edit.\r\n", ch);
    return;
  }
  if (!isdigit (*arg1)) {
    send_to_char ("Please supply a valid number.\r\n", ch);
    return;
  }

  number = atoi (arg1);
  obj_num = real_object (number);

  for (counter = 0; counter <= top_of_zone_table; counter++) {
    if ((number >= (zone_table[counter].number * 100)) &&
        (number <= (zone_table[counter].top))) {
      ch->desc->edit_zone = counter;
      found = 1;
      break;
    }
  }
  if (!found) {
    send_to_char ("Sorry, that number is not part of any zone.\r\n", ch);
    return;
  }

  // this is part of the new routine that checks for the id list from the
  // actual zone itself to determine if they can edit it or not
  {
    int i = 0;
    for (; i < 5; i++)
      if (zone_table[counter].editor_ids[i] == GET_IDNUM(ch))
        break;
    if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) // it was not found
    {
      send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
      return;
    }
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't edit objects from a connected zone.\r\n", ch);
    return;
  }

  ch->player_specials->saved.zonenum = zone_table[counter].number;

  /*
   * put person into editing mode
   */
  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  /*
   * now start playing with the descriptor
   */
  d = ch->desc;
  STATE (d) = CON_IEDIT;

  d->edit_number = number;      /*
                                                           * the VNUM not the REAL NUMBER
                                                           */

  if (obj_num >= 0) {
    struct extra_descr_data *This, *temp, *temp2;
    /*
     * allocate object
     */
    obj = Mem->GetObject();
    //clear_object (obj);
    *obj = obj_proto[obj_num];        /*
                                                                                             * the RNUM
                                                                                             */
    /*
     * copy all strings over
     */
    if (obj_proto[obj_num].text.keywords)
      obj->text.keywords = str_dup(obj_proto[obj_num].text.keywords);
    if (obj_proto[obj_num].text.name)
      obj->text.name = str_dup(obj_proto[obj_num].text.name);
    if (obj_proto[obj_num].text.room_desc)
      obj->text.room_desc = str_dup(obj_proto[obj_num].text.room_desc);
    if (obj_proto[obj_num].text.look_desc)
      obj->text.look_desc = str_dup(obj_proto[obj_num].text.look_desc);

    if (obj_proto[obj_num].ex_description) {
      /*
       * temp is for obj being edited
       */
      temp = new extra_descr_data;
      obj->ex_description = temp;
      for (This = obj_proto[obj_num].ex_description;
           This; This = This->next) {
        if (This->keyword)
          temp->keyword = str_dup (This->keyword);
        if (This->description)
          temp->description = str_dup (This->description);
        if (This->next) {
          temp2 = new extra_descr_data;

          temp->next = temp2;
          temp = temp2;
        } else
          temp->next = NULL;
      }
    }

    d->edit_obj = obj;
#ifdef CONFIRM_EXISTING

    send_to_char ("An object already exists at that number. Do you wish to edit it?\r\n", ch);
    d->edit_mode = IEDIT_CONFIRM_EDIT;
#else

    iedit_disp_menu(d);
#endif

    return;
  } else {
    send_to_char ("That object does not exist, create it?\r\n", ch);
    /*
     * create dummy object!
     */
    d->edit_obj = Mem->GetObject();
    //clear_object (d->edit_obj);
    d->edit_obj->text.keywords = str_dup("object unfinished");
    d->edit_obj->text.name = str_dup("an unfinished object");
    d->edit_obj->text.room_desc =
      str_dup("An unfinished object is lying here.");
    d->edit_obj->text.look_desc =
      str_dup("It looks pretty much like an unfinished object");

    d->edit_obj->obj_flags.wear_flags.SetBit(ITEM_WEAR_TAKE);

    d->edit_obj->obj_flags.condition = d->edit_obj->obj_flags.barrier = 1;

    d->edit_mode = IEDIT_CONFIRM_EDIT;

    return;
  }
}

ACMD(do_iclone)
{

  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  int arg1, arg2, obj_num1, obj_num2, i, counter, zone1 = -1, zone2 = -1;

  two_arguments(argument, buf, buf1);

  if (!*buf || !*buf1) {
    send_to_char("Usage: iclone <old item vnum> <new item vnum>\r\n", ch);
    return;
  }
  arg1 = atoi(buf);
  arg2 = atoi(buf1);

  obj_num1 = real_object(arg1);
  obj_num2 = real_object(arg2);

  if (obj_num1 < 0) {
    send_to_char("Invalid item number.\r\n", ch);
    return;
  }
  if (obj_num2 > -1) {
    send_to_char("You cannot clone over an existing items.\r\n", ch);
    return;
  }

  for (counter = 0; counter <= top_of_zone_table; counter++) {
    if (arg1 >= (zone_table[counter].number * 100) && arg1 <= zone_table[counter].top)
      zone1 = counter;
    if (arg2 >= (zone_table[counter].number * 100) && arg2 <= zone_table[counter].top)
      zone2 = counter;
  }

  if (zone1 < 0 || zone2 < 0) {
    send_to_char("That number is not part of any zone.\r\n", ch);
    return;
  }

  for (i = 0; i < 5; i++)
    if (zone_table[zone1].editor_ids[i] == GET_IDNUM(ch))
      break;
  if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) {
    send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
    return;
  }

  for (i = 0; i < 5; i++)
    if (zone_table[zone2].editor_ids[i] == GET_IDNUM(ch))
      break;
  if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) {
    send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
    return;
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't clone objects into a connected zone.\r\n", ch);
    return;
  }

  // now for the fun part
  // first duplicate the obj
  struct extra_descr_data *This, *temp, *temp2;
  struct obj_data *obj;
  obj = Mem->GetObject();
  //clear_object (obj);
  *obj = obj_proto[obj_num1];

  // copy the strings over
  if (obj_proto[obj_num1].text.keywords)
    obj->text.keywords = str_dup(obj_proto[obj_num1].text.keywords);
  if (obj_proto[obj_num1].text.name)
    obj->text.name = str_dup(obj_proto[obj_num1].text.name);
  if (obj_proto[obj_num1].text.room_desc)
    obj->text.room_desc = str_dup(obj_proto[obj_num1].text.room_desc);
  if (obj_proto[obj_num1].text.look_desc)
    obj->text.look_desc = str_dup(obj_proto[obj_num1].text.look_desc);

  // extra descriptions done next
  if (obj_proto[obj_num1].ex_description) {
    temp = new extra_descr_data;

    obj->ex_description = temp;
    for (This = obj_proto[obj_num1].ex_description; This; This = This->next) {
      if (This->keyword)
        temp->keyword = str_dup (This->keyword);
      if (This->description)
        temp->description = str_dup (This->description);
      if (This->next) {
        temp2 = new extra_descr_data;

        temp->next = temp2;
        temp = temp2;
      } else
        temp->next = NULL;
    }
  }

  // now send em into editing mode real quick and confirm their action
  STATE(ch->desc) = CON_IEDIT;
  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  send_to_char("Are you sure you want to clone that object?\r\n", ch);
  ch->desc->edit_number = arg2; // the vnum
  ch->desc->edit_obj = obj;
  ch->desc->edit_mode = IEDIT_CONFIRM_SAVESTRING;

}

ACMD(do_idelete)
{
  int num, counter, i, found = 0;
  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  one_argument(argument, buf);

  if (!*buf) {
    send_to_char("Usage: idelete <vnum>\r\n", ch);
    return;
  }

  num = atoi(buf);

  if (num == -1) {
    send_to_char("No such object.\r\n", ch);
    return;
  }

  for (counter = 0; counter <= top_of_zone_table; counter++) {
    if ((atoi(buf) >= (zone_table[counter].number * 100)) && (atoi(buf) <=
        (zone_table[counter].top))) {
      found = 1;
      break;
    }
  }
  if (!found) {
    send_to_char ("Sorry, that number is not part of any zone.\r\n", ch);
    return;
  }

  for (i = 0; i < 5; i++)
    if (zone_table[counter].editor_ids[i] == GET_IDNUM(ch))
      break;
  if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) {
    send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
    return;
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't delete objects from a connected zone.\r\n", ch);
    return;
  }

  num = real_object(num);

  ch->player_specials->saved.zonenum = zone_table[counter].number;
  ObjList.RemoveObjNum(num);
  Mem->DeleteObject(&obj_proto[num]);

  for (counter = num; counter < top_of_objt; counter++) {
    ObjList.UpdateObjs(&obj_proto[counter+1], counter);
    obj_index[counter] = obj_index[counter + 1];
    obj_proto[counter] = obj_proto[counter + 1];
    obj_proto[counter].item_number = counter;
  }

  // update the zones by decrementing numbers if >= number deleted
  int zone, cmd_no;
  for (zone = 0; zone <= top_of_zone_table; zone++) {
    for (cmd_no = 0; cmd_no < zone_table[zone].num_cmds; cmd_no++)
      switch (ZCMD.command) {
      case 'E':
      case 'G':
      case 'O':
      case 'C':
      case 'N':
        if (ZCMD.arg1 == num) {
          ZCMD.command = '*';
          ZCMD.if_flag = 0;
          ZCMD.arg1 = 0;
          ZCMD.arg2 = 0;
          ZCMD.arg3 = 0;
        }
        break;
      case 'R':
        if (ZCMD.arg2 == num) {
          ZCMD.command = '*';
          ZCMD.if_flag = 0;
          ZCMD.arg1 = 0;
          ZCMD.arg2 = 0;
          ZCMD.arg3 = 0;
        }
        break;
      case 'P':
        if (ZCMD.arg3 == num || ZCMD.arg1 == num) {
          ZCMD.command = '*';
          ZCMD.if_flag = 0;
          ZCMD.arg1 = 0;
          ZCMD.arg2 = 0;
          ZCMD.arg3 = 0;
        }
        break;
      }
  }

  top_of_objt--;
  write_zone_to_disk(ch->player_specials->saved.zonenum);
  write_objs_to_disk(ch->player_specials->saved.zonenum);
  send_to_char("Done.\r\n", ch);
}

#define MOB d->edit_mob
// medit command
ACMD(do_medit)
{
  int number;
  int mob_num;
  struct descriptor_data *d;
  char arg1[MAX_INPUT_LENGTH];
  struct char_data *mob;
  int counter, found = 0;

  // they must be flagged with olc to edit
  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  if (!olc_state) {
    send_to_char("OLC temporarily unavailable.\r\n", ch);
    return;
  }

  one_argument (argument, arg1);

  // if no argument return
  if (!*argument) {
    send_to_char ("Specify a mob number to edit.\r\n", ch);
    return;
  }
  // is argument a number?
  if (!isdigit (*arg1)) {
    send_to_char ("Please supply a valid number.\r\n", ch);
    return;
  }

  number = atoi (arg1);
  // check if number already exists
  mob_num = real_mobile(number);

  //check zone numbers
  for (counter = 0; counter <= top_of_zone_table; counter++) {
    if ((number >= (zone_table[counter].number * 100)) && (number <= (zone_table[counter].top))) {
      ch->desc->edit_zone = counter;
      found = 1;
      break;
    }
  }
  if (!found) {
    send_to_char ("Sorry, that number is not part of any zone.\r\n", ch);
    return;
  }
  // only allow them to edit their zone
  // this is part of the new routine that checks for the id list from the
  // actual zone itself to determine if they can edit it or not
  {
    int i = 0;
    for (; i < 5; i++)
      if (zone_table[counter].editor_ids[i] == GET_IDNUM(ch))
        break;
    if ((i >= 5) && (!access_level(ch, LVL_ADMIN)))
    {
      send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
      return;
    }
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't edit mobiles from a connected zone.\r\n", ch);
    return;
  }

  ch->player_specials->saved.zonenum = zone_table[counter].number;


  // put person into editing mode
  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  //now start playing with the descriptor
  d = ch->desc;
  STATE (d) = CON_MEDIT;

  d->edit_number = number;      /*
                                                           * the VNUM not the REAL NUMBER
                                                           */

  if (mob_num >= 0) {
    // allocate mobile
    mob = Mem->GetCh();

    *mob = mob_proto[mob_num]; // the RNUM

    // copy all strings over
    if (mob_proto[mob_num].player.physical_text.keywords)
      mob->player.physical_text.keywords =
        str_dup(mob_proto[mob_num].player.physical_text.keywords);
    if (mob_proto[mob_num].player.physical_text.name)
      mob->player.physical_text.name =
        str_dup(mob_proto[mob_num].player.physical_text.name);
    if (mob_proto[mob_num].player.physical_text.room_desc)
      mob->player.physical_text.room_desc =
        str_dup(mob_proto[mob_num].player.physical_text.room_desc);
    if (mob_proto[mob_num].player.physical_text.look_desc)
      mob->player.physical_text.look_desc =
        str_dup(mob_proto[mob_num].player.physical_text.look_desc);

    if (mob_proto[mob_num].char_specials.arrive)
      mob->char_specials.arrive = str_dup(mob_proto[mob_num].char_specials.arrive);
    if (mob_proto[mob_num].char_specials.leave)
      mob->char_specials.leave = str_dup(mob_proto[mob_num].char_specials.leave);
    if (mob_proto[mob_num].player_specials)
      mob->player_specials = &dummy_mob;

    d->edit_mob = mob;
#ifdef CONFIRM_EXISTING

    send_to_char ("A mob already exists at that number. Do you wish to edit it?\r\n", ch);
    d->edit_mode = MEDIT_CONFIRM_EDIT;
#else

    medit_disp_menu(d);
#endif

    return;
  } else {
    send_to_char("That mobile does not exist, create it?\r\n", ch);
    // create a dummy mobile
    d->edit_mob = Mem->GetCh();

    d->edit_mob->player_specials = &dummy_mob;

    d->edit_mob->player.physical_text.keywords = str_dup("mob unfinished");
    d->edit_mob->player.physical_text.name = str_dup("an unfinished mob");
    d->edit_mob->player.physical_text.room_desc = str_dup("An unfinished mob stands here.");
    d->edit_mob->player.physical_text.look_desc =
      str_dup("It looks barely coherent as it waits to be created.\r\n");

    d->edit_mob->player.title = NULL;
    d->edit_mob->char_specials.arrive = str_dup("arrives from");
    d->edit_mob->char_specials.leave = str_dup("leaves");

    d->edit_mob->desc = NULL;

    MOB_FLAGS(d->edit_mob).SetBit(MOB_ISNPC);

    // set some default variables here
    GET_MAX_PHYSICAL(MOB) = 1000;
    GET_PHYSICAL(MOB) = 1000;
    GET_MAX_MENTAL(MOB) = 1000;
    GET_MENTAL(MOB) = 1000;
    GET_ESS(MOB) = 600;
    MOB->real_abils.ess = 600;
    GET_POS(MOB) = POS_STANDING;
    GET_SEX(MOB) = SEX_NEUTRAL;
    MOB->mob_specials.attack_type = 300;
    d->edit_mode = MEDIT_CONFIRM_EDIT;
    return;
  }
}

ACMD(do_mclone)
{

  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  int arg1, arg2, mob_num1, mob_num2, counter, i, zone1 = -1, zone2 = -2;

  two_arguments(argument, buf, buf1);

  if (!*buf || !*buf1) {
    send_to_char("Usage: mclone <old mob vnum> <new mob vnum>\r\n", ch);
    return;
  }
  arg1 = atoi(buf);
  arg2 = atoi(buf1);

  for (counter = 0; counter <= top_of_zone_table; counter++) {
    if (arg1 >= (zone_table[counter].number * 100) && arg1 <= zone_table[counter].top)
      zone1 = counter;
    if (arg2 >= (zone_table[counter].number * 100) && arg2 <= zone_table[counter].top)
      zone2 = counter;
  }

  if (zone1 < 0 || zone2 < 0) {
    send_to_char("That number is not part of any zone.\r\n", ch);
    return;
  }

  for (i = 0; i < 5; i++)
    if (zone_table[zone1].editor_ids[i] == GET_IDNUM(ch))
      break;
  if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) {
    send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
    return;
  }

  for (i = 0; i < 5; i++)
    if (zone_table[zone2].editor_ids[i] == GET_IDNUM(ch))
      break;
  if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) {
    send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
    return;
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't clone mobiles to a connected zone.\r\n", ch);
    return;
  }

  mob_num1 = real_mobile(arg1);
  mob_num2 = real_mobile(arg2);

  if (mob_num1 < 0) {
    send_to_char("Invalid item number.\r\n", ch);
    return;
  }
  if (mob_num2 > -1) {
    send_to_char("You cannot clone over an existing mob.\r\n", ch);
    return;
  }

  // now for the fun part
  // first duplicate the mob
  struct char_data *mob;
  // allocate the structure
  mob = Mem->GetCh();

  *mob = mob_proto[mob_num1]; // the RNUM

  // copy all strings over
  if (mob_proto[mob_num1].player.physical_text.keywords)
    mob->player.physical_text.keywords =
      str_dup(mob_proto[mob_num1].player.physical_text.keywords);
  if (mob_proto[mob_num1].player.physical_text.name)
    mob->player.physical_text.name =
      str_dup(mob_proto[mob_num1].player.physical_text.name);
  if (mob_proto[mob_num1].player.physical_text.room_desc)
    mob->player.physical_text.room_desc =
      str_dup(mob_proto[mob_num1].player.physical_text.room_desc);
  if (mob_proto[mob_num1].player.physical_text.look_desc)
    mob->player.physical_text.look_desc =
      str_dup(mob_proto[mob_num1].player.physical_text.look_desc);

  if (mob_proto[mob_num1].player_specials)
    mob->player_specials = &dummy_mob;

  // put guy into editing mode
  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  STATE (ch->desc) = CON_MEDIT;
  send_to_char("Do you wish to clone this mob?\r\n", ch);
  ch->desc->edit_number = arg2;
  ch->desc->edit_mob = mob;
  ch->desc->edit_mode = MEDIT_CONFIRM_SAVESTRING;
}

ACMD(do_mdelete)
{
  int num, c, counter, i, found = 0;

  one_argument(argument, buf);

  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  if (!*buf) {
    send_to_char("Usage: mdelete <vnum>\r\n", ch);
    return;
  }

  num = atoi(buf);

  if (num == -1) {
    send_to_char("No such mob.\r\n", ch);
    return;
  }

  for (counter = 0; counter <= top_of_zone_table; counter++) {
    if ((atoi(buf) >= (zone_table[counter].number * 100)) && (atoi(buf) <= (zone_table[counter].top))) {
      found = 1;
      break;
    }
  }
  if (!found) {
    send_to_char ("Sorry, that number is not part of any zone.\r\n", ch);
    return;
  }

  for (i = 0; i < 5; i++)
    if (zone_table[counter].editor_ids[i] == GET_IDNUM(ch))
      break;
  if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) {
    send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
    return;
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't delete mobiles from a connected zone.\r\n", ch);
    return;
  }

  ch->player_specials->saved.zonenum = zone_table[counter].number;
  num = real_mobile(num);

  struct char_data *j, *temp, *next_char;
  /* cycle through characters, purging soon-to-be-delete mobs as we go */
  for (temp = character_list; temp; temp = next_char) {
    next_char = temp->next;
    if (IS_NPC(temp) && GET_MOB_RNUM(temp) == num) {
      act("$n disintegrates.", FALSE, temp, 0, 0, TO_ROOM);
      extract_char(temp);
    }
  }

  Mem->DeleteCh(&mob_proto[num]);

  for (counter = num; counter < top_of_mobt; counter++) {
    for (j = character_list; j; j = j->next) {
      if (j->nr == counter) {
        temp = new char_data;
        *temp = *j;
        *j = mob_proto[counter + 1];
        j->in_room = temp->in_room;
        j->nr = counter;
        j->carrying = temp->carrying;
        for (c = 0; c < NUM_WEARS; c++)
          j->equipment[c] = temp->equipment[c];
        j->next_in_room = temp->next_in_room;
        j->next = temp->next;
        Mem->ClearCh(temp);
      }
    }
    mob_index[counter] = mob_index[counter + 1];
    mob_proto[counter] = mob_proto[counter + 1];
    mob_proto[counter].nr = counter;
  }

  // update the zones by decrementing numbers if >= number deleted
  int zone, cmd_no, last = 0;
  for (zone = 0; zone <= top_of_zone_table; zone++) {
    for (cmd_no = 0; cmd_no < zone_table[zone].num_cmds; cmd_no++)
      switch (ZCMD.command) {
      case 'M':
        last = ZCMD.arg1;
        if (ZCMD.arg1 == num) {
          ZCMD.command = '*';
          ZCMD.if_flag = 0;
          ZCMD.arg1 = 0;
          ZCMD.arg2 = 0;
          ZCMD.arg3 = 0;
        } else
          ZCMD.arg1 = (ZCMD.arg1 > num ? ZCMD.arg1 - 1 : ZCMD.arg1);
        break;
      case 'E':
      case 'G':
      case 'C':
      case 'N':
        if (last == num) {
          ZCMD.command = '*';
          ZCMD.if_flag = 0;
          ZCMD.arg1 = 0;
          ZCMD.arg2 = 0;
          ZCMD.arg3 = 0;
        }
        break;
      }
  }

  top_of_mobt--;
  write_zone_to_disk(ch->player_specials->saved.zonenum);
  write_mobs_to_disk(ch->player_specials->saved.zonenum);
  send_to_char("Done.\r\n", ch);
}

ACMD(do_qedit)
{
  int number, rnum, counter, found = 0, i;
  char arg1[MAX_INPUT_LENGTH];
  struct descriptor_data *d;
  struct quest_data *qst;

  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  if (!olc_state) {
    send_to_char("OLC temporarily unavailable.\r\n", ch);
    return;
  }

  any_one_arg(argument, arg1);

  if (!*argument) {
    send_to_char ("Specify a quest number to edit.\r\n", ch);
    return;
  }

  if (!isdigit(*arg1)) {
    send_to_char ("Please supply a valid number.\r\n", ch);
    return;
  }

  number = atoi(arg1);

  rnum = real_quest(number);

  for (counter = 0; counter <= top_of_zone_table; counter++) {
    if ((number >= (zone_table[counter].number * 100)) &&
        (number <= (zone_table[counter].top))) {
      ch->desc->edit_zone = counter;
      found = 1;
      break;
    }
  }
  if (!found) {
    send_to_char ("Sorry, that number is not part of any zone.\r\n", ch);
    return;
  }

  for (i = 0; i < 5; i++)
    if (zone_table[counter].editor_ids[i] == GET_IDNUM(ch))
      break;
  if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) {
    send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
    return;
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't edit quests from a connected zone.\r\n", ch);
    return;
  }

  ch->player_specials->saved.zonenum = zone_table[counter].number;

  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  d = ch->desc;
  STATE(d) = CON_QEDIT;

  d->edit_number = number;

  if (rnum >= 0) {

    qst = new quest_data;

    *qst = quest_table[rnum];

    qst->obj = new struct quest_om_data[QMAX_OBJS];
    for (i = 0; i < qst->num_objs; i++)
      qst->obj[i] = quest_table[rnum].obj[i];

    qst->mob = new struct quest_om_data[QMAX_MOBS];
    for (i = 0; i < qst->num_mobs; i++)
      qst->mob[i] = quest_table[rnum].mob[i];

    if (quest_table[rnum].intro)
      qst->intro = str_dup(quest_table[rnum].intro);
    if (quest_table[rnum].decline)
      qst->decline = str_dup(quest_table[rnum].decline);
    if (quest_table[rnum].quit)
      qst->quit = str_dup(quest_table[rnum].quit);
    if (quest_table[rnum].finish)
      qst->finish = str_dup(quest_table[rnum].finish);
    if (quest_table[rnum].info)
      qst->info = str_dup(quest_table[rnum].info);
    if (quest_table[rnum].done)
      qst->done = str_dup(quest_table[rnum].done);

    d->edit_quest = qst;
#ifdef CONFIRM_EXISTING

    d->edit_mode = QEDIT_CONFIRM_EDIT;
    send_to_char("A quest already exists at that number.  "
                 "Do you wish to edit it?\r\n", ch);
#else

    qedit_disp_menu(d);
#endif

  } else {
    send_to_char("That quest does not exist, create it?\r\n", ch);

    d->edit_quest = new quest_data;
    d->edit_quest->vnum = d->edit_number;
    d->edit_quest->obj = new struct quest_om_data[QMAX_OBJS];
    d->edit_quest->mob = new struct quest_om_data[QMAX_MOBS];

    d->edit_quest->intro = str_dup("I've got an incomplete quest for you.");
    d->edit_quest->decline = str_dup("That's too bad...later, chummer.");
    d->edit_quest->quit =
      str_dup("Null sweat, chummer.  Someone else'll finish the job");
    d->edit_quest->finish = str_dup("Well done.");
    d->edit_quest->info =
      str_dup("Well you see, this quest is rather incomplete, "
              "so I've got no info on it.");

    d->edit_mode = QEDIT_CONFIRM_EDIT;
  }
}

ACMD(do_shedit)
{
  int number = 0, counter, i = 0, found = 0;
  struct shop_data *shop = NULL;
  struct descriptor_data *d;

  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  if (!olc_state) {
    send_to_char("OLC temporarily unavailable.\r\n", ch);
    return;
  }

  one_argument(argument, arg);

  if (!*arg) {
    send_to_char ("Specify a shop number to edit.\r\n", ch);
    return;
  }

  if (!isdigit(*arg)) {
    send_to_char ("Please supply a valid number.\r\n", ch);
    return;
  }

  number = atoi(arg);
  for (counter = 0; counter <= top_of_zone_table; counter++) {
    if ((number >= (zone_table[counter].number * 100)) && (number <= (zone_table[counter].top))) {
      found = 1;
      break;
    }
  }
  if (!found) {
    send_to_char ("Sorry, that number is not part of any zone.\r\n", ch);
    return;
  }

  for (; i < 5; i++)
    if (zone_table[counter].editor_ids[i] == GET_IDNUM(ch))
      break;
  if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) {
    send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
    return;
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't edit shops from a connected zone.\r\n", ch);
    return;
  }

  ch->player_specials->saved.zonenum = zone_table[counter].number;

  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  d = ch->desc;
  STATE(d) = CON_SHEDIT;
  d->edit_number = number;

  if (real_shop(number) > -1) {
    number = real_shop(number);
    shop = new shop_data;
    shop->vnum = shop_table[number].vnum;

    if (shop_table[number].selling) {
      struct shop_sell_data *This, *temp, *temp2;

      temp = new shop_sell_data;
      shop->selling = temp;
      for (This = shop_table[number].selling; This; This = This->next) {
        *temp = *This;
        if (This->next) {
          temp2 = new shop_sell_data;
          temp->next = temp2;
          temp = temp2;
        } else
          temp->next = NULL;
      }
    }
    shop->random_amount = shop_table[number].random_amount;
    shop->random_current = shop_table[number].random_current;
    shop->profit_buy = shop_table[number].profit_buy;
    shop->profit_sell = shop_table[number].profit_sell;
    shop->buytypes = shop_table[number].buytypes;
    shop->type = shop_table[number].type;
    shop->flags = shop_table[number].flags;
    shop->keeper = shop_table[number].keeper;
    shop->races = shop_table[number].races;
    shop->ettiquete = shop_table[number].ettiquete;
    shop->open = shop_table[number].open;
    shop->close = shop_table[number].close;
    if (shop_table[number].no_such_itemk)
      shop->no_such_itemk = str_dup(shop_table[number].no_such_itemk);
    if (shop_table[number].no_such_itemp)
      shop->no_such_itemp = str_dup(shop_table[number].no_such_itemp);
    if (shop_table[number].not_enough_nuyen)
      shop->not_enough_nuyen = str_dup(shop_table[number].not_enough_nuyen);
    if (shop_table[number].doesnt_buy)
      shop->doesnt_buy = str_dup(shop_table[number].doesnt_buy);
    if (shop_table[number].buy)
      shop->buy = str_dup(shop_table[number].buy);
    if (shop_table[number].sell)
      shop->sell = str_dup(shop_table[number].sell);
    if (shop_table[number].shopname)
      shop->shopname = str_dup(shop_table[number].shopname);
    d->edit_shop = shop;
#ifdef CONFIRM_EXISTING

    send_to_char("A shop already exists in that number...edit it?\r\n", ch);
    d->edit_mode = SHEDIT_CONFIRM_EDIT;
#else

    shedit_disp_menu(d);
#endif

  } else {
    send_to_char("That shop does not exist...create it?\r\n", ch);
    d->edit_shop = new shop_data;
    memset((char *) d->edit_shop, 0, sizeof(struct shop_data));
    d->edit_shop->vnum = d->edit_number;
    d->edit_shop->profit_buy = 1.0;
    d->edit_shop->profit_sell = 1.0;
    d->edit_shop->buytypes = 0;
    d->edit_shop->no_such_itemk = str_dup("Sorry, we don't have that.");
    d->edit_shop->no_such_itemp = str_dup("You don't seem to have that.");
    d->edit_shop->not_enough_nuyen = str_dup("You don't have enough nuyen!");
    d->edit_shop->doesnt_buy = str_dup("We don't buy that.");
    d->edit_shop->buy = str_dup("That'll be %d nuyen.");
    d->edit_shop->sell = str_dup("Here's your %d nuyen.");
    d->edit_shop->keeper = -1;
    d->edit_shop->open = 0;
    d->edit_shop->type = SHOP_GREY;
    d->edit_shop->ettiquete = SKILL_STREET_ETIQUETTE;
    d->edit_mode = SHEDIT_CONFIRM_EDIT;
  }
}

ACMD(do_zswitch)
{
  int counter = 0, number, zonenum;
  char arg1[MAX_INPUT_LENGTH];

  // they must be flagged with olc to zswitch
  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  if (!olc_state) {
    send_to_char("OLC temporarily unavailable.\r\n", ch);
    return;
  }

  // we just wanna check the zone number they want to switch to
  any_one_arg(argument, arg1);

  // convert the arg to an int
  number = atoi(arg1);
  // find the real zone number
  zonenum = real_zone(number);

  // and see if they can edit it
  {
    int i = 0;
    for (; i < 5; i++)
      if (i != -1 && zone_table[zonenum].editor_ids[i] == GET_IDNUM(ch))
        break;
    if ((i >= 5) && (!access_level(ch, LVL_ADMIN)))
    {
      send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
      return;
    }
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't switch to a connected zone.\r\n", ch);
    return;
  }

  // and set their zonenum to it
  ch->player_specials->saved.zonenum = number;
}

ACMD(do_zedit)
{
  struct descriptor_data *d = ch->desc;
  int counter = 0, number, zonenum;

  bool add
    = FALSE;
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];

  d = ch->desc;

  // they must be flagged with olc to edit
  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  if (!olc_state) {
    send_to_char("OLC temporarily unavailable.\r\n", ch);
    return;
  }

  two_arguments(argument, arg1, arg2);

  // they can only edit their zone, and since we know their zone number
  // then we do not have to check for boundaries

  zonenum = real_zone(ch->player_specials->saved.zonenum);

  // this is part of the new routine that checks for the id list from the
  // actual zone itself to determine if they can edit it or not
  {
    int i = 0;
    for (; i < 5; i++)
      if (zone_table[zonenum].editor_ids[i] == GET_IDNUM(ch))
        break;
    if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) // it was not found
    {
      send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
      return;
    }
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't edit rooms from a connected zone.\r\n", ch);
    return;
  }

  if (!*arg1) {
    PLR_FLAGS(ch).SetBit(PLR_EDITING);
    STATE(d) = CON_ZEDIT;
    if (zonenum > -1) {
      send_to_char("The zone data for that zone already exists.  Edit it?\r\n",
                   ch);
      d->edit_number = zonenum; // assign this rnum as the edit number
      d->edit_mode = ZEDIT_CONFIRM_EDIT_DATA;
    } else {
      send_to_char("The zone data does not exist for that zone.  Create it?\r\n",
                   ch);
      d->edit_mode = ZEDIT_CONFIRM_CREATE_DATA;
      d->edit_number = zonenum;
    }
    return;
  } else {
    // they have to use 'zedit' first
    if (zonenum < 0) {
      send_to_char("You must create the zone with 'zedit' first.\r\n",
                   ch);
      return;
    }
    if (is_abbrev(arg1, "add"))
      add
        = TRUE; // might as well use the same variable

    // this time, send em to the right place to add a new command
    if (add
       ) {
      PLR_FLAGS(ch).SetBit(PLR_EDITING);
      STATE(d) = CON_ZEDIT;
      // the edit number = command number in array
      d->edit_number = zone_table[zonenum].num_cmds;
#ifdef CONFIRM_EXISTING

      send_to_char("Add a new command to the zone table?\r\n", ch);
      d->edit_mode = ZEDIT_CONFIRM_ADD_CMD;
#else

      d->edit_cmd = new reset_com;
      d->edit_cmd->command = '*';
      zedit_disp_command_menu(d);
#endif

      return;
    }

    if (is_abbrev(arg1, "insert")) {
      add
        = TRUE;
      number = atoi(arg2);
    } else
      // finally, we check for the number
      number = atoi(arg1); // convert string to int

    // the next two ifs return if value is out of range
    if (number < 0) {
      send_to_char(ch, "Command numbers must range from 0 to %d\r\n",
                   zone_table[zonenum].num_cmds);
      return;
    }

    if (number > zone_table[zonenum].num_cmds - 1) {
      send_to_char("To add a new command, use 'zedit add'.\r\n", ch);
      return;
    }
    // just in case they do 0 with 0 cmds
    if ((number == 0) && (zone_table[zonenum].num_cmds == 0)) {
      send_to_char("To add a new command, use 'zedit add'.\r\n", ch);
      return;
    }

    // now put into editing mode
    PLR_FLAGS(ch).SetBit(PLR_EDITING);
    STATE(d) = CON_ZEDIT;

    // set the number to the command number if it exists, else to -1
    // in this case, it is the 'real number' of the zone

    d->edit_number = number;
    d->edit_number2 = 0;
    if (add
       ) {
      send_to_char(ch, "Insert a command at %d?\r\n", number);
      d->edit_mode = ZEDIT_CONFIRM_INSERT_CMD;
    }
    else {
      send_to_char("That command exists, edit it?\r\n", ch);
      d->edit_mode = ZEDIT_CONFIRM_EDIT_CMD;
    }
  }
  return;
}

ACMD(do_zdelete)
{}

ACMD(do_hedit)
{
  struct descriptor_data *d;
  char arg1[MAX_INPUT_LENGTH];
  struct host_data *host;
  int counter, found = 0;
  long host_num, number;
  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  if (!olc_state) {
    send_to_char("OLC temporarily unavailable.\r\n", ch);
    return;
  }

  one_argument (argument, arg1);

  if (!*argument) {
    send_to_char ("Specify a host number to edit.\r\n", ch);
    return;
  }
  if (!isdigit (*arg1)) {
    send_to_char ("Please supply a valid number.\r\n", ch);
    return;
  }

  number = atoi(arg1);
  host_num = real_host(number);

  for (counter = 0; counter <= top_of_zone_table; counter++) {
    if ((number >= (zone_table[counter].number * 100)) &&
        (number <= (zone_table[counter].top))) {
      ch->desc->edit_zone = counter;
      found = 1;
      break;
    }
  }
  if (!found) {
    send_to_char ("Sorry, that number is not part of any zone.\r\n", ch);
    return;
  }

  // this is part of the new routine that checks for the id list from the
  // actual zone itself to determine if they can edit it or not
  {
    int i = 0;
    for (; i < 5; i++)
      if (zone_table[counter].editor_ids[i] == GET_IDNUM(ch))
        break;
    if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) // it wasn't found
    {
      send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
      return;
    }
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't edit hosts from a connected zone.\r\n", ch);
    return;
  }

  ch->player_specials->saved.zonenum = zone_table[counter].number;

  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  d = ch->desc;
  STATE (d) = CON_HEDIT;
  d->edit_number = number;
  if (host_num >= 0) {
    host = Mem->GetHost();
    *host = matrix[host_num];
    if (matrix[host_num].name)
      host->name = str_dup(matrix[host_num].name);
    if (matrix[host_num].keywords)
      host->keywords = str_dup(matrix[host_num].keywords);
    if (matrix[host_num].desc)
      host->desc = str_dup(matrix[host_num].desc);
    if (matrix[host_num].shutdown_start)
      host->shutdown_start = str_dup(matrix[host_num].shutdown_start);
    if (matrix[host_num].shutdown_stop)
      host->shutdown_stop = str_dup(matrix[host_num].shutdown_stop);
    if (matrix[host_num].trigger) {
      struct trigger_step *This, *temp, *temp2;

      temp = new trigger_step;
      host->trigger = temp;
      for (This = matrix[host_num].trigger; This; This = This->next) {
        *temp = *This;
        if (This->next) {
          temp2 = new trigger_step;
          temp->next = temp2;
          temp = temp2;
        } else
          temp->next = NULL;
      }
    }

    if (matrix[host_num].exit) {
      struct exit_data *This, *temp, *temp2;

      temp = new exit_data;
      host->exit = temp;
      for (This = matrix[host_num].exit; This; This = This->next) {
        *temp = *This;
        if (This->number)
          temp->number = str_dup(This->number);
        if (This->next) {
          temp2 = new exit_data;
          temp->next = temp2;
          temp = temp2;
        } else
          temp->next = NULL;
      }
    }

    d->edit_host = host;
#ifdef CONFIRM_EXISTING

    send_to_char ("An matrix host already exists at that number. Do you wish to edit it?\r\n", ch);
    d->edit_mode = HEDIT_CONFIRM_EDIT;
#else

    hedit_disp_data_menu(d);
#endif

    return;
  } else {
    send_to_char ("That host does not exist, create it?\r\n", ch);
    d->edit_host = Mem->GetHost();
    d->edit_host->name = str_dup("An unfinished host");
    d->edit_host->keywords = str_dup("unfinished host");
    d->edit_host->desc = str_dup("This host is unfinished.");
    d->edit_host->shutdown_start = str_dup("A deep echoing voice announces a host shutdown.\r\n");
    d->edit_host->shutdown_stop = str_dup("A deep echoing voice announces the shutdown has been aborted.\r\n");
    d->edit_mode = HEDIT_CONFIRM_EDIT;
    return;
  }
}

ACMD(do_icedit)
{
  struct descriptor_data *d;
  char arg1[MAX_INPUT_LENGTH];
  struct matrix_icon *icon;
  int counter, found = 0;
  long ic_num, number;
  if (!PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  if (!olc_state) {
    send_to_char("OLC temporarily unavailable.\r\n", ch);
    return;
  }

  one_argument (argument, arg1);

  if (!*argument) {
    send_to_char ("Specify a IC number to edit.\r\n", ch);
    return;
  }
  if (!isdigit (*arg1)) {
    send_to_char ("Please supply a valid number.\r\n", ch);
    return;
  }

  number = atoi(arg1);
  ic_num = real_ic(number);

  for (counter = 0; counter <= top_of_zone_table; counter++) {
    if ((number >= (zone_table[counter].number * 100)) &&
        (number <= (zone_table[counter].top))) {
      ch->desc->edit_zone = counter;
      found = 1;
      break;
    }
  }
  if (!found) {
    send_to_char ("Sorry, that number is not part of any zone.\r\n", ch);
    return;
  }

  // this is part of the new routine that checks for the id list from the
  // actual zone itself to determine if they can edit it or not
  {
    int i = 0;
    for (; i < 5; i++)
      if (zone_table[counter].editor_ids[i] == GET_IDNUM(ch))
        break;
    if ((i >= 5) && (!access_level(ch, LVL_ADMIN))) // it wasn't found
    {
      send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
      return;
    }
  }

  if (!(access_level(ch, LVL_EXECUTIVE) || PLR_FLAGGED(ch, PLR_EDCON)) && zone_table[counter].connected) {
    send_to_char("You can't edit hosts from a connected zone.\r\n", ch);
    return;
  }

  ch->player_specials->saved.zonenum = zone_table[counter].number;

  PLR_FLAGS(ch).SetBit(PLR_EDITING);
  d = ch->desc;
  STATE (d) = CON_ICEDIT;
  d->edit_number = number;
  if (ic_num >= 0) {
    icon = Mem->GetIcon();
    *icon = ic_proto[ic_num];
    d->edit_icon = icon;
    if (ic_proto[ic_num].name)
      d->edit_icon->name = str_dup(ic_proto[ic_num].name);
    if (ic_proto[ic_num].look_desc)
      d->edit_icon->look_desc = str_dup(ic_proto[ic_num].look_desc);
    if (ic_proto[ic_num].long_desc)
      d->edit_icon->long_desc = str_dup(ic_proto[ic_num].long_desc);
#ifdef CONFIRM_EXISTING

    send_to_char ("An IC already exists at that number. Do you wish to edit it?\r\n", ch);
    d->edit_mode = ICEDIT_CONFIRM_EDIT;
#else

    icedit_disp_menu(d);
#endif

    return;
  } else {
    send_to_char ("That IC does not exist, create it?\r\n", ch);
    d->edit_icon = Mem->GetIcon();
    d->edit_icon->name = str_dup("An unfinished IC");
    d->edit_icon->look_desc = str_dup("An unfinished IC guards the node.");
    d->edit_icon->long_desc = str_dup("It looks like an unfinished IC.\r\n");
    d->edit_mode = ICEDIT_CONFIRM_EDIT;
    return;
  }
}
