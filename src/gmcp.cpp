/******************************************************************************
 Header files.
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/telnet.h>
#include <time.h>
#include <ctype.h>

#include "gmcp.hpp"
#include "utils.hpp"
#include "awake.hpp"
#include "structs.hpp"
#include "constants.hpp"
#include "comm.hpp"
#include "protocol.hpp"
#include "config.hpp"
#include "nlohmann/json.hpp"
#include "db.hpp"
#include "protocol.hpp"
#include "newmatrix.hpp"
#include "quest.hpp"
#include "zoomies.hpp"

using nlohmann::json;

#define DISCORD_BUFFER_SIZE 200

void generate_discord_details(descriptor_t *apDescriptor, char *smallimage, char *smallimagetext, char *details, char *state);

static void Write( descriptor_t *apDescriptor, const char *apData )
{
  if ( apDescriptor != NULL ) {
   write_to_descriptor( apDescriptor->descriptor, apData );
  }
}

/******************************************************************************
 GMCP functions.
 ******************************************************************************/
void SendGMCPCoreSupports ( descriptor_t *apDescriptor )
{
  if (!apDescriptor || !apDescriptor->pProtocol->bGMCP) return;
  json j;

  j["Core"] = json::array();
  j["Core"].push_back("Supports");

  j["Room"] = json::array();
  j["Room"].push_back("Info");
  j["Room"].push_back("Exits");

  j["Char"] = json::array();
  j["Char"].push_back("Vitals");
  j["Char"].push_back("Info");
  j["Char"].push_back("Pools");

  j["Matrix"] = json::array();
  j["Matrix"].push_back("Info");
  j["Matrix"].push_back("Deck");

  j["External"] = json::array();
  j["External"].push_back("Discord");

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(apDescriptor, "Core.Supports", payload.c_str());
}

void RegisterGMCPDiscordHello( descriptor_t *apDescriptor, const json &payload )
{
  // Log their Discord username for the character. Later on we'll associate this in a more permanent manner.
  auto user_it = payload.find("user");
  // auto private_it = payload.find("private");
  if (user_it != payload.end()) {
    if (apDescriptor->character) {
      mudlog_vfprintf(apDescriptor->character, LOG_SYSLOG, "Got Discord.Hello from %s for %s.", user_it->get<std::string>().c_str(), GET_CHAR_NAME(apDescriptor->character));
    } else {
      mudlog_vfprintf(NULL, LOG_SYSLOG, "Got Discord.Hello from %s before login, so unable to associate with a character.", user_it->get<std::string>().c_str());
    }
  }

  log_vfprintf("Got GMCP Discord HELLO from %s: %s", GET_CHAR_NAME(apDescriptor->character), payload.dump().c_str());
}

void SendGMCPDiscordInfo ( descriptor_t *apDescriptor)
{
  json j;

  j["inviteurl"] = DISCORD_SERVER_URL;
  j["applicationid"] = DISCORD_APP_ID;
  
  // Dump the json to a string and send it.
  std::string send_payload = j.dump();
  SendGMCP(apDescriptor, "External.Discord.Info", send_payload.c_str());

  log_vfprintf("Sending GMCP Discord INFO to %s: %s", GET_CHAR_NAME(apDescriptor->character), send_payload.c_str());
}

// { smallimage: ["iconname", "iconname2", "iconname3"], smallimagetext: "Icon hover text", details: "Details String", state: "State String", partysize: 0, partymax: 10, game: "Achaea", starttime: "timestamp for start" }
void SendGMCPDiscordStatus ( descriptor_t *apDescriptor )
{
  json j;

  char details[DISCORD_BUFFER_SIZE] = {0};
  char state[DISCORD_BUFFER_SIZE] = {0};
  char smallimagetext[DISCORD_BUFFER_SIZE] = {0};
  char smallimage[DISCORD_BUFFER_SIZE] = {0};

  // Set smallimage, smallimagetext, details, and state values here.
  generate_discord_details(apDescriptor, smallimage, smallimagetext, details, state);

  j["smallimage"] = json::array({
    // FYI, icons are: foreground color #02CFB9, shadow color #00AC99 blur 11, stroke color #8DD4CD width 8. Background is black to #3B0769 in a vertical-down gradient.
    smallimage
  });

  j["smallimagetext"] = smallimagetext;

  j["largeimage"] = json::array({
    // FYI, icons are color #0072FF on #000000
    "awcevile-skyline02" // "seattle_synthwave" //"space_needle2" // largeimage
  });
  j["largeimagetext"] = "awakemud.com";

  j["details"] = details;
  j["state"] = state;
  // j["partysize"] = 0;
  // j["partymax"] = 0;
  j["game"] = "AwakeMUD CE";
  // j["starttime"] = apDescriptor->login_time;

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(apDescriptor, "External.Discord.Status", payload.c_str());

  log_vfprintf("Sending GMCP Discord status to %s: %s", GET_CHAR_NAME(apDescriptor->character), payload.c_str());
}

void SendCustomGMCPDiscordStatus ( descriptor_t *apDescriptor, const char *smallimage, const char *smallimagetext, const char *details, const char *state)
{
  json j;

  j["smallimage"] = json::array({
    smallimage
  });

  j["smallimagetext"] = smallimagetext; // todo
  j["details"] = details;
  j["state"] = state;
  j["partysize"] = 0;
  j["partymax"] = 0;
  j["game"] = "AwakeMUD CE";
  j["starttime"] = apDescriptor->login_time;

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(apDescriptor, "External.Discord.Status", payload.c_str());

  log_vfprintf("Sending GMCP Discord status to %s: %s", GET_CHAR_NAME(apDescriptor->character), payload.c_str());
}

void SendGMCPMatrixInfo ( struct char_data *ch )
{
  if (!ch->desc || !ch->desc->pProtocol->bGMCP) return;
  struct matrix_icon *persona = ch->persona;

  json j;

  if (!persona || !persona->in_host || !persona->decker) {
    std::string payload = j.dump();
    SendGMCP(ch->desc, "Matrix.Info", payload.c_str());
    return;
  }

  struct host_data *cur_host = &matrix[persona->in_host];
  j["name"] = cur_host->name;
  j["description"] = cur_host->desc;
  j["vnum"] = cur_host->vnum;
  j["icons"] = json::array();
  for (struct matrix_icon *icon = cur_host->icons; icon; icon = icon->next_in_host) {
    if (!has_spotted(persona, icon)) continue;
    j["icons"].push_back(icon->name);
  }

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(ch->desc, "Matrix.Info", payload.c_str());
}

void SendGMCPMatrixDeck ( struct char_data *ch )
{
  if (!ch->desc || !ch->desc->pProtocol->bGMCP) return;
  struct matrix_icon *persona = ch->persona;

  json j;

  if (!persona || !persona->in_host || !persona->decker) {
    std::string payload = j.dump();
    SendGMCP(ch->desc, "Matrix.Deck", payload.c_str());
    return;
  }

  struct obj_data *deck = persona->decker->deck;
  j["deck"] = json::object();
  j["persona"] = json::object();

  // Deck info
  j["deck"]["name"] = GET_OBJ_NAME(deck);
  j["deck"]["mpcp"] = persona->decker->mpcp;
  j["deck"]["bod"] = persona->decker->bod;
  j["deck"]["masking"] = persona->decker->masking;
  j["deck"]["evasion"] = persona->decker->evasion;
  j["deck"]["sensor"] = persona->decker->sensor;
  j["deck"]["io"] = persona->decker->io;

  // Active programs
  j["deck"]["programs"] = json::array();
  for (struct obj_data *soft = persona->decker->software; soft; soft = soft->next_content) {
    json jsoft;

    jsoft["name"] = GET_OBJ_NAME(soft);
    jsoft["rating"] = GET_PROGRAM_RATING(soft);
    jsoft["type"] = GET_PROGRAM_TYPE(soft);

    j["deck"]["programs"].push_back(jsoft);
  }
  
  // Persona info
  j["persona"]["name"] = persona->name;
  j["persona"]["condition"] = persona->condition;

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(ch->desc, "Matrix.Deck", payload.c_str());
}

void SendGMCPCharInfo( struct char_data * ch )
{
  if (!ch || !ch->desc || !ch->desc->pProtocol->bGMCP) return;
  json j;
  
  j["name"] = GET_CHAR_NAME(ch);
  j["height"] = ((float)GET_HEIGHT(ch) / 100);
  j["weight"] = GET_WEIGHT(ch);
  j["tke"] = GET_TKE(ch);
  j["rep"] = GET_REP(ch);
  j["noto"] = GET_NOT(ch);
  j["metatype"] = pc_race_types_for_wholist[(int) GET_RACE(ch)];
  j["pronouns"] = genders[(int) ch->player.pronouns];
  j["syspoints"] = GET_SYSTEM_POINTS(ch);

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(ch->desc, "Char.Info", payload.c_str());
}

json SerializeRoomExits( struct room_data *room )
{
  json j;
  
  for (int door = 0; door < NUM_OF_DIRS; door++) {
    if (!room->dir_option[door]) continue;
    if (!room->dir_option[door]->to_room) continue;
    if (room->dir_option[door]->to_room == &world[0]) continue;
    if (IS_SET(room->dir_option[door]->exit_info, EX_HIDDEN)) continue;
    
    json exit = json::object();
    exit["direction"] = exitdirs[door];
    exit["to"] = room->dir_option[door]->to_room_vnum;
    if (IS_SET(room->dir_option[door]->exit_info, EX_LOCKED)) {
      exit["state"] = "LOCKED";
    } else if (IS_SET(room->dir_option[door]->exit_info, EX_CLOSED)) {
      exit["state"] = "CLOSED";
    } else {
      exit["state"] = "OPEN";
    }

    j.push_back(exit);
  }

  return j;
}

void SendGMCPExitsInfo( struct char_data *ch ) 
{
  if (!ch || !ch->desc || !ch->desc->pProtocol->bGMCP) return;
  if (!ch->in_room) return;

  json j;
  j["exits"] = SerializeRoomExits(ch->in_room);
  j["room_vnum"] = GET_ROOM_VNUM(ch->in_room);
  j["room_name"] = GET_ROOM_NAME(ch->in_room);

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(ch->desc, "Room.Exits", payload.c_str());
}

void SendGMCPRoomInfo( struct char_data *ch, struct room_data *room ) 
{
  if (!ch || !ch->desc || !ch->desc->pProtocol->bGMCP) return;
  json j;

  j["vnum"] = GET_ROOM_VNUM(room);
  j["name"] = GET_ROOM_NAME(room);

  // Zone info: Not necessary maybe
  // struct zone_data *z = &zone_table[room->zone];
  // j["zone"] = json::object();
  // j["zone"]["number"] = z->number;
  // j["zone"]["name"] = z->name;

  // Flags
  j["flags"] = json::array();
  if (ROOM_FLAGGED(room, ROOM_GARAGE)) j["flags"].push_back("garage");
  if (ROOM_FLAGGED(room, ROOM_STORAGE) && !ROOM_FLAGGED(room, ROOM_CORPSE_SAVE_HACK)) j["flags"].push_back("storage");
  if (ROOM_FLAGGED(room, ROOM_STERILE)) j["flags"].push_back("sterile");
  if (ROOM_FLAGGED(room, ROOM_PEACEFUL)) j["flags"].push_back("peaceful");
  if ((room->matrix && real_host(room->matrix) >= 1)) j["flags"].push_back("jackpoint");
  if (ROOM_FLAGGED(room, ROOM_ENCOURAGE_CONGREGATION)) j["flags"].push_back("socialization bonus");

  
  j["exits"] = SerializeRoomExits(room);

  // Only add coordinates if they are valid.
  j["coords"] = { {"x", room->x}, {"y", room->y}, {"z", room->z} };

  // Add room description if it exists; any quotes will be automatically escaped.
  if (*room->description) {
    j["description"] = room->description;
  }
  
  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(ch->desc, "Room.Info", payload.c_str());
}

void SendGMCP( descriptor_t *apDescriptor, const char *module, const char *apData )
{
  if (!apDescriptor->pProtocol->bGMCP) return;
  char buf[MAX_STRING_LENGTH];

  // Build the GMCP message: IAC SB GMCP [module] [json] IAC SE
  snprintf(buf, sizeof(buf), "%c%c%c%s %s%c%c", IAC, SB, TELOPT_GMCP, module, apData, IAC, SE);

  Write(apDescriptor, buf);
}

void SendGMCPCharVitals( struct char_data * ch )
{
  if (!ch || !ch->desc || !ch->desc->pProtocol->bGMCP) return;
  json j;

  struct obj_data *editor = find_bioware(ch, BIO_PAINEDITOR);

  j["physical"] = editor && GET_BIOWARE_IS_ACTIVATED(editor) ? GET_MAX_PHYSICAL(ch) : GET_PHYSICAL(ch);
  j["mental"] = editor && GET_BIOWARE_IS_ACTIVATED(editor) ? GET_MAX_MENTAL(ch) : GET_MENTAL(ch);
  j["physical_max"] = GET_MAX_PHYSICAL(ch);
  j["mental_max"] = GET_MAX_MENTAL(ch);
  j["karma"] = GET_KARMA(ch);
  j["nuyen"] = GET_NUYEN(ch);
  j["carrying_weight"] = IS_CARRYING_W(ch);
  j["max_carry_weight"] = CAN_CARRY_W(ch);
  j["armor"] = json::object();
  j["armor"]["ballistic"] = GET_BALLISTIC(ch);
  j["armor"]["impact"] = GET_IMPACT(ch);

  const char *domain_str = get_ch_domain_str(ch, TRUE);
  if (domain_str && *domain_str) 
    j["domain"] = domain_str;

  // Current Ammo Handler
  struct obj_data *weapon = GET_EQ(ch, WEAR_WIELD);
  if (weapon && GET_OBJ_TYPE(weapon) == ITEM_WEAPON && WEAPON_IS_GUN(weapon)) {
    if (weapon->contains) {
      j["ammo"] = MIN(GET_WEAPON_MAX_AMMO(weapon), GET_MAGAZINE_AMMO_COUNT(weapon->contains));
    } else {
      j["ammo"] = 0;
    }
    j["max_ammo"] =  GET_WEAPON_MAX_AMMO(weapon);
  } else {
    j["max_ammo"] = 0;
  }

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(ch->desc, "Char.Vitals", payload.c_str());
}

void SendGMCPCharPools( struct char_data * ch )
{
  if (!ch || !ch->desc || !ch->desc->pProtocol->bGMCP) return;
  json j;

  j["pools"] = json::object();
  j["pools"]["combat"] = json::object();
  j["pools"]["combat"]["body"] = GET_BODY_POOL(ch);
  j["pools"]["combat"]["dodge"] = GET_DODGE(ch);
  j["pools"]["combat"]["offense"] = GET_OFFENSE(ch);
  j["pools"]["combat"]["max"] = GET_COMBAT_POOL(ch);

  j["task_pools"] = json::object();
  for (int x = 0; x < 7; x++) {
    j["task_pools"][attributes[x]] = GET_TASK_POOL(ch, x);  
  }

  if (GET_ASTRAL(ch) > 0)
    j["pools"]["astral"] = GET_ASTRAL(ch);
  if (GET_HACKING(ch) > 0)
    j["pools"]["hacking"] = GET_HACKING(ch);
  if (GET_MAGIC_POOL(ch) > 0) {
    j["pools"]["magic"] = json::object();
    j["pools"]["magic"]["spell"] = GET_MAGIC_POOL(ch);
    j["pools"]["magic"]["casting"] = GET_CASTING(ch);
    j["pools"]["magic"]["drain"] = GET_DRAIN(ch);
    j["pools"]["magic"]["defense"] = GET_SDEFENSE(ch);
    if (GET_METAMAGIC(ch, META_REFLECTING) == 2)
      j["pools"]["magic"]["reflecting"] = GET_REFLECT(ch);
  }

  if (GET_CONTROL(ch) > 0)
    j["pools"]["control"] = GET_CONTROL(ch);

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(ch->desc, "Char.Pools", payload.c_str());
}


void ExecuteGMCPMessage(descriptor_t *apDescriptor, const char *module, const json &payload) {
  // log_vfprintf("GMCP module %s, payload %s", module, payload.dump().c_str());
  if (!strncmp(module, "Core.Supports.Get", strlen(module)))
    SendGMCPCoreSupports(apDescriptor);
  else if (!strncmp(module, "Room.Info.Get", strlen(module))) {
    if (!apDescriptor->character) {
      mudlog_vfprintf(apDescriptor->character, LOG_SYSLOG, "Unable to Handle GMCP Room.Info.Get Call: No Valid Character");
      return;
    }

    struct room_data *in_room = get_ch_in_room(apDescriptor->character);
    
    if (!in_room) {
      mudlog_vfprintf(apDescriptor->character, LOG_SYSLOG, "Unable to Handle GMCP Room.Info.Get Call: Not in Room");
      return;
    }
    
    SendGMCPRoomInfo(apDescriptor->character, in_room);
  } else if (!strncmp(module, "Room.Exits.Get", strlen(module)))
    if (!apDescriptor->character)
      mudlog_vfprintf(apDescriptor->character, LOG_SYSLOG, "Unable to Handle GMCP Room.Exits.Get Call: No Valid Character");
    else
      SendGMCPExitsInfo(apDescriptor->character);
  else if (!strncmp(module, "Char.Vitals.Get", strlen(module)))
    if (!apDescriptor->character)
      mudlog_vfprintf(apDescriptor->character, LOG_SYSLOG, "Unable to Handle GMCP Char.Vitals.Get Call: No Valid Character");
    else
      SendGMCPCharVitals(apDescriptor->character);
  else if (!strncmp(module, "Char.Info.Get", strlen(module)))
    if (!apDescriptor->character)
      mudlog_vfprintf(apDescriptor->character, LOG_SYSLOG, "Unable to Handle GMCP Char.Info.Get Call: No Valid Character");
    else
      SendGMCPCharInfo(apDescriptor->character);
  else if (!strncmp(module, "Char.Pools.Get", strlen(module)))
    if (!apDescriptor->character)
      mudlog_vfprintf(apDescriptor->character, LOG_SYSLOG, "Unable to Handle GMCP Char.Pools.Get Call: No Valid Character");
    else
      SendGMCPCharPools(apDescriptor->character);
#ifdef USE_DISCORD_RICH_PRESENCE
  else if (!strncmp(module, "External.Discord.Hello", strlen(module))) {
    RegisterGMCPDiscordHello(apDescriptor, payload);
    SendGMCPDiscordInfo(apDescriptor);
    SendGMCPDiscordStatus(apDescriptor);
  }
  else if (!strncmp(module, "External.Discord.Get", strlen(module))) {
    SendGMCPDiscordStatus(apDescriptor);
  }
#endif
#ifdef IS_BUILDPORT
  else
    log_vfprintf("Received Unhandled GMCP Module Call [%s]: '%s' from %s.", module, payload.dump().c_str(), GET_CHAR_NAME(apDescriptor->character));
#endif
}

void ParseGMCP( descriptor_t *apDescriptor, const char *apData )
{
  // GMCP messages are typically of the form:
  //   Module.Name {"key": "value", ...}
  //
  // First, find the first whitespace which separates the module name from the JSON payload.
  const char *space = strchr(apData, ' ');
  if (!space) {
      // If no space is found, treat the entire message as a module name with an empty payload.
      ExecuteGMCPMessage(apDescriptor, apData, NULL);
      return;
  }

  // Extract the module name.
  size_t module_len = space - apData;
  char module[MAX_GMCP_SIZE];
  if (module_len >= MAX_GMCP_SIZE) {
      module_len = MAX_GMCP_SIZE - 1;
  }
  memcpy(module, apData, module_len);
  module[module_len] = '\0';

  // Skip any additional whitespace to locate the start of the JSON payload.
  while (*space && isspace(*space)) {
      space++;
  }

  // Use nlohmann::json to parse the JSON payload.
  json payload;
  try {
    payload = json::parse(space);
  } catch (const json::parse_error &ex) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "There was a JSON error when attempting to parse GMCP message '%s' from %s.", module, GET_CHAR_NAME(apDescriptor->character));
    payload = NULL;
  }

  // Dispatch the GMCP message.
  ExecuteGMCPMessage(apDescriptor, module, payload);
}

/////// Helper functions.

const char *generate_discord_quest_string(idnum_t quest_rnum) {
  static char result[100] = {0};

  strlcpy(result, "Running the Shadows", sizeof(result));

  if (quest_rnum <= 0) {
    return result;
  }

  switch (quest_table[quest_rnum].vnum) {
    // TODO: If it's a cool quest, put a special string for it.
    default:
      break;
  }

  // Get the Johnson for the quest.
  rnum_t johnson_rnum = real_mobile(quest_table[quest_rnum].johnson);
  if (johnson_rnum <= 0) {
    return result;
  }

  snprintf(result, sizeof(result), "Working for %s", GET_CHAR_NAME(&mob_proto[johnson_rnum]));
  return result;
}

// Generates the second of three lines under "Playing AwakeMUD CE"
#define SET_DETAILS(msg) strlcpy(details, msg, DISCORD_BUFFER_SIZE);
#define SET_STATE(msg) strlcpy(state, msg, DISCORD_BUFFER_SIZE);
#define SET_SMALLIMAGE(msg) strlcpy(smallimage, msg, DISCORD_BUFFER_SIZE);
#define SET_SMALLIMAGETEXT(msg) strlcpy(smallimagetext, msg, DISCORD_BUFFER_SIZE);
void generate_discord_details(descriptor_t *apDescriptor, char *smallimage, char *smallimagetext, char *details, char *state) {
  struct char_data *ch = apDescriptor->character;

  // They haven't connected yet.
  if (!ch) {
    // No icon etc.
    SET_DETAILS("In Menus");
    return;
  }

  // If you've gotten here, they're connected. Choose a details string to return.
  // First, the editing state image text switch.
  switch (STATE(apDescriptor)) {
    case CON_VEDIT:
      SET_SMALLIMAGETEXT("Building Vehicles");
      break;
    case CON_IEDIT:
      SET_SMALLIMAGETEXT("Forming Objects");
      break;
    case CON_REDIT:
      SET_SMALLIMAGETEXT("Painting the Game World");
      break;
    case CON_MEDIT:
      SET_SMALLIMAGETEXT("Molding Mobs");
      break;
    case CON_QEDIT:
      SET_SMALLIMAGETEXT("Instructing Johnsons");
      break;
    case CON_SHEDIT:
      SET_SMALLIMAGETEXT("Stocking Shops");
      break;
    case CON_ZEDIT:
      SET_SMALLIMAGETEXT("Customizing Zones");
      break;
    case CON_HEDIT:
      SET_SMALLIMAGETEXT("Coding Matrix Hosts");
      break;
    case CON_ICEDIT:
      SET_SMALLIMAGETEXT("Writing ICs");
      break;
    case CON_HELPEDIT:
      SET_SMALLIMAGETEXT("Scribing Helpfiles");
      break;
    case CON_HOUSEEDIT_COMPLEX:
      SET_SMALLIMAGETEXT("Building Apartment Complex");
      break;
    case CON_HOUSEEDIT_APARTMENT:
      SET_SMALLIMAGETEXT("Renovating Apartments");
      break;
    case CON_FACTION_EDIT:
      SET_SMALLIMAGETEXT("Negotiating with Factions");
      break;
  }

  //
  switch (STATE(apDescriptor)) {
    case CON_PART_CREATE:
    case CON_DECK_CREATE:
    case CON_PRO_CREATE:
      SET_SMALLIMAGE("circuitry-darker");
      SET_SMALLIMAGETEXT("Hack the Planet!");
      SET_DETAILS("Building a Cyberdeck");
      return;
    case CON_SPELL_CREATE:
      SET_SMALLIMAGE("book-aura-darker");
      SET_SMALLIMAGETEXT("'I cast... MAGIC MISSILE!'");
      if (GET_TRADITION(ch) == TRAD_SHAMANIC) {
        char totem_string[100];
        snprintf(totem_string, sizeof(totem_string), "Communing with %s", totem_types[GET_TOTEM(ch)]);
        SET_DETAILS(totem_string);
      } else {
        SET_DETAILS("Designing Spells");
      }
      return;
    case CON_VEDIT:
    case CON_IEDIT:
    case CON_REDIT:
    case CON_MEDIT:
    case CON_QEDIT:
    case CON_SHEDIT:
    case CON_ZEDIT:
    case CON_HEDIT:
    case CON_ICEDIT:
    case CON_HELPEDIT:
    case CON_HOUSEEDIT_COMPLEX:
    case CON_HOUSEEDIT_APARTMENT:
    case CON_FACTION_EDIT:
      // smallimagetext set above
      SET_SMALLIMAGE("spanner");
      SET_DETAILS("Contributing <3");
      return;
    case CON_DECORATE:
    case CON_DECORATE_VEH:
      SET_DETAILS("Decorating");
      return;
    
    // These cases don't return and instead just break. This lets you override them with later logic.
    case CON_PLAYING:
      SET_DETAILS("Vibing");
      break;
    default:
      SET_DETAILS("In Menus");
      break;
  }
  // Playing 3x3 Control Point
  
  struct room_data *in_room = get_ch_in_room(ch);

  // Add in some other action states.
  if (PLR_FLAGGED(ch, PLR_REMOTE) || AFF_FLAGGED(ch, AFF_RIG)) {
    SET_SMALLIMAGE("steering-wheel-darker");
    SET_SMALLIMAGETEXT("Jacked In");
    SET_DETAILS("Direct Neural Driving");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_PILOT)) {
    SET_SMALLIMAGE("steering-wheel-darker");
    SET_SMALLIMAGETEXT("Behind the Wheel");
    SET_DETAILS("Cruising the Sprawl");
    return;
  }

  if (GET_ROOM_VNUM(in_room) == RM_AIRBORNE) {
    SET_SMALLIMAGE("airplane-darker");
    SET_SMALLIMAGETEXT("'There are free peanuts, right?'");
    SET_DETAILS("Flying the Unfriendly Skies");
    return;
  }

  if (IS_WATER(in_room)) {
    if (ch->in_veh) {
      SET_SMALLIMAGE("sinking-ship");
      SET_SMALLIMAGETEXT("Hopefully not like this.");
      SET_DETAILS("Out for a Sail");
    } else {
      SET_SMALLIMAGE("swimfins");
      SET_SMALLIMAGETEXT("Swim hard, avoid the RIP-tide!");
      SET_DETAILS("Fighting the Current");
    }
    return;
  }

  if (PLR_FLAGGED(ch, PLR_MATRIX)) {
    SET_SMALLIMAGE("cpu-shot-darker");
    SET_SMALLIMAGETEXT("Jacked In");
    SET_DETAILS("Hacking the Matrix");
    return;
  }

  // Being on a job is probably the most common state, so we put it last.
  if (GET_QUEST(ch)) {
    SET_SMALLIMAGE("briefcase-darker");
    SET_SMALLIMAGETEXT("");
    SET_DETAILS(generate_discord_quest_string(GET_QUEST(ch)));
    return;
  }
}
#undef SET_DETAILS
#undef SET_STATE
#undef SET_SI
#undef SET_SIT

void update_gmcp_discord_info(struct descriptor_data *desc) {
#ifdef USE_DISCORD_RICH_PRESENCE
  if (desc && desc->pProtocol && desc->pProtocol->bGMCP) {
    SendGMCPDiscordStatus(desc);
  }
#endif
}