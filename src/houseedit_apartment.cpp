#include "awake.hpp"
#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "olc.hpp"
#include "newhouse.hpp"

#define APT d->edit_apartment

void houseedit_display_apartment_edit_menu(struct descriptor_data *d);

void houseedit_list_apartments(struct char_data *ch, const char *func_remainder) {
  ApartmentComplex *complex;

  // Error message came from find_apartment_complex().
  if (!(complex = find_apartment_complex(func_remainder, ch)))
    return;

  const char *output = complex->list_apartments__returns_new();
  send_to_char(ch, "%s has the following apartments:\r\n%s\r\n", output);
  delete [] output;
}

void houseedit_create_apartment(struct char_data *ch, const char *func_remainder) {
  FAILURE_CASE(!ch->desc, "I don't know how you got here, but this won't work.");

  ApartmentComplex *complex;

  // Error message came from find_apartment_complex().
  if (!(complex = find_apartment_complex(func_remainder, ch)))
    return;

  FAILURE_CASE(!complex->can_houseedit_complex(ch), "You're not an editor of that complex.");

  ch->desc->edit_apartment = new Apartment();
  ch->desc->edit_apartment->set_complex(complex);
  ch->desc->edit_apartment_original = NULL;
  houseedit_display_apartment_edit_menu(ch->desc);
}

void houseedit_delete_apartment(struct char_data *ch, const char *func_remainder) {
  Apartment *apartment = find_apartment(func_remainder, ch);

  // Error message came from find_apartment().
  if (!apartment)
    return;

  // Verify apartment has no current lease.
  FAILURE_CASE(apartment->get_paid_until() > 0, "You can only delete apartments that are not leased.");

  // TODO: Delete apartment.
}

void houseedit_edit_apartment(struct char_data *ch, const char *func_remainder) {
  FAILURE_CASE(!ch->desc, "I don't know how you got here, but this won't work.");

  // houseedit apartment edit [full name]
  Apartment *apartment;

  if (!*arg) {
    // Use the one they're standing in.
    FAILURE_CASE(!ch->in_room || !ch->in_room->apartment, "You must either stand in an apartment OR use ^WHOUSEEDIT APARTMENT <full name>^n.");
    apartment = ch->in_room->apartment->get_apartment();
  } else {
    // Find the referenced complex. Error messages sent during function eval.
    if (!(apartment = find_apartment(arg, ch)))
      return;
  }

  FAILURE_CASE(!apartment->can_houseedit_apartment(ch), "You don't have edit access for that complex.");

  // Allowed to edit: Put them into that mode.
  ch->desc->edit_apartment = new Apartment();
  ch->desc->edit_apartment->clone_from(apartment);
  ch->desc->edit_apartment_original = apartment;
  houseedit_display_apartment_edit_menu(ch->desc);
}

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
        case '1': // shortname
          send_to_char("Enter the new shortname [ex: 3A]: ", CH);
          d->edit_mode = HOUSEEDIT_APARTMENT_SHORTNAME;
          break;
        case '2': // display name
          send_to_char("Enter the new display name [ex: Unit 3A]: ", CH);
          d->edit_mode = HOUSEEDIT_APARTMENT_NAME;
          break;
        case '3': // atrium
          send_to_char("Enter the vnum of the room that characters should appear in when using LEAVE: ", CH);
          d->edit_mode = HOUSEEDIT_APARTMENT_ATRIUM;
          break;
        case '4': // lifestyle
          // TODO - present list of lifestyles, select from among them
          d->edit_mode = HOUSEEDIT_APARTMENT_LIFESTYLE;
          break;
        case '5': // rent
          send_to_char("Enter the new rent amount per month: ", CH);
          d->edit_mode = HOUSEEDIT_APARTMENT_RENT;
          break;
        case '6': // key vnum
          send_to_char("Enter the vnum of the new key: ", CH);
          d->edit_mode = HOUSEEDIT_APARTMENT_KEY;
          break;
        case '7': // rooms
          // TODO - present list of rooms, give option to add/delete by vnum
          d->edit_mode = HOUSEEDIT_APARTMENT_ROOMS;
          break;
        case 'q': // quit and save
        case 'x': // quit, no save
          break;
        default:
          send_to_char("That's not a valid choice.\r\n", CH);
          houseedit_display_apartment_edit_menu(d);
          break;
      }
      break;
    case HOUSEEDIT_APARTMENT_SHORTNAME:
      // TODO: Enforce uniqueness constraints (either here or in class)
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
