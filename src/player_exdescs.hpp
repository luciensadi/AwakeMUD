#include "bitfield.hpp"
#include "types.hpp"
#include "utils.hpp"

#define GET_CHAR_EXDESCS(ch) ((ch)->player_specials->saved.exdescs)
#define GET_CHAR_MAX_EXDESCS(ch) ((ch)->player_specials->max_exdescs)
#define CHAR_HAS_EXDESCS(ch) (!(ch)->player_specials->saved.exdescs.empty())

class PCExDesc {
  idnum_t pc_idnum = 0;
  const char *keyword = NULL;
  const char *name = NULL;
  const char *desc = NULL;
  Bitfield wear_slots;
  PCExDesc *editing_clone_of = NULL;
public:
  // This is only invoked when creating a new desc during editing. Leave most fields blank.
  PCExDesc(idnum_t pc_idnum) :
    pc_idnum(pc_idnum)
  {}

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

  Bitfield get_wear_slots() { return wear_slots; }
  void set_wear_slots(const char *new_string) { wear_slots.FromString(new_string); }
  

  // Saving happens here. Requires pc_idnum and keyword to be set. Invoke during editing.
  void save_to_db();
  // Call this to delete this entry. Requires pc_idnum and keyword to be set. Invoke during editing when char chooses to delete.
  void delete_from_db();

  // Is this in the specified wearslot?
  bool is_in_wearslot(int wearslot) { return wear_slots.IsSet(wearslot); }
};

bool look_at_exdescs(struct char_data *viewer, struct char_data *vict, char *arg);

void syspoints_purchase_exdescs(struct char_data *ch, char *buf, bool is_confirmed);

void set_exdesc_max(struct char_data *ch, int amount, bool save_to_db);

void load_exdescs_from_db(struct char_data *ch);