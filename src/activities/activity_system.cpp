#include "../interpreter.hpp"
#include "classes.hpp"

// fs alias comes from classes.hpp via its namespace fs = std::filesystem declaration.

std::map<std::string, Activity> global_activities = {};

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
  if (!fs::exists(BASE_ACTIVITY_PATH)) {
    log_vfprintf("WARNING: Unable to find base activity path at %s. Will not load anything.", (BASE_ACTIVITY_PATH).c_str());
    return;
  }

  log("Loading activities:");

  for (const auto &itr : fs::directory_iterator(BASE_ACTIVITY_PATH)) {
    if (!itr.is_directory()) {
      fs::path filename = itr.path();
      log_vfprintf("Loading activity %s...", filename.c_str());
      global_activities.emplace(filename.filename().string(), Activity(filename));
    }
  }
}