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
#include "screen.h"
#include "olc.h"
#include "newmatrix.h"
#include "memory.h"

#define IC d->edit_icon
#define NUM_OF_IC_TYPES 13
extern char *cleanup(char *dest, const char *src);

const char *crippler[4] =
  {
    "Bod",
    "Evasion",
    "Sensors",
    "Masking"
  };

const char *tarbaby[4] =
  {
    "Operational",
    "Offensive",
    "Defensive",
    "Special"
  };

#define ICON ic_proto[realcounter]
void write_ic_to_disk(int vnum)
{
  long realcounter, counter;
  int zone = real_zone(vnum);
  FILE *fl;

  sprintf(buf, "%s/%d.ic", MTX_PREFIX, vnum);
  fl = fopen(buf, "w+");
  for (counter = zone_table[zone].number * 100;
       counter <= zone_table[zone].top;
       counter++) {
    realcounter = real_ic(counter);
    if (realcounter >= 0) {
      if (!strcmp("An unfinished IC", ICON.name))
        continue;
      fprintf(fl, "#%ld\n", ic_index[ICON.number].vnum);
      fprintf(fl, "Name:\t%s\n", ICON.name);
      fprintf(fl, "LongDesc:\t%s\n", ICON.look_desc);
      fprintf(fl, "Description:$\n%s~\n", cleanup(buf2, ICON.long_desc));
      fprintf(fl, "Rating:\t%d\n", ICON.ic.rating);
      fprintf(fl, "Type:\t%s\n", ic_type[ICON.ic.type]);
      fprintf(fl, "Subtype:\t%d\n", ICON.ic.subtype);
      fprintf(fl, "Flags:\t%s\n", ICON.ic.options.ToString());
      if (real_ic(ICON.ic.trap) > 0 && ICON.ic.options.IsSet(IC_TRAP))
        fprintf(fl, "Trap:\t%ld\n", ICON.ic.trap);
      if (ICON.ic.options.AreAnySet(IC_EX_OFFENSE, IC_EX_DEFENSE, ENDBIT))
        fprintf(fl, "Expert:\t%d\n", ICON.ic.expert);
      fprintf(fl, "BREAK\n");
    }
  }

  fprintf(fl, "END\n");
  fclose(fl);
  write_index_file("ic");
}
void icedit_disp_option_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int x = 1; x <= IC_TRAP; x += 2)
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n",
                 x, ic_option[x], x + 1, x + 1 <= IC_TRAP ? ic_option[x + 1] : "");
  IC->ic.options.PrintBits(buf1, MAX_STRING_LENGTH, ic_option, IC_TRAP + 1);
  send_to_char(CH, "IC Options: ^c%s^n\r\nEnter IC Option, 0 to quit: ", buf1);
  d->edit_mode = ICEDIT_OPTION_MENU;
}

void icedit_disp_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "^WIC: ^c%d^n\r\n", d->edit_number);
  send_to_char(CH, "^G1^Y) ^WName: ^c%s^n\r\n", IC->name);
  send_to_char(CH, "^G2^Y) ^WRoomDesc: ^c%s^n\r\n", IC->look_desc);
  send_to_char(CH, "^G3^Y) ^WDescription: ^c%s^n\r\n", IC->long_desc);
  send_to_char(CH, "^G4^Y) ^WType: ^c%s-%d^n\r\n", ic_type[IC->ic.type], IC->ic.rating);
  switch(IC->ic.type)
  {
  case 4:
  case 10:
    send_to_char(CH, "^G5^Y) ^WSubType: ^c%s^n\r\n", tarbaby[IC->ic.subtype]);
    break;
  case 0:
  case 8:
    send_to_char(CH, "^G5^Y) ^WSubType: ^c%s^n\r\n", crippler[IC->ic.subtype]);
    break;
  }
  IC->ic.options.PrintBits(buf1, MAX_STRING_LENGTH, ic_option, IC_TRAP + 1);
  send_to_char(CH, "^G6^Y) ^WOptions: ^c%s^n\r\n", buf1);
  if (IC->ic.options.IsSet(IC_TRAP))
    send_to_char(CH, "^G7^Y) ^WTrap IC: ^c%s (%ld)^n\r\n", real_ic(IC->ic.trap) > 0 ? ic_proto[real_ic(IC->ic.trap)].name : "Nothing", IC->ic.trap);
  if (IC->ic.options.AreAnySet(IC_EX_OFFENSE, IC_EX_DEFENSE, ENDBIT))
    send_to_char(CH, "^G8^Y) ^WExpert Rating: ^c%d^n\r\n", IC->ic.expert);
  send_to_char("^Gq^Y) ^WQuit\r\n^Gx^Y) ^WQuit without saving\r\n^wEnter selection: ", CH);
  d->edit_mode = ICEDIT_MAIN_MENU;
}

void icedit_parse(struct descriptor_data *d, char *arg)
{
  long number, ic_num;
  int i = 0;
  switch (d->edit_mode)
  {
  case ICEDIT_CONFIRM_EDIT:
    switch (*arg) {
    case 'y':
    case 'Y':
      icedit_disp_menu(d);
      break;
    case 'n':
    case 'N':
      STATE(d) = CON_PLAYING;
      if (d->edit_icon)
        Mem->DeleteIcon(d->edit_icon);
      d->edit_icon = NULL;
      PLR_FLAGS(CH).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("That's not a valid choice!\r\n", CH);
      send_to_char("Do you wish to edit it?\r\n", CH);
      break;
    }
    break;
  case ICEDIT_CONFIRM_SAVESTRING:
    switch (*arg) {
    case 'y':
    case 'Y': {
        ic_num = real_ic(d->edit_number);
        if (ic_num > 0) {
          struct matrix_icon *i, *temp;
          for (i = icon_list; i; i = i->next)
            if (i->number == ic_num) {
              temp = Mem->GetIcon();
              *temp = *i;
              *i = *d->edit_icon;
              i->in_host = temp->in_host;
              i->next = temp->next;
              i->next_in_host = temp->next_in_host;
              Mem->ClearIcon(temp);
            }
          if (ic_proto[ic_num].name)
            delete [] ic_proto[ic_num].name;
          if (ic_proto[ic_num].look_desc)
            delete [] ic_proto[ic_num].look_desc;
          if (ic_proto[ic_num].long_desc)
            delete [] ic_proto[ic_num].long_desc;
          ic_proto[ic_num] = *d->edit_icon;
        } else {
          int             counter;
          int             found = 0;
          struct index_data *new_ic_index;
          struct matrix_icon *new_ic_proto;
          new_ic_index = new struct index_data[top_of_ic + 2];
          new_ic_proto = new struct matrix_icon[top_of_ic + 2];

          sprintf(buf,"%s wrote new IC #%ld", GET_CHAR_NAME(CH), d->edit_number);
          mudlog(buf, CH, LOG_WIZLOG, TRUE);
          for (counter = 0; counter < top_of_ic + 1; counter++) {
            if (!found) {
              if (ic_index[counter].vnum > d->edit_number) {
                new_ic_index[counter].vnum = d->edit_number;
                new_ic_index[counter].number = 0;
                new_ic_index[counter].func = NULL;
                new_ic_proto[counter] = *(d->edit_icon);
                new_ic_proto[counter].in_host = NOWHERE;
                d->edit_icon->number = counter;
                new_ic_proto[counter].number = counter;
                found = TRUE;
                new_ic_index[counter + 1] = ic_index[counter];
                new_ic_proto[counter + 1] = ic_proto[counter];
                new_ic_proto[counter + 1].number = counter + 1;
              } else {
                new_ic_proto[counter] = ic_proto[counter];
                new_ic_index[counter] = ic_index[counter];
              }
            } else {
              new_ic_index[counter + 1] = ic_index[counter];
              new_ic_proto[counter + 1] = ic_proto[counter];
              new_ic_proto[counter + 1].number = counter + 1;
            }
          }

          if (!found) {
            new_ic_index[top_of_ic + 1].vnum = d->edit_number;
            new_ic_index[top_of_ic + 1].number = 0;
            new_ic_index[top_of_ic + 1].func = NULL;

            clear_icon(new_ic_proto + top_of_ic + 1);
            new_ic_proto[top_of_ic + 1] = *(d->edit_icon);
            new_ic_proto[top_of_ic + 1].in_host = NOWHERE;
            new_ic_proto[top_of_ic + 1].number = top_of_ic + 1;
            d->edit_icon->number = top_of_ic + 1;
          }
          top_of_ic++;
          delete [] ic_proto;
          delete [] ic_index;
          ic_proto = new_ic_proto;
          ic_index = new_ic_index;
        }
        send_to_char("Writing IC to disk.\r\n", CH);
        write_ic_to_disk(CH->player_specials->saved.zonenum);
        send_to_char("Saved.\r\n", CH);
        Mem->ClearIcon(d->edit_icon);
        d->edit_icon = NULL;
        PLR_FLAGS(CH).RemoveBit(PLR_EDITING);
        STATE(d) = CON_PLAYING;
        send_to_char("Done.\r\n", CH);
        break;
      }
    case 'n':
    case 'N':
      send_to_char("Icon not saved, aborting.\r\n", CH);
      STATE(d) = CON_PLAYING;
      PLR_FLAGS(CH).RemoveBit(PLR_EDITING);
      /* free everything up, including strings etc */
      if (d->edit_icon)
        Mem->DeleteIcon(d->edit_icon);
      d->edit_icon = NULL;
      d->edit_number = 0;
      break;
    default:
      send_to_char("Invalid choice!\r\n", CH);
      send_to_char("Do you wish to save this icon internally?", CH);
      break;
    }
    break;
  case ICEDIT_MAIN_MENU:
    switch (LOWER(*arg)) {
    case 'q':
      d->edit_mode = ICEDIT_CONFIRM_SAVESTRING;
      icedit_parse(d, "y");
      break;
    case 'x':
      d->edit_mode = ICEDIT_CONFIRM_SAVESTRING;
      icedit_parse(d, "n");
      break;
    case '1':
      send_to_char("Enter IC name:", CH);
      d->edit_mode = ICEDIT_NAME;
      break;
    case '2':
      send_to_char("Enter Room Desc:", CH);
      d->edit_mode = ICEDIT_ROOM;
      break;
    case '3':
      send_to_char("Enter IC description:\r\n", CH);
      d->edit_mode = ICEDIT_DESC;
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
      CLS(CH);
      for (i = 0; i < NUM_OF_IC_TYPES; i++)
        send_to_char(CH, "%d) %s\r\n", i + 1, ic_type[i]);
      send_to_char(CH, "Enter IC type: ");
      d->edit_mode =  ICEDIT_TYPE;
      break;
    case '5':
      CLS(CH);
      if (d->edit_icon->ic.type != 0 && d->edit_icon->ic.type != 4 && d->edit_icon->ic.type != 8
          && d->edit_icon->ic.type != 10) {
        send_to_char("Invalid choice!", CH);
        icedit_disp_menu(d);
      } else {
        for (i = 0; i < 4; i++)
          if (d->edit_icon->ic.type == 0 || d->edit_icon->ic.type == 8)
            send_to_char(CH, "%d) %s\r\n", i + 1, crippler[i]);
          else
            send_to_char(CH, "%d) %s\r\n", i + 1, tarbaby[i]);
        send_to_char(CH, "Enter IC Sub-Type: ");
        d->edit_mode = ICEDIT_SUBTYPE;
        break;
      }
      break;
    case '6':
      icedit_disp_option_menu(d);
      break;
    case '7':
      send_to_char(CH, "Enter IC to call with trap option: ");
      d->edit_mode = ICEDIT_TRAP;
      break;
    case '8':
      send_to_char(CH, "Enter Expert rating: ");
      d->edit_mode = ICEDIT_EXPERT;
      break;
    default:
      send_to_char("Invalid choice!", CH);
      icedit_disp_menu(d);
      break;
    }
    break;
  case ICEDIT_EXPERT:
    IC->ic.expert = MIN(3, atoi(arg));
    icedit_disp_menu(d);
    break;
  case ICEDIT_OPTION_MENU:
    number = atoi(arg);
    if (number < 0 || number > IC_TRAP)
      icedit_disp_option_menu(d);
    else if (number == 0)
      icedit_disp_menu(d);
    else {
      IC->ic.options.ToggleBit(number);
      icedit_disp_option_menu(d);
    }
    break;
  case ICEDIT_TRAP:
    d->edit_icon->ic.trap = atoi(arg);
    icedit_disp_menu(d);
    break;
  case ICEDIT_NAME:
    if (d->edit_icon->name)
      delete [] d->edit_icon->name;
    d->edit_icon->name = str_dup(arg);
    icedit_disp_menu(d);
    break;
  case ICEDIT_ROOM:
    if (d->edit_icon->look_desc)
      delete [] d->edit_icon->look_desc;
    d->edit_icon->look_desc = str_dup(arg);
    icedit_disp_menu(d);
    break;
  case ICEDIT_TYPE:
    number = atoi(arg);
    if (number <= 0 || number > NUM_OF_IC_TYPES) {
      send_to_char("Invalid choice!\r\n", CH);
      send_to_char("Enter IC Type: ", CH);
    } else {
      d->edit_icon->ic.type = --number;
      send_to_char("Enter IC Rating: ", CH);
      d->edit_mode = ICEDIT_RATING;
    }
    break;
  case ICEDIT_RATING:
    number = atoi(arg);
    if (number <= 0) {
      send_to_char("Invalid choice!\r\n", CH);
      send_to_char("Enter IC Rating: ", CH);
    } else {
      d->edit_icon->ic.rating = number;
      icedit_disp_menu(d);
    }
    break;
  case ICEDIT_SUBTYPE:
    number = atoi(arg);
    if (number <= 0 || number > 4) {
      send_to_char("Invalid choice!\r\n", CH);
      send_to_char("Enter IC SubType: ", CH);
    } else {
      d->edit_icon->ic.subtype = --number;
      icedit_disp_menu(d);
    }
    break;
  }
}
