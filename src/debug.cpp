/************************************************************************
 * Code for the DEBUG command, which invokes various testing protocols. *
 * Written for AwakeMUD Community Edition by Lucien Sadi.     07/01/19  *
 ************************************************************************/

#include <time.h>
#include <string.h>
#include <mysql/mysql.h>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
namespace bf = boost::filesystem;

#include "telnet.hpp"

#include "types.hpp"
#include "awake.hpp"
#include "interpreter.hpp"
#include "bitfield.hpp"
#include "comm.hpp"
#include "utils.hpp"
#include "playergroup_classes.hpp"
#include "playergroups.hpp"
#include "structs.hpp"
#include "handler.hpp"
#include "invis_resistance_tests.hpp"
#include "constants.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "deck_build.hpp"
#include "newshop.hpp"
#include "bullet_pants.hpp"

// The linked list of loaded playergroups.
extern Playergroup *loaded_playergroups;

extern void _put_char_in_withdrawal(struct char_data *ch, int drug_id, bool is_guided);
extern bool player_is_dead_hardcore(long id);

extern void write_objs_to_disk(vnum_t zonenum);
extern void write_mobs_to_disk(vnum_t zonenum);
extern void weather_change();
extern bool ranged_response(struct char_data *combatant, struct char_data *ch);
extern void docwagon_retrieve(struct char_data *ch);
extern void convert_and_write_string_to_file(const char *str, const char *path);

#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
extern void verify_every_pointer_we_can_think_of();
#else
void verify_every_pointer_we_can_think_of() {}
#endif

// We're looking to verify that everything is kosher. Validate canaries, etc.
void verify_data(struct char_data *ch, const char *line, int cmd, int subcmd, const char *section) {
  // Called by a character doing something.
  if (ch) {
    // Check character's canaries.


    // Check their gear's canaries.

    // Check their descriptor's canaries.
  }

  // Called on a tick. This is a more thorough validation. If it fails, we know to look at the command logs in the last while.
  else {

  }
}

void do_pgroup_debug(struct char_data *ch, char *argument) {
  static char arg1[MAX_INPUT_LENGTH];
  static char arg2[MAX_INPUT_LENGTH];
  char *rest_of_argument = NULL;

  if (!*argument) {
    send_to_char(ch, "Syntax: DEBUG PGROUP <modes>\r\n");
    return;
  }

  // Extract the mode switch argument.
  rest_of_argument = any_one_arg(argument, arg1);
  rest_of_argument = any_one_arg(rest_of_argument, arg2);
  send_to_char(ch, "arg1: '%s', arg2: '%s'\r\n", arg1, arg2);

  if (*arg1 && strn_cmp(arg1, "list", strlen(arg1)) == 0) {
    if (!*arg2 && strn_cmp(arg2, "all", strlen(arg2) == 0)) {
      // List all pgroups, including those in the DB.
      send_to_char(ch, "not implemented yet.\r\n");
      return;
    } else if (strn_cmp(arg2, "loaded", strlen(arg2)) == 0) {
      // List only the pgroups that are currently loaded.
      send_to_char(ch, "Currently loaded playergroups:\r\n");
      for (Playergroup *pgr = loaded_playergroups; pgr; pgr = pgr->next_pgroup) {
        send_to_char(ch, "%ld) %s: %s (%s), %s, %s, %s, %s, bank %ld\r\n",
                     pgr->get_idnum(),
                     pgr->get_tag(),
                     pgr->get_name(),
                     pgr->get_alias(),
                     pgr->is_clone() ? "[CLONE]" : "[non-clone]",
                     pgr->is_disabled() ? "[DISABLED]" : "[enabled]",
                     pgr->is_founded() ? "[founded]" : "[provisional]",
                     pgr->is_secret() ? "[secret]" : "[non-secret]",
                     pgr->get_bank());
      }
    } else {
      send_to_char(ch, "unrecognized input '%s'\r\n", arg2);
      return;
    }
  } else {
    send_to_char(ch, "unrecognized input '%s'\r\n", arg2);
    return;
  }
}

extern void _get_negotiation_data_testable(
  int &tn,
  int &skill_dice,
  int kinesics_rating,
  int metavariant_penalty,
  int opponent_intelligence,
  int tn_modifier_from_get_skill,
  int skill_rating,
  int pheromone_dice
);

struct nego_test_values_struct {
  const char *name;
  int intelligence;
  int kinesics;
  int skill_dice;
  int enemy_int;
  int pheromones;
};

extern int get_eti_test_results(
  struct char_data *ch,
  int eti_skill,
  int availtn,
  int availoff,
  int kinesics,
  int meta_penalty,
  int lifestyle,
  int pheromone_dice,
  int skill_dice);

struct eti_test_values_struct {
  const char *name;
  int skill_dice;
  int kinesics;
  int pheromone_dice;
};

extern long payout_slots_testable(long bet);
extern void load_saved_veh(bool purge_existing);
extern void save_vehicles(bool);

extern bf::path global_vehicles_dir;

bool drinks_are_unfucked = TRUE;
ACMD(do_debug) {
  static char arg1[MAX_INPUT_LENGTH];
  static char arg2[MAX_INPUT_LENGTH];
  char *rest_of_argument = NULL;

  // It's a debug function-- let's normalize everything as much as we can.
  memset(arg1, 0, sizeof(arg1));
  memset(arg2, 0, sizeof(arg2));

  if (!*argument) {
    send_to_char("Syntax: DEBUG <mode> <arguments>\r\n", ch);
    send_to_char("Refer to src/debug.cpp for list of modes.\r\n", ch);
    return;
  }

  // Extract the mode switch argument.
  rest_of_argument = any_one_arg(argument, arg1);
  skip_spaces(&rest_of_argument);

  if (is_abbrev(arg1, "cleardriverflag")) {
    struct char_data *vict = get_char_vis(ch, rest_of_argument);
    if (vict) {
      PLR_FLAGS(vict).RemoveBit(PLR_COMPLETED_EXPERT_DRIVER_OVERHAUL);
      send_to_char(ch, "OK, removed %s's expert driver conversion bit.\r\n", GET_CHAR_NAME(vict));
    } else {
      send_to_char(ch, "You don't see anyone named %s.\r\n", rest_of_argument);
    }
    return;
  }

  if (!str_cmp(arg1, "wholistprint")) {
    buf2[0] = '\0';
    for (int i = 0; i < 250; i++) {
      strlcat(buf2, "^F[123]A^y l^bi^Cn^ye ^F[155]w^mi^yt^mh ^ct^oo^ln^Ws ^yo^Yf ^Rc^bo^Ml^eo^Rr^n\r\n", sizeof(buf2));
    }
    convert_and_write_string_to_file(buf2, "text/wholist");
    return;
  }

  if (!str_cmp(arg1, "reloadallvehicles")) {
    send_to_char("Reloading vehicles...\r\n", ch);
    bf::path old_path = bf::path(global_vehicles_dir);
    global_vehicles_dir = bf::system_complete("restore_vehicles");
    load_saved_veh(TRUE);
    global_vehicles_dir = bf::path(old_path);
    send_to_char(ch, "Global vehicles dir is now: %s. Saving vehicles.\r\n", global_vehicles_dir.c_str());
    save_vehicles(FALSE);
    send_to_char(ch, "Save complete.\r\n");
    return;
  }

  if (is_abbrev(arg1, "printvehmap")) {
    extern std::unordered_map<std::string, struct veh_data *> veh_map;

    for (auto& it: veh_map) {
      send_to_char(ch, "%s: %s\r\n", it.first.c_str(), GET_VEH_NAME(it.second));
    }
    return;
  }

  if (is_abbrev(arg1, "genwholist")) {
    send_to_char("OK, generating wholist file.\r\n", ch);
    ACMD_DECLARE(do_who);
    char dummy[5];
    *dummy = 0;
    do_who(ch, dummy, 1, 1);
    return;
  }

  if (is_abbrev(arg1, "hashtest")) {
    std::unordered_map<std::string, int> test_map = {};

    char test_str[100];
    for (int i = 0; i < 10; i++) {
      snprintf(test_str, sizeof(test_str), "this is %d", i);
      test_map[std::string(test_str)] = i;
      send_to_char(ch, "during iteration, at slot %d, we have %d\r\n", i, test_map[test_str]);
    }
    send_to_char(ch, "at slot 3, we have: %d\r\n", test_map[std::string("this is 3")]);
    return;
  }

  if (is_abbrev(arg1, "gambaodds")) {
    int starting_amt = 100;
    int max_bet = 150000;

    int num_profitable_runs = 0;
    int total_runs = 1000;

    for (int attempt = 0; attempt < total_runs; attempt++) {
      long net = 0;
      for (int iter = 0; iter < 30000; iter++) {
        for (int bet = starting_amt; bet < max_bet; bet *= 1.35) {
          net -= bet;
          long win_amt = payout_slots_testable(bet);
          net += win_amt;
          if (win_amt >= bet * 3) {
            break;
          }
        }
      }
      if (net > 0)
        num_profitable_runs++;
    }
    send_to_char(ch, "Had %d profitable runs out of %d (%.2f)", num_profitable_runs, total_runs, (float) num_profitable_runs / total_runs * 100);
    return;
  }

  if (is_abbrev(arg1, "stripcolor")) {
    const char *colorized = "^ccerulean ^yyellow ^rred^oand^Wwhite^n ^[F123]x^[F124]t^[F125]e^[F543]r^[F210]m";
    send_to_char(ch, "Stripping color from '%s^n' gives '%s'.\r\n", colorized, get_string_after_color_code_removal(colorized, ch));
    return;
  }

  if (is_abbrev(arg1, "ammotest")) {
    FAILURE_CASE(!access_level(ch, LVL_PRESIDENT), "no");
    // is it elegant? no.  does it answer questions? maybe.  does it work well? ...also no

    // Figure out the expected cost for various folks to make APDS ammo.
    int runs = 10000000;
    int weapon_type = WEAP_HMG;
    int selected_ammo_type = AMMO_APDS;

    int cost_per_attempted_10_rounds = get_ammo_cost(weapon_type, selected_ammo_type, AMMOBUILD_BATCH_SIZE, NULL);
    int target = ammo_type[selected_ammo_type].tn;

    send_to_char(ch, "OK, running a series of build tests across varied int levels, spending %d each time.\r\n", 
                 cost_per_attempted_10_rounds * runs);

    for (int dice = 12; dice <= 16; dice++) {
      long rounds_made = 0;
      for (int attempt = 0; attempt < runs; attempt++) {
        int success = success_test(dice, target);
        if (success > 0)
          rounds_made += AMMOBUILD_BATCH_SIZE;
      }
      send_to_char(ch, "With %2d dice, it cost us %.2f nuyen per %s.\r\n",
                   dice,
                   (float) cost_per_attempted_10_rounds * runs / rounds_made,
                   get_ammo_representation(weapon_type, selected_ammo_type, 1, NULL));
    }

    return;
  }

  if (is_abbrev(arg1, "etitest")) {
    int orig_num_runs = 15000;

    struct eti_test_values_struct eti_test_values[] =  {
      // name                        skill_dice  kin  phero_dice
      {"Arch-Cap Full Mage",                     8 ,       0,    4},
      {"Arch-Cap Aspected Mage",                 10,       0,    4},
      {"Uncapped Mage",                          11,       0,    4},
      {"Arch-Cap Mundane",                       12,       0,    4},
      {"Arch-Cap Adept",                         10,       3,    4},
      {"Uncapped Adept",                         11,       3,    4},
      {"", 0, 0, 0}
    };

    int lifestyle = LIFESTYLE_LUXURY;

    for (int idx = 0; *eti_test_values[idx].name; idx++) {
      send_to_char(ch, "%s:\r\n", eti_test_values[idx].name);
      for (int tn = 12; tn <= 24; tn += 6) {
        for (int availoff = 0; availoff <= 5; availoff += 5) {
          int total = 0;
          int num_runs = orig_num_runs * (tn / 8);
          for (int iter = 0; iter < num_runs; iter++) {
            int ch_s = get_eti_test_results(
              NULL,
              0,
              tn,
              availoff,
              eti_test_values[idx].kinesics,
              0,
              lifestyle,
              eti_test_values[idx].pheromone_dice,
              eti_test_values[idx].skill_dice);

            int net = ch_s;
            total += net;
          }
          float avg = ((float) total) / num_runs;
          send_to_char(ch, "%2d skill, %d kin, %d phero, %d availoff VS TN %d got %2.2f avg net succ (aka %2.2f tries).\r\n",
                      eti_test_values[idx].skill_dice,
                      eti_test_values[idx].kinesics,
                      eti_test_values[idx].pheromone_dice,
                      availoff,
                      tn,
                      avg,
                      1 / avg);
        }
      }
    }
    return;
  }

  if (is_abbrev(arg1, "negotest")) {
    FAILURE_CASE(!access_level(ch, LVL_PRESIDENT), "no");
    
    int num_runs = 10000;
    send_to_char(ch, "OK, testing negotiation averages with various scenarios.\r\n");
    /*
    send_to_char(ch, "First, figuring out the average max successes to cap out negotiation...\r\n");
    float buy_suc_avg = 0.0, sell_suc_avg = 0.0;
    {
      int suc_total = 0, suc_iter = 0;
      for (int basevalue = 1000; basevalue <= 100000000; basevalue *= 3) {
        for (int net_suc = 0; net_suc <= 10; net_suc++) {
          if (basevalue - (net_suc * (basevalue / 20)) <= (int)(basevalue * 3/4)) {
            // send_to_char(ch, "The most effective negotiation successes you can get at %d is %d.\r\n", basevalue, net_suc);
            suc_iter++;
            suc_total += net_suc;
            break;
          }
        }
      }
      buy_suc_avg = (float)suc_total / suc_iter;
    }
    {
      int suc_total = 0, suc_iter = 0;
      for (int basevalue = 1000; basevalue <= 100000000; basevalue *= 3) {
        for (int net_suc = 0; net_suc <= 10; net_suc++) {
          if (basevalue + (net_suc * (basevalue / 15)) >= (int)(basevalue * 5/4)) {
            // send_to_char(ch, "The most effective negotiation successes you can get at %d is %d.\r\n", basevalue, net_suc);
            suc_iter++;
            suc_total += net_suc;
            break;
          }
        }
      }
      sell_suc_avg = (float)suc_total / suc_iter;
    }
    send_to_char(ch, "The average max successes when purchasing is %.2f.\r\n", buy_suc_avg);
    send_to_char(ch, "The average max successes when selling is %.2f.\r\n", sell_suc_avg);
    */

    // Mage values.
    struct nego_test_values_struct nego_test_values[] = {
      // name                              int kin nego shop_int phero_dice
      {"Unbuffed mage w/ decrease int",      8, 0,  12,      1,   4},
      {"Adept",                              8, 3,  12,      8,   4},
      {"Current Mundane",                    8, 0,  12,      7,   4},
      {"Mundane w/ phero TN changes",        8, 1,  12,      7,   4},
      {"Mundane w/ phero dice + TN changes", 8, 1,  12,      7,   8},
      {"Current Mundane",                    8, 0,  12,      8,   4},
      {"Mundane w/ phero TN changes",        8, 1,  12,      8,   4},
      {"Mundane w/ phero dice + TN changes", 8, 1,  12,      8,   8},
      {"", 0, 0, 0, 0, 0}
    };

    for (int idx = 0; *nego_test_values[idx].name; idx++) {
      int total = 0;
      for (int iter = 0; iter < num_runs; iter++) {
        int ch_tn = 0, ch_dice = 0;
        int t_tn = 0, t_dice = 0;

        _get_negotiation_data_testable(ch_tn, ch_dice, nego_test_values[idx].kinesics, 0, nego_test_values[idx].enemy_int, 0, nego_test_values[idx].skill_dice, nego_test_values[idx].pheromones);
        _get_negotiation_data_testable(t_tn, t_dice, 0, 0, nego_test_values[idx].intelligence, 0, 8, 0);

        int ch_s = success_test(ch_dice, ch_tn);
        int t_s = success_test(t_dice, t_tn);

        int net = ch_s - t_s;
        total += net;
      }
      float avg = ((float) total) / num_runs;
      send_to_char(ch, "%s:\r\n%2d int, %d kin, %2d dice, vs int %d, got %2.2f avg net succ.",
                   nego_test_values[idx].name,
                   nego_test_values[idx].intelligence,
                   nego_test_values[idx].kinesics,
                   (nego_test_values[idx].skill_dice + nego_test_values[idx].pheromones),
                   nego_test_values[idx].enemy_int,
                   avg);
      if (avg >= 5.0) {
        send_to_char(ch, " (Buy: CAP)");
      } else {
        int basevalue = 1000;
        int discounted_price = basevalue - (avg * (basevalue / 20));
        float discount_pct = (1 - ((float) discounted_price / basevalue)) * 100;
        send_to_char(ch, " (Buy: %.0f%% disc)", discount_pct);
      }

      if (avg >= 4.0) {
        send_to_char(ch, " (Sell: CAP)\r\n");
      } else {
        int basevalue = 1000;
        int increased_price = basevalue + (avg * (basevalue / 15));
        float discount_pct = (1 - ((float) basevalue / increased_price)) * 100;
        send_to_char(ch, " (Sell: %.0f%% prof)\r\n", discount_pct);
      }
      send_to_char(ch, "\r\n");
    }

    return;
  }

  if (is_abbrev(arg1, "vision_penalties")) {
    send_to_char(ch, "OK, getting vision penalties.\r\n");

    char rbuf[10000] = { '\0' };
    int penalty = get_vision_penalty(ch, get_ch_in_room(ch), rbuf, sizeof(rbuf));
    send_to_char(ch, "...Got penalty %d. RBUF: %s\r\n", penalty, rbuf);

    *rbuf = 0;
    int tn = 2;
    int dice = get_skill(ch, SKILL_PILOT_FIXEDWING, tn, rbuf, sizeof(rbuf));
    send_to_char(ch, "...dice and tn after get_skill: %d, %d (%s)\r\n", dice, tn, rbuf);
    int successes = success_test(dice, tn, ch, "debug");
    send_to_char(ch, "...result %d\r\n", successes);
    return;
  }

  if (is_abbrev(arg1, "get_skill")) {
    send_to_char(ch, "OK, testing get_skill with and without pointers.\r\n");
    set_character_skill(ch, SKILL_PISTOLS, 0, FALSE);
    set_character_skill(ch, SKILL_RIFLES, 0, FALSE);
    set_character_skill(ch, SKILL_ASSAULT_RIFLES, 0, FALSE);
    set_character_skill(ch, SKILL_SHOTGUNS, 0, FALSE);
    set_character_skill(ch, SKILL_SMG, 5, FALSE);

    int first_tn = 4;
    get_skill(ch, SKILL_PISTOLS, first_tn);
    send_to_char(ch, "first_tn = %d\r\n", first_tn);

    int second_tn = 4;
    get_skill(ch, SKILL_PISTOLS, second_tn);
    send_to_char(ch, "second_tn = %d\r\n", second_tn);
    return;
  }

  if (is_abbrev(arg1, "traceback")) {
    send_to_char(ch, "OK, printing traceback.\r\n");
    log_traceback("manual invocation via debug by %s", GET_CHAR_NAME(ch));
    return;
  }

  if (is_abbrev(arg1, "hardcorecheck")) {
    if (player_is_dead_hardcore(57)) {
      send_to_char(ch, "Success!\r\n");
    } else {
      send_to_char(ch, "Failure :(\r\n");
    }
    return;
  }

  if (is_abbrev(arg1, "dirtybit")) {
    int clean = 0, dirty = 0;
    int clean_storage = 0, dirty_storage = 0;
    for (rnum_t x = 0; x <= top_of_world; x++) {
      if (world[x].dirty_bit) {
        if (ROOM_FLAGGED(&world[x], ROOM_STORAGE))
          dirty_storage++;
        else
          dirty++;
      } else {
        if (ROOM_FLAGGED(&world[x], ROOM_STORAGE))
          clean_storage++;
        else
          clean++;
      }
    }
    send_to_char(ch, "%d of %d rooms are dirty.\r\n", dirty, dirty + clean);
    send_to_char(ch, "%d of %d storage rooms are dirty.\r\n", dirty_storage, dirty_storage + clean_storage);
    return;
  }

  if (strn_cmp(arg1, "pgroups", strlen(arg1)) == 0) {
    do_pgroup_debug(ch, rest_of_argument);
    return;
  }

  if (strn_cmp(arg1, "partslist", strlen(arg1)) == 0) {
    if (!global_in_progress_deck_parts.size()) {
      send_to_char("The global parts list is empty.\r\n", ch);
      return;
    }
    for (auto it : global_in_progress_deck_parts) {
      send_to_char(ch, "%50s^n   %s^n\r\n", GET_OBJ_NAME(it), it->cyberdeck_part_pointer ? GET_OBJ_NAME(it->cyberdeck_part_pointer) : "^r<null>");
    }
    return;
  }

  if (is_abbrev(arg1, "withdraw")) {
    send_to_char(ch, "OK, processing withdrawal.\r\n");

    for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
      // Calculate time since last fix.
      GET_DRUG_LAST_FIX(ch, drug_id) -= SECS_PER_MUD_DAY;
    }

    process_withdrawal(ch);
    return;
  }

  if (is_abbrev(arg1, "generic_find")) {
    const char *find_x_index_names[] = {
      "FIND_CHAR_ROOM    ",
      "FIND_CHAR_WORLD   ",
      "FIND_OBJ_INV      ",
      "FIND_OBJ_ROOM     ",
      "FIND_OBJ_WORLD    ",
      "FIND_OBJ_EQUIP    ",
      "FIND_OBJ_VEH_ROOM ",
      "FIND_CHAR_VEH_ROOM"
    };

    for (int i = 0; i < NUM_FIND_X_BITS; i++) {
      struct obj_data *tmp_object = NULL;
      struct char_data *tmp_char = NULL;

      generic_find(rest_of_argument, 1 << i, ch, &tmp_char, &tmp_object);
      send_to_char(ch, "%s: %s, %s\r\n", find_x_index_names[i], GET_CHAR_NAME(tmp_char), GET_OBJ_NAME(tmp_object));
    }
  }

  if (is_abbrev(arg1, "weather")) {
    rest_of_argument = any_one_arg(rest_of_argument, arg2);

    if (is_abbrev(arg2, "change")) {
      int old_change = weather_info.change;
      int old_pressure = weather_info.pressure;
      int old_sky = weather_info.sky;
      weather_change();
      send_to_char(ch, "Weather: %d -> %d, pressure: %d -> %d, sky: %d -> %d.\r\n",
                   old_change, weather_info.change,
                   old_pressure, weather_info.pressure,
                   old_sky, weather_info.sky);
    }
    else {
      send_to_char(ch, "Valid option: WEATHER CHANGE, not '%s'.\r\n", arg2);
    }

    return;
  }

  if (is_abbrev(arg1, "docwagon")) {
    PLR_FLAGS(ch).SetBit(PLR_DOCWAGON_READY);
    docwagon_retrieve(ch);
    return;
  }

  if (is_abbrev(arg1, "snapback")) {
    rest_of_argument = any_one_arg(rest_of_argument, arg2);

    // Scenario 1: Respond aggressively to someone who was docwagon'd.
    if (is_abbrev(arg2, "1")) {
      // Mitsuhama Elite guard, in the elite's room.
      struct char_data *combatant = read_mobile(17114, VIRTUAL);
      char_to_room(combatant, &world[real_room(17172)]);
      combatant->mob_loaded_in_room = 17172;

      // Disarm them so they're forced to close the distance.
      if (GET_EQ(combatant, WEAR_WIELD))
        extract_obj(unequip_char(combatant, WEAR_WIELD, FALSE));

      act("$n: Loaded.", FALSE, combatant, 0, 0, TO_ROLLS);

      // Move us to the room SW of that.
      char_from_room(ch);
      char_to_room(ch, &world[real_room(17126)]);

      verify_every_pointer_we_can_think_of();
      act("$n: Present 1.", FALSE, combatant, 0, 0, TO_ROLLS);

      // Force them to ranged_response us.
      ranged_response(combatant, ch);

      verify_every_pointer_we_can_think_of();
      act("$n: Present 2.", FALSE, combatant, 0, 0, TO_ROLLS);

      // Docwagon us out.
      PLR_FLAGS(ch).SetBit(PLR_DOCWAGON_READY);
      docwagon_retrieve(ch);

      verify_every_pointer_we_can_think_of();
      act("$n: Present 3.", FALSE, combatant, 0, 0, TO_ROLLS);

      // Make them respond again, which should snap them back.
      ranged_response(combatant, ch);

      verify_every_pointer_we_can_think_of();
      act("$n: Present 4.", FALSE, combatant, 0, 0, TO_ROLLS);

      // Force them to respond again, which should not make them go anywhere.
      ranged_response(combatant, ch);

      verify_every_pointer_we_can_think_of();
      act("$n: Present 5.", FALSE, combatant, 0, 0, TO_ROLLS);

      // Remove our combatant.
      extract_char(combatant);

      verify_every_pointer_we_can_think_of();
    }
    return;
  }

  if (!str_cmp(arg1, "idledeletechar")) {
    extern MYSQL *mysql;

    if (GET_LEVEL(ch) != LVL_PRESIDENT) {
      send_to_char("Sorry, only the game owner can do this.\r\n", ch);
      return;
    }

    int idnum_int = atoi(rest_of_argument);

    if (idnum_int <= 0) {
      send_to_char(ch, "Syntax is 'idledeletechar <idnum>'\r\n");
      return;
    }

    snprintf(buf, sizeof(buf), "UPDATE pfiles SET lastd=0 WHERE idnum=%d", idnum_int);
    mysql_wrapper(mysql, buf);

    send_to_char(ch, "Done - relog the char if you need to fix them, or use SET FILE.\r\n");
    return;
  }

  if (strn_cmp(arg1, "std-map", strlen(arg1)) == 0) {
    std::unordered_map<std::string, bool> seen_names = {};

    const char *first_a = str_dup("first");
    const char *first_b = str_dup("first");
    const char *second = "second";

    std::string str_a = first_a;
    std::string str_b = first_b;
    std::string str_s = second;

    seen_names.emplace(str_a, TRUE);
    seen_names.emplace(str_b, TRUE);
    seen_names.emplace(second, TRUE);

    send_to_char(ch, "After emplacement, map has %ld elements. first_a is%s in it, first_b is%s in it, second is%s in it.\r\n",
                     seen_names.size(),
                     seen_names.find(first_a) == seen_names.end() ? " NOT" : "",
                     seen_names.find(first_b) == seen_names.end() ? " NOT" : "",
                     seen_names.find(second) == seen_names.end() ? " NOT" : ""
                   );

    delete [] first_a;
    delete [] first_b;
    return;
  }

  if (str_str(arg1, "invalidate_cache")) {
    global_existing_player_cache.clear();
    send_to_char(ch, "OK, invalidated the global character cache. Characters will be looked up again.");
    return;
  }

  if (strn_cmp(arg1, "set-perception", strlen(arg1)) == 0) {
    send_to_char(ch, "Alright, setting perception flag on every magical mob that's not a spirit or elemental.\r\n");
    for (rnum_t rnum = 0; rnum <= top_of_mobt; rnum++) {
      struct char_data *mob = &mob_proto[rnum];

      // Skip non-magical mobs.
      if (GET_MAG(mob) <= 0) {
        continue;
      }

      // Skip spirits and elementals.
      if (IS_ANY_ELEMENTAL(mob) || IS_SPIRIT(mob)) {
        continue;
      }

      // If they are dual, remove that flag and replace it with perception.
      if (MOB_FLAGGED(mob, MOB_DUAL_NATURE)) {
        send_to_char(ch, "^GSet %s (%ld) to perceive.^n\r\n", GET_CHAR_NAME(mob), GET_MOB_VNUM(mob));
        MOB_FLAGS(mob).RemoveBit(MOB_DUAL_NATURE);
        MOB_FLAGS(mob).SetBit(MOB_PERCEIVING);
      } else {
        send_to_char(ch, "^YSkip %s (%ld): not dual^n\r\n", GET_CHAR_NAME(mob), GET_MOB_VNUM(mob));
      }
    }

    for (rnum_t idx = 0; idx <= top_of_zone_table; idx++) {
      write_mobs_to_disk(zone_table[idx].number);
    }

    return;
  }

  if (strn_cmp(arg1, "eqdam", strlen(arg1)) == 0) {
    send_to_char(ch, "OK, destroying your equipment.\r\n");
    extern void damage_obj(struct char_data *ch, struct obj_data *obj, int power, int type);
    for (int wearloc = 0; wearloc < NUM_WEARS; wearloc++) {
      struct obj_data *eq = GET_EQ(ch, wearloc);
      if (eq) {
        damage_obj(ch, eq, 100, TYPE_ACID);
      }
    }
    return;
  }

  if (is_abbrev(arg1, "replace_word")) {
    {
      char input[21], output[10], replacement[10];
      strlcpy(replacement, "1234", sizeof(replacement));
      strlcpy(input, "12345678901234567890", sizeof(input));
      replace_word(input, output, sizeof(output), replacement, "xxxxxx");
      send_to_char(ch, "Replacement of %s in %s yielded %s\r\n", replacement, input, output);
    }

    {
      char input[21], output[100];
      strlcpy(input, "test emote, $N", sizeof(input));
      replace_word(input, output, sizeof(output), "$N", "@Lucien");
      send_to_char(ch, "Replacement of $N in %s yielded %s\r\n", input, output);
    }

    {
      char input[30], output[100];
      strlcpy(input, "$N: test emote, $N!", sizeof(input));
      replace_word(input, output, sizeof(output), "$N", "@Lucien");
      send_to_char(ch, "Replacement of $N in %s yielded %s\r\n", input, output);
    }

    {
      char input[1000], output[1000];
      strlcpy(input, "@self salvages a scrap of paper from the detritus and scribbles a quick note. "
                     "\"Could you take this to Natalia downstairs, please? It's just letting her know "
                     "what happened here and that I'll handle the repairs and whatnot.\" She hands $N "
                     "the note with a tiny shrug, adding, \"After all she's done for me, I don't want "
                     "to put her out any more than I have to.\"", sizeof(input));
      replace_word(input, output, sizeof(output), "$N", "@Lucien");
      send_to_char(ch, "Replacement yielded: %s\r\n", output);
    }

    return;
  }

  if (!str_cmp(arg1, "unfuckdrinks") && access_level(ch, LVL_PRESIDENT) && !drinks_are_unfucked) {
    drinks_are_unfucked = TRUE;
    char buf[500];

    send_to_char(ch, "Unfucking drinks. Hope you know what you're doing.\r\n");
    mudlog("^RTHE GREAT UNFUCKENING HAS BEGUN.^g  Will log each changed drink.", ch, LOG_WIZLOG, TRUE);

    for (rnum_t idx = 0; idx <= top_of_objt; idx++) {
      struct obj_data *obj = &obj_proto[idx];

      if (GET_OBJ_TYPE(obj) == ITEM_DRINKCON || GET_OBJ_TYPE(obj) == ITEM_FOUNTAIN) {
        if (GET_DRINKCON_LIQ_TYPE(obj) >= LIQ_EVERCLEAR) {
          int old_liq_type = GET_DRINKCON_LIQ_TYPE(obj)++;
          snprintf(buf, sizeof(buf), "Converted '%s^g' (%ld) from %s to %s.",
                   GET_OBJ_NAME(obj),
                   GET_OBJ_VNUM(obj),
                   drinks[old_liq_type],
                   drinks[GET_DRINKCON_LIQ_TYPE(obj)]);
          mudlog(buf, ch, LOG_SYSLOG, TRUE);
        }
      }
    }
    mudlog("Unfuckening complete. Saving all objects to disk...", ch, LOG_SYSLOG, TRUE);

    for (rnum_t idx = 0; idx <= top_of_zone_table; idx++) {
      write_objs_to_disk(zone_table[idx].number);
    }

    mudlog("Done.", ch, LOG_SYSLOG, TRUE);
    return;
  }

  if (strn_cmp(arg1, "maxfunc", strlen(arg1)) == 0) {
    send_to_char(ch, "MAX(-1, 1) = %d", MAX(-1, 1));
    return;
  }

  if (access_level(ch, LVL_PRESIDENT) && is_abbrev(arg1, "invis")) {
    can_see_through_invis(ch, ch);
    return;
  }

  if (access_level(ch, LVL_PRESIDENT) && is_abbrev(arg1, "drugs")) {
    int drug_id = DRUG_KAMIKAZE;

    send_to_char(ch, "OK, setting you as addicted to %s and putting you just before withdrawal starts.\r\n", drug_types[drug_id].name);
    PLR_FLAGS(ch).SetBit(PLR_ENABLED_DRUGS);

    GET_DRUG_ADDICT(ch, drug_id) = IS_ADDICTED;
    GET_DRUG_DOSE(ch, drug_id) = 0;
    GET_DRUG_ADDICTION_EDGE(ch, drug_id) = 3;
    GET_DRUG_TOLERANCE_LEVEL(ch, drug_id) = 3;

    GET_DRUG_STAGE(ch, drug_id) = DRUG_STAGE_UNAFFECTED;
    GET_DRUG_LAST_FIX(ch, drug_id) = time(0) - (60 * 60 * 24 * 90);
    return;
  }

  if (access_level(ch, LVL_PRESIDENT) && is_abbrev(arg1, "telnet")) {
    char on_string[] =
    {
      (char) IAC,
      (char) WONT,
      (char) TELOPT_ECHO,
      (char) 0,
    };

    char off_string[] =
    {
      (char) IAC,
      (char) WILL,
      (char) TELOPT_ECHO,
      (char) 0,
    };

    SEND_TO_Q(on_string, ch->desc);
    SEND_TO_Q(off_string, ch->desc);
    SEND_TO_Q(on_string, ch->desc);
    SEND_TO_Q(off_string, ch->desc);

    return;
  }

  if (access_level(ch, LVL_PRESIDENT) && strn_cmp(arg1, "swimcheck", strlen(arg1)) == 0) {
    if (!ch->in_room) {
      send_to_char(ch, "You must be in a room to do that.\r\n");
      return;
    }

    send_to_char(ch, "Calculating swim success rates. This will take a bit.\r\n");

    void analyze_swim_successes(struct char_data *temp_char);
    analyze_swim_successes(ch);

    send_to_char(ch, "Done.\r\n");
  }

  if (strn_cmp(arg1, "dice", strlen(arg1)) == 0) {
    skip_spaces(&rest_of_argument);
    rest_of_argument = any_one_arg(rest_of_argument, arg2);

    int dice_to_roll = atoi(arg2);
    int tn = atoi(rest_of_argument);

    if (dice_to_roll == 0 || tn == 0) {
      send_to_char(ch, "Got dice=%d and tn=%d. Syntax: debug dice <number of d6 to roll> <tn>.\r\n", dice_to_roll, tn);
      return;
    }

    /*
    int successes[dice_to_roll + 1];
    memset(successes, 0, sizeof(successes));
    */

    int successes = 0;
    int hits = 0;

    int botches = 0;
    int number_of_rolls_to_do = 1000000;
    int required_hits_to_pass = 1;

    for (int i = 0; i < number_of_rolls_to_do; i++) {
      int rolled = success_test(dice_to_roll, tn);
      if (rolled < 0)
        botches++;
      else if (rolled >= required_hits_to_pass) {
        successes++;
        hits += rolled;
      }
    }

    send_to_char(ch, "Rolled %d successes and %d botches. Roll success rate for %dd6 at TN %d is %.02f%%. Average successes per roll: %.03f.\r\n",
                 successes,
                 botches,
                 dice_to_roll,
                 tn,
                 (((float) successes) / number_of_rolls_to_do) * 100,
                 ((float) hits / (dice_to_roll * number_of_rolls_to_do)) * dice_to_roll
               );
    return;
  }

  if (access_level(ch, LVL_PRESIDENT) && strn_cmp(arg1, "void", strlen(arg1)) == 0) {
    skip_spaces(&rest_of_argument);
    struct obj_data *obj = get_obj_in_list_vis(ch, rest_of_argument, ch->carrying);
    if (obj) {
      send_to_char(ch, "OK, now sending %s beyond time and space...\r\n", GET_OBJ_NAME(obj));
      obj->in_room = NULL;
      obj->in_veh = NULL;
      obj->carried_by = NULL;
      obj->worn_by = NULL;
      obj->in_obj = NULL;
      send_to_char(ch, "Done. %s has had all of its location pointers wiped. The game will now crash when your character is extracted. Enjoy!\r\n", GET_OBJ_NAME(obj));
    } else {
      send_to_char(ch, "You don't seem to have anything called '%s' in your inventory.\r\n", rest_of_argument);
    }
  }

  if (access_level(ch, LVL_PRESIDENT) && strn_cmp(arg1, "metapurge", strlen(arg1)) == 0) {
    for (int i = 0; i < META_MAX; i++) {
      SET_METAMAGIC(ch, i, 0);
    }
    send_to_char("Done-- all your metamagics are zeroed.\r\n", ch);
    return;
  }

  if (access_level(ch, LVL_BUILDER) && strn_cmp(arg1, "ansicolor-known", strlen(arg1)) == 0) {
    int old_val = 0;
    if (ch->desc && ch->desc->pProtocol && ch->desc->pProtocol->pVariables[eMSDP_XTERM_256_COLORS] && ch->desc->pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt) {
      old_val = ch->desc->pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt;
      ch->desc->pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 0;
    }
    ch->desc->pProtocol->pVariables[eMSDP_ANSI_COLORS]->ValueInt = 1;

    const char shit_color[] = "^M";

    send_to_char(ch, "The following destinations are available:\r\n");
    send_to_char(ch, "^BSculpt               [-61142, 30671 ](Available)\r\n");
    send_to_char(ch, "^rPortbus              [-29248, 14724 ](Unavailable)\r\n");
    send_to_char(ch, "^rGate                 [-35198, 17699 ](Unavailable)\r\n");
    send_to_char(ch, "^BLewis                [-16216, 8208  ](Available)\r\n");
    send_to_char(ch, "^rHome                 [-125800, 63000 ](Unavailable)\r\n");
    send_to_char(ch, "^BCourier              [-58616, 29408 ](Available)\r\n");
    send_to_char(ch, "^BCouncil              [-18310, 9255  ](Available)\r\n");
    send_to_char(ch, "^BJack                 [-4040 , 2120  ](Available)\r\n");
    send_to_char(ch, "^BRedmond              [-39726, 19963 ](Available)\r\n");
    send_to_char(ch, "^BBic                  [-78570, 39385 ](Available)\r\n");
    send_to_char(ch, "^rH21                  [-16270, 8235  ](Unavailable)\r\n");
    send_to_char(ch, "^rVanc                 [-35186, 17693 ](Unavailable)\r\n");
    send_to_char(ch, "^BKnight               [-3038 , 1619  ](Available)\r\n");
    send_to_char(ch, "^BTville               [-50624, 25412 ](Available)\r\n");
    send_to_char(ch, "^rPuyallup             [-4806 , 2503  ](Unavailable)\r\n");
    send_to_char(ch, "%sSlewis               [-16222, 8211  ](Unavailable)\r\n", shit_color);
    send_to_char(ch, "^rJval                 [-35096, 17648 ](Unavailable)\r\n");
    send_to_char(ch, "^rH229                 [-35076, 17638 ](Unavailable)\r\n");
    send_to_char(ch, "^rNgate                [-35196, 17698 ](Unavailable)\r\n");
    send_to_char(ch, "1 Entries remaining.\r\n");

    if (old_val) {
      ch->desc->pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = old_val;
    }
  }

  if (access_level(ch, LVL_BUILDER) && strn_cmp(arg1, "ansicolor-experiment", strlen(arg1)) == 0) {
    int old_val = 0;
    if (ch->desc && ch->desc->pProtocol && ch->desc->pProtocol->pVariables[eMSDP_XTERM_256_COLORS] && ch->desc->pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt) {
      old_val = ch->desc->pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt;
      ch->desc->pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 0;
    }
    ch->desc->pProtocol->pVariables[eMSDP_ANSI_COLORS]->ValueInt = 1;

    const char shit_color[] = "^M";

    send_to_char(ch, "The following destinations are available:\r\n");
    send_to_char(ch, "^BSculpt               [-61142, 30671 ](Available)\r\n");
    send_to_char(ch, "^rPortbus              [-29248, 14724 ](Unavailable)\r\n");
    send_to_char(ch, "^rGate                 [-35198, 17699 ](Unavailable)\r\n");
    send_to_char(ch, "^BLewis                [-16216, 8208  ](Available)\r\n");
    send_to_char(ch, "^rHome                 [-125800, 63000 ](Unavailable)\r\n");
    send_to_char(ch, "^BCourier              [-58616, 29408 ](Available)\r\n");
    send_to_char(ch, "^BCouncil              [-18310, 9255  ](Available)\r\n");
    send_to_char(ch, "^BJack                 [-4040 , 2120  ](Available)\r\n");
    send_to_char(ch, "^BRedmond              [-39726, 19963 ](Available)\r\n");
    send_to_char(ch, "^BBic                  [-78570, 39385 ](Available)\r\n");
    send_to_char(ch, "^rH21                  [-16270, 8235  ](Unavailable)\r\n");
    send_to_char(ch, "^rVanc                 [-35186, 17693 ](Unavailable)\r\n");
    send_to_char(ch, "^BKnight               [-3038 , 1619  ](Available)\r\n");
    send_to_char(ch, "^BTville               [-50624, 25412 ](Available)\r\n");
    send_to_char(ch, "^rPuyallup             [-4806 , 2503  ](Unavailable)\r\n");
    send_to_char(ch, "%sSlewis               [-16222, 8211  ](Unavailable)\r\n", shit_color);
    send_to_char(ch, "^r..................................................\r\n");
    send_to_char(ch, "^r..................................................\r\n");
    send_to_char(ch, "^r..................................................\r\n");
    send_to_char(ch, "1 Entries remaining.\r\n");

    if (old_val) {
      ch->desc->pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = old_val;
    }
  }

#ifdef IS_BUILDPORT
  SPECIAL(shop_keeper);
  if (access_level(ch, LVL_ADMIN) && strn_cmp(arg1, "fixshopint", strlen(arg1)) == 0) {
    send_to_char(ch, "OK, rewriting the following shopkeepers to have a max of 8 int:\r\n");
    for (struct char_data *mob = character_list; mob; mob = mob->next_in_character_list) {
      int max_allowed = 8;
      if (MOB_HAS_SPEC(mob, shop_keeper) && GET_REAL_INT(mob) > max_allowed) {
        // Only show it on shops that negotiate.
        bool shop_negotiates = FALSE;
        for (int idx = 0; idx <= top_of_shopt; idx++) {
          if (shop_table[idx].keeper == GET_MOB_VNUM(mob)) {
            shop_negotiates = !shop_table[idx].flags.IsSet(SHOP_WONT_NEGO);
            break;
          }
        }

        if (shop_negotiates) {
          send_to_char(ch, "  [^c%6ld^n] %s (was %d, now 8)\r\n", 
                       GET_MOB_VNUM(mob),
                       GET_CHAR_NAME(mob),
                       GET_REAL_INT(mob));
          GET_REAL_INT(mob) = max_allowed;
          write_mobs_to_disk(get_zone_from_vnum(GET_MOB_VNUM(mob))->number);
        }
      }
    }
  }
#endif
}
