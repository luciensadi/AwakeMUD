#include <unordered_map>
#include <vector>

#include "structs.hpp"
#include "db.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "newdb.hpp"

long total_amount_removed_from_economy_by_slots = 0;

std::unordered_map<idnum_t, long> gamba_ledger = {};

const char *slot_icons[] {
  "^WCredstick",
  "^BNuyen",
  "^L9mm Bullet",
  "^WDiamond",
  "^YLemon",
  "^OHorseshoe",
  "^CBell",
  "^RHeart",
  "^GClover",
  "^MCrown",
  "^[F533]Peach"   // In honor of Momo getting absolutely booty-blasted by the slot machines the day he found out about them
};
#define RESERVED_SLOT_END 3
#define NUM_SLOT_ICONS    11
#define SLOT_PEACH        10

void add_gambling_ledger_entry(struct char_data *ch, int amount) {
  try {
    gamba_ledger.at(GET_IDNUM(ch)) += amount;
  } catch (std::out_of_range) {
    gamba_ledger[GET_IDNUM(ch)] = amount;
  }
}

bool _gamba_cmp_wins(std::pair<idnum_t, long>& a, std::pair<idnum_t, long>& b) {
  return a.second > b.second;
}

bool _gamba_cmp_losses(std::pair<idnum_t, long>& a, std::pair<idnum_t, long>& b) {
  return a.second < b.second;
}

void display_gamba_ledger_leaderboard(struct char_data *ch) {
  std::vector<std::pair<idnum_t, long>> pair_vect = {}; 

  if (gamba_ledger.empty()) {
    send_to_char("Nobody has any gambling wins or losses yet.\r\n", ch);
    return;
  }

  // Build pair_vect with wins.
  for (auto& it : gamba_ledger) {
    if (it.second > 0)
      pair_vect.push_back(it); 
  }

  send_to_char("Top 3 gamblers by wins:\r\n", ch);
  std::sort(pair_vect.begin(), pair_vect.end(), _gamba_cmp_wins);
  
  int printed = 0;
  for (auto& it : pair_vect) {
    if (printed++ == 3)
      break;

    char *name = get_player_name(it.first);
    if (!str_cmp(name, CHARACTER_DELETED_NAME_FOR_SQL)) {
      printed--;
    } else {
      send_to_char(ch, "%20s:  ^c%ld^n\r\n", name, it.second);
    }
    delete [] name;
  }

  // Wipe and rebuild pair vect with losses.
  pair_vect.clear();
  for (auto& it : gamba_ledger) {
    if (it.second < 0)
      pair_vect.push_back(it); 
  }

  send_to_char("\r\nTop 3 gamblers by losses:\r\n", ch);
  std::sort(pair_vect.begin(), pair_vect.end(), _gamba_cmp_losses);
  
  printed = 0;
  for (auto& it : pair_vect) {
    if (printed++ == 3)
      break;

    char *name = get_player_name(it.first);
    if (!str_cmp(name, CHARACTER_DELETED_NAME_FOR_SQL)) {
      printed--;
    } else {
      send_to_char(ch, "%20s:  ^c%ld^n\r\n", name, it.second);
    }
    delete [] name;
  }
}

void payout_slots(struct obj_data *slots) {
  if (!slots->in_room) {
    mudlog("SYSERR: Entered payout_slots() with a slot machine that has no room!", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  int rolled = number(0, 12501);
  int payout_multiplier;

  if (rolled == 1) {
    // 3 Credstick
    payout_multiplier = 60;
    snprintf(buf, sizeof(buf), "There's an electronic fanfare before %s lights up with a brilliant glow, displaying a perfect three-^WCredstick^n line!\r\n", decapitalize_a_an(GET_OBJ_NAME(slots)));
  } else if (rolled <= 4) {
    // 3 Nuyen
    payout_multiplier = 40;
    snprintf(buf, sizeof(buf), "%s dings and whistles loudly before displaying the extremely rare set of three ^Bstacks of nuyen^n!\r\n", CAP(GET_OBJ_NAME(slots)));
  } else if (rolled <= 40) {
    // 3 Cherries
    payout_multiplier = 20;
    snprintf(buf, sizeof(buf), "%s chimes a cheery melody, showing a rare set of three ^PCherries^n!\r\n", CAP(GET_OBJ_NAME(slots)));
  } else if (rolled <= 580) {
    // 3 of Any Other
    payout_multiplier = 10;
    int slot_idx = number(RESERVED_SLOT_END, NUM_SLOT_ICONS - 1);

    snprintf(buf, sizeof(buf), "%s flashes celebratory lights as it displays a trio of %s%s^n!\r\n",
             CAP(GET_OBJ_NAME(slots)),
             slot_icons[slot_idx],
             slot_idx == SLOT_PEACH ? "es" : "s");
  } else if (rolled <= 1231) {
    // Any 2 Cherries
    payout_multiplier = 3;
    switch (number(0, 2)) {
      case 0:
        snprintf(buf, sizeof(buf), "%s glows happily as it displays ^W[ %s ^W| ^PCherry^n ^W| ^PCherry^n ^W]^n.\r\n",
                 CAP(GET_OBJ_NAME(slots)),
                 slot_icons[number(0, NUM_SLOT_ICONS - 1)]);
        break;
      case 1:
        snprintf(buf, sizeof(buf), "%s glows happily as it displays ^W[ ^PCherry^n ^W| %s ^W| ^PCherry^n ^W]^n.\r\n",
                 CAP(GET_OBJ_NAME(slots)),
                 slot_icons[number(0, NUM_SLOT_ICONS - 1)]);
        break;
      default:
        snprintf(buf, sizeof(buf), "%s glows happily as it displays ^W[ ^PCherry^n ^W| ^PCherry^n ^W| %s ^W]^n.\r\n",
                 CAP(GET_OBJ_NAME(slots)),
                 slot_icons[number(0, NUM_SLOT_ICONS - 1)]);
        break;
    }
  } else if (rolled <= 5111) {
    // Any 1 Cherry
    payout_multiplier = 1;
    switch (number(0, 2)) {
      case 0:
        snprintf(buf, sizeof(buf), "%s glows happily as it displays ^W[ ^PCherry^n ^W| %s ^W| %s ^W]^n.\r\n",
                 CAP(GET_OBJ_NAME(slots)),
                 slot_icons[number(0, NUM_SLOT_ICONS - 1)],
                 slot_icons[number(0, NUM_SLOT_ICONS - 1)]);
        break;
      case 1:
        snprintf(buf, sizeof(buf), "%s glows happily as it displays ^W[ %s ^W| ^PCherry^n ^W| %s ^W]^n.\r\n",
                 CAP(GET_OBJ_NAME(slots)),
                 slot_icons[number(0, NUM_SLOT_ICONS - 1)],
                 slot_icons[number(0, NUM_SLOT_ICONS - 1)]);
        break;
      default:
        snprintf(buf, sizeof(buf), "%s glows happily as it displays ^W[ %s ^W| %s ^W| ^PCherry^n ^W]^n.\r\n",
                 CAP(GET_OBJ_NAME(slots)),
                 slot_icons[number(0, NUM_SLOT_ICONS - 1)],
                 slot_icons[number(0, NUM_SLOT_ICONS - 1)]);
        break;
    }
  } else if (rolled == 0) {
    // 3 9mms, hehe
    payout_multiplier = 1;
    snprintf(buf, sizeof(buf), "With an ominous sting of music, %s lights up red, revealing three ^L9mm bullets^n lined up on the screen.\r\n"
             "^rA hatch on the front flips open with a bang, revealing the barrel of a built-in pistol!^n\r\n",
             decapitalize_a_an(GET_OBJ_NAME(slots)));
  } else {
    // No hits, display a random set of non-matching reels.
    payout_multiplier = 0;

    int first = number(0, NUM_SLOT_ICONS - 1), second = number(0, NUM_SLOT_ICONS - 1), third = number(0, NUM_SLOT_ICONS - 1);
    while (first == second && second == third) {
      first = number(0, NUM_SLOT_ICONS - 1);
      second = number(0, NUM_SLOT_ICONS - 1);
      third = number(0, NUM_SLOT_ICONS - 1);
    }

    snprintf(buf, sizeof(buf), "%s bleeps sympathetically as it displays ^W[ %s ^W| %s ^W| %s ^W]^n.\r\n",
                 CAP(GET_OBJ_NAME(slots)),
                 slot_icons[first],
                 slot_icons[second],
                 slot_icons[third]);
  }
  send_to_room(buf, slots->in_room);

  int amount_to_pay = GET_SLOTMACHINE_LAST_SPENT(slots) * payout_multiplier;

  if (amount_to_pay <= 0)
    return;

  // Find the person to pay.
  for (struct char_data *ch = slots->in_room->people; ch; ch = ch->next_in_room) {
    if (GET_IDNUM(ch) == GET_SLOTMACHINE_PLAYER_ID(slots)) {
      if (rolled == 0) {
        send_to_char("^RThe pistol barks, sending a sharp pain ripping through you!^n\r\nAs you stagger back, the machine closes back up and spits out your original bet.\r\n", ch);
        act("^RThe pistol barks!^n As $n staggers back, the machine closes up and spits out $s original bet.", FALSE, ch, 0, 0, TO_ROOM);
        gain_nuyen(ch, amount_to_pay, NUYEN_INCOME_GAMBLING);
        
        if (!IS_SENATOR(ch)) {
          GET_SLOTMACHINE_MONEY_EXTRACTED(slots) -= amount_to_pay;
          total_amount_removed_from_economy_by_slots -= amount_to_pay;
          add_gambling_ledger_entry(ch, amount_to_pay);
        }

        mudlog_vfprintf(ch, LOG_GRIDLOG, "%s got shot by %s! Total removed from economy is now %ld.",
                          GET_CHAR_NAME(ch),
                          GET_OBJ_NAME(slots),
                          total_amount_removed_from_economy_by_slots);

        // No dodge or soak, mostly because this is moderate damage done for the memes.
        damage(ch, ch, MODERATE, TYPE_PISTOL, TRUE);
        return;
      }

      gain_nuyen(ch, amount_to_pay, NUYEN_INCOME_GAMBLING);
      send_to_char(ch, "You receive %d nuyen (a %dx payout)\r\n", amount_to_pay, payout_multiplier);
      snprintf(buf, sizeof(buf), "$n receives %d nuyen (a %dx payout).", amount_to_pay, payout_multiplier);
      act(buf, FALSE, ch, NULL, NULL, TO_ROOM);

      // We only care about player actions in terms of economy tracking.
      if (!IS_SENATOR(ch)) {
        GET_SLOTMACHINE_MONEY_EXTRACTED(slots) -= amount_to_pay;
        total_amount_removed_from_economy_by_slots -= amount_to_pay;
        add_gambling_ledger_entry(ch, amount_to_pay);

        if (payout_multiplier > 3) {
          // Log big payouts (10x and up)
          mudlog_vfprintf(ch, LOG_GRIDLOG, "%s got %d nuyen from a %d-nuyen bet on %s (a %dx payout). Total removed from economy is now %ld.",
                          GET_CHAR_NAME(ch),
                          amount_to_pay,
                          GET_SLOTMACHINE_LAST_SPENT(slots),
                          GET_OBJ_NAME(slots),
                          payout_multiplier,
                          total_amount_removed_from_economy_by_slots);
        } else {
          // We don't want to spam the in-game logs with standard payouts from people playing all the time.
          log_vfprintf("%s got %d nuyen from a %d-nuyen bet on %s (a %dx payout). Total removed from economy is now %ld.",
                      GET_CHAR_NAME(ch),
                      amount_to_pay,
                      GET_SLOTMACHINE_LAST_SPENT(slots),
                      GET_OBJ_NAME(slots),
                      payout_multiplier,
                      total_amount_removed_from_economy_by_slots);
        }
      }
      return;
    }
  }

  if (rolled == 0) {
    send_to_room("...But with the player gone, the slot machine reluctantly closes its hatch again. Phew!\r\n", slots->in_room);
  } else {
    send_to_room("...But with the player gone, the slot machine just eats the winnings.\r\n", slots->in_room);
  }
}

#define MINIMUM_SLOT_SPEND_AMOUNT 100
#define MAXIMUM_SLOT_SPEND_AMOUNT 2500000
SPECIAL(slot_machine) {
  struct obj_data *slots = (struct obj_data *) me;
  char keyword[MAX_INPUT_LENGTH];
  int spend_amount = MINIMUM_SLOT_SPEND_AMOUNT;

  // If I'm not in a room, do nothing.
  if (!slots->in_room)
    return FALSE;

  if (!cmd) {
    // Tick down the slot machine.
    if (GET_SLOTMACHINE_PLAY_TICKS(slots) > 0) {
      GET_SLOTMACHINE_PLAY_TICKS(slots)--;

      switch (GET_SLOTMACHINE_PLAY_TICKS(slots)) {
        case 0:
          // Finished case.
          payout_slots(slots);
          break;
        case 1:
          // Slowing down.
          snprintf(buf, sizeof(buf), "The reels on %s slow, flashing tantalizing hints about the outcome...\r\n", decapitalize_a_an(GET_OBJ_NAME(slots)));
          send_to_room(buf, slots->in_room);
          break;
        default:
          // Still rolling.
          snprintf(buf, sizeof(buf), "The reels on %s blur by, too fast to read.\r\n", decapitalize_a_an(GET_OBJ_NAME(slots)));
          send_to_room(buf, slots->in_room);
          break;
      }
    }
    return FALSE;
  }

  // Astrals can't use slots.
  if (IS_ASTRAL(ch))
    return FALSE;

  if (CMD_IS("pull") || CMD_IS("use")) {
    skip_spaces(&argument);

    if (!*argument) {
      return FALSE;
    }

    char *remainder = any_one_arg(argument, keyword);

    if (get_obj_in_list_vis(ch, keyword, slots->in_room->contents) != slots) {
      return FALSE;
    }

    // We know they're talking about us now.
    if (GET_SLOTMACHINE_PLAY_TICKS(slots) > 0) {
      act("$p is already in use.", FALSE, ch, slots, 0, TO_CHAR);
      return TRUE;
    }

    if (*remainder) {
      spend_amount = atoi(remainder);
      if (spend_amount < MINIMUM_SLOT_SPEND_AMOUNT) {
        send_to_char(ch, "The minimum slot spend is %d nuyen, and '%s' doesn't qualify.\r\n", MINIMUM_SLOT_SPEND_AMOUNT, remainder);
        return TRUE;
      }
      if (spend_amount > MAXIMUM_SLOT_SPEND_AMOUNT) {
        send_to_char(ch, "The maximum slot spend is %d nuyen, and '%s' doesn't qualify.\r\n", MAXIMUM_SLOT_SPEND_AMOUNT, remainder);
        return TRUE;
      }
    }

    if (GET_NUYEN(ch) < spend_amount) {
      send_to_char(ch, "You can't afford a slot play of %d-- you only have %d nuyen on hand.\r\n", spend_amount, GET_NUYEN(ch));
      return TRUE;
    }

    lose_nuyen(ch, spend_amount, NUYEN_OUTFLOW_GAMBLING);

    snprintf(buf, sizeof(buf), "You feed %d nuyen into $p and pull the lever, sending the reels into a blur.", spend_amount);
    act(buf, FALSE, ch, slots, NULL, TO_CHAR);
    snprintf(buf, sizeof(buf), "$n feeds %d nuyen into $p and pulls the lever, sending the reels into a blur.", spend_amount);
    act(buf, FALSE, ch, slots, NULL, TO_ROOM);

    GET_SLOTMACHINE_PLAY_TICKS(slots) = 2;
    GET_SLOTMACHINE_LAST_SPENT(slots) = spend_amount;
    GET_SLOTMACHINE_PLAYER_ID(slots) = GET_IDNUM_EVEN_IF_PROJECTING(ch);

    if (!IS_SENATOR(ch)) {
      GET_SLOTMACHINE_MONEY_EXTRACTED(slots) += spend_amount;
      total_amount_removed_from_economy_by_slots += spend_amount;

      // Track it as a loss immediately, then add back the winnings when those pay out.
      add_gambling_ledger_entry(ch, -spend_amount);
    }

    return TRUE;
  }

  if (IS_SENATOR(ch) && CMD_IS("value")) {
    send_to_char(ch, "This slot machine has %s %d nuyen %s the economy. Globally, slots have %s %ld nuyen.\r\n",
                 GET_SLOTMACHINE_MONEY_EXTRACTED(slots) < 0 ? "paid (-_-)" : "removed",
                 GET_SLOTMACHINE_MONEY_EXTRACTED(slots),
                 GET_SLOTMACHINE_MONEY_EXTRACTED(slots) < 0 ? "into" : "from",
                 total_amount_removed_from_economy_by_slots < 0 ? "generated (-_-)" : "extracted",
                 total_amount_removed_from_economy_by_slots < 0 ? total_amount_removed_from_economy_by_slots * -1 : total_amount_removed_from_economy_by_slots);
    return TRUE;
  }

  return FALSE;
}
#undef MINIMUM_SLOT_SPEND_AMOUNT