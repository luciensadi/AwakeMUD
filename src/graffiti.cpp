#include <unordered_map>

#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "handler.hpp"
#include "quest.hpp"
#include "moderation.hpp"
#include "innervoice.hpp"

#define MAX_GRAFFITI_SPRAYS_PER_CHARACTER 20

extern SPECIAL(spraypaint);
extern void check_quest_destroy(struct char_data *ch, struct obj_data *obj);

std::unordered_map<idnum_t, int> global_graffiti_count = {};

ACMD(do_spray)
{
  skip_spaces(&argument);
  FAILURE_CASE(!*argument, "What do you want to spray? (Syntax: SPRAY <message>)");

  // If they trigger automod with this, bail out.
  if (check_for_banned_content(argument, ch))
    return;

  FAILURE_CASE(!ch->in_room, "You can't do that in a vehicle.");

  {
    int existing_graffiti_count = 0;
    struct obj_data *obj;
    FOR_ITEMS_AROUND_CH(ch, obj) {
      if (OBJ_IS_GRAFFITI(obj))
        existing_graffiti_count++;
    }
    FAILURE_CASE(existing_graffiti_count >= MAXIMUM_GRAFFITI_IN_ROOM,
                 "There's too much graffiti here, you can't find a spare place to paint!\r\n(OOC: You'll have to ##^WCLEANUP GRAFFITI^n before you can paint here.)");
  }

  auto g_g_c_itr = global_graffiti_count.find(GET_IDNUM_EVEN_IF_PROJECTING(ch));
  FAILURE_CASE_PRINTF(g_g_c_itr != global_graffiti_count.end() && g_g_c_itr->second > MAX_GRAFFITI_SPRAYS_PER_CHARACTER,
                      "Sorry, there's a %d-graffiti-per-character limit. Please clean up another one of your creations first.",
                      MAX_GRAFFITI_SPRAYS_PER_CHARACTER);

  for (struct obj_data *obj = ch->carrying; obj; obj = obj->next_content) {
    if (GET_OBJ_SPEC(obj) && GET_OBJ_SPEC(obj) == spraypaint) {
      int length = get_string_length_after_color_code_removal(argument, ch);

      FAILURE_CASE(length >= LINE_LENGTH, "There isn't that much paint in there.");

      // If it's too short, check to make sure there's at least one space in it.
      if (length < 10) {
        const char *ptr = argument;
        for (; *ptr; ptr++) {
          if (*ptr == ' ')
            break;
        }
        FAILURE_CASE(!*ptr, "Please write out something to spray, like 'spray A coiling dragon mural'.");
      }

      // Try to block ASCII sprays.
      {
        int alpha = 0, nonalpha = 0;
        for (const char *ptr = argument; *ptr; ptr++) {
          if (isalnum(*ptr)) {
            alpha++;
          } else if (*ptr != '^' && *ptr != '[' && *ptr != ']' && *ptr != ' ') {
            nonalpha++;
          }
        }
        FAILURE_CASE(nonalpha > 5 && (alpha / 4 < nonalpha), "ASCII art doesn't play well with screenreaders, please write things out!");
      }

      // Don't spam the same sprays.
      if (ch->desc) {
        if (!str_cmp(argument, ch->desc->last_sprayed)) {
          send_to_char("For spam reduction reasons, you can't spray the same thing twice in a row.\r\n", ch);
          return;
        } else {
          strlcpy(ch->desc->last_sprayed, argument, sizeof(ch->desc->last_sprayed));
        }
      }

      struct obj_data *paint = read_object(OBJ_DYNAMIC_GRAFFITI, VIRTUAL, OBJ_LOAD_REASON_SPECPROC);
      snprintf(buf, sizeof(buf), "a piece of graffiti that says \"%s^n\"", argument);
      paint->restring = str_dup(buf);
      snprintf(buf, sizeof(buf), "   ^n%s^n", argument);
      paint->graffiti = str_dup(buf);
      GET_GRAFFITI_SPRAYED_BY(paint) = GET_IDNUM_EVEN_IF_PROJECTING(ch);
      obj_to_room(paint, ch->in_room);

      send_to_char("You tag the area with your spray.\r\n", ch);
      mudlog_vfprintf(ch, LOG_MISCLOG, "[SPRAYLOG]: %s sprayed graffiti: %s.", GET_CHAR_NAME(ch), GET_OBJ_NAME(paint));

      // Track their graffiti total.
      if (g_g_c_itr != global_graffiti_count.end() ) {
        g_g_c_itr->second++;
      } else {
        global_graffiti_count.insert({GET_IDNUM_EVEN_IF_PROJECTING(ch), 1});
      }

      WAIT_STATE(ch, 3 RL_SEC);

      if (++GET_OBJ_TIMER(obj) >= 3) {
        send_to_char("The spray can is now empty, so you throw it away.\r\n", ch);
        extract_obj(obj);
      }
      return;
    }
  }

  send_to_char("You don't have anything to spray with.\r\n", ch);
}

ACMD(do_cleanup)
{
  skip_spaces(&argument);
  if (!*argument) {
    send_to_char("What do you want to clean up?\r\n", ch);
    return;
  }

  struct char_data *tmp_char = NULL;
  struct obj_data *target_obj = NULL;

  generic_find(argument, FIND_OBJ_ROOM, ch, &tmp_char, &target_obj);

  if (!target_obj) {
    send_to_char(ch, "You don't see any graffiti called '%s' here.\r\n", argument);
    return;
  }

  if (!OBJ_IS_GRAFFITI(target_obj)) {
    send_to_char(ch, "%s is not graffiti.\r\n", capitalize(GET_OBJ_NAME(target_obj)));
    return;
  }

  if (ch_is_blocked_by_quest_protections(ch, target_obj, TRUE, TRUE)) {
    send_to_char(ch, "%s isn't yours-- better leave it be.\r\n", capitalize(GET_OBJ_NAME(target_obj)));
    return;
  }

  // If you're not a staff member, you need an item to clean it up.
  if (!access_level(ch, LVL_BUILDER)) {
    struct obj_data *cleaner = NULL;
    for (cleaner = ch->carrying; cleaner; cleaner = cleaner->next_content) {
      if (GET_OBJ_TYPE(cleaner) == ITEM_DRINKCON && GET_DRINKCON_LIQ_TYPE(cleaner) == LIQ_CLEANER && GET_DRINKCON_AMOUNT(cleaner) > 0) {
        break;
      }
    }
    if (!cleaner) {
      send_to_char("You don't have any cleaning solution to remove the paint with.\r\n", ch);
      return;
    }

    // Decrement contents.
    if ((GET_DRINKCON_AMOUNT(cleaner) -= 2) <= 0) {
      send_to_char(ch, "You spray the last of the cleaner from %s over the graffiti.\r\n", decapitalize_a_an(GET_OBJ_NAME(cleaner)));
    }
  }

  if (GET_OBJ_QUEST_CHAR_ID(target_obj)) {
    send_to_char(ch, "You spend a few moments scrubbing away at %s. Community service, good for you!\r\n", GET_OBJ_NAME(target_obj));
    act("$n spends a few moments scrubbing away at $p.", TRUE, ch, target_obj, NULL, TO_ROOM);

    WAIT_STATE(ch, 1 RL_SEC);
  } else {
    send_to_char(ch, "You spend a few minutes scrubbing away at %s. It must have really offended you.\r\n", GET_OBJ_NAME(target_obj));
    send_to_char(ch, "(OOC: Please remember to only clean up offensive or harmful graffiti!)\r\n");
    act("$n spends a few minutes scrubbing away at $p.", TRUE, ch, target_obj, NULL, TO_ROOM);

    WAIT_STATE(ch, 6 RL_SEC);
  }

  // Log it, but only if it's player-generated content.
  if (GET_OBJ_VNUM(target_obj) == OBJ_DYNAMIC_GRAFFITI) {
    snprintf(buf, sizeof(buf), "[SPRAYLOG]: %s cleaned up graffiti: ^n%s^g.", GET_CHAR_NAME(ch), GET_OBJ_NAME(target_obj));
    mudlog(buf, ch, LOG_MISCLOG, TRUE);
  }

  if (COULD_BE_ON_QUEST(ch))
    check_quest_destroy(ch, target_obj);

  extract_obj(target_obj);
}