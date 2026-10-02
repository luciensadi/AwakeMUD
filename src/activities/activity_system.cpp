#include "../interpreter.hpp"
#include "classes.hpp"

// fs alias comes from classes.hpp via its namespace fs = std::filesystem declaration.

std::map<std::string, Activity> global_activities = {};

fs::path activity_base_path() {
  // The server chdir()s into the lib data dir before boot code runs, so the
  // conventional location is <data dir>/activities. Also accept <data dir>
  // /lib/activities (repo root as CWD) for tooling and direct invocation.
  fs::path data_dir_activities = fs::absolute("activities");
  if (fs::exists(data_dir_activities)) {
    return data_dir_activities;
  }
  fs::path lib_subdir_activities = fs::absolute("lib") / "activities";
  if (fs::exists(lib_subdir_activities)) {
    return lib_subdir_activities;
  }
  // Neither exists: default to the conventional location (save_to_disk creates it).
  return data_dir_activities;
}

/* When putting a character in an activity:
  - set desc->running_activity
  - set a flag in DB for "current activity"

  When pulling out of activity, clear desc ptr AND DB flag

  When loading a character, if the DB flag is set AND the desc ptr is not set, try to find the activity to rejoin the character to; if not found, warn and clear the DB flag

  this means you need to track running activities by some sort of non-colliding non-reused index. UUIDs or something
  thus, when spawning an activity (in constructor?), add to global map of running activities (or linked list, might be simpler, and cheap enough since it won't be long or iterated over often)
  when destructing an activity, make sure no entry in the running activities map/list, and if any characters are still in the meta room, log a loud warning and put them back where they came from
  - edit system needs some kind of visualizer to tell you which terminal nodes don't put you in an exit room
  - would also be great to have a tree layout of "here is the ways this can branch", but that gets messy if someone reconnects back up the trunk
  - definitely need a warning for loops. you want to put someone in groundhog day, you need to deliberately write that.

  what happens to physical bodies of characters when you spawn an activity? pull them off into a meta room, newly created for their travel. STRETCH: Desc of room updates as they go--
    but I almost don't want to include this as it increases the writing work of activities and means we'll have fewer completed activities for a given amount of work done
*/

void load_activities() {
  // Iterate over the contents of the lib/activities directory.
  fs::path base_path = activity_base_path();
  if (!fs::exists(base_path)) {
    log_vfprintf("WARNING: Unable to find base activity path at %s. Will not load anything.", base_path.c_str());
    return;
  }

  log("Loading activities:");

  for (const auto &itr : fs::directory_iterator(base_path)) {
    if (!itr.is_directory()) {
      fs::path filename = itr.path();
      log_vfprintf("Loading activity %s...", filename.c_str());
      // One corrupted / hand-edited file must not abort the entire MUD boot.
      try {
        global_activities.emplace(filename.filename().string(), Activity(filename));
      } catch (const std::exception &e) {
        log_vfprintf("SYSERR: Failed to load activity file %s: %s. Skipping it.", filename.filename().c_str(), e.what());
      }
    }
  }
}

void send_activity_debug_msg(struct char_data *ch, const char *fmt, ...) {
  va_list args;

  char msg_str[10000];

  va_start(args, fmt);
  vsnprintf(msg_str, sizeof(msg_str), fmt, args);
  va_end(args);

  strlcpy(msg_str, replace_neutral_color_codes(msg_str, "^L"), sizeof(msg_str));

  // Guard against characters in neither a room list nor a vehicle (limbo /
  // extraction edge cases): deliver just to ch instead of dereferencing nulls.
  if (!ch->in_room && !ch->in_veh) {
    if (PRF_FLAGGED(ch, PRF_ACTIVITIES_DEBUG)) {
      send_to_char(ch, "^L[Activities Debug (%s^L)]: %s^n\r\n", GET_CHAR_NAME(ch), msg_str);
    }
    return;
  }

  // Send it to all subscribers around ch, including ch if necessary
  for (struct char_data *witness = ch->in_room ? ch->in_room->people : ch->in_veh->people;
       witness;
       witness = ch->in_room ? witness->next_in_room : witness->next_in_veh)
  {
    if (PRF_FLAGGED(witness, PRF_ACTIVITIES_DEBUG)) {
      send_to_char(witness, "^L[Activities Debug (%s^L)]: %s^n\r\n", GET_CHAR_NAME(ch), msg_str);
    }
  }
}