#include "structs.hpp"
#include "awake.hpp"
#include "interpreter.hpp"
#include "constants.hpp"
#include "db.hpp"
#include "factions.hpp"

#define FACTION_DEBUG(ch, ...) { if ((ch) && GET_LEVEL((ch)) == LVL_REQUIRED_FOR_FACTION_EDIT) { send_to_char((ch), __VA_ARGS__ ); } }

std::unordered_map<idnum_t, Faction *> global_faction_map = {};

ACMD(do_factions) {
  char mode[MAX_INPUT_LENGTH];

  // NPCs: Nothing
  FAILURE_CASE(IS_NPNPC(ch), "NPCs can't interact with the faction system. Please return to your original body first.");

  // Projections: Trace back to original.
  if (IS_PROJECT(ch)) {
    FAILURE_CASE(!ch->desc || ch->desc->original, "Sorry, that command won't work for you right now.");
    ch = ch->desc->original;
  }

  const char *remainder = one_argument(argument, mode);

  // Everyone has access to LIST.
  if (str_str(mode, "list")) {
    if (access_level(ch, LVL_REQUIRED_FOR_FACTION_EDIT)) {
      send_to_char("You can see the following factions:\r\n", ch);
    } else {
      FAILURE_CASE(ch->faction_rep.empty(), "You haven't encountered any factions yet.");
      send_to_char("You're aware of the following factions:\r\n", ch);
    }

    for (auto &faction : global_faction_map) {
      if (faction.second->ch_can_list_faction(ch)) {
        send_to_char(ch, "%3ld) %24s [%15s]\r\n",
                     faction.first,
                     faction.second->get_full_name(),
                     faction.second->render_faction_status(ch));
      }
    }
    return;
  }

  // Everyone has access to SHOW.
  if (str_str(mode, "show")) {
    idnum_t idnum = atol(remainder);
    FAILURE_CASE(idnum <= 0, "Syntax: ^WFACTIONS SHOW <idnum from FACTIONS LIST>^n");

    auto faction_itr = global_faction_map.find(idnum);
    FAILURE_CASE(faction_itr == global_faction_map.end(), "That's not a valid faction ID.");
    FAILURE_CASE(faction_itr->second->ch_can_list_faction(ch), "You haven't met that faction yet.");

    send_to_char(faction_itr->second->get_description(), ch);

    // Staff also see the config for the faction.
    if (access_level(ch, LVL_REQUIRED_FOR_FACTION_EDIT)) {
      send_to_char(ch, "\r\nIdnum: ^C%4ld^n   Default status: ^W%s^n\r\n", 
                   faction_itr->second->get_idnum(),
                   faction_status_names[faction_itr->second->get_default_status()]);
    }
    return;
  }

  if (access_level(ch, LVL_REQUIRED_FOR_FACTION_EDIT)) {
    #define START_EDITING_FACTION(target_faction) { ch->desc->edit_faction = new target_faction; PLR_FLAGS(ch).SetBit(PLR_EDITING); STATE(ch->desc) = CON_FACTION_EDIT; }

    // factions edit <idnum>: Enter OLC for a faction.
    if (str_str(mode, "edit")) {
      idnum_t idnum = atol(remainder);
      FAILURE_CASE(idnum <= 0, "Syntax: ^WFACTIONS EDIT <idnum from FACTIONS LIST>^n");
      FAILURE_CASE(!PLR_FLAGGED(ch, PLR_OLC), "You need OLC for that.");
      FAILURE_CASE(!olc_state && access_level(ch, LVL_PRESIDENT), "OLC is globally disabled.");

      // Look it up. If it doesn't exist, error out.
      auto faction_itr = global_faction_map.find(idnum);
      FAILURE_CASE_PRINTF(faction_itr == global_faction_map.end(), "There is no faction with idnum %ld. Please use an idnum from FACTIONS LIST.", idnum);

      // Clone it to their edit pointer, then set them to editing it.
      START_EDITING_FACTION(Faction(faction_itr->second));
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

    // Show staff syntax.
    send_to_char("Syntax: ^WFACTIONS LIST^n; or ^WFACTIONS SHOW|EDIT <idnum from list>^n; or ^WFACTIONS CREATE^n.\r\n", ch);
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

  if (ch->faction_rep.find(idnum) == ch->faction_rep.end()) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Got render_faction_status() call for ch who has no rep with faction! Using default value %d.", default_status);
    ch->faction_rep[idnum] = 0; // TODO: Use the default status, which you'll need to convert to raw faction points.
  }

  return faction_status_names[faction_rep_to_status(ch->faction_rep[idnum])];
}

bool Faction::ch_can_list_faction(struct char_data *ch) {
  // Staff see all factions.
  if (access_level(ch, LVL_REQUIRED_FOR_FACTION_EDIT)) {
    return TRUE;
  }
  
  // Players see any faction they have in their map
  else {
    return ch->faction_rep.find(idnum) != ch->faction_rep.end();
  }
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

///////////////////////////////////////////////////////////////////////////////////////
// Non-class method implentation
///////////////////////////////////////////////////////////////////////////////////////

// Gets the enum value of their faction rep.
int get_faction_status(struct char_data *ch, idnum_t faction_id) {
  return faction_rep_to_status(_get_raw_faction_rep(ch, faction_id));
}

// Gets the raw integer value of their faction rep.
int _get_raw_faction_rep(struct char_data *ch, idnum_t faction_id) {
  if (!ch || faction_id <= 0) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Got invalid parameters to _get_raw_faction_rep(%s, %ld)", GET_CHAR_NAME(ch), faction_id);
    return 0; // TODO: Map this to neutral.
  }

  auto faction_itr = ch->faction_rep.find(faction_id);

  if (faction_itr == ch->faction_rep.end()) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Got call to _get_raw_faction_rep() when ch had no rep with faction!");
    return 0; // TODO STRETCH: Change this to default faction rep value.
  }

  return faction_itr->second;
}

// TODO: Should we trace back reflections to real bodies for rep-granting purposes? Probably.
// Alters faction status for PC. (Should we save faction status immediately on change, or wait for save tick?)
void change_faction_points(struct char_data *ch, idnum_t faction_id, int delta, int limit) {
  if (!ch || faction_id <= 0) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Got invalid parameters to change_faction_points(%s, %ld, %d)", GET_CHAR_NAME(ch), faction_id, delta);
    return;
  }

  if (delta == 0) {
    FACTION_DEBUG(ch, "Warning: Got delta of 0 to change_faction_points(%s, %ld, %d)", GET_CHAR_NAME(ch), faction_id, delta);
    return;
  }

  auto faction_itr = ch->faction_rep.find(faction_id);

  if (faction_itr == ch->faction_rep.end()) {
    FACTION_DEBUG(ch, "Your rep with faction %ld has been initialized at %d.\r\n", faction_id, delta);
    ch->faction_rep[faction_id] = delta;
    return;
  }

  if (delta > 0 && limit) {
    // TODO: Enforce limit-- don't raise above average of limit's lower and upper thresholds.
  }

  FACTION_DEBUG(ch, "Your rep with faction %ld has changed from %d", faction_id, ch->faction_rep[faction_id]);
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
  // TODO
  return faction_statuses::NEUTRAL;
}

///////////////////////////////////////////////////////////////////////////////////////
// NCM: Effects on the Game
///////////////////////////////////////////////////////////////////////////////////////

// Can you shop here?
bool faction_shop_will_deal_with_player(struct shop_data *shop, struct char_data *ch);

// Gets a multiplier for prices
float get_shop_faction_buy_from_player_multiplier(struct shop_data *shop, struct char_data *ch);
float get_shop_faction_sell_to_player_multiplier(struct shop_data *shop, struct char_data *ch);

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
void save_player_faction_info(struct char_data *ch);

// Called from newdb.cpp, loads faction info from the DB.
void load_player_faction_info(struct char_data *ch);

/* File save/load for factions themselves. */
void parse_factions();
void save_factions();

///////////////////////////////////////////////////////////////////////////////////////
// NCM: OLC
///////////////////////////////////////////////////////////////////////////////////////

// TODO: If faction idnum is not set on 'Y', set it before saving.
void faction_edit_parse(struct descriptor_data * d, const char *arg);