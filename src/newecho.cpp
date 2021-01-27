#include "structs.h"
#include "interpreter.h"
#include "utils.h"
#include "comm.h"
#include "constants.h"
#include "handler.h"
#include "newdb.h"
#include "lexicons.h"

char mutable_echo_string[MAX_STRING_LENGTH];
char tag_check_string[MAX_STRING_LENGTH];
char storage_string[MAX_STRING_LENGTH];

bool has_required_language_ability_for_sentence(struct char_data *ch, const char *message, int language_skill);
const char *get_random_word_from_lexicon(int language_skill);
const char *generate_random_lexicon_sentence(int language_skill, int length);
const char *replace_too_long_words(struct char_data *ch, const char *message, int language_skill);

// #define NEW_EMOTE_DEBUG(ch, ...) send_to_char((ch), ##__VA_ARGS__)
#define NEW_EMOTE_DEBUG(...)

// #define NEW_EMOTE_DEBUG_SPEECH(ch, ...) send_to_char((ch), ##__VA_ARGS__)
#define NEW_EMOTE_DEBUG_SPEECH(...)

// #define SPEECH_COLOR_CODE_DEBUG(ch, ...) send_to_char((ch), ##__VA_ARGS__)
#define SPEECH_COLOR_CODE_DEBUG(...)



/*
 TODO:
 - capital words in speech should be allowed: ex Aztechnology, but not AZTECHNOLOGY or aztechnology
*/

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
  
  if (*argument != '^' || !isalpha(*(argument + 1))) {
    send_to_char("You need to specify a color code. Example: 'highlight ^^r'. Use '^^n' for neutral / no highlight.\r\n", ch);
    return;
  }
  
  if (*(argument + 1) == 'l') {
    send_to_char("Sorry, you can't use ^^l as a highlight. It's too dark for many people to read easily.\r\n", ch);
    return;
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
    case 1: return 3;
    case 2: return 4;
    case 3: return 5;
    case 4: return 6;
    case 5: return 8;
    case 6: return 10;
    case 7: return 11;
    case 8: return 12;
    case 9: return 14;
    case 10: return 16;
    case 11: return 18;
    default: return 500;
  }
}

const char *generate_display_string_for_character(struct char_data *actor, struct char_data *viewer, struct char_data *target_ch, bool terminate_with_actors_color_code) {
  static char result_string[MAX_STRING_LENGTH];
  struct remem *mem_record;
  const char *terminal_code = "^n";
  bool should_highlight = !PRF_FLAGGED(viewer, PRF_NOHIGHLIGHT) && !PRF_FLAGGED(viewer, PRF_NOCOLOR);
  const char *viewer_highlight = should_highlight ? GET_CHAR_COLOR_HIGHLIGHT(viewer) : "";
  
  
  if (terminate_with_actors_color_code && should_highlight) {
    SPEECH_COLOR_CODE_DEBUG(actor, "Terminating with your color code.\r\n");
    terminal_code = GET_CHAR_COLOR_HIGHLIGHT(actor);
    
    // Make sure there's differentiation between highlight colors.
    if (target_ch == viewer && !strcmp(viewer_highlight, terminal_code)) {
      if (!strcmp((viewer_highlight = string_to_uppercase(viewer_highlight)), terminal_code))
        viewer_highlight = string_to_lowercase(viewer_highlight);
      SPEECH_COLOR_CODE_DEBUG(actor, "Viewer code changed from %s*^n to %s*^n for %s.\r\n", 
                   GET_CHAR_COLOR_HIGHLIGHT(viewer), viewer_highlight, GET_CHAR_NAME(viewer));
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
    else if (viewer->desc && viewer->desc->original)
      snprintf(result_string, sizeof(result_string), "%s%s (you)%s", 
               viewer_highlight, 
               GET_NAME(viewer),
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

char newecho_debug_buf[MAX_STRING_LENGTH];
void send_echo_to_char(struct char_data *actor, struct char_data *viewer, const char *echo_string, bool require_char_name) {
  int tag_index, i;
  char punct_replacement[10];
  struct char_data *target_ch = NULL;
  
  // Sanity check.
  if (!actor || !viewer) {
    mudlog("SYSERR: Received null actor or viewer to send_emote_to_char!", actor, LOG_SYSLOG, TRUE);
    return;
  }
  
  // Don't bother processing emotes for those who can't appreciate them.
  if (!viewer->desc)
    return;
    
  bool should_highlight = !PRF_FLAGGED(viewer, PRF_NOHIGHLIGHT) && !PRF_FLAGGED(viewer, PRF_NOCOLOR);
  
  NEW_EMOTE_DEBUG(actor, "\r\n\r\nBeginning evaluation for %s.\r\n", GET_CHAR_NAME(viewer));
  
  // Scan the string for the actor's name. This is an easy check. In the process, convert echo_string into something mutable, and set i to skip over this new text.
  if (require_char_name && strstr(echo_string, GET_CHAR_NAME(actor)) == NULL && str_str(echo_string, "@self") == NULL) {
    if (echo_string[0] == '\'' && echo_string[1] == 's') {
      snprintf(mutable_echo_string, sizeof(mutable_echo_string), "@self%s", echo_string);
    } else {
      snprintf(mutable_echo_string, sizeof(mutable_echo_string), "@self %s", echo_string);
    }
  } else {
    strlcpy(mutable_echo_string, echo_string, sizeof(mutable_echo_string));
  }

  NEW_EMOTE_DEBUG(actor, "\r\nAfter first pass, mutable_echo_string is '%s'. Evaluating...\r\n", mutable_echo_string);

  // Next, check capitalized words in it for highlighting purposes, unless they've disabled highlighting or color.
  bool quote_mode = FALSE;
  for (i = 0; i < (int) strlen(mutable_echo_string); i++) {
    NEW_EMOTE_DEBUG(actor, "^y%c%s^n", mutable_echo_string[i], mutable_echo_string[i] == '^' ? "^" : "");
    
    // Quote mark? Enable quote mode, which stops expansion of names and just does highlighting.
    if (mutable_echo_string[i] == '"') {
      quote_mode = !quote_mode;
      NEW_EMOTE_DEBUG(actor, "\r\nQuote mode is now %s.\r\n", quote_mode ? "on" : "off");
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
      
      if (require_exact_match)
        NEW_EMOTE_DEBUG(actor, "\r\nUsing exact mode for this evaluation due to non-at and low string length %d.", strlen(tag_check_string));

      target_ch = NULL;

      // Short-circuit check: @self.
      bool self_mode = at_mode && (!str_cmp("self", tag_check_string) || !str_cmp("me", tag_check_string) || !str_cmp("myself", tag_check_string));
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
        if (target_ch)
          NEW_EMOTE_DEBUG(actor, "\r\nWith target string '%s', found %s by memory.\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
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
        if (target_ch)
          NEW_EMOTE_DEBUG(actor, "\r\nWith target string '%s', found %s by name.\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
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
        if (target_ch)
          NEW_EMOTE_DEBUG(actor, "\r\nWith target string '%s', found %s by alias.\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
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
        if (target_ch)
          NEW_EMOTE_DEBUG(actor, "\r\nWith target string '%s', found %s by alias.\r\n", tag_check_string, GET_CHAR_NAME(target_ch));
      }
      
      // Found someone.
      if (target_ch) {
        // If the viewer is the actor, and it's not @self, just stop-- we don't self-highlight.
        if (viewer == target_ch && viewer == actor && !self_mode)
          continue;
          
        // In quote mode, we only do viewer highlights. @-targets are considered invalid in quotes.
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
        NEW_EMOTE_DEBUG(actor, "'%s' resolved to %s.\r\n", tag_check_string, display_string);
      }
      
      else {
        NEW_EMOTE_DEBUG(actor, "\r\nTag string '%s' found nobody.\r\n", tag_check_string);
        i += strlen(tag_check_string) - 1;
      }
      
      // We're ready to move on. i has been incremented, and all necessary writing has been done.
    }
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
    language_in_use = IS_NPC(actor) ? SKILL_ENGLISH : GET_LANGUAGE(actor);
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
        language_in_use = IS_NPC(actor) ? SKILL_ENGLISH : GET_LANGUAGE(actor);
        
      NEW_EMOTE_DEBUG_SPEECH(actor, "\r\nLanguage in use for $n is now %s (target: '%s').\r\n", skills[language_in_use].name, language_string);
        
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
    
    NEW_EMOTE_DEBUG_SPEECH(actor, "\r\nAfter language extraction (now %s), mutable string is: '%s'\r\n", skills[language_in_use].name, mutable_echo_string);
  
    // We're now inside a quote block, with the language for the quote identified. 'i' is the start of the speech.
    char speech_buf[MAX_STRING_LENGTH];
    
    // Copy the quote into the speech buf.
    int speech_idx;
    for (speech_idx = i; speech_idx < (int) strlen(mutable_echo_string) && mutable_echo_string[speech_idx] != '"'; speech_idx++)
      speech_buf[speech_idx - i] = mutable_echo_string[speech_idx];
    speech_buf[speech_idx - i] = '\0';
    
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
    const char *replacement = NULL;
    if (GET_SKILL(viewer, language_in_use) <= 0) {
      if (PRF_FLAGGED(viewer, PRF_SCREENREADER))
        replacement = "(something unintelligble)";
      else
        replacement = generate_random_lexicon_sentence(language_in_use, strlen(speech_buf));
      
      char candidate_ch = get_final_character_from_string(speech_buf);
      if (ispunct(candidate_ch)) {
        snprintf(punct_replacement, sizeof(punct_replacement), "%c^n", candidate_ch);
        quote_termination = punct_replacement;
      } else {
        quote_termination = ".^n";
      }
    } else {
      replacement = replace_too_long_words(viewer, speech_buf, language_in_use);
      
      if (!strcmp(replacement, speech_buf)) {
        replacement = NULL;
      } else if (!ispunct(get_final_character_from_string(replacement))){
        quote_termination = ".^n";
      }
    }
    
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
               
      i = speech_idx + strlen(should_highlight ? GET_CHAR_COLOR_HIGHLIGHT(actor) : "") + strlen(quote_termination);
    }
    
    i++;
  }
  
  NEW_EMOTE_DEBUG_SPEECH(actor, "Finished evaluation of emote projection for %s.\r\n\r\n", GET_CHAR_NAME(viewer));
  
  // Add a newline.
  strlcat(mutable_echo_string, "\r\n", sizeof(mutable_echo_string));
  
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
  
  // Scan the emote for language values. You can only use languages you know, and only up to certain word lengths.
  if (!IS_NPC(ch)) {
    int language_in_use = GET_LANGUAGE(ch);
    for (int i = 0; i < (int) strlen(argument); i++) {
      // Skip everything that's not speech.
      if (argument[i] != '"')
        continue;
      
      // We only accept parenthetical language as the very first thing in the sentence.
      if (argument[++i] == '(') {
        i++;
        
        char language_string[100];
        int language_idx;
        
        // Copy the language into the language_string buffer, stopping at the next parenthetical.
        for (language_idx = i; 
             language_idx - i < (int) sizeof(language_string) - 1 && argument[language_idx] != ')'; 
             language_idx++) 
          language_string[language_idx - i] = argument[language_idx];
        
        // Null-terminate language buffer.
        language_string[language_idx - i] = '\0';
        
        // We expect that the M_E_S pointer is at the closing parens. If this is not true, we ran out of buf
        // space-- that means this is not a language! Just bail out.
        if (argument[language_idx] != ')') {
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
      while (i < (int) strlen(argument) && argument[i] != '"')
        *(ptr++) = argument[i++];
      *ptr = '\0';
      
      if (!has_required_language_ability_for_sentence(ch, storage_string, language_in_use))
        return;
    }
  }
  
  // All checks done, we're clear to emote.  
  
  // Iterate over the viewers in the room.
  for (struct char_data *viewer = ch->in_room ? ch->in_room->people : ch->in_veh->people; 
       viewer; 
       viewer = ch->in_room ? viewer->next_in_room : viewer->next_in_veh) {
    // If the viewer is a valid target, send it to them. Yes, ch is deliberately a possible viewer.
    if (subcmd != SCMD_AECHO || (IS_ASTRAL(viewer) || IS_DUAL(viewer)))
      send_echo_to_char(ch, viewer, argument, subcmd == SCMD_EMOTE);
  }
}

bool has_required_language_ability_for_sentence(struct char_data *ch, const char *message, int language_skill) {
  if (IS_NPC(ch))
    return TRUE;
  
  int max_allowable = max_allowable_word_length_at_language_level(GET_SKILL(ch, language_skill));
  
  char current_word[500];
  char too_long_words[MAX_STRING_LENGTH];
  memset(too_long_words, '\0', sizeof(too_long_words));
  
  char *ptr = current_word;
  int num_words = 0;
  
  // We specifically use <=, because we know this is null-terminated and we want to act on the null at the end.
  for (int i = 0; i <= (int) strlen(message); i++) {
    if (isalpha(message[i])) {
      // Skip capitalized words like Aztechnology.
      if (isupper(message[i]) && (i+1 < (int) strlen(message) && !isupper(message[i+1]))) {
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

const char *generate_random_lexicon_sentence(int language_skill, int length) {
  static char message_buf[MAX_STRING_LENGTH];
  *message_buf = '\0';
  
  bool need_caps = FALSE;
  while ((int) strlen(message_buf) < length) {
    if (!(*message_buf))
      need_caps = TRUE;
    else {
      need_caps = FALSE;
      
      if (!number(0, 10))
        strlcat(message_buf, (need_caps = number(0,2)) ? ". " : ", ", sizeof(message_buf));
      else 
        strlcat(message_buf, " ", sizeof(message_buf));
    }
    
    if (need_caps) {
      strlcat(message_buf, capitalize(get_random_word_from_lexicon(language_skill)), sizeof(message_buf));
    } else {
      strlcat(message_buf, get_random_word_from_lexicon(language_skill), sizeof(message_buf));
    }
  }
  
  // We specifically do not want to end in a period.
  // strlcat(message_buf, ".", sizeof(message_buf));
    
  return message_buf;
}

// When listening, you get too-long words replaced with random ones.
const char *replace_too_long_words(struct char_data *ch, const char *message, int language_skill) {
  static char replaced_message[MAX_STRING_LENGTH];
  static char storage_buf[MAX_STRING_LENGTH];
  int max_allowable = max_allowable_word_length_at_language_level(GET_SKILL(ch, language_skill));
  
  bool screenreader = PRF_FLAGGED(ch, PRF_SCREENREADER);
  
  if (IS_NPC(ch))
    return message;
    
  strlcpy(replaced_message, message, sizeof(replaced_message));
    
  // We specifically use <=, because we know this is null-terminated and we want to act on the null at the end.
  for (char *ptr = replaced_message; ptr && (ptr - replaced_message) <= (int) strlen(replaced_message); ptr++) {
    if (isalpha(*ptr)) {
      // Skip capitalized words. This preserves things like names.
      bool yelling = isupper(*ptr) && isupper(*(ptr + 1));
      if (isupper(*ptr) && !yelling) {
        while (isalpha(*(++ptr)));
        continue;
      }
      
      char *word_ptr = ptr;
      while (isalpha(*(++word_ptr)));
      // word_ptr now points at the character after the end of the word.
      
      int len = word_ptr - ptr;
      if (len > max_allowable) {
        // Terminate the replaced message string at the beginning of the invalid word.
        *ptr = 0;
        
        const char *random_word;
        
        if (screenreader) 
          random_word = "...";
        else
          random_word = get_random_word_from_lexicon(language_skill);
        
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
