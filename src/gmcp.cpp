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

void ExecuteGMCPMessage(descriptor_t *apDescriptor, const char *module, const json &payload) {
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

void SendGMCPExitsInfo( struct char_data *ch ) 
{
  if (!ch || !ch->desc || !ch->desc->pProtocol->bGMCP) return;
  struct veh_data *veh = NULL;

  json j;
  j["exits"] = json::array();

  for (int door = 0; door < NUM_OF_DIRS; door++)
    if (EXIT(ch, door) && EXIT(ch, door)->to_room && EXIT(ch, door)->to_room != &world[0]) {
      if (ch->in_veh || ch->char_specials.rigging) {
        RIG_VEH(ch, veh);
        if (!ROOM_FLAGGED(EXIT(veh, door)->to_room, ROOM_ROAD) &&
            !ROOM_FLAGGED(EXIT(veh, door)->to_room, ROOM_GARAGE) &&
            !IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN))
          j["exits"].push_back({"direction", exitdirs[door], "state", "INACCESSIBLE"});
        else if (!IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED | EX_HIDDEN))
          j["exits"].push_back({"direction", exitdirs[door], "state", "OPEN"});
      } else {
        if (!IS_SET(EXIT(ch, door)->exit_info, EX_HIDDEN) || GET_LEVEL(ch) > LVL_MORTAL) {
          if (IS_SET(EXIT(ch, door)->exit_info, EX_LOCKED))
            j["exits"].push_back({"direction", exitdirs[door], "state", "LOCKED"});
          else if (IS_SET(EXIT(ch, door)->exit_info, EX_CLOSED))
            j["exits"].push_back({"direction", exitdirs[door], "state", "CLOSED"});
          else
            j["exits"].push_back({"direction", exitdirs[door], "state", "OPEN"});
        }
      }
    }

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(ch->desc, "Room.Exits", payload.c_str());
}

void SendGMCPRoomInfo( struct char_data *ch, struct room_data *room ) 
{
  if (!ch || !ch->desc || !ch->desc->pProtocol->bGMCP) return;
  json j;

  j["room_vnum"] = GET_ROOM_VNUM(room);
  j["room_name"] = GET_ROOM_NAME(room);
  j["zone_number"] = room->zone;
  // Only add coordinates if they are valid.
  if (room->x && room->y && room->z) {
    j["coords"] = { {"x", room->x}, {"y", room->y}, {"z", room->z} };
  }

  // Add room description if it exists; any quotes will be automatically escaped.
  if (*room->description) {
    j["room_description"] = room->description;
  }

  if (room->latitude) j["latitude"] = room->latitude;
  if (room->longitude) j["longitude"] = room->longitude;
  
  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(ch->desc, "Room.Info", payload.c_str());

  SendGMCPExitsInfo(ch);
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
  j["pools"] = {"body", GET_BODY_POOL(ch), "magic", GET_MAGIC_POOL(ch), "combat", GET_COMBAT_POOL(ch), "hacking", GET_HACKING(ch)};
  j["task_pools"] = json::object();
  for (int x = 0; x < 7; x++)
    j["task_pools"][attributes[x]] = GET_TASK_POOL(ch, x);  

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(ch->desc, "Char.Status", payload.c_str());
}

void SendGMCPCharPools( struct char_data * ch )
{
  if (!ch || !ch->desc || !ch->desc->pProtocol->bGMCP) return;
  json j;

  j["pools"] = {"body", GET_BODY_POOL(ch), "magic", GET_MAGIC_POOL(ch), "combat", GET_COMBAT_POOL(ch), "hacking", GET_HACKING(ch)};
  j["task_pools"] = json::object();
  for (int x = 0; x < 7; x++)
    j["task_pools"][attributes[x]] = GET_TASK_POOL(ch, x);  

  // Dump the json to a string and send it.
  std::string payload = j.dump();
  SendGMCP(ch->desc, "Char.Pools", payload.c_str());
}