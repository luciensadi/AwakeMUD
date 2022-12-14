#include "awake.hpp"
#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "newhouse.hpp"
#include "houseedit_complex.hpp"
#include "houseedit_apartment.hpp"

extern void ASSIGNMOB(long mob, SPECIAL(fname));

void houseedit_import(struct char_data *ch);
void houseedit_reload(struct char_data *ch, const char *filename);

ACMD(do_houseedit) {
  char mode[100], func[100];
  char *mode_remainder = one_argument(argument, mode);
  char *func_remainder = one_argument(mode_remainder, func);

  if (is_abbrev(mode, "import")) {
    // Import apartments from old format to new format. Destructive!
    FAILURE_CASE(GET_LEVEL(ch) < LVL_PRESIDENT, "You're not erudite enough to do that.");
    FAILURE_CASE(str_cmp(func, "confirm"), "To blow away existing apartments and load from old files, HOUSEEDIT IMPORT CONFIRM.");

    houseedit_import(ch);
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
void houseedit_import(struct char_data *ch) {
  int old_house_lifestyle_multiplier[] = { 1, 3, 10, 25 };

  mudlog_vfprintf(ch, LOG_SYSLOG, "House import started by %s.", GET_CHAR_NAME(ch));

  std::vector<ApartmentComplex*> read_apartment_complexes = {};

  // Read the old house control file. Ripped most of this code straight from house.cpp.
  FILE *fl;
  int num_complexes;
  char line[256], storage_file_name[256];
  bf::path old_house_directory("house");

  if (!(fl = fopen(HCONTROL_FILE, "r+b"))) {
    log("House control file does not exist.");
    return;
  }

  if (!get_line(fl, line) || sscanf(line, "%d", &num_complexes) != 1) {
    log("Error at beginning of house control file.");
    return;
  }

  // Go through each apartment complex entry.
  for (int i = 0; i < num_complexes; i++) {
    idnum_t owner;
    time_t paid_until;
    vnum_t landlord_vnum, house_vnum, key_vnum, atrium;
    char name[20];
    int basecost, num_rooms, lifestyle, atrium_dir;

    get_line(fl, line);
    if (sscanf(line, "%ld %s %d %d", &landlord_vnum, name, &basecost, &num_rooms) != 4) {
      mudlog_vfprintf(ch, LOG_SYSLOG, "Format error in landlord #%d. Terminating.", i);
      fclose(fl);
      return;
    }

    rnum_t landlord_rnum = real_mobile(landlord_vnum);
    if (landlord_rnum < 0) {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Landlord vnum %ld does not match up with a real NPC. Terminating.", landlord_vnum);
      fclose(fl);
      return;
    }

    // Create a complex to represent this one.
    mudlog_vfprintf(ch, LOG_SYSLOG, "Loaded complex %s (landlord %ld).", name, landlord_vnum);
    ApartmentComplex *complex = new ApartmentComplex(landlord_vnum);
    read_apartment_complexes.push_back(complex);

    // Assign our landlord spec.
    ASSIGNMOB(landlord_vnum, landlord_spec);

    // Write our complex to ensure the directory exists.
    complex->save();

    // Go through each apartment in the apartment complex entry.
    for (int x = 0; x < num_rooms; x++) {
      int unused;

      get_line(fl, line);
      if (sscanf(line, "%ld %ld %d %d %s %ld %d %ld", &house_vnum, &key_vnum, &atrium_dir, &lifestyle, name,
                 &owner, &unused, &paid_until) != 8) {
        mudlog_vfprintf(ch, LOG_SYSLOG, "Format error in landlord #%ld room #%d. Terminating.", landlord_vnum, x);
        return;
      }

      rnum_t house_rnum = real_room(house_vnum);

      if (house_rnum < 0) {
        mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: House vnum %ld does not match up with a real room. Terminating.", house_vnum);
        fclose(fl);
        return;
      }

      // Attempt to map the atrium.
      if (!EXIT2(&world[house_rnum], atrium_dir) || !EXIT2(&world[house_rnum], atrium_dir)->to_room) {
        mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: House vnum %ld's atrium exit does not exist. Terminating.", house_vnum);
        fclose(fl);
        return;
      }
      atrium = GET_ROOM_VNUM(EXIT2(&world[house_rnum], atrium_dir)->to_room);

      // Create an apartment to represent this.
      Apartment *apartment = new Apartment(complex, name, key_vnum, atrium, lifestyle, owner, paid_until);
      mudlog_vfprintf(ch, LOG_SYSLOG, "Loading apartment %s (loc %ld, key %ld, atrium %ld, lifestyle %d, owner %ld, paid %ld).",
                      name, house_vnum, key_vnum, atrium, lifestyle, owner, paid_until);

      // Calculate and apply rent.
      int rent = basecost * old_house_lifestyle_multiplier[lifestyle];
      apartment->set_rent(rent);

      // Write our apartment to ensure the directory exists.
      apartment->save_base_info();

      // Create a room for its entry and save it to create its directory.
      ApartmentRoom *room = new ApartmentRoom(apartment, &world[house_rnum]);
      room->save_info();

      // Apply it to the world.
      world[house_rnum].apartment = apartment;
      world[house_rnum].apartment_room = room;

      // Clone the current desc as a decoration.
      if (*(world[house_rnum].description)) {
        room->set_decoration(world[house_rnum].description);
        room->save_decoration();
      }

      // Check to see if the old storage file exists.
      snprintf(storage_file_name, sizeof(storage_file_name), "%ld.house", house_vnum);
      bf::path original_save_file = old_house_directory / storage_file_name;
      if (bf::exists(original_save_file)) {
        mudlog_vfprintf(ch, LOG_SYSLOG, "Transferring storage for subroom %ld.", house_vnum);
        // It does exist-- clone it over, then immediately load it so we don't lose contents to an overwrite.
        bf::path new_save_file = room->get_base_directory() / "storage";
        bf::copy_file(original_save_file, new_save_file);
        room->load_storage_from_specified_path(new_save_file);
      } else {
        mudlog_vfprintf(ch, LOG_SYSLOG, "Subroom %ld had no storage.", house_vnum);
      }

      // Add our new room to our apartment.
      apartment->add_room(room);

      // Add our new apartment to our complex.
      complex->add_apartment(apartment);
    }
  }

  fclose(fl);

  // Add our complex list to our global one. If there are overlaps, complain loudly.
  for (auto *our_complex : read_apartment_complexes) {
    for (auto *existing_complex : global_apartment_complexes) {
      if (!str_cmp(our_complex->get_name(), existing_complex->get_name())) {
        mudlog_vfprintf(ch, LOG_SYSLOG, "YOU DONE GOOFED: New complex is OVERWRITING existing complex %s!", existing_complex->get_name());
      }
    }
    global_apartment_complexes.push_back(our_complex);
  }

  mudlog("House import completed.", ch, LOG_SYSLOG, TRUE);
}
