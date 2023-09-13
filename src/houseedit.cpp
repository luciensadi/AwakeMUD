#include "awake.hpp"
#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "redit.hpp"
#include "newhouse.hpp"
#include "houseedit_complex.hpp"
#include "houseedit_apartment.hpp"

extern void ASSIGNMOB(long mob, SPECIAL(fname));

void houseedit_import_from_old_files(struct char_data *ch, bool nuke_and_pave=FALSE);
void houseedit_reload(struct char_data *ch, const char *filename);
void houseedit_trueup_lifestyles_to_rent(struct char_data *ch);
int copy_old_file_into_subroom_if_it_exists(bf::path old_file, ApartmentRoom *subroom, bool is_house_file);

#define HED_NUKEANDPAVE TRUE
#define HED_IMPORT FALSE

ACMD(do_houseedit) {
  char mode[100], func[100];
  char *mode_remainder = one_argument(argument, mode);
  char *func_remainder = one_argument(mode_remainder, func);

  if (is_abbrev(mode, "nukeandpave")) {
    // Completely blow away all of our existing apartments and complexes and rebuild from old housing files. DESTRUCTIVE.
    FAILURE_CASE(GET_LEVEL(ch) < LVL_PRESIDENT, "You're not erudite enough to do that.");
    FAILURE_CASE(str_cmp(func, "hurtmedaddy"), "To ^RBLOW AWAY ALL EXISTING APARTMENT COMPLEXES^n and create them fresh from old files, type HOUSEEDIT NUKEANDPAVE HURTMEDADDY.");

    houseedit_import_from_old_files(ch, HED_NUKEANDPAVE);
    return;
  }

  if (is_abbrev(mode, "import")) {
    // Destroy current room contents, then assign owners and storage contents from old files. You probably want to run this on the live port.
    FAILURE_CASE(GET_LEVEL(ch) < LVL_PRESIDENT, "You're not erudite enough to do that.");
    FAILURE_CASE(str_cmp(func, "confirm"), "To blow away existing apartment CONTENTS and LEASES and load from old files, type HOUSEEDIT IMPORT CONFIRM.");

    houseedit_import_from_old_files(ch, HED_IMPORT);
    return;
  }

  if (is_abbrev(mode, "trueup")) {
    // Recalculate lifestyle settings for all rooms based on their current rent amounts.
    FAILURE_CASE(GET_LEVEL(ch) < LVL_PRESIDENT, "You're not erudite enough to do that.");
    FAILURE_CASE(str_cmp(func, "confirm"), "To true-up apartment lifestyle settings to match their current rent values, type HOUSEEDIT TRUEUP CONFIRM.");

    houseedit_trueup_lifestyles_to_rent(ch);
    return;
  }

  if (is_abbrev(mode, "reload")) {
    // Reload the storage for the subroom you're currently standing in.
    FAILURE_CASE(GET_LEVEL(ch) < LVL_EXECUTIVE, "You're not erudite enough to do that.");
    FAILURE_CASE(!ch->in_room || !GET_APARTMENT_SUBROOM(ch->in_room), "You must be standing in an apartment for that.");

    houseedit_reload(ch, func_remainder);
    return;
  }

  if (is_abbrev(mode, "complex")) {
    // List the complexes in game.
    if (is_abbrev(func, "list")) {
      houseedit_list_complexes(ch, func_remainder);
      return;
    }

    // Show details about a specific complex.
    if (is_abbrev(func, "show")) {
      houseedit_show_complex(ch, func_remainder);
      return;
    }

    // Create a new complex.
    if (is_abbrev(func, "create")) {
      FAILURE_CASE(!PLR_FLAGGED(ch, PLR_OLC) && !access_level(ch, LVL_PRESIDENT), YOU_NEED_OLC_FOR_THAT);

      houseedit_create_complex(ch);
      return;
    }

    // Delete an existing empty complex.
    if (is_abbrev(func, "delete")) {
      FAILURE_CASE(!PLR_FLAGGED(ch, PLR_OLC) && !access_level(ch, LVL_PRESIDENT), YOU_NEED_OLC_FOR_THAT);
      FAILURE_CASE(GET_LEVEL(ch) < LVL_ADMIN, "You're not erudite enough to do that.");

      houseedit_delete_complex(ch, func_remainder);
      return;
    }

    // Edit an existing complex.
    if (is_abbrev(func, "edit")) {
      FAILURE_CASE(!PLR_FLAGGED(ch, PLR_OLC) && !access_level(ch, LVL_PRESIDENT), YOU_NEED_OLC_FOR_THAT);

      houseedit_edit_existing_complex(ch, func_remainder);
      return;
    }

    send_to_char("Valid modes are COMPLEX LIST / CREATE / DELETE / EDIT.\r\n", ch);
    return;
  }

  else if (is_abbrev(mode, "apartment")) {
    // List existing apartments in the given complex.
    if (is_abbrev(func, "list")) {
      houseedit_list_apartments(ch, func_remainder);
      return;
    }

    // Show details about a specific apartment.
    if (is_abbrev(func, "show")) {
      houseedit_show_apartment(ch, func_remainder);
      return;
    }

    // Create a new apartment in the complex you're standing in, or the named one provided
    if (is_abbrev(func, "create")) {
      FAILURE_CASE(!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC), YOU_NEED_OLC_FOR_THAT);

      houseedit_create_apartment(ch, func_remainder);
      return;
    }

    // Delete an empty, non-leased apartment you're standing in, or one matching the full name provided.
    if (is_abbrev(func, "delete")) {
      FAILURE_CASE(!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC), YOU_NEED_OLC_FOR_THAT);
      FAILURE_CASE(GET_LEVEL(ch) < LVL_ADMIN, "You're not erudite enough to do that.");

      houseedit_delete_apartment(ch, func_remainder);
      return;
    }

    // Edit the apartment you're standing in, or one matching the full name provided.
    if (is_abbrev(func, "edit")) {
      FAILURE_CASE(!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC), YOU_NEED_OLC_FOR_THAT);

      houseedit_edit_apartment(ch, func_remainder);
      return;
    }

    send_to_char("Valid modes are APARTMENT LIST / CREATE / DELETE / EDIT.\r\n", ch);
    return;
  }

  send_to_char(ch, "Valid modes are IMPORT, RELOAD, COMPLEX, APARTMENT.\r\n");
  return;
}

// Overwrite this room's storage with what's on disk.
void houseedit_reload(struct char_data *ch, const char *filename) {
  FAILURE_CASE(!ch->in_room || !GET_APARTMENT_SUBROOM(ch->in_room), "You must be standing in an apartment for that.");
  FAILURE_CASE(!filename || !*filename, "Syntax: ^WHOUSEEDIT RELOAD <filename to load from>^n");

  // Sanitize filename.
  for (const char *c = filename; *c; c++)
    FAILURE_CASE(!string_is_valid_for_paths(filename), "That's not a filename. You must specify a filename that exists at this subroom's base directory.");

  //  Verify that file exists.
  bf::path new_path = GET_APARTMENT_SUBROOM(ch->in_room)->get_base_directory() / filename;
  if (!exists(new_path)) {
    send_to_char(ch, "There is no file at path '%s'.\r\n", new_path.string().c_str());
    return;
  }

  // Log and load.
  mudlog_vfprintf(ch, LOG_SYSLOG, "House reload for %s started by %s.", GET_APARTMENT_SUBROOM(ch->in_room)->get_full_name(), GET_CHAR_NAME(ch));
  GET_APARTMENT_SUBROOM(ch->in_room)->load_storage_from_specified_path(new_path);
  mudlog("House reload completed.", ch, LOG_SYSLOG, TRUE);
}

// Load old house files, parse, and transfer to new format.
void houseedit_import_from_old_files(struct char_data *ch, bool nuke_and_pave) {
  int old_house_lifestyle_multiplier[] = { 1, 3, 10, 25 };

  mudlog_vfprintf(ch, LOG_SYSLOG, "House %s started by %s.", nuke_and_pave ? "nuke-and-pave" : "import", GET_CHAR_NAME(ch));

  // If we're nuking, we'll need a new set of apartment complexes to create. This is left untouched in standard import mode.
  std::vector<ApartmentComplex*> read_apartment_complexes = {};

  // Read the old house control file. Ripped most of this code straight from house.cpp.
  FILE *fl;
  int num_complexes;
  char line[256], storage_file_name[256];
  bf::path old_house_directory("house");
  bf::path old_storage_directory("storage");
  bf::path parsed_storage_directory = old_storage_directory / "loaded-into-housing-subroom";

  // Failure case: The file does not exist.
  if (!(fl = fopen(HCONTROL_FILE, "r+b"))) {
    log("House control file does not exist.");
    send_to_char(ch, "Operation failed: Old house control file does not exist at %s." HCONTROL_FILE);
    return;
  }

  // Failure case: Malformed initial line (number of complexes).
  if (!get_line(fl, line) || sscanf(line, "%d", &num_complexes) != 1) {
    log("Error at beginning of house control file.");
    send_to_char(ch, "Operation failed: Old house control file (%s) is malformed.", HCONTROL_FILE);
    fclose(fl);
    return;
  }

  // Go through each apartment complex entry.
  for (int i = 0; i < num_complexes; i++) {
    idnum_t owner;
    time_t paid_until;
    vnum_t landlord_vnum, house_vnum, key_vnum, atrium;
    char name[20];
    int basecost, num_rooms, lifestyle, atrium_dir;

    // Precondition: Line must not be malformed.
    get_line(fl, line);
    if (sscanf(line, "%ld %s %d %d", &landlord_vnum, name, &basecost, &num_rooms) != 4) {
      mudlog_vfprintf(ch, LOG_SYSLOG, "Format error in old house control file landlord #%d.", i);
      send_to_char(ch, "Operation failed: Old house control file (%s) is malformed.", HCONTROL_FILE);
      fclose(fl);
      return;
    }

    // Precondition: Landlord must match an existing NPC.
    rnum_t landlord_rnum = real_mobile(landlord_vnum);
    if (landlord_rnum < 0) {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Old house control file landlord vnum %ld does not match up with a real NPC.", landlord_vnum);
      send_to_char(ch, "Operation failed: Old house control file (%s) is malformed.", HCONTROL_FILE);
      fclose(fl);
      return;
    }

    ApartmentComplex *complex = NULL;

    if (nuke_and_pave) {
      // Create a complex to represent this one.
      mudlog_vfprintf(ch, LOG_SYSLOG, "Loaded complex %s (landlord %ld).", name, landlord_vnum);
      complex = new ApartmentComplex(landlord_vnum);
      read_apartment_complexes.push_back(complex);

      // Assign our landlord spec.
      ASSIGNMOB(landlord_vnum, landlord_spec);

      // Write our complex to ensure the directory exists.
      complex->save();
    } else {
      // Find the complex that represents this one.
      for (auto &cplx : global_apartment_complexes) {
        if (cplx->get_landlord_vnum() == landlord_vnum) {
          log_vfprintf("Found existing apartment complex for %s (%ld): %s.", name, landlord_vnum, cplx->get_name());
          complex = cplx;
        }
      }
      if (!complex) {
        send_to_char(ch, "Well, shit. No apartment complex exists for %s (landlord %ld). You're gonna have to do some manual repair work.\r\n", name, landlord_vnum);
        return;
      }
    }

    // Go through each apartment in the apartment complex entry.
    for (int x = 0; x < num_rooms; x++) {
      int unused;

      get_line(fl, line);
      if (sscanf(line, "%ld %ld %d %d %s %ld %d %ld", &house_vnum, &key_vnum, &atrium_dir, &lifestyle, name,
                 &owner, &unused, &paid_until) != 8) {
        mudlog_vfprintf(ch, LOG_SYSLOG, "Format error in old house control file's landlord #%ld room #%d.", landlord_vnum, x);
        send_to_char(ch, "Operation failed: Old house control file (%s) is malformed.", HCONTROL_FILE);
        fclose(fl);
        return;
      }

      rnum_t house_rnum = real_room(house_vnum);

      if (house_rnum < 0) {
        mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Old house control file's house vnum %ld does not match up with a real room.", house_vnum);
        send_to_char(ch, "Operation failed: Old house control file (%s) is malformed.", HCONTROL_FILE);
        fclose(fl);
        return;
      }

      // Attempt to map the atrium.
      if (!EXIT2(&world[house_rnum], atrium_dir) || !EXIT2(&world[house_rnum], atrium_dir)->to_room) {
        mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Old house control file's house vnum %ld's atrium exit does not exist.", house_vnum);
        send_to_char(ch, "Operation failed: Old house control file (%s) is malformed.", HCONTROL_FILE);
        fclose(fl);
        return;
      }
      atrium = GET_ROOM_VNUM(EXIT2(&world[house_rnum], atrium_dir)->to_room);

      Apartment *apartment = NULL;
      ApartmentRoom *subroom = NULL;
      if (nuke_and_pave) {
        // Create an apartment to represent this.
        apartment = new Apartment(complex, name, key_vnum, atrium, lifestyle, owner, paid_until);
        mudlog_vfprintf(ch, LOG_SYSLOG, "Loading apartment %s (loc %ld, key %ld, atrium %ld, lifestyle %d, owner %ld, paid %ld).",
                        name, house_vnum, key_vnum, atrium, lifestyle, owner, paid_until);

        // Calculate and apply rent.
        int rent = basecost * old_house_lifestyle_multiplier[lifestyle];
        apartment->set_rent(rent);

        // Write our apartment to ensure the directory exists.
        apartment->save_base_info();

        // Create a room for its entry and save it to create its directory.
        subroom = new ApartmentRoom(apartment, &world[house_rnum]);
        subroom->save_info();

        // Apply it to the world.
        world[house_rnum].apartment = apartment;
        world[house_rnum].apartment_room = subroom;

        // Add our new room to our apartment.
        apartment->add_room(subroom);

        // Add our new apartment to our complex.
        complex->add_apartment(apartment);
      } else {
        // Find the existing apartment in this complex that matches the selected one.
        for (auto &apt : complex->get_apartments()) {
          for (auto &room : apt->get_rooms()) {
            if (room->get_world_room() == &world[house_rnum]) {
              apartment = apt;
              subroom = room;
              break;
            }
          }
        }

        if (!subroom) {
          send_to_char(ch, "Ah, crapbaskets. No existing apartment room was found to match %s's %ld (landlord %ld). You're gonna have to do some manual repair work.\r\n", 
                      name, GET_ROOM_VNUM(&world[house_rnum]), landlord_vnum);
          return;
        }
      }

      // Clone the current desc as a decoration, provided no decoration already exists.
      if (!subroom->get_decoration() && *(world[house_rnum].description)) {
        subroom->set_decoration(world[house_rnum].description);
        subroom->save_decoration();
      }

      // Set our owner and lease time.
      if (owner == 0 || paid_until > time(0)) {
        apartment->set_owner(owner);
        apartment->set_paid_until(paid_until);

        // Look for the old house file. If it exists, clobber existing contents and load this in.
        snprintf(storage_file_name, sizeof(storage_file_name), "%ld.house", house_vnum);
        bf::path original_save_file = old_house_directory / storage_file_name;

        // Load our guests from the old file.
        apartment->load_guests_from_old_house_file(original_save_file.string().c_str());
        // Load our contents from the old file.
        copy_old_file_into_subroom_if_it_exists(original_save_file, subroom, TRUE);
      } else {
        mudlog_vfprintf(ch, LOG_SYSLOG, "NOT loading guests / contents / etc from old house file %ld.house: Lease is not valid.", house_vnum);
      }

      // Look for any associated storage rooms. If they exist, merge them into the new house structure.
      for (auto &room : apartment->get_rooms()) {
        struct room_data *world_room = room->get_world_room();

        if (world_room && ROOM_FLAGGED(world_room, ROOM_STORAGE)) {
          // Room exists and is a storage room. Load it up.
          bf::path original_save_file = old_storage_directory / vnum_to_string(house_vnum);
          copy_old_file_into_subroom_if_it_exists(original_save_file, subroom, FALSE);

          // Strip the storage flag from the room.
#ifdef IS_BUILDPORT
          mudlog_vfprintf(ch, LOG_SYSLOG, "Refusing to remove ROOM_STORAGE flag from apartment subroom %ld: We're on the buildport.", GET_ROOM_VNUM(world_room));
#else
          ROOM_FLAGS(world_room).RemoveBit(ROOM_STORAGE);
#endif

          // Save the removal of the storage flag.
          int zone_idx = get_zone_index_number_from_vnum(GET_ROOM_VNUM(world_room));
          if (zone_idx < 0) {
            mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Unable to derive valid zone index from EXISTING room %ld. Stripping storage flag failed to save, WILL CAUSE ITEM DUPLICATION ON LOAD.", GET_ROOM_VNUM(world_room));
          } else {
            write_world_to_disk(zone_table[zone_idx].number);
          }
        }
      }
    }
  }

  fclose(fl);

  if (nuke_and_pave) {
    // Add our complex list to our global one. If there are overlaps, complain loudly.
    for (auto *our_complex : read_apartment_complexes) {
      for (auto *existing_complex : global_apartment_complexes) {
        if (!str_cmp(our_complex->get_name(), existing_complex->get_name())) {
          mudlog_vfprintf(ch, LOG_SYSLOG, "YOU DONE GOOFED: New complex is OVERWRITING existing complex %s!", existing_complex->get_name());
        }
      }
      global_apartment_complexes.push_back(our_complex);
    }
  }

  mudlog_vfprintf(ch, LOG_SYSLOG, "House %s completed.", nuke_and_pave ? "nuke-and-pave" : "import");
}

#undef HED_IMPORT
#undef HED_NUKEPANDPAVE

void houseedit_trueup_lifestyles_to_rent(struct char_data *ch) {
  mudlog_vfprintf(ch, LOG_SYSLOG, "Housing lifestyle true-up initiated by %s.", GET_CHAR_NAME(ch));
  for (auto *complex : global_apartment_complexes) {
    for (auto *apartment : complex->get_apartments()) {
      for (int lifestyle = NUM_LIFESTYLES - 1; lifestyle >= 0; lifestyle--) {
        if (apartment->get_rent_cost() >= lifestyles[lifestyle].monthly_cost_min) {
          if (apartment->get_lifestyle() != lifestyle) {
            mudlog_vfprintf(ch, LOG_SYSLOG, "Updating lifestyle of %s from %s to %s (rent: %d).",
                            apartment->get_full_name(),
                            lifestyles[apartment->get_lifestyle()].name,
                            lifestyles[lifestyle].name,
                            apartment->get_rent_cost());
            apartment->set_lifestyle(lifestyle);
            apartment->save_base_info();
          }
          break;
        }
      }
    }
  }
  mudlog_vfprintf(ch, LOG_SYSLOG, "Housing lifestyle true-up by %s complete.", GET_CHAR_NAME(ch));
}

// Loads an old file into a subroom. Returns 0 on success, anything else on failure.
int copy_old_file_into_subroom_if_it_exists(bf::path old_file, ApartmentRoom *subroom, bool is_house_file) {
  if (old_file.empty()) {
    mudlog("SYSERR: Received EMPTY old_file path to copy_old_file_into_subroom()!", NULL, LOG_SYSLOG, TRUE);
    return 1;
  }

  if (!subroom) {
    mudlog("SYSERR: Received NULL subroom to copy_old_file_into_subroom()!", NULL, LOG_SYSLOG, TRUE);
    return 2;
  }

  if (!bf::exists(old_file)) {
    // This not an error case, we allow non-existant files to be loaded here.
    return 0;
  }

  mudlog_vfprintf(NULL, LOG_SYSLOG, "Transferring %s storage for subroom %ld.", is_house_file ? "house-file" : "storage-file", subroom->get_vnum());

  // Compose our new location, then transfer the existing file to that location.
  bf::path new_save_file = subroom->get_base_directory() / "storage";
#ifdef USE_OLD_BOOST
  bf::copy_file(old_file, new_save_file, bf::copy_option::overwrite_if_exists);
#else
  bf::copy_file(old_file, new_save_file, bf::copy_options::overwrite_existing);
#endif

  // Immediately load from the newly-transferred file so that we don't lose room contents in a room save.
  subroom->load_storage_from_specified_path(new_save_file);

  // All is good.
  return 0;
}