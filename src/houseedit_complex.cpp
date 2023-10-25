#include <algorithm>

#include "awake.hpp"
#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "olc.hpp"
#include "newhouse.hpp"
#include "houseedit_complex.hpp"

#define COMPLEX d->edit_complex

void houseedit_create_complex(struct char_data *ch) {
  FAILURE_CASE(!ch->desc, "I don't know how you got here, but this won't work.");

  ch->desc->edit_complex = new ApartmentComplex();
  ch->desc->edit_complex->add_editor(GET_IDNUM(ch));
  ch->desc->edit_complex_original = NULL;
  houseedit_display_complex_edit_menu(ch->desc);
  // Save directory is set when the new complex is saved.
}

void houseedit_delete_complex(struct char_data *ch, char *arg) {
  ApartmentComplex *complex;

  // Find the referenced complex. Error messages sent during function eval.
  if (!(complex = find_apartment_complex(arg, ch)))
    return;

  FAILURE_CASE(!complex->can_houseedit_complex(ch), "You don't have edit access for that complex.");
  FAILURE_CASE(!complex->get_apartments().empty(), "You can only delete complexes that have no apartments associated with them.");

  mudlog_vfprintf(ch, LOG_SYSLOG, "%s deleted empty apartment complex %s.", GET_CHAR_NAME(ch), complex->get_name());

  // Un-mark our landlord.
  {
    rnum_t rnum;
    if (complex->get_landlord_vnum() > 0 && (rnum = real_mobile(complex->get_landlord_vnum())) >= 0) {
      if (mob_index[rnum].sfunc == landlord_spec)
        mob_index[rnum].sfunc = NULL;
      if (mob_index[rnum].func == landlord_spec)
        mob_index[rnum].func = mob_index[rnum].sfunc;
    }
  }

  // Remove the complex from the global list.
  global_apartment_complexes.erase(find(global_apartment_complexes.begin(), global_apartment_complexes.end(), complex));

  // Archive the complex files so they're not re-loaded on copyover.
  complex->mark_as_deleted();

  delete complex;
}

void houseedit_show_complex(struct char_data *ch, char *arg) {
  ApartmentComplex *complex;

  // Find the referenced complex. Error messages sent during function eval.
  if (!(complex = find_apartment_complex(arg, ch)))
    return;

  char landlord_name[500];
  rnum_t landlord_rnum;

  // Fetch landlord name if available.
  if ((landlord_rnum = real_mobile(complex->get_landlord_vnum())) < 0) {
    strlcpy(landlord_name, "(invalid)", sizeof(landlord_name));
  } else {
    strlcpy(landlord_name, GET_CHAR_NAME(&mob_proto[landlord_rnum]), sizeof(landlord_name));
  }

  send_to_char(ch, "Name:     ^c%s^n\r\n", complex->get_name());
  send_to_char(ch, "Landlord: ^c%s^n (^c%ld^n)\r\n", landlord_name, complex->get_landlord_vnum());
  send_to_char(ch, "Editors:  ^c%s^n\r\n", complex->list_editors());
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

  size_t max_len = 0;
  for (auto &complex : global_apartment_complexes) {
    if (arg && *arg && !is_abbrev(arg, complex->get_name()))
      continue;
    max_len = MAX(strlen(complex->get_name()), max_len);
  }

  // Calculate our formatting string. Note that this doesn't take any user-supplied content.
  char formatting_string[500];
  snprintf(formatting_string, sizeof(formatting_string), "  ^C%%-%lds^n  Landlord ^c%%6ld^n, ^c%%2d^n apartment%%s, editable by: %%s\r\n", max_len);

  for (auto &complex : global_apartment_complexes) {
    if (arg && *arg && !is_abbrev(arg, complex->get_name()))
      continue;

    send_to_char(ch, formatting_string,
                 complex->get_name(),
                 complex->get_landlord_vnum(),
                 complex->get_apartments().size(),
                 complex->get_apartments().size() == 1 ? "" : "s",
                 complex->list_editors());
  }
}

void houseedit_edit_existing_complex(struct char_data *ch, char *arg) {
  FAILURE_CASE(!ch->desc, "I don't know how you got here, but this won't work.");

  ApartmentComplex *complex;

  // Find the referenced complex. Error messages sent during function eval.
  if (!(complex = find_apartment_complex(arg, ch)))
    return;

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
  if ((landlord_rnum = real_mobile(COMPLEX->get_landlord_vnum())) < 0) {
    strlcpy(landlord_name, "(invalid)", sizeof(landlord_name));
  } else {
    strlcpy(landlord_name, GET_CHAR_NAME(&mob_proto[landlord_rnum]), sizeof(landlord_name));
  }

  // Display info.
  send_to_char("^WApartment Complex Editing^n\r\n", CH);
  send_to_char(CH, "1) Name:     ^c%s^n\r\n", COMPLEX->get_name());
  send_to_char(CH, "2) Landlord: ^c%s^n (^c%ld^n)\r\n", landlord_name, COMPLEX->get_landlord_vnum());
  send_to_char(CH, "3) Editors:  ^c%s^n\r\n", COMPLEX->list_editors());
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
  long parsed_long = atol(arg);

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
        send_to_char(CH, "The current editor set is %s. Enter a name to add/remove, or 0 to quit: ", COMPLEX->list_editors());
        d->edit_mode = HOUSEEDIT_COMPLEX_EDITORS;
      }
      else if (*arg == 'q' || *arg == 'x') {
        if (*arg == 'q') {
          // Sanity check: Name must be valid.
          if (!COMPLEX->get_name() || !*COMPLEX->get_name()) {
            send_to_char("You must specify a valid name to save this complex.\r\n", CH);
            houseedit_display_complex_edit_menu(d);
            return;
          }

          // Sanity check: Landlord must be valid.
          if (COMPLEX->get_landlord_vnum() <= 0 || real_mobile(COMPLEX->get_landlord_vnum()) < 0) {
            send_to_char("You must specify a valid landlord vnum to save this complex.\r\n", CH);
            houseedit_display_complex_edit_menu(d);
            return;
          }

          send_to_char("OK, saving changes.\r\n", CH);

          // It already existed: Overwrite.
          if (d->edit_complex_original) {
            // Log.
            mudlog_vfprintf(CH, LOG_WIZLOG, "%s overwriting complex %s (%ld) [%s] to %s (%ld) [%s].",
                            GET_CHAR_NAME(CH),
                            d->edit_complex_original->get_name(),
                            d->edit_complex_original->get_landlord_vnum(),
                            d->edit_complex_original->list_editors(),
                            COMPLEX->get_name(),
                            COMPLEX->get_landlord_vnum(),
                            COMPLEX->list_editors());

            // Landlord spec is cleared/set here.
            d->edit_complex_original->clone_from(COMPLEX);

            // Write to disk.
            d->edit_complex_original->save();

            // Update apartment full names.
            for (auto &apartment : d->edit_complex_original->get_apartments()) {
              apartment->regenerate_full_name();
            }
          }
          // It didn't exist: Save new.
          else {
            // Compose our new base path.
            {
              const char *name_without_color = get_string_after_color_code_removal(COMPLEX->get_name(), CH);
              if (!name_without_color || !*name_without_color) {
                send_to_char("Sorry, you've specified an invalid complex name. You can't save without one.\r\n", CH);
                houseedit_display_complex_edit_menu(d);
                return;
              }

              bf::path new_save_dir = global_housing_dir / std::string(name_without_color);

              if (bf::exists(new_save_dir)) {
                send_to_char("There's already a directory that matches your new complex. You'll need to change its name.\r\n", CH);
                houseedit_display_complex_edit_menu(d);
                return;
              }

              COMPLEX->set_base_directory(new_save_dir);
            }

            // Log.
            mudlog_vfprintf(CH, LOG_WIZLOG, "%s wrote new complex %s (%ld).",
                            GET_CHAR_NAME(CH),
                            COMPLEX->get_name(),
                            COMPLEX->get_landlord_vnum());

            // Add to global list.
            global_apartment_complexes.push_back(COMPLEX);
            sort(global_apartment_complexes.begin(), global_apartment_complexes.end(), apartment_complex_sort_func);

            // Enact our landlord spec changes.
            rnum_t rnum = real_mobile(COMPLEX->get_landlord_vnum());
            if (rnum >= 0) {
              if (mob_index[rnum].sfunc) {
                log_vfprintf("SYSERR: Assigning too many specs to mob #%d. Losing one.", COMPLEX->get_landlord_vnum());
              }
              mob_index[rnum].sfunc = mob_index[rnum].func;
              mob_index[rnum].func = landlord_spec;
            }

            // Write to disk.
            COMPLEX->save();

            // Null edit_complex so it's not deleted later.
            COMPLEX = NULL;
          }
          // Fall through.
        } else {
          send_to_char("OK, discarding changes.\r\n", CH);
        }

        delete COMPLEX;
        COMPLEX = NULL;
        d->edit_complex_original = NULL;
        STATE(d) = CON_PLAYING;
      }
      else {
        send_to_char("Sorry, that's not an option.\r\n", CH);
        houseedit_display_complex_edit_menu(d);
      }
      break;
    case HOUSEEDIT_COMPLEX_NAME:
      // Length constraints.
      if (strlen(arg) < 5 || strlen(arg) > 40) {
        send_to_char("Name must be between 5 and 40 characters. Try again: ", CH);
        return;
      }

      // No color allowed.
      if (strcmp(arg, get_string_after_color_code_removal(arg, NULL))) {
        send_to_char("Complex names can't contain color codes. Try again: ", CH);
        return;
      }

      // Must be a valid filename.
      if (!string_is_valid_for_paths(arg)) {
        send_to_char("Complex names can only contain letters, numbers, and certain punctuation. Try again: ", CH);
        return;
      }

      // Enforce uniqueness.
      for (auto &complex : global_apartment_complexes) {
        if (complex != d->edit_complex_original && !strcmp(arg, complex->get_name())) {
          send_to_char("That complex name is already in use. Please choose another.\r\n", CH);
          houseedit_display_complex_edit_menu(d);
          return;
        }
      }

      COMPLEX->set_name(arg);
      houseedit_display_complex_edit_menu(d);
      break;
    case HOUSEEDIT_COMPLEX_LANDLORD:
      {
        vnum_t vnum = parsed_long;
        rnum_t rnum = real_mobile(vnum);

        if (rnum < 0) {
          send_to_char("That's not a valid vnum.\r\n", CH);
          houseedit_display_complex_edit_menu(d);
          return;
        }

        // Refuse to double up on landlord specs (skip this check if we're setting the landlord to match our original one)
        if (!d->edit_complex_original || d->edit_complex_original->get_landlord_vnum() != vnum) {
          if (mob_index[rnum].sfunc == landlord_spec || mob_index[rnum].func == landlord_spec) {
            send_to_char("That vnum is already in use as a landlord.\r\n", CH);
            houseedit_display_complex_edit_menu(d);
            return;
          }
        }

        // Set the landlord vnum (additional error checking in this function)
        if (!COMPLEX->set_landlord_vnum(vnum, FALSE)) {
          send_to_char("That's not an acceptable landlord vnum.\r\n", CH);
          houseedit_display_complex_edit_menu(d);
          return;
        }

        houseedit_display_complex_edit_menu(d);
      }
      break;
    case HOUSEEDIT_COMPLEX_EDITORS:
      {
        idnum_t idnum = parsed_long;

        // Zero means quit.
        if (*arg == '0') {
          houseedit_display_complex_edit_menu(d);
          return;
        }

        // Otherwise, we derive their idnum from the entered name.
        if ((idnum = get_player_id(arg)) <= 0) {
          send_to_char("That's not a valid player name.\r\n", CH);
          houseedit_display_complex_edit_menu(d);
          return;
        }

        {
          int rank = get_player_rank(idnum);

          // They must be a builder.
          if (rank < LVL_BUILDER) {
            send_to_char("You must specify a staff member.\r\n", CH);
            houseedit_display_complex_edit_menu(d);
            return;
          }

          // They can't be high-level staff.
          if (rank >= MIN_LEVEL_TO_IGNORE_HOUSEEDIT_EDITOR_STATUS) {
            send_to_char("There's no need to make them an editor-- they have global privs.\r\n", CH);
            houseedit_display_complex_edit_menu(d);
            return;
          }
        }

        COMPLEX->toggle_editor(idnum);

        send_to_char(CH, "The editor set is now %s. Enter a name to add/remove, or 0 to quit: ", COMPLEX->list_editors());
        return;
      }
      break;
    default:
      mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: Unknown edit state %d in houseedit_complex_parse()! Restoring to main menu.", d->edit_mode);
      houseedit_display_complex_edit_menu(d);
      break;
  }
}

#undef COMPLEX
