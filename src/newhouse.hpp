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
#include "lifestyles.hpp"

class ApartmentRoom;
class Apartment;
class ApartmentComplex;

extern const bf::path global_housing_dir;

extern void warn_about_apartment_deletion();
extern void save_all_apartments_and_storage_rooms();
extern ApartmentComplex *find_apartment_complex(const char *name, struct char_data *ch=NULL);
extern Apartment *find_apartment(const char *full_name, struct char_data *ch);

extern bool apartment_sort_func(Apartment *, Apartment *);
extern bool apartment_complex_sort_func(ApartmentComplex *, ApartmentComplex *);

extern std::vector<ApartmentComplex*> global_apartment_complexes;

extern SPECIAL(landlord_spec);

#define GET_APARTMENT(room)                     ((room)->apartment)
#define GET_APARTMENT_SUBROOM(room)             ((room)->apartment_room)
#define GET_APARTMENT_DECORATION(room)          ((room)->apartment_room ? (room)->apartment_room->get_decoration() : NULL)
#define CH_CAN_ENTER_APARTMENT(room, ch)        (((room) && (room)->apartment) ? (room)->apartment->can_enter(ch) : TRUE)
#define IDNUM_CAN_ENTER_APARTMENT(room, idnum)  (((room) && (room)->apartment) ? (room)->apartment->can_enter_by_idnum(idnum) : TRUE)

#define MIN_LEVEL_TO_IGNORE_HOUSEEDIT_EDITOR_STATUS  LVL_ADMIN

/* An ApartmentComplex is composed of N Apartments, and has tracking data for landlord info. */
class ApartmentComplex {
  private:
    // !!! ADDING ANY VARIABLES? UPDATE CLONE_FROM() !!!

    friend class Apartment;

    // The name of the complex (Evergreen Multipliex)
    const char *display_name = NULL;

    // The name of the directory where we store this complex's data.
    bf::path base_directory;

    // The vnum of the landlord
    vnum_t landlord_vnum = -1;

    std::vector<Apartment*> apartments = {};
    std::vector<idnum_t> editors = {};

    // The lifestyle this complex conveys. Can be overridden at the apartment level.
    int lifestyle = LIFESTYLE_LOW;
    // Lifestyle string options provided by this complex.
    std::vector<const char *> default_strings_neutral = {};
    std::vector<const char *> default_strings_gendered = {};
    std::vector<const char *> garage_strings_neutral = {};
    std::vector<const char *> garage_strings_gendered = {};

  public:
    // Given a filename to read from, instantiate an apartment complex.
    ApartmentComplex(bf::path filename);
    ApartmentComplex(vnum_t);
    ApartmentComplex();
    ~ApartmentComplex();

    // Accessors.
    const char *get_name() { return display_name; }
    vnum_t get_landlord_vnum() { return landlord_vnum; }
    std::vector<Apartment*> get_apartments() { return apartments; }
    std::vector<idnum_t> get_editors() { return editors; }
    bf::path get_base_directory() { return base_directory; }
    int get_lifestyle() { return lifestyle; }

    // Mutators.
    bool set_landlord_vnum(vnum_t vnum, bool perform_landlord_overlap_test);
    bool set_name(const char *name);
    void toggle_editor(idnum_t idnum);
    void add_editor(idnum_t idnum);
    void remove_editor(idnum_t idnum);
    void set_base_directory(bf::path path) { base_directory = path; }
    void add_apartment(Apartment *apartment);
    void delete_apartment(Apartment *apartment);

    // Clone our data from the provided complex.
    void clone_from(ApartmentComplex *);

    // Save function.
    void save();

    // Utils / misc
    const char *list_apartments__returns_new();
    void display_room_list_to_character(struct char_data *ch);
    bool ch_already_rents_here(struct char_data *ch);
    bool can_houseedit_complex(struct char_data *ch);
    const char *list_editors();
    void mark_as_deleted();
    int get_crap_count();
};

/* An Apartment is composed of N ApartmentRooms, and has tracking data for the lease etc. */
class Apartment {
  private:
    // !!! ADDING ANY VARIABLES? UPDATE CLONE_FROM() !!!

    friend class ApartmentRoom;
    friend class ApartmentComplex;

    // Info about the apartment.
    const char *shortname = NULL; // 309
    const char *name = NULL; // Unit 309
    const char *full_name = NULL; // Evergreen Multiplex's Unit 309 (derived)
    long nuyen_per_month = 0;
    bf::path base_directory;
    bool garage_override = FALSE;

    // Location and world data for the primary / entrance room.
    vnum_t atrium = NOWHERE;
    vnum_t key_vnum = NOTHING;

    // Info about rooms this apartment has. First one is the entrance and attaches to atrium.
    std::vector<ApartmentRoom*> rooms = {};
    unsigned long garages = 0;

    // Info about the owner and lease.
    idnum_t owned_by_player = 0;
    Playergroup *owned_by_pgroup = NULL;
    time_t paid_until = 0;
    std::vector<long> guests = {};

    // Backlink information.
    ApartmentComplex *complex = NULL;

    // The lifestyle override of this apartment. -1 if it's not set.
    int lifestyle = -1;
    // Lifestyle string options provided by this apartment.
    std::vector<const char *> default_strings_neutral = {};
    std::vector<const char *> default_strings_gendered = {};
    std::vector<const char *> garage_strings_neutral = {};
    std::vector<const char *> garage_strings_gendered = {};

  public:
    // Given a filename to read from, instantiate an individual apartment.
    Apartment(ApartmentComplex *complex, bf::path filename);
    Apartment(ApartmentComplex *complex, const char *, vnum_t, vnum_t, int, idnum_t, time_t);
    Apartment();
    ~Apartment();

    // Accessors
    const char *get_short_name() { return shortname; }
    const char *get_name() { return name; }
    const char *get_full_name() { return full_name; }
    vnum_t get_key_vnum() { return key_vnum; }
    vnum_t get_atrium_vnum() { return atrium; }
    long get_rent_cost() { return nuyen_per_month; }
    time_t get_paid_until() { return paid_until; }
    std::vector<long> get_guests() { return guests; }
    ApartmentComplex *get_complex() { return complex; }
    bf::path get_base_directory() { return base_directory; }
    unsigned long get_garage_count() { return garages; }
    int get_lifestyle() { return lifestyle >= LIFESTYLE_STREETS ? lifestyle : complex->get_lifestyle(); }
    bool get_garage_override() { return garage_override; }
    std::vector<const char *> *get_custom_lifestyle_strings(struct char_data *ch);

    // Mutators
    void set_owner(idnum_t);
    void set_paid_until(time_t);
    void set_complex(ApartmentComplex *new_complex);
    void regenerate_full_name();
    void set_name(const char *newname);
    void set_short_name(const char *newname);

    void set_atrium(vnum_t vnum) {atrium = vnum;}
    void set_key_vnum(vnum_t vnum) {key_vnum = vnum;}
    bool set_rent(long amount, struct char_data *ch=NULL);
    bool set_lifestyle(int new_lifestyle, struct char_data *ch=NULL);
    void set_base_directory(bf::path path) { base_directory = path; }
    void set_garage_override(bool value) { garage_override = value; }


    std::vector<ApartmentRoom*> get_rooms() { return rooms; }
    bool add_room(ApartmentRoom *);
    void delete_room(ApartmentRoom *);

    bool create_or_extend_lease(struct char_data *ch);
    void save_lease();
    void save_rooms();
    void break_lease();
    void save_base_info();

    bool issue_key(struct char_data *ch);

    // Clone our data from the provided apartment.
    void clone_from(Apartment *);

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
    const char *list_rooms__returns_new(bool indent);
    bool can_houseedit_apartment(struct char_data *ch);
    void mark_as_deleted();
    void recalculate_garages();
    bool is_garage_lifestyle();
    void apply_rooms();
    void clamp_rent(struct char_data *ch);

    bool delete_guest(idnum_t idnum);
    void add_guest(idnum_t idnum);
    bool is_guest(idnum_t idnum);

    int get_crap_count();

    // Returns new-- must delete output!
    const char *get_owner_name__returns_new();
};

/* An ApartmentRoom describes a discrete room in the world. */
class ApartmentRoom {
  private:
    friend class Apartment;

    // What is the vnum of this room? (We don't store rnum since that changes)
    vnum_t vnum = -1;

    // What desc will be restored when this apartment's lease is broken?
    const char *decoration = NULL;

    bf::path base_path;
    bf::path storage_path;

    // Backlink to our apartment.
    Apartment *apartment = NULL;

  public:
    ApartmentRoom(Apartment *apartment, bf::path filename);
    ApartmentRoom(Apartment *apartment, struct room_data *room);
    ApartmentRoom(ApartmentRoom *);
    ~ApartmentRoom();

    // Accessors.
    vnum_t get_vnum() { return vnum; }
    const char *get_decoration() { return decoration; }
    Apartment *get_apartment() { return apartment; }
    ApartmentComplex *get_complex() { return apartment->complex; }
    bf::path get_base_directory() { return base_path; }

    // Mutators.
    void set_decoration(const char *new_desc);

    // Save functions.
    void save_storage();
    void save_info();
    void save_decoration();

    // Utility.
    void load_storage();
    void load_storage_from_specified_path(bf::path path);
    const char *get_full_name();

    struct room_data *get_world_room();

    // Restore the apartment's default description.
    void delete_decoration();
    void purge_contents();
    void delete_info();
};

#endif // _newhouse_h_
