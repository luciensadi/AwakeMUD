#include "structs.h"
#include "interpreter.h"
#include "utils.h"
#include "comm.h"
#include "constants.h"
#include "handler.h"

char mutable_echo_string[MAX_STRING_LENGTH];
char tag_check_string[MAX_STRING_LENGTH];
char storage_string[MAX_STRING_LENGTH];

ACMD(do_highlight) {
  skip_spaces(&argument);
  DELETE_ARRAY_IF_EXTANT(SETTABLE_CHAR_COLOR_HIGHLIGHT(ch));
  SETTABLE_CHAR_COLOR_HIGHLIGHT(ch) = str_dup(argument);
  send_to_char(ch, "OK, your highlight is now %s*^n.\r\n", GET_CHAR_COLOR_HIGHLIGHT(ch));
}

const char *generate_display_string_for_character(struct char_data *actor, struct char_data *viewer, struct char_data *target_ch, bool terminate_with_actors_color_code) {
  static char result_string[MAX_STRING_LENGTH];
  struct remem *mem_record;
  const char *terminal_code = "^n";
  const char *viewer_highlight = GET_CHAR_COLOR_HIGHLIGHT(viewer);
  
  if (terminate_with_actors_color_code) {
#ifdef SPEECH_COLOR_CODE_DEBUG
    send_to_char(actor, "Terminating with your color code.\r\n");
#endif
    terminal_code = GET_CHAR_COLOR_HIGHLIGHT(actor);
    
    // Make sure there's differentiation between highlight colors.
    if (target_ch == viewer && !strcmp(viewer_highlight, terminal_code)) {
      if (!strcmp((viewer_highlight = string_to_uppercase(viewer_highlight)), terminal_code))
        viewer_highlight = string_to_lowercase(viewer_highlight);
#ifdef SPEECH_COLOR_CODE_DEBUG
      send_to_char(actor, "Viewer code changed from %s*^n to %s*^n for %s.\r\n", 
                   GET_CHAR_COLOR_HIGHLIGHT(viewer), viewer_highlight, GET_CHAR_NAME(viewer));
#endif
    }
  }
  
  // If the target is the viewer, we don't process their name into a desc, but we do highlight.
  if (target_ch == viewer) {
    // Insert the color code sequence for the viewing character's speech color.
    if (IS_ASTRAL(viewer))
      snprintf(result_string, sizeof(result_string), "%s%s's reflection%s", 
               viewer_highlight, 
               GET_CHAR_NAME(viewer),
               terminal_code);
    else if (PLR_FLAGGED(viewer, PLR_MATRIX))
      snprintf(result_string, sizeof(result_string), "%s%s's persona%s", 
               viewer_highlight, 
               GET_CHAR_NAME(viewer),
               terminal_code);
    else
      snprintf(result_string, sizeof(result_string), "%s%s%s", 
               viewer_highlight, 
               GET_CHAR_NAME(viewer),
               terminal_code);
  }
  
  // If the target is not the viewer, we process it as normal.
  else {
    const char *display_string;
    
    // Switch between display strings based on if the viewer can see and knows the target character.
    if (!CAN_SEE(viewer, target_ch))
      display_string = "someone";
    else if ((mem_record = found_mem(GET_MEMORY(viewer), target_ch)))
      display_string = CAP(mem_record->mem);
    else
      display_string = GET_NAME(target_ch);
      
    // Insert the display string into the mutable string in place of the tag. No terminal_code here-- this always happens outside of speech.
    snprintf(result_string, sizeof(result_string), "%s^n", display_string);
  }
  
  return result_string;
}

//#define NEW_EMOTE_DEBUG
void send_echo_to_char(struct char_data *actor, struct char_data *viewer, const char *echo_string) {
  int tag_index, i;
  struct char_data *target_ch = NULL;
  
  // Sanity check.
  if (!actor || !viewer) {
    mudlog("SYSERR: Received null actor or viewer to send_emote_to_char!", actor, LOG_SYSLOG, TRUE);
    return;
  }
  
  // Don't bother processing emotes for those who can't appreciate them.
  if (!viewer->desc)
    return;
  
#ifdef NEW_EMOTE_DEBUG
  send_to_char(actor, "\r\n\r\nBeginning evaluation for %s.\r\n", GET_CHAR_NAME(viewer));
#endif
  
  // Scan the string for the actor's name. This is an easy check. In the process, convert echo_string into something mutable, and set i to skip over this new text.
  if (str_str(echo_string, GET_CHAR_NAME(actor)) == NULL) {
    if (echo_string[0] == '\'' && echo_string[1] == 's') {
      snprintf(mutable_echo_string, sizeof(mutable_echo_string), "@self%s", echo_string);
    } else {
      snprintf(mutable_echo_string, sizeof(mutable_echo_string), "@self %s", echo_string);
    }
  } else {
    strlcpy(mutable_echo_string, echo_string, sizeof(mutable_echo_string));
  }

#ifdef NEW_EMOTE_DEBUG
  send_to_char(actor, "\r\nAfter first pass, mutable_echo_string is '%s'. Evaluating...\r\n", mutable_echo_string);
#endif  

  // Next, check capitalized words in it for highlighting purposes.
  // TODO: Skip this if they've disabled color or have disabled name highlighting.
  bool quote_mode = FALSE;
  for (i = 0; i < (int) strlen(mutable_echo_string); i++) {
#ifdef NEW_EMOTE_DEBUG
    send_to_char(actor, "^y%c%s^n", mutable_echo_string[i], mutable_echo_string[i] == '^' ? "^" : "");
#endif
    
    // Quote mark? Enable quote mode, which stops expansion of names and just does highlighting.
    if (mutable_echo_string[i] == '"') {
      quote_mode = !quote_mode;
#ifdef NEW_EMOTE_DEBUG
      send_to_char(actor, "\r\nQuote mode is now %s.\r\n", quote_mode ? "on" : "off");
#endif
      continue;
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
      bool require_exact_match = !at_mode && strlen(tag_check_string) < 5;
      
#ifdef NEW_EMOTE_DEBUG
      if (require_exact_match)
        send_to_char(actor, "\r\nUsing exact mode for this evaluation due to non-at and low string length %d.", strlen(tag_check_string));
#endif

      target_ch = NULL;

      // Short-circuit check: @self.
      bool self_mode = !str_cmp("self", tag_check_string) || !str_cmp("me", tag_check_string) || !str_cmp("myself", tag_check_string);
      if (self_mode)
        target_ch = actor;
      
      // Compare it to bystanders' memorized names
      if (!target_ch) {
        for (target_ch = actor->in_room ? actor->in_room->people : actor->in_veh->people; 
             target_ch; 
             target_ch = actor->in_room ? target_ch->next_in_room : target_ch->next_in_veh) 
        {
          // No such thing as remembering an NPC.
          if (IS_NPC(target_ch))
            continue;
            
          // Can't target someone you can't see.
          if (!CAN_SEE(actor, target_ch))
            continue;
            
          struct remem *mem_record = found_mem(GET_MEMORY(actor), target_ch);
          if (mem_record) {
            if (require_exact_match) {
              if (!str_cmp(mem_record->mem, tag_check_string))
                break;
            } else {
              if (!strn_cmp(mem_record->mem, tag_check_string, strlen(tag_check_string)))
                break;
            }
          }
        }
        #ifdef NEW_EMOTE_DEBUG
              if (target_ch)
                send_to_char(actor, "\r\nWith target string '%s', found %s by memory.\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
        #endif
      }
      
      // Didn't find anyone by that memorized name? Check PC names.
      if (!target_ch) {
        for (target_ch = actor->in_room ? actor->in_room->people : actor->in_veh->people; 
             target_ch; 
             target_ch = actor->in_room ? target_ch->next_in_room : target_ch->next_in_veh) 
        {
          // NPCs don't have player data to check.
          if (IS_NPC(target_ch))
            continue;
            
          // Can't target someone you can't see.
          if (!CAN_SEE(actor, target_ch))
            continue;
            
          if (require_exact_match) {
            // In exact match mode, we must match the full and complete name. Happens if the name is too short.
            if (!str_cmp(target_ch->player.char_name, tag_check_string))
              break;
          } else {
            // Otherwise, we can match up to the length of tag_check_string.
            if (!strn_cmp(target_ch->player.char_name, tag_check_string, strlen(tag_check_string)))
              break;
          }
        }
#ifdef NEW_EMOTE_DEBUG
        if (target_ch)
          send_to_char(actor, "\r\nWith target string '%s', found %s by name.\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
#endif
      }
      
      // Didn't find anyone by their PC name? Check keywords (only if not in exact-match mode).
      if (!target_ch && !require_exact_match) {
        for (target_ch = actor->in_room ? actor->in_room->people : actor->in_veh->people; 
             target_ch; 
             target_ch = actor->in_room ? target_ch->next_in_room : target_ch->next_in_veh) 
        {
          // Can't target someone you can't see.
          if (!CAN_SEE(actor, target_ch))
            continue;
            
          if (str_str(GET_KEYWORDS(target_ch), tag_check_string)) {
            // Found someone, stop looking.
            break;
          }
        }
#ifdef NEW_EMOTE_DEBUG
        if (target_ch)
          send_to_char(actor, "\r\nWith target string '%s', found %s by alias.\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
#endif
      }
      
      // Didn't find anyone by their keywords? Check in short description (only if not in exact-match mode).
      if (!target_ch && !require_exact_match) {
        for (target_ch = actor->in_room ? actor->in_room->people : actor->in_veh->people; 
             target_ch; 
             target_ch = actor->in_room ? target_ch->next_in_room : target_ch->next_in_veh) 
        {
          // Can't target someone you can't see.
          if (!CAN_SEE(actor, target_ch))
            continue;
            
          if (str_str(GET_NAME(target_ch), tag_check_string)) {
            // Found someone, stop looking.
            break;
          }
        }
#ifdef NEW_EMOTE_DEBUG
        if (target_ch)
          send_to_char(actor, "\r\nWith target string '%s', found %s by alias.\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
#endif
      }
      
      // Found someone.
      if (target_ch) {
        // If the viewer is the actor, and it's not @self, just stop-- we don't self-highlight.
        if (viewer == target_ch && viewer == actor && !self_mode)
          continue;
          
        // In quote mode, we only do viewer highlights.
        if (quote_mode && target_ch != viewer)
          continue;
          
        // Copy out the rest of the string-- we're inserting something here.
        strncpy(storage_string, mutable_echo_string + tag_index, sizeof(storage_string) - 1);
        
        // Fetch the representation of this character. Edge case: actor is viewer and @self: just give your name.
        const char *display_string;
        if (self_mode && viewer == target_ch && viewer == actor)
          display_string = GET_CHAR_NAME(actor);
        else
          display_string = generate_display_string_for_character(actor, viewer, target_ch, quote_mode);
        
        // Put it into the mutable echo string, replacing the tag, and then append the rest of the storage string.
        snprintf(mutable_echo_string + i - (at_mode ? 1 : 0), sizeof(mutable_echo_string), "%s%s", display_string, storage_string);
  
        // Since we added X known-good characters starting at i, increase i by X-- but decrement by 1 since we'll be incrementing on loop.
        i += strlen(display_string) - 1;
#ifdef NEW_EMOTE_DEBUG
        send_to_char(actor, "'%s' resolved to %s.\r\n", tag_check_string, display_string);
#endif
      }
      
      else {
#ifdef NEW_EMOTE_DEBUG
        send_to_char(actor, "\r\nTag string '%s' found nobody.\r\n", tag_check_string);
#endif
        i += strlen(tag_check_string) - 1;
      }
      
      // We're ready to move on. i has been incremented, and all necessary writing has been done.
    }
  }
  
#ifdef NEW_EMOTE_DEBUG
  send_to_char(actor, "\r\nTagging pass of emote done. Now doing speech pass. Combine these later...");
#endif
  
  // Next pass: Convert speech.
  int language_in_use = -1;
  for (i = 0; i < (int) strlen(mutable_echo_string); i++) {
#ifdef NEW_EMOTE_DEBUG_SPEECH
    send_to_char(actor, "^y%c%s^n", mutable_echo_string[i], mutable_echo_string[i] == '^' ? "^" : "");
#endif
    // Skip everything that's not speech.
    if (mutable_echo_string[i] != '"')
      continue;
    
    // It's a quote mark. Set up our vars and increment i by one so it's pointing at the start of the speech.
    language_in_use = GET_LANGUAGE(actor);
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
      for (skill_num = SKILL_ENGLISH; skill_num <= SKILL_FRENCH; skill_num++) {
        if (!SKILL_IS_LANGUAGE(skill_num))
          continue;
          
        if (!str_cmp(skills[skill_num].name, language_string))
          break;
      }
      
      // Language identified. TODO: Ensure actor has skill in language.
      if (SKILL_IS_LANGUAGE(skill_num))
        language_in_use = skill_num;
      else
        language_in_use = GET_LANGUAGE(actor);
        
#ifdef NEW_EMOTE_DEBUG_SPEECH
      send_to_char(actor, "\r\nLanguage in use is now %s (target: '%s').\r\n", skills[language_in_use].name, language_string);
#endif
        
      // Snip the language block from the mutable string. Handles a single space after the closing parens.
      if (language_idx + 1 < (int) strlen(mutable_echo_string) && isspace(mutable_echo_string[language_idx + 1]))
        strlcpy(storage_string, mutable_echo_string + language_idx + 2, sizeof(storage_string));
      else
        strlcpy(storage_string, mutable_echo_string + language_idx + 1, sizeof(storage_string));
        
      // Decrement i here to make it point at the area where the parens is.
      i--;
      
      strlcpy(mutable_echo_string + i, storage_string, sizeof(mutable_echo_string) - i);
      
      // i now points to the character at the start of the speech.
    }
    
#ifdef NEW_EMOTE_DEBUG_SPEECH
    send_to_char(actor, "\r\nAfter language extraction (now %s), mutable string is: '%s'\r\n", skills[language_in_use].name, mutable_echo_string);
#endif
    
    // We're now inside a quote block, with the language for the quote identified. 'i' is the start of the speech.
    char speech_buf[MAX_STRING_LENGTH];
    
    // Copy the quote into the speech buf.
    int speech_idx;
    for (speech_idx = i; speech_idx < (int) strlen(mutable_echo_string) && mutable_echo_string[speech_idx] != '"'; speech_idx++)
      speech_buf[speech_idx - i] = mutable_echo_string[speech_idx];
    speech_buf[speech_idx - i] = '\0';
    
    // Got it? Good.
#ifdef NEW_EMOTE_DEBUG_SPEECH
    send_to_char(actor, "\r\nSpeech block: '%s'\r\n", speech_buf);
#endif
    
    // If the speech ended with punct, we use that, otherwise we use a period.
    const char *quote_termination;
    if (ispunct(mutable_echo_string[speech_idx - 1])) {
      quote_termination = "^n";
    } else {
      quote_termination = ".^n";
    }
    
    snprintf(storage_string, sizeof(storage_string), "%s%s", quote_termination, mutable_echo_string + speech_idx);
    
    // If the listener can't understand you, mangle it.
    if (GET_SKILL(viewer, language_in_use) <= 0) {
      const char *replacement = "(something unintelligible)";
      quote_termination = ".^n";
      
      snprintf(mutable_echo_string + i, sizeof(mutable_echo_string) - i, "%s%s%s", 
               GET_CHAR_COLOR_HIGHLIGHT(actor),
               replacement, 
               storage_string);
               
      i += strlen(replacement) + strlen(GET_CHAR_COLOR_HIGHLIGHT(actor)) + strlen(quote_termination);
    } else {
      // Just highlight the speech.
      snprintf(mutable_echo_string + i, sizeof(mutable_echo_string) - i, "%s%s%s", 
               GET_CHAR_COLOR_HIGHLIGHT(actor),
               speech_buf, 
               storage_string);
               
      i = speech_idx + strlen(GET_CHAR_COLOR_HIGHLIGHT(actor)) + strlen(quote_termination);
    }
    i++;
  }
  
#ifdef NEW_EMOTE_DEBUG_SPEECH
  send_to_char(actor, "Finished evaluation of emote projection for %s.\r\n\r\n", GET_CHAR_NAME(viewer));
#endif
  // Finally(!), send it to the viewer.
  send_to_char(viewer, capitalize(mutable_echo_string));
}

ACMD(do_new_echo) {
  /* 
  
  Echos (which is what all this stuff really is, just reflavored echoes) can be flagged as astral-only. If not flagged as such, they go to all characters in the room.
  
  No more first person. It's all done by character name. Saves us a ton of grammatical transformations.
  
  If you're tagged in an emote, your character name is highlighted.
  
  You can set speech auto-color for your character. Other people can configure this to not show for them.
  
  Since we've already figured out speech detection above, we can tie in pseudolanguage here as well.
  
  (done) Speech in silent rooms is suppressed. If you emote with speech in a silent room, you get an error.
  
  We allow the following special things to happen in echos, emotes, etc:
  
    > emote With a sigh, Lucien steps up to the bar.
    
    Auto-flags Lucien as self, echos appropriately.
    
    > emote The ceiling falls.
    
    Without special privileges like 'questor', fails and requires you to put your character name in it.
    
    > emote nods to Testmort, then glances at @scruffy. "What's up?"
    
    As Testmort is capitalized, this takes Testmort's name as a tag. Otherwise, you can @-tag and it will look 
      for name->memory->keyword in that order. Failure to resolve an @ means your emote won't send.
      
    > emote A sharp whistle lets Lucien grab Testmort's attention, and he points in @scruffy's direction.
    
    The possessive is not consumed by the tag. Any non-letter breaks the tag immediately, so the possessive remains.
    
    > emote 's eyebrow raises.
    
    The possessive is contracted against the actor's name.
    
  things to watch for: forged rolls and other forged output with this command.
  */
  
  /* Logic for the above:
  
    for each character in room, analyze string, finding tags and replacing them with the appropriate rendering of the character.
    
    I'd love to do the analysis in one pass and have an arbitrary set of tags and speech that can be processed more easily,
      but I don't have a method for doing that in mind right now. Something to revisit later.
      
  */
  
  // Reject hitchers. They have no body and would break things.
  if (PLR_FLAGGED(ch, PLR_MATRIX) && !ch->persona) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  
  skip_spaces(&argument);
  
  // Require an argument.
  if (!*argument) {      
    send_to_char("Yes... but what?\r\n", ch);
    return;
  }
  
  // Can't speak? No emote speech for you.
  if (strchr(argument, '"') != NULL && !char_can_make_noise(ch))
    return;
    
  // Scan for mismatching quotes or mismatching parens. Reject if found.
  int quotes = 0, parens = 0;
  for (int i = 0; i < (int) strlen(argument); i++) {
    if (argument[i] == '"')
      quotes++;
    else if (argument[i] == '(')
      parens++;
    else if (argument[i] == ')')
      parens--;
  }
  if (quotes % 2) {
    send_to_char("There's an error in your emote: You need to close a quote.\r\n", ch);
    return;
  }
  if (parens != 0) {
    send_to_char("There's an error in your emote: You need to close a parenthetical.\r\n", ch);
    return;
  }
  
  // Scan for oversized words. Word length is limited by your skill level in the language.
  
  // TODO: Handling for the various commands.
  // emote -> echo, no change needed
  // aecho -> echo with astral flag
  bool astral_only = (subcmd == SCMD_AECHO);
  
  // Iterate over the viewers in the room.
  for (struct char_data *viewer = ch->in_room ? ch->in_room->people : ch->in_veh->people; 
       viewer; 
       viewer = ch->in_room ? viewer->next_in_room : viewer->next_in_veh) {
    // If the viewer is a valid target, send it to them. Yes, ch is deliberately a possible viewer.
    if (subcmd != SCMD_AECHO || IS_ASTRAL(viewer))
      send_echo_to_char(ch, viewer, argument);
  }
}
