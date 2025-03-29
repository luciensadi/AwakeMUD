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

using nlohmann::json;

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

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(apDescriptor, "Core.Supports", payload.c_str());
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

  j["physical"] = GET_PHYSICAL(ch);
  j["mental"] = GET_MENTAL(ch);
  j["physical_max"] = GET_MAX_PHYSICAL(ch);
  j["mental_max"] = GET_MAX_MENTAL(ch);
  j["karma"] = GET_KARMA(ch);
  j["nuyen"] = GET_NUYEN(ch);
  j["carrying_weight"] = IS_CARRYING_W(ch);
  j["max_carry_weight"] = CAN_CARRY_W(ch);
  j["armor"] = json::object();
  j["armor"]["ballistic"] = GET_BALLISTIC(ch);
  j["armor"]["impact"] = GET_IMPACT(ch);
  if (get_ch_domain_str(ch, TRUE)) 
    j["domain"] = get_ch_domain_str(ch, TRUE);

  // Current Ammo Handler
  if (GET_EQ(ch, WEAR_WIELD) &&
      IS_GUN(GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3)))
    if (GET_EQ(ch, WEAR_WIELD)->contains) {
      j["ammo"] = MIN(GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 5), GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD)->contains, 9));
    } else
      j["ammo"] = 0;
    else
      j["ammo"] = 0;

  // Max ammo handler
  if (GET_EQ(ch, WEAR_WIELD) &&
      IS_GUN(GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3)))
    j["max_ammo"] =  GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 5);
  else
    j["max_ammo"] = 0;

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
  if (!strncmp(module, "Core.Supports.Get", strlen(module)))
    SendGMCPCoreSupports(apDescriptor);
  else if (!strncmp(module, "Room.Info.Get", strlen(module)))
    if (!apDescriptor->character)
      mudlog_vfprintf(apDescriptor->character, LOG_SYSLOG, "Unable to Handle GMCP Room.Info.Get Call: No Valid Character");
    else if (!apDescriptor->character->in_room)
      mudlog_vfprintf(apDescriptor->character, LOG_SYSLOG, "Unable to Handle GMCP Room.Info.Get Call: Not in Room");
    else
      SendGMCPRoomInfo(apDescriptor->character, apDescriptor->character->in_room);
  else if (!strncmp(module, "Room.Exits.Get", strlen(module)))
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
  else
    mudlog_vfprintf(apDescriptor->character, LOG_SYSLOG, "Received Unhandled GMCP Module Call [%s]: %s", module, payload.dump().c_str());
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
    mudlog_vfprintf(NULL, LOG_SYSLOG, "There was a JSON error when attempted to parse %s.", module);
    payload = NULL;
  }

  // Dispatch the GMCP message.
  ExecuteGMCPMessage(apDescriptor, module, payload);
}