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

using namespace std;

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
        phone_number* entry = new phone_number;
        entry->number = strdup(phone); // Duplicate the string to avoid dangling pointers
        entry->to_obj = for_obj;

        // Store in cache
        phone_db[phone_key] = entry;
        return phone; // Return dynamically allocated unique number
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
    return (it != phone_db.end()) ? it->second : NULL;
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

void extract_phone_number(struct obj_data *obj) {
  if (GET_OBJ_TYPE(obj) == ITEM_PHONE ||
      (GET_OBJ_TYPE(obj) == ITEM_CYBERWARE && GET_OBJ_VAL(obj, 0) == CYB_PHONE))
  {
    for (phone = phone_list; phone; phone = phone->next) {
      if (phone->phone == obj) {
        if (phone->dest) {
          phone->dest->dest = NULL;
          phone->dest->connected = FALSE;
          if (phone->dest->persona)
            send_to_icon(phone->dest->persona, "The connection is closed from the other side.\r\n");
          else {
            if (phone->dest->phone->carried_by) {
              if (phone->dest->connected) {
                if (phone->dest->phone->carried_by)
                  send_to_char("The phone is hung up from the other side.\r\n", phone->dest->phone->carried_by);
              } else {
                if (phone->dest->phone->carried_by)
                  send_to_char("Your phone stops ringing.\r\n", phone->dest->phone->carried_by);
                else if (phone->dest->phone->in_obj && phone->dest->phone->in_obj->carried_by)
                  send_to_char("Your phone stops ringing.\r\n", phone->dest->phone->in_obj->carried_by);
                else {
                  snprintf(buf, sizeof(buf), "%s stops ringing.\r\n", phone->dest->phone->text.name);
                  act(buf, FALSE, 0, phone->dest->phone, 0, TO_ROOM);
                }
              }
            }

          }
        }
        REMOVE_FROM_LIST(phone, phone_list, next);
        delete phone;
        break;
      }
    }
  }
}

void extract_phone_number(struct matrix_icon *icon) {
  
  for (struct phone_data *k = phone_list; k; k = k->next) {
    if (k->persona == icon)
    {
      struct phone_data *temp;
      if (k->dest) {
        if (k->dest->persona)
          send_to_icon(k->dest->persona, "The call is terminated from the other end.\r\n");
        else if (k->dest->phone) {
          if (k->dest->connected) {
            if (k->dest->phone->carried_by)
              send_to_char("The phone is hung up from the other side.\r\n", k->dest->phone->carried_by);
          } else {
            if (k->dest->phone->carried_by)
              send_to_char("Your phone stops ringing.\r\n", k->dest->phone->carried_by);
            else if (k->dest->phone->in_obj && k->dest->phone->in_obj->carried_by)
              send_to_char("Your phone stops ringing.\r\n", k->dest->phone->in_obj->carried_by);
            else {
              snprintf(buf, sizeof(buf), "%s stops ringing.\r\n", k->dest->phone->text.name);
              act(buf, FALSE, 0, k->dest->phone, 0, TO_ROOM);
            }
          }
        }
        k->dest->connected = FALSE;
        k->dest->dest = NULL;
      }
      REMOVE_FROM_LIST(k, phone_list, next);
      delete k;
      break;
    }
  }
}