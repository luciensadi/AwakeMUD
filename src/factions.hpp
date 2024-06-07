/* prototype / design: NPCs belong to factions, which matter to players because you can accrue rep with factions.
   - [f] if a faction NPC sees you help their cause, you gain rep
     - [f] killing their enemies can only take you up to Cooperative so folks don't murder-hobo their way into Ally with everyone
     - quests can be flagged as 'ally-worthy', if not flagged as such they can only take you up to Friendly. No being an ally by endlessly running packages

   - [i] if a faction NPC sees you attack / kill someone on their side, you lose rep
   - TODO: if they can't see you (invis etc, or firing from outside of their vision range), they don't report in about you

   - [√] FACTION LIST
   -    [i] as a low-level builder who can't see them all
   - [√] FACTION SHOW x
   - [√] FACTION EDIT x
   - [√] FACTION CREATE
   - [p] FACTION SETREP ch x y
   - [p] FACTION DELETE x

  Why this matters to players:
  - [i] Low rep gets you attacked on sight by faction guards and puts faction non-guard NPCs on alert when they see you
  - [i] Discount / surcharge that scales on rep with the store owner's faction, refusal to sell on Hostile
  - High rep gets you access to more jobs with them?
  - Bouncers that don't let you pass without certain faction status ratings.

  STRETCH:
  - Factions have leaders. When a leader NPC is killed, they don't leave a corpse, and instead are "docwagoned" like a PC using zone-appropriate messaging before being extracted to enable respawn.
  - Attacking the leader is bad and gets you a big rep penalty.
  - You can buy faction rep up to Friendly by paying the leader. No, you can't get your money back by then killing the leader.
  - High rep ratings get you occasional upnods and recognition in passing. Low ones get you scowls and threatening moves from non-wimpy chars.
  - if you're disguised somehow (figure this out), NPCs don't report in unless they see through the disguise (perception check?)
  - Hosts belong to factions, and if they trace you back, you lose a lot of rep.
  - Failing a quest loses rep

  SUPER STRETCH:
  - Figure out a way for players to see which faction an NPC belongs to that doesn't completely break immersion
  - Faction shops can have goods available at different rep levels (no buying X until ally)
  - Wimpy faction mobs flee when a hostile-rep player shows up.
  - Rooms can be flagged as no-trespassing with a severity rating. If you're caught in one, you get a faction rep penalty (w/ cooldown) and may be attacked.
  - Doing harmful quests can lower the opposing faction's view of you (e.g. raid Mitsu on a job, Mitsu's rep goes down)

KEY: 
- p: prototyped
- f: implemented in faction code, but not rolled out to rest of codebase
- i: implemented in whole codebase / ready for testing
- √: tested / done
*/

// √ Figure out how to show builders factions in their zones when they haven't met them yet. [staff must set rep? Or editor list]
// √ If editor list, then: FACTION CREATE makes you an editor of it; you always have view permissions for it.
// √ Save/load mob faction affiliation.

// TODO: Staff can set PC's faction rep
// TODO: Non-editors of protected zones can't see factions from protected zones
// STRETCH: Allow lower-level builders to edit them
// STRETCH: Jobs can have faction rep impacts on other factions (e.g. "steal this thing from Mitsuhama", giver is pleased, mitsu is not)

// TODO: Ensure that you only get the witness penalty from hitting someone once per combat

#include <map>
#include <boost/filesystem.hpp>

#include "structs.hpp"

//////////// Configurables
#define FACTION_REP_DELTA_NPC_ATTACKED  -3
#define FACTION_REP_DELTA_NPC_KILLED    -20
// TODO below this line
#define FACTION_REP_DELTA_NPC_HELPED    1
#define FACTION_REP_DELTA_JOB_COMPLETED 2
#define FACTION_REP_DELTA_JOB_FAILED    -1
//////////////////////////

extern char *str_dup(const char *source);

#define LVL_REQUIRED_FOR_GLOBAL_FACTION_EDIT LVL_ADMIN

#define WITNESSED_NPC_ATTACKED   1
#define WITNESSED_NPC_KILLED     2
#define WITNESSED_NPC_HELPED     3
#define WITNESSED_JOB_COMPLETION 4
#define WITNESSED_JOB_FAILURE    5

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

int get_faction_status_min_rep(int status);
int get_faction_status_max_rep(int status);
int get_faction_status_avg_rep(int status);

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
  Faction() :
    full_name(str_dup("<unnamed faction>")), description(str_dup("<not yet described>"))
  {};

  ~Faction() {
    delete full_name;
    delete description;
  }

  void save();

  idnum_t get_idnum()           { return idnum; }
  const char *get_full_name()   { if (full_name)   return full_name;   return "<unnamed faction>"; }
  const char *get_description() { if (description) return description; return "<not yet described>"; }
  int get_default_status()      { return default_status; }
  int get_default_rep()         { return (default_status == faction_statuses::NEUTRAL ? 0 : get_faction_status_avg_rep(default_status)); }

  void set_full_name(const char *str)   { delete [] full_name;   full_name   = str_dup(str); }
  void set_description(const char *str) { delete [] description; description = str_dup(str); }
  bool set_default_status(int status);
  void set_idnum(idnum_t new_idnum)     { idnum = new_idnum; }

  // Test for if a builder can work with this faction or not.
  bool ch_can_edit_faction(struct char_data *ch) { return GET_LEVEL(ch) == LVL_PRESIDENT; }
  bool ch_can_list_faction(struct char_data *ch);
  const char *list_editors();

  // Returns a faction status string (Friendly etc)
  const char *render_faction_status(struct char_data *ch);

  // Add or remove editors.
  bool is_editor(idnum_t idnum) { for (auto v_id : editors) { if (idnum == v_id) return TRUE; } return FALSE; }
  bool add_editor(idnum_t idnum);
  void remove_editor(idnum_t idnum);
  std::vector<idnum_t> *get_editors() { return &editors; }
};

extern std::map<idnum_t, Faction *> global_faction_map;


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
bool faction_shop_will_deal_with_player(idnum_t faction_idnum, struct char_data *ch);

// Gets a multiplier for prices
float get_shop_faction_buy_from_player_multiplier(idnum_t faction_idnum, struct char_data *ch);
float get_shop_faction_sell_to_player_multiplier(idnum_t faction_idnum, struct char_data *ch);

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

///////////////////////////////////////////////////////////////////////////////////////
// OLC / utility
///////////////////////////////////////////////////////////////////////////////////////

void faction_edit_parse(struct descriptor_data * d, const char *arg);

///// utility
Faction *get_faction(idnum_t idnum);

const char *get_faction_name(idnum_t idnum, struct char_data *viewer);

void list_factions_to_char(struct char_data *ch, bool show_rep=TRUE);