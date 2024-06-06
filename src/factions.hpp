/* prototype / design: NPCs belong to factions, which matter to players because you can accrue rep with factions.
   - [p] if a faction NPC sees you help their cause, you gain rep
     - killing their enemies can only take you up to Cooperative so folks don't murder-hobo their way into Ally with everyone
     - quests can be flagged as 'ally-worthy', if not flagged as such they can only take you up to Friendly. No being an ally by endlessly running packages

   - [p] if a faction NPC sees you attack someone on their side, you lose rep
     - this can take you all the way to Hostile

   - if you're disguised somehow (figure this out), they don't report in unless they see through the disguise
     (perception check?)

   - if they can't see you (invis etc, or firing from outside of their vision range), they don't report in about you
     - this will increase pressure on me to write ranged magic... ugh

  Why this matters to players:
  - [p] Low rep gets you attacked on sight by faction guards and puts faction non-guard NPCs on alert when they see you
  - [p] Discount / surcharge that scales on rep with the store owner's faction, refusal to sell on Hostile
  - High rep gets you access to more jobs with them?
  - Bouncers that don't let you pass without certain faction status ratings.

  STRETCH:
  - Factions have leaders. When a leader NPC is killed, they don't leave a corpse, and instead are "docwagoned" like a PC using zone-appropriate messaging before being extracted to enable respawn.
  - Attacking the leader is bad and gets you a big rep penalty.
  - You can buy faction rep up to Friendly by paying the leader. No, you can't get your money back by then killing the leader.
  - High rep ratings get you occasional upnods and recognition in passing. Low ones get you scowls and threatening moves from non-wimpy chars.

  SUPER STRETCH:
  - Figure out a way for players to see which faction an NPC belongs to that doesn't completely break immersion
  - Faction shops can have goods available at different rep levels (no buying X until ally)
  - Wimpy faction mobs flee when a hostile-rep player shows up.
  - Rooms can be flagged as no-trespassing with a severity rating. If you're caught in one, you get a faction rep penalty (w/ cooldown) and may be attacked.

KEY: 
- p: prototyped
- i: implemented
- √: tested / done
*/

// TODO: Staff can set someone's faction rep
// TODO: Non-editors of protected zones can't see factions from protected zones
// STRETCH: Allow lower-level builders to edit them
// STRETCH: Jobs can have faction rep impacts on other factions (e.g. "steal this thing from Mitsuhama", giver is pleased, mitsu is not)

#include <unordered_map>
#include <boost/filesystem.hpp>

#include "structs.hpp"

extern char *str_dup(const char *source);

#define LVL_REQUIRED_FOR_FACTION_EDIT LVL_ADMIN

#define WITNESSED_NPC_ATTACKED   1
#define WITNESSED_NPC_KILLED     2
#define WITNESSED_NPC_HELPED     3
#define WITNESSED_JOB_COMPLETION 4

enum faction_statuses {
  HOSTILE = 0,
  CAUTIOUS,
  NEUTRAL,
  COOPERATIVE,
  FRIENDLY,
  NEIGHBORLY,
  ALLY
};

#define FACTION_IDNUM_UNDEFINED 0

class Faction {
  idnum_t idnum = FACTION_IDNUM_UNDEFINED;  // 123
  const char *full_name = NULL;             // "The Ancients (Seoulpa)"
  const char *description = NULL;           // A paragraph about the gang.
  int default_status = faction_statuses::NEUTRAL;

  std::vector<idnum_t> editors = {};
public:

  // Used for loading a faction from disk.
  Faction(idnum_t idnum, const char *full_name, const char *description, int default_status):
    idnum(idnum), full_name(str_dup(full_name)), description(str_dup(description)), default_status(default_status)
  {};

  // Clone an existing faction for editing.
  Faction(Faction *clone_from):
    idnum(clone_from->idnum), full_name(str_dup(clone_from->full_name)), description(str_dup(clone_from->description)), default_status(clone_from->default_status)
  {};

  // Loads a faction from a given filename.
  Faction(boost::filesystem::path filename);

  // Creates a new empty faction for editing.
  Faction() {};

  ~Faction() {
    delete full_name;
    delete description;
  }

  idnum_t get_idnum()           { return idnum; }
  const char *get_full_name()   { if (full_name)   return full_name;   return "<unnamed faction>"; }
  const char *get_description() { if (description) return description; return "<not yet described>"; }
  int get_default_status()      { return default_status; }

  void set_full_name(const char *str)   { delete [] full_name;   full_name   = str_dup(str); }
  void set_description(const char *str) { delete [] description; description = str_dup(str); }
  bool set_default_status(int status);

  // Test for if a builder can work with this faction or not.
  bool ch_can_edit_faction(struct char_data *ch) { return GET_LEVEL(ch) == LVL_PRESIDENT; }

  // Returns a faction status string (Friendly etc)
  const char *render_faction_status(struct char_data *ch);

  bool ch_can_list_faction(struct char_data *ch);
};

extern std::unordered_map<idnum_t, Faction *> global_faction_map;


///////////////////////////////////////////////////////////////////////////////////////
// Faction rep / goodwill changers
///////////////////////////////////////////////////////////////////////////////////////

/* Methods to CRUD PC info. All methods create at faction default first if they had no prior dealings. */
// Returns the reputation enum value for the char with the faction.
int get_faction_status(struct char_data *ch, idnum_t faction_id);
int _get_raw_faction_rep(struct char_data *ch, idnum_t faction_id);

// Alters faction status for PC. (Should we save faction status immediately on change, or wait for save tick?)
void change_faction_points(struct char_data *ch, idnum_t faction_id, int delta, int limit=0);

/* Methods for NPCs to call on witnessing events. */
// Uses action_type to define if it's positive, negative, etc.
void faction_witness_saw_ch_do_action_with_optional_victim(struct char_data *witness, struct char_data *ch, int action_type, struct char_data *victim=NULL);

/* Helper methods. */
// Returns the reputation enum value for the specified points value.
int faction_rep_to_status(int rep);

///////////////////////////////////////////////////////////////////////////////////////
// Effects on the Game
///////////////////////////////////////////////////////////////////////////////////////

// Can you shop here?
bool faction_shop_will_deal_with_player(struct shop_data *shop, struct char_data *ch);

// Gets a multiplier for prices
float get_shop_faction_buy_from_player_multiplier(struct shop_data *shop, struct char_data *ch);
float get_shop_faction_sell_to_player_multiplier(struct shop_data *shop, struct char_data *ch);

// Faction hostility check. Don't @ me about the name.
bool faction_leader_says_geef_ze_maar_een_pak_slaag_voor_papa(struct char_data *klapper, struct char_data *klappee);

///////////////////////////////////////////////////////////////////////////////////////
// Saving / loading
///////////////////////////////////////////////////////////////////////////////////////

/* DB save/load is done as rows of ch_idnum, faction_idnum, and faction rating in points. 
   Faction data is saved on the PCs in an unordered map instead of being centrally stored.
*/
// Called from newdb.cpp, saves faction info to the DB.
void save_player_faction_info(struct char_data *ch);

// Called from newdb.cpp, loads faction info from the DB.
void load_player_faction_info(struct char_data *ch);

/* File save/load for factions themselves. */
void parse_factions();
void save_individual_faction(Faction *faction);
void save_factions();

///////////////////////////////////////////////////////////////////////////////////////
// OLC
///////////////////////////////////////////////////////////////////////////////////////

// TODO: If faction idnum is not set on 'Y', set it before saving.
void faction_edit_parse(struct descriptor_data * d, const char *arg);

/////////// Convenience defines
#define GET_MOB_FACTION_IDNUM(mob) (mob)->char_specials.npc_faction_membership