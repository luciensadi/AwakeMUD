#include "awake.hpp"
#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "olc.hpp"
#include "newhouse.hpp"

#define APT d->edit_apartment
///////////////////////////// OLC functions below //////////////////////////////
void houseedit_display_apartment_edit_menu(struct descriptor_data *d) {
  vnum_t key_vnum = APT->get_key_vnum();
  rnum_t key_rnum = real_object(key_vnum);

  vnum_t atrium_vnum = APT->get_atrium_vnum();
  rnum_t atrium_rnum = real_room(atrium_vnum);

  const char *room_string = APT->list_rooms__returns_new(TRUE);

  send_to_char(CH, "1) Shortname:      %s^n\r\n", APT->get_short_name());
  send_to_char(CH, "2) Display name:   %s^n\r\n", APT->get_name());
  send_to_char(CH, "3) Escape To:      %s^n (^c%ld^n)\r\n", atrium_rnum >= 0 ? GET_ROOM_NAME(&world[atrium_rnum]) : "^y(none)^n", atrium_vnum);
  send_to_char(CH, "\r\n");
  send_to_char(CH, "4) Lifestyle:      %s^n\r\n", APT->get_lifestyle_string());
  send_to_char(CH, "5) Cost per month: ^c%ld^n nuyen^n\r\n", APT->get_rent_cost());
  send_to_char(CH, "6) Key:            %s^n (^c%ld^n)^n\r\n", key_rnum >= 0 ? GET_OBJ_NAME(&obj_proto[key_rnum]) : "^y(none)^n", key_vnum);
  send_to_char(CH, "\r\n");
  send_to_char(CH, "7) Rooms:\r\n%s^n\r\n", room_string);
  send_to_char(CH, "\r\n");
  send_to_char(CH, "q) Quit and Save\r\n");
  send_to_char(CH, "x) Quit Without Saving\r\n");
  send_to_char(CH, "\r\n");
  send_to_char(CH, "Enter your selection: ");

  delete [] room_string;

  // Set editing state.
  STATE(d) = CON_HOUSEEDIT_APARTMENT;
  d->edit_mode = HOUSEEDIT_APARTMENT_MAIN_MENU;
}

void houseedit_apartment_parse(struct descriptor_data *d, const char *arg) {
  long parsed_long = atol(arg);

  switch (d->edit_mode) {
    case HOUSEEDIT_APARTMENT_MAIN_MENU:
      switch (*arg) {
        case '1':
          break;
        case '2':
          break;
        case '3':
          break;
        case '4':
          break;
        case '5':
          break;
        case '6':
          break;
        case '7':
          break;
        case 'q':
          break;
        case 'x':
          break;
        default:
          send_to_char("That's not a valid choice.\r\n", CH);
          houseedit_display_apartment_edit_menu(d);
          break;
      }
      break;
    case HOUSEEDIT_APARTMENT_SHORTNAME:
      break;
    case HOUSEEDIT_APARTMENT_NAME:
      break;
    case HOUSEEDIT_APARTMENT_ATRIUM:
      break;
    case HOUSEEDIT_APARTMENT_LIFESTYLE:
      break;
    case HOUSEEDIT_APARTMENT_RENT:
      break;
    case HOUSEEDIT_APARTMENT_KEY:
      break;
    case HOUSEEDIT_APARTMENT_ROOMS:
      break;
    default:
      mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: Unknown edit state %d in houseedit_apartment_parse()! Restoring to main menu.", d->edit_mode);
      houseedit_display_apartment_edit_menu(d);
      break;
  }
}

#undef APT
