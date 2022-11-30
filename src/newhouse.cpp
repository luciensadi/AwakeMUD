#include <fstream>
#include <iostream>

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
#include "newhouse.hpp"

#include "nlohmann/json.hpp"
using nlohmann::json;

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

std::vector<ApartmentComplex*> global_apartment_complexes = {};

const bf::path global_housing_dir = bf::system_complete("lib") / "housing";

// TODO: Restore functionality to hcontrol command

// TODO: Add apartmentroom saving: vnum (for delete/create); decoration (for decorate)
// Decoration could just write a flat file with the text of the decoration and nothing else.

// TODO: Mail player owner when a lease is broken.
// EVENTUAL TODO: Write pgroup log when a lease is broken.

// TODO: The decorate command must enforce two initial spaces and ^n\r\n at end.

// TODO: When a pgroup has no members left, it should be auto-disabled.

// TODO: if IS_BUILDPORT, find_pgroup must always return a group instance with the requested idnum, even if it doesn't exist

// TODO: Verify that an apartment owned by non-buildport player id X is not permanently borked when loaded on the buildport and then transferred back to main
// TODO: same as above, but for pgroups instead

// EVENTUAL TODO: Add logic for mailing pgroup officers with apartment deletion warnings.

// EVENTUAL TODO: Sanity checks for things like reused vnums, etc.

// EVENTUAL TODO: When purging contents and encoutering belongings, move them somewhere safe.

ACMD(do_decorate) {
  extern void write_world_to_disk(int vnum);

  FAILURE_CASE(ch->in_veh, "You can't decorate the interior of vehicles, but you can still customize the outside by visiting a painting booth.\r\n");
  FAILURE_CASE(!ch->in_room || !ch->in_room->apartment, "You must be in an apartment to decorate it.");
  FAILURE_CASE(!ch->in_room->apartment->has_owner_privs(ch), "You must be the owner of this apartment to decorate it.")

  PLR_FLAGS(ch).SetBit(PLR_WRITING);
  send_to_char("Enter your new room description. Terminate with a @ on a new line. Your new desc will be automatically indented and newline-terminated.\r\n", ch);
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

  FAILURE_CASE(!ch->in_room || !ch->in_room->apartment, "You must be in your house to set guests.");
  FAILURE_CASE(!ch->in_room->apartment->has_owner_privs(ch), "You're not an owner or landlord here.");

  if (!*arg) {
    ch->in_room->apartment->list_guests_to_char(ch);
    return;
  }

  // They're adding or deleting a guest. Look them up.
  idnum_t idnum = get_player_id(arg);

  FAILURE_CASE(idnum < 0, "No such player.");
  FAILURE_CASE(player_is_dead_hardcore(idnum), "Permanently-dead characters can't be added as guests.");
  FAILURE_CASE(idnum == GET_IDNUM(ch), "You can't add yourself as a guest.");

  if (ch->in_room->apartment->delete_guest(idnum)) {
    send_to_char("Guest deleted.\r\n", ch);
  } else {
    ch->in_room->apartment->add_guest(idnum);
    send_to_char("Guest added.\r\n", ch);
  }
}

void load_apartment_complexes() {
  // Iterate over the contents of the lib/housing directory.
  if (!exists(global_housing_dir)) {
    log("FATAL ERROR: Unable to find lib/housing. Terminating.");
    log_vfprintf("Rendered path: %s", global_housing_dir.string().c_str());
    exit(1);
  }

  log("Loading apartment complexes:");

  bf::directory_iterator end_itr; // default construction yields past-the-end
  for (bf::directory_iterator itr(global_housing_dir); itr != end_itr; ++itr) {
    if (is_directory(itr->status())) {
      bf::path filename = itr->path();
      log_vfprintf(" - Initializing apartment complex from file %s.", filename.string().c_str());
      ApartmentComplex *complex = new ApartmentComplex(filename);
      global_apartment_complexes.push_back(complex);
      log_vfprintf(" - Fully loaded %s.", complex->get_name());
    }
  }
}

void save_all_apartments_and_storage_rooms() {
  for (auto &complex : global_apartment_complexes) {
    log_vfprintf("Saving complex %s...", complex->get_name());
    for (auto &apartment : complex->get_apartments()) {
      // Skip non-leased apartments.
      if (apartment->get_paid_until() == 0)
        continue;

#ifndef IS_BUILDPORT
      // Invalid owner? Break it and bail.
      if (!apartment->has_owner() || !apartment->owner_is_valid()) {
        mudlog_vfprintf(NULL, LOG_GRIDLOG, "Breaking lease on %s: No owner, or owner is no longer valid.", apartment->get_full_name());
        apartment->break_lease();
        continue;
      }

      // Invalid lease? Break it and bail.
      if (apartment->get_paid_until() < time(0)) {
        mudlog_vfprintf(NULL, LOG_GRIDLOG, "Breaking lease on %s: Lease expired.", apartment->get_full_name());
        apartment->break_lease();
        continue;
      }
#endif

      log_vfprintf("Saving apartment %s...", apartment->get_name());

      // Otherwise, save all the rooms (save_storage logic will skip any that haven't been altered)
      for (auto &room : apartment->get_rooms()) {
        log_vfprintf("Saving subroom %s...", room->get_name());
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

      if (Storage_get_filename(GET_ROOM_VNUM(&world[x]), filename, sizeof(filename))) {
        Storage_save(filename, room);
      } else {
        mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Unable to save storage room %ld: Can't get filename!", GET_ROOM_VNUM(room));
      }
    }
  }
}

/*********** ApartmentComplex ************/
ApartmentComplex::ApartmentComplex(bf::path filename) :
  base_directory(filename)
{
  // Load info from <filename>/info
  {
    json base_info;
    _json_parse_from_file(base_directory / "complex_info.json", base_info);

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
        log_vfprintf(" --- Initializing apartment %s.", room_path.filename().string().c_str());
        Apartment *apartment = new Apartment(this, room_path);
        apartments.push_back(apartment);
        log_vfprintf(" --- Fully loaded %s.", apartment->get_name());
      }
    }
  }
}

void ApartmentComplex::save() {
  // Ensure our base directory exists.
  if (!bf::exists(base_directory)) {
    bf::create_directory(base_directory);
  }

  // Compose the info file.
  json base_info;

  base_info["display_name"] = std::string(display_name);
  base_info["landlord_vnum"] = landlord_vnum;
  base_info["editors"] = editors;

  // Write it out.
  write_json_file(base_directory / "complex_info.json", &base_info);
}

void ApartmentComplex::mark_as_deleted() {
  char deleted_name[500];
  snprintf(deleted_name, sizeof(deleted_name), "deleted-complex-%ld-%s", time(0), display_name);
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
    const char *name = get_player_name(editor_id);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^c%s^n", printed_anything ? ", " : "", name);
    delete [] name;
    printed_anything = TRUE;
  }

  if (!printed_anything)
    strlcpy(buf, "Nobody.", sizeof(buf));

  return buf;
}

bool ApartmentComplex::can_houseedit_complex(struct char_data *ch) {
  if (GET_LEVEL(ch) >= LVL_ADMIN)
    return TRUE;

  if (find(editors.begin(), editors.end(), GET_IDNUM(ch)) != editors.end())
    return TRUE;

  return TRUE;
}

void ApartmentComplex::display_room_list_to_character(struct char_data *ch) {
  std::vector<Apartment*> available_apartments = {};
  std::vector<Apartment*> owned_apartments = {};

  if (apartments.empty()) {
    send_to_char(ch, "It doesn't look like there are any rooms to rent at all.\r\n");
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Found ZERO apartments in %s!", display_name);
    return;
  }

  for (auto &apartment : apartments) {
    if (!apartment->has_owner() || !apartment->owner_is_valid()) {
      available_apartments.push_back(apartment);
      continue;
    }

    if (apartment->has_owner_privs(ch)) {
      owned_apartments.push_back(apartment);
      continue;
    }
  }

  if (!owned_apartments.empty()) {
    bool first_print = TRUE;
    send_to_char(ch, "You have ownership control over the following apartment%s: ", owned_apartments.size() == 1 ? "" : "s");
    for (auto &apartment : apartments) {
      send_to_char(ch, "%s%s", first_print ? "" : ", ", apartment->get_name());
      first_print = FALSE;
    }
    send_to_char("\r\n", ch);
  }

  if (available_apartments.empty()) {
    send_to_char(ch, "It looks like all the rooms here have been claimed.\r\n");
    return;
  }

  send_to_char(ch, "The following rooms are free: \r\n");

  if (!PRF_FLAGGED(ch, PRF_SCREENREADER)) {
    send_to_char(ch, "Name                           Lifestyle      Rooms   Garages  Price  \r\n");
    send_to_char(ch, "----------------------------   ------------   -----   -------  -------\r\n");
  }

  bool color_toggle = TRUE;
  for (auto &apartment : available_apartments) {
    int non_garage = apartment->rooms.size() - apartment->garages;

    if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
      send_to_char(ch, "%s (lifestyle %s, %d room%s, of which %d %s): %d nuyen.\r\n",
                   apartment->name,
                   "placeholder", // lifestyles[apartment->lifestyle].name,
                   apartment->rooms.size(),
                   apartment->rooms.size() == 1 ? "" : "s",
                   apartment->garages,
                   apartment->garages == 1 ? "is a garage" : "are garages",
                   apartment->get_rent_cost());
    } else {
      send_to_char(ch, "%s%-28s%s   %-12s   %-5d   %-7d  %ld\r\n",
                   color_toggle ? "^W" : "^w",
                   apartment->name,
                   color_toggle ? "^W" : "^w",
                   "placeholder", // lifestyles[apartment->lifestyle].name,
                   non_garage,
                   apartment->garages,
                   apartment->get_rent_cost());
    }
    color_toggle = !color_toggle;
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
          log_vfprintf("SYSERR: Assigning too many specs to mob #%d. Losing one.", source->landlord_vnum);
        }
        mob_index[rnum].sfunc = mob_index[rnum].func;
        mob_index[rnum].func = landlord_spec;
      }
    }
  }

  landlord_vnum = source->landlord_vnum;

  base_directory = source->base_directory;

  editors.clear();
  for (auto idnum : source->editors) {
    editors.push_back(idnum);
  }
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
/*********** Apartment ************/

/* Load this apartment entry from files. */
Apartment::Apartment(ApartmentComplex *complex, bf::path base_directory) :
  base_directory(base_directory), complex(complex)
{
  // Load base info from <name>/info.
  {
    log_vfprintf(" ---- Loading apartment base data for %s.", base_directory.filename().string().c_str());
    json base_info;
    _json_parse_from_file(base_directory / "apartment_info.json", base_info);

    shortname = str_dup(base_info["short_name"].get<std::string>().c_str());
    name = str_dup(base_info["name"].get<std::string>().c_str());
    lifestyle = base_info["lifestyle"].get<int>();
    nuyen_per_month = base_info["rent"].get<long>();

    atrium = base_info["atrium"].get<vnum_t>();
    key_vnum = base_info["key"].get<vnum_t>();
  }

  // Load lease info from <name>/lease.
  {
    log_vfprintf(" ---- Loading apartment lease data for %s.", name);
    json base_info;
    _json_parse_from_file(base_directory / "lease.json", base_info);

    paid_until = base_info["paid_until"].get<time_t>();
    guests = base_info["guests"].get<std::vector<idnum_t>>();

    idnum_t owner = base_info["owner"].get<idnum_t>();
    if (owner < 0) {
      // Negative numbers indicate pgroup ownership
      owned_by_pgroup = Playergroup::find_pgroup(-1 * owner);
    } else {
      owned_by_player = owner;
#ifndef IS_BUILDPORT
      if (!does_player_exist(owner) || player_is_dead_hardcore(owner)) {
        // Unit's not valid.
        break_lease();
      }
#endif
    }
  }

  // Load sub-rooms.
  {
    log_vfprintf(" ---- Loading sub-rooms for %s.", name);
    bf::directory_iterator end_itr; // default construction yields past-the-end
    for (bf::directory_iterator itr(base_directory); itr != end_itr; ++itr) {
      if (is_directory(itr->status())) {
        bf::path room_path = itr->path();
        log_vfprintf(" ----- Initializing sub-room '%s'.", room_path.filename().string().c_str());
        ApartmentRoom *apartment_room = new ApartmentRoom(this, room_path);
        rooms.push_back(apartment_room);
        log_vfprintf(" ----- Fully loaded %s's %s at %ld.", name, room_path.filename().string().c_str(), apartment_room->get_vnum());
      }
    }
  }

  // Calculate any derived data.
  char tmp_buf[100];
  snprintf(tmp_buf, sizeof(tmp_buf), "%s's %s", complex->display_name, name);
  full_name = str_dup(tmp_buf);
}

bool Apartment::owner_is_valid() {
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

/* Write lease data to <apartment name>/lease. */
void Apartment::save_lease() {
  json lease_data;

  lease_data["owner"] = get_owner_id();
  lease_data["paid_until"] = paid_until;
  lease_data["guests"] = guests;

  bf::ofstream o(base_directory / "lease.json");
  o << std::setw(4) << lease_data << std::endl;
  o.close();
}

/* Delete <apartment name>/lease and restore default descs. */
void Apartment::break_lease() {
  mudlog_vfprintf(NULL, LOG_GRIDLOG, "Cleaning up lease for %s's %s.", complex->display_name, name);

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
  }
}

/* Check for entry permissions. */
bool Apartment::can_enter(struct char_data *ch) {
  // NPC, but not a spirit or elemental? No entry.
  if (IS_NPC(ch) && !(IS_SPIRIT(ch) || IS_PC_CONJURED_ELEMENTAL(ch)))
    return FALSE;

  // Admins, astral projections, and owners can enter any room.
  if (GET_LEVEL(ch) >= LVL_ADMIN || IS_ASTRAL(ch))
    return TRUE;

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
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: %s is owned by neither player nor group! Allowing entry.", full_name);
    return TRUE;
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

  // Check for owner status or pgroup perms.
  if (owned_by_player > 0) {
    if (idnum == owned_by_player)
      return TRUE;
  } else if (owned_by_pgroup) {
    required_privileges.SetBits(PRIV_LEADER, PRIV_LANDLORD, PRIV_TENANT, ENDBIT);
    if (pgroup_char_has_any_priv(idnum, owned_by_pgroup->get_idnum(), required_privileges))
      return TRUE;
  } else {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: %s is owned by neither player nor group! Allowing entry.", full_name);
    return TRUE;
  }

  // Guests can enter any room.
  for (auto guest_idnum : guests) {
    if (idnum == guest_idnum)
      return TRUE;
  }

  return FALSE;
}

bool Apartment::create_or_extend_lease(struct char_data *ch) {
  struct obj_data *neophyte_card;
  int cost = nuyen_per_month;

  // Subtract subsidy card amounts.
  for (neophyte_card = ch->carrying; neophyte_card; neophyte_card = neophyte_card->next_content) {
    if (GET_OBJ_VNUM(neophyte_card) == OBJ_NEOPHYTE_SUBSIDY_CARD
        && GET_OBJ_VAL(neophyte_card, 0) == GET_IDNUM(ch))
    {
      if (cost >= GET_OBJ_VAL(neophyte_card, 1))
        cost -= GET_OBJ_VAL(neophyte_card, 1);
      else
        cost = 0;
      break;
    }
  }

  // If they can't cover it in cash, draw from bank.
  if (GET_NUYEN(ch) < cost) {
    if (GET_BANK(ch) >= cost) {
      send_to_char("You arrange for a bank transfer to cover what you can't pay from your cash on hand.\r\n", ch);
      lose_bank(ch, cost, NUYEN_OUTFLOW_HOUSING);
    } else {
      send_to_char(ch, "You don't have the %d nuyen required.\r\n", nuyen_per_month);
      return FALSE;
    }
  } else {
    lose_nuyen(ch, cost, NUYEN_OUTFLOW_HOUSING);
    send_to_char(ch, "You hand over %d nuyen.\r\n", nuyen_per_month);
  }

  if (neophyte_card) {
    if (nuyen_per_month >= GET_OBJ_VAL(neophyte_card, 1)) {
      send_to_char(ch, "%s is now empty, so you junk it.\r\n", GET_OBJ_NAME(neophyte_card));
      extract_obj(neophyte_card);
    } else {
      GET_OBJ_VAL(neophyte_card, 1) -= nuyen_per_month;
      send_to_char(ch, "Your housing card has %d nuyen left on it.\r\n", GET_OBJ_VAL(neophyte_card, 1));
    }
  }

  if (!issue_key(ch)) {
    send_to_char(ch, "(OOC: Oops, looks like that apartment is broken. You've been refunded %d nuyen in cash.)\r\n", nuyen_per_month);
    GET_NUYEN_RAW(ch) += nuyen_per_month;
    GET_NUYEN_INCOME_THIS_PLAY_SESSION(ch, NUYEN_OUTFLOW_HOUSING) -= nuyen_per_month;
    return FALSE;
  }

  if (!has_owner()) {
    owned_by_player = GET_IDNUM(ch);
    paid_until = time(0) + (SECS_PER_REAL_DAY*30);
  } else {
    paid_until += SECS_PER_REAL_DAY*30;
  }

  save_lease();
  return TRUE;
}

bool Apartment::issue_key(struct char_data *ch) {
  struct obj_data *key = read_object(key_vnum, VIRTUAL);

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
  } else {
    return GET_IDNUM(ch) == owned_by_player;
  }

  return FALSE;
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
  }
}

// Returns new-- must delete output!
const char * Apartment::get_owner_name__returns_new() {
  if (owned_by_pgroup) {
    char pgroup_buf[1000];
    snprintf(pgroup_buf, sizeof(pgroup_buf), "Group %s^n", owned_by_pgroup->get_name());
    return str_dup(pgroup_buf);
  } else {
    return get_player_name(owned_by_player);
  }
}

// Returns new-- must delete output!
const char *Apartment::list_rooms__returns_new(bool indent) {
  char result[MAX_STRING_LENGTH];
  result[0] = '\0';
  struct room_data *world_room;

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

const char *Apartment::get_lifestyle_string() {
  return "n/a";
}

/********** ApartmentRoom ************/

ApartmentRoom::ApartmentRoom(Apartment *apartment, bf::path filename) :
  name(str_dup(filename.filename().string().c_str())), base_path(filename), apartment(apartment)
{
  // Read from info.json.
  json base_info;
  _json_parse_from_file(filename / "room_info.json", base_info);

  vnum = base_info["vnum"].get<vnum_t>();

  rnum_t rnum = real_room(vnum);
  if (vnum < 0 || rnum < 0) {
    log_vfprintf("SYSERR: Invalid vnum %ld specified in %s.", filename.string().c_str());
    exit(1);
  }

  // Read decoration from standalone file.
  bf::ifstream ifs(base_path / "decoration.txt");
  std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
  ifs.close();
  decoration = str_dup(content.c_str());

  log_vfprintf(" ----- Applying changes from %s to world...", filename.filename().string().c_str());

  struct room_data *room = &world[rnum];

  if (room->apartment) {
    log_vfprintf("ERROR: Room %ld already had an apartment entry! Terminating.", GET_ROOM_VNUM(room));
    exit(1);
  }

  // Set apartment pointer, etc.
  room->apartment = this;

  // Load storage contents.
  storage_path = filename / "storage";
  load_storage();
}

void ApartmentRoom::save_info() {
  json info_data;

  info_data["vnum"] = vnum;

  bf::ofstream o(base_path / "info.json");
  o << std::setw(4) << info_data << std::endl;
  o.close();
}

void ApartmentRoom::save_decoration() {
  bf::ofstream o(base_path / "decoration.txt");
  o << decoration << std::endl;
  o.close();
}

void ApartmentRoom::set_decoration(const char *new_desc) {
  delete [] decoration;

  // TODO: Apply consistent formatting, terminate color code

  decoration = str_dup(new_desc);
}

/* Purge the contents of this apartment, logging as we go. */
void ApartmentRoom::purge_contents() {
  rnum_t rnum = real_room(vnum);
  bool had_anything = FALSE;

  if (rnum < 0) {
    // mudlog_vfprintf(NULL, LOG_SYSLOG, "Refusing to purge contents for %ld: Invalid vnum", vnum);
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
  Storage_save(expired_storage_path.string().c_str(), room);
#endif

  if (room->contents) {
    had_anything = TRUE;

    // Purge all contents, logging as we go.
    int total_value_destroyed = 0;
    for (struct obj_data *obj = room->contents, *next_o; obj; obj = next_o) {
      next_o = obj->next_content;

      const char *representation = generate_new_loggable_representation(obj);

      // If this is belongings, don't purge it.
      if (GET_OBJ_TYPE(obj) == ITEM_CONTAINER && obj->obj_flags.extra_flags.IsSet(ITEM_EXTRA_CORPSE)) {
        mudlog_vfprintf(NULL, LOG_PURGELOG, "Apartment cleanup for %s (%ld) REFUSING to purge corpse %s.", apartment->full_name, GET_ROOM_VNUM(room), representation);
      } else {
        mudlog_vfprintf(NULL, LOG_PURGELOG, "Apartment cleanup for %s (%ld) has purged %s (cost: %d).", apartment->full_name, GET_ROOM_VNUM(room), representation, GET_OBJ_COST(obj));
        total_value_destroyed += GET_OBJ_COST(obj);
        extract_obj(obj);
      }

      delete [] representation;
    }
    mudlog_vfprintf(NULL, LOG_PURGELOG, "Total value destroyed: %d nuyen.", total_value_destroyed);
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
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Refusing to clear room desc for %ld: Invalid vnum", vnum);
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

/* Write storage data to <apartment name>/storage. This is basically the existing houses/<vnum> file. */
void ApartmentRoom::save_storage() {
  struct room_data *room = &world[real_room(vnum)];

  // If the dirty bit is not set, we don't save the contents-- it means nobody is or was here since last save.
  // YES, this does induce bugs, like evaluate programs not decaying if nobody is around to see it happen--
  // but fuck it, if someone exploits it we'll just ban them. Easy enough.
  if (!room->dirty_bit && !room->people) {
    log_vfprintf("Refusing to save room %s (%ld): Not dirty, no people.", GET_ROOM_NAME(room), GET_ROOM_VNUM(room));
    return;
  }

  // Clear the dirty bit now that we've processed it.
  room->dirty_bit = FALSE;

  // Write the file.
  Storage_save(storage_path.string().c_str(), room);
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
    mudlog_vfprintf(NULL, LOG_PURGELOG, "Purging room contents in preparation for storage load: %s", representation);
    extract_obj(room->contents);
    delete [] representation;
  }

  // Perform load.
  House_load_storage(room, path.string().c_str());
}

bool ApartmentRoom::is_guest(idnum_t idnum) {
  auto it = find(apartment->guests.begin(), apartment->guests.end(), idnum);
  return (it != apartment->guests.end());
}

bool ApartmentRoom::delete_guest(idnum_t idnum) {
  auto it = find(apartment->guests.begin(), apartment->guests.end(), idnum);
  if (it != apartment->guests.end()) {
    apartment->guests.erase(it);
    apartment->save_lease();
    return TRUE;
  }
  return FALSE;
}

void ApartmentRoom::add_guest(idnum_t idnum) {
  if (!is_guest(idnum)) {
    apartment->guests.push_back(idnum);
    apartment->save_lease();
  }
}

struct room_data *ApartmentRoom::get_world_room() {
  rnum_t rnum = real_room(vnum);

  if (rnum < 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: %s's %s has invalid vnum %ld!", get_full_name(), get_name(), vnum);
    return NULL;
  }

  return &world[rnum];
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
  }
  save_vehicles(FALSE);
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

      if (days_until_deletion > 5) {
        //log_vfprintf("House %s is OK-- %d days left (%d - %d)", apartment->get_full_name(), days_until_deletion, apartment->get_paid_until(), time(0));
        continue;
      }

      mudlog_vfprintf(NULL, LOG_GRIDLOG, "Sending %d-day rent warning message for apartment %s to %ld.",
                      days_until_deletion,
                      apartment->get_full_name(),
                      apartment->get_owner_id());

      if (apartment->get_owner_pgroup()) {
        // TODO
      } else {
        if (days_until_deletion > 0) {
          snprintf(buf, sizeof(buf), "Remember to pay your rent for apartment %s^n! It will be deemed abandoned and its contents reclaimed ^Rat any time^n.\r\n",
                   apartment->get_full_name());
        } else {
          snprintf(buf, sizeof(buf), "Remember to pay your rent for apartment %s^n. It will be deemed abandoned and its contents reclaimed in %d days.\r\n",
                   apartment->get_full_name(),
                   days_until_deletion);
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
  std::vector<ApartmentComplex *> found_complexes = {};

  for (auto &complex : global_apartment_complexes) {
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

void write_json_file(bf::path path, json *contents) {
  bf::ofstream ofs(path);
  ofs << std::setw(4) << *contents << std::endl;
  ofs.close();
}

void _json_parse_from_file(bf::path path, json &target) {
  if (!exists(path)) {
    log_vfprintf("FATAL ERROR: Unable to find file at path %s. Terminating.");
    exit(1);
  }

  bf::ifstream f(path);
  target = json::parse(f);
  f.close();
}

//////////////////////////// The Landlord spec. ///////////////////////////////

SPECIAL(landlord_spec)
{
  class ApartmentComplex *complex;
  struct char_data *recep = (struct char_data *) me;
  char say_string[500];

  if (!(CMD_IS("list") || CMD_IS("retrieve") || CMD_IS("lease")
        || CMD_IS("leave") || CMD_IS("break") || CMD_IS("pay") || CMD_IS("status")))
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
        if (!apartment->has_owner_privs(ch)) {
          snprintf(say_string, sizeof(say_string), "I would get fired if I let you break the lease on %s.", apartment->get_name());
          mob_say(recep, say_string);
        } else {
          apartment->break_lease();
          mob_say(recep, "I hope you enjoyed your time here.");
        }
        return TRUE;
      }
    }

    mob_say(recep, "Sorry, I don't recognize that unit name.");
    return TRUE;
  }

  else if (CMD_IS("pay")) {
    if (!*arg) {
      send_to_char("Syntax: ^WPAY <unit name>^n\r\n", ch);
      return TRUE;
    }

    for (auto &apartment : complex->get_apartments()) {
      if (is_abbrev(arg, apartment->get_name()) || is_abbrev(arg, apartment->get_short_name())) {
        if (!apartment->has_owner_privs(ch)) {
          snprintf(say_string, sizeof(say_string), "You aren't the owner of %s.", apartment->get_name());
          mob_say(recep, say_string);
        } else {
          apartment->create_or_extend_lease(ch);
          mob_say(recep, "You are paid up for the next period.");
        }
        return TRUE;
      }
    }

    mob_say(recep, "Sorry, I don't recognize that unit name.");
    return TRUE;
  }

  else if (CMD_IS("status")) {
    if (!*arg) {
      send_to_char("Syntax: ^WSTATUS <unit name>^n\r\n", ch);
      return TRUE;
    }

    for (auto &apartment : complex->get_apartments()) {
      if (is_abbrev(arg, apartment->get_name()) || is_abbrev(arg, apartment->get_short_name())) {
        if (!apartment->has_owner()) {
          snprintf(say_string, sizeof(say_string), "%s is currently available for lease.", CAP(apartment->get_name()));
          mob_say(recep, say_string);
        } else if (!apartment->has_owner_privs(ch)) {
          snprintf(say_string, sizeof(say_string), "%s has been leased.", CAP(apartment->get_name()));
          mob_say(recep, say_string);
        } else {
          if (apartment->get_paid_until() - time(0) < 0) {
            snprintf(say_string, sizeof(say_string), "Your rent has expired on %s.", apartment->get_name());
            mob_say(recep, say_string);
          } else {
            snprintf(buf2, sizeof(buf2), "You are paid on %s for another %d days.", apartment->get_name(), (int)((apartment->get_paid_until() - time(0)) / 86400));
            mob_say(recep, buf2);
            strlcpy(buf2, "Note: Those are real-world days.", sizeof(buf2));
            do_say(recep, buf2, 0, SCMD_OSAY);
          }
        }
        return TRUE;
      }
    }

    mob_say(recep, "Sorry, I don't recognize that unit name.");
    return TRUE;
  }
  return FALSE;
}
