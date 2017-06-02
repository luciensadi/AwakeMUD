/* ***************************************************
*   file: zedit.cc                                   *
*   (c) 1996-2000 Christopher J. Dickey, Andrew      *
*   Hynek, and Nick Robertson (c) 2001 The AwakeMUD  *
*   Consortium                                       *
*************************************************** */

#include <stdio.h>
#include <string.h>

#include "structs.h"
#include "awake.h"
#include "interpreter.h"
#include "comm.h"
#include "constants.h"
#include "utils.h"
#include "db.h"
#include <stdlib.h>
#include "boards.h"
#include "screen.h"
#include "olc.h"
#include "newmatrix.h"

extern const char *dirs[];
extern const char *short_where[];

const char *reset_mode[] =
  {
    "Don't reset",
    "Reset if no PCs are in zone",
    "Always reset",
    "\n"
  };

#define COM d->edit_cmd
#define ZONENUM d->character->player_specials->saved.zonenum
#define ZON d->edit_zon
#define MOB(rnum) MOB_VNUM_RNUM(rnum)
#define OBJ(rnum) OBJ_VNUM_RNUM(rnum)
#define ROOM(rnum) world[rnum].number
#define VEH(rnum) VEH_VNUM_RNUM(rnum)
#define HOST(rnum) matrix[rnum].vnum

#define ZONE zone_table[zonenum]
#define Zcmd zone_table[zonenum].cmd[i]
void write_zone_to_disk(int vnum)
{

  int zonenum = real_zone(vnum);

  if (zonenum < 0) {
    mudlog("Attempted to write Non-Existent zone!", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  FILE  *fp;
  int i;

  sprintf(buf, "%s/%d.zon", ZON_PREFIX, vnum);
  fp = fopen(buf, "w+");

  // write it out!
  fprintf(fp, "#%d\n", vnum);
  fprintf(fp, "%s~\n", ZONE.name);
  fprintf(fp, "%d %d %d %d %d %d\n", ZONE.top, ZONE.lifespan, ZONE.reset_mode, ZONE.security, ZONE.connected, ZONE.juridiction);
  fprintf(fp, "%d %d %d %d %d\n", ZONE.editor_ids[0], ZONE.editor_ids[1],
          ZONE.editor_ids[2], ZONE.editor_ids[3], ZONE.editor_ids[4]);
  for (i = 0; i < ZONE.num_cmds; ++i) {
    switch (ZONE.cmd[i].command) {
    case 'M':
      fprintf(fp, "%c %d %ld %ld %ld\n",
              Zcmd.command,
              Zcmd.if_flag,
              MOB(Zcmd.arg1),
              Zcmd.arg2,
              ROOM(Zcmd.arg3));
      break;
    case 'V':
      fprintf(fp, "%c %d %ld %ld %ld\n",
              Zcmd.command,
              Zcmd.if_flag,
              VEH(Zcmd.arg1),
              Zcmd.arg2,
              ROOM(Zcmd.arg3));

      break;
    case 'S':
      fprintf(fp, "%c %d %ld %ld\n",
              Zcmd.command,
              Zcmd.if_flag,
              MOB(Zcmd.arg1),
              Zcmd.arg2);
      break;
    case 'U':
      fprintf(fp, "%c %d %ld %ld\n",
              Zcmd.command,
              Zcmd.if_flag,
              OBJ(Zcmd.arg1),
              Zcmd.arg2);
      break;
    case 'I':
      fprintf(fp, "%c %d %ld %ld\n",
              Zcmd.command,
              Zcmd.if_flag,
              OBJ(Zcmd.arg1),
              Zcmd.arg2);
      break;
    case 'O':
      fprintf(fp, "%c %d %ld %ld %ld\n",
              Zcmd.command,
              Zcmd.if_flag,
              OBJ(Zcmd.arg1),
              Zcmd.arg2,
              ROOM(Zcmd.arg3));
      break;
    case 'H':
      fprintf(fp, "%c %d %ld %ld %ld\n",
              Zcmd.command,
              Zcmd.if_flag,
              OBJ(Zcmd.arg1),
              Zcmd.arg2,
              HOST(Zcmd.arg3));
      break;
    case 'P':
      fprintf(fp, "%c %d %ld %ld %ld\n",
              Zcmd.command,
              Zcmd.if_flag,
              OBJ(Zcmd.arg1),
              Zcmd.arg2,
              OBJ(Zcmd.arg3));
      break;
    case 'D':
      fprintf(fp, "%c %d %ld %ld %ld\n",
              Zcmd.command,
              Zcmd.if_flag,
              ROOM(Zcmd.arg1),
              Zcmd.arg2,
              Zcmd.arg3);
      break;
    case 'E':
      fprintf(fp, "L %d %ld %ld %ld\n",
              Zcmd.if_flag,
              OBJ(Zcmd.arg1),
              Zcmd.arg2,
              Zcmd.arg3);
      break;
    case 'N':
      fprintf(fp, "%c %d %ld %ld %ld\n",
              Zcmd.command,
              Zcmd.if_flag,
              OBJ(Zcmd.arg1),
              Zcmd.arg2,
              Zcmd.arg3);
      break;
    case 'G':
    case 'C':
      fprintf(fp, "%c %d %ld %ld\n",
              Zcmd.command,
              Zcmd.if_flag,
              OBJ(Zcmd.arg1),
              Zcmd.arg2);
      break;
    case 'R':
      fprintf(fp, "%c %d %ld %ld\n",
              Zcmd.command,
              Zcmd.if_flag,
              ROOM(Zcmd.arg1),
              OBJ(Zcmd.arg2));
      break;
    }
  }
  fprintf(fp, "$\n");
  fclose(fp);
}
#undef ZONE
#undef Zcmd

void zedit_disp_wear_menu(struct descriptor_data *d)
{
  CLS(CH);
  int counter;

  for (counter = 0; counter < (NUM_WEARS - 1); counter += 2)
    send_to_char(CH, "^G%2d^Y) ^W%-20s    ^G%2d^Y) ^W%-20s\r\n",
                 counter + 1, short_where[counter],
                 counter + 2, counter + 1 < (NUM_WEARS - 1) ?
                 short_where[counter + 1] : "");

  send_to_char("^wEnter wear position ^Y(^G0 ^Wto quit^Y)^w: ", CH);
}

void zedit_disp_state_menu(struct descriptor_data *d)
{
  send_to_char("\r\n^G1^Y) ^WOpen\r\n^G2^Y) ^WClosed\r\n^G3^Y) ^WClosed and locked\r\n"
               "^G0^Y) ^WQuit\r\n^wEnter a selection: ", CH);
}

void zedit_disp_data_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "^WZone: ^c%d^n\r\n", ZON->number );
  send_to_char(CH, "^G1^Y) ^WName: ^c%s^n\r\n", ZON->name );
  send_to_char(CH, "^G2^Y) ^WTop of zone: ^c%d^n\r\n", ZON->top );
  send_to_char(CH, "^G3^Y) ^WLifespan: ^c%d^n\r\n", ZON->lifespan );
  send_to_char(CH, "^G4^Y) ^WReset mode: ^c%s^n\r\n", reset_mode[ZON->reset_mode] );
  send_to_char(CH, "^G5^Y) ^WSecurity level: ^c%d^n\r\n", ZON->security );
  send_to_char(CH, "^G6^Y) ^WJuridiction: ^c%s^n\r\n", jurid[ZON->juridiction]);
  if (access_level(CH, LVL_VICEPRES))
  {
    send_to_char(CH, "^G7^Y) ^WEditor's ID Numbers: ^c%d^w, ^c%d^w, ^c%d^w, ^c%d^w, ^c%d^n\r\n",
                 ZON->editor_ids[0], ZON->editor_ids[1], ZON->editor_ids[2],
                 ZON->editor_ids[3], ZON->editor_ids[4]);
    send_to_char(CH, "^G8^Y) ^WConnected: ^c%d^n\r\n", ZON->connected);
  }

  send_to_char("^Gq^Y) ^WQuit\r\n^wEnter selection: ", CH);

  d->edit_mode = ZEDIT_DATA_MENU;
}

const char *get_type(char c)
{
  switch (c) {
  case 'M':
    return "MOB";
  case 'O':
    return "OBJECT";
  case 'P':
    return "PUT";
  case 'E':
    return "EQUIP";
  case 'G':
    return "GIVE";
  case 'R':
    return "REMOVE";
  case 'D':
    return "DOOR";
  case 'N':
    return "GIVENUM";
  case 'C':
    return "CYBER/BIOWARE";
  case 'V':
    return "VEHICLE";
  case 'S':
    return "PASSENGER";
  case 'U':
    return "UPGRADE";
  case 'I':
    return "CARGO";
  case 'H':
    return "HOST";
  default:
    return "NONE";
  }
}

char get_real_type(const char *i)
{
  switch (*i) {
  case '1':
    return 'M';
  case '2':
    return 'O';
  case '3':
    return 'P';
  case '4':
    return 'E';
  case '5':
    return 'G';
  case '6':
    return 'R';
  case '7':
    return 'D';
  case '8':
    return 'N';
  case '9':
    return 'C';
  case '0':
    return 'V';
  case 'a':
  case 'A':
    return 'S';
  case 'b':
  case 'B':
    return 'U';
  case 'c':
  case 'C':
    return 'I';
  case 'd':
  case 'D':
    return 'H';
  case 'n':
    return '*';
  default:
    return '*';
  }
}


void zedit_disp_type_cmd(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char("^G1^Y) ^WMOB\r\n"
               "^G2^Y) ^WOBJECT\r\n"
               "^G3^Y) ^WPUT\r\n"
               "^G4^Y) ^WEQUIP\r\n"
               "^G5^Y) ^WGIVE\r\n"
               "^G6^Y) ^WREMOVE\r\n"
               "^G7^Y) ^WDOOR\r\n"
               "^G8^Y) ^WGIVE NUMBER\r\n"
               "^G9^Y) ^WCYBER/BIOWARE\r\n"
               "^G0^Y) ^WVEHICLE\r\n"
               "^GA^Y) ^WDRIVER\r\n"
               "^GB^Y) ^WUPGRADE\r\n"
               "^GC^Y) ^WCARRIED\r\n"
               "^GD^Y) ^WHOST\r\n"
               "^Rn^Y) ^WNOTHING\r\n"
               "^wEnter command type: ", CH);
}

void zedit_disp_direction_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char("^G 1^Y) ^Wnorth\r\n"
               "^G 2^Y) ^Wnortheast\r\n"
               "^G 3^Y) ^Weast\r\n"
               "^G 4^Y) ^Wsoutheast\r\n"
               "^G 5^Y) ^Wsouth\r\n"
               "^G 6^Y) ^Wsouthwest\r\n"
               "^G 7^Y) ^Wwest\r\n"
               "^G 8^Y) ^Wnorthwest\r\n"
               "^G 9^Y) ^Wup\r\n"
               "^G10^Y) ^Wdown\r\n"
               "^wEnter selection: ", CH);
}

void zedit_disp_command_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "^WCommand: ^c%d^n\r\n", d->edit_number );
  send_to_char(CH, "^G1^Y) ^WType: ^c%s^n\r\n", get_type(COM->command) );
  send_to_char(CH, "^G2^Y) ^WConditional: ^c%s^n\r\n",
               (!COM->if_flag ? "Always" : "If Last") );
  switch (COM->command)
  {
  case 'M':
    send_to_char(CH, "^G3^Y) ^WLoad mob: ^c%s ^y(^B%d)^n\r\n",
                 GET_NAME(mob_proto+COM->arg1),
                 MOB(COM->arg1) );
    send_to_char(CH, "^G4^Y) ^WMaximum number in game: ^c%d^n\r\n",
                 COM->arg2 );
    send_to_char(CH, "^G5^Y) ^WLoad in room: ^c%s ^y(^B%d^y)^n\r\n",
                 world[COM->arg3].name,
                 ROOM(COM->arg3) );
    break;
  case 'S':
    send_to_char(CH, "^G3^Y) ^WLoad mob: ^c%s ^y(^B%d)^n\r\n",
                 GET_NAME(mob_proto+COM->arg1),
                 MOB(COM->arg1) );
    send_to_char(CH, "^G4^Y) ^WMaximum number in game: ^c%d^n\r\n",
                 COM->arg2 );
    break;
  case 'U':
    send_to_char(CH, "^G3^Y) ^WAttach obj: ^c%s ^y(^B%d^y)^n\r\n",
                 obj_proto[COM->arg1].text.name,
                 OBJ(COM->arg1) );
    send_to_char(CH, "^G4^Y) ^WMaximum number in game: ^c%d^n\r\n",
                 COM->arg2 );
    break;
  case 'I':
    send_to_char(CH, "^G3^Y) ^WCarry obj: ^c%s ^y(^B%d^y)^n\r\n",
                 obj_proto[COM->arg1].text.name,
                 OBJ(COM->arg1) );
    send_to_char(CH, "^G4^Y) ^WMaximum number in game: ^c%d^n\r\n",
                 COM->arg2 );
    break;
  case 'O':
    send_to_char(CH, "^G3^Y) ^WLoad obj: ^c%s ^y(^B%d^y)^n\r\n",
                 obj_proto[COM->arg1].text.name,
                 OBJ(COM->arg1) );
    send_to_char(CH, "^G4^Y) ^WMaximum number in game: ^c%d^n\r\n",
                 COM->arg2 );
    send_to_char(CH, "^G5^Y) ^WLoad in room: ^c%s ^y(^B%d^y)^n\r\n",
                 world[COM->arg3].name, ROOM(COM->arg3) );
    break;
  case 'H':
    send_to_char(CH, "^G3^Y) ^WLoad obj: ^c%s ^y(^B%d^y)^n\r\n",
                 obj_proto[COM->arg1].text.name,
                 OBJ(COM->arg1) );
    send_to_char(CH, "^G4^Y) ^WMaximum number in game: ^c%d^n\r\n",
                 COM->arg2 );
    send_to_char(CH, "^G5^Y) ^WLoad in host: ^c%s ^y(^B%d^y)^n\r\n",
                 matrix[COM->arg3].name, HOST(COM->arg3) );
    break;
  case 'V':
    send_to_char(CH, "^G3^Y) ^WLoad Veh: ^c%s ^y(^B%d^y)^n\r\n",
                 veh_proto[COM->arg1].short_description,
                 VEH(COM->arg1));
    send_to_char(CH, "^G4^Y) ^WMaximum number in game: ^c%d^n\r\n",
                 COM->arg2 );
    send_to_char(CH, "^G5^Y) ^WLoad in room: ^c%s ^y(^B%d^y)^n\r\n",
                 world[COM->arg3].name, ROOM(COM->arg3) );
    break;
  case 'P':
    send_to_char(CH, "^G3^Y) ^WPut obj: ^c%s ^y(^B%d^y)^n\r\n",
                 obj_proto[COM->arg1].text.name,
                 OBJ(COM->arg1) );
    send_to_char(CH, "^G4^Y) ^WMaximum number in game: ^c%d^n\r\n",
                 COM->arg2 );
    send_to_char(CH, "^G5^Y) ^WInto obj: ^c%s ^y(^B%d^y)^n\r\n",
                 obj_proto[COM->arg3].text.name,
                 OBJ(COM->arg3) );
    break;
  case 'G':
    send_to_char(CH, "^G3^Y) ^WGive obj: ^c%s ^y(^B%d^y)^n\r\n",
                 obj_proto[COM->arg1].text.name,
                 OBJ(COM->arg1) );
    send_to_char(CH, "^G4^Y) ^WMaximum number in game: ^c%d^n\r\n",
                 COM->arg2 );
    break;
  case 'E':
    send_to_char(CH, "^G3^Y) ^WEquip obj: ^c%s (%d)^n\r\n",
                 obj_proto[COM->arg1].text.name,
                 OBJ(COM->arg1));
    send_to_char(CH, "^G4^Y) ^WMaximum number in game: ^c%d^n\r\n",
                 COM->arg2 );
    send_to_char(CH, "^G5^Y) ^WAt ^c%s^W position\r\n",
                 short_where[COM->arg3] );
    break;
  case 'N':
    send_to_char(CH, "^G3^Y) ^WGive obj: ^c%s (%d)^n\r\n",
                 obj_proto[COM->arg1].text.name,
                 OBJ(COM->arg1));
    send_to_char(CH, "^G4^Y) ^WMaximum number in game: ^c%d^n\r\n",
                 COM->arg2);
    send_to_char(CH, "^G5^Y) ^WTotal number to give: ^c%d^n\r\n",
                 COM->arg3);
    break;
  case 'R':
    send_to_char(CH, "^G3^Y) ^WRemove obj: ^c%s ^y(^B%d^y)^n\r\n",
                 obj_proto[COM->arg2].text.name,
                 OBJ(COM->arg2) );
    send_to_char(CH, "^G4^Y) ^WFrom room: ^c%s ^y(^B%d^y)^n\r\n",
                 world[COM->arg1].name,
                 ROOM(COM->arg1) );
    break;
  case 'D':
    send_to_char(CH, "^G3^Y) ^WSet door: ^c%s^n\r\n",
                 dirs[COM->arg2] );
    send_to_char(CH, "^G4^Y) ^WIn room: ^c%s ^y(^B%d^y)^n\r\n",
                 world[COM->arg1].name,
                 ROOM(COM->arg1) );
    send_to_char(CH, "^G5^Y) ^WTo state: ^c%s^n\r\n",
                 (COM->arg3 == 0 ? "open"
                  : (COM->arg3 == 1 ? "closed" :
                     "closed and locked")) );
    break;
  case 'C':
    send_to_char(CH, "^G3^Y) ^WObj: ^c%s ^y(^B%d^y)^n\r\n",
                 obj_proto[COM->arg1].text.name, OBJ(COM->arg1) );
    send_to_char(CH, "^G4^Y) ^WTo: ^c%s^n\r\n", (COM->arg2 == 1 ? "bioware" : "cyberware") );
    break;
  }
  send_to_char("^Gq^Y) ^WQuit\r\n^wEnter your selection: ", CH);
  d->edit_mode = ZEDIT_COMMAND_MENU;
}


// MAIN LOOP!
void zedit_parse(struct descriptor_data *d, const char *arg)
{
  int number, i = 0, zone;

  switch (d->edit_mode)
  {
  case ZEDIT_CONFIRM_EDIT_DATA:
    switch (*arg) {
    case 'y':
    case 'Y':
      d->edit_zon = new zone_data;
      // we do need to zero it out since we are accessing its elements
      memset((char *) ZON, 0, sizeof(struct zone_data));

      *d->edit_zon = zone_table[d->edit_number];
      if (zone_table[d->edit_number].name)
        d->edit_zon->name = str_dup(zone_table[d->edit_number].name);
      zedit_disp_data_menu(d);
      break;
    case 'n':
    case 'N':
      STATE(d) = CON_PLAYING;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("That's not a valid choice!\r\n", d->character);
      send_to_char("Do you wish to edit the data?\r\n", d->character);
      break;
    }
    break;
  case ZEDIT_CONFIRM_CREATE_DATA:
    switch (*arg) {
    case 'y':
    case 'Y':
      d->edit_zon = new zone_data;
      // we do need to zero it out since we are accessing its elements
      memset((char *) ZON, 0, sizeof(struct zone_data));

      // set a few vars
      ZON->name = str_dup("an unfinished zone");
      ZON->number = CH->player_specials->saved.zonenum;
      ZON->top = ZON->number * 100 + 99;
      zedit_disp_data_menu(d);
      break;
    case 'n':
    case 'N':
      STATE(d) = CON_PLAYING;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("That's not a valid choice!\r\n", d->character);
      send_to_char("Do you wish to edit the data?\r\n", d->character);
      break;
    }
    break;
  case ZEDIT_CONFIRM_SAVEDATA:
    switch (*arg) {
      int zone_num;
    case 'y':
    case 'Y':
      zone_num = real_zone(ZON->number);
      sprintf(buf,"%s wrote new zcmd %ld in zone %d",
              GET_CHAR_NAME(d->character), d->edit_number, zone_table[zone_num].number);
      mudlog(buf, d->character, LOG_WIZLOG, TRUE);
      // first we insert into memory
      if (zone_num > -1) { // ie, it already exists
        ZON->cmd = zone_table[zone_num].cmd;
        delete [] zone_table[zone_num].name;
        zone_table[zone_num] = *ZON;
      } else { // here we got to add a new spot
        int counter;
        int found = 0;
        struct zone_data *new_z_table;
        // create new table + 2
        new_z_table = new struct zone_data[top_of_zone_table + 2];

        for (counter = 0; counter < top_of_zone_table + 1; counter++) {
          if (!found) {
            if (zone_table[counter].number > CH->player_specials->saved.zonenum) {
              new_z_table[counter] = *(ZON);
              found = 1;
              new_z_table[counter + 1] = zone_table[counter];
            } else
              new_z_table[counter] = zone_table[counter];
          } else
            new_z_table[counter + 1] = zone_table[counter];
        }
        if (!found)
          new_z_table[top_of_zone_table + 1] = *(ZON);
        top_of_zone_table++;
        delete [] zone_table;
        zone_table = new_z_table;
      }
      write_zone_to_disk(ZONENUM);
      write_index_file("zon");
      d->edit_mode = 0;
      delete ZON;
      ZON = NULL;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      STATE(d) = CON_PLAYING;
      send_to_char("Done.\r\n", d->character);

      break; // end of 'y' case in confirm savestring
    case 'n':
    case 'N':
      if (ZON) {
        if (ZON->name)
          delete [] ZON->name;
        delete ZON;
      }
      STATE(d) = CON_PLAYING;
      ZON = NULL;
      d->edit_number = 0;
      PLR_FLAGS(CH).RemoveBit(PLR_EDITING);
      break; // end of 'n' case in confirm savestring
    default:
      send_to_char("Please enter yes or no.\r\n"
                   "Do you wish to save this zone internally?\r\n", CH);
      break;
    }
    break; // end of confirm savestring

  case ZEDIT_CONFIRM_ADD_CMD:
    switch (*arg) {
    case 'y':
    case 'Y':
      COM = new reset_com;
      COM->command = '*';
      zedit_disp_command_menu(d);
      break;
    case 'n':
    case 'N':
      STATE(d) = CON_PLAYING;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("That's not a valid choice!\r\n", CH);
      send_to_char("Do you wish to add a command?\r\n", CH);
      break;
    }
    break;
  case ZEDIT_CONFIRM_INSERT_CMD:
    switch (*arg) {
    case 'y':
    case 'Y':
      COM = new reset_com;
      COM->command = '*';
      // so it knows to insert if they decide to save
      d->edit_number2 = TRUE;
      zedit_disp_command_menu(d);
      break;
    case 'n':
    case 'N':
      STATE(d) = CON_PLAYING;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("That's not a valid choice!\r\n", CH);
      send_to_char("Do you wish to insert a command?\r\n", CH);
      break;
    }
    break;
  case ZEDIT_CONFIRM_EDIT_CMD:
    switch (*arg) {
    case 'y':
    case 'Y':
      COM = new reset_com;
      *COM = zone_table[real_zone(ZONENUM)].cmd[d->edit_number];
      zedit_disp_command_menu(d);
      break;
    case 'n':
    case 'N':
      STATE(d) = CON_PLAYING;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("That's not a valid choice!\r\n", CH);
      send_to_char("Do you wish to edit the command?\r\n", CH);
      break;
    }
    break;
  case ZEDIT_DATA_MENU:
    switch (*arg) {
    case 'q':
    case 'Q':
      send_to_char("Do you wish to save this zone internally?\r\n", CH);
      d->edit_mode = ZEDIT_CONFIRM_SAVEDATA;
      break;
    case '1':
      send_to_char("Enter zone name: ", CH);
      d->edit_mode = ZEDIT_ZONE_NAME;
      break;
    case '2':
      send_to_char("Enter top of zone: ", CH);
      d->edit_mode = ZEDIT_TOP_OF_ZONE;
      break;
    case '3':
      send_to_char("Lifespan (in ticks between resets): ", CH);
      d->edit_mode = ZEDIT_LIFESPAN;
      break;
    case '4':
      CLS(CH);
      send_to_char("1) Don't reset\r\n"
                   "2) Reset only if no PCs are in the zone\r\n"
                   "3) Always reset\r\n"
                   "0) Quit\r\n"
                   "Enter reset mode: ", CH);
      d->edit_mode = ZEDIT_RESET_MODE;
      break;
    case '5':
      send_to_char("Zone security (1 (none) - 15 (paranoid)):\r\n", CH);
      d->edit_mode = ZEDIT_SECURITY;
      break;
    case '6':
      send_to_char("0) Seattle\r\n"
                   "1) Portland\r\n"
                   "2) Carribean\r\n"
                   "3) Ocean\r\n"
                   "Enter juridiction: ", CH);
      d->edit_mode = ZEDIT_JURID;
      break;
    case '7':
      if (!access_level(CH, LVL_VICEPRES)) {
        send_to_char("That's not a valid choice.\r\n", CH);
        return;
      }
      send_to_char("Enter ID list seperated by spaces:\r\n", CH);
      d->edit_mode = ZEDIT_ID_LIST;
      break;
    case '8':
      if (!access_level(CH, LVL_VICEPRES)) {
        send_to_char("That's not a valid choice.\r\n", CH);
        return;
      }
      send_to_char("Zone is connected (1 - yes, 0 - no)? ", CH);
      d->edit_mode = ZEDIT_CONNECTED;
      break;
    default:
      send_to_char("That's not a valid choice.\r\n", CH);
      zedit_disp_data_menu(d);
      break;
    }
    break;

  case ZEDIT_COMMAND_MENU:
    switch (*arg) {
    case 'q':
    case 'Q':
      send_to_char("Do you wish to save this zone command internally?\r\n", CH);
      d->edit_mode = ZEDIT_CONFIRM_SAVECMDS;
      break;
    case '1':
      zedit_disp_type_cmd(d);
      d->edit_mode = ZEDIT_CMD_TYPE;
      break;
    case '2':
      send_to_char("\r\n1) Always\r\n2) If last\r\nEnter your selection: ",
                   CH);
      d->edit_mode = ZEDIT_IF_FLAG_CMD;
      break;
    case '3':
      switch (COM->command) {
      case 'M':
      case 'S':
        send_to_char("\r\nEnter virtual number of mob: ", CH);
        d->edit_mode = ZEDIT_ARG1;
        break;
      case 'H':
      case 'U':
      case 'I':
      case 'O':
      case 'P':
      case 'E':
      case 'G':
      case 'R':
      case 'N':
      case 'C':
        send_to_char("\r\nEnter virtual number of obj: ", CH);
        d->edit_mode = ZEDIT_ARG1;
        break;
      case 'D':
        zedit_disp_direction_menu(d);
        d->edit_mode = ZEDIT_DIRECTION_OF_DOOR;
        break;
      case 'V':
        send_to_char("\r\nEnter virtual number of vehicle: ", CH);
        d->edit_mode = ZEDIT_ARG1;
        break;
      default:
        zedit_disp_command_menu(d);
        break;
      }
      break;
    case '4':
      switch (COM->command) {
      case 'M':
      case 'S':
      case 'U':
      case 'I':
      case 'O':
      case 'P':
      case 'E':
      case 'N':
      case 'G':
      case 'H':
      case 'V':
        send_to_char("Enter max allowed to exist in game: ", CH);
        d->edit_mode = ZEDIT_ARG2;
        break;
      case 'R':
      case 'D':
        send_to_char("Enter the room number: ", CH);
        d->edit_mode = ZEDIT_REMOVE_ROOM;
        break;
      case 'C':
        send_to_char(" 0) Cyberware\r\n 1) Bioware\r\nEnter location to place obj: ", CH);
        d->edit_mode = ZEDIT_ARG2;
        break;
      default:
        zedit_disp_command_menu(d);
        break;
      }
      break;
    case '5':
      switch (COM->command) {
      case 'M':
      case 'O':
      case 'V':
        send_to_char("Enter the room number: ", CH);
        d->edit_mode = ZEDIT_ARG3;
        break;
      case 'H':
        send_to_char("Enter the host number: ", CH);
        d->edit_mode = ZEDIT_ARG3;
        break;
      case 'P':
        send_to_char("Enter the object number: ", CH);
        d->edit_mode = ZEDIT_ARG3;
        break;
      case 'E':
        zedit_disp_wear_menu(d);
        d->edit_mode = ZEDIT_WEAR;
        break;
      case 'D':
        zedit_disp_state_menu(d);
        d->edit_mode = ZEDIT_DOOR_STATE;
        break;
      case 'N':
        send_to_char("Enter total number to give mob: ", CH);
        d->edit_mode = ZEDIT_ARG3;
        break;
      default:
        zedit_disp_command_menu(d);
        break;
      }
      break;
    }
    break;

  case ZEDIT_CONFIRM_SAVECMDS:
    int zone_num, top_of_cmds;
    switch (*arg) {
    case 'y':
    case 'Y':
      zone_num = real_zone(ZONENUM);
      top_of_cmds = zone_table[zone_num].num_cmds;
      sprintf(buf,"%s wrote new zcmd %ld in zone %d",
              GET_CHAR_NAME(d->character), d->edit_number, zone_table[zone_num].number);
      mudlog(buf, d->character, LOG_WIZLOG, TRUE);
      // first, determine if you are adding or replacing
      if (d->edit_number < top_of_cmds) {
        if (!d->edit_number2)
          zone_table[zone_num].cmd[d->edit_number] = *(COM);
        else {
          int counter;
          struct reset_com *new_cmds;
          new_cmds = new struct reset_com[top_of_cmds + 1];
          if (top_of_cmds > 1) {
            // first count your way up
            for (counter = 0; counter < d->edit_number; counter++)
              new_cmds[counter] = zone_table[zone_num].cmd[counter];
            for (counter = top_of_cmds; counter > d->edit_number;
                 counter--)
              new_cmds[counter] = zone_table[zone_num].cmd[counter-1];
            new_cmds[d->edit_number] = *(COM);
            zone_table[zone_num].num_cmds++;
            delete [] zone_table[zone_num].cmd;
            zone_table[zone_num].cmd = new_cmds;
          } else {
            new_cmds[1] = zone_table[zone_num].cmd[0];
            new_cmds[0] = *(COM);
            zone_table[zone_num].num_cmds++;
            if (zone_table[zone_num].cmd)
              delete [] zone_table[zone_num].cmd;
            zone_table[zone_num].cmd = new_cmds;
          }
        }
      } else {
        int counter;
        struct reset_com *new_cmds;
        // create a new set of commands, with 1 extra spot
        new_cmds = new struct reset_com[top_of_cmds + 1];
        // we know it is always going in at the end, so we copy the old 1st
        if (top_of_cmds > 0) { // you have to do this in case there are 0 cmds
          for (counter = 0; counter < top_of_cmds; counter++)
            new_cmds[counter] = zone_table[zone_num].cmd[counter];
          // tada, here it goes now, do not increase counter, for loop
          new_cmds[counter] = *(COM);          // already did!
          zone_table[zone_num].num_cmds++;
          delete [] zone_table[zone_num].cmd;
          zone_table[zone_num].cmd = new_cmds;
        } else {
          new_cmds[0] = *(COM);
          zone_table[zone_num].num_cmds++;
          if (zone_table[zone_num].cmd)
            delete [] zone_table[zone_num].cmd;
          zone_table[zone_num].cmd = new_cmds;
        }
      } // end else
      write_zone_to_disk(ZONENUM);
      d->edit_mode = 0;
      delete COM;
      COM = NULL;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      STATE(d) = CON_PLAYING;
      send_to_char("Done.\r\n", d->character);
      break; // for 'y' case
    case 'n':
    case 'N':
      if (COM)
        delete COM;
      STATE(d) = CON_PLAYING;
      COM = NULL;
      d->edit_number = 0;
      PLR_FLAGS(CH).RemoveBit(PLR_EDITING);
      break; // for 'n' case
    default:
      send_to_char("That's not a valid choice.\r\n", CH);
      send_to_char("Do you wish to save this zone command internally?\r\n", CH);
      break;
    } // for switch in confirm save cmds
    break; // for ZEDIT_CONFIRM_SAVECMDS

  case ZEDIT_ARG3:
    number = atoi(arg);
    switch (COM->command) {
    case 'H':
      COM->arg3 = MAX(0, real_host(number));
      break;
    case 'M':
    case 'V':
    case 'O':
      COM->arg3 = MAX(0, real_room(number));
      if (!access_level(CH, LVL_ADMIN) && !(number >= (ZONENUM * 100) &&
                                            number <= zone_table[real_zone(ZONENUM)].top))
        COM->arg3 = 0;
      break;
    case 'P':
      COM->arg3 = MAX(0, real_object(number));
      if (!access_level(CH, LVL_ADMIN) && (number < 600 || number > 699)) {
        for (zone = 0; zone <= top_of_zone_table; zone++)
          if (number >= (zone_table[zone].number * 100) && number <= zone_table[zone].top)
            break;
        if (zone <= top_of_zone_table) {
          for (i = 0; i < 5; i++)
            if (zone_table[zone].editor_ids[i] == GET_IDNUM(CH))
              break;
        } else
          i = 5;
      }
      if (i >= 5)
        COM->arg3 = 0;
      break;
    case 'N':
      COM->arg3 = MIN(25, MAX(0, number));
      break;
    }
    zedit_disp_command_menu(d);
    break;

  case ZEDIT_DOOR_STATE:
    number = atoi(arg);
    if ((number < 0) || (number > 3)) {
      zedit_disp_state_menu(d);
      return;
    } else
      if (number != 0)
        COM->arg3 = number - 1;
    zedit_disp_command_menu(d);
    break;

  case ZEDIT_WEAR:
    number = atoi(arg);
    if ((number < 0) || (number > (NUM_WEARS - 1))) {
      zedit_disp_wear_menu(d);
      return;
    } else if (number != 0)
      COM->arg3 = number - 1;
    zedit_disp_command_menu(d);
    break;

  case ZEDIT_REMOVE_ROOM:
    number = atoi(arg);
    COM->arg1 = MAX(0, real_room(number));
    if (!access_level(CH, LVL_ADMIN) && !(number >= (ZONENUM * 100) &&
                                          number <= zone_table[real_zone(ZONENUM)].top))
      COM->arg1 = 0;
    zedit_disp_command_menu(d);
    break;

  case ZEDIT_ARG2:
    number = atoi(arg);
    if (COM->command == 'C' && (number < 0 || number > 1)) {
      send_to_char("Value must be either 0 (cyberware) or 1 (bioware).\r\n"
                   "Enter location to place obj: ", CH);
      return;
    } else if ((number < -1) || (number > 1000)) {
      send_to_char("Value must be between -1 and 1000.\r\n"
                   "Enter max allowed to exist in game: ", CH);
      return;
    }
    COM->arg2 = number;
    zedit_disp_command_menu(d);
    break;

  case ZEDIT_DIRECTION_OF_DOOR:
    number = atoi(arg);
    if (number < 0 || number > 10) {
      zedit_disp_direction_menu(d);
      return;
    } else
      if (number != 0)
        COM->arg2 = number - 1;
    zedit_disp_command_menu(d);
    break;

  case ZEDIT_ARG1:
    number = atoi(arg);
    if (COM->command == 'V') {
      COM->arg1 = MAX(0, real_vehicle(number));
    } else {
      if (COM->command == 'M' || COM->command == 'S') {
        COM->arg1 = MAX(0, real_mobile(number));
        if (!access_level(CH, LVL_ADMIN)) {
          for (zone = 0; zone <= top_of_zone_table; zone++)
            if (number >= (zone_table[zone].number * 100) && number <= zone_table[zone].top)
              break;
          if (zone <= top_of_zone_table) {
            for (i = 0; i < 5; i++)
              if (zone_table[zone].editor_ids[i] == GET_IDNUM(CH))
                break;
          } else
            i = 5;
        }
        if (i >= 5)
          COM->arg1 = 0;
      } else {
        if (COM->command == 'R')
          COM->arg2 = MAX(0, real_object(number));
        else
          COM->arg1 = MAX(0, real_object(number));
        if (!access_level(CH, LVL_ADMIN) && (number < 300 || number > 699 || (number > 499 && number < 600))) {
          for (zone = 0; zone <= top_of_zone_table; zone++)
            if (number >= (zone_table[zone].number * 100) && number <= zone_table[zone].top)
              break;
          if (zone <= top_of_zone_table) {
            for (i = 0; i < 5; i++)
              if (zone_table[zone].editor_ids[i] == GET_IDNUM(CH))
                break;
          } else
            i = 5;
        }
        if (i >= 5) {
          if (COM->command == 'R')
            COM->arg2 = 0;
          else
            COM->arg1 = 0;
        }
      }
    }
    zedit_disp_command_menu(d);
    break;

  case ZEDIT_CMD_TYPE:
    number = atoi(arg);
    if ((number < 0 || number > 9) && *arg != 'n') {
      zedit_disp_type_cmd(d);
      send_to_char("\r\nInvalid selection.  Please try again: ", CH);
      return;
    } else {
      COM->command = get_real_type(arg);
      if ((COM->command == 'E') || (COM->command == 'G') ||
          (COM->command == 'P') || (COM->command == 'N') || (COM->command == 'C'))
        COM->if_flag = 1;
      else
        COM->if_flag = 0;
      COM->arg1 = 0;
      COM->arg2 = 0;
      COM->arg3 = 0;
    }

    zedit_disp_command_menu(d);
    break;

  case ZEDIT_IF_FLAG_CMD:
    number = atoi(arg);
    if ((number < 1) || (number > 2)) {
      send_to_char("Invalid selection.\r\n1) Always\r\n2) If last\r\n"
                   "Enter your selection: ", CH);
    } else {
      COM->if_flag = COM->if_flag;
      zedit_disp_command_menu(d);
    }
    break;

  case ZEDIT_ZONE_NAME:
    if (ZON->name)
      delete [] ZON->name;
    ZON->name = str_dup(arg);
    zedit_disp_data_menu(d);
    break;

  case ZEDIT_SECURITY:
    number = atoi(arg);
    if (number < 1 || number > 15) {
      send_to_char("Security rating must range from 1 to 15.\r\nZone security: ", CH);
      return;
    }
    ZON->security = number;
    zedit_disp_data_menu(d);
    break;

  case ZEDIT_ID_LIST: {
      int t[5] = {0, 0, 0, 0, 0};
      if (sscanf(arg, "%d %d %d %d %d\n", &t[0], &t[1], &t[2], &t[3], &t[4]) == 5) {
        ZON->editor_ids[0] = t[0];
        ZON->editor_ids[1] = t[1];
        ZON->editor_ids[2] = t[2];
        ZON->editor_ids[3] = t[3];
        ZON->editor_ids[4] = t[4];
      }
      zedit_disp_data_menu(d);
    }
    break;
  case ZEDIT_CONNECTED:
    number = atoi(arg);
    if (number != 0 && number != 1) {
      send_to_char("Value must be 0 or 1!  Zone is connected? ", CH);
      return;
    }

    sprintf(buf, "%s set zone %d to connected %d (was %d)",
            GET_CHAR_NAME( CH ), ZON->number, number, ZON->connected );
    mudlog(buf, CH, LOG_WIZLOG, TRUE);

    ZON->connected = number;
    zedit_disp_data_menu(d);
    break;
  case ZEDIT_TOP_OF_ZONE:
    number = atoi(arg);
    if ((d->edit_number == top_of_zone_table) || (d->edit_number == -1))
      if ((number < ZON->number * 100) || (number > (ZON->number * 100 + 499))) {
        send_to_char(CH, "Value must range from %d to %d\r\n", ZON->number * 100,
                     (ZON->number * 100 + 499));
        send_to_char("Enter top of zone: ", CH);
        return;
      } else
        ZON->top = number;
    else
      if ((number < ZON->number * 100) ||
          (number > zone_table[d->edit_number + 1].number * 100 - 1)) {
        send_to_char(CH, "Value must range from %d to %d\r\n", ZON->number * 100,
                     zone_table[d->edit_number + 1].number * 100 - 1);
        send_to_char("Enter top of zone: ", CH);
        return;
      } else
        ZON->top = number;
    zedit_disp_data_menu(d);
    break;

  case ZEDIT_LIFESPAN:
    number = atoi(arg);
    if ((number < 0) || (number > 1440)) {
      send_to_char("Value must range from 0 to 1440\r\n", CH);
      send_to_char("Lifespan (in ticks between resets): ", CH);
    } else {
      ZON->lifespan = number;
      zedit_disp_data_menu(d);
    }
    break;

  case ZEDIT_RESET_MODE:
    number = atoi(arg);
    if ((number < 0) || (number > 3)) {
      send_to_char("Invalid choice.  Please enter from 0 to 3.\r\n", CH);
      send_to_char("Enter reset mode: ", CH);
      return;
    } else
      if (number != 0)
        ZON->reset_mode = number - 1;
    zedit_disp_data_menu(d);
    break;
  case ZEDIT_JURID:
    number = atoi(arg);
    if (number < 0 || number > 3) {
      send_to_char("Invalid choice.  Please enter from 0 to 1.\r\n", CH);
      send_to_char("Enter Juridiction: ", CH);
      return;
    } else
      ZON->juridiction = number;
    zedit_disp_data_menu(d);
    break;
  }
}
