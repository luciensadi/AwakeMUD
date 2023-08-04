/*
 * file: vedit.cc
 * author: Dunkelzahn
 * (c)2001 The AwakeMUD Consortium, Daniel Gelinske
 * purpose: vedit, vehicle editor addon
 * to AwakeOLC, a component of AwakeMUD
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.hpp"
#include "awake.hpp"
#include "constants.hpp"
#include "interpreter.hpp"
#include "comm.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "dblist.hpp"
#include "screen.hpp"
#include "olc.hpp"
#include "memory.hpp"

#define VEH     d->edit_veh

extern class memoryClass *Mem;

void write_vehs_to_disk(int zone);

// extern funcs
extern char *cleanup(char *dest, const char *src);

/* display main menu */
void vedit_disp_menu(struct descriptor_data * d)
{
  CLS(CH);
  send_to_char(CH, "Vehicle number: %s%d%s\r\n", CCCYN(CH, C_CMP), d->edit_number,
               CCNRM(CH, C_CMP));
  send_to_char(CH, "1) Vehicle namelist: %s%s%s\r\n", CCCYN(CH, C_CMP),
               d->edit_veh->name, CCNRM(CH, C_CMP));
  send_to_char(CH, "2) Vehicle shortdesc: %s%s%s\r\n", CCCYN(CH, C_CMP),
               d->edit_veh->short_description, CCNRM(CH, C_CMP));
  send_to_char(CH, "3) Vehicle descript (Stationary): %s%s%s\r\n", CCCYN(CH, C_CMP),
               d->edit_veh->description, CCNRM(CH, C_CMP));
  send_to_char(CH, "4) Vehicle longdesc: \r\n%s\r\n",
               d->edit_veh->long_description ? d->edit_veh->long_description :
               "(not set)");
  send_to_char(CH, "5) Vehicle Inside Descript: \r\n%s\r\n",
               d->edit_veh->inside_description ? d->edit_veh->inside_description : "(not set)");
  send_to_char(CH, "6) Vehicle Inside Rear Descript: \r\n%s\r\n",
               d->edit_veh->rear_description ? d->edit_veh->rear_description : "(not set)");
  send_to_char(CH, "7) Arrive text: ^c%s^n,  8) Leave text: ^c%s^n\r\n",
               d->edit_veh->arrive, d->edit_veh->leave);
  d->edit_veh->flags.PrintBits(buf1, MAX_STRING_LENGTH, veh_flag, NUM_VFLAGS);
  send_to_char(CH, "9) Flags: ^c%s^n\r\n", buf1);
  send_to_char(CH, "a) H(^c%d^n) Sp(^c%d^n) Acc(^c%d^n) B(^c%d^n) Ar(^c%d^n)\r\n",
               d->edit_veh->handling, d->edit_veh->speed, d->edit_veh->accel, d->edit_veh->body,
               d->edit_veh->armor);
  send_to_char(CH, "   Sig(^c%d^n) Au(^c%d^n) P(^c%d^n) Seat(^c%d/%d^n) Load(^c%d^n) Cost(^c%d^n)\r\n",
               d->edit_veh->sig, d->edit_veh->autonav, d->edit_veh->pilot, d->edit_veh->seating[SEATING_FRONT], d->edit_veh->seating[SEATING_REAR],
               (int)d->edit_veh->load, d->edit_veh->cost);
  send_to_char(CH, "b) Type: ^C%s^n\r\n", veh_types[d->edit_veh->type]);
  send_to_char(CH, "c) Stock Engine: ^C%s^n\r\n", engine_types[d->edit_veh->engine]);
  send_to_char("q) Quit and save\r\n"
               "x) Exit and abort\r\n"
               "Enter your choice:\r\n", CH);
  d->edit_mode = VEDIT_MAIN_MENU;
}

/***************************************************************************
 main loop (of sorts).. basically interpreter throws all input to here
 ***************************************************************************/

void vedit_disp_att(struct descriptor_data * d)
{
  CLS(CH);
  send_to_char(CH, "1) Handling:  ^c%d^n\r\n", d->edit_veh->handling);
  send_to_char(CH, "2) Speed:  ^c%d^n\r\n", d->edit_veh->speed);
  send_to_char(CH, "3) Acceleration: ^c%d^n\r\n", d->edit_veh->accel);
  send_to_char(CH, "4) Body:  ^c%d^n\r\n", d->edit_veh->body);
  send_to_char(CH, "5) Armor:  ^c%d^n\r\n", d->edit_veh->armor);
  send_to_char(CH, "6) Signature:  ^c%d^n\r\n", d->edit_veh->sig);
  send_to_char(CH, "7) AutoNav:  ^c%d^n\r\n", d->edit_veh->autonav);
  send_to_char(CH, "8) Pilot:  ^c%d^n\r\n", d->edit_veh->pilot);
  send_to_char(CH, "9) Seating:  ^c%d/%d^n\r\n", d->edit_veh->seating[SEATING_FRONT], d->edit_veh->seating[SEATING_REAR]);
  send_to_char(CH, "0) Load:  ^c%d^n\r\n", (int)d->edit_veh->load);
  send_to_char(CH, "a) Cost:  ^c%d^n\r\n", d->edit_veh->cost);
  send_to_char(CH, "q) Quit\r\n");
  send_to_char(CH, "\r\nEnter your choice:\r\n");
  d->edit_mode = VEDIT_ATT;
}

void vedit_disp_type(struct descriptor_data * d)
{
  CLS(CH);
  send_to_char(CH, "0) Drone\r\n");
  send_to_char(CH, "1) Car\r\n");
  send_to_char(CH, "2) Bike\r\n");
  send_to_char(CH, "3) Truck\r\n");
  send_to_char(CH, "4) Fixed Wing\r\n");
  send_to_char(CH, "5) Rotorcraft\r\n");
  send_to_char(CH, "6) Vector Thrust\r\n");
  send_to_char(CH, "7) Hovercraft\r\n");
  send_to_char(CH, "8) MotorBoat\r\n");
  send_to_char(CH, "9) Ship\r\n");
  send_to_char(CH, "a) Lighter Than Air\r\n");
  send_to_char(CH, "Enter your choice:\r\n");
  d->edit_mode = VEDIT_TYPE;
}

void vedit_disp_flag_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int x = 1; x < NUM_VFLAGS; x++)
    send_to_char(CH, "%d) %s\r\n", x, veh_flag[x]);
  d->edit_veh->flags.PrintBits(buf1, MAX_STRING_LENGTH,
                               veh_flag, NUM_VFLAGS);
  send_to_char(CH, "Vehicle Flags: ^c%s^n\r\nSelect Flag (q to quit): ", buf1);
  d->edit_mode = VEDIT_FLAGS;
}


void vedit_parse(struct descriptor_data * d, const char *arg)
{
  int             number;
  int             veh_number;   /* the RNUM */
  switch (d->edit_mode)
  {

  case VEDIT_CONFIRM_EDIT:
    /* if player hits 'Y' then edit veh */
    switch (*arg) {
    case 'y':
    case 'Y':
      vedit_disp_menu(d);
      break;
    case 'n':
    case 'N':
      STATE(d) = CON_PLAYING;
      /* free up the editing vehicle */
      if (d->edit_veh)
        Mem->DeleteVehicle(d->edit_veh);
      d->edit_veh = NULL;
      d->edit_number = 0;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("That's not a valid choice!\r\n", d->character);
      send_to_char("Do you wish to edit it?\r\n", d->character);
      break;
    }
    break;                      /* end of VEDIT_CONFIRM_EDIT */

  case VEDIT_CONFIRM_SAVESTRING:
    switch (*arg) {
    case 'y':
    case 'Y': {
#ifdef ONLY_LOG_BUILD_ACTIONS_ON_CONNECTED_ZONES
        if (!vnum_from_non_connected_zone(d->edit_number)) {
#else
        {
#endif
          snprintf(buf, sizeof(buf),"%s wrote new vehicle #%ld", GET_CHAR_NAME(d->character), d->edit_number);
          mudlog(buf, d->character, LOG_WIZLOG, TRUE);
        }
        /* write to internal tables */
        veh_number = real_vehicle(d->edit_number);
        if (veh_number > 0) {
          /* we need to run through each and every vehicle currently in the
           * game to see which ones are pointing to this prototype */
          struct veh_data *temp;
          /* if vehicle is pointing to this prototype, then we need to replace
           * with the new one */
          for (struct veh_data *i = veh_list; i; i = i->next) {
            if (veh_number == i->veh_number) {
              temp = Mem->GetVehicle();
              *temp = *i;
              *i = *d->edit_veh;
              /* copy game-time dependent vars over */
              i->in_room = temp->in_room;
              i->veh_number = veh_number;
              i->next_veh = temp->next_veh;
              i->contents = temp->contents;
              i->people = temp->people;
              i->damage = temp->damage;
              i->cspeed = temp->cspeed;
              i->seating[SEATING_FRONT] = temp->seating[SEATING_FRONT];
              i->seating[SEATING_REAR] = temp->seating[SEATING_REAR];
              i->next = temp->next;
              for (int c = 0; c < NUM_MODS; c++)
                i->mod[c] = temp->mod[c];
              i->owner = temp->owner;
              i->mount = temp->mount;

              clear_vehicle(temp);
              delete temp;
            }
          }
          // this function updates pointers to the active list of vehicles
          // in the mud
          /* now safe to free old proto and write over */

#define SAFE_VEH_ARRAY_DELETE(ITEM)                                                                        \
if (d->edit_veh->ITEM && veh_proto[veh_number].ITEM && d->edit_veh->ITEM != veh_proto[veh_number].ITEM) {  \
  delete [] veh_proto[veh_number].ITEM;                                                                    \
  veh_proto[veh_number].ITEM = NULL;                                                                       \
}

          SAFE_VEH_ARRAY_DELETE(name);
          SAFE_VEH_ARRAY_DELETE(description);
          SAFE_VEH_ARRAY_DELETE(short_description);
          SAFE_VEH_ARRAY_DELETE(long_description);
          SAFE_VEH_ARRAY_DELETE(inside_description);
          SAFE_VEH_ARRAY_DELETE(rear_description);
          SAFE_VEH_ARRAY_DELETE(arrive);
          SAFE_VEH_ARRAY_DELETE(leave);

#undef SAFE_VEH_ARRAY_DELETE

          veh_proto[veh_number] = *d->edit_veh;

        } else {
          /* uhoh.. need to make a new place in the vehicle prototype table */
          int             found = FALSE;

          struct veh_data *new_veh_proto;
          struct index_data *new_veh_index;
          struct veh_data *temp_veh;

          /* + 2.. strange but true */
          new_veh_index = new struct index_data[top_of_veht + 2];
          new_veh_proto = new struct veh_data[top_of_veht + 2];
          /* start counting through both tables */
          int counter = 0;
          for (counter = 0; counter <= top_of_veht; counter++) {
            /* if we haven't found it */
            if (!found) {
              /* check if current virtual is bigger than our virtual */
              if (veh_index[counter].vnum > d->edit_number) {
                /* eureka. insert now */
                /*---------*/
                new_veh_index[counter].vnum = d->edit_number;
                new_veh_index[counter].number = 0;
                new_veh_index[counter].func = NULL;
                /*---------*/
                new_veh_proto[counter] = *(d->edit_veh);
                new_veh_proto[counter].in_room = NULL;
                /* it is now safe (and necessary!) to assign real number to
                 * the edit_veh, which has been -1 all this time */
                d->edit_veh->veh_number = counter;
                /* and assign to prototype as well */
                new_veh_proto[counter].veh_number = counter;
                found = TRUE;
                /* insert the other proto at this point */
                new_veh_index[counter + 1] = veh_index[counter];
                new_veh_proto[counter + 1] = veh_proto[counter];
                new_veh_proto[counter + 1].veh_number = counter + 1;
              } else {
                /* just copy from old to new, no num change */
                new_veh_proto[counter] = veh_proto[counter];
                new_veh_index[counter] = veh_index[counter];
              }
            } else {
              /* we HAVE already found it.. therefore copy to vehicle + 1 */
              new_veh_index[counter + 1] = veh_index[counter];
              new_veh_proto[counter + 1] = veh_proto[counter];
              new_veh_proto[counter + 1].veh_number = counter + 1;
            }
          }
          /* if we STILL haven't found it, means the vehicle was > than all
           * the other vehicles.. so insert at end */
          if (!found) {
            new_veh_index[top_of_veht + 1].vnum = d->edit_number;
            new_veh_index[top_of_veht + 1].number = 0;
            new_veh_index[top_of_veht + 1].func = NULL;

            clear_vehicle(new_veh_proto + top_of_veht + 1);
            new_veh_proto[top_of_veht + 1] = *(d->edit_veh);
            new_veh_proto[top_of_veht + 1].in_room = NULL;
            new_veh_proto[top_of_veht + 1].veh_number = top_of_veht + 1;
            /* it is now safe (and necessary!) to assign real number to
             * the edit_veh, which has been -1 all this time */
            d->edit_veh->veh_number = top_of_veht + 1;
          }
          top_of_veht++;


          /* we also have to renumber all the vehicles currently
             existing in the world. This is because when I start
             extracting vehicles, bad things will happen! */
          for (temp_veh = veh_list; temp_veh; temp_veh = temp_veh->next)
            if (temp_veh->veh_number >= d->edit_veh->veh_number)
              temp_veh->veh_number++;

          // Update all vehicle zone loads too.
          {
            #define UPDATE_VALUE(value) {(value) = ((value) >= d->edit_veh->veh_number ? (value) + 1 : (value));}
            for (int zone = 0; zone <= top_of_zone_table; zone++) {
              for (int cmd_no = 0; cmd_no < zone_table[zone].num_cmds; cmd_no++) {
                switch (ZCMD.command) {
                  case 'V':
                    UPDATE_VALUE(ZCMD.arg1);
                    break;
                }
              }
            }
            #undef UPDATE_VALUE
          }

          /* free and replace old tables */
          delete [] veh_proto;
          delete [] veh_index;
          veh_proto = new_veh_proto;
          veh_index = new_veh_index;
        }

        send_to_char("Writing vehicle to disk..", d->character);
        write_vehs_to_disk(d->character->player_specials->saved.zonenum);
        // don't wanna nuke the strings, so we use ClearVehicle
        if (d->edit_veh) {
          clear_vehicle(d->edit_veh);
          delete d->edit_veh;
        }
        d->edit_veh = NULL;
        STATE(d) = CON_PLAYING;
        PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
        send_to_char("Done.\r\n", d->character);
    }
      break;
    case 'n':
    case 'N':
      send_to_char("Vehicle not saved, aborting.\r\n", d->character);
      STATE(d) = CON_PLAYING;
      /* free up the editing vehicle. free_veh *is* safe since
         it checks against prototype table */
      if (d->edit_veh)
        Mem->DeleteVehicle(d->edit_veh);
      d->edit_veh = NULL;
      d->edit_number = 0;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char("Do you wish to save this vehicle internally?\r\n", d->character);
      break;
    }
    break;                      /* end of VEDIT_CONFIRM_SAVESTRING */

  case VEDIT_MAIN_MENU:
    /* throw us out to whichever edit mode based on user input */
    switch (*arg) {
    case 'q':
    case 'Q':
      d->edit_mode = VEDIT_CONFIRM_SAVESTRING;
      vedit_parse(d, "y");
      break;
    case 'x':
    case 'X':
      d->edit_mode = VEDIT_CONFIRM_SAVESTRING;
      vedit_parse(d, "n");
      break;
    case '1':
      send_to_char("Enter namelist:", d->character);
      d->edit_mode = VEDIT_EDIT_NAMELIST;
      break;
    case '2':
      send_to_char("Enter short desc:", d->character);
      d->edit_mode = VEDIT_SHORTDESC;
      break;
    case '3':
      send_to_char("Enter normal desc (Stationary):\r\n", d->character);
      d->edit_mode = VEDIT_DESC;
      break;
    case '4':
      /* let's go out to modify.c */
      send_to_char("Enter long desc:\r\n", d->character);
      d->edit_mode = VEDIT_LONGDESC;
      DELETE_D_STR_IF_EXTANT(d);
      INITIALIZE_NEW_D_STR(d);
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      break;
    case '5':
      send_to_char("Enter inside desc:\r\n", d->character);
      d->edit_mode = VEDIT_INSDESC;
      DELETE_D_STR_IF_EXTANT(d);
      INITIALIZE_NEW_D_STR(d);
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      break;
    case '6':
      send_to_char("Enter inside rear desc:\r\n", d->character);
      d->edit_mode = VEDIT_REARDESC;
      DELETE_D_STR_IF_EXTANT(d);
      INITIALIZE_NEW_D_STR(d);
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      break;
    case '7':
      send_to_char("Enter arrive text:", d->character);
      d->edit_mode = VEDIT_ARRIVE;
      break;
    case '8':
      send_to_char("Enter leave text:", d->character);
      d->edit_mode = VEDIT_LEAVE;
      break;
    case '9':
      vedit_disp_flag_menu(d);
      break;
    case 'a':
      vedit_disp_att(d);
      d->edit_mode = VEDIT_ATT;
      break;
    case 'b':
      vedit_disp_type(d);
      d->edit_mode = VEDIT_TYPE;
      break;
    case 'c':
      for (number = 1; number < NUM_ENGINE_TYPES; number ++)
        send_to_char(CH, "  %d) %s\r\n", number, engine_types[number]);
      send_to_char("Enter default engine type: ", CH);
      d->edit_mode = VEDIT_ENGINE;
      break;
    default:
      send_to_char("That's not a valid choice!\r\n", d->character);
      vedit_disp_menu(d);
      break;
    }
    break;                      /* end of IEDIT_MAIN_MENU */
  case VEDIT_TYPE:
    switch(*arg) {
    case '1':
      d->edit_veh->type = VEH_CAR;
      vedit_disp_menu(d);
      break;
    case '2':
      d->edit_veh->type = VEH_BIKE;
      vedit_disp_menu(d);
      break;
    case '3':
      d->edit_veh->type = VEH_TRUCK;
      vedit_disp_menu(d);
      break;
    case '4':
      d->edit_veh->type = VEH_FIXEDWING;
      vedit_disp_menu(d);
      break;
    case '5':
      d->edit_veh->type = VEH_ROTORCRAFT;
      vedit_disp_menu(d);
      break;
    case '6':
      d->edit_veh->type = VEH_VECTORTHRUST;
      vedit_disp_menu(d);
      break;
    case '7':
      d->edit_veh->type = VEH_HOVERCRAFT;
      vedit_disp_menu(d);
      break;
    case '8':
      d->edit_veh->type = VEH_MOTORBOAT;
      vedit_disp_menu(d);
      break;
    case '9':
      d->edit_veh->type = VEH_SHIP;
      vedit_disp_menu(d);
      break;
    case 'a':
      d->edit_veh->type = VEH_LTA;
      vedit_disp_menu(d);
      break;
    case '0':
    default:
      d->edit_veh->type = VEH_DRONE;
      vedit_disp_menu(d);
      break;
    }
    break;
  case VEDIT_ATT:
    switch(*arg) {
    case '1':
      send_to_char(CH, "Enter Handling Attribute: ");
      d->edit_mode = VEDIT_HAND;
      break;
    case '2':
      send_to_char(CH, "Enter Speed Attribute: ");
      d->edit_mode = VEDIT_SP;
      break;
    case '3':
      send_to_char(CH, "Enter Acceleration Attribute: ");
      d->edit_mode = VEDIT_ACC;
      break;
    case '4':
      send_to_char(CH, "Enter Body Attribute: ");
      d->edit_mode = VEDIT_BOD;
      break;
    case '5':
      send_to_char(CH, "Enter Armor Attribute: ");
      d->edit_mode = VEDIT_ARMOR;
      break;
    case '6':
      send_to_char(CH, "Enter Signature Attribute: ");
      d->edit_mode = VEDIT_SIG;
      break;
    case '7':
      send_to_char(CH, "Enter Autonav Attribute: ");
      d->edit_mode = VEDIT_AUTO;
      break;
    case '8':
      send_to_char(CH, "Enter Pilot Attribute: ");
      d->edit_mode = VEDIT_PILOT;
      break;
    case '9':
      send_to_char(CH, "Enter Seating (Front) Attribute: ");
      d->edit_mode = VEDIT_SEAT;
      break;
    case '0':
      send_to_char(CH, "Enter Load Attribute: ");
      d->edit_mode = VEDIT_LOAD;
      break;
    case 'a':
      send_to_char(CH, "Enter Cost: ");
      d->edit_mode = VEDIT_COST;
      break;
    case 'q':
    case 'Q':
      vedit_disp_menu(d);
      break;
    default:
      vedit_disp_att(d);
      break;
    }
    break;
  case VEDIT_HAND:
    number = atoi(arg);
    if ((number < 0) || (number > 50)) {
      send_to_char("Value must range between 0 and 50.\r\n", CH);
      send_to_char("Enter Handling attribute: ", CH);
    } else {
      d->edit_veh->handling = number;
      vedit_disp_att(d);
    }
    break;
  case VEDIT_SP:
    number = atoi(arg);
    if ((number < 0) || (number > 1500)) {
      send_to_char("Value must range between 0 and 1500.\r\n", CH);
      send_to_char("Enter Speed attribute: ", CH);
    } else {
      d->edit_veh->speed = number;
      vedit_disp_att(d);
    }
    break;
  case VEDIT_ACC:
    number = atoi(arg);
    if ((number < 0) || (number > 50)) {
      send_to_char("Value must range between 0 and 50.\r\n", CH);
      send_to_char("Enter Acceleration attribute: ", CH);
    } else {
      d->edit_veh->accel = number;
      vedit_disp_att(d);
    }
    break;
  case VEDIT_BOD:
    number = atoi(arg);
    if ((number < 0) || (number > 50)) {
      send_to_char("Value must range between 0 and 50.\r\n", CH);
      send_to_char("Enter Body attribute: ", CH);
    } else {
      d->edit_veh->body = number;
      vedit_disp_att(d);
    }
    break;
  case VEDIT_ARMOR:
    number = atoi(arg);
    if ((number < 0) || (number > 50)) {
      send_to_char("Value must range between 0 and 50.\r\n", CH);
      send_to_char("Enter Armor attribute: ", CH);
    } else {
      d->edit_veh->armor = number;
      vedit_disp_att(d);
    }
    break;
  case VEDIT_SIG:
    number = atoi(arg);
    if ((number < 0) || (number > 50)) {
      send_to_char("Value must range between 0 and 50.\r\n", CH);
      send_to_char("Enter Signature attribute: ", CH);
    } else {
      d->edit_veh->sig = number;
      vedit_disp_att(d);
    }
    break;
  case VEDIT_AUTO:
    number = atoi(arg);
    if ((number < 0) || (number > 50)) {
      send_to_char("Value must range between 0 and 50.\r\n", CH);
      send_to_char("Enter AutoNav attribute: ", CH);
    } else {
      d->edit_veh->autonav = number;
      vedit_disp_att(d);
    }
    break;
  case VEDIT_PILOT:
    number = atoi(arg);
    if ((number < 0) || (number > 50)) {
      send_to_char("Value must range between 0 and 50.\r\n", CH);
      send_to_char("Enter Pilot attribute: ", CH);
    } else {
      d->edit_veh->pilot = number;
      vedit_disp_att(d);
    }
    break;
  case VEDIT_LOAD:
    number = atoi(arg);
    if ((number < 1) || (number > 15000)) {
      send_to_char("Value must range between 1 and 15000.\r\n", CH);
      send_to_char("Enter Load attribute: ", CH);
    } else {
      d->edit_veh->load = number;
      vedit_disp_att(d);
    }
    break;
  case VEDIT_SEAT:
    number = atoi(arg);
    if ((number < 0) || (number > 50)) {
      send_to_char("Value must range between 0 and 50.\r\n", CH);
      send_to_char("Enter Seating (Front) attribute: ", CH);
    } else {
      d->edit_veh->seating[SEATING_FRONT] = number;
      send_to_char("Enter Seating (Rear) attribute: ", CH);
      d->edit_mode = VEDIT_SEAT2;
    }
    break;
  case VEDIT_SEAT2:
    number = atoi(arg);
    if ((number < 0) || (number > 50)) {
      send_to_char("Value must range between 0 and 50.\r\n", CH);
      send_to_char("Enter Seating (Rear) attribute: ", CH);
    } else {
      d->edit_veh->seating[SEATING_REAR] = number;
      vedit_disp_att(d);
    }
    break;
  case VEDIT_COST:
    number = atoi(arg);
    d->edit_veh->cost = number;
    vedit_disp_att(d);
    break;

  case VEDIT_EDIT_NAMELIST:
    if (d->edit_veh->name)
      delete [] d->edit_veh->name;
    d->edit_veh->name = str_dup(arg);
    vedit_disp_menu(d);
    break;
  case VEDIT_SHORTDESC:
    if (d->edit_veh->short_description)
      delete [] d->edit_veh->short_description;
    d->edit_veh->short_description = str_dup(arg);
    vedit_disp_menu(d);
    break;
  case VEDIT_DESC:
    if (d->edit_veh->description)
      delete [] d->edit_veh->description;
    d->edit_veh->description = str_dup(arg);
    vedit_disp_menu(d);
    break;
  case VEDIT_ARRIVE:
    if (d->edit_veh->arrive)
      delete [] d->edit_veh->arrive;
    d->edit_veh->arrive = str_dup(arg);
    vedit_disp_menu(d);
    break;
  case VEDIT_LEAVE:
    if (d->edit_veh->leave)
      delete [] d->edit_veh->leave;
    d->edit_veh->leave = str_dup(arg);
    vedit_disp_menu(d);
    break;
  case VEDIT_FLAGS:
    number = atoi(arg);
    if (number < 0 || number > NUM_VFLAGS)
      send_to_char(CH, "Invalid choice. Choose Flag (0 to quit):");
    else if (number == 0)
      vedit_disp_menu(d);
    else {
      d->edit_veh->flags.ToggleBit(number);
      vedit_disp_flag_menu(d);
    }
    break;
  case VEDIT_ENGINE:
    number = atoi(arg);
    if (number < 1 || number >= NUM_ENGINE_TYPES)
      send_to_char("Invalid Engine Type. Select Type : ", CH);
    else {
      d->edit_veh->engine = number;
      vedit_disp_menu(d);
    }
  default:
    break;
  }
}
#undef VEH

void write_vehs_to_disk(int zone)
{
  long counter, realcounter;
  FILE *fp;
  struct veh_data *veh;

  zone = real_zone(zone);

  veh = Mem->GetVehicle();

  snprintf(buf, sizeof(buf), "%s/%d.veh", VEH_PREFIX, zone_table[zone].number);
  fp = fopen(buf, "w+");

  /* start running through all vehicles in this zone */
  for (counter = zone_table[zone].number * 100; counter <= zone_table[zone].top; counter++) {
    /* write vehicle to disk */
    realcounter = real_vehicle(counter);
    if (realcounter >= 0) {
      *veh = veh_proto[realcounter];
      if (!strcmp("an unfinished object", veh->name))
        continue;
      fprintf(fp, "#%ld\n", veh_index[veh->veh_number].vnum);
      fprintf(fp, "Name:\t%s\n", veh->name ? veh->name : "unnamed");
      fprintf(fp, "ShortDesc:\t%s\n", veh->short_description ? veh->short_description : "an unnamed vehicle");
      fprintf(fp, "RoomDesc:\t%s\n", veh->description ? veh->description : "An unnamed vehicle sits here");
      fprintf(fp, "LongDesc:$\n%s~\n", cleanup(buf2, veh->long_description ? veh->long_description : "You see an uncreative vehicle."));
      fprintf(fp, "Inside:$\n%s~\n", cleanup(buf2, veh->inside_description ? veh->inside_description: "You see an uncreative vehicle."));
      fprintf(fp, "InsideRear:$\n%s~\n", cleanup(buf2, veh->rear_description ? veh->rear_description: "You see an uncreative vehicle."));
      fprintf(fp, "Leaving:\t%s\n", cleanup(buf2, veh->leave ? veh->leave : "leaves"));
      fprintf(fp, "Arriving:\t%s\n", cleanup(buf2, veh->arrive ? veh->arrive : "arrives"));
      fprintf(fp, "Handling:\t%d\n"
              "Speed:\t%d\n"
              "Accel:\t%d\n"
              "Body:\t%d\n"
              "Armour:\t%d\n"
              "Pilot:\t%d\n",
              veh->handling, veh->speed, veh->accel, veh->body, veh->armor, veh->pilot);
      fprintf(fp, "Sig:\t%d\n"
              "Autonav:\t%d\n"
              "Seating:\t%d\n"
              "SeatingBack:\t%d\n"
              "Load:\t%d\n"
              "Cost:\t%d\n"
              "Type:\t%d\n"
              "Flags:\t%s\n"
              "Engine:\t%d\n",
              veh->sig, veh->autonav, veh->seating[SEATING_FRONT], veh->seating[SEATING_REAR], (int)veh->load, veh->cost, veh->type,
              veh->flags.ToString(), veh->engine);
      fprintf(fp, "BREAK\n");
    }
  }
  /* write final line, close */
  fprintf(fp, "END\n");
  fclose(fp);
  /* nuke temp vehicle, but not the strings */
  clear_vehicle(veh);
  delete veh;
  write_index_file("veh");
}
