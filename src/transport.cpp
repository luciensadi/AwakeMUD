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

#if defined(WIN32) && !defined(__CYGWIN__)
#define strcasecmp(x, y) _stricmp(x,y)
#else
#endif

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "transport.h"
#include "utils.h"
#include "constants.h"

SPECIAL(call_elevator);
SPECIAL(elevator_spec);
extern int find_first_step(vnum_t src, vnum_t target);
// ----------------------------------------------------------------------------

// ______________________________
//
// static vars
// ______________________________

static struct elevator_data *elevator = NULL;
static int num_elevators = 0;

// static const int NUM_SEATTLE_STATIONS = 6;
static const int NUM_SEATAC_STATIONS = 6;

static struct dest_data destinations[] =
  {
    { "tacoma", "Tacoma", 2000 },
    { "knight", "Knight Center", 1519 },
    { "afterlife", "The Afterlife", 2072 },
    { "chinatown", "ChinaTown", 2527 },
    { "evergreen", "Evergreen Complex", 2201 },
    { "twe", "Tacoma Weapons Emporium", 2514 },
    { "nybbles", "Nybbles and Bytes", 2057 },
    { "biohyde", "Biohyde Complex", 4201 },
    { "puyallup", "Central Puyallup", 2457 },
    { "park", "Seattle State Park", 4000 },
    { "seattle", "Seattle", 32679 },
    { "cinema", "Le Cinema Vieux", 32588 },
    { "vieux", "Le Cinema Vieux", 32588 },
    { "charne", "La Charne", 30548 },
    { "tarislar", "Tarislar", 4965 },
    { "wyvern", "The Blue Wyvern", 4909 },
    { "redmond", "Redmond", 14310 },
    { "reaper", "The Reaper", 32517 },
    { "platinum", "Platinum Club", 32685 },
    { "penumbra", "Club Penumbra", 32587 },
    { "big", "The Big Rhino", 32635 },
    { "epicenter", "The Epicenter", 2418 },
    { "yoshi", "Yoshi's Sushi Bar", 32751 },
    { "airport", "Seattle-Tacoma Airport", 19410 },
    { "aztech",  "Aztechnology Pyramid", 30539 },
    { "garage", "Seattle Parking Garage", 32720 },
    { "formal", "Seattle Formal Wear", 32746 },
    { "sda", "The SDA Plaza", 30610 },
    { "dante", "Dante's Inferno", 32661 },
    { "quinns", "Quinn's", 32521 },
    { "shintaru", "Shintaru", 32513 },
    { "docks", "Seattle Dockyards", 32500 },
    { "modern", "Modern Magik", 30514 },
    { "methodist", "Seattle First Methodist", 30565 },
    { "sculleys", "Sculleys", 30591 },
    { "council", "Council Island", 9149 },
    { "moonlight", "Moonlight Mall", 14373 },
    { "library", "Seattle Public Library", 30600 },
    { "kristins", "Kristin's", 30578 },
    { "mitsuhama", "Mitsuhama Towers", 32722 },
    { "chiba", "Little Chiba", 32686 },
    { "stop", "The Stop Gap", 32708 },
    { "apartment", "Redmond Apartment Block", 14337 },
    { "humana", "Humana Hospital", 2536 },
    { "hospital", "Council Island Hospital", 9161 },
    { "coffin", "Tacoma Coffin Hotel", 2045 },
    { "diver", "Easy Diver Coffin Hotel", 32644 },
    { "action", "Action Computers", 32650 },
    { "auburn", "Auburn", 29011 },
    { "hellhound", "Hellhound Bus Stop",  32501 },
    { "lysleul", "Lysleul Plaza", 30551 },
    { "microdeck", "Auburn Microdeck Outlet", 29010 },
    { "novatech", "Novatech Seattle", 32541 },
    { "syberspace", "SyberSpace", 30612 },
    { "pooks", "Pook's Place", 1530 },
    { "errant", "KE Training Facility", 30127 },
    { "italiano", "The Italiano", 30149 },
    { "bank", "UCASBank", 30524 },
    { "matchsticks", "Matchsticks", 30579 },
    { "homewood", "Homewood Suites", 30512 },
    { "mall", "Tacoma Mall", 2093 },
    { "racespec", "Racespec Performance", 30573 },
    { "smiths", "Smith's Pub", 32566 },
    { "marksman", "Marksman Indoor Firing Range", 32682 },
    { "sapphire", "The Star Sapphire", 70301 }, 
    { "everett", "Everett", 39263 },
    { "beacon", "Beacon Mall Everett", 39253 },
    { "touristville", "Touristville", 25313 },
    { "skeleton", "The Skeleton", 25308 },
    { "\n", "", 0 } // this MUST be last
  };

static struct dest_data port_destinations[] =
  {
    { "hellhound", "Hellhound Bus Station", 14624 },
    { "postal", "Royal Postal Station", 14629 },
    { "trans-oregon", "Trans-Oregon Trucking", 14604 },
    { "thompson", "Thompson Autogroup",  14602},
    { "hard", "Hard Times", 14608 },
    { "bank", "TT-Bank", 14610 },
    { "sixth", "Sixth Street Parking", 14680 },
    { "satyricon", "The Satyricon", 14611 },
    { "rocco", "Rocco's Pizza", 14730 },
    { "annabel", "Annabel's Antiquities", 2710 },
    { "square", "O'Bryant Square", 2708 },
    { "parking", "Portland City Parking", 2703 },
    { "mary", "Mary's Bar", 14712 },
    { "tower", "TTBank Tower", 14743 },
    { "city", "City Center Parking", 14751 },
    { "habitat", "Telestrian Habitat", 18800 },
    { "hospital", "Royal Hospital", 14707 },
    { "toadstool", "The Toadstool", 14671 },
    { "powells", "Powell's Technical Books", 14724 },
    { "powell's", "Powell's Technical Books", 14724 },
    { "davis", "Davis Street Offices", 14667 },
    { "max", "Downtown MAX Station", 20800 },
    { "\n", "", 0 } // this MUST be last
 
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
static int process_elevator(struct room_data *room,
                            struct char_data *ch,
                            int cmd,
                            char *argument);

// ____________________________________________________________________________
//
// Taxi
// ____________________________________________________________________________

// ______________________________
//
// utility funcs
// ______________________________

void open_taxi_door(vnum_t room, int dir, int taxi)
{
  EQUALS_NEW(world[room].dir_option[dir], room_direction_data);
  memset((char *) world[room].dir_option[dir], 0,
         sizeof (struct room_direction_data));
  world[room].dir_option[dir]->to_room = taxi;
  world[room].dir_option[dir]->barrier = 8;
  world[room].dir_option[dir]->condition = 8;
  world[room].dir_option[dir]->material = 8;

  dir = rev_dir[dir];

  EQUALS_NEW(world[taxi].dir_option[dir], room_direction_data);
  memset((char *) world[taxi].dir_option[dir], 0,
         sizeof (struct room_direction_data));
  world[taxi].dir_option[dir]->to_room = room;
  world[taxi].dir_option[dir]->barrier = 8;
  world[taxi].dir_option[dir]->condition = 8;
  world[taxi].dir_option[dir]->material = 8;
}

void close_taxi_door(vnum_t room, int dir, int taxi)
{
  if (world[room].dir_option[dir]->keyword)
    delete [] world[room].dir_option[dir]->keyword;
  if (world[room].dir_option[dir]->general_description)
    delete [] world[room].dir_option[dir]->general_description;
  delete world[room].dir_option[dir];
  world[room].dir_option[dir] = NULL;

  dir = rev_dir[dir];

  if (world[taxi].dir_option[dir]->keyword)
    delete [] world[taxi].dir_option[dir]->keyword;
  if (world[taxi].dir_option[dir]->general_description)
    delete [] world[taxi].dir_option[dir]->general_description;
  delete world[taxi].dir_option[dir];
  world[taxi].dir_option[dir] = NULL;
}

void taxi_leaves(void)
{
  int i, j, found = 0, to;
  struct char_data *temp;
  for (j = real_room(FIRST_CAB); j <= real_room(LAST_PORTCAB); j++) {
    found = 0;
    for (temp = world[j].people; temp; temp = temp->next_in_room)
      if (!(GET_MOB_SPEC(temp) && GET_MOB_SPEC(temp) == taxi)) {
        found = 1;
        break;
      }
    if (found)
      continue;
    for (i = NORTH; i < UP; i++)
      if (world[j].dir_option[i]) {
        to = world[j].dir_option[i]->to_room;
        close_taxi_door(to, rev_dir[i], j);
        if (world[to].people) {
          if (j >= real_room(FIRST_PORTCAB))
            sprintf(buf, "The taxi doors slide shut and it pulls off from the kerb.");
          else sprintf(buf, "The taxi door slams shut as its wheels churn up a cloud of smoke.");
          act(buf, FALSE, world[to].people, 0, 0, TO_ROOM);
          act(buf, FALSE, world[to].people, 0, 0, TO_CHAR);
        }
        if (world[j].people)
          act("The door automatically closes.",
              FALSE, world[j].people, 0, 0, TO_CHAR);
      }
    if (j == LAST_CAB)
      j = FIRST_PORTCAB - 1;
  }
}

// ______________________________
//
// hail command
// ______________________________

ACMD(do_hail)
{
  struct char_data *temp;
  int cab, dir, first, last, i = -1;
  bool found = FALSE, empty = FALSE, portland = FALSE;
  SPECIAL(taxi);

  for (dir = NORTH; dir < UP; dir++)
    if (!world[ch->in_room].dir_option[dir])
      empty = TRUE;

  if (IS_ASTRAL(ch)) {
    send_to_char("Magically active cab drivers...now I've heard everything...\n\r",ch);
    return;
  }

  if (world[ch->in_room].sector_type != SPIRIT_CITY || !empty ||
      ROOM_FLAGGED(ch->in_room, ROOM_INDOORS)) {
    send_to_char("There doesn't seem to be any cabs in the area.\r\n", ch);
    return;
  }

  if ( (i = IN_ROOM(ch)) > -1 ) {
    switch (zone_table[world[i].zone].number) {
    case 13:
    case 15:
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
      break;
    case 27:
    case 146:
    case 188:
    case 208:
    case 241:
      portland = TRUE;
      break;
    default:
      /* Cab doesn't service the area */
      send_to_char("There don't seem to be any cabs in the area.\n\r",ch);
      return;
    }
  }

  if (AFF_FLAGS(ch).AreAnySet(AFF_SPELLINVIS, AFF_INVISIBLE, AFF_SPELLIMPINVIS, AFF_IMP_INVIS, ENDBIT))  {
    send_to_char("A cab almost stops, but guns it at the last second, splashing you...\n\r",ch);
    return;
  }

  first = real_room(portland ? FIRST_PORTCAB : FIRST_CAB);
  last = real_room(portland ? LAST_PORTCAB : LAST_CAB);

  for (int tries = 0; tries < 10; tries++) {
    cab = number(first, last);
    for (temp = world[cab].people; temp; temp = temp->next_in_room)
      if (!(GET_MOB_SPEC(temp) && GET_MOB_SPEC(temp) == taxi))
        break;
    if (!temp) {
      found = TRUE;
      for (dir = NORTH; dir < UP; dir++)
        if (world[cab].dir_option[dir])
          found = FALSE;
      if (found)
        break;
    }
  }

  if (!found) {
    send_to_char("Hail as you might, no cab answers.\r\n", ch);
    return;
  }

  for (dir = number(NORTH, UP - 1);; dir = number(NORTH, UP - 1))
    if (!world[ch->in_room].dir_option[dir]) {
      open_taxi_door(ch->in_room, dir, cab);
      if (portland)
      sprintf(buf, "A nice looking red and white cab pulls up smoothly to the kerb, "
              "and its door opens to the %s.", fulldirs[dir]);
      else sprintf(buf, "A beat-up yellow cab screeches to a halt, "
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

SPECIAL(taxi)
{
  extern bool memory(struct char_data *ch, struct char_data *vict);
  ACMD_CONST(do_say);
  ACMD(do_action);

  struct char_data *temp = NULL, *driver = (struct char_data *) me;
  int comm = CMD_NONE, i = 0, j;
  char say[MAX_STRING_LENGTH];
  vnum_t dest = 0;
  bool portland = FALSE;
  if (GET_MOB_VNUM(driver) == 650)
    portland = TRUE;
  if (!cmd) {
    for (temp = world[driver->in_room].people; temp; temp = temp->next_in_room)
      if (temp != driver && memory(driver, temp))
        break;
    if (!temp) {
      GET_SPARE1(driver) = 0;
      GET_SPARE2(driver) = 0;
      GET_ACTIVE(driver) = ACT_AWAIT_CMD;
    } else
      switch (GET_ACTIVE(driver)) {
      case ACT_REPLY_DEST:
        if (portland)
          sprintf(say, "%s?  Sure, that will be %d nuyen.",
                  port_destinations[GET_SPARE2(driver)].str, (int)GET_SPARE1(driver));
        else sprintf(say, "%s?  Yeah, sure...it'll cost ya %d nuyen, whaddya say?",
                  destinations[GET_SPARE2(driver)].str, (int)GET_SPARE1(driver));
        do_say(driver, say, 0, 0);
        if (GET_EXTRA(driver) == 1) {
          do_say(driver, "But seeing as you're new around here, I'll waive my usual fee, okay?", 0, 0);
          GET_SPARE1(driver) = 0;
        }
        GET_EXTRA(driver) = 0;
        GET_ACTIVE(driver) = ACT_AWAIT_YESNO;
        break;
      case ACT_REPLY_NOTOK:
        if (portland)
          do_say(driver, "You don't seem to have enough nuyen!", 0, 0);
        else do_say(driver, "Ya ain't got the nuyen!", 0, 0);
        forget(driver, temp);
        GET_SPARE1(driver) = 0;
        GET_SPARE2(driver) = 0;
        GET_ACTIVE(driver) = ACT_AWAIT_CMD;
        break;
      case ACT_REPLY_TOOBAD:
        if (portland)
          do_say(driver, "Somewhere else then, chummer?", 0, 0);
        else do_say(driver, "Whatever, chum.", 0, 0);
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
          for (j = number(NORTH, NORTHWEST);; j = number(NORTH, NORTHWEST)) {
            if (!x[j]) {
              x[j] = 1;
              y++;
            }
            if (y == NORTHWEST)
              return FALSE;
            dest = real_room(GET_SPARE2(driver));
            if (!world[dest].dir_option[j]) {
              open_taxi_door(dest, j, driver->in_room);
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
              sprintf(buf, "The door, rather noisily, slides open to the %s.",
                      fulldirs[rev_dir[j]]);
              act(buf, FALSE, driver, 0, 0, TO_ROOM);
              act(buf, FALSE, driver, 0, 0, TO_CHAR);
              break;
            }
          }
        }
        break;
      }
    return FALSE;
  }

  if (!IS_NPC(ch) && memory(driver, ch) && (CMD_IS("north") ||
      CMD_IS("east") || CMD_IS("west") || CMD_IS("south") || CMD_IS("ne") ||
      CMD_IS("se") || CMD_IS("sw") || CMD_IS("nw") || CMD_IS("northeast") ||
      CMD_IS("southeast") || CMD_IS("southwest") || CMD_IS("northwest")) && GET_ACTIVE(driver) != ACT_DRIVING) {
    forget(driver, ch);
    return FALSE;
  }

  if (!CAN_SEE(driver, ch) || IS_NPC(ch) ||
      (GET_ACTIVE(driver) != ACT_AWAIT_CMD &&
       GET_ACTIVE(driver) != ACT_AWAIT_YESNO))
    return FALSE;

  skip_spaces(&argument);

  if (CMD_IS("say") || CMD_IS("'")) {
    bool found = FALSE;
    if (GET_ACTIVE(driver) == ACT_AWAIT_CMD)
      for (dest = 0; (portland ? *port_destinations[dest].keyword : *destinations[dest].keyword) != '\n'; dest++)
        if ( str_str((const char *)argument, (portland ? port_destinations[dest].keyword : destinations[dest].keyword))) {
          comm = CMD_DEST;
          found = TRUE;
          break;
        }
    if (!found) {
      if (str_str(argument, "yes") || str_str(argument, "sure") ||
          str_str(argument, "yea") || str_str(argument, "okay"))
        comm = CMD_YES;
      else if (strstr(argument, "no"))
        comm = CMD_NO;
    }
    do_say(ch, argument, 0, 0);
  } else if (CMD_IS("nod")) {
    comm = CMD_YES;
    do_action(ch, argument, cmd, 0);
  } else if (CMD_IS("shake") && !*argument) {
    comm = CMD_NO;
    do_action(ch, argument, cmd, 0);
  } else
    return FALSE;

  if (comm == CMD_DEST && !memory(driver, ch) &&
      (i = real_room(GET_LASTROOM(ch))) > -1 &&
      GET_ACTIVE(driver) == ACT_AWAIT_CMD) {
    for (i = NORTH; i < UP; i++)
      if (world[ch->in_room].dir_option[i]) {
        i = world[ch->in_room].dir_option[i]->to_room;
        break;
      }
    int dist = 0;
    while (i != -1) {
      int x = find_first_step(i, real_room((portland ? port_destinations[dest].vnum : destinations[dest].vnum)));
      if (x == -2)
        break;
      else if (x < 0) {
        i = -1;
        break;
      }
      i = world[i].dir_option[x]->to_room;
      dist++;
    }
    if (i == -1)
      GET_SPARE1(driver) = 250;
    else
      GET_SPARE1(driver) = MIN(250, 5 + dist);
    GET_SPARE2(driver) = dest;
    GET_ACTIVE(driver) = ACT_REPLY_DEST;
    if (PLR_FLAGGED(ch, PLR_NEWBIE))
      GET_EXTRA(driver) = 1;
    else GET_EXTRA(driver) = 0;
    remember(driver, ch);
  } else if (comm == CMD_YES && memory(driver, ch) &&
             GET_ACTIVE(driver) == ACT_AWAIT_YESNO) {
    if (GET_NUYEN(ch) < GET_SPARE1(driver) && !IS_SENATOR(ch)) {
      GET_ACTIVE(driver) = ACT_REPLY_NOTOK;
      return TRUE;
    }
    if (!IS_SENATOR(ch))
      GET_NUYEN(ch) -= GET_SPARE1(driver);
    GET_SPARE1(driver) = (int)(GET_SPARE1(driver) / 50);
    GET_SPARE2(driver) = portland ? port_destinations[GET_SPARE2(driver)].vnum : destinations[GET_SPARE2(driver)].vnum;
    GET_ACTIVE(driver) = ACT_DRIVING;

    for (i = NORTH; i < UP; i++)
      if (world[ch->in_room].dir_option[i]) {
        dest = world[ch->in_room].dir_option[i]->to_room;
        close_taxi_door(dest, rev_dir[i], ch->in_room);
        if (world[dest].people) {
          if (portland)
            sprintf(buf, "The taxi doors slide shut and it pulls off from the kerb.");
          else sprintf(buf, "The taxi door slams shut as its wheels churn up a cloud of smoke.");
          act(buf, FALSE, world[dest].people, 0, 0, TO_ROOM);
          act(buf, FALSE, world[dest].people, 0, 0, TO_CHAR);
        }
        act("The door shuts as the taxi begins to accelerate.",
            FALSE, ch, 0, 0, TO_ROOM);
        act("The door shuts as the taxi begins to accelerate.",
            FALSE, ch, 0, 0, TO_CHAR);
      }
  } else if (comm == CMD_NO && memory(driver, ch) &&
             GET_ACTIVE(driver) == ACT_AWAIT_YESNO)
    GET_ACTIVE(driver) = ACT_REPLY_TOOBAD;

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

static void init_elevators(void)
{
  FILE *fl;
  char line[256];
  int i, j, t[3];
  vnum_t room[1], rnum;
  if (!(fl = fopen(ELEVATOR_FILE, "r"))) {
    log("Error opening elevator file.");
    shutdown();
  }

  if (!get_line(fl, line) || sscanf(line, "%d", &num_elevators) != 1) {
    log("Error at beginning of elevator file.");
    shutdown();
  }

  elevator = new struct elevator_data[num_elevators];

  for (i = 0; i < num_elevators && !feof(fl); i++) {
    get_line(fl, line);
    if (sscanf(line, "%ld %d %d %d", room, t, t + 1, t + 2) != 4) {
      fprintf(stderr, "Format error in elevator #%d, expecting # # # #\n", i);
      shutdown();
    }
    elevator[i].room = room[0];
    if ((rnum = real_room(elevator[i].room)) > -1) {
      world[rnum].func = elevator_spec;
      world[rnum].rating = 0;
    } else
      log("Nonexistent elevator.");
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
        if (sscanf(line, "%ld %d", room, t + 1) != 2) {
          fprintf(stderr, "Format error in elevator #%d, floor #%d\n", i, j);
          shutdown();
        }
        elevator[i].floor[j].vnum = room[0];
        if ((rnum = real_room(elevator[i].floor[j].vnum)) > -1)
          world[rnum].func = call_elevator;
        else {
          elevator[i].floor[j].vnum = -1;
          log("Nonexistent elevator destination -- blocking.");
        }
        elevator[i].floor[j].doors = t[1];
      }
    } else
      elevator[i].floor = NULL;
  }
  fclose(fl);
}

static void open_elevator_doors(struct room_data *room, int num, int floor)
{
  int dir;
  long rnum;

  rnum = real_room(elevator[num].floor[floor].vnum);
  dir = elevator[num].floor[floor].doors;

  EQUALS_NEW(room->dir_option[dir], room_direction_data);
  memset((char *) room->dir_option[dir], 0, sizeof (struct room_direction_data));
  room->dir_option[dir]->to_room = rnum;
  room->dir_option[dir]->barrier = 8;
  room->dir_option[dir]->condition = 8;
  room->dir_option[dir]->material = 8;

  dir = rev_dir[dir];

  EQUALS_NEW(world[rnum].dir_option[dir], room_direction_data);
  memset((char *) world[rnum].dir_option[dir], 0, sizeof (struct room_direction_data));
  world[rnum].dir_option[dir]->to_room = real_room(room->number);
  world[rnum].dir_option[dir]->barrier = 8;
  world[rnum].dir_option[dir]->condition = 8;
  world[rnum].dir_option[dir]->material = 8;

  elevator[num].dir = UP - 1;
  if (world[rnum].people)
  {
    sprintf(buf, "The elevator doors open to the %s.", fulldirs[dir]);
    act(buf, FALSE, world[rnum].people, 0, 0, TO_ROOM);
    act(buf, FALSE, world[rnum].people, 0, 0, TO_CHAR);
  }
}

static void close_elevator_doors(struct room_data *room, int num, int floor)
{
  long rnum;
  int dir;

  dir = elevator[num].floor[floor].doors;
  rnum = room->dir_option[dir]->to_room;

  if (room->dir_option[dir]->keyword)
    delete [] room->dir_option[dir]->keyword;
  if (room->dir_option[dir]->general_description)
    delete [] room->dir_option[dir]->general_description;
  delete room->dir_option[dir];
  room->dir_option[dir] = NULL;

  dir = rev_dir[dir];

  if (world[rnum].dir_option[dir]->keyword)
    delete [] world[rnum].dir_option[dir]->keyword;
  if (world[rnum].dir_option[dir]->general_description)
    delete [] world[rnum].dir_option[dir]->general_description;
  delete world[rnum].dir_option[dir];
  world[rnum].dir_option[dir] = NULL;

  if (world[rnum].people)
  {
    act("The elevator doors close.",
        FALSE, world[rnum].people, 0, 0, TO_ROOM);
    act("The elevator doors close.",
        FALSE, world[rnum].people, 0, 0, TO_CHAR);
  }
}

// ______________________________
//
// elevator lobby / call-button spec
// ______________________________

SPECIAL(call_elevator)
{
  int i = 0, j, index = -1;
  long rnum;
  if (!cmd)
    return FALSE;

  for (i = 0; i < num_elevators && index < 0; i++)
    for (j = 0; j < elevator[i].num_floors && index < 0; j++)
      if (elevator[i].floor[j].vnum == world[ch->in_room].number)
        index = i;

  if (CMD_IS("push")) {
    skip_spaces(&argument);
    if (!*argument || !(!strcasecmp("elevator", argument) ||
                        !strcasecmp("button", argument)))
      send_to_char("Press what?\r\n", ch);
    else {
      if (ch->in_veh)
        return FALSE;
      if (IS_ASTRAL(ch)) {
        send_to_char("You can't do that in your current state.\r\n", ch);
        return TRUE;
      }
      if (index < 0 || elevator[index].destination) {
        send_to_char("You press the call button, "
                     "but nothing seems to happen.\r\n", ch);
        return TRUE;
      }
      rnum = real_room(elevator[index].room);
      for (i = 0; i < UP; i++)
        if (world[rnum].dir_option[i] &&
            world[rnum].dir_option[i]->to_room == ch->in_room) {
          send_to_char("The door is already open!\r\n", ch);
          elevator[index].destination = 0;
          return TRUE;
        }
      send_to_char("You press the call button, "
                   "and the small light turns on.\r\n", ch);
      elevator[index].destination = world[ch->in_room].number;
    }
    return TRUE;
  }

  if (CMD_IS("look")) {
    one_argument(argument, arg);
    if (!*arg || index < 0 || !(!strcasecmp("panel", arg) ||
                                !strcasecmp("elevator", arg)))
      return FALSE;

    rnum = real_room(elevator[index].room);

    i = world[rnum].rating + 1 - elevator[index].num_floors - elevator[index].start_floor;
    if (i > 0)
      send_to_char(ch, "The floor indicator shows that the elevator is "
                   "currently at B%d.\r\n", i);
    else if (i == 0)
      send_to_char(ch, "The floor indicator shows that the elevator is "
                   "currently at the ground floor.\r\n");
    else
      send_to_char(ch, "The floor indicator shows that the elevator is "
                   "current at floor %d.\r\n", 0 - i);
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

static int process_elevator(struct room_data *room,
                            struct char_data *ch,
                            int cmd,
                            char *argument)
{
  int num, temp, number, floor = 0;

  for (num = 0; num < num_elevators; num++)
    if (elevator[num].room == room->number)
      break;

  if (num >= num_elevators)
    return 0;

  if (!cmd)
  {
    if (elevator[num].destination && elevator[num].dir == -1) {
      for (temp = 0; temp <= elevator[num].num_floors; temp++)
        if (elevator[num].floor[temp].vnum == elevator[num].destination)
          floor = temp;
      if (floor >= room->rating) {
        elevator[num].dir = DOWN;
        elevator[num].time_left = floor - room->rating;
      } else if (floor < room->rating) {
        elevator[num].dir = UP;
        elevator[num].time_left = room->rating - floor;
      }
      elevator[num].destination = 0;
    }
    if (elevator[num].time_left > 0) {
      elevator[num].time_left--;
      if (elevator[num].dir == DOWN)
        room->rating++;
      else
        room->rating--;
    } else if (elevator[num].dir == UP || elevator[num].dir == DOWN) {
      temp = room->rating + 1 - elevator[num].num_floors - elevator[num].start_floor;
      if (temp > 0)
        sprintf(buf, "The elevator stops at B%d, "
                "and the doors open to the %s.", temp,
                fulldirs[elevator[num].floor[room->rating].doors]);
      else if (temp == 0)
        sprintf(buf, "The elevator stops at the ground floor, "
                "and the doors open to the %s.", fulldirs[elevator[num].floor[room->rating].doors]);
      else
        sprintf(buf, "The elevator stops at floor %d, "
                "and the doors open to the %s.", 0 - temp,
                fulldirs[elevator[num].floor[room->rating].doors]);
      if (room->people) {
        act(buf, FALSE, room->people, 0, 0, TO_ROOM);
        act(buf, FALSE, room->people, 0, 0, TO_CHAR);
      }
      open_elevator_doors(room, num, room->rating);
    } else if (elevator[num].dir > 0)
      elevator[num].dir--;
    else if (!elevator[num].dir) {
      if (room->people) {
        act("The elevator doors close.", FALSE, room->people, 0, 0, TO_ROOM);
        act("The elevator doors close.", FALSE, room->people, 0, 0, TO_CHAR);
      }
      close_elevator_doors(room, num, room->rating);
      elevator[num].dir = -1;
    }
  } else if (CMD_IS("push"))
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
    if (LOWER(*argument) == 'b' && (number = atoi(argument + 1)) > 0)
      number = elevator[num].num_floors + elevator[num].start_floor + number - 1;
    else if (LOWER(*argument) == 'g' && elevator[num].start_floor <= 0)
      number = elevator[num].num_floors + elevator[num].start_floor - 1;
    else if ((number = atoi(argument)) > 0)
      number = elevator[num].num_floors + elevator[num].start_floor - 1 - number;
    else
      number = -1;

    if (number < 0 || number >= elevator[num].num_floors ||
        elevator[num].floor[number].vnum < 0)
      send_to_char(ch, "'%s' isn't a button.\r\n", argument);
    else if (number == room->rating) {
      if (room->dir_option[elevator[num].floor[number].doors]) {
        temp = room->rating + 1 - elevator[num].num_floors - elevator[num].start_floor;
        if (temp > 0)
          send_to_char(ch, "You are already at B%d!\r\n", temp);
        else if (temp == 0)
          send_to_char(ch, "You are already at the ground floor!\r\n");
        else
          send_to_char(ch, "You are already at floor %d!\r\n", 0 - temp);
      } else {
        sprintf(buf, "The elevator doors open to the %s.",
                fulldirs[elevator[num].floor[room->rating].doors]);
        act(buf, FALSE, ch, 0, 0, TO_ROOM);
        act(buf, FALSE, ch, 0, 0, TO_CHAR);
        open_elevator_doors(room, num, room->rating);
      }
    } else {
      if (number > room->rating) {
        elevator[num].dir = DOWN;
        elevator[num].time_left = MAX(1, number - room->rating);
      } else {
        elevator[num].dir = UP;
        elevator[num].time_left = MAX(1, room->rating - number);
      }
      if (!room->dir_option[elevator[num].floor[room->rating].doors])
        sprintf(buf, "The elevator begins to %s.",
                (elevator[num].dir == UP ? "ascend" : "descend"));
      else {
        sprintf(buf, "The elevator doors close and it begins to %s.",
                (elevator[num].dir == UP ? "ascend" : "descend"));
        close_elevator_doors(room, num, room->rating);
      }
      act(buf, FALSE, ch, 0, 0, TO_ROOM);
      act(buf, FALSE, ch, 0, 0, TO_CHAR);
    }
    return TRUE;
  } else if (CMD_IS("look"))
  {
    one_argument(argument, arg);
    if (!*arg || !(!strcasecmp("panel", arg) || !strcasecmp("elevator", arg)
                   || !strcasecmp("buttons", arg)))
      return FALSE;

    strcpy(buf, "The elevator panel displays the following buttons:\r\n");
    number = 0;
    for (floor = 0; floor < elevator[num].num_floors; floor++)
      if (elevator[num].floor[floor].vnum > -1) {
        temp = elevator[num].start_floor + floor;
        if (temp < 0)
          sprintf(buf + strlen(buf), "  B%-2d", 0 - temp);
        else if (temp == 0)
          sprintf(buf + strlen(buf), "  G ");
        else
          sprintf(buf + strlen(buf), "  %-2d", temp);
        number++;
        if (!(number % elevator[num].columns))
          strcat(buf, "\r\n");
      }
    if ((number % elevator[num].columns))
      strcat(buf, "\r\n");
    temp = room->rating + 1 - elevator[num].num_floors - elevator[num].start_floor;
    if (temp > 0)
      sprintf(buf + strlen(buf), "The floor indicator shows that the "
              "elevator is currently at B%d.\r\n", temp);
    else if (temp == 0)
      sprintf(buf + strlen(buf), "The floor indicator shows that the "
              "elevator is currently at the ground floor.\r\n");
    else
      sprintf(buf + strlen(buf), "The floor indicator shows that the "
              "elevator is currently at floor %d.\r\n", 0 - temp);
    send_to_char(buf, ch);
    return TRUE;
  }
  return FALSE;
}

void ElevatorProcess(void)
{
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

void EscalatorProcess(void)
{
  int i, dir;
  struct char_data *temp, *next;

  for (i = 0; i <= top_of_world; i++)
    if (world[i].func && world[i].func == escalator)
      for (temp = world[i].people; temp; temp = next) {
        next = temp->next_in_room;
        if (GET_LASTROOM(temp) > 0 || GET_LASTROOM(temp) < -3)
          GET_LASTROOM(temp) = -3;
        else if (GET_LASTROOM(temp) < 0)
          GET_LASTROOM(temp)++;
        else
          for (dir = NORTH; dir <= DOWN; dir++)
            if (world[i].dir_option[dir] &&
                world[i].dir_option[dir]->to_room > 0) {
              act("As you reach the end, you step off the escalator.",
                  FALSE, temp, 0, 0, TO_CHAR);
              act("$n steps off of the escalator.", TRUE, temp, 0, 0, TO_ROOM);
              char_from_room(temp);
              GET_LASTROOM(temp) = world[i].number;
              char_to_room(temp, world[i].dir_option[dir]->to_room);
              if (temp->desc)
                look_at_room(temp, 0);
              break;
            }
      }
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
  if (!world[car].dir_option[to]) {
    EQUALS_NEW(world[car].dir_option[to], room_direction_data);
    memset((char *) world[car].dir_option[to], 0,
           sizeof(struct room_direction_data));
    world[car].dir_option[to]->to_room = room;
    world[car].dir_option[to]->to_room_vnum = world[room].number;
    world[car].dir_option[to]->barrier = 8;
    world[car].dir_option[to]->condition = 8;
    world[car].dir_option[to]->material = 8;
  }
  if (!world[room].dir_option[from]) {
    EQUALS_NEW(world[room].dir_option[from], room_direction_data);
    memset((char *) world[room].dir_option[from], 0,
           sizeof(struct room_direction_data));
    world[room].dir_option[from]->to_room = car;
    world[room].dir_option[from]->to_room_vnum = world[car].number;
    world[room].dir_option[from]->barrier = 8;
    world[room].dir_option[from]->condition = 8;
    world[room].dir_option[from]->material = 8;
  }
  sprintf(buf, "The monorail stops and the doors open to %s.\r\n", thedirs[to]);
  send_to_room(buf, car);
  sprintf(buf, "The monorail stops and the doors open to %s.\r\n", thedirs[from]);
  send_to_room(buf, room);
}

static void close_doors(int car, int to, int room, int from)
{
  if (world[car].dir_option[to]->keyword)
    delete [] world[car].dir_option[to]->keyword;
  if (world[car].dir_option[to]->general_description)
    delete [] world[car].dir_option[to]->general_description;
  delete world[car].dir_option[to];
  world[car].dir_option[to] = NULL;

  if (world[room].dir_option[from]->keyword)
    delete [] world[room].dir_option[from]->keyword;
  if (world[room].dir_option[from]->general_description)
    delete [] world[room].dir_option[from]->general_description;
  delete world[room].dir_option[from];
  world[room].dir_option[from] = NULL;

  send_to_room("The monorail doors close and it begins accelerating.\r\n", car);
  send_to_room("The monorail doors close and it begins accelerating.\r\n", room);
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
                 roomnum);
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
    send_to_room("A voice announces, \"Next stop: Knight Center\"\r\n", carnum);
    break;
  case 7:
    send_to_room("A voice announces, \"Next stop: Auburn\"\r\n", carnum);
    break;
  case 11:
    send_to_room("A voice announces, \"Next stop: Seattle. Change here for Downtown Shuttle\"\r\n", carnum);
    break;
  case 15:
    send_to_room("A voice announces, \"Next stop: Auburn\"\r\n", carnum);
    break;
  case 19:
    send_to_room("A voice announces, \"Next stop: Knight Center\"\r\n", carnum);
    break;
  case 23:
    send_to_room("A voice announces, \"Next stop: West Tacoma.\"\r\n", carnum);
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
  
  if (!world[ferry].dir_option[to]) {
    EQUALS_NEW(world[ferry].dir_option[to], room_direction_data);
    memset((char *) world[ferry].dir_option[to], 0,
           sizeof(struct room_direction_data));
    world[ferry].dir_option[to]->to_room = room;
    world[ferry].dir_option[to]->to_room_vnum = world[room].number;
    world[ferry].dir_option[to]->barrier = 8;
    world[ferry].dir_option[to]->condition = 8;
    world[ferry].dir_option[to]->material = 8;
  }
  if (!world[room].dir_option[from]) {
    EQUALS_NEW(world[room].dir_option[from], room_direction_data);
    memset((char *) world[room].dir_option[from], 0,
           sizeof(struct room_direction_data));
    world[room].dir_option[from]->to_room = ferry;
    world[room].dir_option[from]->to_room_vnum = world[ferry].number;
    world[room].dir_option[from]->barrier = 8;
    world[room].dir_option[from]->condition = 8;
    world[room].dir_option[from]->material = 8;
  }
  send_to_room("The ferry docks at the pier, and extends its walkway.\r\n", room);
  send_to_room("The ferry docks at the pier, and extends its walkway.\r\n", ferry);
}
void contract_walkway_st(int ferry, int to, int room, int from)
{
  assert(ferry > 0);
  assert(room > 0);

  if (world[ferry].dir_option[to]->keyword)
    delete [] world[ferry].dir_option[to]->keyword;
  if (world[ferry].dir_option[to]->general_description)
    delete [] world[ferry].dir_option[to]->general_description;
  delete world[ferry].dir_option[to];
  world[ferry].dir_option[to] = NULL;
  if (world[room].dir_option[from]->keyword)
    delete [] world[room].dir_option[from]->keyword;
  if (world[room].dir_option[from]->general_description)
    delete [] world[room].dir_option[from]->general_description;
  delete world[room].dir_option[from];
  world[room].dir_option[from] = NULL;
  send_to_room("The walkway recedes, and the ferry departs.\r\n", room);
  send_to_room("The walkway recedes, and the ferry departs.\r\n", ferry);
}

void process_seattle_ferry(void)
{
  static int where = 0;
  int ferry, dock, ind;

  if (where >= 26)
    where = 0;

  ind = (where >= 13 ? 1 : 0);

  ferry = real_room(seattle_ferry[ind].transport);
  dock = real_room(seattle_ferry[ind].room);

  switch (where) {
  case 0:
    send_to_room("The ferry approaches, gliding across the bay towards "
                 "the dock.\r\n", dock);
    break;
  case 1:
  case 14:
    extend_walkway_st(ferry, seattle_ferry[ind].to, dock, seattle_ferry[ind].from);
    break;
  case 4:
  case 17:
    contract_walkway_st(ferry, seattle_ferry[ind].to, dock, seattle_ferry[ind].from);
    break;
  case 5:
    send_to_room("A voice announces through a rusting speaker, "
                 "\"Next stop: Tacoma.\"\r\n", ferry);
    break;
  case 13:
    send_to_room("The ferry approaches, gliding across the bay towards "
                 "the dock.\r\n", dock);
    break;
  case 18:
    send_to_room("A voice announces through a rusting speaker, "
                 "\"Next stop: Seattle.\"\r\n", ferry);
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
  if (!world[bus].dir_option[to]) {
    EQUALS_NEW(world[bus].dir_option[to], room_direction_data);
    memset((char *) world[bus].dir_option[to], 0,
           sizeof(struct room_direction_data));
    world[bus].dir_option[to]->to_room = room;
    world[bus].dir_option[to]->to_room_vnum = world[room].number;
    world[bus].dir_option[to]->barrier = 8;
    world[bus].dir_option[to]->condition = 8;
    world[bus].dir_option[to]->material = 8;
  }
  if (!world[room].dir_option[from]) {
    EQUALS_NEW(world[room].dir_option[from], room_direction_data);
    memset((char *) world[room].dir_option[from], 0,
           sizeof(struct room_direction_data));
    world[room].dir_option[from]->to_room = bus;
    world[room].dir_option[from]->to_room_vnum = world[bus].number;
    world[room].dir_option[from]->barrier = 8;
    world[room].dir_option[from]->condition = 8;
    world[room].dir_option[from]->material = 8;
  }
  send_to_room("The bus rolls up to the platform, and the door opens.\r\n", room);
  send_to_room("The bus rolls up to the platform, and the door opens.\r\n", bus);
}

void close_busdoor(int bus, int to, int room, int from)
{
  if (world[bus].dir_option[to]->keyword)
    delete [] world[bus].dir_option[to]->keyword;
  if (world[bus].dir_option[to]->general_description)
    delete [] world[bus].dir_option[to]->general_description;
  delete world[bus].dir_option[to];
  world[bus].dir_option[to] = NULL;
  if (world[room].dir_option[from]->keyword)
    delete [] world[room].dir_option[from]->keyword;
  if (world[room].dir_option[from]->general_description)
    delete [] world[room].dir_option[from]->general_description;
  delete world[room].dir_option[from];
  world[room].dir_option[from] = NULL;
  send_to_room("The bus door shuts, the driver yells \"^Wall aboard!^n\", and begins driving.\r\n", room);
  send_to_room("The bus door shuts, the driver yells \"^Wall aboard!^n\", and begins driving.\r\n", bus);
}

void process_hellhound_bus(void)
{
  static int where = 0;
  int bus, stop, ind;

  if (where >= 52)
    where = 0;

  ind = (where >= 26 ? 1 : 0);

  bus = real_room(hellhound[ind].transport);
  stop = real_room(hellhound[ind].room);

  switch (where) {
  case 0:
    send_to_room("The bus pulls into the garage, and slowly moves to the platform.\r\n", stop);
    break;
  case 1:
  case 28:
    open_busdoor(bus, hellhound[ind].to, stop, hellhound[ind].from);
    break;
  case 8:
  case 34:
    close_busdoor(bus, hellhound[ind].to, stop, hellhound[ind].from);
    break;
  case 23:
    send_to_room("The driver shouts from the front, \"Next stop: Portland\"\r\n", bus);
    break;
  case 26:
    send_to_room("The bus pulls into the garage, and slowly moves to the platform.\r\n", stop);
    break;
  case 49:
    send_to_room("The driver shouts from the front, \"Next stop: Seattle\".\r\n", bus);
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
  if (!world[bus].dir_option[to]) {
    EQUALS_NEW(world[bus].dir_option[to], room_direction_data);
    memset((char *) world[bus].dir_option[to], 0,
           sizeof(struct room_direction_data));
    world[bus].dir_option[to]->to_room = room;
    world[bus].dir_option[to]->to_room_vnum = world[room].number;
    world[bus].dir_option[to]->barrier = 8;
    world[bus].dir_option[to]->condition = 8;
    world[bus].dir_option[to]->material = 8;
  }
  if (!world[room].dir_option[from]) {
    EQUALS_NEW(world[room].dir_option[from], room_direction_data);
    memset((char *) world[room].dir_option[from], 0,
           sizeof(struct room_direction_data));
    world[room].dir_option[from]->to_room = bus;
    world[room].dir_option[from]->to_room_vnum = world[bus].number;
    world[room].dir_option[from]->barrier = 8;
    world[room].dir_option[from]->condition = 8;
    world[room].dir_option[from]->material = 8;
  }
  send_to_room("The Lear-Cessna Platinum II smoothly lands and lays out a small stairway entrance.\r\n", room);
  send_to_room("The Lear-Cessna Platinum II smoothly lands and lays out a small stairway entrance.\r\n", bus);
}

void camas_retract(int bus, int to, int room, int from)
{
  if (world[bus].dir_option[to]->keyword)
    delete [] world[bus].dir_option[to]->keyword;
  if (world[bus].dir_option[to]->general_description)
    delete [] world[bus].dir_option[to]->general_description;
  delete world[bus].dir_option[to];
  world[bus].dir_option[to] = NULL;
  if (world[room].dir_option[from]->keyword)
    delete [] world[room].dir_option[from]->keyword;
  if (world[room].dir_option[from]->general_description)
    delete [] world[room].dir_option[from]->general_description;
  delete world[room].dir_option[from];
  world[room].dir_option[from] = NULL;
  send_to_room("The stairs retract and the Lear-Cessna Platinum II taxis along the runway before taking flight.\r\n", room);
  send_to_room("The stairs retract and the Lear-Cessna Platinum II taxis along the runway before taking flight.\r\n", bus);
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
  case 84:
    send_to_room("A Lear-Cessna Platinum II appears overhead and circles as it moves in for a landing.\r\n", stop);
    break;
  case 4:
  case 92:
    camas_extend(bus, camas[ind].to, stop, camas[ind].from);
    break;
  case 32:
  case 112:
    camas_retract(bus, camas[ind].to, stop, camas[ind].from);
    break;
  case 72:
    send_to_room("The Lear-Cessna Platinum II approaches a private Caribbean island.\r\n", bus);
    break;
  case 152:
    send_to_room("The Lear-Cessna Platinum II approaches the Salish-Sidhe lands.\r\n", bus);
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
  if (!world[lightrail].dir_option[to]) {
    EQUALS_NEW(world[lightrail].dir_option[to], room_direction_data);
    memset((char *) world[lightrail].dir_option[to], 0,
           sizeof(struct room_direction_data));
    world[lightrail].dir_option[to]->to_room = room;
    world[lightrail].dir_option[to]->to_room_vnum = world[room].number;
    world[lightrail].dir_option[to]->barrier = 8;
    world[lightrail].dir_option[to]->condition = 8;
    world[lightrail].dir_option[to]->material = 8;
  }
  if (!world[room].dir_option[from]) {
    EQUALS_NEW(world[room].dir_option[from], room_direction_data);
    memset((char *) world[room].dir_option[from], 0,
           sizeof(struct room_direction_data));
    world[room].dir_option[from]->to_room = lightrail;
    world[room].dir_option[from]->to_room_vnum = world[lightrail].number;
    world[room].dir_option[from]->barrier = 8;
    world[room].dir_option[from]->condition = 8;
    world[room].dir_option[from]->material = 8;
  }
  send_to_room("The incoming lightrail grinds to a halt and its doors slide open with a hiss.\r\n", room);
  send_to_room("The lightrail grinds to a halt and the doors hiss open.\r\n", lightrail);
}

void close_lightraildoor(int lightrail, int to, int room, int from)
{
  if (world[lightrail].dir_option[to]->keyword)
    delete [] world[lightrail].dir_option[to]->keyword;
  if (world[lightrail].dir_option[to]->general_description)
    delete [] world[lightrail].dir_option[to]->general_description;
  delete world[lightrail].dir_option[to];
  world[lightrail].dir_option[to] = NULL;
  if (world[room].dir_option[from]->keyword)
    delete [] world[room].dir_option[from]->keyword;
  if (world[room].dir_option[from]->general_description)
    delete [] world[room].dir_option[from]->general_description;
  delete world[room].dir_option[from];
  world[room].dir_option[from] = NULL;
  send_to_room("The lightrail's doors slide shut and a tone eminates around the platform, signaling its departure.\r\n", room);
  send_to_room("The lightrail's doors slide shut and a tone signals as it begins moving.\r\n", lightrail);
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
    send_to_room("An LCD Panel Flashes: \"Next Stop: Downtown Portland\".\r\n", train);
    break;
  case 0:
    send_to_room("The lightrail emits a loud grind as it brakes into the station.\r\n", stop);
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
    send_to_room("An LCD Panel Flashes: \"Next Stop: 60th Street\".\r\n", train);
    break;
  case 10:
    send_to_room("The lightrail emits a loud grind as it brakes into the station.\r\n", stop);
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
    send_to_room("An LCD Panel Flashes: \"Next Stop: Gresham\".\r\n", train);
    break;
  case 20:
    send_to_room("The lightrail emits a loud grind as it brakes into the station.\r\n", stop);
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
    send_to_room("An LCD Panel Flashes: \"Next Stop: 60th Street\".\r\n", train);
    break;
  case 30:
    send_to_room("The lightrail emits a loud grind as it brakes into the station.\r\n", stop);
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

void extend_walkway(int ferry, int to, int room, int from)
{
  assert(ferry > 0);
  assert(room > 0);
  
  if (!world[ferry].dir_option[to]) {
    EQUALS_NEW(world[ferry].dir_option[to], room_direction_data);
    memset((char *) world[ferry].dir_option[to], 0,
           sizeof(struct room_direction_data));
    world[ferry].dir_option[to]->to_room = room;
    world[ferry].dir_option[to]->to_room_vnum = world[room].number;
    world[ferry].dir_option[to]->barrier = 8;
    world[ferry].dir_option[to]->condition = 8;
    world[ferry].dir_option[to]->material = 8;
  }
  if (!world[room].dir_option[from]) {
    EQUALS_NEW(world[room].dir_option[from], room_direction_data);
    memset((char *) world[room].dir_option[from], 0,
           sizeof(struct room_direction_data));
    world[room].dir_option[from]->to_room = ferry;
    world[room].dir_option[from]->to_room_vnum = world[ferry].number;
    world[room].dir_option[from]->barrier = 8;
    world[room].dir_option[from]->condition = 8;
    world[room].dir_option[from]->material = 8;
  }
  send_to_room("The ferry docks, and the walkway extends.\r\n", room);
  send_to_room("The ferry docks, and the walkway extends.\r\n", ferry);
}

void contract_walkway(int ferry, int to, int room, int from)
{
  assert(ferry > 0);
  assert(room > 0);
  
  if (world[ferry].dir_option[to]->keyword)
    delete [] world[ferry].dir_option[to]->keyword;
  if (world[ferry].dir_option[to]->general_description)
    delete [] world[ferry].dir_option[to]->general_description;
  delete world[ferry].dir_option[to];
  world[ferry].dir_option[to] = NULL;
  if (world[room].dir_option[from]->keyword)
    delete [] world[room].dir_option[from]->keyword;
  if (world[room].dir_option[from]->general_description)
    delete [] world[room].dir_option[from]->general_description;
  delete world[room].dir_option[from];
  world[room].dir_option[from] = NULL;
  send_to_room("The walkway recedes, and the ferry departs.\r\n", room);
  send_to_room("The walkway recedes, and the ferry departs.\r\n", ferry);
}

void process_seatac_ferry(void)
{
  static int where = 0;
  int ferry, dock, ind;

  if (where >= 26)
    where = 0;

  ind = (where >= 13 ? 1 : 0);

  ferry = real_room(tacsea[ind].transport);
  dock = real_room(tacsea[ind].room);

  switch (where) {
  case 0:
    send_to_room("The ferry approaches, gliding across the bay towards "
                 "the dock.\r\n", dock);
    break;
  case 1:
  case 14:
    extend_walkway(ferry, tacsea[ind].to, dock, tacsea[ind].from);
    break;
  case 4:
  case 17:
    contract_walkway(ferry, tacsea[ind].to, dock, tacsea[ind].from);
    break;
  case 5:
    send_to_room("A voice announces through a rusting speaker, "
                 "\"Next stop: Bradenton.\"\r\n", ferry);
    break;
  case 13:
    send_to_room("The ferry approaches, gliding across the bay towards "
                 "the dock.\r\n", dock);
    break;
  case 18:
    send_to_room("A voice announces through a rusting speaker, "
                 "\"Next stop: Tacoma.\"\r\n", ferry);
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
  case 26:
    send_to_room("The ferry approaches, gliding across the sea towards "
                 "the dock.\r\n", dock);
    break;
  case 1:
  case 28:
    extend_walkway(ferry, victoria[ind].to, dock, victoria[ind].from);
    break;
  case 8:
  case 34:
    contract_walkway(ferry, victoria[ind].to, dock, victoria[ind].from);
    break;
  case 23:
    send_to_room("The ferryman calls out, "
                 "\"Next stop Victoria.\"\r\n", ferry);
    break;
  case 49:
    send_to_room("The ferryman calls out, "
                 "\"Next stop Sauteurs.\"\r\n", ferry);
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
    send_to_room("The ferry approaches, gliding across the sea towards "
                 "the dock.\r\n", dock);
    break;
  case 1:
  case 28:
  case 54:
    extend_walkway(ferry, sugarloaf[ind].to, dock, sugarloaf[ind].from);
    break;
  case 8:
  case 34:
  case 60:
    contract_walkway(ferry, sugarloaf[ind].to, dock, sugarloaf[ind].from);
    break;
  case 23:
    send_to_room("The ferryman calls out, "
                 "\"Next stop Sugar Loaf Island.\"\r\n", ferry);
    break;
  case 49:
    send_to_room("The ferryman calls out,, "
                 "\"Next stop Green Island.\"\r\n", ferry);
    break;
  case 75:
    send_to_room("The ferryman calls out,, "
                 "\"Next stop Sauteurs.\"\r\n", ferry);
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
  if (!world[bus].dir_option[to]) {
    EQUALS_NEW(world[bus].dir_option[to], room_direction_data);
    memset((char *) world[bus].dir_option[to], 0,
           sizeof(struct room_direction_data));
    world[bus].dir_option[to]->to_room = room;
    world[bus].dir_option[to]->to_room_vnum = world[room].number;
    world[bus].dir_option[to]->barrier = 8;
    world[bus].dir_option[to]->condition = 8;
    world[bus].dir_option[to]->material = 8;
  }
  if (!world[room].dir_option[from]) {
    EQUALS_NEW(world[room].dir_option[from], room_direction_data);
    memset((char *) world[room].dir_option[from], 0,
           sizeof(struct room_direction_data));
    world[room].dir_option[from]->to_room = bus;
    world[room].dir_option[from]->to_room_vnum = world[bus].number;
    world[room].dir_option[from]->barrier = 8;
    world[room].dir_option[from]->condition = 8;
    world[room].dir_option[from]->material = 8;
  }
  send_to_room("The Hawker-Ridley HS-895 Skytruck docks with the platform and begins loading passengers.\r\n", room);
  send_to_room("The Hawker-Ridley HS-895 Skytruck docks with the platform and begins loading passengers.\r\n", bus);
}

void grenada_retract(int bus, int to, int room, int from)
{
  if (world[bus].dir_option[to]->keyword)
    delete [] world[bus].dir_option[to]->keyword;
  if (world[bus].dir_option[to]->general_description)
    delete [] world[bus].dir_option[to]->general_description;
  delete world[bus].dir_option[to];
  world[bus].dir_option[to] = NULL;
  if (world[room].dir_option[from]->keyword)
    delete [] world[room].dir_option[from]->keyword;
  if (world[room].dir_option[from]->general_description)
    delete [] world[room].dir_option[from]->general_description;
  delete world[room].dir_option[from];
  world[room].dir_option[from] = NULL;
  send_to_room("The airplane taxis into position on the runway before throttling up and taking off.\r\n", room);
  send_to_room("The airplane taxis into position on the runway before throttling up and taking off.\r\n", bus);
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
    send_to_room("A Hawker-Ridley HS-895 Skytruck lands on the main runway and moves towards the departure gate.\r\n", stop);
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
                 "\"We'll be landing in Grenada shortly ladies and gentlemen.\"\r\n", bus);
    break;
  case 152:
    send_to_room("The voice of the pilot speaks over the intercom, "
                 "\"We'll be landing in Everett shortly ladies and gentlemen.\"\r\n", bus);
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
  //process_seatac_monorail();
  //process_seattle_ferry();
  //process_seatac_ferry();
  //process_hellhound_bus();
  //process_lightrail_train();
  //process_camas_ferry();
  //process_grenada_plane();
  //process_victoria_ferry();
  //process_sugarloaf_ferry();
}

