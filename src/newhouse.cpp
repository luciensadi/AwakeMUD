#include <fstream>
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
namespace bf = boost::filesystem;

#include "types.hpp"
#include "awake.hpp"
#include "structs.hpp"
#include "db.hpp"
#include "utils.hpp"
#include "playergroup_classes.hpp"
#include "handler.hpp"
#include "limits.hpp"
#include "interpreter.hpp"
#include "newhouse.hpp"

#include "nlohmann/json.hpp"
using nlohmann::json;

extern void ASSIGNMOB(long mob, SPECIAL(fname));
extern void Storage_save(const char *file_name, struct room_data *room);

SPECIAL(landlord_spec);

ACMD_DECLARE(do_say);

void remove_vehicles_from_apartment(struct room_data *room);

std::vector<ApartmentComplex> global_apartment_complexes = {};

void _json_parse_from_file(bf::path path, json &target) {
  if (!exists(path)) {
    log_vfprintf("FATAL ERROR: Unable to find file at path %s. Terminating.");
    exit(1);
  }

  log_vfprintf("[Parsing JSON file at %s]", path.string().c_str());

  bf::ifstream f(path);
  target = json::parse(f);
  f.close();
}

// TODO: Add administrative commands to load storage etc from files without a copyover.

void load_apartment_complexes() {
  // Iterate over the contents of the lib/housing directory.
  bf::path housing_dir = bf::system_complete("housing");

  if (!exists(housing_dir)) {
    log("FATAL ERROR: Unable to find lib/housing. Terminating.");
    log_vfprintf("Rendered path: %s", housing_dir.string().c_str());
    exit(1);
  }

  log("Loading apartment complexes:");

  bf::directory_iterator end_itr; // default construction yields past-the-end
  for (bf::directory_iterator itr(housing_dir); itr != end_itr; ++itr) {
    if (is_directory(itr->status())) {
      bf::path filename = itr->path();
      log_vfprintf(" - Initializing apartment complex from file %s.", filename.string().c_str());
      ApartmentComplex complex = ApartmentComplex(filename);
      global_apartment_complexes.push_back(complex);
      log_vfprintf(" - Fully loaded %s.", complex.get_name());
    }
  }

  // EVENTUAL TODO: Sanity checks for things like reused vnums, etc.
}

/*********** ApartmentComplex ************/
ApartmentComplex::ApartmentComplex(bf::path filename) :
  base_directory(filename)
{
  // Load info from <filename>/info
  {
    json base_info;
    _json_parse_from_file(base_directory / "info.json", base_info);

    display_name = str_dup(base_info["display_name"].get<std::string>().c_str());
    landlord_vnum = (vnum_t) base_info["landlord_vnum"].get<vnum_t>();

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
        log_vfprintf(" --- Initializing apartment from file %s.", room_path.string().c_str());
        Apartment apartment = Apartment(this, room_path);
        apartments.push_back(apartment);
        log_vfprintf(" --- Fully loaded %s.", apartment.get_name());
      }
    }
  }
}

void ApartmentComplex::display_room_list_to_character(struct char_data *ch) {
  std::vector<Apartment> available_apartments = {};

  if (apartments.empty()) {
    send_to_char(ch, "It doesn't look like there are any rooms to rent at all.\r\n");
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Found ZERO apartments in %s!", display_name);
    return;
  }

  for (auto apartment : apartments) {
    if (!apartment.has_owner()) {
      available_apartments.push_back(apartment);
    }
  }

  if (available_apartments.empty()) {
    send_to_char(ch, "It looks like all the rooms here have been claimed.\r\n");
    return;
  }

  send_to_char(ch, "The following rooms are free: \r\n");

  if (!PRF_FLAGGED(ch, PRF_SCREENREADER)) {
    send_to_char(ch, "Name        Lifestyle      Rooms   Garages  Price  \r\n");
    send_to_char(ch, "---------   ------------   -----   -------  -------\r\n");
  }

  for (auto apartment : available_apartments) {
    if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
      send_to_char(ch, "Unit %s (lifestyle %s, %d room%s, of which %d %s): %d nuyen.\r\n",
                   apartment.name,
                   "placeholder", // lifestyles[apartment.lifestyle].name,
                   apartment.rooms.size(),
                   apartment.rooms.size() == 1 ? "" : "s",
                   apartment.garages,
                   apartment.garages == 1 ? "is a garage" : "are garages",
                   apartment.get_rent_cost());
    } else {
      send_to_char(ch, "%-9s  %-12s  %-5d  %-7d  %ld\r\n",
                   apartment.name,
                   "placeholder", // lifestyles[apartment.lifestyle].name,
                   apartment.rooms.size(),
                   apartment.garages,
                   apartment.get_rent_cost());
    }
  }
}

bool ApartmentComplex::ch_already_rents_here(struct char_data *ch) {
  for (auto apartment: apartments) {
    if (apartment.owned_by_player == GET_IDNUM(ch))
      return TRUE;
  }
  return FALSE;
}
/*********** Apartment ************/

/* Load this apartment entry from files. */
Apartment::Apartment(ApartmentComplex *complex, bf::path base_directory) :
  complex(complex)
{
  // Load base info from <name>/info.
  {
    log_vfprintf(" ---- Loading apartment base data for %s.", base_directory.filename().string().c_str());
    json base_info;
    _json_parse_from_file(base_directory / "info.json", base_info);

    name = str_dup(base_info["name"].get<std::string>().c_str());
    lifestyle = base_info["lifestyle"].get<int>();
    nuyen_per_month = base_info["rent"].get<long>();

    atrium = base_info["atrium"].get<vnum_t>();
    key_vnum = base_info["key"].get<vnum_t>();

    exit_dir = base_info["exit_dir"].get<dir_t>();
  }

  // Load lease info from <name>/lease.
  {
    log_vfprintf(" ---- Loading apartment lease data for %s.", name);
    json base_info;
    _json_parse_from_file(base_directory / "lease.json", base_info);

    idnum_t owner = base_info["owner"].get<idnum_t>();
    if (owner < 0) {
      // TODO: Populate pgroup info from owner
    } else {
      owned_by_player = owner;
    }

    paid_until = base_info["paid_until"].get<time_t>();
    guests = base_info["guests"].get<std::vector<idnum_t>>();
  }

  // TODO: Load storage info from <name>/storage.
  {
    log_vfprintf(" ---- Loading apartment contents for %s.", name);
  }

  // Load sub-rooms.
  {
    log_vfprintf(" ---- Loading sub-rooms for %s.", name);
    bf::directory_iterator end_itr; // default construction yields past-the-end
    for (bf::directory_iterator itr(base_directory); itr != end_itr; ++itr) {
      if (is_directory(itr->status())) {
        bf::path room_path = itr->path();
        log_vfprintf(" ----- Initializing sub-room from file %s.", room_path.string().c_str());
        ApartmentRoom apartment_room = ApartmentRoom(this, room_path);
        rooms.push_back(apartment_room);
        log_vfprintf(" ----- Fully loaded sub-room at %ld.", apartment_room.get_vnum());
      }
    }
  }

  // Calculate any derived data.
  char tmp_buf[100];
  snprintf(tmp_buf, sizeof(tmp_buf), "%s's Unit %s", complex->display_name, name);
  full_name = str_dup(tmp_buf);

  log_vfprintf(" ---- Applying changes from %s to world...", full_name);

  // TODO: Set house flag, ensure door, etc.
}

/* TODO: Write lease data to <apartment name>/lease. This includes:
   - Owner ID (negative for pgroup ID)
   - Paid until
   - Guest info
*/
void Apartment::save_lease() {}

/* Delete <apartment name>/lease and restore default descs. */
void Apartment::break_lease() {
  mudlog_vfprintf(NULL, LOG_GRIDLOG, "Cleaning up lease for %s's %s.", complex->display_name, name);

  // Clear lease data.
  owned_by_player = 0;
  owned_by_pgroup = NULL;

  // TODO: Delete the lease file.

  // Iterate over rooms and restore them.
  for (auto room: rooms) {
    room.purge_contents();
    room.restore_default_desc();
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
  if (owned_by_player > 0) {
    if (GET_IDNUM(ch) == owned_by_player)
      return TRUE;
  } else if (owned_by_pgroup) {
    // TODO: Pgroup entry.
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
  // No idnum? No entry.
  if (idnum <= 0)
    return FALSE;

  // Check for owner status or pgroup perms.
  if (owned_by_player > 0) {
    if (idnum == owned_by_player)
      return TRUE;
  } else if (owned_by_pgroup) {
    // TODO: Pgroup entry.
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
  if (owned_by_player) {
    if (GET_IDNUM(ch) == owned_by_player)
      return TRUE;
  }

  if (owned_by_pgroup) {
    // TODO: Handle this
  }

  return FALSE;
}


/********** ApartmentRoom ************/

ApartmentRoom::ApartmentRoom(Apartment *apartment, bf::path filename) :
  apartment(apartment)
{
  {
    json base_info;
    _json_parse_from_file(filename / "info.json", base_info);

    vnum = base_info["vnum"].get<vnum_t>();

    rnum_t rnum = real_room(vnum);
    if (vnum < 0 || rnum < 0) {
      log_vfprintf("SYSERR: Invalid vnum %ld specified in %s.", filename.string().c_str());
      exit(1);
    }

    default_room_desc = str_dup(base_info["default_desc"].get<std::string>().c_str());

    log_vfprintf(" ----- Applying changes from %s to world...", filename.filename().string().c_str());

    // Set house flag, apartment pointer, etc.
    struct room_data *room = &world[rnum];
    room->apartment = this;

    // Load storage contents.
    storage_path = filename / "storage";
  }
}

/* Purge the contents of this apartment, logging as we go. */
void ApartmentRoom::purge_contents() {
  rnum_t rnum = real_room(vnum);

  if (rnum < 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Refusing to purge contents for %ld: Invalid vnum", vnum);
    return;
  }

  struct room_data *room = &world[rnum];



  // TODO: Write a storage file, then back it up to <name>/expired/storage_<ownerid>_<epoch>

  // Purge all contents, logging as we go.
  int total_value_destroyed = 0;
  for (struct obj_data *obj = room->contents, *next_o; obj; obj = next_o) {
    next_o = obj->next_content;

    // TODO: If this is belongings, don't purge it. Move it somewhere safe instead?

    const char *representation = generate_new_loggable_representation(obj);
    mudlog_vfprintf(NULL, LOG_PURGELOG, "Apartment cleanup for %s (%ld) has purged %s (cost: %d).", apartment->full_name, GET_ROOM_VNUM(room), representation, GET_OBJ_COST(obj));
    delete [] representation;

    total_value_destroyed += GET_OBJ_COST(obj);

    extract_obj(obj);
  }
  mudlog_vfprintf(NULL, LOG_PURGELOG, "Total value destroyed: %d nuyen.", total_value_destroyed);

  // Transfer all vehicles to the nearest garage.
  remove_vehicles_from_apartment(room);

  // Notify the room.
  send_to_room("A pair of movers comes through and cleans up with brisk efficiency.\r\n", room);
}

/* Overwrite the room's desc with the default. */
void ApartmentRoom::restore_default_desc() {
  rnum_t rnum = real_room(vnum);

  if (rnum < 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Refusing to clear room desc for %ld: Invalid vnum", vnum);
    return;
  }

  struct room_data *room = &world[rnum];

  delete [] room->description;
  room->description = str_dup(default_room_desc);

  mudlog_vfprintf(NULL, LOG_SYSLOG, "Restored default room desc to room %ld.", GET_ROOM_VNUM(room));

  send_to_room("You blink, then shrug-- this place must have always looked this way.\r\n", room);
}

/* TODO: Write storage data to <apartment name>/storage. This is basically the existing houses/<vnum> file. */
void ApartmentRoom::save_storage() {
  Storage_save(storage_path.string().c_str(), &world[real_room(vnum)]);
}

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


//////////////////////////// The Landlord spec. ///////////////////////////////
class ApartmentComplex *get_complex_headed_by_landlord(vnum_t vnum) {
  for (auto &candidate_complex : global_apartment_complexes) {
    if (vnum == candidate_complex.get_landlord_vnum()) {
      return &candidate_complex;
    }
  }
  return NULL;
}

SPECIAL(landlord_spec)
{
  class ApartmentComplex *complex;

  struct char_data *recep = (struct char_data *) me;

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

    for (auto apartment : complex->get_apartments()) {
      if (is_abbrev(arg, apartment.get_name())) {
        if (!apartment.has_owner_privs(ch)) {
          mob_say(recep, "I would get fired if I did that.");
        } else {
          mob_say(recep, "Here you go...");
          apartment.issue_key(ch);
        }
        return TRUE;
      }
    }

    mob_say(recep, "Sorry, I don't recognize that unit number.");
    return TRUE;
  }

  else if (CMD_IS("lease")) {
    if (!*arg) {
      mob_say(recep, "Which room would you like to lease?");
      complex->display_room_list_to_character(ch);
      return TRUE;
    }

    for (auto apartment : complex->get_apartments()) {
      if (is_abbrev(arg, apartment.get_name())) {
        if (apartment.has_owner()) {
          mob_say(recep, "Sorry, I'm afraid that room is already taken.");
        } else if (complex->ch_already_rents_here(ch)) {
          mob_say(recep, "Sorry, we only allow people to lease one room at a time here.");
        } else {
          if (apartment.create_or_extend_lease(ch)) {
            mob_say(recep, "Thank you, here is your key.");
            apartment.issue_key(ch);
          }
        }
        return TRUE;
      }
    }

    mob_say(recep, "Sorry, I don't recognize that unit number.");
    return TRUE;
  }

  else if (CMD_IS("leave") || CMD_IS("break")) {
    if (!*arg) {
      send_to_char("Syntax: ^WBREAK <unit name>^n\r\n", ch);
      return TRUE;
    }

    for (auto apartment : complex->get_apartments()) {
      if (is_abbrev(arg, apartment.get_name())) {
        if (!apartment.has_owner_privs(ch)) {
          mob_say(recep, "I would get fired if I did that.");
        } else {
          apartment.break_lease();
          mob_say(recep, "I hope you enjoyed your time here.");
        }
        return TRUE;
      }
    }

    mob_say(recep, "Sorry, I don't recognize that unit number.");
    return TRUE;
  }

  else if (CMD_IS("pay")) {
    if (!*arg) {
      send_to_char("Syntax: ^WPAY <unit name>^n\r\n", ch);
      return TRUE;
    }

    for (auto apartment : complex->get_apartments()) {
      if (is_abbrev(arg, apartment.get_name())) {
        if (!apartment.has_owner_privs(ch)) {
          mob_say(recep, "You aren't an owner of that apartment.");
        } else {
          apartment.create_or_extend_lease(ch);
          mob_say(recep, "You are paid up for the next period.");
        }
        return TRUE;
      }
    }

    mob_say(recep, "Sorry, I don't recognize that unit number.");
    return TRUE;
  }

  else if (CMD_IS("status")) {
    if (!*arg) {
      send_to_char("Syntax: ^WSTATUS <unit name>^n\r\n", ch);
      return TRUE;
    }

    for (auto apartment : complex->get_apartments()) {
      if (is_abbrev(arg, apartment.get_name())) {
        if (!apartment.has_owner()) {
          mob_say(recep, "That room is currently available for lease.");
        } else if (!apartment.has_owner_privs(ch)) {
          mob_say(recep, "I'm afraid I can't release that information.");
        } else {
          if (apartment.get_paid_until() - time(0) < 0) {
            mob_say(recep, "Your rent has expired on that apartment.");
          } else {
            snprintf(buf2, sizeof(buf2), "You are paid for another %d days.", (int)((apartment.get_paid_until() - time(0)) / 86400));
            mob_say(recep, buf2);
            strlcpy(buf2, "Note: Those are real-world days.", sizeof(buf2));
            do_say(recep, buf2, 0, SCMD_OSAY);
          }
        }
        return TRUE;
      }
    }

    mob_say(recep, "Sorry, I don't recognize that unit number.");
    return TRUE;
  }
  return FALSE;
}
