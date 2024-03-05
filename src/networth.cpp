#include "structs.hpp"
#include "db.hpp"
#include "utils.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "networth.hpp"
#include "constants.hpp"

extern const char *networth_modes[NUM_NETWORTH_BITS];

void _display_all_networth(struct char_data *viewer, struct char_data *victim);
long _get_nuyen_spent_on_initiations(struct char_data *ch);
long _get_nuyen_spent_on_metamagics(struct char_data *ch);
long _get_nuyen_spent_on_skills(struct char_data *ch);
long _get_nuyen_spent_on_attributes(struct char_data *ch);

ACMD(do_networth) {
  char target[MAX_INPUT_LENGTH];
  char *mode = any_one_arg(argument, target);

  if (!*argument || !*target) {
    _display_all_networth(ch, ch);
    return;
  }

  struct char_data *vict = get_player_vis(ch, target, FALSE);

  FAILURE_CASE_PRINTF(!vict, "You don't see anyone online called '%s'.", target);

  if (!mode || !*mode) {
    _display_all_networth(ch, vict);
    return;
  }

  int mode_bits = 0;
  for (int mode_idx = 0; mode_idx < NUM_NETWORTH_BITS; mode_idx++) {
    if (str_str(mode, networth_modes[mode_idx])) {
      mode_bits |= mode_idx;
    }
  }
  send_to_char(ch, "%s's net worth across selected modes (%d): ^c%ld^n\r\n", GET_CHAR_NAME(vict), mode_bits, get_net_worth(vict, mode_bits));
}

long get_net_worth(struct char_data *ch, int mode) {
  // If they've requested all, give all back.
  if (mode & NETWORTH_MODE_ALL) {
    for (int i = 0; i < NUM_NETWORTH_BITS; i++)
      mode |= (1 << i);
  }

  long total_networth = 0;

  // Value of bank and nuyen.
  if (mode & NETWORTH_MODE_LIQUID) {
    total_networth += GET_NUYEN(ch);
    total_networth += GET_BANK(ch);
  }

  // Value of owned vehicles and their contents.
  if (mode & NETWORTH_MODE_VEHICLES) {
    for (struct veh_data *veh = veh_list; veh; veh = veh->next) {
      if (veh->owner == GET_IDNUM_EVEN_IF_PROJECTING(ch)) {
        total_networth += get_cost_of_veh_and_contents(veh);
      }
    }
  }

  // Value of apartment contents.
  if (mode & NETWORTH_MODE_APARTMENTS) {
    // Leave out vehicle values if we've reference them elsewhere.
    bool should_include_vehicles = !(mode & NETWORTH_MODE_VEHICLES);

    for (auto &complex : global_apartment_complexes) {
      for (auto &apartment : complex->get_apartments()) {
        if (apartment->get_owner_id() == GET_IDNUM_EVEN_IF_PROJECTING(ch)) {
          total_networth += apartment->get_cost_of_contents(should_include_vehicles);
        }
      }
    }
  }

  // Value of things on or in their person.
  if (mode & NETWORTH_MODE_ON_PC) {
    // Carried.
    for (struct obj_data *obj = ch->carrying; obj; obj = obj->next_content) {
      total_networth += get_cost_of_obj_and_contents(obj);
    }

    // Worn / equipped.
    for (int wear_idx = 0; wear_idx < NUM_WEARS; wear_idx++) {
      if (GET_EQ(ch, wear_idx)) {
        total_networth += get_cost_of_obj_and_contents(GET_EQ(ch, wear_idx));
      }
    }

    // Installed.
    for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content) {
      total_networth += get_cost_of_obj_and_contents(obj);
    }
    for (struct obj_data *obj = ch->bioware; obj; obj = obj->next_content) {
      total_networth += get_cost_of_obj_and_contents(obj);
    }
  }

  // Leases are irretrievable spend.
  if (mode & NETWORTH_MODE_LEASES) {
    for (auto &complex : global_apartment_complexes) {
      for (auto &apartment : complex->get_apartments()) {
        if (apartment->get_owner_id() == GET_IDNUM_EVEN_IF_PROJECTING(ch)) {
          total_networth += apartment->get_remaining_lease_value();
        }
      }
    }
  }

  if (mode & NETWORTH_MODE_SKILLS_ETC) {
    total_networth += _get_nuyen_spent_on_initiations(ch);

    total_networth += _get_nuyen_spent_on_metamagics(ch);

    total_networth += _get_nuyen_spent_on_skills(ch);

    total_networth += _get_nuyen_spent_on_attributes(ch);
  }

  return total_networth;
}

void _display_all_networth(struct char_data *viewer, struct char_data *victim) {
  long total = 0;

  for (int mode_bit = 1; mode_bit < NUM_NETWORTH_BITS; mode_bit++) {
    long result = get_net_worth(victim, (1 << mode_bit));
    send_to_char(viewer, "%17s: ^c%ld^n\r\n", networth_modes[mode_bit], result);
    total += result;
  }

  send_to_char(viewer, "Total: ^C%ld^n\r\n", total);
}

extern long calculate_init_nuyen_cost(int desired_grade);
long _get_nuyen_spent_on_initiations(struct char_data *ch) {
  if (GET_TRADITION(ch) == TRAD_MUNDANE)
    return 0;

  long total = 0;

  for (int i = 1; i <= GET_GRADE(ch); i++) {
    total += calculate_init_nuyen_cost(i);
  }

  return total;
}

long _get_nuyen_spent_on_metamagics(struct char_data *ch) {
  if (GET_TRADITION(ch) == TRAD_MUNDANE)
    return 0;

  long total = 0;

  // This can't be tracked right now due to the dynamic nature of metamagic cost (current grade multiplied by constant).

  return total;
}

long _get_nuyen_spent_on_skills(struct char_data *ch) {
  long total = 0;

  // Needs additional work to figure out what was purchased in chargen vs outside.

  return total;
}

long _get_nuyen_spent_on_attributes(struct char_data *ch) {
  long total = 0;

  // Needs additional work to figure out what was purchased in chargen vs outside.

  return total;
}