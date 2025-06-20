#include <fstream>
#include <iostream>
#include <limits.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
namespace bf = boost::filesystem;

#include "types.hpp"
#include "awake.hpp"
#include "structs.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "utils.hpp"
#include "playergroup_classes.hpp"
#include "handler.hpp"
#include "limits.hpp"
#include "interpreter.hpp"
#include "newmail.hpp"
#include "constants.hpp"
#include "file.hpp"
#include "vtable.hpp"
#include "newhouse.hpp"
#include "moderation.hpp"
#include "redit.hpp"
#include "vehicles.hpp"

#include "nlohmann/json.hpp"
using nlohmann::json;

#define COMPLEX_INFO_FILE_NAME      "complex_info.json"
#define APARTMENT_INFO_FILE_NAME    "apartment_info.json"
#define LEASE_INFO_FILE_NAME        "lease.json"
#define ROOM_INFO_FILE_NAME         "room_info.json"
#define DELETED_APARTMENTS_DIR_NAME "deleted-apartments"

#define MAX_ROOM_DECORATION_NAME_LEN  70

extern void ASSIGNMOB(long mob, SPECIAL(fname));
extern bool Storage_get_filename(vnum_t vnum, char *filename, int filename_size);
extern void Storage_save(const char *file_name, struct room_data *room);
extern bool player_is_dead_hardcore(long id);
extern bool House_load_storage(struct room_data *world_room, const char *filename);

SPECIAL(landlord_spec);

ACMD_DECLARE(do_say);

void remove_vehicles_from_apartment(struct room_data *room);
void write_json_file(bf::path path, json *contents);
void _json_parse_from_file(bf::path path, json &target);
ApartmentComplex *get_complex_headed_by_landlord(vnum_t vnum);

std::vector<ApartmentComplex*> global_apartment_complexes = {};

#ifdef USE_PRIVATE_CE_WORLD
const bf::path global_housing_dir = bf::system_complete("lib") / "housing2";
#else
const bf::path global_housing_dir = bf::system_complete("lib") / "housing";
#endif

// EVENTUALTODOs for pgroups:
// - Write pgroup log when a lease is broken.
// - When a pgroup has no members left, it should be auto-disabled.
// - if IS_BUILDPORT, find_pgroup must always return a group instance with the requested idnum, even if it doesn't exist
// - Add logic for mailing pgroup officers with apartment deletion warnings.
// - Verify that an apartment owned by non-buildport pgroup X is not permanently borked when loaded on the buildport and then transferred back to main

// TODO: Test apartment lease expiry (demonstrate clearing of stuff, etc)
// TODO: Test HCONTROL DESTROY on an apartment with stuff in it

ACMD(do_decorate) {
  FAILURE_CASE(IS_ASTRAL(ch), "You can't seem to touch the decorating supplies!");
  
  if (ch->in_veh) {
    FAILURE_CASE(!ch->in_veh->owner || ch->in_veh->owner != GET_IDNUM(ch), "You don't own this vehicle.");

    if ((ch->vfront && !ch->in_veh->decorate_front) || (!ch->vfront && !ch->in_veh->decorate_rear)) {
      FAILURE_CASE_PRINTF(GET_NUYEN(ch) < COST_TO_DECORATE_VEH, "You need %d nuyen on hand to cover the materials.", COST_TO_DECORATE_VEH);

      send_to_char(ch, "You spend %d nuyen to purchase decorating materials.\r\n", COST_TO_DECORATE_VEH);
      lose_nuyen(ch, COST_TO_DECORATE_VEH, NUYEN_OUTFLOW_DECORATING);
    }

    STATE(ch->desc) = CON_DECORATE_VEH;
    DELETE_D_STR_IF_EXTANT(ch->desc);
    INITIALIZE_NEW_D_STR(ch->desc);
    ch->desc->max_str = MAX_MESSAGE_LENGTH;
    ch->desc->mail_to = 0;
    return;
  }

  FAILURE_CASE(!ch->in_room || !GET_APARTMENT(ch->in_room), "You must be in an apartment or vehicle to decorate it.");
  FAILURE_CASE(!GET_APARTMENT(ch->in_room)->has_owner_privs(ch), "You must be the owner of this apartment to decorate it.")
  FAILURE_CASE(!GET_APARTMENT_SUBROOM(ch->in_room), "This apartment is bugged! Notify staff.");
  FAILURE_CASE(GET_APARTMENT(ch->in_room)->get_lifestyle() <= LIFESTYLE_SQUATTER && (GET_ROOM_VNUM(ch->in_room) < 6900 || GET_ROOM_VNUM(ch->in_room) > 6999),
               "Squats can't be redecorated.");
  
  // If they've specified an argument, use that to set the room's name.
  skip_spaces(&argument);
  if (*argument) {
    FAILURE_CASE_PRINTF(get_string_length_after_color_code_removal(argument, ch) > MAX_ROOM_DECORATION_NAME_LEN,
                        "You must specify a title of %d characters or fewer.", MAX_ROOM_DECORATION_NAME_LEN);
    
    // No offensive room names.
    if (check_for_banned_content(argument, ch, MODERATION_MODE_DESCRIPTIONS))
      return;

    GET_APARTMENT_SUBROOM(ch->in_room)->set_decorated_name(argument);
    send_to_char(ch, "OK, room name set to '%s'. Customize the description next.\r\n", GET_ROOM_NAME(ch->in_room));
  } else if (GET_APARTMENT_SUBROOM(ch->in_room)->get_decorated_name()) {
    send_to_char("Clearing the old decorated name (you must re-specify it with DECORATE <name> when editing if you want to keep it).\r\n", ch);
    GET_APARTMENT_SUBROOM(ch->in_room)->delete_decorated_name();
    GET_APARTMENT_SUBROOM(ch->in_room)->save_decoration();
  }

  if (!GET_APARTMENT_SUBROOM(ch->in_room)->get_decoration()) {
    FAILURE_CASE_PRINTF(GET_NUYEN(ch) < COST_TO_DECORATE_APT, "You need %d nuyen on hand to cover the materials.", COST_TO_DECORATE_APT);

    send_to_char(ch, "You spend %d nuyen to purchase new decorating materials.\r\n", COST_TO_DECORATE_APT);
    lose_nuyen(ch, COST_TO_DECORATE_APT, NUYEN_OUTFLOW_DECORATING);
  }

  PLR_FLAGS(ch).SetBit(PLR_WRITING);
  send_to_char("Welcome to the description editor! Enter your new room description.\r\n"
               "Terminate with @ on an empty line. You can also just enter @ right now to remove any existing decoration.\r\n"
               "Note that no formatting will be applied here, so indent as/if desired! (MUD-standard indent is 3 spaces.)\r\n", ch);
  act("$n starts to move things around the room.", TRUE, ch, 0, 0, TO_ROOM);
  STATE(ch->desc) = CON_DECORATE;
  DELETE_D_STR_IF_EXTANT(ch->desc);
  INITIALIZE_NEW_D_STR(ch->desc);
  ch->desc->max_str = MAX_MESSAGE_LENGTH;
  ch->desc->mail_to = 0;
}

/* The house command, used by mortal house owners to assign guests */
ACMD(do_house) {
  one_argument(argument, arg);

  FAILURE_CASE(!ch->in_room || !GET_APARTMENT(ch->in_room), "You must be in an apartment to set guests.");

  if (!GET_APARTMENT(ch->in_room)->has_owner_privs(ch)) {
    if (access_level(ch, LVL_FIXER)) {
      send_to_char("You use your staff powers to bypass the fact that you have no ownership stake here.\r\n", ch);
    } else {
      send_to_char("You're not an owner or landlord here.\r\n", ch);
      return;
    }
  }

  if (!*arg) {
    // Drop any expired guests.
    GET_APARTMENT(ch->in_room)->cleanup_deleted_guests();

    GET_APARTMENT(ch->in_room)->list_guests_to_char(ch);
    return;
  }

  // They're adding or deleting a guest. Look them up.
  idnum_t idnum = get_player_id(arg);

  FAILURE_CASE(idnum < 0, "No such player.");
  FAILURE_CASE(player_is_dead_hardcore(idnum), "Permanently-dead characters can't be added as guests.");
  FAILURE_CASE(idnum == GET_IDNUM(ch), "You can't add yourself as a guest.");

  if (GET_APARTMENT(ch->in_room)->delete_guest(idnum)) {
    send_to_char("Guest deleted.\r\n", ch);

    if (!GET_APARTMENT(ch->in_room)->has_owner_privs(ch)) {
      mudlog_vfprintf(ch, LOG_WIZLOG, "%s removed guest idnum %ld from %ld (%s) by staff fiat.", 
                      GET_CHAR_NAME(ch),
                      idnum,
                      GET_ROOM_VNUM(ch->in_room),
                      GET_ROOM_NAME(ch->in_room));
    }
  } else {
    GET_APARTMENT(ch->in_room)->add_guest(idnum);
    send_to_char("Guest added.\r\n", ch);

    if (!GET_APARTMENT(ch->in_room)->has_owner_privs(ch)) {
      mudlog_vfprintf(ch, LOG_WIZLOG, "%s added guest idnum %ld from %ld (%s) by staff fiat.", 
                      GET_CHAR_NAME(ch),
                      idnum,
                      GET_ROOM_VNUM(ch->in_room),
                      GET_ROOM_NAME(ch->in_room));
    }
  }
}

void load_apartment_complexes() {
  // Iterate over the contents of the lib/housing directory.
  if (!exists(global_housing_dir)) {
    log("WARNING: Unable to find lib/housing.");
    log_vfprintf("Rendered path: %s", global_housing_dir.c_str());
    exit(1);
  }

  log("Loading apartment complexes:");

  bf::directory_iterator end_itr; // default construction yields past-the-end
  for (bf::directory_iterator itr(global_housing_dir); itr != end_itr; ++itr) {
    if (is_directory(itr->status())) {
      bf::path filename = itr->path();
      log_vfprintf(" - Initializing apartment complex from file %s.", filename.c_str());
      ApartmentComplex *complex = new ApartmentComplex(filename);
      global_apartment_complexes.push_back(complex);
      log_vfprintf(" - Fully loaded %s.", complex->get_name());
      
      // Ensure it's been saved with the vnum instead of the name. Convert if not.
      if (atol(filename.filename().c_str()) <= 0) {
        bf::path new_name = filename.parent_path() / vnum_to_string(complex->get_landlord_vnum());
        log_vfprintf(" (Complex %s was saved in the old manner ('%s' is not a number), so I'll rename it to '%s'.)", 
                     complex->get_name(),
                     filename.filename().c_str(),
                     new_name.c_str());
        bf::rename(filename, new_name);
      }
    }
  }
  // Sort after fully loaded.
  sort(global_apartment_complexes.begin(), global_apartment_complexes.end(), apartment_complex_sort_func);
}

void save_all_apartments_and_storage_rooms() {
#ifdef IS_BUILDPORT
  log_vfprintf("Refusing to save all apartments and storage rooms: We're on the buildport.");
  return;
#endif

  for (auto &complex : global_apartment_complexes) {
    for (auto &apartment : complex->get_apartments()) {
      // Skip non-leased apartments.
      if (apartment->get_paid_until() == 0)
        continue;

#ifndef IS_BUILDPORT
      // Leased with no or invalid owner? Break the lease and bail.
      if (!apartment->has_owner() || !apartment->owner_is_valid()) {
        mudlog_vfprintf(NULL, LOG_GRIDLOG, "Breaking lease on %s: No owner, or owner is no longer valid.", apartment->get_full_name());
        apartment->break_lease();
        continue;
      }

      // Invalid lease? Skip it and bail. We just won't load this apartment's contents on next boot.
      if (apartment->get_paid_until() < time(0)) {
        // Previously, we broke the lease here, but that stopped arrears from working properly.
        // mudlog_vfprintf(NULL, LOG_GRIDLOG, "Skipping save on %s: Lease expired.", apartment->get_full_name());
        // apartment->break_lease();

        // Now, we just kick out anyone in the room and their vehicles.
        apartment->kick_out_everyone();
        continue;
      }
#endif

      // Otherwise, save all the rooms (save_storage logic will skip any that haven't been altered)
      for (auto &room : apartment->get_rooms()) {
        // log_vfprintf("Saving storage contents for %s's %ld.", apartment->get_full_name(), room->get_vnum());
        room->save_storage();
      }
    }
  }

  // Save all storage rooms.
  for (rnum_t x = 0; x <= top_of_world; x++) {
    struct room_data *room = &world[x];

    if (ROOM_FLAGGED(room, ROOM_STORAGE)) {
      char filename[100];

      // We only save rooms that have potentially had their contents modified since we last saw them.
      if (!room->dirty_bit && !room->people) {
        continue;
      }

      // Clear the dirty bit now that we've processed it.
      room->dirty_bit = FALSE;

      if (Storage_get_filename(GET_ROOM_VNUM(room), filename, sizeof(filename))) {
        Storage_save(filename, room);
      } else {
        mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Unable to save storage room %ld: Can't get filename!", GET_ROOM_VNUM(room));
      }
    }
  }
}

/*********** ApartmentComplex ************/
ApartmentComplex::ApartmentComplex() : display_name(str_dup("Unnamed Complex")) {}

ApartmentComplex::ApartmentComplex(vnum_t landlord) {
  landlord_vnum = landlord;

  rnum_t landlord_rnum = real_mobile(landlord);
  if (landlord_rnum < 0) {
    log_vfprintf("FATAL ERROR: Invalid landlord %ld!", landlord);
    exit(1);
  }

  char tmp[200];
  snprintf(tmp, sizeof(tmp), "%s's Housing Complex", GET_CHAR_NAME(&mob_proto[landlord_rnum]));

  display_name = str_dup(tmp);

  // Becomes something like '.../lib/housing/22608'
  base_directory = global_housing_dir / vnum_to_string(GET_MOB_VNUM(&mob_proto[landlord_rnum]));
  log_vfprintf("Newly-initialized AC w/ LL %ld's base_directory: %s", landlord_vnum, base_directory.c_str());
}

ApartmentComplex::ApartmentComplex(bf::path filename) :
  base_directory(filename)
{
  // Load info from <filename>/info
  {
    json base_info;
    _json_parse_from_file(base_directory / COMPLEX_INFO_FILE_NAME, base_info);

    display_name = str_dup(base_info["display_name"].get<std::string>().c_str());
    landlord_vnum = (vnum_t) base_info["landlord_vnum"].get<vnum_t>();
    editors = base_info["editors"].get<std::vector<idnum_t>>();

    if (real_mobile(landlord_vnum) < 0) {
      log_vfprintf("SYSERR: Landlord vnum %ld does not match up with a real NPC. Terminating.\r\n", landlord_vnum);
      exit(ERROR_CANNOT_RESOLVE_VNUM);
    }
    ASSIGNMOB(landlord_vnum, landlord_spec);

    log_vfprintf(" -- Loaded apartment complex %s with landlord %ld.", display_name, landlord_vnum);
  }

  // Load rooms
  {
    bf::directory_iterator end_itr; // default construction yields past-the-end
    for (bf::directory_iterator itr(base_directory); itr != end_itr; ++itr) {
      if (is_directory(itr->status())) {
        bf::path room_path = itr->path();

#define ROOM_NAME room_path.filename().c_str()
        if (!str_cmp(ROOM_NAME, DELETED_APARTMENTS_DIR_NAME))
          continue;

        log_vfprintf(" --- Initializing apartment %s.", ROOM_NAME);
#undef ROOM_NAME

        Apartment *apartment = new Apartment(this, room_path);
        apartments.push_back(apartment);
        log_vfprintf(" --- Fully loaded %s.", apartment->get_name());
      }
    }
    sort(apartments.begin(), apartments.end(), apartment_sort_func);
  }
}

ApartmentComplex::~ApartmentComplex() {
  // Remove us from the global apartment complex list.
  auto it = find(global_apartment_complexes.begin(), global_apartment_complexes.end(), this);
  if (it != global_apartment_complexes.end())
    global_apartment_complexes.erase(it);

  // Clear our strings.
  delete [] display_name;
}

void ApartmentComplex::ensure_base_directory_exists() {
  if (!bf::exists(base_directory)) {
    log_vfprintf("apartmentcomplex::ensure_base_directory_exists(): Not found. Creating base directory %s.", base_directory.c_str());
    bf::create_directory(base_directory);
  }
}

void ApartmentComplex::save() {
  // Ensure our base directory exists.
  ensure_base_directory_exists();

  // Compose the info file.
  json base_info;

  base_info["display_name"] = std::string(display_name);
  base_info["landlord_vnum"] = landlord_vnum;
  base_info["editors"] = editors;

  // Write it out.
  write_json_file(base_directory / COMPLEX_INFO_FILE_NAME, &base_info);
}

void ApartmentComplex::mark_as_deleted() {
  char deleted_name[500];
  snprintf(deleted_name, sizeof(deleted_name), "%s-%ld", display_name, time(0));
  bf::rename(base_directory, bf::system_complete("deleted-housing") / deleted_name);
}

void ApartmentComplex::add_editor(idnum_t idnum) {
  // Error case.
  if (idnum <= 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Received invalid editor idnum %ld to add_editor() for %s.", idnum, display_name);
    return;
  }

  // Already there.
  if (find(editors.begin(), editors.end(), idnum) != editors.end()) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Not adding editor idnum %ld to %s: Already an editor.", idnum, display_name);
    return;
  }

  editors.push_back(idnum);
}

void ApartmentComplex::remove_editor(idnum_t idnum) {
  // Error case.
  if (idnum <= 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Received invalid editor idnum %ld to remove_editor() for %s.", idnum, display_name);
    return;
  }

  auto it = find(editors.begin(), editors.end(), idnum);

  // Not there.
  if (it == editors.end()) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Can't remove editor idnum %ld from %s: Not an editor.", idnum, display_name);
    return;
  }

  editors.erase(it);
}

void ApartmentComplex::toggle_editor(idnum_t idnum) {
  auto it = find(editors.begin(), editors.end(), idnum);

  if (it == editors.end())
    add_editor(idnum);
  else
    remove_editor(idnum);
}

const char *ApartmentComplex::list_editors() {
  static char buf[500];

  strlcpy(buf, "", sizeof(buf));

  bool printed_anything = FALSE;
  for (auto editor_id : editors) {
    if (get_player_rank(editor_id) < LVL_BUILDER)
      continue;

    const char *name = get_player_name(editor_id);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^C%s^n", printed_anything ? ", " : "", name);
    delete [] name;
    printed_anything = TRUE;
  }

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^cLevel %d+ staff^n", 
           printed_anything ? ", " : "", 
           MIN_LEVEL_TO_IGNORE_HOUSEEDIT_EDITOR_STATUS);

  return buf;
}

bool ApartmentComplex::can_houseedit_complex(struct char_data *ch) {
  if (GET_LEVEL(ch) >= MIN_LEVEL_TO_IGNORE_HOUSEEDIT_EDITOR_STATUS)
    return TRUE;

  if (find(editors.begin(), editors.end(), GET_IDNUM(ch)) != editors.end())
    return TRUE;

  return TRUE;
}

void ApartmentComplex::display_room_list_to_character(struct char_data *ch) {
  std::vector<Apartment*> available_apartments = {};

  if (apartments.empty()) {
    send_to_char(ch, "It doesn't look like there are any rooms to rent at all.\r\n");
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Found ZERO apartments in %s on list!", display_name);
    return;
  }

  for (auto &apartment : apartments) {
    if (!apartment->has_owner() || !apartment->owner_is_valid()) {
      available_apartments.push_back(apartment);
      continue;
    }
  }

  if (available_apartments.empty()) {
    send_to_char(ch, "It looks like all the rooms here have been claimed.\r\n");
    return;
  } else {
    sort(available_apartments.begin(), available_apartments.end(), apartment_sort_func);

    send_to_char(ch, "The following rooms are free: \r\n");

    if (!PRF_FLAGGED(ch, PRF_SCREENREADER)) {
      send_to_char(ch, "Name                           Lifestyle      Rooms   Garages  Price  \r\n");
      send_to_char(ch, "----------------------------   ------------   -----   -------  -------\r\n");
    }

    bool color_toggle = TRUE;
    for (auto &apartment : available_apartments) {
      int non_garage = apartment->rooms.size() - apartment->garages;

      if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
        send_to_char(ch, "%s (lifestyle %s, %ld room%s, of which %lu %s): %ld nuyen.\r\n",
                    apartment->name,
                    lifestyles[apartment->get_lifestyle()].name,
                    apartment->rooms.size(),
                    apartment->rooms.size() == 1 ? "" : "s",
                    apartment->garages,
                    apartment->garages == 1 ? "is a garage" : "are garages",
                    apartment->get_rent_cost());
      } else {
        send_to_char(ch, "%s%-28s%s   %-12s   %-5d   %-7lu  %ld\r\n",
                    color_toggle ? "^W" : "^w",
                    apartment->name,
                    color_toggle ? "^W" : "^w",
                    lifestyles[apartment->get_lifestyle()].name,
                    non_garage,
                    apartment->garages,
                    apartment->get_rent_cost());
      }
      color_toggle = !color_toggle;
    }

    list_owned_apartments_to_ch(ch);
  }
}

void ApartmentComplex::list_owned_apartments_to_ch(struct char_data *ch) {
  std::vector<Apartment*> owned_apartments = {};

  for (auto &apartment : apartments) {
    if (apartment->has_owner_privs(ch)) {
      owned_apartments.push_back(apartment);
    }
  }

  if (!owned_apartments.empty()) {
    sort(owned_apartments.begin(), owned_apartments.end(), apartment_sort_func);

    bool first_print = TRUE;
    send_to_char("\r\nYou are the owner of: ", ch);
    for (auto &apartment : owned_apartments) {
      send_to_char(ch, "%s^c%s^n%s", first_print ? "" : ", ", apartment->get_name(), apartment->is_garage_lifestyle() ? " (garage)" : "");
      first_print = FALSE;
    }
    send_to_char("\r\n", ch);
  }
}

bool ApartmentComplex::ch_already_rents_here(struct char_data *ch) {
  if (IS_NPC(ch))
    return FALSE;

  for (auto &apartment: apartments) {
    if (apartment->owned_by_player == GET_IDNUM(ch))
      return TRUE;
  }
  return FALSE;
}

void ApartmentComplex::clone_from(ApartmentComplex *source) {
  delete [] display_name;
  display_name = str_dup(source->display_name);

  if (source->landlord_vnum != landlord_vnum) {
    rnum_t rnum;

    // Remove the spec from the old mob.
    if (landlord_vnum > 0 && (rnum = real_mobile(landlord_vnum)) >= 0) {
      if (mob_index[rnum].sfunc == landlord_spec)
        mob_index[rnum].sfunc = NULL;
      if (mob_index[rnum].func == landlord_spec)
        mob_index[rnum].func = mob_index[rnum].sfunc;
    }

    // Assign the spec to the new mob if it doesn't have it already.
    if (source->landlord_vnum > 0 && (rnum = real_mobile(source->landlord_vnum)) >= 0) {
      if (mob_index[rnum].sfunc != landlord_spec && mob_index[rnum].func != landlord_spec) {
        if (mob_index[rnum].sfunc) {
          log_vfprintf("SYSERR: Assigning too many specs to mob #%ld. Losing one.", source->landlord_vnum);
        }
        mob_index[rnum].sfunc = mob_index[rnum].func;
        mob_index[rnum].func = landlord_spec;
      }
    }
  }

  landlord_vnum = source->landlord_vnum;

  base_directory = source->base_directory;
  log_vfprintf("Cloned AC w/ LL %ld's base_directory: %s", landlord_vnum, base_directory.c_str());

  editors.clear();
  for (auto idnum : source->editors) {
    editors.push_back(idnum);
  }

  // Create an empty temp vector and swap its contents with apartments, deallocating them.
  std::vector<Apartment *>().swap(apartments);
  for (auto &apartment : source->apartments) {
    Apartment *new_apt = new Apartment();
    new_apt->clone_from(apartment);
    new_apt->complex = this;
    apartments.push_back(new_apt);
  }
  sort(apartments.begin(), apartments.end(), apartment_sort_func);
}

bool ApartmentComplex::set_landlord_vnum(vnum_t vnum, bool perform_landlord_overlap_test) {
  rnum_t rnum = real_mobile(vnum);

  if (rnum < 0) {
    return FALSE;
  }

  // Refuse to assign landlord status to mobs that are already landlords.
  if (perform_landlord_overlap_test && (mob_index[rnum].sfunc == landlord_spec || mob_index[rnum].func == landlord_spec)) {
    return FALSE;
  }

  landlord_vnum = vnum;
  return TRUE;
}

bool ApartmentComplex::set_name(const char *name) {
  delete [] display_name;
  display_name = str_dup(name);
  return TRUE;
}

const char *ApartmentComplex::list_apartments__returns_new() {
  char result[MAX_STRING_LENGTH];
  result[0] = '\0';

  if (apartments.empty()) {
    return str_dup("n/a\r\n");
  }

  for (auto &apartment: apartments) {
    snprintf(ENDOF(result), sizeof(result) + strlen(result), "  - ^C%s^n @ ^c%ld^n (lifestyle ^c%s^n, ^c%ld^n room%s, of which ^c%ld^n %s): ^c%ld^n nuyen.\r\n",
             apartment->name,
             apartment->rooms.empty() ? -1 : apartment->rooms.front()->get_vnum(),
             lifestyles[apartment->get_lifestyle()].name,
             apartment->rooms.size(),
             apartment->rooms.size() == 1 ? "" : "s",
             apartment->garages,
             apartment->garages == 1 ? "is a garage" : "are garages",
             apartment->get_rent_cost());
  }

  return str_dup(result);
}

void ApartmentComplex::add_apartment(Apartment *apartment) {
  // It is an error to call this on an editing struct.
  if (apartment->is_editing_struct) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Attempted to add apartment %s to complex %s, but it is an editing struct instead of an actual apartment!",
                    apartment->get_name(),
                    get_name());
    return;
  }

  auto it = find(apartments.begin(), apartments.end(), apartment);
  if (it != apartments.end()) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Attempted to add apartment %s to complex %s, but it was already there (%s)!",
                    apartment->get_name(),
                    get_name(),
                    (*it)->get_full_name());
    return;
  }

  // Add us to the new complex.
  apartments.push_back(apartment);
  sort(apartments.begin(), apartments.end(), apartment_sort_func);
}

void ApartmentComplex::delete_apartment(Apartment *apartment) {
  // It is an error to call this on an editing struct.
  if (apartment->is_editing_struct) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Attempted to remove apartment %s from complex %s, but it is an editing struct instead of an actual apartment!",
                    apartment->get_name(),
                    get_name());
    return;
  }

  auto it = find(apartments.begin(), apartments.end(), apartment);
  if (it == apartments.end()) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Attempted to delete apartment %s from complex %s, but it wasn't part of it!",
                    apartment->get_name(),
                    get_name());
    return;
  }

  // The apartment destructor removes us from our apartments list, so no need to do more here.
  apartments.erase(it);
}

int ApartmentComplex::get_crap_count() {
  int crap_count = 0;

  for (auto &apartment : get_apartments()) {
    crap_count += apartment->get_crap_count();
  }

  return crap_count;
}

/*********** Apartment ************/

/* Blank apartment for editing. */
Apartment::Apartment() :
  shortname(NULL), name(NULL), full_name(NULL)
{
  snprintf(buf, sizeof(buf), "unnamed-%ld", time(0));
  set_short_name(buf);

  snprintf(buf, sizeof(buf), "Unit %s", shortname);
  name = str_dup(buf);

  snprintf(buf, sizeof(buf), "Somewhere's %s", name);
  full_name = str_dup(buf);
}

Apartment::Apartment(ApartmentComplex *complex, const char *new_name, vnum_t key_vnum, vnum_t new_atrium, int new_lifestyle, idnum_t owner, time_t paid_until) :
  atrium(new_atrium), key_vnum(key_vnum), owned_by_player(owner), paid_until(paid_until), complex(complex), lifestyle(new_lifestyle)
{
  char tmp[1000];

  shortname = str_dup(new_name);

  snprintf(tmp, sizeof(tmp), "Unit %s", shortname);
  name = str_dup(tmp);

  snprintf(tmp, sizeof(tmp), "%s's %s", complex->get_name(), name);
  full_name = str_dup(tmp);

  // Becomes something like 'lib/housing/22608/3A'
  set_base_directory(complex->base_directory / shortname);
  log_vfprintf("Apartment %s's base_directory: %s", full_name, base_directory.c_str());
}

/* Load this apartment entry from files. */
Apartment::Apartment(ApartmentComplex *complex, bf::path base_directory) :
  full_name(NULL), base_directory(base_directory), garages(0), complex(complex)
{
  // Load base info from <name>/info.
  {
    log_vfprintf(" ---- Loading apartment base data for %s.", base_directory.filename().c_str());
    json base_info;
    _json_parse_from_file(base_directory / APARTMENT_INFO_FILE_NAME, base_info);

    shortname = str_dup(base_info["short_name"].get<std::string>().c_str());
    name = str_dup(base_info["name"].get<std::string>().c_str());
    lifestyle = base_info["lifestyle"].get<int>();
    nuyen_per_month = base_info["rent"].get<long>();

    atrium = base_info["atrium"].get<vnum_t>();
    key_vnum = base_info["key"].get<vnum_t>();
  }

  // Load lease info from <name>/lease.
  if (bf::exists(base_directory / LEASE_INFO_FILE_NAME)) {
    if (bf::file_size(base_directory / LEASE_INFO_FILE_NAME) == 0) {
      log_vfprintf(" ---- Apartment lease file for %s exists but is empty. Removing.", name);
      bf::remove(base_directory / LEASE_INFO_FILE_NAME);
    } else {
      log_vfprintf(" ---- Loading apartment lease data for %s.", name);
      json base_info;
      _json_parse_from_file(base_directory / LEASE_INFO_FILE_NAME, base_info);

      paid_until = base_info["paid_until"].get<time_t>();
      guests = base_info["guests"].get<std::vector<idnum_t>>();

      idnum_t owner = base_info["owner"].get<idnum_t>();
      if (owner < 0) {
        // Negative numbers indicate pgroup ownership
        owned_by_pgroup = Playergroup::find_pgroup(-1 * owner);
      } else {
        owned_by_player = owner;
  #ifndef IS_BUILDPORT
        // Owner can no longer hold a lease.
        if (!does_player_exist(owner) || player_is_dead_hardcore(owner)) {
          log_vfprintf(" ----- Owner %ld is not valid: Breaking lease. ", owner);
          break_lease();
        }
        // Lease is expired.
        else if (get_paid_until() < time(0)) {
          log_vfprintf(" ----- Lease by %ld is not paid: Breaking lease. ", owner);
          break_lease();
        }
  #endif
      }
    }
  }

  // Load sub-rooms.
  {
    log_vfprintf(" ---- Loading sub-rooms for %s.", name);
    bf::directory_iterator end_itr; // default construction yields past-the-end
    for (bf::directory_iterator itr(base_directory); itr != end_itr; ++itr) {
      if (is_directory(itr->status())) {
        bf::path room_path = itr->path();

        // Ensure it has an info file. A missing file means this room was deleted.
        if (!bf::exists(room_path / ROOM_INFO_FILE_NAME)) {
          log_vfprintf(" ----- Skipping sub-room '%s': No info path.", room_path.filename().c_str());
          continue;
        }

        log_vfprintf(" ----- Initializing sub-room '%s'.", room_path.filename().c_str());
        ApartmentRoom *apartment_room = new ApartmentRoom(this, room_path);
        rooms.push_back(apartment_room);
        log_vfprintf(" ----- Fully loaded %s's %s at %ld.", name, room_path.filename().c_str(), apartment_room->get_vnum());
      }
    }
    recalculate_garages();
  }

  // Calculate any derived data.
  regenerate_full_name();

  // Drop any expired guests.
  cleanup_deleted_guests();
}

Apartment::~Apartment() {
  // Delete us from our parent complex.
  if (complex) {
    auto it = find(complex->apartments.begin(), complex->apartments.end(), this);
    if (it != complex->apartments.end())
      complex->apartments.erase(it);
  }

  // Delete any pointers to us from rooms.
  for (auto &room : rooms) {
    rnum_t rnum = real_room(room->vnum);
    if (rnum >= 0 && world[rnum].apartment == this) {
      world[rnum].apartment = NULL;
    }
  }

  // Clean up our str_dup'd bits.
  delete [] shortname;
  delete [] name;
  delete [] full_name;
}

void Apartment::ensure_base_directory_exists() {
  if (!bf::exists(base_directory)) {
    log_vfprintf("apartmentcomplex::ensure_base_directory_exists(): Not found. Creating base directory %s.", base_directory.c_str());
    bf::create_directory(base_directory);
  }
}


void Apartment::cleanup_deleted_guests() {
  // Drop any expired guests.
  guests.erase(std::remove_if(guests.begin(), guests.end(),
               [](idnum_t i) { return !does_player_exist(i); }), guests.end());
}

long Apartment::get_cost_of_contents(bool including_vehicles) {
  long total = 0;
  for (auto &room : rooms) {
    total += room->get_cost_of_contents(including_vehicles);
  }
  return total;
}

long Apartment::get_remaining_lease_value() {
  int days_until_deletion = (get_paid_until() - time(0)) / (60 * 60 * 24);
  int cost_per_day = nuyen_per_month / 30;
  return MAX(0, days_until_deletion * cost_per_day);
}

#define REPLACE_STR(item) {delete [] item; item = str_dup(source->item);}
#define REPLACE(item) {item = source->item;}
// Note that we discard and overwrite the room vector here.
void Apartment::clone_from(Apartment *source) {
  REPLACE_STR(shortname);
  REPLACE_STR(name);
  REPLACE_STR(full_name);

  REPLACE(lifestyle);
  REPLACE(nuyen_per_month);
  REPLACE(garage_override);

  REPLACE(atrium);
  REPLACE(key_vnum);

  // Create an empty temp vector and swap its contents with our current rooms vector, deallocating previous rooms.
  std::vector<ApartmentRoom *>().swap(rooms);
  // Copy over the apartment's rooms.
  for (auto &room : source->rooms) {
    ApartmentRoom *new_room = new ApartmentRoom(room);
    new_room->apartment = this;
    rooms.push_back(new_room);
  }

  REPLACE(garages);

  REPLACE(owned_by_player);
  REPLACE(owned_by_pgroup);
  REPLACE(paid_until);

  guests.clear();
  for (auto idnum : source->guests) {
    guests.push_back(idnum);
  }

  // set_base_directory() is called as part of set_complex()
  set_complex(source->get_complex());
}

bool Apartment::can_houseedit_apartment(struct char_data *ch) {
  return complex->can_houseedit_complex(ch);
}

bool Apartment::owner_is_valid() {
#ifdef IS_BUILDPORT
  // All owners are valid on the buildport.
  return TRUE;
#endif

  if (!owned_by_player)
    return FALSE;

  if (owned_by_pgroup)
    return !owned_by_pgroup->is_disabled();

  return does_player_exist(owned_by_player) && !player_is_dead_hardcore(owned_by_player);
}

idnum_t Apartment::get_owner_id() {
  if (owned_by_pgroup) {
    return -1 * owned_by_pgroup->get_idnum();
  }
  return owned_by_player;
}

bool Apartment::is_garage_lifestyle() {
  return garage_override || garages > (rooms.size() + 1) / 2;
}

void Apartment::set_base_directory(bf::path new_base) {
  // If we had no base directory to begin with, this is probably happening during initialization or editing. Take no on-disc action.
  if (base_directory.empty() || is_editing_struct) {
    base_directory = new_base;
    return;
  }

  // If this is a rename, process that.
  if (base_directory != new_base) {
    log_vfprintf("Renaming %s's base directory from %s to %s.",
                 full_name,
                 base_directory.c_str(),
                 new_base.c_str());
    bf::rename(base_directory, new_base);
  }

  // Finally, make the change.
  base_directory = new_base;

  // Update our rooms as well.
  for (auto *aroom : rooms) {
    aroom->regenerate_paths();
  }
}


void Apartment::save_base_info() {
  if (base_directory.empty()) {
    if (!complex) {
      // Some kind of editing going on, just stop.
      return;
    }
    log_vfprintf("No base directory found! Generating a new one...");
    set_base_directory(complex->base_directory / shortname);
  }

  // Ensure our base directory exists.
  log_vfprintf("apartment::save_base_info(): Checking for base directory %s...", base_directory.c_str());
  if (!bf::exists(base_directory)) {
    log_vfprintf("apartment::save_base_info(): Not found. Creating base directory %s.", base_directory.c_str());
    complex->ensure_base_directory_exists();
    bf::create_directory(base_directory);
  }

  json base_info;

  base_info["short_name"] = std::string(shortname);
  base_info["name"] = std::string(name);
  // Full name is derived and not saved here.

  base_info["lifestyle"] = lifestyle;
  base_info["rent"] = nuyen_per_month;

  base_info["atrium"] = atrium;
  base_info["key"] = key_vnum;

  // Write it out.
  write_json_file(base_directory / APARTMENT_INFO_FILE_NAME, &base_info);
  log("apartment::save_base_info(): Write complete.");
}

void Apartment::save_rooms() {
  if (rooms.empty()) {
    log_vfprintf("Skipping save_rooms() for %s: No rooms to save.", get_full_name());
    return;
  }

  // Looks like we're some kind of editing structure.
  if (base_directory.empty()) {
    // Regardless, if someone's trying to save_rooms() on us, we must matter. Save base info to create the dir, then save here.
    mudlog_vfprintf(NULL, LOG_SYSLOG, "WARNING: Called save_rooms() on improperly-initialized apartment '%s'! Saving its base info first.", get_full_name());
    save_base_info();
  }

  // Add all on-disk room directories to a vector.
  std::vector<std::string> existing_dirs = {};

  log("apartment::save_rooms(): Searching for existing room directories.");

  bf::directory_iterator end_itr; // default construction yields past-the-end
  for (bf::directory_iterator itr(base_directory); itr != end_itr; ++itr) {
    if (is_directory(itr->status())) {
      existing_dirs.push_back(itr->path().filename().string());
    }
  }

  log_vfprintf("save_rooms() for %s: existing_dirs:", get_full_name());
  for (auto it : existing_dirs)
    log_vfprintf(" - %s", it.c_str());

  log_vfprintf("save_rooms() for %s: Now saving rooms.", get_full_name());

  // Iterate through all rooms in apartment.
  for (auto &room : rooms) {
    log_vfprintf(" - %ld @ %s", room->get_vnum(), room->base_path.c_str());
    // Write or update room data. This also auto-creates subdir if necessary.
    room->save_info();

    // Remove its path from the on-disk room directories vector
    auto it = find(existing_dirs.begin(), existing_dirs.end(), room->base_path.filename().string());
    if (it != existing_dirs.end()) {
      existing_dirs.erase(it);
    }
  }

  // For any remaining directory in vector, invalidate it (delete info json)
  for (auto dir : existing_dirs) {
    log_vfprintf("save_rooms() for %s: %s still existed.", get_full_name(), dir.c_str());
    if (bf::exists(base_directory / dir / ROOM_INFO_FILE_NAME)) {
      log_vfprintf("save_rooms() for %s: Destroying info file for %s.", get_full_name(), dir.c_str());
      bf::remove(base_directory / dir / ROOM_INFO_FILE_NAME);
    }
  }
}

/* Write lease data to <apartment name>/lease. */
void Apartment::save_lease() {
  if (get_owner_id() == 0) {
    if (bf::exists(base_directory / LEASE_INFO_FILE_NAME)) {
      log_vfprintf("save_lease() called on empty apartment %s, so removing file", get_full_name());
      bf::remove(base_directory / LEASE_INFO_FILE_NAME);
    }
  } else {
    json lease_data;

    lease_data["owner"] = get_owner_id();
    lease_data["paid_until"] = paid_until;
    lease_data["guests"] = guests;

    bf::ofstream o(base_directory / LEASE_INFO_FILE_NAME);
    o << std::setw(4) << lease_data << std::endl;
    o.close();
  }
}

/* Removes all characters from the apartment and dumps them in the atrium. */
void Apartment::kick_out_everyone() {
  for (auto &room: rooms) {
    room->kick_out_characters();
    room->kick_out_vehicles();
  }
}

/* Delete <apartment name>/lease and restore default descs. */
void Apartment::break_lease() {
  mudlog_vfprintf(NULL, LOG_GRIDLOG, "Cleaning up lease for %s's %s.", complex->display_name, name);

  // Mail the owner.
  if (owned_by_pgroup) {
    // EVENTUALTODO
  } else if (owned_by_player > 0) {
    char mail_buf[1000];
    snprintf(mail_buf, sizeof(mail_buf), "The lease on %s has expired, and its contents have been reclaimed.\r\n", get_full_name());
    raw_store_mail(get_owner_id(), 0, "Your former landlord", mail_buf);
  }

  // Clear lease data.
  owned_by_player = 0;
  owned_by_pgroup = NULL;
  paid_until = 0;
  guests.clear();

  // Overwrite the lease file.
  save_lease();

  // Iterate over rooms and restore them.
  for (auto &room: rooms) {
    room->purge_contents();
    room->delete_decoration();
    room->delete_decorated_name();
    room->kick_out_characters();
    room->kick_out_vehicles();
  }

  // Iterate over all characters in the game and have them recalculate their lifestyles. This also confirms their lifestyle string.
  for (struct char_data *plr = character_list; plr; plr = plr->next_in_character_list) {
    int old_best_lifestyle = GET_BEST_LIFESTYLE(plr);
    
    calculate_best_lifestyle(plr);

    if (plr->desc && PRF_FLAGGED(plr, PRF_SEE_TIPS) && GET_BEST_LIFESTYLE(plr) < old_best_lifestyle) {
      send_to_char("^L(Hint: Your lifestyle went down due to a broken lease. You should select a new lifestyle string with the Change Lifestyle option in ^wCUSTOMIZE PHYSICAL^L.)^n\r\n", plr);
    }
  }
}

/* Check for entry permissions. */
bool Apartment::can_enter(struct char_data *ch) {
  // NPC, but not a spirit or elemental? No entry.
  if (IS_NPC(ch) && !(IS_SPIRIT(ch) || IS_PC_CONJURED_ELEMENTAL(ch)))
    return FALSE;

  // Admins and astral projections can enter any room.
  if (GET_LEVEL(ch) >= LVL_ADMIN || IS_ASTRAL(ch))
    return TRUE;

  // The lease must be paid up for guest / owner status to work. Fine if it's not leased though.
  if ((owned_by_pgroup || owned_by_player) && get_paid_until() < time(0))
    return FALSE;

  // Check for owner status or pgroup perms.
  if (owned_by_pgroup) {
    // Not a member of any group, or member of wrong group.
    if (GET_PGROUP_MEMBER_DATA(ch)
        && GET_PGROUP(ch) == owned_by_pgroup
        && GET_PGROUP_MEMBER_DATA(ch)->privileges.AreAnySet(PRIV_LEADER, PRIV_LANDLORD, PRIV_TENANT, ENDBIT))
    {
      return TRUE;
    }
    // Owned by a pgroup, but this ch does not have perms in it: Fall through.
  } else if (owned_by_player > 0) {
    if (GET_IDNUM(ch) == owned_by_player)
      return TRUE;
    // Owned by a player, but not this one: Fall through.
  } else {
    // Only staff can enter unleased rooms.
    // mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: %s is owned by neither player nor group! Allowing entry.", full_name);
    return IS_SENATOR(ch);
  }

  // Guests can enter any room.
  for (auto guest_idnum : guests) {
    if (GET_IDNUM(ch) == guest_idnum)
      return TRUE;
  }

#ifdef IS_BUILDPORT
  if (GET_LEVEL(ch) >= LVL_BUILDER) {
    send_to_char("(OOC: Entry allowed via staff override.)\r\n", ch);
    return TRUE;
  }
#endif

  return FALSE;
}

bool Apartment::can_enter_by_idnum(idnum_t idnum) {
  Bitfield required_privileges;

  // No idnum? No entry.
  if (idnum <= 0)
    return FALSE;

  // If nobody owns it at all, only staff can enter.
  if ((!owned_by_player && !owned_by_pgroup) || paid_until < time(0)) {
    return get_player_rank(idnum) >= LVL_BUILDER;
  }

  // Check for owner status or pgroup perms.
  if (owned_by_player > 0) {
    if (idnum == owned_by_player)
      return TRUE;
    // Fall through: Check for guest status.
  } else if (owned_by_pgroup) {
    required_privileges.SetBits(PRIV_LEADER, PRIV_LANDLORD, PRIV_TENANT, ENDBIT);
    if (pgroup_char_has_any_priv(idnum, owned_by_pgroup->get_idnum(), required_privileges))
      return TRUE;
    // Fall through: Check for guest status.
  }

  // Guests can enter any room.
  for (auto guest_idnum : guests) {
    if (idnum == guest_idnum)
      return TRUE;
  }

  // Neither owner nor guest: Fail.
  return FALSE;
}

int Apartment::get_days_in_arrears() {
  if (paid_until < time(0)) {
    int secs_in_arrears = time(0) - paid_until;
    int days_in_arrears = secs_in_arrears / SECS_PER_REAL_DAY;

    return days_in_arrears;
  }

  return 0;
}

bool Apartment::create_or_extend_lease(struct char_data *ch) {
  struct obj_data *neophyte_card = NULL;
  int cost = nuyen_per_month;

  // Special case: The apartment has already been leased and has expired.
  // You must pay the cost in arrears for the room, PLUS the new month.
  if (paid_until < time(0) && owned_by_player == GET_IDNUM(ch)) {
    int days_in_arrears = get_days_in_arrears();

    if (days_in_arrears > 0) {
      send_to_char(ch, "You grimace at the realization that %s is %d day%s in arrears, increasing the cost you have to pay to regain access.\r\n",
                   get_full_name(),
                   days_in_arrears,
                   days_in_arrears == 1 ? "" : "s");
      
      int cost_per_day = nuyen_per_month / 30;

      // Arrears penalty: 10% on top of cost.
      cost += (cost_per_day * days_in_arrears) * 1.1;

      // Max cost is 4 IRL months (3 months of arrears plus a month of rent): don't want to penalize people too harshly when they're returning to the game.
      cost = MIN(nuyen_per_month * 4, cost);
    }
  }

  int displayed_cost = cost;

  // Subtract subsidy card amounts, but only for High or lower.
  for (neophyte_card = ch->carrying; neophyte_card; neophyte_card = neophyte_card->next_content) {
    if (GET_OBJ_VNUM(neophyte_card) == OBJ_NEOPHYTE_SUBSIDY_CARD
        && GET_SUBSIDY_CARD_OWNER(neophyte_card) == GET_IDNUM(ch))
    {
      // Deny use of card for anything over High.
      if (get_lifestyle() > LIFESTYLE_HIGH) {
        send_to_char(ch, "You offer up %s as payment, but you get a *look* in response. Apparently subsidy cards aren't usable here.\r\n", GET_OBJ_NAME(neophyte_card));
        neophyte_card = NULL;
        break;
      }

      if (owned_by_player && owned_by_player != GET_IDNUM(ch)) {
        send_to_char(ch, "Sorry, you can only use subsidy cards to pay for apartments you directly own. Trying with cash...\r\n", ch);
        neophyte_card = NULL;
        break;
      }

      if (cost >= GET_SUBSIDY_CARD_VALUE(neophyte_card))
        cost -= GET_SUBSIDY_CARD_VALUE(neophyte_card);
      else
        cost = 0;
      break;
    }
  }

  // If they can't cover it in cash, draw from bank.
  if (GET_NUYEN(ch) < cost) {
    if (GET_BANK(ch) >= cost) {
      send_to_char("You arrange for a bank transfer.\r\n", ch);
      lose_bank(ch, cost, NUYEN_OUTFLOW_HOUSING);
    } else {
      send_to_char(ch, "You don't have the %d nuyen required.\r\n", displayed_cost);
      return FALSE;
    }
  } else {
    lose_nuyen(ch, cost, NUYEN_OUTFLOW_HOUSING);
    send_to_char(ch, "You hand over %d nuyen.\r\n", displayed_cost);
  }

  if (neophyte_card) {
    if (displayed_cost >= GET_OBJ_VAL(neophyte_card, 1)) {
      send_to_char(ch, "%s is now empty, so you junk it.\r\n", GET_OBJ_NAME(neophyte_card));
      extract_obj(neophyte_card);
    } else {
      GET_OBJ_VAL(neophyte_card, 1) -= displayed_cost;
      send_to_char(ch, "Your housing card has %d nuyen left on it.\r\n", GET_OBJ_VAL(neophyte_card, 1));
    }
  }

  if (!has_owner()) {
    // We expect that there were no existing guests.
    if (!guests.empty()) {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Newly-rented apartment %s already had guests! Running break_lease().", full_name);
      guests.clear();
      break_lease();
    }
    else {
      // We expect that it is empty.
      for (auto &apt_room : rooms) {
        struct room_data *room = apt_room->get_world_room();

        if (!room) {
          mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: %s's %ld has no world room!", get_full_name(), apt_room->vnum);
          continue;
        }

        if (room->people || room->contents || room->vehicles) {
          mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: %s did not have break_lease() run on it correctly! Running break_lease(). (ppl %s, obj %s, veh %s)",
                          get_full_name(),
                          room->people ? "Y" : "N",
                          room->contents ? "Y" : "N",
                          room->vehicles ? "Y" : "N");
          // Run break_lease to clean it out and wipe everything.
          break_lease();
          break;
        }
      }
    }

    owned_by_player = GET_IDNUM(ch);
    paid_until = time(0) + (SECS_PER_REAL_DAY*30);
  } 
  // Arrears extension requires setting a new lease timeout.
  else if (get_days_in_arrears() > 0) {
    paid_until = time(0) + (SECS_PER_REAL_DAY * 30);
  } 
  // Standard lease extension.
  else {
    paid_until += SECS_PER_REAL_DAY*30;
  }

  save_lease();

  // Update their lifestyle, forcing confirmation of lifestyle string validity.
  int old_best_lifestyle = GET_BEST_LIFESTYLE(ch);
  calculate_best_lifestyle(ch);

  if (GET_BEST_LIFESTYLE(ch) > old_best_lifestyle) {
    if (PRF_FLAGGED(ch, PRF_SEE_TIPS)) {
      send_to_char("^L(Hint: Since your lifestyle has just increased, you can flaunt your new status with the Change Lifestyle option in ^wCUSTOMIZE PHYSICAL^L!)^n\r\n", ch);
    }
  }

  return TRUE;
}

bool Apartment::issue_key(struct char_data *ch) {
  struct obj_data *key = read_object(key_vnum, VIRTUAL, OBJ_LOAD_REASON_APARTMENT_KEY_ISSUE);

  if (!key) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Apartment entry for %s specifies key vnum %ld, but it does not exist!", full_name, key_vnum);
    return FALSE;
  }

  obj_to_char(key, ch);
  return TRUE;
}

bool Apartment::has_owner_privs(struct char_data *ch) {
  if (owned_by_pgroup) {
    // Not a member of any group, or member of wrong group.
    if (!GET_PGROUP_MEMBER_DATA(ch) || GET_PGROUP(ch) != owned_by_pgroup) {
      return FALSE;
    }

    // Doesn't have the right perms.
    if (!GET_PGROUP_MEMBER_DATA(ch)->privileges.AreAnySet(PRIV_LEADER, PRIV_LANDLORD, ENDBIT)) {
      return FALSE;
    }

    // Has the right perms etc.
    return TRUE;
  } else {
    // We just care if they're the actual owner right now.
    return GET_IDNUM(ch) == owned_by_player;
  }
}

bool Apartment::has_owner_privs_by_idnum(idnum_t idnum) {
  Bitfield required_privileges;

  // No idnum? No ownership.
  if (idnum <= 0)
    return FALSE;

  if (owned_by_pgroup) {
    required_privileges.SetBits(PRIV_LEADER, PRIV_LANDLORD, ENDBIT);
    return pgroup_char_has_any_priv(idnum, owned_by_pgroup->get_idnum(), required_privileges);
  } else {
    return idnum == owned_by_player;
  }

  return FALSE;
}

void Apartment::list_guests_to_char(struct char_data *ch) {
  send_to_char(ch, "  Guests: ");

  if (guests.empty()) {
    send_to_char("None.\r\n", ch);
    return;
  }

  bool printed_guest_yet = FALSE;

  std::vector<idnum_t> invalid_guests = {};
  for (auto guest_idnum : guests) {
    const char *temp;

    if (player_is_dead_hardcore(guest_idnum) || !(temp = get_player_name(guest_idnum))) {
      invalid_guests.push_back(guest_idnum);
      continue;
    }

    send_to_char(ch, "%s%s", printed_guest_yet ? ", " : "", temp);
    DELETE_ARRAY_IF_EXTANT(temp);

    printed_guest_yet = TRUE;
  }

  // Drop any invalid guests.
  for (auto guest_idnum : invalid_guests) {
    guests.erase(find(guests.begin(), guests.end(), guest_idnum));
  }

  if (!printed_guest_yet) {
    send_to_char("None.\r\n", ch);
  } else {
    send_to_char("\r\n", ch);
  }
}

// Returns new-- must delete output!
const char * Apartment::get_owner_name__returns_new() {
  char buf[1000];
  if (owned_by_pgroup) {
    snprintf(buf, sizeof(buf), "Group %s^n", owned_by_pgroup->get_name());
    return str_dup(buf);
  } else if (owned_by_player < 0) {
    return str_dup("Unloaded Group");
  } else {
    return get_player_name(owned_by_player);
  }
}

// Returns new-- must delete output!
const char *Apartment::list_rooms__returns_new(bool indent) {
  char result[MAX_STRING_LENGTH];
  result[0] = '\0';
  struct room_data *world_room;

  if (rooms.empty()) {
    return str_dup("n/a\r\n");
  }

  bool already_printed = FALSE;
  for (auto &room: rooms) {
    if (!(world_room = room->get_world_room()))
      break;

    snprintf(ENDOF(result), sizeof(result) + strlen(result), "%s%s^n (%ld)\r\n",
             indent ? "   - " : (already_printed ? ", " : ""),
             GET_ROOM_NAME(world_room),
             GET_ROOM_VNUM(world_room));

    already_printed = TRUE;
  }
  return str_dup(result);
}

// Note that all the strings in the vector returned here are str_dup'd-- destroy them after use!
#define COPY_SOURCE_TO_RESULTS(source) { for (auto it : source) { results.push_back(str_dup(it)); } }
std::vector<const char *> *Apartment::get_custom_lifestyle_strings(struct char_data *ch) {
  static std::vector<const char *> results = {};

  results.clear();

  // If we're a garage, we only return garage strings.
  if (is_garage_lifestyle()) {
    if (GET_PRONOUNS(ch) == PRONOUNS_NEUTRAL) {
      // Copy in our apartment-level custom strings for this lifestyle.
      COPY_SOURCE_TO_RESULTS(garage_strings_neutral);
      // And our complex-level custom ones.
      COPY_SOURCE_TO_RESULTS(complex->garage_strings_neutral);
    } else {
      COPY_SOURCE_TO_RESULTS(garage_strings_gendered);
      COPY_SOURCE_TO_RESULTS(complex->garage_strings_gendered);
    }
  } else {
    if (GET_PRONOUNS(ch) == PRONOUNS_NEUTRAL) {
      COPY_SOURCE_TO_RESULTS(default_strings_neutral);
      COPY_SOURCE_TO_RESULTS(complex->default_strings_neutral);
    } else {
      COPY_SOURCE_TO_RESULTS(default_strings_gendered);
      COPY_SOURCE_TO_RESULTS(complex->default_strings_gendered);
    }
  }

  return &results;
}
#undef COPY_SOURCE_TO_RESULTS

void Apartment::mark_as_deleted() {
  bf::path deleted_apartments = complex->base_directory / DELETED_APARTMENTS_DIR_NAME;

  // Ensure our deleted-apartments dir exists.
  if (!bf::exists(deleted_apartments)) {
    bf::create_directory(deleted_apartments);
  }

  char deleted_name[500];
  snprintf(deleted_name, sizeof(deleted_name), "%s-%ld", name, time(0));
  bf::rename(base_directory, deleted_apartments / deleted_name);
}

void Apartment::set_complex(ApartmentComplex *new_complex) {
  // If we're an editing struct, just point us at the new complex.
  if (is_editing_struct) {
    complex = new_complex;
    regenerate_full_name();
    return;
  }

  // Remove us from our existing complex, provided we have one.
  if (complex) {
    complex->delete_apartment(this);
  }

  // Perform the actual update.
  complex = new_complex;

  // Add us to that complex's apartments.
  complex->add_apartment(this);

  // Change our save directory. Becomes something like 'lib/housing/22608/3A'
  // TODO: Validate that an existing apartment with this name does not already exist in that directory.
  set_base_directory(complex->base_directory / shortname);
  log_vfprintf("Changed apartment %s's complex; new base_directory: %s", full_name, base_directory.c_str());

  // Recalculate our full name.
  regenerate_full_name();

  // Note: Sub-room directory recalculation was handled in set_base_directory().
}

void Apartment::clamp_rent(struct char_data *ch) {
  // Clamp the rent to the lifestyle's band.
  int lifestyle = get_lifestyle();
  long minimum = lifestyles[lifestyle].monthly_cost_min;
  long maximum = lifestyle < NUM_LIFESTYLES - 1 ? lifestyles[lifestyle + 1].monthly_cost_min : UINT_MAX;
  if (nuyen_per_month < minimum || nuyen_per_month > maximum) {
    if (ch)
      send_to_char(ch, "Your selected lifestyle's rent band is %ld - %ld nuyen. Clamped your apartment's rent to match.\r\n", minimum, maximum);
    nuyen_per_month = MIN(maximum, MAX(minimum, nuyen_per_month));
  }
}

bool Apartment::set_rent(long amount, struct char_data *ch) {
  // Rent value must be 1 or greater.
  if (amount <= 0) {
    if (ch)
      send_to_char("The Sprawl isn't that kind-- rent values must be greater than 0.\r\n", ch);
    return FALSE;
  }

  nuyen_per_month = amount;

  clamp_rent(ch);

  return TRUE;
}

bool Apartment::set_lifestyle(int new_lifestyle, struct char_data *ch) {
  if (new_lifestyle < -1 || new_lifestyle >= NUM_LIFESTYLES) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Received invalid lifestyle %d to apartment.set_lifestyle!", new_lifestyle);
    return FALSE;
  }

  lifestyle = new_lifestyle;

  clamp_rent(ch);

  return TRUE;
}

bool Apartment::is_guest(idnum_t idnum) {
  auto it = find(guests.begin(), guests.end(), idnum);
  return (it != guests.end());
}

bool Apartment::delete_guest(idnum_t idnum) {
  auto it = find(guests.begin(), guests.end(), idnum);
  if (it != guests.end()) {
    guests.erase(it);
    save_lease();
    return TRUE;
  }
  return FALSE;
}

void Apartment::add_guest(idnum_t idnum) {
  if (idnum != 0 && does_player_exist(idnum) && !is_guest(idnum)) {
    guests.push_back(idnum);
    save_lease();
  }
}

bool Apartment::add_room(ApartmentRoom *room) {
  // Check to make sure it's not part of this apartment. Technically duplicates work done below, but with a different message.
  auto it = find(rooms.begin(), rooms.end(), room);
  if (it != rooms.end()) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Attempted to add room %ld to %s, but it was already there!", room->vnum, get_full_name());
    return FALSE;
  }

  // Check to make sure this isn't part of another apartment somewhere.
  for (auto &cplx : global_apartment_complexes) {
    for (auto &apt : cplx->get_apartments()) {
      auto it = find(apt->rooms.begin(), apt->rooms.end(), room);
      if (it != apt->rooms.end()) {
        mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Attempted to add room %ld to %s, but it is already part of %s!", room->vnum, get_full_name(), apt->get_full_name());
        return FALSE;
      }
    }
  }

  rooms.push_back(room);
  recalculate_garages();
  return TRUE;
}

void Apartment::recalculate_garages() {
  garages = 0;

  for (auto &room : rooms) {
    rnum_t rnum = real_room(room->vnum);
    if (rnum < 0) {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Invalid subroom %ld while calculating garages for %s!", room->vnum, get_full_name());
    } else {
      if (ROOM_FLAGGED(&world[rnum], ROOM_GARAGE))
        garages++;
    }
  }
}

void Apartment::regenerate_full_name() {
  delete [] full_name;
  char formatted_name[120];
  snprintf(formatted_name, sizeof(formatted_name), "%s's %s", 
           complex ? complex->get_name() : "<editing struct>", 
           name);
  full_name = str_dup(formatted_name);
}

void Apartment::set_name(const char *newname) {
  delete [] name;
  name = str_dup(newname);

  regenerate_full_name();
}

void Apartment::set_short_name(const char *newname) {
  // We absolutely requre that the new name contains only letters, spaces, and numbers.
  for (const char *c = newname; *c; c++) {
    if (!isalnum(*c) && !isspace(*c) && *c != '-') {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Attempted to set invalid apartment shortname '%s' for '%s'.", newname, full_name);
      return;
    }
  }

  // Clean up old one.
  delete [] shortname; 
  shortname = str_dup(newname); 

  /*
  bf::path new_base_directory = complex ? (complex->base_directory / shortname) : (global_housing_dir / shortname);

  // Move our files over (only if we have a base dir in the first place). Generates something like 'lib/housing/22608/3A'
  if (!base_directory.empty()) {
    if (bf::exists(base_directory)) {
      log_vfprintf("Moving files over for %s's rename: '''%s''' -> '''%s'''", full_name, base_directory.c_str(), new_base_directory.c_str());
      bf::rename(base_directory, new_base_directory);
    }
  }

  // Change where we save to in the future.
  base_directory = new_base_directory;

  log_vfprintf("Changed apartment %s's short_name: new name is %s, new base_directory is %s", full_name, shortname, base_directory.c_str());

  // Force all sub-rooms to recalculate as well.
  for (auto *aroom : rooms) {
    aroom->regenerate_paths();
  }
  */

  // Recalculate our full name.
  regenerate_full_name();
}

void Apartment::apply_rooms() {
  for (auto &room : rooms) {
    rnum_t rnum = real_room(room->vnum);
    if (rnum >= 0) {
      world[rnum].apartment = this;
      world[rnum].apartment_room = room;
    } else {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Invalid vnum %ld when applying rooms for %s!", room->vnum, get_full_name());
    }
  }
}

int Apartment::get_crap_count() {
  int crap_count = 0;
  long cash_count = 0;

  for (auto &room : get_rooms()) {
    rnum_t rnum = real_room(room->get_vnum());
    if (rnum >= 0) {
      struct room_data *world_room = &world[rnum];

      crap_count += count_objects_in_room(world_room, cash_count);

      for (struct veh_data *veh = world_room->vehicles; veh; veh = veh->next_veh) {
        crap_count += count_objects_in_veh(veh, cash_count);
      }
    }
  }

  return crap_count;
}

// Gets the vnum of the first room in the list. Hopefully the one that's attached to the atrium, but who knows...
vnum_t Apartment::get_root_vnum() {
  for (auto &room : get_rooms()) {
    rnum_t rnum = real_room(room->get_vnum());
    if (rnum >= 0) {
      return GET_ROOM_VNUM(&world[rnum]);
    }
  }

  return 0;
}

void Apartment::set_owner(idnum_t owner) {
  owned_by_player = owner;

  // Overwrite the lease file.
  save_lease();
}

void Apartment::set_paid_until(time_t paid_tm) {
  paid_until = paid_tm;

  // Overwrite the lease file.
  save_lease();
}

void Apartment::load_guests_from_old_house_file(const char *filename) {
  File fl;

  if (!(fl.Open(filename, "r+b"))) { /* no file found */
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Not loading house file %s (guests): File not found.", filename);
    return;
  }

  log_vfprintf("Loading house file %s (guest mode).", filename);
  VTable data;
  data.Parse(&fl);
  fl.Close();
  snprintf(buf3, sizeof(buf3), "house-load-guests %s", filename);

  // Wipe old guests.
  for (auto guest : guests) {
    log_vfprintf("Wiping old guest %ld from %s.", guest, full_name);
  }
  guests.clear();

  for (int i = 0; i < MAX_GUESTS; i++) {
    snprintf(buf, sizeof(buf), "GUESTS/Guest%d", i);
    idnum_t guest = data.GetLong(buf, 0);
    add_guest(guest);
  }
}

/********** ApartmentRoom ************/

ApartmentRoom::ApartmentRoom(Apartment *apartment, bf::path filename) :
  base_path(filename), apartment(apartment)
{
  // Read from room info file.
  json base_info;
  _json_parse_from_file(filename / ROOM_INFO_FILE_NAME, base_info);

  vnum = base_info["vnum"].get<vnum_t>();

  rnum_t rnum = real_room(vnum);
  if (vnum < 0 || rnum < 0) {
    log_vfprintf("SYSERR: Invalid vnum %ld specified in %s.", vnum, filename.c_str());
    exit(1);
  }

  struct room_data *room = &world[rnum];

  // Sanity check for double apartment load.
  if (GET_APARTMENT(room) || GET_APARTMENT_SUBROOM(room)) {
    log_vfprintf("ERROR: Room %ld already had an apartment entry! Terminating.", GET_ROOM_VNUM(room));
    exit(1);
  }

  // Set apartment pointer, etc.
  room->apartment_room = this;
  room->apartment = apartment;
  storage_path = filename / "storage";

  if (apartment->get_paid_until() >= time(0) && apartment->has_owner() && apartment->owner_is_valid()) {
    // Load decorated name.
    if (base_info.find("decorated_name") != base_info.end()) {
      decorated_name = str_dup(base_info["decorated_name"].get<std::string>().c_str());
    } else {
      decorated_name = NULL;
    }

    // Read decoration from standalone file.
    bf::ifstream ifs(base_path / "decoration.txt");
    std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    ifs.close();
    decoration = *(content.c_str()) ? str_dup(content.c_str()) : NULL;

    log_vfprintf(" ----- Applying changes from %s to world...", filename.filename().c_str());

    // Load storage.
    load_storage();
  }
  else {
    // Lease isn't valid, bail out.
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Refusing to load decoration and storage for %ld: Lease is expired, or owner does not exist.", GET_ROOM_VNUM(room));
    delete [] decoration;
    decoration = NULL;
    delete [] decorated_name;
    decorated_name = NULL;
  }
}

// Note that we DO NOT set room's backlink pointers to us here. You MUST true up later.
ApartmentRoom::ApartmentRoom(Apartment *apartment, struct room_data *room) :
  apartment(apartment)
{
  vnum = GET_ROOM_VNUM(room);
  decoration = NULL;
  decorated_name = NULL;

  regenerate_paths();
}

// Clone, duplicating the decoration but straight referencing apartment.
ApartmentRoom::ApartmentRoom(ApartmentRoom *source) {
  vnum = source->vnum;
  decoration = source->decoration ? str_dup(source->decoration) : NULL;
  decorated_name = source->decorated_name ? str_dup(source->decorated_name) : NULL;
  base_path = source->base_path;
  storage_path = source->storage_path;
  apartment = source->apartment;
}

ApartmentRoom::~ApartmentRoom() {
  // Make sure nobody's pointing to us.
  rnum_t rnum = real_room(vnum);
  if (rnum >= 0 && world[rnum].apartment_room == this) {
    world[rnum].apartment_room = NULL;
  }

  // Remove us from our parent apartment, if we have one.
  if (apartment) {
    auto it = find(apartment->rooms.begin(), apartment->rooms.end(), this);
    if (it != apartment->rooms.end())
      apartment->rooms.erase(it);
  }
}

long ApartmentRoom::get_cost_of_contents(bool including_vehicles) {
  struct room_data *room = &world[real_room(vnum)];

  long total = 0;
  for (struct obj_data *obj = room->contents; obj; obj = obj->next_content) {
    total += get_cost_of_obj_and_contents(obj);
  }

  if (including_vehicles) {
    for (struct veh_data *veh = room->vehicles; veh; veh = veh->next_veh) {
      total += get_cost_of_veh_and_contents(veh);
    }
  }

  return total;
}

void ApartmentRoom::regenerate_paths() {
  log_vfprintf("Regenerating paths for %ld (%s || %s)...", vnum, base_path.c_str(), storage_path.c_str());
  base_path = apartment->base_directory / vnum_to_string(vnum);
  storage_path = base_path / "storage";
  log_vfprintf("Done, got %s || %s.", base_path.c_str(), storage_path.c_str());
}

void ApartmentRoom::save_info() {
  json info_data;

  info_data["vnum"] = vnum;
  if (decorated_name) {
    info_data["decorated_name"] = decorated_name;
  }

  // We can't guarantee our base path exists at this point, so we ensure it does here.
  if (!bf::exists(base_path)) {
    apartment->ensure_base_directory_exists();
    bf::create_directory(base_path);
  }

  bf::ofstream o(base_path / ROOM_INFO_FILE_NAME);
  o << std::setw(4) << info_data << std::endl;
  o.close();
}

void ApartmentRoom::save_decoration() {
  bf::path filepath = base_path / "decoration.txt";

  if (decoration) {
    // Write the decoration file.
    bf::ofstream o(filepath);
    o << decoration;
    o.close();
  } else {
    // Delete the decoration file.
    bf::remove(filepath);
  }

  // The decorated name is saved to the info file, so we save that here too.
  save_info();
}

void ApartmentRoom::set_decorated_name(const char *new_desc) {
  // Wipe out existing decoration.
  delete [] decorated_name;
  decorated_name = NULL;

  // Clearing decoration.
  if (!*new_desc)
    return;

  // Initialize our formatted desc.
  size_t new_desc_len = strlen(new_desc);
  size_t formatted_desc_size = new_desc_len + 1000;
  char formatted_desc[formatted_desc_size];
  strlcpy(formatted_desc, new_desc, formatted_desc_size);

  // Ensure that it's terminated with a color code.
  if (new_desc[new_desc_len - 2] != '^' || (new_desc[new_desc_len - 1] != 'n' || new_desc[new_desc_len - 1] != 'N')) {
    strlcat(formatted_desc, "^n", formatted_desc_size);
  }

  decorated_name = str_dup(new_desc);

  // Save it.
  save_info();
}

void ApartmentRoom::set_decoration(const char *new_desc) {
  // Wipe out existing decoration.
  delete [] decoration;
  decoration = NULL;

  // Clearing decoration.
  if (!*new_desc) {
    save_decoration();
    return;
  }

  // Initialize our formatted desc.
  size_t new_desc_len = strlen(new_desc);
  size_t formatted_desc_size = new_desc_len + 1000;
  char formatted_desc[formatted_desc_size];
  strlcpy(formatted_desc, new_desc, formatted_desc_size);

  // Ensure that it's terminated with a color code.
  if (new_desc[new_desc_len - 2] != '^' || (new_desc[new_desc_len - 1] != 'n' || new_desc[new_desc_len - 1] != 'N')) {
    strlcat(formatted_desc, "^n", formatted_desc_size);
  }

  decoration = str_dup(new_desc);

  // Save it.
  save_decoration();
}

/* Purge the contents of this apartment, logging as we go. */
void ApartmentRoom::purge_contents() {
  rnum_t rnum = real_room(vnum);
  bool had_anything = FALSE;

  if (rnum < 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Refusing to purge contents for apartment room %ld: Invalid vnum", vnum);
    return;
  }

  struct room_data *room = &world[rnum];

#ifndef IS_BUILDPORT
  // Ensure the expired directory exists.
  bf::path expired_path = base_path / "expired";
  if (!bf::exists(expired_path)) {
    bf::create_directory(expired_path);
  }

  // Write a backup storage file to <name>/expired/storage_<ownerid>_<epoch>
  char filename[100];
  snprintf(filename, sizeof(filename), "%ld_%ld", time(0), apartment->get_owner_id());
  bf::path expired_storage_path = expired_path / filename;
  Storage_save(expired_storage_path.c_str(), room);
#endif

  if (room->contents) {
    had_anything = TRUE;

    // Purge all contents, logging as we go.
    int total_value_destroyed = 0;
    for (struct obj_data *obj = room->contents, *next_o; obj; obj = next_o) {
      next_o = obj->next_content;

      const char *representation = generate_new_loggable_representation(obj);

      // If this is belongings, don't purge it. Move it to Dante's if possible.
      if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER && obj->obj_flags.extra_flags.IsSet(ITEM_EXTRA_CORPSE)) {
        rnum_t dantes_rnum = real_room(RM_ENTRANCE_TO_DANTES);
        if (dantes_rnum >= 0) {
          mudlog_vfprintf(NULL, LOG_PURGELOG, "Apartment cleanup for %s (%ld) REFUSING to purge PC corpse %s. Will instead move it to Dante's.", apartment->full_name, GET_ROOM_VNUM(room), representation);
          obj_from_room(obj);
          obj_to_room(obj, &world[dantes_rnum]);
          send_to_room("You blink and realize that a pile of belongings must have been here the whole time.", obj->in_room);
        } else {
          mudlog_vfprintf(NULL, LOG_PURGELOG, "Apartment cleanup for %s (%ld) REFUSING to purge PC corpse %s, but FAILED to move it to Dante's!", apartment->full_name, GET_ROOM_VNUM(room), representation);
        }
      } 
      // Blow it away.
      else {
        mudlog_vfprintf(NULL, LOG_PURGELOG, "Apartment cleanup for %s (%ld) has purged %s (cost: %d).", apartment->full_name, GET_ROOM_VNUM(room), representation, GET_OBJ_COST(obj));
        total_value_destroyed += GET_OBJ_COST(obj);
        extract_obj(obj);
      }

      delete [] representation;
    }
    mudlog_vfprintf(NULL, LOG_PURGELOG, "Total value destroyed from %s (%ld): %d nuyen.", apartment->full_name, GET_ROOM_VNUM(room), total_value_destroyed);
  }

  // Transfer all vehicles to the nearest garage.
  if (room->vehicles) {
    had_anything = TRUE;
    remove_vehicles_from_apartment(room);
  }

  // Notify the room.
  if (had_anything)
    send_to_room("A pair of movers comes through and cleans up with brisk efficiency.\r\n", room);

  // Save it. Set the dirty bit to ensure it's done.
  room->dirty_bit = TRUE;
  save_storage();
}

/* Overwrite the room's desc with the default. */
void ApartmentRoom::delete_decoration() {
  rnum_t rnum = real_room(vnum);

  if (rnum < 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Refusing to clear decoration for %ld: Invalid vnum", vnum);
    return;
  }

  struct room_data *room = &world[rnum];

  if (!decoration) {
    // No action to take-- bail out.
    return;
  }

  delete [] decoration;
  decoration = NULL;

  send_to_room("You blink, then shrug-- this place must have always looked this way.\r\n", room);
}

void ApartmentRoom::delete_decorated_name() {
  rnum_t rnum = real_room(vnum);

  if (rnum < 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Refusing to clear decorated name for %ld: Invalid vnum", vnum);
    return;
  }

  struct room_data *room = &world[rnum];

  if (!decorated_name) {
    // No action to take-- bail out.
    return;
  }

  delete [] decorated_name;
  decorated_name = NULL;

  send_to_room("You blink, then shrug-- this place must have always been called that.\r\n", room);
}

void _escort_char_to_atrium(struct char_data *ch, struct room_data *atrium) {
  send_to_char(ch, "You are escorted out.\r\n");
  STOP_WORKING(ch);
  STOP_DRIVING(ch);
  char_from_room(ch);
  char_to_room(ch, atrium);
  GET_POS(ch) = POS_STANDING;

  if (!PRF_FLAGGED(ch, PRF_SCREENREADER)) {
    look_at_room(ch, 0, 0);
  }
}

void _empty_out_vehicle_to_atrium(struct veh_data *veh, struct room_data *atrium) {
  // Kick out all the characters in the vehicle.
  for (struct char_data *ch = veh->people, *next_ch; ch; ch = next_ch) {
    next_ch = ch->next_in_veh;
    _escort_char_to_atrium(ch, atrium);
  }

  // Stop the vehicle in case it was running.
  if (!veh->dest)
    veh->cspeed = SPEED_OFF;
  stop_chase(veh);

  // Recurse.
  for (struct veh_data *contained = veh->carriedvehs, *next_cont; contained; contained = next_cont) {
    next_cont = contained->next_veh;
    _empty_out_vehicle_to_atrium(contained, atrium);
  }
}

void ApartmentRoom::kick_out_characters() {
  struct room_data *room = &world[real_room(vnum)];
  struct room_data *atrium = &world[real_room(apartment->get_atrium_vnum())];

  if (!room || !atrium) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Could not kick out characters from %s (%ld): Room or atrium (%ld) did not exist!", 
                    get_full_name(),
                    vnum,
                    apartment->get_atrium_vnum());
    return;
  }

  for (struct char_data *ch = room->people, *next_ch; ch; ch = next_ch) {
    next_ch = ch->next_in_room;
    _escort_char_to_atrium(ch, atrium);
  }

  for (struct veh_data *veh = room->vehicles, *next_veh; veh; veh = next_veh) {
    next_veh = veh->next_veh;
    _empty_out_vehicle_to_atrium(veh, atrium);
  }
}

void ApartmentRoom::kick_out_vehicles() {
  struct room_data *room = &world[real_room(vnum)];
  struct room_data *parking_garage = &world[real_room(RM_SEATTLE_PARKING_GARAGE)];

  if (!room || !parking_garage) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Could not kick out vehicles from %s (%ld): Room or garage did not exist!",
                    get_full_name(),
                    vnum);
    return;
  }

  for (struct veh_data *veh = room->vehicles, *next_veh; veh; veh = next_veh) {
    next_veh = veh->next_veh;

    // Stuff it somewhere in the Seattle parking garage.
    veh_from_room(veh);
    veh_to_room(veh, parking_garage);

    // Give it a pity repair if needed so it's not wiped on crash.
    if (veh->damage >= VEH_DAM_THRESHOLD_DESTROYED) {
      veh->damage = VEH_DAM_THRESHOLD_DESTROYED - 1;
    }
  }
}

/* Write storage data to <apartment name>/storage. This is basically the existing houses/<vnum> file. */
void ApartmentRoom::save_storage() {
  struct room_data *room = &world[real_room(vnum)];

  // If the dirty bit is not set, we don't save the contents-- it means nobody is or was here since last save.
  // YES, this does induce bugs, like evaluate programs not decaying if nobody is around to see it happen--
  // but fuck it, if someone exploits it we'll just ban them. Easy enough.
  if (!room->dirty_bit && !room->people) {
    // log_vfprintf("Refusing to save room %s (%ld): Not dirty, no people.", GET_ROOM_NAME(room), GET_ROOM_VNUM(room));
    return;
  }

  // Warn if we've been given a room with an invalid lease.
  if (apartment->get_paid_until() - time(0) < 0) {
#ifndef IS_BUILDPORT
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Received save_storage() call for apartment with invalid lease (%s / %ld)!", GET_ROOM_NAME(room), GET_ROOM_VNUM(room));
#endif
    return;
  }

  const char *save_location = storage_path.c_str();

  /*
  log_vfprintf("Saving storage for room %s (%ld): %s dirty, %s occupied. Saving to %s.", 
                GET_ROOM_NAME(room),
                GET_ROOM_VNUM(room),
                room->dirty_bit ? "is" : "not",
                room->people ? "is" : "not",
                save_location);
  */

  // Clear the dirty bit now that we've processed it.
  room->dirty_bit = FALSE;

  // Write the file.
  Storage_save(save_location, room);
}

void ApartmentRoom::load_storage() {
  load_storage_from_specified_path(storage_path);
}

void ApartmentRoom::load_storage_from_specified_path(bf::path path) {
  // We assume our room exists. If it doesn't, we'll crash at this point.
  rnum_t rnum = real_room(vnum);

  if (rnum < 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: ApartmentRoom %s is associated with non-existent vnum %ld!", get_full_name(), vnum);
    return;
  }

  struct room_data *room = &world[rnum];

  // Blow away any existing storage.
  while (room->contents) {
    const char *representation = generate_new_loggable_representation(room->contents);
    log_vfprintf("Purging room contents in preparation for storage load: %s", representation);
    extract_obj(room->contents);
    delete [] representation;
  }

  // Perform load.
  House_load_storage(room, path.c_str());
}

const char * ApartmentRoom::get_full_name() {
  static char fullname[80];
  snprintf(fullname, sizeof(fullname), "%s's %ld", apartment->full_name, vnum);
  return fullname;
}

struct room_data *ApartmentRoom::get_world_room() {
  rnum_t rnum = real_room(vnum);

  if (rnum < 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: %s has invalid room vnum %ld!", get_full_name(), vnum);
    return NULL;
  }

  return &world[rnum];
}

void ApartmentRoom::delete_info() {
  if (bf::exists(base_path / ROOM_INFO_FILE_NAME)) {
    bf::remove(base_path / ROOM_INFO_FILE_NAME);
  }
}

///////////////////////// Utility functions ////////////////////////////////

// Given a room, move all vehicles from it to the Seattle garage, logging as we go.
void remove_vehicles_from_apartment(struct room_data *room) {
  struct room_data *garage = &world[real_room(RM_SEATTLE_PARKING_GARAGE)];

  while (room->vehicles) {
    struct veh_data *veh = room->vehicles;
    mudlog_vfprintf(NULL, LOG_GRIDLOG, "Apartment cleanup: Moving vehicle %s (%ld) from %s (%ld) to %s (%ld).",
                    GET_VEH_NAME(veh), GET_VEH_VNUM(veh),
                    GET_ROOM_NAME(room), GET_ROOM_VNUM(room),
                    GET_ROOM_NAME(garage), GET_ROOM_VNUM(garage));
    veh_from_room(veh);
    veh_to_room(veh, garage);
    save_single_vehicle(veh);
  }
}

// Send warnings about expiring apartments.
void warn_about_apartment_deletion() {
  char buf[1000];

  // log("Beginning apartment deletion warning cycle.");
  for (auto &complex : global_apartment_complexes) {
    for (auto &apartment : complex->get_apartments()) {
      if (!apartment->has_owner() || !apartment->owner_is_valid()) {
        //log_vfprintf("No owner for %s.", apartment->get_full_name());
        continue;
      }

      int days_until_deletion = (apartment->get_paid_until() - time(0)) / (60 * 60 * 24);

      if (days_until_deletion != 14 && days_until_deletion != 7 && (days_until_deletion > 5 || days_until_deletion < 0)) {
        //log_vfprintf("House %s is OK-- %d days left (%d - %d)", apartment->get_full_name(), days_until_deletion, apartment->get_paid_until(), time(0));
        continue;
      }

      mudlog_vfprintf(NULL, LOG_GRIDLOG, "Sending %d-day rent warning message for apartment %s to %ld.",
                      days_until_deletion,
                      apartment->get_full_name(),
                      apartment->get_owner_id());

      if (apartment->get_owner_pgroup()) {
        // EVENTUALTODO
      } else {
        if (days_until_deletion == 0) {
          snprintf(buf, sizeof(buf), "Remember to pay your rent for apartment %s^n! It will be deemed abandoned and its contents reclaimed ^Rat any time^n.\r\n",
                   apartment->get_full_name());
        } else if (days_until_deletion > 0) {
          snprintf(buf, sizeof(buf), "Remember to pay your rent for apartment %s^n. It will be deemed abandoned and its contents reclaimed in %d days.\r\n",
                   apartment->get_full_name(),
                   days_until_deletion);
        } else {
          // Negative days: No-op.
          continue;
        }
        raw_store_mail(apartment->get_owner_id(), 0, "Your landlord", buf);
      }
    }
  }
  // log("Apartment deletion warning cycle complete.");
}

// Given a name, return the complex that starts with it, or NULL on error.
// Error condition: More than one complex matches.
ApartmentComplex *find_apartment_complex(const char *name, struct char_data *ch) {
  if (!name || !*name) {
    // Get the one they're standing in.
    if (ch->in_room) {
      if (GET_APARTMENT(ch->in_room)) {
        return GET_APARTMENT(ch->in_room)->get_complex();
      }

      // Check if they're standing next to a landlord. If so, grab that landlord's complex.
      ApartmentComplex *complex;
      for (struct char_data *mob = ch->in_room->people; mob; mob = mob->next_in_room) {
        // Are they a landlord?
        if (GET_MOB_SPEC(mob) == landlord_spec || GET_MOB_SPEC2(mob) == landlord_spec) {
          // They're a landlord-- find their complex.
          if (!(complex = get_complex_headed_by_landlord(GET_MOB_VNUM(mob)))) {
            mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Could not find apartment complex for landlord %s (%ld)!", GET_CHAR_NAME(mob), GET_MOB_VNUM(mob));
            break;
          }
          // Found it-- return it.
          return complex;
        }
      }
    }

    send_to_char("You must provide a complex name OR be standing in an apartment to do that.\r\n", ch);
    return NULL;
  }

  std::vector<ApartmentComplex *> found_complexes = {};

  for (auto &complex : global_apartment_complexes) {
    // Exact match.
    if (!str_cmp(name, complex->get_name()))
      return complex;

    // Imprecise match.
    if (is_abbrev(name, complex->get_name()))
      found_complexes.push_back(complex);
  }

  if (found_complexes.size() == 1)
    return found_complexes.front();

  if (ch) {
    if (found_complexes.size() > 1) {
      send_to_char(ch, "Please refine your query. The following complexes matched your search '%s':\r\n", name);
      for (auto &complex : found_complexes)
        send_to_char(ch, "  %s\r\n", complex->get_name());
      return NULL;
    }

    send_to_char(ch, "No complexes matched your search '%s'.\r\n", name);
    return NULL;
  }

  return NULL;
}

ApartmentComplex *get_complex_headed_by_landlord(vnum_t vnum) {
  for (auto &candidate_complex : global_apartment_complexes) {
    if (vnum == candidate_complex->get_landlord_vnum()) {
      return candidate_complex;
    }
  }
  return NULL;
}

Apartment *find_apartment(const char *full_name, struct char_data *ch) {
  std::vector<Apartment *> found_apartments = {};

  if (!*full_name) {
    if (ch && ch->in_room && GET_APARTMENT(ch->in_room)) {
      return GET_APARTMENT(ch->in_room);
    }
    send_to_char("You must supply an apartment name to look for.\r\n", ch);
    return NULL;
  }

  // Attempt to search for a shortname match in the complex we're standing in.
  if (ch->in_room) {
    ApartmentComplex *complex = NULL;

    if (GET_APARTMENT(ch->in_room)) {
      complex = GET_APARTMENT(ch->in_room)->get_complex();
    } else {
      for (struct char_data *tmp = ch->in_room->people; tmp; tmp = tmp->next_in_room) {
        if (IS_NPC(tmp) && MOB_HAS_SPEC(tmp, landlord_spec)) {
          complex = get_complex_headed_by_landlord(GET_MOB_VNUM(tmp));
          break;
        }
      }
    }

    // We're actually standing in one-- search it.
    if (complex) {
      for (auto &apartment : complex->get_apartments()) {
        // Exact match.
        if (!str_cmp(full_name, apartment->get_short_name()) || !str_cmp(full_name, apartment->get_name()))
          return apartment;

        // Imprecise match.
        if (is_abbrev(full_name, apartment->get_short_name()) || is_abbrev(full_name, apartment->get_name()))
          found_apartments.push_back(apartment);
      }

      if (found_apartments.size() == 1)
        return found_apartments.front();

      found_apartments.clear();

      send_to_char(ch, "Didn't find an apartment matching '%s' in %s. Trying globally...\r\n", full_name, complex->get_name());
    }
  }

  for (auto &complex : global_apartment_complexes) {
    for (auto &apartment : complex->get_apartments()) {
      // Exact match.
      if (!str_cmp(full_name, apartment->get_full_name()))
        return apartment;

      // Imprecise match.
      if (is_abbrev(full_name, apartment->get_full_name()) || is_abbrev(full_name, apartment->get_short_name()) || is_abbrev(full_name, apartment->get_name()))
        found_apartments.push_back(apartment);
    }
  }

  if (found_apartments.size() == 1)
    return found_apartments.front();

  if (ch) {
    if (found_apartments.size() > 1) {
      send_to_char(ch, "Please refine your query. The following apartments matched your full-name search '%s':\r\n", full_name);
      for (auto &apartment : found_apartments)
        send_to_char(ch, "  %s\r\n", apartment->get_full_name());
      return NULL;
    }

    send_to_char(ch, "No apartments matched your full-name search '%s'. Remember to include the possessive complex name too, like [Evergreen Multiplex's Unit 3A].\r\n", full_name);
    return NULL;
  }

  return NULL;
}

void write_json_file(bf::path path, json *contents) {
  log_vfprintf("write_json_file(%s, ...): Beginning to write.", path.c_str());
  bf::ofstream ofs(path);
  ofs << std::setw(4) << *contents << std::endl;
  ofs.close();
}

void _json_parse_from_file(bf::path path, json &target) {
  if (!exists(path)) {
    log_vfprintf("FATAL ERROR: Unable to find file at path %s. Terminating.", path.c_str());
    exit(1);
  } else {
    log_vfprintf("Reading JSON data from %s.", path.c_str());
  }

  bf::ifstream f(path);
  target = json::parse(f);
  f.close();
}

void globally_rewrite_room_to_apartment_pointers() {
  for (auto &complex : global_apartment_complexes) {
    for (auto &apartment : complex->get_apartments()) {
      for (auto &room : apartment->get_rooms()) {
        rnum_t rnum = real_room(room->get_vnum());
        if (rnum < 0) {
          mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Previously-valid apartment room %ld from %s no longer exists!", room->get_vnum(), apartment->get_full_name());
        } else {
          world[rnum].apartment = apartment;
          world[rnum].apartment_room = room;
        }
      }
    }
  }
}

bool apartment_sort_func(Apartment *i1, Apartment *i2) {
  return strcmp(i1->get_name(), i2->get_name()) < 0;
}

bool apartment_complex_sort_func(ApartmentComplex *i1, ApartmentComplex *i2) {
  return strcmp(i1->get_name(), i2->get_name()) < 0;
}

//////////////////////////// The Landlord spec. ///////////////////////////////

SPECIAL(landlord_spec)
{
  class ApartmentComplex *complex;
  struct char_data *recep = (struct char_data *) me;
  char say_string[500];

  if (!(CMD_IS("list") || CMD_IS("retrieve") || CMD_IS("lease")
        || CMD_IS("leave") || CMD_IS("break") || CMD_IS("pay") || CMD_IS("status")
        || CMD_IS("info") || CMD_IS("check") || CMD_IS("payout")))
    return FALSE;

  if (!CAN_SEE(recep, ch)) {
    act("You're having a hard time getting $S attention.", FALSE, ch, 0, recep, TO_CHAR);
    return TRUE;
  }

  // Iterate through the apartment complexes and find the one headed by this landlord.
  if (!(complex = get_complex_headed_by_landlord(GET_MOB_VNUM(recep)))) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Could not find apartment complex for landlord %s (%ld)!", GET_CHAR_NAME(recep), GET_MOB_VNUM(recep));
    return FALSE;
  }

  if (CMD_IS("list")) {
    complex->display_room_list_to_character(ch);
    return TRUE;
  }

  one_argument(argument, arg);

  if (CMD_IS("retrieve")) {
    if (!*arg) {
      send_to_char("Syntax: ^WRETRIEVE <unit name>^n\r\n", ch);
      return TRUE;
    }

    for (auto &apartment : complex->get_apartments()) {
      if (is_abbrev(arg, apartment->get_name()) || is_abbrev(arg, apartment->get_short_name())) {
        if (!apartment->has_owner_privs(ch)) {
          snprintf(say_string, sizeof(say_string), "You're not the owner of %s.", apartment->get_name());
          mob_say(recep, say_string);
        } else {
          mudlog_vfprintf(NULL, LOG_GRIDLOG, "%s collected a key for %s.", GET_CHAR_NAME(ch), apartment->get_full_name());
          mob_say(recep, "Here you go...");
          apartment->issue_key(ch);
        }
        return TRUE;
      }
    }

    mob_say(recep, "Sorry, I don't recognize that unit name.");
    return TRUE;
  }

  else if (CMD_IS("lease")) {
    if (!*arg) {
      mob_say(recep, "Which room would you like to lease?");
      complex->display_room_list_to_character(ch);
      return TRUE;
    }

    for (auto &apartment : complex->get_apartments()) {
      if (is_abbrev(arg, apartment->get_name()) || is_abbrev(arg, apartment->get_short_name())) {
        if (apartment->has_owner()) {
          snprintf(say_string, sizeof(say_string), "Sorry, I'm afraid room %s is already taken.", apartment->get_name());
          mob_say(recep, say_string);
        } else if (complex->ch_already_rents_here(ch)) {
          mob_say(recep, "Sorry, we only allow people to lease one room at a time here.");
        } else {
          if (apartment->create_or_extend_lease(ch)) {
            mudlog_vfprintf(NULL, LOG_GRIDLOG, "%s leased %s.", GET_CHAR_NAME(ch), apartment->get_full_name());
            mob_say(recep, "Thank you, here is your key.");
            apartment->issue_key(ch);
          }
        }
        return TRUE;
      }
    }

    mob_say(recep, "Sorry, I don't recognize that unit name.");
    return TRUE;
  }

  else if (CMD_IS("leave") || CMD_IS("break")) {
    if (!*arg) {
      send_to_char("Syntax: ^WBREAK <unit name>^n\r\n", ch);
      return TRUE;
    }

    for (auto &apartment : complex->get_apartments()) {
      if (is_abbrev(arg, apartment->get_name()) || is_abbrev(arg, apartment->get_short_name())) {
        if (!apartment->has_owner()) {
          snprintf(say_string, sizeof(say_string), "Nobody's rented out %s yet.", apartment->get_name());
          mob_say(recep, say_string);
        } else if (!apartment->has_owner_privs(ch)) {
          snprintf(say_string, sizeof(say_string), "I would get fired if I let you break the lease on %s.", apartment->get_name());
          mob_say(recep, say_string);
        } else {
          mudlog_vfprintf(NULL, LOG_GRIDLOG, "%s broke their lease on %s.", GET_CHAR_NAME(ch), apartment->get_full_name());
          apartment->break_lease();
          mob_say(recep, "I hope you enjoyed your time here.");
        }
        return TRUE;
      }
    }

    mob_say(recep, "Sorry, I don't recognize that unit name.");
    return TRUE;
  }

  // 'payout' is here so staff can pay for rooms too.
  else if (CMD_IS("pay") || CMD_IS("payout")) {
    if (!*arg) {
      send_to_char("Syntax: ^WPAY <unit name>^n\r\n", ch);
      return TRUE;
    }

    for (auto &apartment : complex->get_apartments()) {
      if (is_abbrev(arg, apartment->get_name()) || is_abbrev(arg, apartment->get_short_name())) {
        if (access_level(ch, LVL_FIXER)) {
          if (apartment->create_or_extend_lease(ch)) {
            mudlog_vfprintf(ch, LOG_WIZLOG, "Paid another month on %s. New expiry: %ld.", apartment->get_full_name(), apartment->get_paid_until());
            mob_say(recep, "You are paid up for the next period.");
          }
        }
        if (!apartment->has_owner_privs(ch) && !apartment->is_guest(GET_IDNUM(ch))) {
          snprintf(say_string, sizeof(say_string), "You aren't an owner or guest of %s.", apartment->get_name());
          mob_say(recep, say_string);
        } else {
          if (apartment->create_or_extend_lease(ch)) {
            mudlog_vfprintf(ch, LOG_GRIDLOG, "Paid another month on %s. New expiry: %ld.", apartment->get_full_name(), apartment->get_paid_until());
            mob_say(recep, "You are paid up for the next period.");
          }
        }
        return TRUE;
      }
    }

    mob_say(recep, "Sorry, I don't recognize that unit name.");
    return TRUE;
  }

  else if (CMD_IS("status") || CMD_IS("info") || CMD_IS("check")) {
    if (!*arg) {
      send_to_char("Syntax: ^WSTATUS <unit name>^n\r\n", ch);
      complex->list_owned_apartments_to_ch(ch);
      return TRUE;
    }

    for (auto &apartment : complex->get_apartments()) {
      if (is_abbrev(arg, apartment->get_name()) || is_abbrev(arg, apartment->get_short_name())) {
        if (!apartment->has_owner()) {
          snprintf(say_string, sizeof(say_string), "%s is currently available for lease.", CAP(apartment->get_name()));
          mob_say(recep, say_string);
        } else if (!apartment->has_owner_privs(ch) && !apartment->is_guest(GET_IDNUM(ch))) {
          snprintf(say_string, sizeof(say_string), "%s has been leased.", CAP(apartment->get_name()));
          mob_say(recep, say_string);
        } else {
          if (apartment->get_paid_until() - time(0) < 0) {
            int days_in_arrears = apartment->get_days_in_arrears();

            if (days_in_arrears > 0) {
              snprintf(say_string, sizeof(say_string), "%s's rent is %d day%s in arrears. You'll have to PAY it to regain access.", 
                       CAP(apartment->get_name()),
                       days_in_arrears,
                       days_in_arrears == 1 ? "" : "s");
            } else {
              snprintf(say_string, sizeof(say_string), "%s's rent has expired. You'll have to ^WPAY%s it to regain access.", 
                       CAP(apartment->get_name()),
                       GET_CHAR_COLOR_HIGHLIGHT(ch));
            }
            mob_say(recep, say_string);
          } else {
            int days = (int)((apartment->get_paid_until() - time(0)) / SECS_PER_REAL_DAY);
            int hours = (int)((apartment->get_paid_until() - time(0)) / SECS_PER_REAL_HOUR);
            int minutes = (int)((apartment->get_paid_until() - time(0)) / SECS_PER_REAL_MIN);
            int seconds = apartment->get_paid_until() - time(0);
            
            if (days > 0) {
              snprintf(buf2, sizeof(buf2), "%s is paid up for another %d day%s.", CAP(apartment->get_name()), days, days == 1 ? "" : "s");
            } else if (hours > 0) {
              snprintf(buf2, sizeof(buf2), "%s is paid up for another %d hour%s.", CAP(apartment->get_name()), hours, hours == 1 ? "" : "s");
            } else if (minutes > 0) {
              snprintf(buf2, sizeof(buf2), "%s is paid up for another %d minute%s.", CAP(apartment->get_name()), minutes, minutes == 1 ? "" : "s");
            } else {
              snprintf(buf2, sizeof(buf2), "%s is paid up for another %d second%s.", CAP(apartment->get_name()), seconds, seconds == 1 ? "" : "s");
            }
            mob_say(recep, buf2);
            strlcpy(buf2, "(Note: That's in real-world time.)", sizeof(buf2));
            do_say(recep, buf2, 0, SCMD_OSAY);
          }
        }

        if (IS_SENATOR(ch)) {
          send_to_char(ch, "(staff info: owner %ld, lease %ld [%ld secs left])\r\n", 
                        apartment->get_owner_id(), 
                        apartment->get_paid_until(), 
                        apartment->get_paid_until() - time(0));
        }
        
        return TRUE;
      }
    }

    mob_say(recep, "Sorry, I don't recognize that unit name.");
    return TRUE;
  }
  return FALSE;
}
