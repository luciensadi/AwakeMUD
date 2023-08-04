// Point-to-point flight between rooms.
#include <math.h>

#include "awake.hpp"
#include "structs.hpp"
#include "db.hpp"
#include "interpreter.hpp"
#include "comm.hpp"
#include "utils.hpp"
#include "handler.hpp"
#include "zoomies.hpp"

// External prototypes.
extern void chkdmg(struct veh_data * veh);

// TODO: Test mortals logging in in the airborne room.
// TODO: Confirm that rigged flight works. (RIG and CONTROL)
// TODO: Prevent rigger from unrigging during flight. (this sounds shitty, especially for long flights?)

ACMD(do_flyto) {
  // FLYTO <destination code>
  struct veh_data *veh = NULL;
  struct room_data *target_room = NULL;
  int skill_level;

  skip_spaces(&argument);

  RIG_VEH(ch, veh);

  if (!veh) {
    act("$n scrunches up $s face and flaps $s arms furiously, but doesn't quite manage to achieve liftoff.", TRUE, ch, 0, 0, TO_ROOM);
    send_to_char("You scrunch up your face and flap your arms furiously, but don't quite manage to achieve liftoff.\r\n", ch);
    return;
  }

  FAILURE_CASE_PRINTF(get_veh_controlled_by_char(ch) != veh, "You're not controlling %s.", GET_VEH_NAME(veh));

  // Is the flight system working?
  rnum_t in_flight_room_rnum = real_room(RM_AIRBORNE);
  if (in_flight_room_rnum < 0) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: In-flight room %ld does not exist! Flight system will not work until it does.", RM_AIRBORNE);
    send_to_char("Sorry, the flight system is currently disabled.\r\n", ch);
    return;
  }

  // Can it fly at all?
  FAILURE_CASE_PRINTF(!veh_can_traverse_air(veh), "%s isn't flight-capable.", CAP(GET_VEH_NAME_NOFORMAT(veh)));

  // Do you have the skills to fly it?
  skill_level = GET_SKILL(ch, get_pilot_skill_for_veh(veh));
  FAILURE_CASE_PRINTF(skill_level <= 0, "You have no idea how to pilot %s.", GET_VEH_NAME(veh));

  // Can you fly from your current room? Sends a message with any errors.
  if (!check_for_valid_launch_location(ch, veh, TRUE))
    return;

  // Flight destination check. If none provided, give a list of available ones.
  if (!*argument) {
    send_to_char("You're aware of the following destinations:\r\n", ch);
    for (rnum_t rnum = 0; rnum <= top_of_world; rnum++) {
      if (veh->in_room != &world[rnum] && veh_can_launch_from_or_land_at(veh, &world[rnum])) {
        float distance = get_flight_distance_to_room(veh, &world[rnum]);

        send_to_char(ch, "%3s) %40s  (%.1f kms, %d nuyen in fuel)\r\n", 
                     GET_ROOM_FLIGHT_CODE(&world[rnum]), 
                     GET_ROOM_NAME(&world[rnum]), 
                     distance,
                     calculate_flight_cost(veh, distance));
      }
    }
    return;
  }

  // They've provided a flight destination. Find it.
  for (rnum_t rnum = 0; rnum <= top_of_world; rnum++) {
    if (veh_can_launch_from_or_land_at(veh, &world[rnum]) && !str_cmp(argument, GET_ROOM_FLIGHT_CODE(&world[rnum]))) {
      target_room = &world[rnum];
      break;
    }
  }

  FAILURE_CASE_PRINTF(!target_room, "You're not aware of any place called %s.", argument);
  FAILURE_CASE(target_room == veh->in_room, "You're already there.");

  // Valid destination. Calculate fuel cost.
  float distance = get_flight_distance_to_room(veh, target_room);
  int fuel_cost = calculate_flight_cost(veh, distance);

  FAILURE_CASE_PRINTF(GET_NUYEN(ch) < fuel_cost, "It will cost %d nuyen for fuel and maintenance, but you only have %d on hand.", fuel_cost, GET_NUYEN(ch));
  send_to_char(ch, "After purchasing the requisite fuel and maintenance for %d nuyen, you begin the takeoff process.\r\n", fuel_cost);
  lose_nuyen(ch, fuel_cost, NUYEN_OUTFLOW_FLIGHT_FUEL);

  // Roll your flight test, which is modified by weather etc.
  int result = flight_test(ch, veh);
  if (result < 0) {
    // Botch. A botch costs the full fuel cost (it's loaded when you take off, and lost when you crash).
    switch (veh->type) {
      case VEH_ROTORCRAFT:
      case VEH_VECTORTHRUST:
        snprintf(buf, sizeof(buf), "%s lifts off, but an unlucky bird strike sends it plunging back to earth!\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
        send_to_room(buf, veh->in_room, veh);
        send_to_veh("You lift off, but an unlucky bird strike sends you plunging back to earth!\r\n", veh, NULL, TRUE);
        break;
      case VEH_LTA:
        snprintf(buf, sizeof(buf), "%s floats dumpily towards the sky, but an unlucky wind blows it into an obstacle, sending it plunging back to earth!\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
        send_to_room(buf, veh->in_room, veh);
        send_to_veh("You float dumpily towards the sky, but an unlucky wind blows you into a pylon! The resulting gash in the balloon sends you plunging back to earth.\r\n", veh, NULL, TRUE);
        break;
      default:
        // It's a fixed wing or a non-specified drone.
        snprintf(buf, sizeof(buf), "%s roars down the runway and lifts off, but an unlucky bird strike sends it plunging back to earth!\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
        send_to_room(buf, veh->in_room, veh);
        send_to_veh("You roar down the runway and lift off, but an unlucky bird strike sends you plunging back to earth!\r\n", veh, NULL, TRUE);
        break;
    }

    crash_flying_vehicle(veh);
    return;
  } else if (result == 0) {
    // Failure, but you survive. This only costs a minimal amount for takeoff fuel.
    send_to_char("You abort your takeoff at the last moment, narrowly avoiding disaster.\r\n", ch);
    snprintf(buf, sizeof(buf), "%s aborts its takeoff at the last moment.\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
    send_to_room(buf, veh->in_room, veh);
    send_to_veh("Your vehicle aborts its takeoff at the last moment.\r\n", veh, ch, TRUE);
    int refund = fuel_cost - MIN(fuel_cost, 100);
    
    if (refund > 0) {
      // Yes, specifically gain on an outflow: we're refunding an expenditure.
      gain_nuyen(ch, refund, NUYEN_OUTFLOW_FLIGHT_FUEL);
      send_to_char(ch, "You've been refunded %d nuyen for the unused fuel.\r\n", refund);
    }
    return;
  }
  
  // Successful takeoff. This costs the full fuel amount.
  send_to_char(ch, "You guide %s into the air.\r\n", GET_VEH_NAME(veh));

  switch (veh->type) {
    case VEH_ROTORCRAFT:
    case VEH_VECTORTHRUST:
      snprintf(buf, sizeof(buf), "%s lifts off, thrumming away into the distance.\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
      send_to_room(buf, veh->in_room, veh);
      send_to_veh("You lift off and are on your way.\r\n", veh, NULL, TRUE);
      break;
    case VEH_LTA:
      snprintf(buf, sizeof(buf), "%s floats dumpily towards the sky, making for the cloud layer.\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
      send_to_room(buf, veh->in_room, veh);
      send_to_veh("You float dumpily towards the sky.\r\n", veh, NULL, TRUE);
      break;
    default:
      // It's a fixed wing or a non-specified drone.
      snprintf(buf, sizeof(buf), "%s roars down the runway and lifts off!\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
      send_to_room(buf, veh->in_room, veh);
      send_to_veh("You roar down the runway and lift off!\r\n", veh, NULL, TRUE);
      break;
  }

  // Set destination.
  veh->flight_target = target_room;
  veh->flight_duration = calculate_flight_time(veh, distance);

  // Move vehicle from room to safe place until it returns to ground.
  veh_from_room(veh);
  veh_to_room(veh, &world[in_flight_room_rnum]);

  snprintf(buf, sizeof(buf), "Somewhere in the distance, you spot %s ascending into the sky.\r\n", GET_VEH_NAME(veh));
  send_to_room(buf, veh->in_room, veh);

  // Let them know how long it will be.
  send_flight_estimate(ch, veh);
}

void send_flight_estimate(struct char_data *ch, veh_data *veh) {
  if (!veh) {
    mudlog("SYSERR: Got NULL vehicle to send_flight_estimate()!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  float flight_irl_seconds = veh->flight_duration * (PULSE_FLIGHT / PASSES_PER_SEC);
  float flight_game_hours = flight_irl_seconds / SECS_PER_MUD_HOUR;
  int irl_minutes = (int) ceilf(flight_irl_seconds / 60);
  send_to_char(ch, "You estimate that you'll be airborne for the next %0.1f hours (%d RL minute%s).\r\n",
               flight_game_hours,
               irl_minutes,
               irl_minutes == 1 ? "" : "s");
  if (IS_SENATOR(ch)) {
    // send_to_char(ch, "(debug: duration %d, giving %f seconds, %f hours)\r\n", veh->flight_duration, flight_irl_seconds, flight_game_hours);
  }
}

bool check_for_valid_launch_location(struct char_data *ch, struct veh_data *veh, bool message) {
  // Can't be in a vehicle.
  FALSE_CASE_PRINTF(veh->in_veh, "You'll need to drive out of %s first.", GET_VEH_NAME(veh->in_veh));

  // Must be in a room.
  if (!veh->in_room) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Somehow %s (piloted by %s) ended up with no containing vehicle or room!", GET_VEH_NAME(veh), GET_CHAR_NAME(ch));
    send_to_char("Something went wrong, your vehicle seems lost. Contact staff for assistance.\r\n", ch);
    return FALSE;
  }

  // Confirm that the room they're in is flagged properly for their vehicle type.
  if (!veh_can_launch_from_or_land_at(veh, veh->in_room)) {
    switch (veh->type) {
      case VEH_ROTORCRAFT:
      case VEH_VECTORTHRUST:
      case VEH_LTA:
        send_to_char("You must be at a helipad or runway to take off.\r\n", ch);
        return FALSE;
      default:
        // It's a drone or something. Assume it's fixed wing.
        send_to_char("You must be at a runway to take off.\r\n", ch);
        return FALSE;
    }
  }
  
  // All kosher, go for it.
  return TRUE;
}

bool veh_can_launch_from_or_land_at(struct veh_data *veh, struct room_data *room) {
  switch (veh->type) {
    case VEH_ROTORCRAFT:
    case VEH_VECTORTHRUST:
    case VEH_LTA:
      return ROOM_FLAGGED(room, ROOM_HELIPAD) || ROOM_FLAGGED(room, ROOM_RUNWAY);
    default:
      // It's a fixed wing or a non-specified drone.
      return ROOM_FLAGGED(room, ROOM_RUNWAY);
  }
}

double degrees_to_radians(float degrees) {
  return degrees * M_PI / 180;
}

#define EARTH_RADIUS_KM 6371
float get_flight_distance_to_room(struct veh_data *veh, struct room_data *room) {
  // Calculates the great circle distance in kilometers between two GPS points (the ones occupied by the vehicle and the room).
  // Adapted from https://stackoverflow.com/questions/365826/calculate-distance-between-2-gps-coordinates.
  float dLat = degrees_to_radians(veh->in_room->latitude - room->latitude);
  float dLon = degrees_to_radians(veh->in_room->longitude - room->longitude);

  float lat1 = degrees_to_radians(room->latitude);
  float lat2 = degrees_to_radians(veh->in_room->latitude);

  float a = sin(dLat/2) * sin(dLat/2) +
          sin(dLon/2) * sin(dLon/2) * cos(lat1) * cos(lat2); 
  float c = 2 * atan2(sqrt(a), sqrt(1-a)); 

  float distance = EARTH_RADIUS_KM * c;

  /*
  mudlog_vfprintf(NULL, LOG_MISCLOG, "DEBUG: Distance between %s (%f, %f) and %s (%f, %f) is %0.2f kilometers.",
                  GET_ROOM_NAME(veh->in_room),
                  veh->in_room->latitude,
                  veh->in_room->longitude,
                  GET_ROOM_NAME(room),
                  room->latitude,
                  room->longitude,
                  distance);
  */

  return distance;
}
#undef EARTH_RADIUS_KM

float _calculate_flight_hours(struct veh_data *veh, float distance) {
  // Given a distance in kilometers, calculates how long it will take for a vehicle to traverse it.
  if (!veh) {
    mudlog("SYSERR: Received NULL vehicle to _calculate_flight_hours()!", NULL, LOG_SYSLOG, TRUE);
    return 1000000;
  }

  return distance / (MAX(1, veh->speed) * 1.2);
}

int calculate_flight_cost(struct veh_data *veh, float distance) {
  // Returns the fuel, maintenance, storage etc cost for a vehicle of this type/load/etc to fly the given distance.
  // Approximated from an online calculator that gave the flight hour cost for a Cessna. If someone wants to get more granular and provide better numbers, I welcome a PR!

  if (!veh) {
    mudlog("SYSERR: Received NULL vehicle to calculate_flight_cost()!", NULL, LOG_SYSLOG, TRUE);
    return 1000000;
  }

  // Optempo: Divide by 100,000, round to nearest tenth.
  float optempo_cost = ((((float) veh->cost) / 1000) + .5) / 100;

  // Crank it up by 10% to account for maintenance and storage.
  optempo_cost *= 1.1;

  return (int) ceilf(distance * optempo_cost);
}

int calculate_flight_time(struct veh_data *veh, float distance) {
  // Given a vehicle and a distance to travel, figure out how long it will take that vehicle to get there.

  // Recall that vehicle speeds are expressed as meters per combat turn.

  if (!veh) {
    mudlog("SYSERR: Received NULL vehicle to calculate_flight_time()!", NULL, LOG_SYSLOG, TRUE);
    return 1000000;
  }

  // Calculate the game-time hours it will take to travel the given distance.
  float flight_hours = _calculate_flight_hours(veh, distance);

  // Convert these to game ticks.
  float in_game_ticks = ceilf(flight_hours * SECS_PER_MUD_HOUR / (PULSE_FLIGHT / PASSES_PER_SEC));

  return in_game_ticks;
}

int flight_test(struct char_data *ch, struct veh_data *veh) {
  // Roll a test of piloting skill VS base TN, modified by weather, wounds, etc. Return the result.
  char rbuf[1000];

  if (!ch || !veh) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Received %s ch and %s vehicle to flight_test()!", ch ? "valid" : "NULL", veh ? "valid" : "NULL");
    return 0;
  }

  snprintf(rbuf, sizeof(rbuf), "Flight test modifiers for %s: ", GET_CHAR_NAME(ch));

  int skill_num = get_pilot_skill_for_veh(veh);
  int tn = 4;

  modify_target_rbuf(ch, rbuf, sizeof(rbuf));
  
  // Apply weather modifiers.
  switch (weather_info.sky) {
    case SKY_CLOUDY:
      strlcat(rbuf, "Cloudy, TN +1", sizeof(rbuf));
      tn += 1;
      break;
    case SKY_RAINING:
      strlcat(rbuf, "Raining, TN +2", sizeof(rbuf));
      tn += 2;
      break;
    case SKY_LIGHTNING:
      strlcat(rbuf, "Storming, TN +3", sizeof(rbuf));
      tn += 3;
      break;
  }
  
  int dice = get_skill(ch, skill_num, tn, rbuf, sizeof(rbuf));
  int successes = success_test(dice, tn, ch, "flight-test");
  snprintf(ENDOF(rbuf), sizeof(rbuf) - strlen(rbuf), ". Dice %d, TN %d: %d success%s.", dice, tn, successes, successes == 1 ? "" : "es");

  act(rbuf, TRUE, ch, 0, 0, TO_ROLLS);

  return successes;
}

void crash_flying_vehicle(struct veh_data *veh) {
  // If airborne, move to random road location and destroy. Otherwise, destroy in place.
  rnum_t in_flight_room_rnum;

  if ((in_flight_room_rnum = real_room(RM_AIRBORNE)) < 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: In-flight room %ld does not exist! (crash_flying_vehicle())", RM_AIRBORNE);
    return;
  }

  if (!veh) {
    mudlog("SYSERR: Received NULL vehicle to crash_flying_vehicle()!", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  if (veh->in_room == &world[in_flight_room_rnum]) {
    // Let them know it's falling.
    snprintf(buf, sizeof(buf), "Somewhere in the distance, you see %s plunge rapidly towards the ground!\r\n", GET_VEH_NAME(veh));
    send_to_room(buf, veh->in_room, veh);

    // Select a random road room that's in a connected zone.
    for (int world_idx = 0; world_idx <= top_of_world; world_idx++) {
      if (ROOM_FLAGGED(&world[world_idx], ROOM_ROAD) && dice(1, 200) <= 1 && !vnum_from_non_connected_zone(world[world_idx].number)) {
        veh_from_room(veh);
        veh_to_room(veh, &world[world_idx]);
        snprintf(buf, sizeof(buf), "%s comes hurtling in on a collision course with the road!\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
        send_to_room(buf, veh->in_room, veh);
        break;
      }
    }

    // Failed to go anywhere: Send it to the I-5.
    if (veh->in_room == &world[in_flight_room_rnum]) {
      rnum_t i5_room = real_room(RM_FLIGHT_CRASH_FALLBACK);
      if (i5_room < 0) {
        mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Failed to crash flying vehicle-- fallback room %d does not exist!", RM_FLIGHT_CRASH_FALLBACK);
        return;
      }

      veh_from_room(veh);
      veh_to_room(veh, &world[i5_room]);
      snprintf(buf, sizeof(buf), "%s comes hurtling in on a collision course with the road!\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
      send_to_room(buf, veh->in_room, veh);
    }
  }

  // Now that we're certain it's landed in the real world, destroy it.
  veh->damage = VEH_DAM_THRESHOLD_DESTROYED;
  chkdmg(veh);
}

void process_flying_vehicles() {
  // Iterate through all vehicles in Airborne and handle them.
  rnum_t in_flight_room_rnum;

  if ((in_flight_room_rnum = real_room(RM_AIRBORNE)) < 0) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: In-flight room %ld does not exist! (process_flying_vehicles())", RM_AIRBORNE);
    return;
  }

  for (struct veh_data *veh = world[in_flight_room_rnum].vehicles; veh; veh = veh->next_veh) {
    struct char_data *controller;

    // Figure out who's controlling it.
    if (!(controller = veh->rigger)) {
      for (controller = veh->people; controller; controller = controller->next_in_veh) {
        if (get_veh_controlled_by_char(controller) == veh) {
          break;
        }
      }
    }

    // Nobody's at the controls? Crash it.
    if (!controller) {
      send_to_veh("Your stomach drops as your uncontrolled vehicle plunges towards the ground!\r\n", veh, controller, TRUE);
      crash_flying_vehicle(veh);
      continue;
    }

    // Excessively damaged? Crash it.
    if (veh->damage >= VEH_DAM_THRESHOLD_DESTROYED) {
      mudlog_vfprintf(controller, LOG_SYSLOG, "SYSERR: Fully-damaged vehicle %s controlled by %s is airborne somehow! Crashing them.",
                      GET_VEH_NAME(veh),
                      GET_CHAR_NAME(controller));
      send_to_char(controller, "With the flight surfaces destroyed, %s plummets to the ground!\r\n", GET_VEH_NAME(veh));
      send_to_veh("Your stomach drops as your vehicle goes into an uncontrolled descent!\r\n", veh, controller, TRUE);
      crash_flying_vehicle(veh);
      continue;
    }

    // No valid destination? Crash it.
    if (!veh->flight_target || !(ROOM_FLAGGED(veh->flight_target, ROOM_RUNWAY) || ROOM_FLAGGED(veh->flight_target, ROOM_HELIPAD))) {
      mudlog_vfprintf(controller, LOG_SYSLOG, "SYSERR: Vehicle %s controlled by %s got airborne, but their flight target is invalid! Crashing them.",
                      GET_VEH_NAME(veh),
                      GET_CHAR_NAME(controller));
      send_to_char("You get turned around in the clouds, and by the time you realize what's happening, you're on a collision course with the ground!\r\n", controller);
      send_to_veh("Your stomach drops as your vehicle goes into an uncontrolled descent!\r\n", veh, controller, TRUE);
      crash_flying_vehicle(veh);
      continue;
    }
    
    // It's properly controlled. Roll a flight test.
    int result = flight_test(controller, veh);
    if (result < 0) {
      // Botch? Ground.
      send_to_char(controller, "You fumble at the controls, accidentally putting %s into a steep dive towards the ground!\r\n", GET_VEH_NAME(veh));
      send_to_veh("Your stomach drops as your vehicle goes into an uncontrolled descent!\r\n", veh, controller, FALSE);
      crash_flying_vehicle(veh);
      continue;
    } else if (result == 0) {
      // Fail? Make no progress.
    } else {
      // Successful roll: Tick down flight duration.
      veh->flight_duration--;

      // If the flight duration has ended, land it.
      if (veh->flight_duration <= 0) {
        send_to_char(controller, "You guide %s in for a landing at %s...\r\n", GET_VEH_NAME(veh), GET_ROOM_NAME(veh->flight_target));

        snprintf(buf, sizeof(buf), "Somewhere in the distance, you see %s beginning its descent.\r\n", GET_VEH_NAME(veh));
        send_to_room(buf, veh->in_room, veh);
        
        veh_from_room(veh);
        veh_to_room(veh, veh->flight_target);
        
        // Yes, we've had one roll, but what about _second_ roll?
        result = flight_test(controller, veh);
        if (result < 0) {
          // Botched landing: Crash.
          send_to_char("You fuck up the landing, sending your vehicle straight into the ground!\r\n", controller);
          send_to_veh("You scream as a botched landing sends you hurtling towards the ground!\r\n", veh, controller, FALSE);
          snprintf(buf, sizeof(buf), "%s comes hurtling in on a deadly collision course!\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
          send_to_room(buf, veh->in_room, veh);
          crash_flying_vehicle(veh);
        } else if (result == 0) {
          // Zero successes: Land with damage.
          send_to_char("You fuck up the landing, slamming your vehicle into the ground painfully!\r\n", controller);
          send_to_veh("There's an almighty BANG as your vehicle slams into the ground!\r\n", veh, controller, FALSE);
          snprintf(buf, sizeof(buf), "%s comes hurtling in on a collision course!\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
          send_to_room(buf, veh->in_room, veh);
          veh->damage += 5;
          chkdmg(veh);
        } else {
          // Successful landing.
          send_to_char("You stick the landing, setting down gently.\r\n", controller);
          send_to_veh("One smooth landing later, you're back on solid ground.\r\n", veh, controller, FALSE);
          snprintf(buf, sizeof(buf), "%s flies in and settles gently to the ground.\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
          send_to_room(buf, veh->in_room, veh);
        }
      }
    }
  }
}

bool veh_is_currently_flying(struct veh_data *veh) {
  rnum_t in_flight_room_rnum;

  if ((in_flight_room_rnum = real_room(RM_AIRBORNE)) < 0) {
    return FALSE;
  }

  return veh->in_room == &world[in_flight_room_rnum];
}

void clear_veh_flight_info(struct veh_data *veh) {
  veh->flight_duration = 0;
  veh->flight_target = NULL;
}