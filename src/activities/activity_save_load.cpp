#include "../interpreter.hpp"
#include "classes.hpp"

// Given a filename, loads it as an activity. Returns a pointer to it, in case you care about that.
Activity *load_single_activity(const std::string& slug);

// Iterate through the save directory to find all activity files, then call load_single_activity on each.
void load_activities();

// Write each activity to the save directory.
void save_activities();

// Find and 'delete' the file on disk related to this activity. Actually moves it to an archive directory.
int delete_single_activity(Activity *activity);