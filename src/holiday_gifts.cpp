/* Responsible for handing out gifts to anyone who logs in on a holiday or within the next X days. 
   
   Features:
   - Flexible structure; define your own holiday dates and present names
   - Not game-breaking: Gives little perks and toys, not something that will alter the game for you

   Needs:
   - A way to track who's gotten what gifts already
   - A way to specify what the gift list is for a given holiday, unless all are the same
*/

#include <ctime>

#include "awake.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "utils.hpp"
#include "handler.hpp"
#include "interpreter.hpp"
#include "holiday_gifts.hpp"

std::vector<class holiday_entry> holiday_entries = {
  {"Christmas '23", 12, 25, 2023,
     "A puff of peppermint-scented air distracts you, and before you know it, there's a festive present in your hands.\r\n",
     "a festive present with 'Happy Holidays!' emblazoned on the tag",
     "Done up in shiny wrapping paper and finished with a neat bow, this holiday present is sure to please. Why not ^WOPEN^n it now?",
     OBJ_VINTAGE_UGLY_CHRISTMAS_SWEATER},
  // Hopefully I'll be able to come back in 2024 and rewrite the below, but it's here just in case.
  {"Christmas '24", 12, 25, 2024,
     "A puff of peppermint-scented air distracts you, and before you know it, there's a festive present in your hands.\r\n",
     "a festive present with 'Happy Holidays!' emblazoned on the tag",
     "Done up in shiny wrapping paper and finished with a neat bow, this holiday present is sure to please. Why not ^WOPEN^n it now?",
     OBJ_CHRISTMAS_2024_GIFT}
};

void award_holiday_gifts() {
  for (auto holiday : holiday_entries) {
    // Skip inactive holidays.
    if (!holiday.is_active()) {
      continue;
    }

    // Got an active holiday.
    for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
      struct char_data *ch = d->original ? d->original : d->character;

      if (ch && holiday.is_eligible(ch)) {
        holiday.award_gift(ch);
      }
    }
  }
}

bool holiday_entry::is_active() {
  time_t current_time_epoch = time(0);

  for (int day_idx = 0; day_idx <= NUM_DAYS_AFTER_HOLIDAY_FOR_GIFTS; day_idx++) {
    struct tm *local_time = localtime(&current_time_epoch);

    int local_year = local_time->tm_year + 1900;
    int local_mon = local_time->tm_mon + 1;

    // Yes, the time bracketing will fail around New Year's if you make a holiday for that,
    // because when the year rolls over this check will fail. Add logic to bridge years
    // if/when that edge case comes up.
    if (local_year == year && local_mon == month && local_time->tm_mday == day) {
      return TRUE;
    }

    // Increment by one day. Messy, I know, but easier than adding days to the calendar
    // struct and then trying to roll over the month etc appropriately.
    current_time_epoch += 86400;
  }
  
  return FALSE;
}

bool holiday_entry::already_got_award(struct char_data *ch) {
  return player_has_db_tag(GET_IDNUM(ch), holiday_name);
}

bool holiday_entry::is_eligible(struct char_data *ch) {
  // No gifting to NPCs or linkdead folks.
  if (!ch || IS_NPC(ch) || !ch->desc)
    return FALSE;

  // Still in chargen or very new to the game? No-go.
  if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED) || GET_TKE(ch) < 10)
    return FALSE;

  // Not in playing state? Nope.
  if (STATE(ch->desc) != CON_PLAYING)
    return FALSE;

  // Idle? Try again later.
  if (ch->char_specials.timer > 15 || PRF_FLAGGED(ch, PRF_AFK))
    return FALSE;

  // They already got a present-- bail out.
  if (already_got_award(ch))
    return FALSE;

  // Good to go.
  return TRUE;
}

void holiday_entry::award_gift(struct char_data *ch) {
  // Create a gift based on the specified name and desc.
  struct obj_data *present = read_object(OBJ_HOLIDAY_GIFT, VIRTUAL);
  present->restring = str_dup(present_restring);
  present->photo = str_dup(present_photo);
  GET_HOLIDAY_GIFT_ISSUED_TO(present) = GET_IDNUM(ch);
  GET_HOLIDAY_GIFT_ISSUED_BY(present) = -1;

  // Put some goodies in it.
  GET_HOLIDAY_GIFT_SYSPOINT_VALUE(present) = 20;
  GET_HOLIDAY_GIFT_DECKBUILD_TOKENS(present) = 2;
  GET_HOLIDAY_GIFT_ONE_SHOT_HEALS(present) = 3;

  // Add a cool(?) item.
  struct obj_data *gift = read_object(gift_vnum, VIRTUAL);
  obj_to_obj(gift, present);

  // Hand off gift to char with message.
  obj_to_char(present, ch);
  send_to_char(present_message, ch);

  // Persist the fact that they got it.
  set_db_tag(GET_IDNUM(ch), holiday_name);

  // Save them.
  playerDB.SaveChar(ch);

  // Log it for posterity.
  mudlog_vfprintf(ch, LOG_SYSLOG, "Gave %s (%ld) a holiday gift (%d sysp, %d tokens, %d heals) for %s.",
                  GET_CHAR_NAME(ch),
                  GET_IDNUM(ch),
                  GET_HOLIDAY_GIFT_SYSPOINT_VALUE(present),
                  GET_HOLIDAY_GIFT_DECKBUILD_TOKENS(present),
                  GET_HOLIDAY_GIFT_ONE_SHOT_HEALS(present),
                  holiday_name);
}

SPECIAL(holiday_gift) {
  struct obj_data *obj = (struct obj_data *) me;

  // Check to make sure I'm being carried or worn by my user.
  if (!ch || (obj->carried_by != ch && obj->worn_by != ch))
    return FALSE;

  if (!CMD_IS("open"))
    return FALSE;

  strlcpy(buf, argument, sizeof(buf));
  char *no_seriously_fuck_pointers = buf;
  skip_spaces(&no_seriously_fuck_pointers);
  any_one_arg(buf, buf2);
  if (get_obj_in_list_vis(ch, buf2, ch->carrying) == obj) {
    // Make sure it's yours.
    if (GET_HOLIDAY_GIFT_ISSUED_TO(obj) != GET_IDNUM(ch)) {
      send_to_char(ch, "%s isn't yours. Only the person it was gifted to can open it.\r\n", CAP(GET_OBJ_NAME(obj)));
      char *owner_name = get_player_name(GET_HOLIDAY_GIFT_ISSUED_TO(obj));
      mudlog_vfprintf(ch, LOG_CHEATLOG, "Warning: %s is holding a holiday gift that actually belongs to %s (%ld).",
                      GET_CHAR_NAME(ch),
                      owner_name,
                      GET_HOLIDAY_GIFT_ISSUED_TO(obj));
      delete [] owner_name;
      return TRUE;
    }

    // Open the present.
    send_to_char(ch, "You happily tear %s open and find", decapitalize_a_an(GET_OBJ_NAME(obj)));

    int sent_count = 0;

    // Award syspoints.
    if (GET_HOLIDAY_GIFT_SYSPOINT_VALUE(obj) > 0) {
      send_to_char(ch, " %d syspoint%s", GET_HOLIDAY_GIFT_SYSPOINT_VALUE(obj), GET_HOLIDAY_GIFT_SYSPOINT_VALUE(obj) == 1 ? "" : "s");
      GET_SYSTEM_POINTS(ch) += GET_HOLIDAY_GIFT_SYSPOINT_VALUE(obj);
      GET_HOLIDAY_GIFT_SYSPOINT_VALUE(obj) = 0;
      sent_count++;
    }

    // Award deckbuild tokens.
    if (GET_HOLIDAY_GIFT_DECKBUILD_TOKENS(obj) > 0) {
      // Message.
      if (obj->contains) {
        send_to_char(ch, "%s%d deckbuilding token%s",
                     sent_count > 0 ? ", " : " ", 
                     GET_HOLIDAY_GIFT_DECKBUILD_TOKENS(obj),
                     GET_HOLIDAY_GIFT_DECKBUILD_TOKENS(obj) == 1 ? "" : "s");
      } else {
        send_to_char(ch, "%s%d deckbuilding token%s",
                     sent_count > 1 ? ", and " : (sent_count == 1 ? " and " : " "), 
                     GET_HOLIDAY_GIFT_DECKBUILD_TOKENS(obj),
                     GET_HOLIDAY_GIFT_DECKBUILD_TOKENS(obj) == 1 ? "" : "s");
      }

      // Hand off.
      while (GET_HOLIDAY_GIFT_DECKBUILD_TOKENS(obj)-- > 0) {
        struct obj_data *token = read_object(OBJ_STAFF_REBATE_FOR_DECKBUILDING, VIRTUAL);
        GET_DECKBUILDING_TOKEN_ISSUED_BY(obj) = -1;
        obj_to_char(token, ch);
      }
      
      sent_count++;
    }

    // Award self-heals.
    if (GET_HOLIDAY_GIFT_ONE_SHOT_HEALS(obj) > 0) {
      // Message.
      if (obj->contains) {
        send_to_char(ch, "%s%d single-use healing injector%s",
                     sent_count > 0 ? ", " : " ", 
                     GET_HOLIDAY_GIFT_ONE_SHOT_HEALS(obj),
                     GET_HOLIDAY_GIFT_ONE_SHOT_HEALS(obj) == 1 ? "" : "s");
      } else {
        send_to_char(ch, "%s%d single-use healing injector%s",
                     sent_count > 1 ? ", and " : (sent_count == 1 ? " and " : " "), 
                     GET_HOLIDAY_GIFT_ONE_SHOT_HEALS(obj),
                     GET_HOLIDAY_GIFT_ONE_SHOT_HEALS(obj) == 1 ? "" : "s");
      }

      // Hand off.
      while (GET_HOLIDAY_GIFT_ONE_SHOT_HEALS(obj)-- > 0) {
        struct obj_data *injector = read_object(OBJ_ONE_SHOT_HEALING_INJECTOR, VIRTUAL);
        GET_HEALING_INJECTOR_ISSUED_BY(injector) = -1;
        obj_to_char(injector, ch);
      }
      
      sent_count++;
    }

    // Award contained objects.
    while (obj->contains) {
      struct obj_data *temp = obj->contains;
      obj_from_obj(temp);
      obj_to_char(temp, ch);

      if (obj->contains) {
        send_to_char(ch, "%s%s", sent_count > 0 ? ", " : " ", decapitalize_a_an(GET_OBJ_NAME(temp)));
      } else {
        send_to_char(ch, "%s%s", sent_count > 1 ? ", and " : (sent_count == 1 ? " and " : " "), decapitalize_a_an(GET_OBJ_NAME(temp)));
      }

      sent_count++;
    }

    send_to_char(ch, "%s.\r\n", sent_count <= 0 ? " ...nothing :(" : "");

    // Extract the present.
    extract_obj(obj);

    return TRUE;
  }

  return FALSE;
}