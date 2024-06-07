#include "structs.hpp"
#include "awake.hpp"
#include "interpreter.hpp"
#include "constants.hpp"
#include "db.hpp"
#include "olc.hpp"
#include "newdb.hpp"
#include "factions.hpp"

#include "nlohmann/json.hpp"
using nlohmann::json;

extern void write_json_file(bf::path path, json *contents);
extern void _json_parse_from_file(bf::path path, json &target);
void faction_edit_main_menu(struct descriptor_data * d);

#define FACTION_DEBUG(ch, ...) { if ((ch) && GET_LEVEL((ch)) == LVL_PRESIDENT) { send_to_char((ch), __VA_ARGS__ ); } }

std::map<idnum_t, Faction *> global_faction_map = {};

ACMD(do_factions) {
  char mode[MAX_INPUT_LENGTH];
  Faction *faction = NULL;

  // NPCs: Nothing
  FAILURE_CASE(IS_NPNPC(ch), "NPCs can't interact with the faction system. Please return to your original body first.");

  // Projections: Trace back to original.
  if (IS_PROJECT(ch)) {
    FAILURE_CASE(!ch->desc || ch->desc->original, "Sorry, that command won't work for you right now.");
    ch = ch->desc->original;
  }

  char *remainder = one_argument(argument, mode);

  // Everyone has access to LIST.
  if (str_str(mode, "list")) {
    FAILURE_CASE(global_faction_map.empty(), "No factions have been created yet.");

    // TODO: Staff can use 'FACTIONS LIST <x>' to show faction list for X

    if (access_level(ch, LVL_REQUIRED_FOR_GLOBAL_FACTION_EDIT)) {
      send_to_char("You can see the following factions:\r\n", ch);
    } else {
      FAILURE_CASE(!IS_SENATOR(ch) && ch->faction_rep.empty(), "You haven't encountered any factions yet.");
      send_to_char("You're aware of the following factions:\r\n", ch);
    }

    list_factions_to_char(ch);
    return;
  }

  // Everyone has access to SHOW.
  if (str_str(mode, "show")) {
    FAILURE_CASE(global_faction_map.empty(), "No factions have been created yet.");

    idnum_t idnum = atol(remainder);
    FAILURE_CASE(idnum <= 0, "Syntax: ^WFACTIONS SHOW <idnum from FACTIONS LIST>^n");
    FAILURE_CASE(!(faction = get_faction(idnum)), "That's not a valid faction ID.");
    FAILURE_CASE(faction->ch_can_list_faction(ch), "You haven't met that faction yet.");

    send_to_char(faction->get_description(), ch);

    // Staff also see the config for the faction.
    if (faction->ch_can_edit_faction(ch)) {
      send_to_char(ch, "\r\nIdnum: ^C%4ld^n   Default status: ^W%s^n\r\n", 
                   faction->get_idnum(),
                   faction_status_names[faction->get_default_status()]);
    }
    return;
  }

#define START_EDITING_FACTION(target_faction) {           \
  ch->desc->edit_faction = new target_faction;            \
  PLR_FLAGS(ch).SetBit(PLR_EDITING);                      \
  STATE(ch->desc) = CON_FACTION_EDIT;                     \
  if (!ch->desc->edit_faction->is_editor(GET_IDNUM(ch)))  \
    ch->desc->edit_faction->add_editor(GET_IDNUM(ch));    \
  faction_edit_main_menu(ch->desc);                       \
}


  if (IS_SENATOR(ch)) {
    // factions edit <idnum>: Enter OLC for a faction.
    if (str_str(mode, "edit")) {
      FAILURE_CASE(!PLR_FLAGGED(ch, PLR_OLC), "You need OLC for that.");
      FAILURE_CASE(!olc_state && access_level(ch, LVL_PRESIDENT), "OLC is globally disabled.");

      idnum_t idnum = atol(remainder);
      FAILURE_CASE(idnum <= 0, "Syntax: ^WFACTIONS EDIT <idnum from FACTIONS LIST>^n");

      FAILURE_CASE_PRINTF(!(faction = get_faction(idnum)), "There is no faction with idnum %ld. Please use an idnum from FACTIONS LIST.", idnum);
      FAILURE_CASE_PRINTF(!faction->ch_can_edit_faction(ch), "You're not an approved editor for faction %ld (%s).", faction->get_idnum(), get_faction_name(faction->get_idnum(), ch));

      // Clone it to their edit pointer, then set them to editing it.
      START_EDITING_FACTION(Faction(faction));
      return;
    }

    // factions create: Enter OLC for a new faction.
    if (str_str(mode, "create")) {
      FAILURE_CASE(!PLR_FLAGGED(ch, PLR_OLC), "You need OLC for that.");
      FAILURE_CASE(!olc_state && access_level(ch, LVL_PRESIDENT), "OLC is globally disabled.");

      // Create a new blank faction, then set them to editing it.
      START_EDITING_FACTION(Faction());
      return;
    }

    // factions delete: Remove a faction. Will have large knock-on effects.
    if (str_str(mode, "delete")) {
      FAILURE_CASE(!access_level(ch, LVL_PRESIDENT), "You must be the game owner to do that.");
      FAILURE_CASE(!PLR_FLAGGED(ch, PLR_OLC), "You need OLC for that.");
      FAILURE_CASE(!olc_state && access_level(ch, LVL_PRESIDENT), "OLC is globally disabled.");

      idnum_t idnum = atol(remainder);
      FAILURE_CASE(idnum <= 0, "Syntax: ^WFACTIONS DELETE <idnum from FACTIONS LIST>^n");

      FAILURE_CASE_PRINTF(!(faction = get_faction(idnum)), "There is no faction with idnum %ld. Please use an idnum from FACTIONS LIST.", idnum);
      FAILURE_CASE_PRINTF(!faction->ch_can_edit_faction(ch), "You're not an approved editor for faction %ld (%s).", faction->get_idnum(), get_faction_name(faction->get_idnum(), ch));

      // TODO: Remove the faction from the map.
      // TODO: Remove the faction from everyone who references it (NPCs, ...)
      // TODO: Remove the file for the faction.
      send_to_char("Not implemented yet.\r\n", ch);
      return;
    }

    if (str_str(mode, "setrep") && access_level(ch, LVL_ADMIN)) {
      // TODO: factions setrep <target> <faction_idnum> <rep>
      char target[MAX_INPUT_LENGTH], faction_idnum[MAX_INPUT_LENGTH];
      const char *rep_str = two_arguments(remainder, target, faction_idnum);

      // TODO: Find player online (search descriptors)
      // TODO: Find player offline (load char)
      // TODO: Set their rep in the selected faction
      int rep_amt = atoi(rep_str);
      return;
    }

    // Show staff syntax.
    send_to_char("Syntax: ^WFACTIONS LIST^n; or ^WFACTIONS SHOW|EDIT <idnum from list>^n; or ^WFACTIONS CREATE^n.\r\n", ch);
    if (access_level(ch, LVL_ADMIN)) {
      send_to_char("Also:   ^WFACTIONS SETREP <char name> <faction idnum> <raw rep amount>^n\r\n", ch);
    }
  } else {
    // Show player syntax.
    send_to_char("Syntax: ^WFACTIONS LIST^n; or ^WFACTIONS SHOW <idnum from list>^n.\r\n", ch);
  }
}

///////////////////////////////////////////////////////////////////////////////////////
// Class method implementation
///////////////////////////////////////////////////////////////////////////////////////

const char * Faction::render_faction_status(struct char_data *ch) {
  if (!ch) {
    mudlog("SYSERR: Got NULL ch to render_faction_status()!", ch, LOG_SYSLOG, TRUE);
    return "<error>";
  }

  int rep_to_use;

  if (ch->faction_rep.find(idnum) == ch->faction_rep.end()) {
    rep_to_use = get_default_rep();
    
    FACTION_DEBUG(ch, "^L[You have no rep in render_faction_status() call, using default rep value %d (%s).]^n\r\n",
                  rep_to_use,
                  faction_status_names[default_status]);
  } else {
    rep_to_use = ch->faction_rep[idnum];
  }

  return faction_status_names[faction_rep_to_status(rep_to_use)];
}

bool Faction::ch_can_list_faction(struct char_data *ch) {
  if (IS_SENATOR(ch)) {
    // High-level staff see all factions.
    if (access_level(ch, LVL_REQUIRED_FOR_GLOBAL_FACTION_EDIT)) {
      return TRUE;
    }
    // Otherwise, you can see them if you're an editor...
    if (ch_can_edit_faction(ch)) {
      return TRUE;
    }
    // Or if you meet standard PC requirements. Fall through.
  }
  
  // Players see any faction they have in their map
  return ch->faction_rep.find(idnum) != ch->faction_rep.end();
}

bool Faction::set_default_status(int status) {
  if (default_status < faction_statuses::HOSTILE || default_status > faction_statuses::ALLY) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Attempted to set default faction status to %d, which is outside allowed range %d ≤ X ≤ %d",
                    status,
                    faction_statuses::HOSTILE,
                    faction_statuses::ALLY);
    return FALSE;
  }

  default_status = status;
  return TRUE;
}

void Faction::remove_editor(idnum_t idnum) {
  auto it = std::remove_if(editors.begin(), editors.end(), [&](idnum_t x) { return x == idnum; });
  editors.erase(it, editors.end());
}

bool Faction::add_editor(idnum_t idnum) {
  if (!is_editor(idnum)) {
    if (!does_player_exist(idnum) || get_player_rank(idnum) < LVL_BUILDER) {
      return FALSE;
    }

    editors.push_back(idnum);
    return TRUE;
  }

  return FALSE;
}

const char * Faction::list_editors() {
  static char editor_list[2000];
  *editor_list = '\0';

  if (editors.empty()) {
    return "<no editors>";
  }

  for (auto editor_idnum : editors) {
    const char *plr_name = get_player_name(editor_idnum);
    snprintf(ENDOF(editor_list), sizeof(editor_list) - strlen(editor_list), "%s%s (%ld)", 
             *editor_list ? ", " : "",
             plr_name,
             editor_idnum);
    delete [] plr_name;
  }

  return editor_list;
}

///////////////////////////////////////////////////////////////////////////////////////
// Non-class method implentation
///////////////////////////////////////////////////////////////////////////////////////

// Gets the enum value of their faction rep.
int get_faction_status(struct char_data *ch, idnum_t faction_id) {
  return faction_rep_to_status(_get_raw_faction_rep(ch, faction_id));
}

// Gets the raw integer value of their faction rep.
int _get_raw_faction_rep(struct char_data *ch, idnum_t faction_id) {
  if (!ch || faction_id <= FACTION_IDNUM_UNDEFINED) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Got invalid parameters to _get_raw_faction_rep(%s, %ld)", GET_CHAR_NAME(ch), faction_id);
    return get_faction_status_avg_rep(faction_statuses::NEUTRAL);
  }

  auto faction_itr = ch->faction_rep.find(faction_id);

  // They have no rep with the faction. Try to give the default value.
  if (faction_itr == ch->faction_rep.end()) {
    Faction *faction = get_faction(faction_id);

    if (!faction) {
      // Faction didn't even exist.
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Got call to _get_raw_faction_rep() with invalid faction!");
      return 0;
    } else {
      // Use the default faction rep value.
      return faction->get_default_rep();
    }
  }

  return faction_itr->second;
}

// TODO: Should we trace back reflections to real bodies for rep-granting purposes? Probably.
// Alters faction status for PC. (Should we save faction status immediately on change, or wait for save tick?)
void change_faction_points(struct char_data *ch, idnum_t faction_id, int delta, int limit_to_status) {
  Faction *faction;
  if (!ch || faction_id <= 0) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Got invalid parameters to change_faction_points(%s, %ld, %d)", GET_CHAR_NAME(ch), faction_id, delta);
    return;
  }

  if (delta == 0) {
    FACTION_DEBUG(ch, "Warning: Got delta of 0 to change_faction_points(%s, %ld, %d)", GET_CHAR_NAME(ch), faction_id, delta);
    return;
  }

  if (!(faction = get_faction(faction_id))) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Got invalid faction to change_faction_points(%s, %ld, %d)", GET_CHAR_NAME(ch), faction_id, delta);
    return;
  }

  auto faction_itr = ch->faction_rep.find(faction_id);
  if (faction_itr == ch->faction_rep.end()) {
    int default_rep = faction->get_default_rep();
    ch->faction_rep[faction_id] = delta + default_rep;
    FACTION_DEBUG(ch, "Your rep with faction %ld (%s) has been initialized at %d (%d + %d).\r\n", 
                  faction_id,
                  faction->get_full_name(),
                  ch->faction_rep[faction_id],
                  default_rep,
                  delta);
    return;
  }

  if (delta > 0 && limit_to_status) {
    // Enforce limit-- don't raise above average of limit's lower and upper thresholds.
    int max_rep = get_faction_status_avg_rep(limit_to_status);

    // Maxed out.
    if (ch->faction_rep[faction_id] >= max_rep) {
      FACTION_DEBUG(ch, "Your rep with faction %ld (%s) is unchanged (status limit %s / %d, you're at %s / %d)",
                    faction_id,
                    faction->get_full_name(),
                    faction_status_names[limit_to_status],
                    max_rep,
                    faction_status_names[faction_rep_to_status(ch->faction_rep[faction_id])]);
      return;
    }

    // Almost maxed out.
    if (ch->faction_rep[faction_id] + delta > max_rep) {
      delta = max_rep - ch->faction_rep[faction_id];
      FACTION_DEBUG(ch, "Capping rep change with %ld (%s) (status limit %s / %d, you're at %s / %d)",
                    faction_id,
                    faction->get_full_name(),
                    faction_status_names[limit_to_status],
                    max_rep,
                    faction_status_names[faction_rep_to_status(ch->faction_rep[faction_id])]);
      // Delta is changed to bring us to max. Fall through.
    }

    // Below max. Fall through
  }

  FACTION_DEBUG(ch, "Your rep with faction %ld (%s) has changed from %d", faction_id, faction->get_full_name(), ch->faction_rep[faction_id]);
  ch->faction_rep[faction_id] += delta;
  FACTION_DEBUG(ch, "to %d.\r\n", ch->faction_rep[faction_id]);
}

/* Methods for NPCs to call on witnessing events. */
// Uses action_type to define if it's positive, negative, etc. Also internally defines maximum threshold you can reach by that action.
void faction_witness_saw_ch_do_action_with_optional_victim(struct char_data *witness, struct char_data *ch, int action_type, struct char_data *victim) {
  if (!witness || !ch || IS_NPNPC(ch)) { // null victim is not always an error, because this includes things like job completions etc
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Got invalid parameters to npc_saw_action_on_target_by_ch(%s, %s, %d, %s)",
                    GET_CHAR_NAME(witness),
                    GET_CHAR_NAME(ch),
                    action_type,
                    GET_CHAR_NAME(victim));
    return;
  }

  // Not an error, because we want to be able to call this on every NPC and have it checked here.
  if (!GET_MOB_FACTION_IDNUM(witness)) {
    return;
  }

  // Modify their faction points based on our alignment.
  switch (action_type) {
    case WITNESSED_NPC_ATTACKED:
      if (GET_MOB_FACTION_IDNUM(witness) == GET_MOB_FACTION_IDNUM(victim))
        change_faction_points(ch, GET_MOB_FACTION_IDNUM(witness), -5);
      break;
    case WITNESSED_NPC_KILLED:
      if (GET_MOB_FACTION_IDNUM(witness) == GET_MOB_FACTION_IDNUM(victim))
        change_faction_points(ch, GET_MOB_FACTION_IDNUM(witness), -20);
      break;
    case WITNESSED_NPC_HELPED:
      if (GET_MOB_FACTION_IDNUM(witness) == GET_MOB_FACTION_IDNUM(victim))
        change_faction_points(ch, GET_MOB_FACTION_IDNUM(witness), 3, faction_statuses::NEIGHBORLY);
      break;
    case WITNESSED_JOB_COMPLETION:
      if (GET_MOB_FACTION_IDNUM(witness) == GET_MOB_FACTION_IDNUM(victim))
        change_faction_points(ch, GET_MOB_FACTION_IDNUM(witness), 2, faction_statuses::FRIENDLY);
      break;
    default:
      mudlog_vfprintf(victim, LOG_SYSLOG, "SYSERR: Received unknown action type %d to faction_witness(%s, %s, %d, %s)",
                      GET_CHAR_NAME(witness),
                      GET_CHAR_NAME(ch),
                      action_type,
                      GET_CHAR_NAME(victim));
      break;
  }
}

/* Helper methods. */
// Returns the reputation enum value for the specified points value.
int faction_rep_to_status(int rep) {
  for (int status_idx = faction_statuses::HOSTILE; status_idx <= faction_statuses::ALLY; status_idx++) {
    if (rep >= get_faction_status_min_rep(status_idx) && rep <= get_faction_status_max_rep(status_idx))
      return status_idx;
  }

  mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Got invalid rep %d to faction_rep_to_status()!", rep);
  return faction_statuses::NEUTRAL;
}

///////////////////////////////////////////////////////////////////////////////////////
// NCM: Effects on the Game
///////////////////////////////////////////////////////////////////////////////////////

// Can you shop here?
bool faction_shop_will_deal_with_player(idnum_t faction_idnum, struct char_data *ch) {
  if (!ch || faction_idnum < 0) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid params to faction_shop_will_deal_with_player(%ld, %s)!", faction_idnum, ch);
    return FALSE;
  }
  
  return get_faction_status(ch, faction_idnum) >= faction_statuses::CAUTIOUS;
}

// Gets a multiplier for prices
float get_shop_faction_buy_from_player_multiplier(idnum_t faction_idnum, struct char_data *ch) {
  if (!ch || faction_idnum < 0) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid params to get_shop_faction_buy_from_player_multiplier(%ld, %s)!", faction_idnum, ch);
    return 0.0f;
  }

  int status = get_faction_status(ch, faction_idnum);

  switch (status) {
    case faction_statuses::HOSTILE:
      return 0.0f;
    case faction_statuses::CAUTIOUS:
      return 0.8f;
    case faction_statuses::NEUTRAL:
      return 1.0f;
    case faction_statuses::FRIENDLY:
      return 1.02f;
    case faction_statuses::NEIGHBORLY:
      return 1.04f;
    case faction_statuses::ALLY:
      return 1.06f;
    default:
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid faction status %d to get_shop_faction_sell_to_player_multiplier(%ld, %s)!", status, faction_idnum, ch);
      return 1.0f;
  }
}

float get_shop_faction_sell_to_player_multiplier(idnum_t faction_idnum, struct char_data *ch) {
  if (!ch || faction_idnum < 0) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid params to get_shop_faction_buy_from_player_multiplier(%ld, %s)!", faction_idnum, ch);
    return 100.0f;
  }

  int status = get_faction_status(ch, faction_idnum);

  switch (status) {
    case faction_statuses::HOSTILE:
      return 100.0f;
    case faction_statuses::CAUTIOUS:
      return 1.1f;
    case faction_statuses::NEUTRAL:
      return 1.0f;
    case faction_statuses::FRIENDLY:
      return 0.98f;
    case faction_statuses::NEIGHBORLY:
      return 0.96f;
    case faction_statuses::ALLY:
      return 0.94f;
    default:
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid faction status %d to get_shop_faction_buy_from_player_multiplier(%ld, %s)!", status, faction_idnum, ch);
      return 1.0f;
  }
}

// Faction hostility check. Don't @ me about the name.
bool faction_leader_says_geef_ze_maar_een_pak_slaag_voor_papa(struct char_data *klapper, struct char_data *klappee) {
  if (!klapper || !klappee || !IS_NPNPC(klapper)) {
    mudlog_vfprintf(klappee, LOG_SYSLOG, "SYSERR: Ongeldige parameters ontvangen voor de spanking-functie(%s, %s)!", GET_CHAR_NAME(klapper), GET_CHAR_NAME(klappee));
    return FALSE;
  }

  // Don't force puppeted chars to attack. Not an error.
  if (klapper->desc) {
    return FALSE;
  }

  // NPC is not part of a faction. Not an error.
  if (!GET_MOB_FACTION_IDNUM(klapper)) {
    return FALSE;
  }

  return get_faction_status(klappee, GET_MOB_FACTION_IDNUM(klapper)) <= faction_statuses::HOSTILE;
}

///////////////////////////////////////////////////////////////////////////////////////
// NCM: Saving / loading
///////////////////////////////////////////////////////////////////////////////////////

/* DB save/load is done as rows of ch_idnum, faction_idnum, and faction rating in points. 
   Faction data is saved on the PCs in an unordered map instead of being centrally stored.
*/
// Called from newdb.cpp, saves faction info to the DB.
void save_player_faction_info(struct char_data *ch) {
  char query_buf[MAX_STRING_LENGTH];
  bool printed_anything = FALSE;

  strlcpy(query_buf, "INSERT INTO pfiles_factions (`idnum`, `faction`, `rep`) VALUES (", sizeof(query_buf));
  for (auto rep_itr : ch->faction_rep) {
    snprintf(ENDOF(query_buf), sizeof(query_buf) - strlen(query_buf), "%s%ld, %ld, %d",
             printed_anything ? "), (" : "",
             GET_IDNUM(ch),
             rep_itr.first,
             rep_itr.second);
    printed_anything = TRUE;
  }

  if (printed_anything) {
    strlcat(query_buf, ");", sizeof(query_buf));
    mysql_wrapper(mysql, query_buf);
  }
}

// Called from newdb.cpp, loads faction info from the DB.
void load_player_faction_info(struct char_data *ch) {
  char query_buf[MAX_STRING_LENGTH];
  MYSQL_RES *res;
  MYSQL_ROW row;

  snprintf(query_buf, sizeof(query_buf), "SELECT `faction`, `rep` FROM pfiles_factions WHERE `idnum`=%ld;", GET_IDNUM(ch));
  mysql_wrapper(mysql, query_buf);

  if (!(res = mysql_use_result(mysql))) {
    log_vfprintf("Refusing to load faction info for %s (%ld): No result on query.", GET_CHAR_NAME(ch), GET_IDNUM(ch));
    return;
  }

  while ((row = mysql_fetch_row(res))) {
    ch->faction_rep[atol(row[0])] = atoi(row[1]);
  }
  mysql_free_result(res);
}

const bf::path factions_dir_path = bf::system_complete("lib") / "factions";

/* File save/load for factions themselves. */
Faction::Faction(bf::path file_path) {
  json faction_file;
  _json_parse_from_file(file_path, faction_file);

  idnum = (idnum_t) faction_file["idnum"].get<idnum_t>();
  full_name = str_dup(faction_file["full_name"].get<std::string>().c_str());
  description = str_dup(faction_file["description"].get<std::string>().c_str());
  default_status = (int) faction_file["default_status"].get<int>();
  editors = faction_file["editors"].get<std::vector<idnum_t>>();
}

void Faction::save() {
  json faction_file;

  faction_file["idnum"] = idnum;
  faction_file["full_name"] = full_name;
  faction_file["description"] = description;
  faction_file["default_status"] = default_status;
  faction_file["editors"] = editors;

  if (!bf::exists(factions_dir_path)) {
    bf::create_directory(factions_dir_path);
  }

  write_json_file(factions_dir_path / vnum_to_string(idnum), &faction_file);
}

void parse_factions() {
  if (!bf::exists(factions_dir_path)) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Factions directory %s did not exist. Creating it.", factions_dir_path.c_str());
    bf::create_directory(factions_dir_path);
  }

  bf::directory_iterator end_itr; // default construction yields past-the-end
  for (bf::directory_iterator itr(factions_dir_path); itr != end_itr; ++itr) {
    bf::path filename = itr->path();
    log_vfprintf(" - Initializing faction from file %s.", filename.c_str());
    Faction *faction = new Faction(filename);
    global_faction_map[faction->get_idnum()] = faction;
    log_vfprintf(" - Fully loaded %s.", faction->get_full_name());
  }
}

///////////////////////////////////////////////////////////////////////////////////////
// NCM: OLC
///////////////////////////////////////////////////////////////////////////////////////

void faction_edit_main_menu(struct descriptor_data * d) {
  send_to_char(CH, "1) Faction name:   ^c%s^n\r\n", d->edit_faction->get_full_name());
  send_to_char(CH, "2) Description:\r\n   %s^n\r\n", d->edit_faction->get_description());
  send_to_char(CH, "3) Default status: ^c%s^n\r\n", faction_status_names[d->edit_faction->get_default_status()]);
  send_to_char(CH, "4) Editors: ^c%s^n\r\n", d->edit_faction->list_editors());
  send_to_char(CH, "\r\n"
                   "q) Save and quit\r\n"
                   "x) Quit without saving\r\n"
                   "\r\n"
                   "Select a choice: ");
  d->edit_mode = FACTION_EDIT_MAIN_MENU;
}

void faction_edit_editors_menu(struct descriptor_data *d) {
  send_to_char(CH, "Current editors: %s\r\n", d->edit_faction->list_editors());
               send_to_char(CH, "Enter the name of an editor to add or remove, or press ENTER on an empty line to leave: ");
               d->edit_mode = FACTION_EDIT_EDITORS;
  d->edit_mode = FACTION_EDIT_EDITORS;
}

void faction_edit_parse(struct descriptor_data * d, const char *arg) {
  switch (d->edit_mode) {
    case FACTION_EDIT_NAME:
      d->edit_faction->set_full_name(arg);
      break;
    case FACTION_EDIT_DESC:
      // We should not get here.
      send_to_char(CH, "You look lost, friend.\r\n");
      break;
    case FACTION_EDIT_DEFAULT_STATUS:
      {
        int number = atoi(arg);
        if (number < faction_statuses::HOSTILE || number > faction_statuses::ALLY) {
          send_to_char(CH, "Unrecognized status number %d, aborting change.\r\n", number);
        } else {
          d->edit_faction->set_default_status(number);
        }
      }
      break;
    case FACTION_EDIT_EDITORS:
      {
        if (!*arg) {
          faction_edit_main_menu(d);
          return;
        }

        idnum_t editor_idnum = get_player_id(arg);
        if (editor_idnum < 0) {
          send_to_char(CH, "There is no player named %s. Try again, or hit ENTER on an empty line to abort.\r\n", arg);
          return;
        }

        if (d->edit_faction->is_editor(editor_idnum)) {
          d->edit_faction->remove_editor(editor_idnum);
        } else {
          if (!d->edit_faction->add_editor(editor_idnum)) {
            send_to_char(CH, "There is no builder named '%s'.\r\n", arg);
          }
        }
        faction_edit_editors_menu(d);
        return;
      }
      break;
    case FACTION_EDIT_MAIN_MENU:
      {
        switch (*arg) {
          case '1':
            send_to_char("Enter the faction's name: ", CH);
            d->edit_mode = FACTION_EDIT_NAME;
            return;
          case '2':
            d->edit_mode = FACTION_EDIT_DESC;
            DELETE_D_STR_IF_EXTANT(d);
            INITIALIZE_NEW_D_STR(d);
            d->max_str = MAX_MESSAGE_LENGTH;
            d->mail_to = 0;
            return;
          case '3':
            for (int status_idx = faction_statuses::HOSTILE; status_idx <= faction_statuses::ALLY; status_idx++)
              send_to_char(CH, "%d) %s\r\n", status_idx, faction_status_names[status_idx]);
            send_to_char("Select the default status that players start at with this faction: ", CH);
            d->edit_mode = FACTION_EDIT_DEFAULT_STATUS;
            return;
          case '4':
            faction_edit_editors_menu(d);
            return;
          case 'q':
          case 'Q':
            {
              idnum_t idnum = d->edit_faction->get_idnum();
              if (idnum == FACTION_IDNUM_UNDEFINED) {
                // It didn't exist. Set its idnum to max + 1.
                for (auto &faction_itr : global_faction_map) {
                  idnum = MAX(idnum, faction_itr.first);
                }
                idnum += 1;
                d->edit_faction->set_idnum(idnum);
                FACTION_DEBUG(CH, "^L[Auto-assigned idnum %ld.]^n\r\n", d->edit_faction->get_idnum());
              } else {
                // It supposedly exists. Find it and delete the existing copy.
                auto faction_itr = global_faction_map.find(idnum);

                if (faction_itr == global_faction_map.end()) {
                  mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: Could not find matching entry in global_faction_map when saving edited faction!");
                } else {
                  // Delete existing faction so we can replace it.
                  delete faction_itr->second;
                  faction_itr->second = NULL;
                  global_faction_map.erase(faction_itr);
                  FACTION_DEBUG(CH, "^L[Replaced existing faction %ld.]^n\r\n", d->edit_faction->get_idnum());
                }
              }
              // Add our new/edited copy to the map.
              global_faction_map[idnum] = d->edit_faction;
            }
            // Save it.
            d->edit_faction->save();
            // Clear our memory pointer so it doesn't get purged.
            d->edit_faction = NULL;
            // Fall through.
          case 'x':
          case 'X':
            // Delete our memory copy, if any.
            delete d->edit_faction;
            d->edit_faction = NULL;
            // Go back to playing state.
            STATE(d) = CON_PLAYING;
            PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
            send_to_char("OK.\r\n", CH);
            return;
          default:
            send_to_char("That's not a valid choice.\r\n", CH);
            break;
        }
      }
      break;
  }
  faction_edit_main_menu(d);
}

///////////////////////////////////////////////////////////////////////////////////////
// NCM: Utility
///////////////////////////////////////////////////////////////////////////////////////

// Converts idnums to faction class pointers.
Faction *get_faction(idnum_t idnum) {
  auto faction_itr = global_faction_map.find(idnum);

  if (faction_itr == global_faction_map.end())
    return NULL;

  return faction_itr->second;
}

const char *get_faction_name(idnum_t idnum, struct char_data *viewer) {
  Faction *faction = get_faction(idnum);

  if (!faction || !faction->ch_can_list_faction(viewer)) {
    return "<invalid faction>";
  }

  return faction->get_full_name();
}

void list_factions_to_char(struct char_data *ch, bool show_rep) {
  for (auto &faction_itr : global_faction_map) {
    if (faction_itr.second->ch_can_list_faction(ch)) {
      send_to_char(ch, "%3ld) %24s (%s",
                    faction_itr.first,
                    faction_itr.second->get_full_name(),
                    faction_itr.second->render_faction_status(ch));
      if (show_rep)
        send_to_char(ch, " @ %3d rep", _get_raw_faction_rep(ch, faction_itr.first));
      send_to_char(")\r\n", ch);
    }
  }
}

int get_faction_status_min_rep(int status) {
  switch (status) {
    case faction_statuses::HOSTILE:      // -151 to -500
      return -500;
    case faction_statuses::CAUTIOUS:     // -150 to -51
      return -150;
    case faction_statuses::NEUTRAL:      // -10 to 99
      return -10;
    case faction_statuses::COOPERATIVE:  // 100 to 199
      return 100;
    case faction_statuses::FRIENDLY:     // 200 to 299
      return 200;
    case faction_statuses::NEIGHBORLY:   // 300 to 399
      return 300;
    case faction_statuses::ALLY:         // 400 to 500
      return 400;
    default:
      mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Invalid status to get_faction_status_min_rep(%d)", status);
      return -500;
  }
}

int get_faction_status_max_rep(int status) {
  if (status == faction_statuses::ALLY)
    return 500;
  
  return get_faction_status_min_rep(status + 1) - 1;
}

int get_faction_status_avg_rep(int status) {
  return (get_faction_status_min_rep(status) + get_faction_status_max_rep(status)) / 2;
}