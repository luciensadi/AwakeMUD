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

extern bool can_edit_zone(struct char_data *ch, rnum_t real_zone);

void houseedit_display_apartment_edit_menu(struct descriptor_data *d);

void houseedit_list_apartments(struct char_data *ch, const char *func_remainder) {
  ApartmentComplex *complex;

  // Error message came from find_apartment_complex().
  if (!(complex = find_apartment_complex(func_remainder, ch)))
    return;

  const char *output = complex->list_apartments__returns_new();
  send_to_char(ch, "%s has the following apartments:\r\n%s\r\n", complex->get_name(), output);
  delete [] output;
}

void houseedit_show_apartment(struct char_data *ch, char *arg) {
  Apartment *apartment = find_apartment(arg, ch);

  // Find the referenced apartment. Error messages sent during function eval.
  if (!apartment)
    return;

  vnum_t key_vnum = apartment->get_key_vnum();
  rnum_t key_rnum = real_object(key_vnum);

  vnum_t atrium_vnum = apartment->get_atrium_vnum();
  rnum_t atrium_rnum = real_room(atrium_vnum);

  const char *room_string = apartment->list_rooms__returns_new(TRUE);

  send_to_char(ch, "Shortname:      %s^n\r\n", apartment->get_short_name());
  send_to_char(ch, "Display name:   %s^n\r\n", apartment->get_name());
  send_to_char(ch, "Escape To:      %s^n (^c%ld^n)\r\n", atrium_rnum >= 0 ? GET_ROOM_NAME(&world[atrium_rnum]) : "^y(none)^n", atrium_vnum);
  send_to_char(ch, "\r\n");
  send_to_char(ch, "Lifestyle:      %s^n%s\r\n",
               lifestyles[apartment->get_lifestyle()].name,
               apartment->is_garage_lifestyle() ? (apartment->get_garage_override() ? " [forced garage]" : " [garage]") : "");
  send_to_char(ch, "\r\n");
  send_to_char(ch, "Cost per month: ^c%ld^n nuyen^n\r\n", apartment->get_rent_cost());
  send_to_char(ch, "Key:            %s^n (^c%ld^n)^n\r\n", key_rnum >= 0 ? GET_OBJ_NAME(&obj_proto[key_rnum]) : "^y(none)^n", key_vnum);
  send_to_char(ch, "\r\n");
  send_to_char(ch, "Complex:        %s^n\r\n", apartment->get_complex() ? apartment->get_complex()->get_name() : "^y(null)^n");
  send_to_char(ch, "\r\n");
  if (apartment->get_paid_until() > 0) {
    const char *owner_name = apartment->get_owner_name__returns_new();
    send_to_char(ch, "Leased To:      %s^n (%ld)\r\n", owner_name, apartment->get_owner_id());

    int seconds = apartment->get_paid_until() - time(0);
    int minutes = seconds / 60;
    int hours = minutes / 60;
    int days = hours / 24;

    if (days > 0) {
      send_to_char(ch, "For the next:   %ld day%s\r\n", days, days == 1 ? "" : "s");
    } else if (hours > 0) {
      send_to_char(ch, "For the next:   %ld hour%s\r\n", hours, hours == 1 ? "" : "s");
    } else if (minutes > 0) {
      send_to_char(ch, "For the next:   %ld minute%s\r\n", minutes, minutes == 1 ? "" : "s");
    } else {
      send_to_char(ch, "For the next:   %ld second%s\r\n", seconds, seconds == 1 ? "" : "s");
    }

    delete [] owner_name;
    send_to_char(ch, "\r\n");
  }
  send_to_char(ch, "Rooms:\r\n%s^n\r\n", room_string);

  delete [] room_string;
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

  // Delete in-memory record. Removal from its complex is done in deconstruction.
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

void houseedit_display_room_edit_menu(struct descriptor_data *d) {
  const char *rooms = APT->list_rooms__returns_new(TRUE);
  send_to_char(CH, "%s\r\n", rooms);
  delete [] rooms;
  send_to_char("Enter a vnum to add or remove, or 0 to quit: ", CH);
  d->edit_mode = HOUSEEDIT_APARTMENT_ROOMS;
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
  send_to_char(CH, "4) Lifestyle:      %s^n%s\r\n", lifestyles[APT->get_lifestyle()].name, APT->is_garage_lifestyle() ? (APT->get_garage_override() ? " [forced garage]" : " [garage]") : "");
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
          if (is_abbrev("unnamed-", APT->get_name())) {
            send_to_char(CH, "You need to set the shortname first.\r\n");
            houseedit_display_room_edit_menu(d);
            return;
          }

          send_to_char("Enter the new display name [ex: Unit 3A]: ", CH);
          d->edit_mode = HOUSEEDIT_APARTMENT_NAME;
          break;
        case '3': // atrium
          send_to_char("Enter the vnum of the room that characters should appear in when using LEAVE: ", CH);
          d->edit_mode = HOUSEEDIT_APARTMENT_ATRIUM;
          break;
        case '4': // lifestyle
          for (int lifestyle_idx = 0; lifestyle_idx < NUM_LIFESTYLES; lifestyle_idx++)
            send_to_char(CH, "%d) %s\r\n", lifestyle_idx, lifestyles[lifestyle_idx].name);
          send_to_char("\r\nEnter the new lifestyle rating: ", CH);
          d->edit_mode = HOUSEEDIT_APARTMENT_LIFESTYLE;
          break;
        case '5': // rent
          if (APT->get_lifestyle() < NUM_LIFESTYLES - 1) {
            send_to_char(CH, "Enter the new rent amount per month (%d - %d): ", lifestyles[APT->get_lifestyle()].monthly_cost_min, lifestyles[APT->get_lifestyle() + 1].monthly_cost_min);
          } else {
            send_to_char(CH, "Enter the new rent amount per month (%d minimum): ", lifestyles[APT->get_lifestyle()].monthly_cost_min);
          }
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
          houseedit_display_room_edit_menu(d);
          break;
        case 'q': // quit and save
        case 'x': // quit, no save
          if (*arg == 'q') {
            if (!APT->get_complex() || !APT->get_short_name() || !*APT->get_short_name()) {
              send_to_char("You'll need to specify the complex and shortname first.\r\n", CH);
              return;
            }

            send_to_char("OK, saving changes.\r\n", CH);

            if (APT->get_rent_cost() < lifestyles[APT->get_lifestyle()].monthly_cost_min) {
              send_to_char(CH, "Rent is too low, so raising it to %ld.\r\n", lifestyles[APT->get_lifestyle()].monthly_cost_min);
            }

            // It already existed: Overwrite.
            if (d->edit_apartment_original) {
              // Log.
              mudlog_vfprintf(CH, LOG_WIZLOG, "%s overwriting apartment: %s.",
                              GET_CHAR_NAME(CH),
                              d->edit_apartment_original->get_full_name());


              for (auto &room : d->edit_apartment_original->get_rooms()) {
                // Strip apartment status from all prior world-rooms.
                rnum_t rnum = real_room(room->get_vnum());
                if (rnum >= 0) {
                  world[rnum].apartment = NULL;
                  world[rnum].apartment_room = NULL;
                }

                // Invalidate the room on disk. This ensures it's not brought back on copyover.
                room->delete_info();
              }

              // Copy over our changes.
              d->edit_apartment_original->clone_from(APT);

              for (auto &room : d->edit_apartment_original->get_rooms()) {
                // Apply room status to all new rooms.
                rnum_t rnum = real_room(room->get_vnum());
                if (rnum >= 0) {
                  world[rnum].apartment = d->edit_apartment_original;
                  world[rnum].apartment_room = room;
                }

                // We don't write the files for the new world-rooms here-- that's done in save_rooms().
              }

              // Write to disk.
              d->edit_apartment_original->save_base_info();
              d->edit_apartment_original->save_rooms();

              // Iterate over all characters in the game and have them recalculate their lifestyles. This also confirms lifestyle string validity.
              for (struct char_data *plr = character_list; plr; plr = plr->next) {
                calculate_best_lifestyle(plr);
              }
            }
            // It didn't exist: Save new.
            else {
              // Log.
              mudlog_vfprintf(CH, LOG_WIZLOG, "%s wrote new apartment: %s.",
                              GET_CHAR_NAME(CH),
                              APT->get_full_name());

              // Add to complex's apartment list.
              COMPLEX->add_apartment(APT);

              // Write to disk.
              APT->save_base_info();
              APT->save_rooms();

              // Apply to world.
              APT->apply_rooms();

              // Null edit_apartment so it's not deleted later.
              APT = NULL;
            }

            // Fall through.
          } else {
            send_to_char("OK, discarding changes.\r\n", CH);
          }

          delete APT;
          APT = NULL;
          d->edit_apartment_original = NULL;
          STATE(d) = CON_PLAYING;
          break;
        default:
          send_to_char("That's not a valid choice.\r\n", CH);
          houseedit_display_apartment_edit_menu(d);
          break;
      }
      break;
    case HOUSEEDIT_APARTMENT_SHORTNAME:
      if (strlen(arg) == 0) {
        send_to_char("OK, aborting.\r\n", CH);
        houseedit_display_apartment_edit_menu(d);
        return;
      }

      // Length constraints.
      if (strlen(arg) < 1 || strlen(arg) > 20) {
        send_to_char("Name must be between 1 and 20 characters. Try again: ", CH);
        return;
      }

      // No color allowed.
      if (strcmp(arg, get_string_after_color_code_removal(arg, NULL))) {
        send_to_char("Apartment shortnames can't contain color codes. Try again: ", CH);
        return;
      }

      // Must be a valid filename.
      if (!string_is_valid_for_paths(arg)) {
        send_to_char("Apartment shortnames can only contain letters, numbers, and certain punctuation. Try again: ", CH);
        return;
      }

      // Enforce uniqueness.
      if (COMPLEX) {
        for (auto &apartment : COMPLEX->get_apartments()) {
          if (!strcmp(arg, apartment->get_short_name())) {
            send_to_char("That short name is already in use in this complex. Please choose another, or enter nothing to abort: ", CH);
            return;
          }
        }
      }

      // Enforce character constraints (0-9a-zA-Z)
      for (const char *chk = arg; *chk; chk++) {
        if (!isalnum(*chk)) {
          send_to_char("Shortnames can only contain letters and/or numbers (ex: 3A). Please choose another: ", CH);
          return;
        }
      }

      {
        // Create our new dir and check to make sure it doesn't already exist.
        bf::path new_save_dir = APT->get_base_directory() / arg; // safe because we filtered characters earlier
        if (bf::exists(new_save_dir) && (!d->edit_apartment_original || new_save_dir != d->edit_apartment_original->get_base_directory())) {
          send_to_char("There's already a directory that matches your new apartment. You'll need to change its name.\r\n", CH);
          houseedit_display_apartment_edit_menu(d);
          return;
        }
        APT->set_base_directory(new_save_dir);
      }

      // If the apartment's name doesn't contain this string, clear it.
      if (APT->get_name() && !strstr(APT->get_name(), arg)) {
        send_to_char(CH, "%s is not contained in %s, so I'll create a new apartment name.\r\n", arg, APT->get_name());

        char default_name[500];
        snprintf(default_name, sizeof(default_name), "Unit %s", arg);
        APT->set_name(default_name);
      }

      d->edit_apartment->set_short_name(arg);

      houseedit_display_apartment_edit_menu(d);
      break;
    case HOUSEEDIT_APARTMENT_NAME:
      if (strlen(arg) == 0) {
        send_to_char("OK, aborting.\r\n", CH);
        houseedit_display_apartment_edit_menu(d);
        return;
      }

      // Length constraints.
      if (strlen(arg) < 3 || strlen(arg) > 30) {
        send_to_char("Name must be between 3 and 30 characters. Try again: ", CH);
        return;
      }

      // No color allowed.
      if (strcmp(arg, get_string_after_color_code_removal(arg, NULL))) {
        send_to_char("Apartment names can't contain color codes. Try again: ", CH);
        return;
      }

      // Must be a valid filename.
      if (!string_is_valid_for_paths(arg)) {
        send_to_char("Apartment names can only contain letters, numbers, and certain punctuation. Try again: ", CH);
        return;
      }

      // Enforce uniqueness.
      if (COMPLEX) {
        for (auto &apartment : COMPLEX->get_apartments()) {
          if (!strcmp(arg, apartment->get_name())) {
            send_to_char("That apartment name is already in use in this complex. Please choose another, or enter nothing to abort: ", CH);
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
      if (parsed_int != -1 && (parsed_int < COMPLEX->get_lifestyle() || parsed_int >= NUM_LIFESTYLES)) {
        send_to_char(CH, "That's not a valid lifestyle. Enter a lifestyle number between %d and %d, or -1 to use the complex's lifestyle: ", COMPLEX->get_lifestyle(), NUM_LIFESTYLES - 1);
        return;
      }

      APT->set_lifestyle(parsed_int, CH);

      send_to_char("Is this apartment primarily a garage? Y/N: ", CH);
      d->edit_mode = HOUSEEDIT_APARTMENT_GARAGE_OVERRIDE;
      break;
    case HOUSEEDIT_APARTMENT_GARAGE_OVERRIDE:
      if (*arg == 'y' || *arg == 'Y') {
        APT->set_garage_override(TRUE);
      } else if (*arg == 'n' || *arg == 'N') {
        APT->set_garage_override(FALSE);
      } else {
        send_to_char("That's not a choice. Select Y or N: ", CH);
        return;
      }
      houseedit_display_apartment_edit_menu(d);
      break;
    case HOUSEEDIT_APARTMENT_RENT:
      // Don't allow changes to leased apartment rents.
      if (APT->get_paid_until() > 0) {
        if (!access_level(CH, LVL_ADMIN)) {
          send_to_char("You can't edit the rent amount for a leased apartment at your level.\r\n", CH);
          houseedit_display_apartment_edit_menu(d);
          return;
        } else {
          send_to_char(CH, "WARNING: Editing rent value for an apartment that's already in use! Was %d. Player will not receive a refund or deduction.\r\n", APT->get_rent_cost());
        }
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
      {
        vnum_t vnum = parsed_long;
        rnum_t rnum = real_room(vnum);

        if (vnum == 0) {
          // They want to quit.
          houseedit_display_apartment_edit_menu(d);
          return;
        }

        if (rnum < 0) {
          send_to_char("That's not a valid vnum.\r\n", CH);
          houseedit_display_apartment_edit_menu(d);
          return;
        }

        struct room_data *room = &world[rnum];

        // First, iterate through our own rooms. If it's there, we want to remove it.
        for (auto &apt_room : APT->get_rooms()) {
          if (apt_room->get_vnum() == GET_ROOM_VNUM(room)) {
            APT->delete_room(apt_room);
            send_to_char(CH, "OK, deleted %s^n from the room list.", GET_ROOM_NAME(room));
            houseedit_display_room_edit_menu(d);
            return;
          }
        }

        // It wasn't there, so we want to add it. We require that the builder has edit perms over this room.
        int real_zonenum = get_zone_index_number_from_vnum(vnum);
        if (real_zonenum < 0 || real_zonenum > top_of_zone_table) {
          send_to_char(CH, "%ld is not part of any zone.\r\n", vnum);
        } else if (!can_edit_zone(CH, real_zonenum)) {
          send_to_char(CH, "Sorry, you don't have access to edit zone %ld.\r\n", zone_table[(real_zonenum)].number);
        } else if (zone_table[(real_zonenum)].connected && !(access_level(CH, LVL_EXECUTIVE) || PLR_FLAGGED(CH, PLR_EDCON))) {
          send_to_char(CH, "Sorry, zone %d is marked as connected to the game world, so you can't edit it.\r\n", zone_table[(real_zonenum)].number);
        }
        // They have edit permissions, so we can go ahead and add it.
        else {
          // Sanity check: Room may not be part of an apartment already.
          if (GET_APARTMENT(room) && GET_APARTMENT(room) != d->edit_apartment_original) {
            send_to_char(CH, "That room is already part of %s.", GET_APARTMENT(room)->get_full_name());
            houseedit_display_room_edit_menu(d);
            return;
          }

          // Add ApartmentRoom that points to room, then add it to our list of rooms.
          ApartmentRoom *new_aptroom = new ApartmentRoom(APT, room);
          APT->add_room(new_aptroom);
          send_to_char("Done.\r\n", CH);
        }
      }
      houseedit_display_room_edit_menu(d);
      break;
    default:
      mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: Unknown edit state %d in houseedit_apartment_parse()! Restoring to main menu.", d->edit_mode);
      houseedit_display_apartment_edit_menu(d);
      break;
  }
}

#undef APT
#undef COMPLEX
