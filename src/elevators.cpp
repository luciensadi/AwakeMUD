#include "structs.hpp"
#include "db.hpp"
#include "transport.hpp"
#include "constants.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "vehicles.hpp"
#include "handler.hpp"

// Statics
struct elevator_data *elevator = NULL;
int num_elevators = 0;

// Externs
extern void perform_fall(struct char_data *ch);

extern int ELEVATOR_SHAFT_FALL_RATING;

// Prototypes
static void make_elevator_door(vnum_t rnum_to, vnum_t rnum_from, int direction_from);
static void open_elevator_doors(struct room_data *car, rnum_t elevator_idx, int floor, bool send_opening_message_to_car);
static void close_elevator_doors(struct room_data *car, rnum_t elevator_idx, int floor, bool send_closing_message_to_car);
static bool find_elevator(struct room_data *in_room, rnum_t &elevator_idx, rnum_t &floor_idx);
static void call_elevator(struct room_data *called_from, rnum_t elevator_idx, rnum_t floor_idx, struct char_data *ch, rnum_t car_rnum);
static int parse_elevator_floor_number(const char *argument, rnum_t elevator_idx, char *floorstring, size_t floorstring_sz);
static bool push_elevator_button(struct room_data *called_from, rnum_t elevator_idx, rnum_t floor_idx, struct char_data *ch, const char *argument);
static void move_elevator(struct room_data *car, rnum_t elevator_idx);
static void show_elevator_location_to_ch(struct char_data *ch, struct room_data *car, rnum_t elevator_idx);
static void show_elevator_panel_to_ch(struct char_data *ch, struct room_data *car, rnum_t elevator_idx);

// ______________________________
//
// elevator lobby / call-button spec
// ______________________________

ACMD(do_push_while_rigging) {
  rnum_t elevator_idx = 0, floor_idx = 0;
  
  skip_spaces(&argument);

  struct veh_data *veh;
  RIG_VEH(ch, veh);
  
  // We fail hard here because there is no other push logic set up for rigged vehicles.
  FAILURE_CASE(!veh, "You need to be in a vehicle to do that.");
  FAILURE_CASE(!get_veh_grabber(veh), "Your vehicle isn't equipped with a manipulator arm.");
  FAILURE_CASE(!veh->in_room, "Your manipulator arm can't find any elevator buttons from in here.");
  FAILURE_CASE(!find_elevator(veh->in_room, elevator_idx, floor_idx), "There's nothing here your vehicle can press.");

  push_elevator_button(veh->in_room, elevator_idx, floor_idx, ch, argument);
}

ACMD_DECLARE(do_look);
ACMD(do_look_while_rigging) {
  rnum_t elevator_idx = 0, floor_idx = 0;
  
  skip_spaces(&argument);

  struct veh_data *veh;
  RIG_VEH(ch, veh);

  // We silently fail and fall back to do_look here for almost all cases.
  if (!veh || !veh->in_room || !find_elevator(veh->in_room, elevator_idx, floor_idx)) {
    do_look(ch, argument, cmd, subcmd);
    return;
  }

  // They're looking at something that we don't care about.
  one_argument(argument, arg);
  if (!*arg || !(!strn_cmp("panel", arg, strlen(arg)) || !strn_cmp("elevator", arg, strlen(arg))
      || !strn_cmp("buttons", arg, strlen(arg)) || !strn_cmp("controls", arg, strlen(arg))))
  {
    do_look(ch, argument, cmd, subcmd);
    return;
  }

  struct room_data *car = &world[real_room(elevator[elevator_idx].room)];

  if (floor_idx == -1) {
    // In an elevator car. Show both the panel and the location.
    show_elevator_panel_to_ch(ch, car, elevator_idx);
  }

  show_elevator_location_to_ch(ch, car, elevator_idx);
}

SPECIAL(call_elevator)
{
  rnum_t elevator_idx = 0, floor_idx = 0;
  if (!cmd || !ch || !ch->in_room)
    return FALSE;

  // Fall back to our custom vehicle handler.
  if (IS_RIGGING(ch))
    return FALSE;

  bool cmd_is_push = CMD_IS("push") || CMD_IS("press");
  bool cmd_is_look = CMD_IS("look") || CMD_IS("examine") || CMD_IS("read");

  if (!cmd_is_push && !cmd_is_look)
    return FALSE;

  // Determine our elevator and floor index.
  if (!find_elevator(ch->in_room, elevator_idx, floor_idx) || elevator_idx < 0) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Elevator call button at %s (%ld) could not find associated elevator.",
                    GET_ROOM_NAME(ch->in_room),
                    GET_ROOM_VNUM(ch->in_room));
    return FALSE;
  }

  // Push button.
  if (cmd_is_push) {
    skip_spaces(&argument);
    if (!*argument || !(!strcasecmp("elevator", argument) ||
                        !strcasecmp("button", argument)   ||
                        !strcasecmp("call", argument)))
    {
      // Don't consume the command.
      return FALSE;
    }

    return push_elevator_button(ch->in_room, elevator_idx, floor_idx, ch, argument);
  }

  // Look at the panel.
  else if (cmd_is_look) {
    one_argument(argument, arg);
    if (!*arg || !(!strn_cmp("panel", arg, strlen(arg)) || !strn_cmp("elevator", arg, strlen(arg))))
      return FALSE;

    show_elevator_location_to_ch(ch, &world[real_room(elevator[elevator_idx].room)], elevator_idx);
    return TRUE;
  }

  return FALSE;
}

// ______________________________
//
// processing funcs
// ______________________________

int process_elevator(struct room_data *car,
                     struct char_data *ch,
                     int cmd,
                     char *argument)
{
  rnum_t elevator_idx, floor_idx;

  if (!find_elevator(car, elevator_idx, floor_idx))
    return FALSE;

  if (!cmd) {
    // This is a world update tick, so we move the elevator if needed.
    move_elevator(car, elevator_idx);
    return 1;
  }

  // PUSH <button>
  if (CMD_IS("push") || CMD_IS("press")) {
    skip_spaces(&argument);
    return push_elevator_button(car, elevator_idx, -1, ch, argument);
  }
  
  // LOOK PANEL
  if (CMD_IS("look") || CMD_IS("examine") || CMD_IS("read"))
  {
    one_argument(argument, arg);
    if (!*arg || !(!strn_cmp("panel", arg, strlen(arg)) || !strn_cmp("elevator", arg, strlen(arg))
                   || !strn_cmp("buttons", arg, strlen(arg)) || !strn_cmp("controls", arg, strlen(arg))))
      return FALSE;

    show_elevator_panel_to_ch(ch, car, elevator_idx);
    show_elevator_location_to_ch(ch, car, elevator_idx);
    return TRUE;
  }
  
  // LEAVE
  if (CMD_IS("leave")) {
    if (!IS_ASTRAL(ch) && !IS_SENATOR(ch)) {
      send_to_char("You can't get to the access panel's lock from in here. Looks like you'll have to send the car elsewhere, then break into the shaft from the landing.\r\n", ch);
      return TRUE;
    } else {
      int rnum = real_room(elevator[elevator_idx].floor[car->rating].shaft_vnum);
      if (rnum >= 0) {
        if (IS_SENATOR(ch)) {
          send_to_char(ch, "You manifest your bullshit staff powers and step out of the %selevator car.\r\n", elevator[elevator_idx].is_moving ? "moving " : "");
        } else {
          send_to_char(ch, "You phase out of the %selevator car.\r\n", elevator[elevator_idx].is_moving ? "moving " : "");
        }
        char_from_room(ch);
        char_to_room(ch, &world[rnum]);
        act("$n phases out through the wall.\r\n", TRUE, ch, NULL, NULL, TO_ROOM);
        if (!PRF_FLAGGED(ch, PRF_SCREENREADER))
          look_at_room(ch, 0, FALSE);
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
// elevator spec
// ______________________________

SPECIAL(elevator_spec)
{
  struct room_data *car = (struct room_data *) me;

  if (cmd && process_elevator(car, ch, cmd, argument))
    return TRUE;

  return FALSE;
}

// ______________________________
//
// utility / local funcs
// ______________________________

void try_to_enter_elevator_car(struct char_data *ch) {
  // Iterate through elevators to find one that contains this shaft.
  for (rnum_t elevator_idx = 0; elevator_idx < num_elevators; elevator_idx++) {
    int car_rating = world[real_room(elevator[elevator_idx].room)].rating;
    // Check for the car being at this floor.
    if (elevator[elevator_idx].floor[car_rating].shaft_vnum == ch->in_room->number) {
      if (IS_ASTRAL(ch)) {
        send_to_char(ch, "You phase into the %selevator car.\r\n", elevator[elevator_idx].is_moving ? "moving " : "");
        char_from_room(ch);
        char_to_room(ch, &world[real_room(elevator[elevator_idx].room)]);
        act("$n phases in through the wall.\r\n", TRUE, ch, NULL, NULL, TO_ROOM);
      } else {
        if (elevator[elevator_idx].is_moving && !IS_SENATOR(ch)) {
          send_to_char("You can't enter a moving elevator car!\r\n", ch);
          return;
        }

        send_to_char("You jimmy open the access hatch and drop into the elevator car. The hatch locks closed behind you.\r\n", ch);
        char_from_room(ch);
        char_to_room(ch, &world[real_room(elevator[elevator_idx].room)]);
        act("The access hatch in the ceiling squeaks briefly open and $n drops into the car.", FALSE, ch, NULL, NULL, TO_ROOM);
        if (!PRF_FLAGGED(ch, PRF_SCREENREADER))
          look_at_room(ch, 0, FALSE);
      }
      return;
    }
  }
}

// Pushes a call button, or pushes a panel button if you're inside.
static bool push_elevator_button(struct room_data *called_from, rnum_t elevator_idx, rnum_t floor_idx, struct char_data *ch, const char *argument) {  
  if (!ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Received call to push_elevator_button(%s, %ld, %ld, NULL, %s) with NULL ch!",
                    GET_ROOM_NAME(called_from), elevator_idx, floor_idx, argument);
    return FALSE;
  }

  if (ch->in_veh && !IS_RIGGING(ch)) {
    // Don't block people from pushing vehicles out the back.
    return FALSE;
  }

  if (elevator_idx < 0) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Got NEGATIVE elevator_idx to push_elevator_button().");
    return FALSE;
  }

  TRUE_CASE(IS_ASTRAL(ch), "Astral entities can't push buttons.");
  TRUE_CASE(!argument || !*argument, "Press which button?");

  rnum_t car_rnum = real_room(elevator[elevator_idx].room);
  TRUE_CASE(car_rnum < 0, "This elevator is broken. Alert staff.");

  struct veh_data *veh = NULL;
  RIG_VEH(ch, veh);

  if (veh && !IS_RIGGING(ch)) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Somehow got to push_elevator_button() while in a vehicle and not rigging!");
    return FALSE;
  }

  // Disallow use during combat.
  TRUE_CASE(CH_IN_COMBAT(ch) || (veh && VEH_IN_COMBAT(veh)), "You're a little busy right now!");

  if (floor_idx == -1) {
    // They're in the elevator car. Push a floor button or open/close.
    TRUE_CASE(IS_ASTRAL(ch), "You can't do that in your current state.");
    TRUE_CASE(elevator[elevator_idx].dir >= UP, "The elevator has already been activated.");

    // PUSH OPEN
    if (!strncmp(argument, "open", strlen(argument))) {
      if (IS_SET(called_from->dir_option[elevator[elevator_idx].floor[called_from->rating].doors]->exit_info, EX_CLOSED)) {
        open_elevator_doors(called_from, elevator_idx, called_from->rating, TRUE);
      } else {
        send_to_char(ch, "You jab the OPEN button%s a few times, then realize the doors are already open.\r\n", veh ? " with your manipulator" : "");
      }
      return TRUE;
    }
    
    // PUSH CLOSE
    if (!strncmp(argument, "close", strlen(argument))) {
      if (!IS_SET(called_from->dir_option[elevator[elevator_idx].floor[called_from->rating].doors]->exit_info, EX_CLOSED)) {
        close_elevator_doors(called_from, elevator_idx, called_from->rating, TRUE);
        elevator[elevator_idx].dir = -1;
      } else {
        send_to_char("You stare blankly at the closed elevator doors in front of you and wonder what you were thinking.\r\n", ch);
      }
      return TRUE;
    }

    // PUSH <floor>
    char floorstring[50];
    int target_floor = parse_elevator_floor_number(argument, elevator_idx, floorstring, sizeof(floorstring));
    
    TRUE_CASE_PRINTF(target_floor < 0
                     || target_floor >= elevator[elevator_idx].num_floors
                     || elevator[elevator_idx].floor[target_floor].vnum < 0,
                     "'%s' isn't a button.\r\n", argument);
    
    if (target_floor == called_from->rating) {
      if (called_from->dir_option[elevator[elevator_idx].floor[target_floor].doors]) {
        int temp = called_from->rating + 1 - elevator[elevator_idx].num_floors - elevator[elevator_idx].start_floor;
        if (temp > 0)
          send_to_char(ch, "You are already at B%d, so the doors to the %s slide open.\r\n",
                       temp,
                       fulldirs[elevator[elevator_idx].floor[called_from->rating].doors]);
        else if (temp == 0)
          send_to_char(ch, "You are already at the ground floor, so the doors to the %s slide open.\r\n",
                       fulldirs[elevator[elevator_idx].floor[called_from->rating].doors]);
        else
          send_to_char(ch, "You are already at floor %d, so the doors to the %s slide open.\r\n",
                       0 - temp,
                       fulldirs[elevator[elevator_idx].floor[called_from->rating].doors]);
        open_elevator_doors(called_from, elevator_idx, called_from->rating, FALSE);
      } else {
        open_elevator_doors(called_from, elevator_idx, called_from->rating, TRUE);
        elevator[elevator_idx].is_moving = FALSE;
      }
    } else {
      if (target_floor > called_from->rating) {
        elevator[elevator_idx].dir = DOWN;
        elevator[elevator_idx].time_left = MAX(1, target_floor - called_from->rating);
      } else {
        elevator[elevator_idx].dir = UP;
        elevator[elevator_idx].time_left = MAX(1, called_from->rating - target_floor);
      }

      if (veh) {
        snprintf(buf, sizeof(buf), "The button for %s lights up as %s pushes it with its manipulator.", floorstring, GET_VEH_NAME(veh));
        send_to_room(buf, veh->in_room, veh);
        send_to_char(ch, "You push the button for %s with your manipulator, and it lights up.\r\n", floorstring);
      } else {
        snprintf(buf, sizeof(buf), "The button for %s lights up as $n pushes it.", floorstring);
        act(buf, FALSE, ch, 0, 0, TO_ROOM);
        send_to_char(ch, "You push the button for %s, and it lights up.\r\n", floorstring);
      }
    }
  } else {
    // They're at an elevator stop.
    call_elevator(called_from, elevator_idx, floor_idx, ch, car_rnum);
  }
  return TRUE;
}

static void make_elevator_door(vnum_t rnum_to, vnum_t rnum_from, int direction_from) {
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

void init_elevators(void)
{
  FILE *fl;
  char line[256];
  int i, j, t[3];
  vnum_t car_vnum, floor_vnum, rnum, shaft_vnum, shaft_rnum;
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
    if (sscanf(line, "%ld %d %d %d", &car_vnum, t, t + 1, t + 2) != 4) {
      fprintf(stderr, "Format error in elevator #%d, expecting # # # #\n", i);
      shutdown();
    }
    elevator[i].room = car_vnum;
    if ((rnum = real_room(elevator[i].room)) > -1) {
      world[rnum].func = elevator_spec;
      world[rnum].rating = 0;
      if (real_panel >= 0) {
        // Add decorative elevator control panel to elevator car.
        obj = read_object(real_panel, REAL, OBJ_LOAD_REASON_SPECPROC);
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
        if (sscanf(line, "%ld %ld %d", &floor_vnum, &shaft_vnum, t + 1) != 3) {
          fprintf(stderr, "Format error in elevator #%d, floor #%d\n", i, j);
          shutdown();
        }
        elevator[i].floor[j].vnum = floor_vnum;
        elevator[i].floor[j].shaft_vnum = shaft_vnum;
        elevator[i].floor[j].doors = t[1];
        //snprintf(buf, sizeof(buf), "%s: vnum %ld, shaft %ld, doors %s.", line, floor_vnum, shaft_vnum, fulldirs[t[1]]);
        //log(buf);

        // Ensure the landing exists.
        rnum = real_room(elevator[i].floor[j].vnum);
        if (rnum > -1) {
          world[rnum].func = call_elevator;
          if (real_button >= 0) {
            // Add decorative elevator call button to landing.
            obj = read_object(real_button, REAL, OBJ_LOAD_REASON_SPECPROC);
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

static void open_elevator_doors(struct room_data *car, rnum_t elevator_idx, int floor, bool send_opening_message_to_car)
{
  int dir = elevator[elevator_idx].floor[floor].doors;
  vnum_t landing_rnum = real_room(elevator[elevator_idx].floor[floor].vnum);
  struct room_data *landing = &world[landing_rnum];
  char msg_buf[1000];

  if (car->dir_option[dir]) {
    REMOVE_BIT(car->dir_option[dir]->exit_info, EX_ISDOOR);
    REMOVE_BIT(car->dir_option[dir]->exit_info, EX_CLOSED);
    REMOVE_BIT(car->dir_option[dir]->exit_info, EX_LOCKED);
    if (send_opening_message_to_car) {
      snprintf(msg_buf, sizeof(msg_buf), "The elevator doors open to the %s.\r\n", fulldirs[dir]);
      send_to_room(msg_buf, car);
    }
  } else {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "WARNING: Invalid direction %s from car %ld (%s).", dirs[dir], GET_ROOM_VNUM(car), GET_ROOM_NAME(car));
  }

  dir = rev_dir[dir];

  if (landing->dir_option[dir]) {
    REMOVE_BIT(landing->dir_option[dir]->exit_info, EX_ISDOOR);
    REMOVE_BIT(landing->dir_option[dir]->exit_info, EX_CLOSED);
    REMOVE_BIT(landing->dir_option[dir]->exit_info, EX_LOCKED);
    snprintf(msg_buf, sizeof(msg_buf), "The elevator doors open to the %s.\r\n", fulldirs[dir]);
    send_to_room(msg_buf, landing);
  } else {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "WARNING: Invalid direction %s from landing %ld (%s).", dirs[dir], GET_ROOM_VNUM(landing), GET_ROOM_NAME(landing));
  }

  // Clean up.
  elevator[elevator_idx].dir = UP - 1;
}

static void close_elevator_doors(struct room_data *car, rnum_t elevator_idx, int floor, bool send_closing_message_to_car)
{
  int dir = elevator[elevator_idx].floor[floor].doors;
  vnum_t landing_rnum = real_room(elevator[elevator_idx].floor[floor].vnum);
  struct room_data *landing = &world[landing_rnum];

  if (car->dir_option[dir]) {
    // Close the doors between landing and car, but do not destroy or replace anything.
    SET_BIT(car->dir_option[dir]->exit_info, EX_ISDOOR);
    SET_BIT(car->dir_option[dir]->exit_info, EX_CLOSED);
    SET_BIT(car->dir_option[dir]->exit_info, EX_LOCKED);
    if (send_closing_message_to_car)
      send_to_room("The elevator doors close.\r\n", car);
  } else {
    snprintf(buf, sizeof(buf), "WARNING: Invalid direction %s from car %ld (%s).",
             dirs[dir], GET_ROOM_VNUM(car), GET_ROOM_NAME(car));
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  }

  dir = rev_dir[dir];

  if (landing->dir_option[dir]) {
    SET_BIT(landing->dir_option[dir]->exit_info, EX_ISDOOR);
    SET_BIT(landing->dir_option[dir]->exit_info, EX_CLOSED);
    SET_BIT(landing->dir_option[dir]->exit_info, EX_LOCKED);
    send_to_room("The elevator doors close.\r\n", landing);
  } else {
    snprintf(buf, sizeof(buf), "WARNING: Invalid direction %s from landing %ld (%s).",
             dirs[dir], GET_ROOM_VNUM(landing), GET_ROOM_NAME(landing));
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
  }
}

static bool find_elevator(struct room_data *in_room, rnum_t &elevator_idx, rnum_t &floor_idx) {
  if (!in_room)
    return FALSE;

  // Find the elevator index.
  for (elevator_idx = 0; elevator_idx < num_elevators; elevator_idx++) {
    if (elevator[elevator_idx].room == GET_ROOM_VNUM(in_room)) {
      // Floor index -1 means that you're in the elevator car itself.
      floor_idx = -1;
      return TRUE;
    }

    // Index found, and we're at a floor instead of in the car. Find the floor index.
    for (floor_idx = 0; floor_idx < elevator[elevator_idx].num_floors; floor_idx++) {
      if (elevator[elevator_idx].floor[floor_idx].vnum == GET_ROOM_VNUM(in_room)) {
        // Index found.
        return TRUE;
      }
    }
  }

  elevator_idx = floor_idx = 0;
  return FALSE;
}

static void call_elevator(struct room_data *called_from, rnum_t elevator_idx, rnum_t floor_idx, struct char_data *ch, rnum_t car_rnum) {
  struct veh_data *veh;

  struct room_direction_data *car_exit = world[car_rnum].dir_option[elevator[elevator_idx].floor[floor_idx].doors];
  FAILURE_CASE(car_exit && car_exit->to_room == called_from && !IS_SET(car_exit->exit_info, EX_CLOSED), "The door is already open!");
  FAILURE_CASE(elevator[elevator_idx].destination, "You press the call button, but the elevator's already headed somewhere else. Try again soon.");

  if (IS_RIGGING(ch)) {
    RIG_VEH(ch, veh);
    
    if (veh) {
      send_to_char("You extend your manipulator and call the elevator.\r\n", ch);
      // Idk why, but cannot be assed to compose a separate string right now. Fix later.
      send_to_room("A nearby vehicle presses the call button.\r\n", called_from, veh);
    } else {
      send_to_char("This system is temporarily disabled.\r\n", ch);
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: No veh in IS_RIGGING stanza of call_elevator()!");
      return;
    }
  } else {
    send_to_char("You press the call button, and the small light turns on.\r\n", ch);
    act("$n presses the call button.", FALSE, ch, 0, 0, TO_ROOM);
  }

  elevator[elevator_idx].destination = GET_ROOM_VNUM(called_from);
}

static int parse_elevator_floor_number(const char *argument, rnum_t elevator_idx, char *floorstring, size_t floorstring_sz) {
  int floor_num;

  if (LOWER(*argument) == 'b' && (floor_num = atoi(argument + 1)) > 0) {
    snprintf(floorstring, floorstring_sz, "B%d", floor_num);
    floor_num = elevator[elevator_idx].num_floors + elevator[elevator_idx].start_floor + floor_num - 1;
  }
  else if (LOWER(*argument) == 'g' && elevator[elevator_idx].start_floor <= 0) {
    strcpy(floorstring, "G");
    floor_num = elevator[elevator_idx].num_floors + elevator[elevator_idx].start_floor - 1;
  } else if ((floor_num = atoi(argument)) > 0) {
    snprintf(floorstring, floorstring_sz, "%d", floor_num);
    floor_num = elevator[elevator_idx].num_floors + elevator[elevator_idx].start_floor - 1 - floor_num;
  } else
    floor_num = -1;

  return floor_num;
}

static void move_elevator(struct room_data *car, rnum_t elevator_idx) {
  rnum_t floor_idx = 0;

  if (elevator[elevator_idx].destination && elevator[elevator_idx].dir == -1) {
    // Iterate through elevator floors, looking for the destination floor.
    for (int temp = 0; temp < elevator[elevator_idx].num_floors; temp++)
      if (elevator[elevator_idx].floor[temp].vnum == elevator[elevator_idx].destination)
        floor_idx = temp;

    // Set ascension or descension appropriately.
    if (floor_idx >= car->rating) {
      elevator[elevator_idx].dir = DOWN;
      elevator[elevator_idx].time_left = floor_idx - car->rating;
    } else if (floor_idx < car->rating) {
      elevator[elevator_idx].dir = UP;
      elevator[elevator_idx].time_left = car->rating - floor_idx;
    }

    // Clear the destination.
    elevator[elevator_idx].destination = 0;
  }

  // Move the elevator one floor.
  if (elevator[elevator_idx].time_left > 0) {
    if (!elevator[elevator_idx].is_moving) {
      // Someone pushed the button, but the elevator hasn't started yet. Make it move.
      if (IS_SET(car->dir_option[elevator[elevator_idx].floor[car->rating].doors]->exit_info, EX_CLOSED))
        snprintf(buf, sizeof(buf), "The elevator begins to %s.\r\n", (elevator[elevator_idx].dir == UP ? "ascend" : "descend"));
      else {
        snprintf(buf, sizeof(buf), "The elevator doors close and it begins to %s.\r\n", (elevator[elevator_idx].dir == UP ? "ascend" : "descend"));
        close_elevator_doors(car, elevator_idx, car->rating, FALSE);
      }
      send_to_room(buf, &world[real_room(car->number)]);

      // Message the shaft.
      send_to_room("The elevator car shudders and begins to move.\r\n", &world[real_room(elevator[elevator_idx].floor[car->rating].shaft_vnum)]);

      // Notify the next shaft room that they're about to get a car in the face.
      snprintf(buf, sizeof(buf), "The shaft rumbles ominously as the elevator car %s you begins to accelerate towards you.\r\n",
              elevator[elevator_idx].dir == DOWN ? "above" : "below");
      send_to_room(buf, &world[real_room(elevator[elevator_idx].floor[car->rating + (elevator[elevator_idx].dir == DOWN ? 1 : -1)].shaft_vnum)]);

      // Notify all other shaft rooms about the elevator starting to move.
      for (int i = 0; i < elevator[elevator_idx].num_floors; i++) {
        if (i == car->rating || i == car->rating + (elevator[elevator_idx].dir == DOWN ? 1 : -1))
          continue;

        send_to_room("The shaft rumbles ominously as an elevator car begins to move.\r\n", &world[real_room(elevator[elevator_idx].floor[i].shaft_vnum)]);
      }

      elevator[elevator_idx].is_moving = TRUE;
      return;
    }

    elevator[elevator_idx].time_left--;

    // Message for moving the car out of this part of the shaft.
    struct room_data *shaft = &world[real_room(elevator[elevator_idx].floor[car->rating].shaft_vnum)];
    struct room_data *landing = &world[real_room(elevator[elevator_idx].floor[car->rating].vnum)];
    int dir = elevator[elevator_idx].floor[car->rating].doors;

    snprintf(buf, sizeof(buf), "The elevator car %s swiftly away from you.\r\n", elevator[elevator_idx].dir == DOWN ? "descends" : "ascends");
    send_to_room(buf, &world[real_room(shaft->number)]);
    /* If you fail an athletics test, you get dragged by the car, sustaining impact damage and getting pulled along with the elevator. */
    for (struct char_data *vict = shaft->people, *temp; vict; vict = temp) {
      temp = vict->next_in_room;

      // Nohassle imms and astral projections are immune to this bullshit.
      if (PRF_FLAGGED(vict, PRF_NOHASSLE) || IS_ASTRAL(vict))
        continue;

      // if you pass the test, continue
      int base_target = 6 + modify_target(vict);
      int dice = get_skill(vict, SKILL_ATHLETICS, base_target);
      if (success_test(dice, base_target) > 0) {
        // No message on success (would be spammy)
        continue;
      }

      // Failure case: Deal damage.
      act("^R$n is caught up in the mechanism and dragged along!^n", FALSE, vict, 0, 0, TO_ROOM);
      send_to_char("^RThe elevator mechanism snags you and drags you along!^n\r\n", vict);

      char_from_room(vict);
      char_to_room(vict, &world[real_room(elevator[elevator_idx].floor[(elevator[elevator_idx].dir == DOWN ? car->rating + 1 : car->rating - 1)].shaft_vnum)]);

      snprintf(buf, sizeof(buf), "$n ragdolls in from %s, propelled by the bulk of the moving elevator.", elevator[elevator_idx].dir == DOWN ? "above" : "below");
      act(buf, FALSE, vict, 0, 0, TO_ROOM);

      int power = MAX(4, 15 - (GET_IMPACT(vict) / 2)); // Base power 15 (getting pinned and dragged by an elevator HURTS). Impact armor helps.
      int success = success_test(GET_BOD(vict), MAX(2, power) + modify_target(vict));
      int dam = convert_damage(stage(-success, power));

      // Check for vict's death. This continue statement is essentially no-op, but proves that I've cleared this statement.
      // ...Except you DIDN'T, dumbass: if they're killed by the elevator, their ->next_in_room is garbage! Fixed by adding temp.
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
    if (elevator[elevator_idx].dir == DOWN)
      car->rating++;
    else
      car->rating--;

    // Message for moving the car into this part of the shaft.
    shaft = &world[real_room(elevator[elevator_idx].floor[car->rating].shaft_vnum)];
    landing = &world[real_room(elevator[elevator_idx].floor[car->rating].vnum)];
    dir = elevator[elevator_idx].floor[car->rating].doors;
    snprintf(buf, sizeof(buf), "A rush of air precedes the arrival of an elevator car from %s.\r\n", elevator[elevator_idx].dir == DOWN ? "above" : "below");
    send_to_room(buf, &world[real_room(shaft->number)]);

    /* If you fail an athletics test, the elevator knocks you off the wall, dealing D impact damage and triggering fall. */
    struct char_data *nextc;
    for (struct char_data *vict = shaft->people; vict; vict = nextc) {
      // Safeguard against weirdness around dead characters. Will still crash in certain circumstances with extracted followers.
      nextc = vict->next_in_room;

      // Nohassle imms and astral projections are immune to this bullshit.
      if (PRF_FLAGGED(vict, PRF_NOHASSLE) || IS_ASTRAL(vict))
        continue;

      // if you pass the test, continue
      int base_target = 6 + modify_target(vict);
      int dice = get_skill(vict, SKILL_ATHLETICS, base_target);
      if (success_test(dice, base_target) > 0) {
        act("$n squeezes $mself against the shaft wall, avoiding the passing car.", TRUE, vict, 0, 0, TO_ROOM);
        continue;
      }

      // Failure case: Deal damage.
      act("^R$n is crushed by the bulk of the elevator!^n", FALSE, vict, 0, 0, TO_ROOM);
      send_to_char("^RThe elevator grinds you against the hoistway wall with crushing force!^n\r\n", vict);

      int power = MAX(4, 15 - (GET_IMPACT(vict) / 2)); // Base power 15 (getting pinned and dragged by an elevator HURTS). Impact armor helps.
      int success = success_test(GET_BOD(vict), MAX(2, power) + modify_target(vict));
      int dam = convert_damage(stage(-success, power));

      // Damage them, and stop the pain if they died.
      if (damage(vict, vict, dam, TYPE_ELEVATOR, TRUE))
        continue;

      perform_fall(vict);
    }

    make_elevator_door(real_room(car->number), real_room(landing->number), rev_dir[dir]);

    make_elevator_door(real_room(landing->number), real_room(car->number), dir);

    shaft->temporary_stored_exit[dir] = shaft->dir_option[dir];
    shaft->dir_option[dir] = NULL;

    if (elevator[elevator_idx].time_left > 0) {
      // Notify the next shaft room that they're about to get a car in the face.
      snprintf(buf, sizeof(buf), "A light breeze brushes by as the elevator car %s you speeds towards you.\r\n",
              elevator[elevator_idx].dir == DOWN ? "above" : "below");
      send_to_room(buf, &world[real_room(elevator[elevator_idx].floor[car->rating + (elevator[elevator_idx].dir == DOWN ? 1 : -1)].shaft_vnum)]);
    }
  }

  // Arrive at the target floor.
  else if (elevator[elevator_idx].dir == UP || elevator[elevator_idx].dir == DOWN) {
    int temp = car->rating + 1 - elevator[elevator_idx].num_floors - elevator[elevator_idx].start_floor;
    if (temp > 0)
      snprintf(buf, sizeof(buf), "The elevator stops at B%d, and the doors open to the %s.\r\n", temp,
              fulldirs[elevator[elevator_idx].floor[car->rating].doors]);
    else if (temp == 0)
      snprintf(buf, sizeof(buf), "The elevator stops at the ground floor, and the doors open to the %s.\r\n",
              fulldirs[elevator[elevator_idx].floor[car->rating].doors]);
    else
      snprintf(buf, sizeof(buf), "The elevator stops at floor %d, and the doors open to the %s.\r\n",
              0 - temp, fulldirs[elevator[elevator_idx].floor[car->rating].doors]);
    send_to_room(buf, &world[real_room(car->number)]);
    open_elevator_doors(car, elevator_idx, car->rating, FALSE);
    elevator[elevator_idx].is_moving = FALSE;

    // Notify this shaft room that the elevator has stopped.
    send_to_room("The cable thrums with tension as the elevator comes to a graceful halt.\r\n",
                  &world[real_room(elevator[elevator_idx].floor[car->rating].shaft_vnum)]);

    // Notify all other shaft rooms that the elevator has stopped.
    for (int i = 0; i < elevator[elevator_idx].num_floors; i++) {
      if (i == car->rating)
        continue;

      send_to_room("The ominous rumblings from the shaft's guiderails fade.\r\n", &world[real_room(elevator[elevator_idx].floor[i].shaft_vnum)]);
    }
  } else if (elevator[elevator_idx].dir > 0)
    elevator[elevator_idx].dir--;
  else if (!elevator[elevator_idx].dir) {
    close_elevator_doors(car, elevator_idx, car->rating, TRUE);
    elevator[elevator_idx].dir = -1;
  }
}

static void show_elevator_location_to_ch(struct char_data *ch, struct room_data *car, rnum_t elevator_idx) {
  int floor = car->rating + 1 - elevator[elevator_idx].num_floors - elevator[elevator_idx].start_floor;
  if (floor > 0)
    send_to_char(ch, "The floor indicator shows that the elevator is currently at B%d.\r\n", floor);
  else if (floor == 0)
    send_to_char(ch, "The floor indicator shows that the elevator is currently at the ground floor.\r\n");
  else
    send_to_char(ch, "The floor indicator shows that the elevator is currently at floor %d.\r\n", 0 - floor);
}

static void show_elevator_panel_to_ch(struct char_data *ch, struct room_data *car, rnum_t elevator_idx) {
  char msg_buf[1000];

  strcpy(msg_buf, "The elevator panel displays the following buttons:\r\n");
  int number = 0;
  for (rnum_t floor_idx = elevator[elevator_idx].num_floors - 1; floor_idx >= 0; floor_idx--)
    if (elevator[elevator_idx].floor[floor_idx].vnum > -1) {
      int temp = elevator[elevator_idx].start_floor + floor_idx;
      if (temp < 0)
        snprintf(ENDOF(msg_buf), sizeof(msg_buf) - strlen(msg_buf), "  B%-2d", 0 - temp);
      else if (temp == 0)
        snprintf(ENDOF(msg_buf), sizeof(msg_buf) - strlen(msg_buf), "  G ");
      else
        snprintf(ENDOF(msg_buf), sizeof(msg_buf) - strlen(msg_buf), "  %-2d", temp);
      if (PRF_FLAGGED(ch, PRF_SCREENREADER) || !(++number % elevator[elevator_idx].columns))
        strcat(msg_buf, "\r\n");
    }
  if ((number % elevator[elevator_idx].columns) && !PRF_FLAGGED(ch, PRF_SCREENREADER))
    strcat(msg_buf, "\r\n");

  snprintf(ENDOF(msg_buf), sizeof(msg_buf) - strlen(msg_buf), "\r\n(OPEN)%s(CLOSE)\r\n\r\n", PRF_FLAGGED(ch, PRF_SCREENREADER) ? "\r\n" : "  ");
  
  send_to_char(msg_buf, ch);
}
