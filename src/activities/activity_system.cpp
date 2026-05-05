#include "../interpreter.hpp"
#include "classes.hpp"

std::map<std::string, Activity> global_activities = {};

void load_activities() {
  // Iterate over the contents of the lib/activities directory.
  if (!exists(BASE_ACTIVITY_PATH)) {
    log_vfprintf("WARNING: Unable to find base activity path at %s. Will not load anything.", (BASE_ACTIVITY_PATH).c_str());
    return;
  }

  log("Loading activities:");

  bf::directory_iterator end_itr; // default construction yields past-the-end
  for (bf::directory_iterator itr(BASE_ACTIVITY_PATH); itr != end_itr; ++itr) {
    if (!is_directory(itr->status())) {
      bf::path filename = itr->path();
      log_vfprintf("Loading activity %s...", filename.c_str());
      global_activities.emplace(filename.filename().string(), Activity(filename));
    }
  }
}