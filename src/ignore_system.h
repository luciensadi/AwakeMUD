#ifndef _ignore_system_h_
#define _ignore_system_h_

#include <unordered_map>

#include "bitfield.h"  // for Bitfield
#include "structs.h"   // for struct char_data

/* Design Principles

  This system needs to support:
  - block OOC messages (no visibility on channel)
  - block radio messages (don't hear their messages)
  - block mindlink (disables their ability to send you mindlinks, breaks the spell if it's on you)
  - block tells (disables their ability to send you tells)
  - block interaction (can't see speech, emotes, and socials; also turns on following, mindlink, radio)
  - block following (they cannot follow you, and are unfollowed if they are)
  - block wholist / where (they can't see you on the wholist or in the wherelist)

  to do this, we need a data structure that has a blocker number, and blockee number, and a bitfield of blocked things

  this data structure needs to save (write to SQL?)

  this data structure should be fast to look up (unordered map?)

  could just stick an unordered map on the PC and compare against that... but that spreads out the logic
  may as well centralize here for easier maintenance

*/

#define GET_IGNORE_DATA(ch)  (ch->ignore_data)

#define IS_IGNORING(ch, mode, vict) (GET_IGNORE_DATA((ch)) ? GET_IGNORE_DATA((ch))->mode((vict)) : FALSE)

#define MODE_NOT_SILENT            0
#define MODE_SILENT                1

#define IGNORE_BIT_WHERE           0
#define IGNORE_BIT_OOC_CHANNELS    1
#define IGNORE_BIT_RADIO           2
#define IGNORE_BIT_MINDLINK        3
#define IGNORE_BIT_TELLS           4
#define IGNORE_BIT_OSAYS           5
#define IGNORE_BIT_IC_INTERACTION  6 // includes shouts
#define IGNORE_BIT_FOLLOWING       7
#define IGNORE_BIT_CALLS           8
#define NUM_IGNORE_BITS            9

extern const char *ignored_bits_in_english[];

void globally_remove_vict_id_from_logged_in_ignore_lists(long vict_idnum);

// The main IgnoreData class definition.
class IgnoreData {
  // The character who owns this block of data. Used primarily for saving.
  struct char_data *ch;

  // Private helper functions.
  bool _ignore_bit_is_set_for(int ignore_bit, long vict_idnum);
  void _set_ignore_bit_for(int ignore_bit, long vict_idnum);
  void _remove_ignore_bit_for(int ignore_bit, long vict_idnum);

  // Private DB-manipulation functions.
  void _save_entry_to_db(long vict_idnum, Bitfield bitfield);
  void _delete_entry_from_db(long vict_idnum);
public:
  // A map of ignored-character idnum to a bitfield of ignored values. Uses the IGNORE_BIT defines above.
  std::unordered_map<long, Bitfield> ignored_map;

  /////////////////// Constructors and load/save functions /////////////////////
  IgnoreData(struct char_data *char_to_associate_with) {
    ch = char_to_associate_with;
  }

  void load_from_db();

  //////////////////////// Block getters and setters ///////////////////////////
  // Don't display OOC-channel messages from the target.
  void toggle_blocking_oocs(long vict_idnum, const char *vict_name, int mode);
  bool is_blocking_oocs_from(long vict_idnum) { return _ignore_bit_is_set_for(IGNORE_BIT_OOC_CHANNELS, vict_idnum); }
  bool is_blocking_oocs_from(struct char_data *vict) { return is_blocking_oocs_from(GET_IDNUM_EVEN_IF_PROJECTING(vict)); }

  // Don't display messages from the targeted character on your radio output.
  void toggle_blocking_radios(long vict_idnum, const char *vict_name, int mode);
  bool is_blocking_radios_from(long vict_idnum) { return _ignore_bit_is_set_for(IGNORE_BIT_RADIO, vict_idnum); }
  bool is_blocking_radios_from(struct char_data *vict) { return is_blocking_radios_from(GET_IDNUM_EVEN_IF_PROJECTING(vict)); }

  // Don't allow the targeted character to call you.
  void toggle_blocking_calls(long vict_idnum, const char *vict_name, int mode);
  bool is_blocking_calls_from(long vict_idnum) { return _ignore_bit_is_set_for(IGNORE_BIT_CALLS, vict_idnum); }
  bool is_blocking_calls_from(struct char_data *vict) { return is_blocking_calls_from(GET_IDNUM_EVEN_IF_PROJECTING(vict)); }

  // Prevent the target from sending you mindlinks, and breaks the mindlink if it's set.
  void toggle_blocking_mindlinks(long vict_idnum, const char *vict_name, int mode);
  bool is_blocking_mindlinks_from(long vict_idnum) { return _ignore_bit_is_set_for(IGNORE_BIT_MINDLINK, vict_idnum); }
  bool is_blocking_mindlinks_from(struct char_data *vict) { return is_blocking_mindlinks_from(GET_IDNUM_EVEN_IF_PROJECTING(vict)); }

  // Prevent the target from sending you tells.
  void toggle_blocking_tells(long vict_idnum, const char *vict_name, int mode);
  bool is_blocking_tells_from(long vict_idnum) { return _ignore_bit_is_set_for(IGNORE_BIT_TELLS, vict_idnum); }
  bool is_blocking_tells_from(struct char_data *vict) { return is_blocking_tells_from(GET_IDNUM_EVEN_IF_PROJECTING(vict)); }

  // Don't display the target's osays.
  void toggle_blocking_osays(long vict_idnum, const char *vict_name, int mode);
  bool is_blocking_osays_from(long vict_idnum) { return _ignore_bit_is_set_for(IGNORE_BIT_OSAYS, vict_idnum); }
  bool is_blocking_osays_from(struct char_data *vict) { return is_blocking_osays_from(GET_IDNUM_EVEN_IF_PROJECTING(vict)); }

  // Prevent the target from interacting with you ICly (forces on several other toggles). Includes socials, emotes/echoes, vemotes, says, shouts.
  void toggle_blocking_ic_interaction(long vict_idnum, const char *vict_name, int mode);
  bool is_blocking_ic_interaction_from(long vict_idnum) { return _ignore_bit_is_set_for(IGNORE_BIT_IC_INTERACTION, vict_idnum); }
  bool is_blocking_ic_interaction_from(struct char_data *vict) { return is_blocking_ic_interaction_from(GET_IDNUM_EVEN_IF_PROJECTING(vict)); }

  // Prevent the target from following you, and unfollow them if they are. (Abusable in PvP)
  void toggle_blocking_following(long vict_idnum, const char *vict_name, int mode);
  bool is_blocking_following_from(long vict_idnum) { return _ignore_bit_is_set_for(IGNORE_BIT_FOLLOWING, vict_idnum); }
  bool is_blocking_following_from(struct char_data *vict) { return is_blocking_following_from(GET_IDNUM_EVEN_IF_PROJECTING(vict)); }

  // Used to block the target from seeing you with the WHERE command.
  void toggle_blocking_where_visibility(long vict_idnum, const char *vict_name, int mode);
  bool is_blocking_where_visibility_for(long vict_idnum) { return _ignore_bit_is_set_for(IGNORE_BIT_WHERE, vict_idnum); }
  bool is_blocking_where_visibility_for(struct char_data *vict) { return is_blocking_where_visibility_for(GET_IDNUM_EVEN_IF_PROJECTING(vict)); }
};

#endif
