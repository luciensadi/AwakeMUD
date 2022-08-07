#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "constants.hpp"
#include "handler.hpp"
#include "newdb.hpp"
#include "lexicons.hpp"
#include "newecho.hpp"
#include "db.hpp"
#include "ignore_system.hpp"

char mutable_echo_string[MAX_STRING_LENGTH];
char tag_check_string[MAX_STRING_LENGTH];
char storage_string[MAX_STRING_LENGTH];

// #define NEW_EMOTE_DEBUG(ch, ...) send_to_char((ch), ##__VA_ARGS__)
#define NEW_EMOTE_DEBUG(...)

// #define NEW_EMOTE_DEBUG_SPEECH(ch, ...) send_to_char((ch), ##__VA_ARGS__)
#define NEW_EMOTE_DEBUG_SPEECH(...)

// #define SPEECH_COLOR_CODE_DEBUG(ch, ...) send_to_char((ch), ##__VA_ARGS__)
#define SPEECH_COLOR_CODE_DEBUG(...)

// #define PSEUDOLANGUAGE_REPLACEMENT_DEBUG(ch, ...) send_to_char((ch), ##__VA_ARGS__)
#define PSEUDOLANGUAGE_REPLACEMENT_DEBUG(ch, ...)

const char *allowed_abbreviations[] = {
  "Mr", "Ms", "Mrs", "Mz"
  , "\n"
};

ACMD(do_highlight) {
  if (!argument) {
    send_to_char("Syntax: highlight <a color code>. Example: highlight ^^Y\r\n", ch);
    return;
  }

  skip_spaces(&argument);

  if (!*argument) {
    if (GET_CHAR_COLOR_HIGHLIGHT(ch) && *GET_CHAR_COLOR_HIGHLIGHT(ch))
      send_to_char(ch, "Your current highlight code is '%s' (%s*^n).\r\n", double_up_color_codes(GET_CHAR_COLOR_HIGHLIGHT(ch)), GET_CHAR_COLOR_HIGHLIGHT(ch));

    else
      send_to_char("You haven't set a highlight yet. You can do so with ^WHIGHLIGHT <color code>^n, for example ^WHIGHLIGHT ^^Y^n.\r\n", ch);

    return;
  }

  if (*argument != '^') {
    send_to_char("You need to specify a color code. Example: 'highlight ^^r'. Use '^^n' for neutral / no highlight.\r\n", ch);
    return;
  }

  if (isalpha(*(argument + 1))) {
    // Single-character color code: Must be exactly two characters.
    if (strlen(argument) != 2) {
      send_to_char("You need to specify a color code. Example: 'highlight ^^r'. Use '^^n' for neutral / no highlight.\r\n", ch);
      return;
    }

    // No dark black.
    if (*(argument + 1) == 'l') {
      send_to_char("Sorry, you can't use ^^l as a highlight. It's too dark for many people to read easily.\r\n", ch);
      return;
    }
  }

  else if (*(argument + 1) == '[') {
    // XTERM256 color code.
    if (strlen(argument) != 7) {
      send_to_char("You need to specify a color code. Example: 'highlight ^^[F123]'. Use '^^n' for neutral / no highlight.\r\n", ch);
      return;
    }
  }

  DELETE_ARRAY_IF_EXTANT(SETTABLE_CHAR_COLOR_HIGHLIGHT(ch));
  SETTABLE_CHAR_COLOR_HIGHLIGHT(ch) = str_dup(argument);
  send_to_char(ch, "OK, your highlight is now '%s' (%s*^n).\r\n", double_up_color_codes(GET_CHAR_COLOR_HIGHLIGHT(ch)), GET_CHAR_COLOR_HIGHLIGHT(ch));

  playerDB.SaveChar(ch);
}

/* Technically, language is supposed to restrict the types of things you can understand.
   Since linguistic parsing at that level is beyond what I'm willing to write in C (and
   also technically beyond most systems today), we're going with a rudimentary metric
   based on word length. Hey, it's quick. -- LS */

int max_allowable_word_length_at_language_level(int level) {

  switch (level) {
    case 0: return 0;
    case 1: return 4;
    case 2: return 5;
    case 3: return 6;
    case 4: return 7;
    case 5: return 9;
    case 6: return 11;
    case 7: return 13;
    case 8: return 15;
    case 9: return 17;
    case 10: return 19;
    case 11: return 25;
    default: return 500;
  }
}

const char *generate_display_string_for_character(struct char_data *actor, struct char_data *viewer, struct char_data *target_ch, bool terminate_with_actors_color_code) {
  static char result_string[MAX_STRING_LENGTH];
  struct remem *mem_record;
  const char *terminal_code = "^n";
  bool should_highlight = !PRF_FLAGGED(viewer, PRF_NOHIGHLIGHT) && !PRF_FLAGGED(viewer, PRF_NOCOLOR);
  const char *viewer_highlight = should_highlight ? GET_CHAR_COLOR_HIGHLIGHT(viewer) : "";

  struct veh_data *viewer_veh = NULL, *target_veh = NULL;

  if (PLR_FLAGGED(viewer, PLR_REMOTE) || AFF_FLAGGED(viewer, AFF_PILOT) || AFF_FLAGGED(viewer, AFF_RIG)) {
    RIG_VEH(viewer, viewer_veh);
  }

  // TODO: This logic isn't quite right-- what if I am in the room with a meat body that is rigging, and I target the meat body?
  if (PLR_FLAGGED(target_ch, PLR_REMOTE) || AFF_FLAGGED(target_ch, AFF_PILOT) || AFF_FLAGGED(target_ch, AFF_RIG)) {
    RIG_VEH(target_ch, target_veh);
  }

  if (terminate_with_actors_color_code && should_highlight) {
    SPEECH_COLOR_CODE_DEBUG(actor, "Terminating with your color code.\r\n");
    terminal_code = GET_CHAR_COLOR_HIGHLIGHT(actor);

    // Make sure there's differentiation between highlight colors.
    if (target_ch == viewer && !strcmp(viewer_highlight, terminal_code)) {
      if (!strcmp((viewer_highlight = string_to_uppercase(viewer_highlight)), terminal_code))
        viewer_highlight = string_to_lowercase(viewer_highlight);
      SPEECH_COLOR_CODE_DEBUG(actor, "Viewer code changed from %s*^n to %s*^n for %s.\r\n",
                                     GET_CHAR_COLOR_HIGHLIGHT(viewer),
                                     viewer_highlight,
                                     GET_CHAR_NAME(viewer));
    }
  }

  // If the target is the viewer, we don't process their name into a desc, but we do highlight.
  if (target_ch == viewer) {
    // Insert the color code sequence for the viewing character's speech color.
    if (viewer_veh) {
      snprintf(result_string, sizeof(result_string), "%syour vehicle%s",
               viewer_highlight,
               terminal_code);
    } else if (IS_ASTRAL(viewer)) {
      snprintf(result_string, sizeof(result_string), "%s%s's reflection%s",
               viewer_highlight,
               GET_CHAR_NAME(viewer),
               terminal_code);
    } else if (PLR_FLAGGED(viewer, PLR_MATRIX)) {
      snprintf(result_string, sizeof(result_string), "%s%s's persona%s",
               viewer_highlight,
               GET_CHAR_NAME(viewer),
               terminal_code);
    } else if (viewer->desc && viewer->desc->original) {
      snprintf(result_string, sizeof(result_string), "%s%s (you)%s",
               viewer_highlight,
               decapitalize_a_an(GET_NAME(viewer)),
               terminal_code);
    } else {
      snprintf(result_string, sizeof(result_string), "%s%s%s",
               viewer_highlight,
               GET_CHAR_NAME(viewer),
               terminal_code);
    }
  }

  // If the target is not the viewer, we process it as normal.
  else {
    const char *display_string;

    if (target_veh && viewer->in_veh != target_veh) {
      display_string = GET_VEH_NAME(target_veh);
    } else {
      // Switch between display strings based on if the viewer can see and knows the target character.

      // If the viewer is ignoring the target, we make it clear that an ignored person is being addressed.
      if (IS_IGNORING(viewer, is_blocking_ic_interaction_from, target_ch))
        display_string = "(ignored)";
      else if (!CAN_SEE(viewer, target_ch))
        display_string = "someone";
      else if (!IS_NPC(viewer) && (mem_record = safe_found_mem(viewer, target_ch)))
        display_string = CAP(mem_record->mem);
      else
        display_string = decapitalize_a_an(GET_NAME(target_ch));
    }

    // Insert the display string into the mutable string in place of the tag. No terminal_code here-- this always happens outside of speech.
    snprintf(result_string, sizeof(result_string), "%s^n", display_string);
  }

  return result_string;
}

// Finds the first occurrence of standalone word 'str' in string.
const char *strstr_isolated(const char *string, const char *search_string) {
  const char *ptr = strstr(string, search_string);
  int search_len = strlen(search_string);

  while (ptr && isalpha(*(ptr + search_len)))
    ptr = strstr(ptr + 1, search_string);

  return ptr;
}

// Finds the first occurrence of standalone word 'str' in string (ignores case)
const char *str_str_isolated(const char *string, const char *search_string) {
  const char *ptr = str_str(string, search_string);
  int search_len = strlen(search_string);

  while (ptr && isalpha(*(ptr + search_len)))
    ptr = str_str(ptr + 1, search_string);

  return ptr;
}

char newecho_debug_buf[MAX_STRING_LENGTH];
void send_echo_to_char(struct char_data *actor, struct char_data *viewer, const char *echo_string, bool require_char_name) {
  int tag_index, i;
  char scratch_space[500];
  struct char_data *target_ch = NULL;
  struct remem *mem_record = NULL;

  // Sanity check.
  if (!actor || !viewer) {
    mudlog("SYSERR: Received null actor or viewer to send_emote_to_char!", actor, LOG_SYSLOG, TRUE);
    return;
  }

  // Don't bother processing emotes for those who can't appreciate them.
  if (!viewer->desc)
    return;

  struct veh_data *veh = NULL, *in_veh = actor->in_veh, *unpiloted_vehicle = NULL;
  struct room_data *in_room = actor->in_room;

  NEW_EMOTE_DEBUG(actor, "\r\n\r\n^gBeginning evaluation for %s.^n\r\n", GET_CHAR_NAME(viewer));

  if (PLR_FLAGGED(actor, PLR_REMOTE) || AFF_FLAGGED(actor, AFF_PILOT) || AFF_FLAGGED(actor, AFF_RIG)) {
    RIG_VEH(actor, veh);

    if (veh) {
      in_room = veh->in_room;
      in_veh = veh->in_veh;
      NEW_EMOTE_DEBUG(actor, "^yRIG_VEH found vehicle %s (room: %s, veh: %s).^n", GET_VEH_NAME(veh), in_room ? "Y":"N", in_veh ? "Y":"N");
    }
  }

  bool should_highlight = !PRF_FLAGGED(viewer, PRF_NOHIGHLIGHT) && !PRF_FLAGGED(viewer, PRF_NOCOLOR);

  // Scan the string for the actor's name. This is an easy check. In the process, convert echo_string into something mutable, and set i to skip over this new text.
  bool must_prepend_name = TRUE;
  if (require_char_name) {
    // We're finding all characters up to the first quote mark, and checking those for the name.
    const char *quote_ptr = strchr(echo_string, '"');
    const char *start_of_block = echo_string;

    while (quote_ptr) {
      memset(storage_string, 0, sizeof(storage_string));
      strncpy(storage_string, start_of_block, quote_ptr - start_of_block);
      // send_to_char(actor, "Storage string: '%s' (quote_ptr = %c, start_of_block = %c)\r\n", storage_string, *quote_ptr, *start_of_block);

      // Check the string for '@self'.
      if (!(must_prepend_name = !str_str_isolated(storage_string, "@self"))) {
        // send_to_char("Found @self outside of quotes.\r\n", actor);
        break;
      }

      // Check the string for the capitalized character's name.
      if (!(must_prepend_name = !strstr_isolated(storage_string, capitalize(GET_CHAR_NAME(actor))))) {
        // send_to_char("Found name outside of quotes.\r\n", actor);
        break;
      }

      // Since we're before a quote (outside of dialogue), we want to advance to the next quote.
      if (!(quote_ptr = strchr(quote_ptr + 1, '"'))) {
        // send_to_char("No more quotes.\r\n", actor);
        break;
      }

      // If the next character is a null character, we're at the end of the emote.
      if (!(start_of_block = quote_ptr + 1)) {
        // send_to_char("Null character after quote.\r\n", actor);
        break;
      }

      // Finally, advance to the quote at the beginning of the next dialogue, or re-evaluate if there is no more dialogue.
      if (!(quote_ptr = strchr(start_of_block, '"'))) {
        // send_to_char("Breaking out of while loop, no more quotes.\r\n", actor);
        break;
      }
    }

    if (start_of_block && *start_of_block) {
      // send_to_char(actor, "Last attempt. Evaluating '%s'.\r\n", start_of_block);
      if (!(must_prepend_name = !(str_str_isolated(start_of_block, "@self") || str_str_isolated(start_of_block, "@me") || str_str_isolated(start_of_block, "@myself")))) {
        // send_to_char("Found @self in final analysis.\r\n", actor);
      }

      // Check the string for the capitalized character's name.
      else if (!(must_prepend_name = !strstr_isolated(start_of_block, capitalize(GET_CHAR_NAME(actor))))) {
        // send_to_char("Found name in final analysis.\r\n", actor);
      }
    }

  } else {
    must_prepend_name = FALSE;
  }

  // If we didn't find it, we have to put their name at the beginning.
  if (must_prepend_name) {
    if (echo_string[0] == '\'' && echo_string[1] == 's') {
      snprintf(mutable_echo_string, sizeof(mutable_echo_string), "@self%s", echo_string);
    } else {
      snprintf(mutable_echo_string, sizeof(mutable_echo_string), "@self %s", echo_string);
    }
  } else {
    strlcpy(mutable_echo_string, echo_string, sizeof(mutable_echo_string));
  }

  const char *name_ptr;
  if (require_char_name
      && ((name_ptr = strstr(echo_string, GET_CHAR_NAME(actor))) == NULL || !isalpha(*(name_ptr + strlen(GET_CHAR_NAME(actor)))))
      && str_str(echo_string, "@self") == NULL) {
    // TODO: What's the point of this ifblock?
  }

  NEW_EMOTE_DEBUG(actor, "\r\nAfter first pass, mutable_echo_string is '%s'. Evaluating...\r\n", mutable_echo_string);

  // Next, check capitalized words in it for highlighting purposes, unless they've disabled highlighting or color.
  bool quote_mode = FALSE;
  bool start_of_new_sentence = TRUE;
  for (i = 0; i < (int) strlen(mutable_echo_string); i++) {
    NEW_EMOTE_DEBUG(actor, "^y%c%s^n", mutable_echo_string[i], mutable_echo_string[i] == '^' ? "^" : "");

    // Quote mark? Enable quote mode, which stops expansion of names and just does highlighting.
    if (mutable_echo_string[i] == '"') {
      quote_mode = !quote_mode;
      NEW_EMOTE_DEBUG(actor, "\r\nQuote mode is now %s.\r\n", quote_mode ? "on" : "off");
      continue;
    }

    if (mutable_echo_string[i] == '.' || mutable_echo_string[i] == '!' || mutable_echo_string[i] == '?') {
      start_of_new_sentence = TRUE;
    }

    bool at_mode = FALSE;
    // Identify anything starting with a capital letter or @-symbol.
    if (isupper(mutable_echo_string[i]) || ((at_mode = (mutable_echo_string[i] == '@')) && isalpha(mutable_echo_string[++i]))) {
      // Read it into a new string until we hit the end of the tag (no more alphabetical letters)
      for (tag_index = i; isalpha(mutable_echo_string[tag_index]); tag_index++)
        tag_check_string[tag_index - i] = mutable_echo_string[tag_index];

      // Null-terminate the tag check string.
      tag_check_string[tag_index - i] = '\0';

      // Short names don't auto-match unless they're complete matches or explicit tags.
      bool require_exact_match = !at_mode && (quote_mode || (start_of_new_sentence || strlen(tag_check_string) < 5));

      if (require_exact_match) {
        NEW_EMOTE_DEBUG(actor, "\r\nUsing exact mode for this evaluation due to non-at and low string length %d.\r\n", strlen(tag_check_string));
      }

      target_ch = NULL;
      unpiloted_vehicle = NULL;

      // Short-circuit check: @self.
      NEW_EMOTE_DEBUG(actor, "Evaluating tag check string '^c%s^n' (at_mode=%s).\r\n", tag_check_string, at_mode ? "Y":"N");
      bool self_mode = at_mode && (!str_cmp("self", tag_check_string) || !str_cmp("me", tag_check_string) || !str_cmp("myself", tag_check_string));
      if (self_mode) {
        target_ch = actor;
      } else {
        // Compare it to bystanders' memorized names. Try for exact matches first.
        if (!target_ch) {
          for (target_ch = in_room ? in_room->people : in_veh->people;
               target_ch;
               target_ch = in_room ? target_ch->next_in_room : target_ch->next_in_veh)
          {
            // No such thing as remembering an NPC, and you can't target someone you can't see.
            if (IS_NPC(target_ch) || !CAN_SEE(actor, target_ch))
              continue;

            // Check for exact match with memory.
            if (!IS_NPC(actor) && (mem_record = safe_found_mem(actor, target_ch)) && !str_cmp(mem_record->mem, tag_check_string))
              break;
          }
          if (target_ch) {
            NEW_EMOTE_DEBUG(actor, "\r\nWith target string '%s', found %s by memory (exact).\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
          }
        }

        // Compare it to bystanders' memorized names. Now that exact matches mode is out of the way, go for imprecise.
        if (!target_ch && !require_exact_match) {
          for (target_ch = in_room ? in_room->people : in_veh->people;
               target_ch;
               target_ch = in_room ? target_ch->next_in_room : target_ch->next_in_veh)
          {
            // No such thing as remembering an NPC, and you can't target someone you can't see.
            if (IS_NPC(target_ch) || !CAN_SEE(actor, target_ch))
              continue;

            // Check for imprecise match with memory.
            if (!IS_NPC(actor) && (mem_record = safe_found_mem(actor, target_ch)) && !strn_cmp(mem_record->mem, tag_check_string, strlen(tag_check_string)))
              break;
          }
          if (target_ch) {
            NEW_EMOTE_DEBUG(actor, "\r\nWith target string '%s', found %s by memory (approximate).\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
          }
        }

        // Didn't find anyone by that memorized name? Check PC names, trying for exact match first.
        if (!target_ch) {
          for (target_ch = in_room ? in_room->people : in_veh->people;
               target_ch;
               target_ch = in_room ? target_ch->next_in_room : target_ch->next_in_veh)
          {
            // NPCs don't have player data to check, and you can't target someone you can't see.
            if (IS_NPC(target_ch) || !CAN_SEE(actor, target_ch))
              continue;

            if (!str_cmp(target_ch->player.char_name, tag_check_string))
              break;
          }
          if (target_ch) {
            NEW_EMOTE_DEBUG(actor, "\r\nWith target string '%s', found %s by name (exact).\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
          }
        }

        if (!target_ch && !require_exact_match) {
          for (target_ch = in_room ? in_room->people : in_veh->people;
               target_ch;
               target_ch = in_room ? target_ch->next_in_room : target_ch->next_in_veh)
          {
            // NPCs don't have player data to check, and you can't target someone you can't see.
            if (IS_NPC(target_ch) || !CAN_SEE(actor, target_ch))
              continue;

            if (!strn_cmp(target_ch->player.char_name, tag_check_string, strlen(tag_check_string)))
              break;
          }
          if (target_ch) {
            NEW_EMOTE_DEBUG(actor, "\r\nWith target string '%s', found %s by name (approximate).\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
          }
        }

        // Didn't find anyone by their PC name? Check keywords (only if not in exact-match mode).
        if (!target_ch && !require_exact_match) {
          for (target_ch = in_room ? in_room->people : in_veh->people;
               target_ch;
               target_ch = in_room ? target_ch->next_in_room : target_ch->next_in_veh)
          {
            // Can't target someone you can't see.
            if (!CAN_SEE(actor, target_ch))
              continue;

            if (str_str(GET_KEYWORDS(target_ch), tag_check_string)) {
              // Found someone, stop looking.
              break;
            }
          }
          if (target_ch) {
            NEW_EMOTE_DEBUG(actor, "\r\nWith target string '%s', found %s by alias.\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
          }
        }

        // Didn't find anyone by their keywords? Check in short description (only if not in exact-match mode).
        if (!target_ch && !require_exact_match) {
          for (target_ch = in_room ? in_room->people : in_veh->people;
               target_ch;
               target_ch = in_room ? target_ch->next_in_room : target_ch->next_in_veh)
          {
            // Can't target someone you can't see.
            if (!CAN_SEE(actor, target_ch))
              continue;

            if (str_str(GET_NAME(target_ch), tag_check_string)) {
              // Found someone, stop looking.
              break;
            }
          }
          if (target_ch) {
            NEW_EMOTE_DEBUG(actor, "\r\nWith target string '%s', found %s by alias.\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
          }
        }

        // Still nobody? Alright, check vehicles for keywords.
        if (!target_ch && !require_exact_match) {
          for (struct veh_data *tveh = in_room ? in_room->vehicles : in_veh->carriedvehs;
               tveh;
               tveh = tveh->next_veh)
          {
            if (is_abbrev(GET_VEH_NAME(tveh), tag_check_string)) {
              // Found a valid vehicle, stop looking.
              target_ch = get_driver(tveh);

              if (target_ch) {
                // TODO: This means you can only target a vehicle that is actively controlled by someone. Empty and non-controlled vehicles are skipped.
                NEW_EMOTE_DEBUG(actor, "\r\nWith target string '%s', found %s by vehicle name.\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
                break;
              }
            }
          }

          // Didn't find a piloted vehicle by name? Look by room desc.
          if (!target_ch) {
            for (struct veh_data *tveh = in_room ? in_room->vehicles : in_veh->carriedvehs;
                 tveh;
                 tveh = tveh->next_veh)
            {
              if (is_abbrev(GET_VEH_ROOM_DESC(tveh), tag_check_string)) {
                // Found a valid vehicle, stop looking.
                target_ch = get_driver(tveh);

                if (target_ch) {
                  // TODO: This means you can only target a vehicle that is actively controlled by someone. Empty and non-controlled vehicles are skipped.
                  NEW_EMOTE_DEBUG(actor, "\r\nWith target string '%s', found %s by vehicle room desc.\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
                  break;
                }
              }
            }
          }

          // Didn't find a piloted vehicle by room desc? Look for unpiloted by name.
          if (!target_ch) {
            for (struct veh_data *tveh = in_room ? in_room->vehicles : in_veh->carriedvehs;
                 tveh;
                 tveh = tveh->next_veh)
            {
              if (str_str(GET_VEH_NAME(tveh), tag_check_string)) {
                unpiloted_vehicle = tveh;
                NEW_EMOTE_DEBUG(actor, "\r\nWith target string '%s', found unpiloted %s by vehicle name.\r\n", tag_check_string, GET_VEH_NAME(tveh));
                break;
              }
            }
          }

          // Didn't find unpiloted by name? Unpiloted by room desc.
          if (!unpiloted_vehicle) {
            for (struct veh_data *tveh = in_room ? in_room->vehicles : in_veh->carriedvehs;
                 tveh;
                 tveh = tveh->next_veh)
            {
              if (str_str(GET_VEH_ROOM_DESC(tveh), tag_check_string)) {
                unpiloted_vehicle = tveh;
                NEW_EMOTE_DEBUG(actor, "\r\nWith target string '%s', found unpiloted %s by vehicle room desc.\r\n", tag_check_string, GET_VEH_NAME(tveh));
                break;
              }
            }
          }
        }
      }

      // Found someone.
      if (target_ch || unpiloted_vehicle) {
        const char *display_string;

        // Copy out the rest of the string-- we're inserting something here.
        strncpy(storage_string, mutable_echo_string + tag_index, sizeof(storage_string) - 1);

        if (target_ch) {
          // If the viewer is the actor, and it's not @self, just stop-- we don't self-highlight.
          if (viewer == target_ch && viewer == actor && !self_mode)
            continue;

          // In quote mode, we only do viewer highlights. @-targets are considered invalid in quotes.
          if (quote_mode && target_ch != viewer)
            continue;

          // Fetch the representation of this character. Edge case: actor is viewer and @self: just give your name.
          if (self_mode && viewer == target_ch && viewer == actor) {
            NEW_EMOTE_DEBUG(actor, "^mActor == viewer == target, using straight strings.^n\r\n");
            if (veh && viewer->in_veh != veh) {
              display_string = GET_VEH_NAME(veh);
            } else {
              display_string = GET_CHAR_NAME(actor);
            }
          } else {
            if (quote_mode) {
              snprintf(scratch_space, sizeof(scratch_space), "%s%s%s", GET_CHAR_COLOR_HIGHLIGHT(viewer), tag_check_string, GET_CHAR_COLOR_HIGHLIGHT(actor));
              display_string = scratch_space;
              NEW_EMOTE_DEBUG(actor, "^mQuote mode: Using tag_check_string directly for %s resulted in '%s'.^n\r\n", GET_CHAR_NAME(actor), display_string);
            } else {
              display_string = generate_display_string_for_character(actor, viewer, target_ch, FALSE);
              NEW_EMOTE_DEBUG(actor, "^mComposing display string for %s resulted in '%s'.^n\r\n", GET_CHAR_NAME(actor), display_string);
            }
          }
        }

        else if (unpiloted_vehicle) {
          display_string = GET_VEH_NAME(unpiloted_vehicle);
        }

        else {
          mudlog("SYSERR: Entered the found-someone block of newecho without either target_ch or unpiloted_vehicle!", actor, LOG_SYSLOG, TRUE);
          continue;
        }

        // Put it into the mutable echo string, replacing the tag, and then append the rest of the storage string.
        snprintf(mutable_echo_string + i - (at_mode ? 1 : 0), sizeof(mutable_echo_string), "%s%s", display_string, storage_string);

        // Since we added X known-good characters starting at i, increase i by X-- but decrement by 1 since we'll be incrementing on loop.
        i += strlen(display_string) - 1;
        NEW_EMOTE_DEBUG(actor, "'%s' resolved to %s %s.\r\n", tag_check_string, target_ch ? "character" : "vehicle", display_string);
      }
      else {
        NEW_EMOTE_DEBUG(actor, "\r\nTag string '%s' found nobody.\r\n", tag_check_string);
        i += strlen(tag_check_string) - 1;
      }

      // We're ready to move on. i has been incremented, and all necessary writing has been done.
      start_of_new_sentence = FALSE;
    }

    else if (isalpha(mutable_echo_string[i]))
      start_of_new_sentence = FALSE;
  }

  NEW_EMOTE_DEBUG(actor, "\r\nTagging pass of emote done. Now doing speech pass. Combine these later...");

  // Next pass: Convert speech.
  int language_in_use = -1;
  for (i = 0; i < (int) strlen(mutable_echo_string); i++) {
    NEW_EMOTE_DEBUG_SPEECH(actor, "^y%c%s^n", mutable_echo_string[i], mutable_echo_string[i] == '^' ? "^" : "");
    // Skip everything that's not speech.
    if (mutable_echo_string[i] != '"')
      continue;

    // It's a quote mark. Set up our vars and increment i by one so it's pointing at the start of the speech.
    language_in_use = GET_VIABLE_LANGUAGE(actor);
    i++;

    // We only accept parenthetical language as the very first thing in the sentence.
    if (mutable_echo_string[i] == '(') {
      // i currently points to a parens, so we increment it by one. Remember this for when we chop the parens out later.
      i++;

      char language_string[100];
      int language_idx;

      // Copy the language into the language_string buffer, stopping at the next parenthetical.
      for (language_idx = i;
           language_idx - i < (int) sizeof(language_string) - 1 && mutable_echo_string[language_idx] != ')';
           language_idx++)
        language_string[language_idx - i] = mutable_echo_string[language_idx];

      // Null-terminate language buffer.
      language_string[language_idx - i] = '\0';

      // We expect that the M_E_S pointer is at the closing parens. If this is not true, we ran out of buf
      // space-- that means this is not a language! Bail out.
      if (mutable_echo_string[language_idx] != ')') {
        i = language_idx;
        continue;
      }

      // Identify the language in use.
      int skill_num;
      for (skill_num = SKILL_ENGLISH; skill_num < MAX_SKILLS; skill_num++) {
        if (!SKILL_IS_LANGUAGE(skill_num))
          continue;

        if (!str_cmp(skills[skill_num].name, language_string))
          break;
      }

      // Language identified.
      if (SKILL_IS_LANGUAGE(skill_num))
        language_in_use = skill_num;
      else
        language_in_use = GET_VIABLE_LANGUAGE(actor);

      NEW_EMOTE_DEBUG_SPEECH(actor, "\r\nLanguage in use for $n is now %s (target: '%s').\r\n", skills[language_in_use].name, language_string);

      // Snip the language block from the mutable string (we wrote the proper one to storage already). Handles a single space after the closing parens.
      if (language_idx + 1 < (int) strlen(mutable_echo_string) && isspace(mutable_echo_string[language_idx + 1]))
        strlcpy(storage_string, mutable_echo_string + language_idx + 2, sizeof(storage_string));
      else
        strlcpy(storage_string, mutable_echo_string + language_idx + 1, sizeof(storage_string));

      // Decrement i here to make it point at the area where the parens is. Remember that i points to the original string, not our storage string.
      i--;

      // Stitch it all together.
      strlcpy(mutable_echo_string + i, storage_string, sizeof(mutable_echo_string) - i);

      // i now points to the character at the start of the speech.
    }

    NEW_EMOTE_DEBUG_SPEECH(actor, "\r\nAfter language extraction (now %s), mutable string is: '%s'\r\n", skills[language_in_use].name, mutable_echo_string);

    // We're now inside a quote block, with the language for the quote identified. 'i' is the start of the speech.
    char speech_buf[MAX_STRING_LENGTH];

    // First, add the language tag string. This is for visual representation only and is used to clarify which language is in use.
    if (language_in_use != SKILL_ENGLISH && (IS_NPC(viewer) || GET_SKILL(viewer, language_in_use) > 0)) {
      snprintf(speech_buf, sizeof(speech_buf), "(%s) ", capitalize(skills[language_in_use].name));
    } else {
      strlcpy(speech_buf, "", sizeof(speech_buf));
    }
    int speech_buf_len_after_language_tag = strlen(speech_buf);
    // send_to_char(actor, "^y(%s): current buf: %s; tag length is %d\r\n", GET_CHAR_NAME(viewer), speech_buf, speech_buf_len_after_language_tag);

    // Copy the quote into the speech buf.
    int speech_idx;
    bool is_first_letter_in_speech = TRUE;
    for (speech_idx = i; speech_idx < (int) strlen(mutable_echo_string) && mutable_echo_string[speech_idx] != '"'; speech_idx++) {
      if (is_first_letter_in_speech) {
        speech_buf[speech_idx - i + speech_buf_len_after_language_tag] = toupper(mutable_echo_string[speech_idx]);
        is_first_letter_in_speech = FALSE;
      } else {
        speech_buf[speech_idx - i + speech_buf_len_after_language_tag] = mutable_echo_string[speech_idx];
      }
    }

    speech_buf[speech_idx - i + speech_buf_len_after_language_tag] = '\0';

    // Write the rest of the emote into the storage string.
    strlcpy(storage_string, mutable_echo_string + speech_idx, sizeof(storage_string));

    NEW_EMOTE_DEBUG_SPEECH(actor, "\r\nSpeech block: '%s'\r\n", speech_buf);

    // If the speech ended with punct, we use that, otherwise we use a period.
    const char *quote_termination;
    if (ispunct(mutable_echo_string[speech_idx - 1])) {
      quote_termination = "^n";
    } else {
      quote_termination = ".^n";
    }

    // If the listener can't understand you, mangle it.
    const char *replacement = replace_too_long_words(viewer, actor, speech_buf, language_in_use, GET_CHAR_COLOR_HIGHLIGHT(actor));
    if (!strcmp(replacement, speech_buf)) {
      replacement = NULL;
    } else if (!ispunct(get_final_character_from_string(replacement))){
      quote_termination = ".^n";
    }

    NEW_EMOTE_DEBUG_SPEECH(actor, "\r\nProposed replacement: '%s'\r\n", replacement ? double_up_color_codes(replacement) : "n/a");

    // Known bug: Capitalization does not work after a language word is retained.
    if (replacement) {
      // Insert the replacement.
      snprintf(mutable_echo_string + i, sizeof(mutable_echo_string) - i, "%s%s%s%s",
               should_highlight ? GET_CHAR_COLOR_HIGHLIGHT(actor) : "",
               capitalize(replacement),
               quote_termination,
               storage_string);

      i += strlen(replacement) + strlen(GET_CHAR_COLOR_HIGHLIGHT(actor)) + strlen(quote_termination);
    } else {
      // Just highlight the speech.
      snprintf(mutable_echo_string + i, sizeof(mutable_echo_string) - i, "%s%s%s%s",
               should_highlight ? GET_CHAR_COLOR_HIGHLIGHT(actor) : "",
               capitalize(speech_buf),
               quote_termination,
               storage_string);

      i = speech_idx + speech_buf_len_after_language_tag + strlen(should_highlight ? GET_CHAR_COLOR_HIGHLIGHT(actor) : "") + strlen(quote_termination);
    }

    i++;
  }

  NEW_EMOTE_DEBUG_SPEECH(actor, "Finished evaluation of emote projection for %s.\r\n\r\n", GET_CHAR_NAME(viewer));

  // Add a newline.
  strlcat(mutable_echo_string, "\r\n", sizeof(mutable_echo_string));

  // Finally(!), send it to the viewer.
  send_to_char(viewer, capitalize(mutable_echo_string));

  // And put it in their emote history.
  store_message_to_history(viewer->desc, COMM_CHANNEL_EMOTES, mutable_echo_string);
}

ACMD(do_new_echo) {
  char storage_buf[MAX_INPUT_LENGTH * 2 + 1];
  struct veh_data *veh = NULL;

  struct room_data *in_room = ch->in_room;
  struct veh_data *in_veh = ch->in_veh;

  // Reject vemoters without a vehicle.
  if (subcmd == SCMD_VEMOTE) {
    RIG_VEH(ch, veh);

    FAILURE_CASE(!veh || !(PLR_FLAGGED(ch, PLR_REMOTE) || AFF_FLAGGED(ch, AFF_PILOT) || AFF_FLAGGED(ch, AFF_RIG)), "You can only use the vemote command while controlling a vehicle.\r\n");

    in_room = veh->in_room;
    in_veh = veh->in_veh;
  }

  // Reject hitchers. They have no body and would break things.
  FAILURE_CASE(PLR_FLAGGED(ch, PLR_MATRIX) && !ch->persona, "You can't do that while hitching.\r\n");

  skip_spaces(&argument);
  delete_doubledollar(argument);

  // Require an argument.
  FAILURE_CASE(!*argument, "Yes... but what?\r\n");

  // Can't speak? No emote speech for you. NOTE: Wrapping your speech in single quotes to avoid this is a punishable exploit.
  FAILURE_CASE(strchr(argument, '"') != NULL && !char_can_make_noise(ch), "You can't seem to make any noise.\r\n");

  // Double up percentages. This is necessary to prevent unexpected insertion of data during format runs.
  char *pct_reader = argument;
  char *pct_writer = storage_buf;
  while (*pct_reader && (pct_writer - storage_buf) < (int) sizeof(storage_buf)) {
    if (*pct_reader == '%')
      *(pct_writer++) = '%';
    *(pct_writer++) = *(pct_reader++);
  }

  // Reject lines that would overflow the buffer.
  FAILURE_CASE((pct_writer - storage_buf) >= (int) sizeof(storage_buf), "Sorry, your emote was too long. Please shorten it.\r\n");

  // Null-terminate the buffer.
  *pct_writer = '\0';

  // Scan for mismatching quotes or mismatching parens. Reject if found.
  int quotes = 0, parens = 0;
  for (int i = 0; i < (int) strlen(storage_buf); i++) {
    if (storage_buf[i] == '"')
      quotes++;
    else if (storage_buf[i] == '(')
      parens++;
    else if (storage_buf[i] == ')')
      parens--;
  }

  // Require that we have an even number of quotes.
  FAILURE_CASE(quotes % 2 != 0, "There's an error in your emote: You need to close a quote.\r\n");

  // Require that we have an equal number of open and close parens.
  if (parens != 0) {
    send_to_char(ch, "There's an error in your emote: %s.\r\n", parens < 0 ? "You have too many close-parens." : "You need to close a parenthetical");
    return;
  }

  // Prevent forgery of the dice command (3 dice are rolled...)
  bool is_carat = FALSE;
  bool has_valid_content = FALSE;
  for (const char *ptr = storage_buf; *ptr; ptr++) {
    if (is_carat) {
      is_carat = FALSE;
      continue;
    }

    if (isdigit(*ptr)) {
      send_to_char(ch, "The first word of your emote cannot contain a digit.\r\n");
      return;
    }

    if (*ptr == '^')
      is_carat = TRUE;
    else if (*ptr == ' ')
      break;
    else if (isalpha(*ptr))
      has_valid_content = TRUE;
  }

  // If we didn't find any valid first-word content after anti-forgery processing, bail out.
  FAILURE_CASE(!has_valid_content, "You can't begin your emote with blank space.\r\n");

  // Scan the emote for language values. You can only use languages you know, and only up to certain word lengths.
  if (!IS_NPC(ch)) {
    // Use the language you're speaking, unless it's somehow invalid-- then use English.
    int language_in_use = GET_VIABLE_LANGUAGE(ch);

    for (int i = 0; i < (int) strlen(storage_buf); i++) {
      // Skip everything that's not speech.
      if (storage_buf[i] != '"')
        continue;

      // We only accept parenthetical language as the very first thing in the sentence.
      if (storage_buf[++i] == '(') {
        i++;

        char language_string[100];
        int language_idx;

        // Copy the language into the language_string buffer, stopping at the next parenthetical.
        for (language_idx = i;
             language_idx - i < (int) sizeof(language_string) - 1 && storage_buf[language_idx] != ')';
             language_idx++)
          language_string[language_idx - i] = storage_buf[language_idx];

        // Null-terminate language buffer.
        language_string[language_idx - i] = '\0';

        // We expect that the language_idx pointer is at the closing parens. If this is not true, we ran out of buf
        // space-- that means this is not a language! Just bail out.
        if (storage_buf[language_idx] != ')') {
          continue;
        }

        // Identify the language in use.
        int skill_num;
        for (skill_num = SKILL_ENGLISH; skill_num < MAX_SKILLS; skill_num++) {
          if (!SKILL_IS_LANGUAGE(skill_num))
            continue;

          if (!str_cmp(skills[skill_num].name, language_string))
            break;
        }

        // Language identified.
        if (SKILL_IS_LANGUAGE(skill_num)) {
          language_in_use = skill_num;
          i = language_idx + 1;
        } else {
          send_to_char(ch, "'%s' is not a language.\r\n", language_string);
          return;
        }
      }

      if (GET_SKILL(ch, language_in_use) <= 0) {
        send_to_char(ch, "You don't know how to speak %s.\r\n", skills[language_in_use].name);
        return;
      }

      // Extract the spoken content and check it for length.
      char *ptr = storage_string;
      while (i < (int) strlen(storage_buf) && storage_buf[i] != '"')
        *(ptr++) = storage_buf[i++];
      *ptr = '\0';

      if (!has_required_language_ability_for_sentence(ch, storage_string, language_in_use))
        return;
    }
  }

  // Only questors and staff can echo without their names.
  bool must_echo_with_name = !(!(ch->desc) || access_level(ch, LVL_BUILDER) || PRF_FLAGGED(ch, PRF_QUESTOR));

  // All checks done, we're clear to emote. We know that storage_buf gets reused later, so let's save our emote from that fate.
  char emote_buf[strlen(storage_buf) + 1];
  strlcpy(emote_buf, storage_buf, sizeof(emote_buf));

  ch->char_specials.last_social_action = 0;

  // Iterate over the viewers in the room or vehicle.
  for (struct char_data *viewer = in_room ? in_room->people : in_veh->people;
       viewer;
       viewer = in_room ? viewer->next_in_room : viewer->next_in_veh)
  {
    // If they've ignored you, no luck.
    if (IS_IGNORING(viewer, is_blocking_ic_interaction_from, ch))
      continue;

    // If it's aecho, only send to people who see astral.
    if (subcmd == SCMD_AECHO && !(IS_ASTRAL(viewer) || IS_DUAL(viewer)))
      continue;

    // If they're insensate, nvm.
    if (!AWAKE(viewer) || PLR_FLAGGED(viewer, PLR_REMOTE) || PLR_FLAGGED(viewer, PLR_MATRIX))
      continue;

    // Since the viewer is a valid target, send it to them. Yes, ch is deliberately a possible viewer.
    send_echo_to_char(ch, viewer, (const char *) emote_buf, must_echo_with_name);
  }

  // Send it to anyone who's rigging a vehicle here.
  for (struct veh_data *veh = in_room ? in_room->vehicles : in_veh->carriedvehs;
       veh;
       veh = veh->next_veh)
  {
    if (veh->rigger && veh->rigger->desc)
      send_echo_to_char(ch, veh->rigger, (const char *) emote_buf, must_echo_with_name);
  }
}

bool has_required_language_ability_for_sentence(struct char_data *ch, const char *message, int language_skill) {
  if (IS_NPC(ch))
    return TRUE;

  int skill_level = GET_SKILL(ch, language_skill);

  // Masters of the language can say anything.
  if (skill_level >= 12)
    return TRUE;

  int max_allowable = max_allowable_word_length_at_language_level(skill_level);

  char current_word[500];
  char too_long_words[MAX_STRING_LENGTH];
  memset(too_long_words, '\0', sizeof(too_long_words));

  char *ptr = current_word;
  int num_words = 0;

  // We specifically use <=, because we know this is null-terminated and we want to act on the null at the end.
  for (int i = 0; i <= (int) strlen(message); i++) {
    if (isalpha(message[i])) {
      // Skip capitalized words like Aztechnology.
      if ((i == 0 || (!isalpha(message[i-1]))) && isupper(message[i]) && (i+1 < (int) strlen(message) && isalpha(message[i+1]) && !isupper(message[i+1]))) {
        while (isalpha(message[++i]) && i < (int) strlen(message));
        continue;
      } else {
        // Yelling or not capitalized? Start evaluating.
        *(ptr++) = message[i];
      }
    }

    else {
      *ptr = '\0';
      if ((int) strlen(current_word) > max_allowable) {
        if (too_long_words[0] != '\0')
          strlcat(too_long_words, "', '", sizeof(too_long_words));
        strlcat(too_long_words, current_word, sizeof(too_long_words));
        num_words++;
      }
      ptr = current_word;
    }

    if (message[i] == '"')
      break;
  }

  if (too_long_words[0] != '\0') {
    send_to_char(ch,
                 "The following word%s too long for %s's understanding of %s: '%s'.\r\n"
                 "Please limit your speech to words of length %d or less.\r\n",
                 num_words != 1 ? "s are" : " is",
                 GET_CHAR_NAME(ch),
                 skills[language_skill].name,
                 too_long_words,
                 max_allowable);
    return FALSE;
  }

  return TRUE;
}

// When listening, you get too-long words replaced with random ones.
// Known bug: 'Sentence. Non-name sentence' will lose non-name.
const char *replace_too_long_words(struct char_data *ch, struct char_data *speaker, const char *message, int language_skill, const char *terminal_code, bool override_skill_to_zero) {
  static char replaced_message[MAX_STRING_LENGTH];
  static char storage_buf[MAX_STRING_LENGTH];
  char name_buf[100];
  const char *random_word = NULL;

  // No character? No processing.
  if (!ch) {
    mudlog("SYSERR: Received NULL character to replace_too_long_words().", ch, LOG_SYSLOG, TRUE);
    return message;
  }

  // NPCs get no processing, so let's short-circuit and just bail out.
  if (IS_NPC(ch))
    return message;

  if (!message) {
    mudlog("SYSERR: Received NULL message to replace_too_long_words().", ch, LOG_SYSLOG, TRUE);
    return message;
  }

  if (!SKILL_IS_LANGUAGE(language_skill)) {
    snprintf(storage_buf, sizeof(storage_buf), "SYSERR: Received invalid skill %d to replace_too_long_words().", language_skill);
    mudlog(storage_buf, ch, LOG_SYSLOG, TRUE);
    return message;
  }

  if (terminal_code == NULL) {
    mudlog("SYSERR: Received NULL terminal code to replace_too_long_words().", ch, LOG_SYSLOG, TRUE);
    return message;
  }

  PSEUDOLANGUAGE_REPLACEMENT_DEBUG(ch, "Received message for pseudolanguage mangle: '%s'\r\n", double_up_color_codes(message));

  const char *viewer_highlight = GET_CHAR_COLOR_HIGHLIGHT(ch);

  // We always want the viewer's name to stand out, so we make sure it never matches the background/terminal code.
  // KNOWN ISSUE: This does not work with xterm256 codes. Anyone want to write that logic?
  if (!strcmp((viewer_highlight = string_to_uppercase(viewer_highlight)), terminal_code))
    viewer_highlight = string_to_lowercase(viewer_highlight);

  // Calculate their max allowable word length.
  int max_allowable = max_allowable_word_length_at_language_level(GET_SKILL(ch, language_skill));
  // Override it if that's been configured. Used only for the Discord bot.
  if (override_skill_to_zero && language_skill != SKILL_ENGLISH)
    max_allowable = 0;

  // If they don't want pseudolanguage strings, reflect that here.
  bool no_pseudolanguage = PRF_FLAGGED(ch, PRF_NOPSEUDOLANGUAGE);

  // Before doing any operations on the string, we strip out color codes (we'll apply our own as needed).
  strlcpy(replaced_message, get_string_after_color_code_removal(message, NULL), sizeof(replaced_message));

  PSEUDOLANGUAGE_REPLACEMENT_DEBUG(ch, "After color code removal: '%s'\r\n", replaced_message);

  // If the speech starts with a single parenthezised word, and that word is a language name, we don't touch it.
  char *ptr = replaced_message;
  if (*ptr == '(') {
    char *verification_ptr = ptr + 1;
    // Scan the whole ptr looking for a close-parens without illegal characters.
    for (; *verification_ptr; verification_ptr++) {
      PSEUDOLANGUAGE_REPLACEMENT_DEBUG(ch, "^y%c^n", *verification_ptr);
      if (*verification_ptr == ')')
        break;

      // Apostrophe is allowed due to Or'zet being a language.
      if (!isalpha(*verification_ptr) && *verification_ptr != '\'')
        break;
    }

    // Found a closing parens with no illegal characters before it? Skip the whole thing, it's a language tag.
    if (*verification_ptr == ')')
      ptr = ++verification_ptr;
  }

  // We specifically use <=, because we know this is null-terminated and we want to act on the null at the end.
  bool at_start_of_new_sentence = TRUE;
  bool need_caps = FALSE;
  for (; ptr && (ptr - replaced_message) <= (int) strlen(replaced_message); ptr++) {
    random_word = NULL;
    need_caps = FALSE;
    PSEUDOLANGUAGE_REPLACEMENT_DEBUG(ch, "^y%c^n", *ptr);

    // We only care about characters commonly found in words.
    if (isalpha(*ptr) || *ptr == '\'' || *ptr == '-') {
      // Capitalized words are flagged for further checking.
      bool yelling = isupper(*ptr) && isupper(*(ptr + 1));
      bool capital_word = !yelling && isupper(*ptr) && isalpha(*(ptr + 1));

      if (capital_word) {
        PSEUDOLANGUAGE_REPLACEMENT_DEBUG(ch, "^r^^^n", *ptr);
      } if (yelling) {
        PSEUDOLANGUAGE_REPLACEMENT_DEBUG(ch, "^M!^n", *ptr);
      }

      if (at_start_of_new_sentence) {
        at_start_of_new_sentence = FALSE;
        need_caps = TRUE;
      }

      // Skim to the end of the word. Words must match [a-zA-Z'\-]+
      char *word_ptr = ptr;
      while (isalpha(*(word_ptr)) || *word_ptr == '\'' || *word_ptr == '-') {
        word_ptr++;
        PSEUDOLANGUAGE_REPLACEMENT_DEBUG(ch, "^g%c^n", *word_ptr);
      }
      // word_ptr now points at the character after the end of the word.

      // Check to see if we're at the end of a sentence or dealing with a capital word.
      if (capital_word || *(word_ptr) == '.' || *(word_ptr) == '!' || *(word_ptr) == '?') {
        // Possibly. Make sure word is not a common title. This means we actually care about the contents of the word, so clone it out.
        int storage_idx = 0;
        for (; storage_idx < (word_ptr - ptr); storage_idx++) {
          tag_check_string[storage_idx] = *(ptr + storage_idx);
        }
        // And zero-terminate it.
        tag_check_string[storage_idx] = 0;

        // Could be a name, let's check.
        if (capital_word) {
          // If they allow highlights, and the name matches theirs...
          bool should_highlight = FALSE;
          if (!PRF_FLAGGED(ch, PRF_NOHIGHLIGHT) && !PRF_FLAGGED(ch, PRF_NOCOLOR)
              && (!strcmp(tag_check_string, GET_CHAR_NAME(ch)))) // Eventual todo: add a memory check of the speaker, so 'remem x buttface' will highlight buttface for the listener.
          {
            should_highlight = TRUE;
          }

          // Go through memory.
          if (!should_highlight && speaker && !IS_NPC(speaker)) {
            struct remem *mem_record = safe_found_mem(speaker, ch);
            if (mem_record && !str_cmp(mem_record->mem, tag_check_string))
              should_highlight = TRUE;
          }

          if (should_highlight) {
            // It's their name! We get to swap out the replacement word with our highlighted name and go about our day.
            snprintf(name_buf, sizeof(name_buf), "%s%s%s",
                    viewer_highlight,
                    tag_check_string,
                    terminal_code);
            random_word = name_buf;
            PSEUDOLANGUAGE_REPLACEMENT_DEBUG(ch, "\r\nName identified. name_buf='%s' (composed from v_h='%s', t_c_s='%s', t_c='%s')\r\n",
                                                 name_buf,
                                                 viewer_highlight,
                                                 tag_check_string,
                                                 terminal_code);
          } else {
            if (!need_caps)
              random_word = tag_check_string;
          }
        }

        // End of sentence, or just an abbreviated title like 'Mr'?
        if (*(word_ptr) == '.' || *(word_ptr) == '!' || *(word_ptr) == '?') {
          at_start_of_new_sentence = TRUE;
          if (*(word_ptr) == '.') {
            for (int i = 0; *allowed_abbreviations[i] != '\n' && at_start_of_new_sentence; i++)
              if (!strcmp(allowed_abbreviations[i], tag_check_string))
                at_start_of_new_sentence = FALSE;
          }

          PSEUDOLANGUAGE_REPLACEMENT_DEBUG(ch, "\r\nAfter '%s' (v2), sentence mode is now %s\r\n", tag_check_string, at_start_of_new_sentence ? "on" : "off");
        }
      }

      int len = word_ptr - ptr;
      if (random_word || len > max_allowable) {
        // Terminate the replaced message string at the beginning of the invalid word.
        *ptr = 0;

        if (!random_word) {
          // Grab our replacement word.
          if (no_pseudolanguage)
            random_word = "...";
          else
            random_word = need_caps ? capitalize(get_random_word_from_lexicon(language_skill)) : get_random_word_from_lexicon(language_skill);
        }

        need_caps = FALSE;

        // Replace the word in the sentence and dump it to storage_buf.
        snprintf(storage_buf, sizeof(storage_buf), "%s%s%s",
                 replaced_message,  // Message up to beginning of invalid word.
                 yelling ? string_to_uppercase(random_word) : random_word, // a random word in the language
                 word_ptr);         // the rest of the owl-- I mean, the rest of the sentence from the end of the word.

        // Write over replaced_message with storage_buf.
        strlcpy(replaced_message, storage_buf, sizeof(replaced_message));

        ptr += strlen(random_word) - 1;
      } else {
        ptr = word_ptr - 1;
      }
    }
  }

  return replaced_message;
}
