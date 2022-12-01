#include "awake.hpp"
#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "olc.hpp"
#include "newhouse.hpp"

#define APT d->edit_apartment
#define COMPLEX d->edit_apartment->get_complex()

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

  // Enforce permissions.
  FAILURE_CASE(!apartment->can_houseedit_apartment(ch), "You don't have edit access for that apartment.");

  // Verify apartment has no current lease.
  FAILURE_CASE(apartment->get_paid_until() > 0, "You can only delete apartments that are not leased.");

  mudlog_vfprintf(ch, LOG_SYSLOG, "%s deleted apartment: %s.", GET_CHAR_NAME(ch), apartment->get_full_name());

  // Mark files as deleted.
  apartment->mark_as_deleted();

  // Delete in-memory record.
  delete apartment;
}

void houseedit_edit_apartment(struct char_data *ch, const char *func_remainder) {
  FAILURE_CASE(!ch->desc, "I don't know how you got here, but this won't work.");

  // houseedit apartment edit [full name]
  Apartment *apartment = find_apartment(func_remainder, ch);

  // Error message came from find_apartment().
  if (!apartment)
    return;

  FAILURE_CASE(!apartment->can_houseedit_apartment(ch), "You don't have edit access for that apartment.");

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
  send_to_char(CH, "7) Complex:        %s^n\r\n", COMPLEX ? COMPLEX->get_name() : "^y(null)^n");
  send_to_char(CH, "\r\n");
  send_to_char(CH, "8) Rooms:\r\n%s^n\r\n", room_string);
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
  long parsed_int = atoi(arg);

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
          // EVENTUALTODO - present list of lifestyles, select from among them
          // d->edit_mode = HOUSEEDIT_APARTMENT_LIFESTYLE;
          send_to_char("Lifestyles not yet implemented.\r\n", CH);
          houseedit_display_apartment_edit_menu(d);
          break;
        case '5': // rent
          send_to_char("Enter the new rent amount per month: ", CH);
          d->edit_mode = HOUSEEDIT_APARTMENT_RENT;
          break;
        case '6': // key vnum
          send_to_char("Enter the vnum of the new key: ", CH);
          d->edit_mode = HOUSEEDIT_APARTMENT_KEY;
          break;
        case '7': // complex
          send_to_char("Enter the name of the complex you'd like this apartment to be part of: ", CH);
          d->edit_mode = HOUSEEDIT_APARTMENT_COMPLEX;
          break;
        case '8': // rooms
          {
            const char *rooms = APT->list_rooms__returns_new(TRUE);
            send_to_char(CH, "%s\r\n", rooms);
            delete [] rooms;
            send_to_char("Enter a vnum to add or remove: ", CH);
          }
          d->edit_mode = HOUSEEDIT_APARTMENT_ROOMS;
          break;
        case 'q': // quit and save
        case 'x': // quit, no save
          // TODO
          break;
        default:
          send_to_char("That's not a valid choice.\r\n", CH);
          houseedit_display_apartment_edit_menu(d);
          break;
      }
      break;
    case HOUSEEDIT_APARTMENT_SHORTNAME:
      // No color allowed.
      if (strcmp(arg, get_string_after_color_code_removal(arg, NULL))) {
        send_to_char("Apartment shortnames can't contain color codes. Try again: ", CH);
        return;
      }

      // Enforce uniqueness.
      if (COMPLEX) {
        for (auto &apartment : COMPLEX->get_apartments()) {
          if (!strcmp(arg, apartment->get_short_name())) {
            send_to_char("That short name is already in use in this complex. Please choose another: ", CH);
            return;
          }
        }
      }

      // If the apartment's name doesn't contain this string, clear it.
      if (APT->get_name() && !strstr(APT->get_name(), arg)) {
        send_to_char(CH, "%s is not contained in %s, so I'll create a new apartment name.\r\n", arg, APT->get_name());

        char default_name[500];
        snprintf(default_name, sizeof(default_name), "Unit %s", arg);
        APT->set_name(default_name);
      }

      d->edit_apartment->set_name(arg);
      houseedit_display_apartment_edit_menu(d);
      break;
    case HOUSEEDIT_APARTMENT_NAME:
      // No color allowed.
      if (strcmp(arg, get_string_after_color_code_removal(arg, NULL))) {
        send_to_char("Apartment names can't contain color codes. Try again: ", CH);
        return;
      }

      // Enforce uniqueness.
      if (COMPLEX) {
        for (auto &apartment : COMPLEX->get_apartments()) {
          if (!strcmp(arg, apartment->get_name())) {
            send_to_char("That short name is already in use in this complex. Please choose another: ", CH);
            return;
          }
        }
      }

      // Name must contain short name.
      if (APT->get_short_name() && !strstr(arg, APT->get_short_name())) {
        send_to_char(CH, "You must specify an apartment name that contains the short name %s. Try again: ", APT->get_short_name());
        return;
      }

      APT->set_name(arg);
      houseedit_display_apartment_edit_menu(d);
      break;
    case HOUSEEDIT_APARTMENT_ATRIUM:
      {
        vnum_t vnum = parsed_long;
        rnum_t rnum = real_room(vnum);

        if (rnum < 0) {
          send_to_char("That's not a valid vnum.\r\n", CH);
          houseedit_display_apartment_edit_menu(d);
          return;
        }

        APT->set_atrium(vnum);
      }
      houseedit_display_apartment_edit_menu(d);
      break;
    case HOUSEEDIT_APARTMENT_LIFESTYLE:
      // We push all the error checking into the lifestyle setting function.
      APT->set_lifestyle(parsed_int, CH);
      houseedit_display_apartment_edit_menu(d);
      break;
    case HOUSEEDIT_APARTMENT_RENT:
      // Don't allow changes to leased apartment rents.
      if (APT->get_paid_until() > 0 && !access_level(CH, LVL_ADMIN)) {
        send_to_char("You can't edit the rent amount for a leased apartment at your level.\r\n", CH);
        houseedit_display_apartment_edit_menu(d);
        return;
      }

      // We push all the remaining error checking into the rent setting function.
      APT->set_rent(parsed_long, CH);
      houseedit_display_apartment_edit_menu(d);
      break;
    case HOUSEEDIT_APARTMENT_KEY:
      {
        vnum_t vnum = parsed_long;
        rnum_t rnum = real_object(vnum);

        if (rnum < 0) {
          send_to_char("That's not a valid vnum.\r\n", CH);
          houseedit_display_apartment_edit_menu(d);
          return;
        }

        d->edit_apartment->set_key_vnum(vnum);
      }
      houseedit_display_apartment_edit_menu(d);
      break;
    case HOUSEEDIT_APARTMENT_COMPLEX:
      {
        ApartmentComplex *complex = find_apartment_complex(arg, CH);

        if (complex) {
          // Require edit access to that complex.
          if (!complex->can_houseedit_complex(CH)) {
            send_to_char(CH, "Sorry, you don't have access to edit %s.\r\n", complex->get_name());
          } else {
            // Transfer it. Handling happens in apartment transfer func.
            APT->set_complex(complex);
          }
        }
      }
      houseedit_display_apartment_edit_menu(d);
      break;
    case HOUSEEDIT_APARTMENT_ROOMS:
      // TODO: Enforce can_edit_zone for the room vnum
      // TODO: Adding
        // TODO: Enforce that room is not part of an apt already
        // TODO: Add apartmentroom that points to room, then add it
      // TODO: Removing
        // TODO: Enforce that room is already part of this apt
        // TODO: Remove room and delete apartmentroom for it
      break;
    default:
      mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: Unknown edit state %d in houseedit_apartment_parse()! Restoring to main menu.", d->edit_mode);
      houseedit_display_apartment_edit_menu(d);
      break;
  }
}

#undef APT
#undef COMPLEX
