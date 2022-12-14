/************************************************************************
 * Code for the DEBUG command, which invokes various testing protocols. *
 * Written for AwakeMUD Community Edition by Lucien Sadi.     07/01/19  *
 ************************************************************************/

#include <time.h>
#include <string.h>
#include <mysql/mysql.h>

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

// The linked list of loaded playergroups.
extern Playergroup *loaded_playergroups;

extern void _put_char_in_withdrawal(struct char_data *ch, int drug_id, bool is_guided);
extern bool player_is_dead_hardcore(long id);

extern void write_objs_to_disk(vnum_t zonenum);
extern void write_mobs_to_disk(vnum_t zonenum);

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

bool drinks_are_unfucked = FALSE;
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

  if (is_abbrev(arg1, "hardcorecheck")) {
    if (player_is_dead_hardcore(57)) {
      send_to_char(ch, "Success!\r\n");
    } else {
      send_to_char(ch, "Failure :(\r\n");
    }
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

  if (is_abbrev(arg1, "withdraw")) {
    send_to_char(ch, "OK, processing withdrawal.\r\n");

    for (int drug_id = MIN_DRUG; drug_id < NUM_DRUGS; drug_id++) {
      // Calculate time since last fix.
      GET_DRUG_LAST_FIX(ch, drug_id) -= SECS_PER_MUD_DAY;
    }

    process_withdrawal(ch);
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

    send_to_char(ch, "After emplacement, map has %d elements. first_a is%s in it, first_b is%s in it, second is%s in it.\r\n",
                     seen_names.size(),
                     seen_names.find(first_a) == seen_names.end() ? " NOT" : "",
                     seen_names.find(first_b) == seen_names.end() ? " NOT" : "",
                     seen_names.find(second) == seen_names.end() ? " NOT" : ""
                   );

    delete [] first_a;
    delete [] first_b;
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
          snprintf(buf, sizeof(buf), "Converted '%s^g' (%ld) from %s to %s.",
                   GET_OBJ_NAME(obj),
                   GET_OBJ_VNUM(obj),
                   drinks[GET_DRINKCON_LIQ_TYPE(obj)],
                   drinks[++GET_DRINKCON_LIQ_TYPE(obj)]);
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
}
