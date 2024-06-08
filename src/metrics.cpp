#include "interpreter.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "bullet_pants.hpp"
#include "metrics.hpp"

long global_ammotrack[(END_OF_AMMO_USING_WEAPONS + 1) - START_OF_AMMO_USING_WEAPONS][NUM_AMMOTYPES][NUM_AMMOTRACKS] = {{{0}}};

const char *ammotrack_names[NUM_AMMOTRACKS] = {
  "crafting",
  "purchase",
  "looted",
  "chargen",
  "combat"
};

// TODO: You're tracking at the wrong point. Instead of doing it on load/unload, track creation.
// - ammo box finished test: +10 from crafting
// - npc weapon dropped in corpse: +ammo in magazine
// - chargen creation: +ammo
// - weapon fired by PC: -ammo (account for burst etc)
// - weapon sold or extracted: -ammo in magazine (don't bother tracking it separately, just deduct from what was spawned)

ACMD(do_metrics) {
  send_to_char(ch, "Current ammo metrics:\r\n");

  // I want to see the net of each ammo type, plus gain vs loss, with the ability to drill down.
  /* 
  
  > metrics ammo

  Current ammo net amounts:
  1239 APDS assault rifle rounds (-1000 loading, +2239 crafting)
  */
  for (int _weap_idx = START_OF_AMMO_USING_WEAPONS; _weap_idx <= END_OF_AMMO_USING_WEAPONS; _weap_idx++) {
    int weap_idx = _weap_idx - START_OF_AMMO_USING_WEAPONS;

    for (int ammo_idx = 0; ammo_idx < NUM_AMMOTYPES; ammo_idx++) {
      // First, calculate to see if we have a net change.
      long net_change = 0;
      bool any_change = false;
      for (int track_idx = 0; track_idx < NUM_AMMOTRACKS; track_idx++) {
        if (global_ammotrack[weap_idx][ammo_idx][track_idx] != 0)
          any_change = true;
        net_change += global_ammotrack[weap_idx][ammo_idx][track_idx];
      }

      // If we do, print about it.
      if (any_change) {
        send_to_char(ch, " %6ld %s ( ", net_change, get_ammo_representation(_weap_idx, ammo_idx, net_change == 1 ? 1 : 0, ch));
        for (int track_idx = 0; track_idx < NUM_AMMOTRACKS; track_idx++) {
          if (global_ammotrack[weap_idx][ammo_idx][track_idx] != 0) {
            send_to_char(ch, "%s[%ld] ", ammotrack_names[track_idx], global_ammotrack[weap_idx][ammo_idx][track_idx]);
          }
        }
        send_to_char(")\r\n", ch);
      }
    }
  }
}

// send_to_char(ch, " %s: %s\r\n", ammotrack_names[track_idx], );