#include "bitfield.hpp"
#include "types.hpp"
#include "utils.hpp"

#define GET_CHAR_EXDESCS(ch) ((ch)->player_specials->saved.exdescs)
#define GET_CHAR_MAX_EXDESCS(ch) ((ch)->player_specials->max_exdescs)
#define CHAR_HAS_EXDESCS(ch) (!(ch)->player_specials->saved.exdescs.empty())

#define GET_CHAR_COVERED_WEARLOCS(ch) ((ch)->player_specials->covered_wearlocs)

#define MAX_EXDESC_KEYWORD_LENGTH  99
#define MAX_EXDESC_NAME_LENGTH     199
#define MAX_EXDESC_DESC_LENGTH     65535

#define PC_EXDESC_EDIT_MAIN_MENU       0
#define PC_EXDESC_EDIT_EDIT_MENU       1
#define PC_EXDESC_EDIT_DELETE_MENU     2
#define PC_EXDESC_EDIT_OLC_MENU        3
#define PC_EXDESC_EDIT_OLC_WEAR_MENU   4
#define PC_EXDESC_EDIT_OLC_SET_KEYWORD 5
#define PC_EXDESC_EDIT_OLC_SET_NAME    6
#define PC_EXDESC_EDIT_OLC_SET_DESC    7

#define PC_EXDESC_EDIT_OLC_FROM_CREATE 1
#define PC_EXDESC_EDIT_OLC_FROM_EDIT   2

class PCExDesc {
  idnum_t pc_idnum = 0;
  const char *keyword = NULL;
  const char *name = NULL;
  const char *desc = NULL;
  Bitfield wear_slots;
  PCExDesc *editing_clone_of = NULL;
public:
  // This is only invoked when creating a new desc during editing. Leaves most fields unchanged.
  PCExDesc(idnum_t pc_idnum) :
    pc_idnum(pc_idnum)
  {
    keyword = str_dup("keyword");
    name = str_dup("An unfinished extra description hovers about them.");
    desc = str_dup("It hasn't been written yet.");
  }

  // Clones an exdesc. Used for editing an existing desc where you want to be able to abort.
  PCExDesc(PCExDesc *original) {
    pc_idnum = original->pc_idnum;
    keyword = str_dup(original->keyword);
    name = str_dup(original->name);
    desc = str_dup(original->desc);
    wear_slots.FromString(original->wear_slots.ToString());
    editing_clone_of = original;
  }

  // This is invoked when loading from DB. Fill out all fields.
  PCExDesc(idnum_t pc_idnum, const char *keyword, const char *name, const char *desc, const char *wear_slots_string) :
    pc_idnum(pc_idnum),
    keyword(str_dup(keyword)),
    name(str_dup(name)),
    desc(str_dup(desc))
  {
    wear_slots.FromString(wear_slots_string);
  }

  ~PCExDesc() {
    delete [] keyword;
    delete [] name;
    delete [] desc;
    editing_clone_of = NULL;
  }

  // Getters / setters
  idnum_t get_pc_idnum() { return pc_idnum; }
  void set_pc_idnum(idnum_t new_id) { pc_idnum = new_id; }

  const char *get_keyword() { return keyword; }
  void set_keyword(const char *new_keyword) { delete [] keyword; keyword = str_dup(new_keyword); }

  const char *get_name() { return name; }
  void set_name(const char *new_name) { delete [] name; name = str_dup(new_name); }

  const char *get_desc() { return desc; }
  void set_desc(const char *new_desc) { delete [] desc; desc = str_dup(new_desc); }

  Bitfield *get_wear_slots() { return &wear_slots; }
  void set_wear_slots(const char *new_string) { wear_slots.FromString(new_string); }

  void overwrite_editing_clone();
  
  // Saving happens here. Requires pc_idnum and keyword to be set. Invoke when saving after editing.
  void save_to_db();

  // Delete this specific exdesc.
  void delete_from_db();

  // Is this in the specified wearslot?
  bool is_in_wearslot(int wearslot) { return wear_slots.IsSet(wearslot); }
};

bool look_at_exdescs(struct char_data *viewer, struct char_data *vict, char *arg);

void syspoints_purchase_exdescs(struct char_data *ch, char *buf, bool is_confirmed);

void set_exdesc_max(struct char_data *ch, int amount, bool save_to_db);

void load_exdescs_from_db(struct char_data *ch);
void delete_all_exdescs_from_db(struct char_data *ch);
void write_all_exdescs_to_db(struct char_data *ch);

void pc_exdesc_edit_disp_main_menu(struct descriptor_data *d);
void _pc_exdesc_edit_olc_menu(struct descriptor_data *d);
void clone_exdesc_vector_to_edit_mob_for_editing(struct descriptor_data *d);
void overwrite_pc_exdescs_with_edit_mob_exdescs_and_then_save_to_db(struct descriptor_data *d);
void pc_exdesc_edit_parse(struct descriptor_data *d, const char *arg);

bool viewer_can_see_at_least_one_exdesc_on_vict(struct char_data *viewer, struct char_data *victim);