#include <cstdlib>
#include <cstring>

#include "structs.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "dblist.hpp"
#include "screen.hpp"
#include "list.hpp"
#include "awake.hpp"
#include "constants.hpp"
#include "newdb.hpp"
#include "newmatrix.hpp"
#include "phone.hpp"
#include "ignore_system.hpp"
#include "newecho.hpp"
#include "moderation.hpp"

using namespace std;

extern const char *get_voice_perceived_by(struct char_data *speaker, struct char_data *listener, bool invis_staff_should_be_identified);


unordered_map<string, phone_data*> phone_db = {};

char* generate_phone_number() {
    int num = 1000000 + rand() % 9000000; // Ensures a 7-digit number
    char* phone = new char[8]; // Allocate space for 7 digits, 1 symbol + null terminator
    snprintf(phone, 9, "%03d-%04d", num / 10000, num % 10000); // Format as ###-####
    return phone;
}

// Check if a phone number exists in the database
bool phone_exists(const char* phone) {
    char query[128];
    MYSQL_RES *res;
    MYSQL_ROW row;

    snprintf(query, sizeof(query), "SELECT phone_number FROM phone_numbers WHERE phone_number = '%s';", phone);
    if (mysql_wrapper(mysql, query)) {
        snprintf(buf3, sizeof(buf3), "WARNING: Failed to check phone database for %s. See server log for MYSQL error.", phone);
        mudlog(buf3, NULL, LOG_SYSLOG, TRUE);
        return true; // Assume exists to avoid duplicates if an error occurs
    }

    res = mysql_store_result(mysql);
    if ((row = mysql_fetch_row(res))) {
      mysql_free_result(res);
      return TRUE;
    } else {
      // Failed to find a match-- clean up and continue.
      mysql_free_result(res);
    }

    return FALSE;
}

bool is_phone_number_valid_format(const char* input) {
    // Check for null pointer
    if (!input) return FALSE;

    // Check first three characters (digits)
    for (int i = 0; i < 3; ++i) {
        if (!isdigit(input[i])) return FALSE;
    }

    // Check for hyphen
    if (input[3] != '-') return FALSE;

    // Check last four characters (digits)
    for (int i = 4; i < 8; ++i) {
        if (!isdigit(input[i])) return FALSE;
    }

    // Ensure string ends properly
    return input[8] == '\0';
}

phone_data* create_phone_number(struct obj_data *for_obj, int load_origin) {
    char* phone;
    do {
        if (phone) delete[] phone; // Free previous allocation if it exists
        phone = generate_phone_number();
    } while (phone_exists(phone));

    snprintf(buf, sizeof(buf), 
        "INSERT INTO phone_number (phone_number, "
        " associated_obj_idnum, "
        " associated_obj_vnum, "
        " created_on, "
        " load_origin "
        " VALUES ('%s', %ld, %ld, %ld, '%s');",
        phone, // We know this is safe, not user input
        GET_OBJ_VNUM(for_obj),
        GET_OBJ_IDNUM(for_obj),
        time(0),
        load_origin
    );

    if (mysql_wrapper(mysql, buf)) {
        string phone_key(phone);
         // Create new phone_number struct
        phone_data* entry = new phone_data;
        entry->number = strdup(phone); // Duplicate the string to avoid dangling pointers
        entry->device = for_obj;

        // Store in cache
        phone_db[phone_key] = entry;
        return entry;
    } else {
        delete[] phone; // Free memory on failure
        return NULL;
    }
}

void extract_phone_number(const char* phone_number) {
    snprintf(buf, sizeof(buf), "DELETE FROM phone_number WHERE phone_number = '%s';", phone_number);
    if (!mysql_wrapper(mysql, buf)) {
        snprintf(buf3, sizeof(buf3), "WARNING: Failed to release phone number %s. See server log for MYSQL error.", phone_number);
        mudlog(buf3, NULL, LOG_SYSLOG, TRUE);
    }

    auto it = phone_db.find(phone_number);
    if (it != phone_db.end()) {
        free((void*)it->second->number); // Free strdup memory
        delete it->second; // Delete struct
        phone_db.erase(it); // Remove from cache
    }
}

bool resolve_phone_number(const char* phone_number, struct phone_data *&phone) {
    auto it = phone_db.find(phone_number);
    phone = (it != phone_db.end()) ? it->second : NULL;
    return phone != NULL;
}

void send_to_other_phone(struct char_data *speaker, struct phone_data *sender, const char* message) {
  if (!sender->connected_to) return;

  struct phone_data *receiver = sender->connected_to;
  send_to_phone(receiver, message);    
}

bool try_get_phone_character(struct phone_data *receiver, struct char_data *&to_character) {
  to_character = receiver->device->carried_by;
  if (!to_character)
    to_character = receiver->device->worn_by;
  if (!to_character && receiver->device->in_obj)
    to_character = receiver->device->in_obj->carried_by;
  if (!to_character && receiver->device->in_obj)
    to_character = receiver->device->in_obj->worn_by;
  if (!to_character)
    return FALSE;
  return TRUE;
}

void send_to_phone(struct phone_data *receiver, const char* message) {
  if (receiver->persona) {
    store_message_to_history(receiver->persona->decker->ch->desc, COMM_CHANNEL_PHONE, message);
    send_to_icon(receiver->persona, message);
    return;
  }

  if (!receiver->device) {
    return; // TODO: Maybe mudlog this?
  }
  
  struct char_data *to_character = NULL;
  if (!try_get_phone_character(receiver, to_character)) {
    return;
  }

  store_message_to_history(to_character->desc, COMM_CHANNEL_PHONE, message);
  send_to_char(message, to_character);
}

void terminate_call(struct phone_data *for_phone, bool silent = FALSE) {
  if (!for_phone || !for_phone->connected_to) return;

  if (!silent && for_phone->connected_to->persona) {
    if (for_phone->connected_to->status == PHONE_STATUS_RINGING)
      snprintf(buf1, sizeof(buf1), "The flashing phone icon fades from view, and \"Missed Call: %s\" flashes briefly.\r\n", for_phone->number);
    else
      snprintf(buf1, sizeof(buf1), "The flashing phone icon fades from view.\r\n");
    send_to_other_phone(for_phone, buf1);
  } else if (!silent) {
    if (for_phone->status == PHONE_STATUS_IN_CALL)
      snprintf(buf1, sizeof(buf1), "%s stops ringing, and \"Missed Call: %s\" flashes briefly on its display.\r\n",
        capitalize(GET_OBJ_NAME(for_phone->connected_to->device)), for_phone->number);
    else
      snprintf(buf1, sizeof(buf1), "Your phone stops ringing.\r\n");
    send_to_other_phone(for_phone, buf1);
  }

  for_phone->status = PHONE_STATUS_ONLINE;
  for_phone->connected_to->connected_to = NULL;
  for_phone->connected_to->status = PHONE_STATUS_ONLINE;
  for_phone->connected_to = NULL;
}

void extract_phone_number(struct obj_data *obj) {
  for (auto it = phone_db.begin(); it != phone_db.end(); ++it) {
    struct phone_data *k = it->second;
    if (k->device != obj) continue;

    terminate_call(k); // This is always safe to call.

    k->device = NULL;
  }
}

void extract_phone_number(struct matrix_icon *icon) {
  for (auto it = phone_db.begin(); it != phone_db.end(); ++it) {
    struct phone_data *k = it->second;
    if (k->persona != icon) continue;

    terminate_call(k); // This is always safe to call.

    k->device = NULL;
  }
}

void ring_phone(struct phone_data *receiver) {
  // Matrix call?
  if (receiver->persona) {
    send_to_icon(receiver->persona, "A small telephone symbol blinks in the top left of your view.\r\n");
    return;
  }
  // RL call.
  else {
    struct char_data * tch;
    if (!try_get_phone_character(receiver, tch)) {
      // We couldn't get a phone character, so just ring the room.
      if (receiver->silent) { 
        return; // Silence mode so just return
      }
      
      snprintf(buf, sizeof(buf), "%s^n rings.", GET_OBJ_NAME(receiver->device));
      if (receiver->device->in_veh) {
        send_to_veh(buf, receiver->device->in_veh, NULL, TRUE);
        return;
      }
      
      send_to_room(buf, receiver->device->in_room);
      return;
    }

    if (GET_POS(tch) == POS_SLEEPING) {
      if (success_test(GET_WIL(tch), 4)) {
        GET_POS(tch) = POS_RESTING;
        send_to_char("You are woken by your phone ringing.\r\n", tch);
      } else if (!GET_OBJ_VAL(receiver->device, 3)) {
        act("$n's phone rings.", FALSE, tch, 0, 0, TO_ROOM);
        return;
      } else
        return;
    }
    if (!receiver->silent) {
      send_to_char("Your phone rings.\r\n", tch);
      act("$n's phone rings.", FALSE, tch, NULL, NULL, TO_ROOM);
    } else {
      if (GET_OBJ_TYPE(receiver->device) == ITEM_CYBERWARE || success_test(GET_INT(tch), 4))
        send_to_char("You feel your phone ring.\r\n", tch);
    }
  }
}

void print_phone_info(struct char_data *ch, struct obj_data *obj, struct phone_data *phone, bool is_decker) {
  if (is_decker) 
    send_to_char(ch, "You check your commlink app:\r\n");
  else
    send_to_char(ch, "You check %s's display:\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
  send_to_char(ch, "  Phone Number:  ^c%s^n", phone->number);
  
  if (phone->connected_to) {
    if (phone->status == PHONE_STATUS_IN_CALL)
      send_to_char(ch, "  In Call With:  ^c%s^n", phone->connected_to->number);
    else if (phone->status == PHONE_STATUS_BUSY)
      send_to_char(ch, "        Caling:  ^c%s^n", phone->connected_to->number);
    else if (phone->status == PHONE_STATUS_RINGING)
      send_to_char(ch, " Incoming Call:  ^c^s^n", phone->connected_to->number);
  }
}

void do_phone_talk(struct char_data *ch, struct phone_data *phone, char* message)
{
  if (check_for_banned_content(message, ch))
    return;

  bool internal_only = FALSE;
  char phone_obj_name[500]; 
  if (phone->persona && phone->persona->decker) {
    snprintf(phone_obj_name, sizeof(phone_obj_name), "a commlink program");
    internal_only = TRUE;
  } else {
    if (!phone->device) {
      snprintf(buf, sizeof(buf), "Tried to talk on phone number %s, but there are no devices associated with it? WTF", 
        phone->number);
      mudlog(buf, ch, LOG_WIZLOG, TRUE);
      return;
    }
    if (GET_OBJ_TYPE(phone->device) == ITEM_CYBERWARE && GET_CYBERWARE_TYPE(phone->device) == CYB_PHONE)
      internal_only = TRUE;
    snprintf(phone_obj_name, sizeof(phone_obj_name), "%s", decapitalize_a_an(GET_OBJ_NAME(phone->device)));
  }
  FAILURE_CASE_PRINTF(phone->status == PHONE_STATUS_OFFLINE, "Try switching %s on first.\r\n", phone_obj_name);
  FAILURE_CASE(!phone->connected_to, "But you don't currently have a call.\r\n");
  FAILURE_CASE_PRINTF(phone->status != PHONE_STATUS_IN_CALL, "%s is still busy ringing.\r\n", CAP(phone_obj_name));

  if (!char_can_make_noise(ch, "You can't seem to make any noise.\r\n"))
    return;

  skip_spaces(&message);

  FAILURE_CASE(!*message, "Yes, but what do you want to say?\r\n");

  int language = !SKILL_IS_LANGUAGE(GET_LANGUAGE(ch)) ? SKILL_ENGLISH : GET_LANGUAGE(ch);
  if (!has_required_language_ability_for_sentence(ch, message, language))
    return;

  #define VOICE_BUF_SIZE 20
  char voice[VOICE_BUF_SIZE] = {"$v"};

  for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content)
    if (GET_CYBERWARE_TYPE(obj) == CYB_VOICEMOD && GET_CYBERWARE_FLAGS(obj))
      strlcpy(voice, "A masked voice", VOICE_BUF_SIZE);

  if (AFF_FLAGGED(ch, AFF_VOICE_MODULATOR))
    strlcpy(voice, "A masked voice", VOICE_BUF_SIZE);

  if (ch->persona && ch->persona->decker) {
    snprintf(buf3, sizeof(buf3), "^YYou transmit to your comm program in ");
  } else if (internal_only) {
    snprintf(buf3, sizeof(buf3), "^YYou silently vocalize to your cyberphone in ");
  } else {
    snprintf(buf3, sizeof(buf3), "^YYou say into your phone in ");
  }
  snprintf(ENDOF(buf3), sizeof(buf3) - strlen(buf3), "%s, \"%s%s^Y\"\r\n",
            skills[language].name,
            capitalize(message),
            ispunct(get_final_character_from_string(message)) ? "" : ".");
  send_to_phone(phone, buf3);


  bool tch_is_matrix = phone->connected_to->persona != NULL;
  struct char_data *tch = tch_is_matrix 
    ? phone->connected_to->persona->decker->ch 
    : get_obj_possessor(phone->connected_to->device);
  snprintf(buf, sizeof(buf), "^Y%s^Y on the other end of the line says in %s, \"%s%s^Y\"",
            tch_is_matrix ? get_voice_perceived_by(ch, tch, FALSE) : voice,
            (IS_NPC(tch) || GET_SKILL(tch, language) > 0) ? skills[language].name : "an unknown language",
            capitalize(replace_too_long_words(tch, ch, message, language, "^Y")),
            ispunct(get_final_character_from_string(message)) ? "" : ".");
  send_to_other_phone(phone, buf);

  
  if (internal_only)
    return;

  // Send to nearby people.
  for (tch = ch->in_veh ? ch->in_veh->people : ch->in_room->people; tch; tch = ch->in_veh ? tch->next_in_veh : tch->next_in_room) {
    if (tch != ch) {
      snprintf(buf2, MAX_STRING_LENGTH, "$z^n says into $s phone in %s, \"%s%s%s^n\"",
              (IS_NPC(tch) || GET_SKILL(tch, language) > 0) ? skills[language].name : "an unknown language",
              (PRF_FLAGGED(tch, PRF_NOHIGHLIGHT) || PRF_FLAGGED(tch, PRF_NOCOLOR)) ? "" : GET_CHAR_COLOR_HIGHLIGHT(ch),
              capitalize(replace_too_long_words(tch, ch, message, language, GET_CHAR_COLOR_HIGHLIGHT(ch), FALSE)),
              ispunct(get_final_character_from_string(message)) ? "" : ".");
      store_message_to_history(tch->desc, COMM_CHANNEL_SAYS, act(buf2, FALSE, ch, 0, tch, TO_VICT));
    }
  }
}

void handle_phone_command(struct char_data *ch, int subcmd, char *argument)
{
  struct phone_data *phone = NULL, *targ_phone = NULL;
  struct obj_data *phone_device = NULL;
  char phone_obj_name[500]; 
  bool is_decker = FALSE;

  if (ch->persona && ch->persona->decker) {
    phone = ch->persona->decker->phone;
    phone_device = ch->persona->decker->deck;
    is_decker = TRUE;
    snprintf(phone_obj_name, sizeof(phone_obj_name), "a commlink program");
  } else {
    for (struct obj_data *obj = ch->carrying; obj; obj = obj->next_content) {
      if (GET_OBJ_TYPE(obj) == ITEM_PHONE) {
        phone_device = obj;
        break;
      }
    }

    for (int x = 0; !phone_device && x < NUM_WEARS; x++) {
      if (GET_EQ(ch, x) && GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_PHONE) {
        phone_device = GET_EQ(ch, x);
        break;
      }
    }

    if (!phone_device) {
      for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content)
        if (GET_CYBERWARE_TYPE(obj) == CYB_PHONE) {
          phone_device = obj;
          break;
        }
    }

    if (phone_device)
      snprintf(phone_obj_name, sizeof(phone_obj_name), "%s", decapitalize_a_an(GET_OBJ_NAME(phone_device)));
  }

  FAILURE_CASE(!phone_device, "Unable to find any phone on your person; are you sure you have one?\r\n");

  if (!phone && !try_resolve_obj_phone(phone_device, phone)) {
    snprintf(buf, sizeof(buf), "Tried to load %s's phone %s (VNUM#%ld) (IDNUM#%ld), but couldn't find their number? WTF?", 
      GET_CHAR_NAME(ch), GET_OBJ_NAME(phone_device), GET_OBJ_VNUM(phone_device), GET_OBJ_IDNUM(phone_device));
    mudlog(buf, ch, LOG_WIZLOG, TRUE);
    return;
  }

  if (!subcmd) {
    any_one_arg(argument, arg);
    if (is_abbrev(arg, "off")) {
      FAILURE_CASE_PRINTF(phone->status == PHONE_STATUS_OFFLINE, "%s is already turned off.\r\n", CAP(phone_obj_name));
      if (phone->status != PHONE_STATUS_ONLINE) {
        terminate_call(phone);
      }
      phone->status = PHONE_STATUS_OFFLINE;
      send_to_char(ch, "You turn %s off.", phone_obj_name);
      return;
    } else if (is_abbrev(arg, "on")) {
      FAILURE_CASE_PRINTF(phone->status != PHONE_STATUS_OFFLINE, "%s is already turned on.\r\n", CAP(phone_obj_name));
      phone->status = PHONE_STATUS_ONLINE;
      send_to_char(ch, "You turn %s on.", phone_obj_name);
      return;
    } else if (is_abbrev(arg, "ring")) {
      FAILURE_CASE_PRINTF(!phone->silent, "%s is already set to ring.\r\n", CAP(phone_obj_name));
      send_to_char(ch, "You set %s to ring on calls.\r\n", phone_obj_name);
      phone->silent = FALSE;
      return;
    } else if (is_abbrev(arg, "silent")) {
      FAILURE_CASE_PRINTF(phone->silent, "%s is already set to silent mode.\r\n", CAP(phone_obj_name));
      send_to_char(ch, "You set %s to silent mode.\r\n", phone_obj_name);
      phone->silent = TRUE;
      return;
    }

    // Fall thru:
    if (is_decker) 
      return print_phone_info(ch, phone_device, phone, is_decker);
    
    struct phone_data *temp_phone = NULL;
    for (struct obj_data *obj = ch->carrying; obj; obj = obj->next_content) {
      if (GET_OBJ_TYPE(obj) == ITEM_PHONE) {
        if (!try_resolve_obj_phone(obj, temp_phone)) {
          snprintf(buf, sizeof(buf), "Tried to load %s's phone %s (VNUM#%ld) (IDNUM#%ld), but couldn't find their number? WTF?", 
            GET_CHAR_NAME(ch), GET_OBJ_NAME(phone_device), GET_OBJ_VNUM(phone_device), GET_OBJ_IDNUM(phone_device));
          mudlog(buf, ch, LOG_WIZLOG, TRUE);
          continue;
        }
        print_phone_info(ch, obj, temp_phone, is_decker);
        send_to_char(ch, "\r\n");
      }
    }

    for (int x = 0; !phone_device && x < NUM_WEARS; x++) {
      if (GET_EQ(ch, x) && GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_PHONE) {
        if (!try_resolve_obj_phone(GET_EQ(ch, x), temp_phone)) {
          snprintf(buf, sizeof(buf), "Tried to load %s's phone %s (VNUM#%ld) (IDNUM#%ld), but couldn't find their number? WTF?", 
            GET_CHAR_NAME(ch), GET_OBJ_NAME(phone_device), GET_OBJ_VNUM(phone_device), GET_OBJ_IDNUM(phone_device));
          mudlog(buf, ch, LOG_WIZLOG, TRUE);
          continue;
        }
        print_phone_info(ch, GET_EQ(ch, x), temp_phone, is_decker);
        send_to_char(ch, "\r\n");
      }
    }

    for (struct obj_data *obj = ch->cyberware; obj; obj = obj->next_content)
      if (GET_CYBERWARE_TYPE(obj) == CYB_PHONE) {
        if (!try_resolve_obj_phone(obj, temp_phone)) {
          snprintf(buf, sizeof(buf), "Tried to load %s's phone %s (VNUM#%ld) (IDNUM#%ld), but couldn't find their number? WTF?", 
            GET_CHAR_NAME(ch), GET_OBJ_NAME(phone_device), GET_OBJ_VNUM(phone_device), GET_OBJ_IDNUM(phone_device));
          mudlog(buf, ch, LOG_WIZLOG, TRUE);
          continue;
        }
        print_phone_info(ch, obj, temp_phone, is_decker);
        send_to_char(ch, "\r\n");
      }
    return;
  }

  if (subcmd == SCMD_ANSWER) {
    FAILURE_CASE_PRINTF(phone->status == PHONE_STATUS_OFFLINE, "Try switching %s on first.\r\n", phone_obj_name);
    FAILURE_CASE(!phone->connected_to, "There are no incoming connections currently.\r\n");
    FAILURE_CASE(phone->connected_to && phone->status == PHONE_STATUS_IN_CALL, "You're already connected to a call.\r\n");
    FAILURE_CASE(phone->connected_to && phone->status == PHONE_STATUS_BUSY, "How are you going to answer when you're calling someone?\r\n");
    
    send_to_other_phone(phone, "The call is answered.\r\n");
    send_to_phone(phone, "You answer your incoming call.\r\n");

    phone->status = PHONE_STATUS_IN_CALL;
    phone->connected_to->status = PHONE_STATUS_IN_CALL;
  } else if (subcmd == SCMD_RING) {
    FAILURE_CASE_PRINTF(phone->status == PHONE_STATUS_OFFLINE, "Try switching %s on first.\r\n", phone_obj_name);
    FAILURE_CASE(phone->connected_to, "Your phone is currently busy and cannot call out.\r\n");
    
    any_one_arg(argument, arg);
    FAILURE_CASE(!*arg, "Ring what number?\r\n");
    FAILURE_CASE(!is_phone_number_valid_format(arg), "That is not a valid number. The format should be ####-####.\r\n");
    FAILURE_CASE(!try_resolve_phone_number(arg, targ_phone), "The phone is picked up and a polite female voice says, \"The number you have dialed is not in"
                   " service. Please check your number and try again.\"\r\n");
    FAILURE_CASE(phone == targ_phone, "Why would you want to call yourself?\r\n");

    FAILURE_CASE(targ_phone->status != PHONE_STATUS_ONLINE, "You hear the busy signal.\r\n");
    FAILURE_CASE(targ_phone->persona && IS_IGNORING(targ_phone->persona->decker->ch, is_blocking_calls_from, ch), "You hear the busy signal.\r\n");
    
    phone->connected_to = targ_phone;
    phone->status = PHONE_STATUS_BUSY;
    targ_phone->connected_to = phone;
    targ_phone->status = PHONE_STATUS_RINGING;

    ring_phone(targ_phone);
    send_to_char("It begins to ring.\r\n", ch);
  } else if (subcmd == SCMD_HANGUP) {
    FAILURE_CASE_PRINTF(phone->status == PHONE_STATUS_OFFLINE, "Try switching %s on first.\r\n", phone_obj_name);
    FAILURE_CASE(!phone->connected_to, "There's no point hanging up when you're not in a call.\r\n");
    terminate_call(phone, FALSE);
    send_to_char("You end the call.\r\n", ch);
  } else if (subcmd == SCMD_TALK) {
    do_phone_talk(ch, phone, argument);
  }
}

void phones_update() 
{
  struct phone_data *phone = NULL;
  for (auto it = phone_db.begin(); it != phone_db.end(); ++it) {
    phone = it->second;
    if (!phone->connected_to) continue;

    if (phone->status == PHONE_STATUS_RINGING) {
      ring_phone(phone);
    }
  }
}