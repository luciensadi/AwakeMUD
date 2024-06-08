#ifndef __metrics_hpp__
#define __metrics_hpp__

#include "awake.hpp"

// Track ammo sources.
#define AMMOTRACK_CRAFTING       0
#define AMMOTRACK_PURCHASES      1
#define AMMOTRACK_NPC_SPAWNED    2
#define AMMOTRACK_CHARGEN        3
#define AMMOTRACK_COMBAT         4
#define NUM_AMMOTRACKS           5

extern long global_ammotrack[(END_OF_AMMO_USING_WEAPONS + 1) - START_OF_AMMO_USING_WEAPONS][NUM_AMMOTYPES][NUM_AMMOTRACKS];

#define AMMOTRACK(ch, weaptype, ammotype, category, amt) { if (!ch || !IS_NPC(ch)) global_ammotrack[weaptype - START_OF_AMMO_USING_WEAPONS][ammotype][category] += amt; }
#define AMMOTRACK_OK(weaptype, ammotype, category, amt) { global_ammotrack[weaptype - START_OF_AMMO_USING_WEAPONS][ammotype][category] += amt; }

#endif // __metrics_hpp__