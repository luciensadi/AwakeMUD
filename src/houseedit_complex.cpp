#include "awake.hpp"
#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "olc.hpp"
#include "newhouse.hpp"
#include "houseedit_complex.hpp"

void houseedit_create_complex(struct char_data *ch) {
  FAILURE_CASE(!ch->desc, "I don't know how you got here, but this won't work.");

  ch->desc->edit_complex = new ApartmentComplex();
  ch->desc->edit_complex_original = NULL;
  houseedit_display_complex_edit_menu(ch->desc);
}

void houseedit_delete_complex(struct char_data *ch, char *arg) {
  ApartmentComplex *complex;

  // Find the referenced complex. Error messages sent during function eval.
  if (!(complex = find_apartment_complex(arg, ch)))
    return;

  FAILURE_CASE(!complex->can_houseedit_complex(ch), "You don't have edit access for that complex.");
  FAILURE_CASE(!complex->get_apartments().empty(), "You can only delete complexes that have no apartments associated with them.");

  // TODO: Log.
  // TODO: Remove the complex from the global list.
  // TODO: Delete the complex.

  send_to_char(ch, "HDC: Not yet implemented. Got complex %s though.", complex->get_name());
}

void houseedit_list_complexes(struct char_data *ch, char *arg) {
  if (global_apartment_complexes.size() <= 0) {
    send_to_char("There are no complexes currently defined.\r\n", ch);
    return;
  }

  if (arg && *arg)
    send_to_char(ch, "The following complexes starting with '%s' exist:\r\n", arg);
  else
    send_to_char("The following complexes exist:\r\n", ch);

  for (auto &complex : global_apartment_complexes) {
    if (arg && *arg && !is_abbrev(arg, complex->get_name()))
      continue;

    send_to_char(ch, "  %s^n: Landlord ^c%ld^n, ^c%d^n apartments, editable by: %s\r\n",
                 complex->get_name(),
                 complex->get_landlord_vnum(),
                 complex->get_apartments().size(),
                 complex->list_editors());
  }
}

void houseedit_edit_existing_complex(struct char_data *ch, char *arg) {
  ApartmentComplex *complex;

  if (!*arg) {
    // Use the one they're standing in.
    FAILURE_CASE(!ch->in_room || !ch->in_room->apartment, "You must either stand in an apartment OR use ^WHOUSEEDIT COMPLEX <name>^n.");
    complex = ch->in_room->apartment->get_complex();
  } else {
    // Find the referenced complex. Error messages sent during function eval.
    if (!(complex = find_apartment_complex(arg, ch)))
      return;
  }

  FAILURE_CASE(!complex->can_houseedit_complex(ch), "You don't have edit access for that complex.");

  // Allowed to edit: Put them into that mode.
  ch->desc->edit_complex = new ApartmentComplex();
  ch->desc->edit_complex->clone_from(complex);
  ch->desc->edit_complex_original = complex;
  houseedit_display_complex_edit_menu(ch->desc);
}

///////////////////////////// OLC functions below //////////////////////////////
void houseedit_display_complex_edit_menu(struct descriptor_data *d) {
  char landlord_name[500];
  rnum_t landlord_rnum;

  // Fetch landlord name if available.
  if ((landlord_rnum = real_mobile(d->edit_complex->get_landlord_vnum())) < 0) {
    strlcpy(landlord_name, "(invalid)", sizeof(landlord_name));
  } else {
    strlcpy(landlord_name, GET_CHAR_NAME(&mob_proto[landlord_rnum]), sizeof(landlord_name));
  }

  // Display info.
  send_to_char("^WApartment Complex Editing^n\r\n", CH);
  send_to_char(CH, "1) Name:     ^c%s^n\r\n", d->edit_complex->get_name());
  send_to_char(CH, "2) Landlord: ^c%s^n (^c%ld^n)\r\n", landlord_name, d->edit_complex->get_landlord_vnum());
  send_to_char(CH, "3) Editors:  ^c%s^n\r\n", d->edit_complex->list_editors());
  send_to_char("\r\n", CH);
  send_to_char("q) Quit and Save\r\n", CH);
  send_to_char("x) Quit Without Saving\r\n", CH);
  send_to_char("\r\n", CH);
  send_to_char("Make a selection: ", CH);

  // Set editing state.
  STATE(d) = CON_HOUSEEDIT_COMPLEX;
  d->edit_mode = HOUSEEDIT_COMPLEX_MAIN_MENU;
}

void houseedit_complex_parse(struct descriptor_data *d, const char *arg) {
  switch (d->edit_mode) {
    case HOUSEEDIT_COMPLEX_MAIN_MENU:
      if (*arg == '1') { // Complex name
        send_to_char("Enter a new name: ", CH);
        d->edit_mode = HOUSEEDIT_COMPLEX_NAME;
      }
      else if (*arg == '2') { // Landlord vnum
        send_to_char("Enter the vnum of the new landlord: ", CH);
        d->edit_mode = HOUSEEDIT_COMPLEX_LANDLORD;
      }
      else if (*arg == '3') { // Editors
        // TODO
        d->edit_mode = HOUSEEDIT_COMPLEX_EDITORS;
      }
      else if (*arg == 'q' || *arg == 'x') {
        if (*arg == 'q') {
          send_to_char("OK, saving changes.\r\n", CH);

          // TODO: Remove landlord spec from old LL and give it to new.

          // It already existed: Overwrite.
          if (d->edit_complex_original) {
            mudlog_vfprintf(CH, LOG_WIZLOG, "%s overwriting complex %s (%ld) to %s (%ld).\r\n",
                            GET_CHAR_NAME(CH),
                            d->edit_complex_original->get_name(),
                            d->edit_complex_original->get_landlord_vnum(),
                            d->edit_complex->get_name(),
                            d->edit_complex->get_landlord_vnum());
            d->edit_complex_original->clone_from(d->edit_complex);
            d->edit_complex_original->save();
          }
          // It didn't exist: Save new.
          else {
            mudlog_vfprintf(CH, LOG_WIZLOG, "%s wrote new complex %s (%ld).\r\n",
                            GET_CHAR_NAME(CH),
                            d->edit_complex->get_name(),
                            d->edit_complex->get_landlord_vnum());
            global_apartment_complexes.push_back(d->edit_complex);
            d->edit_complex->save();
            // Null edit_complex so it's not deleted later.
            d->edit_complex = NULL;
          }
          // Fall through.
        } else {
          send_to_char("OK, discarding changes.\r\n", CH);
        }

        delete d->edit_complex;
        d->edit_complex = NULL;
        STATE(d) = CON_PLAYING;
      }
      else {
        send_to_char("Sorry, that's not an option.\r\n", CH);
        houseedit_display_complex_edit_menu(d);
      }
      break;
    case HOUSEEDIT_COMPLEX_NAME:
      d->edit_complex->set_name(arg);
      houseedit_display_complex_edit_menu(d);
      break;
    case HOUSEEDIT_COMPLEX_LANDLORD:
      if (!d->edit_complex->set_landlord_vnum(atol(arg))) {
        send_to_char("That's not a valid vnum.\r\n", CH);
      }
      houseedit_display_complex_edit_menu(d);
      break;
    case HOUSEEDIT_COMPLEX_EDITORS:
      // TODO
      break;
    default:
      mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: Unknown edit state %d in houseedit_complex_parse()! Restoring to main menu.", d->edit_mode);
      houseedit_display_complex_edit_menu(d);
      break;
  }
}
