/* **********************************************************************
*  file: transport.cc                                                   *
*  author: Andrew Hynek                                                 *
*  (original monorail code, now deleted, written by Chris Dickey)       *
*  purpose: contains all routines for time- and command-based transport *
*  Copyright (c) 1998 by Andrew Hynek                                   *
*  (c) 2001 The AwakeMUD Consortium                                     *
********************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <vector>

#if defined(WIN32) && !defined(__CYGWIN__)
#define strcasecmp(x, y) _stricmp(x,y)
#else
#endif

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "transport.hpp"
#include "utils.hpp"
#include "constants.hpp"
#include "config.hpp"

SPECIAL(call_elevator);
SPECIAL(elevator_spec);
extern int calculate_distance_between_rooms(vnum_t start_room_vnum, vnum_t target_room_vnum, bool ignore_roads);
extern void perform_fall(struct char_data *ch);

extern int find_first_step(vnum_t src, vnum_t target, bool ignore_roads);

ACMD_DECLARE(do_echo);
struct dest_data *get_dest_data_list_for_zone(int zone_num);

bool cab_jurisdiction_matches_destination(vnum_t cab_vnum, vnum_t dest_vnum);

// ----------------------------------------------------------------------------

// ______________________________
//
// static vars
// ______________________________

struct elevator_data *elevator = NULL;
int num_elevators = 0;

extern int ELEVATOR_SHAFT_FALL_RATING;

// static const int NUM_SEATTLE_STATIONS = 6;
static const int NUM_SEATAC_STATIONS = 6;

struct dest_data seattle_taxi_destinations[] =
{
  { "action", "electronics", "Action Computers", 32650, TAXI_DEST_TYPE_SHOPPING, TRUE },
  { "microdeck", "", "Auburn Microdeck Outlet", 29010, TAXI_DEST_TYPE_SHOPPING, TRUE },
  { "aztech", "pyramid",  "Aztechnology Pyramid", 30539, TAXI_DEST_TYPE_CORPORATE_PARK , TRUE },
  { "auburn", "", "Auburn", 29011, TAXI_DEST_TYPE_AREA_OF_TOWN , TRUE },
  { "beacon", "", "Beacon Mall Everett", 39253, TAXI_DEST_TYPE_SHOPPING, TRUE },
  { "bicson", "", "Bicson Biomedical", 39285, TAXI_DEST_TYPE_HOSPITALS, TRUE },
  { "biohyde", "", "Biohyde Complex", 4201, TAXI_DEST_TYPE_CORPORATE_PARK , TRUE },
  { "puyallup", "", "Central Puyallup", 2457, TAXI_DEST_TYPE_AREA_OF_TOWN , TRUE },
  { "chinatown", "", "Chinatown", 2527, TAXI_DEST_TYPE_AREA_OF_TOWN, TRUE },
  { "penumbra", "", "Club Penumbra", 32587, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "council", "island", "Council Island", 9149, TAXI_DEST_TYPE_AREA_OF_TOWN , TRUE },
  { "hospital", "", "Council Island Hospital", 9161, TAXI_DEST_TYPE_HOSPITALS, TRUE },
  { "dante", "dante's", "Dante's Inferno", 32661, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "diver", "easy", "Easy Diver Coffin Hotel", 32644, TAXI_DEST_TYPE_ACCOMMODATIONS, TRUE },
  { "evergreen", "", "Evergreen Complex", 2201, TAXI_DEST_TYPE_ACCOMMODATIONS, TRUE },
  { "everett", "", "Everett", 39263, TAXI_DEST_TYPE_AREA_OF_TOWN , TRUE },
  { "epicenter", "", "The Epicenter", 2418, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "hellhound", "bus", "Hellhound Bus Stop",  32501, TAXI_DEST_TYPE_TRANSPORTATION, TRUE },
  { "homewood", "suites", "Homewood Suites", 30512, TAXI_DEST_TYPE_ACCOMMODATIONS, TRUE },
  { "humana", "", "Humana Hospital", 2536, TAXI_DEST_TYPE_HOSPITALS, TRUE },
  { "errant", "", "KE Training Facility", 30127, TAXI_DEST_TYPE_CORPORATE_PARK , TRUE},
  { "knight", "center", "Knight Center", 1519, TAXI_DEST_TYPE_CORPORATE_PARK, TRUE },
  { "kristin's", "kristin", "Kristin's", 30578, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "charne", "", "La Charne", 30548, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, FALSE },
  { "cinema", "vieux", "Le Cinema Vieux", 32588, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE},
  { "chiba", "", "Little Chiba", 32686, TAXI_DEST_TYPE_SHOPPING, TRUE },
  { "lysleul", "plaza", "Lysleul Plaza", 30551, TAXI_DEST_TYPE_SHOPPING, TRUE },
  { "platinum", "", "Platinum Club", 32685, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "pooks", "pook's", "Pook's Place", 1530, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "apartment", "", "Redmond Apartment Block", 14337, TAXI_DEST_TYPE_ACCOMMODATIONS, TRUE },
  { "marksman", "firing", "Marksman Indoor Firing Range", 32682, TAXI_DEST_TYPE_OTHER, TRUE },
  { "matchsticks", "", "Matchsticks", 30579, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "mitsuhama", "", "Mitsuhama Towers", 32722, TAXI_DEST_TYPE_CORPORATE_PARK, TRUE },
  { "modern", "magik", "Modern Magik", 30514, TAXI_DEST_TYPE_SHOPPING, TRUE },
  { "moonlight", "", "Moonlight Mall", 14373, TAXI_DEST_TYPE_SHOPPING, TRUE },
  { "novatech", "", "Novatech Seattle", 32541, TAXI_DEST_TYPE_CORPORATE_PARK, TRUE },
  { "nybbles", "bytes", "Nybbles and Bytes", 2057, TAXI_DEST_TYPE_SHOPPING, TRUE },
  { "quinns", "quinn's", "Quinn's", 32521, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "racespec", "", "Racespec Performance", 30573, TAXI_DEST_TYPE_SHOPPING, TRUE },
  { "redmond", "", "Redmond", 14310, TAXI_DEST_TYPE_AREA_OF_TOWN , TRUE },
  { "sculleys", "", "Sculleys", 30591, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "seattle", "", "Seattle", 32679, TAXI_DEST_TYPE_AREA_OF_TOWN, TRUE },
  { "docks", "dockyards", "Seattle Dockyards", 32500, TAXI_DEST_TYPE_AREA_OF_TOWN , TRUE },
  { "methodist", "church", "Seattle First Methodist", 30565, TAXI_DEST_TYPE_OTHER, TRUE },
  { "formal", "", "Seattle Formal Wear", 32746, TAXI_DEST_TYPE_SHOPPING, TRUE },
  { "zoo", "", "Seattle Municipal Zoo", 32569, TAXI_DEST_TYPE_OTHER, TRUE },
  { "garage", "parking", "Seattle Parking Garage", 32720, TAXI_DEST_TYPE_OTHER , TRUE },
  { "park", "", "Seattle State Park", 4000, TAXI_DEST_TYPE_AREA_OF_TOWN , TRUE },
  { "library", "", "Seattle Public Library", 30600, TAXI_DEST_TYPE_OTHER, TRUE },
  { "airport", "seatac", "Seattle-Tacoma Airport", 19410, TAXI_DEST_TYPE_AREA_OF_TOWN , TRUE },
  { "shintaru", "", "Shintaru", 32513, TAXI_DEST_TYPE_CORPORATE_PARK , TRUE },
  { "smiths", "pub", "Smith's Pub", 32566, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "syberspace", "", "SyberSpace", 30612, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "tarislar", "", "Tarislar", 4965, TAXI_DEST_TYPE_AREA_OF_TOWN , TRUE },
  { "tacoma", "", "Tacoma", 2000, TAXI_DEST_TYPE_AREA_OF_TOWN, TRUE },
  { "coffin", "motel", "Tacoma Coffin Motel", 2045, TAXI_DEST_TYPE_ACCOMMODATIONS, TRUE },
  { "mall", "", "Tacoma Mall", 2093, TAXI_DEST_TYPE_SHOPPING, TRUE },
  { "TWE", "", "Tacoma Weapons Emporium", 2514, TAXI_DEST_TYPE_SHOPPING, TRUE },
  { "afterlife", "", "The Afterlife", 2072, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "rhino", "big", "The Big Rhino", 32635, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "wyvern", "blue", "The Blue Wyvern", 4909, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "italiano", "", "The Italiano", 30149, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "muscovite", "lounge", "The Muscovite Lounge", 30548, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS , FALSE },
  { "neophyte", "guild",  "The Neophyte Guild", 32679, TAXI_DEST_TYPE_OTHER, TRUE },
  { "skeleton", "", "The Skeleton", 25308, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "sapphire", "star", "The Star Sapphire", 70301, TAXI_DEST_TYPE_ACCOMMODATIONS, TRUE },
  { "stop", "gap", "The Stop Gap", 32708, TAXI_DEST_TYPE_SHOPPING, TRUE },
  { "junkyard", "",  "The Tacoma Junkyard", 2070, TAXI_DEST_TYPE_AREA_OF_TOWN, TRUE },
  { "reaper", "", "The Reaper", 32517, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
  { "touristville", "", "Touristville", 25313, TAXI_DEST_TYPE_AREA_OF_TOWN , TRUE },
  { "bank", "ucasbank", "UCASBank", 30524, TAXI_DEST_TYPE_OTHER, TRUE },
  { "yoshi", "sushi", "Yoshi's Sushi Bar", 32751, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
#ifdef USE_PRIVATE_CE_WORLD
    { "slitch", "pit", "The Slitch Pit", 32660, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
    { "planetary", "", "Planetary Corporation", 72503, TAXI_DEST_TYPE_CORPORATE_PARK, FALSE },
    { "splat", "paintball", "The SPLAT! Paintball Arena", 32653, TAXI_DEST_TYPE_OTHER, TRUE },
    { "nerp", "", "The NERPcorpolis", 6901, TAXI_DEST_TYPE_OOC, TRUE },
    { "glenn", "quiet", "Quiet Glenn Apartments", 32713, TAXI_DEST_TYPE_ACCOMMODATIONS, TRUE },
    { "triple", "inn", "Triple Tree Inn", 12401, TAXI_DEST_TYPE_ACCOMMODATIONS, TRUE },
    { "rokhalla", "hooligans", "Rokhalla", 32759, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
    { "impulse", "", "Impulse Garage Complex", 25310, TAXI_DEST_TYPE_ACCOMMODATIONS, TRUE },
#endif
    { "\n", "", "", 0, 0, 0 } // this MUST be last
  };

struct dest_data portland_taxi_destinations[] =
  {
    { "hellhound", "bus", "Hellhound Bus Station", 14624, TAXI_DEST_TYPE_TRANSPORTATION, TRUE  },
    { "postal", "", "Royal Postal Station", 14629, TAXI_DEST_TYPE_OTHER, TRUE },
    { "trans-Oregon", "trucking", "Trans-Oregon Trucking", 14604, TAXI_DEST_TYPE_OTHER, TRUE },
    { "thompson", "autogroup", "Thompson Autogroup",  14602, TAXI_DEST_TYPE_SHOPPING, TRUE},
    { "hard", "", "Hard Times", 14608, TAXI_DEST_TYPE_SHOPPING, TRUE},
    { "bank", "", "TT-Bank", 14610, TAXI_DEST_TYPE_OTHER, TRUE},
    { "sixth", "garage", "Sixth Street Parking", 14680, TAXI_DEST_TYPE_AREA_OF_TOWN, TRUE},
    { "satyricon", "", "The Satyricon", 14611, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
    { "rocco", "pizza", "Rocco's Pizza", 14730, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
    { "annabel", "annabel's", "Annabel's Antiquities", 2710, TAXI_DEST_TYPE_SHOPPING, TRUE },
    { "square", "", "O'Bryant Square", 2708, TAXI_DEST_TYPE_AREA_OF_TOWN, TRUE },
    { "parking", "", "Portland City Parking", 2703, TAXI_DEST_TYPE_AREA_OF_TOWN, TRUE},
    { "mary", "bar", "Mary's Bar", 14712, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
    { "tower", "ttbank", "TTBank Tower", 14743, TAXI_DEST_TYPE_OTHER, TRUE },
    { "city", "center", "City Center Parking", 14751, TAXI_DEST_TYPE_AREA_OF_TOWN, TRUE},
    { "habitat", "telestrian", "Telestrian Habitat", 18800, TAXI_DEST_TYPE_SHOPPING, TRUE },
    { "hospital", "royal", "Royal Hospital", 14707, TAXI_DEST_TYPE_HOSPITALS, TRUE },
    { "toadstool", "", "The Toadstool", 14671, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE },
    { "powell's", "powell", "Powell's Technical Books", 14724, TAXI_DEST_TYPE_SHOPPING, TRUE },
    { "davis", "offices", "Davis Street Offices", 14667, TAXI_DEST_TYPE_CORPORATE_PARK, TRUE},
    { "max", "station", "Downtown MAX Station", 20800, TAXI_DEST_TYPE_TRANSPORTATION, TRUE  },
    { "\n", "", "", 0, 0, 0 } // this MUST be last

  };

struct dest_data caribbean_taxi_destinations[] =
  {
    { "airport", "", "St. Patricks International Airport", 62117, TAXI_DEST_TYPE_TRANSPORTATION, TRUE},
    { "pelagie", "apartments", "Pelagie Waterfront Apartments", 62125, TAXI_DEST_TYPE_ACCOMMODATIONS, TRUE},
    { "montplaisir", "restaurant", "Montplaisir Restaurant", 62151, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE},
    { "viltz", "motel", "Viltz Motel",  62135, TAXI_DEST_TYPE_ACCOMMODATIONS, TRUE},
    { "church", "sarasses", "Church of Saint Sarasses", 62157, TAXI_DEST_TYPE_OTHER, TRUE},
    { "cienfuegos", "cantina", "Cienfuegos Cantina", 62146, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE},
    { "prospect", "union", "Prospect & Union", 62207, TAXI_DEST_TYPE_AREA_OF_TOWN, TRUE},
    { "docwagon", "", "Victoria DocWagon", 62249, TAXI_DEST_TYPE_HOSPITALS, TRUE},
    { "garage", "parking", "Victoria City Parking Garage", 62247, TAXI_DEST_TYPE_OTHER, TRUE},
    { "mchughs", "food", "McHughs Fast Food", 62272, TAXI_DEST_TYPE_RESTAURANTS_AND_NIGHTCLUBS, TRUE},
    { "casino", "grandmaion", "Grandmaion Casino", 62275, TAXI_DEST_TYPE_OTHER, TRUE},
    { "police", "station", "Victoria Police Station", 62264, TAXI_DEST_TYPE_OTHER, TRUE},
    { "\n", "", "", 0, 0, 0 } // this MUST be last
  };

struct taxi_dest_type taxi_dest_type_info[] = {
  { "^yAreas of Town", "^Y" },
  { "^cCorporate Parks", "^C" },
  { "^gTourist Attractions", "^G" },
  { "^rTransportation", "^R" },
  { "^mRestaurants and Nightclubs", "^M" },
  { "^bShopping", "^B" },
  { "^gAccommodation", "^G" },
  { "^rHospitals", "^R" },
  { "^WEverything Else", "^W" },
  { "^MOOC Areas", "^W"},
};

struct transport_type
{
  vnum_t transport;
  dir_t to;
  vnum_t room;
  dir_t from;
};

static struct transport_type seatac[NUM_SEATAC_STATIONS] =
  {
    { 501, SOUTH, 3018, NORTH },
    { 501, EAST, 20235, WEST },
    { 501, EAST, 29036, WEST },
    { 501, EAST, 9823, WEST },
    { 501, EAST, 29036, WEST },
    { 501, EAST, 20235, WEST },
  };
/*
static struct transport_type seattle[NUM_SEATTLE_STATION] =
  {
    { 502, SOUTH, 9824, NORTH },
  }
*/
// ______________________________
//
// func prototypes
// ______________________________

SPECIAL(taxi);
int process_elevator(struct room_data *room,
                     struct char_data *ch,
                     int cmd,
                     char *argument);

// ____________________________________________________________________________
//
// Taxi
// ____________________________________________________________________________

// Taxi sign: If you look at it, it prints a dynamic description instead of the hardcoded one.
SPECIAL(taxi_sign) {
  struct obj_data *obj = (struct obj_data *) me;

  struct obj_data *tmp_object = NULL;
  struct char_data *tmp_char = NULL;

  struct dest_data *dest_data_list = NULL;

  // No argument given, no character available, no problem. Skip it.
  if (!*argument || !ch)
    return FALSE;

  // You in a vehicle? I don't know how you got this sign, but skip it.
  if (ch->in_veh)
    return FALSE;

  // If it's not a command that would reveal this sign's data, skip it.
  if (!(CMD_IS("look") || CMD_IS("examine") || CMD_IS("read")))
    return FALSE;

  // Search the room and find something that matches me.
  generic_find(argument, FIND_OBJ_ROOM, ch, &tmp_char, &tmp_object);

  // Senpai didn't notice me? Skip it, he's not worth it.
  if (!tmp_object || tmp_object != obj)
    return FALSE;

  // Select our destination list based on our vnum.
  switch (GET_OBJ_VNUM(obj)) {
    case OBJ_SEATTLE_TAXI_SIGN:
      dest_data_list = seattle_taxi_destinations;
      break;
    case OBJ_PORTLAND_TAXI_SIGN:
      dest_data_list = portland_taxi_destinations;
      break;
    case OBJ_CARIBBEAN_TAXI_SIGN:
      dest_data_list = caribbean_taxi_destinations;
      break;
    default:
      snprintf(buf, sizeof(buf), "Warning: taxi_sign spec given to object %s (%ld) that had no matching definition in transport.cpp!",
              GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj));
      mudlog(buf, ch, LOG_SYSLOG, TRUE);
      return FALSE;
  }

  // Set up our default string.
  strncpy(buf, "The keyword for each location is listed after the location name.  ^WSAY^n the keyword to the driver, and for a small fee, he will drive you to your destination.\r\nYou can also ^WSAY^n a set of GridGuide coordinates, like ^WSAY -65322, 32761^n.\r\n", sizeof(buf) - 1);
  if (!PRF_FLAGGED(ch, PRF_SCREENREADER))
    strlcat(buf, "-------------------------------------------------\r\n", sizeof(buf));

  bool is_first_printed_dest_title = TRUE;

  // Print that sweet, sweet destination-flavored goodness.
  for (unsigned int taxi_dest_type = 0; taxi_dest_type < NUM_TAXI_DEST_TYPES; taxi_dest_type++) {
    // Scan the list once to see if we have any of this dest type in the system.
    bool has_this_dest_type = FALSE;
    for (int dest_index = 0; *dest_data_list[dest_index].keyword != '\n'; dest_index++) {
      if (DEST_IS_VALID(dest_index, dest_data_list) && dest_data_list[dest_index].type == taxi_dest_type) {
        has_this_dest_type = TRUE;
        break;
      }
    }

    // If we don't have anything from this dest type, continue.
    if (!has_this_dest_type)
      continue;

    // We have something! Print the dest type's title to make the list ~~fancy~~. Extra carriage return before if we're not the first.
    if (PRF_FLAGGED(ch, PRF_SCREENREADER))
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%sSection: %s^n:\r\n", is_first_printed_dest_title ? "" : "\r\n", taxi_dest_type_info[taxi_dest_type].title_string);
    else
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%s^n\r\n", is_first_printed_dest_title ? "" : "\r\n", taxi_dest_type_info[taxi_dest_type].title_string);
    is_first_printed_dest_title = FALSE;

    // Iterate through and populate the dest list with what we've got available.
    for (unsigned int dest_index = 0; *dest_data_list[dest_index].keyword != '\n'; dest_index++) {
      if (DEST_IS_VALID(dest_index, dest_data_list) && dest_data_list[dest_index].type == taxi_dest_type) {
        if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s (keyword: %s)^n\r\n",
                  dest_data_list[dest_index].str,
                  capitalize(dest_data_list[dest_index].keyword));
        } else {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-35s - %s%s^n\r\n",
                  dest_data_list[dest_index].str,
                  taxi_dest_type_info[taxi_dest_type].entry_color_string,
                  capitalize(dest_data_list[dest_index].keyword));
        }
      }
    }
  }

  // Finally, tack on a newline and send it all to the character!
  strcat(buf, "\r\n");
  strlcat(buf, "The keyword for each location is listed after the location name.  ^WSAY^n the keyword to the driver, and for a small fee, he will drive you to your destination.\r\n"
               "Gotten stuck? ^WSAY STUCK^n.\r\n", sizeof(buf));
  send_to_char(buf, ch);

  return TRUE;
}

// ______________________________
//
// utility funcs
// ______________________________

void create_linked_exit(int rnum_a, int dir_a, int rnum_b, int dir_b, const char *source) {
  if (rnum_a <= -1) {
    mudlog("WARNING: Got negative rnum for create_linked_exit()!", NULL, LOG_SYSLOG, TRUE);
  } else {
    if (!world[rnum_a].dir_option[dir_a]) {
      world[rnum_a].dir_option[dir_a] = new room_direction_data;
      memset((char *) world[rnum_a].dir_option[dir_a], 0,
             sizeof(struct room_direction_data));
      world[rnum_a].dir_option[dir_a]->to_room = &world[rnum_b];
      world[rnum_a].dir_option[dir_a]->to_room_vnum = world[rnum_b].number;
      world[rnum_a].dir_option[dir_a]->barrier = 8;
      world[rnum_a].dir_option[dir_a]->condition = 8;
      world[rnum_a].dir_option[dir_a]->material = 8;
      SET_BIT(world[rnum_a].dir_option[dir_a]->exit_info, EX_IS_TEMPORARY);
  #ifdef USE_DEBUG_CANARIES
      world[rnum_a].dir_option[dir_a]->canary = CANARY_VALUE;
  #endif
      snprintf(buf, sizeof(buf), "Successfully created %s exit from %s (%ld) to %s (%ld) for %s.",
               dirs[dir_a],
               GET_ROOM_NAME(&world[rnum_a]),
               GET_ROOM_VNUM(&world[rnum_a]),
               rnum_b >= 0 ? GET_ROOM_NAME(&world[rnum_b]) : "nowhere???",
               rnum_b >= 0 ? GET_ROOM_VNUM(&world[rnum_b]) : -1,
               source
              );
#ifdef TRANSPORT_DEBUG
      mudlog(buf, NULL, LOG_SYSLOG, TRUE);
#endif
    } else {
      /* This situation happens somewhat often-- either we edit a zone that has a ferry/bus/etc, or someone dies in it,
         causing it to be saved to disk with the exit already baked in. This isn't a big issue, so I'm disabling the
         warning. The only real effect is that the state of the transport is out of sync at the start of the game,
         with potentially a straight bridge across the ferry between destinations if the exits are fucky enough.
         It fixes itself over time though. -- LS '22

      snprintf(buf, sizeof(buf), "WARNING: create_linked_exit() for %s (%s from %ld) would have overwritten an existing exit!",
               source, dirs[dir_a], GET_ROOM_VNUM(&world[rnum_a]));
      mudlog(buf, NULL, LOG_SYSLOG, TRUE);
      */
    }
  }

  if (rnum_b <= -1) {
    mudlog("WARNING: Got negative rnum for create_linked_exit()!", NULL, LOG_SYSLOG, TRUE);
  } else {
    if (!world[rnum_b].dir_option[dir_b]) {
      world[rnum_b].dir_option[dir_b] = new room_direction_data;
      memset((char *) world[rnum_b].dir_option[dir_b], 0,
             sizeof(struct room_direction_data));
      world[rnum_b].dir_option[dir_b]->to_room = &world[rnum_a];
      world[rnum_b].dir_option[dir_b]->to_room_vnum = world[rnum_a].number;
      world[rnum_b].dir_option[dir_b]->barrier = 8;
      world[rnum_b].dir_option[dir_b]->condition = 8;
      world[rnum_b].dir_option[dir_b]->material = 8;
      SET_BIT(world[rnum_b].dir_option[dir_b]->exit_info, EX_IS_TEMPORARY);
  #ifdef USE_DEBUG_CANARIES
      world[rnum_b].dir_option[dir_b]->canary = CANARY_VALUE;
  #endif
      snprintf(buf, sizeof(buf), "Successfully created %s exit from %s (%ld) to %s (%ld) for %s.",
               dirs[dir_b],
               GET_ROOM_NAME(&world[rnum_b]),
               GET_ROOM_VNUM(&world[rnum_b]),
               rnum_b >= 0 ? GET_ROOM_NAME(&world[rnum_a]) : "nowhere???",
               rnum_b >= 0 ? GET_ROOM_VNUM(&world[rnum_a]) : -1,
               source
              );
#ifdef TRANSPORT_DEBUG
      mudlog(buf, NULL, LOG_SYSLOG, TRUE);
#endif
    } else {
      snprintf(buf, sizeof(buf), "WARNING: create_linked_exit() for %s (%s from %ld) would have overwritten an existing exit!",
               source, dirs[dir_b], GET_ROOM_VNUM(&world[rnum_b]));
      mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    }
  }
}

void delete_exit(int bus, int to, const char *source) {
  if (!world[bus].dir_option[to]) {
    snprintf(buf, sizeof(buf), "WARNING: delete_exit() for %s got invalid exit %s from %ld!",
             source, dirs[to], GET_ROOM_VNUM(&world[bus]));
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return;
  }

  snprintf(buf, sizeof(buf), "Successfully deleted %s exit from %s (%ld) to %s (%ld) for %s.",
           dirs[to],
           GET_ROOM_NAME(&world[bus]),
           GET_ROOM_VNUM(&world[bus]),
           world[bus].dir_option[to]->to_room ? GET_ROOM_NAME(world[bus].dir_option[to]->to_room) : "nowhere???",
           world[bus].dir_option[to]->to_room ? GET_ROOM_VNUM(world[bus].dir_option[to]->to_room) : -1,
           source
          );

  if (world[bus].dir_option[to]->keyword)
    delete [] world[bus].dir_option[to]->keyword;

  if (world[bus].dir_option[to]->general_description)
    delete [] world[bus].dir_option[to]->general_description;

  delete world[bus].dir_option[to];

  world[bus].dir_option[to] = NULL;

#ifdef TRANSPORT_DEBUG
  mudlog(buf, NULL, LOG_SYSLOG, TRUE);
#endif
}

void delete_linked_exit(int bus, int to, int room, int from, const char *source) {
  delete_exit(bus, to, source);
  delete_exit(room, from, source);
}

void open_taxi_door(struct room_data *room, int dir, struct room_data *taxi, sbyte rating=0)
{
  create_linked_exit(real_room(GET_ROOM_VNUM(room)), dir, real_room(GET_ROOM_VNUM(taxi)), rev_dir[dir], "open taxi door");

  // Optionally set the taxi's room rating so it waits a few ticks before trying to drive off again.
  taxi->rating = rating;
}

void close_taxi_door(struct room_data *room, int dir, struct room_data *taxi)
{
  if (!room || !room->dir_option[dir]) {
    snprintf(buf, sizeof(buf), "WARNING: Taxi %ld had invalid room or exit! (first block - %s (%ld)'s %s)",
             taxi ? GET_ROOM_VNUM(taxi) : -1,
             room ? GET_ROOM_NAME(room) : "NO ROOM",
             room ? GET_ROOM_VNUM(room) : -1,
             dirs[dir]);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  } else {
    snprintf(buf, sizeof(buf), "Successfully deleted %s exit from %s (%ld) to %s (%ld) for close-taxi-door (first block).",
             dirs[dir],
             GET_ROOM_NAME(room),
             GET_ROOM_VNUM(room),
             room->dir_option[dir]->to_room ? GET_ROOM_NAME(room->dir_option[dir]->to_room) : "nowhere???",
             room->dir_option[dir]->to_room ? GET_ROOM_VNUM(room->dir_option[dir]->to_room) : -1
            );

    if (room->dir_option[dir]->keyword)
      delete [] room->dir_option[dir]->keyword;
    if (room->dir_option[dir]->general_description)
      delete [] room->dir_option[dir]->general_description;
    delete room->dir_option[dir];
    room->dir_option[dir] = NULL;

#ifdef TRANSPORT_DEBUG
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
#endif
  }

  dir = rev_dir[dir];

  if (!taxi || !taxi->dir_option[dir]) {
    snprintf(buf, sizeof(buf), "WARNING: Taxi %ld had invalid room or exit! (second block - %s (%ld)'s %s)",
             taxi ? GET_ROOM_VNUM(taxi) : -1,
             taxi ? GET_ROOM_NAME(taxi) : "NO ROOM",
             taxi ? GET_ROOM_VNUM(taxi) : -1,
             dirs[dir]);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  } else {
    snprintf(buf, sizeof(buf), "Successfully deleted %s exit from %s (%ld) to %s (%ld) for close-taxi-door (second block).",
             dirs[dir],
             GET_ROOM_NAME(taxi),
             GET_ROOM_VNUM(taxi),
             taxi->dir_option[dir]->to_room ? GET_ROOM_NAME(taxi->dir_option[dir]->to_room) : "nowhere???",
             taxi->dir_option[dir]->to_room ? GET_ROOM_VNUM(taxi->dir_option[dir]->to_room) : -1
            );

    if (taxi->dir_option[dir]->keyword)
      delete [] taxi->dir_option[dir]->keyword;
    if (taxi->dir_option[dir]->general_description)
      delete [] taxi->dir_option[dir]->general_description;
    delete taxi->dir_option[dir];
    taxi->dir_option[dir] = NULL;

#ifdef TRANSPORT_DEBUG
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
#endif
  }
}

void raw_taxi_leaves(rnum_t real_room_num) {
  struct room_data *to;

  // Message the cab and surrounding rooms.
  for (int i = NORTH; i < UP; i++)
    if (world[real_room_num].dir_option[i]) {
      to = world[real_room_num].dir_option[i]->to_room;
      close_taxi_door(to, rev_dir[i], &world[real_room_num]);
      if (to->people) {
        if (real_room_num >= real_room(FIRST_SEATTLE_CAB) && real_room_num <= real_room(LAST_SEATTLE_CAB))
          snprintf(buf, sizeof(buf), "The taxi door slams shut as its wheels churn up a cloud of smoke.");
        else if (real_room_num >= real_room(FIRST_PORTLAND_CAB) && real_room_num <= real_room(LAST_PORTLAND_CAB))
          snprintf(buf, sizeof(buf), "The taxi doors slide shut and it pulls off from the curb.");
        else // if (real_room_num >= real_room(FIRST_CARIBBEAN_CAB) || real_room_num <= real_room(LAST_CARIBBEAN_CAB))
          snprintf(buf, sizeof(buf), "The truck's doors close and it pulls away from the curb.");

        act(buf, FALSE, to->people, 0, 0, TO_ROOM);
        act(buf, FALSE, to->people, 0, 0, TO_CHAR);
      }
      if (world[real_room_num].people) {
        act("The door shuts as the taxi begins to accelerate.",
            FALSE, world[real_room_num].people, 0, 0, TO_ROOM);
        act("The door shuts as the taxi begins to accelerate.",
            FALSE, world[real_room_num].people, 0, 0, TO_CHAR);
      }
    }
}

void do_taxi_leaves(rnum_t real_room_num) {
  struct char_data *temp;

  if (real_room_num <= NOWHERE || real_room_num > top_of_world) {
    snprintf(buf, sizeof(buf), "WARNING: Invalid taxi rnum %ld given to do_taxi_leaves().", real_room_num);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // Skip taxis that are occupied by non-drivers.
  for (temp = world[real_room_num].people; temp; temp = temp->next_in_room)
    if (!(GET_MOB_SPEC(temp)
          && (GET_MOB_SPEC(temp) == taxi
              || GET_MOB_SPEC2(temp) == taxi))) {
      return;
    }

  // Decrement taxis with a wait-rating, then skip them.
  if (world[real_room_num].rating > 0) {
    world[real_room_num].rating--;
    return;
  }

  raw_taxi_leaves(real_room_num);
}
void taxi_leaves(void)
{
  int rr_last_seattle_cab = real_room(LAST_SEATTLE_CAB);
  for (int j = real_room(FIRST_SEATTLE_CAB); j <= rr_last_seattle_cab; j++) {
    do_taxi_leaves(j);
  }

#ifdef USE_PRIVATE_CE_WORLD
  int rr_last_caribean_cab = real_room(LAST_CARIBBEAN_CAB);
  for (int j = real_room(FIRST_CARIBBEAN_CAB); j <= rr_last_caribean_cab; j++) {
    do_taxi_leaves(j);
  }
#endif

  int rr_last_portland_cab = real_room(LAST_PORTLAND_CAB);
  for (int j = real_room(FIRST_PORTLAND_CAB); j <= rr_last_portland_cab; j++) {
    do_taxi_leaves(j);
  }
}

// ______________________________
//
// hail command
// ______________________________

ACMD(do_hail)
{
  struct char_data *temp;
  int cab, first, last;
  bool found = FALSE, empty = FALSE;
  SPECIAL(taxi);

  struct dest_data *dest_data_list = NULL;

  if (!ch->in_room) {
    send_to_char("You can't do that here.\r\n", ch);
    mudlog("SYSERR: Attempt to hail a vehicle from non-room location.", ch, LOG_SYSLOG, TRUE);
    return;
  }

  for (int dir = NORTH; dir < UP; dir++)
    if (!ch->in_room->dir_option[dir])
      empty = TRUE;

  if (IS_ASTRAL(ch)) {
    send_to_char("Magically active cab drivers...now I've heard everything...\n\r",ch);
    return;
  }

  // Special condition: Hailing from Imm HQ's central room.
  if (ch->in_room->number == 10000) {
    skip_spaces(&argument);
    if (*argument && !strncmp(argument, "portland", strlen(argument))) {
      send_to_char("Portland cab network selected.\r\n", ch);
      dest_data_list = portland_taxi_destinations;
    } else if (*argument && !strncmp(argument, "caribbean", strlen(argument))){
      send_to_char("Caribbean cab network selected.\r\n", ch);
      dest_data_list = caribbean_taxi_destinations;
    } else {
      send_to_char("Seattle (default) cab network selected. Use 'hail portland' or 'hail caribbean' for others.\r\n", ch);
      dest_data_list = seattle_taxi_destinations;
    }
  }
  else {
    // Cabs can always be caught from the freeway. Realistic? Nah, but keeps people from being stranded.
    if (!ROOM_FLAGGED(ch->in_room, ROOM_FREEWAY)
        && (ch->in_room->sector_type != SPIRIT_CITY
            || !empty
            || ROOM_FLAGGED(ch->in_room, ROOM_INDOORS))) {
      send_to_char("This isn't really the kind of place that cabs frequent.\r\n", ch);
      return;
    }

    dest_data_list = get_dest_data_list_for_zone(zone_table[ch->in_room->zone].number);

    if (dest_data_list == NULL) {
      // Someone fucked up.
      if (ROOM_FLAGGED(ch->in_room, ROOM_FREEWAY)) {
        snprintf(buf, sizeof(buf), "WARNING: Freeway room '%s' (%ld) is not supported in cab switch statement! Defaulting to Seattle.",
                 GET_ROOM_NAME(ch->in_room), GET_ROOM_VNUM(ch->in_room));
        mudlog(buf, ch, LOG_SYSLOG, TRUE);
        dest_data_list = seattle_taxi_destinations;
      } else {
        /* Cab doesn't service the area */
        send_to_char("There don't seem to be any cabs active in the area.\r\n",ch);
        snprintf(buf, sizeof(buf), "Cab expansion opportunity: Called in '%s' (%ld) of zone '%s' (%d), which is not supported in cab switch.",
                 GET_ROOM_NAME(ch->in_room), GET_ROOM_VNUM(ch->in_room),
                 zone_table[ch->in_room->zone].name, zone_table[ch->in_room->zone].number);
        mudlog(buf, ch, LOG_SYSLOG, TRUE);
        return;
      }
    }

    if (AFF_FLAGS(ch).AreAnySet(AFF_SPELLINVIS, AFF_INVISIBLE, AFF_SPELLIMPINVIS, AFF_IMP_INVIS, ENDBIT)
        || GET_INVIS_LEV(ch) > 0)  {
      send_to_char("A cab almost stops, but guns it at the last second, splashing you... hailing taxis while invisible is hard!\r\n",ch);
      return;
    }
}
  if (dest_data_list == portland_taxi_destinations) {
    first = real_room(FIRST_PORTLAND_CAB);
    last = real_room(LAST_PORTLAND_CAB);
  } else if (dest_data_list == caribbean_taxi_destinations) {
    first = real_room(FIRST_CARIBBEAN_CAB);
    last = real_room(LAST_CARIBBEAN_CAB);
  } else {
    first = real_room(FIRST_SEATTLE_CAB);
    last = real_room(LAST_SEATTLE_CAB);
  }

  for (cab = first; cab <= last; cab++) {
    // Skip in-use cabs.
    for (temp = world[cab].people; temp; temp = temp->next_in_room)
      if (!(GET_MOB_SPEC(temp) && (GET_MOB_SPEC(temp) == taxi || GET_MOB_SPEC2(temp) == taxi)))
        break;
    if (!temp) {
      found = TRUE;
      // Skip cabs that are already open and waiting at the curb.
      for (int dir = NORTH; dir < UP; dir++)
        if (world[cab].dir_option[dir])
          found = FALSE;
      if (found)
        break;
    }
  }

  if (!found) {
    send_to_char("Hail as you might, no cab answers.\r\n", ch);
    snprintf(buf, sizeof(buf), "Warning: No cab available in %s-- are all in use?",
             dest_data_list == seattle_taxi_destinations ? "Seattle" :
               (dest_data_list == portland_taxi_destinations ? "Portland" :
                 (dest_data_list == caribbean_taxi_destinations ? "the Caribbean" : "ERROR")));
    mudlog(buf, ch, LOG_SYSLOG, TRUE);
    return;
  }

  for (int dir = number(NORTH, UP - 1);; dir = number(NORTH, UP - 1))
    if (!ch->in_room->dir_option[dir]) {
      open_taxi_door(ch->in_room, dir, &world[cab], 1);
      if (dest_data_list == portland_taxi_destinations)
        snprintf(buf, sizeof(buf), "A nice looking red and white cab pulls up smoothly to the curb, "
                 "and its door opens to the %s.", fulldirs[dir]);
      else if (dest_data_list == caribbean_taxi_destinations)
        snprintf(buf, sizeof(buf), "A small, compact truck with a retrofitted cab putters up to the curb, "
                 "and its door opens to the %s.", fulldirs[dir]);
      else
        snprintf(buf, sizeof(buf), "A beat-up yellow cab screeches to a halt, "
                 "and its door opens to the %s.", fulldirs[dir]);

      act(buf, FALSE, ch, 0, 0, TO_ROOM);
      act(buf, FALSE, ch, 0, 0, TO_CHAR);
      return;
    }
}

// ______________________________
//
// driver spec
// ______________________________

// get_dest_data_list_for_zone(zone_table[ch->in_room->zone].number);
struct dest_data *get_dest_data_list_for_zone(int zone_num) {
  switch (zone_num) {
    case 13:
    case 15:
    case 16:
    case 20:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 29:
    case 30:
    case 32:
    case 38:
    case 40:
    case 42:
    case 45:
    case 49:
    case 74:
    case 81:
    case 91:
    case 143:
    case 194:
    case 290:
    case 293:
    case 301:
    case 305:
    case 325:
    case 392:
    case 707:
    case 253:
    case 725:
      return seattle_taxi_destinations;
    case 27:
    case 146:
    case 188:
    case 208:
    case 241:
      return portland_taxi_destinations;
    case 621:
    case 622:
    case 623:
      return caribbean_taxi_destinations;
  }

  // Destination not found.
  return NULL;
}

// get_dest_data_list_from_driver_vnum(GET_MOB_VNUM(driver));
struct dest_data *get_dest_data_list_from_driver_vnum(vnum_t vnum) {
  switch (vnum) {
    case 600:
      return seattle_taxi_destinations;
    case 650:
      return portland_taxi_destinations;
    case 640:
      return caribbean_taxi_destinations;
  }

  // Destination not found.
  return NULL;
}

SPECIAL(taxi)
{
  extern bool memory(struct char_data *ch, struct char_data *vict);
  ACMD_CONST(do_say);
  ACMD_DECLARE(do_action);

  struct char_data *temp = NULL, *driver = (struct char_data *) me;
  struct room_data *temp_room = NULL;
  int comm = CMD_TAXI_NONE;
  char say[MAX_STRING_LENGTH];
  vnum_t dest = 0;

  struct dest_data *destination_list = get_dest_data_list_from_driver_vnum(GET_MOB_VNUM(driver));

  // Evaluate the taxi driver's reactions.
  if (!cmd) {
    // Iterate through the cab's contents, looking for anyone that the driver recognizes.
    for (temp = driver->in_room->people; temp; temp = temp->next_in_room)
      if (temp != driver && memory(driver, temp))
        break;

    // If nobody was found, clear out the driver's memory structure and wait for a command.
    if (!temp) {
      GET_SPARE1(driver) = 0;
      GET_SPARE2(driver) = 0;
      GET_ACTIVE(driver) = ACT_AWAIT_CMD;
      return FALSE;
    }

    // Otherwise, process the incoming command.
    switch (GET_ACTIVE(driver)) {
      case ACT_REPLY_DEST:
        // Are we in gridguide mode?
        if (GET_SPARE2(driver) < 0) {
          if (destination_list == portland_taxi_destinations)
            snprintf(say, sizeof(say), "%s?  Sure, that will be %d nuyen.",
                     get_string_after_color_code_removal(GET_ROOM_NAME(&world[real_room(-GET_SPARE2(driver))]), ch), (int)GET_SPARE1(driver));
          else if (destination_list == caribbean_taxi_destinations)
            snprintf(say, sizeof(say), "%s?  Yeah, sure...it'll cost ya %d nuyen, whaddya say?",
                     get_string_after_color_code_removal(GET_ROOM_NAME(&world[real_room(-GET_SPARE2(driver))]), ch), (int)GET_SPARE1(driver));
          else
            snprintf(say, sizeof(say), "%s?  Yeah, sure...it'll cost ya %d nuyen, whaddya say?",
                     get_string_after_color_code_removal(GET_ROOM_NAME(&world[real_room(-GET_SPARE2(driver))]), ch), (int)GET_SPARE1(driver));

          do_say(driver, say, 0, 0);
          if (GET_EXTRA(driver) == 1) {
            do_say(driver, "But seeing as you're new around here, I'll waive my usual fee, okay?", 0, 0);
            GET_SPARE1(driver) = 0;
          }
        }
        // Standard dest mode.
        else {
          if (destination_list[GET_SPARE2(driver)].vnum == RM_NERPCORPOLIS_LOBBY)
            do_say(ch, "The NERPcorpolis?  Sure, that ride is free.  Want to go there?", 0, 0);
          else {
            if (destination_list == portland_taxi_destinations)
              snprintf(say, sizeof(say), "%s?  Sure, that will be %d nuyen.",
                       portland_taxi_destinations[GET_SPARE2(driver)].str, (int)GET_SPARE1(driver));
            else if (destination_list == caribbean_taxi_destinations)
              snprintf(say, sizeof(say), "%s?  Yeah, sure...it'll cost ya %d nuyen, whaddya say?",
                       caribbean_taxi_destinations[GET_SPARE2(driver)].str, (int)GET_SPARE1(driver));
            else
              snprintf(say, sizeof(say), "%s?  Yeah, sure...it'll cost ya %d nuyen, whaddya say?",
                      seattle_taxi_destinations[GET_SPARE2(driver)].str, (int)GET_SPARE1(driver));
            do_say(driver, say, 0, 0);
            if (GET_EXTRA(driver) == 1) {
              do_say(driver, "But seeing as you're new around here, I'll waive my usual fee, okay?", 0, 0);
              GET_SPARE1(driver) = 0;
            }
          }
        }

        GET_EXTRA(driver) = 0;
        GET_ACTIVE(driver) = ACT_AWAIT_YESNO;
        break;
      case ACT_REPLY_NOTOK:
        if (destination_list == portland_taxi_destinations)
          do_say(driver, "You don't seem to have enough nuyen!", 0, 0);
        else
          do_say(driver, "Ya ain't got the nuyen!", 0, 0);
        forget(driver, temp);
        GET_SPARE1(driver) = 0;
        GET_SPARE2(driver) = 0;
        GET_ACTIVE(driver) = ACT_AWAIT_CMD;
        break;
      case ACT_DRIVING:
        if (GET_SPARE1(driver) > 0)
          GET_SPARE1(driver)--;
        else {
          int x[] = { 0, 0, 0, 0, 0, 0, 0, 0 }, y = 0;
          for (int j = number(NORTH, NORTHWEST);; j = number(NORTH, NORTHWEST)) {
            if (!x[j]) {
              x[j] = 1;
              y++;
            }
            if (y == NORTHWEST)
              return FALSE;
            dest = real_room(GET_SPARE2(driver));
            if (!world[dest].dir_option[j]) {
              open_taxi_door(&world[dest], j, driver->in_room);
              do_say(driver, "Ok, here we are.", 0, 0);
              forget(driver, temp);
              GET_SPARE2(driver) = 0;
              GET_ACTIVE(driver) = ACT_AWAIT_CMD;
              if (world[dest].people) {
                act("A taxi pulls to a stop, its door sliding open.",
                    FALSE, world[dest].people, 0, 0, TO_ROOM);
                act("A taxi pulls to a stop, its door sliding open.",
                    FALSE, world[dest].people, 0, 0, TO_CHAR);
              }
              snprintf(buf, sizeof(buf), "The door, rather noisily, slides open to the %s.",
                      fulldirs[rev_dir[j]]);
              act(buf, FALSE, driver, 0, 0, TO_ROOM);
              act(buf, FALSE, driver, 0, 0, TO_CHAR);
              break;
            }
          }
        }
        break;
    }

    // Bail out after processing the command.
    return FALSE;
  }

  // If someone's bailing out of the cab, forget them.
  if (!IS_NPC(ch) && memory(driver, ch) && (CMD_IS("north") ||
      CMD_IS("east") || CMD_IS("west") || CMD_IS("south") || CMD_IS("ne") ||
      CMD_IS("se") || CMD_IS("sw") || CMD_IS("nw") || CMD_IS("northeast") ||
      CMD_IS("southeast") || CMD_IS("southwest") || CMD_IS("northwest") || CMD_IS("leave")) && GET_ACTIVE(driver) != ACT_DRIVING) {
    forget(driver, ch);
    return FALSE;
  }

  // If you're an NPC, or if the driver's not listening in the first place-- bail.
  if (IS_NPC(ch) ||
      (GET_ACTIVE(driver) != ACT_AWAIT_CMD &&
       GET_ACTIVE(driver) != ACT_AWAIT_YESNO))
    return FALSE;

  skip_spaces(&argument);

  if (CMD_IS("say") || CMD_IS("'")) {
    // Failure condition: If you can't speak, the cabbie can't hear you.
    if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
      return FALSE;

    bool found = FALSE;
    if (GET_ACTIVE(driver) == ACT_AWAIT_CMD)
      for (dest = 0; *(destination_list[dest].keyword) != '\n'; dest++) {
        // Skip invalid destinations.
        if (!DEST_IS_VALID(dest, destination_list))
          continue;

        if (str_str((const char *)argument, destination_list[dest].keyword)
            || (*(destination_list[dest].subkeyword) && str_str((const char *)argument, destination_list[dest].subkeyword)))
        {
          comm = CMD_TAXI_DEST;
          found = TRUE;
          do_say(ch, argument, 0, 0);
          strncpy(buf2, " punches a few buttons on the meter, calculating the fare.", sizeof(buf2));
          do_echo(driver, buf2, 0, SCMD_EMOTE);
          forget(driver, ch);
          break;
        }

        if (str_str((const char *)argument, "stuck")) {
          for (int i = 0; i < NUM_OF_DIRS; i++)
            if (EXIT(ch, i) && CAN_GO(ch, i)) {
              send_to_char(ch, "^M(OOC: There's an exit to the %s, try heading in that direction.)^n\r\n", fulldirs[i]);
              return TRUE;
            }

          send_to_char(ch, "^M(OOC: Sorry you're stuck! We'll send you to Dante's Inferno. This will be logged for staff review.)^n\r\n");
          mudlog("^YUsing stuck command to get out of taxi.^g", ch, LOG_GRIDLOG, TRUE);
          char_from_room(ch);

          int rnum = real_room(RM_ENTRANCE_TO_DANTES);
          if (rnum > -1)
            char_to_room(ch, &world[rnum]);
          else
            char_to_room(ch, &world[1]);

          act("$n is ejected from a taxi.", TRUE, ch, 0, 0, TO_ROOM);

          look_at_room(ch, 0, 0);
          return TRUE;
        }
      }

    if (!found) {
      if (GET_ACTIVE(driver) == ACT_AWAIT_YESNO) {
        if (str_str(argument, "yes") || str_str(argument, "sure") || str_str(argument, "alright") ||
            str_str(argument, "yeah") || str_str(argument, "okay") || str_str(argument, "yup") || str_str(argument, "OK")) {
          comm = CMD_TAXI_YES;
        } else if (str_str(argument, "no") || str_str(argument, "nah") || str_str(argument, "negative")) {
          comm = CMD_TAXI_NO;
        }
      } else {
        // The destination was not on the list. Check for gridguide coordinates.
        char x_buf[MAX_INPUT_LENGTH + 1], y_buf[MAX_INPUT_LENGTH + 1];
        two_arguments(argument, x_buf, y_buf);
        long x_coord = atol(x_buf), y_coord = atol(y_buf);

        // Looks like we got gridguide coordinates.
        if (x_coord && y_coord) {
          // Check to see if they're valid.
          vnum_t dest_vnum = vnum_from_gridguide_coordinates(x_coord, y_coord, ch);

          // Invalid coordinates.
          if (dest_vnum <= 0) {
            strncpy(buf2, " punches a few buttons on the meter to calculate the fare, but it immediately flashes red.", sizeof(buf2));
            do_echo(driver, buf2, 0, SCMD_EMOTE);
            snprintf(buf2, sizeof(buf2), " Oi, chummer, those aren't valid coordinates.");
            do_say(driver, buf2, 0, 0);
            return TRUE;
          }

          // Cross-jurisdiction (Seattle to Caribbean, etc).
          if (!cab_jurisdiction_matches_destination(GET_ROOM_VNUM(ch->in_room), dest_vnum)) {
            strncpy(buf2, " punches a few buttons on the meter to calculate the fare, but it immediately flashes yellow.", sizeof(buf2));
            do_echo(driver, buf2, 0, SCMD_EMOTE);
            snprintf(buf2, sizeof(buf2), " How exactly am I supposed to get there?");
            do_say(driver, buf2, 0, 0);
            return TRUE;
          }

          // Grid doesn't extend there in an unbroken path? No good.
          // 1. Find the room the taxi is attached to.
          temp_room = NULL;
          for (int dir = NORTH; dir < UP; dir++)
            if (ch->in_room->dir_option[dir]) {
              temp_room = ch->in_room->dir_option[dir]->to_room;
              break;
            }

          // 2. Pathfind there. Fail if we can't find a path.
          if (temp_room && find_first_step(real_room(temp_room->number), real_room(dest_vnum), FALSE) < 0) {
            strncpy(buf2, " punches a few buttons on the meter to calculate the fare, but it flashes red after a moment.", sizeof(buf2));
            do_echo(driver, buf2, 0, SCMD_EMOTE);
            snprintf(buf2, sizeof(buf2), " The GridGuide network doesn't connect through to there.");
            do_say(driver, buf2, 0, 0);
            return TRUE;
          }

          // Valid location.
          comm = CMD_TAXI_DEST_GRIDGUIDE;
          found = TRUE;
          dest = -dest_vnum;
          do_say(ch, argument, 0, 0);
          strncpy(buf2, " punches a few buttons on the meter, calculating the fare.", sizeof(buf2));
          do_echo(driver, buf2, 0, SCMD_EMOTE);
          forget(driver, ch);
        }

        // We didn't get a pair of coordinates, either. Bail out.
        else {
          do_say(ch, argument, 0, 0);
          if (destination_list == portland_taxi_destinations) {
            snprintf(buf2, sizeof(buf2), " Sorry chummer, rules are rules. You need to tell me something from the sign, or give me a pair of GridGuide coordinates.");
          } else {
            snprintf(buf2, sizeof(buf2), " Hey chummer, rules is rules. You gotta tell me something off of that sign there, or give me a pair of GridGuide coordinates.");
          }
          do_say(driver, buf2, 0, 0);
          return TRUE;
        }
      }
    }
  } else if ((CMD_IS("nod") || CMD_IS("agree")) && CAN_SEE(driver, ch)) {
    comm = CMD_TAXI_YES;
    do_action(ch, argument, cmd, 0);
  } else if ((CMD_IS("shake") || CMD_IS("disagree")) && CAN_SEE(driver, ch)) {
    comm = CMD_TAXI_NO;
    do_action(ch, argument, cmd, 0);
  } else
    return FALSE;

  /* I would like to extend a personal and heartfelt 'fuck you' to whomever thought that using the anonymously-named 'i' as both an rnum and a direction was a good idea. - LS */
  if ((comm == CMD_TAXI_DEST || comm == CMD_TAXI_DEST_GRIDGUIDE) && !memory(driver, ch) && GET_ACTIVE(driver) == ACT_AWAIT_CMD) {
    if (real_room(GET_LASTROOM(ch)) <= -1) {
      GET_SPARE1(driver) = MAX_CAB_FARE;
    } else {
      // We're in a cab, so find the door out. This identifies our start room.
      for (int dir = NORTH; dir < UP; dir++)
        if (ch->in_room->dir_option[dir]) {
          temp_room = ch->in_room->dir_option[dir]->to_room;
          break;
        }

      int distance_between_rooms;
      if (comm == CMD_TAXI_DEST)
        distance_between_rooms = calculate_distance_between_rooms(temp_room->number, destination_list[dest].vnum, FALSE);
      else
        distance_between_rooms = calculate_distance_between_rooms(temp_room->number, -dest, FALSE);

      if (distance_between_rooms < 0)
        GET_SPARE1(driver) = MAX_CAB_FARE;
      else
        GET_SPARE1(driver) = MIN(MAX_CAB_FARE, 5 + distance_between_rooms);
    }

    // Rides to the NERPcorpolis are free.
    if (dest > 0 && destination_list[dest].vnum == RM_NERPCORPOLIS_LOBBY)
      GET_SPARE1(driver) = 0;

    GET_SPARE2(driver) = dest;
    GET_ACTIVE(driver) = ACT_REPLY_DEST;
    if (PLR_FLAGGED(ch, PLR_NEWBIE))
      GET_EXTRA(driver) = 1;
    else GET_EXTRA(driver) = 0;
    remember(driver, ch);
  } else if (comm == CMD_TAXI_YES && memory(driver, ch) &&
             GET_ACTIVE(driver) == ACT_AWAIT_YESNO) {
    if (GET_NUYEN(ch) < GET_SPARE1(driver) && !IS_SENATOR(ch)) {
      GET_ACTIVE(driver) = ACT_REPLY_NOTOK;
      return TRUE;
    }
    if (!IS_SENATOR(ch))
      lose_nuyen(ch, GET_SPARE1(driver), NUYEN_OUTFLOW_TAXIS);

    // Set the delay, staff get no delay.
    if (access_level(ch, LVL_BUILDER)) {
      send_to_char("Your staff status convinces the driver to go as fast as possible.\r\n", ch);
      GET_SPARE1(driver) = 1;
    } else
      GET_SPARE1(driver) = (int)(GET_SPARE1(driver) / 50);
    // Are we in gridguide mode? If so, this holds the vnum in negative form.
    if (GET_SPARE2(driver) < 0)
      GET_SPARE2(driver) *= -1;
    else
      GET_SPARE2(driver) = destination_list[GET_SPARE2(driver)].vnum;
    GET_ACTIVE(driver) = ACT_DRIVING;

    raw_taxi_leaves(real_room(GET_ROOM_VNUM(ch->in_room)));
  } else if (comm == CMD_TAXI_NO && memory(driver, ch) && GET_ACTIVE(driver) == ACT_AWAIT_YESNO) {
    if (destination_list == portland_taxi_destinations)
      do_say(driver, "Somewhere else then, chummer?", 0, 0);
    else
      do_say(driver, "Whatever, chum.", 0, 0);
    forget(driver, ch);
    GET_SPARE1(driver) = 0;
    GET_SPARE2(driver) = 0;
    GET_ACTIVE(driver) = ACT_AWAIT_CMD;
  }

  return TRUE;
}

// ______________________________
//
// Elevators
// ______________________________

// ______________________________
//
// utility funcs
// ______________________________

void make_elevator_door(vnum_t rnum_to, vnum_t rnum_from, int direction_from) {
  // If it doesn't exist, make it so.
#define DOOR world[rnum_from].dir_option[direction_from]
  if (!DOOR) {
#ifdef ELEVATOR_DEBUG
    snprintf(buf, sizeof(buf), "Building new elevator door %s from %ld to %ld.", fulldirs[direction_from], world[rnum_from].number, world[rnum_to].number);
    log(buf);
#endif
    DOOR = new room_direction_data;
    memset((char *) DOOR, 0, sizeof(struct room_direction_data));
#ifdef USE_DEBUG_CANARIES
    DOOR->canary = CANARY_VALUE;
#endif
  } else {
#ifdef ELEVATOR_DEBUG
    snprintf(buf, sizeof(buf), "Reusing existing elevator door %s from %ld to %ld.", fulldirs[direction_from], world[rnum_from].number, world[rnum_to].number);
    log(buf);
#endif
  }

  // Set the exit to point to the correct rnum.
  DOOR->to_room = &world[rnum_to];

  if (!DOOR->general_description)
    DOOR->general_description = str_dup("A pair of elevator doors allows access to the liftway.\r\n");

  if (!DOOR->keyword)
    DOOR->keyword = str_dup("doors shaft");

  if (!DOOR->key)
    DOOR->key = OBJ_ELEVATOR_SHAFT_KEY;

  if (!DOOR->key_level || DOOR->key_level < 8)
    DOOR->key_level = 8;

  if (!DOOR->material || DOOR->material == MATERIAL_BRICK)
    DOOR->material = MATERIAL_METAL;

  if (!DOOR->barrier || DOOR->barrier < 8)
    DOOR->barrier = 8;

#ifdef USE_DEBUG_CANARIES
    DOOR->canary = CANARY_VALUE;
#endif

  // Set up the door's flags.
  SET_BIT(DOOR->exit_info, EX_ISDOOR);
  SET_BIT(DOOR->exit_info, EX_CLOSED);
  SET_BIT(DOOR->exit_info, EX_LOCKED);
#undef DOOR
}

static void init_elevators(void)
{
  FILE *fl;
  char line[256];
  int i, j, t[3];
  vnum_t room, rnum, shaft_vnum, shaft_rnum;
  int real_button = -1, real_panel = -1;

  struct obj_data *obj = NULL;

  if (!(fl = fopen(ELEVATOR_FILE, "r"))) {
    log("Error opening elevator file.");
    shutdown();
  }

  if (!get_line(fl, line) || sscanf(line, "%d", &num_elevators) != 1) {
    log("Error at beginning of elevator file.");
    shutdown();
  }

  // E_B_V and E_P_V are defined in transport.h
  if ((real_button = real_object(ELEVATOR_BUTTON_VNUM)) < 0)
    log("Elevator button object does not exist; will not load elevator call buttons.");
  if ((real_panel = real_object(ELEVATOR_PANEL_VNUM)) < 0)
    log("Elevator panel object does not exist; will not load elevator control panels.");

  elevator = new struct elevator_data[num_elevators];

  for (i = 0; i < num_elevators && !feof(fl); i++) {
    get_line(fl, line);
    if (sscanf(line, "%ld %d %d %d", &room, t, t + 1, t + 2) != 4) {
      fprintf(stderr, "Format error in elevator #%d, expecting # # # #\n", i);
      shutdown();
    }
    elevator[i].room = room;
    if ((rnum = real_room(elevator[i].room)) > -1) {
      world[rnum].func = elevator_spec;
      world[rnum].rating = 0;
      if (real_panel >= 0) {
        // Add decorative elevator control panel to elevator car.
        obj = read_object(real_panel, REAL);
        obj_to_room(obj, &world[rnum]);
      }
    } else {
      snprintf(buf, sizeof(buf), "Nonexistent elevator cab %ld in elevator %d. Skipping.", elevator[i].room, i);
      log(buf);
      continue;
    }


    elevator[i].columns = t[0];
    elevator[i].time_left = 0;
    elevator[i].dir = -1;
    elevator[i].destination = 0;
    elevator[i].num_floors = t[1];
    elevator[i].start_floor = t[2];

    if (elevator[i].num_floors > 0) {
      elevator[i].floor = new struct floor_data[elevator[i].num_floors];
      for (j = 0; j < elevator[i].num_floors; j++) {
        get_line(fl, line);
        if (sscanf(line, "%ld %ld %d", &room, &shaft_vnum, t + 1) != 3) {
          fprintf(stderr, "Format error in elevator #%d, floor #%d\n", i, j);
          shutdown();
        }
        elevator[i].floor[j].vnum = room;
        elevator[i].floor[j].shaft_vnum = shaft_vnum;
        elevator[i].floor[j].doors = t[1];
        //snprintf(buf, sizeof(buf), "%s: vnum %ld, shaft %ld, doors %s.", line, room, shaft_vnum, fulldirs[t[1]]);
        //log(buf);

        // Ensure the landing exists.
        rnum = real_room(elevator[i].floor[j].vnum);
        if (rnum > -1) {
          world[rnum].func = call_elevator;
          if (real_button >= 0) {
            // Add decorative elevator call button to landing.
            obj = read_object(real_button, REAL);
            obj_to_room(obj, &world[rnum]);
          }

          // Ensure that there is an elevator door leading to/from the shaft.
          shaft_rnum = real_room(elevator[i].floor[j].shaft_vnum);
          if (shaft_rnum > -1) {
            make_elevator_door(rnum, shaft_rnum, elevator[i].floor[j].doors);
            make_elevator_door(shaft_rnum, rnum, rev_dir[elevator[i].floor[j].doors]);
            // Flag the shaft appropriately.
            ROOM_FLAGS(&world[shaft_rnum]).SetBits(ROOM_NOMOB, ROOM_INDOORS, ROOM_NOBIKE, ROOM_ELEVATOR_SHAFT, ROOM_FALL, ENDBIT);
            world[shaft_rnum].rating = ELEVATOR_SHAFT_FALL_RATING;

            // Set up the combat options.
            world[shaft_rnum].x = MAX(2, world[shaft_rnum].x);
            world[shaft_rnum].y = MAX(2, world[shaft_rnum].y);
            world[shaft_rnum].z = 5.0; // Override the existing values, since this factors into fall rating.
          } else {
            snprintf(buf, sizeof(buf), "Fatal error: Nonexistent elevator shaft vnum %ld.", elevator[i].floor[j].shaft_vnum);
            log(buf);
            shutdown();
          }
        } else {
          snprintf(buf, sizeof(buf), "Nonexistent elevator destination %ld -- blocking.", elevator[i].floor[j].vnum);
          log(buf);
          elevator[i].floor[j].vnum = -1;
        }
      }

      // Now make sure the shaft rooms are connected.
      long last_shaft_vnum = -1;
      for (j = 0; j < elevator[i].num_floors; j++) {
        // No error recovery here- if the shaft is invalid, bail tf out.
        if (real_room(elevator[i].floor[j].shaft_vnum) == -1) {
          snprintf(buf, sizeof(buf), "SYSERR: Shaft vnum %ld for floor %d of elevator %d is invalid.",
                  elevator[i].floor[j].shaft_vnum, j, i);
          mudlog(buf, NULL, LOG_SYSLOG, TRUE);
          break;
        }

        // First shaft room? Nothing to do-- we didn't verify the kosherness of the next one yet.
        if (last_shaft_vnum == -1) {
          last_shaft_vnum = elevator[i].floor[j].shaft_vnum;
          continue;
        }

#define DOOR(index, dir) world[real_room(elevator[i].floor[index].shaft_vnum)].dir_option[dir]
        // Does the exit already exist?
        if (DOOR(j, UP)) {
          // Purge it.
          DELETE_ARRAY_IF_EXTANT(DOOR(j, UP)->keyword);
          DELETE_ARRAY_IF_EXTANT(DOOR(j, UP)->general_description);
          DELETE_ARRAY_IF_EXTANT(DOOR(j, UP)->go_into_thirdperson);
          DELETE_ARRAY_IF_EXTANT(DOOR(j, UP)->go_into_secondperson);
          DELETE_ARRAY_IF_EXTANT(DOOR(j, UP)->come_out_of_thirdperson);
          DELETE_AND_NULL(DOOR(j, UP));
        }

        // Does its mirror already exist?
        if (DOOR(j - 1, DOWN)) {
          // Purge it.
          DELETE_ARRAY_IF_EXTANT(DOOR(j - 1, DOWN)->keyword);
          DELETE_ARRAY_IF_EXTANT(DOOR(j - 1, DOWN)->general_description);
          DELETE_ARRAY_IF_EXTANT(DOOR(j - 1, DOWN)->go_into_thirdperson);
          DELETE_ARRAY_IF_EXTANT(DOOR(j - 1, DOWN)->go_into_secondperson);
          DELETE_ARRAY_IF_EXTANT(DOOR(j - 1, DOWN)->come_out_of_thirdperson);
          DELETE_AND_NULL(DOOR(j - 1, DOWN));
        }

        // Create the exit in both directions.
        DOOR(j - 1, DOWN) = new room_direction_data;
        memset((char *) DOOR(j - 1, DOWN), 0, sizeof(struct room_direction_data));
        DOOR(j - 1, DOWN)->to_room = &world[real_room(elevator[i].floor[j].shaft_vnum)];
#ifdef USE_DEBUG_CANARIES
        DOOR(j - 1, DOWN)->canary = CANARY_VALUE;
#endif

        DOOR(j, UP) = new room_direction_data;
        memset((char *) DOOR(j, UP), 0, sizeof(struct room_direction_data));
        DOOR(j, UP)->to_room = &world[real_room(elevator[i].floor[j - 1].shaft_vnum)];
#ifdef USE_DEBUG_CANARIES
        DOOR(j, UP)->canary = CANARY_VALUE;
#endif
#undef DOOR
      }

      // Ensure an exit from car -> landing and vice/versa exists. Store the shaft's exit.
      struct room_data *car = &world[real_room(elevator[i].room)];
      struct room_data *landing = &world[real_room(elevator[i].floor[car->rating].vnum)];
      struct room_data *shaft = &world[real_room(elevator[i].floor[car->rating].shaft_vnum)];
      int dir = elevator[i].floor[car->rating].doors;

      // Ensure car->landing exit.
      make_elevator_door(real_room(landing->number), real_room(car->number), dir);

      // Ensure landing->car exit.
      make_elevator_door(real_room(car->number), real_room(landing->number), rev_dir[dir]);

      // Store shaft->landing exit.
      if (shaft->dir_option[dir])
        shaft->temporary_stored_exit[dir] = shaft->dir_option[dir];
      shaft->dir_option[dir] = NULL;

    } else
      elevator[i].floor = NULL;
  }
  fclose(fl);
}

static void open_elevator_doors(struct room_data *car, int num, int floor)
{
  int dir = elevator[num].floor[floor].doors;
  vnum_t landing_rnum = real_room(elevator[num].floor[floor].vnum);
  struct room_data *landing = &world[landing_rnum];

  if (car->dir_option[dir]) {
    REMOVE_BIT(car->dir_option[dir]->exit_info, EX_ISDOOR);
    REMOVE_BIT(car->dir_option[dir]->exit_info, EX_CLOSED);
    REMOVE_BIT(car->dir_option[dir]->exit_info, EX_LOCKED);
  } else {
    snprintf(buf, sizeof(buf), "WARNING: Invalid direction %s from car %ld (%s).",
             dirs[dir], GET_ROOM_VNUM(car), GET_ROOM_NAME(car));
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  }

  dir = rev_dir[dir];

  if (landing->dir_option[dir]) {
    REMOVE_BIT(landing->dir_option[dir]->exit_info, EX_ISDOOR);
    REMOVE_BIT(landing->dir_option[dir]->exit_info, EX_CLOSED);
    REMOVE_BIT(landing->dir_option[dir]->exit_info, EX_LOCKED);
  } else {
    snprintf(buf, sizeof(buf), "WARNING: Invalid direction %s from landing %ld (%s).",
             dirs[dir], GET_ROOM_VNUM(landing), GET_ROOM_NAME(landing));
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  }

  // Clean up.
  elevator[num].dir = UP - 1;

  snprintf(buf, sizeof(buf), "The elevator doors open to the %s.", fulldirs[dir]);
  send_to_room(buf, landing);
}

static void close_elevator_doors(struct room_data *room, int num, int floor)
{
  int dir = elevator[num].floor[floor].doors;
  vnum_t landing_rnum = real_room(elevator[num].floor[floor].vnum);
  struct room_data *landing = &world[landing_rnum];

  if (room->dir_option[dir]) {
    // Close the doors between landing and car, but do not destroy or replace anything.
    SET_BIT(room->dir_option[dir]->exit_info, EX_ISDOOR);
    SET_BIT(room->dir_option[dir]->exit_info, EX_CLOSED);
    SET_BIT(room->dir_option[dir]->exit_info, EX_LOCKED);
  } else {
    snprintf(buf, sizeof(buf), "WARNING: Invalid direction %s from car %ld (%s).",
             dirs[dir], GET_ROOM_VNUM(room), GET_ROOM_NAME(room));
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  }

  dir = rev_dir[dir];

  if (landing->dir_option[dir]) {
    SET_BIT(landing->dir_option[dir]->exit_info, EX_ISDOOR);
    SET_BIT(landing->dir_option[dir]->exit_info, EX_CLOSED);
    SET_BIT(landing->dir_option[dir]->exit_info, EX_LOCKED);
  } else {
    snprintf(buf, sizeof(buf), "WARNING: Invalid direction %s from landing %ld (%s).",
             dirs[dir], GET_ROOM_VNUM(landing), GET_ROOM_NAME(landing));
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  }

  send_to_room("The elevator doors close.\r\n", landing);
}

// ______________________________
//
// elevator lobby / call-button spec
// ______________________________

SPECIAL(call_elevator)
{
  int i = 0, j, index = -1;
  long rnum;
  if (!cmd || !ch || !ch->in_room)
    return FALSE;

  for (i = 0; i < num_elevators && index < 0; i++)
    for (j = 0; j < elevator[i].num_floors && index < 0; j++)
      if (elevator[i].floor[j].vnum == ch->in_room->number)
        index = i;

  if (CMD_IS("push") || CMD_IS("press")) {
    skip_spaces(&argument);
    if (!*argument || !(!strcasecmp("elevator", argument) ||
                        !strcasecmp("button", argument)   ||
                        !strcasecmp("call", argument)))
      send_to_char("Press what?\r\n", ch);
    else {
      if (ch->in_veh)
        return FALSE;
      if (IS_ASTRAL(ch)) {
        send_to_char("You can't do that in your current state.\r\n", ch);
        return TRUE;
      }
      if (index < 0 || elevator[index].destination) {
        send_to_char("You press the call button, but the elevator's already headed somewhere else, so nothing happens.\r\n", ch);
        return TRUE;
      }
      rnum = real_room(elevator[index].room);
      for (i = 0; i < UP; i++)
        if (world[rnum].dir_option[i] &&
            world[rnum].dir_option[i]->to_room == ch->in_room &&
            !IS_SET(world[rnum].dir_option[i]->exit_info, EX_CLOSED)) {
          send_to_char("The door is already open!\r\n", ch);
          elevator[index].destination = 0;
          return TRUE;
        }
      send_to_char("You press the call button, and the small light turns on.\r\n", ch);
      act("$n presses the call button.", FALSE, ch, 0, 0, TO_ROOM);
      elevator[index].destination = ch->in_room->number;
    }
    return TRUE;
  }

  if (CMD_IS("look") || CMD_IS("examine") || CMD_IS("read")) {
    one_argument(argument, arg);
    if (!*arg || index < 0 ||
        !(!strn_cmp("panel", arg, strlen(arg)) || !strn_cmp("elevator", arg, strlen(arg))))
      return FALSE;

    rnum = real_room(elevator[index].room);

    i = world[rnum].rating + 1 - elevator[index].num_floors - elevator[index].start_floor;
    if (i > 0)
      send_to_char(ch, "The floor indicator shows that the elevator is currently at B%d.\r\n", i);
    else if (i == 0)
      send_to_char(ch, "The floor indicator shows that the elevator is currently at the ground floor.\r\n");
    else
      send_to_char(ch, "The floor indicator shows that the elevator is currently at floor %d.\r\n", 0 - i);
    return TRUE;
  }

  return FALSE;
}

// ______________________________
//
// elevator spec
// ______________________________

SPECIAL(elevator_spec)
{
  struct room_data *room = (struct room_data *) me;

  if (cmd && process_elevator(room, ch, cmd, argument))
    return TRUE;

  return FALSE;
}

// ______________________________
//
// processing funcs
// ______________________________

int process_elevator(struct room_data *room,
                     struct char_data *ch,
                     int cmd,
                     char *argument)
{
  int num, temp, number, floor = 0, dir;
  int base_target, dice, power, success, dam;
  struct room_data *shaft, *landing, *car;
  char floorstring[10];

  for (num = 0; num < num_elevators; num++)
    if (elevator[num].room == room->number)
      break;

  if (num >= num_elevators)
    return 0;

  if (!cmd)
  {
    if (elevator[num].destination && elevator[num].dir == -1) {
      // Iterate through elevator floors, looking for the destination floor.
      for (temp = 0; temp < elevator[num].num_floors; temp++)
        if (elevator[num].floor[temp].vnum == elevator[num].destination)
          floor = temp;

      // Set ascension or descension appropriately.
      if (floor >= room->rating) {
        elevator[num].dir = DOWN;
        elevator[num].time_left = floor - room->rating;
      } else if (floor < room->rating) {
        elevator[num].dir = UP;
        elevator[num].time_left = room->rating - floor;
      }

      // Clear the destination.
      elevator[num].destination = 0;
    }

    // Move the elevator one floor.
    if (elevator[num].time_left > 0) {
      if (!elevator[num].is_moving) {
        // Someone pushed the button, but the elevator hasn't started yet. Make it move.
        if (IS_SET(room->dir_option[elevator[num].floor[room->rating].doors]->exit_info, EX_CLOSED))
          snprintf(buf, sizeof(buf), "The elevator begins to %s.\r\n", (elevator[num].dir == UP ? "ascend" : "descend"));
        else {
          snprintf(buf, sizeof(buf), "The elevator doors close and it begins to %s.\r\n", (elevator[num].dir == UP ? "ascend" : "descend"));
          close_elevator_doors(room, num, room->rating);
        }
        send_to_room(buf, &world[real_room(room->number)]);

        // Message the shaft.
        send_to_room("The elevator car shudders and begins to move.\r\n", &world[real_room(elevator[num].floor[room->rating].shaft_vnum)]);

        // Notify the next shaft room that they're about to get a car in the face.
        snprintf(buf, sizeof(buf), "The shaft rumbles ominously as the elevator car %s you begins to accelerate towards you.\r\n",
                elevator[num].dir == DOWN ? "above" : "below");
        send_to_room(buf, &world[real_room(elevator[num].floor[room->rating + (elevator[num].dir == DOWN ? 1 : -1)].shaft_vnum)]);

        // Notify all other shaft rooms about the elevator starting to move.
        for (int i = 0; i < elevator[num].num_floors; i++) {
          if (i == room->rating || i == room->rating + (elevator[num].dir == DOWN ? 1 : -1))
            continue;

          send_to_room("The shaft rumbles ominously as an elevator car begins to move.\r\n", &world[real_room(elevator[num].floor[i].shaft_vnum)]);
        }

        elevator[num].is_moving = TRUE;
        return TRUE;
      }

      elevator[num].time_left--;

      // Message for moving the car out of this part of the shaft.
      car = room;
      shaft = &world[real_room(elevator[num].floor[room->rating].shaft_vnum)];
      landing = &world[real_room(elevator[num].floor[room->rating].vnum)];
      dir = elevator[num].floor[room->rating].doors;

      snprintf(buf, sizeof(buf), "The elevator car %s swiftly away from you.\r\n", elevator[num].dir == DOWN ? "descends" : "ascends");
      send_to_room(buf, &world[real_room(shaft->number)]);
      /* If you fail an athletics test, you get dragged by the car, sustaining impact damage and getting pulled along with the elevator. */
      for (struct char_data *vict = shaft->people; vict; vict = vict->next_in_room) {
        // Nohassle imms and astral projections are immune to this bullshit.
        if (PRF_FLAGGED(vict, PRF_NOHASSLE) || IS_ASTRAL(vict))
          continue;

        // if you pass the test, continue
        base_target = 6 + modify_target(vict);
        dice = get_skill(vict, SKILL_ATHLETICS, base_target);
        if (success_test(dice, base_target) > 0) {
          // No message on success (would be spammy)
          continue;
        }

        // Failure case: Deal damage.
        act("^R$n is caught up in the mechanism and dragged along!^n", FALSE, vict, 0, 0, TO_ROOM);
        send_to_char("^RThe elevator mechanism snags you and drags you along!^n\r\n", vict);

        char_from_room(vict);
        char_to_room(vict, &world[real_room(elevator[num].floor[(elevator[num].dir == DOWN ? room->rating + 1 : room->rating - 1)].shaft_vnum)]);

        snprintf(buf, sizeof(buf), "$n ragdolls in from %s, propelled by the bulk of the moving elevator.", elevator[num].dir == DOWN ? "above" : "below");
        act(buf, FALSE, vict, 0, 0, TO_ROOM);

        power = MIN(4, 15 - (GET_IMPACT(vict) / 2)); // Base power 15 (getting pinned and dragged by an elevator HURTS). Impact armor helps.
        success = success_test(GET_BOD(vict), MAX(2, power) + modify_target(vict));
        dam = convert_damage(stage(-success, power));

        // Check for vict's death. This continue statement is essentially no-op, but proves that I've cleared this statement.
        if (damage(vict, vict, dam, TYPE_ELEVATOR, TRUE))
          continue;
      }

      // Restore the exit shaft->landing. EDGE CASE: Will be NULL if the car started here on boot and hasn't moved.
      if (shaft->temporary_stored_exit[dir]) {
        // We assume that it was stored by the elevator moving through here, so the dir_option should be NULL and safe to overwrite.
        shaft->dir_option[dir] = shaft->temporary_stored_exit[dir];
        shaft->temporary_stored_exit[dir] = NULL;
      }
      make_elevator_door(real_room(landing->number), real_room(shaft->number), dir);

      // Restore the landing->shaft exit. Same edge case and assumptions as above.
      make_elevator_door(real_room(shaft->number), real_room(landing->number), rev_dir[dir]);

      // Remove the elevator car's exit (entirely possible that the floor we're going to has no exit in that direction).
      if (car->dir_option[dir]) {
        DELETE_ARRAY_IF_EXTANT(car->dir_option[dir]->general_description);
        DELETE_ARRAY_IF_EXTANT(car->dir_option[dir]->keyword);
        delete car->dir_option[dir];
        car->dir_option[dir] = NULL;
      }

      // Move the elevator one floor in the correct direction.
      if (elevator[num].dir == DOWN)
        room->rating++;
      else
        room->rating--;

      // Message for moving the car into this part of the shaft.
      shaft = &world[real_room(elevator[num].floor[room->rating].shaft_vnum)];
      landing = &world[real_room(elevator[num].floor[room->rating].vnum)];
      dir = elevator[num].floor[room->rating].doors;
      snprintf(buf, sizeof(buf), "A rush of air precedes the arrival of an elevator car from %s.\r\n", elevator[num].dir == DOWN ? "above" : "below");
      send_to_room(buf, &world[real_room(shaft->number)]);

      /* If you fail an athletics test, the elevator knocks you off the wall, dealing D impact damage and triggering fall. */
      for (struct char_data *vict = shaft->people; vict; vict = vict->next_in_room) {
        // Nohassle imms and astral projections are immune to this bullshit.
        if (PRF_FLAGGED(vict, PRF_NOHASSLE) || IS_ASTRAL(vict))
          continue;

        // if you pass the test, continue
        base_target = 6 + modify_target(vict);
        dice = get_skill(vict, SKILL_ATHLETICS, base_target);
        if (success_test(dice, base_target) > 0) {
          act("$n squeezes $mself against the shaft wall, avoiding the passing car.", TRUE, vict, 0, 0, TO_ROOM);
          continue;
        }

        // Failure case: Deal damage.
        act("^R$n is crushed by the bulk of the elevator!^n", FALSE, vict, 0, 0, TO_ROOM);
        send_to_char("^RThe elevator grinds you against the hoistway wall with crushing force!^n\r\n", vict);

        power = MIN(4, 15 - (GET_IMPACT(vict) / 2)); // Base power 15 (getting pinned and dragged by an elevator HURTS). Impact armor helps.
        success = success_test(GET_BOD(vict), MAX(2, power) + modify_target(vict));
        dam = convert_damage(stage(-success, power));

        // Damage them, and stop the pain if they died.
        if (damage(vict, vict, dam, TYPE_ELEVATOR, TRUE))
          continue;

        perform_fall(vict);
      }

      make_elevator_door(real_room(car->number), real_room(landing->number), rev_dir[dir]);

      make_elevator_door(real_room(landing->number), real_room(car->number), dir);

      shaft->temporary_stored_exit[dir] = shaft->dir_option[dir];
      shaft->dir_option[dir] = NULL;

      if (elevator[num].time_left > 0) {
        // Notify the next shaft room that they're about to get a car in the face.
        snprintf(buf, sizeof(buf), "A light breeze brushes by as the elevator car %s you speeds towards you.\r\n",
                elevator[num].dir == DOWN ? "above" : "below");
        send_to_room(buf, &world[real_room(elevator[num].floor[room->rating + (elevator[num].dir == DOWN ? 1 : -1)].shaft_vnum)]);
      }
    }

    // Arrive at the target floor.
    else if (elevator[num].dir == UP || elevator[num].dir == DOWN) {
      temp = room->rating + 1 - elevator[num].num_floors - elevator[num].start_floor;
      if (temp > 0)
        snprintf(buf, sizeof(buf), "The elevator stops at B%d, and the doors open to the %s.\r\n", temp,
                fulldirs[elevator[num].floor[room->rating].doors]);
      else if (temp == 0)
        snprintf(buf, sizeof(buf), "The elevator stops at the ground floor, and the doors open to the %s.\r\n",
                fulldirs[elevator[num].floor[room->rating].doors]);
      else
        snprintf(buf, sizeof(buf), "The elevator stops at floor %d, and the doors open to the %s.\r\n",
                0 - temp, fulldirs[elevator[num].floor[room->rating].doors]);
      send_to_room(buf, &world[real_room(room->number)]);
      open_elevator_doors(room, num, room->rating);
      elevator[num].is_moving = FALSE;

      // Notify this shaft room that the elevator has stopped.
      send_to_room("The cable thrums with tension as the elevator comes to a graceful halt.\r\n",
                   &world[real_room(elevator[num].floor[room->rating].shaft_vnum)]);

      // Notify all other shaft rooms that the elevator has stopped.
      for (int i = 0; i < elevator[num].num_floors; i++) {
        if (i == room->rating)
          continue;

        send_to_room("The ominous rumblings from the shaft's guiderails fade.\r\n", &world[real_room(elevator[num].floor[i].shaft_vnum)]);
      }
    } else if (elevator[num].dir > 0)
      elevator[num].dir--;
    else if (!elevator[num].dir) {
      send_to_room("The elevator doors close.\r\n", &world[real_room(room->number)]);
      close_elevator_doors(room, num, room->rating);
      elevator[num].dir = -1;
    }
  } else if (CMD_IS("push") || CMD_IS("press"))
  {
    if (IS_ASTRAL(ch)) {
      send_to_char("You can't do that in your current state.\r\n", ch);
      return TRUE;
    }
    if (!*argument) {
      send_to_char("Press which button?\r\n", ch);
      return TRUE;
    }

    if (elevator[num].dir >= UP) {
      send_to_char("The elevator has already been activated.\r\n", ch);
      return TRUE;
    }

    skip_spaces(&argument);
    if (!strncmp(argument, "open", strlen(argument))) {
      if (IS_SET(room->dir_option[elevator[num].floor[room->rating].doors]->exit_info, EX_CLOSED)) {
        snprintf(buf, sizeof(buf), "The elevator doors open to the %s.",
                fulldirs[elevator[num].floor[room->rating].doors]);
        act(buf, FALSE, ch, 0, 0, TO_ROOM);
        act(buf, FALSE, ch, 0, 0, TO_CHAR);
        open_elevator_doors(room, num, room->rating);
      } else {
        send_to_char("You jab the OPEN button a few times, then realize the doors are already open.\r\n", ch);
      }
      return TRUE;
    } else if (!strncmp(argument, "close", strlen(argument))) {
      if (!IS_SET(room->dir_option[elevator[num].floor[room->rating].doors]->exit_info, EX_CLOSED)) {
        send_to_room("The elevator doors close.\r\n", &world[real_room(room->number)]);
        close_elevator_doors(room, num, room->rating);
        elevator[num].dir = -1;
      } else {
        send_to_char("You stare blankly at the closed elevator doors in front of you and wonder what you were thinking.\r\n", ch);
      }
      return TRUE;
    }

    if (LOWER(*argument) == 'b' && (number = atoi(argument + 1)) > 0) {
      snprintf(floorstring, sizeof(floorstring), "B%d", number);
      number = elevator[num].num_floors + elevator[num].start_floor + number - 1;
    }
    else if (LOWER(*argument) == 'g' && elevator[num].start_floor <= 0) {
      strcpy(floorstring, "G");
      number = elevator[num].num_floors + elevator[num].start_floor - 1;
    } else if ((number = atoi(argument)) > 0) {
      snprintf(floorstring, sizeof(floorstring), "%d", number);
      number = elevator[num].num_floors + elevator[num].start_floor - 1 - number;
    } else
      number = -1;

    if (number < 0 || number >= elevator[num].num_floors ||
        elevator[num].floor[number].vnum < 0)
      send_to_char(ch, "'%s' isn't a button.\r\n", argument);
    else if (number == room->rating) {
      if (room->dir_option[elevator[num].floor[number].doors]) {
        temp = room->rating + 1 - elevator[num].num_floors - elevator[num].start_floor;
        if (temp > 0)
          send_to_char(ch, "You are already at B%d, so the doors to the %s slide open.\r\n",
                       temp,
                       fulldirs[elevator[num].floor[room->rating].doors]);
        else if (temp == 0)
          send_to_char(ch, "You are already at the ground floor, so the doors to the %s slide open.\r\n",
                       fulldirs[elevator[num].floor[room->rating].doors]);
        else
          send_to_char(ch, "You are already at floor %d, so the doors to the %s slide open.\r\n",
                       0 - temp,
                       fulldirs[elevator[num].floor[room->rating].doors]);
        open_elevator_doors(room, num, room->rating);
      } else {
        snprintf(buf, sizeof(buf), "The elevator doors open to the %s.",
                fulldirs[elevator[num].floor[room->rating].doors]);
        act(buf, FALSE, ch, 0, 0, TO_ROOM);
        act(buf, FALSE, ch, 0, 0, TO_CHAR);
        open_elevator_doors(room, num, room->rating);
        elevator[num].is_moving = FALSE;
      }
    } else {
      if (number > room->rating) {
        elevator[num].dir = DOWN;
        elevator[num].time_left = MAX(1, number - room->rating);
      } else {
        elevator[num].dir = UP;
        elevator[num].time_left = MAX(1, room->rating - number);
      }

      snprintf(buf, sizeof(buf), "The button for %s lights up as $n pushes it.", floorstring);
      act(buf, FALSE, ch, 0, 0, TO_ROOM);
      send_to_char(ch, "You push the button for %s, and it lights up.\r\n", floorstring);
    }
    return TRUE;
  } else if (CMD_IS("look") || CMD_IS("examine") || CMD_IS("read"))
  {
    one_argument(argument, arg);
    if (!*arg || !(!strn_cmp("panel", arg, strlen(arg)) || !strn_cmp("elevator", arg, strlen(arg))
                   || !strn_cmp("buttons", arg, strlen(arg)) || !strn_cmp("controls", arg, strlen(arg))))
      return FALSE;

    strcpy(buf, "The elevator panel displays the following buttons:\r\n");
    number = 0;
    for (floor = 0; floor < elevator[num].num_floors; floor++)
      if (elevator[num].floor[floor].vnum > -1) {
        temp = elevator[num].start_floor + floor;
        if (temp < 0)
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  B%-2d", 0 - temp);
        else if (temp == 0)
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  G ");
        else
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  %-2d", temp);
        number++;
        if (!(number % elevator[num].columns) || PRF_FLAGGED(ch, PRF_SCREENREADER))
          strcat(buf, "\r\n");
      }
    if ((number % elevator[num].columns) && !PRF_FLAGGED(ch, PRF_SCREENREADER))
      strcat(buf, "\r\n");

    // Invert the lines of the panel.
    // asdf todo-- while loop with strstr to find newline, then PREPEND to buf1 and advance the while loop ptr

    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n(OPEN)%s(CLOSE)\r\n\r\n", PRF_FLAGGED(ch, PRF_SCREENREADER) ? "\r\n" : "  ");
    temp = room->rating + 1 - elevator[num].num_floors - elevator[num].start_floor;
    if (temp > 0)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "The floor indicator shows that the "
              "elevator is currently at B%d.\r\n", temp);
    else if (temp == 0)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "The floor indicator shows that the "
              "elevator is currently at the ground floor.\r\n");
    else
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "The floor indicator shows that the "
              "elevator is currently at floor %d.\r\n", 0 - temp);
    send_to_char(buf, ch);
    return TRUE;
  } else if (CMD_IS("leave")) {
    if (!IS_ASTRAL(ch)) {
      send_to_char("You can't get to the access panel's lock from in here. Looks like you'll have to send the car elsewhere, then break into the shaft from the landing.\r\n", ch);
      return TRUE;
    } else {
      int rnum = real_room(elevator[num].floor[room->rating].shaft_vnum);
      if (rnum >= 0) {
        send_to_char(ch, "You phase out of the %selevator car.\r\n", elevator[num].is_moving ? "moving " : "");
        char_from_room(ch);
        char_to_room(ch, &world[rnum]);
        act("$n phases out through the wall.\r\n", TRUE, ch, NULL, NULL, TO_ROOM);
      } else {
        send_to_char(ch, "Something went wrong.\r\n");
        mudlog("SYSERR: Invalid elevator shaft vnum.", ch, LOG_SYSLOG, TRUE);
      }
    }
    return TRUE;
  }
  return FALSE;
}

void ElevatorProcess(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  int i, rnum;

  char empty_argument = '\0';
  for (i = 0; i < num_elevators; i++)
    if (elevator && (rnum = real_room(elevator[i].room)) > -1)
      process_elevator(&world[rnum], NULL, 0, &empty_argument);
}

// ______________________________
//
// Escalators
// ______________________________

// ______________________________
//
// spec
// ______________________________


SPECIAL(escalator)
{
  return FALSE;
}

// ______________________________
//
// processing funcs
// ______________________________

std::vector<struct room_data *> escalator_list = {};
void boot_escalators() {
  escalator_list.clear();

  for (rnum_t i = 0; i <= top_of_world; i++) {
    if (world[i].func && world[i].func == escalator) {
      // TODO: Won't this break if we renumber the world / expand the room array?
      escalator_list.push_back(&world[i]);
      log_vfprintf("- Added escalator room %s (%ld)", GET_ROOM_NAME(&world[i]), GET_ROOM_VNUM(&world[i]));
    }
  }
}

void process_single_escalator(struct room_data *escalator_room) {
  struct char_data *next, *temp;
  int dir;

  for (temp = escalator_room->people; temp; temp = next) {
    next = temp->next_in_room;
    if (GET_LASTROOM(temp) > 0 || GET_LASTROOM(temp) < -3) {
      GET_LASTROOM(temp) = -3;
    } else if (GET_LASTROOM(temp) < 0) {
      GET_LASTROOM(temp)++;
    } else {
      for (dir = NORTH; dir <= DOWN; dir++) {
        if (escalator_room->dir_option[dir] &&
            escalator_room->dir_option[dir]->to_room) {
          act("As you reach the end, you step off the escalator.",
              FALSE, temp, 0, 0, TO_CHAR);
          act("$n steps off of the escalator.", TRUE, temp, 0, 0, TO_ROOM);
          char_from_room(temp);
          GET_LASTROOM(temp) = escalator_room->number;
          char_to_room(temp, escalator_room->dir_option[dir]->to_room);
          if (temp->desc)
            look_at_room(temp, 0, 0);
          break;
        }
      }
    }
  }
}

void EscalatorProcess(void)
{
  PERF_PROF_SCOPE(pr_, __func__);

  std::for_each(escalator_list.begin(), escalator_list.end(), process_single_escalator);
}

// ______________________________
//
// Monorail
// ______________________________

// ______________________________
//
// utility funcs
// ______________________________

static void open_doors(int car, int to, int room, int from)
{
  create_linked_exit(car, to, room, from, "monorail open_doors");

  snprintf(buf, sizeof(buf), "The monorail stops and the doors open to %s.\r\n", thedirs[to]);
  send_to_room(buf, &world[car]);
  snprintf(buf, sizeof(buf), "The monorail stops and the doors open to %s.\r\n", thedirs[from]);
  send_to_room(buf, &world[room]);
}

static void close_doors(int car, int to, int room, int from)
{
  if (car <= NOWHERE || car > top_of_world) {
    snprintf(buf, sizeof(buf), "SYSERR: Nonexistent car rnum %d sent to close_doors().", car);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  } else if (!(world[car].dir_option[to])) {
    snprintf(buf, sizeof(buf), "SYSERR: There is no %s exit from car %ld, close_doors() would have crashed.", dirs[to], GET_ROOM_VNUM(&world[car]));
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  } else {
    delete_exit(car, to, "close_doors p1");
    send_to_room("The monorail doors close and it begins accelerating.\r\n", &world[car]);
  }

  if (room <= NOWHERE || room > top_of_world) {
    snprintf(buf, sizeof(buf), "SYSERR: Nonexistent room rnum %d sent to close_doors().", room);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  } else if (!(world[room].dir_option[from])) {
    snprintf(buf, sizeof(buf), "SYSERR: There is no %s exit from room %ld, close_doors() would have crashed.", dirs[from], GET_ROOM_VNUM(&world[room]));
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  } else {
    delete_exit(room, from, "close_doors p2");
    send_to_room("The monorail doors close and it begins accelerating.\r\n", &world[room]);
  }
}

// ______________________________
//
// processing funcs
// ______________________________

void process_seatac_monorail(void)
{
  int carnum, roomnum, ind;
  static int where = 0;

  if (where >= 24)
    where = 0;

  ind = (int)(where / 4);

  carnum = real_room(seatac[ind].transport);
  roomnum = real_room(seatac[ind].room);

  switch (where) {
  case 0:
  case 4:
  case 8:
  case 12:
  case 16:
  case 20:
    send_to_room("Lights flash along the runway as the monorail approaches.\r\n",
                 &world[roomnum]);
    break;
  case 1:
  case 5:
  case 9:
  case 13:
  case 17:
  case 21:
    open_doors(carnum, seatac[ind].to, roomnum, seatac[ind].from);
    break;
  case 2:
  case 6:
  case 10:
  case 14:
  case 18:
  case 22:
    close_doors(carnum, seatac[ind].to, roomnum, seatac[ind].from);
    break;
  case 3:
    send_to_room("A voice announces, \"Next stop: Knight Center.\"\r\n", &world[carnum]);
    break;
  case 7:
    send_to_room("A voice announces, \"Next stop: Auburn.\"\r\n", &world[carnum]);
    break;
  case 11:
    send_to_room("A voice announces, \"Next stop: Seattle. Change here for Downtown Shuttle.\"\r\n", &world[carnum]);
    break;
  case 15:
    send_to_room("A voice announces, \"Next stop: Auburn.\"\r\n", &world[carnum]);
    break;
  case 19:
    send_to_room("A voice announces, \"Next stop: Knight Center.\"\r\n", &world[carnum]);
    break;
  case 23:
    send_to_room("A voice announces, \"Next stop: West Tacoma.\"\r\n", &world[carnum]);
    break;
  }

  where++;
}

// ______________________________
//
// Ferries and busses
// ______________________________

/* Seattle/Tacoma Ferry */

struct transport_type seattle_ferry[2] =
  {
    {
      12620, SOUTH, 12613, NORTH
    },
    {
      12620, SOUTHEAST, 2007, NORTHWEST
    },
  };

void extend_walkway_st(int ferry, int to, int room, int from)
{
  assert(ferry > 0);
  assert(room > 0);

  create_linked_exit(ferry, to, room, from, "extend_walkway_st");

  send_to_room("The Seattle-Tacoma ferry docks at the pier, and extends its walkway.\r\n", &world[room]);
  send_to_room("The ferry docks at the pier, and extends its walkway.\r\n", &world[ferry]);
}
void contract_walkway_st(int ferry, int to, int room, int from)
{
  assert(ferry > 0);
  assert(room > 0);

  delete_linked_exit(ferry, to, room, from, "contract_walkway_st");

  send_to_room("The walkway recedes, and the Seattle-Tacoma ferry departs.\r\n", &world[room]);
  send_to_room("The walkway recedes, and the ferry departs.\r\n", &world[ferry]);
}

void process_seattle_ferry(void)
{
  static int where = 0;
  int ferry, dock, ind;

  if (where >= 30)
    where = 0;

  ind = (where >= 15 ? 1 : 0);

  ferry = real_room(seattle_ferry[ind].transport);
  dock = real_room(seattle_ferry[ind].room);

  switch (where) {
  case 0:
    send_to_room("The Seattle-Tacoma ferry approaches, gliding across the bay towards "
                 "the dock.\r\n", &world[dock]);
    break;
  case 1:
    extend_walkway_st(ferry, seattle_ferry[ind].to, dock, seattle_ferry[ind].from);
    break;
  case 6:
    contract_walkway_st(ferry, seattle_ferry[ind].to, dock, seattle_ferry[ind].from);
    break;
  case 7:
    send_to_room("A voice announces through a rusting speaker, "
                 "\"Next stop: Tacoma.\"\r\n", &world[ferry]);
    break;
  case 15:
    send_to_room("The Seattle-Tacoma ferry approaches, gliding across the bay towards "
                 "the dock.\r\n", &world[dock]);
    break;
  case 16:
    extend_walkway_st(ferry, seattle_ferry[ind].to, dock, seattle_ferry[ind].from);
    break;
  case 21:
    contract_walkway_st(ferry, seattle_ferry[ind].to, dock, seattle_ferry[ind].from);
    break;
  case 23:
    send_to_room("A voice announces through a rusting speaker, "
                 "\"Next stop: Seattle.\"\r\n", &world[ferry]);
    break;
  }

  where++;
}

/* Hellhound Bus Spec
 * Bus from Seattle to Portland and back
 */



struct transport_type hellhound[2] =
  {
    {
      670, NORTHWEST, 32763, SOUTHEAST
    },
    {  670, SOUTHWEST, 14700, NORTHEAST
    },
  };

void open_busdoor(int bus, int to, int room, int from)
{
  create_linked_exit(bus, to, room, from, "open_busdoor");

  send_to_room("The bus rolls up to the platform, and the door opens.\r\n", &world[room]);
  send_to_room("The bus rolls up to the platform, and the door opens.\r\n", &world[bus]);
}

void close_busdoor(int bus, int to, int room, int from)
{
  delete_linked_exit(bus, to, room, from, "close_busdoor");

  send_to_room("The bus door shuts, the driver yells \"^Wall aboard!^n\", and begins driving.\r\n", &world[room]);
  send_to_room("The bus door shuts, the driver yells \"^Wall aboard!^n\", and begins driving.\r\n", &world[bus]);
}

void process_hellhound_bus(void)
{
  static int where = 0;
  int bus, stop, ind;

  if (where >= 56)
    where = 0;

  ind = (where >= 28 ? 1 : 0);

  bus = real_room(hellhound[ind].transport);
  stop = real_room(hellhound[ind].room);

  switch (where) {
  case 0:
    send_to_room("The bus pulls into the garage, and slowly moves to the platform.\r\n", &world[stop]);
    break;
  case 1:
    open_busdoor(bus, hellhound[ind].to, stop, hellhound[ind].from);
    break;
  case 10:
    close_busdoor(bus, hellhound[ind].to, stop, hellhound[ind].from);
    break;
  case 25:
    send_to_room("The driver shouts from the front, \"Next stop: Portland.\"\r\n", &world[bus]);
    break;
  case 28:
    send_to_room("The bus pulls into the garage, and slowly moves to the platform.\r\n", &world[stop]);
    break;
  case 30:
    open_busdoor(bus, hellhound[ind].to, stop, hellhound[ind].from);
    break;
  case 38:
    close_busdoor(bus, hellhound[ind].to, stop, hellhound[ind].from);
    break;
  case 53:
    send_to_room("The driver shouts from the front, \"Next stop: Seattle.\"\r\n", &world[bus]);
    break;
  }
  where++;
}

/* Camas-Gresham Route
 */

struct transport_type camas[2] =
  {
    { 6099, NORTH, 6013, SOUTH },
    { 6099, SOUTH, 6014, NORTH },
  };

void camas_extend(int bus, int to, int room, int from)
{
  create_linked_exit(bus, to, room, from, "camas_extend");

  send_to_room("The Lear-Cessna Platinum II smoothly lands and lays out a small stairway entrance.\r\n", &world[room]);
  send_to_room("The Lear-Cessna Platinum II smoothly lands and lays out a small stairway entrance.\r\n", &world[bus]);
}

void camas_retract(int bus, int to, int room, int from)
{
  delete_linked_exit(bus, to, room, from, "camas_retract");

  send_to_room("The stairs retract and the Lear-Cessna Platinum II taxis along the runway before taking flight.\r\n", &world[room]);
  send_to_room("The stairs retract and the Lear-Cessna Platinum II taxis along the runway before taking flight.\r\n", &world[bus]);
}

void process_camas_ferry(void)
{
  static int where = 0;
  int bus, stop, ind;

  if (where >= 168)
    where = 0;

  ind = (where >= 84 ? 1 : 0);

  bus = real_room(camas[ind].transport);
  stop = real_room(camas[ind].room);

  switch (where) {
  case 0:
    send_to_room("A Lear-Cessna Platinum II appears overhead and circles as it moves in for a landing.\r\n", &world[stop]);
    break;
  case 4:
    camas_extend(bus, camas[ind].to, stop, camas[ind].from);
    break;
  case 32:
    camas_retract(bus, camas[ind].to, stop, camas[ind].from);
    break;
  case 72:
    send_to_room("The Lear-Cessna Platinum II approaches a private Caribbean island.\r\n", &world[bus]);
    break;
  case 84:
    send_to_room("A Lear-Cessna Platinum II appears overhead and circles as it moves in for a landing.\r\n", &world[stop]);
    break;
  case 92:
    camas_extend(bus, camas[ind].to, stop, camas[ind].from);
    break;
  case 112:
    camas_retract(bus, camas[ind].to, stop, camas[ind].from);
    break;
  case 152:
    send_to_room("The Lear-Cessna Platinum II approaches the Salish-Sidhe lands.\r\n", &world[bus]);
    break;
  }
  where++;
}


//FILEPOINTER
//Downtown Portlan, 60th Street Hospital, Gresham, 60th Street back from Gresham
//Lightrail
struct transport_type lightrail[4] =
  {
    {
      14799, SOUTH, 14701, NORTH
    }
    , //Downtown Portlan
    { 14799, EAST, 14702, WEST }, //60thStreet/Hospital
    { 14799, WEST, 14711, EAST }, //Gresham
    { 14799, EAST, 14702, WEST }, //60thStreet/BackfromGresham
  };

void open_lightraildoor(int lightrail, int to, int room, int from)
{
  create_linked_exit(lightrail, to, room, from, "open_lightraildoor");

  send_to_room("The incoming lightrail grinds to a halt and its doors slide open with a hiss.\r\n", &world[room]);
  send_to_room("The lightrail grinds to a halt and the doors hiss open.\r\n", &world[lightrail]);
}

void close_lightraildoor(int lightrail, int to, int room, int from)
{
  delete_linked_exit(lightrail, to, room, from, "close_lightraildoor");

  send_to_room("The lightrail's doors slide shut and a tone emanates around the platform, signaling its departure.\r\n", &world[room]);
  send_to_room("The lightrail's doors slide shut and a tone signals as it begins moving.\r\n", &world[lightrail]);
}

void process_lightrail_train(void)
{
  static int where = 0;
  int train, stop;
  static int ind = 0;
  if (where >= 40)
    where = 0;

  train = real_room(lightrail[ind].transport);
  stop = real_room(lightrail[ind].room);

  switch (where) {
    //Downtown Stop Stuff
  case 39:
    send_to_room("An LCD Panel Flashes: \"Next Stop: Downtown Portland.\"\r\n", &world[train]);
    break;
  case 0:
    send_to_room("The lightrail emits a loud grind as it brakes into the station.\r\n", &world[stop]);
    break;
  case 1:
    open_lightraildoor(train, lightrail[ind].to, stop, lightrail[ind].from);
    break;
  case 5:
    close_lightraildoor(train, lightrail[ind].to, stop, lightrail[ind].from);
    ind = 1;
    break;
    //60th Stop Stuff (1)
  case 9:
    send_to_room("An LCD Panel Flashes: \"Next Stop: 60th Street.\"\r\n", &world[train]);
    break;
  case 10:
    send_to_room("The lightrail emits a loud grind as it brakes into the station.\r\n", &world[stop]);
    break;
  case 11:
    open_lightraildoor(train, lightrail[ind].to, stop, lightrail[ind].from);
    break;
  case 15:
    close_lightraildoor(train, lightrail[ind].to, stop, lightrail[ind].from);
    ind = 2;
    break;
    //Gresham Stop Stuff
  case 19:
    send_to_room("An LCD Panel Flashes: \"Next Stop: Gresham.\"\r\n", &world[train]);
    break;
  case 20:
    send_to_room("The lightrail emits a loud grind as it brakes into the station.\r\n", &world[stop]);
    break;
  case 21:
    open_lightraildoor(train, lightrail[ind].to, stop, lightrail[ind].from);
    break;
  case 25:
    close_lightraildoor(train, lightrail[ind].to, stop, lightrail[ind].from);
    ind = 3;
    break;
    //To60th
  case 29:
    send_to_room("An LCD Panel Flashes: \"Next Stop: 60th Street.\"\r\n", &world[train]);
    break;
  case 30:
    send_to_room("The lightrail emits a loud grind as it brakes into the station.\r\n", &world[stop]);
    break;
  case 31:
    open_lightraildoor(train, lightrail[ind].to, stop, lightrail[ind].from);
    break;
  case 35:
    close_lightraildoor(train, lightrail[ind].to, stop, lightrail[ind].from);
    ind = 0;
    break;
  }
  where++;
}


struct transport_type tacsea[2] =
  {
    {
      2099, SOUTH, 2007, NORTH
    },
    { 2099, NORTH, 14500, SOUTH }
  };

void extend_walkway(int ferry, int to, int room, int from, const char *ferry_name)
{
  assert(ferry > 0);
  assert(room > 0);

  create_linked_exit(ferry, to, room, from, "extend_walkway");

  snprintf(buf3, sizeof(buf3), "The %s ferry docks, and the walkway extends.\r\n", ferry_name);
  send_to_room(buf3, &world[room]);

  send_to_room("The ferry docks, and the walkway extends.\r\n", &world[ferry]);
}

void contract_walkway(int ferry, int to, int room, int from, const char *ferry_name)
{
  assert(ferry > 0);
  assert(room > 0);

  delete_linked_exit(ferry, to, room, from, "contract_walkway");

  snprintf(buf3, sizeof(buf3), "The walkway recedes, and the %s ferry departs.\r\n", ferry_name);
  send_to_room(buf3, &world[room]);
  send_to_room("The walkway recedes, and the ferry departs.\r\n", &world[ferry]);
}

void process_seatac_ferry(void)
{
  static int where = 0;
  int ferry, dock, ind;

  if (where >= 30)
    where = 0;

  ind = (where >= 15 ? 1 : 0);

  ferry = real_room(tacsea[ind].transport);
  dock = real_room(tacsea[ind].room);

  switch (where) {
  case 0:
    send_to_room("The Bradenton-Tacoma ferry approaches, gliding across the bay towards "
                 "the dock.\r\n", &world[dock]);
    break;
  case 1:
    extend_walkway(ferry, tacsea[ind].to, dock, tacsea[ind].from, "Bradenton-Tacoma");
    break;
  case 6:
    contract_walkway(ferry, tacsea[ind].to, dock, tacsea[ind].from, "Bradenton-Tacoma");
    break;
  case 7:
    send_to_room("A voice announces through a rusting speaker, "
                 "\"Next stop: Bradenton.\"\r\n", &world[ferry]);
    break;
  case 15:
    send_to_room("The Bradenton-Tacoma ferry approaches, gliding across the bay towards "
                 "the dock.\r\n", &world[dock]);
    break;
  case 16:
    extend_walkway(ferry, tacsea[ind].to, dock, tacsea[ind].from, "Bradenton-Tacoma");
    break;
  case 21:
    contract_walkway(ferry, tacsea[ind].to, dock, tacsea[ind].from, "Bradenton-Tacoma");
    break;
  case 22:
    send_to_room("A voice announces through a rusting speaker, "
                 "\"Next stop: Tacoma.\"\r\n", &world[ferry]);
    break;
  }

  where++;
}

struct transport_type victoria[2] =
  {
    { 62100, NORTH, 62104, SOUTH },
    { 62100, NORTH, 62276, SOUTH }
  };

void process_victoria_ferry(void)
{
  static int where = 0;
  int ferry, dock, ind;

  if (where >= 52)
    where = 0;

  ind = (where >= 26 ? 1 : 0);

  ferry = real_room(victoria[ind].transport);
  dock = real_room(victoria[ind].room);

  switch (where) {
  case 0:
    send_to_room("The Victoria-Sauteurs ferry approaches, gliding across the sea towards "
                 "the dock.\r\n", &world[dock]);
    break;
  case 1:
    extend_walkway(ferry, victoria[ind].to, dock, victoria[ind].from, "Victoria-Sauteurs");
    break;
  case 8:
    contract_walkway(ferry, victoria[ind].to, dock, victoria[ind].from, "Victoria-Sauteurs");
    break;
  case 23:
    send_to_room("The ferryman calls out, "
                 "\"Next stop: Victoria.\"\r\n", &world[ferry]);
    break;
  case 26:
    send_to_room("The Victoria-Sauteurs ferry approaches, gliding across the sea towards "
                 "the dock.\r\n", &world[dock]);
    break;
  case 28:
    extend_walkway(ferry, victoria[ind].to, dock, victoria[ind].from, "Victoria-Sauteurs");
    break;
  case 34:
    contract_walkway(ferry, victoria[ind].to, dock, victoria[ind].from, "Victoria-Sauteurs");
    break;
  case 49:
    send_to_room("The ferryman calls out, "
                 "\"Next stop: Sauteurs.\"\r\n", &world[ferry]);
    break;
  }

  where++;

}

struct transport_type sugarloaf[3] =
  {
    { 62199, EAST, 62104, WEST },
    { 62199, EAST, 62522, WEST },
    { 62199, EAST, 62523, WEST }
  };

void process_sugarloaf_ferry(void)
{
  static int where = 0;
  int ferry, dock, ind;

  if (where >= 78)
    where = 0;

  ind = (int)(where / 26);

  ferry = real_room(sugarloaf[ind].transport);
  dock = real_room(sugarloaf[ind].room);

  switch (where) {
  case 0:
  case 26:
  case 52:
    send_to_room("The island ferry approaches, gliding across the sea towards "
                 "the dock.\r\n", &world[dock]);
    break;
  case 1:
  case 28:
  case 54:
    extend_walkway(ferry, sugarloaf[ind].to, dock, sugarloaf[ind].from, "Sugar Loaf Island - Green Island - Sauteurs");
    break;
  case 8:
  case 34:
  case 60:
    contract_walkway(ferry, sugarloaf[ind].to, dock, sugarloaf[ind].from, "Sugar Loaf Island - Green Island - Sauteurs");
    break;
  case 23:
    send_to_room("The ferryman calls out, "
                 "\"Next stop: Sugar Loaf Island.\"\r\n", &world[ferry]);
    break;
  case 49:
    send_to_room("The ferryman calls out, "
                 "\"Next stop: Green Island.\"\r\n", &world[ferry]);
    break;
  case 75:
    send_to_room("The ferryman calls out, "
                 "\"Next stop: Sauteurs.\"\r\n", &world[ferry]);
    break;

  }

  where++;

}

struct transport_type grenada[2] =
  {
    { 62198, NORTH, 62167, SOUTH },
    { 62198, SOUTH, 62197, NORTH },
  };

void grenada_extend(int bus, int to, int room, int from)
{
  create_linked_exit(bus, to, room, from, "grenada_extend");

  send_to_room("The Hawker-Ridley HS-895 Skytruck docks with the platform and begins loading passengers.\r\n", &world[room]);
  send_to_room("The Hawker-Ridley HS-895 Skytruck docks with the platform and begins loading passengers.\r\n", &world[bus]);
}

void grenada_retract(int bus, int to, int room, int from)
{
  delete_linked_exit(bus, to, room, from, "grenada_retract");

  send_to_room("The airplane taxis into position on the runway before throttling up and taking off.\r\n", &world[room]);
  send_to_room("The airplane taxis into position on the runway before throttling up and taking off.\r\n", &world[bus]);
}

void process_grenada_plane(void)
{
  static int where = 0;
  int bus, stop, ind;

  if (where >= 168)
    where = 0;

  ind = (where >= 84 ? 1 : 0);

  bus = real_room(grenada[ind].transport);
  stop = real_room(grenada[ind].room);

  switch (where) {
  case 0:
  case 84:
    send_to_room("A Hawker-Ridley HS-895 Skytruck lands on the main runway and moves towards the departure gate.\r\n", &world[stop]);
    break;
  case 4:
  case 92:
    grenada_extend(bus, grenada[ind].to, stop, grenada[ind].from);
    break;
  case 32:
  case 112:
    grenada_retract(bus, grenada[ind].to, stop, grenada[ind].from);
    break;
  case 72:
    send_to_room("The voice of the pilot speaks over the intercom, "
                 "\"We'll be landing in Grenada shortly ladies and gentlemen.\"\r\n", &world[bus]);
    break;
  case 152:
    send_to_room("The voice of the pilot speaks over the intercom, "
                 "\"We'll be landing in Everett shortly ladies and gentlemen.\"\r\n", &world[bus]);
    break;
  }
  where++;
}

// ______________________________
//
// external interface funcs
// ______________________________

void TransportInit()
{
  init_elevators();
}

/* Also known as the 'cause a segfault randomly' function.
   This would work if the rooms being referenced actually existed. */
void MonorailProcess(void)
{
  process_seatac_monorail();
  process_seattle_ferry();
  process_seatac_ferry();
  process_hellhound_bus();
  process_lightrail_train();
  process_camas_ferry();
  process_grenada_plane();
  process_victoria_ferry();
  process_sugarloaf_ferry();
}

bool room_is_a_taxicab(vnum_t vnum) {
  return ((vnum >= FIRST_SEATTLE_CAB && vnum <= LAST_SEATTLE_CAB)
          || (vnum >= FIRST_PORTLAND_CAB && vnum <= LAST_PORTLAND_CAB)
          || (vnum >= FIRST_CARIBBEAN_CAB && vnum <= LAST_CARIBBEAN_CAB));
}

bool cab_jurisdiction_matches_destination(vnum_t cab_vnum, vnum_t dest_vnum) {
  int cab_jurisdiction;

  if (cab_vnum >= FIRST_SEATTLE_CAB && cab_vnum <= LAST_SEATTLE_CAB) {
    cab_jurisdiction = JURISDICTION_SEATTLE;
  }

  else if (cab_vnum >= FIRST_PORTLAND_CAB && cab_vnum <= LAST_PORTLAND_CAB) {
    cab_jurisdiction = JURISDICTION_PORTLAND;
  }

  else if (cab_vnum >= FIRST_CARIBBEAN_CAB && cab_vnum <= LAST_CARIBBEAN_CAB) {
    cab_jurisdiction = JURISDICTION_CARIBBEAN;
  }

  else {
    mudlog("SYSERR: Unrecognized cab vnum in cab_jurisdiction_matches_destination()! Update the function.", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  int dest_zone_idx = get_zone_index_number_from_vnum(dest_vnum);

  return cab_jurisdiction == zone_table[dest_zone_idx].jurisdiction;
}
