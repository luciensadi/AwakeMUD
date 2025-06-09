/*
  To lower the amount of iteration we need to do on mobs/objects/etc, we will be
unloading zones that aren't in use.

  1) Track the last time any player was in a given zone. A simple hook on
from_room and to_room to set a timestamp works here. 2) Once a zone goes unused
for a length of time X, unload all mobs and items in the zone EXCEPT FOR:
     - player corpses
     - quest-flagged things
     - anything in an apartment or storage room
     - anything with a player drop record on it (the code will handle this
separately) 3) Pause zoneloading / resetting / mobact for this zone.

  Then, when a player enters the zone:
  1) Hook into char_to_room. When you're about to enter an offloaded zone, loop
the following Y times:
     - run resets
     - run mobact Z times.
  2) Cleanup any corpses generated during the above process.

  TODO: Investigate how many "load once at game start, then never again" things
we have zoneloading. Probably convert all of these to limited-cap loads.
  TODO: When someone rigs or unrigs a vehicle, check to make sure the zone is
loaded, and populate it if it isn't.
  TODO: Set zone last action timer any time a command is run in the zone


notes for handler changes:
ADD to the zone's amount when:
√ a player or possessed npc enters a room in the zone
√ a player or possessed npc is physically added to a vehicle in the zone
√ a player is virtually added to a vehicle in the zone (remote rigging)
√ a vehicle containing a player enters a room in
√ a staffer possesses an NPC in the zone

REMOVE from the zone's amount when:
√ a player or possessed npc leaves a room in the zone
√ a player or possessed npc physically leaves a vehicle in the zone
√ a player virtually leaves a vehicle in the zone
√ a vehicle containing one or more players leaves a room in the zone
√ a staffer unpossesses an NPC in the zone

ADD to the vehicle's amount when:
√ a staffer possesses an NPC in the veh
√ a player or possessed npc physically enters
√ a player virtually enters

REMOVE from the vehicle's amount when:
√ a staffer unpossesses an NPC in the veh
√ a player or possessed npc physically leaves
√ a player virtually leaves
*/

#include "awake.hpp"
#include "db.hpp"
#include "handler.hpp"
#include "structs.hpp"

extern void reset_zone(rnum_t zone, int reboot);
extern void do_single_mobile_activity(struct char_data *ch);

bool zone_has_pc_occupied_vehicles(struct zone_data *zone) {
  rnum_t room_rnum;

  for (vnum_t room_vnum = zone->number * 100; room_vnum <= zone->top;
       room_vnum++) {
    if ((room_rnum = real_room(room_vnum)) < 0)
      continue;

    for (struct veh_data *veh = world[room_rnum].vehicles; veh;
         veh = veh->next_veh) {
      if (veh->rigger)
        return true;

      for (struct char_data *ch = veh->people; ch; ch = ch->next_in_veh) {
        if (ch->desc || !IS_NPC(ch))
          return true;
      }
    }
  }

  return false;
}

void _attempt_extract_zone_obj(struct obj_data *obj) {
  // No extracting quest targets.
  if (GET_OBJ_QUEST_CHAR_ID(obj))
    return;

  // Don't extract anything dropped by a player.
  if (obj->dropped_by_char)
    return;

  // No extracting corpses.
  if (IS_OBJ_STAT(obj, ITEM_EXTRA_CORPSE))
    return;

  extract_obj(obj);
}

void _attempt_extract_zone_character(struct char_data *ch) {
  // No extracting questies.
  if (GET_MOB_QUEST_CHAR_ID(ch))
    return;

  // Sanity check: Not a PC or PC-controlled entity.
  if (ch->desc) {
    mudlog_vfprintf(
        ch, LOG_SYSLOG,
        "SYSERR: Found character with descriptor while unloading zone!");
    return;
  }

  // Check inventory for quest items.
  for (struct obj_data *obj = ch->carrying; obj; obj = obj->next_content) {
    if (GET_OBJ_QUEST_CHAR_ID(obj))
      return;
  }

  extract_char(ch);
}

/* Offload zone, removing all non-quest and non-player-owned things in it. */
void _offload_zone(struct zone_data *zone) {
  if (!zone) {
    mudlog_vfprintf(NULL, LOG_SYSLOG,
                    "SYSERR: Attempted to offload NULL zone!");
    return;
  }

  rnum_t room_rnum;

  for (vnum_t room_vnum = zone->number * 100; room_vnum <= zone->top;
       room_vnum++) {
    if ((room_rnum = real_room(room_vnum)) < 0)
      continue;

    struct room_data *room = &world[room_rnum];

    // Skip storage rooms, garages, and apartments.
    if (ROOM_FLAGGED(room, ROOM_STORAGE) || ROOM_FLAGGED(room, ROOM_GARAGE) ||
        room->apartment)
      continue;

    // Extract vehicles.
    for (struct veh_data *veh = room->vehicles, *next_veh; veh;
         veh = next_veh) {
      next_veh = veh->next_veh;

      if (veh->owner > 0)
        continue;

      for (struct char_data *ch = veh->people, *next_ch; ch; ch = next_ch) {
        next_ch = ch->next_in_veh;
        _attempt_extract_zone_character(ch);
      }

      for (struct obj_data *obj = room->contents, *next_obj; obj;
           obj = next_obj) {
        next_obj = obj->next_content;
        _attempt_extract_zone_obj(obj);
      }

      // Sanity check: Only unload if nobody's in it after purge.
      if (!veh->people && !veh->contents)
        extract_veh(veh);
    }

    // Extract characters that aren't quest-flagged.
    for (struct char_data *ch = room->people, *next_ch; ch; ch = next_ch) {
      next_ch = ch->next_in_room;
      _attempt_extract_zone_character(ch);
    }

    // Extract items that aren't corpses or quest targets.
    for (struct obj_data *obj = room->contents, *next_obj; obj;
         obj = next_obj) {
      next_obj = obj->next_content;
      _attempt_extract_zone_obj(obj);
    }
  }

  // Flag it as offloaded so we can A) see it in stats, and B) know it needs to
  // be hotloaded on entry.
  zone->offloaded_at = time(0);

  mudlog_vfprintf(NULL, LOG_MISCLOG, "Offloaded zone %ld (%s) at epoch %ld.", zone->number, zone->name, zone->offloaded_at);
}

/* Call this from the main loop on a timer. It iterates over the list of zones
   in the game, calling offload_zone on each. Certain zones can be flagged as
   never-unload (apartment buildings mostly), so we check for that. */
void attempt_to_offload_unused_zones() {
#ifdef TEMPORARY_COMPILATION_GUARD
  time_t current_timestamp = time(0);
  int offloaded_count = 0;

  mudlog("Looking for zones to offload...", NULL, LOG_SYSLOG, TRUE);
  for (int zone_idx = 0; zone_idx < top_of_zone_table; zone_idx++) {
    struct zone_data *zone = &zone_table[zone_idx];

    // Make sure it's not already offloaded and hasn't been flagged as
    // never-offload (-1).
    if (zone->offloaded_at != 0)
      continue;

    // Check if there are currently players in the zone.
    if (zone->players_in_zone > 0)
      continue;

    // Nobody there. Check if last-used time is too recent.
    if (zone->last_player_action >= current_timestamp - (60 * 20))
      continue;

    // It's old enough. Check for rigged vehicles. This is the most expensive
    // check, so we save it for last.
    if (zone_has_pc_occupied_vehicles(zone))
      continue;

    // Passed conditional checks, offload it.
    _offload_zone(zone);
    offloaded_count++;
  }
  mudlog_vfprintf(NULL, LOG_SYSLOG, "Offload complete. %d %s offloaded.",
                  offloaded_count, offloaded_count == 1 ? "zone" : "zones");
#else
  log("Skipping zone offload check-- TEMPORARY_COMPILATION_GUARD is not "
      "defined.");
#endif
}

///////////////////// hotload code

#define NUM_RESETS_TO_DO_WHEN_HOTLOADING 3
#define NUM_MOBACTS_TO_DO_PER_HOTLOAD_LOOP 5

void _process_hotloaded_mob(struct char_data *ch, vnum_t zone_number) {
  if (ch->desc) {
    mudlog_vfprintf(
        ch, LOG_SYSLOG,
        "SYSERR: Found character with descriptor while hotloading zone %ld!",
        zone_number);
    return;
  }

  for (int mobacts_to_do = NUM_MOBACTS_TO_DO_PER_HOTLOAD_LOOP;
       mobacts_to_do > 0; mobacts_to_do--) {
    do_single_mobile_activity(ch);
    // Restore them in case of drain etc.
    GET_MENTAL(ch) = 1000;
    GET_PHYSICAL(ch) = 1000;
  }
}

/* Checks to make sure the zone is already offloaded, then hotloads it, bringing
 * up contents and running resets etc. */
void hotload_zone(rnum_t zone_idx) {
#ifdef TEMPORARY_COMPILATION_GUARD
  rnum_t room_rnum;
  struct zone_data *zone = &zone_table[zone_idx];

  if (zone->offloaded_at <= 0)
    return;

  mudlog_vfprintf(NULL, LOG_MISCLOG,
                  "Hotloading zone %ld, which was offloaded %0.2f minutes ago.",
                  zone->number, (time(0) - zone->offloaded_at) / 60);

  // Unflag as offloaded and set the last activity to now to show that it was
  // just loaded and needs to not be offloaded for a while.
  zone->offloaded_at = 0;
  zone->last_player_action = time(0);

  // To hotload a zone, we loop through its resets a set number of times, and on
  // each reset we loop through mobact for that zone a few times.
  for (int resets_to_do = NUM_RESETS_TO_DO_WHEN_HOTLOADING; resets_to_do > 0;
       resets_to_do--) {
    reset_zone(zone_idx, false);

    for (vnum_t room_vnum = zone->number * 100; room_vnum <= zone->top;
         room_vnum++) {
      if ((room_rnum = real_room(room_vnum)) < 0 ||
          (!world[room_rnum].people && !world[room_rnum].vehicles))
        continue;

      // On-foot mobact.
      for (struct char_data *ch = world[room_rnum].people, *next_ch; ch;
           ch = next_ch) {
        next_ch = ch->next_in_room;
        _process_hotloaded_mob(ch, zone->number);
      }

      // In-vehicle mobact.
      for (struct veh_data *veh = world[room_rnum].vehicles, *next_veh; veh;
           veh = next_veh) {
        next_veh = veh->next_veh;
        for (struct char_data *ch = veh->people, *next_ch; ch; ch = next_ch) {
          next_ch = ch->next_in_veh;
          _process_hotloaded_mob(ch, zone->number);
        }
      }
    }
  }

  // Clean up corpses that aren't player corpses. This is to prevent a hotloaded
  // warfare zone from popping a bunch of corpses for you to loot at once.
  for (vnum_t room_vnum = zone->number * 100; room_vnum <= zone->top;
       room_vnum++) {
    if ((room_rnum = real_room(room_vnum)) < 0 || (!world[room_rnum].contents))
      continue;

    for (struct obj_data *obj = world[room_rnum].contents, *next_obj; obj;
         obj = next_obj) {
      next_obj = obj->next_content;

      if (!IS_OBJ_STAT(obj, ITEM_EXTRA_CORPSE))
        continue;

      if (GET_CORPSE_IS_PC(obj) || GET_OBJ_QUEST_CHAR_ID(obj))
        continue;

      extract_obj(obj);
    }
  }

  mudlog_vfprintf(NULL, LOG_MISCLOG, "Hotloading of zone %ld is complete.",
                  zone->number);
#else
  // Do nothing, we don't hotload zones when this flag is disabled.
#endif
}

int calculate_players_in_vehicle(struct veh_data *veh) {
  int single_veh_players_present =
      (veh->rigger && PLR_FLAGGED(veh->rigger, PLR_REMOTE)) ? 1 : 0;

  for (struct char_data *tmp = veh->people; tmp; tmp = tmp->next_in_veh) {
    if (tmp->desc || !IS_NPNPC(tmp))
      single_veh_players_present++;
  }

  return single_veh_players_present;
}

void recalculate_whole_game_players_in_zone() {
  rnum_t room_rnum;

  // Iterate over all zones
  for (rnum_t zone_idx = 0; zone_idx <= top_of_zone_table; zone_idx++) {
    struct zone_data *zone = &zone_table[zone_idx];
    int on_foot_players_present = 0;
    int veh_players_present = 0;

    int veh_discrepancy = 0;

    // iterate over all rooms in zone
    for (vnum_t room_vnum = zone->number * 100; room_vnum <= zone->top;
         room_vnum++) {
      if ((room_rnum = real_room(room_vnum)) < 0 ||
          (!world[room_rnum].people && !world[room_rnum].vehicles))
        continue;

      // On-foot count.
      for (struct char_data *tmp = world[room_rnum].people; tmp;
           tmp = tmp->next_in_room) {
        if (tmp->desc || !IS_NPNPC(tmp)) {
          on_foot_players_present++;
        }
      }

      // Vehicle count.
      for (struct veh_data *veh = world[room_rnum].vehicles; veh;
           veh = veh->next_veh) {
        int single_veh_players_present = calculate_players_in_vehicle(veh);

        if (single_veh_players_present != veh->players_in_veh) {
          mudlog_vfprintf(
              NULL, LOG_SYSLOG,
              "Vehicle '%s^n' failed PIV validation: has %d, actual %d.\r\n",
              GET_VEH_NAME(veh), veh->players_in_veh, veh_players_present);
          veh_discrepancy += (single_veh_players_present - veh->players_in_veh);

          // Set correct amount.
          veh->players_in_veh = single_veh_players_present;
        }

        veh_players_present += single_veh_players_present;
      }
    }

    // Validate findings.
    if (zone->players_in_zone !=
        (on_foot_players_present + veh_players_present)) {
      if (zone->players_in_zone ==
          (on_foot_players_present + veh_players_present + veh_discrepancy)) {
        mudlog_vfprintf(NULL, LOG_SYSLOG,
                        "Zone '%s^n' failed PIZ validation, but this is "
                        "explained by vehicle validation failure.\r\n",
                        zone->name);
      } else {
        mudlog_vfprintf(NULL, LOG_SYSLOG,
                        "Zone '%s^n' failed PIZ validation: expected total %d, "
                        "actual %d (%d + %d (vd %d)).\r\n",
                        zone->name, zone->players_in_zone,
                        (on_foot_players_present + veh_players_present),
                        on_foot_players_present, veh_players_present,
                        veh_discrepancy);
      }

      // Set correct amount.
      zone->players_in_zone = on_foot_players_present + veh_players_present;
    }
  }
}

void modify_players_in_zone(rnum_t in_zone, int amount, const char *origin) {
  zone_table[in_zone].players_in_zone += amount;

#ifdef DEBUG_PLAYERS_IN_ZONE
  mudlog_vfprintf(NULL, LOG_MISCLOG, "PIZ %d for '%s^n' from %s, now %d",
                  amount, zone_table[in_zone].name, origin,
                  zone_table[in_zone].players_in_zone);
#endif

  if (zone_table[in_zone].players_in_zone < 0 ||
      zone_table[in_zone].players_in_zone > 100) {
    mudlog_vfprintf(NULL, LOG_SYSLOG,
                    "SYSERR: Negative or too-high player-in-zone count %d "
                    "after %d change from %s. Recalculating whole game.",
                    zone_table[in_zone].players_in_zone, amount, origin);

    // Recalculate the whole game's PIZ counts as a stopgap.
    recalculate_whole_game_players_in_zone();
  }
}

void modify_players_in_veh(struct veh_data *veh, int amount,
                           const char *origin) {
  veh->players_in_veh += amount;

#ifdef DEBUG_PLAYERS_IN_ZONE
  mudlog_vfprintf(NULL, LOG_MISCLOG, "PIZ %d for veh %ld from %s, now %d",
                  amount, GET_VEH_IDNUM(veh), origin, veh->players_in_veh);
#endif

  if (veh->players_in_veh < 0 || veh->players_in_veh > 100) {
    mudlog_vfprintf(NULL, LOG_SYSLOG,
                    "SYSERR: Negative or too-high player-in-veh count %d after "
                    "%d change from %s. Recalculating whole game.",
                    veh->players_in_veh, amount, origin);

    // Recalculate the whole game's PIZ counts as a stopgap.
    recalculate_whole_game_players_in_zone();
  }
}