#ifndef _newhouse_h_
#define _newhouse_h_

#include <string>
#include <vector>

#include "types.hpp"
#include "awake.hpp"
#include "structs.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "playergroup_classes.hpp"

class ApartmentRoom;
class Apartment;
class ApartmentComplex;

extern std::vector<ApartmentComplex> global_apartment_complexes;

/* An ApartmentComplex is composed of N Apartments, and has tracking data for landlord info. */
class ApartmentComplex {
  private:
    friend class Apartment;

    // The name of the complex (Evergreen Multipliex)
    const char *name;

    // The vnum of the landlord
    vnum_t landlord_vnum;

    std::vector<Apartment> rooms = {};

  public:
    // Given a filename to read from, instantiate an apartment complex.
    ApartmentComplex(std::string filename);

    // Save function.
    void save();
};

/* An Apartment is composed of N ApartmentRooms, and has tracking data for the lease etc. */
class Apartment {
  private:
    friend class ApartmentRoom;

    // Info about the apartment.
    const char *name; // 309
    const char *full_name; // Evergreen Multiplex's Unit 309 (derived)
    int lifestyle;
    long nuyen_per_month;

    // Location and world data for the primary / entrance room.
    vnum_t atrium;
    vnum_t key;
    dir_t exit_dir;

    // Info about rooms this apartment has. First one is the entrance and attaches to atrium.
    std::vector<ApartmentRoom> rooms = {};

    // Info about the owner and lease.
    idnum_t owned_by_player;
    Playergroup *owned_by_pgroup;
    time_t paid_until;
    std::vector<long> guests = {};

    // Backlink information.
    ApartmentComplex *complex;

  public:
    // Given a filename to read from, instantiate an individual apartment.
    Apartment(ApartmentComplex *complex, std::string filename);
    ~Apartment() { delete [] name; delete [] full_name; }

    // Save function.
    void save_lease();
    void save_storage();

    void break_lease();

    const char *get_apartment_descriptor();
};

/* An ApartmentRoom describes a discrete room in the world. */
class ApartmentRoom {
  private:
    // What is the vnum of this room? (We don't store rnum since that changes)
    vnum_t vnum;

    // What desc will be restored when this apartment's lease is broken?
    const char *default_room_desc;

    Apartment *apartment;

  public:
    ApartmentRoom(vnum_t vnum, const char *default_room_desc, Apartment *apartment) :
      vnum(vnum), default_room_desc(str_dup(default_room_desc)), apartment(apartment)
    {
      if (vnum <= 0) {
        mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Invalid vnum %ld provided to apartment room!", vnum);
      }
    }
    ~ApartmentRoom() { delete [] default_room_desc; }

    // Getters.
    vnum_t get_vnum() { return vnum; }
    const char *get_default_room_desc() { return default_room_desc; }

    // Will add setters and save function if OLC is added for these.

    // Restore the apartment's default description.
    void purge_contents();
    void restore_default_desc();
};

#endif // _newhouse_h_