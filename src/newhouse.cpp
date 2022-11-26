#include <fstream>
#include <iostream>

#include <boost/filesystem.hpp>
namespace bf = boost::filesystem;

#include "types.hpp"
#include "awake.hpp"
#include "structs.hpp"
#include "db.hpp"
#include "utils.hpp"
#include "playergroup_classes.hpp"
#include "handler.hpp"
#include "newhouse.hpp"

#include "nlohmann/json.hpp"
using nlohmann::json;

std::vector<ApartmentComplex> global_apartment_complexes = {};

void load_apartment_complexes() {
  // Iterate over the contents of the lib/housing directory.
  bf::path housing_dir("lib/housing/");

  if (!exists(housing_dir)) {
    log("FATAL ERROR: Unable to find lib/housing. Terminating.");
    exit(1);
  }

  bf::directory_iterator end_itr; // default construction yields past-the-end
  for (bf::directory_iterator itr(housing_dir); itr != end_itr; ++itr) {
    if (is_directory(itr->status())) {
      std::string filename = itr->path().filename().string();
      log_vfprintf("Found apartment complex filename %s.", filename.c_str());
      global_apartment_complexes.push_back(ApartmentComplex(filename));
    }
  }
}

/*********** ApartmentComplex ************/
ApartmentComplex::ApartmentComplex(std::string filename) {
  // Load info from <filename>/info
}
/*********** Apartment ************/

/* Load this apartment entry from files. */
Apartment::Apartment(ApartmentComplex *complex, std::string filename) :
  complex(complex)
{
  // TODO: Load base info from <name>/info.
  {
    std::ifstream f(filename);
    json base_info = json::parse(f);
    f.close();
  }

  // TODO: Load lease info from <name>/lease.

  // TODO: Perform sanity checking here to see if we have a valid paid_until.

  // TODO: Load storage info from <name>/storage.

  // Calculate any derived data.
  char tmp_buf[100];
  snprintf(tmp_buf, sizeof(tmp_buf), "%s's Unit %s", complex->name, name);
  full_name = str_dup(tmp_buf);
}

/* TODO: Write lease data to <apartment name>/lease. This includes:
   - Owner ID (negative for pgroup ID)
   - Paid until
   - Guest info
*/
void Apartment::save_lease() {}

/* TODO: Write storage data to <apartment name>/storage. This is basically the existing houses/<vnum> file. */
void Apartment::save_storage() {}

/* Delete <apartment name>/lease and restore default descs. */
void Apartment::break_lease() {
  mudlog_vfprintf(NULL, LOG_GRIDLOG, "Cleaning up lease for %s's %s.", complex->name, name);

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

/********** ApartmentRoom ************/

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

    const char *representation = generate_new_loggable_representation(obj);
    mudlog_vfprintf(NULL, LOG_PURGELOG, "Apartment cleanup for %s (%ld) has purged %s (cost: %d).", apartment->full_name, GET_ROOM_VNUM(room), representation, GET_OBJ_COST(obj));
    delete [] representation;

    total_value_destroyed += GET_OBJ_COST(obj);

    extract_obj(obj);
  }
  mudlog_vfprintf(NULL, LOG_PURGELOG, "Total value destroyed: %d nuyen.", total_value_destroyed);

  // TODO: Transfer all vehicles to the nearest garage.

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
