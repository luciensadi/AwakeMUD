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

using nlohmann::json;

static void Write( descriptor_t *apDescriptor, const char *apData )
{
  if ( apDescriptor != NULL ) {
   if ( apDescriptor->pProtocol->WriteOOB > 0 ) {
    apDescriptor->pProtocol->WriteOOB = 2;
   }
   // Append our output to the player's output buffer.
   write_to_output( apData, apDescriptor );

   // Writing directly to the descriptor can break control sequences mid-stream, causing display problems.
   // write_to_descriptor( apDescriptor->descriptor, apData );
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
  j["Char"].push_back("Status");
  j["Char"].push_back("Info");
  j["Char"].push_back("Pools");

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(apDescriptor, "Core.Supports", payload.c_str());
}

void SendGMCPCharInfo( struct char_data * ch )
{
  if (!ch || !ch->desc || !ch->desc->pProtocol->bGMCP) return;
  json j;
  j["name"] = GET_CHAR_NAME(ch);
  j["height"] = ((float)GET_HEIGHT(ch) / 100);
  j["weight"] = GET_WEIGHT(ch);
  j["karma"] = GET_KARMA(ch);
  j["tke"] = GET_TKE(ch);
  j["rep"] = GET_REP(ch);
  j["noto"] = GET_NOT(ch);
  j["metatype"] = GET_RACE(ch);
  j["pronouns"] = ch->player.pronouns;

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
  j["zone_number"] = room->zone;
  j["exits"] = SerializeRoomExits(room);
  // Only add coordinates if they are valid.
  if (room->x && room->y && room->z) {
    j["coords"] = { {"x", room->x}, {"y", room->y}, {"z", room->z} };
  }

  // Add room description if it exists; any quotes will be automatically escaped.
  if (*room->description) {
    j["description"] = room->description;
  }

  if (room->latitude) j["latitude"] = room->latitude;
  if (room->longitude) j["longitude"] = room->longitude;
  
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

void SendGMCPCharStatus( struct char_data * ch )
{
  if (!ch || !ch->desc || !ch->desc->pProtocol->bGMCP) return;
  json j;

  j["physical"] = GET_PHYSICAL(ch);
  j["mental"] = GET_MENTAL(ch);
  j["physical_max"] = GET_MAX_PHYSICAL(ch);
  j["mental_max"] = GET_MAX_MENTAL(ch);

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(ch->desc, "Char.Status", payload.c_str());
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
      mudlog_vfprintf(NULL, LOG_SYSLOG, "Unable to Handle GMCP Room.Info.Get Call: No Valid Character");
    else if (!apDescriptor->character->in_room)
      mudlog_vfprintf(NULL, LOG_SYSLOG, "Unable to Handle GMCP Room.Info.Get Call: Not in Room");
    else
      SendGMCPRoomInfo(apDescriptor->character, apDescriptor->character->in_room);
  else if (!strncmp(module, "Room.Exits.Get", strlen(module)))
    if (!apDescriptor->character)
      mudlog_vfprintf(NULL, LOG_SYSLOG, "Unable to Handle GMCP Room.Exits.Get Call: No Valid Character");
    else
      SendGMCPExitsInfo(apDescriptor->character);
  else if (!strncmp(module, "Char.Status.Get", strlen(module)))
    if (!apDescriptor->character)
      mudlog_vfprintf(NULL, LOG_SYSLOG, "Unable to Handle GMCP Char.Status.Get Call: No Valid Character");
    else
      SendGMCPCharStatus(apDescriptor->character);
  else if (!strncmp(module, "Char.Info.Get", strlen(module)))
    if (!apDescriptor->character)
      mudlog_vfprintf(NULL, LOG_SYSLOG, "Unable to Handle GMCP Char.Info.Get Call: No Valid Character");
    else
      SendGMCPCharInfo(apDescriptor->character);
  else if (!strncmp(module, "Char.Pools.Get", strlen(module)))
    if (!apDescriptor->character)
      mudlog_vfprintf(NULL, LOG_SYSLOG, "Unable to Handle GMCP Char.Pools.Get Call: No Valid Character");
    else
      SendGMCPCharPools(apDescriptor->character);
  else
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Received Unhandled GMCP Module Call [%s]: %s", module, payload.dump().c_str());
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