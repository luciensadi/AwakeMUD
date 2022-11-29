#ifndef _newhouse_h_
#define _newhouse_h_

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
namespace bf = boost::filesystem;

#include "types.hpp"
#include "awake.hpp"
#include "structs.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "playergroup_classes.hpp"

class ApartmentRoom;
class Apartment;
class ApartmentComplex;

extern void warn_about_apartment_deletion();
extern void save_all_apartments_and_storage_rooms();
extern ApartmentComplex *find_apartment_complex(const char *name, struct char_data *ch=NULL);

extern std::vector<ApartmentComplex*> global_apartment_complexes;

/* An ApartmentComplex is composed of N Apartments, and has tracking data for landlord info. */
class ApartmentComplex {
  private:
    friend class Apartment;

    // The name of the complex (Evergreen Multipliex)
    const char *display_name = NULL;

    // The name of the directory where we store this complex's data.
    bf::path base_directory;

    // The vnum of the landlord
    vnum_t landlord_vnum = -1;

    std::vector<Apartment*> apartments = {};
    std::vector<idnum_t> editors = {};

  public:
    // Given a filename to read from, instantiate an apartment complex.
    ApartmentComplex(bf::path filename);
    // Create an empty complex.
    ApartmentComplex() : display_name(str_dup("Unnamed Complex")) {}

    // Accessors.
    const char *get_name() { return display_name; }
    vnum_t get_landlord_vnum() { return landlord_vnum; }
    std::vector<Apartment*> get_apartments() { return apartments; }
    std::vector<idnum_t> get_editors() { return editors; }

    // Mutators.
    bool set_landlord_vnum(vnum_t vnum);
    bool set_name(const char *name);

    // Clone our data from the provided complex.
    void clone_from(ApartmentComplex *);

    // Save function.
    void save();

    // Utils / misc
    void display_room_list_to_character(struct char_data *ch);
    bool ch_already_rents_here(struct char_data *ch);
    bool can_houseedit_complex(struct char_data *ch);
    const char *list_editors();
};

/* An Apartment is composed of N ApartmentRooms, and has tracking data for the lease etc. */
class Apartment {
  private:
    friend class ApartmentRoom;
    friend class ApartmentComplex;

    // Info about the apartment.
    const char *shortname = NULL; // 309
    const char *name = NULL; // Unit 309
    const char *full_name = NULL; // Evergreen Multiplex's Unit 309 (derived)
    int lifestyle = 0;
    long nuyen_per_month = 0;
    bf::path base_directory;

    // Location and world data for the primary / entrance room.
    vnum_t atrium = NOWHERE;
    vnum_t key_vnum = NOTHING;
    dir_t exit_dir = NORTH;

    // Info about rooms this apartment has. First one is the entrance and attaches to atrium.
    std::vector<ApartmentRoom*> rooms = {};
    int garages = 0;

    // Info about the owner and lease.
    idnum_t owned_by_player = 0;
    Playergroup *owned_by_pgroup = NULL;
    time_t paid_until = 0;
    std::vector<long> guests = {};

    // Backlink information.
    ApartmentComplex *complex = NULL;

  public:
    // Given a filename to read from, instantiate an individual apartment.
    Apartment(ApartmentComplex *complex, bf::path filename);

    // Accessors
    const char *get_name() { return name; }
    const char *get_full_name() { return full_name; }
    vnum_t get_key_vnum() { return key_vnum; }
    vnum_t get_atrium_vnum() { return atrium; }
    long get_rent_cost() { return nuyen_per_month; }
    time_t get_paid_until() { return paid_until; }
    std::vector<ApartmentRoom*> get_rooms() { return rooms; }
    std::vector<long> get_guests() { return guests; }

    // Mutators
    void set_owner(idnum_t);
    void set_paid_until(time_t);

    bool create_or_extend_lease(struct char_data *ch);
    void save_lease();
    void break_lease();

    bool issue_key(struct char_data *ch);

    // Utility / misc
    bool can_enter(struct char_data *ch);
    bool can_enter_by_idnum(idnum_t idnum);
    bool has_owner_privs(struct char_data *ch);
    bool has_owner_privs_by_idnum(idnum_t idnum);
    bool has_owner() { return owned_by_pgroup || owned_by_player; }
    idnum_t get_owner_id();
    Playergroup *get_owner_pgroup() { return owned_by_pgroup; }
    bool owner_is_valid();
    void list_guests_to_char(struct char_data *ch);

    // Returns new-- must delete output!
    const char *get_owner_name__returns_new();
};

/* An ApartmentRoom describes a discrete room in the world. */
class ApartmentRoom {
  private:
    // What is the vnum of this room? (We don't store rnum since that changes)
    vnum_t vnum = -1;

    // What desc will be restored when this apartment's lease is broken?
    const char *decoration = NULL;

    // What is this subroom's name? (Only used for debugging)
    const char *name = NULL;

    bf::path base_path;
    bf::path storage_path;

    // Backlink to our apartment.
    Apartment *apartment = NULL;

  public:
    ApartmentRoom(Apartment *apartment, bf::path filename);

    // Accessors.
    vnum_t get_vnum() { return vnum; }
    const char *get_decoration() { return decoration; }
    vnum_t get_atrium_vnum() { return apartment->get_atrium_vnum(); }
    const char *get_full_name() { return apartment->get_full_name(); }
    const char *get_name() { return name; }
    bool has_owner() { return apartment->has_owner(); }
    bool has_owner_privs(struct char_data *ch) { return apartment->has_owner_privs(ch); }
    bool can_enter(struct char_data *ch) { return apartment->can_enter(ch); }
    bool can_enter_by_idnum(idnum_t idnum) { return apartment->can_enter_by_idnum(idnum); }
    bool is_guest(idnum_t idnum);
    ApartmentComplex *get_complex() { return apartment->complex; }

    // Mutators.
    void set_decoration(const char *new_desc);

    // Save functions.
    void save_storage();
    void save_info();
    void save_decoration();

    // Utility.
    void list_guests_to_char(struct char_data *ch) { apartment->list_guests_to_char(ch); }
    void load_storage();

    bool delete_guest(idnum_t idnum);
    void add_guest(idnum_t idnum);

    // Restore the apartment's default description.
    void purge_contents();
    void delete_decoration();
};

#endif // _newhouse_h_
